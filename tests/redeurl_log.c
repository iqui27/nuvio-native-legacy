// rede_url_log (rede.h): URL de imagem/meta que vai para o log nunca leva a
// config/chave da pessoa. As chaves aqui sao FALSAS, de formato parecido.
#include "../src/rede.h"
#include "nvtmp.h"
#include <stdio.h>
#include <string.h>

static int falhas;
static void caso(const char *url, const char *esperado) {
  char b[160];
  rede_url_log(url, b, sizeof b);
  if (strcmp(b, esperado)) {
    printf("FALHOU: %s\n  saiu     %s\n  esperado %s\n", url, b, esperado);
    falhas++;
  }
}
static void semVazar(const char *url, const char *segredo) {
  char b[160];
  rede_url_log(url, b, sizeof b);
  if (strstr(b, segredo)) { printf("VAZOU '%s' em %s\n", segredo, b); falhas++; }
}

int main(void) {
  // btttr.cc (BetterPosters/PostersPlus): segmentos CURTOS sao a config.
  caso("https://btttr.cc/abcDE1234/pp01/Zx9yW8vU7tS6rQ/tt0111161.jpg",
       "https://btttr.cc/<redigido>/<redigido>/<redigido>/tt0111161.jpg");
  caso("https://api.btttr.cc/k3y/poster", "https://api.btttr.cc/<redigido>/<redigido>");
  caso("https://btttr.cc/soconfig", "https://btttr.cc/<redigido>");
  // AioMetadata, ElfHosted, RPDB, top-posters
  caso("https://aiometadata.elfhosted.com/stremio/0f0e0d0c-aaaa-bbbb-cccc-111122223333/meta/movie/tt1.json",
       "https://aiometadata.elfhosted.com/<redigido>/<redigido>/<redigido>/<redigido>/tt1.json");
  caso("https://meu-aiometadata.example.org/cfg/poster/tt42", "https://meu-aiometadata.example.org/<redigido>/<redigido>/tt42");
  caso("https://api.ratingposterdb.com/t0-free-rpdb/imdb/poster-default/tt0111161.jpg?fallback=true",
       "https://api.ratingposterdb.com/<redigido>/<redigido>/<redigido>/tt0111161.jpg?<redigido>");
  caso("https://api.top-streaming.stream/TP-abc/imdb/poster-default/tt5.jpg",
       "https://api.top-streaming.stream/<redigido>/<redigido>/<redigido>/tt5.jpg");
  // generico: segmento longo / jwt / uuid / config serializada / hex
  caso("https://addon.example.com/eyJhbGciOi/manifest.json",
       "https://addon.example.com/<redigido>/manifest.json");
  caso("https://addon.example.com/lang=pt|key=abc/catalog/movie/top.json",
       "https://addon.example.com/<redigido>/catalog/movie/top.json");
  caso("https://addon.example.com/0123456789abcdef0123456789abcdef/poster/tt1.jpg",
       "https://addon.example.com/<redigido>/poster/tt1.jpg");
  caso("https://addon.example.com/550e8400-e29b-41d4-a716-446655440000/x",
       "https://addon.example.com/<redigido>/x");
  caso("https://addon.example.com/cfg%7B%22k%22%7D/x", "https://addon.example.com/<redigido>/x");
  // userinfo e query
  caso("https://usuario:senha@iptv.example.com/live/a.png?token=XYZ",
       "https://iptv.example.com/live/a.png?<redigido>");
  // o que interessa para diagnostico fica: TMDB, metahub, virtual do app
  caso("https://image.tmdb.org/t/p/w500/8Gxv8gSFCU0XGDykEGv7zR1n2ua.jpg",
       "https://image.tmdb.org/t/p/w500/8Gxv8gSFCU0XGDykEGv7zR1n2ua.jpg");
  caso("https://images.metahub.space/background/medium/tt0111161/img",
       "https://images.metahub.space/background/medium/tt0111161/img");
  caso("https://nuvio.invalid/arte/tmdb/m/278/pt", "https://nuvio.invalid/arte/tmdb/m/278/pt");
  // ultimo segmento longo SEM cara de arquivo de midia: redigido
  caso("https://addon.example.com/d/AbCdEfGhIjKlMnOpQrStUvWxYz", "https://addon.example.com/d/<redigido>");
  // sem esquema: copia
  caso(nvt("/nuvio/cache/abc.img"), nvt("/nuvio/cache/abc.img"));
  caso("", "");
  // buffer pequeno: corta sem estourar
  { char b[12]; rede_url_log("https://btttr.cc/a/b/tt1.jpg", b, sizeof b);
    if (strlen(b) != 11) { printf("FALHOU corte: '%s'\n", b); falhas++; } }
  semVazar("https://btttr.cc/MINHACHAVE/pp/tt1.jpg", "MINHACHAVE");
  semVazar("https://BTTTR.CC/MINHACHAVE/tt1.jpg", "MINHACHAVE");
  semVazar("https://x.elfhosted.com/MINHACHAVE/tt1.jpg", "MINHACHAVE");
  if (falhas) { printf("redeurl_log: %d falha(s)\n", falhas); return 1; }
  puts("redeurl_log: ok");
  return 0;
}
