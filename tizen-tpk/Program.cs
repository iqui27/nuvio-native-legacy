// Host .NET do Nuvio .tpk no Tizen 6+.  (API11: GLView na janela principal, ver API11_GLVIEW.) Nao tem tela propria: abre um GLWindow
// de tela cheia e, a cada quadro, entrega o contexto GL ao C (libnuvio.so,
// src/tpk.c), que roda o app inteiro num fio seu. Teclas do controle vao pelo
// nome (XF86Back, Up, ...) e o C traduz para SDL. O player esta em Video.cs.
//
// Por que GLWindow e nao GLView: GLView so existe a partir da API11 e o alvo
// comeca na API8 (Tizen 6.0). O spike mediu GLWindow + .so a ~45 fps no Tizen 6.
//
// A janela padrao do NUI (Window.Instance) fica transparente por baixo do
// GLWindow: e nela que o player prende o video.
//
// TIZEN 9 (#170, #137): aberto pelo menu da TV, o app sobe, desenha o PRIMEIRO
// quadro e para (log "anterior" da UN75CU7700GXZD no D1: "[t] ... primeiro
// quadro na tela" e nada mais ate DALI_FRAMEWORK_DESTROY); aberto pelo
// Apps2Samsung, roda. Suspeita (nao provada): o lancador do Tizen 9 sobe a
// janela principal (opaca para o gerenciador de janelas) por cima do GLWindow
// quando o arranque termina; o GLWindow coberto e iconificado e o fio de
// desenho dele para. Por isso: o vigia sobe o GLWindow de volta se os quadros
// pararem com a janela principal visivel e o app em primeiro plano, e, se nem
// assim voltar, poe o motivo na tela para foto. O rastro de etapas diz onde o
// arranque anterior parou.
#pragma warning disable CS0618
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Reflection;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using Tizen.Multimedia;
using Tizen.NUI;
using IOPath = System.IO.Path;
using NuiWindow = Tizen.NUI.Window;
using NuiRect = Tizen.NUI.Rectangle;
using NuiColor = Tizen.NUI.Color;
using NuiTimer = Tizen.NUI.Timer;
using NuiLabel = Tizen.NUI.BaseComponents.TextLabel;
#if NV_API11
using GLView = Tizen.NUI.BaseComponents.GLView;
#endif

namespace NuvioTpk
{
    class Program : NUIApplication
    {
        [DllImport("libnuvio.so")] static extern int nv_tpk_iniciar(string arte, string dados, int w, int h);
        [DllImport("libnuvio.so")] static extern int nv_tpk_quadro();
        [DllImport("libnuvio.so")] static extern void nv_tpk_config(int esperaMs, int swapZero);
        [DllImport("libnuvio.so")] static extern void nv_tpk_tecla(string nome, int apertou);
        [DllImport("libnuvio.so")] static extern void nv_tpk_log(string linha);
        [DllImport("libnuvio.so")] static extern void nv_tpk_ciclo(int memoria);
        [DllImport("libdl.so.2")] static extern IntPtr dlopen(string path, int flags);
        [DllImport("libdl.so.2")] static extern IntPtr dlerror();
        [DllImport("libcapi-appfw-application.so.0")] static extern IntPtr app_get_data_path();
        [DllImport("libc.so.6")] static extern void free(IntPtr pointer);
        [DllImport("libc.so.6")] static extern int setenv(string name, string value, int overwrite);
        // Reserva de teclas de midia (#196), ver ReservaTeclasMidia. Assinaturas
        // do Ecore_Wl2.h/Ecore_Wayland2.h do rootstrap tizen-9.0-device.core.
        [DllImport("libecore_wl2.so.1")] static extern IntPtr ecore_wl2_window_find(uint id);
        [DllImport("libecore_wl2.so.1")] static extern byte ecore_wl2_window_keygrab_set(IntPtr win, string key, int mod, int notMod, int priority, int modo);

        const int W = 1920, H = 1080;
#if NV_API8
        const string Host = "api8";
#elif NV_API9
        const string Host = "api9";
#else
        const string Host = "api11";
#endif

        // CANARIO (#137): toca res/silencio.mp4 pelo player do filme logo que a
        // janela sobe, para a TV tirar o audio do Samsung TV Plus que segue por
        // baixo (ver Video.PrimeAudio). false = desliga, o host volta a ser o de
        // antes. Espera PRIME_ESPERA_MS depois do gl.Show() para nao disputar
        // com a abertura.
        // CANARIO 7 (#195): desligado no 6.0/6.5. Suspeita (nao provada): o
        // segundo Player com Display aos 1,5 s coincide com a pausa de ~2 s de
        // todo arranque no 6.5.
#if NV_API11
        const bool PRIME_AUDIO = true;
#else
        const bool PRIME_AUDIO = false;
#endif
        const int PRIME_ESPERA_MS = 1500;

        // ================= CANARIO DE JANELA (#137, #170) =================
        // Relatos (rawldon, Tizen 6.0): (1) a tela anterior do YouTube aparece
        // atras do trailer; (2) fechar o Nuvio deixa a TV preta e muda; (4)
        // Tizen 9 pelo menu: primeiro quadro e o GLWindow para. Hipotese NAO
        // provada: empilhamento/opacidade das janelas. Cada chave abaixo e
        // independente, e cada uma escreve "[janela] ..." no nuvio.log e uma
        // nota no rastro de etapas, para um relato apontar qual importou.
        //
        // Base (TizenFX + dali-adaptor, lidos na fonte):
        //  - NUIApplication() = NUICoreBackend("", WindowMode.Opaque): a janela
        //    principal nasce OPACA; o DALi passa isso a
        //    ecore_wl2_window_alpha_set (window-base-ecore-wl2.cpp).
        //  - Window/GLWindow.SetOpaqueState(true) -> tizen_policy_set_opaque_state:
        //    o gerenciador de janelas trata a janela translucida como opaca ao
        //    calcular a visibilidade (doc do TizenFX; sem efeito numa janela ja
        //    opaca). Janela coberta por uma opaca fica "fully obscured".
        //  - GLWindow iconificado para o fio de desenho (gl-window-impl.cpp,
        //    OnIconifyChanged -> mGlWindowRenderThread->Pause).
        //
        // JANELA_PRINCIPAL_TRANSPARENTE: a principal nasce em
        // WindowMode.Transparent. Alvo: (4). Se o lancador do Tizen 9 sobe a
        // principal por cima do GLWindow, uma principal OPACA cobre o GLWindow,
        // que e iconificado e para de desenhar; transparente, nao cobre. Liga
        // na API11 (o host do Tizen 7+/9, onde o (4) acontece) e na API9
        // (Tizen 6.5, #612); 6.0 continua opaco.
        // #612: on QE65QN91BATXXH (6.5), adWroc reported native rendering
        // continuing behind a black screen. Transparent + no opaque state
        // worked with unchanged native libraries: the main window may cover
        // the separate GLWindow. An extra diagnostic window was also removed,
        // so the cause is not isolated. Opaque state guarded against TV Plus
        // showing underneath; the reporter saw no such issue on 6.5.
#if NV_API11 || NV_API9
        const bool JANELA_PRINCIPAL_TRANSPARENTE = true;
#else
        const bool JANELA_PRINCIPAL_TRANSPARENTE = false;
#endif
        // JANELA_PRINCIPAL_OPACA: SetOpaqueState(true) na principal (a de
        // BAIXO do Nuvio, dona do video). Alvo: (1) e o TV Plus por baixo. Diz
        // ao gerenciador que o Nuvio cobre o app anterior sem mexer na ordem
        // entre as nossas duas janelas: o GLWindow (em cima, translucido) nao
        // e coberto, e o video, na principal, tambem nao. Desligada na API11,
        // onde a principal e transparente de proposito (subindo por cima do
        // GLWindow, opaca, voltaria a cobri-lo: o (4) de novo).
        //
        // CANARIO (#188, #195, #185): com API11_GLVIEW nao ha GLWindow para
        // cobrir, e o motivo acima deixa de valer. Sem o estado opaco, a
        // principal transparente nao cobre a janela de baixo, e no Tizen 9 a
        // tela inicial da Samsung aparece em todo pixel de alfa 0 sem video
        // (a linha de 1 px da #185 na S90D; o destaque inteiro na #195). Com
        // ele, o gerenciador trata a janela de baixo como coberta. NAO PROVADO
        // em TV: sai como canario. So muda o calculo de visibilidade, nao o
        // alfa da janela (o furo continua chegando ao video).
#if NV_API11
        const bool JANELA_PRINCIPAL_OPACA = API11_GLVIEW;
#elif NV_API9
        const bool JANELA_PRINCIPAL_OPACA = false; // #612: keep the main window transparent.
#else
        const bool JANELA_PRINCIPAL_OPACA = true;
#endif
        // JANELA_GL_OPACA: SetOpaqueState(true) no GLWindow. O outro jeito de
        // dizer "o Nuvio cobre a tela". DESLIGADA: o GLWindow esta POR CIMA da
        // janela do video, e opaco para o gerenciador cobriria a propria janela
        // do video (que pode ser iconificada e levar o video junto). So para
        // um teste dirigido, se a principal opaca nao bastar.
        const bool JANELA_GL_OPACA = false;
        // JANELA_SOBE_GL_APPCONTROL: a cada AppControl (e como o menu da TV
        // abre/reabre o app), sobe o GLWindow 300 ms depois. Alvo: (4). Na
        // API8 so registra (6.0 funciona).
        // CANARIO 8 (#195): ligado tambem na API8. Com o canario 7 (video no GL,
        // GL opaco) a QN85Q70A (6.0) saiu pela Home e, reaberta pelo menu, recebeu
        // 30 AppControl sem OnResume e sem janela visivel (D1 15660/15664): o app
        // so voltava reiniciando a TV. Na API8 so sobe se alguma janela esta
        // escondida, para nao mexer na primeira abertura.
        const bool JANELA_SOBE_GL_APPCONTROL = true;
        // JANELA_SAIDA_LIMPA: ao sair, solta o player (Stop/Unprepare/Display
        // nenhum/Dispose), esconde as janelas e so entao Exit(); se o processo
        // ainda estiver vivo JANELA_SAIDA_PRAZO_MS depois, _exit(0). Alvo: (2):
        // se a TV fica preta porque o processo nao morre (ou morre segurando o
        // plano de video), o rastro do proximo arranque diz qual das duas.
        const bool JANELA_SAIDA_LIMPA = true;
        const int JANELA_SAIDA_PRAZO_MS = 4000;

