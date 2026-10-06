// Watch Together: a interface — ver juntosui.h.
#include "juntosui.h"
#include "botoes.h"
#include "catalogo.h"
#include "gfx.h"
#include "idioma.h"
#include "ilha.h"
#include "juntos.h"
#include "juntosfonte.h"
#include "player.h"
#include "qr.h"
#include "recomenda.h"
#include "socialvis.h"
#include "text.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEGURAR_OK_MS  500u   // segurar OK com o OSD escondido abre as reacoes
#define BOLHA_MS       2500u
#define CHAT_MS        4000u
#define BOLHAS         3
#define FOLHA_AMIGOS   8

static const char *REACOES[5] = { "risada", "susto", "coracao", "palmas", "hein" };
static const char *FRASES[4] = { "pausa_rapidinho", "volta_um_pouco", "legenda", "ja_volto" };
static const char *frase(const char *k) {
  if (!strcmp(k, "pausa_rapidinho")) return i18n("Pausa rapidinho");
  if (!strcmp(k, "volta_um_pouco")) return i18n("Volta um pouco");
  if (!strcmp(k, "legenda")) return i18n("Legenda?");
  if (!strcmp(k, "ja_volto")) return i18n("Já volto");
  return "";
}
static void iconeReacao(char *dst, size_t n, const char *k) { snprintf(dst, n, "jt_%s", k); }

typedef enum { CAMADA_NADA = 0, CAMADA_FOLHA, CAMADA_CODIGO, CAMADA_SALA, CAMADA_SAIR, CAMADA_PEDIDO, CAMADA_REACOES } Camada;
static Camada camada;
static int foco, linhaReacao;

// folha do anfitriao
static int folhaIdx, folhaT, folhaE, soEu, nAmigos;
static char amigoId[FOLHA_AMIGOS][96], amigoNome[FOLHA_AMIGOS][64], amigoAv[FOLHA_AMIGOS][512];
static int marcado[FOLHA_AMIGOS];
static int pediuReproduzir, pediuFecharPlayer;
// codigo
static char cod[7] = "000000";
static int codPos;
static const char *COD_ALFA = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
// pedido de entrada (anfitriao)
static char pedidoId[96], pedidoNome[64], pedidoAv[256];
// convite em tela
static JuntosConvite conv;
// reacoes e bolhas
static Uint32 okDesceEm, agoraUlt;
static SDL_Event okEvento;
static int okSegurando;
static struct { char quem[96], nome[64], av[256], icone[32], txt[220]; Uint32 ate; } bolha[BOLHAS];
// QR
static GLuint texQr;
static char texQrDe[200];
static int mostrarQr;

static int noPlayer(void) { return player_aberto(); }

// --- avisos ---------------------------------------------------------------------------------
// No player, o aviso e da plrilha (toast); fora, da ilha do relogio com o rosto.
static void aviso(const char *chave, const char *texto, const char *icone, const char *quem, unsigned ms) {
  const JsMotor *m = juntos_motor();
  const JsMembro *p = m && quem ? jsm_membro(m, quem) : NULL;
  if (!texto || !*texto) return;
  if (noPlayer()) { player_toast_ex(texto, ms, icone ? icone : "aj_users", 0); return; }
  {
    IlhaAvisoEx e;
    memset(&e, 0, sizeof e);
    e.chave = chave; e.tipo = ILHA_ACENTO; e.icone = icone ? icone : "aj_users"; e.texto = texto; e.ms = ms;
    if (p) { e.rosto = p->av; e.rostoNome = p->nome; }
    ilha_avisar_ex(&e);
  }
}

static const char *nomeDe(const char *id, const char *reserva) {
  const JsMotor *m = juntos_motor();
  const JsMembro *p = m && id ? jsm_membro(m, id) : NULL;
  if (p && p->nome[0]) return p->nome;
  return reserva && *reserva ? reserva : "?";
}

// --- convite ---------------------------------------------------------------------------------
void juntosui_convite(const JuntosConvite *c) {
  static IlhaModal m;
  IlhaAvisoEx e;
  char txt[320], f1[120], f2[200], chave[64];
  conv = *c;
  memset(&m, 0, sizeof m);
  snprintf(m.kicker, sizeof m.kicker, "%s", i18n("Assistir juntos"));
  snprintf(m.titulo, sizeof m.titulo, "%s", c->titulo);
  if (c->imp.rotulo[0]) snprintf(m.linha, sizeof m.linha, i18n("Fonte de %s: %s"), c->deNome, c->imp.rotulo);
  snprintf(m.texto, sizeof m.texto, "%s",
           i18n("Cada um assiste pela própria conta e pelos próprios addons. Se a sua fonte for outra versão, o app avisa."));
  snprintf(m.arte, sizeof m.arte, "%s", c->poster);
  snprintf(m.rosto, sizeof m.rosto, "%s", c->deAv);
  snprintf(m.rostoNome, sizeof m.rostoNome, "%s", c->deNome);
  m.tipo = ILHA_ACENTO;
  m.nBotoes = 3;
  snprintf(m.botao[0], sizeof m.botao[0], "%s", i18n("Entrar"));
  snprintf(m.botaoIcone[0], sizeof m.botaoIcone[0], "aj_users");
  snprintf(m.botao[1], sizeof m.botao[1], "%s", i18n("Agora não"));
  snprintf(m.botao[2], sizeof m.botao[2], "%s", i18n("Não receber convites desta pessoa"));
  snprintf(m.botaoIcone[2], sizeof m.botaoIcone[2], "aj_x");
  snprintf(txt, sizeof txt, i18n("%s te chamou para assistir %s"), ilha_forte(f1, sizeof f1, c->deNome),
           ilha_forte(f2, sizeof f2, c->titulo));
  snprintf(chave, sizeof chave, "juntos:c:%s", c->id);
  memset(&e, 0, sizeof e);
  e.chave = chave; e.tipo = ILHA_ACENTO; e.texto = txt; e.ms = 12000u; e.modal = &m;
  e.rosto = c->deAv; e.rostoNome = c->deNome; e.capa = c->poster;
  ilha_avisar_ex(&e);
  if (noPlayer()) player_toast_ex(txt, 5000, "aj_users", 0);
  printf("[juntos] convite na ilha\n");
}

