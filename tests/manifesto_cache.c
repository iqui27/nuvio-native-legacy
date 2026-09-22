// Cache UNICA de manifesto (desc_manifesto_cache_obter/guardar, descoberta.h).
//
// Nasceu da unificacao entre a sonda de capacidades de addons.c
// (addons_sondar_manifestos) e o download de manifesto da descoberta
// (descoberta.c: maniLargar/fioManifesto). Os dois passaram a usar a MESMA
// tabela (maniCache, protegida por maniTrava dentro de descoberta.c); este
// teste prova o contrato que os dois lados dependem, sem precisar montar a
// arvore de addons nem falar com a rede de verdade.
//
// SO LINKA descoberta.c: nenhuma das duas funcoes publicas chama rede,
// addons.c, SDL, ajustes, fileiras, colecoes, trakt ou nuvem — so a trava e a
// tabela estaticas do proprio arquivo. O link com -dead_strip (ver
// manifesto_cache.sh, mesma receita de tests/homepos.sh) descarta o resto de
// descoberta.c que nao e alcancado a partir daqui, entao nenhum destes
// simbolos externos precisa de stub.
#include "../src/descoberta.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// SIMULA UM CHAMADOR (a sonda OU a descoberta): consulta a cache; em caso de
// falta, "baixa" (na verdade so incrementa `downloads` e fabrica um corpo) e
// guarda o resultado para o proximo chamador. E exatamente o par de chamadas
// que sondar() (addons.c) e maniLargar()/fioManifesto() (descoberta.c) fazem
// hoje, so que sem rede de verdade — dai o teste provar "mesma url e versao =
// um download" sem precisar de um stub de rede_baixar ligado ao resto do app.
static int downloads;

static char *pedir(const char *url, unsigned versao) {
  char *corpo = desc_manifesto_cache_obter(url, versao);
  if (corpo) return corpo;
  downloads++;
  { char buf[64];
    snprintf(buf, sizeof buf, "{\"id\":\"addon-%u\"}", versao);
    corpo = strdup(buf); }
  desc_manifesto_cache_guardar(url, versao, corpo);
  return corpo;
}

int main(void) {
  char *a, *b, *c;

  // Duas "chamadas" com a MESMA url e versao — uma so gera download. E o caso
  // que motivou a unificacao: addonsui.c/guia.c chamando addons_sondar_
  // manifestos() depois de a descoberta ja ter baixado o mesmo addon nesta
  // mesma versao de lista.
  downloads = 0;
  a = pedir("https://addon/manifest.json", 7);
  assert(downloads == 1);
  b = pedir("https://addon/manifest.json", 7);
  assert(downloads == 1);
  assert(a && b && !strcmp(a, b));
  free(a); free(b);

  // URL diferente: nao acha nada da primeira, gera o SEU proprio download.
  downloads = 0;
  a = pedir("https://um/manifest.json", 1);
  b = pedir("https://outro/manifest.json", 1);
  assert(downloads == 2);
  free(a); free(b);

  // MESMA url, VERSAO diferente: a lista de addons mudou (ligar/desligar,
  // instalar, remover) e a entrada antiga fica stale — tem de baixar de novo,
  // nunca entregar o corpo da versao velha.
  downloads = 0;
  a = pedir("https://addon/manifest.json", 1);
  b = pedir("https://addon/manifest.json", 2);
  assert(downloads == 2);
  assert(strcmp(a, b) != 0);
  free(a); free(b);

  // desc_manifesto_cache_guardar NAO TOMA POSSE: quem chama continua dono do
  // corpo original e pode livre-lo na hora, sem que a cache fique com um
  // ponteiro pendurado (use-after-free se a copia nao fosse de verdade).
  { char *original = strdup("{\"id\":\"x\"}");
    desc_manifesto_cache_guardar("https://posse/manifest.json", 9, original);
    free(original);   // se guardar() tivesse tomado posse, isto seria duplo free adiante
    c = desc_manifesto_cache_obter("https://posse/manifest.json", 9);
    assert(c && !strcmp(c, "{\"id\":\"x\"}"));
    free(c); }

  // Corpo NULL ou vazio: guardar() nao registra nada (nem derruba o processo).
  desc_manifesto_cache_guardar("https://vazio/manifest.json", 1, NULL);
  desc_manifesto_cache_guardar("https://vazio/manifest.json", 1, "");
  assert(desc_manifesto_cache_obter("https://vazio/manifest.json", 1) == NULL);

  // Url nunca guardada: miss limpo, sem lixo de memoria.
  assert(desc_manifesto_cache_obter("https://nunca/manifest.json", 1) == NULL);

  printf("PASS: cache unica de manifesto (obter/guardar) — %d asserts\n", 1);
  return 0;
}
