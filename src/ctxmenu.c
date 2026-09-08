#include "ctxmenu.h"
#include "catalogo.h"
#include "trakt.h"
#include "extras.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "layout.h"
#include "anim.h"
#include "ajustes.h"
#include "progresso.h"
#include <stdio.h>
#include <string.h>

// Mantem o header publico de Trakt estavel: estas leituras sao o contrato
// interno entre a modal e as escritas assincronas do proprio port.
extern int trakt_operacao_estado(int tipo);
extern int trakt_watchlist_tipo(const char *imdb, const char *tipo, int adicionar);
extern int trakt_assistido_tipo(const char *imdb, const char *tipo, int marcar);
extern int cat_historico_estado_item(int indice);
extern void cat_historico_definir_id(const char *imdb, const char *tipo, int visto);

enum { CTX_OP_NENHUMA, CTX_OP_LISTA = 1, CTX_OP_HISTORICO = 2 };
enum { CTX_PENDENTE = 1, CTX_CONFIRMADA = 2, CTX_FALHA = 3 };

// MEDIDO no bundle 1.0.4: o dialogo tem 37,5vw de largura (720 px em 1920).
#define CTX_W      720.0f
#define CTX_PAD     44.0f
#define CTX_LINHA   86.0f     // altura de cada botao
#define CTX_GAP     12.0f
#define CTX_CAB    148.0f     // titulo, estados e rotulo do grupo
#define CTX_STATUS_H 34.0f
#define CTX_RODAPE  70.0f

static int   aberto, idx = -1, foco, pedDetalhes = -1;
static float anim;
static int   operacao, intencao, estadoOperacao;
static int   espelhoAplicado;
static char  operacaoImdb[16];
static volatile int holdAtivo, holdCancelado, holdPronto;
// O OK QUE ABRIU O MODAL AINDA ESTA AFUNDADO.
//
// O menu do cartaz abre NO LIMIAR, com o dedo ainda no botao (home.c dispara
// em home_atualizar, nao no KEYUP) — e isso e de proposito: esperar a soltura
// faria a barra encher na tela sem nada acontecer. O preco e que a repeticao
// automatica do controle continua mandando KEYDOWN de OK, e o modal recem-
// aberto os tratava como escolha: "quando abre o modal e eu ainda estou
// segurando, ele ja clica sozinho".
//
// Enquanto esta marca vale, OK nao escolhe nada aqui. Ela cai no primeiro
// KEYUP de OK — ou seja, exige um toque NOVO, que e o que o dono espera.
static volatile int esperandoSoltura;
static Uint32 holdDesde;

static int teclaOk(SDL_Keycode k) {
  return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE;
}

// A home e quem conhece o item focado, por isso ela continua decidindo qual
// indice entregar a ctx_abrir no KEYUP. Este observador fornece o feedback
// durante a retenção e arma a janela longa; setas/Voltar invalidam o gesto
// antes que a home possa transformá-lo em ação.
static int observarHold(void *u, SDL_Event *e) {
  (void)u;
  if (e->type == SDL_KEYDOWN) {
    SDL_Keycode k = e->key.keysym.sym;
    if (teclaOk(k) && !e->key.repeat) {
      holdAtivo = 1;
      holdCancelado = 0;
      holdPronto = 0;
      holdDesde = SDL_GetTicks();
    } else if (holdAtivo &&
               (k == SDLK_UP || k == SDLK_DOWN || k == SDLK_LEFT ||
                k == SDLK_RIGHT || k == SDLK_AC_BACK || k == SDLK_ESCAPE ||
                k == SDLK_BACKSPACE || e->key.keysym.scancode == NV_SCANCODE_BACK)) {
      holdCancelado = 1;
    }
  } else if (e->type == SDL_KEYUP && teclaOk(e->key.keysym.sym)) {
    if (holdAtivo && !holdCancelado && SDL_GetTicks() - holdDesde >= NV_HOLD_MS)
      holdPronto = 1;
    holdAtivo = 0;
    esperandoSoltura = 0;
  }
  return 0;
}

