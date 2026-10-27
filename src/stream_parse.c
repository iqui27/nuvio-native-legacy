#include "streams.h"
#include "mediaurl.h"
#include "badges.h"
#include "js.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <math.h>

// Addons tambem mandam numeros como strings. Evita conversao indefinida de
// infinito ou expoentes enormes a int/long, preservando tamanho desconhecido.
static long tamanhoMB(double valor) {
  return isfinite(valor) && valor > 0 && valor < (double)LONG_MAX ? (long)valor : 0;
}

// Find only a direct property of this exact object. JSON strings and nested
// objects are skipped; proxyHeaders/request cannot masquerade as videoSize.
static const char *propriedadeRaiz(const char *p, const char *fim, const char *nome, const char **chave) {
  int depth = 0;
  size_t n = strlen(nome);
  if (!p || p >= fim || *p != '{') return NULL;
  while (p < fim) {
    if (*p == '"') {
      const char *ini = ++p;
      while (p < fim && *p != '"') {
        if (*p == '\\' && p + 1 < fim) p++;
        p++;
      }
      if (p >= fim) return NULL;
      const char *apos = p + 1;
      while (apos < fim && isspace((unsigned char)*apos)) apos++;
      if (depth == 1 && (size_t)(p - ini) == n && !memcmp(ini, nome, n) && apos < fim && *apos == ':') {
        apos++; while (apos < fim && isspace((unsigned char)*apos)) apos++;
        if (chave) *chave = ini - 1;
        return apos < fim ? apos : NULL;
      }
    } else if (*p == '{' || *p == '[') depth++;
    else if (*p == '}' || *p == ']') { if (--depth == 0) break; }
    p++;
  }
  return NULL;
}
static const char *valorRaiz(const char *p, const char *fim, const char *nome) {
  return propriedadeRaiz(p, fim, nome, NULL);
}

// strstr LIMITADO a [ini,fim). O strstr comum varre ate o NUL do documento e a
// guarda `q < fim` so descarta o achado DEPOIS da varredura: cada chave ausente
// num stream (sources, proxyHeaders...) era uma leitura do resto inteiro da
// resposta — quadratico no numero de fontes e o motivo da lista travar em TVs
// fracas com respostas de milhares de streams.
static const char *buscaEm(const char *ini, const char *fim, const char *termo) {
  size_t n = strlen(termo);
  if (!ini || !fim || fim < ini) return NULL;
  while ((size_t)(fim - ini) >= n) {
    const char *p = memchr(ini, termo[0], (size_t)(fim - ini) - n + 1);
    if (!p) return NULL;
    if (!memcmp(p, termo, n)) return p;
    ini = p + 1;
  }
  return NULL;
}

static const char *objetoRaiz(const char *p, const char *fim, const char *nome) {
  const char *v = valorRaiz(p, fim, nome), *f;
  if (!v || *v != '{' || !(f = js_fim(v)) || f <= v || f > fim || f[-1] != '}') return NULL;
  return v;
}

// A chave ja foi localizada na profundidade correta. Limita a decodificacao
// ao proprio valor, sem procurar nomes iguais em proxyHeaders ou outra fonte.
static int textoRaiz(const char *p, const char *fim, const char *nome, char *dst, unsigned tam) {
  const char *v = valorRaiz(p, fim, nome), *f;
  if (!v || *v != '"' || !tam) return 0;
  for (f = v + 1; f < fim && *f != '"'; f++) if (*f == '\\' && f + 1 < fim) f++;
  if (f >= fim || (size_t)(f - v - 1) >= tam) return 0;
  return js_cadeia(v, dst, tam);
}
static int textoRaizLinhas(const char *p, const char *fim, const char *nome, char *dst, unsigned tam) {
  const char *chave = NULL, *v = propriedadeRaiz(p, fim, nome, &chave), *f;
  if (!v || *v != '"' || !tam) return 0;
  for (f = v + 1; f < fim && *f != '"'; f++) if (*f == '\\' && f + 1 < fim) f++;
  if (f >= fim) return 0;
  return js_texto_linhas(chave, f + 1, nome, dst, tam);
}

// Valores estruturados podem ser texto, numero (resolucao/canais) ou lista de
// textos. Objetos e booleanos nao viram etiquetas; listas aninhadas nao contam.
static int campoFiltro(const char *v, const char *fim, char *dst, unsigned tam) {
  const char *f;
  if (!v || v >= fim || !tam) return 0;
  dst[0] = 0;
  if (*v == '"') {
    for (f = v + 1; f < fim && *f != '"'; f++) if (*f == '\\' && f + 1 < fim) f++;
    return f < fim && (size_t)(f - v - 1) < tam && js_cadeia(v, dst, tam);
  }
  if (*v == '[') {
    const char *af = js_fim(v), *q = v + 1;
    unsigned usada = 0;
    if (!af || af > fim || af <= q || af[-1] != ']') return 0;
    while (q < af - 1) {
      char item[128];
      while (q < af - 1 && (isspace((unsigned char)*q) || *q == ',')) q++;
      if (q >= af - 1) break;
      if (*q != '"') { dst[0] = 0; return 0; }
      for (f = q + 1; f < af - 1 && *f != '"'; f++) if (*f == '\\' && f + 1 < af - 1) f++;
      if (f >= af - 1 || (size_t)(f - q - 1) >= sizeof item || !js_cadeia(q, item, sizeof item)) {
        dst[0] = 0; return 0;
      }
      size_t n = strlen(item);
      if ((size_t)usada + n + (usada ? 1u : 0u) >= tam) { dst[0] = 0; return 0; }
      if (usada) dst[usada++] = '|';
      memcpy(dst + usada, item, n + 1); usada += (unsigned)n;
      q = f + 1;
      while (q < af - 1 && isspace((unsigned char)*q)) q++;
      if (q < af - 1 && *q != ',') { dst[0] = 0; return 0; }
    }
    return usada > 0;
  }
  if (!isdigit((unsigned char)*v)) return 0;
  for (f = v; f < fim && (isdigit((unsigned char)*f) || *f == '.'); f++);
  if (f == v || (size_t)(f - v) >= tam ||
      (f < fim && *f != ',' && *f != '}' && !isspace((unsigned char)*f))) return 0;
  memcpy(dst, v, (size_t)(f - v)); dst[f - v] = 0;
  char *e;
  double d = strtod(dst, &e);
  if (*e || !isfinite(d) || d <= 0) { dst[0] = 0; return 0; }
  return 1;
}

static void camposFiltro(const char *const *objetos, int n, const char *fim,
                          const char *const *nomes, char *dst, unsigned tam) {
  dst[0] = 0;
  for (int i = 0; i < n; i++) if (objetos[i])
    for (int j = 0; nomes[j]; j++)
      if (campoFiltro(valorRaiz(objetos[i], js_fim(objetos[i]), nomes[j]), fim, dst, tam)) return;
}

// Inteiro decimal sem arredondamento de double. Ao contrario do StreamFit,
// este tamanho tambem pode vir do raw.size declarado pelo AIOStreams.
static uint64_t bytesFiltro(const char *v, const char *fim) {
  uint64_t n = 0;
  int quoted;
  if (!v || v >= fim) return 0;
  quoted = *v == '"'; if (quoted) v++;
  if (v >= fim || !isdigit((unsigned char)*v)) return 0;
  if (!quoted && *v == '0' && v + 1 < fim && isdigit((unsigned char)v[1])) return 0;
  while (v < fim && isdigit((unsigned char)*v)) {
    unsigned d = (unsigned)(*v++ - '0');
    if (n > (UINT64_MAX - d) / 10) return 0;
    n = n * 10 + d;
  }
  if (quoted && (v >= fim || *v++ != '"')) return 0;
  while (v < fim && isspace((unsigned char)*v)) v++;
  return v < fim && (*v == ',' || *v == '}') ? n : 0;
}

static uint64_t bytesTextoFiltro(const char *texto) {
  static const struct { const char *unidade; uint64_t escala; } unidades[] = {
    {"GiB", UINT64_C(1073741824)}, {"GB", UINT64_C(1000000000)},
    {"MiB", UINT64_C(1048576)}, {"MB", UINT64_C(1000000)},
    {"TiB", UINT64_C(1099511627776)}, {"TB", UINT64_C(1000000000000)},
    {"KiB", UINT64_C(1024)}, {"KB", UINT64_C(1000)}
  };
  for (const char *p = texto; p && *p; p++) {
    if (!isdigit((unsigned char)*p) || (p > texto &&
        (isalnum((unsigned char)p[-1]) || p[-1] == '.' || p[-1] == '-'))) continue;
    char *fim;
    long double valor = strtold(p, &fim);
    if (fim == p || !isfinite(valor) || valor <= 0) continue;
    while (isspace((unsigned char)*fim)) fim++;
    for (size_t i = 0; i < sizeof unidades / sizeof unidades[0]; i++) {
      size_t n = strlen(unidades[i].unidade);
      if (strncasecmp(fim, unidades[i].unidade, n) || isalnum((unsigned char)fim[n])) continue;
      long double bytes = valor * (long double)unidades[i].escala;
      // A comparacao estrita evita conversao indefinida na representacao
      // arredondada de UINT64_MAX em plataformas cujo long double e double.
      if (!isfinite(bytes) || bytes <= 0 || bytes >= (long double)UINT64_MAX) return 0;
      uint64_t inteiro = (uint64_t)bytes;
      return inteiro + ((long double)inteiro < bytes);
    }
  }
  return 0;
}

