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
// The microphone button registers with unknown capability (-1) at startup.
// TextField/InputMethodContext and feature/microphone are accessed only on an
// explicit request, so defective IME firmware cannot block launch. Automatic
// mode hides the button after a negative capability response; failed IME setup
// cancels the request and leaves the app keyboard usable.
//
// TECLADO DO SISTEMA PARA DIGITAR: com o ajuste Teclado em "Do sistema", o app
// abre este MESMO campo e o MESMO teclado da Samsung com o texto atual da busca
// (Iniciar(atual)), para digitar em vez de ditar. O texto inteiro e o cursor
// voltam a cada mudanca (nv_tpk_texto_valor / nv_tpk_texto_cursor). Concluido
// ou o painel fechado pelo teclado confirma; Voltar cancela. Se o teclado nao
// subir em 3 s o C desiste dele nesta sessao e a grade do app volta.
//
// TIPO DO CAMPO: o C diz se e texto, e-mail ou SENHA. Senha abre o teclado da
// Samsung no layout de senha e sem previsao (o teclado nao aprende a senha), e o
// campo escondido e limpo ao fechar. Registro novo (nv_tpk_voz_registrar_texto_
// tipo); com uma libnuvio.so antiga, que nao o tem, fica o registro sem tipo e o
// C novo, ao contrario, nunca manda senha a um host sem ele (fica na grade).
//
// ESQUERDA/DIREITA: o campo escondido move o cursor com as setas, tanto no
// microfone quanto ao digitar, e as setas NAO seguem para o app embaixo. O
// cursor e a propriedade "primaryCursorPosition" do DALi, lida e escrita pelo
// NOME: o TextField.PrimaryCursorPosition do C# nao existe na API8 (NuvioTpk60).
// O IMF recebe a mesma posicao, para o teclado da Samsung inserir ali. Ao
// digitar, o cursor vai ao C para o app desenhar o dele no mesmo lugar. Firmware
// sem a propriedade: as setas seguem como antes.
//
// Ponte (src/entrada_texto.c): C -> host iniciar()/parar() (fio do C; aqui vao
// ao fio principal) e abrirTexto(atual, tipo); host -> C nv_tpk_texto_valor(texto
// inteiro), nv_tpk_texto_cursor(posicao), nv_tpk_texto_fim(1) com texto,
// nv_tpk_texto_fim(0) sem texto/Voltar.
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
        [DllImport("libnuvio.so")] static extern void nv_tpk_voz_registrar_texto(IntPtr abrir);
        [DllImport("libnuvio.so")] static extern void nv_tpk_voz_registrar_texto_tipo(IntPtr abrir);
        [DllImport("libnuvio.so")] static extern void nv_tpk_texto_valor(string valor);
        [DllImport("libnuvio.so")] static extern void nv_tpk_texto_cursor(int posicao);
        [DllImport("libnuvio.so")] static extern void nv_tpk_texto_fim(int confirmou);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FnSemArgs();
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FnAbrirTexto(IntPtr atual);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FnAbrirTextoTipo(IntPtr atual, int tipo);
        const int TipoEmail = 1, TipoSenha = 2;   // ST_IME_* (src/sistexto.h)

        readonly Action<string> log;
        // Referencias vivas: o C guarda os ponteiros.
        readonly FnSemArgs fIniciar, fParar;
        readonly FnAbrirTexto fAbrirTexto;
        readonly FnAbrirTextoTipo fAbrirTextoTipo;

        TextField campo;
        InputMethodContext imf;
        bool aberto, viuPainel;
        bool modoTexto;   // aberto para DIGITAR (teclado do sistema), nao para o microfone
        int tipoAplicado;   // tipo do campo ja posto no TextField (0 = o padrao do DALi)
        Tizen.NUI.Timer prazo, relogioCursor;
        int sessao, aberturaPendente, ultimoCursor = -2;
        int indiceCursor = -2;   // propriedade do cursor no DALi; -2 = ainda nao procurada
        bool cursorFalhou;
        uint ultimoTempo;
        string ultimoNome;

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
            // Digitar no teclado da Samsung (ajuste Teclado "Do sistema"): o texto
            // atual e copiado AGORA, no fio do C, antes do buffer dele mudar.
            fAbrirTextoTipo = (p, tipo) =>
            {
                string atual = PtrToUtf8(p);
                int minha = Volatile.Read(ref sessao);
                Interlocked.Exchange(ref aberturaPendente, 1);
                principal(() => { if (minha == Volatile.Read(ref sessao)) Iniciar(atual, tipo); });
            };
            fAbrirTexto = p => fAbrirTextoTipo(p, 0);
            // Native IME crashes/hangs cannot be caught by C#. Never touch it
            // before the user requests the optional microphone keyboard (#558).
            nv_tpk_voz_registrar(Marshal.GetFunctionPointerForDelegate(fIniciar),
                                 Marshal.GetFunctionPointerForDelegate(fParar), -1);
            try { nv_tpk_voz_registrar_texto_tipo(Marshal.GetFunctionPointerForDelegate(fAbrirTextoTipo)); }
            catch (EntryPointNotFoundException) { nv_tpk_voz_registrar_texto(Marshal.GetFunctionPointerForDelegate(fAbrirTexto)); }
            Log("microphone registered; Samsung IME deferred until requested");
        }

        static string PtrToUtf8(IntPtr p)
        {
            if (p == IntPtr.Zero) return "";
            int n = 0;
            while (Marshal.ReadByte(p, n) != 0) n++;
            var b = new byte[n];
            Marshal.Copy(p, b, 0, n);
            return System.Text.Encoding.UTF8.GetString(b);
        }

        // Posicoes do cursor contam caracteres (codigos Unicode), nao unidades UTF-16.
        static int Caracteres(string s)
        {
            int n = 0;
            foreach (char c in s ?? "") if (!char.IsLowSurrogate(c)) n++;
            return n;
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
                tipoAplicado = 0;   // campo novo: o tipo padrao do DALi
                campo = new TextField
                {
                    Size2D = new Size2D(2, 2),
                    Position2D = new Position2D(0, 0),
                    Opacity = 0.01f,
                    Focusable = false,
                    EnableSelection = false,
                };
                campo.TextChanged += (s, e) =>
                {
                    if (!aberto) return;
                    nv_tpk_texto_valor(campo.Text ?? "");
                    EnviarCursor();
                };
                campo.KeyEvent += (s, e) =>
                {
                    if (!aberto) return false;
                    if (Tecla(e.Key)) return true;   // Esquerda/Direita movem o cursor
                    if (e.Key.State != Key.StateType.Down) return false;
                    string k = e.Key.KeyPressedName;
                    if (k == "Return" || k == "KP_Enter" || k == "Select") { Terminar(true); return true; }
                    if (k == "XF86Back" || k == "Escape") { Terminar(false); return true; }
                    return false;
                };
                Window.Instance.Add(campo);
                imf = campo.GetInputMethodContext();
                if (imf == null) throw new InvalidOperationException("No InputMethodContext");
                imf.StatusChanged += (s, e) =>
                {
                    Log("teclado da Samsung " + (e.StatusChanged ? "aberto" : "fechado"));
                    if (aberto && e.StatusChanged) { viuPainel = true; SoltarPrazo(); }
                    // Fechou pelo proprio teclado: o que foi dito vale.
                    else if (aberto && viuPainel) Terminar(true);
                };
                return true;
            }
            catch (Exception e)
            {
                Log("TextField indisponivel: " + e.GetType().Name + ": " + e.Message);
                try { if (campo != null) { Window.Instance.Remove(campo); campo.Dispose(); } } catch { }
                campo = null; imf = null;
                return false;
            }
        }

        // atual == null: botao de microfone (campo limpo). Senao: digitar no teclado
        // do sistema, com o texto que o campo do app ja tem.
        void Iniciar(string atual = null, int tipo = 0)
        {
            Interlocked.Exchange(ref aberturaPendente, 0);
            if (aberto) return;
            if (imf == null)
            {
                // Ao digitar, falhar avisa o C (-1) para a grade do app voltar; no
                // microfone segue como sempre (cancela em silencio).
                if (!MontarCampo()) { nv_tpk_texto_fim(atual != null ? -1 : 0); return; }
                int mic = Microfone();
                nv_tpk_voz_registrar(Marshal.GetFunctionPointerForDelegate(fIniciar),
                                     Marshal.GetFunctionPointerForDelegate(fParar), mic);
            }
            try
            {
                // Antes de abrir: TextChanged nao apaga a busca do app. No microfone o
                // campo comeca limpo; ao digitar, comeca com o texto do app.
                modoTexto = atual != null;
                // Antes de ativar o IME: senha nunca abre num campo comum. Falhar
                // aqui e o "nao subiu" (-1): a grade do app assume.
                AplicarTipo(modoTexto ? tipo : 0);
                campo.Text = atual ?? "";   // o DALi poe o cursor no fim do texto
                ultimoCursor = -2;
                cursorFalhou = false;
                aberto = true;
                viuPainel = false;
                campo.Focusable = true;
                if (!FocusManager.Instance.SetCurrentFocusView(campo))
                    throw new InvalidOperationException("TextField did not receive focus");
                // O IME pode aceitar ShowInputPanel sem nunca aparecer (GLWindow).
                prazo = new Tizen.NUI.Timer(3000);
                prazo.Tick += (s, e) =>
                {
                    if (aberto && !viuPainel) { Log("keyboard startup failed: panel did not appear within 3 s"); Terminar(false, true); }
                    return false;
                };
                prazo.Start();
                imf.Activate();
                imf.ShowInputPanel();
                if (modoTexto) IniciarRelogioCursor();
                Log("teclado da Samsung pedido" + (modoTexto ? " (digitar)" : " (microfone)"));
            }
            catch (Exception e)
            {
                Log("keyboard startup failed: " + e.GetType().Name + ": " + e.Message);
                // Inclui falhas anteriores a aberto=true (Text/Focusable/foco).
                aberto = true;
                Terminar(false, true);
            }
        }

        // Layout do teclado da Samsung e previsao, so quando o tipo muda.
        void AplicarTipo(int tipo)
        {
            if (tipo == tipoAplicado) return;
            var m = new InputMethod
            {
                PanelLayout = tipo == TipoSenha ? InputMethod.PanelLayoutType.Password
                            : tipo == TipoEmail ? InputMethod.PanelLayoutType.Email
                            : InputMethod.PanelLayoutType.Normal,
            };
            campo.InputMethodSettings = m.OutputMap;
            imf.TextPrediction = tipo != TipoSenha;
            tipoAplicado = tipo;
            Log("tipo do campo " + tipo);
        }

        // Concluido / painel fechado (confirmou) ou Voltar. No microfone so entrega com
        // texto; ao digitar, confirmar vale mesmo com o campo vazio (a pessoa pode
        // ter apagado tudo) e o texto segue como esta, sem aparar. O que ja veio
        // pelo TextChanged fica no campo do app nos dois casos.
        void Terminar(bool confirmou, bool falhou = false)
        {
            if (!aberto) return;
            aberto = false;
            string t = modoTexto ? (campo.Text ?? "") : (campo.Text ?? "").Trim();
            bool entrega = confirmou && (t.Length > 0 || modoTexto);
            if (entrega) nv_tpk_texto_valor(t);
            nv_tpk_texto_fim(falhou ? -1 : entrega ? 1 : 0);
            Esconder();
        }

        // ESQUERDA / DIREITA movem o cursor do texto (microfone e digitar). Chega
        // pelo campo e tambem pela janela (Program.Tecla): o mesmo toque conta uma
        // vez so, e nunca segue para o app embaixo. true = a tecla foi daqui.
        public bool Tecla(Key k)
        {
            if (!aberto || campo == null) return false;
            string nome = k.KeyPressedName;
            int d = nome == "Left" || nome == "KP_Left" ? -1 : nome == "Right" || nome == "KP_Right" ? 1 : 0;
            if (d == 0 || IndiceCursor() < 0) return false;
            // Time 0 (TV que nao informa) nunca e "o mesmo toque": cada repeticao anda.
            if (k.State == Key.StateType.Down && !(k.Time != 0 && k.Time == ultimoTempo && nome == ultimoNome))
            {
                ultimoTempo = k.Time;
                ultimoNome = nome;
                MoverCursor(d);
            }
            return true;
        }

        // Indice da propriedade "primaryCursorPosition" no DALi, procurado uma vez; -1 = nao tem.
        int IndiceCursor()
        {
            if (indiceCursor == -2)
            {
                try { indiceCursor = campo.GetPropertyIndex("primaryCursorPosition"); }
                catch { indiceCursor = -1; }
                if (indiceCursor < 0) { indiceCursor = -1; Log("cursor: primaryCursorPosition indisponivel"); }
            }
            return indiceCursor;
        }

        bool LerCursor(out int p)
        {
            p = 0;
            int i = IndiceCursor();
            if (i < 0) return false;
            using (var v = campo.GetProperty(i)) return v != null && v.Get(out p);
        }

        void MoverCursor(int d)
        {
            try
            {
                int p, n = Caracteres(campo.Text);
                if (!LerCursor(out p)) return;
                int q = Math.Max(0, Math.Min(n, p + d));
                if (q != p)
                {
                    using (var v = new PropertyValue(q)) campo.SetProperty(IndiceCursor(), v);
                    imf.SetCursorPosition((uint)q);
                    imf.NotifyCursorPosition();
                }
                Log("cursor " + p + " -> " + q + " de " + n);
                EnviarCursor();
            }
            catch (Exception e) { Log("cursor: " + e.GetType().Name + ": " + e.Message); }
        }

        // Ao digitar, o C desenha o cursor do app onde esta o do teclado.
        void EnviarCursor()
        {
            if (!modoTexto || campo == null || cursorFalhou) return;
            try
            {
                int p;
                if (!LerCursor(out p) || p == ultimoCursor) return;
                ultimoCursor = p;
                nv_tpk_texto_cursor(p);
            }
            catch (Exception e)
            {
                cursorFalhou = true;
                Log("cursor: " + e.GetType().Name + ": " + e.Message);
            }
        }

        // O teclado da Samsung tambem mexe no cursor por conta propria (sem
        // TextChanged): um relogio leve confere enquanto o campo esta aberto.
        void IniciarRelogioCursor()
        {
            PararRelogioCursor();
            if (IndiceCursor() < 0) return;
            relogioCursor = new Tizen.NUI.Timer(150);
            relogioCursor.Tick += (s, e) =>
            {
                if (!aberto || !modoTexto || cursorFalhou) return false;
                EnviarCursor();
                return true;
            };
            relogioCursor.Start();
        }

        void PararRelogioCursor()
        {
            try { relogioCursor?.Stop(); } catch { }
            relogioCursor = null;
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
            PararRelogioCursor();
            try { imf?.HideInputPanel(); imf?.Deactivate(); } catch { }
            try { FocusManager.Instance.ClearFocus(); } catch { }
            // A senha nao fica no campo escondido (aberto ja e false: nada vai ao C).
            if (tipoAplicado == TipoSenha) try { campo.Text = ""; } catch { }
            campo.Focusable = false;
        }
    }
}
