// Ponte do alvo .tpk da Samsung. Ver o porque em tpk.h.
//
// O mesmo protocolo serve aos dois hosts: no Tizen 6+ quem chama
// nv_tpk_quadro e o render do GLWindow (NUI); no 4/5 e o OnUpdate da
// TVGLApplication da Samsung (Tizen.NET.TV), o molde do JuvoPlayer.OpenGL.
// Nos dois o contexto esta corrente quando a chamada chega, e quem troca os
// buffers e o framework, depois que ela volta.
//
// Dois fios dividem UM contexto EGL, o do GLWindow do host .NET:
//   - o fio de desenho do NUI chama nv_tpk_quadro() com o contexto corrente,
//     solta o contexto, passa a vez ao app e espera ele devolver;
//   - o fio do app roda o main() de sempre; quando o laco chega no
//     SDL_GL_SwapWindow (tpk_gl_trocar) ele solta o contexto e devolve a vez.
// Quem troca os buffers e o GLWindow, depois que nv_tpk_quadro retorna. Um
// contexto EGL so pode estar corrente num fio por vez; a vez (`vez`) garante
// isso sem que nenhum dos lados precise saber o que o outro faz.
#ifdef NV_TPK
#include "tpkdesp.h"
#include "tpkteclas.h"
#include "layout.h"
#include <SDL2/SDL.h>
#include "tpk_egl.h"
#include <GLES2/gl2.h>
#include <time.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <locale.h>
#include "video.h"
#include "player.h"
#include "cdnlink.h"
#include "streams.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv);

// Shared startup trace: persist native steps for every TPK host.
// ---------------------------------------------------------------- RASTRO
// Ver tpk.h. Caminho definido em nv_tpk_iniciar (data/tpk-etapas.txt); o host
// .NET escreve no mesmo arquivo com O_APPEND, e cada linha e UM write, entao
// as duas fontes nao se misturam. Sem stdio: se o processo morrer logo depois,
// a linha ja esta no disco. Tambem vai para o nuvio.log (printf), que e o que
// o envio automatico sobe quando o login funcionar.
static char etapasArq[600], etapasAnt[600];
void nv_tpk40_etapa(const char *linha) {
  char buf[700];
  int fd, n;
  if (!etapasArq[0] || !linha) return;
  n = snprintf(buf, sizeof buf, "native %s\n", linha);
  if (n <= 0) return;
  if ((size_t)n >= sizeof buf) n = (int)sizeof buf - 1;
  fd = open(etapasArq, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd >= 0) { (void)!write(fd, buf, (size_t)n); close(fd); }
  printf("[etapa] %s", buf);
  fflush(stdout);
}
// O rastro do arranque ANTERIOR (renomeado pelo host) entra no nuvio.log desta
// execucao, para subir com o envio automatico. Chamado do fio do app depois
// que o main() ja redirecionou o stdout para o log.
static void etapasAnteriorParaLog(void) {
  static int feito;
  FILE *f;
  char l[400];
  if (feito || !etapasAnt[0]) return;
  feito = 1;
  f = fopen(etapasAnt, "r");
  if (!f) return;
  while (fgets(l, sizeof l, f)) printf("[etapa-anterior] %s", l);
  fclose(f);
  fflush(stdout);
}

