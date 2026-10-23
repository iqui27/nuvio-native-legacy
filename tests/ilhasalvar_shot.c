// CAPTURA DE SALVAR NA ILHA (ilhasalvar.h): a capa em voo ate a pilula, o
// "Salvo em ..." assentado, o "Removido da lista", a pergunta de primeira vez
// ("Onde o + salva?") nascendo da pilula e a resposta.
//
// NAO ENTRA NA SUITE (*_shot.sh): precisa de janela GL e de olho humano. Cada
// captura e um PNG so da faixa de cima (a altura do quadro do mockup), para
// comparar lado a lado com o mockup renderizado na mesma escala.
//
// FUNDO: NUVIO_SHOT_FUNDOS=<pasta> com bgNN.png (o fundo do quadro NN do
// mockup, 1920x1080, sem a ilha) — assim o vidro fica sobre a MESMA arte. Sem
// a pasta, a arte embarcada 00.jpg. NUVIO_SHOT_VIDRO=0 = solido.
#include "ilha.h"
#include "nvtmp.h"
#include "ilhasalvar.h"
#include "ilhaacao.h"
#include "ilhasinais.h"
#include "trakt.h"
#include "simklauth.h"
#include "dados.h"
#include "salvos.h"
#include "anim.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *win;
static const char *base;
static GLuint fundoTex;
static int fundoN = -1, direita;

static void fundo(int n) {
  const char *dir = getenv("NUVIO_SHOT_FUNDOS");
  char cam[600];
  SDL_Surface *s, *t;
  if (n == fundoN) return;
  fundoN = n;
  if (fundoTex) { glDeleteTextures(1, &fundoTex); fundoTex = 0; }
  if (dir && *dir) snprintf(cam, sizeof cam, "%s/bg%02d.png", dir, n);
  else snprintf(cam, sizeof cam, "deploy/app/art/00.jpg");
  s = IMG_Load(cam);
  if (!s) return;
  t = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(s);
  if (!t) return;
  glGenTextures(1, &fundoTex);
  glBindTexture(GL_TEXTURE_2D, fundoTex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, t->pitch / 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->w, t->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t->pixels);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  SDL_FreeSurface(t);
}

// Grava a faixa [0, h) da tela em PNG (sem BMP intermediario: disco cheio).
static void captura(const char *nome, int h) {
  unsigned char *pix = malloc(1920 * (size_t)h * 4);
  SDL_Surface *s;
  char cam[700];
  int y;
  assert(pix);
  glReadPixels(0, 1080 - h, 1920, h, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, h, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < h; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (size_t)(h - 1 - y) * 1920 * 4, 1920 * 4);
  snprintf(cam, sizeof cam, "%s-%s.png", base, nome);
  assert(IMG_SavePNG(s, cam) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", cam);
}

static int relogio = 1;
static const char *atividade;
static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    // O orcamento de rasterizacao de texto e POR QUADRO: sem isto, as linhas
    // novas depois das primeiras centenas voltam vazias (o rotulo do botao).
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(3);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (fundoTex) gfx_rect((GfxRect){ 0, 0, 1920, 1080 }, fundoTex, GFX_SNAP, 0, 0, 0, 0, 1, 1, 1, 1);
    if (atividade) ilha_atividade(atividade, -1.0f);
    ilha_relogio_visivel(relogio);
    // A margem do mockup: pilula em x 96 (esquerda) ou a 64 da borda direita.
    if (direita) ilha_ancorar(1920 - 64, 36, 1); else ilha_ancorar(96, 36, 0);
    ilha_desenhar(agora);
    ilhasinais_passo(agora);
    SDL_GL_SwapWindow(win);
    SDL_Delay(16);
  }
}
static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  ilha_evento(&e);
}

static void limpar(void) {
  int q;
  while (ilha_aviso_vez()[0]) ilha_retirar(ilha_aviso_vez());
  for (q = 0; q < ILHA_N_CARTOES; q++) ilha_cartao(q, NULL);
  ilha_modal_fechar(1);
  atividade = NULL;
  direita = 0;
  quadros(45);
}