void juntosui_acao(const char *chave, int botao) {
  if (!strncmp(chave, "juntos:c:", 9)) {
    const char *id = chave + 9;
    if (strcmp(id, conv.id)) return;
    if (botao == 1) juntos_entrar_id(id);
    else if (botao == 2) juntos_recusar(id);
    else if (botao == 3) { juntos_silenciar(conv.deId); juntos_recusar(id); }
  }
}

// --- eventos da sala -------------------------------------------------------------------------
static void poeBolha(const char *quem, const char *icone, const char *txt, unsigned ms) {
  const JsMotor *m = juntos_motor();
  const JsMembro *p = m ? jsm_membro(m, quem) : NULL;
  int i, alvo = 0;
  for (i = 0; i < BOLHAS; i++) if (bolha[i].ate < bolha[alvo].ate) alvo = i;
  memset(&bolha[alvo], 0, sizeof bolha[0]);
  snprintf(bolha[alvo].quem, sizeof bolha[0].quem, "%s", quem);
  if (p) { snprintf(bolha[alvo].nome, sizeof bolha[0].nome, "%s", p->nome); snprintf(bolha[alvo].av, sizeof bolha[0].av, "%s", p->av); }
  if (icone) snprintf(bolha[alvo].icone, sizeof bolha[0].icone, "%s", icone);
  if (txt) snprintf(bolha[alvo].txt, sizeof bolha[0].txt, "%s", txt);
  bolha[alvo].ate = agoraUlt + ms;
}

static void textoAcao(const JsEvento *e, char *t, size_t n) {
  const char *quem = e->txt[0] ? e->txt : nomeDe(e->quem, "");
  t[0] = 0;
  if (!strcmp(e->k, "pausa")) snprintf(t, n, i18n("%s pausou"), quem);
  else if (!strcmp(e->k, "play")) snprintf(t, n, i18n("%s continuou"), quem);
  else if (!strcmp(e->k, "comecar")) snprintf(t, n, i18n("%s começou a sessão"), quem);
  else if (!strcmp(e->k, "proximo")) snprintf(t, n, i18n("%s foi para o próximo episódio"), quem);
  else if (!strcmp(e->k, "seek")) {
    if (e->n < 0) snprintf(t, n, i18n("%s voltou %d s"), quem, -e->n);
    else snprintf(t, n, i18n("%s avançou %d s"), quem, e->n);
  }
}

