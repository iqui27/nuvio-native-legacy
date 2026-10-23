// Ver legref.h. Coletor INDEPENDENTE de uma faixa de texto embutida no MKV,
// para servir de referencia ao AutoSync. Nao conversa com mkvass/overlay.
#include "legref.h"
#include "rede.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <errno.h>
// strcasestr is a GNU extension: the webOS toolchain and emcc do not declare it.
static int contemSemCaixa(const char *h, const char *n) {
  size_t k = strlen(n);
  for (; *h; h++) if (!strncasecmp(h, n, k)) return 1;
  return 0;
}

#define LR_CABECA   (64L * 1024L)        // inicio do arquivo: EBML, SeekHead, Info, Tracks
#define LR_TRACKS_MAX (2L * 1024L * 1024L)
#define LR_CUES_MAX (8L * 1024L * 1024L)
#define LR_BLOCO    2048L                // janela de um BlockGroup de legenda
#define LR_BLOCO_MAX (64L * 1024L)
#define LR_VAO      (8L * 1024L)         // junta Ranges vizinhos a menos disto
#define LR_LEITURA_MAX (128L * 1024L)
#define LR_FAIXAS   64
#define LR_EVENTOS_MAX 7999              // acima disto o AutoSync recusa de qualquer jeito

// --- EBML -------------------------------------------------------------------
static int largura(unsigned char b) {
  int n = 1;
  if (!b) return 0;
  while (!(b & 0x80)) { b <<= 1; n++; }
  return n;
}
static int lerId(const unsigned char *p, long resta, unsigned long *id) {
  int n, i;
  if (resta < 1) return 0;
  n = largura(p[0]);
  if (n < 1 || n > 4 || n > resta) return 0;
  *id = 0;
  for (i = 0; i < n; i++) *id = (*id << 8) | p[i];
  return n;
}
// Tamanho com o marcador removido. -1 em *tam = desconhecido (todos os bits 1).
static int lerTam(const unsigned char *p, long resta, long long *tam) {
  int n, i, uns;
  unsigned long long v;
  if (resta < 1) return 0;
  n = largura(p[0]);
  if (n < 1 || n > 8 || n > resta) return 0;
  v = p[0] & (0xFFu >> n);
  uns = v == (0xFFu >> n);
  for (i = 1; i < n; i++) { v = (v << 8) | p[i]; if (p[i] != 0xFF) uns = 0; }
  *tam = uns ? -1 : (long long)v;
  return n;
}
static unsigned long long lerUint(const unsigned char *p, long n) {
  unsigned long long v = 0;
  long i;
  for (i = 0; i < n && i < 8; i++) v = (v << 8) | p[i];
  return v;
}
// Proximo filho dentro de [o, fim). Devolve 1 com id/dados/tam; 0 no fim ou
// em lixo. Filho que passa do fim e lixo (o pai mente ou o buffer foi cortado).
typedef struct { const unsigned char *p; long o, fim; } It;
static int prox(It *it, unsigned long *id, const unsigned char **d, long *tam) {
  int a, b;
  long long t;
  if (it->o >= it->fim) return 0;
  a = lerId(it->p + it->o, it->fim - it->o, id);
  if (!a) return 0;
  b = lerTam(it->p + it->o + a, it->fim - it->o - a, &t);
  if (!b || t < 0 || t > it->fim - it->o - a - b) return 0;
  *d = it->p + it->o + a + b; *tam = (long)t;
  it->o += a + b + (long)t;
  return 1;
}

// --- PARSERS PUROS ------------------------------------------------------------
typedef struct {
  int numero, tipo, forced, codificada;
  char codec[24], idioma[24], nome[96];
  const unsigned char *priv; long nPriv;
} Faixa;

static void copiaStr(char *dst, size_t tam, const unsigned char *d, long n) {
  size_t k = (size_t)n < tam - 1 ? (size_t)n : tam - 1;
  memcpy(dst, d, k); dst[k] = 0;
  while (k && !dst[k - 1]) dst[--k] = 0;
}

static int lerTracks(const unsigned char *p, long n, Faixa *v, int max) {
  It it = { p, 0, n }, sub;
  unsigned long id, id2;
  const unsigned char *d, *d2;
  long t, t2;
  int k = 0;
  while (prox(&it, &id, &d, &t)) {
    Faixa f;
    if (id != 0xAE || k >= max) continue;
    memset(&f, 0, sizeof f);
    snprintf(f.idioma, sizeof f.idioma, "eng");   // padrao do Matroska
    sub = (It){ d, 0, t };
    while (prox(&sub, &id2, &d2, &t2)) {
      if (id2 == 0xD7) f.numero = (int)lerUint(d2, t2);
      else if (id2 == 0x83) f.tipo = (int)lerUint(d2, t2);
      else if (id2 == 0x86) copiaStr(f.codec, sizeof f.codec, d2, t2);
      else if (id2 == 0x22B59C) copiaStr(f.idioma, sizeof f.idioma, d2, t2);
      else if (id2 == 0x22B59D && t2 > 0) copiaStr(f.idioma, sizeof f.idioma, d2, t2);
      else if (id2 == 0x536E) copiaStr(f.nome, sizeof f.nome, d2, t2);
      else if (id2 == 0x55AA) f.forced = lerUint(d2, t2) != 0;
      else if (id2 == 0x63A2) { f.priv = d2; f.nPriv = t2; }
      else if (id2 == 0x6D80) f.codificada = 1;   // compressao/cifra: nao lemos
    }
    v[k++] = f;
  }
  return k;
}

int legref_cues(const unsigned char *p, long n, int faixa, double escala,
                LegRefPonto **saida, int *semRel) {
  It it = { p, 0, n };
  unsigned long id;
  const unsigned char *d;
  long t;
  int k = 0, cap = 0;
  LegRefPonto *v = NULL;
  *saida = NULL; if (semRel) *semRel = 0;
  while (prox(&it, &id, &d, &t)) {
    It cp = { d, 0, t }, tp;
    unsigned long id2, id3;
    const unsigned char *d2, *d3;
    long t2, t3;
    double tempo = -1;
    if (id != 0xBB) continue;
    // CueTime vem antes das posicoes em todo muxer conhecido, mas a ordem nao
    // e garantida: le o tempo primeiro.
    while (prox(&cp, &id2, &d2, &t2)) if (id2 == 0xB3) tempo = (double)lerUint(d2, t2) * escala;
    if (tempo < 0) continue;
    cp.o = 0;
    while (prox(&cp, &id2, &d2, &t2)) {
      long long pos = -1; int rel = -1, tr = 0; double dur = -1;
      if (id2 != 0xB7) continue;
      tp = (It){ d2, 0, t2 };
      while (prox(&tp, &id3, &d3, &t3)) {
        if (id3 == 0xF7) tr = (int)lerUint(d3, t3);
        else if (id3 == 0xF1) pos = (long long)lerUint(d3, t3);
        else if (id3 == 0xF0) rel = (int)lerUint(d3, t3);
        else if (id3 == 0xB2) dur = (double)lerUint(d3, t3) * escala;
      }
      if (tr != faixa || pos < 0) continue;
      if (rel < 0) { if (semRel) (*semRel)++; continue; }
      if (k == cap) {
        LegRefPonto *nv;
        if (cap >= LR_EVENTOS_MAX + 1) { free(v); return -1; }
        cap = cap ? cap * 2 : 256;
        nv = realloc(v, (size_t)cap * sizeof *v);
        if (!nv) { free(v); return -1; }
        v = nv;
      }
      v[k++] = (LegRefPonto){ pos, rel, tempo, dur };
    }
  }
  *saida = v;
  return k;
}

// --- O MODULO -----------------------------------------------------------------
typedef struct {
  uint64_t id, sessao;
  char url[4096], idioma[24], cabs[2048];
  int excl[16], nExcl;
  LegRefOrcamento orc;
  LegendaDocumento *(*local)(const char *, uint64_t, const int *, int);
} Pedido;

struct LegRef {
  pthread_mutex_t m;
  pthread_cond_t c;
  pthread_t fio;
  int fioVivo, sair, pausado;
  LegRefLer ler; void *lerU;
  Pedido ped;              // o pedido atual (id 0 = nenhum)
  int temPedido;           // ainda nao pego pelo fio
  uint64_t gerador;
  LegRefStatus st;
  LegendaDocumento *doc;   // pronto, do pedido st.pedido
};

