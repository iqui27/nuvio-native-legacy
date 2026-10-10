// ISSUE #18 — CATALOGO DENTRO DE COLECAO VIRANDO FILEIRA SOLTA NA HOME.
//
// Reproduz o cenario do relator, que continuou vendo o defeito depois do
// v1.0.11: as colecoes dele sao empurradas pelo app web do Xperience e chegam
// pela CONTA, nao pelo pacote. Uma fonte da conta declara `addonId` e NAO
// declara URL — a URL so existe depois que alguem leu o manifesto daquele
// addon, e no arranque isso acontece DEPOIS de o sync ter guardado a colecao.
//
// O teste roda a montagem de verdade (montar(), pelo desc_iniciar) contra uma
// rede falsa, com a mesma sequencia medida na TV:
//   1. sync entrega as colecoes  -> col_definir_json com a sonda ainda muda
//   2. a montagem le o manifesto -> so agora addons_base_por_id sabe a URL
//   3. a montagem filtra         -> col_por_catalogo tem de casar mesmo assim
//
// Sem o conserto, o passo 3 nao casa e os catalogos da colecao viram fileira.
#include <assert.h>
#include <unistd.h>
int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_busca_cinemeta(void) { return 1; }
int ajustes_busca_nuvio(void) { return 0; }   // #311: Primeiro (padrao)
int ajustes_ocultar_nao_lancados(void) { return 0; }   // #369: descoberta.c le o ajuste
#include "../src/descoberta.c"
#include "jellyfin_stub.inc"
Uint32 SDL_GetTicks(void) { return 0; }
unsigned recomenda_geracao(void) { return 1; }

// ------------------------------------------------------------------ o addon
#define BASE "https://xperience.example/abc"
#define AID  "app.xperience.demo"

static int sondaLeu;          // addons_manifesto_lido ja passou?
static int limiteFileiras = 16;
static int addonAtivo = 1;

// Seis catalogos: quatro dentro da colecao, dois soltos. A ordem coloca os da
// colecao PRIMEIRO de proposito — e assim que o Xperience declara os dele, e e
// o que faz o teto ser gasto pelos errados.
static const char *MANIFESTO =
  "{\"id\":\"" AID "\",\"name\":\"Xperience\",\"resources\":[\"catalog\"],\"catalogs\":["
  "{\"type\":\"movie\",\"id\":\"col_a\",\"name\":\"Col A\"},"
  "{\"type\":\"movie\",\"id\":\"col_b\",\"name\":\"Col B\"},"
  "{\"type\":\"series\",\"id\":\"col_c\",\"name\":\"Col C\"},"
  "{\"type\":\"movie\",\"id\":\"col_d\",\"name\":\"Col D\"},"
  "{\"type\":\"movie\",\"id\":\"solto_a\",\"name\":\"Solto A\"},"
  "{\"type\":\"movie\",\"id\":\"solto_b\",\"name\":\"Solto B\"}]}";

// A colecao da conta, no shape do collectionsStore.js do web: `addonId` e
// nenhuma `addonBaseUrl`. E isto que o app web do Xperience empurra.
static const char *COLECAO_DA_CONTA =
  "{\"collections\":[{\"id\":\"c9\",\"title\":\"Minha selecao\",\"folders\":["
  "{\"id\":\"f1\",\"title\":\"Pasta 1\",\"sources\":["
  "{\"provider\":\"addon\",\"addonId\":\"" AID "\",\"type\":\"movie\",\"catalogId\":\"col_a\"},"
  "{\"provider\":\"addon\",\"addonId\":\"" AID "\",\"type\":\"movie\",\"catalogId\":\"col_b\"}]},"
  "{\"id\":\"f2\",\"title\":\"Pasta 2\",\"sources\":["
  "{\"provider\":\"addon\",\"addonId\":\"" AID "\",\"type\":\"series\",\"catalogId\":\"col_c\"},"
  "{\"provider\":\"addon\",\"addonId\":\"" AID "\",\"type\":\"movie\",\"catalogId\":\"col_d\"}]}"
  "]}]}";

