// Descoberta local do Farol — ver farolacha.h.
// Puro: sem SDL, sem rede, sem dados.h, sem malloc, sem log, sem estaticos
// mutaveis.
#include "farolacha.h"
#include "cacavisto.h"
#include "faroltexto.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FA_PROG_JANELA 256
#define FA_SORT_CACHE  256

static unsigned char minusculaAscii(unsigned char c) {
  return (c >= 'A' && c <= 'Z') ? (unsigned char)(c - 'A' + 'a') : c;
}

static int igualSemCaixa(const char *a, const char *b) {
  if (!a || !b) return 0;
  for (; *a && *b; a++, b++)
    if (minusculaAscii((unsigned char)*a) != minusculaAscii((unsigned char)*b))
      return 0;
  return !*a && !*b;
}

static int prefixoSemCaixa(const char *s, const char *pref) {
  for (; *pref; s++, pref++)
    if (!*s || minusculaAscii((unsigned char)*s) != minusculaAscii((unsigned char)*pref))
      return 0;
  return 1;
}

static const char *pulaEspacos(const char *p) {
  while (*p == ' ' || *p == '\t') p++;
  return p;
}

// Rotulos de tipo que aparecem no primeiro segmento de CatItem.genero
// ("Filme · Crime · Drama", "Programa de TV · Action") e nao sao genero.
static int ehRotuloTipo(const char *seg) {
  static const char *const T[] = {
    "Filme", "Movie", "Film", "Filmas", "Película", "Phim", "Ταινία",
    "Филм", "Фильм", "Фільм", "فيلم", "映画", "电影", "電影",
    "Programa de TV", "TV Show", "Dizi", "Emisiune TV", "Phim bộ",
    "Serial", "Serial TV", "Serialas", "Serie", "Serie TV", "Serija",
    "Seriál", "Sorozat", "TV-Sendung", "TV-program", "Tv-program",
    "Émission de TV", "Σειρά", "Сериал", "Телепередача", "Телепрограма",
    "برنامج تلفزيوني", "テレビ番組", "电视节目", "電視節目",
    "Série", "Series", "Show", "Серіал", "シリーズ", "剧集", "劇集"
  };
  size_t i;
  for (i = 0; i < sizeof T / sizeof T[0]; i++)
    if (igualSemCaixa(seg, T[i])) return 1;
  return 0;
}

static int ehSerie(const CatItem *c) {
  return c && (!strcmp(c->tipo, "series") || !strcmp(c->tipo, "show"));
}

// Canais ao vivo e vagas vazias nao entram na descoberta de filmes/series.
static int itemElegivelBase(const CatItem *c) {
  if (!c) return 0;
  if (!c->titulo[0] && !c->imdb[0] && c->tmdb <= 0) return 0;
  if (!strcmp(c->tipo, "channel") || !strcmp(c->tipo, "channels") ||
      !strcmp(c->tipo, "tv") || !strcmp(c->tipo, "events"))
    return 0;
  if (!strncmp(c->imdb, "cs:channel:", 11)) return 0;
  return 1;
}

// Extrai o id da obra sem sufixo de episodio:
//   "tt1234567:2:4" -> "tt1234567"
//   "kitsu:41370:1:5" -> "kitsu:41370"
//   "tmdb:550" -> "tmdb:550"
static void idBaseDe(const char *src, char *dst, size_t tam) {
  size_t n;
  if (!tam) return;
  if (!src || !src[0]) { dst[0] = '\0'; return; }
  if (!strncmp(src, "tt", 2)) {
    const char *dp = strchr(src, ':');
    n = dp ? (size_t)(dp - src) : strlen(src);
  } else {
    const char *c2 = strrchr(src, ':');
    n = strlen(src);
    if (c2 && c2 > src && isdigit((unsigned char)c2[1])) {
      const char *p = c2 + 1;
      while (isdigit((unsigned char)*p)) p++;
      if (!*p) {
        const char *c1 = c2 - 1;
        while (c1 > src && isdigit((unsigned char)*c1)) c1--;
        if (*c1 == ':' && c1 > src && c1 + 1 < c2)
          n = (size_t)(c1 - src);
      }
    }
  }
  if (n >= tam) n = tam - 1;
  memcpy(dst, src, n);
  dst[n] = '\0';
}