typedef struct {
  LegRef *r; uint64_t id;
  Pedido p;
  char urlFinal[4096];
  long long bytes; int pedidos;
  struct timespec ultimo; int temUltimo;
  LegRefMotivo erro;
} Job;

static int parou(void *u) {
  Job *j = u; LegRef *r = j->r; int x;
  pthread_mutex_lock(&r->m); x = r->sair || r->ped.id != j->id; pthread_mutex_unlock(&r->m);
  return x;
}

static long msDesde(const struct timespec *a) {
  struct timespec b; clock_gettime(CLOCK_MONOTONIC, &b);
  return (long)((b.tv_sec - a->tv_sec) * 1000 + (b.tv_nsec - a->tv_nsec) / 1000000);
}

// Espera `ms` acordando no cancelamento. 1 = cancelado.
static int esperar(Job *j, long ms) {
  LegRef *r = j->r; struct timespec ate; int x;
  clock_gettime(CLOCK_REALTIME, &ate);
  ate.tv_sec += ms / 1000; ate.tv_nsec += (ms % 1000) * 1000000L;
  if (ate.tv_nsec >= 1000000000L) { ate.tv_sec++; ate.tv_nsec -= 1000000000L; }
  pthread_mutex_lock(&r->m);
  while (!(x = r->sair || r->ped.id != j->id))
    if (pthread_cond_timedwait(&r->c, &r->m, &ate) == ETIMEDOUT) break;
  x = r->sair || r->ped.id != j->id;
  pthread_mutex_unlock(&r->m);
  return x;
}

static void progresso(Job *j, int feitos, int total) {
  LegRef *r = j->r;
  pthread_mutex_lock(&r->m);
  if (r->st.pedido == j->id) {
    r->st.feitos = feitos; r->st.total = total;
    r->st.bytes = j->bytes; r->st.pedidos = j->pedidos;
  }
  pthread_mutex_unlock(&r->m);
}

// Um Range, respeitando pausa, ritmo e orcamento. NULL com j->erro preenchido.
static unsigned char *ler(Job *j, long long ini, long n, long *tam) {
  LegRef *r = j->r; unsigned char *b; int st = 0;
  long intervalo = j->p.orc.pedidosPorSeg > 0 ? 1000 / j->p.orc.pedidosPorSeg : 0;
  for (;;) {   // PAUSA: seek/buffer curto. Nao conta prazo nem orcamento.
    int pausa;
    pthread_mutex_lock(&r->m); pausa = r->pausado; pthread_mutex_unlock(&r->m);
    if (!pausa) break;
    if (esperar(j, 200)) { j->erro = LEGREF_PARADO; return NULL; }
  }
  if (j->temUltimo && intervalo > 0) {
    long passou = msDesde(&j->ultimo);
    if (passou < intervalo && esperar(j, intervalo - passou)) { j->erro = LEGREF_PARADO; return NULL; }
  }
  if (parou(j)) { j->erro = LEGREF_PARADO; return NULL; }
  if (j->pedidos >= j->p.orc.maxPedidos || j->bytes + n > j->p.orc.maxBytes) {
    j->erro = LEGREF_ORCAMENTO; return NULL;
  }
  j->pedidos++;
  clock_gettime(CLOCK_MONOTONIC, &j->ultimo); j->temUltimo = 1;
  *tam = 0;
  b = r->ler(r->lerU, j->urlFinal, ini, n, tam, &st, parou, j);
  if (b) j->bytes += *tam;
  if (parou(j)) { free(b); j->erro = LEGREF_PARADO; return NULL; }
  if (!b) { if (j->erro != LEGREF_PARADO) j->erro = st == 200 ? LEGREF_SEM_RANGE : LEGREF_REDE; return NULL; }
  if (st != 206) { free(b); j->erro = LEGREF_SEM_RANGE; return NULL; }
  return b;
}

// Elemento inteiro em `abs`, com teto. Le o cabecalho e depois o corpo.
static unsigned char *lerElemento(Job *j, long long abs, unsigned long esperado, long teto,
                                  long *dados, long *nDados) {
  long tam = 0, n2 = 0; unsigned long id; long long t; int a, b;
  unsigned char *h = ler(j, abs, 16, &tam), *corpo;
  if (!h) return NULL;
  a = lerId(h, tam, &id);
  b = a ? lerTam(h + a, tam - a, &t) : 0;
  free(h);
  if (!a || !b || id != esperado || t < 0) { j->erro = LEGREF_SEM_INDICE; return NULL; }
  if (t > teto) { j->erro = LEGREF_ORCAMENTO; return NULL; }
  corpo = ler(j, abs + a + b, (long)t, &n2);
  if (!corpo) return NULL;
  if (n2 < t) { free(corpo); j->erro = LEGREF_INCOMPLETO; return NULL; }
  *dados = 0; *nDados = (long)t;
  return corpo;
}

// --- BLOCOS ---------------------------------------------------------------------
typedef struct { double inicio, fim; char *texto; } Evento;

// Le um BlockGroup/SimpleBlock da faixa em p. 1 = ok; 0 = nao e este;
// -1 = precisa de *precisa bytes; -2 = lacing/sem duracao (faixa nao serve).
static int lerGrupo(const unsigned char *p, long n, int faixa, double escala, double durCue,
                    int *rel16, double *dur, const unsigned char **txt, long *nTxt, long *precisa) {
  unsigned long id; long long s; int a, b;
  a = lerId(p, n, &id);
  if (!a || (id != 0xA0 && id != 0xA3)) return 0;
  b = lerTam(p + a, n - a, &s);
  if (!b || s <= 4 || s > LR_BLOCO_MAX) return 0;
  if (a + b + s > n) { *precisa = a + b + (long)s; return -1; }
  {
    const unsigned char *bloco = NULL; long nb = 0; double d = -1;
    if (id == 0xA3) { bloco = p + a + b; nb = (long)s; }
    else {
      It it = { p + a + b, 0, (long)s }; unsigned long id2; const unsigned char *d2; long t2;
      int achou = 0;
      while (prox(&it, &id2, &d2, &t2)) {
        if (id2 == 0xA1) { bloco = d2; nb = t2; achou++; }
        else if (id2 == 0x9B) d = (double)lerUint(d2, t2) * escala;
      }
      // Os filhos tem de fechar EXATAMENTE no tamanho do grupo: e o que separa
      // um BlockGroup de verdade de bytes de video que comecam com 0xA0.
      if (it.o != s || achou != 1) return 0;
    }
    {
      int c = largura(bloco[0]), k; unsigned long tr = 0;
      if (c < 1 || c > 4 || c + 3 > nb) return 0;
      tr = bloco[0] & (0xFFu >> c);
      for (k = 1; k < c; k++) tr = (tr << 8) | bloco[k];
      if ((int)tr != faixa) return 0;
      *rel16 = (int)(short)((bloco[c] << 8) | bloco[c + 1]);
      if ((bloco[c + 2] >> 1) & 3) return -2;   // lacing em legenda: nao lemos
      if (d <= 0) d = durCue;
      if (d <= 0) return -2;
      *dur = d; *txt = bloco + c + 3; *nTxt = nb - c - 3;
    }
  }
  return 1;
}

// --- MONTAGEM DO DOCUMENTO -------------------------------------------------------
typedef struct { char *s; size_t n, cap; int falhou; } Str;
static void anexar(Str *b, const char *s, size_t n) {
  if (b->falhou) return;
  if (b->n + n + 1 > b->cap) {
    size_t cap = b->cap ? b->cap : 65536; char *nv;
    while (cap < b->n + n + 1) cap *= 2;
    if (cap > 16u * 1024u * 1024u) { b->falhou = 1; return; }
    nv = realloc(b->s, cap);
    if (!nv) { b->falhou = 1; return; }
    b->s = nv; b->cap = cap;
  }
  memcpy(b->s + b->n, s, n); b->n += n; b->s[b->n] = 0;
}
static void anexarf(Str *b, const char *fmt, double t, int ass) {
  char x[32]; long ms = (long)(t * 1000.0 + 0.5);
  if (ass) snprintf(x, sizeof x, fmt, ms / 3600000, ms / 60000 % 60, ms / 1000 % 60, ms % 1000 / 10);
  else snprintf(x, sizeof x, fmt, ms / 3600000, ms / 60000 % 60, ms / 1000 % 60, ms % 1000);
  anexar(b, x, strlen(x));
}

