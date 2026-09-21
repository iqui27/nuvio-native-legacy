#include "trailerapple.h"
#include "rede.h"
#include "js.h"
#include "dados.h"
#include "ajustes.h"
#include <SDL2/SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#define TA_MAX 32
typedef struct {
  char imdb[16];
  char url[1024];       // master da Apple (ABR completo)
  char toca[600];       // file:// do master reduzido a UMA variante, ou vazio
  int  tocaQual;        // teto de qualidade com que `toca` foi montado
  long expira;
  int  emVoo, respondeu;
} Entrada;
static Entrada tab[TA_MAX];
static int nTab, prox;
static SDL_mutex *mtx;

#define UTS_HOST "https://tv.apple.com/api/uts/v3"
#define UTS_QUERY "caller=web&sf=143441&v=76&pfm=appletv&locale=en-US&l=en&utsk=6e3013c6d6fae3c2::::::235656c069bb0efb"
static const char *const CABS[] = { "Accept: application/json", "Origin: https://tv.apple.com", NULL };

static void trancar(void)   { if (!mtx) mtx = SDL_CreateMutex(); SDL_LockMutex(mtx); }
static void destrancar(void){ SDL_UnlockMutex(mtx); }

static Entrada *achar(const char *imdb) {
  int i;
  for (i = 0; i < nTab; i++) if (!strcmp(tab[i].imdb, imdb)) return &tab[i];
  return NULL;
}
static Entrada *reservar(const char *imdb) {
  Entrada *e = achar(imdb);
  if (e) return e;
  if (nTab < TA_MAX) e = &tab[nTab++];
  else { e = &tab[prox]; prox = (prox + 1) % TA_MAX; }
  memset(e, 0, sizeof *e);
  snprintf(e->imdb, sizeof e->imdb, "%s", imdb);
  return e;
}

static void caminhoDisco(const char *imdb, char *dst, unsigned tam) {
  char pasta[512];
  dst[0] = 0;
  if (!dados_caminho(pasta, sizeof pasta, "trailer")) return;
  mkdir(pasta, 0755);
  snprintf(dst, tam, "%s/%s-apple", pasta, imdb);
}
static int lerDisco(Entrada *e) {
  char c[600], linha[1200];
  FILE *f;
  caminhoDisco(e->imdb, c, sizeof c);
  if (!c[0] || !(f = fopen(c, "r"))) return 0;
  if (!fgets(linha, sizeof linha, f)) { fclose(f); return 0; }
  e->expira = atol(linha);
  e->url[0] = 0;
  if (fgets(linha, sizeof linha, f)) { linha[strcspn(linha, "\r\n")] = 0; if (strcmp(linha, "-")) snprintf(e->url, sizeof e->url, "%s", linha); }
  fclose(f);
  return 1;
}
static void gravarDisco(const Entrada *e) {
  char c[600];
  FILE *f;
  caminhoDisco(e->imdb, c, sizeof c);
  if (!c[0] || !(f = fopen(c, "w"))) return;
  fprintf(f, "%ld\n%s\n", e->expira, e->url[0] ? e->url : "-");
  fclose(f);
  dados_marcar_sujo(1);   // IDBFS (Samsung): cache re-obtivel, descarga leve
}
static int valido(const Entrada *e) { return e->respondeu && e->expira > (long)time(NULL); }

static void caminhoMaster(const char *imdb, char *dst, unsigned tam, const char *sufixo) {
  char pasta[512];
  dst[0] = 0;
  if (!dados_caminho(pasta, sizeof pasta, "trailer")) return;
  snprintf(dst, tam, "%s/%s-apple%s", pasta, imdb, sufixo);
}

