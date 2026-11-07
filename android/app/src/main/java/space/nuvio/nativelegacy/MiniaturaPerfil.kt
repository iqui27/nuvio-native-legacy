package space.nuvio.nativelegacy

// Output pixels only: the source decoder still processes the original picture.
internal fun miniaturaLargura(baixoRam: Boolean, memoriaTotal: Long, bits: Int, escolhida: Int = 0): Int = when {
    escolhida in listOf(320, 480, 640) -> escolhida
    bits and 128 != 0 -> 480  // diagnostic override for A/B comparisons
    bits and 64 != 0 -> 320
    baixoRam || memoriaTotal in 1..(2L shl 30) -> 320
    else -> 480
}
