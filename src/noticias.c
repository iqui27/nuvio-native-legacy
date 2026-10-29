// Ver noticias.h.
#include "noticias.h"
#include "rede.h"
#include "dados.h"
#include "ajustes.h"
#include "idiomacod.h"
#include "idioma.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

#define NOT_ENTRADAS 24
#define NOT_VALIDADE (6 * 3600)

typedef struct {
  char imdb[40];
  // A LINGUA faz parte da chave: a busca e o arquivo de disco ja eram por
  // idioma, mas a memoria nao — trocar de idioma com a Agenda aberta
  // continuava mostrando as manchetes do anterior por ate 6 h (captura -fx-ru,
  // 29/09/2026: citacao em portugues numa tela em russo).
  int  lg;
  int  n, respondeu, emVoo;
  long quando;                 // epoch da resposta
  Noticia itens[NOT_MAX];
} Entrada;

static Entrada ent[NOT_ENTRADAS];
static int nEnt;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

static Entrada *acharLg(const char *imdb, int lg) {
  int i;
  for (i = 0; i < nEnt; i++) if (ent[i].lg == lg && !strcmp(ent[i].imdb, imdb)) return &ent[i];
  return NULL;
}
static Entrada *achar(const char *imdb) { return acharLg(imdb, ajustes_idioma()); }
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
  e->lg = ajustes_idioma();
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

// O QUE MUDA POR IDIOMA na busca de manchetes, indexado por IDIOMA_*: o sufixo
// do arquivo de cache (uma lista por idioma, para trocar de idioma nao mostrar
// a do anterior), a palavra de apoio da busca ("Silo" serie acha a serie) e o
// trio hl/gl/ceid do Google News. As palavras sao as que o jornalismo local usa,
// nao traducao literal do menu.
static const struct { const char *cod, *serie, *filme, *hl, *gl, *ceid; } PAR[IDIOMA_N] = {
  { "pt", "s\xc3\xa9rie",                       "filme",                        "pt-BR", "BR", "BR:pt-419" },
  { "en", "series",                              "movie",                        "en-US", "US", "US:en" },
  { "ro", "serial",                              "film",                         "ro",    "RO", "RO:ro" },
  { "uk", "серіал", "фільм", "uk", "UA", "UA:uk" },
  { "ru", "сериал", "фильм", "ru", "RU", "RU:ru" },
  { "fr", "s\xc3\xa9rie",                       "film",                         "fr",    "FR", "FR:fr" },
  { "de", "Serie",                               "Film",                         "de",    "DE", "DE:de" },
  { "es", "serie",                               "película",                     "es",    "ES", "ES:es" },
  // Os 22 de 2026-09. hl/gl/ceid conferidos contra news.google.com em 29/09/2026
  // (RSS com itens = ok). DUAS edicoes NAO EXISTEM: dinamarques e bosnio
  // respondem 302 para a edicao en-US, e a noticia vem em ingles (o titulo entre
  // aspas continua achando a serie); ficam com o trio certo para o dia em que o
  // Google as abrir. A edicao servia (RS:sr) devolve manchetes em CIRILICO.
  { "it", "serie tv", "film", "it", "IT", "IT:it" },
  { "nl", "serie", "film", "nl", "NL", "NL:nl" },
  { "pl", "serial", "film", "pl", "PL", "PL:pl" },
  { "tr", "dizi", "film", "tr", "TR", "TR:tr" },
  { "ptpt", "s\xc3\xa9rie", "filme", "pt-PT", "PT", "PT:pt-150" },
  { "sv", "serie", "film", "sv", "SE", "SE:sv" },
  { "da", "serie", "film", "da", "DK", "DK:da" },
  { "no", "serie", "film", "no", "NO", "NO:no" },
  { "cs", "seri\xc3\xa1l", "film", "cs", "CZ", "CZ:cs" },
  { "sk", "seri\xc3\xa1l", "film", "sk", "SK", "SK:sk" },
  { "sl", "serija", "film", "sl", "SI", "SI:sl" },
  { "hu", "sorozat", "film", "hu", "HU", "HU:hu" },
  { "lt", "serialas", "filmas", "lt", "LT", "LT:lt" },
  { "bs", "serija", "film", "bs", "BA", "BA:bs" },
  { "sr", "serija", "film", "sr", "RS", "RS:sr" },
  { "bg", "сериал", "филм", "bg", "BG", "BG:bg" },
  { "el", "σειρά", "ταινία", "el", "GR", "GR:el" },
  { "id", "serial", "film", "id", "ID", "ID:id" },
  { "vi", "phim truyền hình", "phim", "vi", "VN", "VN:vi" },
  { "ja", "ドラマ", "映画", "ja", "JP", "JP:ja" },
  { "zhcn", "剧集", "电影", "zh-CN", "CN", "CN:zh-Hans" },
  { "zhtw", "影集", "電影", "zh-TW", "TW", "TW:zh-Hant" },
  { "ar", "مسلسل", "فيلم", "ar", "SA", "SA:ar" },
};
_Static_assert(sizeof PAR / sizeof *PAR == IDIOMA_N, "noticias.c: uma linha de PAR por IDIOMA_* (idiomacod.h)");

