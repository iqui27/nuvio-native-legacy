// EPG: grade XMLTV externa para os canais ao vivo. Ver epg.h para o porque.
//
// FLUXO (desde o plano do guia de TV, M4):
//   epg_iniciar()     -> fio le os .gz do epgshare01 (cache de 12 h) e as grades
//                        extras (provedor, url-tvg, XMLTV por URL) EM FLUXO
//                        (xmltv.c): nenhum XML inteiro na memoria, so os canais
//                        do filtro e a janela de tempo ficam. Extras vao para um
//                        cache binario por fonte.
//   epg_passo()       -> no fio de desenho: publica a grade pronta e pede
//                        re-carga quando o cache passou de 12 h.
//   epg_casar(...)    -> cascata: id -> nome -> apelidos/regras (M5).
//   epg_agora/proximo -> programa no ar e os seguintes.
//   epg_janela/faixa  -> quanto a grade cobre e os programas de um intervalo.
//
// QUANTO A FONTE DA (MEDIDO em 2026-09-18 nos cinco XML do epgshare01): cada
// arquivo cobre de 3,3 a 3,8 dias A FRENTE do download e de 0,3 a 1,8 dias para
// tras. 801 canais e 72257 programas brutos nos cinco arquivos.
//
// O CASAMENTO POR NOME e o coracao deste arquivo. O addon chama o canal de um
// jeito ("RecordTV Paulista"), a grade de outro ("Record TV", "RECORD"):
//   1. chave normalizada exata (acentos fora, minusculas, so alfanumericos;
//      sufixos de qualidade "hd/4k/sd" e as palavras "canal/tv/channel"
//      descartadas na variante curta) — por indice ordenado, nao varredura;
//   2. apelidos conhecidos (tabela ALIAS abaixo);
//   3. a chave da grade mais comprida que seja PREFIXO da do canal;
//   4. a chave do canal e prefixo de UMA SO chave da grade;
//   5. chave comprida da grade inteira dentro do nome;
//   6. afiliada regional pelo primeiro token.
// O que sobra sem casamento simplesmente nao tem grade real.
#include "epg.h"
#include "perfisessao.h"
#include "iptvid.h"
#include <stdatomic.h>
#include "xmltv.h"
#include "rede.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <zlib.h>
#include <strings.h>
#include <time.h>

static unsigned long agoraMs(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned long)ts.tv_sec * 1000UL + (unsigned long)(ts.tv_nsec / 1000000L);
}

// AS FONTES DO epgshare01, POR PAIS (#158). Ate a 1.5.3 eram cinco fixas —
// BR1/BR2 (o FrostView), PT1, MX1 e AR1 — e a pessoa do #158, com canais
// romenos, tinha 797 canais de grade e nenhum que fosse dela. O epgshare01
// publica um arquivo (ou mais) por pais: epg_ripper_<PAIS><N>.xml.gz, 195
// arquivos na listagem de 29/09/2026. A tabela abaixo e o subconjunto que cabe
// numa TV: os de ate ~5 MB de gzip. US2 (6,5 MB), PL1 (8,5 MB) e o
// ALL_SOURCES1 (194 MB) ficam de fora pelo mesmo motivo que US/UK ficavam —
// o XML aberto passa de 60 MB. MEDIDO em 29/09: RO1 = 1,68 MB de gzip,
// 21 MB de XML, 370 canais, 45692 programas; RO2 = 13 KB, 148 canais.
//
// QUEM ESCOLHE e guia.c (epg_paises_definir): o ajuste de Ajustes > Conteudo,
// ou, no automatico, o idioma do app e dos metadados mais o prefixo de pais
// dos canais do Xtream ("RO: Pro TV", "|RO| Antena 1"). Sem escolha nenhuma,
// as cinco de sempre — e o que quem nunca mexeu continua vendo.
typedef struct { const char *pais; const char *arquivos; } EpgPais;
static const EpgPais PAISES[] = {
  { "AL", "AL1" }, { "AR", "AR1" }, { "AT", "AT1" }, { "AU", "AU1" },
  { "BA", "BA1" }, { "BE", "BE2" }, { "BG", "BG1" }, { "BR", "BR1 BR2" },
  { "CA", "CA2" }, { "CH", "CH1" }, { "CL", "CL1" }, { "CO", "CO1" },
  { "CY", "CY1" }, { "CZ", "CZ1" }, { "DE", "DE1" }, { "DK", "DK1" },
  { "ES", "ES1" }, { "FI", "FI1" }, { "FR", "FR1" }, { "GR", "GR1" },
  { "HR", "HR1" }, { "HU", "HU1" }, { "IE", "IE1" }, { "IL", "IL1" },
  { "IT", "IT1" }, { "LT", "LT1" }, { "LV", "LV1" }, { "MT", "MT1" },
  { "MX", "MX1" }, { "NL", "NL1" }, { "NO", "NO1" }, { "NZ", "NZ1" },
  { "PE", "PE1" }, { "PT", "PT1" }, { "RO", "RO1 RO2" }, { "RS", "RS1" },
  { "SE", "SE1" }, { "SK", "SK1" }, { "TR", "TR1 TR3" }, { "UK", "UK1" },
  { "UY", "UY1" },
};
#define EPG_N_PAISES ((int)(sizeof PAISES / sizeof PAISES[0]))
// Teto de arquivos numa carga: cada um e um XML inteiro em memoria durante o
// processamento (um de cada vez), mas a grade publicada soma todos.
#define EPG_MAX_FONTES 8
static const char *PADRAO_ARQ[] = { "BR1", "BR2", "PT1", "MX1", "AR1" };
// Arquivos ativos ("RO1"...). Escritos sob `trava`; o fio copia no comeco.
static char fontesArq[EPG_MAX_FONTES][8];
static int  nFontesArq = -1;           // -1 = nunca definido: o padrao
static char paisesAtivos[64];
static volatile int fontesMudaram;

// URL e nomes de cache de um arquivo. Os nomes das cinco antigas sao os
// mesmos de antes ("epg-br1.xml.gz"): quem atualiza nao baixa tudo de novo.
static void nomesFonte(const char *arq, char *url, size_t nu, char *xml, char *gz,
                       char *ts, size_t nn) {
  char m[8]; size_t i;
  for (i = 0; arq[i] && i < sizeof m - 1; i++)
    m[i] = (char)((arq[i] >= 'A' && arq[i] <= 'Z') ? arq[i] + 32 : arq[i]);
  m[i] = 0;
  if (url) snprintf(url, nu, "https://epgshare01.online/epgshare01/epg_ripper_%s.xml.gz", arq);
  if (xml) snprintf(xml, nn, "epg-%s.xml", m);
  if (gz)  snprintf(gz,  nn, "epg-%s.xml.gz", m);
  if (ts)  snprintf(ts,  nn, "epg-%s.ts", m);
}

// --- tetos por plataforma ------------------------------------------------------------
// O teto agora e de MEMORIA RETIDA, nao de tamanho de arquivo (M4). Evento de 16
// B (int32: o epoch cabe ate 2038) + ~20 B de titulo na arena: 600 mil
// programas ~ 22 MB na LG/Android, 400 mil no .tpk, 150 mil no .wgt (heap WASM).
#if defined(__EMSCRIPTEN__)
#define EPG_MAX_CANAL 6000
#define EPG_EVS_MAX   150000
#define EPG_ARENA_MAX (6L << 20)
#define EPG_REDE_MAX  (48LL << 20)
#elif defined(NV_TPK)
#define EPG_MAX_CANAL 16000
#define EPG_EVS_MAX   400000
#define EPG_ARENA_MAX (16L << 20)
#define EPG_REDE_MAX  (300LL << 20)
#else
#define EPG_MAX_CANAL 24000
#define EPG_EVS_MAX   600000
#define EPG_ARENA_MAX (24L << 20)
#define EPG_REDE_MAX  (300LL << 20)
#endif
#define EPG_EVS_INI   32768
#define EPG_ARENA_INI (1L << 20)
#define EPG_MAX_CHAVES 6
// A grade fala ~3,5 dias para a frente; renovar duas vezes por dia cobre a
// virada sem baixar ~1,4 MB a cada abertura do app.
#define EPG_CACHE_SEG (12 * 3600)
// Evento que terminou ha mais tempo que isto e descartado na carga (48 h e um
// teto para cache velho), e o que comeca depois de 7 dias tambem: grade de
// provedor com um mes inteiro nao cabe numa TV e ninguem rola tanto.
#define EPG_JANELA_PASSADO (48 * 3600)
#define EPG_JANELA_FUTURO  (7 * 86400)
// Prazo da grade por URL: 5 min no total, e o vigia da rede corta corpo parado.
#define EPG_PRAZO_FLUXO_S 300

// Canal da grade. Os textos ficam na arena de CHAVES (`k`), por deslocamento:
// a arena cresce por realloc durante a carga.
typedef struct {
  long id, nome, icone;               // icone: <icon src> (0 = sem)
  long chaves[EPG_MAX_CHAVES];
  int  nChaves;
  int  fonte;                         // 0 = epgshare01; 1.. = extras[fonte-1]
  long evIni, evN;
} EpgCanal;

// 16 bytes em toda plataforma. `tit` e deslocamento na arena de titulos.
typedef struct { int canal; int ini, fim; unsigned tit; } EpgEv;

// Indice de chave normalizada -> canal, para o casamento exato (M5): ordenado
// por chave, busca binaria. Montado em wFechar.
typedef struct { long chave; int canal; } EpgIdx;

typedef struct {
  EpgCanal *canais; int nCanais, capCanais;
  int      *hash;  int capHash;        // (fonte, id sem caixa) -> canal; -1 vazio
  EpgEv   *evs;    int nEvs, capEvs;
  char    *arena;  long arenaN, arenaCap;   // titulos
  char    *k;      long kN, kCap;           // ids, nomes e chaves
  EpgIdx  *idx;    int nIdx;
  int      descartados;
} EpgGrade;

static EpgGrade W;                     // construcao (fio / teste)
static EpgGrade P;                     // publicada (fio de desenho)

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int      estado = EPG_PARADO;
static time_t   carregadaEm;
static int      fioVivo;
static atomic_int pendPronto;
static int      pendOk;
static int      perfilGrade = 1, reiniciarPend;
static struct {
  NvPerfilSessao sessao;
  volatile int cancelada;
  int perfil, memoriaBaixa, soProvedores, forcarRede;
  time_t agora;
} carga;
static int epgMemoriaBaixa, soProvedores, forcarRede;
static unsigned epgRevisao;

void epg_memoria_definir(int modo, long ramMB) {
  if (modo < EPG_MEMORIA_AUTO || modo > EPG_MEMORIA_COMPLETA) modo = EPG_MEMORIA_AUTO;
  int baixa = modo == EPG_MEMORIA_BAIXA ||
              (modo == EPG_MEMORIA_AUTO && (ramMB <= 0 || ramMB <= 2048));
#ifdef __EMSCRIPTEN__
  if (modo == EPG_MEMORIA_AUTO) baixa = 1;
#endif
  pthread_mutex_lock(&trava);
  if (epgMemoriaBaixa != baixa) {
    epgMemoriaBaixa = baixa; fontesMudaram = 1;
    if (fioVivo) { carga.cancelada = 1; reiniciarPend = 1; }
  }
  pthread_mutex_unlock(&trava);
}

