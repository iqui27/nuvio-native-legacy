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
#include "nvtmp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../src/epg.h"
#include "../src/rede.h"
#include <zlib.h>

// dubles do que epg.c referencia mas o teste nao exercita
// As fontes do epgshare01 vem por aqui. `urlsPedidas` junta o que foi pedido
// (#158: a escolha de pais tem de pedir RO1/RO2, e nao BR1...); `roCorpo` e o
// que o "epgshare01" responde para o RO1.
static char urlsPedidas[4096];
static char *roCorpo; static long roN;
char *rede_baixar_bin(const char *u, int s, long *n) {
  (void)s;
  if (strlen(urlsPedidas) + strlen(u) + 2 < sizeof urlsPedidas) { strcat(urlsPedidas, u); strcat(urlsPedidas, "\n"); }
  if (roCorpo && strstr(u, "epg_ripper_RO1.xml.gz")) {
    char *b = malloc((size_t)roN); memcpy(b, roCorpo, (size_t)roN); *n = roN; return b;
  }
  return 0;
}

uint64_t perfis_sessao_geracao(void) { return 0; }
int perfis_sessao_atual(uint64_t s) { return s == 0; }
char *rede_baixar_bin_medido_controle(const char *url, int s, const char *const *cab,
    const RedeControle *ctl, long *n, RedeMedida *m) {
  (void)cab; (void)ctl; (void)m; return rede_baixar_bin(url, s, n);
}

// A GRADE DO PROVEDOR (#158) vem por aqui, EM FLUXO (M4): `extraCorpo`/`extraN`
// e o que o "servidor" responde, entregue em pedacos de 777 bytes (corte no meio
// das tags de proposito); `extraLimitado` simula a rede caindo; `geradorN` > 0
// troca o corpo por um XMLTV gerado de `geradorN` canais (o arquivo grande que
// nunca existe inteiro na memoria do teste).
static char *extraCorpo; static long extraN; static int extraLimitado, extraPedidos;
static char extraUltimaUrl[1100];
static int geradorN; static long long geradorBytes;
static void xtvtime(char *dst, time_t t);
int rede_baixar_fluxo(const char *url, int segundos, const char *const *cab,
                      int (*cb)(const char *dados, size_t n, void *u), void *u,
                      long long maxBytes, int *status, long long *bytes) {
  long i;
  (void)segundos; (void)cab; (void)maxBytes;
  extraPedidos++;
  snprintf(extraUltimaUrl, sizeof extraUltimaUrl, "%s", url);
  if (status) *status = 200;
  if (bytes) *bytes = 0;
  if (geradorN > 0) {
    char bloco[2048], a[24], b[24];
    time_t t = time(NULL);
    int c, k;
    geradorBytes = 0;
    xtvtime(a, t - 600); xtvtime(b, t + 3000);
    cb("<?xml version=\"1.0\"?><tv>", 26, u); geradorBytes += 26;
    for (c = 0; c < geradorN; c++) {
      int w = snprintf(bloco, sizeof bloco, "<channel id=\"g%d.x\"><display-name>Gerado %d</display-name></channel>", c, c);
      cb(bloco, (size_t)w, u); geradorBytes += w;
    }
    for (c = 0; c < geradorN; c++)
      for (k = 0; k < 6; k++) {
        int w;
        xtvtime(a, t - 600 + k * 3600); xtvtime(b, t + 3000 + k * 3600);
        w = snprintf(bloco, sizeof bloco,
          "<programme channel=\"g%d.x\" start=\"%s\" stop=\"%s\"><title>Programa %d de %d</title>"
          "<desc>Uma descricao comprida o bastante para o arquivo pesar como o de um provedor de verdade, "
          "com elenco, sinopse e classificacao indicativa.</desc></programme>", c, a, b, k, c);
        cb(bloco, (size_t)w, u); geradorBytes += w;
      }
    cb("</tv>", 5, u);
    if (bytes) *bytes = geradorBytes;
    return 1;
  }
  if (!extraCorpo) { if (status) *status = 0; return 0; }
  for (i = 0; i < extraN; i += 777) {
    if (extraLimitado && i > extraN / 2) { if (status) *status = 0; return 0; }
    if (!cb(extraCorpo + i, (size_t)(extraN - i < 777 ? extraN - i : 777), u)) break;
  }
  if (bytes) *bytes = extraN;
  return 1;
}

