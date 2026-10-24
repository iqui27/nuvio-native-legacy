package space.nuvio.nativelegacy

import android.Manifest
import android.app.UiModeManager
import android.content.Context
import android.content.ComponentName
import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.graphics.Color
import android.graphics.PixelFormat
import android.graphics.drawable.GradientDrawable
import android.net.Uri
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.LinkProperties
import android.os.Build
import android.os.Bundle
import android.os.Process
import android.os.SystemClock
import android.provider.Settings
import android.speech.RecognitionListener
import android.speech.RecognizerIntent
import android.speech.SpeechRecognizer
import android.text.Editable
import android.text.InputFilter
import android.text.InputType
import android.text.TextWatcher
import android.util.Log
import android.system.Os
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.Gravity
import android.view.SurfaceHolder
import android.view.View
import android.view.ViewGroup
import android.view.WindowInsets
import android.view.WindowManager
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.RelativeLayout
import androidx.core.content.FileProvider
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsControllerCompat
import org.libsdl.app.SDLActivity
import java.io.File
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

// Ponte entre o Android e o nucleo C (libmain.so): prepara ambiente e arquivos
// ANTES do SDL subir, traduz teclas de controle remoto e poe a camada de video
// do ExoPlayer atras da superficie GLES do SDL.
class NuvioActivity : SDLActivity() {
    companion object {
        // Vive enquanto o processo vive: ver o comeco de onCreate.
        @Volatile private var jaCriada = false
    }


    // SDL2 e compartilhada; SDL2_image e SDL2_ttf entram estaticas na libmain.
    override fun getLibraries(): Array<String> = arrayOf("SDL2", "main")

    // Bounds pass allocates no pixels; subsampling happens inside the codec.
    fun decodeArtwork(bytes: ByteArray, width: Int, original: IntArray): android.graphics.Bitmap? {
        val options = android.graphics.BitmapFactory.Options()
        options.inJustDecodeBounds = true
        android.graphics.BitmapFactory.decodeByteArray(bytes, 0, bytes.size, options)
        if (options.outWidth <= 0 || options.outHeight <= 0 || width <= 0) return null
        original[0] = options.outWidth
        original[1] = options.outHeight
        var sample = 1
        while (options.outWidth / (sample * 2) >= width) sample *= 2
        // Reject pathological aspect ratios before allocating codec pixels.
        val pixels = ((options.outWidth.toLong() + sample - 1) / sample) *
            ((options.outHeight.toLong() + sample - 1) / sample)
        if (pixels > 2L * 1024 * 1024) return null
        options.inSampleSize = sample
        options.inJustDecodeBounds = false
        options.inScaled = false
        options.inPremultiplied = false
        options.inPreferredConfig = android.graphics.Bitmap.Config.ARGB_8888
        return try {
            android.graphics.BitmapFactory.decodeByteArray(bytes, 0, bytes.size, options)
        } catch (_: OutOfMemoryError) { null }
    }

    private var camadaVideo: FrameLayout? = null
    private var touchInsetLeft = 0
    private var touchInsetTop = 0
    private var touchInsetRight = 0
    @Volatile private var interfaceMobile = false
    @Volatile private var playerTelaCheiaTouch = false
    private var superficieTvW = 0
    private var superficieTvH = 0
    private var cutoutModoTv = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT
    private var entradaModoTv = WindowManager.LayoutParams.SOFT_INPUT_ADJUST_UNSPECIFIED

    // Called by the SDL thread after loading the device setting and on changes.
    // Keep the same Activity/SDL thread: native global state cannot be recreated.
    fun interfaceModo(mobile: Boolean) {
        interfaceMobile = mobile
        runOnUiThread {
            if (mBrokenLibraries || interfaceMobile != mobile) return@runOnUiThread
            fecharCampo("X")
            if (mobile) mSurface.holder.setSizeFromLayout()
            else if (superficieTvW > 0 && superficieTvH > 0)
                mSurface.holder.setFixedSize(superficieTvW, superficieTvH)
            else mSurface.holder.setSizeFromLayout()
            aplicarOrientacaoTouch()
            aplicarViewportModo()
            aplicarCampoModo()
            NvPlayer.interfaceModo(mobile)
            mLayout.requestLayout()
            mSurface.requestFocus()
            Log.i("nuvio", "[interface] modo=${if (mobile) "mobile" else "tv"}")
        }
    }

    private fun aplicarOrientacaoTouch() {
        val orientacao = when (InterfaceMode.orientation(interfaceMobile, playerTelaCheiaTouch)) {
            InterfaceMode.Orientation.LANDSCAPE -> ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
            InterfaceMode.Orientation.SENSOR_LANDSCAPE -> ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            InterfaceMode.Orientation.FULL_USER -> ActivityInfo.SCREEN_ORIENTATION_FULL_USER
        }
        if (requestedOrientation != orientacao) requestedOrientation = orientacao
    }

    // Called by the SDL thread only for the full-screen player. Trailer,
    // retained-video and guide/PiP surfaces keep the browsing orientation.
    fun orientarPlayer(telaCheia: Boolean) {
        playerTelaCheiaTouch = telaCheia
        runOnUiThread { aplicarOrientacaoTouch() }
    }

    // A fixed SDL window chooses orientation from its initial width/height.
    // Keep its later callbacks from overriding the selected interface policy.
    override fun setOrientationBis(w: Int, h: Int, resizable: Boolean, hint: String) {
        runOnUiThread { aplicarOrientacaoTouch() }
    }

