package space.nuvio.nativelegacy

/** Header-only inspection. Unsupported/laced/non-keyframe blocks stay unknown. */
internal object ThumbnailBlockProbe {
    data class Vint(val value: Long, val bytes: Int)
    fun vint(data: ByteArray, offset: Int, id: Boolean): Vint? {
        if (offset !in data.indices) return null
        val first = data[offset].toInt() and 255
        var length = 1; var mask = 128
        while (length <= 8 && first and mask == 0) { length++; mask = mask shr 1 }
        if (length > 8 || offset > data.size - length) return null
        var value = (if (id) first else first and (mask - 1)).toLong()
        for (i in 1 until length) value = value shl 8 or (data[offset+i].toLong() and 255)
        if (!id && value == (1L shl (7*length)) - 1) return null
        return Vint(value, length)
    }
    fun clusterHeader(data: ByteArray): Int? {
        val id = vint(data, 0, true) ?: return null
        if (id.value != 0x1f43b675L) return null
        // Cluster sizes may be unknown; their encoded width still locates data.
        val at = id.bytes
        if (at !in data.indices) return null
        val first = data[at].toInt() and 255
        val width = (1..8).firstOrNull { first and (128 shr (it-1)) != 0 } ?: return null
        return (at + width).takeIf { it <= data.size }
    }
    sealed class Step {
        data class Found(val start: Long, val end: Long): Step()
        data class Skip(val next: Long): Step()
        object Unknown: Step()
    }
    fun block(data: ByteArray, start: Long, track: Long, sourceBytes: Long): Step {
        if (track <= 0 || start < 0) return Step.Unknown
        val id = vint(data, 0, true) ?: return Step.Unknown
        val size = vint(data, id.bytes, false) ?: return Step.Unknown
        val header = id.bytes + size.bytes
        if (size.value <= 0 || size.value > 64L.shl(20) || start > sourceBytes - header - size.value) return Step.Unknown
        val end = start + header + size.value
        if (id.value == 0x1f43b675L || id.value == 0xa0L) return Step.Unknown // BlockGroup needs reference validation.
        if (id.value != 0xa3L) return Step.Skip(end)
        val number = vint(data, header, false) ?: return Step.Unknown
        if (size.value < number.bytes + 3) return Step.Unknown
        val flagsAt = header + number.bytes + 2
        if (flagsAt !in data.indices) return Step.Unknown
        if (number.value != track) return Step.Skip(end)
        val flags = data[flagsAt].toInt() and 255
        if (flags and 128 == 0 || flags and 6 != 0) return Step.Unknown
        return Step.Found(start, end)
    }
    fun targets(durationMs: Long, count: Int = 50): List<Long> {
        require(count in 50..100 && durationMs in 60000..86400000)
        val slots = (durationMs / 10000).toInt()
        val n = minOf(count, slots)
        return (0 until n).map { (1L + it.toLong() * (slots-1) / (n-1)) * 10000 }.distinct()
    }
}
