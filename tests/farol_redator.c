#include "farolredator.h"
#include "farolnativo.h"
#include "farolerro.h"
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
 "OLED55CANARIO1", "QN55CANARIO1", "webos-4-starfish-CANARIO21-mkv-hevc10-dv-eac3",
 "12345", "123456", "1234567", "12345678",
 "127.0.1", "10.1", "127.0.0.1", "0xc633642a", "0xc6.0x33.0x64.0x2a",
 "0300.0263.0100.0052", "3232235521", "m3_sol4_private",
 "av_CANARIO_PRIVADO", "addon_10472911", "PLAYER_ERROR_CANARIO_PRIVADO"
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
      } else if (c->tipo == FT_VERSAO)
        strcpy(valor, !strcmp(c->caminho, "aparelho.so") ? "4.9-webos" : "2.1.0-ipk");
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
  // IPv4 em hexadecimal, octal, misto e inteiro decimal: o parser WHATWG le
  // todos como IP; nem com hash podem sair.
  farolredator_hash_host("http://0xc633642a/", sal, c); assert(!strcmp(c, "x"));
  farolredator_hash_host("http://0xc6.0x33.0x64.0x2a/", sal, c); assert(!strcmp(c, "x"));
  farolredator_hash_host("http://0300.0263.0100.0052/", sal, c); assert(!strcmp(c, "x"));
  farolredator_hash_host("http://0xc6.51.100.42/", sal, c); assert(!strcmp(c, "x"));
  farolredator_hash_host("http://3232235521/", sal, c); assert(!strcmp(c, "x"));
  farolredator_hash_host("http://127.0.1/", sal, c); assert(!strcmp(c, "x"));
  // Bordas do parser WHATWG (todas leem como IPv4 em Node/Deno/browser):
  // "0x" sozinho vale 0 e zeros a esquerda nao tem teto de comprimento.
  farolredator_hash_host("http://0x/", sal, c); assert(!strcmp(c, "x"));
  farolredator_hash_host("http://0x.0x.0x.0x1/", sal, c); assert(!strcmp(c, "x"));
  farolredator_hash_host("http://0x000c633642a/", sal, c); assert(!strcmp(c, "x"));
  farolredator_hash_host("http://0x00000000c633642a/", sal, c); assert(!strcmp(c, "x"));
  farolredator_hash_host("http://000000000000000000000000000000000001.0x1/", sal, c); assert(!strcmp(c, "x"));
  farolredator_hash_host("http://0x000000000000000000000000000000000000000001/", sal, c); assert(!strcmp(c, "x"));
  // Termina em numero mas falha o parse: host invalido, nao dominio — sai "x".
  farolredator_hash_host("http://1.2.3.4.5/", sal, c); assert(!strcmp(c, "x"));
  farolredator_hash_host("http://0x100000000/", sal, c); assert(!strcmp(c, "x"));
  farolredator_hash_host("http://nas.2015/", sal, c); assert(!strcmp(c, "x"));
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
// Abre o objeto pai do caminho e escreve o campo; devolve o rc do campo.
static int escrever_em(FarolJson *j, const char *pai, const char *chave,
                       FarolTipo t, const void *v) {
  int rc;
  assert(farolredator_abrir(j, j->buf, j->tam));
  if (pai) assert(farolredator_objeto(j, pai));
  rc = farolredator_campo(j, chave, t, v);
  while (j->prof) assert(farolredator_fechar(j));
  return rc;
}
static void colisoes(void) {
  FarolJson j; char buf[4096];
  j.buf = buf; j.tam = sizeof buf;
  // PIN de 4-8 digitos (admin.h) nunca cabe em cliente.commit.
  const char *pins[] = {"1234", "12345", "123456", "1234567", "12345678"};
  for (size_t i = 0; i < sizeof pins / sizeof *pins; i++) {
    memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
    assert(!escrever_em(&j, "cliente", "commit", FT_HASH, pins[i]) && j.campos_recusados == 1);
    assert(!strstr(buf, pins[i]));
  }
  // Nem o commit do build nem um SHA com letra colidem; o todo numerico e
  // recusado porque um parser nao o distingue de PIN.
  memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
  assert(escrever_em(&j, "cliente", "commit", FT_HASH, "c18c21756"));
  memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
  assert(!escrever_em(&j, "cliente", "commit", FT_HASH, "123456789"));
  // IPv4 que um parser de URL resolve (abreviado, hex, octal, inteiro) nao
  // entra em nenhum campo de versao.
  const char *ips[] = {"127.0.1", "10.1", "127.0.0.1", "0xc633642a",
    "0xc6.0x33.0x64.0x2a", "0300.0263.0100.0052", "3232235521"};
  const char *campos[][2] = {{"aparelho", "so"}, {"cliente", "versao"}};
  for (size_t k = 0; k < sizeof campos / sizeof *campos; k++)
    for (size_t i = 0; i < sizeof ips / sizeof *ips; i++) {
      memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
      assert(!escrever_em(&j, campos[k][0], campos[k][1], FT_VERSAO, ips[i]) &&
             j.campos_recusados == 1);
      assert(!strstr(buf, ips[i]));
    }
  // A forma nao ambigua: numero com rotulo de origem.
  memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
  assert(escrever_em(&j, "aparelho", "so", FT_VERSAO, "4.9-webos"));
  memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
  assert(escrever_em(&j, "cliente", "versao", FT_VERSAO, "2.1.0-ipk"));
  memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
  assert(escrever_em(&j, "cliente", "versao", FT_VERSAO, "2.1.0-tpk65"));
  memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
  assert(escrever_em(&j, "cliente", "versao", FT_VERSAO, "14-apk"));
  // O rotulo e de lista fechada: PIN ("4.9-7362194") e titulo
  // ("2.1.0-segreditulo") nao viram sufixo de versao.
  const char *sufs[] = {"4.9-7362194", "2.1.0-segreditulo", "2.1.0-1234",
    "1.0-webosx", "4.9-", "2.1.0-CANARIO", "6.5-tpk99"};
  for (size_t k = 0; k < sizeof campos / sizeof *campos; k++)
    for (size_t i = 0; i < sizeof sufs / sizeof *sufs; i++) {
      memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
      assert(!escrever_em(&j, campos[k][0], campos[k][1], FT_VERSAO, sufs[i]) &&
             j.campos_recusados == 1);
      assert(!strstr(buf, sufs[i]));
    }
}
static void codigos(void) {
  FarolJson j; char buf[4096], fp[160];
  // Todo codigo que farolerro_ler (farol-3) emite entra em erro.nativo e no
  // terceiro componente do fingerprint — os dois leem a mesma gramatica.
  // A lista fixa e a do produtor (farolerro_codigos, exportada por
  // farolerro.h); a esperada fica copiada AQUI, nome a nome, para que uma
  // remocao na fonte quebre este teste em vez de iterar a lista alterada.
  static const char *const esperados[] = {
    "ums_pipeline", "ums_faixa_audio", "ls2_permissao",
    "m3_parado", "m3_audio", "av_conexao",
    "tpk_conexao", "tpk_audio", "tpk_conflito", "tpk_preso",
    "ums_rede", "m3_rede", "gen_rede",
    "ums_http", "av_http", "tpk_http", "m3_http", "gen_http",
    "ums_formato", "av_formato", "tpk_formato", "m3_formato", "gen_formato",
    "ums_outro", "av_outro", "tpk_outro", "m3_outro", "gen_outro",
  };
  const char *todos[] = {
    "ums_100", "ums_200", "ums_204", "ums_210", "ums_300", "ums_40000",
    "ums_40403", "ums_49999", "m3_2000", "m3_2004", "m3_2008", "m3_3000",
    "m3_4003", "m3_4999",
    // os 11 nomes do enum AVPlayError documentado pela Samsung
    "av_none", "av_invalid_parameter", "av_no_such_file",
    "av_invalid_operation", "av_seek_failed", "av_invalid_state",
    "av_not_supported_file", "av_not_supported_format", "av_invalid_uri",
    "av_connection_failed", "av_genereic",
  };
  assert(farolerro_codigos_n == (int)(sizeof esperados / sizeof *esperados));
  for (int i = 0; i < farolerro_codigos_n; i++) {
    assert(!strcmp(farolerro_codigos[i], esperados[i]));
    assert(farolnativo_cod_ok(esperados[i]));
  }
  for (size_t i = 0; i < sizeof todos / sizeof *todos; i++)
    assert(farolnativo_cod_ok(todos[i]));
  const char *fora[] = {"ums_99", "ums_301", "ums_39999", "ums_50000",
    "m3_1999", "m3_2003", "m3_2007", "m3_5000", "m3_sol4", "gen_xyz",
    "ls2_outro", "av_", "av_123456789012345678901",
    // texto livre depois de "player_error" e nome fora do enum nao entram
    "av_canario_privado", "av_fatal2", "av_none_x", "av_connection_failed2"};
  for (size_t i = 0; i < sizeof fora / sizeof *fora; i++)
    assert(!farolnativo_cod_ok(fora[i]));
  for (size_t i = 0; i < sizeof esperados / sizeof *esperados; i++) {
    memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
    assert(escrever_em(&j, "erro", "nativo", FT_TOKEN, esperados[i]));
    snprintf(fp, sizeof fp, "webos-4-starfish-%s-mkv-hevc10-dv-eac3",
             esperados[i]);
    memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
    assert(escrever_em(&j, "erro", "fingerprint", FT_TOKEN, fp));
  }
  for (size_t i = 0; i < sizeof todos / sizeof *todos; i++) {
    memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
    assert(escrever_em(&j, "erro", "nativo", FT_TOKEN, todos[i]));
    snprintf(fp, sizeof fp, "webos-4-starfish-%s-mkv-hevc10-dv-eac3", todos[i]);
    memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
    assert(escrever_em(&j, "erro", "fingerprint", FT_TOKEN, fp));
  }
  // Token composto do produtor de fonte e o pseudonimo de add-on COM sal —
  // h:<8 hex> de farolredator_hash_host, o mesmo do host. O addon_<8 hex>
  // sem sal do produtor antigo e recusado.
  const char *ks[][2] = {{"1080p_sdr_nodv_outro", "1"}, {"h:9c1e77ab", "1"},
                         {"addon_9c1e77ab", "0"}, {"addon_160c7391", "0"},
                         {"addon_canario00", "0"}, {"1080p_sol4_nodv_outro", "0"}};
  for (size_t k = 0; k < sizeof ks / sizeof *ks; k++) {
    int zero = 0;
    memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
    assert(farolredator_abrir(&j, buf, sizeof buf));
    assert(farolredator_array(&j, "linha"));
    assert(farolredator_objeto(&j, NULL));
    assert(farolredator_campo(&j, "t", FT_INT, &zero));
    assert(farolredator_campo(&j, "m", FT_ENUM, "fonte"));
    assert(farolredator_campo(&j, "e", FT_ENUM, "fonte_escolhida"));
    assert(!!farolredator_campo(&j, "k", FT_TOKEN, ks[k][0]) == (ks[k][1][0] == '1'));
    while (j.prof) assert(farolredator_fechar(&j));
    assert(!strstr(buf, "canario") && !strstr(buf, "sol4"));
  }
}
// A mensagem do AVPlay pode embutir texto livre — a sonda da revisao usou uma
// URL com PLAYER_ERROR_CANARIO_PRIVADO. O classificador (farolerro_ler,
// farol-3) extrai o nome cru; o redator so deixa sair os nomes do enum.
static void avplay(void) {
  FarolJson j; char buf[1024], fp[160];
  FarolErro erro;
  assert(farolerro_ler(FP_TIZEN_WGT,
    "prepare: https://example.invalid/PLAYER_ERROR_CANARIO_PRIVADO?chave=test",
    0, &erro));
  assert(!strcmp(erro.cod, "av_canario_privado"));
  j.buf = buf; j.tam = sizeof buf;
  assert(!escrever_em(&j, "erro", "nativo", FT_TOKEN, erro.cod));
  assert(j.campos_recusados == 1 && !strstr(buf, "canario"));
  snprintf(fp, sizeof fp, "tizen_wgt-4-avplay-%s-mkv-hevc10-dv-eac3", erro.cod);
  memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
  assert(!escrever_em(&j, "erro", "fingerprint", FT_TOKEN, fp));
  assert(j.campos_recusados == 1 && !strstr(buf, "canario"));
  // Nome real do enum segue entrando nos dois campos.
  assert(farolerro_ler(FP_TIZEN_WGT, "PLAYER_ERROR_INVALID_STATE", 0, &erro));
  assert(!strcmp(erro.cod, "av_invalid_state"));
  memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
  assert(escrever_em(&j, "erro", "nativo", FT_TOKEN, erro.cod));
  snprintf(fp, sizeof fp, "tizen_wgt-4-avplay-%s-mkv-hevc10-dv-eac3", erro.cod);
  memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
  assert(escrever_em(&j, "erro", "fingerprint", FT_TOKEN, fp));
}
// Os valores fixos de linha[].k que os produtores reais gravam no anel
// (grep de farol_ev nas branches farol-10 e farol-7): FE_ADDON_BUSCA com
// "busca" (addons.c), FE_FONTE_PROXIMA com "proxima" (streams.c),
// FE_MEM_BAIXA com "mem_baixa" (memlog.c) e FE_ABRIR com "mpv"
// (player.c sob NV_DESKTOP_MPV). Os quatro saem com "k" no JSON; um token
// valido fora da lista fechada segue recusado.
static void linha_k(void) {
  FarolJson j; char buf[2048], esperado[64]; int i;
  const char *casos[][3] = {
    {"addon", "addon_busca", "busca"},
    {"fonte", "fonte_proxima", "proxima"},
    {"memoria", "mem_baixa", "mem_baixa"},
    {"player", "abrir", "mpv"}};
  for (size_t k = 0; k < sizeof casos / sizeof *casos; k++) {
    memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
    assert(farolredator_abrir(&j, buf, sizeof buf));
    assert(farolredator_array(&j, "linha"));
    assert(farolredator_objeto(&j, NULL));
    i = 0; assert(farolredator_campo(&j, "t", FT_INT, &i));
    assert(farolredator_campo(&j, "m", FT_ENUM, casos[k][0]));
    assert(farolredator_campo(&j, "e", FT_ENUM, casos[k][1]));
    i = 1; assert(farolredator_campo(&j, "a", FT_INT, &i));
    assert(farolredator_campo(&j, "k", FT_TOKEN, casos[k][2]));
    while (j.prof) assert(farolredator_fechar(&j));
    assert(j.campos_recusados == 0);
    snprintf(esperado, sizeof esperado, "\"k\":\"%s\"", casos[k][2]);
    assert(strstr(buf, esperado));
    json_valido(buf); seguro(buf);
  }
  // Token com alfabeto valido mas fora da lista: recusado, nao vira "x".
  memset(&j, 0, sizeof j); j.buf = buf; j.tam = sizeof buf;
  assert(farolredator_abrir(&j, buf, sizeof buf));
  assert(farolredator_array(&j, "linha"));
  assert(farolredator_objeto(&j, NULL));
  i = 0; assert(farolredator_campo(&j, "t", FT_INT, &i));
  assert(farolredator_campo(&j, "m", FT_ENUM, "addon"));
  assert(farolredator_campo(&j, "e", FT_ENUM, "addon_busca"));
  assert(!farolredator_campo(&j, "k", FT_TOKEN, "titulo_livre"));
  while (j.prof) assert(farolredator_fechar(&j));
  assert(j.campos_recusados == 1 && !strstr(buf, "titulo_livre"));
}
static void pacote_dourado(void) {
  FarolJson j; char buf[FAROL_JSON_N], esperado[FAROL_JSON_N + 16];
  int i; double d; FILE *f; size_t n;
  // O exemplo do PLANO 2.6 escrito pela API, byte a byte igual a
  // tests/farol_registro_v1.json — a fixture que o farol-5 valida.
  assert(farolredator_abrir(&j, buf, sizeof buf));
  i = 1; assert(farolredator_campo(&j, "v", FT_INT, &i));
  assert(farolredator_campo(&j, "gatilho", FT_ENUM, "falha_sem_conserto"));
  assert(farolredator_campo(&j, "sintoma", FT_ENUM, "nao_comeca"));
  assert(farolredator_objeto(&j, "erro"));
  assert(farolredator_campo(&j, "codigo", FT_ENUM, "TV_FORMATO_REPETE"));
  assert(farolredator_campo(&j, "estagio", FT_ENUM, "tv"));
  assert(farolredator_campo(&j, "nativo", FT_TOKEN, "ums_204"));
  i = 204; assert(farolredator_campo(&j, "nativo_num", FT_INT, &i));
  i = 0; assert(farolredator_campo(&j, "http", FT_INT, &i));
  assert(farolredator_campo(&j, "fingerprint", FT_TOKEN,
         "webos-4-starfish-ums_204-mkv-hevc10-dv-eac3"));
  assert(farolredator_campo(&j, "fp", FT_HASH, "9c1e77ab03f2d4e1"));
  assert(farolredator_array(&j, "tentou"));
  assert(farolredator_objeto(&j, NULL));
  assert(farolredator_campo(&j, "acao", FT_ENUM, "proxima_fonte"));
  i = 0; assert(farolredator_campo(&j, "ok", FT_BOOL, &i));
  i = 6400; assert(farolredator_campo(&j, "ms", FT_INT, &i));
  assert(farolredator_fechar(&j));
  assert(farolredator_objeto(&j, NULL));
  assert(farolredator_campo(&j, "acao", FT_ENUM, "reconexao"));
  i = 0; assert(farolredator_campo(&j, "ok", FT_BOOL, &i));
  i = 13000; assert(farolredator_campo(&j, "ms", FT_INT, &i));
  assert(farolredator_fechar(&j));
  assert(farolredator_fechar(&j));
  assert(farolredator_fechar(&j));
  assert(farolredator_objeto(&j, "aparelho"));
  assert(farolredator_campo(&j, "plataforma", FT_ENUM, "webos"));
  assert(farolredator_campo(&j, "so", FT_VERSAO, "4.9-webos"));
  i = 4; assert(farolredator_campo(&j, "so_maior", FT_INT, &i));
  assert(farolredator_campo(&j, "modelo", FT_TOKEN, "OLED55C9PSA"));
  i = 1870; assert(farolredator_campo(&j, "ram_total_mb", FT_INT, &i));
  i = 212; assert(farolredator_campo(&j, "ram_livre_mb", FT_INT, &i));
  assert(farolredator_objeto(&j, "tela"));
  i = 3840; assert(farolredator_campo(&j, "w", FT_INT, &i));
  i = 2160; assert(farolredator_campo(&j, "h", FT_INT, &i));
  i = 1; assert(farolredator_campo(&j, "hdr", FT_INT, &i));
  assert(farolredator_campo(&j, "dv", FT_INT, &i));
  assert(farolredator_campo(&j, "hevc", FT_INT, &i));
  i = -1; assert(farolredator_campo(&j, "av1", FT_INT, &i));
  assert(farolredator_fechar(&j));
  assert(farolredator_objeto(&j, "audio"));
  i = 1; assert(farolredator_campo(&j, "dts", FT_INT, &i));
  i = -1; assert(farolredator_campo(&j, "truehd", FT_INT, &i));
  i = 1; assert(farolredator_campo(&j, "eac3", FT_INT, &i));
  assert(farolredator_campo(&j, "ac3", FT_INT, &i));
  assert(farolredator_fechar(&j));
  assert(farolredator_objeto(&j, "rede"));
  i = 0; assert(farolredator_campo(&j, "offline", FT_BOOL, &i));
  assert(farolredator_campo(&j, "hosts_com_falha", FT_INT, &i));
  d = 0; assert(farolredator_campo(&j, "vazao_mbps", FT_NUM1, &d));
  assert(farolredator_fechar(&j));
  assert(farolredator_fechar(&j));
  assert(farolredator_objeto(&j, "cliente"));
  assert(farolredator_campo(&j, "versao", FT_VERSAO, "2.1.0-ipk"));
  assert(farolredator_campo(&j, "commit", FT_HASH, "c18c21756"));
  assert(farolredator_campo(&j, "build", FT_ENUM, "webos-ipk"));
  assert(farolredator_campo(&j, "backend", FT_ENUM, "starfish"));
  assert(farolredator_campo(&j, "idioma", FT_ENUM, "pt-BR"));
  i = 1842; assert(farolredator_campo(&j, "aberto_s", FT_INT, &i));
  assert(farolredator_campo(&j, "gpu", FT_ENUM, "auto:alto"));
  assert(farolredator_objeto(&j, "ajustes"));
  i = 1; assert(farolredator_campo(&j, "glass_ui", FT_INT, &i));
  assert(farolredator_campo(&j, "qualidade_max", FT_ENUM, "4k"));
  d = 40; assert(farolredator_campo(&j, "tamanho_max_gb", FT_NUM1, &d));
  assert(farolredator_campo(&j, "dts", FT_ENUM, "ac3"));
  i = 0; assert(farolredator_campo(&j, "dv_mkv", FT_INT, &i));
  i = 1; assert(farolredator_campo(&j, "legenda_auto_sync", FT_INT, &i));
  assert(farolredator_fechar(&j));
  assert(farolredator_objeto(&j, "addons"));
  i = 12; assert(farolredator_campo(&j, "total", FT_INT, &i));
  i = 11; assert(farolredator_campo(&j, "ativos", FT_INT, &i));
  i = 1; assert(farolredator_campo(&j, "isolados", FT_INT, &i));
  assert(farolredator_array(&j, "tipos"));
  assert(farolredator_campo(&j, NULL, FT_ENUM, "catalogo"));
  assert(farolredator_campo(&j, NULL, FT_ENUM, "fontes"));
  assert(farolredator_campo(&j, NULL, FT_ENUM, "legendas"));
  assert(farolredator_fechar(&j));
  assert(farolredator_fechar(&j));
  assert(farolredator_array(&j, "debrid"));
  assert(farolredator_campo(&j, NULL, FT_ENUM, "torbox"));
  assert(farolredator_fechar(&j));
  assert(farolredator_array(&j, "contas"));
  assert(farolredator_campo(&j, NULL, FT_ENUM, "trakt"));
  assert(farolredator_campo(&j, NULL, FT_ENUM, "simkl"));
  assert(farolredator_fechar(&j));
  assert(farolredator_fechar(&j));
  assert(farolredator_objeto(&j, "midia"));
  assert(farolredator_campo(&j, "container", FT_ENUM, "mkv"));
  assert(farolredator_campo(&j, "video", FT_ENUM, "hevc10"));
  assert(farolredator_campo(&j, "hdr", FT_ENUM, "dv"));
  assert(farolredator_campo(&j, "res", FT_ENUM, "2160p"));
  d = 23.976; assert(farolredator_campo(&j, "fps", FT_NUM1, &d));
  d = 24.6; assert(farolredator_campo(&j, "mbps", FT_NUM1, &d));
  d = 14.2; assert(farolredator_campo(&j, "gb", FT_NUM1, &d));
  assert(farolredator_campo(&j, "audio", FT_ENUM, "eac3_51"));
  assert(farolredator_campo(&j, "legenda", FT_ENUM, "srt_embutida"));
  assert(farolredator_campo(&j, "entrega", FT_ENUM, "debrid_cache"));
  i = 206; assert(farolredator_campo(&j, "http", FT_INT, &i));
  i = 210; assert(farolredator_campo(&j, "ttfb_ms", FT_INT, &i));
  assert(farolredator_fechar(&j));
  assert(farolredator_array(&j, "cadeia"));
  const char *cadeia[][3] = {
    {"internet", "ok", NULL}, {"addons", "ok", "1920"}, {"debrid", "ok", NULL},
    {"fonte", "ok", "206"}, {"tv", "falhou", NULL}, {"audio", "nao_testado", NULL}};
  for (size_t k = 0; k < sizeof cadeia / sizeof *cadeia; k++) {
    assert(farolredator_objeto(&j, NULL));
    assert(farolredator_campo(&j, "passo", FT_ENUM, cadeia[k][0]));
    assert(farolredator_campo(&j, "estado", FT_ENUM, cadeia[k][1]));
    if (cadeia[k][2]) {
      i = atoi(cadeia[k][2]);
      assert(farolredator_campo(&j, !strcmp(cadeia[k][0], "fonte") ? "http" : "ms",
             FT_INT, &i));
    }
    assert(farolredator_fechar(&j));
  }
  assert(farolredator_fechar(&j));
  assert(farolredator_array(&j, "linha"));
  const char *linha[][5] = {
    {"-4820", "fonte", "escolhida", "2160p_hdr_dv_eac3", "9120,142"},
    {"-3610", "player", "abrir", "starfish", "1,3"},
    {"-1950", "video", "erro_nativo", "ums_204", "204,0"}};
  for (size_t k = 0; k < sizeof linha / sizeof *linha; k++) {
    assert(farolredator_objeto(&j, NULL));
    i = atoi(linha[k][0]); assert(farolredator_campo(&j, "t", FT_INT, &i));
    assert(farolredator_campo(&j, "m", FT_ENUM, linha[k][1]));
    assert(farolredator_campo(&j, "e", FT_ENUM, linha[k][2]));
    i = atoi(linha[k][4]); assert(farolredator_campo(&j, "a", FT_INT, &i));
    i = atoi(strchr(linha[k][4], ',') + 1);
    assert(farolredator_campo(&j, "b", FT_INT, &i));
    assert(farolredator_campo(&j, "k", FT_TOKEN, linha[k][3]));
    assert(farolredator_fechar(&j));
  }
  assert(farolredator_fechar(&j));
  assert(farolredator_objeto(&j, "privacidade"));
  i = 1; assert(farolredator_campo(&j, "redator", FT_INT, &i));
  i = 0; assert(farolredator_campo(&j, "campos_recusados", FT_INT, &i));
  assert(farolredator_fechar(&j));
  assert(farolredator_fechar(&j));
  json_valido(buf); seguro(buf);
  f = fopen("tests/farol_registro_v1.json", "rb");
  assert(f);
  n = fread(esperado, 1, sizeof esperado - 1, f);
  fclose(f);
  esperado[n] = 0;
  while (n && (esperado[n-1] == '\n' || esperado[n-1] == '\r')) esperado[--n] = 0;
  assert(!strcmp(buf, esperado));
}
int main(void) {
  pacote(); tokens(); hashes(); colisoes(); codigos(); avplay(); linha_k(); pacote_dourado(); limites();
  puts("farol_redator: schema, canarios, tokens, hash, colisoes, codigos, avplay, linha_k, dourado e limites ok");
  return 0;
}
