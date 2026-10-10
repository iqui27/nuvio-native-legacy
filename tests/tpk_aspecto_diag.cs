using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using NuvioTpk;
using Tizen.Multimedia;

class AspectoDiagTest
{
    [DllImport("libcapi-media-player.so.0")] static extern double probe(int i);
    [DllImport("libcapi-media-player.so.0")] static extern void fail(int op, int rc);
    static int Main(string[] args)
    {
        NativeLibrary.SetDllImportResolver(typeof(Video).Assembly, (name, a, p) => name == "libcapi-media-player.so.0" ? NativeLibrary.Load(args[0]) : IntPtr.Zero);
        var log = new List<string>();
        int ack = -1, accepted = -1;
        NvVid.Registrar = (a,b,c,d,e,f,g) => {};
        NvVid.RegistrarFaixas = a => {};
        NvVid.Evento = (e,a,b) => { if (e == 9) { ack = a; accepted = b; } };
        NvVid.LogNativo = log.Add;
        var v = new Video(() => new Display(), a => a(), Path.GetTempPath(), 1920, 1080);
        var flags = BindingFlags.NonPublic | BindingFlags.Instance;
        var player = new Player();
        typeof(Video).GetField("player", flags).SetValue(v, player);
        typeof(Video).GetField("aspectoTizen", flags).SetValue(v, "5.0");
        typeof(Video).GetField("aspectoModelo", flags).SetValue(v, "QN55Q60RAG-test");
        var apply = typeof(Video).GetMethod("AspectoAplicar", flags);
        Action<int> run = n => { apply.Invoke(v, new object[] { n }); Check(ack == n, "ack identity"); };
        run(0); Check(accepted == 1 && probe(0) == (int)PlayerDisplayMode.LetterBox, "A Original");
        run(1); Check(probe(0) == (int)PlayerDisplayMode.FullScreen, "A Fill");
        run(2); Check(probe(0) == (int)PlayerDisplayMode.CroppedFull, "A Zoom");
        Props.VideoW = 1920; Props.VideoH = 800;
        run(3); Check(probe(1)==0 && probe(2)==140 && probe(3)==1920 && probe(4)==800, "B Original wide");
        run(4); Check(probe(2)==0 && probe(4)==1080, "B Fill");
        run(5); Check(probe(1)==-336 && probe(3)==2592 && probe(4)==1080, "B Zoom outside screen");
        run(6); Check(accepted==0 && log[log.Count-1].Contains("window=unavailable"), "C unavailable on NUI");
        int wx=0, wy=0, ww=0, wh=0;
        v.GeometriaJanela = (x,y,w,h) => { wx=x; wy=y; ww=w; wh=h; return null; };
        run(6); Check(ww==1920 && wh==800 && wy==140, "C Original");
        run(7); Check(ww==1920 && wh==1080, "C Fill");
        run(8); Check(wx==-336 && ww==2592, "C Zoom");
        run(0); Check(wx==0 && wy==0 && ww==1920 && wh==1080, "C to A restores window");
        run(9); Check(probe(7)==1 && probe(8)==1 && probe(0)==(int)PlayerDisplayMode.LetterBox, "D Original");
        run(10); Check(probe(7)==1 && probe(0)==(int)PlayerDisplayMode.FullScreen, "D Fill");
        run(11); Check(Math.Abs(probe(7)-1920.0/2592)<1e-6 && probe(8)==1 && probe(5)>0, "D Zoom source crop");
        run(0); Check(probe(5)==0 && probe(7)==1 && log[log.Count-1].Contains("source-reset=0x00000000"), "D to A resets source");
        Props.VideoW=1440; Props.VideoH=1080;
        run(3); Check(probe(1)==240 && probe(3)==1440, "B Original 4:3");
        run(5); Check(probe(2)==-180 && probe(4)==1440, "B Zoom 4:3");
        run(11); Check(probe(7)==1 && probe(8)==0.75 && probe(6)==0.125, "D Zoom 4:3");
        run(-1); Check(probe(8)==1, "OFF restores source");
        fail(1,-22); run(3);
        Check(accepted==0 && log[log.Count-1].Contains("mode:Roi=0xffffffea") && log[log.Count-1].Contains("display-roi=0x00000000"), "native failure preserves both rc without fallback");
        fail(1,0); fail(2,-95); run(5); Check(accepted==0, "ROI rejection reported"); fail(2,0);
        run(11); fail(3,-5); run(0); Check(accepted==0 && log[log.Count-1].Contains("reset=failed"), "failed source reset blocks contaminated result"); fail(3,0); run(0);
        typeof(Video).GetField("aspectoTizen", flags).SetValue(v, "4.0");
        run(9); Check(accepted==0 && log[log.Count-1].Contains("requires-Tizen5"), "source API gated on Tizen4");
        Props.VideoW=0; run(3); Check(accepted==0, "unknown dimensions are not success");
        Check(log.Exists(l=>l.Contains("[aspecto-diag] tizen=5.0 model=QN55Q60RAG-test method=B:DisplayRoi mode=Original video-query=GetVideoProperties video=1920x800")), "complete one-line metadata");
        Check(log.TrueForAll(l=>!l.Contains("\n")), "single-line records");
        Console.WriteLine("tpk_aspecto_diag: all checks passed (fake native library; no TV proof)");
        return 0;
    }
    static void Check(bool ok, string msg) { if (!ok) throw new Exception(msg); Console.WriteLine("ok: " + msg); }
}
namespace Tizen.System {
    public static class Information {
        public static bool TryGetValue<T>(string key, out T value) { value=default(T); return false; }
    }
}