void juntosui_evento(const JsEvento *e) {
  const JsMotor *m = juntos_motor();
  char t[300];
  int souEu = m && !strcmp(e->quem, m->eu);
  switch (e->tipo) {
    case JSV_ACAO:
      if (souEu) break;
      textoAcao(e, t, sizeof t);
      aviso("juntos:acao", t, !strcmp(e->k, "pausa") ? "aj_pause" : "aj_play", e->quem, 2500u);
      break;
    case JSV_AVISO: {
      const char *n = e->txt[0] ? e->txt : nomeDe(e->quem, "");
      if (souEu && strcmp(e->k, "anfitriao")) break;
      t[0] = 0;
      if (!strcmp(e->k, "entrou")) snprintf(t, sizeof t, i18n("%s entrou"), n);
      else if (!strcmp(e->k, "voltou")) snprintf(t, sizeof t, i18n("%s voltou"), n);
      else if (!strcmp(e->k, "saiu")) snprintf(t, sizeof t, i18n("%s saiu"), n);
      else if (!strcmp(e->k, "ausente")) snprintf(t, sizeof t, i18n("%s saiu por um momento"), n);
      else if (!strcmp(e->k, "anfitriao")) {
        if (souEu) snprintf(t, sizeof t, "%s", i18n("Agora você controla a sessão"));
        else snprintf(t, sizeof t, i18n("Agora %s controla"), n);
      } else if (!strcmp(e->k, "esperando")) snprintf(t, sizeof t, i18n("Esperando %s · %d s"), n, e->n);
      else if (!strcmp(e->k, "seguindo")) snprintf(t, sizeof t, i18n("%s continua carregando. A sessão segue sem esperar."), n);
      aviso("juntos:aviso", t, "aj_users", e->quem, !strcmp(e->k, "esperando") ? 4000u : 3000u);
      break;
    }
    case JSV_REACAO: { char ic[40]; iconeReacao(ic, sizeof ic, e->k); poeBolha(e->quem, ic, NULL, BOLHA_MS); break; }
    case JSV_FRASE: poeBolha(e->quem, NULL, frase(e->k), BOLHA_MS + 1000); break;
    case JSV_CHAT: poeBolha(e->quem, "aj_message-square", e->txt, CHAT_MS); break;
    case JSV_PEDIDO:
      snprintf(pedidoId, sizeof pedidoId, "%s", e->quem);
      snprintf(pedidoNome, sizeof pedidoNome, "%s", e->nome[0] ? e->nome : "?");
      snprintf(pedidoAv, sizeof pedidoAv, "%s", e->txt);
      camada = CAMADA_PEDIDO; foco = 0;
      break;
    case JSV_PEDIR_PAUSA:
      snprintf(t, sizeof t, i18n("%s pediu uma pausa"), nomeDe(e->quem, e->txt));
      aviso("juntos:pedir", t, "aj_pause", e->quem, 5000u);
      break;
    case JSV_RECUSADO:
      if (!strcmp(e->k, "anfitriao")) aviso("juntos:erro", i18n("O anfitrião não aceitou a entrada"), "aj_x", NULL, 5000u);
      break;
    case JSV_SINCRONIZANDO: if (noPlayer()) player_toast_ex(i18n("Sincronizando…"), 1500, "aj_rotate-cw", 0); break;
    case JSV_AGUARDANDO: aviso("juntos:aguarda", i18n("Esperando o anfitrião aceitar…"), "aj_users", NULL, 8000u); break;
    case JSV_FIM: {
      char dur[48] = "";
      if (e->n >= 3600) snprintf(dur, sizeof dur, "%d h %d min", e->n / 3600, (e->n % 3600) / 60);
      else snprintf(dur, sizeof dur, "%d min", e->n / 60 > 0 ? e->n / 60 : 1);
      snprintf(t, sizeof t, i18n("Sessão encerrada · %s"), dur);
      aviso("juntos:fim", t, "aj_users", NULL, 6000u);
      break;
    }
    default: break;
  }
}

void juntosui_erro(const char *k) {
  const char *t = i18n("Não foi possível entrar nesta sessão");
  if (!strcmp(k, "sem_sala") || !strcmp(k, "sala_fechada")) t = i18n("Essa sessão não existe mais");
  else if (!strcmp(k, "cheia")) t = i18n("A sessão está cheia");
  else if (!strcmp(k, "limite")) t = i18n("Muitas tentativas. Tente de novo mais tarde.");
  else if (!strcmp(k, "caiu")) t = i18n("A conexão com a sessão caiu");
  else if (!strcmp(k, "fim")) t = i18n("A sessão terminou");
  else if (!strcmp(k, "rede")) t = i18n("Sem conexão com o serviço de sessões");
  if (noPlayer()) player_toast_ex(t, 5000, "aj_triangle-alert", 1);
  else ilha_avisar("juntos:erro", ILHA_ERRO, "aj_triangle-alert", t, 6000u, 0);
}

void juntosui_sala_abriu(void) {
  if (camada == CAMADA_FOLHA || camada == CAMADA_CODIGO) camada = CAMADA_NADA;
  // O cartao da sala sobe sozinho fora do player (convidado entrando, anfitriao
  // antes do play); no player ele so vem pela regua (segurar OK).
  if (!noPlayer()) { camada = CAMADA_SALA; foco = 0; }
  mostrarQr = 0;
}

void juntosui_sala_fechou(void) {
  if (camada == CAMADA_SALA || camada == CAMADA_SAIR || camada == CAMADA_PEDIDO || camada == CAMADA_REACOES)
    camada = CAMADA_NADA;
  memset(bolha, 0, sizeof bolha);
  mostrarQr = 0;
}

void juntosui_fonte_diferente(int v, int pal) {
  const char *t = NULL;
  if (v == JSF_NENHUMA) t = i18n("Nenhuma fonte encontrada nos seus addons para esta sessão");
  else if (v == JSF_DIFERENTE) t = pal ? i18n("Sua fonte é uma versão PAL, mais rápida. Pode desalinhar.")
                                       : i18n("Sua fonte tem duração diferente. Pode desalinhar em cenas.");
  if (t) aviso("juntos:fonte", t, "aj_triangle-alert", NULL, 7000u);
}

void juntosui_perguntar_sair(void) { camada = CAMADA_SAIR; foco = 1; }

// --- folha do anfitriao --------------------------------------------------------------------------
void juntosui_abrir_folha(int idx, int t, int e) {
  int i, n = socialvis_n_amigos();
  if (!juntos_disponivel()) {
    ilha_avisar("juntos:erro", ILHA_INFO, "aj_users", i18n("Ligue a conta Nuvio e o Social para assistir junto"), 5000u, 0);
    return;
  }
  if (juntos_ativo()) { camada = CAMADA_SALA; foco = 0; return; }
  folhaIdx = idx; folhaT = t; folhaE = e;
  nAmigos = 0;
  for (i = 0; i < n && nAmigos < FOLHA_AMIGOS; i++) {
    const SvAmigo *a = socialvis_amigo(i);
    if (!a || !a->id[0]) continue;
    snprintf(amigoId[nAmigos], sizeof amigoId[0], "%s", a->id);
    snprintf(amigoNome[nAmigos], sizeof amigoNome[0], "%s", a->apelido[0] ? a->apelido : a->nome);
    snprintf(amigoAv[nAmigos], sizeof amigoAv[0], "%s", a->avatar);
    marcado[nAmigos] = 0;
    nAmigos++;
  }
  camada = CAMADA_FOLHA;
  foco = 0;
}