static int anoInicialMeta(const char *meta) {
  if (!meta) return 0;
  meta = pulaEspacos(meta);
  if (isdigit((unsigned char)meta[0]) && isdigit((unsigned char)meta[1]) &&
      isdigit((unsigned char)meta[2]) && isdigit((unsigned char)meta[3]) &&
      !isdigit((unsigned char)meta[4]))
    return (meta[0] - '0') * 1000 + (meta[1] - '0') * 100 +
           (meta[2] - '0') * 10 + (meta[3] - '0');
  return 0;
}

static int mesmaObra(const CatItem *a, const CatItem *b) {
  char ia[64], ib[64];
  if (!a || !b) return 0;
  if (a == b) return 1;
  if (a->tipo[0] && b->tipo[0] && ehSerie(a) != ehSerie(b)) return 0;
  idBaseDe(a->imdb, ia, sizeof ia);
  idBaseDe(b->imdb, ib, sizeof ib);
  if (ia[0] && ib[0]) return !strcmp(ia, ib);
  if (a->tmdb > 0 && b->tmdb > 0) return a->tmdb == b->tmdb;
  if (!ia[0] && !ib[0] && a->tmdb <= 0 && b->tmdb <= 0 &&
      a->titulo[0] && b->titulo[0] && igualSemCaixa(a->titulo, b->titulo)) {
    int aa = anoInicialMeta(a->meta), ab = anoInicialMeta(b->meta);
    return (!aa || !ab || aa == ab);
  }
  return 0;
}

// ------------------------------------------------------------------ duracao --

static long lerNumeroCurto(const char **pp, int maxDig) {
  const char *p = *pp;
  long v = 0;
  int k = 0;
  while (isdigit((unsigned char)*p)) {
    if (++k > maxDig) return -1;
    v = v * 10 + (*p++ - '0');
  }
  if (!k) return -1;
  *pp = p;
  return v;
}

static int consomeUnidadeHoras(const char **pp) {
  static const char *const H[] = { "hours", "hour", "horas", "hora", "hrs", "hr" };
  const char *p = *pp;
  size_t i;
  for (i = 0; i < sizeof H / sizeof H[0]; i++) {
    size_t t = strlen(H[i]);
    if (prefixoSemCaixa(p, H[i]) && !isalpha((unsigned char)p[t])) {
      *pp = p + t;
      return 1;
    }
  }
  if ((p[0] == 'h' || p[0] == 'H') && !isalpha((unsigned char)p[1])) {
    *pp = p + 1;
    return 1;
  }
  return 0;
}

static int consomeUnidadeMin(const char **pp) {
  static const char *const M[] = {
    "minutes", "minute", "minutos", "minuto", "mins", "min", "m"
  };
  const char *p = *pp;
  size_t i;
  for (i = 0; i < sizeof M / sizeof M[0]; i++) {
    size_t t = strlen(M[i]);
    if (prefixoSemCaixa(p, M[i]) && !isalpha((unsigned char)p[t])) {
      *pp = p + t;
      return 1;
    }
  }
  return 0;
}

static int minutosEmFaixa(long min) {
  return (min >= 1 && min <= 1440) ? (int)min : 0;
}

static int lerDuracaoIso(const char *p) {
  long h = 0, m = 0, s = 0, v;
  int viuH = 0, viuM = 0, viuS = 0;
  if (!prefixoSemCaixa(p, "PT")) return 0;
  p += 2;
  while (*p) {
    v = lerNumeroCurto(&p, 5);
    if (v < 0) return 0;
    if (*p == 'H' || *p == 'h') {
      if (viuH || viuM || viuS) return 0;
      h = v; viuH = 1;
    } else if (*p == 'M' || *p == 'm') {
      if (viuM || viuS) return 0;
      m = v; viuM = 1;
    } else if (*p == 'S' || *p == 's') {
      if (viuS) return 0;
      s = v; viuS = 1;
    } else {
      return 0;
    }
    p++;
  }
  if (!viuH && !viuM && !viuS) return 0;
  if (!viuH && !viuM && s < 60) return 0;
  return minutosEmFaixa(h * 60 + m + (s >= 30 ? (s + 30) / 60 : s / 60));
}

