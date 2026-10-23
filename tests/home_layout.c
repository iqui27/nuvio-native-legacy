// Sem janela, rede ou TV: valida composição editorial e foco sobre dados reais
// de catálogo (fixtures), usando a implementação da Home e do catálogo.
#include <assert.h>
#include <string.h>
// Antes do home.c: os dois acessores viram os do teste (ver homeCom).
#define ajustes_posteres_deitados teste_posteres_deitados
static int testeDeitados;
static int teste_posteres_deitados(void) { return testeDeitados; }
#define ajustes_home_layout teste_home_layout
#define ajustes_hero_ligado teste_hero_ligado
#define ajustes_espaco_titulos teste_espaco_titulos
#define ajustes_hero_genero teste_hero_genero
#define txt_desenhar teste_txt_desenhar
static int testeGenero = 1, nTxtDesenhado = 0, nBadge = 0, testeImdb = 0;
static int teste_hero_genero(void) { return testeGenero; }
#define ajustes_espaco_fileiras teste_espaco_fileiras
#define ajustes_ocultar_filmes_vistos teste_ocultar_vistos
static int ocultarVistos;
static int teste_ocultar_vistos(void) { return ocultarVistos; }
static float espacoTit = 1.0f, espacoFil = 1.0f;
static float teste_espaco_titulos(void) { return espacoTit; }
static float teste_espaco_fileiras(void) { return espacoFil; }
// Fundo da Dinamica (#205): o modo e a cor fixa vem das variaveis abaixo.
#define ajustes_fundo_din teste_fundo_din
#define ajustes_fundo_din_cor teste_fundo_din_cor
static int testeFundoDin = 0;
static float testeFundoCor[3] = { 0.07f, 0.46f, 0.81f };
static int teste_fundo_din(void) { return testeFundoDin; }
static void teste_fundo_din_cor(float rgb[3]) { memcpy(rgb, testeFundoCor, sizeof(float) * 3); }
#define NV_TESTE_HOME 1
#include "../src/home.c"
void desc_sinopse_hero(const int *idx, int n) { (void)idx; (void)n; }
void desc_ocultar_vistos_atualizar(void) {}

void cachearte_marcar_grupo(int grupo, const char *url, int variante, int essencial, int emUso) {
  (void)grupo; (void)url; (void)variante; (void)essencial; (void)emUso;
}
void cachearte_limpar_referencias_grupo(int grupo) { (void)grupo; }
void cachearte_estatisticas_pedir(void) {}
void tex_cache_marcar_larg(int grupo, const char *url, float larg, int essencial, int emUso) {
  (void)grupo; (void)url; (void)larg; (void)essencial; (void)emUso;
}
int tex_falhou(const char *url) { (void)url; return 0; }
int tex_largura_fonte(const char *url) { (void)url; return 0; }
float tex_aspecto(const char *c) { (void)c; return 0.0f; }
Uint32 SDL_GetTicks(void) { return 0; }

// catalogo.c agora le o progresso de progresso.c, que fala com dados.c e
// perfis.c. Aqui nao ha disco nem conta: dublês vazios bastam.
char *dados_ler(const char *nome) { (void)nome; return NULL; }
int   dados_gravar(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
// NULL de proposito: fileiras.c sai cedo em carregar() e a lista nasce vazia,
// que e o estado de quem nunca abriu o app. O teste de layout nao tem opiniao
// sobre escolha local de fileiras.
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  (void)dst; (void)tam; (void)nome; return NULL;
}
int   dados_apagar(const char *nome) { (void)nome; return 1; }
void  dados_marcar_sujo(int leve) { (void)leve; }
void  sync_proteger_ajustes_locais(void) {}
int   perfis_ativo(void) { return 1; }
int   contalib_tem_conta(void) { return 0; }
int   trakt_ativo(void) { return 0; }
int   simkl_ativo(void) { return 0; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
const char *addons_nome_por_id(const char *id) { (void)id; return ""; }
// fileiras.c declara estas como NV_PERFIL_OPCIONAL (weak_import); sem
// definicao, o ld do Mach-O falha no link do fixture. Zeradas, o teste fica
// no caminho "sem conta vinculada" que ja era o dele.
unsigned addons_versao(void) { return 0; }
int     addons_perfil_da_lista(void) { return 0; }
int     addons_travar_lista(unsigned v, int p) { (void)v; (void)p; return 0; }
void    addons_destravar_lista(void) {}

// LAYOUT E DESTAQUE DO TESTE. home.c le os dois por estes acessores; trocados
// aqui por variaveis, sem arrastar o disco e os idiomas de ajustes.c para o
// link. O padrao e o da TV que nunca mexeu: Moderna, destaque ligado.
static int testeLayout = HOME_LAYOUT_MODERNA, testeHero = 1;
int teste_home_layout(void) { return testeLayout; }
int teste_hero_ligado(void) { return testeHero; }
void teste_txt_desenhar(TxtLinha l, float x, float y) { (void)l; (void)x; (void)y; nTxtDesenhado++; }
// Escala de texto e componentes (gfx.c): a mesma conta, sem GL. testeTxt e o
// fator da camada; o padrao 1 deixa tudo como era.
static float testeTxt = 1.0f, testeAtiva = 1.0f;
float gfx_escala(void) { return testeAtiva; }
float gfx_escala_txt(void) { return testeTxt; }
float gfx_escala_tela_txt(void) { return (testeTxt != 1.0f && testeAtiva == testeTxt) ? testeTxt : 1.0f; }
float gfx_poster_k(void) { return testeAtiva != 1.0f ? 1.0f / testeTxt : 1.0f; }
float gfx_escala_txt_entrar(void) { float a = testeAtiva; testeAtiva = testeTxt; return a; }
void gfx_escala_sair(float a) { testeAtiva = a; }
// Duble do desenho do card editorial: sem GL, so o que o teste do genero conta.
float gfx_tex_aspect_atual;
void gfx_recorte(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }
void gfx_sem_recorte(void) {}
void gfx_veu_base(GfxRect c, float r, float f, float a) { (void)c; (void)r; (void)f; (void)a; }
static int nSinopse;
static int nHolofote, stageTitleDraws, stageTextDraws;
static GfxRect stageTitleRect, stageGenreRect;
static float stageBadgeWidth;
void gfx_rect(GfxRect r, GLuint t, GfxModo m, float f, float a, float b, float c, float d, float e, float g, float h) {
  if (m == GFX_HOLOFOTE || m == GFX_PALCO_CHAO) nHolofote++;
  if (m == GFX_COR && d > 0.9f && e > 0.7f && g < 0.2f) stageBadgeWidth = r.w;
  (void)r; (void)t; (void)m; (void)f; (void)a; (void)b; (void)c; (void)d; (void)e; (void)g; (void)h; }
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float a) {
  gfx_rect(r, 0, GFX_COR, 0, 0, 0, raio, cr, cg, cb, a);
}
GLuint tex_obter_logo_larg(const char *c, float l) { (void)c; (void)l; return 0; }
GLuint tex_obter_logo_larg_qualquer(const char *c, float l) { (void)c; (void)l; return 0; }
void tex_publicar_paleta(const char *c) { (void)c; }
const char *logotitulo_url(const CatItem *ci, float w) { (void)ci; (void)w; return NULL; }
int  tex_marca_escura(const char *c) { (void)c; return 0; }
TxtLinha txt_linha_corta(TxtEstilo e, const char *s, int r, int g, int b, int a, float w) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)a; (void)w; return (TxtLinha){ 1, 10, 10, 0, 0 }; }
float badge_desenhar(float x, float y, const char *t, BadgeEstilo e, float a) { nBadge++; (void)x; (void)y; (void)t; (void)e; (void)a; return 0; }
// Exercise the real IMDb measurement/drawing code in this scene.
#define badge_desenhar stage_real_badge_desenhar
#define badge_imdb stage_real_badge_imdb
#include "../src/badges.c"
#undef badge_desenhar
#undef badge_imdb
float badge_imdb(float x, float y, int n, int e, float a) {
  nBadge++; return stage_real_badge_imdb(x, y, n, e, a);
}
TxtLinha txt_linha(TxtEstilo e, const char *s, int r, int g, int b, int a) {
  (void)e; (void)r; (void)g; (void)b; (void)a;
  return (TxtLinha){1, (int)strlen(s) * 16, 24, 0, 0};
}
float txt_bloco_corta(TxtEstilo e, const char *s, int r, int g, int b,
                       float x, float y, float w, float leading, float a, int max) {
  (void)s; (void)r; (void)g; (void)b;
  if (e == TXT_HERO_SIN) nSinopse++;
  if (e == TXT_TITULO3 && a > 0) {
    stageTitleDraws++;
    stageTitleRect = (GfxRect){x, y, w, leading * max};
  }
  return leading * max;
}
float sinopse_bloco_corta(TxtEstilo e, const char *s, int r, int g, int b,
                          float x, float y, float w, float leading, float a, int max) {
  assert(max <= 3);
  return txt_bloco_corta(e, s, r, g, b, x, y, w, leading, a, max);
}
int  imdbnota_obter(const char *i, int c, int s) { (void)i; (void)c; (void)s; return testeImdb; }
const char *i18n(const char *s) { return s; }
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) {
  if (a > 0 && stageTextDraws++ == 0) stageGenreRect = (GfxRect){x, y, l.w, l.h};
}
float sinopse_bloco(TxtEstilo o, const char *t, int r, int g, int b, float x, float y, float l, float d, float a, int m) {
  (void)o; (void)t; (void)r; (void)g; (void)b; (void)x; (void)y; (void)l; (void)d; (void)a; (void)m; return 90.0f; }
