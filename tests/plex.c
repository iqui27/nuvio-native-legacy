// Plex: pure parsers on fixture strings, the PIN state machine, the id/GUID
// matching, and the whole integration against tests/plex_server.py (a fake
// plex.tv + PMS). No real server, no real account, never prints a token.
#include "plex.h"
#include "srvidx.h"
#include "idbase.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// ---- stubs for the app modules plex.c / jellyfin.c talk to
static char dirDados[512];
static int perfilAtivo = 1;
int perfis_ativo(void) { return perfilAtivo; }
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  snprintf(dst, tam, "%s/%s", dirDados, nome);
  return dst;
}
char *dados_ler(const char *nome) {
  char c[700];
  FILE *f;
  long n;
  char *b;
  dados_caminho(c, sizeof c, nome);
  f = fopen(c, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1);
  n = (long)fread(b, 1, (size_t)n, f);
  fclose(f);
  b[n] = 0;
  return b;
}
int dados_apagar(const char *nome) { char c[700]; dados_caminho(c, sizeof c, nome); return remove(c) == 0; }
int dados_gravar_leve(const char *nome, const char *conteudo) {
  char c[700];
  FILE *f;
  dados_caminho(c, sizeof c, nome);
  f = fopen(c, "wb");
  if (!f) return 0;
  fputs(conteudo, f);
  return fclose(f) == 0;
}
void dados_fs_travar(void) {}
void dados_fs_liberar(void) {}

#define MACHINE "0123456789abcdef0123456789abcdef01234567"
static char raiz[256];

static unsigned long long ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned long long)t.tv_sec * 1000ull + (unsigned long long)t.tv_nsec / 1000000ull;
}
static void dormir(unsigned m) { struct timespec d = { m / 1000, (long)(m % 1000) * 1000000L }; nanosleep(&d, NULL); }

static char *controle(const char *rota) {
  char url[400];
  RedePedido p;
  RedeResposta r;
  char *c;
  snprintf(url, sizeof url, "%s/control/%s", raiz, rota);
  memset(&p, 0, sizeof p);
  p.url = url;
  rede_pedir(&p, &r);
  assert(r.erro == REDE_OK && r.status == 200);
  c = strdup(r.corpo ? r.corpo : "");
  rede_resposta_limpar(&r);
  return c;
}
static int conta(const char *h, const char *agulha) {
  int n = 0;
  for (; (h = strstr(h, agulha)) != NULL; h += strlen(agulha)) n++;
  return n;
}
static PxEstado esperarEstado(PxEstado alvo, unsigned prazo) {
  unsigned long long fim = ms() + prazo;
  PxEstado e;
  char d[160];
  while ((e = plex_estado(d, sizeof d)) != alvo && ms() < fim) dormir(20);
  return e;
}

static PxConta contaFixture(void) {
  PxConta c;
  memset(&c, 0, sizeof c);
  snprintf(c.base, sizeof c.base, "http://plex.example:32400");
  snprintf(c.servidorId, sizeof c.servidorId, "%s", MACHINE);
  snprintf(c.token, sizeof c.token, "tok-fixture");
  snprintf(c.dispositivoId, sizeof c.dispositivoId, "nuvio-test-p1");
  return c;
}

// ------------------------------------------------------------- PIN machine
static void testarPin(void) {
  PxPin p;
  unsigned long long t = 1000000;
  pxpin_iniciar(&p);
  assert(p.est == PXP_OCIOSO && !pxpin_vencido(&p, t));
  assert(!pxpin_criado(&p, t, "", "ABCD", 900) && p.est == PXP_OCIOSO);   // no id: refused
  assert(!pxpin_criado(&p, t, "77", "", 900) && p.est == PXP_OCIOSO);     // no code: refused
  assert(pxpin_criado(&p, t, "123456", "WXYZ", 900));
  assert(p.est == PXP_ESPERANDO && !strcmp(p.codigo, "WXYZ"));
  // Not due until the interval has passed.
  assert(!pxpin_vencido(&p, t + PXPIN_INTERVALO_MS - 1) && pxpin_vencido(&p, t + PXPIN_INTERVALO_MS));
  t += PXPIN_INTERVALO_MS;
  pxpin_resultado(&p, t, 0, NULL);                      // pending
  assert(p.est == PXP_ESPERANDO && !pxpin_vencido(&p, t) && pxpin_vencido(&p, t + PXPIN_INTERVALO_MS));
  // Transport failures are tolerated up to the limit, and a good answer resets the count.
  t += PXPIN_INTERVALO_MS;
  pxpin_resultado(&p, t, PX_ERR_REDE, NULL);
  pxpin_resultado(&p, t, PX_ERR_HTTP, NULL);
  pxpin_resultado(&p, t, PX_ERR_REDE, NULL);
  assert(p.est == PXP_ESPERANDO && p.falhas == 3);
  pxpin_resultado(&p, t, 0, NULL);
  assert(p.falhas == 0);
  for (int i = 0; i < PXPIN_FALHAS_MAX; i++) pxpin_resultado(&p, t, PX_ERR_REDE, NULL);
  assert(p.est == PXP_ERRO);
  pxpin_resultado(&p, t, 1, "late-token");              // a finished pin is never revived
  assert(p.est == PXP_ERRO && !p.token[0]);

  // Linked: the token is kept, polling stops being due.
  pxpin_criado(&p, t, "9", "ABCD", 900);
  pxpin_resultado(&p, t + 2000, 1, "acct-token");
  assert(p.est == PXP_LIGADO && !strcmp(p.token, "acct-token") && !pxpin_vencido(&p, t + 99999));
  pxpin_cancelar(&p);                                    // cancel after link keeps the token
  assert(p.est == PXP_LIGADO && p.token[0]);
  // "linked" with an empty token is not a link.
  pxpin_criado(&p, t, "9", "ABCD", 900);
  pxpin_resultado(&p, t, 1, "");
  assert(p.est == PXP_ESPERANDO);

  // plex.tv forgot the pin (404).
  pxpin_resultado(&p, t, PX_ERR_EXPIRADO, NULL);
  assert(p.est == PXP_EXPIRADO);

  // The clock alone expires a waiting pin; the default lifetime is 15 minutes.
  pxpin_criado(&p, t, "9", "ABCD", 0);
  pxpin_relogio(&p, t + 899 * 1000);
  assert(p.est == PXP_ESPERANDO);
  pxpin_relogio(&p, t + 900 * 1000);
  assert(p.est == PXP_EXPIRADO);
  pxpin_criado(&p, t, "9", "ABCD", 60);
  pxpin_resultado(&p, t + 61 * 1000, 0, NULL);          // an answer after the deadline still expires it
  assert(p.est == PXP_EXPIRADO);

  // Cancel wipes any token and ends polling.
  pxpin_criado(&p, t, "9", "ABCD", 900);
  pxpin_cancelar(&p);
  assert(p.est == PXP_CANCELADO && !pxpin_vencido(&p, t + 99999));
  pxpin_resultado(&p, t, 1, "x");
  assert(p.est == PXP_CANCELADO && !p.token[0]);
  printf("pin state machine ok\n");
}