int   addons_n(void)              { return addonAtivo; }
const char *addons_base(int i)    { (void)i; return BASE; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }
const char *addons_nome(int i)              { (void)i; return "addon"; }
unsigned addons_versao(void)        { return 1; }   // estatico no teste: sem troca de lista
const char *addons_base_por_id(const char *id) {
  if (!sondaLeu) return "";       // exatamente o que addons.c faz antes da sonda
  return id && !strcmp(id, AID) ? BASE : "";
}
void addons_manifesto_lido(int i, const char *corpo) { (void)i; (void)corpo; sondaLeu = 1; }

// ------------------------------------------------------------------ a rede
static int pedidosDeCatalogo;
static int snapshotValido, snapshotTem;
static int preservarFixture;
static CatFileira antigaFixture;
char *rede_baixar(const char *url, int t) {
  (void)t;
  if (strstr(url, "/manifest.json")) return strdup(MANIFESTO);
  if (strstr(url, "/catalog/")) {
    pedidosDeCatalogo++;
    return strdup("{\"metas\":[{\"id\":\"tt1\",\"name\":\"Um\",\"poster\":\"1.jpg\"},"
                  "{\"id\":\"tt2\",\"name\":\"Dois\",\"poster\":\"2.jpg\"}]}");
  }
  return NULL;
}

// ------------------------------------------------------- o que a home recebe
static CatFileira publicadas[CAT_FIL_MAX];
static int nPublicadas;
static void guardar(const CatFileira *f, int n) {
  if (n > CAT_FIL_MAX) n = CAT_FIL_MAX;
  memcpy(publicadas, f, sizeof(CatFileira) * (size_t)n);
  nPublicadas = n;
}
void cat_definir_tudo(const CatItem *l, int q, const CatFileira *f, int n) {
  (void)l; (void)q; guardar(f, n);
}
void cat_republicar_fileiras(const CatFileira *f, int n) { guardar(f, n); }

