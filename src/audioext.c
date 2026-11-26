#if !defined(_POSIX_C_SOURCE) && !defined(_GNU_SOURCE)
#define _POSIX_C_SOURCE 200809L   /* clock_gettime (tests build with -std=c11) */
#endif
// Audio from another version of the title: the marked source (audioext.h).
#include "audioext.h"
#include "mediaurl.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char alvoAtual[64], alvoMarca[64];
static char urlMarca[NV_MEDIA_URL_MAX], cabMarca[512], nomeMarca[192];
static int offsetMs, tocando;
static unsigned offsetGeracao;
static int offsetSuave;      // the last change came from a check while playing
static double posAtual = -1; // playback position (audioext_posicao), -1 unknown
static char falha[160], idiomaMarca[8];
// Fallback copies of the marked file (audioext_reserva).
static char resUrl[3][NV_MEDIA_URL_MAX], resCab[3][512];
static int resN, resProx;
// The debrid service's placeholder clip on a version said to be cached: the
// file is being prepared (TorBox adds it to the account on the first open).
// The chosen link (urlPrim) is tried again after AUDIOEXT_ESPERA_MS, up to
// `tentativas` times; then every link tried goes to the "not ready" list.
static char urlPrim[NV_MEDIA_URL_MAX], cabPrim[512];
static int tentativas, tentativa;
static uint64_t retryEm;          // ms (relogio()), 0 none
static int preparandoAviso;       // the try to tell the player (consumed)
static char negadas[4][NV_MEDIA_URL_MAX];
static int negadasN;
static uint64_t relogioPadrao(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000u + (uint64_t)(t.tv_nsec / 1000000);
}
static uint64_t (*relogio)(void) = relogioPadrao;
void audioext_definir_relogio(uint64_t (*fn)(void)) { relogio = fn ? fn : relogioPadrao; }
static unsigned pedidoGeracao;
static unsigned autoGeracao;
static int prefSoAudio, prefSerie = 1;
// SAFETY NET (owner, 07/11): a start with this mark began and the other
// version's audio never played (it failed, hung, or the person backed out
// while it loaded). The next start of the title then plays the file's own
// audio instead of trying the same thing again.
static int pendente, voltouPadrao;
static int avisoCorte, avisoVelocidade, avisoPendente;   // other cut / speed, once per start
static int carregando;   // the other version's audio is being opened (player card)
// "Match automatically": requested (for the player), working, and the outcome
// for one toast: 1 found (offset applied), -1 not trusted.
static int dopasujPedido, dopasujAtivo, dopasujAviso;
static AudioExtDopasuj dopasujInfo;
// The other version's speed from a match (ppm; 0 the same), with the offset.
static int velocidadePpm;
static int kandN, kandMs[3], kandPct[3];   // the last match's best places
// The donor's audio stream picked in the menu (-1 = by language), for the mark.
static int faixaMarca = -1;
// The marked version came from another episode's source list (the two
// releases number the episodes differently, owner's log 21/11, Farscape):
// that episode's number, 0 = this episode. Nothing of it goes to the series
// memory: the next episode keeps its own numbering.
static int epOutro;
// The series being used now: a working copy of its row in serTab below.
static struct { char serie[64], episodio[64]; AudioExtIdent id; int offset, velocidade, ativo, temFaixa; AudioExtFaixa faixa; } memo;
// SAVED SERIES MEMORY (owner 24/11): one row per series, account and profile,
// AUDIOEXT_SERIE_MAX rows (the one used longest ago leaves first), in
// audioextserie.txt through the functions of audioext_definir_memoria_arquivo.
// No time limit: a row goes when the person goes back to the file's own audio
// (audioext_original) or another version is remembered for that series.
// Without the functions (tests) the rows live in memory only.
typedef struct {
  char serie[64], episodio[64], conta[96];
  int perfil;
  unsigned usado;   // bigger = used more recently (a counter, saved with the row)
  AudioExtIdent id;
  int offset, velocidade, temFaixa;
  AudioExtFaixa faixa;
} SerieMem;
static SerieMem serTab[AUDIOEXT_SERIE_MAX];
static int serN, serCarregado, serSujo;
static uint64_t serSujoEm;
static char donoConta[96];
static int donoPerfil;
static char *(*serLer)(const char *);
static int (*serGravar)(const char *, const char *);
static int (*serApagar)(const char *);
#define SERIE_ARQUIVO "audioextserie.txt"
#define SERIE_CABECALHO "# nuvio audio serie v1\n"
// This episode came from the series memory with a remembered track (owner
// 09/11: two English tracks, the one chosen): the hint for the player.
static int dicaAtiva;
static AudioExtFaixa dica;
// The offset came from the series memory (same version as the last episode):
// an automatic match at start that is not sure keeps it quietly.
static int offsetLembrado;
static void serieDe(const char *alvo, char *out, size_t tam) {
  const char *c = alvo ? strchr(alvo, ':') : NULL;
  snprintf(out, tam, "%.*s", c ? (int)(c - alvo) : 0, c ? alvo : "");
}


