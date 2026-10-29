// Player do Nuvio .tpk, comum aos dois hosts (NUI no Tizen 6+, ElmSharp no
// 4/5). Tizen.Multimedia.Player no plano de video da TV; a janela GL fica por
// cima, translucida, e o C abre o furo onde o video aparece. O C pede pelas
// funcoes registradas em nv_tpk_video_registrar e ouve pelo
// nv_tpk_video_evento (src/video_tpk.c).
//
// Chamadas nativas ficam no fio da sessao; janela/Display nascem no principal.
// Tique pede uma amostra sem esperar; o C so le o ultimo numero confirmado.
using System;
using System.IO;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;
using System.Collections.Concurrent;
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
        [DllImport("libnuvio.so")] static extern void nv_tpk_video_registrar_aspecto(IntPtr janela);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void RegistrarAspectoDel(IntPtr janela);
        public static RegistrarAspectoDel RegistrarAspecto = nv_tpk_video_registrar_aspecto;

        [DllImport("libnuvio.so")] static extern void nv_tpk_video_registrar_faixas(IntPtr escolher);
        [DllImport("libnuvio.so")] static extern void nv_tpk_video_faixa(int tipo, int idx, string lingua);
        [DllImport("libnuvio.so")] static extern void nv_tpk_video_faixas_fim(int selAudio, int selLeg);
        [DllImport("libnuvio.so")] static extern void nv_tpk_video_legenda(string texto, int durMs);
        [DllImport("libnuvio.so")] static extern void nv_tpk_log(string linha);
        [DllImport("libnuvio.so")] static extern void nv_tpk_video_posicao(int ms, int motivo, int consultaMs, long carimboNs);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void RegistrarDel(IntPtr abrir, IntPtr parar, IntPtr pausar, IntPtr buscar, IntPtr volume, IntPtr janela, IntPtr pos);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void EventoDel(int tipo, int a, int b);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void RegistrarFaixasDel(IntPtr escolher);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void FaixaDel(int tipo, int idx, string lingua);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void FaixasFimDel(int selAudio, int selLeg);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void LegendaDel(string texto, int durMs);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void LogDel(string linha);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] public delegate void PosicaoDel(int ms, int motivo, int consultaMs, long carimboNs);

        // Padrao: as thunks de P/Invoke acima (soname). O NuvioTpk40 repontа na
        // rota ELF.
        public static RegistrarDel Registrar = nv_tpk_video_registrar;
        public static EventoDel Evento = nv_tpk_video_evento;
        public static RegistrarFaixasDel RegistrarFaixas = nv_tpk_video_registrar_faixas;
        public static FaixaDel Faixa = nv_tpk_video_faixa;
        public static FaixasFimDel FaixasFim = nv_tpk_video_faixas_fim;
        public static LegendaDel Legenda = nv_tpk_video_legenda;
        public static LogDel LogNativo = nv_tpk_log;
        // Subtitle clock (owner 25/10): every real position reading, stamped at
        // the middle of the query. An older libnuvio without it: no-op.
        public static PosicaoDel Posicao = nv_tpk_video_posicao;

        // Repontа tudo por ponteiro de funcao (rota do carregador ELF do
        // NuvioTpk40). resolve(nome) devolve o endereco do simbolo no dynsym.
        public static void Ligar(Func<string, IntPtr> resolve)
        {
            IntPtr p;
            p = resolve("nv_tpk_video_registrar_aspecto");
            RegistrarAspecto = p != IntPtr.Zero ? Marshal.GetDelegateForFunctionPointer<RegistrarAspectoDel>(p) : new RegistrarAspectoDel(_ => { });
            if ((p = resolve("nv_tpk_video_registrar")) != IntPtr.Zero) Registrar = Marshal.GetDelegateForFunctionPointer<RegistrarDel>(p);
            if ((p = resolve("nv_tpk_video_evento")) != IntPtr.Zero) Evento = Marshal.GetDelegateForFunctionPointer<EventoDel>(p);
            if ((p = resolve("nv_tpk_video_registrar_faixas")) != IntPtr.Zero) RegistrarFaixas = Marshal.GetDelegateForFunctionPointer<RegistrarFaixasDel>(p);
            if ((p = resolve("nv_tpk_video_faixa")) != IntPtr.Zero) Faixa = Marshal.GetDelegateForFunctionPointer<FaixaDel>(p);
            if ((p = resolve("nv_tpk_video_faixas_fim")) != IntPtr.Zero) FaixasFim = Marshal.GetDelegateForFunctionPointer<FaixasFimDel>(p);
            if ((p = resolve("nv_tpk_video_legenda")) != IntPtr.Zero) Legenda = Marshal.GetDelegateForFunctionPointer<LegendaDel>(p);
            if ((p = resolve("nv_tpk_log")) != IntPtr.Zero) LogNativo = Marshal.GetDelegateForFunctionPointer<LogDel>(p);
            p = resolve("nv_tpk_video_posicao");
            Posicao = p != IntPtr.Zero ? Marshal.GetDelegateForFunctionPointer<PosicaoDel>(p) : new PosicaoDel((a, b, c, d) => { });
        }
    }

    class Video
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FnAbrir(IntPtr url, IntPtr cabecalhos);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FnSemArg();
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FnInt(int v);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FnRet(int x, int y, int w, int h);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FnAspecto(int x, int y, int w, int h, int lx, int ly, int lw, int lh);
        [DllImport("libcapi-media-player.so.0")] static extern int player_set_display_mode(IntPtr p, PlayerDisplayMode mode);
        [DllImport("libcapi-media-player.so.0")] static extern int player_set_display_roi_area(IntPtr p, int x, int y, int w, int h);
        // DTS conversion (X-Nuvio-Prebuffer from src/video_tpk.c): only in IDLE, before Prepare.
        [DllImport("libcapi-media-player.so.0")] static extern int player_set_streaming_buffering_time(IntPtr p, int preMs, int reMs);
        [DllImport("libcapi-media-player.so.0")] static extern int player_get_streaming_buffering_time(IntPtr p, out int preMs, out int reMs);
        FnAspecto fAspecto;
        bool aspectoRecusado;
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate int FnPos();
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FnEscolher(int tipo, int idx);

        const int EV_PRONTO = 1, EV_TOCANDO = 2, EV_PAUSADO = 3, EV_FIM = 4, EV_ERRO = 5, EV_TAMANHO = 6, EV_BUFFER = 7, EV_VELOCIDADE = 8, EV_PRESO = 9, EV_CONSULTA_LENTA = 10, EV_BUSCOU = 11, EV_BUSCA_FALHOU = 12;

        readonly Func<Display> fazDisplay;
        readonly Action<Action> principal;
        readonly string logArq;
        readonly int telaW, telaH;
        // Referencias vivas: o C guarda os ponteiros, o GC nao pode recolher.
        FnAbrir fAbrir; FnSemArg fParar; FnInt fPausar, fBuscar, fVolume; FnRet fJanela; FnPos fPos; FnEscolher fEscolher;

        VideoWindowMetrics windowMetrics = new VideoWindowMetrics();
        Player player;
        int sessao, playerSessao;
        volatile int posMs;
        int posGer;
        bool acabou;

        // Um contexto por sessao mantem comandos e continuacoes async no mesmo
        // fio. Fechar descarta a fila antiga; nunca espera uma API presa.
        sealed class FioVideo : SynchronizationContext
        {
            readonly BlockingCollection<Action> fila = new BlockingCollection<Action>();
            readonly object trava = new object();
            bool fechado;
            internal int consulta;
            internal FioVideo(Action<string> log)
            {
                new Thread(() => {
                    SetSynchronizationContext(this);
                    foreach (var a in fila.GetConsumingEnumerable())
                        try { a(); } catch (Exception e) { log("fio video: " + e); }
                    fila.Dispose();
                }) { IsBackground = true, Name = "Nuvio video" }.Start();
            }
            public override void Post(SendOrPostCallback d, object state)
            {
                lock (trava) { if (!fechado) fila.Add(() => d(state)); }
            }
            internal void Fechar()
            {
                lock (trava) { fechado = true; while (fila.TryTake(out _)) { } fila.CompleteAdding(); }
            }
        }
        FioVideo fio;
        readonly object trocaTrava = new object();
        Task<bool> liberacao = Task.FromResult(true);
        internal int PrazoPararMs = 5000;
        internal int PrazoBuscarMs = 15000;

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
            fVolume = v => ComSessao(() => PedirVolume(v), false);
            fJanela = (x, y, w, h) => ComSessao(() => Janela(x, y, w, h));
            fAspecto = (x,y,w,h,lx,ly,lw,lh) => ComSessao(() => Janela(x,y,w,h,lx,ly,lw,lh));
            // Older staged libnuvio versions only have the original window callback.
            try { NvVid.RegistrarAspecto(Marshal.GetFunctionPointerForDelegate(fAspecto)); }
            catch (EntryPointNotFoundException) { }
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

        // App foi para segundo plano (tecla Home): PARAR, nao pausar. Com o
        // player preso o Pause nunca chega e o som seguia na Home (#412). O
        // descarte em background solta Muted e o plano sem o fio da sessao; a
        // volta e a reconexao de sempre no relogio parado (watchdog do C).
        public void PausarPeloSistema() { Parar(); }

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

        // Relogio do host: nenhuma API no principal, uma consulta em voo por
        // sessao. Mantem a amostra real (estimar esconderia o watchdog do C).
        public void Tique()
        {
            // The 40 ms clock thread is alive: it is the only reader. Here the
            // flag was taken BEFORE Task.Run, and while the queued task waited for
            // a pool thread the clock thread found the flag busy: log 8 (26/10)
            // had 4 readings a second instead of 25 (subtitles froze up to 250 ms
            // after each resume).
            if (Stopwatch.GetTimestamp() - Volatile.Read(ref relogioVivo) < Stopwatch.Frequency / 2) return;
            var f = Volatile.Read(ref fio);
            if (f == null || Interlocked.Exchange(ref f.consulta, 1) != 0) return;
            int minha = Volatile.Read(ref sessao);
            // Polling must not occupy the command queue: firmware reads can take seconds.
            int ger = Volatile.Read(ref posGer);
            _ = Task.Run(() => LerTique(f, minha, ger));
        }

        // Subtitle clock (owner 25/10): its own thread, every 40 ms. The NUI timer
        // on the main thread fell to 32 readings in 10 s on heavy signs (log 4,
        // 25/10) against ~260 on light ones; the thread pool can starve as well.
        // Same one-query-in-flight flag as Tique and LerPosicao.
        long relogioVivo;   // Stopwatch ticks of the clock thread's last turn
        public void IniciaRelogio(Func<bool> parar)
        {
            new Thread(() => {
                while (!parar()) {
                    Volatile.Write(ref relogioVivo, Stopwatch.GetTimestamp());
                    try {
                        var f = Volatile.Read(ref fio);
                        if (f != null && Interlocked.Exchange(ref f.consulta, 1) == 0)
                            LerTique(f, Volatile.Read(ref sessao), Volatile.Read(ref posGer));
                    } catch { }
                    Thread.Sleep(40);
                }
            }) { IsBackground = true, Name = "Nuvio relogio" }.Start();
        }

        void LerTique(FioVideo f, int minha, int ger)
        {
            var p = Volatile.Read(ref player);
            long inicio = Stopwatch.GetTimestamp();
            try {
                if (p != null && playerSessao == minha && minha == Volatile.Read(ref sessao) && p.State == PlayerState.Playing) {
                    int ms = p.GetPlayPosition();
                    long fim = Stopwatch.GetTimestamp();
                    lock (trocaTrava) {
                        if (minha == sessao && player == p && ger == posGer) { posMs = ms; Informar(ms, 0, inicio, fim); }
                    }
                }
            } catch (InvalidOperationException e) {
                Log("posicao: " + e.Message);
                Preso(minha, -1);
            } catch { }
            finally {
                long ms = (Stopwatch.GetTimestamp() - inicio) * 1000 / Stopwatch.Frequency;
                if (ms >= 50) Log("[video] tpk consulta posicao levou " + ms + " ms (fio video)");
                if (ms >= 5000 && minha == Volatile.Read(ref sessao)) NvVid.Evento(EV_CONSULTA_LENTA, 0, 0);
                Volatile.Write(ref f.consulta, 0);
            }
        }

        // ---- subtitle clock (owner 25/10) ------------------------------------
        // .NET's Stopwatch on Linux is CLOCK_MONOTONIC; the C side checks it.
        static long Ns(long ticks) =>
            Stopwatch.Frequency == 1000000000L ? ticks : (long)(ticks * (1e9 / Stopwatch.Frequency));

        bool posicaoAusente;   // older libnuvio: nv_tpk_video_posicao missing
        void Informar(int ms, int motivo, long inicio, long fim)
        {
            if (posicaoAusente) return;
            int dur = (int)((fim - inicio) * 1000 / Stopwatch.Frequency);
            try { NvVid.Posicao(ms, motivo, dur, Ns(inicio + (fim - inicio) / 2)); }
            catch (EntryPointNotFoundException) { posicaoAusente = true; }
            catch (Exception) { }
        }

        // A real reading of where the TV is, OFF the command queue (firmware reads
        // can take seconds). After a seek the TV lands on a keyframe (6.3 s before
        // the target in the log of 25/10) and after a pause the last poll can be
        // ~0.5 s old; Tique only reads while Playing. motivo: 1 seek, 2 pause,
        // 3 pause settled. Shares Tique's "one query in flight" flag.
        void LerPosicao(Player p, int minha, int ger, int motivo, int atrasoMs)
        {
            var f = Volatile.Read(ref fio);
            if (f == null || p == null) return;
            _ = Task.Run(async () => {
                if (atrasoMs > 0) await Task.Delay(atrasoMs);
                for (int espera = 0; Interlocked.Exchange(ref f.consulta, 1) != 0; espera += 20) {
                    if (espera >= 1500) return;
                    await Task.Delay(20);
                }
                long inicio = Stopwatch.GetTimestamp();
                try {
                    if (playerSessao != minha || minha != Volatile.Read(ref sessao) || player != p || ger != Volatile.Read(ref posGer)) return;
                    var st = p.State;
                    if (st != PlayerState.Playing && st != PlayerState.Paused) return;
                    int ms = p.GetPlayPosition();
                    long fim = Stopwatch.GetTimestamp();
                    lock (trocaTrava) {
                        if (minha == sessao && player == p && ger == posGer) { posMs = ms; Informar(ms, motivo, inicio, fim); }
                    }
                } catch { }
                finally { Volatile.Write(ref f.consulta, 0); }
            });
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

        // Checagem da sessao e aviso sob a trava do Trocar: um EV_PRESO da
        // sessao velha nunca chega depois de o C abrir a nova.
        void Preso(int minha, int alvo)
        {
            lock (trocaTrava) { if (minha == sessao) NvVid.Evento(EV_PRESO, alvo, 0); }
        }

        // Invalida tambem comandos que ficaram na fila durante uma troca.
        void ComSessao(Action a, bool exigePlayer = true)
        {
            int minha = Volatile.Read(ref sessao);
            var f = Volatile.Read(ref fio);
            f?.Post(_ => { if (minha == Volatile.Read(ref sessao) && (!exigePlayer || playerSessao == minha)) a(); }, null);
        }

        // Descarte independente da fila: ate GetPlayPosition/Prepare presos
        // deixam passar Muted e Display=null. Nao cria outro dono do decoder
        // sem confirmar Dispose; falha/timeout vira erro recuperavel na UI.
        void Trocar(Action<int> abrir)
        {
            int minha;
            FioVideo novo;
            Task<bool> soltar;
            lock (trocaTrava)
            {
                minha = Interlocked.Increment(ref sessao);
                fio?.Fechar();
                var velho = player; player = null;
                var prime = primer; primer = null; primerGen++;
                if (velho != null || prime != null || naoSolto != null) {
                    var anterior = liberacao;
                    liberacao = Task.Run(async () => {
                        await anterior.ConfigureAwait(false);
                        // #412: Dispose recusado no player preso. Cada abertura
                        // nova tenta de novo; sem Dispose confirmado, nada abre.
                        bool ok = true;
                        var resto = Interlocked.Exchange(ref naoSolto, null);
                        if (resto != null) ok = Soltar(resto);
                        if (prime != null) ok = Liberar(prime) && ok;
                        if (velho != null) ok = Liberar(velho) && ok;
                        return ok;
                    });
                }
                soltar = liberacao;
                ReportWindowMetrics(); windowMetrics = new VideoWindowMetrics();
                novo = fio = new FioVideo(Log);
            }
            Principal(() => {
                if (janelaMovida && GeometriaJanela != null) {
                    try { RestaurarJanela(); } catch (Exception e) { Log("reset-window: " + e.Message); }
                }
            });
            novo.Post(async _ => {
                // #603: reset before yielding to decoder release. Trailer volume
                // can arrive before this session has a Player; keep that request.
                volAlvo = 1f; volPedido = false;
                if (await Task.WhenAny(soltar, Task.Delay(PrazoPararMs)) != soltar || !await soltar) {
                    Log("[video] tpk parar nao confirmou; player indisponivel, processo preservado");
                    lock (trocaTrava) { if (minha == sessao) NvVid.Evento(EV_ERRO, -3, 0); }
                    return;
                }
                if (minha != Volatile.Read(ref sessao)) return;
                Log("[video] tpk parar confirmado (fio video)");
                if (abrir != null) abrir(minha);
            }, null);
        }

        async void Abrir(string url, string cabecalhos, int minha)
        {
            posMs = 0; acabou = false; Interlocked.Increment(ref posGer);
            buscando = false; buscaPend = pausaPend = -1;
            aspectoRecusado = false;
            try
            {
                var p = new Player();
                // Publicar e trocar de sessao usam a mesma trava: um Trocar no
                // meio do "new Player()" ou captura p (e solta) ou o acha aqui.
                bool viva;
                lock (trocaTrava) { viva = minha == Volatile.Read(ref sessao); if (viva) { player = p; playerSessao = minha; } }
                if (!viva) { Liberar(p); return; }
                p.PlaybackCompleted += (s, e) => ComSessao(() => {
                    // #302: completion alone is not proof that the pipeline ended.
                    // A live segment must not reopen a player that is still playing.
                    // Serialize the state read with player commands, never on the callback thread.
                    if (minha != Volatile.Read(ref sessao) || player != p) return;
                    try {
                        if (p.State != PlayerState.Ready) return;
                        lock (trocaTrava) {
                            if (minha == sessao && player == p && !acabou) {
                                acabou = true;
                                Interlocked.Increment(ref posGer);
                                // Ready no longer exposes a playing position. Publish EOS
                                // at duration so the existing next-episode flow can run.
                                try { posMs = p.StreamInfo.GetDuration(); } catch { }
                                NvVid.Evento(EV_FIM, 0, 0);
                            }
                        }
                    } catch (Exception ex) { Log("completion state: " + ex.Message); }
                });
                p.ErrorOccurred += (s, e) => { if (minha == Volatile.Read(ref sessao)) { Log("erro " + e.Error); NvVid.Evento(EV_ERRO, (int)e.Error, 0); } };
                p.BufferingProgressChanged += (s, e) => { if (minha == Volatile.Read(ref sessao)) NvVid.Evento(EV_BUFFER, e.Percent, 0); };
                p.PlaybackInterrupted += (s, e) => { if (minha == Volatile.Read(ref sessao)) { Log("interrompido: " + e.Reason); NvVid.Evento(EV_PAUSADO, 0, 0); } };
                p.SubtitleUpdated += (s, e) => { if (minha == Volatile.Read(ref sessao)) NvVid.Legenda(e.Text ?? "", (int)e.Duration); };
                foreach (var linha in (cabecalhos ?? "").Split('\n'))
                {
                    int i = linha.IndexOf(':');
                    if (i <= 0) continue;
                    string nome = linha.Substring(0, i).Trim(), valor = linha.Substring(i + 1).Trim();
                    if (nome == "X-Nuvio-Prebuffer")
                    {
                        try
                        {
                            int ms = int.Parse(valor), rc = player_set_streaming_buffering_time(p.Handle, ms, 0), pre = -1, re = -1;
                            player_get_streaming_buffering_time(p.Handle, out pre, out re);
                            Log("[dts-tpk] prebuffer " + ms + " ms: rc=" + rc + ", player says pre=" + pre + " re=" + re);
                        }
                        catch (Exception e) { Log("[dts-tpk] prebuffer: " + e.GetType().Name + " " + e.Message); }
                        continue;
                    }
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
        // host 6+ (Program.cs) chama isto; no 4/5 `primer` e sempre null e o
        // Trocar so encontra `player` para soltar.
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
        // Muted e Display=null vem primeiro: silenciam e soltam o plano mesmo
        // quando Stop/Unprepare travam (#412, saida para a Home).
        Player naoSolto; // Dispose falhou; o proximo Trocar tenta de novo

        bool Soltar(Player p)
        {
            try { p.Dispose(); Log("[video] tpk dispose confirmado na nova tentativa"); return true; }
            catch (Exception e) { Log("parar dispose de novo: " + e.Message); Volatile.Write(ref naoSolto, p); return false; }
        }

        bool Liberar(Player p)
        {
            // Dispose owns native teardown. Preliminary property reads/writes,
            // Stop and Unprepare can each block beyond the release deadline.
            // Trocar still requires confirmed disposal before creating a player.
            try { p.Dispose(); return true; }
            catch (Exception e) { Log("parar dispose: " + e.Message); Volatile.Write(ref naoSolto, p); return false; }
        }

        bool Liberar(ref Player p) { if (!Liberar(p)) return false; Volatile.Write(ref p, null); return true; }

        bool buscando;
        int buscaPend = -1, pausaPend = -1;

        void Pausar(bool pausa)
        {
            var p = player;
            int minha = playerSessao;
            if (p == null) return;
            if (buscando) { pausaPend = pausa ? 1 : 0; return; }
            try
            {
                if (pausa && p.State == PlayerState.Playing) {
                    p.Pause(); NvVid.Evento(EV_PAUSADO, 0, 0);
                    // Where it really stopped, then once more when the pipeline has settled.
                    int ger = Volatile.Read(ref posGer);
                    LerPosicao(p, minha, ger, 2, 0);
                    LerPosicao(p, minha, ger, 3, 300);
                }
                else if (!pausa && (p.State == PlayerState.Paused || p.State == PlayerState.Ready)) { acabou = false; p.Start(); windowMetrics.Invalidate(); NvVid.Evento(EV_TOCANDO, 0, 0); }
            }
            catch (InvalidOperationException e)
            {
                Log("pausar: " + e.Message);
                Preso(minha, -1);
            }
            catch (Exception e) { Log("pausar: " + e.Message); }
        }

        async void Buscar(int ms)
        {
            if (player == null) return;
            windowMetrics.Invalidate();
            if (buscando) { buscaPend = ms; return; }
            var p = player;
            int minha = Volatile.Read(ref sessao);
            buscando = true;
            acabou = false;
            Interlocked.Increment(ref posGer);
            bool expirou = false;
            try {
                var busca = p.SetPlayPositionAsync(ms, false);
                _ = busca.ContinueWith(t => { var _e = t.Exception; }, TaskContinuationOptions.OnlyOnFaulted);
                if (await Task.WhenAny(busca, Task.Delay(PrazoBuscarMs)) != busca) {
                    expirou = true; // Nao aceitar comandos neste player ate a troca.
                    Log("buscar: prazo vencido");
                    Preso(minha, buscaPend >= 0 ? buscaPend : ms);
                    return;
                }
                await busca;
                // So depois do seek aceito: pausado, o Tique nao consulta.
                lock (trocaTrava) {
                    if (minha == sessao && player == p) {
                        posMs = ms;
                        NvVid.Evento(EV_BUSCOU, ms, 0);
                    }
                }
                // The target is not where the TV is: it seeks to a keyframe. Read it.
                LerPosicao(p, minha, Volatile.Read(ref posGer), 1, 0);
                if (minha != Volatile.Read(ref sessao)) return;
                // PlaybackInterrupted pode ter emitido evento 3 durante o seek.
                if (p.State == PlayerState.Playing) NvVid.Evento(EV_TOCANDO, 0, 0);
                else if (p.State == PlayerState.Paused) NvVid.Evento(EV_PAUSADO, 0, 0);
            }
            catch (InvalidOperationException e) {
                Log("buscar: " + e.Message);
                // The core decides (mkvfix.h): an MKV with a hidden index reopens
                // through the local fix; with no index at all, it tells the user.
                NvVid.Evento(EV_BUSCA_FALHOU, ms, 0);
                Preso(minha, buscaPend >= 0 ? buscaPend : ms);
                return;
            }
            catch (Exception e) { Log("buscar: " + e.Message); NvVid.Evento(EV_BUSCA_FALHOU, ms, 0); return; }
            finally { if (!expirou && minha == Volatile.Read(ref sessao)) buscando = false; }
            if (minha != Volatile.Read(ref sessao)) return;
            // SetPlayPositionAsync terminou: nao disputar Pause/outro seek com
            // a transicao que produz evento 3; o cache ja recebeu o alvo aceito.
            if (buscaPend >= 0) { int alvo = buscaPend; buscaPend = -1; Buscar(alvo); }
            else if (pausaPend >= 0) { bool pausa = pausaPend != 0; pausaPend = -1; Pausar(pausa); }
        }

        // Host 4/5 (Program40): move/redimensiona a janela ElmSharp do video.
        // Devolve null se aplicou, ou a mensagem do erro. Fica null no 6+, que
        // continua so com Mode=Roi + SetRoi (comportamento inalterado).
        public Func<int, int, int, int, string> GeometriaJanela;
        bool janelaMovida;

        void RestaurarJanela()
        {
            string erro = GeometriaJanela(0,0,telaW,telaH);
            if (erro != null) throw new InvalidOperationException(erro);
            janelaMovida = false;
        }

        void Janela(int x, int y, int w, int h)
        {
            Janela(x,y,w,h,x,y,w,h,x == 0 && y == 0 && w == telaW && h == telaH);
        }

        void Janela(int x, int y, int w, int h, int lx, int ly, int lw, int lh)
        {
            Janela(x,y,w,h,lx,ly,lw,lh,false);
        }

        // #426 (Q7FN, Tizen 4.0): a ROI/window with negative origin or bigger
        // than the panel leaves the 2018 plane anchored top-left with a black
        // band below. Tizen 5+ took it in the field, so only Program40 on a 4.x
        // platform clears this; zoom then degrades to contain, centered.
        public bool PlanoAceitaRoiFora = true;

        // Keeps a destination rectangle inside the panel. Bigger than the panel:
        // contain-fit, centered (aspect kept). Otherwise: slide inside.
        internal static void CabeNaTela(ref int x, ref int y, ref int w, ref int h, int telaW, int telaH)
        {
            if (w <= 0 || h <= 0 || telaW <= 0 || telaH <= 0) return;
            if (w > telaW || h > telaH)
            {
                double s = Math.Min((double)telaW / w, (double)telaH / h);
                w = Math.Max(1, Math.Min(telaW, (int)Math.Round(w * s)));
                h = Math.Max(1, Math.Min(telaH, (int)Math.Round(h * s)));
                x = (telaW - w) / 2;
                y = (telaH - h) / 2;
                return;
            }
            x = Math.Max(0, Math.Min(x, telaW - w));
            y = Math.Max(0, Math.Min(y, telaH - h));
        }

        void Janela(int x, int y, int w, int h, int lx, int ly, int lw, int lh, bool telaCheiaSimples)
        {
            if (player == null || w <= 0 || h <= 0) return;
            if (!PlanoAceitaRoiFora)
            {
                CabeNaTela(ref x, ref y, ref w, ref h, telaW, telaH);
                CabeNaTela(ref lx, ref ly, ref lw, ref lh, telaW, telaH);
            }
            bool applied = false;
            windowMetrics.Begin(player, x, y, w, h, x == 0 && y == 0 && w == telaW && h == telaH);
            long started = Stopwatch.GetTimestamp();
            string motivo = null;
            try
            {
                // Before the aspect bridge, the simple fullscreen callback
                // meant preserve source aspect. Roi stretches 2.39:1 and 4:3.
                if (telaCheiaSimples)
                {
                    if (janelaMovida && GeometriaJanela != null) RestaurarJanela();
                    player.DisplaySettings.Mode = PlayerDisplayMode.LetterBox;
                    applied = true;
                    return;
                }
                if (!aspectoRecusado)
                {
                    try
                    {
                        if (janelaMovida && GeometriaJanela != null) RestaurarJanela();
                        int rc = player_set_display_mode(player.Handle, PlayerDisplayMode.Roi);
                        if (rc == 0) rc = player_set_display_roi_area(player.Handle, x, y, w, h);
                        if (rc == 0) { applied = true; return; }
                        motivo = "DisplayRoi rc=0x" + rc.ToString("x8");
                    }
                    catch (Exception e) { motivo = "DisplayRoi " + e.GetType().Name + ": " + e.Message; }
                    aspectoRecusado = true; // Only this player generation; a new open retries B.
                    // A driver may mutate Mode/ROI before returning an error.
                    // Leave Roi before applying the old path; never cache partial success.
                    player.DisplaySettings.Mode = PlayerDisplayMode.LetterBox;
                }
                bool fullscreen = lx == 0 && ly == 0 && lw == telaW && lh == telaH;
                if (GeometriaJanela != null) applied = JanelaTizen45(lx, ly, lw, lh, fullscreen);
                else
                {
                    player.DisplaySettings.Mode = fullscreen ? PlayerDisplayMode.LetterBox : PlayerDisplayMode.Roi;
                    if (!fullscreen) player.DisplaySettings.SetRoi(new Rectangle(lx, ly, lw, lh));
                    applied = true;
                }
            }
            catch (Exception e)
            {
                motivo = (motivo ?? "legacy") + "; restore: " + e.GetType().Name + ": " + e.Message;
            }
            finally
            {
                if (!applied)
                {
                    // Failed legacy work must not leave a moved ElmSharp window or active partial ROI.
                    try { player.DisplaySettings.Mode = PlayerDisplayMode.LetterBox; }
                    catch (Exception e) { motivo += "; reset-mode: " + e.Message; }
                    if (GeometriaJanela != null && janelaMovida)
                    {
                        try { RestaurarJanela(); }
                        catch (Exception e) { motivo += "; reset-window: " + e.Message; }
                    }
                }
                if (motivo != null) Log("[aspecto] sessao=" + playerSessao + " " + motivo.Replace('\n',' ').Replace('\r',' ') + " fallback=" + (applied ? "ok" : "failed"));
                windowMetrics.Complete(applied, Stopwatch.GetTimestamp() - started);
            }
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
                janelaMovida = e0 != null;
                player.DisplaySettings.Mode = PlayerDisplayMode.LetterBox;
                Log("[aspect] tpk40 modo=letterbox " + rotulo + (e0 == null ? " ok" : " falhou: " + e0));
                return e0 == null;
            }
            janelaMovida = true; // The void geometry setter may partially mutate then fail.
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
                if (janelaMovida) RestaurarJanela();
                player.DisplaySettings.Mode = PlayerDisplayMode.Roi;
                player.DisplaySettings.SetRoi(new Rectangle(x, y, w, h));
                Log("[aspect] tpk40 modo=roi " + rotulo + " ok");
                return true;
            }
            catch (Exception e) { Log("[aspect] tpk40 modo=roi " + rotulo + " falhou: " + e.GetType().Name + ": " + e.Message); return false; }
        }
    }
}
