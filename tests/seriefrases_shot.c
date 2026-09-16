// CAPTURA DOS PAINEIS DE FRASES E DE FICHA DE PRODUCAO, sem rede.
//
// Mesmo motivo de tests/serieaud_shot.c: painel de texto longo numa TV e
// julgado por olho, nao por assercao. O que esta sob olhar aqui e (a) se a
// superficie de foco acompanha a altura da fala em vez de ter altura fixa, (b)
// se a fala SELECIONADA le como selecionada no vocabulario desta base —
// superficie clara preenchida, texto escuro, sem contorno — (c) se a coluna da
// aspa segura o alinhamento vertical de falas de 1, 2 e 3 linhas, e (d) se o
// credito da fonte cabe.
//
// AS FALAS AQUI SAO TEXTO NEUTRO DE MEDIDA, e isso e DE PROPOSITO — nao e
// preguica de copiar a fonte. Duas razoes, as duas praticas:
//
//   1. A captura existe para julgar FORMA, e uma fala famosa e o pior material
//      possivel para isso: quem olha le a fala, reconhece o filme e para de ver
//      a tipografia. Texto que nao diz nada devolve o olho para a quebra de
//      linha, a entrelinha e a coluna, que e o que esta sob julgamento.
//   2. O comprimento aqui e ESCOLHIDO para bater nos limites do desenho — uma
//      linha, duas, tres, e uma que estoura as tres e tem de fechar com "…".
//      Fala de verdade cai onde cai, e nao cobre os quatro casos.
//
// O caminho de dados REAL continua coberto, e por teste de verdade:
// tests/seriefrases.c usa resposta do Wikidata e wikitexto do Wikiquote
// copiados byte a byte do fio. Esta captura nao e sobre dado, e sobre desenho.
//
// NAO CHAMA dados_iniciar DE PROPOSITO: sem ela nada e lido nem gravado.
#include "seriefrases.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "layout.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/seriefrases.c"

typedef struct { const char *quem, *texto; } Fx;

// AS CINCO FORMAS QUE O DESENHO PRECISA AGUENTAR, uma por linha, na ordem em
// que aparecem na captura. O NOME de quem fala e generico pelo mesmo motivo do
// texto: "Personagem A" nao distrai, e o que importa nele e o comprimento.
//
//   0  tres linhas cheias           -> a superficie de foco tem de ter tres
//                                      linhas de altura, nao uma fixa
//   1  uma linha curta              -> a superficie encolhe junto; e o caso em
//                                      que uma altura fixa apareceria na hora
//   2  duas linhas SEM QUEM FALOU   -> a atribuicao some e nao deixa buraco
//                                      nem empurra a proxima citacao
//   3  estoura as tres linhas       -> tem de fechar com "…" em vez de acabar
//                                      no meio de uma palavra
//   4  duas linhas com aspas DENTRO -> a fala do Wikiquote muitas vezes ja traz
//                                      aspas de dialogo no meio; a coluna da
//                                      aspa grande NAO pode dancar por causa
//                                      disso
static const Fx FALAS[] = {
  { "Personagem A",
    "Esta primeira fala de medida e comprida o bastante para ocupar tres "
    "linhas inteiras da coluna, e existe para mostrar que a superficie de foco "
    "cresce junto com o texto em vez de ter uma altura combinada de antemao." },
  { "Personagem B", "Uma fala curta de uma linha so." },
  { "",
    "Esta fala nao tem nome de quem falou, que e o caso de perto de metade do "
    "que vem do Wikiquote, e por isso ela termina no fim do texto." },
  { "Personagem C com nome comprido",
    "Esta ultima fala de medida passa das tres linhas de proposito, e enche "
    "muito mais coluna do que cabe, para que o corte apareca na captura: o que "
    "se quer ver aqui e a reticencia fechando a terceira linha no fim de uma "
    "palavra inteira, e nunca no meio de uma, porque quem le precisa saber que "
    "o que falta foi o aplicativo que aparou." },
  { "Personagem D",
    "Uma fala de medida com \"aspas de dialogo\" dentro dela, para conferir "
    "que a coluna da aspa grande continua no mesmo lugar." }
};
// A ficha vem com os rotulos ja em portugues porque e assim que addFato os
// recebe do modulo (ele passa por i18n antes). Os valores sao inventados e
// redondos; o que importa aqui e o COMPRIMENTO da lista de premios, que e o
// campo que mais quebra linha no painel estreito.
static const Fx FICHA[] = {
  { "Orçamento", "US$ 185 mi" },
  { "Bilheteria", "US$ 1.01 bi" },
  { "Filmado em", "Cidade Um · Estúdio Dois (estúdios) · Cidade Três · Cidade Quatro" },
  { "Baseado em", "Obra de exemplo" },
  { "Prêmios", "Prêmio de exemplo de melhor ator secundário · Prêmio de exemplo de melhor edição de som · Prêmio de exemplo de melhor vilão" }
};

