// Tela de resultado do spike, fase 0 do plano. Uma linha por teste, OK/FALHOU
// + mensagem. Depois do veredito dos testes GL (3 s) o relatorio sobe sozinho
// (Relatorio.cs); se nao subir, a tela pede foto. Voltar sai.
// Window.Instance e nao Window.Default: Instance existe nas APIs antigas, e o
// alvo sao TVs antigas.
#pragma warning disable CS0618
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using NvSpike;
using Tizen.NUI;
using Tizen.NUI.BaseComponents;
using Tizen.System;

namespace NvSpikeNui
{
    class Program : NUIApplication
    {
        [DllImport("libnvspike.so")] static extern int nv_ping();
        [DllImport("libnvspike.so")] static extern int nv_thread_test();
        [DllImport("libnvspike.so")] static extern int nv_draw(int ctx, int w, int h, float t);
        [DllImport("libnvspike.so")] static extern int nv_quadros(int ctx);
        [DllImport("libc", SetLastError = true)] static extern int chmod(string path, int mode);

#if NV_API8
        const string Pacote = "NvSpikeNui60";
#elif NV_API9
        const string Pacote = "NvSpikeNui65";
#else
        const string Pacote = "NvSpikeNui";
        GLView glView;
#endif
        readonly Stopwatch relogio = new Stopwatch();
        View coluna;
        string modelo = "?";

        // Linhas na ordem da tela; as dos testes GL sao substituidas pelo
        // veredito antes do envio.
        readonly List<string> linhas = new List<string>();
        readonly List<TextLabel> rotulos = new List<TextLabel>();
        int idxGLView = -1, idxGLWindow = -1, idxEnvio = -1;

        protected override void OnCreate()
        {
            base.OnCreate();

            Window janela = Window.Instance;
            janela.BackgroundColor = Color.Black;
            janela.KeyEvent += AoTeclar;

            coluna = new View
            {
                Layout = new LinearLayout { LinearOrientation = LinearLayout.Orientation.Vertical, CellPadding = new Size2D(0, 8) },
                WidthSpecification = LayoutParamPolicies.MatchParent,
                HeightSpecification = LayoutParamPolicies.MatchParent,
                Padding = new Extents(40, 40, 40, 40),
            };
            janela.Add(coluna);

            Linha($"Nuvio spike .tpk ({Pacote}) - codigo {Relatorio.Codigo}");
            Linha(TestaPing());
            Linha(TestaThread());
            AdicionaGLView();
            AdicionaGLWindow();
            Linha(TestaSpikebin());
            Linha(TestaVersao());
            idxEnvio = Linha(Relatorio.Ligado ? "Enviando relatorio em 3 s..." : "Envio desligado neste build: mande uma foto desta tela.");
            Linha("Voltar sai do app.");
            AgendaVeredito();
        }

        int Linha(string texto)
        {
            var l = new TextLabel(texto) { TextColor = Color.White, PointSize = 16, MultiLine = true, WidthSpecification = 1840 };
            coluna.Add(l);
            linhas.Add(texto);
            rotulos.Add(l);
            return linhas.Count - 1;
        }

        void Troca(int i, string texto)
        {
            if (i < 0) return;
            linhas[i] = texto;
            rotulos[i].Text = texto;
        }

        void AoTeclar(object s, Window.KeyEventArgs e)
        {
            if (e.Key.State == Key.StateType.Down && (e.Key.KeyPressedName == "XF86Back" || e.Key.KeyPressedName == "Escape"))
                Exit();
        }

        // Teste 1: DllImport da .so propria.
        string TestaPing()
        {
            try
            {
                int r = nv_ping();
                return r == 42 ? "1. DllImport (.so propria): OK" : $"1. DllImport: FALHOU (nv_ping devolveu {r}, esperava 42)";
            }
            catch (Exception e) { return $"1. DllImport: FALHOU ({e.GetType().Name}: {e.Message})"; }
        }

        // Teste 2: pthread dentro da .so.
        string TestaThread()
        {
            try
            {
                return nv_thread_test() != 0 ? "2. pthread na .so: OK" : "2. pthread na .so: FALHOU (thread nao rodou)";
            }
            catch (Exception e) { return $"2. pthread na .so: FALHOU ({e.GetType().Name}: {e.Message})"; }
        }

        // Resultado dos testes GL so existe depois de alguns quadros: as
        // linhas nascem "aguardando" e um Timer de 3 s escreve o veredito
        // (quadros desenhados pela .so + ultimo glGetError) e dispara o envio.
        string erroGL0, erroGL1;
        int glErr0, glErr1;
        Timer veredito;

        void AgendaVeredito()
        {
            relogio.Start();
            veredito = new Timer(3000);
            veredito.Tick += (s, e) =>
            {
                if (idxGLView >= 0) Troca(idxGLView, Veredito("3. GLView + .so", 0, erroGL0, glErr0));
                if (idxGLWindow >= 0) Troca(idxGLWindow, Veredito("3b. GLWindow + .so", 1, erroGL1, glErr1));
                Envia();
                return false;
            };
            veredito.Start();
        }

        async void Envia()
        {
            if (!Relatorio.Ligado) return;
            var copia = new List<string>(linhas);
            copia.RemoveRange(idxEnvio, copia.Count - idxEnvio);
            string r = await Relatorio.Enviar(Pacote, modelo, copia);
            Troca(idxEnvio, r);
        }

