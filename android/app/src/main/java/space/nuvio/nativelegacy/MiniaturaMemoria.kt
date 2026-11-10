package space.nuvio.nativelegacy

// Admission estimate, not a measurement: eight reference/output surfaces plus
// 32 MiB codec/GPU overhead. Main10 surfaces may use 16-bit components (3 B/pixel).
internal fun miniaturaReservaBytes(width: Int, height: Int, tenBit: Boolean): Long {
    val pixels = width.coerceAtLeast(1).toLong() * height.coerceAtLeast(1)
    return pixels * 8 * (if (tenBit) 3 else 2) + (32L shl 20)
}

internal fun miniaturaMemoriaAdmite(available: Long, reserve: Long): Boolean =
    available > 0 && available - reserve >= (364L shl 20) // 300 MiB floor + 64 MiB recovery margin
