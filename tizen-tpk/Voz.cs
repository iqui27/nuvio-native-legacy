// BOTAO DE MICROFONE DAS BUSCAS. O teclado do Nuvio Legacy
// continua sendo o teclado das duas buscas (a da barra lateral e o Spotlight do
// CH-). Ao lado do "Digitar pelo celular" fica um botao de microfone que abre
// o TECLADO DA SAMSUNG so para usar o microfone dele: a pessoa aperta o
// microfone do controle, fala, e o texto entra no campo do app.
//
// POR QUE O TECLADO DA SAMSUNG: e por ele que a voz chega aos apps na Samsung
// (o NuvioTVSmart e o Stremio usam o teclado da Samsung como teclado principal,
// e o microfone do controle dita nele). A Samsung nao da a terceiros uma API de
// busca por voz com texto livre, e nao ha chamada para o teclado ja abrir
// ouvindo: a pessoa aperta o microfone do controle depois que ele aparece.
//
// COMO: um TextField 2x2 invisivel na janela principal ganha o foco do NUI e o
// IME da Samsung sobe sobre ele. O campo so e focavel enquanto o botao o usa,
// para o foco do NUI nunca parar nele sozinho. O que o teclado escreve (letras
// ou fala) vem inteiro para o C; Concluido entrega, Voltar cancela, o painel
// fechado pelo proprio teclado entrega o que ja tiver sido dito.
//
// QUANDO O BOTAO APARECE: o host registra sempre que monta o TextField e o
// InputMethodContext, e diz o que a TV respondeu em
// http://tizen.org/feature/microphone (1 tem, 0 nao tem, -1 sem resposta). O C
// decide com o ajuste Tela inicial > Mais opcoes > Barra lateral > "Botao de
// microfone na busca": Automatico esconde so com 0; Ligado mostra sempre;
// Desligado esconde. Sem registro (sem IME), nunca ha botao.
//
// Ponte (src/entrada_texto.c): C -> host iniciar()/parar() (fio do C; aqui vao
// ao fio principal); host -> C nv_tpk_texto_valor(texto inteiro),
// nv_tpk_texto_fim(1) com texto, nv_tpk_texto_fim(0) sem texto/Voltar.
//
// NAO TESTADO NUMA TV. No 6.0/6.5 (GLWindow por cima) o IME pode nao subir; no
// 8/9 (GLView na janela principal) o TextField esta na mesma janela. O log diz:
// "[texto] host voz ...".
using System;
using System.Runtime.InteropServices;
using System.Threading;
using Tizen.NUI;
using Tizen.NUI.BaseComponents;

namespace NuvioTpk
{
    class Voz
    {
        [DllImport("libnuvio.so")] static extern void nv_tpk_voz_registrar(IntPtr iniciar, IntPtr parar, int microfone);
        [DllImport("libnuvio.so")] static extern void nv_tpk_texto_valor(string valor);
        [DllImport("libnuvio.so")] static extern void nv_tpk_texto_fim(int confirmou);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FnSemArgs();

        readonly Action<string> log;
        // Referencias vivas: o C guarda os ponteiros.
        readonly FnSemArgs fIniciar, fParar;

        TextField campo;
        InputMethodContext imf;
        bool aberto, viuPainel;
        Tizen.NUI.Timer prazo;
        int sessao, aberturaPendente;

        public Voz(Action<Action> principal, Action<string> log)
        {
            this.log = log;
            fIniciar = () =>
            {
                int minha = Volatile.Read(ref sessao);
                Interlocked.Exchange(ref aberturaPendente, 1);
                principal(() => { if (minha == Volatile.Read(ref sessao)) Iniciar(); });
            };
            fParar = () => principal(Parar);
            int mic = Microfone();
            if (!MontarCampo()) return;
            nv_tpk_voz_registrar(Marshal.GetFunctionPointerForDelegate(fIniciar),
                                 Marshal.GetFunctionPointerForDelegate(fParar), mic);
            Log("botao de microfone registrado (microfone=" + mic + ")");
        }

        void Log(string s) { try { log("[texto] host voz " + s); } catch { } }

