// Mesma tela de resultado do NvSpikeNui, para Tizen 4.x (TVs 2018-2020): sem
// GLView nem GLWindow nessa API, entao o teste 3 fica "N/D". O relatorio sobe
// sozinho logo depois dos testes (Relatorio.cs). Voltar sai.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using ElmSharp;
using NvSpike;
using Tizen.Applications;
using Tizen.System;

namespace NvSpikeLegacy
{
    class Program : CoreUIApplication
    {
        [DllImport("libnvspike.so")] static extern int nv_ping();
        [DllImport("libnvspike.so")] static extern int nv_thread_test();
        [DllImport("libc", SetLastError = true)] static extern int chmod(string path, int mode);
        [DllImport("libdl.so.2")] static extern IntPtr dlopen(string path, int flags);
        [DllImport("libdl.so.2")] static extern IntPtr dlerror();
        const int RTLD_NOW = 2, RTLD_GLOBAL = 0x100;
        string preCarga = "";

        const string Pacote = "NvSpikeLegacy";
        Window janela;
        Box coluna;
        string modelo = "?";
        readonly List<string> linhas = new List<string>();
        readonly List<Label> rotulos = new List<Label>();

        protected override void OnCreate()
        {
            base.OnCreate();

            janela = new Window("NvSpikeLegacy");
            janela.BackButtonPressed += (s, e) => Exit();
            var fundo = new Background(janela) { Color = Color.Black };
            fundo.Show();
            janela.AddResizeObject(fundo);

            coluna = new Box(janela) { AlignmentX = -1, AlignmentY = 0, WeightX = 1, WeightY = 1 };
            coluna.Show();
            janela.AddResizeObject(coluna);
            janela.Show();

            Linha($"Nuvio spike .tpk ({Pacote}) - codigo {Relatorio.Codigo}");
            Linha(TestaPing());
            Linha(TestaThread());
            Linha("3. GLView/GLWindow + .so: N/D (nao existem no Tizen 4.x)");
            Linha(TestaSpikebin());
            Linha(TestaVersao());
            int idxEnvio = Linha(Relatorio.Ligado ? "Enviando relatorio..." : "Envio desligado neste build: mande uma foto desta tela.");
            Linha("Voltar sai do app.");
            if (Relatorio.Ligado) Envia(idxEnvio);
        }

        async void Envia(int idx)
        {
            string r = await Relatorio.Enviar(Pacote, modelo, linhas.GetRange(0, idx));
            linhas[idx] = r;
            rotulos[idx].Text = Marcado(r);
        }

        int Linha(string texto)
        {
            var l = new Label(janela) { Text = Marcado(texto), AlignmentX = -1, WeightX = 1, LineWrapType = WrapType.Word };
            l.Show();
            coluna.PackEnd(l);
            linhas.Add(texto);
            rotulos.Add(l);
            return linhas.Count - 1;
        }

        // Label do Elementary interpreta markup: mensagem de excecao com '<'
        // ou '&' sumiria ou quebraria a linha.
        static string Marcado(string t) =>
            "<font_size=26 color=#FFFFFF>" + t.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;") + "</font>";

        // O dotnet-launcher do Tizen 4/5 nao poe o lib/ do pacote na busca do
        // DllImport (a TV QE65Q80RATXXC tentou so "liblibnvspike.so.so"). Com a
        // .so ja aberta por caminho absoluto, o dlopen("libnvspike.so") do
        // DllImport casa pelo soname (-Wl,-soname no tools/tizen-tpk-spike.sh).
        void PreCarrega()
        {
            string raiz = System.IO.Path.GetFullPath(System.IO.Path.Combine(
                Tizen.Applications.Application.Current.DirectoryInfo.Resource, ".."));
            var erros = new List<string>();
            foreach (var sub in new[] { "lib", "bin", "lib/armel", "bin/runtimes/linux-armel/native" })
            {
                string p = System.IO.Path.Combine(raiz, sub, "libnvspike.so");
                if (!System.IO.File.Exists(p)) continue;
                if (dlopen(p, RTLD_NOW | RTLD_GLOBAL) != IntPtr.Zero) { preCarga = "via " + sub + "/"; return; }
                erros.Add(sub + ": " + Marshal.PtrToStringAnsi(dlerror()));
            }
            preCarga = erros.Count > 0 ? "dlopen falhou: " + string.Join("; ", erros)
                                       : "libnvspike.so nao achada em " + raiz;
        }

        string TestaPing()
        {
            try { PreCarrega(); }
            catch (Exception e) { preCarga = $"pre-carga: {e.GetType().Name}: {e.Message}"; }
            try
            {
                int r = nv_ping();
                return r == 42 ? $"1. DllImport (.so propria): OK ({preCarga})" : $"1. DllImport: FALHOU (nv_ping devolveu {r}, esperava 42)";
            }
            catch (Exception e) { return $"1. DllImport: FALHOU ({e.GetType().Name}: {e.Message}) [{preCarga}]"; }
        }

        string TestaThread()
        {
            try
            {
                return nv_thread_test() != 0 ? "2. pthread na .so: OK" : "2. pthread na .so: FALHOU (thread nao rodou)";
            }
            catch (Exception e) { return $"2. pthread na .so: FALHOU ({e.GetType().Name}: {e.Message})"; }
        }

        string TestaSpikebin()
        {
            try
            {
                var dir = Tizen.Applications.Application.Current.DirectoryInfo;
                string exe = System.IO.Path.Combine(dir.Data, "spikebin");
                System.IO.File.Copy(System.IO.Path.Combine(dir.Resource, "..", "lib", "spikebin"), exe, true);
                if (chmod(exe, 0x1ED) != 0) return $"4. Process.Start(spikebin): FALHOU (chmod errno={Marshal.GetLastWin32Error()})";
                string destino = System.IO.Path.Combine(dir.Data, "spikebin.txt");
                System.IO.File.Delete(destino);
                using (var p = Process.Start(new ProcessStartInfo(exe, destino) { UseShellExecute = false }))
                {
                    if (!p.WaitForExit(5000)) return "4. Process.Start(spikebin): FALHOU (nao terminou em 5 s)";
                    bool escreveu = System.IO.File.Exists(destino);
                    return (p.ExitCode == 0 && escreveu)
                        ? "4. Process.Start(spikebin): OK"
                        : $"4. Process.Start(spikebin): FALHOU (exit={p.ExitCode}, escreveu={escreveu})";
                }
            }
            catch (Exception e) { return $"4. Process.Start(spikebin): FALHOU ({e.GetType().Name}: {e.Message})"; }
        }

        string TestaVersao()
        {
            try
            {
                Information.TryGetValue<string>("http://tizen.org/feature/platform.version", out string versao);
                Information.TryGetValue<string>("http://tizen.org/system/model_name", out string m);
                if (!string.IsNullOrEmpty(m)) modelo = m;
                return $"5. Tizen {versao ?? "?"} / modelo {modelo} / {RuntimeInformation.FrameworkDescription}";
            }
            catch (Exception e) { return $"5. Versao/modelo: FALHOU ({e.GetType().Name}: {e.Message})"; }
        }

        static void Main(string[] args)
        {
            Elementary.Initialize();
            Elementary.ThemeOverlay();
            var app = new Program();
            app.Run(args);
        }
    }
}
