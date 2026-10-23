// A fonte escolhida a mao, lembrada por titulo e por perfil. Ver a nota longa
// em fontepref.h — em especial a ordem de casamento (bingeGroup declarado pelo
// addon, depois provedor + trilha de audio), por que a chave nao pode ser a
// url, e por que a preferencia vence.
#include "fontepref.h"
#include "idbase.h"
#include "dados.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

static FontePref tabela[FONTEPREF_MAX];
static int nTab;
static int carregado;
static int perfil;
static pthread_mutex_t travaTocando = PTHREAD_MUTEX_INITIALIZER;
static FontePref tocandoPref;

static void esquecerTocando(void) {
  pthread_mutex_lock(&travaTocando);
  memset(&tocandoPref, 0, sizeof tocandoPref);
  pthread_mutex_unlock(&travaTocando);
}

// A MEMORIA DO AUTOMATICO (ver fontepref.h): tabela e arquivo proprios.
// `sessao` so existe em RAM (o arquivo nao tem a coluna): 1 = gravada por este
// processo. E o que "Reusar o grupo entre sessoes" desligado olha — por marca e
// nao por relogio, porque esta TV arranca com a hora errada (ver venceu()).
// A fonte que o AUTOMATICO tocou com sucesso guarda o MESMO registro de uma
// escolha a mao (dono, 16/10: "testada e segura pelo automatico" vale como
// preferencia quando nao ha escolha a mao em vigor).
typedef struct { FontePref p; int sessao; } AutoPref;
static AutoPref tabAuto[FONTEPREF_MAX];
static int nAuto;

// OS QUATRO AJUSTES de Reproducao (fontepref_configurar). Os valores iniciais
// sao o comportamento de antes deles existirem.
static int cfgReusar = 1, cfgBinge = 1, cfgBingeSessoes = 1;
static long long cfgValidadeS = FONTEPREF_VALIDADE_S;

void fontepref_configurar(int reusar, long long validadeS, int preferirGrupo, int grupoEntreSessoes) {
  cfgReusar = reusar ? 1 : 0;
  cfgValidadeS = validadeS > 0 ? validadeS : FONTEPREF_VALIDADE_S;
  cfgBinge = preferirGrupo ? 1 : 0;
  cfgBingeSessoes = grupoEntreSessoes ? 1 : 0;
}

// O QUE ESTE PROCESSO GRAVOU na memoria do automatico, para a marca `sessao`
// sobreviver a uma troca de perfil (que rele o arquivo). Cheia, a mais antiga
// sai; perder uma marca so faz "entre sessoes desligado" esquecer um titulo.
#define SESSAO_MAX 64
static struct { int perfil; char id[24]; } sessaoGravou[SESSAO_MAX];
static int nSessao;
static void sessaoMarcar(const char *id) {
  int i;
  for (i = 0; i < nSessao; i++)
    if (sessaoGravou[i].perfil == perfil && !strcmp(sessaoGravou[i].id, id)) return;
  if (nSessao == SESSAO_MAX) { memmove(sessaoGravou, sessaoGravou + 1, sizeof sessaoGravou[0] * (SESSAO_MAX - 1)); nSessao--; }
  sessaoGravou[nSessao].perfil = perfil;
  snprintf(sessaoGravou[nSessao].id, sizeof sessaoGravou[nSessao].id, "%s", id);
  nSessao++;
}
static int sessaoTem(const char *id) {
  int i;
  for (i = 0; i < nSessao; i++)
    if (sessaoGravou[i].perfil == perfil && !strcmp(sessaoGravou[i].id, id)) return 1;
  return 0;
}

// A memoria do automatico pode ser usada agora? Prefer binge group desligado:
// nunca. Reusar entre sessoes desligado: so o que este processo gravou.
static int autoUsavel(int k) {
  long long agora = (long long)time(NULL), q;
  if (!cfgBinge || k < 0 || k >= nAuto) return 0;
  if (!cfgBingeSessoes && !tabAuto[k].sessao) return 0;
  q = tabAuto[k].p.quandoS;
  return !(q > 0 && agora > q && agora - q > FONTEPREF_VALIDADE_S);
}

// UM ARQUIVO POR PERFIL (fontepref-p<N>.txt), como fileirasui-p<N>.txt em
// fileiras.c e pelo mesmo motivo: sem a separacao, a escolha de audio de um
// perfil mandaria na reproducao do outro — que e o defeito que a separacao das
// fileiras existiu para consertar. p0 e "ninguem escolheu perfil ainda", que e
// o caso de quem nao tem conta.
//
// FONTEPREF_PERFIS e ate onde fontepref_esquecer varre. CONTA_PERFIL_MAX e 8
// hoje (perfis.h); 16 cobre o dobro sem custo — sao dados_apagar em arquivo
// que nao existe.
#define FONTEPREF_PERFIS 16

static const char *arquivoDoPerfil(void) {
  static char nome[48];
  snprintf(nome, sizeof nome, "fontepref-p%d.txt", perfil);
  return nome;
}

static const char *arquivoAuto(void) {
  static char nome[48];
  snprintf(nome, sizeof nome, "fontepref-auto-p%d.txt", perfil);
  return nome;
}

// --- assinatura de idioma/audio ---------------------------------------------
//
// BANDEIRA E DUAS LETRAS, e e por isso que ela e decodificada em vez de
// comparada como texto. 🇧🇷 sao dois "regional indicator": U+1F1E7 U+1F1F7, em
// UTF-8 F0 9F 87 A7 e F0 9F 87 B7. O ultimo byte menos 0xA6 da a letra ASCII.
// Decodificar transforma a bandeira num codigo comparavel ("BR") em vez de
// exigir uma tabela de 250 sequencias de bytes.
#define RI_BASE 0xA6

static int bandeira(const unsigned char *p, char *dst) {
  if (p[0] != 0xF0 || p[1] != 0x9F || p[2] != 0x87) return 0;
  if (p[3] < 0xA6 || p[3] > 0xBF) return 0;
  if (p[4] != 0xF0 || p[5] != 0x9F || p[6] != 0x87) return 0;
  if (p[7] < 0xA6 || p[7] > 0xBF) return 0;
  dst[0] = (char)('A' + (p[3] - RI_BASE));
  dst[1] = (char)('A' + (p[7] - RI_BASE));
  dst[2] = 0;
  return 1;
}

// TERMOS ESCRITOS. A lista e curta de proposito: cada entrada a mais e uma
// chance de duas fontes que eram iguais deixarem de casar por causa de uma
// palavra que so uma delas traz. Entram as marcas que os addons realmente
// escrevem no `name`/`description` e que uma pessoa notaria trocada.
//
// "dublado" vira DUB e nao POR: dublado diz que HA dublagem, nao em que
// idioma. "nacional", no jargao de lancamento em portugues, diz o idioma.
//
// TUDO SEM ACENTO de proposito: o texto da fonte passa por normalizar() antes,
// que rebaixa "Portugues"/"Português"/"PORTUGUÊS" a uma forma so. Sem isso a
// mesma fonte anunciada com e sem acento em dois episodios geraria duas
// assinaturas diferentes e o casamento falharia sem ninguem entender por que.
static const struct { const char *termo; const char *cod; } TERMOS[] = {
  { "dual audio",  "DUAL" }, { "dual-audio", "DUAL" }, { "dualaudio", "DUAL" },
  { "multi audio", "MULTI" }, { "multi-audio", "MULTI" }, { "multiaudio", "MULTI" },
  { "multi-subs",  "MULTI" },
  { "dublado",     "DUB"  }, { "dublagem",  "DUB" }, { "dubbed", "DUB" },
  { "nacional",    "POR"  },
  { "portuguese",  "POR"  }, { "portugues", "POR" },
  { "brazilian",   "POR"  }, { "brasileiro", "POR" },
  { "pt-br",       "POR"  }, { "ptbr", "POR" }, { "pt_br", "POR" },
  { "english",     "ENG"  }, { "ingles", "ENG" },
  { "spanish",     "SPA"  }, { "espanol", "SPA" },
  { "latino",      "SPA"  }, { "castellano", "SPA" },
  { "french",      "FRA"  }, { "frances", "FRA" }, { "francais", "FRA" },
  { "german",      "GER"  }, { "deutsch", "GER" }, { "alemao", "GER" },
  { "italian",     "ITA"  }, { "italiano", "ITA" },
  { "japanese",    "JPN"  }, { "japones", "JPN" },
  { "korean",      "KOR"  }, { "coreano", "KOR" },
  { "hindi",       "HIN"  }, { "tamil", "TAM" }, { "telugu", "TEL" },
  { "russian",     "RUS"  }, { "russo", "RUS" },
  { "legendado",   "LEG"  }, { "subbed", "LEG" }
};
#define N_TERMOS ((int)(sizeof TERMOS / sizeof *TERMOS))

