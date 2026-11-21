// Modo SO INDICE do legref (.tpk 6+): a referencia sai do Cues do MKV, sem
// ler um bloco de legenda. Partes puras (sem fixtures, sem ffmpeg):
//   1. linha do tempo por tipo (SRT sem CueDuration, ASS, PGS);
//   2. legref_cues_tempos aceita ponto sem posicao de bloco (legref_cues nao);
//   3. a engine casa uma referencia com fim ESTIMADO (LEGENDA_DOC_FIM_ESTIMADO).
//   bash tests/legref_indice.sh        SANITIZE=1
#include "autosync.h"
#include "legref.h"
#include "rede.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *rede_baixar_bin(const char *url, int segundos, long *n) { (void)url; (void)segundos; (void)n; return NULL; }
unsigned rede_pedido_capacidades(void) { return REDE_CAP_JOB; }
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; memset(r, 0, sizeof *r); r->erro = REDE_INDISPONIVEL; return 0; }
void rede_resposta_limpar(RedeResposta *r) { free(r->corpo); free(r->cabecalhos); r->corpo = r->cabecalhos = NULL; }
const char *i18n(const char *s) { return s; }

static int casos;
#define PERTO(a, b) (fabs((a) - (b)) < 1e-6)

// --- 1. linhas do tempo ------------------------------------------------------
static void srtSemDuracao(void) {
  // Fora de ordem e com um inicio repetido: sai ordenado e sem a repeticao.
  LegRefPonto v[] = { { 0, 0, 20.0, 0 }, { 0, 0, 10.0, 0 }, { 0, 0, 12.0, 0 }, { 0, 0, 21.0, 0 }, { 0, 0, 12.0, 0 } };
  LegRefFala *f = NULL; int est = 0, filt = 0;
  int n = legref_indice_linha(v, 5, LEGREF_IDX_TEXTO, 0, &f, &est, &filt);
  assert(n == 4 && est == 1 && filt == 0);
  assert(PERTO(f[0].inicio, 10) && PERTO(f[0].fim, 12));     // proximo inicio
  assert(PERTO(f[1].inicio, 12) && PERTO(f[1].fim, 16));     // 8 s ate o proximo: teto de 4 s
  assert(PERTO(f[2].inicio, 20) && PERTO(f[2].fim, 21));
  assert(PERTO(f[3].inicio, 21) && PERTO(f[3].fim, 23));     // a ultima: 2 s
  free(f); casos++;
  // Com CueDuration: os tempos do arquivo, nada estimado.
  LegRefPonto w[] = { { 0, 0, 10.0, 1.5 }, { 0, 0, 12.0, 2.25 } };
  n = legref_indice_linha(w, 2, LEGREF_IDX_TEXTO, 0, &f, &est, &filt);
  assert(n == 2 && est == 0 && PERTO(f[0].fim, 11.5) && PERTO(f[1].fim, 14.25));
  free(f); casos++;
}

static void assFiltro(void) {
  // 40 falas normais, uma rajada de karaoke (10 inicios em 1 s) e uma placa
  // de 30 s com CueDuration: o filtro deixa so as 40.
  LegRefPonto v[64]; int k = 0;
  for (int i = 0; i < 40; i++) v[k++] = (LegRefPonto){ 0, 0, 100.0 + i * 4.0, 2.0 };
  for (int i = 0; i < 10; i++) v[k++] = (LegRefPonto){ 0, 0, 30.0 + i * 0.1, 0.2 };
  v[k++] = (LegRefPonto){ 0, 0, 60.0, 30.0 };
  LegRefFala *f = NULL; int est = 0, filt = 0;
  int n = legref_indice_linha(v, k, LEGREF_IDX_ASS, 0, &f, &est, &filt);
  assert(n == 40 && filt == 11 && est == 0);
  for (int i = 0; i < n; i++) assert(f[i].inicio >= 100.0);
  free(f); casos++;
  // Arquivo atipico (quase tudo "fora do dialogo"): o filtro nao corta, como no NuvioTV.
  k = 0;
  for (int i = 0; i < 30; i++) v[k++] = (LegRefPonto){ 0, 0, 10.0 + i * 0.15, 0.1 };
  n = legref_indice_linha(v, k, LEGREF_IDX_ASS, 0, &f, &est, &filt);
  assert(n == 30 && filt == 0);
  free(f); casos++;
}

