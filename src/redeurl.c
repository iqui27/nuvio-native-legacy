#include "rede.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

// SO ESTA FUNCAO, e num arquivo proprio de proposito.
//
// Ela e a redacao de credencial dos logs (ver rede.h) e nao toca em rede
// nenhuma: nao ha socket, nao ha curl, nao ha dlopen, nao ha estado. O resto de
// rede.c tem tudo isso.
//
// O QUE ISSO CONSERTA: tests/debrid.sh finge o transporte (define os seus
// proprios rede_postar_st e rede_baixar_st) e por isso NAO PODE linkar rede.c —
// os simbolos colidiriam. Enquanto esta funcao morava la, o teste simplesmente
// nao linkava, e a alternativa obvia — um stub no teste — seria a pior das
// saidas: o unico lugar onde debrid.c esconde o segmento /d/<chave>/ do link do
// Real-Debrid passaria a ser codigo que o teste NAO exercita, e uma regressao
// que vazasse a chave no log sairia verde.
//
// Quem linka src/*.c (Mac, ARM, Tizen — os tres fazem glob) nao percebe
// diferenca: e a mesma funcao, no mesmo cabecalho.
const char *rede_url_publica(const char *url, char *dst, unsigned tam) {
  const char *e, *h, *at = NULL, *p;
  unsigned n;
  if (!dst || tam == 0) return "";
  dst[0] = 0;
  if (!url || !*url) return dst;
  e = strstr(url, "://");
  if (!e) { snprintf(dst, tam, "%.*s", (int)tam - 1, url); return dst; }
  h = e + 3;
  while (*h && *h != '/' && *h != '?' && *h != '#') h++;
  // A autoridade tambem pode trazer userinfo (comum em addons IPTV). O
  // caminho ja era cortado, mas deixar `usuario:senha@` antes do host ainda
  // vazaria uma credencial no log. Conservamos esquema, host e porta.
  for (p = e + 3; p < h; p++) if (*p == '@') at = p;
  { const char *inicio = at ? at + 1 : e + 3;
    n = (unsigned)(e + 3 - url);
    if (n >= tam) n = tam - 1;
    memcpy(dst, url, n);
    if (n < tam - 1) {
      unsigned restante = tam - 1 - n;
      unsigned autoridade = (unsigned)(h - inicio);
      if (autoridade > restante) autoridade = restante;
      memcpy(dst + n, inicio, autoridade);
      n += autoridade;
    }
    dst[n] = 0;
  }
  // O "/..." avisa que havia caminho: sem ele, um log com host nu parece um
  // pedido a raiz do servidor, que e uma leitura errada.
  if (*h && n + 4 < tam) { memcpy(dst + n, "/...", 4); dst[n + 4] = 0; }
  return dst;
}

// ---------------------------------------------------------------------------
// rede_url_log: ver rede.h. Regra:
//   - esquema + host (userinfo some) sempre ficam;
//   - query e fragmento viram "?<redigido>" (chave de API mora ali);
//   - em host que guarda a configuracao da pessoa NO CAMINHO (lista abaixo),
//     todo segmento vira <redigido>, menos um ultimo que seja id de titulo
//     (tt123.jpg) — o que diz QUAL cartaz falhou sem dizer de quem;
//   - em qualquer host, um segmento "suspeito" vira <redigido>: >= 24 chars,
//     uuid, comeco de JWT/JSON-base64 ("eyJ"), ou com = % : { | ~ , ;
//     (config serializada). Excecao so para o ULTIMO segmento quando ele tem
//     cara de arquivo de midia (radical [A-Za-z0-9_-] ate 40 + extensao
//     conhecida): o nome do arquivo do TMDB tem 27-32 chars e e o que diz
//     QUAL imagem falhou.
#define RL_RED "<redigido>"

static int rl_hostChaveNoCaminho(const char *h, unsigned n) {
  static const char *const sufixos[] = {
    "btttr.cc", "elfhosted.com", "elfhosted.cc", "ratingposterdb.com",
    "top-streaming.stream", "top-posters.com", "toposters.com", NULL };
  static const char *const pedacos[] = {
    "aiometadata", "aiopostr", "betterposter", "postersplus", "posters-plus",
    "top-poster", "toposter", "rpdb", NULL };
  int i;
  for (i = 0; sufixos[i]; i++) {
    unsigned k = (unsigned)strlen(sufixos[i]);
    if (n >= k && !strncasecmp(h + n - k, sufixos[i], k) &&
        (n == k || h[n - k - 1] == '.')) return 1;
  }
  for (i = 0; pedacos[i]; i++) {
    unsigned k = (unsigned)strlen(pedacos[i]), j;
    for (j = 0; j + k <= n; j++) if (!strncasecmp(h + j, pedacos[i], k)) return 1;
  }
  return 0;
}