// MINUSCULA E SEM ACENTO. Os addons escrevem a mesma coisa de tres jeitos
// ("Dual Audio", "DUAL AUDIO", "Dual Áudio") e o que a assinatura precisa e que
// os tres virem o mesmo texto. So a faixa latina do UTF-8 (0xC3 0x80..0xBF) e
// rebaixada: e onde moram os acentos do portugues, do espanhol e do frances. O
// resto dos bytes passa intacto — inclusive as bandeiras, que sao 0xF0 e nao
// entram nesta faixa.
static void normalizar(const char *src, char *dst, size_t tam) {
  static const char *BASE = "aaaaaaaceeeeiiiidnooooo*ouuuuy";  /* C0..DE */
  size_t k = 0;
  if (!tam) return;
  for (; src && *src && k + 1 < tam; src++) {
    unsigned char c = (unsigned char)*src;
    if (c == 0xC3 && (unsigned char)src[1] >= 0x80 && (unsigned char)src[1] <= 0xBE) {
      unsigned char d = (unsigned char)src[1];
      // 0x80..0x9E e a metade maiuscula, 0xA0..0xBE a minuscula: o mesmo
      // indice serve para as duas.
      unsigned idx = (d >= 0xA0 ? d - 0xA0 : d - 0x80);
      if (idx < 30) { dst[k++] = BASE[idx]; src++; continue; }
    }
    dst[k++] = (char)tolower(c);
  }
  dst[k] = 0;
}

// Token isolado, a mesma disciplina de stream_parse.c: "dub" nao pode casar
// dentro de "Dubai", e "leg" nao pode casar dentro de "legacy". A fronteira e
// alfanumerica ASCII.
// Jumps between occurrences of the first letter (strchr) instead of comparing
// at every byte: the same answer, many times fewer calls — this runs for ~45
// terms over each source's text whenever a remembered source is matched.
static int tokenEm(const char *s, const char *t) {
  size_t n = strlen(t);
  const char *p = s;
  while ((p = strchr(p, t[0])) != NULL) {
    if ((p == s || !isalnum((unsigned char)p[-1])) &&
        !strncmp(p, t, n) && !isalnum((unsigned char)p[n])) return 1;
    p++;
  }
  return 0;
}

#define TRILHA_MARCAS 10

static int jaTem(char marcas[][6], int n, const char *cod) {
  int i;
  for (i = 0; i < n; i++) if (!strcmp(marcas[i], cod)) return 1;
  return 0;
}

void fontepref_trilha(const Stream *s, char *dst, unsigned tam) {
  char texto[6000];
  char marcas[TRILHA_MARCAS][6];
  int n = 0, i, j, len;
  unsigned k = 0;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!s) return;
  // ROTULO + DESCRICAO + ARQUIVO, nesta ordem, porque cada addon escreve a
  // lingua num lugar: o Torrentio manda as bandeiras no `title` (que vira
  // descricao), o AIOStreams escreve "Dual Audio" no `name`, e alguns grupos so
  // deixam a marca no nome do arquivo.
  { char bruto[6000];
    snprintf(bruto, sizeof bruto, "%s %s %s", s->rotulo, s->descricao, s->arquivo);
    normalizar(bruto, texto, sizeof texto); }
  len = (int)strlen(texto);

  for (i = 0; i < len && n < TRILHA_MARCAS; i++) {
    char cod[4];
    // 8 bytes a frente: bandeira() le o par inteiro. Sem esta guarda ela leria
    // depois do NUL na ultima bandeira da string. O comprimento e medido UMA
    // vez: strlen dentro do laco num texto de 6 KB e trabalho quadratico numa
    // funcao chamada por fonte da lista.
    if ((unsigned char)texto[i] == 0xF0 && i + 8 <= len &&
        bandeira((const unsigned char *)texto + i, cod)) {
      if (!jaTem(marcas, n, cod)) snprintf(marcas[n++], 6, "%s", cod);
      i += 7;
    }
  }
  for (j = 0; j < N_TERMOS && n < TRILHA_MARCAS; j++)
    if (tokenEm(texto, TERMOS[j].termo) && !jaTem(marcas, n, TERMOS[j].cod))
      snprintf(marcas[n++], 6, "%s", TERMOS[j].cod);

  // ORDENADA: a assinatura tem de ser a mesma quando as marcas chegam em ordem
  // diferente, e chegam — o mesmo grupo escreve "🇧🇷 / 🇺🇸" num episodio e
  // "🇺🇸 / 🇧🇷" no seguinte. Insercao, porque sao no maximo dez.
  for (i = 1; i < n; i++) {
    char tmp[6];
    snprintf(tmp, sizeof tmp, "%s", marcas[i]);
    for (j = i; j > 0 && strcmp(marcas[j - 1], tmp) > 0; j--)
      snprintf(marcas[j], 6, "%s", marcas[j - 1]);
    snprintf(marcas[j], 6, "%s", tmp);
  }
  for (i = 0; i < n && k + 1 < tam; i++)
    k += (unsigned)snprintf(dst + k, tam - k, "%s%s", i ? "+" : "", marcas[i]);
}

// --- tabela -----------------------------------------------------------------

// A BYTE THAT IS PART OF A WORD: ASCII alphanumeric, or any byte of a
// multi-byte character. UTF-8 bytes are all >= 0x80 and isalnum() is false for
// them in the C locale, so without this a Korean-only name ("멋진 신세계")
// collapses to dots: the key comes out empty, two different series agree on it,
// and a name with no episode marker reduces to its spec ("1080p.mkv") — a
// collision on a wrong row, which is the bug this key exists to prevent.
static int wordByte(unsigned char c) {
  return isalnum(c) || c >= 0x80;
}

// THE KEY OF A RELEASE NAME: lowercased, non-alphanumerics collapsed to one
// dot, and CUT AT THE EPISODE MARKER ("s01e06", "1x06"). What stays is the
// name of the work itself, which is the only thing that separates two works
// whose sources share provider, bingeGroup, label and height (the Scandal bug:
// "The.Scandal.2026.S01E06..." and "Scandal.2026.S01E06..." both become keys
// that name their own work, and the same work matches across episodes).
//
// NO MARKER, OR MORE THAN ONE: keep the whole name. It then matches at most
// the same episode, and the caller falls back to the old provider+track rule,
// which is where it was before this key existed.
static void fileKey(const char *src, char *dst, size_t tam) {
  char low[600];
  size_t i, k, cut = 0, markers = 0;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!src) return;
  // A PATH IS NOT A NAME. Some addons send "Show.Name.S01E02/Show.Name.S01E02.mkv"
  // in `filename`; the directory repeats the name, which turns one marker into
  // two and declines the cut (measured on a 1656-name corpus: 90 such rows).
  // Only the basename is the name of the release.
  { const char *slash = strrchr(src, '/');
    if (slash) src = slash + 1; }
  for (i = 0; src[i] && i + 1 < sizeof low; i++)
    low[i] = (char)tolower((unsigned char)src[i]);
  low[i] = 0;
  // "s01e06" / "1x06": 1-2 digits, 'e' or 'x', 1-2 digits, and the whole run
  // must not touch a word byte on either side. That boundary rule is what keeps
  // "1280x720" and "x264" out: they are digits touching digits or letters.
  for (i = 0; low[i]; i++) {
    size_t j = i, d1, d2;
    if (j > 0 && wordByte((unsigned char)low[j - 1])) continue;
    if (low[j] == 's') j++;
    for (d1 = 0; d1 < 2 && isdigit((unsigned char)low[j]); d1++) j++;
    if (!d1 || (low[j] != 'e' && low[j] != 'x')) continue;
    j++;
    for (d2 = 0; d2 < 2 && isdigit((unsigned char)low[j]); d2++) j++;
    if (!d2 || wordByte((unsigned char)low[j])) continue;
    markers++;
    if (!cut) cut = i;
  }
  if (markers != 1) cut = i;   // ambiguous: keep the whole name
  // A NAME TOO LONG IS DROPPED, NOT CUT. A movie carries its whole spec string
  // and no marker ("...truehd.7.1.atmos.x264.mgb.mkv", 97 chars measured), so a
  // truncated key would let two different releases of two different works agree
  // on the first 255 bytes and score the wrong row. Empty here means no key, and
  // the caller falls back to the rule it used before this key existed.
  // Built into a local first, so the fit is decided on the finished key.
  { char out[sizeof low];
    for (k = 0, i = 0; i < cut && k + 1 < sizeof out; i++) {
      unsigned char c = (unsigned char)low[i];
      if (wordByte(c)) out[k++] = (char)c;
      else if (k && out[k - 1] != '.') out[k++] = '.';
    }
    while (k && out[k - 1] == '.') k--;
    out[k] = 0;
    if (k + 1 > tam) return;   // would truncate: no key at all
    memcpy(dst, out, k + 1);
  }
}

// --- the release fingerprint (fontepref.h) ----------------------------------
static void limpar(char *s);

