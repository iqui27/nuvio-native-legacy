#include "artehero.h"
#include "artereserva.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TRES REGRAS, na ordem em que valem a pena.
//
// 1. metahub `background`: JA E 1920x1080 e custa 850 KB. Medido em 17/09 com
//    curl: /background/medium/, /big/, /large/ e /original/ devolvem o MESMO
//    arquivo, byte a byte. (Medido em 20/09: /background/small/ EXISTE e e
//    outro arquivo, 480x270 e 12 KB — tex_cache.c usa isso no Tizen para
//    card.) Quando o
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

static int (*jaFalhou)(const char *) = NULL;
void artehero_definir_falhou(int (*falhou)(const char *caminho)) {
  jaFalhou = falhou;
}
static int falhou(const char *u) { return u && jaFalhou && jaFalhou(u); }

static int (*mesmaImagem)(const char *, const char *) = NULL;
void artehero_definir_igual(int (*igual)(const char *a, const char *b)) {
  mesmaImagem = igual;
}

static int (*resolvida)(const char *, char *, size_t) = NULL;
void artehero_definir_resolvida(int (*f)(const char *url, char *saida, size_t tam)) {
  resolvida = f;
}

static int fanartLigado;
void artehero_fanart_disponivel(int sim) { fanartLigado = sim ? 1 : 0; }

static int qualidadeImg = 1;
void artehero_qualidade(int nivel) {
  if (nivel >= 0 && nivel <= 2) qualidadeImg = nivel;
}

// FUNDO EM `original` SO ONDE O DECODE E ESCALADO. Na LG o JPEG passa por
// jpegrapido.c (libjpeg da TV, reducao na DCT): um backdrop 3840x2160 sai em
// 1920 sem nunca existir inteiro na memoria. Na Samsung o decode e do
// NAVEGADOR (createImageBitmap) e devolve os 3840x2160 cheios — 33 MB em
// ABGR8888 — dentro de um heap WASM de 256 MiB. MEDIDO no registro 1450
// (Tizen 6, 2 GB, 1.4.0): tres fundos `original` decodificados em 3 s, malloc
// de 21 para 68 MiB, 5,7 MiB livres no heap e `Aborted(OOM)` nove segundos
// depois — o unico abort fatal da 1.4.0. No Tizen a ALTA fica em w1280 para
// fundo e still; o logo (PNG pequeno) continua subindo.
static int fundoOriginal(void) {
#ifdef __EMSCRIPTEN__
  return 0;
#else
  return qualidadeImg == 2;
#endif
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
  // O TAMANHO DO STILL SEGUE A QUALIDADE ESCOLHIDA, e aqui o motivo e MEDIDO na
  // C9 com a linha de decode lento:
  //
  //   fundo do metahub   1920x1080  ->   348, 379, 348 ms
  //   still `original`   3840x2160  ->  1596, 1627, 1966 ms
  //
  // Dois segundos e a arte chegando depois de a pessoa ter passado por ela, e
  // foi isso que o dono viu ("demorando bem mais para carregar as artes"). O
  // metahub nao tem um tamanho fixo para still: `original` e o que a fonte
  // tiver, e para varias series isso e 3840.
  //
  // No PADRAO o still vem em w1280 (1280x720, ~0,9 MP, da ordem de 150 ms) e o
  // desenho amplia 1,5x — a mesma conta que o fundo do TMDB fazia antes, e que
  // num still de episodio custa menos do que esperar dois segundos por ele. Na
  // ALTA vale a espera: e para isso que o nivel existe.
  { char id[32];
    const char *tam = fundoOriginal() ? "original" : "w1280";
    idLimpo(item->imdb, id, sizeof id);
    snprintf(buf, sizeof buf,
             "https://episodes.metahub.space/%s/%d/%d/%s.jpg",
             id, item->temporada, item->episodio, tam); }
  return buf;
}

