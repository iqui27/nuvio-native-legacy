// Player do Nuvio .tpk, comum aos dois hosts (NUI no Tizen 6+, ElmSharp no
// 4/5). Tizen.Multimedia.Player no plano de video da TV; a janela GL fica por
// cima, translucida, e o C abre o furo onde o video aparece. O C pede pelas
// funcoes registradas em nv_tpk_video_registrar e ouve pelo
// nv_tpk_video_evento (src/video_tpk.c).
//
// Toda chamada ao player passa pelo fio principal (Principal), porque o C
// chama do fio dele. A posicao e lida pelo relogio do host (Tique) e o C so le
// o numero guardado, sem esperar ninguem.
using System;
using System.IO;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Threading;
using Tizen.Multimedia;

namespace NuvioTpk
{
    // Despacho dos pontos de entrada de video/log do libnuvio, comum aos dois
    // hosts. POR PADRAO cada delegate aponta para o [DllImport("libnuvio.so")]
    // correspondente — que resolve pelo soname. E o caminho dos hosts Tizen 6+
    // (NuvioTpk/60/65) e tambem da rota memfd do NuvioTpk40, onde a lib entra no
    // link map do loader: comportamento identico ao codigo anterior.
    //
    // So o NuvioTpk40, quando a lib e carregada pelo carregador de ELF proprio
    // (a lib NAO entra no link map, entao DllImport-por-soname NAO resolveria),
    // chama NvVid.Ligar(resolve) para repontar estes delegates para ponteiros de
    // funcao vindos do dynsym do carregador. Nada disso e acionado nos 6+.
    static class NvVid
    {
        [DllImport("libnuvio.so")] static extern void nv_tpk_video_registrar(IntPtr abrir, IntPtr parar, IntPtr pausar,
                                                                              IntPtr buscar, IntPtr volume, IntPtr janela, IntPtr pos);
        [DllImport("libnuvio.so")] static extern void nv_tpk_video_evento(int tipo, int a, int b);
        [DllImport("libnuvio.so")] static extern void nv_tpk_video_registrar_faixas(IntPtr escolher);
        [DllImport("libnuvio.so")] static extern void nv_tpk_video_faixa(int tipo, int idx, string lingua);
        [DllImport("libnuvio.so")] static extern void nv_tpk_video_faixas_fim(int selAudio, int selLeg);
        [DllImport("libnuvio.so")] static extern void nv_tpk_video_legenda(string texto, int durMs);
        [DllImport("libnuvio.so")] static extern void nv_tpk_log(string linha);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void RegistrarDel(IntPtr abrir, IntPtr parar, IntPtr pausar, IntPtr buscar, IntPtr volume, IntPtr janela, IntPtr pos);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void EventoDel(int tipo, int a, int b);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void RegistrarFaixasDel(IntPtr escolher);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void FaixaDel(int tipo, int idx, string lingua);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void FaixasFimDel(int selAudio, int selLeg);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void LegendaDel(string texto, int durMs);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void LogDel(string linha);

        // Padrao: as thunks de P/Invoke acima (soname). O NuvioTpk40 repontа na
        // rota ELF.
        public static RegistrarDel Registrar = nv_tpk_video_registrar;
        public static EventoDel Evento = nv_tpk_video_evento;
        public static RegistrarFaixasDel RegistrarFaixas = nv_tpk_video_registrar_faixas;
        public static FaixaDel Faixa = nv_tpk_video_faixa;
        public static FaixasFimDel FaixasFim = nv_tpk_video_faixas_fim;
        public static LegendaDel Legenda = nv_tpk_video_legenda;
        public static LogDel LogNativo = nv_tpk_log;