static void metadadosFiltro(const char *obj, const char *fim, const char *nome,
                             const char *descricao, const char *titulo, Stream *s) {
  const char *cr = objetoRaiz(obj, fim, "clientResolve");
  const char *st = cr ? objetoRaiz(cr, js_fim(cr), "stream") : NULL;
  const char *raw = st ? objetoRaiz(st, js_fim(st), "raw") : NULL;
  const char *parsed = raw ? objetoRaiz(raw, js_fim(raw), "parsed") : NULL;
  if (!parsed && st) parsed = objetoRaiz(st, js_fim(st), "parsed");
  if (!parsed && cr) parsed = objetoRaiz(cr, js_fim(cr), "parsed");
  if (!raw) raw = objetoRaiz(obj, fim, "raw");
  if (!parsed && raw) parsed = objetoRaiz(raw, js_fim(raw), "parsed");
  if (!parsed) parsed = objetoRaiz(obj, fim, "parsed");
  const char *data = objetoRaiz(obj, fim, "streamData");
  const char *parsedFile = data ? objetoRaiz(data, js_fim(data), "parsedFile") : NULL;
  const char *bh = objetoRaiz(obj, fim, "behaviorHints");
  const char *objetos[] = {parsedFile, parsed, raw, st, cr, obj};
  static const char *const chaves[FONTEFILTRO_N][4] = {
    {"resolution", "videoResolution", NULL},
    {"quality", NULL},
    {"hdr", "visualTags", "videoTags", NULL},
    {"audio", "audioTags", "audioCodec", NULL},
    {"channels", "audioChannels", NULL},
    {"codec", "videoCodec", "encode", NULL},
    {"languages", "language", "audioLanguages", NULL}
  };
  char campos[FONTEFILTRO_N][512], grupo[128] = "", nomeCompleto[1024] = "", texto[6144];
  for (int k = 0; k < FONTEFILTRO_N; k++)
    camposFiltro(objetos, 6, fim, chaves[k], campos[k], sizeof campos[k]);
  // O leitor de faixas ja validou audioTracks/languages. Legendas nao sao
  // evidencia de audio, e a preferencia de idioma nao declara uma faixa.
  if (!campos[FONTEFILTRO_LINGUAS][0] && s->audioIdiomas[0])
    snprintf(campos[FONTEFILTRO_LINGUAS], sizeof campos[FONTEFILTRO_LINGUAS], "%s", s->audioIdiomas);
  static const char *const grupoChaves[] = {"group", "releaseGroup", "release_group", NULL};
  camposFiltro(objetos, 6, fim, grupoChaves, grupo, sizeof grupo);
  if (!textoRaiz(obj, fim, "filename", nomeCompleto, sizeof nomeCompleto) &&
      !(bh && textoRaiz(bh, js_fim(bh), "filename", nomeCompleto, sizeof nomeCompleto)) &&
      !(cr && textoRaiz(cr, js_fim(cr), "filename", nomeCompleto, sizeof nomeCompleto)) &&
      !(raw && textoRaiz(raw, js_fim(raw), "filename", nomeCompleto, sizeof nomeCompleto)) && raw)
    textoRaiz(raw, js_fim(raw), "torrentName", nomeCompleto, sizeof nomeCompleto);
  snprintf(s->arquivo, sizeof s->arquivo, "%.*s", (int)sizeof s->arquivo - 1, nomeCompleto);
  // Mantem title e description mesmo quando ambos existem; nao usa URL,
  // cabecalhos ou nome do provedor como evidencia de formato/idioma/grupo.
  snprintf(texto, sizeof texto, "%s\n%s\n%s", nome, descricao, titulo);
  uint64_t bytes = s->tamanhoBytes;
  if (!bytes && bh) bytes = bytesFiltro(valorRaiz(bh, js_fim(bh), "videoSize"), js_fim(bh));
  if (!bytes && raw) bytes = bytesFiltro(valorRaiz(raw, js_fim(raw), "size"), js_fim(raw));
  if (!bytes) bytes = bytesFiltro(valorRaiz(obj, fim, "videoSize"), fim);
  if (!bytes) bytes = bytesTextoFiltro(texto);
  FonteFiltroEntrada e = {
    .resolucao = campos[FONTEFILTRO_RESOLUCAO], .qualidade = campos[FONTEFILTRO_QUALIDADE],
    .visual = campos[FONTEFILTRO_VISUAL], .audio = campos[FONTEFILTRO_AUDIO],
    .canais = campos[FONTEFILTRO_CANAIS], .encode = campos[FONTEFILTRO_ENCODE],
    .linguas = campos[FONTEFILTRO_LINGUAS], .grupo = grupo,
    .filename = nomeCompleto, .texto = texto, .tamanhoBytes = bytes
  };
  fontefiltro_classificar(&e, &s->filtro);
  s->filtroLido = 1;
}


