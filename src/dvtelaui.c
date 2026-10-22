// O DESENHO DA TELA DO DOLBY VISION EM MKV (dvtela.h).
//
// O QUE A PESSOA VE. A arte do titulo tao desfocada que so fica a cor, um veu
// escuro e, no meio, um cartao de vidro (o mesmo material das ilhas e das
// novidades): a esquerda o que esta acontecendo, em passos que so acendem com
// o sinal real do backend; a direita uma ilustracao do que e Dolby Vision em
// MKV — a imagem HDR10 e, por cima, a camada do Dolby Vision. A camada de
// cima flutua separada enquanto o arquivo e lido, acende quando o perfil
// aparece no cabecalho e desce ate encaixar na de baixo conforme o caminho
// abre e a TV aceita o fluxo. Nada aqui anda por relogio: o relogio so faz a
// camada respirar e a varredura de leitura passar.
//
// CUSTO (C9, 60 fps): nenhuma passada de desfoque por quadro. O fundo e a
// copia 96x54 ja desfocada da arte (gfx_desfocado, gerada uma vez) esticada,
// um veu e uns vinte retangulos pequenos. Com Animacoes reduzidas tudo nasce
// no lugar (anim_mola* obedecem a politica) e a varredura fica parada.
#include "dvtela.h"
#include "ajustes.h"
#include "anim.h"
#include "botoes.h"
#include "gfx.h"
#include "idioma.h"
#include "layout.h"
#include "plrui.h"
#include "tex_cache.h"
#include "text.h"
#define NV_ESCALA_TELA_ATIVA
#include "escala.h"
#include "ponteiro.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define C_W        1320.0f
#define C_H         720.0f
#define C_RAIO       40.0f
#define C_PAD        64.0f
#define COL_W       700.0f       // coluna do texto
#define IL_W        420.0f       // a ilustracao
#define LIN_H        50.0f       // um passo
#define PLACA_W     340.0f
#define PLACA_H     118.0f

static char arteUrl[600];
static Uint32 ultQuadro;
#ifdef NV_SHOT_HOOKS
static Uint32 relogioFixo;
void dvtela_shot_relogio(Uint32 ms) { relogioFixo = ms; }
#endif

// As molas do desenho (independentes da maquina, que so diz o estado). Uma
// instancia e a da tela de verdade; a previa das novidades tem a propria.
typedef struct {
  float vao, vaoV;          // 1 = camada solta no alto, 0 = encaixada
  float luz, luzV;          // a camada do DV acesa (perfil achado)
  float linhaAudio;         // a linha do audio abrindo espaco
  float feito[DVT_PASSOS], feitoV[DVT_PASSOS];
} Molas;
static Molas molas = { .vao = 1.0f };
static Molas molasPrevia = { .vao = 1.0f };

void dvtela_definir_arte(const char *url) {
  snprintf(arteUrl, sizeof arteUrl, "%s", url ? url : "");
}

// "A_TRUEHD" -> "TrueHD": como o app chama o codec em todo lugar.
static const char *nomeCodec(const char *c) {
  if (!c || !*c) return "";
  if (!strcmp(c, "A_TRUEHD") || !strncmp(c, "A_MLP", 5)) return "TrueHD";
  if (!strcmp(c, "A_EAC3")) return "E-AC-3";
  if (!strcmp(c, "A_AC3")) return "AC-3";
  if (!strncmp(c, "A_DTS", 5)) return "DTS";
  if (!strncmp(c, "A_AAC", 5)) return "AAC";
  if (!strcmp(c, "A_FLAC")) return "FLAC";
  if (!strcmp(c, "A_OPUS")) return "Opus";
  return c[0] == 'A' && c[1] == '_' ? c + 2 : c;
}

// O vidro escuro das ilhas: miolo quase opaco e um fio claro na borda.
static void vidro(GfxRect r, float raioPx, float a) {
  gfx_cor(r, raioPx / r.h, 0.070f, 0.073f, 0.082f, 0.92f * a);
  gfx_anel(r, raioPx / r.h, 1.0f, 1, 1, 1, 0.09f * a);
}