        // Repontа tudo por ponteiro de funcao (rota do carregador ELF do
        // NuvioTpk40). resolve(nome) devolve o endereco do simbolo no dynsym.
        public static void Ligar(Func<string, IntPtr> resolve)
        {
            IntPtr p;
            if ((p = resolve("nv_tpk_video_registrar")) != IntPtr.Zero) Registrar = Marshal.GetDelegateForFunctionPointer<RegistrarDel>(p);
            if ((p = resolve("nv_tpk_video_evento")) != IntPtr.Zero) Evento = Marshal.GetDelegateForFunctionPointer<EventoDel>(p);
            if ((p = resolve("nv_tpk_video_registrar_faixas")) != IntPtr.Zero) RegistrarFaixas = Marshal.GetDelegateForFunctionPointer<RegistrarFaixasDel>(p);
            if ((p = resolve("nv_tpk_video_faixa")) != IntPtr.Zero) Faixa = Marshal.GetDelegateForFunctionPointer<FaixaDel>(p);
            if ((p = resolve("nv_tpk_video_faixas_fim")) != IntPtr.Zero) FaixasFim = Marshal.GetDelegateForFunctionPointer<FaixasFimDel>(p);
            if ((p = resolve("nv_tpk_video_legenda")) != IntPtr.Zero) Legenda = Marshal.GetDelegateForFunctionPointer<LegendaDel>(p);
            if ((p = resolve("nv_tpk_log")) != IntPtr.Zero) LogNativo = Marshal.GetDelegateForFunctionPointer<LogDel>(p);
        }
    }

    class Video
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FnAbrir(IntPtr url, IntPtr cabecalhos);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FnSemArg();
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FnInt(int v);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FnRet(int x, int y, int w, int h);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int FnPos();
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FnEscolher(int tipo, int idx);

        const int EV_PRONTO = 1, EV_TOCANDO = 2, EV_PAUSADO = 3, EV_FIM = 4, EV_ERRO = 5, EV_TAMANHO = 6, EV_BUFFER = 7, EV_VELOCIDADE = 8;

        readonly Func<Display> fazDisplay;
        readonly Action<Action> principal;
        readonly string logArq;
        readonly int telaW, telaH;
        // Referencias vivas: o C guarda os ponteiros, o GC nao pode recolher.
        FnAbrir fAbrir; FnSemArg fParar; FnInt fPausar, fBuscar, fVolume; FnRet fJanela; FnPos fPos; FnEscolher fEscolher;

        readonly VideoWindowMetrics windowMetrics = new VideoWindowMetrics();
        Player player;
        int sessao, playerSessao;
        volatile int posMs;

        public Video(Func<Display> fazDisplay, Action<Action> principal, string dados, int telaW, int telaH)
        {
            this.fazDisplay = fazDisplay;
            this.principal = principal;
            this.telaW = telaW;
            this.telaH = telaH;
            logArq = Path.Combine(dados, "tpk-host.log");
            fAbrir = (u, c) => { string url = Marshal.PtrToStringAnsi(u), cab = Marshal.PtrToStringAnsi(c); Trocar(minha => Abrir(url, cab, minha)); };
            fParar = Parar;
            fPausar = p => ComSessao(() => Pausar(p != 0));
            fBuscar = ms => ComSessao(() => Buscar(ms));
            fVolume = v => ComSessao(() => PedirVolume(v));
            fJanela = (x, y, w, h) => ComSessao(() => Janela(x, y, w, h));
            fPos = () => posMs;
            NvVid.Registrar(Marshal.GetFunctionPointerForDelegate(fAbrir), Marshal.GetFunctionPointerForDelegate(fParar),
                                   Marshal.GetFunctionPointerForDelegate(fPausar), Marshal.GetFunctionPointerForDelegate(fBuscar),
                                   Marshal.GetFunctionPointerForDelegate(fVolume), Marshal.GetFunctionPointerForDelegate(fJanela),
                                   Marshal.GetFunctionPointerForDelegate(fPos));
            fEscolher = (tipo, idx) => ComSessao(() => Escolher(tipo, idx));
            NvVid.RegistrarFaixas(Marshal.GetFunctionPointerForDelegate(fEscolher));
        }

