// Ver partes.h para o porque.
#include "partes.h"
#include "debrid.h"
#include "dados.h"
#include "js.h"
#include "rede.h"
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

int partes_disponivel(void) {
#ifdef NV_TPK
  return 1;
#else
  return 0;
#endif
}

// ---------------------------------------------------------------- JSON
//
// O valor de `chave` NA RAIZ do objeto que comeca em `ob` ('{'). O item do
// mylist tem "id", "hash" e "name" na raiz e de novo dentro de cada arquivo:
// js_texto/js_num pegam o primeiro que aparece, que pode ser o do arquivo.
static const char *raiz(const char *ob, const char *fim, const char *chave) {
  size_t L = strlen(chave);
  int prof = 0;
  const char *p = ob;
  if (!p || *p != '{') return NULL;
  if (!fim) fim = p + strlen(p);
  for (; p < fim && *p; p++) {
    char c = *p;
    if (c == '"') {
      const char *ini = p + 1, *q = ini;
      while (q < fim && *q && *q != '"') { if (*q == '\\' && q + 1 < fim && q[1]) q++; q++; }
      if (q >= fim || !*q) return NULL;
      if (prof == 1 && (size_t)(q - ini) == L && !memcmp(ini, chave, L)) {
        const char *v = q + 1;
        while (v < fim && isspace((unsigned char)*v)) v++;
        if (v < fim && *v == ':') {
          v++;
          while (v < fim && isspace((unsigned char)*v)) v++;
          return v < fim ? v : NULL;
        }
      }
      p = q;
      continue;
    }
    if (c == '{' || c == '[') prof++;
    else if (c == '}' || c == ']') { if (--prof <= 0) return NULL; }
  }
  return NULL;
}
static int textoRaiz(const char *ob, const char *fim, const char *chave, char *dst, size_t n) {
  const char *v = raiz(ob, fim, chave);
  if (n) dst[0] = 0;
  return v && *v == '"' && js_cadeia(v, dst, n);
}
static double numRaiz(const char *ob, const char *fim, const char *chave, double padrao) {
  const char *v = raiz(ob, fim, chave);
  char *e;
  double d;
  if (!v || !(*v == '-' || isdigit((unsigned char)*v))) return padrao;
  d = strtod(v, &e);
  return e != v && isfinite(d) ? d : padrao;
}

// ---------------------------------------------------------------- puro

static int ehHex(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

int partes_hashes_texto(const char *s, char saida[][41], int max) {
  int k = 0;
  if (!s) return 0;
  for (const char *p = s; *p && k < max; ) {
    if (!ehHex((unsigned char)*p)) { p++; continue; }
    const char *q = p;
    while (*q && ehHex((unsigned char)*q)) q++;
    if (q - p == 40) {
      int dup = 0;
      for (int i = 0; i < 40; i++) saida[k][i] = (char)tolower((unsigned char)p[i]);
      saida[k][40] = 0;
      for (int j = 0; j < k && !dup; j++) dup = !strcmp(saida[j], saida[k]);
      if (!dup) k++;
    }
    p = q;
  }
  return k;
}

static const char *base(const char *caminho) {
  const char *b = caminho;
  for (const char *p = caminho; *p; p++) if (*p == '/' || *p == '\\') b = p + 1;
  return b;
}

static int ehVideo(const char *nome) {
  static const char *ext[] = { ".mkv", ".mp4", ".m4v", ".avi", ".mov", ".ts", ".m2ts", ".webm", ".wmv", NULL };
  size_t L = strlen(nome);
  for (int i = 0; ext[i]; i++) {
    size_t e = strlen(ext[i]);
    if (L > e && !strcasecmp(nome + L - e, ext[i])) return 1;
  }
  return 0;
}

// Palavra inteira no nome (sem letra/numero dos dois lados): "nced" casa em
// "[NCED]" e em "NCED01", mas "cm" nao casa em "cm01"? Casa — e o "CM01" dos
// discos de anime e propaganda mesmo. Por isso a lista so tem o que e extra
// em qualquer contexto.
static int temPalavra(const char *nome, const char *w) {
  size_t L = strlen(w);
  for (const char *p = nome; *p; p++) {
    if (strncasecmp(p, w, L)) continue;
    if (p > nome && isalpha((unsigned char)p[-1])) continue;
    if (isalpha((unsigned char)p[L])) continue;
    return 1;
  }
  return 0;
}
static int nomeDeExtra(const char *caminho) {
  static const char *w[] = { "sample", "trailer", "ncop", "nced", "creditless", "menu", "extras",
                             "bonus", "featurette", "featurettes", "preview", "teaser", "promo",
                             "interview", "making", "behind the scenes", "deleted scene",
                             "special features", "cm", "pv", "spot", NULL };
  for (int i = 0; w[i]; i++) if (temPalavra(caminho, w[i])) return 1;
  return 0;
}

// O caminho sem a pasta do torrent (o primeiro componente, quando ha pastas):
// "[KH] Baka and Test S1+S2+OVAs+SP/..." nao pode fazer todo arquivo virar
// especial por causa do nome do pacote.
static const char *semRaiz(const char *caminho) {
  const char *b = strchr(caminho, '/');
  if (!b) b = strchr(caminho, '\\');
  return b && b[1] ? b + 1 : caminho;
}
// "S00E03", "S0E3", "Season 0", pasta "Specials".
static int temporadaZero(const char *s) {
  for (const char *p = s; *p; p++) {
    if ((p == s || !isalnum((unsigned char)p[-1])) && tolower((unsigned char)p[0]) == 's') {
      const char *q = p + 1;
      if (*q == '0') { while (*q == '0') q++; if (tolower((unsigned char)*q) == 'e' && isdigit((unsigned char)q[1])) return 1; }
    }
    if (!strncasecmp(p, "season", 6)) {
      const char *q = p + 6;
      while (*q == ' ' || *q == '.' || *q == '_' || *q == '-') q++;
      if (*q == '0' && !isdigit((unsigned char)q[1])) return 1;
    }
  }
  return 0;
}
static int nomeDeEspecial(const char *rel) {
  static const char *w[] = { "ova", "ovas", "oad", "oads", "ona", "sp", "sps", "special", "specials",
                             "movie", "recap", "picture drama", NULL };
  for (int i = 0; w[i]; i++) if (temPalavra(rel, w[i])) return 1;
  return temporadaZero(rel);
}

static int numeroEm(const char *p, int max, int *usou) {
  int v = 0, k = 0;
  while (isdigit((unsigned char)p[k]) && k < 4) v = v * 10 + (p[k++] - '0');
  *usou = k;
  return k > 0 && k <= max ? v : -1;
}
// TEMPORADA E EPISODIO pelo nome: "S01E05", "1x05", "Season 2/...", "2nd
// Season", " S2 ", "E05", "EP05", "Episode 5", " - 05" (o jeito dos fansubs).
void partes_temporada_episodio(const char *nome, int *temporada, int *episodio) {
  const char *b = nome ? base(nome) : "";
  int t = 0, e = 0, u;
  if (temporada) *temporada = 0;
  if (episodio) *episodio = 0;
  if (!nome) return;
  for (const char *p = nome; *p; p++) {
    int antes = p == nome || !isalnum((unsigned char)p[-1]);
    if (antes && tolower((unsigned char)*p) == 's' && isdigit((unsigned char)p[1])) {
      int v = numeroEm(p + 1, 2, &u);
      const char *q = p + 1 + u;
      if (v >= 0 && tolower((unsigned char)*q) == 'e' && isdigit((unsigned char)q[1])) {
        int ep = numeroEm(q + 1, 3, &u);
        if (ep > 0) { t = v; e = ep; break; }
      }
      if (v > 0 && !isalnum((unsigned char)*q) && !t) t = v;
    }
    if (antes && isdigit((unsigned char)*p)) {
      int v = numeroEm(p, 2, &u);
      const char *q = p + u;
      if (v > 0 && tolower((unsigned char)*q) == 'x' && isdigit((unsigned char)q[1])) {
        int ep = numeroEm(q + 1, 3, &u);
        if (ep > 0 && !isdigit((unsigned char)q[1 + u]) && u >= 2) { t = v; e = ep; break; }
      }
      if (v > 0 && (!strncasecmp(q, "st", 2) || !strncasecmp(q, "nd", 2) || !strncasecmp(q, "rd", 2) || !strncasecmp(q, "th", 2))) {
        const char *r = q + 2;
        while (*r == ' ' || *r == '.' || *r == '_' || *r == '-') r++;
        if (!strncasecmp(r, "season", 6) && !t) t = v;
      }
    }
    if (antes && !strncasecmp(p, "season", 6)) {
      const char *q = p + 6;
      int v;
      while (*q == ' ' || *q == '.' || *q == '_' || *q == '-') q++;
      v = numeroEm(q, 2, &u);
      if (v > 0 && !t) t = v;
    }
  }
  if (!e) {
    for (const char *p = b; *p && !e; p++) {
      int antes = p == b || !isalnum((unsigned char)p[-1]);
      if (antes && !strncasecmp(p, "episode", 7)) {
        const char *q = p + 7; while (*q == ' ' || *q == '.' || *q == '_') q++;
        int v = numeroEm(q, 3, &u); if (v > 0) e = v;
      } else if (antes && !strncasecmp(p, "ep", 2) && (isdigit((unsigned char)p[2]) || ((p[2] == ' ' || p[2] == '.') && isdigit((unsigned char)p[3])))) {
        int v = numeroEm(p + (isdigit((unsigned char)p[2]) ? 2 : 3), 3, &u); if (v > 0) e = v;
      } else if (antes && tolower((unsigned char)*p) == 'e' && isdigit((unsigned char)p[1])) {
        int v = numeroEm(p + 1, 3, &u); if (v > 0 && !isalnum((unsigned char)p[1 + u])) e = v;
      } else if (p[0] == ' ' && p[1] == '-' && p[2] == ' ' && isdigit((unsigned char)p[3])) {
        int v = numeroEm(p + 3, 3, &u);
        if (v > 0 && !isalnum((unsigned char)p[3 + u]) && p[3 + u] != '.') e = v;
        else if (v > 0 && p[3 + u] == '.' && !isdigit((unsigned char)p[4 + u])) e = v;   // " - 05.mkv"
      }
    }
  }
  if (temporada) *temporada = t;
  if (episodio) *episodio = e;
}

// ORDEM NATURAL: "2" antes de "10", caixa ignorada.
static int natural(const char *a, const char *b) {
  while (*a && *b) {
    if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
      unsigned long long x = 0, y = 0;
      while (isdigit((unsigned char)*a)) x = x * 10 + (unsigned)(*a++ - '0');
      while (isdigit((unsigned char)*b)) y = y * 10 + (unsigned)(*b++ - '0');
      if (x != y) return x < y ? -1 : 1;
      continue;
    }
    int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
    if (ca != cb) return ca < cb ? -1 : 1;
    a++; b++;
  }
  return (unsigned char)*a - (unsigned char)*b;
}

