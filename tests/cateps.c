// AS FAIXAS DE EPISODIO SOBREVIVEM AO CATALOGO CRESCER.
//
// Os episodios nao vivem num vetor por titulo: ha um vetor unico e cada titulo
// guarda uma janela (epIni, epQtd) sobre ele — a mesma forma das fileiras, e a
// mesma armadilha. Quem acrescenta um titulo no FIM nao muda o indice de
// ninguem, entao nada ali pode invalidar a janela dos outros.
//
// Era o defeito: garantirFaixas() zerava o vetor inteiro e era chamada tambem
// por cat_acrescentar e pelo append em lote. Na TV isso apagava os episodios da
// serie ABERTA assim que a descoberta acrescentava qualquer titulo, e a secao
// de episodios desaparecia da pagina de detalhe — o D-pad passava de
// "Temporadas" direto para as abas, porque secao com zero colunas e
// intransponivel.
//
// O QUE ESTE TESTE PROVA:
//   1. acrescentar UM titulo nao mexe nos episodios de quem ja estava;
//   2. acrescentar um LOTE tambem nao;
//   3. o titulo NOVO nasce com zero episodios, e nao com lixo do realloc;
//   4. trocar o catalogo inteiro (cat_definir_tudo) INVALIDA tudo, porque ai os
//      indices mudaram de verdade — o contrario dos casos acima.
//
//   bash tests/cateps.sh
// Alem das faixas, o teste da NOTA DE EPISODIO do TMDB (issue #87) vive aqui:
// desc_tmdb_notas_temporada e pura sobre JSON + CatEp, e o jeito de linka-la e
// o mesmo de tests/colfileiras.c — descoberta.c INTEIRO entra por #include e
// os simbolos que ele pede (addons, rede, trakt...) viram dubles abaixo. Os
// cat_* NAO viram duble: este arquivo existe exatamente para exercitar os de
// verdade do catalogo.c.
int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_busca_cinemeta(void) { return 1; }
int ajustes_busca_nuvio(void) { return 0; }   // #311: Primeiro (padrao)
int ajustes_ocultar_filmes_vistos(void) { return 0; }    // #366: descoberta.c le o ajuste
#include "../src/descoberta.c"
#include "../src/progresso.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#define NV_TEST_SESSAO_PROPRIA
#define NV_TEST_SOCIAL_PROPRIO
#include "jellyfin_stub.inc"

Uint32 SDL_GetTicks(void) { return 0; }