static int aceitaHorario(long long ini, long long fim, time_t agora) {
  if (carga.memoriaBaixa) return (fim ? fim : ini + 1800) > (long long)agora &&
                                      ini < (long long)agora + 24 * 3600;
  return (fim ? fim : ini + 1800) >= (long long)agora - EPG_JANELA_PASSADO &&
         ini <= (long long)agora + EPG_JANELA_FUTURO;
}

static int cargaAtual(void) {
  return !carga.cancelada && nv_perfil_sessao_atual(carga.sessao);
}

static void gradeSoltar(EpgGrade *g) {
  free(g->canais); free(g->hash); free(g->evs); free(g->arena); free(g->k); free(g->idx);
  memset(g, 0, sizeof *g);
}
#define KS(g, off) ((g)->k + (off))

// Chave da fonte de cada indice de `fonte` dos canais (0 = epgshare01 = "").
static char chaveFonteW[EPG_MAX_EXTRAS + 2][24], chaveFonteP[EPG_MAX_EXTRAS + 2][24];
static int nChaveFonteW, nChaveFonteP;
static char hashFonteW[EPG_MAX_EXTRAS + 2][17], hashFonteP[EPG_MAX_EXTRAS + 2][17];
// --- normalizacao de nomes ---------------------------------------------------
// Desfaz o UTF-8: acentuadas viram a letra de base (á->a, ç->c); tudo que nao
// e alfanumerico separa palavra. `curta` tambem descarta as palavras que nao
// identificam o canal ("canal", "tv", "channel", "rede") — "Canal Sony" casa
// com "SONY", "TV Brasil" com "TV BRASIL".
static int letraBase(unsigned cp) {
  if (cp >= 0xC0 && cp <= 0xC5) return 'a';
  if (cp == 0xC7) return 'c';
  if (cp >= 0xC8 && cp <= 0xCB) return 'e';
  if (cp >= 0xCC && cp <= 0xCF) return 'i';
  if (cp == 0xD1) return 'n';
  if ((cp >= 0xD2 && cp <= 0xD6) || cp == 0xD8) return 'o';
  if (cp >= 0xD9 && cp <= 0xDC) return 'u';
  if (cp == 0xDD) return 'y';
  if (cp >= 0xE0 && cp <= 0xE5) return 'a';
  if (cp == 0xE7) return 'c';
  if (cp >= 0xE8 && cp <= 0xEB) return 'e';
  if (cp >= 0xEC && cp <= 0xEF) return 'i';
  if (cp == 0xF1) return 'n';
  if ((cp >= 0xF2 && cp <= 0xF6) || cp == 0xF8) return 'o';
  if (cp >= 0xF9 && cp <= 0xFC) return 'u';
  if (cp == 0xFD || cp == 0xFF) return 'y';
  if (cp == 0xB2) return '2';
  if (cp == 0xB3) return '3';            // ³ (ids "HD.³.br" do epgshare01)
  // Latin estendido A (š ž ł…): faixas de pares maiuscula/minuscula com a
  // mesma letra de base. O que nao esta aqui vira separador, nunca erro.
  // ROMENO COM VIRGULA (#158): Ș ș Ț ț sao U+0218..U+021B, fora do Latin
  // estendido A. Viravam separador — "TVR Timișoara" (id do RO1) saia como
  // "tvrtimi" + "oara". As variantes com cedilha (Ş ş Ţ ţ) caem na faixa abaixo.
  if (cp == 0x218 || cp == 0x219) return 's';
  if (cp == 0x21A || cp == 0x21B) return 't';
  // Letras modificadoras dos nomes de painel ("PRO TV ᴴᴰ", "ᶠᴴᴰ", "ᴿᴬᵂ"):
  // viram a letra comum, e o token ("hd", "fhd", "raw") cai como qualidade.
  if (cp >= 0x1D2C && cp <= 0x1D42) {
    static const char M[] = "a?b?de?ghijklmn?o?prtuw";
    char c = M[cp - 0x1D2C];
    return c == '?' ? 0 : c;
  }
  if (cp == 0x1DA0) return 'f';
  if (cp == 0x2C7D) return 'v';
  if (cp >= 0x100 && cp <= 0x17F) {
    if (cp <= 0x105) return 'a';
    if (cp <= 0x10D) return 'c';
    if (cp <= 0x111) return 'd';
    if (cp <= 0x11B) return 'e';
    if (cp <= 0x123) return 'g';
    if (cp <= 0x127) return 'h';
    if (cp <= 0x131) return 'i';
    if (cp <= 0x135) return cp <= 0x133 ? 'i' : 'j';   // Ĳĳ, Ĵĵ
    if (cp <= 0x138) return 'k';
    if (cp <= 0x142) return 'l';
    if (cp <= 0x14B) return 'n';
    if (cp <= 0x153) return 'o';                       // Œœ viram 'o'
    if (cp <= 0x159) return 'r';
    if (cp <= 0x161) return 's';
    if (cp <= 0x167) return 't';
    if (cp <= 0x173) return 'u';
    if (cp <= 0x175) return 'w';
    if (cp <= 0x178) return 'y';
    if (cp <= 0x17E) return 'z';
    return 's';                                        // ſ
  }
  return 0;
}

static int tokenInutil(const char *t, int n, int curta) {
  // Qualidade e codec: nomes de painel IPTV trazem isto em todo canal ("PRO TV
  // FHD", "Antena 1 HEVC", "Digi Sport 1 RAW 50FPS") e a grade nao.
  static const char *qual[] = { "hd","fhd","uhd","4k","8k","sd","hdtv","fullhd",
                                "hevc","h265","h264","raw","50fps","60fps","25fps",
                                "1080p","1080i","720p","2160p","backup","hq","lq" };
  static const char *marc[] = { "canal","channel","tv","rede","and","e" };
  int i;
  for (i = 0; i < (int)(sizeof qual / sizeof qual[0]); i++)
    if ((int)strlen(qual[i]) == n && !strncmp(qual[i], t, (size_t)n)) return 1;
  if (curta)
    for (i = 0; i < 6; i++)
      if ((int)strlen(marc[i]) == n && !strncmp(marc[i], t, (size_t)n)) return 1;
  return 0;
}

// Escreve a chave em `dst`. Devolve o comprimento. Se `prim` nao e NULL,
// recebe o PRIMEIRO token aceito — e a forma de reconhecer afiliada
// regional: "SBT RJ" abre com "sbt", a chave inteira da grade da rede-mae.
// PREFIXO DE PAIS OU DE PACOTE no comeco do nome (#158): "RO: Pro TV",
// "RO | Antena 1", "|RO| Kanal D", "[RO] Digi 24", "VIP: HBO". A grade nao tem
// isso, e "ro" colado na frente ("roprotv") nao casa com nada. Reconhece de 2
// a 4 letras seguidas de ':' ou '|' ou ']' (com espacos opcionais), e o
// par "XX - " so para os codigos de pais conhecidos — "TV - Record" nao pode
// perder o "TV". Devolve onde o nome comeca; `pais` recebe o prefixo em
// maiusculas quando tem 2 letras (dica de pais para o guia), ou "".
const char *epg_sem_prefixo(const char *s, char pais[4]) {
  const char *p = s, *q;
  int n = 0, abre = 0;
  if (pais) pais[0] = 0;
  if (!s) return s;
  while (*p == ' ') p++;
  if (*p == '|' || *p == '[' || *p == '(') { abre = 1; p++; while (*p == ' ') p++; }
  for (q = p; ((*q >= 'A' && *q <= 'Z') || (*q >= 'a' && *q <= 'z')) && n < 5; q++) n++;
  if (n < 2 || n > 4) return s;
  { const char *r = q;
    while (*r == ' ') r++;
    if (*r == ':' || *r == '|' || *r == ']' || *r == ')' ||
        ((*r == '-' || !strncmp(r, "\xe2\x80\xa2", 3)) && !abre && n == 2)) {
      if (*r == '-' || *r == ':' || *r == '|' || *r == ']' || *r == ')') {
        // "XX - " sem ser pais conhecido: nao e prefixo.
        if (*r == '-' && n == 2) {
          char c[3] = { (char)(p[0] & ~32), (char)(p[1] & ~32), 0 };
          if (!epg_pais_existe(c)) return s;
        }
        r++;
      } else r += 3;
      // "|RO|" tem a barra de fechamento colada.
      while (*r == ' ' || *r == '|' || *r == ':' || *r == '-') r++;
      if (!*r) return s;                       // o nome inteiro era "RO:"
      if (pais && n == 2) { pais[0] = (char)(p[0] & ~32); pais[1] = (char)(p[1] & ~32); pais[2] = 0; }
      return r;
    } }
  return s;
}

static int normChave(const char *s, char *dst, int cap, int curta,
                     char *prim, int pcap) {
  char tok[48]; int tn = 0, n = 0, primOk = 0;
  unsigned cp; const unsigned char *p;
  s = epg_sem_prefixo(s, NULL);
  p = (const unsigned char *)s;
  if (prim && pcap > 0) prim[0] = 0;
  #define FLUSHTOK() do { \
    if (tn && !tokenInutil(tok, tn, curta)) { \
      if (n + tn < cap - 1) { memcpy(dst + n, tok, (size_t)tn); n += tn; } \
      if (prim && !primOk && tn < pcap) \
        { memcpy(prim, tok, (size_t)tn); prim[tn] = 0; primOk = 1; } } \
    tn = 0; } while (0)
  while (*p) {
    cp = 0;
    if (*p < 0x80) cp = *p++;
    else if ((*p & 0xE0) == 0xC0 && p[1]) { cp = ((*p & 0x1F) << 6) | (p[1] & 0x3F); p += 2; }
    else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) { cp = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; }
    else { p++; continue; }
    if (cp >= 'A' && cp <= 'Z') cp += 32;
    if ((cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9')) {
      if (tn < (int)sizeof tok - 1) tok[tn++] = (char)cp;
    } else {
      int b = letraBase(cp);
      if (b) { if (tn < (int)sizeof tok - 1) tok[tn++] = (char)b; }
      else FLUSHTOK();
    }
  }
  FLUSHTOK();
  #undef FLUSHTOK
  dst[n] = 0;
  return n;
}

// --- apelidos ------------------------------------------------------------------
// Nome do addon (ja normalizado) -> chave da grade. Cada linha existe porque
// um canal real do FrostView ficou sem par pelas regras genericas. Manter
// curto: a tendencia natural da lista e crescer a cada canal novo sem grade.
static const char *ALIAS[][2] = {
  { "h2",            "history2"             },
  { "premiere1",     "premiereclubes"       },   // Premiere 1 = Premiere FC/Clubes
  { "discoveryhh",   "discoveryhomehealth"  },
  { "universaltv",   "universal"            },
  { "sonychannel",   "sony"                 },
  { "cnbbrasil",     "cnbc"                 },
  { "historychannel","history"              },
  { "uniao",         "recordtv"             },   // TV Uniao (Record, Fortaleza)
  { "uniaofortaleza","recordtv"             },
};

