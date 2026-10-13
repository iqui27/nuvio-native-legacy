// Filmografia 2.1: regra da ficha da pessoa. Ver filmomodelo.h.
#include "filmomodelo.h"
#include "conquistasregra.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Copia sem partir um caractere UTF-8 ao meio.
static void cpy(char *dst, size_t n, const char *src) {
  size_t i = 0;
  if (!n) return;
  if (src) for (; src[i] && i + 1 < n; i++) dst[i] = src[i];
  if (src && src[i]) while (i > 0 && ((unsigned char)src[i] & 0xC0) == 0x80) i--;
  dst[i] = 0;
}

static int palavra(unsigned char c) { return isalnum(c) || c == '_'; }

static int eq_ci(const char *a, const char *b, size_t n) {
  for (size_t i = 0; i < n; i++)
    if (!a[i] || tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return 0;
  return 1;
}

static int sub(const char *h, const char *w) {
  size_t m = strlen(w);
  for (; *h; h++) if (eq_ci(h, w, m)) return 1;
  return 0;
}

// `w` no comeco de uma palavra de `h`; `fim` exige borda de palavra tambem no fim.
static int acha(const char *h, const char *w, int fim) {
  size_t m = strlen(w);
  for (size_t i = 0; h[i]; i++) {
    if (i && palavra((unsigned char)h[i - 1])) continue;
    if (eq_ci(h + i, w, m) && (!fim || !palavra((unsigned char)h[i + m]))) return 1;
  }
  return 0;
}

unsigned fm_hash(const char *s) {
  uint32_t h = 2166136261u;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; }
  return h;
}

// ---- Juntar creditos.
int fm_voz(const char *p) { return sub(p, "(voice)") || sub(p, "(voz)"); }

static int tem_seg(const char *lista, const char *s) {
  size_t m = strlen(s);
  for (const char *p = lista; *p;) {
    const char *f = strstr(p, " / ");
    size_t len = f ? (size_t)(f - p) : strlen(p);
    if (len == m && !strncmp(p, s, m)) return 1;
    if (!f) break;
    p = f + 3;
  }
  return 0;
}

int fm_ruido(const FmCredito *c) {
  const char *p = c->personagem;
  if (c->ruido == 1 || acha(p, "self", 1) || acha(p, "himself", 1) || acha(p, "herself", 1) ||
      acha(p, "themselves", 1) || sub(p, "(uncredited)") || sub(p, "(archive")) return 1;
  if (c->tv && c->papeis == FM_ATUA && c->eps > 0 && c->eps <= 2) return 2;
  return 0;
}

int fm_juntar(const FmBruto *b, int n, const char *hoje, FmCredito *out, int max) {
  int k = 0;
  for (int i = 0; i < n; i++) {
    const FmBruto *r = &b[i];
    int j = 0;
    while (j < k && !(out[j].tmdb == r->tmdb && out[j].tv == r->tv)) j++;
    if (j == k) {
      FmCredito *c;
      if (k >= max) continue;
      c = &out[k++];
      memset(c, 0, sizeof *c);
      c->tmdb = r->tmdb; c->tv = r->tv; c->ordem = 99;
      cpy(c->titulo, sizeof c->titulo, r->titulo);
    }
    FmCredito *c = &out[j];
    c->papeis |= r->papel;
    if (r->personagem[0] && !tem_seg(c->personagem, r->personagem)) {
      size_t u = strlen(c->personagem);
      if (u) { cpy(c->personagem + u, sizeof c->personagem - u, " / "); u = strlen(c->personagem); }
      cpy(c->personagem + u, sizeof c->personagem - u, r->personagem);
    }
    if (!c->ano) c->ano = r->ano;
    if (!c->lancamento[0]) cpy(c->lancamento, sizeof c->lancamento, r->lancamento);
    if (!c->poster[0]) cpy(c->poster, sizeof c->poster, r->poster);
    if (!c->imdb[0]) cpy(c->imdb, sizeof c->imdb, r->imdb);
    if (r->eps > c->eps) c->eps = r->eps;
    if (r->epsTotal > c->epsTotal) c->epsTotal = r->epsTotal;
    if (r->votos > c->votos) c->votos = r->votos;
    if (r->papel == FM_ATUA && r->ordem < c->ordem) c->ordem = r->ordem;
    if (r->talk) c->ruido = 1;
  }
  for (int i = 0; i < k; i++) {
    FmCredito *c = &out[i];
    c->voz = fm_voz(c->personagem);
    c->proximo = c->lancamento[0] ? strcmp(c->lancamento, hoje) > 0 : c->ano > atoi(hoje);
    c->ruido = fm_ruido(c);
  }
  return k;
}

