// Caminho do SDL (LG e Android) de src/entrada_texto.c, com os eventos que a
// C9 MEDIU entregar (01/10/2026): TEXTINPUT com a palavra inteira, sem KEYDOWN
// de letra, e Backspace como KEYDOWN. NV_TEXTO_SDL_TESTE liga esse caminho no Mac.
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <pthread.h>
#include "../src/entrada_texto.h"

void texto_sistema_teste_reset(void);

// Versao de webOS que o modulo enxerga no teste (imeSemOskMata).
static int tMajor = 26;
int texto_teste_webos_major(void) { return tMajor; }

// "queda.c" do teste: o tratador que ja existia quando o modulo instala o dele.
// Uma falha em OUTRO fio tem de chegar aqui pelo encadeamento, nao saltar para
// a pilha do fio principal.
static sigjmp_buf saltaQueda;
static int quedaChamou;
static void quedaFake(int s, siginfo_t *si, void *ctx) {
  (void)s; (void)si; (void)ctx;
  quedaChamou++;
  siglongjmp(saltaQueda, 1);
}
static void *fioQueCai(void *p) {
  (void)p;
  if (sigsetjmp(saltaQueda, 1) == 0) raise(SIGSEGV);
  return NULL;
}
void texto_teste_crash_outro(void) {
  pthread_t t;
  pthread_create(&t, NULL, fioQueCai, NULL);
  pthread_join(t, NULL);
}

static int falhas;
#define OK(c, m) do { if (c) printf("ok - %s\n", m); else { printf("FALHOU - %s\n", m); falhas++; } } while (0)

static char valor[300];
static int nTexto, nFim;

static void enviar(const SDL_Event *e) {
  // o laco do main.c: observar primeiro, depois o app
  texto_sistema_observar(e);
}
static void quadro(void) {
  SDL_Event e;
  texto_sistema_quadro();
  while (SDL_PollEvent(&e)) {
    if (e.type == texto_sistema_evento()) {
      if (e.user.code == TS_EV_TEXTO) { nTexto++; snprintf(valor, sizeof valor, "%s", texto_sistema_valor()); }
      if (e.user.code == TS_EV_FIM) nFim++;
    }
  }
}
static void texto(const char *t) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_TEXTINPUT; snprintf(e.text.text, sizeof e.text.text, "%s", t);
  enviar(&e);
}
static SDL_Event tecla(SDL_Keycode k) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  return e;
}

