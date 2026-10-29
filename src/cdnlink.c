// Ver cdnlink.h para o porque.
#include "cdnlink.h"
#include "rede.h"
#define NV_DUBLE_SESSAO
#include "perfisessao.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define CDN_MAX 8
#define CDN_URL 4096

typedef struct {
  char orig[CDN_URL];
  char final[CDN_URL];
  char cabecalhos[2048];
  uint64_t sessao;
  unsigned em;    // relogio() quando foi resolvido
  int usado;
} Entrada;

static Entrada cache[CDN_MAX];
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static unsigned (*relogioTeste)(void);

static unsigned relogio(void) {
  struct timespec ts;
  if (relogioTeste) return relogioTeste();
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned)((unsigned long long)ts.tv_sec * 1000ULL + (unsigned long long)ts.tv_nsec / 1000000ULL);
}

void cdnlink_relogio_teste(unsigned (*fn)(void)) { relogioTeste = fn; }
void cdnlink_zerar(void) {
  pthread_mutex_lock(&trava);
  memset(cache, 0, sizeof cache);
  pthread_mutex_unlock(&trava);
}

static int ehHttp(const char *u) {
  return u && (!strncasecmp(u, "http://", 7) || !strncasecmp(u, "https://", 8));
}

const char *cdnlink_host(const char *url, char *b, unsigned n) {
  const char *p, *f;
  size_t k;
  if (!b || !n) return "";
  b[0] = 0;
  if (!url) return b;
  p = strstr(url, "://");
  p = p ? p + 3 : url;
  // credenciais no endereco (user:pass@host) nunca vao para o log
  f = p + strcspn(p, "/?#");
  { const char *at = memchr(p, '@', (size_t)(f - p)); if (at) p = at + 1; }
  k = strcspn(p, ":/?#");
  if (k >= n) k = n - 1;
  memcpy(b, p, k);
  b[k] = 0;
  return b;
}

// O caminho de `u` (sem consulta), em minusculas, termina em `ext`?
static int caminhoTermina(const char *u, const char *ext) {
  const char *p = strstr(u, "://"), *fim;
  size_t n, e = strlen(ext);
  p = p ? p + 3 : u;
  p += strcspn(p, "/");
  fim = p + strcspn(p, "?#");
  n = (size_t)(fim - p);
  return n >= e && !strncasecmp(fim - e, ext, e);
}

int cdnlink_origem_ok(const char *orig) {
  char h[256];
  if (!ehHttp(orig) || strlen(orig) >= CDN_URL) return 0;
  cdnlink_host(orig, h, sizeof h);
  if (!h[0] || !strcasecmp(h, "localhost") || !strncmp(h, "127.", 4) || !strcmp(h, "[::1]")) return 0;
  if (caminhoTermina(orig, ".m3u8") || caminhoTermina(orig, ".m3u") || caminhoTermina(orig, ".mpd")) return 0;
  // JA E O LINK DO CDN do TorBox (o requestdl da lista de arquivos, partes.h):
  // seguir o redirecionamento so atrasava a abertura.
  { size_t L = strlen(h);
    if (L >= 10 && !strcasecmp(h + L - 10, ".tb-cdn.st")) return 0; }
  return 1;
}

int cdnlink_aceita(const char *orig, const char *final) {
  char a[256], b[256];
  if (!cdnlink_origem_ok(orig) || !ehHttp(final) || strlen(final) >= CDN_URL) return 0;
  if (!strcmp(orig, final)) return 0;
  cdnlink_host(orig, a, sizeof a);
  cdnlink_host(final, b, sizeof b);
  if (!b[0] || !strcasecmp(a, b)) return 0;
  // destino local nunca (um addon que redireciona ao 127.0.0.1 da TV)
  if (!strcasecmp(b, "localhost") || !strncmp(b, "127.", 4)) return 0;
  return 1;
}

