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
static int usaArquivo;
int dados_gravar(const char *nome, const char *conteudo) {
  // COM ARQUIVO LIGADO, GRAVA DE VERDADE. Sem isto nao da para testar o par
  // gravar/carregar: a escrita virava um no-op e a releitura devolvia sempre o
  // arquivo anterior — um teste que passaria com a gravacao quebrada.
  if (usaArquivo) {
    char caminho[600];
    FILE *f;
    snprintf(caminho, sizeof caminho, "/tmp/%s", nome);
    f = fopen(caminho, "w");
    if (!f) return 0;
    fputs(conteudo, f);
    fclose(f);
    return 1;
  }
  (void)nome; (void)conteudo; return 1;
}
// NULL ate o teste da tabela cheia escrever o arquivo de verdade: linha lida
// do disco nasce com vista=0, que e o que separa "morta" de "ainda nao montou"
// no resgate de fileiras.c. Registrar pela API marcaria vista=1 e o teste
// mediria outra coisa.
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  if (!usaArquivo) { (void)dst; (void)tam; (void)nome; return NULL; }
  snprintf(dst, tam, "/tmp/%s", nome);
  return dst;
}
void fil_teste_recarregar(void);
void fil_teste_esquecer_vista(int i);
int  fil_teste_achar(const char *chave);

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
  fil_espelhar_ordem(DA_HOME, NULL, N_HOME);
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
  fil_espelhar_ordem(DA_HOME, NULL, N_HOME);
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
    assert(fil_aceita_tipo(i) == (fil_linha_origem(i) != FIL_ORIGEM_APP));
  puts("ok  catalogo e colecao escolhem a forma do card; fileira do app nao");

  // 6b. ESTILO PELO MENU DO CARTAZ. O menu grava o MESMO `tipo` da tela de
  //     Ajustes; colecao so aceita as formas dela, e o ciclo dos Ajustes pula
  //     o resto.
  { int tipos[FIL_TIPO_N], n, k, col = -1, cat = -1; const char *rot[FIL_TIPO_N];
    fil_registrar("collection_formas", "Formas", "", "", 3);
    fil_registrar("addon_movie_formas", "Formas cat", "Addon", "movie", 3);
    n = fil_estilos("addon_movie_formas", tipos, rot, FIL_TIPO_N);
    // TODAS as formas de FilTipo, cada uma uma vez (o modal de estilo nao tem
    // mais o teto de sete linhas do menu que deixava o 4:3 e a faixa de fora).
    // Ordem das LINHAS do modal achatada: as tres paisagens juntas (a linha
    // "Paisagem"), a faixa, os tres 4:3 juntos, os rankings.
    assert(n == FIL_TIPO_N && tipos[0] == FIL_TIPO_AUTO && tipos[1] == FIL_TIPO_CARTAZ &&
           tipos[2] == FIL_TIPO_SERVICO && tipos[3] == FIL_TIPO_COLECAO &&
           tipos[4] == FIL_TIPO_DESTAQUE && tipos[5] == FIL_TIPO_LARGA &&
           tipos[6] == FIL_TIPO_DESTAQUE_QUADRADO &&
           tipos[7] == FIL_TIPO_DESTAQUE_QUADRADO_M &&
           tipos[8] == FIL_TIPO_DESTAQUE_QUADRADO_G && !strcmp(rot[2], "Paisagem pequena"));
    { int visto[FIL_TIPO_N] = {0}, k;
      for (k = 0; k < n; k++) { assert(!visto[tipos[k]]); visto[tipos[k]] = 1; } }
    // Issue #201: os dois rankings no menu, o numerado (Dinamica) primeiro.
    assert(tipos[9] == FIL_TIPO_RANKING && tipos[10] == FIL_TIPO_TOP10 &&
           !strcmp(rot[9], "Ranking numerado"));
    // AS LINHAS: o que so muda de tamanho divide uma linha so.
    { FilEstiloLinha l[FIL_TIPO_N];
      int nl = fil_estilo_linhas("addon_movie_formas", l, FIL_TIPO_N);
      // 7 de antes + Linha do tempo, Cartaz com ficha e Palco (06/10).
      assert(nl == 10);
      assert(l[7].tipos[0] == FIL_TIPO_LINHA_TEMPO && l[8].tipos[0] == FIL_TIPO_FICHA &&
             l[9].tipos[0] == FIL_TIPO_PALCO);
      assert(!strcmp(l[2].rotulo, "Paisagem") && l[2].n == 3 &&
             l[2].tipos[0] == FIL_TIPO_SERVICO && l[2].tipos[2] == FIL_TIPO_DESTAQUE &&
             !strcmp(l[2].nomes[2], "Paisagem grande"));
      assert(!strcmp(l[4].rotulo, "Destaque 4:3") && l[4].n == 3 &&
             l[4].tipos[0] == FIL_TIPO_DESTAQUE_QUADRADO &&
             l[4].tipos[2] == FIL_TIPO_DESTAQUE_QUADRADO_G);
      for (k = 0; k < nl; k++) if (k != 2 && k != 4) assert(l[k].n == 1);
      assert(fil_estilo_linhas("collection_formas", l, FIL_TIPO_N) == 4 && l[1].n == 1);
      assert(fil_estilo_linhas("continue_watching", l, FIL_TIPO_N) == 0); }
    // OS 4:3 MAIORES: a mesma forma, com fator. O 6 gravado de antes continua
    // sendo o 4:3 de sempre (fator 1), e o Tamanho Grande nao passa do teto.
    assert(fil_definir_tipo("addon_movie_formas", FIL_TIPO_DESTAQUE_QUADRADO));
    assert(fil_escala("addon_movie_formas") == 1.0f);
    assert(fil_definir_tipo("addon_movie_formas", FIL_TIPO_DESTAQUE_QUADRADO_G));
    assert(fil_tipo("addon_movie_formas") == FIL_TIPO_DESTAQUE_QUADRADO_G);
    assert(fil_escala("addon_movie_formas") == 1.5f);
    assert(fil_escala_tipo("addon_movie_formas", FIL_TIPO_DESTAQUE_QUADRADO_M) == 1.25f);
    assert(fil_escala_tipo("addon_movie_formas", FIL_TIPO_CARTAZ) == 1.0f);
    assert(!fil_definir_tipo("collection_formas", FIL_TIPO_DESTAQUE_QUADRADO_M));
    assert(fil_estilo_ajuda("addon_movie_formas", FIL_TIPO_DESTAQUE_QUADRADO_G)[0]);
    assert(!strcmp(fil_tipo_rotulo(FIL_TIPO_DESTAQUE_QUADRADO_G), "Destaque 4:3 grande"));
    for (k = 0; k < fil_n(); k++)
      if (!strcmp(fil_chave(k), "addon_movie_formas")) {
        while (fil_linha_tam(k) != FIL_TAM_GRANDE) fil_ciclar_tam(k);
        assert(fil_escala("addon_movie_formas") == 1.5f);              // teto
        assert(fil_escala_tipo("addon_movie_formas", FIL_TIPO_DESTAQUE_QUADRADO_M) == 1.5f);
        assert(fil_escala_tipo("addon_movie_formas", FIL_TIPO_DESTAQUE_QUADRADO) == 1.2f);
        while (fil_linha_tam(k) != FIL_TAM_PADRAO) fil_ciclar_tam(k);
      }
    assert(fil_definir_tipo("addon_movie_formas", FIL_TIPO_LARGA));
    assert(fil_tipo("addon_movie_formas") == FIL_TIPO_LARGA);
    assert(!fil_definir_tipo("collection_formas", FIL_TIPO_LARGA));   // colecao nao
    assert(fil_estilo_ajuda("addon_movie_formas", FIL_TIPO_LARGA)[0]);
    assert(strcmp(fil_estilo_ajuda("collection_formas", FIL_TIPO_CARTAZ),
                  fil_estilo_ajuda("addon_movie_formas", FIL_TIPO_CARTAZ)));
    assert(fil_definir_tipo("addon_movie_formas", FIL_TIPO_RANKING));
    assert(fil_tipo("addon_movie_formas") == FIL_TIPO_RANKING);
    assert(!fil_definir_tipo("collection_formas", FIL_TIPO_RANKING));   // colecao nao
    assert(fil_definir_tipo("addon_movie_formas", FIL_TIPO_LINHA_TEMPO));
    assert(fil_tipo("addon_movie_formas") == FIL_TIPO_LINHA_TEMPO);
    assert(!fil_definir_tipo("collection_formas", FIL_TIPO_PALCO));     // colecao nao
    assert(fil_estilo_ajuda("addon_movie_formas", FIL_TIPO_FICHA)[0]);
    n = fil_estilos("collection_formas", tipos, rot, 8);
    assert(n == 4 && tipos[1] == FIL_TIPO_COLECAO &&
           tipos[2] == FIL_TIPO_DESTAQUE_QUADRADO && tipos[3] == FIL_TIPO_CARTAZ);
    assert(fil_estilos("continue_watching", tipos, rot, 8) == 0);
    assert(!fil_definir_tipo("continue_watching", FIL_TIPO_CARTAZ));
    assert(!fil_definir_tipo("collection_formas", FIL_TIPO_TOP10));   // nao e forma de colecao
    assert(fil_definir_tipo("collection_formas", FIL_TIPO_CARTAZ));
    assert(fil_tipo("collection_formas") == FIL_TIPO_CARTAZ);
    assert(!strcmp(fil_estilo_rotulo("collection_formas", FIL_TIPO_CARTAZ), "Pôster"));
    assert(fil_definir_tipo("addon_movie_formas", FIL_TIPO_SERVICO));
    assert(fil_tipo("addon_movie_formas") == FIL_TIPO_SERVICO);
    for (k = 0; k < fil_n(); k++) {
      if (!strcmp(fil_chave(k), "collection_formas")) col = k;
      if (!strcmp(fil_chave(k), "addon_movie_formas")) cat = k;
    }
    assert(col >= 0 && cat >= 0);
    assert(!strcmp(fil_linha_tipo_rotulo(col), "Pôster"));
    assert(!strcmp(fil_linha_tipo_rotulo(cat), fil_tipo_rotulo(FIL_TIPO_SERVICO)));
    // Ciclo dos Ajustes numa colecao: Poster -> Automatico -> Paisagem -> Quadrado.
    fil_ciclar_tipo(col); assert(fil_linha_tipo(col) == FIL_TIPO_AUTO);
    fil_ciclar_tipo(col); assert(fil_linha_tipo(col) == FIL_TIPO_COLECAO);
    fil_ciclar_tipo(col); assert(fil_linha_tipo(col) == FIL_TIPO_DESTAQUE_QUADRADO);
    fil_ciclar_tipo(col); assert(fil_linha_tipo(col) == FIL_TIPO_CARTAZ);
    fil_definir_tipo("collection_formas", FIL_TIPO_AUTO);
    fil_definir_tipo("addon_movie_formas", FIL_TIPO_AUTO); }
  puts("ok  estilo da fileira: menu e Ajustes gravam o mesmo tipo");

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
    fil_espelhar_ordem(meiaHome, NULL, 2);
    // catB e catA ocupavam os indices 3 e 4 e sao os unicos que a tela viu;
    // eles podem trocar entre si, e ninguem mais pode sair do lugar.
    { const char *esperado[] = { "continuar", "amigos", "colecao", "catB", "catA" };
      conferir("meia home nao rebaixa o ausente", esperado, 5); }
    // E quando a tela ve os dois na ordem INVERTIDA, a troca acontece — dentro
    // dos mesmos dois slots. Sem esta metade o teste passaria com uma funcao
    // que simplesmente nao faz nada.
    { const char *invertida[] = { "catA", "catB" };
      const char *esperado[] = { "continuar", "amigos", "colecao", "catA", "catB" };
      fil_espelhar_ordem(invertida, NULL, 2);
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
      fil_espelhar_ordem(comOrfa, NULL, 3);
      conferir("chave desconhecida na frente nao desalinha", esperado, 5); }
    // NULL, repetida, e n maior que a lista: nenhum deles pode escrever fora.
    { const char *sujo[4]; const char *esperado[] = { "continuar", "amigos", "colecao", "catA", "catB" };
      sujo[0] = NULL; sujo[1] = "catA"; sujo[2] = "catA"; sujo[3] = "catB";
      fil_espelhar_ordem(sujo, NULL, 4);
      conferir("NULL e chave repetida nao quebram", esperado, 5); } }
  puts("ok  home pela metade nao empurra a colecao para o fim (#30)");

  // MOVER O BLOCO DO ADDON INTEIRO. A folha de Ajustes tem dois modos: fileira
  // a fileira (fil_mover) e bloco (fil_mover_grupo), que troca TODAS as linhas
  // do addon com o bloco vizinho de uma vez. Sem ele, juntar o Xperience
  // inteiro para cima era N movimentos — um por catalogo do addon.
  fil_esquecer();
  fil_registrar("continue_watching", "Continuar", "", "", 5);
  fil_registrar("cineA", "Top", "Cinemeta", "movie", 10);
  fil_registrar("cineB", "Popular", "Cinemeta", "series", 10);
  fil_registrar("xpA", "For You", "Xperience", "movie", 12);
  fil_registrar("xpB", "Trending", "Xperience", "series", 12);
  { const char *o0[] = { "continue_watching", "cineA", "cineB", "xpA", "xpB" };
    conferir("blocos antes de mover", o0, 5); }
  // Sobe o BLOCO Xperience inteiro (a partir da linha xpB, de proposito —
  // a funcao tem de achar a extensao do bloco sozinha).
  { int novo = fil_mover_grupo(4, -1);
    const char *o1[] = { "continue_watching", "xpA", "xpB", "cineA", "cineB" };
    conferir("bloco Xperience subiu inteiro", o1, 5);
    assert(novo == 2);   // xpB: indice 4 -> 2 (pulou o bloco de 2 do Cinemeta)
  }
  // E desce de volta, trocando com o bloco Cinemeta inteiro de uma vez.
  { int novo = fil_mover_grupo(2, 1);
    const char *o2[] = { "continue_watching", "cineA", "cineB", "xpA", "xpB" };
    conferir("bloco Xperience desceu inteiro", o2, 5);
    assert(novo == 4); }
  // Borda: o topo nao sobe porque nao ha nada acima.
  { int novo = fil_mover_grupo(0, -1);
    assert(novo == 0);
    const char *o3[] = { "continue_watching", "cineA", "cineB", "xpA", "xpB" };
    conferir("topo nao sobe mais", o3, 5); }
  puts("ok  mover bloco troca o addon inteiro com o vizinho");

  // AGRUPAR POR ADDON. Quando a ordem veio misturada da conta, a acao junta as
  // linhas de cada addon: fixas do app primeiro, colecoes depois, catalogos
  // por addon na ordem de primeira aparicao — e dentro do addon a ordem
  // relativa se preserva (o sort e estavel).
  fil_esquecer();
  fil_registrar("xpA", "For You", "Xperience", "movie", 12);
  fil_registrar("continue_watching", "Continuar", "", "", 5);
  fil_registrar("cineA", "Top", "Cinemeta", "movie", 10);
  fil_registrar("xpB", "Trending", "Xperience", "series", 12);
  fil_registrar("collection_a24", "A24", "", "", 8);
  fil_registrar("cineB", "Popular", "Cinemeta", "series", 10);
  fil_ordenar_por_addon();
  { const char *o4[] = { "continue_watching", "collection_a24",
                         "xpA", "xpB", "cineA", "cineB" };
    conferir("ordenar por addon", o4, 6); }
  // Idempotente: ordenar de novo nao muda nada.
  fil_ordenar_por_addon();
  { const char *o5[] = { "continue_watching", "collection_a24",
                         "xpA", "xpB", "cineA", "cineB" };
    conferir("ordenar por addon (2a vez)", o5, 6); }
  puts("ok  agrupar por addon junta os catalogos sem embaralhar o addon");

  // TABELA CHEIA NAO ENGOLE CHAVE VIVA. O arquivo do dono lotou FIL_MAX com
  // chaves mortas do AICat (uma nova a cada ciclo) e a colecao do FrostView
  // aparecia na home sem nunca entrar na folha. O resgate toma a vaga de uma
  // linha morta sem escolha; linha com escolha (desligada, forma, tamanho)
  // nao e vitima.
  fil_esquecer();
  { int j;
    // As mortas vem DO ARQUIVO: e assim que elas existem de verdade — carregadas
    // no arranque, nunca declaradas nesta sessao (vista=0). Pelo registrador elas
    // seriam vistas e o resgate, certamente, nao as tocaria.
    FILE *arq = fopen("/tmp/fileirasui.txt", "w");
    assert(arq);
    // campos: chave \t oculta \t tipo \t tamanho \t titulo — 0/0/1 sao os
    // PADROES de quem nunca foi escolhida (tipo AUTO=0, tamanho PADRAO=1).
    for (j = 0; j < FIL_MAX; j++)
      fprintf(arq, "linha morta_%d\t0\t0\t1\tMorta %d\n", j, j);
    fclose(arq);
    usaArquivo = 1;
    fil_teste_recarregar();
    assert(fil_n() == FIL_MAX);
    // Duas com escolha: uma desligada, uma com forma trocada. Nenhuma pode
    // ser vitima do resgate.
    fil_alternar(10);
    fil_ciclar_tipo(20);
    // A home mostra so tres chaves: duas mortas e UMA NOVA.
    { const char *home[] = { "morta_5", "nova_frostview", "morta_7" };
      const char *tit[]  = { "Morta 5", "FrostView", "Morta 7" };
      fil_espelhar_ordem(home, tit, 3);
      assert(fil_n() == FIL_MAX);   // a tabela continua cheia, nao estoura
      { int v = -1;
        for (j = 0; j < fil_n(); j++)
          if (!strcmp(fil_chave(j), "nova_frostview")) v = j;
        assert(v >= 0);                          // a nova entrou
        assert(!strcmp(fil_titulo(v), "FrostView")); // e com o titulo, nao a chave
        assert(fil_linha_na_home(v)); }
      // As duas escolhidas seguem na lista.
      { int tem10 = 0, tem20 = 0;
        for (j = 0; j < fil_n(); j++) {
          if (!strcmp(fil_chave(j), "morta_10")) { tem10 = 1; assert(fil_linha_oculta(j)); }
          if (!strcmp(fil_chave(j), "morta_20")) { tem20 = 1; assert(fil_linha_tipo(j) == 1); }
        }
        assert(tem10 && tem20); } } }
  puts("ok  tabela cheia: chave viva toma a vaga de linha morta sem escolha");

  // TABELA CHEIA SO DE LINHAS COM FORMA ESCOLHIDA (a C9 do dono, 20/09/2026:
  // 283 do Xperience com tipo 2/3/4, o resto oculto). Antes: "nada
  // dispensavel", a chave nova ficava fora — e, pior, "Continuar assistindo"
  // (AUTO/padrao, no arranque nem naHome nem vista) tinha sido despejada e
  // voltado NO FIM. Agora: (a) fileira do app nunca sai; (b) as primeiras
  // `limite` ligadas nao saem; (c) sem dispensavel puro, sai a ultima fora da
  // home MESMO com forma escolhida.
  fil_esquecer();
  { int j;
    FILE *arq = fopen("/tmp/fileirasui.txt", "w");
    assert(arq);
    fprintf(arq, "limite 7\nordem 1\n");
    fprintf(arq, "linha continue_watching\t0\t0\t1\tContinuar assistindo\n");
    fprintf(arq, "linha social_activity\t0\t0\t1\tAmigos assistindo\n");
    for (j = 2; j < FIL_MAX; j++)
      fprintf(arq, "linha xp_%d\t0\t2\t1\tXperience %d\n", j, j);
    fclose(arq);
    usaArquivo = 1;
    fil_teste_recarregar();
    assert(fil_n() == FIL_MAX);
    fil_registrar("canal_novo", "Canais", "addon", "tv", 12);
    assert(fil_n() == FIL_MAX);
    { int v = -1, cw = -1, so = -1, ult = -1;
      for (j = 0; j < fil_n(); j++) {
        if (!strcmp(fil_chave(j), "canal_novo")) v = j;
        if (!strcmp(fil_chave(j), "continue_watching")) cw = j;
        if (!strcmp(fil_chave(j), "social_activity")) so = j;
        { char u[16]; snprintf(u, sizeof u, "xp_%d", FIL_MAX - 1);
          if (!strcmp(fil_chave(j), u)) ult = j; }
      }
      assert(v == fil_n() - 1);        // a nova entrou, no fim
      assert(cw == 0 && so == 1);      // as do app ficaram onde estavam
      assert(ult < 0);                 // saiu a ULTIMA com forma, nao uma do topo
      for (j = 2; j < 7; j++) {        // as primeiras `limite` ligadas ficaram
        char k[16]; snprintf(k, sizeof k, "xp_%d", j);
        assert(!strcmp(fil_chave(j), k));
      } } }
  puts("ok  tabela cheia so de escolhidas: fileira do app e topo da home nunca saem");

  // #294: com a tabela cheia, colecao lida do disco (vista=0, fora da home)
  // NAO e dispensavel: o catalogo novo toma a vaga de um catalogo.
  fil_esquecer();
  { int j, c = 0, n = 0;
    FILE *arq = fopen("/tmp/fileirasui.txt", "w");
    assert(arq);
    fprintf(arq, "limite 7\nordem 1\n");
    for (j = 0; j < FIL_MAX; j++)
      fprintf(arq, "linha %s_%d\t0\t0\t1\tFileira %d\n",
              j >= FIL_MAX - 3 ? "collection_g" : "cat", j, j);
    fclose(arq);
    usaArquivo = 1;
    fil_teste_recarregar();
    assert(fil_n() == FIL_MAX);
    fil_registrar("addon_novo", "Novo", "addon", "movie", 5);
    for (j = 0; j < fil_n(); j++) {
      if (!strncmp(fil_chave(j), "collection_g_", 13)) c++;
      if (!strcmp(fil_chave(j), "addon_novo")) n = 1;
    }
    assert(n && c == 3); }
  puts("ok  tabela cheia: colecao nao e despejada na janela da troca de perfil");

  // MOVER PULA LINHA FORA DA HOME. A fantasma no meio nao come o movimento:
  // a fileira troca com a proxima VIVA, e a home muda junto.
  fil_esquecer();
  fil_registrar("vivoA", "A", "Cinemeta", "movie", 4);
  fil_registrar("fantasma", "G", "AICat", "movie", 4);
  fil_registrar("vivoB", "B", "Cinemeta", "movie", 4);
  fil_remover(1);                   // a fantasma esta OCULTA: e ela que e transparente
  { const char *home[] = { "vivoA", "vivoB" };
    fil_espelhar_ordem(home, NULL, 2);
    assert(fil_mover(0, 1) == 2);   // A passa pela oculta e para depois de B
    { const char *ordem[] = { "vivoB", "fantasma", "vivoA" };
      conferir("vivo pulou a fantasma", ordem, 3); }
    // E a home reflete: A depois de B.
    { int ord[8], q = fil_unir(home, 2, ord, 8);
      assert(q == 2);
      assert(!strcmp(home[ord[0]], "vivoB") && !strcmp(home[ord[1]], "vivoA")); } }
  puts("ok  mover pula linha oculta");

  // MOVER ENTRE LIGADAS QUE A HOME AINDA NAO MONTOU (recem-adicionada, na
  // fila): o vizinho e a proxima ligada, e nao "a proxima que a home ja tem".
  fil_esquecer();
  fil_registrar("h1", "H1", "X", "movie", 4);
  fil_registrar("h2", "H2", "X", "movie", 4);
  fil_registrar("nova", "Nova", "Y", "movie", 4);
  { const char *home[] = { "h1", "h2" };
    fil_espelhar_ordem(home, NULL, 2);         // nova: ligada, naHome=0
    assert(fil_mover(1, 1) == 2);              // h2 troca com nova, nao trava
    assert(!strcmp(fil_chave(1), "nova") && !strcmp(fil_chave(2), "h2"));
    assert(fil_mover(2, -1) == 1);             // e volta
    assert(fil_mover(1, -1) == 0);             // e sobe de novo, sem travar
  }
  puts("ok  mover entre ligadas que a home ainda nao montou");

  // NA HOME / NA FILA / FORA, E A FILA E SO A ORDEM. Limite 4 (o minimo e 3),
  // seis ligadas: as quatro primeiras estao na home, as duas seguintes na fila,
  // e remover uma da home faz a primeira da fila subir SEM ninguem mexer nela.
  fil_esquecer();
  fil_definir_limite(4);
  { int j;
    const char *n[] = { "a", "b", "c", "d", "e", "f", "g" };
    for (j = 0; j < 7; j++) fil_registrar(n[j], n[j], "X", "movie", 1);
    fil_remover(6);                       // g fora
    assert(fil_estado(0) == FIL_NA_HOME && fil_estado(3) == FIL_NA_HOME);
    assert(fil_estado(4) == FIL_NA_FILA && fil_estado(5) == FIL_NA_FILA);
    assert(fil_estado(6) == FIL_FORA);
    assert(fil_n_na_home() == 4 && fil_n_fila() == 2);
    fil_remover(1);                       // b sai da home
    assert(fil_estado(4) == FIL_NA_HOME); // e subiu sozinha
    assert(fil_estado(5) == FIL_NA_FILA);
    assert(fil_n_fila() == 1);
    // ADICIONAR com a home cheia: vai para o FIM do bloco ligado, na fila,
    // atras de quem ja esperava — e o chamador fica sabendo.
    { int est = -1, novo = fil_adicionar(1, &est);   // b volta
      int pf = -1, pb = -1;
      assert(est == FIL_NA_FILA);
      assert(!strcmp(fil_chave(novo), "b"));
      for (j = 0; j < fil_n(); j++) { if (!strcmp(fil_chave(j), "f")) pf = j; if (!strcmp(fil_chave(j), "b")) pb = j; }
      assert(pf < pb); }                  // f esperava antes: continua na frente
    assert(fil_n_fila() == 2);
    // LIMITE QUE BAIXA: quem ficou alem vira FORA, nao fila.
    fil_definir_limite(4);                // sem mudanca: nada acontece
    assert(fil_n_fila() == 2);
    fil_definir_limite(3);
    assert(fil_n_na_home() == 3 && fil_n_fila() == 0);
    { int ocultas = 0; for (j = 0; j < fil_n(); j++) if (fil_linha_oculta(j)) ocultas++;
      assert(ocultas == 4); } }           // g, e as tres empurradas
  puts("ok  fila: e a ordem; remover promove; limite menor manda para fora");

  // NORMALIZAR: ligada alem do limite SEM marca de fila vira fora; a que a
  // pessoa pos na fila fica; a que a home desenha (naHome) fica.
  fil_esquecer();
  fil_definir_limite(3);
  { int j, est = -1;
    const char *n[] = { "a", "b", "c", "d", "e", "f" };
    for (j = 0; j < 6; j++) fil_registrar(n[j], n[j], "X", "movie", 1);
    // e: a home desenhou (vaga garantida) — espelha como na home
    { const char *home[] = { "a", "b", "c", "e" }; fil_espelhar_ordem(home, NULL, 4); }
    // f: pedida pela pessoa com a home cheia
    { int pf = -1; for (j = 0; j < fil_n(); j++) if (!strcmp(fil_chave(j), "f")) pf = j;
      fil_adicionar(pf, &est); assert(est == FIL_NA_FILA); }
    fil_normalizar();
    for (j = 0; j < fil_n(); j++) {
      const char *k = fil_chave(j);
      // #358: d e catalogo NOVO (registrado nesta sessao, intocado): nao coube,
      // espera na fila em vez de ir para Fora.
      if (!strcmp(k, "d")) assert(!fil_linha_oculta(j) && fil_estado(j) == FIL_NA_FILA);
      if (!strcmp(k, "e")) assert(!fil_linha_oculta(j));          // na home: fica
      if (!strcmp(k, "f")) assert(!fil_linha_oculta(j) && fil_estado(j) == FIL_NA_FILA);
    } }
  puts("ok  normalizar: catalogo novo que nao coube vai para a fila (#358)");

  // #358: add-on novo com N catalogos. Com espaco livre todos entram na Home;
  // com a Home cheia o excedente vira FILA (nunca OC_PESSOA / Fora).
  fil_esquecer();
  fil_definir_limite(5);
  { int j, na_home = 0, na_fila = 0, fora = 0;
    char ch[32];
    for (j = 0; j < 3; j++) { snprintf(ch, sizeof ch, "n%d", j); fil_registrar(ch, ch, "Novo", "movie", 1); }
    fil_normalizar();
    for (j = 0; j < fil_n(); j++) assert(fil_estado(j) == FIL_NA_HOME);   // cabem: Home
    for (j = 3; j < 9; j++) { snprintf(ch, sizeof ch, "n%d", j); fil_registrar(ch, ch, "Novo", "movie", 1); }
    fil_normalizar();
    for (j = 0; j < fil_n(); j++) {
      if (fil_linha_oculta(j)) fora++;
      else if (fil_estado(j) == FIL_NA_FILA) na_fila++;
      else na_home++;
    }
    assert(na_home == 5 && na_fila == 4 && fora == 0);
    // O que o dono tirou de proposito continua Fora.
    { int pj = -1; for (j = 0; j < fil_n(); j++) if (!strcmp(fil_chave(j), "n1")) pj = j;
      fil_remover(pj); }
    fil_normalizar();
    for (j = 0; j < fil_n(); j++) if (!strcmp(fil_chave(j), "n1")) assert(fil_linha_oculta(j));
  }
  puts("ok  #358: addon novo cabe na Home; excedente na fila; Fora explicito fica");

  // #358: linha que JA ESTAVA no arquivo (sem a marca "nova") alem do limite
  // segue a regra antiga: vai para Fora. A nova do mesmo arquivo vai para a
  // fila, e a marca sobrevive ao recarregar antes da folha normalizar.
  fil_esquecer();
  { int j, achouVelha = 0, achouNova = 0;
    usaArquivo = 1;
    { FILE *f = fopen("/tmp/fileirasui-p1.txt", "w");
      fprintf(f, "limite 3\nordem 0\n");
      fprintf(f, "linha v1\t0\t0\t1\t0\tV1\n");
      fprintf(f, "linha v2\t0\t0\t1\t0\tV2\n");
      fprintf(f, "linha v3\t0\t0\t1\t0\tV3\n");
      fprintf(f, "linha v4\t0\t0\t1\t0\tV4\n");
      fprintf(f, "linha nv\t0\t0\t1\t0\tNV\n");
      fprintf(f, "nova nv\n");
      fclose(f); }
    fil_definir_perfil(1);
    fil_teste_recarregar();      // persistencia: a marca volta do disco
    fil_normalizar();
    for (j = 0; j < fil_n(); j++) {
      if (!strcmp(fil_chave(j), "v4")) { achouVelha = 1; assert(fil_linha_oculta(j)); }
      if (!strcmp(fil_chave(j), "nv")) { achouNova = 1; assert(!fil_linha_oculta(j) && fil_estado(j) == FIL_NA_FILA); }
    }
    assert(achouVelha && achouNova);
    // a fila persiste e a marca "nova" ja foi consumida
    fil_teste_recarregar();
    for (j = 0; j < fil_n(); j++)
      if (!strcmp(fil_chave(j), "nv")) assert(!fil_linha_oculta(j) && fil_estado(j) == FIL_NA_FILA);
    remove("/tmp/fileirasui-p1.txt");
    fil_definir_perfil(0);
    usaArquivo = 0; }
  puts("ok  #358: linha antiga alem do limite segue Fora; marca e fila persistem");

  // PODA: catalogo de addon que sumiu da conta sai; o resto fica. A lista vem
  // da conta do PERFIL desta escolha (perfilDaLista == 1 == perfil ativo).
  fil_esquecer();
  { int j;
    const char *ids[] = { "addonvivo" }; const char *bases[] = { "https://vivo.example" };
    // Nada visto == tudo candidato; mas so catalogo de addon ausente cai.
    // Antes: simula linha lida do disco (vista=0) para o fantasma.
    usaArquivo = 1;
    { FILE *f = fopen("/tmp/fileirasui-p1.txt", "w");
      fprintf(f, "limite 7\nordem 0\n");
      fprintf(f, "linha addonvivo_movie_top\t0\t0\t1\tTop\n");
      fprintf(f, "linha addonmorto_movie_top\t0\t0\t1\tFantasma\n");
      fprintf(f, "linha continue_watching\t0\t0\t1\tContinuar\n");
      fprintf(f, "linha collection_x\t0\t0\t1\tColecao\n");
      fclose(f); }
    fil_definir_perfil(1);
    fil_teste_recarregar();
    assert(fil_n() == 4);
    // O vivo foi VISTO nesta sessao; o fantasma nao. So o fantasma cai.
    fil_registrar("addonvivo_movie_top", "Top", "Vivo", "movie", 5);
    assert(fil_podar_catalogos(ids, bases, 1, 1) == 1);
    assert(fil_n() == 3);
    for (j = 0; j < fil_n(); j++) assert(strcmp(fil_chave(j), "addonmorto_movie_top") != 0);
    // Chamar de novo nao tira mais nada.
    assert(fil_podar_catalogos(ids, bases, 1, 1) == 0);
    remove("/tmp/fileirasui-p1.txt");
    fil_definir_perfil(0);
    usaArquivo = 0; }
  puts("ok  poda: catalogo de addon removido sai, app e colecao ficam");

  // A PODA DO ARRANQUE NA C9 (24/09). O perfil 1 tem Meu Futebol, Pluto TV e
  // Bingecat na conta; a primeira volta le o art/addons.txt do PACOTE, que nao
  // os tem. A poda rodava com essa lista e apagava do fileirasui-p1.txt as
  // escolhas dele — Pluto "fora da home" voltava ligado quando a conta chegava.
  fil_esquecer();
  { const char *ids[] = { "org.stremio.opensubtitlesv3", "app.xperience", "com.akashitv" };
    const char *bases[] = { "https://os.example", "https://xp.example", "https://ak.example" };
    int j, achouPluto = 0, achouBinge = 0, achouFut = 0;
    usaArquivo = 1;
    { FILE *f = fopen("/tmp/fileirasui-p1.txt", "w");
      fprintf(f, "limite 16\nordem 1\n");
      fprintf(f, "linha app.xperience_movie_top\t0\t0\t1\t0\tTop\n");
      // Desligada na TV: "Pluto TV - Channels" fora da home.
      fprintf(f, "linha org.plutotv_tv_pluto\t1\t0\t1\t0\tPluto TV - Channels\n");
      // Forma escolhida (destaque largo).
      fprintf(f, "linha com.aicat.x.nuvio_movie_list\t0\t2\t1\t0\tList for you\n");
      // Intocada: ligada, tudo no padrao.
      fprintf(f, "linha org.meufutebol_tv_canais\t0\t0\t1\t0\tMeu Futebol\n");
      fclose(f); }
    fil_definir_perfil(1);
    assert(fil_n() == 4);
    // Lista do PACOTE (nao veio da conta): nada sai.
    assert(fil_podar_catalogos(ids, bases, 3, 0) == 0);
    // Lista da conta de OUTRO perfil (o 2, da sessao anterior): nada sai.
    assert(fil_podar_catalogos(ids, bases, 3, 2) == 0);
    assert(fil_n() == 4);
    // Lista da conta DESTE perfil sem esses addons (removidos de verdade): so
    // a linha intocada e fantasma. A desligada e a com forma sao escolha.
    assert(fil_podar_catalogos(ids, bases, 3, 1) == 1);
    assert(fil_n() == 3);
    for (j = 0; j < fil_n(); j++) {
      if (!strcmp(fil_chave(j), "org.plutotv_tv_pluto")) { achouPluto = 1; assert(fil_linha_oculta(j)); }
      if (!strcmp(fil_chave(j), "com.aicat.x.nuvio_movie_list")) { achouBinge = 1; assert(fil_linha_tipo(j) == 2); }
      if (!strcmp(fil_chave(j), "org.meufutebol_tv_canais")) achouFut = 1;
    }
    assert(achouPluto && achouBinge && !achouFut);
    // E continua oculta depois de reler o arquivo gravado.
    fil_teste_recarregar();
    fil_definir_perfil(0); fil_definir_perfil(1);
    assert(fil_oculta("org.plutotv_tv_pluto"));
    remove("/tmp/fileirasui-p1.txt");
    fil_definir_perfil(0);
    usaArquivo = 0; }
  puts("ok  poda: lista do pacote ou de outro perfil nao poda; escolha nunca e podada");

  // ADDON NOVO (vaga garantida, cotacat.h): so o que este perfil nunca viu e
  // sobre o qual nao decidiu nada.
  fil_esquecer();
  { usaArquivo = 1;
    { FILE *f = fopen("/tmp/fileirasui-p1.txt", "w");
      fprintf(f, "limite 7\nordem 0\n");
      fprintf(f, "linha org.visto_movie_a\t0\t0\t1\t0\tJa visto\n");
      fclose(f); }
    fil_definir_perfil(1);
    assert(fil_n() == 1);
    // Veio do arquivo: o perfil ja viu.
    fil_registrar("org.visto_movie_a", "Ja visto", "Visto", "movie", -1);
    assert(!fil_addon_novo("org.visto", "https://visto.example"));
    // So registrado nesta sessao, nada escolhido: novo.
    fil_registrar("org.novo_movie_a", "Novo", "Novo", "movie", -1);
    fil_registrar("org.novo_movie_b", "Novo B", "Novo", "movie", -1);
    assert(fil_addon_novo("org.novo", "https://novo.example"));
    // Nunca registrado: novo (a descoberta ainda nao declarou).
    assert(fil_addon_novo("org.nunca", "https://nunca.example"));
    // Uma fileira dele tirada da home: deixou de ser novo.
    { int j;
      for (j = 0; j < fil_n(); j++) if (!strcmp(fil_chave(j), "org.novo_movie_b")) fil_remover(j); }
    assert(!fil_addon_novo("org.novo", "https://novo.example"));
    // Prefixo parecido nao e o mesmo addon.
    assert(fil_addon_novo("org.nov", "https://nov.example"));
    remove("/tmp/fileirasui-p1.txt");
    fil_definir_perfil(0);
    usaArquivo = 0; }
  puts("ok  addon novo: so o nunca visto e sem escolha");

  // TABELA CHEIA: o que a home desenha TEM de caber na lista.
  //
  // O defeito medido na C9: 279 catalogos declarados contra um teto de 192, e
  // seis fileiras desenhadas na home que nao existiam na tela de fileiras —
  // sem como mover nem desligar. fil_registrar recusava em silencio.
  fil_esquecer();
  { int j;
    char ch[32];
    for (j = 0; j < FIL_MAX; j++) {
      snprintf(ch, sizeof ch, "enche_%d", j);
      fil_registrar(ch, ch, "X", "movie", 1);
    }
    assert(fil_n() == FIL_MAX);
    // Ninguem foi visto nem esta na home: a ultima e dispensavel e sai.
    for (j = 0; j < FIL_MAX; j++) fil_teste_esquecer_vista(j);
    fil_registrar("chegou_depois", "Chegou depois", "Y", "movie", 3);
    assert(fil_n() == FIL_MAX);
    assert(!strcmp(fil_chave(FIL_MAX - 1), "chegou_depois"));
    // A DE CIMA NAO SE MEXE: o despejo sai do fim, nao do topo.
    assert(!strcmp(fil_chave(0), "enche_0"));
    // Quem esta na home nao pode ser despejado. Marca a ultima como desenhada
    // e confere que a vitima passa a ser a anterior.
    { const char *home[1];
      home[0] = fil_chave(FIL_MAX - 1);
      fil_espelhar_ordem(home, NULL, 1);
      for (j = 0; j < FIL_MAX - 1; j++) fil_teste_esquecer_vista(j);
      fil_registrar("mais_uma", "Mais uma", "Y", "movie", 3);
      assert(fil_n() == FIL_MAX);
      assert(!strcmp(fil_chave(FIL_MAX - 1), "mais_uma"));
      // "chegou_depois" continua na lista: ela estava na home.
      { int achou = 0;
        for (j = 0; j < fil_n(); j++)
          if (!strcmp(fil_chave(j), "chegou_depois")) achou = 1;
        assert(achou); } } }
  puts("ok  tabela cheia despeja dispensavel e mantem o que esta na home");

  // E CONFIGURACAO DA PESSOA NAO E DESPEJADA POR FALTA DE ESPACO: com todas as
  // linhas desligadas (que e escolha dela), a nova fica de fora — mas o log
  // diz, que era o que faltava.
  fil_esquecer();
  { int j;
    char ch[32];
    for (j = 0; j < FIL_MAX; j++) {
      snprintf(ch, sizeof ch, "cfg_%d", j);
      fil_registrar(ch, ch, "X", "movie", 1);
      fil_remover(j);                 // desliga: e escolha da pessoa
      fil_teste_esquecer_vista(j);
    }
    assert(fil_n() == FIL_MAX);
    fil_registrar("nao_cabe", "Nao cabe", "Y", "movie", 3);
    assert(fil_n() == FIL_MAX);
    for (j = 0; j < fil_n(); j++) assert(strcmp(fil_chave(j), "nao_cabe") != 0); }
  puts("ok  tabela cheia nao despeja o que a pessoa configurou");

  // FORA DA COTA, MAS ESCOLHIVEL (#126). O catalogo que a cota de declaracoes
  // nao leu entra por fil_registrar_se_couber: aparece na lista para poder ser
  // ligado, nunca despeja ninguem, e fil_escolhida so responde por ele depois
  // que a pessoa o poe na home.
  fil_esquecer();
  { int j, i, estado = -1;
    char ch[32];
    fil_definir_limite(3);
    for (j = 0; j < 5; j++) {
      snprintf(ch, sizeof ch, "cand_%d", j);
      fil_registrar(ch, ch, "Ultra MAX", "movie", -1);
    }
    fil_registrar_se_couber("ultra_150", "Ultra 150", "Ultra MAX", "movie");
    assert(fil_n() == 6);
    assert(!strcmp(fil_chave(5), "ultra_150"));         // no fim, depois dos candidatos
    assert(fil_escolhida("cand_0") == 0);               // dentro do limite
    assert(fil_escolhida("cand_2") == 2);
    assert(fil_escolhida("cand_3") == -1);              // ligada so por ter entrado
    assert(fil_escolhida("ultra_150") == -1);           // listada, nao escolhida
    assert(fil_escolhida("nao_existe") == -1);
    // A folha abre (normaliza; #358: os candidatos NOVOS que nao couberam vao
    // para a fila, nao para "fora"), a pessoa tira uma da home — cand_3 sobe — e
    // poe a de fora da cota: agora ela e escolhida (fica na fila, home cheia).
    fil_normalizar();
    assert(fil_escolhida("ultra_150") == -1);
    fil_remover(0);
    assert(fil_escolhida("cand_0") == -1);              // oculta nao e escolha
    i = fil_adicionar(5, &estado);
    assert(i >= 0 && estado == FIL_NA_FILA);
    assert(fil_escolhida("ultra_150") >= 0);
    // Tabela cheia: o registro de fora da cota fica de fora, calado, e nao
    // despeja nem uma linha dispensavel.
    fil_esquecer();
    for (j = 0; j < FIL_MAX; j++) {
      snprintf(ch, sizeof ch, "cheia_%d", j);
      fil_registrar(ch, ch, "X", "movie", -1);
      fil_teste_esquecer_vista(j);
    }
    fil_registrar_se_couber("sem_lugar", "Sem lugar", "X", "movie");
    assert(fil_n() == FIL_MAX);
    for (j = 0; j < fil_n(); j++) assert(strcmp(fil_chave(j), "sem_lugar") != 0);
    assert(!strcmp(fil_chave(FIL_MAX - 1), "cheia_767") || FIL_MAX != 768);
    fil_definir_limite(FIL_LIMITE_PADRAO); }
  puts("ok  catalogo fora da cota e listado, escolhivel e nunca despeja");

  // FONTE DO DESTAQUE: sobrevive ao arquivo, e um arquivo escrito por uma
  // versao que nao a conhece continua valendo.
  fil_esquecer();
  usaArquivo = 1;
  remove("/tmp/fileirasui.txt");
  fil_teste_recarregar();
  assert(!strcmp(fil_hero_fonte(), ""));          // padrao: automatico
  fil_registrar("continuar", "Continuar assistindo", "", "", 5);
  fil_definir_hero_fonte("*");
  assert(!strcmp(fil_hero_fonte(), "*"));
  fil_teste_recarregar();
  assert(!strcmp(fil_hero_fonte(), "*"));         // veio do arquivo
  fil_definir_hero_fonte("continuar");
  fil_teste_recarregar();
  assert(!strcmp(fil_hero_fonte(), "continuar"));
  // Arquivo de uma versao ANTERIOR (sem a linha `hero`): le sem reclamar e o
  // destaque volta ao automatico, em vez de herdar lixo.
  { FILE *f = fopen("/tmp/fileirasui.txt", "w");
    assert(f);
    fputs("limite 7\nordem 1\nlinha continuar\t0\t0\t1\t0\tContinuar assistindo\n", f);
    fclose(f); }
  fil_teste_recarregar();
  assert(!strcmp(fil_hero_fonte(), ""));
  puts("ok  fonte do destaque grava, le e tolera arquivo antigo");

  // SEMENTE SO PARA O PERFIL 1. O fileirasui.txt antigo e a escolha de quem
  // usava o app antes dos perfis — que sincronizava sempre o perfil 1. Relato
  // do dono na C9: o perfil 2 abria com a ordem, o limite e o destaque do 1.
  // Perfil sem arquivo proprio que nao e o 1 comeca do padrao.
  fil_esquecer();
  usaArquivo = 1;
  remove("/tmp/fileirasui-p1.txt");
  remove("/tmp/fileirasui-p2.txt");
  { FILE *f = fopen("/tmp/fileirasui.txt", "w");
    assert(f);
    fputs("limite 11\nordem 1\nhero *\nlinha catDoUm\t0\t0\t1\t0\tDo perfil 1\n", f);
    fclose(f); }
  fil_definir_perfil(2);
  assert(fil_limite() == FIL_LIMITE_PADRAO);
  assert(fil_n() == 0);
  assert(!fil_tem_ordem());
  assert(!strcmp(fil_hero_fonte(), ""));
  fil_definir_perfil(1);
  assert(fil_limite() == 11);
  assert(fil_n() == 1 && !strcmp(fil_chave(0), "catDoUm"));
  assert(!strcmp(fil_hero_fonte(), "*"));
  // Voltar ao 2 continua sem nada do 1 (e o destaque zera, como antes).
  fil_definir_perfil(2);
  assert(fil_n() == 0 && !strcmp(fil_hero_fonte(), "") && fil_limite() == FIL_LIMITE_PADRAO);
  remove("/tmp/fileirasui.txt");
  remove("/tmp/fileirasui-p1.txt");
  remove("/tmp/fileirasui-p2.txt");
  fil_definir_perfil(0);
  usaArquivo = 0;
  puts("ok  fileirasui.txt antigo semeia so o perfil 1");

  // ISSUE #197 (UA55TU8200, Tizen 5.5): segurar a seta no "limite de
  // fileiras" passava por cada valor, e CADA passo para baixo mandava para
  // "Fora da Home" o que ficou alem dele. Log 12448: limite 15 -> 3 -> 20 e a
  // home ficou com "2 fileiras na tela (limite 20, 16 fileira(s) no
  // catalogo)". Agora a rajada so vale no fim da edicao, e so o valor final.
  fil_esquecer();
  fil_definir_limite(10);
  { int j, ocultas;
    char n[12][8];
    for (j = 0; j < 12; j++) { snprintf(n[j], sizeof n[j], "r%d", j); fil_registrar(n[j], n[j], "X", "movie", 1); }
    for (j = 10; j >= 3; j--) fil_ajustar_limite(j);    // desce ate 3...
    for (j = 4; j <= 20; j++) fil_ajustar_limite(j);    // ...e sobe ate 20
    fil_confirmar_limite();
    for (ocultas = 0, j = 0; j < fil_n(); j++) if (fil_linha_oculta(j)) ocultas++;
    assert(ocultas == 0 && fil_limite() == 20 && fil_n_na_home() == 12);
    // Rajada que TERMINA mais baixa: vale a decisao do dono, fora da home.
    for (j = 19; j >= 5; j--) fil_ajustar_limite(j);
    assert(fil_n_na_home() == 5 && fil_n_fila() == 7);   // antes de confirmar: nada escondido
    fil_confirmar_limite();
    for (ocultas = 0, j = 0; j < fil_n(); j++) if (fil_linha_oculta(j)) ocultas++;
    assert(ocultas == 7 && fil_n_fila() == 0);
    fil_confirmar_limite();                              // sem rajada: nada
    for (ocultas = 0, j = 0; j < fil_n(); j++) if (fil_linha_oculta(j)) ocultas++;
    assert(ocultas == 7); }
  puts("ok  #197: rajada no limite so esconde pelo valor final");

  // ISSUE #197, a outra metade: catalogo que a cota deixou de fora entra na
  // lista para ESCOLHA (fil_registrar_se_couber) e entrava LIGADO. Com vagas
  // sobrando na home (a rajada acima as abria), ele ficava dentro do limite,
  // fil_escolhida o dava como escolhido na TV e a volta seguinte o pedia: as
  // fileiras de ator do Xperience em ordem de manifesto (Mahershala Ali,
  // Matthew McConaughey...) apareceram na home sem ninguem as escolher.
  fil_esquecer();
  fil_definir_limite(20);
  { int j, est = -1, pf = -1;
    fil_registrar("cw", "Continuar", "", "", 3);
    fil_registrar("cand", "Trending", "Xperience", "movie", 12);
    fil_registrar_se_couber("fora_a", "Mahershala Ali", "Xperience", "movie");
    fil_registrar_se_couber("fora_b", "Matthew McConaughey", "Xperience", "movie");
    assert(fil_escolhida("fora_a") < 0 && fil_escolhida("fora_b") < 0);
    assert(fil_escolhida("cand") == 1);
    for (j = 0; j < fil_n(); j++) {
      if (!strcmp(fil_chave(j), "fora_a")) { assert(fil_estado(j) == FIL_FORA); pf = j; }
      if (!strcmp(fil_chave(j), "fora_b")) assert(fil_estado(j) == FIL_FORA);
    }
    // Nao e escolha da pessoa: a cota nao o trata como desligado (a ordem da
    // conta ainda pode pedi-lo) e ele nao ocupa vaga nem fila.
    assert(!fil_oculta("fora_a"));
    assert(fil_n_na_home() == 2 && fil_n_fila() == 0);
    // Quando a descoberta o pede (vira candidato), ele passa a ser ligado.
    fil_registrar("fora_b", "Matthew McConaughey", "Xperience", "movie", 12);
    for (j = 0; j < fil_n(); j++) if (!strcmp(fil_chave(j), "fora_b")) assert(fil_estado(j) == FIL_NA_HOME);
    // A pessoa ADICIONA pela aba "Fora": entra na home, que tem vaga.
    for (j = 0; j < fil_n(); j++) if (!strcmp(fil_chave(j), "fora_a")) pf = j;
    fil_adicionar(pf, &est);
    assert(est == FIL_NA_HOME && fil_escolhida("fora_a") >= 0);
    // E se ela a REMOVE, e escolha: fil_oculta passa a valer.
    for (j = 0; j < fil_n(); j++) if (!strcmp(fil_chave(j), "fora_a")) pf = j;
    fil_remover(pf);
    assert(fil_oculta("fora_a")); }
  puts("ok  #197: fora da cota entra fora da home, sem tomar vaga");

  // LIMPEZA UNICA DO #197 sobre um arquivo NO FORMATO DO RELATOR, reconstruido
  // dos logs 12448/12454/12489 (UA55TU8200): limite 12, ordem propria, 768
  // linhas; as fileiras que a home tinha antes da rajada (Movies, Series, AI for
  // you, IMDb Top 100...) ocultas; as de ator do Xperience em ordem de manifesto
  // (Mahershala Ali ... Paul Rudd) ligadas; o resto do Xperience oculto pela
  // normalizacao; um catalogo que a pessoa adicionou na TV (PenguPlay) e um que
  // ela tirou (HdHub) — esses dois nao podem mudar.
  { static const char *XP = "app.xperience.7d740fc3-61dd-4b90-8bd6-6eded683626e";
    static const char *BC = "com.aicat.4shiv27.nuvio";
    static const char *ATORES[] = { "mahershala_ali", "matthew_mcconaughey", "melissa_mccarthy",
      "meryl_streep", "michael_b_jordan", "michael_herbig", "michelle_yeoh", "mila_kunis",
      "millie_bobby_brown", "natalie_portman", "nicolas_cage", "nicole_kidman", "oscar_isaac",
      "owen_wilson", "pascal", "paul_mescal", "paul_rudd" };
    #define N_ATORES 17
    char conta[9][160];
    const char *contaP[9];
    int j, total = 0, restantes;
    FILE *f;
    snprintf(conta[0], 160, "%s_movie_trending_movies", XP);
    snprintf(conta[1], 160, "%s_movie_bw_spiderman", XP);
    snprintf(conta[2], 160, "%s_movie_movies", XP);
    snprintf(conta[3], 160, "%s_series_series", XP);
    snprintf(conta[4], 160, "%s_series_bw_safed_sagar", XP);
    snprintf(conta[5], 160, "%s_movie_actor_ana_de_armas_movies", XP);
    snprintf(conta[6], 160, "%s_movie_ai_for_you", BC);
    snprintf(conta[7], 160, "%s_series_ai_for_you", BC);
    snprintf(conta[8], 160, "%s_movie_imdb_top_100", XP);
    for (j = 0; j < 9; j++) contaP[j] = conta[j];
    fil_esquecer();
    usaArquivo = 1;
    f = fopen("/tmp/fileirasui.txt", "w");
    assert(f);
    fputs("# Fileiras da Home\nlimite 12\nordem 1\n", f);
    fputs("linha continue_watching\t0\t0\t1\t0\tContinuar assistindo\n", f); total++;
    fputs("linha social_activity\t0\t0\t1\t0\tEntre amigos\n", f); total++;
    fprintf(f, "linha %s\t0\t0\t1\t0\tTrending - Movie\n", conta[0]); total++;
    fprintf(f, "linha %s\t0\t0\t1\t0\tBecause you watched Spider-Man\n", conta[1]); total++;
    for (j = 2; j < 9; j++) { fprintf(f, "linha %s\t1\t0\t1\t0\tDa conta %d\n", conta[j], j); total++; }
    fputs("linha com.penguplay_movie_top\t0\t0\t1\t0\tPenguPlay Top\n", f); total++;
    fputs("linha com.stremio.HdHub_movie_latest\t1\t0\t1\t0\tHdHub Latest\n", f); total++;
    fputs("linha collection_streaming\t0\t0\t1\t0\tStreaming\n", f); total++;
    for (j = 0; j < N_ATORES; j++) { fprintf(f, "linha %s_movie_actor_%s_movies\t0\t0\t1\t0\tAtor %d - Movie\n", XP, ATORES[j], j); total++; }
    for (j = 0; total < FIL_MAX; j++, total++)
      fprintf(f, "linha %s_movie_outro_%d\t1\t0\t1\t0\tOutro %d\n", XP, j, j);
    fclose(f);
    fil_teste_recarregar();
    assert(fil_n() == FIL_MAX && fil_limite() == 12);
    assert(fil_migrar_197(contaP, 9) == 7 + N_ATORES);
    for (j = 2; j < 9; j++) assert(!fil_oculta(conta[j]));                 // (a) voltaram
    for (j = 0; j < fil_n(); j++) {
      const char *k = fil_chave(j);
      if (strstr(k, "_movie_actor_") && strstr(k, XP) && !strstr(k, "ana_de_armas"))
        assert(fil_estado(j) == FIL_FORA && !fil_oculta(k));              // (b) sugestao
      if (!strcmp(k, "com.penguplay_movie_top")) assert(fil_estado(j) != FIL_FORA);
      if (!strcmp(k, "com.stremio.HdHub_movie_latest")) assert(fil_oculta(k));
      if (strstr(k, "_movie_outro_")) assert(fil_oculta(k));
    }
    assert(fil_escolhida(conta[2]) >= 0 && fil_escolhida(conta[8]) >= 0);
    assert(fil_n_na_home() == 13 && fil_n_capacidade() == 10);
    assert(fil_n_fila() == 0); // CW/social/collection do not consume catalogue slots
    // UMA VEZ SO: a marca foi gravada no arquivo, e na memoria ja nao roda.
    { static char buf[300000]; size_t n; FILE *g = fopen("/tmp/fileirasui.txt", "r");
      assert(g); n = fread(buf, 1, sizeof buf - 1, g); buf[n] = 0; fclose(g);
      assert(strstr(buf, "\nmigracao 197\n")); }
    assert(fil_migrar_197(contaP, 9) == 0); }
  puts("ok  #197 limpeza: arquivo do relator volta a home dele, uma vez so");

  // A marca vale no disco: um arquivo com "migracao 197" nao e tocado, e um
  // arquivo SEM o padrao do defeito so ganha a marca.
  { FILE *f;
    const char *conta[] = { "xp_movie_a" };
    int j;
    fil_esquecer();
    f = fopen("/tmp/fileirasui.txt", "w");
    fputs("limite 7\nordem 1\nmigracao 197\nlinha xp_movie_a\t1\t0\t1\t0\tA\n", f);
    for (j = 0; j < 300; j++) fprintf(f, "linha xp_movie_i%d\t%d\t0\t1\t0\tI%d\n", j, j < 150, j);
    fclose(f);
    fil_teste_recarregar();
    assert(fil_migrar_197(conta, 1) == 0 && fil_oculta("xp_movie_a"));
    fil_esquecer();
    f = fopen("/tmp/fileirasui.txt", "w");
    fputs("limite 7\nordem 1\nlinha xp_movie_a\t1\t0\t1\t0\tA\nlinha xp_movie_b\t0\t0\t1\t0\tB\n", f);
    fclose(f);
    fil_teste_recarregar();
    assert(fil_migrar_197(conta, 1) == 0 && fil_oculta("xp_movie_a"));   // pequeno: fora do padrao
    { char buf[256]; size_t n; f = fopen("/tmp/fileirasui.txt", "r"); n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f);
      assert(strstr(buf, "migracao 197")); }
    assert(fil_migrar_197(conta, 1) == 0);
    fil_esquecer();
    remove("/tmp/fileirasui.txt");
    usaArquivo = 0; }
  puts("ok  #197 limpeza: marca no arquivo; fora do padrao nao mexe");

  // A HOME LE O MESMO ESTADO DO EDITOR (fil_estado_chave). Limite 3, quatro
  // ligadas: "d" e a fila e a home nao pode desenha-la; "z" desligada esta fora;
  // chave que a tabela nao conhece devolve -1 (a home nao a barra).
  fil_esquecer();
  fil_definir_limite(3);
  { const char *n[] = { "a", "b", "c", "d", "z" };
    int j;
    for (j = 0; j < 5; j++) fil_registrar(n[j], n[j], "X", "movie", 1);
    fil_remover(4);
    assert(fil_estado_chave("a") == FIL_NA_HOME && fil_estado_chave("c") == FIL_NA_HOME);
    assert(fil_estado_chave("d") == FIL_NA_FILA);
    assert(fil_estado_chave("z") == FIL_FORA);
    assert(fil_estado_chave("nunca") == -1 && fil_estado_chave("") == -1);
    fil_remover(0);                              // abre vaga: d entra sozinha
    assert(fil_estado_chave("d") == FIL_NA_HOME); }
  puts("ok  estado por chave: fila e fora nao entram na home");

  // #319: REMOVER (OU DESLIGAR) UM ADDON TIRA AS FILEIRAS DELE DA HOME. Log
  // S3R7Q0: Pluto TV desligado na conta e a poda "adiada: 1 addon(s) sem
  // manifesto lido" para sempre. O desligado nao tem id (nao entra na volta);
  // so o LIGADO sem id e volta incompleta, e quem decide isso e a descoberta.
  // Aqui: (1) a poda casa o removido mesmo com um desligado sem id na lista;
  // (2) a fileira COM escolha de um addon removido/desligado nao ocupa vaga do
  // limite — as reais depois dela continuam na home.
  fil_esquecer();
  { int j;
    const char *ids[]   = { "addonvivo", "" };               // 2o: desligado, sem id
    const char *bases[] = { "https://vivo.example", "https://off.example" };
    const int ativos[]  = { 1, 0 };
    usaArquivo = 1;
    { FILE *f = fopen("/tmp/fileirasui-p1.txt", "w");
      fprintf(f, "limite 3\nordem 1\n");
      // fantasma com escolha (forma = 2) ANTES das reais: sem o fix ele gasta
      // uma das 2 vagas e "real2" cai para a fila.
      fprintf(f, "linha addonmorto_movie_top\t0\t2\t1\tFantasma\n");
      fprintf(f, "linha addonvivo_movie_real1\t0\t0\t1\tReal1\n");
      fprintf(f, "linha addonvivo_movie_real2\t0\t0\t1\tReal2\n");
      fprintf(f, "linha addonvivo_movie_real3\t0\t0\t1\tReal3\n");
      fprintf(f, "linha addonmorto_movie_solto\t0\t0\t1\tSolto\n");  // sem escolha
      fclose(f); }
    fil_definir_perfil(1);
    fil_teste_recarregar();
    assert(fil_n() == 5);
    assert(fil_estado_chave("addonvivo_movie_real3") == FIL_NA_FILA);   // o defeito
    // a descoberta registra os dois catalogos do addon vivo nesta volta
    fil_registrar("addonvivo_movie_real1", "Real1", "Vivo", "movie", 5);
    fil_registrar("addonvivo_movie_real2", "Real2", "Vivo", "movie", 5);
    fil_registrar("addonvivo_movie_real3", "Real3", "Vivo", "movie", 5);
    // (1) a linha sem escolha do removido sai; a com escolha fica
    assert(fil_podar_catalogos(ids, bases, 2, 1) == 1);
    assert(fil_n() == 4);
    for (j = 0; j < fil_n(); j++) assert(strcmp(fil_chave(j), "addonmorto_movie_solto") != 0);
    // (2) a que ficou deixa de contar vaga e aparece como fora
    assert(fil_marcar_sem_addon(ids, bases, ativos, 2, 1) == 1);
    assert(fil_estado_chave("addonmorto_movie_top") == FIL_FORA);
    assert(fil_estado_chave("addonvivo_movie_real1") == FIL_NA_HOME);
    assert(fil_estado_chave("addonvivo_movie_real3") == FIL_NA_HOME);
    // lista de outro perfil / do pacote nao marca nada
    assert(fil_marcar_sem_addon(ids, bases, ativos, 2, 2) == 0);
    assert(fil_marcar_sem_addon(ids, bases, ativos, 2, 0) == 0);
    // o addon volta (ligado): a linha volta como estava, com a escolha
    { const int todos[] = { 1, 1 };
      const char *ids2[] = { "addonvivo", "addonmorto" };
      const char *bases2[] = { "https://vivo.example", "https://off.example" };
      assert(fil_marcar_sem_addon(ids2, bases2, todos, 2, 1) == 1);
      assert(fil_estado_chave("addonmorto_movie_top") != FIL_FORA);
      assert(fil_linha_tipo(0) == 2); }
    remove("/tmp/fileirasui-p1.txt");
    fil_definir_perfil(0);
    usaArquivo = 0; }
  puts("ok  #319: addon removido/desligado nao ocupa vaga da home e a poda nao fica adiada");

  // #365: FANTASMAS EM "FORA DA HOME". Removido o addon, as suas linhas
  // "Fora da Home" — a sugestao automatica (oculta=2) e a oculta impunida
  // que sobrou de arquivo antigo (oculta=1, sem mais escolha nenhuma) —
  // ficavam na aba "Fora da Home" para sempre: #204 so despeja o intruso
  // via orfaDespejavel (que exige !vista da volta ATUAL) e so acima de 25%
  // da tabela; a limpeza "na remocao do addon" entra no marcar_sem_addon,
  // que e o que sabe que o addon acabou — mas so da FILA INTACTA: sugestao,
  // ou oculta da pessoa sem visibilidade guardada (arquivo antigo), com
  // tamanho/forma padrao, sem fila e nao e a fonte do destaque.
  fil_esquecer();
  { const char *vivos[] = { "addonvivo", "addonmorto" };
    const char *bases[] = { "https://vivo.example", "https://off.example" };
    int ativos[] = { 1, 0 };   // addonmorto removido da conta
    char nomeLixo[FIL_CHAVE], tituloLixo[FIL_TITULO];
    int k, lixo = 0;
    usaArquivo = 1;
    { FILE *f = fopen("/tmp/fileirasui-p1.txt", "w");
      fprintf(f, "limite 16\n");
      fprintf(f, "linha addonvivo_movie_a\t0\t0\t1\tVivo A\n");
      // Oculta pela pessoa um dia, sem mais nada de escolha guardada.
      fprintf(f, "linha addonmorto_movie_pessoa\t1\t0\t1\tMorto Pessoa\n");
      // Sugestao automatica de fora-da-cota (oculta=2).
      fprintf(f, "linha addonmorto_movie_auto\t2\t0\t1\tMorto Auto\n");
      // Fora da home com escolha REAL (visPessoal): fica.
      fprintf(f, "linha addonmorto_movie_escolha\t1\t0\t1\tMorto Escolha\n");
      fprintf(f, "visibilidade addonmorto_movie_escolha\n");
      // Ligadas e indesejadas (#204 ja cuida pelo orfaDespejavel quando o
      // volume passa de 25%; aqui, ficam so marcadas fora — mas tem de
      // ganhar estado FIL_FORA na tela).
      for (k = 0; k < 100; k++) {
        snprintf(nomeLixo, sizeof nomeLixo, "addonmorto_movie_lixo%d", k);
        snprintf(tituloLixo, sizeof tituloLixo, "Lixo %d", k);
        fprintf(f, "linha %s\t0\t0\t1\t%s\n", nomeLixo, tituloLixo);
      }
      fclose(f); }
    fil_definir_perfil(1);
    assert(fil_n() == 4 + 100);
    assert(fil_teste_achar("addonmorto_movie_pessoa") >= 0);
    assert(fil_teste_achar("addonmorto_movie_auto") >= 0);
    assert(fil_teste_achar("addonmorto_movie_escolha") >= 0);
    // A volta da descoberta: so o vivo declara coisas; o marcar registra o
    // estado do addon que saiu — e limpa o que nao tem mais razao de ficar.
    fil_registrar("addonvivo_movie_a", "Vivo A", "Vivo", "movie", 5);
    { int mudado = fil_marcar_sem_addon(vivos, bases, ativos, 2, 1);
      assert(mudado >= 102); }
    assert(fil_teste_achar("addonmorto_movie_pessoa") < 0);
    assert(fil_teste_achar("addonmorto_movie_auto") < 0);
    // A escolha real (visPessoal) fica, como sempre ficou.
    assert(fil_teste_achar("addonmorto_movie_escolha") >= 0);
    // O vivo e o lixo ligado (sem oculta) ficam; 100 + 1 escolha + 1 vivo.
    assert(fil_n() == 102);
    for (k = 0; k < fil_n(); k++)
      if (!strncmp(fil_chave(k), "addonmorto_movie_lixo", 21)) lixo++;
    assert(lixo == 100);
    assert(fil_teste_achar("addonvivo_movie_a") >= 0);
    remove("/tmp/fileirasui-p1.txt");
    fil_definir_perfil(0);
    usaArquivo = 0; }
  puts("ok  #365: catalogo intocado de addon removido sai da aba Fora da Home");

  // #327: "Adicionada a Home" de um catalogo engolido por pasta de colecao nao
  // mudava nada (emColecao seguia ligado). A escolha local vence a colecao.
  { int est = -1, i, dentro[2] = { 1, 1 }, fora[2] = { 0, 0 };
    const char *chs[2] = { "x_movie_a", "x_movie_b" };
    fil_esquecer();
    fil_definir_limite(7);
    fil_registrar("x_movie_a", "A", "X", "movie", 5);
    fil_registrar("x_movie_b", "B", "X", "movie", 5);
    fil_conta_reconciliar(chs, fora, dentro, 2);
    assert(fil_estado_chave("x_movie_a") == FIL_FORA);
    assert(!fil_adicionada_na_tv("x_movie_a"));
    i = -1;
    { int k; for (k = 0; k < fil_n(); k++) if (!strcmp(fil_chave(k), "x_movie_a")) i = k; }
    assert(i >= 0);
    i = fil_adicionar(i, &est);
    assert(est == FIL_NA_HOME);
    assert(fil_estado_chave("x_movie_a") == FIL_NA_HOME);
    assert(fil_adicionada_na_tv("x_movie_a"));
    // a proxima reconciliacao (o sync) devolve o catalogo a pasta, a menos que
    // quem reconcilia respeite a escolha local — o contrato e fil_adicionada_na_tv
    assert(fil_adicionada_na_tv("x_movie_a") && !fil_adicionada_na_tv("x_movie_b"));
    fil_remover(i);
    assert(!fil_adicionada_na_tv("x_movie_a")); }
  puts("ok  #327: adicionar um catalogo de pasta de colecao o traz para a Home");
  // 2.0.4 (Discord + dono): catalogo DESLIGADO pela ordem da conta. OK no
  // editor dizia "Added to Home" e a linha ficava em "Not on Home"; o sync
  // seguinte tambem reaplicava o desligado.
  { int est = -1, i = -1, k, contaOff[1] = { 1 }, nada[1] = { 0 };
    const char *chs[1] = { "cine_movie_top" };
    fil_esquecer();
    fil_definir_limite(7);
    fil_registrar("cine_movie_top", "Featured", "Cinemeta", "movie", 5);
    fil_conta_reconciliar(chs, contaOff, nada, 1);
    assert(fil_estado_chave("cine_movie_top") == FIL_FORA);
    for (k = 0; k < fil_n(); k++) if (!strcmp(fil_chave(k), "cine_movie_top")) i = k;
    i = fil_adicionar(i, &est);
    assert(est == FIL_NA_HOME);
    assert(fil_estado_chave("cine_movie_top") == FIL_NA_HOME);
    fil_conta_reconciliar(chs, contaOff, nada, 1);   // o sync seguinte
    assert(fil_estado_chave("cine_movie_top") == FIL_NA_HOME);
    fil_remover(i);
    fil_conta_reconciliar(chs, contaOff, nada, 1);
    assert(fil_estado_chave("cine_movie_top") == FIL_FORA); }
  puts("ok  adicionar vence o desligado da conta, e o sync nao desfaz");

  // #360: o add com a conta LIGADA nao congela a fileira nesta TV. A conta e o
  // dono do estado (sync de mao unica): se ela desligar depois, esconde aqui
  // tambem. So o add FEITO contra o desligado ja conhecido vale para sempre.
  { int est = -1, i = -1, k, contaOn[1] = { 0 }, contaOff[1] = { 1 }, nada[1] = { 0 };
    const char *chs[1] = { "cine_movie_top" };
    fil_esquecer();
    fil_definir_limite(7);
    fil_registrar("cine_movie_top", "Featured", "Cinemeta", "movie", 5);
    fil_conta_reconciliar(chs, contaOn, nada, 1);        // a conta diz LIGADA
    assert(fil_estado_chave("cine_movie_top") == FIL_NA_HOME);
    for (k = 0; k < fil_n(); k++) if (!strcmp(fil_chave(k), "cine_movie_top")) i = k;
    i = fil_adicionar(i, &est);                          // snapshot: conta ligada
    assert(est == FIL_NA_HOME);
    fil_conta_reconciliar(chs, contaOff, nada, 1);       // a conta desligou DEPOIS
    assert(fil_estado_chave("cine_movie_top") == FIL_FORA); }
  puts("ok  #360: add com conta ligada nao trava o sync; a conta desligando depois vence");

  // E o "para sempre" do add contra o desligado sobrevive ao restart: o
  // snapshot viaja no arquivo (linha "addvence").
  { int est = -1, i = -1, k, contaOff[1] = { 1 }, nada[1] = { 0 };
    const char *chs[1] = { "cine_movie_top" };
    fil_esquecer();
    fil_definir_limite(7);
    fil_registrar("cine_movie_top", "Featured", "Cinemeta", "movie", 5);
    fil_conta_reconciliar(chs, contaOff, nada, 1);
    for (k = 0; k < fil_n(); k++) if (!strcmp(fil_chave(k), "cine_movie_top")) i = k;
    usaArquivo = 1;
    i = fil_adicionar(i, &est);                          // contra o desligado
    assert(est == FIL_NA_HOME);
    // reinicio: troca de perfil e volta rele o arquivo do disco
    fil_definir_perfil(1);
    fil_definir_perfil(0);
    fil_registrar("cine_movie_top", "Featured", "Cinemeta", "movie", 5);
    fil_conta_reconciliar(chs, contaOff, nada, 1);
    assert(fil_estado_chave("cine_movie_top") == FIL_NA_HOME);
    remove("/tmp/fileirasui.txt");
    usaArquivo = 0; }
  puts("ok  #360: add contra o desligado sobrevive ao restart (addvence no arquivo)");
  // #204: TABELA CHEIA DE CATALOGOS QUE NINGUEM DECLARA. Log MD8G2R: 768 linhas
  // velhas (auto-"Fora", oculta=1 sem visibilidade) e 60 catalogos novos do
  // BingeCat recusados. Cenario A: lixo > 25% (poda ja). Cenario B: lixo
  // pequeno, a tabela enche e o registro despeja o mais antigo.
  { int cen;
    for (cen = 0; cen < 2; cen++) {
      int j, nLixo = cen == 0 ? 700 : 100, nDecl = FIL_MAX - nLixo, nNovos = 60;
      const char *decl[FIL_MAX + 64];
      static char buf[FIL_MAX + 64][48];
      int nd = 0, achouEsc = 0, achouTipo = 0, achouDecl = 0, nNovosOk = 0;
      FILE *arq;
      fil_esquecer();
      usaArquivo = 1;
      fil_definir_perfil(0); fil_definir_perfil(1);
      arq = fopen("/tmp/fileirasui-p1.txt", "w");
      assert(arq);
      for (j = 0; j < nLixo; j++)   // oculta=1: "Fora" automatico, sem escolha
        fprintf(arq, "linha binge_seed_%d\t1\t0\t1\tSeed %d\n", j, j);
      for (j = 0; j < nDecl; j++)
        fprintf(arq, "linha vivo_%d\t0\t0\t1\tVivo %d\n", j, j);
      fprintf(arq, "visibilidade binge_seed_3\n");        // pessoa mexeu: fica
      fprintf(arq, "visibilidade vivo_0\n");
      fclose(arq);
      fil_teste_recarregar();
      assert(fil_n() == FIL_MAX);
      for (j = 0; j < nDecl; j++) { snprintf(buf[nd], 48, "vivo_%d", j); decl[nd] = buf[nd]; nd++; }
      for (j = 0; j < nNovos; j++) { snprintf(buf[nd], 48, "binge_novo_%d", j); decl[nd] = buf[nd]; nd++; }
      // forma escolhida numa orfa tambem protege
      { int q; for (q = 0; q < fil_n(); q++)
          if (!strcmp(fil_chave(q), "binge_seed_7")) fil_ciclar_tipo(q); }
      fil_marcar_declaradas(decl, nd, 1);
      for (j = 0; j < nNovos; j++) fil_registrar(decl[nDecl + j], decl[nDecl + j], "BingeCat", "series", 1);
      fil_gravar_registro();
      for (j = 0; j < fil_n(); j++) {
        const char *c = fil_chave(j);
        if (!strncmp(c, "binge_novo_", 11)) nNovosOk++;
        if (!strcmp(c, "binge_seed_3")) achouEsc = 1;
        if (!strcmp(c, "binge_seed_7")) achouTipo = 1;
        if (!strcmp(c, "vivo_0")) achouDecl++;
      }
      assert(nNovosOk == nNovos);          // as 60 novas entraram
      assert(achouEsc && achouTipo);       // escolha da pessoa sobreviveu
      assert(achouDecl == 1);              // declaradas intocadas
      { int vivos = 0; for (j = 0; j < fil_n(); j++) if (!strncmp(fil_chave(j), "vivo_", 5)) vivos++;
        assert(vivos == nDecl); }
      assert(fil_n() <= FIL_MAX);
      if (cen == 0) assert(fil_n() < 400);   // o arquivo encolheu
      // persistencia: relê e nada volta
      fil_teste_recarregar();
      fil_definir_perfil(0); fil_definir_perfil(1);
      { int vivos = 0, novos = 0, esc = 0;
        for (j = 0; j < fil_n(); j++) {
          const char *c = fil_chave(j);
          if (!strncmp(c, "vivo_", 5)) vivos++;
          if (!strncmp(c, "binge_novo_", 11)) novos++;
          if (!strcmp(c, "binge_seed_3") || !strcmp(c, "binge_seed_7")) esc++;
        }
        assert(vivos == nDecl && novos == nNovos && esc == 2); }
      remove("/tmp/fileirasui-p1.txt");
      fil_definir_perfil(0);
      usaArquivo = 0;
    } }
  puts("ok  #204: catalogo que nenhum addon declara sai antes de recusar o novo; escolha explicita fica");

  puts("fileiras: tudo ok");
  return 0;
}
