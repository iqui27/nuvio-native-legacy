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
#include "amigostitulo.h"
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
  // A serie nao traz imdb_id na raiz: vem em external_ids (pedido junto). E o
  // id com que os Salvos e o Detalhe conhecem o titulo.
  v = valorRaiz(json, NULL, "external_ids");
  if (v && *v == '{') {
    imdb[0] = 0;
    if (js_texto(v, js_fim(v), "imdb_id", imdb, sizeof imdb) && !strncmp(imdb, "tt", 2))
      snprintf(s->obra.imdb, sizeof s->obra.imdb, "%s", imdb);
  }

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
  // "espaço sideral", e nao "espaço": essa e a chave de i18n do ROTULO da tecla
  // de espaco (busca.c, spotlight.c, teclado.c), e com uma chave para os dois a
  // tecla saia "Weltraum"/"космос" (tests/espaco.sh).
  { "space", "espaço sideral" }, { "space travel", "viagem espacial" }, { "spy", "espiões" },
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

// ---------------------------------------------------------------------------
// 2b. VIZINHANCA DE UM TITULO (mapa.h). Puro: recebe tudo ja lido.

int mapa_ler_lista(const char *json, int serie, MapaObra *o, int max, int *total) {
  const char *p;
  int n = 0;
  if (total) *total = 0;
  if (!json || !o || max <= 0) return 0;
  if (total) *total = (int)numRaiz(json, "total_results", 0.0);
  p = js_array(json, NULL, "results");
  while (p && *p == '{' && n < max) {
    const char *f = js_fim(p);
    lerObraLista(p, f, &o[n], NULL, NULL);
    snprintf(o[n].tipo, sizeof o[n].tipo, "%s", serie ? "series" : "movie");
    if (o[n].tmdb > 0 && o[n].titulo[0] && o[n].poster[0]) n++;
    p = js_prox(f);
  }
  return n;
}

static int igualSemCaixa(const char *a, const char *b) {
  for (; *a && *b; a++, b++)
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
  return !*a && !*b;
}

long mapa_ler_keyword_id(const char *json, const char *nome) {
  const char *p;
  if (!json || !nome || !nome[0]) return 0;
  p = js_array(json, NULL, "results");
  while (p && *p == '{') {
    const char *f = js_fim(p);
    char n[64] = "";
    js_texto(p, f, "name", n, sizeof n);
    if (igualSemCaixa(n, nome)) return (long)js_num(p, f, "id", 0.0);
    p = js_prox(f);
  }
  return 0;
}

#define VIZ_USADOS 64
typedef struct { long long ch[VIZ_USADOS]; unsigned h[VIZ_USADOS]; int n; } VizUsados;

static int vizJa(const VizUsados *u, const MapaObra *o) {
  long long ch = chaveObra(o);
  unsigned h = mapa_hash_titulo(o->titulo);
  int i;
  for (i = 0; i < u->n; i++) if (u->ch[i] == ch || u->h[i] == h) return 1;
  return 0;
}

static void vizMarca(VizUsados *u, const MapaObra *o) {
  if (u->n >= VIZ_USADOS) return;
  u->ch[u->n] = chaveObra(o);
  u->h[u->n] = mapa_hash_titulo(o->titulo);
  u->n++;
}

static int vizPoe(const MapaVizEntrada *e, MapaVizGrupo *g, VizUsados *u,
                  const MapaObra *o, const char *motivo) {
  MapaVizItem *it;
  unsigned h;
  int i;
  if (g->n >= MAPA_VIZ_ITENS || !o->titulo[0] || !o->poster[0] || vizJa(u, o)) return 0;
  it = &g->itens[g->n++];
  it->obra = *o;
  snprintf(it->motivo, sizeof it->motivo, "%s", motivo ? motivo : "");
  h = mapa_hash_titulo(o->titulo);
  it->visto = 0;
  for (i = 0; i < e->nVistos; i++) if (e->vistos[i] == h) { it->visto = 1; break; }
  vizMarca(u, o);
  return 1;
}

static int candTemGente(const MapaVizCand *c, unsigned h) {
  int i;
  for (i = 0; i < c->nGente; i++) if (c->gente[i] == h) return 1;
  return 0;
}

static int candTemGen(const MapaVizCand *c, unsigned h) {
  int i;
  for (i = 0; i < c->nGen; i++) if (c->gen[i] == h) return 1;
  return 0;
}

static int candGenComum(const MapaVizCand *c, const MapaSemente *f) {
  int i, n = 0;
  for (i = 0; i < f->nGen; i++) if (candTemGen(c, mapa_hash_titulo(f->gen[i].nome))) n++;
  return n;
}

void mapa_vizinhos_montar(const MapaVizEntrada *e, MapaVizinhos *v) {
  const MapaSemente *f = e->foco;
  VizUsados u;
  MapaVizGrupo *g;
  unsigned rev = v->revisao;
  int i, k;
  memset(v, 0, sizeof *v);
  v->revisao = rev;
  if (!f) return;
  u.n = 0;
  v->foco = f->obra;
  vizMarca(&u, &f->obra);
  { unsigned h = mapa_hash_titulo(f->obra.titulo);
    for (i = 0; i < e->nVistos; i++) if (e->vistos[i] == h) v->focoVisto = 1; }
  for (i = 0; i < f->nGen && i < 3; i++) {
    size_t n = strlen(v->generos);
    snprintf(v->generos + n, sizeof v->generos - n, "%s%s", n ? "  \xc2\xb7  " : "", f->gen[i].nome);
  }

  // PESSOA. Os creditos do TMDB quando vieram; senao a pessoa do titulo que
  // aparece em mais titulos do catalogo; sem nenhuma em comum, a mesma decada.
  g = &v->g[MAPA_VIZ_PESSOA];
  g->tipo = MAPA_GR_PESSOA;
  if (e->cred && e->cred->n > 0 && e->credNome && e->credNome[0]) {
    snprintf(g->sub, sizeof g->sub, "%s", e->credNome);
    g->ref = e->cred->pessoa;
    for (i = 0; i < e->cred->n; i++) vizPoe(e, g, &u, &e->cred->obras[i], e->credNome);
  }
  if (!g->n) {
    int melhor = -1, melhorN = 0;
    for (i = 0; i < f->nGente; i++) {
      unsigned h = mapa_hash_titulo(f->gente[i].nome);
      int c = 0;
      for (k = 0; k < e->nPool; k++)
        if (candTemGente(&e->pool[k], h) && !vizJa(&u, &e->pool[k].obra)) c++;
      if (c > 0 && e->pessoaPref && h == e->pessoaPref) { melhor = i; break; }
      if (c > melhorN) { melhorN = c; melhor = i; }
    }
    if (melhor >= 0) {
      unsigned h = mapa_hash_titulo(f->gente[melhor].nome);
      snprintf(g->sub, sizeof g->sub, "%s", f->gente[melhor].nome);
      g->ref = f->gente[melhor].id;
      for (k = 0; k < e->nPool; k++)
        if (candTemGente(&e->pool[k], h)) vizPoe(e, g, &u, &e->pool[k].obra, g->sub);
    }
  }
  if (!g->n && f->obra.ano > 0) {
    int dec = (f->obra.ano / 10) * 10;
    char d[16];
    snprintf(d, sizeof d, "%d", dec);
    g->tipo = MAPA_GR_EPOCA;
    g->ref = 0;
    snprintf(g->sub, sizeof g->sub, "%s", d);
    // O mais proximo no tempo primeiro; a cada volta, o melhor que sobrou.
    while (g->n < MAPA_VIZ_ITENS) {
      const MapaObra *best = NULL;
      int bd = 1000;
      for (k = 0; k < e->nPool + f->nRec; k++) {
        const MapaObra *o = k < e->nPool ? &e->pool[k].obra : &f->rec[k - e->nPool].o;
        int dif;
        if (!o->ano || (o->ano / 10) * 10 != dec || !o->poster[0] || vizJa(&u, o)) continue;
        dif = abs(o->ano - f->obra.ano);
        if (dif < bd) { bd = dif; best = o; }
      }
      if (!best || !vizPoe(e, g, &u, best, d)) break;
    }
    if (!g->n) g->tipo = MAPA_GR_PESSOA;
  }

  // TEMA. A palavra-chave pelo /discover; sem ela, o genero do titulo que mais
  // se repete no catalogo.
  g = &v->g[MAPA_VIZ_TEMA];
  g->tipo = MAPA_GR_TEMA;
  if (e->nTema > 0 && e->temaNome && e->temaNome[0]) {
    snprintf(g->sub, sizeof g->sub, "%s", e->temaNome);
    snprintf(g->kw, sizeof g->kw, "%s", e->temaKw ? e->temaKw : "");
    g->ref = e->temaId;
    for (i = 0; i < e->nTema; i++) vizPoe(e, g, &u, &e->tema[i], e->temaNome);
  }
  if (!g->n) {
    int melhor = -1, melhorN = 0;
    for (i = 0; i < f->nGen; i++) {
      unsigned h = mapa_hash_titulo(f->gen[i].nome);
      int c = 0;
      for (k = 0; k < e->nPool; k++)
        if (candTemGen(&e->pool[k], h) && !vizJa(&u, &e->pool[k].obra)) c++;
      // O genero do fio que a pessoa vem seguindo ganha, se ainda rende.
      if (c > 0 && e->generoPref && h == e->generoPref) { melhor = i; break; }
      if (c > melhorN) { melhorN = c; melhor = i; }
    }
    if (melhor >= 0) {
      unsigned h = mapa_hash_titulo(f->gen[melhor].nome);
      g->tipo = MAPA_GR_GENERO;
      g->ref = 0;
      snprintf(g->sub, sizeof g->sub, "%s", f->gen[melhor].nome);
      // Quem divide MAIS generos com o titulo vem antes.
      while (g->n < MAPA_VIZ_ITENS) {
        const MapaObra *best = NULL;
        int bn = 0;
        for (k = 0; k < e->nPool; k++) {
          int c;
          if (!candTemGen(&e->pool[k], h) || vizJa(&u, &e->pool[k].obra)) continue;
          c = candGenComum(&e->pool[k], f);
          if (c > bn) { bn = c; best = &e->pool[k].obra; }
        }
        if (!best || !vizPoe(e, g, &u, best, g->sub)) break;
      }
    }
  }

  // RECOMENDADOS. Os do TMDB; senao generos em comum e ano perto, do catalogo
  // (e, sem genero nenhum em comum, o que o catalogo tem de mais bem avaliado:
  // a fileira nunca fica vazia com catalogo carregado).
  g = &v->g[MAPA_VIZ_REC];
  g->tipo = MAPA_GR_REC;
  for (i = 0; i < f->nRec; i++) vizPoe(e, g, &u, &f->rec[i].o, "");
  if (g->n) v->remoto = f->quando != 0;
  while (g->n < MAPA_VIZ_ITENS && !v->remoto) {
    const MapaObra *best = NULL;
    float bs = -1e9f;
    for (k = 0; k < e->nPool; k++) {
      const MapaObra *o = &e->pool[k].obra;
      float sc;
      if (vizJa(&u, o) || !o->poster[0]) continue;
      sc = (float)candGenComum(&e->pool[k], f) * 10.0f + (float)o->nota * 0.02f;
      if (o->ano && f->obra.ano) sc -= (float)abs(o->ano - f->obra.ano) * 0.25f;
      if (sc > bs) { bs = sc; best = o; }
    }
    if (!best || !vizPoe(e, g, &u, best, "")) break;
  }
  if (f->quando) v->remoto = 1;

  // AMIGOS. O que eles gostaram, o mais novo primeiro; `sub` leva os nomes.
  g = &v->g[MAPA_VIZ_AMIGOS];
  g->tipo = MAPA_GR_AMIGOS;
  for (i = 0; i < e->nAmigos; i++) {
    if (!vizPoe(e, g, &u, &e->amigos[i].obra, e->amigos[i].quem)) continue;
    if (!strstr(g->sub, e->amigos[i].quem)) {
      size_t n = strlen(g->sub);
      if (g->ref < 2) snprintf(g->sub + n, sizeof g->sub - n, "%s%s", n ? ", " : "", e->amigos[i].quem);
      g->ref++;
    }
  }
}