        // ================= CANARIO GLVIEW (API11, #137, #170) =================
        // Evidencia (D1 9050/9051/9054, UN75CU7700, Tizen 9, aberto pelo menu):
        // gl-window visivel, 1o quadro, gl-window com foco, 2,96 s PAUSE do app,
        // ~10 s a TV fecha. Pelo sdb (Apps2Samsung) o mesmo build roda a 55-60
        // fps. Hipotese NAO provada: no Tizen 9 o ciclo de vida do app segue a
        // janela PRINCIPAL, e um GLWindow de tela cheia por cima a faz parecer
        // coberta. API11_GLVIEW = true: NADA de segunda janela; o desenho vai
        // num GLView (widget GL da arvore NUI, TizenFX API10+) de tela cheia
        // dentro da janela principal, o modelo do spike de setembro que passou
        // nas tres TVs Tizen 9 (tizen-tpk-spike/dotnet/NvSpikeNui/Program.cs,
        // AdicionaGLView). false = volta ao GLWindow. So existe na API11; 6.0 e
        // 6.5 seguem com GLWindow, sem mudanca.
#if NV_API11
        const bool API11_GLVIEW = true;
#else
        const bool API11_GLVIEW = false;
#endif

        // O modo da principal so pode ser escolhido no construtor. ("", Opaque)
        // e exatamente o NUIApplication() de antes (NUICoreBackend: stylesheet
        // "" e Opaque por padrao, conferido na API8 e na API11 do TizenFX).
        Program() : base("", JANELA_PRINCIPAL_TRANSPARENTE ? WindowMode.Transparent : WindowMode.Opaque) { }

        GLWindow gl;
#if NV_API11
        GLView glView;
#endif
        volatile bool fim;
        NuiTimer vigia, prime;
        Video video;
#if NV_TEXTO
        Texto texto;
#else
        // Guardado aqui de proposito: o C chama os delegates do Voz; sem esta
        // referencia o GC os recolheria.
        Voz voz;
#endif
        static bool soCarregada;

        protected override void OnCreate()
        {
            Etapa("begin nui-oncreate");
            base.OnCreate();
            Etapa("ok nui-oncreate");
            string dados = DirectoryInfo.Data;
            if (string.IsNullOrEmpty(etapasArq)) etapaAnterior = RodarEtapas(dados);
            Etapa("note launch host=" + Host + " " + RuntimeInformation.FrameworkDescription + " tv=" + Tv());
            try { Arranca(dados); }
            catch (Exception e)
            {
                Etapa("note launch threw " + e.GetType().Name + ": " + e.Message);
                Erro("Nuvio could not start.", e.GetType().Name + ": " + e.Message);
            }
        }