const char *artehero_url(const CatItem *item) {
  static char buf[512];
  const char *b;
  if (!item) return NULL;
  b = item->backdrop;
  // A MESMA IMAGEM DO CARD, SEMPRE (19/09, pedido do dono): "clicar no card e
  // as coisas simplesmente se posicionarem, nao uma mudanca de pagina". Ate
  // aqui o heroi e o detalhe pediam o fundo do METAHUB para todo titulo com
  // id do IMDb — outra foto, outro download, e quando o metahub nao tinha o
  // titulo (404) o detalhe abria SEM arte enquanto o card mostrava uma. Agora
  // heroi e detalhe sao o arquivo que o card ja baixou, decodificado inteiro
  // (a promocao de tex_cache): zero download novo, zero troca de foto. O
  // w1280 do TMDB amplia 1,5x na tela cheia; o fundo do metahub (fileiras do
  // Cinemeta) ja e 1920. Na ALTA o TMDB sobe para `original`, que com o
  // decode escalado (jpegrapido.c) sai em 1920 sem custar os 3840 inteiros.
  if (b[0]) {
    if (fundoOriginal()) {
      const char *p = strstr(b, "/t/p/w1280/");
      if (!p) p = strstr(b, "/t/p/w780/");
      if (p) {
        size_t pre = (size_t)(p - b);
        const char *resto = strchr(p + 5, '/');   // depois do tamanho
        if (pre < sizeof buf && resto && resto[1]) {
          snprintf(buf, sizeof buf, "%.*s/t/p/original/%s", (int)pre, b, resto + 1);
          return buf;
        }
      }
      { const char *p2 = strstr(b, "/medium/");
        if (p2 && strstr(b, "media.trakt.tv")) {
          size_t pre = (size_t)(p2 - b);
          if (pre < sizeof buf) {
            snprintf(buf, sizeof buf, "%.*s/full/%s", (int)pre, b, p2 + strlen("/medium/"));
            return buf;
          }
        } }
    }
    return b;
  }
  // SEM FUNDO NENHUM: o metahub monta um por id do IMDb — fundo de verdade em
  // vez do cartaz esticado. Se ja falhou, o cartaz.
  if (!strncmp(item->imdb, "tt", 2)) {
    char id[32];
    idLimpo(item->imdb, id, sizeof id);
    snprintf(buf, sizeof buf,
             "https://images.metahub.space/background/medium/%s/img", id);
    if (!falhou(buf)) return buf;
  }
  if (item->poster[0]) return item->poster;
  return NULL;
}

// POR QUE O AJUSTE "BACKGROUND DO HERO" NAO FAZIA NADA (22/09, dono: "a
// settings de selecionar o source das artes nao ta funcionando").
//
// A versao anterior so devolvia TMDB/Trakt quando o item JA trazia a url
// daquela fonte (backdropTmdb/backdropTrakt), e NULL no resto — e o chamador
// (home.c, arte_hero_do_item) caia em artehero_url(), a arte automatica. Um
// item do Cinemeta chega com `background` = metahub/<tt> (curl no top de
// filmes em 22/09: os tres primeiros, os tres assim) e mais nada:
//   1 Catalogo  = backdropCatalogo = backdrop = metahub/<tt>
//   2 Metahub   = metahub/<tt>, montado pelo id: o MESMO arquivo
//   3 TMDB      = NULL -> automatico -> metahub/<tt>
//   4 Trakt     = NULL -> automatico -> metahub/<tt>
// Cinco escolhas, uma imagem. backdropTmdb so nasce quando o titulo e ABERTO
// (fotosDoElenco, descoberta.c) ou vem de catalogo do TMDB; backdropTrakt so
// em item vindo de lista do Trakt. E o card nunca lia o ajuste.
//
// Agora TMDB e Trakt sem url no item viram url VIRTUAL pelo id do IMDb
// (artereserva.h), resolvida no fio de rede do tex_cache: uma consulta por
// titulo, cacheada em disco sob a virtual.
// O ANO do titulo, da linha "2022 · 3 temporadas" do catalogo. 0 = nao ha.
static int anoDoItem(const CatItem *item) {
  const char *p = item->meta;
  for (; p && p[0]; p++)
    if (p[0] >= '0' && p[0] <= '9' && p[1] >= '0' && p[1] <= '9' &&
        p[2] >= '0' && p[2] <= '9' && p[3] >= '0' && p[3] <= '9' &&
        !(p[4] >= '0' && p[4] <= '9')) {
      int a = atoi(p);
      if (a > 1880 && a < 2100) return a;
    }
  return 0;
}

