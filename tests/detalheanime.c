// DETALHE DE ANIME: TIPO INCERTO NAO VIRA FILME (relato do Mizikashi1, v1.4.2).
//
// O log 2043 (LG, v1.4.2) mostrava, ao abrir Mushoku Tensei (tt13293588):
//   [desc] Mushoku Tensei: Jobless Reincarnation: 3 atores, dir='Miklós Jancsó', 0 temporadas
// O item vinha de um catalogo do AIOMetadata com `type: "anime"`; buscarEps
// fazia `ehFilme = tipo != "series"` e pedia /meta/movie/tt13293588 ao
// Cinemeta, que responde com OUTRO titulo ("My Way Home", 1965, dir. Miklos
// Jancso, 3 atores, 0 videos). A chave do cache de /meta era so o id, entao a
// abertura seguinte, ja como serie, lia o corpo do filme ("meta do cache",
// 0 episodios).
//
// O QUE ESTE TESTE PROVA (fixtures reduzidas das respostas reais do Cinemeta):
//   1. item de catalogo "anime" cujo meta diz "series" entra como serie;
//   2. item "anime" sem `type` proprio: buscarEps pergunta /meta/series
//      primeiro, publica episodios e grava tipo "series" no item, sem o
//      diretor do filme homonimo, e zera um id TMDB sem tipo provado;
//   3. o cache de /meta separa filme de serie do mesmo id;
//   4. filme de verdade continua um pedido so, /meta/movie;
//   5. id que nao e do IMDb (kitsu:) nao vai ao Cinemeta.
//
//   bash tests/detalheanime.sh
int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_busca_cinemeta(void) { return 1; }
int ajustes_busca_nuvio(void) { return 0; }   // #311: Primeiro (padrao)
int ajustes_ocultar_nao_lancados(void) { return 0; }   // #369: descoberta.c le o ajuste
#include "../src/descoberta.c"
Uint32 SDL_GetTicks(void) { return 0; }
#include "../src/progresso.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include "jellyfin_stub.inc"

// --- DUBLES: nenhum participa da regra, so fazem descoberta.c linkar ---------
// (o conjunto e o de tests/colfileiras.c, menos os cat_* — que catalogo.c ja
// traz — e mais os que este teste ja tinha)
int         ajustes_idioma_ingles(void) { return 0; }
int ajustes_idioma(void) { return 0; }
int   ajustes_cw_concluido(void)         { return 90; }  // factory "watched" threshold
unsigned recomenda_geracao(void)         { return 0; }   // social generation: no social layer here
int   ajustes_cw_ordem(void)               { return 0; }   // Padrao (issue #127)
int   ajustes_cw_mostrar_nao_exibidos(void) { return 1; }
int   ajustes_cw_proximo(void) { return 1; }
const char *i18n(const char *s)         { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
static const char *fakeDados = "";
const char *dados_dir(void)             { return fakeDados; }
const char *sessao_usuario(void)        { return ""; }
static int fakePerfil = 1;
int         perfis_ativo(void)          { return fakePerfil; }
int   ajustes_itens_fileira(void)          { return 12; }   // padrao (#163)
unsigned homeestado_geracao(void) { return 1; }
int homeestado_contexto_valido(void) { return 0; }
int homeestado_tem_fileira(const char *chave) { (void)chave; return 0; }
int homeestado_ordem_fileira(const char *chave) { (void)chave; return -1; }
int homeestado_salvar_se_geracao(const CatFileira *f, int n, unsigned g) {
  (void)f; (void)n; return g == 1;
}
int homeestado_identidade_geracao(unsigned g, char *d, unsigned z, int *p) {
  if (g != 1) return 0;
  if (d && z) d[0] = 0;
  if (p) *p = 1;
  return 1;
}
// Contexto em partes (homeestado.h, 1.4.5): constante aqui, entao nada muda
// no meio da montagem e o fim dela segue o caminho de sempre.
void homeestado_contexto(HomeContexto *c) { *c = (HomeContexto){0}; c->perfil = 1; }
int homeestado_mudancas(const HomeContexto *a, const HomeContexto *b) { (void)a; (void)b; return 0; }
const char *homeestado_mudancas_texto(int m, char *b, unsigned t) { (void)m; if (b && t) b[0] = 0; return b; }
int prog_ler(ProgRegistro *saida, int max) { (void)saida; (void)max; return 0; }
int prog_gravar_local(const char *imdb, int t, int e, double p, double d) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; return 0;
}

void  SDL_Delay(Uint32 ms)                 { usleep(ms * 1000); }
int   ajustes_cw_fonte(void)               { return 0; }
int   ajustes_tmdb_ligado(void)            { return 1; }
static int fakeMetaExterno, fakeTmdbBasico;
int   ajustes_meta_externo(void)           { return fakeMetaExterno; }
static int fakeSoCinemeta;
int   ajustes_meta_so_cinemeta(void)        { return fakeSoCinemeta; }
int   ajustes_fundo_addon(void)            { return 0; }
int   ajustes_logo_addon(void)             { return 0; }
int   ajustes_tmdb_basico(void)            { return fakeTmdbBasico; }
static int fakeTmdbArte;
int   ajustes_tmdb_arte(void)              { return fakeTmdbArte; }
int   ajustes_tmdb_elenco(void)            { return 0; }
int   ajustes_tmdb_cw(void)                { return 0; }
static const char *fakeIdioma = "pt-BR";
const char *ajustes_tmdb_idioma(void)      { return fakeIdioma; }
const char *ajustes_tmdb_chave(void)       { return ""; }
void  fil_gravar_registro(void)            { }
int   fil_lista_e_deste_perfil(int p)       { (void)p; return 1; }
unsigned fil_perfil_geracao(void)          { return 0; }
FilPassada fil_passada_ler(void)          { FilPassada p = {0, 0}; return p; }
int   fil_passada_valida(const FilPassada *p) { (void)p; return 1; }
void  fil_registrar_de(const FilPassada *p, const char *c, const char *t, const char *a, const char *k, int n) { (void)p; (void)c; (void)t; (void)a; (void)k; (void)n; }
void  fil_registrar_se_couber_de(const FilPassada *p, const char *c, const char *t, const char *a, const char *k) { (void)p; (void)c; (void)t; (void)a; (void)k; }
int   fil_podar_catalogos(const char *const *ids, const char *const *bases, int n,
                          int perfilDaLista) {
  (void)ids; (void)bases; (void)n; (void)perfilDaLista; return 0; }
int fil_marcar_sem_addon(const char *const *ids, const char *const *bases, const int *ativos, int n, int perfilDaLista) {
  (void)ids; (void)bases; (void)ativos; (void)n; (void)perfilDaLista; return 0;
}
int   fil_addon_novo(const char *id, const char *base) { (void)id; (void)base; return 0; }
int   addons_perfil_da_lista(void)         { return 0; }
int addons_base_desligada(const char *b) { (void)b; return 0; }   // 203-desligados
int   addons_ativo(int i)                  { (void)i; return 1; }
int   fil_limite(void)                     { return 16; }
int   fil_oculta(const char *c)            { (void)c; return 0; }
int   fil_adicionada_na_tv(const char *c) { (void)c; return 0; }
int   fil_estado_chave(const char *c) { (void)c; return -1; }
const char *fil_hero_fonte(void) { return ""; }
// Dubles da escolha da cota (#126): nada escolhido na TV, e o registro dos
// catalogos fora da cota nao interessa a este teste.
int fil_escolhida(const char *c) { (void)c; return -1; }
int fil_migrar_197(const char *const *c, int n) { (void)c; (void)n; return 0; }
void fil_registrar_se_couber(const char *c, const char *t, const char *a,
                             const char *tp) { (void)c; (void)t; (void)a; (void)tp; }
void  fil_registrar(const char *c, const char *t, const char *a,
                    const char *tp, int itens) {
  (void)c; (void)t; (void)a; (void)tp; (void)itens;
}
int   fil_tem_ordem(void)                  { return 0; }
int   fil_unir(const char *const *c, int n, int *s, int m) {
  int i; (void)c; for (i = 0; i < n && i < m; i++) s[i] = i; return i;
}
void  marco(const char *n)                 { (void)n; }
void  prog_chave(char *d, unsigned n, const char *c, int t, int e) {
  (void)c; (void)t; (void)e; if (n) d[0] = 0;
}
void  prog_content_id(char *d, unsigned n, const char *i, int *t, int *e) {
  (void)i; (void)t; (void)e; if (n) d[0] = 0;
}
int   prog_por_chave(const char *c, ProgRegistro *s) { (void)c; (void)s; return 0; }
// "Tirar de Continuar assistindo" (desc_tirar_continuar, tests/cwremover.sh).
void  prog_remover(const char *c)          { (void)c; }
void  prog_marcar_removido(const char *i)  { (void)i; }
int   prog_removido_vence(const char *i, long long ms) { (void)i; (void)ms; return 0; }
int   trakt_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   trakt_continuar_falhou(void)        { return 0; }
// Simkl (issue #110): sem vinculo nos testes de fileira, como o Trakt acima.
int   simkl_ativo(void)                    { return 0; }