void cdnlink_lembrar(const char *orig, const char *final, const char *cabecalhos, uint64_t sessao) {
  int i, alvo = -1;
  unsigned agora, maisVelho = 0;
  if (!cabecalhos) cabecalhos = "";
  if (!cdnlink_aceita(orig, final) || strlen(cabecalhos) >= sizeof cache[0].cabecalhos) return;
  agora = relogio();
  if (!nv_perfil_sessao_travar(sessao)) return;
  pthread_mutex_lock(&trava);
  for (i = 0; i < CDN_MAX; i++) if (cache[i].usado && cache[i].sessao == sessao &&
      !strcmp(cache[i].orig, orig) && !strcmp(cache[i].cabecalhos, cabecalhos)) { alvo = i; break; }
  if (alvo < 0) for (i = 0; i < CDN_MAX; i++) if (!cache[i].usado) { alvo = i; break; }
  if (alvo < 0) for (i = 0; i < CDN_MAX; i++) {
    unsigned idade = agora - cache[i].em;
    if (alvo < 0 || idade > maisVelho) { alvo = i; maisVelho = idade; }
  }
  snprintf(cache[alvo].orig, sizeof cache[alvo].orig, "%s", orig);
  snprintf(cache[alvo].final, sizeof cache[alvo].final, "%s", final);
  snprintf(cache[alvo].cabecalhos, sizeof cache[alvo].cabecalhos, "%s", cabecalhos);
  cache[alvo].sessao = sessao;
  cache[alvo].em = agora;
  cache[alvo].usado = 1;
  pthread_mutex_unlock(&trava);
  nv_perfil_sessao_destravar();
}

int cdnlink_recente(const char *orig, const char *cabecalhos, unsigned maxMs, char *dst, unsigned tam, unsigned *idadeMs) {
  int i, ok = 0;
  unsigned agora;
  uint64_t sessao = nv_perfil_sessao_ler();
  if (!cabecalhos) cabecalhos = "";
  if (dst && tam) dst[0] = 0;
  if (!orig || !*orig || !dst || !tam) return 0;
  agora = relogio();
  pthread_mutex_lock(&trava);
  for (i = 0; i < CDN_MAX; i++) {
    unsigned idade;
    if (!cache[i].usado || cache[i].sessao != sessao || strcmp(cache[i].orig, orig) ||
        strcmp(cache[i].cabecalhos, cabecalhos)) continue;
    idade = agora - cache[i].em;
    if (idade <= maxMs && strlen(cache[i].final) < tam) {
      memcpy(dst, cache[i].final, strlen(cache[i].final) + 1);
      if (idadeMs) *idadeMs = idade;
      ok = 1;
    }
    break;
  }
  pthread_mutex_unlock(&trava);
  return ok && nv_perfil_sessao_atual(sessao);
}

int cdnlink_resolver(const char *orig, const char *cabecalhos, int segundos,
                     char *dst, unsigned tam, unsigned *ms) {
  const char *vetor[8];
  char copia[2048], fim[CDN_URL], ho[256], hf[256];
  int nc = 0, http = 0, ok;
  unsigned t0 = relogio(), dur;
  uint64_t sessao = nv_perfil_sessao_ler();
  if (dst && tam) dst[0] = 0;
  if (ms) *ms = 0;
  if (!dst || !tam || !cdnlink_origem_ok(orig)) return 0;
  if (cabecalhos && *cabecalhos) {
    char *l, *ctx = NULL;
    snprintf(copia, sizeof copia, "%s", cabecalhos);
    for (l = strtok_r(copia, "\n", &ctx); l && nc < 7; l = strtok_r(NULL, "\n", &ctx))
      if (*l && strncasecmp(l, "X-Nuvio-", 8)) vetor[nc++] = l;
  }
  vetor[nc] = NULL;
  ok = rede_url_final_tipo(orig, segundos, nc ? vetor : NULL, fim, sizeof fim, &http, NULL, 0, NULL);
  dur = relogio() - t0;
  if (ms) *ms = dur;
  cdnlink_host(orig, ho, sizeof ho);
  if (!ok) {
    printf("[fonte-cdn] could not resolve %s in %u ms (HTTP %d): the player gets the add-on link\n", ho, dur, http);
    fflush(stdout);
    return 0;
  }
  cdnlink_host(fim, hf, sizeof hf);
  if (!nv_perfil_sessao_atual(sessao)) return 0;
  if (!cdnlink_aceita(orig, fim) || strlen(fim) >= tam) {
    printf("[fonte-cdn] %s does not redirect to another host (%u ms): the player gets the add-on link\n", ho, dur);
    fflush(stdout);
    return 0;
  }
  memcpy(dst, fim, strlen(fim) + 1);
  cdnlink_lembrar(orig, fim, cabecalhos, sessao);
  printf("[fonte-cdn] resolved %s -> %s in %u ms\n", ho, hf, dur);
  fflush(stdout);
  return 1;
}

unsigned cdnlink_validade_ms(void) {
  return CDNLINK_VALIDADE_MS;
}
