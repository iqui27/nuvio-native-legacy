#include "travajuntos.h"
#include "trava.h"
#include "contapend.h"
#include "idioma.h"
#include "ilha.h"
#include "ilhaacao.h"
#include <stdio.h>
#include <string.h>

// O que o Desfazer precisa para tirar a marca dos alvos.
typedef struct {
  char serie[DIARIO_ID_MAX];
  char titulo[64];
  short t, e;
  long long quando;
  int perfil[TRAVA_MEMBROS];
  int n;
} Marca;

static void nomeDe(int perfil, char *saida, size_t n) {
  int i;
  snprintf(saida, n, "%d", perfil);
  for (i = 0; i < perfis_n(); i++) {
    const ContaPerfil *p = perfis_item(i);
    if (p && p->indice == perfil && p->nome[0]) { snprintf(saida, n, "%s", p->nome); return; }
  }
}

// "A", "A e B", "A e mais N".
static void frase(const Marca *m, char *saida, size_t n) {
  char a[64], b[64], f[160];
  if (m->n < 1) { saida[0] = 0; return; }
  nomeDe(m->perfil[0], a, sizeof a);
  if (m->n == 1) { snprintf(f, sizeof f, i18n("Marcado para %s"), a); }
  else if (m->n == 2) {
    char ab[140];
    nomeDe(m->perfil[1], b, sizeof b);
    snprintf(ab, sizeof ab, i18n("%s e %s"), a, b);
    snprintf(f, sizeof f, i18n("Marcado para %s"), ab);
  } else {
    char ab[140];
    snprintf(ab, sizeof ab, i18n("%s e mais %d"), a, m->n - 1);
    snprintf(f, sizeof f, i18n("Marcado para %s"), ab);
  }
  snprintf(saida, n, "%s", f);
}

static void desfazer(const void *p) {
  const Marca *m = p;
  VistoPar par = { m->t, m->e };
  int i;
  for (i = 0; i < m->n; i++) {
    diario_apagar(m->perfil[i], m->serie, m->t, m->e, m->quando);
    contapend_episodios_perfil(m->perfil[i], m->serie, "series", &par, 1, 0);
  }
  contapend_chutar();
}

static void ouvinte(int perfil, int evento, const DiarioEntrada *e, void *ctx) {
  int alvos[TRAVA_MEMBROS], na, i;
  Marca m;
  VistoPar par;
  unsigned char bits;
  Trava tv;
  int ids[8];
  IlhaAcao a;
  char fr[200];
  (void)ctx;
  if (evento != DIARIO_EV_NOVA || !e) return;
  if (e->origem != DIARIO_ORIG_PLAYER || e->tipo != DIARIO_SERIE) return;
  if (e->temporada < 1 || e->episodio < 1) return;
  if (e->flags & (DIARIO_F_PARCIAL | DIARIO_F_JUNTOS)) return;
  na = trava_juntos_alvos(e->id, perfil, alvos, TRAVA_MEMBROS);
  if (na < 1) return;

  memset(&m, 0, sizeof m);
  snprintf(m.serie, sizeof m.serie, "%s", e->id);
  m.t = e->temporada; m.e = e->episodio; m.quando = e->quando;
  par.temporada = e->temporada; par.episodio = e->episodio;
  bits = (unsigned char)(1u << (perfil - 1));
  for (i = 0; i < na; i++) bits |= (unsigned char)(1u << (alvos[i] - 1));

  for (i = 0; i < na; i++) {
    DiarioEntrada d = *e;
    long long q;
    if (diario_episodio(alvos[i], e->id, e->temporada, e->episodio, &q)) continue;   // ja viu
    d.origem = DIARIO_ORIG_MANUAL;
    d.flags = (unsigned short)((e->flags & ~(DIARIO_F_REVER | DIARIO_F_DORMIU | DIARIO_F_DORMIU_SUS)) | DIARIO_F_JUNTOS);
    d.comPerfis = bits;
    d.nota10 = 0; d.tags = 0; d.seg = 0; d.dormiuSeg = 0;
    if (diario_adicionar(alvos[i], &d) < 0) continue;
    contapend_episodios_perfil(alvos[i], e->id, "series", &par, 1, 1);
    m.perfil[m.n++] = alvos[i];
  }
  if (m.n < 1) return;
  diario_definir_flags(perfil, e->id, e->temporada, e->episodio, e->quando, DIARIO_F_JUNTOS, 0);
  contapend_chutar();

  if (trava_da_serie(e->id, perfil, ids, 8) > 0 && trava_obter(ids[0], &tv)) snprintf(m.titulo, sizeof m.titulo, "%s", tv.titulo);
  frase(&m, fr, sizeof fr);
  memset(&a, 0, sizeof a);
  a.icone = "aj_users";
  a.frase = fr;
  a.titulo = m.titulo;
  a.onde = "";
  a.tipo = ILHA_OK;
  a.desfazer = desfazer; a.ctx = &m; a.ctxN = sizeof m;
  ilhaacao_feita(&a);
}

void travajuntos_iniciar(void) { diario_ouvir(ouvinte, NULL); }