// UMA VARIANTE, NAO O ABR. Medido na C9 (20/09/2026): entregue o master
// inteiro, o uMS comeca pela variante mais baixa (556x232) e sobe aos poucos —
// e cada troca muda o tamanho do quadro, o que invalida o recorte de fonte
// calculado no videoInfo anterior. Um master reduzido a uma variante (com o
// grupo de audio dela) toca fixo na definicao escolhida desde o primeiro
// quadro. Escolha: a maior largura que cabe no teto dos Ajustes, sem Dolby
// Vision (dvh1/dvhe: trailer mudo no fundo nao e lugar para trocar o modo da
// TV), avc1 preferido a hvc1 na mesma largura (decodifica em qualquer LG).
static int atributo(const char *linha, const char *nome, char *dst, unsigned tam) {
  const char *p = linha;
  size_t n = strlen(nome);
  unsigned k = 0;
  while ((p = strstr(p, nome)) != NULL) {
    if ((p == linha || p[-1] == ',' || p[-1] == ':') && p[n] == '=') {
      p += n + 1;
      if (*p == '"') { p++; while (*p && *p != '"' && k + 1 < tam) dst[k++] = *p++; }
      else while (*p && *p != ',' && *p != '\r' && *p != '\n' && k + 1 < tam) dst[k++] = *p++;
      dst[k] = 0;
      return 1;
    }
    p += n;
  }
  dst[0] = 0;
  return 0;
}

static int montarReduzido(const char *imdb, int teto, char *saida, unsigned tam) {
  char cm[600], cr[600], linha[2048];
  char melhorInf[2048] = "", melhorUri[1024] = "", melhorAudio[128] = "";
  int melhorW = 0, melhorAvc = 0;
  FILE *f, *g;
  caminhoMaster(imdb, cm, sizeof cm, ".m3u8");
  caminhoMaster(imdb, cr, sizeof cr, "-play.m3u8");
  if (!cm[0] || !(f = fopen(cm, "r"))) return 0;
  // Passo 1: escolher a variante.
  while (fgets(linha, sizeof linha, f)) {
    char res[32], cod[128], aud[128], uri[1024];
    int w = 0, avc;
    if (strncmp(linha, "#EXT-X-STREAM-INF:", 18)) continue;
    atributo(linha, "RESOLUTION", res, sizeof res);
    atributo(linha, "CODECS", cod, sizeof cod);
    atributo(linha, "AUDIO", aud, sizeof aud);
    w = atoi(res);
    if (w <= 0) continue;
    if (strstr(cod, "dvh1") || strstr(cod, "dvhe")) continue;
    if (teto && w > (teto >= 1080 ? 1920 : teto >= 720 ? 1280 : 864)) continue;
    avc = strstr(cod, "avc1") != NULL;
    if (!fgets(uri, sizeof uri, f)) break;
    uri[strcspn(uri, "\r\n")] = 0;
    if (uri[0] == '#' || strncmp(uri, "http", 4)) continue;
    if (w > melhorW || (w == melhorW && avc && !melhorAvc)) {
      melhorW = w; melhorAvc = avc;
      snprintf(melhorInf, sizeof melhorInf, "%s", linha);
      snprintf(melhorUri, sizeof melhorUri, "%s", uri);
      snprintf(melhorAudio, sizeof melhorAudio, "%s", aud);
    }
  }
  if (!melhorW) { fclose(f); return 0; }
  // Passo 2: escrever o reduzido com o grupo de audio da variante.
  g = fopen(cr, "w");
  if (!g) { fclose(f); return 0; }
  fprintf(g, "#EXTM3U\n#EXT-X-VERSION:6\n#EXT-X-INDEPENDENT-SEGMENTS\n");
  rewind(f);
  while (fgets(linha, sizeof linha, f)) {
    char grp[128];
    if (strncmp(linha, "#EXT-X-MEDIA:", 13)) continue;
    if (!strstr(linha, "TYPE=AUDIO")) continue;
    atributo(linha, "GROUP-ID", grp, sizeof grp);
    if (melhorAudio[0] && !strcmp(grp, melhorAudio)) { fputs(linha, g); break; }
  }
  fputs(melhorInf, g);
  fprintf(g, "%s\n", melhorUri);
  fclose(g); fclose(f);
  printf("[trailer] apple %s: variante %dpx %s (teto %d)\n", imdb, melhorW, melhorAvc ? "avc1" : "hvc1", teto);
  fflush(stdout);
  snprintf(saida, tam, "file://%s", cr);
  return 1;
}

