// TESTE DE INTEGRACAO REAL NA HOME (src/home.c + src/faroldica.c — issue 48).
// Inclui src/home.c diretamente (mesmo arranjo de tests/homepos.c e
// tests/cw_ok_home.c) para provar:
//   1. P2 entra como coluna 0 da primeira fileira do Farol (farol_gosto1),
//      nunca duplicada em farol_gosto2; cartazes deslocam para 1..n-1;
//   2. cartao com acao == FP_ACAO_NADA NAO entra na ordem de foco:
//      DOWN do hero cai em col 1, LEFT em col 1 abre menu, UP/DOWN entre
//      fileiras nunca cai em col 0, ponteiroCard(r,0) e ignorado, e
//      alvoScrollFil(r, 1, ...) devolve 0 para manter o cartao a vista;
//   3. cartao focavel entra na col 0; OK executa abrir/tocar/salvar/comparar/
//      escolha (home_pediu_abrir/tocar + home_item_focado, salvos_tem,
//      faroldica_pediu_comparar/escolha);
//   4. Voltar longo (SDLK_ESCAPE >= NV_HOLD_MS) dispensa via avisodisp e tira
//      a coluna 0 na proxima sincronizacao;
//   5. movimento reduzido (ajustes_animacoes_reduzidas) assenta foco em 1 passo
//      e caminho ocioso nao relê/copia dica quando a geracao nao muda.
#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "avisodisp.h"
#include "salvos.h"
// Relogio simulado de SDL_GetTicks() para testar hold de Voltar sem sleep!
static Uint32 fakeTicks = 1000u;
#include "../src/home.c"
Uint32 SDL_GetTicks(void) { return fakeTicks; }

// -------------------------------------------- dubles minimos da UI/ambiente
static int stubPerfil = 1;
static int stubRecomendar = 1;
static int stubSeguro = 0;
static long long stubAgoraS = 1767225600LL + 300LL * 86400LL + 36000LL;
static int stubOrigemChamou = 0;
static char stubOrigemImdb[64];
static unsigned stubCenaGer = 1u;

static long long relogioFake(void) { return stubAgoraS; }
int perfis_ativo(void) { return stubPerfil; }
uint64_t perfis_sessao_geracao(void) { return 1; }
int perfis_sessao_atual(uint64_t s) { return s == 1; }
int seguro_perfil_ativo(void) { return stubSeguro; }
int farolsinal_recomendar(void) { return stubRecomendar; }
int farolsinal_modelo(FarolGosto *out) {
  if (out) { memset(out, 0, sizeof *out); out->titulos = 12; }
  return 1;
}
void farolsinal_origem(const CatItem *ci, int origem) {
  (void)origem;
  stubOrigemChamou++;
  snprintf(stubOrigemImdb, sizeof stubOrigemImdb, "%s", ci ? ci->imdb : "");
}
int farol_fileira(const char *ch) {
  return ch && (!strcmp(ch, "farol_gosto1") || !strcmp(ch, "farol_gosto2"));
}
unsigned farol_cena_geracao(void) { return stubCenaGer; }
const char *farol_motivo(const char *ch, int col, const char *imdb) {
  (void)ch; (void)col; (void)imdb; return NULL;
}

static int stubAnimReduzida = 0;
int ajustes_animacoes_reduzidas(void) { return stubAnimReduzida; }
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

// Stubs leves de modulos da UI nao exercitados pelo teste de foco/acao
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
  (void)ci; (void)eps; (void)n; (void)limiarPct; (void)agoraMs; (void)saida;
  return 0;
}
int menu_ocupado(Uint32 a) { (void)a; return 0; }
void menu_fechar(void) {}
int detail_aberto(void) { return 0; }
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

// Helpers de tecla
static void teclaDown(SDL_Keycode k) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  home_evento(&e);
}
static void teclaUp(SDL_Keycode k) {
  SDL_Event e = {0}; e.type = SDL_KEYUP; e.key.keysym.sym = k;
  home_evento(&e);
}
static void teclaCurta(SDL_Keycode k) {
  teclaDown(k);
  fakeTicks += 50u;
  teclaUp(k);
}