static int ehSerie(const CatItem *item) { return !strcmp(item->tipo, "series"); }

// O titulo num segmento da url virtual: o mesmo codigo de af_codificar
// (artefontes.c, que o resolvedor usa para decodificar), repetido aqui para
// este modulo continuar sem dependencia — varios testes o compilam sozinho.
static void codificar(const char *s, char *dst, size_t n) {
  static const char hex[] = "0123456789ABCDEF";
  size_t k = 0;
  for (; s && *s && k + 4 < n; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') dst[k++] = (char)c;
    else { dst[k++] = '%'; dst[k++] = hex[c >> 4]; dst[k++] = hex[c & 15]; }
  }
  if (n) dst[k] = 0;
}

// ANIME: id de addon de anime (kitsu:, mal:, anilist:) ou, para um tt,
// genero Animacao/Anime E pais Japao. So o genero pegaria Pixar e Disney,
// e cada um custaria duas buscas (Kitsu e AniList) para nada.
static int ehAnime(const CatItem *item) {
  if (!strncmp(item->imdb, "kitsu:", 6) || !strncmp(item->imdb, "mal:", 4) ||
      !strncmp(item->imdb, "anilist:", 8)) return 1;
  if (strncmp(item->imdb, "tt", 2)) return 0;
  if (!strstr(item->genero, "Anima") && !strstr(item->genero, "Anime")) return 0;
  return strstr(item->pais, "Jap") != NULL;
}

// Tamanho do TMDB por aparelho e qualidade (23/09/2026). MEDIDO na C9 com
// conexao nova, backdrop de Um Sonho de Liberdade: w780 63 KB 0,84-1,05 s,
// w1280 208 KB 1,14-1,19 s, original (1920 neste titulo) 446 KB 1,23-1,34 s;
// com a conexao reusada 0,26 / 0,32 / 0,37 s. A rede quase nao separa os
// tres: o que separa e o decode (o original de varios titulos e 3840, ~1,6 s
// na C9). Baixa = w780, Padrao = w1280, Alta na LG = original; Samsung nunca
// original (fundoOriginal).
static const char *tamTmdb(int grande) {
  if (qualidadeImg == 0) return "w780";
  return grande && fundoOriginal() ? "original" : "w1280";
}

// "/m278" ou "/t1399" quando o item ja sabe o id do TMDB (moviedb_id do
// Cinemeta): o resolvedor pula o /find. "" quando nao sabe.
static const char *sufixoTmdb(const CatItem *item, char *buf, size_t n) {
  buf[0] = 0;
  if (item->tmdb > 0 && item->tipo[0])
    snprintf(buf, n, "/%c%ld", ehSerie(item) ? 't' : 'm', item->tmdb);
  return buf;
}

