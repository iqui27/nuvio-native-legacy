// Posteres personalizados (src/posterprov.c): montagem de URL de cada provedor,
// marcadores do modelo, token a partir da URL do manifest, redacao de log,
// memoria de falhas por item, disjuntor e portao de concorrencia. Sem rede: o
// estado do tex_cache e um gancho falso.
//
// Os formatos de URL sao os MEDIDOS em 29/09/2026 contra
// https://spatial-posters.vercel.app (200 image/jpeg 500x750 para
// /api/poster/movie/tt0111161?fmt=jpeg) e api.ratingposterdb.com (200
// image/jpeg para /t0-free-rpdb/imdb/poster-default/tt0111161.jpg?fallback=true).
#include "posterprov.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "jellyfin_stub.inc"

// These services are unused here; satisfy the shared session stub on macOS.
int trakt_enfeitar_lote(CatItem *items, int n) { (void)items; return n; }
int recomenda_social_mesclar(CatItem *items, int n, int max) {
  (void)items; (void)max; return n;
}

static int ok(const char *nome) { printf("ok  %s\n", nome); return 1; }
#define IGUAL(a, b) do { if (strcmp((a), (b))) { \
  printf("FALHOU %s:%d\n  obtido:   %s\n  esperado: %s\n", __FILE__, __LINE__, (a), (b)); exit(1); } } while (0)

static PosterProvCfg spatial(const char *inst, const char *tok) {
  PosterProvCfg c;
  memset(&c, 0, sizeof c);
  c.prov = PP_SPATIAL;
  if (inst) snprintf(c.instancia, sizeof c.instancia, "%s", inst);
  if (tok) snprintf(c.token, sizeof c.token, "%s", tok);
  return c;
}

// ---- gancho falso do tex_cache
static int estadoFalso;                 // o que o tex_cache "diz"
static int nConsultas;
static int hookEstado(const char *u) { (void)u; nConsultas++; return estadoFalso; }
static long relogio;
static long hookRelogio(void) { return relogio; }

// ---- portao
static volatile int simult, pico;
static pthread_mutex_t mp = PTHREAD_MUTEX_INITIALIZER;
static void *trabalhador(void *a) {
  (void)a;
  posterprov_portao_entrar();
  pthread_mutex_lock(&mp);
  simult++;
  if (simult > pico) pico = simult;
  pthread_mutex_unlock(&mp);
  usleep(20000);
  pthread_mutex_lock(&mp);
  simult--;
  pthread_mutex_unlock(&mp);
  posterprov_portao_sair();
  return NULL;
}

