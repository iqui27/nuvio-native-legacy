// FAROLCOMPARA — tela P4 "Comparar". Contrato em farolcompara.h.
//
// Fios: tudo roda na UI, menos a consulta de "Ver qualidade" — um fio
// destacado por pedido, com geracao atomica: cancelar = trocar a geracao;
// o fio confere entre um titulo e outro (e o add-on confere pelo
// `cancelado`) e o que chega de geracao velha e descartado. O resultado
// passa por uma trava e so entra na tela em farolcompara_atualizar.
#include "farolcena.h"          // antes de qualquer farolrec.h (ver farolcena.h)
#include "farolcompara.h"

#include <ctype.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "addons.h"
#include "ajustes.h"
#include "amigostitulo.h"
#include "anim.h"
#include "badges.h"
#include "botoes.h"
#include "catalogo.h"
#include "escala.h"
#include "farolficha.h"
#include "faroltexto.h"
#include "fonteantecipa.h"
#include "fontecache.h"
#include "gfx.h"
#include "horafmt.h"
#include "idbase.h"
#include "idioma.h"
#include "layout.h"
#include "linguas.h"
#include "ondever.h"
#include "perfisessao.h"
#include "player.h"
#include "plrui.h"
#include "ponteiro.h"
#include "streams.h"
#include "tela21.h"
#include "teclavoltar.h"
#include "tex_cache.h"
#include "text.h"

#define FCMP_EIXOS     FAROL_EIXO_MAX
#define FCMP_MEM_MAX   16
#define FCMP_GAP       32.0f
#define FCMP_PAD       24.0f
#define FCMP_POSTER_W  100.0f
#define FCMP_POSTER_H  150.0f
#define FCMP_LINHA_H   52.0f
#define FCMP_TOPO_COL  248.0f

enum { FCMP_PED_NADA, FCMP_PED_ABRIR, FCMP_PED_TOCAR };

static const char *const FCMP_ROTULO[FCMP_EIXOS] = {
  FT_CMP_DURACAO, FT_CMP_NOTA, FT_CMP_CLIMA, FT_CMP_PORQUE,
  FT_CMP_QUALIDADE, FT_CMP_TERMINA, FT_CMP_ONDE, FT_CMP_AMIGOS
};

// ------------------------------------------------------------ estado da tela
static struct {
  int     aberto;
  float   entrada, animFoco[FCMP_COL_MAX], animBotao;
  int     n;
  FarolCompItem it[FCMP_COL_MAX];
  uint8_t eixos[FCMP_EIXOS];    // os da cena, na ordem
  int     nEixos;
  uint8_t vis[FCMP_EIXOS];      // os que sobem (algum dado)
  int     nVis;
  char    cel[FCMP_COL_MAX][FCMP_EIXOS][FCMP_CEL_TAM];
  int     escolha, foco, focoBotao, armado;
  char    veredito[256];
  int     pedido, pedCat;
  char    pedRef[FAROL_REF_TAM];
  int     consultando;
  unsigned genAtual;
  uint64_t sessao;
} S;

static FarolCena fcCena;                 // ~26 KB: fora da pilha
static char fcJson[FAROL_CENA_MAX + 1];

// Ganchos de teste.
static FarolCompConsulta fcFnConsulta;
static int fcCapsHdr = -2, fcCapsDv = -2;
static long long (*fcFnEpoch)(void);
static int (*fcFnPlayer)(void);
static uint64_t (*fcFnSessao)(void);

// A sessao de perfil em que a tela abriu: trocar de perfil com o Comparar
// aberto fecha a tela (os itens e a memoria sao da sessao antiga).
static uint64_t fcmp_sessao(void) {
  return fcFnSessao ? fcFnSessao() : nv_perfil_sessao_ler();
}

// Memoria da sessao: fontes ja consultadas por id.
static struct { char id[64]; int n; FarolInfoFonte f[FCMP_FONTE_MAX]; } fcMem[FCMP_MEM_MAX];
static int fcMemN, fcMemProx;
static uint64_t fcMemSessao;
static int fcMemSessaoOk;

// Consulta assincrona.
typedef struct {
  unsigned gen;
  int nCols;
  int col[FCMP_COL_MAX];
  char id[FCMP_COL_MAX][64];
  char tipo[FCMP_COL_MAX][8];
  FarolCompConsulta fn;
  AddonsConsulta *consulta;
} FcmpJob;

static pthread_mutex_t fcTrava = PTHREAD_MUTEX_INITIALIZER;
static atomic_uint fcGen;
static atomic_int fcVivos;
static struct {
  int pronto;
  unsigned gen;
  int col, n;
  char id[64];
  FarolInfoFonte f[FCMP_FONTE_MAX];
} fcRes[FCMP_COL_MAX];
static unsigned fcFimGen;                // geracao cujo fio terminou

// ------------------------------------------------------------ utilidades
static void fcmp_copia(char *dst, size_t cap, const char *s) {
  if (!dst || !cap) return;
  snprintf(dst, cap, "%s", s ? s : "");
}

static int fcmp_reduzida(void) {
  return anim_politica_reduzida || ajustes_animacoes_reduzidas();
}

static int fcmp_player(void) { return fcFnPlayer ? fcFnPlayer() : player_aberto(); }

static long long fcmp_agora(void) {
  return fcFnEpoch ? fcFnEpoch() : (long long)time(NULL);
}

static const char *fcmp_id_fontes(const FarolCompItem *it) {
  return it->idFontes[0] ? it->idFontes : it->ref;
}

static const struct { const char *nome; const char *lit; } FCMP_MODS[] = {
#define FCMP_MOD_X(nome) { "FT_" #nome, FT_##nome },
  FAROL_FRASES(FCMP_MOD_X)
#undef FCMP_MOD_X
};

static const char *fcmp_modelo(const char *nome) {
  size_t i;
  if (!nome || !nome[0]) return NULL;
  for (i = 0; i < sizeof FCMP_MODS / sizeof *FCMP_MODS; i++)
    if (!strcmp(FCMP_MODS[i].nome, nome)) return FCMP_MODS[i].lit;
  return NULL;
}

