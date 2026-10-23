// Apple TV sidebar (layout Dinamica): owner 14/10 — "a font da sidebar tem que
// ser mais pesada e o titulo streaming pode ser mais marcante". Checks the
// item labels use a heavier style than the old TXT_DET_META, the "Streaming"
// header is a bigger, bolder, tracked section title with room above it, the
// panel miolo is floored so the Home row titles do not read through, and the
// island clock is hidden while the sidebar is open (single clock).
#include "../src/menu.c"
#include <assert.h>
#include <stdio.h>

// --- captures ---------------------------------------------------------------
static TxtEstilo capItem, capCabecalho;
static int capCabecalhoVisto, capItemVisto, viuRotuloTv;
static float capMiolo, capMioloA;
static float opacidadeVidro = 0.78f;   // "78% e o vidro de sempre"
static int layoutTeste = HOME_LAYOUT_DINAMICA;
float gfx_tex_aspect_atual;
float gfx_card_forcar_cover_atual;

// --- ajustes / layout -------------------------------------------------------
int ajustes_home_layout(void) { return layoutTeste; }
int ajustes_menu_explorar(void) { return 1; }
int ajustes_menu_guia(void) { return 1; }
int ajustes_menu_agenda(void) { return 1; }
int ajustes_menu_perfil(void) { return 1; }
int ajustes_rail_recolhida(void) { return 0; }
int ajustes_menu_topo(void) { return 0; }
int ajustes_vidro(void) { return 1; }
int ajustes_animacoes_reduzidas(void) { return 0; }
float ajustes_vidro_opacidade(void) { return opacidadeVidro; }
float gfx_vidro_opacidade(void) { return opacidadeVidro / 0.78f; }
int gfx_efeitos_minimos(void) { return 0; }
int ajustes_relogio_12h(void) { return 0; }
void ajustes_acento(float *r, float *g, float *b) { *r = 1; *g = 1; *b = 1; }
float gfx_escala_ui(void) { return 1.0f; }
float ilha_relogio_largura(void) { return 100.0f; }

// --- pastas de Streaming (duas, para o cabecalho existir) -------------------
static const int pastasFake[] = { 7, 8 };
static ColFolder pastaFake = { .title = "Netflix" };
int home_streaming_barra(const int **v) { *v = pastasFake; return 2; }
const ColFolder *col_folder(int id) { (void)id; return &pastaFake; }
const char *col_capa(const ColFolder *pf) { (void)pf; return NULL; }
void col_cor(const ColFolder *pf, float *r, float *g, float *b) { (void)pf; *r = *g = 0.3f; *b = 0.34f; }

// --- perfil / relogio -------------------------------------------------------
static const ContaPerfil conta = { .nome = "Henrique", .corHex = "#ff8800" };

const ContaPerfil *perfis_item_ativo(void) { return &conta; }
// --- texto: registra os estilos pedidos -------------------------------------
TxtLinha txt_linha(TxtEstilo s, const char *t, int r, int g, int b, int a) {
  (void)r; (void)g; (void)b; (void)a;
  return (TxtLinha){ 1, (int)strlen(t) * 12, 24, 0, 0 };
}
TxtLinha txt_linha_corta(TxtEstilo s, const char *t, int r, int g, int b, int a, float max) {
  TxtLinha l = txt_linha(s, t, r, g, b, a);
  (void)max;
  // O ROTULO DO ITEM: registra o estilo que a barra pediu.
  capItem = s; capItemVisto++;
  if (s == TXT_TV_ROTULO) viuRotuloTv = 1;
  return l;
}
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) { (void)x; (void)y; (void)a; (void)l; }
int txt_largura(TxtEstilo s, const char *t) { (void)s; return (int)strlen(t) * 12; }
float txt_tracking(TxtEstilo s, const char *t, int r, int g, int b,
                   float x, float y, float a, float trk) {
  (void)r; (void)g; (void)b; (void)x; (void)y; (void)a; (void)trk;
  capCabecalho = s; capCabecalhoVisto++;
  return (float)strlen(t) * 12;
}

