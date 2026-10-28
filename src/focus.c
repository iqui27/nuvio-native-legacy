#include "focus.h"
#include <string.h>
#include <math.h>

int focus_rect_nearest(const GfxRect *r, int n, int current, int dx, int dy) {
  if (current < 0 || current >= n) return -1;
  int best = current;
  float distance = INFINITY;
  for (int i = 0; i < n; i++) {
    float x = r[i].x + r[i].w / 2 - r[current].x - r[current].w / 2;
    float y = r[i].y + r[i].h / 2 - r[current].y - r[current].h / 2;
    float along = x * dx + y * dy;
    if (along < 1) continue;
    float score = along + 3 * fabsf(dx ? y : x);
    if (score < distance) { distance = score; best = i; }
  }
  return best;
}

void focus_iniciar(Foco *f, int nFileiras, const int *nColunas) {
  memset(f, 0, sizeof *f);
  if (nFileiras > FOCUS_MAX_FILEIRAS) nFileiras = FOCUS_MAX_FILEIRAS;
  f->nFileiras = nFileiras;
  for (int i = 0; i < nFileiras; i++) f->nColunas[i] = nColunas[i];
}

int focus_mover(Foco *f, int dx, int dy) {
  int fAntes = f->fileira, cAntes = f->coluna;

  if (dx) {
    int novo = f->coluna + dx;
    if (novo >= 0 && novo < f->nColunas[f->fileira]) f->coluna = novo;
  }
  if (dy) {
    // PULA fileira vazia. Num filme as fileiras de temporada e de episodio tem
    // zero colunas, e pousar nelas era foco em coisa que a tela nem desenha: o
    // D-pad parecia travado e a rolagem ainda mirava o grupo vazio. Uma fileira
    // sem item nunca deve receber foco, entao a busca segue no mesmo sentido
    // ate achar uma que tenha — ou desistir na borda, devolvendo 0.
    int nova = f->fileira + dy;
    while (nova >= 0 && nova < f->nFileiras && f->nColunas[nova] <= 0) nova += dy;
    if (nova >= 0 && nova < f->nFileiras) {
      // guarda onde estava nesta fileira antes de sair
      f->colunaLembrada[f->fileira] = f->coluna;
      f->fileira = nova;
      int alvo = f->colunaLembrada[nova];
      if (alvo >= f->nColunas[nova]) alvo = f->nColunas[nova] - 1;
      if (alvo < 0) alvo = 0;
      f->coluna = alvo;
    }
  }
  return (f->fileira != fAntes || f->coluna != cAntes);
}

int focus_mover_grade(Foco *f, int dx, int dy) {
  int fAntes = f->fileira, cAntes = f->coluna;

  if (dx) {
    int novo = f->coluna + dx;
    if (novo >= 0 && novo < f->nColunas[f->fileira]) f->coluna = novo;
  }
  if (dy) {
    // Mesma regra de PULAR fileira vazia do focus_mover: uma fileira sem item
    // nunca recebe foco.
    int nova = f->fileira + dy;
    while (nova >= 0 && nova < f->nFileiras && f->nColunas[nova] <= 0) nova += dy;
    if (nova >= 0 && nova < f->nFileiras) {
      int alvo = f->coluna;
      // A memoria continua sendo ESCRITA, para o caso de a mesma estrutura ser
      // percorrida tambem por focus_mover (a biblioteca troca de modo e
      // reinicia o foco); ela so nao e LIDA aqui.
      f->colunaLembrada[f->fileira] = f->coluna;
      if (alvo >= f->nColunas[nova]) alvo = f->nColunas[nova] - 1;
      if (alvo < 0) alvo = 0;
      f->fileira = nova;
      f->coluna = alvo;
    }
  }
  return (f->fileira != fAntes || f->coluna != cAntes);
}

int focus_indice(const Foco *f, int fileira, int coluna) {
  return (f->fileira == fileira && f->coluna == coluna);
}
