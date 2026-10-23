// TEXTO DO SISTEMA: o teclado (IME) e a voz do aparelho para os campos de texto
// do app (Spotlight, Busca e a modal de teclado.c).
//
// POR QUE EXISTE (relato do dono na TCL, Android 14, 01/10/2026): no Spotlight
// ele "nao consegue clicar no campo de texto, so no teclado", e queria DITADO.
// O microfone do controle da TCL manda ASSIST, que o Android entrega ao Gemini e
// nunca ao app (KeyEvent: "not delivered to applications"); o ditado tem de vir
// de dentro do app.
//
// O QUE O ANDROID FAZ (NuvioActivity.kt):
//   - TECLADO: um EditText invisivel recebe o foco e chama o IME do sistema
//     (Gboard e o teclado da TCL tem o proprio microfone: o ditado dele chega
//     como texto normal). A cada mudanca o texto INTEIRO vem para ca, com a
//     composicao em andamento: o campo do app espelha o do IME, nunca soma
//     letra por letra — por isso ditado, autocorrecao e apagar funcionam sem
//     contar teclas. Concluir/Voltar fecham o IME e devolvem o foco ao SDL.
//     O SDL_StartTextInput do SDL 2.30 NAO foi usado: ele gera KEYDOWN falso
//     para cada letra ASCII (SDLInputConnection.updateText), e o espaco e o
//     backspace falsos viravam "OK" e "fechar" nas telas do app.
//   - VOZ: android.speech.SpeechRecognizer DENTRO do app, com a permissao
//     RECORD_AUDIO pedida no primeiro uso (o dono: "o aplicativo nem pediu
//     permissao do mic igual outros pedem"); parciais aparecem no campo
//     enquanto a pessoa fala, e o nivel do som (onRmsChanged) anima o
//     microfone. Sem reconhecedor, ou com a permissao negada, cai na tela de
//     voz do sistema (RecognizerIntent); sem ela tambem, abre o teclado do
//     sistema e avisa que o microfone dele dita.
//
// FORA DO ANDROID (LG, Samsung, Mac) nada disto existe: st_*_disponivel() = 0 e
// as telas ficam como eram (no Spotlight, o teclado do app abre com OK no
// campo). O que se sabe, NADA testado em TV (01/10/2026):
//   - LG: o app usa o libSDL2 de fabrica; SUSPEITO que o SDL_StartTextInput
//     dele nao levante o teclado do webOS (pesquisa em spotlight.h, "LG E O
//     TECLADO DO SISTEMA"; a linha "[spotlight] lg sdl ... osk=N" do log vai
//     dizer). O caminho certo seria embarcar o SDL-webOS do webosbrew.
//   - Samsung .tpk (host .NET, Voz.cs) e .wgt (<input> do tizen-shell.html):
//     st_voz_disponivel() e o botao de microfone das buscas. st_voz_iniciar
//     abre o teclado da Samsung, o estado fica ST_DIGITANDO e st_aviso() diz
//     para apertar o microfone do controle (o teclado ainda nao ouve). O
//     teclado cobre a tela: o texto so chega ao campo quando ele fecha (ST_FIM).
//     No .wgt, st_ime_disponivel() tambem permite digitar no teclado da TV;
//     no .tpk de release, digitar continua no teclado do app.
//
// Para captura e teste no Mac, NUVIO_SISTEXTO_TESTE=1
// finge que existe (abrir so loga) e st_teste_evento injeta o que o Android
// mandaria.
#ifndef NV_SISTEXTO_H
#define NV_SISTEXTO_H
#include <stddef.h>

// Quem pediu: o texto que volta e so dele.
enum { ST_DONO_NENHUM = 0, ST_SPOT, ST_BUSCA, ST_TECLADO };
// O que esta aberto agora.
enum { ST_PARADO = 0, ST_DIGITANDO, ST_PERMISSAO, ST_OUVINDO, ST_VOZ_SISTEMA };
// Retorno de st_ler.
enum { ST_NADA = 0, ST_TEXTO, ST_FIM, ST_CANCELOU, ST_PEDE_TECLADO };

int   st_ime_disponivel(void);
int   st_voz_disponivel(void);
// O campo ja abre o teclado do sistema sozinho ao aparecer? So no Android (o
// pedido do dono); na LG/Samsung so com OK no campo.
int   st_abre_sozinho(void);
// Ajuste "Teclado" valido AQUI: 0 automatico, 1 so a grade do app, 2 o teclado do
// sistema direto. Onde o ajuste nao existe (Mac, testes) e sempre 0.
int   st_teclado_modo(void);
// 1 = "Do app" fora do Android: o teclado da TV (LG/Samsung) nunca abre.
int   st_teclado_do_app(void);
// Abre o teclado do sistema com `inicial` no campo (max em bytes). 1 = abriu.
int   st_ime_abrir(int dono, const char *inicial, int max);
// Tipo do PROXIMO st_ime_abrir (consumido nele): o Android abre o teclado de
// e-mail ou o de senha (sem sugestao, sem aprender o que foi digitado).
enum { ST_IME_TEXTO = 0, ST_IME_EMAIL = 1, ST_IME_SENHA = 2 };
void  st_ime_tipo(int tipo);
// Comeca o ditado (permissao, reconhecedor do app, ou os degraus de reserva).
int   st_voz_iniciar(int dono);
// Fecha o teclado e para a voz, se forem desse dono.
void  st_fechar(int dono);
int   st_estado(void);
int   st_dono(void);
// Nivel do som enquanto ouve, 0..1.
float st_nivel(void);
// Ultimo aviso para a pessoa (texto pt, passar por i18n), "" se nenhum. Vale
// ate a proxima abertura.
const char *st_aviso(void);
// Uma vez por quadro pelo dono. ST_TEXTO: `dst` e o texto INTEIRO do campo
// agora (substitui, nao soma). ST_FIM: o mesmo e a entrada terminou (Concluir
// ou fim da fala). ST_CANCELOU: fechou sem concluir (o texto que ja veio fica).
// ST_PEDE_TECLADO: a voz nao existe aqui; o dono abre st_ime_abrir com o texto
// dele (o aviso ja diz que o microfone do teclado do sistema dita).
int   st_ler(int dono, char *dst, size_t n);
// Mesmo retorno e ordem de st_ler. O sinal opcional distingue um valor vazio
// recebido de uma ausencia de texto, inclusive quando Voltar chega no mesmo
// lote. O dono decide se conserva esse valor; fechar nao conclui a entrada.
int   st_ler_valor(int dono, char *dst, size_t n, int *valorRecebido);

// Primeira coisa no laco de eventos de quem tem o campo: 1 = o evento era do
// texto do sistema (o aviso de valor/fim de entrada_texto.h, ou TEXTINPUT,
// letra e Backspace que JA entraram no valor) e nao deve ser tratado de novo.
// Com o teclado da TV aberto, Backspace NAO e "apagar/fechar" da tela.
#include <SDL2/SDL.h>
int   st_evento(const SDL_Event *e);

// --- testes ---
// Liga/desliga o modo de teste (o mesmo de NUVIO_SISTEXTO_TESTE=1).
void  st_teste_ligar(int on);
// Evento cru, no formato do Android (ver sistexto.c).
void  st_teste_evento(const char *ev);
#endif