// O FUNDO NUMA PASSADA SO, COM RUIDO (dono, C9 OLED, 08/10: "o gradiente do
// background ta daquele jeito"). Eram tres camadas translucidas escuras —
// chapado, a arte desfocada pelo GFX_CARD (que nao tem ruido) a 55% e um veu
// — cada uma quantizada em 8 bits por conta propria: o degrade escuro da arte
// saia em patamares de ate 166 px (tests/dvtela_shot.c, medirFaixas), que o
// OLED mostra como contorno. Agora a arte e o escurecimento saem do GFX_SNAP
// com o nv_dither ligado (uPar.x = 1, o mesmo do Frost): mix(arte, escuro,
// uFoco) e meio degrau de ruido antes de quantizar, numa passada de tela
// cheia so — a mesma conta que o chapado + arte*0,8*0,55 + veu de 38% davam
// no meio da tela. O degrade da base (veu, ja com ruido) fica por cima.
// A copia desfocada (96x54) ja e 16:9 como a tela; o GFX_SNAP a estica inteira.
static void fundo(float a) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  GLuint tb = 0;
  if (arteUrl[0]) {
    GLuint t = tex_obter_larg(arteUrl, 480);
    if (t) tb = gfx_desfocado(t, arteUrl);
  }
  // Opaco desde o primeiro quadro: atras pode estar o HDR10 do player da TV.
  if (tb) gfx_rect(tela, tb, GFX_SNAP, 0.727f, 2.0f, 0.0f, 0.0f, 0.0104f, 0.0111f, 0.0138f, a);
  else gfx_cor(tela, 0.0f, 0.016f, 0.017f, 0.021f, a);
  // Escurece a base: o cartao fica numa poca de luz.
  gfx_veu_css(tela, 0, 0.0f, 1.0f, 0.35f * a);
}

// --------------------------------------------------------------- a ilustracao
// Duas placas, a imagem HDR10 embaixo e a camada do Dolby Vision em cima.
static void placa(GfxRect r, float cr, float cg, float cb, float fill, float borda, float a) {
  gfx_cor(r, 22.0f / r.h, cr, cg, cb, fill * a);
  gfx_anel(r, 22.0f / r.h, 1.5f, 1, 1, 1, borda * a);
}