// Idiomas de faixas sao metadados positivos do addon.
// Valida cada valor antes de usa-lo: JSON cortado, objetos muito
// fundos e chaves em cabecalhos nunca viram disponibilidade de audio/legenda.
static const char *faixasEspacos(const char *p, const char *fim) {
  while (p < fim && isspace((unsigned char)*p)) p++;
  return p;
}
static unsigned faixasHex4(const char *p) {
  unsigned n = 0;
  for (int k = 0; k < 4; k++) {
    unsigned char c = (unsigned char)p[k];
    if (!isxdigit(c)) return UINT_MAX;
    n = n * 16 + (c <= '9' ? (unsigned)(c - '0') : (unsigned)(tolower(c) - 'a' + 10));
  }
  return n;
}
static const char *faixasFimTexto(const char *p, const char *fim) {
  if (p >= fim || *p++ != '"') return NULL;
  while (p < fim) {
    unsigned char c = (unsigned char)*p++;
    if (c == '"') return p;
    if (c < 0x20) return NULL;
    if (c != '\\') continue;
    if (p >= fim) return NULL;
    c = (unsigned char)*p++;
    if (strchr("\"\\/bfnrt", c)) continue;
    if (c != 'u' || fim - p < 4) return NULL;
    unsigned cp = faixasHex4(p);
    if (cp == UINT_MAX) return NULL;
    p += 4;
    if (cp >= 0xDC00 && cp <= 0xDFFF) return NULL;
    if (cp >= 0xD800 && cp <= 0xDBFF) {
      if (fim - p < 6 || p[0] != '\\' || p[1] != 'u') return NULL;
      unsigned lo = faixasHex4(p + 2);
      if (lo < 0xDC00 || lo > 0xDFFF) return NULL;
      p += 6;
    }
  }
  return NULL;
}
static const char *faixasFimValor(const char *p, const char *fim, unsigned prof) {
  if (p >= fim || prof > 16) return NULL;
  if (*p == '"') return faixasFimTexto(p, fim);
  if (*p == '{' || *p == '[') {
    int objeto = *p == '{';
    char fecha = objeto ? '}' : ']';
    p = faixasEspacos(p + 1, fim);
    if (p < fim && *p == fecha) return p + 1;
    for (;;) {
      if (objeto) {
        p = faixasFimTexto(p, fim);
        if (!p) return NULL;
        p = faixasEspacos(p, fim);
        if (p >= fim || *p != ':') return NULL;
        p = faixasEspacos(p + 1, fim);
      }
      p = faixasFimValor(p, fim, prof + 1);
      if (!p) return NULL;
      p = faixasEspacos(p, fim);
      if (p >= fim) return NULL;
      if (*p == fecha) return p + 1;
      if (*p != ',') return NULL;
      p = faixasEspacos(p + 1, fim);
    }
  }
  static const char *const literais[] = {"null", "true", "false"};
  for (unsigned k = 0; k < sizeof literais / sizeof *literais; k++) {
    size_t n = strlen(literais[k]);
    if ((size_t)(fim - p) >= n && !memcmp(p, literais[k], n)) return p + n;
  }
  // Numeros sao ignorados como idiomas, mas precisam de sintaxe JSON valida.
  if (*p == '-') p++;
  if (p >= fim || !isdigit((unsigned char)*p)) return NULL;
  if (*p == '0') p++;
  else while (p < fim && isdigit((unsigned char)*p)) p++;
  if (p < fim && *p == '.') {
    p++;
    if (p >= fim || !isdigit((unsigned char)*p)) return NULL;
    while (p < fim && isdigit((unsigned char)*p)) p++;
  }
  if (p < fim && (*p == 'e' || *p == 'E')) {
    p++;
    if (p < fim && (*p == '+' || *p == '-')) p++;
    if (p >= fim || !isdigit((unsigned char)*p)) return NULL;
    while (p < fim && isdigit((unsigned char)*p)) p++;
  }
  return p;
}
static int faixasUtf8(const unsigned char *p) {
  while (*p) {
    unsigned c = *p++, cp, n;
    if (c < 0x80) continue;
    if (c >= 0xC2 && c <= 0xDF) { cp = c & 31; n = 1; }
    else if (c >= 0xE0 && c <= 0xEF) { cp = c & 15; n = 2; }
    else if (c >= 0xF0 && c <= 0xF4) { cp = c & 7; n = 3; }
    else return 0;
    unsigned minimo = n == 1 ? 0x80 : n == 2 ? 0x800 : 0x10000;
    for (unsigned k = 0; k < n; k++) {
      if ((*p & 0xC0) != 0x80) return 0;
      cp = (cp << 6) | (*p++ & 63);
    }
    if (cp < minimo || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF) ||
        (cp >= 0x80 && cp <= 0x9F) || cp == 0x2028 || cp == 0x2029) return 0;
  }
  return 1;
}
static int faixasTexto(const char *p, const char *fim, char *dst, unsigned tam) {
  const char *f = faixasFimTexto(p, fim);
  char json[1024];
  if (!f || !tam || (size_t)(f - p) >= sizeof json || (size_t)(f - p - 2) >= tam) return 0;
  // O decodificador geral converte alguns controles em espacos. Aqui nao
  // aceitar "en\\u0000..." nem valores com quebras que parecam cabecalhos.
  for (const char *q = p + 1; q < f - 1; q++) if (*q == '\\') {
    q++;
    if (strchr("bfnrt", *q)) return 0;
    if (*q == 'u') {
      unsigned cp = faixasHex4(q + 1);
      if (cp < 0x20 || cp == 0x7F) return 0;
      q += 4;
    }
  }
  memcpy(json, p, (size_t)(f - p)); json[f - p] = 0;
  return js_cadeia(json, dst, tam) && faixasUtf8((const unsigned char *)dst);
}
static void faixaIdiomaAdicionar(char *dst, unsigned tam, const char *ini, const char *fim) {
  char idioma[128];
  while (ini < fim && isspace((unsigned char)*ini)) ini++;
  while (fim > ini && isspace((unsigned char)fim[-1])) fim--;
  size_t n = (size_t)(fim - ini);
  if (!n || n >= sizeof idioma) return;
  int letra = 0;
  for (const char *p = ini; p < fim; p++) {
    unsigned char c = (unsigned char)*p;
    if (c >= 0x80 || isalpha(c)) letra = 1;
    else if (!isdigit(c) && c != ' ' && c != '-' && c != '_' && c != '(' && c != ')' && c != '\'' && c != '.') return;
  }
  if (!letra) return;
  memcpy(idioma, ini, n); idioma[n] = 0;
  static const char *const vazios[] = {
    "und", "unknown", "null", "none", "undefined", "unavailable", "unreported",
    "not reported", "not specified", "n.a.", "zxx", "mul", "multi", "multilingual",
    "multiple", "multiple languages", "auto", "original"
  };
  for (unsigned k = 0; k < sizeof vazios / sizeof *vazios; k++)
    if (!strcasecmp(idioma, vazios[k])) return;
  for (const char *p = dst; *p;) {
    const char *e = strchr(p, ',');
    if (!e) e = p + strlen(p);
    if ((size_t)(e - p) == n && !strncasecmp(p, idioma, n)) return;
    p = *e ? e + 1 : e;
  }
  size_t usado = strlen(dst);
  if (usado + n + (usado ? 1 : 0) >= tam) return;
  if (usado) dst[usado++] = ',';
  memcpy(dst + usado, idioma, n + 1);
}
static void faixasValorLer(const char *v, const char *fim, char *dst, unsigned tam, unsigned prof, int faixa) {
  if (!v || v >= fim || prof > 3) return;
  if (*v == '"') {
    char texto[1024];
    if (!faixasTexto(v, fim, texto, sizeof texto)) return;
    const char *p = texto;
    for (const char *q = p;; q++) if (!*q || *q == ',' || *q == '|' || *q == ';') {
      faixaIdiomaAdicionar(dst, tam, p, q);
      if (!*q) break;
      p = q + 1;
    }
  } else if (*v == '[') {
    const char *q = faixasEspacos(v + 1, fim);
    while (q < fim && *q != ']') {
      const char *f = faixasFimValor(q, fim, 0);
      if (!f) return;
      // Lista aninhada nao tem significado de faixa: nao a achatar.
      if (*q == '"' || *q == '{') faixasValorLer(q, f, dst, tam, prof + 1, faixa);
      q = faixasEspacos(f, fim);
      if (q < fim && *q == ',') q = faixasEspacos(q + 1, fim);
      else break;
    }
  } else if (*v == '{') {
    // language/lang pertencem a faixa; code/name sao aceitos apenas dentro
    // de um objeto declarado como idioma. O nome/codec de uma faixa nao e
    // seu idioma: {name:"Stereo",code:"AAC"} nao informa idioma nenhum.
    static const char *const chaves[] = {"language", "lang", "languageCode", "language_code", "code", "iso639_1", "iso639_2", "name"};
    unsigned n = faixa ? 4 : sizeof chaves / sizeof *chaves;
    for (unsigned k = 0; k < n; k++) {
      const char *p = valorRaiz(v, fim, chaves[k]);
      if (p && (*p == '"' || *p == '{')) {
        faixasValorLer(p, fim, dst, tam, prof + 1, 0);
        return;
      }
    }
  }
}
static void faixasCampo(const char *obj, const char *fim, const char *chave, char *dst, unsigned tam, int faixa) {
  const char *v = valorRaiz(obj, fim, chave), *f;
  if (!v || !(f = faixasFimValor(v, fim, 0))) return;
  const char *apos = faixasEspacos(f, fim);
  if (apos >= fim || (*apos != ',' && *apos != '}')) return;
  faixasValorLer(v, f, dst, tam, 0, faixa);
}

