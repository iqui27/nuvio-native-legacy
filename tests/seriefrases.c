// Frases (Wikiquote) e ficha de producao (Wikidata): leitura da resposta
// SPARQL, limpeza de wikitexto, extracao das falas nos DOIS formatos que o
// Wikiquote usa, e o cache de disco — inclusive o CACHE NEGATIVO, que e o que
// impede um titulo sem nada de pagar dois pedidos por visita para sempre.
//
//   bash tests/seriefrases.sh
//
// Inclui o .c, como tests/recomenda.c: o parse e o fio sao estaticos. O .sh
// compila tudo MENOS src/seriefrases.c.
//
// A RESPOSTA DO WIKIDATA E REAL, copiada de query.wikidata.org em 16/09/2026.
// O WIKITEXTO do fixture reproduz as FORMAS DE MARCACAO observadas nas paginas
// reais (cabecalho `== Personagem ==` com falas em `*`, do The Dark Knight
// (film); dialogo em `: '''Quem''': fala`, do Severance (TV series); a linha de
// creditos em italico; a linha de [[File:…]]) — e o que o parser tem de
// aguentar. O TEXTO das falas e curto e proprio de proposito: o que esta sob
// teste e a marcacao, e copiar paginas inteiras do Wikiquote para dentro do
// repositorio nao e necessario para isso.
//
// ONDE ELE ESCREVE: exclusivamente em NUVIO_DADOS, e CONFERE antes de escrever.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define rede_baixar_com  teste_rede_com

#include "../src/seriefrases.c"

// --- DUBLE DA REDE -----------------------------------------------------------

static int  nPedidos, nSparql, nWiki;
static const char *respSparql, *respWiki;

static char *dup(const char *s) {
  char *r;
  if (!s) return NULL;
  r = (char *)malloc(strlen(s) + 1);
  if (r) strcpy(r, s);
  return r;
}

char *teste_rede_com(const char *url, int seg, const char *const *cab) {
  (void)seg;
  nPedidos++;
  // O cabecalho de identificacao nao e opcional na Wikimedia; um pedido sem ele
  // volta 403 e o sintoma na tela seria "este titulo nao tem nada".
  if (!cab || !cab[0] || !strstr(cab[0], "User-Agent:")) {
    printf("FALHA: pedido sem User-Agent\n");
    return NULL;
  }
  if (strstr(url, "query.wikidata.org")) { nSparql++; return dup(respSparql); }
  if (strstr(url, "wikiquote.org"))      { nWiki++;   return dup(respWiki); }
  return NULL;
}

// --- CONFERENCIAS ------------------------------------------------------------

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHA: " __VA_ARGS__); \
                                         printf("\n"); } } while (0)

static void esperar(void) {
  int k;
  for (k = 0; k < 2000 && seriefrases_carregando(); k++) {
    struct timespec t = { 0, 2000000 };
    nanosleep(&t, NULL);
  }
  CONFERE(!seriefrases_carregando(), "o fio nao terminou em 4 s");
}

static void simularArranque(void) {
  nFrases = nFatos = selecionado = 0;
  imdbAtual[0] = 0;
  imdbPedido[0] = 0;
  paginaWq[0] = 0;
  emPortugues = 0;
  abandonar = 0;
}

// --- FIXTURES ----------------------------------------------------------------

// Resposta REAL de query.wikidata.org para tt0903747 (Breaking Bad), 16/09/2026,
// COPIADA BYTE A BYTE — com o embelezamento e tudo. O espaco em `"qe" : {` NAO
// e detalhe: a primeira versao do leitor procurava `"qe":{` cru, passava com um
// fixture compacto feito a mao e devolvia ZERO contra o servidor de verdade,
// em todo titulo. Fixture inventado nao pega esse tipo de defeito.
//
// Repare tambem nos varios `"value"` do documento: e por isso que sparqlCampo
// precisa recortar o objeto da variavel antes de procurar.
static const char *SPARQL_BB =
  "{\n  \"head\" : {\n    \"vars\" : [ \"qe\", \"qp\", \"o\", \"ou\", \"b\","
  " \"bu\", \"n\", \"d\", \"loc\", \"pr\", \"em\", \"ba\" ]\n  },\n"
  "  \"results\" : {\n    \"bindings\" : [ {\n"
  "      \"qe\" : {\n        \"type\" : \"uri\",\n"
  "        \"value\" : \"https://en.wikiquote.org/wiki/Breaking_Bad\"\n      },\n"
  "      \"n\" : {\n        \"datatype\" : \"http://www.w3.org/2001/XMLSchema#decimal\",\n"
  "        \"type\" : \"literal\",\n        \"value\" : \"62\"\n      },\n"
  "      \"loc\" : {\n        \"type\" : \"literal\",\n"
  "        \"value\" : \"Albuquerque\"\n      },\n"
  "      \"pr\" : {\n        \"type\" : \"literal\",\n"
  "        \"value\" : \"Pr\xc3\xa9mio Peabody \xc2\xb7 Golden Globe Award de melhor s\xc3\xa9rie dram\xc3\xa1tica\"\n      },\n"
  "      \"em\" : {\n        \"xml:lang\" : \"pt\",\n        \"type\" : \"literal\",\n"
  "        \"value\" : \"AMC\"\n      }\n    } ]\n  }\n}";

