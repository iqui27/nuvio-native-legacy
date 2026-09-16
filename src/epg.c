// EPG: grade XMLTV externa para os canais ao vivo. Ver epg.h para o porque.
//
// FLUXO:
//   epg_iniciar()     -> fio baixa BR1+BR2 (.gz) do epgshare01, infla com zlib,
//                        grava o XML na pasta de dados e processa.
//   epg_passo()       -> no fio de desenho: publica a grade pronta e pede
//                        re-carga quando o cache passou de 12 h.
//   epg_match(nome)   -> nome do addon -> indice do canal na grade, ou -1.
//   epg_agora/proximo -> programa no ar e os seguintes.
//
// O CASAMENTO POR NOME e o coracao deste arquivo. O addon chama o canal de um
// jeito ("RecordTV Paulista"), a grade de outro ("Record TV", "RECORD").
// A regra, nesta ordem:
//   1. chave normalizada exata (acentos fora, minusculas, so alfanumericos;
//      sufixos de qualidade "hd/4k/sd" e as palavras "canal/tv/channel"
//      descartadas na variante curta);
//   2. apelidos conhecidos (tabela ALIAS abaixo);
//   3. a chave da grade mais comprida que seja PREFIXO da do canal
//      ("recordtvpaulista" herda "recordtv");
//   4. a chave do canal e prefixo de UMA SO chave da grade
//      (ambigua nao casa — a tabela de apelidos decide os empates).
// O que sobra sem casamento (os "24h" de maratona, cams de reality, feeds de
// evento) simplesmente nao tem grade real: o guia os mostra "ao vivo", sem
// programa — e isso e a verdade, nao um defeito do casamento.
#include "epg.h"
#include "rede.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <zlib.h>

#define EPG_MAX_CANAL 1400
#define EPG_MAX_CHAVES 6
// A grade fala ~3,5 dias para a frente; renovar duas vezes por dia cobre a
// virada sem baixar ~1,4 MB a cada abertura do app.
#define EPG_CACHE_SEG (12 * 3600)
// Evento que terminou ha mais tempo que isto nao interessa a "agora/a seguir".
#define EPG_JANELA_PASSADO 7200

// BR1/BR2 cobrem o FrostView. PT1/MX1/AR1 entram para os OUTROS addons de
// canal que o dono possa instalar — Portugal e America Latina sao os
// catalogos de canal mais comuns depois do brasileiro. US/UK ficam fora:
// 6,5 MB de gzip viram ~60 MB de XML, caro demais para um ganho raro.
#define EPG_N_FONTES 5
static const char *FONTE[EPG_N_FONTES] = {
  "https://epgshare01.online/epgshare01/epg_ripper_BR1.xml.gz",
  "https://epgshare01.online/epgshare01/epg_ripper_BR2.xml.gz",
  "https://epgshare01.online/epgshare01/epg_ripper_PT1.xml.gz",
  "https://epgshare01.online/epgshare01/epg_ripper_MX1.xml.gz",
  "https://epgshare01.online/epgshare01/epg_ripper_AR1.xml.gz",
};
static const char *CACHE_XML[EPG_N_FONTES] = {
  "epg-br1.xml", "epg-br2.xml", "epg-pt1.xml", "epg-mx1.xml", "epg-ar1.xml" };
static const char *CACHE_TS[EPG_N_FONTES]  = {
  "epg-br1.ts",  "epg-br2.ts",  "epg-pt1.ts",  "epg-mx1.ts",  "epg-ar1.ts"  };

typedef struct {
  char id[96];
  char nome[96];                      // display-name, para log e depuracao
  char chaves[EPG_MAX_CHAVES][96];    // variantes normalizadas de nome/id
  int  nChaves;
  long evIni, evN;                    // janela no vetor de eventos
} EpgCanal;

typedef struct {
  int    canal;
  time_t ini, fim;
  const char *tit;                    // dentro da arena da grade
} EpgEv;

// DOIS CONJUNTOS, e a separacao e o que torna a concorrencia segura:
//   W_* = a grade EM CONSTRUCAO. So o fio de carga (ou o teste) toca.
//   publicados = a grade que epg_match/agora leem. Trocada de uma vez, sob a
//   trava, quando o fio termina.
typedef struct {
  EpgCanal canais[EPG_MAX_CANAL];
  int      nCanais;
  long     ordemPorId[EPG_MAX_CANAL];  // indices de canais, ordenados por id
  EpgEv   *evs;    int nEvs, capEvs;
  char    *arena;  long arenaN, arenaCap;
} EpgGrade;