// Position of the ONE episode marker ("s01e06", "1x06") in a lowercased name,
// the same rule as fileKey; -1 with none or more than one.
static long marcadorEp(const char *low) {
  size_t i, markers = 0;
  long cut = -1;
  for (i = 0; low[i]; i++) {
    size_t j = i, d1, d2;
    if (j > 0 && wordByte((unsigned char)low[j - 1])) continue;
    if (low[j] == 's') j++;
    for (d1 = 0; d1 < 2 && isdigit((unsigned char)low[j]); d1++) j++;
    if (!d1 || (low[j] != 'e' && low[j] != 'x')) continue;
    j++;
    for (d2 = 0; d2 < 2 && isdigit((unsigned char)low[j]); d2++) j++;
    if (!d2 || wordByte((unsigned char)low[j])) continue;
    markers++;
    if (cut < 0) cut = (long)i;
  }
  return markers == 1 ? cut : -1;
}

static int soAlnum(const char *t, size_t n) {
  size_t i;
  if (!n) return 0;
  for (i = 0; i < n; i++) if (!isalnum((unsigned char)t[i])) return 0;
  return 1;
}
static int soDigitos(const char *t, size_t n) {
  size_t i;
  for (i = 0; i < n; i++) if (!isdigit((unsigned char)t[i])) return 0;
  return n > 0;
}

// The release group of a file name (lowercased, extension already cut):
// "[SubsPlease] Show - 05 (1080p)" -> "subsplease" (anime puts it in front);
// "show.s01e05.1080p.web-dl.h264-ntb" -> "ntb" (after the LAST '-'). Words that
// only look like a group ("dl" of web-dl, "ray" of blu-ray, an episode number)
// do not count.
static void grupoDoNome(const char *low, char *dst, size_t tam) {
  const char *a, *b;
  size_t n;
  dst[0] = 0;
  if (low[0] == '[' && (b = strchr(low, ']')) != NULL) {
    a = low + 1; n = (size_t)(b - a);
    if (n >= 2 && n < tam && n <= 30) { memcpy(dst, a, n); dst[n] = 0; }
    return;
  }
  a = strrchr(low, '-');
  if (!a) return;
  a++;
  while (*a == ' ') a++;
  n = strlen(a);
  while (n && (a[n - 1] == ' ' || a[n - 1] == ']' || a[n - 1] == ')')) n--;
  // "group[rarbg]" / "group (1)": keep the group itself.
  { const char *q = memchr(a, '[', n); if (q) n = (size_t)(q - a); }
  { const char *q = memchr(a, '(', n); if (q) n = (size_t)(q - a); }
  while (n && a[n - 1] == ' ') n--;
  if (n < 2 || n > 24 || n >= tam || !soAlnum(a, n) || soDigitos(a, n)) return;
  if ((n == 2 && !strncmp(a, "dl", 2)) || (n == 3 && !strncmp(a, "ray", 3)) ||
      (n == 3 && !strncmp(a, "rip", 3)) || (n == 3 && !strncmp(a, "web", 3))) return;
  memcpy(dst, a, n); dst[n] = 0;
}

// THE RELEASE GROUP FROM THE DESCRIPTION: AIOStreams formatters write it
// after a 🏷️ (U+1F3F7) — "🏷️ FraMeSToR". Used only when the file name gave
// no group.
static void grupoEtiqueta(const char *d, char *dst, size_t tam) {
  const char *p = d ? strstr(d, "\xF0\x9F\x8F\xB7") : NULL;
  size_t n = 0;
  dst[0] = 0;
  if (!p) return;
  p += 4;
  if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xB8 && (unsigned char)p[2] == 0x8F) p += 3;
  while (*p == ' ') p++;
  while (p[n] && (isalnum((unsigned char)p[n]) || p[n] == '-' || p[n] == '_' || p[n] == '.') && n + 1 < tam && n < 30) n++;
  if (n < 2 || (p[n] && !isspace((unsigned char)p[n]))) return;
  for (size_t i = 0; i < n; i++) dst[i] = (char)tolower((unsigned char)p[i]);
  dst[n] = 0;
}

// --- styled Unicode back to ASCII (fontepref_nivel) -------------------------
// One pass: lowercase ASCII letters/digits and '+' stay; math alphanumerics,
// small caps, sub/superscript digits, fullwidth and enclosed letters become
// their ASCII letter/digit; label markers (🎚 🏆 » ▸ ( [) become '#'; anything
// else becomes a space. Runs of spaces collapse.
static int capsPequena(unsigned cp) {
  switch (cp) {
    case 0x1D00: return 'a'; case 0x0299: return 'b'; case 0x1D04: return 'c'; case 0x1D05: return 'd';
    case 0x1D07: return 'e'; case 0xA730: return 'f'; case 0x0262: return 'g'; case 0x029C: return 'h';
    case 0x026A: return 'i'; case 0x1D0A: return 'j'; case 0x1D0B: return 'k'; case 0x029F: return 'l';
    case 0x1D0D: return 'm'; case 0x0274: return 'n'; case 0x1D0F: return 'o'; case 0x1D18: return 'p';
    case 0xA7AF: return 'q'; case 0x0280: return 'r'; case 0xA731: return 's'; case 0x1D1B: return 't';
    case 0x1D1C: return 'u'; case 0x1D20: return 'v'; case 0x1D21: return 'w'; case 0x028F: return 'y';
    case 0x1D22: return 'z';
    case 0x0493: return 'f'; case 0x0445: return 'x'; case 0x03D9: return 'q';   // Tamtaro lookalikes
    case 0x00B9: return '1'; case 0x00B2: return '2'; case 0x00B3: return '3'; case 0x2070: return '0';
    default: return 0;
  }
}
static int dobrarCp(unsigned cp) {
  int c;
  if (cp < 0x80) {
    if (isalnum((int)cp)) return tolower((int)cp);
    if (cp == '+') return '+';
    if (cp == '(' || cp == '[') return '#';
    return ' ';
  }
  if ((c = capsPequena(cp)) != 0) return c;
  if (cp >= 0x1D400 && cp <= 0x1D6A3) { unsigned k = (cp - 0x1D400) % 52; return k < 26 ? 'a' + (int)k : 'a' + (int)k - 26; }
  if (cp >= 0x1D7CE && cp <= 0x1D7FF) return '0' + (int)((cp - 0x1D7CE) % 10);
  if (cp >= 0x2080 && cp <= 0x2089) return '0' + (int)(cp - 0x2080);
  if (cp >= 0x2074 && cp <= 0x2079) return '0' + (int)(cp - 0x2070);
  if (cp >= 0xFF01 && cp <= 0xFF5E) return dobrarCp(cp - 0xFEE0);
  if (cp >= 0x2460 && cp <= 0x2468) return '1' + (int)(cp - 0x2460);
  if (cp >= 0x24B6 && cp <= 0x24CF) return 'a' + (int)(cp - 0x24B6);
  if (cp >= 0x24D0 && cp <= 0x24E9) return 'a' + (int)(cp - 0x24D0);
  if (cp >= 0x1F130 && cp <= 0x1F149) return 'a' + (int)(cp - 0x1F130);
  if (cp >= 0x1F150 && cp <= 0x1F169) return 'a' + (int)(cp - 0x1F150);
  if (cp >= 0x1F170 && cp <= 0x1F189) return 'a' + (int)(cp - 0x1F170);
  if (cp == 0x1F39A || cp == 0x1F3C6 || cp == 0x00BB || cp == 0x25B8) return '#';
  return ' ';
}
static size_t dobrar(const char *in, char *out, size_t cap, size_t k) {
  const unsigned char *p = (const unsigned char *)(in ? in : "");
  while (*p && k + 2 < cap) {
    unsigned cp = *p; int n = 1, c;
    if (cp >= 0xF0 && p[1] && p[2] && p[3]) { cp = ((cp & 7u) << 18) | ((p[1] & 63u) << 12) | ((p[2] & 63u) << 6) | (p[3] & 63u); n = 4; }
    else if (cp >= 0xE0 && p[1] && p[2]) { cp = ((cp & 15u) << 12) | ((p[1] & 63u) << 6) | (p[2] & 63u); n = 3; }
    else if (cp >= 0xC0 && p[1]) { cp = ((cp & 31u) << 6) | (p[1] & 63u); n = 2; }
    p += n;
    if (cp == 0xFE0F || cp == 0x200B || cp == 0x200C || cp == 0x200D || cp == 0x2060) continue;
    c = dobrarCp(cp);
    if (c == ' ' && (k == 0 || out[k - 1] == ' ')) continue;
    out[k++] = (char)c;
  }
  out[k] = 0;
  return k;
}