// ------------------------------------------------------------------ o resto
void  SDL_Delay(Uint32 ms)                 { usleep(ms * 1000); }
int   ajustes_cw_fonte(void)               { return 0; }
int   ajustes_cw_ordem(void)               { return 0; }   // Padrao (issue #127)
int   ajustes_itens_fileira(void)          { return 12; }   // padrao (#163)
int   ajustes_cw_mostrar_nao_exibidos(void) { return 1; }
int   ajustes_cw_proximo(void) { return 1; }
int   ajustes_idioma_ingles(void)          { return 0; }
int ajustes_idioma(void) { return 0; }
unsigned homeestado_geracao(void) { return 1; }
int homeestado_contexto_valido(void) { return snapshotValido; }
int homeestado_tem_fileira(const char *chave) { return snapshotTem && chave && !strcmp(chave, "old-row"); }
int homeestado_ordem_fileira(const char *chave) { (void)chave; return -1; }
void homeestado_salvar(const CatFileira *f, int n) { (void)f; (void)n; }
int homeestado_salvar_se_geracao(const CatFileira *f, int n, unsigned g) { (void)f; (void)n; return g == 1; }
int homeestado_identidade_geracao(unsigned g, char *d, unsigned z, int *p) {
  if (g != 1) return 0;
  if (d && z) snprintf(d, z, ""); if (p) *p = 1; return 1;
}
const char *sessao_usuario(void) { return ""; }
// Contexto em partes (homeestado.h, 1.4.5): constante aqui, entao nada muda
// no meio da montagem e o fim dela segue o caminho de sempre.
void homeestado_contexto(HomeContexto *c) { *c = (HomeContexto){0}; c->perfil = 1; }
int homeestado_mudancas(const HomeContexto *a, const HomeContexto *b) { (void)a; (void)b; return 0; }
const char *homeestado_mudancas_texto(int m, char *b, unsigned t) { (void)m; if (b && t) b[0] = 0; return b; }
const CatFileira *cat_fileira(int i) { return preservarFixture && i == 0 ? &antigaFixture : NULL; }
int cat_n_fileiras(void) { return preservarFixture; }
int cat_copiar_fileira(const char *k, CatItem *o, int m, CatFileira *meta) {
  (void)meta;
  if (preservarFixture && m > 0 && !strcmp(k, antigaFixture.chave)) {
    memset(o, 0, sizeof *o); snprintf(o->imdb, sizeof o->imdb, "tt_preserved");
    return 1;
  }
  return 0;
}
int cat_gravar_cache_se_identidade(const char *d, const char *u, int p) {
  (void)d; (void)u; (void)p; return 1;
}
// Integracao TMDB ligada por padrao, como no app de verdade — o portao
// desc_chave_tmdb consulta estes stubs pelo caminho inteiro.
int   ajustes_tmdb_ligado(void)            { return 1; }
int   ajustes_meta_externo(void)           { return 0; }
int   ajustes_meta_so_cinemeta(void)        { return 0; }
int   addons_aceita_id(int i, const char *t, const char *id) { (void)i; (void)t; (void)id; return -1; }
const char *ajustes_tmdb_idioma(void)      { return "pt-BR"; }
int   cat_acrescentar(const CatItem *i)    { (void)i; return -1; }
void  cat_atualizar_item(int i, const CatItem *n) { (void)i; (void)n; }
void  cat_atualizar_item_sem_abas(int i, const CatItem *n) { (void)i; (void)n; }
// 2.0.3: fios da descoberta copiam e escrevem por partes (2b4234eb, 70acffaf).
int   cat_copiar_item(int i, CatItem *s) { (void)i; (void)s; return 0; }
int   cat_completar_sinopse(int i, const char *im, const char *si, const char *ti) { (void)i; (void)im; (void)si; (void)ti; return 0; }
int   cat_aplicar_localizado(int i, const char *im, const char *ti, const char *si, const char *lo, const char *fu) { (void)i; (void)im; (void)ti; (void)si; (void)lo; (void)fu; return 0; }
int   ajustes_tmdb_basico(void)            { return 0; }
int   ajustes_tmdb_arte(void)              { return 0; }
void  cat_cache_substituido(void)          { }
void  cat_definir_episodios(int i, const CatEp *l, int n) { (void)i; (void)l; (void)n; }
int   cat_do_cache(void)                   { return 0; }
int   cat_n(void)                          { return 0; }
int   cat_home_apenas_fixas(void)          { return 0; }   // not a progressive publish here
int   cat_mesclar_listas(const CatItem *v, int q) { (void)v; (void)q; return 0; }   // no watchlist/collection rows in this fixture
unsigned long cat_assinatura(void)         { return 0; }
unsigned long cat_assinatura_de(const CatItem *l, int q, const CatFileira *f, int n) {
  (void)l; (void)q; (void)f; (void)n; return 1; }
