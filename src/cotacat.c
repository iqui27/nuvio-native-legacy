// Regra da cota de catalogos por addon. Ver cotacat.h.
#include "cotacat.h"
#include <stdlib.h>
#include <string.h>

static int menor(const CotaPrio *pr, int a, int b) {
  if (pr[a].nivel != pr[b].nivel) return pr[a].nivel < pr[b].nivel;
  if (pr[a].pos != pr[b].pos) return pr[a].pos < pr[b].pos;
  return a < b;
}

int cota_escolher(const CotaPrio *pr, int n, int max, char *escolhido) {
  int *ord, i, j, promovidos = 0;
  if (!pr || !escolhido || n < 1) return 0;
  if (max < 0) max = 0;
  if (n <= max) { memset(escolhido, 1, (size_t)n); return 0; }
  memset(escolhido, 0, (size_t)n);
  ord = (int *)malloc(sizeof(int) * (size_t)n);
  if (!ord) {
    // Sem memoria para ordenar: a regra antiga, os primeiros do manifesto.
    memset(escolhido, 1, (size_t)max);
    return 0;
  }
  // Insercao sobre INDICES: no pior caso (o Xperience, ~600 catalogos) sao
  // ~90 mil comparacoes de inteiros, uma vez por volta da descoberta.
  for (i = 0; i < n; i++) {
    for (j = i - 1; j >= 0 && menor(pr, i, ord[j]); j--) ord[j + 1] = ord[j];
    ord[j + 1] = i;
  }
  for (i = 0; i < max; i++) escolhido[ord[i]] = 1;
  free(ord);
  for (i = max; i < n; i++) if (escolhido[i]) promovidos++;
  return promovidos;
}

int cota_vaga_garantida(int *ordem, int nOrdem, const int *addonDe,
                        const char *desligado, const CotaAddon *ad, int nAd,
                        int janela, int ordemPropria,
                        int *vagaAddon, int *vagaCand, int maxVagas) {
  int i, dadas = 0;
  if (!ordem || !addonDe || !desligado || !ad || ordemPropria) return 0;
  if (janela > nOrdem) janela = nOrdem;
  for (i = 0; i < nAd && janela > 1; i++) {
    int q, alvo = -1, ceder = -1;
    if (!ad[i].ativo || !ad[i].novo) continue;
    for (q = 0; q < janela; q++) if (addonDe[ordem[q]] == i) break;
    if (q < janela) continue;                   // ja tem vaga
    for (q = janela; q < nOrdem; q++)
      if (addonDe[ordem[q]] == i && !desligado[ordem[q]]) { alvo = q; break; }
    if (alvo < 0) continue;                     // nada dele que possa entrar
    // Cede a ULTIMA posicao da janela cujo addon ja aparece antes dela: quem
    // perde e a segunda fileira de quem tem duas, nunca a unica de alguem.
    { int r, s;
      for (r = janela - 1; r > 0 && ceder < 0; r--) {
        int ar = addonDe[ordem[r]];
        if (ar < 0) continue;
        for (s = 0; s < r; s++) if (addonDe[ordem[s]] == ar) { ceder = r; break; }
      } }
    if (ceder < 0) continue;                    // ninguem tem duas
    { int mov = ordem[alvo], w;
      for (w = alvo; w > ceder; w--) ordem[w] = ordem[w - 1];
      ordem[ceder] = mov;
      if (dadas < maxVagas) {
        if (vagaAddon) vagaAddon[dadas] = i;
        if (vagaCand) vagaCand[dadas] = mov;
      } }
    dadas++;
  }
  return dadas;
}
