// EPG: grade XMLTV externa para os canais ao vivo. Ver epg.h para o porque.
//
// FLUXO:
//   epg_iniciar()     -> fio baixa BR1+BR2 (.gz) do epgshare01, infla com zlib,
//                        grava o XML na pasta de dados e processa.
//   epg_passo()       -> no fio de desenho: publica a grade pronta e pede
//                        re-carga quando o cache passou de 12 h.
//   epg_match(nome)   -> nome do addon -> indice do canal na grade, ou -1.
//   epg_agora/proximo -> programa no ar e os seguintes.
//   epg_janela/faixa  -> quanto a grade cobre e os programas de um intervalo
//                        (grade de varios dias no guia).
//
// QUANTO A FONTE DA (MEDIDO em 2026-09-18 nos cinco XML em ~/.nuvio, baixados
// em 2026-09-16): cada arquivo cobre de 3,3 a 3,8 dias A FRENTE do download e
// de 0,3 a 1,8 dias para tras; as cinco fontes terminavam no MESMO instante
// (2026-09-20 04:28 UTC) apesar de baixadas em horas diferentes — o horizonte
// e do publicador, nao do relogio de quem baixa. Uma semana NAO esta no
// arquivo; o que se pode oferecer ao guia e a janela inteira que vem, e este
// modulo ja retinha tudo que e futuro. 801 canais e 72257 programas brutos
// nos cinco arquivos; 797 canais / 59297 programas publicados com arquivo
// fresco (duplicatas BR1/BR2 e o corte de passado explicam a diferenca).
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
#include <strings.h>

// 2400 e nao 1400 (#158): as cinco de sempre ja davam 797 canais, e RO1+RO2
// somam mais 518.
#define EPG_MAX_CANAL 2400
#define EPG_MAX_CHAVES 6
// A grade fala ~3,5 dias para a frente; renovar duas vezes por dia cobre a
// virada sem baixar ~1,4 MB a cada abertura do app.
#define EPG_CACHE_SEG (12 * 3600)
// Evento que terminou ha mais tempo que isto e descartado na carga. Era 2 h
// quando o guia so mostrava "agora/a seguir"; com a grade de varios dias o
// dia de ontem interessa. 48 h e um TETO para cache velho de outra sessao
// (o arquivo em si nunca passou de 1,8 dias para tras — medido), nao um
// recorte do que a fonte entrega.
#define EPG_JANELA_PASSADO (48 * 3600)
// Os vetores da grade CRESCEM sob demanda a partir destes tamanhos e param
// nestes tetos. Medido com arquivo fresco: ~72 mil insercoes brutas e 1,46 MB
// de titulos; os tetos dao ~5x de folga e limitam o dano no heap fixo de
// 256 MiB do Tizen se uma fonte inchar (programa alem do teto e descartado
// com uma linha de log, nunca estouro). Antes eram 90000 evs e 8 MiB de
// arena alocados FIXOS por grade — 11,3 MB, duas vezes durante a troca W/P,
// para ~3 MB de dados.
#define EPG_EVS_INI   32768
#define EPG_EVS_MAX   400000
#define EPG_ARENA_INI (1L << 20)
#define EPG_ARENA_MAX (16L << 20)

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

typedef struct {
  char id[96];
  char nome[96];                      // display-name, para log e depuracao
  char chaves[EPG_MAX_CHAVES][96];    // variantes normalizadas de nome/id
  int  nChaves;
  long evIni, evN;                    // janela no vetor de eventos
} EpgCanal;