        void Arranca(string dados)
        {
            var principal = SynchronizationContext.Current;
            var janela = NuiWindow.Instance;
            janela.BackgroundColor = NuiColor.Transparent;
            Janela("principal modo=" + (JANELA_PRINCIPAL_TRANSPARENTE ? "Transparent" : "Opaque (padrao)") + " host=" + Host);
            if (JANELA_PRINCIPAL_OPACA)
            {
                try { janela.SetOpaqueState(true); Janela("principal SetOpaqueState(true) -> IsOpaqueState=" + janela.IsOpaqueState()); }
                catch (Exception e) { Janela("principal SetOpaqueState falhou " + e.GetType().Name + ": " + e.Message); }
            }
            else Janela("principal SetOpaqueState: desligado");
            // Rastro nunca derruba o arranque: sem esses eventos, o app segue.
            try
            {
                janela.VisibilityChanged += (s, e) => { if (!e.Visibility) { try { nv_tpk_ciclo(0); } catch (EntryPointNotFoundException) { } } principalVisivel = e.Visibility; Etapa("note main-window visible=" + e.Visibility + Contagem()); Janela("principal visivel=" + e.Visibility, false); };
                janela.FocusChanged += (s, e) => { Etapa("note main-window focus=" + e.FocusGained + Contagem()); if (e.FocusGained) SobeGlSePreciso("main-window focus"); };
            }
            catch (Exception e) { Etapa("note main-window events unavailable " + e.GetType().Name + ": " + e.Message); }

            string arte = IOPath.Combine(DirectoryInfo.Resource, "art");

            // A .so aberta por caminho absoluto ANTES do primeiro DllImport, como
            // no pacote do Tizen 4/5: se o launcher desta TV nao procurar no lib/
            // do pacote (4/5 nao procurava; no 6.5 ninguem mediu, #170), o
            // DllImport("libnuvio.so") casa pelo soname com a ja carregada.
            string so = IOPath.Combine(IOPath.GetFullPath(IOPath.Combine(DirectoryInfo.Resource, "..")), "lib", "libnuvio.so");

            // AUTO-ATUALIZACAO (opt-in por staging verificado): SO quando ha uma
            // libnuvio.so encenada e VERIFICADA mais nova que a empacotada, ela e
            // memfd-carregada aqui e os DllImport deste assembly sao ROTEADOS para
            // ela (NvCarga.RotearDllImport). So o RTLD_GLOBAL nao bastava (#184):
            // o runtime achava lib/libnuvio.so por caminho e o app seguia na
            // empacotada. SEM staging, este 6+ nao usa memfd nenhum: cai no
            // dlopen simples de sempre, byte a byte igual ao anterior. Qualquer
            // falha no staging apaga o staging e volta para a empacotada — a
            // tentativa de atualizar nunca impede o app de abrir.
            Etapa("note native-library path=" + so + " exists=" + File.Exists(so));
            Etapa("begin dlopen-so");
            bool carregou = false;
            try
            {
                string staged = NvCarga.DecidirStaged(dados, NvCarga.VersaoEmpacotada(DirectoryInfo.Resource), out string _);
                if (staged != null && !NvCarga.PodeRotear())
                    Etapa("note staged lib skipped: runtime cannot route DllImport");
                else if (staged != null)
                {
                    IntPtr h = NvCarga.MemfdDlopen(File.ReadAllBytes(staged), out string _);
                    if (h == IntPtr.Zero) { Etapa("note staged lib failed, using the bundled one"); NvCarga.ApagarStaged(dados); }
                    else if (NvCarga.RotearDllImport(typeof(Program).Assembly, h, out string falhaRota))
                    { carregou = true; Etapa("note loaded staged lib by memfd, DllImport routed to it"); }
                    else { Etapa("note staged lib loaded but DllImport routing failed (" + falhaRota + "), using the bundled one"); NvCarga.ApagarStaged(dados); }
                }
            }
            catch { try { NvCarga.ApagarStaged(dados); } catch { } carregou = false; }

            if (!carregou)
            {
                dlerror();
                if (dlopen(so, 2 | 0x100) == IntPtr.Zero)
                {
                    string e = Marshal.PtrToStringAnsi(dlerror());
                    Etapa("fail dlopen-so " + e);
                    Erro("The TV did not let Nuvio load its native library.", so + ": " + (string.IsNullOrEmpty(e) ? "refused without a message" : e));
                    return;
                }
            }
            soCarregada = true;
            Etapa("ok dlopen-so");

            Etapa("begin video-init");
            try
            {
                video = new Video(DisplayDoVideo,
                                  a => { if (principal != null) principal.Post(_ => a(), null); else a(); },
                                  dados, W, H);
            }
            catch (Exception e)
            {
                Etapa("fail video-init " + e.GetType().Name + ": " + e.Message);
                Erro("Nuvio could not start the player.", e.GetType().Name + ": " + e.Message);
                return;
            }
            Etapa("ok video-init");
#if NV_TEXTO
            // CANARIO (Texto.cs): teclado/ditado do sistema. So com -p:NvTextoCanario=1.
            Etapa("begin texto-init");
            try { texto = new Texto(a => { if (principal != null) principal.Post(_ => a(), null); else a(); }, s => video.Log(s)); }
            catch (Exception e) { Etapa("note texto-init " + e.GetType().Name + ": " + e.Message); }
            Etapa("ok texto-init");
#else
            // Botao de microfone das buscas (Voz.cs): abre o teclado da Samsung
            // so para o microfone dele. Se aparece, decide o ajuste "Botao de
            // microfone na busca" com o que a TV diz sobre microfone.
            Etapa("begin voz-init");
            try { voz = new Voz(a => { if (principal != null) principal.Post(_ => a(), null); else a(); }, s => video.Log(s)); }
            catch (Exception e) { Etapa("note voz-init " + e.GetType().Name + ": " + e.Message); }
            Etapa("ok voz-init");
#endif

            // API8: o callback roda no fio principal e o DALi troca os buffers
            // mesmo em quadro pulado, entao espera o app terminar o quadro.
            // API9+: fio de render proprio, respeita o 0 de "pular", e o swap
            // com vsync e que derrubava o Tizen 9 a 3 fps no spike.
            Etapa("begin nv_tpk_iniciar");
            try
            {
                // Optional panel heuristic must not gate native startup.
                try
                {
                    if (setenv("NUVIO_TV_MODEL", Tv().Split(' ')[0], 1) != 0)
                        Etapa("note panel model unavailable: setenv failed");
                }
                catch (Exception e) { Etapa("note panel model unavailable: " + e.Message); }
#if NV_API8
                nv_tpk_config(-1, 0);
#else
                nv_tpk_config(50, 1);
#endif
                if (nv_tpk_iniciar(arte, dados, W, H) != 0) throw new Exception("nv_tpk_iniciar failed");
            }
            catch (Exception e)
            {
                try { File.WriteAllText(IOPath.Combine(dados, "tpk-erro.txt"), e.ToString()); } catch { }
                Etapa("fail nv_tpk_iniciar " + e.GetType().Name + ": " + e.Message);
                Erro("Nuvio could not start.", e.GetType().Name + ": " + e.Message);
                return;
            }
            Etapa("ok nv_tpk_iniciar");
            Etapa("begin apps-init");
            try { Apps.Registrar(a => { if (principal != null) principal.Post(_ => a(), null); else a(); }); } catch (Exception e) { Etapa("note apps-init " + e.GetType().Name); }
            Etapa("ok apps-init");
            NvCarga.LimparAntigos(dados);
            // Prova no log (#184): >0 = os DllImport passaram pelo resolvedor e
            // cairam na encenada; o "[atualizacao] instalada X" do C deve dizer
            // a versao dela.
            if (carregou) Etapa("note staged lib routes=" + NvCarga.RotaUsos);

            inicioDesenhoMs = relogio.ElapsedMilliseconds;
            Etapa("begin gl-window");
            try
            {
                CriaJanelaGL();
            }
            catch (Exception e)
            {
                Etapa("fail gl-window " + e.GetType().Name + ": " + e.Message);
                Erro("Nuvio could not open its window.", e.GetType().Name + ": " + e.Message);
                return;
            }
            Etapa("ok gl-window");
            ReservaTeclasMidia();
            Etapa("begin first-frame");
            if (etapaAnterior != null) Aviso("Previous launch stopped at: " + etapaAnterior);
        }

        bool tvLogada;
        int tiques;

        // Janela de desenho no ar (GLWindow ou GLView), para o vigia e a saida.
        bool DesenhoPronto()
        {
#if NV_API11
            if (glView != null) return true;
#endif
            return gl != null;
        }

        // true = so a janela principal (GLView); nunca true fora da API11.
        static bool JanelaUnica { get { return API11_GLVIEW; } }

        static string Tv()
        {
            string versao = null, modelo = null;
            try { Tizen.System.Information.TryGetValue<string>("http://tizen.org/feature/platform.version", out versao); } catch { }
            try { Tizen.System.Information.TryGetValue<string>("http://tizen.org/system/model_name", out modelo); } catch { }
            return (modelo ?? "?") + " tizen=" + (versao ?? "?");
        }

        void LogaTv()
        {
            video.Log("[tv] modelo=" + Tv() + " host=" + Host +
                      " dotnet=" + RuntimeInformation.FrameworkDescription + " tela=" + W + "x" + H);
            // O rastro do arranque anterior entra no nuvio.log deste, que o envio
            // automatico sobe: e assim que um arranque que morreu chega ao D1.
            try
            {
                string ant = IOPath.Combine(DirectoryInfo.Data, "tpk-etapas-anterior.txt");
                if (!File.Exists(ant)) return;
                var linhas = File.ReadAllLines(ant);
                int de = Math.Max(0, linhas.Length - 80);
                for (int i = de; i < linhas.Length; i++) video.Log("[etapa-anterior] " + linhas[i]);
            }
            catch { }
        }

        void CriaJanelaGL()
        {
#if NV_API11
            if (API11_GLVIEW) CriaGlView(); else
#endif
            CriaGlWindow();
            IniciaVigia();
        }

#if NV_API11
        // GLView de tela cheia na janela principal (a do video). Modelo do
        // spike (AdicionaGLView): new GLView(RGBA8888), RegisterGLCallbacks
        // (init, quadro -> int, terminate), RenderingMode Continuous, Add na
        // janela. Alem do spike: SetGraphicsConfig (sem depth/stencil/msaa, GLES
        // 2.0) e a superficie RGBA8888 -> alfa 8 bits, o que gfx_furo (alfa 0)
        // precisa para o video aparecer por baixo (ver o cabecalho do nv_tpk_quadro).
        void CriaGlView()
        {
            Janela("glview: janela unica (sem GLWindow); nao sobe gl no appcontrol, gl opaco n/a");
            glView = new GLView(GLView.ColorFormat.RGBA8888)
            {
                Name = "nuvio-glview",
                Position2D = new Position2D(0, 0),
                Size2D = new Size2D(W, H),
                BackgroundColor = NuiColor.Transparent,
            };
            // Interlocked: o vigia conta "parado" a partir daqui.
            Interlocked.Exchange(ref ultimaChamadaMs, relogio.ElapsedMilliseconds);
            try { glView.SetGraphicsConfig(false, false, 0, GLESVersion.Version20); Janela("glview SetGraphicsConfig(depth=0 stencil=0 msaa=0 GLES2) ok"); }
            catch (Exception e) { Janela("glview SetGraphicsConfig falhou " + e.GetType().Name + ": " + e.Message + " (segue com o padrao do GLView)"); }
            // init/terminate rodam no fio de desenho do GLView, nao no principal.
            glView.RegisterGLCallbacks(
                () => Etapa("note glview init callback" + Contagem()),
                () => Quadro(),
                () => Etapa("note glview terminate callback" + Contagem()));
            glView.RenderingMode = GLRenderingMode.Continuous;
            // GLView's mode does not keep its owning DALi window updating.
            // A static video hole must still get frames between watchdog ticks.
            NuiWindow.Instance.RenderingBehavior = RenderingBehaviorType.Continuously;
            NuiWindow.Instance.KeyEvent += (s, e) => Tecla(e.Key, "main");
            NuiWindow.Instance.Add(glView);
            Etapa("note glview created size=" + W + "x" + H + " format=RGBA8888 mode=Continuous" + Contagem());
            Janela("glview criado " + W + "x" + H + " RGBA8888 Continuous, na janela principal");
        }
#endif

