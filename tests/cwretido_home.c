// UM TITULO, UM LUGAR (cwretido.h; dono, 03/10: "o que ta no continue playing
// nao pode ta no continue watching; so entra la quando sair dessa fileira").
//
// Enquanto o cartao da ilha (ou a faixa "Retomar agora") segura X, a fileira
// "Continuar assistindo" da home NAO mostra X — nem depois de uma refacao que
// o publica na frente (fecharSessao -> desc_refazer_continuar). Quando o
// cartao solta X, ele volta NA FRENTE (cwfrente.h, o que app.c faz por
// desc_continuar_otimista), sem mexer nas outras fileiras. Inclui src/home.c
// direto, como tests/cwremover_home.c. Sem janela, rede ou TV.
#include <assert.h>
// Duples do cache de arte do Codex (mesmos de tests/home_layout.c).
void cachearte_marcar_grupo(int grupo, const char *url, int variante, int essencial, int emUso) {
  (void)grupo; (void)url; (void)variante; (void)essencial; (void)emUso; }
void cachearte_limpar_referencias_grupo(int grupo) { (void)grupo; }
void cachearte_estatisticas_pedir(void) {}
void tex_cache_marcar_larg(int grupo, const char *url, float larg, int essencial, int emUso) {
  (void)grupo; (void)url; (void)larg; (void)essencial; (void)emUso; }
#include "../src/home.c"
// heroAquecerVizinhos (home_atualizar) pede a arte dos vizinhos: sem textura aqui.
unsigned long tex_hash_public(const char *c) { (void)c; return 0; }
GLuint tex_obter_hero_quente(const char *c) { (void)c; return 0; }
void desc_sinopse_hero(const int *idx, int n) { (void)idx; (void)n; }
#include "cwfrente.h"

