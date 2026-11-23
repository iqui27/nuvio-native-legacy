// TESTE DE DESPACHO REAL DO ROTEADOR (src/home.c + src/faroldica.c + trechos
// reais de src/app.c: abrirTitulo, home_pediu_abrir e home_pediu_tocar).
// O runner tests/farol_dica.sh injeta os blocos literais de src/app.c nos
// marcadores APP_ABRIR_TITULO e APP_HOME_DISPATCH abaixo.
// Prova de ponta a ponta (sem arte, fora do catalogo):
//   1. Serie parada (progresso T2E4 + Diario nome, sem arte) -> OK abrir ->
//      app.c chama detail_abrir() com tipo="series", T2E4, arte=NULL.
//   2. Serie fora do catalogo (Diario T3E7, sem arte) -> OK tocar ->
//      app.c chama detail_abrir() + desc_episodios() com cwTocarT=3, cwTocarE=7.
//   3. Filme fora do catalogo (Diario nome, sem arte) -> OK tocar ->
//      app.c chama detail_abrir() + detail_pedir_reproduzir().
//   4. Serie fora do catalogo SEM episodio (Diario T0E0, sem arte) -> OK tocar ->
//      app.c chama detail_abrir() (pagina abre) sem detail_pedir_reproduzir().
#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "avisodisp.h"
#include "diario.h"
#include "progresso.h"
#include "salvos.h"

static Uint32 fakeTicks = 1000u;
#include "home_real.inc"
Uint32 SDL_GetTicks(void) { return fakeTicks; }

// ---- dubles minimos da UI/ambiente + espias do roteador de detalhe ----
static int stubPerfil = 1;
static uint64_t stubSessao = 1;
static long long stubAgoraS = 1767225600LL + 350LL * 86400LL + 36000LL;
static unsigned stubCenaGer = 1u;

static int espDetailAbrir;
static int stubDetailAberto;
static HomeItem espDetailItem;
static int espDetailReproduzir;
static int espDescartarRetido;
static int espDescEpisodios;
static int espDetailIdx = -1;

static long long relogioFake(void) { return stubAgoraS; }
int perfis_ativo(void) { return stubPerfil; }
uint64_t perfis_sessao_geracao(void) { return stubSessao; }
int perfis_sessao_atual(uint64_t s) { return s == stubSessao; }
int perfis_sessao_travar(uint64_t s) { return s == stubSessao; }
void perfis_sessao_destravar(void) {}
int perfil_infantil_ativo(void) { return 0; }
const char *desc_chave_tmdb(void) { return ""; }
const char *desc_tmdb_idioma(void) { return "pt-BR"; }
int mapa_recomendacoes_tmdb(const char *id, long tmdb, int serie,
                            const char *chave, const char *idioma,
                            MapaRecomendacoes *out) {
  (void)id; (void)tmdb; (void)serie; (void)chave; (void)idioma;
  if (out) memset(out, 0, sizeof *out);
  return 0;
}
int seguro_perfil_ativo(void) { return 0; }
int farolsinal_recomendar(void) { return 1; }
int farolsinal_modelo(FarolGosto *out) {
  if (out) { memset(out, 0, sizeof *out); out->titulos = 12; }
  return 1;
}
void farolsinal_origem(const CatItem *ci, int origem) { (void)ci; (void)origem; }
int farol_fileira(const char *ch) {
  return ch && (!strcmp(ch, "farol_gosto1") || !strcmp(ch, "farol_gosto2"));
}
unsigned farol_cena_geracao(void) { return stubCenaGer; }
const char *farol_motivo(const char *ch, int col, const char *imdb) {
  (void)ch; (void)col; (void)imdb; return NULL;
}

