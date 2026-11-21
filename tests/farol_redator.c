#include "farolredator.h"
#include "sha256.h"
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char permitidos[] =
 "|v|gatilho|sintoma|erro|erro.codigo|erro.estagio|erro.nativo|erro.nativo_num|erro.http|erro.fingerprint|erro.fp|erro.tentou|erro.tentou[]|erro.tentou[].acao|erro.tentou[].ok|erro.tentou[].ms|"
 "aparelho|aparelho.plataforma|aparelho.so|aparelho.so_maior|aparelho.modelo|aparelho.ram_total_mb|aparelho.ram_livre_mb|aparelho.tela|aparelho.tela.w|aparelho.tela.h|aparelho.tela.hdr|aparelho.tela.dv|aparelho.tela.hevc|aparelho.tela.av1|aparelho.audio|aparelho.audio.dts|aparelho.audio.truehd|aparelho.audio.eac3|aparelho.audio.ac3|aparelho.rede|aparelho.rede.offline|aparelho.rede.hosts_com_falha|aparelho.rede.vazao_mbps|"
 "cliente|cliente.versao|cliente.commit|cliente.build|cliente.backend|cliente.idioma|cliente.aberto_s|cliente.gpu|cliente.ajustes|cliente.ajustes.glass_ui|cliente.ajustes.qualidade_max|cliente.ajustes.tamanho_max_gb|cliente.ajustes.dts|cliente.ajustes.dv_mkv|cliente.ajustes.legenda_auto_sync|cliente.addons|cliente.addons.total|cliente.addons.ativos|cliente.addons.isolados|cliente.addons.tipos|cliente.addons.tipos[]|cliente.debrid|cliente.debrid[]|cliente.contas|cliente.contas[]|"
 "midia|midia.container|midia.video|midia.hdr|midia.res|midia.fps|midia.mbps|midia.gb|midia.audio|midia.legenda|midia.entrega|midia.http|midia.ttfb_ms|"
 "cadeia|cadeia[]|cadeia[].passo|cadeia[].estado|cadeia[].ms|cadeia[].http|"
 "linha|linha[]|linha[].t|linha[].m|linha[].e|linha[].a|linha[].b|linha[].k|privacidade|privacidade.redator|privacidade.campos_recusados|";
