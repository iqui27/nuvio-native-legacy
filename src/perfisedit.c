// Gerenciar perfis — ver perfisedit.h para o porquê do modal.
#include "perfisedit.h"
#include "perfis.h"
#include "teclado.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include "ajustes.h"
#include "ponteiro.h"
#include "plrui.h"
#include "sync.h"
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

// AS 16 CORES DO APP OFICIAL (ProfileModels.kt, PROFILE_COLORS): o perfil
// criado aqui pode ser editado no celular amanha, e a paleta tem de ser a
// mesma para a cor nao "mudar sozinha" entre aparelhos.
static const char *CORES[16] = {
  "#1E88E5", "#E53935", "#43A047", "#FB8C00",
  "#8E24AA", "#00ACC1", "#F4511E", "#3949AB",
  "#C0CA33", "#D81B60", "#00897B", "#5E35B1",
  "#7CB342", "#039BE5", "#FFB300", "#6D4C41",
};

#define PEX_LARG       820.0f
#define PEX_PAD         44.0f
#define PEX_TITULO_Y    52.0f
#define PEX_LINHA_H     84.0f
#define PEX_LINHA_GAP   10.0f
#define PEX_COR_LADO    60.0f
#define PEX_COR_GAP     16.0f
#define PEX_COR_COLS     8
#define PEX_AVATAR       48.0f
#define PEX_MOLA       NV_MOLA_TELA

typedef enum { PEX_LISTA, PEX_ACAO, PEX_COR, PEX_CONFIRMA, PEX_SALVANDO, PEX_ERRO } PEXEstado;

static PEXEstado est;
static int aberto;
static int foco;            // linha da lista / da acao / celula da cor / da confirma
static int alvo;            // slot do perfil em edicao (-1: o perfil NOVO)
static char nomeNovo[64];   // o que o teclado deu (adicionar) ou o renomeado
static char corNova[10];    // cor escolhida (adicionar)
static int tecladoPara;     // 1 = o teclado aberto e deste modal
static float anim;          // 0..1: entrada do modal
static float animFoco[CONTA_PERFIL_MAX + 1];
static float animFocoAcao[3];
static float animFocoCor[16];
static float animFocoConfirma[2];

// O FIO DA REDE. push e delete BLOQUEIAM (perfis.c) e o re-pull junto: sem um
// fio, o modal congelava a tela inteira por toda a viagem. O molde e o do PIN
// em perfilsel.c: geracao invalida o resultado de um fio que chegou atrasado.
typedef struct { PerfisPush l[CONTA_PERFIL_MAX]; int n; int apagar; int indice; } PexTarefa;
static PexTarefa tarefa;
static pthread_t fio;
static int fioVivo;
static _Atomic int pexResultado;   // 0 pendente, 1 ok, -1 sem rede
static _Atomic unsigned pexGeracao;

static int podeAdicionar(void) {
  return perfis_n() >= 0 && perfis_n() < CONTA_PERFIL_MAX && perfis_indice_livre() > 0;
}
// Remover o UNICO perfil deixaria a conta sem ninguem para escolher; a TV
// nao oferece o que o app oficial tambem nao deixa fazer sem(confirmar).
static int podeRemover(void) { return perfis_n() > 1; }

void perfisedit_abrir(int adicionar) {
  int i;
  est = PEX_LISTA;
  aberto = 1;
  foco = 0;
  alvo = -1;
  tecladoPara = 0;
  anim = 0.0f;
  nomeNovo[0] = corNova[0] = 0;
  for (i = 0; i < CONTA_PERFIL_MAX + 1; i++) animFoco[i] = 0.0f;
  for (i = 0; i < 3; i++) animFocoAcao[i] = 0.0f;
  for (i = 0; i < 16; i++) animFocoCor[i] = 0.0f;
  for (i = 0; i < 2; i++) animFocoConfirma[i] = 0.0f;
  if (adicionar && podeAdicionar()) {
    tecladoPara = 1;
    teclado_abrir_com("Nome do perfil", "", 24, NULL, NULL);
  }
}
int perfisedit_aberto(void) { return aberto; }
int perfisedit_foco(void) { return foco; }

