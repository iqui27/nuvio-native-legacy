// Coletor do aparelho e do cliente do Farol — ver farolperfil.h (issue 8).
// Sem SDL, sem rede, sem dados.h, sem alocacao dinamica, sem _Thread_local.
#include "farolperfil.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif

/* --- constantes fechadas do schema (espelham farolredator.c) -------------- */

static const char *const PLATS_VALIDAS =
  "webos|tizen_wgt|tizen_tpk|android|outra";
static const char *const BUILDS_VALIDOS =
  "webos-ipk|tizen-wgt|tizen-tpk|tizen-tpk40|tizen-tpk60|tizen-tpk65|android-apk|debug";
static const char *const BACKENDS_VALIDOS =
  "starfish|avplay|tpk|media3|outro";
static const char *const IDIOMAS_VALIDOS =
  "pt-BR|en|es|fr|de|it|ar|bg|ca|cs|da|el|fi|he|hi|hr|hu|id|ja|ko|nl|no|pl|ro|"
  "ru|sk|sv|tr|uk|vi|zh-CN|zh-TW";
static const char *const GPUS_VALIDAS =
  "auto:alto|auto:medio|auto:baixo|alto|medio|baixo|desconhecido";
static const char *const QUALIDADES_VALIDAS =
  "auto|4k|1080p|720p|480p";
static const char *const DTS_VALIDOS =
  "auto|ac3|aac|direto|desligado";
static const char *const VER_TAGS_VALIDAS =
  "ipk|webos|tizen|wgt|tpk|tpk40|tpk60|tpk65|apk|android|dev|debug";

static int na_lista(const char *s, const char *opcoes) {
  size_t n;
  const char *p;
  if (!s || !*s || !opcoes) return 0;
  n = strlen(s);
  for (p = opcoes; *p;) {
    const char *fim = strchr(p, '|');
    size_t len = fim ? (size_t)(fim - p) : strlen(p);
    if (len == n && memcmp(s, p, n) == 0) return 1;
    p += len;
    if (*p) p++;
  }
  return 0;
}

static void copiar_str(char *dst, size_t cap, const char *src) {
  size_t n;
  if (!dst || !cap) return;
  if (!src) { dst[0] = '\0'; return; }
  n = strlen(src);
  if (n >= cap) n = cap - 1;
  memcpy(dst, src, n);
  dst[n] = '\0';
}

/* --- validadores puros (IPv4, versao, commit, modelo) --------------------- */