        // ================= CANARIO 7 (#195, #203): VIDEO NO PROPRIO GLWindow =================
        // Toda implementacao de referencia (DALi VideoView underlay,
        // dali-extension tizen-video-player-ecore-wl2.cpp; flutter-tizen
        // video_player_videohole; JuvoPlayer; TizenFX Display.cs) usa UMA
        // janela: a que desenha a UI com o furo e dona do player (display
        // OVERLAY na Ecore_Wl2_Window dela) e esta OPACA para o compositor
        // (ecore_wl2_window_alpha_set(win, false)). Aqui o video era da
        // principal e o furo num GLWindow translucido por cima; medido na
        // QA55LS03B (6.5): com a janela do video visivel (canario 5, D1 14876+)
        // o furo ainda mostra a tela da TV, ou seja, nada opaco nosso cobre o
        // lancador por baixo do GL. Aqui: (1) o Player liga no Ecore_Wl2_Window
        // do GLWindow (EcoreDisplaySetter interno do TizenFX por reflexao, num
        // Display criado normal); (2) o GLWindow continua ARGB no EGL (o furo
        // precisa do alfa) mas fica opaco para o compositor. NAO PROVADO em TV;
        // cada passo escreve "[janela] video7: ...". So api8 e api9.
#if NV_API8 || NV_API9
        const bool VIDEO_NO_GL = true;
#else
        const bool VIDEO_NO_GL = false;
#endif
        [DllImport("libecore_wl2.so.1")] static extern void ecore_wl2_window_alpha_set(IntPtr win, byte alpha);
        [DllImport("libecore_wl2.so.1")] static extern byte ecore_wl2_window_alpha_get(IntPtr win);
        IntPtr glEcore = IntPtr.Zero;
        bool jaPausou;
        int displaysGl;

        void PrendeVideoNoGl()
        {
            if (!VIDEO_NO_GL) return;
            try
            {
                int idPrincipal = -1;
                try { idPrincipal = NuiWindow.Instance.GetNativeId(); } catch (Exception e) { Janela("video7: GetNativeId da principal falhou " + e.GetType().Name); }
                IntPtr pPrincipal = idPrincipal >= 0 ? ecore_wl2_window_find((uint)idPrincipal) : IntPtr.Zero;
                var outras = new List<string>();
                IntPtr achada = IntPtr.Zero; int idAchada = -1, n = 0;
                for (uint id = 0; id < 256; id++)
                {
                    IntPtr w = ecore_wl2_window_find(id);
                    if (w == IntPtr.Zero || w == pPrincipal) continue;
                    n++; outras.Add(id.ToString());
                    achada = w; idAchada = (int)id;   // a de id maior: o GL nasce depois da principal
                }
                Janela("video7: principal id=" + idPrincipal + " achada=" + (pPrincipal != IntPtr.Zero) + "; outras janelas wl2: " + n + " (ids " + string.Join(",", outras) + ")");
                if (achada == IntPtr.Zero) { Janela("video7: janela do gl NAO achada, video fica na principal"); return; }
                glEcore = achada;
                Janela("video7: janela do gl = wl2 id " + idAchada);
                try
                {
                    byte antes = ecore_wl2_window_alpha_get(glEcore);
                    ecore_wl2_window_alpha_set(glEcore, 0);
                    byte depois = ecore_wl2_window_alpha_get(glEcore);
                    Janela("video7: gl alpha_set(false): alpha " + antes + " -> " + depois);
                    if (depois != 0) PlanoBOpaco("alpha_get ainda 1");
                }
                catch (Exception e) { Janela("video7: alpha_set falhou " + e.GetType().Name + ": " + e.Message); PlanoBOpaco("alpha_set falhou"); }
                // Confere ja: um Display de teste com o setter trocado.
                var d = DisplayDoVideo();
                Janela("video7: display de teste " + (d != null ? "ok" : "nulo"));
            }
            catch (Exception e) { Janela("video7: falhou " + e.GetType().Name + ": " + e.Message + " (video fica na principal)"); glEcore = IntPtr.Zero; }
        }

        void PlanoBOpaco(string porque)
        {
            try { gl.SetOpaqueState(true); Janela("video7: plano B gl SetOpaqueState(true) (" + porque + ") -> " + gl.IsOpaqueState()); }
            catch (Exception e) { Janela("video7: plano B falhou " + e.GetType().Name + ": " + e.Message); }
        }

        // Display para cada Player (um Display so serve a um dono). Com a janela
        // do gl achada: Display(principal) com o setter interno trocado por um
        // EcoreDisplaySetter(janela do gl). Qualquer falha: o de sempre.
        Display DisplayDoVideo()
        {
            var d = new Display(NuiWindow.Instance);
            if (glEcore == IntPtr.Zero) return d;
            try
            {
                var asm = typeof(Display).Assembly;
                var tSetter = asm.GetType("Tizen.Multimedia.EcoreDisplaySetter", true);
                var setter = Activator.CreateInstance(tSetter, BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public, null, new object[] { glEcore }, null);
                FieldInfo campo = null;
                foreach (var f in typeof(Display).GetFields(BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public))
                    if (f.FieldType.Name == "IDisplaySetter" || f.FieldType.IsAssignableFrom(tSetter) && f.FieldType != typeof(object)) { campo = f; break; }
                if (campo == null) throw new Exception("campo IDisplaySetter nao achado no Display");
                campo.SetValue(d, setter);
                if (displaysGl++ < 3) Janela("video7: display preso na janela do gl (campo " + campo.Name + ")");
            }
            catch (Exception e)
            {
                if (displaysGl++ < 3) Janela("video7: setter falhou " + e.GetType().Name + ": " + e.Message + " (este player fica na principal)");
                return new Display(NuiWindow.Instance);
            }
            return d;
        }

        void CriaGlWindow()
        {
            Etapa("begin gl-create");
            gl = new GLWindow("nuvio", new NuiRect(0, 0, W, H), true);
            Etapa("ok gl-create");
            Etapa("begin gl-config");
            // O vigia conta "parado" a partir daqui ate a primeira chamada.
            Interlocked.Exchange(ref ultimaChamadaMs, relogio.ElapsedMilliseconds);
#if NV_API8
            gl.SetEglConfig(false, false, 0, GLWindow.GLESVersion.Version_2_0);
            gl.RegisterGlCallback(() => { Etapa("ok gl-context-callback"); }, () => { Quadro(); }, () => { });
#elif NV_API9
            gl.SetEglConfig(false, false, 0, GLESVersion.Version20);
            gl.RegisterGlCallback(() => { Etapa("ok gl-context-callback"); }, () => Quadro(), () => { });
            gl.RenderingMode = GLRenderingMode.Continuous;
#else
            gl.SetGraphicsConfig(false, false, 0, GLESVersion.Version20);
            gl.RegisterGLCallbacks(() => { Etapa("ok gl-context-callback"); }, () => Quadro(), () => { });
            gl.RenderingMode = GLRenderingMode.Continuous;
#endif
            Etapa("ok gl-config");
            // As duas janelas repassam tecla: qual delas fica com o foco depende
            // do firmware, e so uma recebe de cada vez.
            gl.KeyEvent += (s, e) => Tecla(e.Key, "gl");
            NuiWindow.Instance.KeyEvent += (s, e) => Tecla(e.Key, "main");
            try
            {
                gl.VisibilityChanged += (s, e) => { if (!e.Visibility) { try { nv_tpk_ciclo(0); } catch (EntryPointNotFoundException) { } } glVisivel = e.Visibility; Etapa("note gl-window visible=" + e.Visibility + Contagem()); Janela("gl visivel=" + e.Visibility, false); };
                gl.FocusChanged += (s, e) => Etapa("note gl-window focus=" + e.FocusGained + Contagem());
            }
            catch (Exception e) { Etapa("note gl-window events unavailable " + e.GetType().Name + ": " + e.Message); }
            Etapa("begin gl-show");
            gl.Show();
            Etapa("ok gl-show");
            if (JANELA_GL_OPACA)
            {
                try { gl.SetOpaqueState(true); Janela("gl SetOpaqueState(true) -> IsOpaqueState=" + gl.IsOpaqueState()); }
                catch (Exception e) { Janela("gl SetOpaqueState falhou " + e.GetType().Name + ": " + e.Message); }
            }
            else Janela("gl SetOpaqueState: desligado (translucido, como antes)");
            PrendeVideoNoGl();
        }

