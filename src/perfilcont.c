#include "arteurl.h"
// Ver perfilcont.h.
#include "perfilcont.h"
#include "progresso.h"
#include "catalogo.h"
#include "ajustes.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARQ "perfilcont.txt"
#define SNAP_MAX CONTA_PERFIL_MAX

typedef struct {
  int  perfil, t, e;
  char contentId[48];
  char titulo[160], poster[NV_ARTE_URL_MAX], epNome[96];
} Snap;

static Snap snaps[SNAP_MAX];
static int nSnaps, snapsCarregado;
static CatItem rascunho;   // static: CatItem e grande demais para a pilha

static void limpar(char *s) {
  for (; *s; s++) if (*s == '\t' || *s == '\n' || *s == '\r') *s = ' ';
}

static void carregarSnaps(void) {
  char *buf, *ctx = NULL, *linha;
  if (snapsCarregado) return;
  snapsCarregado = 1;
  nSnaps = 0;
  buf = dados_ler(ARQ);
  if (!buf) return;
  for (linha = strtok_r(buf, "\n", &ctx); linha && nSnaps < SNAP_MAX;
       linha = strtok_r(NULL, "\n", &ctx)) {
    Snap s;
    char *c[7], *q = linha;
    int k;
    memset(&s, 0, sizeof s);
    // Campos vazios contam: nao da para usar strtok aqui.
    for (k = 0; k < 7; k++) {
      c[k] = q;
      if (k < 6) { q = strchr(q, '\t'); if (!q) break; *q++ = 0; }
    }
    if (k < 6) continue;
    s.perfil = atoi(c[0]); s.t = atoi(c[2]); s.e = atoi(c[3]);
    snprintf(s.contentId, sizeof s.contentId, "%s", c[1]);
    snprintf(s.titulo, sizeof s.titulo, "%s", c[4]);
    snprintf(s.poster, sizeof s.poster, "%s", c[5]);
    snprintf(s.epNome, sizeof s.epNome, "%s", c[6]);
    if (s.perfil > 0 && s.contentId[0] && s.titulo[0]) snaps[nSnaps++] = s;
  }
  free(buf);
}

static void gravarSnaps(void) {
  size_t cap = (size_t)nSnaps * (sizeof(Snap) + 64u) + 16u;
  char *b = malloc(cap);
  int i;
  size_t n = 0;
  if (!b) return;
  b[0] = 0;
  for (i = 0; i < nSnaps; i++)
    n += (size_t)snprintf(b + n, cap - n, "%d\t%s\t%d\t%d\t%s\t%s\t%s\n", snaps[i].perfil,
                          snaps[i].contentId, snaps[i].t, snaps[i].e, snaps[i].titulo,
                          snaps[i].poster, snaps[i].epNome);
  dados_gravar(ARQ, b);
  free(b);
}

static int achaSnap(int perfil) {
  int i;
  for (i = 0; i < nSnaps; i++) if (snaps[i].perfil == perfil) return i;
  return -1;
}

// Um registro de progresso -> o titulo conhecido pelo catalogo, ou 0.
static int doCatalogo(const ProgRegistro *r) {
  memset(&rascunho, 0, sizeof rascunho);
  return cat_copiar_por_id(r->contentId, r->episodio > 0 ? "series" : "movie", &rascunho) &&
         rascunho.titulo[0];
}

int perfilcont_de(const ContaPerfil *p, PerfilCont *saida) {
  ProgRegistro r;
  int i;
  if (!p || !saida || p->temPin) return 0;
  if (!prog_continuar_de_perfil_min(p->indice, ajustes_cw_minimo(), ajustes_cw_concluido(), &r)) return 0;
  memset(saida, 0, sizeof *saida);
  if (doCatalogo(&r)) {
    snprintf(saida->titulo, sizeof saida->titulo, "%s", rascunho.titulo);
    snprintf(saida->poster, sizeof saida->poster, "%s", rascunho.poster);
    if (rascunho.temporada == r.temporada && rascunho.episodio == r.episodio)
      snprintf(saida->epNome, sizeof saida->epNome, "%s", rascunho.nomeEpisodio);
  } else {
    carregarSnaps();
    i = achaSnap(p->indice);
    if (i < 0 || strcmp(snaps[i].contentId, r.contentId)) return 0;
    snprintf(saida->titulo, sizeof saida->titulo, "%s", snaps[i].titulo);
    snprintf(saida->poster, sizeof saida->poster, "%s", snaps[i].poster);
    if (snaps[i].t == r.temporada && snaps[i].e == r.episodio)
      snprintf(saida->epNome, sizeof saida->epNome, "%s", snaps[i].epNome);
  }
  saida->serie = r.episodio > 0;
  saida->t = r.temporada ? r.temporada : (saida->serie ? 1 : 0);
  saida->e = r.episodio;
  saida->progresso = (float)(r.posSeg / r.durSeg);
  saida->restanteMin = (int)((r.durSeg - r.posSeg) / 60.0 + 0.5);
  if (saida->restanteMin < 1) saida->restanteMin = 1;
  return 1;
}

void perfilcont_registrar(void) {
  int i, m = perfis_n(), mudou = 0;
  carregarSnaps();
  for (i = 0; i < m; i++) {
    const ContaPerfil *p = perfis_item(i);
    ProgRegistro r;
    Snap s;
    int k;
    if (!p || p->temPin || !prog_continuar_de_perfil_min(p->indice, ajustes_cw_minimo(), ajustes_cw_concluido(), &r) ||
        !doCatalogo(&r) || strlen(rascunho.poster) >= sizeof s.poster) continue;
    memset(&s, 0, sizeof s);
    s.perfil = p->indice; s.t = r.temporada; s.e = r.episodio;
    snprintf(s.contentId, sizeof s.contentId, "%s", r.contentId);
    snprintf(s.titulo, sizeof s.titulo, "%s", rascunho.titulo);
    snprintf(s.poster, sizeof s.poster, "%s", rascunho.poster);
    if (rascunho.temporada == r.temporada && rascunho.episodio == r.episodio)
      snprintf(s.epNome, sizeof s.epNome, "%s", rascunho.nomeEpisodio);
    limpar(s.titulo); limpar(s.poster); limpar(s.epNome);
    k = achaSnap(s.perfil);
    if (k >= 0 && !memcmp(&snaps[k], &s, sizeof s)) continue;
    if (k < 0 && nSnaps < SNAP_MAX) k = nSnaps++;
    if (k < 0) continue;
    snaps[k] = s;
    mudou = 1;
  }
  if (mudou) gravarSnaps();
}

void perfilcont_esquecer(void) {
  nSnaps = 0;
  snapsCarregado = 1;
  dados_apagar(ARQ);
}
