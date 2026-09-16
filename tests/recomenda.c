// Recomendacoes: leitura do JSON do servico, o caminho do 304, o cursor
// avancando, o cache de disco sobrevivendo a um arranque e a contagem do selo.
//
//   bash tests/recomenda.sh
//
// Inclui o .c de proposito, como tests/atualizacao.c: as funcoes de rede e de
// parse sao estaticas, e expo-las so para o teste seria API a mais. O .sh
// compila tudo MENOS src/recomenda.c.
//
// A REDE E INTERCEPTADA POR #define, nao por servidor de mentira. Os tres
// nomes de rede.h viram outros ANTES do include, entao recomenda.c chama os
// dublês daqui e o rede.c de verdade continua linkado sem conflito. E o que
// permite exercitar o 304 — que e o caso COMUM em producao e o mais facil de
// quebrar sem ninguem notar, porque quebrado ele so custa banda.
//
// ONDE ELE ESCREVE: exclusivamente em NUVIO_DADOS, e ele CONFERE isso antes de
// qualquer escrita. Sem essa conferencia um teste de cache sobrescreve o
// ~/.nuvio de quem o roda — as recomendacoes, a lista de salvos e a sessao.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef REC_TESTE_SEM_URL
#define NV_REC_URL ""
#else
#define NV_REC_URL "http://127.0.0.1:8799"
#endif

// --- interceptacao da rede ---------------------------------------------------
#define rede_baixar_etag  teste_rede_etag
#define rede_postar_st    teste_rede_postar
#define rede_baixar_st    teste_rede_st
#define rede_baixar_com   teste_rede_com

#include "../src/recomenda.c"

static const char *respCorpo;     // corpo que o proximo GET devolve
static int   respStatus = 200;
static char  respEtag[96];
static char  ultimaUrl[600];
static char  ultimoIfNone[200];
static char  ultimoCorpoPost[2400];
static int   nGet, nPost;

static char *dup(const char *s) {
  char *r;
  if (!s) return NULL;
  r = (char *)malloc(strlen(s) + 1);
  if (r) strcpy(r, s);
  return r;
}

char *teste_rede_etag(const char *url, int seg, const char *const *cab,
                      int *status, char *etag, unsigned tamEtag) {
  int k;
  (void)seg;
  nGet++;
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url);
  ultimoIfNone[0] = 0;
  for (k = 0; cab && cab[k]; k++)
    if (!strncmp(cab[k], "If-None-Match:", 14))
      snprintf(ultimoIfNone, sizeof ultimoIfNone, "%s", cab[k] + 15);
  if (status) *status = respStatus;
  if (etag && tamEtag) snprintf(etag, tamEtag, "%s", respEtag);
  if (respStatus == 304) return NULL;
  return dup(respCorpo);
}

char *teste_rede_postar(const char *url, int seg, const char *const *cab,
                        const char *corpo, int *status) {
  (void)seg; (void)cab;
  nPost++;
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url);
  snprintf(ultimoCorpoPost, sizeof ultimoCorpoPost, "%s", corpo ? corpo : "");
  if (status) *status = respStatus;
  return dup(respCorpo);
}

char *teste_rede_st(const char *url, int seg, const char *const *cab, int *status) {
  (void)seg; (void)cab;
  nGet++;
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url);
  if (status) *status = respStatus;
  return dup(respCorpo);
}

char *teste_rede_com(const char *url, int seg, const char *const *cab) {
  (void)seg; (void)cab;
  nGet++;
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url);
  return dup(respCorpo);
}

// --- conferencias ------------------------------------------------------------
static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHA: " __VA_ARGS__); \
                                         printf("\n"); } } while (0)

#ifndef REC_TESTE_SEM_URL
static char *lerArquivo(const char *nome) {
  FILE *f = fopen(nome, "rb");
  long n;
  char *s;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  s = (char *)malloc((size_t)n + 1);
  if (!s) { fclose(f); return NULL; }
  if (fread(s, 1, (size_t)n, f) != (size_t)n) { free(s); fclose(f); return NULL; }
  s[n] = 0;
  fclose(f);
  return s;
}