static char f1[200], f2[200], txt[300];
static IlhaAvisoEx ex(const char *chave, int tipo, const char *icone, unsigned ms) {
  IlhaAvisoEx e;
  memset(&e, 0, sizeof e);
  e.chave = chave; e.tipo = tipo; e.icone = icone; e.texto = txt; e.ms = ms ? ms : 60000;
  return e;
}
// Um aviso, `n` = o numero do quadro do mockup (fundo e nome do arquivo).
static void pilula(int n, const char *nome, IlhaAvisoEx e) {
  char b[80];
  fundo(n);
  limpar();
  ilha_avisar_ex(&e);
  quadros(70);
  snprintf(b, sizeof b, "%02d-%s", n, nome);
  captura(b, n >= 33 ? 250 : 140);
}
static void modal(int n, const char *nome, int h, IlhaAvisoEx e) {
  char b[80];
  fundo(n);
  limpar();
  ilha_avisar_ex(&e);
  quadros(50);
  tecla(SDLK_s);
  quadros(80);
  snprintf(b, sizeof b, "%02d-%s", n, nome);
  captura(b, h);
}


static CatItem item;
// Roda quadros ate passarem `ate` ms desde `t0`, capturando cada marco de `marcos`.
static void voo(const char *prefixo, const int *marcos, int n, Uint32 t0, int h) {
  // Le os pixels de cada marco na hora (barato) e so grava os PNG depois: o
  // PNG de 840 linhas levava ~350 ms por quadro e atrasava o relogio do voo.
  unsigned char *buf[8];
  int i, y;
  for (i = 0; i < n; i++) {
    buf[i] = malloc(1920 * (size_t)h * 4);
    assert(buf[i]);
    while ((int)(SDL_GetTicks() - t0) < marcos[i]) quadros(1);
    printf("[t] %s %d real=%u\n", prefixo, marcos[i], SDL_GetTicks() - t0);
    glReadPixels(0, 1080 - h, 1920, h, GL_RGBA, GL_UNSIGNED_BYTE, buf[i]);
  }
  for (i = 0; i < n; i++) {
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, h, 32, SDL_PIXELFORMAT_RGBA32);
    char cam[700];
    for (y = 0; y < h; y++)
      memcpy((char *)s->pixels + y * s->pitch, buf[i] + (size_t)(h - 1 - y) * 1920 * 4, 1920 * 4);
    snprintf(cam, sizeof cam, "%s-%s-%04dms.png", base, prefixo, marcos[i]);
    assert(IMG_SavePNG(s, cam) == 0);
    SDL_FreeSurface(s);
    free(buf[i]);
  }
}