// --- DUBLES: nenhum participa da regra, so fazem descoberta.c linkar ---------
// (o conjunto e o de tests/colfileiras.c, menos os cat_* — que catalogo.c ja
// traz — e mais os que este teste ja tinha)
int         ajustes_idioma_ingles(void) { return 0; }
int ajustes_idioma(void) { return 0; }
int ajustes_fonte_biblioteca(void) { return 0; }
int   ajustes_cw_concluido(void)         { return 90; }  // factory "watched" threshold
int   ajustes_cw_minimo(void)             { return 1; }
unsigned recomenda_geracao(void)         { return 0; }   // social generation: no social layer here
int   ajustes_cw_ordem(void)               { return 0; }   // Padrao (issue #127)
int   ajustes_cw_mostrar_nao_exibidos(void) { return 1; }
int   ajustes_cw_proximo(void) { return 1; }
const char *i18n(const char *s)         { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
const char *dados_dir(void)             { return ""; }
char *dados_caminho(char *dst, unsigned size, const char *name) {
  (void)dst; (void)size; (void)name; return NULL;
}
const char *sessao_usuario(void)        { return ""; }
static int perfilTeste = 1;
int         perfis_ativo(void)          { return perfilTeste; }
// Sessao fixa deste teste isolado; publicadores ainda validam e travam a
// identidade capturada. Uma geracao diferente nunca pode publicar.
static pthread_mutex_t sessaoTesteTrava = PTHREAD_MUTEX_INITIALIZER;
static uint64_t sessaoTesteGeracao = 1;
uint64_t perfis_sessao_geracao(void) { return sessaoTesteGeracao; }
int perfis_sessao_atual(uint64_t g) { return g == perfis_sessao_geracao(); }
int perfis_sessao_travar(uint64_t g) {
  pthread_mutex_lock(&sessaoTesteTrava);
  if (perfis_sessao_atual(g)) return 1;
  pthread_mutex_unlock(&sessaoTesteTrava);
  return 0;
}
void perfis_sessao_destravar(void) { pthread_mutex_unlock(&sessaoTesteTrava); }
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
int arte_reserva_episodios(const char *imdb, const char *corpo) { (void)imdb; (void)corpo; return 0; }
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
int   ajustes_tmdb_basico(void)            { return 0; }
int   ajustes_nota_episodio(int fonte)     { (void)fonte; return 1; }
int   ajustes_meta_externo(void)           { return 0; }
int   ajustes_meta_so_cinemeta(void)        { return 0; }
int   ajustes_fundo_addon(void)            { return 0; }
int   ajustes_logo_addon(void)             { return 0; }
int   addons_aceita_id(int i, const char *t, const char *id) { (void)i; (void)t; (void)id; return -1; }
int   ajustes_tmdb_arte(void)              { return 0; }
int   ajustes_tmdb_elenco(void)            { return 0; }
int   ajustes_tmdb_cw(void)                { return 0; }
int   ajustes_ocultar_nao_lancados(void)   { return 0; }
int   ajustes_itens_fileira(void)          { return 16; }
const char *ajustes_tmdb_idioma(void)      { return "pt-BR"; }
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
int fil_marcar_declaradas(const char *const *chaves, int n, int perfilDaLista) { (void)chaves; (void)n; (void)perfilDaLista; return 0; }
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
int trakt_enfeitar_lote_sessao(CatItem *s, int n, uint64_t sessao) {
  (void)sessao; return trakt_enfeitar_lote(s, n);
}
int   trakt_lista(const char *q, CatItem *s, int m) { (void)q; (void)s; (void)m; return 0; }
int   trakt_lista_cresc(const char *q, CatItem **s, int m) { (void)q; (void)s; (void)m; return 0; }
// O servico social proprio (recomenda.c) fica fora deste teste: a uniao e so o que o Trakt trouxe.
int   recomenda_social_mesclar(CatItem *i, int nTrakt, int max) { (void)i; (void)max; return nTrakt; }
int recomenda_social_mesclar_sessao(CatItem *i, int nTrakt, int max, uint64_t s) {
  return perfis_sessao_atual(s) ? recomenda_social_mesclar(i, nTrakt, max) : 0;
}
int   trakt_social(CatItem *s, int m)      { (void)s; (void)m; return 0; }
int trakt_cw_concluiu(const char *id, long long ms) { (void)id; (void)ms; return 0; }
int trakt_e_a_seguir(const char *id) { (void)id; return 0; }
int prog_oculto_soltar(const char *id, long long ms) { (void)id; (void)ms; return 0; }
int prog_oculto_vence(const char *id, long long ms) { (void)id; (void)ms; return 0; }
const char *nuvem_trakt_cliente(void)      { return ""; }
int   addons_n(void)                       { return 0; }
// O teste de metadados nao monta manifestos nem enumera contas remotas.
void addons_lista_travar(void) { assert(!"unexpected addon manifest enumeration"); }
void addons_lista_destravar(void) { assert(!"unexpected addon manifest enumeration"); }
const char *addons_base(int i)             { (void)i; return ""; }
const char *addons_id_manifesto(int i)     { (void)i; return ""; }
const char *addons_nome(int i)            { (void)i; return "addon"; }
unsigned addons_versao(void)             { return 1; }   // estatico no teste
int addons_sondado(int i) { (void)i; return 0; }
int addons_fornece(int i, int oque) { (void)i; (void)oque; return 1; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
void  addons_manifesto_lido(int i, const char *corpo) { (void)i; (void)corpo; }
int addons_identidade_recebida(const char *base, unsigned versao, uint64_t sessao, const char *corpo) {
  (void)base; (void)corpo;
  return versao == addons_versao() && perfis_sessao_atual(sessao);
}
int addons_manifesto_recebido(const char *base, unsigned versao, uint64_t sessao, const char *corpo) {
  return addons_identidade_recebida(base, versao, sessao, corpo);
}
static int metaRedePedidos, metaObservarCatalogo = -1, metaPublicadoAntesTMDB;
static int metaTrocarPerfilTMDB;
char *rede_baixar(const char *u, int t) {
  (void)t;
  metaRedePedidos++;
  if (metaObservarCatalogo >= 0 && strstr(u, "api.themoviedb.org")) {
    CatItem publicado;
    assert(cat_copiar_item(metaObservarCatalogo, &publicado));
    assert(publicado.nElenco == 1);
    assert(!strcmp(publicado.elenco[0].nome, "Tim Robbins"));
    assert(!strcmp(publicado.elenco[0].papel, "Andy Dufresne"));
    assert(!strcmp(publicado.elenco[0].foto, "https://addon.test/tim.jpg"));
    metaPublicadoAntesTMDB++;
    if (metaTrocarPerfilTMDB) {
      perfilTeste = 2;
      sessaoTesteGeracao++;
      snprintf(publicado.titulo, sizeof publicado.titulo, "Profile 2 keeps its metadata");
      snprintf(publicado.elenco[0].papel, sizeof publicado.elenco[0].papel, "Profile 2 role");
      snprintf(publicado.elenco[0].foto, sizeof publicado.elenco[0].foto, "https://profile2.test/keep.jpg");
      cat_definir_tudo(&publicado, 1, NULL, 0);
      metaTrocarPerfilTMDB = 0;
    }
  }
  return NULL;
}
char *rede_baixar_com(const char *u, int t, const char *const *c) {
  (void)c; return rede_baixar(u, t); }
char *rede_baixar_st(const char *u,int t,const char *const *c,int *status) {
  (void)c; if (status) *status=200; return rede_baixar(u,t);
}

// cat_acrescentar publica a reserva junto com o item. Este espião deixa o
// teste provar os dois campos sem depender de rede ou de uma implementação de
// TMDB: o registry próprio é exercitado em tests/artereserva.c.
static int reservaN;
static char reservaUrls[8][512];
static char reservaIds[8][24];
static int reservaTipos[8];
int arte_reserva_registrar(const char *url, const char *imdb, int poster) {
  if (reservaN < 8) {
    snprintf(reservaUrls[reservaN], sizeof reservaUrls[reservaN], "%s", url);
    snprintf(reservaIds[reservaN], sizeof reservaIds[reservaN], "%s", imdb);
    reservaTipos[reservaN] = poster;
  }
  reservaN++;
  return 1;
}

static CatItem itens[3];

static void montarCatalogo(void) {
  int i;
  memset(itens, 0, sizeof itens);
  for (i = 0; i < 3; i++) {
    snprintf(itens[i].titulo, sizeof itens[i].titulo, "T%d", i);
    snprintf(itens[i].imdb, sizeof itens[i].imdb, "tt%d", i);
    snprintf(itens[i].tipo, sizeof itens[i].tipo, "series");
  }
  cat_definir_tudo(itens, 3, NULL, 0);
}

// Tres episodios com nome reconhecivel, para a assercao ser sobre CONTEUDO e
// nao so sobre a contagem: um vetor com o tamanho certo e os dados de outra
// serie passaria por um teste que so contasse.
static void publicar(int alvo, const char *prefixo, int quantos) {
  CatEp eps[4];
  int i;
  memset(eps, 0, sizeof eps);
  for (i = 0; i < quantos; i++) {
    eps[i].temporada = 1;
    eps[i].episodio = i + 1;
    snprintf(eps[i].nome, sizeof eps[i].nome, "%s-E%d", prefixo, i + 1);
  }
  cat_definir_episodios(alvo, eps, quantos);
}

static int temEpisodios(int alvo, const char *prefixo, int quantos) {
  int i;
  if (cat_n_episodios(alvo) != quantos) return 0;
  for (i = 0; i < quantos; i++) {
    char esperado[64];
    const CatEp *e = cat_episodio(alvo, i);
    snprintf(esperado, sizeof esperado, "%s-E%d", prefixo, i + 1);
    if (!e || strcmp(e->nome, esperado)) return 0;
  }
  return 1;
}

// #528: os retratos ja vieram na ficha; nao dependem de uma chamada ao TMDB.
static void testarMetaElenco(void) {
  static CatItem it;
  const char *rico =
    "{\"meta\":{\"videos\":[{\"cast\":[\"Wrong Person\"]}],"
    "\"cast\":[\"Jacob Tremblay\",\"Shailene Woodley\"],\"app_extras\":{\"cast\":["
    "{\"name\":\"Shailene Woodley\",\"character\":\"Joanne\",\"photo\":\"https://addon.test/sw.jpg\"},"
    "{\"name\":\"Jacob Tremblay\",\"character\":\"Ted\",\"photo\":\"https://addon.test/jt.jpg\",\"id\":99}],"
    "\"directors\":[{\"name\":\"Director Name\",\"character\":\"Director Name\",\"photo\":\"https://addon.test/director.jpg\"}]}}}";
  memset(&it, 0, sizeof it);
  assert(desc_meta_elenco(rico, &it, 0) == 2);
  assert(!strcmp(it.elenco[0].nome, "Jacob Tremblay"));
  assert(!strcmp(it.elenco[0].papel, "Ted"));
  assert(!strcmp(it.elenco[0].foto, "https://addon.test/jt.jpg"));
  assert(!strcmp(it.elenco[1].papel, "Joanne"));
  assert(!strcmp(it.elenco[1].foto, "https://addon.test/sw.jpg"));
  assert(!it.elenco[0].tmdb && !it.elenco[1].tmdb); // id do addon nao e TMDB
  assert(!strcmp(it.direcao, "Director Name"));
  puts("ok  meta cast rico casa por nome e preserva a ordem da raiz (#528)");

  memset(&it, 0, sizeof it);
  assert(desc_meta_elenco("{\"meta\":{\"app_extras\":{\"cast\":["
    "{\"name\":\"Tim Robbins\",\"character\":\"Andy Dufresne\",\"photo\":\"https://image.tmdb.org/t/p/w500/tim.jpg\"},"
    "{\"name\":\"Morgan Freeman\",\"character\":\"Red\",\"photo\":null},"
    "{\"name\":\"Bob Gunton\"}]}}}", &it, 0) == 3);
  assert(!strcmp(it.elenco[0].papel, "Andy Dufresne") && strstr(it.elenco[0].foto, "/tim.jpg"));
  assert(!it.elenco[1].foto[0] && !it.elenco[2].foto[0]);
  puts("ok  AIOMetadata sem cast na raiz e foto null mantem iniciais (#528)");

  memset(&it, 0, sizeof it);
  desc_meta_elenco("{\"meta\":{\"cast\":[\"Zo\\u00eb Kravitz\",\"宮崎駿\",\"高畑勲\",\"J. K. Simmons\"]}}", &it, 0);
  desc_meta_elenco("{\"meta\":{\"app_extras\":{\"cast\":["
    "{\"name\":\"JK Simmons\",\"photo\":\"https://addon.test/jk.jpg\"},"
    "{\"name\":\"高畑勲\",\"photo\":\"https://addon.test/t.jpg\"},"
    "{\"name\":\"ZOE KRAVITZ\",\"photo\":\"https://addon.test/zoe.jpg\"},"
    "{\"name\":\"宮崎駿\",\"photo\":\"https://addon.test/m.jpg\"}]}}}", &it, 1);
  assert(it.nElenco == 4);
  assert(strstr(it.elenco[0].foto, "/zoe.jpg"));
  assert(strstr(it.elenco[1].foto, "/m.jpg") && strstr(it.elenco[2].foto, "/t.jpg"));
  assert(strstr(it.elenco[3].foto, "/jk.jpg"));
  puts("ok  nomes UTF-8, escapes, acentos e pontuacao sem trocar pessoas (#528)");

  memset(&it, 0, sizeof it);
  desc_meta_elenco(rico, &it, 0);
  it.elenco[0].tmdb = 3;
  desc_meta_elenco("{\"meta\":{\"cast\":[\"Shailene Woodley\",\"Someone Else\",\"Jacob Tremblay\"]}}", &it, 0);
  assert(it.nElenco == 3 && strstr(it.elenco[0].foto, "/sw.jpg"));
  assert(!it.elenco[1].foto[0] && !it.elenco[1].papel[0] && !it.elenco[1].tmdb);
  assert(strstr(it.elenco[2].foto, "/jt.jpg") && it.elenco[2].tmdb == 3);
  desc_meta_elenco("{\"meta\":{\"name\":\"No cast\"}}", &it, 0);
  assert(it.nElenco == 3 && strstr(it.elenco[2].foto, "/jt.jpg"));
  puts("ok  reabrir preserva campos da mesma pessoa sem herdar os de outra (#528)");

  memset(&it, 0, sizeof it);
  desc_meta_elenco(rico, &it, 0);
  desc_meta_elenco("{\"meta\":{\"app_extras\":{\"cast\":["
    "{\"name\":\"Jacob Tremblay\",\"character\":\"Other role\",\"photo\":\"https://other.test/other.jpg\"},"
    "{\"name\":\"New Person\",\"photo\":\"https://other.test/new.jpg\"}]}}}", &it, 1);
  assert(!strcmp(it.elenco[0].papel, "Ted") && strstr(it.elenco[0].foto, "/jt.jpg"));
  assert(it.nElenco == 3 && !strcmp(it.elenco[2].nome, "New Person"));
  const char *tmdb = "{\"cast\":[{\"name\":\"Shailene Woodley\",\"id\":2,\"character\":\"TMDB role\",\"profile_path\":\"/tmdb.jpg\"},"
    "{\"name\":\"Jacob Tremblay\",\"id\":3,\"character\":\"TMDB role\",\"profile_path\":\"/tmdb.jpg\"},"
    "{\"name\":\"New Person\",\"id\":4,\"character\":\"New role\"}]}";
  assert(tmdbElenco(tmdb, &it, 1) == 3);
  assert(!strcmp(it.elenco[0].papel, "Ted") && strstr(it.elenco[0].foto, "/jt.jpg") && it.elenco[0].tmdb == 3);
  assert(!strcmp(it.elenco[1].papel, "Joanne") && strstr(it.elenco[1].foto, "/sw.jpg") && it.elenco[1].tmdb == 2);
  assert(!strcmp(it.elenco[2].papel, "New role") && strstr(it.elenco[2].foto, "/new.jpg"));
  assert(desc_tmdb_elenco(tmdb, &it) == 3); // preferencia desligada mantem semantica TMDB
  assert(!strcmp(it.elenco[0].papel, "TMDB role") && strstr(it.elenco[0].foto, "/tmdb.jpg"));
  puts("ok  complemento nao troca campos primarios; TMDB ainda completa ids e lacunas (#528)");

  memset(&it, 0, sizeof it);
  assert(desc_meta_elenco("{\"meta\":{\"cast\":[\"\"]}}", &it, 0) == 0);
  desc_meta_elenco("{\"meta\":{\"app_extras\":{\"cast\":["
    "{\"name\":\"A\",\"photo\":\"file:///private/photo.jpg\"},"
    "{\"name\":\"B\",\"photo\":{\"url\":\"https://addon.test/b.jpg\"}},"
    "{\"name\":\"C\",\"photo\":\"https://addon.test/bad\\nphoto.jpg\"},"
    "{\"name\":\"D\",\"photo\":\"http://addon.test/d.jpg\"}]}}}", &it, 0);
  assert(it.nElenco == 4);
  assert(!it.elenco[0].foto[0] && !it.elenco[1].foto[0] && !it.elenco[2].foto[0]);
  assert(!strcmp(it.elenco[3].foto, "http://addon.test/d.jpg"));
  char grande[900];
  memset(grande, 'a', sizeof grande);
  const char *prefixo = "{\"meta\":{\"app_extras\":{\"cast\":[{\"name\":\"Long\",\"photo\":\"https://addon.test/";
  memcpy(grande, prefixo, strlen(prefixo));
  strcpy(grande + 800, "\"}]}}}");
  desc_meta_elenco(grande, &it, 0);
  assert(it.nElenco == 1 && !it.elenco[0].foto[0]);
  puts("ok  fotos ausentes, invalidas ou truncadas deixam iniciais (#528)");

  memset(&it, 0, sizeof it);
  char cheio[2048] = "{\"meta\":{\"cast\":[";
  for (int k = 0; k < CAT_ELENCO_MAX + 4; k++) {
    char nome[40]; snprintf(nome, sizeof nome, "%s\"Person %d\"", k ? "," : "", k);
    strcat(cheio, nome);
  }
  strcat(cheio, "],\"app_extras\":{\"directors\":[{\"name\":\"Director\",\"photo\":\"https://addon.test/director.jpg\"}]}}}");
  desc_meta_elenco(cheio, &it, 0);
  assert(it.nElenco == CAT_ELENCO_MAX && !strcmp(it.elenco[11].nome, "Person 11"));
  assert(!strcmp(it.direcao, "Director"));
  // O limite e a ordem antigos ficam; nenhum diretor desloca ator da fileira.
  desc_meta_elenco("{\"meta\":null}", &it, 0);
  assert(it.nElenco == CAT_ELENCO_MAX);
  desc_meta_elenco("{\"meta\":{\"app_extras\":{\"directors\":[{\"name\":\"Person 3\",\"character\":\"Person 3\",\"photo\":\"https://addon.test/p3.jpg\"}],"
    "\"writers\":[{\"name\":\"Person 8\",\"character\":\"Writer\",\"photo\":\"https://addon.test/p8.jpg\"}]}}}", &it, 0);
  assert(it.nElenco == CAT_ELENCO_MAX && !strcmp(it.elenco[11].nome, "Person 11"));
  assert(strstr(it.elenco[3].foto, "/p3.jpg") && !it.elenco[3].papel[0]);
  assert(strstr(it.elenco[8].foto, "/p8.jpg") && !it.elenco[8].papel[0]);
  puts("ok  teto e ordem de elenco existentes; direcao rica vira texto (#528)");


  // Uma entrada incompleta nao esconde o resto de um array valido.
  memset(&it, 0, sizeof it);
  assert(desc_meta_elenco("{\"meta\":{\"cast\":[null,42,true,[\"Nested\"],{},\"First\",false,\"Second\"],"
    "\"app_extras\":{\"cast\":[null,{\"name\":\"Second\",\"photo\":\"https://addon.test/second.jpg\"},"
    "[{}],false,{\"name\":\"First\",\"photo\":\"https://addon.test/first.jpg\"}]}}}", &it, 0) == 2);
  assert(!strcmp(it.elenco[0].nome, "First") && strstr(it.elenco[0].foto, "/first.jpg"));
  assert(!strcmp(it.elenco[1].nome, "Second") && strstr(it.elenco[1].foto, "/second.jpg"));
  puts("ok  membros invalidos nao descartam os retratos seguintes (#528)");

  // Nao apagar caracteres que distinguem duas pessoas com o mesmo nome latino.
  memset(&it, 0, sizeof it);
  assert(desc_meta_elenco("{\"meta\":{\"cast\":[\"李明 Lee\",\"王强 Lee\"],\"app_extras\":{\"cast\":["
    "{\"name\":\"王强 LEE\",\"character\":\"Second role\",\"photo\":\"https://addon.test/wang.jpg\"},"
    "{\"name\":\"李明 Lee\",\"character\":\"First role\",\"photo\":\"https://addon.test/li.jpg\"}]}}}", &it, 0) == 2);
  assert(!strcmp(it.elenco[0].papel, "First role") && strstr(it.elenco[0].foto, "/li.jpg"));
  assert(!strcmp(it.elenco[1].papel, "Second role") && strstr(it.elenco[1].foto, "/wang.jpg"));
  it.elenco[0].tmdb = 123;
  assert(desc_meta_elenco("{\"meta\":{\"cast\":[\"王强 Lee\"]}}", &it, 0) == 1);
  assert(!it.elenco[0].tmdb && strstr(it.elenco[0].foto, "/wang.jpg"));
  puts("ok  nomes mistos nao trocam fotos nem identidade TMDB (#528)");


  // Homonimos com ids conhecidos continuam pessoas diferentes, mesmo se o
  // TMDB devolver a ordem inversa. A preferencia desligada ainda usa o TMDB.
  memset(&it, 0, sizeof it);
  it.nElenco = 2;
  for (int k = 0; k < 2; k++) snprintf(it.elenco[k].nome, sizeof it.elenco[k].nome, "Same Name");
  it.elenco[0].tmdb = 11;
  it.elenco[1].tmdb = 22;
  assert(desc_tmdb_elenco("{\"cast\":["
    "{\"name\":\"Same Name\",\"id\":22,\"character\":\"Second\",\"profile_path\":\"/second.jpg\"},"
    "{\"name\":\"Same Name\",\"id\":11,\"character\":\"First\",\"profile_path\":\"/first.jpg\"}]}", &it) == 2);
  assert(it.nElenco == 2 && it.elenco[0].tmdb == 11 && it.elenco[1].tmdb == 22);
  assert(!strcmp(it.elenco[0].papel, "First") && strstr(it.elenco[0].foto, "/first.jpg"));
  assert(!strcmp(it.elenco[1].papel, "Second") && strstr(it.elenco[1].foto, "/second.jpg"));
  puts("ok  homonimos com ids TMDB distintos conservam a identidade (#528)");
}

// Mesmo pedido capturado usado pelo worker atual; a execucao fica sincrona
// para observar a publicacao antes do complemento TMDB sem criar outro fio.
static void executarMetaTeste(void) {
  EpPedido pedido = { .item = 0, .geracao = epGeracao,
                      .sessao = nv_perfil_sessao_ler() };
  assert(cat_copiar_item(0, &pedido.base));
  snprintf(pedido.id, sizeof pedido.id, "%s", pedido.base.imdb);
  pthread_once(&epPedidoOnce, epPedidoKeyCriar);
  assert(!pthread_setspecific(epPedidoKey, &pedido));
  fioEpVivo = 1;
  buscarEps(NULL);
  assert(!pthread_setspecific(epPedidoKey, NULL));
}

// #528: o worker real publica o meta antes da cauda TMDB falhar. A resposta
// vem do cache de metadados real; catalogo.c e os publicadores tambem sao reais.
static void testarMetaTMDBOffline(void) {
  static CatItem it, publicado;
  const char *corpo = "{\"meta\":{\"id\":\"tt0111161\",\"type\":\"movie\","
    "\"name\":\"The Shawshank Redemption\",\"description\":\"A real synopsis.\","
    "\"app_extras\":{\"cast\":[{\"name\":\"Tim Robbins\",\"character\":\"Andy Dufresne\","
    "\"photo\":\"https://addon.test/tim.jpg\"}]}}}";
  int antes = metaRedePedidos;
  char chave[96];
  memset(&it, 0, sizeof it);
  snprintf(it.imdb, sizeof it.imdb, "tt0111161");
  snprintf(it.tipo, sizeof it.tipo, "movie");
  snprintf(it.titulo, sizeof it.titulo, "The Shawshank Redemption");
  // Nome raso ja existente: a publicacao parcial antiga nao preenche fotos.
  it.nElenco = 1;
  snprintf(it.elenco[0].nome, sizeof it.elenco[0].nome, "Tim Robbins");
  cat_definir_tudo(&it, 1, NULL, 0);
  desc_meta_chave(chave, sizeof chave, "movie", it.imdb);
  metaCacheGuardar(chave, corpo);
  metaObservarCatalogo = 0;
  metaPublicadoAntesTMDB = 0;
  snprintf(tmdbChave, sizeof tmdbChave, "offline-test");
  executarMetaTeste();
  tmdbChave[0] = 0;
  metaObservarCatalogo = -1;
  assert(metaRedePedidos == antes + 1); // /find tentou e o duble devolveu NULL
  assert(metaPublicadoAntesTMDB == 1 && !fioEpVivo);
  assert(cat_copiar_item(0, &publicado));
  assert(publicado.nElenco == 1 && !strcmp(publicado.elenco[0].papel, "Andy Dufresne"));
  assert(!strcmp(publicado.elenco[0].foto, "https://addon.test/tim.jpg"));
  puts("ok  worker publica retrato antes do TMDB e conserva apos falha (#528)");


  // Mesma obra no mesmo indice, agora de outro perfil: nem o id do titulo
  // autoriza a cauda antiga a substituir os campos do novo dono.
  cat_definir_tudo(&it, 1, NULL, 0);
  metaObservarCatalogo = 0;
  metaTrocarPerfilTMDB = 1;
  snprintf(tmdbChave, sizeof tmdbChave, "offline-test");
  executarMetaTeste();
  tmdbChave[0] = 0;
  metaObservarCatalogo = -1;
  assert(perfilTeste == 2 && !metaTrocarPerfilTMDB);
  assert(cat_copiar_item(0, &publicado));
  assert(!strcmp(publicado.titulo, "Profile 2 keeps its metadata"));
  assert(!strcmp(publicado.elenco[0].papel, "Profile 2 role"));
  assert(!strcmp(publicado.elenco[0].foto, "https://profile2.test/keep.jpg"));
  // O corpo do perfil 1 continua no cache dele, nunca no do novo perfil.
  assert(metaCacheObter(chave) == NULL);
  perfilTeste = 1;
  sessaoTesteGeracao++;
  assert(descSessaoVincular(nv_perfil_sessao_ler()));
  char *guardado = metaCacheObter(chave);
  assert(guardado && !strcmp(guardado, corpo));
  free(guardado);
  puts("ok  cauda e cache de metadados respeitam o dono do perfil (#528)");
}

int main(void) {
  testarMetaElenco();
  testarMetaTMDBOffline();
  CatItem novo;
  CatItem lote[2];

  // A) UM TITULO ACRESCENTADO no fim nao apaga os episodios de quem ja estava.
  //    Este e o caso que a TV mostrava: a serie aberta perdia a secao inteira
  //    quando a descoberta trazia mais um titulo.
  montarCatalogo();
  publicar(0, "A", 3);
  publicar(2, "C", 2);
  assert(temEpisodios(0, "A", 3));
  memset(&novo, 0, sizeof novo);
  snprintf(novo.titulo, sizeof novo.titulo, "novo");
  { int i = cat_acrescentar(&novo);
    assert(i == 3);
    assert(temEpisodios(0, "A", 3));
    assert(temEpisodios(2, "C", 2));
    // E o recem-chegado nasce VAZIO. realloc nao inicializa o que cresceu: um
    // epQtd de lixo aqui faria cat_episodio ler fora do vetor de episodios.
    assert(cat_n_episodios(i) == 0); }
  puts("ok  acrescentar um titulo preserva as faixas e o novo nasce vazio");

  // B) O MESMO PARA O LOTE, que e outro caminho de codigo.
  montarCatalogo();
  publicar(1, "B", 4);
  memset(lote, 0, sizeof lote);
  snprintf(lote[0].titulo, sizeof lote[0].titulo, "L0");
  snprintf(lote[1].titulo, sizeof lote[1].titulo, "L1");
  { int idx[2];
    assert(cat_acrescentar_lote(lote, 2, idx) == 2);
    assert(temEpisodios(1, "B", 4));
    assert(cat_n_episodios(idx[0]) == 0 && cat_n_episodios(idx[1]) == 0); }
  puts("ok  acrescentar em lote preserva as faixas");

  // C) DOIS APPENDS SEGUIDOS. A cauda zerada da primeira vez nao pode ser
  //    zerada de novo por cima de episodios publicados depois dela.
  montarCatalogo();
  { int i = cat_acrescentar(&novo);
    publicar(i, "D", 2);
    assert(cat_acrescentar(&novo) >= 0);
    assert(temEpisodios(i, "D", 2)); }
  puts("ok  publicar depois de crescer e crescer de novo mantem o que foi publicado");

  // D) TROCAR O CATALOGO INTEIRO INVALIDA, e tem de invalidar: os indices
  //    passam a apontar para outros titulos. E o oposto exato de (A), e por
  //    isso os dois estao no mesmo arquivo — quem mexer num vai ler o outro.
  montarCatalogo();
  publicar(0, "A", 3);
  assert(temEpisodios(0, "A", 3));
  cat_definir_tudo(itens, 3, NULL, 0);
  assert(cat_n_episodios(0) == 0);
  puts("ok  trocar o catalogo inteiro invalida as faixas");

  // D2) #151: A MESMA SERIE EM DUAS FILEIRAS. Indice 0 e o card do CW, 2 e a
  //     copia de onde o detalhe abriu e a unica com episodios. Reabrir o
  //     player pela primeira copia (cat_indice_por_imdb) perdia a lista.
  montarCatalogo();
  snprintf(itens[2].imdb, sizeof itens[2].imdb, "tt0");
  cat_definir_tudo(itens, 3, NULL, 0);
  publicar(2, "S", 3);
  assert(cat_indice_por_imdb("tt0") == 0);               // o defeito: CW primeiro
  assert(cat_indice_titulo("tt0", 2) == 2);              // fica onde estava
  assert(cat_indice_titulo("tt0", 0) == 2);              // CW sem lista: vai a copia com
  assert(cat_indice_titulo("tt0", -1) == 2);
  assert(cat_indice_titulo("tt0", 1) == 2);              // 1 e outro titulo
  assert(cat_indice_titulo("tt0:1:3", 0) == 2);          // id com episodio grudado
  assert(cat_indice_titulo("tt1", 1) == 1);              // filme/sem lista: o proprio
  assert(cat_indice_titulo("tt9", 1) == -1);
  puts("ok  mesma serie em duas fileiras: a copia com episodios vence a do CW");

  // #391: uma lista longa nao perde os episodios depois do antigo teto de 1200.
  montarCatalogo();
  {
    CatEp *longa = calloc(2001, sizeof *longa);
    assert(longa);
    for (int i = 0; i < 2001; i++) {
      longa[i].temporada = 1;
      longa[i].episodio = i + 1;
    }
    cat_definir_episodios(0, longa, 2001);
    free(longa);
    assert(cat_n_episodios(0) == 2001);
    for (int i = 0; i < 2001; i++) {
      assert(cat_episodio(0, i)->temporada == 1);
      assert(cat_episodio(0, i)->episodio == i + 1);
    }
  }
  puts("ok  #391: catalogo preserva S1E1..S1E2001");

  // E) APPEND DE ARTE: o caminho unitário precisa registrar poster e fundo
  // exatamente como o lote e a publicação completa. Repetir o mesmo item é
  // permitido pelo catálogo, mas deve reusar a mesma chave no registry (a
  // deduplicação da chave é coberta pelo teste de arte); aqui garantimos que
  // não existe um caminho unitário que simplesmente esqueça a arte.
  montarCatalogo();
  { CatItem arte;
    int antes;
    memset(&arte, 0, sizeof arte);
    snprintf(arte.imdb, sizeof arte.imdb, "tt9000001");
    snprintf(arte.poster, sizeof arte.poster, "https://addon/poster/one.jpg");
    snprintf(arte.backdrop, sizeof arte.backdrop, "https://addon/background/one.jpg");
    reservaN = 0;
    assert(cat_acrescentar(&arte) == 3);
    assert(reservaN == 2);
    assert(!strcmp(reservaUrls[0], arte.poster) && !strcmp(reservaIds[0], arte.imdb) && reservaTipos[0] == 1);
    assert(!strcmp(reservaUrls[1], arte.backdrop) && !strcmp(reservaIds[1], arte.imdb) && reservaTipos[1] == 0);
    antes = reservaN;
    assert(cat_acrescentar(&arte) == 4);
    assert(reservaN == antes + 2);
  }
  puts("ok  append unitario registra poster/fundo em novo e repetido item");

  // F) NOTA TMDB POR EPISODIO (issue #87). JSON de temporada sintetico com
  //    tres episodios — um SEM vote_average — mais um de OUTRA temporada na
  //    lista, que nao pode ser tocado. Casa por episode_number; devolve
  //    quantos ganharam nota.
  { CatEp eps2[4];
    const char *json =
      "{\"episodes\":["
      "{\"episode_number\":1,\"vote_average\":8.3},"
      "{\"episode_number\":2},"
      "{\"episode_number\":3,\"vote_average\":7.0}]}";
    memset(eps2, 0, sizeof eps2);
    eps2[0].temporada = 1; eps2[0].episodio = 1;
    eps2[1].temporada = 1; eps2[1].episodio = 2;
    eps2[2].temporada = 1; eps2[2].episodio = 3;
    eps2[3].temporada = 2; eps2[3].episodio = 1;
    assert(desc_tmdb_notas_temporada(json, eps2, 4, 1) == 2);
    assert(eps2[0].nota == 83);          // 8.3 x10
    assert(eps2[1].nota == 0);           // sem vote_average: continua zero
    assert(eps2[2].nota == 70);
    assert(eps2[3].nota == 0);           // temporada 2 nao e da resposta
    // Pedindo a temporada errada nao preenche nada — e nao devolve nada.
    memset(eps2, 0, sizeof eps2);
    eps2[0].temporada = 1; eps2[0].episodio = 1;
    assert(desc_tmdb_notas_temporada(json, eps2, 1, 2) == 0);
    assert(eps2[0].nota == 0); }
  puts("ok  nota TMDB por episodio casa numero/temporada e ignora o que falta");

  // 8b. #150: a sinopse no idioma pedido entra; vazia deixa a do Cinemeta.
  { CatEp e3[2];
    const char *json =
      "{\"episodes\":["
      "{\"episode_number\":1,\"overview\":\"Walter descobre o c\\u00e2ncer.\"},"
      "{\"episode_number\":2,\"overview\":\"\"}]}";
    memset(e3, 0, sizeof e3);
    e3[0].temporada = 1; e3[0].episodio = 1;
    snprintf(e3[0].sinopse, sizeof e3[0].sinopse, "Walter learns he has cancer.");
    e3[1].temporada = 1; e3[1].episodio = 2;
    snprintf(e3[1].sinopse, sizeof e3[1].sinopse, "English only.");
    assert(desc_tmdb_notas_temporada(json, e3, 2, 1) == 1);
    assert(!strcmp(e3[0].sinopse, "Walter descobre o c\xc3\xa2ncer."));
    assert(!strcmp(e3[1].sinopse, "English only."));
    // Mesma resposta de novo: nada muda, nada conta.
    assert(desc_tmdb_notas_temporada(json, e3, 2, 1) == 0); }
  puts("ok  sinopse TMDB no idioma pedido; vazia mantem a do addon (#150)");

  // 9. #153: o elenco do TMDB casa POR NOME. A ordem das duas bases difere
  //    (o caso da foto: Tremblay e Woodley trocados), a grafia tambem (acento,
  //    caixa, ponto), e nome sem par nao ganha o rosto de ninguem.
  { static CatItem it;
    const char *json =
      "{\"cast\":["
      "{\"id\":1,\"name\":\"Russell Crowe\",\"character\":\"Henry Murray\",\"profile_path\":\"/rc.jpg\"},"
      "{\"id\":2,\"name\":\"Shailene Woodley\",\"character\":\"Joanne Miller\",\"profile_path\":\"/sw.jpg\"},"
      "{\"id\":3,\"name\":\"Jacob Tremblay\",\"character\":\"Ted Kaczynski\",\"profile_path\":\"/jt.jpg\"},"
      "{\"id\":4,\"name\":\"Zoë Kravitz\",\"character\":\"X\"},"
      "{\"id\":5,\"name\":\"\"},"
      "{\"id\":6,\"name\":\"J.K. Simmons\",\"character\":\"Y\",\"profile_path\":\"/jk.jpg\"}]}";
    memset(&it, 0, sizeof it);
    snprintf(it.elenco[0].nome, sizeof it.elenco[0].nome, "Russell Crowe");
    snprintf(it.elenco[1].nome, sizeof it.elenco[1].nome, "Jacob Tremblay");
    snprintf(it.elenco[2].nome, sizeof it.elenco[2].nome, "Shailene Woodley");
    snprintf(it.elenco[3].nome, sizeof it.elenco[3].nome, "ZOE KRAVITZ");
    snprintf(it.elenco[4].nome, sizeof it.elenco[4].nome, "Ninguem Aqui");
    it.nElenco = 5;
    assert(desc_tmdb_elenco(json, &it) == 4);
    assert(!strcmp(it.elenco[1].papel, "Ted Kaczynski"));
    assert(strstr(it.elenco[1].foto, "/jt.jpg") && it.elenco[1].tmdb == 3);
    assert(!strcmp(it.elenco[2].papel, "Joanne Miller"));
    assert(strstr(it.elenco[2].foto, "/sw.jpg") && it.elenco[2].tmdb == 2);
    assert(it.elenco[3].tmdb == 4 && !it.elenco[3].foto[0]);   // casou sem foto
    assert(!it.elenco[4].foto[0] && !it.elenco[4].papel[0] && it.elenco[4].tmdb == 0);
    // Quem sobrou do TMDB entra no fim; o sem nome nao.
    assert(it.nElenco == 6);
    assert(!strcmp(it.elenco[5].nome, "J.K. Simmons") && it.elenco[5].tmdb == 6);
    // Segunda passada (reabrir o detalhe) nao duplica ninguem.
    assert(desc_tmdb_elenco(json, &it) == 5);
    assert(it.nElenco == 6); }
  puts("ok  elenco do TMDB casa por nome, nao por posicao (#153)");

  puts("cateps: tudo ok");
  {
    CatEp timed = {0}; timed.temporada=1; timed.episodio=1;
    const char *runtime = "{\"episodes\":[{\"episode_number\":1,\"runtime\":47}]}";
    assert(desc_tmdb_notas_temporada(runtime,&timed,1,1)==1);
    assert(!strcmp(timed.duracao,"47 min"));
    assert(desc_tmdb_notas_temporada(runtime,&timed,1,1)==0);
    assert(desc_tmdb_notas_temporada("{\"episodes\":[{\"episode_number\":1,\"runtime\":null}]}",&timed,1,1)==0);
    assert(!strcmp(timed.duracao,"47 min"));
  }
  // #556: o Cinemeta ja fornece estes especiais; nao inventar outra numeracao.
  {
    FILE *f = fopen("tests/fixtures/battle_through_the_heavens_specials.json", "rb");
    char *corpo; long tam; CatEp eps556[32]; TempsPub tp556;
    CatItem c556 = {0}; char alvo[96];
    assert(f); fseek(f, 0, SEEK_END); tam = ftell(f); rewind(f);
    corpo = malloc((size_t)tam + 1); assert(corpo);
    assert(fread(corpo, 1, (size_t)tam, f) == (size_t)tam); corpo[tam] = 0; fclose(f);
    assert(desc_meta_tem_temporadas(corpo));
    assert(desc_meta_n_episodios(corpo) == 23);
    assert(parsearEpisodios(corpo, eps556, 32) == 23);
    temporadasDosEps(eps556, 23, &tp556);
    assert(tp556.n == 3 && tp556.t[0] == 1 && tp556.t[1] == 5 && tp556.t[2] == 0);
    for (int i = 0; i < 21; i++) {
      assert(eps556[i].temporada == 0 && eps556[i].episodio == i + 1);
      snprintf(alvo, sizeof alvo, "tt12409194:0:%d", i + 1);
      assert(!strcmp(eps556[i].vid, alvo));
    }
    strcpy(c556.imdb, "tt12409194"); strcpy(c556.tipo, "series");
    cat_definir_tudo(&c556, 1, NULL, 0); cat_definir_episodios(0, eps556, 23);
    assert(cat_id_stream(0, 0, 21, alvo, sizeof alvo) && !strcmp(alvo, "tt12409194:0:21"));
    strcpy(eps556[0].vid, "addon:special:one"); cat_definir_episodios(0, eps556, 23);
    assert(cat_id_stream(0, 0, 1, alvo, sizeof alvo) && !strcmp(alvo, "addon:special:one"));
    cat_apontar_episodio(0, 0, 21);
    assert(cat_item(0)->temporada == 0 && cat_item(0)->episodio == 21);
    // ID do addon continua sendo o ID do video, inclusive em T0.
    strcpy(c556.imdb, "kitsu:41370"); strcpy(eps556[0].vid, "kitsu:special:one");
    cat_definir_tudo(&c556, 1, NULL, 0); cat_definir_episodios(0, eps556, 1);
    assert(cat_id_stream(0, 0, 1, alvo, sizeof alvo) && !strcmp(alvo, "kitsu:special:one"));
    cat_definir_tudo(&c556, 1, NULL, 0); // sem lista de videos deste addon
    assert(!cat_id_stream(0, 0, 1, alvo, sizeof alvo) && !alvo[0]);
    free(corpo);
  }
  {
    const char *s = "{\"videos\":[{\"season\":0,\"episode\":1,\"id\":\"a\"},"
                    "{\"season\":0,\"episode\":1,\"id\":\"duplicado\"},"
                    "{\"season\":0,\"episode\":2,\"id\":\"b\"}]}";
    CatEp eps556[3]; EpSet tempos556 = {0};
    const char *ratings = "{\"imdbRating\":9.9,\"videos\":["
      "{\"season\":1,\"episode\":1,\"imdbRating\":\"9.2\"},"
      "{\"season\":1,\"episode\":2,\"rating\":\"7.4\"},"
      "{\"season\":1,\"episode\":3,\"imdbRating\":11}]}";
    assert(parsearEpisodios(ratings, eps556, 3) == 3);
    assert(eps556[0].notaImdb == 92 && eps556[0].nota == 0);
    assert(eps556[1].notaImdb == 74 && eps556[2].notaImdb == 0);
    { CatEp merged = {0};
      merged.temporada = merged.episodio = 1;
      desc_mesclar_episodios(&merged, 1, eps556, 3, DESC_MESCLA_VAZIOS);
      assert(merged.notaImdb == 92);
      merged.notaImdb = 83;
      desc_mesclar_episodios(&merged, 1, eps556, 3, DESC_MESCLA_TEXTO);
      assert(merged.notaImdb == 83); }
    assert(desc_meta_tem_temporadas(s) && desc_meta_n_episodios(s) == 2);
    assert(parsearEpisodios(s, eps556, 3) == 2 && !strcmp(eps556[0].vid, "a"));
    assert(epSetNovo(&tempos556, 0, 0) && !epSetNovo(&tempos556, 0, 0));
    assert(epSetNovo(&tempos556, 5, 0) && tempos556.n == 2); free(tempos556.v);
    assert(metaArquivosCompativeis(s, s));
    assert(!desc_meta_tem_temporadas("{\"videos\":[{\"season\":0}]}"));
    assert(desc_meta_n_episodios("{\"videos\":[{\"season\":0}]}") == 0);
  }
  puts("ok  #556: especiais preservam temporada, IDs e abas nao sequenciais");
  return 0;
}
