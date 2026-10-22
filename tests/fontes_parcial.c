// #221: "apertar Play leva 20-25 s ate as fontes". A folha enche a cada addon
// que responde e a escolha automatica nao espera o mais lento.
//
// addons.c, rede.c e streams.c REAIS, contra um servidor HTTP local de addons
// falsos com latencias diferentes (tests/fontes_parcial.sh o sobe):
//   ordem de instalacao  nome    resposta
//   0                    Lento   1,5 s, uma 4K
//   1                    Rapido  0,4 s, duas 1080p
//   2                    Quebrado  HTTP 500 na hora (e na segunda chance)
//   3                    Tardio  3 s, uma 720p
// O que se prova:
//   - as fontes do Rapido entram antes do Lento responder, e o que falta e
//     dito pelo nome (addons_faltam);
//   - quando o Lento chega, ele vai para CIMA na exibicao (ordem dos addons) e
//     os indices de quem ja estava nao mudam; o cartao em foco na folha
//     continua o mesmo;
//   - a escolha automatica: 1080p nao basta antes do prazo; o prazo libera; a
//     4K libera; "Primeira da lista" espera o addon instalado antes; a fonte
//     lembrada num addon que falta segura ate ele responder;
//   - no fim a lista NAO e substituida (mesmos indices, nada perdido).
// Com um diretorio como argumento grava capturas BMP da folha enchendo.
#include "addons.h"
#include "streams.h"
#include "ajustes.h"
#include "fonteauto.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "badges.h"
#include "rede.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static SDL_Window *janela;

static void captura(const char *dir, const char *nome) {
  char cam[700];
  int q;
  if (!janela) return;
  snprintf(cam, sizeof cam, "%s/%s.bmp", dir, nome);
  // A folha anima a entrada; alguns quadros ate assentar.
  for (q = 0; q < 40; q++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    stream_folha_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(.025f, .025f, .03f, 1); glClear(GL_COLOR_BUFFER_BIT);
    stream_folha_desenhar(SDL_GetTicks());
    if (q == 39) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_ABGR8888);
      int y;
      assert(pix && s);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, cam) == 0);
      SDL_FreeSurface(s); free(pix);
    }
    SDL_GL_SwapWindow(janela);
  }
  printf("captura: %s\n", cam);
}

static void tecla(SDL_Keycode k) {
  SDL_Event e = { 0 };
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  stream_folha_evento(&e);
}

// Espera (drenando como o laco do app) ate `cond` ou `ms`.
static Uint32 t0;
#define ESPERA(cond, ms) do { Uint32 _l = SDL_GetTicks() + (ms); \
  while (!(cond) && SDL_GetTicks() < _l) { addons_estado(); \
    stream_folha_atualizar(1.0f / 60.0f, SDL_GetTicks()); SDL_Delay(5); } } while (0)

static int indiceDe(const char *prov, int k) {
  int i, j = 0;
  for (i = 0; i < stream_n(); i++)
    if (!strcmp(stream_item(i)->provedor, prov) && j++ == k) return i;
  return -1;
}

static void ajustes(const char *dir, int primeira) {
  char cam[600];
  FILE *f;
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dir);
  f = fopen(cam, "w"); assert(f);
  // fonteAutoLocal 0 = Melhor fonte; qualidade Automatica; espera 5 s.
  fprintf(f, "idioma 0\nselected_theme 2\nfonteAutoLocal %d\nfontePrazoLocal 1\n", primeira);
  fclose(f);
  ajustes_dir(dir);
}

