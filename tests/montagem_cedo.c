// O TRAKT NAO ESPERA OS ADDONS, E A VOLTA CONDENADA NAO VAI ATE O FIM.
//
// Log da C9 do dono (1.4.6-dev, master), escolhendo o perfil 1:
//   ~315s montar: inicio
//   ~315s [sync] addons: perfil 1 (ativo 1) -> 12 linha(s)
//   ~315s [desc] remontagem pedida; roda ao fim do ciclo atual
//   ~345s manifestos lidos                        (30 s de manifestos)
//   ~357s [rede] falha 28 em <catalogo>           (um catalogo no timeout)
//   ~360s [trakt] watchlist: 112 ; collection: 98 (45 s depois do perfil)
//   ~360s [desc] montagem descartada: remontagem pedida no meio; recomecando
//   ~360s montar: inicio                          (segunda volta inteira)
//
// descoberta.c inteiro por #include, montar() de verdade, com catalogo.c,
// homeestado.c, colecoes.c e catordem.c de verdade (o mesmo arranjo de
// tests/montagem_estrutura.c). So a rede, o Trakt e fileiras.c sao dubles.
//
//   1. addons novos ANTES de a volta ler a lista: atendido pela propria volta
//      (uma montagem, nenhum descarte, nenhum catalogo pedido duas vezes), e os
//      manifestos da lista nova baixados em paralelo, nao um a um;
//   2. credencial nova no meio (desc_repetir): a volta para no ponto seguro
//      seguinte e recomeca — a condenada nao pede catalogo nenhum;
//   3. addons novos DEPOIS de a lista ser lida: condena, para esperando os
//      catalogos, recomeca;
//   4. tela vazia: watchlist na tela ENQUANTO os catalogos ainda estao na rede,
//      e no fim uma copia so de cada titulo;
//   5. home inteira na tela: a lista nova e mesclada (marca + acrescenta) sem
//      esperar os catalogos;
//   6. um catalogo pendurado nao segura a rodada: publica sem ele e segue.
//
//   bash tests/montagem_cedo.sh      (SANITIZE=1 para ASan/UBSan)
#include <assert.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#if !defined(MONTAGEM_PERFIS_REAIS) && !defined(NV_TEST_SESSAO_PROPRIA)
// These six discovery cases have one fixed account/profile per process.
// The CW owner fixture instead links real perfis.c, including invalidation.
#include "stub_perfisessao.h"
#endif
int arte_reserva_episodios(const char *imdb, const char *corpo) { (void)imdb; (void)corpo; return 0; }

int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_busca_cinemeta(void) { return 1; }
int ajustes_busca_nuvio(void) { return 0; }   // #311: Primeiro (padrao)
int ajustes_ocultar_nao_lancados(void) { return 0; }   // #369: descoberta.c le o ajuste
int ajustes_ocultar_filmes_vistos(void) { return 0; }    // #366: descoberta.c le o ajuste
int ajustes_fonte_biblioteca(void) { return 0; }
#include "../src/descoberta.c"
// Session, addon and social functions belong to this fixture or real modules.
#define NV_TEST_SERVIDORES_APENAS
#include "jellyfin_stub.inc"
#undef NV_TEST_SERVIDORES_APENAS
#include "nvtmp.h"
Uint32 SDL_GetTicks(void) { return 0; }

#define BASE "https://addon.example/abc"
#define AID  "app.addon.demo"

static const char *MANIFESTO =
  "{\"id\":\"" AID "\",\"name\":\"Addon\",\"resources\":[\"catalog\"],\"catalogs\":["
  "{\"type\":\"movie\",\"id\":\"cat_a\",\"name\":\"Cat A\"},"
  "{\"type\":\"movie\",\"id\":\"cat_b\",\"name\":\"Cat B\"},"
  "{\"type\":\"movie\",\"id\":\"cat_c\",\"name\":\"Cat C\"},"
  "{\"type\":\"movie\",\"id\":\"cat_d\",\"name\":\"Cat D\"},"
  "{\"type\":\"movie\",\"id\":\"cat_e\",\"name\":\"Cat E\"},"
  "{\"type\":\"movie\",\"id\":\"cat_f\",\"name\":\"Cat F\"}]}";