static const char *urlDaFonte(const CatItem *item, int fonte, int grande,
                              char *saida, size_t tam) {
  const char *b = NULL;
  char id[32], suf[24];
  int temTt;
  if (!item) return NULL;
  temTt = !strncmp(item->imdb, "tt", 2);
  if (temTt) idLimpo(item->imdb, id, sizeof id);
  switch (fonte) {
    case ARTEHERO_TMDB_OUTRO:
      if (!temTt) return NULL;
      snprintf(saida, tam, "%s" "tmdbalt/%s/%s%s", ARTE_VIRTUAL_PREFIXO,
               tamTmdb(grande), id, sufixoTmdb(item, suf, sizeof suf));
      return saida;
    case ARTEHERO_APPLE: {
      // Um tamanho para card e destaque (o mesmo arquivo): 1920 na LG, 1280
      // na Samsung e na Baixa.
      char enc[200];
      int ano = anoDoItem(item);
#ifdef __EMSCRIPTEN__
      const char *t = "1280";
#else
      const char *t = qualidadeImg == 0 ? "1280" : "1920";
#endif
      if (!temTt || !ano || !item->titulo[0]) return NULL;
      codificar(item->titulo, enc, sizeof enc);
      snprintf(saida, tam, "%s" "apple/%s/%s/%c/%d/%s", ARTE_VIRTUAL_PREFIXO,
               t, id, ehSerie(item) ? 's' : 'm', ano, enc);
      return saida;
    }
    case ARTEHERO_FANART:
      if (!fanartLigado || !temTt) return NULL;
      snprintf(saida, tam, "%s" "fanart/full/%s/%c/%ld", ARTE_VIRTUAL_PREFIXO,
               id, ehSerie(item) ? 's' : 'm', item->tmdb > 0 ? item->tmdb : 0L);
      return saida;
    case ARTEHERO_ANIME: {
      char enc[200], aid[40];
      if (!ehAnime(item)) return NULL;
      if (temTt) snprintf(aid, sizeof aid, "%s", id);
      else {
        // "kitsu:7442" inteiro; um ":<episodio>" depois dele sai.
        const char *d = strchr(item->imdb, ':');
        const char *d2 = d ? strchr(d + 1, ':') : NULL;
        size_t n = d2 ? (size_t)(d2 - item->imdb) : strlen(item->imdb);
        if (n >= sizeof aid) return NULL;
        memcpy(aid, item->imdb, n); aid[n] = 0;
      }
      codificar(item->titulo, enc, sizeof enc);
      snprintf(saida, tam, "%s" "anime/large/%s/%c/%d/%s", ARTE_VIRTUAL_PREFIXO,
               aid, ehSerie(item) ? 's' : 'm', anoDoItem(item), enc);
      return saida;
    }
    case ARTEHERO_CATALOGO:
      b = item->backdropCatalogo[0] ? item->backdropCatalogo
        : item->backdrop[0] ? item->backdrop : NULL;
      break;
    case ARTEHERO_METAHUB:
      // Um tamanho so (medium = 1920, ver regra 1 no topo); o card no Tizen
      // desce para /small/ dentro do tex_cache.
      if (!temTt) return NULL;
      snprintf(saida, tam, "https://images.metahub.space/background/medium/%s/img", id);
      return saida;
    case ARTEHERO_TMDB:
      b = item->backdropTmdb[0] ? item->backdropTmdb :
          strstr(item->backdrop, "image.tmdb.org/t/p/") ? item->backdrop : NULL;
      if (!b && temTt) {
        snprintf(saida, tam, "%s" "tmdb/%s/%s%s", ARTE_VIRTUAL_PREFIXO,
                 tamTmdb(grande), id, sufixoTmdb(item, suf, sizeof suf));
        return saida;
      }
      break;
    case ARTEHERO_TRAKT:
      b = item->backdropTrakt[0] ? item->backdropTrakt :
          strstr(item->backdrop, "media.trakt.tv/") ? item->backdrop : NULL;
      if (!b && temTt) {
        snprintf(saida, tam, "%s" "trakt/%s/%s", ARTE_VIRTUAL_PREFIXO,
                 grande && fundoOriginal() ? "full" : "medium", id);
        return saida;
      }
      break;
    default: return NULL;
  }
  if (!b || !b[0]) return NULL;
  if (!grande || !fundoOriginal()) return b;
  // Tela cheia na ALTA: a mesma subida para o original que artehero_url faz.
  { const char *p = strstr(b, "/t/p/w1280/");
    if (!p) p = strstr(b, "/t/p/w780/");
    if (p) {
      size_t pre = (size_t)(p - b);
      const char *resto = strchr(p + 5, '/');
      if (pre < tam && resto && resto[1]) {
        snprintf(saida, tam, "%.*s/t/p/original/%s", (int)pre, b, resto + 1);
        return saida;
      }
    } }
  { const char *p2 = strstr(b, "/medium/");
    if (p2 && strstr(b, "media.trakt.tv/")) {
      size_t pre = (size_t)(p2 - b);
      if (pre < tam) {
        snprintf(saida, tam, "%.*s/full/%s", (int)pre, b, p2 + strlen("/medium/"));
        return saida;
      }
    } }
  return b;
}

