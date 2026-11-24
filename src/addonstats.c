#include "addonstats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#ifndef NV_ADDONSTATS_PURO
#include "dados.h"
#endif

typedef struct {
  char nome[48];
  int n;                                  // amostras guardadas (<= JANELA)
  unsigned ms[ADDONSTATS_JANELA];         // da mais antiga para a mais nova
  unsigned char ok[ADDONSTATS_JANELA];
} Reg;

static Reg reg[ADDONSTATS_MAX];
// ponytail: 16 recent installation/type scopes; least recently used scope evicted.
#define AIO_SCOPES 16
typedef struct {
  char key[48], type[16];
  uint64_t age; time_t retryAt; int count;
  AddonAioRequest *request[ADDONSTATS_AIO_HISTORY];
} AioHistory;
static AioHistory aioHistory[AIO_SCOPES];
static uint64_t aioAge;
static int nReg;
static int sujo;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

static Reg *achar(const char *nome, int criar) {
  int i;
  if (!nome || !*nome) return NULL;
  for (i = 0; i < nReg; i++) if (!strcasecmp(reg[i].nome, nome)) return &reg[i];
  if (!criar) return NULL;
  if (nReg >= ADDONSTATS_MAX) {
    // Cheio: o mais antigo sai. Ordem do vetor = ordem de criacao.
    memmove(&reg[0], &reg[1], sizeof reg[0] * (ADDONSTATS_MAX - 1));
    nReg = ADDONSTATS_MAX - 1;
  }
  memset(&reg[nReg], 0, sizeof reg[nReg]);
  snprintf(reg[nReg].nome, sizeof reg[nReg].nome, "%s", nome);
  return &reg[nReg++];
}

static AioHistory *aio_find(const char *key,const char *type,int create) {
  if(!key || !*key || !type || strlen(key)>=48 || strlen(type)>=16) return NULL;
  int oldest=0;
  for(int i=0;i<AIO_SCOPES;i++) {
    if(!strcmp(aioHistory[i].key,key) && !strcmp(aioHistory[i].type,type)) return &aioHistory[i];
    if(aioHistory[i].age<aioHistory[oldest].age) oldest=i;
  }
  if(!create) return NULL;
  AioHistory *h=&aioHistory[oldest];
  for(int i=0;i<h->count;i++) free(h->request[i]);
  memset(h,0,sizeof *h); snprintf(h->key,sizeof h->key,"%s",key); snprintf(h->type,sizeof h->type,"%s",type);
  return h;
}
void addonstats_aio_reset(void) {
  pthread_mutex_lock(&trava);
  for(int i=0;i<AIO_SCOPES;i++) { for(int j=0;j<aioHistory[i].count;j++) free(aioHistory[i].request[j]); }
  memset(aioHistory,0,sizeof aioHistory); aioAge=0;
  pthread_mutex_unlock(&trava);
}
void addonstats_aio_complete(const char *key,const char *type,const char *json,int http,int transport,unsigned long ms,int retryAfter) {
  AddonAioRequest *r=calloc(1,sizeof *r); AioSnapshot *s=calloc(1,sizeof *s);
  if(!r || !s) { free(r); free(s); return; }
  r->completed=time(NULL); r->http=http; r->transport=transport; r->clientMs=ms;
  r->playable=r->statistics=r->errors=-1;
  if(http>=200 && http<300 && !transport && json) {
    aio_resultados(s,json); r->valid=s->responseValid;
    if(r->valid) {
      r->playable=s->playable; r->statistics=s->estatisticas; r->errors=s->erros;
      r->evidenceCount=s->evidencias; r->omitted=s->evidenceOmitted;
      memcpy(r->evidence,s->evidencia,sizeof r->evidence);
    }
  }
  r->state=transport?AIO_REQUEST_TRANSPORT_ERROR:http==401 || http==403?AIO_REQUEST_CONFIG_LOCKED:
    http==429?AIO_REQUEST_RATE_LIMITED:http<200 || http>=300?AIO_REQUEST_HTTP_ERROR:
    !r->valid?AIO_REQUEST_INVALID:r->errors>0?AIO_REQUEST_SOURCE_ERROR:r->playable==0?AIO_REQUEST_EMPTY:AIO_REQUEST_OK;
  free(s);
  pthread_mutex_lock(&trava);
  AioHistory *h=aio_find(key,type,1);
  if(h) {
    h->age=++aioAge;
    if(http==429) { int wait=retryAfter>0?retryAfter:60; if(wait>3600) wait=3600; h->retryAt=r->completed+wait; }
    if(h->count==ADDONSTATS_AIO_HISTORY) { free(h->request[0]); memmove(h->request,h->request+1,sizeof h->request[0]*(--h->count)); }
    h->request[h->count++]=r;
  } else free(r);
  pthread_mutex_unlock(&trava);
}
int addonstats_aio_history(const char *key,const char *type,AddonAioRequest *out,int max) {
  pthread_mutex_lock(&trava); AioHistory *h=aio_find(key,type,0); int n=h?h->count:0;
  if(n>max) n=max; if(n<0) n=0;
  if(out) for(int i=0;i<n;i++) out[i]=*h->request[h->count-n+i];
  pthread_mutex_unlock(&trava); return n;
}
int addonstats_aio_wait(const char *key,const char *type) {
  pthread_mutex_lock(&trava); AioHistory *h=aio_find(key,type,0);
  time_t now=time(NULL); int wait=h && h->retryAt>now?(int)(h->retryAt-now):0;
  pthread_mutex_unlock(&trava); return wait;
}
void addonstats_registrar(const char *nome, unsigned ms, int respondeu) {
  Reg *r;
  pthread_mutex_lock(&trava);
  r = achar(nome, 1);
  if (r) {
    if (r->n >= ADDONSTATS_JANELA) {
      memmove(r->ms, r->ms + 1, sizeof r->ms[0] * (ADDONSTATS_JANELA - 1));
      memmove(r->ok, r->ok + 1, sizeof r->ok[0] * (ADDONSTATS_JANELA - 1));
      r->n = ADDONSTATS_JANELA - 1;
    }
    r->ms[r->n] = ms > 600000u ? 600000u : ms;
    r->ok[r->n] = respondeu ? 1 : 0;
    r->n++;
    sujo = 1;
  }
  pthread_mutex_unlock(&trava);
}

