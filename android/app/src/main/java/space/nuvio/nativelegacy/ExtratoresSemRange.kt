package space.nuvio.nativelegacy

import android.net.Uri
import androidx.media3.extractor.DefaultExtractorsFactory
import androidx.media3.extractor.Extractor
import androidx.media3.extractor.ExtractorsFactory
import androidx.media3.extractor.mkv.MatroskaExtractor
import androidx.media3.extractor.text.SubtitleParser

// Extratores da sessao do player. Quando o ParaleloDataSource ja viu o
// servidor ignorar Range (SessaoSemRange), o MKV abre com
// FLAG_DISABLE_SEEK_FOR_CUES: toca do inicio, sem seek, em vez de pedir a
// posicao do Cues no fim do arquivo (o que baixaria o arquivo inteiro ate la).
//
// O Media3 chama createExtractors DEPOIS do primeiro open() da carga
// (ProgressiveMediaPeriod: open, depois progressiveMediaExtractor.init), entao a
// marca da primeira resposta ja vale para o extrator dessa mesma carga.
//
// O DefaultMediaSourceFactory configura o parser de legendas pela interface;
// tudo e repassado aos dois conjuntos para que so o flag do MKV mude.
class ExtratoresSemRange(private val semRange: SessaoSemRange) : ExtractorsFactory {
    private val normal = DefaultExtractorsFactory()
    private val semCues = DefaultExtractorsFactory()
        .setMatroskaExtractorFlags(MatroskaExtractor.FLAG_DISABLE_SEEK_FOR_CUES)

    private fun atual(): ExtractorsFactory = if (semRange.visto) semCues else normal

    override fun createExtractors(): Array<Extractor> = atual().createExtractors()

    override fun createExtractors(uri: Uri, responseHeaders: Map<String, List<String>>): Array<Extractor> =
        atual().createExtractors(uri, responseHeaders)

    override fun setSubtitleParserFactory(subtitleParserFactory: SubtitleParser.Factory): ExtractorsFactory {
        normal.setSubtitleParserFactory(subtitleParserFactory)
        semCues.setSubtitleParserFactory(subtitleParserFactory)
        return this
    }

    @Suppress("OVERRIDE_DEPRECATION")
    override fun experimentalSetTextTrackTranscodingEnabled(textTrackTranscodingEnabled: Boolean): ExtractorsFactory {
        @Suppress("DEPRECATION")
        normal.experimentalSetTextTrackTranscodingEnabled(textTrackTranscodingEnabled)
        @Suppress("DEPRECATION")
        semCues.experimentalSetTextTrackTranscodingEnabled(textTrackTranscodingEnabled)
        return this
    }

    override fun experimentalSetCodecsToParseWithinGopSampleDependencies(codecsToParseWithinGopSampleDependencies: Int): ExtractorsFactory {
        normal.experimentalSetCodecsToParseWithinGopSampleDependencies(codecsToParseWithinGopSampleDependencies)
        semCues.experimentalSetCodecsToParseWithinGopSampleDependencies(codecsToParseWithinGopSampleDependencies)
        return this
    }
}