static int lerDuracaoTrecho(const char *ini, size_t len) {
  char seg[64];
  const char *p;
  long v1, v2;
  while (len && (ini[0] == ' ' || ini[0] == '\t')) { ini++; len--; }
  while (len && (ini[len - 1] == ' ' || ini[len - 1] == '\t')) len--;
  if (!len || len >= sizeof seg) return 0;
  memcpy(seg, ini, len);
  seg[len] = '\0';

  if (prefixoSemCaixa(seg, "PT")) return lerDuracaoIso(seg);

  p = seg;
  v1 = lerNumeroCurto(&p, 4);
  if (v1 < 0) return 0;
  p = pulaEspacos(p);
  if (!*p) return 0; // numero solto em meta ("2024") e ano, nunca minutos

  if (consomeUnidadeHoras(&p)) {
    int colado = isdigit((unsigned char)*p);
    p = pulaEspacos(p);
    if (!*p) return minutosEmFaixa(v1 * 60);
    v2 = lerNumeroCurto(&p, 2);
    if (v2 < 0) return 0;
    p = pulaEspacos(p);
    if (!*p) {
      // Forma compacta de desc_duracao_min_curta ("1h40", "2h05").
      return (colado && v2 < 60) ? minutosEmFaixa(v1 * 60 + v2) : 0;
    }
    if (!consomeUnidadeMin(&p)) return 0;
    if (*pulaEspacos(p)) return 0;
    return minutosEmFaixa(v1 * 60 + v2);
  }

  if (consomeUnidadeMin(&p)) {
    if (*pulaEspacos(p)) return 0;
    return minutosEmFaixa(v1);
  }
  return 0;
}

static const char *proximoSeparadorMeta(const char *p, size_t *tamSep) {
  for (; *p; p++) {
    if ((unsigned char)p[0] == 0xc2 && (unsigned char)p[1] == 0xb7) {
      *tamSep = 2; // "·" UTF-8
      return p;
    }
    if ((unsigned char)p[0] == 0xe2 && (unsigned char)p[1] == 0x80 &&
        (unsigned char)p[2] == 0xa2) {
      *tamSep = 3; // "•" UTF-8
      return p;
    }
    if (*p == '|' || *p == ',' || *p == ';') {
      *tamSep = 1;
      return p;
    }
  }
  *tamSep = 0;
  return NULL;
}

int farolacha_minutos_meta(const char *meta) {
  const char *p = meta;
  if (!p || !*p) return 0;
  for (;;) {
    size_t tamSep = 0;
    const char *sep = proximoSeparadorMeta(p, &tamSep);
    size_t len = sep ? (size_t)(sep - p) : strlen(p);
    int min = lerDuracaoTrecho(p, len);
    if (min > 0) return min;
    if (!sep) return 0;
    p = sep + tamSep;
  }
}

// ------------------------------------------------------------------ generos --

static int proximoGeneroItem(const char **cursor, int *primeiro,
                             char *dst, size_t tam) {
  const char *p = *cursor;
  if (!p || !*p || !tam) return 0;
  while (*p) {
    const char *fim = p;
    size_t avanco = 0, len;
    while (*fim) {
      if ((unsigned char)fim[0] == 0xc2 && (unsigned char)fim[1] == 0xb7) {
        avanco = 2;
        break;
      }
      if (*fim == ',' || *fim == '|') {
        avanco = 1;
        break;
      }
      fim++;
    }
    len = (size_t)(fim - p);
    while (len && (p[0] == ' ' || p[0] == '\t')) { p++; len--; }
    while (len && (p[len - 1] == ' ' || p[len - 1] == '\t')) len--;
    *cursor = avanco ? fim + avanco : fim;
    if (!len) { p = *cursor; continue; }
    if (len >= tam) len = tam - 1;
    memcpy(dst, p, len);
    dst[len] = '\0';
    if (*primeiro) {
      *primeiro = 0;
      if (ehRotuloTipo(dst)) { p = *cursor; continue; }
    }
    return 1;
  }
  dst[0] = '\0';
  return 0;
}