static char erro[512];
TpkEgl tpkEgl;
int tpk_egl_carregar(void) {
  static const char *const nomes[] = { "libEGL.so.1", "libEGL.so", "libEGL.so.1.4", NULL };
  void *h = NULL;
  int i;
  for (i = 0; nomes[i] && !h; i++) h = dlopen(nomes[i], RTLD_NOW | RTLD_GLOBAL);
  if (!h) { snprintf(erro, sizeof erro, "libEGL not found: %s", dlerror()); return -1; }
#define PEGA(c, n) *(void **)&tpkEgl.c = dlsym(h, n)
  PEGA(GetCurrentDisplay, "eglGetCurrentDisplay"); PEGA(GetCurrentSurface, "eglGetCurrentSurface");
  PEGA(GetCurrentContext, "eglGetCurrentContext"); PEGA(GetError, "eglGetError");
  PEGA(QueryContext, "eglQueryContext"); PEGA(QuerySurface, "eglQuerySurface");
  PEGA(ChooseConfig, "eglChooseConfig");
  PEGA(GetConfigAttrib, "eglGetConfigAttrib"); PEGA(GetDisplay, "eglGetDisplay");
  PEGA(Initialize, "eglInitialize"); PEGA(BindAPI, "eglBindAPI");
  PEGA(CreateWindowSurface, "eglCreateWindowSurface"); PEGA(CreateContext, "eglCreateContext");
  PEGA(SwapInterval, "eglSwapInterval"); PEGA(SwapBuffers, "eglSwapBuffers");
  PEGA(MakeCurrent, "eglMakeCurrent"); PEGA(GetProcAddress, "eglGetProcAddress");
#undef PEGA
  // These calls are unconditional in the context handoff and GL diagnostics.
#define REQUIRED(c, n) if (!tpkEgl.c) { snprintf(erro, sizeof erro, "libEGL missing %s", n); return -1; }
  REQUIRED(GetCurrentDisplay, "eglGetCurrentDisplay");
  REQUIRED(GetCurrentSurface, "eglGetCurrentSurface");
  REQUIRED(GetCurrentContext, "eglGetCurrentContext");
  REQUIRED(MakeCurrent, "eglMakeCurrent");
  REQUIRED(GetError, "eglGetError");
  REQUIRED(QueryContext, "eglQueryContext");
  REQUIRED(ChooseConfig, "eglChooseConfig");
  REQUIRED(GetConfigAttrib, "eglGetConfigAttrib");
#undef REQUIRED
  erro[0] = 0;
  return 0;
}

// A VEZ. Quem tem a vez pode ter o contexto corrente; o outro nao toca em GL.
//   vez == VEZ_NUI: o fio do framework (render do GLWindow, ou o OnUpdate da
//                   TVGLApplication) segura o contexto;
//   vez == VEZ_APP: o fio do app esta desenhando um quadro.
// `pendente` = o app entregou um quadro que ainda nao foi trocado na tela.
// `appEsperando` = o app esta parado em tpk_gl_criar/tpk_gl_trocar esperando
// a vez; so entao vale a pena entregar o contexto.
enum { VEZ_NUI = 0, VEZ_APP = 1 };
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  mudou = PTHREAD_COND_INITIALIZER;
static int vez = VEZ_NUI, iniciado, terminou, pendente, appEsperando, nuiCorrente = 1;
static EGLDisplay dpy = EGL_NO_DISPLAY;
static EGLSurface sup = EGL_NO_SURFACE;
static EGLContext ctx = EGL_NO_CONTEXT;
static int telaW = 1920, telaH = 1080;
static int saidaPedida;
void tpk_pedir_saida(int w, int h) {
  // Menor que 1080 (o "720p leve") e pedido ao host tambem; sem suporte o
  // host devolve 1920 e o codigo interno em 720 cobre a diferenca.
  __atomic_store_n(&saidaPedida, w >= 3840 && h >= 2160 ? 3840 :
                                  w <= 1280 && h <= 720 ? 1280 : 1920,
                   __ATOMIC_RELEASE);
}
// Read by the host before constructing GLWindow; zero means startup is pending.
__attribute__((visibility("default")))
int nv_tpk_saida_pedida(void) { return __atomic_load_n(&saidaPedida, __ATOMIC_ACQUIRE); }
static char dirArte[512];
// Configuracao do host (nv_tpk_config): quanto o framework espera o app num
// quadro (ms; <0 = ate ele terminar) e se desliga o vsync do eglSwapBuffers.
static int esperaMs = 50, swapZero;
static unsigned nQuadros, nPulados;
static int contextoFalhou; // Render-thread only; do not poll the app thread's error buffer.

// Fio do app: devolve a vez ao framework e espera ela voltar.
static int cicloPendente;
static void appEsperaVez(int entregou) {
  pthread_mutex_lock(&trava);
  if (entregou) { vez = VEZ_NUI; pendente = 1; pthread_cond_broadcast(&mudou); }
  appEsperando = 1;
  pthread_cond_broadcast(&mudou);
  while (vez != VEZ_APP || cicloPendente) {
    if (cicloPendente) {
      Uint32 tipo = cicloPendente == 2 ? SDL_APP_LOWMEMORY : SDL_APP_WILLENTERBACKGROUND;
      cicloPendente = 0;
      pthread_mutex_unlock(&trava);
      player_ciclo_evento(tipo);
      pthread_mutex_lock(&trava);
      pthread_cond_broadcast(&mudou);
    } else pthread_cond_wait(&mudou, &trava);
  }
  appEsperando = 0;
  pthread_mutex_unlock(&trava);
}

