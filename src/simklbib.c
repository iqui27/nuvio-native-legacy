#include "simklbib.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// FORMATO (texto; os corpos vao com o tamanho na frente, entao podem ter
// quebras de linha):
//   nuvio-simkl-biblioteca 1
//   conta <assinatura, 16 hex>
//   atividades <bytes>
//   <corpo>
//   biblioteca <bytes>
//   <corpo>
// Mudar o formato = subir SIMKLBIB_VERSAO; a versao velha cai na leitura cheia.
#define SIMKLBIB_MAGIA  "nuvio-simkl-biblioteca"
#define SIMKLBIB_VERSAO 1

typedef struct { char s[40]; } BibNome;
static BibNome nome(int perfil) {
  BibNome n;
  snprintf(n.s, sizeof n.s, SIMKLBIB_ARQ_FMT, perfil);
  return n;
}

unsigned long long simklbib_conta(const char *token) {
  unsigned long long h = 14695981039346656037ULL;
  if (!token || !*token) return 0;
  for (; *token; token++) h = (h ^ (unsigned char)*token) * 1099511628211ULL;
  return h ? h : 1;
}

void simklbib_liberar(SimklBib *b) {
  if (!b) return;
  free(b->atividades);
  free(b->biblioteca);
  b->atividades = b->biblioteca = NULL;
}

void simklbib_apagar(int perfil) { dados_apagar(nome(perfil).s); }

// "<rotulo> <n>\n" seguido de n bytes e "\n". Devolve uma copia do corpo e
// avanca *p; NULL se o rotulo, o tamanho ou o fim nao batem.
static char *secao(const char **p, const char *fim, const char *rotulo) {
  size_t r = strlen(rotulo), n = 0;
  const char *q = *p;
  char *corpo;
  if ((size_t)(fim - q) <= r || memcmp(q, rotulo, r) || q[r] != ' ') return NULL;
  q += r + 1;
  if (q >= fim || *q < '0' || *q > '9') return NULL;
  for (; q < fim && *q >= '0' && *q <= '9'; q++) {
    if (n > (size_t)(fim - q)) return NULL;   // maior que o arquivo
    n = n * 10 + (size_t)(*q - '0');
  }
  if (q >= fim || *q != '\n') return NULL;
  q++;
  if (!n || n > (size_t)(fim - q) || (size_t)(fim - q) - n < 1 || q[n] != '\n') return NULL;
  corpo = malloc(n + 1);
  if (!corpo) return NULL;
  memcpy(corpo, q, n);
  corpo[n] = 0;
  *p = q + n + 1;
  return corpo;
}

int simklbib_ler(int perfil, unsigned long long conta, SimklBib *b) {
  char *txt, cab[64];
  const char *p, *fim;
  unsigned long long dono = 0;
  int versao = 0, k = 0, ok = 0;
  if (!b) return 0;
  b->atividades = b->biblioteca = NULL;
  if (!conta) return 0;
  txt = dados_ler(nome(perfil).s);
  if (!txt) return 0;
  p = txt;
  fim = txt + strlen(txt);
  snprintf(cab, sizeof cab, "%s %%d\nconta %%16llx\n%%n", SIMKLBIB_MAGIA);
  if (sscanf(p, cab, &versao, &dono, &k) == 2 && k > 0 && versao == SIMKLBIB_VERSAO) {
    p += k;
    if (dono != conta) {
      printf("[simkl] biblioteca p%d: de outro vinculo, descartada\n", perfil);
    } else {
      b->atividades = secao(&p, fim, "atividades");
      if (b->atividades) b->biblioteca = secao(&p, fim, "biblioteca");
      ok = b->biblioteca && p == fim;
      if (!ok) printf("[simkl] biblioteca p%d: arquivo cortado ou ilegivel, descartado\n", perfil);
    }
  } else {
    printf("[simkl] biblioteca p%d: formato desconhecido, descartada\n", perfil);
  }
  free(txt);
  if (!ok) {
    simklbib_liberar(b);
    simklbib_apagar(perfil);
  }
  fflush(stdout);
  return ok;
}

int simklbib_gravar(int perfil, unsigned long long conta, const SimklBib *b) {
  size_t na, nb, tam;
  char *txt;
  size_t u;
  int ok;
  if (!conta || !b || !b->atividades || !b->biblioteca) return 0;
  na = strlen(b->atividades);
  nb = strlen(b->biblioteca);
  if (!na || !nb) return 0;
  tam = na + nb + 160;
  txt = malloc(tam);
  if (!txt) return 0;
  u = (size_t)snprintf(txt, tam, "%s %d\nconta %016llx\natividades %zu\n",
               SIMKLBIB_MAGIA, SIMKLBIB_VERSAO, conta, na);
  memcpy(txt + u, b->atividades, na); u += na;
  u += (size_t)snprintf(txt + u, tam - u, "\nbiblioteca %zu\n", nb);
  memcpy(txt + u, b->biblioteca, nb); u += nb;
  txt[u++] = '\n';
  txt[u] = 0;
  ok = dados_gravar_leve(nome(perfil).s, txt);
  printf("[simkl] biblioteca p%d gravada: %zu KB%s\n", perfil, u / 1024,
         ok ? "" : " (FALHOU)");
  fflush(stdout);
  free(txt);
  return ok;
}
