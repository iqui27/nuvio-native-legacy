// .NET ERRORS IN THE APP LOG (owner 28/10). The native crash reporter
// (src/queda.c) records every fault, and a fault in C# code is tagged ".NET":
// CoreCLR turns it into a NullReferenceException inside C#. Without this file
// the log never says WHICH C# line it was, and an exception nobody catches only
// reached the startup trace. Here, into the same log as the C side:
//   - unhandled exceptions (and unobserved task exceptions), whole stack;
//   - the first 20 NullReference / AccessViolation exceptions of the session
//     (first-chance: the moment they are thrown, caught or not), with the C#
//     stack of the throwing thread.
// Every line goes through nv_tpk_log_net (src/tpk.c), which REDACTS it with
// the log page's rules (URLs cut at the host, keys / tokens / passwords /
// cookies, long token-like strings) and stamps the session time the native
// fault records use ("[net t=12.345s] ..."). Nothing is written here unredacted:
// an older libnuvio without that export gets no .NET lines at all. Never throws.
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

namespace NuvioTpk
{
    static class NetLog
    {
        [DllImport("libnuvio.so")] static extern void nv_tpk_log_net(string linha);

        const int PrimeirasMax = 20;
        [ThreadStatic] static bool dentro;   // logging must never log itself
        static int primeiras;
        static volatile bool semNet;

        public static void Registrar()
        {
            AppDomain.CurrentDomain.UnhandledException += (s, e) =>
                Erro(e.IsTerminating ? "unhandled (fatal)" : "unhandled", e.ExceptionObject as Exception, e.ExceptionObject);
            TaskScheduler.UnobservedTaskException += (s, e) => Erro("unobserved task", e.Exception, null);
            AppDomain.CurrentDomain.FirstChanceException += (s, e) => PrimeiraChance(e.Exception);
        }

        static void Linha(string l)
        {
            if (semNet) return;
            try { nv_tpk_log_net(l); }
            catch (EntryPointNotFoundException) { semNet = true; }   // older lib: no unredacted fallback
            catch (Exception) { }                                    // lib not loaded yet
        }

        static void Erro(string onde, Exception e, object bruto)
        {
            if (dentro) return;
            dentro = true;
            try
            {
                if (e == null) { Linha(onde + ": " + (bruto != null ? bruto.ToString() : "?")); return; }
                Linha(onde + ": " + e.GetType().FullName + ": " + e.Message + " thread " + QualFio());
                string[] linhas = e.ToString().Split('\n');
                for (int i = 1; i < linhas.Length && i <= 40; i++)
                {
                    string l = linhas[i].Trim();
                    if (l.Length > 0) Linha("  " + l);
                }
            }
            catch (Exception) { }
            finally { dentro = false; }
        }

        static void PrimeiraChance(Exception e)
        {
            if (dentro || !(e is NullReferenceException || e is AccessViolationException)) return;
            int n = Interlocked.Increment(ref primeiras);
            if (n > PrimeirasMax) return;
            dentro = true;
            try
            {
                Linha("first-chance #" + n + " " + e.GetType().Name + ": " + e.Message + " thread " + QualFio() +
                      (n == PrimeirasMax ? " (last one logged this session)" : ""));
                // The exception has no stack yet (it was just thrown): the
                // throwing thread's own stack, from the frame that threw.
                StackFrame[] quadros = new StackTrace(1, false).GetFrames();
                int k = 0;
                if (quadros != null)
                    foreach (StackFrame q in quadros)
                    {
                        var m = q.GetMethod();
                        if (m == null) continue;
                        Linha("  at " + (m.DeclaringType != null ? m.DeclaringType.FullName + "." : "") + m.Name);
                        if (++k >= 16) break;
                    }
            }
            catch (Exception) { }
            finally { dentro = false; }
        }

        static string QualFio()
        {
            var t = Thread.CurrentThread;
            return t.ManagedThreadId + (string.IsNullOrEmpty(t.Name) ? "" : " \"" + t.Name + "\"");
        }
    }
}