// --- grade em construcao (W) -------------------------------------------------------
static int wAbrir(EpgGrade *g) {
  memset(g, 0, sizeof *g);
  g->capEvs = EPG_EVS_INI;
  g->evs = malloc(sizeof(EpgEv) * (size_t)g->capEvs);
  g->arenaCap = EPG_ARENA_INI;
  g->arena = malloc((size_t)g->arenaCap);
  g->kCap = 256 * 1024;
  g->k = malloc((size_t)g->kCap);
  if (!g->evs || !g->arena || !g->k) { gradeSoltar(g); return 0; }
  g->k[0] = 0; g->kN = 1;              // deslocamento 0 = ""
  return 1;
}

// Copia para a arena de chaves; 0 ("") quando nao coube (nunca falha alto: o
// canal fica sem aquele texto).
static long kDup(EpgGrade *g, const char *s) {
  long n = (long)strlen(s) + 1, d;
  if (n == 1) return 0;
  if (g->kN + n > g->kCap) {
    long cap = g->kCap * 2;
    char *no;
    while (cap < g->kN + n) cap *= 2;
    no = realloc(g->k, (size_t)cap);
    if (!no) return 0;
    g->k = no; g->kCap = cap;
  }
  d = g->kN;
  memcpy(g->k + d, s, (size_t)n); g->kN += n;
  return d;
}

static unsigned long hId(int fonte, const char *id) {
  unsigned long h = 2166136261UL ^ ((unsigned long)fonte * 16777619UL);
  for (; *id; id++) {
    unsigned char c = (unsigned char)*id;
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
    h ^= c; h *= 16777619UL; h &= 0xffffffffUL;
  }
  return h;
}

static int wHashRefazer(EpgGrade *g, int cap) {
  int *no = malloc(sizeof(int) * (size_t)cap), i;
  if (!no) return 0;
  for (i = 0; i < cap; i++) no[i] = -1;
  for (i = 0; i < g->nCanais; i++) {
    unsigned long h = hId(g->canais[i].fonte, KS(g, g->canais[i].id)) & (unsigned long)(cap - 1);
    while (no[h] >= 0) h = (h + 1) & (unsigned long)(cap - 1);
    no[h] = i;
  }
  free(g->hash); g->hash = no; g->capHash = cap;
  return 1;
}

static int wCanalPorId(const EpgGrade *g, int fonte, const char *id) {
  unsigned long h;
  if (!g->capHash || !id || !id[0]) return -1;
  h = hId(fonte, id) & (unsigned long)(g->capHash - 1);
  while (g->hash[h] >= 0) {
    const EpgCanal *c = &g->canais[g->hash[h]];
    if (c->fonte == fonte && !strcasecmp(KS(g, c->id), id)) return g->hash[h];
    h = (h + 1) & (unsigned long)(g->capHash - 1);
  }
  return -1;
}

static void wAddChave(EpgGrade *g, int i, const char *nome);
static void wChavePorId(EpgGrade *g, int i);

static int wNovoCanal(EpgGrade *g, int fonte, const char *id) {
  EpgCanal *c;
  int i;
  if (g->nCanais >= EPG_MAX_CANAL) return -1;
  if (g->nCanais >= g->capCanais) {
    int cap = g->capCanais ? g->capCanais * 2 : 1024;
    EpgCanal *no;
    if (cap > EPG_MAX_CANAL) cap = EPG_MAX_CANAL;
    no = realloc(g->canais, sizeof *no * (size_t)cap);
    if (!no) return -1;
    g->canais = no; g->capCanais = cap;
  }
  if ((g->nCanais + 1) * 2 > g->capHash && !wHashRefazer(g, g->capHash ? g->capHash * 2 : 2048)) return -1;
  i = g->nCanais++;
  c = &g->canais[i];
  memset(c, 0, sizeof *c);
  c->fonte = fonte;
  c->id = kDup(g, id);
  { unsigned long h = hId(fonte, id) & (unsigned long)(g->capHash - 1);
    while (g->hash[h] >= 0) h = (h + 1) & (unsigned long)(g->capHash - 1);
    g->hash[h] = i; }
  wChavePorId(g, i);
  return i;
}

static void wAddChave(EpgGrade *g, int i, const char *nome) {
  char k[96]; int j, curta;
  for (curta = 0; curta < 2; curta++) {
    if (!normChave(nome, k, sizeof k, curta, NULL, 0) || !k[0]) continue;
    for (j = 0; j < g->canais[i].nChaves; j++)
      if (!strcmp(k, KS(g, g->canais[i].chaves[j]))) break;
    if (j < g->canais[i].nChaves || g->canais[i].nChaves >= EPG_MAX_CHAVES) continue;
    { long o = kDup(g, k);
      if (o) g->canais[i].chaves[g->canais[i].nChaves++] = o; }
  }
}

// O id "Globo.RJ.br" tambem vira nome: onde o display-name falta ou difere,
// a forma do id e a unica pista. Os ids do epgshare01 vem em dois moldes:
// "Sao.Paulo/SP..Cartoonito.br" (cidade/UF..canal) e "SBT.br" (canal direto).
// O nome do canal e o que vem depois do ".."; sem ele a ultima "/" devolvia
// "SP..cartoonito" e a chave saia "spcartoonito".
static void idParaNome(const char *id, char *tmp, size_t n) {
  char buf[160];
  const char *s;
  size_t k;
  snprintf(buf, sizeof buf, "%s", id);
  { char *b = strrchr(buf, '.');
    if (b && (b - buf) > 2 && b[1] && b[2] && !b[3]) *b = 0; }   // ".br" do fim
  { char *dd = strstr(buf, "..");
    s = dd ? dd + 2 : buf; }
  { const char *b = strrchr(s, '/');
    if (b) s = b + 1; }
  for (k = 0; *s && k < n - 1; s++) {
    if (*s == '(') {
      while (*s && *s != ')') s++;
      if (!*s) break;
      continue; }
    tmp[k++] = (*s == '.' || *s == '_') ? ' ' : *s;
  }
  tmp[k] = 0;
}
static void wChavePorId(EpgGrade *g, int i) {
  char tmp[160];
  idParaNome(KS(g, g->canais[i].id), tmp, sizeof tmp);
  wAddChave(g, i, tmp);
}

static int wReservarEv(EpgGrade *g) {
  if (g->nEvs < g->capEvs) return 1;
  if (g->capEvs >= EPG_EVS_MAX) return 0;
  { int cap = g->capEvs * 2 > EPG_EVS_MAX ? EPG_EVS_MAX : g->capEvs * 2;
    EpgEv *no = realloc(g->evs, sizeof(EpgEv) * (size_t)cap);
    if (!no) return 0;
    g->evs = no; g->capEvs = cap; }
  return 1;
}

static long wTitulo(EpgGrade *g, const char *s) {
  long n = (long)strlen(s) + 1, d;
  if (g->arenaN + n > g->arenaCap) {
    long cap = g->arenaCap;
    while (cap < g->arenaN + n && cap < EPG_ARENA_MAX) cap *= 2;
    if (cap > EPG_ARENA_MAX) cap = EPG_ARENA_MAX;
    if (cap < g->arenaN + n) return -1;
    { char *no = realloc(g->arena, (size_t)cap);
      if (!no) return -1;
      g->arena = no; g->arenaCap = cap; }
  }
  d = g->arenaN;
  memcpy(g->arena + d, s, (size_t)n); g->arenaN += n;
  return d;
}

static int wAddEv(EpgGrade *g, int canal, long long ini, long long fim, const char *titulo) {
  long copia;
  if (!wReservarEv(g) || (copia = wTitulo(g, titulo)) < 0) { g->descartados++; return 0; }
  g->evs[g->nEvs].canal = canal;
  g->evs[g->nEvs].ini = (int)ini;
  g->evs[g->nEvs].fim = (int)fim;
  g->evs[g->nEvs].tit = (unsigned)copia;
  g->nEvs++;
  return 1;
}

static int wCmpEv(const void *a, const void *b) {
  const EpgEv *x = a, *y = b;
  if (x->canal != y->canal) return x->canal - y->canal;
  return (x->ini > y->ini) - (x->ini < y->ini);
}
static const EpgGrade *gOrdem;
static int cmpIdx(const void *a, const void *b) {
  const EpgIdx *x = a, *y = b;
  int c = strcmp(gOrdem->k + x->chave, gOrdem->k + y->chave);
  return c ? c : x->canal - y->canal;
}

// Fecha a grade construida: ordena os eventos, tira as duplicatas de BR1/BR2
// (mesmo canal, mesmo inicio e fim), da fim ao programa que veio sem `stop` (o
// inicio do seguinte), mede a janela de cada canal e monta o indice de chaves.
static void wFechar(EpgGrade *g) {
  int i, w; long ini;
  qsort(g->evs, (size_t)g->nEvs, sizeof(EpgEv), wCmpEv);
  for (i = 0, w = 0; i < g->nEvs; i++)
    if (w == 0 || g->evs[i].canal != g->evs[w-1].canal ||
        g->evs[i].ini != g->evs[w-1].ini || g->evs[i].fim != g->evs[w-1].fim)
      g->evs[w++] = g->evs[i];
  g->nEvs = w;
  for (i = 0; i < g->nEvs; i++)
    if (g->evs[i].fim <= g->evs[i].ini) {
      int prox = (i + 1 < g->nEvs && g->evs[i + 1].canal == g->evs[i].canal) ? g->evs[i + 1].ini : 0;
      g->evs[i].fim = prox > g->evs[i].ini ? prox : g->evs[i].ini + 1800;
    }
  for (i = 0, ini = 0; i < g->nCanais; i++) {
    g->canais[i].evIni = ini;
    while (ini < g->nEvs && g->evs[ini].canal == i) ini++;
    g->canais[i].evN = ini - g->canais[i].evIni;
  }
  { int n = 0, j;
    for (i = 0; i < g->nCanais; i++) n += g->canais[i].nChaves;
    free(g->idx); g->idx = n ? malloc(sizeof(EpgIdx) * (size_t)n) : NULL; g->nIdx = 0;
    if (g->idx) {
      for (i = 0; i < g->nCanais; i++)
        for (j = 0; j < g->canais[i].nChaves; j++) {
          g->idx[g->nIdx].chave = g->canais[i].chaves[j];
          g->idx[g->nIdx].canal = i;
          g->nIdx++;
        }
      gOrdem = g;
      qsort(g->idx, (size_t)g->nIdx, sizeof(EpgIdx), cmpIdx);
    } }
}

// --- o que entra (filtro) -------------------------------------------------------------
// Vetores ordenados de strings (ids em minusculas, nomes ja normalizados): o
// filtro de uma grade extra. NULL/0 = tudo entra (epgshare01).
typedef struct {
  EpgGrade *g; int fonte;
  char **ids; int nIds;
  char **nomes; int nNomes;
  time_t agora;
  int canais, programas, foraJanela;
} EpgProc;

static int cmpStr(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }
static int temStr(char **v, int n, const char *s) {
  return n > 0 && bsearch(&s, v, (size_t)n, sizeof *v, cmpStr) != NULL;
}
static void minusc(const char *s, char *d, size_t n) {
  size_t i;
  for (i = 0; s[i] && i < n - 1; i++) d[i] = (char)((s[i] >= 'A' && s[i] <= 'Z') ? s[i] + 32 : s[i]);
  d[i] = 0;
}