// Encaixa so %s e %d, na ordem; aridade errada = sem frase.
static int fcmp_encaixar(const char *fmt, char args[][FCMP_CEL_TAM], int n,
                         char *dst, size_t cap) {
  size_t o = 0;
  int ai = 0;
  const char *p = fmt;
  if (!fmt || !dst || !cap) return 0;
  while (*p && o + 1 < cap) {
    const char *v;
    char num[24];
    size_t k;
    if (*p != '%') { dst[o++] = *p++; continue; }
    p++;
    if (*p == '%') { dst[o++] = *p++; continue; }
    if ((*p != 's' && *p != 'd') || ai >= n) return 0;
    if (*p == 'd') { snprintf(num, sizeof num, "%d", atoi(args[ai])); v = num; }
    else v = args[ai];
    ai++; p++;
    k = strlen(v);
    if (o + k >= cap) k = cap - 1 - o;
    memcpy(dst + o, v, k);
    o += k;
  }
  dst[o] = 0;
  return ai == n && o > 0;
}

static void fcmp_idioma(const char *cod, char *dst, size_t cap) {
  char base[12];
  const char *nm;
  size_t i = 0;
  dst[0] = 0;
  if (!cod || !cod[0]) return;
  while (cod[i] && cod[i] != '-' && cod[i] != '_' && i + 1 < sizeof base) {
    base[i] = (char)tolower((unsigned char)cod[i]);
    i++;
  }
  base[i] = 0;
  if (!strcmp(base, "pob") || !strcmp(base, "ptb") || !strcmp(base, "br") ||
      !strcmp(base, "por")) snprintf(base, sizeof base, "pt");
  else if (!strcmp(base, "eng")) snprintf(base, sizeof base, "en");
  else if (!strcmp(base, "spa") || !strcmp(base, "esp")) snprintf(base, sizeof base, "es");
  nm = i18n(ling_nome(base[0] ? base : cod));
  snprintf(dst, cap, "%s", nm && nm[0] ? nm : cod);
}

static void fcmp_primeiro_iso(const char *csv, char *dst, size_t cap) {
  size_t i = 0;
  dst[0] = 0;
  if (!csv) return;
  while (*csv == ' ' || *csv == ',' || *csv == ';') csv++;
  while (csv[i] && csv[i] != ',' && csv[i] != ';' && csv[i] != ' ' && i + 1 < cap) {
    dst[i] = (char)tolower((unsigned char)csv[i]);
    i++;
  }
  dst[i] = 0;
}

// Resumo de uma fonte a partir so dos campos dela (vale em qualquer fio).
// Sem a sondagem da lista global, "toca" so e afirmado com cache confirmado.
static void fcmp_resumir(const Stream *s, FarolInfoFonte *f) {
  int cacheOk;
  memset(f, 0, sizeof *f);
  if (s->altura >= 2000 || (s->badges & badges_bit("r-4k"))) f->altura = 2160;
  else if (s->altura >= 1000 || (s->badges & badges_bit("r-1080"))) f->altura = 1080;
  else if (s->altura >= 700 || (s->badges & badges_bit("r-720"))) f->altura = 720;
  else f->altura = s->altura;
  f->dv = (s->dolbyVision ||
           (s->badges & (badges_bit("v-dv") | badges_bit("a-atmos-dv") |
                         badges_bit("a-truehd-dv") | badges_bit("a-dd-dv")))) ? 1 : 0;
  f->hdr = (!f->dv &&
            (s->badges & (badges_bit("v-hdr10plus") | badges_bit("v-hdr10") |
                          badges_bit("v-hdr") | badges_bit("v-hlg")))) ? 1 : 0;
  cacheOk = s->emCache && !s->cacheNegado && (!s->foraCache || s->cacheNativo) &&
            !s->urlP2P && !s->naoVideo;
  if (s->naoVideo) f->toca = 0;
  else if (cacheOk && stream_fonte_disponivel(s)) f->toca = 1;
  else f->toca = -1;
  fcmp_primeiro_iso(s->audioIdiomas, f->audio, sizeof f->audio);
  if (!f->audio[0]) {
    const char *id = stream_idioma(s);
    if (id && (!strcmp(id, "Dublado") || !strcmp(id, "Dual áudio")))
      snprintf(f->audio, sizeof f->audio, "pt");
  }
  fcmp_primeiro_iso(s->legendaIdiomas, f->legenda, sizeof f->legenda);
  if (!f->legenda[0]) {
    const char *id = stream_idioma(s);
    if (id && !strcmp(id, "Legendado")) snprintf(f->legenda, sizeof f->legenda, "pt");
  }
}

// Corta em `max` DEPOIS de priorizar quem toca: os 24 primeiros da lista
// podem ser todos inconclusivos (toca != 1) e a fonte confirmada estar na
// posicao 28 — truncar antes descartava exatamente a que sustentava a linha.
static int fcmp_resumir_lista(const Stream *l, int n, FarolInfoFonte *out, int max) {
  int i, pass, k = 0;
  if (!l || max <= 0) return 0;
  for (pass = 0; pass < 2 && k < max; pass++)
    for (i = 0; i < n && k < max; i++) {
      fcmp_resumir(&l[i], &out[k]);
      if ((pass == 0) == (out[k].toca == 1)) k++;
    }
  return k;
}

// ------------------------------------------------------------ memoria
static void fcmp_mem_sessao(void) {
  uint64_t g = fcmp_sessao();
  if (fcMemSessaoOk && g == fcMemSessao) return;
  memset(fcMem, 0, sizeof fcMem);
  fcMemN = fcMemProx = 0;
  fcMemSessao = g;
  fcMemSessaoOk = 1;
}

static int fcmp_mem_achar(const char *id) {
  int i;
  if (!id || !id[0]) return -1;
  fcmp_mem_sessao();
  for (i = 0; i < fcMemN; i++) if (!strcmp(fcMem[i].id, id)) return i;
  return -1;
}