// ---------------------------------------------------------------------------
// 2c. CLIMAS. A tabela editorial e a regra de cada um.

static const struct { const char *nome; int filme, tv; } GEN_CANON[] = {
  { "action", MAPA_G_ACAO, 0 }, { "ação", MAPA_G_ACAO, 0 }, { "acción", MAPA_G_ACAO, 0 },
  { "adventure", MAPA_G_AVENTURA, 0 }, { "aventura", MAPA_G_AVENTURA, 0 },
  { "action & adventure", MAPA_G_ACAO, MAPA_G_AVENTURA },
  { "comedy", MAPA_G_COMEDIA, 0 }, { "comédia", MAPA_G_COMEDIA, 0 }, { "comedia", MAPA_G_COMEDIA, 0 },
  { "crime", MAPA_G_CRIME, 0 }, { "crimen", MAPA_G_CRIME, 0 },
  { "drama", MAPA_G_DRAMA, 0 },
  { "family", MAPA_G_FAMILIA, 0 }, { "família", MAPA_G_FAMILIA, 0 }, { "familia", MAPA_G_FAMILIA, 0 },
  { "fantasy", MAPA_G_FANTASIA, 0 }, { "fantasia", MAPA_G_FANTASIA, 0 }, { "fantasía", MAPA_G_FANTASIA, 0 },
  { "science fiction", MAPA_G_FICCAO, 0 }, { "sci-fi", MAPA_G_FICCAO, 0 },
  { "ficção científica", MAPA_G_FICCAO, 0 }, { "ciencia ficción", MAPA_G_FICCAO, 0 },
  { "sci-fi & fantasy", MAPA_G_FICCAO, MAPA_G_FANTASIA },
  { "mystery", MAPA_G_MISTERIO, 0 }, { "mistério", MAPA_G_MISTERIO, 0 }, { "misterio", MAPA_G_MISTERIO, 0 },
  { "horror", MAPA_G_TERROR, 0 }, { "terror", MAPA_G_TERROR, 0 },
  { "thriller", MAPA_G_SUSPENSE, 0 }, { "suspense", MAPA_G_SUSPENSE, 0 },
};

unsigned mapa_genero_mascara(const char *nome) {
  char b[64];
  size_t i, k = 0;
  if (!nome) return 0;
  while (*nome == ' ') nome++;
  // So o ASCII perde a caixa: as letras acentuadas da tabela ja estao em
  // minuscula e o catalogo manda "Ação", nunca "AÇÃO".
  for (; *nome && k + 1 < sizeof b; nome++)
    b[k++] = (*nome >= 'A' && *nome <= 'Z') ? (char)(*nome - 'A' + 'a') : *nome;
  while (k && b[k - 1] == ' ') k--;
  b[k] = 0;
  for (i = 0; i < sizeof GEN_CANON / sizeof GEN_CANON[0]; i++)
    if (!strcmp(GEN_CANON[i].nome, b))
      return (1u << GEN_CANON[i].filme) | (GEN_CANON[i].tv ? 1u << GEN_CANON[i].tv : 0u);
  return 0;
}

// Ids do TMDB de cada genero canonico. A TV nao tem Terror nem Suspense, e
// junta Acao/Aventura e Ficcao/Fantasia num genero so.
static const int GEN_TMDB[MAPA_G_N][2] = {
  { 0, 0 }, { 28, 10759 }, { 12, 10759 }, { 35, 35 }, { 80, 80 }, { 18, 18 },
  { 10751, 10751 }, { 14, 10765 }, { 878, 10765 }, { 9648, 9648 }, { 27, 0 }, { 53, 0 },
};

// COMO UM CLIMA SE DEFINE.
//   modo CLIMA_KW   pelas palavras-chave (qualquer uma). Um titulo do catalogo,
//                   que nao tem palavras, entra pela regra de generos: qualquer
//                   um de `gen` E, quando ha, tambem `genE`.
//   modo CLIMA_GEN  so pelo genero (qualquer um de `gen`).
//   modo CLIMA_AMBOS genero E palavra-chave no TMDB; no catalogo, o genero.
// `soSerie` restringe a series ("para maratonar").
enum { CLIMA_KW = 0, CLIMA_GEN, CLIMA_AMBOS };
#define CLIMA_KW_MAX 4
typedef struct {
  const char *nome, *desc;
  int modo, soSerie;
  int gen[3], genE;
  const char *kw[CLIMA_KW_MAX];
} ClimaRegra;

static const ClimaRegra CLIMAS[MAPA_CLIMA_N] = {
  { "Pra pensar depois", "Finais que ficam na cabeça", CLIMA_KW, 0,
    { MAPA_G_MISTERIO, MAPA_G_SUSPENSE, 0 }, MAPA_G_DRAMA,
    { "plot twist", "twist ending", "memory", "obsession" } },
  { "Tempo bagunçado", "Passado e futuro se misturando", CLIMA_KW, 0,
    { MAPA_G_FICCAO, 0, 0 }, MAPA_G_MISTERIO,
    { "time travel", "time loop", "parallel world", NULL } },
  { "Longe da Terra", "Espaço, solidão e primeiro contato", CLIMA_KW, 0,
    { MAPA_G_FICCAO, 0, 0 }, MAPA_G_AVENTURA,
    { "space", "space travel", "first contact", "astronaut" } },
  { "Sobreviver juntos", "Quando o grupo é a única saída", CLIMA_KW, 0,
    { MAPA_G_AVENTURA, MAPA_G_ACAO, 0 }, MAPA_G_DRAMA,
    { "survival", "survivor", "zombie apocalypse", NULL } },
  { "Cidade pequena, segredo grande", "Todo mundo se conhece, ninguém sabe nada", CLIMA_KW, 0,
    { MAPA_G_MISTERIO, 0, 0 }, MAPA_G_DRAMA,
    { "small town", "family secrets", NULL, NULL } },
  { "Família complicada", "Laços, luto e o que ficou por dizer", CLIMA_KW, 0,
    { MAPA_G_FAMILIA, 0, 0 }, MAPA_G_DRAMA,
    { "dysfunctional family", "father son relationship", "grief", "sibling relationship" } },
  { "Mistério para maratonar", "Séries com um porquê que não solta", CLIMA_GEN, 1,
    { MAPA_G_MISTERIO, 0, 0 }, 0, { NULL, NULL, NULL, NULL } },
  { "Crime e culpa", "Quem faz o quê, e a que preço", CLIMA_GEN, 0,
    { MAPA_G_CRIME, 0, 0 }, 0, { NULL, NULL, NULL, NULL } },
  { "Rir de nervoso", "Comédia com um pé na esquisitice", CLIMA_AMBOS, 0,
    { MAPA_G_COMEDIA, 0, 0 }, 0, { "dark comedy", "satire", "absurd", NULL } },
  { "Terror sem exagero", "Medo que vem da atmosfera", CLIMA_AMBOS, 0,
    { MAPA_G_TERROR, 0, 0 }, 0, { "supernatural", "haunted house", "psychological horror", NULL } },
  { "O mundo depois", "O que sobra quando algo dá errado", CLIMA_KW, 0,
    { MAPA_G_FICCAO, 0, 0 }, MAPA_G_DRAMA,
    { "post-apocalyptic future", "dystopia", "climate change", NULL } },
};

