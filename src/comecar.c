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
#include "botoes.h"
#include "dados.h"
#include "gfx.h"
#include "idioma.h"
#include "layout.h"
#include "ponteiro.h"
#include "simkl.h"
#include "simklauth.h"
#include "teclado.h"
#include "text.h"
#include "trakt.h"
#include "traktauth.h"
#define NV_ESCALA_TELA_ATIVA
#include "escala.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define Q_W        1920.0f
#define Q_H        1080.0f
#define MARGEM      120.0f
#define ABRIR_MS    320.0f
#define FECHAR_MS   180.0f
#define PASSOS        4
#define MAX_ITENS    16
#define BACK_LONGO_MS 700   // Voltar segurado por tanto tempo pula o fluxo todo

// Ids de foco. Os de campo valem dentro do passo; os botoes valem em todos.
enum { I_NOME = 0, I_COR0 = 1 };                                       // passo 0
enum { I_STREMIO = 0, I_AIO, I_NUVIO, I_ZERO };                        // passo 1
enum { I_TRAKT = 0, I_SIMKL, I_TV };                                   // passo 2
enum { B_VOLTAR = 100, B_PRINC, B_SEC, B_PULAR };

typedef struct { int id; GfxRect r; } Item;

static int   aberto, passo, foco = B_PRINC, pedido, esperaTeclado, decidido, temDados;
static Uint32 backDesde;   // timestamp do Voltar que abriu a pressao atual; 0 = nenhuma
static float entrada;
static ComecarSalvo dados;   // o que a pessoa esta preenchendo (so vai ao disco no fim)
static ComecarSalvo antes;   // o que ja estava gravado quando a tela abriu

static const unsigned CORES[COMECAR_CORES] = { 0x7c5cff, 0xe0565b, 0x00a88f, 0xf0a030, 0x3d8bff, 0xe05aa0 };

int comecar_aberto(void) { return aberto; }
int comecar_passo(void) { return passo; }
int comecar_foco(void) { return foco; }
void comecar_teste_esquecer(void) {
  aberto = 0; entrada = 0.0f; decidido = 0; temDados = 0; backDesde = 0;
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
  passo = p < 0 ? 0 : p >= PASSOS ? PASSOS - 1 : p;
  foco = foco_padrao();
}

static void abrir(void) {
  aberto = 1;
  entrada = 0.0f;
  pedido = COMECAR_PEDIU_NADA;
  esperaTeclado = 0;
  backDesde = 0;
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
    case 0: return i18n("Continuar");
    case 1: return id == B_VOLTAR ? i18n("Voltar") : i18n("Pular este passo");
    case 2: return id == B_VOLTAR ? i18n("Voltar") : i18n("Continuar");
    default: return id == B_PRINC ? i18n("Ir para a Home") : i18n("Fazer o tour · 1 min");
  }
}

// ------------------------------------------------------------------- layout
static float ox, oy;

static void origem(void) {
  ox = (NV_TELA_W - Q_W) * 0.5f;
  oy = (NV_TELA_H - Q_H) * 0.5f;
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
  float x = MARGEM, yc = 972.0f;
  int ids[3], k = 0, i;
  if (passo == PASSOS - 1) { ids[k++] = B_PRINC; ids[k++] = B_SEC; }
  else {
    if (passo > 0) ids[k++] = B_VOLTAR;
    ids[k++] = B_PRINC;
    ids[k++] = B_PULAR;
  }
  for (i = 0; i < k; i++) {
    int prim = ids[i] == B_PRINC;
    float w = botao_largura(rotulo_botao(ids[i]), NULL, prim);
    float h = prim ? BOTAO_H_PRIMARIO : BOTAO_H_SECUNDARIO;
    poe(it, n, ids[i], x, yc - h * 0.5f, w, h);
    x += w + BOTAO_GAP;
  }
}