// O hero pede a arte atual e a anterior no mesmo quadro durante a mistura, e
// o card e o destaque do mesmo titulo tambem. Um buffer unico faria a ultima
// url montada sobrescrever as outras. As tentativas montam num buffer LOCAL e
// so a vencedora ocupa o anel — assim cada chamada gasta UMA posicao, e 16
// cobre destaque atual/anterior, prefetch e os cards do quadro com folga.
static const char *fixar(const char *u, const char *tmp) {
  static char anel[16][512];
  static int vez;
  char *b;
  if (u != tmp) return u;              // url do proprio item: ja e estavel
  b = anel[vez++ % 16];
  snprintf(b, sizeof anel[0], "%s", tmp);
  return b;
}

const char *artehero_url_fonte(const CatItem *item, int fonte) {
  char tmp[512];
  if (!item) return NULL;
  if (fonte <= ARTEHERO_AUTO) return artehero_url(item);
  return fixar(urlDaFonte(item, fonte, 1, tmp, sizeof tmp), tmp);
}

// A MESMA FOTO em tamanhos diferentes conta como igual: TMDB w1280 e
// original do mesmo arquivo, Trakt medium e full, metahub pelo id. Sem isto o
// modo "diferente" aceitaria como "outra arte" o original do proprio card.
static void chaveFoto(const char *u, char *k, size_t n) {
  const char *p;
  if (!u) { k[0] = 0; return; }
  if ((p = strstr(u, "/t/p/")) != NULL && (p = strchr(p + 5, '/')) != NULL) {
    snprintf(k, n, "tmdb%s", p); return;
  }
  if (strstr(u, "media.trakt.tv/") &&
      ((p = strstr(u, "/medium/")) != NULL || (p = strstr(u, "/full/")) != NULL)) {
    snprintf(k, n, "trakt%s", strchr(p + 1, '/')); return;
  }
  if ((p = strstr(u, "images.metahub.space/background/")) != NULL &&
      (p = strchr(p + 32, '/')) != NULL) {
    snprintf(k, n, "metahub%s", p); return;
  }
  if (!strncmp(u, ARTE_VIRTUAL_PREFIXO, strlen(ARTE_VIRTUAL_PREFIXO))) {
    const char *f = u + strlen(ARTE_VIRTUAL_PREFIXO), *t = strchr(f, '/');
    if (t && (p = strchr(t + 1, '/')) != NULL) {
      snprintf(k, n, "v%.*s%s", (int)(t - f), f, p); return;
    }
  }
  snprintf(k, n, "%s", u);
}
static int mesmaFoto(const char *a, const char *b) {
  char ka[512], kb[512];
  if (!a || !b) return 0;
  chaveFoto(a, ka, sizeof ka);
  chaveFoto(b, kb, sizeof kb);
  if (!strcmp(ka, kb)) return 1;
  // Uma virtual ja resolvida compara pela url REAL: o outro do TMDB que caiu
  // no mesmo backdrop do card (em outro tamanho) e a mesma foto.
  if (resolvida) {
    char ra[600], rb[600];
    const char *xa = resolvida(a, ra, sizeof ra) ? ra : a;
    const char *xb = resolvida(b, rb, sizeof rb) ? rb : b;
    if (xa != a || xb != b) {
      chaveFoto(xa, ka, sizeof ka);
      chaveFoto(xb, kb, sizeof kb);
      if (!strcmp(ka, kb)) return 1;
    }
  }
  // Caminhos diferentes, mesmo arquivo: catalogo do Cinemeta x metahub pelo
  // id (1763947 bytes nos dois no relatorio 1669), ou uma virtual que
  // resolveu para a url do card. So se sabe depois do download.
  return mesmaImagem && mesmaImagem(a, b);
}