void fontepref_nivel(const Stream *s, char *dst, unsigned tam) {
  char txt[3200], *tok[400];
  int nt = 0, i, best = 0, alt = 0;
  size_t k;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!s) return;
  k = dobrar(s->rotulo, txt, sizeof txt, 0);
  if (k + 2 < sizeof txt) { txt[k++] = ' '; txt[k] = 0; }
  dobrar(s->descricao, txt, sizeof txt, k);
  // Tokens: words, and '#' (a label marker) as a token of its own.
  for (char *p = txt; *p && nt < (int)(sizeof tok / sizeof *tok);) {
    while (*p == ' ') p++;
    if (!*p) break;
    if (*p == '#') { *p++ = 0; tok[nt++] = (char *)"#"; continue; }
    tok[nt++] = p;
    while (*p && *p != ' ' && *p != '#') p++;
    if (*p == ' ') *p++ = 0;
    else if (*p == '#' && nt < (int)(sizeof tok / sizeof *tok)) { *p++ = 0; tok[nt++] = (char *)"#"; }   // word glued to a marker
  }
  // SEADEX, alt before best. "best release" / "seadex alt" / "alternative" /
  // "alt best release" anywhere; a bare "best" or "alt" only within 3 tokens
  // after a label marker. A bare "seadex" never.
  for (i = 0; i < nt; i++) {
    const char *t = tok[i], *n1 = i + 1 < nt ? tok[i + 1] : "", *n2 = i + 2 < nt ? tok[i + 2] : "";
    int marca = 0;
    for (int j = i - 1; j >= 0 && j >= i - 3; j--) if (!strcmp(tok[j], "#")) { marca = 1; break; }
    if ((!strcmp(t, "seadex") && !strcmp(n1, "alt")) || !strcmp(t, "alternative") ||
        (!strcmp(t, "alt") && !strcmp(n1, "best") && !strcmp(n2, "release")) || (!strcmp(t, "alt") && marca)) alt = 1;
    else if ((!strcmp(t, "best") && !strcmp(n1, "release")) || (!strcmp(t, "best") && marca)) best = 1;
  }
  if (alt || best) { snprintf(dst, tam, "%s", alt ? "seadex-alt" : "seadex-best"); return; }
  // TIER: a source word, then t1..t8 / "tier" + 1..8 / "01".."08". The first
  // one wins (the formatters' own ::first).
  for (i = 0; i < nt; i++) {
    const char *t = tok[i], *src = NULL;
    int adv = 1, d = 0;
    char pre[4] = "";
    if ((!strcmp(t, "de") || !strcmp(t, "fr")) && i + 1 < nt) { snprintf(pre, sizeof pre, "%s", t); i++; t = tok[i]; }
    if (!strcmp(t, "anime") && i + 1 < nt) { i++; t = tok[i]; }
    if (!strcmp(t, "remux")) src = "remux";
    else if ((!strcmp(t, "uhd") || !strcmp(t, "hd")) && i + 1 < nt && !strcmp(tok[i + 1], "bluray")) { src = t[0] == 'u' ? "uhd-bluray" : "hd-bluray"; adv = 2; }
    else if ((!strcmp(t, "uhd") || !strcmp(t, "hd")) && i + 2 < nt && !strcmp(tok[i + 1], "blu") && !strcmp(tok[i + 2], "ray")) { src = t[0] == 'u' ? "uhd-bluray" : "hd-bluray"; adv = 3; }
    else if (!strcmp(t, "bluray")) src = "bluray";
    else if (!strcmp(t, "blu") && i + 1 < nt && !strcmp(tok[i + 1], "ray")) { src = "bluray"; adv = 2; }
    else if (!strcmp(t, "bd")) src = "bd";
    else if (!strcmp(t, "web")) {
      src = "web";
      if (i + 1 < nt && !strcmp(tok[i + 1], "scene")) { snprintf(dst, tam, "%s%sweb-scene", pre, pre[0] ? "-" : ""); return; }
    }
    if (!src || i + adv >= nt) continue;
    t = tok[i + adv];
    if (t[0] == 't' && t[1] >= '1' && t[1] <= '8' && !t[2]) d = t[1] - '0';
    else if (!strcmp(t, "tier") && i + adv + 1 < nt) {
      const char *n = tok[i + adv + 1];
      if (n[0] == '0' && n[1] >= '1' && n[1] <= '8' && !n[2]) d = n[1] - '0';
      else if (n[0] >= '1' && n[0] <= '8' && !n[1]) d = n[0] - '0';
    }
    if (d) { snprintf(dst, tam, "%s%s%s-t%d", pre, pre[0] ? "-" : "", src, d); return; }
  }
}

void fontepref_lancamento(const Stream *s, char *dst, unsigned tam) {
  char low[1100], grupo[32] = "", tok[32], ant[32] = "";
  const char *res = "", *fonte = "", *cod = "", *hdr = "", *src, *p;
  int temNome, remux = 0, dv = 0, hdrMais = 0;
  size_t i, n;
  long cut;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!s) return;
  temNome = s->arquivo[0] != 0;
  src = temNome ? s->arquivo : s->descricao;
  if (temNome) { const char *sl = strrchr(src, '/'); if (sl) src = sl + 1; }
  for (i = 0; src[i] && i + 1 < sizeof low; i++) low[i] = (char)tolower((unsigned char)src[i]);
  low[i] = 0;
  if (temNome) {
    static const char *EXT[] = { ".mkv", ".mp4", ".avi", ".ts", ".m2ts", ".webm", ".mov", ".m4v" };
    size_t e;
    n = strlen(low);
    for (e = 0; e < sizeof EXT / sizeof *EXT; e++) {
      size_t m = strlen(EXT[e]);
      if (n > m && !strcmp(low + n - m, EXT[e])) { low[n - m] = 0; break; }
    }
    grupoDoNome(low, grupo, sizeof grupo);
  }
  if (!grupo[0]) grupoEtiqueta(s->descricao, grupo, sizeof grupo);
  if (strstr(low, "hdr10+") || strstr(low, "hdr10plus")) hdrMais = 1;
  // Only what comes after the episode marker names the release; before it is
  // the show (which could hold "web" or "4k" in its own title).
  cut = marcadorEp(low);
  p = cut >= 0 ? low + cut : low;
  while (*p) {
    while (*p && !wordByte((unsigned char)*p)) p++;
    for (n = 0; *p && wordByte((unsigned char)*p); p++) if (n + 1 < sizeof tok) tok[n++] = *p;
    tok[n] = 0;
    if (!n) break;
    if (!strcmp(tok, "2160p") || !strcmp(tok, "4k") || !strcmp(tok, "uhd")) { if (!res[0]) res = "2160"; }
    else if (!strcmp(tok, "1080p") || !strcmp(tok, "1080i")) { if (!res[0]) res = "1080"; }
    else if (!strcmp(tok, "720p")) { if (!res[0]) res = "720"; }
    else if (!strcmp(tok, "576p")) { if (!res[0]) res = "576"; }
    else if (!strcmp(tok, "480p")) { if (!res[0]) res = "480"; }
    else if (!strcmp(tok, "remux") || !strcmp(tok, "bdremux")) remux = 1;
    else if (!strcmp(tok, "bluray") || !strcmp(tok, "bdrip") || !strcmp(tok, "brrip") ||
             (!strcmp(tok, "ray") && !strcmp(ant, "blu"))) { if (!fonte[0] || !strcmp(fonte, "web")) fonte = "bluray"; }
    else if (!strcmp(tok, "webdl") || (!strcmp(tok, "dl") && !strcmp(ant, "web"))) fonte = "webdl";
    else if (!strcmp(tok, "webrip") || (!strcmp(tok, "rip") && !strcmp(ant, "web"))) fonte = "webrip";
    else if (!strcmp(tok, "web")) { if (!fonte[0]) fonte = "web"; }
    else if (!strcmp(tok, "hdtv")) { if (!fonte[0]) fonte = "hdtv"; }
    else if (!strcmp(tok, "dvdrip") || !strcmp(tok, "dvd")) { if (!fonte[0]) fonte = "dvd"; }
    else if (!strcmp(tok, "x265") || !strcmp(tok, "h265") || !strcmp(tok, "hevc") ||
             (!strcmp(tok, "265") && !strcmp(ant, "h"))) { if (!cod[0]) cod = "hevc"; }
    else if (!strcmp(tok, "x264") || !strcmp(tok, "h264") || !strcmp(tok, "avc") ||
             (!strcmp(tok, "264") && !strcmp(ant, "h"))) { if (!cod[0]) cod = "avc"; }
    else if (!strcmp(tok, "av1")) { if (!cod[0]) cod = "av1"; }
    else if (!strcmp(tok, "dv") || !strcmp(tok, "dovi") ||
             (!strcmp(tok, "vision") && !strcmp(ant, "dolby"))) dv = 1;
    else if (!strcmp(tok, "hdr") || !strcmp(tok, "hdr10") || !strcmp(tok, "hdr10plus")) { if (!hdr[0]) hdr = "hdr"; }
    snprintf(ant, sizeof ant, "%s", tok);
  }
  if (remux) fonte = "remux";
  if (dv) hdr = "dv"; else if (hdrMais) hdr = "hdr10p";
  if (!grupo[0] && !res[0] && !fonte[0] && !cod[0]) return;   // nothing names the release
  snprintf(dst, tam, "%s|%s|%s|%s|%s", grupo, res, fonte, cod, hdr);
  limpar(dst);
}