void perfisedit_fechar(void) {
  if (!aberto) return;
  // O teclado fechando junto nao pode "confirmar" nada depois daqui.
  tecladoPara = 0;
  if (teclado_aberto()) {
    SDL_Event e = { 0 };
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE;
    teclado_evento(&e);
  }
  atomic_fetch_add(&pexGeracao, 1);
  fioVivo = 0;
  aberto = 0;
}

// --- a rede -----------------------------------------------------------------

static void *fioTarefa(void *u) {
  PexTarefa *t = u;
  unsigned ger = atomic_load(&pexGeracao);
  int ok = t->apagar ? perfis_remover(t->indice) : perfis_push_lista(t->l, t->n);
  if (ok == 1) perfis_puxar();   // a lista da tela e o disco saem do servidor
  if (atomic_load(&pexGeracao) == ger)
    atomic_store(&pexResultado, ok == 1 ? 1 : -1);
  return NULL;
}

// Envia (num fio) a lista `l` — criar, renomear ou recolorir — ou o apagar.
static void ir(PexTarefa *t) {
  tarefa = *t;
  atomic_store(&pexResultado, 0);
  atomic_fetch_add(&pexGeracao, 1);
  est = PEX_SALVANDO;
  fioVivo = 1;
  if (pthread_create(&fio, NULL, fioTarefa, &tarefa) == 0) pthread_detach(fio);
  else { fioVivo = 0; est = PEX_ERRO; }
}

static void salvarLista(const PerfisPush *l, int n) {
  PexTarefa t;
  memset(&t, 0, sizeof t);
  memcpy(t.l, l, (size_t)n * sizeof *l);
  t.n = n; t.apagar = 0;
  ir(&t);
}

static void apagarPerfil(int indice) {
  PexTarefa t;
  memset(&t, 0, sizeof t);
  t.apagar = 1; t.indice = indice;
  ir(&t);
}

// A lista ATUAL com uma edicao aplicada. `slot` -1 = acrescentar no fim.
static int montar(PerfisPush *l, int slot, const char *nome, const char *cor) {
  int n = perfis_push_atuais(l, CONTA_PERFIL_MAX);
  if (slot >= 0 && slot < n) {
    if (nome) snprintf(l[slot].nome, sizeof l[slot].nome, "%s", nome);
    if (cor) snprintf(l[slot].corHex, sizeof l[slot].corHex, "%s", cor);
  } else if (slot < 0 && n < CONTA_PERFIL_MAX) {
    memset(&l[n], 0, sizeof l[n]);
    l[n].indice = perfis_indice_livre();
    snprintf(l[n].nome, sizeof l[n].nome, "%s", nome ? nome : "");
    snprintf(l[n].corHex, sizeof l[n].corHex, "%s", cor ? cor : "#1E88E5");
    n++;
  }
  return n;
}

// O TECLADO: o resultado e CONSUMIDO NA LEITURA — atender uma vez, aqui.
static void atenderTeclado(void) {
  if (!tecladoPara || teclado_aberto()) return;
  { int r = teclado_resultado();
    tecladoPara = 0;
    if (r != TECLADO_PRONTO) {
      // Cancelou no nome de um perfil NOVO: nada existe ainda, volta a lista.
      if (alvo == -1 && est == PEX_LISTA) return;
      return;
    }
    { const char *t = teclado_texto();
      if (!t || !t[0]) return;                     // nome vazio nao salva nada
      snprintf(nomeNovo, sizeof nomeNovo, "%s", t); }
    if (alvo >= 0) {
      // RENOMEAR: a cor continua a da conta, so o nome muda.
      PerfisPush l[CONTA_PERFIL_MAX];
      salvarLista(l, montar(l, alvo, nomeNovo, NULL));
    } else {
      // ADICIONAR: o nome veio, falta a cor.
      foco = 0;
      est = PEX_COR;
    } }
}

// --- teclas -----------------------------------------------------------------

