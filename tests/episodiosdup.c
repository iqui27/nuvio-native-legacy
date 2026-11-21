// EPISODIO DUPLICADO NA LISTA (#328, Samsung 2.0.2): um /meta cujo videos[]
// repete (temporada, episodio) (agregador de anime com varias entradas) virava
// cartoes identicos, porque parsearEpisodios nao deduplicava. Mantem o primeiro.
//
//   bash tests/episodiosdup.sh
int ajustes_social(void) { return 1; }
int ajustes_fonte_biblioteca(void) { return 0; }
int ajustes_nota_episodio(int fonte) { (void)fonte; return 0; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_busca_cinemeta(void) { return 1; }
int ajustes_busca_nuvio(void) { return 0; }   // #311: Primeiro (padrao)
int ajustes_ocultar_nao_lancados(void) { return 0; }   // #369: descoberta.c le o ajuste
int ajustes_ocultar_filmes_vistos(void) { return 0; }    // #366: descoberta.c le o ajuste
// FALTA DE MEMORIA NO CRESCIMENTO DO CONJUNTO (EpSet): com `callocFalha` ligado,
// todo calloc de mais de 1024 itens (so o crescimento do conjunto pede isso
// neste teste) responde NULL. O primeiro, de 1024, passa.
#include <stdlib.h>
static int callocFalha, callocNegados;
static void *callocDoTeste(size_t n, size_t tam) {
  if (callocFalha && n > 1024) { callocNegados++; return NULL; }
  return calloc(n, tam);
}
static int limitarArrays, respostaFalsaViva;
static void *arrayGrande;
static void *mallocDoTeste(size_t n) {
  if (n >= 5u * 1024u * 1024u && __atomic_load_n(&respostaFalsaViva, __ATOMIC_ACQUIRE)) abort();
  if (limitarArrays && n >= 1024 * 1024 && arrayGrande) return NULL;
  void *p = malloc(n);
  if (limitarArrays && n >= 1024 * 1024) arrayGrande = p;
  return p;
}
static void freeDoTeste(void *p) {
  if (p == arrayGrande) arrayGrande = NULL;
  free(p);
}
static int esperaRespostaTeste;
#define NV_TMDB_AGUARDANDO() __atomic_store_n(&esperaRespostaTeste, 1, __ATOMIC_RELEASE)
#define malloc(n) mallocDoTeste(n)
#define free(p) freeDoTeste(p)
#define calloc(n, tam) callocDoTeste(n, tam)
#include "../src/descoberta.c"
#undef malloc
#undef free
#undef calloc
Uint32 SDL_GetTicks(void) { return 0; }
#include "../src/progresso.h"
#include "../src/metaep.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#define NV_TEST_SERVIDORES_FICHA
#define NV_TEST_PLEX_AVISO
#define NV_TEST_REDE_PARAR
#include "jellyfin_stub.inc"
static _Thread_local RedePararEstado redeTesteEstado;
RedePararEstado rede_parar_trocar(int (*parar)(void *), void *usuario) {
  RedePararEstado anterior = redeTesteEstado;
  redeTesteEstado = (RedePararEstado){ parar, usuario, 1 };
  anterior.instalado = 1;
  return anterior;
}
static _Thread_local int plexTesteReduzido;
static int plexTesteAvisos, plexTesteVagasLiberadas;
void plex_vaga_liberar(void) { __atomic_add_fetch(&plexTesteVagasLiberadas, 1, __ATOMIC_RELAXED); }
int plex_tomar_reduzido(void) { int r = plexTesteReduzido; plexTesteReduzido = 0; return r; }
void plex_publicar_aviso_metadata(void) { __atomic_add_fetch(&plexTesteAvisos, 1, __ATOMIC_RELAXED); }
static pthread_mutex_t servidorTesteTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t servidorTesteCond = PTHREAD_COND_INITIALIZER;
static int servidorBloqueia, servidorEntrou, servidorSoltar, servidorNovoTerminou;
int servidores_ficha_preparar(CatItem *item, CatEp **eps, int maxEps, CatEpPreparar preparar, void *ctx, int (*cancelado)(void *), void *cancelCtx, int *vaga) {
  (void)item; (void)maxEps; (void)cancelado; (void)cancelCtx; if (vaga) *vaga = 0;
  pthread_mutex_lock(&servidorTesteTrava);
  if (servidorBloqueia) {
    servidorEntrou = 1; pthread_cond_broadcast(&servidorTesteCond);
    while (!servidorSoltar) pthread_cond_wait(&servidorTesteCond, &servidorTesteTrava);
  }
  pthread_mutex_unlock(&servidorTesteTrava);
  if (!servidorBloqueia) return -1;
  if (vaga) *vaga = 1;
  plexTesteReduzido = 1;
  *eps = preparar(ctx, 1);
  if (!*eps) return -1;
  memset(*eps, 0, sizeof **eps);
  (*eps)->temporada = 1; (*eps)->episodio = 99;
  return 1;
}
static void *servidorTesteNovo(void *arg) {
  (void)arg;
  publicarEpisodios("{\"videos\":[{\"season\":1,\"episode\":1}]}", 0, "new", NULL, 0, NULL);
  pthread_mutex_lock(&servidorTesteTrava);
  servidorNovoTerminou = 1; pthread_cond_broadcast(&servidorTesteCond);
  pthread_mutex_unlock(&servidorTesteTrava);
  return NULL;
}
typedef struct { pthread_mutex_t trava; pthread_cond_t cond; int pronto, sair, resposta, prontoNovo, fimNovo, addon; TmdbEtapa etapa; } EtapaBloqueada;
static int testeRedeAnterior(void *u) { (void)u; return 0; }
static int testeRedeAbortou;
static void *etapaBloqueada(void *arg) {
  EtapaBloqueada *t = arg;
  EpPedido pedido = {0};
  pedido.geracao = epGeracao;
  pthread_once(&epPedidoOnce, epPedidoKeyCriar);
  pthread_setspecific(epPedidoKey, &pedido);
  char *resposta = t->resposta ? malloc(1024u * 1024u) : NULL;
  assert(!t->resposta || resposta);
  if (resposta) __atomic_store_n(&respostaFalsaViva, 1, __ATOMIC_RELEASE);
  pthread_mutex_lock(&tmdbDadosTrava);
  t->etapa.textos = malloc(TMDB_TEXTOS_MAX);
  t->etapa.patches = malloc(5000u * sizeof(TmdbPatch));
  t->etapa.chaves = malloc(5000u * sizeof(CatEpChave));
  t->etapa.temporadas = malloc(5000u * sizeof(int));
  assert(t->etapa.textos && t->etapa.patches && t->etapa.chaves && t->etapa.temporadas);
  t->etapa.respostaPendente = t->resposta;
  tmdbAtiva = &t->etapa; tmdbAtivaGeracao = epGeracao;
  pthread_mutex_unlock(&tmdbDadosTrava);
  pthread_mutex_lock(&t->trava); t->pronto = 1; pthread_cond_broadcast(&t->cond);
  if (!t->resposta) while (!t->sair) pthread_cond_wait(&t->cond, &t->trava);
  pthread_mutex_unlock(&t->trava);
  if (t->resposta) {
    rede_parar_trocar(testeRedeAnterior, t);
    assert(!tmdbBaixarCancelavel("mock-tmdb-season", 15));
    assert(redeTesteEstado.parar == testeRedeAnterior && redeTesteEstado.usuario == t);
    rede_parar_trocar(NULL, NULL);
  }
  pthread_mutex_lock(&tmdbDadosTrava);
  assert(t->etapa.descartada && !t->etapa.textos && !t->etapa.patches && !t->etapa.chaves && !t->etapa.temporadas);
  free(resposta);
  __atomic_store_n(&respostaFalsaViva, 0, __ATOMIC_RELEASE);
  t->etapa.respostaPendente = 0; pthread_cond_broadcast(&tmdbDadosCond);
  tmdbAtiva = NULL; tmdbEtapaLimpar(&t->etapa);
  pthread_mutex_unlock(&tmdbDadosTrava);
  pthread_setspecific(epPedidoKey, NULL);
  return NULL;
}
static void *esperarWorkspace(void *arg) {
  EtapaBloqueada *t = arg; int preso = 0; CatEp *eps = NULL;
  pthread_mutex_lock(&t->trava); t->prontoNovo = 1; pthread_cond_broadcast(&t->cond); pthread_mutex_unlock(&t->trava);
  if (t->addon) {
    size_t cap = 250000, pos = 0; char *json = malloc(cap); assert(json);
    pos += (size_t)snprintf(json + pos, cap - pos, "{\"videos\":[");
    for (int i = 0; i < 5000; i++)
      pos += (size_t)snprintf(json + pos, cap - pos, "%s{\"season\":1,\"episode\":%d}", i ? "," : "", i + 1);
    assert(pos + 4 < cap); snprintf(json + pos, cap - pos, "]}");
    assert(publicarEpisodiosEscolhidos(json, 0, "test", NULL, 0, NULL, 0, 1, 0) == 5000);
    free(json);
  } else {
    eps = epServidorPreparar(&preso, 5000);
    assert(eps && preso); free(eps); pthread_mutex_unlock(&epMemoriaTrava);
  }
  pthread_mutex_lock(&t->trava); t->fimNovo = 1; pthread_cond_broadcast(&t->cond); pthread_mutex_unlock(&t->trava);
  return NULL;
}
// Manifest loading is outside this metadata harness.
void addons_lista_travar(void) {}
void addons_lista_destravar(void) {}
void ws_destino(const char *b, const char *u, int p, char out[20]) {
  (void)b; (void)u; (void)p; out[0] = 0;
}
int addons_identidade_recebida(const char *b, unsigned v, NvPerfilSessao s, const char *c) {
  (void)b; (void)v; (void)s; (void)c; return 1;
}
int addons_manifesto_recebido(const char *b, unsigned v, NvPerfilSessao s, const char *c) {
  return addons_identidade_recebida(b, v, s, c);
}
void addons_manifesto_lido_base(int i, const char *b, const char *d,
                               unsigned v, const char *c) {
  (void)i; (void)b; (void)d; (void)v; (void)c;
}

// --- DUBLES: nenhum participa da regra, so fazem descoberta.c linkar ---------
// (o conjunto e o de tests/colfileiras.c, menos os cat_* — que catalogo.c ja
// traz — e mais os que este teste ja tinha)
int         ajustes_idioma_ingles(void) { return 0; }
int ajustes_idioma(void) { return 0; }
int ajustes_cw_minimo(void) { return 1; }
int   ajustes_cw_concluido(void)         { return 90; }  // factory "watched" threshold
int ajustes_cw_proximo(void) { return 1; }
int trakt_e_a_seguir(const char *id) { (void)id; return 0; }
int trakt_cw_concluiu(const char *id, long long ms) { (void)id; (void)ms; return 0; }
int prog_oculto_vence(const char *id, long long ms) { (void)id; (void)ms; return 0; }
int prog_oculto_soltar(const char *id, long long ms) { (void)id; (void)ms; return 0; }
unsigned recomenda_geracao(void)         { return 0; }   // social generation: no social layer here
int   ajustes_cw_ordem(void)               { return 0; }   // Padrao (issue #127)
int   ajustes_cw_mostrar_nao_exibidos(void) { return 1; }
const char *i18n(const char *s)         { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
static const char *fakeDados = "";
const char *dados_dir(void)             { return fakeDados; }
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  snprintf(dst, tam, "%s/%s", fakeDados, nome); return dst;
}
void addons_catalogo_chave(const char *id, const char *base, const char *tipo,
                           const char *catalogo, char *dst, unsigned tam) {
  (void)base; snprintf(dst, tam, "%s:%s:%s", id, tipo, catalogo);
}
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
int prog_gravar_local(const char *imdb, int t, int e, double p, double d, int tocou) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; (void)tocou; return 0;
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
int fil_marcar_declaradas(const char *const *chaves, int n, int perfil) {
  (void)chaves; (void)n; (void)perfil; return 0;
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

char *rede_baixar(const char *u, int t) {
  int r;
  (void)t;
  if (!strcmp(u, "mock-tmdb-season")) {
    while (!__atomic_load_n(&esperaRespostaTeste, __ATOMIC_ACQUIRE) ||
           !redeTesteEstado.parar || !redeTesteEstado.parar(redeTesteEstado.usuario)) sched_yield();
    __atomic_store_n(&testeRedeAbortou, 1, __ATOMIC_RELEASE);
    return NULL;
  }
  if (nPedidos < 64) snprintf(pedidos[nPedidos], sizeof pedidos[0], "%s", u);
  nPedidos++;
  for (r = 0; r < nRotas; r++)
    if (strstr(u, rotas[r].trecho)) return rotas[r].resp ? strdup(rotas[r].resp) : NULL;
  if (strstr(u, "cinemeta")) {
    if (strstr(u, "/meta/series/tt0388629.json") && cineSerie) return strdup(cineSerie);
    if (strstr(u, "/meta/series/tt21975436.json") && cineSerie) return strdup(cineSerie);
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
  if (strstr(u, "themoviedb.org/3/find/")) return tmdbFind ? strdup(tmdbFind) : NULL;
  if (strstr(u, "themoviedb.org/3/tv/555/season/1?") && tmdbTemp1) return strdup(tmdbTemp1);
  if (strstr(u, "themoviedb.org/3/tv/555/season/")) return strdup("{\"episodes\":[]}");
  if (strstr(u, "themoviedb.org/3/movie/278?") && tmdbFilme) return strdup(tmdbFilme);
  return NULL;   // outros addons: fora do teste
}
char *rede_baixar_st(const char *u, int t, const char *const *c, int *status) {
  (void)c; if (status) *status = 200; return rede_baixar(u, t);
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
  if (getenv("NV_TEST_787_RESPONSE")) {
    EtapaBloqueada t = { .trava = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER, .resposta = 1 };
    pthread_t antigo, novo; int descartada = 0;
    t.addon = !strcmp(getenv("NV_TEST_787_RESPONSE"), "addon");
    if (t.addon) catalogoCom("ttnew", "series", "new");
    alarm(20);
    assert(!pthread_create(&antigo, NULL, etapaBloqueada, &t));
    pthread_mutex_lock(&t.trava);
    while (!t.pronto) pthread_cond_wait(&t.cond, &t.trava);
    pthread_mutex_unlock(&t.trava);
    pthread_mutex_lock(&epPedidoTrava); epGeracao++; pthread_mutex_unlock(&epPedidoTrava);
    assert(!pthread_create(&novo, NULL, esperarWorkspace, &t));
    while (!descartada) {
      pthread_mutex_lock(&tmdbDadosTrava); descartada = t.etapa.descartada; pthread_mutex_unlock(&tmdbDadosTrava);
      if (!descartada) sched_yield();
    }
    pthread_join(antigo, NULL); pthread_join(novo, NULL);
    assert(t.fimNovo && __atomic_load_n(&testeRedeAbortou, __ATOMIC_ACQUIRE));
    alarm(0); return 0;
  }
  if (getenv("NV_TEST_787_STAGE")) {
    EtapaBloqueada t = { .trava = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER };
    pthread_t fio; int preso = 0; CatEp *eps;
    assert(!pthread_create(&fio, NULL, etapaBloqueada, &t));
    pthread_mutex_lock(&t.trava);
    while (!t.pronto) pthread_cond_wait(&t.cond, &t.trava);
    pthread_mutex_unlock(&t.trava);
    assert(t.etapa.textos && t.etapa.patches && !t.etapa.descartada);
    pthread_mutex_lock(&epPedidoTrava); epGeracao++; pthread_mutex_unlock(&epPedidoTrava);
    eps = epServidorPreparar(&preso, 5000);
    assert(eps && preso);
    pthread_mutex_lock(&tmdbDadosTrava);
    assert(t.etapa.descartada && !t.etapa.textos && !t.etapa.patches && !t.etapa.chaves && !t.etapa.temporadas);
    pthread_mutex_unlock(&tmdbDadosTrava);
    free(eps); pthread_mutex_unlock(&epMemoriaTrava);
    pthread_mutex_lock(&t.trava); t.sair = 1; pthread_cond_broadcast(&t.cond); pthread_mutex_unlock(&t.trava);
    pthread_join(fio, NULL);
    return 0;
  }
  if (getenv("NV_TEST_787_SERVER")) {
    pthread_t antigo, novo;
    EpPedido *pedido = calloc(1, sizeof *pedido);
    struct timespec prazo;
    int terminou;
    assert(pedido);
    catalogoCom("jf:old", "series", "old");
    assert(cat_copiar_item(0, &pedido->base));
    snprintf(pedido->id, sizeof pedido->id, "%s", "jf:old");
    pedido->item = 0; pedido->geracao = ++epGeracao;
    pedido->sessao = nv_perfil_sessao_ler();
    servidorBloqueia = 1;
    assert(!pthread_create(&antigo, NULL, buscarEpsPedido, pedido));
    pthread_mutex_lock(&servidorTesteTrava);
    while (!servidorEntrou) pthread_cond_wait(&servidorTesteCond, &servidorTesteTrava);
    pthread_mutex_unlock(&servidorTesteTrava);
    catalogoCom("ttnew", "series", "new");
    pthread_mutex_lock(&epPedidoTrava); epGeracao++; pthread_mutex_unlock(&epPedidoTrava);
    snprintf(epAlvoId, sizeof epAlvoId, "%s", "ttnew");
    assert(!pthread_create(&novo, NULL, servidorTesteNovo, NULL));
    clock_gettime(CLOCK_REALTIME, &prazo); prazo.tv_sec += 2;
    pthread_mutex_lock(&servidorTesteTrava);
    while (!servidorNovoTerminou && !pthread_cond_timedwait(&servidorTesteCond, &servidorTesteTrava, &prazo)) {}
    terminou = servidorNovoTerminou;
    servidorSoltar = 1; pthread_cond_broadcast(&servidorTesteCond);
    pthread_mutex_unlock(&servidorTesteTrava);
    pthread_join(antigo, NULL); pthread_join(novo, NULL);
    assert(terminou && cat_n_episodios(0) == 1);
    assert(plexTesteAvisos == 0 && plexTesteVagasLiberadas == 1);
    assert(cat_episodio(0, 0) && cat_episodio(0, 0)->episodio == 1);
    catalogoCom("jf:new", "series", "server");
    epItem = 0;
    buscarEps(NULL);
    assert(cat_n_episodios(0) == 1 && cat_episodio(0, 0)->episodio == 99);
    assert(plexTesteAvisos == 1 && plexTesteVagasLiberadas == 2);
    return 0;
  }
  if (getenv("NV_TEST_787_MEMORY") || getenv("NV_TEST_787_STABLE")) {
    size_t cap = 400000, a = 0, b = 0;
    char *base = malloc(cap), *overlay = malloc(cap);
    assert(base && overlay);
    a += (size_t)snprintf(base + a, cap - a, "{\"videos\":[");
    b += (size_t)snprintf(overlay + b, cap - b, "{\"videos\":[");
    for (int i = 1; i <= 4650; i++) {
      a += (size_t)snprintf(base + a, cap - a, "%s{\"season\":%d,\"episode\":%d}", i == 1 ? "" : ",", (i - 1) / 150 + 1, (i - 1) % 150 + 1);
      b += (size_t)snprintf(overlay + b, cap - b, "%s{\"season\":%d,\"episode\":%d,\"name\":\"translated\"}", i == 1 ? "" : ",", (i - 1) / 150 + 1, (i - 1) % 150 + 1);
    }
    assert(a + 3 < cap && b + 3 < cap);
    snprintf(base + a, cap - a, "]}"); snprintf(overlay + b, cap - b, "]}");
    catalogoCom("tt787", "series", "Long series");
    snprintf(epAlvoId, sizeof epAlvoId, "%s", "tt787");
    limitarArrays = 1;
    assert(publicarEpisodios(base, 0, "Long series", overlay, DESC_MESCLA_TEXTO, NULL) == 4650);
    limitarArrays = 0;
    assert(cat_n_episodios(0) == 4650);
    const CatEp *last = cat_episodio(0, 4649);
    assert(last && !strcmp(last->nome, "translated"));
    if (getenv("NV_TEST_787_STABLE")) {
    assert(publicarEpisodios(base, 0, "Long series", NULL, 0, NULL) == 4650);
    EpPedido pedido = {0};
    snprintf(pedido.id, sizeof pedido.id, "%s", "tt787");
    pedido.geracao = epGeracao;
    unsigned geracaoAntes = cat_geracao_episodios();
    const CatEp *ponteiroAnterior = cat_episodio(0, 0);
    assert(ponteiroAnterior);
    pthread_once(&epPedidoOnce, epPedidoKeyCriar);
    assert(!pthread_setspecific(epPedidoKey, &pedido));
    limitarArrays = 1;
    assert(publicarEpisodios(base, 0, "Long series", overlay, DESC_MESCLA_TEXTO, NULL) == 4650);
    limitarArrays = 0;
    assert(!pthread_setspecific(epPedidoKey, NULL));
    last = cat_episodio(0, 4649);
    assert(last && !strcmp(last->nome, "translated"));
    assert(cat_geracao_episodios() != geracaoAntes);
    assert(ponteiroAnterior->temporada == 1);
    }
    free(base); free(overlay);
    return 0;
  }
  if (getenv("NV_TEST_787_PARSE")) {
    size_t cap = 400000, k = 0;
    char *body = malloc(cap);
    CatEp *eps = malloc(4650 * sizeof *eps);
    assert(body && eps);
    k += (size_t)snprintf(body + k, cap - k, "{\"videos\":[");
    for (int i = 1; i <= 4650; i++)
      k += (size_t)snprintf(body + k, cap - k,
        "%s{\"season\":%d,\"episode\":%d,\"ids\":{\"tvdb\":%d}}",
        i == 1 ? "" : ",", (i - 1) / 150 + 1, (i - 1) % 150 + 1, i);
    k += (size_t)snprintf(body + k, cap - k,
      ",{\"season\":31,\"episode\":150,\"ids\":{\"tvdb\":99999}}]}");
    assert(k < cap);
    assert(parsearEpisodios(body, eps, 4650) == 4650);
    assert(eps[4649].temporada == 31 && eps[4649].episodio == 150 && !eps[4649].tvdbEp);
    free(eps); free(body);
    return 0;
  }
  // The shell test runs this separately to exercise the real publication path.
  if (getenv("NV_TEST_787")) {
    char *body = malloc(400000);
    assert(body);
    for (int c = 0; c < 4; c++) {
      int seasons = c == 0 ? 52 : c == 1 ? 31 : c == 2 ? 50 : 40;
      int per = c == 0 ? 20 : c == 1 ? 150 : c == 2 ? 100 : 130;
      size_t k = (size_t)sprintf(body, "{\"videos\":[");
      TempsPub tp = {0};
      for (int t = 1; t <= seasons; t++)
        for (int e = 1; e <= per; e++)
          k += (size_t)sprintf(body + k, "%s{\"season\":%d,\"episode\":%d}",
                               t == 1 && e == 1 ? "" : ",", t, e);
      sprintf(body + k, "]}");
      catalogoCom("tt787", "series", "Long series");
      snprintf(epAlvoId, sizeof epAlvoId, "%s", "tt787");
      int total = seasons * per;
      int n = publicarEpisodiosEx(body, 0, "Long series", NULL, 0, &tp, 0);
      int esperado = total > 5000 ? 5000 : total;
      fprintf(stderr, "#787: %d x %d -> %d episodes, %d seasons (total %d)\n", seasons, per, n, tp.n, total);
      assert(n == esperado && tp.n == seasons);
      assert(cat_n_episodios(0) == n);
      assert(cat_item(0)->nTemporadas == seasons);
      if (n == total) {
        const CatEp *last = cat_episodio(0, n - 1);
        assert(last && last->temporada == seasons && last->episodio == per);
      }
      if (seasons == 40 && per == 130 && n < total) {
        epCorpoLiberar();
        epCorpoGuardar("tt787", body);
        assert(cat_item(0)->epsParciais);
        assert(appendTemporadaCorpo(0, 40));
        assert(epTempResidente(0, 40) == per);
      }
    }
    // CAMINHO DE PRODUCAO (#787 volta 3): /meta de 40x130 publica o teto, e a
    // aba pedida por desc_episodios completa do corpo guardado — a truncada
    // (39) e a ausente (40) — sem que um buscarEps posterior as apague.
    {
      char *meta = malloc(400000);
      size_t kk = (size_t)snprintf(meta, 400000,
        "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\",\"name\":\"Series\",\"videos\":[");
      assert(meta);
      for (int t = 1; t <= 40; t++)
        for (int e = 1; e <= 130; e++)
          kk += (size_t)(e == 1
            ? snprintf(meta + kk, 400000 - kk,
                "%s{\"id\":\"source:%d:1\",\"season\":%d,\"episode\":1,"
                "\"overview\":\"Sinopse %d\",\"runtime\":\"25min\","
                "\"thumbnail\":\"https://t.test/%d.jpg\"}",
                t == 1 && e == 1 ? "" : ",", t, t, t, t)
            : snprintf(meta + kk, 400000 - kk,
                "%s{\"id\":\"source:%d:%d\",\"season\":%d,\"episode\":%d}",
                ",", t, e, t, e));
      snprintf(meta + kk, 400000 - kk, "]}}");
      nFake = 0; nRotas = 0; addonMeta = 0; addonResp = NULL;
      cineSerie = meta;
      limparCacheMeta();
      epCorpoLiberar();
      catalogoCom("tt13293588", "series", "Series");
      abrir();
      assert(cat_n_episodios(0) == 5000);
      assert(epTempResidente(0, 39) == 60 && epTempResidente(0, 40) == 0);
      assert(cat_item(0)->nTemporadas == 40 && cat_item(0)->epsParciais);
      fioEpVivo = 0;   // o fio da abertura terminou, como na pagina real
      int pedidos0 = nPedidos;
      desc_episodios(0, 39);
      assert(epTempResidente(0, 39) == 130);
      assert(nPedidos == pedidos0 && !fioEpVivo);
      desc_episodios(0, 40);
      assert(epTempResidente(0, 40) == 130 && epTempResidente(0, 39) == 130);
      assert(nPedidos == pedidos0 && !fioEpVivo);
      // ID DE VIDEO E CAMPOS PRESERVADOS (#787 r2): a temporada recomposta do
      // corpo guardado tem de trazer vid ("source:T:E", que o addon de fontes
      // recebe no /stream), sinopse, duracao e miniatura do /meta — a versao
      // reduzida publicava so numero/nome e cat_id_stream voltava ao
      // "tt13293588:T:E" montado na mao.
      { char stream[64], esperado[64]; const CatEp *ep1;
        int i2, n2 = cat_n_episodios(0), t;
        for (t = 39; t <= 40; t++) {
          ep1 = NULL;
          for (i2 = 0; i2 < n2; i2++) {
            const CatEp *c = cat_episodio(0, i2);
            if (c && c->temporada == t && c->episodio == 1) { ep1 = c; break; }
          }
          assert(ep1);
          assert(cat_id_stream(0, t, 1, stream, sizeof stream));
          snprintf(esperado, sizeof esperado, "source:%d:1", t);
          assert(!strcmp(stream, esperado) && !strcmp(ep1->vid, esperado));
          snprintf(esperado, sizeof esperado, "Sinopse %d", t);
          assert(!strcmp(ep1->sinopse, esperado));
          snprintf(esperado, sizeof esperado, "https://t.test/%d.jpg", t);
          assert(!strcmp(ep1->duracao, "25 min") && !strcmp(ep1->thumb, esperado));
        } }
      cineSerie = NULL; free(meta);
      puts("ok  #787: aba truncada e aba ausente completam do corpo, sem rebuscar, com ids e campos");
    }
    free(body);
    return 0;
  }
  {
    CatEp old[5] = {{0}}, source[10] = {{0}}, expected[10] = {{0}};
    const int order[] = {3, 0, 0, 8, 1};
    int k = 0;
    catalogoCom("ttstable", "series", "Stable");
    for (int i = 0; i < 5; i++) {
      old[i].temporada = source[i].temporada = 1;
      old[i].episodio = order[i]; source[i].episodio = i;
      snprintf(old[i].nome, sizeof old[i].nome, "old%d", i);
      snprintf(source[i].nome, sizeof source[i].nome, "new%d", i);
    }
    cat_definir_episodios(0, old, 5);
    for (int i = 0; i < 5; i++) {
      int j;
      for (j = 0; j < 5; j++) if (old[i].episodio == source[j].episodio) break;
      expected[k++] = j < 5 ? source[j] : old[i];
    }
    for (int j = 0; j < 5; j++) {
      int i;
      for (i = 0; i < k; i++) if (expected[i].episodio == source[j].episodio) break;
      if (i == k) expected[k++] = source[j];
    }
    assert(mesclarEstavel(source, 5, 10, 0, 5) == k);
    assert(!memcmp(source, expected, (size_t)k * sizeof *source));
  }
  {
    const char *base = "{\"videos\":[{\"season\":2,\"episode\":1},{\"season\":1},{\"season\":1},{\"season\":1,\"episode\":2,\"name\":\"original\"}]}";
    const char *overlay = "{\"videos\":[{\"season\":1,\"episode\":2,\"name\":\"first\",\"runtime\":25,\"rating\":8.3},{\"season\":1,\"episode\":2,\"name\":\"duplicate\"},{\"season\":1,\"name\":\"zero A\"},{\"season\":2,\"episode\":1,\"description\":\"synopsis\"},{\"season\":1,\"name\":\"zero B\"}]}";
    CatEp expected[8], actual[8], other[8];
    int count = parsearEpisodios(base, expected, 8);
    int countOther = parsearEpisodios(overlay, other, 8);
    assert(count == 4 && countOther == 4);
    for (int mode = DESC_MESCLA_TEXTO; mode <= DESC_MESCLA_VAZIOS; mode++) {
      memcpy(actual, expected, (size_t)count * sizeof *actual);
      CatEp reference[8];
      memcpy(reference, expected, (size_t)count * sizeof *reference);
      desc_mesclar_episodios(reference, count, other, countOther, mode);
      mesclarEpisodiosJson(actual, count, overlay, mode);
      assert(!memcmp(reference, actual, (size_t)count * sizeof *actual));
    }
  }
  const char *js =
    "{\"videos\":["
    "{\"id\":\"a:1\",\"season\":1,\"episode\":1,\"name\":\"Izuku Midoriya: Origin\"},"
    "{\"id\":\"a:1b\",\"season\":1,\"episode\":1,\"name\":\"Izuku Midoriya: Origin\"},"
    "{\"id\":\"a:2\",\"season\":1,\"episode\":2,\"name\":\"What It Takes\"},"
    "{\"id\":\"a:1c\",\"season\":1,\"episode\":1,\"name\":\"Izuku Midoriya: Origin\"},"
    "{\"id\":\"b:1\",\"season\":2,\"episode\":1,\"name\":\"Two\"}]}";
  CatEp e[16];
  int n = parsearEpisodios(js, e, 16);
  assert(n == 3);
  assert(e[0].temporada == 1 && e[0].episodio == 1 && !strcmp(e[0].vid, "a:1"));
  assert(e[1].temporada == 1 && e[1].episodio == 2);
  assert(e[2].temporada == 2 && e[2].episodio == 1);
  // Um ID repetido em tres pares distintos e ambiguo nos tres. A limpeza nao
  // pode zerar os dois primeiros e, por isso, deixar o terceiro sobreviver.
  n = parsearEpisodios("{\"videos\":["
                       "{\"id\":\"a\",\"season\":1,\"episode\":1,\"ids\":{\"tvdb\":900}},"
                       "{\"id\":\"b\",\"season\":1,\"episode\":2,\"ids\":{\"tvdb\":900}},"
                       "{\"id\":\"c\",\"season\":2,\"episode\":1,\"ids\":{\"tvdb\":900}}]}", e, 16);
  assert(n == 3 && e[0].tvdbEp == 0 && e[1].tvdbEp == 0 && e[2].tvdbEp == 0);
  n = parsearEpisodios("{\"videos\":["
                       "{\"id\":\"a\",\"season\":2,\"episode\":1},"
                       "{\"id\":\"b\",\"season\":2,\"episode\":1,\"ids\":{\"tvdb\":50}}]}", e, 16);
  assert(n == 1 && e[0].tvdbEp == 50);
  n = parsearEpisodios("{\"videos\":["
                       "{\"id\":\"a\",\"season\":2,\"episode\":1,\"ids\":{\"tvdb\":50}},"
                       "{\"id\":\"b\",\"season\":2,\"episode\":1,\"ids\":{\"tvdb\":51}},"
                       "{\"id\":\"c\",\"season\":2,\"episode\":1,\"ids\":{\"tvdb\":50}}]}", e, 16);
  assert(n == 1 && e[0].tvdbEp == 0);
  // Sem "episode" (todos 0): nao sao repeticoes, nenhum some.
  n = parsearEpisodios("{\"videos\":[{\"id\":\"x\",\"season\":1,\"name\":\"X\"},"
                       "{\"id\":\"y\",\"season\":1,\"name\":\"Y\"},"
                       "{\"id\":\"z\",\"season\":1,\"name\":\"Z\"}]}", e, 16);
  assert(n == 3);
  // The Blacklist: an addon videos[] with season = running index, episode 1 and
  // a year-only date made 22 fake one-episode seasons. Never parsed/counted.
  {
    char bad[4096];
    size_t k = (size_t)snprintf(bad, sizeof bad, "{\"videos\":[");
    int i;
    for (i = 1; i <= 22; i++)
      k += (size_t)snprintf(bad + k, sizeof bad - k,
        "%s{\"id\":\"tt2741602:%d:1\",\"season\":%d,\"episode\":1,\"released\":\"2013-01-01T00:00:00.000Z\"}",
        i > 1 ? "," : "", i, i);
    snprintf(bad + k, sizeof bad - k, "]}");
    assert(desc_meta_n_episodios(bad) == 0);
    assert(parsearEpisodios(bad, e, 16) == 0);
    assert(js_videos_temporadas_falsas(bad));
    assert(!metaep_existe(bad, 1, 1));
    assert(!metaep_proximo(bad, 1, 1, NULL));
    char date[40];
    assert(!metaep_lancamento(bad, 1, 1, date, sizeof date));
  }
  // Sparse addon-only catalogs keep every episode, even without Cinemeta.
  {
    char sparse[2048];
    size_t k = (size_t)snprintf(sparse, sizeof sparse,
      "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\",\"name\":\"Sparse\",\"videos\":[");
    for (int i = 1; i <= 5; i++)
      k += (size_t)snprintf(sparse + k, sizeof sparse - k,
        "%s{\"id\":\"sparse:%d:1\",\"season\":%d,\"episode\":1,\"released\":\"202%d-02-%02d\"}",
        i > 1 ? "," : "", i, i, i, i);
    snprintf(sparse + k, sizeof sparse - k, "]}}");
    assert(desc_meta_n_episodios(sparse) == 5);
    assert(parsearEpisodios(sparse, e, 16) == 5);
    assert(!js_videos_temporadas_falsas(sparse));
    assert(metaep_existe(sparse, 5, 1));
    MetaEpInfo next;
    assert(metaep_proximo(sparse, 1, 1, &next));
    assert(next.temporada == 2 && next.episodio == 1);
    char date[40];
    assert(metaep_lancamento(sparse, 5, 1, date, sizeof date));
    assert(!strcmp(date, "2025-02-05"));
    nFake = 1; nRotas = 0;
    fake[0] = (FakeAddon){ "Sparse", "https://sparse.test", "org.sparse", "tt", 0 };
    rota("sparse.test/meta/series/tt13293588.json", sparse);
    rota("cinemeta", NULL);
    limparCacheMeta();
    catalogoDe("tt13293588", "series", "Sparse", "org.sparse");
    abrir();
    assert(cat_n_episodios(0) == 5 && cat_item(0)->nTemporadas == 5);
    for (int i = 0; i < 5; i++) {
      char id[32], date[16];
      snprintf(id, sizeof id, "sparse:%d:1", i + 1);
      snprintf(date, sizeof date, "202%d-02-%02d", i + 1, i + 1);
      const CatEp *ep = cat_episodio(0, i);
      assert(ep->temporada == i + 1 && ep->episodio == 1);
      assert(!strcmp(ep->vid, id) && !strcmp(ep->dataIso, date));
    }
    nFake = 0; nRotas = 0;
    limparCacheMeta();
  }
  // Missing dates and genuine episode numbers do not prove fake seasons.
  for (int dated = 0; dated < 2; dated++) {
    char sparse[2048];
    size_t k = (size_t)snprintf(sparse, sizeof sparse, "{\"videos\":[");
    for (int i = 1; i <= 5; i++)
      k += (size_t)snprintf(sparse + k, sizeof sparse - k,
        "%s{\"season\":%d,\"episode\":%d%s}", i > 1 ? "," : "", i,
        dated ? 2 : 1, dated ? ",\"released\":\"2020-01-01\"" : "");
    snprintf(sparse + k, sizeof sparse - k, "]}");
    assert(desc_meta_n_episodios(sparse) == 5);
    assert(parsearEpisodios(sparse, e, 16) == 5);
  }
  // A real short-season series (6 seasons, 2 episodes each) stays intact.
  {
    char ok[4096];
    size_t k = (size_t)snprintf(ok, sizeof ok, "{\"videos\":[");
    int i;
    for (i = 0; i < 12; i++)
      k += (size_t)snprintf(ok + k, sizeof ok - k,
        "%s{\"season\":%d,\"episode\":%d}", i ? "," : "", i / 2 + 1, i % 2 + 1);
    snprintf(ok + k, sizeof ok - k, "]}");
    assert(desc_meta_n_episodios(ok) == 12);
    assert(parsearEpisodios(ok, e, 16) == 12);
  }
  // A lista adiantada e somente um fallback visual. Quando a fonte canonica
  // chega, sua topologia substitui a provisoria: S1E2 e S2E1 abaixo carregam
  // o mesmo TVDB, mas apenas o par final deve continuar visivel.
  {
    const char *cedo = "{\"videos\":["
      "{\"id\":\"tt111:1:1\",\"season\":1,\"episode\":1,\"name\":\"One early\",\"ids\":{\"tvdb\":101}},"
      "{\"id\":\"tt111:1:2\",\"season\":1,\"episode\":2,\"name\":\"Thirteen\",\"ids\":{\"tvdb\":102}}]}";
    const char *final = "{\"videos\":["
      "{\"id\":\"tt111:1:1\",\"season\":1,\"episode\":1,\"name\":\"One final\",\"ids\":{\"tvdb\":101}},"
      "{\"id\":\"tt111:2:1\",\"season\":2,\"episode\":1,\"name\":\"Season two\",\"ids\":{\"tvdb\":102}}]}";
    TempsPub tp = {0};
    catalogoCom("tt111", "series", "Alternate numbering");
    snprintf(epAlvoId, sizeof epAlvoId, "%s", "tt111");
    assert(publicarEpisodiosEx(cedo, 0, "Alternate numbering", NULL, 0, &tp, 1) == 2);
    assert(cat_n_episodios(0) == 2 && cat_item(0)->epsParciais);
    assert(publicarEpisodiosEx(final, 0, "Alternate numbering", cedo,
                              DESC_MESCLA_VAZIOS, &tp, 0) == 2);
    assert(cat_n_episodios(0) == 2 && !cat_item(0)->epsParciais);
    assert(cat_episodio(0, 0)->temporada == 1 && cat_episodio(0, 0)->episodio == 1);
    assert(cat_episodio(0, 1)->temporada == 2 && cat_episodio(0, 1)->episodio == 1);
    assert(cat_episodio(0, 1)->tvdbEp == 102);
    // Resposta final vazia conserva a lista provisoria/final ja utilizavel.
    assert(publicarEpisodiosEx("{\"videos\":[]}", 0, "Alternate numbering",
                              NULL, 0, &tp, 0) == 0);
    assert(cat_n_episodios(0) == 2);
  }
  // Kaiju No. 8: Cinemeta's complete-looking 23-episode S1 must still be
  // replaced when the selected metadata addon proves a 35-episode S1/S2
  // topology. Keeping both numeric layouts scatters duplicate episodes.
  {
    char cine[8192], addon[12288];
    size_t a = (size_t)snprintf(cine, sizeof cine,
      "{\"meta\":{\"id\":\"tt21975436\",\"type\":\"series\","
      "\"name\":\"Kaiju No. 8\",\"videos\":[");
    size_t b = (size_t)snprintf(addon, sizeof addon,
      "{\"meta\":{\"id\":\"tt21975436\",\"type\":\"series\","
      "\"name\":\"Kaiju No. 8\",\"videos\":[");
    for (int i = 1; i <= 23; i++)
      a += (size_t)snprintf(cine + a, sizeof cine - a,
        "%s{\"season\":1,\"episode\":%d,\"ids\":{\"tvdb\":%d}}",
        i > 1 ? "," : "", i, 1000 + i);
    for (int i = 1; i <= 35; i++) {
      int t = i <= 12 ? 1 : 2, e = i <= 12 ? i : i - 12;
      b += (size_t)snprintf(addon + b, sizeof addon - b,
        "%s{\"season\":%d,\"episode\":%d,\"ids\":{\"tvdb\":%d}}",
        i > 1 ? "," : "", t, e, 1000 + i);
    }
    snprintf(cine + a, sizeof cine - a, "]}}");
    snprintf(addon + b, sizeof addon - b, "]}}");
    nFake = 0; nRotas = 0; addonMeta = 1; addonTipo = "/series/";
    fakeMetaExterno = 0; fakeSoCinemeta = 0; fakeIdioma = "en-US";
    cineSerie = cine; addonResp = addon;
    limparCacheMeta();
    catalogoCom("tt21975436", "series", "Kaiju No. 8");
    abrir();
    assert(cat_n_episodios(0) == 35);
    int velho = 0, novo = 0;
    for (int i = 0; i < cat_n_episodios(0); i++) {
      const CatEp *x = cat_episodio(0, i);
      velho += x->temporada == 1 && x->episodio == 13;
      novo += x->temporada == 2 && x->episodio == 1 && x->tvdbEp == 1013;
    }
    assert(!velho && novo == 1);
    addonMeta = 0; addonResp = NULL; cineSerie = NULL; nRotas = 0;
    limparCacheMeta();
  }
  // Samsung 2.0.4: One Piece renumerado por um addon de fontes.
  {
    size_t cap = 8u << 20, k;
    char *cine = malloc(cap), *arq = malloc(cap);
    int i, caso, via;
    assert(cine && arq);
    for (caso = 0; caso < 6; caso++) {
      // 18 temporadas/1180 vs 1/1200; depois 89%, 90%, 100% de
      // cobertura em uma temporada; por fim 98% com perda de temporadas.
      // Quatro variantes por arquivo.
      int nc = caso && caso < 4 ? 100 : 1180, na = caso == 5 ? 10843 : (caso && caso < 4 ? 120 : 1200);
      k = (size_t)snprintf(cine, cap, "{\"meta\":{\"id\":\"tt0388629\",\"type\":\"series\",\"name\":\"One Piece\",\"videos\":[");
      for (i = 0; i < nc; i++) {
        int t = caso && caso != 5 ? 1 : i % 18 + 1, ep = caso && caso != 5 ? i + 1 : i / 18 + 1;
        if (caso == 4 && i >= nc - 17) { t = i - (nc - 17) + 2; ep = 1; }
        k += (size_t)snprintf(cine + k, cap - k,
          "%s{\"id\":\"tt0388629:%d:%d\",\"season\":%d,\"episode\":%d,\"name\":\"C%d\"}",
          i ? "," : "", t, ep, t, ep, i);
      }
      snprintf(cine + k, cap - k, "]}}");
      k = (size_t)snprintf(arq, cap, "{\"meta\":{\"id\":\"tt0388629\",\"type\":\"series\",\"name\":\"One Piece\",\"videos\":[");
      for (i = 0; i < na * 4; i++) {
        int ep = i % na + 1, t = 1;
        // Field logs 95009/95369/95816: 10843 distinct file episodes
        // replace 1180 catalog episodes, then get truncated to 2400.
        // Full pair coverage must not make an oversized replacement safe.
        if (caso == 5) {
          int j = i % na;
          t = j < nc ? j % 18 + 1 : 1;
          ep = j < nc ? j / 18 + 1 : j + 1;
        }
        if (caso == 1 && ep > 89) ep += 100;
        if (caso == 2 && ep > 90) ep += 100;
        k += (size_t)snprintf(arq + k, cap - k,
          "%s{\"id\":\"file%d\",\"season\":%d,\"episode\":%d,\"name\":\"One.Piece.%d.mkv\"}",
          i ? "," : "", i, t, ep, ep);
      }
      snprintf(arq + k, cap - k, "]}}");
      assert(k < cap - 8);
      for (via = 0; via < 2; via++) {
        nFake = 0; nRotas = 0; addonMeta = 1; addonTipo = "/series/";
        fakeMetaExterno = 0; fakeSoCinemeta = 0; fakeIdioma = "en-US";
        cineSerie = cine; addonResp = arq;
        if (via) {
          addonMeta = 0; addonResp = NULL; nFake = 1;
          fake[0] = (FakeAddon){ "Fontes", "https://fx.test/SEGREDO", "org.fx", "tt", 0 };
          rota("fx.test/SEGREDO/meta/series/tt0388629.json", arq);
        }
        limparCacheMeta();
        if (via) catalogoDe("tt0388629", "series", "One Piece", "org.fx");
        else catalogoCom("tt0388629", "series", "One Piece");
        abrir();
        printf("One Piece caso=%d catalogo-primeiro=%d: %d episodios, %d temporadas\n",
               caso, via, cat_n_episodios(0), cat_item(0)->nTemporadas);
        assert(cat_n_episodios(0) == (caso < 2 || caso >= 4 ? nc : na));
        assert(cat_item(0)->nTemporadas == (caso && caso < 4 ? 1 : 18));
        if (caso < 2 || caso >= 4) {
          assert(!strcmp(cat_episodio(0, 0)->vid, "tt0388629:1:1"));
          assert(!strstr(cat_episodio(0, nc - 1)->nome, ".mkv"));
        }
      }
    }
    free(cine); free(arq);
    puts("ok  temporadas e cobertura de 90% preservam a numeracao do catalogo");
  }
  // --- LISTA DE ARQUIVOS DE ADDON DE FONTE NO LUGAR DOS EPISODIOS (2.0.3.1) ---
  // Relato de TV LG na 2.0.2 (Breaking Bad): um addon de FONTES respondia o
  // /meta com um video por ARQUIVO de torrent — 18529 entradas, quase todas
  // repeticoes dos mesmos (temporada, episodio). Ganhava da lista de 62 por
  // contagem BRUTA, o corte de 1200 valia sobre as repeticoes e a pagina
  // mostrava "Episodio 1" em todos os cartoes, "1200 de 1200 assistidos".
  {
    static const int porTemp[5] = { 7, 13, 13, 13, 16 };   // 62 episodios
    size_t cap = 4u << 20, k = 0;
    char *cine = malloc(cap), *arq = malloc(cap), *maior = malloc(cap);
    int t, ep, i, total = 0;
    assert(cine && arq && maior);
    k = (size_t)snprintf(cine, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"description\":\"S.\",\"videos\":[");
    for (t = 1; t <= 5; t++)
      for (ep = 1; ep <= porTemp[t - 1]; ep++, total++)
        k += (size_t)snprintf(cine + k, cap - k,
                              "%s{\"id\":\"tt13293588:%d:%d\",\"season\":%d,\"episode\":%d,"
                              "\"name\":\"T%dE%d\"}", total ? "," : "", t, ep, t, ep, t, ep);
    snprintf(cine + k, cap - k, "]}}");
    assert(total == 62);
    // 18000 arquivos, todos repeticoes de T1E1..T1E7.
    k = (size_t)snprintf(arq, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"videos\":[");
    for (i = 0; i < 18000; i++)
      k += (size_t)snprintf(arq + k, cap - k,
                            "%s{\"id\":\"arq%d\",\"season\":1,\"episode\":%d,"
                            "\"name\":\"Serie.S01E%02d.1080p.mkv\"}", i ? "," : "",
                            i, i % 7 + 1, i % 7 + 1);
    snprintf(arq + k, cap - k, "]}}");
    assert(k < cap - 8);

    nFake = 0; nRotas = 0; addonMeta = 1; addonTipo = "/series/";
    fakeMetaExterno = 0; fakeSoCinemeta = 0; fakeIdioma = "en-US";
    cineSerie = cine;
    addonResp = arq;
    limparCacheMeta();
    catalogoCom("tt13293588", "series", "Serie");
    abrir();
    assert(pediu("addon.test/SEGREDO/meta/series/tt13293588.json"));
    printf("lista de arquivos: %d episodios, %d abas\n",
           cat_n_episodios(0), cat_item(0)->nTemporadas);
    assert(cat_n_episodios(0) == 62);
    assert(cat_item(0)->nTemporadas == 5);
    assert(cat_episodio(0, 0)->episodio == 1 && cat_episodio(0, 1)->episodio == 2);
    assert(cat_episodio(0, 61)->temporada == 5 && cat_episodio(0, 61)->episodio == 16);
    puts("ok  lista de arquivos de addon de fonte nao troca a lista de episodios");
    assert(desc_meta_n_episodios(cine) == 62);
    assert(desc_meta_n_episodios(arq) == 7);          // distintos, nao 18000

    // O corte de VIDEOS_MAX vale sobre episodios DISTINTOS: 1300 repeticoes de
    // T1E1 na frente nao gastam as vagas dos outros.
    k = (size_t)snprintf(maior, cap, "{\"videos\":[");
    for (i = 0; i < 1300; i++)
      k += (size_t)snprintf(maior + k, cap - k, "%s{\"id\":\"r%d\",\"season\":1,\"episode\":1}",
                            i ? "," : "", i);
    for (ep = 2; ep <= 80; ep++)
      k += (size_t)snprintf(maior + k, cap - k, ",{\"id\":\"e%d\",\"season\":1,\"episode\":%d}", ep, ep);
    snprintf(maior + k, cap - k, "]}");
    {
      CatEp *g = malloc(sizeof(CatEp) * VIDEOS_MAX);
      assert(g);
      n = parsearEpisodios(maior, g, VIDEOS_MAX);
      assert(n == 80);
      assert(!strcmp(g[0].vid, "r0") && g[79].episodio == 80);
      free(g);
    }
    puts("ok  repeticoes nao gastam as vagas dos episodios distintos");

    // Addon LEGITIMO com mais episodios DISTINTOS (e algumas repeticoes, #328)
    // continua ganhando: 62 + a temporada 6 com 10, cada episodio duas vezes.
    k = (size_t)snprintf(maior, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"videos\":[");
    total = 0;
    for (t = 1; t <= 6; t++)
      for (ep = 1; ep <= (t == 6 ? 10 : porTemp[t - 1]); ep++)
        for (i = 0; i < 2; i++, total++)
          k += (size_t)snprintf(maior + k, cap - k,
                                "%s{\"id\":\"m%d\",\"season\":%d,\"episode\":%d,\"name\":\"M\"}",
                                total ? "," : "", total, t, ep);
    snprintf(maior + k, cap - k, "]}}");
    assert(desc_meta_n_episodios(maior) == 72);
    addonResp = maior;
    limparCacheMeta();
    catalogoCom("tt13293588", "series", "Serie");
    abrir();
    assert(cat_n_episodios(0) == 72);
    assert(cat_item(0)->nTemporadas == 6);
    puts("ok  addon com mais episodios distintos continua ganhando");
    // SEM MEMORIA PARA CRESCER O CONJUNTO, a repeticao continua sendo repeticao:
    // 600 episodios, cada um duas vezes. O conjunto de 1024 casas cabe os 600;
    // o crescimento (pedido ao passar de meia carga) falha sempre.
    k = (size_t)snprintf(maior, cap, "{\"videos\":[");
    for (i = 0; i < 1200; i++)
      k += (size_t)snprintf(maior + k, cap - k, "%s{\"id\":\"f%d\",\"season\":1,\"episode\":%d}",
                            i ? "," : "", i, i % 600 + 1);
    snprintf(maior + k, cap - k, "]}");
    callocFalha = 1; callocNegados = 0;
    n = desc_meta_n_episodios(maior);
    printf("sem memoria para crescer: %d distintos (%d callocs negados)\n", n, callocNegados);
    assert(callocNegados > 0);                 // a falha foi mesmo injetada
    assert(n == 600);
    {
      CatEp *g = malloc(sizeof(CatEp) * VIDEOS_MAX);
      assert(g);
      n = parsearEpisodios(maior, g, VIDEOS_MAX);
      assert(n == 600);
      assert(!strcmp(g[0].vid, "f0") && g[599].episodio == 600);
      free(g);
    }
    callocFalha = 0;
    puts("ok  sem memoria para crescer o conjunto, repeticao continua repeticao");

    // CATALOGO PRIMEIRO: a mesma lista de arquivos, agora na ficha do addon que
    // PUBLICOU o item (titulo aberto de um catalogo dele). A base manda no
    // texto, mas a lista de arquivos dela nao pode esconder os 62 do Cinemeta.
    addonMeta = 0; addonResp = NULL;
    nFake = 1;
    fake[0] = (FakeAddon){ "Fontes", "https://fx.test/SEGREDO", "org.fx", "tt", 0 };
    nRotas = 0;
    rota("fx.test/SEGREDO/meta/series/tt13293588.json", arq);
    limparCacheMeta();
    catalogoDe("tt13293588", "series", "Serie", "org.fx");
    abrir();
    assert(pediu("fx.test/SEGREDO/meta/series/tt13293588.json"));
    assert(pediu("cinemeta"));
    printf("catalogo primeiro, lista de arquivos: %d episodios, %d abas\n",
           cat_n_episodios(0), cat_item(0)->nTemporadas);
    assert(cat_n_episodios(0) == 62);
    assert(cat_item(0)->nTemporadas == 5);
    assert(cat_episodio(0, 61)->temporada == 5 && cat_episodio(0, 61)->episodio == 16);
    for (i = 0; i < 62; i++) assert(!strstr(cat_episodio(0, i)->nome, ".mkv"));
    assert(!strcmp(cat_episodio(0, 0)->nome, "T1E1"));
    puts("ok  catalogo primeiro: lista de arquivos da base nao esconde a lista real");
    // LISTA DE ARQUIVOS QUE SABE MAIS EPISODIOS NAO E JOGADA FORA: 20 episodios
    // distintos em 4 variantes cada (80 videos, "cara" de lista de arquivos)
    // contra 12 do Cinemeta. Sem a recusa em bloco os 8 a mais aparecem; nos 12
    // que os dois tem, o nome e o do Cinemeta e nao o nome de arquivo.
    k = (size_t)snprintf(cine, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"description\":\"S.\",\"videos\":[");
    for (ep = 1; ep <= 12; ep++)
      k += (size_t)snprintf(cine + k, cap - k,
                            "%s{\"id\":\"tt13293588:1:%d\",\"season\":1,\"episode\":%d,"
                            "\"name\":\"C%d\"}", ep > 1 ? "," : "", ep, ep, ep);
    snprintf(cine + k, cap - k, "]}}");
    k = (size_t)snprintf(maior, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"videos\":[");
    for (i = 0; i < 80; i++)
      k += (size_t)snprintf(maior + k, cap - k,
                            "%s{\"id\":\"v%d\",\"season\":1,\"episode\":%d,"
                            "\"name\":\"Serie.S01E%02d.v%d.mkv\"}", i ? "," : "",
                            i, i % 20 + 1, i % 20 + 1, i / 20);
    snprintf(maior + k, cap - k, "]}}");
    assert(desc_meta_n_episodios(cine) == 12 && desc_meta_n_episodios(maior) == 20);
    nFake = 0; nRotas = 0; addonMeta = 1; addonTipo = "/series/";
    cineSerie = cine; addonResp = maior;
    limparCacheMeta();
    catalogoCom("tt13293588", "series", "Serie");
    abrir();
    printf("20x4 contra 12: %d episodios\n", cat_n_episodios(0));
    assert(cat_n_episodios(0) == 20);
    assert(cat_episodio(0, 19)->episodio == 20);
    assert(!strcmp(cat_episodio(0, 0)->nome, "C1") && !strcmp(cat_episodio(0, 11)->nome, "C12"));
    puts("ok  lista de arquivos com mais episodios distintos nao e descartada");
    // Upstream 70d8d67ab: a longer (not file-list) addon list without text
    // preference still takes Cinemeta titles where it only has filenames.
    // Addon video IDs and season 0 stay intact.
  { char *longa = malloc(cap); assert(longa);
    k = (size_t)snprintf(longa, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"videos\":["
                         "{\"id\":\"sp1\",\"season\":0,\"episode\":1,\"name\":\"Special\"}");
    for (ep = 1; ep <= 14; ep++)
      k += (size_t)snprintf(longa + k, cap - k,
                            ",{\"id\":\"a%d\",\"season\":1,\"episode\":%d,"
                            "\"name\":\"Serie.S01E%02d.1080p.x264.mkv\"}", ep, ep, ep);
    snprintf(longa + k, cap - k, "]}}");
    nFake = 0; nRotas = 0; addonMeta = 1; addonTipo = "/series/";
    cineSerie = cine; addonResp = longa;
    limparCacheMeta();
    catalogoCom("tt13293588", "series", "Serie");
    abrir();
    assert(cat_n_episodios(0) == 15);
    assert(cat_episodio(0, 0)->temporada == 0 && !strcmp(cat_episodio(0, 0)->vid, "sp1"));
    assert(!strcmp(cat_episodio(0, 1)->nome, "C1") && !strcmp(cat_episodio(0, 1)->vid, "a1"));
    assert(!strcmp(cat_episodio(0, 12)->nome, "C12") && !strcmp(cat_episodio(0, 12)->vid, "a12"));
    assert(!strcmp(cat_episodio(0, 14)->nome, "Episódio 14") && !strcmp(cat_episodio(0, 14)->vid, "a14"));
    assert(cat_item(0)->nTemporadas == 2 && cat_item(0)->temporadas[0] == 1 &&
           cat_item(0)->temporadas[1] == 0 && !cat_item(0)->epsParciais);
    puts("ok  addon list: filename names take Cinemeta titles, ids and season 0 kept");
    // Cache refresh: a fresh entry is reused; an expired one is fetched again
    // and the refreshed names land without changing ids or season tabs.
    { char *p = strstr(longa, "Serie.S01E14.1080p.x264.mkv"); assert(p);
      memcpy(p, "Fresh14\"}", 9); p[9] = ']'; p[10] = '}'; p[11] = '}'; p[12] = 0; }
    catalogoCom("tt13293588", "series", "Serie");
    abrir();
    assert(!pediu("/meta/series/tt13293588.json"));
    assert(!strcmp(cat_episodio(0, 14)->nome, "Episódio 14"));
    for (i = 0; i < META_CACHE_N; i++)
      if (metaCache[i].corpo) metaCache[i].quando -= META_TTL_S + 1;
    catalogoCom("tt13293588", "series", "Serie");
    abrir();
    assert(pediu("/meta/series/tt13293588.json"));
    assert(cat_n_episodios(0) == 15);
    assert(!strcmp(cat_episodio(0, 14)->nome, "Fresh14") && !strcmp(cat_episodio(0, 14)->vid, "a14"));
    assert(!strcmp(cat_episodio(0, 1)->nome, "C1") && !strcmp(cat_episodio(0, 0)->vid, "sp1"));
    assert(cat_item(0)->nTemporadas == 2 && cat_item(0)->temporadas[1] == 0);
    puts("ok  meta cache: fresh entry reused, expired entry refreshed, ids/tabs kept");
    addonResp = NULL; free(longa); }
    // O mesmo pelo caminho do catalogo primeiro.
    addonMeta = 0; addonResp = NULL;
    nFake = 1;
    fake[0] = (FakeAddon){ "Fontes", "https://fx.test/SEGREDO", "org.fx", "tt", 0 };
    rota("fx.test/SEGREDO/meta/series/tt13293588.json", maior);
    limparCacheMeta();
    catalogoDe("tt13293588", "series", "Serie", "org.fx");
    abrir();
    assert(cat_n_episodios(0) == 20);
    assert(!strcmp(cat_episodio(0, 0)->nome, "C1") && !strcmp(cat_episodio(0, 19)->nome, "Episódio 20"));
    puts("ok  catalogo primeiro: lista de arquivos com mais episodios fica");
    // EMPATE NAO E DA LISTA DE ARQUIVOS, nem com a preferencia pela ficha do
    // addon ligada: 12 distintos x 5 variantes contra os mesmos 12.
    k = (size_t)snprintf(maior, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"videos\":[");
    for (i = 0; i < 60; i++)
      k += (size_t)snprintf(maior + k, cap - k,
                            "%s{\"id\":\"w%d\",\"season\":1,\"episode\":%d,"
                            "\"name\":\"Serie.S01E%02d.w%d.mkv\"}", i ? "," : "",
                            i, i % 12 + 1, i % 12 + 1, i / 12);
    snprintf(maior + k, cap - k, "]}}");
    nFake = 0; nRotas = 0; addonMeta = 1; addonResp = maior; fakeMetaExterno = 1;
    limparCacheMeta();
    catalogoCom("tt13293588", "series", "Serie");
    abrir();
    assert(cat_n_episodios(0) == 12);
    for (i = 0; i < 12; i++) assert(!strstr(cat_episodio(0, i)->nome, ".mkv"));
    assert(!strcmp(cat_episodio(0, 0)->vid, "tt13293588:1:1"));
    fakeMetaExterno = 0;
    puts("ok  empate: a lista de arquivos nao ganha nem com a ficha do addon preferida");
    // SEM ADDON DE METADADOS, a preferencia nao muda nada (#794 volta 3): a
    // serie cai no provedor geral e publica os 3 episodios, como na base.
    nFake = 0; nRotas = 0; addonMeta = 0; addonResp = NULL; cineSerie = NULL;
    fakeMetaExterno = 1;
    limparCacheMeta();
    catalogoCom("tt13293588", "series", "Serie");
    abrir();
    assert(cat_n_episodios(0) == 3);
    assert(!strcmp(cat_episodio(0, 0)->vid, "tt13293588:1:1"));
    assert(pediu("cinemeta"));
    puts("ok  sem addon: a preferencia cai no provedor geral");
    // "USAR SEMPRE O CINEMETA" desliga a preferencia: com addon instalado o
    // caminho e o da base — provedor geral consultado e a arbitragem de
    // sempre, que a lista maior do addon (5 contra 3) vence.
    addonMeta = 1; addonResp = META_SERIE_ADDON; addonTipo = "/series/";
    fakeSoCinemeta = 1;
    limparCacheMeta();
    catalogoCom("tt13293588", "series", "Serie");
    abrir();
    assert(cat_n_episodios(0) == 5);
    assert(pediu("cinemeta"));
    fakeSoCinemeta = 0;
    puts("ok  usar sempre o Cinemeta: a preferencia nao pula o provedor geral");
    // COM UM ADDON QUE RESPONDE a lista e so a dele: nem o Cinemeta nem o
    // catalogo do Nuvio sao consultados.
    nPedidos = 0;
    limparCacheMeta();
    catalogoCom("tt13293588", "series", "Serie");
    abrir();
    assert(cat_n_episodios(0) == 5);
    assert(pediu("addon.test/SEGREDO/meta/series/tt13293588.json"));
    assert(!pediu("cinemeta") && !pediu("catalog.nuvio"));
    fakeMetaExterno = 0; addonMeta = 0; addonResp = NULL;
    puts("ok  addon responde: a lista e so a dele, sem Cinemeta nem Nuvio");
    // CATALOGO DO ADDON + PREFERENCIA + FICHA SEM EPISODIOS (#794 r2): o addon
    // que publicou o titulo responde so nome e sinopse ("videos": []). A
    // preferencia nao pode pular o provedor geral — senao a ficha abre sem
    // episodio nenhum. Com episodios de verdade ela continua exclusiva.
    {
      static const char vazio[] =
        "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\",\"name\":\"Serie\","
        "\"description\":\"So ficha.\",\"videos\":[]}}";
      nFake = 1; nRotas = 0;
      fake[0] = (FakeAddon){ "Meta", "https://mt.test/SEGREDO", "org.mt", "tt", 0 };
      rota("mt.test/SEGREDO/meta/series/tt13293588.json", vazio);
      fakeMetaExterno = 1; cineSerie = NULL;
      limparCacheMeta();
      catalogoDe("tt13293588", "series", "Serie", "org.mt");
      abrir();
      printf("catalogo vazio + preferencia: %d episodios, provedor geral %s\n",
             cat_n_episodios(0),
             (pediu("cinemeta") || pediu("catalog.nuvio")) ? "consultado" : "PULADO");
      assert(cat_n_episodios(0) == 3);
      assert(!strcmp(cat_episodio(0, 0)->vid, "tt13293588:1:1"));
      assert(pediu("cinemeta") || pediu("catalog.nuvio"));
      // Com episodios de verdade na ficha a preferencia segue exclusiva.
      nRotas = 0;
      rota("mt.test/SEGREDO/meta/series/tt13293588.json", META_SERIE_ADDON);
      nPedidos = 0;
      limparCacheMeta();
      catalogoDe("tt13293588", "series", "Serie", "org.mt");
      abrir();
      assert(cat_n_episodios(0) == 5);
      assert(!pediu("cinemeta") && !pediu("catalog.nuvio"));
      fakeMetaExterno = 0; nFake = 0; nRotas = 0;
      puts("ok  catalogo + preferencia: ficha sem episodios nao pula o provedor geral");
    }
    // EMPATE ACIMA DO TETO: o Cinemeta e a lista de arquivos conhecem os
    // mesmos pares, mas so VIDEOS_MAX sao publicados. A contagem vem do corpo
    // inteiro, e a lista continua a do Cinemeta.
    int acima = VIDEOS_MAX + 100;
    k = (size_t)snprintf(cine, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"description\":\"S.\",\"videos\":[");
    for (ep = 1; ep <= acima; ep++)
      k += (size_t)snprintf(cine + k, cap - k,
                            "%s{\"id\":\"tt13293588:1:%d\",\"season\":1,\"episode\":%d,"
                            "\"name\":\"C%d\"}", ep > 1 ? "," : "", ep, ep, ep);
    snprintf(cine + k, cap - k, "]}}");
    k = (size_t)snprintf(maior, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"videos\":[");
    for (i = 0; i < acima * 4; i++)
      k += (size_t)snprintf(maior + k, cap - k,
                            "%s{\"id\":\"z%d\",\"season\":1,\"episode\":%d,"
                            "\"name\":\"Serie.S01E%04d.z%d.mkv\"}", i ? "," : "",
                            i, acima - i % acima, acima - i % acima, i / acima);
    snprintf(maior + k, cap - k, "]}}");
    assert(k < cap - 8);
    assert(desc_meta_n_episodios(cine) == acima && desc_meta_n_episodios(maior) == acima);
    nFake = 0; nRotas = 0; addonMeta = 1; addonTipo = "/series/";
    cineSerie = cine; addonResp = maior;
    limparCacheMeta();
    catalogoCom("tt13293588", "series", "Serie");
    abrir();
    printf("%d contra %dx4: primeiro id %s, %d episodios\n",
           acima, acima, cat_episodio(0, 0)->vid, cat_n_episodios(0));
    assert(cat_n_episodios(0) == VIDEOS_MAX);
    assert(!strcmp(cat_episodio(0, 0)->vid, "tt13293588:1:1"));
    assert(cat_episodio(0, VIDEOS_MAX - 1)->episodio == VIDEOS_MAX);
    assert(!strcmp(cat_episodio(0, 0)->nome, "C1"));
    puts("ok  empate acima do teto: a lista de arquivos nao ganha");
    // CATALOGO PRIMEIRO COM TRES FONTES: lista de arquivos (20), addon de
    // verdade (10), Cinemeta (12). A lista de arquivos fica (sabe mais), e o
    // nome do Cinemeta tem de entrar por cima mesmo com outra fonte no meio.
    {
      static char dez[2048], doze[2048];
      size_t kk;
      kk = (size_t)snprintf(dez, sizeof dez, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                            "\"name\":\"Serie\",\"videos\":[");
      for (ep = 1; ep <= 10; ep++)
        kk += (size_t)snprintf(dez + kk, sizeof dez - kk,
                               "%s{\"id\":\"d%d\",\"season\":1,\"episode\":%d,\"name\":\"D%d\"}",
                               ep > 1 ? "," : "", ep, ep, ep);
      snprintf(dez + kk, sizeof dez - kk, "]}}");
      kk = (size_t)snprintf(doze, sizeof doze, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                            "\"name\":\"Serie\",\"description\":\"S.\",\"videos\":[");
      for (ep = 1; ep <= 12; ep++)
        kk += (size_t)snprintf(doze + kk, sizeof doze - kk,
                               "%s{\"id\":\"tt13293588:1:%d\",\"season\":1,\"episode\":%d,"
                               "\"name\":\"C%d\"}", ep > 1 ? "," : "", ep, ep, ep);
      snprintf(doze + kk, sizeof doze - kk, "]}}");
      k = (size_t)snprintf(maior, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                           "\"name\":\"Serie\",\"videos\":[");
      for (i = 0; i < 80; i++)
        k += (size_t)snprintf(maior + k, cap - k,
                              "%s{\"id\":\"v%d\",\"season\":1,\"episode\":%d,"
                              "\"name\":\"Serie.S01E%02d.v%d.mkv\"}", i ? "," : "",
                              i, i % 20 + 1, i % 20 + 1, i / 20);
      snprintf(maior + k, cap - k, "]}}");
      addonMeta = 0; addonResp = NULL; cineSerie = doze;
      nFake = 2;
      fake[0] = (FakeAddon){ "Fontes", "https://fx.test/SEGREDO", "org.fx", "tt", 0 };
      fake[1] = (FakeAddon){ "Meta", "https://mt.test/SEGREDO", "org.mt", "tt", 0 };
      nRotas = 0;
      rota("fx.test/SEGREDO/meta/series/tt13293588.json", maior);
      rota("mt.test/SEGREDO/meta/series/tt13293588.json", dez);
      limparCacheMeta();
      catalogoDe("tt13293588", "series", "Serie", "org.fx");
      abrir();
      assert(pediu("fx.test/SEGREDO") && pediu("mt.test/SEGREDO") && pediu("cinemeta"));
      printf("tres fontes: %d episodios, primeiro nome '%s'\n",
             cat_n_episodios(0), cat_episodio(0, 0)->nome);
      assert(cat_n_episodios(0) == 20);
      for (i = 0; i < 12; i++) {
        char esperado[8];
        snprintf(esperado, sizeof esperado, "C%d", i + 1);
        assert(!strcmp(cat_episodio(0, i)->nome, esperado));
      }
      puts("ok  catalogo primeiro: nome do Cinemeta entra mesmo com outra fonte no meio");
      // LISTA DE ARQUIVOS QUE FICOU NAO E "TEXTO DO ADDON": o retorno liga
      // DESC_EPT_SO_VAZIO em buscarEps e o TMDB deixaria de traduzir os nomes,
      // prendendo os 8 nomes de arquivo que sobraram. Como em episodiosDoAddon,
      // devolve 0. (Chamada direta: o que se mede e o retorno.)
      {
        MetaFontes m3;
        TempsPub tp3 = {0};
        int r;
        memset(&m3, 0, sizeof m3);
        m3.corpo[0] = maior; m3.corpo[1] = dez; m3.corpo[2] = doze;
        m3.cine[2] = 1; m3.n = 3;
        r = episodiosDoCatalogo(0, "Serie", "tt13293588", &m3, &tp3);
        printf("lista de arquivos que ficou: retorno %d\n", r);
        assert(cat_n_episodios(0) == 20);
        assert(r == 0);
        // A base de verdade continua contando como episodios do addon.
        memset(&m3, 0, sizeof m3);
        memset(&tp3, 0, sizeof tp3);
        m3.corpo[0] = dez; m3.corpo[1] = doze; m3.cine[1] = 1; m3.n = 2;
        r = episodiosDoCatalogo(0, "Serie", "tt13293588", &m3, &tp3);
        assert(cat_n_episodios(0) == 12 && r == 1);
        assert(!strcmp(cat_episodio(0, 0)->nome, "D1"));
      }
      puts("ok  catalogo primeiro: lista de arquivos que ficou nao trava a traducao do TMDB");
    }
    // CONJUNTO CHEIO sem poder crescer: 1023 pares entram (sobra a casa vazia
    // que encerra a sondagem), repetido segue repetido, e o 1024o nunca e
    // guardado — responde "novo" toda vez, sem travar a procura.
    {
      EpSet cj = { 0 };
      callocFalha = 1; callocNegados = 0;
      for (i = 1; i <= 1023; i++) assert(epSetNovo(&cj, 1, i) == 1);
      assert(cj.n == 1023 && cj.cap == 1024);
      for (i = 1; i <= 1023; i++) assert(epSetNovo(&cj, 1, i) == 0);
      for (i = 0; i < 3; i++) assert(epSetNovo(&cj, 1, 1024) == 1);
      assert(epSetNovo(&cj, 2, 1) == 1);
      assert(cj.n == 1023 && cj.cap == 1024 && callocNegados > 0);
      assert(epSetNovo(&cj, 1, 1023) == 0);
      free(cj.v);
      callocFalha = 0;
      puts("ok  conjunto cheio sem crescer: 1023 pares, o resto nao entra e a procura termina");
    }
    // #391: listas completas ficam nos dois caminhos; so os pares repetidos saem.
    {
      int falhas = 0;
      const struct { int cine, addon, porTemp, copias, inicio; } casos[] = {
        {100, 1180, 1180, 1, 0}, // Cinemeta incompleto, addon completo
        {0, 2001, 2001, 1, 0},  // addon-only S1E1..S1E2001
        {0, 2001, 2001, 2, 0},  // 4002 entradas, 2001 pares distintos
        {100, 1180, 1180, 2, 0}, // repeticoes nao escondem os 1180 episodios
        {0, 1, 1, 1, 2000},     // S1E2001 isolado tambem fica
        {176, 196, 1000, 56, 0}  // 10976 entradas, 196 pares distintos
      };
      for (unsigned caso = 0; caso < sizeof casos / sizeof casos[0]; caso++) {
        int nc = casos[caso].cine, na = casos[caso].addon;
        int porTemp = casos[caso].porTemp, copias = casos[caso].copias;
        char *corpos[] = { cine, maior };
        for (int fonte = 0; fonte < 2; fonte++) {
          int quantos = fonte ? na * copias : nc;
          k = (size_t)snprintf(corpos[fonte], cap,
            "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\",\"name\":\"Serie\",\"videos\":[");
          for (i = 0; i < quantos; i++) {
            int par = fonte ? i % na : i;
            t = par / porTemp + 1; ep = par % porTemp + 1;
            if (fonte) ep += casos[caso].inicio;
            k += (size_t)snprintf(corpos[fonte] + k, cap - k,
              "%s{\"id\":\"%c%d\",\"season\":%d,\"episode\":%d,\"name\":\"%c%d\"}",
              i ? "," : "", fonte ? 'A' : 'C', i, t, ep, fonte ? 'A' : 'C', i);
            assert(k < cap - 8);
          }
          snprintf(corpos[fonte] + k, cap - k, "]}}");
        }
        assert(desc_meta_n_episodios(cine) == nc);
        assert(desc_meta_n_episodios(maior) == na);
        for (int via = 0; via < 2; via++) {
          int esperado = na;
          nFake = 0; nRotas = 0; addonMeta = 1; addonTipo = "/series/";
          cineSerie = cine; addonResp = maior;
          if (via) {
            addonMeta = 0; addonResp = NULL; nFake = 1;
            fake[0] = (FakeAddon){ "Meta", "https://fx.test/SEGREDO", "org.fx", "tt", 0 };
            rota("fx.test/SEGREDO/meta/series/tt13293588.json", maior);
          }
          limparCacheMeta();
          if (via) catalogoDe("tt13293588", "series", "Serie", "org.fx");
          else catalogoCom("tt13293588", "series", "Serie");
          abrir();
          printf("#391 caso=%u catalogo-primeiro=%d: %d episodios, esperado=%d\n",
                 caso, via, cat_n_episodios(0), esperado);
          if (cat_n_episodios(0) != esperado) { falhas++; continue; }
          if (esperado) {
            int inicio = casos[caso].inicio;
            assert(cat_episodio(0, 0)->vid[0] == 'A');
            assert(cat_episodio(0, 0)->temporada == 1);
            assert(cat_episodio(0, 0)->episodio == inicio + 1);
            const CatEp *ultimo = cat_episodio(0, esperado - 1);
            assert(ultimo->temporada == (esperado - 1) / porTemp + 1);
            assert(ultimo->episodio == (esperado - 1) % porTemp + 1 + inicio);
          }
        }
      }
      fflush(stdout);
      assert(falhas == 0);
      puts("ok  #391: listas completas e deduplicacao nos dois caminhos");
    }
    nFake = 0; nRotas = 0;
    addonMeta = 0; cineSerie = NULL; addonResp = NULL;
    limparCacheMeta();
    free(cine); free(arq); free(maior);
  }
  puts("episodiosdup: ok");
  return 0;
}
