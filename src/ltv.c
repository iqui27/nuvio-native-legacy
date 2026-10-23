// Live TV por fonte — ver ltv.h.
#include "ltv.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <pthread.h>

// --- arena ----------------------------------------------------------------------
struct LtvBloco { LtvBloco *prox; size_t cap; char d[]; };

const char *ltv_dupn(LtvArena *a, const char *s, size_t n) {
  char *r;
  if (!s || !n) return "";
  if (!a->atual || a->usado + n + 1 > a->atual->cap) {
    size_t cap = n + 1 > LTV_BLOCO ? n + 1 : LTV_BLOCO;
    LtvBloco *b = malloc(sizeof *b + cap);
    if (!b) return "";
    b->prox = NULL; b->cap = cap;
    if (a->atual) a->atual->prox = b; else a->prim = b;
    a->atual = b; a->usado = 0; a->total += cap;
  }
  r = a->atual->d + a->usado;
  memcpy(r, s, n); r[n] = 0;
  a->usado += n + 1;
  return r;
}
const char *ltv_dup(LtvArena *a, const char *s) { return ltv_dupn(a, s, s ? strlen(s) : 0); }
void ltv_arena_soltar(LtvArena *a) {
  LtvBloco *b = a->prim;
  while (b) { LtvBloco *p = b->prox; free(b); b = p; }
  memset(a, 0, sizeof *a);
}
size_t ltv_arena_bytes(const LtvArena *a) { return a->total; }

// --- fontes ---------------------------------------------------------------------
unsigned long ltv_hash(const char *s) {
  unsigned long h = 2166136261UL;
  for (; s && *s; s++) { h ^= (unsigned char)*s; h *= 16777619UL; h &= 0xffffffffUL; }
  return h;
}
void ltv_chave_fonte(int tipo, const char *ref, char *dst, size_t n) {
  if (!n) return;
  switch (tipo) {
    case LTV_XTREAM:  snprintf(dst, n, "xt"); break;
    case LTV_STALKER: snprintf(dst, n, "st"); break;
    case LTV_M3U:     snprintf(dst, n, "m%08lx", ltv_hash(ref)); break;
    default:          snprintf(dst, n, "a%08lx", ltv_hash(ref)); break;
  }
}
const char *ltv_tipo_nome(int tipo) {
  return tipo == LTV_XTREAM ? "xtream" : tipo == LTV_STALKER ? "stalker"
       : tipo == LTV_M3U ? "m3u" : "addon";
}

// Os numeros do plano (4.2), conta e nao medicao em TV: ~250 B por canal na
// arena + o GCanal; 30 mil canais ~ 9 MB. O .wgt vive no heap WASM.
int ltv_teto_canais(int plat, long memMB) {
  int t = plat == 1 ? 8000 : plat == 2 ? 20000 : 30000;
  if (memMB > 0 && memMB < 1200 && t > 5000) t = 5000;
  return t;
}

// --- grupos -----------------------------------------------------------------------
static int escolhido(const char *nome, const char *const *sel, int nsel) {
  int i;
  for (i = 0; i < nsel; i++) if (sel[i] && !strcmp(sel[i], nome)) return 1;
  return 0;
}
int ltv_grupos_admitir(const int *nPorGrupo, const char *const *nomes, int ng,
                       const char *const *sel, int nsel, int cota,
                       unsigned char *entra) {
  int i, soma = 0;
  if (cota < 0) cota = 0;
  for (i = 0; i < ng; i++) entra[i] = 0;
  if (nsel > 0) {
    // A ESCOLHA DA PESSOA MANDA, mas o teto continua sendo de memoria: os
    // escolhidos entram na ordem do provedor ate a cota.
    for (i = 0; i < ng; i++)
      if (escolhido(nomes[i], sel, nsel) && soma + nPorGrupo[i] <= cota) {
        entra[i] = 1; soma += nPorGrupo[i];
      }
    if (soma > 0) return soma;
    // Escolha que nao casa com nada (o provedor renomeou tudo): o padrao.
  }
  for (i = 0; i < ng; i++) {
    if (soma + nPorGrupo[i] <= cota) { entra[i] = 1; soma += nPorGrupo[i]; }
    else if (soma == 0 && cota > 0) { entra[i] = 1; soma = cota; break; }
  }
  return soma;
}