void fm_personagem(const FmCredito *c, char *dst, size_t n) {
  size_t o = 0;
  const char *p = c->personagem;
  if (!n) return;
  while (*p && o + 1 < n) {
    size_t m = 0;
    if (*p == '(') m = eq_ci(p, "(voice)", 7) ? 7 : eq_ci(p, "(voz)", 5) ? 5 : 0;
    if (m) {
      p += m;
      while (o && dst[o - 1] == ' ') o--;
      continue;
    }
    dst[o++] = *p++;
  }
  dst[o] = 0;
}

static const char *ident(const char *s) { return s; }

void fm_rotulo(const FmCredito *c, char *dst, size_t n, const char *(*tr)(const char *)) {
  const char *p[3];
  int np = 0;
  size_t o = 0;
  if (!tr) tr = ident;
  if (c->papeis & FM_DIR) p[np++] = "Direção";
  if (c->papeis & FM_ROT) p[np++] = "Roteiro";
  if (c->papeis & FM_ATUA)
    p[np++] = c->ruido ? (c->ruido == 2 ? "Participação" : "Como si")
              : c->voz ? "Voz" : c->ordem <= 1 ? "Protagonista" : "Elenco";
  if (!n) return;
  dst[0] = 0;
  for (int i = 0; i < np; i++) {
    int w = snprintf(dst + o, n - o, "%s%s", i ? " · " : "", tr(p[i]));
    if (w < 0 || (size_t)w >= n - o) break;
    o += (size_t)w;
  }
}

// ---- Marcos e capitulos.
// Ordem por modo: 0 novos, 1 antigos, 2 votos, 3 preferido-depois-votos. Estavel.
static int antes(const FmCredito *l, int modo, int prefer, int a, int b) {
  const FmCredito *x = &l[a], *y = &l[b];
  if (modo == 3) {
    int pa = !!(x->papeis & prefer), pb = !!(y->papeis & prefer);
    if (pa != pb) return pa > pb;
    return x->votos > y->votos;
  }
  if (modo != 2 && x->ano != y->ano) return modo == 0 ? x->ano > y->ano : x->ano < y->ano;
  return x->votos > y->votos;
}

static void ordena(int *idx, int n, const FmCredito *l, int modo, int prefer) {
  for (int i = 1; i < n; i++) {
    int v = idx[i], j = i;
    while (j > 0 && antes(l, modo, prefer, v, idx[j - 1])) { idx[j] = idx[j - 1]; j--; }
    idx[j] = v;
  }
}

static int add_marco(FmMarco *out, int *k, int max, const FmCredito *l, int tipo, int idx) {
  if (idx < 0 || *k >= max) return 0;
  for (int i = 0; i < *k; i++) if (out[i].idx == idx) return 0;
  out[*k].tipo = tipo; out[*k].ano = l[idx].ano; out[*k].idx = idx;
  (*k)++;
  return 1;
}

