@file:OptIn(androidx.media3.common.util.UnstableApi::class)

package space.nuvio.nativelegacy

import android.content.Context
import android.util.Log
import space.nuvio.nativelegacy.audiosync.AudioSubtitleSyncController
import space.nuvio.nativelegacy.audiosync.AudioSyncStatus
import space.nuvio.nativelegacy.audiosync.SubtitleSyncModel
import space.nuvio.nativelegacy.audiosync.SyncLog

// RESERVA POR AUDIO DA SINCRONIA DE LEGENDA (Android, 06/11, dono): a
// AudioSyncFallback do NuvioTV-Reshaped, com o C no lugar do AutoSync dela.
//
// Quem decide e o legsync.c (o mesmo plano em todas as plataformas):
//  * armar   - o plano automatico comecou: ouvir enquanto a faixa embutida e
//              tentada, para o audio ja ter o que ouvir se ela recusar;
//  * assumir - a embutida recusou (ou nao ha): sincronizar ESTA legenda pelo
//              audio. As falas vem do documento que o C ja baixou e leu;
//  * parar   - a embutida aceitou, a pessoa trocou de legenda/fonte, Parar.
// O resultado (trechos: tempo da legenda -> tempo do video) vai ao C a cada
// mudanca e o overlay o aplica como um mapa aceito do AutoSync. As outras
// legendas do mesmo idioma (add-ons) sao testadas contra o mesmo audio e uma
// que encaixa pode entrar no lugar (o C confere a regra de uma troca por
// episodio). Tudo no fio principal, exceto os retornos do controlador
// (status/troca), que so chamam o C.
internal class ReservaAudio(context: Context) {
    val controlador = AudioSubtitleSyncController(
        context = context,
        onStatus = ::status,
        requestSwitch = ::trocar,
        fetchCues = ::falasDe,
    )

    /** URL (chave do C) da legenda que o audio sincroniza agora; null = nenhuma. */
    @Volatile private var alvo: String? = null
    private var aplicado: SubtitleSyncModel? = null
    private var aplicadoEstimado = false

    init {
        // Como a AudioSyncFallback: nada e decodificado antes do plano armar.
        controlador.listensBeforeSession = false
    }

    fun armar(dadosMoveis: Boolean) {
        parar()
        controlador.enabled = true
        controlador.samplingOnMobileData = dadosMoveis
        controlador.listensBeforeSession = true
        controlador.setReferenceSubtitles(candidatas())
        SyncLog.i("armed: listening while the embedded reference decides")
    }

    fun assumir(chave: String, idioma: String, falas: List<Triple<Long, Long, String>>) {
        alvo = chave
        aplicado = null; aplicadoEstimado = false
        controlador.enabled = true
        controlador.listensBeforeSession = true
        controlador.setReferenceSubtitles(candidatas())
        SyncLog.i("taking over: syncing the chosen subtitle (${falas.size} lines) to the audio")
        controlador.startSession(chave, idioma, falas)
    }

    fun parar() {
        val tinha = alvo != null
        alvo = null
        aplicado = null; aplicadoEstimado = false
        controlador.listensBeforeSession = false
        controlador.stopSession()
        if (tinha) SyncLog.i("stopped")
    }

    /** A cada tique do player (250 ms, fio principal). */
    fun tique(posicaoMs: Long, duracaoMs: Long) {
        controlador.onPlaybackPosition(posicaoMs, duracaoMs)
        val chave = alvo ?: return
        val modelo = controlador.currentModel()
        val estimado = modelo != null && controlador.isEstimate()
        if (modelo === aplicado && estimado == aplicadoEstimado) return
        aplicado = modelo; aplicadoEstimado = estimado
        val trechos = modelo?.segments.orEmpty()
        try {
            NvPlayer.nativeAudioSyncModelo(
                chave,
                LongArray(trechos.size) { trechos[it].fromMediaMs },
                DoubleArray(trechos.size) { trechos[it].scale },
                DoubleArray(trechos.size) { trechos[it].shiftMs },
                estimado,
            )
        } catch (_: UnsatisfiedLinkError) {}
    }

    fun liberar() {
        parar()
        controlador.release()
    }

    private fun status(s: AudioSyncStatus) {
        val chave = alvo ?: return
        val motivo = when (s) {
            is AudioSyncStatus.TooFewLines -> "too few dialogue lines (${s.lines})"
            AudioSyncStatus.Unavailable -> "speech detector unavailable"
            else -> null
        }
        if (motivo == null) { SyncLog.d("status $s"); return }
        try { NvPlayer.nativeAudioSyncFalhou(chave, motivo) } catch (_: UnsatisfiedLinkError) {}
    }

    // Fio do alinhador: o C decide (uma troca por episodio) e liga a legenda
    // pelo mesmo caminho da escolha da pessoa; a sessao nova adota o mapa.
    private fun trocar(url: String) {
        val chave = alvo ?: return
        try { NvPlayer.nativeAudioSyncTrocar(chave, url) } catch (_: UnsatisfiedLinkError) {}
    }

    companion object {
        private const val TAG = "NvPlayer"
        private const val RS = '\u001e'
        private const val US = '\u001f'

        // As legendas dos add-ons desta sessao: url, idioma, nome (src/video_android.c).
        fun candidatas(): List<AudioSubtitleSyncController.ReferenceCandidate> {
            val bruto = try { NvPlayer.nativeAudioSyncCandidatas() } catch (_: UnsatisfiedLinkError) { null } ?: return emptyList()
            return bruto.split(RS).mapNotNull { linha ->
                val c = linha.split(US)
                if (c.size < 3 || c[0].isEmpty()) null
                else AudioSubtitleSyncController.ReferenceCandidate(url = c[0], language = c[1], label = c[2])
            }
        }

        // Baixa e le pelo caminho das legendas do C (rede, charset, SRT/ASS/VTT).
        // Bloqueante: so nos fios do controlador.
        fun falasDe(url: String): List<Triple<Long, Long, String>> {
            val bruto = try { NvPlayer.nativeAudioSyncFalas(url) } catch (_: UnsatisfiedLinkError) { null }
                ?: return emptyList()
            return decodificar(bruto)
        }

        fun decodificar(bruto: String): List<Triple<Long, Long, String>> =
            bruto.split(RS).mapNotNull { linha ->
                val c = linha.split(US, limit = 3)
                if (c.size < 3) return@mapNotNull null
                val a = c[0].toLongOrNull() ?: return@mapNotNull null
                val b = c[1].toLongOrNull() ?: return@mapNotNull null
                Triple(a, b, c[2])
            }

        fun log(texto: String) {
            Log.i(TAG, texto)
            try { NvPlayer.nativeAudioLog(texto) } catch (_: UnsatisfiedLinkError) {}
        }
    }
}
