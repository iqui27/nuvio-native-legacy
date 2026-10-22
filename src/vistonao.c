#include "vistonao.h"
#include "sessao.h"
#include "perfis.h"
#include "dados.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// RELOGIO QUE NAO SE PODE CRER. TV recem-ligada, sem rede, pode acordar em
// 1970 ou anos a frente. Um gesto carimbado em 1970 perderia de QUALQUER visto
// ("o remoto e mais novo"); um carimbado em 2040 barraria tudo ate 2040.
//   - relogio antes de VN_RELOGIO_SAO: a entrada nasce SEM instante
//     (VN_SEM_RELOGIO) e ganha de tudo ate o relogio acertar;
//   - entrada mais de um dia A FRENTE de um relogio sao: e rebaixada para
//     "agora" na primeira consulta. Cobre as duas origens (sem relogio e
//     relogio adiantado) com a mesma linha.
#define VN_RELOGIO_SAO 1704067200000LL        // 01/01/2024 UTC
#define VN_SEM_RELOGIO 9000000000000000LL
#define VN_DIA_MS      86400000LL

typedef struct {
  int   perfil;
  char  id[16];        // a mesma chave de vistoep.c: o id ate o ':'
  short temp, ep;
  long long ms;        // quando a pessoa desmarcou
} Ent;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static Ent *ents;
static int nEnt, capEnt, carregado, avisouTeto;
static char usuario[80];
static long long (*relogio)(void);

static long long agoraMs(void) {
  struct timespec ts;
  if (relogio) return relogio();
  clock_gettime(CLOCK_REALTIME, &ts);
  return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}
void vistonao_relogio(long long (*f)(void)) { relogio = f; }

static void idBase(const char *s, char *d, size_t tam) {
  size_t k = 0;
  d[0] = 0;
  if (!s) return;
  while (s[k] && s[k] != ':' && k + 1 < tam) { d[k] = s[k]; k++; }
  d[k] = 0;
}

static void nomeArquivo(char *d, size_t tam) {
  char u[64];
  size_t k = 0, j = 0;
  while (usuario[k] && j + 1 < sizeof u) {
    char c = usuario[k++];
    if (isalnum((unsigned char)c) || c == '-') u[j++] = c;
  }
  u[j] = 0;
  snprintf(d, tam, "vistonao-%s.txt", u[0] ? u : "anon");
}

// So com a trava.
static void gravar(void) {
  char arq[128], *buf, *p;
  size_t tam = (size_t)nEnt * 64 + 16;
  int i;
  nomeArquivo(arq, sizeof arq);
  if (!nEnt) { dados_apagar(arq); return; }
  buf = (char *)malloc(tam);
  if (!buf) return;
  p = buf;
  *p = 0;
  for (i = 0; i < nEnt; i++) {
    int k = snprintf(p, tam - (size_t)(p - buf), "%d\t%s\t%d\t%d\t%lld\n",
                     ents[i].perfil, ents[i].id, ents[i].temp, ents[i].ep, ents[i].ms);
    if (k < 0 || (size_t)k >= tam - (size_t)(p - buf)) break;
    p += k;
  }
  dados_gravar(arq, buf);
  free(buf);
}

static int caber(int n) {
  Ent *x;
  int novo;
  if (n <= capEnt) return 1;
  novo = capEnt ? capEnt * 2 : 64;
  while (novo < n) novo *= 2;
  if (novo > VISTONAO_MAX) novo = VISTONAO_MAX;
  if (novo < n) return 0;
  x = (Ent *)realloc(ents, sizeof *x * (size_t)novo);
  if (!x) return 0;
  ents = x;
  capEnt = novo;
  return 1;
}

