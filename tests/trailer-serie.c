// #123: serie sem trailer. A ficha /tv/<id> nao pedia `videos`, o parse
// pulava serie, e com language=pt-BR sem include_video_language o TMDB so
// devolve video em portugues — trailer de serie costuma ser en ou sem idioma.
#include "../src/extras.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void extras_teste_url_ficha(char *dst, unsigned cap, int serie, long id,
                            const char *idioma, int trailers);
int extras_teste_trailers_ficha(const char *corpo, int serie, const char *idioma,
                                char yt[][16], int max);
int extras_teste_hero_serie(const char *corpo, const char *idioma, char *dst, unsigned cap);
void extras_teste_url_hero(char *dst, unsigned cap, int serie, long id, const char *idioma);

static int falhas;
static void check(const char *nome, int ok) {
  printf("%s %s\n", ok ? "ok" : "FALHOU:", nome);
  if (!ok) falhas++;
}

static char *ler(const char *c) {
  FILE *f = fopen(c, "rb");
  long n;
  char *s;
  if (!f) { perror(c); exit(2); }
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
  s = malloc((size_t)n + 1);
  if (fread(s, 1, (size_t)n, f) != (size_t)n) exit(2);
  s[n] = 0; fclose(f);
  return s;
}

int main(void) {
  char url[512], yt[EX_TRAILER_MAX][16];
  char *corpo = ler("tests/fixtures/tmdb_tv_videos_ptbr.json");
  int n, i, temEp = 0;

  extras_teste_url_ficha(url, sizeof url, 1, 1399, "pt-BR", 1);
  check("ficha de serie pede videos", strstr(url, "append_to_response=") &&
                                      strstr(url, "videos") != NULL);
  check("ficha de serie pede pt,en,null",
        strstr(url, "include_video_language=pt,en,null") != NULL);
  extras_teste_url_ficha(url, sizeof url, 0, 603, "pt-BR", 1);
  check("ficha de filme pede pt,en,null",
        strstr(url, "videos") && strstr(url, "include_video_language=pt,en,null"));
  extras_teste_url_ficha(url, sizeof url, 0, 603, "en-US", 1);
  check("idioma en nao repete en", strstr(url, "include_video_language=en,null") != NULL);
  extras_teste_url_ficha(url, sizeof url, 1, 1399, "pt-BR", 0);
  check("trailers desligado nao pede videos",
        !strstr(url, "videos") && !strstr(url, "include_video_language"));

  n = extras_teste_trailers_ficha(corpo, 1, "pt-BR", yt, EX_TRAILER_MAX);
  printf("   serie: %d trailer(s):", n);
  for (i = 0; i < n; i++) printf(" %s", yt[i]);
  printf("\n");
  check("serie com videos en/null tem trailer", n > 0);
  check("primeiro e o da temporada mais recente",
        n > 0 && !strcmp(yt[0], "s3TrailerEN"));
  for (i = 0; i < n; i++)
    if (!strcmp(yt[i], "ep4Preview1")) temEp = 1;
  check("promo de episodio fica de fora", !temEp);
  check("so YouTube Trailer/Teaser (3 validos)", n == 3);

  // HERO: o mesmo /videos, a mesma escolha.
  extras_teste_url_hero(url, sizeof url, 1, 1399, "pt-BR");
  check("hero de serie pede /tv/<id>/videos com pt,en,null",
        strstr(url, "/tv/1399/videos?") &&
        strstr(url, "include_video_language=pt,en,null"));
  { char h[16];
    const char *bloco = strchr(strstr(corpo, "\"videos\""), '{');
    int nh = extras_teste_hero_serie(bloco, "pt-BR", h, sizeof h);
    check("hero de serie escolhe a temporada mais recente",
          nh == 3 && !strcmp(h, "s3TrailerEN")); }

  free(corpo);
  puts(falhas ? "trailer-serie: FALHOU" : "trailer-serie: tudo ok");
  return falhas ? 1 : 0;
}