// ------------------------------------------------------------ a identidade
static char dirDados[PATH_MAX];
static const char *fixtureDono = "dono";
const char *sessao_usuario(void)  { return __atomic_load_n(&fixtureDono, __ATOMIC_ACQUIRE); }
#ifndef MONTAGEM_PERFIS_REAIS
int         perfis_ativo(void)    { return 1; }
#endif
const char *dados_dir(void)       { return dirDados; }
char *dados_caminho(char *dst, unsigned size, const char *name) {
  snprintf(dst, size, "%s/%s", dirDados, name); return dst;
}
int ajustes_nota_episodio(int source) { (void)source; return 1; }
void addons_catalogo_chave(const char *id, const char *base, const char *type,
                           const char *catalog, char *dst, unsigned size) {
  (void)base; snprintf(dst, size, "%s_%s_%s", id, type, catalog);
}
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
// A lista "nova" do caso 1 troca a URL do addon (outro token no caminho, como
// a conta faz): e isso que torna inutil o manifesto largado com a velha.
static unsigned versaoAddons = 1;
// Current addons.h snapshots borrow the same lock as fixture list writers.
// This is a real ownership boundary, not an unlocked success stub.
static pthread_mutex_t fixtureAddonListLock = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local unsigned fixtureAddonListDepth;
static unsigned fixtureAddonListSnapshots;
void addons_lista_travar(void) {
  assert(!fixtureAddonListDepth);
  assert(!pthread_mutex_lock(&fixtureAddonListLock));
  fixtureAddonListDepth = 1;
  __atomic_add_fetch(&fixtureAddonListSnapshots, 1, __ATOMIC_RELAXED);
}
void addons_lista_destravar(void) {
  assert(fixtureAddonListDepth == 1);
  fixtureAddonListDepth = 0;
  assert(!pthread_mutex_unlock(&fixtureAddonListLock));
}
#define BASE_NOVA BASE "-v2"
static unsigned versaoAtual(void) { return __atomic_load_n(&versaoAddons, __ATOMIC_ACQUIRE); }
static const char *baseAtual(void)     { return versaoAtual() > 1 ? BASE_NOVA : BASE; }
int   addons_n(void)                   { return 1; }
const char *addons_base(int i)         { (void)i; return baseAtual(); }
int   addons_ativo(int i)              { (void)i; return 1; }
static int fixtureMeta;
int   addons_fornece(int i, int oque)     { (void)i; return fixtureMeta && oque == ADD_META; }
int   addons_sondado(int i)              { (void)i; return fixtureMeta; }
const char *addons_id_manifesto(int i) { (void)i; return AID; }
const char *addons_nome(int i)         { (void)i; return "Addon"; }
unsigned addons_versao(void)           { return versaoAtual(); }
// Fixture fakes the account/rows; a missing full snapshot defers exclusion.
int addons_copiar_identidades(AddonIdentidade *saida, int max, unsigned versao, int perfil) {
  (void)saida; (void)max; (void)versao; (void)perfil; return -1;
}
int addons_travar_lista(unsigned versao, int perfil) { (void)versao; (void)perfil; return 0; }
void addons_destravar_lista(void) {}
const char *addons_base_por_id(const char *id) { return id && !strcmp(id, AID) ? baseAtual() : ""; }
void  addons_manifesto_lido(int i, const char *c) { (void)i; (void)c; }
int addons_identidade_recebida(const char *base, unsigned versao, NvPerfilSessao sessao,
                              const char *corpo) {
  (void)corpo;
  return nv_perfil_sessao_atual(sessao) && versao == versaoAtual() && !strcmp(base, baseAtual());
}
int addons_manifesto_recebido(const char *base, unsigned versao, NvPerfilSessao sessao,
                             const char *corpo) {
  return addons_identidade_recebida(base, versao, sessao, corpo);
}

// ------------------------------------------------------------------ o relogio
static unsigned long long agoraMs(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned long long)t.tv_sec * 1000ull + (unsigned long long)t.tv_nsec / 1000000ull;
}

// ------------------------------------------------------------------ a rede
enum { NADA, ADDONS_NA_1A_FILEIRA, ESPERAR_LISTA, PENDURAR_C };
static volatile int armadoCatalogo = NADA;
static volatile int listaViuAntesDoCatalogo = -1;
static char imdbEsperado[32];
static volatile int pedidosCatalogo, pendurados;
// Manifesto baixado NO FIO DA MONTAGEM = o rede_baixar serial de lerManifesto,
// e nao os fios de maniLargar.
// The native assembly entry emits its marker before creating any workers.
// A fresh pthread lifetime starts with role0, even if pthread_t is reused.
static _Thread_local int fixtureFioMontagem;
static volatile int manifestosEmSerie;
static pthread_mutex_t pedTrava = PTHREAD_MUTEX_INITIALIZER;
/* Hooks only for the focused cold/warm publication fixture. Defaults keep
 * the original six scenarios unchanged. */
