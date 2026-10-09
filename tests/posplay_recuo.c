// Estado real de posplay.c, com catalogo e desenho dublados. Sem SDL, janela
// ou rede: testa a contagem, o pedido ao roteador e o OK depois de um recuo.
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "../src/catalogo.h"

// O teste inclui o modulo inteiro; so suas dependencias de UI sao inertes.
#define NV_POSPLAY_H
#define NV_IDIOMA_H
#define NV_EXTRAS_H
#define NV_GFX_H
#define NV_TEXT_H
#define NV_TEX_CACHE_H
#define NV_ANIM_H
#define NV_AJUSTES_H
#define NV_VISTOEP_H
#define NV_DETAIL_H
#define NV_DESCOBERTA_H
#define NV_VIDEO_H
#define NV_PLAYER_H
#define NV_PLRUI_H
#define NV_PLRILHA_H
#define NV_ESCALA_H
#define NV_VTELA_W 1920.0f
#define NV_VTELA_H 1080.0f
typedef unsigned Uint32;
typedef unsigned GLuint;
typedef struct { int type; struct { struct { int sym, scancode; } keysym; } key; } SDL_Event;
enum { SDL_KEYDOWN = 1, SDLK_AC_BACK, SDLK_ESCAPE, SDLK_BACKSPACE,
       SDLK_DOWN, SDLK_RETURN, SDLK_KP_ENTER, SDLK_RIGHT, SDLK_LEFT };
typedef struct { float x, y, w, h; } GfxRect;
typedef struct { int w, h; } TxtLinha;
static float semDesenho(void) { return 0.0f; }
#define ESCALA_INI() ((void)0)
#define ESCALA_FIM() ((void)0)
#define anim_mola(valor, alvo, dt, mola) (alvo)
#define gfx_anel(...) ((void)0)
#define gfx_cor(...) ((void)0)
#define gfx_rect(...) ((void)0)
#define gfx_recorte(...) ((void)0)
#define gfx_sem_recorte() ((void)0)
#define tex_obter_larg(...) 0u
#define tex_aspecto(...) 1.0f
#define txt_linha(...) ((TxtLinha){0, 0})
#define txt_linha_corta(...) ((TxtLinha){0, 0})
#define txt_desenhar_alpha(...) ((void)0)
#define txt_bloco_corta(...) ((void)0)
#define plrui_material(...) ((void)0)
#define plrui_botao(...) semDesenho()
#define plrui_kicker(...) semDesenho()
#define plrui_dicas(...) semDesenho()
static float gfx_tex_aspect_atual;

static CatItem titulo;
static CatEp episodios[2];
const CatItem *cat_item(int i) { return i == 0 ? &titulo : NULL; }
int cat_indice_vivo(int i, const char *id) { return i == 0 && !strcmp(id, titulo.imdb) ? 0 : -1; }
int cat_indice_por_imdb(const char *id) { return !strcmp(id, titulo.imdb) ? 0 : -1; }
int cat_n_episodios(int i) { return i == 0 ? 2 : 0; }
const CatEp *cat_episodio(int i, int e) { return i == 0 && e >= 0 && e < 2 ? &episodios[e] : NULL; }
static const char *i18n(const char *s) { return s; }
static int extras_n_relacionados(void) { return 0; }
static const char *extras_relacionado_imdb(int i) { (void)i; return ""; }
static const char *extras_relacionado_titulo(int i) { (void)i; return ""; }
static const char *extras_relacionado_ano(int i) { (void)i; return ""; }
static const char *extras_relacionado_poster(int i) { (void)i; return ""; }
static int ajustes_desfocar_nao_assistidos(void) { return 0; }
static int ajustes_vidro(void) { return 0; }
static void ajustes_acento(float *r, float *g, float *b) { *r = *g = *b = 0; }
static int vistoep_estado(const char *id, int t, int e) { (void)id; (void)t; (void)e; return 0; }
static GLuint gfx_desfocado(GLuint t, const char *arte) { (void)arte; return t; }
static void desc_pedir_titulo(const char *id) { (void)id; }
static void desc_duracao_txt(const char *s, char *out, size_t n) { snprintf(out, n, "%s", s); }
static double video_creditos(void) { return 0; }
double intro_creditos_seg(void) { return 0; }
static void player_episodio_atual(int *t, int *e) { *t = *e = 1; }
static void player_aprender_creditos(void) {}
static int player_velocidade_efetiva(void) { return 100; }
double vel_tempo_real(double s, int cent) { return s * 100.0 / cent; }

#include "../src/posplay.c"

