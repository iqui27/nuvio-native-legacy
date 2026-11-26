// Selo do que esta tocando: classe de resolucao e marca de HDR medido.
#include <stdio.h>
#include "badges.h"
#include "selotoca.h"

static int falhas;
#define EQ(a, b) do { int x = (a), y = (b); if (x != y) { falhas++; printf("FALHOU %s: %d != %d (linha %d)\n", #a, x, y, __LINE__); } } while (0)

int main(void) {
  EQ(selotoca_resolucao(3840, 2160), FMT_4K);
  EQ(selotoca_resolucao(3840, 1600), FMT_4K);    // 2.39:1
  EQ(selotoca_resolucao(2880, 2160), FMT_4K);    // 4:3 em 2160p
  EQ(selotoca_resolucao(2560, 1440), -1);        // 1440p: sem selo
  EQ(selotoca_resolucao(1920, 1080), FMT_1080);
  EQ(selotoca_resolucao(1920, 800), FMT_1080);
  EQ(selotoca_resolucao(1440, 1080), FMT_1080);  // 4:3 em 1080p
  EQ(selotoca_resolucao(1280, 720), FMT_720);
  EQ(selotoca_resolucao(1280, 536), FMT_720);
  EQ(selotoca_resolucao(960, 720), FMT_720);
  EQ(selotoca_resolucao(720, 480), FMT_SD);
  EQ(selotoca_resolucao(0, 0), -1);              // ainda sem videoInfo

  EQ(selotoca_hdr("none", 0, -1), -1);
  EQ(selotoca_hdr(NULL, 0, -1), -1);
  EQ(selotoca_hdr("HDR10", 0, -1), FMT_HDR10);
  EQ(selotoca_hdr("hdr10", 0, -1), FMT_HDR10);
  EQ(selotoca_hdr("HLG", 0, -1), FMT_HLG);
  EQ(selotoca_hdr("HDR10+", 0, -1), FMT_HDR10P);
  EQ(selotoca_hdr("HDR10Plus", 0, -1), FMT_HDR10P);
  // Dolby Vision so com a confirmacao do pipeline, nunca pelo texto sozinho.
  EQ(selotoca_hdr("DolbyVision", 1, -1), FMT_DV);
  EQ(selotoca_hdr("HDR10", 1, -1), FMT_DV);
  EQ(selotoca_hdr("DolbyVision", 0, -1), -1);
  // A fonte so fala quando a plataforma nao mede.
  EQ(selotoca_hdr("desconhecido", 0, FMT_HDR10P), FMT_HDR10P);
  EQ(selotoca_hdr("desconhecido", 0, FMT_HLG), FMT_HLG);
  EQ(selotoca_hdr("desconhecido", 0, FMT_HDR), FMT_HDR);
  EQ(selotoca_hdr("desconhecido", 0, FMT_DV), -1);
  EQ(selotoca_hdr("desconhecido", 0, -1), -1);
  EQ(selotoca_hdr("none", 0, FMT_HDR10), -1);    // medido SDR vence o nome da fonte
  EQ(selotoca_hdr("HDR10", 0, FMT_HDR10P), FMT_HDR10);

  if (falhas) { printf("selotoca: %d falha(s)\n", falhas); return 1; }
  printf("selotoca: ok\n");
  return 0;
}
