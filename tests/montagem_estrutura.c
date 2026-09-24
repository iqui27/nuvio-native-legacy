// ESTRUTURA QUE MUDA NO MEIO DA MONTAGEM NAO JOGA FORA O QUE CHEGOU.
//
// Log de campo (Samsung Tizen 6, 1.4.4, @rawldon, 241 colecoes, 5 addons):
//   18.5s montar: inicio
//   29.6s [colecoes] 241 pastas vindas da conta
//   48.8s [desc] catalogos: ... 16 de 16 fileira(s) no limite
//   48.9s [desc] montagem descartada: conta/perfil/config mudou no meio
//   85.0s [home] 24 fileiras na tela
//   85.9s [desc] cache descartado: conta/perfil/config mudou durante a montagem
// A home da conta apareceu aos 85 s e nao aos 49 s, e o arranque seguinte
// comecou sem cache. A mesma linha de descarte aparece 115 vezes nos logs.
//
// Aqui descoberta.c inteiro entra por #include e roda montar() de verdade
// (desc_iniciar), com homeestado.c, catalogo.c, colecoes.c e catordem.c DE
// VERDADE — a assinatura, o snapshot e o cache em disco sao os do app. So a
// rede, o Trakt e fileiras.c sao dubles. A mudanca "chega do sync" dentro de
// trakt_lista("watchlist"), que montar() chama DEPOIS de buscar as fileiras e
// ANTES de decidir se publica: e o mesmo instante do log (colecoes chegando
// com os catalogos ja no ar), so que deterministico.
//
//   1. colecoes chegando (estrutura) com o que foi buscado bastando: UMA
//      montagem, nada descartado, fileiras na tela, snapshot e cache validos
//      no "proximo arranque";
//   2. colecao que engole uma fileira buscada: publica o que veio PRIMEIRO,
//      e so depois pede o ciclo de rede que traz o catalogo que faltou;
//   3. registro de fileiras.c crescendo (fil_registrar da propria montagem,
//      fil_espelhar_ordem da home) nao conta como mudanca;
//   4. troca de DONO no meio continua descartando: nada do dono anterior na
//      tela nem no cache do novo.
//
//   bash tests/montagem_estrutura.sh
#include <assert.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
int arte_reserva_episodios(const char *imdb, const char *corpo) { (void)imdb; (void)corpo; return 0; }

#include "../src/descoberta.c"

#define BASE "https://addon.example/abc"
#define AID  "app.addon.demo"

// Seis catalogos; limite 3. A ordem do manifesto e a ordem da home.
static const char *MANIFESTO =
  "{\"id\":\"" AID "\",\"name\":\"Addon\",\"resources\":[\"catalog\"],\"catalogs\":["
  "{\"type\":\"movie\",\"id\":\"cat_a\",\"name\":\"Cat A\"},"
  "{\"type\":\"movie\",\"id\":\"cat_b\",\"name\":\"Cat B\"},"
  "{\"type\":\"movie\",\"id\":\"cat_c\",\"name\":\"Cat C\"},"
  "{\"type\":\"movie\",\"id\":\"cat_d\",\"name\":\"Cat D\"},"
  "{\"type\":\"movie\",\"id\":\"cat_e\",\"name\":\"Cat E\"},"
  "{\"type\":\"movie\",\"id\":\"cat_f\",\"name\":\"Cat F\"}]}";

// Colecao de um addon que nao esta instalado: muda a assinatura das colecoes
// e nao engole nada — o caso comum de "241 pastas chegaram".
static const char *COLECAO_ALHEIA =
  "{\"collections\":[{\"id\":\"z\",\"title\":\"Z\",\"folders\":"
  "[{\"id\":\"zf\",\"title\":\"Z\",\"sources\":[{\"provider\":\"addon\","
  "\"addonId\":\"nao.instalado\",\"type\":\"movie\",\"catalogId\":\"z\"}]}]}]}";
// Colecao que ENGOLE cat_a: a vaga dele tem de ir para cat_d, que ninguem
// buscou ainda.
static const char *COLECAO_ENGOLE_A =
  "{\"collections\":[{\"id\":\"c9\",\"title\":\"Selecao\",\"folders\":["
  "{\"id\":\"f1\",\"title\":\"Pasta\",\"sources\":["
  "{\"provider\":\"addon\",\"addonId\":\"" AID "\",\"type\":\"movie\",\"catalogId\":\"cat_a\"}]}"
  "]}]}";