static void abrir(void) {
  posplay_fechar();
  for (int i = 0; i < 10; i++) posplay_atualizar(1.0f, 500, 0, 1000, 1, 0, 0);
  posplay_atualizar(0.016f, 1000, 997, 1000, 1, 0, 1);
  assert(posplay_visivel());
}
static int pedir(void) {
  int t = 0, e = 0;
  int ok = posplay_pediu_episodio(&t, &e);
  if (ok) assert(t == 1 && e == 2);
  return ok;
}
static void ok(void) {
  SDL_Event ev = {0};
  ev.type = SDL_KEYDOWN;
  ev.key.keysym.sym = SDLK_RETURN;
  assert(posplay_evento(&ev) == 1);
}

int main(void) {
  snprintf(titulo.imdb, sizeof titulo.imdb, "tt001");
  titulo.temporada = titulo.episodio = 1;
  episodios[0].temporada = episodios[1].temporada = 1;
  episodios[0].episodio = 1; episodios[1].episodio = 2;

  abrir();
  posplay_atualizar(0.016f, 4100, 1000, 1000, 1, 0, 1);
  assert(pedir() && !pedir());

  abrir();
  posplay_recuar();
  posplay_atualizar(0.016f, 6000, 996, 1000, 1, 0, 1);
  posplay_atualizar(0.016f, 12000, 1000, 1000, 1, 0, 1);
  assert(posplay_visivel() && !pedir());
  // Fora da janela, uma nova passagem volta ao comportamento de sempre.
  posplay_atualizar(0.016f, 13000, 700, 1000, 1, 0, 0);
  posplay_atualizar(0.016f, 14000, 997, 1000, 1, 0, 1);
  posplay_atualizar(0.016f, 17100, 1000, 1000, 1, 0, 1);
  assert(pedir());

  // A barra pode sair da janela antes do recuo chegar ao motor. Isso nao
  // libera a contagem quando uma amostra antiga volta a dizer "fim".
  abrir();
  posplay_recuar();
  posplay_aguardar_busca(1);
  posplay_atualizar(0.016f, 6000, 700, 1000, 1, 0, 0);
  posplay_atualizar(0.016f, 7000, 1000, 1000, 1, 0, 1);
  posplay_aguardar_busca(0);
  posplay_atualizar(0.016f, 8000, 1000, 1000, 1, 0, 1);
  assert(posplay_visivel() && !pedir());
  // So a observacao fora da janela DEPOIS da busca libera a proxima passagem.
  posplay_atualizar(0.016f, 9000, 700, 1000, 1, 0, 0);
  posplay_atualizar(0.016f, 10000, 997, 1000, 1, 0, 1);
  posplay_atualizar(0.016f, 13100, 1000, 1000, 1, 0, 1);
  assert(pedir());

  abrir();
  posplay_atualizar(0.016f, 4100, 1000, 1000, 1, 0, 1);
  assert(!posplay_visivel());
  posplay_recuar();
  assert(!pedir() && posplay_visivel());
  posplay_atualizar(0.016f, 12000, 998, 1000, 1, 0, 1);
  assert(!pedir());
  ok();
  posplay_recuar();
  assert(pedir() && !pedir());

  // O reset de outro titulo solta o bloqueio do recuo anterior.
  abrir();
  posplay_atualizar(0.016f, 4100, 1000, 1000, 1, 0, 1);
  assert(pedir());

  // Destino mostrado antes do seek terminar nao pode virar Next automatico.
  abrir();
  posplay_aguardar_busca(1);
  posplay_atualizar(0.016f, 6000, 1000, 1000, 1, 0, 1);
  assert(posplay_visivel() && !pedir());
  ok();
  posplay_aguardar_busca(1);
  assert(pedir() && !pedir());        // OK explicito passa mesmo durante o seek

  // Suspender tambem apaga o automatico que o roteador ainda nao consumiu.
  abrir();
  posplay_atualizar(0.016f, 4100, 1000, 1000, 1, 0, 1);
  posplay_aguardar_busca(1);
  assert(!pedir() && posplay_visivel());
  posplay_atualizar(0.016f, 6000, 1000, 1000, 1, 0, 1);
  assert(!pedir());
  posplay_aguardar_busca(0);          // fim nativo confirmou o destino
  posplay_atualizar(0.016f, 6100, 1000, 1000, 1, 0, 1);
  assert(pedir());

  // Progresso depois da busca devolve a contagem dos segundos restantes.
  abrir();
  posplay_aguardar_busca(1);
  posplay_atualizar(0.016f, 6000, 998, 1000, 1, 0, 1);
  assert(!pedir());
  posplay_aguardar_busca(0);
  posplay_atualizar(0.016f, 6100, 998, 1000, 1, 0, 1);
  assert(!pedir());
  posplay_atualizar(0.016f, 8200, 1000, 1000, 1, 0, 1);
  assert(pedir());

  // Nova abertura tambem limpa uma espera antiga.
  abrir();
  posplay_atualizar(0.016f, 4100, 1000, 1000, 1, 0, 1);
  assert(pedir());
  puts("posplay_recuo: contagem, pedido automatico e OK manual ok");
  return 0;
}