        void IniciaVigia()
        {
            // O canario de audio (PRIME_AUDIO) nao depende de nenhuma chave de
            // janela; se a principal opaca ja pausar o TV Plus sozinha, o clipe
            // so fica redundante. A linha diz o que estava ligado junto.
            Janela("prime de audio " + (PRIME_AUDIO ? "ligado" : "desligado") + " (independe destas chaves)");

            // Exit() tem de sair do fio principal, e Quadro() roda no de desenho.
            vigia = new NuiTimer(250);
            vigia.Tick += (s, e) =>
            {
                if (fim) { Etapa("note app ended (main returned)"); Sair("app ended"); return false; }
                video.Tique();
                // ~3 s depois de abrir, quando o main() do app ja redirecionou
                // o stdout para o nuvio.log: uma linha dizendo que TV e esta,
                // para o D1 separar os relatos por versao da Tizen.
                if (!tvLogada && ++tiques >= 12) { LogaTv(); tvLogada = true; DespejaJanela(); }
                Vigia();
                return true;
            };
            vigia.Start();

            AgendaPrime();
        }

        // Nunca derruba nada: qualquer falha vira linha no log e o app segue.
        void AgendaPrime()
        {
            if (!PRIME_AUDIO) return;
            try
            {
                string arq = IOPath.Combine(DirectoryInfo.Resource, "silencio.mp4");
                prime = new NuiTimer(PRIME_ESPERA_MS);
                prime.Tick += (s, e) =>
                {
                    try { if (!fim && video != null) video.PrimeAudio(arq); }
                    catch (Exception ex) { video?.Log("[audio] prime fail " + ex.GetType().Name + ": " + ex.Message); }
                    return false;
                };
                prime.Start();
            }
            catch (Exception e) { video?.Log("[audio] prime fail agendar " + e.GetType().Name + ": " + e.Message); }
        }

        // ================= VIGIA DO DESENHO (Tizen 9) =================

        // Contadores do fio de desenho: chamadas do framework e quadros trocados.
        int chamadas, trocados;
        static readonly Stopwatch relogio = new Stopwatch();
        long ultimaChamadaMs = -1, inicioDesenhoMs;
        volatile bool pausado;
        bool principalVisivel = true, glVisivel = true;
        int subidas, subidasFoco;
        long paradoDesdeMs = -1;
        NuiLabel telaParado;

        string Contagem()
        {
            return " t=" + (relogio.ElapsedMilliseconds / 1000.0).ToString("0.0") + "s calls=" + chamadas + " frames=" + trocados +
                   " paused=" + pausado;
        }

        // Sobe o GLWindow por cima da janela principal. So quando o app esta em
        // primeiro plano e a principal visivel: com a tela inicial da TV por
        // cima (tecla Home), as duas ficam invisiveis e o app nao se intromete.
        void SobeGl(string porque)
        {
            if (gl == null || erroNaTela || saindo) return;   // GLView: gl == null, nada a subir
            subidas++;
            Etapa("note raise gl-window #" + subidas + " (" + porque + ")" + Contagem() + " mainVisible=" + principalVisivel + " glVisible=" + glVisivel);
            // A JANELA PRINCIPAL PRIMEIRO (#188, #195). O video e desenhado nela
            // (Video.cs: new Display(NuiWindow.Instance)). Ao sair pela tecla
            // Home ela fica invisivel, e reabrir pelo menu da TV (AppControl)
            // subia SO o GLWindow: a principal nunca voltava, o plano de video
            // ficava escondido e o furo do GL mostrava a tela inicial da
            // Samsung. Medido na QE55QN95B (Tizen 6.5, api9), 1.6.1: todas as
            // sessoes terminam com "principal visivel=False" depois de
            // "appcontrol: sobe o gl". Sobe a principal e so entao o GL por cima.
            if (!principalVisivel)
            {
                try
                {
                    var w = NuiWindow.Instance;
                    w.Show();
                    w.Raise();
                    Janela("principal reaberta (" + porque + ")");
                }
                catch (Exception e) { Janela("principal nao reabriu " + e.GetType().Name + ": " + e.Message); }
            }
            try { gl.Show(); gl.Raise(); } catch (Exception e) { Etapa("note raise failed " + e.GetType().Name + ": " + e.Message); }
        }

        void SobeGlSePreciso(string porque)
        {
#if NV_API8
            // Tizen 6.0 funciona hoje: so registra.
#else
            if (gl == null || pausado || subidasFoco >= 5) return;   // GLView: gl == null
            if (!glVisivel || (trocados > 0 && ParadoMs() > 1000)) { subidasFoco++; SobeGl(porque); }
#endif
        }

        long ParadoMs()
        {
            long u = Interlocked.Read(ref ultimaChamadaMs);
            return u < 0 ? relogio.ElapsedMilliseconds : relogio.ElapsedMilliseconds - u;
        }

        // A cada 250 ms, no fio principal. Quadros parados ha mais de 1,5 s com o
        // app em primeiro plano e a janela principal visivel = o GLWindow foi
        // coberto. Sobe ele ate 3 vezes; se nem assim voltar, a tela diz o que
        // houve (com os numeros) para uma foto.
        void Vigia()
        {
            if (!DesenhoPronto() || erroNaTela || saindo) return;
            // Draw callbacks may run forever without producing an app frame.
            // Keep the existing NUI diagnostics usable before native UI exists.
            if (trocados == 0 && !pausado && principalVisivel &&
                relogio.ElapsedMilliseconds - inicioDesenhoMs >= 15000)
            {
                Etapa("fail first-frame" + Contagem());
                Erro("Nuvio has not drawn its first frame.",
                     "Startup timed out. Please photograph this screen. Diagnostics: data/tpk-etapas.txt.");
                return;
            }
            long parado = ParadoMs();
            if (parado < 1500 || pausado || !principalVisivel)
            {
                if (paradoDesdeMs >= 0 && parado < 1500)
                {
                    Etapa("note frames resumed after " + (relogio.ElapsedMilliseconds - paradoDesdeMs) + " ms" + Contagem());
                    paradoDesdeMs = -1;
                    subidas = 0;
                    TiraTelaParado();
                }
                return;
            }
            if (paradoDesdeMs < 0)
            {
                paradoDesdeMs = relogio.ElapsedMilliseconds;
                Etapa("note frames stalled" + Contagem() + " mainVisible=" + principalVisivel + " glVisible=" + glVisivel);
            }
            long ha = relogio.ElapsedMilliseconds - paradoDesdeMs;
            if (!JanelaUnica && subidas < 3 && ha >= subidas * 1000L) { SobeGl("frames stalled " + parado + " ms"); return; }
            if (telaParado == null && ha >= 4000 && trocados < 30) MostraTelaParado(parado);
        }