// Ate tres: detalhes, biblioteca e — so em filme/serie — assistido.
#define CTX_MAX 3
static struct { const char *rot; int acao; } ops[CTX_MAX];
static int nOps;
static float focoAnim[CTX_MAX];
static int holdObservador;
enum { OP_DETALHES, OP_LISTA, OP_ASSISTIDO, OP_TIRAR_CONTINUAR };

static int indiceAtual(void) {
  int n = cat_n();
  int achado;
  if (n < 1 || idx < 0 || idx >= n) return -1;
  if (operacaoImdb[0]) {
    achado = cat_indice_por_imdb(operacaoImdb);
    // A resposta pode chegar depois de a descoberta trocar o bloco. Nunca
    // reutilizar `idx` nesse caso, pois ele pode ser outro titulo.
    return achado;
  }
  return idx;
}

static void montar(void) {
  int i = indiceAtual();
  const CatItem *ci = i >= 0 ? cat_item(i) : NULL;
  nOps = 0;
  if (!ci) return;
  ops[nOps].rot = "Ver detalhes";        ops[nOps].acao = OP_DETALHES;  nOps++;
  // Sem IMDb nao ha endpoint remoto suportado para esta acao. Nao oferecer
  // um botao que so aparentaria funcionar e inventaria estado local.
  if (ci->imdb[0]) {
    if (estadoOperacao == CTX_PENDENTE && operacao == CTX_OP_LISTA)
      ops[nOps].rot = intencao ? "Adicionando à biblioteca..."
                               : "Removendo da biblioteca...";
    else
      ops[nOps].rot = ci->naLista ? "Remover da biblioteca"
                                  : "Adicionar à biblioteca";
    ops[nOps].acao = OP_LISTA; nOps++;
  }
  // O web so oferece "assistido" em filme e serie — nao em canal nem evento,
  // que sao tipos que os addons do dono tambem declaram.
  if (ci->imdb[0] && (!strcmp(ci->tipo, "movie") || !strcmp(ci->tipo, "series"))) {
    if (estadoOperacao == CTX_PENDENTE && operacao == CTX_OP_HISTORICO)
      ops[nOps].rot = intencao ? "Marcando como assistido..."
                               : "Desmarcando como assistido...";
    else
      ops[nOps].rot = cat_historico_estado_item(i) == 1
                        ? "Desmarcar como assistido"
                        : "Marcar como assistido";
    ops[nOps].acao = OP_ASSISTIDO; nOps++;
  }
  // TIRAR DE "CONTINUAR ASSISTINDO".
  //
  // So aparece em item que TEM progresso — e o unico caso em que a acao quer
  // dizer alguma coisa, e oferecer em todo card poluiria o menu com um botao
  // que nao faz nada. `progresso` e o campo que a home usa para decidir se
  // desenha a barra, entao a condicao aqui e a mesma que poe o item na fileira.
  //
  // Distinta de "marcar como assistido": aquela e historico no Trakt e vale
  // para o titulo; esta apaga a POSICAO DE RETOMADA local, que e o que faz o
  // card aparecer na fileira. Quem terminou um filme quer as duas; quem
  // desistiu no meio quer so esta.
  if (ci->progresso > 0 && ci->imdb[0]) {
    ops[nOps].rot = "Tirar de Continuar assistindo";
    ops[nOps].acao = OP_TIRAR_CONTINUAR; nOps++;
  }
}

void ctx_abrir(int indice) {
  if (holdCancelado) {
    holdCancelado = 0;
    holdPronto = 0;
    return;
  }
  if (indice < 0 || indice >= cat_n() || !cat_item(indice)) return;
  // A longa ja consumiu o gesto na home. Limpar a sentinela aqui evita que o
  // KEYUP seguinte seja reaproveitado como uma selecao dentro da modal.
  holdPronto = 0;
  esperandoSoltura = 1;   // o OK que abriu ainda esta afundado; ver a nota acima
  idx = indice; foco = 0; aberto = 1; pedDetalhes = -1;
  operacao = CTX_OP_NENHUMA; intencao = 0; estadoOperacao = 0;
  espelhoAplicado = 0;
  operacaoImdb[0] = 0;
  memset(focoAnim, 0, sizeof focoAnim);
  montar();
}