// `tit` e DESLOCAMENTO na arena, nao ponteiro: a arena cresce por realloc
// durante a carga e um ponteiro guardado aqui apontaria para o bloco velho.
// O ponteiro e resolvido na consulta (P.arena + tit), quando a arena ja nao
// muda. Custo MEDIDO no Mac (64 bits): 32 B por evento, mais 20,2 B de titulo
// na arena em media (1464717 B / 72257 programas). MEDIDO tambem no wasm32
// com o emsdk upstream 6.0.9: 32 B (time_t de 8 bytes alinha a 8). NAO
// medido: o fork Emscripten da Samsung que o tizen.sh usa, e o ARM 32 bits
// da LG (16 B se o time_t de la for de 4 bytes — suposicao).
typedef struct {
  int    canal;
  time_t ini, fim;
  long   tit;
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

// Reserva os vetores iniciais de uma grade em construcao. 0 = sem memoria.
static int wAbrir(EpgGrade *g) {
  memset(g, 0, sizeof *g);
  g->capEvs = EPG_EVS_INI;
  g->evs = malloc(sizeof(EpgEv) * (size_t)g->capEvs);
  g->arenaCap = EPG_ARENA_INI;
  g->arena = malloc((size_t)g->arenaCap);
  if (!g->evs || !g->arena) { free(g->evs); free(g->arena); g->evs = NULL; g->arena = NULL; return 0; }
  return 1;
}

// Garante lugar para mais um evento, dobrando ate o teto. 0 = teto ou sem
// memoria; quem chamou descarta o programa (e a grade continua valida).
static int wReservarEv(EpgGrade *g) {
  if (g->nEvs < g->capEvs) return 1;
  if (g->capEvs >= EPG_EVS_MAX) return 0;
  { int cap = g->capEvs * 2 > EPG_EVS_MAX ? EPG_EVS_MAX : g->capEvs * 2;
    EpgEv *no = realloc(g->evs, sizeof(EpgEv) * (size_t)cap);
    if (!no) return 0;
    g->evs = no; g->capEvs = cap; }
  return 1;
}

// Copia o titulo para a arena e devolve o deslocamento, ou -1 se nao coube
// (teto ou sem memoria). Dobra a arena quando precisa — por isso quem guarda
// o resultado guarda deslocamento, nunca ponteiro (ver EpgEv).
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
// FILTRO DE CANAIS da grade do provedor (#158). O xmltv.php traz a grade de
// TODOS os canais do painel — dezenas de milhares num provedor europeu — e o
// PASSO 1 abaixo para no EPG_MAX_CANAL: os canais que a pessoa tem podiam
// ficar de fora so por virem depois no arquivo. Com o filtro, so entram os
// ids que o Xtream deu como epg_channel_id. NULL = sem filtro (epgshare01).
static char (*filtroIds)[64];
static int nFiltroIds;
static int cmpId(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

static int wProcessarF(EpgGrade *g, char *xml, char (*filtro)[64], int nFiltro);
static int wProcessar(EpgGrade *g, char *xml) { return wProcessarF(g, xml, NULL, 0); }
static int wProcessarF(EpgGrade *g, char *xml, char (*filtro)[64], int nFiltro) {
  char *p = xml, *fecha;
  int n = 0, descartados = 0;
  time_t agora = time(NULL);

  // PASSO 1: canais.
  while ((p = strstr(p, "<channel "))) {
    char id[96];
    int i;
    fecha = strstr(p, "</channel>");
    if (!fecha) break;
    if (achaAtr(p, fecha, "id", id, sizeof id) &&
        (!filtro || bsearch(id, filtro, (size_t)nFiltro, sizeof filtro[0], cmpId))) {
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
      if (a && b && a < b) {
        // Titulo maior que isto nao existe na fonte (maximo medido: 106 B);
        // o corte e so protecao contra XML malformado.
        char titulo[240]; int tn = (int)(b - (a + 1));
        long copia;
        if (tn > (int)sizeof titulo - 1) tn = sizeof titulo - 1;
        memcpy(titulo, a + 1, (size_t)tn); titulo[tn] = 0;
        desentidade(titulo);
        if (!wReservarEv(g) || (copia = wTitulo(g, titulo)) < 0) {
          descartados++;
        } else {
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
  if (descartados) {
    // Bateu no teto (EPG_EVS_MAX/EPG_ARENA_MAX) ou faltou memoria. Uma linha,
    // nao uma por programa: o sintoma no guia e "grade acaba antes do fim".
    printf("[epg] %d programas descartados: teto de memoria da grade\n", descartados);
    fflush(stdout);
  }
  return n;
}

// --- carga: disco, rede, gzip -------------------------------------------------------
// O buffer de saida CRESCE conforme o inflate avanca — uma estimativa fixa
// (gz*10) estourava nas fontes regionais maiores (PT1 comprime ~11x).
// `teto` 0 = sem teto; acima dele o inflate para e devolve NULL (a grade do
// provedor pode vir com dezenas de MB, ver epg_fonte_extra).
static char *desgzip(const char *buf, long n, long *nOut, long teto) {
  z_stream z; long cap = n * 6 + (1 << 20), feito = 0;
  if (teto > 0 && cap > teto + (1 << 16)) cap = teto + (1 << 16);
  char *out = malloc((size_t)cap);
  if (!out) return NULL;
  memset(&z, 0, sizeof z);
  if (inflateInit2(&z, 16 + 15) != Z_OK) { free(out); return NULL; }
  z.next_in = (Bytef *)buf; z.avail_in = (uInt)n;
  for (;;) {
    if (cap - feito < (1 << 16)) {                 // folga minima: dobra
      char *no;
      if (teto > 0 && feito > teto) { inflateEnd(&z); free(out); return NULL; }
      no = realloc(out, (size_t)(cap * 2));
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

// O CACHE GUARDA O .gz COMO VEIO DA REDE, e nao o XML aberto (23/09/2026).
//
// No Tizen a pasta de dados e IDBFS: a descarga seguinte (dados.c) entrega ao
// IndexedDB o CONTEUDO INTEIRO de cada arquivo mudado, clonado de uma vez numa
// tarefa do FIO PRINCIPAL (IDBFS.reconcile -> store.put, no callback do
// getRemoteSet — fora ate do `idbfs=N/X ms` do log, que so mede a varredura).
// Com o XML aberto eram as cinco grades inteiras, ~10x o gzip (o comentario do
// topo fala em ~1,4 MB de gzip). O log da Samsung 1.4.1 (registro 1638) tem a
// descarga logo depois da grade nova custando 1157 ms so na parte medida, a
// maior de toda a sessao, e o FPS caindo de 25 para 0,4 em seguida. Gravando
// o .gz, o fio principal clona o que veio da rede; o custo de abrir o gzip de
// novo (inflate, ~dezenas de ms) fica no fio da grade, que ja o pagava na
// primeira carga.
//
// Escrita direta no caminho, sob a trava de FS que o Tizen exige.
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

// Le o .gz do cache (binario: dados_ler corta no primeiro NUL).
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

// gzip -> XML com o NUL no fim (o parser trata como texto).
static char *abrirGz(const char *gz, long ngz, long *nOut) {
  char *xml = desgzip(gz, ngz, nOut, 0), *c;
  if (!xml) return NULL;
  c = realloc(xml, (size_t)*nOut + 1);
  if (!c) { free(xml); return NULL; }
  c[*nOut] = 0;
  return c;
}

static char *obterXml(const char *arq, long *nOut) {
  char *xml;
  long ngz = 0;
  char *gz, url[160], nXml[40], nGz[40], nTs[40];
  nomesFonte(arq, url, sizeof url, nXml, nGz, nTs, sizeof nXml);
  // O cache antigo, de XML aberto, sai: no Tizen ele era a maior descarga do
  // IDBFS. Sem arquivo, dados_apagar nao faz nada. Uma vez por arquivo e
  // processo (os nomes cabem numa lista curta).
  { static char limpou[16][8]; static int nLimpou;
    int k;
    for (k = 0; k < nLimpou && strcmp(limpou[k], arq); k++) {}
    if (k == nLimpou && nLimpou < 16) {
      snprintf(limpou[nLimpou++], sizeof limpou[0], "%s", arq);
      dados_apagar(nXml);
    } }
  // Cache fresco primeiro: a abertura nao paga a rede quando o arquivo do dia
  // ja esta no aparelho.
  if (arquivoIdade(nTs) < EPG_CACHE_SEG) {
    gz = lerBin(nGz, &ngz);
    xml = gz ? abrirGz(gz, ngz, nOut) : NULL;
    free(gz);
    if (xml && xml[0] == '<') return xml;
    free(xml);
  }
  gz = rede_baixar_bin(url, 60, &ngz);
  if (!gz) { printf("[epg] %s: sem resposta\n", nXml); fflush(stdout); return NULL; }
  xml = abrirGz(gz, ngz, nOut);
  if (xml && xml[0] == '<') gravarBin(nGz, nTs, gz, ngz);
  free(gz);
  return xml;
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

// --- a grade do PROPRIO PROVEDOR (#158) --------------------------------------
//
// As cinco fontes acima sao brasileiras e latinas. No #158 (LG C4, Xtream com
// canais romenos) o guia mostrava 797 canais de grade e nenhum programa: nada
// ali cobre a Romenia, e o epg_channel_id que o Xtream manda em cada canal so
// existe no XMLTV do proprio provedor (<servidor>/xmltv.php), que o app nao
// baixava. Esta e a sexta fonte, opcional, definida por quem sabe dela
// (guia.c, com o cadastro do Xtream).
//
// A URL LEVA USUARIO E SENHA: nunca sai em log, e no disco fica so um hash
// dela (para saber se o cache e desta fonte), nunca ela.
//
// TAMANHO: um provedor grande entrega dezenas de MB. Acima do teto a grade e
// ignorada com uma linha de log — melhor sem grade do provedor que o app
// fechando por memoria (no Tizen o heap inteiro e 256 MiB).
#ifdef __EMSCRIPTEN__
#define EPG_EXTRA_TETO_REDE (12L << 20)
#define EPG_EXTRA_TETO_XML  (48L << 20)
#else
#define EPG_EXTRA_TETO_REDE (32L << 20)
#define EPG_EXTRA_TETO_XML  (128L << 20)
#endif
static const char *EXTRA_BIN = "epg-extra.bin";
static const char *EXTRA_TS  = "epg-extra.ts";
static const char *EXTRA_ID  = "epg-extra.id";
static char extraUrl[1100];
static volatile int extraMudou;

static void hashUrl(const char *u, char *dst, size_t tam) {
  unsigned long long h = 1469598103934665603ULL;
  for (; *u; u++) { h ^= (unsigned char)*u; h *= 1099511628211ULL; }
  snprintf(dst, tam, "%016llx", h);
}

// Bytes da rede (gzip ou XML puro, o xmltv.php manda os dois conforme o
// painel) -> XML terminado em NUL, ou NULL.
static char *abrirExtra(const char *b, long n, long *nOut) {
  char *xml;
  if (n > 2 && (unsigned char)b[0] == 0x1f && (unsigned char)b[1] == 0x8b) {
    xml = desgzip(b, n, nOut, EPG_EXTRA_TETO_XML);
    if (xml) { char *c = realloc(xml, (size_t)*nOut + 1);
               if (!c) { free(xml); return NULL; }
               xml = c; xml[*nOut] = 0; }
  } else {
    xml = malloc((size_t)n + 1);
    if (xml) { memcpy(xml, b, (size_t)n); xml[n] = 0; *nOut = n; }
  }
  if (!xml) return NULL;
  { long k = 0;                                 // BOM e espaco antes do '<'
    while (k < *nOut && k < 64 && xml[k] != '<') k++;
    if (k >= *nOut || xml[k] != '<') { free(xml); return NULL; }
    if (k) { memmove(xml, xml + k, (size_t)(*nOut - k) + 1); *nOut -= k; } }
  return xml;
}

static char *obterExtra(const char *url, long *nOut) {
  char id[24], *b, *xml, *velho;
  long n = 0;
  RedeControle ctl;
  RedeMedida med;
  hashUrl(url, id, sizeof id);
  velho = dados_ler(EXTRA_ID);
  if (velho && !strcmp(velho, id) && arquivoIdade(EXTRA_TS) < EPG_CACHE_SEG) {
    b = lerBin(EXTRA_BIN, &n);
    xml = b ? abrirExtra(b, n, nOut) : NULL;
    free(b);
    if (xml) { free(velho); return xml; }
  }
  free(velho);
  memset(&ctl, 0, sizeof ctl);
  memset(&med, 0, sizeof med);
  ctl.max_bytes = EPG_EXTRA_TETO_REDE;
  b = rede_baixar_bin_medido_controle(url, 120, NULL, &ctl, &n, &med);
  if (!b || med.limitado) {
    // O teto vai no log (#158): "maior que o teto" sem o numero nao dizia se
    // faltava pouco ou muito. No registro 6311 foi esta linha, e dali para a
    // frente a grade do provedor nunca entrou — a grade curta por canal
    // (get_short_epg, ver guia.c) e o epgshare01 do pais cobrem esse caso.
    printf("[epg] grade do provedor: %s (teto %ld MB, %ld ms)\n",
           med.limitado ? "maior que o teto, ignorada"
                        : med.status ? "resposta sem corpo" : "sem resposta",
           (long)(EPG_EXTRA_TETO_REDE >> 20), (long)med.ms);
    if (med.status && med.status != 200) printf("[epg] grade do provedor: HTTP %d\n", med.status);
    fflush(stdout);
    free(b);
    return NULL;
  }
  xml = abrirExtra(b, n, nOut);
  if (xml && gravarBin(EXTRA_BIN, EXTRA_TS, b, n)) dados_gravar_leve(EXTRA_ID, id);
  if (!xml) { printf("[epg] grade do provedor: %ld B que nao sao XMLTV\n", n); fflush(stdout); }
  free(b);
  return xml;
}

void epg_fonte_extra_ids(const char *const *ids, int n) {
  char (*novo)[64] = NULL;
  int i, k = 0;
  if (ids && n > 0) novo = malloc(sizeof *novo * (size_t)n);
  if (novo)
    for (i = 0; i < n; i++)
      if (ids[i] && ids[i][0]) snprintf(novo[k++], sizeof novo[0], "%s", ids[i]);
  if (novo && k) qsort(novo, (size_t)k, sizeof novo[0], cmpId);
  pthread_mutex_lock(&trava);
  free(filtroIds);
  filtroIds = k ? novo : NULL;
  nFiltroIds = k;
  if (!k) free(novo);
  pthread_mutex_unlock(&trava);
}

void epg_fonte_extra(const char *url) {
  if (!url) url = "";
  pthread_mutex_lock(&trava);
  if (strcmp(url, extraUrl)) {
    snprintf(extraUrl, sizeof extraUrl, "%s", url);
    extraMudou = 1;
  }
  pthread_mutex_unlock(&trava);
}

static void *fioEpg(void *u) {
  int i, ok = 0;
  char extra[sizeof extraUrl];
  (void)u;
  char arq[EPG_MAX_FONTES][8];
  int nArq;
  pthread_mutex_lock(&trava);
  snprintf(extra, sizeof extra, "%s", extraUrl);
  extraMudou = 0;
  fontesMudaram = 0;
  if (nFontesArq < 0) {
    for (nArq = 0; nArq < (int)(sizeof PADRAO_ARQ / sizeof PADRAO_ARQ[0]); nArq++)
      snprintf(arq[nArq], sizeof arq[0], "%s", PADRAO_ARQ[nArq]);
  } else {
    for (nArq = 0; nArq < nFontesArq; nArq++) snprintf(arq[nArq], sizeof arq[0], "%s", fontesArq[nArq]);
  }
  pthread_mutex_unlock(&trava);
  if (!wAbrir(&W)) { pendPronto = 1; pendOk = 0; return NULL; }
  for (i = 0; i < nArq; i++) {
    long n = 0;
    char *xml = obterXml(arq[i], &n);
    if (xml) {
      char nXml[40];
      int achou = wProcessar(&W, xml);
      nomesFonte(arq[i], NULL, 0, nXml, NULL, NULL, sizeof nXml);
      printf("[epg] %s: %d programas\n", nXml, achou);
      fflush(stdout);
      free(xml);
      ok = 1;
    }
  }
  if (extra[0]) {
    long n = 0;
    char *xml = obterExtra(extra, &n);
    if (xml) {
      // O filtro e copiado sob a trava: o fio do guia pode troca-lo durante
      // o processamento.
      char (*f)[64] = NULL; int nf = 0, achou;
      pthread_mutex_lock(&trava);
      if (filtroIds && nFiltroIds > 0 && (f = malloc(sizeof *f * (size_t)nFiltroIds)) != NULL) {
        memcpy(f, filtroIds, sizeof *f * (size_t)nFiltroIds); nf = nFiltroIds; }
      pthread_mutex_unlock(&trava);
      achou = wProcessarF(&W, xml, f, nf);
      free(f);
      printf("[epg] grade do provedor: %d programas (%ld KB)\n", achou, n / 1024);
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
    // A cobertura sai no log para a proxima medicao nao depender de script:
    // "+3,4 d" e o horizonte que a fonte entregou nesta carga.
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
  // Grade velha e o fio parado: tenta de novo quando o relogio diz que o cache
  // envelheceu (o download decide sozinho se reusa o arquivo ou baixa de novo).
  if (estado == EPG_PRONTO && !fioVivo &&
      time(NULL) - carregadaEm > EPG_CACHE_SEG)
    epg_iniciar();
  // A grade do provedor chegou (ou saiu) depois da carga: refaz. As cinco
  // fontes fixas vem do cache do disco, so a nova vai a rede.
  else if ((extraMudou || fontesMudaram) && !fioVivo &&
           (estado == EPG_PRONTO || estado == EPG_FALHOU))
    epg_iniciar();
}

// A grade lista canais SEM programa: a BR1 real tem 266 <channel> e so 105
// com <programme> (medido em 2026-09-18). "Globo RJ" casava exato com
// "São.Paulo/SP..Globo.HD.br", que nao tem grade, enquanto "Globo.br" — a
// MESMA chave "globo" — tinha 3,5 dias de programacao; o guia mostrava
// "AO VIVO" para a Globo. Quando a regra devolve um canal vazio, este passo
// procura outro canal com a mesma chave que tenha programa; se nao ha, fica
// o vazio (o teste de molde casa canais sem programa de proposito, e um
// casamento vazio nao e pior que nenhum). Chamar com a trava tomada.
static int comGrade(int i, const char *chave) {
  int j, c;
  if (i < 0 || P.canais[i].evN > 0 || !chave) return i;
  for (j = 0; j < P.nCanais; j++) {
    if (P.canais[j].evN <= 0) continue;
    for (c = 0; c < P.canais[j].nChaves; c++)
      if (!strcmp(chave, P.canais[j].chaves[c])) return j;
  }
  return i;
}

int epg_match_id(const char *id) {
  int i, r = -1;
  if (estado != EPG_PRONTO || !id || !id[0]) return -1;
  pthread_mutex_lock(&trava);
  for (i = 0; i < P.nCanais; i++)
    if (!strcmp(P.canais[i].id, id)) {
      // Canal da grade sem programa nao e resposta: o nome ainda pode casar
      // com outro que tenha (ver comGrade).
      if (P.canais[i].evN > 0) { r = i; break; }
    }
  pthread_mutex_unlock(&trava);
  return r;
}

// GUARDA DE NUMERO (#158): o casamento por prefixo NAO pode parar no meio de
// um numero. "Pro TV 2" (chave "protv2") herdava a grade de "Pro TV"
// ("protv") pela regra 3, e "Digi Sport 1" a de "Digi Sport" — outro canal,
// outra programacao. Verdadeiro quando o caractere depois do corte e digito e
// o de antes tambem, ou o de depois e digito e a parte comum termina em
// letra (o numero e do canal, nao da rede).
static int cortaNumero(const char *k, long corte) {
  return k[corte] >= '0' && k[corte] <= '9';
}

int epg_match(const char *nome) {
  char k1[96], k2[96], prim[48];
  int i, j, c, melhor = -1;
  long melhorTam = 0, t;
  const char *chave = NULL;             // a chave da grade que casou
  if (estado != EPG_PRONTO || !nome || !nome[0]) return -1;
  normChave(nome, k1, sizeof k1, 0, NULL, 0);
  normChave(nome, k2, sizeof k2, 1, prim, sizeof prim);
  if (!k1[0]) return -1;

  pthread_mutex_lock(&trava);
  #define DEVOLVE(idx, kv) do { int r_ = comGrade((idx), (kv)); \
    pthread_mutex_unlock(&trava); return r_; } while (0)
  // 1. exata
  for (i = 0; i < P.nCanais; i++)
    for (j = 0; j < P.canais[i].nChaves; j++)
      if (!strcmp(k1, P.canais[i].chaves[j]) ||
          (k2[0] && !strcmp(k2, P.canais[i].chaves[j])))
        DEVOLVE(i, P.canais[i].chaves[j]);

  // 2. apelido
  for (j = 0; j < (int)(sizeof ALIAS / sizeof ALIAS[0]); j++)
    if (!strcmp(k1, ALIAS[j][0]) || (k2[0] && !strcmp(k2, ALIAS[j][0]))) {
      for (i = 0; i < P.nCanais; i++)
        for (c = 0; c < P.canais[i].nChaves; c++)
          if (!strcmp(ALIAS[j][1], P.canais[i].chaves[c]))
            DEVOLVE(i, ALIAS[j][1]);
    }

  // 3. a grade e prefixo do canal ("recordtvpaulista" -> "recordtv")
  t = (long)strlen(k1);
  for (i = 0; i < P.nCanais; i++)
    for (j = 0; j < P.canais[i].nChaves; j++) {
      long tc = (long)strlen(P.canais[i].chaves[j]);
      if (tc >= 4 && tc < t &&
          !strncmp(k1, P.canais[i].chaves[j], (size_t)tc) && tc > melhorTam &&
          !cortaNumero(k1, tc))
        { melhor = i; melhorTam = tc; chave = P.canais[i].chaves[j]; }
    }
  if (melhor >= 0) DEVOLVE(melhor, chave);

  // 4. o canal e prefixo de UMA SO chave da grade (ambigua nao casa)
  { int nCand = 0;
    if (t >= 4)
      for (i = 0; i < P.nCanais; i++)
        for (j = 0; j < P.canais[i].nChaves; j++)
          if ((long)strlen(P.canais[i].chaves[j]) > t &&
              !strncmp(P.canais[i].chaves[j], k1, (size_t)t) && melhor != i &&
              !cortaNumero(P.canais[i].chaves[j], t)) {
            melhor = i; nCand++; chave = P.canais[i].chaves[j];
          }
    if (nCand == 1) DEVOLVE(melhor, chave); }

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
        { const char *em = strstr(k1, kv);
          if (!em && k2[0]) em = strstr(k2, kv);
          if (!em) continue;
          // A guarda de numero vale aqui tambem: "digisport1" nao herda
          // "digisport" pelo meio do nome.
          if (cortaNumero(em, tc)) continue; }
        if (tc > melhorTc) { melhorTc = tc; melhor = i; melhorChave = kv; ambig = 0; }
        else if (tc == melhorTc && i != melhor &&
                 !(melhorChave && !strcmp(melhorChave, kv)))
          ambig = 1;
      }
    if (melhor >= 0 && !ambig) DEVOLVE(melhor, melhorChave); }

  // 6. o primeiro token util do canal E a chave inteira da grade: e a regra
  // da afiliada regional — "SBT RJ"/"SBT Thathi Vale" herdam a grade da
  // rede "sbt". Minimo 3 letras para sigla de rede nao colar em qualquer um.
  if (prim[0] && (long)strlen(prim) >= 3)
    for (i = 0; i < P.nCanais; i++)
      for (j = 0; j < P.canais[i].nChaves; j++)
        if (!strcmp(prim, P.canais[i].chaves[j]))
          DEVOLVE(i, P.canais[i].chaves[j]);
  #undef DEVOLVE
  pthread_mutex_unlock(&trava);
  return -1;
}

// Copia o evento m da grade publicada para *p, resolvendo o titulo na arena.
// Chamar com a trava tomada.
static void pubProg(long m, EpgProg *p) {
  p->ini = P.evs[m].ini; p->fim = P.evs[m].fim; p->titulo = P.arena + P.evs[m].tit;
}

// Primeiro evento do canal que ainda nao terminou em `t` (fim > t), ou -1.
// Os eventos de um canal estao ordenados por inicio e, na pratica, nao se
// sobrepoem (a fonte emite uma sequencia), entao "fim > t" e monotono e a
// busca binaria vale. Chamar com a trava tomada.
static long pubPrimeiroVivo(int epg, time_t t) {
  long lo = P.canais[epg].evIni, hi = lo + P.canais[epg].evN - 1, m = -1;
  while (lo <= hi) {
    long mid = (lo + hi) >> 1;
    if (P.evs[mid].fim <= t) lo = mid + 1;
    else { m = mid; hi = mid - 1; }
  }
  return m;
}

int epg_agora(int epg, time_t agora, EpgProg *p) {
  long m;
  int ok = 0;
  if (epg < 0 || epg >= P.nCanais) return 0;
  pthread_mutex_lock(&trava);
  m = pubPrimeiroVivo(epg, agora);
  if (m >= 0 && P.evs[m].ini <= agora) { pubProg(m, p); ok = 1; }
  pthread_mutex_unlock(&trava);
  return ok;
}

int epg_proximo(int epg, time_t agora, int k, EpgProg *p) {
  long m, topo;
  int ok = 0;
  if (epg < 0 || epg >= P.nCanais || k < 0) return 0;
  pthread_mutex_lock(&trava);
  topo = P.canais[epg].evIni + P.canais[epg].evN;
  m = pubPrimeiroVivo(epg, agora);
  // m = programa no ar (ou o primeiro futuro); o "proximo" pula o que esta no ar
  if (m >= 0 && P.evs[m].ini <= agora) m++;
  if (m >= 0) {
    m += k;
    if (m < topo) { pubProg(m, p); ok = 1; }
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

int epg_janela(int epg, time_t *ini, time_t *fim) {
  int ok = 0;
  if (epg < 0 || epg >= P.nCanais) return 0;
  pthread_mutex_lock(&trava);
  if (P.canais[epg].evN > 0) {
    long a = P.canais[epg].evIni, b = a + P.canais[epg].evN - 1;
    // O ultimo por INICIO e quase sempre o ultimo por fim; se a fonte trouxer
    // um evento longo antes de um curto, `fim` sai menor do que o real — erro
    // para o lado conservador ("cobre menos"), nunca afirma cobertura que
    // nao tem.
    if (ini) *ini = P.evs[a].ini;
    if (fim) *fim = P.evs[b].fim;
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
      if (!ok || P.evs[x].ini < a) a = P.evs[x].ini;
      if (!ok || P.evs[y].fim > b) b = P.evs[y].fim;
      ok = 1; }
  }
  pthread_mutex_unlock(&trava);
  if (ini) *ini = a;
  if (fim) *fim = b;
  return ok;
}

int epg_total(int epg) {
  if (epg < 0 || epg >= P.nCanais) return 0;
  return (int)P.canais[epg].evN;
}

// `pular`: quantos da sequencia (a mesma de epg_faixa, em ordem do indice) ja
// foram entregues — o cursor de quem pagina e POSICAO, nao horario: a XMLTV nao
// rejeita sobreposicao e o fim dos programas nao e monotono (#344).
int epg_faixa_desde(int epg, time_t de, time_t ate, int pular, EpgProg *out, int cap) {
  long m, topo;
  int n = 0, v = 0;
  if (epg < 0 || epg >= P.nCanais || ate <= de || (out && cap <= 0)) return 0;
  if (pular < 0) pular = 0;
  pthread_mutex_lock(&trava);
  topo = P.canais[epg].evIni + P.canais[epg].evN;
  m = pubPrimeiroVivo(epg, de);           // primeiro com fim > de
  if (m >= 0)
    for (; m < topo && P.evs[m].ini < ate && (!out || n < cap); m++) {
      if (v++ < pular) continue;
      if (out) pubProg(m, &out[n]);
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
  free(P.evs); free(P.arena); memset(&P, 0, sizeof P);
  free(W.evs); free(W.arena); memset(&W, 0, sizeof W);
  estado = EPG_PARADO; fioVivo = 0; pendPronto = 0;
  pthread_mutex_unlock(&trava);
}

int epg_xml_processar(char *xml) {
  int n;
  epg_teste_limpar();
  if (!wAbrir(&W)) return -1;
  n = wProcessar(&W, xml);
  wFechar(&W);
  pthread_mutex_lock(&trava);
  free(P.evs); free(P.arena);
  P = W; memset(&W, 0, sizeof W);
  estado = EPG_PRONTO;
  pthread_mutex_unlock(&trava);
  return n;
}