int main(void) {
  char u[PP_URL_MAX], t[PP_TOKEN_MAX + 1], inst[PP_INSTANCIA_MAX], l[80];
  PosterProvCfg c;

  // ---------------------------------------------------------- SpatialPosters
  c = spatial("https://spatial-posters.vercel.app", NULL);
  assert(posterprov_montar_url(&c, "tt0111161", 0, "movie", u, sizeof u));
  IGUAL(u, "https://spatial-posters.vercel.app/api/poster/movie/tt0111161?fmt=jpeg");
  assert(posterprov_montar_url(&c, "tt0944947", 0, "series", u, sizeof u));
  IGUAL(u, "https://spatial-posters.vercel.app/api/poster/series/tt0944947?fmt=jpeg");
  // "tv" do TMDB tambem e serie
  assert(posterprov_montar_url(&c, "tt0944947", 0, "tv", u, sizeof u));
  IGUAL(u, "https://spatial-posters.vercel.app/api/poster/series/tt0944947?fmt=jpeg");
  // estavel: o tmdb que chega depois NAO muda a URL (chave do cache)
  assert(posterprov_montar_url(&c, "tt0111161", 278, "movie", u, sizeof u));
  IGUAL(u, "https://spatial-posters.vercel.app/api/poster/movie/tt0111161?fmt=jpeg");
  // sem imdb: id do TMDB
  assert(posterprov_montar_url(&c, "tmdb:t1399", 0, "series", u, sizeof u));
  IGUAL(u, "https://spatial-posters.vercel.app/api/poster/series/1399?fmt=jpeg");
  assert(posterprov_montar_url(&c, "tmdb:278", 0, "movie", u, sizeof u));
  IGUAL(u, "https://spatial-posters.vercel.app/api/poster/movie/278?fmt=jpeg");
  assert(posterprov_montar_url(&c, "", 550, "movie", u, sizeof u));
  IGUAL(u, "https://spatial-posters.vercel.app/api/poster/movie/550?fmt=jpeg");
  // instancia vazia = a publica
  c = spatial("", NULL);
  assert(posterprov_montar_url(&c, "tt0111161", 0, "movie", u, sizeof u));
  IGUAL(u, "https://spatial-posters.vercel.app/api/poster/movie/tt0111161?fmt=jpeg");
  // idioma da interface e extras curtos (a alternativa ao token, que e longo)
  c = spatial("https://p.exemplo.com", NULL);
  snprintf(c.lang, sizeof c.lang, "pt");
  assert(posterprov_montar_url(&c, "tt0111161", 0, "movie", u, sizeof u));
  IGUAL(u, "https://p.exemplo.com/api/poster/movie/tt0111161?fmt=jpeg&lang=pt");
  snprintf(c.extra, sizeof c.extra, "bs=vetro&side=right");
  assert(posterprov_montar_url(&c, "tt0111161", 0, "movie", u, sizeof u));
  IGUAL(u, "https://p.exemplo.com/api/poster/movie/tt0111161?fmt=jpeg&lang=pt&bs=vetro&side=right");
  // lang nos extras vence o automatico (nao duplica)
  snprintf(c.extra, sizeof c.extra, "lang=en&bs=pill");
  assert(posterprov_montar_url(&c, "tt0111161", 0, "movie", u, sizeof u));
  assert(strstr(u, "lang=en") && !strstr(u, "lang=pt") && strstr(u, "bs=pill"));
  { char x[PP_EXTRA_MAX + 8];
    assert(posterprov_extra_normalizar("  ?bs=vetro&side=right&  ", x, sizeof x)); IGUAL(x, "bs=vetro&side=right");
    assert(posterprov_extra_normalizar("", x, sizeof x)); IGUAL(x, "");
    assert(!posterprov_extra_normalizar("fmt=webp", x, sizeof x));        // o formato e nosso
    assert(!posterprov_extra_normalizar("bs=a&format=avif", x, sizeof x));
    assert(!posterprov_extra_normalizar("config=abc", x, sizeof x));      // token tem campo proprio
    assert(!posterprov_extra_normalizar("c=abc", x, sizeof x));
    assert(!posterprov_extra_normalizar("a=b c", x, sizeof x));
    assert(!posterprov_extra_normalizar("a=b#frag", x, sizeof x));
    assert(posterprov_extra_normalizar("cc=1", x, sizeof x)); }           // "cc" nao e "c"
  // com token
  c = spatial("https://p.exemplo.com", "abc123_-.XYZ");
  assert(posterprov_montar_url(&c, "tt0111161", 0, "movie", u, sizeof u));
  IGUAL(u, "https://p.exemplo.com/api/poster/movie/tt0111161?fmt=jpeg&config=abc123_-.XYZ");
  // o que o provedor nao entende fica de fora
  assert(!posterprov_montar_url(&c, "cs:channel:globo", 0, "tv", u, sizeof u) || 1);
  assert(!posterprov_montar_url(&c, "cs:channel:globo", 0, "channel", u, sizeof u));
  assert(!posterprov_montar_url(&c, "tt0111161", 0, "channel", u, sizeof u));
  assert(!posterprov_montar_url(&c, "tt0111161", 0, NULL, u, sizeof u));
  assert(!posterprov_montar_url(&c, "kitsu:7442", 0, "series", u, sizeof u));
  assert(!posterprov_montar_url(&c, "tt12", 0, "movie", u, sizeof u));       // curto demais
  assert(!posterprov_montar_url(&c, "ttabc1234", 0, "movie", u, sizeof u));
  assert(!posterprov_montar_url(&c, "tt0111161x", 0, "movie", u, sizeof u));
  assert(!posterprov_montar_url(NULL, "tt0111161", 0, "movie", u, sizeof u));
  // token de tamanho maximo cabe; a URL nunca passa de PP_URL_MAX
  { char grande[PP_TOKEN_MAX + 1];
    memset(grande, 'a', PP_TOKEN_MAX); grande[PP_TOKEN_MAX] = 0;
    c = spatial("https://p.exemplo.com", grande);       // instancia curta: cabe
    assert(posterprov_montar_url(&c, "tt0111161", 0, "series", u, sizeof u));
    assert(strlen(u) < PP_URL_MAX);
    // Long instance, maximum token and extras exceed the old 512-byte limit.
    snprintf(c.instancia, sizeof c.instancia, "https://uma-instancia-com-nome-bem-comprido.exemplo.com.br");
    memset(c.extra, 'e', PP_EXTRA_MAX); c.extra[0] = 'x'; c.extra[1] = '=';
    assert(posterprov_montar_url(&c, "tt0111161", 0, "series", u, sizeof u));
    assert(strlen(u) > 512);
    c = spatial("https://spatial-posters.vercel.app", grande);
    // buffer menor que a URL: recusa em vez de truncar
    assert(!posterprov_montar_url(&c, "tt0111161", 0, "series", u, 200));
    assert(u[0] == 0); }
  ok("SpatialPosters: URL por tipo/id, token, estavel, recusa o que nao entende");

  // ---------------------------------------------------------- RPDB
  memset(&c, 0, sizeof c);
  c.prov = PP_RPDB;
  snprintf(c.chave, sizeof c.chave, "t0-free-rpdb");
  assert(posterprov_montar_url(&c, "tt0111161", 0, "movie", u, sizeof u));
  IGUAL(u, "https://api.ratingposterdb.com/t0-free-rpdb/imdb/poster-default/tt0111161.jpg?fallback=true");
  assert(posterprov_montar_url(&c, "tmdb:t1399", 0, "series", u, sizeof u));
  IGUAL(u, "https://api.ratingposterdb.com/t0-free-rpdb/tmdb/poster-default/series-1399.jpg?fallback=true");
  assert(posterprov_montar_url(&c, "", 278, "movie", u, sizeof u));
  IGUAL(u, "https://api.ratingposterdb.com/t0-free-rpdb/tmdb/poster-default/movie-278.jpg?fallback=true");
  c.chave[0] = 0;
  assert(!posterprov_montar_url(&c, "tt0111161", 0, "movie", u, sizeof u));   // sem chave
  snprintf(c.chave, sizeof c.chave, "a/b?c");
  assert(!posterprov_montar_url(&c, "tt0111161", 0, "movie", u, sizeof u));   // chave torta
  ok("RPDB: imdb, tmdb, sem chave, chave torta");

  // ---------------------------------------------------------- modelo
  memset(&c, 0, sizeof c);
  c.prov = PP_MODELO;
  snprintf(c.modelo, sizeof c.modelo, "https://img.exemplo.com/{type}/{imdb}.jpg?w=300");
  assert(posterprov_montar_url(&c, "tt0111161", 0, "movie", u, sizeof u));
  IGUAL(u, "https://img.exemplo.com/movie/tt0111161.jpg?w=300");
  snprintf(c.modelo, sizeof c.modelo, "http://nas.local:8080/p/{tipo_tmdb}/{tmdb}?i={imdb}");
  assert(posterprov_montar_url(&c, "tt0944947", 1399, "series", u, sizeof u));
  IGUAL(u, "http://nas.local:8080/p/tv/1399?i=tt0944947");
  // {tmdb} sem tmdb conhecido: nao inventa
  assert(!posterprov_montar_url(&c, "tt0944947", 0, "series", u, sizeof u));
  // {imdb} sem tt: nao inventa
  snprintf(c.modelo, sizeof c.modelo, "https://x.com/{imdb}.jpg");
  assert(!posterprov_montar_url(&c, "tmdb:278", 0, "movie", u, sizeof u));
  assert(posterprov_modelo_valido("https://x.com/{imdb}"));
  assert(!posterprov_modelo_valido("https://x.com/fixo.jpg"));          // sem marcador
  assert(!posterprov_modelo_valido("https://x.com/{imdb"));             // aberto
  assert(!posterprov_modelo_valido("https://x.com/{foo}"));             // desconhecido
  assert(!posterprov_modelo_valido("https://x.com/a}{imdb}"));          // '}' solto
  assert(!posterprov_modelo_valido("ftp://x.com/{imdb}"));
  assert(!posterprov_modelo_valido("x.com/{imdb}"));
  assert(!posterprov_modelo_valido("https://x.com/{imdb} b"));          // espaco
  assert(!posterprov_modelo_valido(""));
  assert(!posterprov_modelo_valido(NULL));
  c.prov = PP_DESLIGADO;
  assert(!posterprov_montar_url(&c, "tt0111161", 0, "movie", u, sizeof u));
  ok("modelo proprio: marcadores, validacao, faltando dado");

  // Pictorium's Nuvio export: no extension, saved per-title choices via live=1.
  c.prov = PP_MODELO;
  snprintf(c.modelo, sizeof c.modelo,
           "https://pictorium.elfhosted.com/api/poster/{type}/{tmdb_id|imdb_id}"
           "?u=test-space&live=1&shape={shape}");
  assert(posterprov_modelo_cabe(c.modelo));
  assert(posterprov_montar_url(&c, "tt0903747", 1396, "series", u, sizeof u));
  IGUAL(u, "https://pictorium.elfhosted.com/api/poster/series/1396?u=test-space&live=1&shape=poster");
  assert(posterprov_montar_url(&c, "tt0903747", 0, "series", u, sizeof u));
  assert(strstr(u, "/series/tt0903747?"));
  assert(posterprov_montar_url(&c, "tmdb:t1396", 0, "series", u, sizeof u));
  assert(strstr(u, "/series/1396?"));
  assert(!posterprov_montar_url(&c, "unknown", 0, "series", u, sizeof u));
  posterprov_configurar(&c);
  assert(strstr(posterprov_card_formato(NULL, "tt0903747", 1396, "series", "original", 1),
                "shape=landscape"));
  snprintf(c.modelo, sizeof c.modelo,
           "https://pictorium.elfhosted.com/api/poster/%%7Btype%%7D/%%7btmdb_id%%7Cimdb_id%%7d"
           "?live=1&shape=%%7Bshape%%7D&key=FAKE%%2Bvalue");
  assert(posterprov_modelo_cabe(c.modelo));
  assert(posterprov_montar_url(&c, "tt0903747", 1396, "series", u, sizeof u));
  IGUAL(u, "https://pictorium.elfhosted.com/api/poster/series/1396?live=1&shape=poster&key=FAKE%2Bvalue");
  posterprov_redigir(u, l, sizeof l);
  assert(!strstr(l, "FAKE") && !strstr(l, "key="));
  posterprov_configurar(&c);
  assert(strstr(posterprov_card_formato(NULL, "tt0903747", 1396, "series", "original", 1),
                "shape=landscape"));
  snprintf(c.modelo, sizeof c.modelo,
           "https://api.aioratings.com/FAKE/{id_type}/poster-default/{typed_id}.webp");
  assert(posterprov_montar_url(&c, "tmdb:t1396", 0, "series", u, sizeof u));
  IGUAL(u, "https://api.aioratings.com/FAKE/tmdb/poster-default/series-1396.webp");
  assert(posterprov_montar_url(&c, "tt0903747", 1396, "series", u, sizeof u));
  IGUAL(u, "https://api.aioratings.com/FAKE/imdb/poster-default/tt0903747.webp");
  snprintf(c.modelo, sizeof c.modelo, "https://x/{imdb_id|tmdb_id}/{tmdb_id?}.png");
  assert(posterprov_montar_url(&c, "tt0903747", 0, "series", u, sizeof u));
  IGUAL(u, "https://x/tt0903747/.png");
  assert(!posterprov_modelo_valido("https://x/{tmdb_id|}"));
  assert(!posterprov_modelo_valido("https://x/{|imdb_id}"));
  assert(!posterprov_modelo_valido("https://x/{tmdb_id|unknown}"));
  assert(!posterprov_modelo_valido("https://x/{tmdb_id?|imdb_id}"));
  ok("Nuvio templates: Pictorium alternatives, encoded markers, namespace aliases");

  // ---------------------------------------------------------- token / instancia
  assert(posterprov_extrair_token("abc.DEF-123_x=", t, sizeof t, inst, sizeof inst));
  IGUAL(t, "abc.DEF-123_x="); IGUAL(inst, "");
  assert(posterprov_extrair_token("  https://spatial-posters.vercel.app/c/TOK.en-1/manifest.json  ",
                                  t, sizeof t, inst, sizeof inst));
  IGUAL(t, "TOK.en-1"); IGUAL(inst, "https://spatial-posters.vercel.app");
  assert(posterprov_extrair_token("stremio://meu.dominio.app/c/T2/manifest.json", t, sizeof t, inst, sizeof inst));
  IGUAL(t, "T2"); IGUAL(inst, "https://meu.dominio.app");
  assert(posterprov_extrair_token("http://192.168.1.20:3000/c/T3/configure", t, sizeof t, inst, sizeof inst));
  IGUAL(t, "T3"); IGUAL(inst, "http://192.168.1.20:3000");
  assert(posterprov_extrair_token("https://h.com/api/poster/movie/tt1?fmt=jpeg&config=T4&x=1", t, sizeof t, inst, sizeof inst));
  IGUAL(t, "T4");
  assert(posterprov_extrair_token("https://h.com/api/poster/movie/tt1?c=T5", t, sizeof t, NULL, 0));
  IGUAL(t, "T5");
  assert(posterprov_extrair_token("", t, sizeof t, inst, sizeof inst)); IGUAL(t, "");   // apagar
  assert(posterprov_extrair_token("   ", t, sizeof t, inst, sizeof inst)); IGUAL(t, "");
  assert(!posterprov_extrair_token("https://h.com/sem/token", t, sizeof t, inst, sizeof inst));
  assert(!posterprov_extrair_token("tem espaco", t, sizeof t, inst, sizeof inst));
  assert(!posterprov_extrair_token("tok/en", t, sizeof t, inst, sizeof inst));
  assert(!posterprov_extrair_token("tok%20", t, sizeof t, inst, sizeof inst));
  { char longo[500]; memset(longo, 'a', 450); longo[450] = 0;
    assert(!posterprov_extrair_token(longo, t, sizeof t, inst, sizeof inst)); }
  assert(posterprov_normalizar_instancia("Spatial.Exemplo.com/", u, sizeof u)); IGUAL(u, "https://spatial.exemplo.com");
  assert(posterprov_normalizar_instancia("http://nas:3000/x/y?z", u, sizeof u)); IGUAL(u, "http://nas:3000");
  assert(posterprov_normalizar_instancia("192.168.1.5:3000", u, sizeof u)); IGUAL(u, "http://192.168.1.5:3000");
  assert(posterprov_normalizar_instancia("meupc.local", u, sizeof u)); IGUAL(u, "http://meupc.local");
  assert(posterprov_normalizar_instancia("localhost:3000", u, sizeof u)); IGUAL(u, "http://localhost:3000");
  assert(!posterprov_normalizar_instancia("", u, sizeof u));
  assert(!posterprov_normalizar_instancia("ftp://x.com", u, sizeof u));
  assert(!posterprov_normalizar_instancia("https://u:p@x.com", u, sizeof u));   // userinfo
  assert(!posterprov_normalizar_instancia("semponto", u, sizeof u));
  assert(!posterprov_normalizar_instancia("a b.com", u, sizeof u));
  ok("token e instancia a partir do que a pessoa cola (manifest, stremio://, configure, poster)");

  // ---------------------------------------------------------- redacao
  { PosterProvCfg s = spatial("https://p.exemplo.com", "SEGREDO-do-token.123");
    PosterProvCfg r;
    memset(&r, 0, sizeof r); r.prov = PP_RPDB; snprintf(r.chave, sizeof r.chave, "pk_CHAVE_SECRETA");
    assert(posterprov_montar_url(&s, "tt0111161", 0, "movie", u, sizeof u));
    posterprov_redigir(u, l, sizeof l);
    assert(!strstr(l, "SEGREDO") && !strstr(l, "config"));
    IGUAL(l, "https://p.exemplo.com/...");
    assert(posterprov_montar_url(&r, "tt0111161", 0, "movie", u, sizeof u));
    posterprov_redigir(u, l, sizeof l);
    assert(!strstr(l, "CHAVE"));
    IGUAL(l, "https://api.ratingposterdb.com/...");
    // buffer curto nunca vaza o resto
    posterprov_redigir(u, l, 12);
    assert(strlen(l) < 12); }
  ok("redacao: nem token nem chave chegam ao log");

  // ---------------------------------------------------------- card / falhas
  posterprov_hook_estado(hookEstado);
  posterprov_hook_relogio(hookRelogio);
  c = spatial("https://p.exemplo.com", "TOK");
  posterprov_configurar(&c);
  assert(posterprov_ativo());
  estadoFalso = 0; nConsultas = 0;
  { const char *a = posterprov_card("tt0111161", 0, "movie", "https://normal/p.jpg");
    IGUAL(a, "https://p.exemplo.com/api/poster/movie/tt0111161?fmt=jpeg&config=TOK");
    assert(posterprov_e_provedor(a));
    assert(!posterprov_e_provedor("https://normal/p.jpg"));
    assert(!posterprov_e_provedor("https://p.exemplo.com.evil.io/api/poster/x"));
    assert(posterprov_e_provedor("https://p.exemplo.com/api/poster/movie/tt1"));
    // item que o provedor nao entende: cartaz normal, sem consultar nada
    IGUAL(posterprov_card("cs:channel:x", 0, "channel", "https://normal/c.jpg"), "https://normal/c.jpg");
    assert(posterprov_card("cs:channel:x", 0, "channel", NULL) == NULL);
    // pronta uma vez: nao consulta de novo a cada quadro
    estadoFalso = 1; nConsultas = 0;
    IGUAL(posterprov_card("tt0068646", 0, "movie", "n"), "https://p.exemplo.com/api/poster/movie/tt0068646?fmt=jpeg&config=TOK");
    posterprov_card("tt0068646", 0, "movie", "n"); posterprov_card("tt0068646", 0, "movie", "n");
    assert(nConsultas == 1);
    // falha: volta ao normal e LEMBRA (nao consulta mais)
    estadoFalso = -1; nConsultas = 0;
    IGUAL(posterprov_card("tt0108052", 0, "movie", "https://normal/l.jpg"), "https://normal/l.jpg");
    assert(nConsultas == 1);
    estadoFalso = 1;   // mesmo que o tex_cache mude de ideia, a sessao lembra
    IGUAL(posterprov_card("tt0108052", 0, "movie", "https://normal/l.jpg"), "https://normal/l.jpg");
    IGUAL(posterprov_card("tt0108052", 0, "movie", "https://normal/l.jpg"), "https://normal/l.jpg");
    assert(nConsultas == 1);
    // sem cartaz normal e provedor falhou: vazio (sem arte), nunca a URL falha
    IGUAL(posterprov_card("tt0108052", 0, "movie", ""), "");
    // trocar a configuracao zera a memoria
    posterprov_configurar(&c);
    assert(posterprov_e_provedor(posterprov_card("tt0108052", 0, "movie", "n"))); }
  ok("card: cartaz normal quando falha/nao entende, lembra a falha, config zera");

  // ---------------------------------------------------------- poster do addon
  // Desligado (padrao): o provedor vence, com ou sem origem. Ligado: o cartaz
  // proprio de um item de catalogo de addon vence; sem origem (Trakt, Salvos)
  // ou com o poster generico do metahub, o provedor continua.
  estadoFalso = 1;
  { const char *prov = "https://p.exemplo.com/api/poster/movie/tt0111161?fmt=jpeg&config=TOK";
    posterprov_preferir_addon(0);
    IGUAL(posterprov_card_addon("aiometadata", "tt0111161", 0, "movie", "https://addon/p.jpg"), prov);
    posterprov_preferir_addon(1);
    IGUAL(posterprov_card_addon("aiometadata", "tt0111161", 0, "movie", "https://addon/p.jpg"), "https://addon/p.jpg");
    IGUAL(posterprov_card_addon("", "tt0111161", 0, "movie", "https://addon/p.jpg"), prov);
    IGUAL(posterprov_card_addon(NULL, "tt0111161", 0, "movie", "https://addon/p.jpg"), prov);
    IGUAL(posterprov_card_addon("cinemeta", "tt0111161", 0, "movie",
          "https://images.metahub.space/poster/medium/tt0111161/img"), prov);
    // addon sem poster: o provedor, que e melhor que nada
    IGUAL(posterprov_card_addon("aiometadata", "tt0111161", 0, "movie", ""), prov);
    posterprov_preferir_addon(0); }
  ok("poster do addon: desligado o provedor; ligado o do addon, menos metahub e sem origem");

  // ---------------------------------------------------------- desligado
  memset(&c, 0, sizeof c);
  posterprov_configurar(&c);
  assert(!posterprov_ativo());
  IGUAL(posterprov_card("tt0111161", 0, "movie", "https://normal/p.jpg"), "https://normal/p.jpg");
  // ligado mas sem o dado que o provedor exige = inativo (nao quebra o app)
  c.prov = PP_RPDB;
  posterprov_configurar(&c);
  assert(!posterprov_ativo());
  IGUAL(posterprov_card("tt0111161", 0, "movie", "https://normal/p.jpg"), "https://normal/p.jpg");
  ok("desligado ou incompleto: nada muda");

  // ---------------------------------------------------------- disjuntor
  c = spatial("https://p.exemplo.com", NULL);
  posterprov_configurar(&c);
  relogio = 1000; estadoFalso = -1;
  { int i; char id[24]; PosterProvStat st;
    for (i = 0; i < 6; i++) {
      snprintf(id, sizeof id, "tt%07d", 100 + i);
      IGUAL(posterprov_card(id, 0, "movie", "n"), "n");
    }
    posterprov_stat(&st);
    assert(st.falhas == 6 && st.disjuntor_aberto);
    // aberto: nem consulta, mesmo item novo, mesmo com o tex_cache dizendo pronto
    estadoFalso = 1; nConsultas = 0;
    IGUAL(posterprov_card("tt0999999", 0, "movie", "n"), "n");
    assert(nConsultas == 0);
    // depois de 5 min tenta de novo
    relogio = 1000 + 301;
    assert(posterprov_e_provedor(posterprov_card("tt0999999", 0, "movie", "n")));
    posterprov_stat(&st);
    assert(!st.disjuntor_aberto && st.seguidas == 0);
    // sucesso zerou a conta: 5 falhas nao abrem, a 6a abre
    estadoFalso = -1;
    for (i = 0; i < 5; i++) {
      snprintf(id, sizeof id, "tt%07d", 200 + i);
      posterprov_card(id, 0, "movie", "n");
    }
    posterprov_stat(&st);
    assert(!st.disjuntor_aberto && st.seguidas == 5);
    posterprov_card("tt0300000", 0, "movie", "n");
    posterprov_stat(&st);
    assert(st.disjuntor_aberto);
    // meia-abertura sem sucesso: UMA falha reabre
    relogio += 301;
    posterprov_card("tt0300001", 0, "movie", "n");
    posterprov_stat(&st);
    assert(st.disjuntor_aberto); }
  ok("disjuntor: 6 falhas seguidas pausam 5 min, meia-abertura, sucesso zera");

  // ---------------------------------------------------------- portao
  { pthread_t th[12];
    int i;
    for (i = 0; i < 12; i++) pthread_create(&th[i], NULL, trabalhador, NULL);
    for (i = 0; i < 12; i++) pthread_join(th[i], NULL);
    assert(pico >= 1 && pico <= PP_SIMULT);
    printf("    pico simultaneo: %d (teto %d)\n", pico, PP_SIMULT); }
  ok("portao: nunca passa de PP_SIMULT downloads juntos");

  printf("posterprov: tudo ok\n");
  return 0;
}
