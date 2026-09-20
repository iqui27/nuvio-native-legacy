// Cartao de primeira vez do SOCIAL — tres paginas, uma por recurso novo.
//
// POR QUE EXISTE: recomendar um titulo a um amigo nao e um botao a mais, e uma
// forma nova de usar o app — e ela nasce ESCONDIDA em tres lugares diferentes
// (a aba Social atras da tecla AZUL, o aviaozinho na pagina do titulo, o menu
// do cartaz) e, pior, comeca VAZIA: sem contato nenhum a aba nao tem o que
// mostrar e o recurso parece quebrado. O dono pediu "um modal explicando as
// novas funcionalidades, todos bem diagramados e com imagens".
//
// IRMAO DE novidades.c e pipintro.c: mesma anatomia (cartao central sobre a
// home, figura a esquerda, recursos a direita, arquivo-marca na pasta de
// dados). O que muda e que aqui sao TRES PAGINAS, porque sao tres recursos
// distintos e empilhar doze linhas num cartao so daria um folheto, nao uma
// explicacao. Esquerda/direita andam, os tres pontos dizem onde a pessoa esta,
// e Voltar fecha em qualquer ponto — ninguem e obrigado a ler ate o fim.
//
// O OK AVANCA, e nao fecha como nos irmaos de UMA pagina. Num cartao de tres
// paginas o OK que fecha e uma armadilha: o gesto mais natural do controle
// apagaria duas telas que a pessoa nunca veria existir. Na ULTIMA pagina ele
// fecha, e o rodape diz qual dos dois ele e em cada momento.
//
// A MARCA E POR CONTEUDO, nao por versao (a mesma regra de novidades.c):
// "recintro-social.txt" cobre ESTA rodada. A proxima novidade ganha o seu
// proprio arquivo e o seu proprio cartao.
//
// AS FIGURAS NAO SAO IMAGEM. Nao ha renderizador de SVG neste app (ver gfx.h),
// entao cada pagina desenha uma MINIATURA da tela de verdade com as mesmas
// primitivas dela, nas mesmas proporcoes: o painel da tecla AZUL sai em 0.62
// dos 776x1032 de salvospainel.c, os botoes circulares saem na proporcao
// glifo/circulo de detail.c, e as seis caixas do codigo saem na forma de
// recenviar.c. Os ROTULOS dentro das miniaturas sao as chaves de i18n que
// aquelas telas ja usam ("SALVOS", "SOCIAL", "Recomendar a um amigo"), e nao
// texto novo — assim a figura nao pode divergir da tela em outro idioma.
//
// SEM NUVIO_REC_URL O CARTAO NAO EXISTE. Anunciar um recurso que nao esta
// naquele pacote e pior que silencio: recomenda_ativo() decide, aqui e em
// salvospainel.c, e as duas superficies somem juntas.
#include "recintro.h"
#include "recomenda.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RI_ARQ "recintro-social.txt"

// Mesma geometria de novidades.c e pipintro.c — a familia de anuncios fica
// reconhecivel de longe, e quem ja viu um sabe onde olhar neste.
#define RI_W        1440.0f
#define RI_H         800.0f
#define RI_X        ((NV_TELA_W - RI_W) * 0.5f)
#define RI_Y        ((NV_TELA_H - RI_H) * 0.5f)
#define RI_PAD        64.0f
#define RI_FIG_W     560.0f
#define RI_TXT_X     (RI_X + RI_PAD + RI_FIG_W + 56.0f)
#define RI_TXT_W     (RI_X + RI_W - RI_PAD - RI_TXT_X)
#define RI_FEAT_H    124.0f
#define RI_FEAT_ICO   64.0f
#define RI_ABRIR_MS  280.0f
#define RI_FECHAR_MS 160.0f
#define RI_PAG_MS    170.0f
#define RI_PAGINAS       3

static int   aberto, decidido, pagina, sentido;
static float entrada, passo = 1.0f;

int recintro_aberto(void) { return aberto; }