typedef struct { ParteArq a; char caminho[512]; } ArqTmp;
// Episodios, depois especiais, depois extras; dentro de cada um a ordem
// natural do caminho — e a temporada primeiro quando TODO episodio diz a sua.
static int ordemPorTemporada;   // sob tmpTrava
static int cmpArq(const void *x, const void *y) {
  const ArqTmp *a = x, *b = y;
  if (a->a.tipo != b->a.tipo) return a->a.tipo - b->a.tipo;
  if (ordemPorTemporada && a->a.tipo == PARTE_EP && a->a.temporada != b->a.temporada)
    return a->a.temporada - b->a.temporada;
  return natural(a->caminho, b->caminho);
}

void partes_lista_limpar(PartesLista *l) {
  free(l->arq);
  memset(l, 0, sizeof *l);
}
int partes_lista_copiar(PartesLista *dst, const PartesLista *src) {
  if (dst == src) return 1;
  ParteArq *arq = src->n ? malloc((size_t)src->n * sizeof *arq) : NULL;
  if (src->n && !arq) { partes_lista_limpar(dst); dst->estado = PARTES_ERRO; return 0; }
  if (src->n) memcpy(arq, src->arq, (size_t)src->n * sizeof *arq);
  partes_lista_limpar(dst);
  *dst = *src;
  dst->arq = arq;
  return 1;
}

int partes_ler_item(const char *item, const char *fim, int usenet, PartesLista *l) {
  ArqTmp *tmp = NULL;
  int cap = 0;
  static pthread_mutex_t tmpTrava = PTHREAD_MUTEX_INITIALIZER;
  const char *fa, *el;
  int n = 0;
  uint64_t maior = 0;
  if (!item || *item != '{' || !l) return 0;
  if (!fim) fim = js_fim(item);
  partes_lista_limpar(l);
  l->atual = -1;
  l->usenet = usenet;
  { const char *a = raiz(item, fim, "airlocked"); l->airlock = a && (!strncmp(a, "true", 4) || (*a == '1' && (a[1] == ',' || a[1] == '}' || isspace((unsigned char)a[1]))) || !strncmp(a, "\"true\"", 6)); }
  l->item = (int)numRaiz(item, fim, "id", -1);
  if (l->item <= 0) return 0;
  { char h[64] = "";
    if (textoRaiz(item, fim, "hash", h, sizeof h) && strlen(h) == 40)
      for (int i = 0; i <= 40; i++) l->hash[i] = (char)tolower((unsigned char)h[i]); }
  fa = raiz(item, fim, "files");
  if (!fa || *fa != '[') return 0;
  pthread_mutex_lock(&tmpTrava);
  el = fa + 1;
  while (el < fim && isspace((unsigned char)*el)) el++;
  for (; el && el < fim && *el == '{'; el = js_prox(js_fim(el))) {
    const char *ef = js_fim(el);
    char nome[512] = "", curto[PARTES_NOME] = "";
    double tam, id;
    if (!textoRaiz(el, ef, "name", nome, sizeof nome)) continue;
    if (!textoRaiz(el, ef, "short_name", curto, sizeof curto) || !curto[0])
      snprintf(curto, sizeof curto, "%s", base(nome));
    if (!ehVideo(curto)) continue;
    id = numRaiz(el, ef, "id", -1);
    if (id < 0) continue;
    tam = numRaiz(el, ef, "size", 0);
    if (n == cap) {
      int next = cap ? cap * 2 : 64;
      ArqTmp *grown = realloc(tmp, (size_t)next * sizeof *tmp);
      if (!grown) { free(tmp); pthread_mutex_unlock(&tmpTrava); return 0; }
      tmp = grown; cap = next;
    }
    memset(&tmp[n], 0, sizeof tmp[n]);
    tmp[n].a.id = (int)id;
    tmp[n].a.bytes = tam > 0 ? (uint64_t)tam : 0;
    snprintf(tmp[n].a.nome, sizeof tmp[n].a.nome, "%s", curto);
    snprintf(tmp[n].caminho, sizeof tmp[n].caminho, "%s", nome);
    { const char *rel = semRaiz(nome);
      tmp[n].a.tipo = nomeDeEspecial(rel) ? PARTE_ESPECIAL : nomeDeExtra(rel) ? PARTE_EXTRA : PARTE_EP;
      tmp[n].a.temporadaZero = temporadaZero(rel);
      partes_temporada_episodio(rel, &tmp[n].a.temporada, &tmp[n].a.episodio); }
    if (tmp[n].a.bytes > maior) maior = tmp[n].a.bytes;
    n++;
  }
  // Bem menor que o maior arquivo (um quinto) tambem e extra: abertura sem
  // creditos, previa, menu. Uma parte de filme nunca e tao menor que a maior.
  // O nome so vale com o tamanho junto: "The Menu (2022).mkv" e o filme, e
  // nenhum extra tem a metade do tamanho do maior arquivo. Especial curto
  // (SP de 5 min) continua especial.
  { int eps = 0, todosComTemporada = 1;
    for (int i = 0; i < n; i++) {
      ParteArq *a = &tmp[i].a;
      if (a->tipo == PARTE_EXTRA && maior > 0 && a->bytes * 2 >= maior) a->tipo = PARTE_EP;
      if (a->tipo == PARTE_EP && maior > 0 && a->bytes > 0 && a->bytes * 5 < maior) a->tipo = PARTE_EXTRA;
      eps += a->tipo == PARTE_EP;
    }
    // Tudo "especial" (um filme cujo nome diz Movie): sao as partes.
    if (!eps) for (int i = 0; i < n; i++) if (tmp[i].a.tipo == PARTE_ESPECIAL) tmp[i].a.tipo = PARTE_EP;
    for (int i = 0; i < n; i++) {
      tmp[i].a.extra = tmp[i].a.tipo != PARTE_EP;
      if (tmp[i].a.tipo == PARTE_EP && tmp[i].a.temporada <= 0) todosComTemporada = 0;
    }
    ordemPorTemporada = todosComTemporada; }
  qsort(tmp, (size_t)n, sizeof tmp[0], cmpArq);
  l->arq = n ? malloc((size_t)n * sizeof *l->arq) : NULL;
  if (n && !l->arq) { free(tmp); pthread_mutex_unlock(&tmpTrava); return 0; }
  for (int i = 0; i < n; i++) l->arq[i] = tmp[i].a;
  free(tmp);
  pthread_mutex_unlock(&tmpTrava);
  l->n = n;
  return n > 0;
}

// O arquivo da fonte dentro da lista: pelo nome (e tamanho, quando ha dois
// com o mesmo nome em pastas diferentes), pelo tamanho exato, pelo numero do
// episodio do nome da fonte; sem nada disso, o maior so num item de um video
// principal (num pacote, "o maior" e um episodio qualquer).
static int arquivoDaFonte(const PartesLista *l, const char *arquivo, uint64_t bytes) {
  int achou = -1, maior = -1, eps = 0, t = 0, e = 0;
  const char *b = arquivo && *arquivo ? base(arquivo) : "";
  for (int i = 0; i < l->n; i++) {
    if (b[0] && !strcasecmp(l->arq[i].nome, b)) {
      if (achou < 0 || (bytes && l->arq[i].bytes == bytes)) achou = i;
    }
    if (maior < 0 || l->arq[i].bytes > l->arq[maior].bytes) maior = i;
    eps += !l->arq[i].extra;
  }
  if (achou < 0 && bytes)
    for (int i = 0; i < l->n && achou < 0; i++) if (l->arq[i].bytes == bytes) achou = i;
  if (achou < 0 && b[0]) {
    partes_temporada_episodio(b, &t, &e);
    for (int i = 0; e > 0 && i < l->n && achou < 0; i++)
      if (l->arq[i].tipo == PARTE_EP && l->arq[i].episodio == e && (!t || !l->arq[i].temporada || l->arq[i].temporada == t))
        achou = i;
  }
  if (achou >= 0) return achou;
  return eps <= 1 ? maior : -1;
}
// O item tem um VIDEO deste tamanho exato?
static int itemTemTamanho(const char *item, const char *fim, uint64_t bytes) {
  const char *fa = raiz(item, fim, "files"), *el;
  if (!fa || *fa != '[' || !bytes) return 0;
  el = fa + 1;
  while (el < fim && isspace((unsigned char)*el)) el++;
  for (; el && el < fim && *el == '{'; el = js_prox(js_fim(el))) {
    const char *ef = js_fim(el);
    char curto[PARTES_NOME] = "", nome[512] = "";
    double tm = numRaiz(el, ef, "size", 0);
    if (tm <= 0 || (uint64_t)tm != bytes) continue;
    if (!textoRaiz(el, ef, "short_name", curto, sizeof curto) || !curto[0]) {
      textoRaiz(el, ef, "name", nome, sizeof nome);
      snprintf(curto, sizeof curto, "%s", base(nome));
    }
    if (ehVideo(curto)) return 1;
  }
  return 0;
}

// O item tem um VIDEO com este nome (e, sabendo, este tamanho)?
static int itemTemArquivo(const char *item, const char *fim, const char *b, uint64_t bytes) {
  const char *fa = raiz(item, fim, "files"), *el;
  if (!fa || *fa != '[' || !b[0]) return 0;
  el = fa + 1;
  while (el < fim && isspace((unsigned char)*el)) el++;
  for (; el && el < fim && *el == '{'; el = js_prox(js_fim(el))) {
    const char *ef = js_fim(el);
    char nome[512] = "", curto[PARTES_NOME] = "";
    if (!textoRaiz(el, ef, "name", nome, sizeof nome)) continue;
    if (!textoRaiz(el, ef, "short_name", curto, sizeof curto) || !curto[0])
      snprintf(curto, sizeof curto, "%s", base(nome));
    if (strcasecmp(curto, b)) continue;
    if (bytes) {
      double t = numRaiz(el, ef, "size", 0);
      if (t > 0 && (uint64_t)t != bytes) continue;
    }
    return 1;
  }
  return 0;
}

// OS ITENS DE UM CORPO DE MYLIST: o inicio e o fim de cada um, numa passada
// so. MEDIDO na TV (16/10): a lista de torrents de uma conta grande tem 7,5 MB;
// percorrer item por item lendo "hash" e cada "files" custava 2,4 s mesmo com
// a lista na memoria. Agora o texto procurado (o hash, o nome do arquivo) e
// achado com strstr no corpo inteiro e so o item que o contem e lido.
typedef struct { const char *ini, *fim; } Vao;
static int itensDoCorpo(const char *corpo, Vao **saida) {
  const char *dados = js_array(corpo, NULL, "data"), *it;
  int n = 0, cap = 0;
  Vao *v = NULL;
  *saida = NULL;
  if (!dados) {
    // mylist?id= responde um item so, como objeto
    const char *d = strstr(corpo, "\"data\"");
    if (d) { d = strchr(d + 6, ':'); if (d) { d++; while (isspace((unsigned char)*d)) d++; } }
    if (!d || *d != '{') return 0;
    v = malloc(sizeof *v);
    if (!v) return 0;
    v[0].ini = d; v[0].fim = js_fim(d);
    *saida = v;
    return 1;
  }
  for (it = dados; it && *it == '{'; ) {
    const char *f = js_fim(it);
    if (n == cap) {
      Vao *t = realloc(v, (size_t)(cap = cap ? cap * 2 : 256) * sizeof *v);
      if (!t) break;
      v = t;
    }
    v[n].ini = it; v[n].fim = f; n++;
    it = js_prox(f);
  }
  *saida = v;
  return n;
}
static int vaoDe(const Vao *v, int n, const char *p) {
  int a = 0, b = n - 1;
  while (a <= b) {
    int m = (a + b) / 2;
    if (p < v[m].ini) b = m - 1;
    else if (p >= v[m].fim) a = m + 1;
    else return m;
  }
  return -1;
}

