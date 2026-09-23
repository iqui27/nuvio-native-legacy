// Mapa do gosto — ver mapa.h. Tres partes, nesta ordem no arquivo:
//   1. leitura pura do JSON do TMDB (testada em tests/mapa.c);
//   2. o cruzamento, tambem puro;
//   3. o que tem estado: sementes do fio principal, o fio do TMDB e o cache.
#include "mapa.h"
#include "js.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef NV_MAPA_PURO
#include "catalogo.h"
#include "descoberta.h"
#include "progresso.h"
#include "salvos.h"
#include "dados.h"
#include "rede.h"
#include <pthread.h>
#endif

#define TMDB_IMG "https://image.tmdb.org/t/p/"

// ---------------------------------------------------------------------------
// 1. LEITURA

// Valor de uma chave de PROFUNDIDADE 1 do objeto que comeca em `ini`.
// js_texto/js_num param na primeira ocorrencia do nome, e o TMDB repete os
// nomes dentro de objetos aninhados: em /tv, "vote_average" de
// last_episode_to_air vem ANTES do da raiz, e "name" de created_by[] antes do
// titulo. Devolve o ponteiro para o primeiro caractere do valor.
static const char *valorRaiz(const char *ini, const char *fim, const char *chave) {
  const char *p = ini;
  size_t k = strlen(chave);
  int prof = 0;
  if (!p) return NULL;
  while (*p && (!fim || p < fim) && *p != '{') p++;
  if (!*p || (fim && p >= fim)) return NULL;
  for (; *p && (!fim || p < fim); p++) {
    char c = *p;
    if (c == '"') {
      const char *s = p + 1, *q = s;
      while (*q && *q != '"') { if (*q == '\\' && q[1]) q++; q++; }
      if (!*q) return NULL;
      if (prof == 1 && (size_t)(q - s) == k && !memcmp(s, chave, k)) {
        const char *v = q + 1;
        while (*v == ' ' || *v == '\n' || *v == '\r' || *v == '\t') v++;
        if (*v == ':') {
          v++;
          while (*v == ' ' || *v == '\n' || *v == '\r' || *v == '\t') v++;
          return v;
        }
      }
      p = q;
    } else if (c == '{' || c == '[') prof++;
    else if (c == '}' || c == ']') { prof--; if (prof <= 0) return NULL; }
  }
  return NULL;
}

static double numRaiz(const char *corpo, const char *chave, double padrao) {
  const char *v = valorRaiz(corpo, NULL, chave);
  if (!v || !(*v == '-' || (*v >= '0' && *v <= '9'))) return padrao;
  return atof(v);
}

// Copia texto e troca o que quebraria o cache (tab, fim de linha) por espaco.
static void copiaLimpa(char *dst, size_t n, const char *src) {
  size_t i;
  if (!dst || !n) return;
  snprintf(dst, n, "%s", src ? src : "");
  for (i = 0; dst[i]; i++)
    if (dst[i] == '\t' || dst[i] == '\n' || dst[i] == '\r') dst[i] = ' ';
}

static void urlImg(char *dst, size_t n, const char *tam, const char *caminho) {
  if (caminho && caminho[0] == '/') snprintf(dst, n, TMDB_IMG "%s%s", tam, caminho);
}

static int anoDe(const char *data) {
  if (!data || strlen(data) < 4) return 0;
  if (!isdigit((unsigned char)data[0])) return 0;
  return atoi(data) > 1870 ? atoi(data) : 0;
}

// Um elemento de recommendations.results / combined_credits.
static void lerObraLista(const char *p, const char *f, MapaObra *o, long *gen, int *nGen) {
  char t[160] = "", cam[128] = "", data[16] = "", mt[12] = "";
  memset(o, 0, sizeof *o);
  o->catIndice = -1;
  o->tmdb = (long)js_num(p, f, "id", 0.0);
  js_texto(p, f, "media_type", mt, sizeof mt);
  if (!js_texto(p, f, "title", t, sizeof t)) js_texto(p, f, "name", t, sizeof t);
  copiaLimpa(o->titulo, sizeof o->titulo, t);
  snprintf(o->tipo, sizeof o->tipo, "%s", !strcmp(mt, "tv") ? "series" : "movie");
  if (!mt[0] && !js_texto(p, f, "release_date", data, sizeof data) &&
      js_texto(p, f, "first_air_date", data, sizeof data))
    snprintf(o->tipo, sizeof o->tipo, "series");
  if (!data[0] && !js_texto(p, f, "release_date", data, sizeof data))
    js_texto(p, f, "first_air_date", data, sizeof data);
  o->ano = anoDe(data);
  if (js_texto(p, f, "poster_path", cam, sizeof cam)) urlImg(o->poster, sizeof o->poster, "w185", cam);
  cam[0] = 0;
  if (js_texto(p, f, "backdrop_path", cam, sizeof cam)) urlImg(o->fundo, sizeof o->fundo, "w780", cam);
  { char sin[600] = "";
    js_texto(p, f, "overview", sin, sizeof sin);
    copiaLimpa(o->sinopse, sizeof o->sinopse, sin); }
  o->nota = (int)(js_num(p, f, "vote_average", 0.0) * 10.0 + 0.5);
  o->votos = (int)js_num(p, f, "vote_count", 0.0);
  if (gen && nGen) {
    // js_array so entrega elementos objeto/texto; genre_ids e de numeros.
    const char *g = valorRaiz(p, f, "genre_ids");
    *nGen = 0;
    if (g && *g == '[') { g++; while (*g == ' ') g++; } else g = NULL;
    while (g && *nGen < MAPA_GEN_MAX && (*g == '-' || isdigit((unsigned char)*g))) {
      gen[(*nGen)++] = atol(g);
      while (*g && *g != ',' && *g != ']') g++;
      if (*g != ',') break;
      g++;
      while (*g == ' ') g++;
    }
  }
}

static void addPessoa(MapaSemente *s, long id, const char *nome, const char *foto, int dir) {
  int i;
  if (!nome || !nome[0] || s->nGente >= MAPA_GENTE_MAX) return;
  for (i = 0; i < s->nGente; i++) if (s->gente[i].id == id) return;
  s->gente[s->nGente].id = id;
  copiaLimpa(s->gente[s->nGente].nome, sizeof s->gente[0].nome, nome);
  s->gente[s->nGente].foto[0] = 0;
  urlImg(s->gente[s->nGente].foto, sizeof s->gente[0].foto, "w185", foto);
  s->gente[s->nGente].direcao = dir;
  s->nGente++;
}

