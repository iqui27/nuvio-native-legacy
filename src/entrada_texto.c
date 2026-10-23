// Teclado/ditado do sistema por plataforma. API e regra de uso: entrada_texto.h.
//
// LG webOS (pesquisa e medicao de 01/10/2026)
//   O app usa o libSDL2 DO APARELHO. Na C9 do dono (webOS 4.5, Rockhopper
//   4.10.2) o arquivo e /usr/lib/libSDL2-2.0.so.0.4.1, e as strings dele dizem
//   "lib32-libsdl2-webos/2.0.5-83.gld4tv": e o SDL 2.0.5 da LG, nao o upstream.
//   MEDIDO (strings do binario na TV): ele tem WebOSShowScreenKeyboard,
//   WebOSHideScreenKeyboard, WebOSIsScreenKeyboardShown, text_model_factory e
//   handlers TextModelEnter/Leave/KeySym/DeleteSurroundingText/
//   InputPanelState, mais "called text_model_activate". Ou seja, o SDL de
//   fabrica liga SDL_StartTextInput ao text_model do compositor (o mesmo
//   protocolo que o MaliitServer da TV atende). MEDIDO no D1 (registro 15988 e
//   16001, webos, Mali-G71): "[spotlight] lg sdl 2.0.5 osk=1 textinput=0" —
//   SDL_HasScreenKeyboardSupport responde 1 na TV.
//   MEDIDO NA C9 (OLED65C9PSA, 01/10/2026, porta "ime:abrir" + captura do
//   compositor por com.webos.service.tv.capture/executeOneShot):
//     - SDL_StartTextInput abre o teclado da LG (QWERTY "POR", com tecla de
//       MICROFONE, Enter, Limpar todos) na metade de baixo da tela; o
//       SDL_IsScreenKeyboardShown vira 1 uns 5 quadros depois.
//     - Texto confirmado chega como UM SDL_TEXTINPUT com a palavra inteira
//       (insertText "matrix" -> TEXTINPUT de 6 bytes; "x ção" -> 7 bytes,
//       UTF-8 certo), SEM KEYDOWN de letra. Apagar no teclado chega como
//       KEYDOWN SDLK_BACKSPACE (scancode 42).
//     - Enter do teclado FECHA o teclado e nao manda Return ao app; so se ve
//       o IsScreenKeyboardShown cair. Por isso na LG TS_EV_FIM nunca diz
//       "confirmou".
//     - SDL_StopTextInput esconde o teclado.
//     - O texto foi injetado pelo com.webos.service.ime/insertText (API
//       interna, so root): o MaliitServer entrega ao app pelo mesmo
//       commit_string que a digitacao no controle. Digitar com o controle de
//       verdade e a tecla de microfone NAO foram exercitados por mim.
//   ARMADILHA MEDIDA: o Spotlight de hoje descarta TEXTINPUT ASCII (espera o
//   KEYDOWN, que na LG nao vem) e fecha com Backspace no campo vazio — por isso
//   o consumidor TEM de usar texto_sistema_engole e o valor deste modulo.
//   Ditado: o servico com.webos.service.voiceinput da TV e PRIVADO
//   (api-permissions.d: getDevices/startStreaming... em "private"); app de
//   terceiros nao o chama. TS_VOZ fica 0 na LG.
//
// Android: o mesmo SDL_StartTextInput (o Spotlight ja usava). Ditado continua
// no android_ditado_* do Spotlight; aqui TS_VOZ fica 0.
//
// Samsung .wgt: o tizen-shell.html tem um <input> escondido e a funcao
// window.nvTexto. focus() por JS abre o IME porque o metadado
// use.keypad.without.useraction vale true por padrao (Samsung, guia
// "Keyboard/IME"). Done = keyCode 65376, Cancel = 65385. O JS guarda o valor e
// um contador; texto_sistema_quadro le os dois por EM_ASM (sem chamar o WASM de
// dentro de um evento JS, que com ASYNCIFY pode cair no meio de um sono).
// O <input> serve a digitacao e ao botao de microfone das buscas;
// nvTexto.microfone diz o que a TV respondeu sobre microfone.
//
// Samsung .tpk: o host .NET registra abrir/fechar por nv_tpk_texto_registrar
// (canario, Texto.cs) ou so o botao de microfone por nv_tpk_voz_registrar
// (release, Voz.cs), e devolve por nv_tpk_texto_valor/nv_tpk_texto_fim. Host
// antigo nao registra: texto_sistema_disponivel() da 0 e fica o teclado do app.
#include "entrada_texto.h"
#include "arteurl.h"
#include <stdio.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define TS_MAX NV_ARTE_URL_MAX