static LegendaDocumento *montar(const Faixa *f, Evento *ev, int n, uint64_t sessao, int ass) {
  Str b = { 0 }; int i; LegendaDocumento *doc; int nDoc = 0;
  LegendaDocumentoInfo info;
  memset(&info, 0, sizeof info);
  info.sessao = sessao;
  info.flags = LEGENDA_DOC_COMPLETO | (f->forced ? LEGENDA_DOC_FORCED : 0);
  snprintf(info.idioma, sizeof info.idioma, "%s", f->idioma);
  snprintf(info.origem, sizeof info.origem, "Embedded");
  // Identidade OPACA: o numero da faixa, nunca a URL.
  snprintf(info.identidade, sizeof info.identidade, "mkv-track:%d", f->numero);
  if (ass) {
    if (f->priv && f->nPriv > 0) anexar(&b, (const char *)f->priv, strnlen((const char *)f->priv, (size_t)f->nPriv));
    if (!b.s || !strstr(b.s, "[Events]"))
      anexar(&b, "\n[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n", 88);
    else anexar(&b, "\n", 1);
  }
  for (i = 0; i < n; i++) {
    const char *t = ev[i].texto;
    if (ass) {
      // Bloco ASS do Matroska: ReadOrder, Layer, Style, Name, MarginL, MarginR,
      // MarginV, Effect, Text. Vira Dialogue: Layer, Start, End, Style, ...
      const char *c1 = strchr(t, ','), *c2 = c1 ? strchr(c1 + 1, ',') : NULL;
      if (!c2) continue;
      anexar(&b, "Dialogue: ", 10);
      anexar(&b, c1 + 1, (size_t)(c2 - c1 - 1));
      anexarf(&b, ",%ld:%02ld:%02ld.%02ld", ev[i].inicio, 1);
      anexarf(&b, ",%ld:%02ld:%02ld.%02ld", ev[i].fim, 1);
      anexar(&b, c2, strlen(c2));
      anexar(&b, "\n", 1);
    } else {
      char num[16]; snprintf(num, sizeof num, "%d\n", i + 1);
      anexar(&b, num, strlen(num));
      anexarf(&b, "%02ld:%02ld:%02ld,%03ld", ev[i].inicio, 0);
      anexar(&b, " --> ", 5);
      anexarf(&b, "%02ld:%02ld:%02ld,%03ld", ev[i].fim, 0);
      anexar(&b, "\n", 1);
      anexar(&b, t, strlen(t));
      anexar(&b, "\n\n", 2);
    }
  }
  if (b.falhou || !b.s) { free(b.s); return NULL; }
  doc = legenda_documento_criar(b.s, &info);
  // O parser descartou alguma fala (texto vazio, tempo invalido): o documento
  // pode ate servir para desenho, mas nao e a faixa inteira.
  if (doc) { legenda_documento_dados(doc, &nDoc);
    if (nDoc != n && (legenda_documento_info(doc)->flags & LEGENDA_DOC_COMPLETO)) {
      legenda_documento_liberar(doc);
      info.flags &= ~LEGENDA_DOC_COMPLETO;
      doc = legenda_documento_criar(b.s, &info);
    } }
  free(b.s);
  return doc;
}

static int textoFaixa(const Faixa *f) {
  return f->tipo == 0x11 && !f->codificada &&
         (!strcmp(f->codec, "S_TEXT/UTF8") || !strcmp(f->codec, "S_TEXT/ASS") ||
          !strcmp(f->codec, "S_TEXT/SSA"));
}
// Letreiros/forced traduzem placas, nao o dialogo: nao servem de referencia.
static int letreiro(const Faixa *f) {
  static const char *const p[] = { "forced", "sign", "song", "letreiro", "forzad", "forcé" };
  size_t i;
  if (f->forced) return 1;
  for (i = 0; i < sizeof p / sizeof *p; i++) if (contemSemCaixa(f->nome, p[i])) return 1;
  return 0;
}
// ISO 639-2 (o que MP4 e muito MKV gravam) para as duas letras do 639-1 que
// a pessoa escolhe. Fora da tabela, as duas primeiras letras.
static void idioma2(const char *x, char o[3]) {
  static const char *const t[] = { "eng","en","por","pt","spa","es","fra","fr","fre","fr","deu","de","ger","de",
    "ita","it","jpn","ja","kor","ko","zho","zh","chi","zh","rus","ru","nld","nl","dut","nl","pol","pl","tur","tr",
    "ara","ar","swe","sv","nor","no","nob","no","dan","da","fin","fi","ces","cs","cze","cs","hun","hu","ron","ro",
    "rum","ro","ell","el","gre","el","heb","he","hin","hi","tha","th","vie","vi","ind","id","ukr","uk","cat","ca",
    "hrv","hr","srp","sr","slk","sk","slo","sk","bul","bg","msa","ms","may","ms", NULL };
  o[0] = o[1] = o[2] = 0;
  if (strlen(x) == 3)
    for (int i = 0; t[i]; i += 2) if (!strncasecmp(x, t[i], 3)) { o[0] = t[i + 1][0]; o[1] = t[i + 1][1]; return; }
  if (x[0] && x[1]) { o[0] = (char)(x[0] | 32); o[1] = (char)(x[1] | 32); }
}
static int mesmoIdioma(const char *a, const char *b) {
  char x[3], y[3];
  idioma2(a, x); idioma2(b, y);
  return x[0] && y[0] && x[0] == y[0] && x[1] == y[1];
}
static int cmpPonto(const void *a, const void *b) {
  const LegRefPonto *x = a, *y = b;
  if (x->pos != y->pos) return x->pos < y->pos ? -1 : 1;
  return x->rel - y->rel;
}

// So TEMPOS (Cues do MKV com CueDuration, tabela de amostras do MP4): o
// texto e um rotulo distinto por fala, sem pretensao de texto (o AutoSync so
// usa o tempo; o rotulo passa nos filtros de "fala de verdade").
typedef struct { double inicio, fim; } Tempo;
static LegendaDocumento *montarTempos(const Tempo *v, int n, const char *idioma, const char *ident,
                                      int forced, uint64_t sessao) {
  LegendaCue *c = calloc((size_t)(n ? n : 1), sizeof *c); LegendaDocumento *d;
  LegendaDocumentoInfo info;
  if (!c) return NULL;
  for (int i = 0; i < n; i++) {
    c[i].inicio = v[i].inicio; c[i].fim = v[i].fim; c[i].cor = -1; c[i].posX = c[i].posY = -1; c[i].ordem = i;
    snprintf(c[i].texto, sizeof c[i].texto, "fala %04d", i + 1);
  }
  memset(&info, 0, sizeof info);
  info.sessao = sessao; info.flags = LEGENDA_DOC_COMPLETO | (forced ? LEGENDA_DOC_FORCED : 0);
  snprintf(info.idioma, sizeof info.idioma, "%s", idioma);
  snprintf(info.origem, sizeof info.origem, "Embedded");
  snprintf(info.identidade, sizeof info.identidade, "%s", ident);
  d = n > 0 ? legenda_documento_de_cues(c, n, &info) : NULL;
  free(c);
  return d;
}
static int cmpTempo(const void *a, const void *b) {
  const Tempo *x = a, *y = b; return x->inicio < y->inicio ? -1 : x->inicio > y->inicio;
}
static void anunciar(Job *j, LegRefStatus *st, int numero, const char *idioma, const char *codec) {
  st->faixa = numero;
  snprintf(st->idioma, sizeof st->idioma, "%s", idioma);
  snprintf(st->codec, sizeof st->codec, "%s", codec);
  pthread_mutex_lock(&j->r->m);
  if (j->r->st.pedido == j->id) {
    j->r->st.faixa = numero;
    snprintf(j->r->st.idioma, sizeof j->r->st.idioma, "%s", idioma);
    snprintf(j->r->st.codec, sizeof j->r->st.codec, "%s", codec);
  }
  pthread_mutex_unlock(&j->r->m);
}