int mapa_ler_detalhe(const char *json, int serie, MapaSemente *s) {
  char t[200] = "", cam[128] = "", data[16] = "", imdb[24] = "";
  const char *v, *fimV, *p;
  if (!json || !s || json[0] != '{') return 0;
  if (js_texto_raiz(json, serie ? "name" : "title", t, sizeof t) && t[0])
    copiaLimpa(s->obra.titulo, sizeof s->obra.titulo, t);
  if (js_texto_raiz(json, "poster_path", cam, sizeof cam))
    urlImg(s->obra.poster, sizeof s->obra.poster, "w185", cam);
  cam[0] = 0;
  if (js_texto_raiz(json, "backdrop_path", cam, sizeof cam))
    urlImg(s->obra.fundo, sizeof s->obra.fundo, "w780", cam);
  { char sin[900] = "";
    if (js_texto_raiz(json, "overview", sin, sizeof sin) && sin[0])
      copiaLimpa(s->obra.sinopse, sizeof s->obra.sinopse, sin); }
  if (js_texto_raiz(json, serie ? "first_air_date" : "release_date", data, sizeof data))
    if (anoDe(data)) s->obra.ano = anoDe(data);
  if (!serie && js_texto_raiz(json, "imdb_id", imdb, sizeof imdb) && imdb[0])
    snprintf(s->obra.imdb, sizeof s->obra.imdb, "%s", imdb);
  { double va = numRaiz(json, "vote_average", -1.0);
    if (va >= 0) s->obra.nota = (int)(va * 10.0 + 0.5);
    s->obra.votos = (int)numRaiz(json, "vote_count", (double)s->obra.votos); }
  { long id = (long)numRaiz(json, "id", 0.0); if (id > 0) s->obra.tmdb = id; }
  snprintf(s->obra.tipo, sizeof s->obra.tipo, "%s", serie ? "series" : "movie");

  // generos da raiz
  v = valorRaiz(json, NULL, "genres");
  if (v && *v == '[') {
    s->nGen = 0;
    fimV = js_fim(v);
    for (p = v + 1; p && p < fimV && *p; ) {
      while (*p && *p != '{' && *p != ']') p++;
      if (*p != '{') break;
      { const char *f = js_fim(p);
        if (s->nGen < MAPA_GEN_MAX) {
          s->gen[s->nGen].id = (long)js_num(p, f, "id", 0.0);
          js_texto(p, f, "name", s->gen[s->nGen].nome, sizeof s->gen[0].nome);
          if (s->gen[s->nGen].nome[0]) s->nGen++;
        }
        p = f ? f + 1 : NULL; }
    }
  }

  // keywords.keywords (filme) ou keywords.results (serie)
  v = valorRaiz(json, NULL, "keywords");
  if (v && *v == '{') {
    fimV = js_fim(v);
    p = js_array(v, fimV, serie ? "results" : "keywords");
    s->nKw = 0;
    while (p && p < fimV && s->nKw < MAPA_KW_MAX) {
      const char *f = js_fim(p);
      s->kw[s->nKw].id = (long)js_num(p, f, "id", 0.0);
      s->kw[s->nKw].nome[0] = 0;
      js_texto(p, f, "name", s->kw[s->nKw].nome, sizeof s->kw[0].nome);
      if (s->kw[s->nKw].nome[0]) s->nKw++;
      p = js_prox(f);
    }
  }

  // Direcao/criacao primeiro: e o fio mais forte entre duas historias.
  s->nGente = 0;
  if (serie) {
    v = valorRaiz(json, NULL, "created_by");
    if (v && *v == '[') {
      fimV = js_fim(v);
      for (p = v + 1; p && p < fimV && *p; ) {
        while (*p && *p != '{' && *p != ']') p++;
        if (*p != '{') break;
        { const char *f = js_fim(p);
          char nome[80] = "", foto[128] = "";
          js_texto(p, f, "name", nome, sizeof nome);
          js_texto(p, f, "profile_path", foto, sizeof foto);
          addPessoa(s, (long)js_num(p, f, "id", 0.0), nome, foto, 1);
          p = f ? f + 1 : NULL; }
      }
    }
  }
  v = valorRaiz(json, NULL, "credits");
  if (v && *v == '{') {
    const char *fc = js_fim(v);
    const char *crew = valorRaiz(v, fc, "crew");
    const char *cast = valorRaiz(v, fc, "cast");
    if (!serie && crew && *crew == '[') {
      const char *fcr = js_fim(crew);
      for (p = crew + 1; p && p < fcr && *p; ) {
        while (*p && *p != '{' && *p != ']') p++;
        if (*p != '{') break;
        { const char *f = js_fim(p);
          char job[32] = "";
          js_texto(p, f, "job", job, sizeof job);
          if (!strcmp(job, "Director")) {
            char nome[80] = "", foto[128] = "";
            js_texto(p, f, "name", nome, sizeof nome);
            js_texto(p, f, "profile_path", foto, sizeof foto);
            addPessoa(s, (long)js_num(p, f, "id", 0.0), nome, foto, 1);
          }
          p = f ? f + 1 : NULL; }
      }
    }
    if (cast && *cast == '[') {
      const char *fca = js_fim(cast);
      int k = 0;
      for (p = cast + 1; p && p < fca && *p && k < 8; ) {
        while (*p && *p != '{' && *p != ']') p++;
        if (*p != '{') break;
        { const char *f = js_fim(p);
          char nome[80] = "", foto[128] = "";
          js_texto(p, f, "name", nome, sizeof nome);
          js_texto(p, f, "profile_path", foto, sizeof foto);
          addPessoa(s, (long)js_num(p, f, "id", 0.0), nome, foto, 0);
          k++;
          p = f ? f + 1 : NULL; }
      }
    }
  }

  v = valorRaiz(json, NULL, "recommendations");
  if (v && *v == '{') {
    fimV = js_fim(v);
    p = js_array(v, fimV, "results");
    s->nRec = 0;
    while (p && p < fimV && s->nRec < MAPA_REC_MAX) {
      const char *f = js_fim(p);
      MapaRec *r = &s->rec[s->nRec];
      lerObraLista(p, f, &r->o, r->generos, &r->nGen);
      if (r->o.tmdb > 0 && r->o.titulo[0] && r->o.poster[0]) s->nRec++;
      p = js_prox(f);
    }
  }
  return s->obra.titulo[0] != 0;
}

int mapa_ler_creditos(const char *json, int direcao, MapaCreditos *c) {
  const char *arr, *fimA, *p;
  if (!json || !c) return 0;
  c->n = 0;
  arr = valorRaiz(json, NULL, direcao ? "crew" : "cast");
  if (!arr || *arr != '[') return 0;
  fimA = js_fim(arr);
  for (p = arr + 1; p && p < fimA && *p; ) {
    const char *f;
    MapaObra o;
    int i, j;
    while (*p && *p != '{' && *p != ']') p++;
    if (*p != '{') break;
    f = js_fim(p);
    if (direcao) {
      char job[32] = "";
      js_texto(p, f, "job", job, sizeof job);
      if (strcmp(job, "Director") && strcmp(job, "Creator")) { p = f ? f + 1 : NULL; continue; }
    }
    lerObraLista(p, f, &o, NULL, NULL);
    p = f ? f + 1 : NULL;
    if (o.tmdb <= 0 || !o.poster[0] || o.votos < 50) continue;
    for (i = 0; i < c->n; i++) if (c->obras[i].tmdb == o.tmdb) break;
    if (i < c->n) continue;
    // Insercao ordenada por votos: o que a pessoa fez de mais visto primeiro.
    for (i = 0; i < c->n && c->obras[i].votos >= o.votos; i++) {}
    if (i >= MAPA_CRED_MAX) continue;
    if (c->n < MAPA_CRED_MAX) c->n++;
    for (j = c->n - 1; j > i; j--) c->obras[j] = c->obras[j - 1];
    c->obras[i] = o;
  }
  return c->n;
}

// ---------------------------------------------------------------------------
// TEMAS. As palavras-chave do TMDB nao tem traducao na API (vem em ingles com
// qualquer `language`). A tabela cobre as que mais aparecem em filme e serie
// popular; fora dela a palavra nao vira tema — o genero (esse sim traduzido
// pelo TMDB) cobre a lacuna. Em ingles o nome volta pela tabela de idioma
// (idioma_tab.h), como qualquer outro texto.
static const char *const TEMAS[][2] = {
  { "alien", "alienígenas" }, { "alien invasion", "invasão alienígena" },
  { "alternate history", "história alternativa" }, { "amnesia", "amnésia" },
  { "android", "androides" }, { "apocalypse", "apocalipse" },
  { "artificial intelligence (a.i.)", "inteligência artificial" },
  { "assassin", "assassinos" }, { "based on comic", "baseado em HQ" },
  { "based on novel or book", "baseado em livro" },
  { "based on true story", "história real" }, { "biography", "biografia" },
  { "black hole", "buraco negro" }, { "brother brother relationship", "irmãos" },
  { "coming of age", "amadurecimento" }, { "conspiracy", "conspiração" },
  { "corruption", "corrupção" }, { "cyberpunk", "cyberpunk" },
  { "dark comedy", "comédia sombria" }, { "detective", "detetives" },
  { "drug cartel", "cartel" }, { "drugs", "drogas" }, { "dystopia", "distopia" },
  { "espionage", "espionagem" }, { "extraterrestrial technology", "tecnologia alienígena" },
  { "family", "família" }, { "father daughter relationship", "pai e filha" },
  { "father son relationship", "pai e filho" }, { "friendship", "amizade" },
  { "future", "futuro" }, { "gangster", "gângsteres" }, { "ghost", "fantasmas" },
  { "heist", "assalto" }, { "hitman", "matador de aluguel" },
  { "haunted house", "casa assombrada" }, { "hacker", "hackers" },
  { "high school", "ensino médio" }, { "investigation", "investigação" },
  { "kidnapping", "sequestro" }, { "loneliness", "solidão" }, { "love", "amor" },
  { "mafia", "máfia" }, { "magic", "magia" }, { "martial arts", "artes marciais" },
  { "memory", "memória" }, { "mental illness", "doença mental" },
  { "mind control", "controle da mente" }, { "monster", "monstros" },
  { "mother daughter relationship", "mãe e filha" }, { "multiverse", "multiverso" },
  { "murder", "assassinato" }, { "mystery", "mistério" }, { "mythology", "mitologia" },
  { "nightmare", "pesadelo" }, { "nuclear war", "guerra nuclear" },
  { "obsession", "obsessão" }, { "parallel world", "mundo paralelo" },
  { "police", "polícia" }, { "politics", "política" }, { "post-apocalyptic future", "pós-apocalipse" },
  { "prison", "prisão" }, { "psychopath", "psicopatas" }, { "psychological thriller", "suspense psicológico" },
  { "revenge", "vingança" }, { "robot", "robôs" }, { "romance", "romance" },
  { "sci-fi", "ficção científica" }, { "secret identity", "identidade secreta" },
  { "serial killer", "assassino em série" }, { "small town", "cidade pequena" },
  { "space", "espaço" }, { "space travel", "viagem espacial" }, { "spy", "espiões" },
  { "superhero", "super-heróis" }, { "supernatural", "sobrenatural" },
  { "survival", "sobrevivência" }, { "teenager", "adolescência" },
  { "time loop", "loop temporal" }, { "time travel", "viagem no tempo" },
  { "twist ending", "reviravolta final" }, { "vampire", "vampiros" },
  { "virtual reality", "realidade virtual" }, { "war", "guerra" },
  { "witch", "bruxas" }, { "world war ii", "Segunda Guerra" },
  { "zombie", "zumbis" }, { "zombie apocalypse", "apocalipse zumbi" },
  { "dream", "sonhos" }, { "grief", "luto" }, { "hope", "esperança" },
  { "sibling relationship", "irmãos" }, { "cult", "seitas" },
  { "anime", "anime" }, { "based on manga", "baseado em mangá" },
  { "desert", "deserto" }, { "ocean", "oceano" }, { "island", "ilha" },
  { "road trip", "pé na estrada" }, { "chosen one", "o escolhido" },
};

