using System;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using NuvioTpk;
using Tizen.Multimedia;

class TrailerAudioTest {
    static Video video;
    static void Entry(string name, params object[] args) => ((Delegate)typeof(Video).GetField(name, BindingFlags.NonPublic | BindingFlags.Instance).GetValue(video)).DynamicInvoke(args);
    static void Wait(Func<bool> ready) {
        var timer = Stopwatch.StartNew();
        while (!ready() && timer.ElapsedMilliseconds < 3000) Thread.Sleep(1);
        if (!ready()) throw new Exception("host timeout");
    }
    static void Check(bool ok, string message) {
        if (!ok) throw new Exception(message);
        Console.WriteLine("ok: " + message);
    }
    static void Open() {
        var url = Marshal.StringToHGlobalAnsi("https://fixture.invalid/trailer.mp4");
        try { Entry("fAbrir", url, IntPtr.Zero); }
        finally { Marshal.FreeHGlobal(url); }
    }
    static void Drain() {
        var f = typeof(Video).GetField("fio", BindingFlags.NonPublic | BindingFlags.Instance).GetValue(video);
        using (var done = new ManualResetEventSlim()) {
            ((SynchronizationContext)f).Post(_ => done.Set(), null);
            Check(done.Wait(3000), "session queue drained");
        }
    }
    static int Main() {
        NvVid.Registrar = (a,b,c,d,e,f,g) => {};
        NvVid.RegistrarFaixas = a => {}; NvVid.RegistrarAspecto = a => {};
        NvVid.Evento = (a,b,c) => {}; NvVid.Faixa = (a,b,c) => {}; NvVid.FaixasFim = (a,b) => {};
        NvVid.LogNativo = Console.WriteLine;
        video = new Video(() => new Display(), a => a(), System.IO.Path.GetTempPath(), 1920, 1080);
        Open(); Wait(() => Player.All.Count == 1 && Player.All[0].Starts == 1);
        // A prior trailer/film still owns the decoder when C sends the next volume.
        var old = Player.All[0]; old.BlockDispose = true;
        Open(); Wait(() => old.DisposeEntered.IsSet);
        Entry("fVolume", 0); Drain();
        old.DisposeRelease.Set();
        Wait(() => Player.All.Count == 2 && Player.All[1].Starts == 1);
        var trailer = Player.All[1]; Drain();
        Check(trailer.Muted && trailer.Volume == 0, "opening mute survives delayed release");
        Entry("fVolume", 100); Drain();
        Check(!trailer.Muted && trailer.Volume == 1, "manual adoption restores trailer audio");
        trailer.BlockDispose = true;
        Open(); Wait(() => trailer.DisposeEntered.IsSet);
        Entry("fVolume", 100); Drain();
        trailer.DisposeRelease.Set();
        Wait(() => Player.All.Count == 3 && Player.All[2].Starts == 1); Drain();
        Check(!Player.All[2].Muted && Player.All[2].Volume == 1, "audible autoplay survives delayed release");
        Entry("fParar");
        return 0;
    }
}
