// REGRAS DA LIVE TV que nao dependem de rede nem de SDL: resolucao lida do
// nome da fonte/canal, nome-base para achar as variantes FHD/HD/SD do mesmo
// canal, e as recomendacoes do diagnostico da Live TV (livetvdiag.c).
//
// Funcoes puras e inline, para o teste compilar no Mac (tests/livetv_regras.sh).
#ifndef NV_LIVETV_REGRAS_H
#define NV_LIVETV_REGRAS_H

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

// Source preferences describe files for movies and episodes. Custom resources
// are not assumed to be VOD: live add-ons can use arbitrary resource names.
static inline int nv_tipo_vod(const char *tipo) {
  return tipo && (!strcasecmp(tipo, "movie") || !strcasecmp(tipo, "series"));
}

// Channel aliases and the live-event types used by Sports Streams/Nebula.
// This classifies playback; sports collections can remain on Home.
// Identify the request before choosing a source, not after opening the UI.
static inline int nv_tipo_canal(const char *tipo) {
  return tipo && (!strcasecmp(tipo, "channel") || !strcasecmp(tipo, "tv") ||
                  !strcasecmp(tipo, "channels") || !strcasecmp(tipo, "live") ||
                  !strcasecmp(tipo, "iptv") || !strcasecmp(tipo, "sport") ||
                  !strcasecmp(tipo, "sports"));
}

// --- resolucao -----------------------------------------------------------------
// Opcao de Ajustes "Resolucao principal": 0 Automatica, 1 4K, 2 1080p, 3 720p, 4 SD.
static inline int nv_res_opcao_altura(int opcao) {
  static const int A[] = { 0, 2160, 1080, 720, 480 };
  return opcao >= 0 && opcao <= 4 ? A[opcao] : 0;
}
static inline int nv_res_altura_opcao(int altura) {
  return altura >= 2160 ? 1 : altura >= 1080 ? 2 : altura >= 720 ? 3 : altura > 0 ? 4 : 0;
}

// Palavra isolada (sem letra/numero colado dos dois lados), sem caixa.
static inline int nv_res_tem(const char *s, const char *t) {
  size_t n = strlen(t);
  const char *p;
  if (!s) return 0;
  for (p = s; *p; p++)
    if ((p == s || !isalnum((unsigned char)p[-1])) && !strncasecmp(p, t, n) &&
        !isalnum((unsigned char)p[n])) return 1;
  return 0;
}

// Altura que o NOME declara. As listas de canal usam marca (4K/UHD, FHD, HD,
// SD) mais do que numero; "HD" isolado e 720 — "FHD" e outra palavra e nao
// casa com ele. 0 = o nome nao diz.
static inline int nv_res_do_texto(const char *s) {
  if (!s || !*s) return 0;
  if (nv_res_tem(s, "4k") || nv_res_tem(s, "uhd") || nv_res_tem(s, "2160") ||
      nv_res_tem(s, "2160p")) return 2160;
  if (nv_res_tem(s, "fhd") || nv_res_tem(s, "1080") || nv_res_tem(s, "1080p") ||
      nv_res_tem(s, "1080i") || nv_res_tem(s, "fullhd")) return 1080;
  if (nv_res_tem(s, "hd") || nv_res_tem(s, "720") || nv_res_tem(s, "720p") ||
      nv_res_tem(s, "hd+")) return 720;
  if (nv_res_tem(s, "sd") || nv_res_tem(s, "480") || nv_res_tem(s, "480p") ||
      nv_res_tem(s, "576") || nv_res_tem(s, "576p")) return 480;
  return 0;
}

// 1 quando a fonte e a resolucao preferida. Sem preferencia, ou fonte que nao
// diz a altura, nada e "preferido" (a ordem do addon vale sozinha).
static inline int nv_res_preferida(int altura, int alvo) {
  return alvo > 0 && altura > 0 && altura == alvo;
}

