// Guia de TV: o EPG externo (epg.c) precisa casar o NOME que o addon da com o
// canal da grade XMLTV, e responder "o que esta no ar" e "o que vem a seguir".
//
// Molde minimo de XMLTV com os casos medidos no FrostView real:
//   - "Globo RJ" no addon vs "Globo.RJ.br" na grade (chave pelo id);
//   - "RecordTV Paulista" vs "Record TV" (prefixo depois de normalizar);
//   - "Canal Sony" vs "SONY" (palavra inutil na variante curta);
//   - "H2" vs "History 2" (apelido da tabela);
//   - "Sao.Paulo/SP..Cartoonito.br": id regional — a chave sai depois do "..";
//   - "TV.Aparecida.(aberta).br": parentese nao entra na chave;
//   - "SBT Thathi Vale": afiliada herda a rede "sbt" pelo primeiro token;
//   - "TV Cidade - RecordTV": substring — a chave mais comprida ("recordtv")
//     vence "record", e a mesma chave em dois canais nao e ambiguidade;
//   - canal "24h" sem grade nenhuma (nao casa, nunca).
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../src/epg.h"

// dubles do que epg.c referencia mas o teste nao exercita
char *rede_baixar_bin(const char *u, int s, long *n) { (void)u;(void)s;(void)n; return 0; }
char *dados_ler(const char *n)        { (void)n; return 0; }
int   dados_gravar_leve(const char *n, const char *c) { (void)n;(void)c; return 1; }
void  dados_marcar_sujo(int l)        { (void)l; }
void  dados_fs_travar(void)           {}
void  dados_fs_liberar(void)          {}
char *dados_caminho(char *d, unsigned t, const char *n) { (void)d;(void)t;(void)n; return 0; }
int   dados_apagar(const char *n)     { (void)n; return 0; }
const char *dados_dir(void)           { return "/tmp"; }

static int falhas;
static void confere(const char *o_que, int ok) {
  printf("  %-64s %s\n", o_que, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}

// Datas relativas a agora, em formato XMLTV, para o teste nao depender do dia.
static void xtvtime(char *dst, time_t t) {
  struct tm *m = gmtime(&t);
  strftime(dst, 24, "%Y%m%d%H%M%S +0000", m);
}

int main(void) {
  time_t agora = time(NULL);
  char ini1[24], fim1[24], ini2[24], fim2[24], ini3[24], fim3[24];
  char *xml;
  EpgProg p;

  xtvtime(ini1, agora - 1800); xtvtime(fim1, agora + 1800);   // no ar
  xtvtime(ini2, agora + 1800); xtvtime(fim2, agora + 5400);   // proximo
  xtvtime(ini3, agora + 5400); xtvtime(fim3, agora + 7200);   // depois

  const char *molde =
    "<?xml version=\"1.0\"?><tv>"
    "<channel id=\"Globo.RJ.br\"><display-name>Globo RJ</display-name></channel>"
    "<channel id=\"Record.TV.br\"><display-name>Record TV</display-name></channel>"
    "<channel id=\"Record.br\"><display-name>RECORD</display-name></channel>"
    "<channel id=\"SP..Record.TV.br\"><display-name>Record TV SP</display-name></channel>"
    "<channel id=\"Sony.br\"><display-name>SONY CHANNEL</display-name></channel>"
    "<channel id=\"History.2.br\"><display-name>History 2</display-name></channel>"
    "<channel id=\"Sao.Paulo/SP..Cartoonito.br\"><display-name>SP  Cartoonito HD</display-name></channel>"
    "<channel id=\"MG..TV.Aparecida.(aberta).br\"><display-name>MG  TV Aparecida</display-name></channel>"
    "<channel id=\"SBT.br\"><display-name>SBT</display-name></channel>"
    "<programme channel=\"Globo.RJ.br\" start=\"%s\" stop=\"%s\"><title>Jornal Nacional</title></programme>"
    "<programme channel=\"Globo.RJ.br\" start=\"%s\" stop=\"%s\"><title>Novela &amp; Cia</title></programme>"
    "<programme channel=\"Globo.RJ.br\" start=\"%s\" stop=\"%s\"><title>Filme da Noite</title></programme>"
    "<programme channel=\"Record.TV.br\" start=\"%s\" stop=\"%s\"><title>Jornal da Record</title></programme>"
    "</tv>";

  size_t cap = strlen(molde) + 400;
  xml = malloc(cap);
  snprintf(xml, cap, molde, ini1, fim1, ini2, fim2, ini3, fim3, ini1, fim1);

  confere("processa o molde sem erro", epg_xml_processar(xml) == 4);

  { int g = epg_match("Globo RJ");
    confere("Globo RJ casa pela forma do id", g >= 0);
    confere("agora = Jornal Nacional",
            g >= 0 && epg_agora(g, agora, &p) && !strcmp(p.titulo, "Jornal Nacional"));
    confere("proximo = Novela & Cia (entidade decodificada)",
            g >= 0 && epg_proximo(g, agora, 0, &p) && !strcmp(p.titulo, "Novela & Cia"));
    confere("depois = Filme da Noite",
            g >= 0 && epg_proximo(g, agora, 1, &p) && !strcmp(p.titulo, "Filme da Noite"));
  }
  confere("RecordTV Paulista herda a grade Record TV",
          epg_match("RecordTV Paulista") >= 0);
  confere("Canal Sony casa com SONY CHANNEL",
          epg_match("Canal Sony") >= 0);
  confere("H2 casa pelo apelido com History 2",
          epg_match("H2") >= 0);
  confere("id regional Sao.Paulo/SP..Cartoonito casa",
          epg_match("Cartoonito") >= 0);
  confere("parentese do id nao quebra TV Aparecida",
          epg_match("TV Aparecida") >= 0);
  confere("afiliada SBT Thathi Vale herda a rede sbt",
          epg_match("SBT Thathi Vale") >= 0);
  confere("TV Cidade - RecordTV casa pela chave mais comprida",
          epg_match("TV Cidade - RecordTV") >= 0);
  confere("canal 24h sem grade nao casa",
          epg_match("Aladdin 24h") < 0);
  confere("agora fora de canal valido devolve 0",
          !epg_agora(9999, agora, &p));

  if (falhas) { printf("FALHOU: %d checagem(ns)\n", falhas); return 1; }
  puts("PASS: EPG casa nomes do addon com a grade e responde agora/proximos.");
  free(xml);
  return 0;
}
