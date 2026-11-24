// Implementacao de faroldetalhe.h — issue 46 (PLANO-GOSTO secoes 4.2-4.5).
//
// Regras do contrato:
//   1. Bloco nasce no exato quadro em que a ficha do titulo existe.
//   2. `qualidade` (P6) entra com fade quando as fontes confirmadas chegam
//      ate 8 s da abertura; apos 8 s nunca entra (pagina nao muda sob leitura).
//   3. Sem motivo (P3) nem informacao (P6), altura == 0.0f e zero desenho:
//      a pagina do titulo permanece identica, pixel a pixel.
//   4. Selos (ate 3: pessoa, tema, genero) focaveis sem beco; sem selos o
//      bloco nao aceita foco.
//   5. Animacoes reduzidas desligam qualquer fade ou mola.
//   6. Frases exclusivamente de src/faroltexto.h + i18n(); nenhum log de
//      titulo, perfil ou gosto.
#include "faroldetalhe.h"
#include "farolcena.h"
#include "faroltexto.h"
#include "farolsinal.h"
#include "ajustes.h"
#include "amigostitulo.h"
#include "anim.h"
#include "badges.h"
#include "catalogo.h"
#include "diario.h"
#include "extras.h"
#include "fonteantecipa.h"
#include "gfx.h"
#include "horafmt.h"
#include "idbase.h"
#include "idioma.h"
#include "idiomacod.h"
#include "linguas.h"
#include "mapa.h"
#include "ondever.h"
#include "perfilinfantil.h"
#include "perfis.h"
#include "plrui.h"
#include "reacao.h"
#include "recomenda.h"
#include "salvos.h"
#include "seguro.h"
#include "streams.h"
#include "text.h"
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define FD_DIA_EPOCA   1767225600LL   // 2026-01-01 00:00:00 UTC (igual farolsinal)
#define FD_PAD_X       24.0f
#define FD_PAD_Y       16.0f
#define FD_RAIO        24.0f
#define FD_KICK_H      20.0f
#define FD_KICK_GAP    10.0f
#define FD_LINHA_H     26.0f
#define FD_LINHA_GAP   4.0f
#define FD_SELO_TOP    10.0f
#define FD_SELO_H      38.0f
#define FD_SELO_PAD_X  16.0f
#define FD_SELO_GAP    10.0f
#define FD_COL_GAP     36.0f
#define FD_SEM_MAX     16
#define FD_FONTE_MAX   24
#define FD_TEMP_MAX    24
#define FD_TESTE_MAX   24

typedef struct {
  uint8_t tipo;          // FP_SELO_*
  int32_t id;
  long    tmdb;          // > 0 para abrir detail_mostrar_pessoa direto
  int     elencoCol;     // >= 0 se presente em CatItem.elenco
  char    rotulo[80];
  char    foto[256];
  char    fio[64];       // nome cru p/ o Mapa (kw de tema ou genero local)
  float   larg;          // medida 1x por cena (secao 4.4)
} FdSelo;

typedef struct {
  uint8_t k;             // FP_INFO_*
  char    texto[192];
} FdInfo;

typedef struct {
  int     visivel;
  int     nPorque;
  char    porque[FAROL_LINHAS_MAX][192];
  int     nSelos;
  FdSelo  selos[FAROL_SELA_MAX];
  int     nInfo;
  FdInfo  info[FAROL_INFO_MAX];
  int     temQualidade;
  int     soQualidade;   // 1 quando o bloco inteiro depende da linha qualidade
  float   qualAlpha;
  float   blocoAlpha;
  int     focado;
  int     seloAtual;
  float   seloFoco[FAROL_SELA_MAX];
  float   bordaX;
} FdSnap;

typedef struct {
  char       imdb[24];
  FarolFicha ficha;
} FdTesteFicha;

typedef struct {
  char       imdb[24];
  char       titulo[128];
  FarolFicha ficha;
  int        terminou;
} FdTesteSem;

typedef struct {
  int     tipo;          // FP_SELO_PESSOA / TEMA / GENERO ou -1 = amigo ("u<id>")
  int32_t id;
  int     elencoCol;
  char    nome[80];
  char    foto[256];
} FdTesteVocab;

static pthread_mutex_t fd_trava = PTHREAD_MUTEX_INITIALIZER;
#define FD_LOCK()   pthread_mutex_lock(&fd_trava)
#define FD_UNLOCK() pthread_mutex_unlock(&fd_trava)

// Scratch de validacao guardado fora da pilha de UI (~34 KB estaticos sob trava).
static FarolCena s_cenaTmp;
static char      s_jsonTmp[FAROL_CENA_MAX + 1];

// Copia cacheada do retrato do Mapa / MapaVizinhos (atualizada fora de fd_trava).
static pthread_mutex_t fd_mapaMu = PTHREAD_MUTEX_INITIALIZER;
static Mapa         s_mapaCache;
static unsigned     s_mapaRev;
static MapaVizinhos s_vizCache;
static unsigned     s_vizRev;
// catIndice resolvido de cada semente do retrato: a busca por IMDb e linear,
// entao so se refaz quando o catalogo ou a geracao do retrato muda (FAROL46C).
static unsigned     s_semCatRev, s_semMapaRev;
static int          s_semIdx[MAPA_SEM_MAX], s_semIdxN;

static struct {
  int       aberto;
  int       catIdx;
  char      imdb[24];
  unsigned  diaGer;      // diario: geracao consultada (0 = nada consultado)
  char      diaImdb[24]; // diario: titulo consultado (base)
  int       diaTemTit, diaTemEp;
  DiarioTitulo diaTit;
  int       diaRetT, diaRetE;
  long long diaRetQ;
  Uint32    aberturaMs;
  Uint32    ultMs;
  uint64_t  assinatura;
  int       qualEntrou;
  int       qualBloqueada;
  int       cenaManual;       // 1 quando alimentado por faroldetalhe_teste_json
  int       idiomaMedido;
  int       seloLembrado;
  AnimBorda borda;
  FdSnap    snap;

  long long (*fnEpoch)(void);
  Uint32    (*fnMs)(void);
  int          temModeloTeste;
  FarolGosto   modeloTeste;
  FdTesteFicha tFichas[FD_TESTE_MAX]; int nTFichas;
  FdTesteSem   tSems[FD_SEM_MAX];     int nTSems;
  FdTesteVocab tVocab[FD_TESTE_MAX];  int nTVocab;
  int            temFontesTeste;
  char           alvoFontesTeste[64];
  FarolInfoFonte fontesTeste[FD_FONTE_MAX];
  int            nFontesTeste;
  FarolInfoCaps  capsTeste;
} S;

// ------------------------------------------------- tabela de modelos FT_*
static const struct { const char *nome; const char *lit; } FD_MODS[] = {
#define FD_MOD_X(nome) { "FT_" #nome, FT_##nome },
  FAROL_FRASES(FD_MOD_X)
#undef FD_MOD_X
};
#define FD_MODS_N (sizeof FD_MODS / sizeof *FD_MODS)

static const char *fd_modelo_lit(const char *nome) {
  size_t i;
  if (!nome || !nome[0]) return NULL;
  for (i = 0; i < FD_MODS_N; i++)
    if (!strcmp(FD_MODS[i].nome, nome)) return FD_MODS[i].lit;
  return NULL;
}

static int fd_reduzida(void) {
  return anim_politica_reduzida || ajustes_animacoes_reduzidas();
}

static long long fd_agora_s(void) {
  return S.fnEpoch ? S.fnEpoch() : (long long)time(NULL);
}

static Uint32 fd_agora_ms(Uint32 fallback) {
  return S.fnMs ? S.fnMs() : fallback;
}

static uint64_t fd_fnv(uint64_t h, const void *buf, size_t n) {
  const unsigned char *p = (const unsigned char *)buf;
  size_t i;
  if (!h) h = 1469598103934665603ULL;
  for (i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ULL; }
  return h;
}

static uint16_t fd_dia_de(long long epochS) {
  long long d = (epochS - FD_DIA_EPOCA) / 86400LL;
  if (d < 0) d = 0;
  if (d > 65535) d = 65535;
  return (uint16_t)d;
}

static void fd_tempo_partes(long long epochS, struct tm *tmOut,
                            uint8_t *faixaOut, uint8_t *fdsOut) {
  time_t t = (time_t)epochS;
  int p, fds;
  memset(tmOut, 0, sizeof *tmOut);
  localtime_r(&t, tmOut);
  p = tmOut->tm_hour < 6 ? 3 : tmOut->tm_hour < 12 ? 0 :
      tmOut->tm_hour < 18 ? 1 : 2;
  fds = (tmOut->tm_wday == 0 || tmOut->tm_wday == 6) ? 1 : 0;
  *faixaOut = (uint8_t)((fds ? 4 : 0) + p);
  *fdsOut = (uint8_t)fds;
}

static int fd_eh_canal(const CatItem *ci) {
  if (!ci || !ci->imdb[0]) return 1;
  if (!strncmp(ci->imdb, "cs:", 3) || !strncmp(ci->imdb, "pp-live:", 8)) return 1;
  if (!strcasecmp(ci->tipo, "channel") || !strcasecmp(ci->tipo, "live")) return 1;
  return 0;
}

static int fd_ficha_tem_dados(const FarolFicha *f) {
  if (!f) return 0;
  return f->genero[0] != 0 || f->pessoa[0] != 0 || f->tema[0] != 0 ||
         f->minutos > 0 || f->ano > 0;
}

// ------------------------------------------------- resolucao de argumentos
static const char *fd_genero_tmdb_pt(int32_t id) {
  static const struct { int32_t id; const char *pt; } TAB[] = {
    { 28, "Ação" }, { 12, "Aventura" }, { 16, "Animação" }, { 35, "Comédia" },
    { 80, "Crime" }, { 99, "Documentário" }, { 18, "Drama" }, { 10751, "Família" },
    { 14, "Fantasia" }, { 36, "História" }, { 27, "Terror" }, { 10402, "Música" },
    { 9648, "Mistério" }, { 10749, "Romance" }, { 878, "Ficção científica" },
    { 53, "Suspense" }, { 10752, "Guerra" }, { 37, "Faroeste" },
    { 10759, "Ação" }, { 10762, "Infantil" }, { 10763, "Notícias" },
    { 10764, "Reality" }, { 10765, "Ficção científica" }, { 10766, "Novela" },
    { 10767, "Talk show" }, { 10768, "Guerra" }
  };
  size_t i;
  for (i = 0; i < sizeof TAB / sizeof *TAB; i++)
    if (TAB[i].id == id) return TAB[i].pt;
  return NULL;
}

typedef struct {
  const CatItem     *ci;
  const Mapa        *mapa;
  const MapaVizinhos *mv;
  int                temMvAlvo;
  AmigosTitulo       amg;
  int                temAmg;
  char               semImdb[FD_SEM_MAX][24];
  char               semTitulo[FD_SEM_MAX][128];
  int                nSemNomes;
} FdResolCtx;