// Epoch UTC de "Sat, 20 Sep 2026 12:00:00 GMT". Conta civil (dias desde
// 1970), nao timegm: timegm nao existe em toda libc das TVs, e mktime usaria o
// fuso do aparelho.
static long long epochRfc(const char *rfc) {
  static const char *EN[] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
  int d = 0, a = 0, h = 0, mi = 0, se = 0, m = -1, i;
  char mes[8] = "";
  long long y, era, yoe, doy, doe, dias;
  if (sscanf(rfc, "%*[^,], %d %7s %d %d:%d:%d", &d, mes, &a, &h, &mi, &se) < 3) return 0;
  for (i = 0; i < 12; i++) if (!strncmp(mes, EN[i], 3)) m = i + 1;
  if (m < 1 || a < 1970 || d < 1) return 0;
  // days_from_civil (Howard Hinnant).
  y = a - (m <= 2);
  era = (y >= 0 ? y : y - 399) / 400;
  yoe = y - era * 400;
  doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  dias = era * 146097 + doe - 719468;
  return dias * 86400LL + h * 3600LL + mi * 60LL + se;
}

void noticias_quando(const Noticia *nt, long long agora, char *dst, int tam) {
  long long s;
  if (!dst || tam <= 0) return;
  dst[0] = 0;
  if (!nt) return;
  s = nt->quando > 0 ? agora - nt->quando : -1;
  // Relogio da TV atrasado (s < 0) ou pubDate ausente: a data curta, que nao
  // depende do relogio.
  if (s < 0 || s >= 7LL * 86400) { snprintf(dst, (size_t)tam, "%s", nt->data); return; }
  if (s < 3600) {
    int m = (int)(s / 60);
    if (m < 2) snprintf(dst, (size_t)tam, "%s", i18n("agora mesmo"));
    else snprintf(dst, (size_t)tam, i18n("há %d min"), m);
  } else if (s < 86400) snprintf(dst, (size_t)tam, i18n("há %d h"), (int)(s / 3600));
  else if (s < 2 * 86400) snprintf(dst, (size_t)tam, "%s", i18n("ontem"));
  else snprintf(dst, (size_t)tam, i18n("há %d dias"), (int)(s / 86400));
}

// "Sat, 20 Sep 2026 12:00:00 GMT" -> "20 Sep" / "20 set" / "20 вер".
static long dataCurta(const char *rfc, char *dst, size_t cap) {
  static const char *EN[] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
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
    idioma_data_curta(ajustes_idioma(), d, idioma_mes_curto(ajustes_idioma(), m),
                      ano && ano != anoAtual ? ano : 0, dst, cap); }
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
    if (campo(p, fim, "pubDate", buf, sizeof buf)) {
      nt->chave = dataCurta(buf, nt->data, sizeof nt->data);
      nt->quando = epochRfc(buf);
    }
    // O LINK, so http(s): e o que noticia.c vai buscar, e uma TV nao abre
    // javascript: nem file: de um XML de fora.
    if (campo(p, fim, "link", buf, sizeof buf)) {
      desentidar(buf);
      if (!strncmp(buf, "https://", 8) || !strncmp(buf, "http://", 7))
        snprintf(nt->link, sizeof nt->link, "%s", buf);
    }
    if (nt->titulo[0]) e->n++;
    p = fim + 7;
  }
  // DA MAIS NOVA PARA A MAIS VELHA. O RSS de busca vem por relevancia, e a
  // linha da Agenda mostra so a primeira: tem de ser a ultima noticia.
  { int i, j;
    for (i = 1; i < e->n; i++)
      for (j = i; j > 0 && (e->itens[j].chave > e->itens[j - 1].chave ||
                            (e->itens[j].chave == e->itens[j - 1].chave &&
                             e->itens[j].quando > e->itens[j - 1].quando)); j--) {
        Noticia t = e->itens[j]; e->itens[j] = e->itens[j - 1]; e->itens[j - 1] = t;
      } }
}

// --- disco -------------------------------------------------------------------

