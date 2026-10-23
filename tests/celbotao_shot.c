// O BOTAO "DIGITAR PELO CELULAR" (celbotao.h) no Spotlight, na Busca e no
// teclado do app: capturas para o olho e as conferencias de ponta a ponta.
//
// O que cada foto prova:
//   -spot-barra      o disco do celular no fim da barra do Spotlight, sem foco;
//   -spot-botao      DIREITA a partir do campo acende o botao (foco cheio);
//   -spot-cartao     OK: o cartao com QR por cima, ancorado no botao, sem mexer
//                    na barra;
//   -spot-recebido   o texto do celular (curl) entrou no campo, a busca rodou e
//                    o foco foi para os resultados;
//   -spot-expirou    o cartao depois de CEL_VALIDADE_S (o .sh compila com 3 s);
//   -busca-botao     a Busca sem teclado do sistema: CIMA da grade chega no botao;
//   -busca-cartao    o cartao por cima da Busca;
//   -busca-recebido  o texto entrou no campo e os resultados apareceram;
//   -guia / -salvos / -apelido / -rpdb   o teclado do app com o titulo, a dica
//                    e o alfabeto de cada lugar que o abre, botao no fim do campo.
// E confere, sem olho: o servidor so sobe com o cartao (abrir o campo nao abre
// porta), Origin de fora e recusado, o token vale um envio, Voltar e clique
// fora fecham o cartao, o clique do ponteiro no botao abre o cartao.
#include "spotlight.h"
#include "nvtmp.h"
#include "busca.h"
#include "teclado.h"
#include "celbotao.h"
#include "celular.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "catalogo.h"
#include "ponteiro.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static SDL_Window *janela;
static int tela;   // 0 Spotlight, 1 Busca, 2 teclado
static char dirArte[600];

// Como app.c: o cartao aberto come a tecla antes da tela.
static void rotear(const SDL_Event *e) {
  if (celb_evento(e)) return;
  if (tela == 0) spot_evento(e);
  else if (tela == 1) busca_evento(e);
  else teclado_evento(e);
}
static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  rotear(&e);
  e.type = SDL_KEYUP;
  rotear(&e);
}

static void fundo(void) {
  int r, c;
  char cam[700];
  gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, 0.07f, 0.07f, 0.08f, 1.0f);
  for (r = 0; r < 3; r++)
    for (c = 0; c < 8; c++) {
      GfxRect p = { 104.0f + c * 232.0f, 120.0f + r * 330.0f, 208.0f, 312.0f };
      GLuint t;
      snprintf(cam, sizeof cam, "%s/poster/%02d.jpg", dirArte, (r * 8 + c) % 40);
      t = tex_obter_larg(cam, p.w);
      if (t) { gfx_tex_aspect_atual = tex_aspecto(cam);
               gfx_rect(p, t, GFX_CARD, 0, 0, 0, 0.06f, 0, 0, 0, 1.0f);
               gfx_tex_aspect_atual = 0; }
    }
}

static void quadro(void) {
  Uint32 agora = SDL_GetTicks();
  SDL_PumpEvents();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(16);
  gfx_novo_quadro();
  ponteiro_quadro(agora);
  if (tela == 0) spot_atualizar(1.0f / 60.0f, agora);
  else if (tela == 1) busca_atualizar(1.0f / 60.0f, agora);
  else teclado_atualizar(1.0f / 60.0f, agora);
  celb_atualizar(1.0f / 60.0f);
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  if (tela == 0) { fundo(); spot_desenhar(agora, 0); }
  else if (tela == 1) busca_desenhar(agora);
  else { fundo(); teclado_desenhar(agora); }
  celb_desenhar();
  ponteiro_desenhar();
  SDL_GL_SwapWindow(janela);
}
static void quadros(int n) { while (n-- > 0) quadro(); }

static void captura(const char *saida, const char *nome) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  char arq[700];
  SDL_Surface *s;
  int y;
  assert(pix);
  quadros(50);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(arq, sizeof arq, "%s-%s.bmp", saida, nome);
  assert(SDL_SaveBMP(s, arq) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", arq);
}