static long gz(const char *in, char **out) {
  z_stream z; long cap = (long)strlen(in) + 256;
  *out = malloc((size_t)cap);
  memset(&z, 0, sizeof z);
  deflateInit2(&z, 6, Z_DEFLATED, 16 + 15, 8, Z_DEFAULT_STRATEGY);
  z.next_in = (Bytef *)in; z.avail_in = (uInt)strlen(in);
  z.next_out = (Bytef *)*out; z.avail_out = (uInt)cap;
  deflate(&z, Z_FINISH);
  deflateEnd(&z);
  return (long)z.total_out;
}

// Roda o fio da carga ate publicar (epg_passo e quem publica).
static int carregar(void) {
  int k;
  for (k = 0; k < 30000; k++) {      // ate 60 s: o arquivo de 100 MB sob ASan
    struct timespec ts = { 0, 2000000 };
    epg_passo();
    if (epg_estado() == EPG_PRONTO || epg_estado() == EPG_FALHOU) return epg_estado();
    nanosleep(&ts, NULL);
  }
  return -1;
}
// Disco num diretorio temporario de verdade: o cache binario por fonte (M4)
// precisa ir e voltar. Sem NUVIO_TESTE_DIR, nada e gravado (como antes).
static char dirTeste[512];
char *dados_caminho(char *d, unsigned t, const char *n) {
  if (!dirTeste[0]) return 0;
  snprintf(d, t, "%s/%s", dirTeste, n); return d;
}
char *dados_ler(const char *n) {
  char c[700]; FILE *f; long t; char *b;
  if (!dados_caminho(c, sizeof c, n) || !(f = fopen(c, "rb"))) return 0;
  fseek(f, 0, SEEK_END); t = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)t + 1); if (b) { b[fread(b, 1, (size_t)t, f)] = 0; }
  fclose(f); return b;
}
int   dados_gravar_leve(const char *n, const char *c) {
  char cam[700]; FILE *f;
  if (!dados_caminho(cam, sizeof cam, n) || !(f = fopen(cam, "wb"))) return 1;
  fputs(c, f); fclose(f); return 1;
}
void  dados_marcar_sujo(int l)        { (void)l; }
void  dados_fs_travar(void)           {}
void  dados_fs_liberar(void)          {}
int   dados_apagar(const char *n)     { char c[700]; if (dados_caminho(c, sizeof c, n)) remove(c); return 0; }
const char *dados_dir(void)           { return dirTeste[0] ? dirTeste : "/tmp"; }

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

  // --- pelo id do canal (#158) ---------------------------------------------
  confere("id exato casa (Globo.RJ.br)",
          epg_match_id("Globo.RJ.br") >= 0 && epg_match_id("Globo.RJ.br") == epg_match("Globo RJ"));
  confere("id de canal sem programa nao e resposta (Sony.br)",
          epg_match_id("Sony.br") < 0);
  confere("id vazio ou desconhecido: -1",
          epg_match_id("") < 0 && epg_match_id("ProTV.ro") < 0);

  // --- a grade do provedor como sexta fonte ---------------------------------
  { char molde2[1200], *z = NULL; long nz;
    int e;
    snprintf(molde2, sizeof molde2,
      "\xEF\xBB\xBF\n<?xml version=\"1.0\"?><tv>"
      "<channel id=\"ProTV.ro\"><display-name>PRO TV HD</display-name></channel>"
      "<programme channel=\"ProTV.ro\" start=\"%s\" stop=\"%s\"><title>Stirile Pro TV</title></programme>"
      "</tv>", ini1, fim1);
    extraCorpo = molde2; extraN = (long)strlen(molde2);
    epg_fonte_extra("http://painel.exemplo:8080/xmltv.php?username=u&password=SEGREDO");
    epg_iniciar();
    e = carregar();
    confere("XML puro (com BOM) do provedor carrega", e == EPG_PRONTO);
    { int g = epg_match_id("ProTV.ro");
      confere("canal romeno casa pelo epg_channel_id",
              g >= 0 && epg_agora(g, agora, &p) && !strcmp(p.titulo, "Stirile Pro TV")); }
    confere("pediu a URL do provedor", strstr(extraUltimaUrl, "xmltv.php") != NULL);

    // gzip: o xmltv.php de alguns paineis responde comprimido
    nz = gz(molde2 + 4, &z);
    extraCorpo = z; extraN = nz;
    epg_fonte_extra("http://outro.exemplo/xmltv.php?username=u&password=p");
    e = carregar();                        // epg_passo ve a troca e recarrega
    confere("gzip do provedor carrega", e == EPG_PRONTO && epg_match_id("ProTV.ro") >= 0);

    // Filtro (#158): da grade do provedor so entram os ids da lista do Xtream.
    { char molde5[1600];
      const char *ids[] = { "Antena1.ro" };
      snprintf(molde5, sizeof molde5,
        "<?xml version=\"1.0\"?><tv>"
        "<channel id=\"ProTV.ro\"><display-name>PRO TV</display-name></channel>"
        "<channel id=\"Antena1.ro\"><display-name>Antena 1</display-name></channel>"
        "<programme channel=\"ProTV.ro\" start=\"%s\" stop=\"%s\"><title>A</title></programme>"
        "<programme channel=\"Antena1.ro\" start=\"%s\" stop=\"%s\"><title>B</title></programme>"
        "</tv>", ini1, fim1, ini1, fim1);
      extraCorpo = molde5; extraN = (long)strlen(molde5);
      epg_fonte_extra_ids(ids, 1);
      epg_fonte_extra("http://filtro.exemplo/xmltv.php?username=u&password=p");
      e = carregar();
      confere("filtro: so o canal da lista entra da grade do provedor",
              e == EPG_PRONTO && epg_match_id("Antena1.ro") >= 0 && epg_match_id("ProTV.ro") < 0);
      epg_fonte_extra_ids(NULL, 0);
      extraCorpo = z; extraN = nz; }

    // a rede cai no meio do arquivo: nada quebra (o que veio fica, parcial)
    extraLimitado = 1;
    epg_fonte_extra("http://grande.exemplo/xmltv.php?username=u&password=p");
    e = carregar();
    confere("rede caindo no meio: a carga termina", e == EPG_PRONTO || e == EPG_FALHOU);
    extraLimitado = 0;

    // "" tira a fonte
    { int antes = extraPedidos;
      epg_fonte_extra("");
      e = carregar();
      confere("sem fonte extra, nada e pedido", extraPedidos == antes); }
    free(z); extraCorpo = NULL; }

  // --- nomes romenos e de painel (#158) --------------------------------------
  { char molde3[4000];
    snprintf(molde3, sizeof molde3,
      "<?xml version=\"1.0\"?><tv>"
      "<channel id=\"PRO.TV.ro\"><display-name lang=\"ro\">PRO TV</display-name></channel>"
      "<channel id=\"Pro.TV.2.ro\"><display-name lang=\"ro\">Pro 2</display-name></channel>"
      "<channel id=\"Antena.1.ro\"><display-name lang=\"ro\">Antena 1</display-name></channel>"
      "<channel id=\"TVR.Timi\xc8\x99oara.ro\"><display-name lang=\"ro\">TVR Timi\xc8\x99oara</display-name></channel>"
      "<channel id=\"Digi.Sport.ro\"><display-name lang=\"ro\">Digi Sport</display-name></channel>"
      "<channel id=\"Acas\xc4\x83.(HD).ro\"><display-name lang=\"ro\">Acas\xc4\x83 (HD)</display-name></channel>"
      "<programme channel=\"PRO.TV.ro\" start=\"%s\" stop=\"%s\"><title>\xc8\x98tirile Pro TV</title></programme>"
      "<programme channel=\"Antena.1.ro\" start=\"%s\" stop=\"%s\"><title>Observator</title></programme>"
      "<programme channel=\"TVR.Timi\xc8\x99oara.ro\" start=\"%s\" stop=\"%s\"><title>Jurnal</title></programme>"
      "<programme channel=\"Digi.Sport.ro\" start=\"%s\" stop=\"%s\"><title>Fotbal</title></programme>"
      "<programme channel=\"Acas\xc4\x83.(HD).ro\" start=\"%s\" stop=\"%s\"><title>Serial</title></programme>"
      "</tv>", ini1, fim1, ini1, fim1, ini1, fim1, ini1, fim1, ini1, fim1);
    confere("molde romeno processa", epg_xml_processar(molde3) == 5);
    { int pro = epg_match_id("PRO.TV.ro");
      confere("RO: PRO TV FHD casa (prefixo e qualidade fora)", pro >= 0 && epg_match("RO: PRO TV FHD") == pro);
      confere("|RO| Pro TV \xe1\xb4\xb4\xe1\xb4\xb0 casa (letras modificadoras)", epg_match("|RO| Pro TV \xe1\xb4\xb4\xe1\xb4\xb0") == pro);
      confere("[RO] PRO TV HEVC casa", epg_match("[RO] PRO TV HEVC") == pro);
      confere("RO - Pro TV casa (pais conhecido)", epg_match("RO - Pro TV") == pro);
      confere("RO\xe2\x80\xa2PRO TV casa", epg_match("RO\xe2\x80\xa2PRO TV") == pro);
      confere("Pro TV 2 NAO herda a grade da Pro TV (guarda de numero)", epg_match("Pro TV 2") != pro); }
    confere("RO | Antena 1 HD casa", epg_match("RO | Antena 1 HD") == epg_match_id("Antena.1.ro"));
    confere("TVR Timisoara sem acento casa com o id em \xc8\x99", epg_match("TVR Timisoara") >= 0 &&
            epg_match("TVR Timisoara") == epg_match_id("TVR.Timi\xc8\x99oara.ro"));
    confere("TVR Timi\xc5\x9foara (cedilha) casa igual", epg_match("TVR Timi\xc5\x9foara") == epg_match("TVR Timisoara"));
    confere("Digi Sport 1 NAO casa com Digi Sport", epg_match("Digi Sport 1") != epg_match_id("Digi.Sport.ro"));
    confere("Acasa (sem breve) casa com Acas\xc4\x83 (HD)", epg_match("RO: Acasa HD") == epg_match_id("Acas\xc4\x83.(HD).ro"));
    { char pais[4];
      const char *r = epg_sem_prefixo("RO: Pro TV", pais);
      confere("sem_prefixo: RO: -> pais RO", !strcmp(r, "Pro TV") && !strcmp(pais, "RO"));
      r = epg_sem_prefixo("|UK| BBC One", pais);
      confere("sem_prefixo: |UK| -> pais UK", !strcmp(r, "BBC One") && !strcmp(pais, "UK"));
      r = epg_sem_prefixo("TV - Record", pais);
      confere("sem_prefixo: 'TV - Record' fica inteiro", !strcmp(r, "TV - Record") && !pais[0]);
      r = epg_sem_prefixo("VIP: HBO", pais);
      confere("sem_prefixo: prefixo de pacote sai, sem pais", !strcmp(r, "HBO") && !pais[0]);
      r = epg_sem_prefixo("RO:", pais);
      confere("sem_prefixo: nome que e so prefixo fica", !strcmp(r, "RO:"));
      r = epg_sem_prefixo("Antena 1", pais);
      confere("sem_prefixo: sem prefixo fica", !strcmp(r, "Antena 1") && !pais[0]); }
  }

  // --- escolha de pais das fontes (#158) -------------------------------------
  { char molde4[800], *z = NULL; long nz; int e;
    confere("paises: ro -> 2 arquivos (RO1, RO2)", epg_paises_definir("ro") == 2 &&
            !strcmp(epg_paises_ativos(), "RO"));
    confere("paises: RO,BR,RO -> 4 arquivos, sem repetir", epg_paises_definir("RO,BR,RO") == 4 &&
            !strcmp(epg_paises_ativos(), "RO,BR"));
    confere("paises: GB vira UK", epg_paises_definir("gb") == 1 && !strcmp(epg_paises_ativos(), "UK"));
    confere("paises: desconhecido cai no padrao", epg_paises_definir("xx") == 5);
    confere("paises: vazio cai no padrao", epg_paises_definir("") == 5 &&
            !strcmp(epg_paises_ativos(), "BR,PT,MX,AR"));
    confere("pais_existe: RO sim, XX nao", epg_pais_existe("ro") && !epg_pais_existe("XX"));
    snprintf(molde4, sizeof molde4,
      "<?xml version=\"1.0\"?><tv><channel id=\"PRO.TV.ro\"><display-name>PRO TV</display-name></channel>"
      "<programme channel=\"PRO.TV.ro\" start=\"%s\" stop=\"%s\"><title>Stirile</title></programme></tv>",
      ini1, fim1);
    nz = gz(molde4, &z);
    roCorpo = z; roN = nz;
    epg_fonte_extra("");
    epg_paises_definir("RO");
    urlsPedidas[0] = 0;
    epg_teste_limpar();
    epg_iniciar();
    e = carregar();
    confere("com RO escolhido, pede RO1 e RO2 e nao BR1",
            strstr(urlsPedidas, "epg_ripper_RO1.xml.gz") && strstr(urlsPedidas, "epg_ripper_RO2.xml.gz") &&
            !strstr(urlsPedidas, "BR1"));
    confere("grade do RO1 publicada", e == EPG_PRONTO && epg_match("RO: Pro TV HD") >= 0);
    // Trocar de pais com a grade pronta recarrega no epg_passo.
    urlsPedidas[0] = 0;
    epg_paises_definir("BR");
    e = carregar();
    { int k; for (k = 0; k < 200 && !strstr(urlsPedidas, "BR1"); k++) { struct timespec ts = { 0, 2000000 }; epg_passo(); nanosleep(&ts, NULL); }
      e = carregar(); }
    confere("trocar o pais recarrega a grade", strstr(urlsPedidas, "epg_ripper_BR1.xml.gz") != NULL);
    roCorpo = NULL; free(z); epg_paises_definir(""); }


  // --- M4: XMLTV EM FLUXO, filtrado pela lista -------------------------------
  // 60 mil canais e 360 mil programas (~110 MB de XML) passam em pedacos; da
  // grade so ficam os 300 canais que a lista tem (por id) e o nome de um.
  { static const char *ids[300];
    static char buf[300][16];
    const char *nomes[1] = { "Gerado 59999" };
    EpgExtra x;
    EpgFonteInfo inf[4];
    int k, e;
    for (k = 0; k < 300; k++) { snprintf(buf[k], sizeof buf[k], "G%d.X", k * 7); ids[k] = buf[k]; }
    memset(&x, 0, sizeof x);
    x.chave = "m0000beef"; x.url = "http://grade.exemplo/enorme.xml?user=u&pass=SEGREDO";
    x.ids = ids; x.nIds = 300; x.nomes = nomes; x.nNomes = 1;
    epg_fonte_extra("");
    epg_paises_definir("");
    epg_teste_limpar();
    geradorN = 60000;
    epg_extras_definir(&x, 1);
    epg_iniciar();
    e = carregar();
    geradorN = 0;
    printf("  (gerado: %lld MB de XMLTV)\n", geradorBytes >> 20);
    confere("XMLTV de 100 MB+ entra em fluxo", e == EPG_PRONTO && geradorBytes > (100LL << 20));
    confere("filtro: so os 301 canais da lista ficam", epg_n_canais() == 301);
    { int g = epg_match_id("g693.x");      // 99 * 7, sem caixa
      confere("id sem caixa casa (G693.X na lista, g693.x na grade)",
              g >= 0 && epg_agora(g, agora, &p) && !strncmp(p.titulo, "Programa 0", 10)); }
    confere("nome da lista tambem entra pelo filtro", epg_match("Gerado 59999") >= 0);
    k = epg_fontes_info(inf, 4);
    confere("info da fonte: ok, 1806 programas, nao veio do cache",
            k == 1 && inf[0].ok && !inf[0].deCache && inf[0].programas == 301 * 6 &&
            !strcmp(inf[0].chave, "m0000beef"));
    confere("a URL com senha nao esta na chave", !strstr(inf[0].chave, "SEGREDO"));

    // --- M5: cascata --------------------------------------------------------
    { int m = -1, g;
      g = epg_casar("m0000beef", "G7.X", NULL, "Nome que nao bate", &m);
      confere("cascata 1: epg id", g >= 0 && m == EPG_M_ID);
      g = epg_casar("m0000beef", "", "Gerado 14", "outro", &m);
      confere("cascata 2: tvg-name normalizado", g >= 0 && m == EPG_M_NOME &&
              !strcmp(epg_canal_id(g), "g14.x"));
      g = epg_casar("m0000beef", NULL, NULL, "GERADO 21 HD", &m);
      confere("cascata 3: nome sem qualidade e sem caixa", g >= 0 && m == EPG_M_NOME);
      g = epg_casar(NULL, NULL, NULL, "Canal que nao existe", &m);
      confere("sem casamento: -1 e metodo nenhum", g < 0 && m == EPG_M_NENHUM);
      confere("epg_canal_fonte = a chave da grade", g < 0 && !strcmp(epg_canal_fonte(epg_match_id("g7.x")), "m0000beef"));
      confere("epg_achar por fonte + id (escolha manual)",
              epg_achar("m0000beef", "g14.x") >= 0 && epg_achar("outra", "g14.x") < 0); }
    { int sug[5], n = epg_sugerir("Gerado 2093 FHD", sug, 5);
      confere("sugestao (fuzzy) traz o canal certo primeiro",
              n > 0 && !strcmp(epg_canal_nome(sug[0]), "Gerado 2093")); }

    // --- M4: cache binario por fonte ------------------------------------------
    { char dir[] = "/tmp/nuvio-guiaepg-XXXXXX";
      int antes;
      if (mkdtemp(dir)) {
        snprintf(dirTeste, sizeof dirTeste, "%s", dir);
        geradorN = 3000;
        epg_teste_limpar(); epg_iniciar(); e = carregar();
        antes = extraPedidos;
        epg_teste_limpar(); epg_iniciar(); e = carregar();
        k = epg_fontes_info(inf, 4);
        confere("segunda abertura le o cache binario, sem rede",
                e == EPG_PRONTO && extraPedidos == antes && k == 1 && inf[0].deCache &&
                epg_match_id("g693.x") >= 0);
        { char cmd[700]; snprintf(cmd, sizeof cmd, "grep -c SEGREDO %s/* >/dev/null 2>&1", dir);
          confere("o cache nao guarda a URL nem a senha", system(cmd) != 0); }
        // a rede cai e o cache e velho: fica o velho (fonte degradada)
        { char cam[700]; FILE *f;
          snprintf(cam, sizeof cam, "%s/epg-x-m0000beef-p1.bin", dir);
          f = fopen(cam, "r+b");
          if (f) { long long velho = (long long)time(NULL) - 3 * 86400;
                   fseek(f, 16 + 16 + (long)sizeof(unsigned long), SEEK_SET);
                   fwrite(&velho, sizeof velho, 1, f); fclose(f); } }
        geradorN = 0; extraCorpo = NULL;
        epg_teste_limpar(); epg_iniciar(); e = carregar();
        k = epg_fontes_info(inf, 4);
        confere("rede fora + cache velho: fica o velho, marcado do cache",
                e == EPG_PRONTO && k == 1 && inf[0].ok && inf[0].deCache && inf[0].idadeS > 86400);
        { char cmd[700]; snprintf(cmd, sizeof cmd, "rm -rf %s", dir); if (system(cmd)) {} }
        dirTeste[0] = 0;
      } }
    epg_extras_definir(NULL, 0);
    geradorN = 0; }

  if (falhas) { printf("FALHOU: %d checagem(ns)\n", falhas); return 1; }
  puts("PASS: EPG casa nomes do addon com a grade e responde agora/proximos.");
  free(xml);
  return 0;
}
