// UM CATALOGO QUE FALHOU UMA VEZ NAO SOME DA HOME PARA SEMPRE (#195).
//
// Relato (Samsung .tpk 1.6.1): "Bharat Binge catalog never works for me", e
// catalogos que aparecem numa conta e nao em outra. Medido no Mac: o Bharat
// Binge responde 503 a pedidos em rajada e serve "Netflix India" vazio. Ate a
// 1.6.2 o snapshot do homeestado FILTRAVA a montagem: so chave do snapshot era
// pedida. Quem falhou (ou veio vazio) na volta que gravou o snapshot ficava de
// fora dele, o seguinte tomava a vaga e o que falhou nunca mais era pedido.
//
// Mesmo arranjo de tests/montagem_estrutura.c: descoberta.c por #include,
// montar() de verdade, homeestado.c/catalogo.c/colecoes.c/catordem.c de
// verdade; a rede e dublê, com modo por catalogo. Limite 3, seis catalogos.
//
//   1. cat_b falha na primeira volta: a,c,d na tela; na seguinte cat_b
//      responde e volta ao lugar dele (a,b,c) — antes nem era pedido;
//   2. cat_a responde vazio: nao vira fileira so com titulo (b,c,d); quando
//      volta a ter itens, volta ao lugar;
//   3. snapshot cujas chaves nao existem mais: a home enche com os outros
//      candidatos — antes, 0 pedidos e 0 fileiras (log do @tonyh89);
//   4. manifesto sem resposta: o snapshot nao e gravado nessa volta.
//
//   bash tests/snapshot_falha.sh
#include <assert.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
int arte_reserva_episodios(const char *imdb, const char *corpo) { (void)imdb; (void)corpo; return 0; }