const char *artehero_url_card_fonte(const CatItem *item, int fonte, int diferente) {
  char tmp[512];
  const char *u;
  if (!item) return NULL;
  // Diferente LIGADO: o card fica com a arte do catalogo, sempre; a fonte
  // escolhida vai so para o destaque e o detalhe (regra no .h).
  if (diferente || fonte <= ARTEHERO_AUTO) return artehero_url_card(item);
  u = urlDaFonte(item, fonte, 0, tmp, sizeof tmp);
  if (u && !falhou(u)) return fixar(u, tmp);
  return artehero_url_card(item);
}

const char *artehero_url_destaque(const CatItem *item, int fonte, int diferente) {
  // Ordem de busca de "outra arte" quando a escolhida nao serve: TMDB e Trakt
  // primeiro porque sao as que costumam ser OUTRA foto do metahub/Cinemeta
  // (o fanart do Trakt vem do fanart.tv); o metahub e o catalogo por ultimo.
  // (23/09) Apple e o OUTRO do TMDB primeiro: sao as unicas que nao saem da
  // mesma pilha de backdrops do TMDB que o metahub usa (ver ARTEHERO_TMDB_OUTRO
  // no .h). A Apple tambem e a mais rapida medida na C9 (busca 0,4-0,5 s +
  // imagem 0,25-0,29 s, contra 0,7 + 1,2 s do TMDB com conexao nova).
  static const int ORDEM[] = { ARTEHERO_APPLE, ARTEHERO_TMDB_OUTRO,
                               ARTEHERO_FANART, ARTEHERO_ANIME,
                               ARTEHERO_TRAKT, ARTEHERO_TMDB,
                               ARTEHERO_METAHUB, ARTEHERO_CATALOGO };
  char tmp[512];
  const char *card, *u;
  int i;
  if (!item) return NULL;
  if (!diferente) {
    // PADRAO: a mesma imagem do card (19/09), agora com a fonte escolhida
    // valendo para os dois. Sem a fonte, a arte automatica.
    if (fonte <= ARTEHERO_AUTO) return artehero_url(item);
    u = urlDaFonte(item, fonte, 1, tmp, sizeof tmp);
    if (u && !falhou(u)) return fixar(u, tmp);
    return artehero_url(item);
  }
  card = artehero_url_card(item);
  // Com outra arte, TMDB e o OUTRO backdrop do TMDB, nao o padrao.
  if (fonte == ARTEHERO_TMDB) fonte = ARTEHERO_TMDB_OUTRO;
  if (fonte > ARTEHERO_AUTO) {
    u = urlDaFonte(item, fonte, 1, tmp, sizeof tmp);
    if (u && !falhou(u) && !mesmaFoto(u, card)) return fixar(u, tmp);
  }
  for (i = 0; i < (int)(sizeof ORDEM / sizeof *ORDEM); i++) {
    if (ORDEM[i] == fonte) continue;
    u = urlDaFonte(item, ORDEM[i], 1, tmp, sizeof tmp);
    if (u && !falhou(u) && !mesmaFoto(u, card)) return fixar(u, tmp);
  }
  // Nenhuma outra existe (sem id do IMDb, tudo falhou): a do card, que e
  // melhor que tela vazia.
  return artehero_url(item);
}

