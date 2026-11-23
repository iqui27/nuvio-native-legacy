// Testes da issue farol-46 — bloco P3 ("Por que voce vai gostar") + P6 ("info")
// na pagina do titulo (PLANO-GOSTO secoes 4.2-4.5).
//
// Dois modos no mesmo arquivo:
//   - padrao (sem FD_MODO_GL): exercita faroldetalhe + farolcena + farolrec +
//     farolgosto + farolinfo + farolpeca + faroltexto + farolficha + idioma +
//     linguas sob normal, ASan+UBSan e clang TSan (incluindo concorrencia).
//   - -DFD_MODO_GL: inclui src/detail.c real em layout de TV 2.1 (#undef
//     __APPLE__), abre janela GLES2 1920x1080, despacha SDL_Event reais no
//     D-pad, prova identidade pixel a pixel quando o bloco esta ausente e
//     grava PNGs com e sem o bloco.
#undef __APPLE__
#include <SDL2/SDL.h>
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#ifndef FD_MODO_GL
// ============================================================================
// MODO 1: Nucleo deterministico + concorrencia TSan
// ============================================================================
#include "faroldetalhe.h"
#include "farolcena.h"
#include "faroltexto.h"
#include "farolsinal.h"
#include "ajustes.h"
#include "amigostitulo.h"
#include "anim.h"
#include "badges.h"
#include "catalogo.h"
#include "diario.h"
#include "extras.h"
#include "fonteantecipa.h"
#include "gfx.h"
#include "idioma.h"
#include "idiomacod.h"
#include "linguas.h"
#include "mapa.h"
#include "ondever.h"
#include "perfilinfantil.h"
#include "perfis.h"
#include "plrui.h"
#include "reacao.h"
#include "salvos.h"
#include "seguro.h"
#include "streams.h"
#include "text.h"

static _Atomic long long g_epoch = 1775077200LL; // 2026-04-01 21:00 UTC
static _Atomic uint32_t  g_ms    = 1000u;
static _Atomic int       g_idioma = 0;
static _Atomic int       g_reduzida = 0;
static _Atomic int       g_recomendar = 1;
static _Atomic int       g_seguro = 0;
static _Atomic int       g_infantil = 0;
static _Atomic int       g_h12 = 0;

static _Atomic int g_drawTotal = 0;
static _Atomic int g_drawCards = 0;
static _Atomic int g_drawDots  = 0;
static _Atomic int g_drawKick  = 0;
static _Atomic int g_drawPill  = 0;
static _Atomic int g_drawFoco  = 0;
static _Atomic int g_drawPtr   = 0;

static pthread_mutex_t g_catMu = PTHREAD_MUTEX_INITIALIZER;
static CatItem g_itens[4];
static int semente_teste;             // R3: semente do Mapa com slot obsoleto
static int leituras_indice_obsoleto;  // leituras via catIndice guardado
static unsigned mapa_rev_stub = 1;
static _Atomic int g_diaLido = 1;     // R1: 0 = diario frio (arquivo nao lido)
static _Atomic int g_diaGer = 1;      // geracao do diario publicada
static int leituras_diario;           // consultas a diario_titulo/ultimo_ep
static _Atomic int g_catRev = 1;      // revisao do catalogo
static int g_nItens = 0;

static long long relogioS(void)  { return atomic_load(&g_epoch); }
static Uint32    relogioMs(void) { return atomic_load(&g_ms); }

static void zerarContadoresDesenho(void) {
  atomic_store(&g_drawTotal, 0);
  atomic_store(&g_drawCards, 0);
  atomic_store(&g_drawDots, 0);
  atomic_store(&g_drawKick, 0);
  atomic_store(&g_drawPill, 0);
  atomic_store(&g_drawFoco, 0);
  atomic_store(&g_drawPtr, 0);
}

static void preencherFicha(FarolFicha *f, int32_t tmdb, uint8_t tipo,
                           uint16_t ano, uint16_t min, uint8_t nota,
                           int32_t p0, int32_t k0, int32_t g0) {
  memset(f, 0, sizeof *f);
  f->tmdb = tmdb;
  f->tipo = tipo;
  f->ano = ano;
  f->minutos = min;
  f->nota = nota;
  f->votos = 500;
  f->fonte = FF_DE_TMDB;
  if (p0) { f->pessoa[0] = p0; f->nPessoa = 1; }
  if (k0) { f->tema[0] = k0; f->nTema = 1; }
  if (g0) { f->genero[0] = g0; f->nGenero = 1; }
}

static void aquecerModelo(FarolGosto *g, uint16_t dia,
                          int32_t pessoaId, int32_t temaId, int32_t genId) {
  FarolGostoFicha fgf;
  int k;
  farolgosto_iniciar(g, 1);
  memset(&fgf, 0, sizeof fgf);
  fgf.tipo = FG_TIPO_FILME;
  fgf.ano = 2024;
  fgf.minutos = 115;
  fgf.pessoa[0] = pessoaId;
  fgf.tema[0] = temaId;
  fgf.genero[0] = genId;
  for (k = 0; k < 10; k++) {
    fgf.tmdb = 500 + k;
    farolgosto_aplicar(g, &fgf, FG_D_GOSTOU, dia, 2, 1);
  }
}

// --- stubs deterministas dos subsistemas externos do app --------------------
int ajustes_idioma(void)              { return atomic_load(&g_idioma); }
int ajustes_animacoes_reduzidas(void) { return atomic_load(&g_reduzida); }
int ajustes_relogio_12h(void)         { return atomic_load(&g_h12); }
void ajustes_acento(float *r, float *g, float *b) {
  if (r) *r = 0.26f;
  if (g) *g = 0.65f;
  if (b) *b = 0.96f;
}
int farolsinal_recomendar(void) { return atomic_load(&g_recomendar); }
int farolsinal_modelo(FarolGosto *dst) {
  if (dst) farolgosto_iniciar(dst, 1);
  return 0;
}
int seguro_perfil_ativo(void)   { return atomic_load(&g_seguro); }
int perfil_infantil_ativo(void) { return atomic_load(&g_infantil); }
int perfis_ativo(void)          { return 1; }

const CatItem *cat_item(int idx) {
  const CatItem *r = NULL;
  pthread_mutex_lock(&g_catMu);
  if (idx >= 0 && idx < g_nItens) r = &g_itens[idx];
  pthread_mutex_unlock(&g_catMu);
  // R3: conta leituras feitas pelo catIndice guardado do retrato do Mapa —
  // a republicacao troca o dono do slot e o Farol nao pode ler o errado.
  if (semente_teste && idx == 0 && r && strcmp(r->imdb, "tt9000001"))
    leituras_indice_obsoleto++;
  return r;
}
unsigned cat_revisao(void) { return (unsigned)atomic_load(&g_catRev); }
int cat_indice_por_imdb(const char *id) {
  int i, r = -1;
  if (!id || !id[0]) return -1;
  pthread_mutex_lock(&g_catMu);
  for (i = 0; i < g_nItens; i++)
    if (!strcmp(g_itens[i].imdb, id)) { r = i; break; }
  pthread_mutex_unlock(&g_catMu);
  return r;
}