static int filtroAceita(const EpgProc *p, const char *id, const char *const *nomes, int nn) {
  char low[160], k[96], tmp[160];
  int i, curta;
  if (!p->nIds && !p->nNomes) return 1;
  minusc(id, low, sizeof low);
  if (temStr(p->ids, p->nIds, low)) return 1;
  if (!p->nNomes) return 0;
  for (i = 0; i <= nn; i++) {
    const char *nm = i < nn ? nomes[i] : (idParaNome(id, tmp, sizeof tmp), tmp);
    for (curta = 0; curta < 2; curta++)
      if (normChave(nm, k, sizeof k, curta, NULL, 0) && temStr(p->nomes, p->nNomes, k)) return 1;
  }
  return 0;
}

static void cbCanal(const char *id, const char *const *nomes, int nn, void *u) {
  EpgProc *p = u;
  int i, j;
  if (!filtroAceita(p, id, nomes, nn)) return;
  i = wCanalPorId(p->g, p->fonte, id);
  if (i < 0) { i = wNovoCanal(p->g, p->fonte, id); if (i < 0) return; p->canais++; }
  for (j = 0; j < nn; j++) {
    if (!p->g->canais[i].nome) p->g->canais[i].nome = kDup(p->g, nomes[j]);
    wAddChave(p->g, i, nomes[j]);
  }
}
static void cbIcone(const char *id, const char *src, void *u) {
  EpgProc *p = u;
  int i = wCanalPorId(p->g, p->fonte, id);
  if (i >= 0 && !p->g->canais[i].icone) p->g->canais[i].icone = kDup(p->g, src);
}

static void cbPrograma(const char *canal, const char *ini, const char *fim,
                       const char *titulo, void *u) {
  EpgProc *p = u;
  long long a, b;
  int ci = wCanalPorId(p->g, p->fonte, canal);
  if (ci < 0 || !titulo[0]) return;
  a = xmltv_tempo(ini);
  b = fim[0] ? xmltv_tempo(fim) : 0;
  if (!a || (b && b <= a)) return;
  if (!aceitaHorario(a, b, p->agora)) { p->foraJanela++; return; }
  if (wAddEv(p->g, ci, a, b, titulo)) p->programas++;
}

// Alimenta uma grade inteira que ja esta na memoria (gz ou XML), em pedacos.
static int processarBuf(EpgProc *p, const char *b, long n) {
  Xmltv x;
  long i;
  memset(&x, 0, sizeof x);
  x.canal = cbCanal; x.icone = cbIcone; x.programa = cbPrograma; x.u = p;
  if (!xmltv_iniciar(&x)) return 0;
  for (i = 0; i < n; i += 65536)
    if (!xmltv_alimentar(&x, b + i, (size_t)(n - i < 65536 ? n - i : 65536))) break;
  xmltv_fim(&x);
  return x.nCanais > 0 || x.nProgramas > 0;
}
// Ver o topo de PAISES. `codigos` = "RO,BR" (paises); "" volta ao padrao.
// Pais desconhecido e ignorado. Devolve quantos ARQUIVOS ficaram ativos.
int epg_paises_definir(const char *codigos) {
  char arq[EPG_MAX_FONTES][8], ativos[64] = "";
  int n = 0, i, mudou;
  const char *p = codigos ? codigos : "";
  while (*p && n < EPG_MAX_FONTES) {
    char c[4]; int k = 0;
    while (*p == ',' || *p == ' ') p++;
    while (*p && *p != ',' && *p != ' ' && k < 3) {
      c[k++] = (char)((*p >= 'a' && *p <= 'z') ? *p - 32 : *p); p++;
    }
    c[k] = 0;
    while (*p && *p != ',' && *p != ' ') p++;
    if (!k) continue;
    if (!strcmp(c, "GB")) snprintf(c, sizeof c, "UK");
    for (i = 0; i < EPG_N_PAISES; i++)
      if (!strcmp(PAISES[i].pais, c)) {
        const char *a = PAISES[i].arquivos;
        int j, ja = 0;
        for (j = 0; ativos[j]; j += 3) if (!strncmp(ativos + j, c, 2)) ja = 1;
        if (ja) break;
        if (strlen(ativos) + 3 < sizeof ativos) {
          if (ativos[0]) strcat(ativos, ",");
          strcat(ativos, c);
        }
        while (*a && n < EPG_MAX_FONTES) {
          int m = 0;
          while (*a == ' ') a++;
          while (*a && *a != ' ' && m < 7) arq[n][m++] = *a++;
          arq[n][m] = 0;
          if (m) n++;
        }
        break;
      }
  }
  if (!n) {
    for (i = 0; i < (int)(sizeof PADRAO_ARQ / sizeof PADRAO_ARQ[0]); i++)
      snprintf(arq[n++], sizeof arq[0], "%s", PADRAO_ARQ[i]);
    snprintf(ativos, sizeof ativos, "BR,PT,MX,AR");
  }
  pthread_mutex_lock(&trava);
  mudou = n != nFontesArq;
  for (i = 0; !mudou && i < n; i++) if (strcmp(arq[i], fontesArq[i])) mudou = 1;
  if (mudou) {
    // A primeira definicao nao e "mudanca": a carga ainda nao aconteceu, ou
    // aconteceu com o padrao — e so o padrao que difere pede recarga.
    int eraPadrao = nFontesArq < 0;
    int igualPadrao = n == 5;
    for (i = 0; igualPadrao && i < 5; i++) if (strcmp(arq[i], PADRAO_ARQ[i])) igualPadrao = 0;
    for (i = 0; i < n; i++) snprintf(fontesArq[i], sizeof fontesArq[i], "%s", arq[i]);
    nFontesArq = n;
    if (!(eraPadrao && igualPadrao)) fontesMudaram = 1;
  }
  snprintf(paisesAtivos, sizeof paisesAtivos, "%s", ativos);
  pthread_mutex_unlock(&trava);
  if (mudou) { printf("[epg] paises da grade: %s (%d arquivo(s))\n", ativos, n); fflush(stdout); }
  return n;
}

const char *epg_paises_ativos(void) {
  static char c[64];
  pthread_mutex_lock(&trava);
  snprintf(c, sizeof c, "%s", nFontesArq < 0 ? "BR,PT,MX,AR" : paisesAtivos);
  pthread_mutex_unlock(&trava);
  return c;
}

int epg_pais_existe(const char *pais) {
  int i;
  if (!pais) return 0;
  for (i = 0; i < EPG_N_PAISES; i++) if (!strcasecmp(PAISES[i].pais, pais)) return 1;
  return !strcasecmp(pais, "GB");
}

// --- carga: disco e rede ---------------------------------------------------------------
static long arquivoIdade(const char *nome) {
  char *ts = dados_ler(nome);
  long velho = EPG_CACHE_SEG + 1;
  if (ts) { long t = atol(ts); if (t > 0) velho = (long)time(NULL) - t; free(ts); }
  return velho;
}

// O CACHE DO epgshare01 GUARDA O .gz COMO VEIO DA REDE (23/09/2026): no Tizen a
// pasta de dados e IDBFS e a descarga clona o arquivo inteiro no fio principal;
// gravar o XML aberto (~10x o gzip) derrubava o FPS. Abrir o gzip de novo e
// barato e agora e em fluxo (xmltv.c): o XML aberto nunca existe inteiro.
static int gravarBin(const char *nome, const char *nomeTs, const char *gz, long n) {
  char cam[640], tbuf[24]; FILE *f;
  int ok = 0;
  if (!dados_caminho(cam, sizeof cam, nome)) return 0;
  dados_fs_travar();
  f = fopen(cam, "wb");
  if (f) { ok = fwrite(gz, 1, (size_t)n, f) == (size_t)n; ok = (fclose(f) == 0) && ok; }
  dados_fs_liberar();
  if (!ok) return 0;
  snprintf(tbuf, sizeof tbuf, "%ld", (long)time(NULL));
  dados_gravar_leve(nomeTs, tbuf);
  dados_marcar_sujo(1);
  return 1;
}

static char *lerBin(const char *nome, long *n) {
  char cam[640]; FILE *f; char *b = NULL; long t;
  *n = 0;
  if (!dados_caminho(cam, sizeof cam, nome)) return NULL;
  dados_fs_travar();
  f = fopen(cam, "rb");
  if (f) {
    fseek(f, 0, SEEK_END); t = ftell(f); fseek(f, 0, SEEK_SET);
    if (t > 0 && (b = malloc((size_t)t)) != NULL) {
      if ((long)fread(b, 1, (size_t)t, f) == t) *n = t;
      else { free(b); b = NULL; }
    }
    fclose(f);
  }
  dados_fs_liberar();
  return b;
}

// O .gz de um arquivo do epgshare01: o do disco se fresco, senao da rede.
static char *obterGz(const char *arq, long *n) {
  char *gz, url[160], nXml[40], nGz[40], nTs[40];
  nomesFonte(arq, url, sizeof url, nXml, nGz, nTs, sizeof nXml);
  { static char limpou[16][8]; static int nLimpou;   // o cache antigo de XML aberto sai
    int k;
    for (k = 0; k < nLimpou && strcmp(limpou[k], arq); k++) {}
    if (k == nLimpou && nLimpou < 16) {
      snprintf(limpou[nLimpou++], sizeof limpou[0], "%.7s", arq);
      dados_apagar(nXml);
    } }
  if (!carga.forcarRede && arquivoIdade(nTs) < EPG_CACHE_SEG) {
    gz = lerBin(nGz, n);
    if (gz && *n > 2 && (unsigned char)gz[0] == 0x1f) return gz;
    free(gz);
  }
  RedeControle ctl = {0}; ctl.cancelado = &carga.cancelada;
  if (!cargaAtual()) return NULL;
  gz = rede_baixar_bin_medido_controle(url, 60, NULL, &ctl, n, NULL);
  if (!gz) {
    printf("[epg] %s: sem resposta, tentando cache\n", nXml); fflush(stdout);
    return lerBin(nGz, n);
  }
  if (*n > 2 && (unsigned char)gz[0] == 0x1f && (unsigned char)gz[1] == 0x8b) gravarBin(nGz, nTs, gz, *n);
  return gz;
}

// --- grades extras (provedor, url-tvg, XMLTV por URL) -----------------------------------
// A URL pode levar credencial: so o hash dela vai para o disco, nunca ela.
typedef struct {
  char chave[24];
  char *url;
  char **ids; int nIds;        // minusculas, ordenados
  char **nomes; int nNomes;    // normalizados, ordenados
  unsigned long hFiltro;
} EpgExtraInt;
static EpgExtraInt extras[EPG_MAX_EXTRAS];
static int nExtras;
static volatile int extraMudou;
static EpgFonteInfo infoPub[EPG_MAX_EXTRAS], infoW[EPG_MAX_EXTRAS];
static int nInfoPub, nInfoW;
// A interface antiga (#158): uma fonte "xt" com a URL e os ids do Xtream.
static char legadoUrl[1100], legadoM3uUrl[1100];
static char (*legadoIds)[64]; static int nLegadoIds;