// Linhas da folha: amigos, o alternador, Abrir sala, Entrar com codigo.
static int folhaLinhas(void) { return nAmigos + 3; }

static void abrirSala(void) {
  const CatItem *ci = cat_item(folhaIdx);
  const char *lista[FOLHA_AMIGOS];
  int i, n = 0;
  if (!ci) return;
  for (i = 0; i < nAmigos; i++) if (marcado[i]) lista[n++] = amigoId[i];
  juntos_preparar(ci->imdb, folhaT, folhaE, ci->titulo, ci->poster, lista, n, soEu);
  pediuReproduzir = 1;
  camada = CAMADA_NADA;
}

int juntosui_pediu_reproduzir(void) { int p = pediuReproduzir; pediuReproduzir = 0; return p; }
int juntosui_pediu_fechar_player(void) { int p = pediuFecharPlayer; pediuFecharPlayer = 0; return p; }
int juntosui_aberta(void) { return camada != CAMADA_NADA || okSegurando; }

// --- botoes do cartao da sala ------------------------------------------------------------------
enum { BT_COMECAR, BT_CELULAR, BT_TRAVA, BT_SAIR, BT_N };
static int botoesSala(int *b) {
  const JsMotor *m = juntos_motor();
  int n = 0;
  if (m && m->temSala && m->sala.rel.estado == JSE_LOBBY && juntos_pode_controlar()) b[n++] = BT_COMECAR;
  if (m && m->temSala) b[n++] = BT_CELULAR;
  if (juntos_sou_anfitriao()) b[n++] = BT_TRAVA;
  b[n++] = BT_SAIR;
  return n;
}
static const char *rotuloBotao(int b) {
  const JsMotor *m = juntos_motor();
  switch (b) {
    case BT_COMECAR: return i18n("Começar");
    case BT_CELULAR: return i18n("Celular");
    case BT_TRAVA: return m && m->sala.controleAnf ? i18n("Só eu controlo") : i18n("Todos controlam");
    default: return i18n("Sair da sessão");
  }
}
static const char *iconeBotao(int b) {
  switch (b) {
    case BT_COMECAR: return "aj_play";
    case BT_CELULAR: return "aj_smartphone";
    case BT_TRAVA: return "aj_users";
    default: return "aj_log-out";
  }
}

static void apertarSala(int b) {
  const JsMotor *m = juntos_motor();
  switch (b) {
    case BT_COMECAR: juntos_comecar(); break;
    case BT_CELULAR: mostrarQr = !mostrarQr; if (mostrarQr) juntos_pedir_celular(); break;
    case BT_TRAVA: if (m) juntos_controle_anfitriao(!m->sala.controleAnf); break;
    case BT_SAIR:
      if (juntos_sou_anfitriao() && m && m->sala.n > 1) { juntos_sair(); }
      else if (juntos_sou_anfitriao()) juntos_encerrar(), juntos_sair();
      else juntos_sair();
      camada = CAMADA_NADA;
      break;
  }
}

// --- teclas -----------------------------------------------------------------------------------------
static int ehOk(SDL_Keycode k) { return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE; }
static int ehVoltar(SDL_Keycode k) { return k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE || k == SDLK_DELETE; }

