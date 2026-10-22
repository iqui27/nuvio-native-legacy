// QUE CONTENTOR A SONDA DO MKV LEU (mkv.h, MKV_CONT_*). C9, 2.0.3: um MP4 sem
// extensao na URL passou pela sonda de Matroska, que via "nao e EBML" e "a rede
// falhou" do mesmo jeito (0 faixas) e fazia 3 novas tentativas. Aqui:
//   * o classificador puro sobre bytes: EBML, MP4 (ftyp/moov/... no byte 4),
//     MPEG-TS (0x47 a cada 188), M2TS, texto (HTML/JSON de erro), nada;
//   * a sonda de verdade (mkv_faixas_e_caps + rede.c) contra arquivos reais
//     gerados pelo ffmpeg num servidor local, e contra uma porta sem ninguem.
// Uso: mkv_contentor <url-base> <url-morta>
#include "mkv.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int falhas;
static void ok(int c, const char *m) { printf("%s %s\n", c ? "ok  " : "FALHOU", m); if (!c) falhas++; }

static int sonda(const char *base, const char *arq) {
  char url[512]; MkvFaixa fx[MKV_MAX_FAIXAS]; MkvCap caps[4]; int nc = 0, n;
  snprintf(url, sizeof url, "%s/%s", base, arq);
  n = mkv_faixas_e_caps(url, fx, MKV_MAX_FAIXAS, caps, 4, &nc);
  printf("  %s: %d faixa(s), contentor %s\n", arq, n, mkv_contentor_nome(mkv_ultimo_contentor()));
  return n;
}

int main(int argc, char **argv) {
  unsigned char b[400];
  if (argc < 3) { fprintf(stderr, "uso: %s url-base url-morta\n", argv[0]); return 2; }

  printf("[1] classificador puro\n");
  memset(b, 0, sizeof b);
  b[0] = 0x1A; b[1] = 0x45; b[2] = 0xDF; b[3] = 0xA3;
  ok(mkv_contentor(b, sizeof b) == MKV_CONT_MKV, "EBML 1A 45 DF A3 -> MKV");
  memset(b, 0, sizeof b); memcpy(b + 4, "ftyp", 4);
  ok(mkv_contentor(b, sizeof b) == MKV_CONT_MP4, "ftyp no byte 4 -> MP4");
  memset(b, 0, sizeof b); memcpy(b + 4, "moov", 4);
  ok(mkv_contentor(b, sizeof b) == MKV_CONT_MP4, "moov no byte 4 -> MP4");
  memset(b, 0xFF, sizeof b); b[0] = 0x47; b[188] = 0x47;
  ok(mkv_contentor(b, sizeof b) == MKV_CONT_TS, "0x47 em 0 e 188 -> MPEG-TS");
  memset(b, 0xFF, sizeof b); b[4] = 0x47; b[196] = 0x47;
  ok(mkv_contentor(b, sizeof b) == MKV_CONT_TS, "0x47 em 4 e 196 (M2TS) -> MPEG-TS");
  memset(b, 0, sizeof b); memcpy(b, "\n  <!DOCTYPE html><html>", 24);
  ok(mkv_contentor(b, sizeof b) == MKV_CONT_TEXTO, "HTML de erro -> texto (vale nova tentativa)");
  memset(b, 0, sizeof b); memcpy(b, "{\"error\":\"x\"}", 13);
  ok(mkv_contentor(b, 13) == MKV_CONT_TEXTO, "JSON de erro -> texto");
  ok(mkv_contentor(NULL, 0) == MKV_CONT_NADA, "nada lido -> nada (falha de rede)");
  ok(mkv_contentor(b, 3) == MKV_CONT_NADA, "3 bytes -> nada");
  memset(b, 0x11, sizeof b);
  ok(mkv_contentor(b, sizeof b) == MKV_CONT_OUTRO, "binario desconhecido -> outro");
  ok(mkv_contentor_definitivo(MKV_CONT_MP4) && mkv_contentor_definitivo(MKV_CONT_TS) &&
     mkv_contentor_definitivo(MKV_CONT_OUTRO), "MP4, TS e outro binario sao definitivos");
  ok(!mkv_contentor_definitivo(MKV_CONT_NADA) && !mkv_contentor_definitivo(MKV_CONT_TEXTO) &&
     !mkv_contentor_definitivo(MKV_CONT_MKV), "nada, texto e MKV nao sao");

  printf("[2] a sonda de verdade, pela rede local\n");
  ok(sonda(argv[1], "a.mkv") > 0 && mkv_ultimo_contentor() == MKV_CONT_MKV, "MKV real: faixas lidas, contentor MKV");
  ok(sonda(argv[1], "a.mp4") == 0 && mkv_ultimo_contentor() == MKV_CONT_MP4, "MP4 real (ffmpeg): 0 faixas, contentor MP4");
  ok(sonda(argv[1], "a.ts") == 0 && mkv_ultimo_contentor() == MKV_CONT_TS, "MPEG-TS real (ffmpeg): 0 faixas, contentor TS");
  { char url[512]; MkvFaixa fx[4];
    snprintf(url, sizeof url, "%s/x", argv[2]);
    ok(mkv_faixas_e_caps(url, fx, 4, NULL, 0, NULL) == 0 && mkv_ultimo_contentor() == MKV_CONT_NADA,
       "porta sem ninguem: 0 faixas, contentor nada (falha de rede)"); }
  if (falhas) { printf("mkv_contentor: %d falha(s)\n", falhas); return 1; }
  puts("mkv_contentor: ok");
  return 0;
}