const char *mapa_tema_nome(const char *kw) {
  size_t i;
  if (!kw || !kw[0]) return NULL;
  // Sempre o portugues: a chave da tabela de idioma E o portugues, e quem
  // desenha passa por i18n() como todo texto do app (idioma.h).
  for (i = 0; i < sizeof TEMAS / sizeof TEMAS[0]; i++)
    if (!strcmp(TEMAS[i][0], kw)) return TEMAS[i][1];
  return NULL;
}

// ---------------------------------------------------------------------------
// 2. CRUZAMENTO

unsigned mapa_hash_titulo(const char *t) {
  unsigned h = 2166136261u;
  if (!t) return 0;
  for (; *t; t++) {
    unsigned char c = (unsigned char)*t;
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c >= 0x80) {
      h ^= c; h *= 16777619u;
    }
  }
  return h;
}

static long long chaveObra(const MapaObra *o) {
  if (o->tmdb > 0) return (long long)o->tmdb * 2 + (!strcmp(o->tipo, "series") ? 1 : 0);
  if (o->catIndice >= 0) return -1 - (long long)o->catIndice;
  return -(long long)mapa_hash_titulo(o->titulo) - 100000000LL;
}

static int popcount(unsigned v) { int n = 0; while (v) { n += v & 1u; v >>= 1; } return n; }

typedef struct {
  MapaObra o;
  long gen[MAPA_GEN_MAX]; int nGen;
  unsigned de;          // sementes que recomendam
  float q;              // qualidade propria
  int usado;
} Cand;

#define CAND_MAX (MAPA_SEM_MAX * MAPA_REC_MAX)
static Cand cands[CAND_MAX];

static int genComum(const MapaSemente *s, const long *gen, int nGen) {
  int i, j, n = 0;
  for (i = 0; i < s->nGen; i++)
    for (j = 0; j < nGen; j++) if (s->gen[i].id == gen[j]) { n++; break; }
  return n;
}

// O melhor elo entre duas sementes. Devolve MAPA_ELO_* e preenche o motivo.
static int eloDe(const MapaSemente *s, int n, int a, int b, char *motivo, size_t tam) {
  const MapaSemente *A = &s[a], *B = &s[b];
  int i, j, melhor = -1, melhorN = 0;
  motivo[0] = 0;
  // Tema: a palavra em comum MAIS RARA no mapa e a mais especifica.
  for (i = 0; i < A->nKw; i++) {
    const char *nome = mapa_tema_nome(A->kw[i].nome);
    if (!nome) continue;
    for (j = 0; j < B->nKw; j++) if (A->kw[i].id == B->kw[j].id) break;
    if (j >= B->nKw) continue;
    { int k, freq = 0;
      for (k = 0; k < n; k++) {
        int q;
        for (q = 0; q < s[k].nKw; q++) if (s[k].kw[q].id == A->kw[i].id) { freq++; break; }
      }
      if (melhor < 0 || freq < melhorN) { melhor = i; melhorN = freq; } }
  }
  if (melhor >= 0) {
    snprintf(motivo, tam, "%s", mapa_tema_nome(A->kw[melhor].nome));
    return MAPA_ELO_TEMA;
  }
  for (i = 0; i < A->nGente; i++)
    for (j = 0; j < B->nGente; j++)
      if (A->gente[i].id == B->gente[j].id) {
        snprintf(motivo, tam, "%s", A->gente[i].nome);
        return MAPA_ELO_PESSOA;
      }
  for (i = 0; i < A->nGen; i++)
    for (j = 0; j < B->nGen; j++)
      if (A->gen[i].id == B->gen[j].id) {
        snprintf(motivo, tam, "%s", A->gen[i].nome);
        return MAPA_ELO_GENERO;
      }
  if (A->obra.ano && B->obra.ano && A->obra.ano / 10 == B->obra.ano / 10) {
    snprintf(motivo, tam, "%d", (A->obra.ano / 10) * 10);
    return MAPA_ELO_DECADA;
  }
  return MAPA_ELO_NADA;
}

typedef struct { int a, b, c; float score; int elo; char motivo[64]; } Opcao;

