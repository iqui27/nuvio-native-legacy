package space.nuvio.nativelegacy

import android.app.Activity
import android.content.Context
import android.net.Uri
import android.graphics.Bitmap
import android.view.PixelCopy
import android.widget.ImageView
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.os.SystemClock
import android.util.Log
import android.view.Gravity
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.widget.FrameLayout
import androidx.media3.common.AudioAttributes
import androidx.media3.common.C
import androidx.media3.common.Format
import androidx.media3.common.MediaItem
import androidx.media3.common.MimeTypes
import androidx.media3.common.PlaybackException
import androidx.media3.common.Player
import androidx.media3.common.Timeline
import androidx.media3.common.TrackSelectionOverride
import androidx.media3.common.Tracks
import androidx.media3.common.VideoSize
import androidx.media3.common.text.CueGroup
import androidx.media3.common.util.UnstableApi
import androidx.media3.datasource.DefaultDataSource
import androidx.media3.datasource.DefaultHttpDataSource
import androidx.media3.datasource.DataSourceException
import androidx.media3.datasource.HttpDataSource
import androidx.media3.exoplayer.DecoderReuseEvaluation
import androidx.media3.datasource.DataSource
import androidx.media3.datasource.DataSpec
import androidx.media3.datasource.TransferListener
import androidx.media3.exoplayer.DefaultLoadControl
import androidx.media3.exoplayer.DefaultRendererCapabilitiesList
import androidx.media3.exoplayer.ExoPlaybackException
import androidx.media3.exoplayer.ExoPlayer
import androidx.media3.exoplayer.hls.HlsManifest
import androidx.media3.exoplayer.LoadControl
import androidx.media3.exoplayer.Renderer
import androidx.media3.exoplayer.RendererCapabilities
import androidx.media3.exoplayer.analytics.AnalyticsListener
import androidx.media3.exoplayer.audio.AudioSink
import androidx.media3.exoplayer.audio.DefaultAudioSink
import androidx.media3.exoplayer.mediacodec.MediaCodecDecoderException
import androidx.media3.exoplayer.source.DefaultMediaSourceFactory
import androidx.media3.exoplayer.source.MediaSource
import androidx.media3.exoplayer.source.WrappingMediaSource
import androidx.media3.exoplayer.source.preload.PreloadException
import androidx.media3.exoplayer.source.preload.PreloadMediaSource
import androidx.media3.exoplayer.trackselection.DefaultTrackSelector
import androidx.media3.exoplayer.upstream.DefaultAllocator
import androidx.media3.exoplayer.upstream.DefaultBandwidthMeter
import java.io.InterruptedIOException
import java.util.concurrent.atomic.AtomicInteger

// Player do Nuvio no Android TV: Media3 ExoPlayer numa SurfaceView ATRAS da
// SDLSurface (o C abre um furo transparente por onde ela aparece, ver
// src/video_android.c). Mesma ABI do host .NET do .tpk (tizen-tpk/Video.cs):
// o C pede por abrir/parar/pausar/buscar/volume/janela/escolher de QUALQUER
// fio e tudo aqui passa ao fio principal; o que acontece volta ao C pelos
// natives, sempre do fio principal.
//
// Sessao monotonica: cada abrir/parar sobe `sessao`, e todo callback confere o
// numero em que nasceu. Um player velho nao fala mais depois de liberado.
//
// Eventos (nativeEvento): 1 PRONTO(durMs) 2 TOCANDO 3 PAUSADO 4 FIM 5 ERRO(cod, C.TRACK_TYPE_* do renderer)
// 6 TAMANHO(w,h) 7 BUFFER(pct) 8 PRIMEIRO_QUADRO, e a extensao 9 = a fonte tem
// audio mas nenhuma faixa tem decoder aqui (o C responde video_audio_nao_suportado).
//
// Tipo 2 do escolher() = atraso de legenda em ms: fica GUARDADO aqui e vale
// adiando a entrega de cada cue ao C (so atraso positivo; adiantar um cue que
// ainda nao chegou nao da). O estilo inteiro e do C.
@androidx.annotation.OptIn(UnstableApi::class)
object NvPlayer {
    private const val TAG = "NvPlayer"
    private const val EV_PRONTO = 1
    private const val EV_TOCANDO = 2
    private const val EV_PAUSADO = 3
    private const val EV_FIM = 4
    private const val EV_ERRO = 5
    private const val EV_TAMANHO = 6
    private const val EV_BUFFER = 7
    private const val EV_PRIMEIRO_QUADRO = 8
    private const val EV_AUDIO_SEM_DECODER = 9
    private const val EV_SUPERFICIE_ESTAVEL = 10
    private const val EV_LIVE_RECOVERY = 11
    private const val EV_LIVE_SEGMENT = 12

    private const val TELA_W = 1920   // coordenadas de layout do app
    private const val TELA_H = 1080
    private const val FAIXAS_MAX = 64 // NV_FAIXA_MAX do C
    private const val TIQUE_MS = 250L
    private const val RETRY_DECODER_MS = 5000L

    private val principal = Handler(Looper.getMainLooper())
    private var activity: Activity? = null
    // isLowRamDevice/memoryClass medidos no criarPlayer (true = pouca RAM;
    // false tambem antes de medir). CargaPlayer le sem Context.
    @Volatile internal var poucaRamAparelho = false
    private var camada: FrameLayout? = null
    private var interfaceMobile = false
    private var superficie: SurfaceView? = null
    // #202: a TCL segurou o AudioFlinger por 27-38 s ao soltar o audio do filme
    // anterior; release() no fio principal congelava o app. Cada abertura tem a
    // sua SurfaceView e o player velho e solto num fio proprio.
    private val liberacoesEmCurso = AtomicInteger()
    private var ouvinteAtual: Player.Listener? = null
    private var analiticoAtual: AnalyticsListener? = null

    // Tudo abaixo so no fio principal.
    private var player: ExoPlayer? = null
    private var sessao = 0
    // Invalida um abrir/seek ainda na fila antes de o fio principal executa-lo.
    // A sessao acima segue protegendo os callbacks do ExoPlayer liberado.
    private val pedidos = AtomicInteger()
    private var pedidoAtivo = 0
    private var inicioAtualMs = 0
    // 2.0.3: retomada so com percentual (centesimos de ponto, 0 = nada). Fica
    // pendente ate a timeline trazer a duracao do container; aplicada antes
    // do primeiro quadro, o C nao busca depois do "pronto".
    private var fracaoAtual = 0
    private var fracaoPendente = 0
    private var geracaoNative = 0
    @Volatile private var limitedNext = false
    private var limitedSession = false
    @JvmStatic fun limitedConnections(on: Int) { limitedNext = on != 0 }
    private var urlAtual = ""
    private var cabAtual = ""
    private var abriuEm = 0L
    private var retentou = false
    private var recuperacaoLive: LiveWindowRecovery? = null
    private var pronto = false
    private var duracaoEnviada = -1
    private var videoW = 0
    private var videoH = 0
    private var temJanela = false
    private var jx = 0
    private var jy = 0
    private var jw = TELA_W
    private var jh = TELA_H
    private var atrasoMs = 0
    private var audios = ArrayList<Pair<Tracks.Group, Int>>()
    private var legendas = ArrayList<Pair<Tracks.Group, Int>>()
    private var assinatura = ""
    private var decoderDv = false
    private var ultHdr = ""
    private var ultDv = -1
    private var ultAtmos = -1
    private var avisouSemAudio = false
    private var quadrosPerdidos = 0L
    private var statsTiques = 0
    private var bufferAlvoMs = androidx.media3.exoplayer.DefaultLoadControl.DEFAULT_MAX_BUFFER_MS
    private var bufferMinMs = 0
    private var bufferBytes = 0

    // --- natives (src/video_android.c, Java_space_nuvio_nativelegacy_NvPlayer_*) ---
    @JvmStatic external fun nativeIniciar()
    @JvmStatic external fun nativeLimitedStatus(): Int
    @JvmStatic external fun nativeTunneling(): Boolean
    @JvmStatic external fun nativeEvento(tipo: Int, a: Int, b: Int)
    // flags (#287): 1 = forcada (SELECTION_FLAG_FORCED), 2 = SDH
    // (ROLE_FLAG_DESCRIBES_MUSIC_AND_SOUND); nome = Format.label.
    // mime/canais (#293): codec e canais da faixa de audio.
    @JvmStatic external fun nativeFaixa(tipo: Int, idx: Int, lingua: String, flags: Int, nome: String, mime: String, canais: Int)
    @JvmStatic external fun nativeFaixasFim(selAudio: Int, selLeg: Int)
    @JvmStatic external fun nativeLegenda(texto: String, durMs: Int)
    @JvmStatic external fun nativePos(ms: Long, bufferFimMs: Long, live: Boolean,
                                      sampleBytes: Int, geracao: Int)
    @JvmStatic external fun nativeDecoder4k(hevc: Int, avc: Int, vp9: Int, av1: Int)
    @JvmStatic external fun nativeTela(hdr: Int, dv: Int)
    @JvmStatic external fun nativeHdr(hdr: String, dv: Int, atmos: Int)
    @JvmStatic external fun nativeErroTexto(texto: String)
    @JvmStatic external fun nativeAudioLog(texto: String)
    @JvmStatic external fun nativeRetomada(geracao: Int, aceita: Int)
    @JvmStatic external fun nativeFitPassiva(rede: Long, geracao: Int, origem: String, kbps: IntArray, fimMs: Long)
    @JvmStatic external fun nativeAudioPcm(pcm: ShortArray, n: Int, ptsUs: Long)
    @JvmStatic external fun nativeAudioEstado(fmt: Int)
    // Estatisticas ao vivo (src/estatvivo.h), 1x/s pelo tique: bytes do socket
    // (-1 = esta abertura nao passou pelo ParaleloDataSource, ex. HLS), buffer
    // a frente e o alvo do DefaultLoadControl em ms, conexoes abertas, 1o byte,
    // quadros perdidos (onDroppedVideoFrames) e entregues; host final.
    @JvmStatic external fun nativeStats(bytes: Long, bufferMs: Int, alvoMs: Int, conexoes: Int,
                                        primeiroByteMs: Int, perdidos: Long, quadros: Long, host: String,
                                        minMs: Int, alvoMb: Int, carregando: Int)

    // F06 sincronia por audio (AudioSyncTap.kt / AudioSyncSink.kt). Fio de
    // reproducao; o C so copia para um anel limitado (src/audsync.c).
    private var tap = AudioSyncTap(
        { pcm, n, pts -> try { nativeAudioPcm(pcm, n, pts) } catch (t: UnsatisfiedLinkError) { } },
        { fmt -> try { nativeAudioEstado(fmt) } catch (t: UnsatisfiedLinkError) { } }
    )
    private var recursosAtuais: RecursosPlayer? = null

    // F07 (src/video_android.c -> cacheboost.h): seek cache state of this open.
    // The audio output state for the boost is F06's nativeAudioEstado (one
    // detection for both: PCM vs bitstream at the sink input).
    @JvmStatic external fun nativeCache(estado: Int, pedidoMb: Int, limiteMb: Int, usadoMb: Int)