static void montarCatalogoComFarol(void) {
  CatItem it[18];
  CatFileira fils[3];
  int i;
  memset(it, 0, sizeof it);
  memset(fils, 0, sizeof fils);
  for (i = 0; i < 18; i++) {
    snprintf(it[i].imdb, sizeof it[i].imdb, "tt100%04d", i);
    strcpy(it[i].tipo, "movie");
    snprintf(it[i].titulo, sizeof it[i].titulo, "Titulo %d", i);
    strcpy(it[i].meta, "2024 · 95 min");
  }
  // Fileira 0: farol_gosto1 (6 itens: 0..5)
  strcpy(fils[0].chave, "farol_gosto1");
  strcpy(fils[0].titulo, "Para quem gosta de Drama");
  fils[0].ini = 0; fils[0].n = 6;
  // Fileira 1: farol_gosto2 (6 itens: 6..11)
  strcpy(fils[1].chave, "farol_gosto2");
  strcpy(fils[1].titulo, "Escolhidos para voce");
  fils[1].ini = 6; fils[1].n = 6;
  // Fileira 2: catalogo comum (6 itens: 12..17)
  strcpy(fils[2].chave, "addon_pop");
  strcpy(fils[2].titulo, "Populares");
  strcpy(fils[2].base, "https://a.invalid");
  strcpy(fils[2].catId, "pop");
  strcpy(fils[2].tipo, "movie");
  fils[2].ini = 12; fils[2].n = 6;
  cat_definir_tudo(it, 18, fils, 3);
}