int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_busca_cinemeta(void) { return 1; }
int ajustes_busca_nuvio(void) { return 0; }   // #311: Primeiro (padrao)
int ajustes_ocultar_nao_lancados(void) { return 0; }   // #369: descoberta.c le o ajuste
#include "../src/descoberta.c"
#include "jellyfin_stub.inc"
unsigned recomenda_geracao(void) { return 1; }
Uint32 SDL_GetTicks(void) { return 0; }

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
int   addons_fornece(int i, int oque)     { (void)i; (void)oque; return 0; }
int   addons_sondado(int i)              { (void)i; return 0; }
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
// Modo por catalogo: 'o' responde 2 titulos, 'x' falha (503/timeout: NULL),
// 'v' responde {"metas":[]}. Indice = letra do catId (cat_a = 0).
static char modo[6] = "oooooo";
static volatile int manifestoFalha;
char *rede_baixar(const char *url, int t) {
  char buf[600];
  const char *p;
  char id[16] = "";
  (void)t;
  if (strstr(url, "/manifest.json")) return manifestoFalha ? NULL : strdup(MANIFESTO);
  p = strstr(url, "/catalog/movie/");
  if (!p) return NULL;
  snprintf(id, sizeof id, "%.5s", p + 15);
  pthread_mutex_lock(&pedTrava);
  if (pedidosCatalogo < 64) snprintf(pedidos[pedidosCatalogo], sizeof pedidos[0], "%s", id);
  pedidosCatalogo++;
  pthread_mutex_unlock(&pedTrava);
  if (id[4] >= 'a' && id[4] <= 'f') {
    char m = modo[id[4] - 'a'];
    if (m == 'x') return NULL;
    if (m == 'v') return strdup("{\"metas\":[]}");
  }
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
int   trakt_lista_cresc(const char *q, CatItem **s, int m) { (void)q; (void)s; (void)m; return 0; }
int trakt_lista(const char *q, CatItem *s, int m) { (void)q; (void)s; (void)m; return 0; }
int simkl_plantowatch(CatItem *s, int m) {
  (void)s; (void)m;
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
// Dubles da escolha da cota (#126): nada escolhido na TV, e o registro dos
// catalogos fora da cota nao interessa a este teste.
int fil_escolhida(const char *c) { (void)c; return -1; }
int fil_migrar_197(const char *const *c, int n) { (void)c; (void)n; return 0; }
void fil_registrar_se_couber(const char *c, const char *t, const char *a,
                             const char *tp) { (void)c; (void)t; (void)a; (void)tp; }
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
int   fil_limite(void)                     { return 3; }
const char *fil_hero_fonte(void)           { return "auto"; }
int   fil_oculta(const char *c)            { (void)c; return 0; }
int   fil_adicionada_na_tv(const char *c) { (void)c; return 0; }
int   fil_estado_chave(const char *c) { (void)c; return -1; }
int   fil_tem_ordem(void)                  { return 0; }
int   fil_unir(const char *const *c, int n, int *s, int m) {
  int i; (void)c; for (i = 0; i < n && i < m; i++) s[i] = i; return i; }

// ------------------------------------------------------------------ o resto
static volatile int montagens;
void  marco(const char *n)                 { if (!strcmp(n, "montar: inicio")) montagens++; }
void  SDL_Delay(Uint32 ms)                 { usleep(ms * 1000); }
int   ajustes_idioma_ingles(void)          { return 0; }
int ajustes_idioma(void) { return 0; }
int   ajustes_cw_ordem(void)               { return 0; }   // Padrao (issue #127)
int   ajustes_cw_concluido(void)           { return 90; }  // Percentual assistido de fabrica
int   ajustes_itens_fileira(void)          { return 12; }   // padrao (#163)
int   ajustes_cw_mostrar_nao_exibidos(void) { return 1; }
int   ajustes_cw_proximo(void) { return 1; }
int   ajustes_cw_ligado(void)              { return 1; }
int   ajustes_cw_estilo(void)              { return 0; }
int   ajustes_posteres_deitados(void)      { return 0; }
float ajustes_espaco_fileiras(void) { return 1.0f; }
float ajustes_espaco_titulos(void)  { return 1.0f; }
int   ajustes_rotulos_poster(void)         { return 1; }
int   ajustes_hero_fonte(void)             { return 0; }
int   ajustes_cw_fonte(void)               { return 0; }
int   ajustes_tmdb_ligado(void)            { return 0; }
int   ajustes_tmdb_basico(void)            { return 0; }
int   ajustes_meta_externo(void)           { return 0; }
int   ajustes_fundo_addon(void)            { return 0; }
int   ajustes_logo_addon(void)             { return 0; }
int   ajustes_meta_so_cinemeta(void)        { return 0; }
int   addons_aceita_id(int i, const char *t, const char *id) { (void)i; (void)t; (void)id; return -1; }
int   ajustes_tmdb_arte(void)              { return 0; }
int   ajustes_tmdb_elenco(void)            { return 0; }
int   ajustes_tmdb_cw(void)                { return 0; }
const char *ajustes_tmdb_idioma(void)      { return "pt-BR"; }
const char *ajustes_tmdb_chave(void)       { return ""; }
const char *i18n(const char *s)            { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
int   simkl_ativo(void)                    { return 1; }
int   trakt_ativo(void)                    { return 0; }   // #199: cw da conta so sem Trakt/Simkl
int   simkl_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   simkl_e_a_seguir(const char *id)     { (void)id; return 0; }
int   ajustes_salvos_no_simkl(void)        { return 1; }   // para simkl_plantowatch rodar
int   trakt_enfeitar_lote(CatItem *s, int n) { (void)s; return n; }
// O servico social proprio (recomenda.c) fica fora deste teste: a uniao e so o que o Trakt trouxe.
int   recomenda_social_mesclar(CatItem *i, int nTrakt, int max) { (void)i; (void)max; return nTrakt; }
int   trakt_social(CatItem *s, int m)      { (void)s; (void)m; return 0; }
int   trakt_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   trakt_continuar_falhou(void)        { return 0; }
int   trakt_e_a_seguir(const char *id)     { (void)id; return 0; }
int   trakt_progresso_ocultar(const char *i, int o) { (void)i; (void)o; return 0; }
const char *nuvem_trakt_cliente(void)      { return ""; }
int   arte_reserva_registrar(const char *url, const char *imdb, int poster) {
  (void)url; (void)imdb; (void)poster; return 1; }

// ------------------------------------------------------------------ o teste
static void esperarQuieto(void) {
  int i, quieto = 0;
  for (i = 0; i < 500 && !buscando; i++) usleep(2000);
  for (i = 0; i < 10000 && quieto < 150; i++) {
    usleep(2000);
    quieto = buscando ? 0 : quieto + 1;
  }
  assert(quieto >= 150);
}
static void listar(const char *rotulo) {
  int r;
  printf("    %s:", rotulo);
  for (r = 0; r < cat_n_fileiras(); r++) printf(" %s(%d)", cat_fileira(r)->catId, cat_fileira(r)->n);
  printf("\n");
}
static void zerarPedidos(void) {
  pthread_mutex_lock(&pedTrava); pedidosCatalogo = 0; pthread_mutex_unlock(&pedTrava);
}
// A tela e exatamente estas fileiras, nesta ordem, todas com itens.
static int tela(const char *a, const char *b, const char *c) {
  const char *q[3] = { a, b, c };
  int r;
  if (cat_n_fileiras() != 3) return 0;
  for (r = 0; r < 3; r++) {
    const CatFileira *f = cat_fileira(r);
    char id[16];
    snprintf(id, sizeof id, "cat_%s", q[r]);
    if (strcmp(f->catId, id) || f->n < 1) return 0;
  }
  return 1;
}
// Um arranque: snapshot relido do disco, e uma volta de montagem.
static void volta(void) {
  homeestado_iniciar();
  zerarPedidos();
  desc_repetir();
  esperarQuieto();
}

int main(void) {
  const char *tmp = getenv("TMPDIR");
  snprintf(dirDados, sizeof dirDados, "%snuvio-snapshot-falha-%d",
           tmp && *tmp ? tmp : "/tmp/", (int)getpid());
  assert(mkdir(dirDados, 0700) == 0);
  desc_tmdb(dirDados);
  homeestado_iniciar();

  printf("==> cat_b sem resposta na volta que grava o snapshot\n");
  memcpy(modo, "oxoooo", 6);
  desc_iniciar();
  esperarQuieto();
  listar("tela");
  assert(tela("a", "c", "d"));
  assert(homeestado_contexto_valido() && homeestado_tem_fileira(AID "_movie_cat_b"));
  memcpy(modo, "oooooo", 6);
  volta();
  listar("tela");
  assert(foiPedido("cat_b") == 1);
  assert(tela("a", "b", "c"));
  assert(!foiPedido("cat_d"));             // snapshot inteiro respondeu: nada alem dele
  puts("ok  catalogo que falhou volta na volta seguinte, no lugar dele");

  printf("\n==> cat_a responde vazio\n");
  memcpy(modo, "vooooo", 6);
  volta();
  listar("tela");
  assert(tela("b", "c", "d"));             // nada de fileira so com titulo
  memcpy(modo, "oooooo", 6);
  volta();
  listar("tela");
  assert(tela("a", "b", "c"));
  puts("ok  vazio nao ocupa vaga, e volta quando tiver itens");

  printf("\n==> snapshot com chaves que nao existem mais\n");
  { CatFileira velhas[3];
    int k;
    memset(velhas, 0, sizeof velhas);
    for (k = 0; k < 3; k++) snprintf(velhas[k].chave, sizeof velhas[k].chave, "sumiu_%d", k);
    homeestado_salvar(velhas, 3); }
  volta();
  listar("tela");
  assert(tela("a", "b", "c"));
  puts("ok  snapshot sem candidato vivo nao deixa a home vazia");

  printf("\n==> manifesto sem resposta\n");
  { CatFileira velhas[1];
    memset(velhas, 0, sizeof velhas);
    snprintf(velhas[0].chave, sizeof velhas[0].chave, "marca");
    homeestado_salvar(velhas, 1); }
  maniCacheLimpar(); // Force the failed network request instead of reusing a valid manifest.
  manifestoFalha = 1;
  volta();
  manifestoFalha = 0;
  homeestado_iniciar();
  assert(homeestado_contexto_valido() && homeestado_tem_fileira("marca"));
  puts("ok  volta sem manifesto nao grava snapshot");

  puts("snapshot_falha: tudo ok");
  return 0;
}