// --- numeracao --------------------------------------------------------------------
int ltv_numero_achar(const LtvNum *c, int n, int numero, int filtro, int *ambiguo) {
  int i, melhor = -1, fontes[16], nf = 0;
  if (ambiguo) *ambiguo = 0;
  if (!c || n <= 0 || numero <= 0) return -1;
  for (i = 0; i < n; i++) {
    int k;
    if (c[i].numero != numero) continue;
    for (k = 0; k < nf && fontes[k] != c[i].fonte; k++) {}
    if (k == nf && nf < 16) fontes[nf++] = c[i].fonte;
  }
  if (ambiguo) *ambiguo = nf > 1;
  if (filtro >= 0) {
    for (i = 0; i < n; i++) if (c[i].fonte == filtro && c[i].numero == numero) return i;
    return -1;
  }
  for (i = 0; i < n; i++) if (c[i].fav && c[i].numero == numero) return i;
  for (i = 0; i < n; i++)
    if (c[i].numero == numero && (melhor < 0 || c[i].ordemFonte < c[melhor].ordemFonte))
      melhor = i;
  return melhor;
}

int ltv_dig_apertar(LtvDigitos *g, int digito, unsigned agora) {
  size_t n = strlen(g->d);
  if (digito < 0 || digito > 9) return 0;
  if (!n && digito == 0) return 0;
  if (n >= LTV_DIG_MAX) return 0;
  g->d[n] = (char)('0' + digito); g->d[n + 1] = 0;
  g->desde = agora ? agora : 1;
  return 1;
}
int ltv_dig_ativo(const LtvDigitos *g) { return g->d[0] != 0; }
int ltv_dig_confirmar(LtvDigitos *g, int *numero) {
  if (!g->d[0]) return 0;
  if (numero) *numero = atoi(g->d);
  ltv_dig_limpar(g);
  return 1;
}
int ltv_dig_pronto(LtvDigitos *g, unsigned agora, int *numero) {
  if (!g->d[0] || agora - g->desde < LTV_DIG_MS) return 0;
  return ltv_dig_confirmar(g, numero);
}
void ltv_dig_limpar(LtvDigitos *g) { g->d[0] = 0; g->desde = 0; }

// --- favoritos ----------------------------------------------------------------------
int ltv_fav_linha(const char *linha, char *chave, size_t nc, char *id, size_t ni) {
  const char *tab, *fim;
  size_t k;
  if (nc) chave[0] = 0;
  if (ni) id[0] = 0;
  if (!linha) return 0;
  while (*linha == ' ' || *linha == '\r' || *linha == '\n') linha++;
  fim = linha + strcspn(linha, "\r\n");
  if (fim == linha) return 0;
  tab = memchr(linha, '\t', (size_t)(fim - linha));
  if (tab) {
    k = (size_t)(tab - linha);
    if (k >= nc) return 0;
    memcpy(chave, linha, k); chave[k] = 0;
    linha = tab + 1;
  }
  k = (size_t)(fim - linha);
  if (!k || k >= ni) return 0;
  memcpy(id, linha, k); id[k] = 0;
  return 1;
}

// --- preferencias --------------------------------------------------------------------
// Uma linha por fonte: chave, depois campos "nome=valor" separados por TAB; os
// grupos escolhidos vao no fim, como "g=<nome>" repetido (o nome de grupo pode
// ter qualquer coisa menos TAB e quebra de linha).
int ltv_pref_de_texto(const char *t, LtvPref *out, int max) {
  int n = 0;
  while (t && *t && n < max) {
    const char *fim = t + strcspn(t, "\n");
    LtvPref p;
    const char *c = t;
    memset(&p, 0, sizeof p);
    p.modo = -1; p.proxy = -1;
    { size_t k = strcspn(c, "\t\n");
      if (k && k < sizeof p.chave) { memcpy(p.chave, c, k); p.chave[k] = 0; }
      c += k; }
    while (c < fim && *c == '\t') {
      const char *v; size_t k;
      c++;
      k = strcspn(c, "\t\n");
      v = memchr(c, '=', k);
      if (v) {
        char nome[8] = "", val[256];
        size_t kn = (size_t)(v - c), kv = k - kn - 1;
        if (kn < sizeof nome) { memcpy(nome, c, kn); nome[kn] = 0; }
        if (kv >= sizeof val) kv = sizeof val - 1;
        memcpy(val, v + 1, kv); val[kv] = 0;
        if (!strcmp(nome, "az")) p.az = atoi(val);
        else if (!strcmp(nome, "tz")) p.fusoMin = atoi(val);
        else if (!strcmp(nome, "fmt")) p.formato = atoi(val);
        else if (!strcmp(nome, "modo")) p.modo = atoi(val);
        else if (!strcmp(nome, "proxy")) p.proxy = atoi(val);
        else if (!strcmp(nome, "g") && val[0]) {
          size_t u = strlen(p.grupos);
          if (u + kv + 2 < sizeof p.grupos) {
            if (u) p.grupos[u++] = '\n';
            memcpy(p.grupos + u, val, kv + 1);
          }
        }
      }
      c += k;
    }
    if (p.fusoMin < -720) p.fusoMin = -720;
    if (p.fusoMin > 840) p.fusoMin = 840;
    if (p.chave[0]) out[n++] = p;
    t = *fim ? fim + 1 : fim;
  }
  return n;
}