// TEXTO PARA COMPARAR NOMES: minusculas, so letras e numeros, uma palavra por
// espaco, com espaco nas pontas (" baka and test "): "Baka and Test: Summon
// the Beasts", "[Koten_Gars] Baka and Test - 01 [...]" e "Baka.and.Test.S01"
// viram a mesma escrita. "&" e "and". Fora do ASCII fica como esta.
void partes_normalizar(const char *s, char *d, size_t n) {
  size_t k = 0;
  int espaco = 1;
  if (!n) return;
  if (n < 4) { d[0] = 0; return; }
  d[k++] = ' ';
  for (; s && *s && k + 6 < n; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '&') {
      if (!espaco) d[k++] = ' ';
      memcpy(d + k, "and ", 4); k += 4; espaco = 1;
    } else if (isalnum(c) || c >= 0x80) {
      d[k++] = (char)tolower(c); espaco = 0;
    } else if (!espaco) {
      d[k++] = ' '; espaco = 1;
    }
  }
  if (!espaco) d[k++] = ' ';
  d[k] = 0;
}
// O grupo do lancamento no comeco do nome do arquivo ("[Koten_Gars] ..."),
// normalizado; vazio sem colchete.
static void grupoDoArquivo(const char *arquivo, char *d, size_t n) {
  const char *b = arquivo && *arquivo ? base(arquivo) : "", *f;
  char g[96];
  d[0] = 0;
  if (*b != '[' || !(f = strchr(b, ']')) || f - b < 2 || f - b > (long)sizeof g - 1) return;
  memcpy(g, b + 1, (size_t)(f - b - 1));
  g[f - b - 1] = 0;
  partes_normalizar(g, d, n);
}
int partes_achar_n(const char *corpo, int usenet, char hashes[][41], int nHashes,
                   int hashesConfiaveis, const char *arquivo, uint64_t bytes, PartesLista *l, int *itens) {
  return partes_achar_t(corpo, usenet, hashes, nHashes, hashesConfiaveis, arquivo, bytes, NULL, l, itens);
}

int partes_achar_t(const char *corpo, int usenet, char hashes[][41], int nHashes,
                   int hashesConfiaveis, const char *arquivo, uint64_t bytes, const char *titulo,
                   PartesLista *l, int *itens) {
  const char *b = arquivo && *arquivo ? base(arquivo) : "";
  Vao *v = NULL;
  int n, achou = 0;
  if (itens) *itens = 0;
  if (!corpo || !l) return 0;
  n = itensDoCorpo(corpo, &v);
  if (itens) *itens = n;
  // 1) e 3) hash: o do addon (confiavel) ou os 40 hex do link. O TorBox guarda
  //    o hash em minusculas, como partes_hashes_texto devolve.
  for (int i = 0; i < nHashes * 2 && !achou; i++) {
    // minusculas (o TorBox) e, por garantia, maiusculas
    char hx[41];
    for (int c = 0; c < 41; c++)
      hx[c] = (char)(i % 2 ? toupper((unsigned char)hashes[i / 2][c]) : hashes[i / 2][c]);
    for (const char *p = strstr(corpo, hx); p && !achou; p = strstr(p + 40, hx)) {
      int k = vaoDe(v, n, p);
      char h[64] = "";
      if (k < 0 || !textoRaiz(v[k].ini, v[k].fim, "hash", h, sizeof h) || strcasecmp(h, hx)) continue;
      if (!partes_ler_item(v[k].ini, v[k].fim, usenet, l)) break;
      l->atual = arquivoDaFonte(l, arquivo, bytes);
      snprintf(l->como, sizeof l->como, "%s", i / 2 == 0 && hashesConfiaveis ? "hash" : "link hash");
      l->exato = 1;
      achou = 1;
    }
  }
  // 2) o nome do arquivo da fonte num arquivo de algum item. Nome com aspa,
  //    barra ou fora do ASCII pode vir escapado no JSON: ai vale a varredura
  //    item a item, a lenta.
  if (!achou && nHashes == 0 && b[0]) {
    int simples = 1;
    for (const unsigned char *c = (const unsigned char *)b; *c; c++)
      if (*c >= 0x80 || *c == '"' || *c == '\\') simples = 0;
    if (simples) {
      for (const char *p = strstr(corpo, b); p && !achou; p = strstr(p + 1, b)) {
        int k = vaoDe(v, n, p);
        if (k < 0 || !itemTemArquivo(v[k].ini, v[k].fim, b, bytes)) continue;
        if (!partes_ler_item(v[k].ini, v[k].fim, usenet, l)) continue;
        l->atual = arquivoDaFonte(l, arquivo, bytes);
        snprintf(l->como, sizeof l->como, "filename");
        achou = 1;
      }
    } else {
      for (int k = 0; k < n && !achou; k++) {
        if (!itemTemArquivo(v[k].ini, v[k].fim, b, bytes)) continue;
        if (!partes_ler_item(v[k].ini, v[k].fim, usenet, l)) continue;
        l->atual = arquivoDaFonte(l, arquivo, bytes);
        snprintf(l->como, sizeof l->como, "filename");
        achou = 1;
      }
    }
  }
  // 4) TAMANHO EXATO do arquivo (videoSize do addon) num video de algum item:
  //    num arquivo de varios GB, o mesmo numero de bytes e o mesmo arquivo.
  if (!achou && nHashes == 0 && bytes > 1024ULL * 1024 * 50) {
    char num[32];
    snprintf(num, sizeof num, "%llu", (unsigned long long)bytes);
    for (const char *p = strstr(corpo, num); p && !achou; p = strstr(p + 1, num)) {
      int k = vaoDe(v, n, p);
      if (isdigit((unsigned char)p[strlen(num)]) || (p > corpo && isdigit((unsigned char)p[-1]))) continue;
      if (k < 0 || !itemTemTamanho(v[k].ini, v[k].fim, bytes)) continue;
      if (!partes_ler_item(v[k].ini, v[k].fim, usenet, l)) continue;
      l->atual = arquivoDaFonte(l, arquivo, bytes);
      snprintf(l->como, sizeof l->como, "size");
      achou = 1;
    }
  }
  // O titulo NAO escolhe sozinho (dono, 16/10: [Prof] no lugar de
  // [Koten_Gars]): ele so alimenta a lista de copias (partes_copias).
  (void)titulo;
  free(v);
  return achou;
}
int partes_achar(const char *corpo, int usenet, char hashes[][41], int nHashes,
                 int hashesConfiaveis, const char *arquivo, uint64_t bytes, PartesLista *l) {
  return partes_achar_n(corpo, usenet, hashes, nHashes, hashesConfiaveis, arquivo, bytes, l, NULL);
}

// ---------------------------------------------------------------- copias
//
// SMART DETECT POR PONTOS (dono, 16/10, a tela do AIOStreams: tamanho,
// resolucao, qualidade, codec, grupo, edicao, audio, canais, idiomas). Cada
// copia do titulo na conta ganha pontos pelo que concorda com a fonte e perde
// pelo que discorda; temporada e tamanho do episodio pesam quase como veto.
typedef struct { int res, codec, bits10, audio, origem, dual; } Atrib;
static void atributos(const char *texto, Atrib *a) {
  char n[1400];
  memset(a, 0, sizeof *a);
  partes_normalizar(texto, n, sizeof n);
  if (strstr(n, "2160") || strstr(n, " 4k ") || strstr(n, " uhd ")) a->res = 2160;
  else if (strstr(n, "1080")) a->res = 1080;
  else if (strstr(n, "720p") || strstr(n, " 720 ")) a->res = 720;
  else if (strstr(n, "480p") || strstr(n, "576p")) a->res = 480;
  if (strstr(n, "x265") || strstr(n, "h265") || strstr(n, " h 265 ") || strstr(n, "hevc")) a->codec = 2;
  else if (strstr(n, "x264") || strstr(n, "h264") || strstr(n, " h 264 ") || strstr(n, " avc ")) a->codec = 1;
  else if (strstr(n, " av1 ")) a->codec = 3;
  if (strstr(n, "10bit") || strstr(n, " 10 bit ") || strstr(n, "hi10")) a->bits10 = 1;
  if (strstr(n, "eac3") || strstr(n, " ddp") || strstr(n, " dd ")) a->audio = 4;
  else if (strstr(n, "truehd")) a->audio = 7;
  else if (strstr(n, "flac")) a->audio = 1;
  else if (strstr(n, " dts")) a->audio = 5;
  else if (strstr(n, " ac3 ")) a->audio = 3;
  else if (strstr(n, " opus ")) a->audio = 6;
  else if (strstr(n, " aac")) a->audio = 2;
  if (strstr(n, "remux") || strstr(n, "bluray") || strstr(n, " blu ray ") || strstr(n, "bdrip") || strstr(n, " bd ") ||
      strstr(n, " bdmv ")) a->origem = 1;
  else if (strstr(n, " web") || strstr(n, "webrip")) a->origem = 2;
  else if (strstr(n, " dvd")) a->origem = 3;
  if (strstr(n, " dual ") || strstr(n, " multi ")) a->dual = 1;
}
static const char *nomeCodec(int c) { return c == 2 ? "x265" : c == 1 ? "x264" : c == 3 ? "AV1" : ""; }
static const char *nomeAudio(int c) {
  static const char *t[] = { "", "FLAC", "AAC", "AC3", "E-AC3", "DTS", "Opus", "TrueHD" };
  return c >= 0 && c < 8 ? t[c] : "";
}
static void porqueMais(char *d, size_t n, const char *w) {
  size_t k = strlen(d);
  if (k + strlen(w) + 3 < n) snprintf(d + k, n - k, "%s%s", k ? ", " : "", w);
}
// O arquivo deste episodio numa copia: pelo numero (e temporada), senao o de
// tamanho mais perto, senao o maior episodio.
static int arquivoDoEpisodio(const PartesLista *c, int t, int e, uint64_t bytes) {
  int melhor = -1;
  if (e > 0)
    for (int i = 0; i < c->n; i++)
      if (c->arq[i].tipo == PARTE_EP && c->arq[i].episodio == e && (!t || !c->arq[i].temporada || c->arq[i].temporada == t))
        { if (melhor < 0 || (t && c->arq[i].temporada == t && c->arq[melhor].temporada != t)) melhor = i; }
  if (melhor >= 0) return melhor;
  for (int i = 0; i < c->n; i++) {
    if (c->arq[i].tipo != PARTE_EP) continue;
    if (melhor < 0) { melhor = i; continue; }
    if (bytes) {
      uint64_t da = c->arq[i].bytes > bytes ? c->arq[i].bytes - bytes : bytes - c->arq[i].bytes;
      uint64_t db = c->arq[melhor].bytes > bytes ? c->arq[melhor].bytes - bytes : bytes - c->arq[melhor].bytes;
      if (da < db) melhor = i;
    } else if (c->arq[i].bytes > c->arq[melhor].bytes) melhor = i;
  }
  return melhor;
}

