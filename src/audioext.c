// Audio from another version of the title: the marked source (audioext.h).
#include "audioext.h"
#include "mediaurl.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char alvoAtual[64], alvoMarca[64];
static char urlMarca[NV_MEDIA_URL_MAX], cabMarca[512], nomeMarca[192];
static int offsetMs, tocando;
static unsigned offsetGeracao;
static char falha[160], idiomaMarca[8];
static unsigned pedidoGeracao;
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
// One remembered series at a time (test build, not saved).
static struct { char serie[64], episodio[64]; AudioExtIdent id; int offset, velocidade, ativo, temFaixa; AudioExtFaixa faixa; } memo;
// This episode came from the series memory with a remembered track (owner
// 09/11: two English tracks, the one chosen): the hint for the player.
static int dicaAtiva;
static AudioExtFaixa dica;
// The offset came from the series memory (same version as the last episode):
// an automatic match at start that is not sure keeps it quietly.
static int offsetLembrado;
static char perfilAtual[96];
static void serieDe(const char *alvo, char *out, size_t tam) {
  const char *c = alvo ? strchr(alvo, ':') : NULL;
  snprintf(out, tam, "%.*s", c ? (int)(c - alvo) : 0, c ? alvo : "");
}

void audioext_limpar(void) {
  audioext_sonda_cancelar();
  pedidoGeracao++;
  urlMarca[0] = cabMarca[0] = nomeMarca[0] = alvoMarca[0] = idiomaMarca[0] = 0;
  offsetMs = 0; offsetGeracao++; velocidadePpm = 0; kandN = 0;
  pendente = 0; faixaMarca = -1; carregando = 0; dicaAtiva = 0; offsetLembrado = 0;
  dopasujPedido = dopasujAtivo = 0;
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
void audioext_sonda_cancelar(void);
void audioext_alvo(const char *alvo) {
  if (strcmp(alvoAtual, alvo ? alvo : "")) audioext_sonda_cancelar();   // another title's tracks never land here
  snprintf(alvoAtual, sizeof alvoAtual, "%s", alvo ? alvo : "");
  if (urlMarca[0] && alvoAtual[0] && !mesmoAlvo(alvoAtual, alvoMarca)) {
    printf("[ext-audio] mark cleared: another title or episode (%s)\n", alvoAtual);
    fflush(stdout);
    audioext_limpar();
  }
}
int audioext_alternar(const char *url, const char *cabecalhos, const char *nome) {
  if (!url || !*url) return -1;
  audioext_sonda_cancelar();
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
  offsetMs = 0; offsetGeracao++; velocidadePpm = 0; kandN = 0;
  pendente = 0; faixaMarca = -1; dicaAtiva = 0; offsetLembrado = 0;
  // The name is the add-on's label, never the URL (signed links stay out of logs).
  printf("[ext-audio] audio source marked: %.80s (target %s)\n", nomeMarca, alvoMarca);
  fflush(stdout);
  return 1;
}
void audioext_escolher(const char *url, const char *cabecalhos, const char *nome, const char *idioma) {
  if (!url || !*url) return;
  if (!audioext_marcada(url)) {
    snprintf(urlMarca, sizeof urlMarca, "%s", url);
    snprintf(cabMarca, sizeof cabMarca, "%s", cabecalhos ? cabecalhos : "");
    snprintf(nomeMarca, sizeof nomeMarca, "%s", nome ? nome : "");
    snprintf(alvoMarca, sizeof alvoMarca, "%s", alvoAtual);
    offsetMs = 0; offsetGeracao++; velocidadePpm = 0; kandN = 0; kandN = 0; velocidadePpm = 0;
    faixaMarca = -1; dicaAtiva = 0; offsetLembrado = 0;
  }
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
unsigned audioext_pedido_geracao(void) { return pedidoGeracao; }
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

/* TRACKS OF A VERSION (hold-OK menu). Bounded: ONE worker thread at most,
 * one pending request (a newer one replaces it). The UI polls with the
 * request's generation; a stale result (older request, another title or
 * profile, cancelled) is never published. Cancel interrupts the read in
 * flight through the platform's hook. */
static int (*sondaFn)(const char *, const char *, AudioExtFaixa *, int);
static void (*sondaCancelFn)(void);
static int (*sondaSegura)(const char *, const char *, AudioExtFaixa *, int, const int *);
static pthread_mutex_t sondaLock = PTHREAD_MUTEX_INITIALIZER;
static unsigned sondaGer;
static int sondaN = -2, sondaVivo;
static AudioExtFaixa sondaFaixas[AUDIOEXT_FAIXAS_MAX];
typedef struct { unsigned ger; char *url, *cab; int cancel; } SondaPedido;
static SondaPedido *sondaAtiva;
static SondaPedido *sondaFila;   // the one waiting request (owned under sondaLock)
static char *dup(const char *t) { size_t n = strlen(t) + 1; char *d = malloc(n); if (d) memcpy(d, t, n); return d; }
static void sondaLiberar(SondaPedido *p) { if (p) { free(p->url); free(p->cab); free(p); } }
static void *sondaFio(void *arg) {
  (void)arg;
  for (;;) {
    SondaPedido *p;
    AudioExtFaixa f[AUDIOEXT_FAIXAS_MAX];
    int n;
    pthread_mutex_lock(&sondaLock);
    p = sondaFila; sondaFila = NULL;
    if (!p) { sondaVivo = 0; pthread_mutex_unlock(&sondaLock); return NULL; }
    n = p->ger == sondaGer;
    sondaAtiva = p;
    int (*safe)(const char *, const char *, AudioExtFaixa *, int, const int *) = sondaSegura;
    int (*fn)(const char *, const char *, AudioExtFaixa *, int) = sondaFn;
    pthread_mutex_unlock(&sondaLock);
    n = !n ? -1 : safe ? safe(p->url, p->cab, f, AUDIOEXT_FAIXAS_MAX, &p->cancel) : fn ? fn(p->url, p->cab, f, AUDIOEXT_FAIXAS_MAX) : -1;
    if (n > AUDIOEXT_FAIXAS_MAX) n = AUDIOEXT_FAIXAS_MAX;
    pthread_mutex_lock(&sondaLock);
    sondaAtiva = NULL;
    if (p->ger == sondaGer) {
      sondaN = n < 0 ? -1 : n;
      if (n > 0) memcpy(sondaFaixas, f, (size_t)n * sizeof *f);
    }
    pthread_mutex_unlock(&sondaLock);
    printf("[ext-audio] tracks read for the hold-OK menu: %d\n", n);
    fflush(stdout);
    sondaLiberar(p);
  }
}
void audioext_definir_sonda(int (*fn)(const char *, const char *, AudioExtFaixa *, int)) { sondaFn = fn; }
void audioext_definir_sonda_segura(int (*fn)(const char *, const char *, AudioExtFaixa *, int, const int *)) { pthread_mutex_lock(&sondaLock); sondaSegura = fn; pthread_mutex_unlock(&sondaLock); }
void audioext_definir_sonda_cancelar(void (*fn)(void)) { sondaCancelFn = fn; }
int audioext_sonda_disponivel(void) { return sondaFn != NULL || sondaSegura != NULL; }
unsigned audioext_sondar(const char *url, const char *cab) {
  SondaPedido *p = calloc(1, sizeof *p), *velho;
  pthread_t t;
  unsigned ger;
  int criar = 0, ocupado;
  if (p) { p->url = dup(url ? url : ""); p->cab = dup(cab ? cab : ""); }
  pthread_mutex_lock(&sondaLock);
  ger = ++sondaGer; sondaN = -2;
  if (sondaAtiva) __atomic_store_n(&sondaAtiva->cancel, 1, __ATOMIC_RELAXED);
  velho = sondaFila; sondaFila = NULL;
  if (!p || !p->url || !p->cab || (!sondaFn && !sondaSegura) || !url || !*url) { sondaN = -1; pthread_mutex_unlock(&sondaLock); sondaLiberar(p); sondaLiberar(velho); return ger; }
  p->ger = ger; sondaFila = p;
  ocupado = sondaVivo;
  if (!sondaVivo) { sondaVivo = 1; criar = 1; }
  pthread_mutex_unlock(&sondaLock);
  sondaLiberar(velho);
  if (ocupado && sondaCancelFn) sondaCancelFn();   // the read in flight is stale now
  if (criar) {
    pthread_attr_t a;
    int ok = pthread_attr_init(&a) == 0;
    if (ok) pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
    if (!ok || pthread_create(&t, &a, sondaFio, NULL)) {
      pthread_mutex_lock(&sondaLock);
      sondaVivo = 0; velho = sondaFila; sondaFila = NULL;
      if (ger == sondaGer) sondaN = -1;
      pthread_mutex_unlock(&sondaLock);
      sondaLiberar(velho);
    }
    if (ok) pthread_attr_destroy(&a);
  }
  return ger;
}
void audioext_sonda_cancelar(void) {
  SondaPedido *velho;
  int ocupado;
  pthread_mutex_lock(&sondaLock);
  ++sondaGer; sondaN = -1;
  if (sondaAtiva) __atomic_store_n(&sondaAtiva->cancel, 1, __ATOMIC_RELAXED);
  velho = sondaFila; sondaFila = NULL;
  ocupado = sondaVivo;
  pthread_mutex_unlock(&sondaLock);
  sondaLiberar(velho);
  if (ocupado && sondaCancelFn) sondaCancelFn();
}
int audioext_sonda_ocupada(void) {
  int v;
  pthread_mutex_lock(&sondaLock); v = sondaVivo; pthread_mutex_unlock(&sondaLock);
  return v;
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
int audioext_pedido(const char *urlVideo) {
  return urlMarca[0] && mesmoAlvo(alvoAtual, alvoMarca) && urlVideo && *urlVideo &&
         strcmp(urlVideo, urlMarca) != 0;
}
const char *audioext_url(void) { return urlMarca; }
const char *audioext_cabecalhos(void) { return cabMarca; }
const char *audioext_nome(void) { return nomeMarca; }
int audioext_offset_ms(void) { return offsetMs; }
int audioext_offset_somar(int ms) {
  return audioext_offset_definir(offsetMs + ms);
}
// +-10 min: another cut can be minutes away (owner, 07/11).
int audioext_offset_definir(int ms) {
  offsetMs = ms;
  if (offsetMs > AUDIOEXT_OFFSET_MAX) offsetMs = AUDIOEXT_OFFSET_MAX;
  if (offsetMs < -AUDIOEXT_OFFSET_MAX) offsetMs = -AUDIOEXT_OFFSET_MAX;
  offsetGeracao++;
  { char sr[64]; serieDe(alvoMarca, sr, sizeof sr);
    if (memo.ativo && sr[0] && !strcmp(sr, memo.serie)) memo.offset = offsetMs; }
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
  if (!dopasujAtivo && !(ok && r->como == 5) && !r->automatico) return;
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
    offsetMs = ms; offsetGeracao++;
    // Another speed: the other version is opened again at it (the swap).
    if (r->ppm != velocidadePpm) { velocidadePpm = r->ppm; pedidoGeracao++; }
    { char sr[64]; serieDe(alvoMarca, sr, sizeof sr);
      if (memo.ativo && sr[0] && !strcmp(sr, memo.serie)) { memo.offset = offsetMs; memo.velocidade = velocidadePpm; } }
  }
  dopasujInfo.ms = offsetMs;
  dopasujAviso = ok ? 1 : -1;
}
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
  if (tipo == 2 || tipo == 4) {   // resumed / a small shift corrected: the converter's new offset
    offsetMs = ms > AUDIOEXT_OFFSET_MAX ? AUDIOEXT_OFFSET_MAX : ms < -AUDIOEXT_OFFSET_MAX ? -AUDIOEXT_OFFSET_MAX : ms;
    offsetGeracao++;   // the converter already moved: the live apply finds it equal
  }
  seguirAviso = tipo; seguirMs = offsetMs;
}
int audioext_seguir_aviso_consumir(int *ms) { int a = seguirAviso; seguirAviso = 0; *ms = seguirMs; return a; }
int audioext_reconexao_consumir(void) { int r = reconexao; reconexao = 0; return r; }
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
void audioext_falhou(const char *motivo) {
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
}
int audioext_so_audio(void) { return prefSoAudio; }
void audioext_lembrar_serie(const AudioExtIdent *id) {
  char sr[64];
  serieDe(alvoMarca, sr, sizeof sr);
  if (!prefSerie || !id || !sr[0]) return;
  memset(&memo, 0, sizeof memo);
  snprintf(memo.serie, sizeof memo.serie, "%s", sr);
  snprintf(memo.episodio, sizeof memo.episodio, "%s", alvoMarca);
  memo.id = *id; memo.offset = offsetMs; memo.velocidade = velocidadePpm; memo.ativo = 1; memo.temFaixa = 0;
  printf("[ext-audio] series memory: group=%s addon=%.40s lang=%s %dp codec=%d\n", memo.id.grupo[0] ? memo.id.grupo : "-",
         memo.id.addon, memo.id.idioma, memo.id.altura, memo.id.codec);
  fflush(stdout);
}
int audioext_serie_procura(const char *alvo, AudioExtIdent *id) {
  char sr[64];
  if (!prefSerie || !memo.ativo || !alvo || !*alvo) return 0;
  serieDe(alvo, sr, sizeof sr);
  if (!sr[0] || strcmp(sr, memo.serie) || !strcmp(alvo, memo.episodio)) return 0;
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
  offsetMs = memo.offset; offsetGeracao++; velocidadePpm = memo.velocidade;
  faixaMarca = -1;
  dicaAtiva = memo.temFaixa; if (dicaAtiva) dica = memo.faixa;
  offsetLembrado = 1;
  pedidoGeracao++;
  printf("[ext-audio] series memory: this episode uses %.80s, offset %d ms\n", nomeMarca, offsetMs);
  fflush(stdout);
}
void audioext_serie_esquecer(void) { memo.ativo = 0; }
void audioext_lembrar_faixa_serie(const AudioExtFaixa *f) {
  char sr[64];
  serieDe(alvoMarca, sr, sizeof sr);
  if (!f || !prefSerie || !memo.ativo || !sr[0] || strcmp(sr, memo.serie)) return;
  memo.faixa = *f; memo.temFaixa = 1;
  printf("[ext-audio] series memory: track %d (%s %s %dch \"%.40s\")\n", f->stream, f->idioma, f->codec, f->canais, f->titulo);
  fflush(stdout);
}
int audioext_offset_lembrado(void) { return offsetLembrado && urlMarca[0]; }
int audioext_faixa_dica(AudioExtFaixa *out) {
  if (!dicaAtiva || !urlMarca[0] || faixaMarca >= 0) return 0;
  if (out) *out = dica;
  return 1;
}
// PROFILE ISOLATION: another profile (or account) on this TV forgets the mark,
// the series memory and any track read in flight; nothing carries over.
void audioext_perfil(const char *chave) {
  const char *c = chave ? chave : "";
  if (!strcmp(perfilAtual, c)) return;
  if (perfilAtual[0] || urlMarca[0] || memo.ativo) {
    printf("[ext-audio] profile changed: mark and series memory forgotten\n"); fflush(stdout);
  }
  snprintf(perfilAtual, sizeof perfilAtual, "%s", c);
  audioext_sonda_cancelar();
  if (urlMarca[0]) pedidoGeracao++;   // a playing session goes back to its own audio
  audioext_limpar();
  memset(&memo, 0, sizeof memo);
}