static const ClimaRegra *climaRegra(int id) {
  return id >= 0 && id < MAPA_CLIMA_N ? &CLIMAS[id] : NULL;
}

const char *mapa_clima_nome(int id) { const ClimaRegra *r = climaRegra(id); return r ? r->nome : ""; }
const char *mapa_clima_descricao(int id) { const ClimaRegra *r = climaRegra(id); return r ? r->desc : ""; }

int mapa_clima_n_kw(int id) {
  const ClimaRegra *r = climaRegra(id);
  int n = 0;
  if (!r) return 0;
  while (n < CLIMA_KW_MAX && r->kw[n]) n++;
  return n;
}

const char *mapa_clima_kw(int id, int i) {
  return i >= 0 && i < mapa_clima_n_kw(id) ? CLIMAS[id].kw[i] : NULL;
}

static int climaGeneros(const ClimaRegra *r, unsigned generos) {
  int i, algum = 0;
  for (i = 0; i < 3 && r->gen[i]; i++) if (generos & (1u << r->gen[i])) algum = 1;
  return algum && (!r->genE || (generos & (1u << r->genE)));
}

int mapa_clima_casa(int id, unsigned generos, const char *const *kws, int nKw, int serie) {
  const ClimaRegra *r = climaRegra(id);
  int i, k, temKw = 0;
  if (!r || (r->soSerie && !serie)) return 0;
  if (r->modo == CLIMA_GEN) return climaGeneros(r, generos);
  if (nKw <= 0 || !kws) return climaGeneros(r, generos);
  for (i = 0; i < nKw && !temKw; i++)
    for (k = 0; k < CLIMA_KW_MAX && r->kw[k]; k++)
      if (kws[i] && igualSemCaixa(kws[i], r->kw[k])) { temKw = 1; break; }
  if (r->modo == CLIMA_AMBOS) {
    int g = 0;
    for (i = 0; i < 3 && r->gen[i]; i++) if (generos & (1u << r->gen[i])) g = 1;
    return temKw && g;
  }
  return temKw;
}

int mapa_clima_consulta(int id, int serie, const long *ids, int nIds, char *dst, size_t n) {
  const ClimaRegra *r = climaRegra(id);
  size_t k = 0;
  int i, nk = 0, gen;
  if (!r || !dst || !n) return 0;
  dst[0] = 0;
  if (r->soSerie && !serie) return 0;
  gen = GEN_TMDB[r->gen[0]][serie ? 1 : 0];
  if (r->modo != CLIMA_KW) {
    // Genero que a TV nao tem: o clima so de genero nao existe para series; o
    // de genero + palavra segue so pelas palavras.
    if (!gen && r->modo == CLIMA_GEN) return 0;
    if (gen) k += (size_t)snprintf(dst + k, n - k, "with_genres=%d", gen);
  }
  if (r->modo != CLIMA_GEN) {
    for (i = 0; i < nIds && i < CLIMA_KW_MAX && k + 24 < n; i++) {
      if (!ids || ids[i] <= 0) continue;
      // "|" e OU no TMDB; vai escapado porque e caractere reservado em URL.
      k += (size_t)snprintf(dst + k, n - k, "%s%ld",
                            nk ? "%7C" : (k ? "&with_keywords=" : "with_keywords="), ids[i]);
      nk++;
    }
    if (!nk) { dst[0] = 0; return 0; }
  }
  return dst[0] != 0;
}

#ifndef NV_MAPA_PURO
// ---------------------------------------------------------------------------
// 3. ESTADO

#define CACHE_NOME   "explorar-mapa.txt"
#define CACHE_MAX    16
#define CACHE_DIAS   7
#define VISTOS_MAX   96

static void vizEsquecer(void);
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
  vizEsquecer();
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

// O MESMO formato serve ao mapa (explorar-mapa.txt) e a vizinhanca
// (explorar-viz.txt), que guarda mais duas coisas: Q da a data dos creditos de
// uma pessoa (no mapa eles valem enquanto a semente valer) e T abre o
// /discover de uma palavra-chave, com as obras em linhas O.
#define VIZ_TEMA_MAX 10
typedef struct { long kw; int serie; long long quando; int n; MapaObra o[VIZ_TEMA_MAX]; } VizTema;
typedef struct {
  MapaSemente *s; int n, maxS;
  MapaCreditos *c; long long *cq; int nc, maxC;   // cq pode ser NULL (mapa)
  VizTema *t; int nt, maxT;                       // t pode ser NULL (mapa)
} Cache;
// Fio solto com pilha explicita (o padrao do WebAssembly e pequeno; os vetores
// grandes dos fios sao estaticos). 1 = criou.
static int fioSolto(void *(*fn)(void *)) {
  pthread_t fio;
  pthread_attr_t at;
  int ok;
  pthread_attr_init(&at);
  pthread_attr_setstacksize(&at, 256 * 1024);
  ok = pthread_create(&fio, &at, fn, NULL) == 0;
  if (ok) pthread_detach(fio);
  pthread_attr_destroy(&at);
  return ok;
}

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

static void cacheLer(Cache *c, const char *nome, const char *idioma) {
  char *txt = dados_ler(nome), *linha, *prox;
  MapaSemente *atual = NULL;
  MapaCreditos *cr = NULL;
  VizTema *tm = NULL;
  long long quandoCred = 0;
  long pessoaQ = 0;
  c->n = c->nc = c->nt = 0;
  if (!txt) return;
  if (strncmp(txt, "NVMAPA 1\n", 9)) { free(txt); return; }
  // O IDIOMA DO TMDB e parte do cache (#187): titulos, sinopses e cartazes vem
  // no idioma pedido, e trocar o idioma dos metadados deixava a semana inteira
  // de mapa na lingua antiga. Cache sem a linha L (anterior a isto) cai tambem.
  { char esperado[40];
    snprintf(esperado, sizeof esperado, "L\t%s\n", idioma);
    if (strncmp(txt + 9, esperado, strlen(esperado))) { free(txt); return; } }
  for (linha = txt + 9; linha && *linha; linha = prox) {
    char *p;
    prox = strchr(linha, '\n');
    if (prox) *prox++ = 0;
    p = linha + 2;
    if (linha[1] != '\t') continue;
    if (linha[0] == 'S' && c->n < c->maxS) {
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
        if (c->nc >= c->maxC) { cr = NULL; continue; }
        if (c->cq) c->cq[c->nc] = pessoaQ == pid ? quandoCred : 0;
        cr = &c->c[c->nc++];
        memset(cr, 0, sizeof *cr);
        cr->pessoa = pid;
      }
      if (cr->n < MAPA_CRED_MAX) lerObraCampos(&p, &cr->obras[cr->n++]);
    } else if (linha[0] == 'Q') {
      pessoaQ = atol(campo(&p));
      quandoCred = atoll(campo(&p));
    } else if (linha[0] == 'T' && c->t) {
      tm = NULL;
      if (c->nt >= c->maxT) continue;
      tm = &c->t[c->nt++];
      memset(tm, 0, sizeof *tm);
      tm->kw = atol(campo(&p));
      tm->serie = atoi(campo(&p));
      tm->quando = atoll(campo(&p));
    } else if (linha[0] == 'O' && tm && tm->n < VIZ_TEMA_MAX) {
      lerObraCampos(&p, &tm->o[tm->n]);
      snprintf(tm->o[tm->n].tipo, sizeof tm->o[tm->n].tipo, "%s", tm->serie ? "series" : "movie");
      tm->n++;
    }
  }
  free(txt);
}

static size_t escreverObra(char *b, size_t n, const MapaObra *o) {
  return (size_t)snprintf(b, n, "%ld\t%s\t%d\t%d\t%d\t%s\t%s\t%s\t%s\n",
                          o->tmdb, o->tipo, o->ano, o->nota, o->votos,
                          o->titulo, o->poster, o->fundo, o->sinopse);
}