// "A seguir" da conta (#199): so para linkar; sem vistos, nada semeia.
int   trakt_ativo(void)                    { return 1; }
int   ajustes_cw_do_episodio_mais_alto(void) { return 1; }
int   contalib_sementes_a_seguir(ContaSemente *s, int m, int a) { (void)s; (void)m; (void)a; return 0; }
int   simkl_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   simkl_e_a_seguir(const char *id)     { (void)id; return 0; }
int   simkl_plantowatch(CatItem *s, int m) { (void)s; (void)m; return 0; }
int   ajustes_salvos_no_simkl(void)        { return 0; }
int   trakt_enfeitar_lote(CatItem *s, int n) { (void)s; (void)n; return 0; }
int   trakt_lista(const char *q, CatItem *s, int m) { (void)q; (void)s; (void)m; return 0; }
int   trakt_lista_cresc(const char *q, CatItem **s, int m) { (void)q; (void)s; (void)m; return 0; }
// O servico social proprio (recomenda.c) fica fora deste teste: a uniao e so o que o Trakt trouxe.
int   recomenda_social_mesclar(CatItem *i, int nTrakt, int max) { (void)i; (void)max; return nTrakt; }
int   trakt_social(CatItem *s, int m)      { (void)s; (void)m; return 0; }
const char *nuvem_trakt_cliente(void)      { return ""; }
// Um addon de metadados, ligado so no caso 7 (o resto do teste roda sem addon).
static int addonMeta;
// Varios addons de metadados para o catalogo primeiro (caso 20 em diante). Com
// nFake > 0 mandam eles; com 0, vale o addon unico de antes.
//   pref  : o idPrefix que o "manifesto" declara ("" = nao declara -> -1)
//   nega  : 1 = o manifesto declara que este tipo/prefixo NAO e dele (-> 0)
typedef struct { const char *nome, *base, *id, *pref; int nega; } FakeAddon;
static FakeAddon fake[4];
static int nFake;
int   addons_n(void)                       { return nFake ? nFake : (addonMeta ? 1 : 0); }
int   addons_sondado(int i)                { (void)i; return 1; }
int   addons_fornece(int i, int oque)      { (void)i; return oque == ADD_META; }
const char *addons_base(int i)             { return nFake ? fake[i].base : (addonMeta ? "https://addon.test/SEGREDO" : ""); }
const char *addons_id_manifesto(int i)     { return nFake ? fake[i].id : ""; }
const char *addons_nome(int i)            { return nFake ? fake[i].nome : "addon"; }
int   addons_aceita_id(int i, const char *t, const char *id) {
  (void)t;
  if (!nFake) return -1;
  if (fake[i].nega) return 0;
  if (!fake[i].pref[0]) return -1;
  return !strncmp(id, fake[i].pref, strlen(fake[i].pref)) ? 1 : 0;
}
unsigned addons_versao(void)             { return 1; }   // estatico no teste
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
void  addons_manifesto_lido(int i, const char *corpo) { (void)i; (void)corpo; }

// --- REDE FALSA: respostas do Cinemeta, reduzidas --------------------------
static const char *META_FILME_ERRADO =
  "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"movie\",\"name\":\"My Way Home\","
  "\"director\":[\"Mikl\xc3\xb3s Jancs\xc3\xb3\"],"
  "\"cast\":[\"Andr\xc3\xa1s Koz\xc3\xa1k\",\"Sergey Nikonenko\",\"B\xc3\xa9la Barsi\"],"
  "\"moviedb_id\":94664,\"videos\":[]}}";
static const char *META_SERIE =
  "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
  "\"name\":\"Mushoku Tensei: Jobless Reincarnation\",\"director\":[],"
  "\"cast\":[\"Yumi Uchiyama\",\"Tomokazu Sugita\",\"Ai Kayano\"],"
  "\"moviedb_id\":94664,\"videos\":["
  "{\"id\":\"tt13293588:1:1\",\"season\":1,\"episode\":1,\"name\":\"Jobless Reincarnation\"},"
  "{\"id\":\"tt13293588:1:2\",\"season\":1,\"episode\":2,\"name\":\"Getting Ahead of Myself\"},"
  "{\"id\":\"tt13293588:2:1\",\"season\":2,\"episode\":1,\"name\":\"The Brokenhearted Mage\"}]}}";
// O addon do usuario conhece 5 episodios da mesma serie; o Cinemeta so 3.
static const char *META_SERIE_ADDON =
  "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
  "\"name\":\"Mushoku Tensei\",\"videos\":["
  "{\"season\":1,\"episode\":1,\"name\":\"A\"},{\"season\":1,\"episode\":2,\"name\":\"B\"},"
  "{\"season\":1,\"episode\":3,\"name\":\"C\"},{\"season\":2,\"episode\":1,\"name\":\"D\"},"
  "{\"season\":2,\"episode\":2,\"name\":\"E\"}]}}";
static const char *META_FILME_OK =
  "{\"meta\":{\"id\":\"tt0111161\",\"type\":\"movie\",\"name\":\"The Shawshank Redemption\","
  "\"director\":[\"Frank Darabont\"],\"cast\":[\"Tim Robbins\"],\"videos\":[]}}";

// Meta completo, como o Cinemeta devolve (#176): descricao, fundo, logo, ano.
static const char *META_COMPLETO =
  "{\"meta\":{\"id\":\"tt0000176\",\"type\":\"movie\",\"name\":\"Filme Salvo\","
  "\"poster\":\"https://p.test/poster.jpg\",\"background\":\"https://p.test/fundo.jpg\","
  "\"logo\":\"https://p.test/logo.png\",\"description\":\"Sinopse de verdade.\","
  "\"releaseInfo\":\"2019\",\"runtime\":\"2h 1min\",\"moviedb_id\":4242,"
  "\"imdbRating\":\"8.1\",\"genres\":[\"Drama\"],\"cast\":[\"Ator Um\"],\"videos\":[]}}";

// Filme SEM elenco no Cinemeta, com sinopse em ingles: o caso em que
// fotosDoElenco saia cedo e a sinopse ficava em ingles (fix/detalhe-traducao).
static const char *META_SEM_ELENCO =
  "{\"meta\":{\"id\":\"tt0000178\",\"type\":\"movie\",\"name\":\"Movie Without Cast\","
  "\"poster\":\"https://p.test/sem-elenco.jpg\",\"description\":\"English plot.\",\"genres\":[\"Action\",\"Sci-Fi\"],\"cast\":[],\"videos\":[]}}";

// O que os addons/TMDB falsos respondem; cada caso liga o seu.
static const char *addonResp, *addonTipo = "/series/";
static const char *tmdbFind, *tmdbTemp1, *tmdbFilme, *cineSerie;
static char pedidos[64][600];
static int nPedidos;

// Rotas do catalogo primeiro: o primeiro trecho que casa responde (resp NULL =
// falha de rede). Vazio, tudo cai nas respostas fixas de antes.
static struct { const char *trecho, *resp; } rotas[8];
static int nRotas;
static void rota(const char *trecho, const char *resp) {
  rotas[nRotas].trecho = trecho; rotas[nRotas].resp = resp; nRotas++;
}

static void (*caudaB)(void);
char *rede_baixar(const char *u, int t) {
  int r;
  (void)t;
  if (nPedidos < 64) snprintf(pedidos[nPedidos], sizeof pedidos[0], "%s", u);
  nPedidos++;
  for (r = 0; r < nRotas; r++)
    if (strstr(u, rotas[r].trecho)) return rotas[r].resp ? strdup(rotas[r].resp) : NULL;
  if (strstr(u, "cinemeta")) {
    if (strstr(u, "/meta/movie/tt13293588.json"))  return strdup(META_FILME_ERRADO);
    if (strstr(u, "/meta/series/tt13293588.json")) return strdup(cineSerie ? cineSerie : META_SERIE);
    if (strstr(u, "/meta/movie/tt0111161.json"))   return strdup(META_FILME_OK);
    if (strstr(u, "/meta/movie/tt0000176.json"))   return strdup(META_COMPLETO);
    if (strstr(u, "/meta/movie/tt0000178.json"))   return strdup(META_SEM_ELENCO);
  }
  // O addon de metadados responde o que o teste pos em `addonResp`, no tipo que
  // o teste pos em `addonTipo` (o de serie e o padrao dos casos antigos).
  if (strstr(u, "addon.test/SEGREDO/meta/") && addonResp &&
      strstr(u, addonTipo)) return strdup(addonResp);
  // TMDB (#176): so o que o teste liga em `tmdbResp*`.
  // Caso 31: a cauda de enriquecimento do fio A pede o /find; nesse meio o
  // fio B (mesmo titulo, reaberto) publica 72 episodios em 3 temporadas.
  if (strstr(u, "themoviedb.org/3/find/tt0000373") && caudaB) caudaB();
  if (strstr(u, "themoviedb.org/3/find/")) return tmdbFind ? strdup(tmdbFind) : NULL;
  if (strstr(u, "themoviedb.org/3/tv/555/season/1?") && tmdbTemp1) return strdup(tmdbTemp1);
  if (strstr(u, "themoviedb.org/3/tv/555/season/")) return strdup("{\"episodes\":[]}");
  if (strstr(u, "themoviedb.org/3/movie/278?") && tmdbFilme) return strdup(tmdbFilme);
  return NULL;   // outros addons: fora do teste
}
char *rede_baixar_com(const char *u, int t, const char *const *c) {
  (void)c; return rede_baixar(u, t); }
int arte_reserva_registrar(const char *url, const char *imdb, int poster) {
  (void)url; (void)imdb; (void)poster; return 1; }

static int pediu(const char *trecho) {
  int i;
  for (i = 0; i < nPedidos && i < 64; i++) if (strstr(pedidos[i], trecho)) return 1;
  return 0;
}
int arte_reserva_episodios(const char *imdb, const char *corpo) { (void)imdb; (void)corpo; return 0; }

static void limparCacheMeta(void) {
  int i;
  for (i = 0; i < META_CACHE_N; i++) { free(metaCache[i].corpo); metaCache[i].corpo = NULL; }
  metaNegLimpar();
}

// /meta de serie com `nt` temporadas de cont[t] episodios (ids "<tt>:T:E"),
// no formato que o Nuvio e o Cinemeta mandam, so com os campos que o parser le.
static void corpoSerie(char *b, size_t n, const char *tt, const int *cont, int nt) {
  size_t k = (size_t)snprintf(b, n, "{\"meta\":{\"id\":\"%s\",\"type\":\"series\","
                              "\"name\":\"Serie\",\"videos\":[", tt);
  int t, e, prim = 1;
  for (t = 0; t < nt; t++)
    for (e = 1; e <= cont[t] && k < n; e++, prim = 0)
      k += (size_t)snprintf(b + k, n - k, "%s{\"id\":\"%s:%d:%d\",\"season\":%d,\"episode\":%d,"
                            "\"name\":\"E%d\"}", prim ? "" : ",", tt, t + 1, e, t + 1, e, e);
  if (k < n) snprintf(b + k, n - k, "]}}");
  assert(k + 4 < n);
}

// O fio B do caso 31: publica a lista e as abas T1-T3 do mesmo titulo, como
// buscarEps faria (cat_definir_episodios e cat_atualizar_item).
static void publicaB(void) {
  static CatEp eps[72];
  CatItem it;
  int i;
  for (i = 0; i < 72; i++) {
    memset(&eps[i], 0, sizeof eps[i]);
    eps[i].temporada = i / 24 + 1; eps[i].episodio = i % 24 + 1;
  }
  cat_definir_episodios(0, eps, 72);
  assert(cat_copiar_item(0, &it));
  it.nTemporadas = 3; it.temporadas[0] = 1; it.temporadas[1] = 2; it.temporadas[2] = 3;
  cat_atualizar_item(0, &it);
}

