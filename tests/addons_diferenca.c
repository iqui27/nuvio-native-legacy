// O QUE O BOTAO "Sincronizar addons" (#360) DIZ depois da volta da conta.
// addons_diferenca compara a lista de antes com a de depois pela URL: quantos
// entraram, quantos sairam e quantos dos que ficaram mudaram (ligado/desligado
// ou ordem). Um erro aqui vira aviso mentiroso na TV ("ja estao em dia" com um
// addon novo), que e exatamente a queixa do issue. NAO precisa de rede.
//
//   bash tests/addons_diferenca.sh
#include <stdio.h>
// A entrega real tem o harness watchstate_poda.sh; aqui nao ha diario.
void wsenvio_podar(void) {}
void wsenvio_podar_addon(const char *base) { (void)base; }
#include <string.h>
#include "addons.h"
#include "fontecache.h"

// addons_definir_lista esquece o cache de fontes quando a lista muda: aqui
// nao ha cache, so a contagem.
void fontecache_vod_limpar(void) {}

static int falhas;
static void conferir(const char *caso, int e, int s, int m, int ee, int se, int me) {
  if (e != ee || s != se || m != me) {
    printf("FALHOU %s: +%d -%d ~%d (esperado +%d -%d ~%d)\n", caso, e, s, m, ee, se, me);
    falhas++;
  }
}
static AddonRemoto A(const char *url, int ativo) {
  AddonRemoto a;
  memset(&a, 0, sizeof a);
  snprintf(a.nome, sizeof a.nome, "%s", url);
  snprintf(a.url, sizeof a.url, "%s", url);
  a.ativo = ativo;
  return a;
}

int main(void) {
  int e, s, m;
  AddonRemoto antes[4], depois[4];
  antes[0] = A("https://a/manifest.json", 1);
  antes[1] = A("https://b/manifest.json", 1);
  antes[2] = A("https://c/manifest.json", 0);

  // igual: nada mudou
  memcpy(depois, antes, sizeof antes);
  addons_diferenca(antes, 3, depois, 3, &e, &s, &m);
  conferir("igual", e, s, m, 0, 0, 0);

  // um entrou no celular, um saiu
  depois[0] = antes[0]; depois[1] = antes[2]; depois[2] = A("https://d/manifest.json", 1);
  addons_diferenca(antes, 3, depois, 3, &e, &s, &m);
  conferir("entrou-saiu", e, s, m, 1, 1, 0);

  // ligado/desligado no celular: nem entrou nem saiu, mas mudou
  memcpy(depois, antes, sizeof antes);
  depois[2].ativo = 1;
  addons_diferenca(antes, 3, depois, 3, &e, &s, &m);
  conferir("ligou", e, s, m, 0, 0, 1);

  // reordenado: troca de a e b
  depois[0] = antes[1]; depois[1] = antes[0]; depois[2] = antes[2];
  addons_diferenca(antes, 3, depois, 3, &e, &s, &m);
  conferir("ordem", e, s, m, 0, 0, 1);

  // o primeiro sync de uma TV sem lista: tudo entrou
  addons_diferenca(antes, 0, depois, 3, &e, &s, &m);
  conferir("vazia-antes", e, s, m, 3, 0, 0);

  // a conta tirou tudo (o app mantem a local, mas a conta diria isto)
  addons_diferenca(antes, 3, depois, 0, &e, &s, &m);
  conferir("vazia-depois", e, s, m, 0, 3, 0);

  // saidas NULL nao quebram
  addons_diferenca(antes, 3, depois, 3, NULL, NULL, NULL);

  // IDA E VOLTA PELO MODULO: o que addons_exportar devolve depois de
  // addons_definir_lista e o que sync.c compara. A mesma lista de novo (o
  // caso comum do botao) nao conta nada; a URL e comparada como base (sem o
  // /manifest.json), igual para as duas pontas.
  { AddonRemoto um[2], ex1[4], ex2[4];
    int n1, n2;
    um[0] = A("https://a/manifest.json", 1);
    um[1] = A("https://b/manifest.json", 1);
    addons_definir_lista(um, 2);
    n1 = addons_exportar(ex1, 4);
    addons_definir_lista(um, 2);
    n2 = addons_exportar(ex2, 4);
    addons_diferenca(ex1, n1, ex2, n2, &e, &s, &m);
    conferir("mesma-lista", e, s, m, 0, 0, 0);
    um[1] = A("https://e/manifest.json", 1);
    addons_definir_lista(um, 2);
    n2 = addons_exportar(ex2, 4);
    addons_diferenca(ex1, n1, ex2, n2, &e, &s, &m);
    conferir("troca-real", e, s, m, 1, 1, 0); }

  if (falhas) return 1;
  printf("addons_diferenca: ok\n");
  return 0;
}