static void ilustracao(const DvtelaEstado *e, const Molas *m, float x, float y, float w, float h, Uint32 agora, float a) {
  int red = ajustes_animacoes_reduzidas();
  const float vao = m->vao, luz = m->luz;
  float ar, ag, ab, cx = x + w * 0.5f;
  float respira = red ? 0.0f : sinf((float)(agora % 4000u) / 4000.0f * 6.2831853f);
  float yBase = y + h * 0.5f + 34.0f;            // topo da placa de baixo
  float gap = 18.0f + 92.0f * vao;               // vao entre as placas
  GfxRect baixo = { cx - PLACA_W * 0.5f, yBase, PLACA_W, PLACA_H };
  GfxRect cima = { cx - PLACA_W * 0.5f, yBase - PLACA_H - gap + respira * 4.0f * vao,
                   PLACA_W, PLACA_H };
  ajustes_acento(&ar, &ag, &ab);

  // A placa de baixo: a imagem que a TV ja sabe tocar.
  gfx_rect((GfxRect){ baixo.x - 30, baixo.y - 6, baixo.w + 60, baixo.h + 54 }, 0, GFX_SOMBRA,
           1.0f, 0, 0, 0.16f, 0, 0, 0, 0.55f * a);
  placa(baixo, 0.17f, 0.18f, 0.21f, 1.0f, 0.10f, a);
  { TxtLinha l = txt_linha(TXT_V3_SELO, "HDR10", 243, 242, 239, 200);
    txt_desenhar_alpha(l, baixo.x + 24.0f, baixo.y + 22.0f, a); }
  // Tres "linhas de imagem" apagadas: e uma imagem, nao um botao.
  { int i;
    for (i = 0; i < 3; i++)
      gfx_cor((GfxRect){ baixo.x + 24.0f, baixo.y + 58.0f + i * 14.0f, (baixo.w - 48.0f) * (i == 2 ? 0.55f : 1.0f), 6.0f },
              0.5f, 1, 1, 1, 0.08f * a);
  }
  // LENDO O ARQUIVO: uma varredura passa pela placa de baixo (o cabecalho e o
  // inicio dela). So enquanto o passo e esse; parada com animacoes reduzidas.
  if (e->ativa && e->passo == DVT_PASSO_LER) {
    float t = red ? 0.35f : (float)(agora % 1800u) / 1800.0f;
    float sx = baixo.x + 14.0f + (baixo.w - 28.0f) * anim_suave(t);
    gfx_cor((GfxRect){ sx - 1.5f, baixo.y + 12.0f, 3.0f, baixo.h - 24.0f }, 0.5f, ar, ag, ab, 0.85f * a);
    gfx_rect((GfxRect){ sx - 26.0f, baixo.y + 4.0f, 52.0f, baixo.h - 8.0f }, 0, GFX_SOMBRA,
             1.0f, 0, 0, 0.5f, ar, ag, ab, 0.30f * a);
  }

  // A camada do Dolby Vision: contorno enquanto nao se sabe, acesa no acento
  // quando o perfil aparece no arquivo.
  if (luz > 0.01f)
    gfx_rect((GfxRect){ cima.x - 50, cima.y - 40, cima.w + 100, cima.h + 80 }, 0, GFX_SOMBRA,
             1.0f, 0, 0, 0.5f, ar, ag, ab, 0.32f * luz * a);
  gfx_rect((GfxRect){ cima.x - 24, cima.y - 2, cima.w + 48, cima.h + 40 }, 0, GFX_SOMBRA,
           1.0f, 0, 0, 0.16f, 0, 0, 0, 0.45f * a);
  placa(cima, 0.12f + (ar - 0.12f) * 0.55f * luz, 0.13f + (ag - 0.13f) * 0.55f * luz,
        0.16f + (ab - 0.16f) * 0.55f * luz, 0.55f + 0.40f * luz, 0.14f + 0.16f * luz, a);
  { TxtLinha l = txt_linha(TXT_V3_SELO, "DOLBY VISION", 243, 242, 239, 255);
    txt_desenhar_alpha(l, cima.x + 24.0f, cima.y + 22.0f, (0.55f + 0.45f * luz) * a); }
  if (e->perfil > 0) {
    char p[24];
    TxtLinha l;
    snprintf(p, sizeof p, "P%d", e->perfil);
    l = txt_linha(TXT_V3_SELO, p, 243, 242, 239, 255);
    txt_desenhar_alpha(l, cima.x + cima.w - 24.0f - (float)l.w, cima.y + 22.0f, luz * a);
  }
  // Os "metadados de cena": tracos curtos de alturas diferentes, como um
  // grafico de brilho por cena — e o que a camada carrega por cima da imagem.
  { static const float ALT[12] = { .35f, .6f, .45f, .8f, .55f, .3f, .7f, .9f, .5f, .4f, .65f, .5f };
    int i;
    float bw = (cima.w - 48.0f) / 12.0f;
    for (i = 0; i < 12; i++) {
      float hh = 34.0f * ALT[i];
      gfx_cor((GfxRect){ cima.x + 24.0f + i * bw + 3.0f, cima.y + cima.h - 20.0f - hh, bw - 6.0f, hh },
              0.3f, 1, 1, 1, (0.10f + 0.45f * luz) * a);
    } }
  // O ENCAIXE: a ponte de luz entre as duas, que cresce conforme o vao fecha.
  if (vao < 0.6f) {
    float k = anim_clamp((0.6f - vao) / 0.6f, 0.0f, 1.0f);
    gfx_cor((GfxRect){ cx - (PLACA_W * 0.38f) * k, cima.y + cima.h + gap * 0.5f - 1.5f,
                       PLACA_W * 0.76f * k, 3.0f }, 0.5f, ar, ag, ab, 0.7f * k * a);
  }
}

// ------------------------------------------------------------------- os passos
typedef struct { const char *icone; char rot[96]; int feito, atual, visivel; } Linha;

