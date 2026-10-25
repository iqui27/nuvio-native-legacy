// COMEÇAR. Ver comecar.h.
//
// O FOCO E GEOMETRICO: cada passo monta uma lista de retangulos (montar) e
// CIMA/BAIXO/ESQUERDA/DIREITA vao para o mais proximo na direcao. O desenho, o
// foco por teclado e os alvos do ponteiro usam a mesma lista, entao nenhum dos
// tres desencontra. Nada depende de medir texto antes de existir.
#include "comecar.h"
#include "comecarregra.h"
#include "ajustes.h"
#include "anim.h"
#include "revela.h"
#include "teclavoltar.h"
#include "botoes.h"
#include "dados.h"
#include "gfx.h"
#include "idioma.h"
#include "importarfluxo.h"
#include "celbotao.h"
#include "layout.h"
#include "catalogo.h"
#include "colecoes.h"
#include "descoberta.h"
#include "addons.h"
#include "idiomacod.h"
#include "linguas.h"
#include "perfilinfantil.h"
#include "perfis.h"
#include "plrui.h"
#include "tex_cache.h"
#include "ponteiro.h"
#include "salvos.h"
#include "simkl.h"
#include "simklauth.h"
#include "teclado.h"
#include "text.h"
#include "trakt.h"
#include "traktauth.h"
#define NV_TXT_TELA
#include "tela21.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARGEM      120.0f
#define PASSOS        4
#define MAX_ITENS    20
#define BACK_LONGO_MS 700   // Voltar segurado por tanto tempo pula o fluxo todo
#define BACK_DEBOUNCE_MS 250

// Ids de foco. Os de campo valem dentro do passo; os botoes valem em todos.
enum { I_NOME = 0, I_COR0 = 1, I_INFANTIL = 7, I_IDIOMA = 8 };       // passo 0
enum { I_STREMIO = 0, I_AIO, I_NUVIO, I_ZERO };                        // passo 1
enum { I_TRAKT = 0, I_SIMKL, I_TV, I_CH_CW, I_CH_SALVA, I_CH_HIST }; // passo 2
enum { B_VOLTAR = 100, B_PRINC, B_SEC, B_PULAR };

typedef struct { int id; GfxRect r; } Item;

static int   aberto, passo, foco = B_PRINC, pedido, esperaTeclado, decidido, temDados;
static Uint32 backDesde, backUltimo;   // relogio proprio: main.c zera o evento LG
static int backPressionado, backVisto;
static float entrada, pagina, tempoPasso, focoAnim[B_PULAR + 1];
static GfxMini paginaAnterior;
static char ultimoCodigo[24], ultimoLink[200];
static ComecarSalvo dados;   // o que a pessoa esta preenchendo (so vai ao disco no fim)
static ComecarSalvo antes;   // o que ja estava gravado quando a tela abriu

// Profile identity colors; never used for navigation focus.
static const SDL_Color CORES[COMECAR_CORES] = {
  {124,92,255,255}, {224,86,91,255}, {0,168,143,255},
  {240,160,48,255}, {61,139,255,255}, {224,90,160,255}
};

int comecar_aberto(void) { return aberto; }
int comecar_passo(void) { return passo; }
int comecar_foco(void) { return foco; }
void comecar_teste_esquecer(void) {
  aberto = 0; entrada = 0.0f; decidido = 0; temDados = 0;
  backPressionado = backVisto = 0;
  memset(&dados, 0, sizeof dados);
  memset(&antes, 0, sizeof antes);
}

int comecar_pedido(void) {
  int p = pedido;
  pedido = COMECAR_PEDIU_NADA;
  return p;
}

// ------------------------------------------------------------------ gravacao
// So duas coisas gravam comecar.txt: concluir (o resumo) e pular. Nada de
// "andamento" nem de gravar a cada escolha: fechar o app no meio nao deixa rastro.
static void gravar_feito(void) {
  char buf[256];
  dados.estado = COMECAR_ESTADO_FEITO;
  if (comecarregra_serializar(&dados, buf, sizeof buf)) dados_gravar(COMECAR_ARQ, buf);
}

static int existe_dado(const char *arq) {
  char *s = dados_ler(arq);
  int r = s && s[0];
  free(s);
  return r;
}

// Chamada uma vez no arranque, ANTES do login e do sync: eles criam estes
// arquivos, e ai nao daria mais para distinguir uma instalacao que vem da 2.0.x
// de uma limpa.
void comecar_marcar_dados(void) {
  temDados = existe_dado("sessao.txt") || existe_dado("perfis.txt") || existe_dado("progresso.txt");
}

static void carregar(void) {
  char *s = dados_ler(COMECAR_ARQ);
  comecarregra_ler(s, &dados);
  free(s);
}

// ------------------------------------------------------------------- estado
static int foco_padrao(void) {
  if (passo == 0) return dados.nome[0] ? B_PRINC : I_NOME;
  if (passo == 1) return I_NUVIO;
  if (passo == 2) return I_TRAKT;
  return B_PRINC;
}

static void ir_passo(int p) {
  if (p != passo && entrada > .99f && !gfx_mini_ativo() &&
      !ajustes_animacoes_reduzidas() && gfx_mini_alvo(&paginaAnterior, 1920, 1080)) {
    gfx_mini_comecar(&paginaAnterior, 0, 0, 1);
    comecar_desenhar(SDL_GetTicks());
    gfx_mini_terminar();
  }
  if (p != passo) importarfluxo_cancelar();
  passo = p < 0 ? 0 : p >= PASSOS ? PASSOS - 1 : p;
  foco = foco_padrao();
  pagina = tempoPasso = 0;
  memset(focoAnim, 0, sizeof focoAnim);
}

static void abrir(void) {
  aberto = 1;
  gfx_mini_liberar(&paginaAnterior);
  ultimoCodigo[0] = ultimoLink[0] = 0;
  entrada = pagina = tempoPasso = 0.0f;
  memset(focoAnim, 0, sizeof focoAnim);
  pedido = COMECAR_PEDIU_NADA;
  esperaTeclado = 0;
  backPressionado = backVisto = 0;
  antes = dados;
  passo = 0;
  foco = foco_padrao();
}

void comecar_abrir(void) {
  carregar();
  dados.estado = COMECAR_ESTADO_NENHUM;
  abrir();
}

void comecar_decidir(int loginNovo) {
  int o;
  if (decidido) return;   // uma vez por execucao, com a home de pe (app.c)
  decidido = 1;
  carregar();
  o = comecarregra_decidir(&dados, loginNovo, temDados);
  if (o == COMECAR_COMPLETO) abrir();
}

static void concluir(int oQue) {
  aberto = 0;
  pedido = oQue;
  gravar_feito();
}

// Pular: marca como feito e nada mais. O que foi digitado nesta passada se
// perde; o que ja estava gravado (reabrir por Ajustes) fica como estava.
static void pular(void) {
  aberto = 0;
  pedido = COMECAR_PEDIU_NADA;
  dados = antes;
  gravar_feito();
}

// ------------------------------------------------------------------- rotulos
static const char *rotulo_botao(int id) {
  if (id == B_PULAR) return i18n("Pular tudo");
  switch (passo) {
    case 0: return id == B_VOLTAR ? i18n("Voltar") : i18n("Continuar");
    case 1: return id == B_VOLTAR ? i18n("Voltar") :
      i18n(importarfluxo_estado() == IMPF_OK ? "Continuar" : "Pular este passo");
    case 2: return id == B_VOLTAR ? i18n("Voltar") : i18n("Continuar");
    default: return id == B_PRINC ? i18n("Ir para a Home") : i18n("Fazer o tour · 1 min");
  }
}