size_t ltv_pref_texto(const LtvPref *p, int n, char *dst, size_t tam) {
  size_t u = 0;
  int i;
  if (!tam) return 0;
  dst[0] = 0;
  for (i = 0; i < n; i++) {
    const char *g = p[i].grupos;
    int w = snprintf(dst + u, tam - u, "%s\taz=%d\ttz=%d\tfmt=%d\tmodo=%d\tproxy=%d",
                     p[i].chave, p[i].az, p[i].fusoMin, p[i].formato, p[i].modo, p[i].proxy);
    if (w < 0 || (size_t)w >= tam - u) break;
    u += (size_t)w;
    while (*g) {
      size_t k = strcspn(g, "\n");
      if (u + k + 4 >= tam) break;
      memcpy(dst + u, "\tg=", 3); u += 3;
      memcpy(dst + u, g, k); u += k;
      g += k; if (*g) g++;
    }
    if (u + 2 >= tam) break;
    dst[u++] = '\n'; dst[u] = 0;
  }
  return u;
}

static LtvPref prefs[LTV_PREF_MAX];
static int nPrefs, prefPerfil = -1;
static void prefNome(char *d, size_t n, int perfil) {
  if (perfil <= 0) snprintf(d, n, "%s", "ltv-fontes.txt"); // i18n: internal filename
  else snprintf(d, n, "ltv-fontes-p%d.txt", perfil); // i18n: internal filename
}
void ltv_pref_carregar(int perfil) {
  char nome[40], *t;
  prefNome(nome, sizeof nome, perfil);
  prefPerfil = perfil;
  t = dados_ler(nome);
  nPrefs = t ? ltv_pref_de_texto(t, prefs, LTV_PREF_MAX) : 0;
  free(t);
}
LtvPref *ltv_pref(const char *chave, int criar) {
  int i;
  if (!chave || !chave[0]) return NULL;
  for (i = 0; i < nPrefs; i++) if (!strcmp(prefs[i].chave, chave)) return &prefs[i];
  if (!criar) return NULL;
  if (nPrefs >= LTV_PREF_MAX) {
    // A mais antiga sai (fonte que a pessoa tirou faz tempo).
    memmove(prefs, prefs + 1, sizeof prefs[0] * (LTV_PREF_MAX - 1));
    nPrefs--;
  }
  memset(&prefs[nPrefs], 0, sizeof prefs[0]);
  snprintf(prefs[nPrefs].chave, sizeof prefs[nPrefs].chave, "%s", chave);
  prefs[nPrefs].modo = -1; prefs[nPrefs].proxy = -1;
  return &prefs[nPrefs++];
}
void ltv_pref_gravar(void) {
  static char buf[LTV_PREF_MAX * 2300];
  char nome[40];
  prefNome(nome, sizeof nome, prefPerfil);
  ltv_pref_texto(prefs, nPrefs, buf, sizeof buf);
  dados_gravar(nome, buf);
}
int ltv_pref_grupos(const LtvPref *p, const char **v, int max) {
  // Os nomes ficam num buffer estatico por fonte para nao exigir que o chamador
  // copie: a linha com '\n' vira strings separadas numa copia.
  static char copia[LTV_PREF_MAX][2048];
  int idx = (int)(p - prefs), n = 0;
  char *s;
  if (!p || idx < 0 || idx >= LTV_PREF_MAX || !p->grupos[0]) return 0;
  snprintf(copia[idx], sizeof copia[idx], "%s", p->grupos);
  s = copia[idx];
  while (*s && n < max) {
    char *f = strchr(s, '\n');
    v[n++] = s;
    if (!f) break;
    *f = 0; s = f + 1;
  }
  return n;
}
void ltv_pref_definir_grupos(LtvPref *p, const char *const *v, int n) {
  size_t u = 0;
  int i;
  if (!p) return;
  p->grupos[0] = 0;
  for (i = 0; i < n; i++) {
    size_t k = strlen(v[i]);
    if (!k || strchr(v[i], '\t') || strchr(v[i], '\n')) continue;
    if (u + k + 2 >= sizeof p->grupos) break;
    if (u) p->grupos[u++] = '\n';
    memcpy(p->grupos + u, v[i], k + 1); u += k;
  }
}