int fm_marcos(const FmCredito *l, int n, FmMarco *out, int max) {
  int asc[FM_MAX], na = 0, k = 0;
  if (n > FM_MAX) n = FM_MAX;
  for (int i = 0; i < n; i++) if (!l[i].ruido && l[i].ano > 0) asc[na++] = i;
  if (!na) return 0;
  ordena(asc, na, l, 1, 0);
  int f_filme = -1, f_prot = -1, f_dir = -1, f_rot = -1, topo = -1;
  for (int j = 0; j < na; j++) {
    const FmCredito *c = &l[asc[j]];
    if (f_filme < 0 && !c->tv) f_filme = asc[j];
    if (f_prot < 0 && (c->papeis & FM_ATUA) && c->ordem <= 1) f_prot = asc[j];
    if (f_dir < 0 && (c->papeis & FM_DIR)) f_dir = asc[j];
    if (f_rot < 0 && (c->papeis & FM_ROT)) f_rot = asc[j];
  }
  for (int i = 0; i < n; i++)
    if (!l[i].ruido && l[i].ano > 0 && (topo < 0 || l[i].votos > l[topo].votos)) topo = i;
  add_marco(out, &k, max, l, l[asc[0]].tv ? FM_M_PRIMEIRA : FM_M_ESTREIA, asc[0]);
  add_marco(out, &k, max, l, FM_M_ESTREIA, f_filme);
  add_marco(out, &k, max, l, FM_M_PROTAG, f_prot);
  add_marco(out, &k, max, l, FM_M_DIRECAO, f_dir);
  add_marco(out, &k, max, l, FM_M_ROTEIRO, f_rot);
  if (na >= 3) add_marco(out, &k, max, l, FM_M_MAIS_VOTADO, topo);
  if (na >= 2) add_marco(out, &k, max, l, FM_M_RECENTE, asc[na - 1]);
  for (int i = 1; i < k; i++) {   // por ano, estavel
    FmMarco v = out[i];
    int j = i;
    while (j > 0 && v.ano < out[j - 1].ano) { out[j] = out[j - 1]; j--; }
    out[j] = v;
  }
  return k;
}

static int prioridade(int tipo) {
  static const int ord[] = { FM_M_PRIMEIRA, FM_M_ESTREIA, FM_M_MAIS_VOTADO, FM_M_DIRECAO, FM_M_AGORA,
                             FM_M_PROTAG, FM_M_ROTEIRO };
  for (int i = 0; i < 7; i++) if (ord[i] == tipo) return i;
  return 99;
}

int fm_capitulos(const FmCredito *l, int n, const FmMarco *m, int nm, FmCap *out, int max) {
  int min = 0, mx = 0, tem = 0, qtd = 0;
  if (n > FM_MAX) n = FM_MAX;
  for (int i = 0; i < n; i++) {
    if (l[i].ruido || l[i].ano <= 0) continue;
    if (!tem || l[i].ano < min) min = l[i].ano;
    if (!tem || l[i].ano > mx) mx = l[i].ano;
    tem = 1; qtd++;
  }
  if (!tem || max < 1) return 0;
  if (qtd == 1) { out[0].tipo = FM_C_UNICO; out[0].de = min; out[0].ate = mx; return 1; }
  FmMarco cand[FM_MARCOS_MAX + 2];
  int nc = 0;
  for (int i = 0; i < nm && nc < FM_MARCOS_MAX; i++)
    if (m[i].tipo != FM_M_RECENTE) { cand[nc] = m[i]; nc++; }
  if (mx - min >= 8) { cand[nc].tipo = FM_M_AGORA; cand[nc].ano = mx - 2; cand[nc].idx = -1; nc++; }
  for (int i = 1; i < nc; i++) {
    FmMarco v = cand[i];
    int j = i;
    while (j > 0 && prioridade(v.tipo) < prioridade(cand[j - 1].tipo)) { cand[j] = cand[j - 1]; j--; }
    cand[j] = v;
  }
  int ano[FM_CAP_MAX], tipo[FM_CAP_MAX], ns = 0;
  for (int i = 0; i < nc && ns < FM_CAP_MAX; i++) {
    int dup = 0;
    for (int j = 0; j < ns; j++) if (ano[j] == cand[i].ano) dup = 1;
    if (!dup) { ano[ns] = cand[i].ano; tipo[ns] = cand[i].tipo; ns++; }
  }
  for (int i = 1; i < ns; i++) {
    int a = ano[i], t = tipo[i], j = i;
    while (j > 0 && a < ano[j - 1]) { ano[j] = ano[j - 1]; tipo[j] = tipo[j - 1]; j--; }
    ano[j] = a; tipo[j] = t;
  }
  if (ns > max) ns = max;
  for (int i = 0; i < ns; i++) {
    out[i].tipo = tipo[i]; out[i].de = ano[i];
    out[i].ate = i + 1 < ns ? ano[i + 1] - 1 : mx;
  }
  return ns;
}

