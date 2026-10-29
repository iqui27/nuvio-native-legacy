// Ver recresp.h.
#include "recresp.h"
#include "dados.h"
#include "perfis.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int perfilLido = -1;
static const char *arquivo(void) {
  static char nome[64]; // todas as chamadas estao sob trava
  if (perfilLido == 1) return "recomendacoes-respostas.txt";
  snprintf(nome, sizeof nome, "recomendacoes-respostas-p%d.txt", perfilLido);
  return nome;
}
#define RR_ARQ arquivo()

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static RecResp lista[RECRESP_MAX];
static int     nLista, lido;
static unsigned revisao = 1;

// --- pura ----------------------------------------------------------------------------

// So o que o servidor guarda (a-z0-9 e espaco, minusculo): a frase na TV e a
// que chega na outra ponta, e nao uma que o servidor reescreve depois.
static void limparTexto(char *dst, size_t tam, const char *src) {
  size_t k = 0;
  int espaco = 0;
  if (!tam) return;
  for (; src && *src && k + 1 < tam && k < RECRESP_TEXTO_MAX; src++) {
    unsigned char c = (unsigned char)*src;
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      if (espaco && k) dst[k++] = ' ';
      espaco = 0;
      if (k + 1 < tam) dst[k++] = (char)c;
    } else espaco = 1;
  }
  dst[k] = 0;
}

int recresp_aplicar(RecResp *r, int ev, int reacao, const char *texto, long long agora) {
  RecResp antes;
  char t[RECRESP_TEXTO_MAX + 4];
  if (!r || r->rec <= 0) return 0;
  antes = *r;
  if (!r->quando && !r->assistida && !r->respondida) r->reacao = RECRESP_SEM_REACAO;
  switch (ev) {
    case RECRESP_EV_ASSISTIU:
      r->assistida = 1;
      break;
    case RECRESP_EV_RESPONDEU:
      r->assistida = 1;
      r->respondida = 1;
      if (reacao >= -1 && reacao <= 1) r->reacao = reacao;
      limparTexto(t, sizeof t, texto);
      if (t[0]) snprintf(r->texto, sizeof r->texto, "%s", t);
      break;
    case RECRESP_EV_PULOU:
      r->assistida = 1;
      r->respondida = 1;
      break;
    default:
      return 0;
  }
  if (antes.assistida == r->assistida && antes.respondida == r->respondida &&
      antes.reacao == r->reacao && !strcmp(antes.texto, r->texto) && antes.quando)
    return 0;
  r->quando = agora;
  r->versao++;
  return 1;
}

int recresp_mesclar(RecResp *r, int terminou, int reacao, const char *texto,
                    long long respondido, long long agora) {
  RecResp antes;
  char t[RECRESP_TEXTO_MAX + 4];
  int servidorNovo;
  if (!r || r->rec <= 0) return 0;
  if (r->versao > r->enviada) return 0;     // mudanca daqui ainda nao enviada: vence
  antes = *r;
  if (!r->quando && !r->assistida && !r->respondida) r->reacao = RECRESP_SEM_REACAO;
  servidorNovo = respondido > r->quando;
  if (terminou) r->assistida = 1;
  if (respondido > 0) { r->assistida = 1; r->respondida = 1; }
  if (reacao >= -1 && reacao <= 1 && (servidorNovo || r->reacao == RECRESP_SEM_REACAO))
    r->reacao = reacao;
  limparTexto(t, sizeof t, texto);
  if (t[0] && (servidorNovo || !r->texto[0])) snprintf(r->texto, sizeof r->texto, "%s", t);
  if (antes.assistida == r->assistida && antes.respondida == r->respondida &&
      antes.reacao == r->reacao && !strcmp(antes.texto, r->texto) && antes.quando)
    return 0;
  if (respondido > r->quando) r->quando = respondido;
  if (!r->quando) r->quando = agora;
  return 1;
}

