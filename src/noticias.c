// Ver noticias.h.
#include "noticias.h"
#include "rede.h"
#include "dados.h"
#include "ajustes.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NOT_ENTRADAS 24
#define NOT_VALIDADE (6 * 3600)

typedef struct {
  char imdb[40];
  int  n, respondeu, emVoo;
  long quando;                 // epoch da resposta
  Noticia itens[NOT_MAX];
} Entrada;

static Entrada ent[NOT_ENTRADAS];
static int nEnt;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

static Entrada *achar(const char *imdb) {
  int i;
  for (i = 0; i < nEnt; i++) if (!strcmp(ent[i].imdb, imdb)) return &ent[i];
  return NULL;
}
static Entrada *reservar(const char *imdb) {
  Entrada *e = achar(imdb);
  if (e) return e;
  // Cheio: recicla a mais velha que nao esta em voo.
  if (nEnt < NOT_ENTRADAS) e = &ent[nEnt++];
  else {
    int i, v = -1;
    for (i = 0; i < nEnt; i++)
      if (!ent[i].emVoo && (v < 0 || ent[i].quando < ent[v].quando)) v = i;
    if (v < 0) v = 0;
    e = &ent[v];
  }
  memset(e, 0, sizeof *e);
  snprintf(e->imdb, sizeof e->imdb, "%s", imdb);
  return e;
}

// --- texto -------------------------------------------------------------------

static void codificar(const char *s, char *dst, size_t cap) {
  size_t z = 0;
  const unsigned char *c = (const unsigned char *)s;
  for (; *c && z + 4 < cap; c++) {
    if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') ||
        *c == '-' || *c == '_' || *c == '.') dst[z++] = (char)*c;
    else if (*c == ' ') dst[z++] = '+';
    else { snprintf(dst + z, 4, "%%%02X", *c); z += 3; }
  }
  dst[z] = 0;
}

// Entidades que o Google poe no titulo dentro do XML: &amp; &#39; &quot; e
// as numericas. So o que aparece de fato nas manchetes.
static void desentidar(char *s) {
  char *r = s, *w = s;
  while (*r) {
    if (*r == '&') {
      if (!strncmp(r, "&amp;", 5))       { *w++ = '&';  r += 5; continue; }
      if (!strncmp(r, "&quot;", 6))      { *w++ = '"';  r += 6; continue; }
      if (!strncmp(r, "&apos;", 6))      { *w++ = '\''; r += 6; continue; }
      if (!strncmp(r, "&lt;", 4))        { *w++ = '<';  r += 4; continue; }
      if (!strncmp(r, "&gt;", 4))        { *w++ = '>';  r += 4; continue; }
      if (r[1] == '#') {
        long v = strtol(r + 2 + (r[2] == 'x' || r[2] == 'X'), NULL, (r[2] == 'x' || r[2] == 'X') ? 16 : 10);
        char *fim = strchr(r, ';');
        if (fim && v > 0) {
          // UTF-8 de ate 3 bytes: o que uma manchete usa.
          if (v < 0x80) *w++ = (char)v;
          else if (v < 0x800) { *w++ = (char)(0xC0 | (v >> 6)); *w++ = (char)(0x80 | (v & 0x3F)); }
          else { *w++ = (char)(0xE0 | (v >> 12)); *w++ = (char)(0x80 | ((v >> 6) & 0x3F)); *w++ = (char)(0x80 | (v & 0x3F)); }
          r = fim + 1; continue;
        }
      }
    }
    *w++ = *r++;
  }
  *w = 0;
}

// Copia o conteudo entre <tag> e </tag> a partir de `de`; devolve onde parou.
static const char *campo(const char *de, const char *fim, const char *tag, char *dst, size_t cap) {
  char ab[40], fe[40];
  const char *a, *b;
  size_t n;
  dst[0] = 0;
  snprintf(ab, sizeof ab, "<%s", tag);
  snprintf(fe, sizeof fe, "</%s>", tag);
  a = strstr(de, ab);
  if (!a || a >= fim) return NULL;
  a = strchr(a, '>');
  if (!a) return NULL;
  a++;
  b = strstr(a, fe);
  if (!b || b > fim) return NULL;
  // CDATA, quando ha.
  if (!strncmp(a, "<![CDATA[", 9)) { a += 9; if (b - 3 > a && !strncmp(b - 3, "]]>", 3)) b -= 3; }
  n = (size_t)(b - a);
  if (n >= cap) n = cap - 1;
  memcpy(dst, a, n); dst[n] = 0;
  return b;
}