static void catalogoCom(const char *imdb, const char *tipo, const char *titulo) {
  CatItem it;
  memset(&it, 0, sizeof it);
  snprintf(it.imdb, sizeof it.imdb, "%s", imdb);
  snprintf(it.tipo, sizeof it.tipo, "%s", tipo);
  snprintf(it.titulo, sizeof it.titulo, "%s", titulo);
  // Um tmdb qualquer, como o de um catalogo que o trouxesse com o tipo errado.
  it.tmdb = 94664;
  cat_definir_tudo(&it, 1, NULL, 0);
}

// Um item de catalogo com id proprio de addon e a origem dele.
static void catalogoDe(const char *imdb, const char *tipo, const char *titulo,
                       const char *origem) {
  CatItem it;
  memset(&it, 0, sizeof it);
  snprintf(it.imdb, sizeof it.imdb, "%s", imdb);
  snprintf(it.tipo, sizeof it.tipo, "%s", tipo);
  snprintf(it.titulo, sizeof it.titulo, "%s", titulo);
  snprintf(it.origem, sizeof it.origem, "%s", origem);
  cat_definir_tudo(&it, 1, NULL, 0);
}

static void abrir(void) {
  nPedidos = 0;
  epItem = 0;
  fioEpVivo = 1;
  buscarEps(NULL);
}

