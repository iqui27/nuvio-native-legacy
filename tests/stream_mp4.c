// "E MP4?" TEM UMA RESPOSTA SO (stream_e_mp4). C9, 2.0.3: Deadpool & Wolverine
// pelo AIOStreams|ElfHosted 2160p WEB-DL — URL sem extensao, ".mp4" so no
// rotulo. O cartao da fonte (containerDa) dizia MP4, mas o app.c anunciava o
// contentor ao video so por s->mp4 e pela URL: video_dv_candidato dava 1, a
// tela do Dolby Vision em MKV entrou e a sonda de Matroska rodou num MP4.
// Aqui: o predicado unico, o cartao concordando com ele, e (no .sh) os tres
// pontos do app.c que anunciam o contentor usando o mesmo predicado.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/streams.c"

static void fonte(Stream *s, const char *url, const char *rotulo, int mp4) {
  memset(s, 0, sizeof *s);
  snprintf(s->url, sizeof s->url, "%s", url);
  snprintf(s->rotulo, sizeof s->rotulo, "%s", rotulo);
  s->mp4 = mp4; s->fileIdx = -1;
}

int main(void) {
  Stream s;
  // O do relato: ".mp4" so no rotulo (nome do arquivo que o addon mostra).
  fonte(&s, "https://aiostreams.invalid/api/v1/playback/abc", "Deadpool.and.Wolverine.2024.2160p.WEB-DL.DV.mp4", 0);
  assert(stream_e_mp4(&s));
  assert(!strcmp(containerDa(&s), "MP4"));
  // As outras duas formas de saber, como antes.
  fonte(&s, "https://h.invalid/v/filme.mp4?tok=1", "Filme 2160p", 0);
  assert(stream_e_mp4(&s) && !strcmp(containerDa(&s), "MP4"));
  fonte(&s, "https://h.invalid/v/abc", "Filme 2160p", 1);
  assert(stream_e_mp4(&s) && !strcmp(containerDa(&s), "MP4"));
  // MKV continua MKV.
  fonte(&s, "https://h.invalid/v/filme.mkv", "Filme.2160p.DV.mkv", 0);
  assert(!stream_e_mp4(&s) && !strcmp(containerDa(&s), "MKV"));
  fonte(&s, "https://h.invalid/v/abc", "Filme 2160p DV", 0);
  assert(!stream_e_mp4(&s));
  assert(!stream_e_mp4(NULL));
  puts("stream_mp4: .mp4 no rotulo e MP4 para o cartao e para o video");
  return 0;
}