int mapa_vizinhos_copiar(MapaVizinhos *dst, unsigned *revisao) {
  (void)revisao;
  if (dst) memset(dst, 0, sizeof *dst);
  return 0;
}
int mapa_copiar(Mapa *dst, unsigned *revisao) {
  if (!dst || !revisao || *revisao == mapa_rev_stub) return 0;
  memset(dst, 0, sizeof *dst);
  if (semente_teste) {
    dst->nSem = 1;
    snprintf(dst->sem[0].imdb, sizeof dst->sem[0].imdb, "tt9000001");
    snprintf(dst->sem[0].titulo, sizeof dst->sem[0].titulo, "Semente vista");
    dst->sem[0].catIndice = 0;   // slot do retrato: fica obsoleto na republicacao
    dst->semOrigem[0] = MAPA_ORIGEM_VISTO;
  }
  *revisao = mapa_rev_stub;
  return 1;
}
const char *mapa_tema_nome(const char *kw) {
  if (!kw) return NULL;
  if (!strcmp(kw, "time travel")) return "viagem no tempo";
  if (!strcmp(kw, "dystopia")) return "distopia";
  return NULL;
}
unsigned mapa_hash_titulo(const char *t) {
  unsigned h = 2166136261u;
  if (!t) return 0;
  for (; *t; t++) {
    unsigned char c = (unsigned char)*t;
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c >= 0x80) {
      h ^= c;
      h *= 16777619u;
    }
  }
  return h;
}
long mapa_hash_nome(const char *s) {
  return -(long)(mapa_hash_titulo(s) & 0x3fffffff) - 1;
}
void mapa_generos_locais(const char *g, MapaEtiqueta *gen, int *nGen, int max) {
  char buf[160], *p, *tok;
  int primeiro = 1;
  if (!gen || !nGen) return;
  snprintf(buf, sizeof buf, "%s", g ? g : "");
  p = buf;
  *nGen = 0;
  while (p && *p && *nGen < max) {
    char *sep = strstr(p, "\xc2\xb7");
    if (sep) *sep = 0;
    tok = p;
    while (*tok == ' ') tok++;
    { size_t k = strlen(tok); while (k && tok[k - 1] == ' ') tok[--k] = 0; }
    if (tok[0] && !primeiro) {
      gen[*nGen].id = mapa_hash_nome(tok);
      snprintf(gen[*nGen].nome, sizeof gen[0].nome, "%s", tok);
      (*nGen)++;
    }
    primeiro = 0;
    p = sep ? sep + 2 : NULL;
  }
}
int mapa_ano_meta(const char *meta) {
  const char *p = meta;
  if (!p) return 0;
  for (; *p; p++) {
    if (p[0] >= '1' && p[0] <= '2' &&
        p[1] >= '0' && p[1] <= '9' &&
        p[2] >= '0' && p[2] <= '9' &&
        p[3] >= '0' && p[3] <= '9') {
      int a = (p[0] - '0') * 1000 + (p[1] - '0') * 100 +
              (p[2] - '0') * 10 + (p[3] - '0');
      if (a >= 1900 && a <= 2100) return a;
    }
  }
  return 0;
}
int mapa_ler_ids(const char *json, int serie, MapaDetalhe *d) {
  (void)json; (void)serie;
  if (d) memset(d, 0, sizeof *d);
  return 0;
}

int amigostitulo_obter(const char *imdb, AmigosTitulo *saida) {
  (void)imdb;
  if (saida) memset(saida, 0, sizeof *saida);
  return 0;
}
void amigostitulo_primeiro_nome(const char *nome, char *dst, size_t tam) {
  size_t i = 0;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!nome) return;
  while (nome[i] && nome[i] != ' ' && i + 1 < tam) { dst[i] = nome[i]; i++; }
  dst[i] = 0;
}

int diario_nome(const char *id, char *tit, size_t capT, int *ano, char *p, size_t capP) {
  (void)id; (void)ano; (void)p; (void)capP;
  if (tit && capT) tit[0] = 0;
  return 0;
}
int diario_carregado(int perfil)  { (void)perfil; return atomic_load(&g_diaLido); }
unsigned diario_geracao(int perfil) { (void)perfil; return (unsigned)atomic_load(&g_diaGer); }
int diario_nomes_lido(void)       { return atomic_load(&g_diaLido); }
int diario_titulo(int perfil, const char *id, DiarioTitulo *saida) {
  (void)perfil; (void)id;
  leituras_diario++;
  if (saida) memset(saida, 0, sizeof *saida);
  return 0;
}
int diario_ultimo_episodio(int perfil, const char *id, int *t, int *e, long long *q) {
  (void)perfil; (void)id;
  leituras_diario++;
  if (t) *t = 0;
  if (e) *e = 0;
  if (q) *q = 0;
  return 0;
}
int salvos_tem(const char *id)    { (void)id; return 0; }
int reacao_nota(const char *id)   { (void)id; return 0; }
int reacao_notas_lida(void)       { return 1; }
int ondever_n(const char *id)     { (void)id; return 0; }
int ondever_item(const char *id, int i, OndeVer *dst) {
  (void)id; (void)i;
  if (dst) memset(dst, 0, sizeof *dst);
  return 0;
}
int extras_n_temporadas(void)        { return 0; }
int extras_temporada_numero(int i)   { (void)i; return 0; }
int extras_n_eps(int t)              { (void)t; return 0; }
int extras_ep_nota(int t, int e)     { (void)t; (void)e; return 0; }
int extras_progresso_serie(int *v, int *tot) { if (v) *v = 0; if (tot) *tot = 0; return 0; }
int extras_n_colecao(void)           { return 0; }
long extras_colecao_tmdb(int i)      { (void)i; return 0; }

void stream_capacidades(int *hdr, int *dv, int *hevc, int *av1) {
  if (hdr) *hdr = 1;
  if (dv) *dv = 1;
  if (hevc) *hevc = 1;
  if (av1) *av1 = 1;
}
int stream_lista_do_alvo(const char *id) { (void)id; return 0; }
int stream_n(void)                       { return 0; }
const Stream *stream_item(int i)         { (void)i; return NULL; }
int stream_fonte_estado(int i)           { (void)i; return FA_NADA; }
int stream_incompativel(int i)           { (void)i; return 0; }
int stream_fonte_disponivel(const Stream *s) { return s != NULL; }
const char *stream_idioma(const Stream *s)   { (void)s; return NULL; }
uint64_t badges_bit(const char *nome)        { (void)nome; return 0; }

void plrui_material(GfxRect r, float raio, int modal, float a) {
  (void)r; (void)raio; (void)modal;
  if (a > 0.002f) { atomic_fetch_add(&g_drawTotal, 1); atomic_fetch_add(&g_drawCards, 1); }
}
float plrui_kicker(const char *s, float x, float y, int r, int g, int b, float a) {
  (void)x; (void)y; (void)r; (void)g; (void)b;
  if (a > 0.002f) { atomic_fetch_add(&g_drawTotal, 1); atomic_fetch_add(&g_drawKick, 1); }
  return s ? (float)strlen(i18n(s)) * 8.0f : 0.0f;
}
void plrui_botao_repouso(GfxRect r, float a) {
  (void)r;
  if (a > 0.002f) { atomic_fetch_add(&g_drawTotal, 1); atomic_fetch_add(&g_drawPill, 1); }
}
void plrui_pilula_foco(GfxRect r, float a) {
  (void)r;
  if (a > 0.002f) { atomic_fetch_add(&g_drawTotal, 1); atomic_fetch_add(&g_drawFoco, 1); }
}
int plrui_tinta(void) { return 16; }

void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float a) {
  (void)cr; (void)cg; (void)cb;
  if (a > 0.002f) {
    atomic_fetch_add(&g_drawTotal, 1);
    if (fabsf(r.w - 10.0f) < 0.01f && fabsf(r.h - 10.0f) < 0.01f && fabsf(raio - 0.5f) < 0.01f)
      atomic_fetch_add(&g_drawDots, 1);
  }
}

int txt_largura(TxtEstilo e, const char *s) {
  (void)e;
  const char *tr = s ? i18n(s) : "";
  return (int)strlen(tr) * 9;
}
TxtLinha txt_linha_corta(TxtEstilo e, const char *s, int r, int g, int b, int a, float maxW) {
  (void)e; (void)r; (void)g; (void)b; (void)a;
  TxtLinha tl;
  const char *tr = s ? i18n(s) : "";
  int w = (int)strlen(tr) * 9;
  memset(&tl, 0, sizeof tl);
  if ((float)w > maxW && maxW > 0.0f) w = (int)maxW;
  tl.w = w;
  tl.h = 22;
  return tl;
}
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) {
  (void)l; (void)x; (void)y;
  if (a > 0.002f) atomic_fetch_add(&g_drawTotal, 1);
}
void ponteiro_alvo(float x, float y, float w, float h,
                   PonteiroFn f, PonteiroFn ok, int r, int c) {
  (void)x; (void)y; (void)w; (void)h; (void)f; (void)ok; (void)r; (void)c;
  atomic_fetch_add(&g_drawPtr, 1);
}
static void ptrDummy(int r, int c) { (void)r; (void)c; }

static uint16_t diaAtual(void) {
  return (uint16_t)((atomic_load(&g_epoch) - 1767225600LL) / 86400LL);
}