int juntosui_evento_sdl(const SDL_Event *e) {
  SDL_Keycode k;
  if (e->type != SDL_KEYDOWN && e->type != SDL_KEYUP) return 0;
  k = e->key.keysym.sym;
  // SEGURAR OK no player com o OSD escondido: a regua de reacoes. O toque curto
  // continua sendo o play/pausa: ele e entregue ao player quando a tecla SOBE.
  if (camada == CAMADA_NADA && juntos_ativo() && noPlayer() && !player_controles_visiveis() && ehOk(k)) {
    if (e->type == SDL_KEYDOWN) {
      if (!e->key.repeat && !okSegurando) { okSegurando = 1; okDesceEm = SDL_GetTicks(); okEvento = *e; }
      return 1;
    }
    if (okSegurando) {
      okSegurando = 0;
      if (SDL_GetTicks() - okDesceEm < SEGURAR_OK_MS) player_evento(&okEvento);
      return 1;
    }
    return 0;
  }
  if (camada == CAMADA_NADA) return 0;
  if (e->type == SDL_KEYUP) return 1;
  switch (camada) {
    case CAMADA_FOLHA: {
      int n = folhaLinhas();
      if (ehVoltar(k)) camada = CAMADA_NADA;
      else if (k == SDLK_UP && foco > 0) foco--;
      else if (k == SDLK_DOWN && foco < n - 1) foco++;
      else if (ehOk(k)) {
        if (foco < nAmigos) marcado[foco] = !marcado[foco];
        else if (foco == nAmigos) soEu = !soEu;
        else if (foco == nAmigos + 1) abrirSala();
        else { camada = CAMADA_CODIGO; codPos = 0; }
      }
      return 1;
    }
    case CAMADA_CODIGO: {
      const char *p = strchr(COD_ALFA, cod[codPos]);
      int i = p ? (int)(p - COD_ALFA) : 0;
      if (ehVoltar(k)) camada = CAMADA_FOLHA;
      else if (k == SDLK_LEFT && codPos > 0) codPos--;
      else if (k == SDLK_RIGHT && codPos < 5) codPos++;
      else if (k == SDLK_UP) cod[codPos] = COD_ALFA[(i + 1) % 32];
      else if (k == SDLK_DOWN) cod[codPos] = COD_ALFA[(i + 31) % 32];
      else if (k >= SDLK_0 && k <= SDLK_9) { cod[codPos] = (char)('0' + (k - SDLK_0)); if (codPos < 5) codPos++; }
      else if (k >= SDLK_a && k <= SDLK_z) {
        char c = (char)('A' + (k - SDLK_a));
        if (strchr(COD_ALFA, c)) { cod[codPos] = c; if (codPos < 5) codPos++; }
      } else if (ehOk(k)) { juntos_entrar_codigo(cod); camada = CAMADA_NADA; }
      return 1;
    }
    case CAMADA_SALA: {
      int b[BT_N], n = botoesSala(b);
      if (foco >= n) foco = n - 1;
      if (ehVoltar(k)) { camada = CAMADA_NADA; mostrarQr = 0; }
      else if (k == SDLK_LEFT && foco > 0) foco--;
      else if (k == SDLK_RIGHT && foco < n - 1) foco++;
      else if (ehOk(k)) apertarSala(b[foco]);
      return 1;
    }
    case CAMADA_SAIR:
      if (ehVoltar(k)) camada = CAMADA_NADA;
      else if (k == SDLK_LEFT) foco = 0;
      else if (k == SDLK_RIGHT) foco = 1;
      else if (ehOk(k)) {
        camada = CAMADA_NADA;
        if (foco == 0) {
          if (juntos_sou_anfitriao()) { const JsMotor *m = juntos_motor(); if (m && m->sala.n <= 1) juntos_encerrar(); }
          juntos_sair();
          pediuFecharPlayer = 1;
        }
      }
      return 1;
    case CAMADA_PEDIDO:
      if (k == SDLK_LEFT) foco = 0;
      else if (k == SDLK_RIGHT) foco = 1;
      else if (ehOk(k) || ehVoltar(k)) {
        const JsMotor *m = juntos_motor();
        if (m) jsm_aceitar((JsMotor *)m, pedidoId, ehOk(k) && foco == 0);
        camada = CAMADA_NADA;
      }
      return 1;
    case CAMADA_REACOES: {
      const JsMotor *m = juntos_motor();
      int n = linhaReacao == 0 ? 6 : 4;
      if (ehVoltar(k)) camada = CAMADA_NADA;
      else if (k == SDLK_LEFT && foco > 0) foco--;
      else if (k == SDLK_RIGHT && foco < n - 1) foco++;
      else if (k == SDLK_DOWN && linhaReacao == 0) { linhaReacao = 1; if (foco > 3) foco = 3; }
      else if (k == SDLK_UP && linhaReacao == 1) linhaReacao = 0;
      else if (ehOk(k) && m) {
        if (linhaReacao == 0 && foco == 5) { camada = CAMADA_SALA; foco = 0; return 1; }
        if (linhaReacao == 0) jsm_reacao((JsMotor *)m, REACOES[foco]);
        else jsm_frase((JsMotor *)m, FRASES[foco]);
        camada = CAMADA_NADA;
      }
      return 1;
    }
    default: return 0;
  }
}

void juntosui_atualizar(Uint32 agora) {
  agoraUlt = agora;
  if (okSegurando && agora - okDesceEm >= SEGURAR_OK_MS && camada == CAMADA_NADA) {
    okSegurando = 0;
    camada = CAMADA_REACOES; linhaReacao = 0; foco = 0;
  }
  if (!juntos_ativo() && (camada == CAMADA_SALA || camada == CAMADA_REACOES || camada == CAMADA_SAIR || camada == CAMADA_PEDIDO))
    camada = CAMADA_NADA;
  if (!noPlayer() && camada == CAMADA_REACOES) camada = CAMADA_NADA;
}

// --- desenho ---------------------------------------------------------------------------------------
static void texto(TxtEstilo es, const char *s, float x, float y, int c, float a, float maxW) {
  TxtLinha l = maxW > 0 ? txt_linha_corta(es, s, c, c, c, 255, maxW) : txt_linha(es, s, c, c, c, 255);
  txt_desenhar_alpha(l, x, y, a);
}
static float largura(TxtEstilo es, const char *s) { return (float)txt_linha(es, s, 255, 255, 255, 255).w; }

static void painel(GfxRect r) {
  gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, 0, 0, 0, 0.45f);
  gfx_vidro_painel(r, 0.06f, 0.82f, 1.0f);
}