// --- saved series memory ------------------------------------------------------
static size_t serEsc(char *d, size_t cap, const char *src) {
  size_t k = 0;
  for (; src && *src && k + 2 < cap; src++) {
    char c = *src;
    if (c == '\t' || c == '\n' || c == '\\' || c == '\r') {
      d[k++] = '\\'; d[k++] = c == '\t' ? 't' : c == '\n' ? 'n' : c == '\r' ? 'r' : '\\';
    } else d[k++] = c;
  }
  d[k] = 0;
  return k;
}
static void serDesesc(char *d, size_t cap, const char *src) {
  size_t k = 0;
  for (; *src && k + 1 < cap; src++) {
    if (*src == '\\' && src[1]) {
      src++;
      d[k++] = *src == 't' ? '\t' : *src == 'n' ? '\n' : *src == 'r' ? '\r' : *src;
    } else d[k++] = *src;
  }
  d[k] = 0;
}
static char *serCampo(char **p) {
  char *ini = *p, *t;
  if (!ini) return (char *)"";
  t = strchr(ini, '\t');
  if (t) { *t = 0; *p = t + 1; } else *p = NULL;
  return ini;
}
static int serDono(const SerieMem *e) { return e->perfil == donoPerfil && !strcmp(e->conta, donoConta); }
static int serAchar(const char *serie) {
  for (int i = 0; i < serN; i++)
    if (serDono(&serTab[i]) && !strcmp(serTab[i].serie, serie)) return i;
  return -1;
}
static void serCarregar(void) {
  char *b, *linha, *prox;
  if (serCarregado || !serLer) return;
  serCarregado = 1;
  b = serLer(SERIE_ARQUIVO);
  if (!b) return;
  if (strncmp(b, SERIE_CABECALHO, sizeof SERIE_CABECALHO - 1)) { free(b); return; }
  for (linha = b; linha && *linha && serN < AUDIOEXT_SERIE_MAX; linha = prox) {
    char *fim = strchr(linha, '\n'), *p = linha;
    SerieMem e;
    prox = fim ? fim + 1 : NULL;
    if (fim) *fim = 0;
    if (linha[0] == '#' || !linha[0]) continue;
    memset(&e, 0, sizeof e);
    serDesesc(e.serie, sizeof e.serie, serCampo(&p));
    serDesesc(e.episodio, sizeof e.episodio, serCampo(&p));
    serDesesc(e.conta, sizeof e.conta, serCampo(&p));
    e.perfil = atoi(serCampo(&p));
    e.usado = (unsigned)strtoul(serCampo(&p), NULL, 10);
    serDesesc(e.id.grupo, sizeof e.id.grupo, serCampo(&p));
    serDesesc(e.id.addon, sizeof e.id.addon, serCampo(&p));
    serDesesc(e.id.idioma, sizeof e.id.idioma, serCampo(&p));
    serDesesc(e.id.nome, sizeof e.id.nome, serCampo(&p));
    e.id.altura = atoi(serCampo(&p));
    e.id.codec = atoi(serCampo(&p));
    e.offset = atoi(serCampo(&p));
    e.velocidade = atoi(serCampo(&p));
    e.temFaixa = atoi(serCampo(&p)) != 0;
    e.faixa.stream = atoi(serCampo(&p));
    e.faixa.canais = atoi(serCampo(&p));
    serDesesc(e.faixa.codec, sizeof e.faixa.codec, serCampo(&p));
    serDesesc(e.faixa.idioma, sizeof e.faixa.idioma, serCampo(&p));
    serDesesc(e.faixa.titulo, sizeof e.faixa.titulo, serCampo(&p));
    if (!e.serie[0]) continue;
    if (e.offset > AUDIOEXT_OFFSET_MAX) e.offset = AUDIOEXT_OFFSET_MAX;
    if (e.offset < -AUDIOEXT_OFFSET_MAX) e.offset = -AUDIOEXT_OFFSET_MAX;
    { int dup = 0;
      for (int i = 0; i < serN; i++)
        if (serTab[i].perfil == e.perfil && !strcmp(serTab[i].conta, e.conta) && !strcmp(serTab[i].serie, e.serie)) dup = 1;
      if (!dup) serTab[serN++] = e; }
  }
  free(b);
  printf("[ext-audio] series memory: %d saved series read\n", serN); fflush(stdout);
}
static void serMarcarSujo(void) {
  if (!serSujo) serSujoEm = relogio();
  serSujo = 1;
}
static void serGravarAgora(void) {
  size_t cap = sizeof SERIE_CABECALHO + (size_t)serN * 2000, k;
  char *b;
  serSujo = 0;
  if (!serGravar) return;
  if (!serN) { if (serApagar) serApagar(SERIE_ARQUIVO); return; }
  b = malloc(cap);
  if (!b) return;
  k = (size_t)snprintf(b, cap, "%s", SERIE_CABECALHO);
  for (int i = 0; i < serN && k < cap; i++) {
    const SerieMem *e = &serTab[i];
    char t[6][300];
    serEsc(t[0], sizeof t[0], e->serie); serEsc(t[1], sizeof t[1], e->episodio);
    serEsc(t[2], sizeof t[2], e->conta); serEsc(t[3], sizeof t[3], e->id.grupo);
    serEsc(t[4], sizeof t[4], e->id.addon); serEsc(t[5], sizeof t[5], e->id.nome);
    { char il[20], fc[50], fi[34], ft[130];
      serEsc(il, sizeof il, e->id.idioma); serEsc(fc, sizeof fc, e->faixa.codec);
      serEsc(fi, sizeof fi, e->faixa.idioma); serEsc(ft, sizeof ft, e->faixa.titulo);
      int n = snprintf(b + k, cap - k, "%s\t%s\t%s\t%d\t%u\t%s\t%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n",
                       t[0], t[1], t[2], e->perfil, e->usado, t[3], t[4], il, t[5], e->id.altura, e->id.codec,
                       e->offset, e->velocidade, e->temFaixa, e->faixa.stream, e->faixa.canais, fc, fi, ft);
      if (n < 0) break;
      k += (size_t)n; }
  }
  if (k < cap) serGravar(SERIE_ARQUIVO, b);
  free(b);
}
// The working copy goes to its row (a new row for a new series: the full table
// drops the row used longest ago).
static void memoSalvar(void) {
  int i, maxU = 0;
  if (!memo.ativo || !memo.serie[0]) return;
  serCarregar();
  for (int j = 0; j < serN; j++) if ((int)serTab[j].usado > maxU) maxU = (int)serTab[j].usado;
  i = serAchar(memo.serie);
  if (i < 0) {
    if (serN < AUDIOEXT_SERIE_MAX) i = serN++;
    else {
      i = 0;
      for (int j = 1; j < serN; j++) if (serTab[j].usado < serTab[i].usado) i = j;
      printf("[ext-audio] series memory: full (%d), %s leaves\n", AUDIOEXT_SERIE_MAX, serTab[i].serie); fflush(stdout);
    }
  }
  memset(&serTab[i], 0, sizeof serTab[i]);
  snprintf(serTab[i].serie, sizeof serTab[i].serie, "%s", memo.serie);
  snprintf(serTab[i].episodio, sizeof serTab[i].episodio, "%s", memo.episodio);
  snprintf(serTab[i].conta, sizeof serTab[i].conta, "%s", donoConta);
  serTab[i].perfil = donoPerfil;
  serTab[i].usado = (unsigned)maxU + 1;
  serTab[i].id = memo.id;
  serTab[i].offset = memo.offset; serTab[i].velocidade = memo.velocidade;
  serTab[i].temFaixa = memo.temFaixa; serTab[i].faixa = memo.faixa;
  serMarcarSujo();
}
// The series' row (this account and profile) becomes the working copy.
static int memoCarregar(const char *serie) {
  int i;
  if (memo.ativo && !strcmp(memo.serie, serie)) return 1;
  serCarregar();
  i = serAchar(serie);
  if (i < 0) return 0;
  memset(&memo, 0, sizeof memo);
  snprintf(memo.serie, sizeof memo.serie, "%s", serTab[i].serie);
  snprintf(memo.episodio, sizeof memo.episodio, "%s", serTab[i].episodio);
  memo.id = serTab[i].id;
  memo.offset = serTab[i].offset; memo.velocidade = serTab[i].velocidade;
  memo.temFaixa = serTab[i].temFaixa; memo.faixa = serTab[i].faixa;
  memo.ativo = 1;
  return 1;
}
void audioext_definir_memoria_arquivo(char *(*ler)(const char *), int (*gravar)(const char *, const char *),
                                      int (*apagar)(const char *)) {
  serLer = ler; serGravar = gravar; serApagar = apagar;
  // The table comes from this file from now on (the app sets it once, at start).
  serCarregado = 0; serN = 0; serSujo = 0; memo.ativo = 0;
}
void audioext_memoria_dono(const char *conta, int perfil) {
  if (!conta) conta = "";
  if (perfil == donoPerfil && !strcmp(conta, donoConta)) return;
  snprintf(donoConta, sizeof donoConta, "%s", conta);
  donoPerfil = perfil;
  memo.ativo = 0;   // another person's series: read from the table on demand
}
void audioext_memoria_descarregar(void) { if (serSujo) serGravarAgora(); }
int audioext_memoria_series(void) { serCarregar(); return serN; }