// Grava a marca. Sem pasta gravavel isto e no-op e o cartao volta no proximo
// arranque; e honesto, e dados.c ja disse no log por que nao ha pasta. Mesma
// escolha de novidades.c e de registro.c.
static void marcarVisto(void) { dados_gravar(RI_ARQ, "1\n"); }

void recintro_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  // O PACOTE SEM SERVICO NAO ANUNCIA NADA, e nem grava marca: se o dono
  // publicar depois um pacote COM NUVIO_REC_URL, o cartao ainda tem de
  // aparecer naquela TV.
  if (!recomenda_ativo()) { decidido = 0; return; }
  s = dados_ler(RI_ARQ);
  if (s) { free(s); return; }
  aberto = 1;
  pagina = 0;
  sentido = 0;
  passo = 1.0f;
}

static void fechar(void) {
  aberto = 0;
  marcarVisto();
}

static void ir(int d) {
  int nova = pagina + d;
  if (nova < 0 || nova >= RI_PAGINAS) return;
  pagina = nova;
  sentido = d;
  passo = 0.0f;
}

void recintro_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_LEFT)  { ir(-1); return; }
  if (k == SDLK_RIGHT) { ir(+1); return; }
  // OK AVANCA ATE A ULTIMA, e la fecha. Ver a nota no topo.
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (pagina < RI_PAGINAS - 1) ir(+1);
    else fechar();
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    fechar();
    return;
  }
  // CIMA/BAIXO ficam engolidos: nao ha o que focar, e vazar a tecla para a home
  // moveria o foco dela embaixo do cartao. Mesma regra de novidades.c.
}

void recintro_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (passo < 1.0f) passo = anim_rampa(passo, 1.0f, dt, RI_PAG_MS);
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? RI_ABRIR_MS : RI_FECHAR_MS);
}