static int montar(Item *it) {
  int n = 0, i;
  origem();
  if (passo == 0) {
    poe(it, &n, I_NOME, MARGEM, 430.0f, 820.0f, 84.0f);
    for (i = 0; i < COMECAR_CORES; i++) poe(it, &n, I_COR0 + i, MARGEM + (float)i * 90.0f, 548.0f, 72.0f, 72.0f);
  } else if (passo == 1) {
    for (i = 0; i < 4; i++) poe(it, &n, i, MARGEM, 330.0f + (float)i * 114.0f, 860.0f, 104.0f);
  } else if (passo == 2) {
    float w = (1680.0f - 2.0f * 28.0f) / 3.0f;
    for (i = 0; i < 3; i++) poe(it, &n, i, MARGEM + (float)i * (w + 28.0f), 330.0f, w, 330.0f);
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

static void abrir_teclado(void) {
  teclado_abrir_com(i18n("Seu nome"), i18n("Só o nome pede ação; o resto já vem preenchido."), COMECAR_NOME_MAX,
                    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -", dados.nome);
  esperaTeclado = 1;
  backDesde = 0;
}

static void ok(void) {
  if (foco == B_VOLTAR) { ir_passo(passo - 1); return; }
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
  } else if (passo == 1) {
    if (foco == I_NUVIO || foco == I_ZERO) ir_passo(2);   // Stremio e AIOManager: Em breve
  } else if (passo == 2) {
    if (foco == I_TRAKT && !trakt_ativo()) traktauth_comecar();
    else if (foco == I_SIMKL && !simkl_ativo()) simklauth_comecar();
    else if (foco == I_TV) ir_passo(3);
  }
}

void comecar_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto) return;
  // O teclado da TV e modal: com ele aberto, toda tecla e dele (digitar,
  // Voltar cancela, Pronto confirma). Quem le o resultado e comecar_atualizar.
  if (teclado_aberto()) { teclado_evento(e); return; }
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_LEFT)  { mover(-1, 0); return; }
  if (k == SDLK_RIGHT) { mover(1, 0); return; }
  if (k == SDLK_UP)    { mover(0, -1); return; }
  if (k == SDLK_DOWN)  { mover(0, 1); return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) { ok(); return; }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    // Voltar: um passo para tras; no primeiro, pula o fluxo. Segurado por
    // BACK_LONGO_MS (as repeticoes da tecla), pula de qualquer passo. Repeticao
    // nunca volta passo: so a primeira descida conta como "voltar".
    if (!e->key.repeat) {
      backDesde = e->key.timestamp ? e->key.timestamp : 1;
      if (passo == 0) pular();
      else ir_passo(passo - 1);
    } else if (backDesde && e->key.timestamp - backDesde >= BACK_LONGO_MS) pular();
    return;
  }
  if (!e->key.repeat) backDesde = 0;
}

void comecar_atualizar(float dt, Uint32 agora) {
  int red = ajustes_animacoes_reduzidas();
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
  if (red) { entrada = aberto ? 1.0f : 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt, aberto ? ABRIR_MS : FECHAR_MS);
}

// ------------------------------------------------------------------- desenho
typedef struct { int r, g, b; } Tinta;

static void txt_em(TxtEstilo est, const char *s, float x, float y, float maxW, int r, int g, int b, float a) {
  TxtLinha l = txt_linha_corta(est, s, r, g, b, 255, maxW);
  txt_desenhar_alpha(l, x, y, a);
}

static void cor_hex(unsigned h, float *r, float *g, float *b) {
  *r = (float)((h >> 16) & 255) / 255.0f;
  *g = (float)((h >> 8) & 255) / 255.0f;
  *b = (float)(h & 255) / 255.0f;
}

static void pt_foco(int id, int nada) { (void)nada; foco = id; }

static void alvo(GfxRect r, int id) {
  if (aberto) ponteiro_alvo(r.x, r.y, r.w, r.h, pt_foco, NULL, id, 0);
}

