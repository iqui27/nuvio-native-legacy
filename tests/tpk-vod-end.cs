using System;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using NuvioTpk;
using Tizen.Multimedia;
class VodEndTest {
    static Video video;
    static int ends, errors;
    static void Entry(string name, params object[] args) => ((Delegate)typeof(Video).GetField(name, BindingFlags.NonPublic | BindingFlags.Instance).GetValue(video)).DynamicInvoke(args);
    static void Check(bool ok, string message) { if (!ok) throw new Exception(message); Console.WriteLine("ok: " + message); }
    static void Wait(Func<bool> ready) { var t = Stopwatch.StartNew(); while (!ready() && t.ElapsedMilliseconds < 3000) Thread.Sleep(1); Check(ready(), "host responded"); }
    static void Open() { var u = Marshal.StringToHGlobalAnsi("https://fixture.invalid/episode"); try { Entry("fAbrir", u, IntPtr.Zero); } finally { Marshal.FreeHGlobal(u); } }
    static int Main() {
        NvVid.Registrar = (a,b,c,d,e,f,g) => {}; NvVid.RegistrarFaixas = a => {}; NvVid.RegistrarAspecto = a => {};
        NvVid.Evento = (t,a,b) => { if (t == 4) Interlocked.Increment(ref ends); if (t == 5) Interlocked.Increment(ref errors); };
        NvVid.Faixa = (a,b,c) => {}; NvVid.FaixasFim = (a,b) => {}; NvVid.LogNativo = Console.WriteLine;
        video = new Video(() => new Display(), a => a(), System.IO.Path.GetTempPath(), 1920, 1080);
        Open(); Wait(() => Player.All.Count == 1 && Player.All[0].Starts == 1);
        var p = Player.All[0];
        p.Complete(false); Thread.Sleep(50); Check(ends == 0, "live segment does not end playback");
        p.PositionDelayMs = 1000; video.Tique(); Check(p.PositionEntered.Wait(1000), "position query entered");
        var timer = Stopwatch.StartNew(); Entry("fPausar", 1);
        while (p.State != PlayerState.Paused && timer.ElapsedMilliseconds < 300) Thread.Sleep(1);
        Check(p.State == PlayerState.Paused, "pause stays responsive during slow position read");
        Entry("fBuscar", 45000); Wait(() => p.SeekCalls == 1);
        p.Complete(true); Wait(() => ends == 1);
        Check((int)((Delegate)typeof(Video).GetField("fPos", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(video)).DynamicInvoke() == 60000, "EOS publishes duration for next-episode flow");
        Thread.Sleep(1100);
        Check((int)((Delegate)typeof(Video).GetField("fPos", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(video)).DynamicInvoke() == 60000, "late position sample cannot overwrite EOS");
        // Ready after EOS must accept replay/seek, without a network reconnect.
        Entry("fBuscar", 10000); Entry("fPausar", 0); Wait(() => p.Starts == 2);
        p.BlockStop = true; Open(); Wait(() => p.Disposed || errors > 0);
        Check(p.Disposed && p.StopCalls == 0, "release goes directly to Dispose without blocking Stop");
        Wait(() => Player.All.Count == 2 && Player.All[1].Starts == 1);
        p.Complete(true); Thread.Sleep(50); Check(ends == 1 && Player.Overlap == 0, "stale EOS ignored; decoder ownership preserved");
        Entry("fParar");
        return 0;
    }
}