void fontepref_id_base(const char *id, char *dst, unsigned tam) {
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!id) return;
  snprintf(dst, tam, "%.*s", (int)idbase_len(id), id);   // "kitsu:41370" inteiro (idbase.h)
}

static int achar(const char *base) {
  int i;
  if (!base || !base[0]) return -1;
  for (i = 0; i < nTab; i++) if (!strcmp(tabela[i].id, base)) return i;
  return -1;
}

// Tira TAB e controle. O rotulo do Torrentio e "Torrentio\n1080p" — com a
// quebra de linha dentro, o arquivo de uma linha por titulo viraria duas e a
// leitura seguinte perderia metade da tabela em silencio.
static void limpar(char *s) {
  for (; *s; s++) if ((unsigned char)*s < 32) *s = ' ';
}

static char *campo(char **p) {
  char *ini = *p, *t;
  if (!ini) return (char *)"";
  t = strchr(ini, '\t');
  if (t) { *t = 0; *p = t + 1; } else { *p = NULL; }
  return ini;
}

// O bingeGroup ENTROU NO FIM DA LINHA, depois do rotulo, e nao no meio.
//
// O arquivo da 1.0.55 tem seis campos. Enfiar o setimo antes do rotulo faria a
// leitura do arquivo ja existente pousar o ROTULO dentro do bingeGroup —
// silenciosamente, porque campo que falta vira vazio e a linha continua
// valida. No fim, o arquivo velho le igualzinho e so nao tem bingeGroup, que e
// a verdade: a 1.0.55 nao sabia o que e isso.
//
// O rotulo deixa de ser "o campo que pode conter qualquer coisa" e passa a ser
// separado por TAB como os outros. Pode: limpar() ja troca TODO byte abaixo de
// 32 por espaco antes de gravar, e o TAB e um deles. O ultimo campo agora e o
// bingeGroup, que passa pelo mesmo limpar().
static unsigned revisao;   // fontepref_revisao: bumps on every change of the tables
unsigned fontepref_revisao(void) { return revisao; }

static void gravar(void) {
  revisao++;
  size_t cap = (size_t)FONTEPREF_MAX * 1000u + 64u;
  char *buf = (char *)malloc(cap);
  size_t k = 0;
  int i;
  if (!buf) return;
  k += (size_t)snprintf(buf + k, cap - k, "# nuvio fontes v1\n");
  for (i = 0; i < nTab && k + 1 < cap; i++)
    k += (size_t)snprintf(buf + k, cap - k, "%s\t%lld\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
                          tabela[i].id, tabela[i].quandoS, tabela[i].altura,
                          tabela[i].trilha, tabela[i].provedor, tabela[i].rotulo,
                          tabela[i].bingeGroup, tabela[i].arquivo, tabela[i].lancamento,
                          tabela[i].nivel);
  dados_gravar(arquivoDoPerfil(), buf);
  free(buf);
}

// O registro de uma fonte, igual para a escolha a mao e para a que o
// automatico tocou.
static void montar(const char *base, const Stream *s, FontePref *novo) {
  memset(novo, 0, sizeof *novo);
  snprintf(novo->id, sizeof novo->id, "%s", base);
  snprintf(novo->provedor, sizeof novo->provedor, "%s", s->provedor);
  snprintf(novo->rotulo, sizeof novo->rotulo, "%s", s->rotulo);
  snprintf(novo->bingeGroup, sizeof novo->bingeGroup, "%s", s->bingeGroup);
  fileKey(s->arquivo, novo->arquivo, sizeof novo->arquivo);
  fontepref_lancamento(s, novo->lancamento, sizeof novo->lancamento);
  fontepref_nivel(s, novo->nivel, sizeof novo->nivel);
  fontepref_trilha(s, novo->trilha, sizeof novo->trilha);
  novo->altura = s->altura;
  novo->quandoS = (long long)time(NULL);
  limpar(novo->provedor);
  limpar(novo->rotulo);
  limpar(novo->trilha);
  limpar(novo->bingeGroup);
  limpar(novo->arquivo);
}
static int mesmoRegistro(const FontePref *a, const FontePref *b) {
  return !strcmp(a->provedor, b->provedor) && !strcmp(a->trilha, b->trilha) &&
         !strcmp(a->bingeGroup, b->bingeGroup) && !strcmp(a->rotulo, b->rotulo) &&
         !strcmp(a->arquivo, b->arquivo) && !strcmp(a->lancamento, b->lancamento) &&
         !strcmp(a->nivel, b->nivel) &&
         a->altura == b->altura;
}
static size_t linhaGravar(char *buf, size_t cap, const FontePref *f) {
  return (size_t)snprintf(buf, cap, "%s\t%lld\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
                          f->id, f->quandoS, f->altura, f->trilha, f->provedor, f->rotulo,
                          f->bingeGroup, f->arquivo, f->lancamento, f->nivel);
}

static void autoCarregar(void) {
  char *b, *linha, *prox;
  int v2 = 0;
  nAuto = 0;
  b = dados_ler(arquivoAuto());
  if (!b) return;
  for (linha = b; linha && *linha && nAuto < FONTEPREF_MAX; linha = prox) {
    char *p, *id, *quando;
    char *fim = strchr(linha, '\n');
    AutoPref *a = &tabAuto[nAuto];
    prox = fim ? fim + 1 : NULL;
    if (fim) *fim = 0;
    if (!strcmp(linha, "# nuvio fontes auto v2")) { v2 = 1; continue; }
    if (linha[0] == '#' || !linha[0]) continue;
    p = linha;
    memset(a, 0, sizeof *a);
    id = campo(&p); quando = campo(&p);
    if (!id[0]) continue;
    snprintf(a->p.id, sizeof a->p.id, "%s", id);
    a->p.quandoS = atoll(quando);
    if (v2) {
      // Os mesmos campos da escolha a mao, na mesma ordem (linhaGravar).
      a->p.altura = atoi(campo(&p));
      snprintf(a->p.trilha, sizeof a->p.trilha, "%s", campo(&p));
      snprintf(a->p.provedor, sizeof a->p.provedor, "%s", campo(&p));
      snprintf(a->p.rotulo, sizeof a->p.rotulo, "%s", campo(&p));
      snprintf(a->p.bingeGroup, sizeof a->p.bingeGroup, "%s", campo(&p));
      snprintf(a->p.arquivo, sizeof a->p.arquivo, "%s", campo(&p));
      snprintf(a->p.lancamento, sizeof a->p.lancamento, "%s", campo(&p));
      snprintf(a->p.nivel, sizeof a->p.nivel, "%s", campo(&p));   // 10th field, may be absent
      if (!a->p.provedor[0]) continue;
    } else {
      // v1: id, quando, bingeGroup (e, por algumas horas de 15/10, provedor e
      // lancamento). So o grupo: casa pela camada do bingeGroup, como antes.
      snprintf(a->p.bingeGroup, sizeof a->p.bingeGroup, "%s", campo(&p));
      snprintf(a->p.provedor, sizeof a->p.provedor, "%s", campo(&p));
      snprintf(a->p.lancamento, sizeof a->p.lancamento, "%s", campo(&p));
      if (!a->p.bingeGroup[0] && !a->p.lancamento[0]) continue;
    }
    a->sessao = sessaoTem(id);   // do disco: so e "desta sessao" se este processo gravou
    nAuto++;
  }
  free(b);
}

static void autoGravar(void) {
  revisao++;
  size_t cap = (size_t)FONTEPREF_MAX * 1000u + 64u, k = 0;
  char *buf = (char *)malloc(cap);
  int i;
  if (!buf) return;
  k += (size_t)snprintf(buf + k, cap - k, "# nuvio fontes auto v2\n");
  for (i = 0; i < nAuto && k + 1 < cap; i++) k += linhaGravar(buf + k, cap - k, &tabAuto[i].p);
  dados_gravar(arquivoAuto(), buf);
  free(buf);
}

static int autoAchar(const char *base) {
  int i;
  for (i = 0; i < nAuto; i++) if (!strcmp(tabAuto[i].p.id, base)) return i;
  return -1;
}

static void autoRemover(int k) {
  if (k < 0 || k >= nAuto) return;
  tabAuto[k] = tabAuto[--nAuto];
  autoGravar();
}

