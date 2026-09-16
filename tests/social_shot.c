// CAPTURA DO PAINEL DA TECLA AZUL com a aba SOCIAL, sem interacao e sem rede.
//
// Existe pelo motivo que tests/ajustes_shot.c ja registra: interface de TV
// julgada so por codigo sai ilegivel a 3 m. Aqui as duas abas sao desenhadas
// com dados de mentira e gravadas em BMP, para serem OLHADAS.
//
// OS DADOS ENTRAM PELO DISCO, e nao por uma porta de teste: o teste escreve
// `recomendacoes.txt` e `salvos.txt` dentro de NUVIO_DADOS e deixa os modulos
// os lerem como leriam no arranque. Assim a captura tambem prova o FORMATO do
// arquivo — se ele mudar e a leitura nao acompanhar, a aba sai vazia na foto.
//
// A ARTE E LOCAL (deploy/app/art/*.jpg): tex_obter carrega caminho local sem
// rede, e uma captura que depende do metahub falha no primeiro avaio offline.
// A URL compilada e o que liga a aba: sem ela recomenda_ativo() e 0 e o painel
// desenha o de sempre — que e o comportamento certo do pacote sem servico e
// nao o que esta foto quer provar.
#define NV_REC_URL "http://127.0.0.1:8799"
#include "../src/recomenda.c"
#include "salvospainel.h"
#include "salvos.h"
#include "ctxmenu.h"
#include "catalogo.h"
#include "dados.h"
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
#include <time.h>

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  spainel_evento(&e);
}

static void teclaCtx(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  ctx_evento(&e);
}

// `cartao` = 1 desenha o aviso de abertura em vez do painel. As duas telas
// compartilham o laco porque a captura e a mesma; o que muda e quem desenha.
enum { DES_PAINEL = 0, DES_CARTAO, DES_CTX };
static int desenharCartao;