void mapa_cruzar(const MapaSemente *s, int n, const MapaCreditos *cred,
                 int nCred, const unsigned *vistos, int nVistos, Mapa *m) {
  static Opcao ops[MAPA_SEM_MAX * MAPA_SEM_MAX * 3];
  int nc = 0, nOps = 0, i, j, k;
  int usoSem[MAPA_SEM_MAX];
  unsigned rev = m->revisao;
  memset(m, 0, sizeof *m);
  m->revisao = rev;
  if (n > MAPA_SEM_MAX) n = MAPA_SEM_MAX;
  m->nSem = n;
  m->estado = n > 0 ? MAPA_LOCAL : MAPA_VAZIO;
  for (i = 0; i < n; i++) {
    m->sem[i] = s[i].obra;
    m->semOrigem[i] = s[i].origem;
    if (s[i].quando) m->estado = MAPA_CRUZADO;
  }

  // Candidatos: a uniao das recomendacoes, com a mascara de quem recomendou.
  for (i = 0; i < n; i++)
    for (j = 0; j < s[i].nRec; j++) {
      const MapaRec *r = &s[i].rec[j];
      long long ch = chaveObra(&r->o);
      unsigned h = mapa_hash_titulo(r->o.titulo);
      int v;
      for (v = 0; v < n; v++)
        if (chaveObra(&s[v].obra) == ch || mapa_hash_titulo(s[v].obra.titulo) == h) break;
      if (v < n) continue;
      for (v = 0; v < nVistos; v++) if (vistos[v] == h) break;
      if (v < nVistos) continue;
      for (k = 0; k < nc; k++) if (chaveObra(&cands[k].o) == ch) break;
      if (k == nc) {
        if (nc >= CAND_MAX) continue;
        memset(&cands[nc], 0, sizeof cands[nc]);
        cands[nc].o = r->o;
        memcpy(cands[nc].gen, r->generos, sizeof r->generos);
        cands[nc].nGen = r->nGen;
        // Nota vale mais que popularidade, mas um 9,0 com 12 votos nao e um
        // 9,0: os votos entram com peso logaritmico e teto.
        cands[nc].q = r->o.nota / 10.0f +
                      (r->o.votos > 0 ? fminf(logf((float)r->o.votos + 1.0f), 9.0f) * 0.45f : 1.5f);
        nc++;
      }
      cands[k].de |= 1u << i;
    }

  // Pontes. Para cada par, as tres melhores historias que atravessam os dois.
  for (i = 0; i < n; i++)
    for (j = (n == 1 ? i : i + 1); j < n; j++) {
      char motivo[64];
      int elo = eloDe(s, n, i, j, motivo, sizeof motivo);
      float forcaElo = elo == MAPA_ELO_TEMA ? 4.0f : elo == MAPA_ELO_PESSOA ? 5.0f :
                       elo == MAPA_ELO_GENERO ? 1.5f : elo == MAPA_ELO_DECADA ? 0.5f : 0.0f;
      int melhores[3] = { -1, -1, -1 };
      float notas[3] = { -1e9f, -1e9f, -1e9f };
      for (k = 0; k < nc; k++) {
        unsigned bi = 1u << i, bj = 1u << j;
        float sc;
        int g;
        if (!(cands[k].de & (bi | bj))) continue;
        sc = cands[k].q + forcaElo;
        if ((cands[k].de & bi) && (cands[k].de & bj) && i != j) sc += 12.0f;
        else sc += 4.0f;
        g = genComum(&s[i], cands[k].gen, cands[k].nGen) +
            (i != j ? genComum(&s[j], cands[k].gen, cands[k].nGen) : 0);
        sc += (float)(g > 3 ? 3 : g) * 1.5f;
        sc += popcount(cands[k].de) * 0.8f;
        { int p;
          for (p = 0; p < 3; p++) if (sc > notas[p]) {
            int q;
            for (q = 2; q > p; q--) { notas[q] = notas[q - 1]; melhores[q] = melhores[q - 1]; }
            notas[p] = sc; melhores[p] = k; break;
          } }
      }
      for (k = 0; k < 3; k++) {
        if (melhores[k] < 0 || nOps >= (int)(sizeof ops / sizeof ops[0])) continue;
        ops[nOps].a = i; ops[nOps].b = j; ops[nOps].c = melhores[k];
        ops[nOps].score = notas[k];
        ops[nOps].elo = ((cands[melhores[k]].de & (1u << i)) &&
                         (cands[melhores[k]].de & (1u << j)) && i != j && elo == MAPA_ELO_NADA)
                        ? MAPA_ELO_DUPLA : elo;
        snprintf(ops[nOps].motivo, sizeof ops[nOps].motivo, "%s", motivo);
        nOps++;
      }
    }
  // Ordena por forca (insercao: sao no maximo 84 opcoes).
  for (i = 1; i < nOps; i++) {
    Opcao t = ops[i];
    for (j = i; j > 0 && ops[j - 1].score < t.score; j--) ops[j] = ops[j - 1];
    ops[j] = t;
  }
  memset(usoSem, 0, sizeof usoSem);
  { int passo;
    // Primeira passada espalha (cada semente em no maximo 2 pontes); a
    // segunda aceita repetir para nao deixar o mapa com uma ponte so.
    for (passo = 0; passo < 2 && m->nPontes < MAPA_PONTE_MAX; passo++)
      for (i = 0; i < nOps && m->nPontes < MAPA_PONTE_MAX; i++) {
        Opcao *o = &ops[i];
        int lim = passo ? 4 : 2, q;
        if (cands[o->c].usado) continue;
        if (usoSem[o->a] >= lim || usoSem[o->b] >= lim) continue;
        for (q = 0; q < m->nPontes; q++)
          if ((m->pontes[q].a == o->a && m->pontes[q].b == o->b)) break;
        if (q < m->nPontes && !passo) continue;
        cands[o->c].usado = 1;
        usoSem[o->a]++; if (o->b != o->a) usoSem[o->b]++;
        m->pontes[m->nPontes].a = o->a;
        m->pontes[m->nPontes].b = o->b;
        m->pontes[m->nPontes].obra = cands[o->c].o;
        m->pontes[m->nPontes].elo = o->elo;
        m->pontes[m->nPontes].forca = (int)o->score;
        snprintf(m->pontes[m->nPontes].motivo, sizeof m->pontes[0].motivo, "%s", o->motivo);
        m->nPontes++;
      }
  }

  // Fios: gente que aparece em duas ou mais sementes.
  { static struct { long id; int n, dir; unsigned mask; int si, gi; } g[64];
    int ng = 0;
    for (i = 0; i < n; i++)
      for (j = 0; j < s[i].nGente; j++) {
        const MapaPessoa *p = &s[i].gente[j];
        for (k = 0; k < ng; k++) if (g[k].id == p->id) break;
        if (k == ng) {
          if (ng >= 64) continue;
          g[ng].id = p->id; g[ng].n = 0; g[ng].dir = p->direcao; g[ng].mask = 0;
          g[ng].si = i; g[ng].gi = j; ng++;
        }
        if (!(g[k].mask & (1u << i))) { g[k].mask |= 1u << i; g[k].n++; }
        if (p->direcao) g[k].dir = 1;
      }
    while (m->nFios < MAPA_FIO_MAX) {
      int best = -1;
      for (k = 0; k < ng; k++) {
        if (g[k].n < 2) continue;
        if (best < 0 || g[k].n > g[best].n ||
            (g[k].n == g[best].n && g[k].dir > g[best].dir)) best = k;
      }
      if (best < 0) break;
      { MapaFio *f = &m->fios[m->nFios];
        const MapaPessoa *p = &s[g[best].si].gente[g[best].gi];
        f->id = p->id;
        snprintf(f->nome, sizeof f->nome, "%s", p->nome);
        snprintf(f->foto, sizeof f->foto, "%s", p->foto);
        f->direcao = g[best].dir;
        for (i = 0; i < n; i++) if (g[best].mask & (1u << i)) f->sementes[f->n++] = i;
        // A proxima obra da pessoa: dos creditos do TMDB quando vieram; senao
        // um candidato do catalogo que ja cite o mesmo nome.
        for (k = 0; k < nCred && !f->temProxima; k++) {
          int q;
          if (cred[k].pessoa != f->id) continue;
          for (q = 0; q < cred[k].n && !f->temProxima; q++) {
            long long ch = chaveObra(&cred[k].obras[q]);
            unsigned h = mapa_hash_titulo(cred[k].obras[q].titulo);
            int v, visto = 0;
            for (v = 0; v < n; v++)
              if (chaveObra(&s[v].obra) == ch || mapa_hash_titulo(s[v].obra.titulo) == h) visto = 1;
            for (v = 0; v < nVistos; v++) if (vistos[v] == h) visto = 1;
            for (v = 0; v < m->nPontes; v++) if (chaveObra(&m->pontes[v].obra) == ch) visto = 1;
            if (!visto) { f->proxima = cred[k].obras[q]; f->temProxima = 1; }
          }
        }
        m->nFios++; }
      g[best].n = 0;
    }
  }

  // Temas: palavras que voltam em mais de uma semente, depois generos.
  { static struct { long id; char nome[48]; int n; unsigned mask; } t[96];
    int nt = 0, fase;
    for (fase = 0; fase < 2; fase++) {
      nt = 0;
      for (i = 0; i < n; i++) {
        int total = fase ? s[i].nGen : s[i].nKw;
        for (j = 0; j < total; j++) {
          const MapaEtiqueta *e = fase ? &s[i].gen[j] : &s[i].kw[j];
          const char *nome = fase ? e->nome : mapa_tema_nome(e->nome);
          if (!nome) continue;
          for (k = 0; k < nt; k++) if (t[k].id == e->id) break;
          if (k == nt) {
            if (nt >= 96) continue;
            t[nt].id = e->id; t[nt].n = 0; t[nt].mask = 0;
            snprintf(t[nt].nome, sizeof t[nt].nome, "%s", nome); nt++;
          }
          if (!(t[k].mask & (1u << i))) { t[k].mask |= 1u << i; t[k].n++; }
        }
      }
      while (m->nTemas < MAPA_TEMA_MAX) {
        int best = -1, dup;
        for (k = 0; k < nt; k++)
          if (t[k].n >= 2 && (best < 0 || t[k].n > t[best].n)) best = k;
        if (best < 0) break;
        for (dup = 0, j = 0; j < m->nTemas; j++)
          if (!strcmp(m->temas[j].nome, t[best].nome) || m->temas[j].mascara == t[best].mask) dup = 1;
        if (!dup) {
          snprintf(m->temas[m->nTemas].nome, sizeof m->temas[0].nome, "%s", t[best].nome);
          m->temas[m->nTemas].n = t[best].n;
          m->temas[m->nTemas].mascara = t[best].mask;
          m->nTemas++;
        }
        t[best].n = 0;
      }
    }
  }

  // Sorte: o resto dos candidatos, pelo mesmo gosto.
  { int escolhidos = 0;
    while (escolhidos < MAPA_SORTE_MAX) {
      int best = -1;
      float bs = -1e9f;
      for (k = 0; k < nc; k++) {
        float sc;
        if (cands[k].usado) continue;
        sc = cands[k].q + popcount(cands[k].de) * 2.5f;
        if (sc > bs) { bs = sc; best = k; }
      }
      if (best < 0) break;
      cands[best].usado = 1;
      m->sorte[escolhidos++] = cands[best].o;
    }
    m->nSorte = escolhidos; }

  m->anoMin = 9999; m->anoMax = 0;
  for (i = 0; i < n; i++) if (s[i].obra.ano) {
    if (s[i].obra.ano < m->anoMin) m->anoMin = s[i].obra.ano;
    if (s[i].obra.ano > m->anoMax) m->anoMax = s[i].obra.ano;
  }
  for (i = 0; i < m->nPontes; i++) if (m->pontes[i].obra.ano) {
    if (m->pontes[i].obra.ano < m->anoMin) m->anoMin = m->pontes[i].obra.ano;
    if (m->pontes[i].obra.ano > m->anoMax) m->anoMax = m->pontes[i].obra.ano;
  }
  if (m->anoMax == 0) m->anoMin = 0;
}

