// Emby: the shared Jellyfin/Emby protocol layer on Emby fixtures, then the whole
// emby_* integration against tests/emby_server.py (fake Emby behind a /media
// reverse-proxy prefix, strict about the Emby-only routes and headers). Also
// checks that the Jellyfin instance and the servidores.c fan-out stay
// independent. No real server, never prints tokens or passwords.
#include "servidores.h"
#include "idbase.h"
#include <assert.h>
#include "stub_perfisessao.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// ---- stubs for the app modules the servers talk to
static char dirDados[512];
static int perfilAtivo = 1;
int perfis_ativo(void) { return perfilAtivo; }
int ajustes_jellyfin_ligado(void) { return 1; }
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
void dados_fs_travar(void) {}
void dados_fs_liberar(void) {}

#define SERVER_ID "9f8e7d6c5b4a39281706f5e4d3c2b1a0"
#define USER_ID "0a1b2c3d4e5f60718293a4b5c6d7e8f9"
static char raiz[256], base[300];

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
static JfEstado esperarEmby(JfEstado alvo, unsigned prazo) {
  unsigned long long fim = ms() + prazo;
  JfEstado e;
  char d[160];
  while ((e = emby_estado(d, sizeof d)) != alvo && ms() < fim) dormir(20);
  return e;
}

static int comeca(const char *s, const char *p) { return !strncmp(s, p, strlen(p)); }

static JfConta contaEmby(void) {
  JfConta c;
  memset(&c, 0, sizeof c);
  c.tipo = JF_TIPO_EMBY;
  snprintf(c.base, sizeof c.base, "http://emby.example:8096");
  snprintf(c.servidorId, sizeof c.servidorId, "%s", SERVER_ID);
  snprintf(c.usuarioId, sizeof c.usuarioId, "%s", USER_ID);
  snprintf(c.token, sizeof c.token, "tok-fixture");
  snprintf(c.dispositivoId, sizeof c.dispositivoId, "nuvio-test-p1");
  snprintf(c.dispositivoNome, sizeof c.dispositivoNome, "Test TV");
  return c;
}

// ------------------------------------------------------------------ fixtures
static const char VIEWS[] =
  "{\"Items\":[{\"Name\":\"Movies\",\"Id\":\"11\",\"CollectionType\":\"movies\"},"
  "{\"Name\":\"Shows\",\"Id\":\"22\",\"CollectionType\":\"tvshows\"},"
  "{\"Name\":\"Music\",\"Id\":\"33\",\"CollectionType\":\"music\"},"
  "{\"Name\":\"Mixed\",\"Id\":\"44\"}],\"TotalRecordCount\":4}";

static const char ITENS[] =
  "{\"Items\":[{\"Name\":\"Emby Movie\",\"Id\":\"1001\",\"Type\":\"Movie\",\"ProductionYear\":2011,"
  "\"RunTimeTicks\":72000000000,\"Overview\":\"Ov\",\"Genres\":[\"Action\",\"Sci-Fi\"],\"OfficialRating\":\"PG\","
  "\"CommunityRating\":6.5,\"ImageTags\":{\"Primary\":\"pt1\",\"Logo\":\"lt1\"},\"BackdropImageTags\":[\"bt1\"],"
  "\"UserData\":{\"PlaybackPositionTicks\":18000000000,\"PlayedPercentage\":25.0,\"Played\":false}},"
  "{\"Name\":\"Series\",\"Id\":\"2001\",\"Type\":\"Series\",\"ProductionYear\":2019},"
  "{\"Name\":\"A song\",\"Id\":\"5\",\"Type\":\"Audio\"},"
  "{\"Name\":\"Bad id\",\"Id\":\"../etc\",\"Type\":\"Movie\"}],\"TotalRecordCount\":120}";

static const char AUTH[] =
  "{\"User\":{\"Name\":\"emby user\",\"ServerId\":\"" SERVER_ID "\",\"Id\":\"" USER_ID "\",\"HasPassword\":true},"
  "\"SessionInfo\":{\"Id\":\"x\"},\"AccessToken\":\"embytoken0123456789abcdef0123456789\",\"ServerId\":\"" SERVER_ID "\"}";