static void pgsPares(void) {
  // 50 falas: exibe em t, limpa em t+2. O indice tem os 100 eventos.
  LegRefPonto v[100]; LegRefFala *f = NULL; int est = 0, filt = 0, n;
  for (int i = 0; i < 50; i++) {
    v[2 * i] = (LegRefPonto){ 0, 0, 10.0 + i * 5.0, 0 };
    v[2 * i + 1] = (LegRefPonto){ 0, 0, 12.0 + i * 5.0, 0 };
  }
  // Faixa de texto do mesmo arquivo com ~50 falas: par confirmado.
  n = legref_indice_linha(v, 100, LEGREF_IDX_PGS, 52, &f, &est, &filt);
  assert(n == 50 && est == 0);
  for (int i = 0; i < 50; i++) assert(PERTO(f[i].inicio, 10.0 + i * 5) && PERTO(f[i].fim, 12.0 + i * 5));
  free(f); casos++;
  // Sem faixa de texto e sem CueDuration: tambem pareia.
  n = legref_indice_linha(v, 100, LEGREF_IDX_PGS, 0, &f, &est, &filt);
  assert(n == 50); free(f); casos++;
  // A faixa de texto diz ~100 falas: os 100 eventos SAO falas (nao pareia).
  n = legref_indice_linha(v, 100, LEGREF_IDX_PGS, 100, &f, &est, &filt);
  assert(n == 100 && est == 1); free(f); casos++;
  // CueDuration na maioria e sem faixa de texto: o indice ja e de falas.
  for (int i = 0; i < 100; i++) v[i].dur = 1.0;
  n = legref_indice_linha(v, 100, LEGREF_IDX_PGS, 0, &f, &est, &filt);
  assert(n == 100 && est == 0); free(f); casos++;
}

// --- 2. Cues sem posicao de bloco --------------------------------------------
static int elem(unsigned char *o, unsigned long id, const unsigned char *d, int n) {
  int k = 0;
  if (id > 0xFFFFFF) o[k++] = (unsigned char)(id >> 24);
  if (id > 0xFFFF) o[k++] = (unsigned char)(id >> 16);
  if (id > 0xFF) o[k++] = (unsigned char)(id >> 8);
  o[k++] = (unsigned char)id;
  o[k++] = (unsigned char)(0x40 | (n >> 8)); o[k++] = (unsigned char)n;   // tamanho em 2 bytes
  memcpy(o + k, d, (size_t)n);
  return k + n;
}
static int uint1(unsigned char *o, unsigned long id, unsigned v) {
  unsigned char d[2] = { (unsigned char)(v >> 8), (unsigned char)v };
  return elem(o, id, d, 2);
}
static void cuesSemPosicao(void) {
  unsigned char cues[4096]; int n = 0;
  for (int i = 0; i < 10; i++) {
    unsigned char tp[64], cp[128]; int a = 0, b = 0;
    a += uint1(tp + a, 0xF7, 3);                 // CueTrack 3
    a += uint1(tp + a, 0xF1, 1000 + i);          // CueClusterPosition, SEM CueRelativePosition
    if (i % 2) a += uint1(tp + a, 0xB2, 1500);   // CueDuration (1 ms de escala)
    b += uint1(cp + b, 0xB3, 10000 + i * 3000);  // CueTime
    b += elem(cp + b, 0xB7, tp, a);
    n += elem(cues + n, 0xBB, cp, b);
  }
  LegRefPonto *p = NULL; int semRel = 0;
  int k = legref_cues(cues, n, 3, 1e-3, &p, &semRel);
  assert(k == 0 && semRel == 10); free(p); casos++;   // o modo de sempre nao serve
  k = legref_cues_tempos(cues, n, 3, 1e-3, 65536, &p);
  assert(k == 10);
  for (int i = 0; i < 10; i++) {
    assert(PERTO(p[i].inicio, 10.0 + i * 3));
    assert(i % 2 ? PERTO(p[i].dur, 1.5) : p[i].dur <= 0);
  }
  free(p); casos++;
  k = legref_cues_tempos(cues, n, 4, 1e-3, 65536, &p); assert(k == 0); free(p); casos++;
}