int main(int argc, char **argv) {
  const char *porta = getenv("NV_PORTA");
  const char *dados = getenv("NUVIO_DADOS");
  const char *shots = argc > 1 ? argv[1] : NULL;
  AddonRemoto a[4];
  char nomes[256];
  int i, iA0, iA1, iL, falta, escolhida;
  Uint32 tPrimeira, tLento, tFim;
  assert(porta && dados);
  ajustes(dados, 0);
  assert(ajustes_fonte_prazo_ms() == 5000);

  if (shots) {
    SDL_GLContext gl;
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    janela = SDL_CreateWindow("Nuvio: fontes chegando", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, 1920, 1080,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    assert(janela);
    gl = SDL_GL_CreateContext(janela); assert(gl);
    SDL_GL_SetSwapInterval(0);
    glViewport(0, 0, 1920, 1080);
    gfx_tamanho_alvo(1920, 1080);
    assert(gfx_iniciar());
    assert(txt_iniciar("deploy/app", 1));
    tex_iniciar(64);
    gfx_icones_dir("deploy/app/art");
    badges_carregar("deploy/app/art");
  } else assert(SDL_Init(SDL_INIT_TIMER) == 0);
  rede_preparar();

  memset(a, 0, sizeof a);
  { const char *nm[4] = { "Lento", "Rapido", "Quebrado", "Tardio" };
    for (i = 0; i < 4; i++) {
      snprintf(a[i].nome, sizeof a[i].nome, "%s", nm[i]);
      snprintf(a[i].url, sizeof a[i].url, "http://127.0.0.1:%s/%s/manifest.json", porta, nm[i]);
      a[i].ativo = 1;
    } }
  assert(addons_definir_lista(a, 4) == 1);

  // ---- 1. a lista enche aos poucos -------------------------------------
  t0 = SDL_GetTicks();
  stream_definir_alvo("tt0000221");
  addons_buscar("tt0000221", "movie");
  stream_folha_contexto("Teste #221");
  stream_folha_abrir();
  if (shots) captura(shots, "1-vazia-buscando");
  ESPERA(stream_n() >= 2, 3000);
  tPrimeira = SDL_GetTicks() - t0;
  assert(stream_n() == 2);
  assert(addons_estado() == ADD_BUSCANDO && addons_busca_parcial());
  falta = addons_faltam(nomes, sizeof nomes);
  printf("primeiras fontes aos %u ms; faltam %d: %s\n", tPrimeira, falta, nomes);
  assert(tPrimeira < 1200);
  assert(falta >= 2 && strstr(nomes, "Lento") && strstr(nomes, "Tardio"));
  iA0 = indiceDe("Rapido", 0); iA1 = indiceDe("Rapido", 1);
  assert(iA0 == 0 && iA1 == 1);

  // Escolha automatica com so 1080p: nao e "boa" (teto automatico = 4K).
  assert(!stream_auto_pode_decidir(-1, 0, 0, 0));
  assert(stream_auto_pode_decidir(-1, 0, 1, 0));          // o prazo libera
  assert(!stream_auto_pode_decidir(-1, 1, 1, 0));         // lembrada pendente segura
  assert(stream_auto_pode_decidir(1, 1, 0, 0));           // lembrada presente vai
  puts("ok  com so as 1080p: espera o prazo; a lembrada pendente segura; a presente vai");

  // A pessoa desce ate a SEGUNDA do Rapido (linha 1) antes de o Lento chegar.
  // A folha abre com o foco na lista, linha 0.
  tecla(SDLK_DOWN);
  ESPERA(0, 50);
  if (shots) captura(shots, "2-rapido-chegou");

  // ---- 2. o Lento chega: entra em cima, ninguem muda de indice -----------
  ESPERA(stream_n() >= 3, 4000);
  tLento = SDL_GetTicks() - t0;
  assert(stream_n() == 3);
  iL = indiceDe("Lento", 0);
  assert(iL == 2 && indiceDe("Rapido", 0) == 0 && indiceDe("Rapido", 1) == 1);
  assert(stream_ordem_addon(iL) == 0 && stream_ordem_addon(0) == 1);
  printf("Lento (4K) aos %u ms, no indice %d, exibido primeiro\n", tLento, iL);
  // Automatico: a 4K (Lento) e a escolhida e ja e "boa".
  assert(stream_automatico() == iL);
  assert(stream_auto_pode_decidir(-1, 0, 0, 0));
  if (shots) captura(shots, "3-lento-chegou-foco-parado");
  // O foco nao pulou: OK escolhe a mesma fonte (a segunda do Rapido).
  tecla(SDLK_RETURN);
  assert(stream_folha_escolheu(&escolhida) && escolhida == iA1);
  puts("ok  o Lento entrou acima, o cartao em foco continuou o mesmo");

  // ---- 3. "Primeira da lista": espera quem foi instalado antes -----------
  ajustes(dados, 1);
  assert(ajustes_fonte_primeira());
  // Com o Lento ja na lista, a primeira (dele) nao muda mais.
  assert(stream_auto_pode_decidir(-1, 0, 0, 0));
  ajustes(dados, 0);

  // ---- 4. fim: nada substituido ------------------------------------------
  ESPERA(addons_estado() != ADD_BUSCANDO, 8000);
  tFim = SDL_GetTicks() - t0;
  assert(addons_estado() == ADD_PRONTO);
  assert(!addons_busca_parcial() && addons_faltam(NULL, 0) == 0);
  assert(stream_n() == 4);
  assert(indiceDe("Rapido", 1) == 1 && indiceDe("Lento", 0) == 2 && indiceDe("Tardio", 0) == 3);
  printf("busca completa aos %u ms (a lista ja tinha fontes desde %u ms)\n", tFim, tPrimeira);
  assert(tFim > 2500);
  stream_folha_abrir();
  if (shots) captura(shots, "4-completa");
  puts("ok  fim da busca nao troca a lista: indices e fontes preservados");

  // ---- 5. "Primeira da lista" com o addon da frente lento ---------------
  ajustes(dados, 1);
  stream_definir_alvo("tt0000222");
  addons_buscar("tt0000222", "movie");
  ESPERA(stream_n() >= 2, 3000);
  assert(stream_n() == 2 && addons_busca_parcial());
  assert(!stream_auto_pode_decidir(-1, 0, 0, 0));          // o Lento (antes) falta
  ESPERA(stream_n() >= 3, 4000);
  assert(stream_auto_pode_decidir(-1, 0, 0, 0));
  puts("ok  Primeira da lista espera o addon instalado antes, nao o de depois");
  ESPERA(addons_estado() != ADD_BUSCANDO, 8000);

  addons_encerrar();
  if (shots) { tex_encerrar(); txt_encerrar(); gfx_encerrar(); }
  SDL_Quit();
  puts("fontes_parcial: tudo ok");
  return 0;
}
