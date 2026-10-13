// LISTAS: leitura das respostas, o que fica gravado e o que chega na Home.
//
// ONDE ESTE TESTE ESCREVE, e por que isso e a PRIMEIRA coisa que ele confere.
// Um teste deste repositorio ja sobrescreveu os dados reais do dono. Aqui o
// script exporta NUVIO_DADOS para uma pasta temporaria e este programa RECUSA
// rodar se dados_dir() nao for exatamente ela — sem a guarda, um dados.c que
// mudasse a ordem dos candidatos levaria as gravacoes de volta para ~/.
//
// SEM REDE. Tudo o que envolve HTTP entra por arquivo de tests/fixtures: o que
// esta sob teste e a LEITURA das respostas e o efeito delas, nao o curl.
#include "listas.h"
#include "listasescrita.h"
#include "colecoes.h"
#include "dados.h"
#include "perfis.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
#define CONFERE(cond, ...) do { \
  if (!(cond)) { falhas++; printf("  FALHOU: "); printf(__VA_ARGS__); printf("\n"); } \
  else { printf("  ok: "); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

static char *ler(const char *caminho) {
  FILE *f = fopen(caminho, "rb");
  long n;
  char *b;
  if (!f) { printf("nao abri %s\n", caminho); exit(2); }
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
  b = malloc((size_t)n + 1);
  assert(b);
  assert(fread(b, 1, (size_t)n, f) == (size_t)n);
  b[n] = 0;
  fclose(f);
  return b;
}

// ---------------------------------------------------------------- 1. leitura

static void testeTraktMinhas(void) {
  char *j = ler("tests/fixtures/trakt_minhas_listas.json");
  int n = lst_ler_trakt(j, 0);
  const LstLista *a, *b;
  printf("\n[1] /users/me/lists\n");
  CONFERE(n == 2, "duas listas lidas (veio %d)", n);
  a = lst_lista(0); b = lst_lista(1);
  CONFERE(a && !strcmp(a->titulo, "Clássicos do terror"), "titulo da primeira");
  CONFERE(a && a->traktId == 1049904, "id 1049904 (veio %ld)", a ? a->traktId : -1);
  // O OBJETO `user` TAMBEM TEM `ids`. Se a leitura casasse a primeira ocorrencia
  // do nome no texto, uma resposta com `user` antes de `ids` devolveria o slug
  // do dono como id da lista — e a lista abriria vazia, sem erro nenhum.
  CONFERE(a && !strcmp(a->autor, "iqui27"), "autor da raiz, nao do ids aninhado");
  CONFERE(a && a->itens == 42, "item_count 42 (veio %d)", a ? a->itens : -1);
  CONFERE(a && a->curtidas == 3, "likes 3 (veio %d)", a ? a->curtidas : -1);
  CONFERE(b && b->traktId == 2200311, "id da segunda");
  // Cabecalho da pagina da lista (6B).
  CONFERE(a && !strcmp(a->slug, "classicos-do-terror"), "slug da lista (veio %s)", a ? a->slug : "");
  CONFERE(a && !strcmp(a->autorNome, "Henrique"), "nome do autor (veio %s)", a ? a->autorNome : "");
  CONFERE(a && !strcmp(a->autorSlug, "iqui27"), "slug do autor (veio %s)", a ? a->autorSlug : "");
  CONFERE(a && !strncmp(a->criada, "2024-10-01", 10), "created_at (veio %s)", a ? a->criada : "");
  CONFERE(a && !strncmp(a->atualizada, "2026-09-01", 10), "updated_at (veio %s)", a ? a->atualizada : "");
  CONFERE(a && a->comentarios == 0, "comment_count 0 (veio %d)", a ? a->comentarios : -9);
  CONFERE(a && !a->privada, "publica");
  CONFERE(b && b->privada, "a segunda e privada");
  free(j);
}

static void testeExtraItem(void) {
  const char *e =
    "{\"rank\": 3,\"id\":5521,\"listed_at\":\"2026-09-04T18:52:07.000Z\",\"notes\":\"Melhor do ano\","
    "\"type\":\"movie\",\"movie\":{\"title\":\"X\",\"notes\":\"falsa\",\"ids\":{\"imdb\":\"tt1\"}}}";
  const char *v = "{\"rank\":1,\"notes\":\"  \",\"movie\":{}}";
  DescItemLista d;
  printf("\n[1b] rank, listed_at e nota do item\n");
  desc_trakt_extra_ler(e, e + strlen(e), &d);
  CONFERE(d.rank == 3, "rank 3 (veio %d)", d.rank);
  CONFERE(d.itemId == 5521, "id da entrada, nao o de movie.ids (veio %ld)", d.itemId);
  CONFERE(d.entrouMs == 1788547927000LL, "listed_at em ms (veio %lld)", d.entrouMs);
  CONFERE(!strcmp(d.nota, "Melhor do ano"), "nota da raiz, nao a do filme (veio %s)", d.nota);
  desc_trakt_extra_ler(v, v + strlen(v), &d);
  CONFERE(d.rank == 1 && d.entrouMs == 0 && d.itemId == 0, "sem listed_at nem id fica 0");
  CONFERE(!d.nota[0], "nota so de espacos vira vazia");
}

static void testeTraktBusca(void) {
  char *j = ler("tests/fixtures/trakt_busca_listas.json");
  int n = lst_ler_trakt(j, 1);
  const LstLista *a;
  printf("\n[2] /search/list (envelope)\n");
  // O terceiro elemento e um resultado de FILME. A busca do Trakt devolve tipos
  // misturados quando o chamador nao filtra, e um filme virando "lista" seria
  // um cartao que nao abre nada.
  CONFERE(n == 2, "duas listas, o resultado de filme fora (veio %d)", n);
  a = lst_lista(0);
  CONFERE(a && !strcmp(a->titulo, "A24 — tudo"), "titulo de dentro do envelope");
  CONFERE(a && a->traktId == 8842211, "id 8842211 (veio %ld)", a ? a->traktId : -1);
  CONFERE(a && !strcmp(a->autor, "cinefilo_br"), "autor publico");
  CONFERE(a && a->curtidas == 901, "curtidas 901 (veio %d)", a ? a->curtidas : -1);
  free(j);
}

static void testeSimkl(void) {
  char *j = ler("tests/fixtures/simkl_plantowatch_movies.json");
  CatItem v[16];
  int n = lst_ler_simkl_itens(j, 0, v, 16);
  printf("\n[3] /sync/all-items/movies/plantowatch\n");
  // Sem imdb nao ha como pedir arte ao Cinemeta nem abrir o detalhe: o item
  // seria um cartaz cinza que nao responde. Fica de fora.
  CONFERE(n == 2, "dois itens; o sem imdb fora (veio %d)", n);
  CONFERE(!strcmp(v[0].titulo, "Aftersun"), "titulo do primeiro");
  CONFERE(!strcmp(v[0].imdb, "tt19770238"), "imdb de dentro de ids (veio %s)", v[0].imdb);
  CONFERE(!strcmp(v[0].tipo, "movie"), "tipo movie");
  CONFERE(!strcmp(v[0].meta, "2022"), "ano em meta (veio %s)", v[0].meta);
  CONFERE(!strcmp(v[1].imdb, "tt6710474"), "imdb do segundo");
  free(j);
}

// ------------------------------------------------- 2. fixar sobrevive ao boot

static LstLista listaTrakt(long id, const char *nome) {
  LstLista l;
  memset(&l, 0, sizeof l);
  l.fonte = LST_TRAKT;
  l.traktId = id;
  l.itens = -1;
  l.curtidas = -1;
  snprintf(l.titulo, sizeof l.titulo, "%s", nome);
  snprintf(l.autor, sizeof l.autor, "iqui27");
  snprintf(l.midia, sizeof l.midia, "MOVIE");
  return l;
}

static void testeFixarSobrevive(void) {
  LstLista l = listaTrakt(1049904, "Clássicos do terror");
  printf("\n[4] fixar sobrevive a um reinicio\n");
  perfis_definir_ativo(1);
  lst_iniciar();
  CONFERE(!lst_fixada(&l), "comeca sem nada fixado");
  CONFERE(lst_alternar_fixada(&l) == 1, "fixa e devolve 1");
  CONFERE(lst_fixada(&l), "fica fixada na mesma sessao");

  // O REINICIO, do jeito que o app faz: o processo morre com o estado em
  // memoria e a proxima sessao le do arquivo do perfil. lst_esquecer + lst_iniciar
  // e exatamente esse par.
  lst_esquecer();
  lst_iniciar();
  CONFERE(lst_fixada(&l), "continua fixada depois do reinicio");

  lst_pedir_fixadas();
  CONFERE(lst_n() == 1, "a aba Fixadas mostra uma (veio %d)", lst_n());
  CONFERE(lst_lista(0) && lst_lista(0)->traktId == 1049904, "e a certa");
}

// --------------------------------------------------- 3. isolamento por perfil

static void testeIsolamentoPerfil(void) {
  LstLista a = listaTrakt(1049904, "Clássicos do terror");
  LstLista b = listaTrakt(8842211, "A24 — tudo");
  printf("\n[5] cada perfil tem as suas\n");
  perfis_definir_ativo(2);
  lst_esquecer();
  lst_iniciar();
  CONFERE(!lst_fixada(&a), "o perfil 2 nao herda a fixada do perfil 1");
  CONFERE(lst_alternar_fixada(&b) == 1, "o perfil 2 fixa a sua");
  CONFERE(lst_fixada(&b) && !lst_fixada(&a), "o perfil 2 ve so a dele");

  perfis_definir_ativo(1);
  lst_esquecer();
  lst_iniciar();
  CONFERE(lst_fixada(&a), "o perfil 1 continua com a dele");
  CONFERE(!lst_fixada(&b), "e nao ve a do perfil 2");
  { char esperado[64];
    snprintf(esperado, sizeof esperado, "listas-p1.txt");
    CONFERE(!strcmp(lst_arquivo(), esperado),
            "o arquivo do perfil 1 e %s (veio %s)", esperado, lst_arquivo()); }
}

// ----------------------------------------------- 4. a Home recebe uma fileira

// O QUE A HOME DESENHA de um grupo de colecao: ela varre col_folder(i), agrupa
// por `group` e monta uma fileira com a chave col_chave_grupo(group) (ver o
// bloco `if (col_n())` em home.c). Entao "a Home teria uma fileira" e
// exatamente: existe pasta com este grupo, com fonte utilizavel.
static int pastaDaLista(long id, ColFolder *saida) {
  char alvo[96];
  int i;
  snprintf(alvo, sizeof alvo, "lista_trakt_%ld", id);
  for (i = 0; i < col_n(); i++) {
    const ColFolder *f = col_folder(i);
    if (f && !strcmp(f->id, alvo)) { if (saida) *saida = *f; return 1; }
  }
  return 0;
}

static void testeAdicionarNaHome(void) {
  LstLista l = listaTrakt(1049904, "Clássicos do terror");
  ColFolder f;
  char chave[192];
  printf("\n[6] adicionar a Home produz a fileira\n");
  perfis_definir_ativo(1);
  lst_esquecer();
  lst_iniciar();
  lst_alternar_fixada(&l);
  CONFERE(!pastaDaLista(1049904, NULL), "antes da acao nao ha pasta");
  CONFERE(lst_alternar_home(&l) == 1, "a acao devolve 1 (ligada)");
  CONFERE(pastaDaLista(1049904, &f), "a pasta existe em col_folder()");
  CONFERE(f.nSources == 1, "com uma fonte (veio %d)", f.nSources);
  // REUSO, e nao mecanismo novo: e o MESMO ColSource que colecoes.c monta para
  // provider "trakt" do editor do site, e o mesmo que desc_vertudo_fonte busca
  // em /lists/<id>/items.
  CONFERE(!strcmp(f.sources[0].prov, "trakt"), "provedor trakt (veio %s)", f.sources[0].prov);
  CONFERE(f.sources[0].traktLista == 1049904, "id da lista na fonte (veio %ld)",
          f.sources[0].traktLista);
  CONFERE(!strcmp(f.group, "Minhas listas"), "grupo da fileira (veio %s)", f.group);
  col_chave_grupo(f.group, chave, sizeof chave);
  CONFERE(!strncmp(chave, "collection_", 11),
          "a chave de fileira e de colecao: %s", chave);

  // O SYNC DA CONTA NAO PODE APAGAR A FILEIRA. col_definir_json reconstroi
  // folders[] do zero a cada pull; sem a reinjecao de col_extra_definir a
  // fileira sumiria sozinha alguns segundos depois do arranque.
  { const char *json =
      "[{\"id\":\"c1\",\"title\":\"A24\",\"folders\":[{\"id\":\"f1\",\"title\":\"Dramas\","
      "\"sources\":[{\"provider\":\"addon\",\"addonBaseUrl\":\"https://x.tv\","
      "\"type\":\"movie\",\"catalogId\":\"top\"}]}]}]";
    int novas = col_definir_json(json);
    CONFERE(novas == 1, "o pull da conta trouxe 1 pasta (veio %d)", novas);
    CONFERE(pastaDaLista(1049904, NULL), "a pasta da lista sobreviveu ao pull"); }

  CONFERE(lst_alternar_home(&l) == 0, "desligar devolve 0");
  CONFERE(!pastaDaLista(1049904, NULL), "e a pasta sai de col_folder()");

  // Desfixar tambem tira da Home: uma fileira que nenhuma tela sabe desligar
  // seria um beco sem saida.
  lst_alternar_home(&l);
  CONFERE(pastaDaLista(1049904, NULL), "religada");
  lst_alternar_fixada(&l);
  CONFERE(!lst_fixada(&l) && !pastaDaLista(1049904, NULL),
          "desfixar leva a fileira junto");
}

// --------------------------------------------- 5. o logout leva tudo junto

static void testeLogout(void) {
  LstLista l = listaTrakt(1049904, "Clássicos do terror");
  printf("\n[7] o logout esquece as fixadas\n");
  perfis_definir_ativo(1);
  lst_esquecer();
  lst_iniciar();
  lst_alternar_fixada(&l);
  lst_alternar_home(&l);
  CONFERE(lst_fixada(&l) && pastaDaLista(1049904, NULL), "fixada e na Home");
  // O LOGOUT chama isto (ver a nota em listas.h sobre a linha que falta em
  // ajustes.c). O teste chama a funcao direto porque e ela que esta sob teste.
  lst_esquecer_conta();
  CONFERE(!pastaDaLista(1049904, NULL), "a pasta sai da Home no logout");
  lst_iniciar();
  CONFERE(!lst_fixada(&l), "e o arquivo do perfil nao a devolve");
}

// ---------------------------------------------------------------- escrita

static char envMetodo[8], envUrl[160], envCorpo[2400];
static int envStatus = 200;
static int envio(const char *m, const char *u, const char *c, char **resp) {
  snprintf(envMetodo, sizeof envMetodo, "%s", m);
  snprintf(envUrl, sizeof envUrl, "%s", u);
  snprintf(envCorpo, sizeof envCorpo, "%s", c);
  *resp = strdup("{}");
  return envStatus;
}

static void testeEscrita(void) {
  char c[2400];
  long ord[3] = { 11, 22, 33 };
  printf("\n[9] escrita nas listas do Trakt\n");
  CONFERE(lstesc_corpo_criar(c, sizeof c, "Noites \"frias\"", "linha1\nlinha2", 1) &&
          !strcmp(c, "{\"name\":\"Noites \\\"frias\\\"\",\"description\":\"linha1\\nlinha2\","
                     "\"privacy\":\"private\"}"), "criar: escapa aspas e quebra (%s)", c);
  CONFERE(!lstesc_corpo_criar(c, sizeof c, "", "x", 0), "criar sem nome e recusado");
  CONFERE(lstesc_corpo_item(c, sizeof c, "tt0111161", "movie", "") &&
          !strcmp(c, "{\"movies\":[{\"ids\":{\"imdb\":\"tt0111161\"}}]}"), "item sem nota (%s)", c);
  CONFERE(lstesc_corpo_item(c, sizeof c, "tt0903747:1:2", "series", "Piloto") &&
          !strcmp(c, "{\"shows\":[{\"ids\":{\"imdb\":\"tt0903747\"},\"notes\":\"Piloto\"}]}"),
          "serie vira shows, id sem :t:e e com notes (%s)", c);
  CONFERE(lstesc_corpo_ordem(c, sizeof c, ord, 3) && !strcmp(c, "{\"rank\":[11,22,33]}"), "ordem (%s)", c);
  ord[1] = 0;
  CONFERE(!lstesc_corpo_ordem(c, sizeof c, ord, 3), "ordem com id 0 e recusada");
  CONFERE(!lstesc_corpo_ordem(c, 12, (long[]){1000000, 2000000}, 2), "ordem que nao cabe e recusada");

  lstesc_teste_envio(envio);
  CONFERE(lstesc_descricao(77, "Nova") == 1, "descricao dispara");
  CONFERE(lstesc_estado() == LSTESC_PENDENTE || lstesc_estado() == LSTESC_OK, "estado andou");
  lstesc_teste_esperar();
  CONFERE(!strcmp(envMetodo, "PUT") && !strcmp(envUrl, "https://api.trakt.tv/users/me/lists/77") &&
          !strcmp(envCorpo, "{\"description\":\"Nova\"}"), "PUT /users/me/lists/77 (%s %s)", envMetodo, envUrl);
  CONFERE(lstesc_consumir() == LSTESC_OK && lstesc_estado() == LSTESC_OCIOSO, "2xx = OK, e consumir zera");
  envStatus = 401;
  CONFERE(lstesc_adicionar(77, "tt1", "movie", NULL) == 1, "adicionar dispara");
  lstesc_teste_esperar();
  CONFERE(!strcmp(envMetodo, "POST") && !strcmp(envUrl, "https://api.trakt.tv/users/me/lists/77/items"),
          "POST .../items (%s)", envUrl);
  CONFERE(lstesc_consumir() == LSTESC_FALHA, "401 = FALHA");
  envStatus = 200;
  { long ids[2] = { 5, 6 };
    CONFERE(lstesc_ordenar(77, ids, 2) == 1, "ordenar dispara");
    lstesc_teste_esperar();
    CONFERE(!strcmp(envUrl, "https://api.trakt.tv/users/me/lists/77/items/reorder") &&
            !strcmp(envCorpo, "{\"rank\":[5,6]}"), "POST .../items/reorder (%s)", envCorpo);
    lstesc_consumir(); }
  // "Adicionar a uma lista" do menu do titulo: as listas da pessoa e o POST do titulo.
  { LstLista l;
    CatItem ci;
    lst_minhas_teste("[{\"name\":\"Noites\",\"ids\":{\"trakt\":901,\"slug\":\"noites\"},\"item_count\":3},"
                     "{\"name\":\"Cult\",\"ids\":{\"trakt\":902,\"slug\":\"cult\"}}]");
    CONFERE(lst_minhas_n() == 2 && lst_minha(1, &l) && !strcmp(l.titulo, "Cult") && l.traktId == 902,
            "minhas: duas listas, a segunda e Cult/902 (n=%d)", lst_minhas_n());
    CONFERE(!lst_minha(2, &l) && !lst_minha(-1, &l), "minhas: fora do indice recusa");
    memset(&ci, 0, sizeof ci);
    snprintf(ci.imdb, sizeof ci.imdb, "tt0111161");
    snprintf(ci.tipo, sizeof ci.tipo, "movie");
    snprintf(ci.titulo, sizeof ci.titulo, "Um Sonho de Liberdade");
    lst_minha(0, &l);
    CONFERE(lstesc_adicionar_titulo(&ci, &l) == 1, "adicionar titulo dispara");
    lstesc_teste_esperar();
    CONFERE(!strcmp(envUrl, "https://api.trakt.tv/users/me/lists/901/items") &&
            !strcmp(envCorpo, "{\"movies\":[{\"ids\":{\"imdb\":\"tt0111161\"}}]}"),
            "titulo vai para a lista escolhida (%s %s)", envUrl, envCorpo);
    lstesc_consumir(); }
  CONFERE(!lstesc_descricao(0, "x") && !lstesc_criar("", "", 0), "id 0 e nome vazio recusados");
  lstesc_teste_envio(NULL);
  CONFERE(!lstesc_criar("X", "", 0), "sem token nao escreve");
}

// ---------------------------------------------------------------- reordenar

static void testeOrdem(void) {
  CatItem v[3], c;
  DescItemLista x[3];
  long ids[8];
  LstLista l;
  char u[160];
  int i;
  printf("\n[10] reordenar e link publico\n");
  memset(v, 0, sizeof v); memset(x, 0, sizeof x);
  for (i = 0; i < 3; i++) {
    snprintf(v[i].titulo, sizeof v[i].titulo, "T%d", i);
    snprintf(v[i].imdb, sizeof v[i].imdb, "tt%d", i);
    x[i].rank = i + 1; x[i].itemId = 100 + i;
  }
  lst_teste_itens(v, 3); lst_teste_extras(x, 3);
  CONFERE(lst_ordem_ids(ids, 8) == 3 && ids[0] == 100 && ids[2] == 102, "ids na ordem do servidor");
  CONFERE(lst_ordem_mover(0, 2) == 1, "mover 0 -> 2");
  lst_item(0, &c);
  CONFERE(!strcmp(c.titulo, "T1"), "a posicao 0 agora e T1 (veio %s)", c.titulo);
  lst_item(2, &c);
  CONFERE(!strcmp(c.titulo, "T0"), "T0 foi para o fim (veio %s)", c.titulo);
  CONFERE(lst_ordem_ids(ids, 8) == 3 && ids[0] == 101 && ids[1] == 102 && ids[2] == 100,
          "ids seguem a tela (%ld %ld %ld)", ids[0], ids[1], ids[2]);
  lst_ordem_mover(2, 0);
  CONFERE(lst_ordem_ids(ids, 8) == 3 && ids[0] == 100 && ids[1] == 101, "voltar 2 -> 0 desfaz");
  CONFERE(!lst_ordem_mover(0, 3) && !lst_ordem_mover(-1, 1), "fora do intervalo recusa");
  x[1].itemId = 0;
  lst_teste_extras(x, 3);
  CONFERE(!lst_ordem_ids(ids, 8), "item sem id impede gravar a ordem");

  memset(&l, 0, sizeof l);
  l.fonte = LST_TRAKT;
  snprintf(l.autorSlug, sizeof l.autorSlug, "iqui27");
  snprintf(l.slug, sizeof l.slug, "classicos-do-terror");
  CONFERE(lst_url_publica(&l, u, sizeof u) && !strcmp(u, "https://trakt.tv/users/iqui27/lists/classicos-do-terror"),
          "url publica (%s)", u);
  l.privada = 1;
  CONFERE(!lst_url_publica(&l, u, sizeof u), "privada nao gera link");
}

// ---------------------------------------------------------------------- main

int main(void) {
  const char *quero = getenv("NUVIO_DADOS");
  if (!quero || !*quero) {
    printf("RECUSADO: rode por tests/listas.sh — NUVIO_DADOS nao esta definido.\n");
    return 2;
  }
  dados_iniciar(quero);
  if (strcmp(dados_dir(), quero)) {
    printf("RECUSADO: dados_dir()=\"%s\" nao e a pasta do teste \"%s\".\n",
           dados_dir(), quero);
    return 2;
  }
  printf("gravando em %s\n", dados_dir());

  testeTraktMinhas();
  testeExtraItem();
  testeTraktBusca();
  testeSimkl();
  testeFixarSobrevive();
  testeIsolamentoPerfil();
  testeAdicionarNaHome();
  testeLogout();
  testeEscrita();
  testeOrdem();

  printf("\n%s (%d falha%s)\n", falhas ? "FALHOU" : "PASSOU", falhas,
         falhas == 1 ? "" : "s");
  return falhas ? 1 : 0;
}