static int rl_ehHex(char c) { return isxdigit((unsigned char)c) != 0; }

static int rl_uuid(const char *s, unsigned n) {
  unsigned i;
  if (n != 36) return 0;
  for (i = 0; i < 36; i++) {
    if (i == 8 || i == 13 || i == 18 || i == 23) { if (s[i] != '-') return 0; }
    else if (!rl_ehHex(s[i])) return 0;
  }
  return 1;
}

static int rl_arquivoMidia(const char *s, unsigned n) {
  static const char *const ext[] = { "jpg", "jpeg", "png", "webp", "gif", "svg",
                                     "avif", "bmp", NULL };
  unsigned p, i;
  for (p = n; p > 0 && s[p - 1] != '.'; p--) {}
  if (p < 2 || p - 1 > 40) return 0;           // sem ponto, radical vazio ou longo
  for (i = 0; i + 1 < p; i++)
    if (!isalnum((unsigned char)s[i]) && s[i] != '_' && s[i] != '-') return 0;
  for (i = 0; ext[i]; i++)
    if (n - p == strlen(ext[i]) && !strncasecmp(s + p, ext[i], n - p)) return 1;
  return 0;
}

// Ultimo segmento num host com chave no caminho: so fica se for um id de
// titulo (tt123, 12345, tmdb-123, com ou sem extensao). Qualquer outra coisa
// pode ser a propria configuracao.
static int rl_idTitulo(const char *s, unsigned n) {
  unsigned i = 0, dig = 0;
  if (n >= 2 && s[0] == 't' && s[1] == 't') i = 2;
  else if (n >= 5 && !strncasecmp(s, "tmdb", 4) && (s[4] == '-' || s[4] == '_')) i = 5;
  for (; i < n && isdigit((unsigned char)s[i]); i++) dig++;
  if (!dig || dig > 12) return 0;
  if (i == n) return 1;
  if (s[i] != '.' || n - i - 1 < 2 || n - i - 1 > 5) return 0;
  for (i++; i < n; i++) if (!isalnum((unsigned char)s[i])) return 0;
  return 1;                                     // tt123.jpg, tt123.json
}

int rede_segmento_suspeito(const char *s, unsigned n) {
  unsigned i;
  if (n >= 24) return 1;
  if (rl_uuid(s, n)) return 1;
  if (n >= 3 && !strncmp(s, "eyJ", 3)) return 1;
  for (i = 0; i < n; i++)
    if (strchr("=%:{}|~,;", s[i])) return 1;
  return 0;
}

const char *rede_url_log(const char *url, char *dst, unsigned tam) {
  const char *e, *h, *a, *p, *fimHost;
  unsigned n = 0;
  int chaveNoCaminho;
  if (!dst || tam == 0) return "";
  dst[0] = 0;
  if (!url || !*url) return dst;
#define RL_POE(s, k) do { unsigned _k = (unsigned)(k); \
    if (_k > tam - 1 - n) { _k = tam - 1 - n; } memcpy(dst + n, (s), _k); n += _k; dst[n] = 0; } while (0)
  e = strstr(url, "://");
  if (!e) { snprintf(dst, tam, "%s", url); return dst; }
  h = e + 3;
  for (fimHost = h; *fimHost && *fimHost != '/' && *fimHost != '?' && *fimHost != '#'; fimHost++) {}
  for (a = h, p = h; p < fimHost; p++) if (*p == '@') a = p + 1;
  RL_POE(url, h - url);
  RL_POE(a, fimHost - a);
  chaveNoCaminho = rl_hostChaveNoCaminho(a, (unsigned)(fimHost - a));
  p = fimHost;
  while (*p == '/') {
    const char *s = p + 1, *f = s;
    int ultimo, red;
    while (*f && *f != '/' && *f != '?' && *f != '#') f++;
    ultimo = *f != '/';
    RL_POE("/", 1);
    if (f > s) {
      unsigned k = (unsigned)(f - s);
      if (chaveNoCaminho) red = !(ultimo && rl_idTitulo(s, k));
      else if (ultimo && rl_arquivoMidia(s, k)) red = 0;
      else red = rede_segmento_suspeito(s, k);
      if (red) RL_POE(RL_RED, sizeof RL_RED - 1);
      else RL_POE(s, k);
    }
    p = f;
  }
  if (*p == '?' || *p == '#') RL_POE("?" RL_RED, sizeof RL_RED);
#undef RL_POE
  return dst;
}
