// HOME PRECOMPUTADA: dentro da validade, as fileiras de catalogo que ja estao
// na tela (e no cache em disco) servem como resposta, sem rede.
//
// Mesmo arranjo de tests/snapshot_falha.c: descoberta.c por #include, montar()
// de verdade, homeestado.c/catalogo.c de verdade; a rede e duble.
//
//   1. primeira volta: tudo da rede, e a home fica fresca;
//   2. volta sem pedido da pessoa dentro da validade: zero pedidos de catalogo,
//      mesma tela, validade NAO renovada;
//   3. validade vencida: tudo da rede de novo, validade renovada;
//   4. desc_repetir() (credencial/addons/idioma/pessoa) vai a rede mesmo fresca;
//   5. volta com catalogo sem resposta nao renova a validade;
//   6. a validade vai no cabecalho do cache em disco e volta com ele;
//   7. relogio para tras e validade zerada nao servem a home.
//
//   bash tests/home_precomputada.sh
#include <assert.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
int arte_reserva_episodios(const char *imdb, const char *corpo) { (void)imdb; (void)corpo; return 0; }

void addons_manifesto_lido_base(int i, const char *base, const char *destino,
                                unsigned versao, const char *corpo) {
  (void)i; (void)base; (void)destino; (void)versao; (void)corpo;
}
void addons_lista_travar(void) {}
void addons_lista_destravar(void) {}
void ws_destino(const char *base, const char *usuario, int perfil, char out[20]) {
  (void)base; (void)usuario; (void)perfil; out[0] = 0;
}
int ajustes_ocultar_filmes_vistos(void) { return 0; }
int ajustes_social(void) { return 1; }
int ajustes_fonte_biblioteca(void) { return 0; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_busca_cinemeta(void) { return 1; }
int ajustes_busca_nuvio(void) { return 0; }   // #311: Primeiro (padrao)
int ajustes_ocultar_nao_lancados(void) { return 0; }   // #369: descoberta.c le o ajuste
#include "../src/descoberta.c"
#include "jellyfin_stub.inc"
#include "nvtmp.h"
void addons_catalogo_chave(const char *id, const char *base, const char *type,
                           const char *catalog, char *dst, unsigned size) {
  (void)base; snprintf(dst, size, "%s_%s_%s", id, type, catalog);
}
int addons_identidade_recebida(const char *base, unsigned versao, NvPerfilSessao sessao,
                              const char *corpo) {
  (void)base; (void)versao; (void)sessao; (void)corpo; return 1;
}
int addons_manifesto_recebido(const char *base, unsigned versao, NvPerfilSessao sessao,
                             const char *corpo) {
  (void)base; (void)versao; (void)sessao; (void)corpo; return 1;
}
int ajustes_cw_do_episodio_mais_alto(void) { return 1; }
int contalib_sementes_a_seguir(ContaSemente *s, int m, int a) { (void)s; (void)m; (void)a; return 0; }
int trakt_cw_concluiu(const char *id, long long ms) { (void)id; (void)ms; return 0; }
int vistoep_estado(const char *imdb, int t, int e) { (void)imdb; (void)t; (void)e; return -1; }
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
int fil_marcar_declaradas(const char *const *chaves, int n, int perfilDaLista) { (void)chaves; (void)n; (void)perfilDaLista; return 0; }
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
int   ajustes_cw_minimo(void)             { return 1; }
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

static int pedidosAgora(void) {
  int n;
  pthread_mutex_lock(&pedTrava); n = pedidosCatalogo; pthread_mutex_unlock(&pedTrava);
  return n;
}
static void voltaSemPedido(void) {
  homeestado_iniciar();
  zerarPedidos();
  desc_iniciar();
  esperarQuieto();
}

int main(void) {
  long long fresca;
  snprintf(dirDados, sizeof dirDados, "%snuvio-home-pre-%d", nvt("/"), (int)getpid());
  assert(mkdir(dirDados, 0700) == 0);
  desc_tmdb(dirDados);
  homeestado_iniciar();

  printf("==> primeira volta: tudo da rede\n");
  assert(!cat_home_fresca_em());
  zerarPedidos();
  desc_iniciar();
  esperarQuieto();
  listar("tela");
  assert(tela("a", "b", "c"));
  assert(pedidosAgora() == 3);
  fresca = cat_home_fresca_em();
  assert(fresca > 0 && (long long)time(NULL) - fresca < 60);
  puts("ok  rede inteira deixa a home fresca");

  printf("\n==> volta comum dentro da validade\n");
  cat_home_fresca(fresca - 30);   // marca conhecida para ver que nao renova
  voltaSemPedido();
  listar("tela");
  assert(tela("a", "b", "c"));
  assert(pedidosAgora() == 0);
  assert(cat_home_fresca_em() == fresca - 30);
  puts("ok  fileiras servidas da home precomputada, sem rede e sem renovar");

  printf("\n==> validade vencida\n");
  cat_home_fresca((long long)time(NULL) - NV_HOME_PRE_VALIDADE_S - 1);
  voltaSemPedido();
  listar("tela");
  assert(tela("a", "b", "c"));
  assert(pedidosAgora() == 3);
  assert((long long)time(NULL) - cat_home_fresca_em() < 60);
  puts("ok  home vencida vai a rede e renova");

  printf("\n==> desc_repetir com a home fresca\n");
  volta();
  assert(tela("a", "b", "c"));
  assert(pedidosAgora() == 3);
  puts("ok  pedido explicito sempre vai a rede");

  printf("\n==> catalogo sem resposta nao renova\n");
  fresca = (long long)time(NULL) - 100;
  cat_home_fresca(fresca);
  memcpy(modo, "oxoooo", 6);
  volta();
  memcpy(modo, "oooooo", 6);
  listar("tela");
  assert(foiPedido("cat_b") == 1);
  assert(cat_home_fresca_em() == fresca);
  puts("ok  volta parcial mantem o horario antigo");

  printf("\n==> validade no cache em disco\n");
  cat_home_fresca(fresca - 7);
  assert(cat_gravar_cache(dirDados));
  cat_home_fresca(0);
  assert(cat_ler_cache(dirDados));
  assert(cat_home_fresca_em() == fresca - 7);
  puts("ok  frescoEm volta com o cache");

  printf("\n==> relogio para tras e validade zerada\n");
  { long long agora = (long long)time(NULL);
    cat_home_fresca(agora + 3600);
    assert(!cat_home_fresca_valida(agora, NV_HOME_PRE_VALIDADE_S));
    cat_home_fresca(agora);
    assert(cat_home_fresca_valida(agora, NV_HOME_PRE_VALIDADE_S));
    assert(!cat_home_fresca_valida(agora, 0));
    cat_apagar_cache();
    assert(!cat_home_fresca_em()); }
  puts("ok  sem prova de idade, nada servido");

  printf("\n==> fileiras do Farol\n");
  { CatItem it[4];
    CatFileira fs[3], ord[9];
    static const char *nomes[9] = { "continue_watching", AID "_movie_cat_a", AID "_movie_cat_b",
      AID "_movie_cat_c", AID "_movie_cat_d", AID "_movie_cat_e", AID "_movie_cat_f",
      "farol_gosto2", "farol_gosto1" };
    int k;
    memset(it, 0, sizeof it); memset(fs, 0, sizeof fs); memset(ord, 0, sizeof ord);
    for (k = 0; k < 4; k++) { snprintf(it[k].imdb, sizeof it[k].imdb, "tt900%d", k); snprintf(it[k].tipo, sizeof it[k].tipo, "movie"); }
    snprintf(fs[0].chave, sizeof fs[0].chave, "%s", AID "_movie_cat_a"); fs[0].ini = 0; fs[0].n = 2;
    snprintf(fs[1].chave, sizeof fs[1].chave, "farol_gosto1"); fs[1].ini = 2; fs[1].n = 1;
    snprintf(fs[2].chave, sizeof fs[2].chave, "%s", AID "_movie_cat_b"); fs[2].ini = 3; fs[2].n = 1;
    cat_definir_tudo(it, 4, fs, 3);
    assert(cat_n_fileiras() == 3);
    assert(cat_gravar_cache(dirDados));
    assert(cat_ler_cache(dirDados));
    assert(cat_n_fileiras() == 2);
    for (k = 0; k < cat_n_fileiras(); k++) assert(strncmp(cat_fileira(k)->chave, "farol_", 6));
    puts("ok  farol_ do disco nao e publicada no arranque");

    cat_definir_tudo(it, 4, fs, 3);
    assert(cat_remover_fileiras_prefixo(dirDados, CAT_PREFIXO_FAROL) == 1);
    assert(cat_n_fileiras() == 2);
    assert(!strcmp(cat_fileira(1)->chave, AID "_movie_cat_b"));
    assert(cat_remover_fileiras_prefixo(dirDados, CAT_PREFIXO_FAROL) == 0);
    assert(cat_ler_cache(dirDados) && cat_n_fileiras() == 2);
    puts("ok  cat_remover_fileiras_prefixo tira da tela e do disco, o resto fica");

    for (k = 0; k < 7; k++) snprintf(ord[k].chave, sizeof ord[k].chave, "%s", nomes[k]);
    homeestado_salvar(ord, 7);
    assert(homeestado_contexto_valido());
    for (k = 0; k < 9; k++) snprintf(ord[k].chave, sizeof ord[k].chave, "%s", nomes[(k + 7) % 9]);
    ordenarPorSnapshot(ord, 9);
    for (k = 0; k < 9; k++) printf(" %s", ord[k].chave);
    printf("\n");
    assert(!strcmp(ord[0].chave, "continue_watching"));
    assert(!strcmp(ord[1].chave, "farol_gosto1"));
    assert(!strcmp(ord[5].chave, "farol_gosto2"));
    assert(!strcmp(ord[4].chave, AID "_movie_cat_c"));
    puts("ok  posicao de fabrica: gosto1 depois do Continuar, gosto2 depois da 5a"); }

  puts("home_precomputada: tudo ok");
  return 0;
}