// --- ESCOLHA E VALIDACAO CRUZADA DAS FAIXAS --------------------------------------
// Ordem de preferencia: o idioma pedido antes; dentro dele, a faixa comum
// antes da SDH/CC (que traz [porta batendo] a mais). Comentario fica de fora:
// nao e o dialogo do filme.
static int comentario(const char *nome) {
  return contemSemCaixa(nome, "comment") || contemSemCaixa(nome, "coment");
}
static int sdh(const char *nome) {
  return contemSemCaixa(nome, "sdh") || contemSemCaixa(nome, "hearing") || contemSemCaixa(nome, "cc") ||
         contemSemCaixa(nome, "surdo");
}
static int ordenar(const char *const *idiomas, const char *const *nomes, const int *ok, int n,
                   const char *quer, int *ordem) {
  int k = 0;
  for (int passo = 0; passo < 4; passo++)
    for (int i = 0; i < n; i++) {
      if (!ok[i]) continue;
      int mesmo = mesmoIdioma(idiomas[i], quer), s = sdh(nomes[i]);
      if ((passo == 0 && mesmo && !s) || (passo == 1 && mesmo && s) || (passo == 2 && !mesmo && !s) ||
          (passo == 3 && !mesmo && s)) ordem[k++] = i;
    }
  return k;
}
// Fracao dos inicios de a com um inicio de b a ate 250 ms, com b deslocado.
static double concordancia(const Tempo *a, int na, const Tempo *b, int nb, double off) {
  int j = 0, c = 0;
  if (na <= 0 || nb <= 0) return 0;
  for (int i = 0; i < na; i++) {
    while (j < nb && b[j].inicio + off < a[i].inicio - .25) j++;
    if (j < nb && b[j].inicio + off <= a[i].inicio + .25) c++;
  }
  return (double)c / na;
}
// 1 concordam no tempo; -1 a mesma estrutura DESLOCADA (uma das duas esta
// fora); 0 sem como dizer (traducao com outro corte, faixa curta).
static int comparar(const Tempo *a, int na, const Tempo *b, int nb) {
  double z = concordancia(a, na, b, nb, 0), melhor = 0, mo = 0;
  if (na < 20 || nb < 20) return 0;
  if (z >= .5) return 1;
  for (int k = -100; k <= 100; k++) {
    double c = concordancia(a, na, b, nb, k * .1);
    if (c > melhor) { melhor = c; mo = k * .1; }
  }
  return melhor >= .5 && (mo >= .5 || mo <= -.5) && z < .25 ? -1 : 0;
}
// A preferida cai so se discorda (deslocada) de DUAS outras que concordam
// entre si: com uma so, nao ha como saber qual das duas esta fora. Devolve a
// posicao em v[] da faixa a usar (0 = a preferida).
static int validar(Tempo *const *v, const int *n, int nv, int *descartou) {
  *descartou = 0;
  for (int x = 1; x < nv; x++) {
    if (!v[x] || comparar(v[0], n[0], v[x], n[x]) != -1) continue;
    for (int y = x + 1; y < nv; y++)
      if (v[y] && comparar(v[0], n[0], v[y], n[y]) == -1 && comparar(v[x], n[x], v[y], n[y]) == 1) {
        *descartou = 1; return x;
      }
  }
  return 0;
}

// --- MP4 / MOV -----------------------------------------------------------------
// Le so o `moov`: as caixas de topo sao puladas pelo tamanho declarado (um
// `mdat` de 20 GB custa os 16 bytes do cabecalho). Da faixa de texto (tx3g,
// wvtt, QuickTime text) bastam stts (duracoes), ctts (deslocamento, raro em
// texto), stsz (tamanho: amostra vazia = intervalo sem fala) e a lista de
// edicao. O texto em si nao e lido (nem stco/stsc): o AutoSync so usa tempo.
// MP4 fragmentado (mvex/moof) fica de fora: o indice nao esta no moov.
#define LR_MOOV_MAX (16L * 1024L * 1024L)
#define LR_CAIXAS   24
static unsigned long be32(const unsigned char *p) {
  return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}