const char *fm_marco_nome(int tipo) {
  switch (tipo) {
    case FM_M_PRIMEIRA: return "Primeira aparição";
    case FM_M_ESTREIA: return "Estreia no cinema";
    case FM_M_PROTAG: return "Primeiro papel principal";
    case FM_M_DIRECAO: return "Estreia na direção";
    case FM_M_ROTEIRO: return "Primeiro roteiro";
    case FM_M_MAIS_VOTADO: return "Mais votado";
    case FM_M_RECENTE: return "Mais recente";
    default: return "";
  }
}

const char *fm_capitulo_nome(int tipo) {
  switch (tipo) {
    case FM_M_PRIMEIRA: return "Primeiros passos";
    case FM_M_ESTREIA: return "Estreia no cinema";
    case FM_M_PROTAG: return "Protagonista";
    case FM_M_DIRECAO: return "Atrás da câmera";
    case FM_M_ROTEIRO: return "Também no roteiro";
    case FM_M_MAIS_VOTADO: return "O papel que marcou";
    case FM_M_AGORA: return "Agora";
    case FM_C_UNICO: return "Estreia";
    default: return "";
  }
}

// ---- Visto.
FmEstado fm_estado(const FmCredito *c, int vistoFilme, int epsVistos, const char *emHist,
                   int manualComo, const char *manualEm) {
  FmEstado s;
  memset(&s, 0, sizeof s);
  if (c->proximo) { s.estado = FM_EM_BREVE; return s; }
  if (c->tv && epsVistos >= 0) {
    int mine = c->eps;
    if (epsVistos > 0) {
      s.epsDe = mine;
      s.epsVistos = mine > 0 && epsVistos > mine ? mine : epsVistos;
      s.estado = mine > 0 && epsVistos >= mine ? FM_VISTO : FM_ANDA;
      s.pct = mine <= 0 ? 0 : (int)((100L * s.epsVistos + mine / 2) / mine);
      if (s.pct > 100) s.pct = 100;
    }
  } else if (vistoFilme > 0) {
    s.estado = vistoFilme >= 100 ? FM_VISTO : FM_ANDA;
    s.pct = vistoFilme >= 100 ? 100 : vistoFilme;
  }
  if (s.estado != FM_NAO && emHist) cpy(s.em, sizeof s.em, emHist);
  if (manualComo) {
    s.estado = FM_VISTO; s.pct = 100; s.manual = 1; s.como = manualComo;
    s.em[0] = 0;
    if (manualEm) cpy(s.em, sizeof s.em, manualEm);
  }
  return s;
}

int fm_conta(const FmEstado *s) {
  return s->estado == FM_VISTO && (!s->manual || s->como == FM_COMO_QUIZ);
}

void fm_resumo(const FmCredito *l, const FmEstado *st, int n, FmResumo *r) {
  static const int bit[3] = { FM_ATUA, FM_DIR, FM_ROT };
  memset(r, 0, sizeof *r);
  for (int i = 0; i < n; i++) {
    if (l[i].ruido || l[i].proximo) continue;
    r->total++;
    if (st[i].estado == FM_VISTO) r->visto++; else if (st[i].estado == FM_ANDA) r->andamento++;
    for (int k = 0; k < 3; k++) {
      if (!(l[i].papeis & bit[k])) continue;
      r->papelTotal[k]++;
      if (st[i].estado == FM_VISTO) r->papelVisto[k]++;
    }
  }
}