static int fd_resolver_titulo(const FdResolCtx *rc, const char *ref,
                              char *dst, size_t cap) {
  int i, idx;
  if (!ref || !ref[0] || !dst || !cap) return 0;
  dst[0] = 0;
  for (i = 0; i < S.nTSems; i++)
    if (!strcmp(S.tSems[i].imdb, ref) && S.tSems[i].titulo[0]) {
      snprintf(dst, cap, "%s", S.tSems[i].titulo);
      return 1;
    }
  if (rc) {
    if (rc->ci && (!strcmp(ref, rc->ci->imdb) || !strcmp(ref, "c0")) &&
        rc->ci->titulo[0]) {
      snprintf(dst, cap, "%s", rc->ci->titulo);
      return 1;
    }
    for (i = 0; i < rc->nSemNomes; i++)
      if (!strcmp(rc->semImdb[i], ref) && rc->semTitulo[i][0]) {
        snprintf(dst, cap, "%s", rc->semTitulo[i]);
        return 1;
      }
    if (rc->mapa) {
      for (i = 0; i < rc->mapa->nSem; i++)
        if (!strcmp(rc->mapa->sem[i].imdb, ref) && rc->mapa->sem[i].titulo[0]) {
          snprintf(dst, cap, "%s", rc->mapa->sem[i].titulo);
          return 1;
        }
    }
  }
  idx = cat_indice_por_imdb(ref);
  if (idx >= 0) {
    const CatItem *c = cat_item(idx);
    if (c && c->titulo[0]) { snprintf(dst, cap, "%s", c->titulo); return 1; }
  }
  // Carga fria de diario-nomes.txt fica fora do fio da UI (FAROL46B): o
  // fallback so entra quando o cache ja foi lido por outro caminho.
  if (diario_nomes_lido() && diario_nome(ref, dst, cap, NULL, NULL, 0) && dst[0])
    return 1;
  return 0;
}

// Identidade base: "tt9000002" e "tt9000002:1:2" sao a mesma obra.
static int fd_mesmo_id(const char *a, const char *b) {
  size_t na = idbase_len(a), nb = idbase_len(b);
  return na != 0 && na == nb && !strncmp(a, b, na);
}

static int fd_resolver_pessoa(const FdResolCtx *rc, int32_t id,
                              char *nome, size_t capNome,
                              char *foto, size_t capFoto,
                              long *tmdbOut, int *elencoColOut) {
  int i;
  if (nome && capNome) nome[0] = 0;
  if (foto && capFoto) foto[0] = 0;
  if (tmdbOut) *tmdbOut = id > 0 ? (long)id : 0;
  if (elencoColOut) *elencoColOut = -1;
  if (!id) return 0;

  for (i = 0; i < S.nTVocab; i++)
    if (S.tVocab[i].tipo == FP_SELO_PESSOA && S.tVocab[i].id == id) {
      if (nome && capNome) snprintf(nome, capNome, "%s", S.tVocab[i].nome);
      if (foto && capFoto) snprintf(foto, capFoto, "%s", S.tVocab[i].foto);
      if (elencoColOut) *elencoColOut = S.tVocab[i].elencoCol;
      return nome && nome[0];
    }

  if (rc && rc->ci) {
    const CatItem *ci = rc->ci;
    for (i = 0; i < ci->nElenco; i++) {
      long tid = ci->elenco[i].tmdb;
      int32_t hid = (int32_t)mapa_hash_nome(ci->elenco[i].nome);
      if ((id > 0 && tid == (long)id) || (id < 0 && hid == id)) {
        if (nome && capNome) snprintf(nome, capNome, "%s", ci->elenco[i].nome);
        if (foto && capFoto) snprintf(foto, capFoto, "%s", ci->elenco[i].foto);
        if (tmdbOut && tid > 0) *tmdbOut = tid;
        if (elencoColOut) *elencoColOut = i;
        return nome && nome[0];
      }
    }
    if (ci->direcao[0]) {
      char buf[256], *p = buf;
      snprintf(buf, sizeof buf, "%s", ci->direcao);
      while (p && *p) {
        char *v = strchr(p, ',');
        char *m = strstr(p, "\xc2\xb7");
        char *fim = (v && (!m || v < m)) ? v : m;
        int av = (fim && fim == m) ? 2 : 1;
        if (fim) *fim = 0;
        while (*p == ' ') p++;
        { size_t k = strlen(p); while (k && p[k - 1] == ' ') p[--k] = 0; }
        if (*p && (int32_t)mapa_hash_nome(p) == id) {
          if (nome && capNome) snprintf(nome, capNome, "%s", p);
          return nome && nome[0];
        }
        p = fim ? fim + av : NULL;
      }
    }
  }
  if (rc && rc->temMvAlvo && rc->mv) {
    for (i = 0; i < MAPA_VIZ_GRUPOS; i++) {
      const MapaVizGrupo *gr = &rc->mv->g[i];
      if (gr->tipo == MAPA_GR_PESSOA && gr->n > 0 &&
          (int32_t)gr->ref == id && gr->sub[0]) {
        if (nome && capNome) snprintf(nome, capNome, "%s", gr->sub);
        return 1;
      }
    }
  }
  if (rc && rc->mapa) {
    for (i = 0; i < rc->mapa->nFios; i++)
      if ((int32_t)rc->mapa->fios[i].id == id && rc->mapa->fios[i].nome[0]) {
        if (nome && capNome) snprintf(nome, capNome, "%s", rc->mapa->fios[i].nome);
        if (foto && capFoto) snprintf(foto, capFoto, "%s", rc->mapa->fios[i].foto);
        return 1;
      }
  }
  return 0;
}

static int fd_resolver_tema(const FdResolCtx *rc, int32_t id,
                            char *dst, size_t cap,
                            char *fio, size_t capFio) {
  int i;
  if (fio && capFio) fio[0] = 0;
  if (!dst || !cap || !id) return 0;
  dst[0] = 0;
  for (i = 0; i < S.nTVocab; i++)
    if (S.tVocab[i].tipo == FP_SELO_TEMA && S.tVocab[i].id == id) {
      const char *pt = mapa_tema_nome(S.tVocab[i].nome);
      snprintf(dst, cap, "%s", i18n(pt ? pt : S.tVocab[i].nome));
      // O fio leva o identificador do Mapa (kw ingles), nao o rotulo i18n.
      if (fio && capFio) snprintf(fio, capFio, "%s", S.tVocab[i].nome);
      return dst[0] != 0;
    }
  if (rc && rc->temMvAlvo && rc->mv) {
    for (i = 0; i < MAPA_VIZ_GRUPOS; i++) {
      const MapaVizGrupo *gr = &rc->mv->g[i];
      if (gr->tipo == MAPA_GR_TEMA && gr->n > 0 && (int32_t)gr->ref == id) {
        const char *pt = gr->sub[0] ? gr->sub : mapa_tema_nome(gr->kw);
        if (pt && pt[0]) {
          snprintf(dst, cap, "%s", i18n(pt));
          if (fio && capFio)
            snprintf(fio, capFio, "%s", gr->kw[0] ? gr->kw : gr->sub);
          return 1;
        }
      }
    }
  }
  return 0;
}

static int fd_resolver_genero(const FdResolCtx *rc, int32_t id,
                              char *dst, size_t cap,
                              char *fio, size_t capFio) {
  const char *pt;
  int i;
  if (fio && capFio) fio[0] = 0;
  if (!dst || !cap || !id) return 0;
  dst[0] = 0;
  for (i = 0; i < S.nTVocab; i++)
    if (S.tVocab[i].tipo == FP_SELO_GENERO && S.tVocab[i].id == id) {
      snprintf(dst, cap, "%s", i18n(S.tVocab[i].nome));
      if (fio && capFio) snprintf(fio, capFio, "%s", S.tVocab[i].nome);
      return dst[0] != 0;
    }
  pt = fd_genero_tmdb_pt(id);
  if (pt) {
    snprintf(dst, cap, "%s", i18n(pt));
    if (fio && capFio) snprintf(fio, capFio, "%s", pt);
    return 1;
  }
  if (rc && rc->ci && rc->ci->genero[0]) {
    MapaEtiqueta gen[8];
    int nGen = 0;
    mapa_generos_locais(rc->ci->genero, gen, &nGen, 8);
    for (i = 0; i < nGen; i++)
      if ((int32_t)gen[i].id == id && gen[i].nome[0]) {
        snprintf(dst, cap, "%s", i18n(gen[i].nome));
        if (fio && capFio) snprintf(fio, capFio, "%s", gen[i].nome);
        return 1;
      }
  }
  if (rc && rc->temMvAlvo && rc->mv && rc->mv->generos[0]) {
    MapaEtiqueta gen[8];
    int nGen = 0;
    char tmp[160];
    snprintf(tmp, sizeof tmp, "X \xc2\xb7 %s", rc->mv->generos);
    mapa_generos_locais(tmp, gen, &nGen, 8);
    for (i = 0; i < nGen; i++)
      if ((int32_t)gen[i].id == id && gen[i].nome[0]) {
        snprintf(dst, cap, "%s", i18n(gen[i].nome));
        if (fio && capFio) snprintf(fio, capFio, "%s", gen[i].nome);
        return 1;
      }
  }
  return 0;
}

static int32_t fd_hash_id31(const char *s) {
  uint64_t h = fd_fnv(0, s, s ? strlen(s) : 0);
  int32_t v = (int32_t)(h & 0x7fffffffU);
  return v ? v : 1;
}

static int fd_resolver_amigo(const FdResolCtx *rc, int32_t id,
                             char *dst, size_t cap) {
  int i;
  if (!dst || !cap || !id) return 0;
  dst[0] = 0;
  for (i = 0; i < S.nTVocab; i++)
    if (S.tVocab[i].tipo == -1 && S.tVocab[i].id == id) {
      snprintf(dst, cap, "%s", S.tVocab[i].nome);
      return dst[0] != 0;
    }
  if (rc && rc->temAmg) {
    for (i = 0; i < rc->amg.n; i++)
      if (fd_hash_id31(rc->amg.a[i].id) == id && rc->amg.a[i].nome[0]) {
        amigostitulo_primeiro_nome(rc->amg.a[i].nome, dst, cap);
        return dst[0] != 0;
      }
  }
  return 0;
}

static void fd_idioma_midia(const char *cod, char *dst, size_t cap) {
  char base[12];
  const char *nm;
  size_t i = 0;
  if (!dst || !cap) return;
  dst[0] = 0;
  if (!cod || !cod[0]) return;
  while (cod[i] && cod[i] != '-' && cod[i] != '_' && i + 1 < sizeof base) {
    base[i] = (char)tolower((unsigned char)cod[i]);
    i++;
  }
  base[i] = 0;
  if (!strcmp(base, "pob") || !strcmp(base, "ptb") || !strcmp(base, "br") ||
      !strcmp(base, "por"))
    snprintf(base, sizeof base, "pt");
  else if (!strcmp(base, "eng"))
    snprintf(base, sizeof base, "en");
  else if (!strcmp(base, "spa") || !strcmp(base, "esp"))
    snprintf(base, sizeof base, "es");
  nm = i18n(ling_nome(base[0] ? base : cod));
  snprintf(dst, cap, "%s", nm && nm[0] ? nm : cod);
}