// --- FIGURA 1: o painel da tecla AZUL na aba Social --------------------------
//
// ESCALA UNICA, 0.62, aplicada a TODAS as medidas de salvospainel.c (SP_W 776,
// SP_H 1032, SP_PAD 44, SP_ABAS_Y +56, SP_ABAS_H 52, SP_LISTA_Y 200,
// SP_POSTER_W/H 92x138, SP_PASSO 160). Escalar cada pedaco "a olho" seria
// desenhar OUTRA tela com o mesmo nome. A altura e a unica coisa CORTADA: 410
// em vez dos 640 cheios, porque o que interessa sao as abas e tres linhas.
static void figPainel(float x, float y, float a) {
  const float e = 0.62f;
  const float pw = 776.0f * e, ph = 410.0f, pad = 44.0f * e;
  float px = x + (RI_FIG_W - pw) * 0.5f;
  float ar, ag, ab, tx, ty, th, ly;
  int i;
  GfxRect p = { px, y, pw, ph };
  ajustes_acento(&ar, &ag, &ab);

  gfx_cor(p, 0.036f, 0.075f, 0.078f, 0.088f, a);
  gfx_rect(p, 0, GFX_ANEL, 0, 2.0f / pw, 0, 0.036f, 0.32f, 0.34f, 0.40f,
           a * 0.7f);

  // --- linha de abas ---------------------------------------------------------
  tx = px + pad;
  ty = y + 56.0f * e;
  th = 52.0f * e;
  for (i = 0; i < 2; i++) {
    int ativa = (i == 1);                       // a foto mostra SOCIAL aberta
    int cor = ativa ? 20 : 176;
    TxtLinha t = txt_linha(TXT_MINI, i18n(i == 0 ? "SALVOS" : "SOCIAL"),
                           cor, cor, cor, 255);
    GfxRect pil = { tx, ty, t.w + 44.0f * e, th };
    // A aba aberta E em foco: preenchida na cor de realce com texto escuro,
    // como a linha de abas de verdade (salvospainel.c) desenha desde
    // 16/09/2026 — ver NV_COR_FOCO em layout.h.
    if (ativa) gfx_cor(pil, NV_RAIO_PILL, ar, ag, ab, a);
    else       gfx_cor(pil, NV_RAIO_PILL, 1.0f, 1.0f, 1.0f, 0.04f * a);
    txt_desenhar_alpha(t, pil.x + 22.0f * e, ty + (th - (float)t.h) * 0.5f, a);
    tx += pil.w + 14.0f * e;
  }

  // --- tres linhas da lista --------------------------------------------------
  ly = y + 176.0f * e;
  for (i = 0; i < 3; i++) {
    float ry = ly + (float)i * 160.0f * e;
    float cw = 92.0f * e, ch = 138.0f * e;
    float lx = px + pad, ltx = px + 164.0f * e;
    float ltw = pw - pad - 164.0f * e;
    if (i == 0) {
      // Anel de foco na primeira linha, na COR DO TEMA — e assim que a linha
      // focada aparece no painel de verdade (salvospainel.c:desenhaRecLinha).
      GfxRect anel = { lx - 7.0f, ry - 6.0f, pw - pad * 2.0f + 14.0f, ch + 12.0f };
      gfx_cor(anel, 0.10f, 0.112f, 0.116f, 0.132f, a);
      gfx_rect(anel, 0, GFX_ANEL, 0, 2.5f / anel.h, 0, 0.10f, ar, ag, ab, a);
    }
    // Cartaz: retangulo de esqueleto, como a linha de verdade sem textura.
    // MAIS CLARO QUE O PREENCHIMENTO DO ANEL DE FOCO (0.112): na primeira
    // versao os dois mediam 0.16 e o cartaz da linha focada SUMIA dentro do
    // proprio foco — so a captura mostrou. Mesma familia do anel branco sobre
    // pilula clara que este recurso ja pagou uma vez. No painel de verdade nao
    // ha choque porque ali o cartaz e uma IMAGEM.
    gfx_cor((GfxRect){ lx, ry, cw, ch }, 0.08f, 0.175f, 0.180f, 0.205f, a);
    // Titulo e "quem mandou · ha quanto tempo": barras, e nao texto de mentira.
    gfx_cor((GfxRect){ ltx, ry + 4.0f, ltw * (0.86f - 0.16f * (float)i), 13.0f },
            0.5f, 0.52f, 0.54f, 0.60f, a * 0.95f);
    gfx_cor((GfxRect){ ltx, ry + 30.0f, ltw * (0.50f - 0.08f * (float)i), 9.0f },
            0.5f, 0.32f, 0.34f, 0.39f, a * 0.9f);
    // A FRASE E TEXTO DE VERDADE, e a chave ja existe: e um dos seis modelos
    // de recomenda.c, entao em ingles a figura muda junto com a tela.
    if (i == 0) {
      char buf[96];
      snprintf(buf, sizeof buf, "\xe2\x80\x9c%s\xe2\x80\x9d",
               i18n("Confia em mim"));
      { TxtLinha t = txt_linha_corta(TXT_MINI, buf, 214, 218, 228, 255, ltw);
        txt_desenhar_alpha(t, ltx, ry + 52.0f, a * 0.95f); }
    } else {
      gfx_cor((GfxRect){ ltx, ry + 56.0f, ltw * (0.42f + 0.10f * (float)i), 9.0f },
              0.5f, 0.42f, 0.44f, 0.50f, a * 0.85f);
    }
    // Ponto de "ainda nao lida" nas duas primeiras.
    if (i < 2)
      gfx_cor((GfxRect){ ltx - 20.0f, ry + 6.0f, 9.0f, 9.0f },
              0.5f, 0.42f, 0.72f, 0.98f, a);
  }

  // --- legenda: os dois sinais que a pagina cita -----------------------------
  //
  // O SELO VERDE VIVE AQUI, e nao na aba da miniatura, porque no painel de
  // verdade ele so aparece na aba FECHADA (salvospainel.c: `!ativa &&
  // novas > 0`) — desenha-lo numa aba aberta seria inventar um estado que a
  // tela nunca mostra. Na legenda ele diz a mesma coisa sem mentir na figura.
  { float ry = y + ph + 22.0f, bx = px;
    GfxRect selo = { bx, ry - 2.0f, 30.0f, 30.0f };
    TxtLinha n = txt_linha(TXT_MINI, "2", 12, 26, 16, 255);
    gfx_cor(selo, 0.5f, 0.400f, 0.733f, 0.416f, a);
    txt_desenhar_alpha(n, selo.x + (selo.w - (float)n.w) * 0.5f,
                       selo.y + (selo.h - (float)n.h) * 0.5f, a);
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("sem abrir ainda"),
                             176, 180, 190, 255);
      txt_desenhar_alpha(t, bx + 40.0f, ry, a * 0.85f); }
    bx += 40.0f + 150.0f;
    gfx_cor((GfxRect){ bx, ry + 8.0f, 12.0f, 12.0f }, 0.5f,
            0.42f, 0.72f, 0.98f, a);
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("não lida"),
                             176, 180, 190, 255);
      txt_desenhar_alpha(t, bx + 24.0f, ry, a * 0.85f); } }
}