void audioext_limpar(void) {
  resN = resProx = 0;
  tentativas = tentativa = 0; retryEm = 0; preparandoAviso = 0;
  urlMarca[0] = cabMarca[0] = nomeMarca[0] = alvoMarca[0] = idiomaMarca[0] = 0;
  offsetMs = 0; offsetGeracao++; offsetSuave = 0; posAtual = -1; velocidadePpm = 0; kandN = 0;
  pendente = 0; faixaMarca = -1; carregando = 0; dicaAtiva = 0; offsetLembrado = 0;
  dopasujPedido = dopasujAtivo = 0; epOutro = 0;
}
// "tt123" and "tt123:1:2" are the same title: the detail page stamps the raw
// id before the episode list arrives (addons.c). Another episode is not.
static int mesmoAlvo(const char *a, const char *b) {
  size_t na = strlen(a), nb = strlen(b);
  if (!strcmp(a, b)) return 1;
  if (na < nb) return !strncmp(a, b, na) && b[na] == ':' && !strchr(a, ':');
  if (nb < na) return !strncmp(a, b, nb) && a[nb] == ':' && !strchr(b, ':');
  return 0;
}
void audioext_alvo(const char *alvo) {
  snprintf(alvoAtual, sizeof alvoAtual, "%s", alvo ? alvo : "");
  if (urlMarca[0] && alvoAtual[0] && !mesmoAlvo(alvoAtual, alvoMarca)) {
    printf("[ext-audio] mark cleared: another title or episode (%s)\n", alvoAtual);
    fflush(stdout);
    audioext_limpar();
  }
}
int audioext_alternar(const char *url, const char *cabecalhos, const char *nome) {
  if (!url || !*url) return -1;
  if (audioext_marcada(url) && !audioext_marcada_nome(url, nome)) {
    // The same file under another add-on's name: the mark goes to this row.
    snprintf(nomeMarca, sizeof nomeMarca, "%s", nome ? nome : "");
    printf("[ext-audio] audio source marked: %.80s (the same file, target %s)\n", nomeMarca, alvoMarca);
    fflush(stdout);
    return 1;
  }
  if (audioext_marcada(url)) {
    audioext_limpar();
    printf("[ext-audio] mark cleared by the user\n"); fflush(stdout);
    return 0;
  }
  snprintf(urlMarca, sizeof urlMarca, "%s", url);
  snprintf(cabMarca, sizeof cabMarca, "%s", cabecalhos ? cabecalhos : "");
  snprintf(nomeMarca, sizeof nomeMarca, "%s", nome ? nome : "");
  snprintf(alvoMarca, sizeof alvoMarca, "%s", alvoAtual);
  idiomaMarca[0] = 0;
  offsetMs = 0; offsetGeracao++; offsetSuave = 0; posAtual = -1; velocidadePpm = 0; kandN = 0;
  pendente = 0; faixaMarca = -1; dicaAtiva = 0; offsetLembrado = 0; epOutro = 0;
  // The name is the add-on's label, never the URL (signed links stay out of logs).
  printf("[ext-audio] audio source marked: %.80s (target %s)\n", nomeMarca, alvoMarca);
  fflush(stdout);
  return 1;
}
void audioext_escolher(const char *url, const char *cabecalhos, const char *nome, const char *idioma) {
  if (!url || !*url) return;
  // A new pick (even of the same version): its fallbacks and tries start over.
  resN = resProx = 0; tentativas = tentativa = 0; retryEm = 0; preparandoAviso = 0;
  snprintf(urlPrim, sizeof urlPrim, "%s", url);
  snprintf(cabPrim, sizeof cabPrim, "%s", cabecalhos ? cabecalhos : "");
  if (strcmp(urlMarca, url)) {
    snprintf(urlMarca, sizeof urlMarca, "%s", url);
    snprintf(cabMarca, sizeof cabMarca, "%s", cabecalhos ? cabecalhos : "");
    snprintf(nomeMarca, sizeof nomeMarca, "%s", nome ? nome : "");
    snprintf(alvoMarca, sizeof alvoMarca, "%s", alvoAtual);
    offsetMs = 0; offsetGeracao++; offsetSuave = 0; posAtual = -1; velocidadePpm = 0; kandN = 0; kandN = 0; velocidadePpm = 0;
    faixaMarca = -1; dicaAtiva = 0; offsetLembrado = 0;
  }
  epOutro = 0;   // audioext_episodio_outro after the pick, for another episode's version
  // The same file can come from two add-ons under two names: the row picked.
  snprintf(nomeMarca, sizeof nomeMarca, "%s", nome ? nome : "");
  snprintf(idiomaMarca, sizeof idiomaMarca, "%s", idioma ? idioma : "");
  pedidoGeracao++;
  pendente = 1;   // the player reopens with it now
  printf("[ext-audio] chosen in the audio menu: %.80s lang=%s (target %s)\n", nomeMarca, idiomaMarca, alvoMarca);
  fflush(stdout);
}
void audioext_original(void) {
  audioext_limpar();
  audioext_serie_esquecer();
  pedidoGeracao++;
  printf("[ext-audio] back to this file's own audio\n"); fflush(stdout);
}
void audioext_sem_resposta(void) {
  audioext_limpar();
  pedidoGeracao++;
  printf("[ext-audio] the chosen version stopped answering: back to this file's own audio (series memory kept)\n");
  fflush(stdout);
}
unsigned audioext_pedido_geracao(void) {
  // The try after the placeholder clip comes due here (the player asks every
  // frame): the same as picking the version again.
  if (retryEm && urlMarca[0] && relogio() >= retryEm) {
    retryEm = 0;
    pedidoGeracao++;
    pendente = 1;
    printf("[ext-audio] debrid placeholder: try %d of %d now\n", tentativa, tentativas); fflush(stdout);
  }
  return pedidoGeracao;
}
void audioext_tentativas(int n) { tentativas = n < 0 ? 0 : n; tentativa = 0; }
int audioext_motivo_placeholder(const char *m) { return m && strstr(m, "(placeholder clip)") != NULL; }
int audioext_preparando(void) { return retryEm != 0 && urlMarca[0]; }
int audioext_preparando_consumir(int *t, int *total) {
  int a = preparandoAviso;
  preparandoAviso = 0;
  if (a && t) *t = a;
  if (a && total) *total = tentativas;
  return a != 0;
}
static void negar(const char *url) {
  if (!url || !*url) return;
  for (int i = 0; i < negadasN; i++) if (!strcmp(negadas[i], url)) return;
  if (negadasN < 4) snprintf(negadas[negadasN++], sizeof negadas[0], "%s", url);
}
int audioext_negada_consumir(char *dst, size_t n) {
  if (!negadasN) return 0;
  snprintf(dst, n, "%s", negadas[0]);
  memmove(negadas[0], negadas[1], sizeof negadas[0] * (size_t)(negadasN - 1));
  negadasN--;
  return 1;
}
void audioext_escolher_faixa(int stream) {
  if (!urlMarca[0] || stream < 0) return;
  faixaMarca = stream; dicaAtiva = 0;
  pedidoGeracao++;
  pendente = 1;   // the player reopens with it now
  printf("[ext-audio] track %d of \"%.80s\" chosen in the audio menu\n", stream, nomeMarca);
  fflush(stdout);
}
int audioext_faixa(void) { return faixaMarca; }
void audioext_definir_faixa(int stream) { if (urlMarca[0]) faixaMarca = stream >= 0 ? stream : -1; }

