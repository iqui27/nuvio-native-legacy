#include "homekit.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void hk_grupo_id(int g, char *dst, unsigned n) { snprintf(dst, n, "kit_home_%d", g); }

int hk_url_valida(const char *url) {
  const char *h;
  if (!url) return 0;
  if (!strncasecmp(url, "https://", 8)) h = url + 8;
  else if (!strncasecmp(url, "http://", 7)) h = url + 7;
  else return 0;
  if (!*h || *h == '/' || *h == '?' || *h == '#') return 0;
  for (; *url; url++) if ((unsigned char)*url <= ' ' || *url == '"' || *url == '\\') return 0;
  return 1;
}

// O documento tem de FECHAR: um download cortado no meio nao pode virar um kit
// com metade das fileiras. Anda respeitando aspas e escape; 1 so se o primeiro
// valor fecha e depois dele ha apenas espaco.
static int completo(const char *p) {
  int prof = 0, emTexto = 0;
  char ult = 0;  // ultimo sinal fora de texto: valor colado em valor e erro
  while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
  if (*p != '{' && *p != '[') return 0;
  for (; *p; p++) {
    if (emTexto) {
      if (*p == '\\') { if (!p[1]) return 0; p++; }
      else if (*p == '"') emTexto = 0;
      continue;
    }
    if (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') continue;
    // "{...}{...}" ou "a" "b": sem a virgula o leitor pararia no primeiro item
    // e aceitaria um kit pela metade.
    if ((*p == '"' || *p == '{' || *p == '[') && (ult == '"' || ult == '}' || ult == ']')) return 0;
    ult = *p;
    if (*p == '"') emTexto = 1;
    else if (*p == '{' || *p == '[') { if (++prof > 64) return 0; }
    else if (*p == '}' || *p == ']') {
      if (--prof == 0) {
        for (p++; *p; p++) if (*p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') return 0;
        return 1;
      }
    }
  }
  return 0;
}

// Arte: so http(s), e so se coube inteira (cheia ate o ultimo byte = cortada).
static void lerArte(const char *p, const char *pe, const char *chave, char *dst, size_t tam, HkKit *k) {
  dst[0] = 0;
  if (!js_texto(p, pe, chave, dst, tam) || !dst[0]) { dst[0] = 0; return; }
  if (strlen(dst) >= tam - 1 || !hk_url_valida(dst)) { dst[0] = 0; k->pulArte++; }
}

static int lerFonte(const char *s, const char *se, ColSource *a, HkKit *k,
                    int (*addon_instalado)(const char *id)) {
  char prov[16] = "";
  memset(a, 0, sizeof *a);
  js_texto(s, se, "provider", prov, sizeof prov);
  if (!strcasecmp(prov, "tmdb")) {
    js_texto(s, se, "title", a->title, sizeof a->title);
    js_texto(s, se, "tmdbSourceType", a->tmdbTipo, sizeof a->tmdbTipo);
    a->tmdbId = (long)js_num(s, se, "tmdbId", 0.0);
    js_texto(s, se, "mediaType", a->midia, sizeof a->midia);
    js_texto(s, se, "sortBy", a->ordenar, sizeof a->ordenar);
    if (!js_bruto(s, se, "filters", a->filtros, sizeof a->filtros) || a->filtros[0] != '{') a->filtros[0] = 0;
    if (a->tmdbId < 0) { k->pulFonte++; return 0; }
    if (!a->title[0]) snprintf(a->title, sizeof a->title, "%s", a->tmdbTipo[0] ? a->tmdbTipo : "TMDB");
    snprintf(a->prov, sizeof a->prov, "tmdb");
    return 1;
  }
  if (!strcasecmp(prov, "trakt")) {
    js_texto(s, se, "title", a->title, sizeof a->title);
    a->traktLista = (long)js_num(s, se, "traktListId", 0.0);
    js_texto(s, se, "mediaType", a->midia, sizeof a->midia);
    js_texto(s, se, "sortBy", a->ordenar, sizeof a->ordenar);
    js_texto(s, se, "sortHow", a->ordem, sizeof a->ordem);
    if (a->traktLista <= 0) { k->pulFonte++; return 0; }
    if (!a->title[0]) snprintf(a->title, sizeof a->title, "Lista Trakt");
    snprintf(a->prov, sizeof a->prov, "trakt");
    return 1;
  }
  if (prov[0] && strcasecmp(prov, "addon")) { k->pulFonte++; return 0; }
  // ADDON: so pelo id. addonBaseUrl do kit nao e lido de proposito (homekit.h).
  js_texto(s, se, "addonId", a->addonId, sizeof a->addonId);
  js_texto(s, se, "type", a->type, sizeof a->type);
  js_texto(s, se, "catalogId", a->catId, sizeof a->catId);
  if (!js_texto(s, se, "title", a->title, sizeof a->title)) snprintf(a->title, sizeof a->title, "%s", a->catId);
  js_texto(s, se, "genre", a->genre, sizeof a->genre);
  if (!strcmp(a->genre, "None")) a->genre[0] = 0;
  if (!a->type[0] || !a->catId[0]) { k->pulFonte++; return 0; }
  if (!a->addonId[0] || (addon_instalado && !addon_instalado(a->addonId))) { k->pulAddon++; return 0; }
  return 1;
}

static void lerColecao(const char *c, const char *ce, HkKit *k, int (*addon_instalado)(const char *id)) {
  char titulo[64] = "", fundo[512] = "";
  int g = k->nGrupos, entrou = 0;
  js_texto_raiz_em(c, ce, "title", titulo, sizeof titulo);
  if (!titulo[0]) {
    for (const char *p = js_array(c, ce, "folders"), *pe = NULL; p; p = js_prox(pe)) { pe = js_fim(p); k->pulPasta++; }
    return;
  }
  lerArte(c, ce, "backdropImageUrl", fundo, sizeof fundo, k);
  for (const char *p = js_array(c, ce, "folders"), *pe = NULL; p; p = js_prox(pe)) {
    ColFolder *v;
    ColSource *fontes;
    const char *src;
    char forma[16] = "", b[8];
    int i = k->nPastas;
    pe = js_fim(p);
    if (g >= HK_GRUPOS_MAX || i >= HK_PASTAS_MAX) { k->pulPasta++; continue; }
    v = &k->pastas[i];
    fontes = k->fontes[i];
    memset(v, 0, sizeof *v);
    v->sources = fontes;
    snprintf(v->group, sizeof v->group, "%s", titulo);
    hk_grupo_id(g, v->groupId, sizeof v->groupId);
    // O id e do APP, nao do kit: nunca colide com pasta da conta nem do pacote.
    snprintf(v->id, sizeof v->id, "kit_%d_%d", g, i);
    js_texto_raiz_em(p, pe, "title", v->title, sizeof v->title);
    lerArte(p, pe, "coverImageUrl", v->cover, sizeof v->cover, k);
    lerArte(p, pe, "heroBackdropUrl", v->hero, sizeof v->hero, k);
    lerArte(p, pe, "titleLogoUrl", v->logo, sizeof v->logo, k);
    snprintf(v->groupBackdrop, sizeof v->groupBackdrop, "%s", fundo);
    v->hideTitle = js_bruto(p, pe, "hideTitle", b, sizeof b) && strstr(b, "true") ? 1 : 0;
    if (!js_texto(p, pe, "tileShape", forma, sizeof forma)) js_texto(p, pe, "posterShape", forma, sizeof forma);
    v->forma = !strcasecmp(forma, "POSTER") ? COL_FORMA_POSTER
             : (!strcasecmp(forma, "LANDSCAPE") || !strcasecmp(forma, "WIDE")) ? COL_FORMA_PAISAGEM
             : COL_FORMA_QUADRADO;   // a regra de col_forma_texto, sem linkar colecoes.c
    src = js_array(p, pe, "sources");
    if (!src) src = js_array(p, pe, "catalogSources");
    for (const char *s = src, *se = NULL; s; s = js_prox(se)) {
      se = js_fim(s);
      if (v->nSources >= HK_FONTES_PASTA) { k->pulFonte++; continue; }
      if (lerFonte(s, se, &fontes[v->nSources], k, addon_instalado)) v->nSources++;
    }
    if (!v->title[0] || !v->nSources) { k->pulPasta++; continue; }
    k->grupoDe[i] = g;
    k->nPastas++;
    entrou = 1;
  }
  if (entrou) k->nGrupos = g + 1;
}

HkResultado hk_ler(const char *json, HkKit *k, int (*addon_instalado)(const char *id)) {
  const char *p, *col;
  if (!k) return HK_ERR_JSON;
  memset(k, 0, sizeof *k);
  if (!json) return HK_ERR_JSON;
  if (strlen(json) > HK_CORPO_MAX) return HK_ERR_GRANDE;
  if (!completo(json)) return HK_ERR_JSON;
  for (p = json; *p == ' ' || *p == '\n' || *p == '\r' || *p == '\t'; p++) {}
  if (*p == '[') col = js_raiz_array(p);
  else {
    if (js_tem(p, NULL, "nuvioKit") && js_num(p, NULL, "nuvioKit", 0.0) > HK_VERSAO) return HK_ERR_VERSAO;
    js_texto_raiz(p, "name", k->nome, sizeof k->nome);
    js_texto_raiz(p, "author", k->autor, sizeof k->autor);
    if (!js_array_presente(p, NULL, "collections")) return HK_ERR_JSON;
    col = js_array(p, NULL, "collections");
  }
  for (const char *c = col, *ce = NULL; c; c = js_prox(ce)) {
    ce = js_fim(c);
    if (*c == '{') lerColecao(c, ce, k, addon_instalado);
  }
  return k->nPastas ? HK_OK : HK_ERR_VAZIO;
}

// --- de volta para texto ---------------------------------------------------
typedef struct { char *p; size_t n, cap; int falhou; } Saida;
static void poe(Saida *o, const char *s, size_t n) {
  if (o->falhou) return;
  if (o->n + n + 1 > o->cap) {
    size_t c = o->cap ? o->cap * 2 : 4096;
    char *q;
    while (c < o->n + n + 1) c *= 2;
    q = realloc(o->p, c);
    if (!q) { o->falhou = 1; return; }
    o->p = q; o->cap = c;
  }
  memcpy(o->p + o->n, s, n);
  o->n += n;
  o->p[o->n] = 0;
}
static void poeS(Saida *o, const char *s) { poe(o, s, strlen(s)); }
static void poeTexto(Saida *o, const char *s) {
  poeS(o, "\"");
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    char e[8];
    if (c == '"' || c == '\\') { e[0] = '\\'; e[1] = (char)c; poe(o, e, 2); }
    else if (c < 0x20) { snprintf(e, sizeof e, "\\u%04x", c); poe(o, e, 6); }
    else poe(o, s, 1);
  }
  poeS(o, "\"");
}
// ,"chave":"valor" — so quando ha valor.
static void campo(Saida *o, const char *chave, const char *valor) {
  if (!valor || !valor[0]) return;
  poeS(o, ",\""); poeS(o, chave); poeS(o, "\":"); poeTexto(o, valor);
}
static void campoNum(Saida *o, const char *chave, long v) {
  char t[64];
  snprintf(t, sizeof t, ",\"%s\":%ld", chave, v);
  poeS(o, t);
}

// Os nomes que o web usa para a forma do tile, na ordem de fora para dentro.
static const char *const FORMA_WEB[3] = { "SQUARE", "POSTER", "LANDSCAPE" };
static const char *formaTexto(int forma) {
  return FORMA_WEB[forma == COL_FORMA_POSTER ? 1 : forma == COL_FORMA_PAISAGEM ? 2 : 0];
}

char *hk_serializar(const HkKit *k) {
  Saida o = { 0 };
  int g, i, s, primeiraCol = 1;
  char t[32];
  if (!k) return NULL;
  snprintf(t, sizeof t, "{\"nuvioKit\":%d", HK_VERSAO);
  poeS(&o, t);
  campo(&o, "name", k->nome);
  campo(&o, "author", k->autor);
  poeS(&o, ",\"collections\":[");
  for (g = 0; g < k->nGrupos; g++) {
    int primeiraPasta = 1;
    for (i = 0; i < k->nPastas; i++) {
      const ColFolder *v = &k->pastas[i];
      if (k->grupoDe[i] != g) continue;
      if (primeiraPasta) {
        poeS(&o, primeiraCol ? "{\"title\":" : ",{\"title\":");
        poeTexto(&o, v->group);
        campo(&o, "backdropImageUrl", v->groupBackdrop);
        poeS(&o, ",\"folders\":[");
        primeiraCol = 0;
      }
      poeS(&o, primeiraPasta ? "{\"title\":" : ",{\"title\":");
      primeiraPasta = 0;
      poeTexto(&o, v->title);
      campo(&o, "coverImageUrl", v->cover);
      campo(&o, "heroBackdropUrl", v->hero);
      campo(&o, "titleLogoUrl", v->logo);
      campo(&o, "tileShape", formaTexto(v->forma));
      if (v->hideTitle) poeS(&o, ",\"hideTitle\":true");
      poeS(&o, ",\"sources\":[");
      for (s = 0; s < v->nSources && s < HK_FONTES_PASTA; s++) {
        const ColSource *a = &k->fontes[i][s];
        poeS(&o, s ? ",{\"provider\":" : "{\"provider\":");
        poeTexto(&o, a->prov[0] ? a->prov : "addon");
        campo(&o, "title", a->title);
        if (!strcmp(a->prov, "tmdb")) {
          campo(&o, "tmdbSourceType", a->tmdbTipo);
          if (a->tmdbId) campoNum(&o, "tmdbId", a->tmdbId);
          campo(&o, "mediaType", a->midia);
          campo(&o, "sortBy", a->ordenar);
          if (a->filtros[0] == '{') { poeS(&o, ",\"filters\":"); poeS(&o, a->filtros); }
        } else if (!strcmp(a->prov, "trakt")) {
          campoNum(&o, "traktListId", a->traktLista);
          campo(&o, "mediaType", a->midia);
          campo(&o, "sortBy", a->ordenar);
          campo(&o, "sortHow", a->ordem);
        } else {
          campo(&o, "addonId", a->addonId);
          campo(&o, "type", a->type);
          campo(&o, "catalogId", a->catId);
          campo(&o, "genre", a->genre);
        }
        poeS(&o, "}");
      }
      poeS(&o, "]}");
    }
    if (!primeiraPasta) poeS(&o, "]}");
  }
  poeS(&o, "]}");
  if (o.falhou) { free(o.p); return NULL; }
  return o.p;
}
