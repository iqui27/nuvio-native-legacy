// F11 Jellyfin: protocol + integration against tests/jellyfin_server.py.
// No external provider, no real server, never prints tokens or passwords.
#include "jellyfin.h"
#include "idbase.h"
#include "srvidx.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// ---- stubs for the app modules jellyfin.c talks to (no dados.c/perfis.c)
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

static char base[256], raiz[256];

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

// GET a player URL with no Authorization header, as the TV pipelines do:
// 1 when the fake accepts the query token.
static int midiaAbre(const char *url) {
  RedePedido p;
  RedeResposta r;
  int ok;
  memset(&p, 0, sizeof p);
  p.url = url;
  rede_pedir(&p, &r);
  ok = r.erro == REDE_OK && r.status == 200;
  rede_resposta_limpar(&r);
  return ok;
}

static JfEstado esperarEstado(JfEstado alvo, unsigned prazo) {
  unsigned long long fim = ms() + prazo;
  JfEstado e;
  char d[160];
  while ((e = jellyfin_estado(d, sizeof d)) != alvo && ms() < fim) dormir(20);
  return e;
}

static void testarIds(void) {
  char id[JFID_MAX], tag[JFID_TAG + 1], item[JFID_ITEM + 1];
  assert(jfid_montar(id, sizeof id, "0F1E2D3C-4B5A-6978-8796-A5B4C3D2E1F0", "C0FFEE000000000000000000000000AB"));
  assert(!strcmp(id, "jf:0f1e2d3c.c0ffee000000000000000000000000ab"));
  assert(strlen(id) < 64);
  assert(jfid_e(id) && !jfid_e("tt0111161") && !jfid_e("kitsu:1"));
  // Whole id is the base: no invented season/episode, no collision with tt.
  assert(idbase_len(id) == strlen(id));
  assert(!idbase_tem_episodio(id));
  assert(jfid_partes(id, tag, item) && !strcmp(tag, "0f1e2d3c"));
  assert(!jfid_partes("jf:0f1e2d3c.abc:1:2", tag, item));
  assert(!jfid_partes("jf:zzzz2d3c.abc", tag, item));
  assert(!jfid_montar(id, sizeof id, "short", "abc"));
  assert(!jfid_montar(id, sizeof id, "0f1e2d3c4b5a69788796a5b4c3d2e1f0", "../etc"));
  printf("ids ok\n");
}

static void testarUrl(void) {
  char u[512];
  assert(jf_url_normalizar("192.168.1.5:8096", u, sizeof u) == JF_OK && !strcmp(u, "http://192.168.1.5:8096"));
  assert(jf_url_normalizar(" HTTPS://media.example/jellyfin/ ", u, sizeof u) == JF_OK &&
         !strcmp(u, "https://media.example/jellyfin"));
  assert(jf_url_normalizar("http://user:pw@host", u, sizeof u) == JF_ERR_ENTRADA);
  assert(jf_url_normalizar("ftp://host", u, sizeof u) == JF_ERR_ENTRADA);
  assert(jf_url_normalizar("http://host/?x=1", u, sizeof u) == JF_ERR_ENTRADA);
  assert(jf_url_normalizar("ho st", u, sizeof u) == JF_ERR_ENTRADA);
  assert(jf_url_normalizar("", u, sizeof u) == JF_ERR_ENTRADA);
  printf("url ok\n");
}