static void cacheGravar(const Cache *c, const char *nome, const char *idioma) {
  size_t cap = 64 * 1024 + (size_t)c->n * (sizeof *c->s + 1024u) +
               ((size_t)c->nc * MAPA_CRED_MAX + (size_t)c->nt * VIZ_TEMA_MAX) * (sizeof(MapaObra) + 128u), k = 0;
  char *b = malloc(cap);
  int i, j;
  if (!b) return;
  k += (size_t)snprintf(b + k, cap - k, "NVMAPA 1\nL\t%s\n", idioma);
#define ESPACO (k + sizeof(MapaObra) + 128u < cap)
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
  for (i = 0; i < c->nc && ESPACO; i++) {
    if (c->cq) k += (size_t)snprintf(b + k, cap - k, "Q\t%ld\t%lld\n", c->c[i].pessoa, c->cq[i]);
    for (j = 0; j < c->c[i].n && ESPACO; j++) {
      k += (size_t)snprintf(b + k, cap - k, "C\t%ld\t", c->c[i].pessoa);
      k += escreverObra(b + k, cap - k, &c->c[i].obras[j]);
    }
  }
  for (i = 0; c->t && i < c->nt && ESPACO; i++) {
    k += (size_t)snprintf(b + k, cap - k, "T\t%ld\t%d\t%lld\n", c->t[i].kw, c->t[i].serie, c->t[i].quando);
    for (j = 0; j < c->t[i].n && ESPACO; j++) {
      k += (size_t)snprintf(b + k, cap - k, "O\t");
      k += escreverObra(b + k, cap - k, &c->t[i].o[j]);
    }
  }
#undef ESPACO
  dados_gravar_leve(nome, b);
  free(b);
}

// --- o fio ---------------------------------------------------------------------

static long resolverTmdb(const char *imdb, char *tipo, size_t nTipo,
                         const char *chaveTmdb, const char *idiomaTmdb) {
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

// `lido` e estatico (18 KB fora da pilha do fio) e dois fios chegam aqui: o do
// mapa e o da vizinhanca.
static pthread_mutex_t travaLido = PTHREAD_MUTEX_INITIALIZER;
static int lerDoTmdb(MapaSemente *s, const char *chaveTmdb, const char *idiomaTmdb) {
  char url[500], *corpo;
  int serie, ok;
  if (!s->obra.tmdb && s->obra.imdb[0]) {
    char tipo[8] = "";
    s->obra.tmdb = resolverTmdb(s->obra.imdb, tipo, sizeof tipo, chaveTmdb, idiomaTmdb);
    if (tipo[0]) snprintf(s->obra.tipo, sizeof s->obra.tipo, "%s", tipo);
  }
  if (!s->obra.tmdb) return 0;
  serie = !strcmp(s->obra.tipo, "series");
  snprintf(url, sizeof url,
           "https://api.themoviedb.org/3/%s/%ld?api_key=%s&language=%s"
           "&append_to_response=keywords,credits,recommendations,external_ids",
           serie ? "tv" : "movie", s->obra.tmdb, chaveTmdb, idiomaTmdb);
  corpo = rede_baixar(url, 20);
  if (!corpo) return 0;
  {
    // A leitura substitui os dados locais (generos com id do TMDB, pessoas
    // com id real). O titulo e o poster do catalogo ficam se o TMDB nao tiver.
    static MapaSemente lido;   // 18 KB: fora da pilha do fio
    pthread_mutex_lock(&travaLido);
    lido = *s;
    lido.nGen = lido.nKw = lido.nGente = lido.nRec = 0;
    ok = mapa_ler_detalhe(corpo, serie, &lido);
    if (ok) { lido.quando = (long long)time(NULL); *s = lido; }
    pthread_mutex_unlock(&travaLido);
  }
  free(corpo);
  return ok;
}

static int lerCreditos(long pessoa, int direcao, MapaCreditos *c,
                       const char *chaveTmdb, const char *idiomaTmdb) {
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

  memset(&cache, 0, sizeof cache);
  cache.maxS = cache.maxC = CACHE_MAX;
  cache.s = calloc(CACHE_MAX, sizeof *cache.s);
  cache.c = calloc(CACHE_MAX, sizeof *cache.c);
  if (!cache.s || !cache.c) {
    free(cache.s); free(cache.c);
    pthread_mutex_lock(&trava); fioVivo = 0; pthread_mutex_unlock(&trava);
    return NULL;
  }
  cacheLer(&cache, CACHE_NOME, idiomaTmdb);

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
      lerDoTmdb(&sem[i], chaveTmdb, idiomaTmdb);
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
      if (!achou && lerCreditos(pid, prova.fios[i].direcao, &cred[nCred], chaveTmdb, idiomaTmdb)) nCred++;
    } }

  publicar(sem, n, cred, nCred, vistos, nv, 0);

  // Regrava: as sementes de agora primeiro, depois o que o cache ja tinha e
  // ainda vale (uma semente que saiu hoje pode voltar amanha).
  { Cache novo;
    memset(&novo, 0, sizeof novo);
    novo.maxS = novo.maxC = CACHE_MAX;
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
      cacheGravar(&novo, CACHE_NOME, idiomaTmdb);
    }
    free(novo.s); free(novo.c); }

  free(cache.s); free(cache.c);
  printf("[mapa] %d sementes cruzadas, %d creditos (tmdb language=%s)\n", n, nCred, idiomaTmdb); fflush(stdout);
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

// ---------------------------------------------------------------------------
// 4. VIZINHANCA: o estado (pedido no fio principal, TMDB num fio, cache).

#define VIZ_NOME     "explorar-viz.txt"
#define VIZ_DISCO    12      // titulos guardados em disco
#define VIZ_MEM      10      // retratos prontos em memoria (subir a trilha)
#define VIZ_POOL     160     // candidatos do catalogo por pedido
#define VIZ_AMIGOS   12
#define VIZ_VISTOS   256

static pthread_mutex_t travaViz = PTHREAD_MUTEX_INITIALIZER;
static MapaVizinhos vizPub;
static unsigned vizRevPub = 1;
static int vizFioVivo, vizPendente;
static unsigned vizSeq;                 // o pedido mais novo; resultado de outro e descartado
static unsigned vizHashPub;             // titulo do retrato publicado (vizChave)
static long vizPessoaPub;
static char vizTemaPub[48];
// O pedido, copiado pelo fio sob a trava.
static MapaSemente vizFocoTrab;
static MapaVizCand vizPoolTrab[VIZ_POOL];
static int vizNPool;
static MapaVizAmigo vizAmgTrab[VIZ_AMIGOS];
static int vizNAmg;
static unsigned vizVistosTrab[VIZ_VISTOS];
static int vizNVistos;
static long vizPessoaPref;
static char vizTemaPref[48];
static char vizChaveApi[96], vizIdioma[12];
static struct {
  unsigned h; long pessoa; char tema[48];
  unsigned uso;
  MapaVizinhos v;
} vizMem[VIZ_MEM];
static unsigned vizUso;

// Identidade de um titulo entre o catalogo e o TMDB: o nome e o tipo. O id nao
// serve — o mesmo titulo chega so com IMDb do catalogo e so com TMDB de uma
// lista de recomendados.
static unsigned vizChave(const MapaObra *o) {
  unsigned h = mapa_hash_titulo(o->titulo) ^ (!strcmp(o->tipo, "series") ? 0x9e3779b9u : 0u);
  return h ? h : 1;
}

int mapa_vizinhos_copiar(MapaVizinhos *dst, unsigned *revisao) {
  int copiou = 0;
  if (!dst || !revisao) return 0;
  pthread_mutex_lock(&travaViz);
  if (vizPub.revisao != *revisao) {
    *dst = vizPub;
    *revisao = vizPub.revisao;
    copiou = 1;
  }
  pthread_mutex_unlock(&travaViz);
  return copiou;
}

int mapa_obra_do_catalogo(int indice, MapaObra *o) {
  const CatItem *ci = cat_item(indice);
  if (!ci || !o || !ci->titulo[0]) return 0;
  obraDoItem(ci, indice, o);
  // "tt123:1:2" (o card de Continuar assistindo) e o titulo "tt123".
  { char *p = strchr(o->imdb, ':'); if (p) *p = 0; }
  if (ci->tmdb > 0 && !o->tmdb) o->tmdb = ci->tmdb;
  return 1;
}

static int candDoItem(const CatItem *ci, int indice, MapaVizCand *c) {
  static MapaSemente tmp;        // so para generosLocais; fio principal
  int i;
  char buf[128], *p, *q;
  c->nGen = c->nGente = 0;
  generosLocais(ci->genero, &tmp);
  for (i = 0; i < tmp.nGen; i++) c->gen[c->nGen++] = mapa_hash_titulo(tmp.gen[i].nome);
  snprintf(buf, sizeof buf, "%s", ci->direcao);
  for (p = buf; p && *p && c->nGente < MAPA_GENTE_MAX; p = q) {
    q = strchr(p, ',');
    if (q) *q++ = 0;
    while (*p == ' ') p++;
    if (*p) c->gente[c->nGente++] = mapa_hash_titulo(p);
  }
  for (i = 0; i < ci->nElenco && i < 6 && c->nGente < MAPA_GENTE_MAX; i++)
    if (ci->elenco[i].nome[0]) c->gente[c->nGente++] = mapa_hash_titulo(ci->elenco[i].nome);
  (void)indice;
  return 1;
}