static void jsonEsc(char *dst, size_t tam, const char *s) {
  size_t k = 0;
  for (; s && *s && k + 7 < tam; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') { dst[k++] = '\\'; dst[k++] = (char)c; }
    else if (c >= 0x20) dst[k++] = (char)c;
  }
  if (tam) dst[k < tam ? k : tam - 1] = 0;
}

size_t recresp_json(const RecResp *r, char *dst, size_t tam) {
  char esc[RECRESP_TEXTO_MAX * 2 + 8], reac[8];
  int n;
  if (!r || !dst || !tam) return 0;
  jsonEsc(esc, sizeof esc, r->texto);
  if (r->reacao >= -1 && r->reacao <= 1) snprintf(reac, sizeof reac, "%d", r->reacao);
  else snprintf(reac, sizeof reac, "null");
  n = snprintf(dst, tam, "{\"id\":%lld,\"reacao\":%s,\"texto\":\"%s\"}", r->rec, reac, esc);
  if (n < 0 || (size_t)n >= tam) { dst[0] = 0; return 0; }
  return (size_t)n;
}

// --- arquivo ------------------------------------------------------------------------
//
// rec TAB assistida TAB respondida TAB reacao TAB quando TAB versao TAB enviada
// TAB texto. O texto por ULTIMO (a-z0-9 e espaco: nunca tem TAB).

static void carregar(void) {
  char *b, *p;
  int perfil = perfis_ativo();
  if (lido && perfilLido == perfil) return;
  perfilLido = perfil;
  lido = 1;
  nLista = 0;
  b = dados_ler(RR_ARQ);
  for (p = b; p && *p && nLista < RECRESP_MAX;) {
    char *fim = strchr(p, '\n');
    RecResp r;
    char texto[RECRESP_TEXTO_MAX + 4] = "";
    if (fim) *fim = 0;
    memset(&r, 0, sizeof r);
    if (*p != '#' &&
        sscanf(p, "%lld\t%d\t%d\t%d\t%lld\t%u\t%u\t%63[^\n]", &r.rec, &r.assistida,
               &r.respondida, &r.reacao, &r.quando, &r.versao, &r.enviada, texto) >= 7 &&
        r.rec > 0) {
      limparTexto(r.texto, sizeof r.texto, texto);
      lista[nLista++] = r;
    }
    if (!fim) break;
    p = fim + 1;
  }
  free(b);
}

static void gravar(void) {
  static char buf[RECRESP_MAX * 140 + 64];
  size_t k = 0;
  int i;
  k += (size_t)snprintf(buf, sizeof buf, "# nuvio respostas v1\n");
  for (i = 0; i < nLista && k < sizeof buf; i++)
    k += (size_t)snprintf(buf + k, sizeof buf - k, "%lld\t%d\t%d\t%d\t%lld\t%u\t%u\t%s\n",
                          lista[i].rec, lista[i].assistida, lista[i].respondida,
                          lista[i].reacao, lista[i].quando, lista[i].versao,
                          lista[i].enviada, lista[i].texto);
  dados_gravar(RR_ARQ, buf);
}

static RecResp *achar(long long rec) {
  int i;
  carregar();
  for (i = 0; i < nLista; i++) if (lista[i].rec == rec) return &lista[i];
  return NULL;
}

// Cria ou devolve a linha. Cheia: sai a mais velha ja enviada (senao a mais velha).
static RecResp *linhaDe(long long rec) {
  RecResp *r = achar(rec);
  if (r) return r;
  if (nLista >= RECRESP_MAX) {
    int i, sai = -1;
    for (i = 0; i < nLista; i++)
      if (lista[i].versao == lista[i].enviada &&
          (sai < 0 || lista[i].quando < lista[sai].quando)) sai = i;
    if (sai < 0) sai = 0;
    memmove(lista + sai, lista + sai + 1, sizeof lista[0] * (size_t)(nLista - sai - 1));
    nLista--;
  }
  r = &lista[nLista++];
  memset(r, 0, sizeof *r);
  r->rec = rec;
  r->reacao = RECRESP_SEM_REACAO;
  return r;
}

