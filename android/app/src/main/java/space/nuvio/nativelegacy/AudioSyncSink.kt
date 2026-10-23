package space.nuvio.nativelegacy

import androidx.media3.common.C
import androidx.media3.common.Format
import androidx.media3.common.MimeTypes
import androidx.media3.common.util.UnstableApi
import androidx.media3.exoplayer.audio.AudioSink
import androidx.media3.exoplayer.audio.ForwardingAudioSink
import java.nio.ByteBuffer

// F06: o AudioSink de sempre (DefaultAudioSink do DefaultRenderersFactory) com
// uma escuta na ENTRADA. handleBuffer traz o tempo de midia de cada buffer,
// coisa que o AudioProcessor/TeeAudioProcessor do Media3 1.8 nao da.
// Non-normal speed requires decoded PCM so Sonic can change the audio clock.
@androidx.annotation.OptIn(UnstableApi::class)
class AudioSyncSink(sink: AudioSink, private val tap: AudioSyncTap,
                    private val forcePcm: () -> Boolean = { false }) : ForwardingAudioSink(sink) {
    override fun supportsFormat(format: Format): Boolean =
        getFormatSupport(format) != AudioSink.SINK_FORMAT_UNSUPPORTED

    override fun getFormatSupport(format: Format): Int =
        if (forcePcm() && format.sampleMimeType != MimeTypes.AUDIO_RAW)
            AudioSink.SINK_FORMAT_UNSUPPORTED
        else super.getFormatSupport(format)

    override fun getFormatOffloadSupport(format: Format): androidx.media3.exoplayer.audio.AudioOffloadSupport =
        if (forcePcm()) androidx.media3.exoplayer.audio.AudioOffloadSupport.DEFAULT_UNSUPPORTED
        else super.getFormatOffloadSupport(format)

    override fun configure(inputFormat: Format, specifiedBufferSize: Int, outputChannels: IntArray?) {
        val pcm = MimeTypes.AUDIO_RAW == inputFormat.sampleMimeType
        val enc = when (inputFormat.pcmEncoding) {
            C.ENCODING_PCM_16BIT -> AudioSyncTap.ENC_PCM16
            C.ENCODING_PCM_FLOAT -> AudioSyncTap.ENC_FLOAT
            C.ENCODING_PCM_8BIT -> AudioSyncTap.ENC_PCM8
            C.ENCODING_PCM_24BIT -> AudioSyncTap.ENC_PCM24
            C.ENCODING_PCM_32BIT -> AudioSyncTap.ENC_PCM32
            else -> 0
        }
        tap.configurar(pcm, enc, inputFormat.channelCount, inputFormat.sampleRate)
        super.configure(inputFormat, specifiedBufferSize, outputChannels)
    }

    override fun handleBuffer(buffer: ByteBuffer, presentationTimeUs: Long, encodedAccessUnitCount: Int): Boolean {
        tap.buffer(buffer, presentationTimeUs)
        return super.handleBuffer(buffer, presentationTimeUs, encodedAccessUnitCount)
    }

    override fun flush() {
        tap.descontinuidade()
        super.flush()
    }
}