        // 0 = audio, 1 = legenda embutida, 2 = atraso da legenda (ms),
        // 3 = velocidade em centesimos (#202).
        void Escolher(int tipo, int idx)
        {
            if (player == null) return;
            if (tipo == 3) { Velocidade(idx); return; }
            // Diagnostics: the player's REAL state at the moment of the write.
            // Tizen.Multimedia track selection also supports Ready and Paused.
            // The native delay is a measured firmware workaround: an early
            // accepted write did not change the demuxed subtitle on the test TV.
            // This is not the web AVPlay state contract.
            string estado = "?";
            try { estado = player.State.ToString(); } catch { }
            Log("select track " + tipo + "/" + idx + " state=" + estado);
            try
            {
                if (tipo == 0) player.AudioTrackInfo.Selected = idx;
                else if (tipo == 1) player.SubtitleTrackInfo.Selected = idx;
                else player.SetSubtitleOffset(idx);
            }
            catch (Exception e) { if (tipo != 2) Log("escolher " + tipo + "/" + idx + ": " + e.Message); }
        }

        // VELOCIDADE (#202). A documentacao diz que SetPlaybackRate lanca
        // InvalidOperationException em streaming e NotAvailableException com
        // audio offload; a resposta vai ao C (EV_VELOCIDADE, b = 1 aceitou), que
        // esconde a linha se a TV recusar. Nao provado em TV.
        void Velocidade(int centesimos)
        {
            float v = Math.Max(25, Math.Min(400, centesimos)) / 100f;
            try
            {
                player.SetPlaybackRate(v);
                Log("velocidade " + v.ToString(System.Globalization.CultureInfo.InvariantCulture) + " ok");
                NvVid.Evento(EV_VELOCIDADE, centesimos, 1);
            }
            catch (Exception e)
            {
                Log("velocidade " + v.ToString(System.Globalization.CultureInfo.InvariantCulture) + ": " + e.GetType().Name + " " + e.Message);
                NvVid.Evento(EV_VELOCIDADE, centesimos, 0);
            }
        }

        // Lista de faixas para o C, logo depois do prepare. Devolve quantas
        // faixas de audio o player listou.
        int Faixas(Player p, bool soAudio = false)
        {
            int selA = -1, selL = -1, nA = 0;
            try
            {
                var a = p.AudioTrackInfo;
                nA = a.GetCount();
                for (int i = 0; i < nA; i++) NvVid.Faixa(0, i, Lingua(() => a.GetLanguageCode(i)));
                try { selA = a.Selected; } catch { }
            }
            catch (Exception e) { Log("faixas de audio: " + e.Message); }
            // #165: legenda listada e audio nao. Se o video tem som, a faixa que
            // toca aparece como unica, em vez de "nenhuma faixa".
            if (nA == 0)
            {
                try
                {
                    var ap = p.StreamInfo.GetAudioProperties();
                    if (ap.Channels > 0) { NvVid.Faixa(0, 0, ""); selA = 0; Log($"audio sem lista do player: {ap.Channels} canais, {ap.SampleRate} Hz"); }
                }
                catch (Exception e) { Log("propriedades de audio: " + e.Message); }
            }
            if (soAudio) { NvVid.FaixasFim(selA, -1); return nA; }
            // #269: SubtitleTrackInfo lanca InvalidOperation fora de Ready/Playing/Paused
            // (player solto ou ainda em Idle). Sem estado valido nao ha lista: segue sem legenda.
            PlayerState estL;
            try { estL = p.State; } catch { estL = PlayerState.Idle; }
            if (estL != PlayerState.Ready && estL != PlayerState.Playing && estL != PlayerState.Paused)
            {
                Log("faixas de legenda: player em " + estL + ", lista pulada");
                NvVid.FaixasFim(selA, -1);
                return nA;
            }
            try
            {
                var l = p.SubtitleTrackInfo;
                int n = l.GetCount();
                for (int i = 0; i < n; i++) NvVid.Faixa(1, i, Lingua(() => l.GetLanguageCode(i)));
                try { selL = l.Selected; } catch { }
            }
            catch (Exception e) { Log("faixas de legenda: " + e.Message); }
            Log($"faixas do player: {nA} audio (sel={selA}), legenda sel={selL}");
            NvVid.FaixasFim(selA, selL);
            return nA;
        }

