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
  fil_registrar("catA", "Popular - Filme", "Cinemeta", "movie", -1);
  fil_registrar("catB", "Em alta", "TMDB", "movie", -1);
  fil_registrar("catC", "For You", "Xperience", "series", -1);
  fil_registrar("continuar", "Continuar assistindo", "", "", 9);
  fil_registrar("amigos", "Amigos assistindo", "", "", 6);
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
  fil_registrar("catD", "Netflix", "AIOStreams", "series", 12);
  { const char *comD[] = { "continuar", "amigos", "catA", "catB", "catC", "catD" };
    int ord[16], q, k;
    q = fil_unir(comD, 6, ord, 16);
    assert(q == 6);
    assert(!strcmp(comD[ord[0]], "continuar"));
    for (k = 0; k < q; k++) if (!strcmp(comD[ord[k]], "catD")) assert(k == 5); }
  puts("ok  fileira nova entra no fim");

  // 5. DE ONDE A FILEIRA VEIO. O relato: "mostre o que e lista e o que e
  //    catalogo". A resposta sai da CHAVE, entao ela se testa sem SDL, sem rede
  //    e sem disco — e sem depender de quem registrou primeiro.
  assert(fil_origem_de("continue_watching") == FIL_ORIGEM_APP);
  assert(fil_origem_de("social_activity")   == FIL_ORIGEM_APP);
  assert(fil_origem_de("last_session")      == FIL_ORIGEM_APP);
  assert(fil_origem_de("collection_a24")    == FIL_ORIGEM_COLECAO);
  // Chave que so COMECA parecida nao e colecao: o prefixo tem de ser exato.
  assert(fil_origem_de("collections_movie_x") == FIL_ORIGEM_CATALOGO);
  assert(fil_origem_de("com.linvo.cinemeta_movie_top") == FIL_ORIGEM_CATALOGO);
  // Chave vazia cai no tratamento conservador, o mesmo que formaFixa ja dava:
  // sem chave, a forma do card nao e escolha desta tela.
  assert(fil_origem_de("") == FIL_ORIGEM_APP);
  assert(fil_origem_de(NULL) == FIL_ORIGEM_APP);
  puts("ok  a origem sai da chave, e nao de um campo novo no arquivo");

  // 6. A ORIGEM MANDA NA FORMA DO CARD. Sao a mesma pergunta e nao podem ter
  //    duas listas de chaves: quem acrescentar uma chave sintetica nova teria
  //    de lembrar das duas.
  for (i = 0; i < fil_n(); i++)
    assert(fil_aceita_tipo(i) == (fil_linha_origem(i) == FIL_ORIGEM_CATALOGO));
  puts("ok  so catalogo de addon escolhe a forma do card");

  // 7. O QUE ACOMPANHA A ORIGEM. Addon e tipo: o PRIMEIRO que souber preenche,
  //    porque sao dois registradores (descoberta e home) e so um conhece o
  //    addon. Contagem: o ULTIMO manda, porque ela e um retrato do agora.
  { int cat = -1;
    for (i = 0; i < fil_n(); i++) if (!strcmp(fil_chave(i), "catA")) cat = i;
    assert(cat >= 0);
    assert(!strcmp(fil_linha_addon(cat), "Cinemeta"));
    assert(!strcmp(fil_linha_conteudo(cat), "Filmes"));
    // Ainda nao foi montada nesta sessao: -1 e "nao sei", nao "vazia".
    assert(fil_linha_itens(cat) == -1);
    // A home registra a MESMA chave sem saber o addon e com a contagem.
    fil_registrar("catA", "Popular", "", "movie", 11);
    assert(!strcmp(fil_linha_addon(cat), "Cinemeta"));   // nao foi apagado
    assert(fil_linha_itens(cat) == 11);
    fil_registrar("catA", "Popular", "OutroAddon", "movie", 12);
    assert(!strcmp(fil_linha_addon(cat), "Cinemeta"));   // o primeiro manda
    assert(fil_linha_itens(cat) == 12);                  // o ultimo manda
    // Fileira do app nao tem addon nem tipo, e isso e informacao, nao falta.
    for (i = 0; i < fil_n(); i++) if (!strcmp(fil_chave(i), "continuar")) cat = i;
    assert(!fil_linha_addon(cat)[0] && !fil_linha_conteudo(cat)[0]);
    assert(fil_linha_itens(cat) == 9); }
  puts("ok  addon e tipo: quem souber primeiro; contagem: quem souber por ultimo");

  // 8. REGISTRAR DE NOVO NAO REESCREVE O ARQUIVO. A descoberta registra as
  //    mesmas chaves a cada ciclo de sync e a contagem muda em quase todos.
  //    Se isso bumpasse `revisao`, a home se remontaria para sempre — e o
  //    arquivo seria reescrito em flash por nada. Foi o defeito que o nome
  //    piscando ja causou; a contagem tem MUITO mais chance de mudar que o nome.
  { unsigned antes;
    // Descarrega o que ficou pendente das etapas acima (a chave nova do passo
    // 4), para medir SO o efeito de registrar de novo o que ja existe.
    fil_gravar_registro();
    antes = fil_revisao();
    for (i = 0; i < 5; i++) {
      fil_registrar("catA", "Popular", "Cinemeta", "movie", 10 + i);
      fil_gravar_registro();
    }
    assert(fil_revisao() == antes); }
  puts("ok  contagem nova nao reescreve o arquivo nem bumpa a revisao");


  // POR ULTIMO, e de proposito: este caso chama fil_esquecer() e recomeca a
  // lista do zero. No meio do arquivo ele derrubava os testes seguintes, que
  // seguem encadeados sobre o estado deixado pelo anterior.
  // HOME PELA METADE NAO REBAIXA QUEM ELA NAO VIU. Issue #30. HOME PELA METADE NAO REBAIXA QUEM ELA NAO VIU. Issue #30.
  //
  // No arranque a home publica em ~16 etapas: os catalogos entram primeiro e as
  // colecoes da conta chegam DEPOIS de todas elas. Cada uma dessas etapas
  // chamava esta funcao. A versao antiga reconstruia a lista inteira — as
  // chaves da tela primeiro, "todo o resto" atras — entao cada etapa precoce
  // empurrava a colecao para o fim, o limite de fileiras a cortava, e cortada
  // ela nunca mais voltava a tela para ser promovida. O relator so conseguia
  // trazer de volta desligando e religando a mao, e o arranque seguinte
  // rebaixava outra vez.
  //
  // A assercao e sobre a POSICAO de quem esta ausente: "colecao" tem de
  // continuar no indice 2 depois de uma remontagem que so conhece catB e catA.
  { const char *ordem[] = { "continuar", "amigos", "colecao", "catB", "catA" };
    const char *meiaHome[] = { "catB", "catA" };
    int j;
    fil_esquecer();
    for (j = 0; j < 5; j++) fil_registrar(ordem[j], ordem[j], "", "movie", 3);
    conferir("antes da meia home", ordem, 5);
    fil_espelhar_ordem(meiaHome, 2);
    // catB e catA ocupavam os indices 3 e 4 e sao os unicos que a tela viu;
    // eles podem trocar entre si, e ninguem mais pode sair do lugar.
    { const char *esperado[] = { "continuar", "amigos", "colecao", "catB", "catA" };
      conferir("meia home nao rebaixa o ausente", esperado, 5); }
    // E quando a tela ve os dois na ordem INVERTIDA, a troca acontece — dentro
    // dos mesmos dois slots. Sem esta metade o teste passaria com uma funcao
    // que simplesmente nao faz nada.
    { const char *invertida[] = { "catA", "catB" };
      const char *esperado[] = { "continuar", "amigos", "colecao", "catA", "catB" };
      fil_espelhar_ordem(invertida, 2);
      conferir("a tela permuta o que conhece", esperado, 5); }
    // CHAVE QUE A LISTA NAO CONHECE, na primeira posicao. Este e o caso REAL
    // mais comum e o unico que separa a implementacao certa de uma errada
    // plausivel: "last_session" entra na lista que a home passa (home.c) mas
    // NAO e registrado, entao achar() devolve -1 para ela.
    //
    // Uma revisao trocou `slots[k++]` por `slots[i]` e a suite inteira passou,
    // inclusive os dois casos acima — porque neles todas as chaves eram
    // conhecidas e i coincidia com k. Com a desconhecida na frente os dois
    // deixam de coincidir, e a variante errada nao permuta nada.
    { const char *comOrfa[] = { "last_session", "catB", "catA" };
      const char *esperado[] = { "continuar", "amigos", "colecao", "catB", "catA" };
      fil_espelhar_ordem(comOrfa, 3);
      conferir("chave desconhecida na frente nao desalinha", esperado, 5); }
    // NULL, repetida, e n maior que a lista: nenhum deles pode escrever fora.
    { const char *sujo[4]; const char *esperado[] = { "continuar", "amigos", "colecao", "catA", "catB" };
      sujo[0] = NULL; sujo[1] = "catA"; sujo[2] = "catA"; sujo[3] = "catB";
      fil_espelhar_ordem(sujo, 4);
      conferir("NULL e chave repetida nao quebram", esperado, 5); } }
  puts("ok  home pela metade nao empurra a colecao para o fim (#30)");
  puts("fileiras: tudo ok");
  return 0;
}