int ajustes_animacoes_reduzidas(void) { return 0; }
void ajustes_acento(float *r, float *g, float *b) { *r = 0.92f; *g = 0.25f; *b = 0.48f; }
int ajustes_vidro(void) { return 1; }
int ajustes_idioma(void) { return 0; }
int ajustes_home_layout(void) { return 0; }
int ajustes_hero_ligado(void) { return 1; }
int ajustes_cw_ligado(void) { return 1; }
int ajustes_cw_estilo(void) { return 0; }
int ajustes_cw_ordem(void) { return 0; }
int ajustes_cw_ok_toca(void) { return 1; }
int ajustes_cw_minimo(void) { return 5; }
int ajustes_cw_concluido(void) { return 90; }
int ajustes_posteres_deitados(void) { return 0; }
int ajustes_rotulos_poster(void) { return 0; }
int ajustes_ocultar_filmes_vistos(void) { return 0; }
int ajustes_ocultar_nao_lancados(void) { return 0; }
int ajustes_bib_home(void) { return 0; }
int ajustes_fonte_biblioteca(void) { return 0; }
int ajustes_social(void) { return 0; }
int ajustes_relogio_ligado(void) { return 1; }
int ajustes_menu_topo(void) { return 0; }
int ajustes_borda_foco(void) { return 1; }
int ajustes_itens_fileira(void) { return 12; }
int ajustes_expandir_poster(void) { return 0; }
float ajustes_expandir_poster_atraso(void) { return 2.0f; }
int ajustes_fundo_din(void) { return 0; }
void ajustes_fundo_din_cor(float c[3]) { c[0]=c[1]=c[2]=0.1f; }
int ajustes_amigos_estilo(void) { return 0; }
int ajustes_trailer_auto(void) { return 0; }
int ajustes_poster_escala_pct(void) { return 100; }
int ajustes_hero_fonte(void) { return 0; }
int ajustes_hero_diferente(void) { return 0; }
int ajustes_progresso_poster(void) { return 1; }
int ajustes_tinta_foco(void) { return 255; }
int ajustes_tinta_foco2(void) { return 200; }
int ajustes_sinopse_tamanho(void) { return 100; }
int ajustes_sufixo_tipo(void) { return 0; }
int ajustes_titulo_fil(void) { return 0; }
int ajustes_qualidade_posters(void) { return 0; }
int ajustes_poster_provider(void) { return 0; }
const char *ajustes_rpdb_chave(void) { return ""; }
const char *ajustes_topposters_chave(void) { return ""; }
int ajustes_trakt_notas(void) { return 0; }
int ajustes_imdb_notas(void) { return 0; }
int ajustes_interface_mobile(void) { return 0; }
int ajustes_tv_home(void) { return 1; }
int ajustes_largura_poster_dp(void) { return 126; }
int ajustes_trailer_hero(void) { return 0; }
unsigned ajustes_trailer_hero_espera_ms(void) { return 5000u; }
int ajustes_hero_arte_diferente(void) { return 0; }
int ajustes_cw_thumb_episodio(void) { return 0; }
float ajustes_espaco_fileiras(void) { return 28.0f; }
float ajustes_espaco_titulos(void) { return 24.0f; }
int ajustes_palco_fundo(void) { return 0; }
float ajustes_conteudo_x(void) { return 48.0f; }
float ajustes_hero_escala_logo(void) { return 1.0f; }
int ajustes_hero_lista(void) { return 10; }
float gfx_escala_txt_entrar(void) { return 1.0f; }
void gfx_escala_sair(float anterior) { (void)anterior; }
GLuint tex_obter_logo_larg_qualquer(const char *c, float l) { (void)c; (void)l; return 0; }
void marco(const char *nome) { (void)nome; }
const char *i18n(const char *s) { return s ? s : ""; }
int cacavisto_filme(const CatItem *ci) { (void)ci; return 0; }
int addons_n(void) { return 1; }
unsigned addons_versao(void) { return 0; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
const char *addons_nome_por_id(const char *id) { (void)id; return ""; }
int contalib_tem_conta(void) { return 0; }
int trakt_ativo(void) { return 0; }
int simkl_ativo(void) { return 0; }
int conta_ativa(void) { return 0; }
const char *sessao_usuario(void) { return "teste"; }
int desc_catalogos_fora(void) { return 0; }
const char *desc_genero_pt(const char *g) { return g ? g : ""; }
void desc_ocultar_vistos_atualizar(void) {}
void desc_sinopse_hero(const int *l, int n) { (void)l; (void)n; }
void sync_proteger_ajustes_locais(void) {}
void biblioteca_alternar_lista(int i) { (void)i; }
int prox_seguinte(const CatItem *ci, const CatEp *eps, int n, int limiarPct,
                  long long agoraMs, ProxSugestao *saida) {
  (void)limiarPct; (void)agoraMs;
  if (!ci || !eps || n <= 0 || !saida) return 0;
  for (int i = 0; i < n; i++) {
    if (eps[i].temporada == ci->temporada && eps[i].episodio == ci->episodio + 1) {
      memset(saida, 0, sizeof *saida);
      saida->temporada = eps[i].temporada;
      saida->episodio = eps[i].episodio;
      return 1;
    }
  }
  return 0;
}
int menu_ocupado(Uint32 a) { (void)a; return 0; }
void menu_fechar(void) {}
int detail_aberto(void) { return stubDetailAberto; }
float detail_progresso(void) { return 0.0f; }
int player_aberto(void) { return 0; }
int trailer_aberto(void) { return 0; }
int trailer_dono(void) { return 0; }
void trailer_fechar(void) {}
int ctx_aberto(void) { return 0; }
void ctx_abrir(int i) { (void)i; }
void ctx_abrir_fileira(const char *c, const char *t) { (void)c; (void)t; }
void ctx_abrir_filmenu(const char *c, const char *t, const char *p, int v) {
  (void)c; (void)t; (void)p; (void)v;
}
void ctx_fileira(const char *c, const char *t) { (void)c; (void)t; }
void ctx_dispensar_retomar(int o) { (void)o; }
void vertudo_abrir(const char *a, const char *b, const char *c, const char *d) {
  (void)a; (void)b; (void)c; (void)d;
}
void vertudo_colecao(const ColFolder *f) { (void)f; }
int homevazio_ativo(int n) { return n <= 0; }
int homevazio_evento(const SDL_Event *e) { (void)e; return 0; }
void cachearte_limpar_referencias_grupo(int g) { (void)g; }
void cachearte_estatisticas_pedir(void) {}
void tex_cache_marcar_larg(int g, const char *u, float l, int a, int b) {
  (void)g; (void)u; (void)l; (void)a; (void)b;
}
const char *tex_arquivo(const char *u) { return u; }
GLuint tex_obter_hero_quente(const char *u) { (void)u; return 0; }
unsigned long tex_hash_public(const char *u) { (void)u; return 0; }
GLuint tex_obter_logo_larg(const char *u, float w) { (void)u; (void)w; return 0; }
float tex_aspecto(const char *u) { (void)u; return 1.0f; }
int tex_falhou(const char *u) { (void)u; return 0; }
int tex_largura_fonte(const char *u) { (void)u; return 0; }
int ponteiro_ok_longo(void) { return 0; }
float gfx_escala(void) { return 1.0f; }
float gfx_escala_txt(void) { return 1.0f; }
float gfx_escala_tela_txt(void) { return 1.0f; }
float gfx_poster_k(void) { return 1.0f; }
void gfx_mini_liberar(GfxMini *m) { if (m) memset(m, 0, sizeof *m); }
void gfx_cor(GfxRect r, float ra, float cr, float cg, float cb, float ca) {
  (void)r; (void)ra; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_anel(GfxRect r, float ra, float e, float cr, float cg, float cb, float ca) {
  (void)r; (void)ra; (void)e; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_vidro_ou_solido(GfxRect r, float ra, float d, float a,
                         float cr, float cg, float cb, float ca) {
  (void)r; (void)ra; (void)d; (void)a; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_icone(GfxRect r, const char *n, float cr, float cg, float cb, float ca) {
  (void)r; (void)n; (void)cr; (void)cg; (void)cb; (void)ca;
}
TxtLinha txt_linha_corta(TxtEstilo e, const char *s, int r, int g, int b, int a, float w) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)a; (void)w;
  TxtLinha l = { 0, 40, 20, 0, 0 }; return l;
}
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) { (void)l; (void)x; (void)y; (void)a; }
float txt_bloco_corta(TxtEstilo e, const char *s, int r, int g, int b,
                      float x, float y, float w, float ld, float a, int ml) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)w; (void)ld; (void)a; (void)ml;
  return 40.0f;
}
void colfileiras_sincronizar(void) {}
int canal_e_chave(const char *c) { (void)c; return 0; }