static void montarCatalogoBase(void) {
  pthread_mutex_lock(&g_catMu);
  memset(g_itens, 0, sizeof g_itens);
  g_nItens = 3;
  // [0]: titulo inicialmente CRU (sem ficha: sem genero, sem elenco, sem meta)
  snprintf(g_itens[0].imdb, sizeof g_itens[0].imdb, "tt9000001");
  snprintf(g_itens[0].titulo, sizeof g_itens[0].titulo, "Horizonte Perdido");
  snprintf(g_itens[0].tipo, sizeof g_itens[0].tipo, "movie");
  g_itens[0].tmdb = 9001;

  // [1]: titulo com dados completos em CatItem
  snprintf(g_itens[1].imdb, sizeof g_itens[1].imdb, "tt9000002");
  snprintf(g_itens[1].titulo, sizeof g_itens[1].titulo, "Ecos do Amanhã");
  snprintf(g_itens[1].tipo, sizeof g_itens[1].tipo, "movie");
  snprintf(g_itens[1].genero, sizeof g_itens[1].genero, "Filme · Ficção científica · Suspense");
  snprintf(g_itens[1].meta, sizeof g_itens[1].meta, "2025 · 110 min");
  snprintf(g_itens[1].classificacao, sizeof g_itens[1].classificacao, "12");
  g_itens[1].tmdb = 9002;
  g_itens[1].nota = 82;
  g_itens[1].nElenco = 1;
  g_itens[1].elenco[0].tmdb = 7701;
  snprintf(g_itens[1].elenco[0].nome, sizeof g_itens[1].elenco[0].nome, "Rebecca Ferguson");
  snprintf(g_itens[1].elenco[0].foto, sizeof g_itens[1].elenco[0].foto, "/rebecca.jpg");

  // [2]: canal ao vivo
  snprintf(g_itens[2].imdb, sizeof g_itens[2].imdb, "cs:globo-sp");
  snprintf(g_itens[2].titulo, sizeof g_itens[2].titulo, "Canal Ao Vivo");
  snprintf(g_itens[2].tipo, sizeof g_itens[2].tipo, "channel");
  pthread_mutex_unlock(&g_catMu);
}

// 1. Nasce no exato quadro em que a ficha existe + um unico ponto de luz.
static void teste_nasce_no_quadro_da_ficha(void) {
  FarolGosto g;
  FarolFicha fAlvo, fSem;
  uint16_t d = diaAtual();
  int q;

  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(relogioS, relogioMs);
  montarCatalogoBase();
  aquecerModelo(&g, d, 7701, 4379, 878);
  faroldetalhe_teste_modelo(&g);
  faroldetalhe_teste_vocab(FP_SELO_PESSOA, 7701, "Rebecca Ferguson", "/rebecca.jpg", 0);
  faroldetalhe_teste_vocab(FP_SELO_TEMA, 4379, "time travel", NULL, -1);

  preencherFicha(&fSem, 1100, FF_SERIE, 2023, 50, 85, 7701, 4379, 878);
  faroldetalhe_teste_semente("tt7770001", "Silo", &fSem, 1);

  // Quadros 0..3 sem ficha: bloco NAO existe (altura == 0, zero desenho).
  atomic_store(&g_ms, 1000u);
  faroldetalhe_abrir(0, 1000u);
  for (q = 0; q < 4; q++) {
    Uint32 t = 1000u + (Uint32)(q * 16);
    atomic_store(&g_ms, t);
    faroldetalhe_atualizar(0, 0, 0, 0.016f, t);
    assert(faroldetalhe_altura(1040.0f) == 0.0f);
    assert(!faroldetalhe_tem_porque());
    assert(!faroldetalhe_tem_info());
    zerarContadoresDesenho();
    faroldetalhe_desenhar(96.0f, 700.0f, 1040.0f, 1.0f, ptrDummy, -5);
    assert(atomic_load(&g_drawTotal) == 0);
  }

  // Quadro 4: ficha chega -> bloco nasce NESTE EXATO QUADRO.
  preencherFicha(&fAlvo, 9001, FF_FILME, 2025, 112, 83, 7701, 4379, 878);
  faroldetalhe_teste_ficha("tt9000001", &fAlvo);

  atomic_store(&g_ms, 1064u);
  faroldetalhe_atualizar(0, 0, 0, 0.016f, 1064u);
  assert(faroldetalhe_altura(1040.0f) > 0.0f);
  assert(faroldetalhe_tem_porque() == 1);
  assert(faroldetalhe_tem_info() == 1);
  assert(faroldetalhe_n_selos() == 3);

  zerarContadoresDesenho();
  faroldetalhe_desenhar(96.0f, 700.0f, 1040.0f, 1.0f, ptrDummy, -5);
  assert(atomic_load(&g_drawCards) == 1);
  assert(atomic_load(&g_drawDots) == 1);    // um unico ponto de luz para P3+P6
  assert(atomic_load(&g_drawKick) == 1);
  assert(atomic_load(&g_drawPill) == 3);
  assert(atomic_load(&g_drawPtr) == 3);
}

// 2. Qualidade entra com fade ate 8 s, nao entra apos 8 s, e sem fade em reduzidas.
static void teste_qualidade_fade_e_prazo_8s(void) {
  FarolGosto g;
  FarolInfoFonte fonte;
  FarolInfoCaps caps = { 1, 1 };
  uint16_t d = diaAtual();
  char l0[192];
  int k;

  memset(&fonte, 0, sizeof fonte);
  fonte.altura = 2160;
  fonte.dv = 1;
  fonte.toca = 1;
  snprintf(fonte.audio, sizeof fonte.audio, "pt");

  // 2a) Chegada em 2,5 s (dentro dos 8 s), animacoes normais -> entra com fade.
  anim_politica_reduzida = 0;
  atomic_store(&g_reduzida, 0);
  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(relogioS, relogioMs);
  montarCatalogoBase();
  aquecerModelo(&g, d, 7701, 4379, 878);
  faroldetalhe_teste_modelo(&g);

  atomic_store(&g_ms, 1000u);
  faroldetalhe_abrir(1, 1000u);
  assert(!faroldetalhe_tem_qualidade());

  atomic_store(&g_ms, 3500u); // +2500 ms <= 8000 ms
  faroldetalhe_teste_fontes("tt9000002", &fonte, 1, &caps);
  faroldetalhe_atualizar(1, 0, 0, 0.016f, 3500u);
  assert(faroldetalhe_tem_qualidade() == 1);
  {
    float a0 = faroldetalhe_alpha_qualidade();
    assert(a0 > 0.0f && a0 < 0.35f);
    for (k = 0; k < 20; k++) {
      Uint32 t = 3516u + (Uint32)(k * 16);
      atomic_store(&g_ms, t);
      faroldetalhe_atualizar(1, 0, 0, 0.016f, t);
    }
    assert(fabsf(faroldetalhe_alpha_qualidade() - 1.0f) < 0.001f);
  }
  assert(faroldetalhe_linha_info(0, l0, sizeof l0));
  assert(strstr(l0, "4K") != NULL && strstr(l0, "Dolby Vision") != NULL);

  // 2b) Fronteira exata 8000 ms: ainda entra.
  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(relogioS, relogioMs);
  faroldetalhe_teste_modelo(&g);
  atomic_store(&g_ms, 1000u);
  faroldetalhe_abrir(1, 1000u);
  atomic_store(&g_ms, 9000u); // 9000 - 1000 == 8000 ms
  faroldetalhe_teste_fontes("tt9000002", &fonte, 1, &caps);
  faroldetalhe_atualizar(1, 0, 0, 0.016f, 9000u);
  assert(faroldetalhe_tem_qualidade() == 1);

  // 2c) Apos 8 s (8001 ms e 13000 ms): NAO entra!
  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(relogioS, relogioMs);
  faroldetalhe_teste_modelo(&g);
  atomic_store(&g_ms, 1000u);
  faroldetalhe_abrir(1, 1000u);
  atomic_store(&g_ms, 9001u); // 8001 ms > 8000 ms
  faroldetalhe_teste_fontes("tt9000002", &fonte, 1, &caps);
  faroldetalhe_atualizar(1, 0, 0, 0.016f, 9001u);
  assert(faroldetalhe_tem_qualidade() == 0);
  atomic_store(&g_ms, 13000u);
  faroldetalhe_atualizar(1, 0, 0, 0.016f, 13000u);
  assert(faroldetalhe_tem_qualidade() == 0);

  // 2d) Cena JSON externa (LLM) chegando apos 8 s tambem tem `qualidade` barrada.
  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(relogioS, relogioMs);
  atomic_store(&g_ms, 2000u);
  faroldetalhe_abrir(1, 2000u);
  atomic_store(&g_ms, 10005u); // +8005 ms
  assert(faroldetalhe_teste_json(
    "{\"v\":1,\"tela\":\"detalhe\",\"pecas\":[{\"t\":\"info\",\"alvo\":\"tt9000002\","
    "\"itens\":[{\"k\":\"qualidade\",\"l\":{\"m\":\"FT_INFO_QUALI_AUDIO\",\"a\":[\"4K\",\"pt\"]}}]}]}",
    0, 10005u) >= 0);
  assert(faroldetalhe_tem_qualidade() == 0);
  assert(faroldetalhe_altura(1040.0f) == 0.0f);

  // 2e) Animacoes reduzidas: entra direto em alpha 1.0f, sem fade.
  anim_politica_reduzida = 1;
  atomic_store(&g_reduzida, 1);
  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(relogioS, relogioMs);
  faroldetalhe_teste_modelo(&g);
  atomic_store(&g_ms, 1000u);
  faroldetalhe_abrir(1, 1000u);
  atomic_store(&g_ms, 3500u);
  faroldetalhe_teste_fontes("tt9000002", &fonte, 1, &caps);
  faroldetalhe_atualizar(1, 0, 0, 0.016f, 3500u);
  assert(faroldetalhe_tem_qualidade() == 1);
  assert(faroldetalhe_alpha_qualidade() == 1.0f);
  assert(faroldetalhe_alpha_bloco() == 1.0f);
  anim_politica_reduzida = 0;
  atomic_store(&g_reduzida, 0);
}