// O LOGO DO TITULO. Mesmo problema do fundo, numa escada diferente.
//
// O Cinemeta devolve o logo do TMDB em `/t/p/original/`, e o desenho maior que
// existe para ele tem 1000 px de largura (NV_DETW_LOGO_MAXW). MEDIDO em 17/09
// com curl no logo de um titulo real (xSj9QrjsR8fZnSuEt6e7QZvrqSy.png):
// `original` e 4127x2000 e 11,9 MB, `w1280` e 1279x620 e 828 KB, `w780` e
// 780x378 e 321 KB, `w500` e 499x242 e 144 KB. Sao dez vezes menos pixels para
// decodificar chegando ao mesmo desenho — no log da C9 esse `original` levou
// 1303, 1464 e 1480 ms, tres vezes, para sair a 480, 512 e 640 px de largura.
//
// A escada documentada do TMDB para logo para em w500, mas o CDN responde a
// w780 e w1280 (conferido acima, os dois com 200 e PNG do tamanho pedido) — o
// redimensionador e o mesmo para todos os tipos.
//
// Na alta a interface e desenhada em 2x, entao o logo pode ocupar 2000 px e so
// `original` tem pixel para isso.
// POR QUE UM ANEL DE BUFFERS e nao um estatico so, como o resto deste arquivo:
// o fundo de tela cheia e UM por quadro, mas o logo nao. A home desenha o do
// destaque, o da colecao e o do card focado no MESMO quadro, e cada um chama
// tex_obter, tex_aspecto e tex_marca_escura com a string — se todos
// compartilhassem um buffer, o segundo desenho reescreveria a url que o
// primeiro ainda esta usando e as texturas sairiam trocadas. Quatro cobre os
// desenhos simultaneos que existem hoje com folga.
#define LOGO_ANEL 4
const char *artehero_url_logo(const char *logo) {
  return artehero_url_logo_larg(logo, 0.0f);
}

// TAMANHO PELO DESENHO. O TMDB serve o mesmo logo em w300, w500, w780, w1280
// e original; um card de fileira desenha o logo com ~300 px e o detalhe com
// ate 1000. MEDIDO na C9 em 19/09: logos de 1280x1152 e 1280x1307 (PNG, w1280)
// levavam 300 a 460 ms para virar 480 a 640 px de textura — o decode de um
// backdrop inteiro por um logo de card. `larg` 0 = nao se sabe: fica o teto
// da qualidade, como antes. Qualidade baixa desce um degrau; alta pede sempre
// o original.
const char *artehero_url_logo_larg(const char *logo, float larg) {
  static char anel[LOGO_ANEL][512];
  static int vez;
  char *buf;
  const char *p, *nome;
  const char *tam;
  // UM TAMANHO SO, para todo lugar (dono, 20/09/2026: "a arte do titulo
  // demora muito pra carregar e ja temos a arte no card — nao da para usar
  // uma em alta para tudo?"). Antes a escada w300/w500/w780/w1280 seguia a
  // largura pedida: o card aberto pedia w500, o heroi e o detalhe w1280 — tres
  // ARQUIVOS diferentes da mesma logo, e abrir o titulo baixava a w1280 do
  // zero mesmo com a w500 ja na tela. Agora a URL e a mesma em todos: o
  // primeiro a mostrar baixa uma vez, os outros reaproveitam o arquivo (a
  // decodificacao sobe de tamanho localmente, sem rede). O card paga um PNG
  // maior (~150 KB contra ~60), uma vez por titulo.
  (void)larg;
  if (qualidadeImg == 2) tam = "original";
  else tam = qualidadeImg == 0 ? "w500" : "w1280";
  if (!logo || !logo[0]) return logo;
  // SVG NAO DECODIFICA. Item com logo .svg pode ja estar gravado no cache do
  // catalogo (entrado antes do filtro da descoberta); devolver a url faria o
  // desenho pedir uma textura que morre em "resposta nao e imagem" e fica
  // FALHOU para sempre — e enquanto isso a caixa do logo ficava vazia em vez
  // de cair no nome escrito, porque para quem desenha "svg" e "carregando"
  // sao indistinguiveis. NULL devolve o fallback de sempre.
  { size_t n = strlen(logo);
    if (n > 4 && !strcmp(logo + n - 4, ".svg")) return NULL; }
  p = strstr(logo, "/t/p/");
  if (!p) return logo;                       // metahub, arquivo local, etc.
  nome = strchr(p + 5, '/');                 // pula o tamanho que veio
  if (!nome || !nome[1]) return logo;
  buf = anel[vez];
  vez = (vez + 1) % LOGO_ANEL;
  { size_t pre = (size_t)(p - logo);
    if (pre >= sizeof anel[0]) return logo;
    snprintf(buf, sizeof anel[0], "%.*s/t/p/%s%s", (int)pre, logo, tam, nome); }
  return buf;
}