static void *fioApp(void *arg) {
  char *argv[] = { "nuvio", dirArte, NULL };
  int r;
  (void)arg;
  nv_tpk40_etapa("note app-main started");
  r = main(2, argv);
  nv_tpk40_etapa("note app-main returned");
  printf("[tpk] main devolveu %d\n", r);
  fflush(stdout);
  pthread_mutex_lock(&trava);
  if (vez == VEZ_APP) eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  terminou = 1;
  vez = VEZ_NUI;
  pthread_cond_broadcast(&mudou);
  pthread_mutex_unlock(&trava);
  return NULL;
}

// Chamada uma vez pelo host, antes do primeiro quadro. Os caminhos vem do
// Tizen.Applications (res/ e data/ do pacote): no .tpk nao ha HOME util nem
// pasta ao lado do executavel.
__attribute__((visibility("default")))
int nv_tpk_iniciar(const char *arte, const char *dados, int w, int h) {
  pthread_t t;
  pthread_attr_t at;
  char log[600];
  if (iniciado) return 0;
  iniciado = 1;
  // O locale do host (.NET, idioma da TV) chega aqui ja aplicado; o main()
  // volta o numerico para o C, e isto cobre o que roda antes dele.
  setlocale(LC_NUMERIC, "C");
  snprintf(etapasArq, sizeof etapasArq, "%s/tpk-etapas.txt", dados);
  snprintf(etapasAnt, sizeof etapasAnt, "%s/tpk-etapas-anterior.txt", dados);
  nv_tpk40_etapa("begin egl-load");
  if (tpk_egl_carregar() != 0) { nv_tpk40_etapa(erro); nv_tpk40_etapa("fail egl-load"); return -1; }
  nv_tpk40_etapa("ok egl-load");
  if (w > 0 && h > 0) { telaW = w; telaH = h; }
  snprintf(dirArte, sizeof dirArte, "%s", arte);
  // <pacote>/res/art: o motor P2P (p2pmotor_motor.c) acha a libnuvio_engine.so
  // em <pacote>/lib mesmo quando esta libnuvio.so e a encenada em data/.
  setenv("NUVIO_TPK_ARTE", arte, 1);
  snprintf(log, sizeof log, "%s/nuvio.log", dados);
  { char ant[600];
    snprintf(ant, sizeof ant, "%s/nuvio-anterior.log", dados);
    setenv("NUVIO_LOG_ANTERIOR", ant, 1); }
  setenv("NUVIO_DADOS", dados, 1);
  tpkdesp_iniciar(dados);   // saida pela TV != queda (tpkdesp.h)
  // LINK DO CDN (cdnlink.h): o player da TV recebe o endereco final no CDN; a
  // verificacao das fontes guarda o que ja resolveu.
  { static const CdnlinkGanchos g = { cdnlink_origem_ok, cdnlink_recente, cdnlink_resolver, cdnlink_validade_ms };
    video_tpk_cdn_ganchos(&g);
    stream_cdn_lembrar = cdnlink_lembrar; }
  setenv("NUVIO_LOG", log, 1);
  setenv("HOME", dados, 1);
  setenv("SDL_VIDEODRIVER", "dummy", 1);
  nv_tpk40_etapa("ok native-environment");
  pthread_attr_init(&at);
  // O main() e o laco usam pilha como um processo normal (buffers de 4-8 KB
  // aos montes); o padrao de fio do Tizen e 8 MB mas nao e garantido.
  pthread_attr_setstacksize(&at, 8 << 20);
  nv_tpk40_etapa("begin app-thread");
  int rc = pthread_create(&t, &at, fioApp, NULL);
  pthread_attr_destroy(&at);
  if (rc != 0) {
    snprintf(erro, sizeof erro, "pthread_create failed: %d", rc);
    nv_tpk40_etapa(erro); nv_tpk40_etapa("fail app-thread"); return -1;
  }
  nv_tpk40_etapa("ok app-thread");
  pthread_detach(t);
  return 0;
}