static int itemTemGenero(const CatItem *c, const char *genero) {
  const char *cur;
  int primeiro = 1;
  char tok[64];
  if (!genero || !genero[0]) return 1;
  if (!c || !c->genero[0]) return 0;
  cur = c->genero;
  while (proximoGeneroItem(&cur, &primeiro, tok, sizeof tok))
    if (igualSemCaixa(tok, genero)) return 1;
  return 0;
}

// --------------------------------------------------------- visto / retomada --

static int consultaCacavisto(const CatItem *c, const char *idBase) {
  if (!c) return 0;
  if (idBase[0] || c->tmdb > 0) {
    if (cacavisto_filme(idBase, c->tmdb, NULL)) return 1;
  }
  if (c->imdb[0] && strcmp(idBase, c->imdb) != 0) {
    if (cacavisto_filme(c->imdb, c->tmdb, NULL)) return 1;
  }
  return 0;
}

static int itemVistoDireto(const CatItem *c, int caca) {
  if (caca) return 1;
  if (!c || c->rewatch) return 0;
  if (!ehSerie(c)) return c->progresso >= 90;
  return c->progresso >= 100 && c->retomadoMs <= 0;
}

static int itemComAtividade(const CatItem *c) {
  return c && (c->progresso > 0 || c->retomadoMs > 0 || c->rewatch != 0);
}

// Cruza tambem outras copias da mesma obra no catalogo (ex.: card em
// "Continuar assistindo" com progresso > 0 e copia em fileira com progresso 0).
static void inspecionarObra(const CatItem *c, int idx, int nCat,
                            const int *progIdx, int nProg, int progCheio,
                            int *vistoOuIniciado, int *vistoFinal) {
  char idBase[64];
  int caca, vf, vi, k;
  idBaseDe(c->imdb, idBase, sizeof idBase);
  caca = consultaCacavisto(c, idBase);
  vf = itemVistoDireto(c, caca);
  vi = caca || itemComAtividade(c);

  if (!progCheio) {
    for (k = 0; k < nProg; k++) {
      const CatItem *o;
      if (progIdx[k] == idx) continue;
      o = cat_item(progIdx[k]);
      if (!o || !mesmaObra(c, o)) continue;
      vi = 1;
      if (!c->rewatch && itemVistoDireto(o, 0)) vf = 1;
    }
  } else {
    for (k = 0; k < nCat; k++) {
      const CatItem *o;
      if (k == idx) continue;
      o = cat_item(k);
      if (!itemComAtividade(o) || !mesmaObra(c, o)) continue;
      vi = 1;
      if (!c->rewatch && itemVistoDireto(o, 0)) vf = 1;
    }
  }
  *vistoOuIniciado = vi;
  *vistoFinal = vf;
}

// ---------------------------------------------------- sorteio deterministico --

static uint32_t misturar32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x85ebca6bu;
  x ^= x >> 13;
  x *= 0xc2b2ae35u;
  x ^= x >> 16;
  return x;
}

static uint32_t hashTexto(uint32_t h, const char *s) {
  if (!s) return h;
  for (; *s; s++) {
    unsigned char c = minusculaAscii((unsigned char)*s);
    h ^= c;
    h *= 16777619u;
  }
  return h;
}

// Semente misturada com a identidade estavel da obra (nao com o indice bruto):
// se "Continuar assistindo" deslocar os indices do catalogo, repetir a mesma
// semente apos cat_revisao() mudar encontra as mesmas obras nos novos indices.
static uint32_t sorteioObra(const CatItem *c, FarolAchaTipo t, unsigned semente) {
  char idBase[64];
  uint32_t h = 2166136261u ^
               misturar32((uint32_t)semente + 0x9e3779b9u + ((uint32_t)t * 0x27d4eb2du));
  idBaseDe(c->imdb, idBase, sizeof idBase);
  h = hashTexto(h, ehSerie(c) ? "series" : "movie");
  if (idBase[0]) {
    h = hashTexto(h, idBase);
  } else if (c->tmdb > 0) {
    h ^= (uint32_t)c->tmdb;
    h = misturar32(h);
  } else {
    h = hashTexto(h, c->titulo);
    h ^= (uint32_t)anoInicialMeta(c->meta) * 2654435761u;
  }
  return misturar32(h);
}