#ifndef NV_MAPA_PURO
// ---------------------------------------------------------------------------
// 3. ESTADO

#define CACHE_NOME   "explorar-mapa.txt"
#define CACHE_MAX    16
#define CACHE_DIAS   7
#define VISTOS_MAX   96

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
// mapa_cruzar usa vetores estaticos (cabem no fio principal do WebAssembly,
// que tem pilha curta); esta trava garante um cruzamento por vez.
static pthread_mutex_t travaMontagem = PTHREAD_MUTEX_INITIALIZER;
static Mapa publicado;
static unsigned revisaoPub = 1;
static int fioVivo;
static unsigned assinaturaFeita;   // sementes do ultimo cruzamento pelo TMDB

// Entrada do fio: copia das sementes, feita no fio principal.
static MapaSemente semTrab[MAPA_SEM_MAX];
static int nSemTrab;
static unsigned vistosTrab[VISTOS_MAX];
static int nVistosTrab;
static char chaveTmdb[96];
static char idiomaTmdb[12];

static void publicar(const MapaSemente *s, int n, const MapaCreditos *cred, int nCred,
                     const unsigned *vistos, int nVistos, int carregando) {
  static Mapa novo;          // montado fora da trava: o desenho so espera a copia
  static MapaSemente vis[MAPA_SEM_MAX];
  int i, k = 0;
  pthread_mutex_lock(&travaMontagem);
  // Semente que so tem o id (o TMDB ainda nao respondeu) nao vira estrela
  // sem nome: fica fora deste retrato e entra no seguinte.
  for (i = 0; i < n && k < MAPA_SEM_MAX; i++) if (s[i].obra.titulo[0]) vis[k++] = s[i];
  novo.revisao = 0;
  mapa_cruzar(vis, k, cred, nCred, vistos, nVistos, &novo);
  novo.carregando = carregando;
  pthread_mutex_lock(&trava);
  novo.revisao = ++revisaoPub;
  publicado = novo;
  pthread_mutex_unlock(&trava);
  pthread_mutex_unlock(&travaMontagem);
}

int mapa_copiar(Mapa *dst, unsigned *revisao) {
  int copiou = 0;
  if (!dst || !revisao) return 0;
  pthread_mutex_lock(&trava);
  if (publicado.revisao != *revisao) {
    *dst = publicado;
    *revisao = publicado.revisao;
    copiou = 1;
  }
  pthread_mutex_unlock(&trava);
  return copiou;
}

void mapa_publicar_teste(const MapaSemente *s, int n, const MapaCreditos *c, int nc) {
  publicar(s, n, c, nc, NULL, 0, 0);
}

void mapa_esquecer(void) {
  pthread_mutex_lock(&trava);
  memset(&publicado, 0, sizeof publicado);
  publicado.revisao = ++revisaoPub;
  assinaturaFeita = 0;
  pthread_mutex_unlock(&trava);
  dados_apagar(CACHE_NOME);
}

// --- sementes a partir do que o app ja tem -----------------------------------

static int anoDoMeta(const char *meta) {
  const char *p = meta;
  if (!p) return 0;
  while (*p) {
    if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1]) &&
        isdigit((unsigned char)p[2]) && isdigit((unsigned char)p[3])) {
      int a = atoi(p);
      if (a > 1870 && a < 2100) return a;
    }
    p++;
  }
  return 0;
}

static long hashNome(const char *s) { return -(long)(mapa_hash_titulo(s) & 0x3fffffff) - 1; }

// "Filme  ·  Ação  ·  Drama": o primeiro pedaco e o tipo, o resto sao generos.
static void generosLocais(const char *g, MapaSemente *s) {
  char buf[160], *p, *tok;
  int primeiro = 1;
  snprintf(buf, sizeof buf, "%s", g ? g : "");
  p = buf;
  s->nGen = 0;
  while (p && *p && s->nGen < MAPA_GEN_MAX) {
    char *sep = strstr(p, "\xc2\xb7");
    if (sep) *sep = 0;
    tok = p;
    while (*tok == ' ') tok++;
    { size_t k = strlen(tok); while (k && tok[k - 1] == ' ') tok[--k] = 0; }
    if (tok[0] && !primeiro) {
      s->gen[s->nGen].id = hashNome(tok);
      snprintf(s->gen[s->nGen].nome, sizeof s->gen[0].nome, "%s", tok);
      s->nGen++;
    }
    primeiro = 0;
    p = sep ? sep + 2 : NULL;
  }
}

static void obraDoItem(const CatItem *ci, int indice, MapaObra *o) {
  memset(o, 0, sizeof *o);
  snprintf(o->imdb, sizeof o->imdb, "%s", !strncmp(ci->imdb, "tt", 2) ? ci->imdb : "");
  if (!strncmp(ci->imdb, "tmdb:", 5)) o->tmdb = atol(ci->imdb + 5);
  snprintf(o->tipo, sizeof o->tipo, "%s", !strcmp(ci->tipo, "series") ? "series" : "movie");
  copiaLimpa(o->titulo, sizeof o->titulo, ci->titulo);
  snprintf(o->poster, sizeof o->poster, "%s", ci->poster[0] ? ci->poster : ci->backdrop);
  snprintf(o->fundo, sizeof o->fundo, "%s", ci->backdrop);
  copiaLimpa(o->sinopse, sizeof o->sinopse, ci->sinopse);
  o->ano = anoDoMeta(ci->meta);
  o->nota = ci->nota;
  o->catIndice = indice;
}

static void sementeDoItem(const CatItem *ci, int indice, int origem, MapaSemente *s) {
  int i;
  memset(s, 0, sizeof *s);
  obraDoItem(ci, indice, &s->obra);
  s->origem = origem;
  generosLocais(ci->genero, s);
  if (ci->direcao[0]) {
    char buf[128], *p, *q;
    snprintf(buf, sizeof buf, "%s", ci->direcao);
    for (p = buf; p && *p; p = q) {
      q = strchr(p, ',');
      if (q) *q++ = 0;
      while (*p == ' ') p++;
      if (*p) addPessoa(s, hashNome(p), p, NULL, 1);
    }
  }
  for (i = 0; i < ci->nElenco && i < 6; i++)
    addPessoa(s, ci->elenco[i].tmdb > 0 ? ci->elenco[i].tmdb : hashNome(ci->elenco[i].nome),
              ci->elenco[i].nome, NULL, 0);
  // elenco[].foto ja e URL pronta; addPessoa espera caminho do TMDB.
  for (i = 0; i < s->nGente; i++) {
    int k;
    for (k = 0; k < ci->nElenco; k++)
      if (!strcmp(ci->elenco[k].nome, s->gente[i].nome))
        snprintf(s->gente[i].foto, sizeof s->gente[i].foto, "%s", ci->elenco[k].foto);
  }
}