static void (*fixtureCatalogo)(const char *id);
static void (*fixtureMarco)(const char *name);
static void (*fixtureTrakt)(int social);
static int fixtureCW, fixtureSocial, fixtureCWEmpty;

static int imdbNaTela(const char *imdb, int (*campo)(const CatItem *)) {
  int i, k = 0;
  for (i = 0; i < cat_n(); i++) {
    const CatItem *c = cat_item(i);
    if (c && !strcmp(c->imdb, imdb) && (!campo || campo(c))) k++;
  }
  return k;
}
static int ehNaLista(const CatItem *c) { return c->naLista; }

static char *(*fixtureRede)(const char *url);
char *rede_baixar(const char *url, int t) {
  if (fixtureRede) return fixtureRede(url);
  assert(!fixtureAddonListDepth); // snapshots must release before transport
  char buf[600];
  const char *p;
  char id[16] = "";
  int armado;
  (void)t;
  if (strstr(url, "/manifest.json")) {
    if (fixtureFioMontagem)
      __atomic_add_fetch(&manifestosEmSerie, 1, __ATOMIC_SEQ_CST);
    return strdup(MANIFESTO);
  }
  p = strstr(url, "/catalog/movie/");
  if (!p) return NULL;
  snprintf(id, sizeof id, "%.5s", p + 15);
  if (fixtureCatalogo) fixtureCatalogo(id);
  pthread_mutex_lock(&pedTrava);
  pedidosCatalogo++;
  armado = armadoCatalogo;
  if (armado == ADDONS_NA_1A_FILEIRA || armado == ESPERAR_LISTA) armadoCatalogo = NADA;
  pthread_mutex_unlock(&pedTrava);
  if (armado == ADDONS_NA_1A_FILEIRA) {
    // A lista JA foi lida: e o caso que tem de condenar a volta.
    desc_repetir_addons();
  } else if (armado == ESPERAR_LISTA) {
    // O CATALOGO SEGURA A RESPOSTA ate a watchlist aparecer na tela (ou 3 s).
    // Se ela so entrasse no fim, como antes, isto esperaria os 3 s em vao.
    int i, viu = 0;
    for (i = 0; i < 300 && !viu; i++) {
      viu = imdbNaTela(imdbEsperado, ehNaLista) > 0;
      if (!viu) usleep(10000);
    }
    listaViuAntesDoCatalogo = viu;
  } else if (armado == PENDURAR_C && !strcmp(id, "cat_c")) {
    __atomic_add_fetch(&pendurados, 1, __ATOMIC_SEQ_CST);
    usleep(2500 * 1000);          // bem mais que o teto de espera do teste
    __atomic_sub_fetch(&pendurados, 1, __ATOMIC_SEQ_CST);
  }
  snprintf(buf, sizeof buf,
           "{\"metas\":[{\"id\":\"tt%s1\",\"type\":\"movie\",\"name\":\"%s 1\",\"poster\":\"p.jpg\"},"
           "{\"id\":\"tt%s2\",\"type\":\"movie\",\"name\":\"%s 2\",\"poster\":\"p.jpg\"}]}",
           id + 4, id, id + 4, id);
  return strdup(buf);
}
char *rede_baixar_com(const char *u, int t, const char *const *c) { (void)c; return rede_baixar(u, t); }
char *rede_baixar_st(const char *u, int t, const char *const *c, int *status) {
  char *body = rede_baixar_com(u, t, c);
  if (status) *status = body ? 200 : 0;
  return body;
}