// NOME-BASE do canal: o nome sem as marcas de resolucao e de codec, em
// minusculas e com espacos simples. "RO| CINEMAX FHD" e "RO| Cinemax HD" dao
// "ro| cinemax"; e assim que as variantes de um mesmo canal Xtream se acham.
static inline void nv_nome_base(const char *nome, char *dst, size_t n) {
  static const char *const MARCAS[] = {
    "4k", "uhd", "2160p", "2160", "fhd", "fullhd", "1080p", "1080i", "1080",
    "hd+", "hd", "720p", "720", "sd", "576p", "576", "480p", "480",
    "hevc", "h265", "h.265", "h264", "h.264", "50fps", "60fps", NULL
  };
  size_t u = 0;
  const char *p = nome ? nome : "";
  int espaco = 0;
  if (!n) return;
  while (*p && u + 1 < n) {
    int k, pulou = 0;
    if (p == nome || !isalnum((unsigned char)p[-1])) {
      for (k = 0; MARCAS[k]; k++) {
        size_t m = strlen(MARCAS[k]);
        if (!strncasecmp(p, MARCAS[k], m) && !isalnum((unsigned char)p[m])) {
          p += m; pulou = 1; break;
        }
      }
    }
    if (pulou) { espaco = 1; continue; }
    if (*p == ' ' || *p == '\t' || *p == '-' || *p == '_' || *p == '(' || *p == ')' ||
        *p == '[' || *p == ']') {
      espaco = 1; p++; continue;
    }
    if (espaco && u) dst[u++] = ' ';
    espaco = 0;
    if (u + 1 < n) dst[u++] = (char)tolower((unsigned char)*p);
    p++;
  }
  dst[u] = 0;
}

// --- busca do guia ---------------------------------------------------------------
// Minusculas sem acento (Latin-1 em UTF-8: a acentuacao de pt/es/fr/ro/de),
// para "sportv" achar "SporTV" e "romania" achar "România". O resto passa como
// veio (cirilico, CJK): casa por igualdade de bytes.
static inline void nv_dobrar(const char *s, char *d, size_t n) {
  static const char *const MAPA_C3 =
    "aaaaaaaceeeeiiii" "dnooooo*ouuuuyts"   /* C3 80..9F (maiusculas) */
    "aaaaaaaceeeeiiii" "dnooooo/ouuuuyty";  /* C3 A0..BF (minusculas) */
  size_t u = 0;
  const unsigned char *p = (const unsigned char *)(s ? s : "");
  if (!n) return;
  while (*p && u + 1 < n) {
    if (p[0] == 0xC3 && p[1] >= 0x80 && p[1] <= 0xBF) { d[u++] = MAPA_C3[p[1] - 0x80]; p += 2; continue; }
    if (p[0] == 0xC4 || p[0] == 0xC5) {        // ă â ș ț ł ő...: a letra-base
      // U+0100..U+017F, a letra-base de cada um (gerado de NFD).
      static const char *const C4 = "aaaaaaccccccccddddeeeeeeeeeegggggggghhhhiiiiiiiiiiiijjkkklllllll";
      static const char *const C5 = "lllnnnnnnnnnoooooooorrrrrrssssssssttttttuuuuuuuuuuuuwwyyyzzzzzzs";
      int i = p[1] - 0x80;
      const char *m = p[0] == 0xC4 ? C4 : C5;
      d[u++] = (i >= 0 && i < 64) ? m[i] : '?';
      p += 2; continue;
    }
    if (*p == 0xC8 && (p[1] == 0x98 || p[1] == 0x99)) { d[u++] = 's'; p += 2; continue; }  // Ș ș
    if (*p == 0xC8 && (p[1] == 0x9A || p[1] == 0x9B)) { d[u++] = 't'; p += 2; continue; }  // Ț ț
    d[u++] = (char)tolower(*p);
    p++;
  }
  d[u] = 0;
}
// `agulha` (ja dobrada) aparece em `palheiro` (crua)?
static inline int nv_contem_dobrado(const char *palheiro, const char *agulha) {
  char a[512];
  if (!agulha || !agulha[0]) return 0;
  nv_dobrar(palheiro, a, sizeof a);
  return strstr(a, agulha) != NULL;
}

// --- recomendacoes do diagnostico -------------------------------------------
// O que deu cada canal testado. `formato`: 0 HLS, 1 TS. Tempos em ms, -1 =
// nao medido. `kbpsFluxo` e a vazao medida no proprio canal (rede ate o
// provedor); `alturaVista` e o que o pipeline/SPS disse.
enum { LTD_OK, LTD_SEM_RESPOSTA, LTD_HTTP, LTD_NAO_E_VIDEO, LTD_SEM_DECODER,
       LTD_ERRO_PLAYER, LTD_SEM_TESTE };
