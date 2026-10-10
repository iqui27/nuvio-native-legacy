// Host do Nuvio para Tizen 4.0/5.0 (TVs 2018-2019).
//
// Essas APIs nao tem GLWindow. O jeito da Samsung e a TVGLApplication
// (pacote Tizen.NET.TV): ela cria a janela GL e chama OnUpdate a cada quadro
// com o contexto corrente; devolvendo true, ela mesma troca os buffers. E o
// molde do JuvoPlayer.OpenGL (github.com/SamsungDForum/JuvoPlayer), inclusive
// no video: uma janela ElmSharp separada, mostrada e rebaixada (Lower), so
// para o player — o GL fica por cima e o C abre o furo onde o video aparece.
// Dai para frente e o mesmo protocolo do Tizen 6+ (src/tpk.c, Video.cs).
//
// ------------------------------------------------------------------------
// CARGA DA libnuvio.so NA TIZEN 4/5 (UEP)
// ------------------------------------------------------------------------
// Numa TV Tizen 4.0 real (optiman QE55Q6FNA) o NvUepProbe/NvMemfd (spike,
// #137) provou:
//   A. dlopen de ARQUIVO do pacote (lib/) -> BLOQUEADO pela UEP
//      ("failed to map segment from shared object")
//   B. dlopen de ARQUIVO em data/         -> tambem BLOQUEADO
//   C. memfd_create por NOME (DllImport)  -> nao existe na libc do Tizen 4/5
//   D. mmap +EXEC de memoria ANONIMA      -> PERMITIDO
//
// Ou seja: mapear PROT_EXEC de um inode de arquivo e barrado, mas memoria
// anonima executavel passa. Dai as duas rotas deste host, em ordem:
//
//   1. memfd (rota preferida): syscall(385) cria um fd anonimo, escrevemos a
//      libnuvio.so nele e damos dlopen("/proc/self/fd/N"). O fd fica ABERTO a
//      vida toda do processo (o /proc/self/fd/N so existe enquanto ele vive).
//      Se passar, o handle e um handle dlopen NORMAL e a lib entra no link map
//      do loader — entao um [DllImport("libnuvio.so")] resolveria pelo soname
//      sozinho. Mesmo assim resolvemos os simbolos por dlsym(handle, ...) e
//      montamos o mesmo despacho por ponteiro de funcao da rota 2, para o
//      resto do app ter UM SO caminho de chamada nas duas rotas. (Ver NvLib.)
//
//   2. Carregador de ELF proprio (rota D): mmap anon RW, mapeia os PT_LOAD,
//      aplica relocacoes R_ARM_RELATIVE/GLOB_DAT/JUMP_SLOT/ABS32, resolve
//      externos por dlsym(RTLD_DEFAULT) (com as NEEDED e libGLESv2/libEGL
//      pre-carregadas GLOBAL para os simbolos aparecerem), roda DT_INIT_ARRAY,
//      mprotect +EXEC por segmento e resolve os nv_tpk_* pelo dynsym. Aqui a
//      lib NAO esta no link map: DllImport-por-soname NAO resolveria. Por isso
//      TODO ponto de entrada do libnuvio e chamado por ponteiro de funcao
//      (NvLib), obtido do dynsym do carregador. A memoria fica viva a vida
//      toda do processo (nunca munmap na rota vencedora).
//
//      O QUE ESTE CARREGADOR NAO FAZ, E POR ISSO RECUSA (#180): TLS de
//      compilador (PT_TLS, R_ARM_TLS_*), relocacoes de outros tipos e simbolos
//      indefinidos nao-fracos que o dlsym nao acha. Antes ele pulava tudo isso
//      em silencio, e a .so carregava "bem" para morrer no primeiro acesso a
//      uma variavel _Thread_local — a primeira requisicao HTTPS, no fio do
//      login. A libnuvio.so deste pacote e compilada a parte (tools/tpk.sh,
//      -DNV_TPK40) sem TLS e com DT_HASH; se um dia voltar a ter, a tela de
//      erro diz exatamente o que, em vez de fechar sem aviso.
//
//   3. Se as DUAS falharem, a tela de erro mostra as mensagens reais.
//
// RASTRO DE ETAPAS (#180): data/tpk-etapas.txt recebe "begin X" / "ok X" /
// "fail X" / "note ..." deste host e do C (nv_tpk40_etapa). No arranque o
// arquivo anterior vira tpk-etapas-anterior.txt e, se ele acabou numa etapa
// sem "ok", a tela mostra qual por alguns segundos ("Previous launch stopped
// at: ...") e o app segue. Nenhum handler de sinal: o runtime .NET e dono de
// SIGSEGV/SIGABRT, e o rastro nao depende deles.
//
// Os alvos Tizen 6+ (NuvioTpk/60/65) nao mudam: la o dlopen da .so do pacote e
// permitido e o host usa DllImport direto (tizen-tpk/Program.cs). So este
// arquivo, compilado unicamente no pacote NuvioTpk40, muda.
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using ElmSharp;
using Tizen.System;
using Tizen.TV.NUI.GLApplication;
using GLKey = Tizen.TV.NUI.GLApplication.Key;

namespace NuvioTpk
{
    // Despacho unico para os pontos de entrada do libnuvio. Nas duas rotas de
    // carga (memfd e carregador ELF) estes delegates sao apontados para o
    // codigo nativo por ponteiro de funcao, entao Program40/Video chamam sempre
    // por aqui, sem depender de o DllImport-por-soname resolver.
    static class NvLib
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate int IniciarDel(string arte, string dados, int w, int h);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void ConfigDel(int esperaMs, int swapZero);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate int QuadroDel();
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate IntPtr ErroDel();
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void TeclaDel(string nome, int apertou);

        // Guardados em campos estaticos: seguram o delegate vivo (o GC nao o
        // recolhe) e o ponteiro nativo e estavel pela vida do processo.
        public static IniciarDel Iniciar;
        public static ConfigDel Config;
        public static QuadroDel Quadro;
        public static ErroDel Erro;
        public static TeclaDel Tecla;

