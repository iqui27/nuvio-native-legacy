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
}