// --- M3U ------------------------------------------------------------------------------
int ltv_m3u_atributo(const char *linha, const char *nome, char *dst, size_t n) {
  size_t k = strlen(nome);
  const char *p = linha;
  if (n) dst[0] = 0;
  while ((p = strchr(p, '='))) {
    const char *ini = p;
    while (ini > linha && (isalnum((unsigned char)ini[-1]) || ini[-1] == '-' || ini[-1] == '_')) ini--;
    if ((size_t)(p - ini) == k && !strncasecmp(ini, nome, k) && p[1] == '"') {
      const char *v = p + 2, *f = strchr(v, '"');
      size_t m;
      if (!f) return 0;
      m = (size_t)(f - v);
      if (m >= n) m = n - 1;
      memcpy(dst, v, m); dst[m] = 0;
      return 1;
    }
    p++;
  }
  return 0;
}

void ltv_m3u_iniciar(LtvM3u *p, LtvM3uCb cb, void *u) {
  memset(p, 0, sizeof *p);
  p->cb = cb; p->u = u;
}

// O nome do canal e o que vem depois da PRIMEIRA virgula fora de aspas: os
// atributos nao tem virgula solta, e o nome pode ter ("ESPN, Brasil").
static const char *nomeExtinf(const char *l) {
  const char *r = NULL;
  int aspas = 0;
  for (; *l && !r; l++) {
    if (*l == '"') aspas = !aspas;
    else if (*l == ',' && !aspas) r = l + 1;
  }
  while (r && (*r == ' ' || *r == '\t')) r++;
  return r ? r : "";
}

static void aparar(char *s) {
  size_t n = strlen(s);
  while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) s[--n] = 0;
}

// "|User-Agent=x&Referer=y" no fim da URL (molde do Kodi): vira cabecalho.
static void sufixoCab(char *url, char *ua, size_t nua, char *ref, size_t nref) {
  char *bar = strchr(url, '|'), *q;
  if (!bar) return;
  *bar++ = 0;
  q = bar;
  while (*q) {
    char *e = strchr(q, '&'), *v;
    if (e) *e = 0;
    v = strchr(q, '=');
    if (v) {
      *v++ = 0;
      if (!strcasecmp(q, "user-agent")) snprintf(ua, nua, "%s", v);
      else if (!strcasecmp(q, "referer") || !strcasecmp(q, "referrer")) snprintf(ref, nref, "%s", v);
    }
    if (!e) break;
    q = e + 1;
  }
}

