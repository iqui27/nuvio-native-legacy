// CAPTURAS DA BUSCA, sem depender da resposta de rede.
//
// O teste existe para olhar a distancia de sofa: campo ativo, foco da grade,
// cursor e estado vazio depois de duas letras. A lista real e assincrona e fica
// para o teste manual do aparelho; aqui o importante e a casca da interacao.
#include "busca.h"
#include "nvtmp.h"
#include "buscasrec.h"
#include "dados.h"
#include "ajustes.h"
#include "rail_shot.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "catalogo.h"
#include "sistexto.h"
#include "spotpessoa.h"
#include "vidro_telas.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *janela;

// TMDB de mentira (spotpessoa_teste): as tres pessoas do mockup, sem rede.
static char *tmdbFalso(const char *url) {
  static const char *j =
    "{\"results\":["
    "{\"id\":3894,\"name\":\"Christian Bale\",\"profile_path\":\"/a.jpg\",\"known_for\":[{\"id\":155,\"media_type\":\"movie\",\"title\":\"The Dark Knight\"},{\"id\":1124,\"media_type\":\"movie\",\"title\":\"The Prestige\"}]},"
    "{\"id\":1245,\"name\":\"Scarlett Johansson\",\"profile_path\":\"/b.jpg\",\"known_for\":[{\"id\":24428,\"media_type\":\"movie\",\"title\":\"The Avengers\"},{\"id\":1124,\"media_type\":\"movie\",\"title\":\"The Prestige\"}]},"
    "{\"id\":6968,\"name\":\"Hugh Jackman\",\"profile_path\":\"/c.jpg\",\"known_for\":[{\"id\":1124,\"media_type\":\"movie\",\"title\":\"The Prestige\"}]}]}";
  (void)url;
  return strdup(j);
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  busca_evento(&e);
}

static void quadro(void) {
  SDL_PumpEvents();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(12);
  gfx_novo_quadro();
  vt_antes();
  busca_atualizar(1.0f / 60.0f, SDL_GetTicks());
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  busca_desenhar(SDL_GetTicks());
  rail_shot_desenhar(MENU_BUSCAR);
  vt_depois();
  if (!vt_ligado()) SDL_GL_SwapWindow(janela);
}