static const char *canarios[] = {
 "CANARIO_0", "canario_1", "CANARIO_2.com", "https://exemplo.invalid/CANARIO_3?chave=CANARIO_4#CANARIO_5",
 "magnet:?xt=urn:btih:CANARIO_6", "Título CANARIO_7", "Perfil CANARIO_8", "CANARIO_9@exemplo.invalid",
 "192.0.2.1", "2001:db8::1", "eyJhbGciOiJIUzI1NiJ9.CANARIO_10.assinatura",
 "Authorization: Bearer CANARIO_11", "https://CANARIO_12:CANARIO_13@exemplo.invalid/a",
 "/home/CANARIO_14/arquivo", "PIN CANARIO_15", "1234", "CANARIO_16%2Fsegredo", "Bearer CANARIO_17",
 "0123456789abcdef0123456789abcdef", "nome.CANARIO_18", "usuario:senha@CANARIO_19", "CANARIO_20+token",
 "OLED55CANARIO1", "QN55CANARIO1", "webos-4-starfish-CANARIO21-mkv-hevc10-dv-eac3"
};
static const FarolCampo *schema;
static size_t n_schema;
static unsigned visitas[256];
static int direto(const char *pai, const char *filho, const char **chave) {
  size_t n = strlen(pai);
  if (strncmp(pai, filho, n)) return 0;
  const char *p = filho+n;
  if (!strcmp(p, "[]")) { *chave = NULL; return 1; }
  if (n) { if (*p++ != '.') return 0; }
  if (!*p || strchr(p, '.') || strchr(p, '[')) return 0;
  *chave = p; return 1;
}
static void preencher(FarolJson *j, const char *pai, const char *canario) {
  for (size_t i = 0; i < n_schema; i++) {
    const char *chave;
    const FarolCampo *c = schema+i;
    if (!direto(pai, c->caminho, &chave)) continue;
    visitas[i]++;
    int tipo_errado = 1;
    unsigned recusados = j->campos_recusados;
    assert(!farolredator_campo(j, chave, c->tipo == FT_INT ? FT_BOOL : FT_INT, &tipo_errado));
    assert(j->campos_recusados == recusados+1);
    if (c->tipo == FT_OBJETO || c->tipo == FT_ARRAY) {
      assert(c->tipo == FT_OBJETO ? farolredator_objeto(j, chave) : farolredator_array(j, chave));
      unsigned repeticoes = !strcmp(c->caminho, "linha[]") ? 256 : 1;
      for (unsigned k = 0; k < repeticoes; k++) {
        if (k) assert(farolredator_objeto(j, NULL));
        preencher(j, c->caminho, canario);
        if (repeticoes > 1) assert(farolredator_fechar(j));
      }
      if (repeticoes == 1) assert(farolredator_fechar(j));
    } else {
      int inteiro = c->tipo == FT_BOOL ? 1 : 42;
      double num = 14.24;
      char valor[160];
      const void *v = valor;
      if (c->tipo == FT_INT || c->tipo == FT_BOOL) v = &inteiro;
      else if (c->tipo == FT_NUM1) v = &num;
      else if (canario) strcpy(valor, canario);
      else if (c->tipo == FT_ENUM) {
        size_t n = strcspn(c->valores, "|"); memcpy(valor, c->valores, n); valor[n] = 0;
      } else if (c->tipo == FT_VERSAO) strcpy(valor, "2.1.0");
      else if (c->tipo == FT_HASH) strcpy(valor, !strcmp(c->caminho, "cliente.commit") ? "c18c21756" : "9c1e77ab03f2d4e1");
      else if (!strcmp(c->caminho, "erro.fingerprint")) strcpy(valor, "webos-4-starfish-ums_204-mkv-hevc10-dv-eac3");
      else if (!strcmp(c->caminho, "aparelho.modelo")) strcpy(valor, "OLED55C9PSA");
      else strcpy(valor, "ums_204");
      int rc = farolredator_campo(j, chave, c->tipo, v);
      if (!canario) assert(rc);
    }
    int v = 1;
    unsigned antes = j->campos_recusados;
    assert(!farolredator_campo(j, "CANARIO_chave", FT_INT, &v));
    assert(j->campos_recusados == antes+1);
  }
}
static void seguro(const char *s) {
  assert(!strstr(s, "://") && !strchr(s, '@') && !strstr(s, "%2F") && !strstr(s, "Bearer"));
  for (const char *p = s; *p; p++) {
    if (!strncmp(p, "CANARIO", 7) || !strncmp(p, "canario", 7)) assert(0);
    if (isdigit((unsigned char)*p)) {
      unsigned a,b,c,d; int n = 0;
      if (sscanf(p, "%u.%u.%u.%u%n", &a,&b,&c,&d,&n) == 4 && n && a < 256 && b < 256 && c < 256 && d < 256) assert(0);
    }
    if (strspn(p, "0123456789abcdefABCDEF") >= 20) assert(0);
  }
}
static const char *ler_string(const char *p, char *out, size_t tam) {
  assert(*p++ == '"');
  size_t n = 0;
  while (*p && *p != '"') { assert((unsigned char)*p >= 32 && *p != '\\' && n+1 < tam); out[n++] = *p++; }
  assert(*p++ == '"'); out[n] = 0; return p;
}
static const char *ler_json(const char *p, const char *caminho, int prof) {
  assert(prof < 9);
  if (*caminho) {
    char chave[120]; snprintf(chave, sizeof chave, "|%s|", caminho);
    assert(strstr(permitidos, chave));
  }
  if (*p == '{' || *p == '[') {
    int array = *p++ == '[';
    char fim = array ? ']' : '}';
    if (*p == fim) return p+1;
    do {
      char chave[96], proximo[120];
      if (array) snprintf(proximo, sizeof proximo, "%s[]", caminho);
      else {
        p = ler_string(p, chave, sizeof chave); assert(*p++ == ':');
        assert(snprintf(proximo, sizeof proximo, "%s%s%s", caminho, *caminho ? "." : "", chave) < (int)sizeof proximo);
      }
      p = ler_json(p, proximo, prof+1);
      if (*p != ',') break;
      p++;
    } while (1);
    assert(*p++ == fim); return p;
  }
  if (*p == '"') { char valor[256]; return ler_string(p, valor, sizeof valor); }
  if (!strncmp(p, "true", 4)) return p+4;
  if (!strncmp(p, "false", 5)) return p+5;
  if (*p == '-') p++;
  assert(isdigit((unsigned char)*p));
  do { p++; } while (isdigit((unsigned char)*p));
  if (*p == '.') { p++; assert(isdigit((unsigned char)*p)); do { p++; } while (isdigit((unsigned char)*p)); }
  return p;
}
static void json_valido(const char *p) { assert(!*ler_json(p, "", 0)); }
static void pacote(void) {
  char buf[FAROL_JSON_N]; FarolJson j;
  schema = farolredator_schema(&n_schema);
  assert(n_schema < 256);
  for (size_t i = 0; i < n_schema; i++) {
    char chave[120]; snprintf(chave, sizeof chave, "|%s|", schema[i].caminho);
    assert(strstr(permitidos, chave));
    for (size_t k = 0; k < i; k++) assert(strcmp(schema[i].caminho, schema[k].caminho));
  }
  assert(farolredator_abrir(&j, buf, sizeof buf));
  preencher(&j, "", NULL); assert(farolredator_fechar(&j));
  assert(j.n == strlen(buf) && j.n <= FAROL_JSON_MAX);
  for (size_t i = 0; i < n_schema; i++) assert(visitas[i]);
  seguro(buf); json_valido(buf);
  assert(strstr(buf, "\"modelo\":\"OLED55C9PSA\""));
  assert(strstr(buf, "\"gb\":14.2"));
  for (size_t k = 0; k < sizeof canarios / sizeof *canarios; k++) {
    assert(farolredator_abrir(&j, buf, sizeof buf));
    preencher(&j, "", canarios[k]); assert(farolredator_fechar(&j));
    seguro(buf); json_valido(buf);
    assert(!strstr(buf, canarios[k]));
  }
}
static void tokens(void) {
  unsigned estado = 12345;
  for (int k = 0; k < 100000; k++) {
    char entrada[256], saida[FAROL_REDATOR_TOKEN_N];
    estado = estado * 1664525u + 1013904223u;
    size_t n = estado % 255;
    for (size_t i = 0; i < n; i++) { estado = estado * 1664525u + 1013904223u; entrada[i] = (char)(1 + estado % 255); }
    entrada[n] = 0;
    farolredator_token(entrada, saida);
    assert(strlen(saida) <= 39 && strlen(saida));
    assert(strspn(saida, "abcdefghijklmnopqrstuvwxyz0123456789_.:+-") == strlen(saida));
  }
  char s[40];
  farolredator_token("ABC_+-.:123", s); assert(!strcmp(s, "abc_+-.:123"));
  farolredator_token("abc/segredo", s); assert(!strcmp(s, "x"));
  farolredator_token("abcdefghijklmnopqrstuvwxyz0123456789abcdefghijkl/", s); assert(!strcmp(s, "x"));
  farolredator_token(NULL, s); assert(!strcmp(s, "x"));
}
static void hashes(void) {
  unsigned char sal[16] = {0}; char a[12], b[12], c[12], digest[65];
  Sha s; unsigned char d[32];
  sha_ini(&s); sha_add(&s, (const unsigned char *)"abc", 3); sha_fim(&s, d); hex(d, 32, digest);
  assert(!strcmp(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  farolredator_hash_host("https://user:pass@EXEMPLO.invalid:443/CANARIO_1?q=CANARIO_2", sal, a);
  farolredator_hash_host("exemplo.invalid", sal, b);
  assert(!strcmp(a,b) && strlen(a) == 10 && !strncmp(a, "h:", 2));
  sal[15] = 1; farolredator_hash_host("exemplo.invalid", sal, c); assert(strcmp(a,c));
  farolredator_hash_host("192.0.2.1", sal, c); assert(!strcmp(c, "x"));
  farolredator_hash_host("2001:db8::1", sal, c); assert(!strcmp(c, "x"));
  farolredator_hash_host(NULL, sal, c); assert(!strcmp(c, "x"));
  farolredator_hash_host("exemplo.invalid", NULL, c); assert(!strcmp(c, "x"));
  assert(farolredator_host_publico("API.TRAKT.TV"));
  assert(farolredator_host_publico("api.themoviedb.org"));
  assert(!farolredator_host_publico("api.trakt.tv.exemplo.invalid"));
  assert(!farolredator_host_publico("https://api.trakt.tv/CANARIO_1"));
  assert(!farolredator_host_publico("api.trakt.tv@exemplo.invalid"));
}
static void limites(void) {
  FarolJson j; char buf[40000]; int i = 1; double d = NAN;
  assert(farolredator_abrir(&j, buf, sizeof buf));
  assert(!farolredator_campo(&j, "v", FT_TOKEN, "CANARIO_1"));
  assert(j.campos_recusados == 1 && !strcmp(buf, "{"));
  assert(!farolredator_objeto(&j, "v"));
  assert(farolredator_campo(&j, "v", FT_INT, &i));
  assert(!farolredator_campo(&j, "v", FT_INT, &i));
  assert(!farolredator_campo(&j, "midia.http", FT_INT, &i));
  assert(farolredator_objeto(&j, "midia"));
  assert(!farolredator_campo(&j, "gb", FT_NUM1, &d));
  d = INFINITY; assert(!farolredator_campo(&j, "gb", FT_NUM1, &d));
  d = -1; assert(!farolredator_campo(&j, "gb", FT_NUM1, &d));
  assert(farolredator_fechar(&j)); assert(farolredator_fechar(&j));
  assert(!farolredator_campo(&j, "v", FT_INT, &i));
  assert(!farolredator_fechar(&j));
  for (size_t n = 3; n < 120; n++) {
    memset(buf, '!', sizeof buf);
    assert(farolredator_abrir(&j, buf, n));
    if (farolredator_objeto(&j, "midia")) {
      farolredator_campo(&j, "video", FT_ENUM, "hevc10");
      assert(farolredator_fechar(&j));
    }
    assert(farolredator_fechar(&j));
    assert(j.n < n && buf[n] == '!');
  }
  assert(farolredator_abrir(&j, buf, sizeof buf)); assert(farolredator_array(&j, "linha"));
  for (int k = 0; k < 256; k++) { assert(farolredator_objeto(&j, NULL)); assert(farolredator_fechar(&j)); }
  assert(!farolredator_objeto(&j, NULL));
  assert(farolredator_fechar(&j)); assert(farolredator_fechar(&j));
}
int main(void) {
  pacote(); tokens(); hashes(); limites();
  puts("farol_redator: schema, canarios, 100000 tokens, hash e limites ok");
  return 0;
}