static int fd_formatar_mes_ano(const char *s, char *dst, size_t cap) {
  static const char *const MES_PT[12] = {
    "janeiro", "fevereiro", "março", "abril", "maio", "junho",
    "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"
  };
  int ano = 0, mes = 0;
  if (!s || strlen(s) != 7 || s[4] != '-') return 0;
  if (!isdigit((unsigned char)s[0]) || !isdigit((unsigned char)s[1]) ||
      !isdigit((unsigned char)s[2]) || !isdigit((unsigned char)s[3]) ||
      !isdigit((unsigned char)s[5]) || !isdigit((unsigned char)s[6]))
    return 0;
  ano = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
  mes = (s[5] - '0') * 10 + (s[6] - '0');
  if (ano < 1900 || mes < 1 || mes > 12) return 0;
  idioma_mes_ano(ajustes_idioma(), i18n(MES_PT[mes - 1]), ano, dst, cap);
  return dst[0] != 0;
}

static int fd_formatar_decorrido(const char *s, char *dst, size_t cap) {
  char *fim = NULL;
  long n;
  const char *f;
  if (!s || !isdigit((unsigned char)s[0])) return 0;
  n = strtol(s, &fim, 10);
  if (n <= 0 || !fim || !fim[0]) return 0;
  if (!strcmp(fim, "min")) {
    f = i18n("%d min");
    snprintf(dst, cap, f, (int)n);
    return 1;
  }
  if (!strcmp(fim, "h")) {
    f = i18n("%d h");
    snprintf(dst, cap, f, (int)n);
    return 1;
  }
  if ((!strcmp(fim, "d") && n == 7) || (!strcmp(fim, "w") && n == 1)) {
    snprintf(dst, cap, "%s", i18n("7 dias"));
    return 1;
  }
  if (!strcmp(fim, "y") && n >= 2) {
    f = i18n("%d anos");
    snprintf(dst, cap, f, (int)n);
    return 1;
  }
  return 0;
}

static int fd_encaixar_fmt(const char *fmt, char args[FAROL_ARGS][128],
                           int nArgs, char *dst, size_t cap) {
  size_t o = 0;
  int ai = 0;
  const char *p = fmt;
  if (!fmt || !dst || !cap) return 0;
  while (*p && o + 1 < cap) {
    if (*p != '%') { dst[o++] = *p++; continue; }
    p++;
    if (*p == '%') { dst[o++] = *p++; continue; }
    while (*p == '-' || *p == '+' || *p == ' ' || *p == '0' || *p == '#') p++;
    while (isdigit((unsigned char)*p)) p++;
    if (*p == '.') { p++; while (isdigit((unsigned char)*p)) p++; }
    if (*p == 'l') p++;
    if (ai >= nArgs) return 0;
    if (*p == 'd' || *p == 'i') {
      char num[32];
      size_t k;
      snprintf(num, sizeof num, "%d", atoi(args[ai++]));
      k = strlen(num);
      if (o + k >= cap) k = cap - 1 - o;
      memcpy(dst + o, num, k);
      o += k;
      p++;
    } else if (*p == 's') {
      size_t k = strlen(args[ai]);
      if (o + k >= cap) k = cap - 1 - o;
      memcpy(dst + o, args[ai], k);
      o += k;
      ai++;
      p++;
    } else {
      return 0;
    }
  }
  dst[o] = 0;
  return o > 0;
}

static int fd_render_texto(const FarolCena *cena, const FarolTexto *t,
                           const FdResolCtx *rc, char *dst, size_t cap) {
  const char *m = t->m;
  const char *lit;
  char args[FAROL_ARGS][128];
  int nArgs = (int)t->nArgs, i;

  if (!dst || !cap) return 0;
  dst[0] = 0;
  if (t->usaX) {
    const char *x = farolpeca_x(cena, t);
    if (!x || !x[0]) return 0;
    snprintf(dst, cap, "%s", x);
    return 1;
  }
  if (!m[0]) return 0;

  memset(args, 0, sizeof args);
  for (i = 0; i < nArgs && i < FAROL_ARGS; i++)
    snprintf(args[i], sizeof args[i], "%s", t->a[i]);

  if (!strcmp(m, "FT_MOT_AMIGO") && nArgs >= 1 && args[0][0] == 'u') {
    if (!fd_resolver_amigo(rc, (int32_t)atol(args[0] + 1), args[0], sizeof args[0])) {
      m = "FT_MOT_AMIGO_ANON";
      nArgs = 0;
    }
  } else if (!strcmp(m, "FT_MOT_RECOMENDADO") && nArgs >= 1 && args[0][0] == 'u') {
    if (!fd_resolver_amigo(rc, (int32_t)atol(args[0] + 1), args[0], sizeof args[0])) {
      m = "FT_MOT_RECOM_ANON";
      nArgs = 0;
    }
  } else if (!strcmp(m, "FT_INFO_RETOMADA") && nArgs == 3) {
    if (!fd_formatar_decorrido(args[2], args[2], sizeof args[2])) {
      m = "FT_INFO_RETOMADA_SIMPLES";
      nArgs = 2;
    }
  } else if (!strcmp(m, "FT_MOT_SALVO") && nArgs == 1) {
    if (!fd_formatar_decorrido(args[0], args[0], sizeof args[0]))
      return 0;
  } else if ((!strcmp(m, "FT_INFO_REVER") || !strcmp(m, "FT_INFO_REVER_SIMPLES")) &&
             nArgs >= 1) {
    if (!fd_formatar_mes_ano(args[0], args[0], sizeof args[0]))
      return 0;
  } else if (!strcmp(m, "FT_INFO_QUALI") && nArgs == 3) {
    char idi[64];
    fd_idioma_midia(args[2], idi, sizeof idi);
    snprintf(args[2], sizeof args[2], "%s", idi);
  } else if (!strcmp(m, "FT_INFO_QUALI_AUDIO") && nArgs == 2) {
    char idi[64];
    fd_idioma_midia(args[1], idi, sizeof idi);
    snprintf(args[1], sizeof args[1], "%s", idi);
  } else if ((!strcmp(m, "FT_INFO_IDIOMA") || !strcmp(m, "FT_INFO_LEGENDA")) &&
             nArgs == 1) {
    char idi[64];
    fd_idioma_midia(args[0], idi, sizeof idi);
    snprintf(args[0], sizeof args[0], "%s", idi);
  }

  for (i = 0; i < nArgs; i++) {
    const char *a = t->a[i];
    if ((a[0] == 't' && a[1] == 't' && isdigit((unsigned char)a[2])) ||
        (a[0] == 'c' && isdigit((unsigned char)a[1]))) {
      if (!fd_resolver_titulo(rc, a, args[i], sizeof args[i])) return 0;
    } else if (a[0] == 'p' && (isdigit((unsigned char)a[1]) || a[1] == '-')) {
      if (!fd_resolver_pessoa(rc, (int32_t)atol(a + 1),
                              args[i], sizeof args[i], NULL, 0, NULL, NULL))
        return 0;
    } else if (a[0] == 'k' && (isdigit((unsigned char)a[1]) || a[1] == '-')) {
      if (!fd_resolver_tema(rc, (int32_t)atol(a + 1), args[i], sizeof args[i],
                            NULL, 0))
        return 0;
    } else if (a[0] == 'g' && (isdigit((unsigned char)a[1]) || a[1] == '-')) {
      if (!fd_resolver_genero(rc, (int32_t)atol(a + 1), args[i], sizeof args[i],
                              NULL, 0))
        return 0;
    } else if (a[0] == 'u' && (isdigit((unsigned char)a[1]) || a[1] == '-')) {
      if (!fd_resolver_amigo(rc, (int32_t)atol(a + 1), args[i], sizeof args[i]))
        return 0;
    }
  }

  lit = fd_modelo_lit(m);
  if (!lit) return 0;
  return fd_encaixar_fmt(i18n(lit), args, nArgs, dst, cap);
}

static void fd_medir_selos_locked(void) {
  int i;
  S.idiomaMedido = ajustes_idioma();
  for (i = 0; i < S.snap.nSelos; i++) {
    int w = txt_largura(TXT_ILHA_SUB, S.snap.selos[i].rotulo);
    float lw = (float)w + FD_SELO_PAD_X * 2.0f;
    if (lw < 72.0f) lw = 72.0f;
    if (lw > 280.0f) lw = 280.0f;
    S.snap.selos[i].larg = lw;
  }
}

static void fd_consumir_cena_locked(const FarolCena *c, const FdResolCtx *rc) {
  int p, i;
  int nPorque = 0, nSelos = 0, nInfo = 0, temQual = 0;
  char porque[FAROL_LINHAS_MAX][192];
  FdSelo selos[FAROL_SELA_MAX];
  FdInfo info[FAROL_INFO_MAX];

  memset(porque, 0, sizeof porque);
  memset(selos, 0, sizeof selos);
  memset(info, 0, sizeof info);

  if (c && !strcmp(c->tela, "detalhe")) {
    for (p = 0; p < c->nPecas; p++) {
      const FarolPeca *pc = &c->pecas[p];
      if (pc->tipo == FP_PORQUE && nPorque == 0) {
        if (pc->u.porque.confianca == 0) continue;
        for (i = 0; i < pc->u.porque.nLinhas && nPorque < FAROL_LINHAS_MAX; i++) {
          if (fd_render_texto(c, &pc->u.porque.linhas[i], rc,
                              porque[nPorque], sizeof porque[0]))
            nPorque++;
        }
        if (nPorque > 0) {
          for (i = 0; i < pc->u.porque.nSelos && nSelos < FAROL_SELA_MAX; i++) {
            const FarolSelo *sl = &pc->u.porque.selos[i];
            FdSelo out;
            int ok = 0;
            memset(&out, 0, sizeof out);
            out.tipo = sl->k;
            out.id = sl->id;
            out.elencoCol = -1;
            if (sl->k == FP_SELO_PESSOA)
              ok = fd_resolver_pessoa(rc, sl->id, out.rotulo, sizeof out.rotulo,
                                      out.foto, sizeof out.foto,
                                      &out.tmdb, &out.elencoCol);
            else if (sl->k == FP_SELO_TEMA)
              ok = fd_resolver_tema(rc, sl->id, out.rotulo, sizeof out.rotulo,
                                    out.fio, sizeof out.fio);
            else if (sl->k == FP_SELO_GENERO)
              ok = fd_resolver_genero(rc, sl->id, out.rotulo, sizeof out.rotulo,
                                      out.fio, sizeof out.fio);
            if (ok && out.rotulo[0]) selos[nSelos++] = out;
          }
        }
      } else if (pc->tipo == FP_INFO && nInfo == 0) {
        for (i = 0; i < pc->u.info.nItens && nInfo < FAROL_INFO_MAX; i++) {
          const FarolInfoItem *it = &pc->u.info.itens[i];
          if (fd_render_texto(c, &it->l, rc, info[nInfo].texto,
                              sizeof info[0].texto)) {
            info[nInfo].k = it->k;
            if (it->k == FP_INFO_QUALIDADE) temQual = 1;
            nInfo++;
          }
        }
      }
    }
  }

  S.snap.nPorque = nPorque;
  memcpy(S.snap.porque, porque, sizeof porque);
  S.snap.nSelos = nSelos;
  memcpy(S.snap.selos, selos, sizeof selos);
  S.snap.nInfo = nInfo;
  memcpy(S.snap.info, info, sizeof info);
  S.snap.temQualidade = temQual;
  S.snap.soQualidade = 0;
  if (nPorque == 0 && nInfo > 0 && temQual) {
    int todosQual = 1;
    for (i = 0; i < nInfo; i++)
      if (info[i].k != FP_INFO_QUALIDADE) todosQual = 0;
    S.snap.soQualidade = todosQual;
  }
  S.snap.visivel = (nPorque > 0 || nInfo > 0);
  if (S.snap.nSelos <= 0) {
    S.snap.focado = 0;
    S.snap.seloAtual = 0;
  } else {
    if (S.seloLembrado >= S.snap.nSelos) S.seloLembrado = S.snap.nSelos - 1;
    if (S.snap.seloAtual >= S.snap.nSelos) S.snap.seloAtual = S.snap.nSelos - 1;
  }
  fd_medir_selos_locked();
}

