#include "fontesmortas.h"
#include "dados.h"
#include <ctype.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define ARQUIVO "fontesmortas.txt"
static char arquivo[80] = ARQUIVO;
#define MAX_MORTAS 128
#define VALIDADE_S (6 * 3600)
typedef struct { uint64_t chave; time_t quando; } Morta;
static Morta mortas[MAX_MORTAS];
static int n, carregado;
static unsigned versao;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

static uint64_t bytes(uint64_t h, const char *s) {
  for (; *s; s++) { h ^= (unsigned char)*s; h *= UINT64_C(1099511628211); }
  return h;
}
static int volatil(const char *p, size_t n) {
  static const char *const nomes[] = {
    "token", "access_token", "expires", "expiry", "exp", "sig", "signature",
    "auth", "authorization", "policy", "key-pair-id"
  };
  for (size_t i = 0; i < sizeof nomes / sizeof *nomes; i++)
    if (strlen(nomes[i]) == n && !strncasecmp(p, nomes[i], n)) return 1;
  return (n >= 6 && !strncasecmp(p, "x-amz-", 6)) ||
         (n >= 7 && !strncasecmp(p, "x-goog-", 7));
}
static uint64_t identidade(const Stream *s) {
  uint64_t h = UINT64_C(14695981039346656037);
  char v[4096], idx[32];
  size_t k = 0;
  if (!s || !s->provedor[0] || (!s->infoHash[0] && !s->url[0])) return 0;
  h = bytes(h, s->provedor);
  h = bytes(h, "|");
  if (s->infoHash[0]) {
    for (const char *p = s->infoHash; *p; p++) v[k++] = (char)tolower((unsigned char)*p);
    v[k] = 0;
    snprintf(idx, sizeof idx, ":%d", s->fileIdx);
    return bytes(bytes(bytes(h, "t:"), v), idx);
  }
  // Host nao distingue caixa. Preservar seletores desconhecidos da query:
  // video, file, episode etc. podem identificar arquivos distintos.
  const char *p = strstr(s->url, "://");
  if (!p) return 0;
  p += 3;
  while (*p && *p != '/' && *p != '?' && *p != '#' && k + 1 < sizeof v)
    { v[k++] = (char)tolower((unsigned char)*p++); }
  while (*p && *p != '?' && *p != '#' && k + 1 < sizeof v) v[k++] = *p++;
  v[k] = 0;
  // Versao nova ignora marcas HTTP antigas, que podiam misturar arquivos.
  h = bytes(bytes(h, "u2:"), v);
  if (*p == '?') {
    p++;
    while (*p && *p != '#') {
      size_t len = strcspn(p, "&#");
      const char *igual = memchr(p, '=', len);
      size_t nomeLen = igual ? (size_t)(igual - p) : len;
      if (len && !volatil(p, nomeLen)) {
        h = bytes(h, "?");
        for (const char *q = p; q < p + len; q++) { h ^= (unsigned char)*q; h *= UINT64_C(1099511628211); }
      }
      p += len;
      if (*p != '&') break;
      p++;
    }
  }
  return h;
}
static int valida(time_t quando, time_t agora) {
  // Relogio errado no boot nao pode apagar a tabela inteira: marca "do
  // futuro" (agora < quando) continua valendo, como em fontepref.c — a TV
  // arranca com o relogio atrasado e so acerta quando a rede sobe.
  return quando > 0 && (agora < quando || agora - quando < VALIDADE_S);
}
static void gravar(void) {
  char buf[MAX_MORTAS * 40 + 32];
  size_t k = 0;
  for (int i = 0; i < n; i++)
    k += (size_t)snprintf(buf + k, sizeof buf - k, "%016llx %lld\n",
                          (unsigned long long)mortas[i].chave, (long long)mortas[i].quando);
  buf[k] = 0;
  dados_gravar(arquivo, buf);
}
static void podar(time_t agora) {
  int k = 0;
  for (int i = 0; i < n; i++) if (valida(mortas[i].quando, agora)) mortas[k++] = mortas[i];
  n = k;
}
// Sem arquivo de dados (dados_dir vazia, .wgt) a tabela vive so na memoria.
// carregado = 1 impede re-leitura por acesso; dados_ler ja e no-op la.
static void carregarSemTrava(void) {
  char *b = dados_ler(arquivo), *p, *linha;
  time_t agora = time(NULL);
  n = 0;
  for (linha = b; linha && *linha && n < MAX_MORTAS; linha = p) {
    unsigned long long chave;
    long long quando;
    p = strchr(linha, '\n');
    if (p) *p++ = 0;
    if (sscanf(linha, "%16llx %lld", &chave, &quando) == 2 && chave &&
        valida((time_t)quando, agora)) mortas[n++] = (Morta){(uint64_t)chave, (time_t)quando};
  }
  carregado = 1;
  if (b) gravar();
  free(b);
}
void fontesmortas_iniciar(void) {
  pthread_mutex_lock(&trava);
  carregarSemTrava();
  versao++;
  pthread_mutex_unlock(&trava);
}
void fontesmortas_definir_escopo(const char *conta, int perfil) {
  char novo[80];
  uint64_t h = bytes(UINT64_C(14695981039346656037), conta ? conta : "");
  snprintf(novo, sizeof novo, "fontesmortas-%016llx-p%d.txt",
           (unsigned long long)h, perfil > 0 ? perfil : 0);
  pthread_mutex_lock(&trava);
  if (strcmp(novo, arquivo)) {
    snprintf(arquivo, sizeof arquivo, "%s", novo);
    memset(mortas, 0, sizeof mortas); n = carregado = 0; versao++;
  }
  pthread_mutex_unlock(&trava);
}
static void garantirSemTrava(void) { if (!carregado) carregarSemTrava(); }
unsigned fontesmortas_versao(void) {
  static time_t ultima;
  time_t agora = time(NULL);
  pthread_mutex_lock(&trava);
  garantirSemTrava();
  if (agora != ultima) {
    int antes = n;
    podar(agora);
    if (n != antes) versao++;
    ultima = agora;
  }
  unsigned v = versao;
  pthread_mutex_unlock(&trava);
  return v;
}
long fontesmortas_idade(const Stream *s) {
  uint64_t chave = identidade(s);
  long idade = -1;
  time_t agora = time(NULL);
  if (!chave) return -1;
  pthread_mutex_lock(&trava);
  garantirSemTrava();
  for (int i = 0; i < n; i++) if (mortas[i].chave == chave && valida(mortas[i].quando, agora)) {
    idade = agora > mortas[i].quando ? (long)(agora - mortas[i].quando) : 0; break;
  }
  pthread_mutex_unlock(&trava);
  return idade;
}
void fontesmortas_marcar(const Stream *s) {
  uint64_t chave = identidade(s);
  time_t agora = time(NULL);
  if (!chave) return;
  pthread_mutex_lock(&trava);
  garantirSemTrava();
  podar(agora);
  int i;
  for (i = 0; i < n && mortas[i].chave != chave; i++);
  if (i == n && n == MAX_MORTAS) {
    i = 0;
    for (int j = 1; j < n; j++) if (mortas[j].quando < mortas[i].quando) i = j;
  } else if (i == n) n++;
  mortas[i] = (Morta){chave, agora};
  versao++;
  gravar();
  pthread_mutex_unlock(&trava);
}
void fontesmortas_limpar(const Stream *s) {
  uint64_t chave = identidade(s);
  if (!chave) return;
  pthread_mutex_lock(&trava);
  garantirSemTrava();
  for (int i = 0; i < n; i++) if (mortas[i].chave == chave) {
    memmove(mortas + i, mortas + i + 1, (size_t)(--n - i) * sizeof *mortas);
    versao++;
    gravar(); break;
  }
  pthread_mutex_unlock(&trava);
}