static void lerArquivo(void) {
  char arq[128], *b, *linha;
  nomeArquivo(arq, sizeof arq);
  b = dados_ler(arq);
  if (!b) return;
  for (linha = b; linha && *linha;) {
    char *fim = strchr(linha, '\n'), id[16];
    int perfil = 0, t = -1, e = 0;
    long long ms = 0;
    if (fim) *fim = 0;
    // Linha que nao fecha os cinco campos e ignorada: arquivo cortado no meio
    // de uma gravacao perde a ultima entrada, nao o arquivo.
    if (sscanf(linha, "%d\t%15[^\t]\t%d\t%d\t%lld", &perfil, id, &t, &e, &ms) == 5 &&
        perfil > 0 && id[0] && t >= 0 && t <= 32767 && e >= 1 && e <= 32767 &&
        caber(nEnt + 1)) {
      Ent *x = &ents[nEnt++];
      memset(x, 0, sizeof *x);
      x->perfil = perfil;
      snprintf(x->id, sizeof x->id, "%s", id);
      x->temp = (short)t;
      x->ep = (short)e;
      x->ms = ms;
    }
    linha = fim ? fim + 1 : NULL;
  }
  free(b);
}

// So com a trava. O arquivo e por usuario da conta ("anon" sem conta): trocou
// de usuario, a memoria e a do novo. Nunca falha — sem conta tambem ha gesto.
static void garantir(void) {
  const char *u = sessao_logada() ? sessao_usuario() : "";
  if (!u) u = "";
  if (carregado && !strcmp(usuario, u)) return;
  nEnt = 0;
  snprintf(usuario, sizeof usuario, "%s", u);
  carregado = 1;
  lerArquivo();
  if (nEnt) {
    printf("[vistoep] desmarcados nesta TV: %d episodios guardados\n", nEnt);
    fflush(stdout);
  }
}

static int achar(int perfil, const char *id, int t, int e) {
  int i;
  for (i = 0; i < nEnt; i++)
    if (ents[i].perfil == perfil && ents[i].temp == t && ents[i].ep == e &&
        !strcmp(ents[i].id, id)) return i;
  return -1;
}

static void tirar(int i) {
  memmove(&ents[i], &ents[i + 1], sizeof *ents * (size_t)(nEnt - i - 1));
  nEnt--;
}

// So com a trava. Devolve 1 quando algo mudou.
static int registrar(int perfil, const char *id, int t, int e, long long ms) {
  int i = achar(perfil, id, t, e);
  Ent *x;
  if (i >= 0) {
    if (ents[i].ms == ms) return 0;
    ents[i].ms = ms;      // desmarcou de novo: o gesto mais recente e o que vale
    return 1;
  }
  if (nEnt >= VISTONAO_MAX || !caber(nEnt + 1)) {
    // CHEIO: sai a mais VELHA, de qualquer perfil. E a que tem mais chance de
    // ja nao proteger nada (as fontes tiveram mais tempo para aplicar o
    // remove); recusar a nova seria perder justamente o gesto de agora.
    int k, velho = 0;
    if (nEnt < 1) return 0;
    for (k = 1; k < nEnt; k++) if (ents[k].ms < ents[velho].ms) velho = k;
    if (!avisouTeto) {
      avisouTeto = 1;
      printf("[vistoep] teto de %d episodios desmarcados; as mais velhas dao lugar\n",
             VISTONAO_MAX);
      fflush(stdout);
    }
    tirar(velho);
  }
  x = &ents[nEnt++];
  memset(x, 0, sizeof *x);
  x->perfil = perfil;
  snprintf(x->id, sizeof x->id, "%s", id);
  x->temp = (short)t;
  x->ep = (short)e;
  x->ms = ms;
  return 1;
}

