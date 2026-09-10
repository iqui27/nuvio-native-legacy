// A biblioteca e os vistos da conta chegam ao catalogo?
//
// ESTE TESTE EXISTE PORQUE O DEFEITO ERA MUDO. `puxarSoLeitura` em sync.c
// chamava `sync_pull_library` e `sync_pull_watched_items`, contava as linhas e
// liberava o corpo. Nao havia erro, nao havia log de falha: a conta respondia,
// o resumo dos ajustes dizia "12 na lista" e a tela de Biblioteca continuava
// vazia com o selo "LOCAL". Foi o issue do @Haylefal na LG webOS 4.
//
// NAO PRECISA DE REDE. Os corpos abaixo sao transcritos do contrato da secao
// 1.5 do PLANO-CONTA-SYNC.md, conferido contra o app web
// (NuvioWeb-0.3.38-beta, js/core/profile/savedLibrarySyncService.js e
// watchedItemsSyncService.js) — inclusive as variacoes que o servidor produz na
// vida real: `poster` vazio, `background` null, `imdb_rating` como string,
// `added_at` ora como numero em ms, ora como timestamp ISO.
//
// O catalogo aqui e um dubl: contalib.c so precisa de seis funcoes dele, e
// linkar catalogo.c traria descoberta, rede e SDL junto.
#include <stdio.h>
#include <string.h>
#include "contalib.h"
#include "catalogo.h"

static int falhas;

static void confere(const char *o_que, int obtido, int esperado) {
  int ok = obtido == esperado;
  printf("  %-58s %s (obtido %d, esperado %d)\n", o_que, ok ? "ok    " : "FALHOU",
         obtido, esperado);
  if (!ok) falhas++;
}

static void confereTexto(const char *o_que, const char *obtido, const char *esperado) {
  int ok = obtido && !strcmp(obtido, esperado);
  printf("  %-58s %s (obtido \"%s\", esperado \"%s\")\n", o_que,
         ok ? "ok    " : "FALHOU", obtido ? obtido : "(nulo)", esperado);
  if (!ok) falhas++;
}

// ---------------------------------------------------------------- dubl do catalogo

#define FALSO_MAX 32
static CatItem cat[FALSO_MAX];
static int catN;

// i18n() DEVOLVENDO A CHAVE, de proposito: este teste confere o PORTUGUES que
// contalib compoe ("Programa de TV · Drama"), e um duble que traduzisse mudaria
// o que esta sendo verificado. O que o duble prova, junto com o codigo, e que a
// composicao PASSA pela traducao — se alguem tirar a chamada, o app volta a
// misturar idiomas na home (#23) e este arquivo nem pisca. Por isso a assercao
// que importa esta no i18n.sh, que varre as chaves; aqui so nao pode quebrar o
// link.
const char *i18n(const char *s) { return s; }

int cat_n(void) { return catN; }

const CatItem *cat_item(int i) {
  // O de verdade envolve o indice (`itens[((i % n) + n) % n]`) em vez de
  // devolver NULL. Copiado de proposito: contalib_reconciliar depende desse
  // comportamento para conferir a marca sem estourar o vetor.
  if (catN < 1) return 0;
  return &cat[((i % catN) + catN) % catN];
}

static int mesmoTitulo(const char *a, const char *b) {
  while (*a && *b && *a != ':' && *b != ':') { if (*a != *b) return 0; a++; b++; }
  return (!*a || *a == ':') && (!*b || *b == ':');
}

int cat_indice_por_imdb(const char *imdb) {
  int i;
  if (!imdb || !imdb[0]) return -1;
  for (i = 0; i < catN; i++)
    if (cat[i].imdb[0] && mesmoTitulo(cat[i].imdb, imdb)) return i;
  return -1;
}

void cat_definir_na_lista(int i, int naLista) {
  if (i >= 0 && i < catN) cat[i].naLista = naLista ? 1 : 0;
}

int cat_acrescentar_lote(const CatItem *v, int qtd, int *saidaIdx) {
  int k;
  // O de verdade RECUSA com catalogo vazio (`if (!v || qtd < 1 || n < 1)`), e
  // essa recusa e o caso que a reconciliacao tem de sobreviver: no arranque sem
  // cache o sync termina antes de existir catalogo nenhum.
  if (!v || qtd < 1 || catN < 1) return 0;
  if (catN + qtd > FALSO_MAX) qtd = FALSO_MAX - catN;
  for (k = 0; k < qtd; k++) {
    cat[catN] = v[k];
    if (saidaIdx) saidaIdx[k] = catN;
    catN++;
  }
  return qtd;
}

static char histId[16][24];
static char histTipo[16][8];
static int  nHist;

void cat_historico_definir_id(const char *imdb, const char *tipo, int visto) {
  if (!visto || nHist >= 16) return;
  snprintf(histId[nHist], sizeof histId[nHist], "%s", imdb ? imdb : "");
  snprintf(histTipo[nHist], sizeof histTipo[nHist], "%s", tipo ? tipo : "");
  nHist++;
}

