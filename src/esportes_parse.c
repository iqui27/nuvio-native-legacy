#include "esportes.h"
#include "js.h"
#include "buscanorm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const EspModalidade modalidades[] = {
  {"soccer", "Futebol"}, {"football", "Futebol americano"},
  {"basketball", "Basquete"}, {"baseball", "Beisebol"}, {"hockey", "Hóquei"},
  {"tennis", "Tênis"}, {"golf", "Golfe"}, {"racing", "Automobilismo"},
  {"mma", "MMA"}, {"rugby", "Rugby"},
  {"rugby-league", "Rugby league"}, {"volleyball", "Vôlei"},
  {"lacrosse", "Lacrosse"}, {"field-hockey", "Hóquei em campo"},
  {"water-polo", "Polo aquático"}, {"australian-football", "Futebol australiano"}
};
int esportes_modalidades_n(void) { return (int)(sizeof modalidades / sizeof *modalidades); }
const EspModalidade *esportes_modalidade(int i) {
  return i >= 0 && i < esportes_modalidades_n() ? &modalidades[i] : NULL;
}
static int slug(const char *s) {
  if (!s || !*s) return 0;
  for (; *s; s++) if (!isalnum((unsigned char)*s) && *s != '-' && *s != '.' && *s != '_') return 0;
  return 1;
}
static const char *fimString(const char *p, const char *fim) {
  for (p++; p < fim; p++) {
    if (*p == '\\') { if (p + 1 < fim) p++; }
    else if (*p == '\"') return p + 1;
  }
  return fim;
}
static const char *valor(const char *p, const char *fim, const char *key) {
  if (!p) return NULL;
  if (!fim) fim = p + strlen(p);
  if (p >= fim || *p++ != '{') return NULL;
  while (p < fim) {
    while (p < fim && (isspace((unsigned char)*p) || *p == ',')) p++;
    if (p >= fim || *p != '\"') return NULL;
    const char *k = p, *ke = fimString(k, fim); p = ke;
    while (p < fim && isspace((unsigned char)*p)) p++;
    if (p >= fim || *p++ != ':') return NULL;
    while (p < fim && isspace((unsigned char)*p)) p++;
    if (p >= fim) return NULL;
    if ((size_t)(ke - k) == strlen(key) + 2 && !memcmp(k + 1, key, strlen(key))) return p;
    if (*p == '{' || *p == '[') { const char *e = js_fim(p); if (!e || e <= p || e > fim) return NULL; p = e; }
    else if (*p == '\"') p = fimString(p, fim);
    else { while (p < fim && *p != ',' && *p != '}') p++; }
  }
  return NULL;
}
static const char *objeto(const char *p, const char *fim, const char *key) {
  const char *v = valor(p, fim, key); return v && *v == '{' ? v : NULL;
}
static const char *array(const char *p, const char *fim, const char *key) {
  const char *v = valor(p, fim, key);
  if (!v || *v++ != '[') return NULL;
  while (*v && isspace((unsigned char)*v)) v++;
  return *v && *v != ']' ? v : NULL;
}
static void texto(const char *p, const char *fim, const char *key, char *dst, size_t n) {
  const char *v = valor(p, fim, key); dst[0] = 0;
  if (v && *v == '\"') (void)js_cadeia(v, dst, n);
}
/* Prefer the provider's dark-background variant when it supplies one. */
static void imagem(const char *p,const char *fim,char *dst,size_t n) {
  for (const char *a=array(p,fim,"logos");a;) {
    const char *end=js_fim(a); char url[512];
    if (!end || end<=a) break;
    texto(a,end,"href",url,sizeof url);
    const char *rel=valor(a,end,"rel");
    int escuro=0;
    if (rel && *rel=='[') { const char *re=js_fim(rel),*d=strstr(rel,"\"dark\"");escuro=d && re && d<re; }
    if (url[0] && (!dst[0] || escuro)) snprintf(dst,n,"%s",url);
    if (escuro && url[0]) break;
    a=js_prox(end);
  }
}
int esportes_ligas_ler(const char *json, EspLiga *out, int cap) {
  const char *p = array(json, NULL, "leagues"); int n = 0;
  for (; p && n < cap; ) {
    const char *end = js_fim(p); EspLiga l = {0};
    if (!end || end <= p) break;
    texto(p, end, "slug", l.id, sizeof l.id); texto(p, end, "name", l.nome, sizeof l.nome);
    imagem(p,end,l.logo,sizeof l.logo);
    if (slug(l.id) && l.nome[0]) out[n++] = l;
    p = js_prox(end);
  }
  return n;
}
static void estado(const char *p, const char *fim, EspEvento *e) {
  const char *s = objeto(p, fim, "status");
  const char *t = s ? objeto(s, js_fim(s), "type") : NULL;
  char v[40] = "", name[80] = "";
  if (!t) return;
  texto(t, js_fim(t), "state", v, sizeof v);
  texto(t, js_fim(t), "name", name, sizeof name);
  texto(t, js_fim(t), "shortDetail", e->detalhe, sizeof e->detalhe);
  if (!e->detalhe[0]) texto(t, js_fim(t), "description", e->detalhe, sizeof e->detalhe);
  e->estado = !strcmp(v, "in") ? 1 : !strcmp(v, "post") ? 2 : 0;
  if (strstr(name, "POSTPONED") || strstr(name, "CANCELED") || strstr(name, "CANCELLED") || strstr(name, "SUSPENDED")) e->estado = 3;
}
static void competicao(const char *p, const char *fim, EspEvento *e) {
  const char *a; char date[64] = "";
  texto(p, fim, "date", date, sizeof date); if (date[0]) e->inicio = (time_t)(js_ms_iso(date) / 1000);
  texto(p, fim, "endDate", date, sizeof date); if (date[0]) e->fim = (time_t)(js_ms_iso(date) / 1000);
  estado(p, fim, e);
  a = objeto(p, fim, "venue");
  if (a) texto(a, js_fim(a), "fullName", e->local, sizeof e->local);
  a = objeto(p, fim, "type");
  if (a) texto(a, js_fim(a), "abbreviation", e->sessao, sizeof e->sessao);
  for (a = array(p, fim, "competitors"); a && e->nParticipantes < 2; ) {
    const char *end = js_fim(a); if (!end) break;
    const char *t = objeto(a, end, "team");
    if (!t) t = objeto(a, end, "athlete");
    if (t) {
      EspParticipante *v = &e->participantes[e->nParticipantes];
      texto(t, js_fim(t), "id", v->id, sizeof v->id);
      if (!v->id[0]) texto(a, end, "id", v->id, sizeof v->id);
      texto(t, js_fim(t), "displayName", v->nome, sizeof v->nome);
      texto(t, js_fim(t), "shortDisplayName", v->curto, sizeof v->curto);
      if (!v->curto[0]) texto(t, js_fim(t), "shortName", v->curto, sizeof v->curto);
      texto(t, js_fim(t), "abbreviation", v->sigla, sizeof v->sigla);
      texto(t,js_fim(t),"logoDark",v->logo,sizeof v->logo);
      if (!v->logo[0]) texto(t,js_fim(t),"logo",v->logo,sizeof v->logo);
      if (!v->logo[0]) imagem(t,js_fim(t),v->logo,sizeof v->logo);
      if (!v->logo[0]) {
        const char *img=objeto(t,js_fim(t),"headshot");
        if (!img) img=objeto(t,js_fim(t),"flag");
        if (img) texto(img,js_fim(img),"href",v->logo,sizeof v->logo);
      }
      texto(a, end, "score", v->pontos, sizeof v->pontos);
      if (!v->pontos[0]) {
        const char *score = objeto(a,end,"score");
        if (score) texto(score,js_fim(score),"displayValue",v->pontos,sizeof v->pontos);
      }
      if (!v->pontos[0]) {
        size_t usado=0;
        for (const char *ls=array(a,end,"linescores"); ls && usado+5<sizeof v->pontos; ) {
          const char *le=js_fim(ls); if (!le || le<=ls) break;
          double val=js_num(ls,le,"value",-1);
          if (val>=0 && val<=999) usado+=(size_t)snprintf(v->pontos+usado,sizeof v->pontos-usado,"%s%.0f",usado?" ":"",val);
          ls=js_prox(le);
        }
      }
      if (v->nome[0]) e->nParticipantes++;
    }
    a = js_prox(end);
  }
  for (a = array(p, fim, "broadcasts"); a && e->nEmissoras < ESP_MAX_EMISSORAS; ) {
    const char *end = js_fim(a), *b; if (!end) break;
    for (b = array(a, end, "names"); b && e->nEmissoras < ESP_MAX_EMISSORAS; ) {
      if (*b != '"') break;
      if (js_cadeia(b, e->emissoras[e->nEmissoras], sizeof e->emissoras[0])) e->nEmissoras++;
      const char *z = b + 1;
      while (z < end && *z && (*z != '"' || z[-1] == '\\')) z++;
      b = z < end ? js_prox(z + 1) : NULL;
    }
    a = js_prox(end);
  }
}
static void acrescentar(const char *p, const char *fim, const char *comp,
                        const char *mod, const EspLiga *l, EspEvento *out, int *n, int cap) {
  if (*n >= cap) return;
  EspEvento e = {0}; char parent[48] = "", cid[48] = "";
  texto(p, fim, "id", parent, sizeof parent);
  texto(p, fim, "name", e.nome, sizeof e.nome);
  snprintf(e.modalidade, sizeof e.modalidade, "%s", mod);
  snprintf(e.liga, sizeof e.liga, "%s", l->nome);
  snprintf(e.ligaId, sizeof e.ligaId, "%s", l->id);
  const char *le=objeto(p,fim,"league");
  if (le) {
    char id[80],nome[160];texto(le,js_fim(le),"slug",id,sizeof id);texto(le,js_fim(le),"name",nome,sizeof nome);
    if (slug(id)) snprintf(e.ligaId,sizeof e.ligaId,"%s",id);
    if (nome[0]) snprintf(e.liga,sizeof e.liga,"%s",nome);
  }
  competicao(p, fim, &e);
  if (comp) {
    e.nParticipantes = e.nEmissoras = 0;
    if (strcmp(mod, "golf")) e.fim = 0;
    competicao(comp, js_fim(comp), &e);
    texto(comp, js_fim(comp), "id", cid, sizeof cid);
  }
  snprintf(e.id, sizeof e.id, "%s%s%s", parent, cid[0] && strcmp(cid, parent) ? ":" : "", cid[0] && strcmp(cid, parent) ? cid : "");
  if (e.nParticipantes == 2 && (!strcmp(mod, "tennis") || !strcmp(mod, "mma")))
    snprintf(e.nome, sizeof e.nome, "%s × %s", e.participantes[0].nome, e.participantes[1].nome);
  if (!strcmp(mod, "racing") || !strcmp(mod, "golf")) e.nParticipantes = 0;
  if (e.id[0] && e.nome[0] && e.inicio > 0) out[(*n)++] = e;
}
int esportes_eventos_ler(const char *json, const char *mod, const EspLiga *l, EspEvento *out, int cap) {
  int n = 0;
  for (const char *p = array(json, NULL, "events"); p && n < cap; ) {
    const char *end = js_fim(p); if (!end) break;
    const char *c = array(p, end, "competitions");
    if (!strcmp(mod, "tennis")) {
      for (const char *g = array(p, end, "groupings"); g && n < cap; ) {
        const char *ge = js_fim(g); if (!ge) break;
        for (c = array(g, ge, "competitions"); c && n < cap; ) {
          const char *ce = js_fim(c); if (!ce) break;
          acrescentar(p, end, c, mod, l, out, &n, cap); c = js_prox(ce);
        }
        g = js_prox(ge);
      }
    } else if (c) {
      do { const char *ce = js_fim(c); if (!ce) break;
        acrescentar(p, end, c, mod, l, out, &n, cap);
        c = js_prox(ce);
      } while (c && n < cap && (!strcmp(mod, "racing") || !strcmp(mod, "mma")));
    } else acrescentar(p, end, NULL, mod, l, out, &n, cap);
    p = js_prox(end);
  }
  return n;
}
static void normalizar(const char *s, char *out, size_t n) {
  char tmp[800]; busca_normalizar(s ? s : "", tmp, sizeof tmp);
  size_t w = 0; int space = 1;
  for (size_t i = 0; tmp[i] && w + 1 < n; i++) {
    unsigned char c = (unsigned char)tmp[i];
    if (c < 128 && !isalnum(c)) { if (!space) out[w++] = ' '; space = 1; }
    else { out[w++] = (char)c; space = 0; }
  }
  if (w && out[w-1] == ' ') w--;
  out[w] = 0;
}
static int frase(const char *hay, const char *name, int min) {
  char key[800]; normalizar(name, key, sizeof key);
  if ((int)strlen(key) < min) return 0;
  const char *p = hay;
  while ((p = strstr(p, key))) {
    if ((p == hay || p[-1] == ' ') && (!p[strlen(key)] || p[strlen(key)] == ' ')) return 1;
    p++;
  }
  return 0;
}
/* Two-letter codes (KC, SF, NE, GB) are too ambiguous as loose words.
 * Accept them only as an explicit pair of this event's opponent codes. */