// ---- Conquistas.
int fm_precisa(int nivel, int total) {
  switch (nivel) {
    case 0: return 0;
    case 1: return 1;
    case 2: return (total + 1) / 2;
    case 3: return (total * 3 + 3) / 4;
    default: return total;
  }
}

const char *fm_nivel_nome(int nivel) {
  static const char *const nome[] = { "Ainda não", "Primeiro passo", "Meio caminho", "Reta final",
                                      "Filmografia completa", "Em ordem" };
  return nome[nivel < 0 ? 0 : nivel > 5 ? 5 : nivel];
}

static void conquista(FmConquista *c, const char *chave, const char *nome, int trilha, int visto,
                      int total, int nivel) {
  cpy(c->chave, sizeof c->chave, chave);
  cpy(c->nome, sizeof c->nome, nome);
  c->trilha = trilha; c->visto = visto; c->total = total; c->nivel = nivel;
}

int fm_conquistas(long tmdb, const char *curto, const FmCredito *l, const FmEstado *st, int n,
                  FmConquista *out) {
  static const struct { int papel; const char *pfx, *nome; int min; } R[2] = {
    { FM_ATUA, "ator", "Filmografia · Atuação", 1 }, { FM_DIR, "dir", "Filmografia · Direção", 3 } };
  int k = 0, lib = 0, vistos = 0, a = -1, z = -1;
  char chave[48], nome[64];
  if (n > FM_MAX) n = FM_MAX;
  for (int r = 0; r < 2; r++) {
    int idx[FM_MAX], nv = 0, tot = 0, datado = 1, ordem = 0;
    for (int i = 0; i < n; i++) {
      if (l[i].ruido || l[i].proximo || !(l[i].papeis & R[r].papel)) continue;
      tot++;
      if (!fm_conta(&st[i])) continue;
      idx[nv++] = i;
      if (st[i].manual || !st[i].em[0]) datado = 0;
    }
    if (tot < R[r].min) continue;
    if (nv == tot && datado) {
      for (int i = 1; i < nv; i++) {
        int v = idx[i], j = i;
        while (j > 0 && strcmp(st[v].em, st[idx[j - 1]].em) < 0) { idx[j] = idx[j - 1]; j--; }
        idx[j] = v;
      }
      ordem = 1;
      for (int j = 1; j < nv; j++) if (l[idx[j]].ano < l[idx[j - 1]].ano) ordem = 0;
    }
    snprintf(chave, sizeof chave, "%s:%ld", R[r].pfx, tmdb);
    conquista(&out[k++], chave, R[r].nome, 1, nv, tot, cqregra_nivel(nv, tot, ordem));
  }
  for (int i = 0; i < n; i++) {
    if (l[i].ruido || l[i].proximo) continue;
    lib++;
    if (fm_conta(&st[i])) vistos++;
    if (l[i].ano <= 0) continue;
    if (a < 0 || l[i].ano < l[a].ano) a = i;
    if (z < 0 || l[i].ano >= l[z].ano) z = i;
  }
  if (lib >= 5) {
    snprintf(chave, sizeof chave, "ator:%ld/cinco", tmdb);
    snprintf(nome, sizeof nome, "Cinco de %s", curto);
    conquista(&out[k++], chave, nome, 0, vistos > 5 ? 5 : vistos, 5, vistos >= 5 ? 1 : 0);
  }
  if (a >= 0 && z >= 0 && a != z) {
    int v = fm_conta(&st[a]) + fm_conta(&st[z]);
    snprintf(chave, sizeof chave, "ator:%ld/ponta", tmdb);
    conquista(&out[k++], chave, "Da estreia ao último", 0, v, 2, v == 2 ? 1 : 0);
  }
  return k;
}

