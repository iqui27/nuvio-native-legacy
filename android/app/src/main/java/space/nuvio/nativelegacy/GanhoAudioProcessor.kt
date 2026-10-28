package space.nuvio.nativelegacy

import androidx.media3.common.C
import androidx.media3.common.audio.AudioProcessor
import androidx.media3.common.audio.BaseAudioProcessor
import androidx.media3.common.util.UnstableApi
import java.nio.ByteBuffer
import java.nio.ByteOrder

// F07 (1.8): the 100..200% volume as a Media3 AudioProcessor in the audio
// sink (NvPlayer: DefaultRenderersFactory.buildAudioSink). A processor and not
// LoudnessEnhancer: it works for every PCM output and with the FFmpeg decoder,
// needs no audio session effect support from the TV, and its math is testable
// (GanhoMath). DefaultAudioSink hands processors 16-bit or float PCM; anything
// else leaves it inactive. Passthrough/offload never reaches processors: the
// boost is then unavailable; the sink-input format (F06 AudioSyncSink -> nativeAudioEstado) tells C.
//
// The gain is a volatile read per buffer: changes apply live, no reconfigure.
@androidx.annotation.OptIn(UnstableApi::class)
class GanhoAudioProcessor : BaseAudioProcessor() {
    @Volatile var ganho = 1f
    @Volatile var normalizar = false
    private var nivel = 1f
    override fun onFlush() { nivel = 1f }

    override fun onConfigure(inputAudioFormat: AudioProcessor.AudioFormat): AudioProcessor.AudioFormat =
        if (inputAudioFormat.encoding == C.ENCODING_PCM_16BIT || inputAudioFormat.encoding == C.ENCODING_PCM_FLOAT)
            inputAudioFormat
        else AudioProcessor.AudioFormat.NOT_SET

    override fun queueInput(inputBuffer: ByteBuffer) {
        val n = inputBuffer.remaining()
        if (n == 0) return
        val out = replaceOutputBuffer(n)
        var g = ganho
        val src = inputBuffer.duplicate().order(ByteOrder.nativeOrder())
        if (normalizar) {
            val probe = src.duplicate().order(ByteOrder.nativeOrder())
            var squares = 0.0
            var count = 0
            val pcm16 = inputAudioFormat.encoding == C.ENCODING_PCM_16BIT
            while (probe.remaining() >= if (pcm16) 2 else 4) {
                val sample = if (pcm16) probe.short / 32768.0 else probe.float.toDouble()
                squares += sample * sample
                count++
            }
            if (count > 0) {
                val rms = kotlin.math.sqrt(squares / count)
                val target = if (rms < .001) 1f else (.15 / rms).toFloat().coerceIn(.25f, 4f)
                val seconds = count.toDouble() / (inputAudioFormat.sampleRate * inputAudioFormat.channelCount)
                val mix = (1 - kotlin.math.exp(-seconds / if (target < nivel) .03 else 1.0)).toFloat()
                nivel += (target - nivel) * mix
                g *= nivel
            }
        } else nivel = 1f
        if (g == 1f) {
            out.put(src)
        } else if (inputAudioFormat.encoding == C.ENCODING_PCM_16BIT) {
            while (src.remaining() >= 2) {
                val sample = src.short
                out.putShort(if (g < 1f) (sample * g).toInt().toShort() else GanhoMath.pcm16(sample, g))
            }
        } else {
            while (src.remaining() >= 4) {
                val sample = src.float
                out.putFloat(if (g < 1f) sample * g else GanhoMath.amostra(sample, g))
            }
        }
        inputBuffer.position(inputBuffer.limit())
        out.flip()
    }
}