static int dueloSiglas(const EspEvento *e,const char *titulo) {
  char a[40],b[40],raw[800],expanded[1600],norm[1600],par[100];
  normalizar(e->participantes[0].sigla,a,sizeof a);
  normalizar(e->participantes[1].sigla,b,sizeof b);
  if(strlen(a)<2 || strlen(b)<2 || !strcmp(a,b) || (strlen(a)!=2 && strlen(b)!=2))return 0;
  busca_normalizar(titulo?titulo:"",raw,sizeof raw);
  size_t w=0;
  for(size_t i=0;raw[i] && w+5<sizeof expanded;i++) {
    if(raw[i]=='@'){memcpy(expanded+w," vs ",4);w+=4;}
    else expanded[w++]=raw[i];
  }
  expanded[w]=0;normalizar(expanded,norm,sizeof norm);
  static const char *seps[]={" vs "," v "," at "," x "," versus "};
  for(size_t i=0;i<sizeof seps/sizeof *seps;i++) {
    snprintf(par,sizeof par,"%s%s%s",a,seps[i],b);if(frase(norm,par,5))return 1;
    snprintf(par,sizeof par,"%s%s%s",b,seps[i],a);if(frase(norm,par,5))return 1;
  }
  return 0;
}
int esportes_corresponde(const EspEvento *e, const char *titulo, time_t ini, time_t fim) {
  char norm[800];
  if (!e || !e->inicio || !ini || fim <= ini || e->estado == 3) return 0;
  /* EPG may include a pre-game build-up, but yesterday's replay or a nearby
   * unrelated fixture must not match merely because it names one team. */
  time_t ate = e->fim > e->inicio ? e->fim : e->inicio + 6 * 3600;
  if (fim <= e->inicio - 30 * 60 || ini > ate || ini < e->inicio - 3 * 3600) return 0;
  normalizar(titulo, norm, sizeof norm);
  static const struct { const char *palavra, *modalidade; } rotulos[] = {
    {"NBA","basketball"},{"WNBA","basketball"},{"basketball","basketball"},
    {"NFL","football"},{"college football","football"},
    {"NHL","hockey"},{"ice hockey","hockey"},
    {"MLB","baseball"},{"baseball","baseball"},{"soccer","soccer"}
  };
  for (size_t i=0;i<sizeof rotulos/sizeof *rotulos;i++)
    if (frase(norm,rotulos[i].palavra,3) && strcmp(e->modalidade,rotulos[i].modalidade)) return 0;
  char league[200]; normalizar(e->liga,league,sizeof league);
  if ((strstr(norm,"women") || strstr(norm,"feminino")) && !strstr(league,"women") && !strstr(league,"feminino")) return 0;
  if (strstr(norm, "replay") || strstr(norm, "highlights") || strstr(norm, "reprise")) return 0;
  if (e->nParticipantes == 2) {
    int ambos=1;
    for (int i = 0; i < 2; i++) {
      const EspParticipante *v = &e->participantes[i];
      if (!frase(norm, v->nome, 4) && !frase(norm, v->curto, 4) && !frase(norm, v->sigla, 3)) ambos=0;
    }
    return ambos || dueloSiglas(e,titulo)?100:0;
  }
  if (!frase(norm, e->nome, 8)) return 0;
  if (!strcmp(e->modalidade, "racing") && e->sessao[0]) {
    if (!strcmp(e->sessao, "FP1") && !frase(norm,"FP1",3) && !frase(norm,"practice 1",8) && !frase(norm,"treino 1",7)) return 0;
    if (!strcmp(e->sessao, "FP2") && !frase(norm,"FP2",3) && !frase(norm,"practice 2",8) && !frase(norm,"treino 2",7)) return 0;
    if (!strcmp(e->sessao, "Q") && !frase(norm,"qualifying",8) && !frase(norm,"qualificacao",8) && !frase(norm,"classificacao",8)) return 0;
    if ((!strcmp(e->sessao, "R") || !strcmp(e->sessao, "Race")) && (strstr(norm,"practice") || strstr(norm,"qualifying") || strstr(norm,"sprint") || strstr(norm,"treino") || strstr(norm,"classificacao"))) return 0;
    if (!strcmp(e->sessao, "FP3") && !frase(norm,"FP3",3) && !frase(norm,"practice 3",8) && !frase(norm,"treino 3",7)) return 0;
  }
  return 90;
}
int esportes_emissora(const EspEvento *e, const char *nome) {
  char norm[800]; normalizar(nome, norm, sizeof norm);
  for (int i = 0; e && i < e->nEmissoras; i++)
    if (frase(norm, e->emissoras[i], 3)) return 1;
  return 0;
}
