package space.nuvio.nativelegacy

// Output pixels only: the source decoder still processes the original picture.
internal fun miniaturaLargura(baixoRam: Boolean, memoriaTotal: Long, bits: Int, escolhida: Int = 0): Int = when {
    // High = 576x324 (owner 27/11/2026): the largest seek preview the app draws (384x216 at the 150 % interface size,
    // on the 1080p interface a 4K TV upscales). 640 was only ever scaled down on screen.
    escolhida in listOf(320, 480, 576) -> escolhida
    bits and 128 != 0 -> 480  // diagnostic override for A/B comparisons
    bits and 64 != 0 -> 320
    baixoRam || memoriaTotal in 1..(2L shl 30) -> 320
    memoriaTotal > (5L shl 29) -> 576   // Automatic: High above 2.5 GB (owner 27/11/2026; the GPU route is fast anyway)
    else -> 480
}