int   cat_gravar_cache(const char *d)      { (void)d; return 0; }
int   cat_indice_por_imdb(const char *s)   { (void)s; return -1; }
const CatItem *cat_item(int i)             { (void)i; return NULL; }
int   cat_n_episodios(int i)               { (void)i; return 0; }
double cat_relogio_ms(void)                { return 0.0; }   // descoberta.c times publicarMontagem; the value is only logged
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
int   addons_fornece(int i, int oque)     { (void)i; (void)oque; return 0; }
int   addons_sondado(int i)              { (void)i; return 0; }
int   fil_limite(void)                     { return limiteFileiras; }
int   fil_oculta(const char *c)            { (void)c; return 0; }
int   fil_adicionada_na_tv(const char *c) { (void)c; return 0; }
int   fil_estado_chave(const char *c) { (void)c; return -1; }
const char *fil_hero_fonte(void) { return ""; }
// A assinatura ganhou addon/tipo/contagem quando a folha de fileiras passou a
// dizer de onde cada fileira vem. Este teste nao tem opiniao sobre nada disso.
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
const char *i18n(const char *s)            { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
void  marco(const char *n)                 { (void)n; }
void  prog_chave(char *d, unsigned n, const char *c, int t, int e) {
  (void)c; (void)t; (void)e; if (n) d[0] = 0;
}
void  prog_content_id(char *d, unsigned n, const char *i, int *t, int *e) {
  (void)i; (void)t; (void)e; if (n) d[0] = 0;
}
int   prog_ler(ProgRegistro *s, int m)     { (void)s; (void)m; return 0; }
int   prog_por_chave(const char *c, ProgRegistro *s) { (void)c; (void)s; return 0; }
// "Tirar de Continuar assistindo" (desc_tirar_continuar, tests/cwremover.sh).
void  prog_remover(const char *c)          { (void)c; }
void  prog_marcar_removido(const char *i)  { (void)i; }
int   prog_removido_vence(const char *i, long long ms) { (void)i; (void)ms; return 0; }
int   cat_tirar_continuar(const char *i)   { (void)i; return 0; }
int   trakt_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   trakt_continuar_falhou(void)        { return 0; }
int   perfis_ativo(void)                  { return 1; }
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
// Fontes nao-addon (issue #44) e a refazagem da fileira CW (#38): o cenario
// testado nao tem nenhum dos dois, mas o codigo referencia os simbolos.
void  cat_trocar_continuar(const CatItem *l, int q) { (void)l; (void)q; }
const char *nuvem_trakt_cliente(void)      { return ""; }
char *rede_baixar_com(const char *u, int t, const char *const *c) {
  (void)c; return rede_baixar(u, t); }

// ------------------------------------------------------------------ o teste
static void esperarCiclo(void) {
  int i;
  for (i = 0; i < 500 && !buscando; i++) usleep(2000);   // ate o fio pegar
  for (i = 0; i < 5000 && buscando; i++) usleep(2000);
  assert(!buscando);
  usleep(20000);   // o fio publica e so depois zera `buscando`; deixa assentar
}

static int temFileira(const char *catId) {
  int i;
  for (i = 0; i < nPublicadas; i++)
    if (!strcmp(publicadas[i].catId, catId)) return 1;
  return 0;
}

static void listar(const char *rotulo) {
  int i;
  printf("    %s:", rotulo);
  for (i = 0; i < nPublicadas; i++) printf(" %s", publicadas[i].catId);
  printf("\n");
}

static void zerarColecoes(void) {
  assert(col_definir_json("{\"collections\":[]}") == 0 && col_n() == 0);
}

int main(void) {
  {
    CatFileira antiga = {0};
    snprintf(antiga.chave, sizeof antiga.chave, "old-row");
    snprintf(antiga.base, sizeof antiga.base, "%s", BASE);
    // Same configured add-on URL after a profile switch is not evidence that
    // the live CatItems belong to this profile.
    addonAtivo = 1; snapshotValido = snapshotTem = 0;
    assert(!fileiraPodeSerPreservada(&antiga));
    // A matching snapshot proves that these rows belong to the current
    // owner/profile/config, even if the source disappeared from the add-on list.
    addonAtivo = 0;
    snapshotValido = snapshotTem = 1;
    assert(fileiraPodeSerPreservada(&antiga));
    puts("ok  same-source rows are rejected without the current-profile snapshot");
    addonAtivo = 1; snapshotValido = snapshotTem = 0;
  }
  {
    // CW/social do not consume the two catalogue slots, including when a
    // missing source is recovered from the accepted profile snapshot.
    CatItem *it = calloc(8, sizeof *it);
    CatFileira rows[5] = {0};
    int n = 3, cap = 8, nr = 3;
    snprintf(rows[0].chave, sizeof rows[0].chave, "continue_watching");
    snprintf(rows[1].chave, sizeof rows[1].chave, "social_activity");
    snprintf(rows[2].chave, sizeof rows[2].chave, "current-row");
    snprintf(rows[2].base, sizeof rows[2].base, "%s", BASE);
    snprintf(antigaFixture.chave, sizeof antigaFixture.chave, "old-row");
    snprintf(antigaFixture.base, sizeof antigaFixture.base, "%s", BASE);
    limiteFileiras = 2; preservarFixture = snapshotValido = snapshotTem = 1;
    preservarFileirasAusentes(&it, &n, &cap, rows, &nr);
    assert(nr == 4 && n == 4 && !strcmp(rows[3].chave, "old-row"));
    assert(!strcmp(it[3].imdb, "tt_preserved"));
    preservarFileirasAusentes(&it, &n, &cap, rows, &nr);
    assert(nr == 4 && n == 4); // idempotent and quota remains bounded
    free(it); preservarFixture = snapshotValido = snapshotTem = 0;
    limiteFileiras = 16;
    puts("ok  #233: missing-source preservation keeps catalogue quota independent of fixed rows");
  }
  // ---------------------------------------------------------------- caso 1
  // A colecao chega ANTES do ciclo (o caminho normal: sync em ~2 s, manifestos
  // em ~7 s) e a sonda ainda nao passou. Os quatro catalogos da colecao NAO
  // podem virar fileira; os dois soltos TEM de virar.
  sondaLeu = 0;
  assert(col_definir_json(COLECAO_DA_CONTA) == 2);
  assert(!sondaLeu);   // a base das fontes ficou vazia, como na TV
  desc_iniciar();
  esperarCiclo();
  listar("caso 1");
  assert(!temFileira("col_a") && !temFileira("col_b") &&
         !temFileira("col_c") && !temFileira("col_d"));
  assert(temFileira("solto_a") && temFileira("solto_b"));
  assert(nPublicadas == 2);
  puts("ok  #18: catalogo de colecao da conta (so addonId) nao vira fileira solta");

  // ---------------------------------------------------------------- caso 2
  // O TETO. Com limite 2 e os catalogos da colecao na FRENTE da ordem, as duas
  // vagas tem de ir para os soltos: fileira filtrada nao pode gastar vaga.
  limiteFileiras = 2;
  pedidosDeCatalogo = 0;
  desc_iniciar();
  esperarCiclo();
  listar("caso 2");
  assert(nPublicadas == 2);
  assert(temFileira("solto_a") && temFileira("solto_b"));
  assert(pedidosDeCatalogo == 2);   // e nao gastou pedido com os da colecao
  puts("ok  #18: fileira dentro de colecao nao consome vaga do teto");

  // ---------------------------------------------------------------- caso 3
  // A COLECAO CHEGANDO DEPOIS DA MONTAGEM (sync lento, ou colecao criada no app
  // web com a TV ligada). O teto ja foi gasto pelos catalogos da colecao; ao
  // remontar sem rede eles saem e a home fica MENOR que o limite. Tem de sair
  // um pedido de ciclo para preencher as vagas.
  zerarColecoes();
  limiteFileiras = 2;
  desc_iniciar();
  esperarCiclo();
  listar("caso 3 (sem colecao)");
  assert(nPublicadas == 2 && temFileira("col_a") && temFileira("col_b"));
  assert(desc_catalogos_fora() > 0);   // o teto DEIXOU catalogo sem pedir
  assert(col_definir_json(COLECAO_DA_CONTA) == 2);
  desc_remontar_fileiras();
  esperarCiclo();
  listar("caso 3 (depois da remontagem)");
  assert(nPublicadas == 2);
  assert(temFileira("solto_a") && temFileira("solto_b"));
  puts("ok  #18: colecao que chega tarde nao deixa a home abaixo do limite");

  // NOME DO CATALOGO PARA A ABA DA COLECAO (01/10, pasta Netflix): a base da
  // fonte da conta nao era a do addon instalado byte a byte e a aba mostrava
  // o id cru. Sem a base exata, (tipo, id) decide quando so ha um nome.
  registrarNomeCatalogo("https://instalado/cfg1", "movie", "streaming_netflix_movies", "Netflix");
  assert(!strcmp(desc_nome_catalogo("https://instalado/cfg1", "movie", "streaming_netflix_movies"), "Netflix"));
  assert(!strcmp(desc_nome_catalogo("https://da-conta/cfg2", "movie", "streaming_netflix_movies"), "Netflix"));
  assert(!desc_nome_catalogo("https://da-conta/cfg2", "series", "streaming_netflix_movies")[0]);
  registrarNomeCatalogo("https://outro", "movie", "streaming_netflix_movies", "Outro nome");
  assert(!desc_nome_catalogo("https://da-conta/cfg2", "movie", "streaming_netflix_movies")[0]);   // dois nomes: nao escolhe
  assert(!strcmp(desc_nome_catalogo("https://outro", "movie", "streaming_netflix_movies"), "Outro nome"));
  puts("ok  nome do catalogo da aba: base exata, senao (tipo, id) com nome unico");

  puts("colfileiras: tudo ok");
  return 0;
}

// Fixture sem persistencia nem conta real.
const char *dados_dir(void) { return ""; }