// Um campo de texto do addon tambem pode RELATAR idiomas. Exigir uma secao
// rotulada e um idioma inteiro conhecido evita confundir Audio: Dolby Atmos,
// nomes de filmes e tags de release com idiomas de faixas.
static int faixaIdiomaTextoConhecido(const char *p, const char *fim) {
  static const char *const nomes[] = {
    "English", "French", "German", "Italian", "Spanish", "Portuguese",
    "Brazilian Portuguese", "Portuguese (Brazil)", "Portuguese (Portugal)",
    "Portuguese (Brasil)", "Portuguese (BR)", "Portuguese (PT)", "ptbr",
    "Português", "Portugues", "Português (Brasil)", "Português (BR)",
    "Português (Portugal)", "Inglês", "Ingles", "Francês",
    "Frances", "Alemão", "Alemao", "Italiano", "Español", "Espanhol",
    "Castilian", "Spanish (Latino)", "Spanish (Latin America)", "Latin American Spanish",
    "Arabic", "Bengali", "Bosnian", "Bulgarian", "Burmese", "Cantonese",
    "Catalan", "Chinese", "Chinese (Simplified)", "Chinese (Traditional)",
    "Croatian", "Czech", "Danish", "Dutch", "Estonian", "Finnish", "Greek",
    "Gujarati", "Hebrew", "Hindi", "Hungarian", "Icelandic", "Indonesian",
    "Japanese", "Kannada", "Khmer", "Korean", "Latvian", "Lithuanian",
    "Macedonian", "Malay", "Malayalam", "Mandarin", "Marathi", "Nepali",
    "Norwegian", "Persian", "Polish", "Punjabi", "Romanian", "Russian",
    "Serbian", "Sinhala", "Slovak", "Slovenian", "Swedish", "Tamil",
    "Telugu", "Thai", "Turkish", "Ukrainian", "Urdu", "Vietnamese",
    "Basque", "Galician", "Welsh", "Filipino", "Tagalog", "Latino", "Japonês",
    "日本語", "中文", "한국어"
  };
  size_t n = (size_t)(fim - p);
  char alias[128];
  if (n >= sizeof alias) return 0;
  memcpy(alias, p, n); alias[n] = 0;
  // Alias de entrada do addon; comparar sem traduzir os dados recebidos.
  if (!strcasecmp(alias, "Português (PT)")) return 1;
  for (unsigned k = 0; k < sizeof nomes / sizeof *nomes; k++)
    if (strlen(nomes[k]) == n && !strncasecmp(p, nomes[k], n)) return 1;
  // ISO 639-1 e os codigos de tres letras usuais dos addons. Validar a familia
  // antes das variantes BCP-47; "AAC", "DTS" e uma extensao .mkv nao contam.
  static const char *const codigos[] = {
    "aa", "ab", "ae", "af", "ak", "am", "an", "ar", "as", "av", "ay", "az",
    "ba", "be", "bg", "bh", "bi", "bm", "bn", "bo", "br", "bs", "ca", "ce",
    "ch", "co", "cr", "cs", "cu", "cv", "cy", "da", "de", "dv", "dz", "ee",
    "el", "en", "eo", "es", "et", "eu", "fa", "ff", "fi", "fj", "fo", "fr",
    "fy", "ga", "gd", "gl", "gn", "gu", "gv", "ha", "he", "hi", "ho", "hr",
    "ht", "hu", "hy", "hz", "ia", "id", "ie", "ig", "ii", "ik", "io", "is",
    "it", "iu", "ja", "jv", "ka", "kg", "ki", "kj", "kk", "kl", "km", "kn",
    "ko", "kr", "ks", "ku", "kv", "kw", "ky", "la", "lb", "lg", "li", "ln",
    "lo", "lt", "lu", "lv", "mg", "mh", "mi", "mk", "ml", "mn", "mr", "ms",
    "mt", "my", "na", "nb", "nd", "ne", "ng", "nl", "nn", "no", "nr", "nv",
    "ny", "oc", "oj", "om", "or", "os", "pa", "pi", "pl", "ps", "pt", "qu",
    "rm", "rn", "ro", "ru", "rw", "sa", "sc", "sd", "se", "sg", "si", "sk",
    "sl", "sm", "sn", "so", "sq", "sr", "ss", "st", "su", "sv", "sw", "ta",
    "te", "tg", "th", "ti", "tk", "tl", "tn", "to", "tr", "ts", "tt", "tw",
    "ty", "ug", "uk", "ur", "uz", "ve", "vi", "vo", "wa", "wo", "xh", "yi",
    "yo", "za", "zh", "zu", "eng", "fra", "fre", "deu", "ger", "ita", "spa", "por",
    "pob", "ptb", "ara", "ben", "bos", "bul", "mya", "bur", "yue", "cat", "zho", "chi",
    "cmn", "hrv", "ces", "cze", "dan", "nld", "dut", "est", "fin", "ell", "gre", "guj",
    "heb", "hin", "hun", "isl", "ice", "ind", "jpn", "kan", "khm", "kor", "lav", "lit",
    "mkd", "mac", "msa", "may", "mal", "mar", "nep", "nor", "nob", "nno", "fas", "per",
    "pol", "pan", "ron", "rum", "rus", "srp", "sin", "slk", "slo", "slv", "swe", "tam",
    "tel", "tha", "tur", "ukr", "urd", "vie", "eus", "baq", "glg", "cym", "wel", "fil"
  };
  size_t base = 0;
  while (base < n && isalpha((unsigned char)p[base]) && (unsigned char)p[base] < 128) base++;
  if (base < 2 || base > 3 || (base < n && p[base] != '-' && p[base] != '_')) return 0;
  int conhecido = 0;
  for (unsigned k = 0; k < sizeof codigos / sizeof *codigos; k++)
    if (strlen(codigos[k]) == base && !strncasecmp(p, codigos[k], base)) { conhecido = 1; break; }
  if (!conhecido) return 0;
  for (size_t k = base; k < n;) {
    if (p[k] != '-' && p[k] != '_') return 0;
    size_t ini = ++k;
    while (k < n && (unsigned char)p[k] < 128 && isalnum((unsigned char)p[k])) k++;
    if (k == ini || k - ini > 8) return 0;
  }
  return 1;
}
static const char *faixasRotuloSubsTexto(const char *p, const char *fim, int *tipo) {
  // Torrentio/AIOStreams: so a parte DEPOIS de "Subs /" e legenda. Os
  // indicadores regionais anteriores nao revelam se eram audio ou Multi Subs.
  static const char *const subs[] = {"multi subs", "subs"};
  for (unsigned k = 0; k < sizeof subs / sizeof *subs; k++) {
    size_t n = strlen(subs[k]);
    if ((size_t)(fim - p) < n || strncasecmp(p, subs[k], n)) continue;
    const char *q = faixasEspacos(p + n, fim);
    if (q < fim && *q == '/') { *tipo = 2; return q + 1; }
  }
  return p;
}
static const char *faixasRotuloPortuguesTexto(const char *p, const char *fim, int *tipo) {
  char rotulo[64];
  const char *q = p;
  while (q < fim && *q != ':' && (size_t)(q - p) < sizeof rotulo) q++;
  if (q == fim || *q != ':') return p;
  const char *ate = q;
  while (ate > p && isspace((unsigned char)ate[-1])) ate--;
  size_t n = (size_t)(ate - p);
  if (n >= sizeof rotulo) return p;
  memcpy(rotulo, p, n); rotulo[n] = 0;
  // Estes sao rotulos recebidos, nao texto de interface a traduzir.
  if (!strcasecmp(rotulo, "áudio") || !strcasecmp(rotulo, "Áudio") ||
      !strcasecmp(rotulo, "idiomas de áudio")) {
    *tipo = 1;
    return q + 1;
  }
  return p;
}
static const char *faixasRotuloTexto(const char *p, const char *fim, int *tipo) {
  static const struct { const char *rotulo; int tipo; } rotulos[] = {
    {"audio languages", 1}, {"audio language", 1}, {"audio", 1},
    {"languages", 1}, {"language", 1},
    {"subtitle languages", 2}, {"subtitle language", 2}, {"subtitles", 2},
    {"subtitle", 2}, {"subs", 2}, {"legendas", 2}, {"legenda", 2}
  };
  for (unsigned k = 0; k < sizeof rotulos / sizeof *rotulos; k++) {
    size_t n = strlen(rotulos[k].rotulo);
    if ((size_t)(fim - p) < n || strncasecmp(p, rotulos[k].rotulo, n)) continue;
    const char *q = faixasEspacos(p + n, fim);
    if (q < fim && *q == ':') { *tipo = rotulos[k].tipo; return q + 1; }
  }
  const char *portugues = faixasRotuloPortuguesTexto(p, fim, tipo);
  return portugues != p ? portugues : faixasRotuloSubsTexto(p, fim, tipo);
}
static const char *faixaIdiomaEmoji(const char *p, const char *fim) {
  // Inversa APENAS dos valores univocos do formatador AIOStreams, dentro de
  // uma secao ja declarada. Nao e uma tabela de paises para inferir idiomas:
  // 🇮🇳 representa sete idiomas e deve continuar desconhecido.
  // https://github.com/Viren070/AIOStreams/blob/main/packages/core/src/utils/languages.ts
  static const struct { const char *emoji, *idioma; } idiomas[] = {
    {"🇬🇧", "English"}, {"🇯🇵", "Japanese"}, {"🇨🇳", "Chinese"},
    {"🇷🇺", "Russian"}, {"🇸🇦", "Arabic"}, {"🇵🇹", "Portuguese"},
    {"🇧🇷", "Portuguese (Brazil)"}, {"🇪🇸", "Spanish"}, {"🇫🇷", "French"},
    {"🇩🇪", "German"}, {"🇮🇹", "Italian"}, {"🇰🇷", "Korean"},
    {"🇧🇩", "Bengali"}, {"🇵🇰", "Punjabi"}, {"🇹🇭", "Thai"},
    {"🇻🇳", "Vietnamese"}, {"🇮🇩", "Indonesian"}, {"🇹🇷", "Turkish"},
    {"🇮🇱", "Hebrew"}, {"🇮🇷", "Persian"}, {"🇺🇦", "Ukrainian"},
    {"🇬🇷", "Greek"}, {"🇱🇹", "Lithuanian"}, {"🇱🇻", "Latvian"},
    {"🇪🇪", "Estonian"}, {"🇵🇱", "Polish"}, {"🇨🇿", "Czech"},
    {"🇸🇰", "Slovak"}, {"🇭🇺", "Hungarian"}, {"🇷🇴", "Romanian"},
    {"🇧🇬", "Bulgarian"}, {"🇷🇸", "Serbian"}, {"🇭🇷", "Croatian"},
    {"🇸🇮", "Slovenian"}, {"🇳🇱", "Dutch"}, {"🇩🇰", "Danish"},
    {"🇫🇮", "Finnish"}, {"🇸🇪", "Swedish"}, {"🇳🇴", "Norwegian"},
    {"🇲🇾", "Malay"}, {"💃🏻", "Latino"}
  };
  size_t n = (size_t)(fim - p);
  for (unsigned k = 0; k < sizeof idiomas / sizeof *idiomas; k++)
    if (strlen(idiomas[k].emoji) == n && !memcmp(p, idiomas[k].emoji, n)) return idiomas[k].idioma;
  return NULL;
}
static size_t faixasSeparadorTexto(const char *p, const char *fim) {
  if (p >= fim) return 0;
  if (*p == ',' || *p == '|' || *p == ';' || *p == '/') return 1;
  return (size_t)(fim - p) >= strlen("•") && !memcmp(p, "•", strlen("•")) ? strlen("•") : 0;
}
static size_t faixasMarcadorTexto(const char *p, const char *fim, int *tipo) {
  static const struct { const char *emoji; int tipo; } marcadores[] = {
    {"🌎", 3}, {"🌐", 1}, {"🗣", 1}, {"⚑", 1}, {"📝", 2}
  };
  for (unsigned k = 0; k < sizeof marcadores / sizeof *marcadores; k++) {
    size_t n = strlen(marcadores[k].emoji);
    if ((size_t)(fim - p) < n || memcmp(p, marcadores[k].emoji, n)) continue;
    // 🌎 tambem e o valor "Multi" do AIO. Sua secao so aceita nomes, como
    // GDrive; nao transformar uma lista generica "🌎 / 🇬🇧" em audio.
    *tipo = marcadores[k].tipo;
    if ((size_t)(fim - p - n) >= strlen("️") && !memcmp(p + n, "️", strlen("️"))) n += strlen("️");
    return n;
  }
  if (p >= fim || (unsigned char)*p < 0x80) return 0;
  // Qualquer outro icone tecnico encerra a secao (🎧/🔊 sao codec/canais).
  // Indicadores regionais e o valor Latino permanecem valores, nao marcadores.
  if ((size_t)(fim - p) >= strlen("💃🏻") && !memcmp(p, "💃🏻", strlen("💃🏻"))) return 0;
  const unsigned char *q = (const unsigned char *)p;
  unsigned cp; size_t n;
  if (*q >= 0xC2 && *q <= 0xDF) { cp = *q & 31; n = 2; }
  else if (*q >= 0xE0 && *q <= 0xEF) { cp = *q & 15; n = 3; }
  else if (*q >= 0xF0 && *q <= 0xF4) { cp = *q & 7; n = 4; }
  else return 0;
  if ((size_t)(fim - p) < n) return 0;
  for (size_t k = 1; k < n; k++) cp = (cp << 6) | (q[k] & 63);
  if (!((cp >= 0x1F000 && cp <= 0x1FAFF && !(cp >= 0x1F1E6 && cp <= 0x1F1FF) &&
         !(cp >= 0x1F3FB && cp <= 0x1F3FF)) ||
        (cp >= 0x2600 && cp <= 0x27BF))) return 0;
  *tipo = 0;
  if ((size_t)(fim - p - n) >= strlen("️") && !memcmp(p + n, "️", strlen("️"))) n += strlen("️");
  return n;
}
static int faixasIndicadoresTexto(const char *p, const char *fim) {
  int indicador = 0;
  while ((p = faixasEspacos(p, fim)) < fim) {
    // O prefixo inline "Subs /" do Torrentio/AIOStreams pode conter apenas
    // indicadores regionais; "No Subs / English" nao declara uma faixa.
    if (fim - p < 4 || memcmp(p, "\xF0\x9F\x87", 3) ||
        (unsigned char)p[3] < 0xA6 || (unsigned char)p[3] > 0xBF) return 0;
    indicador = 1;
    p += 4;
  }
  return indicador;
}
static void faixasLinhaTexto(const char *p, const char *fim, char *audio, unsigned na,
                             char *subs, unsigned ns) {
  int tipo = 0;
  while (p < fim) {
    p = faixasEspacos(p, fim);
    size_t n = faixasSeparadorTexto(p, fim);
    if (n) { p += n; continue; }
    n = faixasMarcadorTexto(p, fim, &tipo);
    if (n) {
      p = faixasEspacos(p + n, fim);
      if (p < fim && *p == ':') p++;
      continue;
    }
    const char *rotulado = faixasRotuloTexto(p, fim, &tipo);
    if (rotulado != p) { p = rotulado; continue; }
    const char *q = p;
    for (; q < fim; q++) {
      int proximo;
      // Uma URL e um valor inteiro: suas barras nao separam idiomas nem
      // permitem reiniciar um rotulo no caminho, como /subtitles:fr.
      if (fim - q >= 3 && !memcmp(q, "://", 3)) {
        tipo = 0;
        q += 3;
        while (q < fim && !isspace((unsigned char)*q) && *q != '|' && *q != ',' && *q != ';') q++;
        break;
      }
      if (faixasSeparadorTexto(q, fim) || faixasMarcadorTexto(q, fim, &proximo)) break;
      // Subs / pode vir imediatamente apos os indicadores sem separador.
      if ((q == p || isspace((unsigned char)q[-1])) && faixasRotuloSubsTexto(q, fim, &proximo) != q) {
        const char *ate = q;
        while (ate > p && isspace((unsigned char)ate[-1])) ate--;
        if (q == p || faixasIndicadoresTexto(p, ate)) break;
      }
    }
    const char *ini = faixasEspacos(p, q), *ate = q;
    while (ate > ini && isspace((unsigned char)ate[-1])) ate--;
    // Outro campo encerra a secao; nao emprestar os valores seguintes.
    if (memchr(ini, ':', (size_t)(ate - ini))) tipo = 0;
    if (tipo) {
      char *dst = tipo == 2 ? subs : audio;
      unsigned tam = tipo == 2 ? ns : na;
      const char *idioma = tipo != 3 ? faixaIdiomaEmoji(ini, ate) : NULL;
      if (idioma) faixaIdiomaAdicionar(dst, tam, idioma, idioma + strlen(idioma));
      else if (faixaIdiomaTextoConhecido(ini, ate)) faixaIdiomaAdicionar(dst, tam, ini, ate);
    }
    p = q;
  }
}
static void faixasCampoTexto(const char *obj, const char *fim, const char *campo, Stream *s) {
  if (s->audioIdiomas[0] && s->legendaIdiomas[0]) return;
  const char *v = valorRaiz(obj, fim, campo), *f;
  char texto[2048], json[sizeof texto + 16];
  char audio[sizeof s->audioIdiomas] = "", subs[sizeof s->legendaIdiomas] = "";
  if (!v || !(f = faixasFimTexto(v, fim)) || (size_t)(f - v - 2) >= sizeof texto) return;
  const char *apos = faixasEspacos(f, fim);
  if (apos >= fim || (*apos != ',' && *apos != '}')) return;
  // A descricao permite quebras, mas nunca NUL, controles ocultos ou um valor
  // cortado cujo prefixo tenha virado um idioma aparentemente completo.
  for (const char *q = v + 1; q < f - 1; q++) if (*q == '\\') {
    q++;
    if (*q == 'b' || *q == 'f') return;
    if (*q == 'u') {
      unsigned cp = faixasHex4(q + 1);
      if ((cp < 0x20 && cp != '\n' && cp != '\r' && cp != '\t') || cp == 0x7F) return;
      q += 4;
    }
  }
  // Decodificar somente o valor validado e ancorado, preservando as quebras.
  // Um objeto temporario pequeno usa o decodificador JSON existente sem
  // permitir que uma chave igual em headers/outro objeto substitua a fonte.
  size_t n = (size_t)(f - v);
  memcpy(json, "{\"v\":", 5);
  memcpy(json + 5, v, n);
  json[n + 5] = '}'; json[n + 6] = 0;
  // O decodificador geral descarta \\r. Aqui uma quebra CR tambem delimita
  // metadados; normalizar so esse escape, sem alterar um \\\\r literal.
  for (size_t k = 6; k < n + 4; k++) if (json[k] == '\\') {
    if (json[++k] == 'r') json[k] = 'n';
  }
  if (!js_texto_linhas(json, json + n + 6, "v", texto, sizeof texto) ||
      !faixasUtf8((const unsigned char *)texto)) return;
  for (const char *p = texto; *p;) {
    const char *q = p;
    while (*q && *q != '\n' && *q != '\r') q++;
    faixasLinhaTexto(p, q, audio, sizeof audio, subs, sizeof subs);
    p = *q ? q + 1 : q;
  }
  if (!s->audioIdiomas[0]) memcpy(s->audioIdiomas, audio, sizeof audio);
  if (!s->legendaIdiomas[0]) memcpy(s->legendaIdiomas, subs, sizeof subs);
}
static void metadadosFaixas(const char *obj, const char *fim, Stream *s) {
  const char *cr = objetoRaiz(obj, fim, "clientResolve");
  const char *st = cr ? objetoRaiz(cr, js_fim(cr), "stream") : NULL;
  const char *raw = st ? objetoRaiz(st, js_fim(st), "raw") : NULL;
  const char *parsed = raw ? objetoRaiz(raw, js_fim(raw), "parsed") : NULL;
  if (!parsed && st) parsed = objetoRaiz(st, js_fim(st), "parsed");
  if (!parsed && cr) parsed = objetoRaiz(cr, js_fim(cr), "parsed");
  if (!raw) raw = objetoRaiz(obj, fim, "raw");
  if (!parsed && raw) parsed = objetoRaiz(raw, js_fim(raw), "parsed");
  const char *parsedRaiz = objetoRaiz(obj, fim, "parsed");
  const char *data = objetoRaiz(obj, fim, "streamData");
  const char *parsedFile = data ? objetoRaiz(data, js_fim(data), "parsedFile") : NULL;
  const char *objetos[] = {parsedFile, parsed, parsedRaiz, raw, st, cr, obj};
  static const char *const audio[] = {"audioLanguages", "audio_languages", "audioTracks", "audio_tracks"};
  static const char *const subs[] = {"subtitleLanguages", "subtitle_languages", "subtitleTracks", "subtitle_tracks"};
  for (unsigned k = 0; k < sizeof objetos / sizeof *objetos; k++) if (objetos[k]) {
    const char *f = js_fim(objetos[k]);
    for (unsigned j = 0; j < sizeof audio / sizeof *audio; j++)
      faixasCampo(objetos[k], f, audio[j], s->audioIdiomas, sizeof s->audioIdiomas, j >= 2);
    for (unsigned j = 0; j < sizeof subs / sizeof *subs; j++)
      faixasCampo(objetos[k], f, subs[j], s->legendaIdiomas, sizeof s->legendaIdiomas, j >= 2);
    const char *tracks = objetoRaiz(objetos[k], f, "tracks");
    if (tracks) {
      const char *tf = js_fim(tracks);
      faixasCampo(tracks, tf, "audio", s->audioIdiomas, sizeof s->audioIdiomas, 1);
      faixasCampo(tracks, tf, "subtitles", s->legendaIdiomas, sizeof s->legendaIdiomas, 1);
      faixasCampo(tracks, tf, "subtitle", s->legendaIdiomas, sizeof s->legendaIdiomas, 1);
    }
  }
  // AIOStreams separa explicitamente languages (audio) de subtitles no
  // ParsedFile. So estes caminhos documentados autorizam o nome generico.
  // https://github.com/Viren070/AIOStreams/blob/main/packages/core/src/db/schemas.ts
  if (parsedFile) {
    faixasCampo(parsedFile, js_fim(parsedFile), "languages", s->audioIdiomas, sizeof s->audioIdiomas, 0);
    faixasCampo(parsedFile, js_fim(parsedFile), "subtitles", s->legendaIdiomas, sizeof s->legendaIdiomas, 0);
  }
  if (parsed) {
    faixasCampo(parsed, js_fim(parsed), "languages", s->audioIdiomas, sizeof s->audioIdiomas, 0);
    faixasCampo(parsed, js_fim(parsed), "subtitles", s->legendaIdiomas, sizeof s->legendaIdiomas, 0);
  }
  // Stremio stream.subtitles sao legendas anexadas a ESTA fonte. Nao buscar
  // no objeto de resposta/titulo, nem afirmar que estas faixas sao embutidas.
  faixasCampo(obj, fim, "subtitles", s->legendaIdiomas, sizeof s->legendaIdiomas, 1);
  // Valores estruturados continuam prioritarios, por tipo. Na falta deles,
  // ler somente secoes declaradas no texto DESTA fonte, nunca seu name,
  // provedor, filename, filtro de idiomas ou faixas descobertas pelo player.
  faixasCampoTexto(obj, fim, "description", s);
  faixasCampoTexto(obj, fim, "title", s);
}