// ------------------------------------------------------------------ o Trakt
static char listaWl[4][32];
static int nListaWl;
static volatile int listasPedidas;
int trakt_lista(const char *q, CatItem *s, int m) {
  int i;
  if (strcmp(q, "watchlist")) return 0;
  __atomic_add_fetch(&listasPedidas, 1, __ATOMIC_SEQ_CST);
  for (i = 0; i < nListaWl && i < m; i++) {
    memset(&s[i], 0, sizeof s[i]);
    snprintf(s[i].imdb, sizeof s[i].imdb, "%s", listaWl[i]);
    snprintf(s[i].tipo, sizeof s[i].tipo, "movie");
    snprintf(s[i].titulo, sizeof s[i].titulo, "Lista %s", listaWl[i]);
    snprintf(s[i].poster, sizeof s[i].poster, "p.jpg");
    s[i].naLista = 1;
  }
  return i;
}
// Mesmo dublê, na forma que cresce: descoberta.c pede a lista por aqui agora.
int trakt_lista_cresc(const char *q, CatItem **s, int m) {
  int n;
  *s = m > 0 ? calloc((size_t)m, sizeof(CatItem)) : NULL;
  if (!*s) return 0;
  n = trakt_lista(q, *s, m);
  if (n <= 0) { free(*s); *s = NULL; return 0; }
  return n;
}
// Synthetic account changes land at the assembly marker, before list read.
enum { S_NADA, S_ADDONS, S_CREDENCIAL };
static volatile int armadoInicio = S_NADA;
// O servico social proprio (recomenda.c) fica fora deste teste: a uniao e so o que o Trakt trouxe.
int   recomenda_social_mesclar(CatItem *i, int nTrakt, int max) { (void)i; (void)max; return nTrakt; }
int recomenda_social_mesclar_sessao(CatItem *i, int nTrakt, int max, uint64_t sessao) {
  return nv_perfil_sessao_atual(sessao) ? recomenda_social_mesclar(i, nTrakt, max) : 0;
}
unsigned recomenda_geracao(void) { return 1; }
int trakt_social(CatItem *s, int m) {
  if (fixtureTrakt) fixtureTrakt(1);
  (void)s; (void)m;
  if (fixtureSocial && m > 0) {
    memset(s, 0, sizeof *s);
    snprintf(s->imdb, sizeof s->imdb, "ttSocial");
    snprintf(s->tipo, sizeof s->tipo, "movie");
    snprintf(s->titulo, sizeof s->titulo, "Social");
    snprintf(s->poster, sizeof s->poster, "social.jpg");
    return 1;
  }
  return 0;
}

// ------------------------------------------------------------- fileiras.c
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
int   fil_lista_e_deste_perfil(int p)       { (void)p; return 1; }
unsigned fil_perfil_geracao(void)          { return 0; }
FilPassada fil_passada_ler(void)          { FilPassada p = {0, 0}; return p; }
int   fil_passada_valida(const FilPassada *p) { (void)p; return 1; }
void  fil_registrar_de(const FilPassada *p, const char *c, const char *t, const char *a, const char *k, int n) { (void)p; (void)c; (void)t; (void)a; (void)k; (void)n; }
#ifndef NV_TEST_FIL_REGISTRAR_PROPRIO
void  fil_registrar_se_couber_de(const FilPassada *p, const char *c, const char *t, const char *a, const char *k) { (void)p; (void)c; (void)t; (void)a; (void)k; }
#endif
int   fil_podar_catalogos(const char *const *ids, const char *const *bases, int n,
                          int perfilDaLista) {
  (void)ids; (void)bases; (void)n; (void)perfilDaLista; return 0; }
int fil_podar_catalogos_de(const char *const *ids, const char *const *bases, int n,
                           int perfil, unsigned versao) {
  (void)ids; (void)bases; (void)n; (void)perfil; (void)versao; return 0;
}
int fil_marcar_declaradas(const char *const *chaves, int n, int perfilDaLista) { (void)chaves; (void)n; (void)perfilDaLista; return 0; }
int fil_marcar_sem_addon(const char *const *ids, const char *const *bases, const int *ativos, int n, int perfilDaLista) {
  (void)ids; (void)bases; (void)ativos; (void)n; (void)perfilDaLista; return 0;
}
int   fil_addon_novo(const char *id, const char *base) { (void)id; (void)base; return 0; }
int   addons_perfil_da_lista(void)         { return 0; }
int addons_base_desligada(const char *b) { (void)b; return 0; }   // 203-desligados
int   fil_limite(void)                     { return 3; }
// Dubles da escolha da cota (#126), como em homejanelas.c.
int fil_escolhida(const char *c) { (void)c; return -1; }
int fil_migrar_197(const char *const *c, int n) { (void)c; (void)n; return 0; }
void fil_registrar_se_couber(const char *c, const char *t, const char *a,
                             const char *tp) { (void)c; (void)t; (void)a; (void)tp; }