static int aberto, viuTeclado, quadrosAberto;
static Uint32 tipoEv;
static char valor[TS_MAX];          // fio do app
static char lido[TS_MAX];           // o que texto_sistema_valor devolve

// Pendencias das pontes (qualquer fio): protegidas pelo spinlock e
// consumidas em texto_sistema_quadro, no fio do app.
static SDL_SpinLock trava;
static char pendValor[TS_MAX];
static int pendTemValor, pendFim = -1;

#ifdef NV_TPK
typedef void (*FnTsAbrir)(const char *atual, int voz);
typedef void (*FnTsFechar)(void);
static FnTsAbrir hAbrir;
static FnTsFechar hFechar;
static int hFlags;
__attribute__((visibility("default")))
void nv_tpk_texto_registrar(FnTsAbrir abrir, FnTsFechar fechar, int flags) {
  hAbrir = abrir; hFechar = fechar; hFlags = flags;
  printf("[texto] host registrou o teclado do sistema (flags=%d)\n", flags);
  fflush(stdout);
}
__attribute__((visibility("default")))
void nv_tpk_texto_valor(const char *v) { texto_sistema_ponte_valor(v); }
__attribute__((visibility("default")))
void nv_tpk_texto_fim(int confirmou) { texto_sistema_ponte_fim(confirmou); }

// BOTAO DE MICROFONE DAS BUSCAS (tizen-tpk/Voz.cs, build de release). O host
// registra quando monta o campo do IME e diz o que a TV respondeu sobre
// microfone (1 tem, 0 nao tem, -1 sem resposta). O texto volta pelas mesmas
// nv_tpk_texto_valor/_fim.
typedef void (*FnVoz)(void);
static FnVoz vIniciar, vParar;
static int vMic = -1;
static int abertoPeloMic;   // o que esta aberto veio do botao de microfone
__attribute__((visibility("default")))
void nv_tpk_voz_registrar(FnVoz iniciar, FnVoz parar, int microfone) {
  vIniciar = iniciar; vParar = parar; vMic = microfone;
  printf("[texto] host registrou o botao de microfone (microfone=%d)\n", microfone);
  fflush(stdout);
}
#endif

#if defined(NV_TPK) || defined(__EMSCRIPTEN__)
// BOTAO DE MICROFONE DAS BUSCAS (.tpk e .wgt). O botao abre o teclado da
// Samsung para usar o microfone dele. No .wgt o campo tambem abre o IME para
// digitar. O ajuste "Botao de microfone na busca" (texto_sistema_mic_modo) decide
// com o que a TV respondeu: Automatico so esconde na TV que diz NAO ter
// microfone.
#define SAMSUNG_MIC 1
static int vModo = TS_MIC_AUTO;
static int micPermitido(void) {
  int tem, mic;
#ifdef NV_TPK
  tem = vIniciar != NULL; mic = -1; // #585: TV microphone capability does not detect the remote's mic.
#else
  // O shell responde uma vez (o <input> e o getCapability nao mudam).
  static int wTem = -1, wMic = -1;
  if (wTem < 0) {
    wTem = EM_ASM_INT({ return (typeof window !== "undefined" && window.nvTexto) ? 1 : 0; });
    if (wTem) wMic = EM_ASM_INT({ var m = window.nvTexto.microfone; return m === 0 || m === 1 ? m : -1; });
    printf("[texto] wgt: botao de microfone %s (microfone=%d)\n", wTem ? "possivel" : "sem campo", wMic);
    fflush(stdout);
  }
  tem = wTem; mic = wMic;
#endif
  if (!tem || vModo == TS_MIC_DESLIGADO) return 0;
  return vModo == TS_MIC_LIGADO || mic != 0;
}
#endif

static void copiar(char *dst, const char *src) {
  size_t n = 0;
  if (!src) src = "";
  while (src[n] && n + 1 < TS_MAX) n++;
  // nao deixa meia sequencia UTF-8 no fim do corte
  if (src[n]) while (n > 0 && ((unsigned char)src[n] & 0xC0) == 0x80) n--;
  memcpy(dst, src, n);
  dst[n] = 0;
}

Uint32 texto_sistema_evento(void) {
  if (!tipoEv) {
    Uint32 t = SDL_RegisterEvents(1);
    tipoEv = t == (Uint32)-1 ? SDL_USEREVENT : t;
  }
  return tipoEv;
}

