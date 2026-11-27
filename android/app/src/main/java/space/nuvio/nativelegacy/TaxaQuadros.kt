package space.nuvio.nativelegacy

import kotlin.math.abs
import kotlin.math.roundToInt

// Match frame rate (Homatics Box R 4K Plus report, 25/11/2026): Media3 only
// votes Surface.setFrameRate "only if seamless", and an HDMI box never switches
// 60 -> 24 Hz seamlessly, so the panel stayed at 60 Hz and 23.976 fps judders.
// With "Always" the player asks the window for a display mode whose refresh
// rate is a whole multiple of the video's, at the same physical resolution.
internal object TaxaQuadros {
    const val SEMPRE = 0
    const val SO_SEM_INTERRUPCAO = 1
    const val DESLIGADO = 2

    // 23.976 vs 24 Hz differ by 0.1 %: the tolerance must sit well below that.
    private const val TOLERANCIA = 0.0005f

    data class Modo(val id: Int, val largura: Int, val altura: Int, val hz: Float)

    fun fpsValido(fps: Float) = fps in 10f..121f

    // Whole multiple (1x, 2x, ...) of the video's frame rate.
    fun casa(hz: Float, fps: Float): Boolean {
        if (!fpsValido(fps) || hz <= 0f) return false
        val k = (hz / fps).roundToInt()
        return k >= 1 && abs(hz - k * fps) <= TOLERANCIA * hz
    }

    // The mode to ask for, or null when the current one already fits or the
    // panel has none that does. Prefers the highest matching rate up to 60 Hz
    // (25 fps -> 50 Hz, not a flickering 25 Hz), then the lowest above it.
    fun escolher(modos: List<Modo>, atual: Modo, fps: Float): Modo? {
        if (!fpsValido(fps) || casa(atual.hz, fps)) return null
        val iguais = modos.filter {
            it.largura == atual.largura && it.altura == atual.altura && casa(it.hz, fps)
        }
        return iguais.filter { it.hz <= 61f }.maxByOrNull { it.hz }
            ?: iguais.minByOrNull { it.hz }
    }

    // Standard video rates. A container stores 23.976 as 41708333 ns per frame
    // (23.97602) or rounds it (23.98); measured timestamps jitter more. Snap to
    // the nearest one within 0.4 %, so matching can stay tight enough to tell
    // 23.976 from 24 (0.1 % apart).
    private val PADRAO = floatArrayOf(
        24000f / 1001f, 24f, 25f, 30000f / 1001f, 30f, 48000f / 1001f, 48f,
        50f, 60000f / 1001f, 60f, 100f, 120000f / 1001f, 120f)

    fun padronizar(fps: Float): Float {
        if (!fpsValido(fps)) return fps
        val perto = PADRAO.minByOrNull { abs(it - fps) } ?: return fps
        return if (abs(perto - fps) <= 0.004f * perto) perto else fps
    }

    // Frame rate from presentation timestamps, for a source whose container
    // states none (Media3's MKV reader never fills Format.frameRate). Frames
    // arrive in decode order, so sort first; the median gap ignores the odd
    // dropped or repeated frame, and the mean of the gaps near it undoes the
    // 1 ms rounding of MKV timestamps (41/42 ms for 23.976). Written on the
    // playback thread, read on the main one.
    class Medidor(private val amostras: Int = 48) {
        private val pts = LongArray(amostras)
        private var n = 0
        @Volatile private var feito = false
        val cheio get() = feito

        private var ult = 0L

        // A jump of more than SALTO_US (seek, discontinuity) starts the sample
        // over: two ranges sorted together give short, random gaps.
        @Synchronized fun quadro(ptsUs: Long) {
            if (n > 0 && abs(ptsUs - ult) > SALTO_US) n = 0
            ult = ptsUs
            if (n < amostras) pts[n++] = ptsUs
            if (n >= amostras) feito = true
        }

        @Synchronized fun fps(): Float {
            if (n < amostras / 2) return -1f
            val o = pts.copyOf(n).sorted()
            val gaps = (1 until o.size).map { o[it] - o[it - 1] }.filter { it > 0 }.sorted()
            if (gaps.size < amostras / 4) return -1f
            val mediana = gaps[gaps.size / 2]
            val perto = gaps.filter { it * 2 >= mediana && it * 2 <= mediana * 3 }
            val media = perto.sum().toFloat() / perto.size
            // Only a standard rate: half of it (every other frame dropped at
            // start) would pick a 60 Hz mode for 23.976.
            val fps = padronizar(1_000_000f / media)
            return if (PADRAO.any { it == fps }) fps else -1f
        }

        private companion object { const val SALTO_US = 500_000L }
    }
}
