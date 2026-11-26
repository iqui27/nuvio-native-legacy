// FAROLCOMPOR — cliente assincrono POST /v1/farol/compor e adocao segura na UI.
// Ver src/farolcompor.h. Sem SDL, sem _Thread_local, sem log de titulo/perfil/token/URL.
#include "farolcompor.h"
#define FarolMotivo FarolRecMotivo
#include "farolrec.h"
#undef FarolMotivo
#include "farolinfo.h"
#include "faroltexto.h"
#include "rede.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef FAROL_COMPOR_WORKER
#define FC_FALLBACK_URL FAROL_COMPOR_WORKER
#elif defined(NV_REC_URL)
#define FC_FALLBACK_URL NV_REC_URL
#else
#define FC_FALLBACK_URL ""
#endif

// Ganchos fracos dos modulos do app: na build completa as definicoes fortes
// vencem; em testes isolados os padroes seguros permitem linkar sem SDL.
__attribute__((weak)) size_t nv_rec_url_copiar(char *dst, size_t tam) {
  if (!dst || !tam) return 0;
  snprintf(dst, tam, "%s", FC_FALLBACK_URL);
  return strlen(dst);
}
__attribute__((weak)) int recomenda_cabecalhos(const char **cab, char *aut,
                                               size_t na, char *via, size_t nv,
                                               char *perfil, size_t np) {
  (void)cab; (void)aut; (void)na; (void)via; (void)nv; (void)perfil; (void)np;
  return 0;
}
__attribute__((weak)) int perfil_infantil_ativo(void) { return 0; }
__attribute__((weak)) const char *ajustes_tmdb_idioma(void) { return "pt-BR"; }
__attribute__((weak)) int rede_saude_offline(void) { return 0; }
__attribute__((weak)) void trakt_http_recusada(void) {}
__attribute__((weak)) int player_aberto(void) { return 0; }
__attribute__((weak)) int seguro_perfil_ativo(void) { return 0; }
__attribute__((weak)) int farolsinal_recomendar(void) { return 1; }
__attribute__((weak)) int perfis_ativo(void) { return 1; }
__attribute__((weak)) uint64_t perfis_sessao_geracao(void) { return 0; }
__attribute__((weak)) const char *sessao_usuario(void) { return ""; }
__attribute__((weak)) const char *app_tela_nome(void) { return ""; }
__attribute__((weak)) int home_fileiras_na_tela(void) { return 0; }
__attribute__((weak)) const char *dados_dir(void) { return ""; }
__attribute__((weak)) char *dados_ler(const char *nome) {
  (void)nome;
  return NULL;
}
__attribute__((weak)) int dados_gravar_leve(const char *nome, const char *conteudo) {
  (void)nome; (void)conteudo;
  return 0;
}
__attribute__((weak)) int dados_apagar(const char *nome) {
  (void)nome;
  return 0;
}
__attribute__((weak)) int  farolconsent_campo_autorizado(const char *caminho) { (void)caminho; return 1; }
__attribute__((weak)) void farolconsent_sincronizar_ativo(void) {}
__attribute__((weak)) void farolconsent_invalidar(void) {}
__attribute__((weak)) void farolconsent_esquecer(void) {}

#define DIA_EPOCA 1767225600LL

// ------------------------------------------------------------ estruturas --

typedef struct {
  int              tem;
  FarolComporModo  modo;
  int              perfil;
  uint64_t         sessao;
  uint64_t         contaHash;
  uint32_t         hModelo;
  int              diaLocal;
  long long        feita;
  int              doCache;
  FarolCena        cena;
  FarolComporCand  cand[FAROL_COMPOR_CAND_MAX];
  int              nCand;
  unsigned         geracaoAdotada;
} CenaSlot;

typedef struct {
  uint64_t         gen;
  long long        fim;
  unsigned         prazoMs;
  int              cancelar;
  int              usouTrakt;
  int              bloqueou;   // contexto mudou (atomico: pode vir do fio da rede)
  FarolComporModo  modo;
  int              perfil;
  uint64_t         sessao;
  uint64_t         contaHash;
  uint32_t         hModelo;
  int              diaLocal;
  long long        quando;
  char             url[512];
  char             aut[3200];
  char             via[64];
  char             perfilCab[64];
  char            *corpo;
  size_t           nCorpo;
  FarolComporCand  cand[FAROL_COMPOR_CAND_MAX];
  int              nCand;
} ComporJob;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  condFim = PTHREAD_COND_INITIALIZER;

static uint64_t        geracaoAtual;
static int             ativos;
static ComporJob      *jobAtual;

static FarolComporInfo estadoAtual;
static CenaSlot        pendente;
static CenaSlot        adotadaHome;
static CenaSlot        adotadaModos[FC_MODO_N];
static unsigned        homeGeracao;

// Estado em RAM do controle 1 chamada Home/dia local/perfil.
static struct {
  int      perfil;
  uint64_t contaHash;
  int      diaLocal;
} homeDiaMem;

// Ultimo pedido de retomada (issue 57): o mesmo (perfil, serie, t, e) nao
// sai duas vezes na sessao, nem se a resposta caiu nas regras.
static struct {
  int      tem;
  int      perfil;
  uint64_t sessao;
  uint64_t contaHash;
  char     imdb[24];
  int      t, e;
} retUltimo;

static int                    consentiuCfg;
static FarolComporAllowlistFn allowlistFn;
static void                  *allowlistUser;

static int        temUrlOverride;
static char       urlOverride[384];
static int        temAuthOverride;
static char       authOverrideAut[512];
static char       authOverrideVia[64];
static char       authOverridePerfil[64];
static char       pastaOverride[512];
static long long (*relFn)(void);
static int       (*visFn)(void);

// ------------------------------------------------------ utilitarios puros --

static long long agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000LL + (long long)(t.tv_nsec / 1000000L);
}

static long long agoraS(void) {
  long long (*fn)(void);
  pthread_mutex_lock(&trava);
  fn = relFn;
  pthread_mutex_unlock(&trava);
  return fn ? fn() : (long long)time(NULL);
}

static int diaLocalDe(long long quando) {
  time_t t = (time_t)quando;
  struct tm tmv;
  localtime_r(&t, &tmv);
  return (tmv.tm_year + 1900) * 10000 + (tmv.tm_mon + 1) * 100 + tmv.tm_mday;
}

static uint16_t diaModeloDe(long long quando) {
  long long d = (quando - DIA_EPOCA) / 86400LL;
  if (d < 0) d = 0;
  if (d > 65535) d = 65535;
  return (uint16_t)d;
}

static int faixaLocalDe(long long quando, int *fimSem) {
  time_t t = (time_t)quando;
  struct tm tmv;
  int p;
  localtime_r(&t, &tmv);
  if (fimSem) *fimSem = (tmv.tm_wday == 0 || tmv.tm_wday == 6) ? 1 : 0;
  p = tmv.tm_hour < 6 ? 3 : (tmv.tm_hour < 12 ? 0 : (tmv.tm_hour < 18 ? 1 : 2));
  return ((tmv.tm_wday == 0 || tmv.tm_wday == 6) ? 4 : 0) + p;
}

static uint64_t hashStr64(const char *s) {
  uint64_t h = 14695981039346656037ULL;
  if (!s) s = "";
  for (; *s; s++) {
    h ^= (unsigned char)*s;
    h *= 1099511628211ULL;
  }
  return h;
}

static uint64_t contaAtualHash(void) {
  const char *u = sessao_usuario();
  if (!u || !u[0]) return 0;
  return hashStr64(u);
}

static int perfilAtualValido(void) {
  int p = perfis_ativo();
  return (p >= 1 && p <= 8) ? p : 1;
}

static int homeVisivel(void) {
  int (*fn)(void);
  const char *t;
  pthread_mutex_lock(&trava);
  fn = visFn;
  pthread_mutex_unlock(&trava);
  if (fn) return fn();
  t = app_tela_nome();
  if (t && !strcmp(t, "home")) return 1;
  return home_fileiras_na_tela();
}

static const char *modoNome(FarolComporModo m) {
  switch (m) {
    case FC_MODO_HOME:     return "home";
    case FC_MODO_COMPARAR: return "comparar";
    case FC_MODO_DICA:     return "dica";
    case FC_MODO_RETOMADA: return "retomada";
    default:               return NULL;
  }
}

static const char *telaEsperadaModo(FarolComporModo m) {
  switch (m) {
    case FC_MODO_HOME:     return "home";
    case FC_MODO_COMPARAR: return "comparar";
    case FC_MODO_DICA:     return "home";
    case FC_MODO_RETOMADA: return "detalhe";
    default:               return "";
  }
}

static unsigned char minAscii(unsigned char c) {
  return (c >= 'A' && c <= 'Z') ? (unsigned char)(c - 'A' + 'a') : c;
}

static int contemSemCaixa(const char *s, const char *sub) {
  size_t i, n = strlen(sub);
  if (!s || !n) return 0;
  for (; *s; s++) {
    for (i = 0; i < n; i++) {
      if (!s[i] || minAscii((unsigned char)s[i]) != minAscii((unsigned char)sub[i]))
        break;
    }
    if (i == n) return 1;
  }
  return 0;
}

static int temControle(const char *s, int permiteQuebra) {
  const unsigned char *p = (const unsigned char *)s;
  if (!p) return 1;
  for (; *p; p++) {
    if (*p == 0x7f) return 1;
    if (*p < 0x20) {
      if (permiteQuebra && (*p == '\t' || *p == '\n' || *p == '\r')) continue;
      return 1;
    }
  }
  return 0;
}

static int temUrlOuEmail(const char *s) {
  if (!s) return 0;
  return contemSemCaixa(s, "://") || contemSemCaixa(s, "www.") || strchr(s, '@') != NULL;
}

static long lerUtf8(const char **pp, const char *fim) {
  const unsigned char *s = (const unsigned char *)*pp;
  unsigned char c;
  long cp;
  int extra, i;
  if (*pp >= fim) return -1;
  c = s[0];
  if (c < 0x80) { *pp += 1; return c; }
  if (c >= 0xC2 && c <= 0xDF)      { cp = c & 0x1F; extra = 1; }
  else if (c >= 0xE0 && c <= 0xEF) { cp = c & 0x0F; extra = 2; }
  else if (c >= 0xF0 && c <= 0xF4) { cp = c & 0x07; extra = 3; }
  else return -1;
  if (fim - *pp < extra + 1) return -1;
  for (i = 1; i <= extra; i++) {
    if ((s[i] & 0xC0) != 0x80) return -1;
    cp = (cp << 6) | (s[i] & 0x3F);
  }
  if ((extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000)) return -1;
  if (cp >= 0xD800 && cp <= 0xDFFF) return -1;
  if (cp > 0x10FFFF) return -1;
  *pp += extra + 1;
  return cp;
}

static int utf8Valido(const char *s) {
  const char *p, *fim;
  if (!s) return 0;
  p = s;
  fim = s + strlen(s);
  while (p < fim) if (lerUtf8(&p, fim) < 0) return 0;
  return 1;
}

static int textoSeguroLimpar(const char *src, char *dst, size_t cap,
                             size_t maxBytes, int permiteQuebra) {
  const char *p, *fim;
  size_t u = 0;
  int emEspaco = 1;
  if (dst && cap) dst[0] = '\0';
  if (!src || !dst || cap < 2 || maxBytes < 1) return 0;
  if (temControle(src, permiteQuebra) || temUrlOuEmail(src)) return 0;
  p = src;
  fim = src + strlen(src);
  while (p < fim) {
    const char *ini = p;
    long cp = lerUtf8(&p, fim);
    size_t nb;
    if (cp < 0) { dst[0] = '\0'; return 0; }
    if (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r') {
      if (!emEspaco) emEspaco = 1;
      continue;
    }
    if (u > 0 && emEspaco) {
      if (u + 1 >= cap || u + 1 > maxBytes) break;
      dst[u++] = ' ';
    }
    nb = (size_t)(p - ini);
    if (u + nb >= cap || u + nb > maxBytes) break;
    memcpy(dst + u, ini, nb);
    u += nb;
    emEspaco = 0;
  }
  dst[u] = '\0';
  return u > 0;
}

static char retBaseLatim(long cp) {
  static const char T[0x40] = {
    'a','a','a','a','a','a', 0 ,'c',
    'e','e','e','e','i','i','i','i',
     0 ,'n','o','o','o','o','o', 0 ,
     0 ,'u','u','u','u','y', 0 , 0 ,
    'a','a','a','a','a','a', 0 ,'c',
    'e','e','e','e','i','i','i','i',
     0 ,'n','o','o','o','o','o', 0 ,
     0 ,'u','u','u','u','y', 0 ,'y',
  };
  return (cp >= 0xC0 && cp <= 0xFF) ? T[cp - 0xC0] : 0;
}

static int retEhCombinante(long cp) {
  return (cp >= 0x0300 && cp <= 0x036F) ||
         (cp >= 0x1AB0 && cp <= 0x1AFF) ||
         (cp >= 0x20D0 && cp <= 0x20FF) ||
         (cp >= 0xFE20 && cp <= 0xFE2F);
}

