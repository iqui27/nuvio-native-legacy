// LG, AUTO-PLAY: MP4 PRIMEIRO NA MESMA FAIXA DE RESOLUCAO (pedido do dono,
// 2.0.3, C9). Com auto-play ligado, HDR em "Preferir" e Dolby Vision ligado, a
// escolha automatica (e o "Melhor para esta TV") poe o MP4 na frente de MKV e
// outros DA MESMA faixa — ate de DV em MKV: MP4 DV > MP4 HDR > MP4 SDR > o
// resto pela regra de sempre. Nunca desce de resolucao por um MP4 (4K MKV
// ganha de 1080p MP4); as regras de auto-play (fonteregra) e as multas de
// cache/origem/teto continuam acima. Fora da LG, ou com qualquer das tres
// opcoes desligada, nada muda.
// Compilado duas vezes pelo .sh: LG (padrao do host) e -DNV_STREAMS_LG=0.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/streams.c"

static char dirAj[256];
// manual 1 = "Escolher a fonte ao reproduzir" (auto-play desligado);
// hdr 0 Preferir / 1 Indiferente / 2 Evitar; dv 1 = ligado.
static void cfg(int manual, int hdr, int dv) {
  char c[300]; FILE *f;
  snprintf(c, sizeof c, "%s/ajustes.txt", dirAj);
  f = fopen(c, "w"); assert(f);
  // "Dolby Vision em MKV" ligado, como na C9 do dono: e o que faz o MKV DV
  // vencer hoje (nivel 4).
  fprintf(f, "escolherFonteManual %d\nfontePrioridadeLocal 0\nfonteHdrLocal %d\ndolbyVision %d\ndvMkvLocal 0\n",
          manual ? 0 : 1, hdr, dv ? 0 : 1);
  fclose(f);
  ajustes_dir(dirAj);
  assert(ajustes_fonte_manual() == manual && ajustes_fonte_hdr() == hdr && ajustes_dolby_vision() == dv);
  assert(ajustes_dv_mkv());
}
static void fonte(Stream *s, const char *rot, int altura, int mp4, int dv, const char *selos) {
  memset(s, 0, sizeof *s);
  snprintf(s->provedor, sizeof s->provedor, "AIOStreams");
  snprintf(s->rotulo, sizeof s->rotulo, "%s", rot);
  snprintf(s->url, sizeof s->url, "https://h.invalid/v/%s.%s", rot, mp4 ? "mp4" : "mkv");
  s->altura = altura; s->mp4 = mp4; s->dolbyVision = dv; s->fileIdx = -1;
  s->badges = badges_detectar(selos);
}
static const char *vence(const Stream *l, int k) {
  int i;
  usleep(250000);   // a foto do StreamFit no automatico vale 200 ms
  stream_definir_alvo("tt6263850");
  stream_definir_lista(l, k);
  i = stream_automatico();
  assert(i >= 0);
  return stream_item(i)->rotulo;
}
static void espera(const char *quem, const char *obtido, const char *caso) {
  if (strcmp(quem, obtido)) {
    fprintf(stderr, "FALHOU (%s): venceu \"%s\", esperado \"%s\"\n", caso, obtido, quem);
    exit(1);
  }
  printf("fonte_lg_mp4: %s -> %s\n", caso, obtido);
}