// Decimal integers only, parsed without double rounding. Numeric strings
// are accepted for compatibility, but fractions/exponents/overflow are
// unavailable for fit. Legacy display size remains separate below.
static uint64_t bytesExatos(const char *obj, const char *fim) {
  const char *bh = valorRaiz(obj, fim, "behaviorHints"), *p, *bf;
  uint64_t valor = 0;
  int quoted;
  if (!bh || *bh != '{' || !(bf = js_fim(bh)) || bf > fim) return 0;
  p = valorRaiz(bh, bf, "videoSize");
  if (!p) return 0;
  quoted = *p == '"'; if (quoted) p++;
  if (p >= bf || !isdigit((unsigned char)*p)) return 0;
  if (!quoted && *p == '0' && p + 1 < bf && isdigit((unsigned char)p[1])) return 0;
  while (p < bf && isdigit((unsigned char)*p)) {
    unsigned d = (unsigned)(*p++ - '0');
    if (valor > (STREAMFIT_BYTES_MAX - d) / 10) return 0;
    valor = valor * 10 + d;
  }
  if (quoted) { if (p >= bf || *p++ != '"') return 0; }
  while (p < bf && isspace((unsigned char)*p)) p++;
  return p < bf && (*p == ',' || *p == '}') ? valor : 0;
}

