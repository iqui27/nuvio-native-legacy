// Harness Tizen separado para a fixture ASS conhecida.
// Reutiliza video_tizen.c, legenda.c e player.c, sem conta ou catálogo.
#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <strings.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../src/catalogo.h"
#include "../src/gl_compat.h"
#include "../src/gfx.h"
#include "../src/text.h"
#include "../src/tex_cache.h"
#include "../src/sdlcompat.h"

#include "../src/video.h"
#include "../src/player.h"
#include "../src/legenda.h"
#include "../src/mkv.h"

enum { FASE_NENHUMA, FASE_EXTERNA, FASE_EXTERNA_PAUSADA, FASE_EMBEDDING,
       FASE_EMBUTIDA, FASE_EMBEDDED_SEM_TEXT, FASE_CONCLUIDA, FASE_ERRO };

static char urlVideo[1400], urlAss[1400];
static volatile int fase, sondaFaixas, sondaAss;
static volatile int sondaFaixaNumero, erroSonda;
static _Atomic int sondaPronta;
static char sondaCodec[24], estado[1200];
static pthread_t fioSonda;
static int fioSondaVivo;
static int diagnosticoPronto;
static Uint32 embeddingAt;

static void copia(char *dst, size_t tam, const char *src) {
  if (!dst || !tam) return;
  snprintf(dst, tam, "%s", src ? src : "");
}

static void *sondarCabecalho(void *arg) {
  MkvFaixa faixas[MKV_MAX_FAIXAS];
  int n, i;
  (void)arg;
  memset(faixas, 0, sizeof faixas);
  n = mkv_faixas(urlVideo, faixas, MKV_MAX_FAIXAS);
  sondaFaixas = n; sondaAss = 0; sondaFaixaNumero = 0; sondaCodec[0] = 0;
  for (i = 0; i < n; i++) {
    if (faixas[i].tipo != 17) continue;
    if (!strcasecmp(faixas[i].codec, "S_TEXT/ASS") ||
        !strcasecmp(faixas[i].codec, "S_TEXT/SSA")) {
      sondaAss = 1; sondaFaixaNumero = faixas[i].numero;
      copia(sondaCodec, sizeof sondaCodec, faixas[i].codec); break;
    }
  }
  if (!sondaAss) for (i = 0; i < n; i++) if (faixas[i].tipo == 17) {
    sondaFaixaNumero = faixas[i].numero;
    copia(sondaCodec, sizeof sondaCodec, faixas[i].codec); break;
  }
  // Os campos acima pertencem ao worker. O release publica todos eles antes
  // de o estado da tela fazer o primeiro acquire; assim a captura não observa
  // codec pela metade durante a leitura do cabeçalho Range.
  atomic_store_explicit(&sondaPronta, 1, memory_order_release);
  printf("[ASS-DIAG] cabecalho: faixas=%d ass=%d track=%d codec=%s\n",
         n, sondaAss, sondaFaixaNumero,
         sondaCodec[0] ? sondaCodec : "desconhecido");
  fflush(stdout);
  return NULL;
}

static int cuesVivos(void) {
  LegendaCue cues[LEGENDA_SIMULTANEAS];
  return legenda_cues(video_pos(), 0, cues, LEGENDA_SIMULTANEAS);
}

static const char *faseNome(void) {
  switch (fase) {
    case FASE_EXTERNA:   return "externa-ass-gl";
    case FASE_EXTERNA_PAUSADA: return "externa-ass-gl-pausada";
    case FASE_EMBEDDING: return "reabrindo-para-embedded";
    case FASE_EMBUTIDA:  return "embedded-text-avplay";
    case FASE_EMBEDDED_SEM_TEXT: return "embedded-text-nao-exposta";
    case FASE_CONCLUIDA: return "concluida";
    case FASE_ERRO:      return "erro";
    default:             return "iniciando";
  }
}

EMSCRIPTEN_KEEPALIVE
int nv_tizen_ass_ready(void) { return diagnosticoPronto; }

EMSCRIPTEN_KEEPALIVE
int nv_tizen_ass_prepared(void) {
  return fase == FASE_EXTERNA && video_pronto();
}