static uint32_t pontosModo(const CatItem *c, FarolAchaTipo t, uint32_t sorteio) {
  uint32_t base, bonus = 0;
  if (t == FA_SURPRESA || t == FA_RETOMAR) return sorteio;
  base = sorteio >> 1; // [0, 2^31): a semente decide, nota/salvo so desempatam de leve
  if (c->nota > 0) {
    int nt = c->nota > 100 ? 100 : c->nota;
    bonus += (uint32_t)nt << 19; // ate ~52M (~2,4% da faixa)
  }
  if (c->naLista) bonus += 1u << 24;
  return base + bonus;
}

typedef struct {
  FarolAchado a;
  int         tier;
  int         progresso;
  int         temPoster;
  long long   retomadoMs;
  uint32_t    pontos;
  uint32_t    sorteio;
} CandInfo;

static CandInfo extrairCand(const FarolAchado *a, FarolAchaTipo t,
                            unsigned semente, const uint32_t *sortCache,
                            int pos) {
  CandInfo ci;
  const CatItem *c = cat_item(a->indice);
  memset(&ci, 0, sizeof ci);
  ci.a = *a;
  ci.tier = (a->motivo & FA_MOT_NAO_VI) ? 1 : 0;
  if (c) {
    ci.progresso = c->progresso;
    ci.temPoster = c->poster[0] != '\0';
    ci.retomadoMs = c->retomadoMs;
    ci.sorteio = (pos >= 0 && pos < FA_SORT_CACHE)
                   ? sortCache[pos]
                   : sorteioObra(c, t, semente);
    ci.pontos = pontosModo(c, t, ci.sorteio);
  }
  return ci;
}

// Ordem estrita (irreflexiva e transitiva) entre duas obras distintas.
static int candMelhor(const CandInfo *x, const CandInfo *y, FarolAchaTipo t) {
  if (t == FA_RETOMAR) {
    int xr = x->retomadoMs > 0, yr = y->retomadoMs > 0;
    if (xr != yr) return xr > yr;
    if (xr && x->retomadoMs != y->retomadoMs) return x->retomadoMs > y->retomadoMs;
    if (x->sorteio != y->sorteio) return x->sorteio > y->sorteio;
    if (x->progresso != y->progresso) return x->progresso > y->progresso;
    return x->a.indice < y->a.indice;
  }
  if (x->tier != y->tier) return x->tier > y->tier;
  if (x->pontos != y->pontos) return x->pontos > y->pontos;
  if (x->sorteio != y->sorteio) return x->sorteio > y->sorteio;
  return x->a.indice < y->a.indice;
}

// Entre duas copias da MESMA obra no catalogo, fica a mais rica/recente.
static int copiaMelhorDaMesmaObra(const CandInfo *novo, const CandInfo *atual,
                                  FarolAchaTipo t) {
  if (t == FA_RETOMAR) {
    if (novo->retomadoMs != atual->retomadoMs)
      return novo->retomadoMs > atual->retomadoMs;
    if (novo->progresso != atual->progresso)
      return novo->progresso > atual->progresso;
    if ((novo->a.minutos > 0) != (atual->a.minutos > 0))
      return novo->a.minutos > 0;
    return 0;
  }
  if (novo->tier != atual->tier) return novo->tier > atual->tier;
  if ((novo->a.minutos > 0) != (atual->a.minutos > 0))
    return novo->a.minutos > 0;
  if (novo->temPoster != atual->temPoster) return novo->temPoster > atual->temPoster;
  return 0;
}

static void trocarVaga(FarolAchado *out, uint32_t *sortCache, int p, int q,
                       uint32_t sP, uint32_t sQ) {
  FarolAchado tmp = out[p];
  out[p] = out[q];
  out[q] = tmp;
  if (p < FA_SORT_CACHE) sortCache[p] = sQ;
  if (q < FA_SORT_CACHE) sortCache[q] = sP;
}

// ---------------------------------------------------------- API principal --

const char *farolacha_rotulo(FarolAchaTipo t) {
  switch (t) {
    case FA_CURTO:    return FT_INTENCAO_CURTO;
    case FA_NAO_VI:   return FT_INTENCAO_NAO_VI;
    case FA_RETOMAR:  return FT_INTENCAO_RETOMAR;
    case FA_SURPRESA: return FT_INTENCAO_SURPRESA;
    default:          return "";
  }
}