// 3. Ausencia total de motivo e informacao -> altura 0, zero chamadas de desenho.
static void teste_ausencia_bloco_zero(void) {
  FarolGosto gFrio;
  FarolFicha fSoGenero;

  farolgosto_iniciar(&gFrio, 1); // frio 0: sem P3
  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(relogioS, relogioMs);
  montarCatalogoBase();
  faroldetalhe_teste_modelo(&gFrio);

  preencherFicha(&fSoGenero, 9001, FF_FILME, 2024, 0, 75, 0, 0, 18);
  faroldetalhe_teste_ficha("tt9000001", &fSoGenero);

  atomic_store(&g_ms, 1000u);
  faroldetalhe_abrir(0, 1000u);
  faroldetalhe_atualizar(0, 0, 0, 0.016f, 1000u);
  assert(faroldetalhe_altura(1040.0f) == 0.0f);
  assert(faroldetalhe_n_selos() == 0);
  assert(faroldetalhe_focar(1) == 0);
  assert(faroldetalhe_focado() == 0);
  zerarContadoresDesenho();
  faroldetalhe_desenhar(96.0f, 700.0f, 1040.0f, 1.0f, ptrDummy, -5);
  assert(atomic_load(&g_drawTotal) == 0);

  // Recusado explicitamente ("Nao e para mim") -> altura 0.
  {
    FarolGosto gRec;
    aquecerModelo(&gRec, diaAtual(), 7701, 4379, 878);
    farolgosto_recusar(&gRec, mapa_hash_titulo("tt9000002"));
    faroldetalhe_teste_modelo(&gRec);
    faroldetalhe_abrir(1, 1000u);
    faroldetalhe_atualizar(1, 0, 0, 0.016f, 1000u);
    assert(faroldetalhe_altura(1040.0f) == 0.0f);
  }

  // Canal ao vivo tambem nunca monta bloco.
  faroldetalhe_abrir(2, 1000u);
  faroldetalhe_atualizar(2, 0, 0, 0.016f, 1000u);
  assert(faroldetalhe_altura(1040.0f) == 0.0f);

  // "Recomendar nesta TV" desligado ou modo seguro ativo -> altura 0.
  atomic_store(&g_recomendar, 0);
  faroldetalhe_abrir(1, 1000u);
  faroldetalhe_atualizar(1, 0, 0, 0.016f, 1000u);
  assert(faroldetalhe_altura(1040.0f) == 0.0f);
  atomic_store(&g_recomendar, 1);

  atomic_store(&g_seguro, 1);
  faroldetalhe_abrir(1, 1000u);
  faroldetalhe_atualizar(1, 0, 0, 0.016f, 1000u);
  assert(faroldetalhe_altura(1040.0f) == 0.0f);
  atomic_store(&g_seguro, 0);
}

// 4. Foco nos selos, ativacao (pessoa vs explorar) e traducao nos 29 idiomas.
static void teste_selos_e_idiomas(void) {
  FarolGosto g;
  FarolFicha fAlvo;
  uint16_t d = diaAtual();
  int menu = 0, lang;
  long tmdb = 0;
  int elCol = -1;
  char nome[96], foto[256], l0[192], s0[80];

  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(relogioS, relogioMs);
  montarCatalogoBase();
  aquecerModelo(&g, d, 7701, 4379, 878);
  faroldetalhe_teste_modelo(&g);
  faroldetalhe_teste_vocab(FP_SELO_TEMA, 4379, "time travel", NULL, -1);

  preencherFicha(&fAlvo, 9002, FF_FILME, 2025, 110, 82, 7701, 4379, 878);
  faroldetalhe_teste_ficha("tt9000002", &fAlvo);

  atomic_store(&g_ms, 1000u);
  faroldetalhe_abrir(1, 1000u);
  assert(faroldetalhe_n_selos() == 3);
  assert(faroldetalhe_focar(1) == 1);
  assert(faroldetalhe_focado() == 1);
  assert(faroldetalhe_selo_atual() == 0);

  assert(faroldetalhe_tecla(SDLK_LEFT, &menu) == 1 && menu == 1);
  assert(faroldetalhe_selo_atual() == 0);
  assert(faroldetalhe_tecla(SDLK_RIGHT, &menu) == 1 && faroldetalhe_selo_atual() == 1);
  assert(faroldetalhe_tecla(SDLK_RIGHT, &menu) == 1 && faroldetalhe_selo_atual() == 2);
  assert(faroldetalhe_tecla(SDLK_RIGHT, &menu) == 1 && faroldetalhe_selo_atual() == 2);
  assert(faroldetalhe_tecla(SDLK_UP, &menu) == 0);
  assert(faroldetalhe_tecla(SDLK_DOWN, &menu) == 0);

#ifdef FD_TEM_SELO_FIO
  {
    int sT = -1;
    int32_t sId = 0;
    char sF[64] = "", fioGenero[64] = "";
    // R7: selo devolve tipo + fio (+ id) para Explorar filtrado (destinos distintos).
    assert(faroldetalhe_ativar_id(&tmdb, &elCol, nome, sizeof nome, foto, sizeof foto,
                                  &sT, sF, sizeof sF, &sId) == FD_ACAO_EXPLORAR);
    assert(sT == FP_SELO_GENERO && sF[0] != 0 && sId == 878);
    assert(faroldetalhe_ativar(&tmdb, &elCol, nome, sizeof nome, foto, sizeof foto,
                               &sT, sF, sizeof sF) == FD_ACAO_EXPLORAR);
    assert(sT == FP_SELO_GENERO && sF[0] != 0);
    snprintf(fioGenero, sizeof fioGenero, "%s", sF);
    faroldetalhe_tecla(SDLK_LEFT, &menu);
    sT = -1; sF[0] = 0;
    assert(faroldetalhe_ativar(&tmdb, &elCol, nome, sizeof nome, foto, sizeof foto,
                               &sT, sF, sizeof sF) == FD_ACAO_EXPLORAR);
    assert(sT == FP_SELO_TEMA && !strcmp(sF, "time travel"));
    assert(strcmp(sF, fioGenero) != 0);
    faroldetalhe_tecla(SDLK_LEFT, &menu);
    sT = -1; sF[0] = 0;
    assert(faroldetalhe_ativar(&tmdb, &elCol, nome, sizeof nome, foto, sizeof foto,
                               &sT, sF, sizeof sF) == FD_ACAO_PESSOA);
    assert(tmdb == 7701 && elCol == 0 && !strcmp(nome, "Rebecca Ferguson"));
    assert(sT == -1 && sF[0] == 0);
  }
#else
  // Revisao base (assinatura antiga): o selo nao carrega destino — o
  // bloqueio 7 e estrutural nela (Explorar generico), sem assert possivel.
  assert(faroldetalhe_ativar(&tmdb, &elCol, nome, sizeof nome, foto, sizeof foto) == FD_ACAO_EXPLORAR);
  faroldetalhe_tecla(SDLK_LEFT, &menu);
  assert(faroldetalhe_ativar(&tmdb, &elCol, nome, sizeof nome, foto, sizeof foto) == FD_ACAO_EXPLORAR);
  faroldetalhe_tecla(SDLK_LEFT, &menu);
  assert(faroldetalhe_ativar(&tmdb, &elCol, nome, sizeof nome, foto, sizeof foto) == FD_ACAO_PESSOA);
  assert(tmdb == 7701 && elCol == 0 && !strcmp(nome, "Rebecca Ferguson"));
#endif

  for (lang = 0; lang < 29; lang++) {
    atomic_store(&g_idioma, lang);
    faroldetalhe_atualizar(1, 0, 0, 0.016f, 1016u + (Uint32)lang);
    assert(faroldetalhe_linha_porque(0, l0, sizeof l0) && l0[0] != 0);
    assert(strstr(l0, "p7701") == NULL && strstr(l0, "k4379") == NULL && strstr(l0, "g878") == NULL);
    assert(faroldetalhe_selo_rotulo(2, NULL, NULL, s0, sizeof s0) && s0[0] != 0);
    assert(strstr(s0, "g878") == NULL);
  }
  atomic_store(&g_idioma, 0);

  // Selo Animation (TMDB 16) em ingles e portugues preserva id 16 e rotulo localizado.
  {
    int sT = -1;
    int32_t sId = 0;
    char sF[64] = "";
    faroldetalhe_teste_reset();
    faroldetalhe_teste_relogio(relogioS, relogioMs);
    montarCatalogoBase();
    aquecerModelo(&g, d, 0, 0, 16);
    faroldetalhe_teste_modelo(&g);
    preencherFicha(&fAlvo, 9002, FF_FILME, 2025, 110, 82, 0, 0, 16);
    faroldetalhe_teste_ficha("tt9000002", &fAlvo);
    atomic_store(&g_idioma, IDIOMA_EN);
    faroldetalhe_abrir(1, 1000u);
    assert(faroldetalhe_n_selos() == 1);
    assert(faroldetalhe_selo_rotulo(0, &sT, &sId, s0, sizeof s0));
    assert(sT == FP_SELO_GENERO && sId == 16 && !strcmp(s0, "Animation"));
    assert(faroldetalhe_focar(1) == 1);
    sT = -1; sId = 0;
    assert(faroldetalhe_ativar_id(&tmdb, &elCol, nome, sizeof nome, foto, sizeof foto,
                                  &sT, sF, sizeof sF, &sId) == FD_ACAO_EXPLORAR);
    assert(sT == FP_SELO_GENERO && sId == 16 && !strcmp(sF, "Anima\xc3\xa7\xc3\xa3o"));
    atomic_store(&g_idioma, IDIOMA_PT);
    faroldetalhe_atualizar(1, 0, 0, 0.016f, 1050u);
    assert(faroldetalhe_selo_rotulo(0, &sT, &sId, s0, sizeof s0));
    assert(sT == FP_SELO_GENERO && sId == 16 && !strcmp(s0, "Anima\xc3\xa7\xc3\xa3o"));
  }
}