static void focarLista(int i, int b) {
  (void)b;
  if (!aberto || est != PEX_LISTA) return;
  { int linhas = perfis_n() + (podeAdicionar() ? 1 : 0);
    if (i >= 0 && i < linhas && i != foco) foco = i; }
}
static void focarAcao(int i, int b) {
  (void)b;
  if (!aberto || est != PEX_ACAO) return;
  if (i >= 0 && i < 3 && i != foco) foco = i;
}
static void focarCor(int i, int b) {
  (void)b;
  if (!aberto || est != PEX_COR) return;
  if (i >= 0 && i < 16 && i != foco) foco = i;
}
static void focarConfirma(int i, int b) {
  (void)b;
  if (!aberto || est != PEX_CONFIRMA) return;
  if (i >= 0 && i < 2 && i != foco) foco = i;
}

void perfisedit_evento(const SDL_Event *e) {
  SDL_Keycode k;
  int linhas, acoes;
  if (!aberto) return;
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;

  // O TECLADO E DONO DO TECLADO enquanto existe (o molde de biblioteca.c).
  if (teclado_aberto()) { teclado_evento(e); return; }

  // Salvando: nenhuma tecla muda nada — o fio responde sozinho.
  if (est == PEX_SALVANDO) return;

  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK) {
    if (est == PEX_ERRO) { est = PEX_LISTA; return; }
    perfisedit_fechar();
    return;
  }
  if (est == PEX_ERRO) {
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) est = PEX_LISTA;
    return;
  }

  linhas = perfis_n() + (podeAdicionar() ? 1 : 0);
  switch (est) {
    case PEX_LISTA:
      if (k == SDLK_UP)        { if (foco > 0) foco--; }
      else if (k == SDLK_DOWN) { if (foco < linhas - 1) foco++; }
      else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        if (foco >= perfis_n()) {
          // ADICIONAR: o nome primeiro, a cor vem depois dele.
          if (podeAdicionar()) {
            alvo = -1;
            tecladoPara = 1;
            teclado_abrir_com("Nome do perfil", "", 24, NULL, NULL);
          }
        } else { alvo = foco; foco = 0; est = PEX_ACAO; }
      }
      break;
    case PEX_ACAO:
      acoes = podeRemover() ? 3 : 2;
      if (k == SDLK_UP)        { if (foco > 0) foco--; }
      else if (k == SDLK_DOWN) { if (foco < acoes - 1) foco++; }
      else if (k == SDLK_BACKSPACE) { est = PEX_LISTA; foco = alvo >= 0 && alvo < perfis_n() ? alvo : 0; }
      else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        const ContaPerfil *p = perfis_item(alvo);
        if (foco == 0) {          // Renomear
          tecladoPara = 1;
          teclado_abrir_com("Nome do perfil", "", 24, NULL, p ? p->nome : NULL);
        } else if (foco == 1) {   // Cor
          est = PEX_COR; foco = 0;
        } else if (podeRemover()) {
          est = PEX_CONFIRMA; foco = 0;
        }
      }
      break;
    case PEX_COR:
      if (k == SDLK_LEFT)       { if (foco % PEX_COR_COLS > 0) foco--; }
      else if (k == SDLK_RIGHT) { if (foco % PEX_COR_COLS < PEX_COR_COLS - 1 && foco < 15) foco++; }
      else if (k == SDLK_UP)    { if (foco >= PEX_COR_COLS) foco -= PEX_COR_COLS; }
      else if (k == SDLK_DOWN)  { if (foco + PEX_COR_COLS < 16) foco += PEX_COR_COLS; }
      else if (k == SDLK_BACKSPACE) {
        est = alvo >= 0 ? PEX_ACAO : PEX_LISTA;   // adicionar cancelado
      }
      else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        PerfisPush l[CONTA_PERFIL_MAX];
        int n;
        snprintf(corNova, sizeof corNova, "%s", CORES[foco]);
        if (alvo >= 0) n = montar(l, alvo, NULL, corNova);
        else n = montar(l, -1, nomeNovo[0] ? nomeNovo : "Perfil", corNova);
        salvarLista(l, n);
      }
      break;
    case PEX_CONFIRMA:
      if (k == SDLK_UP)        { if (foco > 0) foco--; }
      else if (k == SDLK_DOWN) { if (foco < 1) foco++; }
      else if (k == SDLK_BACKSPACE) { est = PEX_ACAO; foco = 1; }
      else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        if (foco == 0) {
          const ContaPerfil *p = perfis_item(alvo);
          if (p) apagarPerfil(p->indice);
        } else { est = PEX_ACAO; foco = 1; }
      }
      break;
    default:
      break;
  }
}

