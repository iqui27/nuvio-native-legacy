// Executes the real Voz.cs against NUI event/failure doubles and a native bridge.
// Program's lifecycle methods are extracted unchanged by tpk_voz_host.sh.
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Tizen.NUI;
using Tizen.NUI.BaseComponents;

static class VozHostTest
{
    [DllImport("libnuvio.so")] static extern void host_start();
    [DllImport("libnuvio.so")] static extern void host_stop();
    [DllImport("libnuvio.so")] static extern void host_reset();
    [DllImport("libnuvio.so")] static extern int host_count();
    [DllImport("libnuvio.so")] static extern int host_result();
    [DllImport("libnuvio.so")] static extern IntPtr host_value();
    static int failures;
    static void Check(bool ok, string message) { if (!ok) throw new Exception(message); }
    static void Run(string name, Action test)
    {
        try { Fault.At = ""; FocusManager.Ok = true; host_reset(); Timer.All.Clear(); test(); Console.WriteLine("PASS: " + name); }
        catch (Exception e) { failures++; Console.WriteLine("FAIL: " + name + ": " + e.Message); }
    }
    static NuvioTpk.Voz New(Action<Action> post = null) => new NuvioTpk.Voz(post ?? (a => a()), s => { });
    static void Cancelled(int result = 0)
    {
        Check(host_count() == 1 && host_result() == result, "must send exactly one completion with the failure reason");
        Check(!TextField.Last.Focusable, "field left focusable");
        TextField.Last.Imf.Status(false);
        Timer.Fire();
        Check(host_count() == 1, "duplicate completion after cancellation");
    }
    static int Main(string[] args)
    {
        NativeLibrary.SetDllImportResolver(typeof(VozHostTest).Assembly, (name, asm, path) => NativeLibrary.Load(args[0]));
        Run("focus refusal reports failure", () => { var v = New(); FocusManager.Ok = false; host_start(); Cancelled(-1); GC.KeepAlive(v); });
        Run("zero frames cannot keep startup black despite live draw callbacks", () => {
            var p = new NuvioTpk.Program(null);
            p.DrawAt(14999); p.WatchAt(14999); Check(p.Error == null, "startup deadline too short");
            p.DrawAt(15000); p.WatchAt(15000); Check(p.Error != null, "no first-frame fallback");
        });
        Run("native startup failure remains visible", () => {
            var p = new NuvioTpk.Program(null); p.FrameResult = -1;
            p.DrawAt(100); p.WatchAt(15000); Check(p.Error != null, "native failure silently exited");
        });
        Run("native callback exception reaches startup fallback", () => {
            var p = new NuvioTpk.Program(null); Fault.At = "native-frame";
            p.DrawAt(100); p.WatchAt(15000); Check(p.Error != null, "native exception escaped diagnostic path");
        });
        Run("successful first frame clears startup deadline", () => {
            var p = new NuvioTpk.Program(null); p.FrameResult = 1;
            p.DrawAt(100); p.FrameResult = 0; p.DrawAt(16000); p.WatchAt(16000);
            Check(p.Error == null, "healthy startup showed fatal fallback");
        });
        Run("startup never constructs optional Samsung IME", () => {
            TextField.Last = null; Fault.At = "field-new";
            var v = New();
            Check(TextField.Last == null && Fault.At == "field-new", "startup called TextField before first frame");
            host_start();
            Check(host_count() == 1 && host_result() == 0, "unavailable IME must cancel, not block startup");
            GC.KeepAlive(v);
        });
        Run("denied optional microphone query keeps startup usable", () => {
            Fault.At = "feature"; var v = New();
            Check(Fault.At == "feature", "startup queried optional microphone capability");
            host_start(); Check(host_count() == 0, "denied capability prevented keyboard opening");
            v.Cancelar(); Cancelled(); GC.KeepAlive(v);
        });
        Run("missing InputMethodContext cancels and permits retry", () => {
            var v = New(); Fault.At = "context"; host_start();
            Check(host_count() == 1 && host_result() == 0, "missing context did not cancel");
            host_reset(); host_start(); Check(host_count() == 0, "IME retry failed");
            v.Cancelar(); Cancelled(); GC.KeepAlive(v);
        });
        foreach (string op in new[] { "text-set", "focus", "activate", "show", "timer-new", "timer-start" })
            Run(op + " exception reports failure", () => { var v = New(); Fault.At = op; host_start(); Cancelled(-1); GC.KeepAlive(v); });
        Run("focusable exception cancels during lazy mount", () => { var v = New(); Fault.At = "focusable"; host_start(); Cancelled(); GC.KeepAlive(v); });
        Run("missing panel times out and can reopen", () => {
            var v = New(); host_start(); Check(host_count() == 0, "cancelled before deadline");
            Check(Timer.All.Exists(t => t.Running && t.Interval > 0 && t.Interval <= 10000), "no bounded opening timer");
            Timer.Fire(); Cancelled(-1); host_reset(); host_start();
            TextField.Last.Imf.Status(true); TextField.Last.Text = "duna"; TextField.Last.Imf.Status(false);
            Check(host_count() == 1 && host_result() == 1, "reopening failed"); GC.KeepAlive(v);
        });
        Run("visible panel does not time out; auto-hide still accepts", () => {
            var v = New(); host_start(); TextField.Last.Imf.Status(true); Timer.Fire();
            Check(host_count() == 0, "deadline cancelled visible panel"); TextField.Last.Text = "  matrix  ";
            TextField.Last.Imf.Status(false);
            Check(host_count() == 1 && host_result() == 1 && Marshal.PtrToStringUTF8(host_value()) == "matrix", "auto-hide changed"); GC.KeepAlive(v);
        });
        Run("Back cancels text", () => { var v = New(); host_start(); TextField.Last.Imf.Status(true); TextField.Last.Text = "matrix"; TextField.Last.Press("XF86Back"); Cancelled(); GC.KeepAlive(v); });
        Run("C stop hides without callback", () => { var v = New(); host_start(); host_stop(); Timer.Fire(); Check(host_count() == 0 && !TextField.Last.Focusable, "stop emitted completion or left field active"); GC.KeepAlive(v); });
        Run("OnPause cancels before system hide; resume permits another session", () => {
            var v = New(); var p = new NuvioTpk.Program(v); host_start(); TextField.Last.Imf.Status(true); TextField.Last.Text = "private";
            p.Pause(); Cancelled(); host_reset(); p.Resume(); host_start(); TextField.Last.Imf.Status(true); TextField.Last.Text = "duna"; TextField.Last.Imf.Status(false);
            Check(host_count() == 1 && host_result() == 1, "resume failed");
        });
        Run("window teardown cancels before hide", () => { var v = New(); var p = new NuvioTpk.Program(v); host_start(); TextField.Last.Imf.Status(true); TextField.Last.Text = "private"; p.Close(); Cancelled(); });
        Run("queued opening cannot survive pause/resume", () => {
            TextField.Last = null;
            var queue = new Queue<Action>(); var v = New(a => queue.Enqueue(a)); var p = new NuvioTpk.Program(v);
            host_start(); p.Pause(); p.Resume(); while (queue.Count > 0) queue.Dequeue()();
            Check(host_count() == 1 && host_result() == 0, "queued request was not cancelled");
            Check(TextField.Last == null, "cancelled request constructed IME");
        });
        return failures == 0 ? 0 : 1;
    }
}
static class Fault
{
    internal static string At;
    internal static void Hit(string op) { if (At == op) { At = ""; throw new Exception(op); } }
}
namespace Tizen.System
{
    static class Information { public static bool TryGetValue<T>(string key, out T value) { Fault.Hit("feature"); value = default(T); return false; } }
}
namespace Tizen.NUI
{
    class Size2D { public Size2D(int w, int h) { } }
    class Position2D { public Position2D(int x, int y) { } }
    class Key { public enum StateType { Down, Up } public StateType State; public string KeyPressedName; }
    class KeyArgs : EventArgs { public Key Key; }
    delegate bool KeyHandler(object sender, KeyArgs e);
    class FocusManager
    {
        public static FocusManager Instance = new FocusManager(); public static bool Ok = true; object current;
        public bool SetCurrentFocusView(object view) { Fault.Hit("focus"); if (Ok) current = view; return Ok; }
        public object GetCurrentFocusView() => current;
        public void ClearFocus() { current = null; }
    }
    class InputMethodContext
    {
        public class StatusArgs : EventArgs { public bool StatusChanged; }
        public event EventHandler<StatusArgs> StatusChanged;
        public void Status(bool on) { StatusChanged?.Invoke(this, new StatusArgs { StatusChanged = on }); }
        public void Activate() { Fault.Hit("activate"); }
        public void ShowInputPanel() { Fault.Hit("show"); }
        public void HideInputPanel() { Status(false); }
        public void Deactivate() { }
    }
    class Timer : IDisposable
    {
        public static List<Timer> All = new List<Timer>(); public uint Interval; public bool Running;
        public Timer(uint interval) { Fault.Hit("timer-new"); Interval = interval; All.Add(this); }
        public class TickEventArgs : EventArgs { }
        public delegate bool TickEventHandler(object sender, TickEventArgs e);
        public event TickEventHandler Tick;
        public void Start() { Fault.Hit("timer-start"); Running = true; }
        public void Stop() { Running = false; }
        public void Dispose() { Running = false; }
        public static void Fire() { foreach (var t in All.ToArray()) if (t.Running && t.Tick?.Invoke(t, new TickEventArgs()) != true) t.Running = false; }
    }
    class Window
    {
        public static Window Instance = new Window(); public void Add(object view) { } public void Remove(object view) { }
        public void Hide() { TextField.Last?.Imf.Status(false); }
    }
    class GLWindow { public void Hide() { Window.Instance.Hide(); } }
    class GLView { public void Hide() { Window.Instance.Hide(); } }
    class NUIApplication
    {
        protected virtual void OnPause() { Window.Instance.Hide(); }
        protected virtual void OnResume() { }
    }
}
namespace Tizen.NUI.BaseComponents
{
    class TextField
    {
        public static TextField Last; public InputMethodContext Imf = new InputMethodContext();
        public TextField() { Fault.Hit("field-new"); Last = this; }
        public void Dispose() { }
        public Size2D Size2D; public Position2D Position2D; public float Opacity; public bool EnableSelection;
        bool focusable; string text = "";
        public bool Focusable { get => focusable; set { Fault.Hit("focusable"); focusable = value; } }
        public string Text { get => text; set { Fault.Hit("text-set"); text = value; TextChanged?.Invoke(this, EventArgs.Empty); } }
        public event EventHandler TextChanged; public event KeyHandler KeyEvent;
        public InputMethodContext GetInputMethodContext() { if (Fault.At == "context") { Fault.At = ""; return null; } return Imf; }
        public void Press(string key) { KeyEvent?.Invoke(this, new KeyArgs { Key = new Key { State = Key.StateType.Down, KeyPressedName = key } }); }
    }
}