// FAROL46B R1: o fio da UI nunca dispara carga nem varredura do Diario.
// Frio -> nenhuma consulta sai; quente -> uma consulta por (imdb, geracao).
static void teste_diario_fora_do_fio_ui(void) {
  FarolGosto g;
  uint16_t d = diaAtual();
  int i;

  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(relogioS, relogioMs);
  montarCatalogoBase();
  aquecerModelo(&g, d, 7701, 4379, 878);
  faroldetalhe_teste_modelo(&g);

  atomic_store(&g_diaLido, 0);
  atomic_store(&g_diaGer, 5);
  leituras_diario = 0;
  atomic_store(&g_ms, 1000u);
  faroldetalhe_abrir(1, 1000u);
  for (i = 0; i < 30; i++)
    faroldetalhe_atualizar(1, 0, 0, 0.016f, 1016u + (Uint32)(i * 16));
  assert(leituras_diario == 0);

  atomic_store(&g_diaLido, 1);
  atomic_store(&g_diaGer, 7);
  faroldetalhe_atualizar(1, 0, 0, 0.016f, 2000u);
  assert(leituras_diario == 1);   // filme: so diario_titulo
  for (i = 0; i < 30; i++)
    faroldetalhe_atualizar(1, 0, 0, 0.016f, 2100u + (Uint32)(i * 16));
  assert(leituras_diario == 1);

  atomic_store(&g_diaGer, 8);     // geracao nova -> exatamente uma consulta
  faroldetalhe_atualizar(1, 0, 0, 0.016f, 3000u);
  assert(leituras_diario == 2);
  atomic_store(&g_diaGer, 1);
}

// FAROL46B R2: o prazo de 8 s e da abertura pela identidade (IMDb base),
// nao da posicao — a republicacao nao pode reabrir a janela.
static void teste_prazo_identidade_nao_posicao(void) {
  FarolGosto g;
  FarolInfoFonte fonte;
  FarolInfoCaps caps = { 1, 1 };
  uint16_t d = diaAtual();

  memset(&fonte, 0, sizeof fonte);
  fonte.altura = 2160;
  fonte.dv = 1;
  fonte.toca = 1;
  snprintf(fonte.audio, sizeof fonte.audio, "pt");

  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(relogioS, relogioMs);
  montarCatalogoBase();
  aquecerModelo(&g, d, 7701, 4379, 878);
  faroldetalhe_teste_modelo(&g);

  atomic_store(&g_ms, 1000u);
  faroldetalhe_abrir(1, 1000u);
  atomic_store(&g_ms, 13000u);    // fonte chega aos 12 s: fora do prazo
  faroldetalhe_teste_fontes("", &fonte, 1, &caps);
  faroldetalhe_atualizar(1, 0, 0, 0.016f, 13000u);
  assert(faroldetalhe_tem_qualidade() == 0);

  { CatItem tmp = g_itens[0];     // republicacao: mesmo IMDb na posicao 0
    g_itens[0] = g_itens[1]; g_itens[1] = tmp;
    atomic_store(&g_catRev, 2); }
  faroldetalhe_atualizar(0, 0, 0, 0.016f, 13016u);
  fprintf(stderr, "republicacao: mesmo IMDb %s; idade=12016ms; qualidade=%d\n",
          g_itens[0].imdb, faroldetalhe_tem_qualidade());
  assert(!faroldetalhe_tem_qualidade());
  { CatItem tmp = g_itens[0];
    g_itens[0] = g_itens[1]; g_itens[1] = tmp;
    atomic_store(&g_catRev, 3); }
}

// FAROL46B R3: o retrato do Mapa guarda catIndice da revisao antiga; a
// semente tem que ser rebuscada pela identidade e conferida.
static void teste_semente_rebusca_identidade(void) {
  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(relogioS, relogioMs);
  montarCatalogoBase();
  semente_teste = 1;
  mapa_rev_stub++;
  leituras_indice_obsoleto = 0;

  faroldetalhe_abrir(1, 1000u);
  { CatItem tmp = g_itens[0];     // tt9000001 sai do slot 0 (entra o canal)
    g_itens[0] = g_itens[2]; g_itens[2] = tmp;
    atomic_store(&g_catRev, 2); }
  faroldetalhe_atualizar(1, 0, 0, 0.016f, 1016u);
  fprintf(stderr, "semente: slot0=%s, idx(tt9000001)=%d, leituras=%d\n",
          g_itens[0].imdb, cat_indice_por_imdb("tt9000001"),
          leituras_indice_obsoleto);
  assert(leituras_indice_obsoleto == 0);
  semente_teste = 0;
  mapa_rev_stub++;
}

// FAROL46B R4: o codigo do idioma e traduzido (i18n) para buffer separado —
// nunca o mesmo ponteiro de leitura e escrita.
static void teste_idioma_linha_qualidade(void) {
  FarolGosto g;
  FarolInfoFonte fonte;
  FarolInfoCaps caps = { 1, 1 };
  uint16_t d = diaAtual();
  char l0[192];

  memset(&fonte, 0, sizeof fonte);
  fonte.altura = 2160;
  fonte.dv = 1;
  fonte.toca = 1;
  snprintf(fonte.audio, sizeof fonte.audio, "pt");

  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(relogioS, relogioMs);
  montarCatalogoBase();
  aquecerModelo(&g, d, 7701, 4379, 878);
  faroldetalhe_teste_modelo(&g);

  atomic_store(&g_ms, 1000u);
  faroldetalhe_abrir(1, 1000u);
  faroldetalhe_teste_fontes("tt9000002", &fonte, 1, &caps);
  atomic_store(&g_idioma, 1);
  faroldetalhe_atualizar(1, 0, 0, 0.016f, 3500u);
  assert(faroldetalhe_tem_qualidade() == 1);
  assert(faroldetalhe_linha_info(0, l0, sizeof l0));
  assert(strcmp(i18n("Português"), "Português") != 0);
  fprintf(stderr, "linha de qualidade: [%s]; idioma esperado: [%s]\n",
          l0, i18n("Português"));
  assert(strstr(l0, i18n("Português")) != NULL);
  atomic_store(&g_idioma, 0);
}