/* TRACKS OF A VERSION (hold-OK menu): the platform's header reader runs on a
 * detached thread; the UI polls with the request's generation, and an older
 * request finishing late is dropped. */
static int (*sondaFn)(const char *, const char *, AudioExtFaixa *, int);
static pthread_mutex_t sondaLock = PTHREAD_MUTEX_INITIALIZER;
static unsigned sondaGer;
static int sondaN = -2;
static AudioExtFaixa sondaFaixas[AUDIOEXT_FAIXAS_MAX];
typedef struct { unsigned ger; char *url, *cab; } SondaPedido;
static char *dup(const char *t) { size_t n = strlen(t) + 1; char *d = malloc(n); if (d) memcpy(d, t, n); return d; }
static void *sondaFio(void *arg) {
  SondaPedido *p = arg;
  AudioExtFaixa f[AUDIOEXT_FAIXAS_MAX];
  int n = sondaFn ? sondaFn(p->url, p->cab, f, AUDIOEXT_FAIXAS_MAX) : -1;
  pthread_mutex_lock(&sondaLock);
  if (p->ger == sondaGer) {
    sondaN = n == AUDIOEXT_SONDA_PLACEHOLDER ? n : n < 0 ? -1 : n;
    if (n > 0) memcpy(sondaFaixas, f, (size_t)n * sizeof *f);
  }
  pthread_mutex_unlock(&sondaLock);
  printf("[ext-audio] tracks read for the hold-OK menu: %d\n", n); fflush(stdout);
  free(p->url); free(p->cab); free(p);
  return NULL;
}
void audioext_definir_sonda(int (*fn)(const char *, const char *, AudioExtFaixa *, int)) { sondaFn = fn; }
int audioext_sonda_disponivel(void) { return sondaFn != NULL; }
unsigned audioext_sondar(const char *url, const char *cab) {
  SondaPedido *p = calloc(1, sizeof *p);
  pthread_t t;
  unsigned ger;
  pthread_mutex_lock(&sondaLock);
  ger = ++sondaGer; sondaN = -2;
  pthread_mutex_unlock(&sondaLock);
  if (!p || !sondaFn || !url || !*url) { free(p); pthread_mutex_lock(&sondaLock); if (ger == sondaGer) sondaN = -1; pthread_mutex_unlock(&sondaLock); return ger; }
  p->ger = ger; p->url = dup(url); p->cab = dup(cab ? cab : "");
  if (!p->url || !p->cab || pthread_create(&t, NULL, sondaFio, p)) {
    free(p->url); free(p->cab); free(p);
    pthread_mutex_lock(&sondaLock); if (ger == sondaGer) sondaN = -1; pthread_mutex_unlock(&sondaLock);
    return ger;
  }
  pthread_detach(t);
  return ger;
}
int audioext_sonda_resultado(unsigned ger, AudioExtFaixa *out, int max) {
  int n;
  pthread_mutex_lock(&sondaLock);
  n = ger == sondaGer ? sondaN : -1;
  if (n > 0) { if (n > max) n = max; memcpy(out, sondaFaixas, (size_t)n * sizeof *out); }
  pthread_mutex_unlock(&sondaLock);
  return n;
}
int audioext_tem_marca(void) { return urlMarca[0] != 0; }
const char *audioext_idioma(void) { return idiomaMarca; }
int audioext_marcada(const char *url) {
  return urlMarca[0] && url && !strcmp(url, urlMarca);
}
int audioext_marcada_nome(const char *url, const char *nome) {
  return audioext_marcada(url) && (!nomeMarca[0] || !nome || !strcmp(nome, nomeMarca));
}
int audioext_pedido(const char *urlVideo) {
  return urlMarca[0] && mesmoAlvo(alvoAtual, alvoMarca) && urlVideo && *urlVideo &&
         strcmp(urlVideo, urlMarca) != 0;
}
const char *audioext_url(void) { return urlMarca; }
const char *audioext_cabecalhos(void) { return cabMarca; }
const char *audioext_nome(void) { return nomeMarca; }
// The series memory keeps the offset of an episode's start (owner's log 16/11,
// Chicago Fire: 3 s from the end of one episode put the next one's start off).
static int memoAceita(void) { return posAtual < 0 || posAtual < AUDIOEXT_SERIE_INICIO_S; }
void audioext_posicao(double segundos) { posAtual = segundos; }
int audioext_offset_suave(void) { return offsetSuave; }
int audioext_offset_ms(void) { return offsetMs; }
int audioext_offset_somar(int ms) {
  return audioext_offset_definir(offsetMs + ms);
}
// +-10 min: another cut can be minutes away (owner, 07/11).
int audioext_offset_definir(int ms) {
  offsetMs = ms;
  if (offsetMs > AUDIOEXT_OFFSET_MAX) offsetMs = AUDIOEXT_OFFSET_MAX;
  if (offsetMs < -AUDIOEXT_OFFSET_MAX) offsetMs = -AUDIOEXT_OFFSET_MAX;
  offsetGeracao++; offsetSuave = 0;
  { char sr[64]; serieDe(alvoMarca, sr, sizeof sr);
    if (memo.ativo && !epOutro && sr[0] && !strcmp(sr, memo.serie) && memoAceita()) { memo.offset = offsetMs; memoSalvar(); } }
  return offsetMs;
}
unsigned audioext_offset_geracao(void) { return offsetGeracao; }
void audioext_tocando_definir(int sim) {
  tocando = sim != 0;
  if (tocando) { pendente = 0; carregando = 0; }
}
void audioext_carregando_definir(int sim) { carregando = sim != 0; }
void audioext_dopasuj_pedir(void) {
  if (!urlMarca[0] || !tocando || dopasujAtivo) return;
  dopasujPedido = 1; dopasujAtivo = 1;
  printf("[ext-audio] match automatically: requested\n"); fflush(stdout);
}
int audioext_dopasuj_pedido_consumir(void) { int p = dopasujPedido; dopasujPedido = 0; return p; }
int audioext_dopasuj_ativo(void) { return dopasujAtivo; }
void audioext_dopasuj_resultado(int ok, const AudioExtDopasuj *r) {
  AudioExtDopasuj z = { 0 };
  if (!r) r = &z;
  // A second look (como 5) or the follow's own match arrive without a click.
  // ...and so does a check made while playing (pozniej).
  if (!dopasujAtivo && !(ok && r->como == 5) && !r->automatico && !r->pozniej) return;
  dopasujAtivo = 0; dopasujPedido = 0;
  dopasujInfo = *r;
  if (r->como != 5) {   // a second look keeps the click's candidates
    kandN = r->nkand < 0 ? 0 : r->nkand > 3 ? 3 : r->nkand;
    for (int i = 0; i < kandN; i++) { kandMs[i] = r->kand_ms[i]; kandPct[i] = r->kand_pct[i]; }
  }
  if (ok && urlMarca[0]) {
    int ms = r->ms;
    if (ms > AUDIOEXT_OFFSET_MAX) ms = AUDIOEXT_OFFSET_MAX;
    if (ms < -AUDIOEXT_OFFSET_MAX) ms = -AUDIOEXT_OFFSET_MAX;
    // Automatic, same speed, a few ms away: kept as it is (AUDIOEXT_AUTO_MIN_MS).
    if ((r->automatico || r->pozniej) && r->ppm == velocidadePpm && ms - offsetMs < AUDIOEXT_AUTO_MIN_MS &&
        offsetMs - ms < AUDIOEXT_AUTO_MIN_MS) {
      printf("[ext-audio] match: %d ms is %d ms from the current offset, kept\n", ms, ms - offsetMs);
      fflush(stdout);
      ms = offsetMs;
    }
    if (ms != offsetMs) { offsetMs = ms; offsetGeracao++; offsetSuave = r->pozniej != 0; }
    // Another speed: the other version is opened again at it (the swap).
    if (r->ppm != velocidadePpm) { velocidadePpm = r->ppm; pedidoGeracao++; }
    { char sr[64]; serieDe(alvoMarca, sr, sizeof sr);
      if (memo.ativo && sr[0] && !strcmp(sr, memo.serie)) {
        if (!r->pozniej && !epOutro && memoAceita()) memo.offset = offsetMs;
        memo.velocidade = velocidadePpm;
        memoSalvar();
      } }
  }
  dopasujInfo.ms = offsetMs;
  dopasujAviso = ok ? 1 : -1;
  if (ok) autoGeracao++;
}
// Bumped by every match result applied: the sync editor's Cancel goes back to
// the newest automatic offset, not to one from before it (owner's log 9).
unsigned audioext_auto_geracao(void) { return autoGeracao; }
int audioext_dopasuj_aviso_consumir(AudioExtDopasuj *out) {
  int a = dopasujAviso;
  dopasujAviso = 0;
  *out = dopasujInfo;
  return a;
}
int audioext_passo(int base, int repeticoes) {
  if (repeticoes <= 0) return base;            // a tap: the step from Settings
  if (repeticoes < 10) return base > 100 ? base : 100;
  if (repeticoes < 25) return 1000;
  return 10000;                                // held: 100 ms, 1 s, then 10 s
}
void audioext_formatar_offset(int ms, char *dst, unsigned tam, const char *fmtSeg, const char *fmtMin) {
  char sinal = ms < 0 ? '-' : '+';
  unsigned a = ms < 0 ? (unsigned)(-(long)ms) : (unsigned)ms;
  if (a < 10000) snprintf(dst, tam, "%c%u ms", sinal, a);
  else if (a < 60000) snprintf(dst, tam, fmtSeg, sinal, a / 1000, a % 1000);
  else snprintf(dst, tam, fmtMin, sinal, a / 60000, a / 1000 % 60, a % 1000);
}
int audioext_velocidade(void) { return urlMarca[0] ? velocidadePpm : 0; }
// Known already in the converter: no new request (pedidoGeracao stays).
void audioext_velocidade_auto(int ppm) {
  if (!urlMarca[0]) return;
  velocidadePpm = ppm;
  printf("[ext-audio] speed %d ppm from the frame rates\n", ppm); fflush(stdout);
}
// The offset the two lengths suggest (another cut with its extra minutes at
// the start), 0 when they are close: for the sync editor.
int audioext_dica_ms(void) {
  if (!urlMarca[0] || (avisoCorte <= 10 && avisoCorte >= -10)) return 0;
  // Beyond the sync's reach (an extended against a theatrical cut, owner's
  // log 07/11 showed it clipped to +10 min): no hint at all.
  if (avisoCorte > AUDIOEXT_DICA_MAX / 1000 || avisoCorte < -AUDIOEXT_DICA_MAX / 1000) return 0;
  return -avisoCorte * 1000;
}
static char idiomaPrincipal[8];
static double saltoPrevisto;
static int reconexao;
void audioext_salto_previsto(double segundos) { saltoPrevisto = segundos; }
double audioext_salto_previsto_consumir(void) { double s = saltoPrevisto; saltoPrevisto = 0; return s; }
void audioext_reconexao_marcar(void) { reconexao = 1; }
static int seguirAviso, seguirMs;
void audioext_seguir_evento(int tipo, int ms) {
  if (!urlMarca[0]) return;
  // resumed / a small shift corrected / the playhead entered another stretch
  // of the edit map (5, quiet): the converter's new offset
  if (tipo == 2 || tipo == 4 || tipo == 5) {
    offsetMs = ms > AUDIOEXT_OFFSET_MAX ? AUDIOEXT_OFFSET_MAX : ms < -AUDIOEXT_OFFSET_MAX ? -AUDIOEXT_OFFSET_MAX : ms;
    offsetGeracao++; offsetSuave = 0;   // the converter already moved: the live apply finds it equal
  }
  if (tipo != 5) { seguirAviso = tipo; seguirMs = offsetMs; }
}
int audioext_seguir_aviso_consumir(int *ms) { int a = seguirAviso; seguirAviso = 0; *ms = seguirMs; return a; }
int audioext_reconexao_consumir(void) { int r = reconexao; reconexao = 0; return r; }
static int lento;
void audioext_lento_marcar(int etapa) { lento = etapa; }
int audioext_lento_consumir(void) { int r = lento; lento = 0; return r; }
void audioext_idioma_principal_definir(const char *idioma) {
  snprintf(idiomaPrincipal, sizeof idiomaPrincipal, "%s", idioma ? idioma : "");
}
const char *audioext_idioma_principal(void) { return idiomaPrincipal; }
int audioext_kandydatos(int *ms, int *pct, int max) {
  int n = urlMarca[0] ? kandN : 0;
  if (n > max) n = max;
  for (int i = 0; i < n; i++) { ms[i] = kandMs[i]; pct[i] = kandPct[i]; }
  return n;
}
int audioext_carregando(void) { return carregando && urlMarca[0]; }
void audioext_inicio_reproducao(const char *urlVideo) {
  if (!audioext_pedido(urlVideo)) return;
  if (pendente) {
    printf("[ext-audio] the last start with \"%.80s\" never played its audio: this file's own audio now\n", nomeMarca);
    fflush(stdout);
    audioext_limpar();
    voltouPadrao = 1;
    return;
  }
  pendente = 1;
}
void audioext_diferenca(int corteSeg, int velocidade) {
  avisoCorte = corteSeg; avisoVelocidade = velocidade;
  avisoPendente = (corteSeg > 10 || corteSeg < -10 || velocidade);
}
int audioext_diferenca_consumir(int *corteSeg, int *velocidade) {
  if (!avisoPendente) return 0;
  avisoPendente = 0;
  *corteSeg = avisoCorte; *velocidade = avisoVelocidade;
  return 1;
}
int audioext_voltou_padrao_consumir(void) {
  int v = voltouPadrao;
  voltouPadrao = 0;
  return v;
}
int audioext_tocando(void) { return tocando; }
void audioext_reserva(const char *url, const char *cabecalhos) {
  if (!url || !*url || resN >= 3 || !urlMarca[0] || !strcmp(url, urlMarca)) return;
  snprintf(resUrl[resN], sizeof resUrl[resN], "%s", url);
  snprintf(resCab[resN], sizeof resCab[resN], "%s", cabecalhos ? cabecalhos : "");
  resN++;
}
void audioext_falhou(const char *motivo) {
  int ph = audioext_motivo_placeholder(motivo);
  // Another copy of the same file is on hand (another add-on): that one now,
  // same name, language and track; the failure is only said if all fail.
  if (resProx < resN && urlMarca[0]) {
    int r = resProx++;
    printf("[ext-audio] copy failed (%.80s): trying copy %d of %d\n",
           motivo && *motivo ? motivo : "unknown error", r + 1, resN);
    fflush(stdout);
    snprintf(urlMarca, sizeof urlMarca, "%s", resUrl[r]);
    snprintf(cabMarca, sizeof cabMarca, "%s", resCab[r]);
    tocando = 0; carregando = 0;
    pedidoGeracao++;
    pendente = 1;   // the player reopens with it now
    return;
  }
  // A version said to be cached answered with the debrid's placeholder clip:
  // the file is being prepared. The chosen link again in a few seconds (the
  // copies after it again too); meanwhile this file's own audio plays.
  if (ph && urlMarca[0] && tentativa < tentativas) {
    tentativa++;
    printf("[ext-audio] debrid placeholder on a cached version: try %d of %d in %d s\n",
           tentativa, tentativas, AUDIOEXT_ESPERA_MS / 1000);
    fflush(stdout);
    snprintf(urlMarca, sizeof urlMarca, "%s", urlPrim);
    snprintf(cabMarca, sizeof cabMarca, "%s", cabPrim);
    resProx = 0;
    tocando = 0; carregando = 0;
    retryEm = relogio() + AUDIOEXT_ESPERA_MS;
    preparandoAviso = tentativa;
    return;
  }
  // Not ready after all: the links tried are marked not cached (the source
  // list learns it too), like a debrid "still downloading" answer there.
  if (ph) { negar(urlPrim); for (int r = 0; r < resN; r++) negar(resUrl[r]); }
  snprintf(falha, sizeof falha, "%s", motivo && *motivo ? motivo : "unknown error");
  // The session goes back to the file's own audio, and so does the next start:
  // the mark goes away (it can be picked again in the audio menu).
  audioext_limpar();
}
int audioext_falha_consumir(char *dst, unsigned tam) {
  if (!falha[0]) return 0;
  if (dst && tam) snprintf(dst, tam, "%s", falha);
  falha[0] = 0;
  return 1;
}

