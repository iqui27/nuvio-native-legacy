#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "catordem.h"
#include "catordemcache.h"

static char *arquivo;
static char nomeArquivo[80];
static int falhas;

char *dados_ler(const char *nome) {
  if (!arquivo || strcmp(nome, nomeArquivo)) return NULL;
  return strdup(arquivo);
}

int dados_gravar(const char *nome, const char *conteudo) {
  snprintf(nomeArquivo, sizeof nomeArquivo, "%s", nome);
  free(arquivo);
  arquivo = strdup(conteudo);
  return arquivo != NULL;
}

int dados_apagar(const char *nome) {
  if (!strcmp(nome, nomeArquivo)) {
    free(arquivo);
    arquivo = NULL;
  }
  return 1;
}

static void confere(const char *descricao, int obtido, int esperado) {
  int ok = obtido == esperado;
  printf("  %-58s %s (obtido %d, esperado %d)\n", descricao,
         ok ? "ok    " : "FALHOU", obtido, esperado);
  if (!ok) falhas++;
}

int main(void) {
  const char *resposta =
    "[{\"settings_json\":{\"items\":["
    "{\"addon_id\":\"xperience\",\"type\":\"movie\","
    "\"catalog_id\":\"foryou\",\"order\":0},"
    "{\"addon_id\":\"cinemeta\",\"type\":\"series\","
    "\"catalog_id\":\"trending\",\"order\":1,\"enabled\":false}]}}]";

  printf("cache local da ordem de catalogos:\n");
  catordem_esquecer();
  confere("resposta remota aceita", catordem_ler(resposta), 1);
  confere("cache do perfil 3 gravado", catordem_cache_gravar(3, "conta-a", resposta), 1);

  // Simula o que acontece num update: a memoria do processo desaparece, mas
  // o arquivo da pasta de dados continua existindo.
  catordem_esquecer();
  confere("ordem volta do cache depois do update",
          catordem_cache_carregar(3, "conta-a"), 1);
  confere("duas chaves restauradas", catordem_n(), 2);
  confere("catalogo oculto restaurado", catordem_oculta("cinemeta_series_trending", ""), 1);

  confere("perfil diferente nao usa cache", catordem_cache_carregar(4, "conta-a"), 0);
  confere("conta diferente nao usa cache", catordem_cache_carregar(3, "conta-b"), 0);
  confere("logout apaga cache", (catordem_cache_esquecer(),
          catordem_cache_carregar(3, "conta-a")), 0);

  free(arquivo);
  printf("\n%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}