static const char *botao_icone(int id) {
  if (passo != PASSOS - 1 || id == B_PULAR) return NULL;
  return id == B_PRINC ? "aj_house" : "aj_sparkles";
}

// Defaults filled for the person (mock 3C): the first two chips step the real
// setting; "Importar o que já foi visto" is on while a tracker is linked (its
// watched history feeds the catalogue) and OK pulls it again right now.
static int op_chip(int id) {
  return ajustes_rapido_op(id == I_CH_CW ? "cwFonteLocal" : "salvosDestino");
}

// ------------------------------------------------------------------- layout
// Mock 2.1 (design/2.1/comecar.html): 120 px margins, title at 200, footer
// 72 px from the bottom. Anchors are REAL px (ANC); components grow with text.
static float ox, oy, largura, coluna, lateral;

static void origem(void) {
  t21_inicio(&ox, &largura);
  ox -= MARGEM;
  oy = 0;
  coluna = fminf(860.0f, (largura - T21_GAP_BIG) * 0.55f);
  lateral = largura - coluna - T21_GAP_BIG;
}

static float rodape_topo(void) {
  return ANC(T21_SAFE_B) - BOTAO_H_PRIMARIO;
}

// Body starts under title + subtitle; the subtitle never meets the first row.
static float titulo_y(void) { return ANC(170.0f); }
static float corpo_y(void)  { return titulo_y() + 64.0f + 2.0f * 34.0f + 34.0f; }

// Step 1 stacks field, colours and two rows; the gap shrinks before anything leaves the safe area.
static float p0_vao(void) {
  float livre = rodape_topo() - T21_GAP_CARD - corpo_y() - (84.0f + 72.0f + 2.0f * 108.0f + 10.0f);
  return fminf(34.0f, fmaxf(12.0f, livre / 2.0f));
}

static const char *rotulo_chip(int id, char *buf, size_t tam);

// Chip lines once wrapped to the content width (1 at 100%, more at large text).
static int linhas_chips(void) {
  char b[160];
  float x = 0;
  int l = 1;
  for (int i = I_CH_CW; i <= I_CH_HIST; i++) {
    float cw = fminf(largura, botao_largura(rotulo_chip(i, b, sizeof b), i == I_CH_HIST ? "aj_check" : NULL, 0));
    if (x > 0 && x + cw > largura) { l++; x = 0; }
    x += cw + 14.0f;
  }
  return l;
}

static float tracker_altura(void) {
  float livre = rodape_topo() - T21_GAP_CARD - corpo_y() - (46.0f + 28.0f + linhas_chips() * (BOTAO_H_SECUNDARIO + 14.0f) - 14.0f);
  return fminf(330.0f, livre);
}

static void poe(Item *it, int *n, int id, float x, float y, float w, float h) {
  if (*n >= MAX_ITENS) return;
  it[*n].id = id;
  it[*n].r = (GfxRect){ ox + x, oy + y, w, h };
  (*n)++;
}

// Rodape: os botoes da esquerda para a direita; o ultimo e o primario, salvo
// no resumo, onde o primario vem primeiro.
static void rodape(Item *it, int *n) {
  float x = MARGEM, yc = ANC(T21_SAFE_B) - BOTAO_H_PRIMARIO * 0.5f;
  int ids[3], k = 0, i;
  if (passo == PASSOS - 1) { ids[k++] = B_PRINC; ids[k++] = B_SEC; }
  else {
    ids[k++] = B_VOLTAR;   // on step 1 it leaves, like the Back key there
    ids[k++] = B_PRINC;
  }
  for (i = 0; i < k; i++) {
    int prim = ids[i] == B_PRINC;
    float w = botao_largura(rotulo_botao(ids[i]), botao_icone(ids[i]), prim);
    float h = prim ? BOTAO_H_PRIMARIO : BOTAO_H_SECUNDARIO;
    poe(it, n, ids[i], x, yc - h * 0.5f, w, h);
    x += w + BOTAO_GAP;
  }
  if (passo < PASSOS - 1) {
    float w = botao_largura(rotulo_botao(B_PULAR), NULL, 0);
    poe(it, n, B_PULAR, MARGEM + largura - w, ANC(T21_SAFE_T), w, BOTAO_H_SECUNDARIO);
  }
}

static int montar(Item *it) {
  int n = 0, i;
  origem();
  if (passo == 0) {
    float y = corpo_y(), v = p0_vao();
    poe(it, &n, I_NOME, MARGEM, y, coluna, 84.0f);
    y += 84.0f + v;
    for (i = 0; i < COMECAR_CORES; i++) poe(it, &n, I_COR0 + i, MARGEM + (float)i * 90.0f, y, 72.0f, 72.0f);
    y += 72.0f + v;
    poe(it, &n, I_INFANTIL, MARGEM, y, coluna, 108.0f);
    poe(it, &n, I_IDIOMA, MARGEM, y + 118.0f, coluna, 108.0f);
  } else if (passo == 1) {
    float h = fminf(108.0f, (rodape_topo() - T21_GAP_CARD - corpo_y() - 30.0f) / 4.0f);
    for (i = 0; i < 4; i++) poe(it, &n, i, MARGEM, corpo_y() + (float)i * h, coluna, h - 10.0f);
  } else if (passo == 2) {
    float w = (largura - 2.0f * T21_GAP_CARD) / 3.0f, th = tracker_altura(), x = MARGEM;
    float cy = corpo_y() + th + 46.0f + 28.0f;
    char b[160];
    for (i = 0; i < 3; i++) poe(it, &n, i, MARGEM + (float)i * (w + T21_GAP_CARD), corpo_y(), w, th);
    for (i = I_CH_CW; i <= I_CH_HIST; i++) {
      float cw = fminf(largura, botao_largura(rotulo_chip(i, b, sizeof b), i == I_CH_HIST ? "aj_check" : NULL, 0));
      if (x > MARGEM && x + cw > MARGEM + largura) { x = MARGEM; cy += BOTAO_H_SECUNDARIO + 14.0f; }   // wrap at large text
      poe(it, &n, i, x, cy, cw, BOTAO_H_SECUNDARIO);
      x += cw + 14.0f;
    }
  }
  rodape(it, &n);
  return n;
}

// ----------------------------------------------------------- foco geometrico
static void mover(int dx, int dy) {
  Item it[MAX_ITENS];
  int n = montar(it), i, cur = -1, melhor = -1;
  float cx, cy, bs = 1e9f;
  for (i = 0; i < n; i++) if (it[i].id == foco) cur = i;
  if (cur < 0) { foco = foco_padrao(); return; }
  cx = it[cur].r.x + it[cur].r.w * 0.5f;
  cy = it[cur].r.y + it[cur].r.h * 0.5f;
  for (i = 0; i < n; i++) {
    float ax, ay, along, cross, s;
    if (i == cur) continue;
    ax = it[i].r.x + it[i].r.w * 0.5f - cx;
    ay = it[i].r.y + it[i].r.h * 0.5f - cy;
    along = ax * (float)dx + ay * (float)dy;
    cross = dx ? fabsf(ay) : fabsf(ax);
    if (along < 1.0f) continue;
    s = along + 3.0f * cross;
    if (s < bs) { bs = s; melhor = i; }
  }
  if (melhor >= 0) foco = it[melhor].id;
}