        static string Lingua(Func<string> f)
        {
            try { return f() ?? ""; } catch { return ""; }
        }

        // App foi para segundo plano: o player pausa (e o C fica sabendo).
        public void PausarPeloSistema() { SoltaPrimer(); Pausar(true); }

        // VOLUME DO TRAILER (#281: S90C, Tizen 9, trailer sem som com o ajuste
        // de som ligado). So o trailer pede volume (trailer.c); o filme nunca
        // pede e segue intocado. O pedido chega logo depois do Abrir, com o
        // Player ainda em Idle/preparando, e era o UNICO momento em que o
        // Volume era escrito. NAO PROVADO que o Tizen 9 descarta o volume
        // escrito antes do prepare, mas nada mais difere do filme (que toca
        // com som). Entao o alvo fica guardado por sessao e e reaplicado depois
        // do prepare, depois do Start e 1,5 s depois, com Muted=false explicito
        // e o valor LIDO DE VOLTA no registro.
        float volAlvo = 1f;
        bool volPedido;

        void PedirVolume(int v)
        {
            volAlvo = Math.Max(0, Math.Min(100, v)) / 100f;
            volPedido = true;
            if (player != null) AplicaVolume(player, "pedido");
        }

        void AplicaVolume(Player p, string quando)
        {
            if (!volPedido || p == null) return;
            string estado = "?", lido = "?", erro = "";
            try { estado = p.State.ToString(); } catch { }
            // Cada escrita no seu try: uma recusa do Muted nao pode pular o Volume.
            try { p.Muted = volAlvo <= 0f; } catch (Exception e) { erro += " muted:" + e.GetType().Name; }
            try { p.Volume = volAlvo; } catch (Exception e) { erro += " volume:" + e.GetType().Name + " " + e.Message; }
            try { lido = p.Volume.ToString("0.00", CultureInfo.InvariantCulture) + " muted=" + p.Muted; }
            catch (Exception e) { lido = "? (" + e.GetType().Name + ")"; }
            if (erro.Length > 0) lido += " falhou" + erro;
            Log("[audio] volume alvo=" + volAlvo.ToString("0.00", CultureInfo.InvariantCulture) +
                " lido=" + lido + " estado=" + estado + " (" + quando + ")");
        }

        // Relogio do host, no fio principal.
        public void Tique()
        {
            // Uma consulta por tique no principal; fPos so le o cache.
            long inicio = Stopwatch.GetTimestamp();
            try { if (player != null && playerSessao == Volatile.Read(ref sessao) && player.State == PlayerState.Playing) posMs = player.GetPlayPosition(); } catch { }
            long ms = (Stopwatch.GetTimestamp() - inicio) * 1000 / Stopwatch.Frequency;
            if (ms >= 50) Log("[video] tpk consulta posicao levou " + ms + " ms (fio principal)");
        }

        public void Log(string s)
        {
            try { NvVid.LogNativo(s); } catch { }
            try { File.AppendAllText(logArq, DateTime.Now.ToString("HH:mm:ss ") + s + "\n"); } catch { }
        }

        void Principal(Action a)
        {
            principal(() => { try { a(); } catch (Exception e) { Log("principal: " + e); } });
        }

        // Invalida tambem comandos que ficaram na fila durante uma troca.
        void ComSessao(Action a)
        {
            int minha = Volatile.Read(ref sessao);
            Principal(() => { if (minha == Volatile.Read(ref sessao) && playerSessao == minha && !paradaFalhou) a(); });
        }

        [DllImport("libc.so.6", EntryPoint = "_exit")] static extern void SairImediatamente(int codigo);
        internal int PrazoPararMs = 5000;
        internal Action FalhaFatal = () => { try { SairImediatamente(1); } catch { Environment.Exit(1); } };
        int fatal;
        bool paradaFalhou;
        readonly object trocaTrava = new object();
        System.Threading.Timer prazoParada;
        Stopwatch tempoParada;