static int itemPorImdb(const char *imdb) {
  int i, n = cat_n();
  if (!imdb || !imdb[0]) return -1;
  for (i = 0; i < n; i++) {
    const CatItem *ci = cat_item(i);
    if (ci && !strcmp(ci->imdb, imdb)) return i;
  }
  return -1;
}

static int jaTem(const MapaSemente *s, int n, const char *imdb, const char *titulo) {
  int i;
  unsigned h = mapa_hash_titulo(titulo);
  for (i = 0; i < n; i++) {
    if (imdb && imdb[0] && !strcmp(s[i].obra.imdb, imdb)) return 1;
    if (titulo && titulo[0] && mapa_hash_titulo(s[i].obra.titulo) == h) return 1;
  }
  return 0;
}

static int pontuacaoItem(const CatItem *ci) {
  int p = 0;
  if (!ci->poster[0]) return -1;
  p += ci->nota;
  if (ci->sinopse[0]) p += 10;
  if (ci->genero[0]) p += 10;
  return p;
}

static int coletar(MapaSemente *s, int max, unsigned *vistos, int *nVistos,
                   int comTmdb) {
  static ProgRegistro regs[PROG_MAX];
  int n = 0, i, total, nr;
  total = cat_n();
  *nVistos = 0;

  // 1) o que a pessoa assistiu por ultimo, na ordem do progresso local
  nr = prog_ler(regs, PROG_MAX);
  for (i = 0; i < nr && n < max; i++) {
    const char *id = regs[i].contentId;
    int idx, origem;
    if (strncmp(id, "tt", 2) && strncmp(id, "tmdb:", 5)) continue;
    if (jaTem(s, n, id, NULL)) continue;
    origem = (regs[i].durSeg > 0 && regs[i].posSeg / regs[i].durSeg >= 0.9)
             ? MAPA_ORIGEM_VISTO : MAPA_ORIGEM_ANDAMENTO;
    idx = itemPorImdb(id);
    if (idx >= 0) {
      const CatItem *ci = cat_item(idx);
      if (!ci || jaTem(s, n, NULL, ci->titulo)) continue;
      sementeDoItem(ci, idx, origem, &s[n++]);
    } else if (comTmdb && !strncmp(id, "tt", 2)) {
      // Fora do catalogo: so o id. O fio do TMDB resolve titulo e arte; sem
      // ele nao ha o que desenhar e a semente nem entra (comTmdb = 0).
      memset(&s[n], 0, sizeof s[n]);
      snprintf(s[n].obra.imdb, sizeof s[n].obra.imdb, "%s", id);
      snprintf(s[n].obra.tipo, sizeof s[n].obra.tipo, "%s",
               !strcmp(regs[i].tipo, "series") ? "series" : "movie");
      s[n].obra.catIndice = -1;
      s[n].origem = origem;
      n++;
    }
  }
  // 2) catalogo: o que tem progresso ou esta na lista
  for (i = 0; i < total && n < max; i++) {
    const CatItem *ci = cat_item(i);
    if (!ci || !ci->titulo[0] || !(ci->progresso > 0 || ci->naLista)) continue;
    if (jaTem(s, n, ci->imdb, ci->titulo)) continue;
    sementeDoItem(ci, i, ci->progresso >= 90 ? MAPA_ORIGEM_VISTO :
                  ci->progresso > 0 ? MAPA_ORIGEM_ANDAMENTO : MAPA_ORIGEM_LISTA, &s[n++]);
  }
  // 3) salvos locais
  for (i = 0; i < salvos_n() && n < max; i++) {
    const SalvoItem *sv = salvos_item(i);
    int idx;
    if (!sv || jaTem(s, n, sv->id, sv->titulo)) continue;
    idx = itemPorImdb(sv->id);
    if (idx >= 0) { sementeDoItem(cat_item(idx), idx, MAPA_ORIGEM_LISTA, &s[n++]); continue; }
    memset(&s[n], 0, sizeof s[n]);
    snprintf(s[n].obra.imdb, sizeof s[n].obra.imdb, "%s", sv->id);
    snprintf(s[n].obra.tipo, sizeof s[n].obra.tipo, "%s", sv->tipo);
    copiaLimpa(s[n].obra.titulo, sizeof s[n].obra.titulo, sv->titulo);
    snprintf(s[n].obra.poster, sizeof s[n].obra.poster, "%s", sv->poster);
    s[n].obra.ano = anoDoMeta(sv->meta);
    s[n].obra.nota = sv->nota;
    s[n].obra.catIndice = -1;
    s[n].origem = MAPA_ORIGEM_LISTA;
    n++;
  }
  // 4) sem historico suficiente: o que o catalogo tem de mais forte
  while (n < 4 && n < max) {
    int melhor = -1, mp = -1;
    for (i = 0; i < total; i++) {
      const CatItem *ci = cat_item(i);
      int p;
      if (!ci || !ci->titulo[0] || jaTem(s, n, ci->imdb, ci->titulo)) continue;
      p = pontuacaoItem(ci);
      if (p > mp) { mp = p; melhor = i; }
    }
    if (melhor < 0) break;
    sementeDoItem(cat_item(melhor), melhor, MAPA_ORIGEM_ALTA, &s[n++]);
  }
  // O que a pessoa ja comecou nunca volta como sugestao.
  for (i = 0; i < total && *nVistos < VISTOS_MAX; i++) {
    const CatItem *ci = cat_item(i);
    if (ci && ci->progresso > 0) vistos[(*nVistos)++] = mapa_hash_titulo(ci->titulo);
  }
  return n;
}

// Reserva local: cada semente "recomenda" o que o catalogo tem com os mesmos
// generos. E o mesmo cruzamento, so que com o catalogo no papel do TMDB.
static void recomendarDoCatalogo(MapaSemente *s, int n) {
  int i, k, total = cat_n();
  for (i = 0; i < n; i++) {
    s[i].nRec = 0;
    for (k = 0; k < total && s[i].nRec < MAPA_REC_MAX; k++) {
      const CatItem *ci = cat_item(k);
      static MapaSemente tmp;
      int g, h, comum = 0;
      if (!ci || !ci->titulo[0] || !ci->poster[0] || ci->progresso > 0) continue;
      if (jaTem(s, n, ci->imdb, ci->titulo)) continue;
      generosLocais(ci->genero, &tmp);
      for (g = 0; g < tmp.nGen; g++)
        for (h = 0; h < s[i].nGen; h++) if (tmp.gen[g].id == s[i].gen[h].id) comum++;
      if (!comum) continue;
      obraDoItem(ci, k, &s[i].rec[s[i].nRec].o);
      s[i].rec[s[i].nRec].nGen = tmp.nGen;
      for (g = 0; g < tmp.nGen; g++) s[i].rec[s[i].nRec].generos[g] = tmp.gen[g].id;
      s[i].nRec++;
    }
  }
}

static unsigned assinatura(const MapaSemente *s, int n) {
  unsigned h = 2166136261u;
  int i;
  for (i = 0; i < n; i++) {
    h ^= mapa_hash_titulo(s[i].obra.imdb[0] ? s[i].obra.imdb : s[i].obra.titulo);
    h *= 16777619u;
  }
  return h ? h : 1;
}

// --- cache em disco -----------------------------------------------------------
//
// Uma linha por fato, campos separados por TAB (os textos ja chegam sem TAB por
// copiaLimpa). S abre uma semente; G/K/P/R pertencem a ultima S; C guarda um
// credito de pessoa. Texto e nao binario porque dados_gravar_leve e de texto e
// porque sobrevive a qualquer mudanca de struct.

typedef struct { MapaSemente *s; int n; MapaCreditos *c; int nc; } Cache;

static char *campo(char **p) {
  char *ini = *p, *t;
  if (!ini) return (char *)"";
  t = strchr(ini, '\t');
  if (t) { *t = 0; *p = t + 1; } else *p = NULL;
  return ini;
}

static void lerObraCampos(char **p, MapaObra *o) {
  o->tmdb = atol(campo(p));
  snprintf(o->tipo, sizeof o->tipo, "%s", campo(p));
  o->ano = atoi(campo(p));
  o->nota = atoi(campo(p));
  o->votos = atoi(campo(p));
  snprintf(o->titulo, sizeof o->titulo, "%s", campo(p));
  snprintf(o->poster, sizeof o->poster, "%s", campo(p));
  snprintf(o->fundo, sizeof o->fundo, "%s", campo(p));
  snprintf(o->sinopse, sizeof o->sinopse, "%s", campo(p));
  o->catIndice = -1;
}