// --- FIGURA 2: as duas portas do envio ---------------------------------------
//
// Em cima a linha de botoes da pagina do titulo (detail.c), com o circular do
// aviaozinho em FOCO — e o foco de la: o botao CRESCE e inverte a cor, sem
// anel nenhum (NV_DETW2_FOCO_*). Embaixo o cartaz com o menu de contexto, na
// unica linha que interessa. Sao os dois caminhos que abrem a mesma modal.
static void figEnviar(float x, float y, float a) {
  static const char *ICO[4] = { "mais", "naovisto", "fontes", "recomendar" };
  float bx, by;
  int i;
  GfxRect tela = { x, y, RI_FIG_W, 186.0f };

  gfx_cor(tela, 0.075f, 0.105f, 0.108f, 0.122f, a);
  gfx_cor((GfxRect){ x + 24.0f, y + 26.0f, 214.0f, 18.0f }, 0.5f,
          0.55f, 0.57f, 0.63f, a * 0.95f);
  gfx_cor((GfxRect){ x + 24.0f, y + 58.0f, 300.0f, 10.0f }, 0.5f,
          0.30f, 0.31f, 0.36f, a * 0.9f);
  // Pilula primaria: branca nos dois estados, como no aparelho.
  gfx_cor((GfxRect){ x + 24.0f, y + 98.0f, 150.0f, 50.0f }, NV_RAIO_PILL,
          0.96f, 0.96f, 0.97f, a);

  bx = x + 206.0f;
  by = y + 98.0f;
  for (i = 0; i < 4; i++) {
    int foco = (i == 3);
    float d = foco ? 62.0f : 50.0f;
    float cx = bx + (float)i * 72.0f + 25.0f, cy = by + 25.0f;
    GfxRect c = { cx - d * 0.5f, cy - d * 0.5f, d, d };
    float fr = 0.133f, fg = 0.133f, fb = 0.133f, ic = 1.0f;
    float g   = d * 0.333f;                // proporcao MEDIDA no aparelho
    GfxRect ig = { cx - g * 0.5f, cy - g * 0.5f, g, g };
    if (foco) ic = ajustes_acento_tinta(&fr, &fg, &fb);   // como detail.c
    gfx_cor(c, NV_RAIO_PILL, fr, fg, fb, a);
    gfx_icone(ig, ICO[i], ic, ic, ic, a);
  }

  // --- o outro caminho: segurar OK no cartaz --------------------------------
  { float y2 = y + 218.0f;
    GfxRect cartaz = { x + 24.0f, y2, 108.0f, 162.0f };
    GfxRect menu   = { x + 156.0f, y2, 380.0f, 200.0f };
    gfx_cor(cartaz, 0.08f, 0.16f, 0.165f, 0.19f, a);
    gfx_cor(menu, 0.07f, 0.115f, 0.118f, 0.132f, a);
    for (i = 0; i < 4; i++) {
      GfxRect l = { menu.x + 16.0f, menu.y + 12.0f + (float)i * 45.0f,
                    348.0f, 40.0f };
      if (i == 3) {
        // FOCO INVERTIDO EM DEGRAU, como o menu de verdade: fundo claro e
        // texto escuro. A chave ja existe (ctxmenu.c a usa), entao a figura
        // acompanha o idioma da tela.
        TxtLinha t = txt_linha_corta(TXT_MINI, i18n("Recomendar a um amigo"),
                                     17, 17, 17, 255, l.w - 32.0f);
        gfx_cor(l, 12.0f / l.h, 0.961f, 0.961f, 0.961f, a);
        txt_desenhar_alpha(t, l.x + 16.0f, l.y + (l.h - (float)t.h) * 0.5f, a);
      } else {
        gfx_cor(l, 12.0f / l.h, 0.176f, 0.180f, 0.196f, a);
        gfx_cor((GfxRect){ l.x + 16.0f, l.y + 15.0f,
                           l.w * (0.52f - 0.10f * (float)i), 10.0f },
                0.5f, 0.42f, 0.44f, 0.50f, a * 0.9f);
      }
    } }
}