static int eh_digito(unsigned char c) { return c >= '0' && c <= '9'; }
static int eh_letra(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static int eh_hex(unsigned char c) {
  return eh_digito(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* Parseia inteiro decimal estrito no intervalo [min_v, max_v]. */
static int parse_int_faixa(const char *s, size_t n, int min_v, int max_v, int *out) {
  long long v = 0;
  size_t i = 0;
  if (!s || !n) return 0;
  while (i < n && s[i] == ' ') i++;
  while (n > i && s[n - 1] == ' ') n--;
  if (i >= n) return 0;
  for (; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (!eh_digito(c)) return 0;
    v = v * 10 + (c - '0');
    if (v > (long long)max_v) return 0;
  }
  if (v < (long long)min_v) return 0;
  if (out) *out = (int)v;
  return 1;
}

/* Numero WHATWG de IPv4 (decimal, octal ou 0x hex) — rejeita IPs disfarcados. */
static int ipv4_num(const char *p, size_t n, unsigned long long *out) {
  unsigned long long v = 0;
  int base = 10;
  size_t i = 0;
  if (!n) return 0;
  if (p[0] == '0' && n > 1) {
    if (p[1] == 'x' || p[1] == 'X') { base = 16; i = 2; }
    else { base = 8; i = 1; }
  }
  for (; i < n; i++) {
    int d;
    unsigned char c = (unsigned char)p[i];
    if (eh_digito(c)) d = c - '0';
    else if (base == 16 && c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else if (base == 16 && c >= 'A' && c <= 'F') d = c - 'A' + 10;
    else return 0;
    if (d >= base) return 0;
    if (v <= 0xffffffffULL) {
      v = v * (unsigned)base + (unsigned)d;
      if (v > 0xffffffffULL) v = 0x100000000ULL;
    }
  }
  *out = v;
  return 1;
}

/* 1 se a palavra tem cara de IPv4 (com pelo menos um ponto, ex.: 192.168.1.1,
 * 127.0.1, 0xc6.0x33.0x64.0x2a). */
static int palavra_parece_ip(const char *s, size_t n) {
  const char *p, *fim, *ult;
  unsigned long long v;
  int partes = 0;
  if (!n || !memchr(s, '.', n)) return 0;
  if (s[n - 1] == '.') n--;
  if (!n) return 0;
  fim = s + n;
  ult = fim;
  while (ult > s && ult[-1] != '.') ult--;
  if ((size_t)(fim - ult) == 0 || !ipv4_num(ult, (size_t)(fim - ult), &v))
    return 0;
  for (p = s;;) {
    const char *q = memchr(p, '.', (size_t)(fim - p));
    size_t len = q ? (size_t)(q - p) : (size_t)(fim - p);
    if (++partes > 4 || !len || !ipv4_num(p, len, &v)) return 0;
    if (!q) break;
    if (v > 255) return 0;
    p = q + 1;
  }
  return partes >= 2;
}

/* 1 se a palavra comeca com "www." (sem distinguir caixa). */
static int palavra_parece_www(const char *s, size_t n) {
  if (n < 5) return 0;
  return (s[0] == 'w' || s[0] == 'W') &&
         (s[1] == 'w' || s[1] == 'W') &&
         (s[2] == 'w' || s[2] == 'W') &&
         s[3] == '.';
}

/* 1 se contem sequencia IPv4 decimal de 4 octetos em qualquer posicao. */
static int contem_ipv4_embutido(const char *s, size_t n) {
  size_t i;
  for (i = 0; i < n; i++) {
    size_t j = i;
    int oct;
    if (!eh_digito((unsigned char)s[i])) continue;
    if (i > 0 && eh_digito((unsigned char)s[i - 1])) continue;
    for (oct = 0; oct < 4; oct++) {
      size_t d = 0;
      while (j < n && eh_digito((unsigned char)s[j]) && d < 3) { j++; d++; }
      if (!d) break;
      if (oct < 3) {
        if (j >= n || s[j] != '.') break;
        j++;
      } else {
        if (j == n || !eh_digito((unsigned char)s[j])) return 1;
      }
    }
  }
  return 0;
}

/* 1 se o modelo casa a gramatica estrita do farolredator.c (aparelho.modelo). */
static int modelo_redator_ok(const char *s) {
  static const char *const prefixos[] = {
    "OLED", "QN", "QE", "UE", "UN", "UA", "KD-", "XR-", "TCL"
  };
  size_t i;
  if (!s || !*s) return 0;
  if (strcmp(s, "x") == 0) return 1;
  for (i = 0; i < sizeof prefixos / sizeof *prefixos; i++) {
    size_t n = strlen(prefixos[i]);
    const char *p;
    size_t tela, serie, numero, sufixo;
    if (strncmp(s, prefixos[i], n) != 0) continue;
    p = s + n;
    tela = strspn(p, "0123456789");
    if (tela != 2) return 0;
    p += tela;
    serie = strspn(p, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    if (!serie || serie > (i ? 2u : 1u)) return 0;
    p += serie;
    numero = strspn(p, "0123456789");
    if (!numero || numero > (i ? 4u : 2u)) return 0;
    p += numero;
    sufixo = strspn(p, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    return !p[sufixo] && sufixo <= (i ? 6u : 3u);
  }
  /* Sem excecao por fora dos prefixos: a gramatica e a do modeloOk do
   * servidor JA PUBLICADO (servidor/recomendacoes/src/farol.js), que rejeita
   * o registro inteiro com 422 antes de gravar. LG fora da linha OLED
   * ("50NANO80ASA", "43LM6300PSB") comeca com digito e nao tem caminho —
   * sai "x" como no pai, ate um alargamento do modeloOk ser deployado. */
  return 0;
}

/* Valida versao numerica ("4.9", "4.9.7", "6.5", "14", "2.1.0") ou com sufixo
 * fechado ("2.1.0-ipk"). Rejeita texto livre, IPs e 4+ partes. */
static int parse_versao(const char *s, size_t len, char out[24], int *maior_out, int exigir_maior_pos) {
  const char *p, *fim, *hifen;
  size_t n_num, i;
  int partes = 1, digitos = 0, maior = 0, primeiro = 1;
  char tmp[24];

  if (out) strcpy(out, "?");
  if (maior_out) *maior_out = -1;
  if (!s || !len) return 0;

  p = s;
  fim = s + len;
  while (p < fim && *p == ' ') p++;
  while (fim > p && fim[-1] == ' ') fim--;
  len = (size_t)(fim - p);
  if (!len || len > FAROL_PERFIL_VER_MAX) return 0;

  memcpy(tmp, p, len);
  tmp[len] = '\0';

  hifen = strchr(tmp, '-');
  if (hifen) {
    if (!na_lista(hifen + 1, VER_TAGS_VALIDAS)) return 0;
    n_num = (size_t)(hifen - tmp);
  } else {
    n_num = len;
  }
  if (!n_num || n_num > 14) return 0;
  if (!memchr(tmp, '.', n_num) && n_num > 2) return 0;

  for (i = 0; i < n_num; i++) {
    unsigned char c = (unsigned char)tmp[i];
    if (eh_digito(c)) {
      if (++digitos > 4) return 0;
      if (primeiro) maior = maior * 10 + (c - '0');
    } else if (c == '.' && digitos > 0 && partes < 3) {
      partes++;
      digitos = 0;
      primeiro = 0;
    } else {
      return 0;
    }
  }
  if (!digitos || partes > 3) return 0;
  if (exigir_maior_pos && (maior < 1 || maior > 999)) return 0;

  if (out) strcpy(out, tmp);
  if (maior_out) *maior_out = maior;
  return 1;
}

static int commit_valido(const char *s) {
  size_t n, i;
  int tem_letra = 0;
  if (!s) return 0;
  n = strlen(s);
  if (n < 7 || n > FAROL_PERFIL_COMMIT_MAX) return 0;
  for (i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c >= 'a' && c <= 'f') tem_letra = 1;
    else if (!eh_digito(c)) return 0;
  }
  return tem_letra;
}

/* --- 1. Filtro de modelo -------------------------------------------------- */

int farolperfil_filtrar_modelo(const char *entrada, char out[FAROL_PERFIL_MODELO_MAX + 1]) {
  const char *p, *fim;
  size_t len, i, n = 0, hex_run = 0;
  int tem_alnum = 0;
  char tmp[FAROL_PERFIL_MODELO_MAX + 1];

  if (!out) return 0;
  if (!entrada) { strcpy(out, "?"); return 0; }

  p = entrada;
  while (*p == ' ') p++;
  fim = p + strlen(p);
  while (fim > p && fim[-1] == ' ') fim--;
  len = (size_t)(fim - p);

  /* Rejeita vazio ou bloco exagerado (> 128 bytes: linha de log/lixo). */
  if (!len || len > 128) { strcpy(out, "?"); return 0; }

  /* Valida a entrada INTEIRA (nao so os 32 primeiros bytes), para um sufixo
   * malicioso alem do byte 32 ainda rejeitar a cadeia inteira para "?". */
  for (i = 0; i < len; i++) {
    unsigned char c = (unsigned char)p[i];
    if (eh_letra(c) || eh_digito(c)) {
      tem_alnum = 1;
    } else if (c != '-' && c != '_' && c != ' ' && c != '.') {
      strcpy(out, "?");
      return 0;
    }
    if (c == '.' && i + 1 < len && p[i + 1] == '.') {
      strcpy(out, "?");
      return 0;
    }
    if (eh_hex(c)) {
      if (++hex_run >= 20) { strcpy(out, "?"); return 0; }
    } else {
      hex_run = 0;
    }
  }
  if (!tem_alnum) { strcpy(out, "?"); return 0; }

  /* Checa cada palavra separada por espaco contra IPv4 e "www.". */
  if (contem_ipv4_embutido(p, len)) { strcpy(out, "?"); return 0; }
  for (i = 0; i < len;) {
    size_t j;
    while (i < len && p[i] == ' ') i++;
    j = i;
    while (j < len && p[j] != ' ') j++;
    if (j > i) {
      if (palavra_parece_ip(p + i, j - i) || palavra_parece_www(p + i, j - i)) {
        strcpy(out, "?");
        return 0;
      }
    }
    i = j;
  }

  /* Copia ate 32 bytes (colapsando espacos seguidos em um so) e tira espaco final. */
  for (i = 0; i < len && n < FAROL_PERFIL_MODELO_MAX; i++) {
    char c = p[i];
    if (c == ' ' && n > 0 && tmp[n - 1] == ' ') continue;
    tmp[n++] = c;
  }
  while (n > 0 && tmp[n - 1] == ' ') n--;
  tmp[n] = '\0';

  tem_alnum = 0;
  for (i = 0; i < n; i++) {
    unsigned char c = (unsigned char)tmp[i];
    if (eh_letra(c) || eh_digito(c)) { tem_alnum = 1; break; }
  }
  if (!n || !tem_alnum) { strcpy(out, "?"); return 0; }

  memcpy(out, tmp, n + 1);
  return 1;
}

/* --- 2. Parsers puros ----------------------------------------------------- */

int farolperfil_parse_tela(const char *texto, int *w, int *h) {
  const char *p, *fim, *x;
  int vw = -1, vh = -1;
  if (w) *w = -1;
  if (h) *h = -1;
  if (!texto || !*texto) return 0;
  p = texto;
  while (*p == ' ') p++;
  fim = p + strlen(p);
  while (fim > p && fim[-1] == ' ') fim--;
  if (p >= fim) return 0;
  x = memchr(p, 'x', (size_t)(fim - p));
  if (!x) x = memchr(p, 'X', (size_t)(fim - p));
  if (!x || x == p || x + 1 >= fim) return 0;
  if (!parse_int_faixa(p, (size_t)(x - p), 1, 16384, &vw)) return 0;
  if (!parse_int_faixa(x + 1, (size_t)(fim - (x + 1)), 1, 16384, &vh)) return 0;
  if (w) *w = vw;
  if (h) *h = vh;
  return 1;
}

int farolperfil_parse_android_info(const char *env, FarolTvInfoAndroid *out) {
  const char *p, *fim_linha;
  const char *campos[4] = { NULL, NULL, NULL, NULL };
  size_t lens[4] = { 0, 0, 0, 0 };
  int nc = 0, ok = 0;
  char buf[192];

  if (!out) return 0;
  strcpy(out->modelo, "?");
  out->sdk = -1;
  strcpy(out->so, "?");
  out->so_maior = -1;
  strcpy(out->versao, "?");

  if (!env || !*env) return 0;
  /* So a primeira linha (nao cruza \r ou \n). */
  fim_linha = env + strcspn(env, "\r\n");
  p = env;
  while (nc < 4) {
    const char *sep = memchr(p, '|', (size_t)(fim_linha - p));
    campos[nc] = p;
    lens[nc] = sep ? (size_t)(sep - p) : (size_t)(fim_linha - p);
    nc++;
    if (!sep) break;
    p = sep + 1;
  }

  if (nc >= 1 && lens[0] > 0 && lens[0] < sizeof buf) {
    memcpy(buf, campos[0], lens[0]);
    buf[lens[0]] = '\0';
    if (farolperfil_filtrar_modelo(buf, out->modelo)) ok = 1;
  }
  if (nc >= 2 && lens[1] > 0) {
    if (parse_int_faixa(campos[1], lens[1], 1, 999, &out->sdk)) ok = 1;
  }
  if (nc >= 3 && lens[2] > 0) {
    if (parse_versao(campos[2], lens[2], out->so, &out->so_maior, 1)) ok = 1;
  }
  if (nc >= 4 && lens[3] > 0) {
    if (parse_versao(campos[3], lens[3], out->versao, NULL, 0)) ok = 1;
  }
  return ok;
}

/* Procura `chave` ("modelo=", "tizen=", "tela=") em borda de palavra na linha. */
static const char *achar_chave_linha(const char *ini, const char *fim, const char *chave) {
  size_t klen = strlen(chave);
  const char *p = ini;
  while (p < fim) {
    const char *q = strstr(p, chave);
    if (!q || q >= fim) return NULL;
    if (q == ini || q[-1] == ' ' || q[-1] == '\t') return q + klen;
    p = q + 1;
  }
  return NULL;
}

static int parse_tpk_uma_linha(const char *linha, size_t n, FarolTvInfoTpk *out) {
  static const char *const delim[] = {
    " tizen=", " host=", " dotnet=", " tela=", " sdk=", " app=", " placa=", " fw="
  };
  char copia[512];
  const char *marca, *corpo, *fim, *v;
  int ok = 0;

  if (n >= sizeof copia) n = sizeof copia - 1;
  memcpy(copia, linha, n);
  copia[n] = '\0';

  marca = strstr(copia, "[tv] ");
  if (!marca) return 0;
  /* Rejeita prefixo colado (ex.: "foo[tv] "). */
  if (marca != copia && marca[-1] != ' ' && marca[-1] != '\t') return 0;
  corpo = marca + 5;
  fim = copia + n;

  v = achar_chave_linha(corpo, fim, "modelo=");
  if (v) {
    const char *ate = fim;
    size_t i, mlen;
    char mbuf[192];
    for (i = 0; i < sizeof delim / sizeof *delim; i++) {
      const char *d = strstr(v, delim[i]);
      if (d && d < ate) ate = d;
    }
    mlen = (size_t)(ate - v);
    if (mlen > 0 && mlen < sizeof mbuf) {
      memcpy(mbuf, v, mlen);
      mbuf[mlen] = '\0';
      if (farolperfil_filtrar_modelo(mbuf, out->modelo)) ok = 1;
    }
  }

  v = achar_chave_linha(corpo, fim, "tizen=");
  if (v) {
    size_t vlen = strcspn(v, " \t");
    if (parse_versao(v, vlen, out->so, &out->so_maior, 1)) ok = 1;
  }

  v = achar_chave_linha(corpo, fim, "tela=");
  if (v) {
    size_t vlen = strcspn(v, " \t");
    char tbuf[32];
    if (vlen > 0 && vlen < sizeof tbuf) {
      memcpy(tbuf, v, vlen);
      tbuf[vlen] = '\0';
      if (farolperfil_parse_tela(tbuf, &out->tela_w, &out->tela_h)) ok = 1;
    }
  }
  return ok;
}

int farolperfil_parse_tpk_tv(const char *texto, FarolTvInfoTpk *out) {
  const char *p;
  if (!out) return 0;
  strcpy(out->modelo, "?");
  strcpy(out->so, "?");
  out->so_maior = -1;
  out->tela_w = -1;
  out->tela_h = -1;
  if (!texto || !*texto) return 0;

  for (p = texto; *p;) {
    size_t n = strcspn(p, "\r\n");
    if (n > 0 && parse_tpk_uma_linha(p, n, out)) return 1;
    p += n;
    while (*p == '\r' || *p == '\n') p++;
  }
  return 0;
}

static int parse_linha_mem_kb(const char *p, const char *fim, const char *prefixo,
                              int exigir_pos, int *mb_out) {
  size_t plen = strlen(prefixo);
  unsigned long long kb = 0;
  int digitos = 0;

  if ((size_t)(fim - p) < plen || memcmp(p, prefixo, plen) != 0) return 0;
  p += plen;
  while (p < fim && (*p == ' ' || *p == '\t')) p++;
  while (p < fim && eh_digito((unsigned char)*p)) {
    unsigned char d = (unsigned char)(*p - '0');
    if (kb > ((unsigned long long)INT_MAX * 1024ULL - d) / 10ULL) return 0;
    kb = kb * 10ULL + d;
    digitos = 1;
    p++;
  }
  if (!digitos) return 0;
  if (p >= fim || (*p != ' ' && *p != '\t')) return 0;
  while (p < fim && (*p == ' ' || *p == '\t')) p++;
  if ((size_t)(fim - p) < 2 || p[0] != 'k' || p[1] != 'B') return 0;
  p += 2;
  while (p < fim && (*p == ' ' || *p == '\t')) p++;
  if (p != fim) return 0;
  if (exigir_pos && kb == 0) return 0;
  *mb_out = (int)(kb / 1024ULL);
  return 1;
}

int farolperfil_parse_meminfo(const char *texto, int *total_mb, int *livre_mb) {
  const char *p;
  int tot = -1, liv = -1;
  if (total_mb) *total_mb = -1;
  if (livre_mb) *livre_mb = -1;
  if (!texto || !*texto) return 0;

  for (p = texto; *p;) {
    size_t n = strcspn(p, "\r\n");
    const char *fim = p + n;
    if (tot < 0) parse_linha_mem_kb(p, fim, "MemTotal:", 1, &tot);
    if (liv < 0) parse_linha_mem_kb(p, fim, "MemAvailable:", 0, &liv);
    if (tot >= 0 && liv >= 0) break;
    p = fim;
    while (*p == '\r' || *p == '\n') p++;
  }
  if (total_mb) *total_mb = tot;
  if (livre_mb) *livre_mb = liv;
  return (tot >= 0 || liv >= 0) ? 1 : 0;
}

int farolperfil_parse_wgt_info(const char *texto, FarolTvInfoWgt *out) {
  const char *fim, *sep;
  char mbuf[192];
  size_t mlen;
  int ok = 0;

  if (!out) return 0;
  strcpy(out->modelo, "?");
  strcpy(out->so, "?");
  out->so_maior = -1;
  if (!texto || !*texto) return 0;

  fim = texto + strcspn(texto, "\r\n");
  sep = memchr(texto, '|', (size_t)(fim - texto));
  mlen = sep ? (size_t)(sep - texto) : (size_t)(fim - texto);
  if (mlen > 0 && mlen < sizeof mbuf) {
    memcpy(mbuf, texto, mlen);
    mbuf[mlen] = '\0';
    if (farolperfil_filtrar_modelo(mbuf, out->modelo)) ok = 1;
  }
  if (sep && sep + 1 < fim) {
    if (parse_versao(sep + 1, (size_t)(fim - (sep + 1)), out->so, &out->so_maior, 1))
      ok = 1;
  }
  return ok;
}

int farolperfil_parse_webos_nyx(const char *json, char so[24], int *so_maior) {
  static const char chave[] = "\"webos_release\"";
  const char *p, *q, *ini;
  size_t n;
  if (so) strcpy(so, "?");
  if (so_maior) *so_maior = -1;
  if (!json || !*json) return 0;
  p = strstr(json, chave);
  if (!p) return 0;
  p += sizeof chave - 1;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  if (*p != ':') return 0;
  p++;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  if (*p != '"') return 0;
  ini = ++p;
  for (q = ini; *q && *q != '"'; q++) {
    if (*q == '\\' || (unsigned char)*q < 0x20) return 0;
  }
  if (*q != '"') return 0;
  n = (size_t)(q - ini);
  /* Corta sufixo de build (ex.: "4.9.7-123" -> "4.9.7") mantendo so numerico. */
  {
    const char *h = memchr(ini, '-', n);
    if (h) n = (size_t)(h - ini);
  }
  return parse_versao(ini, n, so, so_maior, 1);
}

int farolperfil_parse_webos_starfish(const char *texto, char so[24], int *so_maior) {
  const char *p;
  if (so) strcpy(so, "?");
  if (so_maior) *so_maior = -1;
  if (!texto || !*texto) return 0;
  for (p = texto; *p;) {
    size_t n = strcspn(p, "\r\n");
    const char *fim = p + n;
    const char *r = p;
    while (r < fim) {
      const char *m = strstr(r, "release ");
      if (!m || m >= fim) break;
      if (m == p || (!eh_letra((unsigned char)m[-1]) && !eh_digito((unsigned char)m[-1]) && m[-1] != '_')) {
        const char *v = m + 8;
        const char *ve = v;
        while (ve < fim && (eh_digito((unsigned char)*ve) || *ve == '.')) ve++;
        if (ve > v && (ve == fim || *ve == ' ' || *ve == '\t' || *ve == '-' || *ve == '_')) {
          if (parse_versao(v, (size_t)(ve - v), so, so_maior, 1)) return 1;
        }
      }
      r = m + 8;
    }
    p = fim;
    while (*p == '\r' || *p == '\n') p++;
  }
  return 0;
}

/* --- 3. Defaults de compilacao -------------------------------------------- */

static const char *plat_padrao(void) {
#if defined(NV_WEBOS) || defined(WEBOS)
  return "webos";
#elif defined(NV_TPK)
  return "tizen_tpk";
#elif defined(__EMSCRIPTEN__)
  return "tizen_wgt";
#elif defined(NV_ANDROID) || defined(__ANDROID__)
  return "android";
#else
  return "outra";
#endif
}

static const char *build_padrao(void) {
#if defined(NV_WEBOS) || defined(WEBOS)
  return "webos-ipk";
#elif defined(NV_TPK40)
  return "tizen-tpk40";
#elif defined(NV_TPK)
  return "tizen-tpk";
#elif defined(__EMSCRIPTEN__)
  return "tizen-wgt";
#elif defined(NV_ANDROID) || defined(__ANDROID__)
  return "android-apk";
#else
  return "debug";
#endif
}

static const char *backend_padrao(void) {
#if defined(NV_WEBOS) || defined(WEBOS)
  return "starfish";
#elif defined(NV_TPK)
  return "tpk";
#elif defined(__EMSCRIPTEN__)
  return "avplay";
#elif defined(NV_ANDROID) || defined(__ANDROID__)
  return "media3";
#else
  return "outro";
#endif
}

void farolperfil_padrao(FarolPerfil *out) {
  if (!out) return;
  memset(out, 0, sizeof *out);

  copiar_str(out->aparelho.plataforma, sizeof out->aparelho.plataforma, plat_padrao());
  strcpy(out->aparelho.so, "?");
  out->aparelho.so_maior = -1;
  out->aparelho.sdk = -1;
  strcpy(out->aparelho.modelo, "?");
  out->aparelho.ram_total_mb = -1;
  out->aparelho.ram_livre_mb = -1;
  out->aparelho.tela = (FarolPerfilTela){ -1, -1, -1, -1, -1, -1 };
  out->aparelho.audio = (FarolPerfilAudio){ -1, -1, -1, -1 };
  out->aparelho.rede = (FarolPerfilRede){ -1, -1, -1.0 };

  if (!parse_versao(NV_VERSAO, strlen(NV_VERSAO), out->cliente.versao, NULL, 0))
    strcpy(out->cliente.versao, "?");
#ifdef NV_COMMIT
  if (commit_valido(NV_COMMIT))
    copiar_str(out->cliente.commit, sizeof out->cliente.commit, NV_COMMIT);
  else
    strcpy(out->cliente.commit, "?");
#else
  strcpy(out->cliente.commit, "?");
#endif
  copiar_str(out->cliente.build, sizeof out->cliente.build, build_padrao());
  copiar_str(out->cliente.backend, sizeof out->cliente.backend, backend_padrao());
  strcpy(out->cliente.idioma, "?");
  out->cliente.aberto_s = -1;
  strcpy(out->cliente.gpu, "desconhecido");
  out->cliente.ajustes.glass_ui = -1;
  out->cliente.ajustes.qualidade_max[0] = '\0';
  out->cliente.ajustes.tamanho_max_gb = -1.0;
  out->cliente.ajustes.dts[0] = '\0';
  out->cliente.ajustes.dv_mkv = -1;
  out->cliente.ajustes.legenda_auto_sync = -1;
  out->cliente.addons = (FarolPerfilAddons){ -1, -1, -1, 0u };
  out->cliente.debrid_mask = 0u;
  out->cliente.contas_mask = 0u;
}

/* --- 4. Leitura em fio proprio com cache (uma vez, nao bloqueante) -------- */

enum { FP_ST_ZERO = 0, FP_ST_LENDO = 1, FP_ST_PRONTO = 2 };

typedef struct {
  char plataforma[16];
  char meminfo_caminho[256];
  char tpk_log_caminho[256];
  char nyx_caminho[256];
  char starfish_caminho[256];
  char env_android_info[256];
  char env_wgt_info[128];
  char env_tv_model[128];
  char env_tizen_ver[64];
  char env_panel_size[64];
  int  usar_env_real;
  void (*antes_de_ler)(void);
} ColetaFontes;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  pronto_cv = PTHREAD_COND_INITIALIZER;
static FarolPerfil     cache_perfil;
static int             cache_inicializado;
static int             estado_coleta;
#ifdef FAROL_PERFIL_TESTE
static int             leituras_feitas;
#endif
static pthread_t       fio_coleta;
static int             fio_criado;
static ColetaFontes    fontes_ativas;
/* Ultima coleta do TPK saiu sem a linha [tv]: o cache nao e definitivo e o
 * proximo iniciar recoleta (a linha pode tardar mais que o prazo). */
static int             tv_pendente;

/* Overrides recebidos em tempo de execucao (nunca perdidos pela coleta). */
static char              ov_modelo[FAROL_PERFIL_MODELO_MAX + 1];
static int               ov_tem_modelo;
static FarolPerfilTela   ov_tela;
static int               ov_tem_tela;
static FarolPerfilAudio  ov_audio;
static int               ov_tem_audio;
static FarolPerfilRede   ov_rede;
static int               ov_tem_rede;
static FarolPerfilCliente ov_cliente;
static int               ov_tem_cliente;

#ifdef FAROL_PERFIL_TESTE
static int             teste_cfg_ativa;
static ColetaFontes    teste_cfg;
#endif

static void garantir_cache_travado(void) {
  if (!cache_inicializado) {
    farolperfil_padrao(&cache_perfil);
    cache_inicializado = 1;
  }
}

static size_t ler_arquivo_limitado(const char *caminho, char *buf, size_t cap) {
  FILE *f;
  size_t n = 0;
  int tentativas = 0;
  if (!caminho || !caminho[0] || !buf || cap < 2) return 0;
  f = fopen(caminho, "rb");
  if (!f) return 0;
  while (n + 1 < cap) {
    size_t r = fread(buf + n, 1, cap - 1 - n, f);
    n += r;
    if (r == 0) {
      if (ferror(f) && errno == EINTR && ++tentativas < 4) { clearerr(f); continue; }
      break;
    }
  }
  buf[n] = '\0';
  fclose(f);
  return n;
}

/* Le o log do TPK em blocos e aplica cada linha "[tv] ..." sobre `out`: a
 * ultima ocorrencia vence, campo a campo. A linha pode chegar depois de
 * qualquer prefixo — o buffer de 8 KiB deixou de ser o limite. Linha maior
 * que o buffer de linha le so o inicio, como a leitura antiga. Devolve 1 se
 * alguma linha [tv] rendeu campo util. */
static int ler_log_tpk_tv(const char *caminho, FarolTvInfoTpk *out) {
  FILE *f;
  char bloco[2048], linha[512];
  size_t nl = 0;
  int achou = 0;
  if (!caminho || !out) return 0;
  strcpy(out->modelo, "?");
  strcpy(out->so, "?");
  out->so_maior = -1;
  out->tela_w = -1;
  out->tela_h = -1;
  f = fopen(caminho, "rb");
  if (!f) return 0;
  for (;;) {
    size_t r = fread(bloco, 1, sizeof bloco, f);
    size_t i;
    for (i = 0; i < r; i++) {
      char c = bloco[i];
      if (c == '\n' || c == '\r') {
        if (nl) {
          FarolTvInfoTpk cand = *out;
          if (parse_tpk_uma_linha(linha, nl, &cand)) { *out = cand; achou = 1; }
          nl = 0;
        }
      } else if (nl + 1 < sizeof linha) {
        linha[nl++] = c;
      }
    }
    if (r < sizeof bloco) break;
  }
  if (nl) {
    FarolTvInfoTpk cand = *out;
    if (parse_tpk_uma_linha(linha, nl, &cand)) { *out = cand; achou = 1; }
  }
  fclose(f);
  return achou;
}

static long long agora_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

/* O host TPK publica a linha [tv] ~3 s depois do arranque (12 tiques de
 * 250 ms, tizen-tpk/Program.cs e NuvioTpk40/Program40.cs). Uma leitura sem
 * ela NAO e definitiva: o fio trabalhador rele o log a cada TPK_TV_PASSO_MS
 * ate achar a linha ou fechar TPK_TV_PRAZO_MS; fechando sem achar, o proximo
 * farolperfil_iniciar() recoleta (cache "pendente"). */
enum { TPK_TV_PRAZO_MS = 4000, TPK_TV_PASSO_MS = 200 };

static void aplicar_overrides_travado(FarolPerfil *dst) {
  if (ov_tem_modelo) copiar_str(dst->aparelho.modelo, sizeof dst->aparelho.modelo, ov_modelo);
  if (ov_tem_tela)   dst->aparelho.tela = ov_tela;
  if (ov_tem_audio)  dst->aparelho.audio = ov_audio;
  if (ov_tem_rede)   dst->aparelho.rede = ov_rede;
  if (ov_tem_cliente) dst->cliente = ov_cliente;
}

static void montar_fontes_padrao(ColetaFontes *f) {
  memset(f, 0, sizeof *f);
  copiar_str(f->plataforma, sizeof f->plataforma, plat_padrao());
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
  copiar_str(f->meminfo_caminho, sizeof f->meminfo_caminho, "/proc/meminfo");
#endif
#if defined(NV_WEBOS) || defined(WEBOS)
  copiar_str(f->nyx_caminho, sizeof f->nyx_caminho, "/var/run/nyx/os_info.json");
  copiar_str(f->starfish_caminho, sizeof f->starfish_caminho, "/etc/starfish-release");
#endif
  f->usar_env_real = 1;
}

static const char *obter_env_ou_cfg(const char *override_val, int usar_real, const char *nome) {
  if (override_val && override_val[0]) return override_val;
  if (usar_real) return getenv(nome);
  return NULL;
}

/* Devolve 1 quando a coleta do TPK saiu sem a linha [tv] (cache pendente:
 * o proximo iniciar recoleta em vez de tratar a leitura como definitiva). */
static int coletar_fora_da_trava(const ColetaFontes *f, FarolPerfil *local) {
  char buf[8192];
  const char *e;
  int tv_pendente = 0;

  farolperfil_padrao(local);
  if (f->plataforma[0] && na_lista(f->plataforma, PLATS_VALIDAS)) {
    copiar_str(local->aparelho.plataforma, sizeof local->aparelho.plataforma, f->plataforma);
    if (strcmp(f->plataforma, "webos") == 0) {
      strcpy(local->cliente.build, "webos-ipk");
      strcpy(local->cliente.backend, "starfish");
    } else if (strcmp(f->plataforma, "tizen_tpk") == 0) {
      /* NV_TPK40 compila o pacote tpk40: o build de compilacao nao e
       * sobrescrito pelo generico da coleta. */
#ifdef NV_TPK40
      strcpy(local->cliente.build, "tizen-tpk40");
#else
      strcpy(local->cliente.build, "tizen-tpk");
#endif
      strcpy(local->cliente.backend, "tpk");
    } else if (strcmp(f->plataforma, "tizen_wgt") == 0) {
      strcpy(local->cliente.build, "tizen-wgt");
      strcpy(local->cliente.backend, "avplay");
    } else if (strcmp(f->plataforma, "android") == 0) {
      strcpy(local->cliente.build, "android-apk");
      strcpy(local->cliente.backend, "media3");
    }
  }

  if (f->antes_de_ler) f->antes_de_ler();

  /* 1. /proc/meminfo */
  if (f->meminfo_caminho[0] && ler_arquivo_limitado(f->meminfo_caminho, buf, sizeof buf) > 0) {
    farolperfil_parse_meminfo(buf, &local->aparelho.ram_total_mb, &local->aparelho.ram_livre_mb);
  }

  /* 2. Painel (NUVIO_PANEL_SIZE) */
  e = obter_env_ou_cfg(f->env_panel_size, f->usar_env_real, "NUVIO_PANEL_SIZE");
  if (e && e[0]) {
    farolperfil_parse_tela(e, &local->aparelho.tela.w, &local->aparelho.tela.h);
  }

  /* 3. Android (NUVIO_TV_INFO) */
  e = obter_env_ou_cfg(f->env_android_info, f->usar_env_real, "NUVIO_TV_INFO");
  if (e && e[0]) {
    FarolTvInfoAndroid ai;
    if (farolperfil_parse_android_info(e, &ai)) {
      if (strcmp(ai.modelo, "?") != 0)
        copiar_str(local->aparelho.modelo, sizeof local->aparelho.modelo, ai.modelo);
      if (ai.sdk > 0) local->aparelho.sdk = ai.sdk;
      if (strcmp(ai.so, "?") != 0) {
        copiar_str(local->aparelho.so, sizeof local->aparelho.so, ai.so);
        local->aparelho.so_maior = ai.so_maior;
      }
      if (strcmp(local->cliente.versao, "?") == 0 && strcmp(ai.versao, "?") != 0)
        copiar_str(local->cliente.versao, sizeof local->cliente.versao, ai.versao);
    }
  }

  /* 4. TPK ([tv] no log + NUVIO_TV_MODEL) */
  {
    char log_path[320];
    int eh_tpk;
    log_path[0] = '\0';
    if (f->tpk_log_caminho[0]) {
      copiar_str(log_path, sizeof log_path, f->tpk_log_caminho);
    }
#ifdef NV_TPK
    else if (f->usar_env_real) {
      const char *lp = getenv("NUVIO_LOG");
      const char *dd = getenv("NUVIO_DADOS");
      if (lp && lp[0]) copiar_str(log_path, sizeof log_path, lp);
      else if (dd && dd[0]) snprintf(log_path, sizeof log_path, "%s/nuvio.log", dd);
    }
#endif
    eh_tpk = strcmp(local->aparelho.plataforma, "tizen_tpk") == 0;
    if (log_path[0]) {
      FarolTvInfoTpk ti;
      int achou = ler_log_tpk_tv(log_path, &ti);
      if (!achou && eh_tpk) {
        /* A linha [tv] sai ~3 s depois do arranque: rele ate achar ou fechar
         * o prazo, sempre neste fio de coleta — nunca no fio principal. */
        const long long limite = agora_ms() + TPK_TV_PRAZO_MS;
        const struct timespec passo = { 0, TPK_TV_PASSO_MS * 1000000L };
        while (agora_ms() < limite) {
          nanosleep(&passo, NULL);
          if ((achou = ler_log_tpk_tv(log_path, &ti))) break;
        }
      }
      if (achou) {
        if (strcmp(ti.modelo, "?") != 0)
          copiar_str(local->aparelho.modelo, sizeof local->aparelho.modelo, ti.modelo);
        if (strcmp(ti.so, "?") != 0) {
          copiar_str(local->aparelho.so, sizeof local->aparelho.so, ti.so);
          local->aparelho.so_maior = ti.so_maior;
        }
        if (ti.tela_w > 0 && ti.tela_h > 0 && local->aparelho.tela.w < 0) {
          local->aparelho.tela.w = ti.tela_w;
          local->aparelho.tela.h = ti.tela_h;
        }
      } else {
        tv_pendente = eh_tpk;
      }
    } else {
      tv_pendente = eh_tpk;
    }
#if defined(NV_TPK) && !defined(NV_TPK40)
    if (strcmp(local->aparelho.so, "6.0") == 0) strcpy(local->cliente.build, "tizen-tpk60");
    else if (strcmp(local->aparelho.so, "6.5") == 0) strcpy(local->cliente.build, "tizen-tpk65");
#endif
  }

  /* 5. Tizen WASM (NUVIO_FAROL_WGT + NUVIO_TIZEN_VERSION) */
  e = obter_env_ou_cfg(f->env_wgt_info, f->usar_env_real, "NUVIO_FAROL_WGT");
  if (e && e[0]) {
    FarolTvInfoWgt wi;
    if (farolperfil_parse_wgt_info(e, &wi)) {
      if (strcmp(wi.modelo, "?") != 0)
        copiar_str(local->aparelho.modelo, sizeof local->aparelho.modelo, wi.modelo);
      if (strcmp(wi.so, "?") != 0) {
        copiar_str(local->aparelho.so, sizeof local->aparelho.so, wi.so);
        local->aparelho.so_maior = wi.so_maior;
      }
    }
  }
  if (strcmp(local->aparelho.so, "?") == 0) {
    e = obter_env_ou_cfg(f->env_tizen_ver, f->usar_env_real, "NUVIO_TIZEN_VERSION");
    if (e && e[0]) parse_versao(e, strlen(e), local->aparelho.so, &local->aparelho.so_maior, 1);
  }

  /* 6. webOS (/var/run/nyx/os_info.json + /etc/starfish-release) */
  if (strcmp(local->aparelho.so, "?") == 0 && f->nyx_caminho[0]) {
    if (ler_arquivo_limitado(f->nyx_caminho, buf, sizeof buf) > 0)
      farolperfil_parse_webos_nyx(buf, local->aparelho.so, &local->aparelho.so_maior);
  }
  if (strcmp(local->aparelho.so, "?") == 0 && f->starfish_caminho[0]) {
    if (ler_arquivo_limitado(f->starfish_caminho, buf, sizeof buf) > 0)
      farolperfil_parse_webos_starfish(buf, local->aparelho.so, &local->aparelho.so_maior);
  }

  /* 7. Fallback generico de modelo (NUVIO_TV_MODEL, usado por TPK e WGT) */
  if (strcmp(local->aparelho.modelo, "?") == 0) {
    e = obter_env_ou_cfg(f->env_tv_model, f->usar_env_real, "NUVIO_TV_MODEL");
    if (e && e[0]) farolperfil_filtrar_modelo(e, local->aparelho.modelo);
  }

  return tv_pendente;
}

static void *trabalhador_coleta(void *arg) {
  ColetaFontes f;
  FarolPerfil local;
  int pendente;
  (void)arg;

  pthread_mutex_lock(&trava);
  f = fontes_ativas;
  pthread_mutex_unlock(&trava);

  pendente = coletar_fora_da_trava(&f, &local);

  pthread_mutex_lock(&trava);
  cache_perfil = local;
  cache_inicializado = 1;
  aplicar_overrides_travado(&cache_perfil);
#ifdef FAROL_PERFIL_TESTE
  leituras_feitas++;
#endif
  tv_pendente = pendente;
  estado_coleta = FP_ST_PRONTO;
  pthread_cond_broadcast(&pronto_cv);
  pthread_mutex_unlock(&trava);
  return NULL;
}

static void iniciar_travado(void) {
  if (estado_coleta == FP_ST_LENDO) return;
  /* PRONTO sem [tv] pendente = cache definitivo: uma leitura por sessao.
   * PRONTO com [tv] pendente = leitura cedo demais: recoleta. */
  if (estado_coleta == FP_ST_PRONTO && !tv_pendente) return;
  garantir_cache_travado();
  aplicar_overrides_travado(&cache_perfil);
#ifdef FAROL_PERFIL_TESTE
  if (teste_cfg_ativa) fontes_ativas = teste_cfg;
  else montar_fontes_padrao(&fontes_ativas);
#else
  montar_fontes_padrao(&fontes_ativas);
#endif
  estado_coleta = FP_ST_LENDO;
  /* Recoleta cria fio novo: junta o anterior antes (aqui nao e LENDO, entao
   * ele ja encerrou — o join volta na hora — e nao vaza descritor). */
  if (fio_criado) { pthread_join(fio_coleta, NULL); fio_criado = 0; }
  if (pthread_create(&fio_coleta, NULL, trabalhador_coleta, NULL) == 0) {
    fio_criado = 1;
  } else {
    /* Nao bloqueia nem retenta em loop; deixa cache nos padroes seguros. */
    estado_coleta = FP_ST_PRONTO;
    pthread_cond_broadcast(&pronto_cv);
  }
}

void farolperfil_iniciar(void) {
  pthread_mutex_lock(&trava);
  iniciar_travado();
  pthread_mutex_unlock(&trava);
}

int farolperfil_obter(FarolPerfil *out) {
  int pronto;
  pthread_mutex_lock(&trava);
  if (estado_coleta == FP_ST_ZERO) iniciar_travado();
  garantir_cache_travado();
  if (out) *out = cache_perfil;
  pronto = (estado_coleta == FP_ST_PRONTO);
  pthread_mutex_unlock(&trava);
  return pronto;
}

int farolperfil_pronto(void) {
  int pronto;
  pthread_mutex_lock(&trava);
  pronto = (estado_coleta == FP_ST_PRONTO);
  pthread_mutex_unlock(&trava);
  return pronto;
}

void farolperfil_encerrar(void) {
  pthread_t t;
  int juntar = 0;
  pthread_mutex_lock(&trava);
  if (fio_criado) {
    t = fio_coleta;
    fio_criado = 0;
    juntar = 1;
  }
  pthread_mutex_unlock(&trava);
  if (juntar) pthread_join(t, NULL);
}

/* --- 5. Receptores em tempo de execucao ----------------------------------- */

int farolperfil_definir_modelo(const char *texto) {
  char limpo[FAROL_PERFIL_MODELO_MAX + 1];
  int ok = farolperfil_filtrar_modelo(texto, limpo);
  pthread_mutex_lock(&trava);
  garantir_cache_travado();
  copiar_str(ov_modelo, sizeof ov_modelo, limpo);
  ov_tem_modelo = 1;
  copiar_str(cache_perfil.aparelho.modelo, sizeof cache_perfil.aparelho.modelo, limpo);
  pthread_mutex_unlock(&trava);
  return ok;
}

void farolperfil_definir_tela(const FarolPerfilTela *tela) {
  if (!tela) return;
  pthread_mutex_lock(&trava);
  garantir_cache_travado();
  ov_tela = *tela;
  ov_tem_tela = 1;
  cache_perfil.aparelho.tela = *tela;
  pthread_mutex_unlock(&trava);
}

void farolperfil_definir_audio(const FarolPerfilAudio *audio) {
  if (!audio) return;
  pthread_mutex_lock(&trava);
  garantir_cache_travado();
  ov_audio = *audio;
  ov_tem_audio = 1;
  cache_perfil.aparelho.audio = *audio;
  pthread_mutex_unlock(&trava);
}

void farolperfil_definir_rede(const FarolPerfilRede *rede) {
  if (!rede) return;
  pthread_mutex_lock(&trava);
  garantir_cache_travado();
  ov_rede = *rede;
  ov_tem_rede = 1;
  cache_perfil.aparelho.rede = *rede;
  pthread_mutex_unlock(&trava);
}

void farolperfil_definir_cliente(const FarolPerfilCliente *cliente) {
  if (!cliente) return;
  pthread_mutex_lock(&trava);
  garantir_cache_travado();
  ov_cliente = *cliente;
  ov_tem_cliente = 1;
  cache_perfil.cliente = *cliente;
  pthread_mutex_unlock(&trava);
}

/* --- 6. Escrita em FarolJson pelo redator --------------------------------- */

static const char *tag_so_para_plat(const char *plat) {
  if (!plat) return "dev";
  if (strcmp(plat, "webos") == 0) return "webos";
  if (strcmp(plat, "tizen_wgt") == 0) return "tizen";
  if (strcmp(plat, "tizen_tpk") == 0) return "tizen";
  if (strcmp(plat, "android") == 0) return "android";
  return "dev";
}

static const char *tag_ver_para_build(const char *build) {
  if (!build) return "dev";
  if (strcmp(build, "webos-ipk") == 0) return "ipk";
  if (strcmp(build, "tizen-wgt") == 0) return "wgt";
  if (strcmp(build, "tizen-tpk40") == 0) return "tpk40";
  if (strcmp(build, "tizen-tpk60") == 0) return "tpk60";
  if (strcmp(build, "tizen-tpk65") == 0) return "tpk65";
  if (strcmp(build, "tizen-tpk") == 0) return "tpk";
  if (strcmp(build, "android-apk") == 0) return "apk";
  return "debug";
}

static int formatar_versao_json(const char *ver, const char *tag, char out[24]) {
  char limpa[24];
  if (!ver || !*ver || strcmp(ver, "?") == 0) return 0;
  if (!parse_versao(ver, strlen(ver), limpa, NULL, 0)) return 0;
  if (strchr(limpa, '-')) {
    copiar_str(out, 24, limpa);
    return 1;
  }
  if (!tag || !na_lista(tag, VER_TAGS_VALIDAS)) tag = "dev";
  if (snprintf(out, 24, "%s-%s", limpa, tag) >= 24) return 0;
  return 1;
}

int farolperfil_escrever_aparelho(FarolJson *j, const FarolPerfilAparelho *a) {
  const char *plat, *mod_json;
  char so_json[24], mod_limpo[FAROL_PERFIL_MODELO_MAX + 1];

  if (!j || !a) return 0;
  if (!farolredator_objeto(j, "aparelho")) return 0;

  plat = na_lista(a->plataforma, PLATS_VALIDAS) ? a->plataforma : "outra";
  if (!farolredator_campo(j, "plataforma", FT_ENUM, plat)) goto falha;

  if (formatar_versao_json(a->so, tag_so_para_plat(plat), so_json)) {
    if (!farolredator_campo(j, "so", FT_VERSAO, so_json)) goto falha;
  }
  if (!farolredator_campo(j, "so_maior", FT_INT, &a->so_maior)) goto falha;

  /* No FarolJson, aparelho.modelo segue a gramatica fechada do redator (ou
   * "x") — a mesma do modeloOk do servidor publicado: emitir fora dela faz o
   * registro inteiro voltar 422. O filtro do coletor aceita espaco —
   * "TCL 55C745", o "<MANUFACTURER> <MODEL>" do NuvioActivity.kt — e a
   * gramatica nao; os espacos saem antes da conferencia e a forma canonica
   * sem espaco e a emitida. Valor fora da gramatica (LG com digito a
   * frente, fabricante fora da lista) ou da lista de privacidade vira "x". */
  if (farolperfil_filtrar_modelo(a->modelo, mod_limpo)) {
    size_t rr, ww = 0;
    for (rr = 0; mod_limpo[rr]; rr++)
      if (mod_limpo[rr] != ' ') mod_limpo[ww++] = mod_limpo[rr];
    mod_limpo[ww] = '\0';
    mod_json = modelo_redator_ok(mod_limpo) ? mod_limpo : "x";
  } else {
    mod_json = "x";
  }
  if (!farolredator_campo(j, "modelo", FT_TOKEN, mod_json)) goto falha;

  if (!farolredator_campo(j, "ram_total_mb", FT_INT, &a->ram_total_mb)) goto falha;
  if (!farolredator_campo(j, "ram_livre_mb", FT_INT, &a->ram_livre_mb)) goto falha;

  {
    /* "tela" sem literal unico: o varredor i18n acusa a palavra como texto de
     * tela; aqui e so a chave do objeto aparelho.tela do schema (§2.6). */
    static const char k_tela[] = { 't', 'e', 'l', 'a', '\0' };
    if (!farolredator_objeto(j, k_tela)) goto falha;
    if (!farolredator_campo(j, "w", FT_INT, &a->tela.w) ||
        !farolredator_campo(j, "h", FT_INT, &a->tela.h) ||
        !farolredator_campo(j, "hdr", FT_INT, &a->tela.hdr) ||
        !farolredator_campo(j, "dv", FT_INT, &a->tela.dv) ||
        !farolredator_campo(j, "hevc", FT_INT, &a->tela.hevc) ||
        !farolredator_campo(j, "av1", FT_INT, &a->tela.av1)) {
      farolredator_fechar(j);
      goto falha;
    }
    if (!farolredator_fechar(j)) goto falha;
  }

  if (!farolredator_objeto(j, "audio")) goto falha;
  if (!farolredator_campo(j, "dts", FT_INT, &a->audio.dts) ||
      !farolredator_campo(j, "truehd", FT_INT, &a->audio.truehd) ||
      !farolredator_campo(j, "eac3", FT_INT, &a->audio.eac3) ||
      !farolredator_campo(j, "ac3", FT_INT, &a->audio.ac3)) {
    farolredator_fechar(j);
    goto falha;
  }
  if (!farolredator_fechar(j)) goto falha;

  if (!farolredator_objeto(j, "rede")) goto falha;
  if (a->rede.offline == 0 || a->rede.offline == 1) {
    if (!farolredator_campo(j, "offline", FT_BOOL, &a->rede.offline)) {
      farolredator_fechar(j);
      goto falha;
    }
  }
  if (!farolredator_campo(j, "hosts_com_falha", FT_INT, &a->rede.hosts_com_falha)) {
    farolredator_fechar(j);
    goto falha;
  }
  if (isfinite(a->rede.vazao_mbps) && a->rede.vazao_mbps >= 0.0 && a->rede.vazao_mbps <= 1000000000.0) {
    if (!farolredator_campo(j, "vazao_mbps", FT_NUM1, &a->rede.vazao_mbps)) {
      farolredator_fechar(j);
      goto falha;
    }
  }
  if (!farolredator_fechar(j)) goto falha;

  return farolredator_fechar(j);

falha:
  farolredator_fechar(j);
  return 0;
}

int farolperfil_escrever_cliente(FarolJson *j, const FarolPerfilCliente *c) {
  static const struct { unsigned bit; const char *nome; } tipos_tab[] = {
    { FAROL_ADDON_CATALOGO, "catalogo" },
    { FAROL_ADDON_FONTES,   "fontes"   },
    { FAROL_ADDON_LEGENDAS, "legendas" },
    { FAROL_ADDON_META,     "meta"     }
  };
  static const struct { unsigned bit; const char *nome; } debrid_tab[] = {
    { FAROL_DEBRID_TORBOX,     "torbox"     },
    { FAROL_DEBRID_REALDEBRID, "realdebrid" },
    { FAROL_DEBRID_ALLDEBRID,  "alldebrid"  },
    { FAROL_DEBRID_PREMIUMIZE, "premiumize" },
    { FAROL_DEBRID_DEBRIDLINK, "debridlink" },
    { FAROL_DEBRID_EASYDEBRID, "easydebrid" }
  };
  static const struct { unsigned bit; const char *nome; } contas_tab[] = {
    { FAROL_CONTA_TRAKT, "trakt" },
    { FAROL_CONTA_SIMKL, "simkl" }
  };
  const char *build, *backend, *gpu;
  char ver_json[24];
  size_t i;

  if (!j || !c) return 0;
  if (!farolredator_objeto(j, "cliente")) return 0;

  build = na_lista(c->build, BUILDS_VALIDOS) ? c->build : "debug";
  backend = na_lista(c->backend, BACKENDS_VALIDOS) ? c->backend : "outro";
  gpu = na_lista(c->gpu, GPUS_VALIDAS) ? c->gpu : "desconhecido";

  if (formatar_versao_json(c->versao, tag_ver_para_build(build), ver_json)) {
    if (!farolredator_campo(j, "versao", FT_VERSAO, ver_json)) goto falha;
  }
  if (commit_valido(c->commit)) {
    if (!farolredator_campo(j, "commit", FT_HASH, c->commit)) goto falha;
  }
  if (!farolredator_campo(j, "build", FT_ENUM, build) ||
      !farolredator_campo(j, "backend", FT_ENUM, backend))
    goto falha;
  if (na_lista(c->idioma, IDIOMAS_VALIDOS)) {
    if (!farolredator_campo(j, "idioma", FT_ENUM, c->idioma)) goto falha;
  }
  if (!farolredator_campo(j, "aberto_s", FT_INT, &c->aberto_s) ||
      !farolredator_campo(j, "gpu", FT_ENUM, gpu))
    goto falha;

  if (!farolredator_objeto(j, "ajustes")) goto falha;
  if (!farolredator_campo(j, "glass_ui", FT_INT, &c->ajustes.glass_ui)) {
    farolredator_fechar(j);
    goto falha;
  }
  if (na_lista(c->ajustes.qualidade_max, QUALIDADES_VALIDAS)) {
    if (!farolredator_campo(j, "qualidade_max", FT_ENUM, c->ajustes.qualidade_max)) {
      farolredator_fechar(j);
      goto falha;
    }
  }
  if (isfinite(c->ajustes.tamanho_max_gb) && c->ajustes.tamanho_max_gb >= 0.0 &&
      c->ajustes.tamanho_max_gb <= 1000000000.0) {
    if (!farolredator_campo(j, "tamanho_max_gb", FT_NUM1, &c->ajustes.tamanho_max_gb)) {
      farolredator_fechar(j);
      goto falha;
    }
  }
  if (na_lista(c->ajustes.dts, DTS_VALIDOS)) {
    if (!farolredator_campo(j, "dts", FT_ENUM, c->ajustes.dts)) {
      farolredator_fechar(j);
      goto falha;
    }
  }
  if (!farolredator_campo(j, "dv_mkv", FT_INT, &c->ajustes.dv_mkv) ||
      !farolredator_campo(j, "legenda_auto_sync", FT_INT, &c->ajustes.legenda_auto_sync)) {
    farolredator_fechar(j);
    goto falha;
  }
  if (!farolredator_fechar(j)) goto falha;

  if (!farolredator_objeto(j, "addons")) goto falha;
  if (!farolredator_campo(j, "total", FT_INT, &c->addons.total) ||
      !farolredator_campo(j, "ativos", FT_INT, &c->addons.ativos) ||
      !farolredator_campo(j, "isolados", FT_INT, &c->addons.isolados) ||
      !farolredator_array(j, "tipos")) {
    farolredator_fechar(j);
    goto falha;
  }
  for (i = 0; i < sizeof tipos_tab / sizeof *tipos_tab; i++) {
    if ((c->addons.tipos_mask & tipos_tab[i].bit) &&
        !farolredator_campo(j, NULL, FT_ENUM, tipos_tab[i].nome)) {
      farolredator_fechar(j);
      farolredator_fechar(j);
      goto falha;
    }
  }
  if (!farolredator_fechar(j) || !farolredator_fechar(j)) goto falha;

  if (!farolredator_array(j, "debrid")) goto falha;
  for (i = 0; i < sizeof debrid_tab / sizeof *debrid_tab; i++) {
    if ((c->debrid_mask & debrid_tab[i].bit) &&
        !farolredator_campo(j, NULL, FT_ENUM, debrid_tab[i].nome)) {
      farolredator_fechar(j);
      goto falha;
    }
  }
  if (!farolredator_fechar(j)) goto falha;

  if (!farolredator_array(j, "contas")) goto falha;
  for (i = 0; i < sizeof contas_tab / sizeof *contas_tab; i++) {
    if ((c->contas_mask & contas_tab[i].bit) &&
        !farolredator_campo(j, NULL, FT_ENUM, contas_tab[i].nome)) {
      farolredator_fechar(j);
      goto falha;
    }
  }
  if (!farolredator_fechar(j)) goto falha;

  return farolredator_fechar(j);

falha:
  farolredator_fechar(j);
  return 0;
}

int farolperfil_escrever(FarolJson *j, const FarolPerfil *p) {
  if (!j || !p) return 0;
  return farolperfil_escrever_aparelho(j, &p->aparelho) &&
         farolperfil_escrever_cliente(j, &p->cliente);
}

/* --- 7. Ganchos deterministas de teste ------------------------------------ */

#ifdef FAROL_PERFIL_TESTE
void farolperfil_teste_reiniciar(void) {
  farolperfil_encerrar();
  pthread_mutex_lock(&trava);
  farolperfil_padrao(&cache_perfil);
  cache_inicializado = 0;
  estado_coleta = FP_ST_ZERO;
  leituras_feitas = 0;
  tv_pendente = 0;
  ov_modelo[0] = '\0';
  ov_tem_modelo = 0;
  ov_tem_tela = 0;
  ov_tem_audio = 0;
  ov_tem_rede = 0;
  ov_tem_cliente = 0;
  teste_cfg_ativa = 0;
  memset(&teste_cfg, 0, sizeof teste_cfg);
  pthread_mutex_unlock(&trava);
}

void farolperfil_teste_configurar(const FarolPerfilTesteFontes *cfg) {
  pthread_mutex_lock(&trava);
  memset(&teste_cfg, 0, sizeof teste_cfg);
  if (cfg) {
    teste_cfg_ativa = 1;
    copiar_str(teste_cfg.plataforma, sizeof teste_cfg.plataforma,
               cfg->plataforma ? cfg->plataforma : plat_padrao());
    copiar_str(teste_cfg.meminfo_caminho, sizeof teste_cfg.meminfo_caminho, cfg->meminfo_caminho);
    copiar_str(teste_cfg.tpk_log_caminho, sizeof teste_cfg.tpk_log_caminho, cfg->tpk_log_caminho);
    copiar_str(teste_cfg.nyx_caminho, sizeof teste_cfg.nyx_caminho, cfg->nyx_caminho);
    copiar_str(teste_cfg.starfish_caminho, sizeof teste_cfg.starfish_caminho, cfg->starfish_caminho);
    copiar_str(teste_cfg.env_android_info, sizeof teste_cfg.env_android_info, cfg->env_android_info);
    copiar_str(teste_cfg.env_wgt_info, sizeof teste_cfg.env_wgt_info, cfg->env_wgt_info);
    copiar_str(teste_cfg.env_tv_model, sizeof teste_cfg.env_tv_model, cfg->env_tv_model);
    copiar_str(teste_cfg.env_tizen_ver, sizeof teste_cfg.env_tizen_ver, cfg->env_tizen_ver);
    copiar_str(teste_cfg.env_panel_size, sizeof teste_cfg.env_panel_size, cfg->env_panel_size);
    teste_cfg.usar_env_real = cfg->usar_env_real;
    teste_cfg.antes_de_ler = cfg->antes_de_ler;
  } else {
    teste_cfg_ativa = 0;
  }
  pthread_mutex_unlock(&trava);
}

int farolperfil_teste_esperar(int timeout_ms) {
  struct timespec ts;
  int ok;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += timeout_ms / 1000;
  ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
  if (ts.tv_nsec >= 1000000000L) {
    ts.tv_sec++;
    ts.tv_nsec -= 1000000000L;
  }
  pthread_mutex_lock(&trava);
  while (estado_coleta == FP_ST_LENDO) {
    if (pthread_cond_timedwait(&pronto_cv, &trava, &ts) == ETIMEDOUT) break;
  }
  ok = (estado_coleta == FP_ST_PRONTO);
  pthread_mutex_unlock(&trava);
  return ok;
}

int farolperfil_teste_leituras(void) {
  int n;
  pthread_mutex_lock(&trava);
  n = leituras_feitas;
  pthread_mutex_unlock(&trava);
  return n;
}
#endif
