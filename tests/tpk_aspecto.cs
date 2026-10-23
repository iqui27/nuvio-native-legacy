using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using NuvioTpk;
using Tizen.Multimedia;
class AspectoTest {
    [DllImport("libcapi-media-player.so.0")] static extern int probe(int i);
    [DllImport("libcapi-media-player.so.0")] static extern void fail(int op, int rc);
    static readonly Queue<Action> queue = new Queue<Action>();
    static readonly List<string> log = new List<string>();
    static Video video;
    static void Check(bool ok, string msg) { if (!ok) throw new Exception(msg); Console.WriteLine("ok: " + msg); }
    static void Entry(string name, params object[] args) => ((Delegate)typeof(Video).GetField(name, BindingFlags.NonPublic|BindingFlags.Instance).GetValue(video)).DynamicInvoke(args);
    // Os comandos de sessao rodam no fio do Video, em ordem. Esperar a fila
    // drenar: postar uma consulta de posicao (Tique) e esperar terminar garante
    // que tudo postado antes foi processado (FIFO).
    static void Pump() {
        while(queue.Count > 0) queue.Dequeue()();
        var sw = System.Diagnostics.Stopwatch.StartNew();
        while (sw.ElapsedMilliseconds < 3000) {
            video.Tique();
            var f = typeof(Video).GetField("fio", BindingFlags.NonPublic|BindingFlags.Instance).GetValue(video);
            if (f == null) break;
            var c = f.GetType().GetField("consulta", BindingFlags.Instance|BindingFlags.Public|BindingFlags.NonPublic);
            if ((int)c.GetValue(f) == 0) {
                // Tique posted and completed, or no pending query. Drain UI callbacks.
                while(queue.Count > 0) queue.Dequeue()();
                return;
            }
            Thread.Sleep(1);
            while(queue.Count > 0) queue.Dequeue()();
        }
    }
    static void WaitPlayer() {
        var sw = System.Diagnostics.Stopwatch.StartNew();
        while (sw.ElapsedMilliseconds < 3000) {
            Pump();
            if (Player.All.Count > 0 && Player.All[Player.All.Count-1].Starts > 0) return;
            Thread.Sleep(5);
        }
        throw new Exception("player nao iniciou");
    }
    static void Open() { var u=Marshal.StringToHGlobalAnsi("https://test/movie"); try { Entry("fAbrir",u,IntPtr.Zero); } finally { Marshal.FreeHGlobal(u); } WaitPlayer(); }
    static void Roi(int x,int y,int w,int h) { Entry("fAspecto",x,y,w,h,x,y,w,h); Pump(); Check(probe(0)==(int)PlayerDisplayMode.Roi && probe(1)==x && probe(2)==y && probe(3)==w && probe(4)==h,"B ROI " + x+","+y+" "+w+"x"+h); }
    static void Simple(int x,int y,int w,int h) {
        int calls=probe(6); Entry("fJanela",x,y,w,h); Pump();
        bool full=x==0 && y==0 && w==1920 && h==1080;
        Check(probe(0)==(int)(full?PlayerDisplayMode.LetterBox:PlayerDisplayMode.Roi),"simple callback " + (full?"fullscreen letterbox":"partial ROI"));
        Check(probe(6)==calls+(full?0:1),"simple callback ROI count");
    }
    static int Main(string[] args) {
        NativeLibrary.SetDllImportResolver(typeof(Video).Assembly,(name,a,p)=>name=="libcapi-media-player.so.0" ? NativeLibrary.Load(args[0]) : IntPtr.Zero);
        NvVid.Registrar=(a,b,c,d,e,f,g)=>{}; NvVid.RegistrarFaixas=a=>{}; NvVid.RegistrarAspecto=a=>{}; NvVid.Evento=(a,b,c)=>{};
        NvVid.Faixa=(a,b,c)=>{}; NvVid.FaixasFim=(a,b)=>{}; NvVid.LogNativo=log.Add;
        // Real bridge registration is covered by the C fixture; lifecycle stubs have no libnuvio.
        video=new Video(()=>new Display(),a=>queue.Enqueue(a),System.IO.Path.GetTempPath(),1920,1080);
        Open();
        foreach(bool elm in new[]{false,true}) {
            video.GeometriaJanela=elm ? (Func<int,int,int,int,string>)((x,y,w,h)=>null) : null;
            Simple(0,0,1920,1080); Simple(0,138,1920,804);
            // Original / Fill / Zoom (contain, stretch, centered cover).
            Roi(0,0,1920,1080); Roi(0,0,1920,1080); Roi(0,0,1920,1080); // 16:9
            Roi(0,138,1920,804); Roi(0,0,1920,1080); Roi(-331,0,2581,1080); // 2.39:1 (1912x800)
            Roi(240,0,1440,1080); Roi(0,0,1920,1080); Roi(0,-180,1920,1440); // 4:3
            // #426: paused geometry still reaches the native bridge on both hosts.
            var pausedPlayer=Player.All[Player.All.Count-1];
            Entry("fPausar",1); Pump();
            Check(pausedPlayer.State==PlayerState.Paused,"pause acknowledged before aspect");
            int pausedStarts=pausedPlayer.Starts, pausedSeeks=pausedPlayer.SeekCalls;
            Simple(240,0,1440,1080); Roi(-331,0,2581,1080);
            Check(pausedPlayer.State==PlayerState.Paused && pausedPlayer.Starts==pausedStarts && pausedPlayer.SeekCalls==pausedSeeks,"paused aspect never resumes or seeks");
            Entry("fPausar",0); Pump();
        }
        // #426: Tizen 4.0 plane takes no off-screen ROI. Everything stays inside
        // the panel, contain-fit and centered; on-screen requests are untouched.
        Open(); video.GeometriaJanela=null; video.PlanoAceitaRoiFora=false;
        Roi(0,0,1920,1079);
        Entry("fAspecto",-331,0,2581,1080,-331,0,2581,1080); Pump();
        Check(probe(0)==(int)PlayerDisplayMode.Roi && probe(1)==0 && probe(2)==138 && probe(3)==1920 && probe(4)==803,"Tizen 4: 2.39:1 zoom is contain-fit and centered");
        Entry("fAspecto",0,-180,1920,1440,0,-180,1920,1440); Pump();
        Check(probe(1)==240 && probe(2)==0 && probe(3)==1440 && probe(4)==1080,"Tizen 4: 4:3 zoom is contain-fit and centered");
        Entry("fAspecto",-144,-77,2210,1246,-144,-77,2210,1246); Pump();
        Check(probe(1)>=0 && probe(2)>=0 && probe(1)+probe(3)<=1920 && probe(2)+probe(4)<=1080,"Tizen 4: log #426 zoom ROI stays on the panel");
        { int wx=9,wy=9,ww=0,wh=0;
          video.GeometriaJanela=(x,y,w,h)=>{wx=x;wy=y;ww=w;wh=h;return null;};
          Open(); fail(2,-95); Entry("fAspecto",-331,0,2581,1080,-331,0,2581,1080); Pump(); fail(2,0);
          Check(wx==0 && wy==138 && ww==1920 && wh==803,"Tizen 4: legacy window fallback also stays on the panel"); }
        video.GeometriaJanela=null; video.PlanoAceitaRoiFora=true; Open();
        var p=Player.All[Player.All.Count-1]; int starts=p.Starts;
        video.GeometriaJanela=null;
        fail(2,-95); Entry("fAspecto",-331,0,2581,1080,0,0,1920,1080); Pump();
        Check(p.DisplaySettings.Mode==PlayerDisplayMode.LetterBox,"ROI rejection restores old 6+ fullscreen path");
        Check(probe(0)==(int)PlayerDisplayMode.LetterBox,"partial native Roi state cleared");
        Check(log.Exists(l=>l.Contains("[aspecto]") && l.Contains("0xffffffa1")),"one-line rejection reason");
        int calls=probe(5); fail(2,0); Entry("fJanela",240,0,1440,1080); Pump();
        Check(probe(5)==calls && p.DisplaySettings.Mode==PlayerDisplayMode.Roi,"fallback retained for entire session");
        Check(p.Starts==starts && !p.Muted && p.State==PlayerState.Playing,"aspect never starts, pauses or mutes");
        Entry("fAspecto",0,-180,1920,1440,0,0,1920,1080); Open();
        Check(probe(5)==calls,"old queued aspect discarded after generation change");
        Roi(240,0,1440,1080); // New session tries B again.
        foreach(bool elm in new[]{false,true}) {
            Open(); p=Player.All[Player.All.Count-1]; int wx=0,wy=0,ww=1920,wh=1080;
            video.GeometriaJanela=elm ? (Func<int,int,int,int,string>)((x,y,w,h)=>{wx=x;wy=y;ww=w;wh=h;return null;}) : null;
            fail(1,-22); calls=probe(6); Entry("fJanela",0,140,1920,800); Pump();
            Check(probe(6)==calls,"mode rejection does not mutate ROI");
            Check(p.DisplaySettings.Mode==(elm?PlayerDisplayMode.FullScreen:PlayerDisplayMode.Roi),"mode rejection uses old host path");
            Check(!elm || (wy==140 && wh==800),"old ElmSharp geometry retained");
            fail(1,0);
        }
        // Legacy window writes can also fail after moving the surface.
        Open(); p=Player.All[Player.All.Count-1];
        int gx=0,gy=0,gw=1920,gh=1080;
        video.GeometriaJanela=(x,y,w,h)=>{ gx=x;gy=y;gw=w;gh=h; return x==0 && y==0 && w==1920 && h==1080 ? null : "geometry partially applied"; };
        fail(2,-95); Entry("fJanela",240,0,1440,1080); Pump(); fail(2,0);
        Check(gx==0 && gy==0 && gw==1920 && gh==1080,"partial ElmSharp mutation rolled back before legacy Roi");
        Check(p.DisplaySettings.Mode==PlayerDisplayMode.Roi,"legacy Roi after geometry rollback");
        video.GeometriaJanela=null; p.DisplaySettings.FailRoi=true;
        Entry("fJanela",240,0,1440,1080); Pump();
        Check(p.DisplaySettings.Mode==PlayerDisplayMode.LetterBox && probe(0)==(int)PlayerDisplayMode.LetterBox,"failed legacy ROI leaves neutral mode");
        Check(log.Exists(l=>l.Contains("[aspecto]") && l.Contains("fallback=failed")),"failed restoration is not reported as success");
        p.DisplaySettings.FailRoi=false;
        video.GeometriaJanela=(x,y,w,h)=>{gx=x;gy=y;gw=w;gh=h;return null;};
        Entry("fJanela",240,0,1440,1080); Pump();
        Entry("fParar"); Pump();
        Check(gx==0 && gy==0 && gw==1920 && gh==1080,"session teardown restores legacy window");
        Console.WriteLine("tpk_aspecto: PASS (native fake, no TV proof)"); return 0;
    }
}