// O mesmo formato, com uma moeda DESCONHECIDA no orcamento e dolar na
// bilheteria: prova que o fato sem unidade reconhecida e OMITIDO em vez de sair
// como um numero de nove digitos sem moeda.
static const char *SPARQL_MOEDA =
  "{\n  \"results\" : {\n    \"bindings\" : [ {\n"
  "      \"o\" : { \"type\" : \"literal\", \"value\" : \"185000000\" },\n"
  "      \"ou\" : { \"type\" : \"uri\", \"value\" : \"http://www.wikidata.org/entity/Q999999\" },\n"
  "      \"b\" : { \"type\" : \"literal\", \"value\" : \"1006234167\" },\n"
  "      \"bu\" : { \"type\" : \"uri\", \"value\" : \"http://www.wikidata.org/entity/Q4917\" }\n"
  "    } ]\n  }\n}";

// Titulo sem item no Wikidata: uma linha de resultado com TODAS as variaveis
// sem valor. E o caso da maioria das series (ver a medida em seriefrases.h).
static const char *SPARQL_VAZIO =
  "{\n  \"head\" : { \"vars\" : [ \"qe\" ] },\n"
  "  \"results\" : { \"bindings\" : [ { } ] }\n}";

// Wikitexto: as formas de marcacao das paginas reais, com falas curtas.
// Repare no que TEM de ser recusado: a linha de creditos em italico, a linha de
// arquivo, e a secao "Taglines".
static const char *WIKI =
  "{\"parse\":{\"title\":\"Fixture\",\"wikitext\":{\"*\":\""
  "{{italic title}}\\n"
  "[[File:Alguma imagem.jpg|thumb|upright|uma legenda de imagem bem comprida]]\\n"
  ": ''Directed by [[Alguem]]. Written by [[Alguem]] and [[Outro]].''\\n"
  "\\n== Comissario Gordon ==\\n"
  "* A cidade precisa de alguem em quem confiar, e nao de mais um vigilante.\\n"
  "* Ele aguenta, porque nao e o heroi de que precisamos <ref>nota</ref> agora.\\n"
  "\\n== Dialogo ==\\n"
  ": '''Detetive''': [olhando pela janela] Voce sabe o que vai acontecer aqui.\\n"
  ": '''Gordon''': {{small|Sei}} Sei, e mesmo assim vou ficar ate o fim disso.\\n"
  "\\n== Taglines ==\\n"
  "* Uma frase de cartaz que nao e fala de personagem nenhum, e sim publicidade.\\n"
  "\"}}}";

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");

  if (!dir || !dir[0]) {
    printf("seriefrases: NUVIO_DADOS nao esta no ambiente; recusando rodar\n");
    return 2;
  }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) {
    printf("seriefrases: dados_dir() e \"%s\" e NUVIO_DADOS e \"%s\"; recusando\n",
           dados_dir(), dir);
    return 2;
  }

  // --- 1. sparqlCampo PEGA A VARIAVEL CERTA ---------------------------------
  // O documento tem "value" em toda variavel. Um js_texto cru devolveria o
  // primeiro deles para qualquer pergunta — e a bilheteria apareceria como
  // "local de filmagem" sem ninguem notar.
  { char v[300];
    CONFERE(sparqlCampo(SPARQL_BB, "qe", v, sizeof v) &&
            !strcmp(v, "https://en.wikiquote.org/wiki/Breaking_Bad"),
            "qe saiu \"%s\"", v);
    CONFERE(sparqlCampo(SPARQL_BB, "em", v, sizeof v) && !strcmp(v, "AMC"),
            "em saiu \"%s\"", v);
    CONFERE(sparqlCampo(SPARQL_BB, "n", v, sizeof v) && !strcmp(v, "62"),
            "n saiu \"%s\"", v);
    CONFERE(!sparqlCampo(SPARQL_BB, "o", v, sizeof v),
            "orcamento ausente virou \"%s\"", v);
    CONFERE(!sparqlCampo(SPARQL_VAZIO, "qe", v, sizeof v),
            "resultado vazio devolveu valor"); }

  // --- 2. LIMPEZA DE WIKITEXTO ----------------------------------------------
  { char s[SF_TEXTO];
    snprintf(s, sizeof s, "%s", "A [[w:Gotham|cidade]] precisa de [[Batman]].");
    limparWiki(s, sizeof s);
    CONFERE(!strcmp(s, "A cidade precisa de Batman."), "ligacoes: \"%s\"", s);
    snprintf(s, sizeof s, "%s", "'''Muito''' ''bem'' {{small|dito}} agora<ref>x</ref>.");
    limparWiki(s, sizeof s);
    CONFERE(!strcmp(s, "Muito bem agora."), "marcacao: \"%s\"", s);
    snprintf(s, sizeof s, "%s", "[olhando pela janela] Voce sabe.");
    limparWiki(s, sizeof s);
    CONFERE(!strcmp(s, "Voce sabe."), "rubrica: \"%s\"", s); }

  // --- 3. EXTRACAO DAS FALAS, NOS DOIS FORMATOS -----------------------------
  { SfFrase c[32];
    int n, i, temGordonSecao = 0, temDetetive = 0;
    // Passa pelo MESMO desescape do fio, e nao por uma copia dele no teste.
    char *corpo = dup(WIKI);
    char *w = desescapar(corpo);
    CONFERE(w != NULL, "desescapar nao achou o wikitexto");
    n = w ? extrairFalas(w, c, 32) : 0;
    for (i = 0; i < n; i++) {
      if (!strcmp(c[i].quem, "Comissario Gordon")) temGordonSecao = 1;
      if (!strcmp(c[i].quem, "Detetive")) temDetetive = 1;
      // NADA do que foi recusado pode ter passado.
      CONFERE(!strstr(c[i].texto, "Directed by"), "credito virou fala: %s", c[i].texto);
      CONFERE(!strstr(c[i].texto, "thumb"), "imagem virou fala: %s", c[i].texto);
      CONFERE(!strstr(c[i].texto, "cartaz"), "tagline virou fala: %s", c[i].texto);
      CONFERE(!strstr(c[i].texto, "[["), "sobrou marcacao: %s", c[i].texto);
      CONFERE(!strstr(c[i].texto, "{{"), "sobrou modelo: %s", c[i].texto);
      CONFERE(!strchr(c[i].texto, '\''), "sobrou aspa de marcacao: %s", c[i].texto);
    }
    CONFERE(n == 4, "extraiu %d falas, esperava 4", n);
    CONFERE(temGordonSecao, "a secao nao virou o nome de quem fala");
    CONFERE(temDetetive, "o negrito antes dos dois-pontos nao virou quem fala");
    free(corpo); }

  // --- 4. MOEDA DESCONHECIDA OMITE O FATO ------------------------------------
  // Um numero de nove digitos sem unidade e pior que fato nenhum.
  nPedidos = nSparql = nWiki = 0;
  respSparql = SPARQL_MOEDA;
  respWiki = NULL;
  simularArranque();
  seriefrases_abrir("tt0468569");
  esperar();
  // O ROTULO E CONFERIDO PELA MESMA i18n() QUE O ESCREVEU, nao pela palavra
  // portuguesa crua. Motivo medido em 16/09/2026: o teste roda com
  // ajustes_idioma_ingles() = 1 (nada chamou ajustes_iniciar, entao o padrao
  // vale), e no dia em que a tabela de idioma ganhou { "Bilheteria", "Box
  // office" } este CONFERE passou a falhar sem que uma linha de seriefrases.c
  // tivesse mudado. Um teste que quebra quando alguem TRADUZ uma palavra esta
  // conferindo o idioma, nao o comportamento — e o comportamento aqui e "a
  // bilheteria em dolar aparece, o orcamento em moeda desconhecida some".
  { int i, temOrc = 0, temBil = 0;
    for (i = 0; i < seriefrases_n_fatos(); i++) {
      if (!strcmp(seriefrases_fato_rotulo(i), i18n("Orçamento"))) temOrc = 1;
      if (!strcmp(seriefrases_fato_rotulo(i), i18n("Bilheteria"))) temBil = 1;
    }
    CONFERE(!temOrc, "orcamento em moeda desconhecida foi mostrado");
    CONFERE(temBil, "bilheteria em dolar nao apareceu"); }

  // --- 5. FLUXO COMPLETO: DOIS PEDIDOS, NEM UM A MAIS -----------------------
  nPedidos = nSparql = nWiki = 0;
  respSparql = SPARQL_BB;
  respWiki = WIKI;
  simularArranque();
  seriefrases_abrir("tt0903747:1:1");   // aceita a chave composta
  esperar();
  CONFERE(nSparql == 1 && nWiki == 1, "pedidos: %d sparql, %d wikiquote",
          nSparql, nWiki);
  CONFERE(seriefrases_n() == 4, "frases %d", seriefrases_n());
  CONFERE(!strcmp(seriefrases_pagina(), "Breaking Bad"), "pagina \"%s\"",
          seriefrases_pagina());
  CONFERE(seriefrases_em_portugues() == 0, "marcou pt numa pagina en");
  CONFERE(seriefrases_n_fatos() >= 4, "fatos %d", seriefrases_n_fatos());

  // --- 6. O CACHE SOBREVIVE AO ARRANQUE -------------------------------------
  simularArranque();
  nPedidos = nSparql = nWiki = 0;
  seriefrases_abrir("tt0903747");
  esperar();
  CONFERE(nPedidos == 0, "a segunda visita gastou %d pedidos", nPedidos);
  CONFERE(seriefrases_n() == 4, "frases do cache %d", seriefrases_n());
  CONFERE(!strcmp(seriefrases_pagina(), "Breaking Bad"),
          "pagina do cache \"%s\"", seriefrases_pagina());
  { int i, achou = 0;
    for (i = 0; i < seriefrases_n_fatos(); i++)
      if (!strcmp(seriefrases_fato_valor(i), "AMC")) achou = 1;
    CONFERE(achou, "a emissora nao voltou do cache"); }

  // --- 7. CACHE NEGATIVO ----------------------------------------------------
  // O caso comum, medido: a maioria das series nao tem nada. Sem gravar o
  // "nada", toda visita a essas series paga dois pedidos de novo.
  nPedidos = nSparql = nWiki = 0;
  respSparql = SPARQL_VAZIO;
  respWiki = NULL;
  simularArranque();
  seriefrases_abrir("tt7777777");
  esperar();
  CONFERE(nSparql == 1 && nWiki == 0,
          "titulo sem nada: %d sparql, %d wikiquote (esperava 1 e 0)",
          nSparql, nWiki);
  CONFERE(seriefrases_n() == 0 && seriefrases_n_fatos() == 0,
          "titulo sem nada trouxe %d frases e %d fatos",
          seriefrases_n(), seriefrases_n_fatos());
  simularArranque();
  nPedidos = 0;
  seriefrases_abrir("tt7777777");
  esperar();
  CONFERE(nPedidos == 0, "o cache negativo nao pegou (%d pedidos)", nPedidos);

  // --- 8. FALHA DE TRANSPORTE NAO VIRA CACHE NEGATIVO -----------------------
  // ACONTECEU DE VERDADE na conferencia ao vivo: o query.wikidata.org estourou
  // o tempo (falha 28 da libcurl) e a versao anterior gravou "este titulo nao
  // tem nada" — que valeria 30 dias. Sem resposta nao ha fato nenhum a guardar,
  // e a proxima visita TEM de tentar de novo.
  nPedidos = nSparql = nWiki = 0;
  respSparql = NULL;                 /* rede_baixar_com devolve NULL em falha */
  respWiki = NULL;
  simularArranque();
  seriefrases_abrir("tt5555555");
  esperar();
  CONFERE(nSparql == 1, "%d pedidos na falha, esperava 1", nSparql);
  simularArranque();
  nPedidos = nSparql = 0;
  respSparql = SPARQL_VAZIO;
  seriefrases_abrir("tt5555555");
  esperar();
  CONFERE(nSparql == 1, "a falha virou cache negativo (%d pedidos na 2a visita)",
          nSparql);

  // --- 9. CACHE VENCIDO ------------------------------------------------------
  { char nome[64], caminho[600], buf[200];
    FILE *f;
    nomeArquivo(nome, sizeof nome, "tt6666666");
    snprintf(buf, sizeof buf, "%s\n%lld\t\t0\n", SF_ARQ_V,
             (long long)time(NULL) - (long long)(SF_VALIDADE_DIAS + 1) * 86400);
    dados_caminho(caminho, sizeof caminho, nome);
    f = fopen(caminho, "w");
    CONFERE(f != NULL, "nao consegui gravar o cache vencido");
    if (f) { fputs(buf, f); fclose(f); }
    simularArranque();
    nPedidos = 0;
    respSparql = SPARQL_VAZIO;
    seriefrases_abrir("tt6666666");
    esperar();
    CONFERE(nPedidos == 1, "cache vencido foi usado (%d pedidos)", nPedidos); }

  printf(falhas ? "seriefrases: %d falha(s)\n" : "seriefrases: ok\n", falhas);
  return falhas ? 1 : 0;
}