    private fun telaCheiaTouch() {
        if (!interfaceMobile) return
        WindowCompat.setDecorFitsSystemWindows(window, false)
        window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN)
        if (Build.VERSION.SDK_INT >= 28) {
            val modo = if (Build.VERSION.SDK_INT >= 30)
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
            else WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            if (window.attributes.layoutInDisplayCutoutMode != modo) {
                window.attributes = window.attributes.apply { layoutInDisplayCutoutMode = modo }
            }
        }
        WindowCompat.getInsetsController(window, window.decorView).apply {
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            hide(WindowInsetsCompat.Type.systemBars())
        }
    }

    private val registroDownload by lazy { RegistroDownload(this) }
    private val fontImport by lazy { FontImport(this) }
    fun importarFonte(): Boolean = fontImport.abrir()
    fun proximaFonteImportada(): String? = fontImport.proximo()

    // Chamado pelo fio SDL, sem ler arquivos nem abrir a tela naquele fio.
    fun baixarRegistro(arquivo: String, sucesso: String, falha: String) {
        runOnUiThread { registroDownload.abrir(arquivo, sucesso, falha) }
    }

    // Vigia do arranque (#266, ArranqueVigia.kt) e o que ele le do C (android.c).
    private var vigia: ArranqueVigia? = null
    external fun nativeEtapa(): String
    external fun nativeQuadros(): Long
    external fun nativeRecUrl(): String
    fun estadoSdl() = "thread=${if (mSDLThread != null) "sim" else "nao"} foco=$mHasFocus " +
        "retomada=$mIsResumedCalled superficie=${mSurface?.mIsSurfaceReady == true}"

    override fun onSaveInstanceState(outState: Bundle) {
        registroDownload.salvar(outState)
        fontImport.salvar(outState)
        super.onSaveInstanceState(outState)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        registroDownload.restaurar(savedInstanceState)
        fontImport.restaurar(savedInstanceState)
        // SEGUNDO onCreate NO MESMO PROCESSO = SEGUNDO main() DO C (o
        // SDLActivity.initialize() zera mSDLThread e o SDL sobe outro fio de
        // main). O nucleo C guarda estado global (mutex, fios, caches) que nao
        // nasce de novo. Medido no D1 (1.7.0, Xiaomi MiTV Android 14, 4 vezes
        // em 6 h): o log da sessao que caiu tem so [tv], [dados] e o "[tex] teto
        // ... pedido em Ajustes" — linha que so sai com o mutex do cache de
        // textura de uma sessao anterior ainda no lugar — e morre em "FORTIFY:
        // pthread_mutex_lock called on a destroyed mutex". Em vez disso, abre
        // o app num processo novo, como na primeira vez.
        if (jaCriada) {
            super.onCreate(savedInstanceState)
            reabrirEmProcessoNovo()
            return
        }
        jaCriada = true
        prepararAmbiente()
        aplicarOrientacaoTouch()
        super.onCreate(savedInstanceState)
        if (Build.VERSION.SDK_INT >= 28) cutoutModoTv = window.attributes.layoutInDisplayCutoutMode
        entradaModoTv = window.attributes.softInputMode
        aplicarOrientacaoTouch()
        if (!mBrokenLibraries) prepararViewportTouch()
        // Antes de tudo que pode falhar daqui para baixo: o vigia so precisa
        // do fio da interface livre.
        if (!mBrokenLibraries) vigia = ArranqueVigia(this).also { it.iniciar() }
        observarRede()
        entregarDeepLink(intent)

        // SDLActivity.mLayout e um RelativeLayout com a SDLSurface dentro.
        // O video fica no indice 0 (atras); a SDLSurface fica por cima, em
        // formato translucido, e o C desenha alfa 0 no "furo" do player.
        val camada = FrameLayout(this)
        camadaVideo = camada
        mLayout.addView(
            camada, 0,
            ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)
        )
        // SDL adds the SurfaceView WRAP_CONTENT; SurfaceView.onMeasure then
        // resolves to min(setFixedSize, parent), so a buffer smaller than the
        // screen (720p) shrank the view to the top-left. MATCH_PARENT lets the
        // compositor scale the small buffer to the full view.
        mSurface.layoutParams = RelativeLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)
        mSurface.setZOrderMediaOverlay(true)
        mSurface.holder.setFormat(PixelFormat.TRANSLUCENT)
        NvPlayer.iniciar(this, camada)
        NvPlayer.interfaceModo(interfaceMobile)
    }

    // SDL and video fill the window, including the camera cutout edges. Keeping
    // mLayout intact also keeps SDL's touch coordinates relative to the video.
    private fun prepararViewportTouch() {
        val raiz = object : FrameLayout(this) {
            override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
                val r = InterfaceMode.viewport(interfaceMobile, View.MeasureSpec.getSize(widthMeasureSpec),
                    View.MeasureSpec.getSize(heightMeasureSpec))
                val atual = mLayout.layoutParams as FrameLayout.LayoutParams
                if (r != null) {
                    if (atual.width != r.width || atual.height != r.height ||
                        atual.leftMargin != r.left || atual.topMargin != r.top) {
                        atual.width = r.width; atual.height = r.height
                        atual.gravity = Gravity.TOP or Gravity.LEFT
                        atual.leftMargin = r.left; atual.topMargin = r.top
                        Log.i("Nuvio", "touch viewport=${r.left},${r.top},${r.width},${r.height}")
                    }
                } else {
                    atual.width = ViewGroup.LayoutParams.MATCH_PARENT
                    atual.height = ViewGroup.LayoutParams.MATCH_PARENT
                    atual.leftMargin = 0; atual.topMargin = 0
                }
                super.onMeasure(widthMeasureSpec, heightMeasureSpec)
            }
        }.apply { setBackgroundColor(Color.BLACK) }
        (mLayout.parent as? ViewGroup)?.removeView(mLayout)
        raiz.addView(mLayout, FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        ViewCompat.setOnApplyWindowInsetsListener(raiz) { _, ins ->
            if (interfaceMobile) {
                val barras = ins.getInsets(WindowInsetsCompat.Type.systemBars())
                val recorte = ins.getInsetsIgnoringVisibility(WindowInsetsCompat.Type.displayCutout())
                touchInsetLeft = maxOf(barras.left, recorte.left)
                touchInsetTop = maxOf(barras.top, recorte.top)
                touchInsetRight = maxOf(barras.right, recorte.right)
                posicionarCampoTouch()
            }
            ins
        }
        setContentView(raiz)
        ViewCompat.requestApplyInsets(raiz)
        aplicarViewportModo()
    }

    private fun aplicarViewportModo() {
        WindowCompat.setDecorFitsSystemWindows(window, !interfaceMobile)
        window.setSoftInputMode(if (interfaceMobile)
            WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING else entradaModoTv)
        if (!interfaceMobile && Build.VERSION.SDK_INT >= 28) {
            window.attributes = window.attributes.apply { layoutInDisplayCutoutMode = cutoutModoTv }
        }
        telaCheiaTouch()
    }

    private external fun nativeToqueCancelou()

    // LINK EXTERNO (docs/deeplink.md): o C valida; aqui so se limita o tamanho.
    private external fun nativeDeepLink(url: String)

    private fun entregarDeepLink(i: Intent?) {
        if (mBrokenLibraries || i?.action != Intent.ACTION_VIEW) return
        val url = i.dataString ?: return
        if (url.length <= 512) nativeDeepLink(url)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        entregarDeepLink(intent)
    }

    override fun dispatchTouchEvent(ev: MotionEvent): Boolean {
        // SDL turns CANCEL into finger UP. Queue the cancellation first so
        // taking over a system gesture cannot confirm the pending tap/hold.
        if (interfaceMobile && !mBrokenLibraries &&
            ev.actionMasked == MotionEvent.ACTION_CANCEL) nativeToqueCancelou()
        return super.dispatchTouchEvent(ev)
    }

    private fun reabrirEmProcessoNovo() {
        Log.w("Nuvio", "segundo onCreate no mesmo processo: reabrindo em processo novo")
        try {
            packageManager.getLaunchIntentForPackage(packageName)?.let {
                it.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
                startActivity(it)
            }
        } catch (_: Exception) {}
        Process.killProcess(Process.myPid())
    }

    // The default route is identified by Android, not a guessed IP/SSID.
    // A new route, relevant capabilities or link properties invalidate the
    // diagnostic immediately. Values stay process-local and are never logged.
    private external fun nativeRedeAlterou(sequencia: Long, conhecida: Boolean)

    // O sistema avisa quando a memoria aperta (RUNNING_MODERATE/LOW/CRITICAL em
    // primeiro plano, UI_HIDDEN e BACKGROUND fora dele). O nucleo C baixa o
    // teto do cache de texturas por 30 s e despeja arte fria. Um upload chegou
    // a levar 16 s com o low-memory killer ativo. SDLActivity so repassa
    // onLowMemory (o aviso mais forte), nao estes degraus.
    private external fun nativeTrimMemoria(nivel: Int)
    private var trimResumida = false

    override fun onLowMemory() {
        super.onLowMemory()
        try { nativeTrimMemoria(80) } catch (_: UnsatisfiedLinkError) { }
    }

    override fun onTrimMemory(level: Int) {
        super.onTrimMemory(level)
        // Background trim levels describe lifecycle, not foreground pressure.
        // Guard both paths: SDL also forwards a strongest-level memory event.
        if (trimResumida && level >= 20) return
        if (level >= 5) SDLActivity.nativeLowMemory()
        // Antes de a libmain.so carregar nao ha para quem avisar.
        try { nativeTrimMemoria(level) } catch (_: UnsatisfiedLinkError) { }
    }
    private val redeTrava = Any()
    private var redeSequencia = 0L
    private var redeFechada = false
    private var redeAtual: Network? = null
    private var redeCap: Int? = null
    private var redeLink: LinkProperties? = null
    private var redeBloqueada = false
    private var redeMonitor: ConnectivityManager? = null
    private var redeCallback: ConnectivityManager.NetworkCallback? = null

    private fun publicarRede() {
        // Called under redeTrava, including destruction and failure paths.
        if (redeSequencia == Long.MAX_VALUE) {
            // An exhausted observer cannot represent any more identities.
            PassivoMedidor.redeGlobal = 0L
            nativeRedeAlterou(redeSequencia, false)
            return
        }
        redeSequencia++
        val conhecida = !redeFechada && redeAtual != null && redeLink != null &&
            redeCap?.let { (it and 3) == 3 } == true && !redeBloqueada
        // The passive StreamFit meter tags its windows with the same epoch.
        PassivoMedidor.redeGlobal = if (conhecida) redeSequencia else 0L
        nativeRedeAlterou(redeSequencia, conhecida)
    }

    private fun observarRede() {
        val cm = getSystemService(Context.CONNECTIVITY_SERVICE) as? ConnectivityManager ?: return
        val cb = object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(network: Network) = synchronized(redeTrava) {
                if (redeFechada) return@synchronized
                redeAtual = network; redeCap = null; redeLink = null; redeBloqueada = false
                publicarRede()
            }
            override fun onCapabilitiesChanged(network: Network, cap: NetworkCapabilities) = synchronized(redeTrava) {
                if (redeFechada || network != redeAtual) return@synchronized
                var chave = 0
                if (cap.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)) chave = chave or 1
                if (cap.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)) chave = chave or 2
                if (cap.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED)) chave = chave or 4
                if (cap.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_VPN)) chave = chave or 8
                // Transport changes matter; changing an estimated bandwidth
                // alone is not a new network and must not erase real evidence.
                for (i in 0..7) if (cap.hasTransport(i)) chave = chave or (1 shl (i + 4))
                if (redeCap != chave) { redeCap = chave; publicarRede() }
            }
            override fun onLinkPropertiesChanged(network: Network, link: LinkProperties) = synchronized(redeTrava) {
                if (redeFechada || network != redeAtual) return@synchronized
                // The callback delivers its own parcelled snapshot. Keep it
                // read-only; the LinkProperties copy constructor is hidden.
                if (redeLink != link) { redeLink = link; publicarRede() }
            }
            override fun onBlockedStatusChanged(network: Network, blocked: Boolean) = synchronized(redeTrava) {
                if (redeFechada || network != redeAtual) return@synchronized
                if (redeBloqueada != blocked) { redeBloqueada = blocked; publicarRede() }
            }
            override fun onLost(network: Network) = synchronized(redeTrava) {
                if (redeFechada || network != redeAtual) return@synchronized
                redeAtual = null; redeCap = null; redeLink = null; publicarRede()
            }
        }
        try {
            cm.registerDefaultNetworkCallback(cb)
            redeMonitor = cm; redeCallback = cb
        } catch (_: Exception) {
            synchronized(redeTrava) { redeFechada = true; redeAtual = null; redeCap = null; redeLink = null; publicarRede() }
        }
    }

    private fun fecharRede() {
        synchronized(redeTrava) { redeFechada = true; redeAtual = null; redeLink = null; publicarRede() }
        redeCallback?.let { cb -> try { redeMonitor?.unregisterNetworkCallback(cb) } catch (_: Exception) {} }
        redeCallback = null; redeMonitor = null
    }

    // Chamado pelo C (android_pedir_superficie), do fio do SDL, antes de criar a
    // janela: fixa o buffer da SDLSurface em w x h e espera a superficie nova
    // chegar (ate 2 s). Devolve true se ela veio nesse tamanho.
    // SDL calls this before EGL surface creation; request a ten-bit UI layer.
    // ExoPlayer retains its separate SurfaceView and video color handling.
    private var hdrCapabilitiesLogged = false
    private var hdrSurfaceRequested = false

    fun pedirHdrInterface(ativa: Boolean): Boolean {
        if (Build.VERSION.SDK_INT < 26) return false
        val done = CountDownLatch(1)
        var ok = false
        runOnUiThread {
            try {
                if (!ativa || (Build.VERSION.SDK_INT >= 34 && display?.isHdr == true)) {
                    window.colorMode = if (ativa) ActivityInfo.COLOR_MODE_HDR else ActivityInfo.COLOR_MODE_DEFAULT
                    if (ativa) {
                        hdrSurfaceRequested = true
                        mSurface.holder.setFormat(PixelFormat.RGBA_1010102)
                        if (Build.VERSION.SDK_INT >= 35) {
                            mSurface.setDesiredHdrHeadroom(1000.0f / 203.0f)
                            window.setDesiredHdrHeadroom(1000.0f / 203.0f)
                        }
                    } else if (hdrSurfaceRequested) {
                        mSurface.holder.setFormat(PixelFormat.TRANSLUCENT)
                        if (Build.VERSION.SDK_INT >= 35) {
                            mSurface.setDesiredHdrHeadroom(0.0f)
                            window.setDesiredHdrHeadroom(0.0f)
                        }
                        hdrSurfaceRequested = false
                    }
                    ok = !ativa || window.colorMode == ActivityInfo.COLOR_MODE_HDR
                }
            } catch (_: Exception) {
                ok = false
            } finally {
                if (ativa && !hdrCapabilitiesLogged) {
                    hdrCapabilitiesLogged = true
                    try {
                        @Suppress("DEPRECATION")
                        val screen = windowManager.defaultDisplay
                        val caps = screen.hdrCapabilities
                        Log.i("Nuvio", "[hdr-ui] cap=display types=${caps.supportedHdrTypes.joinToString(",")} maxLuminance=${caps.desiredMaxLuminance} mode=${screen.mode} isHdr=${if (Build.VERSION.SDK_INT >= 34) screen.isHdr else "unavailable"} colorMode=${window.colorMode} wideColorGamut=${screen.isWideColorGamut}")
                    } catch (error: Exception) {
                        Log.w("Nuvio", "[hdr-ui] cap=error ${error.javaClass.simpleName}: ${error.message}")
                    }
                }
                done.countDown()
            }
        }
        done.await()
        return ok
    }

    fun pedirSuperficie(w: Int, h: Int): Boolean {
        // SDL normalizes touches against the holder buffer dimensions. On a
        // phone that buffer must follow the viewport's physical view size.
        if (interfaceMobile) return false
        superficieTvW = w; superficieTvH = h
        val chegou = CountDownLatch(1)
        var ok = false
        runOnUiThread {
            val holder = mSurface.holder
            val atual = holder.surfaceFrame
            if (atual.width() == w && atual.height() == h) { ok = true; chegou.countDown(); return@runOnUiThread }
            holder.addCallback(object : SurfaceHolder.Callback {
                override fun surfaceCreated(hd: SurfaceHolder) {}
                override fun surfaceDestroyed(hd: SurfaceHolder) {}
                override fun surfaceChanged(hd: SurfaceHolder, f: Int, ww: Int, hh: Int) {
                    if (ww == w && hh == h) { ok = true; hd.removeCallback(this); chegou.countDown() }
                }
            })
            holder.setFixedSize(w, h)
        }
        chegou.await(2, TimeUnit.SECONDS)
        return ok
    }

    // DESPEDIDA (dados_despedida_ler, dados.c). Escondido, o app pode ser morto
    // pelo Android sem saida limpa; o arquivo diz ao proximo arranque que isso
    // nao foi queda, e o modo seguro nao desfaz ajustes por causa dela.
    private fun despedida() = File(filesDir, "dados/despedida.txt")

    @Volatile private var instalando = false

    @Suppress("DEPRECATION")
    private fun apkVersion(file: File): Long? = try {
        packageManager.getPackageArchiveInfo(file.path, 0)?.let {
            if (Build.VERSION.SDK_INT >= 28) it.longVersionCode else it.versionCode.toLong()
        }
    } catch (_: Exception) { null }

    @Suppress("DEPRECATION")
    private fun installedVersion(): Long {
        val info = packageManager.getPackageInfo(packageName, 0)
        return if (Build.VERSION.SDK_INT >= 28) info.longVersionCode else info.versionCode.toLong()
    }

    fun prepararApk(caminho: String): Boolean {
        if (instalando) return false
        val dir = File(cacheDir, "regenerable/atualizacao")
        val file = File(caminho)
        if (file.parentFile != dir) return false
        ApkCache.prune(dir, installedVersion(), file.name, ::apkVersion)
        return true
    }

    // Chamado pelo C (android_instalar_apk, atualizacao.c), do fio do SDL.
    // 1 = instalador aberto, 2 = falta a permissao "instalar apps desta fonte"
    // (abre a tela dela), 0 = falhou. Nao bloqueia: o resultado e do sistema.
    fun instalarApk(caminho: String): Int {
        return try {
            val arq = File(caminho)
            if (!arq.isFile) return 0
            if (Build.VERSION.SDK_INT >= 26 && !packageManager.canRequestPackageInstalls()) {
                startActivity(
                    Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:$packageName"))
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                )
                return 2
            }
            val uri = FileProvider.getUriForFile(this, "$packageName.atualizacao", arq)
            instalando = true
            startActivity(
                Intent(Intent.ACTION_VIEW)
                    .setDataAndType(uri, "application/vnd.android.package-archive")
                    .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_ACTIVITY_NEW_TASK)
            )
            1
        } catch (e: Exception) {
            instalando = false
            0
        }
    }

    // HTTP PELA PILHA DO ANDROID (#266/#332, src/android.c android_http).
    // Reserva da libcurl do APK para pedidos simples (conta, login, Supabase):
    // em Shield/BRAVIA a init da libcurl nao voltava e o login esperava 20 s
    // para falhar. Chamado de fios de rede do C (nunca do fio de UI). Volta
    // [status 4 bytes big-endian][Retry-After seconds 4 bytes][body].
    fun httpPedir(metodo: String, url: String, cabs: String, corpo: ByteArray?, prazoMs: Int): ByteArray {
        var con: java.net.HttpURLConnection? = null
        return try {
            con = java.net.URL(url).openConnection() as java.net.HttpURLConnection
            val prazo = if (prazoMs > 0) prazoMs else 20000
            con.connectTimeout = minOf(prazo, 10000)
            con.readTimeout = prazo
            con.instanceFollowRedirects = true
            con.useCaches = false
            con.requestMethod = metodo
            con.setRequestProperty("User-Agent", "Nuvio/1.0 (webOS)")
            for (l in cabs.split('\n')) {
                val i = l.indexOf(':')
                if (i > 0) con.setRequestProperty(l.substring(0, i).trim(), l.substring(i + 1).trim())
            }
            if (corpo != null) {
                con.doOutput = true
                con.setFixedLengthStreamingMode(corpo.size)
                con.outputStream.use { it.write(corpo) }
            }
            val st = con.responseCode
            val ent = if (st >= 400) con.errorStream else con.inputStream
            val dados = ent?.use { it.readBytes() } ?: ByteArray(0)
            val raw = con.getHeaderField("Retry-After")?.trim().orEmpty()
            val retry = if (raw.isNotEmpty() && raw.all { it in '0'..'9' }) {
                (raw.toLongOrNull() ?: Long.MAX_VALUE).coerceAtMost(Int.MAX_VALUE.toLong()).toInt()
            } else {
                val until = con.getHeaderFieldDate("Retry-After", 0)
                ((until - System.currentTimeMillis() + 999) / 1000).coerceIn(0, Int.MAX_VALUE.toLong()).toInt()
            }
            val r = ByteArray(8 + dados.size)
            r[0] = (st ushr 24).toByte(); r[1] = (st ushr 16).toByte()
            r[2] = (st ushr 8).toByte(); r[3] = st.toByte()
            r[4] = (retry ushr 24).toByte(); r[5] = (retry ushr 16).toByte()
            r[6] = (retry ushr 8).toByte(); r[7] = retry.toByte()
            System.arraycopy(dados, 0, r, 8, dados.size)
            r
        } catch (e: Throwable) {
            val m = (e.javaClass.simpleName + ": " + (e.message ?: "")).take(180).toByteArray()
            val r = ByteArray(8 + m.size)
            System.arraycopy(m, 0, r, 8, m.size)
            r
        } finally {
            try { con?.disconnect() } catch (_: Throwable) {}
        }
    }

    // ONDE ASSISTIR (src/ondever.c), chamados pelo C do fio do SDL.
    //
    // Apps que aparecem no inicio da TV, "pacote\tnome" por linha. Leanback
    // primeiro (e o que a TV mostra), LAUNCHER como reserva para app de
    // celular instalado na TV. Precisa do <queries> do manifesto: sem ele o
    // Android 11+ esconde os outros apps e a lista volta vazia.
    fun listarApps(): String? {
        return try {
            val pm = packageManager
            val vistos = LinkedHashMap<String, String>()
            for (cat in arrayOf(Intent.CATEGORY_LEANBACK_LAUNCHER, Intent.CATEGORY_LAUNCHER)) {
                val q = Intent(Intent.ACTION_MAIN).addCategory(cat)
                for (r in pm.queryIntentActivities(q, 0)) {
                    val pkg = r.activityInfo?.packageName ?: continue
                    if (pkg == packageName || vistos.containsKey(pkg)) continue
                    vistos[pkg] = r.loadLabel(pm).toString().replace('\t', ' ').replace('\n', ' ')
                }
            }
            vistos.entries.joinToString("\n") { it.key + "\t" + it.value }
        } catch (e: Exception) { null }
    }

    fun abrirApp(pacote: String): Boolean {
        return try {
            val i = packageManager.getLeanbackLaunchIntentForPackage(pacote)
                ?: packageManager.getLaunchIntentForPackage(pacote) ?: return false
            startActivity(i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
            true
        } catch (e: Exception) { false }
    }

    // Players de video instalados que abrem um stream http(s): "pacote\tnome" por linha.
    fun listarPlayers(): String? = try {
        val pm = packageManager
        val q = Intent(Intent.ACTION_VIEW).setDataAndType(Uri.parse("https://example.com/v.mp4"), "video/*")
        pm.queryIntentActivities(q, 0).mapNotNull { r ->
            val pkg = r.activityInfo?.packageName
            if (pkg == null || pkg == packageName) null
            else pkg to r.loadLabel(pm).toString().replace('\t', ' ').replace('\n', ' ')
        }.distinctBy { it.first }.sortedBy { it.second.lowercase() }
            .joinToString("\n") { it.first + "\t" + it.second }
    } catch (e: Exception) { null }

    // #501: entrega o stream a outro player; `pacote` vazio = o sistema escolhe.
    // Sem app que aceite, devolve false (o nativo toca no interno). A URL nao vai para o log.
    fun abrirExterno(url: String, titulo: String, pacote: String): Boolean = try {
        val i = Intent(Intent.ACTION_VIEW).setDataAndType(Uri.parse(url), "video/*")
            .putExtra("title", titulo).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        if (pacote.isNotEmpty()) i.setPackage(pacote)
        startActivity(i)
        true
    } catch (e: Exception) { false }

    // A loja (Google Play) na pagina do app; sem pacote, a busca pelo nome.
    fun abrirLoja(pacote: String, nome: String): Boolean {
        val alvo = if (pacote.isNotEmpty()) "market://details?id=$pacote"
                   else "market://search?q=" + Uri.encode(nome)
        return try {
            startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(alvo)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
            true
        } catch (e: Exception) {
            try {
                val web = if (pacote.isNotEmpty()) "https://play.google.com/store/apps/details?id=$pacote"
                          else "https://play.google.com/store/search?q=" + Uri.encode(nome)
                startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(web)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
                true
            } catch (e2: Exception) { false }
        }
    }

    // So apaga ao VOLTAR com o processo vivo: no primeiro onStart quem le (e
    // apaga) a despedida da sessao anterior e o C, no arranque.
    private var jaComecou = false

    override fun onStart() {
        super.onStart()
        if (jaComecou) {
            val f = despedida()
            if (f.exists()) {
                val t = f.readText()
                // "fim" so sobra aqui se o instalador foi cancelado (o C a grava
                // antes de entregar o APK): desfaz, senao uma queda futura
                // pareceria saida limpa.
                if (t.startsWith("oculto") || (instalando && t.startsWith("fim"))) f.delete()
            }
            instalando = false
        }
        jaComecou = true
    }

    private external fun nativeCheckpoint()

    private fun checkpoint() {
        if (!mBrokenLibraries) try { nativeCheckpoint() } catch (_: UnsatisfiedLinkError) {}
    }

    override fun onStop() {
        checkpoint()
        val ini = SystemClock.elapsedRealtime()
        try {
            val f = despedida()
            if (!(f.exists() && f.readText().startsWith("fim"))) f.writeText("oculto\n")
        } catch (_: Exception) {}
        // Instalador ou tela de voz por cima NAO e sair do app: trocar o icone
        // ali fecharia a tarefa embaixo deles (ver aplicarIconePendente).
        if (!instalando && !esperandoTela && !isChangingConfigurations &&
            !registroDownload.escolhendoArquivo && !fontImport.escolhendoArquivo) aplicarIconePendente()
        super.onStop()
        Log.i("nuvio", "[android] parada: ${SystemClock.elapsedRealtime() - ini} ms no fio da interface")
    }

    override fun onResume() {
        trimResumida = true
        super.onResume()
        aplicarOrientacaoTouch()
        telaCheiaTouch()
        vigia?.frente(true)
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) telaCheiaTouch()
    }

    // [android] pausa / parada: quanto o fio da interface ficou em cada passo
    // do ciclo de vida (ANR ao apertar a tecla de ajustes da TV tocando, TCL,
    // 2.0.3). Direto no logcat: o printf do C passa por um pipe e pode esperar.
    override fun onPause() {
        trimResumida = false
        checkpoint()
        val ini = SystemClock.elapsedRealtime()
        vigia?.frente(false)
        NvPlayer.pausarPeloSistema()
        val msPlayer = SystemClock.elapsedRealtime() - ini
        super.onPause()
        Log.i("nuvio", "[android] pausa: ${SystemClock.elapsedRealtime() - ini} ms no fio da interface (player $msPlayer ms)")
    }

    override fun onDestroy() {
        registroDownload.fechar()
        fontImport.fechar()
        vigia?.parar()
        fecharRede()
        NvPlayer.encerrar()
        val saindo = isFinishing
        super.onDestroy()
        // O nucleo C guarda estado global: sair do app e matar o processo, para
        // a proxima abertura nascer limpa (o SDL ja pediu finish quando o main voltou).
        if (saindo) { aplicarIconePendente(); Process.killProcess(Process.myPid()) }
    }

    // Variaveis que o C le (contrato do porte Android). Tem de rodar antes do
    // super.onCreate: o SDL abre o main numa thread logo depois.
    fun limparCache() {
        runOnUiThread {
            NvPlayer.parar()
            CacheMidia.descartar("clear cache")
            // Session release runs on the cache executor before deleting its files.
            CacheMidia.limparTudo(cacheDir)
        }
        File(cacheDir, "logs/nuvio-anterior.log").delete()
        File(cacheDir, "engine/p2p").deleteRecursively()
    }

    private fun prepararAmbiente() {
        val dados = File(filesDir, "dados").apply { mkdirs() }
        val uiMode = (getSystemService(Context.UI_MODE_SERVICE) as? UiModeManager)?.currentModeType
        val automaticMobile = InterfaceMode.automaticMobile(
            uiMode == Configuration.UI_MODE_TYPE_TELEVISION,
            packageManager.hasSystemFeature(PackageManager.FEATURE_TELEVISION),
            packageManager.hasSystemFeature(PackageManager.FEATURE_LEANBACK))
        val preferencia = InterfaceMode.readPreference(File(dados, "ajustes.txt"))
        interfaceMobile = InterfaceMode.resolve(preferencia, automaticMobile)
        val res = File(filesDir, "res")
        extrairAssets(res)
        val versao = try {
            packageManager.getPackageInfo(packageName, 0).versionName ?: ""
        } catch (e: Exception) { "" }
        fun env(k: String, v: String) = Os.setenv(k, v, true)
        env("NUVIO_INTERFACE_AUTO", if (automaticMobile) "1" else "0")
        env("NUVIO_DADOS", dados.path)
        val regenerable = File(cacheDir, "regenerable").apply { mkdirs() }
        ApkCache.prune(File(regenerable, "atualizacao"), installedVersion(), null, ::apkVersion)
        val p2pCache = File(cacheDir, "engine").apply { mkdirs() }
        env("NUVIO_CACHE", regenerable.path)
        env("NUVIO_CACHE_ROOT", cacheDir.path)
        env("TMPDIR", regenerable.path)
        env("NUVIO_P2P_CACHE", p2pCache.path)
        val logs = File(cacheDir, "logs").apply { mkdirs() }
        File(dados, "atualizacao").deleteRecursively()
        File(dados, "p2p").deleteRecursively()
        File(dados, "nuvio.log").delete()
        File(dados, "nuvio-anterior.log").delete()
        // Scoped to signing key/user/device; stable across app reinstall.
        val deviceId = Settings.Secure.getString(contentResolver, Settings.Secure.ANDROID_ID)
        if (!deviceId.isNullOrBlank()) env("NUVIO_DEVICE_ID", "android:$deviceId")
        env("NUVIO_LOG", File(logs, "nuvio.log").path)
        env("NUVIO_LOG_ANTERIOR", File(logs, "nuvio-anterior.log").path)
        env("HOME", dados.path)
        env("NUVIO_ARTE", File(res, "art").path)
        env("NUVIO_LOCALE", java.util.Locale.getDefault().toLanguageTag())
        env("NUVIO_TV_INFO", "${Build.MANUFACTURER} ${Build.MODEL}|${Build.VERSION.SDK_INT}|${Build.VERSION.RELEASE}|$versao")
        // SDL may expose only the 1080p graphics plane on a 4K television.
        // Mode dimensions describe the physical output, including supported UHD modes.
        try {
            if (Build.VERSION.SDK_INT >= 23) {
                @Suppress("DEPRECATION")
                val panel = windowManager.defaultDisplay.supportedModes.maxByOrNull {
                    it.physicalWidth.toLong() * it.physicalHeight
                }
                if (panel != null) {
                    env("NUVIO_PANEL_SIZE", "${panel.physicalWidth}x${panel.physicalHeight}")
                    env("NUVIO_PANEL_SOURCE", "Android Display.Mode supportedModes")
                }
            }
        } catch (_: Exception) { /* Keep the SDL fallback when display info is unavailable. */ }
        // Real metrics include physical DPI; use the same pixel space as that DPI.
        // Display.Mode.physicalWidth/Height are pixel counts, not inches.
        val panelInches = try {
            val metrics = android.util.DisplayMetrics()
            @Suppress("DEPRECATION")
            windowManager.defaultDisplay.getRealMetrics(metrics)
            if (metrics.xdpi > 0 && metrics.ydpi > 0)
                kotlin.math.hypot((metrics.widthPixels / metrics.xdpi).toDouble(),
                    (metrics.heightPixels / metrics.ydpi).toDouble())
            else 0.0
        } catch (_: Exception) { 0.0 }
        env("NUVIO_PANEL_INCHES", if (panelInches.isFinite() && panelInches in 20.0..120.0)
            panelInches.toString() else "0")
        env("NUVIO_SAIDA_ANTERIOR", saidaAnterior())
        // Decoders de video do aparelho, para o log do D1 (#318): a deteccao de
        // MStar (NvPlayer.mstar) so aparecia no logcat e nunca soubemos se casou.
        env("NUVIO_DECODERS", decodersDeVideo())
        // Onde o crash nativo anterior aconteceu (Tombstone.kt); android.c imprime.
        env("NUVIO_QUEDA_ANDROID", quedaAnterior.joinToString("\n"))
    }

    private var quedaAnterior: List<String> = emptyList()

    private fun decodersDeVideo(): String = try {
        android.media.MediaCodecList(android.media.MediaCodecList.REGULAR_CODECS).codecInfos
            .filter { !it.isEncoder && it.supportedTypes.any { t -> t.startsWith("video/") } }
            .joinToString(",") { it.name }.take(400)
    } catch (e: Throwable) { "" }

    // POR QUE O PROCESSO ANTERIOR MORREU, segundo o proprio Android (11+). O log
    // do app nao ve ANR, crash nativo nem o low memory killer: a sessao
    // simplesmente para. Medido no D1 (1.7.0, TCL Android 14): 6 sessoes que
    // terminam em FPS=60 na home, sem linha nenhuma. android.c imprime isto.
    private fun saidaAnterior(): String {
        if (Build.VERSION.SDK_INT < 30) return ""
        return try {
            val am = getSystemService(Context.ACTIVITY_SERVICE) as android.app.ActivityManager
            val r = am.getHistoricalProcessExitReasons(packageName, 0, 1).firstOrNull() ?: return ""
            val nome = when (r.reason) {
                android.app.ApplicationExitInfo.REASON_ANR -> "ANR"
                android.app.ApplicationExitInfo.REASON_CRASH -> "crash-java"
                android.app.ApplicationExitInfo.REASON_CRASH_NATIVE -> "crash-nativo"
                android.app.ApplicationExitInfo.REASON_LOW_MEMORY -> "pouca-memoria"
                android.app.ApplicationExitInfo.REASON_SIGNALED -> "sinal"
                android.app.ApplicationExitInfo.REASON_EXIT_SELF -> "saiu-sozinho"
                android.app.ApplicationExitInfo.REASON_USER_REQUESTED -> "usuario"
                android.app.ApplicationExitInfo.REASON_USER_STOPPED -> "usuario-parou"
                android.app.ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE -> "recurso-excessivo"
                android.app.ApplicationExitInfo.REASON_PERMISSION_CHANGE -> "permissao"
                android.app.ApplicationExitInfo.REASON_DEPENDENCY_DIED -> "dependencia"
                android.app.ApplicationExitInfo.REASON_INITIALIZATION_FAILURE -> "falha-inicio"
                android.app.ApplicationExitInfo.REASON_OTHER -> "outro"
                else -> "desconhecido"
            }
            if (r.reason == android.app.ApplicationExitInfo.REASON_CRASH_NATIVE) quedaAnterior = Tombstone.doExitInfo(r)
            val ha = (System.currentTimeMillis() - r.timestamp) / 1000
            val desc = (r.description ?: "").replace('\n', ' ').take(120)
            "motivo=$nome(${r.reason}) status=${r.status} importancia=${r.importance} " +
                "pss=${r.pss / 1024}MB rss=${r.rss / 1024}MB ha=${ha}s desc=$desc"
        } catch (_: Exception) { "" }
    }

    // Copia assets art/ e fonts/ para filesDir/res, uma vez por INSTALACAO.
    // So o versionCode nao bastava: um APK novo com a MESMA versao (teste na
    // TCL, 02/10/2026) achava a marca igual e ficava com a arte velha — o icone
    // novo do celular (aj_smartphone.png) nao existia em filesDir e o botao saiu
    // vazio. lastUpdateTime muda a cada `adb install -r`/atualizacao.
    private fun extrairAssets(res: File) {
        val marca = File(res, ".versao")
        val atual = try {
            packageManager.getPackageInfo(packageName, 0).let {
                @Suppress("DEPRECATION") "${it.versionCode}-${it.lastUpdateTime}"
            }
        } catch (e: Exception) { "0" }
        if (marca.exists() && marca.readText() == atual && File(res, "art").isDirectory) return
        res.deleteRecursively()
        res.mkdirs()
        for (pasta in arrayOf("art", "fonts")) copiarAsset(pasta, File(res, pasta))
        marca.writeText(atual)
    }

    private fun copiarAsset(caminho: String, destino: File) {
        val filhos = assets.list(caminho) ?: emptyArray()
        if (filhos.isEmpty()) {
            // folha: arquivo (ou pasta vazia, que assets.list tambem devolve vazia)
            try {
                destino.parentFile?.mkdirs()
                assets.open(caminho).use { i -> destino.outputStream().use { o -> i.copyTo(o, 64 * 1024) } }
            } catch (e: java.io.FileNotFoundException) {
                destino.mkdirs()
            }
            return
        }
        destino.mkdirs()
        for (f in filhos) copiarAsset("$caminho/$f", File(destino, f))
    }

    // TEXTO DO SISTEMA (src/sistexto.h, android_st_* em src/android.c): o
    // teclado do sistema e a voz para os campos do app. O C chama do fio do
    // SDL; o trabalho e no fio da interface, e o que acontece volta numa fila
    // de strings (formato em src/sistexto.c) que o C drena por quadro.
    private val eventos = ConcurrentLinkedQueue<String>()
    fun proximoEvento(): String? = eventos.poll()

    // #205: app-specific dir of every volume (index 0 = primary), one per line,
    // empty line for an unavailable one. Writable with no storage permission.
    fun volumesExternos(): String =
        getExternalFilesDirs(null).joinToString("\n") { it?.absolutePath ?: "" }

    private fun log(m: String) = Log.i("nuvio", "[texto] $m")

    // --- Teclado do sistema -------------------------------------------------
    // Na TV, um EditText de 1 px invisivel recebe o foco e chama o IME. No
    // modo Mobile, o mesmo campo fica visivel acima do teclado. O texto
    // INTEIRO (com a composicao em andamento) vai ao C a cada mudanca: o campo
    // desenhado pelo app e um espelho deste.
    private var campo: CampoIme? = null
    private var campoAberto = false
    private var campoImeVisto = false
    private var ignorarMudanca = false
    private var teclaDesceuNoCampo = 0

    private inner class CampoIme(ctx: Context) : EditText(ctx) {
        // Voltar com o IME aberto: fecha o IME e devolve o foco ao app (o texto
        // que ja veio fica). Consome as duas metades, senao o UP cairia no app.
        override fun onKeyPreIme(keyCode: Int, event: KeyEvent): Boolean {
            if (campoAberto && keyCode == KeyEvent.KEYCODE_BACK) {
                if (event.action == KeyEvent.ACTION_UP) fecharCampo("X")
                return true
            }
            return super.onKeyPreIme(keyCode, event)
        }
    }

    private fun criarCampo(): CampoIme {
        campo?.let { return it }
        val c = CampoIme(this)
        c.alpha = if (interfaceMobile) 1f else 0f
        c.isFocusable = true
        c.isFocusableInTouchMode = true
        c.setSingleLine(true)
        c.inputType = InputType.TYPE_CLASS_TEXT
        c.imeOptions = EditorInfo.IME_ACTION_DONE or EditorInfo.IME_FLAG_NO_EXTRACT_UI or
            EditorInfo.IME_FLAG_NO_FULLSCREEN
        c.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, a: Int, b: Int, d: Int) {}
            override fun onTextChanged(s: CharSequence?, a: Int, b: Int, d: Int) {}
            override fun afterTextChanged(s: Editable?) {
                if (!ignorarMudanca && campoAberto) eventos.add("T" + (s?.toString() ?: ""))
            }
        })
        c.setOnEditorActionListener { v, _, _ -> if (campoAberto) fecharCampo("D" + v.text); true }
        // Tecla que o IME NAO consumiu e chegou ao campo: o IME esta fechado (ou a
        // pessoa saiu dele pela borda). Devolve o foco ao app em vez de prender.
        // So vale a tecla que DESCEU aqui: o OK que abriu o teclado desce no SDL e
        // SOBE no campo (MEDIDO na TCL: abriu e fechou em 15 ms).
        c.setOnKeyListener { v, code, ev ->
            if (!campoAberto) return@setOnKeyListener false
            when (code) {
                KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_DOWN,
                KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.KEYCODE_ENTER -> {
                    if (ev.action == KeyEvent.ACTION_DOWN) teclaDesceuNoCampo = code
                    else if (ev.action == KeyEvent.ACTION_UP && teclaDesceuNoCampo == code) {
                        teclaDesceuNoCampo = 0
                        fecharCampo("D" + (v as EditText).text)
                    }
                    true
                }
                else -> false
            }
        }
        // IME escondido por fora (o botao dele de fechar): Android 11+ diz pelos insets.
        c.setOnApplyWindowInsetsListener { v, ins ->
            if (Build.VERSION.SDK_INT >= 30 && campoAberto) {
                val vis = ins.isVisible(WindowInsets.Type.ime())
                if (vis) campoImeVisto = true
                else if (campoImeVisto) fecharCampo("X")
            }
            v.onApplyWindowInsets(ins)
        }
        // Style once; switching mode changes bounds and visibility below.
        run {
            fun dp(value: Int) = (value * resources.displayMetrics.density + 0.5f).toInt()
            c.textSize = 18f
            c.setTextColor(Color.WHITE)
            c.setPadding(dp(16), dp(8), dp(16), dp(8))
            c.minHeight = dp(56)
            c.background = GradientDrawable().apply {
                setColor(Color.rgb(28, 31, 38))
                cornerRadius = dp(12).toFloat()
                setStroke(dp(1).coerceAtLeast(1), Color.rgb(125, 170, 255))
            }
            c.elevation = dp(8).toFloat()
            c.visibility = View.GONE
        }
        mLayout.addView(c, RelativeLayout.LayoutParams(1, 1))
        campo = c
        aplicarCampoModo()
        posicionarCampoTouch()
        return c
    }

    private fun aplicarCampoModo() {
        val c = campo ?: return
        c.alpha = if (interfaceMobile) 1f else 0f
        c.visibility = if (interfaceMobile && !campoAberto) View.GONE else View.VISIBLE
        c.layoutParams = if (interfaceMobile) RelativeLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply {
            addRule(RelativeLayout.ALIGN_PARENT_TOP)
        } else RelativeLayout.LayoutParams(1, 1)
        posicionarCampoTouch()
    }

    private fun posicionarCampoTouch() {
        if (!interfaceMobile) return
        val c = campo ?: return
        val p = c.layoutParams as? RelativeLayout.LayoutParams ?: return
        fun dp(value: Int) = (value * resources.displayMetrics.density + 0.5f).toInt()
        val left = dp(16) + touchInsetLeft
        val right = dp(16) + touchInsetRight
        val top = dp(12) + touchInsetTop
        if (p.leftMargin == left && p.rightMargin == right && p.topMargin == top) return
        p.leftMargin = left; p.rightMargin = right; p.topMargin = top
        c.layoutParams = p
    }

    private fun fecharCampo(ev: String?) {
        val c = campo ?: return
        if (!campoAberto) return
        campoAberto = false
        campoImeVisto = false
        (getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager)
            .hideSoftInputFromWindow(c.windowToken, 0)
        if (interfaceMobile) c.visibility = View.GONE
        c.clearFocus()
        mSurface?.requestFocus()
        telaCheiaTouch()
        if (ev != null) { log("teclado fechou (${ev[0]})"); eventos.add(ev) }
    }

    // Chamado pelo C (android_st_teclado). Nao bloqueia.
    // `pedido` = max | tipo << 16 (tipo: 0 texto, 1 e-mail, 2 senha; ST_IME_* em
    // src/sistexto.h). Senha: teclado sem sugestao e sem aprender o texto.
    fun abrirTeclado(inicial: String, pedido: Int): Boolean {
        val max = pedido and 0xFFFF
        val tipo = (pedido ushr 16) and 0xFF
        runOnUiThread {
            val c = criarCampo()
            ignorarMudanca = true
            c.inputType = when (tipo) {
                1 -> InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS
                2 -> InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
                else -> InputType.TYPE_CLASS_TEXT
            }
            c.setSingleLine(true)
            // E-mail: a tecla do teclado diz "Proximo" (o app abre a senha em
            // seguida); senha e texto: "Concluido". As duas fecham o campo.
            val flags = EditorInfo.IME_FLAG_NO_EXTRACT_UI or EditorInfo.IME_FLAG_NO_FULLSCREEN
            c.imeOptions = (if (tipo == 1) EditorInfo.IME_ACTION_NEXT else EditorInfo.IME_ACTION_DONE) or flags
            c.filters = arrayOf(InputFilter.LengthFilter(if (max > 0) max else 400))
            c.setText(inicial)
            c.setSelection(c.text.length)
            ignorarMudanca = false
            campoAberto = true
            campoImeVisto = false
            teclaDesceuNoCampo = 0
            if (interfaceMobile) {
                c.visibility = View.VISIBLE
                c.bringToFront()
            }
            c.requestFocus()
            val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
            val ok = imm.showSoftInput(c, InputMethodManager.SHOW_IMPLICIT)
            log("teclado pedido (showSoftInput=$ok)")
            // Logo depois do requestFocus o IME as vezes ainda nao se ligou ao
            // campo e o showSoftInput volta false: tenta de novo num instante.
            if (!ok) c.postDelayed({ if (campoAberto) imm.showSoftInput(c, InputMethodManager.SHOW_IMPLICIT) }, 150)
        }
        return true
    }

    // Chamado pelo C (android_st_fechar): fecha teclado e voz SEM evento (quem
    // fechou foi o C, ele ja sabe).
    fun fecharEntrada(): Boolean {
        runOnUiThread {
            fecharCampo(null)
            reconhecedor?.let { try { it.cancel(); it.destroy() } catch (_: Exception) {} }
            reconhecedor = null
        }
        return true
    }

    // --- Voz ------------------------------------------------------------------
    // Degraus: SpeechRecognizer dentro do app (com RECORD_AUDIO pedida no
    // primeiro uso) -> tela de voz do sistema (RecognizerIntent) -> teclado do
    // sistema, que tem o proprio microfone.
    private var reconhecedor: SpeechRecognizer? = null
    private var idiomaVoz = ""
    private var ultimoNivel = -100
    // ICONE DO APP (iconeapp_aplicar_plataforma, src/iconeapp.c). Um
    // activity-alias ".Icone_<id>" por icone; so um ligado de cada vez.
    //
    // A TROCA NAO E NA HORA, e sim quando o app sai da frente (onStop) ou fecha.
    // MEDIDO no emulador Android TV (API 34): desligar o alias pelo qual a tarefa
    // foi aberta FECHA a tarefa e mata o processo cerca de 1 s depois, mesmo com
    // DONT_KILL_APP — o app sumia da tela no meio dos Ajustes. Na saida isso nao
    // importa (a pessoa ja saiu, e a proxima abertura nasce limpa, como sempre).
    //
    // 0 = ja estava assim, 2 = agendada para a saida, -1 = id desconhecido.
    private val ICONES = arrayOf("original", "fenix", "nverde", "tvlaranja", "npixel",
                                 "tricolor", "arco", "tvviva", "cluberetro", "arcaden")
    @Volatile private var iconePendente: String? = null
    @Volatile private var esperandoTela = false   // voz do sistema por cima

    // O pacote instalado muda no debug; a classe do alias continua no namespace.
    private fun compIcone(i: String) = ComponentName(this, "space.nuvio.nativelegacy.Icone_$i")
    private fun iconeLigado(i: String): Boolean {
        val st = packageManager.getComponentEnabledSetting(compIcone(i))
        // DEFAULT = o que o manifest diz: so o original nasce ligado.
        return if (st == PackageManager.COMPONENT_ENABLED_STATE_DEFAULT) i == "original"
               else st == PackageManager.COMPONENT_ENABLED_STATE_ENABLED
    }

    fun trocarIcone(id: String): Int {
        if (id !in ICONES) return -1
        return try {
            if (iconeLigado(id) && ICONES.none { it != id && iconeLigado(it) }) {
                iconePendente = null; 0
            } else { iconePendente = id; 2 }
        } catch (e: Exception) { -1 }
    }

    // Liga o novo ANTES de desligar os outros: na ordem inversa havia um
    // instante sem nenhuma entrada no launcher. EFEITO COLATERAL, de qualquer
    // launcher: ele tira a entrada velha e poe a nova; leva de um a varios
    // segundos e a entrada pode mudar de lugar (fim da lista; um favorito do
    // Android TV ou atalho fixado pode sumir e precisar ser fixado de novo).
    private fun aplicarIconePendente() {
        val id = iconePendente ?: return
        iconePendente = null
        try {
            val pm = packageManager
            pm.setComponentEnabledSetting(compIcone(id),
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP)
            for (i in ICONES) if (i != id && iconeLigado(i))
                pm.setComponentEnabledSetting(compIcone(i),
                    PackageManager.COMPONENT_ENABLED_STATE_DISABLED, PackageManager.DONT_KILL_APP)
        } catch (_: Exception) {}
    }

    private val PEDIDO_DITADO = 4711
    private val PEDIDO_MIC = 4712

    // Chamado pelo C (android_st_ditar). Nao bloqueia.
    fun ditar(idioma: String): Boolean {
        runOnUiThread { iniciarVoz(idioma) }
        return true
    }

    private fun iniciarVoz(idioma: String) {
        idiomaVoz = idioma
        fecharCampo(null)
        val disponivel = SpeechRecognizer.isRecognitionAvailable(this)
        val permitido = checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED
        log("voz: reconhecedor=${if (disponivel) "sim" else "nao"} permissao=${if (permitido) "sim" else "nao"} idioma=$idioma")
        eventos.add("Ireconhecedor=${if (disponivel) "sim" else "nao"} permissao=${if (permitido) "sim" else "nao"}")
        if (!disponivel) { vozDoSistema("semvoz"); return }
        if (!permitido) {
            eventos.add("Spermissao")
            requestPermissions(arrayOf(Manifest.permission.RECORD_AUDIO), PEDIDO_MIC)
            return
        }
        ouvir()
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        if (requestCode == PEDIDO_MIC) {
            val ok = grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED
            log("voz: permissao ${if (ok) "concedida" else "negada"}")
            eventos.add("Ipermissao=${if (ok) "concedida" else "negada"}")
            if (ok) ouvir() else vozDoSistema("negada")
            return
        }
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
    }

    private fun intentVoz(): Intent =
        Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH)
            .putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_WEB_SEARCH)
            .putExtra(RecognizerIntent.EXTRA_MAX_RESULTS, 1)
            .apply { if (idiomaVoz.isNotEmpty()) putExtra(RecognizerIntent.EXTRA_LANGUAGE, idiomaVoz) }

    private fun ouvir() {
        reconhecedor?.let { try { it.destroy() } catch (_: Exception) {} }
        val r = try { SpeechRecognizer.createSpeechRecognizer(this) } catch (e: Exception) { null }
        if (r == null) { vozDoSistema("semvoz"); return }
        reconhecedor = r
        ultimoNivel = -100
        r.setRecognitionListener(object : RecognitionListener {
            override fun onReadyForSpeech(params: Bundle?) { eventos.add("Souvindo") }
            override fun onBeginningOfSpeech() {}
            override fun onRmsChanged(rmsdB: Float) {
                // -2..10 dB e a faixa que o reconhecedor do Google entrega.
                val n = (((rmsdB + 2f) / 12f).coerceIn(0f, 1f) * 100f).toInt()
                if (kotlin.math.abs(n - ultimoNivel) >= 4) { ultimoNivel = n; eventos.add("R$n") }
            }
            override fun onBufferReceived(buffer: ByteArray?) {}
            override fun onEndOfSpeech() {}
            override fun onError(error: Int) {
                log("voice recognition failed: SpeechRecognizer error=$error")
                reconhecedor = null
                try { r.destroy() } catch (_: Exception) {}
                when (error) {
                    SpeechRecognizer.ERROR_INSUFFICIENT_PERMISSIONS -> vozDoSistema("negada")
                    SpeechRecognizer.ERROR_NO_MATCH, SpeechRecognizer.ERROR_SPEECH_TIMEOUT -> eventos.add("Enada")
                    SpeechRecognizer.ERROR_RECOGNIZER_BUSY -> eventos.add("Eocupado")
                    // 12/13: idioma nao suportado/indisponivel no reconhecedor
                    // do aparelho; a tela de voz do sistema pode ter outro.
                    12, 13 -> vozDoSistema("idioma")
                    else -> eventos.add("Eerro:$error")
                }
            }
            override fun onResults(results: Bundle?) {
                reconhecedor = null
                try { r.destroy() } catch (_: Exception) {}
                val t = results?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)?.firstOrNull()
                eventos.add(if (t.isNullOrBlank()) "Enada" else "V$t")
            }
            override fun onPartialResults(partialResults: Bundle?) {
                val t = partialResults?.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)?.firstOrNull()
                if (!t.isNullOrBlank()) eventos.add("P$t")
            }
            override fun onEvent(eventType: Int, params: Bundle?) {}
        })
        try {
            r.startListening(intentVoz().putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true)
                .putExtra(RecognizerIntent.EXTRA_CALLING_PACKAGE, packageName))
        } catch (e: Exception) {
            log("voice recognition failed: startListening: $e")
            reconhecedor = null
            vozDoSistema("semvoz")
        }
    }

    private fun vozDoSistema(motivo: String) {
        log("voice fallback: " + when (motivo) {
            "negada" -> "microphone permission denied"
            "idioma" -> "recognition language unavailable"
            else -> "in-app recognizer unavailable"
        })
        val i = intentVoz()
        if (i.resolveActivity(packageManager) != null) {
            eventos.add("Ssistema:$motivo")
            @Suppress("DEPRECATION")
            try { esperandoTela = true; startActivityForResult(i, PEDIDO_DITADO); return }
            catch (e: Exception) { esperandoTela = false; log("system voice activity failed: $e") }
        }
        // Nem a tela de voz: o C abre o teclado do sistema (o microfone dele dita).
        log("voice fallback: system voice activity unavailable; opening keyboard")
        eventos.add("Steclado:$motivo")
    }

    @Deprecated("startActivityForResult e o que o SDLActivity (Activity) oferece")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (registroDownload.resultado(requestCode, resultCode, data)) return
        if (fontImport.resultado(requestCode, resultCode, data)) return
        if (requestCode == PEDIDO_DITADO) esperandoTela = false
        if (requestCode == PEDIDO_DITADO) {
            val t = if (resultCode == RESULT_OK)
                data?.getStringArrayListExtra(RecognizerIntent.EXTRA_RESULTS)?.firstOrNull() else null
            eventos.add(if (t.isNullOrBlank()) "Enada" else "V$t")
            return
        }
        @Suppress("DEPRECATION")
        super.onActivityResult(requestCode, resultCode, data)
    }

    // As teclas chegam ao C pelo foco da SDLSurface. Se outra View (ou nenhuma)
    // ficou com ele, o processo segue vivo e o controle nao responde (#318).
    fun devolverFoco() {
        if (campoAberto) return
        val s = mSurface ?: return
        if (!s.isFocused) s.requestFocus()
    }

    // Controle remoto: troca a tecla ANTES do SDL, para cair no SDLK que o app espera.
    override fun dispatchKeyEvent(ev: KeyEvent): Boolean {
        if (ev.action == KeyEvent.ACTION_DOWN && !campoAberto && mSurface?.isFocused == false) {
            Log.w("Nuvio", "foco fora da SDLSurface: devolvendo antes da tecla ${ev.keyCode}")
            devolverFoco()
        }
        // Voltar com o C sem desenhar: o SDL entregaria a tecla a um main()
        // preso e nada aconteceria. O vigia abre a tela com "Sair".
        if (ev.keyCode == KeyEvent.KEYCODE_BACK && ev.action == KeyEvent.ACTION_DOWN &&
            ev.repeatCount == 0 && vigia?.voltar() == true) return true
        val novo = when (ev.keyCode) {
            KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.KEYCODE_NUMPAD_ENTER -> KeyEvent.KEYCODE_ENTER
            KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE, KeyEvent.KEYCODE_MEDIA_PLAY,
            KeyEvent.KEYCODE_MEDIA_PAUSE -> KeyEvent.KEYCODE_BREAK
            KeyEvent.KEYCODE_MEDIA_STOP -> KeyEvent.KEYCODE_BACK
            KeyEvent.KEYCODE_MEDIA_FAST_FORWARD, KeyEvent.KEYCODE_MEDIA_NEXT -> KeyEvent.KEYCODE_DPAD_RIGHT
            KeyEvent.KEYCODE_MEDIA_REWIND, KeyEvent.KEYCODE_MEDIA_PREVIOUS -> KeyEvent.KEYCODE_DPAD_LEFT
            // Guide/Menu open the full TV guide, including during live playback.
            KeyEvent.KEYCODE_GUIDE, KeyEvent.KEYCODE_MENU -> KeyEvent.KEYCODE_F10
            KeyEvent.KEYCODE_PROG_BLUE, KeyEvent.KEYCODE_BOOKMARK -> KeyEvent.KEYCODE_S
            // Info (i) = registro (enviar o log), como a vermelha/verde.
            KeyEvent.KEYCODE_PROG_RED, KeyEvent.KEYCODE_PROG_GREEN,
            KeyEvent.KEYCODE_INFO -> KeyEvent.KEYCODE_F9
            // CH+/CH- vao como F7/F8 e o C decide (main.c): troca de canal com
            // canal na tela; fora disso, CH+ = AZUL e CH- = Spotlight, porque a
            // TCL e a maioria dos controles Android TV nao tem teclas coloridas
            // e o microfone da TCL manda ASSIST (abre o Gemini, nao chega aqui).
            KeyEvent.KEYCODE_CHANNEL_UP -> KeyEvent.KEYCODE_F7
            KeyEvent.KEYCODE_CHANNEL_DOWN -> KeyEvent.KEYCODE_F8
            // SPOTLIGHT (spotlight.h). O botao de microfone da maioria dos
            // controles Android TV manda SEARCH, que o sistema entrega ao app:
            // vira F6 (abrir + ditado). ASSIST e VOICE_ASSIST o sistema NAO
            // entrega (KeyEvent: "not delivered to applications"). A amarela
            // vira F5 (so abrir).
            KeyEvent.KEYCODE_SEARCH -> KeyEvent.KEYCODE_F6
            KeyEvent.KEYCODE_PROG_YELLOW -> KeyEvent.KEYCODE_F5
            else -> return super.dispatchKeyEvent(ev)
        }
        return super.dispatchKeyEvent(
            KeyEvent(
                ev.downTime, ev.eventTime, ev.action, novo, ev.repeatCount, ev.metaState,
                ev.deviceId, ev.scanCode, ev.flags, ev.source
            )
        )
    }
}