void farolcompara_lembrar_fontes(const char *ref, const FarolInfoFonte *f, int n) {
  int i;
  if (!ref || !ref[0] || n < 0) return;
  if (n > FCMP_FONTE_MAX) n = FCMP_FONTE_MAX;
  i = fcmp_mem_achar(ref);
  if (i < 0) {
    if (fcMemN < FCMP_MEM_MAX) i = fcMemN++;
    else { i = fcMemProx; fcMemProx = (fcMemProx + 1) % FCMP_MEM_MAX; }
  }
  fcmp_copia(fcMem[i].id, sizeof fcMem[i].id, ref);
  fcMem[i].n = n;
  if (n && f) memcpy(fcMem[i].f, f, (size_t)n * sizeof *f);
}

static void fcmp_fontes_da_memoria(FarolCompItem *it) {
  int i;
  if (it->nFontes >= 0) return;
  i = fcmp_mem_achar(fcmp_id_fontes(it));
  if (i < 0) return;
  it->nFontes = fcMem[i].n;
  if (fcMem[i].n) memcpy(it->fontes, fcMem[i].f, (size_t)fcMem[i].n * sizeof *it->fontes);
}

// ------------------------------------------------------------ celulas
static void fcmp_caps(FarolInfoCaps *c) {
  int hdr = -1, dv = -1, hevc = -1, av1 = -1;
  if (fcCapsHdr == -2 || fcCapsDv == -2) stream_capacidades(&hdr, &dv, &hevc, &av1);
  c->hdr = fcCapsHdr != -2 ? fcCapsHdr : hdr;
  c->dv = fcCapsDv != -2 ? fcCapsDv : dv;
}

static void fcmp_cel_qualidade(const FarolCompItem *it, const FarolInfoCaps *caps,
                               char *dst, size_t cap) {
  FarolInfoLinha l[2];
  size_t o = 0;
  int i, audio = -1;
  dst[0] = 0;
  if (it->nFontes <= 0) return;
  if (farolinfo_qualidade(l, 2, it->fontes, it->nFontes, caps) <= 0) return;
  if (!strcmp(l[0].m, "FT_INFO_QUALI")) audio = 2;
  else if (!strcmp(l[0].m, "FT_INFO_QUALI_AUDIO")) audio = 1;
  else if (!strcmp(l[0].m, "FT_INFO_IDIOMA")) audio = 0;
  else if (strcmp(l[0].m, "FT_INFO_QUALI_SO")) return;
  for (i = 0; i < l[0].na && o + 1 < cap; i++) {
    char v[64];
    if (i == audio) fcmp_idioma(l[0].a[i], v, sizeof v);
    else fcmp_copia(v, sizeof v, l[0].a[i]);
    if (!v[0]) continue;
    o += (size_t)snprintf(dst + o, cap - o, "%s%s", o ? " · " : "", v);
    if (o >= cap) { dst[cap - 1] = 0; break; }
  }
}

static void fcmp_cel_termina(const FarolCompItem *it, char *dst, size_t cap) {
  FarolInfoLinha l;
  time_t t = (time_t)fcmp_agora();
  struct tm tm;
  dst[0] = 0;
  if (it->minutos <= 0) return;
  if (!localtime_r(&t, &tm)) return;
  if (farolinfo_tempo(&l, &tm, ajustes_relogio_12h(), it->minutos, 0) != 1) return;
  if (!strcmp(l.m, "FT_INFO_TERMINA") && l.na == 1) fcmp_copia(dst, cap, l.a[0]);
}

static void fcmp_celulas(void) {
  FarolInfoCaps caps;
  int c, e;
  fcmp_caps(&caps);
  memset(S.cel, 0, sizeof S.cel);
  for (c = 0; c < S.n; c++) {
    const FarolCompItem *it = &S.it[c];
    char *k;
    int m = it->minutos;
    k = S.cel[c][FP_EIXO_DURACAO];
    if (m > 0 && m < 60) snprintf(k, FCMP_CEL_TAM, "%d min", m);
    else if (m > 0 && m % 60) snprintf(k, FCMP_CEL_TAM, "%d h %02d min", m / 60, m % 60);
    else if (m > 0) snprintf(k, FCMP_CEL_TAM, "%d h", m / 60);
    if (it->notaPct > 0 && it->notaPct <= 100)
      snprintf(S.cel[c][FP_EIXO_NOTA], FCMP_CEL_TAM, "%d%%", it->notaPct);
    fcmp_copia(S.cel[c][FP_EIXO_CLIMA], FCMP_CEL_TAM, it->clima);
    fcmp_copia(S.cel[c][FP_EIXO_PORQUE], FCMP_CEL_TAM, it->porque);
    fcmp_cel_qualidade(it, &caps, S.cel[c][FP_EIXO_QUALIDADE], FCMP_CEL_TAM);
    fcmp_cel_termina(it, S.cel[c][FP_EIXO_TERMINA], FCMP_CEL_TAM);
    fcmp_copia(S.cel[c][FP_EIXO_ONDE], FCMP_CEL_TAM, it->onde);
    fcmp_copia(S.cel[c][FP_EIXO_AMIGOS], FCMP_CEL_TAM, it->amigos);
  }
  S.nVis = 0;
  for (e = 0; e < S.nEixos; e++) {
    int tem = 0;
    for (c = 0; c < S.n; c++) if (S.cel[c][S.eixos[e]][0]) tem = 1;
    if (tem) S.vis[S.nVis++] = S.eixos[e];
  }
}

static int fcmp_tem_eixo_cena(int eixo) {
  int e;
  for (e = 0; e < S.nEixos; e++) if (S.eixos[e] == eixo) return 1;
  return 0;
}

int farolcompara_botao_visivel(void) {
  int c;
  if (!S.aberto || fcmp_player() || !fcmp_tem_eixo_cena(FP_EIXO_QUALIDADE)) return 0;
  for (c = 0; c < S.n; c++) if (S.it[c].nFontes < 0) return 1;
  return 0;
}

// ------------------------------------------------------------ consulta
static int fcmp_cancelado(void *ctx) {
  const FcmpJob *j = ctx;
  return atomic_load(&fcGen) != j->gen;
}

static int fcmp_consulta_real(FcmpJob *j, int i, FarolInfoFonte *out, int max) {
  Stream *l = NULL;
  int n, k;
  if (!j->consulta) return -1;
  n = addons_consulta_consultar(j->consulta, j->id[i], j->tipo[i], 4,
                                fcmp_cancelado, j, &l);
  if (n < 0) { free(l); return -1; }
  k = fcmp_resumir_lista(l, n, out, max);
  free(l);
  return k;
}