// --- gfx: no-op, menos o miolo do painel (o alfa e o que importa) -----------
void gfx_vidro_miolo(GfxRect r, float raio, float cr, float cg, float cb, float ca, float a) {
  (void)r; (void)raio; (void)cr; (void)cg; (void)cb;
  capMiolo = ca; capMioloA = a;
}
void gfx_vidro_fosco(GfxRect r, float raio, float a) { (void)r; (void)raio; (void)a; }
void gfx_vidro_material(GfxRect r, float raio, float dens, float a) {
  (void)r; (void)raio;
  capMiolo = dens * gfx_vidro_opacidade(); capMioloA = a;
}
void gfx_vidro_matiz(GfxRect r, float raio, float a) { (void)r; (void)raio; (void)a; }
void gfx_vidro(GfxRect r, float raio, int tipo, float cinza, float dens, float a) {
  (void)r; (void)raio; (void)tipo; (void)cinza; (void)dens; (void)a;
}
void gfx_brilho_topo(GfxRect r, float raio, float al, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)al; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float a) {
  (void)r; (void)raio; (void)cr; (void)cg; (void)cb; (void)a;
}
void gfx_recorte(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }
void gfx_sem_recorte(void) {}
float gfx_escala(void) { return 1.0f; }
void gfx_escala_sair(float e) { (void)e; }
GLuint tex_obter_larg(const char *u, float w) { (void)u; (void)w; return 0; }
GLuint tex_obter(const char *u) { (void)u; return 0; }
float tex_aspecto(const char *u) { (void)u; return 0.0f; }
void gfx_rect(GfxRect r, GLuint t, GfxModo modo, float foco, float parx, float pary, float raio,
              float cr, float cg, float cb, float ca) {
  (void)r; (void)t; (void)modo; (void)foco; (void)parx; (void)pary; (void)raio; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_icone(GfxRect r, const char *n, float cr, float cg, float cb, float a) {
  (void)r; (void)n; (void)cr; (void)cg; (void)cb; (void)a;
}
void gfx_sombra_sob(GfxRect s, float a, float b, float raio, float r, float g, float bl, float al, GfxRect r2, float raio2, float a2) {
  (void)s; (void)a; (void)b; (void)raio; (void)r; (void)g; (void)bl; (void)al; (void)r2; (void)raio2; (void)a2;
}
// assinaturas sem cabecalho (menu.c nao chama no caminho do teste, mas declara)
void gfx_luz_canto(GfxRect r, float raio, float cx, float cy, float alc, float cr, float cg, float cb, float a) {
  (void)r; (void)raio; (void)cx; (void)cy; (void)alc; (void)cr; (void)cg; (void)cb; (void)a;
}
void gfx_vidro_borda(GfxRect r, float raio, float a) { (void)r; (void)raio; (void)a; }
void gfx_vidro_folha(GfxRect r, float raio, float a) { (void)r; (void)raio; (void)a; }
int ponteiro_ativo(void) { return 0; }
void ponteiro_alvo(float x, float y, float w, float h, PonteiroFn cb, PonteiroFn cb2, int i, int j) {
  (void)x; (void)y; (void)w; (void)h; (void)cb; (void)cb2; (void)i; (void)j;
}
int segurar_venceu(Uint32 agora, Uint32 desde, const char *tag) { (void)agora; (void)desde; (void)tag; return 0; }
int ponteiro_ok_longo(void) { return 0; }
void ponteiro_borda_ativar(void) {}
int ponteiro_borda_registrada(void) { return 0; }
const char *i18n(const char *s) { return s; }
int layout_modo_mobile(void) { return 0; }

// O ROTULO DO ITEM vem pelo txt_linha_corta acima (capItem/capItemVisto).

// --- medidas reais dos estilos (text.c, extraido pelo .sh) ------------------
void txt_estilo_medidas(TxtEstilo estilo, int *corpo, int *peso);
#include "sidebaratv_fonte_impl.h"

int main(void) {
  int corpoRot, pesoRot, corpoMeta, pesoMeta, corpoCab, pesoCab, corpoCap, pesoCap;
  float ys[NV_MENU_FOCOS_TV], yRot;

  // 1) FONT MAIS PESADA: o rotulo do item e um estilo proprio, um peso acima
  //    do TXT_DET_META (Regular) de antes, no mesmo corpo.
  txt_estilo_medidas(TXT_TV_ROTULO, &corpoRot, &pesoRot);
  txt_estilo_medidas(TXT_DET_META, &corpoMeta, &pesoMeta);
  assert(pesoRot > pesoMeta && corpoRot == corpoMeta);

  // 2) "STREAMING" MAIS MARCANTE: maior que o TXT_CAPTION de antes, mais
  //    pesado que ele e mais pesado que o rotulo do item (hierarquia).
  txt_estilo_medidas(TXT_TV_SECAO, &corpoCab, &pesoCab);
  txt_estilo_medidas(TXT_CAPTION, &corpoCap, &pesoCap);
  assert(corpoCab > corpoCap && pesoCab > pesoCap && pesoCab > pesoRot);
  // E o vao acima do cabecalho cresceu (separacao de Ajustes).
  assert(TV_ROTULO_H >= 76.0f);
  assert(TV_SECAO_TRK > 0.0f);

  // 3) MIOLO DO PAINEL: com o "Opacidade do vidro" no padrao (0,78), o alfa
  //    e o material a TV_MIOLO_MIN — nada de titulo da Home atravessando.
  assert(TV_MIOLO_MIN >= 0.86f);
  menu_iniciar();
  menu_pilula_mostrar(1.0f);
  menu_abrir();
  for (int i = 0; i < 200; i++) menu_atualizar(0.016f, 0);
  assert(menu_aberto());
  opacidadeVidro = 0.78f; capMiolo = -1.0f;
  menu_desenhar(0);
  assert(capMiolo >= TV_MIOLO_MIN * 0.999f);
  // Ajuste acima do piso respeita o usuario (o piso e um MINIMO).
  opacidadeVidro = 1.0f; capMiolo = -1.0f;
  menu_desenhar(0);
  assert(capMiolo > TV_MIOLO_MIN);

  // 4) O CABECALHO e o rotulo passaram pelos estilos novos no desenho.
  assert(capCabecalhoVisto > 0 && capCabecalho == TXT_TV_SECAO);
  assert(capItemVisto > 0 && viuRotuloTv);

  // 5) RELOGIO UNICO: a barra aberta esconde a ilha do canto (o cabecalho do
  //    painel ja tem o relogio); fechada, a ilha volta.
  assert(menu_relogio_cabe() == 0);
  menu_fechar();
  for (int i = 0; i < 200; i++) menu_atualizar(0.016f, 0);
  assert(menu_relogio_cabe() == 1);
  // Na rail classica a ilha NAO some com o menu aberto (coluna de duas ilhas).
  layoutTeste = HOME_LAYOUT_PADRAO;
  menu_abrir();
  for (int i = 0; i < 200; i++) menu_atualizar(0.016f, 0);
  assert(menu_aberto() && menu_relogio_cabe() == 1);

  // 6) O cabecalho nao invade a primeira pasta: o layout reserva TV_ROTULO_H.
  { float antes = tvLayout(ys, &yRot);
    assert(yRot > 0.0f && ys[MENU_ST0] == yRot + TV_ROTULO_H);
    (void)antes; }

  puts("PASS: sidebar Apple TV — rotulo mais pesado, cabecalho marcante, miolo no piso, relogio unico");
  return 0;
}