static void extraSoltar(EpgExtraInt *e) {
  int i;
  free(e->url);
  for (i = 0; i < e->nIds; i++) free(e->ids[i]);
  for (i = 0; i < e->nNomes; i++) free(e->nomes[i]);
  free(e->ids); free(e->nomes);
  memset(e, 0, sizeof *e);
}

static unsigned long hashTexto(unsigned long h, const char *s) {
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619UL; h &= 0xffffffffUL; }
  return h ^ 0x9e;
}

static void hashUrl(const char *u, char *dst, size_t tam) {
  unsigned long long h = 1469598103934665603ULL;
  for (; *u; u++) { h ^= (unsigned char)*u; h *= 1099511628211ULL; }
  snprintf(dst, tam, "%016llx", h);
}

// Monta a copia interna (com os vetores ordenados e sem repeticao).
static void extraMontar(EpgExtraInt *e, const char *chave, const char *url,
                        const char *const *ids, int nIds, const char *const *nomes, int nNomes) {
  int i, w;
  memset(e, 0, sizeof *e);
  snprintf(e->chave, sizeof e->chave, "%s", chave ? chave : "");
  e->url = strdup(url ? url : "");
  if (nIds > 0 && (e->ids = malloc(sizeof(char *) * (size_t)nIds)) != NULL)
    for (i = 0; i < nIds; i++)
      if (ids[i] && ids[i][0]) {
        char low[160];
        minusc(ids[i], low, sizeof low);
        if ((e->ids[e->nIds] = strdup(low)) != NULL) e->nIds++;
      }
  if (nNomes > 0 && (e->nomes = malloc(sizeof(char *) * (size_t)nNomes * 2)) != NULL)
    for (i = 0; i < nNomes; i++) {
      char k[96]; int curta;
      if (!nomes[i] || !nomes[i][0]) continue;
      for (curta = 0; curta < 2; curta++)
        if (normChave(nomes[i], k, sizeof k, curta, NULL, 0) && k[0] &&
            (e->nomes[e->nNomes] = strdup(k)) != NULL) e->nNomes++;
    }
  if (e->nIds) qsort(e->ids, (size_t)e->nIds, sizeof(char *), cmpStr);
  if (e->nNomes) qsort(e->nomes, (size_t)e->nNomes, sizeof(char *), cmpStr);
  for (i = 1, w = e->nIds ? 1 : 0; i < e->nIds; i++)
    if (strcmp(e->ids[i], e->ids[w - 1])) e->ids[w++] = e->ids[i]; else free(e->ids[i]);
  if (e->nIds) e->nIds = w;
  for (i = 1, w = e->nNomes ? 1 : 0; i < e->nNomes; i++)
    if (strcmp(e->nomes[i], e->nomes[w - 1])) e->nomes[w++] = e->nomes[i]; else free(e->nomes[i]);
  if (e->nNomes) e->nNomes = w;
  e->hFiltro = 2166136261UL;
  for (i = 0; i < e->nIds; i++) e->hFiltro = hashTexto(e->hFiltro, e->ids[i]);
  for (i = 0; i < e->nNomes; i++) e->hFiltro = hashTexto(e->hFiltro, e->nomes[i]);
}

void epg_extras_definir(const EpgExtra *v, int n) {
  EpgExtraInt novo[EPG_MAX_EXTRAS];
  int i, k = 0, mudou;
  if (n > EPG_MAX_EXTRAS) n = EPG_MAX_EXTRAS;
  for (i = 0; i < n; i++)
    if (v[i].url && v[i].url[0])
      extraMontar(&novo[k++], v[i].chave, v[i].url, v[i].ids, v[i].nIds, v[i].nomes, v[i].nNomes);
  pthread_mutex_lock(&trava);
  mudou = k != nExtras;
  for (i = 0; !mudou && i < k; i++)
    if (strcmp(novo[i].chave, extras[i].chave) || strcmp(novo[i].url, extras[i].url)) mudou = 1;
  for (i = 0; i < nExtras; i++) extraSoltar(&extras[i]);
  for (i = 0; i < k; i++) extras[i] = novo[i];
  nExtras = k;
  if (mudou) {
    extraMudou = 1; epgRevisao++;
    if (fioVivo) { carga.cancelada = 1; reiniciarPend = 1; }
  }
  pthread_mutex_unlock(&trava);
}

// Os dois de antes, por cima da lista nova: "xt" e a grade do Xtream.
void epg_fonte_extra_ids(const char *const *ids, int n) {
  char (*novo)[64] = NULL;
  int i, k = 0;
  if (ids && n > 0) novo = malloc(sizeof *novo * (size_t)n);
  if (novo)
    for (i = 0; i < n; i++)
      if (ids[i] && ids[i][0]) snprintf(novo[k++], sizeof novo[0], "%s", ids[i]);
  pthread_mutex_lock(&trava);
  free(legadoIds);
  legadoIds = k ? novo : NULL;
  nLegadoIds = k;
  if (!k) free(novo);
  pthread_mutex_unlock(&trava);
}
void epg_fonte_extra(const char *url) {
  if (!url) url = "";
  pthread_mutex_lock(&trava);
  if (strcmp(url, legadoUrl)) {
    snprintf(legadoUrl, sizeof legadoUrl, "%s", url);
    extraMudou = 1;
  }
  pthread_mutex_unlock(&trava);
}

// CACHE BINARIO POR FONTE: o resultado JA FILTRADO (canais + programas), lido em
// milissegundos e sem reabrir XML. Cabecalho com o hash da URL e do filtro:
// trocou o provedor ou a lista, o cache nao vale.
#define EPGX_MAGIA 0x3245564eu   /* "NVE2" */
#define EPGX_VERSAO 2u   /* 2: <icon src> por canal */
static void nomeCacheX(const char *chave, char *d, size_t n) { snprintf(d, n, "epg-x-%s-p%d.bin", chave[0] ? chave : "geral", carga.perfil > 0 ? carga.perfil : 1); }

static int escreveStr(FILE *f, const char *s) {
  unsigned short k = (unsigned short)(strlen(s) > 65535 ? 65535 : strlen(s));
  return fwrite(&k, 2, 1, f) == 1 && (!k || fwrite(s, 1, k, f) == k);
}
static int leStr(const char **p, const char *fim, char *dst, size_t n) {
  unsigned short k;
  if (*p + 2 > fim) return 0;
  memcpy(&k, *p, 2); *p += 2;
  if (*p + k > fim) return 0;
  { size_t m = k < n - 1 ? k : n - 1; memcpy(dst, *p, m); dst[m] = 0; }
  *p += k;
  return 1;
}

static void cacheXGravar(const EpgExtraInt *e, const EpgGrade *g, int c0, int e0) {
  char nome[64], cam[640], tmp[660], hu[24];
  unsigned u[4];
  long long ts = (long long)time(NULL);
  FILE *f;
  int i, ok;
  nomeCacheX(e->chave, nome, sizeof nome);
  if (!dados_caminho(cam, sizeof cam, nome)) return;
  snprintf(tmp, sizeof tmp, "%s.tmp", cam);
  hashUrl(e->url, hu, sizeof hu);
  dados_fs_travar();
  f = fopen(tmp, "wb");
  if (!f) { dados_fs_liberar(); return; }
  u[0] = EPGX_MAGIA; u[1] = EPGX_VERSAO; u[2] = (unsigned)(g->nCanais - c0); u[3] = (unsigned)(g->nEvs - e0);
  ok = fwrite(u, sizeof u, 1, f) == 1 && fwrite(hu, 16, 1, f) == 1 &&
       fwrite(&e->hFiltro, sizeof e->hFiltro, 1, f) == 1 && fwrite(&ts, sizeof ts, 1, f) == 1;
  for (i = c0; ok && i < g->nCanais; i++)
    ok = escreveStr(f, KS(g, g->canais[i].id)) && escreveStr(f, KS(g, g->canais[i].nome)) &&
         escreveStr(f, KS(g, g->canais[i].icone));
  for (i = e0; ok && i < g->nEvs; i++) {
    int loc[3];
    loc[0] = g->evs[i].canal - c0; loc[1] = g->evs[i].ini; loc[2] = g->evs[i].fim;
    ok = fwrite(loc, sizeof loc, 1, f) == 1 && escreveStr(f, g->arena + g->evs[i].tit);
  }
  ok = (fclose(f) == 0) && ok;
  if (!ok || rename(tmp, cam) != 0) remove(tmp);
  dados_fs_liberar();
  dados_marcar_sujo(1);
}

// Le o cache da fonte para dentro de `g` com a fonte `fonte`. `fresco` = so
// aceita com menos de 12 h e o mesmo filtro; 0 aceita velho (a rede falhou).
// Devolve quantos programas entraram, -1 sem cache que sirva.
static int cacheXLer(const EpgExtraInt *e, EpgGrade *g, int fonte, int fresco, long *idade) {
  char nome[64], hu[24], huArq[17];
  unsigned u[4];
  unsigned long hf;
  long long ts;
  long n = 0;
  char *b;
  const char *p, *fim;
  int base = g->nCanais, *mapa = NULL, i, prog = 0;
  nomeCacheX(e->chave, nome, sizeof nome);
  b = lerBin(nome, &n);
  if (!b) return -1;
  p = b; fim = b + n;
  hashUrl(e->url, hu, sizeof hu);
  if (n < (long)(sizeof u + 16 + sizeof hf + sizeof ts)) { free(b); return -1; }
  memcpy(u, p, sizeof u); p += sizeof u;
  memcpy(huArq, p, 16); huArq[16] = 0; p += 16;
  memcpy(&hf, p, sizeof hf); p += sizeof hf;
  memcpy(&ts, p, sizeof ts); p += sizeof ts;
  if (u[0] != EPGX_MAGIA || u[1] != EPGX_VERSAO || strcmp(huArq, hu)) { free(b); return -1; }
  *idade = (long)((long long)time(NULL) - ts);
  if (fresco && (carga.forcarRede || hf != e->hFiltro || *idade >= EPG_CACHE_SEG || *idade < 0)) { free(b); return -1; }
  if (u[2] > EPG_MAX_CANAL || u[3] > EPG_EVS_MAX ||
      (unsigned long long)u[2] * 4 + (unsigned long long)u[3] * 14 > (unsigned long long)(fim-p)) {
    free(b); return -1;
  }
  mapa = malloc(sizeof(int) * (u[2] ? u[2] : 1));
  if (!mapa) { free(b); return -1; }
  for (i = 0; i < (int)u[2]; i++) {
    char id[160], nm[160], ic[600];
    if (!leStr(&p, fim, id, sizeof id) || !leStr(&p, fim, nm, sizeof nm) || !leStr(&p, fim, ic, sizeof ic)) break;
    mapa[i] = wCanalPorId(g, fonte, id);
    if (mapa[i] < 0) mapa[i] = wNovoCanal(g, fonte, id);
    if (mapa[i] >= 0 && nm[0]) {
      if (!g->canais[mapa[i]].nome) g->canais[mapa[i]].nome = kDup(g, nm);
      wAddChave(g, mapa[i], nm);
    }
    if (mapa[i] >= 0 && ic[0] && !g->canais[mapa[i]].icone) g->canais[mapa[i]].icone = kDup(g, ic);
  }
  if (i == (int)u[2]) {
    time_t agora = time(NULL);
    for (i = 0; i < (int)u[3]; i++) {
      int loc[3]; char t[240];
      if (p + sizeof loc > fim) break;
      memcpy(loc, p, sizeof loc); p += sizeof loc;
      if (!leStr(&p, fim, t, sizeof t)) break;
      if (loc[0] < 0 || loc[0] >= (int)u[2] || mapa[loc[0]] < 0) continue;
      if (!aceitaHorario(loc[1], loc[2], agora)) continue;
      if (wAddEv(g, mapa[loc[0]], loc[1], loc[2], t)) prog++;
    }
  }
  free(mapa); free(b);
  (void)base;
  return prog;
}