static int contem(const char *s, const char *termo) {
  for (; *s; s++) if (!strncasecmp(s, termo, strlen(termo))) return 1;
  return 0;
}

// "FHD"/"Full HD" como ROTULO SOLTO do formatador (#402): AIOStreams e afins
// escrevem "FHD | REMUX | SDR" ou "FHD \u2022 REMUX". Aceita so se:
//  - os dois lados sao inicio/fim de linha, espaco, '|', '\u2022' ou '\u00b7'
//    (nunca '.', '_', '-', '/', '[' — isso e nome de arquivo ou URL);
//  - e o primeiro rotulo da linha, ou encosta num '|' / bullet (pulando
//    espaco). "The FHD Story" e frase, nao rotulo;
//  - o texto nao tem "://".
// Nao o "hd" solto do nv_res_do_texto ("DTS-HD" viraria 720).
static int ehSepFhd(const char *a, const char *ini, int antes) {
  const unsigned char *u = (const unsigned char *)a;
  if (antes) {
    if (a == ini || u[-1] == '\n' || u[-1] == ' ' || u[-1] == '\t' || u[-1] == '|') return 1;
    if (a - ini >= 3 && u[-3] == 0xE2 && u[-2] == 0x80 && u[-1] == 0xA2) return 1;
    return a - ini >= 2 && u[-2] == 0xC2 && u[-1] == 0xB7;
  }
  return !*u || *u == '\n' || *u == ' ' || *u == '\t' || *u == '|' ||
         (u[0] == 0xE2 && u[1] == 0x80 && u[2] == 0xA2) || (u[0] == 0xC2 && u[1] == 0xB7);
}
// Prefixo da linha feito so de SIMBOLO/emoji e espaco, e curto (ate 2 simbolos):
// "\u23f3 FHD", "\u26a1 FHD". So os blocos de simbolo do UTF-8 contam — E2 80..AF xx
// (U+2000..U+2BFF: setas, relogios, raios, estrelas; de U+2C00 em diante ha letras,
// Glagolitico e Georgiano; U+2E00..U+2FFF tem pontuacao e radicais, recusados por
// seguranca) e F0 9F xx xx (emoji), com EF B8 8F
// (seletor de variacao) junto. Letra de outro alfabeto (Cirilico D0/D1, CJK
// E3..E9) e palavra, nao rotulo: "Фильм FHD" e "我的 FHD" continuam frase.
static int prefixoSoSimbolos(const char *ini, const char *fim) {
  const unsigned char *u = (const unsigned char *)ini, *f = (const unsigned char *)fim;
  int simbolos = 0;
  while (u < f) {
    if (*u == ' ' || *u == '\t') { u++; continue; }
    if (f - u >= 3 && u[0] == 0xEF && u[1] == 0xB8 && u[2] == 0x8F) { u += 3; continue; }
    if (f - u >= 3 && u[0] == 0xE2 && u[1] >= 0x80 && u[1] <= 0xAF && (u[2] & 0xC0) == 0x80) u += 3;
    else if (f - u >= 4 && u[0] == 0xF0 && u[1] == 0x9F && (u[2] & 0xC0) == 0x80 &&
             (u[3] & 0xC0) == 0x80) u += 4;
    else return 0;
    if (++simbolos > 2) return 0;
  }
  return simbolos > 0;
}
static int ehRotuloSep(const unsigned char *u) {   // '|' ou bullet comecando em u
  return *u == '|' || (u[0] == 0xE2 && u[1] == 0x80 && u[2] == 0xA2) ||
         (u[0] == 0xC2 && u[1] == 0xB7);
}
static int fhdNoNome(const char *s) {
  static const char *const t[] = {"fhd", "fullhd", "full hd", "full-hd"};
  if (strstr(s, "://")) return 0;
  for (const char *p = s; *p; p++)
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
      size_t n = strlen(t[i]);
      if (strncasecmp(p, t[i], n) || !ehSepFhd(p, s, 1) || !ehSepFhd(p + n, s, 0)) continue;
      const char *a = p, *d = p + n;
      while (a > s && (a[-1] == ' ' || a[-1] == '\t')) a--;
      while (*d == ' ' || *d == '\t') d++;
      if (a == s || a[-1] == '\n' || ehRotuloSep((const unsigned char *)d)) return 1;
      if (a[-1] == '|' ||
          (a - s >= 3 && (unsigned char)a[-1] == 0xA2 && (unsigned char)a[-2] == 0x80 &&
           (unsigned char)a[-3] == 0xE2) ||
          (a - s >= 2 && (unsigned char)a[-1] == 0xB7 && (unsigned char)a[-2] == 0xC2)) return 1;
      // So simbolos (emoji, bytes >= 0x80) e espaco antes na linha: "\u23f3 FHD" do
      // AIOStreams e rotulo; uma palavra ASCII antes ("The FHD") continua frase.
      { const char *ls = a;
        while (ls > s && ls[-1] != '\n') ls--;
        if (prefixoSoSimbolos(ls, a)) return 1; }
    }
  return 0;
}

// Token isolado reconhece .DV., DV/HDR e [DV], mas nunca DVD/DVDRip.
static int token(const char *s, const char *t) {
  size_t n = strlen(t);
  const char *p;
  for (p = s; *p; p++)
    if ((p == s || !isalnum((unsigned char)p[-1])) &&
        !strncasecmp(p, t, n) && !isalnum((unsigned char)p[n])) return 1;
  return 0;
}