static int histTem(const char *imdb) {
  int i;
  for (i = 0; i < nHist; i++) if (!strcmp(histId[i], imdb)) return 1;
  return 0;
}

// Semeia o catalogo como a descoberta faria: titulos que vieram dos addons.
static void semear(const char *const *ids, int n) {
  int i;
  memset(cat, 0, sizeof cat);
  catN = 0;
  for (i = 0; i < n && i < FALSO_MAX; i++) {
    snprintf(cat[catN].imdb, sizeof cat[catN].imdb, "%s", ids[i]);
    snprintf(cat[catN].titulo, sizeof cat[catN].titulo, "titulo %s", ids[i]);
    snprintf(cat[catN].poster, sizeof cat[catN].poster, "https://addon/%s.jpg", ids[i]);
    catN++;
  }
}

static int naListaDe(const char *imdb) {
  int i = cat_indice_por_imdb(imdb);
  return i >= 0 ? cat[i].naLista : -1;
}

// TODA leitura de item passa por estes dois. Um teste de regressao tem de
// REPROVAR contra o codigo antigo, e ali contalib_item devolve NULL e
// cat_indice_por_imdb devolve -1 o tempo todo: deferenciar isso mata o processo
// com sinal 11, e um teste que morre nao diz qual contrato quebrou.
static const ContaLibItem ITEM_VAZIO;
static const CatItem CAT_VAZIO;

static const ContaLibItem *item(int i) {
  const ContaLibItem *c = contalib_item(i);
  return c ? c : &ITEM_VAZIO;
}
static const CatItem *catDe(const char *imdb) {
  int i = cat_indice_por_imdb(imdb);
  return i >= 0 ? &cat[i] : &CAT_VAZIO;
}

// ---------------------------------------------------------------- corpos

// Tres itens, no shape da secao 1.5. As variacoes sao de proposito:
//   - Shawshank ja esta no catalogo (a descoberta trouxe) -> so recebe a marca
//   - Breaking Bad tem poster vazio e background null     -> arte de reserva
//   - Inception tem added_at ISO e genres vazio           -> outro caminho de data
static const char *BIB =
  "[{\"content_id\":\"tt0111161\",\"content_type\":\"movie\","
    "\"name\":\"The Shawshank Redemption\",\"poster\":\"https://art/sr.jpg\","
    "\"poster_shape\":\"POSTER\",\"background\":\"https://art/sr-bg.jpg\","
    "\"description\":\"Dois homens presos.\",\"release_info\":\"1994\","
    "\"imdb_rating\":9.3,\"genres\":[\"Drama\",\"Crime\"],"
    "\"addon_base_url\":\"https://v3-cinemeta.strem.io\","
    "\"added_at\":1756000000000},"
   "{\"content_id\":\"tt0903747\",\"content_type\":\"series\","
    "\"name\":\"Breaking Bad\",\"poster\":\"\",\"poster_shape\":\"POSTER\","
    "\"background\":null,\"description\":\"\",\"release_info\":\"2008-2013\","
    "\"imdb_rating\":\"9.5\",\"genres\":[\"Drama\",\"Thriller\"],"
    "\"addon_base_url\":null,\"added_at\":1757000000000},"
   "{\"content_id\":\"tt1375666\",\"content_type\":\"movie\","
    "\"name\":\"Inception\",\"poster\":\"https://art/in.jpg\","
    "\"release_info\":\"2010\",\"imdb_rating\":8.8,\"genres\":[],"
    "\"added_at\":\"2026-09-01T10:00:00+00:00\"}]";

// Vistos: um filme, uma serie inteira e DOIS episodios. Os episodios nao podem
// virar "serie assistida" — trakt.c pula a linha com "episode" pelo mesmo
// motivo, e sem isso um episodio de oito temporadas marcaria a obra toda.
static const char *VISTOS =
  "[{\"content_id\":\"tt0111161\",\"content_type\":\"movie\","
    "\"title\":\"The Shawshank Redemption\",\"season\":null,\"episode\":null,"
    "\"watched_at\":\"2026-08-30T21:15:00+00:00\"},"
   "{\"content_id\":\"tt0903747\",\"content_type\":\"series\","
    "\"title\":\"Breaking Bad\",\"season\":1,\"episode\":1,"
    "\"watched_at\":1756100000000},"
   "{\"content_id\":\"tt0903747\",\"content_type\":\"series\","
    "\"title\":\"Breaking Bad\",\"season\":1,\"episode\":2,"
    "\"watched_at\":1756200000000},"
   "{\"content_id\":\"tt0944947\",\"content_type\":\"series\","
    "\"title\":\"Game of Thrones\",\"season\":null,\"episode\":null,"
    "\"watched_at\":1756300000000}]";