static int mudoSeg(const Reg *r) {
  int k = 0, i;
  if (!r) return 0;
  for (i = r->n - 1; i >= 0 && !r->ok[i]; i--) k++;
  return k;
}

int addonstats_mudo_seguidas(const char *nome) {
  int k;
  pthread_mutex_lock(&trava);
  k = mudoSeg(achar(nome, 0));
  pthread_mutex_unlock(&trava);
  return k;
}

static int lento(const Reg *r, unsigned limiteMs) {
  int i, acima = 0;
  if (!r || r->n < 3 || limiteMs == 0) return 0;
  for (i = 0; i < r->n; i++) if (!r->ok[i] || r->ms[i] > limiteMs) acima++;
  return acima * 3 >= r->n * 2;
}

int addonstats_lento(const char *nome, unsigned limiteMs) {
  int v;
  pthread_mutex_lock(&trava);
  v = lento(achar(nome, 0), limiteMs);
  pthread_mutex_unlock(&trava);
  return v;
}

int addonstats_ignoravel(const char *nome, unsigned limiteMs) {
  Reg *r;
  int v;
  if (limiteMs == 0) return 0;
  pthread_mutex_lock(&trava);
  r = achar(nome, 0);
  v = r && (mudoSeg(r) >= ADDONSTATS_MUDO_IGNORA || lento(r, limiteMs));
  pthread_mutex_unlock(&trava);
  return v;
}

int addonstats_segunda_s(const char *nome, int padraoS) {
  int m;
  pthread_mutex_lock(&trava);
  m = mudoSeg(achar(nome, 0));
  pthread_mutex_unlock(&trava);
  if (m >= ADDONSTATS_MUDO_PULA) return 0;
  if (m >= 1 && padraoS > ADDONSTATS_SEGUNDA_MUDO_S) return ADDONSTATS_SEGUNDA_MUDO_S;
  return padraoS;
}

static int cmpU(const void *a, const void *b) {
  unsigned x = *(const unsigned *)a, y = *(const unsigned *)b;
  return x < y ? -1 : x > y;
}

unsigned addonstats_mediana(const char *nome) {
  Reg *r;
  unsigned v[ADDONSTATS_JANELA], m = 0;
  int i, k = 0;
  pthread_mutex_lock(&trava);
  r = achar(nome, 0);
  if (r) for (i = 0; i < r->n; i++) if (r->ok[i]) v[k++] = r->ms[i];
  pthread_mutex_unlock(&trava);
  if (k) { qsort(v, (size_t)k, sizeof v[0], cmpU); m = v[k / 2]; }
  return m;
}

int addonstats_serializar(char *dst, unsigned tam) {
  unsigned usado = 0;
  int i, j;
  if (!dst || tam < 2) return 0;
  dst[0] = 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < nReg; i++) {
    int w;
    if (!reg[i].n) continue;
    w = snprintf(dst + usado, tam - usado, "%s\t", reg[i].nome);
    if (w < 0 || (unsigned)w >= tam - usado) break;
    usado += (unsigned)w;
    for (j = 0; j < reg[i].n; j++) {
      w = snprintf(dst + usado, tam - usado, "%s%u:%d", j ? " " : "", reg[i].ms[j], reg[i].ok[j]);
      if (w < 0 || (unsigned)w >= tam - usado) break;
      usado += (unsigned)w;
    }
    if (usado + 2 >= tam) break;
    dst[usado++] = '\n'; dst[usado] = 0;
  }
  pthread_mutex_unlock(&trava);
  return (int)usado;
}