// ------------------------------------------------------------------ parsers
static const char PIN_NOVO[] =
  "{\"id\":123456,\"code\":\"WXYZ\",\"product\":\"Nuvio\",\"trusted\":false,\"expiresIn\":900,"
  "\"createdAt\":\"2026-10-04T10:00:00Z\",\"authToken\":null,\"newRegistration\":null}";
static const char PIN_LIGADO[] =
  "{\"id\":123456,\"code\":\"WXYZ\",\"expiresIn\":412,\"authToken\":\"plexacct-from-fixture\"}";

static const char RECURSOS[] =
  "[{\"name\":\"Phone\",\"product\":\"Plex for Android\",\"provides\":\"player\",\"clientIdentifier\":\"aaaa\"},"
  " {\"name\":\"Shared Box\",\"product\":\"Plex Media Server\",\"provides\":\"server\","
  "  \"clientIdentifier\":\"ABCDEF0123456789ABCDEF0123456789ABCDEF01\",\"productVersion\":\"1.40.1\","
  "  \"owned\":false,\"accessToken\":\"shared-srv-token\",\"connections\":["
  "   {\"protocol\":\"https\",\"address\":\"203.0.113.9\",\"port\":32400,"
  "    \"uri\":\"https://203-0-113-9.hash.plex.direct:32400\",\"local\":false,\"relay\":false,\"IPv6\":false}]},"
  " {\"name\":\"Home PMS\",\"product\":\"Plex Media Server\",\"provides\":\"server,player\","
  "  \"clientIdentifier\":\"" MACHINE "\",\"productVersion\":\"1.41.0\",\"owned\":true,"
  "  \"accessToken\":\"owned-srv-token\",\"connections\":["
  "   {\"protocol\":\"https\",\"address\":\"10.0.0.5\",\"port\":32400,\"uri\":\"https://10-0-0-5.hash.plex.direct:32400\","
  "    \"local\":true,\"relay\":false,\"IPv6\":false},"
  "   {\"protocol\":\"https\",\"address\":\"198.51.100.2\",\"port\":18321,\"uri\":\"https://198-51-100-2.hash.plex.direct:18321/\","
  "    \"local\":false,\"relay\":false,\"IPv6\":false},"
  "   {\"protocol\":\"https\",\"address\":\"relay.plex.direct\",\"port\":8443,\"uri\":\"https://relay.plex.direct:8443\","
  "    \"local\":false,\"relay\":true,\"IPv6\":false},"
  "   {\"protocol\":\"ftp\",\"address\":\"x\",\"port\":1,\"uri\":\"ftp://nope\",\"local\":true,\"relay\":false}]}]";

static const char SECOES[] =
  "{\"MediaContainer\":{\"size\":4,\"Directory\":[{\"key\":\"1\",\"type\":\"movie\",\"title\":\"Movies\"},"
  "{\"key\":\"2\",\"type\":\"show\",\"title\":\"TV \\\"Shows\\\"\"},{\"key\":\"3\",\"type\":\"artist\",\"title\":\"Music\"},"
  "{\"key\":5,\"type\":\"movie\",\"title\":\"Numeric key\"}]}}";

static const char ITENS[] =
  "{\"MediaContainer\":{\"size\":3,\"totalSize\":57,\"Metadata\":["
  "{\"ratingKey\":\"101\",\"guid\":\"plex://movie/5d77\",\"type\":\"movie\",\"title\":\"Movie One\",\"year\":1999,"
  " \"summary\":\"Overview one\",\"rating\":7.8,\"contentRating\":\"R\",\"duration\":7200000,\"viewOffset\":3600000,"
  " \"thumb\":\"/library/metadata/101/thumb/17\",\"art\":\"/library/metadata/101/art/17\","
  " \"Genre\":[{\"tag\":\"Drama\"},{\"tag\":\"Thriller\"}],"
  " \"Guid\":[{\"id\":\"imdb://tt0000101\"},{\"id\":\"tmdb://5101\"},{\"id\":\"tvdb://301\"}],"
  " \"Image\":[{\"alt\":\"x\",\"type\":\"coverPoster\",\"url\":\"/a\"},{\"alt\":\"x\",\"type\":\"clearLogo\",\"url\":\"/library/metadata/101/clearLogo/17\"}]},"
  "{\"ratingKey\":102,\"type\":\"movie\",\"title\":\"Movie Two\",\"year\":2005,\"duration\":5400000,\"Guid\":[{\"id\":\"tmdb://5102\"}]},"
  "{\"ratingKey\":\"201\",\"type\":\"show\",\"title\":\"Show One\",\"year\":2020,\"Guid\":[{\"id\":\"imdb://tt0000201\"}]},"
  "{\"ratingKey\":\"999\",\"type\":\"artist\",\"title\":\"Not video\"}]}}";

static const char DETALHE[] =
  "{\"MediaContainer\":{\"size\":1,\"Metadata\":[{\"ratingKey\":\"101\",\"type\":\"movie\",\"title\":\"Movie One\","
  "\"year\":1999,\"duration\":7200000,\"Role\":[{\"tag\":\"Actor One\",\"role\":\"Lead\",\"thumb\":\"https://image.example/a.jpg\"},"
  "{\"tag\":\"Actor Two\",\"role\":\"Support\",\"thumb\":\"/library/metadata/9/thumb/1\"}],"
  "\"Director\":[{\"tag\":\"Dir One\"},{\"tag\":\"Dir Two\"}]}]}}";