static void captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 150; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    spainel_atualizar(1.0f / 60.0f, SDL_GetTicks());
    recomenda_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ctx_atualizar(1.0f / 60.0f, SDL_GetTicks());
    if (desenharCartao == DES_CARTAO)     recomenda_desenhar(SDL_GetTicks());
    else if (desenharCartao == DES_CTX)   ctx_desenhar(SDL_GetTicks());
    else                                  spainel_desenhar(SDL_GetTicks());
    if (i == 149) {
      unsigned char *pix = (unsigned char *)malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4,
               1920 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

// Escreve o cache como o app o escreveria. Os campos, em ordem:
// id, criado, visto, modelo, tipo, ano, de, deNome, imdb, poster, texto, titulo.
static void semear(const char *dir) {
  char caminho[700];
  long long agora = (long long)time(NULL);
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/recomendacoes.txt", dir);
  f = fopen(caminho, "wb");
  assert(f);
  fprintf(f, "# nuvio recomendacoes v1\n");
  fprintf(f, "7\t%lld\t0\t2\tmovie\t1994\ttrakt:gustavo\tGustavo\ttt0111161\t"
             "deploy/app/art/00.jpg\t\tUm Sonho de Liberdade\n", agora - 900);
  fprintf(f, "6\t%lld\t0\t-1\tseries\t2008\tnuvio:9a1c\tMarina\ttt0903747\t"
             "deploy/app/art/01.jpg\tisso e melhor que tudo\tBreaking Bad\n",
          agora - 9000);
  fprintf(f, "5\t%lld\t1\t4\tmovie\t2014\ttrakt:gustavo\tGustavo\ttt2582802\t"
             "deploy/app/art/02.jpg\t\tWhiplash: Em Busca da Perfeição\n",
          agora - 200000);
  fprintf(f, "4\t%lld\t1\t0\tseries\t2016\tnuvio:3b2d\tCarolina Menezes\t"
             "tt4574334\tdeploy/app/art/03.jpg\t\tStranger Things\n",
          agora - 400000);
  fclose(f);

  snprintf(caminho, sizeof caminho, "%s/salvos.txt", dir);
  f = fopen(caminho, "wb");
  assert(f);
  fprintf(f, "# nuvio salvos v1\n");
  fprintf(f, "tt0110912\tmovie\t%lld\t89\t1994\tdeploy/app/art/04.jpg\t"
             "Pulp Fiction: Tempo de Violência\n", agora - 3600);
  fprintf(f, "tt0068646\tmovie\t%lld\t92\t1972\tdeploy/app/art/05.jpg\t"
             "O Poderoso Chefão\n", agora - 90000);
  fprintf(f, "tt0944947\tseries\t%lld\t92\t2011 · 8 temporadas\t"
             "deploy/app/art/06.jpg\tGame of Thrones\n", agora - 500000);
  fclose(f);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-social";
  const char *dir = getenv("NUVIO_DADOS");
  char nome[700];
  SDL_Window *w;
  SDL_GLContext gl;

  // A MESMA TRAVA de tests/recomenda.sh, e pelo mesmo motivo: esta captura
  // ESCREVE recomendacoes.txt e salvos.txt. Sem a conferencia ela escreveria
  // por cima da lista de verdade de quem a executa.
  if (!dir || !dir[0]) {
    printf("social_shot: NUVIO_DADOS nao esta no ambiente; recusando rodar\n");
    return 2;
  }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) {
    printf("social_shot: dados_dir() e \"%s\" e NUVIO_DADOS e \"%s\"; recusando\n",
           dados_dir(), dir);
    return 2;
  }
  semear(dir);

  // IDIOMA DO DISCO, pelo caminho real. O padrao de fabrica e o INGLES
  // (ajustes.c:620) e quem revisa estas capturas le portugues — sem isto a
  // aba sai "SAVED" e a frase do modelo sai "Trust me". "selected_theme 2" e
  // o acento OCEANO: o anel da linha de abas tem de sair AZUL, provando que
  // ele veio de ajustes_acento e nao de um branco cravado.
  { char caminho[700]; FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
    f = fopen(caminho, "w");
    assert(f);
    fprintf(f, "idioma 0\nselected_theme 2\n");
    fclose(f);
    ajustes_dir(dados_dir()); }

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: aba Social", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);

  salvos_iniciar();
  recomenda_iniciar();
  printf("semeado: %d salvos, %d recomendacoes (%d novas)\n",
         salvos_n(), recomenda_n(), recomenda_n_novas());

  // O CARTAO DE ABERTURA PRIMEIRO: ele so aparece com recomendacao NAO VISTA, e
  // abrir a aba Social marca todas como vistas. Na ordem contraria a captura
  // sairia em branco e nao haveria como distinguir isso de um defeito.
  recomenda_mostrar_se_houver();
  printf("cartao aberto: %d\n", recomenda_aberta());
  desenharCartao = DES_CARTAO;
  snprintf(nome, sizeof nome, "%s-cartao.bmp", saida);
  captura(nome, w);
  desenharCartao = DES_PAINEL;

  spainel_abrir();
  snprintf(nome, sizeof nome, "%s-salvos.bmp", saida);
  captura(nome, w);

  // CIMA leva o foco para a linha de abas; DIREITA troca para SOCIAL. E
  // exatamente o caminho que o D-pad da TV percorre.
  tecla(SDLK_UP);
  tecla(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-social.bmp", saida);
  captura(nome, w);

  // E com o foco JA na lista, que e o estado em que a pessoa passa mais tempo.
  tecla(SDLK_DOWN);
  tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-social-foco.bmp", saida);
  captura(nome, w);

  // --- AS DUAS TELAS DE ENVIO, no menu de contexto do cartaz ---------------
  //
  // Os contatos sao semeados DENTRO do modulo: a lista so existe depois de um
  // ciclo de rede, e uma captura nao deve depender de servidor no ar.
  { CatItem ci;
    memset(&ci, 0, sizeof ci);
    snprintf(ci.imdb, sizeof ci.imdb, "%s", "tt0111161");
    snprintf(ci.tipo, sizeof ci.tipo, "%s", "movie");
    snprintf(ci.titulo, sizeof ci.titulo, "%s", "Um Sonho de Liberdade");
    snprintf(ci.poster, sizeof ci.poster, "%s", "deploy/app/art/00.jpg");
    snprintf(ci.meta, sizeof ci.meta, "%s", "1994 · 2h22");
    cat_definir_tudo(&ci, 1, NULL, 0); }
  nContatos = 4;
  snprintf(contatos[0].nome, sizeof contatos[0].nome, "%s", "Gustavo");
  snprintf(contatos[0].id,   sizeof contatos[0].id,   "%s", "trakt:gustavo");
  snprintf(contatos[1].nome, sizeof contatos[1].nome, "%s", "Marina Duarte");
  snprintf(contatos[1].id,   sizeof contatos[1].id,   "%s", "nuvio:9a1c");
  snprintf(contatos[2].nome, sizeof contatos[2].nome, "%s", "Carolina Menezes");
  snprintf(contatos[2].id,   sizeof contatos[2].id,   "%s", "nuvio:3b2d");
  snprintf(contatos[3].nome, sizeof contatos[3].nome, "%s", "pedrinho_23");
  snprintf(contatos[3].id,   sizeof contatos[3].id,   "%s", "trakt:pedrinho-23");

  desenharCartao = DES_CTX;
  ctx_abrir(0);
  // O OK que abriu o menu ainda esta "afundado"; um KEYUP o solta, como na TV.
  { SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYUP;
    e.key.keysym.sym = SDLK_RETURN;
    ctx_evento(&e); }
  // Desce ate "Recomendar a um amigo". Sao QUATRO opcoes neste item (detalhes,
  // salvar, assistido, recomendar) — o titulo nao tem progresso, entao "Tirar
  // de Continuar assistindo" nao aparece.
  teclaCtx(SDLK_DOWN); teclaCtx(SDLK_DOWN); teclaCtx(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-ctx-opcoes.bmp", saida);
  captura(nome, w);

  teclaCtx(SDLK_RETURN);
  teclaCtx(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-ctx-contatos.bmp", saida);
  captura(nome, w);

  teclaCtx(SDLK_RETURN);
  teclaCtx(SDLK_DOWN); teclaCtx(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-ctx-modelos.bmp", saida);
  captura(nome, w);

  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