static void testarProtocolo(void) {
  JfConta c;
  char segredo[160], codigo[16], senha[64], perfil[2048];
  JfBiblioteca libs[6];
  CatItem *itens = calloc(100, sizeof *itens), det;
  CatEp eps[16];
  JfPlayback *pb = malloc(sizeof *pb);
  int n, total, e, k;
  char *ev;
  memset(&c, 0, sizeof c);
  snprintf(c.dispositivoId, sizeof c.dispositivoId, "nuvio-test-p1");
  // Without the reverse-proxy prefix the route is not a Jellyfin root.
  assert(jf_publico(raiz, NULL, &c) == JF_ERR_FORMATO);
  assert(jf_publico(base, NULL, &c) == JF_OK);
  assert(!strcmp(c.base, base) && !strcmp(c.servidorNome, "Home Server") && !strcmp(c.versao, "10.10.3"));
  assert(!strcmp(c.servidorId, "0f1e2d3c4b5a69788796a5b4c3d2e1f0"));
  // Quick Connect: Initiate is POST; pending, then approved.
  assert(jf_qc_habilitado(&c, NULL) == 1);
  assert(jf_qc_iniciar(&c, NULL, segredo, sizeof segredo, codigo, sizeof codigo) == JF_OK);
  assert(!strcmp(codigo, "482913"));
  assert(jf_qc_conferir(&c, NULL, "nope") == JF_ERR_EXPIRADO);
  assert(jf_qc_conferir(&c, NULL, segredo) == 0);
  assert(jf_qc_conferir(&c, NULL, segredo) == 1);
  assert(jf_qc_autenticar(&c, NULL, segredo) == JF_OK && c.token[0] && !strcmp(c.usuarioNome, "ana"));
  // Password: wrong is 401, right gives a token; the buffer is wiped both ways.
  snprintf(senha, sizeof senha, "wrong");
  assert(jf_autenticar_senha(&c, NULL, "ana", senha) == JF_ERR_AUTH);
  assert(senha[0] == 0);
  snprintf(senha, sizeof senha, "%s", "p\xc3\xa4ssw\xc3\xb6rd \"quoted\"");
  assert(jf_autenticar_senha(&c, NULL, "ana", senha) == JF_OK);
  for (k = 0; k < (int)sizeof senha; k++) assert(senha[k] == 0);
  assert(!strcmp(c.usuarioId, "aa11bb22cc33dd44ee55ff6677889900"));
  // Libraries: music filtered out.
  n = jf_bibliotecas(&c, NULL, libs, 6);
  assert(n == 2 && !strcmp(libs[0].tipo, "movie") && !strcmp(libs[1].tipo, "series"));
  // Paging: 60 movies, two pages, prefixed ids, root-level fields.
  n = jf_itens(&c, NULL, libs[0].id, 0, 24, itens, 100, &total);
  assert(n == 24 && total == 60);
  assert(jfid_e(itens[0].imdb) && !strcmp(itens[0].tipo, "movie") && !strcmp(itens[0].titulo, "Movie 00"));
  assert(itens[0].progresso == 50 && itens[0].restanteMin == 60 && strstr(itens[0].genero, "Mystery"));
  assert(strstr(itens[0].poster, "/jf/Items/") && !strstr(itens[0].poster, "api_key") && !strstr(itens[0].poster, c.token));
  n = jf_itens(&c, NULL, libs[0].id, 48, 24, itens, 100, &total);
  assert(n == 12 && !strcmp(itens[0].titulo, "Movie 48"));
  n = jf_itens(&c, NULL, libs[1].id, 0, 24, itens, 100, &total);
  assert(n == 2 && !strcmp(itens[0].tipo, "series"));   // episode row ignored
  // A series with an IMDb id IS that catalogue title (key spelled "IMDB").
  assert(jfid_e(itens[0].imdb) && !strcmp(itens[1].imdb, "tt0900001"));
  // Movies: no external id stays opaque, IMDb wins over TMDB, TMDB alone is "tmdb:".
  n = jf_itens(&c, NULL, libs[0].id, 0, 60, itens, 100, &total);
  assert(n == 60 && jfid_e(itens[4].imdb) && !strcmp(itens[5].imdb, "tt0001005") && !strcmp(itens[50].imdb, "tmdb:5050"));
  // The matching index: every movie and series, one page here.
  { SrvIdx x;
    srvidx_iniciar(&x);
    assert(jf_indice(&c, NULL, 0, 200, 0, &x, &total) == 62 && total == 62);
    assert(x.n == 62 && srvidx_n_tipo(&x, 's') == 2);
    assert(srvidx_achar(&x, "tt0001007", 0, 0, 'm') >= 0 && x.v[srvidx_achar(&x, "tt0001007", 0, 0, 'm')].altura == 2160);
    assert(srvidx_achar(&x, NULL, 5055, 0, 'm') >= 0 && srvidx_achar(&x, NULL, 0, 999, 's') >= 0);
    assert(srvidx_achar_titulo(&x, srvidx_titulo_hash("movie 02"), 2002, 'm') >= 0);  // home video: title + year
    assert(x.marca == srvidx_iso("2026-09-01T01:41:00Z"));
    // Episodes of a series, on demand.
    assert(jf_indice_episodios(&c, NULL, "5e1e5e1e5e1e5e1e5e1e5e1e5e1e5e1e", &x) == 6);
    assert(srvidx_achar_ep(&x, "5e1e5e1e5e1e5e1e5e1e5e1e5e1e5e1e", 2, 3) >= 0);
    srvidx_limpar(&x); }
  // Detail: nested MediaSources "Name"/"RunTimeTicks" must not win.
  e = jf_detalhe(&c, NULL, "00000000000000000000000c0ffee001", &det);
  assert(e == JF_OK && !strcmp(det.titulo, "Movie 01") && det.nElenco == 2 && !strcmp(det.direcao, "Dir Ector"));
  assert(strstr(det.meta, "120 min"));
  n = jf_episodios(&c, NULL, "abcdef0123456789abcdef0123456789", eps, 16);
  assert(n == 6 && eps[3].temporada == 2 && eps[3].episodio == 1 && jfid_e(eps[3].vid));
  assert(!strcmp(eps[0].data, "11/01/2019"));
  // Device profiles per backend.
  for (k = JF_BACKEND_HOST; k <= JF_BACKEND_TPK; k++) {
    assert(jf_perfil_dispositivo((JfBackend)k, perfil, sizeof perfil));
    assert(strstr(perfil, "DirectPlayProfiles") && !strstr(perfil, "dts"));
  }
  assert(!jf_perfil_dispositivo(JF_BACKEND_WGT, perfil, sizeof perfil));
  // PlaybackInfo: direct play first, transcode HLS from the server's URL.
  e = jf_playbackinfo(&c, NULL, "00000000000000000000000c0ffee001", JF_BACKEND_ANDROID, pb);
  assert(e == JF_OK && pb->n == 2);
  assert(pb->sessao[0].metodo == JF_METODO_DIRETO && pb->fonte[0].altura == 2160);
  assert(strstr(pb->fonte[0].url, "/jf/Videos/") && strstr(pb->fonte[0].url, "static=true") &&
         strstr(pb->fonte[0].url, "ApiKey=") && !strstr(pb->fonte[0].url, "api_key="));
  assert(!strstr(pb->fonte[0].descricao, "/srv/private"));
  assert(strstr(pb->fonte[0].descricao, "HDR10") && strstr(pb->fonte[0].descricao, "EAC3 5.1"));
  assert(pb->sessao[1].metodo == JF_METODO_TRANSCODE && strstr(pb->fonte[1].url, "/jf/videos/") &&
         strstr(pb->fonte[1].url, "master.m3u8") && strstr(pb->fonte[1].url, "ApiKey=") &&
         !strstr(pb->fonte[1].url, "api_key="));            // the server's legacy name, renamed
  assert(midiaAbre(pb->fonte[0].url) && midiaAbre(pb->fonte[1].url));
  assert(!strcmp(pb->sessao[0].sessaoId, "ps0001") && !strcmp(pb->fonte[0].provedor, "Jellyfin"));
  // Check-ins carry int64 ticks: 3 h = 108,000,000,000 ticks.
  free(controle("reset"));
  assert(jf_reportar(&c, NULL, JF_REL_INICIO, &pb->sessao[1], 0) == JF_OK);
  assert(jf_reportar(&c, NULL, JF_REL_PROGRESSO, &pb->sessao[1], 10800.0) == JF_OK);
  assert(jf_reportar(&c, NULL, JF_REL_FIM, &pb->sessao[1], 10800.0) == JF_OK);
  ev = controle("events");
  assert(conta(ev, "\"Playing\"") == 1 && conta(ev, "\"Progress\"") == 1 && conta(ev, "\"Stopped\"") == 1);
  assert(strstr(ev, "108000000000") && strstr(ev, "\"Transcode\"") && strstr(ev, "\"release\""));
  free(ev);
  assert(jf_ticks(1.5) == 15000000LL && jf_seg(15000000LL) == 1.5);
  // JELLYFIN 12 (legacy auth off): "ApiKey" still opens, "api_key" is 401.
  free(controle("version?v=12.0.0"));
  { JfConta c12 = c;
    char velho[2048];
    assert(jf_publico(base, NULL, &c12) == JF_OK && !strcmp(c12.versao, "12.0.0"));
    assert(jf_apikey_moderna(&c12));
    e = jf_playbackinfo(&c12, NULL, "00000000000000000000000c0ffee001", JF_BACKEND_WEBOS, pb);
    assert(e == JF_OK && pb->n == 2);
    assert(strstr(pb->fonte[0].url, "ApiKey=") && strstr(pb->fonte[1].url, "ApiKey="));
    assert(midiaAbre(pb->fonte[0].url) && midiaAbre(pb->fonte[1].url));
    snprintf(velho, sizeof velho, "%s", pb->fonte[0].url);
    { char *k = strstr(velho, "ApiKey=");
      size_t cauda = strlen(k + 7);
      memmove(k + 8, k + 7, cauda + 1);
      memcpy(k, "api_key=", 8); }
    assert(!midiaAbre(velho));
    // A server older than 10.8 only reads "api_key"; Emby always does.
    free(controle("version?v=10.7.5"));
    assert(jf_publico(base, NULL, &c12) == JF_OK && !jf_apikey_moderna(&c12));
    e = jf_playbackinfo(&c12, NULL, "00000000000000000000000c0ffee001", JF_BACKEND_WEBOS, pb);
    assert(e == JF_OK && strstr(pb->fonte[0].url, "api_key=") && !strstr(pb->fonte[0].url, "ApiKey="));
    assert(midiaAbre(pb->fonte[0].url) && midiaAbre(pb->fonte[1].url));
    c12.tipo = JF_TIPO_EMBY;
    assert(!jf_apikey_moderna(&c12));
    c12.tipo = JF_TIPO_JELLYFIN;
    c12.versao[0] = 0;
    assert(jf_apikey_moderna(&c12));
    free(controle("version?v=10.10.3")); }
  // 401 after the server revokes the token.
  free(controle("revoke"));
  assert(jf_bibliotecas(&c, NULL, libs, 6) == JF_ERR_AUTH);
  free(itens); free(pb);
  printf("protocol ok\n");
}