void perfisedit_atualizar(float dt, Uint32 agora) {
  int i;
  int reduzida = ajustes_animacoes_reduzidas();
  (void)agora;
  if (!aberto) return;
  // O TECLADO DESTA MODAL precisa do seu relogio de entrada (anim) andando —
  // parado em 0 ele existe mas nao desenha, e o OK parecia sem resposta.
  if (teclado_aberto()) teclado_atualizar(dt, agora);
  atenderTeclado();
  anim = anim_reduzida(anim_mola(anim, 1.0f, dt, PEX_MOLA), 1.0f, reduzida);
  { int linhas = perfis_n() + 1;
    for (i = 0; i < linhas && i < CONTA_PERFIL_MAX + 1; i++) {
      float alvo = est == PEX_LISTA && i == foco ? 1.0f : 0.0f;
      animFoco[i] = anim_reduzida(anim_mola(animFoco[i], alvo, dt, NV_MOLA_FOCO), alvo, reduzida);
    } }
  for (i = 0; i < 3; i++) {
    float alvo = est == PEX_ACAO && i == foco ? 1.0f : 0.0f;
    animFocoAcao[i] = anim_reduzida(anim_mola(animFocoAcao[i], alvo, dt, NV_MOLA_FOCO), alvo, reduzida);
  }
  for (i = 0; i < 16; i++) {
    float alvo = est == PEX_COR && i == foco ? 1.0f : 0.0f;
    animFocoCor[i] = anim_reduzida(anim_mola(animFocoCor[i], alvo, dt, NV_MOLA_FOCO), alvo, reduzida);
  }
  for (i = 0; i < 2; i++) {
    float alvo = est == PEX_CONFIRMA && i == foco ? 1.0f : 0.0f;
    animFocoConfirma[i] = anim_reduzida(anim_mola(animFocoConfirma[i], alvo, dt, NV_MOLA_FOCO), alvo, reduzida);
  }
  // O FIO RESPONDEU. 1: a lista nova ja esta em memoria (o fio re-puxou) e no
  // disco; -1: nada foi gravado, o aviso diz por que.
  { int r = atomic_load(&pexResultado);
  if (fioVivo && r) {
    fioVivo = 0;
    atomic_store(&pexResultado, 0);
    if (r == 1) {
      // Quem foi apagado pode ser o perfil ATIVO: a escolha de sempre e o
      // primeiro que sobrou, gravado no disco, no fio do quadro.
      if (!perfis_item_ativo() && perfis_n() > 0)
        perfis_definir_ativo(perfis_item(0)->indice);
      est = PEX_LISTA;
      if (foco >= perfis_n() + 1) foco = perfis_n() > 0 ? perfis_n() - 1 : 0;
      alvo = -1;
    } else est = PEX_ERRO;
  } }
}

// --- desenho ----------------------------------------------------------------

static void corDe(const char *hex, float *r, float *g, float *b) {
  unsigned v = 0;
  *r = 0.12f; *g = 0.53f; *b = 0.90f;
  if (!hex || hex[0] != '#' || strlen(hex) < 7) return;
  if (sscanf(hex + 1, "%6x", &v) != 1) return;
  *r = ((v >> 16) & 255) / 255.0f;
  *g = ((v >> 8) & 255) / 255.0f;
  *b = (v & 255) / 255.0f;
}