// "Sat, 20 Sep 2026 12:00:00 GMT" -> "20 Sep" / "20 set".
static long dataCurta(const char *rfc, char *dst, size_t cap) {
  static const char *EN[] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
  static const char *PT[] = { "jan","fev","mar","abr","mai","jun","jul","ago","set","out","nov","dez" };
  int d = 0, m = -1, i, ano = 0;
  char mes[8] = "";
  dst[0] = 0;
  if (sscanf(rfc, "%*[^,], %d %7s %d", &d, mes, &ano) < 2) return 0;
  for (i = 0; i < 12; i++) if (!strncmp(mes, EN[i], 3)) m = i;
  if (m < 0) return 0;
  // O ANO so quando nao e o corrente: "25 set 2025" ao lado de "19 set" diz
  // que uma e velha sem gastar a largura da linha nas novas.
  { time_t agora = time(NULL); struct tm *tmp = gmtime(&agora);
    int anoAtual = tmp ? tmp->tm_year + 1900 : 0;
    if (ano && ano != anoAtual)
      snprintf(dst, cap, "%d %s %d", d, ajustes_idioma_ingles() ? EN[m] : PT[m], ano);
    else
      snprintf(dst, cap, "%d %s", d, ajustes_idioma_ingles() ? EN[m] : PT[m]); }
  return (long)ano * 10000L + (long)(m + 1) * 100L + d;
}

static void interpretar(Entrada *e, const char *xml) {
  const char *p = xml;
  e->n = 0;
  while (e->n < NOT_MAX && (p = strstr(p, "<item>"))) {
    const char *fim = strstr(p, "</item>");
    Noticia *nt = &e->itens[e->n];
    char buf[300], fonte[120];
    if (!fim) break;
    memset(nt, 0, sizeof *nt);
    if (campo(p, fim, "title", buf, sizeof buf)) {
      desentidar(buf);
      snprintf(nt->titulo, sizeof nt->titulo, "%s", buf);
    }
    if (campo(p, fim, "source", fonte, sizeof fonte)) {
      desentidar(fonte);
      snprintf(nt->fonte, sizeof nt->fonte, "%s", fonte);
      // A manchete do Google termina em " - Veiculo"; tirar, que o veiculo vai
      // na linha de baixo.
      { size_t lt = strlen(nt->titulo), lf = strlen(nt->fonte);
        if (lf && lt > lf + 3 && !strcmp(nt->titulo + lt - lf, nt->fonte) &&
            !strncmp(nt->titulo + lt - lf - 3, " - ", 3))
          nt->titulo[lt - lf - 3] = 0; }
    }
    if (campo(p, fim, "pubDate", buf, sizeof buf)) nt->chave = dataCurta(buf, nt->data, sizeof nt->data);
    if (nt->titulo[0]) e->n++;
    p = fim + 7;
  }
  // DA MAIS NOVA PARA A MAIS VELHA. O RSS de busca vem por relevancia, e a
  // linha da Agenda mostra so a primeira: tem de ser a ultima noticia.
  { int i, j;
    for (i = 1; i < e->n; i++)
      for (j = i; j > 0 && e->itens[j].chave > e->itens[j - 1].chave; j--) {
        Noticia t = e->itens[j]; e->itens[j] = e->itens[j - 1]; e->itens[j - 1] = t;
      } }
}

// --- disco -------------------------------------------------------------------

static void nomeDisco(const char *imdb, char *dst, size_t cap) {
  snprintf(dst, cap, "noticias-%s-%s.txt", imdb, ajustes_idioma_ingles() ? "en" : "pt");
}
static void gravar(const Entrada *e) {
  char nome[80], *txt;
  size_t cap = 64 + (size_t)e->n * (sizeof(Noticia) + 8), k = 0;
  int i;
  txt = malloc(cap);
  if (!txt) return;
  k += (size_t)snprintf(txt + k, cap - k, "%ld\n", e->quando);
  for (i = 0; i < e->n && k < cap; i++)
    k += (size_t)snprintf(txt + k, cap - k, "%ld\t%s\t%s\t%s\n", e->itens[i].chave, e->itens[i].data, e->itens[i].fonte, e->itens[i].titulo);
  nomeDisco(e->imdb, nome, sizeof nome);
  dados_gravar_leve(nome, txt);
  free(txt);
}
static int lerDisco(Entrada *e) {
  char nome[80], *txt, *l, *prox;
  long q;
  nomeDisco(e->imdb, nome, sizeof nome);
  txt = dados_ler(nome);
  if (!txt) return 0;
  q = atol(txt);
  if (q <= 0 || time(NULL) - q > NOT_VALIDADE) { free(txt); return 0; }
  e->quando = q; e->n = 0;
  l = strchr(txt, '\n');
  while (l && *++l && e->n < NOT_MAX) {
    Noticia *nt = &e->itens[e->n];
    char *t0, *t1, *t2;
    prox = strchr(l, '\n'); if (prox) *prox = 0;
    t0 = strchr(l, '\t'); if (!t0) { l = prox; continue; }
    *t0++ = 0;
    t1 = strchr(t0, '\t'); if (!t1) { l = prox; continue; }
    *t1++ = 0;
    t2 = strchr(t1, '\t'); if (!t2) { l = prox; continue; }
    *t2++ = 0;
    nt->chave = atol(l);
    snprintf(nt->data, sizeof nt->data, "%s", t0);
    snprintf(nt->fonte, sizeof nt->fonte, "%s", t1);
    snprintf(nt->titulo, sizeof nt->titulo, "%s", t2);
    e->n++;
    l = prox;
  }
  free(txt);
  return 1;
}