static unsigned long long be64(const unsigned char *p) { return ((unsigned long long)be32(p) << 32) | be32(p + 4); }
typedef struct { const unsigned char *p; long o, fim; } Cx;
static int caixa(Cx *c, char tipo[5], const unsigned char **d, long *n) {
  unsigned long long t; long h = 8;
  if (c->fim - c->o < 8) return 0;
  t = be32(c->p + c->o); memcpy(tipo, c->p + c->o + 4, 4); tipo[4] = 0;
  if (t == 1) { if (c->fim - c->o < 16) return 0; t = be64(c->p + c->o + 8); h = 16; }
  else if (t == 0) t = (unsigned long long)(c->fim - c->o);
  if (t < (unsigned long long)h || t > (unsigned long long)(c->fim - c->o)) return 0;
  *d = c->p + c->o + h; *n = (long)(t - (unsigned long long)h); c->o += (long)t;
  return 1;
}
static const unsigned char *filho(const unsigned char *p, long n, const char *quer, long *tam) {
  Cx c = { p, 0, n }; char t[5]; const unsigned char *d; long m;
  while (caixa(&c, t, &d, &m)) if (!memcmp(t, quer, 4)) { *tam = m; return d; }
  return NULL;
}
typedef struct {
  int numero, forced, vazio;
  char codec[8], idioma[8], nome[96];
  unsigned long escala;
  const unsigned char *stts, *stsz, *ctts; long nStts, nStsz, nCtts; int vCtts;
  double desloc;
} Mp4Faixa;
static int mp4Faixa(const unsigned char *p, long n, unsigned long escalaFilme, Mp4Faixa *f) {
  long m, k, ne; const unsigned char *x, *mdia, *stbl, *e;
  memset(f, 0, sizeof *f);
  if ((x = filho(p, n, "tkhd", &m)) && m >= 24) f->numero = (int)be32(x + (x[0] == 1 ? 20 : 12));
  if (!(mdia = filho(p, n, "mdia", &k))) return 0;
  if ((x = filho(mdia, k, "hdlr", &m)) && m >= 24) {
    if (memcmp(x + 8, "text", 4) && memcmp(x + 8, "sbtl", 4) && memcmp(x + 8, "subt", 4)) return 0;
    copiaStr(f->nome, sizeof f->nome, x + 24, m - 24);
  } else return 0;
  if (!(x = filho(mdia, k, "mdhd", &m)) || m < 24) return 0;
  {
    int v1 = x[0] == 1; unsigned lang;
    if (v1 && m < 36) return 0;
    f->escala = be32(x + (v1 ? 20 : 12));
    lang = (unsigned)((x[v1 ? 32 : 20] << 8) | x[v1 ? 33 : 21]);
    if (lang && lang != 0x7FFF) {   // tres letras de 5 bits; 0x7FFF / 0 = sem idioma
      f->idioma[0] = (char)(((lang >> 10) & 31) + 0x60); f->idioma[1] = (char)(((lang >> 5) & 31) + 0x60);
      f->idioma[2] = (char)((lang & 31) + 0x60); f->idioma[3] = 0;
      if (!strcmp(f->idioma, "und")) f->idioma[0] = 0;
    }
  }
  if (!f->escala) return 0;
  {
    long mi; const unsigned char *minf = filho(mdia, k, "minf", &mi);
    if (!minf || !(stbl = filho(minf, mi, "stbl", &k))) return 0;
  }
  if (!(x = filho(stbl, k, "stsd", &m)) || m < 16) return 0;
  e = x + 8; ne = (long)be32(e);
  if (ne < 16 || ne > m - 8) return 0;
  memcpy(f->codec, e + 4, 4); f->codec[4] = 0;
  if (!strcmp(f->codec, "tx3g") || !strcmp(f->codec, "text")) {
    f->vazio = 2;   // so o comprimento do texto, zero
    if (!strcmp(f->codec, "tx3g") && ne >= 20 && (be32(e + 16) & 0xC0000000UL)) f->forced = 1;
  } else if (!strcmp(f->codec, "wvtt")) f->vazio = 8;   // so a caixa vtte
  else return 0;   // stpp (TTML), c608...: nao lemos
  if (!(f->stts = filho(stbl, k, "stts", &f->nStts)) || f->nStts < 8) return 0;
  if (!(f->stsz = filho(stbl, k, "stsz", &f->nStsz)) || f->nStsz < 12) return 0;
  if ((f->ctts = filho(stbl, k, "ctts", &f->nCtts)) && f->nCtts >= 8) f->vCtts = f->ctts[0];
  else f->ctts = NULL;
  // Lista de edicao: edicao vazia empurra; a primeira de midia diz de onde comeca.
  if ((x = filho(p, n, "edts", &m)) && (x = filho(x, m, "elst", &m)) && m >= 8) {
    int v1 = x[0] == 1; long cont = (long)be32(x + 4), tam = v1 ? 20 : 12, i;
    for (i = 0; i < cont && 8 + (i + 1) * tam <= m; i++) {
      const unsigned char *q = x + 8 + i * tam;
      double seg = (double)(v1 ? be64(q) : be32(q));
      long long mt = v1 ? (long long)be64(q + 8) : (long long)(int)be32(q + 4);
      if (mt == -1) { if (escalaFilme) f->desloc += seg / escalaFilme; continue; }
      f->desloc -= (double)mt / f->escala; break;
    }
  }
  return 1;
}
static int letreiroMp4(const Mp4Faixa *f) {
  static const char *const p[] = { "forced", "sign", "song", "letreiro", "forzad", "forc\xc3\xa9" };
  if (f->forced) return 1;
  for (size_t i = 0; i < sizeof p / sizeof *p; i++) if (contemSemCaixa(f->nome, p[i])) return 1;
  return 0;
}
// Falas da faixa (ordenadas). -1 = tabela inconsistente; -2 = falas demais.
static int mp4Tempos(const Mp4Faixa *f, Tempo **saida) {
  unsigned long tamFixo = be32(f->stsz + 4), n = be32(f->stsz + 8), i, e = 0, resta, ce = 0, cresta = 0;
  unsigned long nStts = be32(f->stts + 4), nCtts = f->ctts ? be32(f->ctts + 4) : 0;
  unsigned long long dts = 0; long long co = 0; int k = 0, cap = 0; Tempo *v = NULL;
  *saida = NULL;
  if (!tamFixo && (long)(12 + 4 * (unsigned long long)n) > f->nStsz) return -1;
  if ((long)(8 + 8 * (unsigned long long)nStts) > f->nStts) return -1;
  if (f->ctts && (long)(8 + 8 * (unsigned long long)nCtts) > f->nCtts) return -1;
  resta = nStts ? be32(f->stts + 8) : 0;
  if (nCtts) { cresta = be32(f->ctts + 8); co = f->vCtts ? (long long)(int)be32(f->ctts + 12) : (long long)be32(f->ctts + 12); }
  for (i = 0; i < n; i++) {
    unsigned long delta, tam = tamFixo ? tamFixo : be32(f->stsz + 12 + 4 * i);
    while (!resta && ++e < nStts) resta = be32(f->stts + 8 + 8 * e);
    if (e >= nStts) { free(v); return -1; }
    delta = be32(f->stts + 12 + 8 * e); resta--;
    if (nCtts) {
      while (!cresta && ++ce < nCtts) {
        cresta = be32(f->ctts + 8 + 8 * ce);
        co = f->vCtts ? (long long)(int)be32(f->ctts + 12 + 8 * ce) : (long long)be32(f->ctts + 12 + 8 * ce);
      }
      if (cresta) cresta--;
    }
    if (tam > (unsigned long)f->vazio && delta > 0) {
      if (k == cap) {
        Tempo *nv;
        if (cap >= LR_EVENTOS_MAX + 1) { free(v); return -2; }
        cap = cap ? cap * 2 : 256; nv = realloc(v, (size_t)cap * sizeof *v);
        if (!nv) { free(v); return -2; }
        v = nv;
      }
      v[k].inicio = ((double)dts + (double)co) / f->escala + f->desloc;
      v[k].fim = v[k].inicio + (double)delta / f->escala;
      k++;
    }
    dts += delta;
  }
  if (k) qsort(v, (size_t)k, sizeof *v, cmpTempo);
  *saida = v;
  return k;
}
static LegendaDocumento *coletarMp4(Job *j, LegRefStatus *st, const unsigned char *cab, long tam) {
  long long pos = 0; unsigned char *moov = NULL; long nMoov = 0; int i, nF = 0, escolhida = -1, k;
  Mp4Faixa fx[LR_FAIXAS]; LegendaDocumento *doc = NULL; Tempo *tv = NULL; unsigned long escalaFilme = 0;
  for (i = 0; i < LR_CAIXAS && !moov; i++) {
    unsigned char h[16], *lido = NULL; long n = 0; unsigned long long t; long hdr = 8;
    if (pos + 16 <= tam) memcpy(h, cab + pos, 16);
    else {
      lido = ler(j, pos, 16, &n);
      if (!lido) { if (j->erro == LEGREF_REDE && i > 0) j->erro = LEGREF_SEM_INDICE; return NULL; }   // fim do arquivo sem moov
      if (n < 8) { free(lido); j->erro = LEGREF_SEM_INDICE; return NULL; }
      memset(h, 0, sizeof h); memcpy(h, lido, (size_t)(n < 16 ? n : 16)); free(lido);
    }
    t = be32(h);
    if (t == 1) { t = be64(h + 8); hdr = 16; }
    if (t < (unsigned long long)hdr) { j->erro = LEGREF_SEM_INDICE; return NULL; }   // 0 = ate o fim: sem moov depois
    if (!memcmp(h + 4, "moof", 4)) { j->erro = LEGREF_SEM_INDICE; return NULL; }
    if (!memcmp(h + 4, "moov", 4)) {
      if (t - (unsigned long long)hdr > (unsigned long long)LR_MOOV_MAX) { j->erro = LEGREF_ORCAMENTO; return NULL; }
      nMoov = (long)(t - (unsigned long long)hdr);
      if (pos + (long long)t <= tam) {
        moov = malloc((size_t)nMoov + 1);
        if (!moov) { j->erro = LEGREF_MEMORIA; return NULL; }
        memcpy(moov, cab + pos + hdr, (size_t)nMoov);
      } else {
        long n2 = 0; moov = ler(j, pos + hdr, nMoov, &n2);
        if (!moov) return NULL;
        if (n2 < nMoov) { free(moov); j->erro = LEGREF_INCOMPLETO; return NULL; }
      }
      break;
    }
    pos += (long long)t;
  }
  if (!moov) { j->erro = LEGREF_SEM_INDICE; return NULL; }
  {
    Cx c = { moov, 0, nMoov }; char t[5]; const unsigned char *d; long m;
    while (caixa(&c, t, &d, &m)) {
      if (!memcmp(t, "mvex", 4)) { free(moov); j->erro = LEGREF_SEM_INDICE; return NULL; }   // fragmentado
      if (!memcmp(t, "mvhd", 4) && m >= 24) escalaFilme = be32(d + (d[0] == 1 ? 20 : 12));
    }
    c.o = 0;
    while (caixa(&c, t, &d, &m) && nF < LR_FAIXAS)
      if (!memcmp(t, "trak", 4) && mp4Faixa(d, m, escalaFilme, &fx[nF])) nF++;
  }
  {
    const char *ids[LR_FAIXAS], *nms[LR_FAIXAS]; int ok[LR_FAIXAS], cand[LR_FAIXAS], nCand, e;
    Tempo *v[8] = { 0 }; int nv, nt[8] = { 0 }, desc = 0, x;
    for (i = 0; i < nF; i++) {
      ids[i] = fx[i].idioma; nms[i] = fx[i].nome;
      ok[i] = !letreiroMp4(&fx[i]) && !comentario(fx[i].nome);
      for (e = 0; e < j->p.nExcl; e++) if (j->p.excl[e] == fx[i].numero) ok[i] = 0;
    }
    nCand = ordenar(ids, nms, ok, nF, j->p.idioma, cand);
    if (!nCand) { free(moov); j->erro = LEGREF_SEM_FAIXA; return NULL; }
    // Todas as tabelas ja estao no moov: validacao cruzada sem Range a mais.
    nv = nCand < 8 ? nCand : 8;
    for (x = 0; x < nv; x++) { nt[x] = mp4Tempos(&fx[cand[x]], &v[x]); if (nt[x] < 0) { free(v[x]); v[x] = NULL; nt[x] = 0; } }
    x = v[0] ? validar(v, nt, nv, &desc) : 0;
    if (desc) fprintf(stderr, "[legref] cross_check dropped track=%d (shifted against two agreeing tracks) using=%d\n",
                      fx[cand[0]].numero, fx[cand[x]].numero);
    escolhida = cand[x];
    for (i = 0; i < nv; i++) if (i != x) free(v[i]);
    tv = v[x]; k = v[x] ? nt[x] : mp4Tempos(&fx[escolhida], &tv);
  }
  anunciar(j, st, fx[escolhida].numero, fx[escolhida].idioma, fx[escolhida].codec);
  if (k == -2) j->erro = LEGREF_ORCAMENTO;
  else if (k < 0) j->erro = LEGREF_SEM_INDICE;
  else if (k == 0) j->erro = LEGREF_SEM_FAIXA;
  else {
    char ident[32]; snprintf(ident, sizeof ident, "mp4-track:%d", fx[escolhida].numero);
    progresso(j, k, k);
    doc = montarTempos(tv, k, fx[escolhida].idioma, ident, 0, j->p.sessao);
    if (!doc) j->erro = LEGREF_MEMORIA;
  }
  free(tv); free(moov);
  return doc;
}