// A inicial do nome, respeitando UTF-8 (o mesmo recado de perfilsel.c).
static void inicialDe(const char *nome, char *dst, size_t tam) {
  size_t z = 1;
  if (tam < 5) { if (tam) dst[0] = 0; return; }
  if (!nome || !nome[0]) { dst[0] = '?'; dst[1] = 0; return; }
  while (z < 4 && (nome[z] & 0xc0) == 0x80) z++;
  memcpy(dst, nome, z);
  dst[z] = 0;
}

static void linhaDe(GfxRect r, int focada) {
  if (focada) plrui_linha_foco(r, 20.0f, anim);
  else gfx_cor(r, 20.0f / r.h, 1.0f, 1.0f, 1.0f, 0.06f * anim);
}

void perfisedit_desenhar(Uint32 agora) {
  float a = anim;
  int m = perfis_n(), i;
  int linhas, h;
  GfxRect modal, tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  (void)agora;
  if (!aberto || a < 0.004f) return;

  // Camada modal: o toque embaixo nao escolhe perfil nenhum.
  ponteiro_camada();
  ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, NULL, 0, 0);
  gfx_cor(tela, 0.0f, 0.02f, 0.02f, 0.025f, 0.88f * a);

  linhas = m + (podeAdicionar() ? 1 : 0);
  if (est == PEX_COR) h = 300.0f;
  else if (est == PEX_CONFIRMA) h = 380.0f;
  else if (est == PEX_SALVANDO || est == PEX_ERRO) h = 260.0f;
  else h = PEX_TITULO_Y + 64.0f + (float)linhas * (PEX_LINHA_H + PEX_LINHA_GAP) + PEX_PAD;
  modal.x = (NV_TELA_W - PEX_LARG) * 0.5f;
  modal.y = (NV_TELA_H - h) * 0.5f;
  modal.w = PEX_LARG;
  modal.h = h;
  { GfxRect sombra = { modal.x - 30.0f, modal.y - 14.0f, modal.w + 60.0f, modal.h + 60.0f };
    gfx_rect(sombra, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, 0.60f * a); }
  plrui_material(modal, 24.0f, 1, a);

  if (est == PEX_SALVANDO || est == PEX_ERRO) {
    const char *msg = est == PEX_SALVANDO ? "Salvando..."
                                         : "Sem conexão. Tente novamente.";
    TxtLinha l = txt_linha(TXT_TITULO3, msg, est == PEX_SALVANDO ? 226 : 236,
                           est == PEX_SALVANDO ? 228 : 108,
                           est == PEX_SALVANDO ? 236 : 108, 255);
    txt_desenhar_alpha(l, modal.x + (modal.w - l.w) * 0.5f,
                       modal.y + (h - (float)l.h) * 0.5f, a);
    return;
  }

  if (est == PEX_LISTA || est == PEX_ACAO) {
    const char *titulo = est == PEX_LISTA ? "Gerenciar perfis"
                            : perfis_item(alvo) ? perfis_item(alvo)->nome : "";
    TxtLinha t = txt_linha_corta(TXT_TITULO2, titulo, 247, 248, 250, 255,
                                 PEX_LARG - PEX_PAD * 2.0f);
    txt_desenhar_alpha(t, modal.x + PEX_PAD, modal.y + PEX_TITULO_Y, a);
  }

  if (est == PEX_LISTA) {
    float y = modal.y + PEX_TITULO_Y + 64.0f;
    for (i = 0; i < linhas; i++) {
      GfxRect r = { modal.x + PEX_PAD, y, PEX_LARG - PEX_PAD * 2.0f, PEX_LINHA_H };
      linhaDe(r, i == foco);
      if (i < m) {
        const ContaPerfil *p = perfis_item(i);
        float cr, cg, cb;
        char ini[8];
        TxtLinha iniL;
        GfxRect av = { r.x + 18.0f, r.y + (r.h - PEX_AVATAR) * 0.5f,
                       PEX_AVATAR, PEX_AVATAR };
        corDe(p ? p->corHex : NULL, &cr, &cg, &cb);
        gfx_rect(av, 0, GFX_DISCO, 0, 0, 0, 0, cr * 0.9f, cg * 0.9f, cb * 0.9f, a);
        inicialDe(p ? p->nome : NULL, ini, sizeof ini);
        iniL = txt_linha(TXT_V2_KBD18, ini, 255, 255, 255, 255);
        txt_desenhar_alpha(iniL, av.x + (av.w - iniL.w) * 0.5f,
                           av.y + (av.h - iniL.h) * 0.5f, a);
        { TxtLinha nome = txt_linha_corta(TXT_TITULO3, p ? p->nome : "",
                                          247, 248, 250, 255, r.w - 220.0f);
          txt_desenhar_alpha(nome, r.x + 18.0f + PEX_AVATAR + 20.0f,
                             r.y + (r.h - (float)nome.h) * 0.5f, a); }
        if (p && p->temPin) {
          TxtLinha sel = txt_linha(TXT_CAPTION, "PIN", 226, 228, 236, 255);
          GfxRect pil = { r.x + r.w - (float)sel.w - 46.0f,
                          r.y + (r.h - (float)sel.h - 10.0f) * 0.5f,
                          (float)sel.w + 32.0f, (float)sel.h + 10.0f };
          gfx_cor(pil, NV_RAIO_PILL, 1.0f, 1.0f, 1.0f, (0.10f + 0.10f * animFoco[i]) * a);
          txt_desenhar_alpha(sel, pil.x + 16.0f, pil.y + 5.0f, a);
        }
      } else {
        // O "+": o mesmo disco escuro dos avatares sem foto, com o sinal.
        GfxRect av = { r.x + 18.0f, r.y + (r.h - PEX_AVATAR) * 0.5f,
                       PEX_AVATAR, PEX_AVATAR };
        float ar, ag, ab;
        gfx_rect(av, 0, GFX_DISCO, 0, 0, 0, 0, 0.15f, 0.16f, 0.18f, a);
        ajustes_acento(&ar, &ag, &ab);
        gfx_cor((GfxRect){ av.x + av.w * 0.5f - 14.0f, av.y + av.h * 0.5f - 2.0f,
                           28.0f, 4.0f }, 0.5f, ar, ag, ab, 0.90f * a);
        gfx_cor((GfxRect){ av.x + av.w * 0.5f - 2.0f, av.y + av.h * 0.5f - 14.0f,
                           4.0f, 28.0f }, 0.5f, ar, ag, ab, 0.90f * a);
        { TxtLinha nome = txt_linha(TXT_TITULO3, "Adicionar perfil",
                                    226, 228, 236, 255);
          txt_desenhar_alpha(nome, r.x + 18.0f + PEX_AVATAR + 20.0f,
                             r.y + (r.h - (float)nome.h) * 0.5f, a); }
      }
      if (!fioVivo)
        ponteiro_alvo(r.x, r.y, r.w, r.h, focarLista, NULL, i, 0);
      y += PEX_LINHA_H + PEX_LINHA_GAP;
    }
    // O TECLADO DO NOME, por cima do modal (o dono dele e esta tela).
    if (teclado_aberto()) teclado_desenhar(agora);
    return;
  }

  if (est == PEX_ACAO) {
    // As tres linhas de UM perfil. "Remover" so existe com mais de um.
    static const char *ROT[3] = { "Renomear", "Cor", "Remover" };
    int acoes = podeRemover() ? 3 : 2;
    float y = modal.y + PEX_TITULO_Y + 64.0f;
    for (i = 0; i < acoes; i++) {
      GfxRect r = { modal.x + PEX_PAD, y, PEX_LARG - PEX_PAD * 2.0f, PEX_LINHA_H };
      TxtLinha l;
      linhaDe(r, i == foco);
      l = txt_linha(TXT_TITULO3, ROT[i],
                    i == 2 ? 236 : 247, i == 2 ? 108 : 248, i == 2 ? 108 : 250, 255);
      txt_desenhar_alpha(l, r.x + 22.0f, r.y + (r.h - (float)l.h) * 0.5f, a);
      ponteiro_alvo(r.x, r.y, r.w, r.h, focarAcao, NULL, i, 0);
      y += PEX_LINHA_H + PEX_LINHA_GAP;
    }
    if (teclado_aberto()) teclado_desenhar(agora);
    return;
  }

  if (est == PEX_COR) {
    TxtLinha t = txt_linha(TXT_TITULO2, "Cor", 247, 248, 250, 255);
    float gw = PEX_COR_COLS * PEX_COR_LADO + (PEX_COR_COLS - 1) * PEX_COR_GAP;
    float gx = modal.x + (PEX_LARG - gw) * 0.5f;
    float gy = modal.y + (h - (2.0f * PEX_COR_LADO + PEX_COR_GAP)) * 0.5f + 24.0f;
    txt_desenhar_alpha(t, modal.x + (PEX_LARG - (float)t.w) * 0.5f, modal.y + PEX_TITULO_Y, a);
    for (i = 0; i < 16; i++) {
      float cr, cg, cb, f = animFocoCor[i];
      GfxRect c = { gx + (float)(i % PEX_COR_COLS) * (PEX_COR_LADO + PEX_COR_GAP),
                    gy + (float)(i / PEX_COR_COLS) * (PEX_COR_LADO + PEX_COR_GAP),
                    PEX_COR_LADO, PEX_COR_LADO };
      corDe(CORES[i], &cr, &cg, &cb);
      // FOCO EM SUPERFICIE (a nota de NV_COR_FOCO em layout.h): o quadro
      // branco por fora, e nao uma celula colorida diferente.
      if (f > 0.02f) {
        float e = NV_ANEL_FOCO * f;
        gfx_cor((GfxRect){ c.x - e, c.y - e, c.w + e * 2, c.h + e * 2 },
                0.18f, 0.97f, 0.97f, 0.99f, f * a);
      }
      gfx_cor(c, 0.18f, cr, cg, cb, a);
      // A cor ATUAL do perfil (quem edita) leva o ponto branco, para a pessoa
      // achar a dela na paleta.
      { const ContaPerfil *p = alvo >= 0 ? perfis_item(alvo) : NULL;
        if (p && !strcmp(p->corHex, CORES[i]))
          gfx_cor((GfxRect){ c.x + c.w * 0.5f - 5.0f, c.y + c.h - 16.0f, 10.0f, 10.0f },
                  0.5f, 1, 1, 1, 0.95f * a); }
      ponteiro_alvo(c.x, c.y, c.w, c.h, focarCor, NULL, i, 0);
    }
    return;
  }

  if (est == PEX_CONFIRMA) {
    const ContaPerfil *p = perfis_item(alvo);
    char frase[160];
    TxtLinha t;
    float y = modal.y + PEX_TITULO_Y + 64.0f;
    snprintf(frase, sizeof frase, i18n("Remover %s? O progresso dele sai da conta."),
             p && p->nome[0] ? p->nome : "");
    t = txt_linha_corta(TXT_BODY, frase, 236, 224, 208, 255, PEX_LARG - PEX_PAD * 2.0f);
    txt_desenhar_alpha(t, modal.x + PEX_PAD, y, a);
    y += (float)t.h + 30.0f;
    { static const char *ROT[2] = { "Remover", "Cancelar" };
      for (i = 0; i < 2; i++) {
        GfxRect r = { modal.x + PEX_PAD, y, PEX_LARG - PEX_PAD * 2.0f, PEX_LINHA_H };
        TxtLinha l;
        linhaDe(r, i == foco);
        l = txt_linha(TXT_TITULO3, ROT[i], i == 0 ? 236 : 247,
                      i == 0 ? 108 : 248, i == 0 ? 108 : 250, 255);
        txt_desenhar_alpha(l, r.x + 22.0f, r.y + (r.h - (float)l.h) * 0.5f, a);
        ponteiro_alvo(r.x, r.y, r.w, r.h, focarConfirma, NULL, i, 0);
        y += PEX_LINHA_H + PEX_LINHA_GAP;
      } }
  }
}