static EpgGrade W;                     // construcao (fio / teste)
static EpgGrade P;                     // publicada (fio de desenho)

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int      estado = EPG_PARADO;
static time_t   carregadaEm;
static int      fioVivo;
static int      pendPronto, pendOk;    // o fio terminou e tem resultado

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
  static const char *qual[] = { "hd","fhd","uhd","4k","sd","hdtv","fullhd" };
  static const char *marc[] = { "canal","channel","tv","rede","and","e" };
  int i;
  for (i = 0; i < 7; i++)
    if ((int)strlen(qual[i]) == n && !strncmp(qual[i], t, (size_t)n)) return 1;
  if (curta)
    for (i = 0; i < 6; i++)
      if ((int)strlen(marc[i]) == n && !strncmp(marc[i], t, (size_t)n)) return 1;
  return 0;
}

// Escreve a chave em `dst`. Devolve o comprimento. Se `prim` nao e NULL,
// recebe o PRIMEIRO token aceito — e a forma de reconhecer afiliada
// regional: "SBT RJ" abre com "sbt", a chave inteira da grade da rede-mae.
static int normChave(const char *s, char *dst, int cap, int curta,
                     char *prim, int pcap) {
  char tok[48]; int tn = 0, n = 0, primOk = 0;
  unsigned cp; const unsigned char *p = (const unsigned char *)s;
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

// --- tempo XMLTV -----------------------------------------------------------------
// "20250914223000 -0300" -> epoch. days-from-civil de Hinnant, sem depender de
// timegm (o Tizen/emscripten nao garante).
static long diasDeCivil(int y, int m, int d) {
  long era; unsigned yoe, doy, doe;
  y -= m <= 2;
  era = (y >= 0 ? y : y - 399) / 400;
  yoe = (unsigned)(y - era * 400);
  doy = (153 * (unsigned)(m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
  doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}
static time_t xmltvTempo(const char *s) {
  int y, m, d, hh, mm, ss;
  long seg;
  if (strlen(s) < 14) return 0;
  y  = (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
  m  = (s[4]-'0')*10 + (s[5]-'0');   d  = (s[6]-'0')*10 + (s[7]-'0');
  hh = (s[8]-'0')*10 + (s[9]-'0');   mm = (s[10]-'0')*10 + (s[11]-'0');
  ss = (s[12]-'0')*10 + (s[13]-'0');
  seg = diasDeCivil(y, m, d) * 86400 + hh * 3600 + mm * 60 + ss;
  { const char *f = s + 14;
    while (*f == ' ') f++;
    if ((*f == '+' || *f == '-') && f[1] && f[4]) {
      int off = ((f[1]-'0')*10 + (f[2]-'0')) * 3600 +
                ((f[3]-'0')*10 + (f[4]-'0')) * 60;
      seg += (*f == '+') ? -off : off;
    } }
  return (time_t)seg;
}

// --- varredura ---------------------------------------------------------------------
static const char *achaAtr(const char *tag, const char *fim, const char *atr,
                           char *dst, int cap) {
  char padrao[64];
  const char *p, *v;
  int n;
  snprintf(padrao, sizeof padrao, "%s=\"", atr);
  p = strstr(tag, padrao);
  if (!p || p > fim) { dst[0] = 0; return NULL; }
  v = p + strlen(padrao);
  p = strchr(v, '"');
  if (!p || p > fim) { dst[0] = 0; return NULL; }
  n = (int)(p - v);
  if (n >= cap) n = cap - 1;
  memcpy(dst, v, (size_t)n); dst[n] = 0;
  return p;
}

static void desentidade(char *s) {
  char *r = s, *w = s;
  while (*r) {
    if (*r == '&') {
      char *p = strchr(r, ';');
      if (p && p - r < 10) {
        if      (!strncmp(r, "&amp;",  5)) { *w++ = '&';  r = p + 1; continue; }
        else if (!strncmp(r, "&lt;",   4)) { *w++ = '<';  r = p + 1; continue; }
        else if (!strncmp(r, "&gt;",   4)) { *w++ = '>';  r = p + 1; continue; }
        else if (!strncmp(r, "&quot;", 6)) { *w++ = '"';  r = p + 1; continue; }
        else if (!strncmp(r, "&apos;", 6)) { *w++ = '\''; r = p + 1; continue; }
        else if (r[1] == '#') {
          long cp = atol(r + 2); r = p + 1;
          if (cp < 0x80)  { *w++ = (char)cp; continue; }
          if (cp < 0x800) { *w++ = (char)(0xC0 | (cp >> 6));
                            *w++ = (char)(0x80 | (cp & 0x3F)); continue; }
        }
      }
    }
    *w++ = *r++;
  }
  *w = 0;
}

// --- grade em construcao (W_*) ----------------------------------------------------
static int wCanalPorId(EpgGrade *g, const char *id) {
  long lo = 0, hi = g->nCanais - 1;
  while (lo <= hi) {
    long m = (lo + hi) >> 1;
    int c = strcmp(id, g->canais[g->ordemPorId[m]].id);
    if (!c) return (int)g->ordemPorId[m];
    if (c < 0) hi = m - 1; else lo = m + 1;
  }
  return -1;
}

static void wAddChave(EpgGrade *g, int i, const char *nome) {
  char k[96]; int j, curta;
  for (curta = 0; curta < 2; curta++) {
    if (!normChave(nome, k, sizeof k, curta, NULL, 0) || !k[0]) continue;
    for (j = 0; j < g->canais[i].nChaves; j++)
      if (!strcmp(k, g->canais[i].chaves[j])) break;
    if (j < g->canais[i].nChaves || g->canais[i].nChaves >= EPG_MAX_CHAVES) continue;
    snprintf(g->canais[i].chaves[g->canais[i].nChaves++], 96, "%s", k);
  }
}

// O id "Globo.RJ.br" tambem vira nome: onde o display-name falta ou difere,
// a forma do id e a unica pista. Os ids do epgshare01 vem em dois moldes:
// "Sao.Paulo/SP..Cartoonito.br" (cidade/UF..canal) e "SBT.br" (canal direto).
// O nome do canal e o que vem depois do ".."; sem ele a ultima "/" devolvia
// "SP..cartoonito" e a chave saia "spcartoonito" — meio centenar de canais
// da BR1 ficavam incasaveis ("cartoonito" nunca era gerado).
static void wChavePorId(EpgGrade *g, int i) {
  char buf[96], tmp[96];
  const char *s;
  unsigned k;
  snprintf(buf, sizeof buf, "%s", g->canais[i].id);
  { char *b = strrchr(buf, '.');
    if (b && (b - buf) > 2 && b[1] && b[2] && !b[3]) *b = 0; }   // ".br" do fim
  { char *dd = strstr(buf, "..");
    s = dd ? dd + 2 : buf; }                   // "Cidade/UF..X" -> "X"
  { const char *b = strrchr(s, '/');           // fallback sem "..": "a/b" -> "b"
    if (b) s = b + 1; }
  for (k = 0; *s && k < sizeof tmp - 1; s++) {
    if (*s == '(') {                           // "(espelho)"/"(aberta)": nao
      while (*s && *s != ')') s++;             // identificam o canal
      if (!*s) break;
      continue; }
    tmp[k++] = (*s == '.' || *s == '_') ? ' ' : *s;
  }
  tmp[k] = 0;
  wAddChave(g, i, tmp);
}

static char *wTitulo(EpgGrade *g, const char *s) {
  long n = (long)strlen(s) + 1;
  char *d;
  if (g->arenaN + n >= g->arenaCap) return NULL;
  d = g->arena + g->arenaN;
  memcpy(d, s, (size_t)n); g->arenaN += n;
  return d;
}

static int wCmpEv(const void *a, const void *b) {
  const EpgEv *x = a, *y = b;
  if (x->canal != y->canal) return x->canal - y->canal;
  return (x->ini > y->ini) - (x->ini < y->ini);
}

// Fecha a grade construida: ordena os eventos, tira as duplicatas de BR1/BR2
// (mesmo canal, mesmo inicio e fim) e mede a janela de cada canal.
static void wFechar(EpgGrade *g) {
  int i, w; long ini;
  qsort(g->evs, (size_t)g->nEvs, sizeof(EpgEv), wCmpEv);
  for (i = 0, w = 0; i < g->nEvs; i++)
    if (w == 0 || g->evs[i].canal != g->evs[w-1].canal ||
        g->evs[i].ini != g->evs[w-1].ini || g->evs[i].fim != g->evs[w-1].fim)
      g->evs[w++] = g->evs[i];
  g->nEvs = w;
  for (i = 0, ini = 0; i < g->nCanais; i++) {
    g->canais[i].evIni = ini;
    while (ini < g->nEvs && g->evs[ini].canal == i) ini++;
    g->canais[i].evN = ini - g->canais[i].evIni;
  }
}

// Processa um buffer XMLTV inteiro EM cima de `g` (junta ao que ja existe —
// BR1 e BR2 passam pelos mesmos vetores). O buffer e escrito no processo.
static int wProcessar(EpgGrade *g, char *xml) {
  char *p = xml, *fecha;
  int n = 0;
  time_t agora = time(NULL);

  // PASSO 1: canais.
  while ((p = strstr(p, "<channel "))) {
    char id[96];
    int i;
    fecha = strstr(p, "</channel>");
    if (!fecha) break;
    if (achaAtr(p, fecha, "id", id, sizeof id)) {
      i = wCanalPorId(g, id);
      if (i < 0) {
        if (g->nCanais >= EPG_MAX_CANAL) break;
        i = g->nCanais++;
        memset(&g->canais[i], 0, sizeof g->canais[i]);
        snprintf(g->canais[i].id, sizeof g->canais[i].id, "%s", id);
        { int j = g->nCanais - 1;
          while (j > 0 && strcmp(g->canais[g->ordemPorId[j-1]].id, id) > 0)
            { g->ordemPorId[j] = g->ordemPorId[j-1]; j--; }
          g->ordemPorId[j] = i; }
        wChavePorId(g, i);
      }
      { char *d = strstr(p, "<display-name");
        if (d && d < fecha) {
          char *a = strchr(d, '>'), *b = strstr(d, "</display-name>");
          if (a && b && a < b) {
            char nome[96]; int tn = (int)(b - (a + 1));
            if (tn > (int)sizeof nome - 1) tn = sizeof nome - 1;
            memcpy(nome, a + 1, (size_t)tn); nome[tn] = 0;
            desentidade(nome);
            if (!g->canais[i].nome[0])
              snprintf(g->canais[i].nome, sizeof g->canais[i].nome, "%s", nome);
            wAddChave(g, i, nome);
          } }
      }
    }
    p = fecha + 10;
  }

  // PASSO 2: programas.
  p = xml;
  while ((p = strstr(p, "<programme "))) {
    char id[96], iniS[32], fimS[32], *t;
    int ci;
    time_t ini, fimp;
    fecha = strstr(p, "</programme>");
    if (!fecha) break;
    if (!achaAtr(p, fecha, "channel", id, sizeof id)) { p = fecha + 12; continue; }
    ci = wCanalPorId(g, id);
    if (ci < 0) { p = fecha + 12; continue; }
    if (!achaAtr(p, fecha, "start", iniS, sizeof iniS) ||
        !achaAtr(p, fecha, "stop",  fimS, sizeof fimS)) { p = fecha + 12; continue; }
    ini = xmltvTempo(iniS); fimp = xmltvTempo(fimS);
    if (fimp <= ini || fimp < agora - EPG_JANELA_PASSADO) { p = fecha + 12; continue; }
    t = strstr(p, "<title");
    if (t && t < fecha) {
      char *a = strchr(t, '>'), *b = strstr(t, "</title>");
      if (a && b && a < b && g->nEvs < g->capEvs) {
        char titulo[240]; int tn = (int)(b - (a + 1));
        const char *copia;
        if (tn > (int)sizeof titulo - 1) tn = sizeof titulo - 1;
        memcpy(titulo, a + 1, (size_t)tn); titulo[tn] = 0;
        desentidade(titulo);
        copia = wTitulo(g, titulo);
        if (copia) {
          g->evs[g->nEvs].canal = ci;
          g->evs[g->nEvs].ini   = ini;
          g->evs[g->nEvs].fim   = fimp;
          g->evs[g->nEvs].tit   = copia;
          g->nEvs++; n++;
        }
      }
    }
    p = fecha + 12;
  }
  return n;
}

// --- carga: disco, rede, gzip -------------------------------------------------------
// O buffer de saida CRESCE conforme o inflate avanca — uma estimativa fixa
// (gz*10) estourava nas fontes regionais maiores (PT1 comprime ~11x).
static char *desgzip(const char *buf, long n, long *nOut) {
  z_stream z; long cap = n * 6 + (1 << 20), feito = 0;
  char *out = malloc((size_t)cap);
  if (!out) return NULL;
  memset(&z, 0, sizeof z);
  if (inflateInit2(&z, 16 + 15) != Z_OK) { free(out); return NULL; }
  z.next_in = (Bytef *)buf; z.avail_in = (uInt)n;
  for (;;) {
    if (cap - feito < (1 << 16)) {                 // folga minima: dobra
      char *no = realloc(out, (size_t)(cap * 2));
      if (!no) { inflateEnd(&z); free(out); return NULL; }
      out = no; cap *= 2;
    }
    z.next_out = (Bytef *)out + feito;
    z.avail_out = (uInt)(cap - feito);
    { int r = inflate(&z, 0);
      feito = (long)z.total_out;
      if (r == Z_STREAM_END) break;
      if (r != Z_OK) { inflateEnd(&z); free(out); return NULL; } }
  }
  inflateEnd(&z);
  *nOut = feito;
  return out;
}

static long arquivoIdade(const char *nome) {
  char *ts = dados_ler(nome);
  long velho = EPG_CACHE_SEG + 1;
  if (ts) { long t = atol(ts); if (t > 0) velho = (long)time(NULL) - t; free(ts); }
  return velho;
}

// O XML e grande demais para dados_gravar (feita para strings pequenas):
// escrita direta no caminho, sob a trava de FS que o Tizen exige.
static void gravarXml(const char *nome, const char *tsNome,
                      const char *xml, long n) {
  char cam[640], tbuf[24]; FILE *f;
  if (!dados_caminho(cam, sizeof cam, nome)) return;
  dados_fs_travar();
  f = fopen(cam, "wb");
  if (f) { fwrite(xml, 1, (size_t)n, f); fclose(f); }
  dados_fs_liberar();
  if (!f) return;
  snprintf(tbuf, sizeof tbuf, "%ld", (long)time(NULL));
  dados_gravar_leve(tsNome, tbuf);
  dados_marcar_sujo(1);
}

static char *obterXml(int i, long *nOut) {
  char *xml;
  long ngz = 0;
  char *gz;
  // Cache fresco primeiro: a abertura nao paga a rede quando o arquivo do dia
  // ja esta no aparelho.
  if (arquivoIdade(CACHE_TS[i]) < EPG_CACHE_SEG) {
    xml = dados_ler(CACHE_XML[i]);
    if (xml && xml[0] == '<') { *nOut = (long)strlen(xml); return xml; }
    free(xml);
  }
  gz = rede_baixar_bin(FONTE[i], 60, &ngz);
  if (!gz) return NULL;
  xml = desgzip(gz, ngz, nOut);
  free(gz);
  if (!xml) return NULL;
  { char *c = realloc(xml, (size_t)*nOut + 1);
    if (!c) { free(xml); return NULL; }
    xml = c; xml[*nOut] = 0; }
  gravarXml(CACHE_XML[i], CACHE_TS[i], xml, *nOut);
  return xml;
}

static void *fioEpg(void *u) {
  int i, ok = 0;
  (void)u;
  memset(&W, 0, sizeof W);
  W.capEvs = 90000; W.evs = malloc(sizeof(EpgEv) * (size_t)W.capEvs);
  W.arenaCap = 8L << 20; W.arena = malloc((size_t)W.arenaCap);
  if (!W.evs || !W.arena) { pendPronto = 1; pendOk = 0; return NULL; }
  for (i = 0; i < EPG_N_FONTES; i++) {
    long n = 0;
    char *xml = obterXml(i, &n);
    if (xml) {
      int achou = wProcessar(&W, xml);
      printf("[epg] %s: %d programas\n", CACHE_XML[i], achou);
      fflush(stdout);
      free(xml);
      ok = 1;
    }
  }
  wFechar(&W);
  pendPronto = 1; pendOk = ok;
  return NULL;
}

// --- API ---------------------------------------------------------------------------
void epg_iniciar(void) {
  pthread_t t;
  if (fioVivo) return;
  fioVivo = 1;
  estado = EPG_BAIXANDO;
  if (pthread_create(&t, NULL, fioEpg, NULL) != 0) { fioVivo = 0; estado = EPG_FALHOU; }
  else pthread_detach(t);
}

int epg_estado(void) { return estado; }

void epg_passo(void) {
  if (pendPronto) {
    pthread_mutex_lock(&trava);
    free(P.evs); free(P.arena);
    P = W;
    memset(&W, 0, sizeof W);
    carregadaEm = time(NULL);
    estado = pendOk ? EPG_PRONTO : EPG_FALHOU;
    pendPronto = 0;
    fioVivo = 0;
    pthread_mutex_unlock(&trava);
    printf("[epg] grade publicada: %d canais, %d programas\n", P.nCanais, P.nEvs);
    fflush(stdout);
    return;
  }
  // Grade velha e o fio parado: tenta de novo quando o relogio diz que o cache
  // envelheceu (o download decide sozinho se reusa o arquivo ou baixa de novo).
  if (estado == EPG_PRONTO && !fioVivo &&
      time(NULL) - carregadaEm > EPG_CACHE_SEG)
    epg_iniciar();
}

int epg_match(const char *nome) {
  char k1[96], k2[96], prim[48];
  int i, j, c, melhor = -1;
  long melhorTam = 0, t;
  if (estado != EPG_PRONTO || !nome || !nome[0]) return -1;
  normChave(nome, k1, sizeof k1, 0, NULL, 0);
  normChave(nome, k2, sizeof k2, 1, prim, sizeof prim);
  if (!k1[0]) return -1;

  pthread_mutex_lock(&trava);
  // 1. exata
  for (i = 0; i < P.nCanais; i++)
    for (j = 0; j < P.canais[i].nChaves; j++)
      if (!strcmp(k1, P.canais[i].chaves[j]) ||
          (k2[0] && !strcmp(k2, P.canais[i].chaves[j]))) {
        pthread_mutex_unlock(&trava); return i;
      }

  // 2. apelido
  for (j = 0; j < (int)(sizeof ALIAS / sizeof ALIAS[0]); j++)
    if (!strcmp(k1, ALIAS[j][0]) || (k2[0] && !strcmp(k2, ALIAS[j][0]))) {
      for (i = 0; i < P.nCanais; i++)
        for (c = 0; c < P.canais[i].nChaves; c++)
          if (!strcmp(ALIAS[j][1], P.canais[i].chaves[c])) {
            pthread_mutex_unlock(&trava); return i;
          }
    }

  // 3. a grade e prefixo do canal ("recordtvpaulista" -> "recordtv")
  t = (long)strlen(k1);
  for (i = 0; i < P.nCanais; i++)
    for (j = 0; j < P.canais[i].nChaves; j++) {
      long tc = (long)strlen(P.canais[i].chaves[j]);
      if (tc >= 4 && tc < t &&
          !strncmp(k1, P.canais[i].chaves[j], (size_t)tc) && tc > melhorTam)
        { melhor = i; melhorTam = tc; }
    }
  if (melhor >= 0) { pthread_mutex_unlock(&trava); return melhor; }

  // 4. o canal e prefixo de UMA SO chave da grade (ambigua nao casa)
  { int nCand = 0;
    if (t >= 4)
      for (i = 0; i < P.nCanais; i++)
        for (j = 0; j < P.canais[i].nChaves; j++)
          if ((long)strlen(P.canais[i].chaves[j]) > t &&
              !strncmp(P.canais[i].chaves[j], k1, (size_t)t) && melhor != i) {
            melhor = i; nCand++;
          }
    if (nCand == 1) { pthread_mutex_unlock(&trava); return melhor; } }

  // 5. uma chave comprida da grade aparece INTEIRA dentro do nome do canal:
  // "TV Cidade - RecordTV" carrega "recordtv" no fim. Vence a chave mais
  // comprida ("recordtv" > "record"); empate de tamanho so e ambiguidade se
  // for chave DIFERENTE — a mesma chave em dois canais e a rede duplicada na
  // grade (Record.TV.br, SP..Record...), nao duas redes.
  { long melhorTc = 0; int ambig = 0; const char *melhorChave = NULL;
    melhor = -1;
    for (i = 0; i < P.nCanais; i++)
      for (j = 0; j < P.canais[i].nChaves; j++) {
        const char *kv = P.canais[i].chaves[j];
        long tc = (long)strlen(kv);
        if (tc < 6 || t <= tc) continue;
        if (!strstr(k1, kv) && !(k2[0] && strstr(k2, kv))) continue;
        if (tc > melhorTc) { melhorTc = tc; melhor = i; melhorChave = kv; ambig = 0; }
        else if (tc == melhorTc && i != melhor &&
                 !(melhorChave && !strcmp(melhorChave, kv)))
          ambig = 1;
      }
    if (melhor >= 0 && !ambig) { pthread_mutex_unlock(&trava); return melhor; } }

  // 6. o primeiro token util do canal E a chave inteira da grade: e a regra
  // da afiliada regional — "SBT RJ"/"SBT Thathi Vale" herdam a grade da
  // rede "sbt". Minimo 3 letras para sigla de rede nao colar em qualquer um.
  if (prim[0] && (long)strlen(prim) >= 3)
    for (i = 0; i < P.nCanais; i++)
      for (j = 0; j < P.canais[i].nChaves; j++)
        if (!strcmp(prim, P.canais[i].chaves[j])) {
          pthread_mutex_unlock(&trava); return i;
        }
  pthread_mutex_unlock(&trava);
  return -1;
}

int epg_agora(int epg, time_t agora, EpgProg *p) {
  long lo, hi;
  int ok = 0;
  if (epg < 0 || epg >= P.nCanais) return 0;
  pthread_mutex_lock(&trava);
  lo = P.canais[epg].evIni; hi = lo + P.canais[epg].evN - 1;
  while (lo <= hi) {
    long m = (lo + hi) >> 1;
    if (P.evs[m].fim <= agora) lo = m + 1;
    else if (P.evs[m].ini > agora) hi = m - 1;
    else {
      p->ini = P.evs[m].ini; p->fim = P.evs[m].fim; p->titulo = P.evs[m].tit;
      ok = 1; break;
    }
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

int epg_proximo(int epg, time_t agora, int k, EpgProg *p) {
  long lo, hi, m = -1, topo;
  int ok = 0;
  if (epg < 0 || epg >= P.nCanais || k < 0) return 0;
  pthread_mutex_lock(&trava);
  lo = P.canais[epg].evIni; topo = lo + P.canais[epg].evN;
  hi = topo - 1;
  // primeiro evento que ainda nao terminou
  while (lo <= hi) {
    long mid = (lo + hi) >> 1;
    if (P.evs[mid].fim <= agora) lo = mid + 1;
    else { m = mid; hi = mid - 1; }
  }
  // m = programa no ar (ou o primeiro futuro); o "proximo" pula o que esta no ar
  if (m >= 0 && P.evs[m].ini <= agora) m++;
  m += k;
  if (m >= 0 && m < topo) {
    p->ini = P.evs[m].ini; p->fim = P.evs[m].fim; p->titulo = P.evs[m].tit; ok = 1;
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

// --- teste ----------------------------------------------------------------------------
void epg_teste_limpar(void) {
  pthread_mutex_lock(&trava);
  free(P.evs); free(P.arena); memset(&P, 0, sizeof P);
  free(W.evs); free(W.arena); memset(&W, 0, sizeof W);
  estado = EPG_PARADO; fioVivo = 0; pendPronto = 0;
  pthread_mutex_unlock(&trava);
}

int epg_xml_processar(char *xml) {
  int n;
  epg_teste_limpar();
  W.capEvs = 90000; W.evs = malloc(sizeof(EpgEv) * (size_t)W.capEvs);
  W.arenaCap = 8L << 20; W.arena = malloc((size_t)W.arenaCap);
  if (!W.evs || !W.arena) return -1;
  n = wProcessar(&W, xml);
  wFechar(&W);
  pthread_mutex_lock(&trava);
  free(P.evs); free(P.arena);
  P = W; memset(&W, 0, sizeof W);
  estado = EPG_PRONTO;
  pthread_mutex_unlock(&trava);
  return n;
}