void vistonao_gesto(const char *imdb, const VistoPar *pares, int n, int visto) {
  char id[16];
  int i, mudou = 0, perfil = perfis_ativo();
  long long ms = agoraMs();
  idBase(imdb, id, sizeof id);
  if (!id[0] || perfil < 1) return;
  if (!pares && !visto) return;
  if (!pares) n = 0;
  if (ms < VN_RELOGIO_SAO) ms = VN_SEM_RELOGIO;
  pthread_mutex_lock(&trava);
  garantir();
  // O TITULO INTEIRO marcado: caem todas as dele neste perfil.
  if (!pares)
    for (i = nEnt - 1; i >= 0; i--)
      if (ents[i].perfil == perfil && !strcmp(ents[i].id, id)) { tirar(i); mudou++; }
  for (i = 0; i < n; i++) {
    if (pares[i].episodio < 1 || pares[i].temporada < 0) continue;
    if (visto) {
      int k = achar(perfil, id, pares[i].temporada, pares[i].episodio);
      if (k >= 0) { tirar(k); mudou++; }
    } else {
      mudou += registrar(perfil, id, pares[i].temporada, pares[i].episodio, ms);
    }
  }
  if (mudou) gravar();
  i = nEnt;
  pthread_mutex_unlock(&trava);
  // UMA linha por gesto, e so quando mexeu no arquivo: e a prova, no log da
  // TV, de que a desmarcacao ficou guardada (ou de que marcar de novo a tirou).
  if (mudou && visto)
    printf("[vistoep] %s: marcar de novo solta %d episodios neste perfil (%d); %d guardados\n",
           id, mudou, perfil, i);
  else if (mudou)
    printf("[vistoep] %s: desmarcacao guardada para %d episodios neste perfil (%d); %d guardados\n",
           id, mudou, perfil, i);
  if (mudou) fflush(stdout);
}

int vistonao_barra(const char *imdb, int temporada, int episodio, long long remotoMs) {
  char id[16];
  int k, res = 0, perfil = perfis_ativo();
  idBase(imdb, id, sizeof id);
  if (!id[0] || episodio < 1) return 0;
  pthread_mutex_lock(&trava);
  garantir();
  k = nEnt ? achar(perfil, id, temporada, episodio) : -1;
  if (k >= 0) {
    long long agora = agoraMs();
    // Entrada no futuro de um relogio sao: ver VN_RELOGIO_SAO.
    if (agora >= VN_RELOGIO_SAO && ents[k].ms > agora + VN_DIA_MS) {
      ents[k].ms = agora;
      gravar();
    }
    if (remotoMs > 0 && ents[k].ms < VN_SEM_RELOGIO &&
        remotoMs > ents[k].ms + VISTONAO_FOLGA_MS) {
      // A fonte tem um visto MAIS NOVO que o gesto: ela ganha (ultima vence).
      tirar(k);
      gravar();
      res = -1;
    } else res = 1;
  }
  pthread_mutex_unlock(&trava);
  return res;
}

int vistonao_primeira(const char *imdb, int temporada, int episodio, long long remotoMs,
                      int *pt, int *pe) {
  char id[16];
  int i, achou = 0, bt = 0, be = 0, perfil = perfis_ativo();
  idBase(imdb, id, sizeof id);
  if (!id[0] || episodio < 1 || !pt || !pe) return 0;
  pthread_mutex_lock(&trava);
  garantir();
  for (i = 0; i < nEnt; i++) {
    const Ent *x = &ents[i];
    if (x->perfil != perfil || strcmp(x->id, id) || x->temp < 1) continue;
    if (x->temp > temporada || (x->temp == temporada && x->ep > episodio)) continue;
    // O MESMO CRITERIO DE vistonao_barra, SEM SOLTAR A ENTRADA: remoto mais novo
    // que o gesto (folga incluida) ganhou, e quem a solta e a leitura dos vistos.
    if (remotoMs > 0 && x->ms < VN_SEM_RELOGIO && remotoMs > x->ms + VISTONAO_FOLGA_MS) continue;
    if (!achou || x->temp < bt || (x->temp == bt && x->ep < be)) { bt = x->temp; be = x->ep; achou = 1; }
  }
  pthread_mutex_unlock(&trava);
  if (achou) { *pt = bt; *pe = be; }
  return achou;
}

int vistonao_n(void) {
  int r;
  pthread_mutex_lock(&trava);
  garantir();
  r = nEnt;
  pthread_mutex_unlock(&trava);
  return r;
}

void vistonao_esquecer(void) {
  pthread_mutex_lock(&trava);
  nEnt = 0;
  carregado = 0;
  usuario[0] = 0;
  pthread_mutex_unlock(&trava);
}