        public static bool Pronto => Iniciar != null && Config != null && Quadro != null && Erro != null && Tecla != null;
    }

    class Program : TVGLApplication
    {
        [DllImport("libdl.so.2")] static extern IntPtr dlopen(string path, int flags);
        [DllImport("libdl.so.2")] static extern IntPtr dlsym(IntPtr h, string sym);
        [DllImport("libdl.so.2")] static extern IntPtr dlerror();
        [DllImport("libecore_wayland.so.1")] static extern IntPtr ecore_wl_window_find(uint id);
        [DllImport("libecore_wayland.so.1")] static extern byte ecore_wl_window_keygrab_set(IntPtr win, string key, int mod, int notMod, int priority, int modo);
        [DllImport("libecore_wl2.so.1")] static extern IntPtr ecore_wl2_window_find(uint id);
        [DllImport("libecore_wl2.so.1")] static extern byte ecore_wl2_window_keygrab_set(IntPtr win, string key, int mod, int notMod, int priority, int modo);
        // libc por NUMERO de syscall (a libc do Tizen 4/5 nao exporta
        // memfd_create como simbolo, so o dispatcher generico syscall()).
        [DllImport("libc.so.6", SetLastError = true, EntryPoint = "syscall")]
        static extern int syscall_memfd(int number, IntPtr name, int flags);
        [DllImport("libc.so.6", SetLastError = true, EntryPoint = "syscall")]
        static extern int syscall_cache(int number, IntPtr start, IntPtr end, int flags);
        [DllImport("libc.so.6", SetLastError = true)] static extern IntPtr write(int fd, IntPtr buf, IntPtr count);
        [DllImport("libc.so.6", SetLastError = true)] static extern IntPtr mmap(IntPtr addr, IntPtr len, int prot, int flags, int fd, IntPtr off);
        [DllImport("libc.so.6", SetLastError = true)] static extern int mprotect(IntPtr addr, IntPtr len, int prot);
        [DllImport("libc.so.6", SetLastError = true)] static extern int munmap(IntPtr addr, IntPtr len);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void InitFn();

        const int RTLD_NOW = 2, RTLD_GLOBAL = 0x100, RTLD_LAZY = 1;
        static readonly IntPtr RTLD_DEFAULT = IntPtr.Zero;   // glibc
        const int PROT_READ = 1, PROT_WRITE = 2, PROT_EXEC = 4;
        const int MAP_PRIVATE = 2, MAP_ANONYMOUS = 0x20;    // ARM/Linux
        const int SYS_memfd_create = 385;                    // ARM EABI (armv7)
        const int ARM_NR_cacheflush = 0x0f0002;              // __ARM_NR_cacheflush
        const int PAGE = 4096;
        const int W = 1920, H = 1080;
        // ELF32
        const uint PT_LOAD = 1, PT_DYNAMIC = 2, PT_TLS = 7;
        const int STB_WEAK = 2;
        // Relocacoes ARM que o carregador sabe aplicar. Qualquer outra recusa.
        const int R_ARM_NONE = 0, R_ARM_ABS32 = 2, R_ARM_GLOB_DAT = 21, R_ARM_JUMP_SLOT = 22, R_ARM_RELATIVE = 23;

        // Mantidos vivos pela vida do processo: o fd do memfd (o /proc/self/fd/N
        // depende dele) e o mapeamento anonimo do carregador ELF.
        static int memfdFd = -1;
        static IntPtr elfMap = IntPtr.Zero;
        static long elfSpan = 0;

        // Rastro de etapas (ver o cabecalho). Vazio = ainda sem pasta de dados.
        static string etapasArq = "";

        Window janelaVideo, janelaErro, janelaAviso;
        Video video;
        bool rodando, fim, primeiroQuadro, teclasReservadas;

        protected override void OnCreate()
        {
            base.OnCreate();
            var dir = Tizen.Applications.Application.Current.DirectoryInfo;
            string dados = dir.Data;
            string raiz = Path.GetFullPath(Path.Combine(dir.Resource, ".."));
            string bundled = Path.Combine(raiz, "lib", "libnuvio.so");

            // AUTO-ATUALIZACAO: se ha uma libnuvio.so encenada e VERIFICADA mais
            // nova que a empacotada, CarregaNativo a carrega pela mesma rota memfd
            // (a lib do 4/5 SEMPRE entra por memfd/ELF, entao aqui so muda QUAL
            // arquivo). Se a encenada falhar por qualquer motivo, apaga o staging e
            // tenta a empacotada — atualizar nunca impede o app de abrir.
            string staged = NvCarga.DecidirStaged(dados, NvCarga.VersaoEmpacotada(dir.Resource), out string _);
            string so = staged ?? bundled;

            // Rastro: o arquivo do arranque anterior vira -anterior, e a etapa
            // em que ele parou (se parou) vai para a tela depois que o app subir.
            string etapaAnterior = RodarEtapas(dados);
            Etapa("note launch " + RuntimeInformation.FrameworkDescription);

            string falhaMemfd, falhaElf;
            if (!CarregaNativo(so, out falhaMemfd, out falhaElf))
            {
                if (staged != null)
                {
                    Etapa("note staged lib failed, falling back to the bundled one");
                    NvCarga.ApagarStaged(dados);
                    if (CarregaNativo(bundled, out falhaMemfd, out falhaElf)) goto carregado;
                }
                Etapa("note launch failed: native library did not load");
                Erro("A TV nao deixou o Nuvio nativo carregar.",
                     "memfd (syscall 385): " + falhaMemfd,
                     "carregador ELF: " + falhaElf,
                     "montagem: " + Montagem(raiz));
                return;
            }
            carregado:;

            try
            {
                // Janela do video, atras da janela GL (JuvoPlayer.OpenGL faz igual).
                Etapa("begin video-window");
                janelaVideo = new Window("NuvioVideo") { Geometry = new Rect(0, 0, W, H) };
                janelaVideo.Show();
                janelaVideo.Lower();
                video = new Video(() => new Tizen.Multimedia.Display(janelaVideo), a => EcoreMainloop.PostAndWakeUp(a), dados, W, H);
                video.GeometriaJanela = (gx, gy, gw, gh) =>
                {
                    try { janelaVideo.Geometry = new Rect(gx, gy, gw, gh); return null; }
                    catch (Exception e) { return e.GetType().Name + ": " + e.Message; }
                };
                Etapa("ok video-window");

                // OnUpdate SEMPRE devolve true, como o JuvoPlayer.OpenGL: com
                // false no arranque (app ainda sem quadro) a TVGLApplication
                // parava de chamar, o app nunca recebia o contexto e ficava
                // tela preta sem log (Tizen 5.0, 28/09). Espera sem limite:
                // no arranque limpa para preto, depois espera o quadro do app.
                NvLib.Config(-1, 0);
                Etapa("begin nv_tpk_iniciar");
                if (NvLib.Iniciar(Path.Combine(dir.Resource, "art"), dados, W, H) != 0)
                {
                    Etapa("fail nv_tpk_iniciar");
                    Erro("O Nuvio nao conseguiu iniciar.", Marshal.PtrToStringAnsi(NvLib.Erro()) ?? "nv_tpk_iniciar falhou");
                    return;
                }
                try { Apps.Registrar(a => EcoreMainloop.PostAndWakeUp(a)); } catch (Exception e) { Etapa("note apps-init " + e.GetType().Name); }
                Etapa("ok nv_tpk_iniciar");
            }
            catch (Exception e)
            {
                Etapa("note launch failed: " + e.GetType().Name + ": " + e.Message);
                Erro("O Nuvio nao conseguiu iniciar.", e.GetType().Name + ": " + e.Message);
                return;
            }
            rodando = true;
            Etapa("begin first-frame");
            if (etapaAnterior != null) Aviso("Previous launch stopped at: " + etapaAnterior);
            EcoreMainloop.AddTimer(0.25, () =>
            {
                if (!fim) { video.Tique(); return true; }
                video.Parar();
                string motivo = Marshal.PtrToStringAnsi(NvLib.Erro());
                Etapa("note app ended: " + (string.IsNullOrEmpty(motivo) ? "main returned" : motivo));
                if (string.IsNullOrEmpty(motivo)) Exit();
                else Erro("O Nuvio abriu, mas nao conseguiu desenhar na tela.", motivo,
                          "log: " + Path.Combine(dados, "nuvio.log"));
                return false;
            });
        }

        // Fio principal, contexto GL corrente. true = a TVGLApplication troca
        // os buffers; false = nada novo neste quadro.
        protected override bool OnUpdate()
        {
            // Nunca false enquanto o app vive: a TVGLApplication para de chamar.
            if (fim) return false;
            if (!rodando) return true;
            // Aqui a TVGLApplication ja criou a janela GL. Uma tentativa por
            // backend, nunca por quadro; TOPMOST acompanha o foco da janela.
            if (!teclasReservadas) { teclasReservadas = true; ReservaTeclasMidia(); }
            int r = NvLib.Quadro();
            if (r < 0) fim = true;
            if (r > 0 && !primeiroQuadro) { primeiroQuadro = true; Etapa("ok first-frame"); }
            return r > 0;
        }

        protected override void OnKeyEvent(GLKey k)
        {
            if (janelaErro != null)
            {
                if (k.State == GLKey.StateType.Down && (k.KeyPressedName == "XF86Back" || k.KeyPressedName == "Escape")) Exit();
                return;
            }
            // Qualquer tecla fecha o aviso do arranque anterior; a tecla segue
            // para o app normalmente.
            if (janelaAviso != null && k.State == GLKey.StateType.Down) FecharAviso();
            if (rodando) NvLib.Tecla(k.KeyPressedName, k.State == GLKey.StateType.Down ? 1 : 0);
        }

        protected override void OnPause()
        {
            video?.PausarPeloSistema();
            base.OnPause();
        }

        // #411: as mesmas teclas do host 6+ (#196), por nome, sem trocar o
        // OnKeyEvent. A janela GL nao e a janelaVideo ElmSharp que fica atras.
        // Ecore_Wayland.h (tizen_4.0) / Ecore_Wl2.h (tizen_5.0); provas e
        // limites em ENTREGA-411.md. Nao misturar os ponteiros dos backends.
        static readonly string[] TeclasMidia = {
            "XF86AudioPlay", "XF86AudioPause", "XF86AudioPlayPause", "XF86PlayBack",
            "XF86AudioStop", "XF86AudioRewind", "XF86AudioForward",
            "XF86AudioNext", "XF86AudioPrev", "XF86NextChapter", "XF86PreviousChapter",
            "XF86RaiseChannel", "XF86LowerChannel", "XF86ChannelGuide", "XF86ChannelList",
        };

        void ReservaTeclasMidia()
        {
            // Ambos podem estar instalados: achar a janela de video num deles
            // nao prova que achamos a GL. Tenta os dois independentemente.
            foreach (bool wl2 in new[] { false, true })
            {
                string backend = wl2 ? "ecore_wl2" : "ecore_wl";
                const int TOPMOST = 2;
                try
                {
                    var janelas = new List<IntPtr>();
                    // ponytail: mesmo limite de 64 ids do 6+, no arranque;
                    // enumerar Ecore_Evas se o host passar a criar mais janelas.
                    for (uint id = 0; id < 64; id++)
                    {
                        IntPtr w = wl2 ? ecore_wl2_window_find(id) : ecore_wl_window_find(id);
                        if (w != IntPtr.Zero && !janelas.Contains(w)) janelas.Add(w);
                    }
                    if (janelas.Count == 0) { Etapa("note teclas " + backend + ": nenhuma janela"); continue; }
                    foreach (string k in TeclasMidia)
                    {
                        foreach (IntPtr w in janelas)
                        {
                            try
                            {
                                byte ok = wl2 ? ecore_wl2_window_keygrab_set(w, k, 0, 0, 0, TOPMOST)
                                              : ecore_wl_window_keygrab_set(w, k, 0, 0, 0, TOPMOST);
                                Etapa("note teclas " + backend + " janela=" + w + " " + k +
                                      " topmost=" + (ok != 0 ? "ok" : "recusada"));
                            }
                            catch (Exception e) { Etapa("note teclas " + backend + " janela=" + w + " " + k + " falhou: " + e.GetType().Name + ": " + e.Message); }
                        }
                    }
                }
                catch (Exception e) { Etapa("note teclas " + backend + " indisponivel: " + e.GetType().Name + ": " + e.Message); }
            }
        }

        // ================= RASTRO DE ETAPAS =================

        // Uma linha, um append, sem buffer. Nunca lanca: rastro nao pode derrubar
        // o que ele vigia.
        static void Etapa(string linha)
        {
            if (string.IsNullOrEmpty(etapasArq)) return;
            try { File.AppendAllText(etapasArq, "host " + linha + "\n"); } catch { }
        }

        // Renomeia o rastro anterior e devolve a etapa em que ele parou: o ultimo
        // "begin X" sem "ok X"/"fail X" depois. null = terminou limpo ou nao ha.
        // O arquivo -anterior FICA em data/, para subir com o log quando o login
        // funcionar (o C tambem o copia para o nuvio.log; ver tpk.c).
        static string RodarEtapas(string dados)
        {
            string atual = Path.Combine(dados, "tpk-etapas.txt");
            string anterior = Path.Combine(dados, "tpk-etapas-anterior.txt");
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

        // Linhas "<origem> begin X ..." abrem X; "<origem> ok X ..." e
        // "<origem> fail X ..." fecham. Sobrou alguma aberta: devolve a ULTIMA
        // aberta (a mais funda), com a origem ("host"/"native").
        static string EtapaAberta(string[] linhas)
        {
            var abertas = new List<string>();
            var origem = new Dictionary<string, string>();
            foreach (var l in linhas)
            {
                var p = l.Split(new[] { ' ' }, 4, StringSplitOptions.RemoveEmptyEntries);
                if (p.Length < 3) continue;
                string quem = p[0], verbo = p[1], etapa = p[2];
                if (verbo == "begin") { abertas.Remove(etapa); abertas.Add(etapa); origem[etapa] = quem; }
                else if (verbo == "ok" || verbo == "fail") abertas.Remove(etapa);
            }
            if (abertas.Count == 0) return null;
            string ultima = abertas[abertas.Count - 1];
            return ultima + " (" + (origem.ContainsKey(ultima) ? origem[ultima] : "?") + ")";
        }

        // ================= CARGA NATIVA =================

        // Tenta as duas rotas; na que vencer, deixa NvLib.* apontando para o
        // codigo nativo. Devolve true se o app pode rodar.
        static bool CarregaNativo(string so, out string falhaMemfd, out string falhaElf)
        {
            falhaMemfd = falhaElf = null;
            byte[] elf;
            Etapa("begin read-so");
            try { elf = File.ReadAllBytes(so); }
            catch (Exception e)
            {
                falhaMemfd = falhaElf = "nao consegui ler " + so + ": " + e.Message;
                Etapa("fail read-so " + e.Message);
                return false;
            }
            Etapa("ok read-so " + elf.Length + " bytes");

            // Rota 1: memfd + dlopen(/proc/self/fd/N). Handle dlopen normal.
            Etapa("begin memfd");
            try
            {
                if (Metodo1_Memfd(elf, out falhaMemfd)) { Etapa("ok memfd"); Etapa("note loader=memfd"); return true; }
            }
            catch (Exception e) { falhaMemfd = "EXCECAO " + e.GetType().Name + ": " + e.Message; }
            Etapa("fail memfd " + falhaMemfd);

            // Rota 2: carregador de ELF em memoria anonima.
            Etapa("begin elf-loader");
            try
            {
                if (Metodo2_CarregadorElf(elf, out falhaElf)) { Etapa("ok elf-loader"); Etapa("note loader=elf"); return true; }
            }
            catch (Exception e) { falhaElf = "EXCECAO " + e.GetType().Name + ": " + e.Message; }
            Etapa("fail elf-loader " + falhaElf);

            return false;
        }

        // ---- Rota 1: memfd_create(385) + dlopen(/proc/self/fd/N) ----
        static bool Metodo1_Memfd(byte[] elf, out string falha)
        {
            falha = null;
            IntPtr nome = Marshal.StringToHGlobalAnsi("nuvio");
            int fd;
            try { fd = syscall_memfd(SYS_memfd_create, nome, 0); }
            finally { Marshal.FreeHGlobal(nome); }
            if (fd < 0) { falha = "syscall(385) falhou (errno=" + Marshal.GetLastWin32Error() + ")"; return false; }

            GCHandle pin = GCHandle.Alloc(elf, GCHandleType.Pinned);
            try
            {
                IntPtr baseP = pin.AddrOfPinnedObject();
                int off = 0;
                while (off < elf.Length)
                {
                    int n = (int)write(fd, baseP + off, (IntPtr)(elf.Length - off));
                    if (n <= 0) { falha = "write falhou em " + off + "/" + elf.Length + " (errno=" + Marshal.GetLastWin32Error() + ")"; return false; }
                    off += n;
                }
            }
            finally { pin.Free(); }

            // O fd fica ABERTO a vida toda: /proc/self/fd/N so existe com ele vivo.
            string path = "/proc/self/fd/" + fd;
            dlerror();
            IntPtr h = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
            if (h == IntPtr.Zero) { falha = "dlopen(" + path + ") -> " + Err(); return false; }
            memfdFd = fd;   // segura o fd

            // A lib entrou no link map: [DllImport("libnuvio.so")] resolveria
            // pelo soname sozinho. Mesmo assim ligamos por dlsym(handle) para o
            // resto do app ter o mesmo caminho de chamada da rota 2.
            if (!LigaSimbolos(s => dlsym(h, s), out string qual)) { falha = "handle ok mas simbolo faltou: " + qual; return false; }
            return true;
        }

        // ---- Rota 2: carregador de ELF32 ARM em memoria anonima (rota D) ----
        //
        // ORDEM: tudo que pode recusar vem ANTES de mapear e de tocar em
        // memoria — cabecalhos, PT_TLS, tipos de relocacao, DT_HASH, simbolos
        // indefinidos. Uma recusa aqui e uma frase clara na tela de erro; a
        // alternativa (aplicar so o que se sabe) foi o que escondeu o #180.
        static bool Metodo2_CarregadorElf(byte[] e, out string falha)
        {
            falha = null;
            if (e.Length < 52 || e[0] != 0x7F || e[1] != (byte)'E' || e[2] != (byte)'L' || e[3] != (byte)'F') { falha = "not an ELF file"; return false; }
            if (e[4] != 1) { falha = "not ELFCLASS32"; return false; }
            if (e[5] != 1) { falha = "not little-endian"; return false; }
            ushort machine = U16(e, 18);
            if (machine != 40) { falha = "e_machine=" + machine + ", expected 40 (ARM)"; return false; }

            uint phoff = U32(e, 28);
            ushort phentsize = U16(e, 42);
            ushort phnum = U16(e, 44);

            long minVa = long.MaxValue, maxVa = long.MinValue;
            uint dynVa = 0, dynOff = 0, dynSz = 0;
            for (int i = 0; i < phnum; i++)
            {
                int p = (int)(phoff + (uint)i * phentsize);
                uint ptype = U32(e, p);
                uint poff = U32(e, p + 4);
                uint pvaddr = U32(e, p + 8);
                uint pfilesz = U32(e, p + 16);
                uint pmemsz = U32(e, p + 20);
                if (ptype == PT_TLS)
                {
                    // Este carregador nao monta TLS de compilador (dtv, modulo,
                    // __tls_get_addr). Carregar assim mesmo = morrer no primeiro
                    // acesso a uma _Thread_local, longe daqui e sem mensagem.
                    falha = "the library has a PT_TLS segment (" + pmemsz + " bytes of thread-local storage); this loader cannot set up compiler TLS. Rebuild libnuvio.so with -DNV_TPK40 (tools/tpk.sh)";
                    return false;
                }
                if (ptype == PT_LOAD) { if (pvaddr < minVa) minVa = pvaddr; if (pvaddr + pmemsz > maxVa) maxVa = pvaddr + pmemsz; }
                else if (ptype == PT_DYNAMIC) { dynVa = pvaddr; dynOff = poff; dynSz = pfilesz; }
            }
            if (minVa == long.MaxValue) { falha = "no PT_LOAD segment"; return false; }
            if (dynVa == 0 || dynSz == 0) { falha = "no PT_DYNAMIC segment"; return false; }

            // Le a tabela dinamica AINDA NO ARQUIVO (offsets de arquivo), para
            // conferir tudo antes do mmap. So o que passar e mapeado.
            uint fSymtab = 0, fStrtab = 0, fHash = 0, fRel = 0, fRelsz = 0, fRelent = 8, fJmprel = 0, fPltrelsz = 0;
            var needed = new List<uint>();
            for (uint d = dynOff; d + 8 <= dynOff + dynSz; d += 8)
            {
                int tag = (int)U32(e, (int)d);
                uint val = U32(e, (int)d + 4);
                if (tag == 0) break;
                switch (tag)
                {
                    case 1: needed.Add(val); break;           // DT_NEEDED (indice em strtab)
                    case 4: fHash = val; break;               // DT_HASH
                    case 5: fStrtab = val; break;             // DT_STRTAB
                    case 6: fSymtab = val; break;             // DT_SYMTAB
                    case 17: fRel = val; break;               // DT_REL
                    case 18: fRelsz = val; break;             // DT_RELSZ
                    case 19: fRelent = val; break;            // DT_RELENT
                    case 23: fJmprel = val; break;            // DT_JMPREL
                    case 2: fPltrelsz = val; break;           // DT_PLTRELSZ
                }
            }
            if (fSymtab == 0 || fStrtab == 0) { falha = "no DT_SYMTAB/DT_STRTAB"; return false; }
            // DT_HASH e o unico jeito honesto de saber quantos simbolos ha no
            // dynsym (nchain). Sem ele o antigo chutava 4096 e lia alem da tabela.
            if (fHash == 0) { falha = "no DT_HASH (only GNU_HASH?); link libnuvio.so with -Wl,--hash-style=both (tools/tpk.sh)"; return false; }
            // Nas .so do gcc os enderecos virtuais das tabelas dinamicas coincidem
            // com os offsets de arquivo do primeiro PT_LOAD (p_offset 0, p_vaddr
            // 0). Confere em vez de supor.
            if (!VaEmArquivo(e, phoff, phentsize, phnum, fSymtab, out int oSymtab) ||
                !VaEmArquivo(e, phoff, phentsize, phnum, fStrtab, out int oStrtab) ||
                !VaEmArquivo(e, phoff, phentsize, phnum, fHash, out int oHash))
            { falha = "dynamic tables outside any PT_LOAD"; return false; }
            int nchain = (int)U32(e, oHash + 4);
            if (nchain <= 0 || nchain > 1000000) { falha = "DT_HASH nchain=" + nchain + " is not plausible"; return false; }

            // Tipos de relocacao: tudo que nao se sabe aplicar recusa AGORA.
            var tipos = new SortedDictionary<int, int>();
            if (!ContaRelocs(e, phoff, phentsize, phnum, fRel, fRelsz, fRelent < 8 ? 8 : fRelent, tipos, out falha)) return false;
            if (!ContaRelocs(e, phoff, phentsize, phnum, fJmprel, fPltrelsz, 8, tipos, out falha)) return false;
            foreach (var kv in tipos)
            {
                int t = kv.Key;
                if (t == R_ARM_NONE || t == R_ARM_ABS32 || t == R_ARM_GLOB_DAT || t == R_ARM_JUMP_SLOT || t == R_ARM_RELATIVE) continue;
                string nomeT = t == 17 ? "R_ARM_TLS_DTPMOD32" : t == 18 ? "R_ARM_TLS_DTPOFF32" : t == 19 ? "R_ARM_TLS_TPOFF32" : ("type " + t);
                falha = "unsupported relocation " + nomeT + " (" + kv.Value + " entries); this loader applies only RELATIVE/ABS32/GLOB_DAT/JUMP_SLOT";
                return false;
            }

            // Externos so aparecem no dlsym(RTLD_DEFAULT) se a lib que os define
            // estiver carregada GLOBAL. Pre-carrega as NEEDED da propria .so
            // (libc, libm, libpthread, libdl, librt, libGLESv2...) e o EGL.
            var pre = new List<string> { "libc.so.6", "libm.so.6", "libpthread.so.0", "libdl.so.2", "librt.so.1",
                                         "libEGL.so.1", "libGLESv2.so.2", "libEGL.so", "libGLESv2.so" };
            foreach (var idx in needed) pre.Add(LeCStr(e, oStrtab + (int)idx));
            foreach (var lib in pre) if (!string.IsNullOrEmpty(lib)) dlopen(lib, RTLD_NOW | RTLD_GLOBAL);

            // Simbolos indefinidos NAO-FRACOS: todos tem de existir. Antes, um que
            // faltasse virava 0 em silencio e o app morria no primeiro uso.
            var faltam = new List<string>();
            for (int i = 1; i < nchain; i++)
            {
                int sym = oSymtab + i * 16;
                if (sym + 16 > e.Length) break;
                uint stName = U32(e, sym);
                byte stInfo = e[sym + 12];
                ushort stShndx = U16(e, sym + 14);
                if (stShndx != 0) continue;                    // definido aqui
                if ((stInfo >> 4) == STB_WEAK) continue;       // fraco: pode faltar
                string nome = LeCStr(e, oStrtab + (int)stName);
                if (string.IsNullOrEmpty(nome)) continue;
                if (dlsym(RTLD_DEFAULT, nome) == IntPtr.Zero) { faltam.Add(nome); if (faltam.Count >= 6) break; }
            }
            if (faltam.Count > 0) { falha = "undefined symbols not found on this TV: " + string.Join(", ", faltam); return false; }

            // Daqui em diante e o carregador de sempre.
            long baseVa = minVa & ~(long)(PAGE - 1);
            long span = ((maxVa - baseVa) + PAGE - 1) & ~(long)(PAGE - 1);

            IntPtr map = mmap(IntPtr.Zero, (IntPtr)span, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, IntPtr.Zero);
            if (map == (IntPtr)(-1) || map == IntPtr.Zero) { falha = "mmap(" + span + ") failed (errno=" + Marshal.GetLastWin32Error() + ")"; return false; }
            long delta = map.ToInt64() - baseVa;

            for (int i = 0; i < phnum; i++)
            {
                int p = (int)(phoff + (uint)i * phentsize);
                if (U32(e, p) != PT_LOAD) continue;
                uint poff = U32(e, p + 4);
                uint pvaddr = U32(e, p + 8);
                uint pfilesz = U32(e, p + 16);
                Marshal.Copy(e, (int)poff, (IntPtr)(delta + pvaddr), (int)pfilesz);
            }

            long dynAddr = delta + dynVa;
            long symtab = 0, strtab = 0, hash = 0;
            long rel = 0, relsz = 0, relent = 8;
            long jmprel = 0, pltrelsz = 0;
            long initArray = 0, initArraySz = 0, initFn = 0;
            for (long d = dynAddr; ; d += 8)
            {
                int tag = Marshal.ReadInt32((IntPtr)d);
                uint val = (uint)Marshal.ReadInt32((IntPtr)(d + 4));
                if (tag == 0) break;
                switch (tag)
                {
                    case 4: hash = delta + val; break;        // DT_HASH
                    case 5: strtab = delta + val; break;      // DT_STRTAB
                    case 6: symtab = delta + val; break;      // DT_SYMTAB
                    case 17: rel = delta + val; break;        // DT_REL
                    case 18: relsz = val; break;              // DT_RELSZ
                    case 19: relent = val; break;             // DT_RELENT
                    case 23: jmprel = delta + val; break;     // DT_JMPREL
                    case 2: pltrelsz = val; break;            // DT_PLTRELSZ
                    case 12: initFn = delta + val; break;     // DT_INIT
                    case 25: initArray = delta + val; break;  // DT_INIT_ARRAY
                    case 27: initArraySz = val; break;        // DT_INIT_ARRAYSZ
                }
            }
            if (symtab == 0 || strtab == 0 || hash == 0) { munmap(map, (IntPtr)span); falha = "dynamic table changed after mapping"; return false; }

            if (!RelocaBloco(rel, relsz, relent, symtab, strtab, delta, out falha) ||
                !RelocaBloco(jmprel, pltrelsz, 8, symtab, strtab, delta, out falha))
            { munmap(map, (IntPtr)span); return false; }

            for (int i = 0; i < phnum; i++)
            {
                int p = (int)(phoff + (uint)i * phentsize);
                if (U32(e, p) != PT_LOAD) continue;
                uint pvaddr = U32(e, p + 8);
                uint pmemsz = U32(e, p + 20);
                uint pflags = U32(e, p + 24);   // PF_X=1 PF_W=2 PF_R=4
                long segStart = (delta + pvaddr) & ~(long)(PAGE - 1);
                long segEnd = ((delta + pvaddr + pmemsz) + PAGE - 1) & ~(long)(PAGE - 1);
                int prot = ((pflags & 4) != 0 ? PROT_READ : 0) | ((pflags & 2) != 0 ? PROT_WRITE : 0) | ((pflags & 1) != 0 ? PROT_EXEC : 0);
                int rc = mprotect((IntPtr)segStart, (IntPtr)(segEnd - segStart), prot);
                if (rc != 0 && (pflags & 1) != 0)
                { munmap(map, (IntPtr)span); falha = "mprotect +EXEC denied (errno=" + Marshal.GetLastWin32Error() + ") -> UEP also blocks anonymous exec"; return false; }
            }
            try { syscall_cache(ARM_NR_cacheflush, map, (IntPtr)(map.ToInt64() + span), 0); } catch { }

            // Enderecos em 32 bits SEM sinal. Antes: `(uint)` punha -1 em
            // 4294967295, a guarda `!= -1` nunca pegava, e `(IntPtr)long`
            // acima de int.MaxValue lanca OverflowException num processo de
            // 32 bits — o laco morria ali e as inits seguintes nao rodavam
            // (sigmaboy19, 29/09, "dt-init threw OverflowException").
            // Agora: 0 e 0xFFFFFFFF sao sentinelas, endereco fora do mapa e
            // pulado (com nota), e cada init roda no seu proprio try.
            Etapa("begin dt-init");
            uint mapIni = unchecked((uint)map.ToInt32());
            uint mapFim = unchecked(mapIni + (uint)span);
            int inits = 0, pulados = 0;
            Func<uint, bool> noMapa = x => x >= mapIni && x < mapFim;
            Action<uint, string> roda = (fp, nome) =>
            {
                if (fp == 0 || fp == 0xFFFFFFFFu) return;
                if (!noMapa(fp)) { pulados++; Etapa("note dt-init " + nome + " fora da lib 0x" + fp.ToString("x8")); return; }
                try { Marshal.GetDelegateForFunctionPointer<InitFn>(new IntPtr(unchecked((int)fp)))(); inits++; }
                catch (Exception ex) { /* init pode chamar GL antes do contexto; segue */ Etapa("note dt-init " + nome + " threw " + ex.GetType().Name + ": " + ex.Message); }
            };
            if (initFn != 0) roda(unchecked((uint)initFn), "DT_INIT");
            for (long a = 0; a < initArraySz; a += 4)
                roda(unchecked((uint)Marshal.ReadInt32(new IntPtr(unchecked((int)(uint)(initArray + a))))), "init_array[" + (a / 4) + "]");
            Etapa("ok dt-init " + inits + " rodaram, " + pulados + " fora da lib");

            // Liga NvLib.* pelo dynsym local. Guarda o mapa vivo antes, para o
            // Libera nao rodar se um simbolo faltar (mantemos evidencia).
            elfMap = map; elfSpan = span;
            if (!LigaSimbolos(s => (IntPtr)ResolveLocal(s, symtab, strtab, hash, delta), out string qual))
            {
                falha = "loaded, but symbol missing from dynsym: " + qual;
                return false;
            }
            return true;
        }

        // Endereco virtual -> offset de arquivo, pelo PT_LOAD que o contem.
        static bool VaEmArquivo(byte[] e, uint phoff, ushort phentsize, ushort phnum, uint va, out int off)
        {
            off = 0;
            for (int i = 0; i < phnum; i++)
            {
                int p = (int)(phoff + (uint)i * phentsize);
                if (U32(e, p) != PT_LOAD) continue;
                uint poff = U32(e, p + 4), pvaddr = U32(e, p + 8), pfilesz = U32(e, p + 16);
                if (va >= pvaddr && va < pvaddr + pfilesz) { off = (int)(poff + (va - pvaddr)); return off >= 0 && off < e.Length; }
            }
            return false;
        }

        // Conta os tipos de relocacao de uma tabela (ainda no arquivo).
        static bool ContaRelocs(byte[] e, uint phoff, ushort phentsize, ushort phnum, uint tabVa, uint tamBytes, uint ent,
                                SortedDictionary<int, int> tipos, out string falha)
        {
            falha = null;
            if (tabVa == 0 || tamBytes == 0) return true;
            if (!VaEmArquivo(e, phoff, phentsize, phnum, tabVa, out int off)) { falha = "relocation table outside any PT_LOAD"; return false; }
            if (off + (long)tamBytes > e.Length) { falha = "relocation table runs past end of file"; return false; }
            for (uint o = 0; o + 8 <= tamBytes; o += ent)
            {
                uint rInfo = U32(e, off + (int)o + 4);
                int type = (int)(rInfo & 0xff);
                tipos[type] = tipos.ContainsKey(type) ? tipos[type] + 1 : 1;
            }
            return true;
        }

        // Aponta os 5 delegates de NvLib para o codigo nativo. resolve(nome)
        // devolve o endereco de cada simbolo (dlsym na rota 1, dynsym na rota 2).
        static bool LigaSimbolos(Func<string, IntPtr> resolve, out string qualFaltou)
        {
            qualFaltou = null;
            IntPtr pIni = resolve("nv_tpk_iniciar");
            IntPtr pCfg = resolve("nv_tpk_config");
            IntPtr pQd = resolve("nv_tpk_quadro");
            IntPtr pErr = resolve("nv_tpk_erro");
            IntPtr pTec = resolve("nv_tpk_tecla");
            if (pIni == IntPtr.Zero) { qualFaltou = "nv_tpk_iniciar"; return false; }
            if (pCfg == IntPtr.Zero) { qualFaltou = "nv_tpk_config"; return false; }
            if (pQd == IntPtr.Zero) { qualFaltou = "nv_tpk_quadro"; return false; }
            if (pErr == IntPtr.Zero) { qualFaltou = "nv_tpk_erro"; return false; }
            if (pTec == IntPtr.Zero) { qualFaltou = "nv_tpk_tecla"; return false; }
            NvLib.Iniciar = Marshal.GetDelegateForFunctionPointer<NvLib.IniciarDel>(pIni);
            NvLib.Config = Marshal.GetDelegateForFunctionPointer<NvLib.ConfigDel>(pCfg);
            NvLib.Quadro = Marshal.GetDelegateForFunctionPointer<NvLib.QuadroDel>(pQd);
            NvLib.Erro = Marshal.GetDelegateForFunctionPointer<NvLib.ErroDel>(pErr);
            NvLib.Tecla = Marshal.GetDelegateForFunctionPointer<NvLib.TeclaDel>(pTec);
            // Os pontos de entrada de video/log (chamados de Video.cs) tambem
            // passam a apontar para o mesmo codigo nativo. Na rota memfd isto e
            // redundante (a lib esta no link map e o DllImport ja resolveria),
            // mas mantem UM caminho de chamada nas duas rotas; na rota ELF e
            // obrigatorio, pois o DllImport-por-soname nao resolveria.
            NvVid.Ligar(resolve);
            Apps.Ligar(resolve);
            return NvLib.Pronto;
        }

        // Aplica uma tabela de relocacoes. Os tipos ja foram conferidos antes do
        // mmap (ContaRelocs); aqui um tipo estranho ou um simbolo nao-fraco sem
        // endereco ainda e recusa, nao silencio.
        static bool RelocaBloco(long tabela, long tamBytes, long ent, long symtab, long strtab, long delta, out string falha)
        {
            falha = null;
            if (tabela == 0 || tamBytes == 0) return true;
            if (ent < 8) ent = 8;
            for (long o = 0; o < tamBytes; o += ent)
            {
                long r = tabela + o;
                uint rOffset = (uint)Marshal.ReadInt32((IntPtr)r);
                uint rInfo = (uint)Marshal.ReadInt32((IntPtr)(r + 4));
                int type = (int)(rInfo & 0xff);
                int symidx = (int)(rInfo >> 8);
                IntPtr where = (IntPtr)(delta + rOffset);
                switch (type)
                {
                    case R_ARM_NONE:
                        break;
                    case R_ARM_RELATIVE: // *where += B
                        Marshal.WriteInt32(where, (int)((uint)Marshal.ReadInt32(where) + (uint)delta));
                        break;
                    case R_ARM_ABS32:    // S + A
                    case R_ARM_GLOB_DAT: // S (+A)
                    {
                        if (!ResolveSimbolo(symidx, symtab, strtab, delta, out long s, out falha)) return false;
                        uint a = (uint)Marshal.ReadInt32(where);
                        Marshal.WriteInt32(where, (int)((uint)s + a));
                        break;
                    }
                    case R_ARM_JUMP_SLOT: // S
                    {
                        if (!ResolveSimbolo(symidx, symtab, strtab, delta, out long s, out falha)) return false;
                        Marshal.WriteInt32(where, (int)(uint)s);
                        break;
                    }
                    default:
                        falha = "unsupported relocation type " + type + " at offset 0x" + rOffset.ToString("x");
                        return false;
                }
            }
            return true;
        }

        // Endereco de um simbolo: definido aqui = base + valor; indefinido =
        // dlsym(RTLD_DEFAULT). Indefinido NAO-FRACO sem endereco e recusa.
        static bool ResolveSimbolo(int symidx, long symtab, long strtab, long delta, out long addr, out string falha)
        {
            falha = null;
            long sym = symtab + (long)symidx * 16;
            uint stName = (uint)Marshal.ReadInt32((IntPtr)sym);
            uint stValue = (uint)Marshal.ReadInt32((IntPtr)(sym + 4));
            byte stInfo = Marshal.ReadByte((IntPtr)(sym + 12));
            ushort stShndx = (ushort)Marshal.ReadInt16((IntPtr)(sym + 14));
            if (stShndx != 0) { addr = delta + stValue; return true; }   // definido aqui
            string nome = LeCStr(strtab + stName);
            if (string.IsNullOrEmpty(nome)) { addr = 0; return true; }
            IntPtr p = dlsym(RTLD_DEFAULT, nome);
            addr = p.ToInt64();
            if (addr == 0 && (stInfo >> 4) != STB_WEAK) { falha = "undefined symbol not found: " + nome; return false; }
            return true;                                 // 0 so para fraco
        }

        // Simbolo definido nesta .so, pelo dynsym (nchain do DT_HASH = tamanho).
        static long ResolveLocal(string nome, long symtab, long strtab, long hash, long delta)
        {
            int count = Marshal.ReadInt32((IntPtr)(hash + 4)); // nchain
            if (count <= 0 || count > 1000000) return 0;
            for (int i = 1; i < count; i++)
            {
                long sym = symtab + (long)i * 16;
                uint stName = (uint)Marshal.ReadInt32((IntPtr)sym);
                uint stValue = (uint)Marshal.ReadInt32((IntPtr)(sym + 4));
                ushort stShndx = (ushort)Marshal.ReadInt16((IntPtr)(sym + 14));
                if (stShndx == 0 || stValue == 0) continue;
                if (LeCStr(strtab + stName) == nome) return delta + stValue;
            }
            return 0;
        }

        static string LeCStr(long addr)
        {
            var sb = new System.Text.StringBuilder();
            for (int i = 0; i < 256; i++)
            {
                byte b = Marshal.ReadByte((IntPtr)(addr + i));
                if (b == 0) break;
                sb.Append((char)b);
            }
            return sb.ToString();
        }

        static string LeCStr(byte[] e, int off)
        {
            var sb = new System.Text.StringBuilder();
            for (int i = 0; i < 256 && off + i < e.Length; i++)
            {
                byte b = e[off + i];
                if (b == 0) break;
                sb.Append((char)b);
            }
            return sb.ToString();
        }

        static uint U32(byte[] b, int o) => (uint)(b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24));
        static ushort U16(byte[] b, int o) => (ushort)(b[o] | (b[o + 1] << 8));

        static string Err()
        {
            IntPtr e = dlerror();
            string s = e == IntPtr.Zero ? null : Marshal.PtrToStringAnsi(e);
            return string.IsNullOrEmpty(s) ? "recusado sem mensagem" : s;
        }

        static string Montagem(string raiz)
        {
            try
            {
                string melhor = "", linha = "?";
                foreach (var l in File.ReadAllLines("/proc/self/mounts"))
                {
                    var p = l.Split(' ');
                    if (p.Length > 3 && raiz.StartsWith(p[1]) && p[1].Length > melhor.Length) { melhor = p[1]; linha = p[1] + " " + p[3]; }
                }
                return linha;
            }
            catch (Exception e) { return "? (" + e.Message + ")"; }
        }

        // Tela de erro numa janela ElmSharp por cima de tudo: o motivo, a TV e
        // o pedido de foto.
        void Erro(string titulo, params string[] detalhes)
        {
            rodando = false;
            FecharAviso();
            janelaErro = new Window("NuvioErro") { Geometry = new Rect(0, 0, W, H) };
            janelaErro.BackButtonPressed += (s, e) => Exit();
            var fundo = new Background(janelaErro) { Color = Color.Black };
            fundo.Show();
            janelaErro.AddResizeObject(fundo);
            var coluna = new Box(janelaErro) { AlignmentX = -1, AlignmentY = 0, WeightX = 1, WeightY = 1 };
            coluna.Show();
            janelaErro.AddResizeObject(coluna);
            Information.TryGetValue<string>("http://tizen.org/feature/platform.version", out string versao);
            Information.TryGetValue<string>("http://tizen.org/system/model_name", out string modelo);
            Linha(janelaErro, coluna, titulo, 36);
            foreach (var d in detalhes) Linha(janelaErro, coluna, d, 26);
            Linha(janelaErro, coluna, $"TV {modelo ?? "?"} / Tizen {versao ?? "?"} / {RuntimeInformation.FrameworkDescription}", 26);
            Linha(janelaErro, coluna, "Mande uma FOTO desta tela na issue #137 do GitHub (iqui27/nuvio-native-legacy). Voltar sai.", 26);
            janelaErro.Show();
        }

        // Aviso NAO fatal do arranque anterior (#180): uma faixa preta no topo,
        // por cima da janela GL, por 30 s ou ate a primeira tecla. O app segue
        // rodando por baixo; a ideia e dar tempo de fotografar.
        void Aviso(string texto)
        {
            try
            {
                Etapa("note showing notice: " + texto);
                janelaAviso = new Window("NuvioAviso") { Geometry = new Rect(0, 0, W, 200) };
                var fundo = new Background(janelaAviso) { Color = Color.Black };
                fundo.Show();
                janelaAviso.AddResizeObject(fundo);
                var coluna = new Box(janelaAviso) { AlignmentX = -1, AlignmentY = 0, WeightX = 1, WeightY = 1 };
                coluna.Show();
                janelaAviso.AddResizeObject(coluna);
                Linha(janelaAviso, coluna, texto, 34);
                Linha(janelaAviso, coluna, "Please photograph this and attach it to GitHub issue #180 (iqui27/nuvio-native-legacy). Press any key to hide.", 24);
                janelaAviso.Show();
                EcoreMainloop.AddTimer(30.0, () => { FecharAviso(); return false; });
            }
            catch (Exception e) { Etapa("note notice failed " + e.GetType().Name + ": " + e.Message); janelaAviso = null; }
        }

        void FecharAviso()
        {
            var j = janelaAviso;
            janelaAviso = null;
            if (j == null) return;
            try { j.Hide(); j.Unrealize(); } catch { }
        }

        void Linha(Window janela, Box coluna, string texto, int tam)
        {
            var l = new Label(janela)
            {
                Text = $"<font_size={tam} color=#FFFFFF>" + texto.Replace("&", "&amp;").Replace("<", "&lt;").Replace(">", "&gt;") + "</font>",
                AlignmentX = -1, WeightX = 1, LineWrapType = WrapType.Word
            };
            l.Show();
            coluna.PackEnd(l);
        }

        protected override void OnTerminate()
        {
            Etapa("note terminate");
            video?.Parar();
            base.OnTerminate();
        }

        static void Main(string[] args)
        {
            new Program().Run(args);
        }
    }
}