static const char PLAYBACK[] =
  "{\"MediaSources\":["
  "{\"Id\":\"mediasource_1001\",\"Container\":\"mkv\",\"Name\":\"Emby 1080p\",\"Size\":4000000000,"
  " \"Path\":\"/srv/private/movie.mkv\",\"SupportsDirectPlay\":true,\"SupportsDirectStream\":true,"
  " \"TranscodingUrl\":\"/videos/1001/master.m3u8?MediaSourceId=mediasource_1001&PlaySessionId=ps1\","
  " \"MediaStreams\":[{\"Type\":\"Video\",\"Codec\":\"hevc\",\"Height\":1080,\"VideoRange\":\"HDR\"},"
  "{\"Type\":\"Audio\",\"Codec\":\"eac3\",\"Channels\":6}]},"
  "{\"Id\":\"1b2c3d\",\"Container\":\"mp4\",\"SupportsDirectPlay\":true,\"SupportsDirectStream\":false,"
  " \"TranscodingUrl\":\"/emby/videos/1001/master.m3u8?x=1\"},"
  "{\"Id\":\"bad id\\\"; drop\",\"Container\":\"mkv\",\"SupportsDirectPlay\":true}],"
  "\"PlaySessionId\":\"ps1\"}";

static void testarProtocolo(void) {
  JfConta c = contaEmby(), j;
  char buf[2000], norm[512];
  CatItem *it = malloc(sizeof(CatItem) * 8);
  CatEp *eps = malloc(sizeof(CatEp) * 8);
  JfBiblioteca bib[8];
  JfPlayback *pb = malloc(sizeof *pb);
  int total = 0, n;

  // --- base URL: the /emby root is ours to add, so a typed one is dropped
  assert(jf_url_normalizar_tipo(JF_TIPO_EMBY, "192.168.1.5:8096/emby/", norm, sizeof norm) == JF_OK);
  assert(!strcmp(norm, "http://192.168.1.5:8096"));
  assert(jf_url_normalizar_tipo(JF_TIPO_EMBY, "https://x.example/media/EMBY", norm, sizeof norm) == JF_OK);
  assert(!strcmp(norm, "https://x.example/media"));
  assert(jf_url_normalizar_tipo(JF_TIPO_EMBY, "https://x.example/embyfan", norm, sizeof norm) == JF_OK);
  assert(!strcmp(norm, "https://x.example/embyfan"));            // only a whole "/emby" segment is stripped
  assert(jf_url_normalizar_tipo(JF_TIPO_JELLYFIN, "https://x.example/emby", norm, sizeof norm) == JF_OK);
  assert(!strcmp(norm, "https://x.example/emby"));               // on Jellyfin it is just a path
  assert(jf_url_normalizar_tipo(JF_TIPO_EMBY, "ftp://x", norm, sizeof norm) == JF_ERR_ENTRADA);
  assert(jf_url_normalizar_tipo(JF_TIPO_EMBY, "http://u:p@x", norm, sizeof norm) == JF_ERR_ENTRADA);
  assert(!strcmp(jf_prefixo(&c), "/emby") && !strcmp(jf_tipo_nome(JF_TIPO_EMBY), "Emby"));
  j = c; j.tipo = JF_TIPO_JELLYFIN;
  assert(!strcmp(jf_prefixo(&j), "") && !strcmp(jf_tipo_nome(JF_TIPO_JELLYFIN), "Jellyfin"));

  // --- auth header: X-Emby-Authorization for Emby, Authorization for Jellyfin
  jf_cabecalho_auth(&c, buf, sizeof buf);
  assert(comeca(buf, "X-Emby-Authorization: MediaBrowser Client=\"Nuvio\", Device=\"Test TV\""));
  assert(strstr(buf, "DeviceId=\"nuvio-test-p1\"") && strstr(buf, "Token=\"tok-fixture\""));
  jf_cabecalho_auth(&j, buf, sizeof buf);
  assert(comeca(buf, "Authorization: MediaBrowser Client=\"Nuvio\""));
  c.token[0] = 0;
  jf_cabecalho_auth(&c, buf, sizeof buf);
  assert(!strstr(buf, "Token=") && comeca(buf, "X-Emby-Authorization:"));
  snprintf(c.token, sizeof c.token, "tok-fixture");
  snprintf(c.dispositivoNome, sizeof c.dispositivoNome, "Q\"uote, \xc3\xa9");           // header injection guard
  jf_cabecalho_auth(&c, buf, sizeof buf);
  assert(!strstr(buf, "Q\"uote") && !strchr(buf, '\n'));
  snprintf(c.dispositivoNome, sizeof c.dispositivoNome, "Test TV");

  // --- authentication answer
  { JfConta a = contaEmby();
    char corpo[1024];
    a.token[0] = a.usuarioId[0] = 0;
    snprintf(corpo, sizeof corpo, "%s", AUTH);
    assert(jf_ler_autenticacao(&a, corpo, strlen(corpo)) == JF_OK);
    assert(!strcmp(a.token, "embytoken0123456789abcdef0123456789") && !strcmp(a.usuarioId, USER_ID) &&
           !strcmp(a.usuarioNome, "emby user"));
    assert(!corpo[0]);                                           // the buffer holding the token is wiped
    snprintf(corpo, sizeof corpo, "{\"User\":{\"Id\":\"nothex!\"},\"AccessToken\":\"t\"}");
    assert(jf_ler_autenticacao(&a, corpo, strlen(corpo)) == JF_ERR_FORMATO); }

  // --- libraries, items (numeric ids), namespace em:
  assert(jf_ler_bibliotecas(VIEWS, strlen(VIEWS), bib, 8) == 3);
  assert(!strcmp(bib[2].id, "44") && !strcmp(bib[2].tipo, "movie"));
  assert(!strcmp(bib[0].id, "11") && !strcmp(bib[0].tipo, "movie") && !strcmp(bib[1].tipo, "series"));
  n = jf_ler_itens(&c, ITENS, strlen(ITENS), it, 8, &total);
  assert(n == 2 && total == 120);                                // Audio and the path-traversal id are skipped
  assert(!strcmp(it[0].imdb, "em:9f8e7d6c.1001") && !strcmp(it[0].origem, "emby"));
  assert(jfid_e(it[0].imdb) && idbase_len(it[0].imdb) == strlen(it[0].imdb));
  assert(!strcmp(it[0].meta, "2011 · 120 min") && it[0].nota == 65 && it[0].progresso == 25 && it[0].restanteMin == 90);
  assert(!strcmp(it[0].genero, "Action · Sci-Fi"));
  assert(!strcmp(it[0].poster, "http://emby.example:8096/emby/Items/1001/Images/Primary?maxHeight=600&quality=90&tag=pt1"));
  assert(strstr(it[0].logo, "/emby/Items/1001/Images/Logo") && strstr(it[0].backdrop, "/emby/Items/1001/Images/Backdrop"));
  assert(!strstr(it[0].poster, "api_key") && !strstr(it[0].poster, "tok-fixture"));   // images stay anonymous
  assert(!strcmp(it[1].tipo, "series") && !strcmp(it[1].imdb, "em:9f8e7d6c.2001"));
  // The same body read as Jellyfin gets the jf: namespace and no /emby root.
  { JfConta jc = c; jc.tipo = JF_TIPO_JELLYFIN;
    n = jf_ler_itens(&jc, ITENS, strlen(ITENS), it, 8, &total);
    assert(n == 2 && comeca(it[0].imdb, "jf:") && !strstr(it[0].poster, "/emby/") && !strcmp(it[0].origem, "jellyfin")); }

  // --- episodes
  { const char *e =
      "{\"Items\":[{\"Name\":\"Pilot\",\"Id\":\"3011\",\"Type\":\"Episode\",\"ParentIndexNumber\":1,\"IndexNumber\":1,"
      "\"RunTimeTicks\":27000000000,\"PremiereDate\":\"2019-03-04T00:00:00.0000000Z\",\"ImageTags\":{\"Primary\":\"e1\"}},"
      "{\"Name\":\"Special\",\"Id\":\"3099\",\"Type\":\"Episode\",\"ParentIndexNumber\":0}]}";
    n = jf_ler_episodios(&c, e, strlen(e), eps, 8);
    assert(n == 1 && eps[0].temporada == 1 && eps[0].episodio == 1 && !strcmp(eps[0].vid, "em:9f8e7d6c.3011"));
    assert(!strcmp(eps[0].data, "04/03/2019") && !strcmp(eps[0].duracao, "45 min") && strstr(eps[0].thumb, "/emby/Items/3011/")); }

  // --- playback info: mediasource_ ids, /emby on the transcode path, VideoRange fallback
  assert(jf_ler_playbackinfo(&c, "1001", PLAYBACK, strlen(PLAYBACK), pb) == JF_OK);
  assert(pb->n == 4);                                            // 2 per usable source (direct + HLS); the unsafe id is dropped
  assert(pb->sessao[0].metodo == JF_METODO_DIRETO && !strcmp(pb->sessao[0].fonteId, "mediasource_1001"));
  assert(comeca(pb->sessao[0].url, "http://emby.example:8096/emby/Videos/1001/stream?static=true&MediaSourceId=mediasource_1001"));
  assert(strstr(pb->sessao[0].url, "api_key=tok-fixture"));
  assert(pb->sessao[1].metodo == JF_METODO_STREAM &&
         comeca(pb->sessao[1].url, "http://emby.example:8096/emby/videos/1001/master.m3u8?"));
  assert(!strstr(pb->sessao[1].url, "/emby/emby/"));
  assert(pb->sessao[3].metodo == JF_METODO_TRANSCODE && comeca(pb->sessao[3].url, "http://emby.example:8096/emby/videos/1001/master.m3u8?x=1") &&
         !strstr(pb->sessao[3].url, "/emby/emby/"));                                            // already under /emby: not doubled
  assert(comeca(pb->sessao[2].url, "http://emby.example:8096/emby/Videos/1001/stream") && !strcmp(pb->sessao[2].fonteId, "1b2c3d"));
  assert(!strcmp(pb->fonte[0].provedor, "Emby") && pb->fonte[0].altura == 1080 && comeca(pb->fonte[0].bingeGroup, "emby|"));
  assert(strstr(pb->fonte[0].descricao, "Emby 1080p") && strstr(pb->fonte[0].descricao, "HEVC · HDR · EAC3 5.1 · MKV"));
  assert(!strstr(pb->fonte[0].descricao, "/srv/private") && pb->fonte[0].tamanhoBytes == 4000000000ull);
  { const char *twice = "{\"MediaSources\":[{\"Id\":\"m1\",\"SupportsDirectPlay\":false,\"TranscodingUrl\":\"/emby/videos/9/master.m3u8\"}]}";
    assert(jf_ler_playbackinfo(&c, "9", twice, strlen(twice), pb) == JF_OK && pb->n == 1);
    assert(comeca(pb->sessao[0].url, "http://emby.example:8096/emby/videos/9/master.m3u8?api_key=")); }
  assert(jf_ler_playbackinfo(&c, "9", "{\"x\":1}", 7, pb) == JF_ERR_FORMATO);
  free(it); free(eps); free(pb);
  printf("emby protocol ok\n");
}