static void avisar(int codigo, int confirmou) {
  SDL_Event ev;
  SDL_zero(ev);
  ev.type = texto_sistema_evento();
  ev.user.code = codigo;
  ev.user.data1 = confirmou ? (void *)1 : NULL;
  SDL_PushEvent(&ev);
}

static void terminar(int confirmou, const char *porque) {
  if (!aberto) return;
  aberto = 0;
  printf("[texto] teclado do sistema fechou (%s%s)\n", porque, confirmou ? ", confirmou" : "");
  fflush(stdout);
  avisar(confirmou < 0 ? TS_EV_ERRO : TS_EV_FIM, confirmou > 0);
}

int texto_sistema_disponivel(void) {
#if defined(NV_TPK)
  return (hAbrir ? (TS_TECLADO | (hFlags & TS_VOZ)) : 0) | (micPermitido() ? TS_VOZ : 0);
#elif defined(__EMSCRIPTEN__)
  return (EM_ASM_INT({ return (typeof window !== "undefined" && window.nvTexto) ? 1 : 0; }) ? TS_TECLADO : 0)
       | (micPermitido() ? TS_VOZ : 0);
#elif defined(NV_ANDROID) || defined(NV_TEXTO_SDL_TESTE)
  return TS_TECLADO;
#elif defined(__linux__)
  return SDL_HasScreenKeyboardSupport() ? TS_TECLADO : 0;
#else
  return 0;
#endif
}

int texto_sistema_aberto(void) { return aberto; }

void texto_sistema_mic_modo(int modo) {
#ifdef SAMSUNG_MIC
  if (modo == vModo) return;
  vModo = modo;
  printf("[texto] botao de microfone: ajuste %d\n", modo);
  fflush(stdout);
#else
  (void)modo;
#endif
}

int texto_sistema_voz_pelo_teclado(void) {
#if defined(NV_TPK)
  return !hAbrir && micPermitido();
#elif defined(__EMSCRIPTEN__)
  return micPermitido();
#else
  return 0;
#endif
}

const char *texto_sistema_valor(void) {
  memcpy(lido, valor, sizeof lido);
  return lido;
}

void texto_sistema_abrir(const char *atual, int voz) {
  int tem = texto_sistema_disponivel(), peloMic = 0;
#if defined(NV_TPK)
  // Botao de microfone (Voz.cs): a voz e o teclado da Samsung, sem o teclado
  // do sistema registrado para digitar.
  peloMic = voz && (tem & TS_VOZ) && !hAbrir;
#elif defined(__EMSCRIPTEN__)
  peloMic = voz && (tem & TS_VOZ);
#endif
  if (!peloMic && !(tem & TS_TECLADO)) return;
  if (!(tem & TS_VOZ)) voz = 0;
  copiar(valor, atual);
  SDL_AtomicLock(&trava); pendTemValor = 0; pendFim = -1; SDL_AtomicUnlock(&trava);
  aberto = 1; viuTeclado = 0; quadrosAberto = 0;
  printf("[texto] abrindo o teclado do sistema%s (%d bytes)\n",
         peloMic ? " pelo botao de microfone" : voz ? " com ditado" : "", (int)strlen(valor));
  fflush(stdout);
#if defined(NV_TPK)
  abertoPeloMic = peloMic;
  if (peloMic) vIniciar(); else hAbrir(valor, voz);
#elif defined(__EMSCRIPTEN__)
  EM_ASM({ try { window.nvTexto.abrir(UTF8ToString($0), $1); } catch (e) { console.warn("[texto] " + e); } }, valor, voz);
#else
  SDL_StartTextInput();
#endif
}

void texto_sistema_fechar(void) {
  if (!aberto) return;
#if defined(NV_TPK)
  if (abertoPeloMic) vParar();
  else if (hFechar) hFechar();
#elif defined(__EMSCRIPTEN__)
  EM_ASM({ try { window.nvTexto.fechar(); } catch (e) {} });
#else
  SDL_StopTextInput();
#endif
  terminar(0, "app");
}

#if !defined(NV_TPK) && !defined(__EMSCRIPTEN__)
// Apaga o ultimo caractere UTF-8.
static void apagarUltimo(void) {
  size_t n = strlen(valor);
  while (n > 0) {
    n--;
    if (((unsigned char)valor[n] & 0xC0) != 0x80) break;
  }
  valor[n] = 0;
}
#endif

static int teclaDeTexto(SDL_Keycode k) {
  return k == SDLK_BACKSPACE || (k >= 32 && k < 127);
}

int texto_sistema_engole(const SDL_Event *e) {
  if (!aberto) return 0;
  if (e->type == SDL_TEXTINPUT || e->type == SDL_TEXTEDITING) return 1;
  if (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) return teclaDeTexto(e->key.keysym.sym);
  return 0;
}