static const char *estadoTexto(const JsMembro *p) {
  if (p->alc && (!strcmp(p->st, "tocando") || !strcmp(p->st, "carregando"))) return i18n("alcançando");
  if (!strcmp(p->st, "entrando")) return i18n("entrando");
  if (!strcmp(p->st, "procurando")) return i18n("procurando fonte");
  if (!strcmp(p->st, "carregando")) return i18n("carregando");
  if (!strcmp(p->st, "pronto")) return i18n("pronto");
  if (!strcmp(p->st, "tocando")) return i18n("assistindo");
  if (!strcmp(p->st, "pausado")) return i18n("pausado");
  if (!strcmp(p->st, "semfonte")) return i18n("sem fonte");
  if (!strcmp(p->st, "erro")) return i18n("erro");
  return i18n("fora");
}
static const char *fonteTexto(const char *f) {
  if (!strcmp(f, "mesma")) return i18n("mesma fonte");
  if (!strcmp(f, "parecida")) return i18n("fonte parecida");
  if (!strcmp(f, "diferente")) return i18n("versão diferente");
  if (!strcmp(f, "nenhuma")) return i18n("sem fonte");
  return "";
}

static GLuint qrTextura(const char *u) {
  Qr q; int lado, x, y; unsigned char *px;
  if (!u || !u[0]) return 0;
  if (texQr && !strcmp(texQrDe, u)) return texQr;
  if (!qr_gerar(&q, u)) return 0;
  lado = q.lado + 8;
  px = (unsigned char *)malloc((size_t)lado * lado * 3);
  if (!px) return 0;
  memset(px, 255, (size_t)lado * lado * 3);
  for (y = 0; y < q.lado; y++)
    for (x = 0; x < q.lado; x++)
      if (qr_modulo(&q, x, y)) { size_t k = ((size_t)(y + 4) * lado + (x + 4)) * 3; px[k] = px[k + 1] = px[k + 2] = 0; }
  if (!texQr) glGenTextures(1, &texQr);
  glBindTexture(GL_TEXTURE_2D, texQr);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, lado, lado, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gfx_tex_esquecer(0);
  free(px);
  snprintf(texQrDe, sizeof texQrDe, "%s", u);
  return texQr;
}

static void desenharFolha(void) {
  GfxRect r = { 560, 120, 800, 840 };
  const CatItem *ci = cat_item(folhaIdx);
  float y = r.y + 48;
  int i;
  painel(r);
  texto(TXT_CAPTION, i18n("Assistir juntos"), r.x + 48, y, 200, 1, 0);
  y += 40;
  texto(TXT_ILHA_TITULO, ci ? ci->titulo : "", r.x + 48, y, 245, 1, r.w - 96);
  y += 72;
  texto(TXT_ILHA_CORPO, nAmigos ? i18n("Quem vai assistir com você") : i18n("Nenhum amigo ainda. Quem tiver o código entra pedindo."),
        r.x + 48, y, 190, 1, r.w - 96);
  y += 48;
  for (i = 0; i < folhaLinhas(); i++) {
    GfxRect l = { r.x + 32, y, r.w - 64, 64 };
    float f = foco == i ? 1.0f : 0.0f;
    if (i < nAmigos) {
      int c = botao_superficie(l, f, 1);
      rec_avatar((GfxRect){ l.x + 16, l.y + 10, 44, 44 }, amigoAv[i], amigoNome[i], amigoId[i], 1);
      texto(TXT_ILHA_NOME, amigoNome[i], l.x + 76, l.y + 16, c, 1, l.w - 160);
      gfx_icone((GfxRect){ l.x + l.w - 56, l.y + 16, 32, 32 }, marcado[i] ? "aj_circle-check" : "aj_circle-dot",
                c / 255.0f, c / 255.0f, c / 255.0f, marcado[i] ? 1.0f : 0.35f);
    } else if (i == nAmigos) {
      int c = botao_superficie(l, f, 1);
      gfx_icone((GfxRect){ l.x + 18, l.y + 16, 32, 32 }, "aj_users", c / 255.0f, c / 255.0f, c / 255.0f, 1);
      texto(TXT_ILHA_NOME, soEu ? i18n("Só eu controlo") : i18n("Todos controlam"), l.x + 76, l.y + 16, c, 1, l.w - 100);
    } else {
      botao_pilula(l, i == nAmigos + 1 ? i18n("Abrir sala") : i18n("Entrar com código"),
                   i == nAmigos + 1 ? "aj_play" : "aj_key-round", f, i == nAmigos + 1, 1, 1);
    }
    y += 76;
    if (i == nAmigos) y += 12;
  }
}

static void desenharCodigo(void) {
  GfxRect r = { 610, 300, 700, 420 };
  int i;
  painel(r);
  texto(TXT_CAPTION, i18n("Código da sala"), r.x + 48, r.y + 48, 200, 1, 0);
  texto(TXT_ILHA_CORPO, i18n("Peça o código a quem abriu a sala. CIMA e BAIXO trocam a letra."), r.x + 48, r.y + 92, 190, 1, r.w - 96);
  for (i = 0; i < 6; i++) {
    GfxRect c = { r.x + 64 + i * 98.0f + (i >= 3 ? 20 : 0), r.y + 170, 80, 110 };
    char s[2] = { cod[i], 0 };
    float f = codPos == i ? 1.0f : 0.0f;
    int tinta = botao_superficie(c, f, 1);
    TxtLinha l = txt_linha(TXT_ILHA_TITULO, s, tinta, tinta, tinta, 255);
    txt_desenhar_alpha(l, c.x + (c.w - l.w) / 2, c.y + (c.h - l.h) / 2, 1);
  }
  botao_pilula((GfxRect){ r.x + 48, r.y + 320, r.w - 96, BOTAO_H_SECUNDARIO }, i18n("Entrar"), "aj_users", 0, 1, 0, 1);
}