int partes_copias(const char *corpo, const char *titulo, const char *arquivo, uint64_t bytes,
                  const char *fonteTexto, PartesLista *l) {
  static PartesLista cand;           // grande: so o fio da busca usa (fioTrava)
  static pthread_mutex_t candTrava = PTHREAD_MUTEX_INITIALIZER;
  char tInteiro[256], tPrincipal[256], grp[96], nome[512], nn[600], txt[1400];
  const char *dois;
  Vao *v = NULL;
  int n, srcT = 0, srcE = 0, novos = 0;
  Atrib fa;
  if (!corpo || !titulo || !*titulo || !l) return 0;
  dois = strchr(titulo, ':');
  partes_normalizar(titulo, tInteiro, sizeof tInteiro);
  if (dois) {
    char pre[256];
    snprintf(pre, sizeof pre, "%.*s", (int)(dois - titulo), titulo);
    partes_normalizar(pre, tPrincipal, sizeof tPrincipal);
  } else snprintf(tPrincipal, sizeof tPrincipal, "%s", tInteiro);
  // Titulo curto demais ("up", "it") casaria com qualquer coisa.
  if (strlen(tPrincipal) < 6) return 0;
  grupoDoArquivo(arquivo, grp, sizeof grp);
  partes_temporada_episodio(arquivo ? base(arquivo) : "", &srcT, &srcE);
  atributos(fonteTexto ? fonteTexto : arquivo, &fa);
  n = itensDoCorpo(corpo, &v);
  pthread_mutex_lock(&candTrava);
  for (int k = 0; k < n; k++) {
    int pts = 0, f, temTemp = 0, mesmaTemp = 0, j, pior;
    ParteCopia cp;
    Atrib ca;
    if (!textoRaiz(v[k].ini, v[k].fim, "name", nome, sizeof nome)) continue;
    partes_normalizar(nome, nn, sizeof nn);
    if (!strstr(nn, tInteiro) && !strstr(nn, tPrincipal)) continue;
    if (!partes_ler_item(v[k].ini, v[k].fim, 0, &cand)) continue;
    memset(&cp, 0, sizeof cp);
    cp.item = cand.item;
    snprintf(cp.nome, sizeof cp.nome, "%s", nome);
    for (int i = 0; i < cand.n; i++) cp.nArq += cand.arq[i].tipo == PARTE_EP;
    f = arquivoDoEpisodio(&cand, srcT, srcE, bytes);
    if (f >= 0) { cp.epBytes = cand.arq[f].bytes; cp.temporada = cand.arq[f].temporada; }
    // Tamanho do episodio contra o do addon.
    if (bytes && cp.epBytes) {
      double r = cp.epBytes > bytes ? (double)(cp.epBytes - bytes) / (double)bytes : (double)(bytes - cp.epBytes) / (double)bytes;
      if (r < .002) { pts += 600; cp.tamanho = 2; }
      else if (r < .02) { pts += 300; cp.tamanho = 1; }
      else if (r < .10) pts += 100;
      else if (r > .50) pts -= 400;
      else pts -= 150;
    }
    // Temporada.
    for (int i = 0; i < cand.n; i++) if (cand.arq[i].temporada) { temTemp = 1; if (cand.arq[i].temporada == srcT) mesmaTemp = 1; }
    if (srcT) {
      if (f >= 0 && cand.arq[f].temporada == srcT) pts += 150;
      else if (mesmaTemp) pts += 60;
      else if (temTemp) pts -= 400;
    }
    // Grupo do lancamento.
    if (grp[0] && (strstr(nn, grp) || (f >= 0 && (partes_normalizar(cand.arq[f].nome, txt, sizeof txt), strstr(txt, grp))))) {
      pts += 200; cp.grupo = 1;
    }
    // Resolucao, codec, 10 bits, audio, origem, dual.
    snprintf(txt, sizeof txt, "%s %s", nome, f >= 0 ? cand.arq[f].nome : "");
    atributos(txt, &ca);
    if (fa.res && ca.res) pts += fa.res == ca.res ? 60 : -80;
    if (fa.codec && ca.codec) pts += fa.codec == ca.codec ? 50 : -50;
    if (fa.bits10 != ca.bits10 && (fa.codec || ca.codec)) pts -= 20;
    if (fa.audio && ca.audio) pts += fa.audio == ca.audio ? 50 : -50;
    if (fa.origem && ca.origem) pts += fa.origem == ca.origem ? 30 : -30;
    if (fa.dual && ca.dual) pts += 20;
    if (ca.res) { char r[12]; snprintf(r, sizeof r, "%dp", ca.res); porqueMais(cp.porque, sizeof cp.porque, r); }
    if (ca.codec) porqueMais(cp.porque, sizeof cp.porque, nomeCodec(ca.codec));
    if (ca.audio) porqueMais(cp.porque, sizeof cp.porque, nomeAudio(ca.audio));
    // O nome com o titulo inteiro desempata, nada mais.
    if (strstr(nn, tInteiro)) pts += 5;
    cp.pontos = pts;
    // Entra na lista (sem repetir o item), mantendo as PARTES_COPIAS melhores.
    for (j = 0; j < l->nCopias; j++) if (l->copias[j].item == cp.item) break;
    if (j < l->nCopias) { if (cp.pontos > l->copias[j].pontos) l->copias[j] = cp; continue; }
    if (l->nCopias < PARTES_COPIAS) { l->copias[l->nCopias++] = cp; novos++; continue; }
    pior = 0;
    for (j = 1; j < l->nCopias; j++) if (l->copias[j].pontos < l->copias[pior].pontos) pior = j;
    if (cp.pontos > l->copias[pior].pontos) { l->copias[pior] = cp; novos++; }
  }
  pthread_mutex_unlock(&candTrava);
  free(v);
  // Melhor primeiro.
  for (int a = 1; a < l->nCopias; a++) {
    ParteCopia t = l->copias[a];
    int b = a - 1;
    while (b >= 0 && l->copias[b].pontos < t.pontos) { l->copias[b + 1] = l->copias[b]; b--; }
    l->copias[b + 1] = t;
  }
  return novos;
}

int partes_principais(const PartesLista *l) {
  int k = 0;
  for (int i = 0; l && i < l->n; i++) k += !l->arq[i].extra;
  return k;
}
int partes_seguinte(const PartesLista *l, int k) {
  for (int i = k + 1; l && i < l->n; i++) if (!l->arq[i].extra) return i;
  return -1;
}
int partes_numero(const PartesLista *l, int k) {
  int num = 0;
  if (!l || k < 0 || k >= l->n || l->arq[k].extra) return 0;
  for (int i = 0; i <= k; i++) num += !l->arq[i].extra;
  return num;
}

int partes_conta_virtual(const PartesLista *l, int k, double pos, double dur, double catDur,
                         double *vpos, double *vdur) {
  double antes = 0, total = 0, este;
  if (!l || k < 0 || k >= l->n || l->arq[k].extra || !(dur > 1.0) || !isfinite(pos)) return 0;
  for (int i = 0; i < l->n; i++) {
    if (l->arq[i].extra) continue;
    total += (double)l->arq[i].bytes;
    if (i < k) antes += (double)l->arq[i].bytes;
  }
  este = (double)l->arq[k].bytes;
  if (!(total > 0) || !(este > 0)) return 0;
  {
    double frac = pos / dur, fim;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    fim = catDur > 1.0 ? catDur : dur * total / este;
    *vdur = fim;
    *vpos = fim * (antes + este * frac) / total;
  }
  return 1;
}

// ---------------------------------------------------------------- busca da lista

typedef struct {
  char url[4096];
  char cab[512];      // cabecalhos que o addon exige (para abrir o link)
  char arquivo[512];
  char hash[48];
  char titulo[256];
  char chave[160];
  char casa[400];     // a chave da copia lembrada (sem URL)
  char texto[1400];   // arquivo + rotulo + descricao: os atributos da fonte
  uint64_t bytes;
  unsigned geracao, conta;
  int cedo;   // partes_pedir_cedo: so a pagina 1 da memoria, sem rede
  int item;   // partes_pedir_item: abre este item (Escolher outra copia)
  int nCopias;
  ParteCopia copias[PARTES_COPIAS];
} PedidoLista;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static PartesLista ultima;          // resultado da ultima busca
static unsigned geracaoLista, ultimaConta;
static char chaveUltima[160];       // de que fonte e `ultima`

// A FONTE, sem URL na memoria: o rotulo e o nome do arquivo bastam para
// reconhecer a mesma linha da folha entre duas aberturas.
static void chaveFonte(const Stream *s, char *dst, size_t n) {
  unsigned long long h = 1469598103934665603ULL;
  for (const unsigned char *p = (const unsigned char *)s->url; *p; p++) { h ^= *p; h *= 1099511628211ULL; }
  snprintf(dst, n, "%016llx|%.60s|%.60s", h, s->provedor, s->arquivo);
}

// AS ULTIMAS LISTAS ACHADAS, por fonte: segurar de novo a mesma fonte (ou
// segurar depois do pedido cedo do OK) responde na hora.
#define PARTES_MEMO 16
static struct { char chave[160]; unsigned conta; PartesLista l; } memo[PARTES_MEMO];
static int memoProx;
static int memoPegarSemTrava(const char *ch, PartesLista *l) {
  for (int i = 0; i < PARTES_MEMO; i++)
    if (memo[i].conta == debrid_tb_geracao() && memo[i].chave[0] && !strcmp(memo[i].chave, ch)) { if (l && !partes_lista_copiar(l, &memo[i].l)) return 0; return 1; }
  return 0;
}
static void memoGuardarSemTrava(const char *ch, const PartesLista *l, unsigned conta) {
  int i;
  for (i = 0; i < PARTES_MEMO; i++) if (!strcmp(memo[i].chave, ch)) break;
  if (i == PARTES_MEMO) { i = memoProx; memoProx = (memoProx + 1) % PARTES_MEMO; }
  snprintf(memo[i].chave, sizeof memo[i].chave, "%s", ch);
  if (!partes_lista_copiar(&memo[i].l, l)) { memo[i].chave[0] = 0; return; }
  memo[i].conta = conta;
}

static void memoTirarItem(int item) {
  pthread_mutex_lock(&trava);
  for (int i = 0; i < PARTES_MEMO; i++) if (memo[i].chave[0] && memo[i].l.item == item) memo[i].chave[0] = 0;
  pthread_mutex_unlock(&trava);
}

