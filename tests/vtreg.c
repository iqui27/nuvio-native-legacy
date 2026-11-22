// Vai tocar nesta TV (4.2): o registro anonimo. Ver src/vtreg.h.
#include "vtreg.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static VtRegRegistro norm(VtRegEntrada e) {
  VtRegRegistro r;
  assert(vtreg_normalizar(&e, &r));
  return r;
}

int main(void) {
  char b[400], t[48];
  VtRegRegistro r;

  // Vocabulario fechado: o texto cru do player vira token.
  r = norm((VtRegEntrada){ "webos", "OLED55C9PSA", "05.30.25", "video/x-matroska", "HEVC Main 10",
                              "E-AC-3 Atmos 5.1", "DolbyVision", 23456, 2160, VTR_TOCOU, 0 });
  assert(!strcmp(r.plataforma, "webos") && !strcmp(r.modelo, "OLED55C9PSA") && !strcmp(r.firmware, "05.30.25"));
  assert(!strcmp(r.container, "mkv") && !strcmp(r.video, "hevc") && !strcmp(r.audio, "eac3") && !strcmp(r.hdr, "dv"));
  assert(r.mbps == 40 && r.altura == 2160 && r.resultado == VTR_TOCOU && !r.trocou);
  assert(vtreg_json(&r, b, sizeof b));
  assert(!strcmp(b, "{\"v\":1,\"plataforma\":\"webos\",\"modelo\":\"OLED55C9PSA\",\"firmware\":\"05.30.25\","
                    "\"container\":\"mkv\",\"video\":\"hevc\",\"audio\":\"eac3\",\"hdr\":\"dv\",\"mbps\":40,"
                    "\"altura\":2160,\"resultado\":\"tocou\",\"trocou\":0}"));

  // "ac3" nao pode engolir "eac3"; "none" e SDR; desconhecido e "?"; estranho e "outro".
  r = norm((VtRegEntrada){ "android", "Xiaomi MiTV-MOOQ1", "11", "mp4", "avc1.640028", "ac3", "none", 800, 700, VTR_CAIU, 1 });
  assert(!strcmp(r.audio, "ac3") && !strcmp(r.video, "h264") && !strcmp(r.hdr, "sdr") && r.mbps == 2 && r.altura == 720 && r.trocou == 1);
  r = norm((VtRegEntrada){ "tizen", NULL, NULL, NULL, "RealVideo 4", NULL, NULL, 0, 0, VTR_NAO_ABRIU, 0 });
  assert(!strcmp(r.video, "outro") && !strcmp(r.container, "?") && !strcmp(r.audio, "?") && !strcmp(r.modelo, "?") && r.mbps == 0 && r.altura == 0);
  // Plataforma fora da lista nao passa crua.
  r = norm((VtRegEntrada){ "http://x", "m", "f", "mkv", "hevc", "aac", "HDR10+", 1, 1, VTR_TOCOU, 0 });
  assert(!strcmp(r.plataforma, "outro") && !strcmp(r.hdr, "hdr10p"));

  // NADA de titulo, hash ou endereco: mesmo se alguem ligar o campo errado.
  {
    static const char *const RUINS[] = {
      "http://servidor.exemplo/filme.mkv", "magnet:?xt=urn:btih:abcdef", "192.168.100.24",
      "0123456789abcdef0123456789abcdef01234567", "joao@exemplo.com", "/mnt/usb/Filme (2024).mkv",
      "C:\\filmes\\a.mkv", "token=abc", "www.exemplo.com", "", NULL
    };
    int i;
    for (i = 0; i < 11; i++) {
      assert(!vtreg_texto_seguro(RUINS[i], t, sizeof t));
      assert(!strcmp(t, "?"));
      VtRegEntrada e = { "webos", RUINS[i], RUINS[i], RUINS[i], RUINS[i], RUINS[i], RUINS[i], 5000, 1080, VTR_NAO_ABRIU, 0 };
      assert(vtreg_normalizar(&e, &r) && vtreg_json(&r, b, sizeof b));
      assert(!strstr(b, "exemplo") && !strstr(b, "abcdef") && !strstr(b, "192.168") && !strstr(b, "Filme") && !strstr(b, "filmes"));
      assert(!strcmp(r.modelo, "?") && !strcmp(r.firmware, "?"));
    }
  }
  // Firmware de quatro grupos curtos NAO e IP; aspas e acento somem (JSON sem escape).
  assert(vtreg_texto_seguro("03.21.30.1", t, sizeof t) && !strcmp(t, "03.21.30.1"));
  assert(vtreg_texto_seguro("Sony \"BRAVIA\" 4K (VH2)", t, sizeof t) && !strcmp(t, "Sony BRAVIA 4K VH2"));
  { char longo[200]; memset(longo, 'Z', sizeof longo - 1); longo[sizeof longo - 1] = 0;
    assert(vtreg_texto_seguro(longo, t, sizeof t) && strlen(t) == sizeof t - 1); }

  // Classificacao da falha.
  assert(vtreg_classificar(&(VtRegSinais){ 1, 0, 0 }) == VTR_NAO_ABRIU);
  assert(vtreg_classificar(&(VtRegSinais){ 1, 1, 0 }) == VTR_CAIU);
  assert(vtreg_classificar(&(VtRegSinais){ 1, 0, 4003 }) == VTR_DECODER);
  assert(vtreg_classificar(&(VtRegSinais){ 2, 0, 0 }) == VTR_PRAZO);
  assert(vtreg_classificar(&(VtRegSinais){ 3, 1, 0 }) == VTR_TRAVOU);
  assert(vtreg_classificar(&(VtRegSinais){ 4, 1, 0 }) == VTR_LENTA);
  assert(vtreg_classificar(&(VtRegSinais){ 0, 1, 0 }) < 0);

  // Falha de rede sem formato conhecido nao diz nada do aparelho: nao sai.
  { VtRegEntrada e = { "linux", "pc", "6.8", NULL, NULL, NULL, NULL, 0, 0, VTR_PRAZO, 0 };
    assert(!vtreg_normalizar(&e, &r));
    e.resultado = VTR_NAO_ABRIU; assert(vtreg_normalizar(&e, &r));
    e.resultado = VTR_N; assert(!vtreg_normalizar(&e, &r));
    e.resultado = -1; assert(!vtreg_normalizar(&e, &r)); }

  // Dedupe: a mesma combinacao so sai uma vez; o anel esquece o mais antigo.
  { VtRegVistos v = { { 0 }, 0, 0 };
    unsigned k;
    int i;
    r = norm((VtRegEntrada){ "linux", "pc", "6.8", "mkv", "hevc", "aac", "none", 9000, 1080, VTR_TOCOU, 0 });
    k = vtreg_chave(&r);
    assert(k && vtreg_novo(&v, k) && !vtreg_novo(&v, k));
    r.resultado = VTR_CAIU;
    assert(vtreg_chave(&r) != k && vtreg_novo(&v, vtreg_chave(&r)));
    for (i = 0; i < VTR_VISTOS; i++) assert(vtreg_novo(&v, 1000u + (unsigned)i));
    assert(vtreg_novo(&v, k)); }

  // So envia com consentimento E ajuste ligado E servidor.
  assert(vtreg_pode_enviar(1, 1, 1));
  assert(!vtreg_pode_enviar(0, 1, 1) && !vtreg_pode_enviar(1, 0, 1) && !vtreg_pode_enviar(1, 1, 0));

  // JSON que nao cabe nao sai pela metade.
  assert(!vtreg_json(&r, b, 20) && !b[0]);
  assert(!strcmp(vtreg_resultado_nome(99), "outro"));
  puts("vtreg: tudo ok");
  return 0;
}