// ------------------------------------------------- coleta de dados sem trava
static void fd_primeiro_iso(const char *csv, char *dst, size_t cap) {
  size_t i = 0;
  if (!dst || !cap) return;
  dst[0] = 0;
  if (!csv) return;
  while (*csv == ' ' || *csv == ',' || *csv == ';') csv++;
  while (csv[i] && csv[i] != ',' && csv[i] != ';' && csv[i] != ' ' &&
         i + 1 < cap) {
    dst[i] = (char)tolower((unsigned char)csv[i]);
    i++;
  }
  dst[i] = 0;
  if (!strcmp(dst, "pob") || !strcmp(dst, "ptb") || !strcmp(dst, "pt-br") ||
      !strcmp(dst, "por") || !strcmp(dst, "br"))
    snprintf(dst, cap, "pt");
  else if (!strcmp(dst, "eng"))
    snprintf(dst, cap, "en");
  else if (!strcmp(dst, "spa") || !strcmp(dst, "esp"))
    snprintf(dst, cap, "es");
}

static int fd_coletar_fontes_app(const char *alvo, FarolInfoFonte *out,
                                 int max, FarolInfoCaps *capsOut) {
  int hdr = -1, dv = -1, hevc = -1, av1 = -1;
  int n, i, k = 0;
  stream_capacidades(&hdr, &dv, &hevc, &av1);
  capsOut->hdr = hdr;
  capsOut->dv = dv;
  if (!alvo || !alvo[0] || !stream_lista_do_alvo(alvo)) return 0;
  n = stream_n();
  for (i = 0; i < n && k < max; i++) {
    const Stream *s = stream_item(i);
    FarolInfoFonte f;
    int est, cacheOk;
    if (!s) continue;
    memset(&f, 0, sizeof f);
    if (s->altura >= 2000 || (s->badges & badges_bit("r-4k"))) f.altura = 2160;
    else if (s->altura >= 1000 || (s->badges & badges_bit("r-1080"))) f.altura = 1080;
    else if (s->altura >= 700 || (s->badges & badges_bit("r-720"))) f.altura = 720;
    else f.altura = s->altura;

    f.dv = (s->dolbyVision ||
            (s->badges & (badges_bit("v-dv") | badges_bit("a-atmos-dv") |
                          badges_bit("a-truehd-dv") | badges_bit("a-dd-dv")))) ? 1 : 0;
    f.hdr = (!f.dv &&
             (s->badges & (badges_bit("v-hdr10plus") | badges_bit("v-hdr10") |
                           badges_bit("v-hdr") | badges_bit("v-hlg")))) ? 1 : 0;
    est = stream_fonte_estado(i);
    cacheOk = s->emCache && !s->cacheNegado && (!s->foraCache || s->cacheNativo) &&
              !s->urlP2P && !s->naoVideo;
    if (s->naoVideo || stream_incompativel(i) || est == FA_RUIM)
      f.toca = 0;
    else if ((est == FA_OK || cacheOk) && stream_fonte_disponivel(s))
      f.toca = 1;
    else
      f.toca = -1;

    fd_primeiro_iso(s->audioIdiomas, f.audio, sizeof f.audio);
    if (!f.audio[0]) {
      const char *id = stream_idioma(s);
      if (id && (!strcmp(id, "Dublado") || !strcmp(id, "Dual áudio")))
        snprintf(f.audio, sizeof f.audio, "pt");
    }
    fd_primeiro_iso(s->legendaIdiomas, f.legenda, sizeof f.legenda);
    if (!f.legenda[0]) {
      const char *id = stream_idioma(s);
      if (id && !strcmp(id, "Legendado"))
        snprintf(f.legenda, sizeof f.legenda, "pt");
    }
    out[k++] = f;
  }
  return k;
}

static int fd_enriquecer_ficha(const CatItem *ci, const MapaVizinhos *mv,
                               int temMvAlvo, FarolFicha *f) {
  int temItem = 0, i;
  memset(f, 0, sizeof *f);
  if (ci) temItem = farolficha_ler_item(ci, f);
  if (temMvAlvo && mv) {
    if (!f->tmdb && mv->foco.tmdb > 0) f->tmdb = (int32_t)mv->foco.tmdb;
    for (i = 0; i < MAPA_VIZ_GRUPOS; i++) {
      const MapaVizGrupo *gr = &mv->g[i];
      if (gr->n <= 0 || !gr->ref) continue;
      if (gr->tipo == MAPA_GR_TEMA && f->nTema < FAROL_FICHA_TEMA) {
        int j, ja = 0;
        for (j = 0; j < f->nTema; j++) if (f->tema[j] == (int32_t)gr->ref) ja = 1;
        if (!ja) f->tema[f->nTema++] = (int32_t)gr->ref;
      } else if (gr->tipo == MAPA_GR_PESSOA && f->nPessoa < FAROL_FICHA_PESSOA) {
        int j, ja = 0;
        for (j = 0; j < f->nPessoa; j++) if (f->pessoa[j] == (int32_t)gr->ref) ja = 1;
        if (!ja) f->pessoa[f->nPessoa++] = (int32_t)gr->ref;
      }
    }
  }
  return (temItem || temMvAlvo) && fd_ficha_tem_dados(f);
}

// ------------------------------------------------- ciclo de vida e quadro
void faroldetalhe_abrir(int catIndice, Uint32 agoraMs) {
  const CatItem *ci = cat_item(catIndice);
  FD_LOCK();
  Uint32 relMs = fd_agora_ms(agoraMs);
  S.aberto = 1;
  S.catIdx = catIndice;
  if (ci) idbase_copiar(ci->imdb, S.imdb, sizeof S.imdb);
  else S.imdb[0] = 0;
  S.diaGer = 0;
  S.diaImdb[0] = 0;
  S.diaTemTit = S.diaTemEp = 0;
  S.aberturaMs = relMs;
  S.ultMs = relMs;
  S.assinatura = 0;
  S.qualEntrou = 0;
  S.qualBloqueada = 0;
  S.cenaManual = 0;
  S.seloLembrado = 0;
  memset(&S.borda, 0, sizeof S.borda);
  memset(&S.snap, 0, sizeof S.snap);
  S.snap.blocoAlpha = 1.0f;
  FD_UNLOCK();
  faroldetalhe_atualizar(catIndice, 0, 0, 0.0f, agoraMs);
}

void faroldetalhe_fechar(void) {
  FD_LOCK();
  S.aberto = 0;
  S.catIdx = -1;
  S.imdb[0] = 0;
  S.diaGer = 0;
  S.diaImdb[0] = 0;
  S.diaTemTit = S.diaTemEp = 0;
  S.assinatura = 0;
  S.cenaManual = 0;
  memset(&S.borda, 0, sizeof S.borda);
  memset(&S.snap, 0, sizeof S.snap);
  FD_UNLOCK();
}