// ------------------------------------------------------------ a identidade
static char dono[64] = "dono-A";
static char dirDados[PATH_MAX];
const char *sessao_usuario(void)  { return dono; }
int         perfis_ativo(void)    { return 1; }
const char *dados_dir(void)       { return dirDados; }
static void caminhoDado(char *dst, size_t tam, const char *nome) {
  snprintf(dst, tam, "%s/%s", dirDados, nome);
}
int dados_gravar(const char *nome, const char *conteudo) {
  char p[PATH_MAX + 64]; FILE *f; size_t n; int ok;
  caminhoDado(p, sizeof p, nome); f = fopen(p, "wb");
  if (!f) return 0;
  n = strlen(conteudo); ok = fwrite(conteudo, 1, n, f) == n;
  return fclose(f) == 0 && ok;
}
char *dados_ler(const char *nome) {
  char p[PATH_MAX + 64]; FILE *f; long n; char *b;
  caminhoDado(p, sizeof p, nome); f = fopen(p, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
  b = malloc((size_t)n + 1);
  if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
  b[n] = 0; fclose(f); return b;
}
int dados_apagar(const char *nome) {
  char p[PATH_MAX + 64]; caminhoDado(p, sizeof p, nome); return unlink(p) == 0;
}

// ------------------------------------------------------------------ o addon
int   addons_n(void)                   { return 1; }
const char *addons_base(int i)         { (void)i; return BASE; }
int   addons_ativo(int i)              { (void)i; return 1; }
const char *addons_id_manifesto(int i) { (void)i; return AID; }
const char *addons_nome(int i)         { (void)i; return "Addon"; }
unsigned addons_versao(void)           { return 1; }
const char *addons_base_por_id(const char *id) { return id && !strcmp(id, AID) ? BASE : ""; }
void  addons_manifesto_lido(int i, const char *c) { (void)i; (void)c; }

// ------------------------------------------------------------------ a rede
// Cada catalogo devolve 2 titulos marcados com o catId E o dono de quem
// pediu: e assim que o caso 4 sabe de quem veio cada item na tela.
static volatile int pedidosCatalogo;
static char pedidos[64][16];
static pthread_mutex_t pedTrava = PTHREAD_MUTEX_INITIALIZER;
char *rede_baixar(const char *url, int t) {
  char buf[600];
  const char *p;
  char id[16] = "";
  (void)t;
  if (strstr(url, "/manifest.json")) return strdup(MANIFESTO);
  p = strstr(url, "/catalog/movie/");
  if (!p) return NULL;
  snprintf(id, sizeof id, "%.5s", p + 15);
  pthread_mutex_lock(&pedTrava);
  if (pedidosCatalogo < 64) snprintf(pedidos[pedidosCatalogo], sizeof pedidos[0], "%s", id);
  pedidosCatalogo++;
  pthread_mutex_unlock(&pedTrava);
  snprintf(buf, sizeof buf,
           "{\"metas\":[{\"id\":\"tt%s1%s\",\"type\":\"movie\",\"name\":\"%s %s 1\",\"poster\":\"p.jpg\"},"
           "{\"id\":\"tt%s2%s\",\"type\":\"movie\",\"name\":\"%s %s 2\",\"poster\":\"p.jpg\"}]}",
           id + 4, dono, id, dono, id + 4, dono, id, dono);
  return strdup(buf);
}
char *rede_baixar_com(const char *u, int t, const char *const *c) { (void)c; return rede_baixar(u, t); }
static int foiPedido(const char *id) {
  int i, r = 0;
  pthread_mutex_lock(&pedTrava);
  for (i = 0; i < pedidosCatalogo && i < 64; i++) if (!strcmp(pedidos[i], id)) r++;
  pthread_mutex_unlock(&pedTrava);
  return r;
}

// ------------------------------------------------ "o sync" entre a rede e o fim
enum { NADA, COLECAO_SEM_EFEITO, COLECAO_ENGOLE, REGISTRO_CRESCE, TROCA_DONO };
static volatile int mudancaArmada = NADA;
int trakt_lista(const char *q, CatItem *s, int m) {
  (void)s; (void)m;
  if (strcmp(q, "watchlist")) return 0;
  switch (mudancaArmada) {
    case COLECAO_SEM_EFEITO: assert(col_definir_json(COLECAO_ALHEIA) > 0); break;
    case COLECAO_ENGOLE:     assert(col_definir_json(COLECAO_ENGOLE_A) > 0); break;
    case TROCA_DONO:         snprintf(dono, sizeof dono, "dono-B"); break;
    default: break;
  }
  mudancaArmada = NADA;
  return 0;
}

// ------------------------------------------------------------- fileiras.c
// So o registro, que e o que a montagem e a home reescrevem sozinhas.
static char registro[64][192];
static int nRegistro;
void fil_registrar(const char *c, const char *t, const char *a, const char *tp, int itens) {
  int i;
  (void)t; (void)a; (void)tp; (void)itens;
  for (i = 0; i < nRegistro; i++) if (!strcmp(registro[i], c)) return;
  if (nRegistro < 64) snprintf(registro[nRegistro++], sizeof registro[0], "%s", c);
}
int   fil_n(void)                          { return nRegistro; }
const char *fil_chave(int i)               { return (i >= 0 && i < nRegistro) ? registro[i] : ""; }
int   fil_linha_oculta(int i)              { (void)i; return 0; }
int   fil_linha_tipo(int i)                { (void)i; return FIL_TIPO_AUTO; }
int   fil_linha_tam(int i)                 { (void)i; return FIL_TAM_PADRAO; }
void  fil_gravar_registro(void)            { }
int   fil_podar_catalogos(const char *const *ids, const char *const *bases, int n) {
  (void)ids; (void)bases; (void)n; return 0; }
int   fil_limite(void)                     { return 3; }
const char *fil_hero_fonte(void)           { return "auto"; }
int   fil_oculta(const char *c)            { (void)c; return 0; }
int   fil_tem_ordem(void)                  { return 0; }
int   fil_unir(const char *const *c, int n, int *s, int m) {
  int i; (void)c; for (i = 0; i < n && i < m; i++) s[i] = i; return i; }

// ------------------------------------------------------------------ o resto
static volatile int montagens;
void  marco(const char *n)                 { if (!strcmp(n, "montar: inicio")) montagens++; }
void  SDL_Delay(Uint32 ms)                 { usleep(ms * 1000); }
int   ajustes_idioma_ingles(void)          { return 0; }
int   ajustes_cw_ligado(void)              { return 1; }
int   ajustes_cw_estilo(void)              { return 0; }
int   ajustes_posteres_deitados(void)      { return 0; }
int   ajustes_rotulos_poster(void)         { return 1; }
int   ajustes_hero_fonte(void)             { return 0; }
int   ajustes_cw_fonte(void)               { return 0; }
int   ajustes_tmdb_ligado(void)            { return 0; }
int   ajustes_tmdb_basico(void)            { return 0; }
int   ajustes_tmdb_arte(void)              { return 0; }
int   ajustes_tmdb_elenco(void)            { return 0; }
int   ajustes_tmdb_cw(void)                { return 0; }
const char *ajustes_tmdb_idioma(void)      { return "pt-BR"; }
const char *ajustes_tmdb_chave(void)       { return ""; }
const char *i18n(const char *s)            { return s; }
int   simkl_ativo(void)                    { return 0; }
int   simkl_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   simkl_e_a_seguir(const char *id)     { (void)id; return 0; }
int   simkl_plantowatch(CatItem *s, int m) { (void)s; (void)m; return 0; }
int   ajustes_salvos_no_simkl(void)        { return 0; }
int   trakt_enfeitar_lote(CatItem *s, int n) { (void)s; return n; }
int   trakt_social(CatItem *s, int m)      { (void)s; (void)m; return 0; }
int   trakt_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   trakt_e_a_seguir(const char *id)     { (void)id; return 0; }
const char *nuvem_trakt_cliente(void)      { return ""; }
int   arte_reserva_registrar(const char *url, const char *imdb, int poster) {
  (void)url; (void)imdb; (void)poster; return 1; }

// ------------------------------------------------------------------ o teste
// Quieto = sem montagem no ar por 300 ms seguidos: o fim de montar() zera
// `buscando` e pode disparar outra volta logo em seguida (repetirAoFim).
static void esperarQuieto(void) {
  int i, quieto = 0;
  for (i = 0; i < 500 && !buscando; i++) usleep(2000);
  for (i = 0; i < 10000 && quieto < 150; i++) {
    usleep(2000);
    quieto = buscando ? 0 : quieto + 1;
  }
  assert(quieto >= 150);
}

static int temFileira(const char *catId) {
  int r;
  for (r = 0; r < cat_n_fileiras(); r++)
    if (!strcmp(cat_fileira(r)->catId, catId)) return 1;
  return 0;
}
static int fileirasNaTela(void) { return cat_n_fileiras(); }
static void listar(const char *rotulo) {
  int r;
  printf("    %s:", rotulo);
  for (r = 0; r < cat_n_fileiras(); r++) printf(" %s(%d)", cat_fileira(r)->catId, cat_fileira(r)->n);
  printf("\n");
}
static int cacheEmDisco(void) {
  char p[PATH_MAX + 64]; struct stat st;
  caminhoDado(p, sizeof p, "catalogo-rede.bin");
  return stat(p, &st) == 0;
}
static void apagarCache(void) {
  char p[PATH_MAX + 64]; caminhoDado(p, sizeof p, "catalogo-rede.bin"); unlink(p);
}
static void zerarPedidos(void) {
  pthread_mutex_lock(&pedTrava); pedidosCatalogo = 0; pthread_mutex_unlock(&pedTrava);
}
// Todo item de fileira de catalogo na tela veio do dono `quem`.
static int itensSoDe(const char *quem) {
  int r, i;
  for (r = 0; r < cat_n_fileiras(); r++) {
    const CatFileira *f = cat_fileira(r);
    for (i = 0; i < f->n; i++)
      if (!strstr(cat_item(f->ini + i)->titulo, quem)) return 0;
  }
  return 1;
}

int main(void) {
  const char *tmp = getenv("TMPDIR");
  snprintf(dirDados, sizeof dirDados, "%snuvio-montagem-estrutura-%d",
           tmp && *tmp ? tmp : "/tmp/", (int)getpid());
  assert(mkdir(dirDados, 0700) == 0);
  desc_tmdb(dirDados);      // e quem guarda a pasta da descoberta (dirArteDesc)
  homeestado_iniciar();

  // ---------------------------------------------------------------- caso 1
  printf("==> colecoes chegando no meio, com o buscado bastando\n");
  mudancaArmada = COLECAO_SEM_EFEITO;
  desc_iniciar();
  esperarQuieto();
  listar("tela");
  assert(montagens == 1);                     // nada descartado, nada refeito
  assert(fileirasNaTela() == 3 && temFileira("cat_a") && temFileira("cat_b") &&
         temFileira("cat_c"));
  assert(pedidosCatalogo == 3);
  assert(cacheEmDisco());
  assert(homeestado_contexto_valido() && homeestado_tem_fileira(AID "_movie_cat_a"));
  // O "proximo arranque": mesma conta, mesmas colecoes (agora locais).
  homeestado_iniciar();
  assert(homeestado_contexto_valido());       // snapshot salvo sob a estrutura final
  cat_definir_tudo(NULL, 0, NULL, 0);
  assert(cat_ler_cache(dirDados) == 1 && cat_n_fileiras() == 3);
  puts("ok  estrutura no meio: uma montagem, fileiras na tela, snapshot e cache validos");

  // ---------------------------------------------------------------- caso 2
  printf("\n==> colecao que engole uma fileira ja buscada\n");
  montagens = 0; zerarPedidos();
  mudancaArmada = COLECAO_ENGOLE;
  desc_repetir();
  esperarQuieto();
  listar("tela");
  // Duas voltas, e NENHUMA descartada: a primeira publicou, a segunda so
  // existe para trazer o catalogo que a colecao liberou.
  assert(montagens == 2);
  assert(fileirasNaTela() == 3 && !temFileira("cat_a") && temFileira("cat_b") &&
         temFileira("cat_c") && temFileira("cat_d"));
  assert(foiPedido("cat_d") == 1);
  assert(cacheEmDisco());
  puts("ok  estrutura que pede catalogo novo: publica primeiro, depois um ciclo de rede");

  // ---------------------------------------------------------------- caso 3
  printf("\n==> registro de fileiras crescendo nao conta como mudanca\n");
  { HomeContexto a, b;
    homeestado_contexto(&a);
    fil_registrar("collection_nova", "Nova", "", "", 3);
    homeestado_contexto(&b);
    assert(homeestado_mudancas(&a, &b) == 0); }
  puts("ok  fil_registrar sem escolha da pessoa nao muda a assinatura");

  // ---------------------------------------------------------------- caso 4
  printf("\n==> troca de dono no meio continua descartando\n");
  montagens = 0; zerarPedidos();
  apagarCache();
  mudancaArmada = TROCA_DONO;
  desc_repetir();
  esperarQuieto();
  listar("tela");
  assert(montagens == 2);                     // descartada e refeita
  assert(fileirasNaTela() == 3 && itensSoDe("dono-B"));
  assert(cacheEmDisco());
  cat_definir_tudo(NULL, 0, NULL, 0);
  assert(cat_ler_cache(dirDados) == 1 && itensSoDe("dono-B"));
  snprintf(dono, sizeof dono, "dono-A");
  assert(cat_ler_cache(dirDados) == 0);       // e o do B nao serve ao A
  puts("ok  identidade no meio: descarta, e nada do dono anterior na tela ou no cache");

  puts("montagem_estrutura: tudo ok");
  return 0;
}
