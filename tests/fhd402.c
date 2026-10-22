// #402: fonte rotulada "FHD" (sem o numero 1080) caia em "Outras". O parser de
// verdade sobre uma lista de streams em JSON; altura e selo r-1080 tem de sair.
#include "streams.h"
#include "badges.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int falhas;
static void caso(const Stream *s, int altura, int r1080, const char *nome) {
  int tem = (s->badges & badges_bit("r-1080")) != 0;
  if (s->altura != altura || (r1080 >= 0 && tem != r1080)) {
    printf("FAIL %s: altura=%d (quer %d) r-1080=%d (quer %d)\n",
           nome, s->altura, altura, tem, r1080);
    falhas++;
  } else printf("ok   %s\n", nome);
}
int main(void) {
  Stream *v = NULL;
  int n = stream_extrair("{\"streams\":["
    "{\"url\":\"https://example.invalid/0\",\"name\":\"FHD | REMUX | SDR\"},"
    "{\"url\":\"https://example.invalid/1\",\"name\":\"FHD | SDR\",\"description\":\"WEBRip HEVC\"},"
    "{\"url\":\"https://example.invalid/2\",\"name\":\"Full HD BluRay\"},"
    "{\"url\":\"https://example.invalid/3\",\"name\":\"FullHD\"},"
    "{\"url\":\"https://example.invalid/4\",\"name\":\"UHD | WEB-DL\"},"
    "{\"url\":\"https://example.invalid/5\",\"name\":\"N/A | SDR\",\"description\":\"Dual Audio / JA\"},"
    "{\"url\":\"https://example.invalid/6\",\"name\":\"Movie\",\"description\":\"DTS-HD MA 5.1\"},"
    "{\"url\":\"https://example.invalid/7\",\"name\":\"XFHDX\"},"
    "{\"url\":\"https://example.invalid/8\",\"name\":\"1080p\",\"description\":\"FHD WEB-DL\"},"
    "{\"url\":\"https://example.invalid/9\",\"name\":\"2160p\",\"description\":\"FHD HDR10\"},"
    // Revisao: FHD so na falta de numero escrito, e so do name.
    "{\"url\":\"https://example.invalid/10\",\"name\":\"720p\",\"description\":\"FHD WEB-DL\"},"
    "{\"url\":\"https://example.invalid/11\",\"name\":\"480p | FHD\"},"
    "{\"url\":\"https://example.invalid/12\",\"name\":\"Movie\",\"description\":\"More information: https://fhd.example.invalid/help\"},"
    "{\"url\":\"https://example.invalid/13\",\"name\":\"Movie.WEBRip.x265-FHD.mkv\"},"
    "{\"url\":\"https://example.invalid/14\",\"name\":\"Movie\",\"behaviorHints\":{\"filename\":\"Movie.WEBRip.x265-FHD.mkv\"}},"
    "{\"url\":\"https://example.invalid/15\",\"name\":\"2160p\",\"description\":\"FHD; alternative 720p\"},"
    "{\"url\":\"https://example.invalid/16\",\"name\":\"720p | FHD\"}"
    "]}", "fixture", &v);
  if (n != 17) { printf("FAIL contagem %d\n", n); return 1; }
  caso(&v[0], 1080, 1, "FHD | REMUX | SDR");
  caso(&v[1], 1080, 1, "FHD | SDR + WEBRip HEVC");
  caso(&v[2], 1080, 1, "Full HD BluRay");
  caso(&v[3], 1080, 1, "FullHD");
  caso(&v[4], 2160, 0, "UHD | WEB-DL");
  caso(&v[5], 0, 0, "N/A | SDR + Dual Audio");
  caso(&v[6], 0, 0, "DTS-HD MA 5.1 nao vira 720");
  if (v[6].badges & badges_bit("r-720")) { puts("FAIL DTS-HD com r-720"); falhas++; }
  caso(&v[7], 0, 0, "XFHDX dentro da palavra");
  caso(&v[8], 1080, 1, "1080p + FHD");
  caso(&v[9], 2160, 0, "2160p + FHD: numero explicito vence");
  if (!(v[9].badges & badges_bit("r-4k"))) { puts("FAIL 2160p+FHD sem r-4k"); falhas++; }
  caso(&v[10], 720, 0, "720p no name + FHD na descricao");
  caso(&v[11], 480, 0, "480p | FHD no name");
  caso(&v[12], 0, 0, "fhd como host de URL");
  caso(&v[13], 0, 0, "grupo de release x265-FHD no name");
  caso(&v[14], 0, 0, "grupo de release x265-FHD no filename");
  caso(&v[15], 2160, 0, "2160p + FHD; alternative 720p sem r-1080");
  caso(&v[16], 720, 0, "720p | FHD no name: numero vence");
  free(v);
  // Revisao 2: FHD so como rotulo solto do formatador, nunca dentro de URL,
  // nome de arquivo ou frase; nome cortado no buffer e nome do provedor nao contam.
  { char json[2048], longo[260];
    memset(longo, 'X', 185); strcpy(longo + 185, " | FHDx");   // 192 bytes: rotulo corta em "| FHD"
    snprintf(json, sizeof json, "{\"streams\":["
      "{\"url\":\"https://example.invalid/a\",\"name\":\"https://fhd.example.invalid/help\"},"
      "{\"url\":\"https://example.invalid/b\",\"name\":\"Movie.WEBRip.x265.FHD.mkv\"},"
      "{\"url\":\"https://example.invalid/c\",\"name\":\"Movie.WEBRip.x265_FHD.mkv\"},"
      "{\"url\":\"https://example.invalid/d\",\"name\":\"Movie.WEBRip.x265-[FHD].mkv\"},"
      "{\"url\":\"https://example.invalid/e\",\"name\":\"The FHD Story\"},"
      "{\"url\":\"https://example.invalid/f\",\"name\":\"%s\"},"
      "{\"url\":\"https://example.invalid/g\",\"name\":\"[RD+] AIOStreams\\nFHD \xE2\x80\xA2 REMUX\"},"
      "{\"url\":\"https://example.invalid/h\",\"name\":\"Movie | FHD\"}"
      "]}", longo);
    n = stream_extrair(json, "fixture", &v);
    if (n != 8) { printf("FAIL contagem revisao 2: %d\n", n); return 1; }
    caso(&v[0], 0, 0, "name e URL com host fhd");
    caso(&v[1], 0, 0, "name x265.FHD.mkv");
    caso(&v[2], 0, 0, "name x265_FHD.mkv");
    caso(&v[3], 0, 0, "name x265-[FHD].mkv");
    caso(&v[4], 0, 0, "The FHD Story");
    caso(&v[5], 0, 0, "name cortado no buffer terminando em FHD");
    caso(&v[6], 1080, 1, "FHD no inicio da 2a linha, antes de bullet");
    caso(&v[7], 1080, 1, "Movie | FHD");
    free(v);
    n = stream_extrair("{\"streams\":[{\"url\":\"https://example.invalid/i\",\"description\":\"WEB-DL\"}]}",
                       "FHD Movies", &v);
    if (n != 1) { printf("FAIL contagem provedor: %d\n", n); return 1; }
    caso(&v[0], 0, 0, "sem name, provedor FHD Movies");
    free(v);
    // Revisao 3: o formato real do AIOStreams ("\u23f3 FHD", "\u26a1 FHD") e o rotulo
    // sozinho na 2a linha do name, sem bullet depois.
    n = stream_extrair("{\"streams\":["
      "{\"url\":\"https://example.invalid/j\",\"name\":\"\xE2\x8F\xB3 FHD\"},"
      "{\"url\":\"https://example.invalid/k\",\"name\":\"\xE2\x9A\xA1 FHD | REMUX\"},"
      "{\"url\":\"https://example.invalid/l\",\"name\":\"[RD+] AIOStreams\\nFHD\"},"
      "{\"url\":\"https://example.invalid/m\",\"name\":\"Movie\xE1\x80\xA2 FHD\"},"
      "{\"url\":\"https://example.invalid/n\",\"name\":\"\xE6\x88\x91\xE7\x9A\x84 FHD \xE6\x95\x85\xE4\xBA\x8B\"},"
      "{\"url\":\"https://example.invalid/o\",\"name\":\"\xD0\xA4\xD0\xB8\xD0\xBB\xD1\x8C\xD0\xBC FHD\"},"
      "{\"url\":\"https://example.invalid/p\",\"name\":\"\xF0\x9F\x8E\xAC FHD\"}"
      "]}", "fixture", &v);
    if (n != 7) { printf("FAIL contagem revisao 3: %d\n", n); return 1; }
    caso(&v[0], 1080, 1, "emoji + FHD (AIOStreams)");
    caso(&v[1], 1080, 1, "emoji + FHD | REMUX");
    caso(&v[2], 1080, 1, "FHD sozinho na 2a linha do name");
    caso(&v[3], 0, 0, "caractere U+1022 nao e bullet");
    caso(&v[4], 0, 0, "frase CJK antes de FHD");
    caso(&v[5], 0, 0, "palavra cirilica antes de FHD");
    caso(&v[6], 1080, 1, "emoji de 4 bytes + FHD");
    free(v);
    // Revisao 4: E2 xx xx vai ate U+2FFF, e de U+2C00 em diante ha letras
    // (Georgiano U+2D00.., Glagolitico U+2C00..); so U+2000..U+2BFF e simbolo.
    n = stream_extrair("{\"streams\":["
      "{\"url\":\"https://example.invalid/q\",\"name\":\"\xE2\xB4\x8B\xE2\xB4\x84 FHD\"},"
      "{\"url\":\"https://example.invalid/r\",\"name\":\"\xE2\xB0\x80 FHD\"},"
      "{\"url\":\"https://example.invalid/s\",\"name\":\"\xE2\xAD\x90 FHD\"},"
      "{\"url\":\"https://example.invalid/t\",\"name\":\"\xE2\xAF\xBF FHD\"}"
      "]}", "fixture", &v);
    if (n != 4) { printf("FAIL contagem revisao 4: %d\n", n); return 1; }
    caso(&v[0], 0, 0, "letras georgianas antes de FHD");
    caso(&v[1], 0, 0, "letra glagolitica antes de FHD");
    caso(&v[2], 1080, 1, "estrela U+2B50 + FHD");
    caso(&v[3], 1080, 1, "U+2BFF, ultimo simbolo aceito, + FHD");
    free(v); }
  if (falhas) { printf("FAIL #402: %d caso(s)\n", falhas); return 1; }
  puts("PASS #402: FHD/Full HD agrupam como 1080p.");
  return 0;
}