static const char EPISODIOS[] =
  "{\"MediaContainer\":{\"size\":3,\"Metadata\":["
  "{\"ratingKey\":\"301\",\"type\":\"episode\",\"parentIndex\":1,\"index\":1,\"title\":\"Pilot\",\"duration\":2700000,"
  " \"thumb\":\"/library/metadata/301/thumb/17\",\"originallyAvailableAt\":\"2020-01-02\",\"summary\":\"S1E1\"},"
  "{\"ratingKey\":\"311\",\"type\":\"episode\",\"parentIndex\":2,\"index\":1,\"title\":\"Return\",\"duration\":2700000},"
  "{\"ratingKey\":\"399\",\"type\":\"episode\",\"title\":\"No numbers\"}]}}";

static const char FONTES[] =
  "{\"MediaContainer\":{\"size\":1,\"Metadata\":[{\"ratingKey\":\"101\",\"type\":\"movie\",\"duration\":7200000,\"Media\":["
  "{\"id\":501,\"duration\":7200000,\"width\":1920,\"height\":1080,\"audioChannels\":6,\"audioCodec\":\"eac3\","
  " \"videoCodec\":\"hevc\",\"videoResolution\":\"1080\",\"container\":\"mkv\",\"Part\":[{\"id\":901,"
  "  \"key\":\"/library/parts/901/1700000000/file.mkv\",\"file\":\"/srv/private/Movie.mkv\",\"size\":12345678901,"
  "  \"container\":\"mkv\",\"Stream\":[{\"streamType\":1,\"codec\":\"hevc\",\"colorTrc\":\"smpte2084\",\"DOVIPresent\":false},"
  "  {\"streamType\":2,\"codec\":\"eac3\",\"channels\":6}]}]},"
  "{\"id\":502,\"height\":2160,\"audioChannels\":8,\"audioCodec\":\"truehd\",\"videoCodec\":\"hevc\",\"container\":\"mp4\","
  " \"Part\":[{\"id\":902,\"key\":\"/library/parts/902/1700000001/file.mp4\",\"size\":999,"
  "  \"Stream\":[{\"streamType\":1,\"codec\":\"hevc\",\"DOVIPresent\":true}]}]},"
  "{\"id\":503,\"container\":\"mkv\",\"Part\":[{\"id\":903,\"key\":\"http://evil.example/steal\",\"size\":1}]},"
  "{\"id\":504,\"container\":\"mkv\",\"Part\":[{\"id\":904,\"key\":\"/library/parts/904/1/a b.mkv\"}]}]}]}}";

