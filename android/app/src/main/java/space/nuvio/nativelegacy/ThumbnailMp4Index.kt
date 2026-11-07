package space.nuvio.nativelegacy

/** Bounded index for unencrypted, non-fragmented AVC/HEVC MP4 video.
 * Unsupported layouts fail closed instead of inventing presentation timestamps. */
internal class ThumbnailMp4Index private constructor(
    val cues: LongArray, val sizes: IntArray, val nalLength: Int
) {
    companion object {
        private const val MAX_SAMPLES = 2_000_000
        private const val MAX_SYNC = 20_000
        fun read(size: Long, read: (Long, Int) -> ByteArray?): ThumbnailMp4Index? = try {
            Parser(size, read).parse()
        } catch (_: IllegalArgumentException) { null } catch (_: ArithmeticException) { null } catch (_: NoSuchElementException) { null }
    }
    private class Parser(val size: Long, val reader: (Long, Int) -> ByteArray?) {
        data class Box(val type: String, val data: Long, val end: Long)
        var budget = 8 * 1024 * 1024
        var count = 0
        fun bytes(pos: Long, n: Int): ByteArray {
            require(n >= 0 && n <= budget && pos >= 0 && pos <= size - n)
            budget -= n
            return reader(pos, n)?.also { require(it.size == n) } ?: throw IllegalArgumentException("short MP4 read")
        }
        fun u(b: ByteArray, p: Int, n: Int = 4): Long {
            require(p >= 0 && p <= b.size - n)
            if (n == 8) require(b[p] >= 0)
            var r = 0L
            for (i in p until p + n) r = (r shl 8) or (b[i].toLong() and 255)
            return r
        }
        fun boxes(start: Long, end: Long): List<Box> {
            val result = ArrayList<Box>()
            var p = start
            while (p < end) {
                require(end - p >= 8 && ++count <= 512)
                val h = bytes(p, 8)
                val type = String(h, 4, 4, Charsets.ISO_8859_1)
                var len = u(h, 0); var head = 8L
                if (len == 1L) { require(end - p >= 16); len = u(bytes(p + 8, 8), 0, 8); head = 16 }
                else if (len == 0L) len = end - p
                require(len >= head && len <= end - p)
                result += Box(type, p + head, p + len)
                p += len
            }
            return result
        }
        fun children(box: Box) = boxes(box.data, box.end)
        fun one(boxes: List<Box>, name: String): Box = boxes.single { it.type == name }
        fun body(box: Box): ByteArray {
            require(box.end - box.data <= budget)
            return bytes(box.data, (box.end - box.data).toInt())
        }
        fun table(boxes: List<Box>, type: String, stride: Int, max: Int = MAX_SAMPLES): ByteArray {
            val b = body(one(boxes, type))
            require(b.size >= 8 && b[0].toInt() in 0..1 && u(b, 1, 3) == 0L)
            val n = u(b, 4)
            require(n in 1..max.toLong() && 8L + n * stride == b.size.toLong())
            return b
        }
        fun scale(ticks: Long, timescale: Long): Long {
            require(timescale > 0)
            return Math.addExact(Math.multiplyExact(ticks / timescale, 1_000_000L),
                Math.multiplyExact(ticks % timescale, 1_000_000L) / timescale)
        }
        fun headerTimescale(box: Box): Long {
            val b = bytes(box.data, minOf(32L, box.end - box.data).toInt())
            require(b.isNotEmpty() && b[0].toInt() in 0..1)
            return u(b, if (b[0].toInt() == 1) 20 else 12).also { require(it > 0) }
        }
        fun parse(): ThumbnailMp4Index? {
            val root = boxes(0, size)
            require(root.any { it.type == "ftyp" } && root.none { it.type == "moof" })
            val media = root.filter { it.type == "mdat" }
            require(media.isNotEmpty())
            val moov = children(one(root, "moov"))
            require(moov.none { it.type == "mvex" })
            val movieScale = headerTimescale(one(moov, "mvhd"))
            for (track in moov.filter { it.type == "trak" }) {
                val tr = children(track)
                val mdia = children(one(tr, "mdia"))
                val handler = body(one(mdia, "hdlr"))
                require(handler.size >= 12)
                if (String(handler, 8, 4, Charsets.ISO_8859_1) != "vide") continue
                val timescale = headerTimescale(one(mdia, "mdhd"))
                val stbl = children(one(children(one(mdia, "minf")), "stbl"))
                val sd = one(stbl, "stsd")
                val full = bytes(sd.data, 8)
                require(u(full, 0) == 0L && u(full, 4) == 1L)
                val entry = boxes(sd.data + 8, sd.end).single()
                require(entry.type in setOf("avc1", "avc3", "hvc1", "hev1", "dvhe", "dvh1"))
                require(entry.end - entry.data >= 78)
                val configs = boxes(entry.data + 78, entry.end)
                require(configs.none { it.type == "sinf" })
                val avc = entry.type.startsWith("avc")
                val cfg = body(one(configs, if (avc) "avcC" else "hvcC"))
                require(cfg.isNotEmpty() && cfg[0].toInt() == 1)
                val nalPos = if (avc) 4 else 21
                require(cfg.size > nalPos)
                val nal = (cfg[nalPos].toInt() and 3) + 1
                require(nal != 3)

                val sizesBody = body(one(stbl, "stsz"))
                require(sizesBody.size >= 12 && u(sizesBody, 0) == 0L)
                val commonSize = u(sizesBody, 4)
                val n = u(sizesBody, 8).also { require(it in 1..MAX_SAMPLES.toLong()) }.toInt()
                require(sizesBody.size.toLong() == 12L + if (commonSize == 0L) n * 4L else 0L)
                val sampleSizes = IntArray(n) { i ->
                    (if (commonSize > 0) commonSize else u(sizesBody, 12 + i * 4))
                        .also { require(it in 1..(8L shl 20)) }.toInt()
                }
                val chunks64 = stbl.any { it.type == "co64" }
                val chunks = table(stbl, if (chunks64) "co64" else "stco", if (chunks64) 8 else 4)
                require(chunks[0].toInt() == 0)
                val chunkCount = u(chunks, 4).toInt()
                val sc = table(stbl, "stsc", 12)
                require(sc[0].toInt() == 0)
                val scCount = u(sc, 4).toInt()
                for (i in 0 until scCount) {
                    val first = u(sc, 8 + i * 12)
                    require(first in 1..chunkCount.toLong() && (i != 0 || first == 1L))
                    if (i > 0) require(first > u(sc, 8 + (i - 1) * 12))
                    require(u(sc, 12 + i * 12) in 1..n.toLong() && u(sc, 16 + i * 12) == 1L)
                }
                val syncBox = stbl.singleOrNull { it.type == "stss" }
                val sync = if (syncBox == null) {
                    require(n <= MAX_SYNC); IntArray(n) { it }
                } else {
                    val ss = table(stbl, "stss", 4, MAX_SYNC)
                    require(ss[0].toInt() == 0)
                    IntArray(u(ss, 4).toInt()) { i ->
                        (u(ss, 8 + i * 4) - 1).also {
                            require(it in 0 until n.toLong() && (i == 0 || it >= u(ss, 4 + i * 4)))
                        }.toInt()
                    }
                }
                val offsets = LongArray(sync.size)
                var sample = 0; var scIndex = 0; var syncIndex = 0
                for (chunk in 0 until chunkCount) {
                    if (scIndex + 1 < scCount && chunk + 1L == u(sc, 8 + (scIndex + 1) * 12)) scIndex++
                    var offset = u(chunks, 8 + chunk * (if (chunks64) 8 else 4), if (chunks64) 8 else 4)
                    val perChunk = u(sc, 12 + scIndex * 12).toInt()
                    require(perChunk <= n - sample)
                    val region = media.firstOrNull { offset >= it.data && offset < it.end }
                        ?: throw IllegalArgumentException("sample chunk is outside mdat")
                    repeat(perChunk) {
                        require(offset >= region.data && offset <= region.end - sampleSizes[sample])
                        if (syncIndex < sync.size && sample == sync[syncIndex]) offsets[syncIndex++] = offset
                        offset += sampleSizes[sample++]
                    }
                }
                require(sample == n && syncIndex == sync.size)
                val ts = table(stbl, "stts", 8)
                require(ts[0].toInt() == 0)
                val ct = if (stbl.any { it.type == "ctts" }) table(stbl, "ctts", 8) else null
                fun validateRuns(b: ByteArray, positiveDelta: Boolean) {
                    var total = 0L
                    for (i in 0 until u(b, 4).toInt()) {
                        val run = u(b, 8 + i * 8)
                        require(run > 0)
                        total = Math.addExact(total, run)
                        if (positiveDelta) require(u(b, 12 + i * 8) > 0)
                    }
                    require(total == n.toLong())
                }
                validateRuns(ts, true); ct?.let { validateRuns(it, false) }

                // Support ordinary rate-1 edits: optional leading empty edit plus one media edit.
                var emptyUs = 0L; var mediaStart = 0L; var editDurationUs = Long.MAX_VALUE
                tr.singleOrNull { it.type == "edts" }?.let { edts ->
                    val b = body(one(children(edts), "elst"))
                    require(b.size >= 8 && b[0].toInt() in 0..1 && u(b, 1, 3) == 0L)
                    val width = if (b[0].toInt() == 1) 8 else 4
                    val stride = width * 2 + 4
                    val entries = u(b, 4).toInt()
                    require(entries in 1..2 && b.size == 8 + entries * stride)
                    for (i in 0 until entries) {
                        val p = 8 + i * stride
                        val duration = u(b, p, width)
                        val raw = b.copyOfRange(p + width, p + width * 2)
                        val empty = raw.all { it.toInt() == -1 }
                        require(u(b, p + width * 2) == 0x10000L)
                        if (empty) {
                            require(i == 0 && entries == 2)
                            emptyUs = scale(duration, movieScale)
                        } else {
                            require(i == entries - 1)
                            mediaStart = u(b, p + width, width)
                            editDurationUs = if (duration == 0L) Long.MAX_VALUE else scale(duration, movieScale)
                        }
                    }
                }
                val times = LongArray(sync.size)
                var tsIndex = 0; var tsLeft = u(ts, 8)
                var ctIndex = 0; var ctLeft = ct?.let { u(it, 8) } ?: n.toLong()
                var dts = 0L; syncIndex = 0
                for (i in 0 until n) {
                    val delta = u(ts, 12 + tsIndex * 8)
                    val composition = ct?.let {
                        val raw = u(it, 12 + ctIndex * 8)
                        if (it[0].toInt() == 1) raw.toInt().toLong() else raw
                    } ?: 0L
                    if (syncIndex < sync.size && i == sync[syncIndex]) {
                        val mediaPts = Math.addExact(dts, composition)
                        val relative = Math.subtractExact(mediaPts, mediaStart)
                        val relativeUs = scale(relative, timescale)
                        times[syncIndex++] = if (relative < 0 || relativeUs >= editDurationUs) -1
                            else Math.addExact(emptyUs, relativeUs)
                    }
                    dts = Math.addExact(dts, delta)
                    if (--tsLeft == 0L && i + 1 < n) { tsIndex++; tsLeft = u(ts, 8 + tsIndex * 8) }
                    if (--ctLeft == 0L && i + 1 < n) { ctIndex++; ctLeft = ct?.let { u(it, 8 + ctIndex * 8) } ?: 0 }
                }
                val valid = times.indices.filter { times[it] >= 0 }
                require(valid.isNotEmpty())
                for (i in 1 until valid.size) require(times[valid[i]] > times[valid[i - 1]])
                val cues = LongArray(valid.size * 3)
                val sizes = IntArray(valid.size)
                for ((i, k) in valid.withIndex()) {
                    cues[i * 3] = Math.multiplyExact(times[k], 1000L) // existing cue convention: nanoseconds
                    cues[i * 3 + 1] = offsets[k]
                    sizes[i] = sampleSizes[sync[k]]
                }
                return ThumbnailMp4Index(cues, sizes, nal)
            }
            return null
        }
    }
}