    // --- F07: seek cache and volume boost ---------------------------------------
    // The cache limit is STICKY (the C side sends it only when it changes) and
    // read at each open; any thread writes it, the open reads it on main.
    @Volatile private var cacheMbPedido = 0
    // Warm resume (#492): id of the title of the NEXT open ("" = trailer or
    // channel: it neither keeps nor drops the kept cache) and of the cache
    // being kept when this one closes.
    @Volatile private var tituloPedido = ""
    private var tituloAberto = ""
    private var chaveAberta = ""
    @Volatile private var mornoLigado = true
    private var quadroBitmap: Bitmap? = null
    private var quadroTitulo = ""
    private var quadroUrl = ""
    private var quadroAte = 0L
    private var quadroPedido = 0
    private var quadroView: ImageView? = null
    @Volatile private var quadroVisivel = false
    private var capturando = false
    @JvmStatic fun morno(ligado: Int) {
        mornoLigado = ligado != 0
        if (!mornoLigado) principal.post { esquecerQuadro(); CacheMidia.descartar("warm resume off") }
    }
    private fun esconderQuadro() {
        quadroVisivel = false
        quadroView?.let { (it.parent as? FrameLayout)?.removeView(it) }
        quadroView = null
    }
    private fun esquecerQuadro() {
        quadroPedido++; esconderQuadro()
        quadroBitmap?.recycle(); quadroBitmap = null
        quadroTitulo = ""; quadroUrl = ""
    }
    // PixelCopy reads the hardware Surface, not SDL's transparent framebuffer.
    // Never wait for it on exit. One small bitmap and one request, no decoder held.
    @JvmStatic fun quadro(acao: Int): Int {
        if (acao == 3) return if (quadroVisivel) 1 else 0
        principal.post {
            if (acao == 0) { esquecerQuadro(); return@post }
            if (acao == 2) { mostrarQuadro(quadroUrl, quadroTitulo); return@post }
            if (acao != 1 || !mornoLigado || capturando || tituloAberto.isEmpty() || !quadroVisto) return@post
            val sv = superficie ?: return@post
            if (!sv.holder.surface.isValid) return@post
            val titulo = tituloAberto; val url = urlAtual; val token = ++quadroPedido
            val b = try { Bitmap.createBitmap(480, (480 * videoH / videoW.coerceAtLeast(1)).coerceIn(1, 480), Bitmap.Config.ARGB_8888) }
                    catch (_: OutOfMemoryError) { return@post }
            capturando = true
            try {
                PixelCopy.request(sv, b, { resultado ->
                    capturando = false
                    if (resultado == PixelCopy.SUCCESS && token == quadroPedido && mornoLigado) {
                        quadroBitmap?.recycle(); quadroBitmap = b; quadroTitulo = titulo; quadroUrl = url
                        quadroAte = SystemClock.elapsedRealtime() + CacheSessao.LIMITE_MS
                    } else b.recycle()
                }, principal)
            } catch (_: Exception) { capturando = false; b.recycle() }
        }
        return 0
    }
    private fun mostrarQuadro(url: String, titulo: String) {
        esconderQuadro()
        if (titulo.isEmpty()) return // Home trailer leaves the last title snapshot alone.
        if (!mornoLigado || SystemClock.elapsedRealtime() >= quadroAte || titulo != quadroTitulo || url != quadroUrl) { esquecerQuadro(); return }
        val b = quadroBitmap ?: return
        val act = activity ?: return; val c = camada ?: return
        val v = ImageView(act)
        v.setBackgroundColor(android.graphics.Color.BLACK)
        v.scaleType = ImageView.ScaleType.FIT_CENTER
        v.setImageBitmap(b); v.isFocusable = false
        c.addView(v, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT))
        quadroView = v; quadroVisivel = true
    }
    private var cacheMidia: CacheMidia? = null
    private var cacheEstado = CacheSessao.DESLIGADO
    private var cacheTiques = 0
    // Volume of this open (0..200) and the processor of its audio sink.
    private var ganhoPct = 100
    // With bitstream (passthrough) the processor is bypassed by the sink, so
    // its gain needs no passthrough state here; C caps the row at 100%.
    private var processador: GanhoAudioProcessor? = null
    private const val CACHE_RELATO_TIQUES = 8      // 8 x 250 ms: usage every 2 s

    private fun relatarCache(minha: Int) {
        if (!atual(minha)) return
        val c = cacheMidia
        val ativo = c != null && cacheEstado == CacheSessao.ATIVO
        try {
            nativeCache(cacheEstado, cacheMbPedido, if (ativo) c!!.limiteMb else 0, if (ativo) c!!.usadoMb() else 0)
        } catch (e: UnsatisfiedLinkError) { Log.w(TAG, "cache sem lib: $e") }
    }

    private fun aplicarGanho() {
        player?.volume = GanhoMath.volumePlayer(ganhoPct)
        processador?.ganho = GanhoMath.reforco(ganhoPct)
    }

    // StreamFit passivo (PassivoMedidor.kt). Entrega de qualquer fio (os
    // pedacos baixam em fios proprios); o C so trava um mutex curto.
    private val medidor = PassivoMedidor(
        { SystemClock.elapsedRealtime() }, { System.currentTimeMillis() }, { PassivoMedidor.redeGlobal }
    ) { e ->
        try { nativeFitPassiva(e.rede, e.geracao, e.origem, e.kbps, e.fimMs) }
        catch (t: UnsatisfiedLinkError) { Log.w(TAG, "passivo sem lib: $t") }
    }
    private var medidorToken = 0L
    private fun medirEstado(p: ExoPlayer) {
        val st = p.playbackState
        medidor.estado(medidorToken,
            p.playWhenReady && (st == Player.STATE_READY || st == Player.STATE_BUFFERING), pronto)
    }

    private fun atual(minha: Int) = minha == sessao && pedidoAtivo == pedidos.get()
    private fun confirmarRetomada(geracao: Int, aceita: Boolean) {
        if (geracao != 0) try { nativeRetomada(geracao, if (aceita) 1 else 0) }
        catch (e: UnsatisfiedLinkError) { Log.w(TAG, "retomada sem lib: $e") }
    }

    // #345: uploaded app logs do not include Kotlin's logcat diagnostics.
    private fun relatarAudio(minha: Int, texto: String) {
        if (!atual(minha)) return
        val linha = "sessao=$minha $texto"
        Log.i(TAG, "[audio] $linha")
        try { nativeAudioLog(linha) } catch (_: UnsatisfiedLinkError) { }
    }

    // Exception messages may include stream Format.id/label or URLs (#345).
    private fun camposErroAudio(erro: Throwable): String {
        val formato = when (erro) {
            is AudioSink.InitializationException -> erro.format
            is AudioSink.WriteException -> erro.format
            is AudioSink.ConfigurationException -> erro.format
            else -> null
        }
        val codigo = when (erro) {
            is AudioSink.WriteException -> erro.errorCode
            is MediaCodecDecoderException -> erro.errorCode
            is android.media.MediaCodec.CodecException -> erro.errorCode
            else -> null
        }
        val recuperavel = when (erro) {
            is AudioSink.InitializationException -> erro.isRecoverable
            is AudioSink.WriteException -> erro.isRecoverable
            is android.media.MediaCodec.CodecException -> erro.isRecoverable
            else -> null
        }
        return "erro=${erro.javaClass.simpleName}" +
            (if (erro is AudioSink.UnexpectedDiscontinuityException)
                " expectedUs=${erro.expectedPresentationTimeUs} actualUs=${erro.actualPresentationTimeUs}" else "") +
            (codigo?.let { " errorCode=$it" } ?: "") +
            (formato?.let { " hz=${it.sampleRate} canais=${it.channelCount} encoding=${it.pcmEncoding} mime=${it.sampleMimeType}" } ?: "") +
            (recuperavel?.let { " isRecoverable=$it" } ?: "")
    }

    // Fatal cause chains retain only class names and allowlisted numeric fields.
    private fun camposErroCausa(erro: Throwable): String {
        return erro.javaClass.simpleName + when (erro) {
            is ExoPlaybackException -> " errorCode=${erro.errorCode} type=${erro.type} rendererIndex=${erro.rendererIndex}"
            is PlaybackException -> " errorCode=${erro.errorCode}"
            is HttpDataSource.InvalidResponseCodeException -> " responseCode=${erro.responseCode} errorCode=${erro.reason}"
            is DataSourceException -> " errorCode=${erro.reason}"
            is AudioSink.WriteException -> " errorCode=${erro.errorCode}"
            is MediaCodecDecoderException -> " errorCode=${erro.errorCode}"
            is android.media.MediaCodec.CodecException -> " errorCode=${erro.errorCode}"
            else -> ""
        }
    }

    private fun ev(tipo: Int, a: Int = 0, b: Int = 0) {
        try { nativeEvento(tipo, a, b) } catch (e: UnsatisfiedLinkError) { Log.w(TAG, "evento $tipo sem lib: $e") }
    }

    // One silent, surface-free player. Its request counter is independent of
    // the visible session: even a queued prepare is invalidated by Back/stop.
    private data class ConsultaPreparo(val token: Int, val estado: Int)
    private val travaPreparo = Any()
    private val pedidosPreparo = AtomicInteger()
    @Volatile private var consultaPreparo = ConsultaPreparo(0, 0)
    @Volatile private var preparo: Preparo? = null
    private const val PREPARO_MAX_US = 5_000_000L
    private const val PREPARO_ESPERA_MS = 30_000L

    private class CargaPlayer(val minMs: Int, maxMs: Int, val bytes: Int, preparando: Boolean) :
        DefaultLoadControl(DefaultAllocator(true, C.DEFAULT_BUFFER_SEGMENT_SIZE),
            minMs, maxMs, 2_500, 5_000, bytes, false, 0, false) {
        @Volatile var adotada = !preparando
        @Volatile var cancelada = false
        // At most 1/16 of the heap (1/32 on low-RAM, where the active player
        // and the parallel downloader already press the limit), with the same
        // heap ceiling as normal play.
        private val tetoPreparo = (Runtime.getRuntime().maxMemory() / (if (poucaRamAparelho) 32 else 16))
            .coerceIn(8L shl 20, (if (poucaRamAparelho) 8L else 16L) shl 20).coerceAtMost(bytes.toLong()).toInt()
        fun esperarCapacidade() {
            // HLS/DASH can load a whole large segment between LoadControl
            // checks. Pause reads at the same allocator ceiling as progressive
            // media, retaining the paused player instead of letting that chunk
            // overrun the preparation budget. Loader cancellation interrupts it.
            while (!adotada && !cancelada && allocator.totalBytesAllocated >= tetoPreparo) {
                try { Thread.sleep(20) }
                catch (_: InterruptedException) {
                    Thread.currentThread().interrupt()
                    throw InterruptedIOException("preparation canceled")
                }
            }
            if (cancelada) throw InterruptedIOException("preparation canceled")
        }
        fun cheio() = allocator.totalBytesAllocated >= tetoPreparo
        override fun shouldContinueLoading(parameters: LoadControl.Parameters): Boolean =
            if (adotada) super.shouldContinueLoading(parameters)
            else parameters.bufferedDurationUs < PREPARO_MAX_US && allocator.totalBytesAllocated < tetoPreparo
        override fun shouldStartPlayback(parameters: LoadControl.Parameters): Boolean =
            if (adotada) super.shouldStartPlayback(parameters)
            else parameters.bufferedDurationUs >= 2_500_000L || allocator.totalBytesAllocated >= tetoPreparo
    }

    // Adoption keeps the samples already buffered. At its next progressive
    // read, the loader continues at the exact unread byte through the ordinary
    // parallel/cache chain; it never downloads the initial range a second time.
    // Starting with ParaleloDataSource would allocate its 4 MB prefetch chunks
    // outside LoadControl's allocator, defeating the preparation memory cap.
    private class OrigemPlayer(@Volatile var fabrica: DataSource.Factory, private val carga: CargaPlayer) : DataSource.Factory {
        override fun createDataSource(): DataSource = object : DataSource {
            private var atual: DataSource? = null
            private var fabricaAberta: DataSource.Factory? = null
            private var spec: DataSpec? = null
            private var lidos = 0L
            private val ouvintes = ArrayList<TransferListener>()
            override fun open(dataSpec: DataSpec): Long {
                spec = dataSpec; lidos = 0L
                return abrirFonte(dataSpec, fabrica)
            }
            private fun abrirFonte(dataSpec: DataSpec, fonte: DataSource.Factory): Long {
                val d = fonte.createDataSource()
                for (o in ouvintes) d.addTransferListener(o)
                atual = d
                fabricaAberta = fonte
                return d.open(dataSpec)
            }
            override fun read(buffer: ByteArray, offset: Int, length: Int): Int {
                carga.esperarCapacidade()
                val s = spec
                val fonte = fabrica
                if (s != null && fonte !== fabricaAberta && ParaleloDataSource.serve(s.uri) &&
                    (s.length == C.LENGTH_UNSET.toLong() || lidos < s.length)) {
                    // AIO/debrid resolution already happened during preparation.
                    // Continue at the redirected media URL, with the same headers.
                    val finalUri = atual?.uri ?: s.uri
                    val restante = s.buildUpon().setUri(finalUri)
                        .setKey(s.key ?: s.uri.toString()).build().subrange(lidos)
                    atual?.close()
                    abrirFonte(restante, fonte)
                }
                val n = atual?.read(buffer, offset, length) ?: C.RESULT_END_OF_INPUT
                if (n > 0) lidos += n
                return n
            }
            override fun getUri(): Uri? = atual?.uri
            override fun getResponseHeaders(): Map<String, List<String>> = atual?.responseHeaders ?: emptyMap()
            override fun close() {
                try { atual?.close() } finally { atual = null; spec = null; fabricaAberta = null }
            }
            override fun addTransferListener(transferListener: TransferListener) {
                ouvintes.add(transferListener)
                atual?.addTransferListener(transferListener)
            }
        }
    }

    private class SeletorAudioPreparo(context: Context, private val preferencia: String) :
        DefaultTrackSelector(context) {

        private data class FaixaAudio(
            val renderer: Int,
            val grupo: androidx.media3.common.TrackGroup,
            val indice: Int,
            val idioma: String,
            val selecionada: Boolean
        )

        override fun selectAllTracks(
            mappedTrackInfo: androidx.media3.exoplayer.trackselection.MappingTrackSelector.MappedTrackInfo,
            rendererFormatSupports: Array<Array<IntArray>>,
            rendererMixedMimeTypeAdaptationSupports: IntArray,
            params: DefaultTrackSelector.Parameters
        ): Array<androidx.media3.exoplayer.trackselection.ExoTrackSelection.Definition?> {
            val definicoes = super.selectAllTracks(mappedTrackInfo, rendererFormatSupports,
                rendererMixedMimeTypeAdaptationSupports, params)
            if (preferencia.isEmpty()) return definicoes

            // TrackSelectionUtil.buildTracks and publicarFaixas use this exact
            // renderer/group/track order. C receives only FORMAT_HANDLED audio,
            // capped at FAIXAS_MAX, not every format the selector can try playing.
            val faixas = ArrayList<FaixaAudio>(FAIXAS_MAX)
            publicar@ for (r in 0 until mappedTrackInfo.rendererCount) {
                if (mappedTrackInfo.getRendererType(r) != C.TRACK_TYPE_AUDIO) continue
                val grupos = mappedTrackInfo.getTrackGroups(r)
                for (g in 0 until grupos.length) {
                    val grupo = grupos[g]
                    if (grupo.type != C.TRACK_TYPE_AUDIO) continue
                    for (i in 0 until grupo.length) {
                        if (mappedTrackInfo.getTrackSupport(r, g, i) != C.FORMAT_HANDLED) continue
                        val selecao = definicoes[r]
                        faixas.add(FaixaAudio(r, grupo, i, codigoPublicado(grupo.getFormat(i).language),
                            selecao != null && selecao.group == grupo && i in selecao.tracks))
                        if (faixas.size == FAIXAS_MAX) break@publicar
                    }
                }
            }
            if (faixas.size < 2) return definicoes

            // Tracks marks every member of an adaptive definition selected. C
            // keeps the FIRST such published member, or audioAtual=0 if none was
            // published as selected. Preserve the full definition in either case.
            val atual = faixas.firstOrNull { it.selecionada } ?: faixas[0]
            if (atual.idioma.isNotEmpty() && casa(atual.idioma, preferencia)) return definicoes
            val alvo = faixas.firstOrNull { it.idioma.isNotEmpty() && casa(it.idioma, preferencia) }
                ?: return definicoes

            for (r in definicoes.indices) {
                if (mappedTrackInfo.getRendererType(r) == C.TRACK_TYPE_AUDIO) definicoes[r] = null
            }
            definicoes[alvo.renderer] = androidx.media3.exoplayer.trackselection.ExoTrackSelection.Definition(
                alvo.grupo, alvo.indice)
            return definicoes
        }

        // The native path first copies Format.language through deJString's 16-byte
        // buffer, rejects ONLY these exact lower-case sentinels, then snprintf
        // copies at most seven UTF-8 bytes into VideoFaixa.idioma[8]. Latin-1 here
        // is a lossless byte key: truncation may end inside a UTF-8 character.
        private fun codigoPublicado(raw: String?): String {
            if (raw == null) return ""
            val bytes = ByteArray(15)
            var usados = 0
            var i = 0
            while (i < raw.length && usados + 5 < 16) {
                var c = raw[i++].code
                if (c in 0xD800 until 0xDC00 && i < raw.length && raw[i].code in 0xDC00 until 0xE000) {
                    c = 0x10000 + ((c - 0xD800) shl 10) + (raw[i++].code - 0xDC00)
                }
                // Later strcmp/snprintf stop at the first NUL in the native buffer.
                if (c == 0) break
                when {
                    c < 0x80 -> bytes[usados++] = c.toByte()
                    c < 0x800 -> {
                        bytes[usados++] = (0xC0 or (c shr 6)).toByte()
                        bytes[usados++] = (0x80 or (c and 0x3F)).toByte()
                    }
                    c < 0x10000 -> {
                        bytes[usados++] = (0xE0 or (c shr 12)).toByte()
                        bytes[usados++] = (0x80 or ((c shr 6) and 0x3F)).toByte()
                        bytes[usados++] = (0x80 or (c and 0x3F)).toByte()
                    }
                    else -> {
                        bytes[usados++] = (0xF0 or (c shr 18)).toByte()
                        bytes[usados++] = (0x80 or ((c shr 12) and 0x3F)).toByte()
                        bytes[usados++] = (0x80 or ((c shr 6) and 0x3F)).toByte()
                        bytes[usados++] = (0x80 or (c and 0x3F)).toByte()
                    }
                }
            }
            val recebido = String(bytes, 0, usados, Charsets.ISO_8859_1)
            if (recebido == "und" || recebido == "unknown" || recebido == "zxx") return ""
            return recebido.take(7)
        }

        // Match the native byte-wise ASCII language codes without locale-dependent
        // Unicode folding, trimming, ranking or Media3 preferred-language scoring.
        private fun minusculasAscii(codigo: String): String = buildString(codigo.length) {
            for (c in codigo) append(if (c in 'A'..'Z') (c.code + 32).toChar() else c)
        }

        private fun familia(codigo: String): String {
            val baixo = minusculasAscii(codigo)
            familias[baixo]?.let { return it }
            return baixo.takeWhile { it != '-' && it != '_' }.take(7)
        }

        // codigo is already a native published byte key. The request preference
        // comes from C's ling_audio(); encode it as the same UTF-8 byte key.
        private fun casa(codigo: String, pref: String): Boolean {
            if (pref.isEmpty()) return true
            if (codigo.isEmpty()) return false
            val p = String(pref.toByteArray(Charsets.UTF_8), Charsets.ISO_8859_1).substringBefore('\u0000')
            if (p.isEmpty()) return true
            if (minusculasAscii(codigo) == minusculasAscii(p)) return true
            val a = familia(codigo)
            val b = familia(p)
            if (a == b) return true
            if (a == "sh" || b == "sh") {
                val outro = if (a == "sh") b else a
                return outro == "hr" || outro == "sr" || outro == "bs"
            }
            return false
        }

        companion object {
            // Exact immutable familia() aliases from src/linguas.c. No selector,
            // player, selection parameters or mutable lifecycle state is shared.
            private val familias = mapOf(
                "pob" to "pt", "por" to "pt", "ptb" to "pt", "pt-br" to "pt", "pt_br" to "pt", "br" to "pt",
                "eng" to "en", "en-us" to "en", "en_us" to "en", "en-gb" to "en", "en_gb" to "en",
                "spa" to "es", "esp" to "es", "fre" to "fr", "fra" to "fr", "ger" to "de", "deu" to "de",
                "ita" to "it", "jpn" to "ja", "kor" to "ko", "chi" to "zh", "zho" to "zh",
                "rus" to "ru", "ara" to "ar", "hin" to "hi", "dut" to "nl", "nld" to "nl",
                "swe" to "sv", "nor" to "no", "dan" to "da", "fin" to "fi", "pol" to "pl",
                "tur" to "tr", "heb" to "he", "tha" to "th", "cze" to "cs", "gre" to "el",
                "hun" to "hu", "rum" to "ro", "ron" to "ro", "ukr" to "uk", "vie" to "vi", "ind" to "id",
                "ces" to "cs", "ell" to "el", "slk" to "sk", "slo" to "sk", "bul" to "bg",
                "msa" to "ms", "may" to "ms", "srp" to "sr", "scc" to "sr", "hrv" to "hr",
                "scr" to "hr", "slv" to "sl", "est" to "et", "lav" to "lv", "lit" to "lt",
                "cat" to "ca", "eus" to "eu", "baq" to "eu", "glg" to "gl", "isl" to "is",
                "ice" to "is", "fas" to "fa", "per" to "fa", "fil" to "tl", "tgl" to "tl",
                "nob" to "no", "nno" to "no", "nb" to "no", "nn" to "no", "iw" to "he",
                "chs" to "zh", "cht" to "zh", "zht" to "zh", "zhe" to "zh", "ze" to "zh",
                "pb" to "pt", "ptbr" to "pt", "spn" to "es", "ea" to "es",
                "jp" to "ja", "cz" to "cs", "du" to "nl", "gr" to "el", "in" to "id", "ma" to "ms",
                "bos" to "bs", "hbs" to "sh"
            )
        }
    }

    private class RecursosPlayer(val carga: CargaPlayer, val alvoMs: Int, val idiomaAudio: String = "") {
        lateinit var p: ExoPlayer
        lateinit var origem: OrigemPlayer
        lateinit var rede: (Long) -> DataSource.Factory
        lateinit var renderizadores: androidx.media3.exoplayer.DefaultRenderersFactory
        lateinit var fabricaMidia: DefaultMediaSourceFactory
        var fontePrecarregada: PreloadMediaSource? = null
        var fioPreparo: HandlerThread? = null
        var seletorPreparo: DefaultTrackSelector? = null
        var capacidadesPreparo: androidx.media3.exoplayer.RendererCapabilitiesList? = null
        var bandaPreparo: DefaultBandwidthMeter? = null
        var liberado = false
        @Volatile var adotado = carga.adotada
        @Volatile var audioFormato = AudioSyncTap.FMT_NENHUM
        @Volatile var velocidadePcm = false
        val proc = GanhoAudioProcessor()
        val tap = AudioSyncTap(
            { pcm, n, pts -> if (adotado) try { nativeAudioPcm(pcm, n, pts) } catch (_: UnsatisfiedLinkError) {} },
            { fmt ->
                audioFormato = fmt
                if (adotado) try { nativeAudioEstado(fmt) } catch (_: UnsatisfiedLinkError) {}
            }
        )
        var cache: CacheMidia? = null
        var cacheEstado = CacheSessao.DESLIGADO
        fun construido() = ::p.isInitialized
        fun promover(token: Long) {
            origem.fabrica = rede(token)
            carga.adotada = true
            adotado = true
        }
    }

    private class Preparo(val token: Int, val pedido: Int, val url: String, val cab: String,
                          val inicioMs: Int, val fracao: Int, val buffer: Int, val cacheMb: Int, val proximo: Boolean = false,
                          val idiomaAudio: String = "", val kbps: Int = 0) {
        var recursos: RecursosPlayer? = null
        var ouvinte: Player.Listener? = null
        var analitico: AnalyticsListener? = null
        @Volatile var fracaoPendente = if (inicioMs <= 0) fracao else 0
        @Volatile var inicioAceito = false
        @Volatile var posicaoFonteMs = inicioMs.toLong()
        @Volatile var duracaoFonteMs = C.TIME_UNSET
        @Volatile var fonteBusca = false
        @Volatile var faixasFonteProntas = false
        var decoderDv = false
    }

    private fun preparoAtual(s: Preparo) = preparo === s && s.pedido == pedidosPreparo.get()
    private fun publicarPreparo(s: Preparo, estado: Int) {
        synchronized(travaPreparo) {
            if (preparoAtual(s) && consultaPreparo.token == s.token)
                consultaPreparo = ConsultaPreparo(s.token, estado)
        }
    }

    private fun soltarPreparo() {
        val s = preparo ?: return
        preparo = null
        val r = s.recursos ?: return
        if (r.construido()) {
            try { s.ouvinte?.let { r.p.removeListener(it) } } catch (_: Exception) {}
            try { s.analitico?.let { r.p.removeAnalyticsListener(it) } } catch (_: Exception) {}
        }
        soltarRecursosPreparo(r)
    }

    private fun liberarFontePreparo(r: RecursosPlayer) {
        r.fontePrecarregada?.releasePreloadMediaSource()
        val fio = r.fioPreparo ?: return
        Handler(fio.looper).post {
            r.seletorPreparo?.release()
            r.capacidadesPreparo?.release()
            (r.carga.allocator as? DefaultAllocator)?.reset()
            fio.quitSafely()
        }
    }

    private fun soltarRecursosPreparo(r: RecursosPlayer) {
        if (r.liberado) return
        r.liberado = true
        r.adotado = false
        r.carga.cancelada = true
        if (r.construido()) liberarEmFundo(r.p, null) { liberarFontePreparo(r) }
        else liberarFontePreparo(r)
        r.cache?.liberar()
    }

    @JvmOverloads @JvmStatic fun preparar(url: String, cabecalhos: String, inicioMs: Int, fracao: Int,
                           token: Int, buffer: Int, cacheMb: Int, idiomaAudio: String = "", kbps: Int = 0) {
        enfileirarPreparo(url, cabecalhos, inicioMs, fracao, token, buffer, cacheMb, false, idiomaAudio, kbps)
    }

    @JvmOverloads @JvmStatic fun prepararProximo(url: String, cabecalhos: String, inicioMs: Int, fracao: Int,
                                  token: Int, buffer: Int, cacheMb: Int, idiomaAudio: String = "", kbps: Int = 0) {
        enfileirarPreparo(url, cabecalhos, inicioMs, fracao, token, buffer, cacheMb, true, idiomaAudio, kbps)
    }

    private fun enfileirarPreparo(url: String, cabecalhos: String, inicioMs: Int, fracao: Int,
                                 token: Int, buffer: Int, cacheMb: Int, proximo: Boolean, idiomaAudio: String, kbps: Int) {
        if (token <= 0 || url.isEmpty() || limitedNext) return
        val pedido = synchronized(travaPreparo) {
            val n = pedidosPreparo.incrementAndGet()
            consultaPreparo = ConsultaPreparo(token, 1)
            n
        }
        principal.post {
            if (pedido != pedidosPreparo.get()) return@post
            soltarPreparo()
            val s = Preparo(token, pedido, url, cabecalhos, inicioMs.coerceAtLeast(0),
                if (inicioMs > 0) 0 else fracao.coerceIn(0, 9999), buffer.coerceIn(0, 256), cacheMb.coerceIn(0, 1024), proximo, idiomaAudio, kbps.coerceIn(0, 400_000))
            preparo = s
            prepararMain(s, SystemClock.elapsedRealtime())
        }
    }

    private fun prepararMain(s: Preparo, espera: Long) {
        if (!preparoAtual(s)) return
        val act = activity
        if (!s.proximo && liberacoesEmCurso.get() > 0 && SystemClock.elapsedRealtime() - espera < 3000L) {
            principal.postDelayed({ prepararMain(s, espera) }, 25)
            return
        }
        if (act == null || (!s.proximo && (player != null || liberacoesEmCurso.get() > 0))) {
            publicarPreparo(s, -1)
            return
        }
        try {
            val r = criarPlayer(act, s.url, s.cab, s.buffer, s.cacheMb, true, 0, !s.proximo, s.idiomaAudio, kbps = s.kbps)
            s.recursos = r
            if (s.proximo) {
                prepararFonteProxima(act, s, r)
                agendarPrazoPreparo(s)
                return
            }
            val p = r.p
            p.volume = 0f
            p.playWhenReady = false
            val ouv = object : Player.Listener {
                override fun onTimelineChanged(timeline: Timeline, reason: Int) {
                    if (!preparoAtual(s) || s.fracaoPendente <= 0) return
                    if (p.isCurrentMediaItemLive) { s.fracaoPendente = 0; return }
                    val d = p.duration
                    if (d == C.TIME_UNSET || d <= 1000L || !p.isCurrentMediaItemSeekable) return
                    val f = s.fracaoPendente
                    s.fracaoPendente = 0
                    try { p.seekTo(d * f / 10000L); s.inicioAceito = true }
                    catch (e: Exception) { Log.w(TAG, "preparo: retomada recusada: $e") }
                }
                override fun onPlaybackStateChanged(state: Int) {
                    if (!preparoAtual(s)) return
                    if (state == Player.STATE_READY) {
                        if (s.fracao > 0 && !s.inicioAceito) {
                            publicarPreparo(s, -1)
                            soltarPreparo()
                        } else publicarPreparo(s, 2)
                    } else if (state == Player.STATE_BUFFERING) publicarPreparo(s, 1)
                    else if (state == Player.STATE_ENDED) {
                        publicarPreparo(s, -1)
                        soltarPreparo()
                    }
                }
                override fun onPlayerError(error: PlaybackException) {
                    if (!preparoAtual(s)) return
                    Log.w(TAG, "[preparo] falhou: ${error.errorCodeName}")
                    publicarPreparo(s, -1)
                    soltarPreparo()
                }
            }
            val ana = object : AnalyticsListener {
                override fun onVideoDecoderInitialized(eventTime: AnalyticsListener.EventTime, decoderName: String,
                                                       initializedTimestampMs: Long, initializationDurationMs: Long) {
                    if (preparoAtual(s)) s.decoderDv = decoderDolby(decoderName)
                }
            }
            s.ouvinte = ouv; s.analitico = ana
            p.addListener(ouv); p.addAnalyticsListener(ana)
            val item = itemMidia(s.url)
            if (s.inicioMs > 0) { p.setMediaItem(item, s.inicioMs.toLong()); s.inicioAceito = true }
            else p.setMediaItem(item)
            p.prepare()
            Log.i(TAG, "[preparo] iniciado token=${s.token} pos=${s.inicioMs} fracao=${s.fracao}")
            agendarPrazoPreparo(s)
        } catch (e: Exception) {
            Log.w(TAG, "[preparo] criar: $e")
            publicarPreparo(s, -1)
            soltarPreparo()
        }
    }

    private fun agendarPrazoPreparo(s: Preparo) {
        principal.postDelayed({
            if (preparoAtual(s) && consultaPreparo.estado == 1) {
                publicarPreparo(s, -1)
                soltarPreparo()
            }
        }, PREPARO_ESPERA_MS)
    }

    private class FonteComDuracao(fonte: MediaSource, private val receber: (Timeline) -> Unit) : WrappingMediaSource(fonte) {
        override fun onChildSourceInfoRefreshed(newTimeline: Timeline) {
            receber(newTimeline)
            super.onChildSourceInfoRefreshed(newTimeline)
        }
    }

    private fun prepararFonteProxima(act: Activity, s: Preparo, r: RecursosPlayer) {
        // Sample queues only. Capabilities must have their own descriptors:
        // querying a live audio renderer would enter the current AudioSink from
        // this other looper. These renderers are never enabled, so they create
        // no MediaCodec, AudioTrack, Surface, focus or playback callbacks.
        val fio = HandlerThread("nv-next-preload").apply { start() }
        r.fioPreparo = fio
        val h = Handler(fio.looper)
        val banda = DefaultBandwidthMeter.Builder(act).build() // do not change current playback's ABR meter
        r.bandaPreparo = banda
        val seletor = SeletorAudioPreparo(act, s.idiomaAudio)
        seletor.init({}, banda)
        seletor.parameters = seletor.parameters.buildUpon().setTrackTypeDisabled(C.TRACK_TYPE_TEXT, true).build()
        r.seletorPreparo = seletor
        val descritores = androidx.media3.exoplayer.DefaultRenderersFactory(act)
            .setExtensionRendererMode(androidx.media3.exoplayer.DefaultRenderersFactory.EXTENSION_RENDERER_MODE_ON)
            .setEnableDecoderFallback(true)
        val lista = DefaultRendererCapabilitiesList.Factory(descritores).createRendererCapabilitiesList()
        r.capacidadesPreparo = lista
        val capacidades: Array<RendererCapabilities> = lista.rendererCapabilities
        val fonte = FonteComDuracao(r.fabricaMidia.createMediaSource(itemMidia(s.url))) { timeline ->
            if (!timeline.isEmpty) {
                val w = timeline.getWindow(0, Timeline.Window())
                s.duracaoFonteMs = w.durationMs
                s.fonteBusca = w.isSeekable && !w.isLive
            }
        }
        fun falhou() {
            principal.post {
                if (preparoAtual(s)) { publicarPreparo(s, -1); soltarPreparo() }
            }
        }
        fun retomar(mediaSource: PreloadMediaSource): Boolean {
            val d = s.duracaoFonteMs
            if (!s.fonteBusca || d == C.TIME_UNSET || d <= 1000L) { falhou(); return false }
            s.posicaoFonteMs = d * s.fracaoPendente / 10000L
            s.fracaoPendente = 0
            s.inicioAceito = true
            s.faixasFonteProntas = false
            // The preliminary progressive period has read the container. Stop
            // its preload callback first, then create the exact-position period
            // against the same prepared source on an independent handler.
            h.post { if (preparoAtual(s) && !r.adotado) mediaSource.preload(s.posicaoFonteMs * 1000L) }
            return false
        }
        val controle = object : PreloadMediaSource.PreloadControl {
            override fun onSourcePrepared(mediaSource: PreloadMediaSource): Boolean {
                if (!preparoAtual(s)) return false
                return if (s.fracaoPendente > 0 && s.duracaoFonteMs != C.TIME_UNSET) retomar(mediaSource) else true
            }
            override fun onTracksSelected(mediaSource: PreloadMediaSource): Boolean {
                if (!preparoAtual(s)) return false
                if (s.fracaoPendente > 0) return retomar(mediaSource)
                s.faixasFonteProntas = true
                if (s.inicioMs > 0) s.inicioAceito = s.fonteBusca
                return true
            }
            override fun onContinueLoadingRequested(mediaSource: PreloadMediaSource, bufferedDurationUs: Long): Boolean {
                if (!preparoAtual(s)) return false
                val cheio = r.carga.cheio()
                if (s.faixasFonteProntas && s.fracaoPendente == 0 && (bufferedDurationUs >= 2_500_000L || cheio))
                    publicarPreparo(s, 2)
                return !cheio && bufferedDurationUs < PREPARO_MAX_US
            }
            override fun onLoadedToTheEndOfSource(mediaSource: PreloadMediaSource) {
                if (s.faixasFonteProntas && s.fracaoPendente == 0) publicarPreparo(s, 2) else falhou()
            }
            override fun onUsedByPlayer(mediaSource: PreloadMediaSource) { /* retain queues until player creates its period */ }
            override fun onPreloadError(error: PreloadException, mediaSource: PreloadMediaSource) {
                Log.w(TAG, "[preparo] proximo falhou: ${error.javaClass.simpleName}")
                falhou()
            }
        }
        r.fontePrecarregada = PreloadMediaSource.Factory(r.fabricaMidia, controle, seletor, banda,
            capacidades, r.carga.allocator, fio.looper).createMediaSource(fonte)
        r.fontePrecarregada!!.preload(s.inicioMs.toLong() * 1000L)
        // A large adaptive chunk may be paused at the byte ceiling before its
        // loading callback returns. The selected sample queues still qualify.
        val observar = object : Runnable {
            override fun run() {
                if (!preparoAtual(s)) return
                if (s.faixasFonteProntas && s.fracaoPendente == 0 && r.carga.cheio()) publicarPreparo(s, 2)
                principal.postDelayed(this, TIQUE_MS)
            }
        }
        principal.postDelayed(observar, TIQUE_MS)
        Log.i(TAG, "[preparo] proximo iniciado token=${s.token} pos=${s.inicioMs} fracao=${s.fracao}")
    }

    @JvmStatic fun prepararEstado(token: Int): Int {
        val c = consultaPreparo
        return if (token > 0 && c.token == token) c.estado else 0
    }

    @JvmStatic fun cancelarPreparo(token: Int) {
        val pedido = synchronized(travaPreparo) {
            if (token != 0 && token != consultaPreparo.token) return
            val n = pedidosPreparo.incrementAndGet()
            consultaPreparo = ConsultaPreparo(0, 0)
            n
        }
        principal.post { if (pedido == pedidosPreparo.get()) soltarPreparo() }
    }

    @JvmOverloads @JvmStatic fun abrirPreparada(url: String, cabecalhos: String, inicioMs: Int, fracao: Int,
                                 geracao: Int, token: Int, idiomaAudio: String = "") {
        val pedido = pedidos.incrementAndGet()
        val preparoPedido = synchronized(travaPreparo) {
            val n = pedidosPreparo.incrementAndGet()
            consultaPreparo = ConsultaPreparo(0, 0) // a token can be consumed once
            n
        }
        principal.post {
            if (pedido != pedidos.get()) return@post
            val s = preparo
            val inicio = inicioMs.coerceAtLeast(0)
            val f = if (inicio > 0) 0 else fracao.coerceIn(0, 9999)
            val r = s?.recursos
            if (preparoPedido == pedidosPreparo.get() && s != null && r != null && s.token == token &&
                s.url == url && s.cab == cabecalhos && s.inicioMs == inicio && s.fracao == f &&
                s.idiomaAudio == idiomaAudio &&
                s.buffer == bufferPreset && s.cacheMb == cacheMbPedido && (!r.construido() || r.p.playerError == null) &&
                (f == 0 || s.inicioAceito || s.fracaoPendente > 0)) {
                preparo = null
                if (r.construido()) {
                    s.ouvinte?.let { r.p.removeListener(it) }
                    s.analitico?.let { r.p.removeAnalyticsListener(it) }
                }
                abrirMain(url, cabecalhos, false, inicio, geracao, pedido, f, preparada = s)
            } else {
                soltarPreparo()
                abrirMain(url, cabecalhos, false, inicio, geracao, pedido, f)
            }
        }
    }

    // --- ciclo de vida (NuvioActivity) ---------------------------------------

    // UI-thread mode changes use the same coordinate policy as the native canvas.
    fun interfaceModo(mobile: Boolean) {
        interfaceMobile = mobile
        reaplicarJanela()
    }

    // onCreate, DEPOIS do super.onCreate (a libmain.so ja carregada pelo SDL).
    @JvmStatic
    fun iniciar(activity: Activity, camada: FrameLayout) {
        this.activity = activity
        this.camada = camada
        // F07: seek cache folders of a process that died (crash, kill) go now.
        CacheMidia.limparSobras(activity.cacheDir)
        novaSuperficie(activity)
        // A camada so tem tamanho depois do layout: reaplica a janela quando mudar.
        camada.addOnLayoutChangeListener { _, l, t, r, b, ol, ot, or2, ob ->
            if (r - l != or2 - ol || b - t != ob - ot) reaplicarJanela()
        }
        try { nativeIniciar() } catch (e: UnsatisfiedLinkError) {
            Log.w(TAG, "nativeIniciar sem a lib ainda; o C acha a classe pelo ClassLoader da Activity: $e")
        }
        informarTela(activity)
        val d = decoders4k
        try { nativeDecoder4k(d[0], d[1], d[2], d[3]) }
        catch (e: UnsatisfiedLinkError) { Log.w(TAG, "decoder4k sem lib: $e") }
    }

    // Capacidade HDR/Dolby Vision da tela, para o automatico de fontes. Lista
    // vazia ou erro = -1 (desconhecido); nunca assume que a tela NAO tem HDR.
    private fun informarTela(activity: Activity) {
        try {
            @Suppress("DEPRECATION")
            val tipos = activity.windowManager.defaultDisplay.hdrCapabilities?.supportedHdrTypes
            if (tipos == null || tipos.isEmpty()) { nativeTela(-1, -1); return }
            val dv = if (tipos.contains(android.view.Display.HdrCapabilities.HDR_TYPE_DOLBY_VISION)) 1 else 0
            nativeTela(1, dv)
        } catch (e: Throwable) { Log.w(TAG, "informarTela: $e") }
    }

    // #409: uma consulta por processo, independente da resolucao da tela.
    // 24 fps e o piso UHD de filme; nao promete 4K60 nem suporte a todo perfil.
    private val decoders4k: IntArray by lazy {
        val mimes = arrayOf("video/hevc", "video/avc", "video/x-vnd.on2.vp9", "video/av01")
        val resultado = IntArray(4) { -1 }
        try {
            val codecs = android.media.MediaCodecList(android.media.MediaCodecList.REGULAR_CODECS).codecInfos
            if (codecs.isEmpty()) return@lazy resultado
            for ((i, mime) in mimes.withIndex()) {
                var suporta = false
                var incerto = false
                for (codec in codecs) {
                    try {
                        val nome = codec.name.lowercase(java.util.Locale.ROOT)
                        if (codec.isEncoder || nome.endsWith(".secure") ||
                            nome.startsWith("omx.google.") || nome.startsWith("c2.android.") ||
                            nome.startsWith("c2.google.") || nome.contains(".sw.") ||
                            (android.os.Build.VERSION.SDK_INT >= 29 && codec.isSoftwareOnly)) continue
                        if (!codec.supportedTypes.any { it.equals(mime, ignoreCase = true) }) continue
                        val v = codec.getCapabilitiesForType(mime).videoCapabilities
                        if (v == null) { incerto = true; continue }
                        if (v.isSizeSupported(3840, 2160) && v.areSizeAndRateSupported(3840, 2160, 24.0)) {
                            suporta = true
                            break
                        }
                    } catch (e: Exception) { incerto = true; Log.w(TAG, "decoder4k $mime: $e") }
                }
                resultado[i] = if (suporta) 1 else if (incerto) -1 else 0
            }
        } catch (e: Exception) { Log.w(TAG, "decoder4k: $e") }
        resultado
    }

    // onPause: pausa (o C fica sabendo pelo evento 3) e guarda a posicao; o
    // player continua vivo para o onResume nao precisar de nada.
    @JvmStatic
    fun pausarPeloSistema() {
        cancelarPreparo(0)
        // Cancel queued opens but retain callbacks of the current paused session.
        val aindaAtual = pedidoAtivo == pedidos.get()
        val pedido = pedidos.incrementAndGet()
        if (aindaAtual) pedidoAtivo = pedido
        try { player?.pause() } catch (e: Exception) { Log.w(TAG, "pausarPeloSistema: $e") }
    }

    // onDestroy: solta player e superficie.
    @JvmStatic
    fun encerrar() {
        cancelarPreparo(0)
        soltarPreparo()
        pedidos.incrementAndGet()
        liberar()
        esquecerQuadro(); CacheMidia.descartar("app exit")
        val sv = superficie
        if (sv != null) (sv.parent as? FrameLayout)?.removeView(sv)
        superficie = null
        camada = null
        activity = null
    }

    // --- chamadas do C (qualquer fio) -----------------------------------------

    @JvmStatic fun abrir(url: String, cabecalhos: String) { abrirPosicao(url, cabecalhos, 0, 0) }
    @JvmStatic fun abrirPosicao(url: String, cabecalhos: String, inicioMs: Int, geracao: Int) {
        cancelarPreparo(0)
        val pedido = pedidos.incrementAndGet()
        principal.post {
            if (pedido == pedidos.get()) abrirMain(url, cabecalhos, false, inicioMs.coerceAtLeast(0), geracao, pedido)
        }
    }
    @JvmStatic fun abrirRetomada(url: String, cabecalhos: String, inicioMs: Int, fracao: Int, geracao: Int) {
        cancelarPreparo(0)
        val pedido = pedidos.incrementAndGet()
        principal.post {
            if (pedido == pedidos.get())
                abrirMain(url, cabecalhos, false, inicioMs.coerceAtLeast(0), geracao, pedido, fracao.coerceIn(0, 9999))
        }
    }
    @JvmStatic fun parar() {
        cancelarPreparo(0)
        val pedido = pedidos.incrementAndGet()
        principal.post { if (pedido == pedidos.get()) liberar() }
    }
    private var pausaPedida = false
    @JvmStatic fun pausar(p: Int) {
        principal.post { pausaPedida = p != 0; player?.playWhenReady = !pausaPedida }
    }
    // Episode handoff arms this token before the old visible session closes.
    // Stop only that session; the exact selected preload keeps its queues.
    @JvmStatic fun pararMantendoPreparo(token: Int) {
        val preparoPedido = synchronized(travaPreparo) {
            if (token > 0 && consultaPreparo.token == token) pedidosPreparo.get() else -1
        }
        if (preparoPedido < 0) cancelarPreparo(0)
        val pedido = pedidos.incrementAndGet()
        principal.post {
            if (pedido != pedidos.get()) return@post
            if (preparoPedido < 0 || preparoPedido != pedidosPreparo.get()) {
                cancelarPreparo(0)
                soltarPreparo()
            }
            liberar()
        }
    }
    @JvmStatic fun buscar(ms: Int) {
        val pedido = pedidos.get()
        principal.post {
            if (pedido == pedidos.get()) {
                relatarAudio(sessao, "seek alvoMs=$ms")
                player?.seekTo(ms.coerceAtLeast(0).toLong())
            }
        }
    }
    @JvmStatic fun volume(pct: Int) { principal.post { player?.volume = pct.coerceIn(0, 100) / 100f } }
    // F07: cache limit for the NEXT opens (MB, 0 = off) and the session volume
    // 0..200 (above 100 only while the audio is PCM).
    // #338: buffer size in MiB (0 Auto; C already clamped it to free RAM), sticky, read at open.
    @Volatile private var bufferPreset = 0
    @JvmStatic fun buffer(mb: Int) { bufferPreset = mb.coerceIn(0, 256) }
    // Declared kbps of the next open (0 = unknown/trailer), sticky like buffer().
    @Volatile private var kbpsDeclarado = 0
    @JvmStatic fun kbps(k: Int) { kbpsDeclarado = k.coerceIn(0, 400_000) }
    @JvmStatic fun cache(mb: Int) { cacheMbPedido = mb.coerceIn(0, 1024) }
    @JvmStatic fun cacheTitle(id: String) { tituloPedido = id }
    // Profile switch (C side): the kept cache belongs to the old profile.
    @JvmStatic fun descartarGuardado() { principal.post { CacheMidia.descartar("profile switch"); esquecerQuadro() } }
    @JvmStatic fun ganho(pct: Int) {
        principal.post { ganhoPct = GanhoMath.pct(pct); aplicarGanho() }
    }
    // C reapplies speed after each open; keep PCM policy with that player.
    @JvmStatic fun velocidade(centesimos: Int) {
        val v = centesimos.coerceIn(25, 400) / 100f
        val pedido = pedidos.get()
        principal.post {
            if (pedido != pedidos.get()) return@post
            val p = player ?: return@post
            val r = recursosAtuais ?: return@post
            val pcm = v != 1f
            if (r.velocidadePcm != pcm) {
                val pos = p.currentPosition
                r.velocidadePcm = pcm
                // Re-select the audio decoder against the new sink capabilities.
                // stop/prepare keeps the media source, track overrides and pause.
                p.stop()
                p.seekTo(pos)
                p.setPlaybackSpeed(v)
                p.prepare()
            } else p.setPlaybackSpeed(v)
            Log.i(TAG, "[speed] requested=$v parameters=${p.playbackParameters.speed} pcm=$pcm")
        }
    }
    private val janelaPedido = AtomicInteger()
    // Request -> applied on the UI thread (definida) -> acknowledged after a
    // traversal on a valid surface (aplicada). Lifecycle/layout events drop the
    // acknowledgement and rearm it; the SDL-side query never mutates state.
    @Volatile private var janelaDefinida = -1
    @Volatile private var janelaAplicada = -1
    @Volatile private var janelaSuperficieAplicada: SurfaceView? = null
    private var janelaUltimoPedido: List<Any?>? = null
    @JvmStatic fun janelaPronta(): Boolean {
        val sv = superficie ?: return false
        return !sv.isLayoutRequested && sv.visibility == View.VISIBLE &&
            sv.holder.surface.isValid && !recriando && !recriaAgendada &&
            janelaAplicada == janelaPedido.get() && janelaSuperficieAplicada === sv
    }
    private fun observarJanela(sv: SurfaceView) {
        sv.addOnLayoutChangeListener { _, l, t, r, b, ol, ot, or2, ob ->
            if (superficie === sv && (l != ol || t != ot || r != or2 || b != ob))
                recuperarJanela(sv)
        }
    }
    private fun recuperarJanela(sv: SurfaceView) {
        janelaSuperficieAplicada = null
        // A request still queued confirms itself once applied.
        val pedido = janelaPedido.get()
        if (janelaDefinida == pedido) confirmarJanela(sv, pedido)
    }
    private fun confirmarJanela(sv: SurfaceView, pedido: Int) {
        // A queued request is not a laid-out SurfaceView. Acknowledge only
        // after traversal, and never let an old request acknowledge a new one.
        sv.viewTreeObserver.addOnPreDrawListener(object : android.view.ViewTreeObserver.OnPreDrawListener {
            override fun onPreDraw(): Boolean {
                if (pedido != janelaPedido.get() || superficie !== sv || player == null) {
                    sv.viewTreeObserver.removeOnPreDrawListener(this)
                } else if (!sv.isLayoutRequested && sv.visibility == View.VISIBLE &&
                           sv.holder.surface.isValid && !recriando && !recriaAgendada) {
                    janelaSuperficieAplicada = sv
                    janelaAplicada = pedido
                    sv.viewTreeObserver.removeOnPreDrawListener(this)
                }
                return true
            }
        })
        sv.invalidate()
    }
    @Synchronized @JvmStatic fun janela(x: Int, y: Int, w: Int, h: Int, encaixa: Int) {
        val geometria = listOf(x, y, w, h, encaixa, superficie)
        // Crop retries still apply layout, but unchanged requests keep their acknowledgement.
        val pedido = if (geometria == janelaUltimoPedido) janelaPedido.get() else {
            janelaUltimoPedido = geometria
            janelaPedido.incrementAndGet()
        }
        principal.post {
            definirJanela(x, y, w, h, encaixa != 0)
            janelaDefinida = pedido
            val sv = superficie ?: return@post
            confirmarJanela(sv, pedido)
        }
    }
    @JvmStatic fun escolher(tipo: Int, idx: Int) { principal.post { escolherMain(tipo, idx) } }

    // --- abrir / liberar ------------------------------------------------------

    private fun itemMidia(url: String): MediaItem {
        val baixa = url.lowercase()
        val item = MediaItem.Builder().setUri(url)
        if (baixa.contains(".m3u8") || baixa.contains("m3u8?")) item.setMimeType(MimeTypes.APPLICATION_M3U8)
        else if (baixa.contains(".mpd")) item.setMimeType(MimeTypes.APPLICATION_MPD)
        return item.build()
    }

    private fun decoderDolby(nome: String): Boolean {
        val n = nome.lowercase()
        return n.contains("dolby") || Regex("""\.dv(he|h1|av|a1)""").containsMatchIn(n)
    }

    // Cold opens and prepared opens share renderers, headers, cache, decoder
    // fallback and memory policy. Only preload's audio callbacks and source
    // factory stay dormant until this same player becomes the active session.
    private fun criarPlayer(act: Activity, url: String, cabecalhos: String, preset: Int,
                            pedidoCache: Int, preparando: Boolean, geracao: Int, construir: Boolean = true,
                            idiomaAudio: String = "", titulo: String = "", kbps: Int = kbpsDeclarado): RecursosPlayer {
        // Em aparelho de pouca RAM (ActivityManager: isLowRamDevice,
        // memoryClass <= 128 MB ou totalMem <= 3 GiB; a TCL de 2,4 GB tem
        // memoryClass 192 e so cai pelo totalMem)
        // o teto do buffer cai para a metade: gpu-cache + buffer + pedacos
        // paralelos + preparo do proximo titulo estouraram uma TV de 2,4 GB
        // (PSS ~575 MB, lmkd matou o app). Aparelho com RAM sobrando nao muda.
        val heap = Runtime.getRuntime().maxMemory()
        val am = act.getSystemService(Context.ACTIVITY_SERVICE) as? android.app.ActivityManager
        val totalMem = am?.let { a -> android.app.ActivityManager.MemoryInfo().also { a.getMemoryInfo(it) }.totalMem } ?: 0L
        val poucaRam = (am != null && (am.isLowRamDevice || am.memoryClass <= 128 ||
                        (totalMem in 1L..(3L shl 30)))).also { poucaRamAparelho = it }
        val tetoBuffer = if (poucaRam) (heap / 8).coerceIn(24L shl 20, 48L shl 20).toInt()
                         else (heap / 4).coerceIn(32L shl 20, 96L shl 20).toInt()
        // preset = MiB from Settings (0 Auto). ExoPlayer holds it in the Java heap: never past heap/3.
        // Auto with a high declared bitrate: 48 MB is ~7 s of a 52.8 Mbps remux
        // (TCL rebuffered every 12-30 s). Aim at 30 s of it, never past heap/5
        // and 80 MB on low RAM (the 575 MB PSS kill), heap/3 and 192 MB otherwise.
        val porBitrate = kbps.toLong() * 1000 / 8 * 30
        val tetoAlto = if (poucaRam) minOf(heap / 5, 80L shl 20) else minOf(heap / 3, 192L shl 20)
        val autoBytes = porBitrate.coerceAtMost(tetoAlto).coerceAtLeast(tetoBuffer.toLong()).toInt()
        val (minMs, maxMs, bytes) = if (preset > 0)
            Triple(50_000, 150_000, minOf(preset.toLong() shl 20, heap / 3).toInt())
        else Triple(50_000, 50_000, autoBytes)
        val r = RecursosPlayer(CargaPlayer(minMs, maxMs, bytes, preparando), maxMs, if (preparando) idiomaAudio else "")
        try {
            val http = DefaultHttpDataSource.Factory().setAllowCrossProtocolRedirects(true)
                .setConnectTimeoutMs(15000).setReadTimeoutMs(20000)
            val props = HashMap<String, String>()
            var ua: String? = null
            for (linha in cabecalhos.split('\n')) {
                val i = linha.indexOf(':')
                if (i <= 0) continue
                val nome = linha.substring(0, i).trim()
                val valor = linha.substring(i + 1).trim()
                if (nome.isEmpty()) continue
                if (nome.equals("User-Agent", ignoreCase = true)) { http.setUserAgent(valor); ua = valor }
                else props[nome] = valor
            }
            http.setDefaultRequestProperties(props)
            val limited = !preparando && limitedNext
            // Check only once per player. HLS segments share the same connection gate.
            val checked = java.util.concurrent.atomic.AtomicBoolean()
            val serialHttp: DataSource.Factory = if (limited) LimitedDataSource.Factory(http, { r.carga.cancelada }) {
                if (checked.compareAndSet(false, true)) nativeLimitedStatus() else 0
            } else http
            val baixa = url.lowercase()
            val adaptativo = baixa.contains(".m3u8") || baixa.contains("m3u8?") || baixa.contains(".mpd")
            // Cache off or not applicable for a VOD: whatever was kept is stale.
            if (pedidoCache <= 0 || limited) { r.cacheEstado = CacheSessao.DESLIGADO; if (titulo.isNotEmpty()) CacheMidia.descartar("setting off") }
            else if (adaptativo || !ParaleloDataSource.serve(Uri.parse(url))) {
                r.cacheEstado = CacheSessao.NAO_SE_APLICA
                if (titulo.isNotEmpty()) CacheMidia.descartar("not progressive")
            } else {
                val aoFalhar: (Boolean) -> Unit = { semEspaco ->
                    principal.post {
                        r.cacheEstado = if (semEspaco) CacheSessao.DISCO_CHEIO else CacheSessao.FALHOU
                        if (r.adotado && recursosAtuais === r && cacheEstado == CacheSessao.ATIVO) {
                            cacheEstado = r.cacheEstado
                            Log.w(TAG, "seek cache off for this session (${if (semEspaco) "ENOSPC" else "write error"})")
                            relatarCache(sessao)
                        }
                    }
                }
                // WARM RESUME: the same title and source reuse the downloaded part.
                val guardado = if (titulo.isNotEmpty()) CacheMidia.pegar(titulo, url.substringBefore('#'), pedidoCache, aoFalhar) else null
                val (c, estado) = if (guardado != null) guardado to CacheSessao.ATIVO
                                  else CacheMidia.criar(act.cacheDir, pedidoCache, aoFalhar)
                r.cache = c; r.cacheEstado = estado
            }
            r.rede = { token ->
                val rede: DataSource.Factory = if (limited) serialHttp
                    else ParaleloDataSource.Factory(ua, props, http, medidor, token)
                r.cache?.fabrica(rede) ?: rede
            }
            val origemPreparo = r.cache?.fabrica(serialHttp) ?: serialHttp
            val token = if (preparando) 0L else medidor.sessao(geracao).also { medidorToken = it }
            r.origem = OrigemPlayer(if (preparando) origemPreparo else r.rede(token), r.carga)
            // ON: platform decoder/passthrough first, FFmpeg only as fallback.
            // F06 tap is at the sink input; F07 gain stays in its processors.
            r.renderizadores = object : androidx.media3.exoplayer.DefaultRenderersFactory(act) {
                override fun buildAudioSink(context: Context, enableFloatOutput: Boolean,
                                            enableAudioTrackPlaybackParams: Boolean): AudioSink =
                    // Sonic needs integer PCM; float output bypasses audio processors.
                    AudioSyncSink(DefaultAudioSink.Builder(context).setEnableFloatOutput(false)
                        .setEnableAudioTrackPlaybackParams(false)
                        .setAudioProcessors(arrayOf(r.proc)).build(), r.tap, { r.velocidadePcm })
            }.setExtensionRendererMode(androidx.media3.exoplayer.DefaultRenderersFactory.EXTENSION_RENDERER_MODE_ON)
                .setEnableDecoderFallback(true)
            r.fabricaMidia = DefaultMediaSourceFactory(act)
                // ParaleloDataSource faz as tres tentativas de transporte;
                // nao empilhar outra rodada invisivel do Loader antes da
                // maquina de reconexao 3/10/25 s do nucleo.
                .setLoadErrorHandlingPolicy(PoliticaErroParalelo())
                .setDataSourceFactory(if (limited) DefaultDataSource.Factory(act, r.origem) else LivePlaylistDataSource.Factory(
                    DefaultDataSource.Factory(act, r.origem),
                    DefaultHttpDataSource.Factory().setAllowCrossProtocolRedirects(true)
                        .setConnectTimeoutMs(2000).setReadTimeoutMs(2000)
                        .setUserAgent(ua).setDefaultRequestProperties(props.filterKeys {
                            !it.equals("If-None-Match", true) && !it.equals("If-Modified-Since", true)
                        }),
                    report = { Log.i(TAG, it) },
                    liveTsUri = Uri.parse(url).takeIf { LivePlaylistDataSource.isLiveTs(it) },
                ))
            if (construir) construirPlayer(act, r)
            Log.i(TAG, "[player] buffer preset=$preset min=${minMs / 1000}s max=${maxMs / 1000}s teto=${bytes shr 20}MB preparo=$preparando")
            return r
        } catch (e: Exception) {
            soltarRecursosPreparo(r)
            throw e
        }
    }

    private fun construirPlayer(act: Activity, r: RecursosPlayer) {
        val builder = ExoPlayer.Builder(act, r.renderizadores).setLoadControl(r.carga).setMediaSourceFactory(r.fabricaMidia)
            // Renderer teardown runs on Media3's playback thread. Bound UI waits.
            .setReleaseTimeoutMs(200)
            .setDetachSurfaceTimeoutMs(200)
        val seletor = if (r.idiomaAudio.isNotEmpty()) SeletorAudioPreparo(act, r.idiomaAudio)
                      else DefaultTrackSelector(act)
        seletor.parameters = seletor.parameters.buildUpon()
            .setTunnelingEnabled(nativeTunneling()).build()
        builder.setTrackSelector(seletor)
        r.fioPreparo?.let { builder.setPlaybackLooper(it.looper) }
        r.bandaPreparo?.let { builder.setBandwidthMeter(it) }
        r.p = builder.build()
        if (liberacoesEmCurso.get() > 0) {
            try { r.p.audioSessionId = androidx.media3.common.util.Util.generateAudioSessionIdV21(act) }
            catch (e: Exception) { Log.w(TAG, "sessao de audio nova: $e") }
        }
        // Sem foco de audio: o volume da soundbar/HDMI nao pode pausar (#581).
        r.p.setAudioAttributes(AudioAttributes.Builder().setUsage(C.USAGE_MEDIA)
            .setContentType(C.AUDIO_CONTENT_TYPE_MOVIE).build(), false)
        r.p.trackSelectionParameters = r.p.trackSelectionParameters.buildUpon()
            .setTrackTypeDisabled(C.TRACK_TYPE_TEXT, true).build()
    }

    private fun abrirMain(url: String, cabecalhos: String, reabrindo: Boolean,
                          inicioMs: Int, geracao: Int, pedido: Int, fracao: Int = 0, esperaInicio: Long = -1L,
                          preparada: Preparo? = null) {
        if (pedido != pedidos.get()) {
            preparada?.recursos?.let { soltarRecursosPreparo(it) }
            return
        }
        val act = activity
        if (act == null) {
            preparada?.recursos?.let { soltarRecursosPreparo(it) }
            confirmarRetomada(geracao, false)
            return
        }
        val espera = if (esperaInicio < 0) SystemClock.elapsedRealtime() else esperaInicio
        if (esperaInicio < 0) {
            liberar()
            // O C envia ganho/pausa depois de abrir; a espera nao pode apaga-los.
            ganhoPct = 100
            if (!reabrindo) pausaPedida = false
        }
        // MStar/Amlogic: o overlay e o decoder antigos precisam sair antes do novo.
        // Poll no Handler deixa Back/parar funcionar enquanto o release corre em fundo.
        if (limitedNext || limitedSession || preparada?.proximo == true || (semRecriar && preparada == null)) {
            val ms = SystemClock.elapsedRealtime() - espera
            if (liberacoesEmCurso.get() > 0 && (limitedNext || limitedSession || ms < 3000)) {
                principal.postDelayed({
                    abrirMain(url, cabecalhos, reabrindo, inicioMs, geracao, pedido, fracao, espera, preparada)
                }, 25)
                return
            }
            Log.i(TAG, "[player] esperou o release anterior $ms ms (pendentes=${liberacoesEmCurso.get()})")
        }
        limitedSession = limitedNext
        novaSuperficie(act)
        mostrarQuadro(url, tituloPedido)
        pedidoAtivo = pedido
        hdrRecriadoPara = ""; quadroVisto = false
        recriando = false; recriaAgendada = false; recuperouSuperficie = false; estavelEmitido = false
        principal.removeCallbacks(recriar)
        principal.removeCallbacks(estavelRun)
        val minha = sessao
        urlAtual = url
        cabAtual = cabecalhos
        inicioAtualMs = inicioMs
        geracaoNative = geracao
        fracaoAtual = if (inicioMs > 0) 0 else fracao
        fracaoPendente = 0
        abriuEm = SystemClock.elapsedRealtime()
        if (!reabrindo) retentou = false
        try {
            ParaleloDataSource.Estat.zerar()
            quadrosPerdidos = 0L; statsTiques = 0
            val titulo = tituloPedido
            tituloAberto = ""
            val r = preparada?.recursos ?: criarPlayer(act, url, cabecalhos, bufferPreset, cacheMbPedido, false, geracao, titulo = titulo)
            if (preparada != null) {
                medidorToken = medidor.sessao(geracao)
                r.promover(medidorToken)
                if (!r.construido()) construirPlayer(act, r)
            }
            val p = r.p
            recursosAtuais = r
            player = p
            processador = r.proc
            r.tap.ligado = tap.ligado
            tap = r.tap
            cacheMidia = r.cache; cacheEstado = r.cacheEstado
            if (preparada == null && r.cache != null) { tituloAberto = titulo; chaveAberta = url.substringBefore('#') }
            bufferAlvoMs = r.alvoMs; bufferMinMs = r.carga.minMs; bufferBytes = r.carga.bytes
            if (preparada != null) {
                decoderDv = preparada.decoderDv
                p.setAudioAttributes(AudioAttributes.Builder().setUsage(C.USAGE_MEDIA)
                    .setContentType(C.AUDIO_CONTENT_TYPE_MOVIE).build(), false)
                try { nativeAudioEstado(r.audioFormato) } catch (_: UnsatisfiedLinkError) {}
            }
            aplicarGanho()
            relatarCache(minha)
            val ouv = ouvinte(minha); val ana = analitico(minha)
            ouvinteAtual = ouv; analiticoAtual = ana
            p.addListener(ouv)
            p.addAnalyticsListener(ana)

            val sv = superficie
            if (sv != null) {
                sv.visibility = View.VISIBLE
                p.setVideoSurfaceView(sv)
                sv.holder.addCallback(depoisDoMedia3)
            }
            temJanela = false
            aplicarEncaixe()

            if (preparada == null) {
                val mediaItem = itemMidia(url)
                var inicioAceito = false
                if (inicioMs > 0) {
                    try {
                        // Set position before prepare; no redundant initial Range.
                        p.setMediaItem(mediaItem, inicioMs.toLong())
                        inicioAceito = true
                    } catch (e: Exception) {
                        Log.w(TAG, "posicao inicial recusada; usando retomada normal: $e")
                        p.setMediaItem(mediaItem)
                    }
                } else p.setMediaItem(mediaItem)
                if (inicioMs <= 0 && fracaoAtual > 0) fracaoPendente = fracaoAtual
                else confirmarRetomada(geracao, inicioAceito)
                p.playWhenReady = !pausaPedida
                // false -> false nao gera callback quando a pausa chegou sem player.
                if (pausaPedida) ev(EV_PAUSADO)
                p.prepare()
            } else if (preparada.proximo) {
                // The next-episode source owns the warmed sample queues. Its
                // new player prepares that existing source at the exact same
                // period position, after the previous decoder has been released.
                fracaoPendente = preparada.fracaoPendente
                p.setMediaSource(r.fontePrecarregada!!, preparada.posicaoFonteMs)
                if (fracaoPendente <= 0) confirmarRetomada(geracao, inicioMs > 0 || preparada.inicioAceito)
                p.playWhenReady = true
                p.prepare()
                Log.i(TAG, "[preparo] proximo adotado token=${preparada.token} pos=${preparada.posicaoFonteMs}")
            } else {
                // Transfer timeline/resume state before publishing READY. No new
                // media item, prepare or initial request on this adoption path.
                fracaoPendente = preparada.fracaoPendente
                if (fracaoPendente > 0) aplicarFracao(p)
                else confirmarRetomada(geracao, preparada.inicioAceito)
                ouv.onTracksChanged(p.currentTracks)
                ouv.onVideoSizeChanged(p.videoSize)
                ouv.onPlaybackStateChanged(p.playbackState)
                p.playWhenReady = true
                Log.i(TAG, "[preparo] adotado token=${preparada.token} estado=${p.playbackState}")
            }
            principal.postDelayed(tique(minha), TIQUE_MS)
        } catch (e: Exception) {
            preparada?.recursos?.let { if (recursosAtuais !== it) soltarRecursosPreparo(it) }
            confirmarRetomada(geracao, false)
            Log.w(TAG, "abrir: $e")
            ev(EV_ERRO, -1, 0)
        }
    }

    // Sobe a sessao e solta tudo; qualquer callback pendente do player velho
    // morre na conferencia do numero.
    private fun liberar() {
        recuperacaoLive?.cancel()
        recuperacaoLive = null
        sessao++
        medidor.encerrar()
        val recursos = recursosAtuais
        recursos?.adotado = false
        recursos?.carga?.cancelada = true
        recursos?.liberado = true
        recursosAtuais = null
        val p = player
        player = null
        pronto = false
        duracaoEnviada = -1
        videoW = 0; videoH = 0
        temJanela = false
        assinatura = ""
        audios = ArrayList(); legendas = ArrayList()
        decoderDv = false
        ultHdr = ""; ultDv = -1; ultAtmos = -1
        avisouSemAudio = false
        val sv = superficie
        val ouv = ouvinteAtual; val ana = analiticoAtual
        ouvinteAtual = null; analiticoAtual = null
        soltando = null
        if (p != null) {
            // Remove listeners before posting stop/release to the application looper.
            try { if (ouv != null) p.removeListener(ouv) } catch (e: Exception) { }
            try { if (ana != null) p.removeAnalyticsListener(ana) } catch (e: Exception) { }
            // Esconde sem destruir a superficie (destruir acorda o callback do Media3).
            try { sv?.layoutParams = FrameLayout.LayoutParams(1, 1) } catch (e: Exception) { }
            // A superficie velha fica com o player velho; a proxima abertura cria outra.
            superficie = null
            liberarEmFundo(p, sv) { recursos?.let { liberarFontePreparo(it) } }
        }
        // F07: the session cache goes with its player (released and deleted
        // off the main thread, after the canceled loaders unwind).
        // WARM RESUME: a healthy cache of a known title is kept for the next open.
        cacheMidia?.let { if (mornoLigado && tituloAberto.isNotEmpty()) CacheMidia.guardar(it, tituloAberto, chaveAberta) else it.liberar() }
        cacheMidia = null
        tituloAberto = ""
        cacheEstado = CacheSessao.DESLIGADO
        cacheTiques = 0
        processador = null
        // Depois do release (o fio de reproducao ja parou): sem audio decodificado.
        tap.encerrar()
        try { nativeAudioEstado(AudioSyncTap.FMT_NENHUM) } catch (_: UnsatisfiedLinkError) {}
        superficie?.visibility = View.GONE
    }

    private fun novaSuperficie(act: Activity) {
        if (superficie != null) return
        val c = camada ?: return
        val sv = SurfaceView(act)
        sv.visibility = View.GONE
        // A superficie de video nunca pega o foco do controle: as teclas chegam
        // ao app pelo foco da SDLSurface (SDLSurface.onKey), e foco em outra
        // View deixa o processo vivo e a tela sem resposta (#318).
        sv.isFocusable = false
        sv.isFocusableInTouchMode = false
        sv.holder.addCallback(antesDoMedia3)
        observarJanela(sv)
        c.addView(sv, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT))
        superficie = sv
    }

    // QUANTO O MEDIA3 SEGUROU O FIO PRINCIPAL ao perder a superficie (recriar,
    // ou o sistema ao esconder o app: tecla de ajustes da TV, Home). O callback
    // dele entra na lista entre estes dois (setVideoSurfaceView, em abrirMain).
    private var destruirIni = 0L
    private val antesDoMedia3 = object : SurfaceHolder.Callback {
        override fun surfaceCreated(h: SurfaceHolder) {
            superficie?.let { if (h === it.holder) recuperarJanela(it) }
            Log.i(TAG, "[dvtrace] created sessao=$sessao holder=${System.identityHashCode(h)} atual=${h === superficie?.holder} hdr=$ultHdr frame=${h.surfaceFrame}")
        }
        override fun surfaceChanged(h: SurfaceHolder, f: Int, w: Int, a: Int) {
            superficie?.let { if (h === it.holder) recuperarJanela(it) }
            Log.i(TAG, "[dvtrace] changed sessao=$sessao holder=${System.identityHashCode(h)} atual=${h === superficie?.holder} tamanho=${w}x$a formato=$f hdr=$ultHdr")
        }
        override fun surfaceDestroyed(h: SurfaceHolder) {
            superficie?.let { if (h === it.holder) recuperarJanela(it) }
            destruirIni = SystemClock.elapsedRealtime()
            Log.i(TAG, "[dvtrace] destroyed sessao=$sessao holder=${System.identityHashCode(h)} atual=${h === superficie?.holder} hdr=$ultHdr")
        }
    }
    private val depoisDoMedia3 = object : SurfaceHolder.Callback {
        override fun surfaceCreated(h: SurfaceHolder) {}
        override fun surfaceChanged(h: SurfaceHolder, f: Int, w: Int, a: Int) {}
        override fun surfaceDestroyed(h: SurfaceHolder) {
            Log.i(TAG, "[player] superficie destruida: Media3 segurou o fio principal por ${SystemClock.elapsedRealtime() - destruirIni} ms")
        }
    }

    // Media3 requires both calls on its application looper. Heavy renderer
    // teardown stays on its playback thread; release waits at most 200 ms.
    private fun liberarEmFundo(p: ExoPlayer, sv: SurfaceView?, depois: () -> Unit = {}) {
        liberacoesEmCurso.incrementAndGet()
        val corpo = Runnable {
            val ini = SystemClock.elapsedRealtime()
            try { p.stop() } catch (e: Exception) { Log.w(TAG, "stop: $e") }
            try { p.release() } catch (e: Exception) { Log.w(TAG, "release: $e") }
            try { depois() } catch (e: Exception) { Log.w(TAG, "release preload source: $e") }
            val ms = SystemClock.elapsedRealtime() - ini
            if (ms > 5000) Log.w(TAG, "[player] release anterior levou $ms ms (>5 s: HAL de audio preso?)")
            else Log.i(TAG, "[player] release anterior levou $ms ms")
            liberacoesEmCurso.decrementAndGet()
            principal.post {
                if (sv != null && sv !== superficie) (sv.parent as? FrameLayout)?.removeView(sv)
            }
        }
        Handler(p.applicationLooper).post(corpo)
    }

    // Tique de 250 ms: a posicao que o C le sem esperar ninguem.
    private fun tique(minha: Int): Runnable = object : Runnable {
        override fun run() {
            if (!atual(minha)) return
            val p = player ?: return
            // Keep the signed live-window clock for stall detection. Only the
            // native UI position is clamped; a moving negative position is valid.
            try { nativePos(p.currentPosition, p.bufferedPosition, p.isCurrentMediaItemLive,
                recursosAtuais?.carga?.allocator?.totalBytesAllocated ?: -1, geracaoNative)
            } catch (e: UnsatisfiedLinkError) { }
            if (++statsTiques >= 4) { statsTiques = 0; relatarStats(p) }
            if (cacheMidia != null && cacheEstado == CacheSessao.ATIVO && ++cacheTiques >= CACHE_RELATO_TIQUES) {
                cacheTiques = 0
                relatarCache(minha)
            }
            principal.postDelayed(this, TIQUE_MS)
        }
    }

    // Uma vez por segundo, do fio principal: leituras baratas, nada alocado
    // alem da string do host (que o JNI copia).
    private fun relatarStats(p: ExoPlayer) {
        val e = ParaleloDataSource.Estat
        val buf = (p.bufferedPosition - p.currentPosition).coerceIn(0L, Int.MAX_VALUE.toLong()).toInt()
        var quadros = -1L
        var perdidos = -1L
        p.videoDecoderCounters?.let { c ->
            c.ensureUpdated()
            quadros = (c.renderedOutputBufferCount + c.droppedBufferCount).toLong()
            perdidos = quadrosPerdidos
        }
        try {
            nativeStats(if (e.usado) e.bytes.get() else -1L, buf,
                bufferAlvoMs,
                if (e.usado) e.conexoes.get().coerceAtLeast(0) else -1,
                e.primeiroByteMs, perdidos, quadros, e.host,
                bufferMinMs, bufferBytes shr 20, if (p.isLoading) 1 else 0)
        } catch (t: UnsatisfiedLinkError) { }
    }

    // --- janela ---------------------------------------------------------------

    // Retangulo em coordenadas de layout 1920x1080; aceita origem NEGATIVA e
    // tamanho maior que a tela (zoom): a camada recorta o excedente.
    // `encaixa`: janela LISA do nucleo — o quadro encaixa no retangulo com
    // tarja, como o plano da LG e o LetterBox da Samsung. Sem isso o trailer
    // em "Original" (Apple 2,4:1) saia esticado em 16:9. Recorte (encaixa =
    // false) ja vem com a proporcao certa e e aplicado exato.
    private var pedX = 0; private var pedY = 0; private var pedW = 0; private var pedH = 0
    private var pedEncaixa = true

    private fun definirJanela(x: Int, y: Int, w: Int, h: Int, encaixa: Boolean) {
        if (player == null) return   // como o Video.cs: o C repete depois do videoInfo
        pedX = x; pedY = y; pedW = w; pedH = h; pedEncaixa = encaixa
        temJanela = true
        calcularJanela()
        Log.i(TAG, "[dvtrace] janela sessao=$sessao holder=${System.identityHashCode(superficie?.holder)} pedido=$x,$y ${w}x$h encaixa=$encaixa calculada=$jx,$jy ${jw}x$jh hdr=$ultHdr semRecriar=$semRecriar")
    }

    private fun calcularJanela() {
        if (pedEncaixa && videoW > 0 && videoH > 0 && pedW > 0 && pedH > 0) {
            val esc = minOf(pedW.toFloat() / videoW, pedH.toFloat() / videoH)
            jw = (videoW * esc + 0.5f).toInt()
            jh = (videoH * esc + 0.5f).toInt()
            jx = pedX + (pedW - jw) / 2
            jy = pedY + (pedH - jh) / 2
        } else {
            jx = pedX; jy = pedY; jw = pedW; jh = pedH
        }
        aplicarJanela()
    }

    private fun larguraLogica(): Float {
        val c = camada
        return if (interfaceMobile && c != null && c.width > 0 && c.height > 0)
            TELA_H.toFloat() * c.width / minOf(c.width, c.height) else TELA_W.toFloat()
    }

    private fun alturaLogica(): Float {
        val c = camada
        return if (interfaceMobile && c != null && c.width > 0 && c.height > 0)
            TELA_H.toFloat() * c.height / minOf(c.width, c.height) else TELA_H.toFloat()
    }

    private fun reaplicarJanela() {
        if (!temJanela) aplicarEncaixe() else aplicarJanela()
    }

    // Sem janela pedida ainda: o quadro inteiro encaixado (letterbox) na tela.
    private fun aplicarEncaixe() {
        if (temJanela) return
        val largura = larguraLogica()
        val altura = alturaLogica()
        if (videoW > 0 && videoH > 0) {
            val esc = minOf(largura / videoW, altura / videoH)
            jw = (videoW * esc + 0.5f).toInt()
            jh = (videoH * esc + 0.5f).toInt()
            jx = ((largura - jw) / 2).toInt()
            jy = ((altura - jh) / 2).toInt()
        } else {
            jx = 0; jy = 0; jw = Math.round(largura); jh = Math.round(altura)
        }
        aplicarJanela()
    }

    private fun aplicarJanela() {
        val c = camada ?: return
        val sv = superficie ?: return
        var cw = c.width
        var ch = c.height
        if (cw < 1 || ch < 1) {
            val m = c.resources.displayMetrics
            cw = m.widthPixels; ch = m.heightPixels
        }
        val ex = cw.toFloat() / larguraLogica()
        val ey = ch.toFloat() / alturaLogica()
        // As BORDAS arredondam e o tamanho sai da diferenca: tela cheia cai
        // exatamente em 0,0,cw,ch, sem fresta.
        val x0 = Math.round(jx * ex)
        val y0 = Math.round(jy * ey)
        val x1 = Math.round((jx + jw) * ex)
        val y1 = Math.round((jy + jh) * ey)
        val largura = maxOf(1, x1 - x0)
        val altura = maxOf(1, y1 - y0)
        val gravidade = Gravity.TOP or Gravity.START
        val antes = sv.layoutParams as? FrameLayout.LayoutParams
        // Inspect the current SurfaceView, not a cached rectangle: resize or
        // replacing the surface must still apply its actual layout. Avoid both
        // allocation and requestLayout when repeated requests change nothing.
        if (antes != null && antes.width == largura && antes.height == altura &&
            antes.leftMargin == x0 && antes.topMargin == y0 &&
            antes.rightMargin == 0 && antes.bottomMargin == 0 && antes.gravity == gravidade) return
        val lp = FrameLayout.LayoutParams(largura, altura)
        lp.gravity = gravidade
        lp.leftMargin = x0
        lp.topMargin = y0
        sv.layoutParams = lp
        // A TCL PRENDE A GEOMETRIA DO PLANO DE VIDEO: aplica o primeiro tamanho
        // e posicao da superficie e ignora as mudancas seguintes (o dono: "entra
        // recortado e nao sai; entra no esticar e fica esticado"). Recriar a
        // Surface (GONE -> VISIBLE) faz o compositor montar camada nova com a
        // geometria nova; o ExoPlayer troca a saida do decoder sem recarregar.
        //
        // SO EM SDR ATE A SUPERFICIE ESTAR ESTAVEL. No HDR/DV cada recriacao
        // aqui obrigava o Media3 a soltar a saida do decoder e o C2MtkVdecTv a
        // religar o DispLink: o aspecto salvo aplicado apos o primeiro quadro,
        // e cada troca manual de modo, mudava o tamanho e recriava (~12 vezes
        // em 30 s; TCL 2160p DV, 11/10), com o fio principal parado no glClear
        // ("renderer soltou em 7057 ms", pior quadro 6612.7 ms). Antes de
        // estavel a recriacao fica so nas TRANSICOES DE MODO (hdrNaSuperficie);
        // DEPOIS dela, uma recriacao por mudanca com a espera longa, senao o
        // plano da TCL fica preso na geometria de abertura e a troca Crop ->
        // Ajuste nao aparece (revisao 14/10). DV nativo dispensa a recriacao
        // da ABERTURA (ver recriarSuperficie); depois de estavel, a troca de
        // modo e um pedido do dono e o veto nao se aplica — o plano da TCL so
        // acompanha a geometria nova com a camada nascendo de novo.
        if (ultHdr.isEmpty() || ultHdr == "none") recriarSuperficie(RECRIA_ESPERA_MS)
        else if (estavelEmitido) recriarSuperficie(RECRIA_ESPERA_HDR_MS)
    }

    // Recriar a Surface custa um quadro preto e, em rajada (aspecto apertado
    // varias vezes), travava: as trocas JUNTAM numa so, RECRIA_ESPERA_MS depois
    // da ultima. A mesma recriacao liga o HDR da TCL (ver onRenderedFirstFrame).
    private const val RECRIA_ESPERA_MS = 350L
    // HDR/DV: recriar e caro (DispLink religa), so depois de a superficie parar.
    private const val RECRIA_ESPERA_HDR_MS = 600L
    private var recriando = false
    private var recriaAgendada = false
    private var recuperouSuperficie = false
    private var superficiePos = 0L
    private var quadroVisto = false
    // SOLTAR A SAIDA SEM ESPERAR (2.0.3). Destruir a Surface com o decoder
    // ainda ligado nela faz o Media3 esperar o fio de reproducao DENTRO do
    // surfaceDestroyed, no fio principal (setVideoOutputInternal: ate 2 s, e
    // no estouro derruba o player com timeout). Na TCL do dono o `recriar`
    // segurou o fio principal por 7470 ms (logcat 08/10 00:49:48, "Slow
    // dispatch"): controle morto, e tecla nesse meio = ANR. O pedido vai como
    // mensagem ao fio de reproducao e `depois` so roda, de volta no fio
    // principal, com o renderer ja sem a superficie: a espera do Media3 que
    // vem em seguida encontra o trabalho feito. Player liberado no meio = a
    // mensagem e descartada e `depois` nao roda (a superficie foi com ele).
    private var soltando: ExoPlayer? = null
    private fun soltarSaida(p: ExoPlayer, depois: Runnable) {
        val ini = SystemClock.elapsedRealtime()
        try {
            for (i in 0 until p.rendererCount) {
                if (p.getRendererType(i) != C.TRACK_TYPE_VIDEO) continue
                p.createMessage(p.getRenderer(i)).setType(Renderer.MSG_SET_VIDEO_OUTPUT).setPayload(null).send()
            }
            p.createMessage { _, _ ->
                Log.i(TAG, "[player] superficie: renderer soltou em ${SystemClock.elapsedRealtime() - ini} ms")
                principal.post(depois)
            }.send()
        } catch (e: Exception) {
            Log.w(TAG, "soltarSaida: $e")
            soltando = null
            recriando = false
            avaliarEstavel()
        }
    }
    private val recriar = Runnable {
        recriaAgendada = false
        // O pedido nasce antes do decoder existir (abertura da fonte): se ele
        // virou DV nativo enquanto a recriacao esperava, desistir agora e mais
        // barato que desligar a saida do decoder e religar o DispLink da TCL
        // (15 s no log do dono). O evento de superficie estavel segue normal.
        // Depois de estavel o pedido e do dono trocando o aspecto — recria
        // mesmo em DV, que e o unico gesto que muda a geometria do plano.
        if (decoderDv && !estavelEmitido) { avaliarEstavel(); return@Runnable }
        val p = player
        val sv = superficie
        Log.i(TAG, "[dvtrace] recriar player=${System.identityHashCode(p)} holder=${System.identityHashCode(sv?.holder)} soltando=${soltando === p} vis=${sv?.visibility} valida=${sv?.holder?.surface?.isValid}")
        if (p != null && sv != null && soltando !== p && sv.visibility == View.VISIBLE && sv.holder.surface?.isValid == true) {
            soltando = p
            recriando = true
            recuperouSuperficie = false
            superficiePos = p.currentPosition
            soltarSaida(p) {
                if (player !== p || superficie !== sv) return@soltarSaida
                val ini = SystemClock.elapsedRealtime()
                sv.visibility = View.GONE
                Log.i(TAG, "[player] superficie: destruir levou ${SystemClock.elapsedRealtime() - ini} ms no fio principal")
                principal.post {
                    if (player !== p || superficie !== sv) return@post
                    sv.visibility = View.VISIBLE
                    // Clear the association after attach events queued during destruction.
                    principal.post {
                        if (player === p && superficie === sv) {
                            soltando = null
                            recriando = false
                            avaliarEstavel()
                        }
                    }
                    Log.i(TAG, "[dvtrace] visible player=${System.identityHashCode(p)} holder=${System.identityHashCode(sv.holder)} atual=${player === p && superficie === sv}")
                    (activity as? NuvioActivity)?.devolverFoco()
                }
            }
        }
    }
    // Familias que nao recriam a superficie no HDR (RecriaHdr.kt): MStar e
    // Amlogic. A recriacao existe por causa da TCL; nelas so custa.
    private val semRecriar: Boolean by lazy {
        try {
            val nomes = android.media.MediaCodecList(android.media.MediaCodecList.REGULAR_CODECS).codecInfos
                .filter { !it.isEncoder }.map { it.name }
            val f = RecriaHdr.dispensa(nomes)
            Log.i(TAG, "[player] recriar superficie no HDR: ${f?.let { "nao ($it)" } ?: "sim"}")
            f != null
        } catch (e: Throwable) { false }
    }
    private fun recriarSuperficie(atrasoMs: Long) {
        Log.i(TAG, "[dvtrace] pedido sessao=$sessao holder=${System.identityHashCode(superficie?.holder)} atrasoMs=$atrasoMs hdr=$ultHdr quadro=$quadroVisto semRecriar=$semRecriar soltando=${soltando != null}")
        // DV nativo so dispensa a recriacao da ABERTURA (a que nasce antes de
        // a superficie estabilizar): depois de estavel, a troca de aspecto e o
        // gesto que o dono fazia na mao para o Dolby ligar — vetar aqui deixava
        // o plano preso na geometria de abertura e nenhum modo mudava (TCL,
        // 14/10). O veto do HDR sozinho ja cobre a transicao (hdrNaSuperficie).
        if (semRecriar || (decoderDv && !estavelEmitido)) return
        principal.removeCallbacks(estavelRun)
        principal.removeCallbacks(recriar)
        recriaAgendada = true
        principal.postDelayed(recriar, atrasoMs)
    }
    // A TCL so liga o modo HDR do painel quando a Surface nasce com o decoder
    // ja em HDR: na abertura ela nasceu antes (SDR) e o HDR so aparecia depois
    // de trocar o aspecto (dono, 30/09). Uma recriacao no primeiro quadro HDR.
    // Native DV already configures the panel. Destruction forces TCL DispLink
    // to reconnect (15 s in the captured log). Other HDR gets one recreation.
    private var hdrRecriadoPara = ""
    private var estavelEmitido = false
    private val estavelRun = Runnable {
        if (player != null && quadroVisto && !estavelEmitido) {
            estavelEmitido = true
            Log.i(TAG, "[aspect] superficie estavel (hdr=$ultHdr) sessao=$sessao soltando=${soltando != null} holder=${System.identityHashCode(superficie?.holder)}")
            ev(EV_SUPERFICIE_ESTAVEL)
        }
    }
    private fun avaliarEstavel() {
        if (!quadroVisto || estavelEmitido) return
        principal.removeCallbacks(estavelRun)
        if (recriando || recriaAgendada) return
        principal.postDelayed(estavelRun, 150L)
    }
    // Evidencia para "a TV nao acompanha o fps do filme": fps do video, modos que o
    // painel oferece e o ativo. TCL do dono (06/10): so existe 3840x2160@60, entao
    // nao ha modo para trocar; o Media3 ja vota Surface.setFrameRate sozinho.
    private fun logTaxaDeQuadros() {
        try {
            val fps = player?.videoFormat?.frameRate ?: -1f
            val d = activity?.windowManager?.defaultDisplay ?: return
            val modos = d.supportedModes.joinToString(",") { "${it.modeId}:${it.physicalWidth}x${it.physicalHeight}@${it.refreshRate}" }
            Log.i(TAG, "[quadro] video=${fps}fps tela=${d.mode.modeId}@${d.refreshRate} modos=[$modos]")
            val a = player?.audioFormat
            Log.i(TAG, "[audio] ${a?.sampleMimeType} ${a?.channelCount}ch ${a?.sampleRate}Hz atraso=${atrasoMs}ms")
        } catch (e: Exception) { Log.w(TAG, "logTaxaDeQuadros: $e") }
    }
    private fun hdrNaSuperficie() {
        // Native DV handles the panel transition without destroying its output.
        if (semRecriar || decoderDv) return
        if (!quadroVisto || ultHdr.isEmpty() || ultHdr == "none" || ultHdr == hdrRecriadoPara) return
        hdrRecriadoPara = ultHdr
        Log.i(TAG, "HDR ($ultHdr): recria a superficie para a TV ligar o modo HDR")
        recriarSuperficie(200)
    }

    // --- escolha de faixa ----------------------------------------------------

    private fun escolherMain(tipo: Int, idx: Int) {
        if (tipo == 2) { atrasoMs = idx; return }
        if (tipo == 3) { tap.ligado = idx != 0; return }   // F06: escuta de PCM liga/desliga
        val p = player ?: return
        try {
            val par = p.trackSelectionParameters.buildUpon()
            if (tipo == 0) {
                val (g, i) = audios.getOrNull(idx) ?: return
                relatarAudio(sessao, "escolha indice=$idx grupoIndice=$i mime=${g.getTrackFormat(i).sampleMimeType}")
                par.setOverrideForType(TrackSelectionOverride(g.mediaTrackGroup, i))
            } else if (tipo == 1) {
                if (idx < 0) {
                    par.setTrackTypeDisabled(C.TRACK_TYPE_TEXT, true).clearOverridesOfType(C.TRACK_TYPE_TEXT)
                } else {
                    val (g, i) = legendas.getOrNull(idx) ?: return
                    par.setTrackTypeDisabled(C.TRACK_TYPE_TEXT, false)
                        .setOverrideForType(TrackSelectionOverride(g.mediaTrackGroup, i))
                }
            } else return
            p.trackSelectionParameters = par.build()
        } catch (e: Exception) { Log.w(TAG, "escolher $tipo/$idx: $e") }
    }

    // --- eventos do player ----------------------------------------------------

    private fun ouvinte(minha: Int) = object : Player.Listener {
        private var liveSegmentMs = -1
        private val live = LiveWindowRecovery(
            schedule = { tarefa, atraso -> principal.postDelayed(tarefa, atraso) },
            unschedule = { tarefa -> principal.removeCallbacks(tarefa) },
            active = { atual(minha) },
            status = { prazoMs ->
                if (atual(minha)) ev(EV_LIVE_RECOVERY, prazoMs.toInt(), geracaoNative)
            },
        )
        init { recuperacaoLive = live }
        override fun onPlaybackStateChanged(state: Int) {
            if (!atual(minha)) return
            val p = player ?: return
            Log.i(TAG, "[player] onPlaybackStateChanged state=$state renderer=audio(${p.audioFormat?.sampleMimeType}),video(${p.videoFormat?.sampleMimeType}) loadMs=${SystemClock.elapsedRealtime() - abriuEm}")
            when (state) {
                Player.STATE_BUFFERING -> ev(EV_BUFFER, 0)
                Player.STATE_READY -> {
                    if (live.ready()) Log.i(TAG, "[live] janela recuperada na mesma sessao=$minha")
                    ev(EV_BUFFER, 100)
                    Log.i(TAG, "[speed] ready parameters=${p.playbackParameters.speed} pcm=${recursosAtuais?.velocidadePcm}; awaiting native clock measurement")
                    relatarAudio(minha, "pronto volume=${p.volume} playWhenReady=${p.playWhenReady} suppression=${p.playbackSuppressionReason}")
                    if (!pronto) {
                        // Antes do EV_PRONTO: o C le o ack ao ver o "pronto".
                        if (fracaoPendente > 0) {
                            fracaoPendente = 0
                            Log.w(TAG, "[player] retomada: sem duracao antes do pronto; o C busca depois")
                            confirmarRetomada(geracaoNative, false)
                        }
                        pronto = true
                        duracaoEnviada = duracaoMs(p)
                        ev(EV_PRONTO, duracaoEnviada)
                        if (p.isPlaying) ev(EV_TOCANDO)
                    }
                }
                Player.STATE_ENDED -> ev(EV_FIM)
                else -> {}
            }
            medirEstado(p)
        }

        // Pausa pedida com o player em buffer nao passa por onIsPlayingChanged
        // (ele ja estava parado): a confirmacao do C sai daqui.
        override fun onPlayWhenReadyChanged(playWhenReady: Boolean, reason: Int) {
            if (!atual(minha)) return
            val p = player ?: return
            relatarAudio(minha, "playWhenReady=$playWhenReady reason=$reason")
            if (!playWhenReady && !p.isPlaying && p.playbackState != Player.STATE_ENDED) ev(EV_PAUSADO)
            medirEstado(p)
        }

        override fun onVolumeChanged(volume: Float) {
            relatarAudio(minha, "volume=$volume")
        }

        override fun onPlaybackSuppressionReasonChanged(playbackSuppressionReason: Int) {
            relatarAudio(minha, "suppression=$playbackSuppressionReason")
        }

        override fun onIsPlayingChanged(isPlaying: Boolean) {
            if (!atual(minha)) return
            val p = player ?: return
            if (isPlaying) { if (pronto) ev(EV_TOCANDO) }
            // Parar de tocar por buffer nao e pausa (o evento 7 ja disse).
            else if (!p.playWhenReady && p.playbackState != Player.STATE_ENDED) ev(EV_PAUSADO)
        }

        override fun onTimelineChanged(timeline: Timeline, reason: Int) {
            if (!atual(minha)) return
            val p = player ?: return
            val manifest = p.currentManifest as? HlsManifest
            val segmentMs = if (p.isCurrentMediaItemLive && manifest != null)
                (manifest.mediaPlaylist.targetDurationUs / 1000L).coerceIn(0L, 60_000L).toInt() else 0
            if (segmentMs != liveSegmentMs) {
                liveSegmentMs = segmentMs
                ev(EV_LIVE_SEGMENT, segmentMs, geracaoNative)
            }
            if (!pronto) { aplicarFracao(p); return }
            val d = duracaoMs(p)
            if (d != duracaoEnviada) { duracaoEnviada = d; ev(EV_PRONTO, d) }   // so a duracao muda
        }

        override fun onPositionDiscontinuity(oldPosition: Player.PositionInfo,
                                             newPosition: Player.PositionInfo, reason: Int) {
            if (!atual(minha)) return
            val p = player ?: return
            if (!p.isCurrentMediaItemLive) return
            // Numeric timing only: never log media IDs, manifest URLs or labels.
            val linha = "[live] discontinuity sessao=$minha reason=$reason oldMs=${oldPosition.positionMs} newMs=${newPosition.positionMs} offsetMs=${p.currentLiveOffset} state=${p.playbackState} bufferedMs=${p.totalBufferedDuration}"
            Log.i(TAG, linha)
            try { nativeAudioLog(linha) } catch (_: UnsatisfiedLinkError) { }
        }

        override fun onRenderedFirstFrame() {
            if (!atual(minha)) return
            Log.i(TAG, "[player] onRenderedFirstFrame renderer=video loadMs=${SystemClock.elapsedRealtime() - abriuEm} sessao=$sessao hdr=$ultHdr holder=${System.identityHashCode(superficie?.holder)} soltando=${soltando != null}")
            esconderQuadro()
            ev(EV_PRIMEIRO_QUADRO)
            quadroVisto = true
            logTaxaDeQuadros()
            hdrNaSuperficie()
            avaliarEstavel()
        }

        override fun onVideoSizeChanged(v: VideoSize) {
            if (!atual(minha)) return
            // Pixel anamorfico entra na largura: e a proporcao que o zoom do C usa.
            videoW = (v.width * v.pixelWidthHeightRatio + 0.5f).toInt()
            videoH = v.height
            ev(EV_TAMANHO, videoW, videoH)
            if (temJanela) calcularJanela() else aplicarEncaixe()
        }

        override fun onTracksChanged(tracks: Tracks) {
            if (!atual(minha)) return
            publicarFaixas(tracks)
            for (g in tracks.groups) if (g.type == C.TRACK_TYPE_AUDIO) {
                for (i in 0 until g.length) if (g.isTrackSelected(i)) {
                    val f = g.getTrackFormat(i)
                    relatarAudio(minha, "selecionada grupoIndice=$i idioma=${f.language} mime=${f.sampleMimeType} canais=${f.channelCount} hz=${f.sampleRate}")
                }
            }
            publicarHdr(tracks)
        }

        override fun onCues(cueGroup: CueGroup) {
            if (!atual(minha)) return
            val texto = cueGroup.cues.mapNotNull { it.text?.toString()?.trim() }
                .filter { it.isNotEmpty() }.joinToString("\n")
            // O Media3 nao diz quando o cue acaba: estimativa pelo tamanho, e o
            // proximo grupo (inclusive o vazio) substitui antes disso.
            val dur = if (texto.isEmpty()) 0 else (800 + texto.length * 60).coerceIn(2000, 7000)
            val entregar = Runnable {
                if (atual(minha)) try { nativeLegenda(texto, dur) } catch (e: UnsatisfiedLinkError) { }
            }
            if (atrasoMs > 0) principal.postDelayed(entregar, atrasoMs.toLong()) else entregar.run()
        }

        override fun onPlayerError(error: PlaybackException) {
            if (!atual(minha)) return
            val exo = error as? ExoPlaybackException
            val p = player
            val status = generateSequence(error.cause) { it.cause }
                .filterIsInstance<HttpDataSource.InvalidResponseCodeException>().firstOrNull()?.responseCode
            if (limitedSession && status in listOf(403, 458, 509)) {
                live.cancel()
                ev(EV_ERRO, status ?: 0, C.TRACK_TYPE_UNKNOWN)
                try { nativeErroTexto("HTTP $status") } catch (_: Throwable) {}
                return
            }
            val recoveryTimeout = LiveWindowRecovery.timeoutForSegment(liveSegmentMs)
            if (p != null && live.recover(p, error, SystemClock.elapsedRealtime(), recoveryTimeout) {
                    Log.w(TAG, "[live] recuperacao excedeu $recoveryTimeout ms sessao=$minha; seguindo erro normal")
                    try { nativeErroTexto("${error.errorCodeName} (${error.errorCode}): recovery timeout") } catch (_: Throwable) {}
                    ev(EV_ERRO, error.errorCode, C.TRACK_TYPE_UNKNOWN)
                }) {
                Log.i(TAG, "[live] janela/playlist invalida code=${error.errorCode}; buscando posicao ao vivo sem trocar URL sessao=$minha")
                return
            }
            live.cancel()
            val timeout = generateSequence(error.cause) { it.cause }
                .filterIsInstance<androidx.media3.exoplayer.ExoTimeoutException>().firstOrNull()
            if (p != null && RecriaHdr.timeoutRecuperavel(error.errorCode,
                    recriando, timeout?.timeoutOperation ?: -1)) {
                // Our detach timeout must never become a source-open error.
                if (!recuperouSuperficie) {
                    recuperouSuperficie = true
                    principal.post {
                        if (!atual(minha) || player !== p) return@post
                        superficie?.let { p.setVideoSurfaceView(it) }
                        p.seekTo(superficiePos)
                        p.prepare()
                    }
                }
                return
            }
            val tipo = if (exo?.type == ExoPlaybackException.TYPE_RENDERER && p != null &&
                exo.rendererIndex in 0 until p.rendererCount) p.getRendererType(exo.rendererIndex)
                else C.TRACK_TYPE_UNKNOWN
            val renderer = when (tipo) {
                C.TRACK_TYPE_AUDIO -> "audio"
                C.TRACK_TYPE_VIDEO -> "video"
                else -> "desconhecido"
            }
            // Fatal errors can wrap the same sink/codec exception; never log raw messages.
            val causa = generateSequence(error.cause) { it.cause }.take(3)
                .joinToString(" <- ") { camposErroCausa(it) }
            val diagnostico = "${error.errorCodeName} (${error.errorCode}) renderer=$renderer erro=${camposErroCausa(error)} causa=$causa"
            Log.w(TAG, "[player] onPlayerError loadMs=${SystemClock.elapsedRealtime() - abriuEm} $diagnostico")
            try { nativeErroTexto(diagnostico) } catch (_: Throwable) {}
            // Decoder que falha nos primeiros 5 s: o recurso pode estar sendo
            // solto por outro app (ResourceConflict do Tizen); reabre uma vez.
            val cedo = SystemClock.elapsedRealtime() - abriuEm < RETRY_DECODER_MS
            val decoder = error.errorCode == PlaybackException.ERROR_CODE_DECODER_INIT_FAILED ||
                error.errorCode == PlaybackException.ERROR_CODE_DECODING_FAILED
            if (decoder && cedo && !retentou) {
                retentou = true
                val u = urlAtual
                val c = cabAtual
                val inicio = player?.currentPosition?.takeIf { it > 0 }?.coerceAtMost(Int.MAX_VALUE.toLong())?.toInt()
                    ?: inicioAtualMs
                val geracao = geracaoNative
                val pedido = pedidoAtivo
                val fracao = if (inicio > 0) 0 else fracaoAtual
                principal.postDelayed({ if (atual(minha)) abrirMain(u, c, true, inicio, geracao, pedido, fracao) }, 400)
                return
            }
            ev(EV_ERRO, error.errorCode, tipo)
        }
    }

    private fun analitico(minha: Int) = object : AnalyticsListener {
        // Per listener/session and callback/exception type, on the main looper.
        // First occurrence immediately, then at most once every 30 seconds.
        private val errosAudio = HashMap<Pair<String, Class<*>>, Pair<Long, Long>>()
        private fun relatarErroAudio(tipo: String, erro: Exception) {
            if (!atual(minha)) return
            val chave = tipo to erro.javaClass
            val anterior = errosAudio[chave]
            val agora = SystemClock.elapsedRealtime()
            val ocorrencias = (anterior?.second ?: 0L) + 1L
            if (anterior != null && agora - anterior.first < 30_000L) {
                errosAudio[chave] = anterior.first to ocorrencias
                return
            }
            errosAudio[chave] = agora to ocorrencias
            relatarAudio(minha, "$tipo ${camposErroAudio(erro)} repeticoes=${ocorrencias - 1L}")
        }

        override fun onAudioDecoderInitialized(eventTime: AnalyticsListener.EventTime, decoderName: String,
                                               initializedTimestampMs: Long, initializationDurationMs: Long) {
            relatarAudio(minha, "decoder=$decoderName initMs=$initializationDurationMs")
        }

        override fun onAudioInputFormatChanged(eventTime: AnalyticsListener.EventTime, format: Format,
                                               decoderReuseEvaluation: DecoderReuseEvaluation?) {
            relatarAudio(minha, "entrada mime=${format.sampleMimeType} canais=${format.channelCount} hz=${format.sampleRate}")
        }

        override fun onAudioTrackInitialized(eventTime: AnalyticsListener.EventTime, audioTrackConfig: AudioSink.AudioTrackConfig) {
            relatarAudio(minha, "AudioTrack encoding=${audioTrackConfig.encoding} hz=${audioTrackConfig.sampleRate} canaisMask=${audioTrackConfig.channelConfig} offload=${audioTrackConfig.offload} tunneling=${audioTrackConfig.tunneling}")
        }

        override fun onAudioPositionAdvancing(eventTime: AnalyticsListener.EventTime, playoutStartSystemTimeMs: Long) {
            relatarAudio(minha, "AudioTrack avancando inicioMs=$playoutStartSystemTimeMs")
        }

        override fun onAudioSinkError(eventTime: AnalyticsListener.EventTime, audioSinkError: Exception) {
            relatarErroAudio("sink", audioSinkError)
        }

        override fun onAudioCodecError(eventTime: AnalyticsListener.EventTime, audioCodecError: Exception) {
            relatarErroAudio("codec", audioCodecError)
        }

        override fun onDroppedVideoFrames(eventTime: AnalyticsListener.EventTime, droppedFrames: Int, elapsedMs: Long) {
            if (atual(minha)) quadrosPerdidos += droppedFrames
        }

        override fun onVideoDecoderInitialized(
            eventTime: AnalyticsListener.EventTime, decoderName: String,
            initializedTimestampMs: Long, initializationDurationMs: Long
        ) {
            if (!atual(minha)) return
            // DV so conta com decoder DV de verdade: OMX.dolby.* / c2.dolby.* e,
            // na MediaTek, c2.mtk.dvhe.* / c2.mtk.dvav.* (TCL Smart TV Pro: o
            // painel engatou Dolby Vision e o selo dizia HDR10, 30/09/2026).
            Log.i(TAG, "[player] decoder de video: $decoderName")
            decoderDv = decoderDolby(decoderName)
            player?.let { publicarHdr(it.currentTracks) }
        }
    }

    // A duracao chega na timeline assim que o container e lido (MKV: Info +
    // Cues; MP4: moov), antes de o periodo preparar e decodificar. O seek
    // aqui troca o ponto de partida sem um primeiro quadro em 0.
    private fun aplicarFracao(p: ExoPlayer) {
        val f = fracaoPendente
        if (f <= 0) return
        if (p.isCurrentMediaItemLive) {
            fracaoPendente = 0; confirmarRetomada(geracaoNative, false); return
        }
        val d = p.duration
        if (d == C.TIME_UNSET || d <= 1000L || !p.isCurrentMediaItemSeekable) return
        val alvo = d * f / 10000L
        fracaoPendente = 0
        try {
            p.seekTo(alvo)
            Log.i(TAG, "[player] retomada na preparacao: ${alvo} ms de ${d} ms (${f / 100}.${"%02d".format(f % 100)}%)")
            confirmarRetomada(geracaoNative, true)
        } catch (e: Exception) {
            Log.w(TAG, "retomada por percentual recusada: $e")
            confirmarRetomada(geracaoNative, false)
        }
    }

    private fun duracaoMs(p: ExoPlayer): Int {
        val d = p.duration
        return if (d == C.TIME_UNSET || p.isCurrentMediaItemLive) 0 else d.coerceIn(0L, Int.MAX_VALUE.toLong()).toInt()
    }

    // Lista para o C: audio com decoder e legenda de texto suportada. So manda
    // quando a LISTA muda; troca de selecao nao reenvia (o C reiniciaria a
    // legenda). Legenda sobe sempre desligada.
    private fun publicarFaixas(tracks: Tracks) {
        val a = ArrayList<Pair<Tracks.Group, Int>>()
        val l = ArrayList<Pair<Tracks.Group, Int>>()
        var temAudio = false
        for (g in tracks.groups) {
            for (i in 0 until g.length) {
                if (g.type == C.TRACK_TYPE_AUDIO) {
                    temAudio = true
                    if (g.isTrackSupported(i) && a.size < FAIXAS_MAX) a.add(g to i)
                } else if (g.type == C.TRACK_TYPE_TEXT && g.isTrackSupported(i) && l.size < FAIXAS_MAX) {
                    l.add(g to i)
                }
            }
        }
        // Audio no arquivo e nenhum com decoder: o video segue mudo e o C avisa.
        if (temAudio && a.isEmpty() && !avisouSemAudio) { avisouSemAudio = true; ev(EV_AUDIO_SEM_DECODER) }

        val sig = StringBuilder()
        for ((g, i) in a) sig.append('a').append(g.getTrackFormat(i).id).append(g.getTrackFormat(i).language).append(';')
        for ((g, i) in l) sig.append('l').append(g.getTrackFormat(i).id).append(g.getTrackFormat(i).language).append(';')
        val nova = sig.toString()
        if (nova == assinatura) return
        val tinhaLegenda = legendas.isNotEmpty()
        assinatura = nova
        audios = a
        legendas = l
        if (a.isEmpty() && l.isEmpty()) return
        // Lista de legendas mudou: a escolha antiga nao vale (o C zera legAtual).
        if (tinhaLegenda || l.isNotEmpty()) {
            player?.let {
                it.trackSelectionParameters = it.trackSelectionParameters.buildUpon()
                    .setTrackTypeDisabled(C.TRACK_TYPE_TEXT, true).clearOverridesOfType(C.TRACK_TYPE_TEXT).build()
            }
        }
        try {
            var sel = -1
            for ((n, par) in a.withIndex()) {
                val (g, i) = par
                if (sel < 0 && g.isTrackSelected(i)) sel = n
                val fmt = g.getTrackFormat(i)
                nativeFaixa(0, n, fmt.language ?: "", 0, "", fmt.sampleMimeType ?: "", fmt.channelCount.coerceAtLeast(0))
            }
            for ((n, par) in l.withIndex()) {
                val f = par.first.getTrackFormat(par.second)
                val flags = (if (f.selectionFlags and C.SELECTION_FLAG_FORCED != 0) 1 else 0) or
                    (if (f.roleFlags and C.ROLE_FLAG_DESCRIBES_MUSIC_AND_SOUND != 0) 2 else 0)
                nativeFaixa(1, n, f.language ?: "", flags, f.label ?: "", "", 0)
            }
            nativeFaixasFim(sel, -1)
        } catch (e: UnsatisfiedLinkError) { Log.w(TAG, "faixas sem lib: $e") }
    }

    // HDR do Format do video selecionado; Atmos do audio selecionado (E-AC-3
    // JOC). Dolby Vision so quando o decoder escolhido e DV. Manda so se mudou.
    private fun publicarHdr(tracks: Tracks) {
        var hdr = "none"
        var dv = 0
        var atmos = 0
        for (g in tracks.groups) {
            for (i in 0 until g.length) {
                if (!g.isTrackSelected(i)) continue
                val f = g.getTrackFormat(i)
                if (g.type == C.TRACK_TYPE_VIDEO) {
                    val t = f.colorInfo?.colorTransfer
                    if (f.sampleMimeType == MimeTypes.VIDEO_DOLBY_VISION && decoderDv) { hdr = "DolbyVision"; dv = 1 }
                    else if (t == C.COLOR_TRANSFER_ST2084) hdr = "HDR10"
                    else if (t == C.COLOR_TRANSFER_HLG) hdr = "HLG"
                } else if (g.type == C.TRACK_TYPE_AUDIO && f.sampleMimeType == MimeTypes.AUDIO_E_AC3_JOC) {
                    atmos = 1
                }
            }
        }
        if (hdr == ultHdr && dv == ultDv && atmos == ultAtmos) return
        Log.i(TAG, "[dvtrace] hdr sessao=$sessao $ultHdr->$hdr decoderDv=$decoderDv quadro=$quadroVisto estavel=$estavelEmitido soltando=${soltando != null}")
        ultHdr = hdr; ultDv = dv; ultAtmos = atmos
        try { nativeHdr(hdr, dv, atmos) } catch (e: UnsatisfiedLinkError) { }
        hdrNaSuperficie()
    }
}