static int montarLinhas(const DvtelaEstado *e, Linha L[DVT_PASSOS]) {
  int i, atual = -1;
  memset(L, 0, sizeof(Linha) * DVT_PASSOS);
  L[DVT_PASSO_LER].icone = "aj_file-text";
  snprintf(L[DVT_PASSO_LER].rot, sizeof L[0].rot, "%s", i18n("Lendo o arquivo"));
  L[DVT_PASSO_LER].feito = e->passo >= DVT_PASSO_ACHOU;
  L[DVT_PASSO_ACHOU].icone = "aj_sparkles";
  if (e->perfil > 0) snprintf(L[DVT_PASSO_ACHOU].rot, sizeof L[0].rot, i18n("Dolby Vision perfil %d"), e->perfil);
  else snprintf(L[DVT_PASSO_ACHOU].rot, sizeof L[0].rot, "%s", i18n("Procurando o Dolby Vision"));
  L[DVT_PASSO_ACHOU].feito = e->perfil > 0;
  L[DVT_PASSO_AUDIO].icone = "aj_audio-lines";
  if (e->audioTrocado)
    snprintf(L[DVT_PASSO_AUDIO].rot, sizeof L[0].rot, i18n("Áudio %s trocado por %s"),
             nomeCodec(e->audioDe), nomeCodec(e->audioPara));
  L[DVT_PASSO_AUDIO].feito = 1;
  L[DVT_PASSO_ABRIR].icone = "aj_download";
  snprintf(L[DVT_PASSO_ABRIR].rot, sizeof L[0].rot, "%s", i18n("Abrindo a fonte"));
  L[DVT_PASSO_ABRIR].feito = e->passo >= DVT_PASSO_IMAGEM;
  L[DVT_PASSO_IMAGEM].icone = "aj_image";
  snprintf(L[DVT_PASSO_IMAGEM].rot, sizeof L[0].rot, "%s", i18n("Preparando a imagem"));
  L[DVT_PASSO_IMAGEM].feito = 0;
  for (i = 0; i < DVT_PASSOS; i++) {
    L[i].visivel = i != DVT_PASSO_AUDIO || e->audioTrocado;
    if (L[i].visivel && !L[i].feito && atual < 0) atual = i;
  }
  if (atual >= 0) L[atual].atual = 1;
  return atual;
}

static void passos(const DvtelaEstado *e, const Molas *m, float x, float y, float w, Uint32 agora, float a) {
  Linha L[DVT_PASSOS];
  float ar, ag, ab, yy = y;
  int i;
  ajustes_acento(&ar, &ag, &ab);
  montarLinhas(e, L);
  for (i = 0; i < DVT_PASSOS; i++) {
    float vis = i == DVT_PASSO_AUDIO ? m->linhaAudio : (L[i].visivel ? 1.0f : 0.0f);
    float cy = yy + LIN_H * 0.5f, f = m->feito[i];
    if (vis <= 0.01f) continue;
    // O MARCADOR: anel apagado (vem), ponto que respira (agora), visto no
    // acento que cresce com a mola (feito).
    if (L[i].atual) {
      plrui_respira(x + 14.0f, cy, 14.0f, agora, a * vis);
    } else if (f > 0.02f) {
      float d = 30.0f * (0.6f + 0.4f * f);
      gfx_cor((GfxRect){ x + 14.0f - d * 0.5f, cy - d * 0.5f, d, d }, 0.5f, ar, ag, ab, 0.22f * f * a * vis);
      gfx_icone((GfxRect){ x + 14.0f - 9.0f * f, cy - 9.0f * f, 18.0f * f, 18.0f * f }, "aj_check",
                ar, ag, ab, f * a * vis);
    } else {
      gfx_anel((GfxRect){ x + 5.0f, cy - 9.0f, 18.0f, 18.0f }, 0.5f, 1.5f, 1, 1, 1, 0.25f * a * vis);
    }
    { int alfaTxt = L[i].atual ? 255 : f > 0.5f ? 200 : 105;
      TxtLinha l = txt_linha_corta(L[i].atual ? TXT_V3_LN_B : TXT_V3_LN, L[i].rot, 243, 242, 239, alfaTxt,
                                   w - 96.0f);
      gfx_icone((GfxRect){ x + 46.0f, cy - 11.0f, 22.0f, 22.0f }, L[i].icone, 0.953f, 0.949f, 0.937f,
                ((float)alfaTxt / 255.0f) * 0.85f * a * vis);
      txt_desenhar_alpha(l, x + 84.0f, cy - (float)l.h * 0.5f, a * vis); }
    yy += LIN_H * vis;
  }
}