static void testarIntegracao(void) {
  char nome[700], *arq, senha[64];
  struct stat st;
  CatItem *itens = calloc(JF_FIL_MAX * JF_POR_FILEIRA, sizeof *itens);
  CatFileira fils[JF_FIL_MAX];
  CatItem det;
  CatEp eps[16];
  Stream *lista;
  unsigned v0;
  int n, ni, r;
  unsigned long long t0, fim;
  char alvo[JFID_MAX], *ev;

  jellyfin_carregar();
  assert(jellyfin_estado(NULL, 0) == JF_EST_SEM_SERVIDOR);
  assert(!jellyfin_definir_servidor("http://u:p@bad"));
  assert(jellyfin_definir_servidor(base));
  assert(esperarEstado(JF_EST_SERVIDOR_OK, 5000) == JF_EST_SERVIDOR_OK);
  assert(jellyfin_qc_permitido() == 1);
  assert(!strncmp(jellyfin_servidor_curto(), "127.0.0.1:", 10));
  v0 = jellyfin_fileiras_versao();
  snprintf(senha, sizeof senha, "%s", "p\xc3\xa4ssw\xc3\xb6rd \"quoted\"");
  assert(jellyfin_entrar_senha("ana", senha));
  assert(senha[0] == 0);
  assert(esperarEstado(JF_EST_CONECTADO, 5000) == JF_EST_CONECTADO);
  // Token file: 0600, has the token, never the password.
  dados_caminho(nome, sizeof nome, "jellyfin-p1.txt");
  assert(stat(nome, &st) == 0 && (st.st_mode & 077) == 0);
  arq = dados_ler("jellyfin-p1.txt");
  assert(arq && strstr(arq, "token=pwtoken") && !strstr(arq, "ssw") && strstr(arq, "dev=nuvio-"));
  free(arq);
  // Home rows arrive as a new snapshot version.
  fim = ms() + 5000;
  while (jellyfin_fileiras_versao() == v0 && ms() < fim) dormir(20);
  n = jellyfin_fileiras_copiar(itens, JF_FIL_MAX * JF_POR_FILEIRA, fils, JF_FIL_MAX, &ni);
  assert(n == 2 && ni == JF_POR_FILEIRA + 2);
  assert(jellyfin_chave_fileira(fils[0].chave) && !fils[0].base[0] && strstr(fils[0].titulo, "Jellyfin"));
  // Title page through the profile's connection.
  det = itens[JF_POR_FILEIRA];                 // the series
  r = jellyfin_ficha(&det, eps, 16);
  assert(r == 6 && det.nTemporadas == 2 && det.temporadas[1] == 2);
  // MATCHED TITLES: the index arrives after the rows (full pass), and is on disk.
  fim = ms() + 5000;
  while (jellyfin_indice_n() < 62 && ms() < fim) dormir(20);
  assert(jellyfin_indice_n() == 62);
  arq = dados_ler("srvidx-p1-jf.txt");
  assert(arq && !strncmp(arq, "srvidx 1 0f1e2d3c ", 18) && strstr(arq, "tt0001007") && !strstr(arq, "Movie"));
  free(arq);
  { SrvAchado a;
    assert(jellyfin_tem("tt0001007", "movie", &a) && a.altura == 2160 && !strcmp(a.servidor, "Home Server") &&
           !strcmp(a.tipoSrv, "Jellyfin") && a.local);
    assert(!jellyfin_tem("tt0001007", "series", NULL) && !jellyfin_tem("tt7777777", "movie", NULL));
    assert(jellyfin_tem("tmdb:5055", "movie", NULL) && jellyfin_tem("tt0900001:1:2", "series", NULL)); }
  free(controle("reset"));
  r = jellyfin_consultar("tt0001007", "movie", NULL, 0, NULL, NULL, &lista);
  assert(r == 2 && lista && strstr(lista[0].url, "c0ffee007/stream") &&
         strstr(lista[0].url, "ApiKey=") && !strcmp(lista[0].provedor, "Home Server"));
  // Its check-ins work like the server's own items (URL registered).
  jellyfin_reproducao_tick(lista[0].url, 1, 7200, 1);
  jellyfin_reproducao_fim(lista[0].url, 2, 7200);
  free(lista); lista = NULL;
  fim = ms() + 5000;
  while (jellyfin_relatorios_pendentes() && ms() < fim) dormir(20);
  ev = controle("events");
  assert(conta(ev, "\"Playing\"") == 1 && conta(ev, "\"Stopped\"") == 1);
  free(ev);
  assert(jellyfin_consultar("tt7777777", "movie", NULL, 0, NULL, NULL, &lista) == 0 && !lista);
  // Last rule: title + year, only when the page that asked says them.
  assert(jellyfin_consultar("tt5555555", "movie", NULL, 0, NULL, NULL, &lista) == 0);
  assert(jellyfin_consultar("tt5555555", "movie", "Movie 02", 2003, NULL, NULL, &lista) == 2 && lista);
  free(lista); lista = NULL;
  // Episode: fetched once on demand, then offline (no second episode request).
  free(controle("index"));
  assert(jellyfin_consultar("tt0900001:2:3", "series", NULL, 0, NULL, NULL, &lista) == 2 && lista);
  assert(strstr(lista[0].url, "e90000cb/stream"));    // episode_id(2, 3)
  free(lista); lista = NULL;
  assert(jellyfin_consultar("tt0900001:1:1", "series", NULL, 0, NULL, NULL, &lista) == 2);
  free(lista); lista = NULL;
  assert(jellyfin_consultar("tt0900001:9:9", "series", NULL, 0, NULL, NULL, &lista) == 0);   // not there
  { char *ix = controle("index"); assert(strstr(ix, "\"episodes\": 1")); free(ix); }
  // INCREMENTAL: a movie added on the server comes with a "since" request only.
  free(controle("add?n=70"));
  jellyfin_recarregar_bibliotecas();
  fim = ms() + 5000;
  while (!jellyfin_tem("tt0001070", "movie", NULL) && ms() < fim) dormir(20);
  assert(jellyfin_tem("tt0001070", "movie", NULL));
  { char *ix = controle("index");
    assert(strstr(ix, "[0, 0, \"\"]") && strstr(ix, "\"2026-09-01T01:41:00Z\"") && !strstr(ix, "[0, 200, \"\"]"));
    free(ix); }
  // DELETED on the server: the count disagrees, one full rebuild drops it.
  free(controle("delete?n=7"));
  jellyfin_recarregar_bibliotecas();
  fim = ms() + 5000;
  while (jellyfin_tem("tt0001007", "movie", NULL) && ms() < fim) dormir(20);
  assert(!jellyfin_tem("tt0001007", "movie", NULL) && jellyfin_tem("tt0001070", "movie", NULL));
  assert(jellyfin_consultar("tt0900001:2:3", "series", NULL, 0, NULL, NULL, &lista) == 2);   // episodes survived
  free(lista); lista = NULL;

  // Sources: request, poll, ready.
  snprintf(alvo, sizeof alvo, "%s", itens[1].imdb);
  free(controle("reset"));
  assert(jellyfin_fontes_pedir(alvo));
  fim = ms() + 5000;
  while ((r = jellyfin_fontes_colher(alvo, &lista, &n)) == JF_FONTES_PENDENTE && ms() < fim) dormir(20);
  assert(r == JF_FONTES_PRONTO && n == 2 && lista);
  // A source from another server is refused before any request.
  assert(!jellyfin_fontes_pedir("jf:deadbeef.c0ffee001"));
  // Check-ins: nothing before real playback; start; pause; progress cadence; stop.
  jellyfin_reproducao_tick(lista[0].url, 0, 7200, 0);
  jellyfin_reproducao_tick("https://addon.example/x.mp4", 1, 7200, 1);   // unknown URL ignored
  jellyfin_reproducao_tick(lista[0].url, 1, 7200, 1);
  jellyfin_reproducao_tick(lista[0].url, 2, 7200, 1);                    // < 10 s: no progress
  jellyfin_reproducao_tick(lista[0].url, 3, 7200, 0);                    // pause, immediate
  jellyfin_reproducao_tick(lista[1].url, 0, 7200, 1);                    // source switch: stop 0, start 1
  jellyfin_reproducao_fim(lista[0].url, 3, 7200);                        // already stopped
  jellyfin_reproducao_fim(lista[1].url, 5, 7200);                        // stop + transcode release
  jellyfin_reproducao_tick(lista[0].url, 4, 7200, 1);                    // session gone
  fim = ms() + 5000;
  while (jellyfin_relatorios_pendentes() && ms() < fim) dormir(20);
  ev = controle("events");
  assert(conta(ev, "\"Playing\"") == 2 && conta(ev, "\"Progress\"") == 1 && conta(ev, "\"Stopped\"") == 2);
  assert(strstr(ev, "\"pause\"") && strstr(ev, "\"DirectPlay\"") && conta(ev, "\"release\"") == 1);
  // Order: start(0), pause(0), stop(0) at its last position, start(1), stop(1).
  { const char *a = strstr(ev, "\"Stopped\", \"ticks\": 30000000"), *b = strstr(ev, "\"Transcode\"");
    assert(a && b && a < b); }
  assert(strstr(ev, "Nuvio Desktop"));
  free(ev);
  free(lista);

  // CANCELLATION ON PROFILE SWITCH: a hung PlaybackInfo (server sleeps 4 s)
  // must be cut, its late answer dropped, and the new profile isolated.
  free(controle("slow?s=4"));
  assert(jellyfin_fontes_pedir(alvo));
  dormir(300);
  t0 = ms();
  perfilAtivo = 2;
  jellyfin_perfil_trocou();
  assert(jellyfin_estado(NULL, 0) == JF_EST_SEM_SERVIDOR);
  assert(jellyfin_fontes_colher(alvo, &lista, &n) == JF_FONTES_FALHOU && !lista);
  assert(jellyfin_fileiras_copiar(itens, JF_FIL_MAX * JF_POR_FILEIRA, fils, JF_FIL_MAX, &ni) == 0);
  // The control worker is free again well before the 4 s answer.
  assert(jellyfin_definir_servidor(base));
  assert(esperarEstado(JF_EST_SERVIDOR_OK, 3000) == JF_EST_SERVIDOR_OK);
  assert(ms() - t0 < 3000);
  dormir(4200);   // past the late answer: nothing leaked into profile 2
  assert(jellyfin_fontes_colher(alvo, &lista, &n) == JF_FONTES_FALHOU && !lista);
  assert(!jellyfin_conectado());
  assert(!jellyfin_tem("tt0001070", "movie", NULL) && jellyfin_indice_n() == 0);   // profile 1's index stays there

  // Back to profile 1: token AND index from disk; server revokes; 401 -> expired.
  perfilAtivo = 1;
  jellyfin_perfil_trocou();
  assert(jellyfin_conectado());
  assert(jellyfin_tem("tt0001070", "movie", NULL) && jellyfin_tem("tt0900001:2:3", "series", NULL));
  free(controle("revoke"));
  jellyfin_recarregar_bibliotecas();
  assert(esperarEstado(JF_EST_EXPIROU, 5000) == JF_EST_EXPIROU);
  arq = dados_ler("jellyfin-p1.txt");
  assert(arq && strstr(arq, "token=\n") && strstr(arq, "base="));
  free(arq);

  // Quick Connect through the worker, then sign out (server logout queued).
  assert(jellyfin_entrar_quick_connect());
  assert(esperarEstado(JF_EST_CONECTADO, 8000) == JF_EST_CONECTADO);
  free(controle("reset"));
  jellyfin_esquecer();
  assert(jellyfin_estado(NULL, 0) == JF_EST_SEM_SERVIDOR);
  dados_caminho(nome, sizeof nome, "jellyfin-p1.txt");
  assert(stat(nome, &st) != 0);
  dados_caminho(nome, sizeof nome, "srvidx-p1-jf.txt");
  assert(stat(nome, &st) != 0 && !jellyfin_tem("tt0001070", "movie", NULL));
  fim = ms() + 5000;
  for (;;) {
    ev = controle("events");
    if (strstr(ev, "logout") || ms() > fim) break;
    free(ev);
    dormir(50);
  }
  assert(strstr(ev, "logout"));
  free(ev);
  jellyfin_esquecer_todos();
  dados_caminho(nome, sizeof nome, "jellyfin-p2.txt");
  assert(stat(nome, &st) != 0);
  free(itens);
  printf("integration ok\n");
}

int main(int argc, char **argv) {
  assert(argc == 3);
  snprintf(raiz, sizeof raiz, "%s", argv[1]);
  snprintf(base, sizeof base, "%s/jf", argv[1]);
  snprintf(dirDados, sizeof dirDados, "%s", argv[2]);
  setvbuf(stdout, NULL, _IOLBF, 0);
  testarIds();
  testarUrl();
  testarProtocolo();
  testarIntegracao();
  jellyfin_encerrar();
  printf("jellyfin: all ok\n");
  return 0;
}