static LegendaDocumento *coletar(Job *j, LegRefStatus *st) {
  long tam = 0, nTr = 0, oTr = 0, nCu = 0, oCu = 0;
  unsigned char *cab = NULL, *tracks = NULL, *cues = NULL, *bufTr;
  long long segData = -1, posTracks = -1, posCues = -1, posInfo = -1;
  double escala = 1e-6;   // TimestampScale padrao (1 ms) em segundos
  Faixa fx[LR_FAIXAS]; int nF = 0, escolhida = -1, i, semRel = 0, nP, cand[LR_FAIXAS], nCand = 0;
  LegRefPonto *pts = NULL; Evento *ev = NULL; int nEv = 0;
  LegendaDocumento *doc = NULL; int ass;
  // 1. Cabeca do arquivo. O primeiro pedido segue redirects (debrid) e guarda
  // o endereco final: Range e cabecalho do dono e nao atravessa origem num
  // redirect, entao os seguintes vao direto ao destino.
  cab = ler(j, 0, LR_CABECA, &tam);
  if (!cab && j->erro == LEGREF_SEM_RANGE && strcmp(j->urlFinal, j->p.url)) cab = ler(j, 0, LR_CABECA, &tam);
  if (!cab) goto fim;
  if (tam >= 8 && (!memcmp(cab + 4, "ftyp", 4) || !memcmp(cab + 4, "moov", 4) || !memcmp(cab + 4, "mdat", 4) ||
                   !memcmp(cab + 4, "free", 4) || !memcmp(cab + 4, "wide", 4) || !memcmp(cab + 4, "skip", 4))) {
    doc = coletarMp4(j, st, cab, tam); goto fim;
  }
  if (tam < 4 || cab[0] != 0x1A || cab[1] != 0x45 || cab[2] != 0xDF || cab[3] != 0xA3) { j->erro = LEGREF_NAO_MKV; goto fim; }
  {
    unsigned long id; long long t; long o = 0; int a, b;
    a = lerId(cab, tam, &id); b = a ? lerTam(cab + a, tam - a, &t) : 0;
    if (!a || !b || t < 0) { j->erro = LEGREF_NAO_MKV; goto fim; }
    o = a + b + (long)t;
    a = lerId(cab + o, tam - o, &id); b = a ? lerTam(cab + o + a, tam - o - a, &t) : 0;
    if (!a || !b || id != 0x18538067) { j->erro = LEGREF_NAO_MKV; goto fim; }
    segData = o + a + b;
    o = (long)segData;
    for (;;) {
      long long ts; long hdr, ini = o;
      a = lerId(cab + o, tam - o, &id); if (!a) break;
      b = lerTam(cab + o + a, tam - o - a, &ts); if (!b) break;
      hdr = a + b;
      if (id == 0x1F43B675 || ts < 0) break;   // primeiro Cluster: o resto vem pelo SeekHead
      if (ini + hdr + ts > tam) {              // elemento cortado pela cabeca
        if (id == 0x1654AE6B && posTracks < 0) posTracks = ini - segData;
        break;
      }
      if (id == 0x114D9B74) {
        It sk = { cab + ini + hdr, 0, (long)ts }, e; unsigned long id2, id3; const unsigned char *d2, *d3; long t2, t3;
        while (prox(&sk, &id2, &d2, &t2)) {
          unsigned long alvo = 0; long long pos = -1;
          if (id2 != 0x4DBB) continue;
          e = (It){ d2, 0, t2 };
          while (prox(&e, &id3, &d3, &t3)) {
            if (id3 == 0x53AB) alvo = (unsigned long)lerUint(d3, t3);
            else if (id3 == 0x53AC) pos = (long long)lerUint(d3, t3);
          }
          if (alvo == 0x1654AE6B && posTracks < 0) posTracks = pos;
          else if (alvo == 0x1C53BB6B && posCues < 0) posCues = pos;
          else if (alvo == 0x1549A966 && posInfo < 0) posInfo = pos;
        }
      } else if (id == 0x1549A966) {
        It in = { cab + ini + hdr, 0, (long)ts }; unsigned long id2; const unsigned char *d2; long t2;
        while (prox(&in, &id2, &d2, &t2)) if (id2 == 0x2AD7B1) { unsigned long long v = lerUint(d2, t2); if (v) escala = (double)v / 1e9; }
        posInfo = ini - segData;
      } else if (id == 0x1654AE6B) {
        nF = lerTracks(cab + ini + hdr, (long)ts, fx, LR_FAIXAS);
        bufTr = cab; (void)bufTr;
        posTracks = ini - segData;
      }
      o = (long)(ini + hdr + ts);
    }
  }
  if (!nF && posTracks >= 0) {
    tracks = lerElemento(j, segData + posTracks, 0x1654AE6B, LR_TRACKS_MAX, &oTr, &nTr);
    if (!tracks) goto fim;
    nF = lerTracks(tracks + oTr, nTr, fx, LR_FAIXAS);
  }
  (void)posInfo;
  // 2. As faixas candidatas: texto, nao letreiro, nao comentario, nao
  // excluidas; em ordem de preferencia (ordenar).
  {
    const char *ids[LR_FAIXAS], *nms[LR_FAIXAS]; int ok[LR_FAIXAS];
    for (i = 0; i < nF; i++) {
      int e;
      ids[i] = fx[i].idioma; nms[i] = fx[i].nome;
      ok[i] = textoFaixa(&fx[i]) && !letreiro(&fx[i]) && !comentario(fx[i].nome);
      for (e = 0; e < j->p.nExcl; e++) if (j->p.excl[e] == fx[i].numero) ok[i] = 0;
    }
    nCand = ordenar(ids, nms, ok, nF, j->p.idioma, cand);
  }
  if (!nCand) { j->erro = LEGREF_SEM_FAIXA; goto fim; }
  escolhida = cand[0];
  anunciar(j, st, fx[escolhida].numero, fx[escolhida].idioma, fx[escolhida].codec);
  // 3. O indice.
  if (posCues < 0) { j->erro = LEGREF_SEM_INDICE; goto fim; }
  cues = lerElemento(j, segData + posCues, 0x1C53BB6B, LR_CUES_MAX, &oCu, &nCu);
  if (!cues) goto fim;
  // 3b. Validacao cruzada: o mesmo Cues traz os tempos das outras faixas de
  // texto (com CueDuration). Custa zero Range a mais.
  if (nCand > 1) {
    Tempo *tv[8] = { 0 }; int nt[8] = { 0 }, nv = nCand < 8 ? nCand : 8, desc = 0, x;
    for (x = 0; x < nv; x++) {
      LegRefPonto *pp = NULL; int sr = 0, np = legref_cues(cues + oCu, nCu, fx[cand[x]].numero, escala, &pp, &sr), todos = np > 0 && !sr;
      for (i = 0; i < np && todos; i++) if (!(pp[i].dur > 0)) todos = 0;
      if (todos && (tv[x] = malloc((size_t)np * sizeof **tv))) {
        for (i = 0; i < np; i++) { tv[x][i].inicio = pp[i].inicio; tv[x][i].fim = pp[i].inicio + pp[i].dur; }
        qsort(tv[x], (size_t)np, sizeof **tv, cmpTempo); nt[x] = np;
      }
      free(pp);
    }
    if (tv[0]) {
      x = validar(tv, nt, nv, &desc);
      if (desc) {
        fprintf(stderr, "[legref] cross_check dropped track=%d (shifted against two agreeing tracks) using=%d\n",
                fx[cand[0]].numero, fx[cand[x]].numero);
        escolhida = cand[x];
        anunciar(j, st, fx[escolhida].numero, fx[escolhida].idioma, fx[escolhida].codec);
      }
    }
    for (x = 0; x < nv; x++) free(tv[x]);
  }
  ass = strcmp(fx[escolhida].codec, "S_TEXT/UTF8") != 0;
  nP = legref_cues(cues + oCu, nCu, fx[escolhida].numero, escala, &pts, &semRel);
  if (nP < 0) { j->erro = LEGREF_ORCAMENTO; goto fim; }
  // Um CuePoint sem posicao relativa e um bloco que nao sabemos buscar: a
  // faixa nao sai inteira, entao nao serve.
  if (semRel || nP < 1) { j->erro = LEGREF_SEM_INDICE; goto fim; }
  // SRT com CueDuration em todo ponto (ffmpeg e mkvmerge gravam): o indice
  // JA TEM os tempos. Um Range em vez de um por fala (um longa: ~1300
  // Ranges, minutos no ritmo da TV). ASS continua lendo os blocos: la o
  // texto separa letreiro posicionado de dialogo.
  if (!ass) {
    int todos = 1;
    for (i = 0; i < nP; i++) if (!(pts[i].dur > 0)) todos = 0;
    if (todos) {
      Tempo *tv = malloc((size_t)nP * sizeof *tv); char ident[32];
      if (!tv) { j->erro = LEGREF_MEMORIA; goto fim; }
      for (i = 0; i < nP; i++) { tv[i].inicio = pts[i].inicio; tv[i].fim = pts[i].inicio + pts[i].dur; }
      qsort(tv, (size_t)nP, sizeof *tv, cmpTempo);
      snprintf(ident, sizeof ident, "mkv-track:%d", fx[escolhida].numero);
      progresso(j, nP, nP);
      doc = montarTempos(tv, nP, fx[escolhida].idioma, ident, fx[escolhida].forced, j->p.sessao);
      free(tv);
      if (!doc) j->erro = LEGREF_MEMORIA;
      goto fim;
    }
  }
  qsort(pts, (size_t)nP, sizeof *pts, cmpPonto);
  ev = calloc((size_t)nP, sizeof *ev);
  if (!ev) { j->erro = LEGREF_MEMORIA; goto fim; }
  progresso(j, 0, nP);
  // 4. Os blocos, em ordem de byte, juntando vizinhos.
  {
    long long clPos = -1; int clH = 0; double clBase = 0; int temBase = 0;
    for (i = 0; i < nP;) {
      long long ini = segData + pts[i].pos + 5 + pts[i].rel, fimL = segData + pts[i].pos + 12 + pts[i].rel + LR_BLOCO;
      int g = i + 1; unsigned char *buf; long n = 0;
      while (g < nP) {
        long long a2 = segData + pts[g].pos + 5 + pts[g].rel, b2 = a2 + 7 + LR_BLOCO;
        if (a2 > fimL + LR_VAO || b2 - ini > LR_LEITURA_MAX) break;
        if (b2 > fimL) fimL = b2;
        g++;
      }
      buf = ler(j, ini, (long)(fimL - ini), &n);
      if (!buf) goto fim;
      for (; i < g; i++) {
        long long cl = segData + pts[i].pos;
        int h, ok = 0, hIni = 5, hFim = 12;
        if (cl == clPos && clH) hIni = hFim = clH;
        for (h = hIni; h <= hFim && !ok; h++) {
          long long abs = cl + h + pts[i].rel; long off = (long)(abs - ini), precisa = 0, nTxt = 0;
          int rel16 = 0, r; double dur = 0; const unsigned char *txt = NULL;
          unsigned char *extra = NULL;
          if (off < 0 || off >= n) continue;
          r = lerGrupo(buf + off, n - off, fx[escolhida].numero, escala, pts[i].dur, &rel16, &dur, &txt, &nTxt, &precisa);
          if (r == -1) {   // grupo maior que a janela: le exato, uma vez
            long n2 = 0;
            extra = ler(j, abs, precisa, &n2);
            if (!extra) { free(buf); goto fim; }
            r = lerGrupo(extra, n2, fx[escolhida].numero, escala, pts[i].dur, &rel16, &dur, &txt, &nTxt, &precisa);
            if (r == -1) r = 0;
          }
          if (r == -2) { free(extra); free(buf); j->erro = LEGREF_SEM_DURACAO; goto fim; }
          if (r == 1) {
            // Mesmo Cluster, mesma base: tempo do indice - relativo do bloco.
            double base = pts[i].inicio - rel16 * escala;
            if (cl == clPos && temBase && (base - clBase > 0.002 || clBase - base > 0.002)) { free(extra); continue; }
            ev[nEv].inicio = pts[i].inicio; ev[nEv].fim = pts[i].inicio + dur;
            ev[nEv].texto = malloc((size_t)nTxt + 1);
            if (!ev[nEv].texto) { free(extra); free(buf); j->erro = LEGREF_MEMORIA; goto fim; }
            memcpy(ev[nEv].texto, txt, (size_t)nTxt); ev[nEv].texto[nTxt] = 0;
            { char *s = ev[nEv].texto; size_t m = strlen(s);   // NUL no meio corta; CR some
              while (m && (s[m - 1] == '\r' || s[m - 1] == '\n')) s[--m] = 0;
              if (ass) for (; *s; s++) if (*s == '\r' || *s == '\n') *s = ' '; }
            nEv++; ok = 1;
            if (cl != clPos) { clPos = cl; temBase = 1; clBase = base; }
            clH = h;
          }
          free(extra);
        }
        if (!ok) { free(buf); j->erro = LEGREF_INCOMPLETO; goto fim; }
        if (!(nEv % 16)) progresso(j, nEv, nP);
      }
      free(buf);
    }
  }
  progresso(j, nEv, nP);
  doc = montar(&fx[escolhida], ev, nEv, j->p.sessao, ass);
  if (!doc) j->erro = LEGREF_MEMORIA;
  else if (!(legenda_documento_info(doc)->flags & LEGENDA_DOC_COMPLETO)) {
    legenda_documento_liberar(doc); doc = NULL; j->erro = LEGREF_INCOMPLETO;
  }
fim:
  for (i = 0; i < nEv; i++) free(ev[i].texto);
  free(ev); free(pts); free(cues); free(tracks); free(cab);
  return doc;
}