static void desenharSala(void) {
  const JsMotor *m = juntos_motor();
  GfxRect r = { 360, 140, 1200, 800 };
  float y = r.y + 48;
  int i, b[BT_N], n = botoesSala(b);
  char linha[200], nomes[160] = "";
  if (!m) return;
  painel(r);
  texto(TXT_CAPTION, i18n("Assistir juntos"), r.x + 48, y, 200, 1, 0);
  if (juntos_codigo()[0]) {
    char c[64];
    snprintf(c, sizeof c, i18n("Código %.3s-%s"), juntos_codigo(), juntos_codigo() + 3);
    texto(TXT_ILHA_NOME, c, r.x + r.w - 48 - largura(TXT_ILHA_NOME, c), y - 4, 230, 1, 0);
  }
  y += 40;
  texto(TXT_ILHA_TITULO, m->temSala && m->sala.titulo[0] ? m->sala.titulo : juntos_titulo(), r.x + 48, y, 245, 1, r.w - 96);
  y += 64;
  for (i = 0; i < m->sala.n; i++) if (strcmp(m->sala.m[i].id, m->eu)) {
    size_t k = strlen(nomes);
    snprintf(nomes + k, sizeof nomes - k, "%s%s", k ? ", " : "", m->sala.m[i].nome);
  }
  if (nomes[0]) { snprintf(linha, sizeof linha, i18n("Sessão com %s"), nomes); texto(TXT_ILHA_CORPO, linha, r.x + 48, y, 190, 1, r.w - 96); }
  else texto(TXT_ILHA_CORPO, i18n("Esperando quem você chamou"), r.x + 48, y, 190, 1, r.w - 96);
  y += 60;
  for (i = 0; i < m->sala.n && i < 5; i++) {
    const JsMembro *p = &m->sala.m[i];
    char st[120];
    int pronto = !strcmp(p->st, "pronto") || !strcmp(p->st, "tocando");
    rec_avatar((GfxRect){ r.x + 48, y, 56, 56 }, p->av, p->nome, p->id, p->on ? 1.0f : 0.4f);
    snprintf(linha, sizeof linha, "%s%s", strcmp(p->id, m->eu) ? p->nome : i18n("Você"), p->anf ? " · " : "");
    texto(TXT_ILHA_NOME, linha, r.x + 124, y + 4, 240, 1, 380);
    if (p->anf) texto(TXT_CAPTION, i18n("anfitrião"), r.x + 124 + largura(TXT_ILHA_NOME, linha), y + 8, 170, 1, 0);
    if (!strcmp(p->st, "carregando") && p->pct > 0) snprintf(st, sizeof st, "%s %d%%", estadoTexto(p), p->pct);
    else snprintf(st, sizeof st, "%s", estadoTexto(p));
    texto(TXT_CAPTION, st, r.x + 124, y + 34, pronto ? 200 : 160, 1, 380);
    {
      char d[80] = "";
      snprintf(d, sizeof d, "%s%s", fonteTexto(p->fonte), p->nivel == 'B' ? i18n(" · sync ±1 s") : "");
      texto(TXT_CAPTION, d, r.x + 560, y + 20, 170, 1, mostrarQr ? 200 : 500);
    }
    y += 76;
  }
  if (mostrarQr) {
    GfxRect q = { r.x + r.w - 48 - 300, r.y + 200, 300, 300 };
    GLuint t = qrTextura(juntos_url_celular());
    if (t) gfx_rect(q, t, GFX_SNAP, 0, 0, 0, 0, 1, 1, 1, 1);
    else gfx_cor(q, 0.05f, 1, 1, 1, 0.08f);
    txt_bloco_corta(TXT_CAPTION, i18n("Escaneie com o celular para mandar mensagens e controlar a sessão."),
                    190, 190, 190, q.x, q.y + q.h + 16, q.w, 30, 1, 3);
  }
  {
    float x = r.x + 48, by = r.y + r.h - 48 - BOTAO_H_SECUNDARIO;
    for (i = 0; i < n; i++) {
      float w = botao_largura(rotuloBotao(b[i]), iconeBotao(b[i]), b[i] == BT_COMECAR);
      botao_pilula((GfxRect){ x, by, w, BOTAO_H_SECUNDARIO }, rotuloBotao(b[i]), iconeBotao(b[i]),
                   foco == i ? 1.0f : 0.0f, b[i] == BT_COMECAR, 0, 1);
      x += w + BOTAO_GAP;
    }
  }
}