        // Nao fatal: se os quadros voltarem, some sozinha.
        void MostraTelaParado(long parado)
        {
            try
            {
                Etapa("note showing stalled screen" + Contagem());
                var w = NuiWindow.Instance;
                w.BackgroundColor = NuiColor.Black;
                telaParado = new NuiLabel
                {
                    Text = "Nuvio started, but the TV stopped drawing its window.\n\n" +
                           "Drawn " + trocados + " frame(s), " + chamadas + " draw call(s), none for " + (parado / 1000) + " s. " +
                           "Main window visible=" + principalVisivel + ", Nuvio window visible=" + glVisivel + ", raised " + subidas + " time(s).\n" +
                           "TV " + Tv() + " / host " + Host + " / " + RuntimeInformation.FrameworkDescription +
                           (etapaAnterior != null ? "\nPrevious launch stopped at: " + etapaAnterior : "") +
                           "\n\nPlease post a PHOTO of this screen in issue #170 on GitHub (iqui27/nuvio-native-legacy). Back closes.",
                    MultiLine = true, TextColor = NuiColor.White, PointSize = 20,
                    Size2D = new Size2D(W - 160, H - 160), Position2D = new Position2D(80, 80),
                };
                w.Add(telaParado);
                w.Raise();
            }
            catch (Exception e) { Etapa("note stalled screen failed " + e.GetType().Name + ": " + e.Message); }
        }

        void TiraTelaParado()
        {
            var t = telaParado;
            telaParado = null;
            if (t == null) return;
            try { NuiWindow.Instance.Remove(t); t.Dispose(); NuiWindow.Instance.BackgroundColor = NuiColor.Transparent; gl?.Raise(); } catch { }
        }

        // ================= ERRO FATAL NA TELA =================

        bool erroNaTela;

        // Em vez de fechar em silencio: o motivo na tela, com a TV, para foto.
        void Erro(string titulo, string detalhe)
        {
            // Sem `fim`: o relogio (se ja existir) fecharia o app antes da foto.
            erroNaTela = true;
            Etapa("note error screen: " + titulo + " " + detalhe);
            if (gl != null) { try { gl.Hide(); } catch { } }
#if NV_API11
            if (glView != null) { try { glView.Hide(); } catch { } }
#endif
            try
            {
                var w = NuiWindow.Instance;
                w.BackgroundColor = NuiColor.Black;
                var t = new NuiLabel
                {
                    Text = titulo + "\n\n" + detalhe + "\n\nTV " + Tv() + " / host " + Host + " / " + RuntimeInformation.FrameworkDescription +
                           (etapaAnterior != null ? "\nPrevious launch stopped at: " + etapaAnterior : "") +
                           "\n\nPlease post a PHOTO of this screen in issue #137 on GitHub (iqui27/nuvio-native-legacy). Back closes.",
                    MultiLine = true, TextColor = NuiColor.White, PointSize = 20,
                    Size2D = new Size2D(W - 160, H - 160), Position2D = new Position2D(80, 80),
                };
                w.Add(t);
                w.KeyEvent += (s, e) => { if (e.Key.State == Key.StateType.Down && (e.Key.KeyPressedName == "XF86Back" || e.Key.KeyPressedName == "Escape")) Sair("error screen back"); };
                // No Tizen 9 outra janela pode estar por cima: esta vem para a frente.
                w.Show();
                w.Raise();
            }
            catch (Exception e) { Etapa("note error screen failed " + e.GetType().Name + ": " + e.Message); }
        }

        // ================= RASTRO DE ETAPAS =================
        // data/tpk-etapas.txt: "host begin X" / "host ok X" / "host fail X" /
        // "host note ...", uma linha por append, sem handler de sinal (o CoreCLR
        // e dono deles). No arranque seguinte vira tpk-etapas-anterior.txt; se
        // acabou numa etapa sem ok, a tela mostra qual por 30 s, e as linhas
        // entram no nuvio.log (LogaTv) para subir com o envio automatico.

        static string etapasArq = "";
        static readonly object travaEtapa = new object();
        static string etapaAnterior;
        static int etapasEscritas;

        static void Etapa(string linha)
        {
            if (string.IsNullOrEmpty(etapasArq)) return;
            // Teto: o vigia e os eventos de janela nao podem encher a pasta.
            if (Interlocked.Increment(ref etapasEscritas) > 400) return;
            string l = "host " + linha.Replace('\n', ' ') + " @" + (relogio.ElapsedMilliseconds / 1000.0).ToString("0.00") + "s";
            try { lock (travaEtapa) File.AppendAllText(etapasArq, l + "\n"); } catch { }
            if (soCarregada) { try { nv_tpk_log("[etapa] " + l); } catch { } }
        }

        static string RodarEtapas(string dados)
        {
            string atual = IOPath.Combine(dados, "tpk-etapas.txt");
            string anterior = IOPath.Combine(dados, "tpk-etapas-anterior.txt");
            string parou = null;
            try
            {
                if (File.Exists(atual))
                {
                    if (File.Exists(anterior)) File.Delete(anterior);
                    File.Move(atual, anterior);
                }
                if (File.Exists(anterior)) parou = EtapaAberta(File.ReadAllLines(anterior));
            }
            catch { }
            etapasArq = atual;
            return parou;
        }

        // "begin X" abre, "ok X"/"fail X" fecham. Devolve a ULTIMA aberta, com os
        // numeros da ultima nota do vigia (se houver); null = nada ficou aberto.
        // first-key sozinha numa sessao que terminou normalmente nao conta: e so
        // alguem que abriu e fechou sem apertar tecla.
        static string EtapaAberta(string[] linhas)
        {
            var abertas = new List<string>();
            bool terminou = false, estavel = false;
            string ultimaNota = null;
            foreach (var l in linhas)
            {
                var p = l.Split(new[] { ' ' }, 4, StringSplitOptions.RemoveEmptyEntries);
                if (p.Length < 3) continue;
                string verbo = p[1], etapa = p[2];
                if (verbo == "begin") { abertas.Remove(etapa); abertas.Add(etapa); }
                else if (verbo == "ok" || verbo == "fail")
                {
                    abertas.Remove(etapa);
                    if (verbo == "ok" && etapa == "steady-frames") estavel = true;
                }
                else if (verbo == "note")
                {
                    if (etapa == "terminate") terminou = true;
                    if (l.Contains("calls=")) ultimaNota = l.Substring(l.IndexOf("note ") + 5);
                }
            }
            if (abertas.Count == 0) return null;
            // Background eviction may skip OnTerminate. A steady run or the last
            // paused/unfocused state is normal; unfinished startup stages still warn.
            bool emSegundoPlano = ultimaNota != null &&
                (ultimaNota.Contains("paused=True") || ultimaNota.Contains("focus=False"));
            if (terminou || estavel || emSegundoPlano)
            { abertas.Remove("nui-run"); abertas.Remove("first-key"); }
            if (abertas.Count == 0) return null;
            string r = abertas[abertas.Count - 1];
            if (terminou) r += " (then the TV closed it)";
            if (ultimaNota != null) r += " — last: " + ultimaNota;
            return r;
        }

        // Aviso NAO fatal do arranque anterior: faixa preta no topo, numa janela
        // propria por cima do GLWindow, por 30 s ou ate a primeira tecla.
        NuiWindow janelaAviso;
        NuiLabel avisoRotulo;   // GLView: o aviso e um rotulo na janela principal, sem janela nova

        void Aviso(string texto)
        {
            try
            {
                Etapa("note showing notice: " + texto);
                if (JanelaUnica)
                {
                    var r = new NuiLabel
                    {
                        Text = texto + "\nPlease photograph this and post it in GitHub issue #170 (iqui27/nuvio-native-legacy). Any key hides it.",
                        MultiLine = true, TextColor = NuiColor.White, PointSize = 16, BackgroundColor = NuiColor.Black,
                        Size2D = new Size2D(W, 240), Position2D = new Position2D(0, 0), Padding = new Extents(40, 40, 10, 10),
                    };
                    avisoRotulo = r;
                    NuiWindow.Instance.Add(r);
                    var t1 = new NuiTimer(30000);
                    t1.Tick += (s, e) => { FechaAviso(); return false; };
                    t1.Start();
                    return;
                }
                janelaAviso = new NuiWindow("NuvioAviso", new NuiRect(0, 0, W, 240), false);
                janelaAviso.BackgroundColor = NuiColor.Black;
                janelaAviso.Add(new NuiLabel
                {
                    Text = texto + "\nPlease photograph this and post it in GitHub issue #170 (iqui27/nuvio-native-legacy). Any key hides it.",
                    MultiLine = true, TextColor = NuiColor.White, PointSize = 16,
                    Size2D = new Size2D(W - 80, 220), Position2D = new Position2D(40, 10),
                });
                janelaAviso.KeyEvent += (s, e) => { FechaAviso(); Tecla(e.Key, "notice"); };
                janelaAviso.Show();
                var t = new NuiTimer(30000);
                t.Tick += (s, e) => { FechaAviso(); return false; };
                t.Start();
            }
            catch (Exception e) { Etapa("note notice failed " + e.GetType().Name + ": " + e.Message); janelaAviso = null; }
        }