// FAROL46B R5: "tt…:T:E" normaliza para a serie — mesmo bloco do imdb puro.
static void teste_id_episodio_normaliza(void) {
  FarolInfoFonte fonte;
  FarolInfoCaps caps = { 0, 0 };
  int puro;

  memset(&fonte, 0, sizeof fonte);
  fonte.altura = 1080;
  fonte.toca = 1;
  snprintf(fonte.audio, sizeof fonte.audio, "pt");

  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(relogioS, relogioMs);
  montarCatalogoBase();
  snprintf(g_itens[1].tipo, sizeof g_itens[1].tipo, "series");
  atomic_store(&g_ms, 1000u);
  faroldetalhe_abrir(1, 1000u);
  faroldetalhe_teste_fontes("", &fonte, 1, &caps);
  faroldetalhe_atualizar(1, 1, 2, 0.016f, 1016u);
  puro = faroldetalhe_tem_qualidade();
  assert(puro == 1);

  snprintf(g_itens[1].imdb, sizeof g_itens[1].imdb, "tt9000002:1:2");
  faroldetalhe_abrir(1, 1000u);
  faroldetalhe_atualizar(1, 1, 2, 0.016f, 1016u);
  fprintf(stderr, "qualidade com IMDb puro=%d, com sufixo de episodio=%d\n",
          puro, faroldetalhe_tem_qualidade());
  assert(faroldetalhe_tem_qualidade() == 1);

  snprintf(g_itens[1].imdb, sizeof g_itens[1].imdb, "tt9000002");
  snprintf(g_itens[1].tipo, sizeof g_itens[1].tipo, "movie");
}

// FAROL46B R6: resolucao/formato sem audio declarado ainda contam
// qualidade (FT_INFO_QUALI_SO precisa de modelo de texto).
static void teste_qualidade_sem_audio(void) {
  FarolGosto g;
  FarolInfoFonte fonte;
  FarolInfoCaps caps = { 1, 1 };
  uint16_t d = diaAtual();

  memset(&fonte, 0, sizeof fonte);
  fonte.altura = 2160;
  fonte.dv = 1;
  fonte.toca = 1;
  assert(fonte.audio[0] == 0);

  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(relogioS, relogioMs);
  montarCatalogoBase();
  aquecerModelo(&g, d, 7701, 4379, 878);
  faroldetalhe_teste_modelo(&g);

  atomic_store(&g_ms, 1000u);
  faroldetalhe_abrir(1, 1000u);
  faroldetalhe_teste_fontes("tt9000002", &fonte, 1, &caps);
  faroldetalhe_atualizar(1, 0, 0, 0.016f, 3500u);
  fprintf(stderr, "fonte confirmada 4K DV, audio nao declarado; qualidade=%d\n",
          faroldetalhe_tem_qualidade());
  assert(faroldetalhe_tem_qualidade() == 1);
}

// 5. Estresse concorrente para clang TSan (leitores/navegacao vs atualizacoes).
static _Atomic int g_pararFios = 0;
static void *fioLeitor(void *arg) {
  int id = (int)(intptr_t)arg, i, menu = 0;
  char buf[192];
  for (i = 0; i < 600 && !atomic_load(&g_pararFios); i++) {
    (void)faroldetalhe_altura(1040.0f);
    (void)faroldetalhe_n_selos();
    (void)faroldetalhe_focado();
    (void)faroldetalhe_linha_porque(0, buf, sizeof buf);
    (void)faroldetalhe_linha_info(0, buf, sizeof buf);
    if ((i + id) % 3 == 0) {
      faroldetalhe_focar(1);
      faroldetalhe_tecla((i & 1) ? SDLK_RIGHT : SDLK_LEFT, &menu);
    }
    faroldetalhe_desenhar(96.0f, 700.0f, 1040.0f, 1.0f, ptrDummy, -5);
  }
  return NULL;
}
static void *fioAtualizador(void *arg) {
  (void)arg;
  int i;
  for (i = 0; i < 400; i++) {
    Uint32 t = 2000u + (Uint32)(i * 16);
    atomic_store(&g_ms, t);
    faroldetalhe_atualizar(1, 0, 0, 0.016f, t);
  }
  return NULL;
}

static void teste_concorrencia_tsan(void) {
  pthread_t leitores[3], escritor;
  int i;
  atomic_store(&g_pararFios, 0);
  assert(pthread_create(&escritor, NULL, fioAtualizador, NULL) == 0);
  for (i = 0; i < 3; i++)
    assert(pthread_create(&leitores[i], NULL, fioLeitor, (void *)(intptr_t)i) == 0);
  pthread_join(escritor, NULL);
  atomic_store(&g_pararFios, 1);
  for (i = 0; i < 3; i++) pthread_join(leitores[i], NULL);
}

int main(int argc, char **argv) {
  static const struct { const char *n; void (*f)(void); } ts[] = {
    { "nasce",                teste_nasce_no_quadro_da_ficha },
    { "prazo8s",              teste_qualidade_fade_e_prazo_8s },
    { "ausencia",             teste_ausencia_bloco_zero },
    { "selos",                teste_selos_e_idiomas },
    { "r1-diario-fio-ui",     teste_diario_fora_do_fio_ui },
    { "r2-prazo-identidade",  teste_prazo_identidade_nao_posicao },
    { "r3-semente-identidade",teste_semente_rebusca_identidade },
    { "r4-idioma-buffer",     teste_idioma_linha_qualidade },
    { "r5-id-episodio",       teste_id_episodio_normaliza },
    { "r6-qualidade-sem-audio", teste_qualidade_sem_audio },
    { "tsan",                 teste_concorrencia_tsan },
  };
  int i, achou = 0;
  for (i = 0; i < (int)(sizeof ts / sizeof ts[0]); i++)
    if (argc < 2 || !strcmp(argv[1], ts[i].n)) { ts[i].f(); achou = 1; }
  assert(achou && "teste desconhecido");
  puts("farol_detalhe (core): ok");
  return 0;
}

#else
// ============================================================================
// MODO 2 (-DFD_MODO_GL): Integracao real com src/detail.c + GLES2 1920x1080
// ============================================================================
#include <SDL2/SDL_image.h>
#include "dados.h"
#include "faroldetalhe.h"
#include "farolcena.h"
#include "faroltexto.h"
#include "farolsinal.h"

static void preencherFicha(FarolFicha *f, int32_t tmdb, uint8_t tipo,
                           uint16_t ano, uint16_t min, uint8_t nota,
                           int32_t p0, int32_t k0, int32_t g0) {
  memset(f, 0, sizeof *f);
  f->tmdb = tmdb;
  f->tipo = tipo;
  f->ano = ano;
  f->minutos = min;
  f->nota = nota;
  f->votos = 500;
  f->fonte = FF_DE_TMDB;
  if (p0) { f->pessoa[0] = p0; f->nPessoa = 1; }
  if (k0) { f->tema[0] = k0; f->nTema = 1; }
  if (g0) { f->genero[0] = g0; f->nGenero = 1; }
}

static void aquecerModeloGl(FarolGosto *g, uint16_t dia,
                            int32_t pessoaId, int32_t temaId, int32_t genId) {
  FarolGostoFicha fgf;
  int k;
  farolgosto_iniciar(g, 1);
  memset(&fgf, 0, sizeof fgf);
  fgf.tipo = FG_TIPO_FILME;
  fgf.ano = 2024;
  fgf.minutos = 115;
  fgf.pessoa[0] = pessoaId;
  fgf.tema[0] = temaId;
  fgf.genero[0] = genId;
  for (k = 0; k < 10; k++) {
    fgf.tmdb = 500 + k;
    farolgosto_aplicar(g, &fgf, FG_D_GOSTOU, dia, 2, 1);
  }
}

static time_t glTempoFixo(time_t *out) {
  time_t v = (time_t)1775077200LL;
  if (out) *out = v;
  return v;
}
#define time glTempoFixo
#include "../src/detail.c"
#undef time