// POST pelo curl de verdade; devolve o codigo HTTP (000 = conexao recusada).
static int enviar(const char *url, const char *texto, const char *origem) {
  char cmd[700], saida[16] = { 0 };
  FILE *p;
  snprintf(cmd, sizeof cmd,
           "curl -s -m 5 -o /dev/null -w '%%{http_code}' %s%s%s --data-urlencode 't=%s' '%s'",
           origem ? "-H 'Origin: " : "", origem ? origem : "", origem ? "'" : "", texto, url);
  p = popen(cmd, "r");
  assert(p);
  if (!fgets(saida, sizeof saida, p)) saida[0] = 0;
  pclose(p);
  return atoi(saida);
}

// Ponteiro de verdade (o mesmo caminho do Magic Remote): dois movimentos
// acordam o cursor, o clique vira OK entregue a quem tem o foco.
static void mover(int x, int y) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEMOTION; e.motion.x = x; e.motion.y = y; e.motion.xrel = 40; e.motion.yrel = 40;
  ponteiro_evento(&e, rotear);
}
static void clicar(int x, int y) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_LEFT; e.button.x = x; e.button.y = y;
  ponteiro_evento(&e, rotear);
  e.type = SDL_MOUSEBUTTONUP;
  ponteiro_evento(&e, rotear);
}
static void apontar(int x, int y) {
  mover(x - 80, y - 80); quadros(2);
  mover(x - 20, y - 20); quadros(2);
  mover(x, y); quadros(3);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-celbotao");
  const char *dir = getenv("NUVIO_DADOS");
  SDL_GLContext gl;
  char cwd[400], url[128];
  if (!dir || !*dir) return 2;
  assert(getcwd(cwd, sizeof cwd));
  snprintf(dirArte, sizeof dirArte, "%s/deploy/app/art", cwd);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: botao do celular", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(160);
  gfx_icones_dir("deploy/app/art");
  dados_iniciar(dir);
  ajustes_iniciar();
  cat_carregar("deploy/app/art");
  // Duas fileiras "de addon" com o catalogo do pacote (como spotlight_shot.c):
  // e de onde o Spotlight tira os titulos.
  { static CatFileira fl[2];
    int n = cat_n();
    snprintf(fl[0].chave, sizeof fl[0].chave, "shot.a");
    snprintf(fl[0].titulo, sizeof fl[0].titulo, "Top 10 de hoje - Filme");
    snprintf(fl[0].tipo, sizeof fl[0].tipo, "movie");
    snprintf(fl[0].base, sizeof fl[0].base, "https://addon.exemplo");
    snprintf(fl[0].catId, sizeof fl[0].catId, "top");
    fl[0].ini = 0; fl[0].n = n / 2;
    fl[1] = fl[0];
    snprintf(fl[1].chave, sizeof fl[1].chave, "shot.b");
    snprintf(fl[1].catId, sizeof fl[1].catId, "novos");
    fl[1].ini = n / 2; fl[1].n = n - n / 2;
    cat_republicar_fileiras(fl, 2); }
  ponteiro_teste_janela(1920, 1080);
  ponteiro_iniciar();
  assert(celb_disponivel());

  // --- SPOTLIGHT ---------------------------------------------------------------
  tela = 0;
  spot_abrir(0);
  assert(spot_foco_campo() == 1 && celular_estado() == CEL_PARADO);
  captura(saida, "spot-barra");
  tecla(SDLK_RIGHT);   // Mac: sem Falar, o campo vai direto ao celular
  assert(spot_foco_campo() == 3);
  captura(saida, "spot-botao");
  tecla(SDLK_RETURN);
  assert(celb_aberto() && celb_dono() == CELB_SPOT && celular_estado() == CEL_ESPERANDO);
  captura(saida, "spot-cartao");
  snprintf(url, sizeof url, "%s", celular_url());
  // Origem de fora: recusado, o cartao continua esperando.
  assert(enviar(url, "nada", "http://malicioso.exemplo") == 403);
  quadros(3);
  assert(celb_aberto() && !spot_consulta()[0]);
  assert(enviar(url, "the", NULL) == 200);
  quadros(3);
  assert(!celb_aberto() && celular_estado() == CEL_PARADO);
  assert(!strcmp(spot_consulta(), "the"));
  assert(spot_linha_focada() >= 0);   // a busca rodou e o foco foi aos resultados
  // Token de uso unico: o servidor ja fechou.
  assert(enviar(url, "de novo", NULL) == 0);
  captura(saida, "spot-recebido");
  puts("ok: Spotlight recebeu 'the' do celular e buscou");

  // Voltar fecha o cartao sem fechar o Spotlight.
  tecla(SDLK_ESCAPE);   // lista -> campo
  tecla(SDLK_RIGHT);
  tecla(SDLK_RETURN);
  assert(celb_aberto());
  tecla(SDLK_ESCAPE);
  assert(!celb_aberto() && spot_aberto() && celular_estado() == CEL_PARADO);

  // PONTEIRO: o clique no botao abre o cartao; clique fora fecha.
  { GfxRect c;
    apontar(1522, 168);   // centro do disco: SP_BX + SP_BW - 26 - 32, SP_BY + 52
    assert(spot_foco_campo() == 3);
    clicar(1522, 168);
    quadros(3);
    assert(celb_aberto());
    c = celb_cartao_rect();
    assert(c.w > 0 && c.y > 168);   // abaixo do botao
    clicar(200, 900);
    quadros(2);
    assert(!celb_aberto() && spot_aberto());
    puts("ok: ponteiro abre e fecha o cartao"); }

  // EXPIRADO (CEL_VALIDADE_S = 3 no .sh): o cartao diz, e OK gera outro.
  tecla(SDLK_RIGHT);
  tecla(SDLK_RETURN);
  assert(celb_aberto());
  { Uint32 t0 = SDL_GetTicks(); while (SDL_GetTicks() - t0 < 4500) quadro(); }
  assert(celular_estado() == CEL_EXPIROU);
  captura(saida, "spot-expirou");
  tecla(SDLK_RETURN);
  assert(celb_aberto() && celular_estado() == CEL_ESPERANDO);
  tecla(SDLK_ESCAPE);
  spot_fechar();
  quadros(40);
  assert(celular_estado() == CEL_PARADO);

  // --- BUSCA ---------------------------------------------------------------------
  tela = 1;
  busca_iniciar();
  tecla(SDLK_UP);   // primeira fileira -> sem teclado do sistema, o botao
  assert(busca_foco_campo() == 3);
  captura(saida, "busca-botao");
  tecla(SDLK_RETURN);
  assert(celb_aberto() && celb_dono() == CELB_BUSCA);
  captura(saida, "busca-cartao");
  snprintf(url, sizeof url, "%s", celular_url());
  assert(enviar(url, "  Matrix \n", NULL) == 200);
  quadros(3);
  assert(!celb_aberto() && !strcmp(busca_consulta(), "Matrix"));
  captura(saida, "busca-recebido");
  busca_encerrar();
  puts("ok: Busca recebeu 'Matrix' do celular");

  // --- TECLADO DO APP, nos lugares que o abrem -----------------------------------
  tela = 2;
  { static const struct { const char *nome, *titulo, *dica, *alfa; int max; } L[] = {
      { "guia", "Buscar no guia", "Nome ou número do canal, ou o nome de um programa",
        "abcdefghijklmnopqrstuvwxyz0123456789 ", 32 },
      { "salvos", "Nova categoria", "Um nome curto", NULL, 24 },
      { "apelido", "Apelido", "Como as pessoas vão te achar. Use - entre as palavras.",
        "abcdefghijklmnopqrstuvwxyz0123456789-", 20 },
      { "rpdb", "Chave do RPDB", "Sua chave em ratingposterdb.com. Vazio apaga.",
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_", 64 } };
    int i;
    for (i = 0; i < 4; i++) {
      char n[64];
      teclado_abrir_com(L[i].titulo, L[i].dica, L[i].max, L[i].alfa, NULL);
      assert(celular_estado() == CEL_PARADO);
      tecla(SDLK_LEFT);   // a coluna do campo fica a esquerda da grade
      assert(teclado_foco_campo() == 3);
      captura(saida, L[i].nome);
      if (i == 0) {
        tecla(SDLK_RETURN);
        snprintf(url, sizeof url, "%s", celular_url());
        assert(enviar(url, "Globo News", NULL) == 200);
        quadros(3);
        assert(!strcmp(teclado_texto(), "globo news"));   // filtrado pelo alfabeto do guia
        snprintf(n, sizeof n, "%s-recebido", L[i].nome);
        captura(saida, n);
      }
      tecla(SDLK_ESCAPE);
      quadros(30);
    }
    puts("ok: teclado do app (guia, salvos, apelido, rpdb) com o botao"); }

  puts("PASS: botao do celular no Spotlight, Busca e teclado.");
  return 0;
}
