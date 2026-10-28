// OpenSeek, fonte alternativa para miniaturas: corpos JSON e leitura do bloco "alt".
// C puro, sem rede nem SDL. Ver tests/openseek_alt.sh.
#include "openseekproto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

// Validador minimo: aspas, escapes e balanceamento de {} [].
static int json_ok(const char *s) {
  int pilha[64], d = 0, str = 0;
  for (; *s; s++) {
    if (str) { if (*s == '\\') { if (!s[1]) return 0; s++; } else if (*s == '"') str = 0; continue; }
    if (*s == '"') str = 1;
    else if (*s == '{' || *s == '[') { if (d >= 64) return 0; pilha[d++] = *s; }
    else if (*s == '}' || *s == ']') { if (!d || pilha[--d] != (*s == '}' ? '{' : '[')) return 0; }
  }
  return !str && d == 0;
}

static OpenSeekAltCand cand(const char *cid, int altura, const char *nome, int cache) {
  OpenSeekAltCand c;
  memset(&c, 0, sizeof c);
  snprintf(c.cid, sizeof c.cid, "%s", cid);
  snprintf(c.titleKey, sizeof c.titleKey, "tt1856101:0:0");
  c.altura = altura; c.durMs = 9760000; c.tamanho = 4000000000ULL;
  c.codec = "h264"; c.container = "mkv"; c.nome = nome; c.emCache = cache; c.resolvida = 0;
  return c;
}

