#ifndef NV_ENTRADA_TEXTO_H
#define NV_ENTRADA_TEXTO_H
// TECLADO (E DITADO) DO SISTEMA, por plataforma, atras de uma API so.
//
// Quem tem um campo de texto (Spotlight, Busca) pergunta se ha teclado do
// sistema, abre, e recebe o VALOR INTEIRO do campo por um evento SDL proprio.
// Sem teclado do sistema, nada muda: o teclado desenhado pelo app continua.
//
//   if (texto_sistema_disponivel() & TS_TECLADO) mostrar o campo clicavel;
//   OK no campo:           texto_sistema_abrir(consulta, 0);
//   botao Falar (TS_VOZ):  texto_sistema_abrir(consulta, 1);
//   no laco de eventos:
//     if (e->type == texto_sistema_evento()) {
//       if (e->user.code == TS_EV_TEXTO)  spot_texto_externo(texto_sistema_valor());
//       if (e->user.code == TS_EV_FIM)    ...o teclado fechou (e->user.data1 != 0: confirmou)
//       return;
//     }
//     if (texto_sistema_aberto() && texto_sistema_engole(e)) return;
//   ao fechar a tela:      texto_sistema_fechar();
//
// POR QUE O VALOR INTEIRO, e nao letra a letra: no .wgt e no .tpk quem edita e
// o IME da Samsung, sobre um campo DELE (o <input> escondido, o Entry do
// host), e o que sai de la e o texto todo — inclusive correcao, apagar no meio,
// ditado que troca tudo. Na LG e no Android o SDL entrega SDL_TEXTINPUT e
// Backspace; este modulo os acumula sobre o `atual` e devolve o mesmo valor
// inteiro. O campo do app vira ESPELHO de texto_sistema_valor() enquanto
// texto_sistema_aberto(): uma regra so nas quatro plataformas.
//
// texto_sistema_engole(e): 1 para os eventos que JA entraram no valor
// (SDL_TEXTINPUT, SDL_TEXTEDITING, e o KEYDOWN/KEYUP de letra, digito, espaco e
// Backspace que o IME gera junto). O campo nao deve trata-los de novo, senao a
// letra entra duas vezes. Setas, OK e Voltar NAO sao engolidos.
//
// main.c chama texto_sistema_observar(&e) para cada evento (ANTES do app) e
// texto_sistema_quadro() uma vez por quadro: e assim que o modulo ve o texto e
// percebe que o teclado do sistema fechou por fora (Voltar no teclado da TV).
//
// O QUE CADA PLATAFORMA FAZ (detalhes e o que foi medido: entrada_texto.c)
//   LG webOS  SDL_StartTextInput do SDL DA TV (libsdl2-webos, com text_model).
//   Android   SDL_StartTextInput (IME do sistema; Gboard tem microfone).
//   .wgt      <input> escondido no tizen-shell.html: focus() chama o teclado
//             da Samsung para digitar (TS_TECLADO) e para o botao de
//             microfone das buscas (TS_VOZ, conforme o ajuste).
//   .tpk      host .NET. Release: o mesmo botao (Voz.cs). Canario: teclado e
//             ditado (Texto.cs).
//   Mac       nada (o teclado fisico ja escreve).
#include <SDL2/SDL.h>

enum { TS_TECLADO = 1, TS_VOZ = 2 };
enum { TS_EV_TEXTO = 1, TS_EV_FIM = 2, TS_EV_ERRO = 3 };

// Bits TS_*. 0 = use o teclado do app. TS_VOZ so onde ESTE modulo inicia a
// voz (hoje: o botao de microfone do .tpk e do .wgt). No Android o ditado
// continua com o android_ditado_* do Spotlight.
int  texto_sistema_disponivel(void);
// `atual` e o texto que o campo ja tem (o IME comeca dele). voz=1 pede o
// ditado onde houver; sem ditado, abre so o teclado.
void texto_sistema_abrir(const char *atual, int voz);
void texto_sistema_fechar(void);
int  texto_sistema_aberto(void);
// BOTAO DE MICROFONE DAS BUSCAS (.tpk e .wgt). Ajuste "Botao de microfone na busca"
// (valores na ordem do ajuste): Automatico esconde o botao so na TV que diz
// nao ter microfone. main.c repassa a cada quadro.
enum { TS_MIC_AUTO = 0, TS_MIC_LIGADO = 1, TS_MIC_DESLIGADO = 2 };
void texto_sistema_mic_modo(int modo);
// 1 = a voz daqui e o teclado da Samsung aberto pelo botao de microfone: ele
// ainda nao ouve (a pessoa aperta o microfone do controle), e o teclado modal
// (teclado.c) nao mostra "Falar".
int  texto_sistema_voz_pelo_teclado(void);
// Tipo SDL dos avisos (SDL_RegisterEvents, uma vez). user.code = TS_EV_*;
// em TS_EV_FIM, user.data1 != NULL quando a pessoa confirmou (OK/Concluido).
Uint32 texto_sistema_evento(void);
// Valor inteiro do campo, UTF-8, ate 255 bytes. Vale no fio do app.
const char *texto_sistema_valor(void);
int  texto_sistema_engole(const SDL_Event *e);

// Ganchos do laco (main.c).
void texto_sistema_observar(const SDL_Event *e);
void texto_sistema_quadro(void);

// PONTES (chamadas por quem fala com o IME de fora do SDL: o JS do .wgt via
// texto_sistema_quadro, o host .NET do .tpk). Seguras de qualquer fio.
void texto_sistema_ponte_valor(const char *valor);
// 1 confirms, 0 cancels, -1 reports a keyboard startup failure.
void texto_sistema_ponte_fim(int confirmou);
#endif
