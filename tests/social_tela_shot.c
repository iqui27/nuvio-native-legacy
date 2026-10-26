// CAPTURA DA TELA "ENTRE AMIGOS" (social.c), sem rede. tests/social_shot.c
// fotografa o PAINEL da tecla azul, e nao esta tela; ela so aparecia no
// aparelho. Existe para a correcao da rail fixa (26/09): com
// NUVIO_RAIL=fixa|moderna|recolhida a rail e pintada por cima, como em app.c.
//
// src/social.c e INCLUIDO: `dados` e estatico do modulo e semea-lo por dentro e
// o unico jeito de ter a lista cheia sem o Trakt no ar.
#include "../src/social.c"
#include "nvtmp.h"
#include "rail_shot.h"
#include "dados.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

#include "j4_proof.h"
static GLuint fbo, fboTex;
static void proof_reset(void) { entrada=tempo=rolagem=velocidade=segSel=segFoco=revT=0;sair=0;selecionado=0;filtro=0;zona=1;memset(focoAnim,0,sizeof focoAnim); }
static void proof_focus(void) { selecionado=1; }
static void proof_close(void) { sair=1; }
static void proof_reopen(void) { sair=0; }
static void proof_step(float dt) { social_atualizar(dt,SDL_GetTicks()); }
static void proof_draw(void) { social_desenhar(SDL_GetTicks()); }

static void captura(const char *nome, SDL_Window *win) {
  int i;
  rail_shot_aplicar();
  for (i = 0; i < 30; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    gfx_novo_quadro();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 1920, 1080);
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    social_atualizar(1.0f/60,SDL_GetTicks());
    social_desenhar(SDL_GetTicks());
    rail_shot_desenhar(MENU_PERFIL);
    if (i == 29) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glFinish();
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-social-tela");
  const char *dir = getenv("NUVIO_DADOS");
  // Real bundled art (deploy/app/art/catalogo.txt): poster + backdrop per title.
  static const char *const ACOES[6] = { "assistindo agora", "assistiu", "avaliou", "assistiu", "assistiu", "avaliou" };
  static const char *const TITULOS[6] = { "Fallout", "Widow's Bay", "The Prestige", "One Battle After Another", "From", "Frequency" };
  static const char *const DETALHES[6] = { "T2E5 · The Wrangler", "T1E3", "Filme", "Filme", "T3E7", "Filme" };
  static const int ART[6] = { 0, 1, 2, 3, 4, 5 };
  static const int HA[6] = { 120, 15 * 60, 26 * 3600, 3 * 86400, 5 * 86400, 12 * 86400 };
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  int i;
  assert(dir && *dir);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  dados_iniciar(dir);
  assert(!strcmp(dados_dir(), dir));
  ajustes_iniciar();
  assert(dados_gravar("ajustes.txt","idioma 0\nidiomaAutoLocal 0\n"));
  ajustes_dir(dir);
  assert(ajustes_idioma()==0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: captura social", 0, 0, 64, 64,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  glGenTextures(1, &fboTex);
  glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  char *art=realpath("deploy/app/art",NULL),ico[700];assert(art);gfx_icones_dir(art);snprintf(ico,sizeof ico,"%s/icones/aj_users.png",art);tex_obter_larg(ico,32);free(art);

  memset(&dados, 0, sizeof dados);
  snprintf(dados.pessoa.socialNome, sizeof dados.pessoa.socialNome, "Marina Albuquerque");
  snprintf(dados.pessoa.socialSlug, sizeof dados.pessoa.socialSlug, "marina-alb");
  snprintf(dados.local, sizeof dados.local, "Recife, PE");
  snprintf(dados.bio, sizeof dados.bio, "Cinema de autor, séries britânicas e tudo que tenha trilha do Jonny Greenwood.");
  dados.estado = SOCIAL_PRONTO;
  dados.n = 6;
  for (i = 0; i < 6; i++) {
    Atividade *a = &dados.atividades[i];
    snprintf(a->pessoa, sizeof a->pessoa, "Marina Albuquerque");
    snprintf(a->acao, sizeof a->acao, "%s", i18n(ACOES[i]));
    snprintf(a->titulo, sizeof a->titulo, "%s", TITULOS[i]);
    snprintf(a->detalhe, sizeof a->detalhe, "%s", DETALHES[i]);
    snprintf(a->horario, sizeof a->horario, "%02d/09/2026 · 21:%02d", 10 + i, 10 + i);
    snprintf(a->imdb, sizeof a->imdb, "tt%07d", i + 1);
    a->quando = time(NULL) - HA[i];
    a->avaliacao = !strcmp(ACOES[i], "avaliou");
    snprintf(a->poster,sizeof a->poster,"deploy/app/art/poster/%02d.jpg",ART[i]);
    snprintf(a->fundo,sizeof a->fundo,"deploy/app/art/%02d.jpg",ART[i]);
    tex_obter(a->poster); tex_obter_hero(a->fundo);
  }
  snprintf(dados.pessoa.socialAvatar,sizeof dados.pessoa.socialAvatar,"deploy/app/art/elenco/09_0.jpg");
  tex_obter(dados.pessoa.socialAvatar);j4_art_ready();
  j4_reopen=proof_reopen;
  j4_proof("social",proof_reset,proof_focus,proof_step,proof_draw,proof_close);
  selecionado = 1;
  snprintf(nome, sizeof nome, "%s-pronto.bmp", saida);
  captura(nome, w);
  zona = 0; filtro = 2; selecionado = 0;
  snprintf(nome, sizeof nome, "%s-avaliacoes.bmp", saida);
  captura(nome, w);
  zona = 1; filtro = 0;
  dados.estado = SOCIAL_CARREGANDO;
  snprintf(nome, sizeof nome, "%s-carregando.bmp", saida);
  captura(nome, w);
  dados.estado = SOCIAL_PRIVADO;
  snprintf(nome, sizeof nome, "%s-privado.bmp", saida);
  captura(nome, w);
  printf("pronto\n");
  return 0;
}