static void testarParsers(void) {
  PxConta c = contaFixture();
  char id[64], cod[8], tok[96];
  int exp, i;
  PxServidor srv[4];
  PxBiblioteca bib[8];
  CatItem *it = malloc(sizeof(CatItem) * 8), *det = malloc(sizeof *det);
  CatEp *eps = malloc(sizeof(CatEp) * 8);
  SrvIdx idx;
  int total = 0;
  PxPlayback *pb = malloc(sizeof *pb);

  // --- PIN answers
  assert(px_ler_pin(PIN_NOVO, strlen(PIN_NOVO), id, sizeof id, cod, sizeof cod, &exp, tok, sizeof tok) == 0);
  assert(!strcmp(id, "123456") && !strcmp(cod, "WXYZ") && exp == 900 && !tok[0]);
  assert(px_ler_pin(PIN_LIGADO, strlen(PIN_LIGADO), id, sizeof id, cod, sizeof cod, &exp, tok, sizeof tok) == 1);
  assert(!strcmp(tok, "plexacct-from-fixture") && exp == 412);
  assert(px_ler_pin("{\"errors\":[]}", 13, id, sizeof id, cod, sizeof cod, &exp, tok, sizeof tok) == PX_ERR_FORMATO);
  { const char *largo = "{\"id\":1,\"code\":\"ABCD\",\"authToken\":\"toolongtoken\"}";
    assert(px_ler_pin(largo, strlen(largo), id, sizeof id, cod, sizeof cod, &exp, tok, 4) == PX_ERR_FORMATO); }   // a token that does not fit is refused, not truncated

  // --- resources: only servers; connection order local http raw, local uri, remote, relay
  i = px_ler_recursos(RECURSOS, strlen(RECURSOS), srv, 4);
  assert(i == 2);
  assert(!strcmp(srv[0].nome, "Shared Box") && !srv[0].dono && srv[0].nCon == 1);
  assert(!strcmp(srv[1].nome, "Home PMS") && srv[1].dono && !strcmp(srv[1].id, MACHINE));
  assert(srv[1].nCon == 3);                       // the ftp:// connection is dropped
  assert(!strcmp(srv[1].token, "owned-srv-token"));
  {
    char cand[PX_CON_MAX * 2][320];
    int n = px_candidatas(&srv[1], cand, PX_CON_MAX * 2);
    assert(n == 4);
    assert(!strcmp(cand[0], "http://10.0.0.5:32400"));
    assert(!strcmp(cand[1], "https://10-0-0-5.hash.plex.direct:32400"));
    assert(!strcmp(cand[2], "https://198-51-100-2.hash.plex.direct:18321"));     // trailing '/' stripped
    assert(!strcmp(cand[3], "https://relay.plex.direct:8443"));                   // relay last
    n = px_candidatas(&srv[0], cand, PX_CON_MAX * 2);
    assert(n == 1 && !strncmp(cand[0], "https://203-0-113-9", 19));
  }
  assert(px_ler_recursos("{}", 2, srv, 4) == PX_ERR_FORMATO);

  // --- identity / user
  {
    const char *idj = "{\"MediaContainer\":{\"size\":0,\"machineIdentifier\":\"" MACHINE "\",\"version\":\"1.41.0.8992\"}}";
    char ver[24];
    assert(px_ler_identidade(idj, strlen(idj), id, sizeof id, ver, sizeof ver) == PX_OK);
    assert(!strcmp(id, MACHINE) && !strcmp(ver, "1.41.0.8992"));
    assert(px_ler_identidade("{\"MediaContainer\":{}}", 21, id, sizeof id, ver, sizeof ver) == PX_ERR_FORMATO);
    { const char *u = "{\"id\":7,\"username\":\"someone\",\"title\":\"Some One\",\"email\":\"private@example.invalid\"}";
      assert(px_ler_usuario(u, strlen(u), id, sizeof id) == PX_OK && !strcmp(id, "Some One"));
      u = "{\"username\":\"handle\",\"email\":\"private@example.invalid\"}";
      assert(px_ler_usuario(u, strlen(u), id, sizeof id) == PX_OK && !strcmp(id, "handle")); }
  }

  // --- libraries: movie/show only, escapes decoded, numeric keys accepted
  i = px_ler_bibliotecas(SECOES, strlen(SECOES), bib, 8);
  assert(i == 3);
  assert(!strcmp(bib[0].id, "1") && !strcmp(bib[0].tipo, "movie") && !strcmp(bib[0].nome, "Movies"));
  assert(!strcmp(bib[1].tipo, "series") && !strcmp(bib[1].nome, "TV \"Shows\""));
  assert(!strcmp(bib[2].id, "5"));

  // --- items
  i = px_ler_itens(&c, ITENS, strlen(ITENS), it, 8, &total);
  assert(i == 3 && total == 57);                   // the artist is skipped
  assert(!strcmp(it[0].imdb, "tt0000101") && !strcmp(it[0].tipo, "movie"));      // a Guid: the catalogue title
  { CatItem *op = malloc(sizeof *op);                                            // no Guid: the opaque server id
    static const char SEM[] = "{\"MediaContainer\":{\"Metadata\":[{\"ratingKey\":\"103\",\"type\":\"movie\",\"title\":\"X\"}]}}";
    assert(px_ler_itens(&c, SEM, strlen(SEM), op, 1, NULL) == 1 && !strcmp(op->imdb, "px:01234567.103"));
    assert(jfid_e(op->imdb) && idbase_len(op->imdb) == strlen(op->imdb));
    free(op); }
  assert(!strcmp(it[0].titulo, "Movie One") && !strcmp(it[0].meta, "1999 · 120 min"));
  assert(it[0].nota == 78 && !strcmp(it[0].classificacao, "R") && !strcmp(it[0].genero, "Drama · Thriller"));
  assert(it[0].progresso == 50 && it[0].restanteMin == 60);
  assert(strstr(it[0].poster, "/photo/:/transcode?width=300&height=450") &&
         strstr(it[0].poster, "url=%2Flibrary%2Fmetadata%2F101%2Fthumb%2F17"));
  assert(strstr(it[0].backdrop, "width=1280") && strstr(it[0].logo, "clearLogo"));
  assert(!strcmp(it[0].origem, "plex"));
  // Home row items with a Guid ARE the catalogue title (imdb first, else tmdb).
  assert(!strcmp(it[0].imdb, "tt0000101") && !strcmp(it[1].imdb, "tmdb:5102") && it[1].progresso == 0);
  assert(!strcmp(it[2].tipo, "series") && !strcmp(it[2].meta, "2020"));

  // --- the matching index: imdb + tmdb + tvdb, the show is typed 's', the artist skipped
  srvidx_iniciar(&idx);
  i = px_ler_indice(ITENS, strlen(ITENS), NULL, &idx, &total);
  assert(i == 4 && idx.n == 3 && total == 57);
  assert(!strcmp(idx.v[0].imdb, "tt0000101") && idx.v[0].tmdb == 5101 && idx.v[0].tvdb == 301 &&
         idx.v[0].tipo == 'm' && !strcmp(idx.v[0].item, "101") && idx.v[0].ano == 1999);
  assert(!idx.v[1].imdb[0] && idx.v[1].tmdb == 5102 && !strcmp(idx.v[1].item, "102"));
  assert(idx.v[2].tipo == 's' && !strcmp(idx.v[2].imdb, "tt0000201") && idx.v[2].tmdb == 0);
  assert(idx.v[0].tituloHash == srvidx_titulo_hash("movie one"));
  { int k = px_ler_indice(EPISODIOS, strlen(EPISODIOS), "201", &idx, NULL);
    assert(k == 3 && srvidx_n_eps(&idx, "201") == 2 && srvidx_achar_ep(&idx, "201", 2, 1) >= 0); }   // "No numbers" skipped

  // --- title ids and GUID matching
  { char imdb[16]; long tmdb; int t, e;
    assert(px_id_titulo("tt0000101", imdb, &tmdb, &t, &e) && !strcmp(imdb, "tt0000101") && !tmdb && !t && !e);
    assert(px_id_titulo("tt0000201:2:5", imdb, &tmdb, &t, &e) && t == 2 && e == 5 && !strcmp(imdb, "tt0000201"));
    assert(px_id_titulo("tmdb:5102", imdb, &tmdb, &t, &e) && tmdb == 5102 && !imdb[0]);
    assert(px_id_titulo("tmdb:1399:1:3", imdb, &tmdb, &t, &e) && tmdb == 1399 && t == 1 && e == 3);
    assert(!px_id_titulo("kitsu:123", imdb, &tmdb, &t, &e));
    assert(!px_id_titulo("px:01234567.101", imdb, &tmdb, &t, &e));          // our own ids are not external ids
    assert(!px_id_titulo("jf:0f1e2d3c.abc", imdb, &tmdb, &t, &e));
    assert(!px_id_titulo("tt12345678901234", imdb, &tmdb, &t, &e));
    assert(!px_id_titulo("ttabc", imdb, &tmdb, &t, &e) && !px_id_titulo("tt1:x", imdb, &tmdb, &t, &e));
    assert(!px_id_titulo("", imdb, &tmdb, &t, &e) && !px_id_titulo(NULL, imdb, &tmdb, &t, &e));
    assert(srvidx_achar(&idx, "tt0000101", 0, 0, 'm') == 0);
    assert(srvidx_achar(&idx, "tt0000101", 0, 0, 's') == -1);               // a movie id never matches a show
    assert(srvidx_achar(&idx, "", 5102, 0, 'm') == 1);                      // tmdb only
    assert(srvidx_achar(&idx, "tt0000999", 5101, 0, 'm') == 0);             // unknown imdb, known tmdb
    assert(srvidx_achar(&idx, "tt0000201", 0, 0, 's') == 2);
    assert(srvidx_achar(&idx, "tt0000201", 7201, 0, 'm') == -1);
    assert(srvidx_achar(&idx, "", 0, 0, 'm') == -1);
  }
  srvidx_limpar(&idx);

  // --- detail: cast, directors; a server-path photo is dropped (it would need the token)
  assert(px_ler_detalhe(&c, DETALHE, strlen(DETALHE), det) == PX_OK);
  assert(det->nElenco == 2 && !strcmp(det->elenco[0].nome, "Actor One") && !strcmp(det->elenco[0].papel, "Lead"));
  assert(!strncmp(det->elenco[0].foto, "https://", 8) && !det->elenco[1].foto[0]);
  assert(!strcmp(det->direcao, "Dir One, Dir Two"));
  assert(px_ler_detalhe(&c, "{\"MediaContainer\":{}}", 21, det) == PX_ERR_FORMATO);

  // --- episodes
  i = px_ler_episodios(&c, EPISODIOS, strlen(EPISODIOS), eps, 8);
  assert(i == 2);
  assert(eps[0].temporada == 1 && eps[0].episodio == 1 && !strcmp(eps[0].nome, "Pilot") && !strcmp(eps[0].duracao, "45 min"));
  assert(!strcmp(eps[0].data, "02/01/2020") && !strcmp(eps[0].vid, "px:01234567.301") && strstr(eps[0].thumb, "width=640"));
  assert(eps[1].temporada == 2 && !strcmp(eps[1].vid, "px:01234567.311"));
  { char rk[24];
    assert(px_ler_episodio_rk(EPISODIOS, strlen(EPISODIOS), 2, 1, rk, sizeof rk) == 1 && !strcmp(rk, "311"));
    assert(px_ler_episodio_rk(EPISODIOS, strlen(EPISODIOS), 3, 1, rk, sizeof rk) == 0);
    assert(px_ler_episodio_rk("{}", 2, 1, 1, rk, sizeof rk) == PX_ERR_FORMATO); }

  // --- playback sources: 2 usable versions (+1 HLS for the first), bad part keys refused.
  // Version 2 has TrueHD: this TV (webOS) does not open it, so it is NEVER
  // offered as direct play — only as direct stream (video copied, audio converted).
  assert(px_ler_fontes(&c, "101", FONTES, strlen(FONTES), NULL, pb) == PX_OK);
  assert(pb->n == 3);
  assert(pb->sessao[0].metodo == PX_METODO_DIRETO && pb->sessao[1].metodo == PX_METODO_STREAM &&
         pb->sessao[2].metodo == PX_METODO_STREAM);
  assert(!strcmp(pb->sessao[0].url, "http://plex.example:32400/library/parts/901/1700000000/file.mkv?X-Plex-Token=tok-fixture"));
  assert(strstr(pb->sessao[1].url, "/video/:/transcode/universal/start.m3u8?path=%2Flibrary%2Fmetadata%2F101&mediaIndex=0"));
  assert(strstr(pb->sessao[1].url, "protocol=hls") && strstr(pb->sessao[1].url, pb->sessao[1].sessaoId));
  assert(pb->sessao[2].midia == 1 && strstr(pb->sessao[2].url, "start.m3u8") && strstr(pb->sessao[2].url, "mediaIndex=1"));
  assert(pb->sessao[0].duracaoMs == 7200000 && !strcmp(pb->sessao[0].ratingKey, "101"));
  assert(!strcmp(pb->fonte[0].provedor, "Plex") && pb->fonte[0].altura == 1080 && !pb->fonte[0].mp4);
  assert(!strcmp(pb->fonte[0].rotulo, "1080p · Direct play") && !strcmp(pb->fonte[1].rotulo, "1080p · Direct stream (HLS)"));
  assert(!strcmp(pb->fonte[1].bingeGroup, "plex|DirectStream") && !strcmp(pb->fonte[0].bingeGroup, "plex|DirectPlay"));
  assert(strstr(pb->fonte[0].descricao, "HEVC · HDR10 · EAC3 5.1 · MKV"));
  assert(pb->fonte[2].altura == 2160 && pb->fonte[2].dolbyVision && !pb->fonte[2].mp4 && strstr(pb->fonte[2].descricao, "Dolby Vision"));
  // The server refused version 1 (decision): no direct play for it either.
  { signed char d[PX_FONTES_MAX] = { PX_DEC_NAO, PX_DEC_DESCONHECIDO };
    assert(px_ler_fontes(&c, "101", FONTES, strlen(FONTES), d, pb) == PX_OK);
    assert(pb->n == 2 && pb->sessao[0].metodo == PX_METODO_STREAM && pb->sessao[1].metodo == PX_METODO_STREAM);
    d[0] = PX_DEC_DESCONHECIDO;   // unknown keeps the TV's own verdict
    assert(px_ler_fontes(&c, "101", FONTES, strlen(FONTES), d, pb) == PX_OK && pb->sessao[0].metodo == PX_METODO_DIRETO); }
  // On the desktop build HEVC is not declared: nothing is direct, and the HLS
  // entries are a full transcode.
  jellyfin_backend_definir(JF_BACKEND_HOST);
  assert(px_ler_fontes(&c, "101", FONTES, strlen(FONTES), NULL, pb) == PX_OK);
  assert(pb->n == 2 && pb->sessao[0].metodo == PX_METODO_TRANSCODE && !strcmp(pb->fonte[0].bingeGroup, "plex|Transcode"));
  jellyfin_backend_definir(JF_BACKEND_WEBOS);
  assert(px_ler_fontes(&c, "101", FONTES, strlen(FONTES), NULL, pb) == PX_OK);
  // Media info: the SELECTED audio/subtitle decide, and the decision parser.
  { PxMidiaInfo m[4];
    static const char SEL[] = "{\"MediaContainer\":{\"Metadata\":[{\"Media\":[{\"container\":\"mkv\",\"videoCodec\":\"h264\","
      "\"audioCodec\":\"ac3\",\"Part\":[{\"Stream\":[{\"streamType\":2,\"codec\":\"truehd\",\"selected\":true},"
      "{\"streamType\":3,\"codec\":\"pgs\",\"selected\":true}]}]},"
      "{\"container\":\"mpegts\",\"videoCodec\":\"h264\",\"audioCodec\":\"aac\",\"Part\":[{\"Stream\":["
      "{\"streamType\":3,\"codec\":\"srt\",\"selected\":true}]}]}]}]}}";
    assert(px_ler_midias(SEL, strlen(SEL), m, 4) == 2);
    assert(!strcmp(m[0].acod, "truehd") && !strcmp(m[0].leg, "pgs") && !px_direto_suportado(JF_BACKEND_WEBOS, &m[0]));
    snprintf(m[0].acod, sizeof m[0].acod, "ac3");
    assert(!px_direto_suportado(JF_BACKEND_WEBOS, &m[0]));            // PGS alone still refuses
    m[0].leg[0] = 0;
    assert(px_direto_suportado(JF_BACKEND_WEBOS, &m[0]));
    assert(px_direto_suportado(JF_BACKEND_TPK, &m[1]));               // mpegts is "ts", srt is fine
    snprintf(m[1].acod, sizeof m[1].acod, "dca");
    assert(!px_direto_suportado(JF_BACKEND_ANDROID, &m[1]) && !px_direto_suportado(JF_BACKEND_WGT, &m[1]));
    { char pe[600];
      assert(px_perfil_extra(JF_BACKEND_TPK, pe, sizeof pe) && strstr(pe, "add-direct-play-profile") &&
             strstr(pe, "mpegts") && strstr(pe, "hevc") && !strstr(pe, "dca") && !strstr(pe, "truehd"));
      assert(!px_perfil_extra(JF_BACKEND_WGT, pe, sizeof pe)); }
    assert(px_ler_decisao("{\"MediaContainer\":{\"directPlayDecisionCode\":1000}}", 50) == PX_DEC_DIRETO);
    assert(px_ler_decisao("{\"MediaContainer\":{\"directPlayDecisionCode\":3000}}", 50) == PX_DEC_NAO);
    { static const char PD[] = "{\"MediaContainer\":{\"Metadata\":[{\"Media\":[{\"Part\":[{\"decision\":\"directplay\"}]}]}]}}";
      assert(px_ler_decisao(PD, strlen(PD)) == PX_DEC_DIRETO); }
    assert(px_ler_decisao("{\"MediaContainer\":{}}", 21) == PX_DEC_DESCONHECIDO);
    assert(px_ler_decisao("<xml/>", 6) == PX_DEC_DESCONHECIDO); }
  assert(pb->fonte[0].tamanhoBytes == 12345678901ull && !pb->fonte[1].tamanhoBytes);
  for (i = 0; i < pb->n; i++)
    assert(!strstr(pb->fonte[i].descricao, "/srv/private") && !strstr(pb->fonte[i].url, "evil.example") &&
           !strstr(pb->fonte[i].url, "a b"));
  assert(px_ler_fontes(&c, "101", "{\"MediaContainer\":{\"Metadata\":[{}]}}", 37, NULL, pb) == PX_ERR_FORMATO);
  assert(px_ms(0) == 0 && px_ms(-3) == 0 && px_ms(1.5) == 1500 && px_ms(1e300) > 0);

  // --- id namespaces: px: is a personal-server id for every privacy gate
  assert(jfid_e("px:01234567.101") && jfid_e("em:01234567.101") && jfid_e("jf:01234567.101"));
  assert(!jfid_e("tt0000101") && !jfid_e("p") && !jfid_e("px") && !jfid_e(NULL) && !jfid_e("tmdb:1"));
  { char tag[JFID_TAG + 1], item[JFID_ITEM + 1], out[JFID_MAX];
    assert(jfid_montar_p(JFID_PREFIXO_PLEX, out, sizeof out, MACHINE, "101") && !strcmp(out, "px:01234567.101"));
    assert(jfid_partes_p(JFID_PREFIXO_PLEX, out, tag, item) && !strcmp(tag, "01234567") && !strcmp(item, "101"));
    assert(!jfid_partes_p(JFID_PREFIXO_PLEX, "jf:01234567.101", tag, item));   // namespaces never cross
    assert(!jfid_partes("px:01234567.101", tag, item));
    assert(!jfid_partes_p(JFID_PREFIXO_EMBY, "px:01234567.101", tag, item)); }
  free(it); free(det); free(eps); free(pb);
  printf("plex parsers ok\n");
}

