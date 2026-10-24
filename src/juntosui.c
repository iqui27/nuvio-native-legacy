// Watch Together: a interface — ver juntosui.h.
#include "juntosui.h"
#include "ajustes.h"
#include "botoes.h"
#include "badges.h"
#include "catalogo.h"
#include "gfx.h"
#include "idioma.h"
#include "ilha.h"
#include "plrilha.h"
#include "juntos.h"
#include "juntosfonte.h"
#include "player.h"
#include "qr.h"
#include "recomenda.h"
#include "socialvis.h"
#include "tex_cache.h"
#include "text.h"
#include "tela21.h"
#include "revela.h"
#include "teclavoltar.h"
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

typedef enum { CAMADA_NADA = 0, CAMADA_FOLHA, CAMADA_CODIGO, CAMADA_SALA, CAMADA_SAIR, CAMADA_PEDIDO, CAMADA_REACOES, CAMADA_FONTES } Camada;
static Camada camada;
static int foco, linhaReacao;
static Camada camadaVisual;
static GfxRect corpoBase;
static int corpoDesenhando;
static float entrada, tempoEntrada, focoAnim[8][16], lugarAnim[5], fonteExp[5];
static Uint32 ultimoQuadro;
static float contagemAnim;
static int ultimaContagem;
static float origemX, origemY, folhaScroll, folhaVel;

static float focoDe(int i) { return focoAnim[camadaVisual][i + 1]; }

// folha do anfitriao
static int folhaIdx, folhaT, folhaE, soEu, nAmigos;
static char folhaTitulo[160];
static char amigoId[FOLHA_AMIGOS][96], amigoNome[FOLHA_AMIGOS][64], amigoAv[FOLHA_AMIGOS][512];
static int marcado[FOLHA_AMIGOS];
static int pediuReproduzir, pediuFecharPlayer, pediuEscolherFilme, pediuFontes;
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
static struct { char quem[96], nome[64], av[256], icone[32], txt[220]; Uint32 ate, desde; } bolha[BOLHAS];
// QR
static GLuint texQr;
static char texQrDe[200];
static int mostrarQr;

static void poeBolha(const char *quem, const char *icone, const char *txt, unsigned ms);
static const Stream *fonteLocal(const JsMotor *m);
static int contagemRestante(void);
static void pedirCorpo(void);
static float corpoAltura(float h) {
  return fminf(h, (NV_LAYOUT_REAL_H - NV_ILHA_Y - 24) / gfx_escala_ui() - NV_ILHA_H_ABERTA);
}
#ifdef NV_SHOT_HOOKS
static int shotPlayer, shotContagem;
static char shotArte[512];
void juntosui_shot_player(int sim) { shotPlayer = sim; }
// A folha com amigos de mentira (o catalogo e o social estao vazios na captura).
void juntosui_shot_folha(int idx, const char *const *nomes, int n) {
  int i;
  folhaIdx = idx; nAmigos = 0;
  snprintf(folhaTitulo, sizeof folhaTitulo, "Duna: Parte 2");
  for (i = 0; i < n && i < FOLHA_AMIGOS; i++) {
    snprintf(amigoId[i], sizeof amigoId[0], "nuvio:%d", i);
    snprintf(amigoNome[i], sizeof amigoNome[0], "%s", nomes[i]);
    amigoAv[i][0] = 0; marcado[i] = i == 0; nAmigos++;
  }
  camada = CAMADA_FOLHA; foco = 1;
}
void juntosui_shot_bolhas(void) {
  agoraUlt = SDL_GetTicks();
  poeBolha("nuvio:r", "jt_risada", NULL, 60000);
  poeBolha("nuvio:f", NULL, "Volta um pouco", 60000);
  poeBolha("nuvio:r", "aj_message-square", "a pipoca acabou", 60000);
}
void juntosui_shot_camada(int c, int f) { camada = (Camada)c; foco = f; linhaReacao = 0; }
float juntosui_shot_entrada(void) { return entrada; }
float juntosui_shot_foco(int i) { return focoDe(i); }
void juntosui_shot_arte(const char *caminho) { snprintf(shotArte, sizeof shotArte, "%s", caminho); }
void juntosui_shot_contagem(int n) { shotContagem = n; }
static int noPlayer(void) { return player_aberto() || shotPlayer; }
#else
static int noPlayer(void) { return player_aberto(); }
#endif

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
  snprintf(m.kicker, sizeof m.kicker, "%s", i18n("Assistir Juntos"));
  snprintf(m.titulo, sizeof m.titulo, "%s", c->titulo);
  if (c->imp.rotulo[0]) snprintf(m.linha, sizeof m.linha, i18n("Fonte de %s: %s"), c->deNome, c->imp.rotulo);
  snprintf(m.texto, sizeof m.texto, "%s",
           i18n("Cada um assiste pela própria conta. Se a sua fonte for outra versão, o app avisa."));
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
  bolha[alvo].desde = agoraUlt;
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
  { const CatItem *ci = cat_item(idx); snprintf(folhaTitulo, sizeof folhaTitulo, "%s", ci ? ci->titulo : ""); }
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
int juntosui_pediu_escolher_filme(void) { int p = pediuEscolherFilme; pediuEscolherFilme = 0; return p; }
int juntosui_pediu_fechar_player(void) { int p = pediuFecharPlayer; pediuFecharPlayer = 0; return p; }
int juntosui_pediu_fontes(void) { int p = pediuFontes; pediuFontes = 0; return p; }
int juntosui_aberta(void) { return camada != CAMADA_NADA || okSegurando; }