// --- rede --------------------------------------------------------------------

typedef struct { char imdb[40]; char titulo[200]; char rede[64]; int serie; } Pedido;

static void *buscar(void *arg) {
  Pedido *p = arg;
  char q[700], url[900], *xml;
  Entrada *e;
  int en = ajustes_idioma_ingles();
  // Titulo entre aspas mais a palavra de apoio: "Silo" serie acha a serie e
  // nao o armazem.
  // A REDE entra entre aspas quando se sabe ("Foundation" "Apple TV+"): sem
  // ela a busca por "Foundation" trazia a Wikimedia Foundation.
  if (p->rede[0]) snprintf(q, sizeof q, "\"%s\" \"%s\"", p->titulo, p->rede);
  else snprintf(q, sizeof q, "\"%s\" %s", p->titulo,
                p->serie ? (en ? "series" : "s\xc3\xa9rie") : (en ? "movie" : "filme"));
  { char qc[900]; codificar(q, qc, sizeof qc);
    snprintf(url, sizeof url, "https://news.google.com/rss/search?q=%s&hl=%s&gl=%s&ceid=%s",
             qc, en ? "en-US" : "pt-BR", en ? "US" : "BR", en ? "US:en" : "BR:pt-419"); }
  xml = rede_baixar(url, 12);
  pthread_mutex_lock(&trava);
  e = achar(p->imdb);
  if (e) {
    e->n = 0;
    if (xml) interpretar(e, xml);
    e->quando = (long)time(NULL);
    e->respondeu = 1; e->emVoo = 0;
    if (xml) gravar(e);
    printf("[noticias] %s (%s): %d manchete(s)%s\n", p->imdb, p->titulo, e->n, xml ? "" : " (rede falhou)");
    fflush(stdout);
  }
  pthread_mutex_unlock(&trava);
  free(xml);
  free(p);
  return NULL;
}

void noticias_pedir(const char *imdb, const char *titulo, const char *rede, int serie) {
  Entrada *e;
  Pedido *p;
  pthread_t f;
  if (!imdb || !imdb[0] || !titulo || !titulo[0]) return;
  pthread_mutex_lock(&trava);
  e = reservar(imdb);
  if (!e->respondeu && !e->emVoo && lerDisco(e)) e->respondeu = 1;
  if (e->emVoo || (e->respondeu && time(NULL) - e->quando <= NOT_VALIDADE)) { pthread_mutex_unlock(&trava); return; }
  e->emVoo = 1; e->respondeu = 0;
  pthread_mutex_unlock(&trava);
  p = calloc(1, sizeof *p);
  if (!p) return;
  snprintf(p->imdb, sizeof p->imdb, "%s", imdb);
  snprintf(p->titulo, sizeof p->titulo, "%s", titulo);
  snprintf(p->rede, sizeof p->rede, "%s", rede ? rede : "");
  p->serie = serie;
  if (pthread_create(&f, NULL, buscar, p) == 0) pthread_detach(f);
  else { free(p); pthread_mutex_lock(&trava); e->emVoo = 0; pthread_mutex_unlock(&trava); }
}

int noticias_respondeu(const char *imdb) {
  Entrada *e; int r;
  pthread_mutex_lock(&trava);
  e = imdb ? achar(imdb) : NULL;
  r = e ? e->respondeu : 0;
  pthread_mutex_unlock(&trava);
  return r;
}
int noticias_n(const char *imdb) {
  Entrada *e; int r;
  pthread_mutex_lock(&trava);
  e = imdb ? achar(imdb) : NULL;
  r = (e && e->respondeu) ? e->n : 0;
  pthread_mutex_unlock(&trava);
  return r;
}
// O ponteiro aponta para a tabela estatica; o laco de desenho le no mesmo fio
// em que noticias_n foi chamada, e a entrada so e reciclada por noticias_pedir
// (mesmo fio). E o mesmo contrato de agenda_lista.
const Noticia *noticias_item(const char *imdb, int i) {
  Entrada *e = imdb ? achar(imdb) : NULL;
  if (!e || !e->respondeu || i < 0 || i >= e->n) return NULL;
  return &e->itens[i];
}