static void cacheLer(Cache *c) {
  char *txt = dados_ler(CACHE_NOME), *linha, *prox;
  MapaSemente *atual = NULL;
  MapaCreditos *cr = NULL;
  c->n = c->nc = 0;
  if (!txt) return;
  if (strncmp(txt, "NVMAPA 1\n", 9)) { free(txt); return; }
  for (linha = txt + 9; linha && *linha; linha = prox) {
    char *p;
    prox = strchr(linha, '\n');
    if (prox) *prox++ = 0;
    p = linha + 2;
    if (linha[1] != '\t') continue;
    if (linha[0] == 'S' && c->n < CACHE_MAX) {
      atual = &c->s[c->n++];
      memset(atual, 0, sizeof *atual);
      snprintf(atual->obra.imdb, sizeof atual->obra.imdb, "%s", campo(&p));
      atual->quando = atoll(campo(&p));
      lerObraCampos(&p, &atual->obra);
    } else if (linha[0] == 'G' && atual && atual->nGen < MAPA_GEN_MAX) {
      atual->gen[atual->nGen].id = atol(campo(&p));
      snprintf(atual->gen[atual->nGen].nome, sizeof atual->gen[0].nome, "%s", campo(&p));
      atual->nGen++;
    } else if (linha[0] == 'K' && atual && atual->nKw < MAPA_KW_MAX) {
      atual->kw[atual->nKw].id = atol(campo(&p));
      snprintf(atual->kw[atual->nKw].nome, sizeof atual->kw[0].nome, "%s", campo(&p));
      atual->nKw++;
    } else if (linha[0] == 'P' && atual && atual->nGente < MAPA_GENTE_MAX) {
      MapaPessoa *g = &atual->gente[atual->nGente++];
      g->id = atol(campo(&p));
      g->direcao = atoi(campo(&p));
      snprintf(g->nome, sizeof g->nome, "%s", campo(&p));
      snprintf(g->foto, sizeof g->foto, "%s", campo(&p));
    } else if (linha[0] == 'R' && atual && atual->nRec < MAPA_REC_MAX) {
      MapaRec *r = &atual->rec[atual->nRec++];
      char *gs;
      memset(r, 0, sizeof *r);
      gs = campo(&p);
      while (*gs && r->nGen < MAPA_GEN_MAX) {
        r->generos[r->nGen++] = atol(gs);
        while (*gs && *gs != ',') gs++;
        if (*gs == ',') gs++;
      }
      lerObraCampos(&p, &r->o);
    } else if (linha[0] == 'C') {
      long pid = atol(campo(&p));
      if (!cr || cr->pessoa != pid) {
        if (c->nc >= CACHE_MAX) continue;
        cr = &c->c[c->nc++];
        memset(cr, 0, sizeof *cr);
        cr->pessoa = pid;
      }
      if (cr->n < MAPA_CRED_MAX) lerObraCampos(&p, &cr->obras[cr->n++]);
    }
  }
  free(txt);
}

static size_t escreverObra(char *b, size_t n, const MapaObra *o) {
  return (size_t)snprintf(b, n, "%ld\t%s\t%d\t%d\t%d\t%s\t%s\t%s\t%s\n",
                          o->tmdb, o->tipo, o->ano, o->nota, o->votos,
                          o->titulo, o->poster, o->fundo, o->sinopse);
}

static void cacheGravar(const Cache *c) {
  size_t cap = 64 * 1024 + (size_t)c->n * 24 * 1024, k = 0;
  char *b = malloc(cap);
  int i, j;
  if (!b) return;
  k += (size_t)snprintf(b + k, cap - k, "NVMAPA 1\n");
#define ESPACO (k + 2048 < cap)
  for (i = 0; i < c->n && ESPACO; i++) {
    const MapaSemente *s = &c->s[i];
    if (!s->quando) continue;
    k += (size_t)snprintf(b + k, cap - k, "S\t%s\t%lld\t", s->obra.imdb, s->quando);
    k += escreverObra(b + k, cap - k, &s->obra);
    for (j = 0; j < s->nGen && ESPACO; j++)
      k += (size_t)snprintf(b + k, cap - k, "G\t%ld\t%s\n", s->gen[j].id, s->gen[j].nome);
    for (j = 0; j < s->nKw && ESPACO; j++)
      k += (size_t)snprintf(b + k, cap - k, "K\t%ld\t%s\n", s->kw[j].id, s->kw[j].nome);
    for (j = 0; j < s->nGente && ESPACO; j++)
      k += (size_t)snprintf(b + k, cap - k, "P\t%ld\t%d\t%s\t%s\n", s->gente[j].id,
                            s->gente[j].direcao, s->gente[j].nome, s->gente[j].foto);
    for (j = 0; j < s->nRec && ESPACO; j++) {
      int g;
      k += (size_t)snprintf(b + k, cap - k, "R\t");
      for (g = 0; g < s->rec[j].nGen; g++)
        k += (size_t)snprintf(b + k, cap - k, "%s%ld", g ? "," : "", s->rec[j].generos[g]);
      k += (size_t)snprintf(b + k, cap - k, "\t");
      k += escreverObra(b + k, cap - k, &s->rec[j].o);
    }
  }
  for (i = 0; i < c->nc && ESPACO; i++)
    for (j = 0; j < c->c[i].n && ESPACO; j++) {
      k += (size_t)snprintf(b + k, cap - k, "C\t%ld\t", c->c[i].pessoa);
      k += escreverObra(b + k, cap - k, &c->c[i].obras[j]);
    }
#undef ESPACO
  dados_gravar_leve(CACHE_NOME, b);
  free(b);
}

// --- o fio ---------------------------------------------------------------------

static long resolverTmdb(const char *imdb, char *tipo, size_t nTipo) {
  char url[400], *corpo;
  long id = 0;
  snprintf(url, sizeof url,
           "https://api.themoviedb.org/3/find/%s?api_key=%s&external_source=imdb_id&language=%s",
           imdb, chaveTmdb, idiomaTmdb);
  corpo = rede_baixar(url, 15);
  if (!corpo) return 0;
  { const char *p = js_array(corpo, NULL, "movie_results");
    if (p && *p == '{') { id = (long)js_num(p, js_fim(p), "id", 0.0); snprintf(tipo, nTipo, "movie"); } }
  if (!id) {
    const char *p = js_array(corpo, NULL, "tv_results");
    if (p && *p == '{') { id = (long)js_num(p, js_fim(p), "id", 0.0); snprintf(tipo, nTipo, "series"); }
  }
  free(corpo);
  return id;
}

static int lerDoTmdb(MapaSemente *s) {
  char url[500], *corpo;
  int serie, ok;
  if (!s->obra.tmdb && s->obra.imdb[0]) {
    char tipo[8] = "";
    s->obra.tmdb = resolverTmdb(s->obra.imdb, tipo, sizeof tipo);
    if (tipo[0]) snprintf(s->obra.tipo, sizeof s->obra.tipo, "%s", tipo);
  }
  if (!s->obra.tmdb) return 0;
  serie = !strcmp(s->obra.tipo, "series");
  snprintf(url, sizeof url,
           "https://api.themoviedb.org/3/%s/%ld?api_key=%s&language=%s"
           "&append_to_response=keywords,credits,recommendations",
           serie ? "tv" : "movie", s->obra.tmdb, chaveTmdb, idiomaTmdb);
  corpo = rede_baixar(url, 20);
  if (!corpo) return 0;
  {
    // A leitura substitui os dados locais (generos com id do TMDB, pessoas
    // com id real). O titulo e o poster do catalogo ficam se o TMDB nao tiver.
    static MapaSemente lido;   // 18 KB: fora da pilha do fio
    lido = *s;
    lido.nGen = lido.nKw = lido.nGente = lido.nRec = 0;
    ok = mapa_ler_detalhe(corpo, serie, &lido);
    if (ok) { lido.quando = (long long)time(NULL); *s = lido; }
  }
  free(corpo);
  return ok;
}

