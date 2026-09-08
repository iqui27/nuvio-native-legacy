// A ORDEM DAS FILEIRAS DA HOME, do registro ate o primeiro movimento.
//
// Relato: "quando mexe na reordenacao da home ele caga, ele joga pra baixo o do
// topo e nao ta salvando direito". A causa nao era o movimento: era o que
// acontece NO PRIMEIRO movimento. `linhas[]` nascia na ordem de REGISTRO — que
// e a ordem em que cada fileira apareceu — e as fixas ("Continuar assistindo",
// "Amigos assistindo") sao registradas por home.c depois das de catalogo.
// Enquanto ninguem reordena, fil_unir devolve a identidade e isso nao aparece;
// no primeiro fil_mover a ordem local vira autoridade e a home inteira salta
// para a ordem de registro, derrubando as fixas para o fim.
#include "fileiras.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Dubles: o teste nao sobe dados.c nem toca disco. O caminho devolve NULL de
// proposito — assim carregar() sai cedo e a lista comeca vazia, que e o estado
// de quem nunca abriu o app.
int dados_gravar(const char *nome, const char *conteudo) {
  (void)nome; (void)conteudo; return 1;
}
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  (void)dst; (void)tam; (void)nome; return NULL;
}

// A ordem que a HOME desenha: fixas primeiro, catalogo depois.
static const char *DA_HOME[] = { "continuar", "amigos", "catA", "catB", "catC" };
#define N_HOME 5

static void conferir(const char *quando, const char *const *esperado, int n) {
  int i;
  for (i = 0; i < n; i++)
    if (strcmp(fil_chave(i), esperado[i])) {
      printf("FALHOU (%s): posicao %d e \"%s\", esperava \"%s\"\n",
             quando, i, fil_chave(i), esperado[i]);
      assert(0);
    }
}

// A home pergunta a fil_unir em que ordem desenhar; devolve as chaves ja
// ordenadas, que e o que a pessoa ve na tela.
static void ordemDaHome(const char **saida) {
  int ord[16], q, k;
  q = fil_unir(DA_HOME, N_HOME, ord, 16);
  assert(q == N_HOME);
  for (k = 0; k < q; k++) saida[k] = DA_HOME[ord[k]];
}

int main(void) {
  const char *vista[N_HOME];
  int i;

  // REGISTRO NA ORDEM ERRADA de proposito: e a ordem real do app, em que as
  // fileiras de catalogo entram pela descoberta e as fixas por home.c depois.
  fil_registrar("catA", "Popular - Filme");
  fil_registrar("catB", "Em alta");
  fil_registrar("catC", "For You");
  fil_registrar("continuar", "Continuar assistindo");
  fil_registrar("amigos", "Amigos assistindo");
  assert(fil_n() == N_HOME);

  // Sem ordem local, a home nao muda: fil_unir devolve a identidade.
  ordemDaHome(vista);
  for (i = 0; i < N_HOME; i++) assert(!strcmp(vista[i], DA_HOME[i]));
  puts("ok  sem ordem local a home fica como a descoberta entregou");

  // 1. A LISTA DE AJUSTES PASSA A ESPELHAR A HOME. Antes disto ela mostrava
  //    catA, catB, catC, continuar, amigos — uma lista que ninguem ve na home.
  fil_espelhar_ordem(DA_HOME, N_HOME);
  conferir("depois de espelhar", DA_HOME, N_HOME);
  puts("ok  Ajustes mostra a mesma ordem da home");

  // 2. O PRIMEIRO MOVIMENTO MOVE UMA FILEIRA, e nao reembaralha tudo. Sobe
  //    "catA" (indice 2) uma posicao: ele troca com "amigos" e mais nada muda.
  //    Era aqui que "continuar" e "amigos" desabavam para o fim.
  assert(fil_mover(2, -1) == 1);
  { const char *esperado[] = { "continuar", "catA", "amigos", "catB", "catC" };
    conferir("depois de mover", esperado, N_HOME);
    ordemDaHome(vista);
    for (i = 0; i < N_HOME; i++)
      if (strcmp(vista[i], esperado[i])) {
        printf("FALHOU (home): posicao %d e \"%s\", esperava \"%s\"\n",
               i, vista[i], esperado[i]);
        assert(0);
      }
    assert(!strcmp(vista[0], "continuar")); }
  puts("ok  o primeiro movimento move uma fileira so");

  // 3. DEPOIS DE REORDENAR, quem manda e a pessoa: espelhar vira no-op, senao
  //    a proxima remontagem da home desfaria a escolha dela.
  fil_espelhar_ordem(DA_HOME, N_HOME);
  { const char *esperado[] = { "continuar", "catA", "amigos", "catB", "catC" };
    conferir("espelhar depois de mover", esperado, N_HOME); }
  puts("ok  espelhar nao desfaz a escolha da pessoa");

  // 4. FILEIRA NOVA nao se perde nem rouba o topo: entra no fim.
  fil_registrar("catD", "Netflix");
  { const char *comD[] = { "continuar", "amigos", "catA", "catB", "catC", "catD" };
    int ord[16], q, k;
    q = fil_unir(comD, 6, ord, 16);
    assert(q == 6);
    assert(!strcmp(comD[ord[0]], "continuar"));
    for (k = 0; k < q; k++) if (!strcmp(comD[ord[k]], "catD")) assert(k == 5); }
  puts("ok  fileira nova entra no fim");

  puts("fileiras: tudo ok");
  return 0;
}