// A rede em fluxo, direto no parser.
static int alimentarFluxo(const char *d, size_t n, void *u) {
  return cargaAtual() && xmltv_alimentar((Xmltv *)u, d, n);
}

static void carregarExtra(const EpgExtraInt *e, int fonte, EpgFonteInfo *info) {
  EpgProc pr;
  Xmltv x;
  long idade = 0;
  int c0 = W.nCanais, e0 = W.nEvs, desc0 = W.descartados, r, st = 0;
  long long bytes = 0;
  unsigned long t0 = agoraMs();
  memset(info, 0, sizeof *info);
  snprintf(info->chave, sizeof info->chave, "%.23s", e->chave);
  r = cacheXLer(e, &W, fonte, 1, &idade);
  if (r >= 0) {
    info->ok = 1; info->deCache = 1; info->programas = r; info->idadeS = idade;
    info->canais = W.nCanais - c0;
    printf("[epg] grade %s: %d programas do cache (%ld min)\n", e->chave[0] ? e->chave : "url",
           r, idade / 60);
    fflush(stdout);
    return;
  }
  memset(&pr, 0, sizeof pr);
  pr.g = &W; pr.fonte = fonte; pr.agora = time(NULL);
  pr.ids = e->ids; pr.nIds = e->nIds; pr.nomes = e->nomes; pr.nNomes = e->nNomes;
  memset(&x, 0, sizeof x);
  x.canal = cbCanal; x.icone = cbIcone; x.programa = cbPrograma; x.u = &pr;
  if (!xmltv_iniciar(&x)) return;
  r = rede_baixar_fluxo(e->url, EPG_PRAZO_FLUXO_S, NULL, alimentarFluxo, &x, EPG_REDE_MAX, &st, &bytes);
  info->bytes = bytes;
  info->descartados = W.descartados - desc0;
  // NUNCA a URL: so a chave da fonte, o tamanho e o resultado.
  printf("[epg] grade %s: %s, %lld KB lidos (%s), %d canais, %d programas, %d fora da janela, "
         "%d descartados, %d elementos grandes, %lu ms\n",
         e->chave[0] ? e->chave : "url", r ? "ok" : "falhou", bytes / 1024, x.gz ? "gzip" : "xml",
         pr.canais, pr.programas, pr.foraJanela, info->descartados, x.nPulados,
         (unsigned long)(agoraMs() - t0));
  if (!r && st) printf("[epg] grade %s: HTTP %d\n", e->chave[0] ? e->chave : "url", st);
  fflush(stdout);
  xmltv_fim(&x);
  if (cargaAtual() && r && (pr.programas > 0 || pr.canais > 0)) {
    info->ok = 1; info->programas = pr.programas; info->canais = W.nCanais - c0;
    cacheXGravar(e, &W, c0, e0);
    return;
  }
  // CORTE NO MEIO (rede caiu depois de parte do arquivo): o que entrou fica, e a
  // fonte sai "degradada" — sem gravar o cache, que seria uma grade pela metade.
  if (!r && pr.programas > 0) {
    info->ok = 1; info->programas = pr.programas; info->canais = W.nCanais - c0;
    info->deCache = 0; info->idadeS = -1;
    return;
  }
  // A rede falhou (ou veio vazia): o que havia no disco, mesmo velho, vale
  // mais que nada — a fonte fica "degradada" e nao "fora".
  r = cacheXLer(e, &W, fonte, 0, &idade);
  if (r >= 0) {
    info->ok = 1; info->deCache = 1; info->programas = r; info->idadeS = idade;
    info->canais = W.nCanais - c0;
    printf("[epg] grade %s: rede falhou, fica a do cache (%ld h)\n", e->chave[0] ? e->chave : "url",
           idade / 3600);
    fflush(stdout);
  }
}

static void *fioEpg(void *u) {
  int i, ok = 0, nx = 0;
  char arq[EPG_MAX_FONTES][8];
  int nArq;
  static EpgExtraInt copia[EPG_MAX_EXTRAS + 1];
  (void)u;
  pthread_mutex_lock(&trava);
  extraMudou = 0;
  fontesMudaram = 0;
  if (nFontesArq < 0) {
    for (nArq = 0; nArq < (int)(sizeof PADRAO_ARQ / sizeof PADRAO_ARQ[0]); nArq++)
      snprintf(arq[nArq], sizeof arq[0], "%s", PADRAO_ARQ[nArq]);
  } else {
    for (nArq = 0; nArq < nFontesArq; nArq++) snprintf(arq[nArq], sizeof arq[0], "%s", fontesArq[nArq]);
  }
  // Copia profunda: o fio do guia pode trocar a lista durante a carga.
  for (i = 0; i < nExtras && nx < EPG_MAX_EXTRAS; i++) {
    const char **ids = (const char **)extras[i].ids, **nms = (const char **)extras[i].nomes;
    extraMontar(&copia[nx++], extras[i].chave, extras[i].url, NULL, 0, NULL, 0);
    // os vetores ja estao normalizados: copia direta
    copia[nx - 1].ids = malloc(sizeof(char *) * (size_t)(extras[i].nIds ? extras[i].nIds : 1));
    copia[nx - 1].nomes = malloc(sizeof(char *) * (size_t)(extras[i].nNomes ? extras[i].nNomes : 1));
    if (copia[nx - 1].ids) { int k; for (k = 0; k < extras[i].nIds; k++) copia[nx - 1].ids[copia[nx - 1].nIds++] = strdup(ids[k]); }
    if (copia[nx - 1].nomes) { int k; for (k = 0; k < extras[i].nNomes; k++) copia[nx - 1].nomes[copia[nx - 1].nNomes++] = strdup(nms[k]); }
    copia[nx - 1].hFiltro = extras[i].hFiltro;
  }
  if (legadoUrl[0] && nx < EPG_MAX_EXTRAS) {
    int k, tem = 0;
    for (k = 0; k < nx; k++) if (!strcmp(copia[k].chave, "xt")) tem = 1;
    if (!tem) {
      const char **ids = malloc(sizeof(char *) * (size_t)(nLegadoIds ? nLegadoIds : 1));
      if (ids) for (k = 0; k < nLegadoIds; k++) ids[k] = legadoIds[k];
      extraMontar(&copia[nx++], "xt", legadoUrl, ids, ids ? nLegadoIds : 0, NULL, 0);
      free(ids);
    }
  }
  if (legadoM3uUrl[0] && nx < EPG_MAX_EXTRAS)
    extraMontar(&copia[nx++], "ma00000000", legadoM3uUrl, NULL, 0, NULL, 0);
  for (i = 0; i < nx; i++)
    copia[i].hFiltro = hashTexto(copia[i].hFiltro, carga.memoriaBaixa ? "24h" : "full");
  if (carga.soProvedores) nArq = 0;
  pthread_mutex_unlock(&trava);
  if (!cargaAtual() || !wAbrir(&W)) { for (i = 0; i < nx; i++) extraSoltar(&copia[i]); goto fim; }
  for (i = 0; cargaAtual() && i < nArq; i++) {
    long n = 0;
    char *gz = obterGz(arq[i], &n);
    if (gz) {
      EpgProc pr;
      char nXml[40];
      memset(&pr, 0, sizeof pr);
      pr.g = &W; pr.fonte = 0; pr.agora = time(NULL);
      if (processarBuf(&pr, gz, n)) ok = 1;
      nomesFonte(arq[i], NULL, 0, nXml, NULL, NULL, sizeof nXml);
      printf("[epg] %s: %d programas\n", nXml, pr.programas);
      fflush(stdout);
      free(gz);
    }
  }
  nInfoW = 0;
  for (i = 0; cargaAtual() && i < nx; i++) {
    carregarExtra(&copia[i], i + 1, &infoW[nInfoW]);
    if (infoW[nInfoW].ok) ok = 1;
    nInfoW++;
  }
  // A ordem das fontes extras na grade publicada e a de `copia`: guardar as
  // chaves para epg_canal_fonte.
  pthread_mutex_lock(&trava);
  for (i = 0; i < nx; i++) {
    snprintf(chaveFonteW[i + 1], sizeof chaveFonteW[0], "%.23s", copia[i].chave);
    hashUrl(copia[i].url, hashFonteW[i + 1], sizeof hashFonteW[0]);
  }
  nChaveFonteW = nx + 1;
  pthread_mutex_unlock(&trava);
  for (i = 0; i < nx; i++) extraSoltar(&copia[i]);
  if (W.descartados) {
    printf("[epg] %d programas descartados: teto de memoria da grade (%d programas, %ld KB de titulo)\n",
           W.descartados, EPG_EVS_MAX, (long)(EPG_ARENA_MAX >> 10));
    fflush(stdout);
  }
  wFechar(&W);
fim:
  pendOk = ok;
  atomic_store_explicit(&pendPronto, 1, memory_order_release);
  return NULL;
}
// --- API ---------------------------------------------------------------------------
void epg_iniciar(void) {
  pthread_t t;
  if (fioVivo) {
    if (!cargaAtual()) reiniciarPend = 1;
    return;
  }
  carga.sessao = nv_perfil_sessao_ler(); carga.cancelada = 0;
  carga.perfil = perfilGrade;
  pthread_mutex_lock(&trava);
  carga.memoriaBaixa = epgMemoriaBaixa; carga.soProvedores = soProvedores;
  carga.forcarRede = forcarRede; forcarRede = 0; carga.agora = time(NULL);
  pthread_mutex_unlock(&trava);
  fioVivo = 1;
  estado = EPG_BAIXANDO;
  if (pthread_create(&t, NULL, fioEpg, NULL) != 0) {
    fioVivo = 0; estado = EPG_FALHOU;
  }
  else pthread_detach(t);
}

int epg_estado(void) { return estado; }