static void nomeDisco(const char *imdb, int lg, char *dst, size_t cap) {
  // "noticias2-": o formato ganhou link e instante (dois campos no FIM da
  // linha). Nome novo e nao o mesmo arquivo, porque a versao anterior le o
  // ultimo campo como manchete ate o fim da linha — um downgrade desenharia
  // o link colado ao titulo.
  snprintf(dst, cap, "noticias2-%s-%s.txt", imdb, PAR[lg >= 0 && lg < IDIOMA_N ? lg : 0].cod);
}
static void gravar(const Entrada *e) {
  char nome[80], *txt;
  size_t cap = 64 + (size_t)e->n * (sizeof(Noticia) + 32), k = 0;
  int i;
  txt = malloc(cap);
  if (!txt) return;
  k += (size_t)snprintf(txt + k, cap - k, "%ld\n", e->quando);
  for (i = 0; i < e->n && k < cap; i++)
    k += (size_t)snprintf(txt + k, cap - k, "%ld\t%s\t%s\t%s\t%lld\t%s\n", e->itens[i].chave,
                          e->itens[i].data, e->itens[i].fonte, e->itens[i].titulo,
                          e->itens[i].quando, e->itens[i].link);
  nomeDisco(e->imdb, e->lg, nome, sizeof nome);
  dados_gravar_leve(nome, txt);
  free(txt);
}
static int lerDisco(Entrada *e) {
  char nome[80], *txt, *l, *prox;
  long q;
  nomeDisco(e->imdb, e->lg, nome, sizeof nome);
  txt = dados_ler(nome);
  if (!txt) return 0;
  q = atol(txt);
  if (q <= 0 || time(NULL) - q > NOT_VALIDADE) { free(txt); return 0; }
  e->quando = q; e->n = 0;
  l = strchr(txt, '\n');
  while (l && *++l && e->n < NOT_MAX) {
    Noticia *nt = &e->itens[e->n];
    char *t0, *t1, *t2;
    memset(nt, 0, sizeof *nt);
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
    { char *t3 = strchr(t2, '\t'), *t4 = NULL;
      if (t3) { *t3++ = 0; t4 = strchr(t3, '\t'); if (t4) *t4++ = 0;
                nt->quando = atoll(t3);
                if (t4 && (!strncmp(t4, "https://", 8) || !strncmp(t4, "http://", 7)))
                  snprintf(nt->link, sizeof nt->link, "%s", t4); } }
    snprintf(nt->titulo, sizeof nt->titulo, "%s", t2);
    e->n++;
    l = prox;
  }
  free(txt);
  return 1;
}

// --- rede --------------------------------------------------------------------

typedef struct { char imdb[40]; char titulo[200]; char rede[64]; int serie, lg; } Pedido;

static void *buscar(void *arg) {
  Pedido *p = arg;
  char q[700], url[900], *xml;
  Entrada *e;
  int lg = p->lg;
  // Titulo entre aspas mais a palavra de apoio: "Silo" serie acha a serie e
  // nao o armazem.
  // A REDE entra entre aspas quando se sabe ("Foundation" "Apple TV+"): sem
  // ela a busca por "Foundation" trazia a Wikimedia Foundation.
  if (p->rede[0]) snprintf(q, sizeof q, "\"%s\" \"%s\"", p->titulo, p->rede);
  else snprintf(q, sizeof q, "\"%s\" %s", p->titulo,
                p->serie ? PAR[lg].serie : PAR[lg].filme);
  { char qc[900]; codificar(q, qc, sizeof qc);
#if defined(__EMSCRIPTEN__)
    // SAMSUNG: o Google News nao manda CORS e o fetch do wgt morre (12 de 12
    // "rede falhou" no registro de 21/09/2026). O servico de recomendacoes
    // repassa o mesmo RSS com CORS (rota /v1/noticias, sem sessao). Sem
    // NV_REC_URL na build nao ha por onde: fica sem noticias, sem erro.
    if (!NV_REC_URL[0]) { pthread_mutex_lock(&trava); e = acharLg(p->imdb, p->lg);
      if (e) { e->n = 0; e->quando = (long)time(NULL); e->respondeu = 1; e->emVoo = 0; }
      pthread_mutex_unlock(&trava); free(p); return NULL; }
    snprintf(url, sizeof url, "%s/v1/noticias?q=%s&hl=%s&gl=%s&ceid=%s", NV_REC_URL,
             qc, PAR[lg].hl, PAR[lg].gl, PAR[lg].ceid);
#else
    snprintf(url, sizeof url, "https://news.google.com/rss/search?q=%s&hl=%s&gl=%s&ceid=%s",
             qc, PAR[lg].hl, PAR[lg].gl, PAR[lg].ceid);
#endif
  }
  xml = rede_baixar(url, 12);
  pthread_mutex_lock(&trava);
  e = acharLg(p->imdb, p->lg);
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
  p->lg = e->lg;
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