static void *fcmp_fio(void *arg) {
  FcmpJob *j = arg;
  FarolInfoFonte f[FCMP_FONTE_MAX];
  int i;
  for (i = 0; i < j->nCols; i++) {
    int n;
    if (fcmp_cancelado(j)) break;
    n = j->fn ? j->fn(j->id[i], j->tipo[i], fcmp_cancelado, j, f, FCMP_FONTE_MAX)
              : fcmp_consulta_real(j, i, f, FCMP_FONTE_MAX);
    if (n > FCMP_FONTE_MAX) n = FCMP_FONTE_MAX;
    pthread_mutex_lock(&fcTrava);
    if (!fcmp_cancelado(j)) {
      fcRes[i].pronto = 1;
      fcRes[i].gen = j->gen;
      fcRes[i].col = j->col[i];
      fcRes[i].n = n;
      fcmp_copia(fcRes[i].id, sizeof fcRes[i].id, j->id[i]);
      if (n > 0) memcpy(fcRes[i].f, f, (size_t)n * sizeof *f);
    }
    pthread_mutex_unlock(&fcTrava);
  }
  pthread_mutex_lock(&fcTrava);
  // Um fio cancelado nao carimba o fim: ele podia terminar DEPOIS do fio da
  // consulta nova e regredir fcFimGen, deixando S.consultando preso em 1.
  if (!fcmp_cancelado(j) && j->gen > fcFimGen) fcFimGen = j->gen;
  pthread_mutex_unlock(&fcTrava);
  addons_consulta_liberar(j->consulta);
  free(j);
  atomic_fetch_sub(&fcVivos, 1);
  return NULL;
}

static void fcmp_cancelar(void) {
  S.genAtual = atomic_fetch_add(&fcGen, 1) + 1;
  S.consultando = 0;
  pthread_mutex_lock(&fcTrava);
  memset(fcRes, 0, sizeof fcRes);
  pthread_mutex_unlock(&fcTrava);
}

static void fcmp_consultar(void) {
  FcmpJob *j;
  pthread_t t;
  pthread_attr_t at;
  int c, ok;
  if (S.consultando || !farolcompara_botao_visivel()) return;
  j = calloc(1, sizeof *j);
  if (!j) return;
  for (c = 0; c < S.n; c++) {
    const FarolCompItem *it = &S.it[c];
    if (it->nFontes >= 0) continue;
    j->col[j->nCols] = c;
    fcmp_copia(j->id[j->nCols], sizeof j->id[0], fcmp_id_fontes(it));
    fcmp_copia(j->tipo[j->nCols], sizeof j->tipo[0], it->tipo[0] ? it->tipo : "movie");
    j->nCols++;
  }
  j->fn = fcFnConsulta;
  if (!j->fn && !(j->consulta = addons_consulta_capturar())) { free(j); return; }
  fcmp_cancelar();
  j->gen = S.genAtual;
  atomic_fetch_add(&fcVivos, 1);
  pthread_attr_init(&at);
  pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
  pthread_attr_setstacksize(&at, 512 * 1024);
  ok = pthread_create(&t, &at, fcmp_fio, j) == 0;
  pthread_attr_destroy(&at);
  if (!ok) {
    atomic_fetch_sub(&fcVivos, 1);
    addons_consulta_liberar(j->consulta);
    free(j);
    return;
  }
  S.consultando = 1;
}

// Entrega os resultados da geracao atual (UI).
static void fcmp_colher(void) {
  int i, mudou = 0;
  pthread_mutex_lock(&fcTrava);
  for (i = 0; i < FCMP_COL_MAX; i++) {
    FarolCompItem *it;
    if (!fcRes[i].pronto) continue;
    fcRes[i].pronto = 0;
    if (fcRes[i].gen != S.genAtual || fcRes[i].col < 0 || fcRes[i].col >= S.n) continue;
    it = &S.it[fcRes[i].col];
    if (strcmp(fcmp_id_fontes(it), fcRes[i].id)) continue;
    if (fcRes[i].n >= 0) {
      it->nFontes = fcRes[i].n;
      if (fcRes[i].n) memcpy(it->fontes, fcRes[i].f, (size_t)fcRes[i].n * sizeof *it->fontes);
      farolcompara_lembrar_fontes(fcRes[i].id, it->fontes, it->nFontes);
    } else it->nFontes = 0;   // consulta sem resposta: nada a afirmar
    mudou = 1;
  }
  if (S.consultando && (fcFimGen == S.genAtual || atomic_load(&fcVivos) == 0))
    S.consultando = 0;
  pthread_mutex_unlock(&fcTrava);
  if (mudou) fcmp_celulas();
}

// ------------------------------------------------------------ abertura
static void fcmp_estado_limpo(void) {
  fcmp_cancelar();
  S.aberto = 0;
  S.n = S.nEixos = S.nVis = 0;
  S.escolha = -1;
  S.foco = S.focoBotao = S.armado = 0;
  S.veredito[0] = 0;
  S.animBotao = 0;
  memset(S.animFoco, 0, sizeof S.animFoco);
}

static const FarolCompItem *fcmp_resolver(const FarolRef *r, const FarolCompItem *itens, int n) {
  int i;
  if (r->c >= 0) return r->c < n ? &itens[r->c] : NULL;
  for (i = 0; i < n; i++) if (!strcmp(itens[i].ref, r->s)) return &itens[i];
  return NULL;
}

