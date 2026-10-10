// Compila o Video.cs REAL contra a superficie usada do Tizen.Multimedia.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using NuvioTpk;
using Tizen.Multimedia;
class Test {
    static readonly Queue<Action> queue = new Queue<Action>();
    static int fatal, failures;
    static Video video;
    static void Check(bool ok, string what) { Console.WriteLine((ok ? "ok: " : "FAIL: ") + what); if (!ok) failures++; }
    static void Entry(string field, params object[] args) => ((Delegate)typeof(Video).GetField(field, BindingFlags.Instance|BindingFlags.NonPublic).GetValue(video)).DynamicInvoke(args);
    static void Pump() { while (queue.Count > 0) queue.Dequeue()(); }
    static void Setup() {
        queue.Clear(); Player.All.Clear(); Player.Defer = false; Player.Overlap = 0; fatal = 0;
        video = new Video(() => new Display(), a => queue.Enqueue(a), "/tmp", 1920, 1080);
        typeof(Video).GetField("PrazoPararMs", BindingFlags.Instance|BindingFlags.NonPublic)?.SetValue(video, 150);
        typeof(Video).GetField("FalhaFatal", BindingFlags.Instance|BindingFlags.NonPublic)?.SetValue(video, (Action)(() => Interlocked.Increment(ref fatal)));
    }
    static void Open() {
        var p = Marshal.StringToHGlobalAnsi("https://test/movie");
        try { Entry("fAbrir", p, IntPtr.Zero); } finally { Marshal.FreeHGlobal(p); }
    }
    static int Main() {
        NvVid.Registrar = (a,b,c,d,e,f,g) => {}; NvVid.RegistrarFaixas = a => {};
        NvVid.Evento = (a,b,c) => {}; NvVid.LogNativo = Console.WriteLine;
        NvVid.Faixa = (a,b,c) => {}; NvVid.FaixasFim = (a,b) => {};
        Setup(); Open(); Pump(); var old = Player.All[0];
        Entry("fParar"); Open(); Pump();
        Check(old.Disposed && old.Muted && old.Display == null && Player.All.Count == 2, "mudo, plano solto e dispose antes do proximo player");
        Check(Player.Overlap == 0, "nenhuma sobreposicao de players");
        Entry("fParar"); Pump();
        Setup(); Open(); Pump(); old = Player.All[0]; old.FailDispose = true;
        Entry("fParar"); Open(); Pump();
        Check(Player.All.Count == 1, "dispose recusado impede abertura seguinte");
        Thread.Sleep(250); Check(fatal > 0, "dispose recusado tem prazo fatal");
        Setup(); Open(); Pump(); old = Player.All[0]; old.BlockDispose = true;
        Entry("fParar"); var disposing = Task.Run(Pump);
        Check(old.DisposeEntered.Wait(1000), "Dispose nativo entrou");
        Thread.Sleep(250); Check(fatal == 1 && !old.Disposed, "Dispose preso tem prazo fatal");
        old.DisposeRelease.Set(); disposing.Wait();
        Setup(); Open(); Pump();
        Entry("fParar"); // fio principal nunca drena a fila: simula API nativa presa
        var sw = Stopwatch.StartNew(); for(int i=0;i<10000;i++) Entry("fPos");
        Check(sw.ElapsedMilliseconds < 100, "leituras da posicao nao esperam o host preso");
        Thread.Sleep(250); Check(fatal > 0, "prazo armado antes do despacho ao principal");
        Pump(); // solta a instancia para o proximo teste
        Setup(); Open(); Pump(); old = Player.All[0]; old.BlockStop = true;
        Entry("fParar"); Open(); var drain = Task.Run(Pump);
        Check(old.StopEntered.Wait(1000), "Stop nativo entrou");
        Thread.Sleep(250); Check(fatal == 1, "Stop preso tem prazo independente do principal");
        old.StopRelease.Set(); drain.Wait();
        Check(Player.All.Count == 1, "retorno tardio do Stop nao abre apos timeout");
        Setup(); Open(); Pump(); old = Player.All[0]; old.FailStop = true;
        Entry("fParar"); Pump();
        Thread.Sleep(250);
        Check(old.Disposed && fatal == 0, "Stop recusado ainda tenta Unprepare e Dispose");
        Setup(); Open(); Pump(); old = Player.All[0]; old.FailUnprepare = true;
        Entry("fParar"); Pump(); Thread.Sleep(250);
        Check(old.Disposed && fatal == 0, "Unprepare recusado com Dispose concluido nao encerra");
        Setup(); Open(); Pump();
        Entry("fParar"); Thread.Sleep(100); Open(); Thread.Sleep(100);
        Check(fatal == 1, "pedido repetido nao renova prazo de parada");
        Pump();
        Setup(); Player.Defer = true; Open(); Pump(); old = Player.All[0];
        Check(old.State == PlayerState.Preparing && !old.Prepared.Task.IsCompleted, "prepare realmente pendente em Preparing");
        Entry("fParar"); Pump(); Thread.Sleep(250);
        Check(old.UnprepareCalls == 0, "Voltar em Preparing nao chama Unprepare");
        Check(old.Disposed && fatal == 0, "Voltar em Preparing com Dispose concluido nao encerra");
        old.Prepared.SetResult(true); Thread.Sleep(30);
        Check(old.Starts == 0 && old.DisposeCalls == 1, "prepare tardio nao inicia nem destroi novamente");
        Setup(); Player.Defer = true; Open(); Pump(); old = Player.All[0];
        Player.Defer = false; Open(); Pump(); Thread.Sleep(250);
        Check(old.Disposed && old.UnprepareCalls == 0 && fatal == 0 && Player.All.Count == 2 && Player.Overlap == 0,
              "troca em Preparing libera antes de abrir sem encerrar");
        old.Prepared.SetResult(true); Thread.Sleep(30);
        Check(old.Starts == 0 && old.DisposeCalls == 1, "prepare antigo nao inicia apos troca");
        Entry("fParar"); Pump();
        Setup(); Open(); Entry("fParar"); Thread.Sleep(250);
        Check(fatal == 0, "fila atrasada sem player nativo nao encerra");
        Pump();
        Check(Player.All.Count == 0, "abertura cancelada antes de drenar a fila");
        return failures == 0 ? 0 : 1;
    }
}
namespace Tizen.Multimedia {
    public enum PlayerState { Idle, Preparing, Ready, Playing, Paused }
    public enum PlayerDisplayMode { LetterBox, Roi, FullScreen }
    public class Rectangle { public Rectangle(int x,int y,int w,int h) {} }
    public class Display {}
    public class MediaUriSource { public MediaUriSource(string s) {} }
    public class Info { public int Selected {get;set;} public int GetCount()=>1; public string GetLanguageCode(int i)=>"eng"; }
    public class Props { public int Channels=2, SampleRate=48000; public Props Size=>this; public int Width=1920, Height=1080; }
    public class Stream { public int GetDuration()=>60000; public Props GetVideoProperties()=>new Props(); public Props GetAudioProperties()=>new Props(); }
    public class Settings { public PlayerDisplayMode Mode {get;set;} public void SetRoi(Rectangle r) {} }
    public class Event : EventArgs { public int Error,Percent,Duration; public string Reason,Text; }
    public class Player {
        public static List<Player> All = new List<Player>(); public static bool Defer; public static int Overlap;
        public TaskCompletionSource<bool> Prepared = new TaskCompletionSource<bool>();
        public Player() { if(All.Exists(p=>!p.Disposed)) Overlap++; All.Add(this); }
        public bool Disposed, FailDispose, BlockDispose, BlockStop, FailStop, FailUnprepare;
        public ManualResetEventSlim StopEntered = new ManualResetEventSlim(), StopRelease = new ManualResetEventSlim(); public int Starts, DisposeCalls, UnprepareCalls;
        public ManualResetEventSlim DisposeEntered = new ManualResetEventSlim(), DisposeRelease = new ManualResetEventSlim();
        public PlayerState State {get;set;} public bool Muted {get;set;} public float Volume {get;set;}
        public Display Display {get;set;} public string UserAgent,Cookie;
        public Info AudioTrackInfo = new Info(), SubtitleTrackInfo = new Info(); public Stream StreamInfo = new Stream(); public Settings DisplaySettings = new Settings();
        public event EventHandler<Event> PlaybackCompleted, ErrorOccurred, BufferingProgressChanged, PlaybackInterrupted, SubtitleUpdated;
        public void SetSource(MediaUriSource s) {} public void SetSubtitleOffset(int n) {} public void SetPlaybackRate(float n) {}
        public async Task PrepareAsync() { State=PlayerState.Preparing; if(Defer) await Prepared.Task; if(!Disposed) State=PlayerState.Ready; }
        public void Start() { Starts++; State=PlayerState.Playing; } public void Stop() { StopEntered.Set(); if(BlockStop) StopRelease.Wait(); if(FailStop) throw new Exception("stop recusado"); State=PlayerState.Ready; }
        public void Pause() { State=PlayerState.Paused; } public void Unprepare() { UnprepareCalls++; if(State != PlayerState.Ready && State != PlayerState.Playing && State != PlayerState.Paused) throw new InvalidOperationException("unprepare em " + State); if(FailUnprepare) throw new Exception("unprepare recusado"); State=PlayerState.Idle; }
        public void Dispose() { DisposeCalls++; DisposeEntered.Set(); if(BlockDispose) DisposeRelease.Wait(); if(FailDispose) throw new Exception("dispose recusado"); Disposed=true; }
        public int GetPlayPosition()=>1234; public Task SetPlayPositionAsync(int ms,bool precise)=>Task.CompletedTask;
    }
}
