#include "aiostreams.h"
#include "js.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char *value_end(const char *p) {
  if (*p == '"') {
    for (p++; *p; p++) {
      if (*p == '\\' && p[1]) p++;
      else if (*p == '"') return p+1;
    }
    return NULL;
  }
  if (*p == '{' || *p == '[') return js_fim(p);
  while (*p && !strchr(",}] \t\r\n", *p)) p++;
  return p;
}
// Root-only JSON member lookup. Never confuse nested credential/name fields.
static const char *member(const char *p, const char *key) {
  char k[128];
  if (!p || *p != '{') return NULL;
  p++;
  while (*p) {
    while (isspace((unsigned char)*p) || *p == ',') p++;
    if (*p != '"' || !js_cadeia(p, k, sizeof k)) return NULL;
    p++; while (*p && (*p != '"' || p[-1] == '\\')) p++;
    if (!*p++) return NULL;
    while (isspace((unsigned char)*p)) p++;
    if (*p++ != ':') return NULL;
    while (isspace((unsigned char)*p)) p++;
    if (!strcmp(k, key)) return p;
    const char *end = value_end(p);
    if (!end || end <= p) return NULL;
    p = end;
  }
  return NULL;
}
static int boolean(const char *p, const char *key, int fallback) {
  const char *v = member(p, key);
  return !v ? fallback : !strncmp(v, "true", 4) ? 1 : !strncmp(v, "false", 5) ? 0 : fallback;
}
static void safe(char *s) {
  // Remote labels may themselves contain signed links. Never render them.
  if (strstr(s, "://") || strstr(s, "Bearer ") || strstr(s, "apiKey")) s[0] = 0;
  for (char *p = s; *p; p++) if ((unsigned char)*p < 32) *p = ' ';
}
int aio_url(const char *url, char *origem, size_t n) {
  if (origem && n) origem[0] = 0;
  if (!url || strlen(url) >= 2048) return 0;
  size_t prefix = !strncmp(url, "https://", 8) ? 8 : !strncmp(url, "http://", 7) ? 7 : 0;
  if (!prefix) return 0;
  for (const unsigned char *p = (const unsigned char *)url; *p; p++)
    if (*p <= 32 || *p == 127 || *p == '\\') return 0;
  const char *end = url + prefix;
  while (*end && !strchr("/?#", *end)) end++;
  if (end == url + prefix || memchr(url + prefix, '@', end - url - prefix) ||
      memchr(url + prefix, '%', end - url - prefix)) return 0;
  size_t len = (size_t)(end - url);
  if (!origem || n <= len) return 0;
  memcpy(origem, url, len); origem[len] = 0;
  return 1;
}
int aio_autorizacao(const char *url, char *dst, size_t n) {
  char origin[256], credentials[1024];
  const char *p, *slash, *last;
  dst[0] = 0;
  if (!aio_url(url, origin, sizeof origin)) return 0;
  p = url + strlen(origin);
  if (strncmp(p, "/stremio/", 9)) return 0;
  p += 9; slash = strchr(p, '/');
  if (!slash || slash == p) return 0;
  last = strchr(slash + 1, '/');
  if (!last || last == slash + 1 || strcmp(last, "/manifest.json")) return 0;
  // Aliases/variants are preserved for manifest requests; never guess auth.
  for (const char *q = p; q < last; q++)
    if (*q != '/' && !isalnum((unsigned char)*q) && !strchr("-_=+", *q)) return 0;
  size_t len = last - p;
  if (len >= sizeof credentials || n < 22 + ((len + 2) / 3) * 4) return 0;
  memcpy(credentials, p, len); credentials[slash - p] = ':';
  static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  strcpy(dst, "Authorization: Basic "); size_t k = strlen(dst);
  for (size_t i = 0; i < len; i += 3) {
    unsigned a = (unsigned char)credentials[i], b = i+1 < len ? (unsigned char)credentials[i+1] : 0;
    unsigned c = i+2 < len ? (unsigned char)credentials[i+2] : 0;
    dst[k++] = alphabet[a >> 2]; dst[k++] = alphabet[((a & 3) << 4) | (b >> 4)];
    dst[k++] = i+1 < len ? alphabet[((b & 15) << 2) | (c >> 6)] : '=';
    dst[k++] = i+2 < len ? alphabet[c & 63] : '=';
  }
  dst[k] = 0; memset(credentials, 0, sizeof credentials); return 1;
}
int aio_manifesto(AioSnapshot *s, const char *json) {
  char id[128] = "", name[96] = "";
  s->valida = s->requerida = s->catalogos = s->recursos = 0;
  const char *end=json?js_fim(json):NULL;
  if (!json || *json != '{' || !end || end<=json || end[-1]!='}') return 0;
  for(const char *tail=end;*tail;tail++) if(!isspace((unsigned char)*tail)) return 0;
  js_texto_raiz(json, "id", id, sizeof id);
  js_texto_raiz(json, "name", name, sizeof name);
  if (strncasecmp(id, "aiostreams", 10) && strncasecmp(id,AIO_ID_PREFIX,sizeof AIO_ID_PREFIX-1) && strncasecmp(name, "AIOStreams", 10)) return 0;
  s->requerida = boolean(member(json, "behaviorHints"), "configurationRequired", 0);
  if (s->requerida) return 0;
  snprintf(s->nome, sizeof s->nome, "%s", name[0] ? name : "AIOStreams"); safe(s->nome);
  js_texto_raiz(json, "version", s->versao, sizeof s->versao); safe(s->versao);
  const char *a = member(json, "resources");
  const char *p = a && *a == '[' ? a + 1 : NULL;
  while (p) {
    while (isspace((unsigned char)*p) || *p == ',') p++;
    if (*p == ']') break;
    char r[32] = "";
    if (*p == '"') js_cadeia(p, r, sizeof r); else js_texto_raiz_em(p, js_fim(p), "name", r, sizeof r);
    if (!strcmp(r,"catalog")) s->recursos |= AIO_CATALOGO;
    if (!strcmp(r,"stream")) s->recursos |= AIO_STREAM;
    if (!strcmp(r,"subtitles")) s->recursos |= AIO_LEGENDA;
    if (!strcmp(r,"meta")) s->recursos |= AIO_META;
    if (!strcmp(r,"addon_catalog")) s->recursos |= AIO_ADDONS;
    const char *end = value_end(p); if (!end || end <= p) break; p = end;
  }
  a = member(json, "catalogs"); p = a && *a == '[' ? a+1 : NULL;
  while (p && s->catalogos < AIO_MAX) {
    while (isspace((unsigned char)*p) || *p == ',') p++;
    if (*p != '{') break;
    const char *end = js_fim(p); if (!end) break;
    AioCatalogo c = {0};
    js_texto_raiz_em(p,end,"id",c.id,sizeof c.id);
    js_texto_raiz_em(p,end,"name",c.nome,sizeof c.nome);
    js_texto_raiz_em(p,end,"type",c.tipo,sizeof c.tipo);
    safe(c.id); safe(c.nome); safe(c.tipo);
    if (c.id[0] && c.tipo[0]) { if (!c.nome[0]) strcpy(c.nome,c.id); s->catalogo[s->catalogos++] = c; }
    p = end;
  }
  s->valida = 1; return 1;
}
static int inventory(const char *u, const char *key, AioItem *v, int max) {
  const char *a = member(u,key), *p = a && *a == '[' ? a+1 : NULL; int n = 0;
  while (p && n < max) {
    while (isspace((unsigned char)*p) || *p == ',') p++;
    if (*p != '{') break;
    const char *end = js_fim(p); if (!end) break;
    AioItem item = {0}; item.ativo = boolean(p,"enabled",1);
    js_texto_raiz_em(p,end,"id",item.nome,sizeof item.nome);
    if (!item.nome[0]) js_texto_raiz_em(p,end,"type",item.nome,sizeof item.nome);
    safe(item.nome);
    // Attribution only from explicit preset resource restrictions, never names.
    const char *resources=member(member(p,"options"),"resources");
    if(resources) { char declared[160]=""; const char *q=resources+1;
      while(*q && *q!=']') { while(*q==',' || isspace((unsigned char)*q)) q++;
        if(*q=='"' && js_cadeia(q,declared,sizeof declared) && !strcmp(declared,"subtitles")) item.legenda=1;
        const char *next=value_end(q); if(!next || next<=q) break; q=next;
      }
    }
    if (item.nome[0]) v[n++] = item;
    p = end;
  }
  return n;
}
static void summarize(char *dst,size_t n,const char *p) {
  if(!p) return;
  const char *end=value_end(p); if(!end) return;
  if(*p=='[') {
    for(p++;p<end && *p && *p!=']';) {
      while(isspace((unsigned char)*p)||*p==',') p++;
      char text[80]="";
      if(*p=='"') js_cadeia(p,text,sizeof text);
      else if(*p=='{') js_texto_raiz_em(p,js_fim(p),"key",text,sizeof text);
      else if(isdigit((unsigned char)*p)) snprintf(text,sizeof text,"%.0f",strtod(p,NULL));
      safe(text);
      if(text[0]) { size_t used=strlen(dst); snprintf(dst+used,n-used,"%s%s",used?", ":"",text); }
      const char *next=value_end(p); if(!next || next<=p) break; p=next;
    }
  } else if(*p=='"') { js_cadeia(p,dst,n); safe(dst); }
}
void aio_configuracao(AioSnapshot *s, const char *json) {
  const char *u = member(member(json,"data"),"userData");
  s->configurada = s->servicos = s->addons = 0; memset(s->regras,0,sizeof s->regras);
  if (!u || *u != '{' || value_end(u)[-1]!='}') return;
  s->configurada = 1;
  s->servicos = inventory(u,"services",s->servico,32);
  s->addons = inventory(u,"presets",s->addon,AIO_MAX);
  // Counts summarize known schema fields; never copy custom expressions/keys.
  static const char *const keys[8][5] = {
    {"excludedResolutions","requiredResolutions","includedResolutions","preferredResolutions",NULL},
    {"size","bitrate","resultLimits",NULL},
    {"excludedLanguages","requiredLanguages","includedLanguages","preferredLanguages",NULL},
    {"excludeCached","excludeUncached","excludedStreamTypes","requiredStreamTypes",NULL},
    {"excludedRegexPatterns","requiredRegexPatterns","includedRegexPatterns","preferredRegexPatterns",NULL},
    {"sortCriteria",NULL},{"formatter",NULL},{"proxy",NULL}};
  for (int i=0;i<8;i++) for (int j=0;keys[i][j];j++) if (member(u,keys[i][j])) s->regras[i]++;
  memset(s->resumo,0,sizeof s->resumo);
  s->regraCache=boolean(u,"excludeCached",-1); s->regraUncached=boolean(u,"excludeUncached",-1);
  s->resolucaoObrigatoria[0]=s->resolucaoExcluida[0]=s->idiomaPreferido[0]=0;
  summarize(s->resolucaoObrigatoria,128,member(u,"requiredResolutions"));
  summarize(s->resolucaoExcluida,128,member(u,"excludedResolutions"));
  summarize(s->idiomaPreferido,128,member(u,"preferredLanguages"));
  summarize(s->resumo[0],256,member(u,"requiredResolutions"));
  if(!s->resumo[0][0]) summarize(s->resumo[0],256,member(u,"preferredResolutions"));
  if(!s->resumo[0][0]) summarize(s->resumo[0],256,member(u,"excludedResolutions"));
  summarize(s->resumo[2],256,member(u,"preferredLanguages"));
  summarize(s->resumo[5],256,member(member(u,"sortCriteria"),"global"));
  summarize(s->resumo[6],256,member(member(u,"formatter"),"id"));
  // Numeric ranges only; never render custom expressions or proxy addresses.
  const char *limits=member(member(u,"size"),"global");
  summarize(s->resumo[1],256,member(limits,"movies"));
}
void aio_resultados(AioSnapshot *s, const char *json) {
  const char *a = member(json,"streams"), *p = a && *a == '[' ? a+1 : NULL;
  s->resultados = s->links = s->p2p = 0;
  while (p && s->resultados < AIO_MAX) {
    while (isspace((unsigned char)*p) || *p == ',') p++;
    if (*p != '{') break;
    const char *end = js_fim(p); if (!end) break;
    AioResultado r = {0}; r.cache = -1;
    js_texto_raiz_em(p,end,"name",r.nome,sizeof r.nome); safe(r.nome);
    const char *kind = member(p,"url") ? "HTTP" : member(p,"infoHash") ? "P2P" :
      member(p,"externalUrl") ? "Externo" : member(p,"nzbUrl") ? "NZB" : member(p,"ytId") ? "YouTube" : "Desconhecido";
    snprintf(r.tipo,sizeof r.tipo,"%s",kind);
    if (!strcmp(kind,"P2P")) s->p2p++;
    if (!strcmp(kind,"Externo") || !strcmp(kind,"NZB") || !strcmp(kind,"YouTube")) s->links++;
    const char *sd = member(p,"streamData");
    r.cache = boolean(sd,"cached",-1);
    if(r.cache<0) r.cache=boolean(member(p,"clientResolve"),"isCached",-1);
    const char *parsed=member(sd,"parsedFile"), *resolution=member(parsed,"resolution");
    if(resolution && isdigit((unsigned char)*resolution)) r.altura=(int)strtol(resolution,NULL,10);
    s->resultado[s->resultados++] = r; p = end;
  }
}