// Espias usados diretamente pelo bloco real de src/app.c:
void player_descartar_retido(void) { espDescartarRetido++; }
void detail_abrir(const HomeItem *it) {
  espDetailAbrir++;
  stubDetailAberto = 1;
  if (it) { espDetailItem = *it; espDetailIdx = it->indice; }
}
int detail_indice(void) { return espDetailIdx; }
void detail_pedir_reproduzir(void) { espDetailReproduzir++; }
void desc_episodios(int indice, int forcar) {
  (void)indice; (void)forcar; espDescEpisodios++;
}
static int explorar_pediu_abrir(int *idx) { (void)idx; return 0; }
static void abrirPorIndice(int idx) { (void)idx; }

#define CW_ESPERA_MS 8000u
enum { TELA_HOME = 0, TELA_EXPLORAR = 1 };
static int tela = TELA_HOME;
static int homeAcaoDet = -1;
static Uint32 homeAcaoDetEm = 0;
static int cwRewatch, cwTocarT, cwTocarE, cwEspera;
static CatItem cwEsperaItem;
static Uint32 cwEsperaDesde;

/* APP_ABRIR_TITULO */

static void despacharAppHome(void) {
  int idx = -1;
  HomeItem it;
  (void)idx;
/* APP_HOME_DISPATCH */
}

