package space.nuvio.nativelegacy

/** Reads only MP4 box headers and Dolby configuration, never the movie's mdat payload. */
internal object ThumbnailDolbyVision {
    fun requiresDolbyColour(config: Int): Boolean = config == 5 * 16

    fun supported(config: Int): Boolean = config >= 0 &&
        config / 16 in intArrayOf(7, 8) && config % 16 in intArrayOf(1, 2, 4, 6)

    fun mp4(size: Long, read: (Long, Int) -> ByteArray?): Int {
        if (size < 8) return -1
        var boxes = 0
        var budget = 64 * 1024
        fun bytes(pos: Long, count: Int): ByteArray? {
            if (pos < 0 || count > size || pos > size - count || count > budget) return null
            budget -= count
            return read(pos, count)?.takeIf { it.size == count }
        }
        fun uint(data: ByteArray, offset: Int, count: Int): Long {
            var value = 0L
            for (i in offset until offset + count) value = (value shl 8) or (data[i].toLong() and 255)
            return value
        }
        fun walk(start: Long, end: Long, depth: Int, parent: String): Int {
            if (depth > 10) return -1
            var pos = start
            while (end - pos >= 8 && ++boxes <= 512) {
                val header = bytes(pos, 8) ?: return -1
                val type = String(header, 4, 4, Charsets.ISO_8859_1)
                var length = uint(header, 0, 4)
                var headerSize = 8L
                if (length == 1L) {
                    if (end - pos < 16) return -1
                    val extended = bytes(pos + 8, 8) ?: return -1
                    if (extended[0] < 0) return -1 // unsigned size cannot fit a file offset
                    length = uint(extended, 0, 8)
                    headerSize = 16
                } else if (length == 0L) length = end - pos
                if (length < headerSize || length > end - pos) return -1
                val payload = pos + headerSize
                val boxEnd = pos + length
                if (parent in setOf("hvc1", "hev1", "dvhe", "dvh1") && type in setOf("dvcC", "dvvC")) {
                    if (boxEnd - payload < 5) return -1
                    val config = bytes(payload, 5) ?: return -1
                    if (config[0].toInt() != 1 || config[1].toInt() != 0) return -1
                    // Require an actual base layer, including for otherwise compatible profiles.
                    if (config[3].toInt() and 1 == 0) return -1
                    return ((config[2].toInt() and 255) shr 1) * 16 +
                        ((config[4].toInt() and 255) shr 4)
                }
                val skip = when {
                    type == "moov" && parent == "" -> 0L
                    type == "trak" && parent == "moov" -> 0L
                    type == "mdia" && parent == "trak" -> 0L
                    type == "minf" && parent == "mdia" -> 0L
                    type == "stbl" && parent == "minf" -> 0L
                    type == "stsd" && parent == "stbl" -> 8L // full-box flags and entry count
                    type in setOf("hvc1", "hev1", "dvhe", "dvh1") && parent == "stsd" -> 78L
                    else -> -1L
                }
                if (skip >= 0 && boxEnd - payload >= skip) {
                    val config = walk(payload + skip, boxEnd, depth + 1, type)
                    if (config >= 0) return config
                }
                pos = boxEnd
            }
            return -1
        }
        return walk(0, size, 0, "")
    }
}