int fm_subiu(const FmConquista *antes_, int na, const FmConquista *depois, int nd) {
  int subiu = 0;
  for (int i = 0; i < nd; i++) {
    int ant = 0;
    for (int j = 0; j < na; j++) if (!strcmp(antes_[j].chave, depois[i].chave)) ant = antes_[j].nivel;
    if (depois[i].nivel > ant) subiu++;
  }
  return subiu;
}

// ---- Grade.
void fm_girar(FmFiltro *f, int qual) {
  switch (qual) {
    case 0: f->ordem = (f->ordem + 1) % FM_O_N; break;
    case 1: f->tipo = (f->tipo + 1) % FM_T_N; break;
    case 2: f->papel = (f->papel + 1) % FM_P_N; break;
    case 3: f->status = (f->status + 1) % FM_S_N; break;
    default: break;
  }
}

int fm_filtrar(const FmCredito *l, const FmEstado *st, int n, const FmFiltro *f, int *idx, int max) {
  static const int bit[FM_P_N] = { 0, FM_ATUA, FM_DIR, FM_ROT, 0 };
  int k = 0;
  for (int i = 0; i < n && k < max; i++) {
    const FmCredito *c = &l[i];
    if (f->tipo == FM_T_FILME && c->tv) continue;
    if (f->tipo == FM_T_SERIE && !c->tv) continue;
    if (f->papel == FM_P_VOZ ? !c->voz : (f->papel != FM_P_TODOS && !(c->papeis & bit[f->papel]))) continue;
    if (f->status == FM_S_VISTOS && st[i].estado != FM_VISTO) continue;
    if (f->status == FM_S_NAO && st[i].estado != FM_NAO && st[i].estado != FM_ANDA) continue;
    if (f->ano && c->ano != f->ano) continue;
    idx[k++] = i;
  }
  ordena(idx, k, l, f->ordem == FM_O_NOVOS ? 0 : f->ordem == FM_O_ANTIGOS ? 1 : 2, 0);
  return k;
}

int fm_proximo(const FmCredito *l, const FmEstado *st, int n, int prefer, const FmConquista *cq,
               int ncq, int *motivo, int *nivel) {
  int cand[FM_MAX], nc = 0;
  if (n > FM_MAX) n = FM_MAX;
  *motivo = 0; *nivel = 0;
  for (int i = 0; i < n; i++)
    if (!l[i].ruido && st[i].estado == FM_ANDA) { *motivo = FM_PROX_CONTINUAR; return i; }
  for (int i = 0; i < n; i++) if (!l[i].ruido && st[i].estado == FM_NAO) cand[nc++] = i;
  if (!nc) return -1;
  ordena(cand, nc, l, 3, prefer);
  for (int t = 0; t < ncq; t++) {
    if (!cq[t].trilha || cq[t].nivel >= 4) continue;
    if (fm_precisa(cq[t].nivel + 1, cq[t].total) - cq[t].visto != 1) continue;
    int papel = !strncmp(cq[t].chave, "dir:", 4) ? FM_DIR : FM_ATUA;
    for (int i = 0; i < nc; i++) {
      if (!(l[cand[i]].papeis & papel)) continue;
      *motivo = FM_PROX_FECHA;
      *nivel = cqregra_nivel(cq[t].visto + 1, cq[t].total, 0);
      return cand[i];
    }
  }
  *motivo = FM_PROX_VOTADO;
  return cand[0];
}

// ---- Quiz.
int fm_spoiler(const char *t) {
  static const char *const termo[] = { "twist", "ending", "final", "reviravolta", "morte", "morre",
    "death", "dies", "dead", "murder", "assassin", "kill", "matou", "culpad", "suic", "traic", "traiç" };
  for (size_t i = 0; i < sizeof termo / sizeof *termo; i++) if (acha(t, termo[i], 0)) return 1;
  return 0;
}

typedef struct { uint32_t s; } Rng;

static uint32_t rnd(Rng *r) {
  uint32_t t;
  r->s += 0x6D2B79F5u;
  t = (r->s ^ (r->s >> 15)) * (1u | r->s);
  t = (t + (t ^ (t >> 7)) * (61u | t)) ^ t;
  return t ^ (t >> 14);
}