// `ms` > 0: grava depois de tantos milissegundos de quadros, e nao depois de
// 45 quadros — para fotografar uma animacao no meio (a onda dos resultados).
static void capturaEm(const char *nome, Uint32 ms) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int i, y;
  assert(pix);
  rail_shot_aplicar();
  if (ms) { Uint32 t0 = SDL_GetTicks(); while (SDL_GetTicks() - t0 < ms) quadro(); }
  else for (i = 0; i < 45; i++) quadro();
  quadro();
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4,
           1920 * 4);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", nome);
}
static void captura(const char *nome) { capturaEm(nome, 0); }

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-busca");
  const char *dir = getenv("NUVIO_DADOS");
  char nome[600];
  SDL_GLContext gl;
  if (!dir || !*dir) return 2;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: revisao da Busca", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, 1920, 1080,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(96);
  gfx_icones_dir("deploy/app/art");
  dados_iniciar(dir);
  assert(!strcmp(dados_dir(), dir));
  ajustes_iniciar();
  if (getenv("NUVIO_SHOT_SOLIDO")) ajustes_aplicar_blob("{\"vidro\":false}");
  if (getenv("NUVIO_SHOT_ESCALA")) gfx_escala_txt_definir((float)atof(getenv("NUVIO_SHOT_ESCALA")));
  busca_iniciar();
  // NUVIO_VIDRO_TELAS=<dir> (tests/vidro_telas.sh): glass budget, popular grid.
  if (vt_ligado()) {
    int i;
    vt_vidro(); busca_iniciar();
    for (i = 0; i < 120; i++) { quadro(); SDL_Delay(2); }
    vt_zerar(); for (i = 0; i < 30; i++) quadro();
    vt_png("busca"); vt_relatar("search", 1);
    for (i = 0; i < 8; i++) { tecla(SDLK_RIGHT); quadro(); quadro(); quadro(); }
    vt_relatar("search", 0);
    return vtFalhas ? 1 : 0;
  }

  snprintf(nome, sizeof nome, "%s-vazio.bmp", saida);
  captura(nome);
  tecla(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-foco.bmp", saida);
  captura(nome);
  tecla(SDLK_a);
  tecla(SDLK_g);
  snprintf(nome, sizeof nome, "%s-digitado.bmp", saida);
  captura(nome);
  // ?!# page: bottom row, 4th key; types "é" and ":".
  { int i;
    for (i = 0; i < 6; i++) tecla(SDLK_DOWN);
    for (i = 0; i < 3; i++) tecla(SDLK_RIGHT);
    tecla(SDLK_RETURN);
    for (i = 0; i < 6; i++) tecla(SDLK_UP);
    for (i = 0; i < 4; i++) tecla(SDLK_RIGHT);
    tecla(SDLK_RETURN);
    snprintf(nome, sizeof nome, "%s-simbolos.bmp", saida);
    captura(nome);
  }

  // COM HISTORICO: campo vazio mostra as pilulas no lugar do estado vazio.
  // Termos de tamanhos diferentes e um longo, para a quebra de linha e o
  // "Limpar" no fim aparecerem; registrados do mais antigo para o mais novo.
  { static const char *termos[] = {
      "up", "interestelar", "the office", "dune", "o senhor dos aneis",
      "breaking bad", "matrix", "fundacao", "stranger things", "cidade de deus" };
    int i;
    for (i = 0; i < 10; i++) buscasrec_registrar(termos[i]); }
  busca_iniciar();
  snprintf(nome, sizeof nome, "%s-recentes.bmp", saida);
  captura(nome);
  // Da tecla "a" (coluna 0) ate a ultima coluna e mais um: a ponte leva as
  // pilulas. Depois desce uma linha, para o foco cair no meio da lista.
  { int i; for (i = 0; i < 6; i++) tecla(SDLK_RIGHT); }
  snprintf(nome, sizeof nome, "%s-recentes-foco.bmp", saida);
  captura(nome);
  tecla(SDLK_DOWN);
  tecla(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-recentes-foco2.bmp", saida);
  captura(nome);
  // Ate o "Limpar": fim da ultima linha.
  { int i; tecla(SDLK_DOWN); for (i = 0; i < 10; i++) tecla(SDLK_RIGHT); }
  snprintf(nome, sizeof nome, "%s-recentes-limpar.bmp", saida);
  captura(nome);
  // TECLADO CIRILICO (#176): com o idioma dos metadados em russo aparece a
  // tecla de layout na fileira de baixo; ela troca o teclado e o texto digitado
  // sai em UTF-8 inteiro, e o "apagar" tira um CARACTER, nao um byte.
  ajustes_aplicar_blob("{\"tmdb_language\":\"ru\"}");
  assert(!strcmp(ajustes_tmdb_idioma(), "ru-RU"));
  buscasrec_limpar();
  busca_iniciar();
  snprintf(nome, sizeof nome, "%s-latino-com-tecla.bmp", saida);
  captura(nome);
  { int i;
    for (i = 0; i < 6; i++) tecla(SDLK_DOWN);      /* fileira de baixo */
    for (i = 0; i < 4; i++) tecla(SDLK_RIGHT);     /* tecla de layout */
    tecla(SDLK_RETURN);                            /* cirilico */
    snprintf(nome, sizeof nome, "%s-cirilico.bmp", saida);
    captura(nome);
    for (i = 0; i < 3; i++) tecla(SDLK_LEFT);      /* espaco (coluna 0) */
    for (i = 0; i < 7; i++) tecla(SDLK_UP);        /* primeira fileira: "а" */
    tecla(SDLK_RETURN);                            /* а */
    tecla(SDLK_RIGHT); tecla(SDLK_RETURN);         /* б */
    assert(!strcmp(busca_consulta(), "\xd0\xb0\xd0\xb1"));
    { SDL_Event t;
      memset(&t, 0, sizeof t);
      t.type = SDL_TEXTINPUT;
      snprintf(t.text.text, sizeof t.text.text, "\xc8\x99");   /* ș */
      busca_evento(&t);
      assert(!strcmp(busca_consulta(), "\xd0\xb0\xd0\xb1\xc8\x99"));
      memset(&t, 0, sizeof t);
      t.type = SDL_TEXTINPUT;
      snprintf(t.text.text, sizeof t.text.text, "x");             /* ASCII: KEYDOWN cuida */
      busca_evento(&t);
      assert(!strcmp(busca_consulta(), "\xd0\xb0\xd0\xb1\xc8\x99")); }
    tecla(SDLK_BACKSPACE);                         /* tira o ș inteiro */
    assert(!strcmp(busca_consulta(), "\xd0\xb0\xd0\xb1"));
    snprintf(nome, sizeof nome, "%s-cirilico-digitado.bmp", saida);
    captura(nome); }
  // RESULTADOS E A ONDA (revela.h): o catalogo do pacote filtrado por duas
  // letras. A primeira foto sai no meio da entrada (os cards da direita ainda
  // subindo), a segunda com tudo assentado.
  ajustes_aplicar_blob("{\"tmdb_language\":\"en\"}");
  cat_carregar("deploy/app/art");
  { static CatFileira fl[2];
    int n = cat_n();
    snprintf(fl[0].chave, sizeof fl[0].chave, "shot.a");
    snprintf(fl[0].titulo, sizeof fl[0].titulo, "Popular");
    snprintf(fl[0].tipo, sizeof fl[0].tipo, "movie");
    fl[0].ini = 0; fl[0].n = n / 2;
    fl[1] = fl[0];
    snprintf(fl[1].chave, sizeof fl[1].chave, "shot.b");
    snprintf(fl[1].titulo, sizeof fl[1].titulo, "Trending");
    fl[1].ini = n / 2; fl[1].n = n - n / 2;
    cat_republicar_fileiras(fl, 2); }
  // CAMPO VAZIO COM CATALOGO: os Populares ocupam a coluna da direita. Sem
  // historico, e depois com as pilulas das buscas recentes em cima.
  buscasrec_limpar();
  busca_iniciar();
  snprintf(nome, sizeof nome, "%s-populares.bmp", saida);
  capturaEm(nome, 900);
  tecla(SDLK_RIGHT);   /* ultima coluna do teclado nao: so a ponte */
  { int i; for (i = 0; i < 6; i++) tecla(SDLK_RIGHT); }
  snprintf(nome, sizeof nome, "%s-populares-foco.bmp", saida);
  capturaEm(nome, 500);
  { static const char *termos[] = { "dune", "the office", "matrix", "stranger things", "cidade de deus" };
    int i;
    for (i = 0; i < 5; i++) buscasrec_registrar(termos[i]); }
  busca_iniciar();
  snprintf(nome, sizeof nome, "%s-populares-recentes.bmp", saida);
  capturaEm(nome, 900);
  { int i; for (i = 0; i < 6; i++) tecla(SDLK_RIGHT); }   /* pilulas */
  tecla(SDLK_DOWN);                                       /* desce aos Populares */
  snprintf(nome, sizeof nome, "%s-populares-recentes-foco.bmp", saida);
  capturaEm(nome, 500);
  buscasrec_limpar();
  // PESSOAS: o TMDB de mentira responde com as tres do mockup.
  spotpessoa_teste(tmdbFalso);
  busca_iniciar();
  tecla(SDLK_t); tecla(SDLK_h); tecla(SDLK_e);
  snprintf(nome, sizeof nome, "%s-resultados-onda.bmp", saida);
  capturaEm(nome, 170);
  snprintf(nome, sizeof nome, "%s-resultados.bmp", saida);
  capturaEm(nome, 1200);
  // Foco no melhor resultado e depois num cartaz da fileira de baixo.
  { int i; for (i = 0; i < 6; i++) tecla(SDLK_RIGHT); }
  snprintf(nome, sizeof nome, "%s-resultados-foco.bmp", saida);
  capturaEm(nome, 500);
  tecla(SDLK_DOWN); tecla(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-resultados-foco2.bmp", saida);
  capturaEm(nome, 500);
  // PESSOAS: a fileira entra logo depois da primeira de titulos; OK devolve o
  // mesmo pedido que o Spotlight devolve (SPOT_PESSOA).
  tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-pessoas-foco.bmp", saida);
  capturaEm(nome, 500);
  tecla(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-pessoas-foco2.bmp", saida);
  capturaEm(nome, 500);
  tecla(SDLK_RETURN);
  { SpotPedido pp;
    assert(busca_pediu_pessoa(&pp));
    assert(pp.tipo == SPOT_PESSOA && pp.indice < 0 && pp.tmdb == 1245);
    assert(pp.tituloTmdb == 24428 && !strcmp(pp.tituloTipo, "movie"));
    assert(!strcmp(pp.nome, "Scarlett Johansson"));
    assert(!busca_pediu_pessoa(&pp)); }
  spotpessoa_teste(NULL);
  // ANDROID (sistexto em modo de teste): CIMA da primeira fileira foca o campo,
  // direita o Falar; OK no campo chama o teclado do sistema e o texto dele
  // substitui o campo; Concluir leva aos resultados.
  st_teste_ligar(1);
  busca_iniciar();
  tecla(SDLK_UP);
  snprintf(nome, sizeof nome, "%s-android-campo.bmp", saida);
  captura(nome);
  tecla(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-android-falar.bmp", saida);
  captura(nome);
  tecla(SDLK_LEFT);
  tecla(SDLK_RETURN);
  assert(st_dono() == ST_BUSCA && st_estado() == ST_DIGITANDO);
  st_teste_evento("Ter ");
  quadro();
  assert(!strcmp(busca_consulta(), "er "));
  st_teste_evento("Der");
  quadro();
  assert(!strcmp(busca_consulta(), "er") && st_estado() == ST_PARADO);
  snprintf(nome, sizeof nome, "%s-android-concluiu.bmp", saida);
  captura(nome);
  busca_encerrar();
  st_teste_ligar(0);
  puts("PASS: capturas da Busca gravadas.");
  return 0;
}