// Zera o modulo como se o processo tivesse acabado de nascer, sem tocar no
// disco: e assim que se testa "o cache sobrevive ao arranque" sem precisar de
// um segundo processo.
static void simularArranque(void) {
  nItens = 0;
  nContatos = 0;
  cursor = 0;
  etagRec[0] = 0;
  registrado = 0;
  meuId[0] = 0;
}
#endif

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
#ifndef REC_TESTE_SEM_URL
  const char *cab[3];
  char *fixture;
#endif

  // A TRAVA. Sem ela este teste escreve recomendacoes.txt dentro do ~/.nuvio de
  // quem o executa — e "o teste apagou meus dados" e um defeito que so aparece
  // depois de ja ter apagado.
  if (!dir || !dir[0]) {
    printf("recomenda: NUVIO_DADOS nao esta no ambiente; recusando rodar\n");
    return 2;
  }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) {
    printf("recomenda: dados_dir() e \"%s\" e NUVIO_DADOS e \"%s\"; recusando rodar\n",
           dados_dir(), dir);
    return 2;
  }

#ifdef REC_TESTE_SEM_URL
  // PACOTE SEM NUVIO_REC_URL: a funcao inteira tem de ser invisivel, e
  // "invisivel" aqui quer dizer que NENHUMA chamada de rede acontece.
  CONFERE(!recomenda_ativo(), "sem URL, recomenda_ativo deve ser 0");
  recomenda_iniciar();
  recomenda_verificar();
  recomenda_marcar_vistas();
  CONFERE(recomenda_n() == 0, "sem URL, a lista e sempre vazia");
  CONFERE(recomenda_n_novas() == 0, "sem URL, o selo e sempre 0");
  CONFERE(nGet == 0 && nPost == 0, "sem URL nao pode sair requisicao (%d GET, %d POST)",
          nGet, nPost);
  CONFERE(!recomenda_enviar(NULL, "trakt:x", 0, ""), "sem URL, enviar recusa");
  printf(falhas ? "recomenda (sem URL): %d falhas\n" : "recomenda (sem URL): ok\n",
         falhas);
  return falhas ? 1 : 0;