static void passoEsperaApp(void) {
/* APP_CW_ESPERA */
}

static void teclaOk(void) {
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  home_evento(&e);
  fakeTicks += 50u;
  e.type = SDL_KEYUP;
  home_evento(&e);
}

static void montarCatalogoComFarol(void) {
  CatItem it[12];
  CatFileira fils[2];
  int i;
  memset(it, 0, sizeof it);
  memset(fils, 0, sizeof fils);
  for (i = 0; i < 12; i++) {
    snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
    strcpy(it[i].tipo, "movie");
    snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
    strcpy(it[i].meta, "2024 · 140 min");
  }
  strcpy(fils[0].chave, "farol_gosto1");
  strcpy(fils[0].titulo, "Para voce");
  fils[0].ini = 0; fils[0].n = 6;
  strcpy(fils[1].chave, "addon_pop");
  strcpy(fils[1].titulo, "Populares");
  strcpy(fils[1].base, "https://a.invalid");
  strcpy(fils[1].catId, "pop");
  strcpy(fils[1].tipo, "movie");
  fils[1].ini = 6; fils[1].n = 6;
  cat_definir_tudo(it, 12, fils, 2);
}

static int acharFileira(const char *chave) {
  for (int r = 0; r < nFileiras; r++)
    if (!strcmp(fileiras[r].chave, chave)) return r;
  return -1;
}

static void zerarEspias(void) {
  espDetailAbrir = 0;
  stubDetailAberto = 0;
  memset(&espDetailItem, 0, sizeof espDetailItem);
  espDetailReproduzir = 0;
  espDescartarRetido = 0;
  espDescEpisodios = 0;
  espDetailIdx = -1;
  cwRewatch = cwTocarT = cwTocarE = cwEspera = 0;
  memset(&cwEsperaItem, 0, sizeof cwEsperaItem);
  cwEsperaDesde = 0;
}

static void limparDir(const char *dir) {
  char cmd[512];
  snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
  if (system(cmd) != 0) {}
}

