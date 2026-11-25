package space.nuvio.nativelegacy

import android.graphics.Bitmap
import android.graphics.SurfaceTexture
import android.opengl.EGL14
import android.opengl.EGLConfig
import android.opengl.EGLContext
import android.opengl.EGLDisplay
import android.opengl.EGLSurface
import android.opengl.GLES11Ext
import android.opengl.GLES20
import android.os.Handler
import android.os.HandlerThread
import android.os.SystemClock
import android.util.Log
import android.view.Surface
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

// GPU STEP OF THE DIRECT (PLAYING-FILE) THUMBNAILS (MiniaturaMotor.quadroDireto).
//
// The hardware decoder renders into a SurfaceTexture; ONE draw scales that frame into a 480x270 pbuffer (letterboxed,
// the SurfaceTexture transform carries the decoder's crop and rotation) and only those 480x270 pixels are read back.
// The decoder still decodes every source pixel (a 4K frame is a 4K decode); what this saves is the CPU copy and
// resize of a 24 MB P010 frame. PQ (HDR10 / Dolby Vision with an HDR10 base layer) is tone mapped in the shader with
// the same Hable curve as the CPU path (MiniaturaMotor.pqLut), after BT.2020 -> BT.709.
//
// All GL runs on its own thread (one EGL context, never current anywhere else); callers block on it.
class GpuMiniatura(val largura: Int = MiniaturaMotor.TW, val altura: Int = MiniaturaMotor.TH) {
    private val TW = largura
    private val TH = altura
    companion object {
        private const val TAG = "nuvio-miniatura"

        private const val VS = """
attribute vec2 aPos;
uniform mat4 uTex;
uniform vec4 uPasso;   // footprint of one output pixel in texture space: (dx.xy, dy.xy)
varying vec2 vTex;
varying vec2 vDx;
varying vec2 vDy;
void main() {
    vec2 t = aPos * 0.5 + 0.5;
    vTex = (uTex * vec4(t, 0.0, 1.0)).xy;
    vDx = uPasso.xy; vDy = uPasso.zw;
    gl_Position = vec4(aPos.x, -aPos.y, 0.0, 1.0);   // drawn upside down: glReadPixels then gives rows top first
}"""

        // 4x4 bilinear taps over the output pixel's footprint (~8x8 source pixels at 4K -> 480): no aliasing shimmer.
        private const val FS = """
#extension GL_OES_EGL_image_external : require
precision highp float;
uniform samplerExternalOES uImg;
uniform float uHdr;
varying vec2 vTex;
varying vec2 vDx;
varying vec2 vDy;
vec3 pqLinear(vec3 e) {   // SMPTE ST 2084 EOTF, in nits
    vec3 p = pow(max(e, 0.0), vec3(1.0 / 78.84375));
    return 10000.0 * pow(max(p - 0.8359375, 0.0) / (18.8515625 - 18.6875 * p), vec3(1.0 / 0.1593017578125));
}
vec3 hable(vec3 x) {
    return ((x * (0.15 * x + 0.05) + 0.004) / (x * (0.15 * x + 0.5) + 0.06)) - 0.02 / 0.3;
}
void main() {
    vec3 c = vec3(0.0);
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) {
        vec2 o = vDx * ((float(i) + 0.5) / 4.0 - 0.5) + vDy * ((float(j) + 0.5) / 4.0 - 0.5);
        vec3 s = texture2D(uImg, vTex + o).rgb;
        if (uHdr > 0.5) s = pqLinear(s);
        c += s;
    }
    c /= 16.0;
    if (uHdr > 0.5) {
        c = mat3(1.6605, -0.1246, -0.0182, -0.5876, 1.1329, -0.1006, -0.0728, -0.0083, 1.1187) * c;
        c = max(c, 0.0);
        c = hable(c / 100.0 * 2.0) / hable(vec3(11.2));
        c = pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2));
    }
    gl_FragColor = vec4(c, 1.0);
}"""
    }