int main(void) {
  static const char *SEMENTE[3] = { "tt0111161", "tt4154796", "tt0068646" };
  static const char *OUTRA[2]   = { "tt0816692", "tt0109830" };

  printf("biblioteca da conta chega ao catalogo:\n");
  semear(SEMENTE, 3);
  confere("as tres linhas da conta sao lidas", contalib_ler_biblioteca(BIB), 3);
  confere("a conta e reconhecida como fonte", contalib_tem_conta(), 1);
  // ESTE E O CASO QUE FALHAVA. Antes, o corpo era contado e liberado; o
  // catalogo continuava com os tres titulos da descoberta e nenhuma marca.
  confere("aplicar marca e acrescenta", contalib_aplicar_catalogo(), 3);
  confere("o catalogo cresceu com os dois que faltavam", cat_n(), 5);
  confere("o que ja estava no catalogo so recebeu a marca",
          naListaDe("tt0111161"), 1);
  confere("o que faltava entrou marcado", naListaDe("tt0903747"), 1);
  confere("o terceiro tambem", naListaDe("tt1375666"), 1);
  confere("a semente que nao esta na conta continua sem marca",
          naListaDe("tt4154796"), 0);

  printf("\ncampos da linha:\n");
  confereTexto("nome vira titulo", item(0)->titulo, "Inception");
  confereTexto("tipo normalizado", item(1)->tipo, "series");
  // Ordem por added_at DESCRESCENTE: Inception (ISO de setembro/2026) na
  // frente, depois Breaking Bad e Shawshank. Se `added_at` ISO fosse lido com
  // js_num, ele viraria o ano 2026 em ms (1970) e Inception cairia para o fim.
  confereTexto("ISO em added_at ordena junto com os numericos",
               item(2)->titulo, "The Shawshank Redemption");
  confereTexto("release_info vira meta", item(2)->meta, "1994");
  confereTexto("generos compostos com o tipo",
               item(2)->genero, "Filme · Drama · Crime");
  confereTexto("genres vazio deixa so o tipo", item(0)->genero, "Filme");
  confere("imdb_rating vira porcentagem", item(2)->nota, 93);
  confere("imdb_rating como string tambem", item(1)->nota, 95);
  confereTexto("poster vazio cai no metahub medium", catDe("tt0903747")->poster,
               "https://images.metahub.space/poster/medium/tt0903747/img");
  confereTexto("background null tambem", catDe("tt0903747")->backdrop,
               "https://images.metahub.space/background/medium/tt0903747/img");
  confereTexto("poster da conta e preservado", catDe("tt1375666")->poster,
               "https://art/in.jpg");

  printf("\nlista remota vazia NAO e delecao (secao 1.6, regra 1):\n");
  confere("array vazio e recusado", contalib_ler_biblioteca("[]"), -1);
  confere("e a lista guardada sobrevive", contalib_n(), 3);
  confere("resposta que nao e array e recusada",
          contalib_ler_biblioteca("{\"code\":\"PGRST202\"}"), -1);
  confere("a lista continua de pe", contalib_n(), 3);
  confere("e a fonte continua sendo a conta", contalib_tem_conta(), 1);

  printf("\nreconciliacao depois de a descoberta trocar o catalogo:\n");
  confere("nada a refazer com o catalogo intacto", (contalib_reconciliar(), cat_n()), 5);
  confere("a marca continua onde estava", naListaDe("tt1375666"), 1);
  // cat_definir_tudo: a descoberta publica um catalogo NOVO e tudo que a conta
  // acrescentou desaparece. Sem reconciliar, a Biblioteca esvaziava sozinha uns
  // 20 s depois do arranque.
  semear(OUTRA, 2);
  confere("depois da troca, a conta sumiu do catalogo",
          cat_indice_por_imdb("tt0903747"), -1);
  contalib_reconciliar();
  confere("a reconciliacao repos os tres", cat_n(), 5);
  confere("marcados de novo", naListaDe("tt0111161"), 1);
  confere("inclusive o que so existia na conta", naListaDe("tt1375666"), 1);

  printf("\nvistos da conta alimentam o historico:\n");
  confere("as quatro linhas sao lidas", contalib_ler_vistos(VISTOS), 4);
  confere("so as de titulo inteiro marcam", contalib_aplicar_vistos(), 2);
  confere("o filme foi marcado", histTem("tt0111161"), 1);
  confere("a serie sem episodio tambem", histTem("tt0944947"), 1);
  confere("os episodios NAO marcaram a serie inteira", histTem("tt0903747"), 0);
  confere("vistos vazio tambem e recusado", contalib_ler_vistos("[]"), -1);
  confere("e os guardados sobrevivem", contalib_n_vistos(), 4);

  printf("\nsair da conta apaga tudo:\n");
  contalib_esquecer();
  confere("sem itens", contalib_n(), 0);
  confere("sem vistos", contalib_n_vistos(), 0);
  confere("e o selo volta a nao dizer CONTA", contalib_tem_conta(), 0);
  contalib_reconciliar();   // nao pode explodir nem reescrever nada
  confere("reconciliar sem conta nao mexe no catalogo", cat_n(), 5);

  printf("\n%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}