const char *fil_hero_fonte(void)           { return "auto"; }
int   fil_oculta(const char *c)            { (void)c; return 0; }
int   fil_adicionada_na_tv(const char *c) { (void)c; return 0; }
int   fil_estado_chave(const char *c) { (void)c; return -1; }
int   fil_tem_ordem(void)                  { return 0; }
int   fil_unir(const char *const *c, int n, int *s, int m) {
  int i; (void)c; for (i = 0; i < n && i < m; i++) s[i] = i; return i; }

// ------------------------------------------------------------------ o resto
static volatile int montagens;
void  marco(const char *n) {
  if (!strcmp(n, "montar: inicio")) {
    fixtureFioMontagem = 1; montagens++;
    // This point is after the round captured generation/session and before
    // manifest/list snapshots. Consume once so a retry cannot repeat it.
    int a = __atomic_exchange_n(&armadoInicio, S_NADA, __ATOMIC_ACQ_REL);
    if (a == S_ADDONS) {
      addons_lista_travar();
      __atomic_add_fetch(&versaoAddons, 1, __ATOMIC_RELEASE);
      addons_lista_destravar();
      desc_repetir_addons();
    } else if (a == S_CREDENCIAL) desc_repetir();
  }
  if (fixtureMarco) fixtureMarco(n);
}
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
int   ajustes_cw_fonte(void)               { return fixtureCW ? AJ_CWF_TRAKT : AJ_CWF_CONTA; }
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
int   simkl_ativo(void)                    { return 0; }

// "A seguir" da conta (#199): so para linkar; sem vistos, nada semeia.
int   trakt_ativo(void)                    { return 1; }
int   ajustes_cw_do_episodio_mais_alto(void) { return 1; }
int   contalib_sementes_a_seguir(ContaSemente *s, int m, int a) { (void)s; (void)m; (void)a; return 0; }
int   simkl_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   simkl_e_a_seguir(const char *id)     { (void)id; return 0; }
int   simkl_plantowatch(CatItem *s, int m) { (void)s; (void)m; return 0; }
int   ajustes_salvos_no_simkl(void)        { return 0; }
int   trakt_enfeitar_lote(CatItem *s, int n) { (void)s; return n; }
int trakt_enfeitar_lote_sessao(CatItem *s, int n, uint64_t sessao) {
  return nv_perfil_sessao_atual(sessao) ? trakt_enfeitar_lote(s, n) : 0;
}
int trakt_continuar(CatItem *s, int m) {
  if (fixtureTrakt) fixtureTrakt(0);
  if (!fixtureCW || fixtureCWEmpty || m < 1) return 0;
  memset(s, 0, sizeof *s);
  snprintf(s->imdb, sizeof s->imdb, "ttCW");
  snprintf(s->tipo, sizeof s->tipo, "movie");
  snprintf(s->titulo, sizeof s->titulo, "CW");
  snprintf(s->poster, sizeof s->poster, "cw.jpg");
  s->progresso = 20;
  return 1;
}
int   trakt_e_a_seguir(const char *id)     { (void)id; return 0; }
int trakt_cw_concluiu(const char *id, long long ms) { (void)id; (void)ms; return 0; }
int   trakt_progresso_ocultar(const char *i, int o) { (void)i; (void)o; return 0; }
int   trakt_continuar_falhou(void)         { return 0; }
const char *nuvem_trakt_cliente(void)      { return ""; }
int   arte_reserva_registrar(const char *url, const char *imdb, int poster) {
  (void)url; (void)imdb; (void)poster; return 1; }

