#include "catordemcache.h"
#include "catordem.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_NOME_FMT "home-catalog-settings-p%d.json"
#define CACHE_PERFIS 8

static int perfilSeguro(int perfil) {
  return perfil >= 1 && perfil <= CACHE_PERFIS;
}

static int nomeCache(char *dst, size_t tam, int perfil) {
  if (!dst || !tam || !perfilSeguro(perfil)) return 0;
  snprintf(dst, tam, CACHE_NOME_FMT, perfil);
  return 1;
}

// Le uma linha curta do envelope. O corpo JSON comeca depois da terceira
// linha, e fica intacto para catordem_ler interpretar — nao duplicamos aqui o
// contrato da RPC.
static const char *linha(const char *p, char *dst, size_t tam) {
  const char *fim;
  size_t n;
  if (!p || !dst || tam < 1) return NULL;
  fim = strchr(p, '\n');
  if (!fim) return NULL;
  n = (size_t)(fim - p);
  if (n >= tam) n = tam - 1;
  memcpy(dst, p, n);
  dst[n] = 0;
  return fim + 1;
}

int catordem_cache_carregar(int perfil, const char *usuario) {
  char nome[64], dono[256], perfilLinha[32], esperado[32];
  char *arquivo;
  const char *p;
  int mudou;

  catordem_esquecer();
  if (!nomeCache(nome, sizeof nome, perfil) || !usuario || !usuario[0]) return 0;
  arquivo = dados_ler(nome);
  if (!arquivo) return 0;

  p = arquivo;
  p = linha(p, dono, sizeof dono);
  if (!p || strcmp(dono, "NVCATORDER 1")) { free(arquivo); return 0; }
  p = linha(p, dono, sizeof dono);
  if (!p || strncmp(dono, "owner ", 6) || strcmp(dono + 6, usuario)) {
    free(arquivo);
    return 0;
  }
  p = linha(p, perfilLinha, sizeof perfilLinha);
  snprintf(esperado, sizeof esperado, "profile %d", perfil);
  if (!p || strcmp(perfilLinha, esperado)) { free(arquivo); return 0; }

  mudou = catordem_ler(p);
  free(arquivo);
  if (mudou)
    printf("[catordem] ordem restaurada do cache local (perfil %d)\n", perfil);
  return mudou;
}

int catordem_cache_gravar(int perfil, const char *usuario, const char *resposta) {
  char nome[64];
  char *conteudo;
  size_t tam;
  int ok;

  if (!nomeCache(nome, sizeof nome, perfil) || !usuario || !usuario[0] ||
      !resposta || !resposta[0] || strchr(usuario, '\n') || strchr(usuario, '\r'))
    return 0;
  tam = strlen("NVCATORDER 1\nowner \nprofile \n") + strlen(usuario) +
        20 + strlen(resposta) + 1;
  conteudo = (char *)malloc(tam);
  if (!conteudo) return 0;
  snprintf(conteudo, tam, "NVCATORDER 1\nowner %s\nprofile %d\n%s",
           usuario, perfil, resposta);
  ok = dados_gravar(nome, conteudo);
  free(conteudo);
  if (ok) printf("[catordem] ordem guardada no cache local (perfil %d)\n", perfil);
  return ok;
}

void catordem_cache_esquecer(void) {
  char nome[64];
  int perfil;
  for (perfil = 1; perfil <= CACHE_PERFIS; perfil++)
    if (nomeCache(nome, sizeof nome, perfil)) dados_apagar(nome);
}