// ------------------------------------------------------------- integration
static int nuncaCancela(void *u) { (void)u; return 0; }
static int sempreCancela(void *u) { (void)u; return 1; }

static int plex_consultar_t(const char *id, const char *tipo, int (*cc)(void *), void *ctx, Stream **l) {
  return plex_consultar(id, tipo, NULL, 0, cc, ctx, l);
}

static void integracao(void) {
  char det[160], *ev;
  PxEstado e;
  CatItem *itens = malloc(sizeof(CatItem) * PX_FIL_MAX * PX_POR_FILEIRA);
  CatFileira fils[PX_FIL_MAX];
  int nItens = 0, nf;
  unsigned long long fim;
  Stream *lista = NULL;
  int n = 0, r;
  char alvo[JFID_MAX];
  struct stat st;
  char arq[640];

  plex_carregar();
  assert(plex_estado(det, sizeof det) == PX_EST_SEM_CONTA && !plex_conectado());
  assert(!plex_fontes_pedir("px:01234567.101") && plex_fontes_colher("px:01234567.101", &lista, &n) == PX_FONTES_FALHOU);

  // ---- PIN flow: the code shows up, then the link completes the sign-in
  assert(plex_entrar());
  assert(esperarEstado(PX_EST_CODIGO, 3000) == PX_EST_CODIGO);
  fim = ms() + 3000;
  det[0] = 0;
  while (ms() < fim && strcmp(det, "WXYZ")) { plex_estado(det, sizeof det); dormir(20); }
  assert(!strcmp(det, "WXYZ"));
  assert(!plex_entrar());                          // already in flow: refused, no second pin
  e = esperarEstado(PX_EST_CONECTADO, 15000);
  assert(e == PX_EST_CONECTADO);
  assert(plex_conectado() && !strcmp(plex_servidor_nome(), "Home PMS") && !strcmp(plex_usuario(), "Some One"));
  assert(plex_n_servidores() == 1);
  { char *c = controle("counts");
    assert(strstr(c, "\"pins\": 1") && strstr(c, "\"polls\": 2"));     // exactly 2 polls: linked on the second
    free(c); }

  // The token file: 0600, holds both tokens, and exists.
  snprintf(arq, sizeof arq, "%s/plex-p1.txt", dirDados);
  assert(stat(arq, &st) == 0 && (st.st_mode & 077) == 0);
  { char *t = dados_ler("plex-p1.txt");
    assert(t && strstr(t, "tokc=plexacct-token-0001") && strstr(t, "toks=plexsrv-token-0001"));
    free(t); }

  // ---- Home rows
  fim = ms() + 8000;
  nf = 0;
  while (ms() < fim) {
    nf = plex_fileiras_copiar(itens, PX_FIL_MAX * PX_POR_FILEIRA, fils, PX_FIL_MAX, &nItens);
    if (nf == 2 && plex_indice_n() == 4) break;
    dormir(30);
  }
  assert(nf == 2 && nItens == 4);                  // movies + shows; the music library is skipped
  assert(!strcmp(fils[0].titulo, "Movies · Plex") && !strcmp(fils[0].chave, "plex_1") && fils[0].n == 3);
  assert(!strcmp(fils[1].titulo, "TV Shows · Plex") && fils[1].ini == 3 && !strcmp(fils[1].tipo, "series"));
  assert(plex_chave_fileira(fils[0].chave) && !plex_chave_fileira("cinemeta_movie_top"));
  assert(!strcmp(itens[0].imdb, "tt0000101") && itens[0].progresso == 50 && !strcmp(itens[2].imdb, "px:01234567.103"));
  assert(plex_indice_n() == 4 && plex_casamento_ativo());   // 3 movies (one without Guid: title+year) + 1 show

  // ---- title page of a server-native item
  { CatItem *x = calloc(1, sizeof *x);
    CatEp *eps = calloc(32, sizeof *eps);
    snprintf(x->imdb, sizeof x->imdb, "px:01234567.101");
    snprintf(x->tipo, sizeof x->tipo, "movie");
    assert(plex_ficha(x, eps, 32) == 0);
    assert(x->nElenco == 2 && !strcmp(x->direcao, "Dir One") && !strcmp(x->imdb, "px:01234567.101"));
    memset(x, 0, sizeof *x);
    snprintf(x->imdb, sizeof x->imdb, "px:01234567.201");
    snprintf(x->tipo, sizeof x->tipo, "series");
    assert(plex_ficha(x, eps, 32) == 3);
    assert(x->nTemporadas == 2 && x->temporadas[0] == 1 && x->temporadas[1] == 2);
    snprintf(x->imdb, sizeof x->imdb, "px:deadbeef.201");                      // another server: refused
    assert(plex_ficha(x, eps, 32) == -1);
    free(x); free(eps); }

  // ---- sources of a server-native item
  snprintf(alvo, sizeof alvo, "px:01234567.101");
  assert(plex_fontes_pedir(alvo));
  fim = ms() + 5000;
  while ((r = plex_fontes_colher(alvo, &lista, &n)) == PX_FONTES_PENDENTE && ms() < fim) dormir(20);
  // v1 HEVC/EAC3 direct + its HLS, v2 H.264 direct, v3 DTS only as direct stream.
  assert(r == PX_FONTES_PRONTO && n == 4 && lista);
  assert(!strcmp(lista[0].provedor, "Plex") && strstr(lista[0].url, "/library/parts/901/"));
  assert(strstr(lista[2].url, "/library/parts/902/") && strstr(lista[3].url, "start.m3u8") &&
         strstr(lista[3].url, "mediaIndex=2") && !strstr(lista[3].url, "/library/parts/903/"));
  { char *c = controle("counts"); assert(strstr(c, "\"decisions\": 2")); free(c); }   // none for the DTS version
  assert(strstr(lista[0].url, "http://127.0.0.1:"));                           // the LAN raw-http connection won the probe
  free(lista); lista = NULL;
  assert(!plex_fontes_pedir("px:deadbeef.101"));                               // wrong server: refused, never fetched
  assert(plex_fontes_colher("px:deadbeef.101", &lista, &n) == PX_FONTES_FALHOU && !lista);
  { char *c = controle("counts"); assert(strstr(c, "\"probe\": 1")); free(c); }    // one probe: the first candidate answered

  // ---- matched sources on a REGULAR catalogue title
  assert(plex_consultar_t("tt0000101", "movie", nuncaCancela, NULL, &lista) == 4);
  assert(lista && strstr(lista[0].url, "/library/parts/901/") && !strcmp(lista[0].provedor, "Home PMS"));
  { char url0[2048]; snprintf(url0, sizeof url0, "%s", lista[0].url); free(lista); lista = NULL;
    // check-ins for that matched source: start, no progress before 10 s, pause, stop
    plex_reproducao_tick(url0, 0, 7200, 0);                                    // not playing yet: nothing
    plex_reproducao_tick("https://addon.example/x.mp4", 1, 7200, 1);           // unknown URL ignored
    plex_reproducao_tick(url0, 1, 7200, 1);                                    // started
    plex_reproducao_tick(url0, 2, 7200, 1);                                    // < 10 s: no progress
    plex_reproducao_tick(url0, 3, 7200, 0);                                    // pause, immediate
    plex_reproducao_fim(url0, 5, 7200);                                        // stopped
    plex_reproducao_tick(url0, 6, 7200, 1);                                    // session gone
    fim = ms() + 4000;
    while (plex_relatorios_pendentes() && ms() < fim) dormir(20);
    assert(!plex_relatorios_pendentes());
    ev = controle("events");
    assert(conta(ev, "\"state\": \"playing\"") == 1 && conta(ev, "\"state\": \"paused\"") == 1 &&
           conta(ev, "\"state\": \"stopped\"") == 1);
    assert(strstr(ev, "\"time\": \"1000\"") && strstr(ev, "\"time\": \"5000\"") && strstr(ev, "\"dur\": \"7200000\""));
    assert(!strstr(ev, "transcode-stop"));
    free(ev); }
  assert(plex_consultar_t("tmdb:5102", "movie", nuncaCancela, NULL, &lista) == 4 && lista);   // tmdb-only match
  free(lista); lista = NULL;
  assert(plex_consultar_t("tt0000201:1:2", "series", nuncaCancela, NULL, &lista) == 4 && lista);   // episode 302 resolved
  free(lista); lista = NULL;
  assert(plex_consultar_t("tt0000201:2:1", "tv", nuncaCancela, NULL, &lista) == 4);
  free(lista); lista = NULL;
  assert(plex_consultar_t("tt0000201:3:1", "series", nuncaCancela, NULL, &lista) == 0 && !lista);  // no such episode
  assert(plex_consultar_t("tt0000201", "movie", nuncaCancela, NULL, &lista) == 0);                  // a show is not a movie
  assert(plex_consultar_t("tt9999999", "movie", nuncaCancela, NULL, &lista) == 0);                  // not on the server
  assert(plex_consultar_t("kitsu:12", "series", nuncaCancela, NULL, &lista) == 0);                  // not an external id
  assert(plex_consultar_t("tt0000101", "movie", sempreCancela, NULL, &lista) == -1 && !lista);
  // Last rule: no Guid at all, title + year from the page that asked (±1 year).
  assert(plex_consultar_t("tt4444444", "movie", nuncaCancela, NULL, &lista) == 0);
  assert(plex_consultar("tt4444444", "movie", "No External Id", 2011, nuncaCancela, NULL, &lista) == 4 && lista);
  free(lista); lista = NULL;
  assert(plex_consultar("tt4444444", "movie", "No External Id", 2013, nuncaCancela, NULL, &lista) == 0);
  { SrvAchado a;
    assert(plex_tem("tt0000101", "movie", &a) && !strcmp(a.servidor, "Home PMS") && !strcmp(a.tipoSrv, "Plex") &&
           a.altura == 1080 && a.local);
    assert(plex_tem("tt0000201:1:2", "series", NULL) && !plex_tem("tt0000201", "movie", NULL)); }
  { char arqIdx[700]; struct stat sx;
    snprintf(arqIdx, sizeof arqIdx, "%s/srvidx-p1-px.txt", dirDados);
    assert(stat(arqIdx, &sx) == 0); }

  // ---- the server refuses direct play of version 1: only its HLS entry stays
  free(controle("refuse?media=0"));
  assert(plex_consultar_t("tt0000101", "movie", nuncaCancela, NULL, &lista) == 3);
  assert(strstr(lista[0].url, "start.m3u8") && strstr(lista[0].url, "mediaIndex=0") &&
         strstr(lista[1].url, "/library/parts/902/"));
  free(lista); lista = NULL;
  free(controle("refuse?media="));
  // ---- transcode check-in ends with an explicit stop for the encoder
  assert(plex_consultar_t("tt0000101", "movie", nuncaCancela, NULL, &lista) == 4);
  { char urlT[2048]; snprintf(urlT, sizeof urlT, "%s", lista[1].url); free(lista); lista = NULL;
    assert(strstr(urlT, "start.m3u8"));
    plex_reproducao_tick(urlT, 1, 7200, 1);
    plex_reproducao_fim(urlT, 9, 7200);
    fim = ms() + 4000;
    while (plex_relatorios_pendentes() && ms() < fim) dormir(20);
    ev = controle("events");
    assert(strstr(ev, "transcode-stop"));
    free(ev); }

  // ---- profile switch: nothing of profile 1 survives in profile 2, and it comes back
  perfilAtivo = 2;
  plex_perfil_trocou();
  assert(plex_estado(det, sizeof det) == PX_EST_SEM_CONTA && !plex_conectado());
  assert(plex_fileiras_copiar(itens, PX_FIL_MAX * PX_POR_FILEIRA, fils, PX_FIL_MAX, &nItens) == 0);
  assert(plex_indice_n() == 0 && !plex_casamento_ativo());
  assert(plex_consultar_t("tt0000101", "movie", nuncaCancela, NULL, &lista) == 0);
  perfilAtivo = 1;
  plex_perfil_trocou();
  assert(plex_conectado());                                                     // loaded from plex-p1.txt, no new PIN
  assert(plex_indice_n() >= 4 && plex_tem("tt0000101", "movie", NULL));        // the index came back from disk, at once
  fim = ms() + 8000;
  { int incr = 0;
    while (ms() < fim && !incr) {
      char *c = controle("counts"), *q = strstr(c, "\"incr\": ");
      incr = q && atoi(q + 8) > 0;
      free(c);
      if (!incr) dormir(30);
    }
    assert(incr); }                                                              // the reload asked only what changed
  assert(plex_indice_n() >= 4);

  // ---- sign out: file gone, state clean
  plex_esquecer();
  assert(plex_estado(det, sizeof det) == PX_EST_SEM_CONTA && !plex_conectado());
  assert(stat(arq, &st) != 0);
  assert(plex_fileiras_copiar(itens, PX_FIL_MAX * PX_POR_FILEIRA, fils, PX_FIL_MAX, &nItens) == 0);

  // ---- cancel: polling stops
  assert(plex_entrar());
  assert(esperarEstado(PX_EST_CODIGO, 3000) == PX_EST_CODIGO);
  plex_cancelar_entrada();
  assert(plex_estado(det, sizeof det) == PX_EST_SEM_CONTA);
  dormir(2500);                                                                 // let the worker notice
  { char *c = controle("counts");
    const char *p = strstr(c, "\"polls\": ");
    int antes = p ? atoi(p + 9) : -1;
    free(c);
    dormir(PXPIN_INTERVALO_MS * 2 + 500);
    c = controle("counts");
    p = strstr(c, "\"polls\": ");
    if (!(p && atoi(p + 9) == antes)) fprintf(stderr, "antes=%d depois=%s\n", antes, p ? p : "?");
    assert(p && atoi(p + 9) == antes);                                          // no poll after cancel
    free(c); }

  // ---- account logout wipes every profile file
  perfilAtivo = 3;
  plex_perfil_trocou();
  { char nome[700];
    FILE *f;
    snprintf(nome, sizeof nome, "%s/plex-p3.txt", dirDados);
    f = fopen(nome, "w"); assert(f); fputs("tokc=x\ntoks=y\nbase=http://x\nsid=ab\n", f); fclose(f); }
  plex_esquecer_todos();
  snprintf(arq, sizeof arq, "%s/plex-p3.txt", dirDados);
  assert(stat(arq, &st) != 0);
  free(itens);
  printf("integration ok\n");
}

int main(int argc, char **argv) {
  assert(argc >= 3);
  snprintf(raiz, sizeof raiz, "%s", argv[1]);
  snprintf(dirDados, sizeof dirDados, "%s", argv[2]);
  // A TV backend: the desktop build declares no HEVC, so nothing would be
  // direct play there (that case is pinned in testarParsers).
  jellyfin_backend_definir(JF_BACKEND_WEBOS);
  testarPin();
  testarParsers();
  integracao();
  plex_encerrar();
  printf("plex: all ok\n");
  return 0;
}
