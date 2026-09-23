// Ver tests/artefontes.sh.
#include "artefontes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
#define OK(c, m) do { if (!(c)) { printf("FALHOU: %s\n", m); falhas++; } else printf("ok  %s\n", m); } while (0)

static char *ler(const char *dir, const char *nome) {
  char c[600];
  FILE *f;
  long n;
  char *b;
  snprintf(c, sizeof c, "%s/%s", dir, nome);
  f = fopen(c, "rb");
  if (!f) { printf("sem fixture %s\n", c); exit(1); }
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1);
  if (fread(b, 1, (size_t)n, f) != (size_t)n) exit(1);
  b[n] = 0;
  fclose(f);
  return b;
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "tests/fixtures/arte";
  char padrao[160], outro[160], u[600];
  char *j;

  // TMDB filme (append_to_response=images): padrao da raiz, outro = o sem
  // texto de maior nota que nao e o padrao; o com texto (en) nunca ganha.
  j = ler(dir, "tmdb_movie_278_images.json");
  OK(af_tmdb_fundos(j, NULL, padrao, sizeof padrao, outro, sizeof outro) == 1, "tmdb: outro backdrop existe");
  OK(!strcmp(padrao, "/pNjh59JSxChQktamG3LMp9ZoQzp.jpg"), "tmdb: padrao e o backdrop_path da raiz");
  OK(!strcmp(outro, "/zfbjgQE1uSd9wiPTX4VzsLi0rGG.jpg"), "tmdb: outro = sem texto, maior nota, nao o padrao");
  OK(af_tmdb_fundos(j, "/zfbjgQE1uSd9wiPTX4VzsLi0rGG.jpg", padrao, sizeof padrao, outro, sizeof outro) == 1 &&
     !strcmp(outro, "/kXfqcdQKsToO0OUXHcrrNCHDBzO.jpg"), "tmdb: evitar o do card; empate de nota vai para mais votos");
  free(j);
  // So /images (serie): sem backdrop_path na raiz; o padrao vem do /find.
  j = ler(dir, "tmdb_tv_1396_images.json");
  OK(af_tmdb_fundos(j, "/tsRy63Mu5cu8etL1X7ZLyf7UP1M.jpg", padrao, sizeof padrao, outro, sizeof outro) == 1 &&
     !padrao[0] && !strcmp(outro, "/sp1RSDvoVsbvDouQx1A75ebU35e.jpg"), "tmdb /images: exclui o padrao dado pelo /find");
  free(j);
  OK(af_tmdb_fundos("{\"backdrop_path\":\"/so.jpg\",\"images\":{\"backdrops\":[{\"file_path\":\"/so.jpg\",\"iso_639_1\":null}]}}",
                    NULL, padrao, sizeof padrao, outro, sizeof outro) == 0 && !outro[0], "tmdb: so o padrao = nao ha outro");

  // fanart.tv: sem idioma ("" ou "00") antes do com texto; mais likes.
  j = ler(dir, "fanart_movie_278.json");
  OK(af_fanart_fundo(j, 0, u, sizeof u) && strstr(u, "limpo-muitos.jpg"), "fanart filme: sem texto, mais likes");
  OK(!af_fanart_fundo(j, 1, u, sizeof u), "fanart: filme nao tem showbackground");
  free(j);
  j = ler(dir, "fanart_tv_81189.json");
  OK(af_fanart_fundo(j, 1, u, sizeof u) && strstr(u, "bb-b.jpg"), "fanart serie: showbackground");
  free(j);

  // Kitsu: busca (lista) com ano e tipo; objeto unico por id.
  j = ler(dir, "kitsu_busca_attack_on_titan.json");
  OK(af_kitsu_capa(j, 2013, 1, u, sizeof u) && !strcmp(u, "https://media.kitsu.app/anime/cover_images/7442/large.jpg"),
     "kitsu busca: primeiro com o ano certo");
  OK(af_kitsu_capa(j, 2017, 1, u, sizeof u) && strstr(u, "/8671/"), "kitsu busca: ano decide a temporada");
  OK(!af_kitsu_capa(j, 2013, 0, u, sizeof u), "kitsu busca: filme nao casa com serie TV");
  OK(!af_kitsu_capa(j, 1990, 1, u, sizeof u), "kitsu busca: ano longe = nada");
  free(j);
  j = ler(dir, "kitsu_anime_7442.json");
  OK(af_kitsu_capa(j, 0, 1, u, sizeof u) && strstr(u, "/7442/large.jpg"), "kitsu por id");
  free(j);

  // AniList: bannerImage com as barras escapadas.
  j = ler(dir, "anilist_busca_attack_on_titan.json");
  OK(af_anilist_banner(j, 2013, u, sizeof u) &&
     !strcmp(u, "https://s4.anilist.co/file/anilistcdn/media/anime/banner/16498-8jpFCOcDmneX.jpg"), "anilist: banner");
  OK(!af_anilist_banner(j, 2020, u, sizeof u), "anilist: ano longe = nada");
  OK(!af_anilist_banner("{\"data\":{\"Media\":{\"bannerImage\":null}}}", 0, u, sizeof u), "anilist: banner null");
  free(j);

  // Apple: o modelo do mzstatic no tamanho do desenho.
  OK(af_apple_tamanho("https://is1-ssl.mzstatic.com/image/thumb/CjKP9J_FPtPin1VBVU4-mg/{w}x{h}.{f}", 1920, u, sizeof u) &&
     !strcmp(u, "https://is1-ssl.mzstatic.com/image/thumb/CjKP9J_FPtPin1VBVU4-mg/1920x1080.jpg"), "apple: 1920x1080.jpg");
  OK(af_apple_tamanho("https://x/{w}x{h}.{f}", 1280, u, sizeof u) && strstr(u, "1280x720.jpg"), "apple: 1280x720.jpg");
  OK(!af_apple_tamanho("https://x/1920x1080.jpg", 1280, u, sizeof u), "apple: sem marcador = nada");

  // Titulo no segmento da url virtual: ida e volta, com '/' e acento.
  { char e[200], d[200];
    const char *fim;
    af_codificar("AC/DC: Ação & Fúria", e, sizeof e);
    OK(!strchr(e, '/') && !strchr(e, ' '), "codificar: sem barra nem espaco");
    fim = af_decodificar(e, d, sizeof d);
    OK(!strcmp(d, "AC/DC: Ação & Fúria") && !*fim, "decodificar: volta igual");
    fim = af_decodificar("Dune/resto", d, sizeof d);
    OK(!strcmp(d, "Dune") && *fim == '/', "decodificar: para na barra"); }

  if (falhas) { printf("artefontes: %d falha(s)\n", falhas); return 1; }
  printf("artefontes: tudo ok\n");
  return 0;
}