int main(void) {
  Stream l[4];
  snprintf(dirAj, sizeof dirAj, "%s/nv-lgmp4-aj-%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
  { char m[300]; snprintf(m, sizeof m, "mkdir -p %s", dirAj); (void)!system(m); }
  stream_definir_tela(1, 1);   // tela HDR com Dolby Vision (C9)
  streamfit_limpar();
  cfg(0, 0, 1);

  // A lista do caso: DV em MKV 2160p (vence hoje) e um MP4 HDR 2160p.
  fonte(&l[0], "MKV DV 2160p", 2160, 0, 1, "2160p DV HDR10 BluRay REMUX");
  fonte(&l[1], "MP4 HDR 2160p", 2160, 1, 0, "2160p HDR10 WEB-DL");
#if NV_STREAMS_LG
  espera("MP4 HDR 2160p", vence(l, 2), "LG, auto-play+HDR+DV: MKV DV 2160p x MP4 HDR 2160p");
  // Dentro dos MP4 da faixa: DV > HDR > SDR.
  fonte(&l[2], "MP4 SDR 2160p", 2160, 1, 0, "2160p WEB-DL");
  fonte(&l[3], "MP4 DV 2160p", 2160, 1, 1, "2160p DV WEB-DL");
  espera("MP4 DV 2160p", vence(l, 4), "LG: MP4 DV > MP4 HDR > MP4 SDR > MKV DV");
  espera("MP4 HDR 2160p", vence(l, 3), "LG: MP4 HDR > MP4 SDR");
  // Nunca desce de resolucao: 4K MKV ganha de 1080p MP4.
  fonte(&l[0], "MKV 2160p", 2160, 0, 0, "2160p HDR10 BluRay");
  fonte(&l[1], "MP4 1080p", 1080, 1, 0, "1080p HDR10 WEB-DL");
  espera("MKV 2160p", vence(l, 2), "LG: MKV 2160p x MP4 1080p");
  // Multa vem antes: MP4 fora do cache nao passa na frente do MKV em cache.
  fonte(&l[0], "MKV DV 2160p", 2160, 0, 1, "2160p DV HDR10 BluRay REMUX");
  fonte(&l[1], "MP4 HDR 2160p", 2160, 1, 0, "2160p HDR10 WEB-DL");
  l[1].foraCache = 1;
  espera("MKV DV 2160p", vence(l, 2), "LG: MP4 fora do cache x MKV em cache");
  l[1].foraCache = 0;
  // Qualquer das tres opcoes desligada: a regra de sempre (MKV DV vence).
  cfg(1, 0, 1);
  espera("MKV DV 2160p", vence(l, 2), "LG, auto-play desligado");
  // Dolby Vision desligado: o MKV DV cai para HDR10 e o MP4 HDR ja ganharia
  // pelos +300; o caso que mostra a regra desligada e MKV HDR x MP4 SDR (com a
  // regra ligada o MP4 SDR passaria na frente).
  fonte(&l[0], "MKV HDR 2160p", 2160, 0, 0, "2160p HDR10 BluRay");
  fonte(&l[1], "MP4 SDR 2160p", 2160, 1, 0, "2160p WEB-DL");
  cfg(0, 0, 0);
  espera("MKV HDR 2160p", vence(l, 2), "LG, Dolby Vision desligado");
  cfg(1, 0, 1);
  espera("MKV HDR 2160p", vence(l, 2), "LG, auto-play desligado (MKV HDR x MP4 SDR)");
  cfg(0, 0, 1);
  espera("MP4 SDR 2160p", vence(l, 2), "LG, as tres ligadas (MKV HDR x MP4 SDR)");
  // HDR fora de "Preferir". Em "Indiferente" o MP4 da faixa ja ganha pela regra
  // de sempre (+300 sem o HDR contar), entao o caso que mostra a regra DESLIGADA
  // e "Evitar": la o MKV SDR ganha do MP4 HDR — com a regra ligada perderia.
  fonte(&l[0], "MKV SDR 2160p", 2160, 0, 0, "2160p WEB-DL");
  fonte(&l[1], "MP4 HDR 2160p", 2160, 1, 0, "2160p HDR10 WEB-DL");
  cfg(0, 2, 1);
  espera("MKV SDR 2160p", vence(l, 2), "LG, HDR Evitar");
  // PERFIL 5 EM MP4 NA LG TOCA COMO DV DE VERDADE (a TV aciona o DV nativo no
  // MP4): fora do MP4 primeiro (auto-play desligado) ele ganha de um HDR10 da
  // mesma faixa. Em MKV, ou com Dolby Vision desligado, continua abaixo do HDR10.
  cfg(1, 0, 1);
  fonte(&l[0], "MKV HDR10 2160p", 2160, 0, 0, "2160p HDR10 BluRay");
  fonte(&l[1], "MP4 DV Profile 5 2160p", 2160, 1, 1, "2160p DV WEB-DL");
  espera("MP4 DV Profile 5 2160p", vence(l, 2), "LG, sem MP4 primeiro: MP4 DV perfil 5 x MKV HDR10");
  fonte(&l[1], "MKV DV Profile 5 2160p", 2160, 0, 1, "2160p DV WEB-DL");
  espera("MKV HDR10 2160p", vence(l, 2), "LG: MKV DV perfil 5 x MKV HDR10");
  fonte(&l[1], "MP4 DV Profile 5 2160p", 2160, 1, 1, "2160p DV WEB-DL");
  cfg(1, 0, 0);
  espera("MKV HDR10 2160p", vence(l, 2), "LG, Dolby Vision desligado: MP4 DV perfil 5 x MKV HDR10");
  cfg(0, 0, 1);
  espera("MP4 DV Profile 5 2160p", vence(l, 2), "LG, com MP4 primeiro: MP4 DV perfil 5 x MKV HDR10");
#else
  // Fora da LG (Samsung/Android): nada muda, com as tres opcoes ligadas.
  espera("MKV DV 2160p", vence(l, 2), "fora da LG: MKV DV 2160p x MP4 HDR 2160p");
  fonte(&l[0], "MKV 2160p", 2160, 0, 0, "2160p HDR10 BluRay");
  fonte(&l[1], "MP4 1080p", 1080, 1, 0, "1080p HDR10 WEB-DL");
  espera("MKV 2160p", vence(l, 2), "fora da LG: MKV 2160p x MP4 1080p");
#endif
  return 0;
}