static int linhaM3u(LtvM3u *p, char *l) {
  while (*l == ' ' || *l == '\t' || *l == '\xef' || *l == '\xbb' || *l == '\xbf') l++;
  aparar(l);
  if (!*l) return 1;
  p->nLinhas++;
  if (!strncmp(l, "#EXTM3U", 7)) {
    char u[1024];
    p->cabecalho = 1;
    if (!p->urlTvg[0] && (ltv_m3u_atributo(l, "url-tvg", u, sizeof u) ||
                          ltv_m3u_atributo(l, "x-tvg-url", u, sizeof u))) {
      u[strcspn(u, ", ")] = 0;   // varias URLs separadas por virgula: a primeira
      snprintf(p->urlTvg, sizeof p->urlTvg, "%s", u);
    }
    return 1;
  }
  if (!strncmp(l, "#EXTINF", 7)) {
    snprintf(p->ext, sizeof p->ext, "%s", l);
    p->temExt = 1; p->opUa[0] = p->opRef[0] = 0;
    return 1;
  }
  if (!strncasecmp(l, "#EXTVLCOPT:", 11)) {
    const char *o = l + 11;
    if (!strncasecmp(o, "http-user-agent=", 16)) snprintf(p->opUa, sizeof p->opUa, "%s", o + 16);
    else if (!strncasecmp(o, "http-referrer=", 14)) snprintf(p->opRef, sizeof p->opRef, "%s", o + 14);
    else if (!strncasecmp(o, "http-referer=", 13)) snprintf(p->opRef, sizeof p->opRef, "%s", o + 13);
    return 1;
  }
  if (!strncasecmp(l, "#EXTGRP:", 8) && p->temExt) {
    char g[200];
    if (!ltv_m3u_atributo(p->ext, "group-title", g, sizeof g)) {
      size_t u = strlen(p->ext);
      snprintf(p->ext + u, sizeof p->ext - u, " group-title=\"%s\"", l + 8);
    }
    return 1;
  }
  if (*l == '#') return 1;
  if (!p->temExt) return 1;              // URL sem #EXTINF: nao ha nome
  { LtvM3uCanal c;
    char id[160], nm[200], logo[600], grp[200], chno[16], cu[32], cus[600], cud[16];
    char ua[256], ref[256], url[LTV_M3U_LINHA];
    snprintf(url, sizeof url, "%s", l);
    snprintf(ua, sizeof ua, "%s", p->opUa);
    snprintf(ref, sizeof ref, "%s", p->opRef);
    sufixoCab(url, ua, sizeof ua, ref, sizeof ref);
    memset(&c, 0, sizeof c);
    ltv_m3u_atributo(p->ext, "tvg-id", id, sizeof id);
    ltv_m3u_atributo(p->ext, "tvg-name", nm, sizeof nm);
    ltv_m3u_atributo(p->ext, "tvg-logo", logo, sizeof logo);
    ltv_m3u_atributo(p->ext, "group-title", grp, sizeof grp);
    if (!ltv_m3u_atributo(p->ext, "tvg-chno", chno, sizeof chno))
      ltv_m3u_atributo(p->ext, "channel-number", chno, sizeof chno);
    ltv_m3u_atributo(p->ext, "catchup", cu, sizeof cu);
    ltv_m3u_atributo(p->ext, "catchup-source", cus, sizeof cus);
    ltv_m3u_atributo(p->ext, "catchup-days", cud, sizeof cud);
    c.nome = nomeExtinf(p->ext);
    if (!c.nome[0]) c.nome = nm[0] ? nm : id;
    c.url = url; c.tvgId = id; c.tvgNome = nm; c.logo = logo; c.grupo = grp;
    c.ua = ua; c.referer = ref; c.catchup = cu; c.catchupSrc = cus;
    c.chno = atoi(chno); c.catchupDias = atoi(cud);
    p->temExt = 0;
    if (!c.nome[0] || !url[0]) return 1;
    p->nCanais++;
    if (p->cb && !p->cb(&c, p->u)) { p->parou = 1; return 0; }
  }
  return 1;
}

int ltv_m3u_alimentar(LtvM3u *p, const char *buf, size_t n) {
  size_t i;
  if (p->parou) return 0;
  for (i = 0; i < n; i++) {
    char ch = buf[i];
    if (ch == '\n' || ch == '\r') {
      if (!p->linhaGrande) {
        p->linha[p->nl] = 0;
        if (!linhaM3u(p, p->linha)) { p->nl = 0; return 0; }
      }
      p->nl = 0; p->linhaGrande = 0;
      continue;
    }
    if (p->linhaGrande) continue;
    if (p->nl + 1 >= sizeof p->linha) { p->linhaGrande = 1; p->nl = 0; continue; }
    p->linha[p->nl++] = ch;
  }
  return 1;
}

void ltv_m3u_fim(LtvM3u *p) {
  if (p->nl && !p->linhaGrande && !p->parou) {
    p->linha[p->nl] = 0;
    linhaM3u(p, p->linha);
  }
  p->nl = 0;
}

// --- fontes por URL --------------------------------------------------------------------
static LtvUrl urls[LTV_URLS_MAX];
static int nUrls, urlsPerfil = -1;
static pthread_mutex_t travaUrls = PTHREAD_MUTEX_INITIALIZER;