// AS COPIAS CONFIRMADAS (disco): "<fonte>\t<hash do item>\t<quando>". A fonte
// e o addon + o nome do arquivo + o tamanho (nunca a URL). Hash, o passo que
// adicionou pelo link e "Escolher outra copia" gravam; o proximo segurar acha
// a mesma copia pelo hash dela.
#define CASA_LINHA 480
static char (*casas)[CASA_LINHA];
static int nCasas, casasLidas;
static pthread_mutex_t casaTrava = PTHREAD_MUTEX_INITIALIZER;
static void casaChave(const Stream *s, char *d, size_t n) {
  const char *b = base(s->arquivo);
  snprintf(d, n, "%.80s|%.200s|%llu", s->provedor, b[0] ? b : s->rotulo, (unsigned long long)s->tamanhoBytes);
  for (char *c = d; *c; c++) if (*c == '\t' || *c == '\n' || *c == '\r') *c = ' ';
}
static void casasLer(void) {
  char *t, *l, *ctx = NULL;
  casasLidas = 1; nCasas = 0;
  if (!casas) casas = calloc(PARTES_CASA_MAX, CASA_LINHA);
  if (!casas || !(t = dados_ler(PARTES_CASA_ARQUIVO))) return;
  for (l = strtok_r(t, "\n", &ctx); l && nCasas < PARTES_CASA_MAX; l = strtok_r(NULL, "\n", &ctx))
    if (strchr(l, '\t') && strlen(l) < CASA_LINHA) snprintf(casas[nCasas++], CASA_LINHA, "%s", l);
  free(t);
}
static void casasGravar(void) {
  size_t tam = (size_t)nCasas * (CASA_LINHA + 1) + 1, k = 0;
  char *b = malloc(tam);
  if (!b) return;
  b[0] = 0;
  for (int i = 0; i < nCasas; i++) k += (size_t)snprintf(b + k, tam - k, "%s\n", casas[i]);
  dados_gravar(PARTES_CASA_ARQUIVO, b);
  free(b);
}
static int casaAchar(const char *ch) {
  size_t L = strlen(ch);
  for (int i = 0; i < nCasas; i++) if (!strncmp(casas[i], ch, L) && casas[i][L] == '\t') return i;
  return -1;
}
static int casaPegar(const char *ch, char hash[41]) {
  int i, ok = 0;
  if (!ch[0]) return 0;
  pthread_mutex_lock(&casaTrava);
  if (!casasLidas) casasLer();
  if (casas && (i = casaAchar(ch)) >= 0) {
    const char *h = casas[i] + strlen(ch) + 1;
    if (strlen(h) >= 40) { memcpy(hash, h, 40); hash[40] = 0; ok = 1; }
  }
  pthread_mutex_unlock(&casaTrava);
  return ok;
}
static void casaGuardar(const char *ch, const char *hash, const char *porque) {
  int i;
  if (!ch[0] || !hash || strlen(hash) != 40) return;
  pthread_mutex_lock(&casaTrava);
  if (!casasLidas) casasLer();
  if (casas) {
    if ((i = casaAchar(ch)) >= 0 && !strncmp(casas[i] + strlen(ch) + 1, hash, 40)) { pthread_mutex_unlock(&casaTrava); return; }
    if (i >= 0) { memmove(casas[i], casas[i + 1], (size_t)(nCasas - i - 1) * CASA_LINHA); nCasas--; }
    if (nCasas == PARTES_CASA_MAX) { memmove(casas[0], casas[1], (size_t)(nCasas - 1) * CASA_LINHA); nCasas--; }
    snprintf(casas[nCasas++], CASA_LINHA, "%s\t%s\t%lld", ch, hash, (long long)time(NULL));
    casasGravar();
    printf("[partes] remembered this copy for the source (%s)\n", porque);
    fflush(stdout);
  }
  pthread_mutex_unlock(&casaTrava);
}

static void publicar(const PedidoLista *pd, const PartesLista *l) {
  pthread_mutex_lock(&trava);
  if (pd->conta == debrid_tb_geracao()) {
    if (l->estado == PARTES_PRONTO) memoGuardarSemTrava(pd->chave, l, pd->conta);
    if (!pd->cedo && pd->geracao == geracaoLista) { partes_lista_copiar(&ultima, l); ultimaConta = pd->conta; }
  }
  pthread_mutex_unlock(&trava);
}

// A LISTA DA CONTA, com memoria. MEDIDO na TV (16/10): pedir torrents/mylist
// com bypass_cache a cada segurar levava segundos num filme achado de cara, e
// numa serie que nao achou (torrents + usenet, os dois sem cache) passou de um
// minuto sem resposta. Agora:
//   - a primeira leitura usa a copia em cache do proprio TorBox (rapida);
//     so quando ela nao acha o item vem uma leitura fresca (o torrent pode ter
//     entrado na conta agora, pelo addon);
//   - o corpo fica aqui por LISTA_VALIDADE_MS: o segundo segurar na mesma
//     folha responde da memoria;
//   - a folha de Fontes, ao abrir, ja pede a lista (partes_aquecer).
// Uma leitura de rede por vez: quem chega durante uma leitura espera e usa o
// que ela trouxe.
#define LISTA_VALIDADE_MS 120000u
typedef struct { char *corpo; int st; unsigned em, conta; int fresco, renovar; } ListaCache;
static ListaCache lcache[2];
static pthread_mutex_t lcTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lcRede = PTHREAD_MUTEX_INITIALIZER;

static unsigned msAgora(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned)((unsigned long long)ts.tv_sec * 1000ULL + (unsigned long long)ts.tv_nsec / 1000000ULL);
}
// Fresca da memoria so com menos de 10 s (dono, 16/10, log 19: uma lista
// vazia lida logo depois de esvaziar a conta respondia "nao achou em 0 ms").
#define LISTA_FRESCA_MS 10000u
static char *copiaCache(ListaCache *c, int fresco, int *st) {
  if (c->conta != debrid_tb_geracao() || !c->corpo || msAgora() - c->em >= LISTA_VALIDADE_MS || (fresco && (!c->fresco || msAgora() - c->em >= LISTA_FRESCA_MS)))
    return NULL;
  if (st) *st = c->st;
  return strdup(c->corpo);
}
// *origem: 1 = da memoria, 0 = da rede. So a PRIMEIRA pagina (os itens mais
// novos, onde esta o que se tocou agora) fica na memoria; as outras vem da
// rede e saem quando a busca acaba.
static char *listaConta(int usenet, int pagina, int fresco, int *st, int *origem) {
  char *r;
  int s = 0;
  unsigned t0 = msAgora();
  if (origem) *origem = 1;
  pthread_mutex_lock(&lcRede);
  pthread_mutex_lock(&lcTrava);
  if (pagina == 0 && lcache[usenet].renovar) fresco = 1;
  r = pagina == 0 ? copiaCache(&lcache[usenet], fresco, st) : NULL;
  pthread_mutex_unlock(&lcTrava);
  if (r) { pthread_mutex_unlock(&lcRede); return r; }
  if (origem) *origem = 0;
  unsigned conta = debrid_tb_geracao();
  r = debrid_tb_mylist(usenet, 0, pagina, fresco, &s);
  if (conta != debrid_tb_geracao()) { free(r); r = NULL; s = 0; }
  if (st) *st = s;
  printf("[partes] TorBox %s list page %d (%s): HTTP %d, %u KB in %u ms\n", usenet ? "usenet" : "torrent",
         pagina + 1, fresco ? "fresh" : "TorBox cache", s, r ? (unsigned)(strlen(r) / 1024) : 0u, msAgora() - t0);
  fflush(stdout);
  if (r && s >= 200 && s < 300 && pagina == 0) {
    char *c = strdup(r);
    pthread_mutex_lock(&lcTrava);
    if (c) {
      free(lcache[usenet].corpo);
      lcache[usenet].corpo = c; lcache[usenet].st = s;
      lcache[usenet].em = msAgora(); lcache[usenet].fresco = fresco;
      lcache[usenet].conta = conta; lcache[usenet].renovar = 0;
    }
    pthread_mutex_unlock(&lcTrava);
  }
  pthread_mutex_unlock(&lcRede);
  return r;
}
static void listaEsquecer(void) {
  pthread_mutex_lock(&lcTrava);
  for (int i = 0; i < 2; i++) { free(lcache[i].corpo); memset(&lcache[i], 0, sizeof lcache[i]); }
  pthread_mutex_unlock(&lcTrava);
}

static void *fioAquecer(void *u) {
  int st = 0;
  (void)u;
  free(listaConta(0, 0, 0, &st, NULL));
  return NULL;
}
void partes_aquecer(void) {
  pthread_t t;
  int tem;
  if (!partes_disponivel() || debrid_tb_disponivel() <= 0) return;
  pthread_mutex_lock(&lcTrava);
  tem = lcache[0].conta == debrid_tb_geracao() && lcache[0].corpo && msAgora() - lcache[0].em < LISTA_VALIDADE_MS / 2;
  pthread_mutex_unlock(&lcTrava);
  if (tem) return;
  if (pthread_create(&t, NULL, fioAquecer, NULL) == 0) pthread_detach(t);
}

// O corpo de um mylist?id=: o item (objeto, ou o primeiro de uma lista).
static const char *itemDoCorpo(const char *r) {
  const char *d = r ? strstr(r, "\"data\"") : NULL;
  if (d) { d = strchr(d + 6, ':'); if (d) { d++; while (isspace((unsigned char)*d)) d++; } }
  if (d && *d == '[') { d++; while (isspace((unsigned char)*d)) d++; }
  return d && *d == '{' ? d : NULL;
}
// download_state/active do item (o TorBox apaga itens parados; log 19).
static void logEstadoItem(const char *item, const char *porque) {
  char ds[48] = "";
  const char *f = item ? js_fim(item) : NULL;
  const char *a = item ? raiz(item, f, "active") : NULL;
  if (!item) return;
  textoRaiz(item, f, "download_state", ds, sizeof ds);
  printf("[partes] %s: TorBox item state=%s active=%s\n", porque, ds[0] ? ds : "?",
         a ? (!strncmp(a, "true", 4) ? "yes" : !strncmp(a, "false", 5) ? "no" : "?") : "?");
  fflush(stdout);
}
// A copia lembrada (so o hash dela; o arquivo da fonte e achado depois).
static int acharLembrado(const char *corpo, char lembrado[][41], const PedidoLista *pd, uint64_t bytes, PartesLista *l) {
  static PartesLista t;   // fioTrava
  if (!partes_achar(corpo, 0, lembrado, 1, 0, NULL, 0, &t)) return 0;
  if (!partes_lista_copiar(l, &t)) return 0;
  l->atual = arquivoDaFonte(l, pd->arquivo, bytes);
  l->exato = 0;
  snprintf(l->como, sizeof l->como, "remembered");
  return 1;
}
// "Escolher outra copia": le o item escolhido, acha nele o arquivo da fonte e
// lembra a escolha.
static void abrirItem(PedidoLista *pd, PartesLista *l) {
  int st = 0;
  unsigned t0 = msAgora();
  char *r = debrid_tb_mylist(0, pd->item, 0, 1, &st);
  const char *d = itemDoCorpo(r);
  partes_lista_limpar(l);
  l->atual = -1;
  if (d && partes_ler_item(d, js_fim(d), 0, l)) {
    l->atual = arquivoDaFonte(l, pd->arquivo, pd->bytes);
    l->estado = PARTES_PRONTO;
    snprintf(l->como, sizeof l->como, "chosen");
    l->nCopias = pd->nCopias;
    memcpy(l->copias, pd->copias, sizeof l->copias);
    casaGuardar(pd->casa, l->hash, "chosen in Choose another copy");
    logEstadoItem(d, "chosen copy");
  } else l->estado = st >= 200 && st < 300 ? PARTES_NAO_ACHOU : PARTES_ERRO;
  printf("[partes] chosen copy (TorBox torrent %d): %s in %u ms, %d video file(s), the add-on's file is %d\n", pd->item,
         l->estado == PARTES_PRONTO ? "read" : "not readable", msAgora() - t0, l->n, l->atual + 1);
  fflush(stdout);
  free(r);
  publicar(pd, l);
}