// ------------------------------------------------------------------ o teste
static void esperarQuieto(void) {
  unsigned long long inicio = agoraMs(), quietoDesde = 0;
  // Darwin can coalesce sleeps. Measure 300 ms, not 150 assumed 2 ms wakes.
  // The real accessor also covers the current CW publication/queued retry,
  // under its existing locks; abandoned catalogue workers are not included.
  while (!desc_montando() && agoraMs() - inicio < 1000) usleep(2000);
  inicio = agoraMs();
  for (;;) {
    unsigned long long agora;
    usleep(2000);
    agora = agoraMs();
    if (desc_montando()) quietoDesde = 0;
    else if (!quietoDesde) quietoDesde = agora;
    if (quietoDesde && agora - quietoDesde >= 300) return;
    assert(agora - inicio < 20000);
  }
}
static int temFileira(const char *catId) {
  int r;
  for (r = 0; r < cat_n_fileiras(); r++)
    if (!strcmp(cat_fileira(r)->catId, catId)) return 1;
  return 0;
}
static void listar(const char *rotulo) {
  int r;
  printf("    %s:", rotulo);
  for (r = 0; r < cat_n_fileiras(); r++) printf(" %s(%d)", cat_fileira(r)->catId, cat_fileira(r)->n);
  printf("  [%d itens]\n", cat_n());
}
static void zerar(void) {
  montagens = 0;
  pthread_mutex_lock(&pedTrava); pedidosCatalogo = 0; pthread_mutex_unlock(&pedTrava);
}

// The collector must count a truly serial request, and must not turn a CW
// worker into an assembly worker. Run/reset this before the six real cases.
static void *fixtureManifestoEmWorker(void *u) {
  (void)u;
  trakt_continuar(NULL, 0);
  char *body = rede_baixar(BASE "/manifest.json", 20);
  assert(body); free(body);
  return NULL;
}
static void conferirMedidorManifestos(void) {
  assert(!fixtureFioMontagem);
  __atomic_store_n(&manifestosEmSerie, 0, __ATOMIC_SEQ_CST);
  marco("montar: inicio");
  char *body = rede_baixar(BASE "/manifest.json", 20);
  assert(body); free(body);
  assert(__atomic_load_n(&manifestosEmSerie, __ATOMIC_SEQ_CST) == 1);
  pthread_t worker;
  assert(!pthread_create(&worker, NULL, fixtureManifestoEmWorker, NULL));
  assert(!pthread_join(worker, NULL));
  assert(__atomic_load_n(&manifestosEmSerie, __ATOMIC_SEQ_CST) == 1);
  fixtureFioMontagem = 0;
  __atomic_store_n(&manifestosEmSerie, 0, __ATOMIC_SEQ_CST);
  montagens = 0;
}