// NORMALIZACAO igual a do tvOS: minusculas, sem acento (so o bloco latino de
// dois bytes, que e o que titulo de filme tem), "&" vira " and ", tudo que
// nao e letra ou digito vira espaco, espacos colapsados. Artigos FICAM: "The
// Batman" e "Batman" sao filmes diferentes.
static void normalizar(const char *s, char *dst, unsigned tam) {
  static const char *const acent = "ÀÁÂÃÄÅàáâãäåÈÉÊËèéêëÌÍÎÏìíîïÒÓÔÕÖØòóôõöøÙÚÛÜùúûüÝýÿÑñÇç";
  static const char *const base  = "AAAAAAaaaaaaEEEEeeeeIIIIiiiiOOOOOOooooooUUUUuuuuYyyNnCc";
  unsigned k = 0;
  int espaco = 1;
  const unsigned char *p = (const unsigned char *)s;
  while (*p && k + 2 < tam) {
    int c = -1;
    if (*p == '&') { c = ' '; /* " and " abaixo */
      if (!espaco && k + 1 < tam) dst[k++] = ' ';
      if (k + 4 < tam) { memcpy(dst + k, "and ", 4); k += 4; }
      espaco = 1; p++; continue; }
    if (*p < 0x80) { c = *p++; }
    else if ((*p & 0xE0) == 0xC0 && p[1]) {
      const char *q = acent; int i = 0; c = ' ';
      while (*q) { if ((unsigned char)q[0] == p[0] && (unsigned char)q[1] == p[1]) { c = base[i]; break; } q += 2; i++; }
      p += 2;
    } else { c = ' '; while ((*p & 0xC0) == 0x80 || *p >= 0xC0) { p++; if (!*p) break; if ((*p & 0xC0) != 0x80) break; } }
    if (isalnum(c)) { dst[k++] = (char)tolower(c); espaco = 0; }
    else if (!espaco) { dst[k++] = ' '; espaco = 1; }
  }
  while (k && dst[k - 1] == ' ') k--;
  dst[k] = 0;
}

static int anoDe(const char *meta) {
  const char *p = meta;
  while (p && *p) {
    if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1]) && isdigit((unsigned char)p[2]) && isdigit((unsigned char)p[3]) &&
        !isdigit((unsigned char)p[4])) { int a = atoi(p); if (a > 1880 && a < 2100) return a; }
    p++;
  }
  return 0;
}

static void urlEncode(const char *s, char *dst, unsigned tam) {
  static const char hex[] = "0123456789ABCDEF";
  unsigned k = 0;
  for (; *s && k + 4 < tam; s++) {
    unsigned char c = (unsigned char)*s;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') dst[k++] = (char)c;
    else { dst[k++] = '%'; dst[k++] = hex[c >> 4]; dst[k++] = hex[c & 15]; }
  }
  dst[k] = 0;
}

static int anoDeEpoca(double ms) {
  time_t t = (time_t)(ms / 1000.0);
  struct tm tmv;
  if (ms <= 0) return 0;
  gmtime_r(&t, &tmv);
  return tmv.tm_year + 1900;
}