// Os candidatos do catalogo para este titulo: primeiro quem divide genero,
// pessoa ou decada com ele; depois, ate o teto, o resto (a fileira de
// recomendados nunca fica vazia com catalogo carregado).
static int vizColetarPool(const MapaSemente *foco, MapaVizCand *pool, int max) {
  static MapaVizCand c;
  int n = 0, i, k, passo, total = cat_n();
  unsigned hf = mapa_hash_titulo(foco->obra.titulo);
  for (passo = 0; passo < 2 && n < max; passo++)
    for (i = 0; i < total && n < max; i++) {
      const CatItem *ci = cat_item(i);
      unsigned h;
      int perto = 0, ano;
      if (!ci || !ci->titulo[0] || !ci->poster[0]) continue;
      if (strcmp(ci->tipo, "movie") && strcmp(ci->tipo, "series")) continue;
      h = mapa_hash_titulo(ci->titulo);
      if (h == hf) continue;
      for (k = 0; k < n; k++) if (mapa_hash_titulo(pool[k].obra.titulo) == h) break;
      if (k < n) continue;
      candDoItem(ci, i, &c);
      ano = anoDoMeta(ci->meta);
      if (ano && foco->obra.ano && ano / 10 == foco->obra.ano / 10) perto = 1;
      for (k = 0; k < foco->nGen && !perto; k++)
        if (candTemGen(&c, mapa_hash_titulo(foco->gen[k].nome))) perto = 1;
      for (k = 0; k < foco->nGente && !perto; k++)
        if (candTemGente(&c, mapa_hash_titulo(foco->gente[k].nome))) perto = 1;
      if (perto != (passo == 0)) continue;
      obraDoItem(ci, i, &c.obra);
      pool[n++] = c;
    }
  return n;
}

// O que os amigos gostaram: os titulos do feed (socialvis) que o indice de
// amigostitulo marca com "gostou". O mais novo primeiro, um por titulo.
static int vizColetarAmigos(MapaVizAmigo *a, int max) {
  int n = 0, i, k, total;
  amigostitulo_atualizar();
  total = socialvis_n_eventos();
  for (i = 0; i < total && n < max; i++) {
    const SvEvento *ev = socialvis_evento(i);
    AmigosTitulo t;
    int idx;
    if (!ev || !ev->imdb[0] || !ev->titulo[0]) continue;
    if (!amigostitulo_obter(ev->imdb, &t) || t.nGostou <= 0 || t.n <= 0 || !t.a[0].gostou) continue;
    for (k = 0; k < n; k++) if (!strcmp(a[k].obra.imdb, ev->imdb)) break;
    if (k < n) continue;
    memset(&a[n], 0, sizeof a[n]);
    idx = itemPorImdb(ev->imdb);
    if (idx >= 0) obraDoItem(cat_item(idx), idx, &a[n].obra);
    else {
      snprintf(a[n].obra.imdb, sizeof a[n].obra.imdb, "%s", ev->imdb);
      snprintf(a[n].obra.tipo, sizeof a[n].obra.tipo, "%s", !strcmp(ev->tipo, "series") ? "series" : "movie");
      copiaLimpa(a[n].obra.titulo, sizeof a[n].obra.titulo, ev->titulo);
      snprintf(a[n].obra.poster, sizeof a[n].obra.poster, "%s", ev->poster);
      snprintf(a[n].obra.fundo, sizeof a[n].obra.fundo, "%s", ev->arte);
      a[n].obra.catIndice = -1;
    }
    if (!a[n].obra.poster[0]) continue;
    amigostitulo_primeiro_nome(t.a[0].nome, a[n].quem, sizeof a[n].quem);
    n++;
  }
  return n;
}

static int vizColetarVistos(unsigned *v, int max) {
  int n = 0, i, total = cat_n();
  for (i = 0; i < total && n < max; i++) {
    const CatItem *ci = cat_item(i);
    if (ci && ci->titulo[0] && (ci->progresso > 0 || cat_visto(ci))) v[n++] = mapa_hash_titulo(ci->titulo);
  }
  return n;
}

static void vizGuardarMem(const MapaVizinhos *v, long pessoa, const char *tema) {
  // Chamar com travaViz tomada.
  int i, alvo = 0;
  unsigned h = vizChave(&v->foco);
  for (i = 0; i < VIZ_MEM; i++) {
    if (vizMem[i].h == h && vizMem[i].pessoa == pessoa && !strcmp(vizMem[i].tema, tema)) { alvo = i; break; }
    if (vizMem[i].uso < vizMem[alvo].uso) alvo = i;
  }
  vizMem[alvo].h = h; vizMem[alvo].pessoa = pessoa;
  snprintf(vizMem[alvo].tema, sizeof vizMem[alvo].tema, "%s", tema);
  vizMem[alvo].uso = ++vizUso;
  vizMem[alvo].v = *v;
}

static void vizPublicar(MapaVizinhos *v, unsigned seq, int carregando, long pessoa, const char *tema) {
  pthread_mutex_lock(&travaViz);
  if (seq == vizSeq) {
    v->carregando = carregando;
    v->revisao = ++vizRevPub;
    vizPub = *v;
    vizHashPub = vizChave(&v->foco);
  }
  if (!carregando && v->remoto) vizGuardarMem(v, pessoa, tema);
  pthread_mutex_unlock(&travaViz);
}

static int listaDoTmdb(const char *caminho, int serie, const char *chave, const char *idioma,
                       MapaObra *o, int max, int *total) {
  char url[600], *corpo;
  int n;
  snprintf(url, sizeof url, "https://api.themoviedb.org/3/%s%sapi_key=%s&language=%s",
           caminho, strchr(caminho, '?') ? "&" : "?", chave, idioma);
  corpo = rede_baixar(url, 20);
  if (!corpo) return -1;
  n = mapa_ler_lista(corpo, serie, o, max, total);
  free(corpo);
  return n;
}