int main(void) {
  conferirMedidorManifestos();
#ifndef MONTAGEM_PERFIS_REAIS
  NvPerfilSessao fixa = nv_perfil_sessao_ler();
  CatItem controle = {0};
  assert(perfis_ativo() == 1 && !strcmp(sessao_usuario(), "dono"));
  assert(nv_perfil_sessao_atual(fixa) && !nv_perfil_sessao_atual(fixa + 1));
  assert(nv_perfil_sessao_travar(fixa)); nv_perfil_sessao_destravar();
  assert(!nv_perfil_sessao_travar(fixa + 1));
  assert(trakt_enfeitar_lote_sessao(&controle, 1, fixa) == 1);
  assert(!trakt_enfeitar_lote_sessao(&controle, 1, fixa + 1));
  assert(recomenda_social_mesclar_sessao(&controle, 1, 1, fixa) == 1);
  assert(!recomenda_social_mesclar_sessao(&controle, 1, 1, fixa + 1));
#endif
  const char *tmp = getenv("TMPDIR");
  snprintf(dirDados, sizeof dirDados, "%snuvio-montagem-cedo-%d",
           nvt("/"), (int)getpid());
  assert(mkdir(dirDados, 0700) == 0);
  desc_tmdb(dirDados);
  homeestado_iniciar();
  assert(cat_n() == 0);

  // ---------------------------------------------------------------- caso 4
  // Primeiro, com a tela VAZIA: e o unico momento em que ela esta vazia.
  printf("==> tela vazia: watchlist na tela antes dos catalogos\n");
  nListaWl = 2;
  snprintf(listaWl[0], sizeof listaWl[0], "ttW1");
  snprintf(listaWl[1], sizeof listaWl[1], "ttW2");
  snprintf(imdbEsperado, sizeof imdbEsperado, "ttW1");
  armadoCatalogo = ESPERAR_LISTA;
  zerar();
  desc_iniciar();
  esperarQuieto();
  listar("tela");
  assert(listaViuAntesDoCatalogo == 1);
  assert(montagens == 1 && listasPedidas == 1);
  assert(cat_n_fileiras() == 3 && temFileira("cat_a") && temFileira("cat_b") && temFileira("cat_c"));
  // O rascunho de rabo das publicacoes em partes nao vira copia a mais.
  assert(imdbNaTela("ttW1", ehNaLista) == 1 && imdbNaTela("ttW2", ehNaLista) == 1);
  assert(!parcialNaTela);
  puts("ok  watchlist na tela enquanto os catalogos estao na rede; uma copia no fim");

  // ---------------------------------------------------------------- caso 5
  printf("\n==> home inteira na tela: lista nova mesclada sem esperar catalogo\n");
  nListaWl = 3;
  snprintf(listaWl[2], sizeof listaWl[2], "ttW3");
  snprintf(imdbEsperado, sizeof imdbEsperado, "ttW3");
  listaViuAntesDoCatalogo = -1;
  armadoCatalogo = ESPERAR_LISTA;
  zerar();
  desc_repetir();
  esperarQuieto();
  listar("tela");
  assert(listaViuAntesDoCatalogo == 1);
  assert(montagens == 1);
  assert(imdbNaTela("ttW1", ehNaLista) == 1 && imdbNaTela("ttW3", ehNaLista) == 1);
  assert(cat_n_fileiras() == 3);
  puts("ok  volta silenciosa: marca e acrescenta a lista antes do fim, publicacao final sem duplicata");

  // ---------------------------------------------------------------- caso 1
  printf("\n==> addons novos antes de a volta ler a lista\n");
  armadoInicio = S_ADDONS;
  manifestosEmSerie = 0;
  zerar();
  desc_repetir();
  esperarQuieto();
  listar("tela");
  assert(montagens == 1);           // nem descartada, nem refeita
  assert(pedidosCatalogo == 3);
  assert(manifestosEmSerie == 0);
  puts("ok  lista nova antes da leitura: a propria volta atende, sem descarte, manifestos em paralelo");

  // ---------------------------------------------------------------- caso 2
  printf("\n==> credencial nova no meio: para e recomeca, sem pedir catalogo\n");
  armadoInicio = S_CREDENCIAL;
  zerar();
  desc_repetir();
  esperarQuieto();
  listar("tela");
  assert(montagens == 2);
  assert(pedidosCatalogo == 3);     // so os da volta nova: a condenada parou antes
  assert(cat_n_fileiras() == 3);
  puts("ok  volta condenada para no ponto seguro seguinte; nenhum catalogo a toa");

  // ---------------------------------------------------------------- caso 3
  printf("\n==> addons novos depois de a lista ser lida\n");
  armadoCatalogo = ADDONS_NA_1A_FILEIRA;
  zerar();
  desc_repetir();
  esperarQuieto();
  listar("tela");
  assert(montagens == 2);
  assert(cat_n_fileiras() == 3 && temFileira("cat_a") && temFileira("cat_b") && temFileira("cat_c"));
  puts("ok  lista ja lida: a volta e condenada e recomeca com a nova");

  // ---------------------------------------------------------------- caso 6
  printf("\n==> um catalogo pendurado nao segura a rodada\n");
  { unsigned long long t0;
    int i;
    armadoCatalogo = PENDURAR_C;
    zerar();
    t0 = agoraMs();
    desc_repetir();
    esperarQuieto();
    listar("tela");
    printf("    volta em %llu ms (o pendurado leva 2500)\n", agoraMs() - t0 - 300);
    // cat_c ficou de fora nesta volta (ou com a linha anterior, que o snapshot
    // guarda) e a rodada seguinte trouxe cat_d para a vaga que sobrou.
    assert(agoraMs() - t0 < 2500 + 300);
    assert(__atomic_load_n(&pendurados, __ATOMIC_SEQ_CST) == 1);
    assert(temFileira("cat_a") && temFileira("cat_b"));
    // O fio largado termina depois, sozinho, sem escrever em memoria de ninguem
    // (SANITIZE=1 confere).
    for (i = 0; i < 400 && __atomic_load_n(&pendurados, __ATOMIC_SEQ_CST); i++) usleep(10000);
    assert(pendurados == 0);
    usleep(50000);
    armadoCatalogo = NADA; }
  puts("ok  catalogo lento largado: a home segue, o fio termina sozinho");

  assert(__atomic_load_n(&fixtureAddonListSnapshots, __ATOMIC_RELAXED) > 0);
  assert(!fixtureAddonListDepth);
  puts("montagem_cedo: tudo ok");
  return 0;
}
