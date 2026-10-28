package space.nuvio.nativelegacy

import android.opengl.EGL14
import android.opengl.GLES20 as GL
import android.util.Log
import java.nio.ByteBuffer
import java.nio.ByteOrder

/** Raw YUV -> Dolby RPU -> SDR conversion. Owned by the calling CPU worker.
 * No decoder, SurfaceTexture or asynchronous access to native buffers. */
internal class DolbyThumbnailGpu {
    private var display = EGL14.EGL_NO_DISPLAY
    private var context = EGL14.EGL_NO_CONTEXT
    private var surface = EGL14.EGL_NO_SURFACE
    private var program = 0
    private var width = 0
    private var height = 0
    private val textures = IntArray(2)
    private val vertices = ByteBuffer.allocateDirect(32).order(ByteOrder.nativeOrder()).asFloatBuffer().apply {
        put(floatArrayOf(-1f,-1f,1f,-1f,-1f,1f,1f,1f)); position(0)
    }
    private fun shader(type: Int, source: String): Int {
        val id=GL.glCreateShader(type); GL.glShaderSource(id,source); GL.glCompileShader(id)
        val status=IntArray(1); GL.glGetShaderiv(id,GL.GL_COMPILE_STATUS,status,0)
        if(status[0]==0) {val why=GL.glGetShaderInfoLog(id);GL.glDeleteShader(id);error(why)}
        return id
    }
    private fun init(w: Int,h: Int) {
        width=w;height=h; display=EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY)
        check(EGL14.eglInitialize(display,IntArray(2),0,IntArray(2),1))
        val configs=arrayOfNulls<android.opengl.EGLConfig>(1)
        check(EGL14.eglChooseConfig(display,intArrayOf(EGL14.EGL_RED_SIZE,8,EGL14.EGL_GREEN_SIZE,8,EGL14.EGL_BLUE_SIZE,8,EGL14.EGL_ALPHA_SIZE,8,EGL14.EGL_RENDERABLE_TYPE,EGL14.EGL_OPENGL_ES2_BIT,EGL14.EGL_SURFACE_TYPE,EGL14.EGL_PBUFFER_BIT,EGL14.EGL_NONE),0,configs,0,1,IntArray(1),0))
        val config=checkNotNull(configs[0])
        context=EGL14.eglCreateContext(display,config,EGL14.EGL_NO_CONTEXT,intArrayOf(EGL14.EGL_CONTEXT_CLIENT_VERSION,2,EGL14.EGL_NONE),0)
        surface=EGL14.eglCreatePbufferSurface(display,config,intArrayOf(EGL14.EGL_WIDTH,w,EGL14.EGL_HEIGHT,h,EGL14.EGL_NONE),0)
        check(context!=EGL14.EGL_NO_CONTEXT && surface!=EGL14.EGL_NO_SURFACE)
        check(EGL14.eglMakeCurrent(display,surface,surface,context))
        check(GL.glGetString(GL.GL_EXTENSIONS)?.contains("GL_OES_texture_float")==true)
        val vs=shader(GL.GL_VERTEX_SHADER,"attribute vec2 a; varying vec2 uv; void main(){uv=a*.5+.5;gl_Position=vec4(a,0.,1.);}")
        val fs=shader(GL.GL_FRAGMENT_SHADER,FRAGMENT)
        try {
            program=GL.glCreateProgram();GL.glAttachShader(program,vs);GL.glAttachShader(program,fs);GL.glLinkProgram(program)
            val ok=IntArray(1);GL.glGetProgramiv(program,GL.GL_LINK_STATUS,ok,0);check(ok[0]!=0){GL.glGetProgramInfoLog(program)}
        } finally {GL.glDeleteShader(vs);GL.glDeleteShader(fs)}
        GL.glGenTextures(2,textures,0)
        for(i in 0..1) {
            GL.glActiveTexture(GL.GL_TEXTURE0+i);GL.glBindTexture(GL.GL_TEXTURE_2D,textures[i])
            GL.glTexParameteri(GL.GL_TEXTURE_2D,GL.GL_TEXTURE_MIN_FILTER,GL.GL_NEAREST)
            GL.glTexParameteri(GL.GL_TEXTURE_2D,GL.GL_TEXTURE_MAG_FILTER,GL.GL_NEAREST)
            GL.glTexParameteri(GL.GL_TEXTURE_2D,GL.GL_TEXTURE_WRAP_S,GL.GL_CLAMP_TO_EDGE)
            GL.glTexParameteri(GL.GL_TEXTURE_2D,GL.GL_TEXTURE_WRAP_T,GL.GL_CLAMP_TO_EDGE)
        }
        // Match the CPU reference's byte rounding, not implementation-dependent dithering.
        GL.glDisable(GL.GL_DITHER)
        Log.i("nuvio-miniatura","[dv5-gpu] raw float YUV shader ready ${w}x$h")
    }
    fun convert(w: Int,h: Int,signal: ByteBuffer,metadata: ByteBuffer,rgba: ByteBuffer): Boolean {
        if(program==0)init(w,h)
        check(width==w && height==h)
        check(EGL14.eglMakeCurrent(display,surface,surface,context))
        GL.glUseProgram(program)
        GL.glPixelStorei(GL.GL_UNPACK_ALIGNMENT,1)
        GL.glActiveTexture(GL.GL_TEXTURE0);GL.glBindTexture(GL.GL_TEXTURE_2D,textures[0])
        signal.position(0);GL.glTexImage2D(GL.GL_TEXTURE_2D,0,GL.GL_RGB,w,h,0,GL.GL_RGB,GL.GL_FLOAT,signal.order(ByteOrder.nativeOrder()).asFloatBuffer())
        GL.glUniform1i(GL.glGetUniformLocation(program,"source"),0)
        GL.glActiveTexture(GL.GL_TEXTURE1);GL.glBindTexture(GL.GL_TEXTURE_2D,textures[1])
        metadata.position(0);GL.glTexImage2D(GL.GL_TEXTURE_2D,0,GL.GL_RGBA,256,1,0,GL.GL_RGBA,GL.GL_FLOAT,metadata.order(ByteOrder.nativeOrder()).asFloatBuffer())
        GL.glUniform1i(GL.glGetUniformLocation(program,"metadata"),1)
        val a=GL.glGetAttribLocation(program,"a");GL.glEnableVertexAttribArray(a)
        vertices.position(0);GL.glVertexAttribPointer(a,2,GL.GL_FLOAT,false,0,vertices)
        GL.glViewport(0,0,w,h);GL.glDrawArrays(GL.GL_TRIANGLE_STRIP,0,4)
        rgba.position(0);GL.glReadPixels(0,0,w,h,GL.GL_RGBA,GL.GL_UNSIGNED_BYTE,rgba)
        val error=GL.glGetError();check(error==GL.GL_NO_ERROR){"GL error $error"}
        return true
    }
    fun close() {
        if(display!=EGL14.EGL_NO_DISPLAY) {
            if(context!=EGL14.EGL_NO_CONTEXT && surface!=EGL14.EGL_NO_SURFACE) {
                EGL14.eglMakeCurrent(display,surface,surface,context)
                if(program!=0)GL.glDeleteProgram(program)
                GL.glDeleteTextures(2,textures,0)
            }
            EGL14.eglMakeCurrent(display,EGL14.EGL_NO_SURFACE,EGL14.EGL_NO_SURFACE,EGL14.EGL_NO_CONTEXT)
            if(surface!=EGL14.EGL_NO_SURFACE)EGL14.eglDestroySurface(display,surface)
            if(context!=EGL14.EGL_NO_CONTEXT)EGL14.eglDestroyContext(display,context)
            EGL14.eglReleaseThread()
        }
        program=0;display=EGL14.EGL_NO_DISPLAY;context=EGL14.EGL_NO_CONTEXT;surface=EGL14.EGL_NO_SURFACE
        // Do not eglTerminate the process-shared default display: other app GL contexts use it.
    }
    companion object {
        private const val FRAGMENT="""
precision highp float;
uniform highp sampler2D source;
uniform highp sampler2D metadata;
varying vec2 uv;
float m(float i) {
    vec4 v=texture2D(metadata,vec2((floor(i/4.)+.5)/256.,.5));
    float k=mod(i,4.); if(k<.5)return v.r;if(k<1.5)return v.g;if(k<2.5)return v.b;return v.a;
}
vec3 row(float i) {return vec3(m(i),m(i+1.),m(i+2.));}
float reshape(float c,vec3 v) {
    float base=32.+c*240., n=m(base), index=0., s=c<.5?v.x:(c<1.5?v.y:v.z);
    for(int j=1;j<8;j++) {if(float(j)<n-1. && s>=m(base+1.+float(j)))index=float(j);}
    float at=base+16.+index*28.;
    if(m(at)<.5)return clamp((m(at+4.)*s+m(at+3.))*s+m(at+2.),0.,1.);
    float value=m(at+5.), order=m(at+1.);
    vec3 powers=v; vec4 cross=vec4(v.x*v.y,v.x*v.z,v.y*v.z,v.x*v.y*v.z), products=cross;
    for(int j=0;j<3;j++) {
        if(float(j)<order) {
            float k=at+6.+float(j)*7.;
            value+=dot(row(k),powers)+dot(vec4(m(k+3.),m(k+4.),m(k+5.),m(k+6.)),products);
            powers*=v;products*=cross;
        }
    }
    return clamp(value,0.,1.);
}
vec3 pq(vec3 x) {
    vec3 p=pow(clamp(x,0.,1.),vec3(1./78.84375));
    return pow(max(p-.8359375,0.)/(18.8515625-18.6875*p),vec3(1./.1593017578125))*100.;
}
float hable(float x) {return ((x*(.15*x+.05)+.004)/(x*(.15*x+.5)+.06))-.02/.3;}
void main() {
    vec3 v=texture2D(source,uv).rgb;
    v=clamp(v,vec3(m(33.),m(273.),m(513.)),vec3(m(33.+m(32.)-1.),m(273.+m(272.)-1.),m(513.+m(512.)-1.)));
    vec3 shaped=vec3(reshape(0.,v),reshape(1.,v),reshape(2.,v))-row(9.);
    vec3 cone=pq(vec3(dot(row(0.),shaped),dot(row(3.),shaped),dot(row(6.),shaped)));
    vec3 rgb=vec3(dot(row(12.),cone),dot(row(15.),cone),dot(row(18.),cone));
    float lum=max(0.,dot(vec3(.2627,.6780,.0593),rgb));
    float scale=lum>1e-8?hable(lum)/(hable(10.)*lum):0.;
    vec3 outputRgb=vec3(dot(vec3(1.6605,-.5876,-.0728),rgb),dot(vec3(-.1246,1.1329,-.0083),rgb),dot(vec3(-.0182,-.1006,1.1187),rgb));
    gl_FragColor=vec4(pow(clamp(outputRgb*scale,0.,1.),vec3(1./2.4)),1.);
}
"""
    }
}