// --- FIGURA 3: o codigo e as duas portas de "adicionar amigo" ----------------
//
// As seis caixas saem na forma de recenviar.c (RE_COD_W/H 96x116, vao 14),
// reduzidas para caber na coluna. O RAIO DO gfx_cor E FRACAO DA ALTURA, e nao
// pixel — a mesma pedra em que a primeira versao daquela tela tropecou.
static void figCodigo(float x, float y, float a) {
  static const char COD[7] = "k7m2xq";
  const float bw = 68.0f, bh = 82.0f, vao = 10.0f;
  float total = bw * 6.0f + vao * 5.0f;
  float cx = x + (RI_FIG_W - total) * 0.5f, i;
  GfxRect cartao = { x, y, RI_FIG_W, 168.0f };
  int k;

  gfx_cor(cartao, 16.0f / cartao.h, 0.105f, 0.108f, 0.122f, a);
  i = y + (cartao.h - bh) * 0.5f;
  for (k = 0; k < 6; k++) {
    GfxRect b = { cx + (float)k * (bw + vao), i, bw, bh };
    char ch[2];
    TxtLinha t;
    gfx_cor(b, 12.0f / bh, 1.0f, 1.0f, 1.0f, 0.10f * a);
    ch[0] = COD[k]; ch[1] = 0;
    t = txt_linha(TXT_HEADLINE, ch, 246, 248, 255, 255);
    txt_desenhar_alpha(t, b.x + (b.w - (float)t.w) * 0.5f,
                       b.y + (b.h - (float)t.h) * 0.5f, a);
  }

  // As duas linhas da tela "Adicionar um amigo", com o mesmo foco invertido
  // em degrau de recenviar.c. Os dois rotulos sao as chaves de la.
  for (k = 0; k < 2; k++) {
    GfxRect l = { x + 40.0f, y + 204.0f + (float)k * 68.0f,
                  RI_FIG_W - 80.0f, 56.0f };
    int foco = (k == 0);
    float fr = 0.176f, fg = 0.176f, fb = 0.176f;
    int cor = 240;
    if (foco) cor = (int)(ajustes_acento_tinta(&fr, &fg, &fb) * 255.0f + 0.5f);
    TxtLinha t = txt_linha_corta(TXT_CAPTION2,
        i18n(k == 0 ? "Procurar amigos do Trakt agora"
                    : "Digitar o código de um amigo"),
        cor, cor, cor, 255, l.w - 60.0f);
    gfx_cor(l, 14.0f / l.h, fr, fg, fb, a);
    txt_desenhar_alpha(t, l.x + 30.0f, l.y + (l.h - (float)t.h) * 0.5f, a);
  }
}

