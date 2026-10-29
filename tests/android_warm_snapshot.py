"""Run real warm snapshot methods on JVM with PixelCopy/View doubles, offline."""
from pathlib import Path
import os, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
s = (root/'android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt').read_text()
def method(marker):
    start=s.index(marker); b=s.index('{',start); end=b+1; depth=1
    while depth:
        depth += (s[end]=='{')-(s[end]=='}'); end+=1
    return s[start:end].replace('@JvmStatic ', '')
methods='\n'.join(method(x) for x in ['fun morno(', 'private fun esconderQuadro(', 'private fun esquecerQuadro(', 'fun quadro(', 'private fun mostrarQuadro('])
fixture=r'''
object SystemClock { var now=100L; fun elapsedRealtime()=now }
object CacheSessao { const val LIMITE_MS=10800000L }
object CacheMidia { fun descartar(s:String) {} }
object android { object graphics { object Color { const val BLACK=0 } } }
class Bitmap(val width:Int,val height:Int) {
 var recycled=false; fun recycle(){recycled=true}
 object Config { const val ARGB_8888=1 }
 companion object { fun createBitmap(w:Int,h:Int,c:Int)=Bitmap(w,h) }
}
class FrameLayout {
 class LayoutParams(w:Int,h:Int) { companion object { const val MATCH_PARENT=-1 } }
 val children=mutableListOf<ImageView>()
 fun addView(v:ImageView,p:LayoutParams){v.parent=this;children.add(v)}
 fun removeView(v:ImageView){children.remove(v)}
}
class ImageView(a:Any) {
 var parent:Any?=null; var scaleType=0; var isFocusable=false
 object ScaleType { const val FIT_CENTER=1 }
 fun setBackgroundColor(c:Int){}; fun setImageBitmap(b:Bitmap){}
}
class Surface { var isValid=true }
class Holder { val surface=Surface() }
class SurfaceView { val holder=Holder() }
class Handler { fun post(f:()->Unit){f()} }
object PixelCopy {
 const val SUCCESS=0
 var pending:((Int)->Unit)?=null
 fun request(s:SurfaceView,b:Bitmap,f:(Int)->Unit,h:Handler){pending=f}
 fun finish(r:Int){val f=pending!!;pending=null;f(r)}
}
object Fixture {
 val principal=Handler(); var mornoLigado=true
 var quadroBitmap:Bitmap?=null; var quadroTitulo=""; var quadroUrl=""; var quadroAte=0L
 var quadroPedido=0; var quadroView:ImageView?=null; var quadroVisivel=false; var capturando=false
 var tituloAberto="A"; var urlAtual="url-A"; var quadroVisto=true
 var superficie:SurfaceView?=SurfaceView(); var videoW=1920; var videoH=1080
 var activity:Any?=Any(); var camada:FrameLayout?=FrameLayout()
 METHODS
 fun show(u:String,t:String){mostrarQuadro(u,t)}
 fun firstFrame(){esconderQuadro()}
}
fun main() {
 val f=Fixture
 f.quadro(1); check(f.capturando && f.quadroBitmap==null)
 f.quadro(1); PixelCopy.finish(PixelCopy.SUCCESS)
 val b=f.quadroBitmap!!; check(b.width==480 && b.height==270)
 f.show("trailer",""); check(f.quadroBitmap===b && !f.quadroVisivel)
 f.quadro(2); check(f.quadro(3)==1 && f.camada!!.children.size==1)
 f.firstFrame(); check(f.quadro(3)==0 && f.camada!!.children.isEmpty())
 f.show("url-B","B"); check(f.quadroBitmap==null && b.recycled)
 f.quadro(1); f.quadro(0); PixelCopy.finish(PixelCopy.SUCCESS); check(f.quadroBitmap==null)
 f.quadro(1); PixelCopy.finish(1); check(f.quadroBitmap==null)
 f.quadro(1); PixelCopy.finish(0); SystemClock.now=f.quadroAte
 f.show("url-A","A"); check(f.quadroBitmap==null && !f.quadroVisivel)
 f.quadro(1); PixelCopy.finish(0); f.morno(0); check(f.quadroBitmap==null)
 println("PASS snapshot: 480px capture, trailer preserves, resume shows, first frame hides, other title/profile/off/expiry invalidate, late/failing capture ignored")
}
'''.replace('METHODS',methods)
cache=Path.home()/'.gradle/caches/modules-2/files-2.1'
compiler=list(cache.glob('org.jetbrains.kotlin/*/2.0.21/*/*.jar'))+list(cache.glob('org.jetbrains.intellij.deps/trove4j/*/*/*.jar'))+list(cache.glob('org.jetbrains/annotations/13.0/*/*.jar'))
compiler += list(cache.glob('org.jetbrains.kotlinx/kotlinx-coroutines-core-jvm/*/*/*.jar'))
stdlib=next(cache.glob('org.jetbrains.kotlin/kotlin-stdlib/2.0.21/*/*.jar'))
java=os.environ.get('NUVIO_TEST_JAVA') or str(next((Path.home()/'.local/jdks').glob('jdk-17*/Contents/Home/bin/java')))
with tempfile.TemporaryDirectory(prefix='nv-warm-snapshot-',dir=os.environ.get('TMPDIR')) as d:
    d=Path(d); kt=d/'Snapshot.kt'; kt.write_text(fixture)
    subprocess.run([java,'-cp',os.pathsep.join(map(str,compiler)),'org.jetbrains.kotlin.cli.jvm.K2JVMCompiler','-no-reflect','-no-stdlib','-classpath',str(stdlib),'-d',str(d/'classes'),str(kt)],check=True)
    subprocess.run([java,'-cp',os.pathsep.join([str(d/'classes'),str(stdlib)]),'SnapshotKt'],check=True)
# First real frame must route to the actual hide method, after the generation guard.
start=s.index('override fun onRenderedFirstFrame()'); part=s[start:s.index('override fun onVideoSizeChanged',start)]
assert part.index('if (!atual(minha)) return') < part.index('esconderQuadro()') < part.index('ev(EV_PRIMEIRO_QUADRO)')