// Linha do host (.NET) no mesmo nuvio.log do app: e o log que o painel da TV
// (tecla vermelha) mostra e o que o envio automatico sobe.
__attribute__((visibility("default")))
void nv_tpk_log(const char *linha) {
  printf("[host] %s\n", linha ? linha : "");
  tpkdesp_linha(linha);
  video_tpk_log_host(linha);
  fflush(stdout);
}

// 1 depois que o main() do app devolveu: o host fecha.
__attribute__((visibility("default")))
int nv_tpk_terminou(void) { return terminou; }

// Motivo de o app nao ter subido, para o host por na tela ("" = nenhum).
__attribute__((visibility("default")))
const char *nv_tpk_erro(void) { return erro; }

// Host, antes do primeiro quadro. esperaMs: quanto cada quadro do framework
// espera o app (<0 = sem limite, para o GLWindow da API8, que troca os
// buffers mesmo quando o quadro e pulado). swapZero: eglSwapInterval(0) —
// no Tizen 9 o swap com intervalo 1 espera o vblank do TDM e o GLWindow ja
// dorme ate 60 Hz sozinho (dali-adaptor gl-window-render-thread.cpp).
__attribute__((visibility("default")))
void nv_tpk_config(int espera, int zero) { esperaMs = espera; swapZero = zero; }