static void fcmp_veredito(const FarolTexto *t, const FarolCompItem *itens, int n) {
  char args[FAROL_ARGS][FCMP_CEL_TAM];
  const char *lit;
  int i;
  S.veredito[0] = 0;
  if (!farolpeca_texto_tem(t)) return;
  if (t->usaX) { fcmp_copia(S.veredito, sizeof S.veredito, farolpeca_x(&fcCena, t)); return; }
  if (!(lit = fcmp_modelo(t->m)) || t->nArgs > FAROL_ARGS) return;
  for (i = 0; i < t->nArgs; i++) {
    const char *a = t->a[i];
    if ((a[0] == 'c' && isdigit((unsigned char)a[1])) || (a[0] == 't' && a[1] == 't')) {
      FarolRef r;
      const FarolCompItem *it;
      memset(&r, 0, sizeof r);
      r.c = a[0] == 'c' ? (int16_t)atoi(a + 1) : -1;
      fcmp_copia(r.s, sizeof r.s, a);
      if (!(it = fcmp_resolver(&r, itens, n)) || !it->titulo[0]) return;
      fcmp_copia(args[i], sizeof args[i], it->titulo);
    } else fcmp_copia(args[i], sizeof args[i], a);
  }
  if (!fcmp_encaixar(i18n(lit), args, t->nArgs, S.veredito, sizeof S.veredito))
    S.veredito[0] = 0;
}

int farolcompara_abrir_cena(const char *json, const FarolCompItem *itens, int nItens) {
  const FarolPeca *p = NULL;
  int i, j;
  if (!json || !itens || nItens < 2 || fcmp_player()) return 0;
  if (farolpeca_ler(&fcCena, json, nItens) <= 0 || strcmp(fcCena.tela, "comparar")) return 0;
  for (i = 0; i < fcCena.nPecas && !p; i++)
    if (fcCena.pecas[i].tipo == FP_COMPARAR) p = &fcCena.pecas[i];
  if (!p || p->u.comparar.nItens < 2 || p->u.comparar.nItens > FCMP_COL_MAX) return 0;
  {
    const FarolCompItem *col[FCMP_COL_MAX];
    int nc = p->u.comparar.nItens;
    for (i = 0; i < nc; i++) {
      if (!(col[i] = fcmp_resolver(&p->u.comparar.itens[i], itens, nItens))) return 0;
      for (j = 0; j < i; j++) if (col[j] == col[i] || !strcmp(col[j]->ref, col[i]->ref)) return 0;
    }
    fcmp_estado_limpo();
    S.n = nc;
    for (i = 0; i < nc; i++) S.it[i] = *col[i];
    S.sessao = fcmp_sessao();
  }
  S.nEixos = 0;
  for (i = 0; i < p->u.comparar.nEixos && i < FCMP_EIXOS; i++)
    if (p->u.comparar.eixos[i] <= FP_EIXO_AMIGOS) S.eixos[S.nEixos++] = p->u.comparar.eixos[i];
  S.escolha = p->u.comparar.escolha >= 0 && p->u.comparar.escolha < S.n ? p->u.comparar.escolha : -1;
  fcmp_veredito(&p->u.comparar.veredito, itens, nItens);
  for (i = 0; i < S.n; i++) fcmp_fontes_da_memoria(&S.it[i]);
  fcmp_celulas();
  S.foco = S.escolha >= 0 ? S.escolha : 0;
  S.pedido = FCMP_PED_NADA;
  S.aberto = 1;
  S.entrada = fcmp_reduzida() ? 1 : 0;
  for (i = 0; i < S.n; i++) S.animFoco[i] = (fcmp_reduzida() && i == S.foco) ? 1 : 0;
  return 1;
}

int farolcompara_abrir(const FarolCompItem *itens, int n, int tempoMin) {
  static FarolCompItem uniq[FCMP_COL_MAX];
  static const char *const REFS[FCMP_COL_MAX] = { "c0", "c1", "c2" };
  FarolCenaCtx ctx;
  FarolCenaCmp cmp;
  int i, j, k = 0;
  if (!itens || n < 2 || fcmp_player()) return 0;
  for (i = 0; i < n && k < FCMP_COL_MAX; i++) {
    int rep = !itens[i].ref[0];
    for (j = 0; j < k && !rep; j++) if (!strcmp(uniq[j].ref, itens[i].ref)) rep = 1;
    if (!rep) uniq[k++] = itens[i];
  }
  if (k < 2) return 0;
  memset(&ctx, 0, sizeof ctx);
  ctx.nCand = k;
  ctx.perfil = -1;
  memset(&cmp, 0, sizeof cmp);
  cmp.nItens = k;
  cmp.tempoMin = tempoMin > 0 ? tempoMin : 0;
  for (i = 0; i < k; i++) {
    cmp.itens[i] = REFS[i];
    cmp.pontos[i] = uniq[i].pontos;
    cmp.minutos[i] = (uint16_t)(uniq[i].minutos > 0 && uniq[i].minutos < 65535 ? uniq[i].minutos : 0);
  }
  for (i = 0; i < FCMP_EIXOS; i++) cmp.eixos[i] = (uint8_t)i;
  cmp.nEixos = FCMP_EIXOS;
  if (farolcena_comparar(fcJson, sizeof fcJson, &ctx, &cmp) < 0) return 0;
  return farolcompara_abrir_cena(fcJson, uniq, k);
}

// Primeiro genero da ficha que nao e o tipo ("Programa de TV · Drama").
static void fcmp_clima(const char *genero, char *dst, size_t cap) {
  char tmp[160];
  char *p, *sep;
  dst[0] = 0;
  if (!genero || !genero[0]) return;
  fcmp_copia(tmp, sizeof tmp, genero);
  for (p = tmp; p && *p; p = sep) {
    sep = strstr(p, " · ");
    if (sep) { *sep = 0; sep += strlen(" · "); }
    if (!strcmp(p, "Programa de TV") || !strcmp(p, i18n("Programa de TV")) ||
        !strcmp(p, "Filme") || !strcmp(p, i18n("Filme")) ||
        !strcmp(p, "Série") || !strcmp(p, i18n("Série"))) continue;
    fcmp_copia(dst, cap, p);
    return;
  }
}