int ltv_urls_de_texto(const char *t, LtvUrl *out, int max) {
  int n = 0;
  while (t && *t && n < max) {
    const char *fim = t + strcspn(t, "\n");
    const char *a = memchr(t, '\t', (size_t)(fim - t));
    const char *b = a ? memchr(a + 1, '\t', (size_t)(fim - a - 1)) : NULL;
    if (a && b) {
      LtvUrl u;
      size_t k;
      memset(&u, 0, sizeof u);
      u.tipo = !strncmp(t, "xmltv", 5) ? LTV_URL_XMLTV : LTV_URL_M3U;
      k = (size_t)(b - a - 1); if (k >= sizeof u.nome) k = sizeof u.nome - 1;
      memcpy(u.nome, a + 1, k); u.nome[k] = 0;
      k = (size_t)(fim - b - 1); if (k && b[k] == '\r') k--;
      if (k && k < sizeof u.url) { memcpy(u.url, b + 1, k); u.url[k] = 0; out[n++] = u; }
    }
    t = *fim ? fim + 1 : fim;
  }
  return n;
}
size_t ltv_urls_texto(const LtvUrl *v, int n, char *dst, size_t tam) {
  size_t u = 0;
  int i;
  if (tam) dst[0] = 0;
  for (i = 0; i < n; i++) {
    int w = snprintf(dst + u, tam - u, "%s\t%s\t%s\n", v[i].tipo == LTV_URL_XMLTV ? "xmltv" : "m3u",
                     v[i].nome, v[i].url);
    if (w < 0 || (size_t)w >= tam - u) break;
    u += (size_t)w;
  }
  return u;
}
static void urlsNome(char *d, size_t n, int perfil) {
  if (perfil <= 0) snprintf(d, n, "ltv-urls.txt"); else snprintf(d, n, "ltv-urls-p%d.txt", perfil);
}
void ltv_urls_carregar(int perfil) {
  char nome[40], *t;
  urlsNome(nome, sizeof nome, perfil);
  t = dados_ler(nome);
  pthread_mutex_lock(&travaUrls);
  urlsPerfil = perfil;
  nUrls = t ? ltv_urls_de_texto(t, urls, LTV_URLS_MAX) : 0;
  pthread_mutex_unlock(&travaUrls);
  free(t);
}
static void urlsGravar(void) {
  char nome[40], buf[LTV_URLS_MAX * 1200];
  urlsNome(nome, sizeof nome, urlsPerfil);
  ltv_urls_texto(urls, nUrls, buf, sizeof buf);
  dados_gravar(nome, buf);
}
int ltv_urls_copiar(LtvUrl *out, int max) {
  int n;
  pthread_mutex_lock(&travaUrls);
  n = nUrls < max ? nUrls : max;
  if (n > 0) memcpy(out, urls, sizeof *out * (size_t)n);
  pthread_mutex_unlock(&travaUrls);
  return n;
}
int ltv_urls_adicionar(int tipo, const char *nome, const char *url) {
  int i;
  size_t k;
  if (!url) return -1;
  while (*url == ' ') url++;
  k = strlen(url);
  while (k && (url[k - 1] == ' ' || url[k - 1] == '\n' || url[k - 1] == '\r')) k--;
  if (!k || k >= sizeof urls[0].url || strchr(url, '\t')) return -1;
  if (strncasecmp(url, "http://", 7) && strncasecmp(url, "https://", 8)) return -1;
  pthread_mutex_lock(&travaUrls);
  for (i = 0; i < nUrls; i++)
    if (urls[i].tipo == tipo && !strncmp(urls[i].url, url, k) && !urls[i].url[k]) break;
  if (i == nUrls) {
    if (nUrls >= LTV_URLS_MAX) { pthread_mutex_unlock(&travaUrls); return -1; }
    nUrls++;
  }
  urls[i].tipo = tipo;
  memcpy(urls[i].url, url, k); urls[i].url[k] = 0;
  { char host[64];
    ltv_url_host(urls[i].url, host, sizeof host);
    snprintf(urls[i].nome, sizeof urls[i].nome, "%s", nome && nome[0] && !strchr(nome, '\t') ? nome : host); }
  urlsGravar();
  pthread_mutex_unlock(&travaUrls);
  return i;
}
void ltv_urls_remover(int i) {
  pthread_mutex_lock(&travaUrls);
  if (i >= 0 && i < nUrls) {
    memmove(&urls[i], &urls[i + 1], sizeof urls[0] * (size_t)(nUrls - i - 1));
    nUrls--;
    urlsGravar();
  }
  pthread_mutex_unlock(&travaUrls);
}
void ltv_url_host(const char *url, char *dst, size_t n) {
  const char *p = url, *a, *f;
  size_t k;
  if (!n) return;
  dst[0] = 0;
  if (!url) return;
  if ((a = strstr(p, "://"))) p = a + 3;
  f = p + strcspn(p, "/?#");
  a = memchr(p, '@', (size_t)(f - p));     // usuario:senha@host
  if (a) p = a + 1;
  k = (size_t)(f - p);
  if (k >= n) k = n - 1;
  memcpy(dst, p, k); dst[k] = 0;
}