void texto_sistema_observar(const SDL_Event *e) {
  if (!aberto) return;
#if !defined(NV_TPK) && !defined(__EMSCRIPTEN__)
  // LG e Android: o texto vem pelos eventos do SDL.
  if (e->type == SDL_TEXTINPUT) {
    size_t n = strlen(valor), m = strlen(e->text.text);
    printf("[texto] TEXTINPUT %d bytes\n", (int)m);
    if (n + m < TS_MAX) { memcpy(valor + n, e->text.text, m + 1); avisar(TS_EV_TEXTO, 0); }
    return;
  }
  if (e->type == SDL_TEXTEDITING) {
    printf("[texto] TEXTEDITING %d bytes start=%d len=%d\n", (int)strlen(e->edit.text), e->edit.start, e->edit.length);
    return;
  }
  if (e->type == SDL_KEYDOWN) {
    SDL_Keycode k = e->key.keysym.sym;
    printf("[texto] KEYDOWN sym=%d scancode=%d\n", (int)k, (int)e->key.keysym.scancode);
    if (k == SDLK_BACKSPACE) { apagarUltimo(); avisar(TS_EV_TEXTO, 0); }
    // Letra/digito ASCII como KEYDOWN SEM o TEXTINPUT par: so o Android faz
    // os dois, e la o TEXTINPUT tambem chega, entao nada a acrescentar aqui.
    return;
  }
#else
  (void)e;
#endif
}

void texto_sistema_quadro(void) {
  int temValor, fim;
  char v[TS_MAX];
  if (!aberto) return;
  quadrosAberto++;
#ifdef __EMSCRIPTEN__
  {
    // window.nvTexto.seq sobe a cada mudanca; .fim e -1 (aberto), 0 ou 1.
    static int seqVisto = -1;
    int seq = EM_ASM_INT({ var t = window.nvTexto; return t ? t.seq : -1; });
    if (seq >= 0 && seq != seqVisto) {
      seqVisto = seq;
      EM_ASM({ var t = window.nvTexto; stringToUTF8(t ? String(t.valor) : "", $0, $1); }, v, (int)sizeof v);
      texto_sistema_ponte_valor(v);
    }
    fim = EM_ASM_INT({ var t = window.nvTexto; return t ? t.fim : -1; });
    if (fim >= 0) { EM_ASM({ window.nvTexto.fim = -1; }); texto_sistema_ponte_fim(fim == 2 ? -1 : fim); }
  }
#endif
  SDL_AtomicLock(&trava);
  temValor = pendTemValor; pendTemValor = 0;
  if (temValor) memcpy(v, pendValor, sizeof v);
  fim = pendFim; pendFim = -1;
  SDL_AtomicUnlock(&trava);
  if (temValor && strcmp(v, valor)) {
    memcpy(valor, v, sizeof valor);
    printf("[texto] valor do sistema: %d bytes\n", (int)strlen(valor));
    avisar(TS_EV_TEXTO, 0);
  }
  if (fim >= 0) { terminar(fim == 2 ? -1 : fim, "system"); return; }
#if !defined(NV_TPK) && !defined(__EMSCRIPTEN__) && defined(__linux__)
  // LG/Android: fechou por fora (Voltar no teclado da TV)? So depois de o
  // teclado ter sido visto aberto, e com folga para ele subir.
  {
    SDL_Window *w = SDL_GetKeyboardFocus();
    int mostra = w ? (int)SDL_IsScreenKeyboardShown(w) : 0;
    if (mostra && !viuTeclado) {
      viuTeclado = 1;
      printf("[texto] teclado do sistema na tela (quadro %d)\n", quadrosAberto);
      fflush(stdout);
    }
    if (viuTeclado && !mostra) { SDL_StopTextInput(); terminar(0, "osk-oculto"); }
    else if (!viuTeclado && quadrosAberto == 180) {
      printf("[texto] 180 quadros e SDL_IsScreenKeyboardShown ainda 0 (janela=%p)\n", (void *)w);
      fflush(stdout);
    }
  }
#endif
}

void texto_sistema_ponte_valor(const char *v) {
  SDL_AtomicLock(&trava);
  copiar(pendValor, v);
  pendTemValor = 1;
  SDL_AtomicUnlock(&trava);
}

void texto_sistema_ponte_fim(int confirmou) {
  SDL_AtomicLock(&trava);
  pendFim = confirmou < 0 ? 2 : confirmou ? 1 : 0;
  SDL_AtomicUnlock(&trava);
}