void faroldetalhe_atualizar(int catIndice, int epTemp, int epNum,
                            float dt, Uint32 agoraMs) {
  const CatItem *ci;
  CatItem ciCopia;
  FarolGosto g;
  int temG = 0, recomendar = 1, seguro = 0, infantil = 0, idioma = 0, h12 = 0;
  long long epochS;
  Uint32 relMs, decorrido;
  char alvoStream[64];
  FarolInfoFonte fontesApp[FD_FONTE_MAX];
  FarolInfoCaps capsApp = { -1, -1 };
  int nFontesApp = 0;
  FarolInfoTemp temps[FD_TEMP_MAX];
  int nTemps = 0, faltam = 0, minEp = 0;
  OndeVer ov;
  char ondeNome[64] = "";
  FarolInfoAmigo amigos[AMT_MAX];
  int nAmigos = 0;
  int sagaPos = 0, sagaTotal = 0, sagaVistos = 0;
  int reverAno = 0, reverMes = 0, reverNota10 = 0;
  int retT = 0, retE = 0;
  long long retQuando = 0;
  int jaVisto = 0, salvo = 0;
  uint16_t diaHoje;
  struct tm tmAgora;
  uint8_t faixaHora = 0, fimSemana = 0;
  Mapa mapaLoc;
  MapaVizinhos vizLoc;
  unsigned mapaRevLoc = 0;
  FdResolCtx rc;
  FarolFicha fichaAlvo;
  int temFicha = 0;
  FarolFicha fichasSem[FD_SEM_MAX];
  FarolRecSemente sems[FD_SEM_MAX];
  const char *semRefs[FD_SEM_MAX];
  int nSems = 0;
  int usarTesteFontes = 0, usarTesteModelo = 0;
  uint64_t sig = 0;
  int reduzida;

  // Leituras externas fora de fd_trava (ordem de travas limpa sob TSan).
  ci = cat_item(catIndice);
  if (ci) ciCopia = *ci;
  else memset(&ciCopia, 0, sizeof ciCopia);

  recomendar = farolsinal_recomendar();
  seguro = seguro_perfil_ativo();
  infantil = perfil_infantil_ativo();
  idioma = ajustes_idioma();
  h12 = ajustes_relogio_12h();
  reduzida = fd_reduzida();

  FD_LOCK();
  if (S.cenaManual) {
    int i;
    relMs = fd_agora_ms(agoraMs);
    S.ultMs = relMs;
    if (S.idiomaMedido != idioma) fd_medir_selos_locked();
    if (reduzida) {
      S.snap.qualAlpha = 1.0f;
      S.snap.blocoAlpha = 1.0f;
    } else {
      float passoDt = dt > 0.0f ? dt : 0.016f;
      if (S.snap.temQualidade && S.snap.qualAlpha < 1.0f)
        S.snap.qualAlpha = anim_rampa(S.snap.qualAlpha, 1.0f, passoDt, FD_FADE_MS);
      if (S.snap.blocoAlpha < 1.0f)
        S.snap.blocoAlpha = anim_rampa(S.snap.blocoAlpha, 1.0f, passoDt, FD_FADE_MS);
    }
    for (i = 0; i < FAROL_SELA_MAX; i++) {
      float alvo = (S.snap.focado && i == S.snap.seloAtual) ? 1.0f : 0.0f;
      S.snap.seloFoco[i] = reduzida ? alvo : anim_mola(S.snap.seloFoco[i], alvo, dt, 18.0f);
    }
    anim_borda_passo(&S.borda, dt, reduzida);
    S.snap.bordaX = S.borda.x;
    FD_UNLOCK();
    return;
  }

  relMs = fd_agora_ms(agoraMs);
  {
    char idAgora[24];
    if (ci) idbase_copiar(ci->imdb, idAgora, sizeof idAgora);
    else idAgora[0] = 0;
    // Abertura e identidade (IMDb base), nunca posicao: a republicacao do
    // catalogo troca o catIndice do mesmo titulo (detail.c revalidarIdx) e
    // NAO pode reiniciar aberturaMs nem as guardas de qualidade (FAROL46B).
    if (!S.aberto || (idAgora[0] && strcmp(S.imdb, idAgora) != 0)) {
      S.aberto = 1;
      snprintf(S.imdb, sizeof S.imdb, "%s", idAgora);
      S.diaGer = 0;
      S.diaImdb[0] = 0;
      S.diaTemTit = S.diaTemEp = 0;
      S.aberturaMs = relMs;
      S.assinatura = 0;
      S.qualEntrou = 0;
      S.qualBloqueada = 0;
      S.seloLembrado = 0;
      memset(&S.snap, 0, sizeof S.snap);
      S.snap.blocoAlpha = 1.0f;
    }
    S.catIdx = catIndice;   // espelho do quadro; posicao nao e identidade
  }
  epochS = fd_agora_s();
  decorrido = relMs - S.aberturaMs;
  if (decorrido > FD_PRAZO_QUALIDADE_MS && !S.qualEntrou)
    S.qualBloqueada = 1;
  usarTesteModelo = S.temModeloTeste;
  if (usarTesteModelo) { g = S.modeloTeste; temG = 1; }
  usarTesteFontes = S.temFontesTeste;
  FD_UNLOCK();

  if (!ci || fd_eh_canal(&ciCopia) || !recomendar || seguro) {
    FD_LOCK();
    memset(&S.snap, 0, sizeof S.snap);
    S.assinatura = 0;
    FD_UNLOCK();
    return;
  }

  if (!usarTesteModelo) temG = farolsinal_modelo(&g);
  // A pagina e da obra, nunca do episodio: "tt…:T:E" (cartao de Continuar)
  // normaliza para a serie antes de qualquer busca, fonte ou cena (FAROL46B).
  {
    char idTmp[24];
    idbase_copiar(ciCopia.imdb, idTmp, sizeof idTmp);
    snprintf(ciCopia.imdb, sizeof ciCopia.imdb, "%s", idTmp);
  }
  diaHoje = fd_dia_de(epochS);
  fd_tempo_partes(epochS, &tmAgora, &faixaHora, &fimSemana);

  // Recusado explicitamente ("Nao e para mim") nao mostra bloco no detalhe.
  if (temG && farolgosto_recusado(&g, mapa_hash_titulo(ciCopia.imdb))) {
    FD_LOCK();
    memset(&S.snap, 0, sizeof S.snap);
    S.assinatura = 0;
    FD_UNLOCK();
    return;
  }

  pthread_mutex_lock(&fd_mapaMu);
  (void)mapa_copiar(&s_mapaCache, &s_mapaRev);
  (void)mapa_vizinhos_copiar(&s_vizCache, &s_vizRev);
  mapaLoc = s_mapaCache;
  vizLoc = s_vizCache;
  mapaRevLoc = s_mapaRev;
  pthread_mutex_unlock(&fd_mapaMu);

  memset(&rc, 0, sizeof rc);
  rc.ci = &ciCopia;
  rc.mapa = &mapaLoc;
  rc.mv = &vizLoc;
  rc.temMvAlvo = (vizLoc.foco.imdb[0] && !strcmp(vizLoc.foco.imdb, ciCopia.imdb));
  rc.temAmg = amigostitulo_obter(ciCopia.imdb, &rc.amg);

  // 1) Ficha do alvo: cache de teste -> MapaVizinhos + CatItem.
  FD_LOCK();
  {
    int i;
    for (i = 0; i < S.nTFichas; i++)
      if (!strcmp(S.tFichas[i].imdb, ciCopia.imdb)) {
        fichaAlvo = S.tFichas[i].ficha;
        temFicha = fd_ficha_tem_dados(&fichaAlvo);
        break;
      }
  }
  FD_UNLOCK();
  if (!temFicha)
    temFicha = fd_enriquecer_ficha(&ciCopia, &vizLoc, rc.temMvAlvo, &fichaAlvo);

  // Bloco so nasce a partir do quadro em que a ficha existe.
  if (!temFicha) {
    FD_LOCK();
    memset(&S.snap, 0, sizeof S.snap);
    S.assinatura = 0;
    FD_UNLOCK();
    return;
  }

  // 2) Sementes vistas para explicar "Porque voce terminou X" / "Do diretor de X".
  FD_LOCK();
  if (S.nTSems > 0) {
    int i;
    for (i = 0; i < S.nTSems && nSems < FD_SEM_MAX; i++) {
      fichasSem[nSems] = S.tSems[i].ficha;
      memset(&sems[nSems], 0, sizeof sems[0]);
      sems[nSems].ficha = &fichasSem[nSems];
      sems[nSems].terminou = (uint8_t)(S.tSems[i].terminou ? 1 : 0);
      snprintf(rc.semImdb[nSems], sizeof rc.semImdb[0], "%s", S.tSems[i].imdb);
      snprintf(rc.semTitulo[nSems], sizeof rc.semTitulo[0], "%s", S.tSems[i].titulo);
      semRefs[nSems] = rc.semImdb[nSems];
      nSems++;
    }
    rc.nSemNomes = nSems;
  }
  FD_UNLOCK();

  if (nSems == 0) {
    int i;
    // mo->catIndice refere-se a revisao do catalogo de quando o retrato foi
    // montado: a republicacao troca o dono do slot. A semente e rebuscada
    // pela identidade — mas a busca e linear, entao o indice resolvido so se
    // refaz quando o catalogo ou o retrato muda; o item e conferido a cada
    // quadro mesmo assim (FAROL46C).
    {
      unsigned revCat = cat_revisao();
      if (revCat != s_semCatRev || mapaRevLoc != s_semMapaRev) {
        s_semIdxN = mapaLoc.nSem < MAPA_SEM_MAX ? mapaLoc.nSem : MAPA_SEM_MAX;
        for (i = 0; i < s_semIdxN; i++)
          s_semIdx[i] = cat_indice_por_imdb(mapaLoc.sem[i].imdb);
        s_semCatRev = revCat;
        s_semMapaRev = mapaRevLoc;
      }
    }
    for (i = 0; i < s_semIdxN && nSems < FD_SEM_MAX; i++) {
      const MapaObra *mo = &mapaLoc.sem[i];
      int cIdx = s_semIdx[i];
      if (strncmp(mo->imdb, "tt", 2) || !mo->titulo[0]) continue;
      if (!strcmp(mo->imdb, ciCopia.imdb)) continue;
      if (cIdx < 0) continue;
      {
        const CatItem *cs = cat_item(cIdx);
        if (!cs || !fd_mesmo_id(cs->imdb, mo->imdb) ||
            !farolficha_ler_item(cs, &fichasSem[nSems]) ||
            !fd_ficha_tem_dados(&fichasSem[nSems]))
          continue;
      }
      memset(&sems[nSems], 0, sizeof sems[0]);
      sems[nSems].ficha = &fichasSem[nSems];
      sems[nSems].terminou = (uint8_t)(mapaLoc.semOrigem[i] == MAPA_ORIGEM_VISTO ? 1 : 0);
      snprintf(rc.semImdb[nSems], sizeof rc.semImdb[0], "%s", mo->imdb);
      snprintf(rc.semTitulo[nSems], sizeof rc.semTitulo[0], "%s", mo->titulo);
      semRefs[nSems] = rc.semImdb[nSems];
      nSems++;
    }
    rc.nSemNomes = nSems;
  }

  // 3) Fontes (qualidade) respeitando o prazo estrito de 8 s.
  if (epTemp > 0 && epNum > 0)
    snprintf(alvoStream, sizeof alvoStream, "%s:%d:%d", ciCopia.imdb, epTemp, epNum);
  else
    snprintf(alvoStream, sizeof alvoStream, "%s", ciCopia.imdb);

  FD_LOCK();
  if (!S.qualBloqueada) {
    if (usarTesteFontes) {
      if (!S.alvoFontesTeste[0] || !strcmp(S.alvoFontesTeste, alvoStream) ||
          !strcmp(S.alvoFontesTeste, ciCopia.imdb)) {
        nFontesApp = S.nFontesTeste;
        memcpy(fontesApp, S.fontesTeste, (size_t)nFontesApp * sizeof fontesApp[0]);
        capsApp = S.capsTeste;
      }
    } else {
      FD_UNLOCK();
      nFontesApp = fd_coletar_fontes_app(alvoStream, fontesApp, FD_FONTE_MAX, &capsApp);
      FD_LOCK();
    }
  }
  {
    FarolInfoLinha qLi[2];
    int nq = (!S.qualBloqueada && nFontesApp > 0)
               ? farolinfo_qualidade(qLi, 2, fontesApp, nFontesApp, &capsApp)
               : 0;
    int qRenderizavel = 0, qi;
    for (qi = 0; qi < nq; qi++)
      if (fd_modelo_lit(qLi[qi].m) != NULL) qRenderizavel = 1;

    if (qRenderizavel && !S.qualEntrou && !S.qualBloqueada) {
      if (decorrido <= FD_PRAZO_QUALIDADE_MS) {
        S.qualEntrou = 1;
        if (decorrido == 0 || reduzida)
          S.snap.qualAlpha = 1.0f;
        else
          S.snap.qualAlpha = 0.0f;
      } else {
        S.qualBloqueada = 1;
        nFontesApp = 0;
      }
    }
    if (S.qualBloqueada) nFontesApp = 0;
  }
  FD_UNLOCK();

  // 4) Outros fatos da P6 (continuar, onde, amigos, saga, rever, retomada).
  salvo = ciCopia.naLista || salvos_tem(ciCopia.imdb);
  {
    // Diario so quando quente (diario_carregado e a consulta barata da 38D):
    // carga fria e varredura por quadro ficam fora do fio da UI. O resultado
    // fica cacheado por (imdb, geracao) — a assinatura so muda quando o
    // diario muda; frio, os fatos ficam de fora ate o cache esquentar.
    int ehSerieTv = (!strcmp(ciCopia.tipo, "series") || !strcmp(ciCopia.tipo, "tv"));
    int temTit = 0, temEp = 0, tRL = 0, eRL = 0;
    long long qRL = 0;
    DiarioTitulo dtTit;
    memset(&dtTit, 0, sizeof dtTit);
    if (diario_carregado(0)) {
      unsigned ger = diario_geracao(0);
      int mesmo;
      FD_LOCK();
      mesmo = (S.diaGer == ger && !strcmp(S.diaImdb, ciCopia.imdb));
      if (mesmo) {
        temTit = S.diaTemTit; dtTit = S.diaTit;
        temEp = S.diaTemEp; tRL = S.diaRetT; eRL = S.diaRetE; qRL = S.diaRetQ;
      }
      FD_UNLOCK();
      if (!mesmo) {
        temTit = diario_titulo(0, ciCopia.imdb, &dtTit);
        if (ehSerieTv)
          temEp = diario_ultimo_episodio(0, ciCopia.imdb, &tRL, &eRL, &qRL);
        FD_LOCK();
        S.diaGer = ger;
        idbase_copiar(ciCopia.imdb, S.diaImdb, sizeof S.diaImdb);
        S.diaTemTit = temTit; S.diaTit = dtTit;
        S.diaTemEp = temEp; S.diaRetT = tRL; S.diaRetE = eRL; S.diaRetQ = qRL;
        FD_UNLOCK();
      }
      if (temEp) { retT = tRL; retE = eRL; retQuando = qRL; }
    }
    if (temTit && dtTit.vezes > 0) {
      if (!ehSerieTv) {
        jaVisto = 1;
        if (dtTit.ultimo > 0) {
          time_t tv = (time_t)dtTit.ultimo;
          struct tm tmv;
          if (localtime_r(&tv, &tmv)) {
            reverAno = tmv.tm_year + 1900;
            reverMes = tmv.tm_mon + 1;
          }
          reverNota10 = dtTit.nota10 > 0 ? (int)dtTit.nota10
                      : (reacao_notas_lida() ? reacao_nota(ciCopia.imdb) : 0);
        }
      }
    } else if (ciCopia.progresso >= 90 && !ehSerieTv) {
      jaVisto = 1;
    }
    if (ehSerieTv && jaVisto) { retT = 0; retE = 0; retQuando = 0; }
  }

  if (!strcmp(ciCopia.tipo, "series") || !strcmp(ciCopia.tipo, "tv")) {
    int nt = extras_n_temporadas(), ti, vistosEp = 0, totalEp = 0;
    for (ti = 0; ti < nt && nTemps < FD_TEMP_MAX; ti++) {
      int numT = extras_temporada_numero(ti);
      int ne = extras_n_eps(ti), ei, soma = 0, cnt = 0;
      if (numT <= 0) continue;
      for (ei = 0; ei < ne; ei++) {
        int nota = extras_ep_nota(ti, ei);
        if (nota > 0) { soma += nota; cnt++; }
      }
      temps[nTemps].numero = numT;
      temps[nTemps].comNota = cnt;
      temps[nTemps].notaX10 = cnt > 0 ? (soma + cnt / 2) / cnt : 0;
      nTemps++;
    }
    (void)extras_progresso_serie(&vistosEp, &totalEp);
    if (totalEp > vistosEp && vistosEp > 0) faltam = totalEp - vistosEp;
    minEp = fichaAlvo.minutos;
    if (totalEp > 0 && vistosEp >= totalEp) { jaVisto = 1; retT = 0; retE = 0; retQuando = 0; }
    if (jaVisto) { retT = 0; retE = 0; retQuando = 0; }
  }

  if (ondever_n(ciCopia.imdb) > 0 && ondever_item(ciCopia.imdb, 0, &ov) && ov.nome[0])
    snprintf(ondeNome, sizeof ondeNome, "%s", ov.nome);

  if (rc.temAmg) {
    int i;
    for (i = 0; i < rc.amg.n && nAmigos < AMT_MAX; i++) {
      amigostitulo_primeiro_nome(rc.amg.a[i].nome, amigos[nAmigos].nome,
                                 sizeof amigos[0].nome);
      if (!amigos[nAmigos].nome[0]) continue;
      amigos[nAmigos].terminou = rc.amg.a[i].terminou ? 1 : 0;
      amigos[nAmigos].gostou = rc.amg.a[i].gostou ? 1 : 0;
      nAmigos++;
    }
  }

  {
    int ncol = extras_n_colecao();
    if (ncol >= 2 && ciCopia.tmdb > 0) {
      int i;
      sagaTotal = ncol;
      for (i = 0; i < ncol; i++) {
        if (extras_colecao_tmdb(i) == ciCopia.tmdb) sagaPos = i + 1;
      }
      if (sagaPos <= 0) sagaTotal = 0;
    }
  }

  // 5) Assinatura dos insumos: so recompoe e remede quando algo mudou.
  sig = fd_fnv(0, ciCopia.imdb, strlen(ciCopia.imdb));
  {
    unsigned revCat = cat_revisao();
    sig = fd_fnv(sig, &revCat, sizeof revCat);
  }
  sig = fd_fnv(sig, &fichaAlvo, sizeof fichaAlvo);
  sig = fd_fnv(sig, &epTemp, sizeof epTemp);
  sig = fd_fnv(sig, &epNum, sizeof epNum);
  sig = fd_fnv(sig, &temG, sizeof temG);
  if (temG) {
    sig = fd_fnv(sig, &g.titulos, sizeof g.titulos);
    sig = fd_fnv(sig, g.genero, sizeof g.genero);
    sig = fd_fnv(sig, g.pessoa, sizeof g.pessoa);
    sig = fd_fnv(sig, g.tema, sizeof g.tema);
  }
  sig = fd_fnv(sig, &nSems, sizeof nSems);
  sig = fd_fnv(sig, &nFontesApp, sizeof nFontesApp);
  if (nFontesApp > 0) sig = fd_fnv(sig, fontesApp, (size_t)nFontesApp * sizeof fontesApp[0]);
  sig = fd_fnv(sig, &capsApp, sizeof capsApp);
  sig = fd_fnv(sig, &nTemps, sizeof nTemps);
  if (nTemps > 0) sig = fd_fnv(sig, temps, (size_t)nTemps * sizeof temps[0]);
  sig = fd_fnv(sig, &faltam, sizeof faltam);
  sig = fd_fnv(sig, ondeNome, strlen(ondeNome));
  sig = fd_fnv(sig, &nAmigos, sizeof nAmigos);
  if (nAmigos > 0) sig = fd_fnv(sig, amigos, (size_t)nAmigos * sizeof amigos[0]);
  sig = fd_fnv(sig, &sagaPos, sizeof sagaPos);
  sig = fd_fnv(sig, &sagaTotal, sizeof sagaTotal);
  sig = fd_fnv(sig, &jaVisto, sizeof jaVisto);
  sig = fd_fnv(sig, &salvo, sizeof salvo);
  sig = fd_fnv(sig, &reverAno, sizeof reverAno);
  sig = fd_fnv(sig, &reverMes, sizeof reverMes);
  sig = fd_fnv(sig, &reverNota10, sizeof reverNota10);
  sig = fd_fnv(sig, &retT, sizeof retT);
  sig = fd_fnv(sig, &retE, sizeof retE);
  sig = fd_fnv(sig, &idioma, sizeof idioma);
  sig = fd_fnv(sig, &h12, sizeof h12);
  {
    long long minChave = epochS / 60LL;
    sig = fd_fnv(sig, &minChave, sizeof minChave);
  }

  FD_LOCK();
  S.ultMs = relMs;
  if (sig != S.assinatura) {
    FarolCenaCtx ctx;
    FarolCenaDet det;
    int len, i;

    memset(&ctx, 0, sizeof ctx);
    ctx.g = temG ? &g : NULL;
    ctx.sementes = nSems > 0 ? sems : NULL;
    ctx.nSementes = nSems;
    ctx.sementeRef = nSems > 0 ? semRefs : NULL;
    ctx.dia = diaHoje;
    ctx.faixaHora = faixaHora;
    ctx.fimSemana = fimSemana;
    ctx.perfil = perfis_ativo();
    ctx.feita = epochS;
    ctx.semente = mapa_hash_titulo(ciCopia.imdb);

    memset(&det, 0, sizeof det);
    det.alvo.ficha = &fichaAlvo;
    det.alvo.chave = farolficha_chave(&fichaAlvo);
    det.alvo.h = mapa_hash_titulo(ciCopia.imdb);
    det.alvo.semente = FRC_SEM_SEMENTE;
    det.alvo.fontes = FRC_F_CATALOGO;
    if (jaVisto) det.alvo.flags |= FRC_FL_VISTO;
    if (salvo)   det.alvo.fontes |= FRC_F_SALVO;
    if (infantil && !perfil_infantil_classif_ok(ciCopia.classificacao))
      det.alvo.flags |= FRC_FL_FORA_CLASS;

    // Elo com semente vista (ponte do Mapa ou afinidade direta de semente).
    for (i = 0; i < mapaLoc.nPontes && det.alvo.semente == FRC_SEM_SEMENTE; i++) {
      const MapaPonte *mp = &mapaLoc.pontes[i];
      if (!strcmp(mp->obra.imdb, ciCopia.imdb) &&
          mp->a >= 0 && mp->a < mapaLoc.nSem) {
        int si;
        for (si = 0; si < nSems; si++)
          if (!strcmp(rc.semImdb[si], mapaLoc.sem[mp->a].imdb)) {
            det.alvo.semente = (uint16_t)si;
            det.alvo.fontes |= FRC_F_SEMENTE;
            det.alvo.flags |= FRC_FL_EM_REC;
            break;
          }
      }
    }
    if (det.alvo.semente == FRC_SEM_SEMENTE && nSems > 0) {
      int si;
      for (si = 0; si < nSems; si++) {
        const FarolFicha *fs = sems[si].ficha;
        if (!fs || !sems[si].terminou) continue;
        if ((fichaAlvo.pessoa[0] && fs->pessoa[0] == fichaAlvo.pessoa[0]) ||
            (fichaAlvo.tema[0] && fs->tema[0] == fichaAlvo.tema[0]) ||
            (fichaAlvo.genero[0] && fs->genero[0] == fichaAlvo.genero[0])) {
          det.alvo.semente = (uint16_t)si;
          det.alvo.fontes |= FRC_F_SEMENTE;
          break;
        }
      }
    }

    if (rc.temAmg) {
      for (i = 0; i < rc.amg.n; i++) {
        if (rc.amg.a[i].recomendou) {
          det.alvo.fontes |= FRC_F_REC;
          det.alvo.quem = fd_hash_id31(rc.amg.a[i].id);
          break;
        }
        if (rc.amg.a[i].gostou && !det.alvo.quem) {
          det.alvo.fontes |= FRC_F_AMIGO;
          det.alvo.quem = fd_hash_id31(rc.amg.a[i].id);
          det.alvo.gostoPct = (uint8_t)(rc.amg.a[i].gosto > 0 ? rc.amg.a[i].gosto : 75);
        }
      }
    }

    det.alvoRef = strncmp(ciCopia.imdb, "tt", 2) == 0 ? ciCopia.imdb : NULL;
    det.fontes = (!S.qualBloqueada && nFontesApp > 0) ? fontesApp : NULL;
    det.nFontes = (!S.qualBloqueada && nFontesApp > 0) ? nFontesApp : 0;
    det.caps = capsApp;
    det.temps = nTemps > 0 ? temps : NULL;
    det.nTemps = nTemps;
    det.faltam = faltam;
    det.minEp = minEp;
    det.agora = &tmAgora;
    det.h12 = h12;
    det.minutos = (strcmp(ciCopia.tipo, "series") != 0 &&
                   strcmp(ciCopia.tipo, "tv") != 0) ? fichaAlvo.minutos : 0;
    det.minNoite = 0;
    det.onde = ondeNome[0] ? ondeNome : NULL;
    det.amigos = nAmigos > 0 ? amigos : NULL;
    det.nAmigos = nAmigos;
    det.sagaPos = sagaPos;
    det.sagaTotal = sagaTotal;
    det.sagaVistos = sagaVistos;
    det.reverAno = reverAno;
    det.reverMes = reverMes;
    det.reverNota10 = reverNota10;
    det.retT = retT;
    det.retE = retE;
    det.retQuando = retQuando;

    if (det.alvoRef) {
      len = farolcena_detalhe(s_jsonTmp, sizeof s_jsonTmp, &ctx, &det);
      if (len > 0 && farolpeca_ler(&s_cenaTmp, s_jsonTmp, 0) >= 0) {
        int visAntes = S.snap.visivel;
        fd_consumir_cena_locked(&s_cenaTmp, &rc);
        if (!visAntes && S.snap.visivel) {
          if (S.snap.soQualidade && decorrido > 0 && !reduzida)
            S.snap.blocoAlpha = S.snap.qualAlpha;
          else
            S.snap.blocoAlpha = 1.0f;
        }
      } else {
        S.snap.visivel = 0;
        S.snap.nPorque = S.snap.nSelos = S.snap.nInfo = 0;
      }
    } else {
      S.snap.visivel = 0;
      S.snap.nPorque = S.snap.nSelos = S.snap.nInfo = 0;
    }
    S.assinatura = sig;
  } else if (S.idiomaMedido != idioma) {
    fd_medir_selos_locked();
  }

  if (reduzida) {
    if (S.snap.temQualidade) S.snap.qualAlpha = 1.0f;
    S.snap.blocoAlpha = 1.0f;
  } else {
    float passoDt = dt > 0.0f ? dt : 0.016f;
    if (S.snap.temQualidade && S.snap.qualAlpha < 1.0f) {
      S.snap.qualAlpha = anim_rampa(S.snap.qualAlpha, 1.0f, passoDt, FD_FADE_MS);
      if (S.snap.soQualidade) S.snap.blocoAlpha = S.snap.qualAlpha;
    }
    if (!S.snap.soQualidade && S.snap.blocoAlpha < 1.0f)
      S.snap.blocoAlpha = anim_rampa(S.snap.blocoAlpha, 1.0f, passoDt, FD_FADE_MS);
  }
  {
    int i;
    for (i = 0; i < FAROL_SELA_MAX; i++) {
      float alvo = (S.snap.focado && i == S.snap.seloAtual) ? 1.0f : 0.0f;
      S.snap.seloFoco[i] = reduzida ? alvo : anim_mola(S.snap.seloFoco[i], alvo, dt, 18.0f);
    }
  }
  anim_borda_passo(&S.borda, dt, reduzida);
  S.snap.bordaX = S.borda.x;
  FD_UNLOCK();
}