static void fcmp_amigos(const char *imdb, char *dst, size_t cap) {
  static AmigosTitulo at;
  FarolInfoAmigo am[AMT_MAX];
  FarolInfoLinha l;
  char args[FAROL_ARGS][FCMP_CEL_TAM];
  const char *lit;
  int i, n = 0;
  dst[0] = 0;
  if (!amigostitulo_obter(imdb, &at)) return;
  for (i = 0; i < at.n && i < AMT_MAX; i++) {
    memset(&am[n], 0, sizeof am[n]);
    amigostitulo_primeiro_nome(at.a[i].nome, am[n].nome, sizeof am[n].nome);
    am[n].terminou = at.a[i].terminou;
    am[n].gostou = at.a[i].gostou;
    n++;
  }
  if (farolinfo_amigos(&l, am, n) != 1 || !(lit = fcmp_modelo(l.m)) || l.na > FAROL_ARGS) return;
  for (i = 0; i < l.na; i++) fcmp_copia(args[i], sizeof args[i], l.a[i]);
  if (!fcmp_encaixar(i18n(lit), args, l.na, dst, cap)) dst[0] = 0;
}

// Fontes ja em memoria: a desta tela, a lista do alvo atual e o cache VOD.
static void fcmp_fontes_conhecidas(FarolCompItem *it) {
  const char *id = fcmp_id_fontes(it);
  FontecacheEscopo esc;
  Stream *l = NULL;
  int n = 0, i;
  Uint32 idade = 0;
  fcmp_fontes_da_memoria(it);
  if (it->nFontes >= 0) return;
  if (stream_lista_do_alvo(id)) {
    int k = 0, pass;
    // stream_fonte_estado (FA_OK/FA_RUIM) manda em toca, como a P6; e quem
    // toca entra primeiro — cortar nos 24 primeiros perdia confirmadas.
    for (pass = 0; pass < 2 && k < FCMP_FONTE_MAX; pass++)
      for (i = 0; i < stream_n() && k < FCMP_FONTE_MAX; i++) {
        const Stream *s = stream_item(i);
        int est;
        if (!s) continue;
        est = stream_fonte_estado(i);
        fcmp_resumir(s, &it->fontes[k]);
        if (stream_incompativel(i) || est == FA_RUIM) it->fontes[k].toca = 0;
        else if (est == FA_OK && stream_fonte_disponivel(s)) it->fontes[k].toca = 1;
        if ((pass == 0) == (it->fontes[k].toca == 1)) k++;
      }
    it->nFontes = k;
    return;
  }
  addons_capturar_escopo(&esc);
  if (fontecache_vod_pegar(id, it->tipo, "", &esc, &l, &n, &idade) == FC_ACERTO) {
    it->nFontes = fcmp_resumir_lista(l, n, it->fontes, FCMP_FONTE_MAX);
  }
  free(l);
}

int farolcompara_abrir_titulos(const int *catIndices, int n, const int32_t *pontos, int tempoMin) {
  static FarolCompItem it[FCMP_COL_MAX];
  int i, j, k = 0;
  if (!catIndices || n < 2) return 0;
  // Deduplicar ANTES de cortar em 3: [A A B C] tem de abrir A, B e C.
  for (i = 0; i < n && k < FCMP_COL_MAX; i++) {
    const CatItem *c = cat_item(catIndices[i]);
    FarolFicha ff;
    OndeVer ov;
    FarolCompItem *x = &it[k];
    if (!c || !c->imdb[0]) continue;
    for (j = 0; j < k; j++) if (!strcmp(it[j].ref, c->imdb)) break;
    if (j < k) continue;
    memset(x, 0, sizeof *x);
    fcmp_copia(x->ref, sizeof x->ref, c->imdb);
    fcmp_copia(x->tipo, sizeof x->tipo, c->tipo);
    // Serie sem episodio (catalogo, busca, salvos, Farol) consulta como
    // "tt...:1:1" — a mesma convencao de addons_buscar; sem o carimbo o id
    // cru "tt..." nao casa com alvo, cache VOD nem com o que o addon aceita.
    if (!strcmp(c->tipo, "series") && c->temporada > 0 && c->episodio > 0)
      snprintf(x->idFontes, sizeof x->idFontes, "%.40s:%d:%d", c->imdb, c->temporada, c->episodio);
    else if (!strcmp(c->tipo, "series") && idbase_e_imdb(c->imdb) &&
             !idbase_tem_episodio(c->imdb))
      snprintf(x->idFontes, sizeof x->idFontes, "%.40s:1:1", c->imdb);
    x->catIndice = catIndices[i];
    fcmp_copia(x->titulo, sizeof x->titulo, c->titulo);
    fcmp_copia(x->poster, sizeof x->poster, c->poster);
    fcmp_copia(x->meta, sizeof x->meta, c->meta);
    if (farolficha_ler_item(c, &ff)) { x->minutos = ff.minutos; x->notaPct = ff.nota; }
    if (c->nota > 0) x->notaPct = c->nota;
    x->pontos = pontos ? pontos[i] : (int32_t)x->notaPct * 327;
    fcmp_clima(c->genero, x->clima, sizeof x->clima);
    memset(&ov, 0, sizeof ov);
    if (ondever_item(c->imdb, 0, &ov)) fcmp_copia(x->onde, sizeof x->onde, ov.nome);
    fcmp_amigos(c->imdb, x->amigos, sizeof x->amigos);
    x->nFontes = -1;
    fcmp_fontes_conhecidas(x);
    k++;
  }
  return farolcompara_abrir(it, k, tempoMin);
}

void farolcompara_fechar(void) {
  if (!S.aberto) return;
  fcmp_cancelar();
  S.aberto = 0;
  S.armado = 0;
}

// ------------------------------------------------------------ eventos
static void fcmp_pedir(int tipo) {
  const FarolCompItem *it = &S.it[S.foco];
  S.pedido = tipo;
  S.pedCat = it->catIndice;
  fcmp_copia(S.pedRef, sizeof S.pedRef, it->ref);
  farolcompara_fechar();
}

static void fcmp_ok(void) {
  if (S.focoBotao) fcmp_consultar();
  else fcmp_pedir(FCMP_PED_ABRIR);
}

static int fcmp_tecla_play(const SDL_Event *e) {
  SDL_Keycode k = e->key.keysym.sym;
  return k == SDLK_AUDIOPLAY || k == SDLK_PAUSE ||
         e->key.keysym.scancode == SDL_SCANCODE_AUDIOPLAY;
}