int main(void) {
  char corpo[16385], base[16385], t[512], st[24], want[24];
  OpenSeekFonte f;
  memset(&f, 0, sizeof f);
  f.url = "https://cdn.example.com/f/film.mkv?sig=SEGREDO123";
  f.cabecalhos = "User-Agent: nuvio\nCookie: tem=segredo\nReferer: https://x/\n";
  f.durMs = 9760000; f.imdb = "tt1856101"; f.videoHash = ""; f.arquivo = "Film.2160p.mkv"; f.atMs = -1;

  OpenSeekAltCand play = cand("play", 2160, "Film.2160p.REMUX", 1);
  OpenSeekAltCand cs[30];
  for (int i = 0; i < 30; i++) { char id[24]; snprintf(id, sizeof id, "s%d", i); cs[i] = cand(id, 720, "Film.720p \"quoted\" \\ name", i % 2); }

  size_t nb = openseek_json_previa(base, sizeof base, &f);
  CHECK(nb > 0 && json_ok(base) && !strstr(base, "\"alt\""));

  // 1. com candidatos: JSON valido, bloco alt presente, e NENHUMA URL/segredo dentro do bloco.
  size_t n = openseek_json_previa_alt(corpo, sizeof corpo, &f, &play, cs, 3);
  CHECK(n > nb && json_ok(corpo));
  const char *alt = strstr(corpo, "\"alt\":{");
  CHECK(alt != NULL);
  if (alt) {
    CHECK(!strstr(alt, "http") && !strstr(alt, "SEGREDO") && !strstr(alt, "sig="));
    CHECK(strstr(alt, "\"telemetry\":true") && strstr(alt, "\"cid\":\"s2\""));
    CHECK(strstr(alt, "\"cached\":true") && strstr(alt, "\"resolved\":false"));
  }
  CHECK(strncmp(corpo, base, nb - 1) == 0);                 // o prefixo e o corpo de sempre

  // 2. teto de candidatos
  n = openseek_json_previa_alt(corpo, sizeof corpo, &f, &play, cs, 30);
  { int k = 0; for (const char *p = corpo; (p = strstr(p, "\"cid\":\"s")); p++) k++; CHECK(k == OS_ALT_MAX); CHECK(json_ok(corpo)); }

  // 3. sem candidatos / sem play: exatamente o corpo de sempre (compatibilidade)
  n = openseek_json_previa_alt(corpo, sizeof corpo, &f, &play, cs, 0);
  CHECK(n == nb && strcmp(corpo, base) == 0);
  n = openseek_json_previa_alt(corpo, sizeof corpo, &f, NULL, cs, 3);
  CHECK(n == nb && strcmp(corpo, base) == 0);

  // 4. buffer curto: volta ao corpo sem o bloco, ainda JSON valido
  { char pequeno[4096]; size_t m = openseek_json_previa(pequeno, sizeof pequeno, &f);
    CHECK(m > 0 && m + 400 < sizeof pequeno);
    n = openseek_json_previa_alt(pequeno, m + 60, &f, &play, cs, 24);
    CHECK(n > 0 && json_ok(pequeno) && !strstr(pequeno, "\"alt\"")); }

  // 5. alt-source: url e so as 3 cabecalhos permitidos
  n = openseek_json_alt_fonte(corpo, sizeof corpo, "s2", f.url, f.cabecalhos);
  CHECK(n > 0 && json_ok(corpo) && strstr(corpo, "\"cid\":\"s2\"") && strstr(corpo, "SEGREDO123"));
  CHECK(strstr(corpo, "User-Agent") && strstr(corpo, "Referer") && !strstr(corpo, "Cookie") && !strstr(corpo, "tem=segredo"));
  CHECK(openseek_json_alt_fonte(corpo, sizeof corpo, "s2", "ftp://x/y", "") == 0);
  CHECK(openseek_json_alt_fonte(corpo, sizeof corpo, "", f.url, "") == 0);
  CHECK(openseek_json_alt_fonte(corpo, 20, "s2", f.url, "") == 0);          // nao coube

  // 6. telemetria
  n = openseek_json_telemetria(t, sizeof t, 12500, 0, "cdn.example.com", 1);
  CHECK(n > 0 && json_ok(t) && strstr(t, "\"buffer_ms\":12500") && strstr(t, "\"rebuffering\":false") && strstr(t, "\"conns\":1"));
  n = openseek_json_telemetria(t, sizeof t, -1, 1, "", 99);
  CHECK(n > 0 && json_ok(t) && !strstr(t, "buffer_ms") && strstr(t, "\"rebuffering\":true") && strstr(t, "\"conns\":64") && !strstr(t, "host"));
  n = openseek_json_telemetria(t, sizeof t, 9999999, 0, NULL, -5);
  CHECK(strstr(t, "\"buffer_ms\":600000") && strstr(t, "\"conns\":0"));

  // 7. leitura do bloco alt da resposta de status
  const char *resp = "{\"preview_id\":\"00112233445566778899aabbccddeeff\",\"state\":\"preparing\","
                     "\"alt\":{\"state\":\"wanted\",\"want\":\"s7\",\"reason\":null},\"refresh_after_ms\":1000}";
  CHECK(openseek_ler_alt(resp, st, sizeof st, want, sizeof want) == 1 && !strcmp(st, "wanted") && !strcmp(want, "s7"));
  resp = "{\"state\":\"ready\",\"alt\":{\"state\":\"none\",\"want\":null,\"reason\":\"not_worthwhile\"}}";
  CHECK(openseek_ler_alt(resp, st, sizeof st, want, sizeof want) == 1 && !strcmp(st, "none") && want[0] == 0);
  resp = "{\"state\":\"ready\",\"progress\":{\"tiles_done\":1}}";            // servidor sem o recurso
  CHECK(openseek_ler_alt(resp, st, sizeof st, want, sizeof want) == 0 && want[0] == 0 && st[0] == 0);
  CHECK(openseek_ler_alt(NULL, st, sizeof st, want, sizeof want) == 0);
  resp = "{\"alt\":{\"want\":\"s123456789012345678901234567890\"}}";          // maior que o buffer: truncado, nunca estoura
  CHECK(openseek_ler_alt(resp, st, sizeof st, want, sizeof want) == 1 && strlen(want) < sizeof want);

  if (falhas) { printf("openseek_alt: %d falha(s)\n", falhas); return 1; }
  printf("openseek_alt: ok\n");
  return 0;
}