        static string Veredito(string nome, int ctx, string erro, int glErr)
        {
            if (erro != null) return $"{nome}: FALHOU ({erro})";
            int n;
            try { n = nv_quadros(ctx); } catch (Exception e) { return $"{nome}: FALHOU ({e.GetType().Name}: {e.Message})"; }
            if (n <= 0) return $"{nome}: FALHOU (callback de render nunca chamou a .so)";
            return glErr == 0 ? $"{nome}: OK ({n} quadros em 3 s)" : $"{nome}: FALHOU (glGetError=0x{glErr:X}, {n} quadros)";
        }

        int Desenha(int ctx, int w, int h, ref string erro, ref int glErr)
        {
            try { glErr = nv_draw(ctx, w, h, (float)relogio.Elapsed.TotalSeconds); }
            catch (Exception e) { erro ??= $"{e.GetType().Name}: {e.Message}"; }
            return 1;
        }

#if NV_API8 || NV_API9
        void AdicionaGLView() => Linha("3. GLView + .so: N/D (GLView nao existe antes da API11 / Tizen 8)");
#else
        // Teste 3: GLView (widget GL dentro da arvore NUI) desenhando pela .so.
        void AdicionaGLView()
        {
            try
            {
                glView = new GLView(GLView.ColorFormat.RGBA8888) { WidthSpecification = 320, HeightSpecification = 180 };
                glView.RegisterGLCallbacks(() => { }, () => Desenha(0, 320, 180, ref erroGL0, ref glErr0), () => { });
                glView.RenderingMode = GLRenderingMode.Continuous;
                coluna.Add(glView);
                idxGLView = Linha("3. GLView + .so: aguardando quadros...");
            }
            catch (Exception e)
            {
                Linha($"3. GLView + .so: FALHOU ({e.GetType().Name}: {e.Message})");
            }
        }
#endif

        // Teste 3b: GLWindow (janela GLES inteira, o modelo do SDL de hoje),
        // num retangulo no canto para nao cobrir a tela de resultado.
        GLWindow glWindow;
        void AdicionaGLWindow()
        {
            try
            {
                glWindow = new GLWindow("nvspike-gl", new Rectangle(1400, 780, 480, 270), true);
                // A API do GLWindow mudou entre versoes (conferido por reflexao
                // nos pacotes de referencia):
                //   API8:  SetEglConfig(.., GLWindow.GLESVersion.Version_2_0),
                //          RegisterGlCallback com quadro void, sem RenderingMode
                //          (continuo e o padrao do DALi)
                //   API9:  SetEglConfig(.., GLESVersion.Version20),
                //          RegisterGlCallback com quadro int, RenderingMode
                //   API11: SetGraphicsConfig(.., GLESVersion.Version20),
                //          RegisterGLCallbacks com quadro int, RenderingMode
#if NV_API8
                glWindow.SetEglConfig(false, false, 0, GLWindow.GLESVersion.Version_2_0);
                glWindow.RegisterGlCallback(() => { }, () => { Desenha(1, 480, 270, ref erroGL1, ref glErr1); }, () => { });
#elif NV_API9
                glWindow.SetEglConfig(false, false, 0, GLESVersion.Version20);
                glWindow.RegisterGlCallback(() => { }, () => Desenha(1, 480, 270, ref erroGL1, ref glErr1), () => { });
                glWindow.RenderingMode = GLRenderingMode.Continuous;
#else
                glWindow.SetGraphicsConfig(false, false, 0, GLESVersion.Version20);
                glWindow.RegisterGLCallbacks(() => { }, () => Desenha(1, 480, 270, ref erroGL1, ref glErr1), () => { });
                glWindow.RenderingMode = GLRenderingMode.Continuous;
#endif
                glWindow.KeyEvent += (s, e) =>
                {
                    if (e.Key.State == Key.StateType.Down && (e.Key.KeyPressedName == "XF86Back" || e.Key.KeyPressedName == "Escape"))
                        Exit();
                };
                glWindow.Show();
                idxGLWindow = Linha("3b. GLWindow + .so: aguardando quadros...");
            }
            catch (Exception e)
            {
                Linha($"3b. GLWindow + .so: FALHOU ({e.GetType().Name}: {e.Message})");
            }
        }

        // Teste 4: Process.Start do spikebin ARMv7 estatico.
        string TestaSpikebin()
        {
            try
            {
                // lib/ do pacote e so leitura e o zip nao garante o bit de
                // execucao: copia para data/ e chmod, como o tailscale-tizen.
                string exe = System.IO.Path.Combine(DirectoryInfo.Data, "spikebin");
                System.IO.File.Copy(System.IO.Path.Combine(DirectoryInfo.Resource, "..", "lib", "spikebin"), exe, true);
                if (chmod(exe, 0x1ED) != 0) return $"4. Process.Start(spikebin): FALHOU (chmod errno={Marshal.GetLastWin32Error()})";
                string destino = System.IO.Path.Combine(DirectoryInfo.Data, "spikebin.txt");
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

        // Teste 5: versao do Tizen, modelo e runtime .NET; informativo.
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
            var app = new Program();
            app.Run(args);
        }
    }
}