typedef struct {
  int formato;
  int tocou;            // o pipeline deu o primeiro quadro
  int testouPlayer;     // o pipeline foi tentado (0 no Mac, ou sem tempo)
  int falha;            // LTD_*
  int quadroMs;         // load -> primeiro quadro
  int esperaMs;         // pedido -> primeiro byte (latencia)
  int kbpsFluxo;
  int alturaVista;
  int dezBits;          // SPS de 10 bits (H.264 High 10 / HEVC Main 10)
} LtdCanal;

typedef struct {
  int formato;          // 0 Automatico, 1 HLS, 2 TS (opcao de Ajustes)
  int resolucao;        // opcao de "Resolucao principal"
  int espera;           // 0 Automatica, 1 25 s, 2 45 s
  int kbpsMediana;      // da rede ate o provedor
  int latenciaMs;
  int tocaram[2], tentados[2];   // por formato
  int quadroMedioMs[2];
  int semDecoder, dezBits;
  int confianca;        // canais com medida de player
} LtdRecomendacao;

static inline int ltd_mediana(int *v, int n) {
  int i, j;
  if (n <= 0) return 0;
  for (i = 1; i < n; i++)
    for (j = i; j > 0 && v[j - 1] > v[j]; j--) { int t = v[j]; v[j] = v[j - 1]; v[j - 1] = t; }
  return v[n / 2];
}

// RESOLUCAO PELA VAZAO: um canal 1080p de IPTV anda em 4-8 Mbps, 720p em 2-4,
// 4K em 15-25. Pede-se o dobro do topo da faixa para aguentar os picos e a
// rede da casa variando: 4K so com 40 Mbps, 1080p com 12, 720p com 6.
static inline int ltd_resolucao_pela_vazao(int kbps) {
  if (kbps <= 0) return 0;
  if (kbps >= 40000) return 1;
  if (kbps >= 12000) return 2;
  if (kbps >= 6000) return 3;
  return 4;
}

static inline void ltd_recomendar(const LtdCanal *c, int n, LtdRecomendacao *r) {
  int kb[64], lat[64], nk = 0, nl = 0, i, f, somaQ[2] = { 0, 0 }, nQ[2] = { 0, 0 };
  int maiorQ = 0;
  memset(r, 0, sizeof *r);
  for (i = 0; i < n && i < 64; i++) {
    f = c[i].formato ? 1 : 0;
    if (c[i].kbpsFluxo > 0) kb[nk++] = c[i].kbpsFluxo;
    if (c[i].esperaMs >= 0 && c[i].falha != LTD_SEM_RESPOSTA) lat[nl++] = c[i].esperaMs;
    if (c[i].testouPlayer) {
      r->tentados[f]++;
      r->confianca++;
      if (c[i].tocou) {
        r->tocaram[f]++;
        if (c[i].quadroMs > 0) {
          somaQ[f] += c[i].quadroMs; nQ[f]++;
          if (c[i].quadroMs > maiorQ) maiorQ = c[i].quadroMs;
        }
      }
    }
    if (c[i].falha == LTD_SEM_DECODER) r->semDecoder++;
    if (c[i].dezBits) r->dezBits++;
  }
  r->kbpsMediana = ltd_mediana(kb, nk);
  r->latenciaMs = ltd_mediana(lat, nl);
  for (f = 0; f < 2; f++) r->quadroMedioMs[f] = nQ[f] ? somaQ[f] / nQ[f] : 0;
  // FORMATO: o que tocou mais; empate com os dois tocando, o que abre antes.
  // Sem medida de player (Mac, ou nada testado), Automatico.
  if (r->tocaram[0] > r->tocaram[1]) r->formato = 1;
  else if (r->tocaram[1] > r->tocaram[0]) r->formato = 2;
  else if (r->tocaram[0] && r->quadroMedioMs[0] && r->quadroMedioMs[1])
    r->formato = r->quadroMedioMs[0] <= r->quadroMedioMs[1] ? 1 : 2;
  r->resolucao = ltd_resolucao_pela_vazao(r->kbpsMediana);
  // ESPERA: o canal mais lento que TOCOU decide. Abriu perto do prazo de
  // 15 s do Xtream (ou do de 25 s): sobe um degrau para nao cortar o que ia
  // abrir. Sem nenhum lento, fica a automatica.
  r->espera = maiorQ > 20000 ? 2 : maiorQ > 11000 ? 1 : 0;
}

#endif