char *dados_ler(const char *nome) { (void)nome; return NULL; }
int   dados_gravar(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
int   dados_gravar_leve(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  (void)dst; (void)tam; (void)nome; return NULL;
}
int   dados_apagar(const char *nome) { (void)nome; return 1; }
void  dados_marcar_sujo(int leve) { (void)leve; }
// ajustes_aplicar_blob (abaixo) fala com linguas.c; nada disso entra na regra.
const char *ling_legenda(void) { return ""; }
const char *ling_audio(void) { return ""; }
const char *ling_conta_legenda_valor(void) { return ""; }
const char *ling_conta_legenda2_valor(void) { return ""; }
const char *ling_conta_audio_valor(void) { return ""; }
void ling_conta_legenda(const char *v) { (void)v; }
void ling_conta_legenda2(const char *v) { (void)v; }
void ling_conta_audio(const char *v) { (void)v; }
void selospacote_conta_do_blob(const char *blob) { (void)blob; }
int selospacote_n(void) { return 0; }
int   perfis_ativo(void) { return 1; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
const char *addons_nome_por_id(const char *id) { (void)id; return ""; }
int  tex_falhou(const char *u) { (void)u; return 0; }
int  tex_largura_fonte(const char *u) { (void)u; return 0; }
const char *tex_arquivo(const char *u) { (void)u; return NULL; }
int  player_aberto(void) { return 0; }
int  detail_aberto(void) { return 0; }
int  trailer_aberto(void) { return 0; }
int  trailer_tocando(void) { return 0; }
int ctx_aberto(void) { return 0; }
void ctx_abrir(int indice) { (void)indice; }
void ctx_fileira(const char *c, const char *t) { (void)c; (void)t; }
void ctx_dispensar_retomar(int on) { (void)on; }
void ctx_abrir_fileira(const char *c, const char *t) { (void)c; (void)t; }
void vertudo_abrir(const char *b, const char *t, const char *c, const char *ti) {
  (void)b; (void)t; (void)c; (void)ti;
}
void vertudo_colecao(const ColFolder *f) { (void)f; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }

static Uint32 relogio = 1000;
static void quadro(void) { relogio += 16; home_atualizar(0.016f, relogio); }

static const Fileira *naHome(const char *chave) {
  int r;
  for (r = 0; r < nFileiras; r++) if (!strcmp(fileiras[r].chave, chave)) return &fileiras[r];
  return NULL;
}
static const char *card(const Fileira *f, int c) {
  const CatItem *it = cat_item(fileiraItemIndice(f, c));
  return it ? it->imdb : "";
}
static void conferir(const char *quando, const char *esperado) {
  const Fileira *cw = naHome("continue_watching");
  char tem[200] = "";
  int c;
  for (c = 0; cw && c < cw->n; c++) {
    if (c) strcat(tem, " ");
    strcat(tem, card(cw, c));
  }
  if (strcmp(tem, esperado)) {
    fprintf(stderr, "FALHOU %s: Continuar assistindo = [%s], esperado [%s]\n", quando, tem, esperado);
    exit(1);
  }
  printf("ok  %s: [%s]\n", quando, tem);
}
static void item(CatItem *it, const char *imdb, const char *tipo, int t, int e, int prog) {
  memset(it, 0, sizeof *it);
  snprintf(it->imdb, sizeof it->imdb, "%s", imdb);
  snprintf(it->titulo, sizeof it->titulo, "T %s", imdb);
  snprintf(it->tipo, sizeof it->tipo, "%s", tipo);
  it->temporada = t; it->episodio = e; it->progresso = prog;
}
// O que desc_continuar_otimista faz com o catalogo (descoberta.c), sem a trava.
static void naFrente(const CatItem *novo) {
  static CatItem atual[20], saida[20];
  int n = cat_copiar_fileira("continue_watching", atual, 20, NULL), mudou;
  int k = cw_frente_compor(atual, n, novo, saida, 20, &mudou);
  if (mudou) cat_trocar_continuar(saida, k);
}

int main(void) {
  static CatItem it[6], cw[4];
  static CatFileira fs[2];
  const Fileira *lista;
  fil_definir_limite(FIL_LIMITE_MAX);
  ajustes_aplicar_blob("{\"continueWatchingEnabled\":true}");
  item(&it[0], "tt100", "movie", 0, 0, 40);
  item(&it[1], "tt200:2:3", "series", 2, 3, 30);
  item(&it[2], "tt300", "movie", 0, 0, 20);
  item(&it[3], "tt400", "movie", 0, 0, 0);
  item(&it[4], "tt200", "series", 0, 0, 0);    // a mesma serie numa fileira de catalogo
  snprintf(fs[0].chave, sizeof fs[0].chave, "continue_watching");
  snprintf(fs[0].titulo, sizeof fs[0].titulo, "Continuar assistindo");
  fs[0].ini = 0; fs[0].n = 3;
  snprintf(fs[1].chave, sizeof fs[1].chave, "lista");
  snprintf(fs[1].titulo, sizeof fs[1].titulo, "Lista");
  snprintf(fs[1].base, sizeof fs[1].base, "https://addon.invalid");
  snprintf(fs[1].catId, sizeof fs[1].catId, "top");
  fs[1].ini = 3; fs[1].n = 2;
  cat_definir_tudo(it, 5, fs, 2);
  quadro(); quadro();
  conferir("sem cartao", "tt100 tt200:2:3 tt300");

  // Saiu do player no MEIO de T2E4 da serie tt200: o cartao da ilha (relogio
  // ligado) segura a obra pelo id que o player tinha — a copia da fileira de
  // catalogo, sem episodio no id. A escolha e a mesma de app.c.
  assert(cw_retido_definir(cw_retido_escolher(1, "tt200", "tt999")));
  quadro();
  conferir("cartao da ilha segura tt200", "tt100 tt300");
  lista = naHome("lista");
  assert(lista && lista->n == 2 && !strcmp(card(lista, 1), "tt200"));
  puts("ok  a fileira de catalogo continua com a serie (so o Continuar a esconde)");

  // A REFACAO que fecharSessao pede publica a serie na FRENTE, com o
  // episodio novo. Continua fora enquanto o cartao a segura.
  item(&cw[0], "tt200:2:4", "series", 2, 4, 5);
  cw[1] = it[0]; cw[2] = it[2];
  cat_trocar_continuar(cw, 3);
  quadro();
  conferir("refacao com o cartao ainda na ilha", "tt100 tt300");
  // Um segundo quadro sem mudanca nao a traz de volta (guarda curto).
  quadro();
  conferir("quadro seguinte", "tt100 tt300");

  // O CARTAO SOLTA (dispensado / 30 min sem tecla): volta, e na frente.
  assert(cw_retido_definir(cw_retido_escolher(1, "", "")));
  quadro();
  conferir("cartao saiu da ilha", "tt200:2:4 tt100 tt300");

  // Sem relogio, quem segura e a faixa "Retomar agora" (home_registrar_retorno).
  home_registrar_retorno(1, 600.0, 3000.0);   // tt100 no indice 1 agora
  assert(!strcmp(home_retomar_imdb(), "tt100"));
  assert(cw_retido_definir(cw_retido_escolher(0, "tt200", home_retomar_imdb())));
  quadro();
  conferir("faixa Retomar agora segura tt100", "tt200:2:4 tt300");

  // Uma refacao que poe tt100 no FIM enquanto ele esta retido; ao soltar, app.c
  // o poe na frente (desc_continuar_otimista) — aqui pela mesma composicao.
  cw[0] = *cat_item(0); cw[1] = *cat_item(2); cw[2] = *cat_item(1);
  cat_trocar_continuar(cw, 3);
  quadro();
  conferir("refacao com a faixa segurando", "tt200:2:4 tt300");
  assert(cw_retido_definir(""));
  { CatItem novo = *cat_item(cat_indice_por_imdb("tt100")); naFrente(&novo); }
  quadro();
  conferir("faixa soltou: tt100 na frente", "tt100 tt200:2:4 tt300");
  // DISPENSAR o cartao "Retomar agora" (menu do cartao, home_retomar_dispensar).
  // Mesma sincronia de app.c (cwRetidoSincronizar): ao soltar, o titulo vai
  // para a FRENTE de Continuar assistindo.
  assert(ajustes_shot_valor("relogioTelaLocal", 1) && !ajustes_relogio_ligado());
  home_registrar_retorno(cat_indice_por_imdb("tt300"), 500.0, 3000.0);
  assert(cw_retido_definir(cw_retido_escolher(0, "", home_retomar_imdb())));
  quadro();
  conferir("retomar segura tt300", "tt100 tt200:2:4");
  assert(naHome("last_session") && naHome("last_session")->tipo == FILEIRA_RETORNO);
  home_retomar_dispensar();
  { char antes[64]; snprintf(antes, sizeof antes, "%s", cw_retido());
    assert(cw_retido_definir(cw_retido_escolher(0, "", home_retomar_imdb())));
    assert(antes[0] && !cw_retido_exclui(antes));
    { CatItem novo = *cat_item(cat_indice_por_imdb(antes)); naFrente(&novo); } }
  quadro();
  conferir("dispensado: tt300 na frente", "tt300 tt100 tt200:2:4");
  assert(!naHome("last_session"));
  puts("ok  o cartao dispensado sai e o titulo entra na frente");
  // "Tambem em Continuar assistindo" (2.0.2): ligado, a retencao nao exclui nada.
  assert(!ajustes_cw_retido_tambem());
  assert(cw_retido_definir("tt300") && cw_retido_exclui("tt300"));
  { unsigned r0 = cw_retido_rev(); cw_retido_tambem_definir(1);
    assert(cw_retido_rev() != r0 && !cw_retido_exclui("tt300")); cw_retido_tambem_definir(1);
    cw_retido_tambem_definir(0); assert(cw_retido_exclui("tt300")); }
  assert(cw_retido_definir(cw_retido_escolher(0, "", home_retomar_imdb())));
  puts("ok  com a opcao ligada o titulo retido nao sai da fileira");
  // Republicacao (rede/catalogo): o cartao NAO volta.
  cw[0] = *cat_item(cat_indice_por_imdb("tt300")); cw[1] = *cat_item(0); cw[2] = *cat_item(1);
  cat_trocar_continuar(cw, 3);
  quadro(); quadro();
  assert(!naHome("last_session") && !home_retomar_imdb()[0]);
  puts("ok  republicacao nao traz o cartao de volta");
  // Uma NOVA saida do player poe o cartao de novo (e o segura).
  home_registrar_retorno(cat_indice_por_imdb("tt100"), 700.0, 3000.0);
  assert(cw_retido_definir(cw_retido_escolher(0, "", home_retomar_imdb())));
  quadro();
  assert(naHome("last_session") && !strcmp(home_retomar_imdb(), "tt100"));
  puts("ok  nova saida mostra o cartao de novo");
  // Troca de conta/perfil: esquece e solta o hold sem pedir a frente.
  home_retomar_esquecer();
  assert(!cw_retido()[0] && !home_retomar_imdb()[0]);
  quadro();
  assert(!naHome("last_session"));
  puts("ok  troca de conta/perfil esquece o cartao");
  puts("cwretido_home: tudo ok");
  return 0;
}

// Sem textura carregada nesta fixture de navegacao: usa proporcao padrao.
float tex_aspecto(const char *caminho) { (void)caminho; return 0.0f; }