static void desenharPergunta(const char *titulo, const char *corpo, const char *rostoUrl, const char *rostoNome,
                             const char *b0, const char *b1) {
  GfxRect r = { 610, 360, 700, 340 };
  float w0 = botao_largura(b0, NULL, 1), w1 = botao_largura(b1, NULL, 0);
  float x = r.x + 48;
  painel(r);
  if (rostoNome) { rec_avatar((GfxRect){ r.x + 48, r.y + 44, 64, 64 }, rostoUrl, rostoNome, rostoNome, 1); x = r.x + 132; }
  texto(TXT_ILHA_TITULO, titulo, x, r.y + 52, 245, 1, r.w - (x - r.x) - 48);
  if (corpo) txt_bloco_corta(TXT_ILHA_CORPO, corpo, 190, 190, 190, r.x + 48, r.y + 136, r.w - 96, 34, 1, 3);
  botao_pilula((GfxRect){ r.x + 48, r.y + r.h - 48 - BOTAO_H_SECUNDARIO, w0, BOTAO_H_SECUNDARIO }, b0, NULL, foco == 0, 1, 0, 1);
  botao_pilula((GfxRect){ r.x + 48 + w0 + BOTAO_GAP, r.y + r.h - 48 - BOTAO_H_SECUNDARIO, w1, BOTAO_H_SECUNDARIO }, b1, NULL,
               foco == 1, 0, 0, 1);
}

static void desenharReacoes(void) {
  GfxRect r = { 560, 820, 800, 200 };
  int i;
  gfx_vidro_painel(r, 0.2f, 0.82f, 1);
  for (i = 0; i < 6; i++) {
    GfxRect d = { r.x + 40 + i * 124.0f, r.y + 24, 88, 88 };
    char ic[40];
    if (i < 5) iconeReacao(ic, sizeof ic, REACOES[i]); else snprintf(ic, sizeof ic, "aj_users");
    botao_disco(d, ic, linhaReacao == 0 && foco == i ? 1.0f : 0.0f, 1);
  }
  {
    float x = r.x + 40;
    for (i = 0; i < 4; i++) {
      const char *t = frase(FRASES[i]);
      float w = botao_largura(t, NULL, 0);
      botao_pilula((GfxRect){ x, r.y + 128, w, 52 }, t, NULL, linhaReacao == 1 && foco == i ? 1.0f : 0.0f, 0, 0, 1);
      x += w + 12;
    }
  }
}

// Rostos de quem esta junto, no canto, quando o OSD do player sobe.
static void desenharRostos(void) {
  const JsMotor *m = juntos_motor();
  float x, y = 44;
  int i, n;
  if (!m || !m->temSala || !player_controles_visiveis()) return;
  n = m->sala.n > 5 ? 5 : m->sala.n;
  x = 1920 - 60 - n * 40.0f - 16;
  gfx_vidro_painel((GfxRect){ x - 14, y - 8, n * 40.0f + 44, 64 }, 0.5f, 0.7f, 0.9f);
  for (i = 0; i < n; i++) {
    const JsMembro *p = &m->sala.m[i];
    rec_avatar((GfxRect){ x + i * 40.0f, y, 48, 48 }, p->av, p->nome, p->id, p->on ? 1.0f : 0.45f);
    if (!strcmp(p->st, "carregando") || !strcmp(p->st, "procurando"))
      gfx_cor((GfxRect){ x + i * 40.0f + 34, y + 34, 14, 14 }, 0.5f, 1.0f, 0.8f, 0.3f, 1);
  }
}

static void desenharBolhas(Uint32 agora) {
  float y = 140;
  int i;
  for (i = 0; i < BOLHAS; i++) {
    float a, w;
    if (!bolha[i].ate || agora >= bolha[i].ate) continue;
    a = (float)(bolha[i].ate - agora) / 400.0f;
    if (a > 1) a = 1;
    w = 64 + (bolha[i].icone[0] ? 52 : 0) + (bolha[i].txt[0] ? fminf(520, largura(TXT_ILHA_CORPO, bolha[i].txt)) + 24 : 0) + 16;
    gfx_vidro_painel((GfxRect){ 1920 - 60 - w, y, w, 68 }, 0.5f, 0.82f, a);
    rec_avatar((GfxRect){ 1920 - 60 - w + 12, y + 10, 48, 48 }, bolha[i].av, bolha[i].nome, bolha[i].quem, a);
    {
      float x = 1920 - 60 - w + 72;
      if (bolha[i].icone[0]) { gfx_icone((GfxRect){ x, y + 16, 36, 36 }, bolha[i].icone, 1, 1, 1, a); x += 52; }
      if (bolha[i].txt[0]) texto(TXT_ILHA_CORPO, bolha[i].txt, x, y + 20, 240, a, 520);
    }
    y += 80;
  }
}

void juntosui_desenhar(Uint32 agora) {
  float esc = gfx_escala_entrar();
  if (noPlayer() && juntos_ativo()) { desenharRostos(); desenharBolhas(agora); }
  switch (camada) {
    case CAMADA_FOLHA: desenharFolha(); break;
    case CAMADA_CODIGO: desenharCodigo(); break;
    case CAMADA_SALA: desenharSala(); break;
    case CAMADA_SAIR:
      desenharPergunta(i18n("Sair da sessão?"), i18n("Os outros continuam assistindo sem você."),
                       NULL, NULL, i18n("Sair da sessão"), i18n("Ficar"));
      break;
    case CAMADA_PEDIDO: {
      char t[160];
      snprintf(t, sizeof t, i18n("%s quer entrar"), pedidoNome);
      desenharPergunta(t, i18n("Entrou pelo código da sala."), pedidoAv, pedidoNome, i18n("Aceitar"), i18n("Recusar"));
      break;
    }
    case CAMADA_REACOES: desenharReacoes(); break;
    default: break;
  }
  gfx_escala_sair(esc);
}