// PONTEIRO (#99). A tela tem o teclado (dvtela_evento engole tudo) e o unico
// foco e o botao, entao o foco nunca muda: o hover nao faz nada. Por isso ela
// NAO pode ser uma camada sem alvos (ali qualquer clique vira OK = HDR10): um
// anteparo de tela cheia absorve o clique fora do botao, e so o clique no
// proprio botao entrega o OK (com a mesma espera de DVT_OK_ESPERA_MS do
// controle). So com a tela de pe e assentada.
static void ponteiroBotao(int a, int b) { (void)a; (void)b; }
static int dvtPont;
int dvtela_teste_ponteiro(void) { return dvtPont; }

// ------------------------------------------------------------------ o cartao
// As molas seguem o estado real (a mola2 parte macia e nao passa do alvo).
static void molasSeguir(Molas *m, const DvtelaEstado *e, float dt) {
  int i;
  float alvoVao = e->passo >= DVT_PASSO_IMAGEM ? 0.0f : e->passo >= DVT_PASSO_ABRIR ? 0.45f : 1.0f;
  Linha L[DVT_PASSOS];
  if (!e->ativa && e->saida == DVT_SAIDA_DV) alvoVao = 0.0f;
  m->vao = anim_mola2(&m->vaoV, m->vao, alvoVao, dt, 6.5f);
  m->luz = anim_mola2(&m->luzV, m->luz, e->perfil > 0 ? 1.0f : 0.0f, dt, 8.0f);
  m->linhaAudio = anim_mola(m->linhaAudio, e->audioTrocado ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  montarLinhas(e, L);
  for (i = 0; i < DVT_PASSOS; i++)
    m->feito[i] = anim_mola2(&m->feitoV[i], m->feito[i], L[i].feito ? 1.0f : 0.0f, dt, 14.0f);
}

static void molasZerar(Molas *m) {
  memset(m, 0, sizeof *m);
  m->vao = 1.0f;
}

// O cartao inteiro (vidro, texto, passos, botao e a ilustracao) com o canto de
// cima a esquerda em (cx0, cy0), em 1320 x 720. Sem o fundo e sem ponteiro:
// quem chama decide. `dica` e o alfa da dica calma, `foco` o botao em foco.
static void cartao(const DvtelaEstado *e, const Molas *m, Uint32 agora, float a, float cx0, float cy0,
                   float dica, int foco, int comPonteiro) {
  GfxRect c = { cx0, cy0, C_W, C_H };
  float tx = cx0 + C_PAD, ty = cy0 + C_PAD;
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  gfx_sombra_sob((GfxRect){ c.x - 60, c.y - 20, c.w + 120, c.h + 110 }, 1.0f, 0, 0.10f, 0, 0, 0,
                 0.55f * a, c, C_RAIO, 0.92f * a);
  vidro(c, C_RAIO, a);
  gfx_luz_canto(c, C_RAIO / c.h, c.w * 0.82f, -c.h * 0.10f, c.h * 1.1f, ar, ag, ab, 0.10f * a);

  ajustes_ui_kicker(i18n("Dolby Vision em MKV"), tx, ty, a);
  { TxtLinha l = txt_linha_corta(TXT_NOV_TITULO, i18n("Ligando o Dolby Vision"), 243, 242, 239, 255, COL_W);
    txt_desenhar_alpha(l, tx, ty + 30.0f, a); }
  txt_bloco_corta(TXT_V3_SUB, i18n("Esta TV abre MKV em HDR10. Para ter Dolby Vision, o app lê o arquivo e "
                                   "entrega a imagem à TV. Leva alguns segundos."),
                  243, 242, 239, tx, ty + 104.0f, COL_W - 40.0f, 33.0f, 0.64f * a, 3);

  passos(e, m, tx, ty + 232.0f, COL_W, agora, a);

  // A DICA CALMA (60 s sem mudanca): em ambar, embaixo da ilustracao, na
  // altura do botao que ela menciona. Nao e erro: nada pisca, nada fica
  // vermelho.
  if (dica > 0.01f) {
    float xd = cx0 + C_W - C_PAD - IL_W, yd = cy0 + C_H - C_PAD - BOTAO_H_SECUNDARIO - 4.0f + (1.0f - dica) * 6.0f;
    gfx_icone((GfxRect){ xd, yd + 4.0f, 22.0f, 22.0f }, "aj_clock", 1.0f, 0.77f, 0.35f, dica * a);
    txt_bloco_corta(TXT_V3_SUB, i18n("A fonte está lenta. Dá para esperar ou assistir em HDR10."),
                    255, 196, 90, xd + 34.0f, yd, IL_W - 34.0f, 30.0f, 0.92f * dica * a, 2);
  }

  // O botao (o unico foco) e a dica do Voltar.
  { const char *rot = i18n("Assistir agora em HDR10");
    float bw = botao_largura(rot, "aj_tv-minimal-play", 0);
    GfxRect b = { tx, cy0 + C_H - C_PAD - BOTAO_H_SECUNDARIO, bw, BOTAO_H_SECUNDARIO };
    const char *k[1] = { "Voltar" }, *r[1] = { "Sair" };
    if (comPonteiro) ponteiro_alvo(b.x, b.y, b.w, b.h, ponteiroBotao, NULL, 0, 0);
    botao_pilula(b, rot, "aj_tv-minimal-play", foco ? 1.0f : 0.0f, 0, 0, a);
    plrui_dicas(k, r, 1, b.x + b.w + 28.0f, b.y + b.h * 0.5f, 0, a * 0.85f); }

  ilustracao(e, m, cx0 + C_W - C_PAD - IL_W, cy0 + C_PAD, IL_W, C_H - 2.0f * C_PAD - 70.0f, agora, a);
}

void dvtela_desenhar(Uint32 agora) {
  const DvtelaEstado *e = dvtela_estado();
  float a = dvtela_alfa(), dt;
  if (!dvtela_visivel() || a <= 0.002f) { ultQuadro = 0; return; }
#ifdef NV_SHOT_HOOKS
  if (relogioFixo) agora = relogioFixo;
#endif
  dt = ultQuadro ? (float)(agora - ultQuadro) / 1000.0f : 1.0f / 60.0f;
  if (dt > 0.1f || dt < 0.0f) dt = 1.0f / 60.0f;
  ultQuadro = agora;
  molasSeguir(&molas, e, dt);
  if (e->ativa && e->passo == DVT_PASSO_LER && e->perfil == 0 && agora - e->entrouEm < 50u) {
    // Tela nova: as molas comecam do comeco (a anterior pode ter acabado encaixada).
    molasZerar(&molas);
  }

  fundo(a);
  dvtPont = 0;
  if (e->ativa) {
    ponteiro_camada();   // o player por baixo nao recebe o ponteiro
    if (a >= 0.999f) {
      ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, NULL, 0, 0);   // anteparo
      dvtPont = 1;
    }
  }
  { ESCALA_SE_COUBER_INI(C_W, C_H);
    float sobe = (1.0f - a) * 18.0f;
    cartao(e, &molas, agora, a, (NV_TELA_W - C_W) * 0.5f, (NV_TELA_H - C_H) * 0.5f + sobe,
           dvtela_dica_alfa(), dvtela_foco_botao(), dvtPont);
    ESCALA_SE_COUBER_FIM(); }
}

// A PREVIA DAS NOVIDADES (novidades_cartao.h): o MESMO cartao, desenhado por
// esta funcao, a `s` do tamanho com o canto em (x, y). O estado vem da maquina
// de verdade (dvt_passo) e as molas sao as daqui, so que de uma instancia a
// parte (dvtela_previa_avancar as anda): a tela do player nao e tocada. Sem
// fundo, sem ponteiro, sem dica.
void dvtela_previa_zerar(void) { molasZerar(&molasPrevia); }

void dvtela_previa_avancar(const DvtelaEstado *e, float dt) { molasSeguir(&molasPrevia, e, dt); }

void dvtela_previa_desenhar(float x, float y, float s, const DvtelaEstado *e, Uint32 agora, float a) {
  gfx_transformar(x, y, s, 0.0f, 0.0f);
  cartao(e, &molasPrevia, agora, a, x, y, 0.0f, 1, 0);
  gfx_sem_transformar();
}

float dvtela_previa_largura(void) { return C_W; }
float dvtela_previa_altura(void) { return C_H; }