void epg_passo(void) {
  if (atomic_load_explicit(&pendPronto, memory_order_acquire)) {
    pthread_mutex_lock(&trava);
    if (!cargaAtual()) {
      gradeSoltar(&W);
      estado = EPG_PARADO; fioVivo = 0;
      atomic_store_explicit(&pendPronto, 0, memory_order_relaxed);
      pthread_mutex_unlock(&trava);
      if (reiniciarPend) { reiniciarPend = 0; epg_iniciar(); }
      return;
    }
    if (!pendOk && P.nEvs > 0) {
      gradeSoltar(&W); estado = EPG_PRONTO; carregadaEm = time(NULL);
      fioVivo = 0; atomic_store_explicit(&pendPronto, 0, memory_order_relaxed);
      pthread_mutex_unlock(&trava);
      return;
    }
    gradeSoltar(&P);
    P = W;
    memset(&W, 0, sizeof W);
    memcpy(chaveFonteP, chaveFonteW, sizeof chaveFonteP); nChaveFonteP = nChaveFonteW;
    memcpy(hashFonteP, hashFonteW, sizeof hashFonteP);
    memcpy(infoPub, infoW, sizeof infoPub); nInfoPub = nInfoW;
    carregadaEm = time(NULL); epgRevisao++;
    estado = pendOk ? EPG_PRONTO : EPG_FALHOU;
    pendPronto = 0;
    fioVivo = 0;
    pthread_mutex_unlock(&trava);
    { time_t ji = 0, jf = 0, ag = time(NULL);
      epg_janela_total(&ji, &jf);
      printf("[epg] grade publicada: %d canais, %d programas, %ld B de titulo; "
             "cobre de %+.1f d a %+.1f d\n",
             P.nCanais, P.nEvs, P.arenaN,
             jf ? (double)(ji - ag) / 86400.0 : 0.0,
             jf ? (double)(jf - ag) / 86400.0 : 0.0); }
    fflush(stdout);
    return;
  }
  if (estado == EPG_PRONTO && !fioVivo &&
      time(NULL) - carregadaEm > EPG_CACHE_SEG)
    epg_iniciar();
  else if ((extraMudou || fontesMudaram) && !fioVivo &&
           (estado == EPG_PRONTO || estado == EPG_FALHOU))
    epg_iniciar();
}

void epg_abrir_sessao(int perfil) {
  carga.cancelada = 1;
  pthread_mutex_lock(&trava);
  gradeSoltar(&P);
  for (int i = 0; i < nExtras; i++) extraSoltar(&extras[i]);
  nExtras = nInfoPub = nChaveFonteP = 0;
  free(legadoIds); legadoIds = NULL; nLegadoIds = 0; legadoUrl[0] = legadoM3uUrl[0] = 0;
  paisesAtivos[0] = 0; nFontesArq = -1; extraMudou = fontesMudaram = 0;
  perfilGrade = perfil > 0 ? perfil : 1;
  carregadaEm = 0; estado = EPG_PARADO; reiniciarPend = 0;
  soProvedores = forcarRede = 0; epgRevisao++;
  pthread_mutex_unlock(&trava);
}

int epg_fontes_info(EpgFonteInfo *out, int max) {
  int n = nInfoPub < max ? nInfoPub : max;
  if (out && n > 0) memcpy(out, infoPub, sizeof *out * (size_t)n);
  return n;
}

// Faixa [lo, hi) do indice com a chave `k`. Chamar com a trava tomada.
static int idxFaixa(const char *k, int *lo) {
  int a = 0, b = P.nIdx, n = 0;
  while (a < b) {
    int m = (a + b) / 2;
    if (strcmp(P.k + P.idx[m].chave, k) < 0) a = m + 1; else b = m;
  }
  *lo = a;
  while (a + n < P.nIdx && !strcmp(P.k + P.idx[a + n].chave, k)) n++;
  return n;
}
// Standalone XMLTV keys are shared; provider/account schedules stay scoped.
static int fonteCompativel(int candidata, int fonte) {
  if (fonte == -1 || candidata == fonte || candidata == 0) return 1;
  const char *chave = candidata > 0 && candidata < nChaveFonteP ? chaveFonteP[candidata] : "";
  return chave[0] == 'x' && strspn(chave + 1, "0123456789abcdef") == 8 && chave[9] == 0;
}

// O melhor canal com a chave exata: com programa antes de sem, e da fonte
// pedida antes das outras. -1 = nenhum. Com a trava tomada.
static int idxMelhor(const char *k, int fonte) {
  int lo, n = k && k[0] ? idxFaixa(k, &lo) : 0, i, melhor = -1, pm = -1;
  for (i = 0; i < n; i++) {
    int c = P.idx[lo + i].canal;
    if (!fonteCompativel(P.canais[c].fonte, fonte)) continue;
    int pts = (P.canais[c].evN > 0 ? 2 : 0) + (fonte >= 0 && P.canais[c].fonte == fonte ? 1 : 0);
    if (pts > pm) { pm = pts; melhor = c; }
  }
  return melhor;
}

// A GRADE LISTA CANAIS SEM PROGRAMA (a BR1 real tem 266 <channel> e so 105 com
// <programme>, medido em 18/09): quando a regra devolve um canal vazio, vale
// outro com a mesma chave que tenha programa. Com a trava tomada.
static int comGrade(int i, const char *chave, int fonte) {
  int j;
  if (i < 0 || P.canais[i].evN > 0 || !chave) return i;
  j = idxMelhor(chave, fonte);
  return j >= 0 && P.canais[j].evN > 0 ? j : i;
}

static int fontePorChave(const char *chave) {
  int i;
  if (!chave) return -1;
  for (i = 1; i < nChaveFonteP; i++) if (!strcmp(chaveFonteP[i], chave)) {
    char hu[17];
    for (int j = 0; j < nExtras; j++) if (!strcmp(extras[j].chave, chave)) {
      hashUrl(extras[j].url, hu, sizeof hu);
      return !strcmp(hu, hashFonteP[i]) ? i : -1;
    }
    const char *u = !strcmp(chave, "xt") ? legadoUrl :
                    !strcmp(chave, "ma00000000") ? legadoM3uUrl : "";
    if (u[0]) { hashUrl(u, hu, sizeof hu); if (!strcmp(hu, hashFonteP[i])) return i; }
    return -1;
  }
  return -1;
}

// Id exato sem caixa; a fonte pedida primeiro, depois qualquer outra. So
// devolve canal COM programa. Com a trava tomada.
static int porId(const char *id, int fonte) {
  int f, r;
  if (!id || !id[0] || !P.nCanais) return -1;
  if (fonte >= 0 && (r = wCanalPorId(&P, fonte, id)) >= 0 && P.canais[r].evN > 0) return r;
  for (f = 0; f < nChaveFonteP || f == 0; f++) {
    if (f == fonte || !fonteCompativel(f, fonte)) continue;
    r = wCanalPorId(&P, f, id);
    if (r >= 0 && P.canais[r].evN > 0) return r;
  }
  return -1;
}

int epg_match_id(const char *id) {
  int r;
  if (!P.nCanais || !id || !id[0]) return -1;
  pthread_mutex_lock(&trava);
  r = porId(id, -1);
  pthread_mutex_unlock(&trava);
  return r;
}

int epg_achar(const char *fonte, const char *id) {
  int r = -1, f;
  if (!P.nCanais || !id || !id[0]) return -1;
  pthread_mutex_lock(&trava);
  f = fonte && fonte[0] ? fontePorChave(fonte) : 0;
  if (f >= 0) r = wCanalPorId(&P, f, id);
  pthread_mutex_unlock(&trava);
  return r;
}

// GUARDA DE NUMERO (#158): o casamento por prefixo NAO pode parar no meio de
// um numero. "Pro TV 2" (chave "protv2") herdava a grade de "Pro TV".
static int cortaNumero(const char *k, long corte) {
  return k[corte] >= '0' && k[corte] <= '9';
}

// Os niveis 4 em diante (apelido, prefixo, substring, afiliada), TODOS pelo
// indice ordenado de chaves: nada varre a grade inteira. Com 20 mil canais de
// grade e 30 mil na lista, a varredura das versoes antigas (6 chaves x 20 mil
// por canal) levava minutos numa TV; aqui cada regra e O(tamanho do nome x
// log n). Com a trava.
static int idxCanalCom(const char *k, int fonte) { return idxMelhor(k, fonte); }
static int regras(const char *k1, const char *k2, const char *prim, int fonte) {
  int j, L;
  long t = (long)strlen(k1);
  char sub[96];
  #define DEVOLVE(idx, kv) return comGrade((idx), (kv), fonte)
  // apelido
  for (j = 0; j < (int)(sizeof ALIAS / sizeof ALIAS[0]); j++)
    if (!strcmp(k1, ALIAS[j][0]) || (k2[0] && !strcmp(k2, ALIAS[j][0]))) {
      int c = idxMelhor(ALIAS[j][1], fonte);
      if (c >= 0) DEVOLVE(c, ALIAS[j][1]);
    }
  // a grade e PREFIXO do canal ("recordtvpaulista" -> "recordtv"): a mais comprida
  for (L = (int)t - 1; L >= 4; L--) {
    int c;
    if (cortaNumero(k1, L)) continue;
    memcpy(sub, k1, (size_t)L); sub[L] = 0;
    if ((c = idxCanalCom(sub, fonte)) >= 0) DEVOLVE(c, sub);
  }
  // o canal e prefixo de UMA SO chave da grade (ambigua nao casa)
  if (t >= 4) {
    int lo, a = 0, b = P.nIdx, cand = -1, nCand = 0;
    const char *kc = NULL;
    while (a < b) { int m = (a + b) / 2; if (strcmp(P.k + P.idx[m].chave, k1) < 0) a = m + 1; else b = m; }
    for (lo = a; lo < P.nIdx; lo++) {
      const char *kv = P.k + P.idx[lo].chave;
      if (strncmp(kv, k1, (size_t)t)) break;
      int c = P.idx[lo].canal;
      if (!fonteCompativel(P.canais[c].fonte, fonte)) continue;
      if ((long)strlen(kv) <= t || cortaNumero(kv, t)) continue;
      if (P.idx[lo].canal != cand) { cand = P.idx[lo].canal; nCand++; kc = kv; }
    }
    if (nCand == 1) DEVOLVE(cand, kc);
  }
  // uma chave comprida da grade INTEIRA dentro do nome ("TV Cidade - RecordTV"):
  // vence a mais comprida; empate de tamanho com chaves DIFERENTES e ambiguo.
  { const char *nomes[2] = { k1, k2 };
    for (L = (int)t - 1; L >= 6; L--) {
      int achou = -1, ambig = 0, q, i;
      char chaveAchada[96] = "";
      for (q = 0; q < 2; q++) {
        const char *k = nomes[q];
        int tk = (int)strlen(k);
        for (i = 0; i + L <= tk; i++) {
          int c;
          if (cortaNumero(k + i, L)) continue;
          memcpy(sub, k + i, (size_t)L); sub[L] = 0;
          if ((c = idxCanalCom(sub, fonte)) < 0) continue;
          if (achou < 0) { achou = c; snprintf(chaveAchada, sizeof chaveAchada, "%s", sub); }
          else if (strcmp(chaveAchada, sub)) ambig = 1;
        }
      }
      if (achou >= 0) {
        if (ambig) break;
        { int c = idxMelhor(chaveAchada, fonte);
          snprintf(sub, sizeof sub, "%s", chaveAchada);
          DEVOLVE(c >= 0 ? c : achou, sub); }
      }
    } }
  // afiliada regional: o primeiro token util e a chave inteira de uma rede
  if (prim[0] && (long)strlen(prim) >= 3) {
    int c = idxMelhor(prim, fonte);
    if (c >= 0) DEVOLVE(c, prim);
  }
  #undef DEVOLVE
  return -1;
}