// ------------------------------------------------------------- integration
static void integracao(void) {
  char det[160], arq[640], urlDireta[2048], urlStream[2048];
  CatItem *itens = malloc(sizeof(CatItem) * JF_FIL_MAX * JF_POR_FILEIRA);
  CatFileira fils[JF_FIL_MAX];
  int nItens = 0, nf, n = 0, r;
  unsigned long long fim;
  Stream *lista = NULL;
  struct stat st;
  char senha[64];
  char *ev;

  emby_carregar();
  assert(emby_estado(det, sizeof det) == JF_EST_SEM_SERVIDOR && !emby_conectado() && !jellyfin_conectado());

  // ---- a Jellyfin address in the Emby row (and the reverse) is refused by product name
  snprintf(base, sizeof base, "%s/jf", raiz);
  assert(emby_definir_servidor(base));
  assert(esperarEmby(JF_EST_ERRO, 5000) == JF_EST_ERRO && emby_ultimo_erro() == JF_ERR_FORMATO);
  snprintf(base, sizeof base, "%s/media", raiz);                                  // the Emby server, typed WITHOUT /emby
  assert(jellyfin_definir_servidor(base));                                         // Jellyfin row on an Emby host: no such route
  { unsigned long long f2 = ms() + 5000; char d2[160]; JfEstado x;
    while ((x = jellyfin_estado(d2, sizeof d2)) != JF_EST_ERRO && ms() < f2) dormir(20);
    assert(x == JF_EST_ERRO && jellyfin_ultimo_erro() == JF_ERR_FORMATO); }
  jellyfin_esquecer();

  // ---- the real thing: typed with the /emby suffix and a proxy prefix
  snprintf(base, sizeof base, "%s/media/emby/", raiz);
  assert(emby_definir_servidor(base));
  assert(esperarEmby(JF_EST_SERVIDOR_OK, 5000) == JF_EST_SERVIDOR_OK);
  assert(emby_qc_permitido() == 0 && !emby_entrar_quick_connect());               // Emby has no Quick Connect
  assert(comeca(emby_servidor_curto(), "127.0.0.1:") && strstr(emby_servidor_curto(), "/media") &&
         !strstr(emby_servidor_curto(), "/emby"));
  emby_estado(det, sizeof det);
  assert(strstr(det, "Emby Home") && strstr(det, "4.8.8.0"));

  // ---- wrong password, then the right one; the password buffer is wiped either way
  snprintf(senha, sizeof senha, "wrong");
  assert(emby_entrar_senha("emby user", senha) && !senha[0]);
  assert(esperarEmby(JF_EST_ERRO, 5000) == JF_EST_ERRO && emby_ultimo_erro() == JF_ERR_AUTH && !emby_conectado());
  snprintf(senha, sizeof senha, "emby p\xc3\xa4ssword");
  assert(emby_entrar_senha("emby user", senha) && !senha[0]);
  assert(esperarEmby(JF_EST_CONECTADO, 8000) == JF_EST_CONECTADO);
  assert(emby_conectado() && !strcmp(emby_usuario(), "emby user"));
  assert(!jellyfin_conectado());                                                   // the other instance is untouched
  snprintf(arq, sizeof arq, "%s/emby-p1.txt", dirDados);
  assert(stat(arq, &st) == 0 && (st.st_mode & 077) == 0);
  snprintf(arq, sizeof arq, "%s/jellyfin-p1.txt", dirDados);
  assert(stat(arq, &st) != 0);

  // ---- Home rows: Emby-only routes, "emby_" keys
  fim = ms() + 8000;
  nf = 0;
  while (ms() < fim) {
    nf = emby_fileiras_copiar(itens, JF_FIL_MAX * JF_POR_FILEIRA, fils, JF_FIL_MAX, &nItens);
    if (nf == 2) break;
    dormir(30);
  }
  assert(nf == 2 && nItens == 3);
  assert(!strcmp(fils[0].titulo, "Movies · Emby") && !strcmp(fils[0].chave, "emby_11") && fils[0].n == 2);
  assert(!strcmp(fils[1].titulo, "Shows · Emby") && fils[1].ini == 2 && !strcmp(fils[1].tipo, "series"));
  assert(emby_chave_fileira("emby_11") && !emby_chave_fileira("jellyfin_11") && !jellyfin_chave_fileira("emby_11"));
  assert(comeca(itens[0].imdb, "em:9f8e7d6c.") && itens[0].progresso == 25);
  { int nj = jellyfin_fileiras_copiar(itens, 10, fils, 2, &n); assert(nj == 0 && n == 0); }

  // ---- the fan-out in servidores.c: Emby rows come through it, ids route by namespace
  { CatItem *x = malloc(sizeof(CatItem) * SRV_ITENS_MAX);
    CatFileira f[SRV_FIL_MAX];
    int tot = 0, nr = servidores_fileiras_copiar(x, SRV_ITENS_MAX, f, SRV_FIL_MAX, &tot);
    assert(nr == 2 && tot == 3 && f[1].ini == 2 && servidores_conectado());
    assert(servidores_chave_fileira("emby_11") && servidores_chave_fileira("plex_1") && !servidores_chave_fileira("cw"));
    free(x); }
  assert(!jellyfin_fontes_pedir("em:9f8e7d6c.1001"));                              // not a jf: id
  assert(!plex_fontes_pedir("em:9f8e7d6c.1001"));

  // ---- title page
  { CatItem *x = calloc(1, sizeof *x);
    CatEp *eps = calloc(32, sizeof *eps);
    snprintf(x->imdb, sizeof x->imdb, "em:9f8e7d6c.1001");
    snprintf(x->tipo, sizeof x->tipo, "movie");
    assert(emby_ficha(x, eps, 32) == 0 && x->nElenco == 1 && !strcmp(x->elenco[0].nome, "Mov Actor"));
    memset(x, 0, sizeof *x);
    snprintf(x->imdb, sizeof x->imdb, "em:9f8e7d6c.2001");
    snprintf(x->tipo, sizeof x->tipo, "series");
    assert(servidores_ficha(x, eps, 32) == 4);
    assert(x->nTemporadas == 2 && x->nElenco == 1 && !strcmp(x->direcao, "A Director"));
    snprintf(x->imdb, sizeof x->imdb, "em:00000000.2001");                         // another server's id
    assert(emby_ficha(x, eps, 32) == -1);
    free(x); free(eps); }

  // ---- sources + check-ins
  assert(servidores_fontes_pedir("em:9f8e7d6c.1001"));
  fim = ms() + 5000;
  while ((r = servidores_fontes_colher("em:9f8e7d6c.1001", &lista, &n)) == JF_FONTES_PENDENTE && ms() < fim) dormir(20);
  assert(r == JF_FONTES_PRONTO && n == 2 && lista);                                // direct + HLS (DirectStream)
  assert(!strcmp(lista[0].provedor, "Emby") && strstr(lista[0].url, "/media/emby/Videos/1001/stream"));
  snprintf(urlDireta, sizeof urlDireta, "%s", lista[0].url);
  snprintf(urlStream, sizeof urlStream, "%s", lista[1].url);
  assert(strstr(urlStream, "/media/emby/videos/1001/master.m3u8"));
  free(lista); lista = NULL;
  servidores_reproducao_tick(urlDireta, 0, 7200, 0);
  servidores_reproducao_tick("https://addon.example/x.mp4", 1, 7200, 1);          // unknown URL ignored
  servidores_reproducao_tick(urlDireta, 1, 7200, 1);                              // started
  servidores_reproducao_tick(urlDireta, 3, 7200, 0);                              // pause
  servidores_reproducao_tick(urlStream, 0, 7200, 1);                              // source switch: stop the first, start the second
  servidores_reproducao_fim(urlStream, 5, 7200);                                  // stop + transcode release
  fim = ms() + 4000;
  while (emby_relatorios_pendentes() && ms() < fim) dormir(20);
  ev = controle("events");
  assert(conta(ev, "\"route\": \"Playing\"") == 2 && conta(ev, "\"route\": \"Stopped\"") == 2 &&
         conta(ev, "\"route\": \"Progress\"") == 1);
  assert(strstr(ev, "\"source\": \"mediasource_1001\"") && strstr(ev, "\"ticks\": 50000000"));
  assert(strstr(ev, "encoding-delete"));
  free(ev);

  // ---- nothing hit a route Emby does not have (/UserViews, /Items?userId=, /QuickConnect/...)
  ev = controle("unknown");
  assert(!strcmp(ev, "[]"));
  free(ev);

  // ---- profile switch drops Emby, the file brings it back
  perfilAtivo = 2;
  emby_perfil_trocou();
  assert(!emby_conectado() && emby_fileiras_copiar(itens, 10, fils, 2, &n) == 0);
  perfilAtivo = 1;
  emby_perfil_trocou();
  assert(emby_conectado());

  // ---- sign out: the server hears about it, the file goes
  emby_esquecer();
  assert(emby_estado(det, sizeof det) == JF_EST_SEM_SERVIDOR && !emby_conectado());
  snprintf(arq, sizeof arq, "%s/emby-p1.txt", dirDados);
  assert(stat(arq, &st) != 0);
  fim = ms() + 4000;
  do { ev = controle("events"); if (strstr(ev, "logout")) break; free(ev); ev = NULL; dormir(30); } while (ms() < fim);
  assert(ev && strstr(ev, "\"route\": \"logout\""));
  free(ev);

  // ---- account logout wipes every profile's Emby file (and only those)
  { FILE *f;
    snprintf(arq, sizeof arq, "%s/emby-p4.txt", dirDados);
    f = fopen(arq, "w"); assert(f); fputs("token=x\n", f); fclose(f);
    snprintf(arq, sizeof arq, "%s/keep.txt", dirDados);
    f = fopen(arq, "w"); assert(f); fputs("keep\n", f); fclose(f); }
  servidores_esquecer_todos();
  snprintf(arq, sizeof arq, "%s/emby-p4.txt", dirDados);
  assert(stat(arq, &st) != 0);
  snprintf(arq, sizeof arq, "%s/keep.txt", dirDados);
  assert(stat(arq, &st) == 0);
  free(itens);
  printf("integration ok\n");
}

int main(int argc, char **argv) {
  assert(argc >= 3);
  snprintf(raiz, sizeof raiz, "%s", argv[1]);
  snprintf(dirDados, sizeof dirDados, "%s", argv[2]);
  testarProtocolo();
  integracao();
  emby_encerrar();
  jellyfin_encerrar();
  plex_encerrar();
  printf("emby: all ok\n");
  return 0;
}