int main(void) {
  const char *tmp = getenv("TMPDIR");
  char dir[256];
  if (!tmp || !tmp[0]) {
    fprintf(stderr, "farol_dica_app: TMPDIR obrigatorio\n");
    return 1;
  }
  snprintf(dir, sizeof dir, "%s/nuvio-farol-dica-app-%ld", tmp, (long)getpid());
  limparDir(dir);
  assert(mkdir(dir, 0700) == 0);
  setenv("HOME", dir, 1);
  setenv("NUVIO_DADOS", dir, 1);

  dados_iniciar(dir);
  assert(!strcmp(dados_dir(), dir));
  salvos_perfil(1); salvos_iniciar();
  avisodisp_esquecer();
  faroldica_teste_reiniciar();
  faroldica_teste_relogio(relogioFake);
  montarCatalogoComFarol();

  // 1) Serie fora do catalogo (progresso T2E4 + Diario nome, sem arte) -> abrir
  {
    ProgRegistro pr;
    memset(&pr, 0, sizeof pr);
    pr.perfil = 1;
    strcpy(pr.chave, "tt7770010_s2e4");
    strcpy(pr.contentId, "tt7770010");
    strcpy(pr.tipo, "series");
    pr.temporada = 2; pr.episodio = 4;
    pr.posSeg = 1100; pr.durSeg = 2700;
    pr.lastWatchedMs = (stubAgoraS - 35LL * 86400LL) * 1000LL;
    pr.tocou = 1;
    assert(prog_aplicar_remoto(&pr) == 1);
    diario_nome_definir("tt7770010", "Andor", 2022, "");
  }
  home_atualizar(0.016f, fakeTicks);
  int r1 = acharFileira("farol_gosto1");
  assert(r1 >= 0 && fileiras[r1].temDica == 1 && fileiras[r1].dicaFocavel == 1);
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(espDetailAbrir == 1);
  assert(espDescartarRetido == 1);
  assert(espDetailItem.arte == NULL);
  {
    const CatItem *ci = cat_item(espDetailItem.indice);
    assert(ci && !strcmp(ci->imdb, "tt7770010"));
    assert(!strcmp(ci->tipo, "series"));
    assert(ci->temporada == 2 && ci->episodio == 4);
  }

  // 2) Serie fora do catalogo (Diario DIARIO_SERIE T3E7, sem arte) -> tocar
  stubAgoraS += 86400LL;
  {
    DiarioEntrada de;
    memset(&de, 0, sizeof de);
    strcpy(de.id, "tt7770011");
    de.tipo = DIARIO_SERIE;
    de.origem = DIARIO_ORIG_PLAYER;
    de.temporada = 3; de.episodio = 7;
    de.quando = stubAgoraS - 10LL * 86400LL;
    assert(diario_adicionar(1, &de) >= 0);
    diario_nome_definir("tt7770011", "Dark", 2017, "");
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_tocar_ser\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7770011\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7770011\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(espDetailAbrir == 1);
  assert(espDetailItem.arte == NULL);
  assert(cwTocarT == 3 && cwTocarE == 7);
  assert(cwEspera == 2 && espDescEpisodios == 1);
  {
    const CatItem *ci = cat_item(espDetailItem.indice);
    assert(ci && !strcmp(ci->imdb, "tt7770011") && !strcmp(ci->tipo, "series"));
  }
  {
    CatEp ep;
    char streamId[96] = "";
    memset(&ep, 0, sizeof ep);
    ep.temporada = 3; ep.episodio = 7;
    strcpy(ep.vid, "tt7770011:3:7");
    cat_definir_episodios(detail_indice(), &ep, 1);
    passoEsperaApp();
    assert(cwEspera == 0 && espDetailReproduzir == 1);
    assert(cwTocarT == 3 && cwTocarE == 7);
    assert(cat_id_stream(detail_indice(), cwTocarT, cwTocarE, streamId, sizeof streamId) == 1);
    assert(!strcmp(streamId, "tt7770011:3:7"));
  }

  // 3) Filme fora do catalogo (Diario nome, sem arte) -> tocar
  stubAgoraS += 86400LL;
  diario_nome_definir("tt7770012", "Her", 2013, "");
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_tocar_mov\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7770012\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7770012\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(espDetailAbrir == 1);
  assert(espDetailItem.arte == NULL);
  assert(espDetailReproduzir == 1);

  // 4) Serie fora do catalogo SEM episodio (Diario T0E0, sem arte) -> tocar
  //    Abre a pagina do detalhe (sem exigir arte) e nao pede reproducao de ep 0.
  stubAgoraS += 86400LL;
  {
    DiarioEntrada de;
    memset(&de, 0, sizeof de);
    strcpy(de.id, "tt7770013");
    de.tipo = DIARIO_SERIE;
    de.origem = DIARIO_ORIG_MANUAL;
    de.temporada = 0; de.episodio = 0;
    de.quando = stubAgoraS - 5LL * 86400LL;
    assert(diario_adicionar(1, &de) >= 0);
    diario_nome_definir("tt7770013", "Shogun", 2024, "");
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_tocar_ser0\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7770013\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7770013\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(espDetailAbrir == 1);
  assert(espDetailItem.arte == NULL);
  assert(espDetailReproduzir == 0);
  {
    const CatItem *ci = cat_item(espDetailItem.indice);
    assert(ci && !strcmp(ci->imdb, "tt7770013") && !strcmp(ci->tipo, "series"));
    assert(ci->episodio == 0);
  }

  // 5) Regressao r2: P2 formado com Diario T4E6 sem arte, e descoberta republica
  //    tt7723120 sem arte com T/E zerados (T0E0) entre composicao e OK.
  //    Preserva T4E6 ate o despacho, entra em espera e resolve ID tt7723120:4:6
  //    sem escrever T/E no item global do catalogo.
  stubAgoraS += 86400LL;
  montarCatalogoComFarol();
  {
    DiarioEntrada de;
    memset(&de, 0, sizeof de);
    strcpy(de.id, "tt7723120");
    de.tipo = DIARIO_SERIE;
    de.origem = DIARIO_ORIG_PLAYER;
    de.temporada = 4; de.episodio = 6;
    de.quando = stubAgoraS - 6LL * 86400LL;
    assert(diario_adicionar(1, &de) >= 0);
    diario_nome_definir("tt7723120", "Severance", 2022, "");
  }
  assert(cat_indice_por_imdb("tt7723120") < 0);
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r2_t0e0\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  assert(dicaCache.alvoTemporada == 4 && dicaCache.alvoEpisodio == 6);
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723120");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Severance");
    it[0].temporada = 0; it[0].episodio = 0;
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  fakeTicks += 16u;
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  assert(r1 >= 0 && fileiras[r1].temDica == 1 && fileiras[r1].dicaFocavel == 1);
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(espDetailAbrir == 1);
  assert(espDetailItem.arte == NULL);
  assert(cwTocarT == 4 && cwTocarE == 6);
  assert(cwEspera == 2 && espDescEpisodios == 1);
  assert(cwEsperaItem.temporada == 4 && cwEsperaItem.episodio == 6);
  assert(cat_item(espDetailItem.indice)->temporada == 0 &&
         cat_item(espDetailItem.indice)->episodio == 0);
  {
    CatEp eps[2];
    char streamId[96] = "";
    memset(eps, 0, sizeof eps);
    eps[0].temporada = 1; eps[0].episodio = 1; strcpy(eps[0].vid, "tt7723120:1:1");
    eps[1].temporada = 4; eps[1].episodio = 6; strcpy(eps[1].vid, "tt7723120:4:6");
    cat_definir_episodios(detail_indice(), eps, 2);
    passoEsperaApp();
    assert(cwEspera == 0 && espDetailReproduzir == 1);
    assert(cwTocarT == 4 && cwTocarE == 6);
    assert(cat_id_stream(detail_indice(), cwTocarT, cwTocarE, streamId, sizeof streamId) == 1);
    assert(!strcmp(streamId, "tt7723120:4:6"));
  }

  // 6) Regressao r2: republicacao com OUTRO episodio (T1E2) e progresso concluido
  //    no catalogo NAO desvia T4E6 nem aciona prox_seguinte do item global.
  stubAgoraS += 86400LL;
  montarCatalogoComFarol();
  {
    DiarioEntrada de;
    memset(&de, 0, sizeof de);
    strcpy(de.id, "tt7723121");
    de.tipo = DIARIO_SERIE;
    de.origem = DIARIO_ORIG_PLAYER;
    de.temporada = 4; de.episodio = 6;
    de.quando = stubAgoraS - 6LL * 86400LL;
    assert(diario_adicionar(1, &de) >= 0);
    diario_nome_definir("tt7723121", "Foundation", 2021, "");
  }
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r2_t1e2\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723121\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723121\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  assert(dicaCache.alvoTemporada == 4 && dicaCache.alvoEpisodio == 6);
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723121");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Foundation");
    it[0].temporada = 1; it[0].episodio = 2; it[0].progresso = 100;
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "farol_gosto1");
    strcpy(fils[0].titulo, "Para voce");
    fils[0].ini = 0; fils[0].n = 6;
    strcpy(fils[1].chave, "addon_pop");
    strcpy(fils[1].titulo, "Populares");
    strcpy(fils[1].base, "https://a.invalid");
    strcpy(fils[1].catId, "pop");
    strcpy(fils[1].tipo, "movie");
    fils[1].ini = 6; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  fakeTicks += 16u;
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(espDetailAbrir == 1);
  assert(cwTocarT == 4 && cwTocarE == 6);
  assert(cwEspera == 2 && espDescEpisodios == 1);
  assert(cwEsperaItem.temporada == 4 && cwEsperaItem.episodio == 6);
  assert(cat_item(espDetailItem.indice)->temporada == 1 &&
         cat_item(espDetailItem.indice)->episodio == 2);
  {
    CatEp eps[3];
    char streamId[96] = "";
    memset(eps, 0, sizeof eps);
    eps[0].temporada = 1; eps[0].episodio = 2; strcpy(eps[0].vid, "tt7723121:1:2");
    eps[1].temporada = 1; eps[1].episodio = 3; strcpy(eps[1].vid, "tt7723121:1:3");
    eps[2].temporada = 4; eps[2].episodio = 6; strcpy(eps[2].vid, "tt7723121:4:6");
    cat_definir_episodios(detail_indice(), eps, 3);
    passoEsperaApp();
    assert(cwEspera == 0 && espDetailReproduzir == 1);
    assert(cwTocarT == 4 && cwTocarE == 6);
    assert(cat_id_stream(detail_indice(), cwTocarT, cwTocarE, streamId, sizeof streamId) == 1);
    assert(!strcmp(streamId, "tt7723121:4:6"));
  }

  // 7) Guardas entre OK e despacho: troca de revisao de catalogo, troca de sessao,
  //    troca de perfil e dispensa rejeitam o pedido pendente.
  // 7a) Revisao de catalogo muda apos OK:
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  montarCatalogoComFarol();
  despacharAppHome();
  assert(espDetailAbrir == 0 && espDetailReproduzir == 0 && cwEspera == 0);

  // 7b) Sessao muda apos OK:
  stubAgoraS += 86400LL;
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"app_r2_guard\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt7723120\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt7723120\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  home_atualizar(0.016f, fakeTicks);
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  stubSessao++;
  despacharAppHome();
  assert(espDetailAbrir == 0 && espDetailReproduzir == 0 && cwEspera == 0);

  // 7c) Perfil muda apos OK (mesmo sem alterar stubSessao):
  fakeTicks += 16u;
  home_atualizar(0.016f, fakeTicks);
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  stubPerfil = 2;
  despacharAppHome();
  assert(espDetailAbrir == 0 && espDetailReproduzir == 0 && cwEspera == 0);
  stubPerfil = 1;

  // 7d) Dispensa apos OK:
  fakeTicks += 16u;
  home_atualizar(0.016f, fakeTicks);
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  zerarEspias();
  teclaOk();
  assert(faroldica_dispensar() == 1);
  despacharAppHome();
  assert(espDetailAbrir == 0 && espDetailReproduzir == 0 && cwEspera == 0);

  // 8) Cartao comum de Continuar assistindo preserva semantica normal (sem P2):
  {
    CatItem it[12];
    CatFileira fils[2];
    memset(it, 0, sizeof it);
    memset(fils, 0, sizeof fils);
    strcpy(it[0].imdb, "tt7723122");
    strcpy(it[0].tipo, "series");
    strcpy(it[0].titulo, "Pachinko");
    it[0].temporada = 1; it[0].episodio = 5; it[0].progresso = 40;
    for (int i = 1; i < 12; i++) {
      snprintf(it[i].imdb, sizeof it[i].imdb, "tt300%04d", i);
      strcpy(it[i].tipo, "movie");
      snprintf(it[i].titulo, sizeof it[i].titulo, "Catalogo %d", i);
      strcpy(it[i].meta, "2024 · 140 min");
    }
    strcpy(fils[0].chave, "continue_watching");
    strcpy(fils[0].titulo, "Continuar assistindo");
    fils[0].ini = 0; fils[0].n = 2;
    strcpy(fils[1].chave, "farol_gosto1");
    strcpy(fils[1].titulo, "Para voce");
    fils[1].ini = 2; fils[1].n = 6;
    cat_definir_tudo(it, 12, fils, 2);
  }
  fakeTicks += 16u;
  home_atualizar(0.016f, fakeTicks);
  int rCw = acharFileira("continue_watching");
  assert(rCw >= 0);
  focoHero = 0; foco.fileira = rCw; foco.coluna = 0;
  itemFoco.indice = fileiraItemIndice(&fileiras[rCw], 0);
  itemFoco.rect = (GfxRect){ 48, 200, 320, 180 };
  itemFoco.arte = NULL;
  itemFoco.titulo = "Pachinko";
  itemFoco.genero = NULL;
  itemFoco.meta = NULL;
  publicarItemFoco();
  zerarEspias();
  teclaOk();
  despacharAppHome();
  assert(espDetailAbrir == 1);
  assert(cwTocarT == 1 && cwTocarE == 5);
  assert(cwEspera == 2 && espDescEpisodios == 1);

  limparDir(dir);
  puts("farol_dica (app): ok");
  return 0;
}
