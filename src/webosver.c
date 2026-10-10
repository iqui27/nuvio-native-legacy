#include "webosver.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
#ifdef NV_WEBOS
static char caminhoNyx[256] = "/var/run/nyx/os_info.json";
static char caminhoStarfish[256] = "/etc/starfish-release";
#else
static char caminhoNyx[256], caminhoStarfish[256];   // vazio = nao le (so o hook)
#endif
static int lido, major;
static const char *fonte = "-";
static char linhaStarfish[128];

// "N.N" + sufixo opcional ('.', '-' ou '_' e o que vier). N entre 1 e 999.
static int versaoMaior(const char *s) {
  long n = 0; int d = 0;
  while (*s >= '0' && *s <= '9') { if (d < 4) n = n * 10 + (*s - '0'); d++; s++; }
  if (d < 1 || d > 3 || *s != '.') return 0;
  s++;
  if (!(*s >= '0' && *s <= '9')) return 0;
  while (*s >= '0' && *s <= '9') s++;
  if (*s && *s != '.' && *s != '-' && *s != '_') return 0;
  return n >= 1 ? (int)n : 0;
}

static const char *pula(const char *p) {
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  return p;
}
// p em cima da aspa de abertura. Devolve logo depois da aspa final (NULL se nao
// fecha); *ini/*fim = conteudo, *esc = teve barra.
static const char *string(const char *p, const char **ini, const char **fim, int *esc) {
  *esc = 0; *ini = ++p;
  for (; *p; p++) {
    if (*p == '\\') { *esc = 1; if (!*++p) return NULL; }
    else if (*p == '"') { *fim = p; return p + 1; }
    else if ((unsigned char)*p < 0x20) return NULL;
  }
  return NULL;
}
static int digito(char c) { return c >= '0' && c <= '9'; }
// Primitivo JSON: true, false, null ou numero valido. Devolve depois dele ou NULL.
static const char *primitivo(const char *p) {
  static const char *const lit[] = { "true", "false", "null" };
  for (int i = 0; i < 3; i++) {
    size_t n = strlen(lit[i]);
    if (!strncmp(p, lit[i], n) && !((p[n] >= 'a' && p[n] <= 'z') || digito(p[n]))) return p + n;
  }
  if (*p == '-') p++;
  if (*p == '0') p++;
  else if (digito(*p)) while (digito(*p)) p++;
  else return NULL;
  if (*p == '.') { if (!digito(*++p)) return NULL; while (digito(*p)) p++; }
  if (*p == 'e' || *p == 'E') {
    p++; if (*p == '+' || *p == '-') p++;
    if (!digito(*p)) return NULL;
    while (digito(*p)) p++;
  }
  return p;
}
// Valida e pula um valor JSON inteiro (p ja sem espaco). Cada fechador tem de
// ser do tipo do abridor; fundo maximo 16 (alem disso: invalido).
static const char *valor(const char *p, int prof) {
  if (*p == '"') { const char *a, *b; int e; return string(p, &a, &b, &e); }
  if (*p == '{' || *p == '[') {
    char fecha = *p == '{' ? '}' : ']';
    if (prof >= 16) return NULL;
    p = pula(p + 1);
    if (*p == fecha) return p + 1;
    for (;;) {
      if (fecha == '}') {
        const char *a, *b; int e;
        if (*p != '"' || !(p = string(p, &a, &b, &e))) return NULL;
        p = pula(p);
        if (*p++ != ':') return NULL;
        p = pula(p);
      }
      if (!(p = valor(p, prof + 1))) return NULL;
      p = pula(p);
      if (*p == ',') { p = pula(p + 1); continue; }
      return *p == fecha ? p + 1 : NULL;
    }
  }
  return primitivo(p);
}

int nv_webos_parse_nyx(const char *json) {
  static const char chave[] = "webos_release";
  const char *p;
  int achou = 0, v = 0;
  if (!json) return 0;
  p = pula(json);
  if (*p++ != '{') return 0;
  p = pula(p);
  if (*p == '}') return 0;
  for (;;) {
    const char *ki, *kf, *vi, *vf;
    int ke, ve, casa, vv = 0;
    if (*p != '"' || !(p = string(p, &ki, &kf, &ke))) return 0;
    // Chave com barra (\u005f etc.) nao e decodificada: o documento todo vira
    // desconhecido, para uma duplicata escrita com escape nunca passar batida.
    if (ke) return 0;
    casa = (size_t)(kf - ki) == sizeof chave - 1 && !memcmp(ki, chave, sizeof chave - 1);
    p = pula(p);
    if (*p++ != ':') return 0;
    p = pula(p);
    if (*p == '"') {
      if (!(p = string(p, &vi, &vf, &ve))) return 0;
      if (casa && !ve && vf - vi < 48) {
        char b[48];
        memcpy(b, vi, (size_t)(vf - vi)); b[vf - vi] = 0;
        vv = versaoMaior(b);
      }
    } else if (!(p = valor(p, 1))) return 0;
    if (casa) {
      if (achou && vv != v) return 0;     // duplicata que nao concorda
      achou = 1; v = vv;
    }
    p = pula(p);
    if (*p == ',') { p = pula(p + 1); continue; }
    if (*p++ != '}') return 0;
    break;
  }
  if (*pula(p)) return 0;                  // sobra depois do objeto
  return achou ? v : 0;
}