static SDL_Window *g_win;
static SDL_GLContext g_gl;

static void enviarTecla(SDL_Keycode sym) {
  SDL_Event ev;
  memset(&ev, 0, sizeof ev);
  ev.type = SDL_KEYDOWN;
  ev.key.keysym.sym = sym;
  detail_evento(&ev);
  if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
    ev.type = SDL_KEYUP;
    detail_evento(&ev);
  }
}

static void renderQuadro(unsigned char *rgba, int w, int h, float dt, Uint32 agora) {
  detail_atualizar(dt, agora);
  txt_novo_quadro();
  gfx_novo_quadro();
  glViewport(0, 0, w, h);
  glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  detail_desenhar(agora);
  glFinish();
  if (rgba) glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
}

static void salvarPng(const char *dir, const char *nome,
                      const unsigned char *rgba, int w, int h) {
  char path[1024];
  unsigned char *flip;
  SDL_Surface *surf;
  int y, pitch = w * 4;
  if (!dir || !dir[0]) return;
  flip = malloc((size_t)pitch * (size_t)h);
  assert(flip);
  for (y = 0; y < h; y++)
    memcpy(flip + (size_t)y * (size_t)pitch,
           rgba + (size_t)(h - 1 - y) * (size_t)pitch, (size_t)pitch);
  surf = SDL_CreateRGBSurfaceFrom(flip, w, h, 32, pitch,
                                  0x000000ffu, 0x0000ff00u,
                                  0x00ff0000u, 0xff000000u);
  assert(surf);
  snprintf(path, sizeof path, "%s/%s.png", dir, nome);
  assert(IMG_SavePNG(surf, path) == 0);
  SDL_FreeSurface(surf);
  free(flip);
}

static long long glEpochFixo(void) { return 1775077200LL; }
static Uint32 g_glMs = 5000u;
static Uint32 glMsFixo(void) { return g_glMs; }

