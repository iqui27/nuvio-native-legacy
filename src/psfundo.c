#include "psfundo.h"
#include "catalogo.h"
#include "artehero.h"
#include <stdio.h>
#include <string.h>

// GUARDA OS INDICES, e nao um intervalo: a fileira escolhida pode encolher, o
// catalogo pode ser republicado e o sorteio nao e contiguo por definicao. E a
// mesma razao escrita em home.c:heroSet.
static int lista[PSFUNDO_MAX];
static int listaN;

// Assinatura do que a lista depende, para nao remontar a cada quadro.
static int  montada;
static int  catN;
static char fonteAtual[192];
static unsigned sementeAtual;

static void montar(const char *fonte, unsigned semente) {
  int total = cat_n();
  int i;
  listaN = 0;
  if (total <= 0) return;

  if (fonte[0] == '*' && !fonte[1] && semente) {
    // Sorteio sem repeticao: Fisher-Yates PARCIAL, so os PSFUNDO_MAX primeiros
    // passos. Duplicado de home.c de proposito (ver a nota em psfundo.h).
    static int baralho[512];
    int teto = (int)(sizeof baralho / sizeof *baralho);
    int m = total < teto ? total : teto;
    int n = m < PSFUNDO_MAX ? m : PSFUNDO_MAX;
    unsigned x = semente;
    for (i = 0; i < m; i++) baralho[i] = i;
    for (i = 0; i < n; i++) {
      int j;
      x = x * 1103515245u + 12345u;
      j = i + (int)((x >> 16) % (unsigned)(m - i));
      { int t = baralho[i]; baralho[i] = baralho[j]; baralho[j] = t; }
      lista[listaN++] = baralho[i];
    }
    return;
  }

  if (fonte[0] && !(fonte[0] == '*' && !fonte[1])) {
    // FILEIRA ESCOLHIDA. Nao existir nao e erro: o addon pode voltar, e ate la
    // o fundo cai no automatico — o mesmo que a home faz.
    int nf = cat_n_fileiras();
    for (i = 0; i < nf; i++) {
      const CatFileira *f = cat_fileira(i);
      int k;
      if (!f || strcmp(f->chave, fonte)) continue;
      for (k = 0; k < f->n && listaN < PSFUNDO_MAX; k++) {
        int idx = f->ini + k;
        if (idx >= 0 && idx < total) lista[listaN++] = idx;
      }
      break;
    }
    if (listaN) return;
  }

  for (i = 0; i < total && listaN < PSFUNDO_MAX; i++) lista[listaN++] = i;
}

void psfundo_garantir(const char *fonte, unsigned semente) {
  if (!fonte) fonte = "";
  if (montada && cat_n() == catN && semente == sementeAtual
      && !strcmp(fonte, fonteAtual))
    return;
  catN = cat_n();
  sementeAtual = semente;
  snprintf(fonteAtual, sizeof fonteAtual, "%s", fonte);
  montada = 1;
  montar(fonteAtual, semente);
}

int psfundo_n(void) { return listaN; }

int psfundo_indice(int pos) {
  return (pos >= 0 && pos < listaN) ? lista[pos] : -1;
}

const char *psfundo_url(int pos) {
  // COPIA a url: artehero_url devolve um buffer estatico proprio, que a
  // chamada seguinte sobrescreve — e quem desenha guarda a string entre
  // quadros para saber se a arte na tela ainda e a desejada.
  static char buf[512];
  const char *u;
  int idx = psfundo_indice(pos);
  const CatItem *it = idx >= 0 ? cat_item(idx) : NULL;
  if (!it) return NULL;
  u = artehero_url(it);
  if (!u || !u[0]) return NULL;
  snprintf(buf, sizeof buf, "%s", u);
  return buf;
}

int psfundo_pos_no_tempo(unsigned agoraMs, unsigned inicioMs,
                         unsigned intervaloMs, int n) {
  unsigned passados;
  if (n <= 0 || intervaloMs == 0) return 0;
  // SUBTRACAO DE unsigned, e nao de int: SDL_GetTicks passa de 2^31 depois de
  // 24 dias de TV ligada, e a diferenca em unsigned continua certa mesmo
  // quando o contador da a volta.
  passados = agoraMs - inicioMs;
  return (int)((passados / intervaloMs) % (unsigned)n);
}

void psfundo_limpar(void) {
  montada = 0;
  listaN = 0;
  catN = 0;
  sementeAtual = 0;
  fonteAtual[0] = 0;
}
