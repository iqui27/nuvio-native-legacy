package space.nuvio.nativelegacy

import android.app.Activity
import android.content.Context
import android.net.Uri
import android.os.Handler
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
import androidx.media3.exoplayer.ExoPlaybackException
import androidx.media3.exoplayer.ExoPlayer
import androidx.media3.exoplayer.Renderer
import androidx.media3.exoplayer.analytics.AnalyticsListener
import androidx.media3.exoplayer.audio.AudioSink
import androidx.media3.exoplayer.audio.DefaultAudioSink
import androidx.media3.exoplayer.source.DefaultMediaSourceFactory
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

    private const val TELA_W = 1920   // coordenadas de layout do app
    private const val TELA_H = 1080
    private const val FAIXAS_MAX = 32 // NV_FAIXA_MAX do C
    private const val TIQUE_MS = 250L
    private const val RETRY_DECODER_MS = 5000L

    private val principal = Handler(Looper.getMainLooper())
    private var activity: Activity? = null
    private var camada: FrameLayout? = null
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
    private var urlAtual = ""
    private var cabAtual = ""
    private var abriuEm = 0L
    private var retentou = false
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

    // --- natives (src/video_android.c, Java_space_nuvio_nativelegacy_NvPlayer_*) ---
    @JvmStatic external fun nativeIniciar()
    @JvmStatic external fun nativeEvento(tipo: Int, a: Int, b: Int)
    // flags (#287): 1 = forcada (SELECTION_FLAG_FORCED), 2 = SDH
    // (ROLE_FLAG_DESCRIBES_MUSIC_AND_SOUND); nome = Format.label.
    // mime/canais (#293): codec e canais da faixa de audio.
    @JvmStatic external fun nativeFaixa(tipo: Int, idx: Int, lingua: String, flags: Int, nome: String, mime: String, canais: Int)
    @JvmStatic external fun nativeFaixasFim(selAudio: Int, selLeg: Int)
    @JvmStatic external fun nativeLegenda(texto: String, durMs: Int)
    @JvmStatic external fun nativePos(ms: Int)
    @JvmStatic external fun nativeDecoder4k(hevc: Int, avc: Int, vp9: Int, av1: Int)
    @JvmStatic external fun nativeTela(hdr: Int, dv: Int)
    @JvmStatic external fun nativeHdr(hdr: String, dv: Int, atmos: Int)
    @JvmStatic external fun nativeRetomada(geracao: Int, aceita: Int)
    @JvmStatic external fun nativeFitPassiva(rede: Long, geracao: Int, origem: String, kbps: IntArray, fimMs: Long)
    @JvmStatic external fun nativeAudioPcm(pcm: ShortArray, n: Int, ptsUs: Long)
    @JvmStatic external fun nativeAudioEstado(fmt: Int)

    // F06 sincronia por audio (AudioSyncTap.kt / AudioSyncSink.kt). Fio de
    // reproducao; o C so copia para um anel limitado (src/audsync.c).
    private val tap = AudioSyncTap(
        { pcm, n, pts -> try { nativeAudioPcm(pcm, n, pts) } catch (t: UnsatisfiedLinkError) { } },
        { fmt -> try { nativeAudioEstado(fmt) } catch (t: UnsatisfiedLinkError) { } }
    )

    // F07 (src/video_android.c -> cacheboost.h): seek cache state of this open.
    // The audio output state for the boost is F06's nativeAudioEstado (one
    // detection for both: PCM vs bitstream at the sink input).
    @JvmStatic external fun nativeCache(estado: Int, pedidoMb: Int, limiteMb: Int, usadoMb: Int)

    // --- F07: seek cache and volume boost ---------------------------------------
    // The cache limit is STICKY (the C side sends it only when it changes) and
    // read at each open; any thread writes it, the open reads it on main.
    @Volatile private var cacheMbPedido = 0
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

    private fun ev(tipo: Int, a: Int = 0, b: Int = 0) {
        try { nativeEvento(tipo, a, b) } catch (e: UnsatisfiedLinkError) { Log.w(TAG, "evento $tipo sem lib: $e") }
    }

    // --- ciclo de vida (NuvioActivity) ---------------------------------------

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
            if (r - l != or2 - ol || b - t != ob - ot) aplicarJanela()
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
        try { player?.pause() } catch (e: Exception) { Log.w(TAG, "pausarPeloSistema: $e") }
    }

    // onDestroy: solta player e superficie.
    @JvmStatic
    fun encerrar() {
        pedidos.incrementAndGet()
        liberar()
        val sv = superficie
        if (sv != null) (sv.parent as? FrameLayout)?.removeView(sv)
        superficie = null
        camada = null
        activity = null
    }

    // --- chamadas do C (qualquer fio) -----------------------------------------

    @JvmStatic fun abrir(url: String, cabecalhos: String) { abrirPosicao(url, cabecalhos, 0, 0) }
    @JvmStatic fun abrirPosicao(url: String, cabecalhos: String, inicioMs: Int, geracao: Int) {
        val pedido = pedidos.incrementAndGet()
        principal.post {
            if (pedido == pedidos.get()) abrirMain(url, cabecalhos, false, inicioMs.coerceAtLeast(0), geracao, pedido)
        }
    }
    @JvmStatic fun abrirRetomada(url: String, cabecalhos: String, inicioMs: Int, fracao: Int, geracao: Int) {
        val pedido = pedidos.incrementAndGet()
        principal.post {
            if (pedido == pedidos.get())
                abrirMain(url, cabecalhos, false, inicioMs.coerceAtLeast(0), geracao, pedido, fracao.coerceIn(0, 9999))
        }
    }
    @JvmStatic fun parar() {
        val pedido = pedidos.incrementAndGet()
        principal.post { if (pedido == pedidos.get()) liberar() }
    }
    @JvmStatic fun pausar(p: Int) { principal.post { player?.playWhenReady = (p == 0) } }
    @JvmStatic fun buscar(ms: Int) {
        val pedido = pedidos.get()
        principal.post { if (pedido == pedidos.get()) player?.seekTo(ms.coerceAtLeast(0).toLong()) }
    }
    @JvmStatic fun volume(pct: Int) { principal.post { player?.volume = pct.coerceIn(0, 100) / 100f } }
    // F07: cache limit for the NEXT opens (MB, 0 = off) and the session volume
    // 0..200 (above 100 only while the audio is PCM).
    @JvmStatic fun cache(mb: Int) { cacheMbPedido = mb.coerceIn(0, 1024) }
    @JvmStatic fun ganho(pct: Int) {
        principal.post { ganhoPct = GanhoMath.pct(pct); aplicarGanho() }
    }
    // #202: velocidade em centesimos. O C reaplica a cada abertura (o player
    // novo nasce em 1x), entao aqui nao fica estado.
    @JvmStatic fun velocidade(centesimos: Int) {
        val v = centesimos.coerceIn(25, 400) / 100f
        principal.post { player?.setPlaybackSpeed(v) }
    }
    @JvmStatic fun janela(x: Int, y: Int, w: Int, h: Int, encaixa: Int) { principal.post { definirJanela(x, y, w, h, encaixa != 0) } }
    @JvmStatic fun escolher(tipo: Int, idx: Int) { principal.post { escolherMain(tipo, idx) } }

    // --- abrir / liberar ------------------------------------------------------

    private fun abrirMain(url: String, cabecalhos: String, reabrindo: Boolean,
                          inicioMs: Int, geracao: Int, pedido: Int, fracao: Int = 0, esperaInicio: Long = -1L) {
        if (pedido != pedidos.get()) return
        val act = activity
        if (act == null) { confirmarRetomada(geracao, false); return }
        val espera = if (esperaInicio < 0) SystemClock.elapsedRealtime() else esperaInicio
        if (esperaInicio < 0) {
            liberar()
            // O C envia o ganho depois de abrir; a espera nao pode apaga-lo.
            ganhoPct = 100
        }
        // MStar/Amlogic: o overlay e o decoder antigos precisam sair antes do novo.
        // Poll no Handler deixa Back/parar funcionar enquanto o release corre em fundo.
        if (semRecriar) {
            val ms = SystemClock.elapsedRealtime() - espera
            if (liberacoesEmCurso.get() > 0 && ms < 3000) {
                principal.postDelayed({
                    abrirMain(url, cabecalhos, reabrindo, inicioMs, geracao, pedido, fracao, espera)
                }, 25)
                return
            }
            Log.i(TAG, "[player] esperou o release anterior $ms ms (pendentes=${liberacoesEmCurso.get()})")
        }
        novaSuperficie(act)
        pedidoAtivo = pedido
        hdrRecriado = false; hdrRecriadoPara = ""; quadroVisto = false
        hdrSegundaFeita = false; estavelEmitido = false
        principal.removeCallbacks(recriar)
        principal.removeCallbacks(hdrSegunda)
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
            val http = DefaultHttpDataSource.Factory()
                .setAllowCrossProtocolRedirects(true)
                .setConnectTimeoutMs(15000)
                .setReadTimeoutMs(20000)
            val props = HashMap<String, String>()
            var ua: String? = null
            for (linha in cabecalhos.split('\n')) {
                val i = linha.indexOf(':')
                if (i <= 0) continue
                val nome = linha.substring(0, i).trim()
                val valor = linha.substring(i + 1).trim()
                if (nome.isEmpty()) continue
                if (nome.equals("User-Agent", ignoreCase = true)) { http.setUserAgent(valor); ua = valor } else props[nome] = valor
            }
            http.setDefaultRequestProperties(props)
            // Arquivo progressivo vem em varias conexoes (ParaleloDataSource.kt:
            // o Android limita a janela TCP de cada uma); HLS/DASH seguem na unica.
            // A geracao e a sessao nativa desta abertura (0 numa casca antiga:
            // o C nunca aceita a telemetria passiva dela).
            medidorToken = medidor.sessao(geracao)
            val rede = ParaleloDataSource.Factory(ua, props, http, medidor, medidorToken)

            // F07: SEEK CACHE over the chain above, progressive HTTP VOD only.
            // HLS/DASH (and the live channels, which C never arms) stay out:
            // their segments and playlists have no tested cache contract.
            val baixa = url.lowercase()
            val adaptativo = baixa.contains(".m3u8") || baixa.contains("m3u8?") || baixa.contains(".mpd")
            val pedidoCache = cacheMbPedido
            var origem: androidx.media3.datasource.DataSource.Factory = rede
            if (pedidoCache <= 0) cacheEstado = CacheSessao.DESLIGADO
            else if (adaptativo || !ParaleloDataSource.serve(Uri.parse(url))) cacheEstado = CacheSessao.NAO_SE_APLICA
            else {
                val (c, estado) = CacheMidia.criar(act.cacheDir, pedidoCache) { semEspaco ->
                    // Loader thread: back to main, only for this session.
                    principal.post {
                        if (!atual(minha) || cacheEstado != CacheSessao.ATIVO) return@post
                        cacheEstado = if (semEspaco) CacheSessao.DISCO_CHEIO else CacheSessao.FALHOU
                        Log.w(TAG, "seek cache off for this session (${if (semEspaco) "ENOSPC" else "write error"})")
                        relatarCache(minha)
                    }
                }
                cacheMidia = c
                cacheEstado = estado
                if (c != null) origem = c.fabrica(rede)
            }
            relatarCache(minha)

            // F07: VOLUME BOOST. Preserve the gain received during release;
            // the processor is per player, inside the audio sink.
            val proc = GanhoAudioProcessor()
            processador = proc

            // ON (e nao PREFER): o decodificador da plataforma e o passthrough
            // continuam primeiro; o FFmpeg so entra no codec que a TV nao tem.
            // SINK ORDER (F06 + F07): AudioSyncSink(DefaultAudioSink[gain processor]).
            // The F06 tap reads handleBuffer at the sink INPUT: decoded PCM with
            // its media time, before any processing, so audio sync analyses the
            // unboosted, unlimited signal. The F07 gain runs inside the sink's
            // processor chain, i.e. on what goes to the output. Same sink as the
            // default otherwise (float output and playback params as asked);
            // the FFmpeg renderer gets this same sink. Neither changes
            // passthrough/offload.
            val renderizadores = object : androidx.media3.exoplayer.DefaultRenderersFactory(act) {
                override fun buildAudioSink(context: Context, enableFloatOutput: Boolean,
                                            enableAudioTrackPlaybackParams: Boolean): AudioSink =
                    AudioSyncSink(DefaultAudioSink.Builder(context)
                        .setEnableFloatOutput(enableFloatOutput)
                        .setEnableAudioTrackPlaybackParams(enableAudioTrackPlaybackParams)
                        .setAudioProcessors(arrayOf(proc))
                        .build(), tap)
            }
                .setExtensionRendererMode(androidx.media3.exoplayer.DefaultRenderersFactory.EXTENSION_RENDERER_MODE_ON)
                .setEnableDecoderFallback(true)
            // TETO DO BUFFER EM BYTES (03/10, TCL C755: dois OutOfMemoryError
            // tocando filme). O DefaultLoadControl aceita ate ~144 MB de video e
            // audio, e esses bytes moram no heap JAVA, que nesta TV para em
            // 192 MB: um remux 4K enchia o buffer e derrubava o app. Um quarto
            // do heap, entre 32 e 96 MB, ainda segura dezenas de segundos de 4K.
            val tetoBuffer = (Runtime.getRuntime().maxMemory() / 4)
                .coerceIn(32L shl 20, 96L shl 20).toInt()
            val carga = androidx.media3.exoplayer.DefaultLoadControl.Builder()
                .setTargetBufferBytes(tetoBuffer)
                .setPrioritizeTimeOverSizeThresholds(false)
                .build()
            val p = ExoPlayer.Builder(act, renderizadores)
                .setLoadControl(carga)
                .setMediaSourceFactory(DefaultMediaSourceFactory(act)
                    // DefaultDataSource e nao so http: o trailer da Apple chega como
                    // file:// (master reduzido a uma variante em dados/trailer, trailerapple.c).
                    .setDataSourceFactory(DefaultDataSource.Factory(act, origem)))
                .build()
            player = p
            aplicarGanho()
            semTravaDeFio(p)
            // Release anterior ainda preso no HAL de audio: sessao de audio nova,
            // para o AudioTrack deste filme nao esperar o patch do velho.
            if (liberacoesEmCurso.get() > 0) {
                try {
                    p.audioSessionId = androidx.media3.common.util.Util.generateAudioSessionIdV21(act)
                    Log.i(TAG, "[player] release anterior em curso; sessao de audio nova")
                } catch (e: Exception) { Log.w(TAG, "sessao de audio nova: $e") }
            }
            // Foco de audio GAIN; perder o foco pausa (o C ve o evento 3).
            p.setAudioAttributes(
                AudioAttributes.Builder().setUsage(C.USAGE_MEDIA).setContentType(C.AUDIO_CONTENT_TYPE_MOVIE).build(), true)
            // Legenda desligada ate o app escolher: quem desenha e o C.
            p.trackSelectionParameters = p.trackSelectionParameters.buildUpon()
                .setTrackTypeDisabled(C.TRACK_TYPE_TEXT, true).build()
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

            val item = MediaItem.Builder().setUri(url)
            if (baixa.contains(".m3u8") || baixa.contains("m3u8?")) item.setMimeType(MimeTypes.APPLICATION_M3U8)
            else if (baixa.contains(".mpd")) item.setMimeType(MimeTypes.APPLICATION_MPD)
            val mediaItem = item.build()
            var inicioAceito = false
            if (inicioMs > 0) {
                try {
                    // Media3 recebe o ponto antes de prepare: nao carrega o
                    // inicio so para o C pedir outro Range/seek logo depois.
                    p.setMediaItem(mediaItem, inicioMs.toLong())
                    inicioAceito = true
                } catch (e: Exception) {
                    Log.w(TAG, "posicao inicial recusada; usando retomada normal: $e")
                    p.setMediaItem(mediaItem)
                }
            } else p.setMediaItem(mediaItem)
            // So percentual: o ack sai quando a timeline der a duracao (ou
            // recusado no READY); ate la o C espera, sem seek proprio.
            if (inicioMs <= 0 && fracaoAtual > 0) fracaoPendente = fracaoAtual
            else confirmarRetomada(geracao, inicioAceito)
            p.playWhenReady = true
            p.prepare()
            principal.postDelayed(tique(minha), TIQUE_MS)
        } catch (e: Exception) {
            confirmarRetomada(geracao, false)
            Log.w(TAG, "abrir: $e")
            ev(EV_ERRO, -1, 0)
        }
    }

    // Sobe a sessao e solta tudo; qualquer callback pendente do player velho
    // morre na conferencia do numero.
    private fun liberar() {
        sessao++
        medidor.encerrar()
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
            // Listeners fora ja (so mexem em lista local); o resto, no fio de fundo.
            try { if (ouv != null) p.removeListener(ouv) } catch (e: Exception) { }
            try { if (ana != null) p.removeAnalyticsListener(ana) } catch (e: Exception) { }
            // Esconde sem destruir a superficie (destruir acorda o callback do Media3).
            try { sv?.layoutParams = FrameLayout.LayoutParams(1, 1) } catch (e: Exception) { }
            liberarEmFundo(p, sv)
            // A superficie velha fica com o player velho; a proxima abertura cria outra.
            superficie = null
        }
        // F07: the session cache goes with its player (released and deleted
        // off the main thread, after the canceled loaders unwind).
        cacheMidia?.liberar()
        cacheMidia = null
        cacheEstado = CacheSessao.DESLIGADO
        cacheTiques = 0
        processador = null
        // Depois do release (o fio de reproducao ja parou): sem audio decodificado.
        tap.encerrar()
        superficie?.visibility = View.GONE
    }

    // O release roda em outro fio (liberarEmFundo) e o Media3 1.8 lanca se um
    // fio que nao e o da aplicacao toca o player; o seletor e so do pacote, entao
    // reflexao. Sem ele (outra versao), o release volta ao fio principal.
    private fun semTravaDeFio(p: ExoPlayer) {
        try {
            val m = p.javaClass.getDeclaredMethod("setThrowsWhenUsingWrongThread", Boolean::class.javaPrimitiveType)
            m.isAccessible = true
            m.invoke(p, false)
            fioLivre = true
        } catch (e: Throwable) {
            fioLivre = false
            Log.w(TAG, "[player] sem destravar o fio do release (volta ao fio principal): $e")
        }
    }
    @Volatile private var fioLivre = false

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
        c.addView(sv, FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT))
        superficie = sv
    }

    // QUANTO O MEDIA3 SEGUROU O FIO PRINCIPAL ao perder a superficie (recriar,
    // ou o sistema ao esconder o app: tecla de ajustes da TV, Home). O callback
    // dele entra na lista entre estes dois (setVideoSurfaceView, em abrirMain).
    private var destruirIni = 0L
    private val antesDoMedia3 = object : SurfaceHolder.Callback {
        override fun surfaceCreated(h: SurfaceHolder) {
            Log.i(TAG, "[dvtrace] created sessao=$sessao holder=${System.identityHashCode(h)} atual=${h === superficie?.holder} hdr=$ultHdr frame=${h.surfaceFrame}")
        }
        override fun surfaceChanged(h: SurfaceHolder, f: Int, w: Int, a: Int) {
            Log.i(TAG, "[dvtrace] changed sessao=$sessao holder=${System.identityHashCode(h)} atual=${h === superficie?.holder} tamanho=${w}x$a formato=$f hdr=$ultHdr")
        }
        override fun surfaceDestroyed(h: SurfaceHolder) {
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

    // stop+release do player velho fora do fio principal. A SurfaceView so sai
    // da tela depois: tirar antes dispararia o callback de superficie do
    // Media3, que espera o fio de reproducao (justo o que esta preso).
    private fun liberarEmFundo(p: ExoPlayer, sv: SurfaceView?) {
        liberacoesEmCurso.incrementAndGet()
        val corpo = Runnable {
            val ini = SystemClock.elapsedRealtime()
            try { p.stop() } catch (e: Exception) { Log.w(TAG, "stop: $e") }
            try { p.release() } catch (e: Exception) { Log.w(TAG, "release: $e") }
            val ms = SystemClock.elapsedRealtime() - ini
            if (ms > 5000) Log.w(TAG, "[player] release anterior levou $ms ms (>5 s: HAL de audio preso?)")
            else Log.i(TAG, "[player] release anterior levou $ms ms")
            liberacoesEmCurso.decrementAndGet()
            principal.post {
                if (sv != null && sv !== superficie) (sv.parent as? FrameLayout)?.removeView(sv)
            }
        }
        if (fioLivre) { val t = Thread(corpo, "nv-release"); t.isDaemon = true; t.start() }
        else corpo.run()
    }

    // Tique de 250 ms: a posicao que o C le sem esperar ninguem.
    private fun tique(minha: Int): Runnable = object : Runnable {
        override fun run() {
            if (!atual(minha)) return
            val p = player ?: return
            try { nativePos(p.currentPosition.coerceIn(0L, Int.MAX_VALUE.toLong()).toInt()) } catch (e: UnsatisfiedLinkError) { }
            if (cacheMidia != null && cacheEstado == CacheSessao.ATIVO && ++cacheTiques >= CACHE_RELATO_TIQUES) {
                cacheTiques = 0
                relatarCache(minha)
            }
            principal.postDelayed(this, TIQUE_MS)
        }
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

    // Sem janela pedida ainda: o quadro inteiro encaixado (letterbox) na tela.
    private fun aplicarEncaixe() {
        if (temJanela) return
        if (videoW > 0 && videoH > 0) {
            val esc = minOf(TELA_W.toFloat() / videoW, TELA_H.toFloat() / videoH)
            jw = (videoW * esc + 0.5f).toInt()
            jh = (videoH * esc + 0.5f).toInt()
            jx = (TELA_W - jw) / 2
            jy = (TELA_H - jh) / 2
        } else {
            jx = 0; jy = 0; jw = TELA_W; jh = TELA_H
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
        val ex = cw.toFloat() / TELA_W
        val ey = ch.toFloat() / TELA_H
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
        recriarSuperficie(RECRIA_ESPERA_MS)
    }

    // Recriar a Surface custa um quadro preto e, em rajada (aspecto apertado
    // varias vezes), travava: as trocas JUNTAM numa so, RECRIA_ESPERA_MS depois
    // da ultima. A mesma recriacao liga o HDR da TCL (ver onRenderedFirstFrame).
    private const val RECRIA_ESPERA_MS = 350L
    private var hdrRecriado = false
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
            // Sem a mensagem, o caminho antigo: o Media3 espera no surfaceDestroyed.
            Log.w(TAG, "soltarSaida: $e")
            depois.run()
        }
    }
    private val recriar = Runnable {
        val p = player
        val sv = superficie
        Log.i(TAG, "[dvtrace] recriar player=${System.identityHashCode(p)} holder=${System.identityHashCode(sv?.holder)} soltando=${soltando === p} vis=${sv?.visibility} valida=${sv?.holder?.surface?.isValid}")
        if (p != null && sv != null && soltando !== p && sv.visibility == View.VISIBLE && sv.holder.surface?.isValid == true) {
            soltando = p
            soltarSaida(p) {
                if (soltando === p) soltando = null
                if (player !== p || superficie !== sv) return@soltarSaida
                val ini = SystemClock.elapsedRealtime()
                sv.visibility = View.GONE
                Log.i(TAG, "[player] superficie: destruir levou ${SystemClock.elapsedRealtime() - ini} ms no fio principal")
                principal.post {
                    if (player != null) sv.visibility = View.VISIBLE
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
        if (semRecriar) return
        principal.removeCallbacks(recriar)
        principal.postDelayed(recriar, atrasoMs)
    }
    // A TCL so liga o modo HDR do painel quando a Surface nasce com o decoder
    // ja em HDR: na abertura ela nasceu antes (SDR) e o HDR so aparecia depois
    // de trocar o aspecto (dono, 30/09). Uma recriacao no primeiro quadro HDR.
    // UMA SO NAO BASTA (TCL, 06/10/2026): de vez em quando o filme DV ainda
    // abria escuro e so voltava trocando o aspecto. No log do painel a troca
    // de aspecto reconecta o decoder ao DispLink e o DolbyModule passa de
    // "SDR -> Dolby"; a recriacao de 200 ms depois do primeiro quadro as vezes
    // chega antes de o modulo Dolby ver o fluxo. Recria de novo por tipo de
    // HDR (HDR10 -> DolbyVision tambem conta) e uma segunda vez 1,5 s depois
    // do primeiro quadro HDR, o mesmo gesto que o dono fazia na mao.
    private var hdrRecriadoPara = ""
    private val hdrSegunda = Runnable {
        if (player != null && quadroVisto && ultHdr.isNotEmpty() && ultHdr != "none") {
            Log.i(TAG, "HDR ($ultHdr): segunda recriacao da superficie")
            recriarSuperficie(0)
        }
        hdrSegundaFeita = true
        avaliarEstavel()
    }
    // SUPERFICIE ESTAVEL (DV escuro, 2.0.3): o C so aplica o aspecto salvo
    // depois da ULTIMA recriacao agendada. SDR (ou MStar, que nao recria):
    // logo apos o primeiro quadro. HDR/DV: depois da segunda recriacao.
    private var hdrSegundaFeita = false
    private var estavelEmitido = false
    private val estavelRun = Runnable {
        if (player != null && quadroVisto && !estavelEmitido) {
            estavelEmitido = true
            Log.i(TAG, "[aspect] superficie estavel (hdr=$ultHdr) sessao=$sessao soltando=${soltando != null} segunda=$hdrSegundaFeita holder=${System.identityHashCode(superficie?.holder)}")
            ev(EV_SUPERFICIE_ESTAVEL)
        }
    }
    private fun avaliarEstavel() {
        if (!quadroVisto || estavelEmitido) return
        val hdr = ultHdr.isNotEmpty() && ultHdr != "none" && !semRecriar
        principal.removeCallbacks(estavelRun)
        if (hdr && !hdrSegundaFeita) return   // a segunda recriacao chama de novo
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
        // MStar/Amlogic nao recriam (RecriaHdr.kt): sem log de "recria" nem a
        // segunda rodada; avaliarEstavel ja trata esse caso como SDR.
        if (semRecriar) return
        if (!quadroVisto || ultHdr.isEmpty() || ultHdr == "none" || ultHdr == hdrRecriadoPara) return
        val primeira = !hdrRecriado
        hdrRecriado = true
        hdrRecriadoPara = ultHdr
        Log.i(TAG, "HDR ($ultHdr): recria a superficie para a TV ligar o modo HDR")
        recriarSuperficie(200)
        if (primeira) {
            principal.removeCallbacks(hdrSegunda)
            principal.postDelayed(hdrSegunda, 1500L)
        }
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
        override fun onPlaybackStateChanged(state: Int) {
            if (!atual(minha)) return
            val p = player ?: return
            Log.i(TAG, "[player] onPlaybackStateChanged state=$state renderer=audio(${p.audioFormat?.sampleMimeType}),video(${p.videoFormat?.sampleMimeType}) loadMs=${SystemClock.elapsedRealtime() - abriuEm}")
            when (state) {
                Player.STATE_BUFFERING -> ev(EV_BUFFER, 0)
                Player.STATE_READY -> {
                    ev(EV_BUFFER, 100)
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
            if (!playWhenReady && !p.isPlaying && p.playbackState != Player.STATE_ENDED) ev(EV_PAUSADO)
            medirEstado(p)
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
            if (!pronto) { aplicarFracao(p); return }
            val d = duracaoMs(p)
            if (d != duracaoEnviada) { duracaoEnviada = d; ev(EV_PRONTO, d) }   // so a duracao muda
        }

        override fun onRenderedFirstFrame() {
            if (!atual(minha)) return
            Log.i(TAG, "[player] onRenderedFirstFrame renderer=video loadMs=${SystemClock.elapsedRealtime() - abriuEm} sessao=$sessao hdr=$ultHdr holder=${System.identityHashCode(superficie?.holder)} soltando=${soltando != null}")
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
            val tipo = if (exo?.type == ExoPlaybackException.TYPE_RENDERER && p != null &&
                exo.rendererIndex in 0 until p.rendererCount) p.getRendererType(exo.rendererIndex)
                else C.TRACK_TYPE_UNKNOWN
            val renderer = when (tipo) {
                C.TRACK_TYPE_AUDIO -> "audio"
                C.TRACK_TYPE_VIDEO -> "video"
                else -> "desconhecido"
            }
            Log.w(TAG, "[player] onPlayerError renderer=$renderer nome=${exo?.rendererName} loadMs=${SystemClock.elapsedRealtime() - abriuEm} erro ${error.errorCodeName} (${error.errorCode}): ${error.message}")
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

        override fun onVideoDecoderInitialized(
            eventTime: AnalyticsListener.EventTime, decoderName: String,
            initializedTimestampMs: Long, initializationDurationMs: Long
        ) {
            if (!atual(minha)) return
            // DV so conta com decoder DV de verdade: OMX.dolby.* / c2.dolby.* e,
            // na MediaTek, c2.mtk.dvhe.* / c2.mtk.dvav.* (TCL Smart TV Pro: o
            // painel engatou Dolby Vision e o selo dizia HDR10, 30/09/2026).
            Log.i(TAG, "[player] decoder de video: $decoderName")
            val n = decoderName.lowercase()
            decoderDv = n.contains("dolby") || Regex("""\.dv(he|h1|av|a1)""").containsMatchIn(n)
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
        Log.i(TAG, "[dvtrace] hdr sessao=$sessao $ultHdr->$hdr decoderDv=$decoderDv quadro=$quadroVisto estavel=$estavelEmitido segunda=$hdrSegundaFeita soltando=${soltando != null}")
        ultHdr = hdr; ultDv = dv; ultAtmos = atmos
        try { nativeHdr(hdr, dv, atmos) } catch (e: UnsatisfiedLinkError) { }
        hdrNaSuperficie()
    }
}
