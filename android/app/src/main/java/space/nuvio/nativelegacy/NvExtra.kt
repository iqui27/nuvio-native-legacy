package space.nuvio.nativelegacy

import android.app.Activity
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.Gravity
import android.view.SurfaceView
import android.widget.FrameLayout
import androidx.media3.common.MediaItem
import androidx.media3.common.MimeTypes
import androidx.media3.common.PlaybackException
import androidx.media3.common.Player
import androidx.media3.datasource.DefaultHttpDataSource
import androidx.media3.exoplayer.ExoPlayer
import androidx.media3.exoplayer.DefaultLoadControl
import androidx.media3.exoplayer.source.DefaultMediaSourceFactory

// MULTIVIEW — decodificadores EXTRA alem do player principal (NvPlayer).
// Um ExoPlayer + SurfaceView por slot, na mesma `camada` atras da SDLSurface;
// o C abre um "furo" alfa 0 por slot. Sem audio (volume 0 — o audio e do slot
// principal), sem cache/AFR/HDR/legenda: so live HLS/TS de URL direta.
//
// Nada de URL, usuario ou senha no log: o identificador e o slot.
object NvExtra {
    private const val TAG = "NvExtra"
    private const val MAX = 3
    private val principal = Handler(Looper.getMainLooper())
    private var activity: Activity? = null
    private var camada: FrameLayout? = null

    private class Slot {
        var player: ExoPlayer? = null
        var superficie: SurfaceView? = null
        @Volatile var estado = 0          // 0 parado, 1 preparando, 2 tocando, -1 erro
        var janela: IntArray? = null      // x, y, w, h em pixels de layout 1920x1080
        var geracao = 0
    }
    private val slots = Array(MAX) { Slot() }

    @JvmStatic
    fun iniciar(act: Activity, camada: FrameLayout) {
        this.activity = act
        this.camada = camada
        try { nativeIniciar() } catch (e: UnsatisfiedLinkError) {
            Log.w(TAG, "nativeIniciar sem a lib ainda; o C acha a classe pelo ClassLoader")
        }
    }
    @JvmStatic
    external fun nativeIniciar()

    // Quantos decodificadores extras o aparelho sustenta. Desconhecido: 1
    // (duas telas no total, o padrao pedido para Android TV); se a criacao do
    // player falhar, o slot cai em erro e o C para de pedir.
    @JvmStatic fun maximo(): Int = 1

    private fun itemMidia(url: String): MediaItem {
        val baixa = url.lowercase()
        val b = MediaItem.Builder().setUri(url)
        if (baixa.contains(".m3u8") || baixa.contains("m3u8?")) b.setMimeType(MimeTypes.APPLICATION_M3U8)
        else if (baixa.contains(".mpd")) b.setMimeType(MimeTypes.APPLICATION_MPD)
        return b.build()
    }

    @JvmStatic fun abrir(slot: Int, url: String) {
        principal.post {
            val act = activity ?: return@post
            val c = camada ?: return@post
            if (slot < 0 || slot >= MAX) return@post
            val s = slots[slot]
            pararMain(s)
            s.geracao++
            val minha = s.geracao
            s.estado = 1
            Log.i(TAG, "[mv] slot $slot: abrindo")
            val sv = SurfaceView(act)
            sv.isFocusable = false
            c.addView(sv, FrameLayout.LayoutParams(1, 1))
            s.superficie = sv
            val load = DefaultLoadControl.Builder()
                .setBufferDurationsMs(6_000, 20_000, 1_500, 3_000)
                .build()
            val http = DefaultHttpDataSource.Factory()
                .setAllowCrossProtocolRedirects(true)
                .setConnectTimeoutMs(10_000)
                .setReadTimeoutMs(10_000)
            val p = ExoPlayer.Builder(act)
                .setLoadControl(load)
                .setMediaSourceFactory(DefaultMediaSourceFactory(act).setDataSourceFactory(http))
                .build()
            s.player = p
            p.setVideoSurfaceView(sv)
            p.volume = 0f
            p.addListener(object : Player.Listener {
                override fun onPlaybackStateChanged(state: Int) {
                    if (s.geracao != minha) return
                    if (state == Player.STATE_READY) { s.estado = 2; Log.i(TAG, "[mv] slot $slot: tocando") }
                    else if (state == Player.STATE_IDLE && s.estado != 0) s.estado = 0
                }
                override fun onPlayerError(error: PlaybackException) {
                    if (s.geracao != minha) return
                    s.estado = -1
                    Log.i(TAG, "[mv] slot $slot: erro codigo=${error.errorCode}")
                }
            })
            p.setMediaItem(itemMidia(url))
            p.prepare()
            p.playWhenReady = true
            s.janela?.let { aplicarJanela(s, it) }
        }
    }

    private fun aplicarJanela(s: Slot, j: IntArray) {
        val c = camada ?: return
        val sv = s.superficie ?: return
        var cw = c.width; var ch = c.height
        if (cw < 1 || ch < 1) {
            val m = c.resources.displayMetrics
            cw = m.widthPixels; ch = m.heightPixels
        }
        val ex = cw.toFloat() / larguraLogica()
        val ey = ch.toFloat() / alturaLogica()
        val j = s.janela ?: return
        val x0 = Math.round(j[0] * ex)
        val y0 = Math.round(j[1] * ey)
        val x1 = Math.round((j[0] + j[2]) * ex)
        val y1 = Math.round((j[1] + j[3]) * ey)
        val lp = FrameLayout.LayoutParams(maxOf(1, x1 - x0), maxOf(1, y1 - y0))
        lp.gravity = Gravity.TOP or Gravity.START
        lp.leftMargin = x0
        lp.topMargin = y0
        sv.layoutParams = lp
    }

    @JvmStatic fun janela(slot: Int, x: Int, y: Int, w: Int, h: Int) {
        principal.post {
            if (slot < 0 || slot >= MAX) return@post
            val s = slots[slot]
            val j = intArrayOf(x, y, w, h)
            if (s.janela != null && s.janela!!.contentEquals(j)) return@post
            s.janela = j
            aplicarJanela(s, j)
        }
    }

    private fun pararMain(s: Slot) {
        s.estado = 0
        s.player?.let { p ->
            p.stop()
            p.clearVideoSurface()
            // Soltar fora do fio principal: release() ja segurou o AudioFlinger
            // por dezenas de segundos na TCL (ver NvPlayer).
            Thread { p.release() }.start()
        }
        s.player = null
        s.superficie?.let { (it.parent as? FrameLayout)?.removeView(it) }
        s.superficie = null
    }

    @JvmStatic fun parar(slot: Int) {
        principal.post {
            if (slot < 0 || slot >= MAX) return@post
            slots[slot].geracao++
            pararMain(slots[slot])
        }
    }

    @JvmStatic fun pararTodos() {
        principal.post {
            for (s in slots) { s.geracao++; pararMain(s) }
        }
    }

    @JvmStatic fun estado(slot: Int): Int = if (slot < 0 || slot >= MAX) 0 else slots[slot].estado

    private fun larguraLogica(): Float = NvPlayer.larguraLogicaPublica()
    private fun alturaLogica(): Float = NvPlayer.alturaLogicaPublica()
}