// Uma linha de recurso: icone em disco + titulo + descricao de ate 2 linhas.
// Mesmo desenho e mesmas medidas de novidades.c e pipintro.c.
static float feature(float x, float y, float w, const char *icone,
                     const char *tit, const char *desc, float a) {
  gfx_cor((GfxRect){ x, y + 6.0f, RI_FEAT_ICO, RI_FEAT_ICO },
          0.5f, 0.13f, 0.15f, 0.19f, a);
  gfx_icone((GfxRect){ x + 15.0f, y + 21.0f, 34.0f, 34.0f },
            icone, 0.62f, 0.80f, 0.96f, a);
  { TxtLinha t = txt_linha_corta(TXT_CALLOUT, i18n(tit), 246, 247, 252, 255,
                                 w - RI_FEAT_ICO - 20.0f);
    txt_desenhar_alpha(t, x + RI_FEAT_ICO + 20.0f, y + 4.0f, a); }
  { float h = txt_bloco(TXT_CAPTION, i18n(desc), 176, 180, 190,
                        x + RI_FEAT_ICO + 20.0f, y + 40.0f,
                        w - RI_FEAT_ICO - 20.0f, 27.0f, a * 0.95f, 2);
    return h > RI_FEAT_H - 40.0f ? h + 40.0f : RI_FEAT_H; }
}

// Os tres pontos. O ativo e MAIOR e sai na cor do tema; os outros sao cinza —
// nunca um branco cravado, que e o defeito que ja foi corrigido em outros
// pontos deste app.
static void pontos(float x, float y, float a) {
  float ar, ag, ab;
  int i;
  ajustes_acento(&ar, &ag, &ab);
  for (i = 0; i < RI_PAGINAS; i++) {
    float d = (i == pagina) ? 16.0f : 11.0f;
    GfxRect p = { x + (float)i * 30.0f + (16.0f - d) * 0.5f,
                  y + (16.0f - d) * 0.5f, d, d };
    if (i == pagina) gfx_cor(p, 0.5f, ar, ag, ab, a);
    else             gfx_cor(p, 0.5f, 0.34f, 0.35f, 0.40f, a * 0.9f);
  }
}