        // 1 = tem, 0 = nao tem, -1 = a TV nao respondeu.
        int Microfone()
        {
            const string chave = "http://tizen.org/feature/microphone";
            try
            {
                bool tem;
                if (Tizen.System.Information.TryGetValue(chave, out tem))
                {
                    Log("feature/microphone = " + tem);
                    return tem ? 1 : 0;
                }
                Log("feature/microphone sem resposta");
            }
            catch (Exception e) { Log("feature/microphone: " + e.GetType().Name + ": " + e.Message); }
            return -1;
        }

        bool MontarCampo()
        {
            try
            {
                campo = new TextField
                {
                    Size2D = new Size2D(2, 2),
                    Position2D = new Position2D(0, 0),
                    Opacity = 0.01f,
                    Focusable = false,
                    EnableSelection = false,
                };
                campo.TextChanged += (s, e) => { if (aberto) nv_tpk_texto_valor(campo.Text ?? ""); };
                campo.KeyEvent += (s, e) =>
                {
                    if (!aberto || e.Key.State != Key.StateType.Down) return false;
                    string k = e.Key.KeyPressedName;
                    if (k == "Return" || k == "KP_Enter" || k == "Select") { Terminar(true); return true; }
                    if (k == "XF86Back" || k == "Escape") { Terminar(false); return true; }
                    return false;
                };
                Window.Instance.Add(campo);
                imf = campo.GetInputMethodContext();
                if (imf == null) { Log("sem InputMethodContext: sem botao"); return false; }
                imf.StatusChanged += (s, e) =>
                {
                    Log("teclado da Samsung " + (e.StatusChanged ? "aberto" : "fechado"));
                    if (aberto && e.StatusChanged) { viuPainel = true; SoltarPrazo(); }
                    // Fechou pelo proprio teclado: o que foi dito vale.
                    else if (aberto && viuPainel) Terminar(true);
                };
                return true;
            }
            catch (Exception e) { Log("TextField indisponivel: " + e.GetType().Name + ": " + e.Message); return false; }
        }

        void Iniciar()
        {
            Interlocked.Exchange(ref aberturaPendente, 0);
            if (aberto) return;
            try
            {
                // Limpo ANTES de abrir: TextChanged nao apaga a busca do app.
                campo.Text = "";
                aberto = true;
                viuPainel = false;
                campo.Focusable = true;
                if (!FocusManager.Instance.SetCurrentFocusView(campo))
                    throw new InvalidOperationException("TextField nao ganhou foco");
                // O IME pode aceitar ShowInputPanel sem nunca aparecer (GLWindow).
                prazo = new Tizen.NUI.Timer(3000);
                prazo.Tick += (s, e) =>
                {
                    if (aberto && !viuPainel) { Log("teclado nao apareceu em 3 s"); Terminar(false); }
                    return false;
                };
                prazo.Start();
                imf.Activate();
                imf.ShowInputPanel();
                Log("teclado da Samsung pedido");
            }
            catch (Exception e)
            {
                Log("abrir teclado: " + e.Message);
                // Inclui falhas anteriores a aberto=true (Text/Focusable/foco).
                aberto = true;
                Terminar(false);
            }
        }

        // Concluido / painel fechado (confirmou) ou Voltar. So entrega com texto;
        // o que ja veio pelo TextChanged fica no campo do app nos dois casos.
        void Terminar(bool confirmou)
        {
            if (!aberto) return;
            aberto = false;
            string t = (campo.Text ?? "").Trim();
            if (confirmou && t.Length > 0) nv_tpk_texto_valor(t);
            nv_tpk_texto_fim(confirmou && t.Length > 0 ? 1 : 0);
            Esconder();
        }

        // Pausa/fim da janela: cancela a sessao (e abertura enfileirada) antes de esconder.
        public void Cancelar()
        {
            Interlocked.Increment(ref sessao);
            bool pendente = Interlocked.Exchange(ref aberturaPendente, 0) == 1;
            if (aberto) Terminar(false);
            else if (pendente) nv_tpk_texto_fim(0);
        }

        // O app fechou (Voltar na tela, tela fechou): esconde sem avisar de volta.
        void Parar()
        {
            if (!aberto) return;
            aberto = false;
            Esconder();
        }

        void SoltarPrazo()
        {
            try { prazo?.Stop(); } catch { }
            prazo = null;
        }

        void Esconder()
        {
            SoltarPrazo();
            try { imf?.HideInputPanel(); imf?.Deactivate(); } catch { }
            try { FocusManager.Instance.ClearFocus(); } catch { }
            campo.Focusable = false;
        }
    }
}