int main(int argc, char **argv) {
  const int W = 1920, H = 1080;
  const size_t BYTES = (size_t)W * (size_t)H * 4u;
  const char *shotDir = argc > 1 ? argv[1] : getenv("FAROL_SHOT_DIR");
  unsigned char *pxDesligado, *pxSemMotivo, *pxComBloco, *pxComSelo;
  CatItem cats[2];
  CatEp eps[3];
  HomeItem hi;
  FarolGosto g;
  FarolFicha fVazia, fRica, fSem;
  FarolInfoFonte fonte;
  FarolInfoCaps caps = { 1, 1 };
  uint16_t dia = (uint16_t)((1775077200LL - 1767225600LL) / 86400LL);
  int i;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
#if defined(__APPLE__)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
  g_win = SDL_CreateWindow("farol-46-test", SDL_WINDOWPOS_UNDEFINED,
                           SDL_WINDOWPOS_UNDEFINED, W, H,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(g_win);
  g_gl = SDL_GL_CreateContext(g_win);
  assert(g_gl);
  glViewport(0, 0, W, H);
  gfx_tamanho_alvo(W, H);
  assert(gfx_iniciar());
  { char ic[4096]; if (realpath("deploy/app/art", ic)) gfx_icones_dir(ic); }
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(16);
  ajustes_iniciar();
  dados_iniciar("deploy/app/art");
  {
    const char *dd = dados_dir();
    char arq[768];
    FILE *fp;
    assert(dd && dd[0]);
    snprintf(arq, sizeof arq, "%s/ajustes.txt", dd);
    fp = fopen(arq, "w");
    assert(fp);
    fputs("idioma 0\nselected_theme 2\nvidroLocal 0\n", fp);
    fclose(fp);
    ajustes_dir(dd);
    farolsinal_pasta(dd);
  }
  ajustes_definir_vidro(1);
  anim_politica_reduzida = 1;

  memset(cats, 0, sizeof cats);
  snprintf(cats[0].imdb, sizeof cats[0].imdb, "tt8100001");
  snprintf(cats[0].titulo, sizeof cats[0].titulo, "Cidade Silenciosa");
  snprintf(cats[0].tipo, sizeof cats[0].tipo, "movie");
  snprintf(cats[0].genero, sizeof cats[0].genero, "Filme · Drama · Mistério");
  snprintf(cats[0].meta, sizeof cats[0].meta, "2024");
  snprintf(cats[0].sinopse, sizeof cats[0].sinopse,
           "Um arquivista encontra fitas antigas numa estacao costeira deserta.");
  cats[0].tmdb = 8101;
  cats[0].nota = 79;
  cats[0].nElenco = 2;
  cats[0].elenco[0].tmdb = 7701;
  snprintf(cats[0].elenco[0].nome, sizeof cats[0].elenco[0].nome, "Rebecca Ferguson");
  cats[0].elenco[1].tmdb = 7702;
  snprintf(cats[0].elenco[1].nome, sizeof cats[0].elenco[1].nome, "Wagner Moura");

  snprintf(cats[1].imdb, sizeof cats[1].imdb, "tt8100002");
  snprintf(cats[1].titulo, sizeof cats[1].titulo, "Sinal do Norte");
  snprintf(cats[1].tipo, sizeof cats[1].tipo, "series");
  snprintf(cats[1].genero, sizeof cats[1].genero, "Série · Ficção científica · Suspense");
  snprintf(cats[1].meta, sizeof cats[1].meta, "2025");
  snprintf(cats[1].sinopse, sizeof cats[1].sinopse,
           "Tres engenheiros investigam um pulso repetido a cada oito horas no Artico.");
  cats[1].tmdb = 8102;
  cats[1].nota = 86;
  cats[1].nTemporadas = 1;
  cats[1].temporadas[0] = 1;
  cats[1].nElenco = 2;
  cats[1].elenco[0].tmdb = 7701;
  snprintf(cats[1].elenco[0].nome, sizeof cats[1].elenco[0].nome, "Rebecca Ferguson");
  cats[1].elenco[1].tmdb = 7702;
  snprintf(cats[1].elenco[1].nome, sizeof cats[1].elenco[1].nome, "Wagner Moura");

  memset(eps, 0, sizeof eps);
  for (i = 0; i < 3; i++) {
    eps[i].temporada = 1;
    eps[i].episodio = i + 1;
    snprintf(eps[i].nome, sizeof eps[i].nome, "Capítulo %d", i + 1);
    snprintf(eps[i].sinopse, sizeof eps[i].sinopse, "O sinal muda de frequencia.");
  }
  cat_definir_tudo(cats, 2, NULL, 0);
  cat_definir_episodios(1, eps, 3);

  pxDesligado = malloc(BYTES);
  pxSemMotivo = malloc(BYTES);
  pxComBloco  = malloc(BYTES);
  pxComSelo   = malloc(BYTES);
  assert(pxDesligado && pxSemMotivo && pxComBloco && pxComSelo);

  // --- PROVA A: Sem motivo nem informacao = layout IDENTICO pixel a pixel ---
  faroldetalhe_teste_reset();
  faroldetalhe_teste_relogio(glEpochFixo, glMsFixo);

  farolsinal_definir_recomendar(0);
  memset(&hi, 0, sizeof hi);
  hi.indice = 0;
  hi.titulo = cats[0].titulo;
  hi.genero = cats[0].genero;
  hi.meta   = cats[0].meta;
  detail_abrir(&hi);
  t = 1.0f; pg = 0.0f; heroDesde = 0;
  for (i = 0; i < 8; i++) renderQuadro(NULL, W, H, 0.016f, 5000u + (Uint32)(i * 16));
  renderQuadro(pxDesligado, W, H, 0.016f, 5200u);

  farolsinal_definir_recomendar(1);
  farolgosto_iniciar(&g, 1);
  faroldetalhe_teste_modelo(&g);
  preencherFicha(&fVazia, 8101, FF_FILME, 2024, 0, 79, 0, 0, 18);
  faroldetalhe_teste_ficha("tt8100001", &fVazia);

  detail_fechar_seco();
  detail_atualizar(0.1f, 5300u);
  detail_abrir(&hi);
  t = 1.0f; pg = 0.0f; heroDesde = 0;
  for (i = 0; i < 8; i++) renderQuadro(NULL, W, H, 0.016f, 5400u + (Uint32)(i * 16));
  assert(faroldetalhe_altura(1040.0f) == 0.0f);
  renderQuadro(pxSemMotivo, W, H, 0.016f, 5600u);

  {
    size_t diffBytes = 0, b;
    for (b = 0; b < BYTES; b++)
      if (pxDesligado[b] != pxSemMotivo[b]) diffBytes++;
    assert(diffBytes == 0);
  }
  salvarPng(shotDir, "sem-bloco", pxSemMotivo, W, H);

  // --- PROVA B: Com bloco P3 + P6 + 3 selos focaveis sem beco ---------------
  aquecerModeloGl(&g, dia, 7701, 4379, 878);
  faroldetalhe_teste_modelo(&g);
  preencherFicha(&fSem, 500, FF_SERIE, 2023, 50, 86, 7701, 4379, 878);
  faroldetalhe_teste_semente("tt7770001", "Silo", &fSem, 1);
  faroldetalhe_teste_vocab(FP_SELO_TEMA, 4379, "time travel", NULL, -1);

  preencherFicha(&fRica, 8101, FF_FILME, 2024, 118, 84, 7701, 4379, 878);
  faroldetalhe_teste_ficha("tt8100001", &fRica);

  memset(&fonte, 0, sizeof fonte);
  fonte.altura = 2160;
  fonte.dv = 1;
  fonte.toca = 1;
  snprintf(fonte.audio, sizeof fonte.audio, "pt");
  faroldetalhe_teste_fontes("tt8100001", &fonte, 1, &caps);

  g_glMs = 6000u;
  detail_fechar_seco();
  detail_atualizar(0.1f, 6000u);
  detail_abrir(&hi);
  t = 1.0f; pg = 0.0f; heroDesde = 0;
  for (i = 0; i < 8; i++) renderQuadro(NULL, W, H, 0.016f, 6000u + (Uint32)(i * 16));
  assert(faroldetalhe_altura(1040.0f) > 0.0f);
  assert(faroldetalhe_tem_porque() == 1);
  assert(faroldetalhe_tem_info() == 1);
  assert(faroldetalhe_tem_qualidade() == 1);
  assert(faroldetalhe_n_selos() == 3);
  renderQuadro(pxComBloco, W, H, 0.016f, 6200u);
  salvarPng(shotDir, "com-bloco", pxComBloco, W, H);

  {
    size_t diffPixels = 0, p;
    for (p = 0; p < (size_t)W * (size_t)H; p++)
      if (memcmp(pxComBloco + p * 4, pxSemMotivo + p * 4, 3) != 0) diffPixels++;
    assert(diffPixels > 5000u);
  }

  assert(nivel == 0 && botao == 0 && !faroldetalhe_focado());
  // Ordem visual da pagina: botoes -> selo de versao -> bloco Farol -> secoes.
  enviarTecla(SDLK_DOWN);
  assert(nivel == 0 && focoVersao && !faroldetalhe_focado());
  enviarTecla(SDLK_DOWN);
  assert(nivel == 0 && !focoVersao && faroldetalhe_focado() &&
         faroldetalhe_selo_atual() == 0);
  renderQuadro(pxComSelo, W, H, 0.016f, 6250u);
  salvarPng(shotDir, "com-bloco-selo", pxComSelo, W, H);
  {
    size_t diffFoco = 0, p;
    for (p = 0; p < (size_t)W * (size_t)H; p++)
      if (memcmp(pxComBloco + p * 4, pxComSelo + p * 4, 3) != 0) diffFoco++;
    assert(diffFoco > 400u);
  }

  enviarTecla(SDLK_LEFT);
  assert(detail_pediu_menu() == 1);
  assert(faroldetalhe_focado() && faroldetalhe_selo_atual() == 0);

  enviarTecla(SDLK_RIGHT);
  assert(faroldetalhe_selo_atual() == 1);
  enviarTecla(SDLK_RIGHT);
  assert(faroldetalhe_selo_atual() == 2);
  enviarTecla(SDLK_RIGHT);
  assert(faroldetalhe_selo_atual() == 2);

  enviarTecla(SDLK_DOWN);
  assert(nivel == 1 && !faroldetalhe_focado());
  enviarTecla(SDLK_UP);
  assert(nivel == 0 && faroldetalhe_focado() && faroldetalhe_selo_atual() == 2);
  enviarTecla(SDLK_UP);
  assert(nivel == 0 && !faroldetalhe_focado() && focoVersao);

  enviarTecla(SDLK_DOWN);
  assert(faroldetalhe_focado() && faroldetalhe_selo_atual() == 2);
  enviarTecla(SDLK_ESCAPE);
  assert(!saindo && nivel == 0 && !faroldetalhe_focado() && botao == 0);

  enviarTecla(SDLK_DOWN);
  assert(nivel == 0 && focoVersao && !faroldetalhe_focado());
  enviarTecla(SDLK_DOWN);
  assert(faroldetalhe_focado() && faroldetalhe_selo_atual() == 2);
  enviarTecla(SDLK_LEFT);
  enviarTecla(SDLK_LEFT);
  assert(faroldetalhe_selo_atual() == 0);
  enviarTecla(SDLK_RETURN);
  assert(pessoaAberta == 1);
  enviarTecla(SDLK_ESCAPE);
  renderQuadro(NULL, W, H, 0.016f, 6266u);   // filmo fecha na atualizacao
  assert(pessoaAberta == 0 && !saindo);

  enviarTecla(SDLK_RIGHT);
  assert(faroldetalhe_selo_atual() == 1);
  enviarTecla(SDLK_RETURN);
#ifdef FD_TEM_SELO_FIO
  {
    int seloTipo = -1;
    char seloFio[64];
    assert(detail_pediu_explorar_selo(&seloTipo, seloFio, sizeof seloFio) == 1);
    assert(seloTipo == FP_SELO_TEMA && seloFio[0] != 0);
    fprintf(stderr, "selo de tema -> Explorar fio=[%s]\n", seloFio);
  }
#else
  assert(detail_pediu_explorar() == 1);
#endif

  preencherFicha(&fRica, 8102, FF_SERIE, 2025, 50, 86, 7701, 4379, 878);
  faroldetalhe_teste_ficha("tt8100002", &fRica);
  faroldetalhe_teste_fontes("tt8100002:1:1", &fonte, 1, &caps);
  hi.indice = 1;
  hi.titulo = cats[1].titulo;
  hi.genero = cats[1].genero;
  hi.meta   = cats[1].meta;
  detail_fechar_seco();
  detail_atualizar(0.1f, 7000u);
  detail_abrir(&hi);
  t = 1.0f; pg = 0.0f;
  renderQuadro(NULL, W, H, 0.016f, 7100u);

  {
    static const SDL_Keycode DIRS[4] = { SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT };
    uint32_t rng = 0x46A011u;
    int step, k;
    for (step = 0; step < 300; step++) {
      rng = rng * 1664525u + 1013904223u;
      enviarTecla(DIRS[(rng >> 24) & 3u]);
      (void)detail_pediu_menu();
      pediuRelogio = 0;
      if (step % 15 == 14) {
        for (k = 0; k < 8 && (nivel != 0 || faroldetalhe_focado() || focoVersao); k++)
          enviarTecla(SDLK_UP);
        assert(nivel == 0 && !faroldetalhe_focado() && !focoVersao);
        for (k = 0; k < 5 && nivel == 0; k++)
          enviarTecla(SDLK_DOWN);
        assert(nivel == 1 && !faroldetalhe_focado());
      }
    }
  }

  farolgosto_iniciar(&g, 1);
  faroldetalhe_teste_modelo(&g);
  detail_fechar_seco();
  detail_atualizar(0.1f, 8000u);
  detail_abrir(&hi);
  t = 1.0f; pg = 0.0f;
  renderQuadro(NULL, W, H, 0.016f, 8100u);
  assert(faroldetalhe_tem_info() == 1 && faroldetalhe_n_selos() == 0);
  enviarTecla(SDLK_DOWN);
  assert(nivel == 0 && focoVersao && !faroldetalhe_focado());
  enviarTecla(SDLK_DOWN);
  assert(nivel == 1 && !faroldetalhe_focado());
  enviarTecla(SDLK_UP);
  if (nivel == 1) enviarTecla(SDLK_UP);
  assert(nivel == 0 && !faroldetalhe_focado());

  free(pxDesligado);
  free(pxSemMotivo);
  free(pxComBloco);
  free(pxComSelo);
  tex_encerrar();
  SDL_GL_DeleteContext(g_gl);
  SDL_DestroyWindow(g_win);
  SDL_Quit();
  puts("farol_detalhe (gl + detail.c): ok");
  return 0;
}
#endif