EMSCRIPTEN_KEEPALIVE
int nv_tizen_ass_start(const char *video, const char *ass) {
  if (!video || !*video || !ass || !*ass) return 0;
  copia(urlVideo, sizeof urlVideo, video);
  copia(urlAss, sizeof urlAss, ass);
  fase = FASE_EXTERNA;
  atomic_store_explicit(&sondaPronta, 0, memory_order_relaxed);
  sondaFaixas = sondaAss = erroSonda = 0;
  sondaFaixaNumero = 0; sondaCodec[0] = 0;
  video_iniciar();
  video_janela(0, 0, 1920, 1080);
  player_abrir(0, urlVideo);
  // player_abrir limpa a sessão anterior; esta é a rota de produção do
  // overlay externo, com download e parser reais.
  legenda_carregar(urlAss);
  if (!fioSondaVivo && pthread_create(&fioSonda, NULL, sondarCabecalho, NULL) == 0)
    fioSondaVivo = 1;
  else
    erroSonda = 1;
  printf("[ASS-DIAG] fase externa ASS/SSA: parser GL solicitado\n");
  fflush(stdout);
  return 1;
}

EMSCRIPTEN_KEEPALIVE
int nv_tizen_ass_seek(int segundos) {
  if (segundos < 0) segundos = 0;
  video_buscar((double)segundos);
  printf("[ASS-DIAG] seek solicitado: %ds\n", segundos);
  fflush(stdout);
  return 1;
}

EMSCRIPTEN_KEEPALIVE
int nv_tizen_ass_embedded(void) {
  if (!urlVideo[0] || (fase != FASE_EXTERNA && fase != FASE_EXTERNA_PAUSADA)) return 0;
  fase = FASE_EMBEDDING;
  embeddingAt = SDL_GetTicks();
  legenda_desligar();
  if (!video_tocar(urlVideo)) { fase = FASE_ERRO; return 0; }
  video_janela(0, 0, 1920, 1080);
  printf("[ASS-DIAG] fase embedded: overlay GL desligado; fonte reaberta\n");
  fflush(stdout);
  return 1;
}

EMSCRIPTEN_KEEPALIVE
int nv_tizen_ass_hold_external(void) {
  if (fase != FASE_EXTERNA) return 0;
  video_pausar(1);
  fase = FASE_EXTERNA_PAUSADA;
  printf("[ASS-DIAG] externa pausada em %.3fs; quadro reservado para captura\n", video_pos());
  fflush(stdout);
  return 1;
}

EMSCRIPTEN_KEEPALIVE
int nv_tizen_ass_pump(void) {
  video_bombear();
  if (fase == FASE_EMBEDDING && video_pronto()) {
    int n = video_n_legenda();
    if (n > 0) {
      // O ordinal vira índice absoluto no video_tizen.c; não se supõe que o
      // número do TrackEntry Matroska seja o que o firmware espera.
      video_escolher_legenda(0);
      fase = FASE_EMBUTIDA;
      printf("[ASS-DIAG] embedded TEXT selecionada: ordinal=0 total=%d codec=%s\n",
             n, sondaCodec[0] ? sondaCodec : "cabecalho-pendente");
      fflush(stdout);
    } else if (embeddingAt && SDL_GetTicks() - embeddingAt > 5000) {
      // O cabeçalho MKV já confirmou S_TEXT/ASS; esta fase registra somente
      // que getTotalTrackInfo não expôs TEXT nesta sessão AVPlay. Não vira
      // prova de limitação de firmware sem uma consulta independente.
      fase = FASE_EMBEDDED_SEM_TEXT;
      video_pausar(1);
      printf("[ASS-DIAG] embedded TEXT não exposta por getTotalTrackInfo; sessão pausada\n");
      fflush(stdout);
    }
  }
  // Um pacote separado não possui sessão. Este pump mantém player.c vivo
  // mesmo quando app_atualizar retorna cedo na tela de login.
  player_atualizar(1.0f / 60.0f, SDL_GetTicks());
  if (fase == FASE_EMBUTIDA && video_terminou()) fase = FASE_CONCLUIDA;
  return 1;
}