static int rnd_n(Rng *r, int n) { return (int)(((uint64_t)rnd(r) * (uint32_t)n) >> 32); }

static void emb_s(Rng *r, const char **a, int n) {
  for (int i = n - 1; i > 0; i--) { int j = rnd_n(r, i + 1); const char *t = a[i]; a[i] = a[j]; a[j] = t; }
}

static void emb_i(Rng *r, int *a, int n) {
  for (int i = n - 1; i > 0; i--) { int j = rnd_n(r, i + 1); int t = a[i]; a[i] = a[j]; a[j] = t; }
}

typedef struct {
  const char *const *p, *const *c, *const *t;
  int np, nc, nt;
} Decoys;

// Ate 3 erradas de `pool`: sem as de `ex`, sem repetir, sem spoiler. Devolve quantas.
static int erradas(Rng *r, const char *const *pool, int np, const char *const *ex, int nex,
                   const char *certo, const char **w) {
  const char *c[64];
  int nc = 0;
  for (int i = 0; i < np && nc < 64; i++) {
    const char *p = pool[i];
    int dup = 0;
    if (!p || !*p || fm_spoiler(p) || !strcmp(p, certo)) continue;
    for (int e = 0; e < nex; e++) if (!strcmp(ex[e], p)) dup = 1;
    for (int k = 0; k < nc; k++) if (!strcmp(c[k], p)) dup = 1;
    if (!dup) c[nc++] = p;
  }
  emb_s(r, c, nc);
  if (nc > 3) nc = 3;
  for (int i = 0; i < nc; i++) w[i] = c[i];
  return nc;
}

static void opcoes(Rng *r, FmPergunta *q, int tipo, const char *certo, const char **w) {
  const char *o[FM_QOPC] = { certo, w[0], w[1], w[2] };
  emb_s(r, o, FM_QOPC);
  q->tipo = tipo;
  for (int i = 0; i < FM_QOPC; i++) {
    cpy(q->opc[i], sizeof q->opc[i], o[i]);
    if (o[i] == certo) q->certa = i;
  }
}

static int monta(Rng *r, FmPergunta *q, const FmCredito *c, const char *pessoa, int cod, const Decoys *d) {
  const char *w[3], *ex[FM_COM_MAX + FM_KW_MAX + 1];
  int tipo = cod >> 8, arg = cod & 255, nex = 0;
  char cert[FM_PAP];
  memset(q, 0, sizeof *q);
  if (tipo == FM_Q_CENA) {
    int s[4] = { 0, 1, 2, 3 };
    emb_i(r, s, 4);
    q->tipo = FM_Q_CENA;
    for (int i = 0; i < 4; i++) { q->cena[i] = s[i]; if (!s[i]) q->certa = i; }
    return 1;
  }
  if (tipo == FM_Q_PERSONAGEM) {
    char *corte;
    fm_personagem(c, cert, sizeof cert);
    if ((corte = strstr(cert, " / "))) *corte = 0;
    ex[nex++] = c->personagem;
    if (!cert[0] || erradas(r, d->p, d->np, ex, nex, cert, w) < 3) return 0;
    opcoes(r, q, tipo, cert, w);
    return 1;
  }
  if (tipo == FM_Q_TEMA) {
    for (int i = 0; i < c->nKw; i++) ex[nex++] = c->kw[i];
    if (erradas(r, d->t, d->nt, ex, nex, c->kw[arg], w) < 3) return 0;
    opcoes(r, q, tipo, c->kw[arg], w);
    return 1;
  }
  {
    const char *co[FM_COM_MAX];
    int nco = 0;
    for (int i = 0; i < c->nCom; i++) {
      ex[nex++] = c->com[i];
      if (strcmp(c->com[i], pessoa)) co[nco++] = c->com[i];
    }
    ex[nex++] = pessoa;
    if (!nco) return 0;
    const char *certo = co[rnd_n(r, nco)];
    if (erradas(r, d->c, d->nc, ex, nex, certo, w) < 3) return 0;
    opcoes(r, q, FM_Q_COLEGA, certo, w);
    return 1;
  }
}

