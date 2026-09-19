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

static int (*jaFalhou)(const char *) = NULL;
void artehero_definir_falhou(int (*falhou)(const char *caminho)) {
  jaFalhou = falhou;
}
static int falhou(const char *u) { return u && jaFalhou && jaFalhou(u); }

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

// FUNDO DE CARD EM w780, quando a url e do TMDB.
//
// A descoberta guarda o fundo como w1280 (1280x720). O card deitado desenha
// 736 px, 768 no foco: w780 (780x439) cobre os dois 1:1, com 2,7x menos
// pixels para baixar e decodificar — MEDIDO na C9 em 19/09: cada w1280 custava
// 300 a 430 ms no fio de decode, e uma fileira sao vinte deles. A url da
// descoberta nao muda (o heroi e o detalhe continuam subindo dela para a
// versao grande, ver artehero_url); so o card pede menor. Ponteiro estatico,
// como artehero_url.
// ANEL de 64 e nao um buffer so: home.c guarda o ponteiro do card FOCADO em
// itemFoco.arte e app.c o le depois de a fileira inteira ter sido desenhada —
// com um buffer, ele apontaria para a url do ultimo card da fileira.
#define CARD_ANEL 64
const char *artehero_url_card_deitado(const CatItem *item) {
  static char anel[CARD_ANEL][512];
  static int vez;
  char *buf;
  const char *b, *p;
  if (!item) return NULL;
  b = item->backdrop[0] ? item->backdrop : item->poster[0] ? item->poster : NULL;
  if (!b) return NULL;
  if (qualidadeImg == 2) return b;   // alta: o que o catalogo guardou, inteiro
  p = strstr(b, "/t/p/w1280/");
  if (!p) return b;
  buf = anel[vez]; vez = (vez + 1) % CARD_ANEL;
  { size_t pre = (size_t)(p - b);
    if (pre >= sizeof anel[0] - 64) return b;
    snprintf(buf, sizeof anel[0], "%.*s/t/p/w780/%s", (int)pre, b, p + strlen("/t/p/w1280/")); }
  return buf;
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
    const char *tam = (qualidadeImg == 2) ? "original" : "w1280";
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
  // BAIXA: a url guardada, sem subir de tamanho. O fundo montado por id (o
  // caso "nao ha fundo nenhum") continua valendo — ali a alternativa e o cartaz
  // esticado, que nao e mais leve, e sim mais feio.
  if (qualidadeImg == 0 && item->backdrop[0]) return item->backdrop;
  // METAHUB PRIMEIRO, QUANDO HA ID DO IMDB. Medido: o fundo do metahub e
  // 1920x1080 e pesa ~850 KB; o `original` do TMDB para o mesmo titulo e
  // 3840x2160. Os dois terminam desenhados a 1920 — o teto do cache reduz o
  // segundo — entao o TMDB custa QUATRO VEZES os pixels de decodificacao para
  // chegar ao mesmo lugar. Numa Mali-G71 isso e a diferenca entre a arte
  // aparecer e a arte demorar, e foi o que o dono viu depois da primeira
  // versao desta politica ("ta demorando bem mais para carregar as artes").
  //
  // Nem todo tt tem fundo no metahub; quando o cache disser que falhou, a
  // proxima chamada cai na url guardada (e no `original`, se for TMDB).
  if (!strncmp(item->imdb, "tt", 2)) {
    char id[32];
    idLimpo(item->imdb, id, sizeof id);
    snprintf(buf, sizeof buf,
             "https://images.metahub.space/background/medium/%s/img", id);
    if (!falhou(buf)) return buf;
  }

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
  if (qualidadeImg == 2) tam = "original";
  else if (larg <= 0.0f) tam = qualidadeImg == 0 ? "w500" : "w1280";
  else if (larg <= 300.0f) tam = "w300";
  else if (larg <= 500.0f) tam = qualidadeImg == 0 ? "w300" : "w500";
  else if (larg <= 780.0f) tam = qualidadeImg == 0 ? "w500" : "w780";
  else tam = qualidadeImg == 0 ? "w780" : "w1280";
  if (!logo || !logo[0]) return logo;
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