EMSCRIPTEN_KEEPALIVE
const char *nv_tizen_ass_state(void) {
  int pronta = atomic_load_explicit(&sondaPronta, memory_order_acquire);
  int nCue = (fase == FASE_EXTERNA || fase == FASE_EXTERNA_PAUSADA) ? cuesVivos() : 0;
  int nFaixas = pronta ? sondaFaixas : 0;
  int temAss = pronta ? sondaAss : 0;
  const char *codec = pronta && sondaCodec[0] ? sondaCodec : "desconhecido";
  snprintf(estado, sizeof estado,
           "fase=%s\navplay=%s pronto=%d tocando=%d\npos=%.3f dur=%.3f\n"
           "faixas-text=%d embedded=%d\nexterna-cues=%d\n"
           "mkv-faixas=%d codec=%s\nsonda=%s\n",
           faseNome(), video_ativo() ? "ativo" : "inativo",
           video_pronto(), video_tocando(), video_pos(), video_duracao(),
           video_n_legenda(), fase == FASE_EMBUTIDA, nCue,
           nFaixas, codec,
           pronta ? (temAss ? "S_TEXT/ASS confirmado" : "sem ASS") :
                         (erroSonda ? "falhou" : "em andamento"));
  return estado;
}

EMSCRIPTEN_KEEPALIVE
int nv_tizen_ass_pause(void) {
  video_pausar(1);
  if (fase == FASE_EMBUTIDA) {
    fase = FASE_CONCLUIDA;
    printf("[ASS-DIAG] ciclo concluído; embedded TEXT selecionada; frame mantido para captura\n");
  } else if (fase == FASE_EMBEDDED_SEM_TEXT) {
    printf("[ASS-DIAG] ciclo concluído sem confirmação embedded; frame mantido para captura\n");
  } else {
    fase = FASE_ERRO;
    printf("[ASS-DIAG] ciclo inconclusivo; frame mantido para captura\n");
  }
  fflush(stdout);
  return 1;
}

// O app de produção desenha a tela de login antes de player_desenhar(); isso é
// correto para NuvioTV002, mas esconderia o overlay no pacote diagnóstico sem
// uma sessão. Este entrypoint tem seu próprio loop SDL/GL e chama o mesmo
// player_desenhar de produção após cada pump do AVPlay.
EM_ASYNC_JS(void, nv_tizen_ass_frame, (), {
  await new Promise(function (resolve) { requestAnimationFrame(resolve); });
});

int main(int argc, char **argv) {
  SDL_Window *janela;
  SDL_GLContext contexto;
  int dw = 1920, dh = 1080;
  CatItem item;
  (void)argc; (void)argv;

  SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    printf("[ASS-DIAG] SDL_Init falhou: %s\n", SDL_GetError()); return 1;
  }
  IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);
  nv_blindar_formatos();
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  janela = SDL_CreateWindow("Nuvio ASS Diagnostic", SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, 1920, 1080,
                            SDL_WINDOW_OPENGL);
  if (!janela) {
    printf("[ASS-DIAG] janela falhou: %s\n", SDL_GetError()); return 1;
  }
  contexto = SDL_GL_CreateContext(janela);
  if (!contexto) {
    printf("[ASS-DIAG] contexto GL falhou: %s\n", SDL_GetError()); return 1;
  }
  SDL_GL_SetSwapInterval(1);
  SDL_GL_GetDrawableSize(janela, &dw, &dh);
  glViewport(0, 0, dw, dh);
  gfx_tamanho_alvo(dw, dh);
  if (!gfx_iniciar()) {
    printf("[ASS-DIAG] gfx_iniciar falhou\n"); return 1;
  }
  if (!txt_iniciar("/app", (float)dw / 1920.0f)) {
    printf("[ASS-DIAG] txt_iniciar falhou\n"); return 1;
  }
  tex_escala((float)dw / 1920.0f);
  tex_iniciar(64);
  gfx_icones_dir("/app/art");
  player_dir("/nuvio-ass-diagnostic");
  memset(&item, 0, sizeof item);
  snprintf(item.tipo, sizeof item.tipo, "movie");
  snprintf(item.titulo, sizeof item.titulo, "Fixture ASS / AVPlay");
  cat_definir(&item, 1);
  player_abrir(0, NULL);
  player_erro_fonte();
  player_limpar_erro_fonte();
  diagnosticoPronto = 1;
  printf("[ASS-DIAG] runtime pronto; aguardando fixture\n"); fflush(stdout);

  for (;;) {
    Uint32 agora = SDL_GetTicks();
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    nv_tizen_ass_pump();
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    player_desenhar(agora);
    SDL_GL_SwapWindow(janela);
    nv_tizen_ass_frame();
  }
}

#endif