int epg_casar(const char *fonte, const char *epgId, const char *tvgNome,
              const char *nome, int *metodo) {
  char k1[96], k2[96], prim[48];
  int r = -1, f, m = EPG_M_NENHUM;
  if (metodo) *metodo = EPG_M_NENHUM;
  if (!P.nCanais) return -1;
  pthread_mutex_lock(&trava);
  f = fontePorChave(fonte);
  if (fonte && f < 0) f = -2;
  // 1. epg id exato
  if (epgId && epgId[0] && (r = porId(epgId, f)) >= 0) m = EPG_M_ID;
  // 2. tvg-name e 3. nome, normalizados exatos (indice)
  if (r < 0) {
    const char *nomes[2] = { tvgNome, nome };
    int k;
    for (k = 0; k < 2 && r < 0; k++) {
      if (!nomes[k] || !nomes[k][0]) continue;
      normChave(nomes[k], k1, sizeof k1, 0, NULL, 0);
      normChave(nomes[k], k2, sizeof k2, 1, NULL, 0);
      if ((r = idxMelhor(k1, f)) < 0 && k2[0]) r = idxMelhor(k2, f);
      if (r >= 0) m = EPG_M_NOME;
    }
  }
  // 4. apelidos e regras (so pelo nome que o canal mostra)
  if (r < 0 && nome && nome[0]) {
    normChave(nome, k1, sizeof k1, 0, NULL, 0);
    normChave(nome, k2, sizeof k2, 1, prim, sizeof prim);
    if (k1[0] && (r = regras(k1, k2, prim, f)) >= 0) m = EPG_M_ALIAS;
  }
  pthread_mutex_unlock(&trava);
  if (metodo) *metodo = r >= 0 ? m : EPG_M_NENHUM;
  return r;
}

int epg_match(const char *nome) { return epg_casar(NULL, NULL, NULL, nome, NULL); }

// FUZZY SO SUGERE (M5): palavras normalizadas em comum entre o nome do canal e
// o nome do canal da grade, com bonus para o prefixo comum. Nunca casa sozinho
// — duas redes parecidas ("Discovery" e "Discovery Kids") trocariam de grade.
static int tokens(const char *s, char tok[][24], int max) {
  const unsigned char *p = (const unsigned char *)s;
  int n = 0, k = 0;
  while (n < max) {
    unsigned cp = 0;
    int fim = !*p;
    if (!fim) {
      if (*p < 0x80) cp = *p++;
      else if ((*p & 0xE0) == 0xC0 && p[1]) { cp = ((*p & 0x1F) << 6) | (p[1] & 0x3F); p += 2; }
      else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) { cp = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; }
      else { p++; continue; }
      if (cp >= 'A' && cp <= 'Z') cp += 32;
      if (!((cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9'))) cp = (unsigned)letraBase(cp);
    }
    if (!fim && cp) { if (k < 23) tok[n][k++] = (char)cp; continue; }
    tok[n][k] = 0;
    if (k && !tokenInutil(tok[n], k, 1)) n++;
    k = 0;
    if (fim) break;
  }
  return n;
}
int epg_sugerir(const char *nome, int *out, int max) {
  char a[12][24], b[12][24];
  int na, i, n = 0, *pts;
  if (!P.nCanais || !nome || max <= 0) return 0;
  na = tokens(epg_sem_prefixo(nome, NULL), a, 12);
  if (!na) return 0;
  pts = malloc(sizeof(int) * (size_t)max);
  if (!pts) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < P.nCanais; i++) {
    int nb, x, y, s = 0;
    const char *nm = P.canais[i].nome ? KS(&P, P.canais[i].nome) : KS(&P, P.canais[i].id);
    if (P.canais[i].evN <= 0) continue;
    nb = tokens(nm, b, 12);
    for (x = 0; x < na; x++)
      for (y = 0; y < nb; y++)
        if (!strcmp(a[x], b[y])) { s += 10 + (int)strlen(a[x]); break; }
    if (nb && na && !strncmp(a[0], b[0], 3)) s += 3;
    if (s <= 0) continue;
    // insercao ordenada, os `max` melhores
    { int k = n < max ? n++ : max - 1;
      if (k == max - 1 && n == max && pts[k] >= s) continue;
      while (k > 0 && pts[k - 1] < s) { pts[k] = pts[k - 1]; out[k] = out[k - 1]; k--; }
      pts[k] = s; out[k] = i; }
  }
  pthread_mutex_unlock(&trava);
  free(pts);
  return n;
}

static int canalPublicadoValido(int epg);
int epg_n_canais(void) { return P.nCanais; }
const char *epg_canal_id(int epg) { return epg >= 0 && epg < P.nCanais ? KS(&P, P.canais[epg].id) : ""; }
const char *epg_canal_nome(int epg) {
  if (!canalPublicadoValido(epg)) return "";
  return P.canais[epg].nome ? KS(&P, P.canais[epg].nome) : KS(&P, P.canais[epg].id);
}
const char *epg_canal_icone(int epg) { return epg >= 0 && epg < P.nCanais ? KS(&P, P.canais[epg].icone) : ""; }
const char *epg_icone_id(const char *fonte, const char *id) {
  int c, f;
  if (estado != EPG_PRONTO || !id || !id[0]) return "";
  f = fontePorChave(fonte);
  c = f >= 0 ? wCanalPorId(&P, f, id) : -1;
  if (c >= 0 && P.canais[c].icone) return epg_canal_icone(c);
  for (f = 0; f < nChaveFonteP; f++) {
    c = wCanalPorId(&P, f, id);
    if (c >= 0 && P.canais[c].icone) return epg_canal_icone(c);
  }
  return "";
}
const char *epg_canal_fonte(int epg) {
  int f;
  if (!canalPublicadoValido(epg)) return "";
  f = P.canais[epg].fonte;
  return f > 0 && f < nChaveFonteP ? chaveFonteP[f] : "";
}

static int canalPublicadoValido(int epg) {
  if (epg < 0 || epg >= P.nCanais) return 0;
  int f = P.canais[epg].fonte;
  return f == 0 || (f > 0 && f < nChaveFonteP && fontePorChave(chaveFonteP[f]) == f);
}

static void pubProg(long m, EpgProg *p) {
  p->ini = (time_t)P.evs[m].ini; p->fim = (time_t)P.evs[m].fim; p->titulo = P.arena + P.evs[m].tit;
}

static long pubPrimeiroVivo(int epg, time_t t) {
  long lo = P.canais[epg].evIni, hi = lo + P.canais[epg].evN - 1, m = -1;
  while (lo <= hi) {
    long mid = (lo + hi) >> 1;
    if ((time_t)P.evs[mid].fim <= t) lo = mid + 1;
    else { m = mid; hi = mid - 1; }
  }
  return m;
}

int epg_agora(int epg, time_t agora, EpgProg *p) {
  long m;
  int ok = 0;
  if (!canalPublicadoValido(epg)) return 0;
  pthread_mutex_lock(&trava);
  m = pubPrimeiroVivo(epg, agora);
  if (m >= 0 && (time_t)P.evs[m].ini <= agora) { pubProg(m, p); ok = 1; }
  pthread_mutex_unlock(&trava);
  return ok;
}

int epg_proximo(int epg, time_t agora, int k, EpgProg *p) {
  long m, topo;
  int ok = 0;
  if (!canalPublicadoValido(epg) || k < 0) return 0;
  pthread_mutex_lock(&trava);
  topo = P.canais[epg].evIni + P.canais[epg].evN;
  m = pubPrimeiroVivo(epg, agora);
  if (m >= 0 && (time_t)P.evs[m].ini <= agora) m++;
  if (m >= 0) {
    m += k;
    if (m < topo) { pubProg(m, p); ok = 1; }
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

int epg_janela(int epg, time_t *ini, time_t *fim) {
  int ok = 0;
  if (!canalPublicadoValido(epg)) return 0;
  pthread_mutex_lock(&trava);
  if (P.canais[epg].evN > 0) {
    long a = P.canais[epg].evIni, b = a + P.canais[epg].evN - 1;
    if (ini) *ini = (time_t)P.evs[a].ini;
    if (fim) *fim = (time_t)P.evs[b].fim;
    ok = 1;
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

int epg_janela_total(time_t *ini, time_t *fim) {
  int i, ok = 0;
  time_t a = 0, b = 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < P.nCanais; i++) {
    if (P.canais[i].evN <= 0) continue;
    { long x = P.canais[i].evIni, y = x + P.canais[i].evN - 1;
      if (!ok || (time_t)P.evs[x].ini < a) a = (time_t)P.evs[x].ini;
      if (!ok || (time_t)P.evs[y].fim > b) b = (time_t)P.evs[y].fim;
      ok = 1; }
  }
  pthread_mutex_unlock(&trava);
  if (ini) *ini = a;
  if (fim) *fim = b;
  return ok;
}

int epg_total(int epg) {
  if (!canalPublicadoValido(epg)) return 0;
  return (int)P.canais[epg].evN;
}

// `pular`: quantos da sequencia (a mesma de epg_faixa, em ordem do indice) ja
// foram entregues — o cursor de quem pagina e POSICAO, nao horario: a XMLTV nao
// rejeita sobreposicao e o fim dos programas nao e monotono (#344).
int epg_faixa_desde(int epg, time_t de, time_t ate, int pular, EpgProg *out, int cap) {
  long m, topo;
  int n = 0, v = 0;
  if (!canalPublicadoValido(epg) || ate <= de || (out && cap <= 0)) return 0;
  if (pular < 0) pular = 0;
  pthread_mutex_lock(&trava);
  topo = P.canais[epg].evIni + P.canais[epg].evN;
  m = pubPrimeiroVivo(epg, de);
  if (m >= 0)
    for (; m < topo && (time_t)P.evs[m].ini < ate; m++) {
      if ((time_t)P.evs[m].fim <= de) continue;
      if (v++ < pular) continue;
      if (out) {
        if (n >= cap) break;
        pubProg(m, &out[n]);
      }
      n++;
    }
  pthread_mutex_unlock(&trava);
  return n;
}

int epg_faixa(int epg, time_t de, time_t ate, EpgProg *out, int cap) {
  return epg_faixa_desde(epg, de, ate, 0, out, cap);
}

// --- teste ----------------------------------------------------------------------------
void epg_teste_limpar(void) {
  pthread_mutex_lock(&trava);
  gradeSoltar(&P);
  gradeSoltar(&W);
  estado = EPG_PARADO; fioVivo = 0; pendPronto = 0;
  pthread_mutex_unlock(&trava);
}

int epg_xml_processar(char *xml) {
  EpgProc pr;
  epg_teste_limpar();
  if (!wAbrir(&W)) return -1;
  memset(&pr, 0, sizeof pr);
  pr.g = &W; pr.fonte = 0; pr.agora = time(NULL);
  processarBuf(&pr, xml, (long)strlen(xml));
  wFechar(&W);
  pthread_mutex_lock(&trava);
  gradeSoltar(&P);
  P = W; memset(&W, 0, sizeof W);
  nChaveFonteP = 1;
  estado = EPG_PRONTO;
  pthread_mutex_unlock(&trava);
  return pr.programas;
}

#include "epg_contas.inc"
