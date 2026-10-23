// Reuse the extras fixture dependencies; no network implementation is linked.
#define main ratings_fixture_main
#define rede_baixar_com_teste ratings_fixture_get
#include "notas0.c"
#undef rede_baixar_com_teste
#undef main

static int showRequests, movieRequests, episodeRequests;
char *rede_baixar_com_teste(const char *url, int seg, const char *const *cab) {
  if (strstr(url, "/comments/likes")) {
    const char *file;
    if (strstr(url, "/seasons/")) { episodeRequests++; file = "tests/fixtures/trakt_episode_comments.json"; }
    else if (strstr(url, "/shows/tt12637874/")) { showRequests++; file = "tests/fixtures/trakt_show_comments.json"; }
    else { assert(strstr(url, "/movies/tt33612209/")); movieRequests++; file = "tests/fixtures/trakt_show_comments.json"; }
    FILE *f = fopen(file, "rb"); assert(f);
    char buf[4096]; size_t n = fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[n] = 0;
    return dupstr(buf);
  }
  return ratings_fixture_get(url, seg, cab);
}
static void waitExtras(void) {
  for (int i = 0; i < 1000 && extras_carregando(); i++) usleep(1000);
  assert(!extras_carregando());
}
int main(void) {
  extras_pedir("tt12637874:1:1", 1, 0); waitExtras();
  assert(showRequests == 1 && !movieRequests && !episodeRequests);
  assert(extras_n_comentarios() == 3);
  assert(!strcmp(extras_comentario_usuario(0), "show-reviewer"));
  assert(!strcmp(extras_comentario_texto(0), "Show fixture comment."));
  extras_pedir_comentarios_ep("tt12637874:1:1", 1, 1);
  for (int i = 0; i < 1000 && extras_comentarios_ep_carregando(); i++) usleep(1000);
  assert(!extras_comentarios_ep_carregando() && episodeRequests == 1);
  assert(extras_n_comentarios_ep() == 1 && extras_n_comentarios() == 3);
  assert(!strcmp(extras_comentario_ep_usuario(0), "episode-reviewer"));
  extras_pedir("tt33612209", 0, 0); waitExtras();
  assert(movieRequests == 1 && extras_n_comentarios() == 3);
  puts("PASS: fixture shows/movies endpoints, episode suffix, separate episode list");
  return 0;
}