static void *trabalhar(void *u) {
  LegRef *r = u;
  for (;;) {
    Job j; LegendaDocumento *doc; LegRefStatus st;
    pthread_mutex_lock(&r->m);
    while (!r->sair && !r->temPedido) pthread_cond_wait(&r->c, &r->m);
    if (r->sair) { pthread_mutex_unlock(&r->m); return NULL; }
    memset(&j, 0, sizeof j);
    j.r = r; j.id = r->ped.id; j.p = r->ped; r->temPedido = 0;
    snprintf(j.urlFinal, sizeof j.urlFinal, "%s", j.p.url);
    pthread_mutex_unlock(&r->m);
    memset(&st, 0, sizeof st);
    doc = j.p.local ? j.p.local(j.p.url, j.p.sessao, j.p.excl, j.p.nExcl) : NULL;
    if (doc) {
      const LegendaDocumentoInfo *info = legenda_documento_info(doc);
      int n = 0, faixa = 0; legenda_documento_dados(doc, &n);
      sscanf(info->identidade, "mkv-track:%d", &faixa);
      anunciar(&j, &st, faixa, info->idioma, "S_TEXT/ASS");
      progresso(&j, n, n);
    } else doc = coletar(&j, &st);
    pthread_mutex_lock(&r->m);
    if (r->st.pedido == j.id && r->ped.id == j.id && !r->sair) {
      r->st.bytes = j.bytes; r->st.pedidos = j.pedidos;
      if (doc) { r->st.fase = LEGREF_PRONTO; r->st.motivo = LEGREF_OK; r->doc = doc; doc = NULL; }
      else {
        r->st.fase = j.erro == LEGREF_PARADO ? LEGREF_CANCELADO : LEGREF_INDISPONIVEL;
        r->st.motivo = j.erro;
      }
      fprintf(stderr, "[legref] reference %s reason=%s track=%d events=%d/%d requests=%d bytes=%lld\n",
              r->st.fase == LEGREF_PRONTO ?
                (legenda_documento_info(r->doc)->flags & LEGENDA_DOC_JANELA ? "window" : "complete") : "unavailable",
              legref_motivo(r->st.motivo),
              r->st.faixa, r->st.feitos, r->st.total, j.pedidos, j.bytes);
    }
    pthread_mutex_unlock(&r->m);
    legenda_documento_liberar(doc);   // pedido velho: descarta fora do lock
  }
}