static void *fioLista(void *u) {
  PedidoLista *pd = u;
  static PartesLista l;   // grande demais para a pilha dos fios do Tizen
  static pthread_mutex_t fioTrava = PTHREAD_MUTEX_INITIALIZER;
  char hashes[9][41];
  int nh = 0, confiavel = 0, disp;
  unsigned t0 = msAgora();
  pthread_mutex_lock(&fioTrava);
  partes_lista_limpar(&l);
  l.atual = -1;
  if (pd->conta != debrid_tb_geracao()) goto fim;
  disp = debrid_tb_disponivel();
  if (disp <= 0) {
    l.estado = disp < 0 ? PARTES_SEM_PLANO : PARTES_SEM_CHAVE;
    if (!pd->cedo) {
      printf("[partes] files of the source: %s\n",
             disp < 0 ? "the TorBox account has no API plan" : "no TorBox key in Nuvio (Settings > Integrations > Debrid)");
      fflush(stdout);
      publicar(pd, &l);
    }
    goto fim;
  }
  // O PEDIDO CEDO (OK afundou numa fonte, o segurar ainda nao venceu): so a
  // pagina 1 que ja esta na memoria, sem rede. Achou, fica na memoria por
  // fonte e o segurar responde na hora; nao achou, o segurar faz a busca toda.
  if (pd->cedo) {
    int st = 0;
    char *r;
    pthread_mutex_lock(&lcTrava);
    r = copiaCache(&lcache[0], 0, &st);
    pthread_mutex_unlock(&lcTrava);
    if (r) {
      if (pd->hash[0] && strlen(pd->hash) == 40) {
        for (int i = 0; i < 40; i++) hashes[0][i] = (char)tolower((unsigned char)pd->hash[i]);
        hashes[0][40] = 0;
        nh = 1; confiavel = 1;
      }
      nh += partes_hashes_texto(pd->url, hashes + nh, 9 - nh);
      // So o hash responde cedo: o resto precisa das copias (busca inteira).
      if (partes_achar_t(r, 0, hashes, nh, confiavel, pd->arquivo, pd->bytes, pd->titulo, &l, NULL) && l.exato) {
        l.estado = PARTES_PRONTO;
        publicar(pd, &l);
      }
      free(r);
    }
    goto fim;
  }
  if (pd->item > 0) { abrirItem(pd, &l); goto fim; }
  if (pd->hash[0] && strlen(pd->hash) == 40) {
    for (int i = 0; i < 40; i++) hashes[0][i] = (char)tolower((unsigned char)pd->hash[i]);
    hashes[0][40] = 0;
    nh = 1; confiavel = 1;
  }
  nh += partes_hashes_texto(pd->url, hashes + nh, 9 - nh);
  {
    static PartesLista cop;   // so as copias (fioTrava)
    char lembrado[1][41];
    int temLembrado = nh == 0 && casaPegar(pd->casa, lembrado[0]);
    int achou = 0, st = 0, mem = 0, fresco0 = 0, itens = 0, paginas = 0, pelaLista = 0;
    uint64_t bytes = pd->bytes;
    char *r;
    cop.nCopias = 0;
    // 1) os torrents da conta, pagina por pagina (MEDIDO na TV, 16/10: a
    //    primeira pagina de uma conta com mais de 1000 itens nao tinha o pacote
    //    de Baka and Test, que estava na conta). Ate 10 paginas. So torrents:
    //    usenet nao entra (dono, 16/10). Em cada pagina: hash, a copia lembrada,
    //    nome do arquivo, tamanho exato; e as copias do titulo para a escolha.
    for (int pg = 0; pg < 10 && !achou; pg++) {
      int m = 0;
      r = listaConta(0, pg, 0, &st, &m);
      if (pg == 0) mem = m;
      itens = 0;
      if (r) achou = partes_achar_t(r, 0, hashes, nh, confiavel, pd->arquivo, bytes, pd->titulo, &l, &itens);
      if (r && (!achou || !l.exato) && temLembrado && acharLembrado(r, lembrado, pd, bytes, &l)) achou = 1;
      if (r && !achou) partes_copias(r, pd->titulo, pd->arquivo, bytes, pd->texto, &cop);
      free(r);
      paginas++;
      if (!r || itens < DEBRID_TB_PAGINA) break;
    }
    // 2) a de torrents FRESCA: o addon pode ter posto o torrent na conta agora
    //    (a copia em cache do TorBox atrasa). Os novos ficam na primeira pagina.
    if (!achou) {
      fresco0 = 1;
      r = listaConta(0, 0, 1, &st, NULL);
      if (r) achou = partes_achar_t(r, 0, hashes, nh, confiavel, pd->arquivo, bytes, pd->titulo, &l, NULL);
      if (r && (!achou || !l.exato) && temLembrado && acharLembrado(r, lembrado, pd, bytes, &l)) achou = 1;
      if (r && !achou) partes_copias(r, pd->titulo, pd->arquivo, bytes, pd->texto, &cop);
      free(r);
    }
    // 3) fora da conta: com um hash (do addon, ou o do link, que o NMR poe no
    //    caminho), o TorBox poe o torrent na conta SO se ele ja estiver em cache.
    if (!achou && nh > 0) {
      int id = debrid_tb_adicionar(hashes[0]);
      if (id > 0) {
        listaEsquecer();
        r = debrid_tb_mylist(0, id, 0, 1, &st);
        if (r && partes_achar(r, 0, hashes, 1, confiavel, pd->arquivo, bytes, &l)) {
          achou = 1; l.exato = 1;
          snprintf(l.como, sizeof l.como, "added");
        }
        free(r);
      }
    }
    // 4) FORA DA CONTA, SEM HASH (dono, 16/10, log 16: o pacote [KH] so casou
    //    depois de tocar a fonte, que o pos na conta): abre o link do addon
    //    como o Play abre — o addon pede ao TorBox, o torrent entra na conta —
    //    e le a pagina 1 fresca, com o tamanho exato que o link responde.
    if (!achou && (!strncmp(pd->url, "http://", 7) || !strncmp(pd->url, "https://", 8))) {
      static PartesLista aviso;   // fioTrava; grande demais para a pilha
      long n = 0; int stl = 0, cross = 0;
      int64_t total = -1;
      unsigned t1 = msAgora();
      char *fin = malloc(8192);
      unsigned char *b;
      memset(&aviso, 0, sizeof aviso);
      aviso.estado = PARTES_ADICIONANDO; aviso.atual = -1;
      publicar(pd, &aviso);
      if (fin) fin[0] = 0;
      b = (unsigned char *)rede_baixar_trecho64_final(pd->url, pd->cab, 0, 0, &n, &total, &stl, NULL, fin, &cross);
      free(b); free(fin);
      printf("[partes] not in your TorBox list: opened the add-on link the way Play does (HTTP %d, file %lld bytes, %u ms)\n",
             stl, (long long)total, msAgora() - t1);
      fflush(stdout);
      if (!bytes && total > 1024LL * 1024 * 50) bytes = (uint64_t)total;
      if (stl == 206 || stl == 200) {
        static const unsigned espera[3] = { 1200, 2500, 4000 };
        for (int t = 0; t < 3 && !achou; t++) {
          usleep(espera[t] * 1000u);
          listaEsquecer();
          r = listaConta(0, 0, 1, &st, NULL);
          if (r) achou = partes_achar_t(r, 0, hashes, nh, confiavel, pd->arquivo, bytes, pd->titulo, &l, NULL);
          if (r && !achou && t == 2) partes_copias(r, pd->titulo, pd->arquivo, bytes, pd->texto, &cop);
          free(r);
        }
        if (achou) {
          pelaLista = 1;
          { char c[24]; snprintf(c, sizeof c, "link + %s", l.como); snprintf(l.como, sizeof l.como, "%s", c); }
        }
      }
    }
    // 5) MELHOR COPIA, so com prova forte: tamanho do episodio a 2% (ou o
    //    mesmo), o mesmo grupo quando a fonte diz um, e bem a frente da segunda.
    if (!achou && cop.nCopias > 0) {
      const ParteCopia *c0 = &cop.copias[0];
      char grp[96];
      grupoDoArquivo(pd->arquivo, grp, sizeof grp);
      if (c0->tamanho >= 1 && (!grp[0] || c0->grupo) &&
          (cop.nCopias == 1 || c0->pontos - cop.copias[1].pontos >= 150)) {
        const char *d;
        r = debrid_tb_mylist(0, c0->item, 0, 1, &st);
        d = itemDoCorpo(r);
        if (d && partes_ler_item(d, js_fim(d), 0, &l)) {
          l.atual = arquivoDaFonte(&l, pd->arquivo, bytes);
          achou = 1;
        }
        if (achou) { l.exato = 0; snprintf(l.como, sizeof l.como, "best match"); }
        free(r);
      }
    }
    if (achou) {
      l.estado = PARTES_PRONTO;
      if (!l.exato) {
        l.nCopias = cop.nCopias;
        memcpy(l.copias, cop.copias, sizeof l.copias);
      }
      if (l.exato || pelaLista) casaGuardar(pd->casa, l.hash, l.exato ? "hash" : "added by its link");
      printf("[partes] files of the source: found in TorBox in %u ms (%s, by %s%s): %d video file(s), %d part(s); "
             "the add-on's file is %d (0 = not in it); %d other cop%s\n", msAgora() - t0, l.usenet ? "usenet" : "torrent", l.como,
             mem ? ", list from memory" : "", l.n, partes_principais(&l), l.atual + 1, l.nCopias, l.nCopias == 1 ? "y" : "ies");
    } else if (cop.nCopias > 0) {
      partes_lista_limpar(&l);
      l.atual = -1;
      l.estado = PARTES_ESCOLHER;
      l.nCopias = cop.nCopias;
      memcpy(l.copias, cop.copias, sizeof l.copias);
      printf("[partes] files of the source: no sure match after %u ms; %d cop%s of the title to choose from\n",
             msAgora() - t0, l.nCopias, l.nCopias == 1 ? "y" : "ies");
    } else {
      // A lista de torrents nem veio (rede, 4xx/5xx): e erro, nao "nao achou".
      l.estado = st >= 200 && st < 300 ? PARTES_NAO_ACHOU : PARTES_ERRO;
      printf("[partes] files of the source: not found in TorBox after %u ms (%d list page(s); torrents HTTP %d%s; "
             "hash from add-on %s, %d hash(es) in the link, filename %s)\n", msAgora() - t0, paginas,
             st, fresco0 ? " fresh" : "", confiavel ? "yes" : "no", nh - confiavel, pd->arquivo[0] ? "yes" : "no");
    }
    for (int i = 0; i < l.nCopias && i < 6; i++)
      printf("[partes]   copy %d: %d pts%s%s, episode file %.2f GB, season %d, %d episode(s) [%s] \"%.90s\"\n", i + 1,
             l.copias[i].pontos, l.copias[i].tamanho == 2 ? ", same size" : l.copias[i].tamanho ? ", size within 2%" : "",
             l.copias[i].grupo ? ", same group" : "", (double)l.copias[i].epBytes / 1073741824.0, l.copias[i].temporada,
             l.copias[i].nArq, l.copias[i].porque, l.copias[i].nome);
    fflush(stdout);
    publicar(pd, &l);
  }
fim:
  pthread_mutex_unlock(&fioTrava);
  free(pd);
  return NULL;
}

