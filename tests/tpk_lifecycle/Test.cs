// Compila o Video.cs REAL contra a superficie usada do Tizen.Multimedia.
// Contrato (#412): nenhuma API do player toca o fio principal; o descarte
// trava/pifa vira EV_ERRO para o C em vez de encerrar o processo, e um player
// novo so nasce depois do Dispose do anterior (o decoder da TV e um so).
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
    static int failures, presos, alvoPreso, completed;
    static Video video;
    static readonly List<int> erros = new List<int>();
    static int Erros { get { lock (erros) return erros.Count; } }
    static void Check(bool ok, string what) { Console.WriteLine((ok ? "ok: " : "FAIL: ") + what); if (!ok) failures++; }
    static void Entry(string field, params object[] args) => ((Delegate)typeof(Video).GetField(field, BindingFlags.Instance|BindingFlags.NonPublic).GetValue(video)).DynamicInvoke(args);
    static object EntryPos() => ((Delegate)typeof(Video).GetField("fPos", BindingFlags.Instance|BindingFlags.NonPublic).GetValue(video)).DynamicInvoke();
    static void Pump() { for (;;) { Action a; lock (queue) { if (queue.Count == 0) return; a = queue.Dequeue(); } a(); } }
    static void Wait(Func<bool> ready) { var sw = Stopwatch.StartNew(); while (!ready() && sw.ElapsedMilliseconds < 3000) { Pump(); Thread.Sleep(1); } Check(ready(), "host respondeu a tempo"); }
    static void Setup() {
        lock (queue) queue.Clear();
        Player.All.Clear(); Player.Defer = false; Player.Overlap = 0; lock (erros) erros.Clear();
        video = new Video(() => new Display(), a => { lock (queue) queue.Enqueue(a); }, "/tmp", 1920, 1080);
        typeof(Video).GetField("PrazoPararMs", BindingFlags.Instance|BindingFlags.NonPublic)?.SetValue(video, 150);
    }
    static void Open() {
        var p = Marshal.StringToHGlobalAnsi("https://test/movie");
        try { Entry("fAbrir", p, IntPtr.Zero); } finally { Marshal.FreeHGlobal(p); }
    }
    static void OpenTocado() { Open(); Wait(() => Player.All.Count > 0 && Player.All[Player.All.Count - 1].Starts == 1); }
    static int Main() {
        NvVid.Registrar = (a,b,c,d,e,f,g) => {}; NvVid.RegistrarFaixas = a => {}; NvVid.RegistrarAspecto = a => {};
        NvVid.Evento = (t,a,b) => { if (t == 4) Interlocked.Increment(ref completed); if (t == 5) lock (erros) erros.Add(a); if (t == 9) { alvoPreso = a; Interlocked.Increment(ref presos); } };
        NvVid.LogNativo = Console.WriteLine;
        NvVid.Faixa = (a,b,c) => {}; NvVid.FaixasFim = (a,b) => {};

        // #302: a segment notification is not EOS while the native pipeline plays.
        Setup(); OpenTocado(); var live = Player.All[0]; int ends = completed;
        live.Complete(false);
        Thread.Sleep(50);
        Check(completed == ends && live.Starts == 1 && live.SeekCalls == 0 && !live.Disposed,
              "segment completion while Playing neither ends nor restarts playback");
        live.Complete(true); Wait(() => completed == ends + 1);
        Open(); Wait(() => Player.All.Count == 2 && Player.All[1].Starts == 1);
        Check(live.Disposed && Player.Overlap == 0 &&
              Player.All[1].SeekCalls == 0,
              "real EOS recovery releases audio and video before a fresh source without resume seek");
        live.Complete(true); Thread.Sleep(50);
        Check(completed == ends + 1, "old completion cannot end the replacement session");
        Entry("fParar"); Wait(() => Player.All[1].Disposed);

        // Parar e abrir: o velho sai mudo e sem plano ANTES do novo existir.
        Setup(); OpenTocado(); var old = Player.All[0];
        Entry("fParar"); Open();
        Wait(() => Player.All.Count == 2 && Player.All[1].Starts == 1);
        Check(old.Disposed, "disposed before the next player");
        Check(Player.Overlap == 0, "nenhuma sobreposicao de players");
        Entry("fParar"); Wait(() => Player.All[1].Disposed);

        // Dispose recusado: sem player novo, o C fica sabendo, o processo segue.
        Setup(); OpenTocado(); old = Player.All[0]; old.FailDispose = true;
        Entry("fParar"); Open();
        Thread.Sleep(400); Pump();
        Check(Player.All.Count == 1, "dispose recusado impede abertura seguinte");
        Check(Erros > 0, "descarte falho vira erro para o C, nao saida do processo");

        // Dispose preso: parar de novo nao re-arma nem duplica o erro; o prazo
        // vence sem player novo; destravou, a abertura seguinte funciona.
        Setup(); OpenTocado(); old = Player.All[0]; old.BlockDispose = true;
        Entry("fParar"); Entry("fParar"); Open();
        Check(old.DisposeEntered.Wait(1000), "Dispose nativo entrou");
        Thread.Sleep(400); Pump();
        Check(Player.All.Count == 1, "Dispose preso: nenhum player novo sem o Dispose");
        Check(Erros == 1, "Dispose preso: um erro por prazo, parada repetida nao duplica");
        old.DisposeRelease.Set();
        Wait(() => old.Disposed);
        Open(); Wait(() => Player.All.Count == 2 && Player.All[1].Starts == 1);
        Check(Player.Overlap == 0, "destravado, o player seguinte abre sem sobrepor");
        Entry("fParar"); Wait(() => Player.All[1].Disposed);

        // Posicao: o C le o cache, nunca espera o host.
        Setup(); OpenTocado();
        var sw = Stopwatch.StartNew(); for (int i = 0; i < 10000; i++) Entry("fPos");
        Check(sw.ElapsedMilliseconds < 100, "leituras da posicao nao esperam o host");
        Entry("fParar"); Wait(() => Player.All[0].Disposed);

        // Voltar em Preparing: sem Unprepare, dispose direto; o prepare tardio
        // nao inicia nem destroi de novo (a continuacao morreu com a sessao).
        Setup(); Player.Defer = true; Open();
        Wait(() => Player.All.Count > 0 && Player.All[0].State == PlayerState.Preparing);
        old = Player.All[0];
        Entry("fParar");
        Wait(() => old.Disposed);
        Check(old.UnprepareCalls == 0, "Voltar em Preparing nao chama Unprepare");
        old.Prepared.SetResult(true); Thread.Sleep(100); Pump();
        Check(old.Starts == 0 && old.DisposeCalls == 1, "prepare tardio nao inicia nem destroi novamente");

        // Troca em Preparing: o velho e liberado antes de o novo abrir.
        Setup(); Player.Defer = true; Open();
        Wait(() => Player.All.Count > 0 && Player.All[0].State == PlayerState.Preparing);
        old = Player.All[0];
        Player.Defer = false; Open();
        Wait(() => Player.All.Count == 2 && Player.All[1].Starts == 1);
        Check(old.Disposed && old.UnprepareCalls == 0 && Player.Overlap == 0,
              "troca em Preparing libera antes de abrir, sem sobrepor");
        old.Prepared.SetResult(true); Thread.Sleep(100); Pump();
        Check(old.Starts == 0 && old.DisposeCalls == 1, "prepare antigo nao inicia apos troca");
        Entry("fParar"); Wait(() => Player.All[1].Disposed);
        // #412: APIs recusadas pedem recuperacao; seek nao inventa relogio.
        Setup(); OpenTocado(); old = Player.All[0]; int antes = presos;
        old.FailPause = true; Entry("fPausar", 1); Wait(() => presos > antes);
        Check(alvoPreso == -1, "pause InvalidOperation recupera no ultimo relogio");
        old.FailSeek = true; antes = presos; Entry("fBuscar", 1347033); Wait(() => presos > antes);
        Check(alvoPreso == 1347033 && (int)EntryPos() == 0, "seek InvalidOperation conserva alvo sem publicar progresso falso");
        old.FailState = true; antes = presos; video.Tique(); Wait(() => presos > antes);
        Check(alvoPreso == -1, "GetState InvalidOperation pede recuperacao");
        old.FailState = false; old.FailPause = false; old.FailSeek = false;
        old.SeekPending = new TaskCompletionSource<bool>(); Entry("fBuscar", 1200000);
        Wait(() => old.SeekCalls == 2); Entry("fPausar", 1);
        Thread.Sleep(50); Check(old.State == PlayerState.Playing, "pause aguarda seek nativo em voo");
        old.SeekPending.SetResult(true); Wait(() => old.State == PlayerState.Paused);
        Entry("fParar"); Wait(() => old.Disposed);
        // P1-b: seek tocando trava; prazo conserva alvo e nova sessao o recebe.
        Setup();
        typeof(Video).GetField("PrazoBuscarMs", BindingFlags.Instance|BindingFlags.NonPublic)?.SetValue(video, 150);
        OpenTocado(); old = Player.All[0];
        old.SeekPending = new TaskCompletionSource<bool>(); antes = presos;
        Entry("fBuscar", 1200000); Wait(() => presos > antes);
        Check(alvoPreso == 1200000 && (int)EntryPos() == 0 && old.State == PlayerState.Playing,
              "seek tocando preso conserva alvo sem inventar progresso");
        Entry("fParar"); Open(); Wait(() => Player.All.Count == 2 && Player.All[1].Starts == 1);
        Entry("fBuscar", alvoPreso); Wait(() => (int)EntryPos() == 1200000);
        Check(old.Disposed && Player.Overlap == 0, "seek tocando recupera no alvo numa sessao nova");
        old.SeekPending.SetResult(true);
        Entry("fParar"); Wait(() => Player.All[1].Disposed);
        // Seek nunca conclui e interrompe playback: o prazo independe de Tique.
        Setup();
        typeof(Video).GetField("PrazoBuscarMs", BindingFlags.Instance|BindingFlags.NonPublic)?.SetValue(video, 150);
        OpenTocado(); old = Player.All[0];
        old.SeekPending = new TaskCompletionSource<bool>(); antes = presos;
        Entry("fBuscar", 1200000); Wait(() => old.SeekCalls == 1);
        old.Interromper(); Entry("fPausar", 1); Entry("fBuscar", 1347033);
        video.Tique(); Wait(() => presos > antes);
        Check(alvoPreso == 1347033 && !old.SeekPending.Task.IsCompleted && old.PositionCalls == 0,
              "seek sem conclusao vence prazo pausado sem consulta de posicao");
        Entry("fPausar", 0); Entry("fBuscar", 1357033); Thread.Sleep(50);
        Check(presos == antes + 1 && old.Starts == 1 && old.SeekCalls == 1,
              "seek expirado nao aceita comandos nem repete recuperacao");
        Entry("fParar"); Open(); Wait(() => Player.All.Count == 2 && Player.All[1].Starts == 1);
        Check(old.Disposed && Player.Overlap == 0, "seek preso libera antes de reabrir");
        Entry("fPausar", 1); Wait(() => Player.All[1].State == PlayerState.Paused);
        Entry("fPausar", 0); Wait(() => Player.All[1].Starts == 2);
        Check(Player.All[1].State == PlayerState.Playing, "nova sessao aceita pausa e resume");
        old.SeekPending.SetResult(true); Thread.Sleep(50);
        Check(Player.All[1].Starts == 2, "seek tardio nao altera nova sessao");
        Entry("fParar"); Wait(() => Player.All[1].Disposed);

        // #412 P1-c (log AS0D80): player preso recusa mute/display/estado com
        // InvalidOperation e o Dispose falha uma vez. A primeira reabertura vira
        // EV_ERRO -3; a seguinte tenta o Dispose de novo e chega a sessao nova.
        Setup(); OpenTocado(); old = Player.All[0]; old.Wedged = true; old.FailDisposeTimes = 1;
        Open(); Wait(() => Erros == 1);
        Check(Player.All.Count == 1 && !old.Disposed, "preso: Dispose recusado nao abre outro player");
        Open(); Wait(() => Player.All.Count == 2 && Player.All[1].Starts == 1);
        Check(old.Disposed && Player.Overlap == 0 && Erros == 1, "preso: nova tentativa do Dispose libera e a sessao nova toca");
        Entry("fParar"); Wait(() => Player.All.Count < 2 || Player.All[1].Disposed);
        // Stop/Unprepare InvalidOperation nao impedem Dispose nem a nova sessao.
        Setup(); OpenTocado(); old = Player.All[0]; old.FailStop = old.FailUnprepare = true;
        Open(); Wait(() => Player.All.Count == 2 && Player.All[1].Starts == 1);
        Check(old.StopCalls == 0 && old.UnprepareCalls == 0 && old.Disposed && Player.Overlap == 0,
              "direct Dispose avoids Stop/Unprepare and releases before reopening");
        Entry("fParar"); Wait(() => Player.All[1].Disposed);
        // Sempre recusado: cada reabertura so devolve EV_ERRO -3 (o C limita a 3).
        Setup(); OpenTocado(); old = Player.All[0]; old.Wedged = true; old.FailDisposeTimes = 1000;
        for (int i = 1; i <= 3; i++) { Open(); Wait(() => Erros == i); }
        Check(Player.All.Count == 1 && Player.Overlap == 0, "preso sempre: nenhuma abertura, um erro por tentativa");
        old.FailDisposeTimes = 0; old.Wedged = false;
        // P2: seek pausado concluido publica o alvo no relogio.
        Setup(); OpenTocado(); old = Player.All[0];
        Entry("fPausar", 1); Wait(() => old.State == PlayerState.Paused);
        Entry("fBuscar", 1200000); Wait(() => (int)EntryPos() == 1200000);
        Check((int)EntryPos() == 1200000 && old.State == PlayerState.Paused && old.PositionCalls == 0, "seek pausado atualiza relogio sem consultar o player");
        Entry("fParar"); Wait(() => old.Disposed);
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
        public IntPtr Handle => new IntPtr(1);
        public static List<Player> All = new List<Player>(); public static bool Defer; public static int Overlap;
        public TaskCompletionSource<bool> Prepared = new TaskCompletionSource<bool>();
        public Player() { if(All.Exists(p=>!p.Disposed)) Overlap++; All.Add(this); }
        public bool Disposed, FailDispose, BlockDispose, BlockStop, FailStop, FailUnprepare;
        public ManualResetEventSlim StopEntered = new ManualResetEventSlim(), StopRelease = new ManualResetEventSlim(); public int Starts, DisposeCalls, UnprepareCalls, StopCalls;
        public ManualResetEventSlim DisposeEntered = new ManualResetEventSlim(), DisposeRelease = new ManualResetEventSlim();
        public bool FailPause, FailSeek, FailState; public int SeekCalls; public TaskCompletionSource<bool> SeekPending;
        public bool Wedged; public int FailDisposeTimes;
        void Wedge(string o) { if(Wedged) throw new InvalidOperationException("Failed to " + o + " : InvalidOperation."); }
        PlayerState state; public PlayerState State {get { if(FailState) throw new InvalidOperationException("GetState InvalidOperation"); Wedge("retrieve the state of the player"); return state; } set { state=value; }}
        bool muted; public bool Muted {get=>muted; set { Wedge("set the mute state of the player"); muted=value; }} public float Volume {get;set;}
        Display display; public Display Display {get=>display; set { Wedge("set the display"); display=value; }} public string UserAgent,Cookie;
        public Info AudioTrackInfo = new Info(), SubtitleTrackInfo = new Info(); public Stream StreamInfo = new Stream(); public Settings DisplaySettings = new Settings();
        public event EventHandler<Event> PlaybackCompleted, ErrorOccurred, BufferingProgressChanged, PlaybackInterrupted, SubtitleUpdated;
        public void SetSource(MediaUriSource s) {} public void SetSubtitleOffset(int n) {} public void SetPlaybackRate(float n) {}
        public async Task PrepareAsync() { State=PlayerState.Preparing; if(Defer) await Prepared.Task; if(!Disposed) State=PlayerState.Ready; }
        public void Start() { Starts++; State=PlayerState.Playing; } public void Stop() { StopCalls++; StopEntered.Set(); if(BlockStop) StopRelease.Wait(); if(FailStop) throw new InvalidOperationException("stop InvalidOperation"); State=PlayerState.Ready; }
        public void Pause() { if(FailPause) throw new InvalidOperationException("pause InvalidOperation"); State=PlayerState.Paused; } public void Unprepare() { UnprepareCalls++; if(State != PlayerState.Ready && State != PlayerState.Playing && State != PlayerState.Paused) throw new InvalidOperationException("unprepare em " + State); if(FailUnprepare) throw new InvalidOperationException("unprepare InvalidOperation"); State=PlayerState.Idle; }
        public void Dispose() { DisposeCalls++; DisposeEntered.Set(); if(BlockDispose) DisposeRelease.Wait(); if(FailDispose) throw new Exception("dispose recusado"); if(FailDisposeTimes>0) { FailDisposeTimes--; throw new InvalidOperationException("dispose InvalidOperation"); } Disposed=true; }
        public int PositionDelayMs, PositionCalls;
        public ManualResetEventSlim PositionEntered = new ManualResetEventSlim();
        public int GetPlayPosition() { Interlocked.Increment(ref PositionCalls); PositionEntered.Set(); Thread.Sleep(PositionDelayMs); return 1234; }
        public void Complete(bool ended) { if (ended) State = PlayerState.Ready; PlaybackCompleted?.Invoke(this, new Event()); }
        public void Interromper() { State=PlayerState.Paused; PlaybackInterrupted?.Invoke(this, new Event()); }
        public Task SetPlayPositionAsync(int ms,bool precise) { SeekCalls++; if(FailSeek) throw new InvalidOperationException("seek InvalidOperation"); return SeekPending?.Task ?? Task.CompletedTask; }
    }
}