        // O prazo nasce no fio solicitante, ANTES do Post: o principal pode
        // estar preso em GetPlayPosition/Stop. Pedidos repetidos nao adiam o
        // limite. A trava so protege o prazo, nunca uma chamada Tizen.
        void Trocar(Action<int> abrir)
        {
            int minha;
            lock (trocaTrava)
            {
                if (fatal != 0) return;
                minha = Interlocked.Increment(ref sessao);
                if (prazoParada == null)
                {
                    var tempo = tempoParada = Stopwatch.StartNew();
                    prazoParada = new System.Threading.Timer(_ =>
                    {
                        lock (trocaTrava)
                        {
                            if (prazoParada == null || tempoParada != tempo || fatal != 0) return;
                            prazoParada.Dispose(); prazoParada = null;
                            // Fila atrasada/flags nao provam recurso retido. As
                            // referencias so somem quando Dispose retorna.
                            if (Volatile.Read(ref player) == null && Volatile.Read(ref primer) == null) return;
                            Volatile.Write(ref fatal, 1);
                        }
                        Log("[video] tpk parar NAO confirmou em " + tempo.ElapsedMilliseconds + " ms; encerrando processo");
                        FalhaFatal();
                    }, null, PrazoPararMs, Timeout.Infinite);
                }
            }
            Principal(() =>
            {
                if (minha != Volatile.Read(ref sessao) || Volatile.Read(ref fatal) != 0) return;
                if (paradaFalhou || !PararAtual()) { paradaFalhou = true; return; }
                long ms;
                lock (trocaTrava)
                {
                    if (minha != sessao || fatal != 0) return;
                    ms = tempoParada.ElapsedMilliseconds;
                    prazoParada?.Dispose(); prazoParada = null;
                }
                Log("[video] tpk parar confirmado em " + ms + " ms");
                if (abrir != null && minha == Volatile.Read(ref sessao)) abrir(minha);
            });
        }

        async void Abrir(string url, string cabecalhos, int minha)
        {
            posMs = 0;
            // Player novo nasce no volume cheio; so um pedido DESTA sessao
            // (trailer.c, logo depois do video_tocar) o muda.
            volAlvo = 1f; volPedido = false;
            try
            {
                var p = new Player();
                Volatile.Write(ref player, p); playerSessao = minha;
                p.PlaybackCompleted += (s, e) => { if (minha == Volatile.Read(ref sessao)) NvVid.Evento(EV_FIM, 0, 0); };
                p.ErrorOccurred += (s, e) => { if (minha == Volatile.Read(ref sessao)) { Log("erro " + e.Error); NvVid.Evento(EV_ERRO, (int)e.Error, 0); } };
                p.BufferingProgressChanged += (s, e) => { if (minha == Volatile.Read(ref sessao)) NvVid.Evento(EV_BUFFER, e.Percent, 0); };
                p.PlaybackInterrupted += (s, e) => { if (minha == Volatile.Read(ref sessao)) { Log("interrompido: " + e.Reason); NvVid.Evento(EV_PAUSADO, 0, 0); } };
                p.SubtitleUpdated += (s, e) => { if (minha == Volatile.Read(ref sessao)) NvVid.Legenda(e.Text ?? "", (int)e.Duration); };
                foreach (var linha in (cabecalhos ?? "").Split('\n'))
                {
                    int i = linha.IndexOf(':');
                    if (i <= 0) continue;
                    string nome = linha.Substring(0, i).Trim(), valor = linha.Substring(i + 1).Trim();
                    if (nome.Equals("User-Agent", StringComparison.OrdinalIgnoreCase)) p.UserAgent = valor;
                    else if (nome.Equals("Cookie", StringComparison.OrdinalIgnoreCase)) p.Cookie = valor;
                    else Log("cabecalho ignorado pelo player: " + nome);
                }
                p.SetSource(new MediaUriSource(url));
                p.Display = fazDisplay();
                p.DisplaySettings.Mode = PlayerDisplayMode.LetterBox;
                await p.PrepareAsync();
                // A parada e dona unica do descarte, inclusive se o Prepare
                // terminar depois do pedido de saida ou de outra abertura.
                if (minha != Volatile.Read(ref sessao)) return;
                int dur = 0;
                try { dur = p.StreamInfo.GetDuration(); } catch { }
                try { var v = p.StreamInfo.GetVideoProperties(); NvVid.Evento(EV_TAMANHO, v.Size.Width, v.Size.Height); } catch { }
                int nAudio = Faixas(p);
                AplicaVolume(p, "preparado");
                NvVid.Evento(EV_PRONTO, dur, 0);
                if (minha != Volatile.Read(ref sessao)) return;
                p.Start();
                windowMetrics.Invalidate();
                AplicaVolume(p, "start");
                NvVid.Evento(EV_TOCANDO, 0, 0);
                if (volPedido)
                {
                    await System.Threading.Tasks.Task.Delay(1500);
                    if (minha == Volatile.Read(ref sessao) && player == p) AplicaVolume(p, "tocando 1,5 s");
                }
                // Alguns contêineres/HLS so publicam as faixas de audio depois
                // que a reproducao comeca: le de novo, uma vez.
                if (nAudio == 0)
                {
                    await System.Threading.Tasks.Task.Delay(2000);
                    if (minha == Volatile.Read(ref sessao) && player == p) Faixas(p, true);
                }
            }
            catch (Exception e)
            {
                if (minha != Volatile.Read(ref sessao)) { Log("abrir: sessao antiga encerrada (" + e.GetType().Name + ")"); return; }
                Log("abrir: " + e);
                NvVid.Evento(EV_ERRO, -1, 0);
            }
        }

