package space.nuvio.nativelegacy

import androidx.media3.common.C
import androidx.media3.common.Format
import androidx.media3.common.MimeTypes
import androidx.media3.common.util.UnstableApi
import androidx.media3.exoplayer.audio.AudioSink
import androidx.media3.exoplayer.audio.ForwardingAudioSink

// O AudioSink de sempre (DefaultAudioSink do DefaultRenderersFactory) com duas
// coisas na ENTRADA:
//  * informa ao C se o audio chega como PCM ou bitstream (passthrough/offload):
//    o reforco de volume (F07, cacheboost) depende disso (nativeAudioEstado);
//  * velocidade diferente de 1x exige PCM decodificado (o Sonic muda o relogio).
// Ate 06/11 tambem copiava o PCM para a sincronia por audio em C (AudioSyncTap,
// audsync.c); essa sincronia agora e a do NuvioTV-Reshaped (audiosync/), com a
// escuta dela (PlaybackAudioTap) dentro deste sink.
@androidx.annotation.OptIn(UnstableApi::class)
class SaidaAudioSink(sink: AudioSink, private val estado: (Int) -> Unit,
                     private val forcePcm: () -> Boolean = { false }) : ForwardingAudioSink(sink) {
    companion object {
        const val FMT_NENHUM = 0
        const val FMT_PCM = 1
        const val FMT_BITSTREAM = 2
    }

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
        val raw = MimeTypes.AUDIO_RAW == inputFormat.sampleMimeType
        val pcm = raw && inputFormat.channelCount > 0 && inputFormat.sampleRate > 0 && when (inputFormat.pcmEncoding) {
            C.ENCODING_PCM_8BIT, C.ENCODING_PCM_16BIT, C.ENCODING_PCM_24BIT, C.ENCODING_PCM_32BIT, C.ENCODING_PCM_FLOAT -> true
            else -> false
        }
        estado(if (pcm) FMT_PCM else if (raw) FMT_NENHUM else FMT_BITSTREAM)
        super.configure(inputFormat, specifiedBufferSize, outputChannels)
    }
}