static PedidoLista *novoPedido(const Stream *s, const char *titulo, const char *ch) {
  PedidoLista *pd = calloc(1, sizeof *pd);
  if (!pd) return NULL;
  snprintf(pd->url, sizeof pd->url, "%s", s->url);
  snprintf(pd->cab, sizeof pd->cab, "%s", s->cabecalhos);
  snprintf(pd->texto, sizeof pd->texto, "%s\n%s\n%s", s->arquivo, s->rotulo, s->descricao);
  casaChave(s, pd->casa, sizeof pd->casa);
  snprintf(pd->arquivo, sizeof pd->arquivo, "%s", s->arquivo);
  snprintf(pd->hash, sizeof pd->hash, "%s", s->hashTorrent[0] ? s->hashTorrent : s->infoHash);
  snprintf(pd->titulo, sizeof pd->titulo, "%s", titulo ? titulo : "");
  snprintf(pd->chave, sizeof pd->chave, "%s", ch);
  pd->bytes = s->tamanhoBytes;
  pd->conta = debrid_tb_geracao();
  return pd;
}

void partes_pedir(const Stream *s, const char *titulo) {
  PedidoLista *pd;
  pthread_t t;
  char ch[160];
  if (!s) return;
  chaveFonte(s, ch, sizeof ch);
  pthread_mutex_lock(&trava);
  if (memoPegarSemTrava(ch, &ultima)) {
    // a mesma fonte, ja achada (por este segurar, um anterior ou o pedido cedo)
    geracaoLista++;
    ultimaConta = debrid_tb_geracao();
    snprintf(chaveUltima, sizeof chaveUltima, "%s", ch);
    pthread_mutex_unlock(&trava);
    printf("[partes] files of the source: from memory (%s, by %s): %d video file(s)\n",
           ultima.usenet ? "usenet" : "torrent", ultima.como, ultima.n);
    fflush(stdout);
    return;
  }
  geracaoLista++;
  partes_lista_limpar(&ultima);
  ultimaConta = debrid_tb_geracao();
  ultima.estado = PARTES_BUSCANDO;
  ultima.atual = -1;
  snprintf(chaveUltima, sizeof chaveUltima, "%s", ch);
  pd = novoPedido(s, titulo, ch);
  if (pd) pd->geracao = geracaoLista;
  pthread_mutex_unlock(&trava);
  if (!pd || pthread_create(&t, NULL, fioLista, pd) != 0) {
    free(pd);
    pthread_mutex_lock(&trava);
    ultima.estado = PARTES_ERRO;
    pthread_mutex_unlock(&trava);
    return;
  }
  pthread_detach(t);
}

void partes_pedir_cedo(const Stream *s, const char *titulo) {
  PedidoLista *pd;
  pthread_t t;
  char ch[160];
  int ja, temLista;
  if (!s || !partes_disponivel()) return;
  chaveFonte(s, ch, sizeof ch);
  pthread_mutex_lock(&trava);
  ja = memoPegarSemTrava(ch, NULL);
  pthread_mutex_unlock(&trava);
  pthread_mutex_lock(&lcTrava);
  temLista = lcache[0].conta == debrid_tb_geracao() && lcache[0].corpo && msAgora() - lcache[0].em < LISTA_VALIDADE_MS;
  pthread_mutex_unlock(&lcTrava);
  if (ja || !temLista) return;
  pd = novoPedido(s, titulo, ch);
  if (!pd) return;
  pd->cedo = 1;
  if (pthread_create(&t, NULL, fioLista, pd) != 0) { free(pd); return; }
  pthread_detach(t);
}

void partes_pedir_item(const Stream *s, int item) {
  PedidoLista *pd;
  pthread_t t;
  char ch[160];
  if (!s || item <= 0) return;
  chaveFonte(s, ch, sizeof ch);
  pd = novoPedido(s, NULL, ch);
  if (!pd) return;
  pd->item = item;
  pthread_mutex_lock(&trava);
  pd->nCopias = ultima.nCopias;
  memcpy(pd->copias, ultima.copias, sizeof pd->copias);
  geracaoLista++;
  pd->geracao = geracaoLista;
  partes_lista_limpar(&ultima);
  ultimaConta = pd->conta;
  ultima.estado = PARTES_BUSCANDO;
  ultima.atual = -1;
  ultima.nCopias = pd->nCopias;
  memcpy(ultima.copias, pd->copias, sizeof ultima.copias);
  snprintf(chaveUltima, sizeof chaveUltima, "%s", ch);
  pthread_mutex_unlock(&trava);
  printf("[partes] Choose another copy: TorBox torrent %d\n", item);
  fflush(stdout);
  if (pthread_create(&t, NULL, fioLista, pd) != 0) {
    free(pd);
    pthread_mutex_lock(&trava);
    ultima.estado = PARTES_ERRO;
    pthread_mutex_unlock(&trava);
    return;
  }
  pthread_detach(t);
}

int partes_resultado_de(const Stream *s, PartesLista *l) {
  char ch[160];
  int e;
  if (!s) return PARTES_VAZIO;
  chaveFonte(s, ch, sizeof ch);
  pthread_mutex_lock(&trava);
  e = ultimaConta != debrid_tb_geracao() || strcmp(ch, chaveUltima) ? PARTES_VAZIO : ultima.estado;
  if (e != PARTES_VAZIO && l && !partes_lista_copiar(l, &ultima)) e = PARTES_ERRO;
  pthread_mutex_unlock(&trava);
  return e;
}

int partes_resultado(PartesLista *l) {
  int e;
  pthread_mutex_lock(&trava);
  e = ultimaConta == debrid_tb_geracao() ? ultima.estado : PARTES_VAZIO;
  if (l && e != PARTES_VAZIO && !partes_lista_copiar(l, &ultima)) e = PARTES_ERRO;
  pthread_mutex_unlock(&trava);
  return e;
}

void partes_cancelar(void) {
  pthread_mutex_lock(&trava);
  if (ultima.estado == PARTES_BUSCANDO) {
    geracaoLista++;
    partes_lista_limpar(&ultima);
    chaveUltima[0] = 0;
  }
  pthread_mutex_unlock(&trava);
}

// ---------------------------------------------------------------- link

typedef struct { int usenet, item, arquivo, comLista; unsigned geracao, conta; } PedidoLink;
static unsigned geracaoLink, linkConta;
static int linkEstado = -2;            // ver partes_link_estado
static char linkUrl[4096];
static PartesLista linkLista;

static void *fioLink(void *u) {
  PedidoLink *pd = u;
  static PartesLista l;
  static char url[4096];
  static pthread_mutex_t fioTrava = PTHREAD_MUTEX_INITIALIZER;
  int ok = 1, st = 0;
  pthread_mutex_lock(&fioTrava);
  partes_lista_limpar(&l);
  l.atual = -1;
  url[0] = 0;
  if (pd->comLista && pd->conta == debrid_tb_geracao()) {
    char *r = debrid_tb_mylist(pd->usenet, pd->item, 0, 1, &st);
    const char *d = r ? strstr(r, "\"data\"") : NULL;
    ok = 0;
    if (d) { d = strchr(d + 6, ':'); if (d) { d++; while (isspace((unsigned char)*d)) d++; } }
    if (d && *d == '[') { d++; while (isspace((unsigned char)*d)) d++; }
    if (d && *d == '{' && partes_ler_item(d, js_fim(d), pd->usenet, &l)) {
      for (int i = 0; i < l.n; i++) if (l.arq[i].id == pd->arquivo) { l.atual = i; ok = 1; }
      l.estado = PARTES_PRONTO;
      snprintf(l.como, sizeof l.como, "saved");
    }
    if (!ok) printf("[partes] saved part: the TorBox item or file is gone (HTTP %d)\n", st);
    else logEstadoItem(itemDoCorpo(r), "saved part");
    free(r);
  }
  if (pd->conta != debrid_tb_geracao()) ok = 0;
  if (ok) ok = debrid_tb_link(pd->usenet, pd->item, pd->arquivo, url, sizeof url);
  // Falhou: a lista na memoria e a do item podem estar velhas (o item sumiu
  // da conta); o proximo segurar le de novo.
  if (!ok) { listaEsquecer(); memoTirarItem(pd->item); }
  pthread_mutex_lock(&trava);
  if (pd->geracao == geracaoLink && pd->conta == debrid_tb_geracao()) {
    linkEstado = ok ? 1 : -1;
    snprintf(linkUrl, sizeof linkUrl, "%s", ok ? url : "");
    partes_lista_copiar(&linkLista, &l);
    if (linkLista.estado == PARTES_ERRO) linkEstado = -1;
  }
  pthread_mutex_unlock(&trava);
  printf("[partes] link for file id %d of TorBox %s %d: %s\n", pd->arquivo,
         pd->usenet ? "usenet" : "torrent", pd->item, ok ? "ready" : "failed");
  fflush(stdout);
  pthread_mutex_unlock(&fioTrava);
  free(pd);
  return NULL;
}

void partes_link_pedir(int usenet, int item, int arquivo, int comLista) {
  PedidoLink *pd = calloc(1, sizeof *pd);
  pthread_t t;
  pthread_mutex_lock(&trava);
  geracaoLink++;
  linkEstado = 0;
  linkConta = debrid_tb_geracao();
  linkUrl[0] = 0;
  if (pd) { pd->usenet = usenet; pd->item = item; pd->arquivo = arquivo; pd->comLista = comLista; pd->geracao = geracaoLink; pd->conta = linkConta; }
  pthread_mutex_unlock(&trava);
  if (!pd || pthread_create(&t, NULL, fioLink, pd) != 0) {
    free(pd);
    pthread_mutex_lock(&trava);
    linkEstado = -1;
    pthread_mutex_unlock(&trava);
    return;
  }
  pthread_detach(t);
}

int partes_link_estado(char *url, unsigned n, PartesLista *l) {
  int e;
  pthread_mutex_lock(&trava);
  e = linkConta == debrid_tb_geracao() ? linkEstado : -2;
  if (e == 1) {
    if (url && n) snprintf(url, n, "%s", linkUrl);
    if (l && !partes_lista_copiar(l, &linkLista)) e = -1;
  }
  if (e == 1 || e == -1) { linkEstado = -2; linkUrl[0] = 0; }
  pthread_mutex_unlock(&trava);
  return e;
}

void partes_link_cancelar(void) {
  pthread_mutex_lock(&trava);
  geracaoLink++;
  linkEstado = -2;
  linkUrl[0] = 0;
  pthread_mutex_unlock(&trava);
}

// ---------------------------------------------------------------- Airlock
//
// Liga/desliga o Airlock do item da lista (debrid_tb_airlock), em fio proprio:
// a rede nao pode parar o desenho. Um pedido por vez; o pedido novo durante um
// em curso e ignorado (o toque repetido no OK nao empilha PUTs).
typedef struct { int usenet, item, ligar; unsigned geracao, conta; } PedidoAirlock;
static unsigned geracaoAirlock, airConta;
static int airEstado = -2;             // ver partes_airlock_estado
static int airLigado;
static char airDetalhe[160];

