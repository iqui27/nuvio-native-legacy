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
// NULL ate o teste da tabela cheia escrever o arquivo de verdade: linha lida
// do disco nasce com vista=0, que e o que separa "morta" de "ainda nao montou"
// no resgate de fileiras.c. Registrar pela API marcaria vista=1 e o teste
// mediria outra coisa.
static int usaArquivo = 0;
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  if (!usaArquivo) { (void)dst; (void)tam; (void)nome; return NULL; }
  snprintf(dst, tam, "/tmp/%s", nome);
  return dst;
}
void fil_teste_recarregar(void);
void fil_teste_esquecer_vista(int i);

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
      if (!strcmp(k, "d")) assert(fil_linha_oculta(j));           // sobrou: fora
      if (!strcmp(k, "e")) assert(!fil_linha_oculta(j));          // na home: fica
      if (!strcmp(k, "f")) assert(!fil_linha_oculta(j) && fil_estado(j) == FIL_NA_FILA);
    } }
  puts("ok  normalizar: so a fila pedida fica alem do limite");

  // PODA: catalogo de addon que sumiu da conta sai; o resto fica.
  fil_esquecer();
  { int j;
    const char *ids[] = { "addonvivo" }; const char *bases[] = { "https://vivo.example" };
    // Nada visto == tudo candidato; mas so catalogo de addon ausente cai.
    // Antes: simula linha lida do disco (vista=0) para o fantasma.
    usaArquivo = 1;
    { FILE *f = fopen("/tmp/fileirasui.txt", "w");
      fprintf(f, "limite 7\nordem 0\n");
      fprintf(f, "linha addonvivo_movie_top\t0\t0\t1\tTop\n");
      fprintf(f, "linha addonmorto_movie_top\t0\t0\t1\tFantasma\n");
      fprintf(f, "linha continue_watching\t0\t0\t1\tContinuar\n");
      fprintf(f, "linha collection_x\t0\t0\t1\tColecao\n");
      fclose(f); }
    fil_teste_recarregar();
    assert(fil_n() == 4);
    // O vivo foi VISTO nesta sessao; o fantasma nao. So o fantasma cai.
    fil_registrar("addonvivo_movie_top", "Top", "Vivo", "movie", 5);
    assert(fil_podar_catalogos(ids, bases, 1) == 1);
    assert(fil_n() == 3);
    for (j = 0; j < fil_n(); j++) assert(strcmp(fil_chave(j), "addonmorto_movie_top") != 0);
    // Chamar de novo nao tira mais nada.
    assert(fil_podar_catalogos(ids, bases, 1) == 0);
    usaArquivo = 0; }
  puts("ok  poda: catalogo de addon removido sai, app e colecao ficam");

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

  puts("fileiras: tudo ok");
  return 0;
}