static void *vizTrabalhar(void *arg) {
  static MapaSemente foco;
  static MapaVizCand pool[VIZ_POOL];
  static MapaVizAmigo amg[VIZ_AMIGOS];
  static unsigned vistos[VIZ_VISTOS];
  static MapaVizinhos novo;
  static MapaCreditos cred;
  static VizTema tema;
  (void)arg;
  for (;;) {
    Cache ca;
    MapaVizEntrada e;
    char chave[96], idioma[12], temaPref[48], temaKw[48] = "";
    const char *temaNome = NULL;
    long pessoaPref, temaId = 0;
    unsigned seq;
    int nPool, nAmg, nVistos, i, k, gi = -1, temCred = 0, temTema = 0, mudou = 0, ok = 0, serie;
    long long agora = (long long)time(NULL);

    pthread_mutex_lock(&travaViz);
    if (!vizPendente) { vizFioVivo = 0; pthread_mutex_unlock(&travaViz); break; }
    vizPendente = 0;
    seq = vizSeq;
    foco = vizFocoTrab;
    nPool = vizNPool; memcpy(pool, vizPoolTrab, sizeof(MapaVizCand) * (size_t)nPool);
    nAmg = vizNAmg; memcpy(amg, vizAmgTrab, sizeof(MapaVizAmigo) * (size_t)nAmg);
    nVistos = vizNVistos; memcpy(vistos, vizVistosTrab, sizeof(unsigned) * (size_t)nVistos);
    pessoaPref = vizPessoaPref;
    snprintf(temaPref, sizeof temaPref, "%s", vizTemaPref);
    snprintf(chave, sizeof chave, "%s", vizChaveApi);
    snprintf(idioma, sizeof idioma, "%s", vizIdioma);
    pthread_mutex_unlock(&travaViz);

    memset(&ca, 0, sizeof ca);
    ca.maxS = ca.maxC = ca.maxT = VIZ_DISCO;
    ca.s = calloc(VIZ_DISCO, sizeof *ca.s);
    ca.c = calloc(VIZ_DISCO, sizeof *ca.c);
    ca.cq = calloc(VIZ_DISCO, sizeof *ca.cq);
    ca.t = calloc(VIZ_DISCO, sizeof *ca.t);
    if (ca.s && ca.c && ca.cq && ca.t) {
      cacheLer(&ca, VIZ_NOME, idioma);

      // 1. o titulo: cache de 7 dias, senao uma chamada ao TMDB.
      for (k = 0; k < ca.n && !ok; k++) {
        MapaSemente *c = &ca.s[k];
        if (!c->quando || agora - c->quando > CACHE_DIAS * 86400LL) continue;
        if ((foco.obra.imdb[0] && !strcmp(c->obra.imdb, foco.obra.imdb)) ||
            (foco.obra.tmdb && c->obra.tmdb == foco.obra.tmdb &&
             !strcmp(c->obra.tipo, foco.obra.tipo))) {
          MapaObra local = foco.obra;
          foco = *c;
          foco.obra.catIndice = local.catIndice;
          if (local.imdb[0]) snprintf(foco.obra.imdb, sizeof foco.obra.imdb, "%s", local.imdb);
          if (!foco.obra.poster[0]) snprintf(foco.obra.poster, sizeof foco.obra.poster, "%s", local.poster);
          if (!foco.obra.fundo[0]) snprintf(foco.obra.fundo, sizeof foco.obra.fundo, "%s", local.fundo);
          ok = 1;
        }
      }
      if (!ok && lerDoTmdb(&foco, chave, idioma)) { ok = 1; mudou = 1; }
    }

    if (ok) {
      serie = !strcmp(foco.obra.tipo, "series");
      // 2. a pessoa: a do fio que a pessoa vem seguindo, senao a primeira
      // (direcao/criacao vem antes do elenco em mapa_ler_detalhe).
      for (i = 0; i < foco.nGente; i++) if (pessoaPref && foco.gente[i].id == pessoaPref) gi = i;
      if (gi < 0 && foco.nGente > 0) gi = 0;
      if (gi >= 0 && foco.gente[gi].id > 0) {
        for (k = 0; k < ca.nc && !temCred; k++)
          if (ca.c[k].pessoa == foco.gente[gi].id && ca.c[k].n > 0 &&
              agora - ca.cq[k] <= CACHE_DIAS * 86400LL) { cred = ca.c[k]; temCred = 1; }
        if (!temCred && lerCreditos(foco.gente[gi].id, foco.gente[gi].direcao, &cred, chave, idioma) > 0) {
          temCred = 1; mudou = 1;
          // O mais novo na frente: o corte de VIZ_DISCO leva os mais antigos.
          if (ca.nc >= ca.maxC) ca.nc = ca.maxC - 1;
          memmove(&ca.c[1], &ca.c[0], sizeof *ca.c * (size_t)ca.nc);
          memmove(&ca.cq[1], &ca.cq[0], sizeof *ca.cq * (size_t)ca.nc);
          ca.c[0] = cred; ca.cq[0] = agora; ca.nc++;
        }
      }
      // 3. o tema: a palavra do fio, senao a primeira que tem traducao (as
      // "baseado em ..." por ultimo: dizem a origem, nao o assunto).
      { int ki = -1, passo;
        for (i = 0; i < foco.nKw; i++)
          if (temaPref[0] && !strcmp(foco.kw[i].nome, temaPref) && mapa_tema_nome(foco.kw[i].nome)) ki = i;
        for (passo = 0; passo < 2 && ki < 0; passo++)
          for (i = 0; i < foco.nKw && ki < 0; i++) {
            if (!mapa_tema_nome(foco.kw[i].nome)) continue;
            if (!passo && !strncmp(foco.kw[i].nome, "based on", 8)) continue;
            ki = i;
          }
        if (ki >= 0 && foco.kw[ki].id > 0) {
          temaId = foco.kw[ki].id;
          temaNome = mapa_tema_nome(foco.kw[ki].nome);
          snprintf(temaKw, sizeof temaKw, "%s", foco.kw[ki].nome);
          for (k = 0; k < ca.nt && !temTema; k++)
            if (ca.t[k].kw == temaId && ca.t[k].serie == serie && ca.t[k].n > 0 &&
                agora - ca.t[k].quando <= CACHE_DIAS * 86400LL) { tema = ca.t[k]; temTema = 1; }
          if (!temTema) {
            char cam[160];
            int n;
            snprintf(cam, sizeof cam, "discover/%s?with_keywords=%ld&sort_by=vote_count.desc",
                     serie ? "tv" : "movie", temaId);
            n = listaDoTmdb(cam, serie, chave, idioma, tema.o, VIZ_TEMA_MAX, NULL);
            if (n > 0) {
              tema.kw = temaId; tema.serie = serie; tema.quando = agora; tema.n = n;
              temTema = 1; mudou = 1;
              if (ca.nt >= ca.maxT) ca.nt = ca.maxT - 1;
              memmove(&ca.t[1], &ca.t[0], sizeof *ca.t * (size_t)ca.nt);
              ca.t[0] = tema; ca.nt++;
            }
          }
        }
      }
      memset(&e, 0, sizeof e);
      e.foco = &foco;
      if (temCred) { e.cred = &cred; e.credNome = foco.gente[gi].nome; }
      if (temTema) { e.tema = tema.o; e.nTema = tema.n; e.temaNome = temaNome; e.temaId = temaId; e.temaKw = temaKw; }
      e.pool = pool; e.nPool = nPool;
      e.amigos = amg; e.nAmigos = nAmg;
      e.vistos = vistos; e.nVistos = nVistos;
      novo.revisao = 0;
      mapa_vizinhos_montar(&e, &novo);
      vizPublicar(&novo, seq, 0, pessoaPref, temaPref);
      if (mudou) {
        // O titulo de agora primeiro; o corte leva o mais antigo.
        for (k = 0; k < ca.n; k++)
          if (ca.s[k].obra.tmdb == foco.obra.tmdb && !strcmp(ca.s[k].obra.tipo, foco.obra.tipo)) break;
        if (k == ca.n) { if (ca.n >= ca.maxS) ca.n = ca.maxS - 1; k = ca.n++; }
        memmove(&ca.s[1], &ca.s[0], sizeof *ca.s * (size_t)k);
        ca.s[0] = foco;
        cacheGravar(&ca, VIZ_NOME, idioma);
      }
      printf("[mapa] vizinhanca: %d pessoa, %d tema, %d rec, %d amigos%s\n",
             novo.g[MAPA_VIZ_PESSOA].n, novo.g[MAPA_VIZ_TEMA].n, novo.g[MAPA_VIZ_REC].n,
             novo.g[MAPA_VIZ_AMIGOS].n, mudou ? "" : " (cache)");
      fflush(stdout);
    } else {
      // Sem rede (ou o TMDB nao conhece o titulo): fica o retrato local, sem o
      // "cruzando" eterno.
      pthread_mutex_lock(&travaViz);
      if (seq == vizSeq) { vizPub.carregando = 0; vizPub.revisao = ++vizRevPub; }
      pthread_mutex_unlock(&travaViz);
    }
    free(ca.s); free(ca.c); free(ca.cq); free(ca.t);
  }
  return NULL;
}

void mapa_vizinhos_pedir(const MapaObra *obra, long pessoaPref, const char *temaPref) {
  static MapaSemente foco;
  static MapaVizCand pool[VIZ_POOL];
  static MapaVizAmigo amg[VIZ_AMIGOS];
  static unsigned vistos[VIZ_VISTOS];
  static MapaVizinhos local;
  MapaVizEntrada e;
  const char *chave = desc_chave_tmdb();
  unsigned h, seq;
  int i, idx, comTmdb, nPool, nAmg, nVistos, criar = 0;
  if (!obra || !obra->titulo[0]) return;
  if (!temaPref) temaPref = "";
  if (!chave || !chave[0]) chave = desc_chave_tmdb_reserva();
  comTmdb = chave && chave[0];
  h = vizChave(obra);

  pthread_mutex_lock(&travaViz);
  // O mesmo titulo, pelo mesmo fio, ja publicado (voltar do Detalhe): nada.
  if (vizPub.foco.titulo[0] && vizHashPub == h &&
      vizPessoaPub == pessoaPref && !strcmp(vizTemaPub, temaPref)) {
    pthread_mutex_unlock(&travaViz);
    return;
  }
  seq = ++vizSeq;
  vizPessoaPub = pessoaPref;
  snprintf(vizTemaPub, sizeof vizTemaPub, "%s", temaPref);
  // Pronto em memoria (subir um degrau da trilha): publica e acabou.
  for (i = 0; i < VIZ_MEM; i++)
    if (vizMem[i].uso && vizMem[i].h == h &&
        vizMem[i].pessoa == pessoaPref && !strcmp(vizMem[i].tema, temaPref)) {
      vizMem[i].uso = ++vizUso;
      vizPub = vizMem[i].v;
      vizPub.carregando = 0;
      vizPub.revisao = ++vizRevPub;
      vizHashPub = h;
      vizPendente = 0;
      pthread_mutex_unlock(&travaViz);
      return;
    }
  pthread_mutex_unlock(&travaViz);

  // O retrato local, na hora. O titulo vem do catalogo quando esta nele (traz
  // generos, direcao e elenco); senao so a obra.
  idx = obra->catIndice;
  { const CatItem *ci = idx >= 0 ? cat_item(idx) : NULL;
    unsigned ht = mapa_hash_titulo(obra->titulo);
    if (!ci || mapa_hash_titulo(ci->titulo) != ht) idx = itemPorImdb(obra->imdb);
    ci = idx >= 0 ? cat_item(idx) : NULL;
    if (ci && mapa_hash_titulo(ci->titulo) == ht) {
      sementeDoItem(ci, idx, MAPA_ORIGEM_VISTO, &foco);
      { char *p = strchr(foco.obra.imdb, ':'); if (p) *p = 0; }
      if (!foco.obra.tmdb) foco.obra.tmdb = obra->tmdb ? obra->tmdb : ci->tmdb;
    } else {
      memset(&foco, 0, sizeof foco);
      foco.obra = *obra;
      foco.obra.catIndice = -1;
    } }
  nPool = vizColetarPool(&foco, pool, VIZ_POOL);
  nAmg = vizColetarAmigos(amg, VIZ_AMIGOS);
  nVistos = vizColetarVistos(vistos, VIZ_VISTOS);
  memset(&e, 0, sizeof e);
  e.foco = &foco;
  e.pool = pool; e.nPool = nPool;
  e.amigos = amg; e.nAmigos = nAmg;
  e.vistos = vistos; e.nVistos = nVistos;
  // O fio na reserva local: a pessoa pelo nome (o id local e um hash dele), o
  // genero pelo proprio rotulo que o grupo mostrou.
  for (i = 0; i < foco.nGente && pessoaPref; i++)
    if (foco.gente[i].id == pessoaPref) e.pessoaPref = mapa_hash_titulo(foco.gente[i].nome);
  if (temaPref[0]) e.generoPref = mapa_hash_titulo(temaPref);
  local.revisao = 0;
  mapa_vizinhos_montar(&e, &local);
  vizPublicar(&local, seq, comTmdb, pessoaPref, temaPref);
  if (!comTmdb) return;

  pthread_mutex_lock(&travaViz);
  vizFocoTrab = foco;
  vizNPool = nPool; memcpy(vizPoolTrab, pool, sizeof(MapaVizCand) * (size_t)nPool);
  vizNAmg = nAmg; memcpy(vizAmgTrab, amg, sizeof(MapaVizAmigo) * (size_t)nAmg);
  vizNVistos = nVistos; memcpy(vizVistosTrab, vistos, sizeof(unsigned) * (size_t)nVistos);
  vizPessoaPref = pessoaPref;
  snprintf(vizTemaPref, sizeof vizTemaPref, "%s", temaPref);
  snprintf(vizChaveApi, sizeof vizChaveApi, "%s", chave);
  snprintf(vizIdioma, sizeof vizIdioma, "%s", desc_tmdb_idioma());
  vizPendente = 1;
  if (!vizFioVivo) { vizFioVivo = 1; criar = 1; }
  pthread_mutex_unlock(&travaViz);
  if (criar && !fioSolto(vizTrabalhar)) {
    pthread_mutex_lock(&travaViz);
    vizFioVivo = 0; vizPendente = 0;
    vizPub.carregando = 0; vizPub.revisao = ++vizRevPub;
    pthread_mutex_unlock(&travaViz);
  }
}