// A profile with a PIN changes the kids mark only in Settings, behind the PIN.
static int infantil_travado(void) {
  const ContaPerfil *p = perfis_item_ativo();
  return p && p->temPin;
}

static void abrir_teclado(void) {
  teclado_abrir_com(i18n("Seu nome"), i18n("Só o nome pede ação; o resto já vem preenchido."), COMECAR_NOME_MAX,
                    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -", dados.nome);
  esperaTeclado = 1;
  backPressionado = backVisto = 0;
}

static void ok(void) {
  if (foco == B_VOLTAR) { if (passo == 0) pular(); else ir_passo(passo - 1); return; }
  if (foco == B_PULAR) { pular(); return; }
  if (foco == B_PRINC || foco == B_SEC) {
    if (passo == 0) { if (!dados.nome[0]) abrir_teclado(); else ir_passo(1); }
    else if (passo < PASSOS - 1) ir_passo(passo + 1);
    else concluir(foco == B_SEC ? COMECAR_PEDIU_TOUR : COMECAR_PEDIU_NADA);
    return;
  }
  if (passo == 0) {
    if (foco == I_NOME) abrir_teclado();
    else if (foco >= I_COR0 && foco < I_COR0 + COMECAR_CORES) dados.cor = foco - I_COR0;
    else if (foco == I_INFANTIL && !infantil_travado()) perfil_infantil_definir(perfis_ativo(), !perfil_infantil_ativo());
    else if (foco == I_IDIOMA) ajustes_idioma_passo(1);
  } else if (passo == 1) {
    if (foco == I_NUVIO || foco == I_ZERO) ir_passo(2);
    else if (foco == I_STREMIO) importarfluxo_comecar(IMPF_STREMIO);
    else if (foco == I_AIO) importarfluxo_comecar(IMPF_AIOM);
  } else if (passo == 2) {
    if (foco == I_TRAKT && !trakt_ativo()) traktauth_comecar();
    else if (foco == I_SIMKL && !simkl_ativo()) simklauth_comecar();
    else if (foco == I_TV) ir_passo(3);
    else if (foco == I_CH_HIST) { if (trakt_ativo() || simkl_ativo()) desc_refazer_continuar(); else traktauth_comecar(); }
    else if (foco >= I_CH_CW) ajustes_rapido_passo(op_chip(foco), 1);
  }
}

static void comecar_evento_(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto) return;
  // O teclado da TV e modal: com ele aberto, toda tecla e dele (digitar,
  // Voltar cancela, Pronto confirma). Quem le o resultado e comecar_atualizar.
  if (teclado_aberto()) { teclado_evento(e); return; }
  if (e->type != SDL_KEYDOWN && e->type != SDL_KEYUP) return;
  k = e->key.keysym.sym;
  if (nv_tecla_voltar(e)) {
    Uint32 agora = SDL_GetTicks();
    if (e->type == SDL_KEYUP) { backPressionado = 0; return; }
    // O Back LG traduzido nao tem repeat/timestamp. Descida volta uma vez;
    // rajadas apos soltar ficam no debounce, e segurar usa nosso relogio.
    if (!backPressionado) {
      if (backVisto && agora - backUltimo < BACK_DEBOUNCE_MS) {
        backUltimo = agora;
        return;
      }
      backPressionado = backVisto = 1;
      backDesde = agora;
      if (passo == 0) pular();
      else ir_passo(passo - 1);
    } else if (agora - backDesde >= BACK_LONGO_MS) pular();
    backUltimo = agora;
    return;
  }
  if (e->type != SDL_KEYDOWN) return;
  if (k == SDLK_LEFT)  { mover(-1, 0); return; }
  if (k == SDLK_RIGHT) { mover(1, 0); return; }
  if (k == SDLK_UP)    { mover(0, -1); return; }
  if (k == SDLK_DOWN)  { mover(0, 1); return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) { ok(); return; }
}