void fontepref_iniciar(void) {
  char *b, *linha, *prox;
  if (carregado) return;
  carregado = 1; revisao++;
  nTab = 0;
  autoCarregar();
  b = dados_ler(arquivoDoPerfil());
  if (!b) return;
  for (linha = b; linha && *linha && nTab < FONTEPREF_MAX; linha = prox) {
    char *p, *id, *quando, *altura, *trilha, *provedor, *rotulo, *binge, *file, *lanc, *nivel;
    char *fim = strchr(linha, '\n');
    prox = fim ? fim + 1 : NULL;
    if (fim) *fim = 0;
    if (linha[0] == '#' || !linha[0]) continue;
    p = linha;
    id       = campo(&p);
    quando   = campo(&p);
    altura   = campo(&p);
    trilha   = campo(&p);
    provedor = campo(&p);
    rotulo   = campo(&p);
    // O bingeGroup E O ULTIMO campo, como o titulo em salvos.txt: sendo o
    // ultimo nao precisa de separador depois dele. Campo que faltar vira vazio
    // e a linha continua valida — que e exatamente o que acontece ao ler o
    // arquivo de seis campos gravado pela 1.0.55, e o certo: aquela versao nao
    // lia bingeGroup nenhum.
    binge = campo(&p);
    // The file name is the EIGHTH field, appended after the bingeGroup for the
    // same reason the bingeGroup was appended: an old line does not have it,
    // and a missing field comes out empty. (The bingeGroup used to be the last
    // field, read as "the rest of the line"; it now goes through campo() and
    // the rest of the line is the name.)
    file  = campo(&p);
    // The release fingerprint is the NINTH field, appended for the same reason
    // as the two before it: an older line reads it as empty and matches as it
    // always did.
    lanc  = campo(&p);
    // The tier (SeaDex / Remux T1...) is the TENTH, appended the same way.
    nivel = campo(&p);
    // Any title id, not only IMDb: an anime opened by its kitsu/mal id used to
    // be remembered for the session and dropped here at the next start.
    if (!id[0]) continue;
    if (!provedor[0]) continue;      // sem provedor a linha nao casa com nada
    { FontePref *f = &tabela[nTab++];
      memset(f, 0, sizeof *f);
      snprintf(f->id, sizeof f->id, "%s", id);
      snprintf(f->trilha, sizeof f->trilha, "%s", trilha);
      snprintf(f->provedor, sizeof f->provedor, "%s", provedor);
      snprintf(f->rotulo, sizeof f->rotulo, "%s", rotulo);
      snprintf(f->bingeGroup, sizeof f->bingeGroup, "%s", binge);
      snprintf(f->arquivo, sizeof f->arquivo, "%s", file);
      snprintf(f->lancamento, sizeof f->lancamento, "%s", lanc);
      snprintf(f->nivel, sizeof f->nivel, "%s", nivel);
      f->quandoS = atoll(quando);
      f->altura = atoi(altura); }
  }
  free(b);
  printf("[fonte] %d preferencia(s) de fonte no perfil %d\n", nTab, perfil);
  fflush(stdout);
}

void fontepref_definir_perfil(int p) {
  if (p < 0) p = 0;
  if (p == perfil && carregado) return;
  if (p != perfil) esquecerTocando();
  perfil = p;
  nTab = 0;
  nAuto = 0;
  carregado = 0; revisao++;
}

void fontepref_esquecer(void) {
  int p;
  esquecerTocando();
  // APAGA O ARQUIVO DE TODOS OS PERFIS, e nao so o do perfil corrente. Ao
  // sair da conta os perfis somem junto (perfis_esquecer), entao um arquivo
  // de perfil 3 que ficasse para tras seria lido pela proxima pessoa que
  // acabasse no perfil 3 da conta dela.
  for (p = 0; p <= FONTEPREF_PERFIS; p++) {
    char nome[48];
    snprintf(nome, sizeof nome, "fontepref-p%d.txt", p);
    dados_apagar(nome);
    snprintf(nome, sizeof nome, "fontepref-auto-p%d.txt", p);
    dados_apagar(nome);
  }
  nTab = 0;
  nAuto = 0;
  carregado = 1; revisao++;   // ja sabemos o que ha no disco: nada
}

// VENCEU? Ver FONTEPREF_VALIDADE_S em fontepref.h para o prazo e o porque.
// Aqui ficam as duas bordas, que sao as que mordem numa TV:
//
//   quandoS <= 0  vale para sempre. Linha sem carimbo e linha que veio de um
//                 arquivo que nao tinha a coluna, ou de um atoll que falhou.
//                 Tratar ausencia de data como "velhissima" apagaria a escolha
//                 de todo mundo no primeiro arranque da versao seguinte — o
//                 lado errado para errar.
//   agora < quandoS  (preferencia "do futuro") TAMBEM vale. O app web recusa
//                 esse caso; aqui nao, de proposito: esta TV arranca com o
//                 relogio errado e so acerta quando a rede sobe, entao "do
//                 futuro" e o estado NORMAL dos primeiros segundos de uso. Uma
//                 preferencia honrada a mais custa uma fonte; expirar a tabela
//                 inteira a cada tomada tirada da parede custa o recurso.
static int venceu(const FontePref *f) {
  long long agora = (long long)time(NULL);
  if (!f || f->quandoS <= 0) return 0;
  if (agora <= f->quandoS) return 0;
  return agora - f->quandoS > cfgValidadeS;
}

const FontePref *fontepref_do_titulo(const char *id) {
  char base[24];
  int k;
  // "Reusar a ultima fonte" desligado: a escolha continua gravada (ligar de
  // novo a traz de volta), mas nenhuma leitura a ve.
  if (!cfgReusar) return NULL;
  if (!carregado) fontepref_iniciar();
  fontepref_id_base(id, base, sizeof base);
  k = achar(base);
  if (k < 0) return NULL;
  // "Reusar o grupo entre sessoes" desligado vale tambem para a escolha a
  // mao: so a feita (ou confirmada) por este processo conta.
  if (!cfgBingeSessoes && !sessaoTem(base)) return NULL;
  if (venceu(&tabela[k])) {
    // Nao apaga e nao grava: ver a nota em fontepref.h. A linha morre sozinha
    // na proxima escolha (reescrita) ou quando a tabela encher (a mais antiga
    // sai primeiro, e vencida e velha por definicao).
    printf("[fonte] preferencia de %s venceu (%lld dias); perguntando de novo\n",
           tabela[k].id,
           ((long long)time(NULL) - tabela[k].quandoS) / (24LL * 3600LL));
    fflush(stdout);
    return NULL;
  }
  return &tabela[k];
}

int fontepref_tem(const char *id) { return fontepref_do_titulo(id) != NULL; }

int fontepref_guardar(const char *id, const Stream *s) {
  char base[24];
  int k;
  FontePref novo;
  if (!s || !s->provedor[0]) return 0;
  // Carregar sob demanda, pela mesma razao de salvos_definir: gravar por cima
  // de uma tabela nao lida APAGARIA o arquivo inteiro.
  if (!carregado) fontepref_iniciar();
  fontepref_id_base(id, base, sizeof base);
  if (!base[0]) return 0;

  montar(base, s, &novo);

  // A manual SUBSTITUI a automatica do titulo: com a escolha da pessoa gravada
  // a automatica nao tem mais papel (e ressurgiria se a manual sumisse).
  autoRemover(autoAchar(base));

  k = achar(base);
  if (k < 0) {
    if (nTab >= FONTEPREF_MAX) {
      // Cheia: sai a mais antiga. Nunca falhar em guardar — a alternativa e
      // uma tabela que para de aprender no titulo 200 e ninguem descobre.
      int i, velho = 0;
      for (i = 1; i < nTab; i++)
        if (tabela[i].quandoS < tabela[velho].quandoS) velho = i;
      k = velho;
    } else {
      k = nTab++;
    }
  } else if (mesmoRegistro(&tabela[k], &novo) && !venceu(&tabela[k])) {
    // Mesma escolha de novo: nao reescrever o arquivo por um carimbo de tempo.
    // MENOS quando ela venceu — ai o carimbo E a novidade, e sem reescrever a
    // pessoa reescolheria a mesma fonte a cada abertura ate o fim dos tempos.
    // The file name enters the comparison because it was the missing field:
    // two sources equal in everything but that were "the same choice", and the
    // person's re-pick was dropped in silence (the Scandal bug).
    sessaoMarcar(base);
    return 0;
  }
  tabela[k] = novo;
  sessaoMarcar(base);
  gravar();
  printf("[fonte] preferencia de %s: %s / %s / binge=%s / lancamento=%s / nivel=%s\n", base, novo.provedor,
         novo.trilha[0] ? novo.trilha : "(sem marca de idioma)",
         novo.bingeGroup[0] ? novo.bingeGroup : "(o addon nao declarou)",
         novo.lancamento[0] ? novo.lancamento : "(sem nome de lancamento)",
         novo.nivel[0] ? novo.nivel : "-");
  fflush(stdout);
  return 1;
}

// DESEMPATE, o mesmo das duas camadas. Varias fontes podem dividir o mesmo
// bingeGroup (o addon agrupa por servico/qualidade, nao por arquivo), e varias
// podem dividir provedor + trilha. Resolucao igual vale 2, rotulo igual vale 4
// — o rotulo e o sinal mais forte porque e o texto que a pessoa leu na folha
// quando escolheu.
static int nota(const Stream *s, const FontePref *p) {
  int n = 0;
  if (s->altura == p->altura) n += 2;
  if (!strcmp(s->rotulo, p->rotulo)) n += 4;
  // THE FILE NAME, the strongest of the three: it is the only thing that
  // separates two works whose sources share provider, bingeGroup, label and
  // height (the Scandal bug). +8 beats rotulo+altura together, so when the
  // name matches it decides the row; when it matches nothing (no name, or a
  // name with no episode marker) the score is exactly what it was before.
  if (p->arquivo[0] && s->arquivo[0]) {
    char key[FONTEPREF_ARQUIVO];
    fileKey(s->arquivo, key, sizeof key);
    if (key[0] && !strcmp(key, p->arquivo)) n += 8;
  }
  return n;
}