// --- HTTP -------------------------------------------------------------------------
static unsigned char *lerHttp(void *u, const char *url, long long ini, long n, long *tam,
                              int *status, int (*parar)(void *), void *pu) {
  char range[80], linhas[2048]; const char *cab[18]; RedePedido p; RedeResposta r; unsigned char *b = NULL;
  int k = 0; char *s, *nl;
  (void)u;
  snprintf(range, sizeof range, "Range: bytes=%lld-%lld", ini, ini + n - 1);
  cab[k++] = range;
  // Os mesmos cabecalhos com que o player abre a fonte (proxyHeaders do addon).
  snprintf(linhas, sizeof linhas, "%s", pu ? ((Job *)pu)->p.cabs : "");
  for (s = linhas; *s && k < 17; s = nl) {
    nl = strchr(s, '\n');
    if (nl) *nl++ = 0; else nl = s + strlen(s);
    if (*s && strchr(s, ':') && strncasecmp(s, "Range:", 6)) cab[k++] = s;
  }
  cab[k] = NULL;
  memset(&p, 0, sizeof p);
  p.url = url; p.cabecalhos = cab; p.seguir = 1; p.rebaixar_midia = 1;
  p.prazo_ms = 20000u + (unsigned)(n / 64);
  p.max_bytes = (size_t)n;
  p.parar = parar; p.parar_usuario = pu;
  *status = 0; *tam = 0;
  rede_pedir(&p, &r);
  *status = r.status;
  if (r.erro == REDE_OK && r.status == 206 && r.corpo) {
    b = (unsigned char *)r.corpo; r.corpo = NULL; *tam = (long)r.n_corpo;
    // Endereco final para os proximos Ranges (o Job guarda; nunca vai a log).
    if (r.final[0] && pu) { Job *j = pu; snprintf(j->urlFinal, sizeof j->urlFinal, "%s", r.final); }
  } else if (r.erro == REDE_CANCELADO) {
    if (pu) ((Job *)pu)->erro = LEGREF_PARADO;
    fprintf(stderr, "[legref] range cancelled reason=caller_cancelled http=%d rede_error=%d curl_error=%d request_headers=%d\n",
            r.status, (int)r.erro, r.curl_erro, k - 1);
  } else if (r.status != 200)
    fprintf(stderr, "[legref] range failed reason=%s http=%d rede_error=%d curl_error=%d request_headers=%d\n",
            r.erro == REDE_PRAZO ? "deadline" : r.erro == REDE_LIMITE_CORPO ? "body_limit" :
            r.erro == REDE_LIMITE_CABECALHOS ? "header_limit" : r.erro == REDE_OK ? "invalid_range_response" : "request_failed",
            r.status, (int)r.erro, r.curl_erro, k - 1);
  if (!b && r.status == 200 && r.final[0] && pu) {
    Job *j = pu; snprintf(j->urlFinal, sizeof j->urlFinal, "%s", r.final);
  }
  rede_resposta_limpar(&r);
  return b;
}

int legref_disponivel(void) {
  // Samsung: desligado ate provar em TV. -DNV_LEGREF_TPK liga no .tpk 6+
  // (canario; o 4/5 fica fora: rede sem TLS por fio, outro carregador).
#if defined(__EMSCRIPTEN__) || defined(NV_TPK40) || (defined(NV_TPK) && !defined(NV_LEGREF_TPK))
  return 0;
#else
  return (rede_pedido_capacidades() & REDE_CAP_JOB) != 0;
#endif
}

LegRef *legref_criar(LegRefLer ler, void *u) {
  LegRef *r = calloc(1, sizeof *r);
  if (!r) return NULL;
  r->ler = ler ? ler : lerHttp; r->lerU = u;
  if (pthread_mutex_init(&r->m, NULL)) { free(r); return NULL; }
  if (pthread_cond_init(&r->c, NULL)) { pthread_mutex_destroy(&r->m); free(r); return NULL; }
  if (pthread_create(&r->fio, NULL, trabalhar, r)) {
    pthread_cond_destroy(&r->c); pthread_mutex_destroy(&r->m); free(r); return NULL;
  }
  r->fioVivo = 1;
  return r;
}

void legref_destruir(LegRef *r) {
  if (!r) return;
  pthread_mutex_lock(&r->m); r->sair = 1; pthread_cond_broadcast(&r->c); pthread_mutex_unlock(&r->m);
  if (r->fioVivo) pthread_join(r->fio, NULL);
  legenda_documento_liberar(r->doc);
  pthread_cond_destroy(&r->c); pthread_mutex_destroy(&r->m); free(r);
}

static const char *(*fonteCabs)(void);
static LegendaDocumento *(*fonteLocal)(const char *, uint64_t, const int *, int);
void legref_fonte_local(LegendaDocumento *(*f)(const char *, uint64_t, const int *, int)) { fonteLocal = f; }
void legref_fonte_cabecalhos(const char *(*f)(void)) { fonteCabs = f; }

uint64_t legref_pedir(LegRef *r, const char *url, uint64_t sessao, const char *idioma,
                      const int *excluidas, int nExcluidas, const LegRefOrcamento *orc) {
  LegendaDocumento *velho; uint64_t id; int i;
  if (!r || !url || !*url || strlen(url) >= sizeof r->ped.url || !orc ||
      orc->maxBytes <= 0 || orc->maxPedidos <= 0) return 0;
  pthread_mutex_lock(&r->m);
  velho = r->doc; r->doc = NULL;
  id = ++r->gerador;
  memset(&r->ped, 0, sizeof r->ped);
  r->ped.id = id; r->ped.sessao = sessao; r->ped.orc = *orc; r->ped.local = fonteLocal;
  snprintf(r->ped.url, sizeof r->ped.url, "%s", url);
  snprintf(r->ped.idioma, sizeof r->ped.idioma, "%s", idioma ? idioma : "");
  snprintf(r->ped.cabs, sizeof r->ped.cabs, "%s", fonteCabs && fonteCabs() ? fonteCabs() : "");
  for (i = 0; i < nExcluidas && i < 16; i++) r->ped.excl[i] = excluidas[i];
  r->ped.nExcl = i;
  r->temPedido = 1;
  memset(&r->st, 0, sizeof r->st);
  r->st.fase = LEGREF_LENDO; r->st.pedido = id;
  pthread_cond_broadcast(&r->c);
  pthread_mutex_unlock(&r->m);
  legenda_documento_liberar(velho);
  return id;
}

void legref_cancelar(LegRef *r) {
  LegendaDocumento *velho;
  if (!r) return;
  pthread_mutex_lock(&r->m);
  velho = r->doc; r->doc = NULL;
  r->ped.id = 0; r->temPedido = 0;
  if (r->st.fase == LEGREF_LENDO) { r->st.fase = LEGREF_CANCELADO; r->st.motivo = LEGREF_PARADO; }
  pthread_cond_broadcast(&r->c);
  pthread_mutex_unlock(&r->m);
  legenda_documento_liberar(velho);
}

void legref_pausar(LegRef *r, int pausar) {
  if (!r) return;
  pthread_mutex_lock(&r->m); r->pausado = pausar != 0; pthread_cond_broadcast(&r->c); pthread_mutex_unlock(&r->m);
}

LegRefStatus legref_status(LegRef *r) {
  LegRefStatus s; memset(&s, 0, sizeof s);
  if (!r) { s.fase = LEGREF_INDISPONIVEL; s.motivo = LEGREF_PLATAFORMA; return s; }
  pthread_mutex_lock(&r->m); s = r->st; pthread_mutex_unlock(&r->m);
  return s;
}

LegendaDocumento *legref_tomar(LegRef *r, uint64_t pedido) {
  LegendaDocumento *d = NULL;
  if (!r || !pedido) return NULL;
  pthread_mutex_lock(&r->m);
  if (r->st.pedido == pedido && r->doc) { d = r->doc; r->doc = NULL; }
  pthread_mutex_unlock(&r->m);
  return d;
}

const char *legref_motivo(LegRefMotivo m) {
  static const char *const n[] = { "ok", "platform", "no_range", "not_matroska", "no_text_track",
    "no_index", "no_block_duration", "network", "budget", "incomplete", "out_of_memory", "stopped" };
  return m >= 0 && m < (int)(sizeof n / sizeof *n) ? n[m] : "invalid";
}