void addonstats_zerar(void) {
  addonstats_aio_reset();
  pthread_mutex_lock(&trava);
  nReg = 0; sujo = 0;
  pthread_mutex_unlock(&trava);
}

void addonstats_carregar(const char *texto) {
  const char *p = texto;
  pthread_mutex_lock(&trava);
  nReg = 0;
  while (p && *p) {
    const char *fim = strchr(p, '\n'), *tab;
    size_t len = fim ? (size_t)(fim - p) : strlen(p);
    char linha[512];
    if (len >= sizeof linha) len = sizeof linha - 1;
    memcpy(linha, p, len); linha[len] = 0;
    tab = strchr(linha, '\t');
    if (tab && tab > linha) {
      Reg *r;
      char *q = (char *)tab + 1;
      *(char *)tab = 0;
      r = achar(linha, 1);
      while (r && *q && r->n < ADDONSTATS_JANELA) {
        char *e;
        unsigned long ms = strtoul(q, &e, 10);
        if (e == q || *e != ':') break;
        r->ms[r->n] = ms > 600000UL ? 600000u : (unsigned)ms;
        r->ok[r->n] = e[1] == '1';
        r->n++;
        q = e + 2;
        while (*q == ' ') q++;
      }
    }
    p = fim ? fim + 1 : NULL;
  }
  sujo = 0;
  pthread_mutex_unlock(&trava);
}

// ------------------------------------------------------------- quarentena 24 h
typedef struct { char chave[48]; time_t ate; } Isola;
static Isola isola[ADDONSTATS_ISOLA_CAP];
static int nIsola, isolaSuja;
static time_t (*relogioFn)(void);