const CatItem *farolacha_item(const FarolAchado *a, unsigned revisao) {
  int n;
  const CatItem *c;
  if (!a || a->indice < 0) return NULL;
  if (cat_revisao() != revisao) return NULL;
  n = cat_n();
  if (a->indice >= n) return NULL;
  c = cat_item(a->indice);
  if (!c || cat_revisao() != revisao) return NULL;
  return c;
}

int farolacha_genero_comum(const FarolAchado *achados, int n, unsigned revisao,
                           char *dst, size_t tam) {
  const CatItem *c0;
  const char *cur;
  int primeiro = 1;
  char cand[64];
  if (dst && tam) dst[0] = '\0';
  if (!achados || n < 1 || !dst || !tam) return 0;
  c0 = farolacha_item(&achados[0], revisao);
  if (!c0 || !c0->genero[0]) return 0;
  cur = c0->genero;
  while (proximoGeneroItem(&cur, &primeiro, cand, sizeof cand)) {
    int k, todos = 1;
    for (k = 1; k < n; k++) {
      const CatItem *ck = farolacha_item(&achados[k], revisao);
      if (!ck || !itemTemGenero(ck, cand)) { todos = 0; break; }
    }
    if (todos) {
      snprintf(dst, tam, "%s", cand);
      return 1;
    }
  }
  return 0;
}