// A CHAVE NO NIVEL DE CIMA do objeto, e nao a primeira ocorrencia: um item da
// busca traz genres[0].type="Genre" e genres[0].id ANTES do type/id dele
// proprio, e js_texto (que acha a primeira) devolvia o genero. Anda pelo
// objeto pulando cada valor aninhado inteiro. Devolve o ponteiro para a chave
// (para js_texto/js_num lerem dali) ou NULL.
static const char *chaveTopo(const char *obj, const char *fim, const char *chave) {
  const char *p = obj;
  size_t n = strlen(chave);
  if (!p || *p != '{') return NULL;
  p++;
  while (p < fim && *p) {
    const char *k;
    while (p < fim && (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
    if (p >= fim || *p == '}') return NULL;
    if (*p != '"') return NULL;
    k = p + 1;
    p = k;
    while (p < fim && *p && *p != '"') { if (*p == '\\') p++; p++; }
    if (p >= fim) return NULL;
    if ((size_t)(p - k) == n && !strncmp(k, chave, n)) return k - 1;
    p++;
    while (p < fim && (*p == ' ' || *p == ':')) p++;
    if (*p == '{' || *p == '[') p = js_fim(p);
    else if (*p == '"') { p++; while (p < fim && *p && *p != '"') { if (*p == '\\') p++; p++; } if (p < fim) p++; }
    else while (p < fim && *p && *p != ',' && *p != '}') p++;
  }
  return NULL;
}

// Busca: um unico id com tipo certo, titulo normalizado igual e ano +-1.
// Dois candidatos distintos = nenhum (remake do mesmo ano e o caso em que
// adivinhar erra).
static int buscarId(const char *titulo, int ano, int serie, char *id, unsigned tam) {
  char norm[256], enc[768], url[1100], achado[80] = "";
  char *corpo;
  const char *fim, *shelves, *sh;
  int n = 0;
  normalizar(titulo, norm, sizeof norm);
  if (!norm[0]) return 0;
  urlEncode(titulo, enc, sizeof enc);
  snprintf(url, sizeof url, UTS_HOST "/search?" UTS_QUERY "&searchTerm=%s", enc);
  corpo = rede_baixar_com(url, 12, CABS);
  if (!corpo) return -1;
  fim = corpo + strlen(corpo);
  shelves = js_array(corpo, fim, "shelves");
  for (sh = shelves; sh; sh = js_prox(js_fim(sh))) {
    const char *she = js_fim(sh), *it;
    for (it = js_array(sh, she, "items"); it; it = js_prox(js_fim(it))) {
      const char *ie = js_fim(it);
      char tipo[16] = "", iid[80], tit[256], tn[256];
      const char *kt = chaveTopo(it, ie, "type"), *ki = chaveTopo(it, ie, "id"),
                 *kn = chaveTopo(it, ie, "title"), *kd = chaveTopo(it, ie, "releaseDate");
      int a;
      if (kt) js_texto(kt, ie, "type", tipo, sizeof tipo);
      if (strcmp(tipo, serie ? "Show" : "Movie")) continue;
      if (!ki || !kn || !js_texto(ki, ie, "id", iid, sizeof iid) || !js_texto(kn, ie, "title", tit, sizeof tit)) continue;
      normalizar(tit, tn, sizeof tn);
      if (strcmp(tn, norm)) continue;
      a = kd ? anoDeEpoca(js_num(kd, ie, "releaseDate", 0)) : 0;
      if (!a || abs(a - ano) > 1) continue;
      if (achado[0] && strcmp(achado, iid)) { n = 2; break; }
      if (!achado[0]) { snprintf(achado, sizeof achado, "%s", iid); n = 1; }
    }
    if (n > 1) break;
  }
  free(corpo);
  if (n != 1) return 0;
  snprintf(id, tam, "%s", achado);
  return 1;
}

// backgroundVideo primeiro (e o loop que a propria Apple toca atras do titulo
// e traz o 4K mais vezes), movieClips depois. canvas.shelves NAO: e "mais como
// este", trailer de OUTRO filme.
static int hlsDe(const char *id, int serie, char *url, unsigned tam) {
  char u[1100];
  char *corpo;
  const char *fim, *data, *content, *bg, *assets, *pl;
  int ok = 0;
  snprintf(u, sizeof u, UTS_HOST "/%s/%s?" UTS_QUERY, serie ? "shows" : "movies", id);
  corpo = rede_baixar_com(u, 12, CABS);
  if (!corpo) return -1;
  fim = corpo + strlen(corpo);
  data = strstr(corpo, "\"data\"");
  content = data ? strstr(data, "\"content\"") : NULL;
  bg = content ? strstr(content, "\"backgroundVideo\"") : NULL;
  assets = bg ? strstr(bg, "\"assets\"") : NULL;
  if (assets && js_texto(assets, fim, "hlsUrl", url, tam) && url[0]) ok = 1;
  if (!ok) {
    pl = data ? strstr(data, "\"playables\"") : NULL;
    if (pl) { const char *mc = strstr(pl, "\"movieClips\"");
      if (mc && js_texto(mc, fim, "hlsUrl", url, tam) && url[0]) ok = 1; }
  }
  free(corpo);
  return ok;
}

typedef struct { char imdb[16], titulo[200], meta[96]; int serie; } Pedido;

static void *buscar(void *arg) {
  Pedido *p = arg;
  char id[80], url[1024] = "";
  int ano = anoDe(p->meta), r = 0, semResposta = 0;
  Entrada *e;
  if (ano) {
    int serie = p->serie;
    r = buscarId(p->titulo, ano, serie, id, sizeof id);
    // O catalogo nem sempre etiqueta o tipo (item de "continuar assistindo"
    // sem `tipo`, colecao): sem casar como pedido, tenta o outro tipo. Um
    // filme e uma serie com o mesmo nome e ano e caso raro; nao achar o
    // trailer de uma serie da Apple por falta de etiqueta era o caso comum.
    if (r == 0) { serie = !serie; r = buscarId(p->titulo, ano, serie, id, sizeof id); }
    if (r < 0) semResposta = 1;
    else if (r == 1) { r = hlsDe(id, serie, url, sizeof url); if (r < 0) semResposta = 1; }
  }
#ifndef __EMSCRIPTEN__
  if (url[0]) {
    // O master vai para o disco: e dele que sai o reduzido de uma variante.
    // (Na Samsung o <video> do navegador toca o master inteiro com ABR e
    // object-fit:cover absorve a troca de tamanho; nao ha reduzido.)
    char cm[600];
    char *m = rede_baixar_com(url, 12, CABS);
    caminhoMaster(p->imdb, cm, sizeof cm, ".m3u8");
    if (m && cm[0]) { FILE *g = fopen(cm, "w"); if (g) { fputs(m, g); fclose(g); } }
    free(m);
  }
#endif
  printf("[trailer] apple %s (%s %d): %s\n", p->imdb, p->titulo, ano,
         semResposta ? "sem resposta" : url[0] ? "HLS" : !ano ? "sem ano" : "sem trailer");
  fflush(stdout);
  trancar();
  e = reservar(p->imdb);
  e->emVoo = 0; e->respondeu = 1;
  snprintf(e->url, sizeof e->url, "%s", url);
  e->toca[0] = 0; e->tocaQual = -1;
  e->expira = semResposta ? 0 : (long)time(NULL) + (url[0] ? 3600 : 12 * 3600);
  if (!semResposta) gravarDisco(e);
  destrancar();
  free(p);
  return NULL;
}

void trailerapple_pedir(const char *imdb, const char *titulo, const char *meta, int serie) {
  Entrada *e;
  Pedido *p;
  pthread_t f;
  if (!imdb || !imdb[0] || !titulo || !titulo[0]) return;
  trancar();
  e = reservar(imdb);
  if (!e->respondeu && !e->emVoo && lerDisco(e)) {
    // Resposta do disco so vale com o master junto: sem ele nao ha reduzido,
    // e o master inteiro (ABR) nao serve — o quadro muda de tamanho no meio
    // e o recorte fica errado.
    char cm[600];
    struct stat st;
    caminhoMaster(imdb, cm, sizeof cm, ".m3u8");
#ifdef __EMSCRIPTEN__
    (void)st; (void)cm; e->respondeu = 1;
#else
    e->respondeu = !e->url[0] || (cm[0] && stat(cm, &st) == 0);
#endif
  }
  if (e->emVoo || valido(e)) { destrancar(); return; }
  e->emVoo = 1; e->respondeu = 0;
  destrancar();
  p = calloc(1, sizeof *p);
  if (!p) return;
  snprintf(p->imdb, sizeof p->imdb, "%s", imdb);
  snprintf(p->titulo, sizeof p->titulo, "%s", titulo);
  snprintf(p->meta, sizeof p->meta, "%s", meta ? meta : "");
  p->serie = serie;
  if (pthread_create(&f, NULL, buscar, p) == 0) pthread_detach(f);
  else { free(p); trancar(); e->emVoo = 0; destrancar(); }
}

const char *trailerapple_url(const char *imdb) {
  Entrada *e;
  const char *r = NULL;
  if (!imdb || !imdb[0]) return NULL;
  trancar();
  e = achar(imdb);
  if (e && valido(e) && e->url[0]) {
#ifdef __EMSCRIPTEN__
    r = e->url;
#else
    int teto = ajustes_trailer_qualidade();
    if (e->tocaQual != teto) {
      e->tocaQual = teto;
      if (!montarReduzido(imdb, teto, e->toca, sizeof e->toca)) e->toca[0] = 0;
    }
    // Sem reduzido, sem Apple: quem chama cai no IMDb.
    r = e->toca[0] ? e->toca : NULL;
#endif
  }
  destrancar();
  return r;
}

int trailerapple_respondeu(const char *imdb) {
  Entrada *e;
  int r;
  if (!imdb || !imdb[0]) return 1;
  trancar();
  e = achar(imdb);
  r = !e || e->respondeu;
  destrancar();
  return r;
}