static void gesto(long long rec, int ev, int reacao, const char *texto) {
  if (rec <= 0) return;
  pthread_mutex_lock(&trava);
  { RecResp *r = linhaDe(rec);
    if (recresp_aplicar(r, ev, reacao, texto, (long long)time(NULL))) {
      gravar();
      revisao++;
      printf("[recresp] rec %lld: assistida %d respondida %d reacao %d%s\n", rec,
             r->assistida, r->respondida, r->reacao, r->texto[0] ? " +mensagem" : "");
      fflush(stdout);
    } }
  pthread_mutex_unlock(&trava);
}

// --- API ---------------------------------------------------------------------------

int recresp_ler(long long rec, RecResp *saida) {
  RecResp *r;
  int ok = 0;
  pthread_mutex_lock(&trava);
  r = achar(rec);
  if (r) { if (saida) *saida = *r; ok = 1; }
  pthread_mutex_unlock(&trava);
  return ok;
}
int recresp_assistida(long long rec) { RecResp r; return recresp_ler(rec, &r) && r.assistida; }
int recresp_respondida(long long rec) { RecResp r; return recresp_ler(rec, &r) && r.respondida; }
void recresp_marcar_assistida(long long rec) { gesto(rec, RECRESP_EV_ASSISTIU, RECRESP_SEM_REACAO, NULL); }
void recresp_responder(long long rec, int reacao, const char *texto) {
  gesto(rec, RECRESP_EV_RESPONDEU, reacao, texto);
}
void recresp_pular(long long rec) { gesto(rec, RECRESP_EV_PULOU, RECRESP_SEM_REACAO, NULL); }

void recresp_do_servidor(long long rec, int terminou, int reacao, const char *texto,
                         long long respondido) {
  if (rec <= 0) return;
  if (!terminou && respondido <= 0 && !(reacao >= -1 && reacao <= 1)) return;
  pthread_mutex_lock(&trava);
  { RecResp *r = linhaDe(rec);
    if (recresp_mesclar(r, terminou, reacao, texto, respondido, (long long)time(NULL))) {
      gravar();
      revisao++;
      printf("[recresp] rec %lld do servidor: assistida %d respondida %d reacao %d\n", rec,
             r->assistida, r->respondida, r->reacao);
      fflush(stdout);
    } }
  pthread_mutex_unlock(&trava);
}

unsigned recresp_revisao(void) {
  unsigned v;
  pthread_mutex_lock(&trava); v = revisao; pthread_mutex_unlock(&trava);
  return v;
}

int recresp_pendente(char *corpo, size_t tam, long long *rec, unsigned *versao) {
  int i, ok = 0;
  pthread_mutex_lock(&trava);
  carregar();
  for (i = 0; i < nLista; i++) {
    if (lista[i].versao == lista[i].enviada) continue;
    if (!recresp_json(&lista[i], corpo, tam)) continue;
    if (rec) *rec = lista[i].rec;
    if (versao) *versao = lista[i].versao;
    ok = 1;
    break;
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

void recresp_confirmar(long long rec, unsigned versao) {
  pthread_mutex_lock(&trava);
  { RecResp *r = achar(rec);
    // Uma mudanca feita DURANTE o envio subiu a versao: ela continua pendente.
    if (r && versao > r->enviada && versao <= r->versao) { r->enviada = versao; gravar(); } }
  pthread_mutex_unlock(&trava);
}

void recresp_esquecer(void) {
  pthread_mutex_lock(&trava);
  perfilLido = perfis_ativo();
  nLista = 0;
  lido = 1;
  dados_gravar(RR_ARQ, "# nuvio respostas v1\n");
  revisao++;
  pthread_mutex_unlock(&trava);
}

void recresp_abrir_sessao(void) {
  pthread_mutex_lock(&trava);
  nLista = 0; lido = 0; perfilLido = -1; revisao++;
  pthread_mutex_unlock(&trava);
}