static void carregar(const Fx *q, int nq, const Fx *f, int nf,
                     const char *pagina, int pt, int sel) {
  int i;
  nFrases = nFatos = 0;
  for (i = 0; i < nq && i < SF_FRASE_MAX; i++) {
    snprintf(frases[i].quem, SF_QUEM, "%s", q[i].quem);
    snprintf(frases[i].texto, SF_TEXTO, "%s", q[i].texto);
    nFrases++;
  }
  // Passa pelo addFato de verdade: e ele que decide o corte de uma lista longa,
  // e a captura existe justamente para olhar esse corte.
  for (i = 0; i < nf; i++) {
    char v[300];
    snprintf(v, sizeof v, "%s", f[i].texto);
    primeirosItens(v, 3);
    addFato(f[i].quem, v);
  }
  snprintf(paginaWq, sizeof paginaWq, "%s", pagina);
  emPortugues = pt;
  selecionado = sel;
}

static void gravar(const char *nome, SDL_Window *win) {
  unsigned char *pix = (unsigned char *)malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  SDL_GL_SwapWindow(win);
  printf("captura: %s  (%d desenhos gfx no quadro)\n", nome, gfx_n_rect);
}

static void tela(void) {
  GfxRect a = { NV_CONTENT_PAD, 70.0f, 1060.0f, 0 };
  GfxRect b = { NV_CONTENT_PAD + 1120.0f, 70.0f,
                NV_TELA_W - NV_CONTENT_PAD * 2 - 1120.0f, 0 };
  seriefrases_desenhar(a);
  seriefrases_desenhar_fatos(b);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-seriefrases";
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  int i;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: frases e ficha", SDL_WINDOWPOS_CENTERED,
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
  tex_iniciar(16);

  // UMA CAPTURA POR FORMA DE FOCO, e nao uma so. A superficie de foco e o
  // unico retangulo da lista e a altura dela sai da quebra do texto: se ela
  // errar, erra DIFERENTE em cada forma de fala, e uma captura unica esconderia
  // tres dos quatro erros. As tres primeiras mostram a MESMA lista com o foco
  // andando, entao cada uma tambem serve de captura do estado NAO focado das
  // outras quatro falas.
  { static const struct { int sel; const char *nome; } CASOS[] = {
      { 0, "foco-longo"    },  // foco numa fala de tres linhas
      { 1, "foco-curto"    },  // foco numa fala de uma linha
      { 2, "foco-sem-nome" },  // foco numa fala sem quem falou
      { 3, "foco-cortado"  }   // foco na fala que estoura e fecha com "…"
    };
    size_t c;
    for (c = 0; c < sizeof CASOS / sizeof *CASOS; c++) {
      carregar(FALAS, 5, FICHA, 5, "Título de Exemplo (filme)", 1, CASOS[c].sel);
      for (i = 0; i < 60; i++) {
        SDL_PumpEvents();
        txt_novo_quadro();
        tex_novo_quadro();
        gfx_novo_quadro();
        glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        tela();
        if (i < 59) SDL_GL_SwapWindow(w);
      }
      snprintf(nome, sizeof nome, "%s-%s.bmp", saida, CASOS[c].nome);
      gravar(nome, w);
    } }

  // O ESTADO VAZIO, que e o comum em serie: a secao tem de DIZER que a fonte
  // nao tem a pagina, e nao ficar em branco parecendo defeito.
  nFrases = nFatos = 0;
  paginaWq[0] = 0;
  for (i = 0; i < 40; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    gfx_novo_quadro();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    tela();
    if (i < 39) SDL_GL_SwapWindow(w);
  }
  snprintf(nome, sizeof nome, "%s-vazio.bmp", saida);
  gravar(nome, w);

  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