void recintro_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, y, ap, dx;
  (void)agora;
  if (entrada < 0.002f) return;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0,
          0.72f * entrada);

  dy = (1.0f - a) * 36.0f;
  { GfxRect p = { RI_X, RI_Y + dy, RI_W, RI_H };
    gfx_cor(p, 0.030f, 0.075f, 0.078f, 0.088f, 0.98f * a); }
  gfx_recorte(RI_X, RI_Y + dy, RI_W, RI_H);

  // A TROCA DE PAGINA E UM DESLIZE CURTO, na direcao da tecla: sem ele as tres
  // paginas parecem a MESMA tela piscando texto diferente, e a pessoa perde a
  // nocao de que andou. O cartao e o rodape ficam parados; so o conteudo anda.
  { float s = anim_suave(passo);
    dx = (1.0f - s) * 44.0f * (float)(sentido >= 0 ? 1 : -1);
    ap = a * (0.30f + 0.70f * s); }

  // --- coluna da figura ------------------------------------------------------
  { float fx = RI_X + RI_PAD + dx;
    const char *titulo = pagina == 0 ? "A aba Social"
                       : pagina == 1 ? "Recomendar um título"
                                     : "Adicionar um amigo";
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("NOVO NO NUVIO"),
                             150, 154, 165, 255);
      txt_desenhar_alpha(t, fx, RI_Y + dy + 64.0f, ap * 0.92f); }
    { TxtLinha t = txt_linha_corta(TXT_TITULO3, i18n(titulo),
                                   246, 247, 252, 255, RI_FIG_W);
      txt_desenhar_alpha(t, fx, RI_Y + dy + 96.0f, ap); }
    if      (pagina == 0) figPainel(fx, RI_Y + dy + 176.0f, ap);
    else if (pagina == 1) figEnviar(fx, RI_Y + dy + 192.0f, ap);
    else                  figCodigo(fx, RI_Y + dy + 230.0f, ap);
  }

  // --- coluna de recursos ----------------------------------------------------
  y = RI_Y + dy + 96.0f;
  if (pagina == 0) {
    y += feature(RI_TXT_X + dx, y, RI_TXT_W, "menu_library",
          "Está na tecla AZUL",
          "O painel de Salvos ganhou uma segunda aba, Social — é ali que chega "
          "o que os seus amigos mandaram.", ap);
    y += feature(RI_TXT_X + dx, y, RI_TXT_W, "recomendar",
          "Quem mandou, e o que disse",
          "Cada linha traz o cartaz, o nome de quem enviou, há quanto tempo e a "
          "frase que ele escolheu.", ap);
    y += feature(RI_TXT_X + dx, y, RI_TXT_W, "play",
          "OK abre o título",
          "Da lista você vai direto para a página do filme ou da série, sem "
          "procurar de novo.", ap);
    y += feature(RI_TXT_X + dx, y, RI_TXT_W, "avancar",
          "Chega sem você pedir",
          "O app confere ao abrir e a cada minuto. O número verde na aba conta "
          "o que você ainda não leu.", ap);
  } else if (pagina == 1) {
    y += feature(RI_TXT_X + dx, y, RI_TXT_W, "recomendar",
          "Um botão na página do título",
          "Ao lado de assistir, o botão do aviãozinho manda aquele filme ou "
          "série para um amigo.", ap);
    y += feature(RI_TXT_X + dx, y, RI_TXT_W, "mais",
          "Ou direto do cartaz",
          "Segure OK em cima de qualquer cartaz e escolha \"Recomendar a um "
          "amigo\" no menu que abre.", ap);
    y += feature(RI_TXT_X + dx, y, RI_TXT_W, "avancar",
          "Escolha o amigo, depois a frase",
          "Duas telas curtas: a sua lista de contatos e seis frases prontas, "
          "de \"Confia em mim\" a \"Terminei, sua vez\".", ap);
    y += feature(RI_TXT_X + dx, y, RI_TXT_W, "visto",
          "Ele recebe em até um minuto",
          "Com o app aberto, o aviso aparece na TV dele em até um minuto; "
          "fechado, assim que ele abrir.", ap);
  } else {
    y += feature(RI_TXT_X + dx, y, RI_TXT_W, "menu_profile",
          "Seu código tem seis caracteres",
          "Só letras e números, nada de símbolo — dá para ditar por telefone. "
          "Ele fica na aba Social.", ap);
    y += feature(RI_TXT_X + dx, y, RI_TXT_W, "mais",
          "Um código, e os dois viram contatos",
          "Quem digitar o seu entra na sua lista e você entra na dele. Ninguém "
          "precisa fazer o caminho de volta.", ap);
    y += feature(RI_TXT_X + dx, y, RI_TXT_W, "menu_search",
          "Amigos do Trakt entram sozinhos",
          "Quem você segue no Trakt vira contato automaticamente — mas só "
          "depois de ele também instalar o app.", ap);
    y += feature(RI_TXT_X + dx, y, RI_TXT_W, "oculto",
          "Dá para desfazer",
          "Remover um amigo apaga o vínculo dos dois lados, junto com o que ele "
          "mandou e você ainda não leu.", ap);
  }

  // --- rodape: onde estou e o que a tecla faz -------------------------------
  pontos(RI_TXT_X, RI_Y + dy + RI_H - 104.0f, a);
  { const char *dica = pagina < RI_PAGINAS - 1
        ? "OK ou → para continuar · Voltar fecha"
        : "OK para começar";
    TxtLinha t = txt_linha_corta(TXT_CAPTION2, i18n(dica),
                                 140, 144, 154, 255, RI_TXT_W);
    txt_desenhar_alpha(t, RI_TXT_X, RI_Y + dy + RI_H - 62.0f, a * 0.85f); }

  gfx_sem_recorte();
}
