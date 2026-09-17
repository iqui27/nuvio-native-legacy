#include "artehero.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TRES REGRAS, na ordem em que valem a pena.
//
// 1. metahub `background`: JA E 1920x1080 e custa 850 KB. Medido em 17/09 com
//    curl: /background/medium/, /big/, /large/ e /original/ devolvem o MESMO
//    arquivo, byte a byte — o metahub tem um tamanho so para fundo. Quando o
//    item tem id do IMDb e nao tem fundo melhor, esta url e deterministica e
//    pode ser montada sem consultar ninguem.
// 2. TMDB `w1280` -> `original`: a escada do TMDB para backdrop e
//    w300/w780/w1280/original, sem nada entre 1280 e 3840. Para o desenho de
//    1920 o w1280 amplia 1,5x; `original` e o unico caminho para pixel de
//    verdade. O decode e limitado a 1920 pelo teto do cache (tex_obter_hero),
//    entao o custo e o download e a decodificacao, nao a memoria da textura.
// 3. O que ja esta la. Inclui o cartaz, que a home desenha CONTIDO no lado
//    direito em vez de esticar — ver desenhaArteHero.

// O ID DO TITULO, SEM O EPISODIO COLADO.
//
// Um item de "Continuar assistindo" de serie guarda `imdb` como
// "tt26545992:1:1" — id, temporada e episodio na mesma string, que e a chave
// que o app usa para progresso. O metahub responde por id PURO: com a string
// inteira a url sai "/tt26545992:1:1/1/1/original.jpg" e o servidor devolve
// corpo vazio. MEDIDO no log local: tres downloads falhados antes de eu ver.
static const char *idLimpo(const char *imdb, char *dst, size_t tam) {
  size_t i = 0;
  if (!imdb) return "";
  while (imdb[i] && imdb[i] != ':' && i + 1 < tam) { dst[i] = imdb[i]; i++; }
  dst[i] = 0;
  return dst;
}

static int qualidadeImg = 1;
void artehero_qualidade(int nivel) {
  if (nivel >= 0 && nivel <= 2) qualidadeImg = nivel;
}

const char *artehero_url_card(const CatItem *item) {
  if (!item) return NULL;
  if (item->backdrop[0]) return item->backdrop;
  if (item->poster[0]) return item->poster;
  return NULL;
}

const char *artehero_url_episodio(const CatItem *item) {
  static char buf[512];
  if (!item) return NULL;
  // NA BAIXA O STILL VEM EM w780: e a mesma imagem, no tamanho que o metahub
  // serve mais rapido. O destaque amplia, e nesse nivel e isso que se pediu.
  if (qualidadeImg == 0) {
    if (item->temporada <= 0 || item->episodio <= 0) return NULL;
    if (strncmp(item->imdb, "tt", 2)) return NULL;
    { char id[32];
      idLimpo(item->imdb, id, sizeof id);
      snprintf(buf, sizeof buf,
               "https://episodes.metahub.space/%s/%d/%d/w780.jpg",
               id, item->temporada, item->episodio); }
    return buf;
  }
  if (item->temporada <= 0 || item->episodio <= 0) return NULL;
  if (strncmp(item->imdb, "tt", 2)) return NULL;
  { char id[32];
    idLimpo(item->imdb, id, sizeof id);
    snprintf(buf, sizeof buf,
             "https://episodes.metahub.space/%s/%d/%d/original.jpg",
             id, item->temporada, item->episodio); }
  return buf;
}

const char *artehero_url(const CatItem *item) {
  static char buf[512];
  const char *b;
  if (!item) return NULL;
  // BAIXA: a url guardada, sem subir de tamanho. O fundo montado por id (o
  // caso "nao ha fundo nenhum") continua valendo — ali a alternativa e o cartaz
  // esticado, que nao e mais leve, e sim mais feio.
  if (qualidadeImg == 0 && item->backdrop[0]) return item->backdrop;
  b = item->backdrop;

  // TMDB em w1280: sobe para `original`. A troca e textual e so acontece no
  // caminho exato do TMDB — qualquer outra url passa intacta.
  if (b[0]) {
    const char *p = strstr(b, "/t/p/w1280/");
    if (p) {
      size_t pre = (size_t)(p - b);
      if (pre < sizeof buf) {
        snprintf(buf, sizeof buf, "%.*s/t/p/original/%s", (int)pre, b,
                 p + strlen("/t/p/w1280/"));
        return buf;
      }
    }
    // w780 aparece em fundo vindo do Trakt antigo; mesma conta.
    p = strstr(b, "/t/p/w780/");
    if (p) {
      size_t pre = (size_t)(p - b);
      if (pre < sizeof buf) {
        snprintf(buf, sizeof buf, "%.*s/t/p/original/%s", (int)pre, b,
                 p + strlen("/t/p/w780/"));
        return buf;
      }
    }
    // TRAKT: /medium/ -> /full/. Medido em 17/09 com curl na mesma arte:
    // medium 1280x720 e 70 KB, full 1920x1080 e 155 KB, thumb 853x480. O
    // caminho tem o tamanho no meio ("/fanarts/medium/"), entao a troca e
    // textual como a do TMDB.
    { const char *p2 = strstr(b, "/medium/");
      if (p2 && strstr(b, "media.trakt.tv")) {
        size_t pre = (size_t)(p2 - b);
        if (pre < sizeof buf) {
          snprintf(buf, sizeof buf, "%.*s/full/%s", (int)pre, b,
                   p2 + strlen("/medium/"));
          return buf;
        }
      } }
    return b;
  }

  // SEM FUNDO, MAS COM ID DO IMDB: o metahub responde por id, e a url e
  // deterministica. E o caso do titulo que veio de uma lista sem arte — antes
  // dele o destaque caia no CARTAZ, que e a diferenca que se ve entre "um
  // poster esticado" e "um fundo de verdade".
  if (!strncmp(item->imdb, "tt", 2)) {
    char id[32];
    idLimpo(item->imdb, id, sizeof id);
    snprintf(buf, sizeof buf,
             "https://images.metahub.space/background/medium/%s/img", id);
    return buf;
  }

  if (item->poster[0]) return item->poster;
  return NULL;
}