// CAMADA 1: o bingeGroup que o addon declarou. Sem provedor na condicao, como
// no app web (um `find` por bingeGroup e mais nada): o campo JA E a declaracao
// de identidade de fonte, e exigir provedor por cima so criaria um jeito novo
// de nao casar.
static int porBinge(const FontePref *p) {
  int i, melhor = -1, melhorNota = -1, total = stream_n();
  if (!cfgBinge || !p->bingeGroup[0]) return -1;
  for (i = 0; i < total; i++) {
    const Stream *s = stream_item(i);
    int n;
    if (!s || !s->bingeGroup[0]) continue;
    if (strcmp(s->bingeGroup, p->bingeGroup)) continue;
    n = nota(s, p);
    // `>` e nao `>=`, a mesma disciplina de stream_automatico: em empate fica
    // o PRIMEIRO da lista, que e a ordem em que o addon devolveu.
    if (n > melhorNota) { melhorNota = n; melhor = i; }
  }
  return melhor;
}

// CAMADA 2: a heuristica da 1.0.55, intacta. Ela atende os addons que nao
// mandam bingeGroup — que sao muitos — e o episodio em que o addon MUDOU o
// rotulo de agrupamento.
// A trilha `t` de hoje CONTEM todas as marcas da lembrada? "BR+DUB" contem
// "DUB"; "" nao contem nada e e contida por tudo.
static int contemMarcas(const char *t, const char *lembrada) {
  const char *p = lembrada;
  while (*p) {
    const char *f = strchr(p, '+');
    size_t n = f ? (size_t)(f - p) : strlen(p);
    const char *q = t;
    int achou = 0;
    while (*q) {
      const char *g = strchr(q, '+');
      size_t m = g ? (size_t)(g - q) : strlen(q);
      if (m == n && !strncmp(q, p, n)) { achou = 1; break; }
      if (!g) break;
      q = g + 1;
    }
    if (!achou) return 0;
    if (!f) break;
    p = f + 1;
  }
  return 1;
}

static int porTrilha(const FontePref *p) {
  int i, melhor = -1, melhorNota = -1, total = stream_n(), passo;
  // DOIS PASSOS. 1: provedor E trilha iguais (o casamento estrito de
  // sempre). 2: mesmo provedor e a trilha de hoje CONTEM as marcas da
  // lembrada — o episodio seguinte do mesmo addon costuma vir com uma marca
  // a mais no rotulo ("Dual Audio" hoje, "BR DUB" amanha), e o estrito
  // falhava; com "escolher a fonte ao reproduzir" ligado isso reabria a folha
  // em Retomar (rawldon, #72 na rc1: "sometimes pressing Resume opens the
  // source list"). A regra de fontepref.h continua: nenhuma marca lembrada
  // some — quem escolheu DUB nao cai num ORIGINAL.
  for (passo = 0; passo < 2 && melhor < 0; passo++) {
    for (i = 0; i < total; i++) {
      const Stream *s = stream_item(i);
      char t[FONTEPREF_TRILHA];
      int n;
      if (!s || !s->provedor[0]) continue;
      if (strcasecmp(s->provedor, p->provedor)) continue;
      fontepref_trilha(s, t, sizeof t);
      if (passo == 0 ? strcmp(t, p->trilha) != 0 : !contemMarcas(t, p->trilha)) continue;
      n = nota(s, p);
      if (n > melhorNota) { melhorNota = n; melhor = i; }
    }
    if (melhor >= 0 && passo == 1) {
      printf("[fonte] preferida de %s casou por provedor com trilha aproximada\n", p->id);
      fflush(stdout);
    }
  }
  return melhor;
}

static int escolherManual(const FontePref *p);

// A MEMORIA DO AUTOMATICO, lado de escrita. So entra fonte COM bingeGroup e so
// quando nao ha escolha manual no titulo. Mesmo grupo: so renova o carimbo (e
// so se tiver mais de um dia, para nao reescrever o arquivo a cada episodio).
// Grupo diferente que tocou: substitui — e a release que de fato existe hoje.
int fontepref_auto_guardar(const char *id, const Stream *s) {
  char base[24];
  int k;
  FontePref novo;
  long long agora = (long long)time(NULL);
  if (!s || !s->provedor[0]) return 0;
  if (!carregado) fontepref_iniciar();
  fontepref_id_base(id, base, sizeof base);
  // A ESCOLHA A MAO MANDA: com uma em vigor (gravada, nao vencida, visivel
  // pelos ajustes) a que o automatico tocou nao e guardada.
  if (!base[0] || fontepref_do_titulo(base)) return 0;
  montar(base, s, &novo);
  k = autoAchar(base);
  // A mesma de novo: so renova o carimbo, e so passado um dia (nao reescrever
  // o arquivo a cada episodio).
  if (k >= 0 && mesmoRegistro(&tabAuto[k].p, &novo) && agora - tabAuto[k].p.quandoS < 24LL * 3600LL) {
    tabAuto[k].sessao = 1; sessaoMarcar(base); return 0;
  }
  if (k < 0) {
    if (nAuto >= FONTEPREF_MAX) {
      int i, velho = 0;
      for (i = 1; i < nAuto; i++) if (tabAuto[i].p.quandoS < tabAuto[velho].p.quandoS) velho = i;
      k = velho;
    } else k = nAuto++;
  }
  tabAuto[k].p = novo;
  tabAuto[k].sessao = 1;
  sessaoMarcar(base);
  autoGravar();
  printf("[fonte] fonte que o automatico tocou lembrada para %s: %s / binge=%s / lancamento=%s / nivel=%s\n", base,
         novo.provedor, novo.bingeGroup[0] ? novo.bingeGroup : "-",
         novo.lancamento[0] ? novo.lancamento : "-", novo.nivel[0] ? novo.nivel : "-");
  fflush(stdout);
  return 1;
}

static const Stream *itemGlobal(int i, void *ctx) { (void)ctx; return stream_item(i); }
static const Stream *itemVetor(int i, void *ctx) { return &((const Stream *)ctx)[i]; }

// POINTS OF THE RELEASE: 1000 same addon + same fingerprint with a group, 300
// without a group, 0 otherwise. Without an addon on either side it is 0: a
// fingerprint alone ("1080p webdl") is shared by unrelated releases.
static int pontosLanc(const char *provedor, const char *lanc, const Stream *s) {
  char l[FONTEPREF_LANC];
  if (!lanc[0] || !provedor[0] || !s->provedor[0] || strcasecmp(s->provedor, provedor)) return 0;
  fontepref_lancamento(s, l, sizeof l);
  if (!l[0] || strcmp(l, lanc)) return 0;
  return lanc[0] != '|' ? 1000 : 300;
}

// O MESMO bingeGroup VEM PRIMEIRO (dono, 16/10, como o Nuvio): o addon pos a
// mesma etiqueta na mesma release/pacote, entao ela vale mais que tudo o resto
// somado. As outras regras (lancamento, nivel, provedor) so desempatam ou
// assumem quando nenhuma fonte de hoje tem a etiqueta.
#define PTS_BINGE 5000

// A pontuacao de uma fonte contra a escolha manual (fontepref.h tem a tabela).
// -1 = nao casa por nada; *como = o criterio de maior peso, para o log.
static int casar(const FontePref *p, const Stream *s, const char **como) {
  int pts = 0, lp, best = 0;
  const char *c = "";
  if (!s) return -1;
  // The release only counts when no remembered audio mark is missing today
  // (the rule of fontepref.h: whoever picked DUB never lands on an ORIGINAL),
  // so an equal fingerprint can never pull a source with the wrong audio.
  if ((lp = pontosLanc(p->provedor, p->lancamento, s)) > 0) {
    char t[FONTEPREF_TRILHA];
    fontepref_trilha(s, t, sizeof t);
    if (!contemMarcas(t, p->trilha)) lp = 0;
  }
  if (lp > 0) {
    pts += lp; best = lp; c = lp >= 1000 ? "lancamento" : "lancamento sem grupo";
  }
  if (cfgBinge && p->bingeGroup[0] && s->bingeGroup[0] && !strcmp(s->bingeGroup, p->bingeGroup)) {
    pts += PTS_BINGE; best = PTS_BINGE; c = "bingeGroup";
  }
  if (s->provedor[0] && !strcasecmp(s->provedor, p->provedor)) {
    char t[FONTEPREF_TRILHA];
    fontepref_trilha(s, t, sizeof t);
    // THE TIER: read only for a source of the same addon at the same
    // resolution as the remembered one, and only when the remembered one had
    // a tier — the scan costs nothing for everything else. Same audio guard as
    // the release.
    if (p->nivel[0] && s->altura == p->altura && contemMarcas(t, p->trilha)) {
      char nv[FONTEPREF_NIVEL];
      fontepref_nivel(s, nv, sizeof nv);
      if (nv[0] && !strcmp(nv, p->nivel)) { pts += 400; if (400 > best) { best = 400; c = "nivel (SeaDex/tier)"; } }
    }
    if (!strcmp(t, p->trilha)) { pts += 200; if (200 > best) { best = 200; c = "provedor e audio"; } }
    else if (contemMarcas(t, p->trilha)) { pts += 100; if (100 > best) { best = 100; c = "provedor e audio aproximado"; } }
  }
  if (!pts) return -1;
  if (como) *como = c;
  return pts + nota(s, p);
}

