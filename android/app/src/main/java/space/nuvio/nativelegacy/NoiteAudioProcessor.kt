package space.nuvio.nativelegacy

import androidx.media3.common.C
import androidx.media3.common.audio.AudioProcessor
import androidx.media3.common.audio.BaseAudioProcessor
import androidx.media3.common.util.UnstableApi
import java.nio.ByteBuffer
import java.nio.ByteOrder

// 2.1 night mode as a Media3 AudioProcessor, placed before GanhoAudioProcessor
// in the sink (NvPlayer: buildAudioSink). Same limits as the volume boost: it
// only sees decoded 16-bit or float PCM; passthrough/offload never reaches
// processors, so the mode does nothing there. The channel count and format go
// out unchanged (no downmix): the TV/receiver still gets what it negotiated.
//
// `nivel` is shared by every sink of the process and read per buffer, so the
// setting applies live and needs no reconfigure. Off = the buffer is copied
// untouched.
@androidx.annotation.OptIn(UnstableApi::class)
class NoiteAudioProcessor : BaseAudioProcessor() {
    companion object { @Volatile var nivel = NoiteMath.DESLIGADO }

    private var dsp: NoiteMath.Dsp? = null
    private var quadro = FloatArray(0)

    override fun onConfigure(inputAudioFormat: AudioProcessor.AudioFormat): AudioProcessor.AudioFormat =
        if ((inputAudioFormat.encoding == C.ENCODING_PCM_16BIT || inputAudioFormat.encoding == C.ENCODING_PCM_FLOAT) &&
            inputAudioFormat.channelCount in 1..NoiteMath.CANAIS_MAX) inputAudioFormat
        else AudioProcessor.AudioFormat.NOT_SET

    override fun onFlush() { dsp?.reiniciar() }
    override fun onReset() { dsp = null }

    override fun queueInput(inputBuffer: ByteBuffer) {
        val n = inputBuffer.remaining()
        if (n == 0) return
        val out = replaceOutputBuffer(n)
        val src = inputBuffer.duplicate().order(ByteOrder.nativeOrder())
        val nv = NoiteMath.nivel(nivel)
        val canais = inputAudioFormat.channelCount
        var d = dsp
        if (nv != NoiteMath.DESLIGADO && (d == null || d.nivel != nv || d.canais != canais)) {
            d = NoiteMath.Dsp(nv, canais, inputAudioFormat.sampleRate); dsp = d
            if (quadro.size != canais) quadro = FloatArray(canais)
        }
        if (nv == NoiteMath.DESLIGADO || d == null || !d.ativo) {
            out.put(src)
        } else if (inputAudioFormat.encoding == C.ENCODING_PCM_16BIT) {
            while (src.remaining() >= 2 * canais) {
                for (c in 0 until canais) quadro[c] = src.short / 32768f
                d.quadro(quadro)
                for (c in 0 until canais) out.putShort(NoiteMath.pcm16(quadro[c]))
            }
            out.put(src)
        } else {
            while (src.remaining() >= 4 * canais) {
                for (c in 0 until canais) quadro[c] = src.float
                d.quadro(quadro)
                for (c in 0 until canais) out.putFloat(quadro[c])
            }
            out.put(src)
        }
        inputBuffer.position(inputBuffer.limit())
        out.flip()
    }
}