// --- botoes do cartao da sala ------------------------------------------------------------------
enum { BT_COMECAR, BT_CELULAR, BT_FILME, BT_FONTES, BT_TRAVA, BT_SAIR, BT_N };
static int botoesSala(int *b) {
  const JsMotor *m = juntos_motor();
  int n = 0;
  if (m && m->temSala && m->sala.rel.estado == JSE_LOBBY && juntos_pode_controlar()) b[n++] = BT_COMECAR;
  if (m && m->temSala) b[n++] = BT_CELULAR;
  if (m && m->temSala && juntos_pode_controlar()) b[n++] = BT_FILME;
  if (m && m->temSala) b[n++] = BT_FONTES;
  if (juntos_sou_anfitriao()) b[n++] = BT_TRAVA;
  b[n++] = BT_SAIR;
  return n;
}
static const char *rotuloBotao(int b) {
  const JsMotor *m = juntos_motor();
  switch (b) {
    case BT_COMECAR: return i18n("Começar juntos");
    case BT_CELULAR: return i18n("Convidar");
    case BT_FILME: return i18n("Escolher um filme");
    case BT_FONTES: return i18n("Fontes");
    case BT_TRAVA: return m && m->sala.controleAnf ? i18n("Só eu controlo") : i18n("Todos controlam");
    default: return i18n("Sair da sessão");
  }
}
static const char *iconeBotao(int b) {
  switch (b) {
    case BT_COMECAR: return "aj_play";
    case BT_CELULAR: return "aj_user-plus";
    case BT_FILME: return "aj_film";
    case BT_FONTES: return "aj_circle-check";
    case BT_TRAVA: return "aj_users";
    default: return "aj_log-out";
  }
}

static void apertarSala(int b) {
  const JsMotor *m = juntos_motor();
  switch (b) {
    case BT_COMECAR: juntos_comecar(); break;
    case BT_CELULAR: mostrarQr = !mostrarQr; if (mostrarQr) juntos_pedir_celular(); break;
    case BT_FILME: mostrarQr = 0; camada = CAMADA_NADA; pediuEscolherFilme = 1; break;
    case BT_FONTES: mostrarQr = 0; camada = CAMADA_FONTES; foco = 0; break;
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
static int ehVoltar(SDL_Keycode k) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  return nv_tecla_voltar(&e);
}

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
      const JsMotor *mo = juntos_motor();
      int temVazio = mo && mo->sala.n < 5;
      if (foco >= n) foco = n - 1;
      if (ehVoltar(k)) { camada = CAMADA_NADA; mostrarQr = 0; }
      else if (k == SDLK_UP && temVazio) foco = -1;
      else if (k == SDLK_DOWN && foco < 0) foco = 0;
      else if (foco < 0) { if (ehOk(k)) apertarSala(BT_CELULAR); }
      else if (k == SDLK_LEFT && foco > 0) foco--;
      else if (k == SDLK_RIGHT && foco < n - 1) foco++;
      else if (ehOk(k)) apertarSala(b[foco]);
      return 1;
    }
    case CAMADA_FONTES: {
      const JsMotor *m = juntos_motor();
      int n = m ? (m->sala.n < 5 ? m->sala.n : 5) : 0;
      if (foco > n) foco = n;
      if (ehVoltar(k) || (ehOk(k) && foco == 0)) { camada = CAMADA_SALA; foco = 0; }
      else if (k == SDLK_DOWN && foco < n) foco++;
      else if (k == SDLK_UP && foco > 0) foco--;
      else if (ehOk(k) && m && foco > 0 && !strcmp(m->sala.m[foco - 1].id, m->eu)
               && stream_lista_do_alvo(m->sala.ep) && stream_item(stream_atual())) {
        pediuFontes = 1; camada = CAMADA_NADA;
      }
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
  float dt = ultimoQuadro ? fminf((Uint32)(agora - ultimoQuadro) / 1000.0f, .05f) : 0;
  ultimoQuadro = agora;
  agoraUlt = agora;
  if (okSegurando && agora - okDesceEm >= SEGURAR_OK_MS && camada == CAMADA_NADA) {
    okSegurando = 0;
    camada = CAMADA_REACOES; linhaReacao = 0; foco = 0;
  }
  if (!juntos_ativo() && (camada == CAMADA_SALA || camada == CAMADA_REACOES || camada == CAMADA_SAIR || camada == CAMADA_PEDIDO || camada == CAMADA_FONTES))
    camada = CAMADA_NADA;
  if (!noPlayer() && camada == CAMADA_REACOES) camada = CAMADA_NADA;
  if (camada != CAMADA_NADA && camada != camadaVisual) {
    camadaVisual = camada;
    entrada = tempoEntrada = 0;
    memset(focoAnim[camadaVisual], 0, sizeof focoAnim[0]);
    memset(lugarAnim, 0, sizeof lugarAnim);
    folhaScroll = folhaVel = 0;
  }
  entrada = t21_entrada(entrada, dt, camada != CAMADA_NADA);
  float alvoScroll = camada == CAMADA_FONTES ? fmaxf(0, foco - 2) * 112 :
    fmaxf(0, 48 + 40 + 72 + 48 + (foco + 1) * 76.0f + 24 - corpoAltura(948));
  folhaScroll = anim_mola2(&folhaVel, folhaScroll, alvoScroll, dt, NV_MOLA2_SCROLL);
  if (camada != CAMADA_NADA) tempoEntrada += dt;
  else if (entrada <= 0) camadaVisual = CAMADA_NADA;
  for (int c = 1; c < 8; c++) for (int i = 0; i < 16; i++) {
    int alvo = camada == c && (camada == CAMADA_CODIGO ? codPos + 1 :
               camada == CAMADA_REACOES ? foco + 1 + linhaReacao * 6 : foco + 1) == i;
    focoAnim[c][i] = anim_mola(focoAnim[c][i], alvo, dt, NV_MOLA_FOCO);
  }
  for (int i = 0; i < 5; i++) {
    const JsMotor *m = juntos_motor();
    int presente = m && i <= m->sala.n;
    lugarAnim[i] = anim_mola(lugarAnim[i], presente, dt, NV_MOLA_FOCO);
    int local = m && i < m->sala.n && !strcmp(m->sala.m[i].id, m->eu) && fonteLocal(m);
    fonteExp[i] = anim_mola(fonteExp[i], camada == CAMADA_FONTES && foco == i + 1 && local, dt, NV_MOLA_TELA);
  }
  if (ultimaContagem != contagemRestante()) { ultimaContagem = contagemRestante(); contagemAnim = 0; }
  contagemAnim = anim_mola(contagemAnim, 1, dt, NV_MOLA_FOCO);
  pedirCorpo();
}