        void FechaAviso()
        {
            var r = avisoRotulo;
            if (r != null)
            {
                avisoRotulo = null;
                try { NuiWindow.Instance.Remove(r); r.Dispose(); } catch { }
                return;
            }
            var j = janelaAviso;
            janelaAviso = null;
            if (j == null) return;
            try { j.Hide(); } catch { }
            try { gl?.Raise(); } catch { }
        }

        // ================= TECLAS, QUADRO, CICLO DE VIDA =================

        bool primeiraTecla;

        void Tecla(Key k, string janela)
        {
            if (!primeiraTecla && k.State == Key.StateType.Down)
            {
                primeiraTecla = true;
                Etapa("ok first-key " + k.KeyPressedName + " via " + janela + Contagem());
            }
            if (avisoRotulo != null && k.State == Key.StateType.Down) FechaAviso();
            if (telaParado != null && k.State == Key.StateType.Down && (k.KeyPressedName == "XF86Back" || k.KeyPressedName == "Escape")) { Sair("stalled screen back"); return; }
            nv_tpk_tecla(k.KeyPressedName, k.State == Key.StateType.Down ? 1 : 0);
        }

        // Fio de desenho do NUI. 1 = troca, 0 = pula, -1 = o app acabou.
        int Quadro()
        {
            Interlocked.Exchange(ref ultimaChamadaMs, relogio.ElapsedMilliseconds);
            if (Interlocked.Increment(ref chamadas) == 1) Etapa("note first draw call");
            if (fim || erroNaTela) return 0;
            int r;
            try { r = nv_tpk_quadro(); }
            catch (Exception e)
            {
                Etapa("fail native-frame " + e.GetType().Name + ": " + e.Message);
                return 0; // The UI-thread watchdog presents diagnostics.
            }
            // De novo na saida: na API8 esta chamada roda no fio principal e pode
            // esperar o app terminar um quadro longo; o vigia (mesmo fio) nao
            // pode ler isso como "o framework parou de chamar".
            Interlocked.Exchange(ref ultimaChamadaMs, relogio.ElapsedMilliseconds);
            if (r < 0)
            {
                if (trocados > 0) fim = true;
                else Etapa("fail native-frame before first-frame");
            }
            if (r > 0)
            {
                int n = Interlocked.Increment(ref trocados);
                if (n == 1) { Etapa("ok first-frame" + Contagem()); Etapa("begin first-key"); Etapa("begin steady-frames"); }
                else if (n == 30) Etapa("ok steady-frames" + Contagem());
            }
            return r > 0 ? 1 : 0;
        }

        // #684: T7NSN5 pauses at 3.7-4.2 s with only two frames drawn.
        // ponytail: one bounded retry; real Home presses after interaction or
        // startup must stay paused. TV validation is needed for this heuristic.
        bool RecuperaPausaInicial() => !jaPausou && !primeiraTecla && !saindo &&
            !fim && !erroNaTela && trocados < 3 && relogio.ElapsedMilliseconds < 8000;

        protected override void OnPause()
        {
            bool recuperar = RecuperaPausaInicial();
            pausado = true;
            if (!jaPausou) { jaPausou = true; Janela("primeira pausa (canario 8: video no gl, gl opaco, sem prime, sobe no appcontrol; ela ainda vem?)"); }
            Etapa("note pause" + Contagem() + " mainVisible=" + principalVisivel + (JanelaUnica ? " (glview)" : " glVisible=" + glVisivel));
#if !NV_TEXTO
            try { voz?.Cancelar(); } catch { }
#endif
            try { nv_tpk_ciclo(0); } catch (EntryPointNotFoundException) { }
            video?.PausarPeloSistema();
            base.OnPause();
            if (recuperar)
            {
                // Firmware hides the main window and then GL after OnPause.
                // Wait for both events; an immediate raise is undone by that hide.
                var t = new NuiTimer(100);
                t.Tick += (s, e) => {
                    if (saindo || soltou || fim || erroNaTela || primeiraTecla ||
                        !pausado || relogio.ElapsedMilliseconds >= 8000) return false;
                    if (principalVisivel || (!JanelaUnica && glVisivel)) return true;
                    Etapa("note recovering startup pause" + Contagem());
                    try { NuiWindow.Instance.Show(); NuiWindow.Instance.Raise(); }
                    catch (Exception x) { Etapa("note startup raise failed " + x.GetType().Name + ": " + x.Message); }
                    SobeGl("startup pause");
                    OnResume();
                    return false;
                };
                t.Start();
            }
        }

        protected override void OnLowMemory(Tizen.Applications.LowMemoryEventArgs e)
        {
            try { nv_tpk_ciclo(1); } catch (EntryPointNotFoundException) { }
            base.OnLowMemory(e);
        }

        protected override void OnResume()
        {
            pausado = false;
            Etapa("note resume" + Contagem() + " mainVisible=" + principalVisivel + (JanelaUnica ? " (glview)" : " glVisible=" + glVisivel));
            base.OnResume();
        }

        protected override void OnTerminate()
        {
            Etapa("note terminate" + Contagem());
            if (JANELA_SAIDA_LIMPA) { SoltaTudo("terminate"); ArmaSaidaForcada("terminate"); }
            else video?.Parar();
            base.OnTerminate();
        }

        protected override void OnAppControlReceived(Tizen.Applications.AppControlReceivedEventArgs e)
        {
            string op = "?";
            try { op = e?.ReceivedAppControl?.Operation ?? "?"; } catch { }
            Etapa("note appcontrol op=" + op + Contagem() + " mainVisible=" + principalVisivel + " glVisible=" + glVisivel);
            try { base.OnAppControlReceived(e); } catch (Exception x) { Etapa("note appcontrol base threw " + x.GetType().Name + ": " + x.Message); }
            // Cleanup has already disposed the player and hidden the windows.
            // Never try to revive that instance, including a queued raise.
            if (saindo || soltou)
            {
                Etapa("note appcontrol while exiting: finishing exit");
                ArmaSaidaForcada("appcontrol while exiting");
                Exit();
                return;
            }
            if (JanelaUnica) { Janela("appcontrol: janela unica (GLView), nada a subir"); return; }
            if (!JANELA_SOBE_GL_APPCONTROL || gl == null) return;
#if NV_API8
            if (principalVisivel && glVisivel) { Janela("appcontrol: janelas visiveis, nada a subir"); return; }
#endif
            try
            {
                var t = new NuiTimer(300);
                t.Tick += (s, a) => {
                    if (saindo || soltou) return false;
                    Janela("appcontrol: sobe o gl");
                    SobeGl("appcontrol");
                    pausado = false; // Some firmware sends AppControl without OnResume.
                    return false;
                };
                t.Start();
            }
            catch (Exception x) { Janela("appcontrol: timer falhou " + x.GetType().Name + ": " + x.Message); }
        }

        // ================= SAIDA (canario de janela, problema 2) =================

        bool saindo;

        // Todo fechamento pedido pelo app passa aqui (fim do main, Voltar nas
        // telas de erro). Sem JANELA_SAIDA_LIMPA e o Parar()+Exit() de antes.
        void Sair(string porque)
        {
            if (saindo) return;
            saindo = true;
            Etapa("note exit begin (" + porque + ")" + Contagem());
            if (!JANELA_SAIDA_LIMPA) { video?.Parar(); Exit(); return; }
            SoltaTudo(porque);
            ArmaSaidaForcada(porque);
            Exit();
        }

        bool soltou;