static int acharFileira(const char *chave) {
  for (int r = 0; r < nFileiras; r++)
    if (!strcmp(fileiras[r].chave, chave)) return r;
  return -1;
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
    fprintf(stderr, "farol_dica_home: TMPDIR obrigatorio\n");
    return 1;
  }
  snprintf(dir, sizeof dir, "%s/nuvio-farol-dica-home-%ld", tmp, (long)getpid());
  limparDir(dir);
  assert(mkdir(dir, 0700) == 0);
  setenv("HOME", dir, 1);

  dados_iniciar(dir);
  salvos_perfil(1);
  salvos_iniciar();
  avisodisp_esquecer();
  faroldica_teste_reiniciar();
  faroldica_teste_relogio(relogioFake);
  montarCatalogoComFarol();

  // ---- CENARIO A: dica com acao: nada NAO entra na ordem de foco ----
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"h_nada\","
    "\"l1\":{\"m\":\"FT_DICA_TRAKT\"},"
    "\"acao\":{\"tipo\":\"nada\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);

  focoHero = 1;
  sincronizarFileiras();
  int r1 = acharFileira("farol_gosto1");
  int r2 = acharFileira("farol_gosto2");
  int r3 = acharFileira("addon_pop");
  assert(r1 >= 0 && r2 >= 0 && r3 >= 0);

  // P2 so entra na PRIMEIRA fileira do Farol (nunca duplica em gosto2)
  assert(fileiras[r1].temDica == 1);
  assert(fileiras[r1].dicaFocavel == 0);
  assert(fileiras[r1].n == 7);     // col 0 = dica + 6 cartazes (1..6)
  assert(fileiras[r2].temDica == 0);
  assert(fileiras[r2].n == 6);
  // Mapeamento de indices: col 0 = -1 (dica), col 1 = poster 0, col 6 = poster 5
  assert(fileiraItemIndice(&fileiras[r1], 0) == -1);
  assert(fileiraItemIndice(&fileiras[r1], 1) == 0);
  assert(fileiraItemIndice(&fileiras[r1], 6) == 5);

  // Descer do Hero -> cai na col 1, NUNCA col 0!
  focoHero = 1;
  foco.fileira = r1; foco.coluna = 0; foco.colunaLembrada[r1] = 0;
  teclaCurta(SDLK_DOWN);
  assert(!focoHero);
  assert(foco.fileira == r1);
  assert(foco.coluna == 1);

  // Esquerda em col 1 -> abre menu lateral, NAO vai para col 0!
  teclaCurta(SDLK_LEFT);
  assert(home_pediu_menu() == 1);
  assert(foco.coluna == 1);

  // Direita vai para col 2; esquerda volta a col 1; mais esquerda abre menu
  teclaCurta(SDLK_RIGHT);
  assert(foco.coluna == 2);
  teclaCurta(SDLK_LEFT);
  assert(foco.coluna == 1);
  assert(home_pediu_menu() == 0);
  teclaCurta(SDLK_LEFT);
  assert(home_pediu_menu() == 1);
  assert(foco.coluna == 1);

  // Descer para r2 e voltar para r1 -> nunca cai em col 0
  teclaCurta(SDLK_DOWN);
  assert(foco.fileira == r2);
  foco.colunaLembrada[r1] = 0; // mesmo que algo tente lembrar 0
  teclaCurta(SDLK_UP);
  assert(foco.fileira == r1);
  assert(foco.coluna == 1);

  // Ponteiro/mouse em col 0 ignorado quando acao == nada
  ponteiroCard(r1, 0);
  assert(foco.coluna == 1);

  // alvoScrollFil ao voltar para col 1 devolve 0 para mostrar o cartao na col 0
  assert(alvoScrollFil(r1, 1, 240.0f, 0.0f) == 0.0f);

  // ---- CENARIO B: dica focavel na col 0 + todas acoes via OK na Home ----
  // 1) abrir
  stubAgoraS += 86400LL;
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"h_abrir\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt1000003\"]},"
    "\"acao\":{\"tipo\":\"abrir\",\"alvo\":\"tt1000003\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  sincronizarFileiras();
  r1 = acharFileira("farol_gosto1");
  assert(fileiras[r1].temDica == 1 && fileiras[r1].dicaFocavel == 1);
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  // Pressao longa OK bloqueada no cartao de dica (nao abre ctx_abrir(-1))
  assert(foco_pode_pressao_longa() == 0);
  stubOrigemChamou = 0;
  teclaCurta(SDLK_RETURN);
  assert(home_pediu_abrir() == 1);
  assert(home_pediu_tocar() == 0);
  {
    HomeItem hi;
    assert(home_item_focado(&hi) == 1);
    assert(hi.indice == 3);
    assert(!strcmp(cat_item(hi.indice)->imdb, "tt1000003"));
    assert(stubOrigemChamou == 1 && !strcmp(stubOrigemImdb, "tt1000003"));
  }

  // 2) tocar
  stubAgoraS += 86400LL;
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"h_tocar\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt1000004\"]},"
    "\"acao\":{\"tipo\":\"tocar\",\"alvo\":\"tt1000004\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  sincronizarFileiras();
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  teclaCurta(SDLK_RETURN);
  assert(home_pediu_tocar() == 1);
  assert(home_pediu_abrir() == 0);
  {
    HomeItem hi;
    assert(home_item_focado(&hi) == 1);
    assert(hi.indice == 4);
  }

  // 3) salvar
  stubAgoraS += 86400LL;
  assert(salvos_tem("tt1000005") == 0);
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"h_salvar\","
    "\"l1\":{\"m\":\"FT_DICA_REVER\",\"a\":[\"tt1000005\"]},"
    "\"acao\":{\"tipo\":\"salvar\",\"alvo\":\"tt1000005\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  sincronizarFileiras();
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  teclaCurta(SDLK_RETURN);
  assert(home_pediu_abrir() == 0 && home_pediu_tocar() == 0);
  assert(salvos_tem("tt1000005") == 1);
  assert(dicaSalva == 1);

  // 4) comparar
  stubAgoraS += 86400LL;
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"h_cmp\","
    "\"l1\":{\"m\":\"FT_DICA_CURTO\",\"a\":[\"tt1000002\"]},"
    "\"acao\":{\"tipo\":\"comparar\",\"alvo\":\"tt1000002\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  sincronizarFileiras();
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  teclaCurta(SDLK_RETURN);
  {
    char idOut[FAROL_REF_TAM] = "";
    assert(faroldica_pediu_comparar(idOut, sizeof idOut) == 1);
    assert(!strcmp(idOut, "tt1000002"));
  }

  // 5) escolha + Voltar longo dispensa na Home (avisodisp)
  stubAgoraS += 86400LL;
  assert(faroldica_adotar_cena_json(
    "{\"v\":1,\"tela\":\"home\",\"pecas\":[{\"t\":\"dica\",\"id\":\"h_esc\","
    "\"l1\":{\"m\":\"FT_DICA_PERGUNTAS\"},"
    "\"acao\":{\"tipo\":\"escolha\"},\"icone\":\"aj_farol\"}]}",
    0, NULL, NULL, NULL) == 1);
  sincronizarFileiras();
  r1 = acharFileira("farol_gosto1");
  focoHero = 0; foco.fileira = r1; foco.coluna = 0;
  teclaCurta(SDLK_RETURN);
  assert(faroldica_pediu_escolha() == 1);

  // Movimento reduzido: foco assenta instantaneamente em 1 passo
  stubAnimReduzida = 1;
  assert(ajustes_animacoes_reduzidas() == 1);
  animFoco[r1][0] = 0.0f;
  home_atualizar(0.016f, fakeTicks);
  assert(animFoco[r1][0] == 1.0f);
  // Sem custo por quadro: dicaCacheGer ja coincide com faroldica_geracao()
  assert(dicaCacheGer == faroldica_geracao());
  // Desenho em modo reduzido funciona sem trava/erro
  faroldica_desenhar(&dicaCache, (GfxRect){ 48, 560, 212, 322 }, 0.05f,
                     1.0f, 1.0f, 1, 0);

  // Voltar longo na col 0 -> dispensa em avisodisp e remove col 0 na hora!
  teclaDown(SDLK_ESCAPE);
  fakeTicks += NV_HOLD_MS + 50u;
  home_atualizar(0.016f, fakeTicks);
  teclaUp(SDLK_ESCAPE);
  assert(faroldica_dispensada("h_esc") == 1);
  sincronizarFileiras();
  r1 = acharFileira("farol_gosto1");
  assert(fileiras[r1].temDica == 0);
  assert(fileiras[r1].n == 6);
  assert(fileiraItemIndice(&fileiras[r1], 0) == 0); // col 0 voltou a ser poster 0!

  limparDir(dir);
  puts("farol_dica (home): ok");
  return 0;
}
