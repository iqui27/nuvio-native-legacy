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
// boost is then unavailable; the sink-input format (SaidaAudioSink -> nativeAudioEstado) tells C.
//
// The gain is a volatile read per buffer: changes apply live, no reconfigure.
@androidx.annotation.OptIn(UnstableApi::class)
class GanhoAudioProcessor : BaseAudioProcessor() {
    @Volatile var ganho = 1f

    override fun onConfigure(inputAudioFormat: AudioProcessor.AudioFormat): AudioProcessor.AudioFormat =
        if (inputAudioFormat.encoding == C.ENCODING_PCM_16BIT || inputAudioFormat.encoding == C.ENCODING_PCM_FLOAT)
            inputAudioFormat
        else AudioProcessor.AudioFormat.NOT_SET

    override fun queueInput(inputBuffer: ByteBuffer) {
        val n = inputBuffer.remaining()
        if (n == 0) return
        val out = replaceOutputBuffer(n)
        val g = ganho
        val src = inputBuffer.duplicate().order(ByteOrder.nativeOrder())
        if (g <= 1f) {
            out.put(src)
        } else if (inputAudioFormat.encoding == C.ENCODING_PCM_16BIT) {
            while (src.remaining() >= 2) out.putShort(GanhoMath.pcm16(src.short, g))
        } else {
            while (src.remaining() >= 4) out.putFloat(GanhoMath.amostra(src.float, g))
        }
        inputBuffer.position(inputBuffer.limit())
        out.flip()
    }
}