        // Player solto por inteiro (Display nenhum antes do Dispose) e as duas
        // janelas escondidas, para a TV voltar a tela dela sem esperar o
        // processo sumir. Cada passo isolado: um que falhe nao segura o resto.
        void SoltaTudo(string porque)
        {
            if (soltou) return;
            soltou = true;
#if !NV_TEXTO
            try { voz?.Cancelar(); } catch { }
#endif
            try { video?.Encerrar(); Janela("saida: player solto (" + porque + ")"); } catch (Exception e) { Janela("saida: player falhou " + e.GetType().Name + ": " + e.Message); }
#if NV_API11
            try { glView?.Hide(); } catch { }
#endif
            try { gl?.Hide(); Janela("saida: gl escondido"); } catch (Exception e) { Janela("saida: gl.Hide falhou " + e.GetType().Name + ": " + e.Message); }
            try { NuiWindow.Instance.Hide(); Janela("saida: principal escondida"); } catch (Exception e) { Janela("saida: principal.Hide falhou " + e.GetType().Name + ": " + e.Message); }
        }

        [DllImport("libc.so.6", EntryPoint = "_exit")] static extern void c_exit(int code);
        bool forcadaArmada;

        // Fio de fundo: se o Exit() normal nao levar o processo em
        // JANELA_SAIDA_PRAZO_MS, anota no rastro (o proximo arranque le) e
        // sai por _exit. Com o Exit() normal funcionando, o processo acaba
        // antes e este fio morre junto.
        void ArmaSaidaForcada(string porque)
        {
            if (forcadaArmada) return;
            forcadaArmada = true;
            try
            {
                var t = new Thread(() =>
                {
                    Thread.Sleep(JANELA_SAIDA_PRAZO_MS);
                    Etapa("note exit watchdog: process still alive " + JANELA_SAIDA_PRAZO_MS + " ms after exit (" + porque + "), forcing _exit");
                    try { c_exit(0); } catch { Environment.Exit(0); }
                }) { IsBackground = true, Name = "nuvio-saida" };
                t.Start();
            }
            catch (Exception e) { Etapa("note exit watchdog failed " + e.GetType().Name + ": " + e.Message); }
        }

        // ================= LOG DE JANELA =================
        // "[janela] ..." no nuvio.log (o que sobe para o D1) e nota no rastro.
        // Antes de o main() do app redirecionar o stdout (~3 s), as linhas
        // ficam guardadas e saem logo depois da linha [tv].
        readonly List<string> janelaFila = new List<string>();

        // TECLAS DE MIDIA (#196). Sem reserva, o play/pause do controle chega ao
        // app E ao sistema, e a TV mostra "Not Available" por cima do player (o
        // flutter-tizen viu o mesmo toast, issue 319, e resolveu no engine #234
        // com ecore_wl2_window_keygrab_set TOPMOST). TOPMOST: a janela so recebe
        // a tecla com exclusividade quando esta no topo; e o unico modo de app
        // de terceiro e dispensa o privilegio keygrab (Ecore_Wayland2.h).
        // tv.inputdevice e privilegio so de web (privilege-wrt.properties do
        // tizen-6.0/tv): o manifesto nativo fica como esta.
        //
        // Por nome e nao pelo Window.GrabKey(int) do NUI: o GrabKey traduz o
        // codigo DALi por uma tabela (key-mapping-ecore-wl.cpp) que nao tem o
        // XF86PlayBack do Smart Remote 2021+, e o GLWindow nem tem GrabKey. O
        // GLWindow nao expoe o Ecore_Wl2_Window, entao varre os ids das janelas
        // wl2 deste processo (ecore_wl2_window_find) e reserva em todas: com
        // TOPMOST so a que estiver no topo recebe. O GrabKey do NUI fica de
        // reserva se a libecore_wl2 nao responder. Nada aqui derruba o app.
        static readonly string[] TeclasMidia = {
            "XF86AudioPlay", "XF86AudioPause", "XF86AudioPlayPause", "XF86PlayBack",
            "XF86AudioStop", "XF86AudioRewind", "XF86AudioForward",
            "XF86AudioNext", "XF86AudioPrev", "XF86NextChapter", "XF86PreviousChapter",
            // CH+/CH-: sem reserva a TV os gasta trocando o canal da antena.
            "XF86RaiseChannel", "XF86LowerChannel",
            // GUIA (entre CH+ e CH-): sem reserva a TV abre o guia dela e o app
            // sai (dono, 05/10). O app abre o Guia de TV dele (tpkteclas.c).
            "XF86ChannelGuide", "XF86ChannelList",
        };

        void ReservaTeclasMidia()
        {
            const int TOPMOST = 2; // ECORE_WL2_WINDOW_KEYGRAB_TOPMOST
            try
            {
                var janelas = new List<IntPtr>();
                for (uint id = 0; id < 64; id++)
                {
                    IntPtr w = ecore_wl2_window_find(id);
                    if (w != IntPtr.Zero && !janelas.Contains(w)) janelas.Add(w);
                }
                if (janelas.Count == 0) throw new Exception("nenhuma janela wl2 achada");
                var ok = new List<string>();
                var falhou = new List<string>();
                foreach (var k in TeclasMidia)
                {
                    int n = 0;
                    foreach (var w in janelas) { try { if (ecore_wl2_window_keygrab_set(w, k, 0, 0, 0, TOPMOST) != 0) n++; } catch { } }
                    if (n > 0) ok.Add(k + "(" + n + ")"); else falhou.Add(k);
                }
                LogHost("teclas de midia reservadas: " + (ok.Count > 0 ? string.Join(" ", ok) : "nenhuma") +
                        " | janelas wl2=" + janelas.Count + " modo=topmost" +
                        (falhou.Count > 0 ? " | recusadas: " + string.Join(" ", falhou) : ""));
            }
            catch (Exception e)
            {
                // Reserva: o GrabKey do NUI, codigos DALi (key.h do dali-adaptor)
                // PLAY_CD 172, STOP_CD 173, PAUSE_CD 174, NEXT_SONG 175,
                // PREVIOUS_SONG 176, REWIND 177, FASTFORWARD 178, PLAY_PAUSE 180.
                // Nao o DALI_KEY_PAUSE (170): na tabela ele e o XF86Standby.
                var ok = new List<string>();
                foreach (int c in new[] { 172, 173, 174, 175, 176, 177, 178, 180 })
                {
                    try { if (NuiWindow.Instance.GrabKey(c, NuiWindow.KeyGrabMode.Topmost)) ok.Add(c.ToString()); } catch { }
                }
                LogHost("teclas de midia reservadas: ecore_wl2 falhou (" + e.GetType().Name + ": " + e.Message +
                        "); NUI GrabKey topmost na principal, codigos DALi: " + (ok.Count > 0 ? string.Join(" ", ok) : "nenhum"));
            }
        }

        void LogHost(string linha)
        {
            Etapa("note " + linha);
            Fila("[host] " + linha);
        }

        void Janela(string linha, bool rastro = true)
        {
            if (rastro) Etapa("note janela " + linha);
            Fila("[janela] " + linha + " t=" + (relogio.ElapsedMilliseconds / 1000.0).ToString("0.0") + "s");
        }

        void Fila(string l)
        {
            if (tvLogada && video != null) { video.Log(l); return; }
            lock (janelaFila) { if (janelaFila.Count < 60) janelaFila.Add(l); }
        }

        void DespejaJanela()
        {
            if (video == null) return;
            string[] ls;
            lock (janelaFila) { ls = janelaFila.ToArray(); janelaFila.Clear(); }
            foreach (var l in ls) video.Log(l);
        }

        static void Main(string[] args)
        {
            relogio.Start();
            // Application.DirectoryInfo requires an existing NUIApplication.
            // Read its native data path first so constructor failures survive.
            try
            {
                IntPtr path = app_get_data_path();
                try
                {
                    string dados = Marshal.PtrToStringAnsi(path);
                    if (!string.IsNullOrEmpty(dados)) etapaAnterior = RodarEtapas(dados);
                }
                finally { if (path != IntPtr.Zero) free(path); }
            }
            catch (Exception e) { Console.Error.WriteLine("Startup trace unavailable: " + e.Message); }
            AppDomain.CurrentDomain.UnhandledException += (s, e) => Etapa("note unhandled " + e.ExceptionObject);
            Etapa("note entry host=" + Host + " process=" + RuntimeInformation.ProcessArchitecture + " os=" + RuntimeInformation.OSArchitecture);
            try
            {
                Etapa("begin nui-construct");
                var app = new Program();
                Etapa("ok nui-construct");
                Etapa("begin nui-run");
                app.Run(args);
                Etapa("ok nui-run");
            }
            catch (Exception e) { Etapa("fail nui-start " + e); throw; }
        }
    }
}