        // CANARIO (#137, Samsung TV Plus tocando por baixo do Nuvio). O relato:
        // o som do canal so para quando um filme comeca, isto e, quando um
        // Player deste arquivo prepara e toca. A aposta (NAO provada) e que e o
        // gerenciador de recursos da TV que tira o decodificador/saida de audio
        // do TV Plus nesse momento. Entao, logo que a janela sobe, o host 6+
        // toca pelo MESMO caminho do filme (Player + Display da janela NUI) um
        // clipe de 2 s preto e mudo (res/silencio.mp4, H.264 Main + AAC-LC) por
        // ~1 s e solta tudo (Stop/Unprepare/Dispose), como ao fim de um filme.
        // Nada fica preso: o filme de verdade e a saida seguem como hoje. So o
        // host 6+ (Program.cs) chama isto; no 4/5 `primer` e sempre null e os
        // SoltaPrimer() de Parar/PausarPeloSistema nao fazem nada.
        Player primer;
        int primerGen;

        public async void PrimeAudio(string arquivo)
        {
            int minha = ++primerGen;
            Log("[audio] prime begin " + Path.GetFileName(arquivo));
            try
            {
                if (!File.Exists(arquivo)) { Log("[audio] prime fail sem arquivo " + arquivo); return; }
                if (player != null) { Log("[audio] prime skip: player do app ja aberto"); return; }
                var p = new Player();
                Volatile.Write(ref primer, p);
                p.ErrorOccurred += (s, e) => Log("[audio] prime erro do player " + e.Error);
                p.PlaybackInterrupted += (s, e) => Log("[audio] prime interrompido " + e.Reason);
                p.SetSource(new MediaUriSource(arquivo));
                p.Display = fazDisplay();
                p.DisplaySettings.Mode = PlayerDisplayMode.LetterBox;
                var prep = p.PrepareAsync();
                // Excecao de um prepare abandonado (timeout) nao pode sobrar solta.
                _ = prep.ContinueWith(t => { var _e = t.Exception; }, System.Threading.Tasks.TaskContinuationOptions.OnlyOnFaulted);
                if (await System.Threading.Tasks.Task.WhenAny(prep, System.Threading.Tasks.Task.Delay(8000)) != prep)
                {
                    Log("[audio] prime fail prepare demorou mais de 8 s");
                    return;
                }
                await prep;
                if (minha != primerGen) { Log("[audio] prime cancelado (filme/saida antes de preparar)"); return; }
                Log("[audio] prime prepared");
                p.Start();
                Log("[audio] prime started");
                await System.Threading.Tasks.Task.Delay(1200);
                if (minha != primerGen) { Log("[audio] prime cancelado (filme/saida durante o clipe)"); return; }
                Log("[audio] prime ok");
            }
            catch (Exception e) { Log("[audio] prime fail " + e.GetType().Name + ": " + e.Message); }
            finally { if (minha == primerGen) SoltaPrimer(); }
        }