int ctx_aberto(void) { return aberto; }
int ctx_pediu_detalhes(void) { int v = pedDetalhes; pedDetalhes = -1; return v; }

static void aplicar(void) {
  int atual = indiceAtual();
  const CatItem *ci = atual >= 0 ? cat_item(atual) : NULL;
  int acao;
  if (!ci || foco < 0 || foco >= nOps) return;
  acao = ops[foco].acao;
  if (acao != OP_DETALHES && operacao != CTX_OP_NENHUMA &&
      estadoOperacao != CTX_FALHA) return;
  switch (acao) {
    case OP_DETALHES: pedDetalhes = idx; break;
    case OP_LISTA:
      // Captura a intencao ANTES de qualquer escrita. O mesmo valor segue para
      // o POST e so chega ao espelho local depois de uma resposta 2xx.
      intencao = !ci->naLista;
      snprintf(operacaoImdb, sizeof operacaoImdb, "%s", ci->imdb);
      operacao = CTX_OP_LISTA;
      espelhoAplicado = 0;
      estadoOperacao = CTX_PENDENTE;
      if (!trakt_watchlist_tipo(ci->imdb, ci->tipo, intencao))
        estadoOperacao = CTX_FALHA;
      montar();
      break;
    case OP_ASSISTIDO:
      // Progresso e posicao de retomada, nao historico. So um retrato de
      // historico confirmado pode inverter a acao para "desmarcar".
      intencao = cat_historico_estado_item(atual) == 1 ? 0 : 1;
      snprintf(operacaoImdb, sizeof operacaoImdb, "%s", ci->imdb);
      operacao = CTX_OP_HISTORICO;
      espelhoAplicado = 0;
      estadoOperacao = CTX_PENDENTE;
      if (!trakt_assistido_tipo(ci->imdb, ci->tipo, intencao))
        estadoOperacao = CTX_FALHA;
      montar();
      break;
    case OP_TIRAR_CONTINUAR: {
      // A chave e montada do mesmo jeito que progresso.c monta ao gravar —
      // com temporada e episodio quando ha —, senao a linha apagada seria
      // outra e o card continuaria na fileira.
      char chave[192];
      prog_chave(chave, sizeof chave, ci->imdb, ci->temporada, ci->episodio);
      prog_remover(chave);
      // Efeito local e imediato: sem zerar o campo, o card so sairia da fileira
      // na proxima remontagem do catalogo, e para quem apertou parece que nada
      // aconteceu.
      cat_zerar_progresso(atual);
      aberto = 0;
      break;
    }
  }
  if (acao == OP_DETALHES) aberto = 0;
}

void ctx_evento(const SDL_Event *e) {
  int k;
  if (!aberto) return;
  // A SOLTURA VEM POR AQUI TAMBEM, e nao so pelo SDL_AddEventWatch de
  // observarHold: com o modal aberto, app.c entrega o evento a esta funcao e
  // nao ha garantia de que o watch tenha visto o mesmo KEYUP (as teclas
  // injetadas de /tmp/nuvio-key, por exemplo, nao passam pela fila do SDL).
  // Sem esta linha a marca nunca cairia por esse caminho e o modal ficaria
  // surdo ao OK.
  if (e->type == SDL_KEYUP && teclaOk(e->key.keysym.sym)) esperandoSoltura = 0;
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  // Repeticao automatica NUNCA e uma segunda escolha: quem quer clicar duas
  // vezes solta e aperta de novo.
  if (e->key.repeat && teclaOk(k)) return;
  if (esperandoSoltura && teclaOk(k)) return;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) { aberto = 0; return; }
  // Enquanto a requisicao esta no ar, OK nao repete a escrita. O foco continua
  // sendo o do modal e Voltar sempre pode cancelar a espera visual.
  if (operacao != CTX_OP_NENHUMA) {
    if (estadoOperacao == CTX_PENDENTE) return;
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
      if (estadoOperacao == CTX_FALHA) aplicar();
      else aberto = 0;
    }
    if (k != SDLK_UP && k != SDLK_DOWN) return;
  }
  if (k == SDLK_UP)   { if (foco > 0) foco--; return; }
  if (k == SDLK_DOWN) { if (foco + 1 < nOps) foco++; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) { aplicar(); return; }
}