// --- desenho ---------------------------------------------------------------------------------------
static void texto(TxtEstilo es, const char *s, float x, float y, int c, float a, float maxW) {
  TxtLinha l = maxW > 0 ? txt_linha_corta(es, s, c, c, c, 255, maxW) : txt_linha(es, s, c, c, c, 255);
  txt_desenhar_alpha(l, x, y, a);
}
static float largura(TxtEstilo es, const char *s) { return (float)txt_linha(es, s, 255, 255, 255, 255).w; }

static GfxRect telaReal(void) {
  return (GfxRect){-origemX, -origemY, NV_VTELA_W, NV_VTELA_H};
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
  float h = corpoAltura(fminf(948, 48 + 40 + 72 + 48 + folhaLinhas() * 76.0f + 12 + 40));
  GfxRect r = { 560, (1080 - h) / 2, 800, h };
  float y = r.y + 48;
  int i;
  texto(TXT_CAPTION, i18n("Assistir Juntos"), r.x + 48, y, 200, 1, 0);
  y += 40;
  texto(TXT_ILHA_TITULO, folhaTitulo, r.x + 48, y, 245, 1, r.w - 96);
  y += 72;
  texto(TXT_ILHA_CORPO, nAmigos ? i18n("Quem vai assistir com você") : i18n("Nenhum amigo ainda. Quem tiver o código entra pedindo."),
        r.x + 48, y, 190, 1, r.w - 96);
  y += 48;
  gfx_recorte(r.x, y, r.w, r.y + r.h - 32 - y);
  y -= folhaScroll;
  for (i = 0; i < folhaLinhas(); i++) {
    GfxRect l = { r.x + 32, y, r.w - 64, 64 };
    float f = focoDe(i);
    if (i < nAmigos) {
      plrui_linha_foco(l, T21_R_ROW, f);
      int c = 245;
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
  gfx_sem_recorte();
}

static void desenharCodigo(void) {
  GfxRect r = { 610, 290, 700, 470 };
  int i;
  texto(TXT_CAPTION, i18n("Código da sala"), r.x + 48, r.y + 48, 200, 1, 0);
  txt_bloco_corta(TXT_ILHA_CORPO, i18n("Peça o código a quem abriu a sala. CIMA e BAIXO trocam a letra."),
                  190, 190, 190, r.x + 48, r.y + 92, r.w - 96, 34, 1, 2);
  for (i = 0; i < 6; i++) {
    GfxRect c = { r.x + 64 + i * 98.0f + (i >= 3 ? 20 : 0), r.y + 190, 80, 110 };
    char s[2] = { cod[i], 0 };
    float f = focoDe(i);
    int tinta = botao_superficie(c, f, 1);
    TxtLinha l = txt_linha(TXT_ILHA_TITULO, s, tinta, tinta, tinta, 255);
    txt_desenhar_alpha(l, c.x + (c.w - l.w) / 2, c.y + (c.h - l.h) / 2, 1);
  }
  botao_pilula((GfxRect){ r.x + 48, r.y + 360, r.w - 96, BOTAO_H_SECUNDARIO }, i18n("Entrar"), "aj_users", 0, 1, 0, 1);
}

// --- lobby: o cinema (Velvet Cinema v2) ----------------------------------------------------
// Tela com a arte do filme acesa, fileira de poltronas curva e botoes. Sem vidro:
// tudo e cor solida e a arte, entao fica certo com o vidro desligado de fabrica.
static const char *arteSala(void) {
  static char poster[NV_ARTE_URL_MAX];
  static int idx = -1;
  const JsMotor *m = juntos_motor();
  const CatItem *ci;
  int i, n;
#ifdef NV_SHOT_HOOKS
  if (shotArte[0]) return shotArte;
#endif
  if (!m || !m->temSala || !m->sala.poster[0]) return NULL;
  if (strcmp(poster, m->sala.poster)) {
    snprintf(poster, sizeof poster, "%s", m->sala.poster);
    idx = -1;
    for (i = 0, n = cat_n(); i < n; i++) {
      ci = cat_item(i);
      if (ci && !strcmp(ci->poster, poster)) { idx = i; break; }
    }
  }
  ci = idx >= 0 ? cat_item(idx) : NULL;
  return ci && ci->backdrop[0] ? ci->backdrop : NULL;
}

// A tela do cinema: arte em cover, canto vivo, veu que sobe da base.
static void telaCinema(GfxRect r, float luz, float a) {
  const char *arte = arteSala();
  GLuint t = arte ? tex_obter_hero(arte) : 0;
  if (t) {
    gfx_tex_aspect_atual = tex_aspecto(arte);
    gfx_card_forcar_cover_atual = 1.0f;
    gfx_rect(r, t, GFX_CARD, 1, 0, 0, 0.0f, luz, luz, luz, a);
    gfx_card_forcar_cover_atual = 0.0f;
    gfx_tex_aspect_atual = 0.0f;
  } else gfx_cor(r, 0, 0.14f, 0.11f, 0.18f, a);
  gfx_veu_base(r, 0, 0.7f, 0.9f * a);
  gfx_cor(r, 0, 0.05f, 0.045f, 0.07f, 0.18f * a);
  gfx_veu_css(r, 2, 1.0f, 1.0f, 0.6f * a);   // sombra a esquerda: o texto da tela le sobre qualquer arte
}

static float entradaItem(int i) {
  return revela_entra(1, revela_onda_atraso(i, 0), 1 + (Uint32)(tempoEntrada * 1000));
}

static void textoCentro(TxtEstilo es, const char *s, float cx, float y, int c, float a, float maxW) {
  TxtLinha l = txt_linha_corta(es, s, c, c, c, 255, maxW);
  txt_desenhar_alpha(l, cx - l.w / 2.0f, y, a);
}

#define LUGAR_W 234.0f
#define LUGAR_H 214.0f
static float lugarX(int i) {
  float passo = fminf(286, (NV_VTELA_W - 128 - LUGAR_W) / 4);
  return 960.0f + (i - 2) * passo - LUGAR_W / 2;
}
static float lugarY(int i) {
  return 530.0f + fminf(0, NV_VTELA_H - 1080) * .5f + (i - 2) * (i - 2) * 8.0f;
}

static void desenharLugar(int i, const JsMembro *p, int vazio, float f, float a) {
  GfxRect c = { lugarX(i), lugarY(i), LUGAR_W, LUGAR_H };
  GfxRect av = { c.x + (LUGAR_W - 78) / 2, c.y + 30, 78, 78 };
  float cx = c.x + LUGAR_W / 2;
  float e = ajustes_animacoes_reduzidas_usuario() ? 1 : entradaItem(i);
  float s = (.8f + .2f * lugarAnim[i]) * (1 + .06f * f);
  a *= e;
  gfx_transformar(cx, c.y + LUGAR_H / 2, s, origemX, origemY - NV_FOCO_LIFT * f + (1 - e) * NV_ENTRA_DY);
  t21_ilha(c, T21_R_SHEET, a);
  plrui_linha_foco(c, T21_R_SHEET, f * a);
  if (vazio) {
    botao_disco(av, "aj_user-plus", f, a);
    textoCentro(TXT_ILHA_NOME, i18n("Mais um lugar"), cx, c.y + 122, 245, a, LUGAR_W - 20);
    textoCentro(TXT_CAPTION, i18n("Convide um amigo"), cx, c.y + 158, 190, a, LUGAR_W - 20);
  } else {
    const JsMembro *m0 = p;
    int pronto = !strcmp(m0->st, "pronto") || !strcmp(m0->st, "tocando");
    char st[120];
    const JsMotor *m = juntos_motor();
    rec_avatar(av, m0->av, m0->nome, m0->id, m0->on ? a : 0.4f * a);
    char nome[160];
    if (m && !strcmp(m0->id, m->eu)) snprintf(nome, sizeof nome, "%s%s%s", i18n("Você"), m0->anf ? " · " : "", m0->anf ? i18n("anfitrião") : "");
    else snprintf(nome, sizeof nome, "%s", m0->nome);
    textoCentro(TXT_ILHA_NOME, nome, cx, c.y + 122, 245, a, LUGAR_W - 20);
    if (!strcmp(m0->st, "carregando") && m0->pct > 0) snprintf(st, sizeof st, "%s %d%%", estadoTexto(m0), m0->pct);
    else snprintf(st, sizeof st, "%s", pronto ? i18n("pronto") : estadoTexto(m0));
    if (pronto) {
      float w = largura(TXT_CAPTION, st);
      gfx_icone((GfxRect){ cx - (w + 30) / 2, c.y + 160, 24, 24 }, "aj_check", 0.55f, 0.92f, 0.65f, a);
      texto(TXT_CAPTION, st, cx - (w + 30) / 2 + 30, c.y + 160, 200, a, 0);
    } else textoCentro(TXT_CAPTION, st, cx, c.y + 160, 170, a, LUGAR_W - 20);
  }
  gfx_transformar(960, 540, 1, origemX, origemY);
}

static int todosProntos(const JsMotor *m) {
  int i;
  for (i = 0; i < m->sala.n; i++)
    if (strcmp(m->sala.m[i].st, "pronto") && strcmp(m->sala.m[i].st, "tocando")) return 0;
  return m->sala.n > 0;
}

static void cabecalhoSala(float a) {
  char c[96];
  gfx_icone((GfxRect){ 80, 54, 36, 36 }, "aj_users", 0.92f, 0.92f, 0.9f, a);
  txt_tracking(TXT_V2_KICK, i18n("Assistir Juntos"), 245, 245, 245, 136, 60, a, 2.5f);
  snprintf(c, sizeof c, "%s", i18n("Velvet Cinema · Sala privada"));
  gfx_cor((GfxRect){ 1840 - largura(TXT_ILHA_NOME, c) - 24, 68, 8, 8 }, 0.5f, 0.85f, 0.85f, 0.85f, a);
  texto(TXT_ILHA_NOME, c, 1840 - largura(TXT_ILHA_NOME, c), 54, 225, a, 0);
}

// Remote members expose a verdict, not their private stream metadata.
static const char *plataformaFonte(const JsMembro *p) {
  if (!strcmp(p->plat, "lg")) return "LG · webOS";
  if (!strcmp(p->plat, "wgt")) return "Samsung · Tizen";
  if (!strcmp(p->plat, "android")) return "Android TV";
  if (!strcmp(p->plat, "mac")) return "macOS";
  return p->plat[0] ? p->plat : i18n("TV");
}

static const Stream *fonteLocal(const JsMotor *m) {
  return stream_lista_do_alvo(m->sala.ep) ? stream_item(stream_atual()) : NULL;
}

static void fonteSelos(const Stream *s, const JsImpressao *imp, float x, float y, float a) {
  int altura = s ? s->altura : imp ? imp->altura : 0;
  int hdr = s ? (s->dolbyVision ? FMT_DV : badges_fonte_hdr_marca(s->badges)) : -1;
  char tamanho[64];
  double bytes = s ? (s->tamanhoBytes ? (double)s->tamanhoBytes : s->tamanhoMB * 1048576.0) : imp ? (double)imp->bytes : 0;
  if (altura) {
    FormatoMarca f = altura >= 2160 ? FMT_4K : altura >= 1080 ? FMT_1080 : altura >= 720 ? FMT_720 : FMT_SD;
    float w = marca_formato_largura(f, 20) + 24;
    gfx_cor((GfxRect){ x, y - 6, w, 32 }, 0.5f, 1, 1, 1, 0.08f * a);
    marca_formato(f, x + 12, y, 20, 0.93f, 0.93f, 0.92f, a);
    x += w + 16;
  }
  if (hdr >= 0) x += marca_formato((FormatoMarca)hdr, x, y, 20, 0.8f, 0.8f, 0.8f, a) + 16;
  if (s) {
    const char *audio[] = { "a-atmos", "a-truehd", "a-dtshdma", "a-dts", "a-ddp", "a-dd" };
    FormatoMarca marcas[] = { FMT_ATMOS, FMT_TRUEHD, FMT_DTSHD, FMT_DTS, FMT_DDP, FMT_DD };
    int j;
    for (j = 0; j < 6; j++) if ((j == 0 && s->dolbyAtmos) || (s->badges & badges_bit(audio[j]))) {
      x += marca_formato(marcas[j], x, y, 20, 0.8f, 0.8f, 0.8f, a) + 16;
      break;
    }
  }
  if (bytes > 0) {
    snprintf(tamanho, sizeof tamanho, "%.1f GB", bytes / 1073741824.0);
    texto(TXT_ILHA_META, tamanho, x, y - 2, 190, a, 120);
    x += largura(TXT_ILHA_META, tamanho) + 20;
  }
  texto(TXT_ILHA_META, s && s->provedor[0] ? s->provedor : i18n("Detalhes não compartilhados"), x, y - 2, 165, a, 680 - (x - 580));
}

static void desenharFontes(void) {
  const JsMotor *m = juntos_motor();
  GfxRect painelFontes = { 460, 124, 1380, 868 };
  float a = 1, y = 306;
  float statusX = 460 + corpoBase.w * .62f;
  float statusW = corpoBase.w * .38f - 64;
  int i, n;
  char sala[128];
  if (!m) return;
  n = m->sala.n < 5 ? m->sala.n : 5;
  (void)painelFontes;
  texto(TXT_ILHA_TITULO, i18n("Cada TV, sua fonte"), 500, 160, 245, a, corpoBase.w - 80);
  texto(TXT_ILHA_META, i18n("Cada um assiste pela própria conta. Se a sua fonte for outra versão, o app avisa."), 500, 222, 180, a, corpoBase.w - 80);
  gfx_cor((GfxRect){ 500, 278, corpoBase.w - 80, 1 }, 0, 1, 1, 1, 0.08f * a);

  if (!corpoDesenhando && m->sala.poster[0]) {
    GLuint t = tex_obter(m->sala.poster);
    if (t) {
      gfx_tex_aspect_atual = tex_aspecto(m->sala.poster);
      gfx_rect((GfxRect){ 100, 176, 240, 360 }, t, GFX_CARD, 0, 0, 0, 0.04f, 1, 1, 1, a);
      gfx_tex_aspect_atual = 0;
    }
  }
  if (!corpoDesenhando) {
    texto(TXT_ILHA_TITULO, m->sala.titulo, 80, 572, 245, a, 340);
    snprintf(sala, sizeof sala, "%s · %s", i18n("Código da sala"), m->sala.codigo);
    texto(TXT_ILHA_META, sala, 80, 640, 205, a, 340);
  }

  gfx_recorte(484, 288, corpoBase.w - 48, corpoBase.h - 310);
  y -= folhaScroll;
  for (i = 0; i < n; i++) {
    const JsMembro *p = &m->sala.m[i];
    int eu = !strcmp(p->id, m->eu), mesma = !strcmp(p->fonte, "mesma"), parecida = !strcmp(p->fonte, "parecida");
    int diferente = !strcmp(p->fonte, "diferente");
    const Stream *s = eu ? fonteLocal(m) : NULL;
    const JsImpressao *imp = !s && p->anf ? &m->sala.imp : NULL;
    const char *icone = mesma ? "aj_circle-check" : parecida ? "aj_circle-dot" : "aj_triangle-alert";
    const char *impacto = mesma ? i18n("Mesmo arquivo. Sincronia automática.") : parecida ? i18n("Arquivo parecido. A sincronia pode variar.") : diferente ? i18n("Duração diferente. Algumas cenas podem desalinhar.") : i18n("Aguardando uma fonte para sincronizar.");
    float r = mesma ? 0.55f : 0.96f, g = mesma ? 0.90f : parecida ? 0.78f : 0.56f, b = mesma ? 0.70f : 0.42f;
    float alvo = foco == i + 1 && eu && s ? 1 : 0;
    float h = 96 + 62 * fonteExp[i];
    plrui_linha_foco((GfxRect){ 484, y - 10, corpoBase.w - 48, h }, T21_R_ROW, focoDe(i + 1) * a);
    rec_avatar((GfxRect){ 504, y + 6, 56, 56 }, p->av, p->nome, p->id, p->on ? a : 0.45f * a);
    texto(TXT_ILHA_NOME, p->nome, 580, y, 245, a, statusX - 600);
    snprintf(sala, sizeof sala, "%s%s%s", plataformaFonte(p), eu ? " · " : "", eu ? i18n("Sua TV") : "");
    texto(TXT_ILHA_META, sala, 580, y + 27, 170, a, statusX - 600);
    if (s || imp) fonteSelos(s, imp, 580, y + 56, a);
    else texto(TXT_ILHA_META, i18n("Detalhes não compartilhados"), 580, y + 56, 165, a, statusX - 600);
    gfx_icone((GfxRect){ statusX, y + 1, 26, 26 }, icone, r, g, b, a);
    TxtLinha status = txt_linha_corta(TXT_ILHA_NOME, p->fonte[0] ? fonteTexto(p->fonte) : estadoTexto(p), r * 255, g * 255, b * 255, 255, statusW);
    txt_desenhar_alpha(status, statusX + 38, y, a);
    txt_bloco_corta(TXT_ILHA_APOIO, impacto, 190, 190, 190, statusX + 38, y + 37, statusW, 24, a, 2);
    if (fonteExp[i] > 0.01f) botao_pilula((GfxRect){ 580, y + 82, botao_largura(i18n("Trocar fonte"), "aj_monitor-cog", 0), BOTAO_H_SECUNDARIO }, i18n("Trocar fonte"), "aj_monitor-cog", focoDe(i + 1) * alvo, 0, 0, a * fonteExp[i]);
    y += h + 2;
  }
  gfx_sem_recorte();
  botao_pilula((GfxRect){ 500, corpoBase.y + corpoBase.h - 104, botao_largura(i18n("Voltar à sala"), "aj_users", 1), BOTAO_H_PRIMARIO },
               i18n("Voltar à sala"), "aj_users", focoDe(0), 1, 0, a);
}

static void desenharSala(void) {
  const JsMotor *m = juntos_motor();
  float baixo = fminf(0, NV_VTELA_H - 1080) * .5f;
  float esquerda = -origemX + 80, direita = NV_VTELA_W - origemX - 80;
  GfxRect tela = { 360, 144, 1200, 456 };
  float a = 1;
  int i, b[BT_N], n = botoesSala(b), nm, vazio;
  char linha[200];
  if (!m) return;
  nm = m->sala.n < 5 ? m->sala.n : 5;
  vazio = nm < 5;
  gfx_cor(telaReal(), 0, 0.06f, 0.055f, 0.08f, a);
  cabecalhoSala(a);
  telaCinema(tela, 0.7f, a);
  snprintf(linha, sizeof linha, i18n("Sua sessão privada · %d de %d lugares"), nm, 5);
  texto(TXT_CAPTION, linha, 160, tela.y + 56, 235, a, tela.w - 96);
  txt_bloco_corta(TXT_ILHA_TITULO, i18n("Os melhores lugares são juntos"), 250, 250, 250, 160, tela.y + 92, 460, 52, a, 2);
  texto(TXT_ILHA_NOME, m->temSala && m->sala.titulo[0] ? m->sala.titulo : juntos_titulo(), 160, tela.y + 214, 235, a, 900);
  if (mostrarQr) {
    GfxRect q = { tela.x + tela.w - 48 - 220, tela.y + 56, 220, 220 };
    GLuint t = qrTextura(juntos_url_celular());
    gfx_cor((GfxRect){ q.x - 12, q.y - 12, q.w + 24, q.h + 24 }, 0.06f, 1, 1, 1, a);
    if (t) gfx_rect(q, t, GFX_SNAP, 0, 0, 0, 0, 1, 1, 1, a);
    if (juntos_codigo()[0]) {
      char c[64];
      snprintf(c, sizeof c, i18n("Código %.3s-%s"), juntos_codigo(), juntos_codigo() + 3);
      texto(TXT_ILHA_NOME, c, q.x + (q.w - largura(TXT_ILHA_NOME, c)) / 2, q.y + q.h + 24, 245, a, q.w);
    }
  }
  for (i = 0; i < 5; i++) {
    if (i < nm) desenharLugar(i, &m->sala.m[i], 0, 0, a);
    else if (i == nm) desenharLugar(i, NULL, 1, focoDe(-1), a);
  }
  {
    int pr = !mostrarQr && todosProntos(m);
    const char *s = mostrarQr ? i18n("Escaneie com o celular para mandar mensagens e controlar a sessão.")
        : pr ? i18n("Todos prontos. O filme está preparado em cada TV.") : i18n("Esperando os outros ficarem prontos");
    float w = largura(TXT_ILHA_NOME, s) + (pr ? 48 : 0), x = 960 - w / 2;
    if (pr) gfx_icone((GfxRect){ x, 806 + baixo, 36, 36 }, "aj_circle-check", 0.55f, 0.92f, 0.65f, a);
    texto(TXT_ILHA_NOME, s, x + (pr ? 48 : 0), 806 + baixo, 235, a, NV_VTELA_W - 160);
  }
  {
    float xe = esquerda, xd = direita;
    for (i = 0; i < n; i++) {
      int prim = b[i] == BT_COMECAR, esq = b[i] <= BT_FONTES;
      if (!esq) continue;
      float w = botao_largura(rotuloBotao(b[i]), iconeBotao(b[i]), prim), h = prim ? BOTAO_H_PRIMARIO : BOTAO_H_SECUNDARIO;
      botao_pilula((GfxRect){ xe, 905 + baixo + (BOTAO_H_PRIMARIO - h) / 2, w, h }, rotuloBotao(b[i]), iconeBotao(b[i]),
                   focoDe(i), prim, 0, a * entradaItem(i));
      xe += w + BOTAO_GAP;
    }
    for (i = n - 1; i >= 0; i--) {
      float w;
      if (b[i] <= BT_FONTES) continue;
      w = botao_largura(rotuloBotao(b[i]), iconeBotao(b[i]), 0);
      xd -= w;
      botao_pilula((GfxRect){ xd, 905 + baixo + (BOTAO_H_PRIMARIO - BOTAO_H_SECUNDARIO) / 2, w, BOTAO_H_SECUNDARIO },
                   rotuloBotao(b[i]), iconeBotao(b[i]), focoDe(i), 0, 0, a * entradaItem(i));
      xd -= BOTAO_GAP;
    }
  }
  texto(TXT_CAPTION, i18n("Setas para mover · OK para escolher · Voltar para retornar"), esquerda, 1010 + baixo, 170, a, direita - esquerda);
}

static void desenharPergunta(const char *titulo, const char *corpo, const char *rostoUrl, const char *rostoNome,
                             const char *b0, const char *b1) {
  GfxRect r = { 610, 360, 700, 340 };
  float w0 = botao_largura(b0, NULL, 1), w1 = botao_largura(b1, NULL, 0);
  float x = r.x + 48;
  if (rostoNome) { rec_avatar((GfxRect){ r.x + 48, r.y + 44, 64, 64 }, rostoUrl, rostoNome, rostoNome, 1); x = r.x + 132; }
  texto(TXT_ILHA_TITULO, titulo, x, r.y + 52, 245, 1, r.w - (x - r.x) - 48);
  if (corpo) txt_bloco_corta(TXT_ILHA_CORPO, corpo, 190, 190, 190, r.x + 48, r.y + 136, r.w - 96, 34, 1, 3);
  botao_pilula((GfxRect){ r.x + 48, r.y + r.h - 48 - BOTAO_H_SECUNDARIO, w0, BOTAO_H_SECUNDARIO }, b0, NULL, focoDe(0), 1, 0, 1);
  botao_pilula((GfxRect){ r.x + 48 + w0 + BOTAO_GAP, r.y + r.h - 48 - BOTAO_H_SECUNDARIO, w1, BOTAO_H_SECUNDARIO }, b1, NULL,
               focoDe(1), 0, 0, 1);
}

static void desenharReacoes(void) {
  GfxRect r;
  float wFrases = 0, wDiscos = 6 * 88.0f + 5 * 36.0f, w;
  int i;
  for (i = 0; i < 4; i++) wFrases += botao_largura(frase(FRASES[i]), NULL, 0) + (i ? 12 : 0);
  w = (wFrases > wDiscos ? wFrases : wDiscos) + 80;
  r = (GfxRect){ (1920 - w) / 2, 820, w, 204 };
  for (i = 0; i < 6; i++) {
    GfxRect d = { r.x + (w - wDiscos) / 2 + i * 124.0f, r.y + 24, 88, 88 };
    char ic[40];
    if (i < 5) iconeReacao(ic, sizeof ic, REACOES[i]); else snprintf(ic, sizeof ic, "aj_users");
    float f = focoDe(i);
    gfx_transformar(d.x + d.w / 2, d.y + d.h / 2, 1 + .08f * f, origemX, origemY);
    botao_disco(d, ic, f, entradaItem(i));
    gfx_transformar(960, 540, 1, origemX, origemY);
  }
  {
    float x = r.x + (w - wFrases) / 2;
    for (i = 0; i < 4; i++) {
      const char *t = frase(FRASES[i]);
      float w = botao_largura(t, NULL, 0);
      botao_pilula((GfxRect){ x, r.y + 128, w, BOTAO_H_SECUNDARIO }, t, NULL, focoDe(i + 6), 0, 0, 1);
      x += w + 12;
    }
  }
}

// Rostos de quem esta junto, no canto, quando o OSD do player sobe.
static void desenharRostos(void) {
  const JsMotor *m = juntos_motor();
  float x, y = 124;   // logo abaixo da pilula do relogio, na direita
  int i, n;
#ifdef NV_SHOT_HOOKS
  if (!m || !m->temSala || !(player_controles_visiveis() || shotPlayer)) return;
#else
  if (!m || !m->temSala || !player_controles_visiveis()) return;
#endif
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
  float y = 210;
  int i;
  for (i = 0; i < BOLHAS; i++) {
    float a, w;
    if (!bolha[i].ate || agora >= bolha[i].ate) continue;
    a = ajustes_animacoes_reduzidas_usuario() ? 1 :
        anim_suave(fminf((agora - bolha[i].desde) / 220.0f, (bolha[i].ate - agora) / 220.0f));
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

// Contagem 3-2-1 antes do play: o servidor marca o play num t0 futuro (3 x 500 ms).
static int contagemRestante(void) {
  const JsMotor *m = juntos_motor();
  double falta;
#ifdef NV_SHOT_HOOKS
  if (shotContagem) return shotContagem;
#endif
  if (!m || !m->temSala || m->sala.rel.estado != JSE_TOCANDO) return 0;
  falta = m->sala.rel.t0 - jsr_servidor(&m->rel, (double)SDL_GetTicks64());
  if (falta <= 0 || falta > 1500) return 0;
  return (int)ceil(falta / 500.0);
}

static void desenharContagem(int num) {
  const JsMotor *m = juntos_motor();
  GfxRect tela = { 220, 110, 1480, 820 };
  char n[8];
  int i, nm;
  float x0;
  if (!m) return;
  nm = m->sala.n < 5 ? m->sala.n : 5;
  gfx_cor(telaReal(), 0, 0.04f, 0.035f, 0.06f, 1.0f);
  telaCinema(tela, 0.45f, 1);
  gfx_cor(tela, 0, 0.04f, 0.035f, 0.06f, 0.45f);
  textoCentro(TXT_CAPTION, i18n("Todos prontos"), 960, 190, 235, 1, 1200);
  textoCentro(TXT_ILHA_TITULO, i18n("As luzes se apagam. Juntos."), 960, 236, 250, 1, 1300);
  snprintf(n, sizeof n, "%d", num);
  gfx_transformar(960, 450, .8f + .2f * contagemAnim, origemX, origemY);
  textoCentro(TXT_RANK_GRANDE, n, 960, 340, 255, contagemAnim, 300);
  gfx_transformar(960, 540, 1, origemX, origemY);
  x0 = 960 - nm * 92.0f / 2;
  for (i = 0; i < nm; i++) {
    const JsMembro *p = &m->sala.m[i];
    GfxRect av = { x0 + i * 92.0f + 7, 610, 78, 78 };
    rec_avatar(av, p->av, p->nome, p->id, 1);
  }
  textoCentro(TXT_ILHA_NOME, i18n("Começando juntos"), 960, 730,
              230, 1, 1200);
  textoCentro(TXT_CAPTION, m->temSala && m->sala.titulo[0] ? m->sala.titulo : juntos_titulo(), 960, 776, 190, 1, 1200);
}

static int camadaCorpo(Camada c) {
  return c != CAMADA_NADA && c != CAMADA_SALA;
}

static void desenharCorpo(GfxRect c, float a, void *u) {
  Camada visual = (Camada)(intptr_t)u;
  float opacidade = gfx_opacidade_grupo;
  Camada anterior = camadaVisual;
  camadaVisual = visual;
  origemX = c.x - corpoBase.x;
  origemY = c.y + (noPlayer() ? 0 : NV_ILHA_H_ABERTA) - corpoBase.y;
  gfx_transformar(0, 0, 1, origemX, origemY);
  gfx_opacidade_grupo *= a;
  corpoDesenhando = 1;
  switch (visual) {
    case CAMADA_FOLHA: desenharFolha(); break;
    case CAMADA_CODIGO: desenharCodigo(); break;
    case CAMADA_FONTES: desenharFontes(); break;
    case CAMADA_REACOES: desenharReacoes(); break;
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
    default: break;
  }
  corpoDesenhando = 0;
  gfx_opacidade_grupo = opacidade;
  gfx_sem_transformar();
  camadaVisual = anterior;
}

static void pedirCorpo(void) {
  if (camadaCorpo(camada)) {
    switch (camada) {
      case CAMADA_FOLHA: {
        float h = corpoAltura(fminf(948, 48 + 40 + 72 + 48 + folhaLinhas() * 76.0f + 12 + 40));
        corpoBase = (GfxRect){560, (1080 - h) / 2, 800, h}; break;
      }
      case CAMADA_CODIGO: corpoBase = (GfxRect){610, 290, 700, 470}; break;
      case CAMADA_FONTES: corpoBase = (GfxRect){460, 124, fminf(1380, (NV_LAYOUT_REAL_W - 128) / gfx_escala_ui()), corpoAltura(868)}; break;
      case CAMADA_REACOES: {
        float frases = 0;
        for (int i = 0; i < 4; i++) frases += botao_largura(frase(FRASES[i]), NULL, 0) + (i ? 12 : 0);
        float w = fmaxf(frases, 6 * 88.0f + 5 * 36.0f) + 80;
        corpoBase = (GfxRect){(1920 - w) / 2, 820, w, 204}; break;
      }
      default: corpoBase = (GfxRect){610, 360, 700, 340}; break;
    }
    if (noPlayer()) {
      PlrIlhaPedido p = {0};
      p.w = corpoBase.w; p.h = corpoBase.h;
      p.corpo = desenharCorpo; p.u = (void *)(intptr_t)camada;
      p.texto = i18n("Assistir Juntos"); p.video = 1;
      plrilha_pedir(&p);
    } else ilha_corpo(corpoBase.w, corpoBase.h + NV_ILHA_H_ABERTA, desenharCorpo, (void *)(intptr_t)camada);
  }
}

void juntosui_desenhar(Uint32 agora) {
  float esc = gfx_escala_entrar();
  origemX = (NV_VTELA_W - 1920) / 2;
  origemY = (NV_VTELA_H - 1080) / 2;
  gfx_transformar(960, 540, 1, origemX, origemY);
  if (noPlayer() && juntos_ativo()) { desenharRostos(); desenharBolhas(agora); }
  float opacidade = gfx_opacidade_grupo;
  gfx_opacidade_grupo *= anim_suave(entrada);
  int folha = camadaVisual != CAMADA_SALA && camadaVisual != CAMADA_FONTES && camadaVisual != CAMADA_REACOES;
  if (folha) gfx_transformar(960, 540, T21_SHEET_SCALE + (1 - T21_SHEET_SCALE) * anim_suave(entrada), origemX, origemY);
  if (camadaVisual == CAMADA_SALA) desenharSala();
  gfx_transformar(960, 540, 1, origemX, origemY);
  gfx_opacidade_grupo = opacidade;
  { int c = juntos_ativo() ? contagemRestante() : 0; if (c) desenharContagem(c); }
  gfx_sem_transformar();
  gfx_escala_sair(esc);
}