void farolcompara_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!S.aberto || !e) return;
  if (e->type == SDL_KEYUP) {
    k = e->key.keysym.sym;
    if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && S.armado) { S.armado = 0; fcmp_ok(); }
    return;
  }
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (nv_tecla_voltar(e)) {
    S.armado = 0;
    if (S.consultando) fcmp_cancelar();
    else farolcompara_fechar();
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { if (!e->key.repeat) S.armado = 1; return; }
  S.armado = 0;
  if (fcmp_tecla_play(e)) { if (!e->key.repeat) fcmp_pedir(FCMP_PED_TOCAR); return; }
  if (S.focoBotao) {
    if (k == SDLK_UP) S.focoBotao = 0;
    return;
  }
  if (k == SDLK_LEFT && S.foco > 0) S.foco--;
  else if (k == SDLK_RIGHT && S.foco + 1 < S.n) S.foco++;
  else if (k == SDLK_DOWN && farolcompara_botao_visivel()) S.focoBotao = 1;
}

void farolcompara_atualizar(float dt, Uint32 agora) {
  int i;
  (void)agora;
  if (S.consultando || atomic_load(&fcVivos) > 0) fcmp_colher();
  if (!S.aberto) { S.entrada = t21_entrada(S.entrada, dt, 0); return; }
  if (fcmp_player() || fcmp_sessao() != S.sessao) { farolcompara_fechar(); return; }
  if (S.focoBotao && !farolcompara_botao_visivel() && !S.consultando) S.focoBotao = 0;
  S.entrada = t21_entrada(S.entrada, dt, 1);
  for (i = 0; i < S.n; i++) {
    float alvo = !S.focoBotao && S.foco == i ? 1.0f : 0.0f;
    S.animFoco[i] = fcmp_reduzida() ? alvo : anim_mola(S.animFoco[i], alvo, dt, NV_MOLA_FOCO);
  }
  S.animBotao = fcmp_reduzida() ? (float)S.focoBotao
                                : anim_mola(S.animBotao, (float)S.focoBotao, dt, NV_MOLA_FOCO);
}

// ------------------------------------------------------------ desenho
// Dados (titulo, metadados, celulas) e frases ja localizadas sobem pelo
// caminho bruto: a camada de texto nao pode traduzir o nome de um titulo.
static void fcmp_focar_col(int a, int b) {
  (void)b;
  if (a >= 0 && a < S.n) { S.foco = a; S.focoBotao = 0; }
}
static void fcmp_ativar_col(int a, int b) {
  (void)b;
  if (a >= 0 && a < S.n) { S.foco = a; S.focoBotao = 0; fcmp_pedir(FCMP_PED_ABRIR); }
}
static void fcmp_focar_botao(int a, int b) { (void)a; (void)b; S.focoBotao = 1; }
static void fcmp_ativar_botao(int a, int b) { (void)a; (void)b; S.focoBotao = 1; fcmp_consultar(); }

static void fcmp_ponto(float x, float y, float a) {
  float r, g, b;
  ajustes_acento(&r, &g, &b);
  gfx_cor((GfxRect){x, y, 10, 10}, .5f, r, g, b, a);
}

static void fcmp_coluna(int c, GfxRect card, float a) {
  const FarolCompItem *it = &S.it[c];
  float f = S.animFoco[c];
  float tx = card.x + FCMP_PAD + FCMP_POSTER_W + 20;
  float tw = card.x + card.w - FCMP_PAD - tx;
  GfxRect poster = {card.x + FCMP_PAD, card.y + FCMP_PAD, FCMP_POSTER_W, FCMP_POSTER_H};
  GLuint tex = it->poster[0] ? tex_obter_larg(it->poster, FCMP_POSTER_W) : 0;
  float y, ty = card.y + FCMP_PAD;
  int e;
  t21_ilha_solida(card, T21_R_CARD, a);
  if (f > .01f) gfx_cor(card, T21_R_CARD / fminf(card.w, card.h), 1, 1, 1, .05f * f * a);
  foco_anel(card, T21_R_CARD / fminf(card.w, card.h), f, a);
  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(it->poster);
    gfx_rect(poster, tex, GFX_CARD, 0, 0, 0, 12.0f / FCMP_POSTER_W, 0, 0, 0, a);
    gfx_tex_aspect_atual = 0;
  } else gfx_cor(poster, 12.0f / FCMP_POSTER_W, 1, 1, 1, .06f * a);
  if (c == S.escolha) { fcmp_ponto(tx, ty + 14, a); }
  {
    float dx = c == S.escolha ? 22 : 0;
    TxtLinha l = txt_linha_corta_bruta(T21_ROW_TITLE, it->titulo, T21_TEXT_R, T21_TEXT_G,
                                 T21_TEXT_B, 255, tw - dx);
    txt_desenhar_alpha(l, tx + dx, ty, a);
    ty += (float)l.h + 8;
  }
  if (it->meta[0]) {
    TxtLinha l = txt_linha_corta_bruta(T21_META, it->meta, T21_TEXT_R, T21_TEXT_G, T21_TEXT_B,
                                 255, tw);
    txt_desenhar_alpha(l, tx, ty, a * T21_TEXT_SECONDARY);
  }
  y = card.y + FCMP_PAD + FCMP_POSTER_H + FCMP_PAD;
  for (e = 0; e < S.nVis; e++, y += FCMP_LINHA_H) {
    const char *v = S.cel[c][S.vis[e]];
    TxtLinha lr = txt_linha_corta_bruta(T21_SUPPORT, i18n(FCMP_ROTULO[S.vis[e]]), T21_TEXT_R,
                                  T21_TEXT_G, T21_TEXT_B, 255, card.w - 2 * FCMP_PAD);
    gfx_cor((GfxRect){card.x + FCMP_PAD, y, card.w - 2 * FCMP_PAD, 1}, 0, 1, 1, 1, .06f * a);
    txt_desenhar_alpha(lr, card.x + FCMP_PAD, y + 6, a * T21_TEXT_SUPPORT);
    if (v[0]) {
      TxtLinha lv = txt_linha_corta_bruta(T21_META, v, T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255,
                                    card.w - 2 * FCMP_PAD);
      txt_desenhar_alpha(lv, card.x + FCMP_PAD, y + 6 + (float)lr.h + 2, a);
    }
  }
  if (S.aberto && a > .5f)
    ponteiro_alvo(card.x, card.y, card.w, card.h, fcmp_focar_col, fcmp_ativar_col, c, 0);
}