// ------------------------------------------------- geometria e desenho
static float fd_altura_snap(const FdSnap *sn, float larg) {
  float hEsq = 0.0f, hDir = 0.0f, corpo = 0.0f;
  int duasCols;
  if (!sn || !sn->visivel || (sn->nPorque <= 0 && sn->nInfo <= 0))
    return 0.0f;
  duasCols = (sn->nPorque > 0 && sn->nInfo > 0 && larg >= 720.0f);
  if (sn->nPorque > 0) {
    hEsq = (float)sn->nPorque * FD_LINHA_H +
           (float)(sn->nPorque - 1) * FD_LINHA_GAP;
    if (sn->nSelos > 0) hEsq += FD_SELO_TOP + FD_SELO_H;
  }
  if (sn->nInfo > 0) {
    hDir = (float)sn->nInfo * FD_LINHA_H +
           (float)(sn->nInfo - 1) * FD_LINHA_GAP;
  }
  if (duasCols)
    corpo = hEsq > hDir ? hEsq : hDir;
  else if (sn->nPorque > 0 && sn->nInfo > 0)
    corpo = hEsq + 12.0f + hDir;
  else
    corpo = hEsq > 0.0f ? hEsq : hDir;

  return FD_PAD_Y + FD_KICK_H + FD_KICK_GAP + corpo + FD_PAD_Y;
}