void audioext_preferencias(int soAudio, int serie) {
  prefSoAudio = soAudio != 0; prefSerie = serie != 0;
  if (!prefSerie) memo.ativo = 0;
  // Called every frame: the changes of the last moments go to disk together
  // (a run of +/- presses on the offset is one write).
  if (serSujo && relogio() - serSujoEm >= AUDIOEXT_SERIE_GRAVAR_MS) serGravarAgora();
}
int audioext_so_audio(void) { return prefSoAudio; }
static int nomeTagEpisodio(const char *w, size_t n) {
  size_t i = 0;
  if (n < 2) return 0;
  if (w[0] == 's') {   /* s10e12, s10 */
    for (i = 1; i < n && w[i] >= '0' && w[i] <= '9'; i++) {}
    if (i == 1) return 0;
    if (i == n) return 1;
    if (w[i] != 'e') return 0;
    size_t j = ++i;
    for (; i < n && w[i] >= '0' && w[i] <= '9'; i++) {}
    return i == n && i > j;
  }
  if (w[0] == 'e') {   /* e12 */
    for (i = 1; i < n && w[i] >= '0' && w[i] <= '9'; i++) {}
    return i == n && i > 1;
  }
  for (; i < n && w[i] >= '0' && w[i] <= '9'; i++) {}   /* 10x12 */
  if (i == 0 || i >= n || w[i] != 'x') return 0;
  size_t j = ++i;
  for (; i < n && w[i] >= '0' && w[i] <= '9'; i++) {}
  return i == n && i > j;
}
void audioext_nome_versao(const char *rotulo, char *dst, size_t n) {
  char w[96];
  size_t o = 0;
  if (!dst || !n) return;
  dst[0] = 0;
  if (!rotulo) return;
  for (const char *p = rotulo; ; ) {
    size_t k = 0;
    while (*p && !((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9'))) p++;
    if (!*p) break;
    while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')) {
      if (k + 1 < sizeof w) w[k++] = (char)(*p >= 'A' && *p <= 'Z' ? *p - 'A' + 'a' : *p);
      p++;
    }
    if (nomeTagEpisodio(w, k)) continue;
    if (o + (o ? 1 : 0) + k + 1 > n) break;
    if (o) dst[o++] = ' ';
    memcpy(dst + o, w, k); o += k; dst[o] = 0;
  }
}
void audioext_lembrar_serie(const AudioExtIdent *id) {
  char sr[64];
  // No mark yet (the pick being made now): the episode on screen.
  serieDe(alvoMarca[0] ? alvoMarca : alvoAtual, sr, sizeof sr);
  if (!prefSerie || !id || !sr[0]) return;
  if (epOutro) {
    printf("[ext-audio] series memory: not changed, the audio is episode %d's\n", epOutro); fflush(stdout);
    return;
  }
  memset(&memo, 0, sizeof memo);
  snprintf(memo.serie, sizeof memo.serie, "%s", sr);
  snprintf(memo.episodio, sizeof memo.episodio, "%s", alvoMarca[0] ? alvoMarca : alvoAtual);
  memo.id = *id; memo.offset = offsetMs; memo.velocidade = velocidadePpm; memo.ativo = 1; memo.temFaixa = 0;
  memoSalvar();
  printf("[ext-audio] series memory: group=%s addon=%.40s name=\"%.60s\" lang=%s %dp codec=%d\n", memo.id.grupo[0] ? memo.id.grupo : "-",
         memo.id.addon, memo.id.nome, memo.id.idioma, memo.id.altura, memo.id.codec);
  fflush(stdout);
}
int audioext_serie_procura(const char *alvo, AudioExtIdent *id) {
  char sr[64];
  if (!prefSerie || !alvo || !*alvo) return 0;
  serieDe(alvo, sr, sizeof sr);
  if (!sr[0] || !memoCarregar(sr) || !strcmp(alvo, memo.episodio)) return 0;
  if (urlMarca[0] && mesmoAlvo(alvo, alvoMarca)) return 0;   /* already chosen here */
  if (id) *id = memo.id;
  return 1;
}
void audioext_serie_achou(const char *url, const char *cabecalhos, const char *nome) {
  if (!url || !*url) return;
  snprintf(urlMarca, sizeof urlMarca, "%s", url);
  snprintf(cabMarca, sizeof cabMarca, "%s", cabecalhos ? cabecalhos : "");
  snprintf(nomeMarca, sizeof nomeMarca, "%s", nome ? nome : "");
  snprintf(alvoMarca, sizeof alvoMarca, "%s", alvoAtual);
  snprintf(idiomaMarca, sizeof idiomaMarca, "%s", memo.id.idioma);
  snprintf(memo.episodio, sizeof memo.episodio, "%s", alvoAtual);
  offsetMs = memo.offset; offsetGeracao++; offsetSuave = 0; velocidadePpm = memo.velocidade;
  faixaMarca = -1; epOutro = 0;
  dicaAtiva = memo.temFaixa; if (dicaAtiva) dica = memo.faixa;
  offsetLembrado = 1;
  memoSalvar();   // used now: the last one to leave a full table
  pedidoGeracao++;
  printf("[ext-audio] series memory: this episode uses %.80s, offset %d ms\n", nomeMarca, offsetMs);
  fflush(stdout);
}
// Back to the file's own audio: the series on screen forgets its version.
void audioext_serie_esquecer(void) {
  char sr[64];
  int i;
  serieDe(alvoAtual, sr, sizeof sr);
  if (!sr[0] && memo.ativo) snprintf(sr, sizeof sr, "%s", memo.serie);
  memo.ativo = 0;
  if (!sr[0]) return;
  serCarregar();
  i = serAchar(sr);
  if (i < 0) return;
  serTab[i] = serTab[--serN];
  serMarcarSujo();
  printf("[ext-audio] series memory: %s forgotten\n", sr); fflush(stdout);
}
void audioext_retomar(const char *url, const char *cabecalhos, const char *nome, const char *idioma,
                      int faixa, int offset, int velocidade) {
  if (!url || !*url || !alvoAtual[0]) return;
  resN = resProx = 0; tentativas = tentativa = 0; retryEm = 0; preparandoAviso = 0;
  snprintf(urlPrim, sizeof urlPrim, "%s", url);
  snprintf(cabPrim, sizeof cabPrim, "%s", cabecalhos ? cabecalhos : "");
  snprintf(urlMarca, sizeof urlMarca, "%s", url);
  snprintf(cabMarca, sizeof cabMarca, "%s", cabecalhos ? cabecalhos : "");
  snprintf(nomeMarca, sizeof nomeMarca, "%s", nome ? nome : "");
  snprintf(alvoMarca, sizeof alvoMarca, "%s", alvoAtual);
  snprintf(idiomaMarca, sizeof idiomaMarca, "%s", idioma ? idioma : "");
  offsetMs = offset > AUDIOEXT_OFFSET_MAX ? AUDIOEXT_OFFSET_MAX : offset < -AUDIOEXT_OFFSET_MAX ? -AUDIOEXT_OFFSET_MAX : offset;
  offsetGeracao++; offsetSuave = 0; posAtual = -1; velocidadePpm = velocidade; kandN = 0;
  faixaMarca = faixa >= 0 ? faixa : -1; epOutro = 0; dicaAtiva = 0;
  offsetLembrado = 1;   // a start match that is not sure keeps it, as from the series memory
  pendente = 0; voltouPadrao = 0;
  pedidoGeracao++;
  printf("[ext-audio] resumed with the saved source: %.80s lang=%s track=%d offset %d ms (target %s)\n",
         nomeMarca, idiomaMarca, faixaMarca, offsetMs, alvoMarca);
  fflush(stdout);
}
void audioext_episodio_outro(int episodio) {
  if (!urlMarca[0]) return;
  epOutro = episodio > 0 ? episodio : 0;
  if (epOutro) { printf("[ext-audio] the audio is episode %d's (target %s)\n", epOutro, alvoMarca); fflush(stdout); }
}
int audioext_episodio_outro_atual(void) { return urlMarca[0] ? epOutro : 0; }
// "tt123:1:2" -> base "tt123", season 1, episode 2. IMDb ids only: other
// catalogues (kitsu:ID:EP) number their own way.
int audioext_alvo_episodio(char *base, unsigned tam, int *temporada, int *episodio) {
  const char *a = alvoAtual, *c1 = strchr(a, ':'), *c2 = c1 ? strchr(c1 + 1, ':') : NULL;
  char *fim;
  long t, e;
  if (strncmp(a, "tt", 2) || !c1 || !c2 || strchr(c2 + 1, ':')) return 0;
  t = strtol(c1 + 1, &fim, 10);
  if (fim != c2 || t < 0) return 0;
  e = strtol(c2 + 1, &fim, 10);
  if (*fim || e < 1 || e > 999) return 0;
  if (base) snprintf(base, tam, "%.*s", (int)(c1 - a), a);
  if (temporada) *temporada = (int)t;
  if (episodio) *episodio = (int)e;
  return 1;
}
void audioext_lembrar_faixa_serie(const AudioExtFaixa *f) {
  char sr[64];
  serieDe(alvoMarca, sr, sizeof sr);
  if (!f || !prefSerie || !memo.ativo || epOutro || !sr[0] || strcmp(sr, memo.serie)) return;
  memo.faixa = *f; memo.temFaixa = 1;
  memoSalvar();
  printf("[ext-audio] series memory: track %d (%s %s %dch \"%.40s\")\n", f->stream, f->idioma, f->codec, f->canais, f->titulo);
  fflush(stdout);
}
int audioext_offset_lembrado(void) { return offsetLembrado && urlMarca[0]; }
int audioext_faixa_dica(AudioExtFaixa *out) {
  if (!dicaAtiva || !urlMarca[0] || faixaMarca >= 0) return 0;
  if (out) *out = dica;
  return 1;
}

// ---- "Scan again": the file track the donor is matched against, per show ----
// (owner 18/11). Kept in audio_match.txt: "show\tlanguage\tcodec\tchannels"
// lines, the most recent last, at most AUDIOEXT_MATCH_MAX shows.
#define AUDIOEXT_MATCH_MAX 200
typedef struct { char serie[64], lingua[16], codec[24]; int canais; } MatchSerie;
static MatchSerie matchS[AUDIOEXT_MATCH_MAX];
static int matchN, matchLido;
static char *(*matchLer)(const char *);
static int (*matchGravar)(const char *, const char *);
void audioext_match_io(char *(*ler)(const char *), int (*gravar)(const char *, const char *)) {
  matchLer = ler; matchGravar = gravar; matchLido = 0; matchN = 0;
}
const char *audioext_alvo_atual(void) { return alvoAtual; }
int audioext_serie_chave(const char *alvo, char *o, size_t n) {
  const char *c;
  size_t k;
  if (!o || !n) return 0;
  o[0] = 0;
  if (!alvo || !*alvo) return 0;
  c = strchr(alvo, ':');
  // "tt123:1:2" -> "tt123"; "kitsu:47192:1", "tmdb:123:1:2" -> "kitsu:47192", "tmdb:123".
  if (c && strncmp(alvo, "tt", 2)) c = strchr(c + 1, ':');
  k = c ? (size_t)(c - alvo) : strlen(alvo);
  if (k >= n) k = n - 1;
  memcpy(o, alvo, k); o[k] = 0;
  return 1;
}
static void matchCarregar(void) {
  char *t, *l, *prox;
  if (matchLido) return;
  matchLido = 1; matchN = 0;
  if (!matchLer || !(t = matchLer("audio_match.txt"))) return;
  for (l = t; l && *l; l = prox) {
    MatchSerie m;
    char *c[4];
    int i;
    prox = strchr(l, '\n');
    if (prox) *prox++ = 0;
    c[0] = l;
    for (i = 1; i < 4; i++) { c[i] = c[i - 1] ? strchr(c[i - 1], '\t') : NULL; if (c[i]) *c[i]++ = 0; }
    if (!c[3] || !c[0][0] || matchN >= AUDIOEXT_MATCH_MAX) continue;
    memset(&m, 0, sizeof m);
    snprintf(m.serie, sizeof m.serie, "%s", c[0]); snprintf(m.lingua, sizeof m.lingua, "%s", c[1]);
    snprintf(m.codec, sizeof m.codec, "%s", c[2]); m.canais = atoi(c[3]);
    matchS[matchN++] = m;
  }
  free(t);
}
static void matchSalvar(void) {
  char *b;
  size_t cap = (size_t)matchN * 140 + 1, k = 0;
  int i;
  if (!matchGravar || !(b = malloc(cap))) return;
  b[0] = 0;
  for (i = 0; i < matchN; i++) {
    int w = snprintf(b + k, cap - k, "%s\t%s\t%s\t%d\n", matchS[i].serie, matchS[i].lingua, matchS[i].codec, matchS[i].canais);
    if (w < 0 || (size_t)w >= cap - k) break;
    k += (size_t)w;
  }
  matchGravar("audio_match.txt", b);
  free(b);
}
void audioext_match_definir(const char *lingua, const char *codec, int canais) {
  char serie[64];
  int i;
  if (!audioext_serie_chave(alvoAtual, serie, sizeof serie)) return;
  matchCarregar();
  for (i = 0; i < matchN; i++) if (!strcmp(matchS[i].serie, serie)) break;
  if (i < matchN) { memmove(&matchS[i], &matchS[i + 1], sizeof *matchS * (size_t)(matchN - i - 1)); matchN--; }
  if (lingua && lingua[0]) {
    if (matchN == AUDIOEXT_MATCH_MAX) { memmove(&matchS[0], &matchS[1], sizeof *matchS * (size_t)(matchN - 1)); matchN--; }
    memset(&matchS[matchN], 0, sizeof matchS[matchN]);
    snprintf(matchS[matchN].serie, sizeof matchS[matchN].serie, "%s", serie);
    snprintf(matchS[matchN].lingua, sizeof matchS[matchN].lingua, "%s", lingua);
    snprintf(matchS[matchN].codec, sizeof matchS[matchN].codec, "%s", codec ? codec : "");
    matchS[matchN].canais = canais;
    matchN++;
  }
  printf("[ext-audio] match against for %s: %s%s%s\n", serie, lingua && lingua[0] ? lingua : "Auto", codec && codec[0] ? " " : "",
         codec ? codec : "");
  fflush(stdout);
  matchSalvar();
}
int audioext_match_faixa(char *lingua, size_t nl, char *codec, size_t nc, int *canais) {
  char serie[64];
  int i;
  if (lingua && nl) lingua[0] = 0;
  if (codec && nc) codec[0] = 0;
  if (canais) *canais = 0;
  if (!audioext_serie_chave(alvoAtual, serie, sizeof serie)) return 0;
  matchCarregar();
  for (i = matchN - 1; i >= 0; i--)
    if (!strcmp(matchS[i].serie, serie)) {
      if (lingua && nl) snprintf(lingua, nl, "%s", matchS[i].lingua);
      if (codec && nc) snprintf(codec, nc, "%s", matchS[i].codec);
      if (canais) *canais = matchS[i].canais;
      return 1;
    }
  return 0;
}