    private val fio = HandlerThread("nv-thumb-gl").apply { start() }
    private val gl = Handler(fio.looper)
    private var dpy: EGLDisplay = EGL14.EGL_NO_DISPLAY
    private var ctx: EGLContext = EGL14.EGL_NO_CONTEXT
    private var sup: EGLSurface = EGL14.EGL_NO_SURFACE
    private var tex = 0; private var prog = 0
    private var aPos = 0; private var uTex = 0; private var uPasso = 0; private var uHdr = 0; private var uImg = 0
    private var st: SurfaceTexture? = null
    var surface: Surface? = null; private set
    private val pix: ByteBuffer = ByteBuffer.allocateDirect(TW * TH * 4).order(ByteOrder.nativeOrder())
    val bmp: Bitmap = Bitmap.createBitmap(TW, TH, Bitmap.Config.ARGB_8888)
    // Read only after capturar returns, before the next capture. No extra Java pixel array.
    fun pixelsProntos(): ByteBuffer = pix.duplicate().apply { position(0) }
    private val quad: FloatBuffer = ByteBuffer.allocateDirect(8 * 4).order(ByteOrder.nativeOrder()).asFloatBuffer()
        .apply { put(floatArrayOf(-1f, -1f, 1f, -1f, -1f, 1f, 1f, 1f)); position(0) }
    private val mtx = FloatArray(16)
    private val disponivel = java.util.concurrent.Semaphore(0)
    @Volatile var ok = false; private set

    private fun <T> noFio(ms: Long, f: () -> T): T? {
        val l = CountDownLatch(1)
        var r: T? = null
        gl.post { try { r = f() } catch (e: Exception) { Log.w(TAG, "gpu: ${e.javaClass.simpleName}: ${e.message?.take(80)}") }; l.countDown() }
        return if (l.await(ms, TimeUnit.MILLISECONDS)) r else null
    }