static int lerCreditos(long pessoa, int direcao, MapaCreditos *c) {
  char url[400], *corpo;
  int n;
  if (pessoa <= 0) return 0;
  snprintf(url, sizeof url,
           "https://api.themoviedb.org/3/person/%ld/combined_credits?api_key=%s&language=%s",
           pessoa, chaveTmdb, idiomaTmdb);
  corpo = rede_baixar(url, 20);
  if (!corpo) return 0;
  n = mapa_ler_creditos(corpo, direcao, c);
  c->pessoa = pessoa;
  free(corpo);
  return n;
}

static void *trabalhar(void *arg) {
  static MapaSemente sem[MAPA_SEM_MAX];
  static unsigned vistos[VISTOS_MAX];
  static MapaCreditos cred[MAPA_FIO_MAX];
  Cache cache;
  int n, nv, i, k, nCred = 0;
  long long agora = (long long)time(NULL);
  (void)arg;

  pthread_mutex_lock(&trava);
  n = nSemTrab; nv = nVistosTrab;
  memcpy(sem, semTrab, sizeof sem);
  memcpy(vistos, vistosTrab, sizeof vistos);
  pthread_mutex_unlock(&trava);

  cache.s = calloc(CACHE_MAX, sizeof *cache.s);
  cache.c = calloc(CACHE_MAX, sizeof *cache.c);
  if (!cache.s || !cache.c) {
    free(cache.s); free(cache.c);
    pthread_mutex_lock(&trava); fioVivo = 0; pthread_mutex_unlock(&trava);
    return NULL;
  }
  cacheLer(&cache);

  for (i = 0; i < n; i++) {
    int achou = 0;
    for (k = 0; k < cache.n; k++) {
      MapaSemente *c = &cache.s[k];
      if (!c->quando || agora - c->quando > CACHE_DIAS * 86400LL) continue;
      if ((sem[i].obra.imdb[0] && !strcmp(c->obra.imdb, sem[i].obra.imdb)) ||
          (sem[i].obra.tmdb && c->obra.tmdb == sem[i].obra.tmdb)) {
        int origem = sem[i].origem, idx = sem[i].obra.catIndice;
        MapaObra local = sem[i].obra;
        sem[i] = *c;
        sem[i].origem = origem;
        sem[i].obra.catIndice = idx;
        if (local.imdb[0]) snprintf(sem[i].obra.imdb, sizeof sem[i].obra.imdb, "%s", local.imdb);
        if (!sem[i].obra.poster[0]) snprintf(sem[i].obra.poster, sizeof sem[i].obra.poster, "%s", local.poster);
        achou = 1;
        break;
      }
    }
    if (!achou) {
      lerDoTmdb(&sem[i]);
      // O mapa cresce enquanto chega: a cada duas sementes o desenho ganha
      // estrelas novas em vez de esperar as oito.
      if (i % 2 == 1) publicar(sem, n, NULL, 0, vistos, nv, 1);
    }
  }
  // Sementes que nem o TMDB resolveu (sem titulo) saem.
  for (i = k = 0; i < n; i++) if (sem[i].obra.titulo[0]) sem[k++] = sem[i];
  n = k;

  // Fios: os creditos das pessoas que atravessam o mapa.
  { static Mapa prova;
    prova.revisao = 0;
    pthread_mutex_lock(&travaMontagem);
    mapa_cruzar(sem, n, NULL, 0, vistos, nv, &prova);
    pthread_mutex_unlock(&travaMontagem);
    for (i = 0; i < prova.nFios && nCred < MAPA_FIO_MAX; i++) {
      long pid = prova.fios[i].id;
      int achou = 0;
      if (pid <= 0) continue;
      for (k = 0; k < cache.nc; k++)
        if (cache.c[k].pessoa == pid && cache.c[k].n > 0) { cred[nCred++] = cache.c[k]; achou = 1; break; }
      if (!achou && lerCreditos(pid, prova.fios[i].direcao, &cred[nCred])) nCred++;
    } }

  publicar(sem, n, cred, nCred, vistos, nv, 0);

  // Regrava: as sementes de agora primeiro, depois o que o cache ja tinha e
  // ainda vale (uma semente que saiu hoje pode voltar amanha).
  { Cache novo;
    novo.s = calloc(CACHE_MAX, sizeof *novo.s);
    novo.c = calloc(CACHE_MAX, sizeof *novo.c);
    if (novo.s && novo.c) {
      novo.n = novo.nc = 0;
      for (i = 0; i < n && novo.n < CACHE_MAX; i++) if (sem[i].quando) novo.s[novo.n++] = sem[i];
      for (k = 0; k < cache.n && novo.n < CACHE_MAX; k++) {
        if (agora - cache.s[k].quando > CACHE_DIAS * 86400LL) continue;
        for (i = 0; i < novo.n; i++) if (novo.s[i].obra.tmdb == cache.s[k].obra.tmdb) break;
        if (i == novo.n) novo.s[novo.n++] = cache.s[k];
      }
      for (i = 0; i < nCred && novo.nc < CACHE_MAX; i++) novo.c[novo.nc++] = cred[i];
      for (k = 0; k < cache.nc && novo.nc < CACHE_MAX; k++) {
        for (i = 0; i < novo.nc; i++) if (novo.c[i].pessoa == cache.c[k].pessoa) break;
        if (i == novo.nc) novo.c[novo.nc++] = cache.c[k];
      }
      cacheGravar(&novo);
    }
    free(novo.s); free(novo.c); }

  free(cache.s); free(cache.c);
  printf("[mapa] %d sementes cruzadas, %d creditos\n", n, nCred); fflush(stdout);
  pthread_mutex_lock(&trava);
  fioVivo = 0;
  pthread_mutex_unlock(&trava);
  return NULL;
}

void mapa_pedir(void) {
  static MapaSemente sem[MAPA_SEM_MAX];
  static unsigned vistos[VISTOS_MAX];
  const char *chave = desc_chave_tmdb();
  int n, nv, comTmdb;
  unsigned ass;
  if (!chave || !chave[0]) chave = desc_chave_tmdb_reserva();
  comTmdb = chave && chave[0];

  n = coletar(sem, MAPA_SEM_MAX, vistos, &nv, comTmdb);
  ass = assinatura(sem, n);

  pthread_mutex_lock(&trava);
  if (fioVivo || (comTmdb && ass == assinaturaFeita && publicado.estado == MAPA_CRUZADO)) {
    // Ja cruzado para estas sementes (ou cruzando): nada a refazer.
    pthread_mutex_unlock(&trava);
    return;
  }
  pthread_mutex_unlock(&trava);

  // Retrato local imediato. Com TMDB, as sementes sem titulo (so id) ficam de
  // fora dele: aparecem quando o fio resolver.
  { static MapaSemente vis[MAPA_SEM_MAX];
    int i, k = 0;
    for (i = 0; i < n; i++) if (sem[i].obra.titulo[0]) vis[k++] = sem[i];
    recomendarDoCatalogo(vis, k);
    publicar(vis, k, NULL, 0, vistos, nv, comTmdb && n > 0); }

  if (!comTmdb || n == 0) return;
  pthread_mutex_lock(&trava);
  memcpy(semTrab, sem, sizeof semTrab);
  nSemTrab = n;
  memcpy(vistosTrab, vistos, sizeof vistosTrab);
  nVistosTrab = nv;
  snprintf(chaveTmdb, sizeof chaveTmdb, "%s", chave);
  snprintf(idiomaTmdb, sizeof idiomaTmdb, "%s", desc_tmdb_idioma());
  assinaturaFeita = ass;
  fioVivo = 1;
  pthread_mutex_unlock(&trava);
  { pthread_t fio;
    pthread_attr_t at;
    pthread_attr_init(&at);
    // Pilha explicita: o padrao do WebAssembly e pequeno. Os vetores grandes
    // do fio sao estaticos; a pilha so leva o parser e as URLs.
    pthread_attr_setstacksize(&at, 256 * 1024);
    if (pthread_create(&fio, &at, trabalhar, NULL) != 0) {
      // Sem fio, o retrato local fica — mas sem o "cruzando" eterno.
      pthread_mutex_lock(&trava);
      fioVivo = 0; assinaturaFeita = 0;
      publicado.carregando = 0; publicado.revisao = ++revisaoPub;
      pthread_mutex_unlock(&trava);
    } else pthread_detach(fio);
    pthread_attr_destroy(&at); }
}
#endif