// Fio do framework, contexto dele corrente (ou nao, se o app ficou com a vez
// no quadro anterior). Devolve 1 = troque os buffers, 0 = pule este quadro
// (nao houve desenho novo), -1 = o app acabou, feche.
//
// Nunca espera mais que esperaMs: no Tizen 6.0 este callback roda no fio
// PRINCIPAL do NUI (AddIdle, gl-window-impl.cpp da 6.0), e segurar aqui
// seguraria teclas, relogio e eventos do player. Se o app nao terminou o
// quadro a tempo, o contexto fica com ele e o quadro seguinte do framework
// pula ate ele devolver.
__attribute__((visibility("default")))
int nv_tpk_quadro(void) {
  int r;
  if (contextoFalhou) return -1;
  if (ctx == EGL_NO_CONTEXT) {
    dpy = eglGetCurrentDisplay();
    sup = eglGetCurrentSurface(EGL_DRAW);
    ctx = eglGetCurrentContext();
    if (dpy == EGL_NO_DISPLAY || sup == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT) {
      contextoFalhou = 1;
      snprintf(erro, sizeof erro, "No current EGL display/surface/context (error 0x%x)", eglGetError());
      nv_tpk40_etapa(erro); nv_tpk40_etapa("fail egl-context");
      return -1;
    }
    nv_tpk40_etapa("ok egl-context");
    if (swapZero && tpkEgl.SwapInterval) tpkEgl.SwapInterval(dpy, 0);
    printf("[tpk] contexto do framework: dpy=%p sup=%p ctx=%p espera=%d swap0=%d\n",
           dpy, sup, ctx, esperaMs, swapZero);
    // A linha do FURO: quantos bits de alfa o EGL entregou de fato nesta
    // janela. A=0 -> o chuvisco do furo nao tem como virar transparencia e o
    // plano de video nunca aparece (relato: "chuvisco" por cima do video na
    // Tizen 4.0). E o atalho para a pergunta "o host pediu alfa?".
    { EGLint id = 0;
      EGLConfig cfg;
      EGLint n = 0, pede[] = { EGL_CONFIG_ID, 0, EGL_NONE };
      eglQueryContext(dpy, ctx, EGL_CONFIG_ID, &id);
      pede[1] = id;
      if (eglChooseConfig(dpy, pede, &cfg, 1, &n) && n == 1) {
        EGLint R = -1, G = -1, B = -1, A = -1, surf = -1;
        eglGetConfigAttrib(dpy, cfg, EGL_RED_SIZE, &R);
        eglGetConfigAttrib(dpy, cfg, EGL_GREEN_SIZE, &G);
        eglGetConfigAttrib(dpy, cfg, EGL_BLUE_SIZE, &B);
        eglGetConfigAttrib(dpy, cfg, EGL_ALPHA_SIZE, &A);
        eglGetConfigAttrib(dpy, cfg, EGL_SURFACE_TYPE, &surf);
        printf("[tpk] egl config id=%d R%d G%d B%d A%d surf=0x%x%s\n",
               (int)id, (int)R, (int)G, (int)B, (int)A, (int)surf,
               A > 0 ? "" : "  <<< SEM ALFA: o furo nao revela o video");
      } else printf("[tpk] egl config id=%d nao encontrada pelo ChooseConfig\n", (int)id);
      { GLint ga = -1;
        glGetIntegerv(GL_ALPHA_BITS, &ga);
        printf("[tpk] framebuffer GL alpha bits=%d%s\n", (int)ga,
               ga > 0 ? "" : "  <<< drawable sem alfa"); }
      // O desenho do app cobre telaW x telaH (o que o host disse). Se a
      // superficie real e MAIOR, o resto fica com o que o buffer trouxer — o
      // "chuvisco" por cima do video da Tizen 4.0.
      if (tpkEgl.QuerySurface) {
        EGLint sw = 0, sh = 0;
        eglQuerySurface(dpy, sup, EGL_WIDTH, &sw);
        eglQuerySurface(dpy, sup, EGL_HEIGHT, &sh);
        printf("[tpk] superficie %dx%d (app desenha %dx%d)%s\n",
               (int)sw, (int)sh, telaW, telaH,
               (sw != telaW || sh != telaH) ? "  <<< TAMANHO ERRADO" : "");
        // O app ainda nao leu o tamanho (ele so ganha a vez depois deste
        // quadro): adota o da superficie. Sem isto, numa superficie maior o
        // app desenha so um canto e o resto mostra o que o buffer trouxe —
        // o "chuvisco" por cima do video da Tizen 4.0. O plano de video e
        // posicionado em pixels da superficie (video_escala_definir): errado
        // tambem fica o retangulo dele.
        // 1x1 = janela do host ainda sem tamanho no primeiro quadro (Tizen 9,
        // 2.0.7: tela preta, reabrir as vezes resolve). Fica o tamanho do host.
        if (sw >= 640 && sh >= 360 && ((int)sw != telaW || (int)sh != telaH)) {
          telaW = (int)sw; telaH = (int)sh;
          printf("[tpk] tela adotada da superficie: %dx%d\n", telaW, telaH);
        }
      } else printf("[tpk] eglQuerySurface indisponivel\n");
    }
    fflush(stdout);
  }
  pthread_mutex_lock(&trava);
  if (vez == VEZ_APP) {                 // o app ainda desenha o quadro anterior
    r = terminou ? -1 : 0;
    nPulados++;
    pthread_mutex_unlock(&trava);
    return r;
  }
  if (!nuiCorrente) { eglMakeCurrent(dpy, sup, sup, ctx); nuiCorrente = 1; }
  if (pendente) { pendente = 0; pthread_mutex_unlock(&trava); nQuadros++; return 1; }
  if (terminou) { pthread_mutex_unlock(&trava); return -1; }
  if (!appEsperando) {
    // Arranque: o app ainda nao chegou ao primeiro quadro. No GLWindow da API8
    // (que troca de qualquer jeito) limpa para preto em vez de mostrar lixo.
    pthread_mutex_unlock(&trava);
    // API8 swaps even on return 0. A black clear is not an app frame: keep
    // the host's first-frame watchdog armed until the app actually draws.
    if (esperaMs < 0) {
#ifdef NV_TPK40
      // A janela GL do 4/5 e translucida e o plano do player pode ja estar no
      // ar (o trailer da home sai antes do primeiro quadro do app): um clear
      // opaco tamparia o video ate o app desenhar. Alfa 0: o que houver atras
      // segue visivel, e nunca lixo do buffer velho (o "chuvisco").
      glClearColor(0, 0, 0, 0);
      glClear(GL_COLOR_BUFFER_BIT);
      return 1; // TVGLApplication stops callbacks on false (Tizen 4/5).
#else
      glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
#endif
    }
    return 0;
  }
  eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  nuiCorrente = 0;
  vez = VEZ_APP;
  pthread_cond_broadcast(&mudou);
  if (esperaMs < 0) {
    while (vez == VEZ_APP && !terminou) pthread_cond_wait(&mudou, &trava);
  } else {
    struct timespec ate;
    clock_gettime(CLOCK_REALTIME, &ate);
    ate.tv_nsec += (long)esperaMs * 1000000L;
    ate.tv_sec += ate.tv_nsec / 1000000000L;
    ate.tv_nsec %= 1000000000L;
    while (vez == VEZ_APP && !terminou)
      if (pthread_cond_timedwait(&mudou, &trava, &ate) != 0) break;
  }
  if (vez == VEZ_APP) {                  // passou do prazo: o app segue com o contexto
    r = terminou ? -1 : 0;
    nPulados++;
    pthread_mutex_unlock(&trava);
    return r;
  }
  eglMakeCurrent(dpy, sup, sup, ctx);
  nuiCorrente = 1;
  r = pendente ? 1 : (terminou ? -1 : 0);
  pendente = 0;
  pthread_mutex_unlock(&trava);
  if (r == 1 && (++nQuadros % 600) == 0) {
    printf("[tpk] quadros=%u pulados=%u\n", nQuadros, nPulados);
    fflush(stdout);
  }
  return r;
}