        bool SoltaPrimer()
        {
            primerGen++;
            if (primer == null) return true;
            if (!Liberar(ref primer)) return false;
            Log("[audio] prime released");
            return true;
        }

        void ReportWindowMetrics()
        {
            if (windowMetrics.Requests > 0)
            {
                double scale = 1000.0 / Stopwatch.Frequency;
                Log(string.Format(CultureInfo.InvariantCulture,
                    "[video-window] requests={0} repeated={1} applied={2} failed={3} native_ms={4:F3} max_ms={5:F3}",
                    windowMetrics.Requests, windowMetrics.RepeatedRequests, windowMetrics.Applied,
                    windowMetrics.Failed, windowMetrics.ElapsedTicks * scale, windowMetrics.MaxTicks * scale));
            }
            windowMetrics.Reset();
        }

        public void Parar() { Trocar(null); }
        public void Encerrar() { Parar(); }

        // Dispose confirma a liberacao, independentemente do estado anterior.
        // Preparing nao admite Unprepare; cada passo ainda tenta o Dispose.
        bool Liberar(ref Player p)
        {
            try { p.Muted = true; } catch (Exception e) { Log("parar muted: " + e.Message); }
            try { p.Display = null; } catch (Exception e) { Log("parar display: " + e.Message); }
            try { if (p.State == PlayerState.Playing || p.State == PlayerState.Paused) p.Stop(); }
            catch (Exception e) { Log("parar stop: " + e.Message); }
            try { if (p.State == PlayerState.Ready || p.State == PlayerState.Playing || p.State == PlayerState.Paused) p.Unprepare(); }
            catch (Exception e) { Log("parar unprepare: " + e.Message); }
            try { p.Dispose(); Volatile.Write(ref p, null); return true; }
            catch (Exception e) { Log("parar dispose: " + e.Message); return false; }
        }

        bool PararAtual()
        {
            ReportWindowMetrics();
            if (!SoltaPrimer()) return false;
            if (player == null) return true;
            return Liberar(ref player); // cache preserva a retomada durante a espera da reconexao
        }

        void Pausar(bool pausa)
        {
            if (player == null) return;
            try
            {
                if (pausa && player.State == PlayerState.Playing) { player.Pause(); NvVid.Evento(EV_PAUSADO, 0, 0); }
                else if (!pausa && player.State == PlayerState.Paused) { player.Start(); windowMetrics.Invalidate(); NvVid.Evento(EV_TOCANDO, 0, 0); }
            }
            catch (InvalidOperationException e)
            {
                // #269: o estado mudou entre a checagem e o Pause/Start. Estado ja e outro; avisa uma vez.
                if (!pausarAvisado) { pausarAvisado = true; Log("pausar: estado mudou (" + e.Message + ")"); }
            }
            catch (Exception e) { Log("pausar: " + e.Message); }
        }
        bool pausarAvisado;

        async void Buscar(int ms)
        {
            if (player == null) return;
            windowMetrics.Invalidate();
            try { posMs = ms; await player.SetPlayPositionAsync(ms, false); }
            catch (Exception e) { Log("buscar: " + e.Message); }
        }

        // Host 4/5 (Program40): move/redimensiona a janela ElmSharp do video.
        // Devolve null se aplicou, ou a mensagem do erro. Fica null no 6+, que
        // continua so com Mode=Roi + SetRoi (comportamento inalterado).
        public Func<int, int, int, int, string> GeometriaJanela;
        bool janelaMovida;