int main(void) {
  SDL_Event e;
  SDL_Init(SDL_INIT_EVENTS);
  OK(texto_sistema_disponivel() == TS_TECLADO, "disponivel = TS_TECLADO, sem TS_VOZ");
  e = tecla(SDLK_a);
  OK(!texto_sistema_engole(&e), "fechado nao engole nada");

  texto_sistema_abrir("ma", 1);
  OK(texto_sistema_aberto(), "abre (pedir voz sem ditado abre so o teclado)");
  texto("trix");
  quadro();
  OK(!strcmp(valor, "matrix") && nTexto == 1, "TEXTINPUT soma ao texto atual");
  e = tecla(SDLK_BACKSPACE); enviar(&e);
  quadro();
  OK(!strcmp(valor, "matri"), "Backspace apaga um caractere");
  texto("x ç");
  e = tecla(SDLK_BACKSPACE); enviar(&e);
  quadro();
  OK(!strcmp(valor, "matrix "), "Backspace apaga o caractere UTF-8 inteiro");

  e = tecla(SDLK_a);       OK(texto_sistema_engole(&e), "engole letra (o IME ja a pos no valor)");
  e = tecla(SDLK_BACKSPACE); OK(texto_sistema_engole(&e), "engole Backspace (nao fecha o Spotlight vazio)");
  e = tecla(SDLK_DOWN);    OK(!texto_sistema_engole(&e), "nao engole seta");
  e = tecla(SDLK_RETURN);  OK(!texto_sistema_engole(&e), "nao engole OK");
  e = tecla(SDLK_AC_BACK); OK(!texto_sistema_engole(&e), "nao engole Voltar");
  SDL_zero(e); e.type = SDL_TEXTINPUT; OK(texto_sistema_engole(&e), "engole TEXTINPUT");

  { char longo[400]; int i; texto_sistema_fechar(); quadro(); nFim = 0;
    for (i = 0; i < 399; i++) longo[i] = 'a'; longo[399] = 0;
    texto_sistema_abrir(longo, 0);
    OK(!strcmp(texto_sistema_valor(), longo), "current text retains input beyond 255 bytes"); }
  texto_sistema_fechar();
  quadro();
  OK(nFim == 1 && !texto_sistema_aberto(), "fechar avisa o fim uma vez");
  texto_sistema_fechar();
  quadro();
  OK(nFim == 1, "fechar de novo nao repete o aviso");

  texto_sistema_ponte_valor("ponte");
  texto_sistema_abrir("", 0);
  quadro();
  OK(nTexto == 3 || strcmp(valor, "ponte"), "abrir descarta pendencia velha da ponte");
  texto_sistema_ponte_valor("de fora");
  texto_sistema_ponte_fim(1);
  quadro();
  OK(!strcmp(valor, "de fora") && nFim == 2 && !texto_sistema_aberto(), "ponte (host/JS) entrega valor e fim");

  // #642: o IME do sistema derruba o processo (webOS 26). Com NUVIO_TEXTO_CRASH=1
  // o StartTextInput de teste faz raise(SIGSEGV): o modulo tem de sobreviver,
  // avisar ERRO e ficar indisponivel -> a grade do app assume.
  {
    int nErro = 0;
    texto_sistema_teste_reset();
    setenv("NUVIO_TEXTO_CRASH", "1", 1);
    texto_sistema_abrir("x", 0);
    unsetenv("NUVIO_TEXTO_CRASH");
    OK(!texto_sistema_aberto(), "crash no IME nao derruba o app nem deixa aberto");
    { SDL_Event e; while (SDL_PollEvent(&e))
        if (e.type == texto_sistema_evento() && e.user.code == TS_EV_ERRO) nErro++; }
    OK(nErro == 1, "crash no IME vira TS_EV_ERRO (uma vez)");
    OK(texto_sistema_disponivel() == 0, "depois do crash o sistema fica indisponivel");
    texto_sistema_abrir("y", 0);
    OK(!texto_sistema_aberto(), "abrir de novo nem tenta o IME quebrado");
    // sem o teclado aberto, a sessao segue viva com o teclado do app
    texto_sistema_teste_reset();
    texto_sistema_abrir("z", 0);
    OK(texto_sistema_aberto(), "sessao nova (reset) volta a oferecer o sistema");
    texto_sistema_fechar();
    quadro();
  }

  // #642 rev: uma falha em OUTRO fio nao pode saltar para a pilha do
  // principal. O tratador anterior (aqui o quedaFake) e quem recebe a queda.
  {
    struct sigaction sa;
    quedaChamou = 0;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = quedaFake;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    texto_sistema_teste_reset();
    setenv("NUVIO_TEXTO_CRASH", "2", 1);
    texto_sistema_abrir("x", 0);
    unsetenv("NUVIO_TEXTO_CRASH");
    sigaction(SIGSEGV, &(struct sigaction){ .sa_handler = SIG_DFL }, NULL);
    OK(quedaChamou == 1, "falha em outro fio cai no tratador anterior");
    OK(texto_sistema_aberto(), "falha em outro fio nao quebra a sessao do principal");
    OK(texto_sistema_disponivel() == TS_TECLADO, "falha em outro fio nao condena o IME");
    texto_sistema_fechar();
    quadro();
  }

  // #642 rev: 180 quadros sem o OSK. No webOS 26 condena o IME; nas antigas
  // so derruba a sessao (o teclado delas pode so demorar a subir).
  {
    int i, nErro = 0;
    SDL_Event e;
    texto_sistema_teste_reset();
    tMajor = 4;
    texto_sistema_abrir("x", 0);
    for (i = 0; i < 181; i++) texto_sistema_quadro();
    while (SDL_PollEvent(&e))
      if (e.type == texto_sistema_evento() && e.user.code == TS_EV_ERRO) nErro++;
    OK(nErro == 1 && !texto_sistema_aberto(), "webOS antigo: 180q sem OSK fecha a sessao com erro");
    OK(texto_sistema_disponivel() == TS_TECLADO, "webOS antigo: teclado do sistema NAO fica condenado");

    nErro = 0;
    texto_sistema_teste_reset();
    tMajor = 26;
    texto_sistema_abrir("x", 0);
    for (i = 0; i < 181; i++) texto_sistema_quadro();
    while (SDL_PollEvent(&e))
      if (e.type == texto_sistema_evento() && e.user.code == TS_EV_ERRO) nErro++;
    OK(nErro == 1 && !texto_sistema_aberto(), "webOS 26: 180q sem OSK fecha a sessao com erro");
    OK(texto_sistema_disponivel() == 0, "webOS 26: IME fica condenado e o app assume");
    texto_sistema_teste_reset();
  }

  SDL_Quit();
  return falhas ? 1 : 0;
}