// ---------------------------------------------------------------------------
// 5. CLIMAS: o estado.

#define CLIMA_NOME     "explorar-climas.txt"
#define CLIMA_REMOTO   16
#define CLIMA_KW_TAB   48
#define CLIMA_TAB      4096    // potencia de 2: titulos distintos do catalogo

typedef struct { long long quando; int total, n; MapaObra o[CLIMA_REMOTO]; } ClimaRemoto;

static pthread_mutex_t travaClima = PTHREAD_MUTEX_INITIALIZER;
static MapaClimas climaPub, climaLocal;
static unsigned climaRevPub = 1;
static ClimaRemoto climaRem[MAPA_CLIMA_N];        // por id da tabela
static struct { char nome[40]; long id; } climaKw[CLIMA_KW_TAB];
static int nClimaKw;
static unsigned climaVistos[VIZ_VISTOS];
static int nClimaVistos;
static int climaFioVivo, climaCarregado, climaPedeId = -1, climaIndo = -1;
static char climaChaveApi[96], climaIdioma[12];

int mapa_climas_copiar(MapaClimas *dst, unsigned *revisao) {
  int copiou = 0;
  if (!dst || !revisao) return 0;
  pthread_mutex_lock(&travaClima);
  if (climaPub.revisao != *revisao) {
    *dst = climaPub;
    *revisao = climaPub.revisao;
    copiou = 1;
  }
  pthread_mutex_unlock(&travaClima);
  return copiou;
}

// Local + o que o /discover ja trouxe, e publica. Chamar com travaClima tomada.
static void climaFundir(void) {
  int p, i, k;
  unsigned rev = ++climaRevPub;
  climaPub = climaLocal;
  climaPub.revisao = rev;
  for (p = 0; p < climaPub.n; p++) {
    MapaClima *c = &climaPub.c[p];
    const ClimaRemoto *r = &climaRem[c->id];
    c->carregando = climaIndo == c->id;
    if (r->n <= 0) continue;
    c->remoto = 1;
    c->total += r->total > r->n ? r->total : r->n;
    for (i = 0; i < r->n && c->n < MAPA_CLIMA_ITENS; i++) {
      unsigned h = mapa_hash_titulo(r->o[i].titulo);
      int visto = 0;
      for (k = 0; k < c->n; k++) if (mapa_hash_titulo(c->itens[k].titulo) == h) break;
      if (k < c->n) continue;
      for (k = 0; k < nClimaVistos; k++) if (climaVistos[k] == h) visto = 1;
      c->itens[c->n] = r->o[i];
      c->visto[c->n] = (unsigned char)visto;
      c->n++;
      if (visto) c->vistos++;
    }
  }
}

static void climaCacheLer(const char *idioma) {
  char *txt = dados_ler(CLIMA_NOME), *linha, *prox;
  char esperado[40];
  ClimaRemoto *r = NULL;
  long long agora = (long long)time(NULL);
  if (!txt) return;
  snprintf(esperado, sizeof esperado, "NVCLIMA 1\nL\t%s\n", idioma);
  if (strncmp(txt, esperado, strlen(esperado))) { free(txt); return; }
  pthread_mutex_lock(&travaClima);
  for (linha = txt + strlen(esperado); linha && *linha; linha = prox) {
    char *p;
    prox = strchr(linha, '\n');
    if (prox) *prox++ = 0;
    if (linha[1] != '\t') continue;
    p = linha + 2;
    if (linha[0] == 'W' && nClimaKw < CLIMA_KW_TAB) {
      climaKw[nClimaKw].id = atol(campo(&p));
      snprintf(climaKw[nClimaKw].nome, sizeof climaKw[0].nome, "%s", campo(&p));
      if (climaKw[nClimaKw].nome[0]) nClimaKw++;
    } else if (linha[0] == 'M') {
      int id = atoi(campo(&p));
      long long q = atoll(campo(&p));
      int total = atoi(campo(&p));
      r = NULL;
      if (id < 0 || id >= MAPA_CLIMA_N || agora - q > CACHE_DIAS * 86400LL) continue;
      r = &climaRem[id];
      r->quando = q; r->total = total; r->n = 0;
    } else if (linha[0] == 'O' && r && r->n < CLIMA_REMOTO) {
      lerObraCampos(&p, &r->o[r->n++]);
    }
  }
  pthread_mutex_unlock(&travaClima);
  free(txt);
}

static void climaCacheGravar(const char *idioma) {
  size_t cap = 16 * 1024 + (size_t)MAPA_CLIMA_N * CLIMA_REMOTO * (sizeof(MapaObra) + 128u), k = 0;
  char *b = malloc(cap);
  int i, j;
  if (!b) return;
  k += (size_t)snprintf(b + k, cap - k, "NVCLIMA 1\nL\t%s\n", idioma);
  pthread_mutex_lock(&travaClima);
  for (i = 0; i < nClimaKw && k + 256 < cap; i++)
    k += (size_t)snprintf(b + k, cap - k, "W\t%ld\t%s\n", climaKw[i].id, climaKw[i].nome);
  for (i = 0; i < MAPA_CLIMA_N; i++) {
    if (climaRem[i].n <= 0) continue;
    k += (size_t)snprintf(b + k, cap - k, "M\t%d\t%lld\t%d\n", i, climaRem[i].quando, climaRem[i].total);
    for (j = 0; j < climaRem[i].n && k + sizeof(MapaObra) + 128u < cap; j++) {
      k += (size_t)snprintf(b + k, cap - k, "O\t");
      k += escreverObra(b + k, cap - k, &climaRem[i].o[j]);
    }
  }
  pthread_mutex_unlock(&travaClima);
  dados_gravar_leve(CLIMA_NOME, b);
  free(b);
}

// Id de uma palavra-chave: da tabela ja resolvida, senao /search/keyword. Uma
// palavra que o TMDB nao tem fica com id -1 na tabela (nao e pedida de novo).
static long climaIdKw(const char *nome, const char *chave, const char *idioma) {
  char url[400], esc[80], *corpo;
  size_t k = 0;
  long id = 0;
  int i;
  pthread_mutex_lock(&travaClima);
  for (i = 0; i < nClimaKw; i++) if (!strcmp(climaKw[i].nome, nome)) id = climaKw[i].id;
  pthread_mutex_unlock(&travaClima);
  if (id) return id;
  for (i = 0; nome[i] && k + 4 < sizeof esc; i++) {
    unsigned char c = (unsigned char)nome[i];
    if (isalnum(c) || c == '-') esc[k++] = (char)c;
    else k += (size_t)snprintf(esc + k, sizeof esc - k, "%%%02X", c);
  }
  esc[k] = 0;
  (void)idioma;
  snprintf(url, sizeof url, "https://api.themoviedb.org/3/search/keyword?api_key=%s&query=%s", chave, esc);
  corpo = rede_baixar(url, 15);
  if (!corpo) return 0;            // sem rede: tenta de novo na proxima abertura
  id = mapa_ler_keyword_id(corpo, nome);
  free(corpo);
  if (!id) id = -1;
  pthread_mutex_lock(&travaClima);
  if (nClimaKw < CLIMA_KW_TAB) {
    snprintf(climaKw[nClimaKw].nome, sizeof climaKw[0].nome, "%s", nome);
    climaKw[nClimaKw].id = id;
    nClimaKw++;
  }
  pthread_mutex_unlock(&travaClima);
  return id;
}