// --- 3. a engine com fim estimado --------------------------------------------
static LegendaDocumento *doc(const double *ini, const double *fim, int n, double atraso, int falas, unsigned flags) {
  LegendaCue *c = calloc((size_t)n, sizeof *c); LegendaDocumento *d;
  LegendaDocumentoInfo info; memset(&info, 0, sizeof info);
  info.sessao = 9; info.flags = LEGENDA_DOC_COMPLETO | flags;
  snprintf(info.identidade, sizeof info.identidade, "%s", falas ? "addon:x" : "mkv-track:3");
  for (int i = 0; i < n; i++) {
    c[i].inicio = ini[i] + atraso; c[i].fim = fim[i] + atraso; c[i].cor = -1; c[i].posX = c[i].posY = -1; c[i].ordem = i;
    if (falas) snprintf(c[i].texto, sizeof c[i].texto, "External line %d about something %d", i, i * 37 % 101);
    else snprintf(c[i].texto, sizeof c[i].texto, "fala %04d", i + 1);
  }
  d = legenda_documento_de_cues(c, n, &info); free(c);
  return d;
}
static void engineEstimado(void) {
  enum { N = 400 };
  double ini[N], fim[N], t = 30; unsigned s = 12345;
  LegRefPonto pts[N]; LegRefFala *f = NULL; int est = 0, filt = 0;
  for (int i = 0; i < N; i++) {
    s = s * 1664525u + 1013904223u; ini[i] = t; fim[i] = t + 1.0 + (s % 2500) / 1000.0;
    s = s * 1664525u + 1013904223u; t = fim[i] + 0.3 + (s % 5000) / 1000.0;
    pts[i] = (LegRefPonto){ 0, 0, ini[i], 0 };   // indice sem CueDuration
  }
  int n = legref_indice_linha(pts, N, LEGREF_IDX_TEXTO, 0, &f, &est, &filt);
  assert(n == N && est == 1);
  double ri[N], rf[N];
  for (int i = 0; i < n; i++) { ri[i] = f[i].inicio; rf[i] = f[i].fim; }
  free(f);
  LegendaDocumento *ref = doc(ri, rf, n, 0, 0, LEGENDA_DOC_FIM_ESTIMADO);
  LegendaDocumento *ext = doc(ini, fim, N, 2.5, 1, 0);
  AutoSyncConfig cfg = autosync_config(AUTOSYNC_QUICK);
  AutoSyncResultado r = autosync_alinhar(ext, ref, &cfg, NULL, NULL, NULL);
  printf("fim estimado: estado=%d motivo=%s offset=%d ms\n", r.estado, autosync_motivo(r.motivo), r.offsetMs);
  assert(r.estado == AUTOSYNC_ACCEPTED && abs(abs(r.offsetMs) - 2500) <= 60);
  casos++;
  // Uma externa de OUTRO filme nao passa so porque a referencia e frouxa no fim.
  double oi[N], of[N]; t = 40; s = 777;
  for (int i = 0; i < N; i++) {
    s = s * 1664525u + 1013904223u; oi[i] = t; of[i] = t + 1.0 + (s % 2500) / 1000.0;
    s = s * 1664525u + 1013904223u; t = of[i] + 0.3 + (s % 5000) / 1000.0;
  }
  LegendaDocumento *outro = doc(oi, of, N, 0, 1, 0);
  r = autosync_alinhar(outro, ref, &cfg, NULL, NULL, NULL);
  printf("fim estimado, outro filme: estado=%d motivo=%s\n", r.estado, autosync_motivo(r.motivo));
  assert(r.estado != AUTOSYNC_ACCEPTED);
  casos++;
  legenda_documento_liberar(ref); legenda_documento_liberar(ext); legenda_documento_liberar(outro);
}

int main(void) {
  srtSemDuracao();
  assFiltro();
  pgsPares();
  cuesSemPosicao();
  engineEstimado();
  // O modo so liga no .tpk 6+ (este binario e do host: desligado por padrao).
  assert(!legref_modo_indice_ativo());
  legref_modo_indice(1); assert(legref_modo_indice_ativo()); legref_modo_indice(0);
  casos++;
  printf("legref_indice: %d casos ok\n", casos);
  return 0;
}
