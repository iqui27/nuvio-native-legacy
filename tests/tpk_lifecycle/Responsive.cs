// #412: o Video.cs real com a consulta nativa que leva os 5 s dos logs.
using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using NuvioTpk;
using Tizen.Multimedia;
class Responsive {
    static readonly ConcurrentQueue<Action> ui = new ConcurrentQueue<Action>();
    static Video video;
    static int lentas;
    static void Entry(string name, params object[] args) => ((Delegate)typeof(Video).GetField(name, BindingFlags.NonPublic|BindingFlags.Instance).GetValue(video)).DynamicInvoke(args);
    static void Pump() { while (ui.TryDequeue(out var a)) a(); }
    static void Check(bool ok, string msg) { if (!ok) throw new Exception(msg); Console.WriteLine("ok: " + msg); }
    static void Wait(Func<bool> ready) { var sw=Stopwatch.StartNew(); while(!ready() && sw.ElapsedMilliseconds<2000) { Pump(); Thread.Sleep(1); } Check(ready(),"host respondeu"); }
    static void Open() { var p=Marshal.StringToHGlobalAnsi("https://test/movie"); try { Entry("fAbrir",p,IntPtr.Zero); } finally { Marshal.FreeHGlobal(p); } Pump(); }
    static int Main() {
        NvVid.Registrar=(a,b,c,d,e,f,g)=>{}; NvVid.RegistrarFaixas=a=>{}; NvVid.RegistrarAspecto=a=>{};
        NvVid.Evento=(a,b,c)=>{ if(a==10) Interlocked.Increment(ref lentas); }; NvVid.LogNativo=Console.WriteLine; NvVid.Faixa=(a,b,c)=>{}; NvVid.FaixasFim=(a,b)=>{};
        video=new Video(()=>new Display(),a=>ui.Enqueue(a),System.IO.Path.GetTempPath(),1920,1080);
        Open(); Wait(()=>Player.All.Count>0 && Player.All[0].Starts==1);
        var old=Player.All[0]; old.PositionDelayMs=5000;
        var sw=Stopwatch.StartNew(); video.Tique(); Pump();
        Check(sw.ElapsedMilliseconds<50,"Tique nao bloqueia a UI por 5 s");
        Check(old.PositionEntered.Wait(1000),"consulta lenta executada fora da UI");
        for(int i=0;i<10000;i++) video.Tique();
        Check(old.PositionCalls==1,"consulta pendente nao acumula fios/pedidos");
        Thread.Sleep(5100); Pump();
        Check(lentas==1,"consulta de 5000 ms avisa o watchdog do C");
        video.Tique(); Check(old.PositionEntered.Wait(1000),"nova consulta");
        Entry("fParar"); Pump(); Wait(()=>old.Disposed);
        Check(old.Muted && old.Display==null,"Home silencia e solta o plano mesmo com consulta presa");
        Open(); Wait(()=>Player.All.Count==2 && Player.All[1].Starts==1);
        Thread.Sleep(5100); Pump();
        Check((int)typeof(Video).GetField("posMs",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(video)==0,"posicao tardia nao contamina a sessao nova");
        Entry("fParar"); Pump(); Wait(()=>Player.All[1].Disposed);
        return 0;
    }
}