void farolcompara_desenhar(Uint32 agora) {
  float x, w, a, colW, total, x0, y, cardH;
  int c;
  (void)agora;
  if (!S.aberto && S.entrada < .005f) return;
  if (S.n < 2) return;
  a = anim_suave(S.entrada);
  TXT_INI();
  t21_inicio(&x, &w);
  gfx_cor((GfxRect){0, 0, NV_TELA_W, NV_TELA_H}, 0, .035f, .037f, .043f, .94f * a);
  if (S.aberto && a > .5f) {
    ponteiro_camada();
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, NULL, 0, 0);
  }
  y = T21_SAFE_T;
  fcmp_ponto(x, y + 5, a);
  plrui_kicker(i18n(FT_NOME), x + 22, y, T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, .55f * a);
  y += 40;
  {
    TxtLinha l = txt_linha_bruta(T21_TITLE, i18n(FT_CMP_TITULO), T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255);
    txt_desenhar_alpha(l, x, y, a);
    y += (float)l.h + 12;
  }
  if (S.veredito[0]) {
    TxtLinha l = txt_linha_corta_bruta(T21_BODY, S.veredito, T21_TEXT_R, T21_TEXT_G, T21_TEXT_B,
                                 255, w);
    txt_desenhar_alpha(l, x, y, a * T21_TEXT_SECONDARY);
  }
  colW = fminf(560.0f, (w - (S.n - 1) * FCMP_GAP) / S.n);
  total = S.n * colW + (S.n - 1) * FCMP_GAP;
  x0 = x + (w - total) / 2;
  cardH = FCMP_PAD + FCMP_POSTER_H + FCMP_PAD + S.nVis * FCMP_LINHA_H + (S.nVis ? 8 : 0);
  for (c = 0; c < S.n; c++)
    fcmp_coluna(c, (GfxRect){x0 + c * (colW + FCMP_GAP), FCMP_TOPO_COL, colW, cardH}, a);
  if (farolcompara_botao_visivel() || S.consultando) {
    const char *rot = i18n(FT_CMP_VER_QUALIDADE);
    float bw = (float)txt_largura(T21_CHIP, rot) + 2 * 40;
    GfxRect b = {x0, FCMP_TOPO_COL + cardH + 32, fmaxf(bw, 280), T21_CTRL_H};
    botao_pilula(b, rot, NULL, S.animBotao, 0, 0, a * (S.consultando ? .6f : 1.0f));
    if (S.aberto && a > .5f)
      ponteiro_alvo(b.x, b.y, b.w, b.h, fcmp_focar_botao, fcmp_ativar_botao, 0, 0);
  }
  TXT_FIM();
}

// ------------------------------------------------------------ casca e inspecao
int farolcompara_aberto(void) { return S.aberto; }

static int fcmp_pedido(int tipo, int *catIndice, char *ref, size_t cap) {
  if (S.pedido != tipo) return 0;
  if (catIndice) *catIndice = S.pedCat;
  if (ref && cap) fcmp_copia(ref, cap, S.pedRef);
  S.pedido = FCMP_PED_NADA;
  return 1;
}
int farolcompara_pediu_abrir(int *catIndice, char *ref, size_t cap) {
  return fcmp_pedido(FCMP_PED_ABRIR, catIndice, ref, cap);
}
int farolcompara_pediu_tocar(int *catIndice, char *ref, size_t cap) {
  return fcmp_pedido(FCMP_PED_TOCAR, catIndice, ref, cap);
}

int farolcompara_n_colunas(void) { return S.aberto ? S.n : 0; }
int farolcompara_foco(void) { return S.foco; }
int farolcompara_foco_botao(void) { return S.focoBotao; }
int farolcompara_escolha(void) { return S.escolha; }
int farolcompara_veredito(char *dst, size_t cap) {
  if (dst && cap) fcmp_copia(dst, cap, S.veredito);
  return S.veredito[0] != 0;
}
int farolcompara_n_eixos(void) { return S.nVis; }
int farolcompara_eixo(int i) { return i >= 0 && i < S.nVis ? S.vis[i] : -1; }
int farolcompara_tem_eixo(int eixo) {
  int i;
  for (i = 0; i < S.nVis; i++) if (S.vis[i] == eixo) return 1;
  return 0;
}
int farolcompara_celula(int col, int eixo, char *dst, size_t cap) {
  if (col < 0 || col >= S.n || eixo < 0 || eixo >= FCMP_EIXOS) { if (dst && cap) dst[0] = 0; return 0; }
  if (dst && cap) fcmp_copia(dst, cap, S.cel[col][eixo]);
  return S.cel[col][eixo][0] != 0;
}
int farolcompara_consultando(void) { return S.consultando; }
int farolcompara_qualidade_consultada(int col) {
  return col >= 0 && col < S.n && S.it[col].nFontes >= 0;
}

void farolcompara_teste_consulta(FarolCompConsulta fn) { fcFnConsulta = fn; }
void farolcompara_teste_caps(int hdr, int dv) { fcCapsHdr = hdr; fcCapsDv = dv; }
void farolcompara_teste_relogio(long long (*epochS)(void)) { fcFnEpoch = epochS; }
void farolcompara_teste_player(int (*aberto)(void)) { fcFnPlayer = aberto; }
void farolcompara_teste_sessao(uint64_t (*sessao)(void)) { fcFnSessao = sessao; }
int farolcompara_teste_fios(void) { return atomic_load(&fcVivos); }
int farolcompara_teste_esperar(Uint32 ms) {
  Uint32 t0 = SDL_GetTicks();
  while (atomic_load(&fcVivos) > 0) {
    if (SDL_GetTicks() - t0 >= ms) return 0;
    SDL_Delay(1);
  }
  return 1;
}
void farolcompara_teste_reset(void) {
  fcmp_estado_limpo();
  S.pedido = FCMP_PED_NADA;
  S.entrada = 0;
  memset(fcMem, 0, sizeof fcMem);
  fcMemN = fcMemProx = 0;
  fcMemSessaoOk = 0;
}