void fm_quiz_montar(FmQuiz *z, const FmCredito *c, const char *pessoa, int cenaOk,
                    const char *const *decoyP, int nDP, const char *const *decoyC, int nDC,
                    const char *const *decoyT, int nDT, unsigned semente) {
  Decoys d = { decoyP, decoyC, decoyT, nDP, nDC, nDT };
  Rng r = { fm_hash(c->titulo) ^ (uint32_t)c->tmdb ^ (uint32_t)(semente * 2654435761u) };
  int b[FM_KW_MAX + 4], nb = 0, colega = 0;
  memset(z, 0, sizeof *z);
  if (cenaOk) b[nb++] = FM_Q_CENA << 8;
  if ((c->papeis & FM_ATUA) && c->personagem[0] && !c->ruido) b[nb++] = FM_Q_PERSONAGEM << 8;
  for (int k = 0; k < c->nKw && k < FM_KW_MAX; k++)
    if (!fm_spoiler(c->kw[k])) b[nb++] = FM_Q_TEMA << 8 | k;
  for (int k = 0; k < c->nCom; k++) if (strcmp(c->com[k], pessoa)) colega = 1;
  if (colega) b[nb++] = FM_Q_COLEGA << 8;
  emb_i(&r, b, nb);
  for (int i = 0; i < nb && z->nq < FM_Q_MAX; i++)
    if (monta(&r, &z->q[z->nq], c, pessoa, b[i], &d)) z->nq++;
  // Duas perguntas do mesmo tipo na abertura so quando nao ha outra saida.
  if (z->nq > 2 && z->q[1].tipo == z->q[0].tipo)
    for (int j = 2; j < z->nq; j++)
      if (z->q[j].tipo != z->q[0].tipo) { FmPergunta t = z->q[1]; z->q[1] = z->q[j]; z->q[j] = t; break; }
  z->nAsk = z->nq < 2 ? z->nq : 2;
  z->ordem[1] = 1;
  z->proxReserva = 2;
}

const FmPergunta *fm_quiz_pergunta(const FmQuiz *z) {
  if (z->resultado || z->pergunta >= z->nAsk) return NULL;
  return &z->q[z->ordem[z->pergunta]];
}

void fm_quiz_responder(FmQuiz *z, int escolha) {
  const FmPergunta *q = fm_quiz_pergunta(z);
  if (!q) return;
  if (escolha == q->certa) z->acertos++;
  z->pergunta++;
  if (z->pergunta < z->nAsk) return;
  if (z->nAsk == 2 && z->acertos == 1 && z->proxReserva < z->nq) {
    z->ordem[2] = z->proxReserva++;
    z->nAsk = 3;
    z->bonus = 1;
    return;
  }
  z->resultado = z->acertos >= 2 ? 1 : 2;
}

int fm_quiz_trocar(FmQuiz *z) {
  if (z->trocou || z->resultado || z->pergunta >= z->nAsk || z->proxReserva >= z->nq) return 0;
  z->ordem[z->pergunta] = z->proxReserva++;
  z->trocou = 1;
  return 1;
}

int fm_tentativas(const long *quando, int n, long agora) {
  int falhas = 0;
  for (int i = 0; i < n; i++) if (agora - quando[i] < FM_JANELA_S) falhas++;
  return falhas >= FM_TENTATIVAS ? 0 : FM_TENTATIVAS - falhas;
}

void fm_data_vista(const char *lancamento, int naoLembro, const char *hoje, char *dst, size_t n) {
  int passou = lancamento && strlen(lancamento) == 10 && strcmp(lancamento, hoje) < 0;
  cpy(dst, n, naoLembro && passou ? lancamento : hoje);
}