void ctx_atualizar(float dt, Uint32 agora) {
  int i;
  int atual;
  if (!holdObservador) {
    SDL_AddEventWatch(observarHold, NULL);
    holdObservador = 1;
  }
  if (holdAtivo && agora - holdDesde >= NV_HOLD_MS) holdPronto = 1;
  if (ajustes_animacoes_reduzidas())
    anim = aberto ? 1.0f : 0.0f;
  else
    anim = anim_mola(anim, aberto ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  for (i = 0; i < CTX_MAX; i++)
    focoAnim[i] = ajustes_animacoes_reduzidas()
      ? (aberto && foco == i ? 1.0f : 0.0f)
      : anim_mola(focoAnim[i], aberto && foco == i ? 1.0f : 0.0f,
                  dt, NV_MOLA_FOCO);

  atual = indiceAtual();
  if (aberto && atual < 0) { aberto = 0; return; }

  if (operacao != CTX_OP_NENHUMA && estadoOperacao == CTX_PENDENTE) {
    int novo = trakt_operacao_estado(operacao);
    if (novo == CTX_CONFIRMADA || novo == CTX_FALHA) {
      estadoOperacao = novo;
      if (!espelhoAplicado && atual >= 0) {
        const CatItem *ci = cat_item(atual);
        if (ci && novo == CTX_CONFIRMADA) {
          if (operacao == CTX_OP_LISTA) {
            cat_definir_na_lista(atual, intencao);
          } else {
            cat_historico_definir_id(ci->imdb, ci->tipo, intencao);
          }
        }
        espelhoAplicado = 1;
        montar();
      }
    }
  }
}

void ctx_desenhar(Uint32 agora) {
  const CatItem *ci;
  const char *estados[2];
  const char *mensagem = NULL;
  float a = anim, alt, x, y;
  int i, nEstados = 1;
  (void)agora;
  if (!aberto && holdAtivo) {
    float p = (float)(SDL_GetTicks() - holdDesde) / (float)NV_HOLD_MS;
    TxtLinha t;
    if (p > 1.0f) p = 1.0f;
    t = txt_linha(TXT_CAPTION2,
                  p >= 1.0f ? "Solte para abrir opções" : "Segure OK para opções",
                  220, 224, 232, 255);
    txt_desenhar_alpha(t, (NV_TELA_W - t.w) * 0.5f, NV_TELA_H - 124.0f, 0.94f);
    gfx_cor((GfxRect){ (NV_TELA_W - 420.0f) * 0.5f, NV_TELA_H - 82.0f,
                       420.0f, 8.0f }, 4.0f, 0.18f, 0.2f, 0.23f, 0.96f);
    gfx_cor((GfxRect){ (NV_TELA_W - 420.0f) * 0.5f, NV_TELA_H - 82.0f,
                       420.0f * p, 8.0f }, 4.0f, 0.78f, 0.84f, 0.96f, 0.98f);
  }
  if (a < 0.01f) return;
  ci = indiceAtual() >= 0 ? cat_item(indiceAtual()) : NULL;
  if (!ci) return;

  if (estadoOperacao == CTX_PENDENTE)
    mensagem = operacao == CTX_OP_LISTA ? "Atualizando biblioteca..."
                                        : (intencao ? "Marcando como assistido..."
                                                    : "Desmarcando como assistido...");
  else if (estadoOperacao == CTX_CONFIRMADA)
    mensagem = operacao == CTX_OP_LISTA ? "Biblioteca atualizada"
                                        : (intencao ? "Marcado como assistido"
                                                    : "Desmarcado como assistido");
  else if (estadoOperacao == CTX_FALHA)
    mensagem = "Não foi possível atualizar. Tente novamente.";

  estados[0] = ci->naLista ? "Na biblioteca" : "Fora da biblioteca";
  if (!strcmp(ci->tipo, "movie") || !strcmp(ci->tipo, "series")) {
    { int historico = cat_historico_estado_item(indiceAtual());
      estados[1] = historico == 1 ? "Assistido"
                   : historico == 0 ? "Não assistido"
                   : ci->progresso > 0 ? "Progresso salvo"
                   : "Histórico não consultado"; }
    nEstados = 2;
  }

  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, 0, 0, 0, 0.72f * a); }

  alt = CTX_PAD * 2.0f + CTX_CAB +
        (float)nOps * (CTX_LINHA + CTX_GAP) - CTX_GAP + CTX_RODAPE;
  x = (NV_TELA_W - CTX_W) * 0.5f;
  y = (NV_TELA_H - alt) * 0.5f;
  // Sobe do fundo enquanto aparece, como as outras folhas do app.
  y += (1.0f - a) * 40.0f;

  { GfxRect p = { x, y, CTX_W, alt };
    gfx_cor(p, 0.06f, 0.11f, 0.11f, 0.13f, 0.98f * a); }

  { TxtLinha t = txt_linha(TXT_CAPTION2, "TÍTULO SELECIONADO", 174, 178, 188, 255);
    txt_desenhar_alpha(t, x + CTX_PAD, y + CTX_PAD, a * 0.95f); }
  { TxtLinha t = txt_linha_corta(TXT_HEADLINE, ci->titulo, 245, 248, 255, 255,
                                 CTX_W - CTX_PAD * 2.0f);
    txt_desenhar_alpha(t, x + CTX_PAD, y + CTX_PAD + 28.0f, a); }
  { const char *subtitulo = mensagem ? mensagem : "Opções do título";
    TxtLinha t = txt_linha(TXT_DET_META2, subtitulo, 150, 154, 163, 255);
    txt_desenhar_alpha(t, x + CTX_PAD, y + CTX_PAD + 70.0f, a * 0.9f); }

  { float sx = x + CTX_PAD;
    float sy = y + CTX_PAD + 104.0f;
    for (i = 0; i < nEstados; i++) {
      TxtLinha t = txt_linha(TXT_CAPTION2, estados[i], 215, 218, 225, 255);
      float sw = t.w + 24.0f;
      gfx_cor((GfxRect){ sx, sy, sw, CTX_STATUS_H }, 0.5f,
              0.16f, 0.17f, 0.19f, 0.96f * a);
      txt_desenhar_alpha(t, sx + 12.0f,
                         sy + (CTX_STATUS_H - t.h) * 0.5f, a);
      sx += sw + CTX_GAP;
    } }

  for (i = 0; i < nOps; i++) {
    float by = y + CTX_PAD + CTX_CAB + (float)i * (CTX_LINHA + CTX_GAP);
    GfxRect r = { x + CTX_PAD, by, CTX_W - CTX_PAD * 2.0f, CTX_LINHA };
    float f = focoAnim[i];
    // Mesma linguagem das pilulas: o focado INVERTE (fundo claro, texto
    // escuro), em vez de anel branco sobre preenchimento claro.
    float lum = anim_mistura(0.176f, 0.961f, f);
    int cor = i == foco ? 17 : 240;
    gfx_cor(r, 14.0f / CTX_LINHA, lum, lum, lum, a);
    { TxtLinha t = txt_linha(TXT_PLR_CORPO, ops[i].rot, cor, cor, cor, 255);
      txt_desenhar_alpha(t, r.x + 44.0f,
                         by + (CTX_LINHA - t.h) * 0.5f, a); }
    if (f > 0.02f) {
      TxtLinha seta = txt_linha(TXT_CAPTION2, "▸", cor, cor, cor, 255);
      txt_desenhar_alpha(seta, r.x + 16.0f,
                         by + (CTX_LINHA - seta.h) * 0.5f, a * f);
    }
  }

  { const char *rodape = estadoOperacao == CTX_PENDENTE
                           ? "Voltar Fechar   Aguarde..."
                           : operacao != CTX_OP_NENHUMA
                           ? "↑ ↓ Navegar   OK Fechar   Voltar Fechar"
                           : "↑ ↓ Navegar   OK Selecionar   Voltar Fechar";
    TxtLinha t = txt_linha(TXT_CAPTION2, rodape,
                           155, 159, 169, 255);
    txt_desenhar_alpha(t, x + CTX_PAD,
                       y + alt - CTX_PAD - t.h, a * 0.86f); }
}