int farolacha_listar_genero(FarolAchaTipo t, int maxMin, const char *genero,
                            FarolAchado *out, int max, unsigned semente,
                            unsigned *revisaoOut) {
  int tentativa;
  if (revisaoOut) *revisaoOut = cat_revisao();
  if (!out || max < 1) return 0;
  if (t != FA_CURTO && t != FA_NAO_VI && t != FA_RETOMAR && t != FA_SURPRESA)
    return 0;

  for (tentativa = 0; tentativa < 3; tentativa++) {
    unsigned rev = cat_revisao();
    int nCat = cat_n();
    int progIdx[FA_PROG_JANELA], nProg = 0, progCheio = 0;
    uint32_t sortCache[FA_SORT_CACHE];
    int limCurto = maxMin > 0 ? maxMin : FAROL_ACHA_CURTO_PADRAO;
    int i, k = 0;

    if (nCat <= 0) {
      if (cat_revisao() == rev) {
        if (revisaoOut) *revisaoOut = rev;
        return 0;
      }
      continue;
    }

    for (i = 0; i < nCat; i++) {
      const CatItem *c = cat_item(i);
      if (!itemElegivelBase(c) || !itemComAtividade(c)) continue;
      if (nProg < FA_PROG_JANELA) progIdx[nProg++] = i;
      else { progCheio = 1; break; }
    }

    for (i = 0; i < nCat; i++) {
      const CatItem *c = cat_item(i);
      int durMeta, minutos = 0, vistoOuIniciado = 0, vistoFinal = 0;
      int motivo = FA_MOT_NENHUM, posExistente = -1, p;
      CandInfo novo;

      if (!itemElegivelBase(c)) continue;
      if (!itemTemGenero(c, genero)) continue;

      durMeta = farolacha_minutos_meta(c->meta);
      inspecionarObra(c, i, nCat, progIdx, nProg, progCheio,
                      &vistoOuIniciado, &vistoFinal);

      if (t == FA_NAO_VI) {
        if (vistoOuIniciado) continue;
        minutos = durMeta > 0 ? durMeta
                : (c->progresso == 0 && c->restanteMin > 0 ? c->restanteMin : 0);
        motivo = FA_MOT_NAO_VI;
      } else if (t == FA_CURTO) {
        minutos = durMeta > 0 ? durMeta
                : (c->progresso == 0 && c->restanteMin > 0 ? c->restanteMin : 0);
        if (minutos <= 0 || minutos > limCurto) continue;
        motivo = FA_MOT_CURTO | (!vistoOuIniciado ? FA_MOT_NAO_VI : 0);
      } else if (t == FA_RETOMAR) {
        int pode = c->rewatch ? (c->progresso > 0 && c->progresso < 100)
                              : (!vistoFinal && (c->progresso > 0 || c->retomadoMs > 0));
        if (!pode) continue;
        if (c->restanteMin > 0) {
          minutos = c->restanteMin;
        } else if (durMeta > 0 && c->progresso > 0 && c->progresso < 100) {
          minutos = (durMeta * (100 - c->progresso) + 50) / 100;
          if (minutos < 1) minutos = 1;
        } else {
          minutos = durMeta;
        }
        motivo = FA_MOT_RETOMAR;
      } else { // FA_SURPRESA
        minutos = durMeta > 0 ? durMeta
                : (c->progresso == 0 && c->restanteMin > 0 ? c->restanteMin : 0);
        motivo = FA_MOT_SURPRESA | (!vistoOuIniciado ? FA_MOT_NAO_VI : 0);
      }

      if (c->naLista) motivo |= FA_MOT_SALVO;
      if (c->nota >= 75) motivo |= FA_MOT_NOTA;

      memset(&novo, 0, sizeof novo);
      novo.a.indice = i;
      novo.a.minutos = minutos;
      novo.a.motivo = motivo;
      novo.tier = (motivo & FA_MOT_NAO_VI) ? 1 : 0;
      novo.progresso = c->progresso;
      novo.temPoster = c->poster[0] != '\0';
      novo.retomadoMs = c->retomadoMs;
      novo.sorteio = sorteioObra(c, t, semente);
      novo.pontos = pontosModo(c, t, novo.sorteio);

      for (p = 0; p < k; p++) {
        const CatItem *ce = cat_item(out[p].indice);
        if (ce && mesmaObra(c, ce)) { posExistente = p; break; }
      }

      if (posExistente >= 0) {
        CandInfo atual = extrairCand(&out[posExistente], t, semente,
                                     sortCache, posExistente);
        if (!copiaMelhorDaMesmaObra(&novo, &atual, t)) continue;
        out[posExistente] = novo.a;
        if (posExistente < FA_SORT_CACHE) sortCache[posExistente] = novo.sorteio;
        p = posExistente;
        while (p > 0) {
          CandInfo ant = extrairCand(&out[p - 1], t, semente, sortCache, p - 1);
          if (!candMelhor(&novo, &ant, t)) break;
          trocarVaga(out, sortCache, p, p - 1, novo.sorteio, ant.sorteio);
          p--;
        }
        while (p + 1 < k) {
          CandInfo prox = extrairCand(&out[p + 1], t, semente, sortCache, p + 1);
          if (!candMelhor(&prox, &novo, t)) break;
          trocarVaga(out, sortCache, p, p + 1, novo.sorteio, prox.sorteio);
          p++;
        }
        continue;
      }

      if (k == max) {
        CandInfo pior = extrairCand(&out[k - 1], t, semente, sortCache, k - 1);
        if (!candMelhor(&novo, &pior, t)) continue;
        p = k - 1;
      } else {
        p = k++;
      }
      out[p] = novo.a;
      if (p < FA_SORT_CACHE) sortCache[p] = novo.sorteio;
      while (p > 0) {
        CandInfo ant = extrairCand(&out[p - 1], t, semente, sortCache, p - 1);
        if (!candMelhor(&novo, &ant, t)) break;
        trocarVaga(out, sortCache, p, p - 1, novo.sorteio, ant.sorteio);
        p--;
      }
    }

    if (cat_revisao() == rev) {
      int nFim = cat_n(), ok = 1;
      for (i = 0; i < k; i++)
        if (out[i].indice < 0 || out[i].indice >= nFim || !cat_item(out[i].indice))
          { ok = 0; break; }
      if (ok) {
        if (revisaoOut) *revisaoOut = rev;
        return k;
      }
    }
    // O catalogo trocou no meio da leitura: descarta os indices desta volta.
    if (k > 0) memset(out, 0, sizeof(*out) * (size_t)k);
  }

  if (revisaoOut) *revisaoOut = cat_revisao();
  return 0;
}

int farolacha_listar_rev(FarolAchaTipo t, int maxMin, FarolAchado *out, int max,
                         unsigned semente, unsigned *revisaoOut) {
  return farolacha_listar_genero(t, maxMin, NULL, out, max, semente, revisaoOut);
}

int farolacha_listar(FarolAchaTipo t, int maxMin, FarolAchado *out, int max,
                     unsigned semente) {
  return farolacha_listar_genero(t, maxMin, NULL, out, max, semente, NULL);
}
