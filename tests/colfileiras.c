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
#include "../src/descoberta.c"

// ------------------------------------------------------------------ o addon
#define BASE "https://xperience.example/abc"
#define AID  "app.xperience.demo"

static int sondaLeu;          // addons_manifesto_lido ja passou?
static int limiteFileiras = 16;

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

int   addons_n(void)              { return 1; }
const char *addons_base(int i)    { (void)i; return BASE; }
const char *addons_base_por_id(const char *id) {
  if (!sondaLeu) return "";       // exatamente o que addons.c faz antes da sonda
  return id && !strcmp(id, AID) ? BASE : "";
}
void addons_manifesto_lido(int i, const char *corpo) { (void)i; (void)corpo; sondaLeu = 1; }

// ------------------------------------------------------------------ a rede
static int pedidosDeCatalogo;
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
int   ajustes_idioma_ingles(void)          { return 0; }
int   cat_acrescentar(const CatItem *i)    { (void)i; return -1; }
void  cat_atualizar_item(int i, const CatItem *n) { (void)i; (void)n; }
void  cat_cache_substituido(void)          { }
void  cat_definir_episodios(int i, const CatEp *l, int n) { (void)i; (void)l; (void)n; }
int   cat_do_cache(void)                   { return 0; }
int   cat_gravar_cache(const char *d)      { (void)d; return 0; }
int   cat_indice_por_imdb(const char *s)   { (void)s; return -1; }
const CatItem *cat_item(int i)             { (void)i; return NULL; }
int   cat_n_episodios(int i)               { (void)i; return 0; }
void  fil_gravar_registro(void)            { }
int   fil_limite(void)                     { return limiteFileiras; }
int   fil_oculta(const char *c)            { (void)c; return 0; }
// A assinatura ganhou addon/tipo/contagem quando a folha de fileiras passou a
// dizer de onde cada fileira vem. Este teste nao tem opiniao sobre nada disso.
void  fil_registrar(const char *c, const char *t, const char *a,
                    const char *tp, int itens) {
  (void)c; (void)t; (void)a; (void)tp; (void)itens;
}
int   fil_tem_ordem(void)                  { return 0; }
int   fil_unir(const char *const *c, int n, int *s, int m) {
  int i; (void)c; for (i = 0; i < n && i < m; i++) s[i] = i; return i;
}
const char *i18n(const char *s)            { return s; }
void  marco(const char *n)                 { (void)n; }
void  prog_chave(char *d, unsigned n, const char *c, int t, int e) {
  (void)c; (void)t; (void)e; if (n) d[0] = 0;
}
void  prog_content_id(char *d, unsigned n, const char *i, int *t, int *e) {
  (void)i; (void)t; (void)e; if (n) d[0] = 0;
}
int   prog_ler(ProgRegistro *s, int m)     { (void)s; (void)m; return 0; }
int   prog_por_chave(const char *c, ProgRegistro *s) { (void)c; (void)s; return 0; }
int   trakt_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   trakt_enfeitar_lote(CatItem *s, int n) { (void)s; (void)n; return 0; }
int   trakt_lista(const char *q, CatItem *s, int m) { (void)q; (void)s; (void)m; return 0; }
int   trakt_social(CatItem *s, int m)      { (void)s; (void)m; return 0; }

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
  // col_definir_json recusa vazio de proposito (vazio nao apaga). Uma colecao
  // de um addon que nao existe aqui e o jeito honesto de voltar ao zero.
  col_definir_json("{\"collections\":[{\"id\":\"z\",\"title\":\"Z\",\"folders\":"
                   "[{\"id\":\"zf\",\"title\":\"Z\",\"sources\":[{\"provider\":\"addon\","
                   "\"addonId\":\"nao.instalado\",\"type\":\"movie\",\"catalogId\":\"z\"}]}]}]}");
}

int main(void) {
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

  puts("colfileiras: tudo ok");
  return 0;
}