int nv_webos_parse_starfish(const char *texto, char *linha, size_t cap) {
  const char *l = texto;
  int v = 0;
  if (linha && cap) linha[0] = 0;
  while (l && *l) {
    size_t n = strcspn(l, "\r\n");
    char um[256];
    if (n < sizeof um) {                   // linha longa demais pode cortar o numero: ignora
      const char *r = um;
      memcpy(um, l, n); um[n] = 0;
      if (linha && cap && !linha[0]) snprintf(linha, cap, "%s", um);
      while (!v && (r = strstr(r, "release "))) {
        const char *s = r + 8;
        int d = 0; long m = 0;
        int borda = r == um || !((r[-1] >= 'a' && r[-1] <= 'z') || (r[-1] >= 'A' && r[-1] <= 'Z') ||
                                 (r[-1] >= '0' && r[-1] <= '9') || r[-1] == '_');
        while (*s >= '0' && *s <= '9') { if (d < 4) m = m * 10 + (*s - '0'); d++; s++; }
        if (borda && d >= 1 && d <= 3 && m > 0 &&
            (!*s || *s == '.' || *s == '-' || *s == ' ' || *s == '\t' || *s == '_'))
          v = (int)m;
        r += 8;
      }
    } else if (linha && cap && !linha[0]) snprintf(linha, cap, "%.*s", (int)(cap - 1), l);
    l += n;
    while (*l == '\r' || *l == '\n') l++;
  }
  return v;
}

// Le o arquivo inteiro (ate cap-1). Devolve o tamanho; -1 = erro de E/S; 0 =
// nao existe/vazio. EINTR repete.
static long ler(const char *caminho, char *buf, size_t cap) {
  FILE *f;
  size_t n = 0;
  int tentativas = 0;
  if (!caminho[0]) return 0;
  f = fopen(caminho, "rb");
  if (!f) return 0;
  for (;;) {
    n += fread(buf + n, 1, cap - 1 - n, f);
    if (ferror(f)) {
      if (errno == EINTR && ++tentativas < 4) { clearerr(f); continue; }
      fclose(f); buf[0] = 0; return -1;
    }
    break;
  }
  buf[n] = 0;
  fclose(f);
  return (long)n;
}

// Chamada com a trava. Devolve 0 se a leitura deu erro (resultado nao guardado).
static int resolver(void) {
  char buf[NV_WEBOS_LEITURA];
  long n;
  int v = 0, erro = 0;
  fonte = "-"; major = 0; linhaStarfish[0] = 0;
  n = ler(caminhoNyx, buf, sizeof buf);
  if (n < 0) erro = 1;
  else if (n && n < (long)sizeof buf - 1) v = nv_webos_parse_nyx(buf);  // no limite = cortado
  if (v) { major = v; fonte = "nyx"; }
  n = ler(caminhoStarfish, buf, sizeof buf);
  if (n < 0) erro = 1;
  else if (n) {
    v = nv_webos_parse_starfish(buf, linhaStarfish, sizeof linhaStarfish);
    if (!major && v) { major = v; fonte = "starfish"; }
  }
  // O nyx manda: com ele lido, um erro no starfish (fonte de reserva) so perde
  // a linha do log. Sem versao e com erro, nada e guardado e a proxima
  // pergunta tenta de novo.
  if (erro && !(major && !strcmp(fonte, "nyx"))) { major = 0; fonte = "-"; return 0; }
  lido = 1;
  return 1;
}

int nv_webos_major(void) {
  int v;
  pthread_mutex_lock(&trava);
  if (!lido) resolver();
  v = major;
  pthread_mutex_unlock(&trava);
  return v;
}
const char *nv_webos_fonte(void) {
  const char *f;
  pthread_mutex_lock(&trava);
  if (!lido) resolver();
  f = fonte;
  pthread_mutex_unlock(&trava);
  return f;
}
void nv_webos_starfish_linha(char *out, size_t cap) {
  if (!out || !cap) return;
  pthread_mutex_lock(&trava);
  if (!lido) resolver();
  snprintf(out, cap, "%s", linhaStarfish);
  pthread_mutex_unlock(&trava);
}

#ifdef AJUSTES_TESTE
void nv_webos_testar(const char *nyx, const char *starfish) {
  pthread_mutex_lock(&trava);
#ifdef NV_WEBOS
  snprintf(caminhoNyx, sizeof caminhoNyx, "%s", nyx ? nyx : "/var/run/nyx/os_info.json");
  snprintf(caminhoStarfish, sizeof caminhoStarfish, "%s", starfish ? starfish : "/etc/starfish-release");
#else
  snprintf(caminhoNyx, sizeof caminhoNyx, "%s", nyx ? nyx : "");
  snprintf(caminhoStarfish, sizeof caminhoStarfish, "%s", starfish ? starfish : "");
#endif
  lido = 0;
  pthread_mutex_unlock(&trava);
}
#endif