void comecar_atualizar(float dt, Uint32 agora) {
  int red = ajustes_animacoes_reduzidas();
  if (aberto) importarfluxo_passo();
  if (esperaTeclado) {
    int r;
    if (teclado_aberto()) teclado_atualizar(dt, agora);
    r = teclado_resultado();
    if (r == TECLADO_PRONTO) {
      comecarregra_nome_limpo(teclado_texto(), dados.nome, sizeof dados.nome);
      esperaTeclado = 0;
      if (dados.nome[0] && foco == I_NOME) foco = B_PRINC;
    } else if (r == TECLADO_CANCELOU) esperaTeclado = 0;
  }
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  entrada = t21_entrada(entrada, dt, aberto);
  pagina = red ? 1 : anim_rampa(pagina, 1, dt, 180);
  if (pagina >= 1) gfx_mini_liberar(&paginaAnterior);
  tempoPasso += dt;
  for (int i = 0; i <= B_PULAR; i++) {
    float target = aberto && foco == i ? 1 : 0;
    focoAnim[i] = red ? target : anim_mola(focoAnim[i], target, dt,
                         target > focoAnim[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
}

// ------------------------------------------------------------------- desenho
typedef struct { int r, g, b; } Tinta;

static void txt_em(TxtEstilo est, const char *s, float x, float y, float maxW, int r, int g, int b, float a) {
  TxtLinha l = txt_linha_corta(est, s, r, g, b, 255, maxW);
  txt_desenhar_alpha(l, x, y, a);
}

static float txt_w(TxtEstilo est, const char *s) { return (float)txt_linha(est, s, 0, 0, 0, 255).w; }

// Mock .kick: short caps label, tracked, muted.
static void kicker(const char *s, float x, float y, float a) {
  char up[200];
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(s));
  txt_tracking(T21_KICKER, up, 150, 156, 168, x, y, a, 2.5f);
}

static void cor_perfil(SDL_Color c, float *r, float *g, float *b) {
  *r = c.r / 255.0f; *g = c.g / 255.0f; *b = c.b / 255.0f;
}

static void pt_foco(int id, int nada) { (void)nada; foco = id; }

static void alvo(GfxRect r, int id) {
  if (aberto) ponteiro_alvo(r.x, r.y, r.w, r.h, pt_foco, NULL, id, 0);
}

// Enter: rows rise 24 px and fade in a 28 ms stagger (revela_onda_atraso).
static float entrada_item(GfxRect *r, int i) {
  float p = ajustes_animacoes_reduzidas() ? 1 :
    revela_entra(1, revela_onda_atraso(i, 0), 1 + (Uint32)(tempoPasso * 1000));
  float a = gfx_opacidade_grupo;
  gfx_opacidade_grupo *= p;
  r->y += (1 - p) * NV_ENTRA_DY;
  return a;
}

// Mock .vid row: island surface at rest, the shared lighter step on focus,
// plus the focus spring lift (1 + .02 f). Ink never inverts.
static GfxRect superficie(GfxRect r, float raioPx, float f, float a, Tinta *pri, Tinta *sec) {
  float k = 1.0f + 0.02f * f;
  GfxRect s = { r.x - r.w * (k - 1) * .5f, r.y - r.h * (k - 1) * .5f, r.w * k, r.h * k };
  t21_ilha(s, raioPx, a);
  plrui_linha_foco(s, raioPx, f * a);
  *pri = (Tinta){243, 242, 239};
  *sec = (Tinta){186, 192, 204};
  return s;
}

static void bola(float x, float y, float lado, const char *letra, const char *icone, Tinta t, float focado, float a) {
  GfxRect b = { x, y, lado, lado };
  gfx_cor(b, 0.31f, 1, 1, 1, (.07f + .09f * focado) * a);
  if (icone) gfx_icone((GfxRect){ x + lado * 0.28f, y + lado * 0.28f, lado * 0.44f, lado * 0.44f }, icone,
                       (float)t.r / 255.0f, (float)t.g / 255.0f, (float)t.b / 255.0f, a);
  else if (letra) {
    TxtLinha l = txt_linha(TXT_ILHA_NOME, letra, t.r, t.g, t.b, 255);
    txt_desenhar_alpha(l, x + (lado - (float)l.w) * 0.5f, y + (lado - (float)l.h) * 0.5f, a);
  }
}

// Row of the mock .opc: icon tile, title, description, trailing piece.
static void linha_opcao(GfxRect r, float f, const char *letra, const char *icone, const char *t, const char *d, float a) {
  Tinta p, s;
  r = superficie(r, 24.0f, f, a, &p, &s);
  bola(r.x + 26.0f, r.y + (r.h - 64.0f) * .5f, 64.0f, letra, icone, p, f, a);
  txt_em(TXT_ILHA_NOME, t, r.x + 112.0f, r.y + r.h * .5f - 32.0f, r.w - 250.0f, p.r, p.g, p.b, a);
  txt_em(TXT_CAPTION, d, r.x + 112.0f, r.y + r.h * .5f + 4.0f, r.w - 220.0f, s.r, s.g, s.b, a);
}

static void seta(GfxRect r, float a) {
  gfx_icone((GfxRect){ r.x + r.w - 28.0f - 26.0f, r.y + (r.h - 26.0f) * 0.5f, 26.0f, 26.0f }, "aj_chevron-right",
            186 / 255.0f, 192 / 255.0f, 204 / 255.0f, a);
}

// Mock .lig: 64x36 switch; accent track with contrast knob when on.
static void interruptor(float x, float y, int ligado, float a) {
  float ar, ag, ab, t = ajustes_tinta_foco() / 255.0f;
  ajustes_acento(&ar, &ag, &ab);
  if (ligado) gfx_cor((GfxRect){ x, y, 64, 36 }, .5f, ar, ag, ab, a);
  else gfx_cor((GfxRect){ x, y, 64, 36 }, .5f, 1, 1, 1, .14f * a);
  gfx_cor((GfxRect){ x + (ligado ? 32.0f : 4.0f), y + 4, 28, 28 }, .5f,
          ligado ? t : .73f, ligado ? t : .73f, ligado ? t : .73f, a);
}

// Three waiting dots of the mock .espera (shared breathing point).
static float espera(float x, float y, const char *s, float a) {
  for (int j = 0; j < 3; j++) plrui_respira(x + 5 + j * 22, y + 14, 10, SDL_GetTicks() + j * 180, a);
  txt_em(TXT_ILHA_SUB, s, x + 80, y, 900, 186, 192, 204, a);
  return y + 30;
}

// A inicial do nome (um caractere UTF-8), em maiuscula quando e ASCII.
static void inicial(char *dst, size_t tam) {
  const unsigned char *p = (const unsigned char *)dados.nome;
  size_t n = 1;
  if (!*p) { snprintf(dst, tam, "?"); return; }
  if (*p >= 0xF0) n = 4; else if (*p >= 0xE0) n = 3; else if (*p >= 0xC0) n = 2;
  if (n >= tam) n = tam - 1;
  memcpy(dst, p, n);
  dst[n] = 0;
  if (n == 1) dst[0] = (char)toupper(dst[0]);
}

static void avatar(float cx, float cy, float lado, int cor, TxtEstilo est, float a) {
  float r, g, b;
  char ini[8];
  TxtLinha l;
  cor_perfil(CORES[cor], &r, &g, &b);
  gfx_cor((GfxRect){ cx - lado * 0.5f, cy - lado * 0.5f, lado, lado }, 0.5f, r, g, b, a);
  inicial(ini, sizeof ini);
  l = txt_linha(est, ini, 255, 255, 255, 255);
  txt_desenhar_alpha(l, cx - (float)l.w * 0.5f, cy - (float)l.h * 0.5f, a);
}

static const char *nome_exibido(void) { return dados.nome[0] ? dados.nome : i18n("Perfil principal"); }

static void cabecalho(float a) {
  static const char *const ROT[PASSOS] = { "1 · Perfil", "2 · Trazer suas coisas", "3 · Acompanhar", "4 · Pronto" };
  float inicio = fminf(400.0f, largura * .24f);
  float skip = passo < PASSOS - 1 ? botao_largura(rotulo_botao(B_PULAR), NULL, 0) + BOTAO_GAP : 0;
  float ar, ag, ab, segW = (largura - inicio - skip - 3.0f * 12.0f) / 4.0f;
  int i;
  txt_em(TXT_NOV_TITULO, "nuvio", ox + MARGEM, ANC(T21_SAFE_T), inicio - 24.0f, 243, 242, 239, a);
  ajustes_acento(&ar, &ag, &ab);
  for (i = 0; i < PASSOS; i++) {
    float x = ox + MARGEM + inicio + (float)i * (segW + 12.0f);
    GfxRect s = { x, ANC(T21_SAFE_T) + 10.0f, segW, 5.0f };
    if (i < passo) gfx_cor(s, 0.5f, 1, 1, 1, 0.55f * a);
    else if (i == passo) {
      gfx_cor(s, .5f, 1, 1, 1, .12f * a);
      s.w *= anim_saida(anim_atraso(tempoPasso, 0, 0, T21_ENTER_MS));
      if (s.w > .01f) gfx_cor(s, .5f, ar, ag, ab, a);
    }
    else gfx_cor(s, 0.5f, 1, 1, 1, 0.12f * a);
    txt_em(TXT_CAPTION, i18n(ROT[i]), x, ANC(T21_SAFE_T) + 26.0f, segW, i <= passo ? 186 : 120, i <= passo ? 192 : 126, i <= passo ? 204 : 138, a);
  }
}

static void titulo(const char *t, const char *sub, float w, float a) {
  float y = titulo_y();
  w = fminf(w, largura);
  txt_em(TXT_V2_TIT, t, ox + MARGEM, y, w, 248, 249, 252, a);
  txt_bloco(TXT_V2_26, sub, 186, 192, 204, ox + MARGEM, y + 64.0f + 14.0f, w, 34.0f, 0.9f * a, 2);
}

static void desenha_passo0(float a) {
  Item it[MAX_ITENS];
  int n = montar(it), i;
  Tinta p, s;
  char lng[160];
  titulo(i18n("Quem vai assistir?"),
         i18n("Cada perfil tem o próprio Continuar, as próprias listas e os próprios Ajustes. Dá para criar outros depois."),
         820.0f, a);
  for (i = 0; i < n; i++) {
    GfxRect r = it[i].r;
    int id = it[i].id;
    float f = focoAnim[id];
    if (id >= B_VOLTAR) continue;
    float grupo = entrada_item(&r, id >= I_INFANTIL ? id - 5 : id >= I_COR0 ? 1 : 0);
    if (id == I_NOME) {
      float ar, ag, ab, tx = r.x + 28.0f;
      GfxRect q = superficie(r, 24.0f, f, a, &p, &s);
      if (dados.nome[0]) {
        txt_em(TXT_ILHA_TITULO, dados.nome, tx, q.y + (q.h - 48.0f) * 0.5f, q.w - 380.0f, p.r, p.g, p.b, a);
        tx += fminf(txt_w(TXT_ILHA_TITULO, dados.nome), q.w - 380.0f) + 10.0f;
      } else txt_em(TXT_ILHA_TITULO, i18n("Seu nome"), tx, q.y + (q.h - 48.0f) * 0.5f, q.w - 380.0f, s.r, s.g, s.b, 0.7f * a);
      ajustes_acento(&ar, &ag, &ab);   // mock .cur: caret in the accent while focused
      if (f > .01f && dados.nome[0]) gfx_cor((GfxRect){ tx, q.y + (q.h - 38.0f) * .5f, 3, 38 }, 0, ar, ag, ab, f * a);
      { const char *dica = i18n("ou digite pelo celular");
        txt_em(TXT_ILHA_SUB, dica, q.x + q.w - 28.0f - fminf(txt_w(TXT_ILHA_SUB, dica), 340.0f),
               q.y + (q.h - 26.0f) * 0.5f, 340.0f, 150, 156, 168, a); }
      alvo(r, id);
    } else if (id >= I_COR0 && id < I_COR0 + COMECAR_CORES) {
      int c = id - I_COR0;
      float g = f * 5.0f;   // mock: scale 1.14 on focus
      GfxRect d = { r.x - g, r.y - g, r.w + 2 * g, r.h + 2 * g };
      if (f > .01f) gfx_rect((GfxRect){ d.x - 10, d.y + 4, d.w + 20, d.h + 22 }, 0, GFX_SOMBRA, 1, 0, 0, .5f, 0, 0, 0, .5f * f * a);
      avatar(d.x + d.w * .5f, d.y + d.h * .5f, d.w, c, TXT_ILHA_TITULO, a);
      if (c == dados.cor)   // chosen colour: tick badge, no ring (DESIGN focus rule)
        gfx_icone((GfxRect){ d.x + d.w - 22, d.y - 4, 26, 26 }, "aj_circle-check", 1, 1, 1, a);
      alvo(r, id);
    } else if (id == I_INFANTIL) {
      linha_opcao(r, f, NULL, "aj_lock", i18n("Perfil infantil"),
                  i18n(infantil_travado() ? "Este perfil tem PIN: mude em Ajustes › Interface e conta"
                                          : "Só classificação livre e 10 anos; sem TV ao vivo"), a);
      interruptor(r.x + r.w - 28.0f - 64.0f, r.y + (r.h - 36.0f) * .5f, perfil_infantil_ativo(), a);
      alvo(r, id);
    } else if (id == I_IDIOMA) {
      const char *leg = ling_legenda();
      if (leg && *leg) snprintf(lng, sizeof lng, i18n("%s · legendas em %s"), i18n(ajustes_idioma_nome()), i18n(ling_nome(leg)));
      else snprintf(lng, sizeof lng, "%s", i18n(ajustes_idioma_nome()));
      linha_opcao(r, f, NULL, "aj_languages", i18n("Idioma e legendas"), lng, a);
      seta(r, a);
      alvo(r, id);
    }
    gfx_opacidade_grupo = grupo;
  }
  // O painel da direita: o perfil como vai ficar.
  { float x = ox + MARGEM + coluna + T21_GAP_BIG, w = fminf(520.0f, lateral), px = x + (lateral - w) * .5f;
    float y = corpo_y() - 34.0f, h = fminf(420.0f, rodape_topo() - T21_GAP_CARD - y - 110.0f);
    float lado = fminf(200.0f, h - 150.0f);
    GfxRect r = {px, y, w, h};
    float grupo = entrada_item(&r, 2);
    t21_ilha(r, T21_R_MODAL, a);
    avatar(px + w * .5f, r.y + 44.0f + lado * .5f, lado, dados.cor, TXT_TITULO1, a);
    { const char *nm = nome_exibido(); float nw = fminf(txt_w(TXT_ILHA_TITULO, nm), w - 64.0f);
      txt_em(TXT_ILHA_TITULO, nm, px + (w - nw) * .5f, r.y + 44.0f + lado + 24.0f, w - 64.0f, 248, 249, 252, a); }
    { const char *sb = i18n("Perfil principal · desta TV"); float sw = fminf(txt_w(TXT_ILHA_SUB, sb), w - 64.0f);
      txt_em(TXT_ILHA_SUB, sb, px + (w - sw) * .5f, r.y + 44.0f + lado + 80.0f, w - 64.0f, 150, 156, 168, a); }
    txt_bloco(TXT_ILHA_SUB, i18n("Já usa o Nuvio em outra TV? No próximo passo, “Conta Nuvio” traz os perfis de lá."),
              130, 136, 148, px, r.y + h + 28.0f, w, 28.0f, a, 3);
    gfx_opacidade_grupo = grupo; }
}

// "K7QX2M" -> "K7Q X2M", as on the mock; other lengths stay as they are.
static void codigo_espacado(const char *c, char *dst, size_t tam) {
  if (strlen(c) == 6) snprintf(dst, tam, "%.3s %.3s", c, c + 3);
  else snprintf(dst, tam, "%s", c);
}

// Mock 3B side panel while Stremio waits: QR, link, spaced code, waiting dots, footnote.
static void painel_stremio(float x, float y, float w, float ph, ImpfEstado e, const char *d, float a) {
  float lado = fminf(fminf(268.0f, ph - 240.0f), (w - 108.0f) * .46f);
  float qy = y + 96.0f, tx = x + 44.0f + lado + 36.0f, tw = w - (tx - x) - 44.0f;
  float k = lado / celb_qr_lado(), sy;
  char cod[32];
  kicker("Conta Stremio · pelo celular", x + 44.0f, y + 44.0f, a);
  gfx_transformar(x + 44.0f, qy, k, 0, 0);
  celb_qr_desenhar(ultimoLink, x + 44.0f, qy, a);
  gfx_sem_transformar();
  sy = qy + txt_bloco(TXT_V2_26, i18n("Aponte a câmera do celular e entre na sua conta lá."),
                      220, 224, 230, tx, qy, tw, 34.0f, a, 3) + 20.0f;
  txt_em(TXT_ILHA_SUB, i18n("ou digite no endereço"), tx, sy, tw, 150, 156, 168, a);
  txt_em(TXT_ILHA_SUB, ultimoLink, tx, sy + 28.0f, tw, 186, 192, 204, a);
  codigo_espacado(ultimoCodigo, cod, sizeof cod);
  txt_tracking(TXT_V2_TIT, cod, 248, 249, 252, tx, sy + 62.0f, a, 6.0f);
  sy = qy + lado + 30.0f;
  espera(x + 44.0f, sy, e == IMPF_OK ? d : i18n("Esperando o celular · código vale 10 min"), a);
  if (sy + 70.0f < y + ph - 60.0f) {
    gfx_cor((GfxRect){ x + 44.0f, sy + 56.0f, w - 88.0f, 1 }, 0, 1, 1, 1, .08f * a);
    txt_bloco(TXT_ILHA_SUB, i18n("Sua senha fica no celular; a TV recebe só a chave da conta. Os add-ons chegam na ordem em que estão no Stremio."),
              186, 192, 204, x + 44.0f, sy + 76.0f, w - 88.0f, 28.0f, a, 2);
  }
}

static void desenha_passo1(float a) {
  static const struct { const char *nome, *desc, *letra, *icone; } OP[4] = {
    { "Conta Stremio", "Add-ons na mesma ordem, biblioteca e Continuar assistindo", "S", NULL },
    { "AIOManager", "Sua configuração de AIOStreams e AIOMetadata, já pronta", NULL, "aj_layers" },
    { "Conta Nuvio", "Perfis, Ajustes e salvos de outra TV", "N", NULL },
    { "Começar do zero", "Um conjunto pequeno de add-ons públicos para experimentar", NULL, "aj_sparkles" },
  };
  Item it[MAX_ITENS];
  int n = montar(it), i;
  titulo(i18n("Traga o que você já usa"),
         i18n("Add-ons, catálogos e o que estava assistindo. Escolha uma; dá para juntar outra depois."), 860.0f, a);
  for (i = 0; i < n; i++) {
    GfxRect r = it[i].r;
    int id = it[i].id;
    if (id >= 4) continue;
    float grupo = entrada_item(&r, i);
    linha_opcao(r, focoAnim[id], OP[id].letra, OP[id].icone, i18n(OP[id].nome), i18n(OP[id].desc), a);
    seta(r, a);
    alvo(r, id);
    gfx_opacidade_grupo = grupo;
  }
  // O painel da direita explica a opcao em foco; as apagadas dizem a verdade.
  { float x = ox + MARGEM + coluna + T21_GAP_BIG, y = corpo_y() - 30.0f, w = lateral;
    const char *t, *d;
    int extra = 0;
    int opcao = foco <= I_ZERO ? foco : importarfluxo_fonte() == IMPF_STREMIO ? I_STREMIO :
                importarfluxo_fonte() == IMPF_AIOM ? I_AIO : I_NUVIO;
    int ativo = (opcao == I_STREMIO && importarfluxo_fonte() == IMPF_STREMIO) ||
                (opcao == I_AIO && importarfluxo_fonte() == IMPF_AIOM);
    ImpfEstado e = ativo ? importarfluxo_estado() : IMPF_PARADO;
    char msg[200];
    if (opcao == I_STREMIO || opcao == I_AIO) {
      t = i18n(opcao == I_STREMIO ? "Conta Stremio" : "AIOManager");
      if (e == IMPF_OK) {
        snprintf(msg, sizeof msg, i18n("Entraram %d add-ons e %d salvos."), importarfluxo_addons(), importarfluxo_titulos());
        d = msg;
      } else if (e == IMPF_ERRO) d = i18n("Não deu certo. OK para tentar de novo.");
      else if (e == IMPF_TRABALHA) d = i18n("Importando…");
      else if (e == IMPF_ESPERA)
        d = i18n(opcao == I_STREMIO ? "Abra este endereço no celular e digite o código:"
                                   : "Cole no celular o link de exportação do AIOManager.");
      else d = i18n(opcao == I_STREMIO ? "Entre com a sua conta Stremio pelo celular. Os add-ons e a biblioteca vêm para cá."
                                      : "Cole no celular o link de exportação do AIOManager. Só os add-ons entram.");
      extra = e == IMPF_ESPERA;
    } else if (opcao == I_NUVIO) {
      t = i18n("Conta Nuvio");
      d = i18n("Você já entrou na sua conta Nuvio. Perfis, Ajustes e salvos de outras TVs chegam por ela.");
    } else {
      t = i18n("Começar do zero");
      d = i18n("Você segue com o que já está nesta TV e junta o resto quando quiser.");
    }
    float ph = rodape_topo() - T21_GAP_CARD - y;
    GfxRect r = {x, y, w, ph};
    float grupo = entrada_item(&r, 4);
    y = r.y;
    t21_ilha(r, T21_R_MODAL, a);
    if (ativo && opcao == I_STREMIO && importarfluxo_codigo()[0]) {
      snprintf(ultimoCodigo, sizeof ultimoCodigo, "%s", importarfluxo_codigo());
      snprintf(ultimoLink, sizeof ultimoLink, "%s", importarfluxo_link());
    }
    if (opcao == I_STREMIO && ultimoCodigo[0] && ativo) painel_stremio(x, y, w, ph, e, d, a);
    else {
      kicker(opcao == I_NUVIO || opcao == I_ZERO ? "Nesta TV" : "Pelo celular", x + 44.0f, y + 44.0f, a);
      txt_em(TXT_ILHA_TITULO, t, x + 44.0f, y + 80.0f, w - 88.0f, 248, 249, 252, a);
      txt_bloco(TXT_V2_26, d, 200, 205, 215, x + 44.0f, y + 140.0f, w - 88.0f, 36.0f, 0.9f * a, extra ? 2 : 5);
      if (extra && opcao == I_AIO && celb_embutido(CELB_IMPORTAR))
        celb_desenhar_em(CELB_IMPORTAR, (GfxRect){ x + 44.0f, y + 220.0f, w - 88.0f, ph - 250.0f }, a);
      if (e == IMPF_OK) espera(x + 44.0f, y + ph - 70.0f, d, a);
    }
    gfx_opacidade_grupo = grupo; }
}

static const char *rotulo_chip(int id, char *buf, size_t tam) {
  if (id == I_CH_HIST) return i18n("Importar o que já foi visto");
  snprintf(buf, tam, "%s %s", i18n(id == I_CH_CW ? "Continuar assistindo vem de" : "O + salva em"),
           i18n(ajustes_rapido_valor(op_chip(id))));
  return buf;
}

// Mock .chip: 56 high pill, label + muted value; focus = the shared accent pill.
static void chip(GfxRect r, int id, float f, float a) {
  char b[160];
  float tc = ajustes_tinta_foco() / 255.0f;
  int ti = (int)(243 + (ajustes_tinta_foco() - 243) * f);
  const char *pre = i18n(id == I_CH_CW ? "Continuar assistindo vem de" : "O + salva em");
  float x = r.x + BOTAO_PAD_X2;
  plrui_botao_repouso(r, a);
  plrui_pilula_foco(r, f * a);
  if (id == I_CH_HIST) {
    int on = trakt_ativo() || simkl_ativo();
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    if (on) gfx_cor((GfxRect){ x, r.y + r.h * .5f - 4, 8, 8 }, .5f,
                    ar + (tc - ar) * f, ag + (tc - ag) * f, ab + (tc - ab) * f, a);
    txt_em(TXT_V2_CHIP, rotulo_chip(id, b, sizeof b), x + 20.0f, r.y + (r.h - 30.0f) * .5f, r.w - 76.0f, ti, ti, ti, a);
    return;
  }
  txt_em(TXT_V2_CHIP, pre, x, r.y + (r.h - 30.0f) * .5f, r.w, ti, ti, ti, a);
  x += txt_w(TXT_V2_CHIP, pre) + txt_w(TXT_V2_CHIP, " ");
  { int v = (int)(150 + (ajustes_tinta_foco2() - 150) * f);
    txt_em(TXT_V2_CHIP, i18n(ajustes_rapido_valor(op_chip(id))), x, r.y + (r.h - 30.0f) * .5f, r.x + r.w - x, v, v, v, a); }
}

static void desenha_passo2(float a) {
  static const struct { const char *nome, *desc, *letra, *icone; } TK[3] = {
    { "Trakt", "Histórico, watchlist e Próximos episódios.", "T", NULL },
    { "Simkl", "Bom para anime. Entra pelo código no celular.", "S", NULL },
    { "Só nesta TV", "Sem conta externa. O progresso fica guardado neste perfil.", NULL, "aj_tv-minimal-play" },
  };
  Item it[MAX_ITENS];
  int n = montar(it), i;
  float ar, ag, ab;
  ajustes_acento_marca(&ar, &ag, &ab);
  titulo(i18n("Onde marcar o que você assiste"),
         i18n("O Nuvio avisa o tracker quando você termina um episódio e lê de lá o que falta ver. Pode ligar mais de um."),
         1100.0f, a);
  for (i = 0; i < n; i++) {
    GfxRect r = it[i].r;
    int id = it[i].id;
    float f = focoAnim[id];
    Tinta p, s;
    const char *st = NULL, *cod = "", *url = "";
    int ligado = 0, aguarda = 0;
    if (id >= B_VOLTAR) continue;
    float grupo = entrada_item(&r, id >= I_CH_CW ? 3 + id - I_CH_CW : i);
    if (id >= I_CH_CW) { chip(r, id, f, a); alvo(r, id); gfx_opacidade_grupo = grupo; continue; }
    if (id == I_TRAKT) {
      TraEstado e = traktauth_estado();
      ligado = trakt_ativo();
      st = ligado ? "Ligado" : e == TRA_PEDINDO ? "Pedindo o código…" : e == TRA_AGUARDANDO ? "Esperando"
         : (e == TRA_ERRO || e == TRA_INVALIDO) ? "Não deu certo. OK para tentar de novo." : "Aperte OK para ligar";
      if (!ligado && e == TRA_AGUARDANDO) { aguarda = 1; cod = traktauth_codigo(); url = traktauth_url(); }
    } else if (id == I_SIMKL) {
      SmkEstado e = simklauth_estado();
      ligado = simkl_ativo();
      st = ligado ? "Ligado" : e == SMK_PEDINDO ? "Pedindo o código…" : e == SMK_AGUARDANDO ? "Esperando"
         : e == SMK_ERRO ? "Não deu certo. OK para tentar de novo." : "Aperte OK para ligar";
      if (!ligado && e == SMK_AGUARDANDO) { aguarda = 1; cod = simklauth_codigo(); url = simklauth_url(); }
    } else st = "Sempre ligado";
    r = superficie(r, 28.0f, f, a, &p, &s);
    bola(r.x + 30.0f, r.y + 30.0f, 64.0f, TK[id].letra, TK[id].icone, p, f, a);
    { float tw = aguarda ? r.w - 60.0f - fminf(160.0f, r.w * .38f) - 20.0f : r.w - 60.0f;
      float ty = r.y + 30.0f + 64.0f + 20.0f;
      txt_em(TXT_ILHA_TITULO, i18n(TK[id].nome), r.x + 30.0f, ty, tw, p.r, p.g, p.b, a);
      if (aguarda) {   // mock Simkl card: code + small QR inside the card
        float q = fminf(160.0f, r.w * .38f), k = q / celb_qr_lado();
        txt_bloco(TXT_CAPTION, i18n("Entre pelo celular com o código:"), s.r, s.g, s.b, r.x + 30.0f, ty + 52.0f, tw, 28.0f, a, 2);
        char ce[32]; codigo_espacado(cod, ce, sizeof ce);
        txt_tracking(TXT_ILHA_TITULO, ce, 248, 249, 252, r.x + 30.0f, ty + 116.0f, a, 5.0f);
        gfx_transformar(r.x + r.w - 30.0f - q, r.y + (r.h - q) * .5f, k, 0, 0);
        celb_qr_desenhar(url, r.x + r.w - 30.0f - q, r.y + (r.h - q) * .5f, a);
        gfx_sem_transformar();
      } else txt_bloco(TXT_CAPTION, i18n(TK[id].desc), s.r, s.g, s.b, r.x + 30.0f, ty + 52.0f, tw, 30.0f, a, 3);
    }
    { float sy = r.y + r.h - 30.0f - 26.0f;
      if (aguarda) espera(r.x + 30.0f, sy - 2.0f, i18n(st), a);
      else if (ligado) {
        gfx_icone((GfxRect){ r.x + 30.0f, sy, 24.0f, 24.0f }, "aj_circle-check", ar, ag, ab, a);
        txt_em(TXT_ILHA_NOME, i18n(st), r.x + 64.0f, sy - 4.0f, r.w - 94.0f, (int)(ar * 255), (int)(ag * 255), (int)(ab * 255), a);
      } else txt_em(TXT_ILHA_NOME, i18n(st), r.x + 30.0f, sy - 4.0f, r.w - 60.0f, id == I_TV ? 150 : p.r, id == I_TV ? 156 : p.g, id == I_TV ? 168 : p.b, a); }
    alvo(r, id);
    gfx_opacidade_grupo = grupo;
  }
  kicker("Escolhemos por você · mude se quiser", ox + MARGEM, corpo_y() + tracker_altura() + 46.0f - 4.0f, a);
}

// Mock 3D check (status, never focusable): accent disc with tick (or ring while working), title, detail, hairline.
static float resumo_feito(float x, float y, float w, float h, const char *t, const char *d, int vai, int i, float a) {
  float ar, ag, ab, tc = ajustes_tinta_foco() / 255.0f;
  GfxRect r = {x, y, w, h};
  float grupo = entrada_item(&r, i);
  y = r.y;
  ajustes_acento(&ar, &ag, &ab);
  if (vai) plrui_anel(x + 18.0f, y + 16.0f + 18.0f, 36.0f, 0, SDL_GetTicks(), a);
  else {
    gfx_cor((GfxRect){ x, y + 16.0f, 36.0f, 36.0f }, .5f, ar, ag, ab, a);
    gfx_icone((GfxRect){ x + 8.0f, y + 24.0f, 20.0f, 20.0f }, "aj_check", tc, tc, tc, a);
  }
  txt_em(TXT_ILHA_NOME, t, x + 54.0f, y + 12.0f, w - 54.0f, 243, 242, 239, a);
  txt_em(TXT_ILHA_SUB, d, x + 54.0f, y + 46.0f, w - 54.0f, 150, 156, 168, a);
  if (i < 4) gfx_cor((GfxRect){ x, y + h - 1.0f, w, 1.0f }, 0, 1, 1, 1, 0.07f * a);
  gfx_opacidade_grupo = grupo;
  return h;
}

// The real Continue watching row the Home will open with (no invented titles).
static void mini_home(float x, float y, float w, float a) {
  static CatItem cw[4];
  int n = cat_copiar_fileira("continue_watching", cw, 4, NULL), i;
  float h = w * 9.0f / 16.0f;
  GfxRect r = {x, y, w, h};
  float grupo = entrada_item(&r, 1);
  y = r.y;
  gfx_rect((GfxRect){ x - 20, y + 10, w + 40, h + 50 }, 0, GFX_SOMBRA, 1, 0, 0, .5f, 0, 0, 0, .5f * a);
  if (n <= 0) {
    t21_ilha(r, T21_R_SHEET, a);
    txt_bloco(TXT_V2_26, i18n("O que você começar a ver aparece aqui."), 186, 192, 204, x + 44, y + h * .5f - 20, w - 88, 34, a, 2);
  } else {
    const CatItem *c = &cw[0];
    GLuint t = tex_obter_larg(c->backdrop[0] ? c->backdrop : c->poster, w);
    char m[160];
    if (t) gfx_rect(r, t, GFX_CARD, 0, 0, 0, 24.0f / h, 0, 0, 0, a);
    else gfx_esqueleto(r, 24.0f / h, .14f, .15f, .17f, a);
    gfx_veu_base(r, 24.0f / h, .78f, .9f * a);
    gfx_veu_css(r, 2, 0, .75f, .6f * a);
    txt_em(TXT_TITULO3, c->titulo, x + 44, y + h * .28f, w - 88, 248, 249, 252, a);
    if (c->temporada > 0 && c->restanteMin > 0)
      snprintf(m, sizeof m, i18n("Continuar T%d:E%d · faltam %d min"), c->temporada, c->episodio, c->restanteMin);
    else if (c->restanteMin > 0) snprintf(m, sizeof m, i18n("faltam %d min"), c->restanteMin);
    else snprintf(m, sizeof m, "%s", c->meta);
    txt_em(TXT_ILHA_SUB, m, x + 44, y + h * .28f + 62, w - 88, 200, 205, 215, a);
    { float cw2 = (w - 88 - 3 * 12) / 4.0f, ch = cw2 * 84.0f / 150.0f;
      for (i = 0; i < n; i++) {
        GfxRect q = { x + 44 + i * (cw2 + 12), y + h - 30 - ch, cw2, ch };
        GLuint qt = tex_obter_larg(cw[i].backdrop[0] ? cw[i].backdrop : cw[i].poster, cw2);
        if (qt) gfx_rect(q, qt, GFX_CARD, 0, 0, 0, 10.0f / ch, 0, 0, 0, a);
        else gfx_esqueleto(q, 10.0f / ch, .2f, .21f, .23f, a);
      } }
  }
  txt_em(TXT_ILHA_SUB, i18n("Sua Home, já com o que você estava vendo."), x, y + h + 18, w, 130, 136, 148, a);
  gfx_opacidade_grupo = grupo;
}

static void desenha_passo3(float a) {
  char t[200], d[200];
  static int vistos;
  static Uint32 contado;
  int tr = trakt_ativo(), sk = simkl_ativo(), na = addons_n(), nc = col_n(), i = 0;
  float x = ox + MARGEM, y = corpo_y() - 54.0f, w = coluna;
  float h = fminf(96.0f, (rodape_topo() - T21_GAP_CARD - y) / 5.0f);
  if (!contado || SDL_GetTicks() - contado > 1000) {   // the import keeps landing: recount each second
    contado = SDL_GetTicks() | 1; vistos = 0;
    for (int k = 0; k < cat_n(); k++) vistos += cat_visto(cat_item(k)) != 0;
  }
  snprintf(t, sizeof t, i18n("Pronto, %s."), nome_exibido());
  titulo(t, i18n("Arrumamos isto enquanto você lia:"), 860.0f, a);
  // Mock 3D: five independent outcomes, each from live state.
  snprintf(t, sizeof t, i18n("%d add-ons ativos"), na);
  y += resumo_feito(x, y, w, h, t, i18n(na ? "Na ordem em que chegaram. Mude em Ajustes › Fontes e addons."
                                           : "Nenhum ainda. Junte em Ajustes › Fontes e addons."), 0, i++, a);
  snprintf(t, sizeof t, i18n("Biblioteca: %d para ver, %d assistidos"), salvos_n(), vistos);
  y += resumo_feito(x, y, w, h, t, i18n(tr && sk ? "Trakt e Simkl juntos, sem repetir título."
                                        : tr || sk ? "Dá para mudar em Ajustes › Contas."
                                        : "Você liga o Trakt ou o Simkl depois, em Ajustes › Contas."), 0, i++, a);
  if (nc > 0) snprintf(t, sizeof t, i18n("%d coleções viraram fileiras na Home"), nc);
  else snprintf(t, sizeof t, "%s", i18n("Nenhuma coleção na Home ainda"));
  y += resumo_feito(x, y, w, h, t, i18n("Fixe, mova ou esconda segurando OK."), 0, i++, a);
  snprintf(d, sizeof d, "%s%s%s", ajustes_4k() ? "4K · " : "", ajustes_dolby_vision() ? "Dolby Vision · " : "",
           i18n(ajustes_dts_ac3() ? "DTS vira 5.1 compatível." : "Áudio no formato que a TV aceita."));
  y += resumo_feito(x, y, w, h, i18n("Fonte automática ajustada a esta TV"), d, 0, i++, a);
  resumo_feito(x, y, w, h, i18n("Pular abertura e créditos"),
               i18n(ajustes_pular_abertura() ? "Ligado. As marcas chegam junto com cada título."
                                             : "Desligado. Ligue em Ajustes › Reprodução."), 0, i++, a);
  mini_home(ox + MARGEM + coluna + T21_GAP_BIG, corpo_y() - 54.0f, fminf(760.0f, lateral), a);
}

static void desenha_rodape(float a) {
  static const char *const DICA[PASSOS] = {
    "Leva uns 2 minutos · tudo pode mudar depois em Ajustes",
    "Back volta um passo, nada é perdido",
    "Simkl pode ser ligado depois em Ajustes › Contas",
    "O tour mostra o menu, a ilha do relógio e o botão de Fontes",
  };
  Item it[MAX_ITENS];
  int n = montar(it), i;
  float fim = 0;
  for (i = 0; i < n; i++) {
    int id = it[i].id, prim = id == B_PRINC;
    if (id < B_VOLTAR) continue;
    botao_pilula(it[i].r, rotulo_botao(id), botao_icone(id), focoAnim[id], prim, 0, a * (id == B_PULAR ? .72f + .28f * focoAnim[id] : 1));
    alvo(it[i].r, id);
    if (id != B_PULAR) fim = fmaxf(fim, it[i].r.x + it[i].r.w);
  }
  { const char *d = i18n(DICA[passo]);
    float dx = ox + MARGEM + largura, lw = dx - fim - T21_GAP_BIG, w = fminf(txt_w(TXT_ILHA_SUB, d), lw);
    if (lw > 120.0f) txt_em(TXT_ILHA_SUB, d, dx - w, ANC(T21_SAFE_B) - BOTAO_H_PRIMARIO * .5f - 13.0f, lw, 130, 136, 148, a); }
}

void comecar_desenhar(Uint32 agora) {
  float a = anim_suave(entrada);
  if (entrada < 0.002f) return;
  TXT_INI();
  origem();
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0.051f, 0.051f, 0.051f, entrada);
  cabecalho(a);
  float p = paginaAnterior.tex ? 1 : anim_saida(pagina);
  gfx_transformar(0, 0, 1, (1 - p) * T21_GAP_CARD, 0);
  if (passo == PASSOS - 1) desenha_passo3(a * p);
  else if (passo == 0) desenha_passo0(a * p);
  else if (passo == 1) desenha_passo1(a * p);
  else desenha_passo2(a * p);
  gfx_sem_transformar();
  desenha_rodape(a);
  if (paginaAnterior.tex && !gfx_mini_ativo())
    gfx_mini_desenhar(&paginaAnterior, (GfxRect){0, 0, NV_TELA_W, NV_TELA_H}, 0, a * (1 - anim_saida(pagina)));
  TXT_FIM();
  // O teclado da TV cuida da propria escala e vai por cima de tudo.
  if (esperaTeclado && teclado_aberto()) teclado_desenhar(agora);
}

void comecar_evento(const SDL_Event *e) { TXT_INI(); comecar_evento_(e); TXT_FIM(); }