static int melhorManual(const FontePref *p, int total, const Stream *(*item)(int, void *),
                        void *ctx, const char **como) {
  int i, melhor = -1, melhorPts = -1;
  const char *c = "", *ci;
  for (i = 0; i < total; i++) {
    int pts = casar(p, item(i, ctx), &ci);
    // `>` e nao `>=`: em empate fica o PRIMEIRO da lista (a ordem do addon).
    if (pts > melhorPts) { melhorPts = pts; melhor = i; c = ci; }
  }
  if (como) *como = c;
  return melhor;
}

// A memoria do automatico contra uma lista: a MESMA pontuacao da escolha a mao
// (casar), so entre as fontes que podem tocar aqui (semPerda: nao excluida, nao
// barrada pelas regras do dono, a regra do MP4 da LG, e na mesma resolucao a
// evidencia de release). Resolucao NAO entra (o dono, 16/10: uma "4K" de IA
// nao tira a fonte que ja provou que toca).
static int melhorAuto(const AutoPref *a, int total, const Stream *(*item)(int, void *),
                      void *ctx, int (*semPerda)(int, int, void *), void *ctxPerda, const char **como) {
  int i, melhor = -1, melhorPts = -1;
  const char *c = "", *ci;
  for (i = 0; i < total; i++) {
    int pts = casar(&a->p, item(i, ctx), &ci);
    if (pts <= melhorPts) continue;
    // O mesmo bingeGroup so precisa poder tocar: a regra de evidencia nao o tira.
    // Sem ele o resto soma no maximo 1600, entao >= PTS_BINGE e casou por ele
    // (sem estado global: o preparo do proximo episodio roda em outro fio).
    if (semPerda && !semPerda(i, pts >= PTS_BINGE, ctxPerda)) continue;
    melhorPts = pts; melhor = i; c = ci;
  }
  if (como) *como = c;
  return melhor;
}

static int semPerdaGlobal(int i, int forte, void *ctx) { (void)ctx; return stream_grupo_sem_perda(i, forte); }

// Lado de leitura: ver melhorAuto.
static int escolherAuto(const char *id) {
  char base[24];
  const char *como = "";
  int k, melhor;
  if (!carregado) fontepref_iniciar();
  fontepref_id_base(id, base, sizeof base);
  k = autoAchar(base);
  if (!autoUsavel(k)) return -1;
  melhor = melhorAuto(&tabAuto[k], stream_n(), itemGlobal, NULL, semPerdaGlobal, NULL, &como);
  // Logged when the answer changes, not on every repeat of the same pick.
  { static char ultBase[24]; static int ultMelhor = -2;
    if (melhor >= 0 && (melhor != ultMelhor || strcmp(ultBase, base))) {
      printf("[fonte] preferida de %s na posicao %d pela fonte que o automatico tocou, por %s (lancamento=%s)\n",
             base, melhor, como, tabAuto[k].p.lancamento[0] ? tabAuto[k].p.lancamento : "-");
      fflush(stdout);
    }
    ultMelhor = melhor; snprintf(ultBase, sizeof ultBase, "%s", base); }
  return melhor;
}

const FontePref *fontepref_memoria(const char *id) {
  const FontePref *p = fontepref_do_titulo(id);
  char base[24];
  int k;
  if (p) return p;
  if (!carregado) fontepref_iniciar();
  fontepref_id_base(id, base, sizeof base);
  k = autoAchar(base);
  return autoUsavel(k) ? &tabAuto[k].p : NULL;
}

int fontepref_escolher_manual(const char *id) {
  const FontePref *p = fontepref_do_titulo(id);
  return p ? escolherManual(p) : -1;
}

int fontepref_escolher(const char *id) {
  const FontePref *p = fontepref_do_titulo(id);
  return p ? escolherManual(p) : escolherAuto(id);
}

int fontepref_escolher_lista(const char *id, const Stream *fontes, int total,
                             int (*semPerda)(int, int, void *), void *ctx) {
  if (!fontes || total < 1) return -1;
  const FontePref *p = fontepref_do_titulo(id);
  if (p) return melhorManual(p, total, itemVetor, (void *)fontes, NULL);
  char base[24]; fontepref_id_base(id, base, sizeof base);
  int k = autoAchar(base);
  if (!autoUsavel(k)) return -1;
  return melhorAuto(&tabAuto[k], total, itemVetor, (void *)fontes, semPerda, ctx, NULL);
}

// --- NEXT: A FONTE QUE ESTA TOCANDO (dono, 16/10, como o Nuvio) -----------
// So na RAM: o bingeGroup e o add-on da fonte que tocou de verdade agora.
// Lido tambem pelo worker do proximo episodio, dai a trava.
void fontepref_tocando(const char *id, const Stream *s) {
  char base[24];
  if (!id || !s) return;
  fontepref_id_base(id, base, sizeof base);
  pthread_mutex_lock(&travaTocando);
  montar(base, s, &tocandoPref);
  pthread_mutex_unlock(&travaTocando);
}

int fontepref_tocando_de(const char *id, char *provedor, unsigned tamProv, char *binge, unsigned tamBinge) {
  char base[24];
  int ok;
  if (!id || !cfgBinge) return 0;
  fontepref_id_base(id, base, sizeof base);
  pthread_mutex_lock(&travaTocando);
  ok = base[0] && tocandoPref.bingeGroup[0] && !strcmp(base, tocandoPref.id);
  if (ok && provedor) snprintf(provedor, tamProv, "%s", tocandoPref.provedor);
  if (ok && binge) snprintf(binge, tamBinge, "%s", tocandoPref.bingeGroup);
  pthread_mutex_unlock(&travaTocando);
  return ok;
}

static int bingeTocando(const char *id, int total, const Stream *(*item)(int, void *), void *ctx,
                        int (*podeTocar)(int, int, void *), void *ctxPode) {
  char base[24];
  FontePref p;
  int i, melhor = -1, melhorPts = -1;
  if (!id || !cfgBinge) return -1;
  fontepref_id_base(id, base, sizeof base);
  pthread_mutex_lock(&travaTocando);
  p = tocandoPref;
  pthread_mutex_unlock(&travaTocando);
  if (!base[0] || !p.bingeGroup[0] || strcmp(base, p.id)) return -1;
  // Broad groups share releases: use the playing release/audio to break ties.
  for (i = 0; i < total; i++) {
    const Stream *s = item(i, ctx);
    if (!s || strcmp(s->bingeGroup, p.bingeGroup) || (podeTocar && !podeTocar(i, 1, ctxPode))) continue;
    int pts = casar(&p, s, NULL);
    if (pts > melhorPts) { melhorPts = pts; melhor = i; }
  }
  return melhor;
}

int fontepref_binge_tocando(const char *id) {
  int i = bingeTocando(id, stream_n(), itemGlobal, NULL, semPerdaGlobal, NULL);
  if (i >= 0) { printf("[fonte] next: mesmo bingeGroup da fonte que tocava, posicao %d\n", i); fflush(stdout); }
  return i;
}

int fontepref_escolher_lista_next(const char *id, const Stream *fontes, int total,
                                  int (*semPerda)(int, int, void *), void *ctx) {
  int i;
  if (!fontes || total < 1) return -1;
  i = bingeTocando(id, total, itemVetor, (void *)fontes, semPerda, ctx);
  if (i >= 0) { printf("[fonte] next (preparado): mesmo bingeGroup da fonte que tocava, posicao %d\n", i); fflush(stdout); return i; }
  return fontepref_escolher_lista(id, fontes, total, semPerda, ctx);
}

static int escolherManual(const FontePref *p) {
  const char *como = "";
  // UMA passada pela lista inteira, por pontos (fontepref.h). Nao casou nada:
  // -1, e o automatico assume como sempre — a lembrada nunca trava a escolha.
  int melhor = melhorManual(p, stream_n(), itemGlobal, NULL, &como);
  if (melhor >= 0)
    printf("[fonte] preferida de %s na posicao %d por %s (lancamento=%s)\n", p->id, melhor, como,
           p->lancamento[0] ? p->lancamento : "-");
  else
    printf("[fonte] preferida de %s (%s / %s) nao esta na lista de hoje, indo de automatico\n",
           p->id, p->provedor, p->trilha[0] ? p->trilha : "sem marca");
  fflush(stdout);
  return melhor;
}