        void Janela(int x, int y, int w, int h)
        {
            if (player == null) return;
            bool applied = false, fullscreen = x == 0 && y == 0 && w == telaW && h == telaH;
            windowMetrics.Begin(player, x, y, w, h, fullscreen);
            long started = Stopwatch.GetTimestamp(), ended = 0;
            try
            {
                // QUADRO CHEIO SEM ZOOM: LetterBox, o mesmo caminho da reproducao
                // normal nos 4 hosts. A reproducao cheia pede exatamente a tela
                // (0,0,telaW,telaH), entao esta guarda continua sendo um no-op
                // para ela — 6+ e o host 4/5 nao mudam.
                //
                // ZOOM (#178): o recorte de fonte emulado (src/video_tpk.c
                // video_janela_fonte) manda um ROI que RECUA a origem para
                // negativo ou ESTOURA a tela, para que a fatia desejada do quadro
                // preencha o destino. Antes, `w >= telaW && h >= telaH` engolia um
                // ROI ampliado ancorado em 0,0 de volta para LetterBox, matando o
                // zoom. Agora so o quadro EXATO da tela vira LetterBox; qualquer
                // ROI de zoom (origem negativa OU maior que a tela) passa cru ao
                // SetRoi.
                if (GeometriaJanela != null) { applied = JanelaTizen45(x, y, w, h, fullscreen); return; }
                if (fullscreen) { player.DisplaySettings.Mode = PlayerDisplayMode.LetterBox; applied = true; return; }
                player.DisplaySettings.Mode = PlayerDisplayMode.Roi;
                player.DisplaySettings.SetRoi(new Rectangle(x, y, w, h));
                applied = true;
            }
            catch (Exception e) { ended = Stopwatch.GetTimestamp(); Log("[video-window] apply failed: " + e.Message); }
            finally { windowMetrics.Complete(applied, (ended != 0 ? ended : Stopwatch.GetTimestamp()) - started); }
        }

        // Tizen 4/5 (#203, botao de aspecto/zoom sem efeito). CAUSA PROVAVEL
        // (nao provada em TV): Mode=Roi + SetRoi sobre um Display de janela
        // ElmSharp e aceito sem excecao mas o plano de video do 4/5 o ignora, e
        // o destino "tela cheia" (Esticar) cai em LetterBox, que nao muda nada.
        // Aqui o retangulo de destino vira a GEOMETRIA da janela do video e o
        // player preenche a janela (FullScreen). Se a janela falhar, tenta o
        // ROI antigo. Cada passo vai ao log para a TV provar qual funcionou.
        bool JanelaTizen45(int x, int y, int w, int h, bool fullscreen)
        {
            string rotulo = x + "," + y + " " + w + "x" + h;
            if (fullscreen)
            {
                string e0 = janelaMovida ? GeometriaJanela(0, 0, telaW, telaH) : null;
                janelaMovida = false;
                player.DisplaySettings.Mode = PlayerDisplayMode.LetterBox;
                Log("[aspect] tpk40 modo=letterbox " + rotulo + (e0 == null ? " ok" : " falhou: " + e0));
                return e0 == null;
            }
            string erro = GeometriaJanela(x, y, w, h);
            if (erro == null)
            {
                try
                {
                    player.DisplaySettings.Mode = PlayerDisplayMode.FullScreen;
                    janelaMovida = true;
                    Log("[aspect] tpk40 modo=janela+fullscreen " + rotulo + " ok");
                    return true;
                }
                catch (Exception e) { erro = "Mode=FullScreen " + e.GetType().Name + ": " + e.Message; }
            }
            Log("[aspect] tpk40 modo=janela " + rotulo + " falhou: " + erro);
            try
            {
                player.DisplaySettings.Mode = PlayerDisplayMode.Roi;
                player.DisplaySettings.SetRoi(new Rectangle(x, y, w, h));
                Log("[aspect] tpk40 modo=roi " + rotulo + " ok");
                return true;
            }
            catch (Exception e) { Log("[aspect] tpk40 modo=roi " + rotulo + " falhou: " + e.GetType().Name + ": " + e.Message); return false; }
        }
    }
}
