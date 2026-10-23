// SEEKR: leitura do /sprites e do VTT de miniaturas, e a escolha da cue.
//
// As cargas seguem o formato do SDK oficial (Vtt.kt: "INICIO --> FIM" e a URL
// da folha com "#xywh=") e a resposta de exemplo da pagina do servico (vtt_url
// relativo). Sem chave nao ha resposta real para capturar; tests/seekr_vivo.sh
// confere a API de verdade quando SEEKR_API_KEY esta no ambiente.
//
//   bash tests/seekr.sh
#include "../src/seekrvtt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *VTT =
  "WEBVTT\r\n\r\n"
  "00:00:10.000 --> 00:00:20.000\r\n"
  "https://sprites.seekr.tv/s/603/sheet_000.jpg?sig=abc#xywh=320,0,320,180\r\n\r\n"
  "00:00:00.000 --> 00:00:10.000\r\n"
  "https://sprites.seekr.tv/s/603/sheet_000.jpg?sig=abc#xywh=0,0,320,180\r\n\r\n"
  "00:00:20.000 --> 00:00:30.000\r\n"
  "/s/603/sheet_001.jpg?sig=def#xywh=0,0,320,180\r\n\r\n"
  "01:00:00.000 --> 01:00:10.500\n"
  "\n"
  "/s/603/sheet_001.jpg?sig=def#xywh=640,180,320,180\n"
  "00:00:40.000 --> 00:00:50.000\n"
  "sem fragmento\n"
  "lixo --> tambem\n";

int main(void) {
  char u[512];
  SeekrVtt v;
  int n;

  // /sprites: relativo e absoluto.
  assert(seekr_ler_lookup("{\"media_type\":\"movie\",\"tmdb_id\":12345,"
                          "\"vtt_url\":\"/sprites/12345/7200000/thumbnails.vtt?sig=x\","
                          "\"source_duration_ms\":7200000,\"scale\":1.000123}", u, sizeof u));
  assert(!strcmp(u, "https://sprites.seekr.tv/sprites/12345/7200000/thumbnails.vtt?sig=x"));
  assert(seekr_ler_lookup("{\"vtt_url\":\"https://sprites.seekr.tv/abc123.vtt?sig=xyz\"}", u, sizeof u));
  assert(!strcmp(u, "https://sprites.seekr.tv/abc123.vtt?sig=xyz"));
  assert(!seekr_ler_lookup("{\"error\":\"invalid or missing API key\"}", u, sizeof u));
  assert(!seekr_ler_lookup("{\"vtt_url\":\"javascript:x\"}", u, sizeof u));

  // URL do lookup: filme, episodio, id com sufixo, entradas ruins.
  assert(seekr_url_lookup(u, sizeof u, "tt0133093", 0, 0, 8160000));
  assert(!strcmp(u, "https://api.seekr.tv/sprites?duration_ms=8160000&imdb_id=tt0133093"));
  assert(seekr_url_lookup(u, sizeof u, "tt0903747:2:3", 2, 3, 2880000));
  assert(!strcmp(u, "https://api.seekr.tv/sprites?duration_ms=2880000&show_imdb_id=tt0903747&season=2&episode=3"));
  assert(seekr_url_lookup(u, sizeof u, "tt12409194:0:1", 0, 1, 1200000));
  assert(strstr(u, "show_imdb_id=tt12409194&season=0&episode=1"));
  assert(!seekr_url_lookup(u, sizeof u, "kitsu:1", 0, 0, 1000));
  assert(!seekr_url_lookup(u, sizeof u, "tt1", 0, 0, 0));

  // VTT: 4 cues validas, em ordem, 2 folhas (a relativa ganha o host).
  n = seekr_vtt_ler(VTT, &v);
  assert(n == 4 && v.nCues == 4);
  assert(v.nFolhas == 2);
  assert(!strcmp(v.folhas[0], "https://sprites.seekr.tv/s/603/sheet_000.jpg?sig=abc"));
  assert(!strcmp(v.folhas[1], "https://sprites.seekr.tv/s/603/sheet_001.jpg?sig=def"));
  assert(v.cues[0].ini == 0 && v.cues[0].fim == 10000 && v.cues[0].x == 0);
  assert(v.cues[1].ini == 10000 && v.cues[1].x == 320 && v.cues[1].folha == 0);
  assert(v.cues[2].ini == 20000 && v.cues[2].folha == 1);
  assert(v.cues[3].ini == 3600000 && v.cues[3].fim == 3610500);
  assert(v.cues[3].x == 640 && v.cues[3].y == 180 && v.cues[3].w == 320 && v.cues[3].h == 180);

  // Escolha: a da posicao na primeira metade, a seguinte passada a metade.
  assert(seekr_vtt_cue(&v, -5) == 0);
  assert(seekr_vtt_cue(&v, 0) == 0);
  assert(seekr_vtt_cue(&v, 4999) == 0);
  assert(seekr_vtt_cue(&v, 10000) == 1);
  assert(seekr_vtt_cue(&v, 18500) == 2);     // 18,5 s: o quadro dos 20 s
  assert(seekr_vtt_cue(&v, 25000) == 2);
  assert(seekr_vtt_cue(&v, 1000000) == 3);   // no buraco: a seguinte
  assert(seekr_vtt_cue(&v, 99999999) == 3);
  seekr_vtt_liberar(&v);
  assert(v.nCues == 0 && v.cues == NULL);

  assert(seekr_vtt_ler("WEBVTT\n\n", &v) == 0); seekr_vtt_liberar(&v);
  assert(seekr_vtt_ler(NULL, &v) == 0); seekr_vtt_cue(&v, 1); seekr_vtt_liberar(&v);
  printf("seekr: ok\n");
  return 0;
}
