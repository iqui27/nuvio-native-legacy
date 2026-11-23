#include "rotahost.h"
#include "proxyts.h"
#include <ctype.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ARQUIVO "rotahost.txt"
#define VALIDADE (7 * 24 * 3600)
#define HOST_MAX 512
typedef struct {
  char host[HOST_MAX];
  int rota, modo, falhasDireto, falhasProxy;
  time_t quando;
} Entrada;
static Entrada tabela[ROTAHOST_MAX];
static int carregado;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static char *(*arqLer)(const char *);
static int (*arqGravar)(const char *, const char *);
static int (*arqApagar)(const char *);

static int host(const char *url, char *saida) {
  const char *p, *fim;
  size_t n;
  if (!url) return 0;
  if (!strncmp(url, PROXYTS_PREFIXO, strlen(PROXYTS_PREFIXO))) url += strlen(PROXYTS_PREFIXO);
  p = strstr(url, "://");
  p = p ? p + 3 : url;
  fim = p + strcspn(p, "/?#");
  for (const char *q = p; q < fim; q++) if (*q == '@') p = q + 1;
  n = (size_t)(fim - p);
  if (!n || n >= HOST_MAX) return 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)p[i];
    if (c <= 32 || c == 127 || c == '\\') return 0;
    saida[i] = (char)tolower(c);
  }
  saida[n] = 0;
  return 1;
}

static void expirar(void) {
  time_t agora = time(NULL);
  for (int i = 0; i < ROTAHOST_MAX; i++)
    if (tabela[i].host[0] && (tabela[i].quando > agora ||
        difftime(agora, tabela[i].quando) >= VALIDADE)) tabela[i].host[0] = 0;
}

static void carregar(void) {
  char *buf, *linha;
  if (carregado) return;
  carregado = 1;
  if (!arqLer || !(buf = arqLer(ARQUIVO))) return;
  linha = buf;
  for (int i = 0; *linha && i < ROTAHOST_MAX;) {
    Entrada e = {0};
    long long quando;
    char *fim = strchr(linha, '\n'), chave[HOST_MAX];
    if (fim) *fim = 0;
    if (sscanf(linha, "%511[^\t]\t%d\t%d\t%d\t%d\t%lld", e.host,
               &e.rota, &e.modo, &e.falhasDireto, &e.falhasProxy, &quando) == 6 &&
        host(e.host, chave) && !strcmp(chave, e.host) &&
        e.rota >= -1 && e.rota <= 1 && e.modo >= -1 && e.modo <= 2 &&
        e.falhasDireto >= 0 && e.falhasProxy >= 0) {
      e.quando = (time_t)quando;
      tabela[i++] = e;
    }
    if (!fim) break;
    linha = fim + 1;
  }
  free(buf);
  expirar();
}

static void gravar(void) {
  static char buf[ROTAHOST_MAX * (HOST_MAX + 96)];
  size_t n = 0;
  expirar();
  if (!arqGravar) return;
  for (int i = 0; i < ROTAHOST_MAX; i++) {
    Entrada *e = &tabela[i];
    if (e->host[0]) n += (size_t)snprintf(buf + n, sizeof buf - n,
      "%s\t%d\t%d\t%d\t%d\t%lld\n", e->host, e->rota, e->modo,
      e->falhasDireto, e->falhasProxy, (long long)e->quando);
  }
  buf[n] = 0;
  arqGravar(ARQUIVO, buf);
}

static Entrada *achar(const char *url, int criar) {
  char chave[HOST_MAX];
  int vaga = -1, velha = 0;
  if (!host(url, chave)) return NULL;
  carregar();
  expirar();
  for (int i = 0; i < ROTAHOST_MAX; i++) {
    if (!strcmp(tabela[i].host, chave)) return &tabela[i];
    if (!tabela[i].host[0] && vaga < 0) vaga = i;
    if (tabela[i].quando < tabela[velha].quando) velha = i;
  }
  if (!criar) return NULL;
  Entrada *e = &tabela[vaga < 0 ? velha : vaga];
  memset(e, 0, sizeof *e);
  strcpy(e->host, chave);
  e->rota = e->modo = -1;
  e->quando = time(NULL);
  return e;
}

void rotahost_definir_arquivo(char *(*ler)(const char *),
    int (*escrever)(const char *, const char *), int (*apagar)(const char *)) {
  pthread_mutex_lock(&trava);
  arqLer = ler; arqGravar = escrever; arqApagar = apagar;
  carregado = 0;
  memset(tabela, 0, sizeof tabela);
  pthread_mutex_unlock(&trava);
}

void rotahost_funcionou(const char *url, int rota, int modo) {
  if (rota < 0 || rota > 1 || modo < 0 || modo > 2) return;
  pthread_mutex_lock(&trava);
  Entrada *e = achar(url, 1);
  if (e) {
    e->rota = rota; e->modo = modo;
    e->falhasDireto = e->falhasProxy = 0;
    e->quando = time(NULL);
    gravar();
  }
  pthread_mutex_unlock(&trava);
}

void rotahost_falhou(const char *url, int rota) {
  if (rota < 0 || rota > 1) return;
  pthread_mutex_lock(&trava);
  Entrada *e = achar(url, 1);
  if (e) {
    int *falhas = rota ? &e->falhasProxy : &e->falhasDireto;
    if (*falhas < INT_MAX) (*falhas)++;
    e->quando = time(NULL);
    gravar();
  }
  pthread_mutex_unlock(&trava);
}

static int lembrado(const char *url, int modo) {
  int valor;
  pthread_mutex_lock(&trava);
  Entrada *e = achar(url, 0);
  if (e && (e->rota == 0 ? e->falhasDireto >= 2 :
            e->rota == 1 && e->falhasProxy >= 2)) e = NULL;
  valor = e ? (modo ? e->modo : e->rota) : -1;
  pthread_mutex_unlock(&trava);
  return valor;
}
int rotahost_rota(const char *url) { return lembrado(url, 0); }
int rotahost_modo(const char *url) { return lembrado(url, 1); }
void rotahost_limpar(void) {
  pthread_mutex_lock(&trava);
  memset(tabela, 0, sizeof tabela);
  carregado = 1;
  if (arqApagar) arqApagar(ARQUIVO);
  pthread_mutex_unlock(&trava);
}

int rotahost_plano(const RotaPlanoEntrada *e, RotaTentativa *saida, int max) {
  RotaTentativa op[3];
  int rota, modo, n = 0, total = 1;
  if (!e || !saida || max <= 0) return 0;
  rota = e->proxyForcado >= 0 ? e->proxyForcado : rotahost_rota(e->url);
  if (rota < 0) rota = e->proxyPadrao;
  rota = e->podeProxy && rota > 0;
  modo = e->modoForcado >= 0 ? e->modoForcado : rotahost_modo(e->url);
  if (modo < 0) modo = e->modoPadrao;
  if (modo < 0 || modo > 2) modo = 0;
  op[0] = (RotaTentativa){modo, rota};
  if (e->podeProxy) op[total++] = (RotaTentativa){modo, !rota};
  if (e->variaModo && e->modoForcado < 0) op[total++] = (RotaTentativa){modo == 0 ? 2 : 0, rota};
  for (int i = 0; i < total && n < max && n < ROTAHOST_TENTATIVAS; i++) {
    int repetida = 0;
    for (int j = 0; j < n; j++)
      if (saida[j].modo == op[i].modo && saida[j].rota == op[i].rota) repetida = 1;
    if (!repetida) saida[n++] = op[i];
  }
  return n;
}