int main(void) {
  // 1) deMeta: o `type` do item vence o "anime" do catalogo; o "type" de
  //    trailers[] (aninhado, e vem ANTES) nao conta.
  { const char *js =
      "{\"trailers\":[{\"source\":\"x\",\"type\":\"Trailer\"}],"
      "\"id\":\"tt13293588\",\"type\":\"series\",\"name\":\"Mushoku Tensei\","
      "\"poster\":\"https://p/x.jpg\"}";
    CatItem d;
    assert(deMeta(js, js + strlen(js), "anime", &d));
    assert(!strcmp(d.tipo, "series"));
    const char *js2 =
      "{\"id\":\"tt13293588\",\"name\":\"Mushoku Tensei\",\"poster\":\"https://p/x.jpg\"}";
    assert(deMeta(js2, js2 + strlen(js2), "anime", &d));
    assert(!strcmp(d.tipo, "anime"));   // sem prova, nao inventa
    const char *js3 =
      "{\"id\":\"tt1\",\"type\":\"other\",\"name\":\"X\",\"poster\":\"https://p/x.jpg\"}";
    assert(deMeta(js3, js3 + strlen(js3), "movie", &d));
    assert(!strcmp(d.tipo, "movie")); }
  puts("ok  tipo do item (movie/series) vence o do catalogo; o resto fica");

  // 2) Item "anime" sem tipo proprio: pergunta como serie, publica episodios,
  //    grava "series", nao traz o diretor do filme homonimo.
  limparCacheMeta();
  catalogoCom("tt13293588", "anime", "Mushoku Tensei: Jobless Reincarnation");
  abrir();
  assert(pediu("/meta/series/tt13293588.json"));
  assert(!pediu("/meta/movie/tt13293588.json"));
  { const CatItem *ci = cat_item(0);
    assert(ci);
    assert(!strcmp(ci->tipo, "series"));
    assert(ci->direcao[0] == 0);
    assert(ci->nTemporadas == 2);
    assert(ci->tmdb == 0);           // 94664 sem tipo provado nao fica
    assert(cat_n_episodios(0) == 3); }
  puts("ok  anime vira serie pelo /meta: 3 episodios, 2 temporadas, sem Jancso");

  // 3) Cache por tipo: o corpo do filme guardado para o mesmo id NAO serve a
  //    serie. Simula a sequencia do log: aberto como filme, depois como serie.
  limparCacheMeta();
  catalogoCom("tt13293588", "movie", "Mushoku Tensei: Jobless Reincarnation");
  abrir();
  assert(pediu("/meta/movie/tt13293588.json"));
  catalogoCom("tt13293588", "series", "Mushoku Tensei: Jobless Reincarnation");
  abrir();
  assert(pediu("/meta/series/tt13293588.json"));   // nao leu o do filme
  assert(cat_n_episodios(0) == 3);
  assert(cat_item(0)->direcao[0] == 0);
  puts("ok  cache do /meta separa filme e serie do mesmo id");

  // 4) Filme de verdade: um pedido, /meta/movie, tipo intacto.
  limparCacheMeta();
  catalogoCom("tt0111161", "movie", "The Shawshank Redemption");
  abrir();
  assert(nPedidos >= 1 && strstr(pedidos[0], "/meta/movie/tt0111161.json"));
  assert(!pediu("/meta/series/"));
  assert(!strcmp(cat_item(0)->tipo, "movie"));
  assert(!strcmp(cat_item(0)->direcao, "Frank Darabont"));
  puts("ok  filme continua um pedido so a /meta/movie");

  // 5) Id que nao e do IMDb nao vai ao Cinemeta (nem vira a chave "kitsu").
  limparCacheMeta();
  catalogoCom("kitsu:41370", "anime", "Mushoku Tensei");
  abrir();
  assert(!pediu("cinemeta"));
  assert(fioEpVivo == 0);
  puts("ok  id kitsu: nao pede /meta ao Cinemeta");

  // 6) As puras.
  { const char *t[2];
    assert(desc_meta_tipos("series", t) == 1 && !strcmp(t[0], "series"));
    assert(desc_meta_tipos("movie", t) == 1 && !strcmp(t[0], "movie"));
    assert(desc_meta_tipos("anime", t) == 2 && !strcmp(t[0], "series") && !strcmp(t[1], "movie"));
    assert(desc_meta_tipos("", t) == 2);
    assert(desc_meta_tem_temporadas(META_SERIE));
    assert(!desc_meta_tem_temporadas(META_FILME_ERRADO)); }
  // 7) Cinemeta com MENOS episodios que o addon de metadados (#174, #175): a
  //    lista do addon entra no lugar. Com o addon trazendo igual ou menos, a
  //    do Cinemeta fica.
  limparCacheMeta();
  addonMeta = 1;
  addonResp = META_SERIE_ADDON;
  catalogoCom("tt13293588", "series", "Mushoku Tensei: Jobless Reincarnation");
  abrir();
  assert(pediu("addon.test/SEGREDO/meta/series/tt13293588.json"));
  assert(cat_n_episodios(0) == 5);
  puts("ok  addon com mais episodios que o Cinemeta: lista do addon");
  limparCacheMeta();
  assert(desc_meta_n_episodios(META_SERIE) == 3);
  assert(desc_meta_n_episodios(META_FILME_ERRADO) == 0);
  assert(desc_meta_n_episodios(NULL) == 0);
  addonMeta = 0;
  catalogoCom("tt13293588", "series", "Mushoku Tensei: Jobless Reincarnation");
  abrir();
  assert(!pediu("addon.test"));
  assert(cat_n_episodios(0) == 3);
  puts("ok  sem addon de meta a lista do Cinemeta fica como era");
  // 8) ITEM RASO (#176): o que "Salvos" e a lista do Trakt entregam (titulo,
  //    poster, fundo/logo do metahub, "14" do Trakt) sai do detalhe igual ao
  //    que a busca entrega: sinopse, ano, tmdb, fundo e logo do /meta.
  limparCacheMeta();
  { CatItem it;
    memset(&it, 0, sizeof it);
    snprintf(it.imdb, sizeof it.imdb, "tt0000176");
    snprintf(it.tipo, sizeof it.tipo, "movie");
    snprintf(it.titulo, sizeof it.titulo, "Filme Salvo");
    snprintf(it.poster, sizeof it.poster, "https://p.test/poster.jpg");
    snprintf(it.backdrop, sizeof it.backdrop, "https://images.metahub.space/background/medium/tt0000176/img");
    snprintf(it.logo, sizeof it.logo, "https://images.metahub.space/logo/medium/tt0000176/img");
    snprintf(it.classificacao, sizeof it.classificacao, "14");
    it.naLista = 1;
    cat_definir_tudo(&it, 1, NULL, 0); }
  abrir();
  { const CatItem *ci = cat_item(0);
    assert(ci);
    assert(!strcmp(ci->sinopse, "Sinopse de verdade."));
    assert(!strncmp(ci->meta, "2019", 4));
    assert(ci->tmdb == 4242);
    assert(!strcmp(ci->backdrop, "https://p.test/fundo.jpg"));
    assert(!strcmp(ci->logo, "https://p.test/logo.png"));
    assert(ci->classificacao[0] == 0);
    assert(ci->naLista == 1);                            // o que era do item fica
    assert(!strcmp(ci->poster, "https://p.test/poster.jpg")); }
  puts("ok  item raso (salvos/Trakt) sai do detalhe com o meta da busca");
  // Item que JA tem sinopse nao e tocado (o do catalogo/busca).
  limparCacheMeta();
  { CatItem it;
    memset(&it, 0, sizeof it);
    snprintf(it.imdb, sizeof it.imdb, "tt0000176");
    snprintf(it.tipo, sizeof it.tipo, "movie");
    snprintf(it.titulo, sizeof it.titulo, "Filme Salvo");
    snprintf(it.poster, sizeof it.poster, "https://p.test/poster.jpg");
    snprintf(it.backdrop, sizeof it.backdrop, "https://x.test/outro-fundo.jpg");
    snprintf(it.sinopse, sizeof it.sinopse, "Sinopse do catalogo.");
    cat_definir_tudo(&it, 1, NULL, 0); }
  abrir();
  assert(!strcmp(cat_item(0)->sinopse, "Sinopse do catalogo."));
  assert(!strcmp(cat_item(0)->backdrop, "https://x.test/outro-fundo.jpg"));
  puts("ok  item completo nao e sobrescrito");

  // ===========================================================================
  // #176: CONTEUDO LOCALIZADO. O usuario tem um addon de metadados em ucraniano
  // e via o titulo, os episodios e a sinopse em ingles do Cinemeta.
  // ===========================================================================
  { static const char *EN =
      "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\",\"name\":\"Jobless Reincarnation\","
      "\"description\":\"A 34-year-old is reborn.\",\"genres\":[\"Animation\",\"Fantasy\"],"
      "\"cast\":[\"Yumi Uchiyama\"],\"videos\":["
      "{\"id\":\"tt13293588:1:1\",\"season\":1,\"episode\":1,\"name\":\"Jobless Reincarnation\","
      "\"overview\":\"English 1\",\"thumbnail\":\"https://c/t1.jpg\"},"
      "{\"id\":\"tt13293588:1:2\",\"season\":1,\"episode\":2,\"name\":\"Getting Ahead of Myself\","
      "\"overview\":\"English 2\",\"thumbnail\":\"https://c/t2.jpg\"},"
      "{\"id\":\"tt13293588:2:1\",\"season\":2,\"episode\":1,\"name\":\"The Brokenhearted Mage\","
      "\"overview\":\"English 3\",\"thumbnail\":\"https://c/t3.jpg\"}]}}";
    static const char *UK3 =
      "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
      "\"name\":\"Реінкарнація безробітного\",\"description\":\"Тридцятичотирирічний чоловік renasce.\","
      "\"genres\":[\"Анімація\",\"Фентезі\"],\"videos\":["
      "{\"season\":1,\"episode\":1,\"name\":\"Перший епізод\",\"overview\":\"Опис 1\"},"
      "{\"season\":1,\"episode\":2,\"name\":\"Другий епізод\",\"overview\":\"Опис 2\"},"
      "{\"season\":2,\"episode\":1,\"name\":\"Третій епізод\",\"overview\":\"Опис 3\"}]}}";
    static const char *UK2 =
      "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\",\"name\":\"Реінкарнація\","
      "\"videos\":[{\"season\":1,\"episode\":1,\"name\":\"Перший епізод\",\"overview\":\"Опис 1\"},"
      "{\"season\":1,\"episode\":2,\"name\":\"Другий епізод\",\"overview\":\"Опис 2\"}]}}";
    const CatEp *e;
    cineSerie = EN;
    addonMeta = 1;
    addonTipo = "/series/";
    fakeIdioma = "en-US";

    // 8) SEM a preferencia e em ingles: nada muda (o Cinemeta continua mandando).
    limparCacheMeta();
    fakeMetaExterno = 0;
    addonResp = UK3;
    catalogoCom("tt13293588", "series", "Jobless Reincarnation");
    abrir();
    assert(cat_n_episodios(0) == 3);
    e = cat_episodio(0, 0);
    assert(!strcmp(e->nome, "Jobless Reincarnation") && !strcmp(e->sinopse, "English 1"));
    assert(!strcmp(cat_item(0)->titulo, "Jobless Reincarnation"));
    puts("ok  #176: sem a preferencia e em ingles o Cinemeta segue mandando");

    // 9) COM "Prefere a ficha do addon" (ajustes_meta_externo, que ninguem lia):
    //    titulo, sinopse, generos, nome e sinopse dos episodios vem do addon; o
    //    still que o addon nao tem vem do Cinemeta. A URL do addon nao vai ao log
    //    (o teste so ve o nome, "addon").
    limparCacheMeta();
    fakeMetaExterno = 1;
    catalogoCom("tt13293588", "series", "Jobless Reincarnation");
    abrir();
    assert(cat_n_episodios(0) == 3);
    e = cat_episodio(0, 0);
    assert(!strcmp(e->nome, "Перший епізод") && !strcmp(e->sinopse, "Опис 1"));
    assert(!strcmp(e->thumb, "https://c/t1.jpg"));       /* preenchido do Cinemeta */
    e = cat_episodio(0, 2);
    assert(e->temporada == 2 && !strcmp(e->nome, "Третій епізод"));
    assert(!strcmp(cat_item(0)->titulo, "Реінкарнація безробітного"));
    assert(strstr(cat_item(0)->sinopse, "Тридцятичотирирічний"));
    assert(strstr(cat_item(0)->genero, "Анімація"));
    assert(cat_item(0)->nElenco == 1);                    /* elenco do Cinemeta fica */
    assert(cat_item(0)->nTemporadas == 2);
    puts("ok  #176: com a preferencia, titulo/sinopse/generos/episodios saem do addon");

    // 10) Addon com MENOS episodios: a lista do Cinemeta fica (nao some episodio)
    //     e leva nome/sinopse do addon nos que os dois tem; o 3o segue em ingles.
    limparCacheMeta();
    addonResp = UK2;
    catalogoCom("tt13293588", "series", "Jobless Reincarnation");
    abrir();
    assert(cat_n_episodios(0) == 3);
    assert(!strcmp(cat_episodio(0, 0)->nome, "Перший епізод"));
    assert(!strcmp(cat_episodio(0, 1)->sinopse, "Опис 2"));
    assert(!strcmp(cat_episodio(0, 2)->nome, "The Brokenhearted Mage"));
    puts("ok  #176: addon com menos episodios: lista do Cinemeta com o texto do addon");

    // 11) Addon sem resposta valida ("meta":null) nao apaga o texto do Cinemeta.
    limparCacheMeta();
    addonResp = "{\"meta\":null}";
    catalogoCom("tt13293588", "series", "Jobless Reincarnation");
    abrir();
    assert(!strcmp(cat_item(0)->titulo, "Jobless Reincarnation"));
    assert(!strcmp(cat_episodio(0, 0)->nome, "Jobless Reincarnation"));
    puts("ok  #176: meta invalida do addon nao apaga nada");

    // 12) SEM a preferencia, UI em ucraniano-like (idioma nao ingles) e TMDB
    //     ligado: quem traduz e o TMDB. O addon NAO manda; o nome do episodio vem
    //     do TMDB, e "Episodio 2" (sem traducao) nao entra.
    limparCacheMeta();
    fakeMetaExterno = 0;
    fakeIdioma = "uk-UA";
    fakeTmdbBasico = 1;
    desc_tmdb_definir("0123456789abcdef0123456789abcdef");
    addonResp = UK3;
    tmdbFind = "{\"tv_results\":[{\"id\":555}]}";
    tmdbTemp1 =
      "{\"episodes\":["
      "{\"episode_number\":1,\"name\":\"Безробітне переродження\",\"overview\":\"TMDB 1\",\"vote_average\":8.1},"
      "{\"episode_number\":2,\"name\":\"Епізод 2\",\"overview\":\"\"}]}";
    catalogoCom("tt13293588", "series", "Jobless Reincarnation");
    abrir();
    assert(pediu("themoviedb.org/3/tv/555/season/1?"));
    assert(!strcmp(cat_item(0)->titulo, "Jobless Reincarnation"));    /* addon nao mandou */
    e = cat_episodio(0, 0);
    assert(!strcmp(e->nome, "Безробітне переродження") && !strcmp(e->sinopse, "TMDB 1"));
    assert(e->nota == 81);
    e = cat_episodio(0, 1);
    assert(!strcmp(e->nome, "Getting Ahead of Myself"));              /* generico fica de fora */
    assert(!strcmp(e->sinopse, "English 2"));
    puts("ok  #176: com TMDB num idioma nao ingles o nome do episodio vem traduzido; generico nao entra");

    // 13) COM a preferencia e o TMDB tambem ligado: o addon manda e o TMDB so
    //     preenche o que ficou vazio (nao pisa no texto do addon).
    limparCacheMeta();
    fakeMetaExterno = 1;
    catalogoCom("tt13293588", "series", "Jobless Reincarnation");
    abrir();
    assert(!strcmp(cat_episodio(0, 0)->nome, "Перший епізод"));
    assert(!strcmp(cat_episodio(0, 0)->sinopse, "Опис 1"));
    assert(cat_episodio(0, 0)->nota == 81);                           /* nota do TMDB entra */
    assert(!strcmp(cat_item(0)->titulo, "Реінкарнація безробітного")); /* sem /tv/555 aqui: o caso #209 (30) cobre o TMDB por cima */
    puts("ok  #176: addon preferido + TMDB: o texto do addon nao e pisado, a nota entra");

    // 14) FILME com a preferencia: /meta/movie do addon manda no titulo/sinopse.
    limparCacheMeta();
    fakeTmdbBasico = 0;
    tmdbFind = NULL; tmdbTemp1 = NULL;
    addonTipo = "/movie/";
    addonResp = "{\"meta\":{\"id\":\"tt0111161\",\"type\":\"movie\",\"name\":\"Втеча з Шоушенка\","
                "\"description\":\"Опис фільму\",\"genres\":[\"Драма\"]}}";
    catalogoCom("tt0111161", "movie", "The Shawshank Redemption");
    abrir();
    assert(!strcmp(cat_item(0)->titulo, "Втеча з Шоушенка"));
    assert(!strcmp(cat_item(0)->sinopse, "Опис фільму"));
    assert(!strcmp(cat_item(0)->genero, "Filme  \xc2\xb7  Драма"));   /* tipo + genero: o hero descarta o 1o trecho */
    assert(!strcmp(cat_item(0)->direcao, "Frank Darabont"));          /* resto do Cinemeta */
    puts("ok  #176: filme com a preferencia: titulo/sinopse/generos do addon, resto do Cinemeta");

    // 15) O NOME GENERICO (funcao pura).
    assert(desc_nome_episodio_generico("Episode 3", 3));
    assert(desc_nome_episodio_generico("Episódio 12", 12));
    assert(desc_nome_episodio_generico("Серія 3", 3));
    assert(desc_nome_episodio_generico("#3", 3));
    assert(desc_nome_episodio_generico("", 3));
    assert(!desc_nome_episodio_generico("Episode 3", 4));
    assert(!desc_nome_episodio_generico("The Brokenhearted Mage", 1));
    assert(!desc_nome_episodio_generico("Season Finale 3", 3));
    assert(!desc_nome_episodio_generico("Apollo 13 Pt 2", 2));
    puts("ok  #176: nome generico do TMDB reconhecido, titulo de verdade nao");

    // 16) Mescla pura: modo TEXTO troca; modo VAZIOS so preenche.
    { CatEp a[2], o[2];
      memset(a, 0, sizeof a); memset(o, 0, sizeof o);
      a[0].temporada = 1; a[0].episodio = 1; snprintf(a[0].nome, sizeof a[0].nome, "A");
      a[1].temporada = 1; a[1].episodio = 2;
      o[0].temporada = 1; o[0].episodio = 1; snprintf(o[0].nome, sizeof o[0].nome, "O");
      snprintf(o[0].thumb, sizeof o[0].thumb, "t");
      o[1].temporada = 1; o[1].episodio = 2; snprintf(o[1].nome, sizeof o[1].nome, "O2");
      desc_mesclar_episodios(a, 2, o, 2, DESC_MESCLA_VAZIOS);
      assert(!strcmp(a[0].nome, "A") && !strcmp(a[1].nome, "O2") && !strcmp(a[0].thumb, "t"));
      desc_mesclar_episodios(a, 2, o, 2, DESC_MESCLA_TEXTO);
      assert(!strcmp(a[0].nome, "O")); }
    puts("ok  #176: mescla de episodios: texto sobrepoe, vazios so preenche");

    // CW / SPOTLIGHT ----------------------------------------------------------
    // 17) Texto localizado de um item do Continuar (titulo em ingles do Trakt):
    //     pela FICHA DO ADDON quando preferida, com um pedido por titulo.
    limparCacheMeta();
    memset(locCache, 0, sizeof locCache);
    fakeMetaExterno = 1;
    { CatItem it[2];
      memset(it, 0, sizeof it);
      snprintf(it[0].imdb, sizeof it[0].imdb, "tt0111161");
      snprintf(it[0].tipo, sizeof it[0].tipo, "movie");
      snprintf(it[0].titulo, sizeof it[0].titulo, "The Shawshank Redemption");
      snprintf(it[0].sinopse, sizeof it[0].sinopse, "English overview");
      it[0].poster[0] = 'x';
      snprintf(it[1].imdb, sizeof it[1].imdb, "cs:channel:abc");   /* canal: fora */
      snprintf(it[1].tipo, sizeof it[1].tipo, "channel");
      snprintf(it[1].titulo, sizeof it[1].titulo, "Canal");
      cat_definir_tudo(it, 2, NULL, 0);
      nPedidos = 0;
      desc_localizar_indices((int[]){ 0, 1 }, 2);
      while (locVivo) usleep(2000);
      assert(!strcmp(cat_item(0)->titulo, "Втеча з Шоушенка"));
      assert(!strcmp(cat_item(0)->sinopse, "Опис фільму"));
      assert(!strcmp(cat_item(1)->titulo, "Canal"));
      assert(nPedidos == 1);                                  /* canal nao pergunta */
      /* de novo: cache, nenhum pedido */
      nPedidos = 0;
      desc_localizar_indices((int[]){ 0 }, 1);
      while (locVivo) usleep(2000);
      assert(nPedidos == 0);
      /* refazer a fileira: o texto ja localizado entra sem rede */
      { CatItem v[1];
        memset(v, 0, sizeof v);
        snprintf(v[0].imdb, sizeof v[0].imdb, "tt0111161:1:2");
        snprintf(v[0].tipo, sizeof v[0].tipo, "movie");
        snprintf(v[0].titulo, sizeof v[0].titulo, "The Shawshank Redemption");
        assert(aplicarLocCache(v, 1) == 1);
        assert(!strcmp(v[0].titulo, "Втеча з Шоушенка"));
        assert(nPedidos == 0); } }
    puts("ok  #176: Continuar/destaque: titulo e sinopse do addon, um pedido por titulo, cache, canal fora");

    // 18) Sem addon e sem preferencia, o TMDB no idioma configurado traduz.
    limparCacheMeta();
    memset(locCache, 0, sizeof locCache);
    fakeMetaExterno = 0;
    addonMeta = 0;
    fakeTmdbBasico = 1;
    tmdbFind = "{\"movie_results\":[{\"id\":278}]}";
    tmdbFilme = "{\"id\":278,\"title\":\"Втеча з Шоушенка (TMDB)\",\"overview\":\"Опис TMDB\","
                "\"genres\":[{\"id\":18,\"name\":\"Драма\"}]}";
    { CatItem it[1];
      memset(it, 0, sizeof it);
      snprintf(it[0].imdb, sizeof it[0].imdb, "tt0111161");
      snprintf(it[0].tipo, sizeof it[0].tipo, "movie");
      snprintf(it[0].titulo, sizeof it[0].titulo, "The Shawshank Redemption");
      it[0].poster[0] = 'x';
      cat_definir_tudo(it, 1, NULL, 0);
      desc_localizar_indices((int[]){ 0 }, 1);
      while (locVivo) usleep(2000);
      assert(!strcmp(cat_item(0)->titulo, "Втеча з Шоушенка (TMDB)"));
      assert(!strcmp(cat_item(0)->sinopse, "Опис TMDB")); }
    puts("ok  #176: Continuar/destaque: sem addon, o TMDB no idioma configurado traduz");

    // 19) Em ingles e sem a preferencia nao ha o que localizar: zero pedidos.
    memset(locCache, 0, sizeof locCache);
    fakeIdioma = "en-US";
    nPedidos = 0;
    { CatItem it[1];
      memset(it, 0, sizeof it);
      snprintf(it[0].imdb, sizeof it[0].imdb, "tt0111161");
      snprintf(it[0].tipo, sizeof it[0].tipo, "movie");
      snprintf(it[0].titulo, sizeof it[0].titulo, "The Shawshank Redemption");
      it[0].poster[0] = 'x';
      cat_definir_tudo(it, 1, NULL, 0);
      desc_localizar_indices((int[]){ 0 }, 1);
      assert(!locVivo);
      assert(nPedidos == 0);
      assert(!strcmp(cat_item(0)->titulo, "The Shawshank Redemption")); }
    puts("ok  #176: em ingles e sem a preferencia nada e localizado (zero pedidos)");


    // 20) SINOPSE NO IDIOMA DA INTERFACE, em todos os idiomas, inclusive num
    //     filme SEM elenco no Cinemeta (antes fotosDoElenco saia cedo). O pedido
    //     leva language=<idioma>; o vazio do TMDB (sem traducao) deixa o ingles.
    { static const struct { const char *tag, *sin, *cast; } L[] = {
        { "pt-BR", "Sinopse em portugu\xc3\xaas.",  "pt" },
        { "fr-FR", "Synopsis en fran\xc3\xa7" "ais.", "fr" },
        { "de-DE", "Handlung auf Deutsch.",          "de" },
        { "es-ES", "Sinopsis en espa\xc3\xb1ol.",   "es" },
        { "ro-RO", "Rezumat \xc3\xaen rom\xc3\xa2n\xc4\x83.", "ro" },
        { "uk-UA", "\xd0\x9e\xd0\xbf\xd0\xb8\xd1\x81 \xd1\x83\xd0\xba\xd1\x80\xd0\xb0\xd1\x97\xd0\xbd\xd1\x81\xd1\x8c\xd0\xba\xd0\xbe\xd1\x8e.", "uk" },
        { "ru-RU", "\xd0\x9e\xd0\xbf\xd0\xb8\xd1\x81\xd0\xb0\xd0\xbd\xd0\xb8\xd0\xb5 \xd0\xbf\xd0\xbe-\xd1\x80\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8.", "ru" },
      };
      size_t li;
      desc_tmdb_definir("0123456789abcdef0123456789abcdef");
      fakeTmdbBasico = 1;
      fakeMetaExterno = 0;
      tmdbFind = "{\"movie_results\":[{\"id\":278}]}";
      for (li = 0; li < sizeof L / sizeof *L; li++) {
        char corpoTmdb[400], marca[32];
        fakeIdioma = L[li].tag;
        snprintf(corpoTmdb, sizeof corpoTmdb,
                 "{\"id\":278,\"title\":\"T\",\"overview\":\"%s\",\"genres\":[]}", L[li].sin);
        tmdbFilme = corpoTmdb;
        limparCacheMeta();
        catalogoCom("tt0000178", "movie", "Movie Without Cast");
        abrir();
        snprintf(marca, sizeof marca, "language=%s", L[li].tag);
        assert(pediu(marca));
        assert(cat_item(0)->nElenco == 0);
        if (strcmp(cat_item(0)->sinopse, L[li].sin) != 0) {
          printf("FALHOU %s: sinopse '%s'\n", L[li].tag, cat_item(0)->sinopse);
          return 1;
        }
      }
      /* GENEROS do Cinemeta (ingles) chegam com o tipo na frente e traduzidos:
         o hero descarta o 1o trecho, entao sem o tipo "Action" sumia. */
      assert(!strcmp(cat_item(0)->genero,
                     "Filme  \xc2\xb7  A\xc3\xa7\xc3\xa3o  \xc2\xb7  Fic\xc3\xa7\xc3\xa3o cient\xc3\xad" "fica"));
      /* TMDB sem traducao (overview vazio): fica a sinopse em ingles do Cinemeta. */
      fakeIdioma = "ru-RU";
      tmdbFilme = "{\"id\":278,\"title\":\"\",\"overview\":\"\",\"genres\":[]}";
      limparCacheMeta();
      catalogoCom("tt0000178", "movie", "Movie Without Cast");
      abrir();
      assert(!strcmp(cat_item(0)->sinopse, "English plot."));
      /* TMDB desligado (sem chave/ajuste): nem /find. */
      fakeTmdbBasico = 0;
      limparCacheMeta();
      catalogoCom("tt0000178", "movie", "Movie Without Cast");
      abrir();
      assert(!pediu("/movie/278?"));
      assert(!strcmp(cat_item(0)->sinopse, "English plot.")); }
    puts("ok  sinopse do TMDB no idioma da interface (pt fr de es ro uk ru), inclusive sem elenco; vazio mantem o ingles");

    // 21) EPISODIOS: nome e sinopse traduzidos em qualquer idioma que nao seja o ingles.
    { static const char *TAGS[] = { "pt-BR", "fr-FR", "de-DE", "es-ES", "ro-RO", "uk-UA", "ru-RU" };
      size_t li;
      for (li = 0; li < sizeof TAGS / sizeof *TAGS; li++) {
        CatEp e[1];
        memset(e, 0, sizeof e);
        e[0].temporada = 1; e[0].episodio = 1;
        snprintf(e[0].nome, sizeof e[0].nome, "English name");
        snprintf(e[0].sinopse, sizeof e[0].sinopse, "English overview");
        fakeIdioma = TAGS[li];
        assert(idiomaNaoIngles());
        assert(desc_tmdb_notas_temporada_ex(
          "{\"episodes\":[{\"episode_number\":1,\"vote_average\":7.5,"
          "\"name\":\"Nome local\",\"overview\":\"Resumo local\"}]}",
          e, 1, 1, DESC_EPT_SINOPSE | DESC_EPT_NOME) == 1);
        assert(!strcmp(e[0].nome, "Nome local") && !strcmp(e[0].sinopse, "Resumo local"));
      }
      fakeIdioma = "en-US";
      assert(!idiomaNaoIngles()); }
    puts("ok  nome/sinopse de episodio do TMDB valem em todos os idiomas (idiomaNaoIngles)");

    // 22) Valores crus do TMDB/Trakt/Cinemeta: status, pais e duracao.
    { char b[200];
      assert(!strcmp(desc_status_chave("Returning Series", 1), "Em exibi\xc3\xa7\xc3\xa3o"));
      assert(!strcmp(desc_status_chave("returning series", 1), "Em exibi\xc3\xa7\xc3\xa3o"));
      assert(!strcmp(desc_status_chave("Ended", 1), "Finalizada"));
      assert(!strcmp(desc_status_chave("Canceled", 1), "Cancelada"));
      assert(!strcmp(desc_status_chave("Canceled", 0), "Cancelado"));
      assert(!strcmp(desc_status_chave("Post Production", 0), "Em p\xc3\xb3s-produ\xc3\xa7\xc3\xa3o"));
      assert(!strcmp(desc_status_chave("Released", 0), "Lan\xc3\xa7" "ado"));
      assert(desc_status_chave("Algo Novo", 0) == NULL);
      assert(desc_status_chave("", 0) == NULL);
      desc_pais_txt("United States of America, Canada", b, sizeof b);
      assert(!strcmp(b, "Estados Unidos, Canad\xc3\xa1"));
      desc_pais_txt("Freedonia,  Japan ", b, sizeof b);     /* desconhecido passa cru */
      assert(!strcmp(b, "Freedonia, Jap\xc3\xa3o"));
      desc_pais_txt("", b, sizeof b);
      assert(b[0] == 0);
      desc_duracao_txt("142 min", b, sizeof b);   assert(!strcmp(b, "2h 22min"));
      desc_duracao_txt("2h 22min", b, sizeof b);  assert(!strcmp(b, "2h 22min"));
      desc_duracao_txt("1 h 54 min", b, sizeof b); assert(!strcmp(b, "1h 54min"));
      desc_duracao_txt("45 min", b, sizeof b);    assert(!strcmp(b, "45min"));
      desc_duracao_txt("2h", b, sizeof b);        assert(!strcmp(b, "2h"));
      desc_duracao_txt("120", b, sizeof b);       assert(!strcmp(b, "2h"));
      desc_duracao_txt("3 temporadas", b, sizeof b); assert(!strcmp(b, "3 temporadas"));
      desc_duracao_txt("", b, sizeof b);          assert(b[0] == 0);
      desc_duracao_min(0, b, sizeof b);           assert(b[0] == 0); }
    puts("ok  status/pais/duracao crus viram chaves da tabela (desconhecido passa cru)");

    tmdbFind = tmdbTemp1 = tmdbFilme = NULL;
    cineSerie = NULL; addonResp = NULL; addonTipo = "/series/";
    fakeMetaExterno = fakeTmdbBasico = 0; fakeIdioma = "pt-BR";
    tmdbChave[0] = 0;
  }

  // ===========================================================================
  // CATALOGO PRIMEIRO (metaCatalogo): anime e outros titulos de catalogo de addon
  // com id proprio (kitsu:, mal:, xperience:...) abrem com a ficha e os episodios
  // do addon que os publicou.
  // ===========================================================================
  { static const char *KITSU_META =
      "{\"meta\":{\"id\":\"kitsu:41370\",\"type\":\"series\",\"name\":\"Mushoku Tensei\","
      "\"description\":\"Sinopse do Kitsu.\",\"genres\":[\"Anime\",\"Fantasia\"],"
      "\"videos\":["
      "{\"id\":\"kitsu:41370:1\",\"title\":\"Desempregado\",\"season\":1,\"episode\":1,"
      "\"thumbnail\":\"https://k.test/1.jpg\",\"released\":\"2021-01-11T00:00:00.000Z\"},"
      "{\"id\":\"kitsu:41370:2\",\"title\":\"Segundo\",\"season\":1,\"episode\":2},"
      "{\"id\":\"kitsu:41370:3\",\"title\":\"Terceiro\",\"episode\":3}]}}";
    int i;
    addonMeta = 0; addonResp = NULL; cineSerie = NULL;
    fakeMetaExterno = fakeTmdbBasico = 0; fakeSoCinemeta = 0;
    fakeIdioma = "en-US";

    // 20) Id "kitsu:" de catalogo de anime: os episodios vem do PROPRIO addon,
    //     com o id de video dele, e o Cinemeta nao e perguntado (nao ha "tt").
    nFake = 2;
    fake[0] = (FakeAddon){ "Outro", "https://outro.test/SEGREDO2", "org.outro", "mal:", 0 };
    fake[1] = (FakeAddon){ "Anime Kitsu", "https://kitsu.test/SEGREDO", "org.kitsu", "kitsu:", 0 };
    limparCacheMeta(); nRotas = 0;
    rota("kitsu.test/SEGREDO/meta/series/kitsu:41370.json", KITSU_META);
    catalogoDe("kitsu:41370", "series", "Mushoku Tensei", "org.kitsu");
    abrir();
    assert(pediu("kitsu.test/SEGREDO/meta/series/kitsu:41370.json"));
    assert(!pediu("outro.test"));                 // prefixo "mal:" nao casa
    assert(!pediu("cinemeta"));
    { const CatItem *ci = cat_item(0);
      assert(cat_n_episodios(0) == 3);           // o 3o nao tinha "season"
      assert(!strcmp(ci->imdb, "kitsu:41370"));  // o id do catalogo fica
      assert(!strcmp(ci->sinopse, "Sinopse do Kitsu."));
      // Formato de generosDe desde fix/detalhe-traducao: o tipo vem primeiro
      // (as telas descartam o 1o trecho) e o separador e "  ·  ".
      assert(strstr(ci->genero, "Anime") && strstr(ci->genero, "Fantasia"));
      assert(ci->nTemporadas == 1 && ci->temporadas[0] == 1); }
    // 21) O id de VIDEO do addon e preservado; e ele que vai a busca de fontes.
    assert(!strcmp(cat_episodio(0, 0)->vid, "kitsu:41370:1"));
    assert(!strcmp(cat_episodio(0, 0)->nome, "Desempregado"));   // "title" no lugar de "name"
    assert(!strcmp(cat_episodio(0, 0)->thumb, "https://k.test/1.jpg"));
    { char id[64];
      assert(cat_id_stream(0, 1, 2, id, sizeof id) && !strcmp(id, "kitsu:41370:2"));
      assert(cat_id_stream(0, 1, 9, id, sizeof id) && !strcmp(id, "kitsu:41370:9"));  // sem video: id:ep
      assert(cat_id_stream(0, 0, 0, id, sizeof id) && !strcmp(id, "kitsu:41370")); }
    puts("ok  catalogo primeiro: kitsu: abre com os episodios do addon, video ids preservados");

    // 22) O item de tipo "anime" (do catalogo) pede /meta/anime/ e vira serie.
    limparCacheMeta(); nRotas = 0;
    rota("kitsu.test/SEGREDO/meta/anime/kitsu:41370.json", KITSU_META);
    catalogoDe("kitsu:41370", "anime", "Mushoku Tensei", "org.kitsu");
    abrir();
    assert(pediu("/meta/anime/kitsu:41370.json"));
    assert(cat_n_episodios(0) == 3 && !strcmp(cat_item(0)->tipo, "series"));
    puts("ok  tipo \"anime\" do catalogo: pede /meta/anime/ e resolve como serie");

    // 23) idPrefixes: sem origem conhecida, so o addon cujo prefixo casa e perguntado.
    limparCacheMeta(); nRotas = 0;
    rota("kitsu.test/SEGREDO/meta/series/kitsu:41370.json", KITSU_META);
    catalogoDe("kitsu:41370", "series", "Mushoku Tensei", "");
    abrir();
    assert(pediu("kitsu.test") && !pediu("outro.test"));
    assert(cat_n_episodios(0) == 3);
    // origem que o manifesto diz NAO ser dela: nao perguntada
    fake[1].nega = 1;
    limparCacheMeta(); nRotas = 0;
    catalogoDe("kitsu:41370", "series", "Mushoku Tensei", "org.kitsu");
    abrir();
    assert(!pediu("kitsu.test"));
    assert(cat_n_episodios(0) == 0);
    fake[1].nega = 0;
    puts("ok  idPrefixes: so o addon que declara o prefixo e perguntado; recusa declarada vale");

    // 24) MESCLA: a base e a ficha do addon (id do IMDb, addon que nao e o
    //     Cinemeta); a ficha nao traz elenco nem lista -> o Cinemeta preenche o
    //     vazio, mas nunca troca a sinopse/generos da base.
    nFake = 1;
    fake[0] = (FakeAddon){ "Xperience", "https://xp.test/SEGREDO", "org.xp", "tt", 0 };
    limparCacheMeta(); nRotas = 0;
    rota("xp.test/SEGREDO/meta/series/tt13293588.json",
         "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\",\"name\":\"Mushoku do Xperience\","
         "\"description\":\"Sinopse do Xperience.\",\"genres\":[\"Aventura\"]}}");
    catalogoDe("tt13293588", "series", "Mushoku Tensei", "org.xp");
    abrir();
    { const CatItem *ci = cat_item(0);
      assert(pediu("xp.test/SEGREDO/meta/series/tt13293588.json"));
      assert(pediu("cinemeta"));                 // complemento
      assert(!strcmp(ci->sinopse, "Sinopse do Xperience."));
      assert(strstr(ci->genero, "Aventura") && !strstr(ci->genero, "Drama"));   // generos da base, nao os do Cinemeta
      assert(ci->nElenco == 3);                  // elenco vazio na base: do Cinemeta
      assert(cat_n_episodios(0) == 3);           // base sem lista: a do Cinemeta
      assert(!strcmp(cat_episodio(0, 0)->vid, "tt13293588:1:1")); }
    { char id[64];
      assert(cat_id_stream(0, 1, 2, id, sizeof id) && !strcmp(id, "tt13293588:1:2")); }
    puts("ok  mescla: a base manda; o Cinemeta so preenche elenco e episodios que faltavam");

    // 25) Base COMPLETA com lista propria: a lista dela fica (2 episodios contra
    //     3 do Cinemeta) e o Cinemeta so completa o campo vazio de cada um.
    limparCacheMeta(); nRotas = 0;
    rota("xp.test/SEGREDO/meta/series/tt13293588.json",
         "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\",\"name\":\"Mushoku do Xperience\","
         "\"description\":\"Sinopse do Xperience.\",\"cast\":[\"Ator do Xperience\"],\"videos\":["
         "{\"season\":1,\"episode\":1,\"name\":\"Nome do Xperience\"},"
         "{\"season\":1,\"episode\":2,\"name\":\"Dois\",\"thumbnail\":\"https://xp/2.jpg\"}]}}");
    catalogoDe("tt13293588", "series", "Mushoku Tensei", "org.xp");
    abrir();
    assert(cat_n_episodios(0) == 2);
    assert(!strcmp(cat_episodio(0, 0)->nome, "Nome do Xperience"));
    assert(cat_item(0)->nElenco == 1 && !strcmp(cat_item(0)->elenco[0].nome, "Ator do Xperience"));
    puts("ok  mescla: lista e elenco da base ficam; Cinemeta so preenche campos vazios");

    // 26) Item do Cinemeta (origem "cinemeta"): nada muda, a lista e a do Cinemeta.
    limparCacheMeta(); nRotas = 0;
    catalogoDe("tt13293588", "series", "Mushoku Tensei", "cinemeta");
    abrir();
    assert(pediu("cinemeta") && cat_n_episodios(0) == 3);
    assert(!strcmp(cat_episodio(0, 0)->nome, "Jobless Reincarnation"));
    puts("ok  item do Cinemeta: o caminho de sempre");

    // 27) "Usar sempre o Cinemeta": o kitsu: nao vai a lugar nenhum e o item do
    //     Xperience volta ao Cinemeta puro.
    fakeSoCinemeta = 1;
    limparCacheMeta(); nRotas = 0;
    rota("xp.test/SEGREDO/meta/series/tt13293588.json",
         "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\",\"name\":\"Xperience\","
         "\"description\":\"Sinopse do Xperience.\",\"videos\":[{\"season\":1,\"episode\":1,\"name\":\"X\"}]}}");
    catalogoDe("tt13293588", "series", "Mushoku Tensei", "org.xp");
    abrir();
    assert(cat_n_episodios(0) == 3);
    assert(!strcmp(cat_episodio(0, 0)->nome, "Jobless Reincarnation"));
    nFake = 2;
    fake[0] = (FakeAddon){ "Outro", "https://outro.test/SEGREDO2", "org.outro", "mal:", 0 };
    fake[1] = (FakeAddon){ "Anime Kitsu", "https://kitsu.test/SEGREDO", "org.kitsu", "kitsu:", 0 };
    limparCacheMeta(); nRotas = 0;
    rota("kitsu.test/SEGREDO/meta/series/kitsu:41370.json", KITSU_META);
    catalogoDe("kitsu:41370", "series", "Mushoku Tensei", "org.kitsu");
    abrir();
    assert(nPedidos == 0 && fioEpVivo == 0 && cat_n_episodios(0) == 0);
    fakeSoCinemeta = 0;
    puts("ok  opt-out \"Usar sempre o Cinemeta\": volta ao comportamento de antes");

    // 28) A ficha do addon nao respondeu: o ARM (kitsu -> imdb) leva ao Cinemeta; o
    //     item continua com o id do addon e os episodios trazem o video id do IMDb.
    nFake = 1;
    fake[0] = (FakeAddon){ "Anime Kitsu", "https://kitsu.test/SEGREDO", "org.kitsu", "kitsu:", 0 };
    limparCacheMeta(); nRotas = 0;
    rota("kitsu.test/SEGREDO/meta/", NULL);      // o addon nao respondeu
    rota("arm.haglund.dev/api/v2/ids?source=kitsu&id=41370",
         "{\"kitsu\":41370,\"imdb\":\"tt13293588\",\"themoviedb\":94664}");
    catalogoDe("kitsu:41370", "series", "Mushoku Tensei", "org.kitsu");
    abrir();
    assert(pediu("arm.haglund.dev/api/v2/ids?source=kitsu&id=41370"));
    assert(pediu("/meta/series/tt13293588.json"));
    assert(!strcmp(cat_item(0)->imdb, "kitsu:41370"));
    assert(cat_n_episodios(0) == 3);
    { char id[64];
      assert(cat_id_stream(0, 1, 1, id, sizeof id) && !strcmp(id, "tt13293588:1:1")); }
    // a falha do addon foi lembrada: reabrir nao repete o pedido dele
    nPedidos = 0; fioEpVivo = 1; buscarEps(NULL);
    assert(!pediu("kitsu.test"));
    puts("ok  ARM: kitsu -> imdb -> Cinemeta quando o addon nao serve; falha lembrada");

    // 29) Um pedido por fonte e por abertura.
    nFake = 2;
    fake[0] = (FakeAddon){ "Outro", "https://outro.test/SEGREDO2", "org.outro", "mal:", 0 };
    fake[1] = (FakeAddon){ "Anime Kitsu", "https://kitsu.test/SEGREDO", "org.kitsu", "kitsu:", 0 };
    limparCacheMeta(); nRotas = 0;
    rota("kitsu.test/SEGREDO/meta/series/kitsu:41370.json", KITSU_META);
    catalogoDe("kitsu:41370", "series", "Mushoku Tensei", "org.kitsu");
    abrir();
    { int n1 = 0; for (i = 0; i < nPedidos && i < 64; i++) if (strstr(pedidos[i], "kitsu.test")) n1++;
      assert(n1 == 1); }
    puts("ok  um pedido so a fonte por abertura");

    // 30) ISSUE #209 (LG 50NANO75SPA, 1.6.5): app e TMDB em portugues, titulo e
    //     sinopse da SERIE em ingles. "Prefere a ficha do addon de metadados" e
    //     LIGADO de fabrica (e preferExternalMetaAddonDetail=true no web, que a
    //     conta sincroniza), e textoDoAddon pega o PRIMEIRO addon que declara
    //     "meta" — no registro de uma TV pt-BR da 1.6.5: "Reacher: texto do addon
    //     Ultra MAX"; noutra, "Tensei Shitara Slime Datta Ken: texto do addon
    //     Bingecat AI Assistant". O texto em ingles desse addon travava o TMDB
    //     (DESC_MANTER_*). No web (metaDetailsScreen.js) o TMDB com "Titulo e
    //     sinopse" ligado vence a ficha de QUALQUER addon: aqui tambem, quando o
    //     idioma nao e o ingles. Vazio do TMDB (sem traducao) deixa o do addon.
    nFake = 0; nRotas = 0; addonMeta = 1;
    addonTipo = "/series/";
    cineSerie = NULL;
    fakeMetaExterno = 1;
    fakeSoCinemeta = 0;
    fakeIdioma = "pt-BR";
    fakeTmdbBasico = 1;
    desc_tmdb_definir("0123456789abcdef0123456789abcdef");
    tmdbFind = "{\"tv_results\":[{\"id\":555}]}";
    tmdbTemp1 = NULL;
    addonResp = "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                "\"name\":\"Mushoku Tensei: Jobless Reincarnation\","
                "\"description\":\"A 34-year-old NEET is reincarnated.\",\"videos\":[]}}";
    limparCacheMeta();
    rota("themoviedb.org/3/tv/555?",
         "{\"id\":555,\"genres\":[{\"id\":16,\"name\":\"Anima\xc3\xa7\xc3\xa3o\"}],"
         "\"name\":\"Mushoku Tensei: Uma Segunda Chance\","
         "\"overview\":\"Um homem de 34 anos renasce em outro mundo.\"}");
    catalogoCom("tt13293588", "series", "Mushoku Tensei: Jobless Reincarnation");
    abrir();
    assert(pediu("themoviedb.org/3/tv/555?") && pediu("language=pt-BR"));
    if (strcmp(cat_item(0)->titulo, "Mushoku Tensei: Uma Segunda Chance") ||
        strcmp(cat_item(0)->sinopse, "Um homem de 34 anos renasce em outro mundo.")) {
      printf("FALHOU #209: titulo '%s' sinopse '%s'\n", cat_item(0)->titulo, cat_item(0)->sinopse);
      return 1;
    }
    puts("ok  #209: addon de meta em ingles + TMDB pt-BR: titulo e sinopse do TMDB");

    // 30b) Sem traducao no TMDB (name/overview vazios): o texto do addon fica.
    limparCacheMeta(); nRotas = 0;
    rota("themoviedb.org/3/tv/555?", "{\"id\":555,\"name\":\"\",\"overview\":\"\"}");
    catalogoCom("tt13293588", "series", "Mushoku Tensei: Jobless Reincarnation");
    abrir();
    assert(!strcmp(cat_item(0)->sinopse, "A 34-year-old NEET is reincarnated."));
    puts("ok  #209: TMDB sem traducao nao apaga o texto do addon");

    // 30c) Em ingles com a preferencia: o addon continua mandando (nada mudou).
    limparCacheMeta(); nRotas = 0;
    fakeIdioma = "en-US";
    rota("themoviedb.org/3/tv/555?", "{\"id\":555,\"name\":\"TMDB Name\",\"overview\":\"TMDB overview\"}");
    catalogoCom("tt13293588", "series", "Mushoku Tensei");
    abrir();
    assert(!strcmp(cat_item(0)->titulo, "Mushoku Tensei: Jobless Reincarnation"));
    assert(!strcmp(cat_item(0)->sinopse, "A 34-year-old NEET is reincarnated."));
    puts("ok  #209: em ingles a ficha do addon preferida segue mandando");

    // 30d) Continuar/destaque (localizarTexto): mesma regra. pt-BR, preferencia
    //      ligada, addon em ingles: o TMDB vem primeiro.
    limparCacheMeta(); nRotas = 0;
    memset(locCache, 0, sizeof locCache);
    fakeIdioma = "pt-BR";
    rota("themoviedb.org/3/tv/555?",
         "{\"id\":555,\"name\":\"Mushoku Tensei: Uma Segunda Chance\",\"overview\":\"Sinopse pt.\"}");
    { CatItem it[1];
      memset(it, 0, sizeof it);
      snprintf(it[0].imdb, sizeof it[0].imdb, "tt13293588");
      snprintf(it[0].tipo, sizeof it[0].tipo, "series");
      snprintf(it[0].titulo, sizeof it[0].titulo, "Mushoku Tensei: Jobless Reincarnation");
      it[0].poster[0] = 'x';
      cat_definir_tudo(it, 1, NULL, 0);
      desc_localizar_indices((int[]){ 0 }, 1);
      while (locVivo) usleep(2000);
      if (strcmp(cat_item(0)->titulo, "Mushoku Tensei: Uma Segunda Chance")) {
        printf("FALHOU #209 CW: titulo '%s'\n", cat_item(0)->titulo);
        return 1;
      }
      assert(!strcmp(cat_item(0)->sinopse, "Sinopse pt.")); }
    puts("ok  #209: Continuar/destaque em pt-BR: TMDB antes da ficha do addon");

    // 31) ISSUE #213 (LG G5, 1.7.0, AIOMetadata em russo): o destaque do
    //     arranque mostra o Continuar do Trakt com logo/fundo do metahub
    //     (ingles). A volta de localizacao traz a ARTE do addon de metadados
    //     primeiro; o texto segue o #209 (TMDB no idioma antes do addon).
    limparCacheMeta(); nRotas = 0;
    memset(locCache, 0, sizeof locCache);
    fakeIdioma = "ru-RU";
    fakeTmdbArte = 1;
    addonResp = "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                "\"_providerArt\":{\"logo\":\"https://tmdb.test/en.png\"},"
                "\"name\":\"Реинкарнация безработного\","
                "\"logo\":\"https://addon.test/logo-ru.png\","
                "\"background\":\"https://addon.test/fundo-ru.jpg\",\"videos\":[]}}";
    rota("themoviedb.org/3/tv/555?",
         "{\"id\":555,\"name\":\"Реинкарнация (TMDB)\",\"overview\":\"Синопсис.\","
         "\"images\":{\"logos\":[{\"iso_639_1\":\"ru\",\"file_path\":\"/ru.png\"}]}}");
    { CatItem it[1];
      memset(it, 0, sizeof it);
      snprintf(it[0].imdb, sizeof it[0].imdb, "tt13293588:1:2");
      snprintf(it[0].tipo, sizeof it[0].tipo, "series");
      snprintf(it[0].titulo, sizeof it[0].titulo, "Mushoku Tensei: Jobless Reincarnation");
      snprintf(it[0].logo, sizeof it[0].logo, "https://images.metahub.space/logo/medium/tt13293588/img");
      snprintf(it[0].backdrop, sizeof it[0].backdrop, "https://images.metahub.space/background/medium/tt13293588/img");
      it[0].poster[0] = 'x';
      cat_definir_tudo(it, 1, NULL, 0);
      desc_localizar_indices((int[]){ 0 }, 1);
      while (locVivo) usleep(2000);
      assert(pediu("include_image_language=ru"));
      assert(!strcmp(cat_item(0)->titulo, "Реинкарнация (TMDB)"));
      if (strcmp(cat_item(0)->logo, "https://addon.test/logo-ru.png") ||
          strcmp(cat_item(0)->backdrop, "https://addon.test/fundo-ru.jpg")) {
        printf("FALHOU #213 arte: logo '%s' fundo '%s'\n", cat_item(0)->logo, cat_item(0)->backdrop);
        return 1;
      } }
    puts("ok  #213: Continuar/destaque: logo e fundo do addon de metadados (raiz), texto do #209");

    // 31b) Addon sem logo: o logo do TMDB no idioma entra; o fundo do addon fica.
    limparCacheMeta();
    memset(locCache, 0, sizeof locCache);
    addonResp = "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\",\"name\":\"X\","
                "\"background\":\"https://addon.test/fundo-ru.jpg\",\"videos\":[]}}";
    { CatItem it[1];
      memset(it, 0, sizeof it);
      snprintf(it[0].imdb, sizeof it[0].imdb, "tt13293588");
      snprintf(it[0].tipo, sizeof it[0].tipo, "series");
      snprintf(it[0].titulo, sizeof it[0].titulo, "Mushoku Tensei");
      it[0].poster[0] = 'x';
      cat_definir_tudo(it, 1, NULL, 0);
      desc_localizar_indices((int[]){ 0 }, 1);
      while (locVivo) usleep(2000);
      assert(!strcmp(cat_item(0)->logo, "https://image.tmdb.org/t/p/w500/ru.png")); }
    puts("ok  #213: sem logo no addon, o do TMDB no idioma");

    // 31c) O QUE FOI LOCALIZADO VAI AO DISCO, e a abertura seguinte pinta o
    //      Continuar ja no idioma, sem rede (o ingles de 1-2 s do relato).
    { char dir[] = "/tmp/nuvio-loc-XXXXXX", arq[600];
      CatItem it[1];
      FILE *f;
      assert(mkdtemp(dir));
      fakeDados = dir;
      limparCacheMeta();
      memset(locCache, 0, sizeof locCache);
      locDiscoLido = 0;
      memset(it, 0, sizeof it);
      snprintf(it[0].imdb, sizeof it[0].imdb, "tt13293588");
      snprintf(it[0].tipo, sizeof it[0].tipo, "series");
      snprintf(it[0].titulo, sizeof it[0].titulo, "Mushoku Tensei");
      snprintf(it[0].sinopse, sizeof it[0].sinopse, "English plot\twith tab");
      it[0].poster[0] = 'x';
      cat_definir_tudo(it, 1, NULL, 0);
      desc_localizar_indices((int[]){ 0 }, 1);
      while (locVivo) usleep(2000);
      snprintf(arq, sizeof arq, "%s/loc-texto.txt", dir);
      f = fopen(arq, "r"); assert(f); fclose(f);
      // "Nova abertura": memoria zerada, nenhuma rede.
      memset(locCache, 0, sizeof locCache);
      locDiscoLido = 0;
      nPedidos = 0;
      cat_definir_tudo(it, 1, NULL, 0);
      assert(desc_localizar_catalogo_cache() == 1);
      assert(nPedidos == 0);
      assert(!strcmp(cat_item(0)->titulo, "Реинкарнация (TMDB)"));
      assert(!strcmp(cat_item(0)->sinopse, "Синопсис."));
      assert(!strcmp(cat_item(0)->logo, "https://image.tmdb.org/t/p/w500/ru.png"));
      { CatItem cw = it[0];
        assert(aplicarLocCache(&cw, 1) == 1 && !strcmp(cw.titulo, "Реинкарнация (TMDB)")); }
      // A entrada do disco pinta, mas a rede ainda refaz uma vez na sessao.
      desc_localizar_indices((int[]){ 0 }, 1);
      while (locVivo) usleep(2000);
      assert(pediu("themoviedb.org/3/tv/555?"));
      puts("ok  #213: texto/arte localizados no disco: o primeiro quadro ja no idioma");

      // 31d) Arquivo de outro perfil e descartado (arte de addon pode levar config).
      memset(locCache, 0, sizeof locCache);
      locDiscoLido = 0;
      fakePerfil = 2;
      cat_definir_tudo(it, 1, NULL, 0);
      assert(desc_localizar_catalogo_cache() == 0);
      assert(!strcmp(cat_item(0)->titulo, "Mushoku Tensei"));
      f = fopen(arq, "r"); assert(!f);
      fakePerfil = 1;
      puts("ok  #213: arquivo de outro perfil descartado");
      desc_loc_apagar();
      rmdir(dir);
      fakeDados = ""; }
    fakeTmdbArte = 0;

    tmdbFind = NULL; fakeTmdbBasico = 0; addonResp = NULL;

    nFake = 0; nRotas = 0; addonMeta = 0;
    limparCacheMeta();
  }
  // 30) #372: AS ABAS DE TEMPORADA SAEM DA LISTA PUBLICADA. Log 2.0.3 (webOS):
  //   [desc] The Apothecary Diaries: AIOMetadata tem 72 episodios contra 60 do
  //   Cinemeta; usando a lista do addon ... 1 temporadas
  // A ficha do Nuvio (TMDB) junta tudo na temporada 1; o addon tem 1, 2 e 3.
  // A lista do addon era publicada, mas as abas vinham do corpo do Nuvio: uma
  // aba so, e detail.c so mostra episodio de temporada que tem aba.
  { static char nuvio60[16384], addon72[16384];
    int s2 = 0, i;
    corpoSerie(nuvio60, sizeof nuvio60, "tt0000372", (const int[]){60}, 1);
    corpoSerie(addon72, sizeof addon72, "tt0000372", (const int[]){24, 24, 24}, 3);
    limparCacheMeta(); nRotas = 0; nFake = 0; metaprov_zerar_pausa();
    rota("catalog.nuvio.tv", nuvio60);
    rota("v3-cinemeta", NULL);
    addonMeta = 1; addonResp = addon72; addonTipo = "/series/";
    catalogoCom("tt0000372", "series", "The Apothecary Diaries");
    abrir();
    assert(cat_n_episodios(0) == 72);
    assert(cat_item(0)->nTemporadas == 3);
    assert(cat_item(0)->temporadas[0] == 1 && cat_item(0)->temporadas[2] == 3);
    for (i = 0; i < cat_n_episodios(0); i++) if (cat_episodio(0, i)->temporada == 2) s2++;
    assert(s2 == 24);
    puts("ok  #372: abas de temporada da lista publicada (addon 3 temporadas sobre Nuvio 1)");
    addonMeta = 0; addonResp = NULL; nRotas = 0; limparCacheMeta(); }

  // 31) Codex P2 (#372): A CAUDA DO FIO VELHO NAO DEVOLVE AS ABAS VELHAS. O fio
  //   A publica T1 e solta fioEpVivo para enfeitar (elenco, /find); o titulo
  //   reaberto (fio B) publica T1-T3; a cauda de A republicava o `edit` inteiro
  //   salvo antes e o item voltava a 1 aba com 72 episodios: T2/T3 escondidos.
  { static char nuvio60b[16384];
    corpoSerie(nuvio60b, sizeof nuvio60b, "tt0000373", (const int[]){60}, 1);
    limparCacheMeta(); nRotas = 0; nFake = 0; metaprov_zerar_pausa();
    rota("catalog.nuvio.tv", nuvio60b);
    rota("v3-cinemeta", NULL);
    addonMeta = 0;
    catalogoCom("tt0000373", "series", "Fio Velho");
    caudaB = publicaB;
    abrir();
    caudaB = NULL;
    assert(cat_n_episodios(0) == 72);
    assert(cat_item(0)->nTemporadas == 3);
    assert(cat_item(0)->temporadas[2] == 3);
    puts("ok  #372: cauda do fio velho nao repoe as abas que o fio novo trocou");
    nRotas = 0; limparCacheMeta(); }

  puts("detalheanime: tudo ok");
  return 0;
}