void *tpk_gl_criar(void) {
  etapasAnteriorParaLog();   // o main() ja abriu o nuvio.log a esta altura
  nv_tpk40_etapa("begin app-context");
  appEsperaVez(0);
  if (!eglMakeCurrent(dpy, sup, sup, ctx)) {
    printf("[tpk] eglMakeCurrent no fio do app falhou: 0x%x\n", eglGetError());
    snprintf(erro, sizeof erro, "eglMakeCurrent no fio do app falhou: 0x%x", eglGetError());
    nv_tpk40_etapa(erro); nv_tpk40_etapa("fail app-context");
    return NULL;
  }
  nv_tpk40_etapa("ok app-context");
  return (void *)1;
}

int tpk_gl_intervalo(int intervalo) {
  /* Called on the app thread while it owns the framework EGL context.
   * API9+ already disables vsync: preserve that host configuration. */
  return tpkEgl.SwapInterval && tpkEgl.SwapInterval(dpy, swapZero ? 0 : intervalo) ? 0 : -1;
}

void tpk_gl_trocar(void) {
  eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  appEsperaVez(1);
  eglMakeCurrent(dpy, sup, sup, ctx);
}

void *tpk_gl_corrente(void) {
  if (!tpkEgl.GetCurrentContext) return NULL;
  EGLContext c = eglGetCurrentContext();
  return c == EGL_NO_CONTEXT ? NULL : (void *)c;
}

void tpk_tamanho(int *w, int *h) {
  if (w) *w = telaW;
  if (h) *h = telaH;
}

int tpk_gl_atributo(SDL_GLattr a, int *v) {
  EGLint val = -1, id = 0;
  EGLConfig cfg;
  EGLint n = 0, pede[] = { EGL_CONFIG_ID, 0, EGL_NONE };
  EGLint qual;
  switch (a) {
    case SDL_GL_RED_SIZE:   qual = EGL_RED_SIZE; break;
    case SDL_GL_GREEN_SIZE: qual = EGL_GREEN_SIZE; break;
    case SDL_GL_BLUE_SIZE:  qual = EGL_BLUE_SIZE; break;
    case SDL_GL_ALPHA_SIZE: qual = EGL_ALPHA_SIZE; break;
    default: if (v) *v = 0; return -1;
  }
  eglQueryContext(dpy, ctx, EGL_CONFIG_ID, &id);
  pede[1] = id;
  if (eglChooseConfig(dpy, pede, &cfg, 1, &n) && n == 1)
    eglGetConfigAttrib(dpy, cfg, qual, &val);
  if (v) *v = val;
  return 0;
}

__attribute__((visibility("default")))
void nv_tpk_ciclo(int memoria) {
  // Rendering may already be suspended: wake the app thread without needing GL.
  pthread_mutex_lock(&trava);
  cicloPendente = memoria ? 2 : 1;
  pthread_cond_broadcast(&mudou);
  pthread_mutex_unlock(&trava);
}

// Tecla do controle, pelo nome que o NUI da (Key.KeyPressedName). Vem do fio
// principal do .NET; SDL_PushEvent e seguro de qualquer fio.
__attribute__((visibility("default")))
void nv_tpk_tecla(const char *nome, int apertou) {
  // A tabela mora em tpkteclas.c, para o simulador do Mac usar a mesma.
  SDL_Event e;
  if (!nome) return;
  if (apertou) tpkdesp_tecla(nome);
  if (tpkteclas_evento(nome, apertou, &e)) SDL_PushEvent(&e);
}
#endif