    fun iniciar(): Boolean = noFio(3000) {
        dpy = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY)
        val v = IntArray(2)
        if (!EGL14.eglInitialize(dpy, v, 0, v, 1)) return@noFio false
        val cfgs = arrayOfNulls<EGLConfig>(1); val n = IntArray(1)
        val attr = intArrayOf(EGL14.EGL_RED_SIZE, 8, EGL14.EGL_GREEN_SIZE, 8, EGL14.EGL_BLUE_SIZE, 8, EGL14.EGL_ALPHA_SIZE, 8,
            EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT, EGL14.EGL_SURFACE_TYPE, EGL14.EGL_PBUFFER_BIT, EGL14.EGL_NONE)
        if (!EGL14.eglChooseConfig(dpy, attr, 0, cfgs, 0, 1, n, 0) || n[0] < 1) return@noFio false
        ctx = EGL14.eglCreateContext(dpy, cfgs[0], EGL14.EGL_NO_CONTEXT, intArrayOf(EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE), 0)
        sup = EGL14.eglCreatePbufferSurface(dpy, cfgs[0], intArrayOf(EGL14.EGL_WIDTH, TW, EGL14.EGL_HEIGHT, TH, EGL14.EGL_NONE), 0)
        if (ctx == EGL14.EGL_NO_CONTEXT || sup == EGL14.EGL_NO_SURFACE || !EGL14.eglMakeCurrent(dpy, sup, sup, ctx)) return@noFio false
        prog = programa() ?: return@noFio false
        aPos = GLES20.glGetAttribLocation(prog, "aPos"); uTex = GLES20.glGetUniformLocation(prog, "uTex")
        uPasso = GLES20.glGetUniformLocation(prog, "uPasso"); uHdr = GLES20.glGetUniformLocation(prog, "uHdr")
        uImg = GLES20.glGetUniformLocation(prog, "uImg")
        val t = IntArray(1); GLES20.glGenTextures(1, t, 0); tex = t[0]
        GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, tex)
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR)
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR)
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE)
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE)
        val s = SurfaceTexture(tex)
        s.setOnFrameAvailableListener({ disponivel.release() }, gl)
        st = s; surface = Surface(s)
        ok = true
        true
    } ?: false

    private fun programa(): Int? {
        fun sh(tipo: Int, src: String): Int {
            val s = GLES20.glCreateShader(tipo)
            GLES20.glShaderSource(s, src); GLES20.glCompileShader(s)
            val r = IntArray(1); GLES20.glGetShaderiv(s, GLES20.GL_COMPILE_STATUS, r, 0)
            if (r[0] == 0) { Log.w(TAG, "gpu shader: ${GLES20.glGetShaderInfoLog(s).take(200)}"); GLES20.glDeleteShader(s); return 0 }
            return s
        }
        val v = sh(GLES20.GL_VERTEX_SHADER, VS); val f = sh(GLES20.GL_FRAGMENT_SHADER, FS)
        if (v == 0 || f == 0) return null
        val p = GLES20.glCreateProgram()
        GLES20.glAttachShader(p, v); GLES20.glAttachShader(p, f); GLES20.glLinkProgram(p)
        val r = IntArray(1); GLES20.glGetProgramiv(p, GLES20.GL_LINK_STATUS, r, 0)
        return if (r[0] == 0) null else p
    }

    // Frames rendered but never consumed (a timed-out call) are dropped before a new decode.
    fun descartarPendentes() { noFio(500) { while (disponivel.tryAcquire()) st?.updateTexImage() } }

    // Waits for the frame whose presentation time is ptsUs, draws it letterboxed into 480x270 and reads it back into
    // bmp. srcW/srcH = the visible picture (after crop, rotation applied). false = no such frame in time (stale frames
    // with other timestamps are consumed and ignored).
    var msGpu = 0L
    fun capturar(ptsUs: Long, srcW: Int, srcH: Int, hdr: Boolean, prazoMs: Long): Boolean {
        val fim = SystemClock.elapsedRealtime() + prazoMs
        while (true) {
            val resta = fim - SystemClock.elapsedRealtime()
            if (resta <= 0 || !disponivel.tryAcquire(resta, TimeUnit.MILLISECONDS)) return false
            val r = noFio(1000) {
                val s = st ?: return@noFio false
                val t0 = SystemClock.elapsedRealtime()
                s.updateTexImage()
                if (s.timestamp / 1000 != ptsUs) return@noFio false     // an older frame: not this one
                s.getTransformMatrix(mtx)
                val dw: Int; val dh: Int
                if (srcW.toLong() * TH >= srcH.toLong() * TW) { dw = TW; dh = ((TW.toLong() * srcH / srcW).toInt() / 2 * 2).coerceIn(2, TH) }
                else { dh = TH; dw = ((TH.toLong() * srcW / srcH).toInt() / 2 * 2).coerceIn(2, TW) }
                GLES20.glViewport(0, 0, TW, TH)
                GLES20.glClearColor(0f, 0f, 0f, 1f); GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT)
                GLES20.glViewport((TW - dw) / 2, (TH - dh) / 2, dw, dh)
                GLES20.glUseProgram(prog)
                GLES20.glActiveTexture(GLES20.GL_TEXTURE0)
                GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, tex)
                GLES20.glUniform1i(uImg, 0)
                GLES20.glUniformMatrix4fv(uTex, 1, false, mtx, 0)
                // one output pixel = 1/dw of the unit square, through the transform (scale part only)
                GLES20.glUniform4f(uPasso, mtx[0] / dw, mtx[1] / dw, mtx[4] / dh, mtx[5] / dh)
                GLES20.glUniform1f(uHdr, if (hdr) 1f else 0f)
                GLES20.glEnableVertexAttribArray(aPos)
                GLES20.glVertexAttribPointer(aPos, 2, GLES20.GL_FLOAT, false, 0, quad)
                GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4)
                pix.position(0)
                GLES20.glReadPixels(0, 0, TW, TH, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, pix)
                pix.position(0)
                bmp.copyPixelsFromBuffer(pix)
                msGpu += SystemClock.elapsedRealtime() - t0
                GLES20.glGetError() == GLES20.GL_NO_ERROR
            }
            if (r == true) return true
            if (r == null) return false
        }
    }

    fun soltar() {
        noFio(2000) {
            surface?.release(); st?.release()
            if (prog != 0) GLES20.glDeleteProgram(prog)
            if (tex != 0) GLES20.glDeleteTextures(1, intArrayOf(tex), 0)
            if (dpy != EGL14.EGL_NO_DISPLAY) {
                EGL14.eglMakeCurrent(dpy, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT)
                if (sup != EGL14.EGL_NO_SURFACE) EGL14.eglDestroySurface(dpy, sup)
                if (ctx != EGL14.EGL_NO_CONTEXT) EGL14.eglDestroyContext(dpy, ctx)
                EGL14.eglReleaseThread()
            }
            surface = null; st = null; ok = false
        }
        fio.quitSafely()
        bmp.recycle()
    }
}