void addonstats_relogio(time_t (*agora)(void)) {
  pthread_mutex_lock(&trava);
  relogioFn = agora;
  pthread_mutex_unlock(&trava);
}
static time_t agoraSemTrava(void) { return relogioFn ? relogioFn() : time(NULL); }
time_t addonstats_agora(void) {
  time_t t;
  pthread_mutex_lock(&trava);
  t = agoraSemTrava();
  pthread_mutex_unlock(&trava);
  return t;
}
// Tira o vencido; o vetor continua na ordem de criacao.
static void podarIsola(time_t agora) {
  int i, k = 0;
  for (i = 0; i < nIsola; i++)
    if (isola[i].ate > agora) isola[k++] = isola[i];
  if (k != nIsola) isolaSuja = 1;
  nIsola = k;
}
static Isola *achaIsola(const char *chave) {
  for (int i = 0; i < nIsola; i++) if (!strcmp(isola[i].chave, chave)) return &isola[i];
  return NULL;
}
int addonstats_isolar_teto(const char *chave, int segundos, int teto) {
  Isola *r;
  int rc = 0;
  if (!chave || !*chave || strlen(chave) >= sizeof isola[0].chave || segundos <= 0) return 0;
  pthread_mutex_lock(&trava);
  time_t agora = agoraSemTrava();
  podarIsola(agora);
  r = achaIsola(chave);
  if (r && teto > 0) rc = -1;                       // com teto, reisolar nao renova o prazo
  else if (!r && teto > 0 && nIsola >= teto) rc = -2;
  else {
    if (!r && nIsola < ADDONSTATS_ISOLA_CAP) {
      r = &isola[nIsola++];
      snprintf(r->chave, sizeof r->chave, "%s", chave);
    }
    if (r) { r->ate = agora + segundos; isolaSuja = 1; rc = 1; }
  }
  pthread_mutex_unlock(&trava);
  return rc;
}
int addonstats_isolar(const char *chave, int segundos) {
  return addonstats_isolar_teto(chave, segundos, 0) == 1;
}
int addonstats_desisolar(const char *chave) {
  int ok = 0;
  if (!chave || !*chave) return 0;
  pthread_mutex_lock(&trava);
  podarIsola(agoraSemTrava());
  Isola *r = achaIsola(chave);
  if (r) {
    memmove(r, r + 1, sizeof *r * (size_t)(nIsola - 1 - (int)(r - isola)));
    nIsola--; isolaSuja = 1; ok = 1;
  }
  pthread_mutex_unlock(&trava);
  return ok;
}
int addonstats_isolado_s(const char *chave) {
  int s = 0;
  if (!chave || !*chave) return 0;
  pthread_mutex_lock(&trava);
  time_t agora = agoraSemTrava();
  Isola *r = achaIsola(chave);
  if (r && r->ate > agora) s = (int)(r->ate - agora);
  pthread_mutex_unlock(&trava);
  return s;
}
int addonstats_isolados_n(void) {
  int n = 0;
  pthread_mutex_lock(&trava);
  time_t agora = agoraSemTrava();
  for (int i = 0; i < nIsola; i++) if (isola[i].ate > agora) n++;
  pthread_mutex_unlock(&trava);
  return n;
}
int addonstats_isolar_serializar(char *dst, unsigned tam) {
  unsigned usado = 0;
  if (!dst || tam < 2) return 0;
  dst[0] = 0;
  pthread_mutex_lock(&trava);
  time_t agora = agoraSemTrava();
  for (int i = 0; i < nIsola; i++) {
    if (isola[i].ate <= agora) continue;
    int w = snprintf(dst + usado, tam - usado, "%s\t%lld\n", isola[i].chave, (long long)isola[i].ate);
    if (w < 0 || (unsigned)w >= tam - usado) { dst[usado] = 0; break; }
    usado += (unsigned)w;
  }
  pthread_mutex_unlock(&trava);
  return (int)usado;
}
void addonstats_isolar_carregar(const char *texto) {
  const char *p = texto;
  pthread_mutex_lock(&trava);
  time_t agora = agoraSemTrava();
  nIsola = 0; isolaSuja = 0;
  while (p && *p && nIsola < ADDONSTATS_ISOLA_CAP) {
    const char *fim = strchr(p, '\n'), *tab;
    size_t len = fim ? (size_t)(fim - p) : strlen(p);
    char linha[160];
    if (len >= sizeof linha) len = sizeof linha - 1;
    memcpy(linha, p, len); linha[len] = 0;
    tab = strchr(linha, '\t');
    if (tab && tab > linha && (size_t)(tab - linha) < sizeof isola[0].chave) {
      char *e;
      long long ate = strtoll(tab + 1, &e, 10);
      *(char *)tab = 0;
      // So o que ainda vale e cabe no prazo: data muito a frente = relogio
      // errado de quem gravou, e quarentena eterna e pior que nenhuma.
      if (e != tab + 1 && (*e == 0 || *e == '\r') && ate > (long long)agora &&
          ate <= (long long)agora + ADDONSTATS_ISOLA_S && !achaIsola(linha)) {
        snprintf(isola[nIsola].chave, sizeof isola[nIsola].chave, "%.47s", linha);
        isola[nIsola].ate = (time_t)ate;
        nIsola++;
      }
    }
    p = fim ? fim + 1 : NULL;
  }
  pthread_mutex_unlock(&trava);
}
void addonstats_isolar_zerar(void) {
  pthread_mutex_lock(&trava);
  nIsola = 0; isolaSuja = 0;
  pthread_mutex_unlock(&trava);
}

#ifndef NV_ADDONSTATS_PURO
#define ARQ "fonte-latencia.txt"
#define ARQ_ISOLA "fonte-quarentena.txt"
void addonstats_ler(void) {
  char *t = dados_ler(ARQ);
  if (t) { addonstats_carregar(t); free(t); }
  t = dados_ler(ARQ_ISOLA);
  if (t) { addonstats_isolar_carregar(t); free(t); }
}
void addonstats_salvar(void) {
  char *buf;
  int quer;
  pthread_mutex_lock(&trava);
  quer = sujo; sujo = 0;
  pthread_mutex_unlock(&trava);
  if (!quer) return;
  buf = malloc((size_t)ADDONSTATS_MAX * 160);
  if (!buf) return;
  addonstats_serializar(buf, (unsigned)ADDONSTATS_MAX * 160u);
  dados_gravar_leve(ARQ, buf);
  free(buf);
}
// Uma gravacao por vez: sem isto, dois fios podiam gravar fora de ordem e deixar
// no disco a quarentena velha por cima da nova.
static pthread_mutex_t salvaIsolaTrava = PTHREAD_MUTEX_INITIALIZER;
void addonstats_isolar_salvar(void) {
  char buf[ADDONSTATS_ISOLA_CAP * 80];
  int quer;
  pthread_mutex_lock(&salvaIsolaTrava);
  pthread_mutex_lock(&trava);
  podarIsola(agoraSemTrava());
  quer = isolaSuja; isolaSuja = 0;
  pthread_mutex_unlock(&trava);
  if (quer) {
    addonstats_isolar_serializar(buf, sizeof buf);
    dados_gravar_leve(ARQ_ISOLA, buf);
  }
  pthread_mutex_unlock(&salvaIsolaTrava);
}
#endif