typedef struct {
  char imdb[sizeof(((CatItem *)0)->imdb)];
  char tipo[sizeof(((CatItem *)0)->tipo)];
  char url[512];
  int ativo;
} LogoSessao;

static LogoSessao logoSessao;

static int logo_sessao_mesma(const CatItem *item) {
  return item && item->imdb[0] && logoSessao.ativo &&
         !strcmp(logoSessao.imdb, item->imdb) &&
         !strcmp(logoSessao.tipo, item->tipo);
}

static void logo_sessao_guardar(const char *url) {
  if (url && url[0]) snprintf(logoSessao.url, sizeof logoSessao.url, "%s", url);
}

void artehero_logo_sessao_iniciar(const CatItem *item) {
  const char *url;
  if (!item || !item->imdb[0]) {
    memset(&logoSessao, 0, sizeof logoSessao);
    return;
  }
  if (logo_sessao_mesma(item)) {
    // Só uma falha definitiva libera a seleção. Enquanto está pendente, a
    // tela continua apontando para a arte que já estava escolhida.
    if (logoSessao.url[0] && falhou(logoSessao.url)) logoSessao.url[0] = 0;
  } else {
    memset(&logoSessao, 0, sizeof logoSessao);
    logoSessao.ativo = 1;
    snprintf(logoSessao.imdb, sizeof logoSessao.imdb, "%s", item->imdb);
    snprintf(logoSessao.tipo, sizeof logoSessao.tipo, "%s", item->tipo);
  }
  if (!logoSessao.url[0] && item->logo[0]) {
    url = artehero_url_logo(item->logo);
    if (url && !falhou(url)) logo_sessao_guardar(url);
  }
}

const char *artehero_logo_sessao(const CatItem *item) {
  const char *url;
  if (!item || !item->logo[0])
    return logo_sessao_mesma(item) && logoSessao.url[0] &&
                   !falhou(logoSessao.url) ? logoSessao.url : NULL;
  if (logo_sessao_mesma(item)) {
    if (logoSessao.url[0] && !falhou(logoSessao.url)) return logoSessao.url;
    logoSessao.url[0] = 0;
    url = artehero_url_logo(item->logo);
    if (url && !falhou(url)) logo_sessao_guardar(url);
    return url;
  }
  // A Home sem uma sessão aberta apenas normaliza o item corrente; não o
  // transforma em snapshot, para um card vizinho nunca roubar a identidade.
  return artehero_url_logo(item->logo);
}

const char *artehero_logo_sessao_observar(const CatItem *item) {
  if (!item || !item->imdb[0])
    return item && item->logo[0] ? artehero_url_logo(item->logo) : NULL;
  if (!logo_sessao_mesma(item)) {
    memset(&logoSessao, 0, sizeof logoSessao);
    logoSessao.ativo = 1;
    snprintf(logoSessao.imdb, sizeof logoSessao.imdb, "%s", item->imdb);
    snprintf(logoSessao.tipo, sizeof logoSessao.tipo, "%s", item->tipo);
  }
  if (logoSessao.url[0] && !falhou(logoSessao.url)) return logoSessao.url;
  logoSessao.url[0] = 0;
  if (item->logo[0]) {
    const char *url = artehero_url_logo(item->logo);
    if (url && !falhou(url)) logo_sessao_guardar(url);
    return url;
  }
  return NULL;
}

const char *artehero_logo_sessao_larg(const CatItem *item, float larg) {
  if (logo_sessao_mesma(item)) return artehero_logo_sessao(item);
  return item && item->logo[0] ? artehero_url_logo_larg(item->logo, larg) : NULL;
}