int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS");
  char ajustes[700], arq[700];
  SDL_GLContext gl;
  FILE *f;
  const char *vidro = getenv("NUVIO_SHOT_VIDRO");
  base = argc > 1 ? argv[1] : nvt("/nuvio-ilhasalvar");
  assert(dir && *dir);
  snprintf(ajustes, sizeof ajustes, "%s/ajustes.txt", dir);
  f = fopen(ajustes, "w"); assert(f);
  fprintf(f, "idioma 0\nselected_theme 2\nsalvosDestino 1\n");
  fclose(f);
  ajustes_dir(dir);
  dados_iniciar("deploy/app");
  snprintf(arq, sizeof arq, "%s/simkl-p1.txt", dir);
  f = fopen(arq, "w"); assert(f); fputs("tok\n", f); fclose(f);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: ilhasalvar", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  gl = SDL_GL_CreateContext(win);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  gfx_snap_iniciar(1920, 1080);
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  ajustes_definir_vidro(!(vidro && vidro[0] == '0'));
  simklauth_carregar();
  fundo(0);
  memset(&item, 0, sizeof item);
  snprintf(item.imdb, sizeof item.imdb, "tt0903747");
  snprintf(item.tipo, sizeof item.tipo, "series");
  snprintf(item.titulo, sizeof item.titulo, "Breaking Bad");
  snprintf(item.poster, sizeof item.poster, "deploy/app/art/poster/00.jpg");
  tex_obter_larg(item.poster, 300);   // aquece a capa

  quadros(60);
  // 1. SALVAR (ja perguntado): a capa voa, pousa, "Salvo em Watchlist do Trakt".
  dados_gravar("salvar-perguntado.txt", "1\n");
  assert(!ilhasalvar_perguntar(&item, 1));
  trakt_definir("tk", "cli");
  { static const int m[] = { 120, 260, 360, 460, 560, 700, 1400 };
    Uint32 t0 = SDL_GetTicks();
    ilhasalvar_aviso(&item, 1);
    voo("1-salvar", m, 7, t0, 840); }
  limpar();
  // 2. Tirar: neutro, sem voo.
  { static const int m[] = { 400, 1200 };
    Uint32 t0 = SDL_GetTicks();
    ilhasalvar_aviso(&item, 0);
    voo("2-removido", m, 2, t0, 200); }
  limpar();
  // 3. Primeira vez: a pergunta (Nuvio + Trakt + Simkl ligados).
  dados_apagar("salvar-perguntado.txt");
  assert(ilhasalvar_perguntar(&item, 1));
  assert(ilhasalvar_pergunta_aberta());
  { static const int m[] = { 90, 200, 1600 };
    Uint32 t0 = SDL_GetTicks();
    voo("3-pergunta", m, 3, t0, 560); }
  assert(ilha_modal_aberto());
  tecla(SDLK_RIGHT); quadros(25);
  captura("3b-pergunta-foco-trakt", 560);
  // Responde "Trakt" (foco 1) -> grava, salva e anima.
  tecla(SDLK_RETURN);
  { static const int m[] = { 300, 460, 1500 };
    Uint32 t0 = SDL_GetTicks();
    voo("4-respondeu", m, 3, t0, 840); }
  assert(!ilhasalvar_pergunta_aberta());
  assert(ajustes_salvos_no_trakt());
  { char *s = dados_ler("salvar-perguntado.txt"); assert(s); free(s); }
  assert(salvos_tem("tt0903747"));
  limpar();
  // 5. Segunda: a escolha "Simkl", e depois o Voltar (mantem o padrao).
  dados_apagar("salvar-perguntado.txt");
  salvos_definir(&item, 0);
  assert(ilhasalvar_perguntar(&item, 1));
  quadros(60);
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RETURN);
  quadros(40);
  assert(ajustes_salvos_no_simkl());
  limpar();
  dados_apagar("salvar-perguntado.txt");
  salvos_definir(&item, 0);
  assert(ilhasalvar_perguntar(&item, 1));
  quadros(60);
  tecla(SDLK_BACKSPACE);
  quadros(90);
  assert(!ilhasalvar_pergunta_aberta());
  assert(ajustes_salvos_no_simkl());   // Voltar manteve o padrao em vigor
  assert(salvos_tem("tt0903747"));
  limpar();
  // 7. O aviso -> AZUL abre "o que foi feito" com Desfazer -> desfaz.
  salvos_definir(&item, 0);
  ilhasalvar_executar(&item, 1);
  assert(salvos_tem("tt0903747") && ilhaacao_tem_desfazer());
  quadros(90);
  captura("7a-acao-aviso", 200);
  tecla(SDLK_s);
  quadros(80);
  captura("7b-acao-modal-desfazer", 700);
  tecla(SDLK_RETURN);   // foco 0 = Desfazer
  quadros(30);
  assert(!salvos_tem("tt0903747"));
  captura("7c-apos-desfazer-modal-fecha", 300);
  quadros(60);
  captura("7d-desfeito", 200);
  limpar();
  // 7e. Com o fundo (paisagem) do titulo: o modal usa ele na moldura 16:9.
  snprintf(item.backdrop, sizeof item.backdrop, "deploy/app/art/00.jpg");
  tex_obter_larg(item.backdrop, 480);
  quadros(30);
  salvos_definir(&item, 0);
  ilhasalvar_executar(&item, 1);
  quadros(90);
  tecla(SDLK_s);
  quadros(80);
  captura("7e-acao-modal-paisagem", 700);
  tecla(SDLK_ESCAPE);
  quadros(30);
  item.backdrop[0] = 0;
  limpar();
  // 8. Acao sem Desfazer (Tirar de Continuar): so "Ok".
  { IlhaAcao a;
    memset(&a, 0, sizeof a);
    a.icone = "aj_x"; a.frase = "Removido de Continuar assistindo"; a.titulo = item.titulo;
    a.thumb = item.poster; a.tipo = ILHA_INFO;
    ilhaacao_feita(&a); }
  quadros(90);
  tecla(SDLK_s);
  quadros(80);
  captura("8-acao-sem-desfazer-modal", 700);
  tecla(SDLK_RETURN);
  quadros(40);
  limpar();
  // 6. Animacoes reduzidas: so o aviso, sem voo.
  anim_politica_reduzida = 1;
  { static const int m[] = { 600 };
    Uint32 t0 = SDL_GetTicks();
    ilhasalvar_aviso(&item, 1);
    voo("6-reduzida", m, 1, t0, 840); }
  anim_politica_reduzida = 0;

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  puts("PASS: capturas de salvar na ilha gravadas.");
  return 0;
}