float faroldetalhe_altura(float larg) {
  float h;
  FD_LOCK();
  h = fd_altura_snap(&S.snap, larg);
  FD_UNLOCK();
  return h;
}

void faroldetalhe_desenhar(float x, float y, float larg, float a,
                           PonteiroFn ptrFn, int ptrRow) {
  FdSnap sn;
  float h, aCard, ar, ag, ab, yKick, yCorpo;
  int duasCols, i;

  FD_LOCK();
  sn = S.snap;
  FD_UNLOCK();

  h = fd_altura_snap(&sn, larg);
  if (h <= 0.0f || a <= 0.002f) return;
  aCard = a * sn.blocoAlpha;
  if (aCard <= 0.002f) return;

  plrui_material((GfxRect){ x, y, larg, h }, FD_RAIO, 0, aCard);

  // Um unico ponto de luz do Farol no bloco inteiro (PLANO-GOSTO 4.2).
  ajustes_acento(&ar, &ag, &ab);
  yKick = y + FD_PAD_Y;
  gfx_cor((GfxRect){ x + FD_PAD_X, yKick + (FD_KICK_H - 10.0f) * 0.5f, 10.0f, 10.0f },
          0.5f, ar, ag, ab, aCard);
  plrui_kicker(sn.nPorque > 0 ? FT_PORQUE_TITULO : FT_NOME,
               x + FD_PAD_X + 18.0f, yKick + 2.0f, 168, 172, 180, aCard);

  yCorpo = yKick + FD_KICK_H + FD_KICK_GAP;
  duasCols = (sn.nPorque > 0 && sn.nInfo > 0 && larg >= 720.0f);

  if (sn.nPorque > 0) {
    float colX = x + FD_PAD_X;
    float colW = duasCols ? (larg - 2.0f * FD_PAD_X - FD_COL_GAP) * 0.55f
                          : (larg - 2.0f * FD_PAD_X);
    float ly = yCorpo;
    for (i = 0; i < sn.nPorque; i++) {
      TxtLinha tl = txt_linha_corta(TXT_ILHA_SUB, sn.porque[i],
                                    238, 238, 240, 255, colW);
      txt_desenhar_alpha(tl, colX, ly + (FD_LINHA_H - (float)tl.h) * 0.5f, aCard);
      ly += FD_LINHA_H + FD_LINHA_GAP;
    }
    if (sn.nSelos > 0) {
      float sy = yCorpo + (float)sn.nPorque * FD_LINHA_H +
                 (float)(sn.nPorque - 1) * FD_LINHA_GAP + FD_SELO_TOP;
      float sx = colX + (sn.focado ? sn.bordaX : 0.0f);
      float disp = colW - (float)(sn.nSelos - 1) * FD_SELO_GAP;
      float tetoW = sn.nSelos > 0 ? disp / (float)sn.nSelos : colW;
      float somaW = 0.0f;
      for (i = 0; i < sn.nSelos; i++) somaW += sn.selos[i].larg;
      for (i = 0; i < sn.nSelos; i++) {
        float w = sn.selos[i].larg;
        float f = sn.seloFoco[i];
        int tinta;
        GfxRect pill;
        TxtLinha ts;
        if (somaW > disp && w > tetoW) w = tetoW;
        if (sx + w > colX + colW) w = (colX + colW) - sx;
        if (w < 48.0f) break;
        pill = (GfxRect){ sx, sy, w, FD_SELO_H };
        plrui_botao_repouso(pill, aCard);
        if (f > 0.003f) plrui_pilula_foco(pill, f * aCard);
        tinta = f > 0.5f ? plrui_tinta() : 235;
        ts = txt_linha_corta(TXT_ILHA_SUB, sn.selos[i].rotulo,
                             tinta, tinta, tinta, 255, w - FD_SELO_PAD_X * 2.0f);
        txt_desenhar_alpha(ts, pill.x + (pill.w - (float)ts.w) * 0.5f,
                           pill.y + (pill.h - (float)ts.h) * 0.5f, aCard);
        if (ptrFn && aCard > 0.3f)
          ponteiro_alvo(pill.x, pill.y, pill.w, pill.h, ptrFn, NULL, ptrRow, i);
        sx += w + FD_SELO_GAP;
      }
    }
  }

  if (sn.nInfo > 0) {
    float infoX, infoW, infoY;
    if (duasCols) {
      float esqW = (larg - 2.0f * FD_PAD_X - FD_COL_GAP) * 0.55f;
      float sepX = x + FD_PAD_X + esqW + FD_COL_GAP * 0.5f;
      gfx_cor((GfxRect){ sepX, yCorpo + 2.0f, 1.0f, h - (yCorpo - y) - FD_PAD_Y - 4.0f },
              0.0f, 1.0f, 1.0f, 1.0f, 0.08f * aCard);
      infoX = x + FD_PAD_X + esqW + FD_COL_GAP;
      infoW = (x + larg - FD_PAD_X) - infoX;
      infoY = yCorpo;
    } else if (sn.nPorque > 0) {
      float hEsq = (float)sn.nPorque * FD_LINHA_H +
                   (float)(sn.nPorque - 1) * FD_LINHA_GAP +
                   (sn.nSelos > 0 ? FD_SELO_TOP + FD_SELO_H : 0.0f);
      infoX = x + FD_PAD_X;
      infoW = larg - 2.0f * FD_PAD_X;
      infoY = yCorpo + hEsq + 12.0f;
    } else {
      infoX = x + FD_PAD_X;
      infoW = larg - 2.0f * FD_PAD_X;
      infoY = yCorpo;
    }
    for (i = 0; i < sn.nInfo; i++) {
      float aLi = aCard * (sn.info[i].k == FP_INFO_QUALIDADE ? sn.qualAlpha : 1.0f);
      if (aLi > 0.002f) {
        TxtLinha ti = txt_linha_corta(TXT_ILHA_SUB, sn.info[i].texto,
                                      192, 196, 204, 255, infoW);
        txt_desenhar_alpha(ti, infoX, infoY + (FD_LINHA_H - (float)ti.h) * 0.5f, aLi);
      }
      infoY += FD_LINHA_H + FD_LINHA_GAP;
    }
  }
}

// ------------------------------------------------- foco e navegacao D-pad
int faroldetalhe_n_selos(void) {
  int n;
  FD_LOCK();
  n = S.snap.visivel ? S.snap.nSelos : 0;
  FD_UNLOCK();
  return n;
}

int faroldetalhe_focado(void) {
  int f;
  FD_LOCK();
  f = (S.snap.visivel && S.snap.nSelos > 0 && S.snap.focado) ? 1 : 0;
  FD_UNLOCK();
  return f;
}

int faroldetalhe_selo_atual(void) {
  int c;
  FD_LOCK();
  c = S.snap.seloAtual;
  FD_UNLOCK();
  return c;
}

int faroldetalhe_focar(int entrar) {
  int ok = 0;
  FD_LOCK();
  if (!entrar || !S.snap.visivel || S.snap.nSelos <= 0) {
    S.snap.focado = 0;
  } else {
    int c = S.seloLembrado;
    if (c < 0) c = 0;
    if (c >= S.snap.nSelos) c = S.snap.nSelos - 1;
    S.snap.seloAtual = c;
    S.snap.focado = 1;
    ok = 1;
  }
  FD_UNLOCK();
  return ok;
}

