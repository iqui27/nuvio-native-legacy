package space.nuvio.nativelegacy

import kotlin.math.abs
import kotlin.math.exp
import kotlin.math.log10
import kotlin.math.pow
import kotlin.math.roundToInt
import kotlin.math.tanh

// 2.1 NIGHT MODE / DIALOGUE CLARITY, the math. Pure Kotlin, and a line-by-line
// port of the C reference in src/modonoite.c (tested in tests/modonoite.c):
// the numbers below must stay equal to modonoite_param. Centre channel up,
// surrounds and LFE down, then a linked feed-forward compressor (peak envelope
// over all channels, static curve above the threshold, fixed make-up gain) and
// the same tanh soft limiter as GanhoMath.
//
// Channel order is the Android/Media3 one: FL FR FC LFE BL BR SL SR. Layouts
// without a centre (1, 2 and 4 channels) get the compressor only.
object NoiteMath {
    const val DESLIGADO = 0
    const val SUAVE = 1
    const val FORTE = 2
    const val CANAIS_MAX = 8
    const val FRENTE = 0
    const val CENTRO = 1
    const val LFE = 2
    const val SURROUND = 3

    class Param(val limiarDb: Float, val razao: Float, val compensaDb: Float, val centroDb: Float,
                val surroundDb: Float, val lfeDb: Float, val ataqueMs: Float, val soltaMs: Float)

    private val P = arrayOf(
        Param(0f, 1f, 0f, 0f, 0f, 0f, 5f, 300f),
        Param(-24f, 2f, 6f, 3f, -3f, -3f, 5f, 300f),
        Param(-32f, 4f, 12f, 6f, -6f, -9f, 5f, 300f),
    )

    fun nivel(n: Int) = if (n > DESLIGADO && n <= FORTE) n else DESLIGADO
    fun param(n: Int) = P[nivel(n)]

    fun ganhoDb(n: Int, entradaDb: Double): Double {
        val p = param(n)
        if (entradaDb <= p.limiarDb) return p.compensaDb.toDouble()
        return p.compensaDb - (entradaDb - p.limiarDb) * (1.0 - 1.0 / p.razao)
    }

    fun canal(canais: Int, i: Int): Int {
        if (i < 0 || i >= canais || canais < 3 || canais == 4 || canais > CANAIS_MAX) return FRENTE
        if (i < 2) return FRENTE
        if (i == 2) return CENTRO
        if (canais >= 6 && i == 3) return LFE
        return SURROUND
    }

    private fun linear(db: Float): Float = 10.0.pow(db / 20.0).toFloat()

    private fun limitar(y: Float): Float {
        val a = abs(y)
        if (a <= GanhoMath.JOELHO) return y
        val faixa = 1f - GanhoMath.JOELHO
        val c = GanhoMath.JOELHO + faixa * tanh((a - GanhoMath.JOELHO) / faixa)
        return if (y < 0f) -c else c
    }

    // One stream: state of the envelope between buffers.
    class Dsp(nivel: Int, val canais: Int, taxaHz: Int) {
        val nivel = NoiteMath.nivel(nivel)
        val ativo = this.nivel != DESLIGADO && canais in 1..CANAIS_MAX && taxaHz >= 8000
        private val p = param(this.nivel)
        private val ganhoCanal = FloatArray(maxOf(canais, 1)) { i ->
            linear(when (canal(canais, i)) { CENTRO -> p.centroDb; LFE -> p.lfeDb; SURROUND -> p.surroundDb; else -> 0f })
        }
        private val ataque = if (ativo) 1f - exp(-1000f / (p.ataqueMs * taxaHz)) else 0f
        private val solta = if (ativo) 1f - exp(-1000f / (p.soltaMs * taxaHz)) else 0f
        private var env = 0f

        fun reiniciar() { env = 0f }

        // One interleaved frame (-1..1) in place.
        fun quadro(q: FloatArray) {
            var pico = 0f
            for (c in 0 until canais) {
                q[c] *= ganhoCanal[c]
                val a = abs(q[c])
                if (a > pico) pico = a
            }
            env += (pico - env) * (if (pico > env) ataque else solta)
            val db = 20f * log10(if (env > 1e-6f) env else 1e-6f)
            val g = linear(ganhoDb(nivel, db.toDouble()).toFloat())
            for (c in 0 until canais) q[c] = limitar(q[c] * g)
        }
    }

    fun pcm16(x: Float): Short = (x * 32768f).roundToInt().coerceIn(-32768, 32767).toShort()
}