// Superficie de um cartao/linha: repouso cinza tingido, foco na cor de realce.
// Devolve as tintas de texto (principal e secundaria) para ESSE estado.
static void superficie(GfxRect r, float raioPx, int focado, float a, Tinta *pri, Tinta *sec) {
  if (focado) {
    float fr, fg, fb, t = botao_cor_foco(&fr, &fg, &fb);
    int p = (int)(t * 255.0f + 0.5f);
    botao_luz(r, 1.0f, a);
    gfx_cor(r, raioPx / r.h, fr, fg, fb, a);
    *pri = (Tinta){ p, p, p };
    *sec = p > 128 ? (Tinta){ 238, 238, 238 } : (Tinta){ 60, 60, 60 };
  } else {
    gfx_cor(r, raioPx / r.h, 0.14f, 0.15f, 0.17f, 0.92f * a);
    *pri = (Tinta){ 243, 242, 239 };
    *sec = (Tinta){ 186, 192, 204 };
  }
}

static void bola(float x, float y, float lado, const char *letra, const char *icone, Tinta t, int focado, float a) {
  GfxRect b = { x, y, lado, lado };
  gfx_cor(b, 0.31f, 1, 1, 1, focado ? 0.16f : 0.07f * a);
  if (icone) gfx_icone((GfxRect){ x + lado * 0.28f, y + lado * 0.28f, lado * 0.44f, lado * 0.44f }, icone,
                       (float)t.r / 255.0f, (float)t.g / 255.0f, (float)t.b / 255.0f, a);
  else if (letra) {
    TxtLinha l = txt_linha(TXT_ILHA_NOME, letra, t.r, t.g, t.b, 255);
    txt_desenhar_alpha(l, x + (lado - (float)l.w) * 0.5f, y + (lado - (float)l.h) * 0.5f, a);
  }
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

static void avatar(float cx, float cy, float lado, float a) {
  float r, g, b;
  char ini[8];
  TxtLinha l;
  cor_hex(CORES[dados.cor], &r, &g, &b);
  gfx_cor((GfxRect){ cx - lado * 0.5f, cy - lado * 0.5f, lado, lado }, 0.5f, r, g, b, a);
  inicial(ini, sizeof ini);
  l = txt_linha(TXT_NOV_TITULO, ini, 255, 255, 255, 255);
  txt_desenhar_alpha(l, cx - (float)l.w * 0.5f, cy - (float)l.h * 0.5f, a);
}

static const char *nome_exibido(void) { return dados.nome[0] ? dados.nome : i18n("Perfil principal"); }

static void cabecalho(float a) {
  static const char *const ROT[PASSOS] = { "1 · Perfil", "2 · Trazer suas coisas", "3 · Acompanhar", "4 · Pronto" };
  float ar, ag, ab, segW = (1800.0f - 520.0f - 3.0f * 12.0f) / 4.0f;
  int i;
  txt_em(TXT_NOV_TITULO, "nuvio", ox + MARGEM, oy + 50.0f, 380.0f, 243, 242, 239, a);
  ajustes_acento(&ar, &ag, &ab);
  for (i = 0; i < PASSOS; i++) {
    float x = ox + 520.0f + (float)i * (segW + 12.0f);
    GfxRect s = { x, oy + 70.0f, segW, 5.0f };
    if (i < passo) gfx_cor(s, 0.5f, 1, 1, 1, 0.55f * a);
    else if (i == passo) gfx_cor(s, 0.5f, ar, ag, ab, a);
    else gfx_cor(s, 0.5f, 1, 1, 1, 0.12f * a);
    txt_em(TXT_CAPTION, i18n(ROT[i]), x, oy + 86.0f, segW, i <= passo ? 186 : 120, i <= passo ? 192 : 126, i <= passo ? 204 : 138, a);
  }
}

static void titulo(const char *t, const char *sub, float w, float a) {
  float y = oy + 190.0f;
  txt_em(TXT_NOV_TITULO, t, ox + MARGEM, y, w, 248, 249, 252, a);
  txt_bloco(TXT_V2_26, sub, 186, 192, 204, ox + MARGEM, y + 76.0f, w, 34.0f, 0.9f * a, 3);
}

static void desenha_passo0(float a) {
  Item it[MAX_ITENS];
  int n = montar(it), i;
  Tinta p, s;
  titulo(i18n("Quem vai assistir?"),
         i18n("Cada perfil tem o próprio Continuar, as próprias listas e os próprios Ajustes. Dá para criar outros depois."),
         820.0f, a);
  for (i = 0; i < n; i++) {
    GfxRect r = it[i].r;
    int id = it[i].id, f = foco == id;
    if (id == I_NOME) {
      superficie(r, 24.0f, f, a, &p, &s);
      if (dados.nome[0]) txt_em(TXT_ILHA_TITULO, dados.nome, r.x + 28.0f, r.y + (r.h - 48.0f) * 0.5f, r.w - 360.0f, p.r, p.g, p.b, a);
      else txt_em(TXT_ILHA_TITULO, i18n("Seu nome"), r.x + 28.0f, r.y + (r.h - 48.0f) * 0.5f, r.w - 360.0f, s.r, s.g, s.b, 0.7f * a);
      txt_em(TXT_CAPTION, i18n("OK para digitar"), r.x + r.w - 28.0f - 200.0f, r.y + (r.h - 24.0f) * 0.5f, 200.0f, s.r, s.g, s.b, a);
      alvo(r, id);
    } else if (id >= I_COR0 && id < I_COR0 + COMECAR_CORES) {
      int c = id - I_COR0;
      float fr, fg, fb, g = f ? 6.0f : 0.0f;
      GfxRect d = { r.x - g, r.y - g, r.w + 2 * g, r.h + 2 * g };
      cor_hex(CORES[c], &fr, &fg, &fb);
      gfx_cor(d, 0.5f, fr, fg, fb, a);
      if (c == dados.cor) {
        gfx_anel((GfxRect){ d.x - 6.0f, d.y - 6.0f, d.w + 12.0f, d.h + 12.0f }, 0.5f, 3.0f, 1, 1, 1, 0.85f * a);
        gfx_icone((GfxRect){ d.x + d.w * 0.3f, d.y + d.h * 0.3f, d.w * 0.4f, d.h * 0.4f }, "aj_check", 1, 1, 1, a);
      }
      alvo(r, id);
    }
  }
  // O painel da direita: o perfil como vai ficar.
  { float cx = ox + 1360.0f;
    avatar(cx, oy + 400.0f, 200.0f, a);
    txt_bloco(TXT_ILHA_TITULO, nome_exibido(), 248, 249, 252, cx - 260.0f, oy + 530.0f, 520.0f, 46.0f, a, 1);
    txt_em(TXT_CAPTION, i18n("Perfil principal · desta TV"), cx - 260.0f, oy + 586.0f, 520.0f, 150, 156, 168, a);
    txt_bloco(TXT_CAPTION, i18n("Já usa o Nuvio em outra TV? No próximo passo, “Conta Nuvio” traz os perfis de lá."),
              150, 156, 168, cx - 260.0f, oy + 660.0f, 520.0f, 28.0f, 0.9f * a, 3); }
}

static void desenha_passo1(float a) {
  static const struct { const char *nome, *desc, *letra, *icone; int breve; } OP[4] = {
    { "Conta Stremio", "Add-ons na mesma ordem, biblioteca e Continuar assistindo", "S", NULL, 1 },
    { "AIOManager", "Sua configuração de AIOStreams e AIOMetadata, já pronta", NULL, "aj_layers", 1 },
    { "Conta Nuvio", "Perfis, Ajustes e salvos de outra TV", "N", NULL, 0 },
    { "Começar do zero", "Um conjunto pequeno de add-ons públicos para experimentar", NULL, "aj_sparkles", 0 },
  };
  Item it[MAX_ITENS];
  int n = montar(it), i;
  titulo(i18n("Traga o que você já usa"),
         i18n("Add-ons, catálogos e o que estava assistindo. Escolha uma; dá para juntar outra depois."), 860.0f, a);
  for (i = 0; i < n; i++) {
    GfxRect r = it[i].r;
    int id = it[i].id, f = foco == id;
    float al = OP[id < 4 ? id : 0].breve ? 0.5f * a : a;
    Tinta p, s;
    if (id >= 4) continue;
    superficie(r, 24.0f, f, a, &p, &s);
    bola(r.x + 26.0f, r.y + 20.0f, 64.0f, OP[id].letra, OP[id].icone, p, f, al);
    txt_em(TXT_ILHA_NOME, i18n(OP[id].nome), r.x + 114.0f, r.y + 20.0f, r.w - 300.0f, p.r, p.g, p.b, al);
    txt_em(TXT_CAPTION, i18n(OP[id].desc), r.x + 114.0f, r.y + 58.0f, r.w - 150.0f, s.r, s.g, s.b, al);
    if (OP[id].breve) {
      TxtLinha l = txt_linha(TXT_CAPTION, i18n("Em breve"), s.r, s.g, s.b, 255);
      txt_desenhar_alpha(l, r.x + r.w - 28.0f - (float)l.w, r.y + 20.0f, a);
    } else if (id == I_NUVIO || id == I_ZERO) {
      gfx_icone((GfxRect){ r.x + r.w - 28.0f - 26.0f, r.y + (r.h - 26.0f) * 0.5f, 26.0f, 26.0f }, "aj_chevron-right",
                (float)s.r / 255.0f, (float)s.g / 255.0f, (float)s.b / 255.0f, a);
    }
    alvo(r, id);
  }
  // O painel da direita explica a opcao em foco; as apagadas dizem a verdade.
  { float x = ox + 1100.0f, y = oy + 330.0f;
    const char *t, *d;
    if (foco == I_STREMIO || foco == I_AIO) {
      t = i18n(foco == I_STREMIO ? "Conta Stremio" : "AIOManager");
      d = i18n("Ainda não dá para trazer por aqui. Por enquanto, instale os add-ons em Ajustes › Fontes e addons.");
    } else if (foco == I_NUVIO) {
      t = i18n("Conta Nuvio");
      d = i18n("Você já entrou na sua conta Nuvio. Perfis, Ajustes e salvos de outras TVs chegam por ela.");
    } else {
      t = i18n("Começar do zero");
      d = i18n("Você segue com o que já está nesta TV e junta o resto quando quiser.");
    }
    gfx_cor((GfxRect){ x, y, 700.0f, 340.0f }, 32.0f / 340.0f, 0.12f, 0.125f, 0.145f, 0.9f * a);
    gfx_anel((GfxRect){ x, y, 700.0f, 340.0f }, 32.0f / 340.0f, 1.0f, 1, 1, 1, 0.08f * a);
    txt_em(TXT_ILHA_TITULO, t, x + 44.0f, y + 44.0f, 612.0f, 248, 249, 252, a);
    txt_bloco(TXT_V2_26, d, 200, 205, 215, x + 44.0f, y + 120.0f, 612.0f, 36.0f, 0.9f * a, 5); }
}

static void desenha_passo2(float a) {
  static const struct { const char *nome, *desc, *letra, *icone; } TK[3] = {
    { "Trakt", "Histórico, watchlist e Próximos episódios.", "T", NULL },
    { "Simkl", "Bom para anime. Entra pelo código no celular.", "S", NULL },
    { "Só nesta TV", "Sem conta externa. O progresso fica guardado neste perfil.", NULL, "aj_tv-minimal-play" },
  };
  Item it[MAX_ITENS];
  int n = montar(it), i, espera = 0;
  const char *cod = "", *url = "";
  titulo(i18n("Onde marcar o que você assiste"),
         i18n("O Nuvio avisa o tracker quando você termina um episódio e lê de lá o que falta ver. Pode ligar mais de um."),
         1100.0f, a);
  for (i = 0; i < n; i++) {
    GfxRect r = it[i].r;
    int id = it[i].id, f = foco == id;
    Tinta p, s;
    const char *st = NULL;
    int ligado = 0;
    if (id >= 3) continue;
    if (id == I_TRAKT) {
      TraEstado e = traktauth_estado();
      ligado = trakt_ativo();
      st = ligado ? "Ligado" : e == TRA_PEDINDO ? "Pedindo o código…" : e == TRA_AGUARDANDO ? "Esperando você autorizar"
         : (e == TRA_ERRO || e == TRA_INVALIDO) ? "Não deu certo. OK para tentar de novo." : "Aperte OK para ligar";
      if (!ligado && e == TRA_AGUARDANDO && !espera) { espera = 1; cod = traktauth_codigo(); url = traktauth_url(); }
    } else if (id == I_SIMKL) {
      SmkEstado e = simklauth_estado();
      ligado = simkl_ativo();
      st = ligado ? "Ligado" : e == SMK_PEDINDO ? "Pedindo o código…" : e == SMK_AGUARDANDO ? "Esperando você autorizar"
         : e == SMK_ERRO ? "Não deu certo. OK para tentar de novo." : "Aperte OK para ligar";
      if (!ligado && e == SMK_AGUARDANDO && !espera) { espera = 1; cod = simklauth_codigo(); url = simklauth_url(); }
    } else st = "Aperte OK para seguir sem conta";
    superficie(r, 28.0f, f, a, &p, &s);
    bola(r.x + 30.0f, r.y + 30.0f, 64.0f, TK[id].letra, TK[id].icone, p, f, a);
    txt_em(TXT_ILHA_TITULO, i18n(TK[id].nome), r.x + 30.0f, r.y + 120.0f, r.w - 60.0f, p.r, p.g, p.b, a);
    txt_bloco(TXT_CAPTION, i18n(TK[id].desc), s.r, s.g, s.b, r.x + 30.0f, r.y + 176.0f, r.w - 60.0f, 28.0f, a, 3);
    if (ligado) gfx_icone((GfxRect){ r.x + 30.0f, r.y + r.h - 30.0f - 24.0f, 24.0f, 24.0f }, "aj_circle-check",
                          (float)p.r / 255.0f, (float)p.g / 255.0f, (float)p.b / 255.0f, a);
    txt_em(TXT_CAPTION, i18n(st), r.x + 30.0f + (ligado ? 36.0f : 0.0f), r.y + r.h - 30.0f - 22.0f, r.w - 90.0f, p.r, p.g, p.b, a);
    alvo(r, id);
  }
  // O codigo do dispositivo: nao desenho QR, a pessoa digita no endereco.
  if (espera) {
    float x = ox + MARGEM, y = oy + 700.0f, w = 1680.0f;
    gfx_cor((GfxRect){ x, y, w, 190.0f }, 28.0f / 190.0f, 0.12f, 0.125f, 0.145f, 0.9f * a);
    gfx_anel((GfxRect){ x, y, w, 190.0f }, 28.0f / 190.0f, 1.0f, 1, 1, 1, 0.08f * a);
    txt_em(TXT_V2_26, i18n("Abra este endereço no celular e digite o código:"), x + 44.0f, y + 36.0f, 800.0f, 200, 205, 215, a);
    txt_em(TXT_ILHA_TITULO, url, x + 44.0f, y + 96.0f, 800.0f, 248, 249, 252, a);
    txt_tracking(TXT_NOV_TITULO, cod, 248, 249, 252, x + w - 44.0f - txt_tracking(TXT_NOV_TITULO, cod, 0, 0, 0, -1, 0, 0, 8.0f),
                 y + 56.0f, a, 8.0f);
  }
}

static void linha_resumo(float x, float y, float w, const char *t, const char *d, float a) {
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor((GfxRect){ x, y + 4.0f, 36.0f, 36.0f }, 0.5f, ar, ag, ab, a);
  gfx_icone((GfxRect){ x + 8.0f, y + 12.0f, 20.0f, 20.0f }, "aj_check", 1, 1, 1, a);
  txt_em(TXT_ILHA_NOME, t, x + 54.0f, y, w - 54.0f, 243, 242, 239, a);
  txt_bloco(TXT_CAPTION, d, 186, 192, 204, x + 54.0f, y + 36.0f, w - 54.0f, 26.0f, 0.92f * a, 2);
  gfx_cor((GfxRect){ x, y + 92.0f, w, 1.0f }, 0, 1, 1, 1, 0.07f * a);
}

static void desenha_passo3(float a) {
  char t[160], lig[64];
  int tr = trakt_ativo(), sk = simkl_ativo();
  snprintf(t, sizeof t, i18n("Pronto, %s."), nome_exibido());
  titulo(t, i18n("Isto ficou arrumado:"), 860.0f, a);
  snprintf(t, sizeof t, i18n("Perfil: %s"), nome_exibido());
  linha_resumo(ox + MARGEM, oy + 330.0f, 860.0f, t, i18n("Nome e cor guardados nesta TV."), a);
  snprintf(lig, sizeof lig, "%s", tr && sk ? i18n("Trakt e Simkl ligados") : tr ? i18n("Trakt ligado")
                                 : sk ? i18n("Simkl ligado") : i18n("Só nesta TV"));
  linha_resumo(ox + MARGEM, oy + 330.0f + 110.0f, 860.0f, lig,
               tr || sk ? i18n("Dá para mudar em Ajustes › Contas.") : i18n("Você liga o Trakt ou o Simkl depois, em Ajustes › Contas."), a);
  linha_resumo(ox + MARGEM, oy + 330.0f + 220.0f, 860.0f, i18n("Tour de 1 minuto"),
               i18n("Opcional: um passeio rápido pela Home, pela busca e pela Central de controle."), a);
  { float cx = ox + 1360.0f;
    avatar(cx, oy + 430.0f, 200.0f, a);
    txt_bloco(TXT_ILHA_TITULO, nome_exibido(), 248, 249, 252, cx - 260.0f, oy + 560.0f, 520.0f, 46.0f, a, 1); }
}

static void desenha_rodape(float a) {
  Item it[MAX_ITENS];
  int n = montar(it), i;
  for (i = 0; i < n; i++) {
    int id = it[i].id, prim = id == B_PRINC;
    const char *ic = NULL;
    if (id < B_VOLTAR) continue;
    if (passo == PASSOS - 1) ic = id == B_PRINC ? "aj_house" : "aj_sparkles";
    botao_pilula(it[i].r, rotulo_botao(id), ic, foco == id ? 1.0f : 0.0f, prim, 0, a);
    alvo(it[i].r, id);
  }
}

void comecar_desenhar(Uint32 agora) {
  float a = anim_suave(entrada);
  if (entrada < 0.002f) return;
  ESCALA_SE_COUBER_INI(Q_W, Q_H);
  origem();
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0.051f, 0.051f, 0.051f, entrada);
  cabecalho(a);
  if (passo == PASSOS - 1) desenha_passo3(a);
  else if (passo == 0) desenha_passo0(a);
  else if (passo == 1) desenha_passo1(a);
  else desenha_passo2(a);
  desenha_rodape(a);
  ESCALA_SE_COUBER_FIM();
  // O teclado da TV cuida da propria escala e vai por cima de tudo.
  if (esperaTeclado && teclado_aberto()) teclado_desenhar(agora);
}