void faroldetalhe_ponteiro(int col) {
  FD_LOCK();
  if (S.snap.visivel && col >= 0 && col < S.snap.nSelos) {
    S.snap.focado = 1;
    S.snap.seloAtual = col;
    S.seloLembrado = col;
  }
  FD_UNLOCK();
}

int faroldetalhe_tecla(SDL_Keycode k, int *pediuMenu) {
  int consumiu = 0;
  FD_LOCK();
  if (S.snap.visivel && S.snap.focado && S.snap.nSelos > 0) {
    if (k == SDLK_RIGHT) {
      if (S.snap.seloAtual + 1 < S.snap.nSelos) {
        S.snap.seloAtual++;
        S.seloLembrado = S.snap.seloAtual;
      } else {
        anim_borda_bater(&S.borda, 6.0f);
      }
      consumiu = 1;
    } else if (k == SDLK_LEFT) {
      if (S.snap.seloAtual > 0) {
        S.snap.seloAtual--;
        S.seloLembrado = S.snap.seloAtual;
      } else if (pediuMenu) {
        *pediuMenu = 1;
      }
      consumiu = 1;
    }
  }
  FD_UNLOCK();
  return consumiu;
}

int faroldetalhe_ativar_id(long *tmdb, int *elencoCol,
                           char *nome, size_t tamNome,
                           char *foto, size_t tamFoto,
                           int *seloTipo, char *seloFio, size_t tamFio,
                           int32_t *seloId) {
  int acao = FD_ACAO_NENHUMA;
  if (tmdb) *tmdb = 0;
  if (elencoCol) *elencoCol = -1;
  if (nome && tamNome) nome[0] = 0;
  if (foto && tamFoto) foto[0] = 0;
  if (seloTipo) *seloTipo = -1;
  if (seloFio && tamFio) seloFio[0] = 0;
  if (seloId) *seloId = 0;

  FD_LOCK();
  if (S.snap.visivel && S.snap.focado &&
      S.snap.seloAtual >= 0 && S.snap.seloAtual < S.snap.nSelos) {
    const FdSelo *sl = &S.snap.selos[S.snap.seloAtual];
    if (sl->tipo == FP_SELO_PESSOA) {
      acao = FD_ACAO_PESSOA;
      if (tmdb) *tmdb = sl->tmdb;
      if (elencoCol) *elencoCol = sl->elencoCol;
      if (nome && tamNome) snprintf(nome, tamNome, "%s", sl->rotulo);
      if (foto && tamFoto) snprintf(foto, tamFoto, "%s", sl->foto);
    } else {
      // Tema e genero abrem o Explorar filtrado: o fio e o nome cru do Mapa
      // (kw ou genero local), preservado ate app.c (FAROL46B); o id do selo
      // vai junto quando disponivel.
      acao = FD_ACAO_EXPLORAR;
      if (seloTipo) *seloTipo = sl->tipo;
      if (seloFio && tamFio) snprintf(seloFio, tamFio, "%s", sl->fio);
      if (seloId) *seloId = sl->id;
    }
  }
  FD_UNLOCK();
  return acao;
}

int faroldetalhe_ativar(long *tmdb, int *elencoCol,
                        char *nome, size_t tamNome,
                        char *foto, size_t tamFoto,
                        int *seloTipo, char *seloFio, size_t tamFio) {
  return faroldetalhe_ativar_id(tmdb, elencoCol, nome, tamNome, foto, tamFoto,
                                seloTipo, seloFio, tamFio, NULL);
}

// ------------------------------------------------- inspecao e testes
int faroldetalhe_tem_porque(void) {
  int v;
  FD_LOCK();
  v = S.snap.visivel && S.snap.nPorque > 0;
  FD_UNLOCK();
  return v;
}

int faroldetalhe_tem_info(void) {
  int v;
  FD_LOCK();
  v = S.snap.visivel && S.snap.nInfo > 0;
  FD_UNLOCK();
  return v;
}

int faroldetalhe_tem_qualidade(void) {
  int v;
  FD_LOCK();
  v = S.snap.visivel && S.snap.temQualidade;
  FD_UNLOCK();
  return v;
}

float faroldetalhe_alpha_qualidade(void) {
  float a;
  FD_LOCK();
  a = S.snap.qualAlpha;
  FD_UNLOCK();
  return a;
}

float faroldetalhe_alpha_bloco(void) {
  float a;
  FD_LOCK();
  a = S.snap.blocoAlpha;
  FD_UNLOCK();
  return a;
}

int faroldetalhe_linha_porque(int i, char *dst, size_t cap) {
  int ok = 0;
  if (dst && cap) dst[0] = 0;
  FD_LOCK();
  if (S.snap.visivel && i >= 0 && i < S.snap.nPorque && dst && cap) {
    snprintf(dst, cap, "%s", S.snap.porque[i]);
    ok = 1;
  }
  FD_UNLOCK();
  return ok;
}

int faroldetalhe_linha_info(int i, char *dst, size_t cap) {
  int ok = 0;
  if (dst && cap) dst[0] = 0;
  FD_LOCK();
  if (S.snap.visivel && i >= 0 && i < S.snap.nInfo && dst && cap) {
    snprintf(dst, cap, "%s", S.snap.info[i].texto);
    ok = 1;
  }
  FD_UNLOCK();
  return ok;
}

int faroldetalhe_selo_rotulo(int i, int *tipo, int32_t *id,
                             char *dst, size_t cap) {
  int ok = 0;
  if (dst && cap) dst[0] = 0;
  FD_LOCK();
  if (S.snap.visivel && i >= 0 && i < S.snap.nSelos) {
    if (tipo) *tipo = S.snap.selos[i].tipo;
    if (id) *id = S.snap.selos[i].id;
    if (dst && cap) snprintf(dst, cap, "%s", S.snap.selos[i].rotulo);
    ok = 1;
  }
  FD_UNLOCK();
  return ok;
}

void faroldetalhe_teste_reset(void) {
  FD_LOCK();
  memset(&S, 0, sizeof S);
  S.catIdx = -1;
  S.snap.blocoAlpha = 1.0f;
  s_semCatRev = s_semMapaRev = 0;
  s_semIdxN = 0;
  FD_UNLOCK();
}

void faroldetalhe_teste_relogio(long long (*epochS)(void), Uint32 (*ms)(void)) {
  FD_LOCK();
  S.fnEpoch = epochS;
  S.fnMs = ms;
  FD_UNLOCK();
}

void faroldetalhe_teste_modelo(const FarolGosto *g) {
  FD_LOCK();
  if (g) { S.modeloTeste = *g; S.temModeloTeste = 1; }
  else   { memset(&S.modeloTeste, 0, sizeof S.modeloTeste); S.temModeloTeste = 0; }
  S.assinatura = 0;
  FD_UNLOCK();
}

void faroldetalhe_teste_ficha(const char *imdb, const FarolFicha *f) {
  int i;
  if (!imdb || !imdb[0]) return;
  FD_LOCK();
  for (i = 0; i < S.nTFichas; i++)
    if (!strcmp(S.tFichas[i].imdb, imdb)) {
      if (f) S.tFichas[i].ficha = *f;
      else S.tFichas[i] = S.tFichas[--S.nTFichas];
      S.assinatura = 0;
      FD_UNLOCK();
      return;
    }
  if (f && S.nTFichas < FD_TESTE_MAX) {
    snprintf(S.tFichas[S.nTFichas].imdb, sizeof S.tFichas[0].imdb, "%s", imdb);
    S.tFichas[S.nTFichas].ficha = *f;
    S.nTFichas++;
    S.assinatura = 0;
  }
  FD_UNLOCK();
}

void faroldetalhe_teste_semente(const char *imdb, const char *titulo,
                                const FarolFicha *f, int terminou) {
  if (!imdb || !f) return;
  FD_LOCK();
  if (S.nTSems < FD_SEM_MAX) {
    FdTesteSem *ts = &S.tSems[S.nTSems++];
    memset(ts, 0, sizeof *ts);
    snprintf(ts->imdb, sizeof ts->imdb, "%s", imdb);
    snprintf(ts->titulo, sizeof ts->titulo, "%s", titulo ? titulo : "");
    ts->ficha = *f;
    ts->terminou = terminou;
    S.assinatura = 0;
  }
  FD_UNLOCK();
}

void faroldetalhe_teste_vocab(int tipo, int32_t id, const char *nome,
                              const char *foto, int elencoCol) {
  FD_LOCK();
  if (S.nTVocab < FD_TESTE_MAX && nome && nome[0]) {
    FdTesteVocab *tv = &S.tVocab[S.nTVocab++];
    memset(tv, 0, sizeof *tv);
    tv->tipo = tipo;
    tv->id = id;
    tv->elencoCol = elencoCol;
    snprintf(tv->nome, sizeof tv->nome, "%s", nome);
    if (foto) snprintf(tv->foto, sizeof tv->foto, "%s", foto);
    S.assinatura = 0;
  }
  FD_UNLOCK();
}

void faroldetalhe_teste_fontes(const char *alvo,
                               const FarolInfoFonte *f, int n,
                               const FarolInfoCaps *caps) {
  FD_LOCK();
  if (!f || n <= 0) {
    S.temFontesTeste = 0;
    S.nFontesTeste = 0;
    S.alvoFontesTeste[0] = 0;
  } else {
    if (n > FD_FONTE_MAX) n = FD_FONTE_MAX;
    S.temFontesTeste = 1;
    snprintf(S.alvoFontesTeste, sizeof S.alvoFontesTeste, "%s", alvo ? alvo : "");
    memcpy(S.fontesTeste, f, (size_t)n * sizeof S.fontesTeste[0]);
    S.nFontesTeste = n;
    S.capsTeste = caps ? *caps : (FarolInfoCaps){ 1, 1 };
  }
  S.assinatura = 0;
  FD_UNLOCK();
}

int faroldetalhe_teste_json(const char *json, int nCand, Uint32 agoraMs) {
  int rc;
  FdResolCtx rctx;
  memset(&rctx, 0, sizeof rctx);
  FD_LOCK();
  rc = farolpeca_ler(&s_cenaTmp, json ? json : "", nCand);
  if (rc >= 0) {
    Uint32 relMs = fd_agora_ms(agoraMs);
    Uint32 dec = relMs - S.aberturaMs;
    S.cenaManual = 1;
    fd_consumir_cena_locked(&s_cenaTmp, &rctx);
    if (S.snap.temQualidade) {
      if (dec > FD_PRAZO_QUALIDADE_MS && !S.qualEntrou) {
        int i, k = 0;
        for (i = 0; i < S.snap.nInfo; i++)
          if (S.snap.info[i].k != FP_INFO_QUALIDADE)
            S.snap.info[k++] = S.snap.info[i];
        S.snap.nInfo = k;
        S.snap.temQualidade = 0;
        S.snap.visivel = (S.snap.nPorque > 0 || S.snap.nInfo > 0);
      } else if (!S.qualEntrou) {
        S.qualEntrou = 1;
        S.snap.qualAlpha = (dec == 0 || fd_reduzida()) ? 1.0f : 0.0f;
      }
    }
    S.snap.blocoAlpha = (S.snap.soQualidade && S.snap.qualAlpha < 1.0f)
                          ? S.snap.qualAlpha : 1.0f;
  }
  FD_UNLOCK();
  return rc;
}
