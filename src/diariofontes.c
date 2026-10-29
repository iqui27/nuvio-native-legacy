// FONTES DO DIARIO (diariofontes.h).
#include "diariofontes.h"
#include "diario.h"
#include "dormiu.h"
#include "contalib.h"
#include "corviva.h"
#include "perfis.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static struct {
  int ativo;
  char id[DIARIO_ID_MAX];
  char tipo;
  int t, e;
  double seg;               // tocado neste episodio
  short hora;               // dia*24+hora local do inicio deste episodio
  int gravado;              // a vez deste episodio ja foi ao diario
  double dur;
} P;

int diariofontes_ano(const char *m) {
  int i, a = 0;
  if (!m) return 0;
  for (i = 0; i < 4; i++) { if (m[i] < '0' || m[i] > '9') return 0; a = a * 10 + (m[i] - '0'); }
  return (a >= 1870 && a <= 2200) ? a : 0;
}

void diariofontes_nome(const CatItem *ci) {
  if (!ci || !ci->imdb[0] || !ci->titulo[0]) return;
  diario_nome_definir(ci->imdb, ci->titulo, diariofontes_ano(ci->meta), ci->poster);
}

static short horaAgora(void) {
  time_t t = time(NULL);
  struct tm tmv;
  localtime_r(&t, &tmv);
  return (short)(tmv.tm_wday * 24 + tmv.tm_hour);
}

static unsigned corDaCena(void) {
  CorvivaPaleta p;
  memset(&p, 0, sizeof p);
  if (!corviva_cena_paleta(&p)) return 0;
  { int r = (int)(p.acento[0] * 255.0f + 0.5f), g = (int)(p.acento[1] * 255.0f + 0.5f),
        b = (int)(p.acento[2] * 255.0f + 0.5f);
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    return (unsigned)((r << 16) | (g << 8) | b); }
}

static void gravarVez(void) {
  DiarioEntrada d;
  if (!P.ativo || P.gravado || !P.id[0]) return;
  memset(&d, 0, sizeof d);
  snprintf(d.id, sizeof d.id, "%s", P.id);
  d.tipo = P.tipo;
  d.temporada = (short)P.t; d.episodio = (short)P.e;
  d.origem = DIARIO_ORIG_PLAYER;
  d.quando = (long long)time(NULL);
  d.seg = (int)(P.seg + 0.5);
  d.hora = P.hora;
  d.cor = corDaCena();
  d.durSeg = (int)(P.dur + 0.5);
  diario_adicionar(0, &d);
  P.gravado = 1;
}

void diariofontes_player_passo(const CatItem *ci, int t, int e, double pos, double dur,
                               int tocando, int concluiu, float dt) {
  char id[DIARIO_ID_MAX];
  char tipo;
  if (!ci || !ci->imdb[0]) return;
  diario_separar_id(ci->imdb, id, sizeof id, NULL, NULL);
  tipo = !strcmp(ci->tipo, "series") ? DIARIO_SERIE : DIARIO_FILME;
  if (!P.ativo || strcmp(P.id, id) || P.t != t || P.e != e) {
    memset(&P, 0, sizeof P);
    P.ativo = 1;
    snprintf(P.id, sizeof P.id, "%s", id);
    P.tipo = tipo; P.t = t; P.e = e;
    P.hora = horaAgora();
    diariofontes_nome(ci);
  }
  P.dur = dur;
  if (tocando && dt > 0 && dt < 5) P.seg += dt;
  if (concluiu) gravarVez();
  dormiu_player_passo(id, tipo, t, e, pos, dur, tocando, concluiu, dt);
}

void diariofontes_player_saiu(double pos, double dur, int concluiu) {
  if (P.ativo && concluiu) { if (dur > 0) P.dur = dur; gravarVez(); }
  dormiu_player_saiu(pos, dur, concluiu);
  P.ativo = 0;
}

// ------------------------------------------------------------------- conta

void diariofontes_passo(void) {
  static unsigned revVista;
  static int perfilVisto = -1;
  unsigned rev = contalib_vistos_revisao();
  int perfil = perfis_ativo(), n, i, k = 0;
  static DiarioEntrada lote[512];
  if (rev == revVista && perfil == perfilVisto) return;
  revVista = rev; perfilVisto = perfil;
  n = contalib_n_vistos();
  if (n <= 0) return;
  // Nomes da biblioteca da conta: de graca, ja estao na memoria.
  for (i = 0; i < contalib_n(); i++) {
    const ContaLibItem *it = contalib_item(i);
    if (it && it->titulo[0]) diario_nome_definir(it->id, it->titulo, diariofontes_ano(it->meta), it->poster);
  }
  for (i = 0; i < n; i++) {
    const ContaVisto *v = contalib_visto(i);
    DiarioEntrada *d;
    if (!v || !v->id[0] || v->vistoMs <= 0) continue;   // sem data nao vira entrada
    d = &lote[k++];
    memset(d, 0, sizeof *d);
    snprintf(d->id, sizeof d->id, "%s", v->id);
    d->tipo = (!strcmp(v->tipo, "series") || v->temporada || v->episodio) ? DIARIO_SERIE : DIARIO_FILME;
    d->temporada = (short)v->temporada; d->episodio = (short)v->episodio;
    d->origem = DIARIO_ORIG_CONTA;
    d->quando = v->vistoMs / 1000;
    d->hora = -1;
    // A conta guarda o instante da MARCA: para quem marcou a serie inteira de
    // uma vez, nao e quando viu. A tela diz "data aproximada".
    d->flags = DIARIO_F_DATA_APROX;
    if (k == (int)(sizeof lote / sizeof lote[0])) { diario_adicionar_lote(0, lote, k); k = 0; }
  }
  if (k) diario_adicionar_lote(0, lote, k);
}