// CABECALHOS EXIGIDOS PELO ADDON: behaviorHints.proxyHeaders.request.
//
// MEDIDO em 17/09 contra o addon de um relato: o CDN devolve 403 sem
// Referer/Origin/User-Agent e 200 com eles. O parser entrava em behaviorHints
// so para pegar bingeGroup, entao esses cabecalhos eram descartados e QUALQUER
// addon que dependa de Referer ficava sem tocar, nos dois alvos.
//
// A MAO, e nao com js_texto: os nomes das chaves aqui sao escolhidos pelo
// ADDON (sao nomes de cabecalho HTTP), entao nao ha lista de chaves conhecidas
// para procurar — e preciso varrer os pares que existirem. So profundidade 1
// de "request", que e onde a convencao do Stremio poe os cabecalhos.
//
// Saida no formato que rede.h aceita: uma linha "Nome: valor" por cabecalho.
static void lerProxyHeaders(const char *bh, const char *fim, char *dst, unsigned tam) {
  const char *ph, *rq, *q;
  unsigned u = 0;
  dst[0] = 0;
  ph = buscaEm(bh, fim, "\"proxyHeaders\"");
  if (!ph) return;
  rq = buscaEm(ph, fim, "\"request\"");
  if (!rq) return;
  q = memchr(rq, '{', (size_t)(fim - rq));
  if (!q) return;
  q++;
  // Varre pares "nome":"valor" ate a chave que fecha o objeto. Valor de
  // cabecalho e string, entao um '{' aqui e coisa que nao se esperava: encerra
  // em vez de tentar adivinhar.
  while (q < fim && *q && *q != '}' && *q != '{') {
    char nome[96], valor[320];
    unsigned k;
    const char *f;
    while (q < fim && *q && *q != '"' && *q != '}') q++;
    if (q >= fim || *q != '"') break;
    q++;
    f = q;
    while (f < fim && *f && *f != '"') f++;
    if (f >= fim || *f != '"') break;
    k = (unsigned)(f - q);
    if (k >= sizeof nome) k = sizeof nome - 1;
    memcpy(nome, q, k); nome[k] = 0;
    q = f + 1;
    while (q < fim && *q && *q != ':') q++;
    if (q >= fim || *q != ':') break;
    q++;
    while (q < fim && (*q == ' ' || *q == '\t')) q++;
    if (q >= fim || *q != '"') break;   // valor nao-string: nao e cabecalho
    q++;
    f = q;
    while (f < fim && *f && *f != '"') f++;
    if (f >= fim || *f != '"') break;
    k = (unsigned)(f - q);
    if (k >= sizeof valor) k = sizeof valor - 1;
    memcpy(valor, q, k); valor[k] = 0;
    q = f + 1;
    if (nome[0] && valor[0]) {
      int esc = snprintf(dst + u, tam - u, "%s%s: %s", u ? "\n" : "", nome, valor);
      if (esc < 0 || (unsigned)esc >= tam - u) { dst[u] = 0; break; }
      u += (unsigned)esc;
    }
    while (q < fim && (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')) q++;
    if (q < fim && *q == ',') q++;
  }
}

// "FORA DE CACHE" DITO PELO ADDON. As marcas sao as que os addons mandam de
// verdade: "⏳" e o que o AIOStreams poe no nome de toda fonte nao cacheada
// (registros 1136, 2191, 2501: "⏳ UHD", "⏳ FHD"; a cacheada vem com "⚡"), o
// Torrentio escreve "[TB download]" / "[RD download]" (a cacheada e "[TB+]"),
// e outros usam "⬇" ou a palavra "uncached". "download]" e nao "download"
// solto: "WEB-DL"/"Download" aparecem em titulo de fonte cacheada.
int stream_texto_fora_de_cache(const char *t) {
  if (!t) return 0;
  return strstr(t, "\xe2\x8f\xb3") != NULL          // U+23F3 ⏳
      || strstr(t, "\xe2\xac\x87") != NULL          // U+2B07 ⬇
      || contem(t, " download]") || contem(t, "uncached") || contem(t, "not cached");
}

// As marcas positivas que os addons usam (AIOStreams/Torrentio). Nao ler
// arquivo, URL ou o nome configurado do addon: nao descrevem disponibilidade.
static int textoEmCache(const char *t) {
  return t && (strstr(t, "\xe2\x9a\xa1") || contem(t, "[TB+]") || contem(t, "[RD+]") ||
               contem(t, "[PM+]") || contem(t, "[AD+]"));
}
static int marcaEmCache(const char *p, const char *fim) {
  static const char *const campos[] = {"name", "description", "title"};
  char texto[2048];
  for (unsigned i = 0; i < sizeof campos / sizeof *campos; i++)
    if (js_texto_raiz_em(p, fim, campos[i], texto, sizeof texto) && textoEmCache(texto)) return 1;
  return 0;
}

// Booleano da PROPRIA fonte, nao de cabecalhos nem da fonte seguinte.
// 0 = ausente/invalido; -1 = false; 1 = true. Strings e numeros nao contam.
static int cacheDeclarado(const char *p, const char *fim) {
  const char *cr = objetoRaiz(p, fim, "clientResolve");
  if (!cr) return 0;
  const char *f = js_fim(cr), *v = valorRaiz(cr, f, "isCached");
  if (!v) return 0;
  unsigned n = *v == 't' ? 4 : 5;
  if (f - v < n || memcmp(v, n == 4 ? "true" : "false", n)) return 0;
  const char *apos = faixasEspacos(v + n, f);
  return apos < f && (*apos == ',' || *apos == '}') ? (n == 4 ? 1 : -1) : 0;
}

// "sources": ["tracker:udp://...", "dht:<hash>"] de um stream de torrent, uma
// entrada por linha em `dst`. So as que o servidor de streaming do Stremio
// entende (prefixo tracker: ou dht:); uma URL solta de tracker ganha o prefixo.
// A entrada que nao cabe inteira fica de fora (nunca cortada no meio: um
// tracker pela metade e endereco torto). \/ vira / (JSON escapa a barra).
static void lerFontesP2P(const char *ini, const char *fim, char *dst, unsigned tam) {
  const char *q = buscaEm(ini, fim, "\"sources\"");
  unsigned u = 0;
  dst[0] = 0;
  if (!q) return;
  q += 9;
  while (q < fim && (*q == ' ' || *q == ':' || *q == '\n' || *q == '\t')) q++;
  if (q >= fim || *q != '[') return;
  q++;
  while (q < fim && *q != ']') {
    char e[300];
    unsigned k = 0;
    if (*q != '"') { q++; continue; }
    q++;
    while (q < fim && *q != '"' && k + 1 < sizeof e) {
      if (*q == '\\' && q + 1 < fim) q++;      // \/ -> /
      e[k++] = *q++;
    }
    e[k] = 0;
    while (q < fim && *q != '"') q++;          // entrada maior que o buffer: pula o resto
    if (q < fim) q++;
    if (!strncmp(e, "tracker:", 8) || !strncmp(e, "dht:", 4) ||
        !strncmp(e, "udp://", 6) || !strncmp(e, "http://", 7) || !strncmp(e, "https://", 8)) {
      char ent[320];
      int L = snprintf(ent, sizeof ent, "%s%s",
                       strncmp(e, "tracker:", 8) && strncmp(e, "dht:", 4) ? "tracker:" : "", e);
      if (L > 0 && u + (unsigned)L + 2 <= tam) {
        u += (unsigned)snprintf(dst + u, tam - u, "%s%s", u ? "\n" : "", ent);
      }
    }
  }
}

// SEMEADORES no texto do addon. Nao ha campo para isso no protocolo: cada
// addon escreve do seu jeito, sempre marcador + numero. Torrentio e
// MediaFusion usam "👤 12", Comet/Jackettio "👥 12", outros "Seeders: 12".
// Devolve 1 e preenche *qtd; limite de 9 digitos para nao estourar o int.
static int lerSemeadores(const char *t, int *qtd) {
  static const char *const marcas[] = { "\xF0\x9F\x91\xA4", "\xF0\x9F\x91\xA5", "\xF0\x9F\x8C\xB1",
                                        "Seeders", "seeders", "Seeds", "seeds" };
  for (size_t k = 0; k < sizeof marcas / sizeof *marcas; k++) {
    const char *p = strstr(t, marcas[k]);
    if (!p) continue;
    p += strlen(marcas[k]);
    while (*p == ' ' || *p == ':' || *p == '\t') p++;
    if (!isdigit((unsigned char)*p)) continue;
    long v = 0; int d = 0;
    while (isdigit((unsigned char)*p) && d < 9) { v = v * 10 + (*p++ - '0'); d++; }
    *qtd = (int)v;
    return 1;
  }
  return 0;
}

int stream_extrair(const char *json, const char *provedor, Stream **saida) {
  const char *p, *fim;
  int n = 0, cap = 0;
  Stream *v = NULL;
  *saida = NULL;
  if (!json) return 0;
  p = js_array(json, json + strlen(json), "streams");
  while (p && *p == '{') {
    Stream s = {0};
    char titulo[2048] = "", nomeFonte[1024] = "", texto[5000];
    fim = js_fim(p);
    if (!fim || fim <= p) break;
    // externalUrl abre fora do player; sem midia/hash a fonte fica oculta.
    js_texto_raiz_em(p, fim, "url", s.url, sizeof s.url);
    if (strlen(s.url) == sizeof s.url - 1) {
      size_t tam = (size_t)(fim - p) + 1;
      char *url = malloc(tam);
      if (!url) { p = js_prox(fim); continue; }
      js_texto_raiz_em(p, fim, "url", url, tam);
      int ok = !strncmp(url, "http", 4) && nv_media_url_guardar(url, s.url, sizeof s.url);
      free(url);
      if (!ok) { p = js_prox(fim); continue; }
    }
    { const char *bh = valorRaiz(p, fim, "behaviorHints");
      const char *hint = bh ? valorRaiz(bh, fim, "notWebReady") : NULL;
      s.notWebReady = hint && fim - hint >= 4 && !memcmp(hint, "true", 4) &&
        (hint + 4 == fim || hint[4] == ',' || hint[4] == '}' || isspace((unsigned char)hint[4]));
    }
    s.fileIdx = -1;
    // Torrent puro: sem url, com infoHash (no topo ou dentro de clientResolve,
    // como o AIOStreams manda). A url nasce depois, no debrid.
    if (!s.url[0] || strncmp(s.url, "http", 4)) {
      const char *cr = objetoRaiz(p, fim, "clientResolve");
      s.url[0] = 0;
      if (!js_texto(p, fim, "infoHash", s.infoHash, sizeof s.infoHash) && cr)
        js_texto(cr, fim, "infoHash", s.infoHash, sizeof s.infoHash);
      double indice = js_num(p, fim, "fileIdx", -1);
      if (isfinite(indice) && indice >= 0 && indice <= INT_MAX)
        s.fileIdx = (int)indice;
      if (s.infoHash[0]) lerFontesP2P(p, fim, s.fontes, sizeof s.fontes);
    } else {
      // Fonte com link: o hash, quando o addon manda, so serve para achar o
      // torrent na conta TorBox (partes.h). Nao vira infoHash.
      const char *cr = objetoRaiz(p, fim, "clientResolve");
      if (!js_texto(p, fim, "infoHash", s.hashTorrent, sizeof s.hashTorrent) && cr)
        js_texto(cr, fim, "infoHash", s.hashTorrent, sizeof s.hashTorrent);
    }
    // Without a complete URL/hash there is nothing to play.
    if (s.infoHash[0] || !strncmp(s.url, "http", 4)) {
      js_texto_raiz_em(p, fim, "name", s.rotulo, sizeof s.rotulo);
      // So o name da PROPRIA fonte, inteiro, conta para o FHD (#402): o
      // rotulo de 192 bytes pode cortar "...FHDx" em "...FHD", e o provedor
      // entra no lugar do name que falta mais abaixo.
      int fhd = 0;
      if (textoRaizLinhas(p, fim, "name", nomeFonte, sizeof nomeFonte) && strlen(nomeFonte) < sizeof nomeFonte - 1)
        fhd = fhdNoNome(nomeFonte);
      textoRaizLinhas(p, fim, "description", s.descricao, sizeof s.descricao);
      textoRaizLinhas(p, fim, "title", titulo, sizeof titulo);
      int cache = cacheDeclarado(p, fim);
      s.emCache = cache > 0 || (cache == 0 && marcaEmCache(p, fim));
      s.cacheNegado = cache < 0;
      metadadosFaixas(p, fim, &s);
      metadadosFiltro(p, fim, nomeFonte, s.descricao, titulo, &s);
      // behaviorHints.bingeGroup — ANCORADO NO OBJETO, e nao procurado solto.
      //
      // Duas coisas separadas, e as duas custam uma linha:
      //
      // 1. `bh < fim` E O QUE IMPORTA DE VERDADE. strstr varre ate o fim do
      //    documento, nao ate o fim DESTE stream: sem a guarda, uma fonte que
      //    nao manda behaviorHints herdaria o bingeGroup da fonte SEGUINTE do
      //    array — e herdaria calada, com o sintoma sendo o app lembrar da
      //    fonte errada no episodio seguinte. Mesma guarda que o
      //    "clientResolve" logo acima ja usa, e pelo mesmo motivo.
      // 2. js_texto_raiz_em le so as chaves de PROFUNDIDADE 1 de
      //    behaviorHints. behaviorHints tem objeto dentro dele
      //    (proxyHeaders.request/response, com nomes de cabecalho que o addon
      //    escolhe), e a regra da casa para chave aninhada de Stremio esta em
      //    js.h: procurar solto pega a primeira ocorrencia, que nem sempre e a
      //    que se quer. Nas respostas que deu para inspecionar aqui o js_texto
      //    solto daria o mesmo resultado; ancorar e seguro de graca.
      { const char *bh = objetoRaiz(p, fim, "behaviorHints");
        if (bh) {
          js_texto_raiz_em(bh, fim, "bingeGroup", s.bingeGroup, sizeof s.bingeGroup);
          js_texto_raiz_em(bh, fim, "videoHash", s.videoHash, sizeof s.videoHash);
          lerProxyHeaders(bh, fim, s.cabecalhos, sizeof s.cabecalhos);
        } }
      // Preserve both sibling fields, as official Nuvio does. Title often
      // carries the provider/release while description carries size/language.
      if (titulo[0] && strcmp(titulo, s.descricao)) {
        char descricao[sizeof s.descricao];
        snprintf(descricao, sizeof descricao, "%s", s.descricao);
        snprintf(s.descricao, sizeof s.descricao, "%s%s%s", titulo,
                 descricao[0] ? "\n" : "", descricao);
      }
      if (!s.rotulo[0]) snprintf(s.rotulo, sizeof s.rotulo, "%s", provedor);
      snprintf(s.provedor, sizeof s.provedor, "%s", provedor);
      snprintf(texto, sizeof texto, "%s %s %s %s", s.rotulo, s.descricao, titulo, s.arquivo);
      for (char *q = texto; *q; q++) if (*q == '\n') *q = ' ';
      s.altura = contem(texto, "4320") || token(texto, "8k") ? 4320 :
                 contem(texto, "2160") || token(texto, "4k") || token(texto, "uhd") ? 2160 :
                 contem(texto, "1440") ? 1440 : contem(texto, "1080") ? 1080 :
                 contem(texto, "720") ? 720 : contem(texto, "480") ? 480 : 0;
      s.dolbyVision = token(texto, "dv") || token(texto, "dovi") ||
                      contem(texto, "dolby vision") || contem(texto, "dolbyvision");
      s.dolbyAtmos = token(texto, "atmos");
      s.badges = badges_detectar(texto);
      // "FHD"/"Full HD" (#402): formatadores tipo AIOStreams poem a resolucao
      // so como sigla no name ("FHD | REMUX | SDR"). Vale so na FALTA de
      // resolucao escrita em qualquer campo, e so do name: descricao e
      // filename trazem URL ("https://fhd...") e grupo de release ("x265-FHD").
      // E nunca ao lado de outro selo de resolucao que o badges_detectar ja deu.
      if (!s.altura && fhd &&
          !(s.badges & (badges_bit("r-4k") | badges_bit("r-1080") | badges_bit("r-720")))) {
        s.altura = 1080;
        s.badges |= badges_bit("r-1080");
      }
      s.mp4 = token(texto, "mp4") || contem(nv_media_url(s.url), ".mp4");
      // "av1" tambem casa "av1.0.0" (nivel/perfil, comum em nome de release);
      // token() basta porque nao ha AV1 escrito colado a outra palavra nos
      // nomes que os addons mandam (ao contrario de "dv", que precisa do
      // cuidado de token() com DVDRip — o mesmo cuidado ja se aplica aqui).
      s.av1 = token(texto, "av1") || contem(texto, "av01");
      s.foraCache = stream_texto_fora_de_cache(texto);
      // Cached by the add-on's own mark (clientResolve.isCached:true or "⚡" in
      // the name): only the full source name can veto it. A mark found only in the
      // description/title never overrides a negative status there. A "⏳" age, "download]" or "uncached" deep
      // in the description/filename (AIOStreams + TorBox) marked every cached
      // stream "not cached", and the verified auto pick dropped all of them.
      if (s.foraCache && strlen(nomeFonte) < sizeof nomeFonte - 1 &&
          (cache > 0 || (cache == 0 && textoEmCache(nomeFonte))) &&
          !stream_texto_fora_de_cache(nomeFonte)) s.foraCache = 0;
      // AIOStreams: clientResolve.isCached e explicito; ausente nao e false.
      if (s.infoHash[0] && !s.url[0] && cache < 0) s.foraCache = 1;
      if (s.foraCache) s.emCache = 0;
      if (s.infoHash[0]) s.temSemeadores = lerSemeadores(texto, &s.semeadores);
      s.tamanhoBytes = bytesExatos(p, fim);
      double bytes = js_num(p, fim, "videoSize", 0);
      if (s.tamanhoBytes) s.tamanhoMB = tamanhoMB((double)s.tamanhoBytes / 1048576.0);
      else if (bytes > 0) s.tamanhoMB = tamanhoMB(bytes / (1024.0 * 1024.0));
      else {
        const char *u = strstr(texto, " GB");
        double escala = 1024;
        if (!u) { u = strstr(texto, " MB"); escala = 1; }
        if (u) {
          const char *ini = u;
          while (ini > texto && (isdigit((unsigned char)ini[-1]) || ini[-1] == '.')) ini--;
          if (ini < u) s.tamanhoMB = tamanhoMB(atof(ini) * escala);
        }
      }
      if (n == cap) {
        int nova = cap ? cap * 2 : 32;
        Stream *tmp = realloc(v, (size_t)nova * sizeof *tmp);
        if (!tmp) { free(v); return -1; }
        v = tmp; cap = nova;
      }
      v[n++] = s;
    }
    p = js_prox(fim);
  }
  *saida = v;
  return n;
}
