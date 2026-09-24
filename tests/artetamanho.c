// A variante do tamanho do desenho (src/artetamanho.h). Sem rede.
//
// As URLs sao as do log de campo (LG 1.7 GB, 1.4.3): capa de colecao em
// `original` do TMDB decodificada a 480 e a 704, still e fundo do metahub.
#include "../src/artetamanho.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char saida[600];

static void vira(const char *url, int limite, const char *esperado) {
  saida[0] = 0;
  if (!arte_tamanho_url(url, limite, saida, sizeof saida) || strcmp(saida, esperado)) {
    printf("FALHOU  %s @%d\n  esperado %s\n  saiu     %s\n", url, limite, esperado, saida);
    assert(0);
  }
}

static void fica(const char *url, int limite) {
  if (arte_tamanho_url(url, limite, saida, sizeof saida)) {
    printf("FALHOU  %s @%d deveria ficar, saiu %s\n", url, limite, saida);
    assert(0);
  }
}

int main(void) {
  const char *orig = "https://image.tmdb.org/t/p/original/fIhXD6m9LJgC7yDMOHMpYgkYAJ.jpg";

  // 1. TMDB `original`: a menor da escada que cobre o teto.
  vira(orig, 480, "https://image.tmdb.org/t/p/w780/fIhXD6m9LJgC7yDMOHMpYgkYAJ.jpg");
  vira(orig, 704, "https://image.tmdb.org/t/p/w780/fIhXD6m9LJgC7yDMOHMpYgkYAJ.jpg");
  vira(orig, 780, "https://image.tmdb.org/t/p/w780/fIhXD6m9LJgC7yDMOHMpYgkYAJ.jpg");
  vira(orig, 800, "https://image.tmdb.org/t/p/w1280/fIhXD6m9LJgC7yDMOHMpYgkYAJ.jpg");
  vira(orig, 1280, "https://image.tmdb.org/t/p/w1280/fIhXD6m9LJgC7yDMOHMpYgkYAJ.jpg");
  vira(orig, 288, "https://image.tmdb.org/t/p/w300/fIhXD6m9LJgC7yDMOHMpYgkYAJ.jpg");
  puts("ok  TMDB original -> w300/w780/w1280 pelo teto");

  // 2. Heroi de 1920 (e qualquer teto acima de 1280) continua em original:
  //    e a promocao que pede isso, e ela nao pode receber um w1280.
  fica(orig, 1312);
  fica(orig, 1920);
  puts("ok  teto acima de 1280 fica em original (promocao a heroi intacta)");

  // 3. wNNN desce so quando vale 1,5x; nunca sobe; nunca abaixo do teto.
  vira("https://image.tmdb.org/t/p/w1280/abc.jpg", 704, "https://image.tmdb.org/t/p/w780/abc.jpg");
  fica("https://image.tmdb.org/t/p/w1280/abc.jpg", 800);    // w1280 ja e o justo
  fica("https://image.tmdb.org/t/p/w1280/abc.jpg", 1920);   // nao sobe
  fica("https://image.tmdb.org/t/p/w342/abc.jpg", 288);     // 342 -> 300: sem ganho
  vira("https://image.tmdb.org/t/p/w500/abc.jpg", 288, "https://image.tmdb.org/t/p/w300/abc.jpg");
  fica("https://image.tmdb.org/t/p/w780/abc.jpg", 704);
  fica("https://image.tmdb.org/t/p/w185/abc.jpg", 128);
  fica("https://image.tmdb.org/t/p/h632/abc.jpg", 128);     // perfil por altura
  vira("http://image.tmdb.org/t/p/original/logo.png?x=1", 480, "http://image.tmdb.org/t/p/w780/logo.png?x=1");
  puts("ok  TMDB wNNN: so desce com ganho, sem ampliar, hNNN intacto");

  // 4. Still do metahub: w780/w1280/original.
  vira("https://episodes.metahub.space/tt33083249/1/5/original.jpg", 704,
       "https://episodes.metahub.space/tt33083249/1/5/w780.jpg");
  vira("https://episodes.metahub.space/tt33083249/1/5/original.jpg", 1280,
       "https://episodes.metahub.space/tt33083249/1/5/w1280.jpg");
  vira("https://episodes.metahub.space/tt33083249/1/5/w1280.jpg", 544,
       "https://episodes.metahub.space/tt33083249/1/5/w780.jpg");
  fica("https://episodes.metahub.space/tt33083249/1/5/original.jpg", 1920);
  fica("https://episodes.metahub.space/tt33083249/1/5/w780.jpg", 416);
  puts("ok  still do metahub pela escada w780/w1280/original");

  // 5. Fundo do metahub: small (480x270) so quando cobre; medium e 1920.
  vira("https://images.metahub.space/background/medium/tt3032476/img", 416,
       "https://images.metahub.space/background/small/tt3032476/img");
  vira("https://images.metahub.space/background/medium/tt3032476/img", 480,
       "https://images.metahub.space/background/small/tt3032476/img");
  fica("https://images.metahub.space/background/medium/tt3032476/img", 544);
  fica("https://images.metahub.space/background/small/tt3032476/img", 128);
  fica("https://images.metahub.space/poster/medium/tt3032476/img", 128);
  fica("https://images.metahub.space/logo/medium/tt3032476/img", 128);
  puts("ok  fundo do metahub: small ate 480, medium acima; cartaz e logo intactos");

  // 6. Host desconhecido, caminho local, teto invalido, saida pequena.
  fica("https://pub-c3220058d0b8427593faaf225ea539fd.r2.dev/Backdrops%20festivales/Oscars/Untitled%20design.png", 704);
  fica("https://proxy.example/?u=https://image.tmdb.org/t/p/original/a.jpg", 704);
  fica("/app/art/00.jpg", 416);
  fica("nuvio.invalid/arte/tmdb/w1280/tt0468569", 416);
  fica(orig, 0);
  fica(NULL, 704);
  assert(!arte_tamanho_url(orig, 704, saida, 20));
  puts("ok  host desconhecido, local, teto 0 e saida curta ficam como vieram");

  puts("artetamanho: tudo ok");
  return 0;
}