static void homeCom(int layout, int heroLigado) { testeLayout = layout; testeHero = heroLigado; }

int main(void) {
  // HEROI DIVIDIDO: painel deitado a esquerda (~45% da largura util), a fila de
  // cartazes a direita dele, e o painel segue o foco da fileira.
  {
    int cols[] = {12, 12};
    fileiras[0] = (Fileira){.tipo = FILEIRA_NORMAL, .n = 12, .escala = 1.0f};
    fileiras[1] = (Fileira){.tipo = FILEIRA_HEROI_DIVIDIDO, .n = 12, .ini = 40, .escala = 1.0f};
    focus_iniciar(&foco, 2, cols);
    assert(focus_mover(&foco, 0, 1) && foco.fileira == 1);
    GfxRect p = divididoPainel(1, 100.0f);
    assert(fabsf(p.w - homeLarguraUtil() * 0.45f) < 0.5f);
    assert(p.x == homeConteudoX() && p.y == 100.0f && p.h > 0.0f && p.h <= p.w * 9.0f / 16.0f + 0.5f);
    // A fila comeca a direita do painel, e o PROXIMO titulo e o primeiro dela:
    // o card em foco e o proprio painel, nunca repetido na fila.
    for (int c = 0; c < 6; c++) {
      if (c) assert(focus_mover(&foco, 1, 0));
      scrollX[1] = alvoScrollFil(1, c, scrollX[1], 0.0f);
      float x = homeFileiraX(1, c + 1) + xOffTipo(FILEIRA_HEROI_DIVIDIDO);
      assert(x >= p.x + p.w + 20.0f && x < p.x + p.w + 80.0f);
      assert(divididoItem(1) == 40 + c);   // o painel acompanha o foco
    }
    // O passo e o de um cartaz, nao o do painel; a altura reserva a ficha.
    assert(passoFil(1) < larguraFil(1) + 60.0f);
    assert(alturaTotalFil(1) >= alturaFil(1) + 150.0f);
    // Ano/nota embaixo do poster: o rotulo existe e a altura dele esta reservada.
    assert(temRotulo(FILEIRA_HEROI_DIVIDIDO));
    assert(alturaTotalFil(1) == alturaFil(1) + NV_DIV_META_H);
    memset(scrollX, 0, sizeof scrollX);
  }
  // HEROI DIVIDIDO sem repeticao: com 5 itens, em CADA foco o titulo do painel
  // nao aparece na fila, e a fila e exatamente os itens foco+1.. (inclusive nos
  // ultimos, onde a fila esvazia).
  {
    int cols[] = {5};
    fileiras[0] = (Fileira){.tipo = FILEIRA_HEROI_DIVIDIDO, .n = 5, .ini = 40, .escala = 1.0f};
    focus_iniciar(&foco, 1, cols);
    memset(scrollX, 0, sizeof scrollX);
    for (int f = 0; f < 5; f++) {
      int visiveis[5], nv = 0, hero;
      if (f) assert(focus_mover(&foco, 1, 0));
      scrollX[0] = alvoScrollFil(0, f, scrollX[0], 0.0f);
      hero = divididoItem(0);
      assert(hero == 40 + f);
      for (int c = 0; c < 5; c++) {
        float cx = homeFileiraX(0, c) + xOffTipo(FILEIRA_HEROI_DIVIDIDO) + larguraFil(0) * 0.5f;
        if (divididoCartazVisivel(0, cx, larguraFil(0))) visiveis[nv++] = 40 + c;
      }
      for (int k = 0; k < nv; k++) assert(visiveis[k] != hero);
      assert(nv == 4 - f);
      for (int k = 0; k < nv; k++) assert(visiveis[k] == 40 + f + 1 + k);
    }
    memset(scrollX, 0, sizeof scrollX);
  }
  // Palco percorre as mesmas vagas e usa a mesma borda das fileiras comuns.
  // Igualar a largura isola a regra de ancoragem do tamanho de cada forma.
  {
    int cols[] = {12, 12};
    fileiras[0] = (Fileira){.tipo = FILEIRA_NORMAL, .n = 12, .escala = 1.0f};
    fileiras[1] = (Fileira){.tipo = FILEIRA_PALCO, .n = 12, .escala = 1.0f};
    float lw = larguraFil(1), anterior = 0.0f, altura = alturaTotalFil(1), passo = passoFil(1);
    focus_iniciar(&foco, 2, cols);
    for (int c = 0; c < 12; c++) {
      if (c) assert(focus_mover(&foco, 1, 0));
      palcoPos[1] = (float)foco.coluna; // animacao em repouso
      scrollX[1] = alvoScrollFil(1, c, scrollX[1], 0.0f);
      float cx, s, vizX, vizS;
      palcoGeo(1, c, lw, &cx, &s);
      assert(fabsf(cx - (homeFileiraX(1, c) + lw * 0.5f)) < 0.01f);
      assert(s == 1.0f);
      // A altura reservada nao muda com o foco: as outras fileiras nao andam.
      assert(alturaTotalFil(1) == altura);
      // As vagas ficam paradas: so o foco e os dois vizinhos se movem quando
      // o foco anda uma coluna; o resto da fileira nao se mexe (dono, 14/10).
      if (c) {
        float antes[12];
        palcoPos[1] = (float)(c - 1);
        for (int k = 0; k < 12; k++) { float sk; palcoGeo(1, k, lw, &antes[k], &sk); }
        palcoPos[1] = (float)c;
        for (int k = 0; k < 12; k++) {
          float xk, sk;
          palcoGeo(1, k, lw, &xk, &sk);
          if (k < c - 1 || k > c) assert(fabsf(xk - antes[k]) < 0.01f);
        }
      }
      if (c == 1) assert(cx > anterior && scrollX[1] == 0.0f);
      if (c == 11) assert(scrollX[1] > 0.0f);
      palcoGeo(1, c + 1, lw, &vizX, &vizS);
      assert(fabsf(vizS - NV_PALCO_VIZ) < 0.01f);
      assert(vizX - lw * vizS * 0.5f > cx + lw * s * 0.5f);
      // #205: os vizinhos encolhem E se chegam. O passo entre vizinhos segue
      // a largura escalada — o gap e o da fileira comum (±2 px) — e o foco so
      // empurra o PRIMEIRO vizinho pelo que cresceu.
      { float vizX2, vizS2, gap = gapDe(FILEIRA_PALCO);
        palcoGeo(1, c + 2, lw, &vizX2, &vizS2);
        assert(fabsf((vizX2 - vizX) - (lw * vizS + gap)) < 2.0f);
        assert(fabsf((vizX - cx) - (lw * (1.0f + NV_PALCO_VIZ) * 0.5f + gap
                                    + NV_PALCO_INFO_VAO + NV_PALCO_INFO_W)) < 2.0f); }
      anterior = cx;
    }
    focoHero = 1;
    memset(scrollX, 0, sizeof scrollX);
    // REVISAO #205: o alcance do toque na Palco e a extensao COMPRIMIDA do
    // palcoGeo (o passo de vagas sobrava um vao morto no fim da rolagem); a
    // coluna retomada inverte o palcoGeo (a conta de vagas caia num card ao
    // lado do que a pessoa tinha centrado).
    {
      fileiras[1] = (Fileira){.tipo = FILEIRA_PALCO, .n = 12, .escala = 1.0f};
      float lw = larguraFil(1), passo = passoFil(1);
      float cx, s;
      scrollX[1] = 0.0f;
      // Foco na primeira coluna: o fim da fileira e a borda do ULTIMO card
      // comprimido, bem atras da vaga dele — e o vao morto some do maxX.
      palcoPos[1] = 0.0f;
      palcoGeo(1, 11, lw, &cx, &s);
      assert(fabsf(palcoExtent(1, 12) - (cx + lw * s * 0.5f - homeFileiraX(1, 0))) < 0.01f);
      assert(palcoExtent(1, 12) < (12 - 1) * (lw + gapDe(FILEIRA_PALCO)) + lw - 100.0f);
      // Foco na ultima: o card cresce NA vaga, a extensao volta a ser a de
      // vagas — igual a fileira comum no fim da rolagem.
      palcoPos[1] = 11.0f;
      palcoGeo(1, 11, lw, &cx, &s);
      assert(fabsf(palcoExtent(1, 12) - (cx + lw * s * 0.5f - homeFileiraX(1, 0))) < 0.01f);
      assert(fabsf(palcoExtent(1, 12) - ((12 - 1) * passo + lw)) < 0.01f);
      // A coluna retomada e a do card na ancora — inteira e em plena mola.
      palcoPos[1] = 0.0f;
      for (int k = 0; k < 12; k++) {
        palcoGeo(1, k, lw, &cx, &s);
        assert(palcoColunaEm(1, cx - homeFileiraX(1, 0)) == k);
      }
      palcoPos[1] = 3.5f;
      for (int k = 2; k < 7; k++) {
        palcoGeo(1, k, lw, &cx, &s);
        assert(palcoColunaEm(1, cx - homeFileiraX(1, 0)) == k);
      }
      palcoPos[1] = 0.0f;
    }
    // PALCO, 17/10: a ficha a direita do foco cabe na tela em TODO foco, na
    // escala de texto 80/100/130%, e o vizinho da direita fica depois dela.
    {
      const float escalas[] = {0.8f, 1.0f, 1.3f};
      for (int e = 0; e < 3; e++) {
        int cols[] = {12, 12};
        testeTxt = testeAtiva = escalas[e];
        fileiras[0] = (Fileira){.tipo = FILEIRA_NORMAL, .n = 12, .escala = 1.0f};
        fileiras[1] = (Fileira){.tipo = FILEIRA_PALCO, .n = 12, .escala = 1.0f};
        focoHero = 0;
        focus_iniciar(&foco, 2, cols);
        assert(focus_mover(&foco, 0, 1) && foco.fileira == 1);
        memset(scrollX, 0, sizeof scrollX);
        float lw = larguraFil(1), artH = alturaFil(1);
        for (int c = 0; c < 12; c++) {
          if (c) assert(focus_mover(&foco, 1, 0));
          palcoPos[1] = (float)c;
          scrollX[1] = alvoScrollFil(1, c, scrollX[1], 0.0f);
          GfxRect info = palcoInfoRect(1, 100.0f, lw, artH);
          float cx, s, vx, vs;
          palcoGeo(1, c, lw, &cx, &s);
          assert(cx - lw * 0.5f >= 0.0f);
          assert(info.x >= cx + lw * 0.5f && info.x + info.w <= NV_TELA_W);
          assert(info.y == 100.0f && info.h == artH);
          palcoGeo(1, c + 1, lw, &vx, &vs);
          assert(c == 11 || vx - lw * vs * 0.5f > info.x + info.w);
        }
      }
      testeTxt = testeAtiva = 1.0f;
    }
    // HOLOFOTE: um quad so com o foco NA fileira Palco; nada fora dela.
    {
      int cols[] = {12, 12};
      float lw = larguraFil(1), artH = alturaFil(1);
      anim_politica_reduzida = 1;
      focoHero = 0;
      focus_iniciar(&foco, 2, cols);
      assert(focus_mover(&foco, 0, 1) && foco.fileira == 1);
      nHolofote = 0; palcoFicha(1, 100.0f, lw, artH); holofote(1000);
      assert(nHolofote == 1);
      assert(focus_mover(&foco, 0, -1) && foco.fileira == 0);
      nHolofote = 0; palcoFicha(1, 100.0f, lw, artH); holofote(1016);
      assert(nHolofote == 0);
      assert(focus_mover(&foco, 0, 1) && foco.fileira == 1);
      focoHero = 1;
      nHolofote = 0; palcoFicha(1, 100.0f, lw, artH); holofote(1032);
      assert(nHolofote == 0);
      anim_politica_reduzida = 0;
      focoHero = 1;
    }
  }
  // Stage B scene: inspect actual draw coordinates at every supported anchor.
  {
    CatItem item = {0};
    CatFileira row = {.ini = 0, .n = 1};
    snprintf(item.titulo, sizeof item.titulo, "Teenage Sex and Death at Camp Miasma");
    snprintf(item.genero, sizeof item.genero, "Movie · Comedy · Horror");
    snprintf(item.meta, sizeof item.meta, "2026 · 112 min");
    snprintf(item.sinopse, sizeof item.sinopse, "A director revives a faded slasher franchise.");
    testeImdb = 65;
    cat_definir_tudo(&item, 1, &row, 1);
    fileiras[0] = (Fileira){.tipo = FILEIRA_PALCO, .ini = 0, .n = 1, .escala = 1};
    foco.fileira = foco.coluna = focoHero = 0;
    palcoPos[0] = 0;
    const float scales[] = {.80f, .85f, 1, 1.15f};
    for (int i = 0; i < 4; i++) {
      testeTxt = testeAtiva = scales[i];
      float w = larguraFil(0), h = alturaFil(0);
      GfxRect p = palcoInfoRect(0, 100, w, h);
      stageTitleDraws = stageTextDraws = 0;
      palcoFicha(0, 100, w, h);
      assert(stageTitleDraws == 1);
      float cx, scale;
      palcoGeo(0, 0, w, &cx, &scale);
      assert(stageGenreRect.y == p.y && p.y == 100 + h * (1 - scale) * .5f);
      assert(fabsf(p.x - cx - w * scale * .5f - 36) < .01f);
      assert(stageTitleRect.x >= p.x && stageTitleRect.y >= p.y);
      assert(stageTitleRect.x + stageTitleRect.w <= p.x + p.w);
      assert(stageTitleRect.y + stageTitleRect.h <= p.y + p.h);
      stageBadgeWidth = 0;
      float used = badge_imdb(0, 0, 65, 0, 1);
      assert(stageBadgeWidth >= txt_linha(TXT_MINI, "IMDb", 0, 0, 0, 255).w + 16);
      assert(used == badge_imdb_largura(65));
    }
    testeTxt = testeAtiva = 1;
#ifndef NV_STAGE_BASELINE
    // Same elapsed time at 30/60 fps; repeat retargets without a position jump.
    fileiras[0].n = 12;
    for (int fps = 30; fps <= 60; fps += 30) {
      foco.coluna = 0;
      palcoUlt[0] = 0;
      scrollX[0] = 0;
      palcoPasso(0, 1000);
      assert(palcoMov[0].texto == 1);
      foco.coluna = 1;
      palcoPasso(0, 1100);
      for (int frame = 1; frame * 1000 / fps < 160; frame++)
        palcoPasso(0, 1100 + frame * 1000 / fps);
      palcoPasso(0, 1260);
      assert(palcoPos[0] > .5f && palcoPos[0] < .9f);
      assert(fabsf(palcoMov[0].texto - .4f) < .001f);
      palcoPasso(0, 1500);
      assert(palcoPos[0] == 1 && palcoMov[0].texto == 1);
      foco.coluna = 2;
      palcoPasso(0, 1600);
      palcoPasso(0, 1760);
      float position = palcoPos[0], scroll = scrollX[0];
      foco.coluna = 8;
      palcoPasso(0, 1760);
      assert(palcoPos[0] == position && scrollX[0] == scroll);
      palcoPasso(0, 2160);
      assert(palcoPos[0] == 8 && scrollX[0] == palcoMov[0].scrollAlvo);
      foco.coluna = 7;
      palcoPasso(0, 2200);
      palcoPasso(0, 2360);
      assert(8 - palcoPos[0] > .5f && 8 - palcoPos[0] < .9f);
      palcoPasso(0, 2600);
      assert(palcoPos[0] == 7);
      anim_politica_reduzida = 1;
      foco.coluna = 0;
      palcoPasso(0, 2700);
      assert(palcoPos[0] == 0 && palcoMov[0].texto == 1);
      anim_politica_reduzida = 0;
    }
#endif
    // Changing text scale must keep the last poster's panel and See all visible.
    fileiras[0].n = 4;
    fileiras[0].verTudo = 1;
    foco.fileira = focoHero = 0;
    foco.coluna = 3;
    palcoUlt[0] = 0;
    testeTxt = testeAtiva = .85f;
    scrollX[0] = alvoScrollFil(0, 3, 0, 0);
    palcoPasso(0, 3000);
    testeTxt = testeAtiva = 1.15f;
    palcoPasso(0, 4000);
    GfxRect panel = palcoInfoRect(0, 100, larguraFil(0), alturaFil(0));
    assert(panel.x + panel.w <= NV_TELA_W);
    assert(fabsf(scrollX[0] - alvoScrollFil(0, 3, scrollX[0], 0)) < .01f);
    testeTxt = testeAtiva = 1;
    testeImdb = 0;
  }
  // A composicao abaixo e a da home SEM o destaque no topo: e nela que a
  // primeira fileira de catalogo vira a vitrine (vitrineNaPrimeira). O #201
  // (com destaque) tem bloco proprio no fim.
  homeCom(HOME_LAYOUT_MODERNA, 0);
  // O TETO DE FILEIRAS NO MAXIMO, porque este teste e sobre COMPOSICAO e FOCO.
  //
  // O limite legado é 16 linhas visíveis. O fixture valida a composição dentro
  // desse teto, incluindo as linhas fixas.
  fil_definir_limite(FIL_LIMITE_MAX);
  assert(MAX_FIL <= FOCUS_MAX_FILEIRAS);
  assert(perfilCatalogo("Oscars 2026 - Filme") == FILEIRA_COLECAO);
  assert(perfilCatalogo("NETFLIX - Série") == FILEIRA_SERVICO);
  assert(perfilCatalogo("For You - Filme") == FILEIRA_NORMAL);
  assert(fabsf(larguraDe(FILEIRA_DESTAQUE) /
               alturaDe(FILEIRA_DESTAQUE) - 16.0f / 9.0f) < 0.01f);
  assert(alturaDe(FILEIRA_DESTAQUE) > alturaDe(FILEIRA_COLECAO));
  assert(fabsf(larguraDe(FILEIRA_DESTAQUE_QUADRADO) /
               alturaDe(FILEIRA_DESTAQUE_QUADRADO) - 4.0f / 3.0f) < 0.01f);
  assert(alturaDe(FILEIRA_DESTAQUE_QUADRADO) > alturaDe(FILEIRA_DESTAQUE));
  assert(larguraDe(FILEIRA_DESTAQUE_QUADRADO) * alturaDe(FILEIRA_DESTAQUE_QUADRADO) >
         larguraDe(FILEIRA_DESTAQUE) * alturaDe(FILEIRA_DESTAQUE));
  assert(tipoDaEscolha(FIL_TIPO_DESTAQUE) == FILEIRA_DESTAQUE);
  assert(tipoDaEscolha(FIL_TIPO_DESTAQUE_QUADRADO) == FILEIRA_DESTAQUE_QUADRADO);
  assert(!strcmp(fil_tipo_rotulo(FIL_TIPO_DESTAQUE_QUADRADO), "Destaque 4:3"));
  // Os 4:3 maiores sao a mesma forma; o tamanho vem do fator (fil_escala).
  assert(tipoDaEscolha(FIL_TIPO_DESTAQUE_QUADRADO_M) == FILEIRA_DESTAQUE_QUADRADO);
  assert(tipoDaEscolha(FIL_TIPO_DESTAQUE_QUADRADO_G) == FILEIRA_DESTAQUE_QUADRADO);
  assert(gapDe(FILEIRA_DESTAQUE) == NV_CARD_GAP_GRANDE);
  assert(gapDe(FILEIRA_DESTAQUE_QUADRADO) == NV_CARD_GAP_GRANDE);
  assert(gapDe(FILEIRA_NORMAL) == NV_CARD_GAP);
  // Espaco entre titulos/fileiras (Ajustes): 100 % e o de sempre; o resto escala
  // so o vao medido, e o vao dos cards grandes nunca desce do comum.
  { float f; int l;
    for (f = 0.5f; f <= 1.51f; f += 0.25f) {
      espacoTit = f; espacoFil = f;
      assert(fabsf(gapDe(FILEIRA_NORMAL) - (NV_CARD_GAP * f + xOffTipo(FILEIRA_NORMAL))) < 0.01f);
      assert(gapDe(FILEIRA_DESTAQUE) >= NV_CARD_GAP);
      assert(gapDe(FILEIRA_DESTAQUE_QUADRADO) >= NV_CARD_GAP);
      for (l = 0; l < 3; l++) {
        float base;
        espacoFil = 1.0f; homeCom(l, 0); base = fileiraGap();
        espacoFil = f; assert(fabsf(fileiraGap() - base * f) < 0.01f);
      }
    }
    espacoTit = 1.0f; espacoFil = 1.0f; homeCom(HOME_LAYOUT_MODERNA, 0); }
  // #205 cardexp (dono, 14/10): espacamento da referencia da Apple TV — o
  // titulo sai de cima dos cards e as fileiras respiram — e o card da ficha
  // menor, para nao transbordar o cartaz/fileira (NV_FICHA_W).
  assert(NV_LEGACY_ROW_HEAD_H >= 64.0f);
  assert(NV_FILEIRA_GAP >= 80.0f);
  assert(NV_FICHA_W <= 540.0f);
  // PÔSTER GRANDE P/M/G (dono: "esse estilo de fileira pode ter 3 tamanhos
  // tb"): a mesma forma em tres tamanhos, crescendo sem tirar o card da tela.
  // O M e o de sempre (fator 1, 3:4); no menor a altura cai, e o maior nunca
  // fica mais baixo que o M — na Moderna/Padrao o teto de altura vale para ele
  // tambem, entao la ele cresce na LARGURA (alargaPosterG); na Dinamica, nos dois.
  { static const int T3[3] = { FIL_TIPO_POSTER_GRANDE_P, FIL_TIPO_POSTER_GRANDE, FIL_TIPO_POSTER_GRANDE_G };
    static const int LAY[2] = { HOME_LAYOUT_MODERNA, HOME_LAYOUT_DINAMICA };
    for (int li = 0; li < 2; li++) {
      float w[3], h[3], wc[3];
      homeCom(LAY[li], 0);
      for (int k = 0; k < 3; k++) {
        assert(tipoDaEscolha(T3[k]) == FILEIRA_POSTER_GRANDE);
        fileiras[0] = (Fileira){.tipo = FILEIRA_POSTER_GRANDE, .n = 3,
                                .escala = fil_tipo_fator(T3[k]), .fator = fil_tipo_fator(T3[k])};
        w[k] = larguraFil(0); h[k] = alturaFil(0);
        fileiras[0] = (Fileira){.tipo = FILEIRA_CATALOGOS, .n = 3, .estiloCol = FILEIRA_POSTER_GRANDE,
                                .escala = fil_tipo_fator(T3[k]), .fator = fil_tipo_fator(T3[k])};
        wc[k] = larguraFil(0);
        assert(h[k] > 0.0f && h[k] <= NV_TELA_H - 64.0f);   // cabe na tela
      }
      assert(w[0] < w[1] && w[1] < w[2]);                    // largura cresce
      assert(h[0] < h[1] && h[1] <= h[2] + 0.01f);           // altura nunca encolhe
      assert(w[0] * h[0] < w[1] * h[1] && w[1] * h[1] < w[2] * h[2]);   // area cresce
      assert(wc[0] < wc[1] && wc[1] < wc[2]);                // a pasta (colecao) tambem
      if (LAY[li] == HOME_LAYOUT_DINAMICA) assert(h[1] < h[2]);          // sem teto: mais alto
      // O M e o de antes dos tres tamanhos: 3:4 cheio ou preso so pelo teto.
      fileiras[0] = (Fileira){.tipo = FILEIRA_POSTER_GRANDE, .n = 3, .escala = 1.0f, .fator = 1.0f};
      assert(fabsf(larguraFil(0) / alturaFil(0) - 0.75f) < 0.001f);
    }
    homeCom(HOME_LAYOUT_MODERNA, 0);
    memset(&fileiras[0], 0, sizeof fileiras[0]); }
  assert(larguraDe(FILEIRA_COLECAO) > larguraDe(FILEIRA_SERVICO));
  assert(!temRotulo(FILEIRA_DESTAQUE));
  assert(!temRotulo(FILEIRA_CATALOGOS));

  CatItem *itensTeste = calloc(48, sizeof *itensTeste);
  CatFileira fils[16] = {0};
  assert(itensTeste);
  for (int i = 0; i < 16; i++) {
    snprintf(fils[i].chave, sizeof fils[i].chave, "catalogo_%d", i);
    snprintf(fils[i].titulo, sizeof fils[i].titulo, "Lista %d", i);
    snprintf(fils[i].base, sizeof fils[i].base, "https://example.invalid/addon");
    snprintf(fils[i].tipo, sizeof fils[i].tipo, "movie");
    snprintf(fils[i].catId, sizeof fils[i].catId, "id%d", i);
    fils[i].ini = i*3; fils[i].n = 3;
  }
  snprintf(fils[0].chave, sizeof fils[0].chave, "continue_watching");
  fils[0].base[0] = fils[0].catId[0] = 0;
  snprintf(fils[14].titulo, sizeof fils[14].titulo, "Netflix - Filme");
  snprintf(fils[15].titulo, sizeof fils[15].titulo, "Oscar - Filme");
  cat_definir_tudo(itensTeste, 48, fils, 16);
  sincronizarFileiras();
  // A INTENCAO ORIGINAL, mantida de proposito: 16 catalogos entram, mais
  // "Amigos assistindo" e o destaque, e NENHUM catalogo se perde (o laco logo
  // abaixo cobra isso).
  //
  // ESTA ASSERCAO FALHA HOJE, e a falha e informacao, nao ruido. O teto de
  // fileiras (FIL_LIMITE_MAX, 16) conta as fileiras INSERIDAS junto com as de
  // catalogo, entao a composicao de 18 vagas e cortada em 16 e dois catalogos
  // somem. E exatamente a contabilidade que o relator do issue #18 descreve:
  // "they still take up the 16-row limit and push other collections".
  //
  // NAO AJUSTE O NUMERO PARA FICAR VERDE. Trocar 17 por 16 aqui gravaria como
  // correto o comportamento que o #18 questiona, e o laco de "nenhum catalogo
  // removido" teria de ser afrouxado junto — que e a asserção que da valor ao
  // arquivo. Quando o #18 for decidido, esta linha vira a prova da decisao.
  assert(nFileiras == 17);
  assert(foco.nFileiras == 17);
  assert(fileiras[1].tipo == FILEIRA_SOCIAL && fileiras[1].ini == -1 && fileiras[1].n == 1);
  assert(fileiras[2].tipo == FILEIRA_DESTAQUE);
  for (int i = 0; i < 16; i++) {
    int achou = 0;
    for (int r = 0; r < nFileiras; r++)
      if (!strcmp(fileiras[r].chave, fils[i].chave)) achou++;
    assert(achou == 1); // nenhum catálogo removido ou duplicado
  }
  // #201 (mackojanko): COM o destaque no topo, a primeira fileira de catalogo
  // sai em Automatico como as outras — o destaque ja e a vitrine e percorre
  // os mesmos titulos. Na Dinamica a vitrine continua (e regra dela).
  { int lay, r, destaques;
    for (lay = 0; lay < HOME_LAYOUT_N; lay++) {
      homeCom(lay, 1);
      sincronizarFileiras();
      assert(!strcmp(fileiras[2].chave, "catalogo_1"));
      for (r = 0, destaques = 0; r < nFileiras; r++)
        destaques += fileiras[r].tipo == FILEIRA_DESTAQUE;
      if (lay == HOME_LAYOUT_DINAMICA) {
        assert(fileiras[2].tipo == FILEIRA_DESTAQUE && destaques == 1);
      } else {
        assert(fileiras[2].tipo == FILEIRA_NORMAL && destaques == 0);
        assert(fileiras[2].tipo == fileiras[3].tipo);   // igual a seguinte
      }
    }
    homeCom(HOME_LAYOUT_MODERNA, 0);
    sincronizarFileiras();
    assert(fileiras[2].tipo == FILEIRA_DESTAQUE);
  }
  foco.fileira = 0; foco.coluna = 0;
  for (int i = 0; i < 16; i++) assert(focus_mover(&foco, 0, 1));
  assert(foco.fileira == 16);
  assert(!focus_mover(&foco, 0, 1));
  // Mesma contagem, ordem diferente: manter chave, coluna e scroll.
  foco.fileira = 5; foco.coluna = 2; scrollX[5] = 123;
  char chave[192]; snprintf(chave, sizeof chave, "%s", fileiras[5].chave);
  CatFileira troca = fils[4]; fils[4] = fils[8]; fils[8] = troca;
  cat_definir_tudo(itensTeste, 48, fils, 16);
  sincronizarFileiras();
  assert(!strcmp(fileiras[foco.fileira].chave, chave));
  assert(foco.coluna == 2);
  assert(scrollX[foco.fileira] == 123);
  assert(col_carregar("tests/fixtures/collections") == 2);
  assert(col_folder(0)->nSources==2);
  assert(col_folder(0)->frames==0);
  assert(col_folder(-1)==NULL);
  const char *ids[]={"", "now_playing_movies","trending_movies","trending_series",
    "ai_movies_for_you","ai_series_for_you","snoak_top100_movies","snoak_top100_series"};
  for(int i=1;i<8;i++)snprintf(fils[i].catId,sizeof fils[i].catId,"%s",ids[i]);
  // TOP10 sai do TITULO que o catalogo trouxe (perfilCatalogo), nao do catId —
  // o id e do addon e pode ser qualquer coisa.
  snprintf(fils[6].titulo,sizeof fils[6].titulo,"Top 100 - Filme");
  snprintf(fils[7].titulo,sizeof fils[7].titulo,"Top 100 - Serie");
  cat_definir_tudo(itensTeste,48,fils,16);filsAplicadas=-1;sincronizarFileiras();
  // SEM TABELA DE CURADORIA: a ordem e a da conta (catordem) ou a local
  // (fil_unir) — a montagem nao renomeia nem reordena por cima da escolha.
  // O que a home acrescenta sozinha entra nos lugares fixos dela: "Entre
  // amigos" em segundo, grupos de colecao no FIM, na ordem declarada.
  assert(nFileiras>=11);
  assert(fileiras[1].tipo==FILEIRA_SOCIAL);
  // O primeiro catalogo com conteudo vira o destaque (destaque desligado, ver
  // homeCom); o resto segue a ordem. "Continuar assistindo" fica em [0] e
  // "Entre amigos" em [1], entao o destaque e o [2] — a assercao antiga pedia
  // o [0] e abortava aqui antes do #201 tambem.
  assert(fileiras[2].tipo==FILEIRA_DESTAQUE);
  assert(!strcmp(fileiras[2].chave,"catalogo_1"));
  assert(fileiras[nFileiras-2].tipo==FILEIRA_CATALOGOS);
  assert(!strcmp(fileiras[nFileiras-2].titulo,"Streaming"));
  assert(!strcmp(col_folder(fileiras[nFileiras-2].folders[0])->title,"Netflix"));
  assert(!strcmp(fileiras[nFileiras-1].titulo,"Themes"));
  { int tops=0;
    for (int r=0; r<nFileiras; r++)
      if (fileiras[r].tipo==FILEIRA_TOP10) {
        tops++;
        assert(fileiras[r].stackN==3 && fileiras[r].n==1);
      }
    assert(tops==2); }
  // #366: o primeiro do Top 10 visto. desenhaPilha andava em ini+k.
  { int r, k; Fileira *top = NULL;
    for (int i = 0; i < 48; i++) {
      snprintf(itensTeste[i].imdb, sizeof itensTeste[i].imdb, "tt%07d", i);
      snprintf(itensTeste[i].tipo, sizeof itensTeste[i].tipo, "movie");
    }
    cat_historico_definir_id(itensTeste[fils[6].ini + 1].imdb, "movie", 1);
    for (int i = 0; i < fils[8].n; i++)
      cat_historico_definir_id(itensTeste[fils[8].ini + i].imdb, "movie", 1);
    ocultarVistos = 1;
    cat_definir_tudo(itensTeste, 48, fils, 16); filsAplicadas = -1; sincronizarFileiras();
    for (r = 0; r < nFileiras; r++)
      if (fileiras[r].tipo == FILEIRA_TOP10 && !strcmp(fileiras[r].chave, fils[6].chave)) top = &fileiras[r];
    assert(top && top->stackN == 2 && top->usaItens);
    assert(cat_visto(cat_item(top->ini + 1)));
    { Fileira numerals = *top;
      numerals.tipo = FILEIRA_TOP10_NUM; numerals.n = top->stackN;
      assert(fileiraRanking(&numerals, 0) == 1);
      assert(fileiraRanking(&numerals, 1) == 3);
    }
    for (k = 0; k < top->stackN && k < 6; k++) assert(!cat_visto(cat_item(fileiraItemIndice(top, k))));
    for (r = 0; r < nFileiras; r++)
      if (!strcmp(fileiras[r].chave, fils[8].chave))
        assert(!fileiras[r].n && !fileiras[r].verTudo);
    for (int i = 0; i < fils[6].n; i++)
      cat_historico_definir_id(itensTeste[fils[6].ini + i].imdb, "movie", 1);
    filsAplicadas = -1; sincronizarFileiras();
    top = NULL;
    for (r = 0; r < nFileiras; r++)
      if (!strcmp(fileiras[r].chave, fils[6].chave)) top = &fileiras[r];
    assert(top && !top->n && !top->stackN && !top->verTudo); }
  for (int i=8; i<16; i++) {
    int encontrado=0;
    for (int r=0; r<nFileiras; r++)
      if (!strcmp(fileiras[r].chave, fils[i].chave)) encontrado=1;
    assert(encontrado);
  }
  fileiras[9].n=3;fileiras[9].stackN=0;fileiras[9].verTudo=1;
  for(int i=0;i<nFileiras;i++)assert(strcmp(fileiras[i].titulo,"Seus catálogos"));
  snprintf(fils[15].chave,sizeof fils[15].chave,"social_activity");
  fils[15].base[0]=fils[15].catId[0]=0;
  cat_definir_tudo(itensTeste,48,fils,16);sincronizarFileiras();
  int sociais=0;
  for(int i=0;i<nFileiras;i++)if(fileiras[i].tipo==FILEIRA_SOCIAL){
    sociais++;assert(fileiras[i].ini==45 && fileiras[i].n==3);
  }
  assert(sociais==1); // dados reais substituem vazio, nunca duplicam a fileira
  assert(fileiras[9].stackN==0 && fileiras[9].n==3 && fileiras[9].verTudo);

  // O destaque continua mostrando uma terceira alternativa quando a primeira
  // fonte entrega só dois títulos: o item extra vem de um catálogo já carregado
  // e mantém seu índice real, sem fallback de arte de outro título.
  { CatFileira curtas[2] = {0};
    snprintf(curtas[0].chave, sizeof curtas[0].chave, "curated_movies");
    snprintf(curtas[0].titulo, sizeof curtas[0].titulo, "Destaques");
    snprintf(curtas[0].base, sizeof curtas[0].base, "https://example.invalid/addon");
    snprintf(curtas[0].tipo, sizeof curtas[0].tipo, "movie");
    snprintf(curtas[0].catId, sizeof curtas[0].catId, "curated");
    curtas[0].ini = 0; curtas[0].n = 2;
    snprintf(curtas[1].chave, sizeof curtas[1].chave, "more_movies");
    snprintf(curtas[1].titulo, sizeof curtas[1].titulo, "Mais filmes");
    snprintf(curtas[1].base, sizeof curtas[1].base, "https://example.invalid/addon");
    snprintf(curtas[1].tipo, sizeof curtas[1].tipo, "movie");
    snprintf(curtas[1].catId, sizeof curtas[1].catId, "more");
    curtas[1].ini = 2; curtas[1].n = 2;
    cat_definir_tudo(itensTeste, 4, curtas, 2);
    sincronizarFileiras();
    { int destaque = -1;
      for (int r = 0; r < nFileiras; r++)
        if (fileiras[r].tipo == FILEIRA_DESTAQUE) { destaque = r; break; }
      assert(destaque >= 0);
      assert(fileiras[destaque].n == 3);
      assert(fileiraItemIndice(&fileiras[destaque], 0) == 0);
      assert(fileiraItemIndice(&fileiras[destaque], 1) == 1);
      assert(fileiraItemIndice(&fileiras[destaque], 2) == 2);
    }
  }

  // Arte de outro titulo nunca e fallback silencioso, mesmo quando o indice
  // esta alem do acervo local. Sem catalogo, os vetores locais continuam
  // disponiveis apenas na mesma posicao.
  snprintf(itensTeste[0].poster,sizeof itensTeste[0].poster,"own-poster.jpg");
  snprintf(itensTeste[0].backdrop,sizeof itensTeste[0].backdrop,"own-backdrop.jpg");
  snprintf(itensTeste[1].poster,sizeof itensTeste[1].poster,"other-poster.jpg");
  nBd=2; nPst=1;
  snprintf(bd[0],sizeof bd[0],"fallback-0.jpg");
  snprintf(bd[1],sizeof bd[1],"fallback-1.jpg");
  snprintf(pst[0],sizeof pst[0],"fallback-poster-0.jpg");
  cat_definir_tudo(itensTeste,48,NULL,0);
  assert(!strcmp(arte_por_identidade(0,0),"own-poster.jpg"));
  assert(!strcmp(arte_por_identidade(0,1),"own-backdrop.jpg"));
  assert(!strcmp(arte_por_identidade(1,0),"other-poster.jpg"));
  assert(arte_por_identidade(999,0)==NULL);

  // Ranked rows keep portrait artwork and geometry with landscape posters on.
  {
    Fileira saved = fileiras[0];
    int savedRows = nFileiras, savedFocus = foco.fileira;
    int savedHero = testeHero;
    nFileiras = 1; foco.fileira = 0; testeHero = 0;
    for (int t = 0; t < 2; t++) {
      fileiras[0] = (Fileira){.tipo = t ? FILEIRA_TOP10 : FILEIRA_TOP10_NUM,
                              .ini = 0, .n = 1, .escala = 1.0f};
      float w = larguraFil(0), h = alturaFil(0);
      for (testeDeitados = 0; testeDeitados <= 1; testeDeitados++) {
        assert(!strcmp(dinArteDaCor(), "own-poster.jpg"));
        assert(larguraFil(0) == w && alturaFil(0) == h);
      }
    }
    testeDeitados = 0;
    fileiras[0] = saved; nFileiras = savedRows;
    foco.fileira = savedFocus; testeHero = savedHero;
  }

  // A integracao de segunda ordem retargeta sem overshoot, e reduced motion
  // pode saltar ao destino sem deixar velocidade residual.
  { float x=0, v=0;
    for (int i=0; i<60; i++) {
      x=anim_mola2(&v,x,1.0f,0.016f,NV_MOLA2_SCROLL);
      assert(x>=0.0f && x<=1.0f);
    }
    x=anim_mola2(&v,x,0.0f,0.016f,NV_MOLA2_SCROLL);
    assert(x>=0.0f && x<=1.0f);
    x=anim_mola2_reduzida(&v,x,0.35f,0.016f,NV_MOLA2_SCROLL,1);
    assert(x==0.35f && v==0.0f);
  }
  assert(NV_HERO_FADE_MS>=180.0f && NV_HERO_FADE_MS<=250.0f);

  // --- #103: o cartaz EM FOCO cabe inteiro, tambem no fim da fileira -------
  //
  // O relato veio com foto: fileira de 12 titulos, foco no 11o, e o cartao
  // aberto passando por baixo da borda direita da tela ("part of the focused
  // tile is cut off"). A causa estava no alvo da rolagem — ele media o cartao
  // EM REPOUSO e somava so metade da escala de foco, entao nem a abertura 16:9
  // nem o anel de 4 px entravam na conta.
  //
  // A asserção refaz, a partir do alvo, a MESMA conta que o desenho faz para
  // achar a borda direita do cartao focado; e a unica forma de o teste falhar
  // pelo motivo certo se alguem mexer num dos dois lados sozinho.
  {
    fileiras[0].tipo = FILEIRA_NORMAL;
    fileiras[0].n = 12;
    fileiras[0].stackN = 0;
    fileiras[0].verTudo = 0;
    fileiras[0].escala = 1.0f;
    if (nFileiras < 1) nFileiras = 1;

    const float lw    = larguraFil(0);
    const float passo = passoFil(0);
    const float esc   = 1.0f + escalaDe(FILEIRA_NORMAL);
    const float anel  = ajustes_borda_foco() ? NV_ANEL_FOCO : 0.0f;
    const float limite = NV_TELA_W - NV_HOME_SAFE_RIGHT;

    // As duas maneiras de o cartao ficar maior que a caixa em repouso: so a
    // escala de foco (abre 0) e a abertura em repouso ja completa (abre 1).
    for (int k = 0; k < 2; k++) {
      float abre = (float)k;
      for (int col = 0; col < fileiras[0].n; col++) {
        float sx = alvoScrollFil(0, col, 0.0f, abre);
        float w  = lw * esc
                 + (alturaFil(0) * esc * NV_EXP_ASPECTO - lw * esc) * abre;
        float cx = ajustes_conteudo_x() + (float)col * passo - sx
                 + lw * 0.5f + (w - lw * esc) * 0.5f;
        // Meio pixel de tolerancia: o alvo e float e o corte e visual.
        assert(cx + w * 0.5f + anel <= limite + 0.5f);
        // E nada some pela esquerda: a caixa em repouso do cartao focado nunca
        // comeca antes da margem de conteudo.
        assert(ajustes_conteudo_x() + (float)col * passo - sx >= -0.5f);
      }
    }
    // A primeira coluna nao rola: a fileira comeca onde sempre comecou.
    assert(alvoScrollFil(0, 0, 0.0f, 0.0f) == 0.0f);
    assert(alvoScrollFil(0, 0, 0.0f, 1.0f) == 0.0f);
    // E o ULTIMO cartaz, que e o da foto, exige rolagem MAIOR que a que a
    // conta antiga (so meia escala de foco) produzia.
    { int ult = fileiras[0].n - 1;
      float antigo = (float)ult * passo + lw
                   + lw * escalaDe(FILEIRA_NORMAL) * 0.5f
                   - (NV_TELA_W - ajustes_conteudo_x() - NV_HOME_SAFE_RIGHT);
      assert(alvoScrollFil(0, ult, 0.0f, 1.0f) > antigo + 1.0f); }

    // AS TRES PARCELAS, cada uma cobrada sozinha — sem isto, uma delas pode
    // sumir e o teste acima continuar verde porque as outras duas sobram.
    //
    // A parcela da ABERTURA e a que mais cresce e a que menos aparece nesta
    // execucao: com os ajustes de fabrica "Pôsteres horizontais" esta LIGADO,
    // o cartaz ja mede 16:9 (318x183 medidos aqui) e abrir quase nao o
    // aumenta. Quem relatou o #103 tem a opcao desligada — cartaz 2:3 — e ai
    // o aberto mede altura*16/9 contra a largura em pe: o pulo e de centenas
    // de pixels, e era ele que faltava. A asserção compara com a formula do
    // desenho para valer nas duas configuracoes.
    assert(sobraDireitaFoco(0, 1.0f) > sobraDireitaFoco(0, 0.0f));
    { float e = 1.0f + escalaDe(FILEIRA_NORMAL);
      float abertura = alturaFil(0) * e * NV_EXP_ASPECTO - lw * e;
      assert(fabsf(sobraDireitaFoco(0, 1.0f) - sobraDireitaFoco(0, 0.0f)
                   - abertura) < 0.01f); }
    // O anel so entra quando existe, e a escala so quando o card cresce: as
    // duas nunca valem juntas (ver escalaDe).
    assert(fabsf(sobraDireitaFoco(0, 0.0f)
                 - (lw * escalaDe(FILEIRA_NORMAL) * 0.5f
                    + (ajustes_borda_foco() ? NV_ANEL_FOCO : 0.0f))) < 0.01f);
  }

  // "Mostrar genero no destaque": o card editorial sem logo desenha nome +
  // genero; desligado, so o nome (a arte e o logo ficam, o genero sai).
  { CatItem it = {0}; int ligado, desligado;
    snprintf(it.titulo, sizeof it.titulo, "Titulo");
    snprintf(it.genero, sizeof it.genero, "Terror");
    snprintf(it.tipo, sizeof it.tipo, "movie");
    nTxtDesenhado = 0; testeGenero = 1;
    desenhaEditorialCard(&it, FILEIRA_DESTAQUE, 0, 0, 640, 360, 8);
    ligado = nTxtDesenhado;
    nTxtDesenhado = 0; testeGenero = 0;
    desenhaEditorialCard(&it, FILEIRA_DESTAQUE, 0, 0, 640, 360, 8);
    desligado = nTxtDesenhado;
    assert(ligado == desligado + 1);
    // Desligado tira a linha de baixo INTEIRA do destaque: classificacao
    // (Moderna) e selo IMDb (Padrao/Dinamica) tambem saem.
    snprintf(it.classificacao, sizeof it.classificacao, "16");
    nBadge = 0; desenhaEditorialCard(&it, FILEIRA_DESTAQUE, 0, 0, 640, 360, 8);
    assert(nBadge == 0);
    testeImdb = 75;
    for (int lay = HOME_LAYOUT_PADRAO; lay <= HOME_LAYOUT_DINAMICA; lay++) {
      homeCom(lay, 1);
      nBadge = 0; desenhaEditorialCard(&it, FILEIRA_DESTAQUE, 0, 0, 640, 360, 8);
      assert(nBadge == 0);
      testeGenero = 1;
      nBadge = 0; desenhaEditorialCard(&it, FILEIRA_DESTAQUE, 0, 0, 640, 360, 8);
      assert(nBadge == 1);
      testeGenero = 0;
    }
    testeImdb = 0; homeCom(HOME_LAYOUT_MODERNA, 1);
    // So o destaque: COLECAO/SERVICO/SOCIAL mantem genero e selo.
    TipoFileira outras[] = { FILEIRA_COLECAO, FILEIRA_SERVICO, FILEIRA_SOCIAL };
    for (int k = 0; k < 3; k++) {
      int on, onB, off, offB;
      testeGenero = 1; nTxtDesenhado = 0; nBadge = 0;
      desenhaEditorialCard(&it, outras[k], 0, 0, 640, 360, 8);
      on = nTxtDesenhado; onB = nBadge;
      testeGenero = 0; nTxtDesenhado = 0; nBadge = 0;
      desenhaEditorialCard(&it, outras[k], 0, 0, 640, 360, 8);
      off = nTxtDesenhado; offB = nBadge;
      assert(on == off && onB == offB && onB == 1);
    }
    testeGenero = 1; }

  // O mesmo ajuste no POSTER P/M/G: desligado, sem sinopse nem linha de
  // genero (so arte e logo/nome); ligado, tudo como sempre.
  { CatItem it = {0}; float fatores[3] = { 0.8f, 1.0f, 1.25f };
    snprintf(it.titulo, sizeof it.titulo, "Titulo");
    snprintf(it.genero, sizeof it.genero, "Terror");
    snprintf(it.sinopse, sizeof it.sinopse, "Uma sinopse.");
    for (int k = 0; k < 3; k++) {
      int on, onS, off, offS;
      testeGenero = 1; stageTextDraws = 0; nSinopse = 0;
      desenhaPosterGrande(&it, NULL, 0, 0, 480, 640, 8, 1.0f, fatores[k]);
      on = stageTextDraws; onS = nSinopse;
      testeGenero = 0; stageTextDraws = 0; nSinopse = 0;
      desenhaPosterGrande(&it, NULL, 0, 0, 480, 640, 8, 1.0f, fatores[k]);
      off = stageTextDraws; offS = nSinopse;
      assert(onS == 1 && offS == 0 && on == off + 1);
    }
    testeGenero = 1; }

  // FUNDO DA DINAMICA (#205): cinza e cor fixa nao mudam com o foco; o
  // imersivo fica numa faixa estreita de L e nao retinge a cada passo.
  {
    struct { const char *url; float rgb[3]; } arte[5] = {
      { "t://amarelo", { 1.00f, 0.90f, 0.10f } },   // amarelo vivo
      { "t://escuro",  { 0.05f, 0.05f, 0.12f } },   // capa escura
      { "t://vermelho",{ 0.90f, 0.08f, 0.10f } },
      { "t://azul",    { 0.10f, 0.25f, 0.95f } },
      { "t://branco",  { 0.97f, 0.97f, 0.95f } },   // cartaz branco
    };
    float maxD[3] = { 0, 0, 0 };
    for (int i = 0; i < 5; i++) {
      CorvivaPaleta pal; memset(&pal, 0, sizeof pal);
      pal.ok = 1; memcpy(pal.acento, arte[i].rgb, sizeof pal.acento);
      corviva_anotar(arte[i].url, &pal);
    }
    for (int modo = 0; modo < 3; modo++) {
      float cor[5][3], ls[5], lmin = 9, lmax = -9, spread = 0;
      testeFundoDin = modo;
      dinUlt = 0; dinArteVista[0] = dinArteAssentada[0] = 0; dinVistaDesde = 0;
      memcpy(dinCor, DIN_CINZA, sizeof dinCor); memcpy(dinAlvo, DIN_CINZA, sizeof dinAlvo);
      dinQueda = dinQuedaAlvo = 1.0f;
      Uint32 t = 1000;
      for (int i = 0; i < 5; i++) {
        for (int f = 0; f < 240; f++) { t += 16; dinPasso(t, arte[i].url); }   // ~3,8 s: assenta
        memcpy(cor[i], dinCor, sizeof cor[i]);
        float lab[3]; corviva_srgb_para_oklab(cor[i], lab); ls[i] = lab[0];
        if (ls[i] < lmin) lmin = ls[i];
        if (ls[i] > lmax) lmax = ls[i];
        for (int k = 0; k < 3; k++) {
          float d = fabsf(cor[i][k] - cor[0][k]);
          if (d > spread) spread = d;
        }
      }
      printf("fundo din modo %d: dL=%.4f spread_rgb=%.4f\n", modo, lmax - lmin, spread);
      if (modo != DIN_FUNDO_IMERSIVO) {
        assert(spread < 1e-4f);            // identico em todos os titulos
        if (modo == DIN_FUNDO_CINZA)
          assert(fabsf(cor[0][0] - 28 / 255.0f) < 1e-3f && fabsf(cor[0][2] - 30 / 255.0f) < 1e-3f);
        assert(dinQueda > 0.999f);          // liso, sem degrade
      } else {
        assert(spread > 0.002f);            // acompanha a arte (controle: o teste enxerga cor)
        assert(lmax - lmin < 0.06f);        // faixa estreita de brilho
        assert(lmax < 0.30f && lmin > 0.14f);
        for (int i = 0; i < 5; i++) {       // croma com teto
          float lab[3]; corviva_srgb_para_oklab(cor[i], lab);
          assert(sqrtf(lab[1] * lab[1] + lab[2] * lab[2]) < 0.05f);
        }
        (void)maxD;
      }
    }
    // ROLAGEM RAPIDA no imersivo: a cada 100 ms outro titulo; ninguem assenta, a cor nao retinge.
    {
      testeFundoDin = DIN_FUNDO_IMERSIVO;
      dinUlt = 0; dinArteVista[0] = dinArteAssentada[0] = 0; dinVistaDesde = 0;
      Uint32 t = 1000;
      for (int f = 0; f < 120; f++) { t += 16; dinPasso(t, arte[0].url); }
      char primeira[512]; snprintf(primeira, sizeof primeira, "%s", dinArteAssentada);
      assert(!strcmp(primeira, arte[0].url));
      int retintou = 0;
      for (int passo = 0; passo < 30; passo++) {          // 3 s, um titulo a cada ~100 ms
        for (int f = 0; f < 6; f++) { t += 16; dinPasso(t, arte[1 + passo % 4].url); }
        if (strcmp(dinArteAssentada, primeira)) retintou++;
      }
      printf("fundo din rolagem rapida: retingiu %d vezes em 30 passos\n", retintou);
      assert(retintou == 0);
      for (int f = 0; f < 40; f++) { t += 16; dinPasso(t, arte[3].url); }   // para: >= 400 ms
      assert(!strcmp(dinArteAssentada, arte[3].url));
    }
    testeFundoDin = 0;
    // CONTRASTE: texto branco sobre o fundo mais claro dos tres modos fica legivel (>= 7:1).
    {
      float pior = 99.0f;
      for (int modo = 0; modo < 3; modo++) {
        testeFundoDin = modo; dinUlt = 0;
        dinArteVista[0] = dinArteAssentada[0] = 0;
        Uint32 t = 1000;
        for (int i = 0; i < 5; i++)
          for (int f = 0; f < 240; f++) {
            t += 16; dinPasso(t, arte[i].url);
            float lin[3];
            for (int k = 0; k < 3; k++) {
              float c = dinCor[k];
              lin[k] = c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
            }
            float y = 0.2126f * lin[0] + 0.7152f * lin[1] + 0.0722f * lin[2];
            float ct = corviva_contraste_y(1.0f, y);
            if (ct < pior) pior = ct;
          }
      }
      printf("fundo din pior contraste do texto branco: %.2f:1\n", pior);
      assert(pior >= 7.0f);
      testeFundoDin = 0;
    }
  }

  free(itensTeste);
  puts("home layout: PASS (fallback, colecoes, ordem, ranks, foco, rolagem #103)");
  return 0;
}