static int retEhSeparador(long cp) {
  if (cp < 0x80) {
    return !((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') ||
             (cp >= '0' && cp <= '9'));
  }
  if (cp == 0xAA || cp == 0xBA || cp == 0xB0) return 0;
  if (cp >= 0xC0 && cp <= 0xFF) return retBaseLatim(cp) == 0 && cp != 0xC6 &&
    cp != 0xD0 && cp != 0xD8 && cp != 0xDE && cp != 0xDF && cp != 0xE6 &&
    cp != 0xF0 && cp != 0xF8 && cp != 0xFE;
  return cp == 0xA0 || cp == 0x2028 || cp == 0x2029 ||
         (cp >= 0xA1 && cp <= 0xBF) ||
         (cp >= 0x2000 && cp <= 0x206F) ||
         (cp >= 0x3001 && cp <= 0x303F) ||
         (cp >= 0xFF01 && cp <= 0xFF65);
}

static int retTemIraFuturo(const char *s) {
  const char *p = s, *fim = s + strlen(s);
  int sepAntes = 1;
  while (p < fim) {
    long cp = lerUtf8(&p, fim);
    if (cp < 0) return 1;
    if (retEhCombinante(cp)) continue;
    if (sepAntes && (cp == 'i' || cp == 'I')) {
      const char *q = p;
      long c2 = lerUtf8(&q, fim);
      if (c2 == 'r' || c2 == 'R') {
        long c3 = lerUtf8(&q, fim);
        int ehA = (c3 == 0xE1 || c3 == 0xC1);
        if (!ehA && (c3 == 'a' || c3 == 'A')) {
          const char *q2 = q;
          if (lerUtf8(&q2, fim) == 0x0301) { ehA = 1; q = q2; }
        }
        if (ehA) {
          long c4 = lerUtf8(&q, fim);
          if (c4 == ' ' || c4 == '\t') {
            while (q < fim) {
              long c5 = lerUtf8(&q, fim);
              if (c5 != ' ' && c5 != '\t') {
                if (c5 >= 0 && !retEhSeparador(c5)) return 1;
                break;
              }
            }
          }
        }
      }
    }
    sepAntes = retEhSeparador(cp);
  }
  return 0;
}

static int retEhNaoFuturoAra(const char *s) {
  static const char *const exc[] = {
    "alvar\xc3\xa1", "guaran\xc3\xa1", "par\xc3\xa1", "maracuj\xc3\xa1", "caj\xc3\xa1",
    "car\xc3\xa1", "bacar\xc3\xa1", "man\xc3\xa1", "saar\xc3\xa1", "sahar\xc3\xa1",
  };
  size_t i;
  for (i = 0; i < sizeof exc / sizeof exc[0]; i++) {
    if (!strcmp(s, exc[i])) return 1;
  }
  return 0;
}

static int retTemFuturoAcentuado(const char *s) {
  const char *p = s, *fim = s + strlen(s);
  char pal[96];
  size_t len = 0;
  int temDigito = 0;
  static const char *const sufs[] = {
    "ar\xc3\xa1", "er\xc3\xa1", "ir\xc3\xa1",
    "ar\xc3\xa3o", "er\xc3\xa3o", "ir\xc3\xa3o",
    "ar\xc3\xa1n", "er\xc3\xa1n", "ir\xc3\xa1n",
  };
  for (;;) {
    long cp = p < fim ? lerUtf8(&p, fim) : -2;
    if (cp == -1) return 1;
    if (cp >= 0 && retEhCombinante(cp)) {
      if ((cp == 0x0301 || cp == 0x0303) && len > 0 && pal[len - 1] == 'a' && len + 1 < sizeof pal) {
        pal[len - 1] = (char)0xC3;
        pal[len++] = (char)(cp == 0x0301 ? 0xA1 : 0xA3);
      }
      continue;
    }
    if (cp >= 0 && !retEhSeparador(cp)) {
      if (cp >= '0' && cp <= '9') temDigito = 1;
      if (cp >= 'A' && cp <= 'Z') cp += 32;
      else if (cp >= 0xC0 && cp <= 0xDD && cp != 0xD7) cp += 32;
      if (cp < 0x80) {
        if (len + 1 < sizeof pal) pal[len++] = (char)cp;
      } else if (cp < 0x800) {
        if (len + 2 < sizeof pal) {
          pal[len++] = (char)(0xC0 | (cp >> 6));
          pal[len++] = (char)(0x80 | (cp & 0x3F));
        }
      } else if (cp < 0x10000) {
        if (len + 3 < sizeof pal) {
          pal[len++] = (char)(0xE0 | (cp >> 12));
          pal[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
          pal[len++] = (char)(0x80 | (cp & 0x3F));
        }
      } else {
        if (len + 4 < sizeof pal) {
          pal[len++] = (char)(0xF0 | (cp >> 18));
          pal[len++] = (char)(0x80 | ((cp >> 12) & 0x3F));
          pal[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
          pal[len++] = (char)(0x80 | (cp & 0x3F));
        }
      }
      continue;
    }
    if (len > 0 && !temDigito) {
      size_t i;
      pal[len] = '\0';
      if (!retEhNaoFuturoAra(pal)) {
        for (i = 0; i < sizeof sufs / sizeof sufs[0]; i++) {
          size_t sl = strlen(sufs[i]);
          if (len > sl && memcmp(pal + len - sl, sufs[i], sl) == 0) {
            size_t rad = len - sl, k, cps = 0;
            for (k = 0; k < rad; k++) {
              if (((unsigned char)pal[k] & 0xC0) != 0x80) cps++;
            }
            if (cps >= 2) return 1;
          }
        }
      }
    }
    len = 0;
    temDigito = 0;
    if (cp == -2) break;
  }
  return 0;
}

static int retEhWillFuturoSeg(const char *s) {
  static const char *const seg[] = {
    "he", "she", "it", "they", "we", "you", "i", "there", "this", "that", "these", "those",
    "anyone", "someone", "everyone", "nobody", "noone",
    "anything", "something", "everything", "nothing",
    "not", "never", "still", "also", "soon", "eventually", "finally", "probably", "likely",
    "be", "have", "do", "go", "come", "see", "say", "make", "take", "know", "get", "give",
    "find", "tell", "ask", "try", "leave", "die", "live", "kill", "save", "escape", "return",
    "reveal", "discover", "change", "betray", "win", "lose", "survive", "fight", "hide", "flee",
    "run", "fall", "meet", "keep", "hold", "hear", "think", "bring", "break", "speak", "help",
    "stay", "wait", "stop", "start", "begin", "learn", "pay", "send", "move", "turn", "face",
    "follow", "lead", "watch", "show", "use", "need", "want", "become", "happen", "end",
  };
  size_t i;
  for (i = 0; i < sizeof seg / sizeof seg[0]; i++) {
    if (!strcmp(s, seg[i])) return 1;
  }
  return 0;
}

static int retTemWillFuturo(const char *s) {
  char orig[320], low[320];
  char *tokO[64], *tokL[64];
  const char *p = s, *fim = s + strlen(s);
  size_t uo = 0, ul = 0;
  int nt = 0, emEspaco = 1, i;
  int temPergunta = strchr(s, '?') != NULL;

  while (p < fim && uo + 5 < sizeof orig && ul + 5 < sizeof low) {
    const char *ini = p;
    long cp = lerUtf8(&p, fim);
    char b;
    if (cp < 0) return 1;
    if (retEhCombinante(cp)) continue;
    if (retEhSeparador(cp)) { emEspaco = 1; continue; }
    if (uo > 0 && emEspaco) { orig[uo++] = ' '; low[ul++] = ' '; }
    emEspaco = 0;
    if (cp < 0x80) {
      orig[uo++] = (char)cp;
      low[ul++] = (cp >= 'A' && cp <= 'Z') ? (char)(cp + 32) : (char)cp;
    } else {
      const char *q = ini;
      while (q < p && uo + 1 < sizeof orig) orig[uo++] = *q++;
      b = retBaseLatim(cp);
      if (b) low[ul++] = b;
      else {
        q = ini;
        while (q < p && ul + 1 < sizeof low) low[ul++] = *q++;
      }
    }
  }
  orig[uo] = '\0';
  low[ul] = '\0';

  {
    char *qo = orig, *ql = low;
    while (*qo && *ql && nt < 64) {
      while (*qo == ' ') qo++;
      while (*ql == ' ') ql++;
      if (!*qo || !*ql) break;
      tokO[nt] = qo;
      tokL[nt] = ql;
      nt++;
      while (*qo && *qo != ' ') qo++;
      while (*ql && *ql != ' ') ql++;
      if (*qo == ' ') *qo++ = '\0';
      if (*ql == ' ') *ql++ = '\0';
    }
  }

  for (i = 0; i < nt; i++) {
    if ((!strcmp(tokO[i], "will") || !strcmp(tokO[i], "WILL")) && i + 1 < nt)
      return 1;
    if (!strcmp(tokO[i], "Will") && i + 1 < nt) {
      if (i == 0 && temPergunta) return 1;
      if (retEhWillFuturoSeg(tokL[i + 1])) return 1;
    }
  }
  return 0;
}

static int retParseInt(const char *s, int *out) {
  long v = 0;
  int n = 0;
  if (!s || !*s) return 0;
  while (s[n] >= '0' && s[n] <= '9') {
    v = v * 10 + (s[n] - '0');
    if (v > 100000) v = 100000;
    n++;
  }
  if (!n || s[n] != '\0') return 0;
  *out = (int)v;
  return 1;
}

static int retParseOrdNum(const char *s, int *out) {
  long v = 0;
  int n = 0;
  const char *suf;
  if (!s || !*s) return 0;
  while (s[n] >= '0' && s[n] <= '9') {
    v = v * 10 + (s[n] - '0');
    if (v > 100000) v = 100000;
    n++;
  }
  if (!n) return 0;
  suf = s + n;
  if (!suf[0] || !strcmp(suf, "a") || !strcmp(suf, "o") ||
      !strcmp(suf, "st") || !strcmp(suf, "nd") ||
      !strcmp(suf, "rd") || !strcmp(suf, "th")) {
    *out = (int)v;
    return 1;
  }
  return 0;
}

static int retParseTxEy(const char *s, int *tOut, int *eOut) {
  long tv = 0, ev = 0;
  int nt = 0, ne = 0;
  if (!s || (s[0] != 't' && s[0] != 's')) return 0;
  s++;
  while (*s >= '0' && *s <= '9') {
    tv = tv * 10 + (*s++ - '0');
    if (tv > 100000) tv = 100000;
    nt++;
  }
  if (!nt || *s != 'e') return 0;
  s++;
  while (*s >= '0' && *s <= '9') {
    ev = ev * 10 + (*s++ - '0');
    if (ev > 100000) ev = 100000;
    ne++;
  }
  if (!ne || *s != '\0') return 0;
  *tOut = (int)tv;
  *eOut = (int)ev;
  return 1;
}

static int retCardinalUnidade(const char *s) {
  static const struct { const char *p; int v; } tab[] = {
    { "um", 1 }, { "uma", 1 }, { "one", 1 }, { "uno", 1 }, { "un", 1 }, { "una", 1 },
    { "dois", 2 }, { "duas", 2 }, { "two", 2 }, { "dos", 2 },
    { "tres", 3 }, { "three", 3 },
    { "quatro", 4 }, { "four", 4 }, { "cuatro", 4 },
    { "cinco", 5 }, { "five", 5 },
    { "seis", 6 }, { "six", 6 },
    { "sete", 7 }, { "seven", 7 }, { "siete", 7 },
    { "oito", 8 }, { "eight", 8 }, { "ocho", 8 },
    { "nove", 9 }, { "nine", 9 }, { "nueve", 9 },
  };
  size_t i;
  for (i = 0; i < sizeof tab / sizeof tab[0]; i++) {
    if (!strcmp(s, tab[i].p)) return tab[i].v;
  }
  return 0;
}

static int retCardinalDez(const char *s) {
  static const struct { const char *p; int v; } tab[] = {
    { "dez", 10 }, { "ten", 10 }, { "diez", 10 },
    { "onze", 11 }, { "eleven", 11 }, { "once", 11 },
    { "doze", 12 }, { "twelve", 12 }, { "doce", 12 },
    { "treze", 13 }, { "thirteen", 13 }, { "trece", 13 },
    { "catorze", 14 }, { "quatorze", 14 }, { "fourteen", 14 }, { "catorce", 14 },
    { "quinze", 15 }, { "fifteen", 15 }, { "quince", 15 },
    { "dezesseis", 16 }, { "dezasseis", 16 }, { "sixteen", 16 }, { "dieciseis", 16 },
    { "dezessete", 17 }, { "dezassete", 17 }, { "seventeen", 17 }, { "diecisiete", 17 },
    { "dezoito", 18 }, { "eighteen", 18 }, { "dieciocho", 18 },
    { "dezenove", 19 }, { "dezanove", 19 }, { "nineteen", 19 }, { "diecinueve", 19 },
    { "veintiuno", 21 }, { "veintiun", 21 }, { "veintiuna", 21 },
    { "veintidos", 22 }, { "veintitres", 23 }, { "veinticuatro", 24 }, { "veinticinco", 25 },
    { "veintiseis", 26 }, { "veintisiete", 27 }, { "veintiocho", 28 }, { "veintinueve", 29 },
  };
  size_t i;
  for (i = 0; i < sizeof tab / sizeof tab[0]; i++) {
    if (!strcmp(s, tab[i].p)) return tab[i].v;
  }
  return 0;
}

static int retCardinalDezena(const char *s) {
  static const struct { const char *p; int v; } tab[] = {
    { "vinte", 20 }, { "twenty", 20 }, { "veinte", 20 },
    { "trinta", 30 }, { "thirty", 30 }, { "treinta", 30 },
    { "quarenta", 40 }, { "forty", 40 }, { "cuarenta", 40 },
    { "cinquenta", 50 }, { "fifty", 50 }, { "cincuenta", 50 },
    { "sessenta", 60 }, { "sixty", 60 }, { "sesenta", 60 },
    { "setenta", 70 }, { "seventy", 70 },
    { "oitenta", 80 }, { "eighty", 80 }, { "ochenta", 80 },
    { "noventa", 90 }, { "ninety", 90 },
  };
  size_t i;
  for (i = 0; i < sizeof tab / sizeof tab[0]; i++) {
    if (!strcmp(s, tab[i].p)) return tab[i].v;
  }
  return 0;
}

static int retOrdinalUnidade(const char *s) {
  static const struct { const char *p; int v; } tab[] = {
    { "primeiro", 1 }, { "primeira", 1 }, { "primeiros", 1 }, { "primeiras", 1 },
    { "primero", 1 }, { "primera", 1 }, { "primer", 1 }, { "primeros", 1 }, { "primeras", 1 }, { "first", 1 },
    { "segundo", 2 }, { "segunda", 2 }, { "segundos", 2 }, { "segundas", 2 }, { "second", 2 },
    { "terceiro", 3 }, { "terceira", 3 }, { "terceiros", 3 }, { "terceiras", 3 },
    { "tercero", 3 }, { "tercera", 3 }, { "tercer", 3 }, { "terceros", 3 }, { "terceras", 3 }, { "third", 3 },
    { "quarto", 4 }, { "quarta", 4 }, { "quartos", 4 }, { "quartas", 4 },
    { "cuarto", 4 }, { "cuarta", 4 }, { "cuartos", 4 }, { "cuartas", 4 }, { "fourth", 4 },
    { "quinto", 5 }, { "quinta", 5 }, { "quintos", 5 }, { "quintas", 5 }, { "fifth", 5 },
    { "sexto", 6 }, { "sexta", 6 }, { "sextos", 6 }, { "sextas", 6 }, { "sixth", 6 },
    { "setimo", 7 }, { "setima", 7 }, { "setimos", 7 }, { "setimas", 7 },
    { "septimo", 7 }, { "septima", 7 }, { "septimos", 7 }, { "septimas", 7 }, { "seventh", 7 },
    { "oitavo", 8 }, { "oitava", 8 }, { "oitavos", 8 }, { "oitavas", 8 },
    { "octavo", 8 }, { "octava", 8 }, { "octavos", 8 }, { "octavas", 8 }, { "eighth", 8 },
    { "nono", 9 }, { "nona", 9 }, { "nonos", 9 }, { "nonas", 9 },
    { "noveno", 9 }, { "novena", 9 }, { "novenos", 9 }, { "novenas", 9 }, { "ninth", 9 },
  };
  size_t i;
  for (i = 0; i < sizeof tab / sizeof tab[0]; i++) {
    if (!strcmp(s, tab[i].p)) return tab[i].v;
  }
  return 0;
}

static int retOrdinalSimples(const char *s) {
  static const struct { const char *p; int v; } tab[] = {
    { "undecimo", 11 }, { "undecima", 11 }, { "onzeno", 11 }, { "onzena", 11 }, { "eleventh", 11 },
    { "duodecimo", 12 }, { "duodecima", 12 }, { "twelfth", 12 },
    { "thirteenth", 13 }, { "fourteenth", 14 }, { "fifteenth", 15 }, { "sixteenth", 16 },
    { "seventeenth", 17 }, { "eighteenth", 18 }, { "nineteenth", 19 },
  };
  size_t i;
  for (i = 0; i < sizeof tab / sizeof tab[0]; i++) {
    if (!strcmp(s, tab[i].p)) return tab[i].v;
  }
  return 0;
}

static int retOrdinalDezena(const char *s) {
  static const struct { const char *p; int v; } tab[] = {
    { "decimo", 10 }, { "decima", 10 }, { "decimos", 10 }, { "decimas", 10 }, { "tenth", 10 },
    { "vigesimo", 20 }, { "vigesima", 20 }, { "twentieth", 20 },
    { "trigesimo", 30 }, { "trigesima", 30 }, { "thirtieth", 30 },
    { "quadragesimo", 40 }, { "quadragesima", 40 }, { "fortieth", 40 },
    { "quinquagesimo", 50 }, { "quinquagesima", 50 }, { "fiftieth", 50 },
    { "sexagesimo", 60 }, { "sexagesima", 60 }, { "sixtieth", 60 },
    { "septuagesimo", 70 }, { "septuagesima", 70 }, { "setuagesimo", 70 }, { "setuagesima", 70 }, { "seventieth", 70 },
    { "octogesimo", 80 }, { "octogesima", 80 }, { "eightieth", 80 },
    { "nonagesimo", 90 }, { "nonagesima", 90 }, { "ninetieth", 90 },
  };
  size_t i;
  for (i = 0; i < sizeof tab / sizeof tab[0]; i++) {
    if (!strcmp(s, tab[i].p)) return tab[i].v;
  }
  return 0;
}

static int retOrdinalFused(const char *s) {
  static const struct { const char *p; int v; } pref[] = {
    { "decimo", 10 }, { "decima", 10 }, { "vigesimo", 20 }, { "vigesima", 20 },
    { "trigesimo", 30 }, { "trigesima", 30 }, { "quadragesimo", 40 }, { "quadragesima", 40 },
    { "quinquagesimo", 50 }, { "quinquagesima", 50 }, { "sexagesimo", 60 }, { "sexagesima", 60 },
    { "septuagesimo", 70 }, { "septuagesima", 70 }, { "octogesimo", 80 }, { "octogesima", 80 },
    { "nonagesimo", 90 }, { "nonagesima", 90 },
  };
  size_t i;
  for (i = 0; i < sizeof pref / sizeof pref[0]; i++) {
    size_t pl = strlen(pref[i].p);
    if (strncmp(s, pref[i].p, pl) == 0 && s[pl] != '\0') {
      int u = retOrdinalUnidade(s + pl);
      if (u > 0) return pref[i].v + u;
    }
  }
  if (strncmp(s, "decim", 5) == 0 && s[5] != '\0') {
    if (retOrdinalUnidade(s + 5) == 8) return 18;
  }
  return 0;
}

static int retEhArtigoUm(const char *s) {
  return !strcmp(s, "um") || !strcmp(s, "uma") || !strcmp(s, "un") || !strcmp(s, "una") ||
         !strcmp(s, "a") || !strcmp(s, "an");
}

static int retParseNumeroExtenso(char *const *tok, const int *quebra, int nt, int i,
                                 int *lenOut, int *artigoOut) {
  int v;
  *lenOut = 0;
  *artigoOut = 0;
  if (retParseOrdNum(tok[i], &v)) { *lenOut = 1; return v; }
  if ((v = retOrdinalUnidade(tok[i])) > 0) { *lenOut = 1; return v; }
  if ((v = retOrdinalSimples(tok[i])) > 0) { *lenOut = 1; return v; }
  if ((v = retOrdinalFused(tok[i])) > 0) { *lenOut = 1; return v; }
  if ((v = retOrdinalDezena(tok[i])) > 0) {
    if (i + 1 < nt && !quebra[i + 1]) {
      int u = retOrdinalUnidade(tok[i + 1]);
      if (u > 0) { *lenOut = 2; return v + u; }
      if ((!strcmp(tok[i + 1], "e") || !strcmp(tok[i + 1], "y")) &&
          i + 2 < nt && !quebra[i + 2]) {
        u = retOrdinalUnidade(tok[i + 2]);
        if (u > 0) { *lenOut = 3; return v + u; }
      }
    }
    *lenOut = 1;
    return v;
  }
  if ((v = retCardinalDezena(tok[i])) > 0) {
    if (i + 1 < nt && !quebra[i + 1]) {
      int u = retCardinalUnidade(tok[i + 1]);
      if (!u) u = retOrdinalUnidade(tok[i + 1]);
      if (u > 0) { *lenOut = 2; return v + u; }
      if ((!strcmp(tok[i + 1], "e") || !strcmp(tok[i + 1], "y")) &&
          i + 2 < nt && !quebra[i + 2]) {
        u = retCardinalUnidade(tok[i + 2]);
        if (!u) u = retOrdinalUnidade(tok[i + 2]);
        if (u > 0) { *lenOut = 3; return v + u; }
      }
    }
    *lenOut = 1;
    return v;
  }
  if ((v = retCardinalDez(tok[i])) > 0) {
    if (!strcmp(tok[i], "diez") && i + 2 < nt && !quebra[i + 1] &&
        !strcmp(tok[i + 1], "y") && !quebra[i + 2]) {
      int u = retCardinalUnidade(tok[i + 2]);
      if (u > 0) { *lenOut = 3; return 10 + u; }
    }
    *lenOut = 1;
    return v;
  }
  if ((v = retCardinalUnidade(tok[i])) > 0) {
    *lenOut = 1;
    *artigoOut = retEhArtigoUm(tok[i]);
    return v;
  }
  return 0;
}

static int retEhFuturoSimples(const char *s) {
  static const char *const ban[] = {
    "futuro", "futura", "futuros", "futuras", "upcoming", "spoiler", "spoilers",
  };
  size_t i;
  for (i = 0; i < sizeof ban / sizeof ban[0]; i++) {
    if (!strcmp(s, ban[i])) return 1;
  }
  return 0;
}

static int retEhSubstEp(const char *s) {
  return !strcmp(s, "episodio") || !strcmp(s, "episodios") ||
         !strcmp(s, "episode") || !strcmp(s, "episodes") ||
         !strcmp(s, "capitulo") || !strcmp(s, "capitulos") ||
         !strcmp(s, "chapter") || !strcmp(s, "chapters") ||
         !strcmp(s, "ep") || !strcmp(s, "eps") || !strcmp(s, "cap");
}

static int retEhSubstTemp(const char *s) {
  return !strcmp(s, "temporada") || !strcmp(s, "temporadas") ||
         !strcmp(s, "season") || !strcmp(s, "seasons") ||
         !strcmp(s, "temp") || !strcmp(s, "seas");
}

static int retEhSubstPlural(const char *s) {
  return !strcmp(s, "episodios") || !strcmp(s, "episodes") ||
         !strcmp(s, "capitulos") || !strcmp(s, "chapters") ||
         !strcmp(s, "eps") || !strcmp(s, "temporadas") ||
         !strcmp(s, "seasons") || !strcmp(s, "seas");
}

static int retEhQuantPlural(const char *s) {
  static const char *const q[] = {
    "several", "few", "many", "some", "multiple", "various", "other",
    "varios", "varias", "alguns", "algumas", "algunos", "algunas",
    "poucos", "poucas", "pocos", "pocas", "muitos", "muitas", "muchos", "muchas",
    "outros", "outras", "otros", "otras",
  };
  size_t i;
  for (i = 0; i < sizeof q / sizeof q[0]; i++) {
    if (!strcmp(s, q[i])) return 1;
  }
  return 0;
}

static int retEhQuantPar(const char *s) {
  return !strcmp(s, "couple") || !strcmp(s, "pair") || !strcmp(s, "par");
}

static int retEhProximo(const char *s) {
  return !strcmp(s, "proximo") || !strcmp(s, "proxima") ||
         !strcmp(s, "proximos") || !strcmp(s, "proximas") ||
         !strcmp(s, "seguinte") || !strcmp(s, "seguintes") ||
         !strcmp(s, "siguiente") || !strcmp(s, "siguientes");
}

static int retEhNextAlvo(const char *s) {
  return !strcmp(s, "episode") || !strcmp(s, "episodes") ||
         !strcmp(s, "ep") || !strcmp(s, "eps") ||
         !strcmp(s, "season") || !strcmp(s, "seasons") ||
         !strcmp(s, "chapter") || !strcmp(s, "chapters") ||
         !strcmp(s, "time") || !strcmp(s, "times");
}

static int retEhProxAlvo(const char *s) {
  return retEhSubstEp(s) || retEhSubstTemp(s) ||
         !strcmp(s, "vez") || !strcmp(s, "vezes") ||
         !strcmp(s, "time") || !strcmp(s, "times");
}

static int retEhArtProx(const char *s) {
  return !strcmp(s, "no") || !strcmp(s, "na") || !strcmp(s, "nos") || !strcmp(s, "nas") ||
         !strcmp(s, "el") || !strcmp(s, "la") || !strcmp(s, "los") || !strcmp(s, "las");
}

static int retEhPrepEspacial(const char *s) {
  return !strcmp(s, "a") || !strcmp(s, "ao") || !strcmp(s, "aos") || !strcmp(s, "as") ||
         !strcmp(s, "de") || !strcmp(s, "da") || !strcmp(s, "do") ||
         !strcmp(s, "das") || !strcmp(s, "dos") || !strcmp(s, "del");
}

static int retEhNumLig(const char *s) {
  return !strcmp(s, "n") || !strcmp(s, "no") || !strcmp(s, "num") ||
         !strcmp(s, "nro") || !strcmp(s, "nr") ||
         !strcmp(s, "numero") || !strcmp(s, "number");
}

static int retEhAbrevPonto(const char *s) {
  return !strcmp(s, "n") || !strcmp(s, "no") || !strcmp(s, "num") ||
         !strcmp(s, "nro") || !strcmp(s, "nr") ||
         !strcmp(s, "ep") || !strcmp(s, "eps") || !strcmp(s, "cap") ||
         !strcmp(s, "temp") || !strcmp(s, "seas");
}

static int retEhMarcadorTemp(char *const *tok, const int *quebra, int nt, int i) {
  static const char *const m1[] = {
    "later", "after", "afterward", "afterwards", "ahead", "following",
    "depois", "adiante", "apos", "despues", "adelante",
  };
  size_t k;
  if (i >= nt || quebra[i]) return 0;
  for (k = 0; k < sizeof m1 / sizeof m1[0]; k++) {
    if (!strcmp(tok[i], m1[k])) return 1;
  }
  if (i + 1 >= nt || quebra[i + 1]) return 0;
  return (!strcmp(tok[i], "from") && !strcmp(tok[i + 1], "now")) ||
         (!strcmp(tok[i], "mais") && !strcmp(tok[i + 1], "tarde")) ||
         (!strcmp(tok[i], "mas") && (!strcmp(tok[i + 1], "tarde") || !strcmp(tok[i + 1], "adelante"))) ||
         (!strcmp(tok[i], "a") && !strcmp(tok[i + 1], "frente"));
}

static int retEhAfterDemonstrativo(char *const *tok, const int *quebra, int nt, int i) {
  static const char *const dem[] = {
    "this", "that", "it", "current",
    "este", "esta", "esse", "essa", "deste", "desta", "desse", "dessa",
    "isto", "isso", "disto", "disso", "aquele", "aquela", "daquele", "daquela",
    "ese", "esa", "esto", "eso", "aquel", "aquella", "atual", "actual",
  };
  size_t k;
  int j;
  if (i >= nt || quebra[i]) return 0;
  if (strcmp(tok[i], "after") && strcmp(tok[i], "following") &&
      strcmp(tok[i], "depois") && strcmp(tok[i], "apos") && strcmp(tok[i], "despues")) {
    return 0;
  }
  j = i + 1;
  if (j < nt && !quebra[j] && (!strcmp(tok[j], "de") || !strcmp(tok[j], "of"))) j++;
  if (j >= nt || quebra[j]) return 0;
  for (k = 0; k < sizeof dem / sizeof dem[0]; k++) {
    if (!strcmp(tok[j], dem[k])) return 1;
  }
  return 0;
}

// Coerente com validarResumoRetomada (farolgosto.js): recusa referencias a
// outro episodio/temporada (numericas ou por extenso) e futuro/spoiler.
static int retomadaResumoSemSpoiler(const char *s, int temporada, int episodio) {
  char pre[320], fold[320];
  char *tok[64];
  int quebra[64];
  const char *p, *fim;
  size_t up = 0, uf = 0, idx;
  int nt = 0, i, qPend = 0;

  if (!s || !s[0] || temporada < 1 || episodio < 1) return 0;
  if (retTemIraFuturo(s) || retTemFuturoAcentuado(s) || retTemWillFuturo(s)) return 0;

  // Passo 1: dobra acentos e maiusculas mantendo '.', ',', ';', '!', '?' para
  // colapsar ordinais pontuados (3.a -> 3a) e preservar quebras de oracao.
  p = s;
  fim = s + strlen(s);
  while (p < fim && up + 5 < sizeof pre) {
    const char *ini = p;
    long cp = lerUtf8(&p, fim);
    char b;
    if (cp < 0) return 0;
    if (retEhCombinante(cp)) continue;
    if (cp == '.' || cp == ',' || cp == ';' || cp == '!' || cp == '?') {
      pre[up++] = (char)cp;
      continue;
    }
    if (retEhSeparador(cp)) {
      if (up > 0 && pre[up - 1] != ' ') pre[up++] = ' ';
      continue;
    }
    if (cp == 0xAA) { pre[up++] = 'a'; continue; }
    if (cp == 0xBA || cp == 0xB0) { pre[up++] = 'o'; continue; }
    if (cp >= 'A' && cp <= 'Z') { pre[up++] = (char)(cp + 32); continue; }
    if ((cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9')) {
      pre[up++] = (char)cp;
      continue;
    }
    b = retBaseLatim(cp);
    if (b) { pre[up++] = b; continue; }
    while (ini < p && up + 1 < sizeof pre) pre[up++] = *ini++;
  }
  pre[up] = '\0';

  // Passo 2: colapsa (\d+)\s*\.\s*([ao])\b -> $1$2 e converte separadores em ' '
  // marcando quebras de oracao com '\x01'.
  idx = 0;
  while (idx < up && uf + 2 < sizeof fold) {
    unsigned char ch = (unsigned char)pre[idx];
    if (ch >= '0' && ch <= '9') {
      size_t dFim = idx;
      while (dFim < up && pre[dFim] >= '0' && pre[dFim] <= '9') dFim++;
      {
        size_t k = dFim;
        while (k < up && pre[k] == ' ') k++;
        if (k < up && pre[k] == '.') {
          k++;
          while (k < up && pre[k] == ' ') k++;
          if (k < up && (pre[k] == 'a' || pre[k] == 'o')) {
            unsigned char seg = (k + 1 < up) ? (unsigned char)pre[k + 1] : 0;
            int lim = (seg == 0 || seg == ' ' || seg == '.' || seg == ',' ||
                       seg == ';' || seg == '!' || seg == '?');
            if (lim) {
              while (idx < dFim && uf + 2 < sizeof fold) fold[uf++] = pre[idx++];
              fold[uf++] = pre[k];
              idx = k + 1;
              continue;
            }
          }
        }
      }
    }
    if (ch == ',' || ch == ';' || ch == '.' || ch == '!' || ch == '?') {
      int abrev = 0;
      if (ch == '.' && idx > 0) {
        size_t ini = idx;
        while (ini > 0 && pre[ini - 1] != ' ' &&
               pre[ini - 1] != '.' && pre[ini - 1] != ',' &&
               pre[ini - 1] != ';' && pre[ini - 1] != '!' && pre[ini - 1] != '?') {
          ini--;
        }
        if (idx > ini && idx - ini < 16) {
          char ab[16];
          memcpy(ab, pre + ini, idx - ini);
          ab[idx - ini] = '\0';
          abrev = retEhAbrevPonto(ab);
        }
      }
      fold[uf++] = abrev ? ' ' : '\x01';
      idx++;
      continue;
    }
    fold[uf++] = (char)ch;
    idx++;
  }
  fold[uf] = '\0';

  {
    char *q = fold;
    while (*q && nt < 64) {
      while (*q == ' ' || *q == '\x01') {
        if (*q == '\x01') qPend = 1;
        q++;
      }
      if (!*q) break;
      tok[nt] = q;
      quebra[nt] = qPend;
      nt++;
      qPend = 0;
      while (*q && *q != ' ' && *q != '\x01') q++;
      if (*q == '\x01') { qPend = 1; *q++ = '\0'; }
      else if (*q == ' ') { *q++ = '\0'; }
    }
  }

  for (i = 0; i < nt; i++) {
    int tVal = 0, eVal = 0, num = 0, len = 0, art = 0, j;
    if (retEhFuturoSimples(tok[i])) return 0;
    if (!strcmp(tok[i], "vai") && i + 1 < nt && !quebra[i + 1]) return 0;
    if (!strcmp(tok[i], "va") && i + 2 < nt && !quebra[i + 1] &&
        !strcmp(tok[i + 1], "a") && !quebra[i + 2])
      return 0;
    if ((!strcmp(tok[i], "em") || !strcmp(tok[i], "en")) &&
        i + 1 < nt && !quebra[i + 1] && !strcmp(tok[i + 1], "breve"))
      return 0;
    if (!strcmp(tok[i], "later") && i + 1 < nt && !quebra[i + 1] && !strcmp(tok[i + 1], "in"))
      return 0;
    if (!strcmp(tok[i], "season") && i + 1 < nt && !quebra[i + 1] && !strcmp(tok[i + 1], "finale"))
      return 0;
    if (!strcmp(tok[i], "final") && i + 2 < nt && !quebra[i + 1] &&
        (!strcmp(tok[i + 1], "da") || !strcmp(tok[i + 1], "de")) &&
        !quebra[i + 2] && !strcmp(tok[i + 2], "temporada"))
      return 0;
    if (!strcmp(tok[i], "next") && i + 1 < nt && !quebra[i + 1] && retEhNextAlvo(tok[i + 1]))
      return 0;
    if (retEhProximo(tok[i])) {
      if (i > 0 && !quebra[i] && (retEhSubstEp(tok[i - 1]) || retEhSubstTemp(tok[i - 1])))
        return 0;
      if (i + 1 < nt && !quebra[i + 1] && retEhProxAlvo(tok[i + 1]))
        return 0;
      if (i > 0 && !quebra[i] && retEhArtProx(tok[i - 1]) &&
          (i + 1 >= nt || quebra[i + 1] || !retEhPrepEspacial(tok[i + 1])))
        return 0;
    }

    if (retParseTxEy(tok[i], &tVal, &eVal) &&
        (tVal != temporada || eVal != episodio))
      return 0;
    if ((tok[i][0] == 't' || tok[i][0] == 's') &&
        retParseInt(tok[i] + 1, &tVal) && i + 1 < nt && !quebra[i + 1] &&
        tok[i + 1][0] == 'e' && retParseInt(tok[i + 1] + 1, &eVal) &&
        (tVal != temporada || eVal != episodio))
      return 0;
    if (tok[i][0] == 'e' && tok[i][1] == 'p' &&
        retParseInt(tok[i] + 2, &num) && num != episodio)
      return 0;

    if (retEhSubstPlural(tok[i]) && retEhMarcadorTemp(tok, quebra, nt, i + 1)) return 0;
    if ((retEhSubstEp(tok[i]) || retEhSubstTemp(tok[i])) &&
        retEhMarcadorTemp(tok, quebra, nt, i + 1))
      return 0;
    if (retEhQuantPlural(tok[i]) && i + 1 < nt && !quebra[i + 1] &&
        retEhSubstPlural(tok[i + 1]))
      return 0;
    if (retEhQuantPar(tok[i])) {
      j = i + 1;
      if (j < nt && !quebra[j] && (!strcmp(tok[j], "of") || !strcmp(tok[j], "de"))) j++;
      if (j < nt && !quebra[j] && retEhSubstPlural(tok[j])) return 0;
    }
    if ((retEhSubstEp(tok[i]) || retEhSubstTemp(tok[i])) &&
        retEhAfterDemonstrativo(tok, quebra, nt, i + 1))
      return 0;

    if (retEhSubstEp(tok[i])) {
      j = i + 1;
      if (j < nt && !quebra[j] && retEhNumLig(tok[j])) j++;
      if (j < nt && !quebra[j]) {
        num = retParseNumeroExtenso(tok, quebra, nt, j, &len, &art);
        if (num > 0 && (retEhSubstPlural(tok[i]) || num != episodio)) return 0;
      }
    }
    if (retEhSubstTemp(tok[i])) {
      j = i + 1;
      if (j < nt && !quebra[j] && retEhNumLig(tok[j])) j++;
      if (j < nt && !quebra[j]) {
        num = retParseNumeroExtenso(tok, quebra, nt, j, &len, &art);
        if (num > 0 && (retEhSubstPlural(tok[i]) || num != temporada)) return 0;
      }
    }

    num = retParseNumeroExtenso(tok, quebra, nt, i, &len, &art);
    if (num == 0 && retEhArtigoUm(tok[i])) {
      num = 1;
      len = 1;
      art = 1;
    }
    if (num > 0 && i + len < nt && !quebra[i + len]) {
      int posAlvo = i + len;
      const char *alvo = tok[posAlvo];
      if (retEhSubstPlural(alvo)) return 0;
      if (retEhSubstEp(alvo) || retEhSubstTemp(alvo)) {
        if (retEhMarcadorTemp(tok, quebra, nt, posAlvo + 1)) return 0;
        if (!(art && num == 1)) {
          if (retEhSubstEp(alvo) && num != episodio) return 0;
          if (retEhSubstTemp(alvo) && num != temporada) return 0;
        }
      }
    }
    if (num > 0 && len > 1) i += len - 1;
  }
  return 1;
}

static int idiomaValido(const char *s) {
  size_t n, i;
  if (!s || !s[0]) return 0;
  n = strlen(s);
  if (n < 2 || n > 7) return 0;
  if (!(s[0] >= 'a' && s[0] <= 'z') || !(s[1] >= 'a' && s[1] <= 'z')) return 0;
  if (n == 2) return 1;
  if (s[2] != '-' || n < 5) return 0;
  for (i = 3; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return 0;
  }
  return 1;
}

static int imdbValido(const char *s) {
  size_t n = 0;
  if (!s || s[0] != 't' || s[1] != 't') return 0;
  s += 2;
  while (isdigit((unsigned char)*s)) { s++; n++; }
  return *s == '\0' && n >= 1 && n <= 17;
}

static const char *generoPublicoNome(int32_t id) {
  static const struct { int32_t id; const char *nome; } G[] = {
    { 28, "Acao" }, { 12, "Aventura" }, { 16, "Animacao" }, { 35, "Comedia" },
    { 80, "Crime" }, { 99, "Documentario" }, { 18, "Drama" }, { 10751, "Familia" },
    { 14, "Fantasia" }, { 36, "Historia" }, { 27, "Terror" }, { 10402, "Musica" },
    { 9648, "Misterio" }, { 10749, "Romance" }, { 878, "Ficcao cientifica" },
    { 53, "Suspense" }, { 10752, "Guerra" }, { 37, "Faroeste" },
    { 10759, "Acao e Aventura" }, { 10762, "Infantil" }, { 10763, "Noticias" },
    { 10764, "Reality" }, { 10765, "Ficcao e Fantasia" }, { 10766, "Novela" },
    { 10767, "Talk show" }, { 10768, "Guerra e Politica" },
  };
  size_t i;
  if (id <= 0) return NULL;
  for (i = 0; i < sizeof G / sizeof *G; i++)
    if (G[i].id == id) return G[i].nome;
  return NULL;
}

// ------------------------------------------------------ escritor JSON ------

typedef struct {
  char  *buf;
  size_t n;
  size_t cap;
  int    erro;
} Esc;

static void escLit(Esc *e, const char *s) {
  size_t k;
  if (e->erro) return;
  k = strlen(s);
  if (e->n + k >= e->cap) { e->erro = 1; return; }
  memcpy(e->buf + e->n, s, k);
  e->n += k;
  e->buf[e->n] = '\0';
}

static void escChar(Esc *e, char ch) {
  if (e->erro) return;
  if (e->n + 1 >= e->cap) { e->erro = 1; return; }
  e->buf[e->n++] = ch;
  e->buf[e->n] = '\0';
}

static void escStr(Esc *e, const char *s) {
  const unsigned char *p = (const unsigned char *)(s ? s : "");
  escChar(e, '"');
  for (; *p && !e->erro; p++) {
    unsigned char ch = *p;
    if (ch == '"' || ch == '\\') {
      escChar(e, '\\');
      escChar(e, (char)ch);
    } else if (ch == '\b') { escLit(e, "\\b"); }
    else if (ch == '\f')   { escLit(e, "\\f"); }
    else if (ch == '\n')   { escLit(e, "\\n"); }
    else if (ch == '\r')   { escLit(e, "\\r"); }
    else if (ch == '\t')   { escLit(e, "\\t"); }
    else if (ch < 0x20 || ch == 0x7f) {
      char u[7];
      snprintf(u, sizeof u, "\\u%04x", (unsigned)ch);
      escLit(e, u);
    } else {
      escChar(e, (char)ch);
    }
  }
  escChar(e, '"');
}

static void escInt(Esc *e, long long v) {
  char b[32];
  snprintf(b, sizeof b, "%lld", v);
  escLit(e, b);
}

static void escPesoQ8(Esc *e, int16_t peso) {
  char b[24];
  long x = ((long)peso * 100 + (peso >= 0 ? 128 : -128)) / 256;
  long absx = x < 0 ? -x : x;
  snprintf(b, sizeof b, "%s%ld.%02ld", x < 0 ? "-" : "", absx / 100, absx % 100);
  escLit(e, b);
}

// ------------------------------------------------ extracao pura do gosto ---

typedef struct {
  int32_t id;
  int16_t peso;
} AfinTemp;

static int cmpAfinDesc(const void *a, const void *b) {
  const AfinTemp *pa = (const AfinTemp *)a;
  const AfinTemp *pb = (const AfinTemp *)b;
  if (pb->peso != pa->peso) return pb->peso > pa->peso ? 1 : -1;
  return pa->id < pb->id ? -1 : (pa->id > pb->id ? 1 : 0);
}

static int extrairDim(const FarolGosto *g, int dim, const FarolAfin *src, int cap,
                      uint16_t dia, int maxOut,
                      const char *(*nomeFn)(int32_t, void *), void *u,
                      int ehGenero, FarolComporAfin *out) {
  AfinTemp tmp[FG_TEMA_N];
  int nt = 0, i, k = 0;
  if (!g || !src || !out || maxOut <= 0) return 0;
  for (i = 0; i < cap && nt < FG_TEMA_N; i++) {
    int32_t w;
    if (src[i].id <= 0) continue; // SOMENTE ids publicos TMDB (> 0)
    w = farolgosto_peso(g, dim, src[i].id, dia);
    if (w <= 0) continue;
    tmp[nt].id = src[i].id;
    tmp[nt].peso = (int16_t)w;
    nt++;
  }
  if (nt > 1) qsort(tmp, (size_t)nt, sizeof tmp[0], cmpAfinDesc);
  for (i = 0; i < nt && k < maxOut; i++) {
    int dup = 0, j;
    const char *nm = NULL;
    for (j = 0; j < k; j++) if (out[j].id == tmp[i].id) { dup = 1; break; }
    if (dup) continue;
    memset(&out[k], 0, sizeof out[k]);
    out[k].id = tmp[i].id;
    out[k].peso = tmp[i].peso;
    if (nomeFn) nm = nomeFn(tmp[i].id, u);
    if ((!nm || !nm[0]) && ehGenero) nm = generoPublicoNome(tmp[i].id);
    if (nm && nm[0]) {
      textoSeguroLimpar(nm, out[k].nome, sizeof out[k].nome, 63, 0);
    }
    k++;
  }
  return k;
}

int farolcompor_extrair_gosto(const FarolGosto *g, uint16_t dia,
                              const char *(*nomePessoaFn)(int32_t, void *),
                              const char *(*nomeTemaFn)(int32_t, void *),
                              void *u,
                              FarolComporAfin *genOut, int *nGen,
                              FarolComporAfin *temaOut, int *nTema,
                              FarolComporAfin *pesOut, int *nPes) {
  int ng = 0, nt = 0, np = 0;
  if (nGen) *nGen = 0;
  if (nTema) *nTema = 0;
  if (nPes) *nPes = 0;
  if (!g) return 0;
  if (genOut) {
    ng = extrairDim(g, FG_DIM_GENERO, g->genero, FG_GENERO_N, dia,
                    FAROL_COMPOR_GEN_MAX, NULL, NULL, 1, genOut);
    if (nGen) *nGen = ng;
  }
  if (temaOut) {
    nt = extrairDim(g, FG_DIM_TEMA, g->tema, FG_TEMA_N, dia,
                    FAROL_COMPOR_TEMA_MAX, nomeTemaFn, u, 0, temaOut);
    if (nTema) *nTema = nt;
  }
  if (pesOut) {
    np = extrairDim(g, FG_DIM_PESSOA, g->pessoa, FG_PESSOA_N, dia,
                    FAROL_COMPOR_PESSOA_MAX, nomePessoaFn, u, 0, pesOut);
    if (nPes) *nPes = np;
  }
  return ng + nt + np;
}

// ----------------------------------------- serializacao estrita do pedido ---

static void escListaAfin(Esc *e, const char *chave, const FarolComporAfin *arr,
                         int n, int maxItens, int ehGenero) {
  int escritos = 0, i, j;
  int32_t usados[FAROL_COMPOR_GEN_MAX];
  char camBase[48], camId[56], camNome[56], camPeso[56];
  int okNome, okPeso;
  if (!arr || n <= 0 || e->erro) return;
  snprintf(camBase, sizeof camBase, "gosto.%s", chave);
  snprintf(camId, sizeof camId, "gosto.%s[].id", chave);
  if (!farolconsent_campo_autorizado(camBase) ||
      !farolconsent_campo_autorizado(camId))
    return;
  snprintf(camNome, sizeof camNome, "gosto.%s[].nome", chave);
  snprintf(camPeso, sizeof camPeso, "gosto.%s[].peso", chave);
  okNome = farolconsent_campo_autorizado(camNome);
  okPeso = farolconsent_campo_autorizado(camPeso);
  if (maxItens > FAROL_COMPOR_GEN_MAX) maxItens = FAROL_COMPOR_GEN_MAX;
  for (i = 0; i < n && escritos < maxItens; i++) {
    int32_t id = arr[i].id;
    int dup = 0;
    char nome[64];
    if (id <= 0) continue; // apenas ID publico TMDB > 0
    for (j = 0; j < escritos; j++) if (usados[j] == id) { dup = 1; break; }
    if (dup) continue;
    nome[0] = '\0';
    if (arr[i].nome[0]) textoSeguroLimpar(arr[i].nome, nome, sizeof nome, 63, 0);
    else if (ehGenero) {
      const char *gn = generoPublicoNome(id);
      if (gn) textoSeguroLimpar(gn, nome, sizeof nome, 63, 0);
    }
    escLit(e, escritos == 0 ? ",\"" : ",");
    if (escritos == 0) { escLit(e, chave); escLit(e, "\":["); }
    escLit(e, "{\"id\":");
    escInt(e, id);
    if (nome[0] && okNome) {
      escLit(e, ",\"nome\":");
      escStr(e, nome);
    }
    if (arr[i].peso != 0 && okPeso) {
      escLit(e, ",\"peso\":");
      escPesoQ8(e, arr[i].peso);
    }
    escLit(e, "}");
    usados[escritos++] = id;
  }
  if (escritos > 0) escLit(e, "]");
}

static int motivoTipoNome(uint8_t tipo, const char **nomeOut) {
  switch (tipo) {
    case FMO_TERMINOU:    *nomeOut = "FMO_TERMINOU"; return 1;
    case FMO_GENERO:      *nomeOut = "FMO_GENERO"; return 1;
    case FMO_PESSOA:      *nomeOut = "FMO_PESSOA"; return 1;
    case FMO_TEMA:        *nomeOut = "FMO_TEMA"; return 1;
    case FMO_AMIGO:       *nomeOut = "FMO_AMIGO"; return 1;
    case FMO_SALVO:       *nomeOut = "FMO_SALVO"; return 1;
    case FMO_CABE_HOJE:   *nomeOut = "FMO_CABE_HOJE"; return 1;
    case FMO_REVER:       *nomeOut = "FMO_REVER"; return 1;
    case FMO_FORA_CURVA:  *nomeOut = "FMO_FORA_CURVA"; return 1;
    case FMO_RECOMENDADO: *nomeOut = "FMO_RECOMENDADO"; return 1;
    default:              return 0;
  }
}

// Ancoras ELEGIVEIS do pedido (<= 3, id IMDb valido, titulo publico seguro,
// sem duplicata). `deEntrada[i]` leva o indice de ped->ancoras[i] (o `a` de
// FMO_TERMINOU) a posicao elegivel, ou -1. O corpo so publica a ancora que
// algum motivo FMO_TERMINOU de candidato EFETIVAMENTE enviado referencia.
typedef struct {
  int  n;
  char id[FAROL_COMPOR_ANCORA_MAX][20];
  char titulo[FAROL_COMPOR_ANCORA_MAX][104];
  int  deEntrada[FAROL_COMPOR_CAND_MAX];
  int  nEntrada;
} AncorasElegiveis;

static void ancorasElegiveis(const FarolComporPedido *ped, AncorasElegiveis *ae) {
  int i, k;
  memset(ae, 0, sizeof *ae);
  if (!ped->ancoras || ped->nAncoras <= 0) return;
  ae->nEntrada = ped->nAncoras < FAROL_COMPOR_CAND_MAX ? ped->nAncoras : FAROL_COMPOR_CAND_MAX;
  for (i = 0; i < ae->nEntrada; i++) {
    ae->deEntrada[i] = -1;
    if (!imdbValido(ped->ancoras[i].id)) continue;
    for (k = 0; k < ae->n; k++)
      if (!strcmp(ae->id[k], ped->ancoras[i].id)) break;
    if (k < ae->n) { ae->deEntrada[i] = k; continue; }
    if (ae->n >= FAROL_COMPOR_ANCORA_MAX) continue;
    if (!textoSeguroLimpar(ped->ancoras[i].titulo, ae->titulo[ae->n],
                           sizeof ae->titulo[ae->n], 96, 0))
      continue;
    snprintf(ae->id[ae->n], sizeof ae->id[ae->n], "%s", ped->ancoras[i].id);
    ae->deEntrada[i] = ae->n++;
  }
}

static int ancoraDoMotivo(const AncorasElegiveis *ae, int32_t a) {
  if (!ae || a < 0 || a >= ae->nEntrada) return -1;
  return ae->deEntrada[a];
}

// `usadas` (opcional) recebe o bit de cada ancora elegivel referenciada.
static void escCandidato(Esc *e, int idx, const FarolComporCand *c,
                         const AncorasElegiveis *ae, unsigned *usadas) {
  char idPub[32], titulo[124];
  int hasGen = 0, hasMot = 0, i;
  const char *cStr;
  char cTag[16];
  if (e->erro || !c) return;

  if (imdbValido(c->imdb)) snprintf(idPub, sizeof idPub, "%s", c->imdb);
  else if (c->tmdb > 0)    snprintf(idPub, sizeof idPub, "tmdb:%d", c->tmdb);
  else idPub[0] = '\0';

  if (!idPub[0]) return;
  if (!textoSeguroLimpar(c->titulo, titulo, sizeof titulo, 118, 0)) return;
  if (!farolconsent_campo_autorizado("candidatos[].c") ||
      !farolconsent_campo_autorizado("candidatos[].id") ||
      !farolconsent_campo_autorizado("candidatos[].tipo") ||
      !farolconsent_campo_autorizado("candidatos[].titulo"))
    return;

  snprintf(cTag, sizeof cTag, "c%d", idx);
  cStr = cTag;

  escLit(e, "{\"c\":");
  escStr(e, cStr);
  escLit(e, ",\"id\":");
  escStr(e, idPub);
  escLit(e, ",\"tipo\":");
  escStr(e, c->tipo ? "series" : "movie");
  escLit(e, ",\"titulo\":");
  escStr(e, titulo);
  if (c->ano >= 1880 && c->ano <= 2100 && farolconsent_campo_autorizado("candidatos[].ano")) {
    escLit(e, ",\"ano\":");
    escInt(e, c->ano);
  }
  if (farolconsent_campo_autorizado("candidatos[].generos")) {
    for (i = 0; i < c->nGeneros && i < 8; i++) {
      if (c->generos[i] <= 0) continue; // apenas TMDB publico > 0
      if (!hasGen) { escLit(e, ",\"generos\":["); hasGen = 1; }
      else escLit(e, ",");
      escInt(e, c->generos[i]);
    }
    if (hasGen) escLit(e, "]");
  }

  if (c->minutos > 0 && c->minutos <= 2000 && farolconsent_campo_autorizado("candidatos[].minutos")) {
    escLit(e, ",\"minutos\":");
    escInt(e, c->minutos);
  }
  if (c->nota10 >= 0 && c->nota10 <= 100 && farolconsent_campo_autorizado("candidatos[].nota")) {
    char nb[16];
    snprintf(nb, sizeof nb, "%d.%d", c->nota10 / 10, c->nota10 % 10);
    escLit(e, ",\"nota\":");
    escLit(e, nb);
  }
  if (c->pontos != 0 && farolconsent_campo_autorizado("candidatos[].pontos")) {
    escLit(e, ",\"pontos\":");
    escInt(e, c->pontos);
  }

  if (farolconsent_campo_autorizado("candidatos[].motivos")) {
    for (i = 0; i < c->nMotivos && i < FAROL_COMPOR_MOT_MAX; i++) {
      const FarolComporMotivoItem *mo = &c->motivos[i];
      const char *tn = NULL;
      if (!motivoTipoNome(mo->tipo, &tn)) continue;
      if (mo->tipo == FMO_AMIGO || mo->tipo == FMO_RECOMENDADO) {
        // REGRA ESTRITA: motivo de amigo SEMPRE anonimizado (zero quem / amigo).
        escLit(e, !hasMot ? ",\"motivos\":[" : ",");
        hasMot = 1;
        escLit(e, "{\"tipo\":");
        escStr(e, tn);
        escLit(e, ",\"a\":0,\"b\":0}");
      } else if (mo->tipo == FMO_TERMINOU) {
        // So referencia ancora elegivel (nunca semente/diario fora das <=3 ancoras).
        int k = ancoraDoMotivo(ae, mo->a);
        if (k < 0) continue;
        escLit(e, !hasMot ? ",\"motivos\":[" : ",");
        hasMot = 1;
        escLit(e, "{\"tipo\":\"FMO_TERMINOU\",\"id\":");
        escStr(e, ae->id[k]);
        escLit(e, "}");
        if (usadas) *usadas |= 1u << k;
      } else if (mo->tipo == FMO_GENERO || mo->tipo == FMO_FORA_CURVA ||
                 mo->tipo == FMO_PESSOA || mo->tipo == FMO_TEMA) {
        if (mo->a <= 0) continue; // apenas IDs publicos TMDB > 0
        escLit(e, !hasMot ? ",\"motivos\":[" : ",");
        hasMot = 1;
        escLit(e, "{\"tipo\":");
        escStr(e, tn);
        escLit(e, ",\"a\":");
        escInt(e, mo->a);
        escLit(e, ",\"b\":0}");
      } else if (mo->tipo == FMO_CABE_HOJE) {
        int min = mo->a > 0 ? (int)mo->a : (int)c->minutos;
        escLit(e, !hasMot ? ",\"motivos\":[" : ",");
        hasMot = 1;
        escLit(e, "{\"tipo\":\"FMO_CABE_HOJE\",\"a\":");
        escInt(e, min > 0 ? min : 0);
        escLit(e, ",\"b\":0}");
      } else if (mo->tipo == FMO_SALVO || mo->tipo == FMO_REVER) {
        // Nunca enviar datas de salvo/rever.
        escLit(e, !hasMot ? ",\"motivos\":[" : ",");
        hasMot = 1;
        escLit(e, "{\"tipo\":");
        escStr(e, tn);
        escLit(e, ",\"a\":0,\"b\":0}");
      }
    }
    if (hasMot) escLit(e, "]");
  }

  if ((c->faltam > 0 || c->minNoite > 0 || c->cabeHoje != 0) &&
      farolconsent_campo_autorizado("candidatos[].eixos")) {
    int first = 1;
    escLit(e, ",\"eixos\":{");
    if (c->faltam > 0 && farolconsent_campo_autorizado("candidatos[].eixos.faltam")) {
      escLit(e, "\"faltam\":");
      escInt(e, c->faltam);
      first = 0;
    }
    if (c->minNoite > 0 && farolconsent_campo_autorizado("candidatos[].eixos.min_noite")) {
      if (!first) escLit(e, ",");
      escLit(e, "\"min_noite\":");
      escInt(e, c->minNoite);
      first = 0;
    }
    if (c->cabeHoje != 0 && farolconsent_campo_autorizado("candidatos[].eixos.cabe_hoje")) {
      if (!first) escLit(e, ",");
      escLit(e, "\"cabe_hoje\":");
      escInt(e, c->cabeHoje > 0 ? 1 : 0);
    }
    escLit(e, "}");
  }

  escLit(e, "}");
}

static int fatoChaveValida(const char *k) {
  size_t i;
  if (!k || !k[0] || strlen(k) > 32) return 0;
  for (i = 0; k[i]; i++) {
    unsigned char c = (unsigned char)k[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_'))
      return 0;
  }
  // Nunca permitir chaves suspeitas de vazamento privado em fatos.
  if (!strcmp(k, "perfil") || !strcmp(k, "profile") || !strcmp(k, "usuario") ||
      !strcmp(k, "diario") || !strcmp(k, "amigos") || !strcmp(k, "addons") ||
      !strcmp(k, "fontes") || !strcmp(k, "token") || !strcmp(k, "data"))
    return 0;
  return 1;
}

// Uma passada de escrita. `forcados` != NULL: escreve EXATAMENTE esses
// candidatos de entrada (na ordem, como c0..k-1) e publica so as ancoras do
// bit `ancMask`; NULL: seleciona sob o teto de 12 KB, publicando todas as
// ancoras elegiveis (cota superior do cabecalho). `mapa[k]` recebe o indice
// de entrada do candidato escrito como "c<k>"; `usadas`, as ancoras que os
// motivos dos candidatos ESCRITOS referenciam.
static size_t montarCorpoPassada(const FarolComporPedido *ped, const AncorasElegiveis *ae,
                                 unsigned ancMask, const int *forcados, int nForcados,
                                 char *dst, size_t cap, int *mapa, int *nMapa,
                                 unsigned *usadas) {
  const char *modoStr, *idiStr;
  char fraseNorm[FAROL_COMPOR_FRASE_MAX + 4];
  int hora, fimSem = 0, nivel, i, k, candEscritos = 0, fatosEscritos = 0, nAncEsc = 0;
  size_t lim;
  Esc e;

  *nMapa = 0;
  *usadas = 0;
  modoStr = modoNome(ped->modo);
  idiStr = (ped->idioma && idiomaValido(ped->idioma)) ? ped->idioma : "pt-BR";
  if (ped->hora >= 0 && ped->hora <= 7) {
    hora = ped->hora;
    if (ped->fimSemana >= 0) fimSem = ped->fimSemana ? 1 : 0;
    else (void)faixaLocalDe(agoraS(), &fimSem);
  } else {
    long long ag = agoraS();
    int fs = 0;
    hora = faixaLocalDe(ag, &fs);
    fimSem = (ped->fimSemana >= 0) ? (ped->fimSemana ? 1 : 0) : fs;
  }
  nivel = ped->nivel < 0 ? 0 : (ped->nivel > 2 ? 2 : ped->nivel);

  fraseNorm[0] = '\0';
  if (ped->frase && ped->frase[0]) {
    textoSeguroLimpar(ped->frase, fraseNorm, sizeof fraseNorm, FAROL_COMPOR_FRASE_MAX, 1);
  }

  lim = cap < (size_t)(FAROL_COMPOR_CORPO_MAX + 1) ? cap : (size_t)(FAROL_COMPOR_CORPO_MAX + 1);
  e.buf = dst;
  e.n = 0;
  e.cap = lim;
  e.erro = 0;
  dst[0] = '\0';

  if (!farolconsent_campo_autorizado("v") ||
      !farolconsent_campo_autorizado("modo") ||
      !farolconsent_campo_autorizado("idioma") ||
      !farolconsent_campo_autorizado("hora") ||
      !farolconsent_campo_autorizado("infantil") ||
      !farolconsent_campo_autorizado("consentiu") ||
      !farolconsent_campo_autorizado("gosto") ||
      !farolconsent_campo_autorizado("gosto.nivel") ||
      !farolconsent_campo_autorizado("gosto.fim_semana") ||
      !farolconsent_campo_autorizado("candidatos")) {
    return 0;
  }

  escLit(&e, "{\"v\":1,\"modo\":");
  escStr(&e, modoStr);
  escLit(&e, ",\"idioma\":");
  escStr(&e, idiStr);
  escLit(&e, ",\"hora\":");
  escInt(&e, hora);
  escLit(&e, ",\"infantil\":0,\"consentiu\":1");

  // --- retomada (issue 57): SO no modo retomada, SO o episodio ja visto ---
  // Sem nenhum dos tres campos, o modo retomada segue o pedido de cena da #52.
  if (ped->modo == FC_MODO_RETOMADA &&
      (ped->retTemporada || ped->retEpisodio || ped->retSinopse)) {
    char sin[FAROL_COMPOR_SINOPSE_MAX + 4];
    if (ped->retTemporada < 1 || ped->retTemporada > 999 ||
        ped->retEpisodio < 1 || ped->retEpisodio > 9999 ||
        !textoSeguroLimpar(ped->retSinopse, sin, sizeof sin,
                           FAROL_COMPOR_SINOPSE_MAX, 1)) {
      dst[0] = '\0';
      return 0;
    }
    escLit(&e, ",\"retomada\":{\"temporada\":");
    escInt(&e, ped->retTemporada);
    escLit(&e, ",\"episodio\":");
    escInt(&e, ped->retEpisodio);
    escLit(&e, ",\"sinopse\":");
    escStr(&e, sin);
    escLit(&e, "}");
    memset(sin, 0, sizeof sin);
  }

  // --- gosto ---
  escLit(&e, ",\"gosto\":{\"nivel\":");
  escInt(&e, nivel);
  escLit(&e, ",\"fim_semana\":");
  escInt(&e, fimSem ? 1 : 0);

  escListaAfin(&e, "generos", ped->generos, ped->nGeneros, FAROL_COMPOR_GEN_MAX, 1);
  escListaAfin(&e, "temas", ped->temas, ped->nTemas, FAROL_COMPOR_TEMA_MAX, 0);
  escListaAfin(&e, "pessoas", ped->pessoas, ped->nPessoas, FAROL_COMPOR_PESSOA_MAX, 0);

  // <= 3 ancoras publicas (na passada final: SO as referenciadas por motivo enviado)
  if (farolconsent_campo_autorizado("gosto.ancoras") &&
      farolconsent_campo_autorizado("gosto.ancoras[].id") &&
      farolconsent_campo_autorizado("gosto.ancoras[].titulo")) {
    for (k = 0; k < ae->n; k++) {
      if (!(ancMask & (1u << k))) continue;
      escLit(&e, nAncEsc == 0 ? ",\"ancoras\":[" : ",");
      escLit(&e, "{\"id\":");
      escStr(&e, ae->id[k]);
      escLit(&e, ",\"titulo\":");
      escStr(&e, ae->titulo[k]);
      escLit(&e, "}");
      nAncEsc++;
    }
    if (nAncEsc > 0) escLit(&e, "]");
  }
  escLit(&e, "}");
  if (e.erro) { dst[0] = '\0'; return 0; }

  // --- candidatos (<= 24, cN compactado 0..k-1 e teto de 12 KB) ---
  escLit(&e, ",\"candidatos\":[");
  if (ped->candidatos && ped->nCandidatos > 0) {
    // Reserva margem para fatos/frase + fechamento ("]," + "fatos":[...] + "}").
    size_t reservaCauda = 256 + (fraseNorm[0] ? strlen(fraseNorm) * 2 + 32 : 0);
    int nIt = forcados ? nForcados : ped->nCandidatos;
    for (k = 0; k < nIt && candEscritos < FAROL_COMPOR_CAND_MAX; k++) {
      int iEnt = forcados ? forcados[k] : k;
      size_t marcaN = e.n;
      unsigned usadasCand = 0;
      Esc probe;
      char candBuf[1024];
      if (iEnt < 0 || iEnt >= ped->nCandidatos) { dst[0] = '\0'; return 0; }
      probe.buf = candBuf;
      probe.n = 0;
      probe.cap = sizeof candBuf;
      probe.erro = 0;
      candBuf[0] = '\0';
      escCandidato(&probe, candEscritos, &ped->candidatos[iEnt], ae, &usadasCand);
      if (probe.erro || probe.n == 0) {
        if (forcados) { dst[0] = '\0'; return 0; }
        continue;
      }
      if (e.n + probe.n + (candEscritos > 0 ? 1 : 0) + reservaCauda >= e.cap) {
        if (forcados) { dst[0] = '\0'; return 0; }
        break; // para no candidato anterior antes de passar dos 12 KB
      }
      if (candEscritos > 0) escLit(&e, ",");
      escLit(&e, candBuf);
      if (e.erro) {
        if (forcados) { dst[0] = '\0'; return 0; }
        e.n = marcaN; dst[e.n] = '\0'; e.erro = 0; break;
      }
      mapa[candEscritos] = iEnt;
      *usadas |= usadasCand;
      candEscritos++;
    }
  }
  escLit(&e, "]");

  // --- fatos (<= 8, inclui frase ditada opcional quando aplicavel) ---
  if (((ped->fatos && ped->nFatos > 0) || fraseNorm[0]) &&
      farolconsent_campo_autorizado("fatos")) {
    int okAlvo = farolconsent_campo_autorizado("fatos[].alvo");
    for (i = 0; ped->fatos && i < ped->nFatos && fatosEscritos < FAROL_COMPOR_FATOS_MAX; i++) {
      const FarolComporFato *ft = &ped->fatos[i];
      char val[164], camFato[64];
      size_t marcaN = e.n;
      if (!fatoChaveValida(ft->chave)) continue;
      snprintf(camFato, sizeof camFato, "fatos[].%s", ft->chave);
      if (!farolconsent_campo_autorizado(camFato)) continue;
      val[0] = '\0';
      if (ft->valor[0] && !textoSeguroLimpar(ft->valor, val, sizeof val, 150, 0))
        continue;
      if (!val[0] && !ft->temInteiro) continue;
      escLit(&e, fatosEscritos == 0 ? ",\"fatos\":[" : ",");
      escLit(&e, "{");
      if (okAlvo && ft->alvo[0] && (imdbValido(ft->alvo) ||
          (ft->alvo[0] == 'c' && isdigit((unsigned char)ft->alvo[1])))) {
        escLit(&e, "\"alvo\":");
        escStr(&e, ft->alvo);
        escLit(&e, ",");
      }
      escLit(&e, "\"");
      escLit(&e, ft->chave);
      escLit(&e, "\":");
      if (ft->temInteiro && !val[0]) escInt(&e, ft->inteiro);
      else escStr(&e, val);
      escLit(&e, "}");
      if (e.erro) {
        e.erro = 0;
        e.n = marcaN;
        dst[e.n] = '\0';
        break;
      }
      fatosEscritos++;
    }
    if (fraseNorm[0] && fatosEscritos < FAROL_COMPOR_FATOS_MAX &&
        farolconsent_campo_autorizado("fatos[].frase")) {
      size_t marcaN = e.n;
      escLit(&e, fatosEscritos == 0 ? ",\"fatos\":[" : ",");
      escLit(&e, "{\"frase\":");
      escStr(&e, fraseNorm);
      escLit(&e, "}");
      if (e.erro) {
        e.erro = 0;
        e.n = marcaN;
        dst[e.n] = '\0';
      } else {
        fatosEscritos++;
      }
    }
    if (fatosEscritos > 0) escLit(&e, "]");
  }

  escLit(&e, "}");
  if (e.erro || e.n > FAROL_COMPOR_CORPO_MAX) {
    dst[0] = '\0';
    return 0;
  }
  *nMapa = candEscritos;
  return e.n;
}

size_t farolcompor_montar_corpo_mapa(const FarolComporPedido *ped, char *dst, size_t cap,
                                     int *mapaOut, int *nMapaOut) {
  AncorasElegiveis ae;
  int mapa[FAROL_COMPOR_CAND_MAX], mapa2[FAROL_COMPOR_CAND_MAX];
  int nMapa = 0, nMapa2 = 0, k;
  unsigned usadas = 0, usadas2 = 0;
  size_t n;

  if (nMapaOut) *nMapaOut = 0;
  if (dst && cap) dst[0] = '\0';
  if (!ped || !dst || cap < 64) return 0;
  // Requisito de privacidade: suprimir pedido quando infantil ou sem consentimento.
  if (ped->infantil || !ped->consentiu) return 0;
  if (!modoNome(ped->modo)) return 0;

  ancorasElegiveis(ped, &ae);
  // 1a passada: escolhe os candidatos sob o teto com o cabecalho MAIOR
  // possivel (todas as ancoras elegiveis) e descobre quais ancoras os
  // motivos dos candidatos escolhidos citam.
  n = montarCorpoPassada(ped, &ae, (1u << ae.n) - 1u, NULL, 0, dst, cap,
                         mapa, &nMapa, &usadas);
  if (n == 0) return 0;
  // 2a passada (final): o MESMO conjunto, na mesma ordem, e so as ancoras
  // referenciadas. O cabecalho so encolhe, entao todo candidato cabe de novo.
  n = montarCorpoPassada(ped, &ae, usadas, mapa, nMapa, dst, cap,
                         mapa2, &nMapa2, &usadas2);
  if (n == 0 || nMapa2 != nMapa || usadas2 != usadas) { dst[0] = '\0'; return 0; }
  for (k = 0; k < nMapa; k++) {
    if (mapa2[k] != mapa[k]) { dst[0] = '\0'; return 0; }
    if (mapaOut) mapaOut[k] = mapa[k];
  }
  if (nMapaOut) *nMapaOut = nMapa;
  return n;
}

size_t farolcompor_montar_corpo(const FarolComporPedido *ped, char *dst, size_t cap) {
  return farolcompor_montar_corpo_mapa(ped, dst, cap, NULL, NULL);
}

// ------------------------------------------------- validacao da resposta ----

// Detecta resposta explicita de recuo {"origem":"regras"} sem erros falsos.
static int ehJsonRegrasFallback(const char *json, size_t len) {
  const char *p = json, *fim = json + len;
  int viuOrigem = 0;
  if (!json || len == 0 || len > 256) return 0;
  while (p < fim && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
  if (p >= fim || *p != '{') return 0;
  p++;
  for (;;) {
    char chave[32], val[32];
    size_t k = 0;
    while (p < fim && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (p < fim && *p == '}') { p++; break; }
    if (viuOrigem) return 0;
    if (p >= fim || *p != '"') return 0;
    p++;
    while (p < fim && *p != '"' && k + 1 < sizeof chave) {
      if ((unsigned char)*p < 0x20 || *p == '\\') return 0;
      chave[k++] = *p++;
    }
    chave[k] = '\0';
    if (p >= fim || *p != '"') return 0;
    p++;
    while (p < fim && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (p >= fim || *p != ':') return 0;
    p++;
    while (p < fim && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (p >= fim || *p != '"') return 0;
    p++;
    k = 0;
    while (p < fim && *p != '"' && k + 1 < sizeof val) {
      if ((unsigned char)*p < 0x20 || *p == '\\') return 0;
      val[k++] = *p++;
    }
    val[k] = '\0';
    if (p >= fim || *p != '"') return 0;
    p++;
    if (strcmp(chave, "origem") != 0 || strcmp(val, "regras") != 0) return 0;
    viuOrigem = 1;
  }
  while (p < fim && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
  return viuOrigem && p == fim;
}

static int refCandValida(const FarolRef *r, const FarolComporCand *cand, int nCand) {
  if (!r) return 0;
  if (r->c >= 0) {
    if (!cand || r->c >= nCand) return 0;
    return imdbValido(cand[r->c].imdb) || cand[r->c].tmdb > 0;
  }
  if (imdbValido(r->s)) return 1;
  return 0;
}

static int textoArgsResolvem(const FarolTexto *t, const FarolComporCand *cand, int nCand) {
  int i;
  if (!t) return 0;
  if (t->usaX) return 1;
  if (!t->m[0]) return 0;
  for (i = 0; i < t->nArgs && i < FAROL_ARGS; i++) {
    const char *a = t->a[i];
    if (a[0] == 'c' && isdigit((unsigned char)a[1])) {
      int idx = atoi(a + 1);
      if (!cand || idx < 0 || idx >= nCand) return 0;
    }
  }
  return 1;
}

int farolcompor_validar_resposta(FarolComporModo modo, const char *json, size_t len,
                                 const FarolComporCand *cand, int nCand,
                                 FarolCena *cenaOut, int *ehRegrasOut) {
  FarolCena tmp;
  int rc, i, j, temEsperada = 0;
  const char *telaEsp;

  if (ehRegrasOut) *ehRegrasOut = 0;
  if (cenaOut) memset(cenaOut, 0, sizeof *cenaOut);
  if (!json || len == 0 || len > FAROL_CENA_MAX) return -1;
  if (strlen(json) != len) return -1;
  if (!utf8Valido(json)) return -1;

  if (ehJsonRegrasFallback(json, len)) {
    if (ehRegrasOut) *ehRegrasOut = 1;
    return 0;
  }

  telaEsp = telaEsperadaModo(modo);
  if (!telaEsp || !telaEsp[0]) return -1;

  rc = farolpeca_ler(&tmp, json, nCand);
  if (rc <= 0 || tmp.nPecas <= 0) return -1;
  if (strcmp(tmp.tela, telaEsp) != 0) return -1;

  // Qualquer peca recusada por FPE_REF (ex: "c99" fora da lista) invalida a resposta.
  if (tmp.nRecusadas > 0) {
    for (i = 0; i < FAROL_PECA_MAX; i++) {
      if (tmp.erro[i] == FPE_REF) return -1;
    }
  }

  for (i = 0; i < tmp.nPecas; i++) {
    const FarolPeca *pc = &tmp.pecas[i];
    switch (pc->tipo) {
      case FP_FILEIRA:
        if (strcmp(pc->u.fileira.id, "gosto1") != 0 &&
            strcmp(pc->u.fileira.id, "gosto2") != 0)
          return -1;
        if (pc->u.fileira.nItens < 6) return -1;
        for (j = 0; j < pc->u.fileira.nItens; j++) {
          if (!refCandValida(&pc->u.fileira.itens[j], cand, nCand)) return -1;
        }
        if (!textoArgsResolvem(&pc->u.fileira.titulo, cand, nCand)) return -1;
        if (modo == FC_MODO_HOME) temEsperada = 1;
        break;
      case FP_DICA:
        if (pc->u.dica.temAlvo &&
            !refCandValida(&pc->u.dica.alvo, cand, nCand))
          return -1;
        if (modo == FC_MODO_DICA) temEsperada = 1;
        break;
      case FP_PORQUE:
        if (!refCandValida(&pc->u.porque.alvo, cand, nCand)) return -1;
        if (modo == FC_MODO_RETOMADA) temEsperada = 1;
        break;
      case FP_INFO:
        if (!refCandValida(&pc->u.info.alvo, cand, nCand)) return -1;
        if (modo == FC_MODO_RETOMADA) {
          for (j = 0; j < pc->u.info.nItens; j++) {
            const FarolInfoItem *it = &pc->u.info.itens[j];
            if (it->k == FP_INFO_RETOMADA && it->l.usaX &&
                strcmp(it->l.m, "FT_INFO_RETOMADA_SIMPLES") == 0 &&
                it->l.nArgs == 2) {
              const char *rx = farolpeca_x(&tmp, &it->l);
              int rt = atoi(it->l.a[0]), re = atoi(it->l.a[1]);
              if (!rx || !farolinfo_retomada_linha_ok(rx) ||
                  !retomadaResumoSemSpoiler(rx, rt, re))
                return -1;
            }
          }
          temEsperada = 1;
        }
        break;
      case FP_COMPARAR:
        if (pc->u.comparar.nItens < 2) return -1;
        for (j = 0; j < pc->u.comparar.nItens; j++) {
          if (!refCandValida(&pc->u.comparar.itens[j], cand, nCand)) return -1;
        }
        if (modo == FC_MODO_COMPARAR) temEsperada = 1;
        break;
      case FP_ACHADOS:
        if (pc->u.achados.nItens < 1) return -1;
        for (j = 0; j < pc->u.achados.nItens; j++) {
          if (!refCandValida(&pc->u.achados.itens[j], cand, nCand)) return -1;
        }
        break;
      case FP_ESCOLHA:
        if (pc->u.escolha.nOpcoes < 2) return -1;
        break;
      default:
        return -1;
    }
  }

  if (!temEsperada) return -1;
  if (cenaOut) *cenaOut = tmp;
  return tmp.nPecas;
}

// ------------------------- persistencia e serializacao de farol-cena-pN.json

static void refParaString(const FarolRef *r, const FarolComporCand *cand, int nCand,
                          char *dst, size_t cap) {
  if (!dst || !cap) return;
  dst[0] = '\0';
  if (!r) return;
  if (r->c >= 0 && cand && r->c < nCand && imdbValido(cand[r->c].imdb)) {
    snprintf(dst, cap, "%s", cand[r->c].imdb);
    return;
  }
  snprintf(dst, cap, "%s", r->s);
}

static void escTextoCena(Esc *e, const FarolCena *c, const FarolTexto *t,
                         const FarolComporCand *cand, int nCand) {
  int first = 1, i;
  escLit(e, "{");
  if (t->m[0]) {
    escLit(e, "\"m\":");
    escStr(e, t->m);
    first = 0;
    if (t->nArgs > 0) {
      escLit(e, ",\"a\":[");
      for (i = 0; i < t->nArgs && i < FAROL_ARGS; i++) {
        const char *a = t->a[i];
        if (i > 0) escLit(e, ",");
        if (a[0] == 'c' && isdigit((unsigned char)a[1])) {
          int idx = atoi(a + 1);
          if (cand && idx >= 0 && idx < nCand && imdbValido(cand[idx].imdb)) {
            escStr(e, cand[idx].imdb);
            continue;
          }
        }
        escStr(e, a);
      }
      escLit(e, "]");
    }
  }
  if (t->usaX) {
    const char *x = farolpeca_x(c, t);
    if (x && x[0]) {
      if (!first) escLit(e, ",");
      escLit(e, "\"x\":");
      escStr(e, x);
    }
  }
  escLit(e, "}");
}

static const char *formaNome(uint8_t forma) {
  switch (forma) {
    case FP_FORMA_CARTAZ: return "cartaz";
    case FP_FORMA_LARGA:  return "larga";
    case FP_FORMA_FICHA:  return "ficha";
    case FP_FORMA_PALCO:  return "palco";
    default:              return "cartaz";
  }
}

static const char *acaoNome(uint8_t acao) {
  switch (acao) {
    case FP_ACAO_ABRIR:    return "abrir";
    case FP_ACAO_TOCAR:    return "tocar";
    case FP_ACAO_COMPARAR: return "comparar";
    case FP_ACAO_ESCOLHA:  return "escolha";
    case FP_ACAO_SALVAR:   return "salvar";
    case FP_ACAO_NADA:     return "nada";
    default:               return "nada";
  }
}

// Serializa cena Home validada para cache em disco (`farol-cena-p<n>.json`),
// resolvendo "c<n>" -> "tt..." para manter validade apos reinicio.
static int serializarCenaHomeCache(const FarolCena *c, const FarolComporCand *cand,
                                   int nCand, int perfil, long long feita,
                                   char *dst, size_t cap) {
  Esc e;
  int p, escritos = 0, i;
  FarolCena check;
  if (!c || !dst || cap < 128) return 0;
  e.buf = dst;
  e.n = 0;
  e.cap = cap < (size_t)(FAROL_CENA_MAX + 1) ? cap : (size_t)(FAROL_CENA_MAX + 1);
  e.erro = 0;
  dst[0] = '\0';

  escLit(&e, "{\"v\":1,\"tela\":\"home\",\"perfil\":");
  escInt(&e, perfil);
  escLit(&e, ",\"feita\":");
  escInt(&e, feita);
  escLit(&e, ",\"vale_s\":");
  escInt(&e, FAROL_COMPOR_CACHE_TTL_S);
  escLit(&e, ",\"origem\":\"llm\",\"pecas\":[");

  for (p = 0; p < c->nPecas; p++) {
    const FarolPeca *pc = &c->pecas[p];
    if (pc->tipo == FP_FILEIRA) {
      char refs[FAROL_ITENS_MAX][32];
      int todosTt = 1;
      for (i = 0; i < pc->u.fileira.nItens && i < FAROL_ITENS_MAX; i++) {
        refParaString(&pc->u.fileira.itens[i], cand, nCand, refs[i], sizeof refs[i]);
        if (!imdbValido(refs[i])) { todosTt = 0; break; }
      }
      if (!todosTt || pc->u.fileira.nItens < 6) continue;
      if (escritos > 0) escLit(&e, ",");
      escLit(&e, "{\"t\":\"fileira\",\"id\":");
      escStr(&e, pc->u.fileira.id);
      escLit(&e, ",\"titulo\":");
      escTextoCena(&e, c, &pc->u.fileira.titulo, cand, nCand);
      if (farolpeca_texto_tem(&pc->u.fileira.sub)) {
        escLit(&e, ",\"sub\":");
        escTextoCena(&e, c, &pc->u.fileira.sub, cand, nCand);
      }
      escLit(&e, ",\"forma\":");
      escStr(&e, formaNome(pc->u.fileira.forma));
      escLit(&e, ",\"itens\":[");
      for (i = 0; i < pc->u.fileira.nItens; i++) {
        if (i > 0) escLit(&e, ",");
        escStr(&e, refs[i]);
      }
      escLit(&e, "]");
      if (pc->u.fileira.nMotivo > 0) {
        int nm = 0;
        for (i = 0; i < pc->u.fileira.nMotivo; i++) {
          int it = pc->u.fileira.motivo[i].item;
          if (it < 0 || it >= pc->u.fileira.nItens) continue;
          escLit(&e, nm == 0 ? ",\"motivo\":{" : ",");
          escStr(&e, refs[it]);
          escLit(&e, ":");
          escTextoCena(&e, c, &pc->u.fileira.motivo[i].t, cand, nCand);
          nm++;
        }
        if (nm > 0) escLit(&e, "}");
      }
      escLit(&e, "}");
      escritos++;
    } else if (pc->tipo == FP_DICA) {
      char alvo[32] = "";
      if (pc->u.dica.temAlvo) {
        refParaString(&pc->u.dica.alvo, cand, nCand, alvo, sizeof alvo);
        if (!imdbValido(alvo)) continue;
      }
      if (escritos > 0) escLit(&e, ",");
      escLit(&e, "{\"t\":\"dica\",\"id\":");
      escStr(&e, pc->u.dica.id);
      escLit(&e, ",\"l1\":");
      escTextoCena(&e, c, &pc->u.dica.l1, cand, nCand);
      if (farolpeca_texto_tem(&pc->u.dica.l2)) {
        escLit(&e, ",\"l2\":");
        escTextoCena(&e, c, &pc->u.dica.l2, cand, nCand);
      }
      escLit(&e, ",\"acao\":{\"tipo\":");
      escStr(&e, acaoNome(pc->u.dica.acao));
      if (alvo[0]) {
        escLit(&e, ",\"alvo\":");
        escStr(&e, alvo);
      }
      escLit(&e, "}");
      if (pc->u.dica.icone[0]) {
        escLit(&e, ",\"icone\":");
        escStr(&e, pc->u.dica.icone);
      }
      escLit(&e, "}");
      escritos++;
    }
  }
  escLit(&e, "]}");

  if (e.erro || escritos == 0 || e.n > FAROL_CENA_MAX) {
    dst[0] = '\0';
    return 0;
  }
  // Conferir antes de gravar: garante que o JSON em disco passa em farolpeca_ler sem candidatos.
  if (farolpeca_ler(&check, dst, 0) <= 0 || check.nRecusadas > 0) {
    dst[0] = '\0';
    return 0;
  }
  return 1;
}

static const char *pastaEfetiva(char *buf, size_t cap) {
  pthread_mutex_lock(&trava);
  if (pastaOverride[0]) {
    snprintf(buf, cap, "%s", pastaOverride);
    pthread_mutex_unlock(&trava);
    return buf;
  }
  pthread_mutex_unlock(&trava);
  return NULL;
}

static char *lojaLer(const char *nome) {
  char dir[512];
  if (pastaEfetiva(dir, sizeof dir)) {
    char p[640];
    FILE *f;
    long n;
    char *b;
    snprintf(p, sizeof p, "%s/%s", dir, nome);
    f = fopen(p, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    n = ftell(f);
    if (n < 0 || n > 65536L) { fclose(f); return NULL; }
    rewind(f);
    b = (char *)malloc((size_t)n + 1);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) {
      free(b);
      fclose(f);
      return NULL;
    }
    b[n] = '\0';
    fclose(f);
    return b;
  }
  return dados_ler(nome);
}

static int lojaGravar(const char *nome, const char *txt) {
  char dir[512];
  if (pastaEfetiva(dir, sizeof dir)) {
    char p[640], pt[660];
    FILE *f;
    size_t n = strlen(txt);
    int ok;
    snprintf(p, sizeof p, "%s/%s", dir, nome);
    snprintf(pt, sizeof pt, "%s/%s.tmp", dir, nome);
    f = fopen(pt, "wb");
    if (!f) return 0;
    ok = fwrite(txt, 1, n, f) == n;
    if (fclose(f) != 0 || !ok) { unlink(pt); return 0; }
    if (rename(pt, p) != 0) { unlink(pt); return 0; }
    return 1;
  }
  return dados_gravar_leve(nome, txt);
}

static void lojaApagar(const char *nome) {
  char dir[512];
  if (pastaEfetiva(dir, sizeof dir)) {
    char p[640];
    snprintf(p, sizeof p, "%s/%s", dir, nome);
    unlink(p);
    return;
  }
  (void)dados_apagar(nome);
}

static void nomeArquivoCena(int perfil, char *dst, size_t cap) {
  snprintf(dst, cap, "farol-cena-p%d.json", perfil);
}

static void nomeArquivoMeta(int perfil, char *dst, size_t cap) {
  snprintf(dst, cap, "farol-compor-p%d.txt", perfil);
}

typedef struct {
  int       valido;
  int       dia;
  long long feita;
  uint64_t  contaHash;
} MetaDia;

static void lerMetaDia(int perfil, uint64_t contaEsperada, MetaDia *out) {
  char nome[64], *txt;
  memset(out, 0, sizeof *out);
  nomeArquivoMeta(perfil, nome, sizeof nome);
  txt = lojaLer(nome);
  if (!txt) return;
  {
    int dia = 0;
    long long feita = 0;
    unsigned long long conta = 0;
    if (sscanf(txt, "FC 1\ndia=%d\nfeita=%lld\nconta=%llx", &dia, &feita, &conta) == 3) {
      if (conta == (unsigned long long)contaEsperada && dia > 0) {
        out->valido = 1;
        out->dia = dia;
        out->feita = feita;
        out->contaHash = (uint64_t)conta;
      }
    }
  }
  free(txt);
}

static void gravarMetaDia(int perfil, int diaLocal, long long feita, uint64_t contaHash) {
  char nome[64], buf[160];
  nomeArquivoMeta(perfil, nome, sizeof nome);
  snprintf(buf, sizeof buf, "FC 1\ndia=%d\nfeita=%lld\nconta=%016llx\n",
           diaLocal, feita, (unsigned long long)contaHash);
  lojaGravar(nome, buf);
}

static int carregarCacheHomeDisco(int perfil, uint64_t contaHash, long long agora,
                                  FarolCena *cenaOut) {
  char nome[64], *raw;
  MetaDia meta;
  FarolCena c;
  int rc, i, temFileira = 0;
  long long idade;

  nomeArquivoCena(perfil, nome, sizeof nome);
  raw = lojaLer(nome);
  if (!raw) return 0;

  if (strlen(raw) > FAROL_CENA_MAX || !utf8Valido(raw)) {
    free(raw);
    lojaApagar(nome);
    return 0;
  }

  lerMetaDia(perfil, contaHash, &meta);
  if (contaHash != 0 && (!meta.valido || meta.contaHash != contaHash)) {
    free(raw);
    return 0;
  }

  rc = farolpeca_ler(&c, raw, 0);
  free(raw);
  if (rc <= 0 || c.nRecusadas > 0 || strcmp(c.tela, "home") != 0) {
    lojaApagar(nome);
    return 0;
  }
  if (!(c.opt & FP_O_PERFIL) || c.perfil != perfil) return 0;
  if (!(c.opt & FP_O_FEITA) || !(c.opt & FP_O_VALE) || c.vale_s <= 0) {
    lojaApagar(nome);
    return 0;
  }
  idade = agora - c.feita;
  if (idade < 0 || idade >= c.vale_s || c.vale_s > FAROL_COMPOR_CACHE_TTL_S) {
    lojaApagar(nome);
    return 0;
  }
  for (i = 0; i < c.nPecas; i++) {
    const FarolPeca *pc = &c.pecas[i];
    if (pc->tipo == FP_FILEIRA) {
      int j;
      if (strcmp(pc->u.fileira.id, "gosto1") != 0 &&
          strcmp(pc->u.fileira.id, "gosto2") != 0)
        return 0;
      if (pc->u.fileira.nItens < 6) return 0;
      for (j = 0; j < pc->u.fileira.nItens; j++) {
        if (!imdbValido(pc->u.fileira.itens[j].s)) return 0;
      }
      temFileira = 1;
    }
  }
  if (!temFileira) return 0;
  if (cenaOut) *cenaOut = c;
  return 1;
}

// --------------------------------------------------- cancelamento e rede ---

static void limparJob(ComporJob *j) {
  if (!j) return;
  if (j->corpo) {
    memset(j->corpo, 0, j->nCorpo);
    free(j->corpo);
  }
  memset(j->aut, 0, sizeof j->aut);
  memset(j->via, 0, sizeof j->via);
  memset(j->perfilCab, 0, sizeof j->perfilCab);
  free(j);
}

static int jobCancelado(ComporJob *j) {
  int sim;
  pthread_mutex_lock(&trava);
  sim = j->cancelar || j->gen != geracaoAtual;
  pthread_mutex_unlock(&trava);
  return sim;
}

static int suprimidoAgora(void);

// 1 quando o contexto que autorizou o job deixou de valer: player aberto,
// perfil infantil, sem consentimento, modo seguro, "Recomendar" desligado ou
// outro perfil/sessao/conta. Sem I/O; chamada fora da `trava`.
static int contextoMudou(ComporJob *j) {
  if (suprimidoAgora() || perfilAtualValido() != j->perfil ||
      perfis_sessao_geracao() != j->sessao || contaAtualHash() != j->contaHash) {
    __atomic_store_n(&j->bloqueou, 1, __ATOMIC_RELEASE);
    return 1;
  }
  return 0;
}

static int jobBloqueou(ComporJob *j) {
  return __atomic_load_n(&j->bloqueou, __ATOMIC_ACQUIRE);
}

// Parada do job: cancelado/superado, prazo de 6 s (que inclui a resolucao das
// credenciais) ou contexto que mudou. Vale para a renovacao de JWT (gancho
// por fio de rede_parar_trocar) e para o POST (RedePedido.parar).
static int comporParar(void *u) {
  ComporJob *j = (ComporJob *)u;
  return jobCancelado(j) || agoraMs() >= j->fim || contextoMudou(j);
}

static int obterAuth(char *aut, size_t na, char *via, size_t nv,
                     char *perfilCab, size_t np, int *usouTrakt);

static void *executarCompor(void *u) {
  ComporJob *j = (ComporJob *)u;
  const char *cab[5];
  int nc = 0, http = 0, authOk = 0, usouTrakt = 0;
  int cancelado = 0, prazoEstourou = 0, transporteFalhou = 0, bloqueado = 0;
  char *respCorpo = NULL;
  size_t respLen = 0;
  long long restaMs;
  FarolCena parsed;
  int ehRegras = 0, valRc = -1;

  // 1. Credenciais DENTRO do job ja registrado: recomenda_cabecalhos pode
  //    renovar o JWT por HTTP (sessao.c); o gancho por fio faz essa rede
  //    obedecer ao cancelamento, ao prazo de 6 s e a mudanca de contexto.
  if (!jobCancelado(j) && !contextoMudou(j)) {
    RedePararEstado prev = rede_parar_trocar(comporParar, j);
    authOk = obterAuth(j->aut, sizeof j->aut, j->via, sizeof j->via,
                       j->perfilCab, sizeof j->perfilCab, &usouTrakt);
    rede_parar_trocar(prev.parar, prev.usuario);
  }
  j->usouTrakt = usouTrakt;

  // 2. Revalida TODOS os bloqueios imediatamente antes de abrir o POST.
  if (jobCancelado(j)) cancelado = 1;
  else if (jobBloqueou(j) || contextoMudou(j)) bloqueado = 1;
  else if ((restaMs = j->fim - agoraMs()) <= 0) prazoEstourou = 1;
  else if (!authOk) transporteFalhou = 1;
  if (!cancelado && !bloqueado && !prazoEstourou && !transporteFalhou &&
      j->modo == FC_MODO_HOME) {
    int marcouMem = 0, gravouMeta = 0;
    // A cota Home (1/dia local/perfil) so e gasta quando o POST vai sair.
    if (jobBloqueou(j) || contextoMudou(j)) {
      bloqueado = 1;
    } else if ((restaMs = j->fim - agoraMs()) <= 0) {
      prazoEstourou = 1;
    } else {
      pthread_mutex_lock(&trava);
      if (j->cancelar || j->gen != geracaoAtual) {
        cancelado = 1;
      } else if (jobBloqueou(j)) {
        bloqueado = 1;
      } else if (agoraMs() >= j->fim) {
        prazoEstourou = 1;
      } else {
        homeDiaMem.perfil = j->perfil;
        homeDiaMem.contaHash = j->contaHash;
        homeDiaMem.diaLocal = j->diaLocal;
        marcouMem = 1;
      }
      pthread_mutex_unlock(&trava);
    }
    if (marcouMem) {
      if (jobCancelado(j)) cancelado = 1;
      else if (jobBloqueou(j) || contextoMudou(j)) bloqueado = 1;
      else if ((restaMs = j->fim - agoraMs()) <= 0) prazoEstourou = 1;
      else {
        gravarMetaDia(j->perfil, j->diaLocal, j->quando, j->contaHash);
        gravouMeta = 1;
        if (jobCancelado(j)) cancelado = 1;
        else if (jobBloqueou(j) || contextoMudou(j)) bloqueado = 1;
        else if ((restaMs = j->fim - agoraMs()) <= 0) prazoEstourou = 1;
      }
      if (cancelado || bloqueado || prazoEstourou) {
        pthread_mutex_lock(&trava);
        if (homeDiaMem.perfil == j->perfil &&
            homeDiaMem.contaHash == j->contaHash &&
            homeDiaMem.diaLocal == j->diaLocal) {
          memset(&homeDiaMem, 0, sizeof homeDiaMem);
        }
        pthread_mutex_unlock(&trava);
        if (gravouMeta) {
          char nomeMeta[64];
          nomeArquivoMeta(j->perfil, nomeMeta, sizeof nomeMeta);
          lojaApagar(nomeMeta);
        }
      }
    }
  }
  restaMs = j->fim - agoraMs();
  if (!cancelado && !bloqueado && !prazoEstourou && !transporteFalhou && restaMs <= 0)
    prazoEstourou = 1;
  if (cancelado || bloqueado || prazoEstourou || transporteFalhou)
    goto depositar; // zero bytes enviados

  if (j->aut[0]) cab[nc++] = j->aut;
  if (j->via[0]) cab[nc++] = j->via;
  if (j->perfilCab[0]) cab[nc++] = j->perfilCab;
#ifndef __EMSCRIPTEN__
  cab[nc++] = "Content-Type: application/json";
#endif
  cab[nc] = NULL;

#ifdef __EMSCRIPTEN__
  {
    RedePararEstado prev = rede_parar_trocar(comporParar, j);
    int seg = (int)((restaMs + 999LL) / 1000LL);
    if (seg < 1) seg = 1;
    respCorpo = rede_postar_st(j->url, seg, cab, j->corpo, &http);
    rede_parar_trocar(prev.parar, prev.usuario);
    if (jobCancelado(j)) {
      cancelado = 1;
    } else if (jobBloqueou(j)) {
      bloqueado = 1;
    } else if (agoraMs() >= j->fim && http == 0) {
      prazoEstourou = 1;
    } else if (!respCorpo || http == 0) {
      transporteFalhou = 1;
    } else {
      respLen = strlen(respCorpo);
    }
  }
#else
  {
    RedePedido req;
    RedeResposta res;
    memset(&req, 0, sizeof req);
    memset(&res, 0, sizeof res);
    req.metodo = "POST";
    req.url = j->url;
    req.cabecalhos = cab;
    req.corpo = j->corpo;
    req.n_corpo = j->nCorpo;
    req.prazo_ms = (unsigned)restaMs;
    req.max_bytes = FAROL_CENA_MAX + 512u; // rejeita > 8 KB na validacao tambem
    req.max_cabecalhos = 8192u;
    req.seguir = 0;
    req.parar = comporParar;
    req.parar_usuario = j;

    rede_pedir(&req, &res);
    http = res.status;
    if (jobCancelado(j)) {
      cancelado = 1;
    } else if (jobBloqueou(j)) {
      bloqueado = 1;
    } else if (res.erro == REDE_PRAZO ||
               (res.erro == REDE_CANCELADO && agoraMs() >= j->fim)) {
      prazoEstourou = 1;
    } else if (res.erro == REDE_LIMITE_CORPO) {
      valRc = -1; // > 8 KB
    } else if (res.erro != REDE_OK || http == 0) {
      transporteFalhou = 1;
    } else if (res.corpo && res.n_corpo > 0) {
      respCorpo = res.corpo;
      respLen = res.n_corpo;
      res.corpo = NULL;
    }
    rede_resposta_limpar(&res);
  }
#endif

  if (!cancelado && !bloqueado && !prazoEstourou && !transporteFalhou && http == 200 && respCorpo) {
    valRc = farolcompor_validar_resposta(j->modo, respCorpo, respLen,
                                         j->cand, j->nCand, &parsed, &ehRegras);
  }
  free(respCorpo);

  if (!cancelado && !bloqueado && http == 401 && j->usouTrakt)
    trakt_http_recusada();

depositar:
  // O worker NAO adota diretamente nem toca UI: apenas deposita em `pendente`
  // se a geracao ainda e atual. O fio principal adota em `farolcompor_passo()`.
  pthread_mutex_lock(&trava);
  if (jobAtual == j) jobAtual = NULL;
  if (j->gen == geracaoAtual) {
    if (cancelado || j->cancelar) {
      estadoAtual.estado = FC_CANCELADO;
      estadoAtual.motivo = FC_MOT_CANCELADO;
      estadoAtual.http = 0;
      estadoAtual.preservaRegra = 1;
    } else if (bloqueado) {
      estadoAtual.estado = FC_SUPRIMIDO;
      estadoAtual.motivo = FC_MOT_SUPRIMIDO;
      estadoAtual.http = 0;
      estadoAtual.doCache = 0;
      estadoAtual.preservaRegra = 1;
    } else if (valRc > 0) {
      memset(&pendente, 0, sizeof pendente);
      pendente.tem = 1;
      pendente.modo = j->modo;
      pendente.perfil = j->perfil;
      pendente.sessao = j->sessao;
      pendente.contaHash = j->contaHash;
      pendente.hModelo = j->hModelo;
      pendente.diaLocal = j->diaLocal;
      pendente.feita = j->quando;
      pendente.doCache = 0;
      pendente.cena = parsed;
      pendente.nCand = j->nCand;
      if (j->nCand > 0)
        memcpy(pendente.cand, j->cand, (size_t)j->nCand * sizeof j->cand[0]);
      estadoAtual.http = http;
      estadoAtual.motivo = FC_MOT_OK;
      estadoAtual.preservaRegra = 0;
    } else {
      FarolComporMotivo mot;
      if (ehRegras) mot = FC_MOT_REGRAS;
      else if (prazoEstourou) mot = FC_MOT_PRAZO;
      else if (transporteFalhou) mot = FC_MOT_OFFLINE;
      else if (http == 429) mot = FC_MOT_LIMITE;
      else mot = FC_MOT_ERRO;

      estadoAtual.estado = FC_REGRAS;
      estadoAtual.motivo = mot;
      estadoAtual.http = http;
      estadoAtual.doCache = 0;
      estadoAtual.preservaRegra = 1;
    }
  }
  ativos--;
  if (ativos == 0) pthread_cond_broadcast(&condFim);
  pthread_mutex_unlock(&trava);

  limparJob(j);
  return NULL;
}

// ------------------------------------------------ gates e controle publico -

static int suprimidoAgora(void) {
  if (player_aberto()) return 1;
  if (perfil_infantil_ativo()) return 1;
  if (!farolcompor_consentimento()) return 1;
  if (seguro_perfil_ativo()) return 1;
  if (!farolsinal_recomendar()) return 1;
  return 0;
}

void farolcompor_definir_consentimento(int consentiu) {
  pthread_mutex_lock(&trava);
  consentiuCfg = consentiu ? 1 : 0;
  if (!consentiuCfg) {
    if (jobAtual) { jobAtual->cancelar = 1; jobAtual = NULL; }
    geracaoAtual++;
    memset(&pendente, 0, sizeof pendente);
    memset(&adotadaHome, 0, sizeof adotadaHome);
    memset(adotadaModos, 0, sizeof adotadaModos);
    homeGeracao++;
  }
  pthread_mutex_unlock(&trava);
}

void farolcompor_definir_allowlist(FarolComporAllowlistFn fn, void *usuario) {
  pthread_mutex_lock(&trava);
  allowlistFn = fn;
  allowlistUser = usuario;
  pthread_mutex_unlock(&trava);
}

int farolcompor_consentimento(void) {
  FarolComporAllowlistFn fn;
  void *u;
  int cfg;
  pthread_mutex_lock(&trava);
  fn = allowlistFn;
  u = allowlistUser;
  cfg = consentiuCfg;
  pthread_mutex_unlock(&trava);
  if (fn) return fn(perfilAtualValido(), u) != 0;
  return cfg != 0;
}

static int obterBaseUrl(char *dst, size_t cap) {
  dst[0] = '\0';
  pthread_mutex_lock(&trava);
  if (temUrlOverride) {
    snprintf(dst, cap, "%s", urlOverride);
    pthread_mutex_unlock(&trava);
    return dst[0] != '\0';
  }
  pthread_mutex_unlock(&trava);
#ifdef FAROL_COMPOR_WORKER
  if (FAROL_COMPOR_WORKER[0]) {
    snprintf(dst, cap, "%s", FAROL_COMPOR_WORKER);
    return 1;
  }
#endif
  nv_rec_url_copiar(dst, cap);
  return dst[0] != '\0';
}

static int obterAuth(char *aut, size_t na, char *via, size_t nv,
                     char *perfilCab, size_t np, int *usouTrakt) {
  const char *cab[4];
  aut[0] = '\0'; via[0] = '\0'; perfilCab[0] = '\0';
  *usouTrakt = 0;

  pthread_mutex_lock(&trava);
  if (temAuthOverride) {
    snprintf(aut, na, "%s", authOverrideAut);
    snprintf(via, nv, "%s", authOverrideVia);
    snprintf(perfilCab, np, "%s", authOverridePerfil);
    *usouTrakt = !strcmp(via, "X-Nuvio-Auth: trakt");
    pthread_mutex_unlock(&trava);
    return aut[0] != '\0';
  }
  pthread_mutex_unlock(&trava);

  if (!recomenda_cabecalhos(cab, aut, na, via, nv, perfilCab, np)) return 0;
  *usouTrakt = !strcmp(via, "X-Nuvio-Auth: trakt");
  return 1;
}

static void marcarSuprimidoEstado(FarolComporModo modo, int perfil, uint64_t sessao) {
  pthread_mutex_lock(&trava);
  estadoAtual.estado = FC_SUPRIMIDO;
  estadoAtual.motivo = FC_MOT_SUPRIMIDO;
  estadoAtual.modo = modo;
  estadoAtual.perfil = perfil;
  estadoAtual.sessao = sessao;
  estadoAtual.http = 0;
  estadoAtual.doCache = 0;
  estadoAtual.preservaRegra = 1;
  pthread_mutex_unlock(&trava);
}

static void copiarCandParaSlot(CenaSlot *dst, const FarolComporCand *src, int nCand) {
  int k = nCand;
  if (k < 0) k = 0;
  if (k > FAROL_COMPOR_CAND_MAX) k = FAROL_COMPOR_CAND_MAX;
  dst->nCand = k;
  if (k > 0 && src) memcpy(dst->cand, src, (size_t)k * sizeof src[0]);
}

int farolcompor_pedir(const FarolComporPedido *ped) {
  char corpoBuf[FAROL_COMPOR_CORPO_MAX + 1];
  char baseUrl[384];
  size_t nCorpo;
  int perfil, diaLocal, consentiu, infantil, k;
  int mapa[FAROL_COMPOR_CAND_MAX], nMapa = 0;
  uint64_t sessao, contaHash, gen0;
  long long agora;
  FarolComporPedido copia;

  if (!ped || !modoNome(ped->modo)) return 0;
  // O job fica vinculado a esta geracao desde o inicio: qualquer troca de
  // perfil/cancelamento ate o registro do job aborta o pedido.
  pthread_mutex_lock(&trava);
  gen0 = geracaoAtual;
  pthread_mutex_unlock(&trava);
  perfil = perfilAtualValido();
  sessao = perfis_sessao_geracao();
  contaHash = contaAtualHash();
  agora = agoraS();
  diaLocal = diaLocalDe(agora);

  consentiu = ped->consentiu && farolcompor_consentimento();
  infantil = ped->infantil || perfil_infantil_ativo();
  if (player_aberto() || infantil || !consentiu ||
      seguro_perfil_ativo() || !farolsinal_recomendar()) {
    marcarSuprimidoEstado(ped->modo, perfil, sessao);
    return 0;
  }

  // Politica da Home: cache de 24 h em disco + no maximo 1 chamada Home/dia local/perfil.
  if (ped->modo == FC_MODO_HOME) {
    FarolCena cenaCache;
    MetaDia meta;
    int memBloq = 0, jaAtivo = 0;

    pthread_mutex_lock(&trava);
    if (adotadaHome.tem && adotadaHome.perfil == perfil &&
        adotadaHome.sessao == sessao && adotadaHome.contaHash == contaHash &&
        (agora - adotadaHome.feita) >= 0 &&
        (agora - adotadaHome.feita) < FAROL_COMPOR_CACHE_TTL_S) {
      jaAtivo = 1;
    } else if (pendente.tem && pendente.modo == FC_MODO_HOME &&
               pendente.perfil == perfil && pendente.sessao == sessao &&
               pendente.contaHash == contaHash) {
      jaAtivo = 1;
    } else if (jobAtual && !jobAtual->cancelar &&
               jobAtual->modo == FC_MODO_HOME && jobAtual->perfil == perfil &&
               jobAtual->sessao == sessao && jobAtual->contaHash == contaHash) {
      jaAtivo = 1;
    }
    pthread_mutex_unlock(&trava);
    if (jaAtivo) return 1;

    if (carregarCacheHomeDisco(perfil, contaHash, agora, &cenaCache)) {
      pthread_mutex_lock(&trava);
      if (jobAtual) { jobAtual->cancelar = 1; jobAtual = NULL; }
      geracaoAtual++;
      memset(&pendente, 0, sizeof pendente);
      pendente.tem = 1;
      pendente.modo = FC_MODO_HOME;
      pendente.perfil = perfil;
      pendente.sessao = sessao;
      pendente.contaHash = contaHash;
      pendente.hModelo = ped->hModelo;
      pendente.diaLocal = diaLocal;
      pendente.feita = cenaCache.feita;
      pendente.doCache = 1;
      pendente.cena = cenaCache;
      copiarCandParaSlot(&pendente, ped->candidatos, ped->nCandidatos);
      estadoAtual.estado = FC_CARREGANDO;
      estadoAtual.motivo = FC_MOT_CACHE;
      estadoAtual.modo = FC_MODO_HOME;
      estadoAtual.perfil = perfil;
      estadoAtual.sessao = sessao;
      estadoAtual.doCache = 1;
      estadoAtual.http = 0;
      estadoAtual.geracao = geracaoAtual;
      estadoAtual.preservaRegra = 0;
      pthread_mutex_unlock(&trava);
      return 1;
    }

    pthread_mutex_lock(&trava);
    memBloq = (homeDiaMem.perfil == perfil &&
               homeDiaMem.contaHash == contaHash &&
               homeDiaMem.diaLocal == diaLocal);
    pthread_mutex_unlock(&trava);
    if (!memBloq) {
      lerMetaDia(perfil, contaHash, &meta);
      if (meta.valido && meta.dia == diaLocal) memBloq = 1;
    }
    if (memBloq) {
      pthread_mutex_lock(&trava);
      estadoAtual.estado = FC_REGRAS;
      estadoAtual.motivo = FC_MOT_LIMITE;
      estadoAtual.modo = FC_MODO_HOME;
      estadoAtual.perfil = perfil;
      estadoAtual.sessao = sessao;
      estadoAtual.doCache = 0;
      estadoAtual.http = 0;
      estadoAtual.preservaRegra = 1;
      pthread_mutex_unlock(&trava);
      return 0;
    }
  }

  copia = *ped;
  copia.consentiu = 1;
  copia.infantil = 0;
  if (!copia.idioma || !idiomaValido(copia.idioma))
    copia.idioma = ajustes_tmdb_idioma();

  nCorpo = farolcompor_montar_corpo_mapa(&copia, corpoBuf, sizeof corpoBuf, mapa, &nMapa);
  if (nCorpo == 0) {
    pthread_mutex_lock(&trava);
    estadoAtual.estado = FC_REGRAS;
    estadoAtual.motivo = FC_MOT_ERRO;
    estadoAtual.preservaRegra = 1;
    pthread_mutex_unlock(&trava);
    return 0;
  }

  // Credenciais NAO sao resolvidas aqui (podem fazer HTTP fora do controle de
  // cancelamento): o worker resolve dentro do job registrado.
  if (rede_saude_offline() || !obterBaseUrl(baseUrl, sizeof baseUrl)) {
    memset(corpoBuf, 0, sizeof corpoBuf);
    pthread_mutex_lock(&trava);
    estadoAtual.estado = FC_REGRAS;
    estadoAtual.motivo = FC_MOT_OFFLINE;
    estadoAtual.modo = ped->modo;
    estadoAtual.perfil = perfil;
    estadoAtual.sessao = sessao;
    estadoAtual.http = 0;
    estadoAtual.doCache = 0;
    estadoAtual.preservaRegra = 1;
    pthread_mutex_unlock(&trava);
    return 0;
  }

  {
    ComporJob *j = (ComporJob *)calloc(1, sizeof *j);
    size_t nBase;
    pthread_attr_t attr;
    pthread_t fio;
    int erro, attrOk;

    if (!j) { memset(corpoBuf, 0, sizeof corpoBuf); return 0; }
    j->corpo = (char *)malloc(nCorpo + 1);
    if (!j->corpo) { memset(corpoBuf, 0, sizeof corpoBuf); limparJob(j); return 0; }
    memcpy(j->corpo, corpoBuf, nCorpo + 1);
    memset(corpoBuf, 0, sizeof corpoBuf);
    j->nCorpo = nCorpo;

    nBase = strlen(baseUrl);
    while (nBase > 0 && baseUrl[nBase - 1] == '/') baseUrl[--nBase] = '\0';
    snprintf(j->url, sizeof j->url, "%s/v1/farol/compor", baseUrl);

    j->modo = ped->modo;
    j->perfil = perfil;
    j->sessao = sessao;
    j->contaHash = contaHash;
    j->hModelo = ped->hModelo;
    j->diaLocal = diaLocal;
    j->quando = agora;
    j->prazoMs = FAROL_COMPOR_PRAZO_MS;
    j->fim = agoraMs() + (long long)j->prazoMs;
    // A lista do job e a COMPACTADA que entrou no corpo: "c<k>" do JSON ->
    // j->cand[k], 1-para-1, mesmo quando sanitizacao/teto descartaram.
    j->nCand = nMapa;
    for (k = 0; k < nMapa; k++) j->cand[k] = ped->candidatos[mapa[k]];

    attrOk = pthread_attr_init(&attr) == 0;
    if (!attrOk) { limparJob(j); return 0; }
    erro = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_mutex_lock(&trava);
    if (geracaoAtual != gen0) {
      // Cancelado/trocado de perfil enquanto o corpo era montado.
      estadoAtual.estado = FC_CANCELADO;
      estadoAtual.motivo = FC_MOT_CANCELADO;
      estadoAtual.http = 0;
      estadoAtual.preservaRegra = 1;
      pthread_mutex_unlock(&trava);
      pthread_attr_destroy(&attr);
      limparJob(j);
      return 0;
    }
    if (jobAtual) { jobAtual->cancelar = 1; jobAtual = NULL; }
    geracaoAtual++;
    j->gen = geracaoAtual;
    memset(&pendente, 0, sizeof pendente);
    if (!erro) {
      ativos++;
      jobAtual = j;
      estadoAtual.estado = FC_CARREGANDO;
      estadoAtual.motivo = FC_MOT_NENHUM;
      estadoAtual.modo = ped->modo;
      estadoAtual.perfil = perfil;
      estadoAtual.sessao = sessao;
      estadoAtual.http = 0;
      estadoAtual.doCache = 0;
      estadoAtual.preservaRegra = 1;
      estadoAtual.geracao = geracaoAtual;
      erro = pthread_create(&fio, &attr, executarCompor, j);
      if (erro) {
        if (jobAtual == j) jobAtual = NULL;
        ativos--;
        if (ativos == 0) pthread_cond_broadcast(&condFim);
      }
    }
    if (erro) {
      estadoAtual.estado = FC_REGRAS;
      estadoAtual.motivo = FC_MOT_OFFLINE;
      estadoAtual.preservaRegra = 1;
    }
    pthread_mutex_unlock(&trava);
    pthread_attr_destroy(&attr);

    if (erro) { limparJob(j); return 0; }
    return 1;
  }
}

int farolcompor_pedir_home(const FarolGosto *g,
                           const FarolComporCand *cand, int nCand,
                           const FarolComporAncora *ancoras, int nAncoras,
                           const char *(*nomePessoaFn)(int32_t, void *),
                           void *u, uint32_t hModelo) {
  FarolComporAfin gen[FAROL_COMPOR_GEN_MAX];
  FarolComporAfin tem[FAROL_COMPOR_TEMA_MAX];
  FarolComporAfin pes[FAROL_COMPOR_PESSOA_MAX];
  int ng = 0, nt = 0, np = 0, hora, fimSem = 0;
  long long agora = agoraS();
  FarolComporPedido ped;

  if (!g) return 0;
  if (farolrec_nivel(g) < 1) return 0;
  farolconsent_sincronizar_ativo();
  farolcompor_extrair_gosto(g, diaModeloDe(agora), nomePessoaFn, NULL, u,
                            gen, &ng, tem, &nt, pes, &np);
  hora = faixaLocalDe(agora, &fimSem);

  memset(&ped, 0, sizeof ped);
  ped.modo = FC_MODO_HOME;
  ped.idioma = ajustes_tmdb_idioma();
  ped.hora = hora;
  ped.fimSemana = fimSem;
  ped.nivel = farolrec_nivel(g);
  ped.infantil = perfil_infantil_ativo();
  ped.consentiu = farolcompor_consentimento();
  ped.hModelo = hModelo;
  ped.generos = gen; ped.nGeneros = ng;
  ped.temas = tem; ped.nTemas = nt;
  ped.pessoas = pes; ped.nPessoas = np;
  ped.ancoras = ancoras; ped.nAncoras = nAncoras;
  ped.candidatos = cand; ped.nCandidatos = nCand;
  return farolcompor_pedir(&ped);
}

int farolcompor_carregar_cache_home(void) {
  int perfil = perfilAtualValido();
  uint64_t sessao = perfis_sessao_geracao();
  uint64_t conta = contaAtualHash();
  long long agora = agoraS();
  FarolCena c;

  farolconsent_sincronizar_ativo();
  if (suprimidoAgora()) return 0;
  if (!carregarCacheHomeDisco(perfil, conta, agora, &c)) return 0;

  pthread_mutex_lock(&trava);
  memset(&adotadaHome, 0, sizeof adotadaHome);
  adotadaHome.tem = 1;
  adotadaHome.modo = FC_MODO_HOME;
  adotadaHome.perfil = perfil;
  adotadaHome.sessao = sessao;
  adotadaHome.contaHash = conta;
  adotadaHome.diaLocal = diaLocalDe(agora);
  adotadaHome.feita = c.feita;
  adotadaHome.doCache = 1;
  adotadaHome.cena = c;
  homeGeracao++;
  adotadaHome.geracaoAdotada = homeGeracao;
  adotadaModos[FC_MODO_HOME] = adotadaHome;
  estadoAtual.estado = FC_PRONTO;
  estadoAtual.motivo = FC_MOT_CACHE;
  estadoAtual.modo = FC_MODO_HOME;
  estadoAtual.perfil = perfil;
  estadoAtual.sessao = sessao;
  estadoAtual.doCache = 1;
  estadoAtual.preservaRegra = 0;
  pthread_mutex_unlock(&trava);
  return 1;
}

int farolcompor_passo(void) {
  CenaSlot local;
  int perfil, sup, cons, ply, vis, sup2, adotou = 0;
  uint64_t sessao, conta;
  long long agora;

  cons = farolcompor_consentimento();
  ply = player_aberto();
  sup = ply || perfil_infantil_ativo() || !cons ||
        seguro_perfil_ativo() || !farolsinal_recomendar();
  perfil = perfilAtualValido();
  sessao = perfis_sessao_geracao();
  conta = contaAtualHash();
  agora = agoraS();

  if (sup) {
    // Suprime qualquer pendencia ou cena adotada se player abriu / infantil / sem consentimento.
    int tinhaAdotada = 0;
    int hardSup = perfil_infantil_ativo() || !cons ||
                  seguro_perfil_ativo() || !farolsinal_recomendar();
    pthread_mutex_lock(&trava);
    memset(&pendente, 0, sizeof pendente);
    if (hardSup) {
      if (adotadaHome.tem) tinhaAdotada = 1;
      memset(&adotadaHome, 0, sizeof adotadaHome);
      memset(adotadaModos, 0, sizeof adotadaModos);
      if (tinhaAdotada) homeGeracao++;
      if (jobAtual) { jobAtual->cancelar = 1; jobAtual = NULL; geracaoAtual++; }
      estadoAtual.estado = FC_SUPRIMIDO;
      estadoAtual.motivo = FC_MOT_SUPRIMIDO;
      estadoAtual.preservaRegra = 1;
    } else if (ply && estadoAtual.estado == FC_CARREGANDO) {
      // Player abriu no meio: descarta resposta pendente para nao adotar.
      if (jobAtual) { jobAtual->cancelar = 1; jobAtual = NULL; geracaoAtual++; }
      estadoAtual.estado = FC_SUPRIMIDO;
      estadoAtual.motivo = FC_MOT_SUPRIMIDO;
      estadoAtual.preservaRegra = 1;
    }
    pthread_mutex_unlock(&trava);
    return 0;
  }

  vis = homeVisivel();
  memset(&local, 0, sizeof local);
  pthread_mutex_lock(&trava);
  if (pendente.tem) {
    // Confere identidade/contexto no momento exato da adocao no fio principal.
    if (pendente.perfil != perfil || pendente.sessao != sessao ||
        pendente.contaHash != conta) {
      memset(&pendente, 0, sizeof pendente);
      estadoAtual.estado = FC_REGRAS;
      estadoAtual.motivo = FC_MOT_CANCELADO;
      estadoAtual.preservaRegra = 1;
    } else if (pendente.modo == FC_MODO_HOME && adotadaHome.tem &&
               adotadaHome.perfil == perfil && vis) {
      // Mantem estagiado ate a Home sair da tela (nao troca cartaz olhando).
    } else {
      local = pendente;
      memset(&pendente, 0, sizeof pendente);
    }
  }
  pthread_mutex_unlock(&trava);

  if (!local.tem) return 0;

  if (local.modo == FC_MODO_HOME && !local.doCache) {
    char cacheJson[FAROL_CENA_MAX + 1];
    if (serializarCenaHomeCache(&local.cena, local.cand, local.nCand,
                                perfil, local.feita > 0 ? local.feita : agora,
                                cacheJson, sizeof cacheJson)) {
      char nome[64];
      nomeArquivoCena(perfil, nome, sizeof nome);
      lojaGravar(nome, cacheJson);
    }
    gravarMetaDia(perfil, local.diaLocal > 0 ? local.diaLocal : diaLocalDe(agora),
                  local.feita > 0 ? local.feita : agora, conta);
  }

  sup2 = suprimidoAgora();
  pthread_mutex_lock(&trava);
  // Confere se nada cancelou/trocou enquanto gravava em disco.
  if (perfilAtualValido() == perfil && perfis_sessao_geracao() == sessao &&
      contaAtualHash() == conta && !sup2) {
    if (local.modo == FC_MODO_HOME) {
      adotadaHome = local;
      homeGeracao++;
      adotadaHome.geracaoAdotada = homeGeracao;
    }
    if ((unsigned)local.modo < FC_MODO_N) {
      adotadaModos[local.modo] = local;
      adotadaModos[local.modo].geracaoAdotada =
        (local.modo == FC_MODO_HOME) ? homeGeracao : (unsigned)geracaoAtual;
    }
    estadoAtual.estado = FC_PRONTO;
    estadoAtual.motivo = local.doCache ? FC_MOT_CACHE : FC_MOT_OK;
    estadoAtual.modo = local.modo;
    estadoAtual.perfil = perfil;
    estadoAtual.sessao = sessao;
    estadoAtual.doCache = local.doCache;
    estadoAtual.preservaRegra = 0;
    adotou = 1;
  }
  pthread_mutex_unlock(&trava);
  return adotou;
}

void farolcompor_cancelar(void) {
  pthread_mutex_lock(&trava);
  if (jobAtual) {
    jobAtual->cancelar = 1;
    jobAtual = NULL;
  }
  geracaoAtual++;
  memset(&pendente, 0, sizeof pendente);
  if (estadoAtual.estado == FC_CARREGANDO) {
    estadoAtual.estado = FC_CANCELADO;
    estadoAtual.motivo = FC_MOT_CANCELADO;
    estadoAtual.preservaRegra = 1;
  }
  pthread_mutex_unlock(&trava);
}

void farolcompor_encerrar(void) {
  pthread_mutex_lock(&trava);
  if (jobAtual) { jobAtual->cancelar = 1; jobAtual = NULL; }
  geracaoAtual++;
  while (ativos > 0) pthread_cond_wait(&condFim, &trava);
  memset(&pendente, 0, sizeof pendente);
  memset(&adotadaHome, 0, sizeof adotadaHome);
  memset(adotadaModos, 0, sizeof adotadaModos);
  memset(&estadoAtual, 0, sizeof estadoAtual);
  memset(&retUltimo, 0, sizeof retUltimo);
  pthread_mutex_unlock(&trava);
}

void farolcompor_trocar_perfil(void) {
  pthread_mutex_lock(&trava);
  if (jobAtual) { jobAtual->cancelar = 1; jobAtual = NULL; }
  geracaoAtual++;
  memset(&pendente, 0, sizeof pendente);
  memset(&adotadaHome, 0, sizeof adotadaHome);
  memset(adotadaModos, 0, sizeof adotadaModos);
  memset(&estadoAtual, 0, sizeof estadoAtual);
  memset(&homeDiaMem, 0, sizeof homeDiaMem);
  memset(&retUltimo, 0, sizeof retUltimo);
  homeGeracao++;
  pthread_mutex_unlock(&trava);
  farolconsent_invalidar();
}

void farolcompor_esquecer(void) {
  int p;
  farolcompor_trocar_perfil();
  for (p = 1; p <= 8; p++) {
    char nome[64];
    nomeArquivoCena(p, nome, sizeof nome);
    lojaApagar(nome);
    nomeArquivoMeta(p, nome, sizeof nome);
    lojaApagar(nome);
  }
  farolconsent_esquecer();
}

void farolcompor_zerar_gosto(void) {
  int p = perfilAtualValido();
  char nome[64];
  farolcompor_trocar_perfil();
  nomeArquivoCena(p, nome, sizeof nome);
  lojaApagar(nome);
  nomeArquivoMeta(p, nome, sizeof nome);
  lojaApagar(nome);
}

// ------------------------------------------- retomada sem spoiler (57) ---

int farolcompor_pedir_retomada(const char *imdb, const char *titulo,
                               int temporada, int episodio,
                               const char *sinopse) {
  FarolComporPedido ped;
  FarolComporCand c;
  int perfil, repetido, ok;
  uint64_t sessao, conta;

  if (!imdbValido(imdb) || !titulo || !titulo[0] || !sinopse || !sinopse[0])
    return 0;
  if (temporada < 1 || temporada > 999 || episodio < 1 || episodio > 9999)
    return 0;
  // Bloqueios ANTES de qualquer coisa: nada e montado nem gravado.
  if (suprimidoAgora()) return 0;
  // A Home (ou outro modo) em voo tem prioridade: nao derruba, tenta depois.
  if (farolcompor_em_voo()) return 0;

  perfil = perfilAtualValido();
  sessao = perfis_sessao_geracao();
  conta = contaAtualHash();
  pthread_mutex_lock(&trava);
  repetido = retUltimo.tem && retUltimo.perfil == perfil &&
             retUltimo.sessao == sessao && retUltimo.contaHash == conta &&
             retUltimo.t == temporada && retUltimo.e == episodio &&
             !strcmp(retUltimo.imdb, imdb);
  pthread_mutex_unlock(&trava);
  if (repetido) return 0;

  memset(&c, 0, sizeof c);
  snprintf(c.imdb, sizeof c.imdb, "%s", imdb);
  c.tipo = 1;
  snprintf(c.titulo, sizeof c.titulo, "%s", titulo);
  c.nota10 = -1;

  memset(&ped, 0, sizeof ped);
  ped.modo = FC_MODO_RETOMADA;
  ped.idioma = ajustes_tmdb_idioma();
  ped.hora = -1;
  ped.fimSemana = -1;
  ped.infantil = perfil_infantil_ativo();
  ped.consentiu = farolcompor_consentimento();
  ped.candidatos = &c;
  ped.nCandidatos = 1;
  ped.retTemporada = temporada;
  ped.retEpisodio = episodio;
  ped.retSinopse = sinopse;
  ok = farolcompor_pedir(&ped);

  if (ok) {
    pthread_mutex_lock(&trava);
    retUltimo.tem = 1;
    retUltimo.perfil = perfil;
    retUltimo.sessao = sessao;
    retUltimo.contaHash = conta;
    snprintf(retUltimo.imdb, sizeof retUltimo.imdb, "%s", imdb);
    retUltimo.t = temporada;
    retUltimo.e = episodio;
    pthread_mutex_unlock(&trava);
  }
  return ok;
}

int farolcompor_retomada_resumo(const char *imdb, int temporada, int episodio,
                                char *dst, size_t cap) {
  FarolCena cena;
  FarolComporCand cand[FAROL_COMPOR_CAND_MAX];
  int nCand = 0, p, i;

  if (dst && cap) dst[0] = '\0';
  if (!dst || cap < 2 || !imdbValido(imdb) || temporada < 1 || episodio < 1)
    return 0;
  if (!farolcompor_cena_modo(FC_MODO_RETOMADA, &cena, cand, &nCand,
                             FAROL_COMPOR_CAND_MAX, NULL))
    return 0;
  if (nCand < 1 || strcmp(cand[0].imdb, imdb) != 0) return 0;

  for (p = 0; p < cena.nPecas; p++) {
    const FarolPeca *pc = &cena.pecas[p];
    if (pc->tipo != FP_INFO) continue;
    // So a peca da propria serie (c0 ou o tt dela).
    if (!(pc->u.info.alvo.c == 0 ||
          (pc->u.info.alvo.c < 0 && !strcmp(pc->u.info.alvo.s, imdb))))
      continue;
    for (i = 0; i < pc->u.info.nItens; i++) {
      const FarolInfoItem *it = &pc->u.info.itens[i];
      const FarolTexto *t = &it->l;
      const char *x;
      if (it->k != FP_INFO_RETOMADA || !t->usaX) continue;
      // O (t, e) que o servidor diz ter resumido TEM de ser o pedido.
      if (strcmp(t->m, "FT_INFO_RETOMADA_SIMPLES") != 0 || t->nArgs != 2)
        continue;
      { char ts[16], es[16];
        snprintf(ts, sizeof ts, "%d", temporada);
        snprintf(es, sizeof es, "%d", episodio);
        if (strcmp(t->a[0], ts) != 0 || strcmp(t->a[1], es) != 0) continue; }
      x = farolpeca_x(&cena, t);
      if (!x || !farolinfo_retomada_linha_ok(x) || !faroltexto_aceita(x) ||
          !retomadaResumoSemSpoiler(x, temporada, episodio))
        continue;
      if (strlen(x) + 1 > cap) return 0;
      memcpy(dst, x, strlen(x) + 1);
      return 1;
    }
  }
  return 0;
}

// ------------------------------------------------------------ leitura UI ---

FarolComporEstado farolcompor_estado(FarolComporInfo *info) {
  FarolComporEstado st;
  pthread_mutex_lock(&trava);
  if (info) *info = estadoAtual;
  st = estadoAtual.estado;
  pthread_mutex_unlock(&trava);
  return st;
}

int farolcompor_em_voo(void) {
  int n;
  pthread_mutex_lock(&trava);
  n = ativos;
  pthread_mutex_unlock(&trava);
  return n > 0;
}

unsigned farolcompor_home_geracao(void) {
  unsigned g;
  pthread_mutex_lock(&trava);
  g = homeGeracao;
  pthread_mutex_unlock(&trava);
  return g;
}

int farolcompor_cena_adotada(FarolCena *cenaOut, FarolComporCand *candOut,
                             int *nCandOut, int maxCand, uint32_t *hModeloOut) {
  int perfil = perfilAtualValido();
  uint64_t sessao = perfis_sessao_geracao();
  uint64_t conta = contaAtualHash();
  long long agora = agoraS();
  int ok = 0;

  if (nCandOut) *nCandOut = 0;
  if (hModeloOut) *hModeloOut = 0;
  if (perfil_infantil_ativo() || !farolcompor_consentimento() ||
      seguro_perfil_ativo() || !farolsinal_recomendar())
    return 0;

  pthread_mutex_lock(&trava);
  if (adotadaHome.tem && adotadaHome.perfil == perfil &&
      adotadaHome.sessao == sessao && adotadaHome.contaHash == conta) {
    long long idade = agora - adotadaHome.feita;
    if (idade >= 0 && idade < FAROL_COMPOR_CACHE_TTL_S) {
      if (cenaOut) *cenaOut = adotadaHome.cena;
      if (hModeloOut) *hModeloOut = adotadaHome.hModelo;
      if (candOut && nCandOut && maxCand > 0) {
        int k = adotadaHome.nCand < maxCand ? adotadaHome.nCand : maxCand;
        if (k > 0) memcpy(candOut, adotadaHome.cand, (size_t)k * sizeof candOut[0]);
        *nCandOut = k;
      }
      ok = 1;
    }
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

int farolcompor_cena_modo(FarolComporModo modo, FarolCena *cenaOut,
                          FarolComporCand *candOut, int *nCandOut, int maxCand,
                          uint64_t *geracaoOut) {
  int perfil = perfilAtualValido();
  uint64_t sessao = perfis_sessao_geracao();
  uint64_t conta = contaAtualHash();
  int ok = 0;

  if (nCandOut) *nCandOut = 0;
  if (geracaoOut) *geracaoOut = 0;
  if ((unsigned)modo >= FC_MODO_N) return 0;
  if (player_aberto() || perfil_infantil_ativo() || !farolcompor_consentimento() ||
      seguro_perfil_ativo() || !farolsinal_recomendar())
    return 0;

  pthread_mutex_lock(&trava);
  if (adotadaModos[modo].tem && adotadaModos[modo].perfil == perfil &&
      adotadaModos[modo].sessao == sessao &&
      adotadaModos[modo].contaHash == conta) {
    if (cenaOut) *cenaOut = adotadaModos[modo].cena;
    if (geracaoOut) *geracaoOut = adotadaModos[modo].geracaoAdotada;
    if (candOut && nCandOut && maxCand > 0) {
      int k = adotadaModos[modo].nCand < maxCand ? adotadaModos[modo].nCand : maxCand;
      if (k > 0) memcpy(candOut, adotadaModos[modo].cand, (size_t)k * sizeof candOut[0]);
      *nCandOut = k;
    }
    ok = 1;
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

// --------------------------------------------------------- ganchos de teste -

void farolcompor_definir_url(const char *baseUrl) {
  pthread_mutex_lock(&trava);
  if (baseUrl && baseUrl[0]) {
    temUrlOverride = 1;
    snprintf(urlOverride, sizeof urlOverride, "%s", baseUrl);
  } else {
    temUrlOverride = 0;
    urlOverride[0] = '\0';
  }
  pthread_mutex_unlock(&trava);
}

void farolcompor_definir_auth(const char *autorizacao, const char *via,
                              const char *perfilCab) {
  pthread_mutex_lock(&trava);
  if (autorizacao && autorizacao[0]) {
    temAuthOverride = 1;
    snprintf(authOverrideAut, sizeof authOverrideAut, "%s", autorizacao);
    snprintf(authOverrideVia, sizeof authOverrideVia, "%s",
             via ? via : "X-Nuvio-Auth: nuvio");
    snprintf(authOverridePerfil, sizeof authOverridePerfil, "%s",
             perfilCab ? perfilCab : "");
  } else {
    temAuthOverride = 0;
    authOverrideAut[0] = '\0';
    authOverrideVia[0] = '\0';
    authOverridePerfil[0] = '\0';
  }
  pthread_mutex_unlock(&trava);
}

void farolcompor_relogio(long long (*s)(void)) {
  pthread_mutex_lock(&trava);
  relFn = s;
  pthread_mutex_unlock(&trava);
}

void farolcompor_pasta(const char *dir) {
  pthread_mutex_lock(&trava);
  if (dir && dir[0]) snprintf(pastaOverride, sizeof pastaOverride, "%s", dir);
  else pastaOverride[0] = '\0';
  pthread_mutex_unlock(&trava);
}

void farolcompor_teste_home_visivel(int (*v)(void)) {
  pthread_mutex_lock(&trava);
  visFn = v;
  pthread_mutex_unlock(&trava);
}

void farolcompor_teste_zera(void) {
  farolcompor_encerrar();
  pthread_mutex_lock(&trava);
  consentiuCfg = 0;
  allowlistFn = NULL;
  allowlistUser = NULL;
  temUrlOverride = 0;
  urlOverride[0] = '\0';
  temAuthOverride = 0;
  authOverrideAut[0] = '\0';
  authOverrideVia[0] = '\0';
  authOverridePerfil[0] = '\0';
  pastaOverride[0] = '\0';
  relFn = NULL;
  visFn = NULL;
  memset(&homeDiaMem, 0, sizeof homeDiaMem);
  homeGeracao = 0;
  pthread_mutex_unlock(&trava);
}
