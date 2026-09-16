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
      // A FOTO DE QUEM MANDOU E A NOTA DO TITULO vem do servidor na MESMA
      // resposta. A nota vem em centesimos (93 = 9,3), como o `nota` do
      // CatItem — se ela virar 9 ou 9.3 em algum lado, o selo desenha "0,9".
      CONFERE(strstr(r.deAvatar, "walter.trakt.tv") != NULL,
              "deAvatar: [%s]", r.deAvatar);
      CONFERE(r.nota == 93, "nota em centesimos: %d", r.nota);
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
    // AMIGO SEM FOTO E CASO NORMAL, e nao erro: conta Nuvio nao expoe avatar na
    // verificacao de identidade (ver idNuvio no servidor). O campo chega vazio
    // e quem desenha cai na inicial num disco colorido.
    CONFERE(!r.deAvatar[0], "contato de conta Nuvio vem sem foto: [%s]",
            r.deAvatar);
    CONFERE(r.nota == 95, "nota da serie: %d", r.nota);
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
    CONFERE(strstr(r.deAvatar, "walter.trakt.tv") != NULL,
            "a foto veio do disco: [%s]", r.deAvatar);
    CONFERE(r.nota == 93, "a nota veio do disco: %d", r.nota);
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
    // A NOTA SAI DO CatItem DE QUEM MANDA. E a unica origem possivel: quem
    // recebe pode nao ter o titulo no catalogo dele.
    ci.nota = 92;
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
    CONFERE(strstr(ultimoCorpoPost, "\"nota\":92") != NULL,
            "nota no corpo: [%s]", ultimoCorpoPost);
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

  // --- 12. O ARQUIVO DA VERSAO ANTERIOR CONTINUA CARREGANDO ----------------
  //
  // O cache v1 nao tem `nota` nem `deAvatar`, e as duas colunas novas entraram
  // ANTES do titulo — que e lido como "o resto da linha". Sem o desvio por
  // contagem de TAB, a linha v1 seria lida com o titulo no lugar da nota e a
  // aba Social abriria com quatro linhas sem nome nenhum na TV de quem
  // atualizar o app. Esta e a linha EXATA que a versao instalada hoje grava.
  { char caminho[600];
    FILE *f;
    RecItem r;
    dados_caminho(caminho, sizeof caminho, "recomendacoes.txt");
    f = fopen(caminho, "wb");
    CONFERE(f != NULL, "abrir %s para escrever a v1", caminho);
    if (f) {
      fprintf(f, "# nuvio recomendacoes v1\n");
      fprintf(f, "7\t1757900000\t0\t2\tmovie\t1994\ttrakt:gustavo\tGustavo\t"
                 "tt0111161\thttps://exemplo/p.jpg\t\tUm Sonho de Liberdade\n");
      fprintf(f, "6\t1757890000\t1\t-1\tseries\t2008\tnuvio:9a1c\tMarina\t"
                 "tt0903747\thttps://exemplo/q.jpg\tolha isso\tBreaking Bad\n");
      fclose(f);
    }
    simularArranque();
    recomenda_iniciar();
    CONFERE(recomenda_n() == 2, "as duas linhas v1 carregaram, nao %d",
            recomenda_n());
    recomenda_item(0, &r);
    CONFERE(!strcmp(r.titulo, "Um Sonho de Liberdade"),
            "o titulo v1 nao virou nota: [%s]", r.titulo);
    CONFERE(r.nota == 0, "linha v1 nao tem nota: %d", r.nota);
    CONFERE(!r.deAvatar[0], "linha v1 nao tem foto: [%s]", r.deAvatar);
    recomenda_item(1, &r);
    CONFERE(!strcmp(r.titulo, "Breaking Bad"), "segundo titulo v1: [%s]", r.titulo);
    CONFERE(!strcmp(r.texto, "olha isso"), "texto livre v1: [%s]", r.texto);

    // E A PRIMEIRA GRAVACAO SOBE O ARQUIVO PARA v2, sem perder o que a v1
    // tinha. marcar_vistas grava; conferimos a forma da linha, nao so o texto.
    recomenda_marcar_vistas();
    { char *conteudo = lerArquivo(caminho);
      int tabs = 0;
      const char *linha2 = NULL, *q;
      CONFERE(conteudo != NULL, "reler %s", caminho);
      if (conteudo) {
        CONFERE(strstr(conteudo, "# nuvio recomendacoes v2") == conteudo,
                "o cabecalho subiu para v2");
        linha2 = strchr(conteudo, '\n');
        linha2 = linha2 ? linha2 + 1 : NULL;
        for (q = linha2; q && *q && *q != '\n'; q++) if (*q == '\t') tabs++;
        CONFERE(tabs == 13, "a linha v2 tem 13 TABs, nao %d", tabs);
        CONFERE(strstr(conteudo, "Um Sonho de Liberdade") != NULL,
                "o titulo sobreviveu a subida de versao");
        free(conteudo);
      } }
    simularArranque();
    recomenda_iniciar();
    CONFERE(recomenda_n() == 2, "a v2 recem-gravada recarrega: %d", recomenda_n());
    recomenda_item(0, &r);
    CONFERE(!strcmp(r.titulo, "Um Sonho de Liberdade"),
            "titulo depois da subida: [%s]", r.titulo); }

  // --- APARECER PARA OUTRAS PESSOAS ----------------------------------------
  //
  // O QUE ESTE BLOCO PROVA, e nada disso e visivel na captura: que o padrao e
  // "nao perguntado" e nao "sim"; que a resposta e gravada ANTES de qualquer
  // rede e sobrevive ao arranque; que sair da conta a apaga; e que `descobrivel
  // = 1` vindo do servidor so e adotado por um aparelho que NUNCA perguntou.
  //
  // O terceiro e o unico caminho pelo qual `aparecer` vira SIM sem alguem
  // apertar OK nesta TV, e ele existe para a segunda TV da mesma pessoa. Se ele
  // passasse a valer tambem para quem ja respondeu NAO, o "nao" duraria ate o
  // proximo arranque — e um consentimento que se desfaz sozinho e o defeito que
  // este bloco esta aqui para impedir.
  { char caminho[600];
    char *conteudo;
    dados_apagar("recomendacoes-aparecer.txt");
    aparecer = REC_APARECER_NAO_PERGUNTADO;
    aparecerPendente = -1;
    simularArranque();
    recomenda_iniciar();
    CONFERE(recomenda_aparecer() == REC_APARECER_NAO_PERGUNTADO,
            "sem arquivo, o padrao e nao perguntado: %d", recomenda_aparecer());

    recomenda_responder_aparecer(0);
    CONFERE(recomenda_aparecer() == REC_APARECER_NAO,
            "responder nao grava NAO: %d", recomenda_aparecer());
    CONFERE(aparecerPendente == 0, "o aviso ao servidor ficou pendente: %d",
            aparecerPendente);
    dados_caminho(caminho, sizeof caminho, "recomendacoes-aparecer.txt");
    conteudo = lerArquivo(caminho);
    CONFERE(conteudo != NULL, "o arquivo da resposta existe: %s", caminho);
    if (conteudo) {
      CONFERE(atoi(conteudo) == REC_APARECER_NAO,
              "o disco guarda a resposta: [%s]", conteudo);
      free(conteudo);
    }

    // SOBREVIVE AO ARRANQUE. Sem isto a pergunta voltaria toda vez que a TV
    // liga, que e como um consentimento vira um obstaculo a ser clicado.
    aparecer = REC_APARECER_SIM;        // lixo, para provar que a leitura manda
    simularArranque();
    recomenda_iniciar();
    CONFERE(recomenda_aparecer() == REC_APARECER_NAO,
            "a resposta volta do disco: %d", recomenda_aparecer());

    // UM SERVIDOR DIZENDO 1 NAO DESFAZ UM "NAO" DADO AQUI.
    aparecerPendente = -1;
    { int desc = 1;
      if (aparecer == REC_APARECER_NAO_PERGUNTADO) {
        if (desc) { aparecer = REC_APARECER_SIM; gravarAparecer(); }
      } else if (desc != (aparecer == REC_APARECER_SIM)) {
        aparecerPendente = (aparecer == REC_APARECER_SIM) ? 1 : 0;
      } }
    CONFERE(recomenda_aparecer() == REC_APARECER_NAO,
            "servidor em 1 nao vira o NAO desta TV: %d", recomenda_aparecer());
    CONFERE(aparecerPendente == 0,
            "e o proximo ciclo corrige o servidor: %d", aparecerPendente);

    // MAS UM APARELHO QUE NUNCA PERGUNTOU ADOTA O "SIM" de outra TV da mesma
    // pessoa — senao a pergunta apareceria uma vez por aparelho.
    dados_apagar("recomendacoes-aparecer.txt");
    aparecer = REC_APARECER_NAO_PERGUNTADO;
    aparecerPendente = -1;
    { int desc = 1;
      if (aparecer == REC_APARECER_NAO_PERGUNTADO) {
        if (desc) { aparecer = REC_APARECER_SIM; gravarAparecer(); }
      } }
    CONFERE(recomenda_aparecer() == REC_APARECER_SIM,
            "quem nunca perguntou adota o sim do servidor: %d",
            recomenda_aparecer());

    // SAIR DA CONTA APAGA A RESPOSTA. Quem entrar depois nao respondeu nada.
    recomenda_esquecer();
    CONFERE(recomenda_aparecer() == REC_APARECER_NAO_PERGUNTADO,
            "logout devolve a pergunta: %d", recomenda_aparecer());
    conteudo = lerArquivo(caminho);
    CONFERE(conteudo == NULL, "e apaga o arquivo da resposta");
    free(conteudo); }

  // --- SUGESTOES ------------------------------------------------------------
  //
  // A frase de origem e o unico texto da linha que explica por que um nome
  // desconhecido esta na tela. Os tres casos tem de sair diferentes — e o
  // terceiro (amigo sem nome do intermediario) e o que uma montagem ingenua
  // deixaria em "Amigo de ".
  { RecSugestao s;
    char frase[128];
    memset(&s, 0, sizeof s);
    snprintf(s.origem, sizeof s.origem, "%s", "trakt");
    rec_sugestao_origem(frase, sizeof frase, &s);
    CONFERE(strstr(frase, "Trakt") != NULL, "origem trakt: [%s]", frase);

    snprintf(s.origem,  sizeof s.origem,  "%s", "amigo");
    snprintf(s.viaNome, sizeof s.viaNome, "%s", "Gustavo");
    rec_sugestao_origem(frase, sizeof frase, &s);
    CONFERE(strstr(frase, "Gustavo") != NULL,
            "origem amigo cita quem faz a ponte: [%s]", frase);

    s.viaNome[0] = 0;
    rec_sugestao_origem(frase, sizeof frase, &s);
    CONFERE(frase[0] && !strchr(frase, ':') && strlen(frase) > 8,
            "amigo sem nome do intermediario ainda diz algo: [%s]", frase);

    // A LISTA SOME COM O LOGOUT, como a de recomendacoes e pela mesma razao:
    // uma sugestao e "gente que VOCE talvez conheca", e ela na tela de quem
    // acabou de entrar seria o pior vazamento possivel deste recurso.
    nSugestoes = 2;
    memset(sugestoes, 0, sizeof sugestoes);
    snprintf(sugestoes[0].id, sizeof sugestoes[0].id, "%s", "trakt:um");
    snprintf(sugestoes[1].id, sizeof sugestoes[1].id, "%s", "nuvio:dois");
    CONFERE(recomenda_n_sugestoes() == 2, "duas sugestoes semeadas: %d",
            recomenda_n_sugestoes());
    CONFERE(recomenda_sugestao(0, &s) && !strcmp(s.id, "trakt:um"),
            "copia a sugestao 0: [%s]", s.id);
    CONFERE(!recomenda_sugestao(2, &s), "indice fora da faixa recusa");
    // Aceitar uma sugestao a tira da lista NA HORA, sem esperar a rede.
    CONFERE(recomenda_adicionar_sugerido("trakt:um"), "enfileira o vinculo");
    CONFERE(recomenda_n_sugestoes() == 1,
            "a aceita sai da lista na hora: %d", recomenda_n_sugestoes());
    recomenda_esquecer();
    CONFERE(recomenda_n_sugestoes() == 0, "logout limpa as sugestoes: %d",
            recomenda_n_sugestoes()); }

  free(fixture);
  printf(falhas ? "recomenda: %d falhas\n" : "recomenda: ok\n", falhas);
  return falhas ? 1 : 0;
#endif
}