static void *climaTrabalhar(void *arg) {
  static MapaObra filmes[CLIMA_REMOTO], series[CLIMA_REMOTO];
  char chave[96], idioma[12];
  (void)arg;
  for (;;) {
    int id, carregar, nf = 0, ns = 0, tf = 0, ts = 0, i, falhou = 0;
    long ids[CLIMA_KW_MAX];
    char q[160], cam[260];
    long long agora = (long long)time(NULL);
    pthread_mutex_lock(&travaClima);
    carregar = !climaCarregado;
    id = climaPedeId;
    climaPedeId = -1;
    if (!carregar && id < 0) { climaFioVivo = 0; pthread_mutex_unlock(&travaClima); break; }
    snprintf(chave, sizeof chave, "%s", climaChaveApi);
    snprintf(idioma, sizeof idioma, "%s", climaIdioma);
    pthread_mutex_unlock(&travaClima);

    if (carregar) {
      climaCacheLer(idioma);
      pthread_mutex_lock(&travaClima);
      climaCarregado = 1;
      climaFundir();
      pthread_mutex_unlock(&travaClima);
    }
    if (id < 0 || id >= MAPA_CLIMA_N || !chave[0]) continue;
    pthread_mutex_lock(&travaClima);
    if (climaRem[id].n > 0 && agora - climaRem[id].quando <= CACHE_DIAS * 86400LL) {
      pthread_mutex_unlock(&travaClima);
      continue;                      // em cache: sem rede
    }
    climaIndo = id;
    climaFundir();
    pthread_mutex_unlock(&travaClima);

    for (i = 0; i < CLIMA_KW_MAX; i++) {
      const char *nome = mapa_clima_kw(id, i);
      ids[i] = nome ? climaIdKw(nome, chave, idioma) : 0;
      if (nome && !ids[i]) falhou = 1;
    }
    if (!falhou) {
      if (mapa_clima_consulta(id, 0, ids, CLIMA_KW_MAX, q, sizeof q)) {
        snprintf(cam, sizeof cam, "discover/movie?%s&sort_by=popularity.desc&vote_count.gte=150", q);
        nf = listaDoTmdb(cam, 0, chave, idioma, filmes, CLIMA_REMOTO, &tf);
      }
      if (mapa_clima_consulta(id, 1, ids, CLIMA_KW_MAX, q, sizeof q)) {
        snprintf(cam, sizeof cam, "discover/tv?%s&sort_by=popularity.desc&vote_count.gte=100", q);
        ns = listaDoTmdb(cam, 1, chave, idioma, series, CLIMA_REMOTO, &ts);
      }
    }
    pthread_mutex_lock(&travaClima);
    if (nf > 0 || ns > 0) {
      ClimaRemoto *r = &climaRem[id];
      int a = 0, b = 0;
      r->n = 0;
      // Filme e serie intercalados: os dois tipos aparecem na primeira tela.
      while (r->n < CLIMA_REMOTO && (a < nf || b < ns)) {
        if (a < nf) r->o[r->n++] = filmes[a++];
        if (b < ns && r->n < CLIMA_REMOTO) r->o[r->n++] = series[b++];
      }
      r->total = (nf > 0 ? tf : 0) + (ns > 0 ? ts : 0);
      r->quando = agora;
    }
    climaIndo = -1;
    climaFundir();
    pthread_mutex_unlock(&travaClima);
    if (nf > 0 || ns > 0) climaCacheGravar(idioma);
    printf("[mapa] clima %d: %d filmes, %d series do /discover\n", id, nf > 0 ? nf : 0, ns > 0 ? ns : 0);
    fflush(stdout);
  }
  return NULL;
}

static void climaAcordar(void) {
  // Chamar com travaClima tomada; solta e cria o fio fora dela.
  int criar = 0;
  if (!climaFioVivo) { climaFioVivo = 1; criar = 1; }
  pthread_mutex_unlock(&travaClima);
  if (criar && !fioSolto(climaTrabalhar)) {
    pthread_mutex_lock(&travaClima);
    climaFioVivo = 0; climaIndo = -1;
    pthread_mutex_unlock(&travaClima);
  }
}

void mapa_climas_pedir(void) {
  static MapaClimas novo;
  static unsigned tab[CLIMA_TAB];
  static unsigned vistos[VIZ_VISTOS];
  const char *chave = desc_chave_tmdb();
  int i, m, total = cat_n(), nVistos = 0, vistosTotal = 0, nSeen[MAPA_CLIMA_N], nUnseen[MAPA_CLIMA_N];
  if (!chave || !chave[0]) chave = desc_chave_tmdb_reserva();
  memset(&novo, 0, sizeof novo);
  memset(tab, 0, sizeof tab);
  memset(nSeen, 0, sizeof nSeen);
  memset(nUnseen, 0, sizeof nUnseen);
  novo.n = MAPA_CLIMA_N;
  for (m = 0; m < MAPA_CLIMA_N; m++) novo.c[m].id = m;
  for (i = 0; i < total; i++) {
    const CatItem *ci = cat_item(i);
    unsigned h, pos, gen = 0;
    int visto, serie, passos = 0;
    char buf[160], *p;
    if (!ci || !ci->titulo[0] || !ci->poster[0]) continue;
    serie = !strcmp(ci->tipo, "series");
    if (!serie && strcmp(ci->tipo, "movie")) continue;
    h = mapa_hash_titulo(ci->titulo);
    if (!h) h = 1;
    // O mesmo titulo vive em varias fileiras do catalogo: conta uma vez.
    for (pos = h & (CLIMA_TAB - 1); tab[pos] && tab[pos] != h && passos < CLIMA_TAB; passos++)
      pos = (pos + 1) & (CLIMA_TAB - 1);
    if (tab[pos] == h || passos >= CLIMA_TAB) continue;
    tab[pos] = h;
    visto = ci->progresso > 0 || cat_visto(ci);
    if (visto) { vistosTotal++; if (nVistos < VIZ_VISTOS) vistos[nVistos++] = h; }
    snprintf(buf, sizeof buf, "%s", ci->genero);
    for (p = buf; p && *p; ) {
      char *sep = strstr(p, "\xc2\xb7");
      if (sep) *sep = 0;
      gen |= mapa_genero_mascara(p);
      p = sep ? sep + 2 : NULL;
    }
    for (m = 0; m < MAPA_CLIMA_N; m++) {
      MapaClima *c = &novo.c[m];
      if (!mapa_clima_casa(m, gen, NULL, 0, serie)) continue;
      c->total++;
      if (visto) c->vistos++;
      // Metade para o que ela viu, metade para o que nao: as duas fileiras do
      // clima aberto tem de onde tirar.
      if (c->n >= MAPA_CLIMA_ITENS || (visto ? nSeen[m] : nUnseen[m]) >= MAPA_CLIMA_ITENS / 2) continue;
      obraDoItem(ci, i, &c->itens[c->n]);
      { char *d = strchr(c->itens[c->n].imdb, ':'); if (d) *d = 0; }
      c->visto[c->n] = (unsigned char)visto;
      c->n++;
      if (visto) nSeen[m]++; else nUnseen[m]++;
    }
  }
  for (m = 0; m < MAPA_CLIMA_N; m++)
    novo.c[m].afinidade = vistosTotal > 0 ? (novo.c[m].vistos * 100 + vistosTotal / 2) / vistosTotal : 0;
  // Ordem de afinidade; empate fica na ordem editorial (insercao estavel).
  for (i = 1; i < MAPA_CLIMA_N; i++) {
    static MapaClima t;
    t = novo.c[i];
    for (m = i; m > 0 && novo.c[m - 1].afinidade < t.afinidade; m--) novo.c[m] = novo.c[m - 1];
    novo.c[m] = t;
  }
  pthread_mutex_lock(&travaClima);
  climaLocal = novo;
  memcpy(climaVistos, vistos, sizeof vistos);
  nClimaVistos = nVistos;
  snprintf(climaChaveApi, sizeof climaChaveApi, "%s", chave ? chave : "");
  snprintf(climaIdioma, sizeof climaIdioma, "%s", desc_tmdb_idioma());
  climaFundir();
  if (!climaCarregado) { climaAcordar(); return; }   // le o cache do disco fora do fio principal
  pthread_mutex_unlock(&travaClima);
}

void mapa_clima_abrir(int id) {
  if (id < 0 || id >= MAPA_CLIMA_N) return;
  pthread_mutex_lock(&travaClima);
  if (!climaChaveApi[0]) { pthread_mutex_unlock(&travaClima); return; }
  climaPedeId = id;
  climaAcordar();
}

static void vizEsquecer(void) {
  pthread_mutex_lock(&travaViz);
  memset(&vizPub, 0, sizeof vizPub);
  vizPub.revisao = ++vizRevPub;
  memset(vizMem, 0, sizeof vizMem);
  vizSeq++; vizPendente = 0;
  vizHashPub = 0;
  pthread_mutex_unlock(&travaViz);
  pthread_mutex_lock(&travaClima);
  memset(climaRem, 0, sizeof climaRem);
  memset(&climaLocal, 0, sizeof climaLocal);
  nClimaVistos = 0;
  climaFundir();
  pthread_mutex_unlock(&travaClima);
  dados_apagar(VIZ_NOME);
  dados_apagar(CLIMA_NOME);
}
#endif