// Update memo entries and bypass TorBox's stale list cache on the next fetch.
static void airlockNaMemoria(int usenet, int item, int ligar) {
  pthread_mutex_lock(&trava);
  for (int i = 0; i < PARTES_MEMO; i++)
    if (memo[i].chave[0] && memo[i].l.item == item && memo[i].l.usenet == usenet) memo[i].l.airlock = ligar;
  pthread_mutex_unlock(&trava);
  pthread_mutex_lock(&lcRede);
  pthread_mutex_lock(&lcTrava);
  free(lcache[usenet].corpo);
  memset(&lcache[usenet], 0, sizeof lcache[usenet]);
  lcache[usenet].renovar = 1;
  pthread_mutex_unlock(&lcTrava);
  pthread_mutex_unlock(&lcRede);
}

static void *fioAirlock(void *u) {
  PedidoAirlock *pd = u;
  char det[160] = "";
  int ok = pd->conta == debrid_tb_geracao() && debrid_tb_airlock(pd->usenet, pd->item, pd->ligar, det, sizeof det);
  if (ok && pd->conta == debrid_tb_geracao()) airlockNaMemoria(pd->usenet, pd->item, pd->ligar);
  pthread_mutex_lock(&trava);
  if (pd->geracao == geracaoAirlock && pd->conta == debrid_tb_geracao()) {
    airEstado = ok ? 1 : -1;
    airLigado = pd->ligar;
    snprintf(airDetalhe, sizeof airDetalhe, "%s", det);
  }
  pthread_mutex_unlock(&trava);
  free(pd);
  return NULL;
}

void partes_airlock_pedir(int usenet, int item, int ligar) {
  PedidoAirlock *pd;
  pthread_t t;
  pthread_mutex_lock(&trava);
  if (airEstado == 0 && airConta == debrid_tb_geracao()) { pthread_mutex_unlock(&trava); return; }
  pd = calloc(1, sizeof *pd);
  geracaoAirlock++;
  airEstado = pd ? 0 : -1;
  airConta = debrid_tb_geracao();
  airDetalhe[0] = 0;
  if (pd) { pd->usenet = usenet; pd->item = item; pd->ligar = ligar; pd->geracao = geracaoAirlock; pd->conta = airConta; }
  pthread_mutex_unlock(&trava);
  if (!pd) return;
  if (pthread_create(&t, NULL, fioAirlock, pd) != 0) {
    free(pd);
    pthread_mutex_lock(&trava);
    airEstado = -1;
    pthread_mutex_unlock(&trava);
    return;
  }
  pthread_detach(t);
}

int partes_airlock_estado(int *ligado, char *detalhe, unsigned n) {
  int e;
  pthread_mutex_lock(&trava);
  e = airConta == debrid_tb_geracao() ? airEstado : -2;
  if (e == 1 || e == -1) {
    if (ligado) *ligado = airLigado;
    if (detalhe && n) snprintf(detalhe, n, "%s", airDetalhe);
    airEstado = -2; airDetalhe[0] = 0;
  }
  pthread_mutex_unlock(&trava);
  return e;
}

// ---------------------------------------------------------------- sessao

static char sesAlvo[64];
static PartesLista sesLista;
static int sesK = -1;

void partes_sessao_iniciar(const char *alvo, const PartesLista *l, int k) {
  if (!alvo || !*alvo || !l || k < 0 || k >= l->n || l->arq[k].extra || partes_principais(l) < 2) {
    partes_sessao_parar("not a multi-part file");
    return;
  }
  snprintf(sesAlvo, sizeof sesAlvo, "%s", alvo);
  if (!partes_lista_copiar(&sesLista, l)) { partes_sessao_parar("allocation failed"); return; }
  sesK = k;
  printf("[partes] parts session on: part %d of %d\n", partes_numero(l, k), partes_principais(l));
  fflush(stdout);
}
void partes_sessao_parar(const char *porque) {
  if (sesAlvo[0]) {
    printf("[partes] parts session off: %s\n", porque ? porque : "-");
    fflush(stdout);
  }
  sesAlvo[0] = 0;
  sesK = -1;
}
int partes_sessao_de(const char *alvo) {
  return sesAlvo[0] && alvo && !strcmp(alvo, sesAlvo) && sesK >= 0;
}
const PartesLista *partes_sessao_lista(void) { return sesAlvo[0] ? &sesLista : NULL; }
int partes_sessao_indice(void) { return sesAlvo[0] ? sesK : -1; }
void partes_sessao_ir(int k) { if (sesAlvo[0] && k >= 0 && k < sesLista.n) sesK = k; }

int partes_virtual(const char *alvo, double pos, double dur, double catDur,
                   double *vpos, double *vdur, int *ultima) {
  if (!partes_sessao_de(alvo)) return 0;
  if (!partes_conta_virtual(&sesLista, sesK, pos, dur, catDur, vpos, vdur)) return 0;
  if (ultima) *ultima = partes_seguinte(&sesLista, sesK) < 0;
  return 1;
}

// ---------------------------------------------------------------- arquivo tocando
//
// O ARQUIVO DO TORBOX QUE TOCA AGORA (escolhido na lista, uma parte, o
// Retomar). Com ele a saida do player grava o ponto DESTE arquivo, filme ou
// episodio, e o Retomar volta nele — e nao na fonte que o automatico escolheria
// (log da TV, 16/10: Horimiya E1 escolhido no pacote, Retomar abriu outra
// fonte nos 44%).
static char arqAlvo[64];
static PartesPonto arqPonto;
void partes_arquivo_definir(const char *alvo, int usenet, int item, int arquivo) {
  if (!alvo || !*alvo || item <= 0 || arquivo < 0) { arqAlvo[0] = 0; return; }
  snprintf(arqAlvo, sizeof arqAlvo, "%s", alvo);
  arqPonto.usenet = usenet; arqPonto.item = item; arqPonto.arquivo = arquivo; arqPonto.pos = 0;
}
int partes_arquivo_de(const char *alvo, PartesPonto *p) {
  if (!arqAlvo[0] || !alvo || strcmp(alvo, arqAlvo)) return 0;
  if (p) *p = arqPonto;
  return 1;
}
void partes_arquivo_limpar(void) { arqAlvo[0] = 0; }

// ---------------------------------------------------------------- ponto (disco)
//
// Uma linha por alvo: "<escopo>\t<alvo>\t<usenet>\t<item>\t<arquivo>\t<pos>\t<quando>".
// Escopo = hash da conta + perfil (a conta nao vai em claro para o disco).
#define LINHA 256
static char pontos[PARTES_GUARDADAS][LINHA];
static int nPontos, pontosLidos;
static pthread_mutex_t pontoTrava = PTHREAD_MUTEX_INITIALIZER;

static void escopo(const char *conta, int perfil, const char *alvo, char *b, size_t t) {
  unsigned long long h = 14695981039346656037ULL;
  for (const unsigned char *p = (const unsigned char *)(conta ? conta : ""); *p; p++) { h ^= *p; h *= 1099511628211ULL; }
  snprintf(b, t, "%016llx-p%d\t%s\t", h, perfil > 0 ? perfil : 0, alvo ? alvo : "");
}
static void pontosLer(void) {
  char *t, *l, *ctx = NULL;
  pontosLidos = 1; nPontos = 0;
  t = dados_ler(PARTES_ARQUIVO);
  if (!t) return;
  for (l = strtok_r(t, "\n", &ctx); l && nPontos < PARTES_GUARDADAS; l = strtok_r(NULL, "\n", &ctx))
    if (strchr(l, '\t') && strlen(l) < LINHA) snprintf(pontos[nPontos++], LINHA, "%s", l);
  free(t);
}
static void pontosGravar(void) {
  size_t tam = (size_t)nPontos * (LINHA + 1) + 1, k = 0;
  char *b = malloc(tam);
  if (!b) return;
  b[0] = 0;
  for (int i = 0; i < nPontos; i++) k += (size_t)snprintf(b + k, tam - k, "%s\n", pontos[i]);
  if (nPontos) dados_gravar(PARTES_ARQUIVO, b);
  else dados_gravar(PARTES_ARQUIVO, "");
  free(b);
}
static int pontoAchar(const char *pre) {
  size_t L = strlen(pre);
  for (int i = 0; i < nPontos; i++) if (!strncmp(pontos[i], pre, L)) return i;
  return -1;
}
static void pontoTirar(int i) {
  memmove(pontos[i], pontos[i + 1], (size_t)(nPontos - i - 1) * LINHA);
  nPontos--;
}

void partes_ponto_guardar(const char *alvo, const char *conta, int perfil, const PartesPonto *p) {
  char pre[160];
  int i;
  if (!alvo || !*alvo || !p || p->item <= 0 || p->arquivo < 0) return;
  escopo(conta, perfil, alvo, pre, sizeof pre);
  pthread_mutex_lock(&pontoTrava);
  if (!pontosLidos) pontosLer();
  if ((i = pontoAchar(pre)) >= 0) pontoTirar(i);
  else if (nPontos == PARTES_GUARDADAS) pontoTirar(0);
  snprintf(pontos[nPontos++], LINHA, "%s%d\t%d\t%d\t%.1f\t%lld", pre, p->usenet ? 1 : 0, p->item,
           p->arquivo, p->pos > 0 && isfinite(p->pos) ? p->pos : 0.0, (long long)time(NULL));
  pontosGravar();
  pthread_mutex_unlock(&pontoTrava);
}

int partes_ponto_pegar(const char *alvo, const char *conta, int perfil, PartesPonto *p) {
  char pre[160];
  int i, ok = 0;
  if (!alvo || !*alvo || !p) return 0;
  escopo(conta, perfil, alvo, pre, sizeof pre);
  pthread_mutex_lock(&pontoTrava);
  if (!pontosLidos) pontosLer();
  if ((i = pontoAchar(pre)) >= 0) {
    int u = 0, it = 0, a = 0;
    double pos = 0;
    long long quando = 0;
    if (sscanf(pontos[i] + strlen(pre), "%d\t%d\t%d\t%lf\t%lld", &u, &it, &a, &pos, &quando) == 5 &&
        it > 0 && a >= 0 && (long long)time(NULL) - quando <= PARTES_VALIDADE_S) {
      p->usenet = u; p->item = it; p->arquivo = a; p->pos = pos;
      ok = 1;
    } else {
      pontoTirar(i);
      pontosGravar();
    }
  }
  pthread_mutex_unlock(&pontoTrava);
  return ok;
}

void partes_ponto_esquecer(const char *alvo, const char *conta, int perfil) {
  char pre[160];
  int i;
  if (!alvo || !*alvo) return;
  escopo(conta, perfil, alvo, pre, sizeof pre);
  pthread_mutex_lock(&pontoTrava);
  if (!pontosLidos) pontosLer();
  if ((i = pontoAchar(pre)) >= 0) { pontoTirar(i); pontosGravar(); }
  pthread_mutex_unlock(&pontoTrava);
}

void partes_ponto_zerar(void) {
  pthread_mutex_lock(&pontoTrava);
  nPontos = 0; pontosLidos = 0;
  pthread_mutex_unlock(&pontoTrava);
}