#else
  CONFERE(recomenda_ativo(), "com URL, recomenda_ativo deve ser 1");
  if (!mtx) mtx = SDL_CreateMutex();

  fixture = lerArquivo("tests/recomenda_rec.json");
  CONFERE(fixture != NULL, "fixture tests/recomenda_rec.json");
  if (!fixture) return 1;

  cab[0] = "Authorization: Bearer tok-teste";
  cab[1] = "X-Nuvio-Auth: nuvio";
  cab[2] = NULL;

  // --- 1. um item do JSON de verdade ---------------------------------------
  { const char *p = js_array(fixture, NULL, "itens");
    RecItem r;
    CONFERE(p != NULL, "o array itens existe");
    if (p) {
      lerItem(p, js_fim(p), &r);
      CONFERE(r.id == 3, "id do primeiro item: %lld", r.id);
      CONFERE(!strcmp(r.imdb, "tt0111161"), "imdb: [%s]", r.imdb);
      CONFERE(!strcmp(r.tipo, "movie"), "tipo: [%s]", r.tipo);
      CONFERE(!strcmp(r.deNome, "Gustavo Lima"), "deNome: [%s]", r.deNome);
      CONFERE(!strcmp(r.titulo, "Um Sonho de Liberdade"), "titulo: [%s]", r.titulo);
      CONFERE(!strcmp(r.ano, "1994"), "ano: [%s]", r.ano);
      CONFERE(r.modelo == 2, "modelo: %d", r.modelo);
      CONFERE(r.criado == 1757900000LL, "criado: %lld", r.criado);
      CONFERE(r.visto == 0, "nao vista");
      CONFERE(strstr(r.poster, "tt0111161") != NULL, "poster: [%s]", r.poster);
    } }

  // --- 2. a resposta inteira, com o cursor andando -------------------------
  respCorpo = fixture;
  respStatus = 200;
  snprintf(respEtag, sizeof respEtag, "\"9-3-2\"");
  nGet = 0;
  CONFERE(lerRecs(cab) == 1, "lerRecs fala com o servidor");
  CONFERE(nGet == 1, "uma requisicao, nao %d", nGet);
  CONFERE(strstr(ultimaUrl, "/v1/rec?desde=0") != NULL,
          "a primeira consulta parte do cursor 0: [%s]", ultimaUrl);
  CONFERE(!ultimoIfNone[0], "sem ETag guardado nao vai If-None-Match: [%s]",
          ultimoIfNone);
  CONFERE(recomenda_n() == 3, "tres recomendacoes, nao %d", recomenda_n());
  CONFERE(cursor == 3, "cursor avancou para 3, nao %lld", cursor);
  CONFERE(!strcmp(etagRec, "\"9-3-2\""), "ETag guardado: [%s]", etagRec);

  // --- 3. o selo ------------------------------------------------------------
  CONFERE(recomenda_n_novas() == 2, "duas nao vistas, nao %d", recomenda_n_novas());

  // --- 4. a ordem: mais nova primeiro --------------------------------------
  { RecItem r;
    CONFERE(recomenda_item(0, &r) && r.id == 3, "a primeira linha e a mais nova");
    CONFERE(recomenda_item(2, &r) && r.id == 1, "a ultima linha e a mais velha");
    CONFERE(!recomenda_item(3, &r), "fora da faixa devolve 0"); }

  // --- 5. o texto livre chega inteiro --------------------------------------
  { RecItem r;
    recomenda_item(1, &r);
    CONFERE(r.modelo == -1, "modelo -1 e texto livre, nao %d", r.modelo);
    CONFERE(!strcmp(rec_frase(&r), "olha isso hoje a noite"),
            "frase do texto livre: [%s]", rec_frase(&r)); }
  { RecItem r;
    recomenda_item(0, &r);
    // Compara com i18n(MODELOS[2]), nao com MODELOS[2] cru: desde que as
    // chaves dos modelos entraram em idioma_tab.h, rec_frase devolve a frase
    // TRADUZIDA quando a interface esta em ingles, e era isso que se queria.
    // O teste checa que e a frase do modelo 2, em qualquer idioma.
    CONFERE(!strcmp(rec_frase(&r), i18n(MODELOS[2])),
            "frase do modelo 2: [%s]", rec_frase(&r)); }

  // --- 6. O 304 -------------------------------------------------------------
  //
  // O caso COMUM em producao: uma vez por minuto, por TV, sem nada novo. Ele
  // tem de mandar o ETag guardado, receber 304 e NAO mexer em nada.
  respStatus = 304;
  respCorpo = NULL;
  nGet = 0;
  CONFERE(lerRecs(cab) == 1, "304 e resposta valida, nao falha");
  CONFERE(nGet == 1, "o 304 tambem custa uma requisicao");
  CONFERE(strstr(ultimaUrl, "/v1/rec?desde=3") != NULL,
          "a segunda consulta parte do cursor 3: [%s]", ultimaUrl);
  CONFERE(!strcmp(ultimoIfNone, "\"9-3-2\""),
          "If-None-Match com o ETag guardado: [%s]", ultimoIfNone);
  CONFERE(recomenda_n() == 3, "o 304 nao mexe na lista (%d)", recomenda_n());
  CONFERE(cursor == 3, "o 304 nao mexe no cursor (%lld)", cursor);
  CONFERE(!strcmp(etagRec, "\"9-3-2\""), "o 304 preserva o ETag: [%s]", etagRec);

  // --- 7. o cache sobrevive ao arranque ------------------------------------
  simularArranque();
  CONFERE(recomenda_n() == 0, "o arranque simulado esvaziou a memoria");
  recomenda_iniciar();
  CONFERE(recomenda_n() == 3, "o disco devolveu as tres, nao %d", recomenda_n());
  CONFERE(cursor == 3, "o disco devolveu o cursor: %lld", cursor);
  CONFERE(!strcmp(etagRec, "\"9-3-2\""), "o disco devolveu o ETag: [%s]", etagRec);
  CONFERE(recomenda_n_novas() == 2, "o selo sobreviveu: %d", recomenda_n_novas());
  { RecItem r;
    recomenda_item(0, &r);
    // TITULO E POSTER E QUEM MANDOU, e nao so o id: e o que faz a aba desenhar
    // no primeiro quadro, antes de a rede responder.
    CONFERE(!strcmp(r.titulo, "Um Sonho de Liberdade"),
            "titulo veio do disco: [%s]", r.titulo);
    CONFERE(!strcmp(r.deNome, "Gustavo Lima"), "quem mandou veio do disco: [%s]",
            r.deNome);
    CONFERE(strstr(r.poster, "tt0111161") != NULL, "poster veio do disco: [%s]",
            r.poster);
    recomenda_item(1, &r);
    CONFERE(!strcmp(r.texto, "olha isso hoje a noite"),
            "texto livre veio do disco: [%s]", r.texto); }

  // --- 8. marcar como vistas apaga o selo, e isso tambem sobrevive ---------
  recomenda_marcar_vistas();
  CONFERE(recomenda_n_novas() == 0, "o selo zerou: %d", recomenda_n_novas());
  CONFERE(nVistoFila == 2, "duas a confirmar no servidor, nao %d", nVistoFila);
  simularArranque();
  recomenda_iniciar();
  CONFERE(recomenda_n_novas() == 0, "o selo continua zerado depois do arranque");

  // --- 9. envio: o corpo que sai e o que o servidor espera -----------------
  { CatItem ci;
    memset(&ci, 0, sizeof ci);
    snprintf(ci.imdb, sizeof ci.imdb, "%s", "tt0068646");
    snprintf(ci.tipo, sizeof ci.tipo, "%s", "movie");
    snprintf(ci.titulo, sizeof ci.titulo, "%s", "O Poderoso Chefão");
    snprintf(ci.poster, sizeof ci.poster, "%s", "https://exemplo/p.jpg");
    // O CatItem nao tem campo de ano: ele guarda "1972 · 2h55" em `meta`.
    snprintf(ci.meta, sizeof ci.meta, "%s", "1972 · 2h55");
    respStatus = 200;
    respCorpo = "{\"ok\":1,\"id\":9}";
    envioEstado = REC_ENVIO_NADA;
    fila.cheia = 0;
    CONFERE(recomenda_enviar(&ci, "trakt:gustavo", 4, ""), "o envio entra na fila");
    CONFERE(fila.cheia == 1, "a fila ficou cheia");
    CONFERE(!strcmp(fila.ano, "1972"), "o ano saiu do meta: [%s]", fila.ano);
    nPost = 0;
    enviarFila(cab);
    CONFERE(nPost == 1, "um POST, nao %d", nPost);
    CONFERE(strstr(ultimaUrl, "/v1/rec") != NULL, "rota do envio: [%s]", ultimaUrl);
    CONFERE(strstr(ultimoCorpoPost, "\"para\":\"trakt:gustavo\"") != NULL,
            "para: [%s]", ultimoCorpoPost);
    CONFERE(strstr(ultimoCorpoPost, "\"imdb\":\"tt0068646\"") != NULL, "imdb");
    CONFERE(strstr(ultimoCorpoPost, "\"modelo\":4") != NULL, "modelo");
    CONFERE(strstr(ultimoCorpoPost, "\"ano\":\"1972\"") != NULL, "ano");
    CONFERE(recomenda_envio_estado() == REC_ENVIO_OK, "estado do envio: %d",
            recomenda_envio_estado());
    // Modelo fora da faixa nao vira requisicao: o servidor guardaria um indice
    // que nenhuma TV sabe desenhar.
    CONFERE(!recomenda_enviar(&ci, "trakt:gustavo", 99, ""), "modelo 99 recusado");
    CONFERE(!recomenda_enviar(&ci, "", 0, ""), "sem destinatario, recusado"); }

  // --- 10. o JSON do envio escapa aspas ------------------------------------
  { char esc[80];
    jsonEsc(esc, sizeof esc, "as\"pas\\e barra");
    CONFERE(!strcmp(esc, "as\\\"pas\\\\e barra"), "escape: [%s]", esc); }

  // --- 11. esquecer apaga o aparelho ---------------------------------------
  recomenda_esquecer();
  CONFERE(recomenda_n() == 0, "esquecer zerou a lista");
  CONFERE(cursor == 0, "esquecer zerou o cursor");
  { char caminho[600];
    dados_caminho(caminho, sizeof caminho, "recomendacoes.txt");
    CONFERE(fopen(caminho, "rb") == NULL, "recomendacoes.txt foi apagado"); }

  free(fixture);
  printf(falhas ? "recomenda: %d falhas\n" : "recomenda: ok\n", falhas);
  return falhas ? 1 : 0;
#endif
}
