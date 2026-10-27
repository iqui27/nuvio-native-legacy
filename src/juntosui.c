// Watch Together: a interface — ver juntosui.h.
#include "juntosui.h"
#include "ajustes.h"
#include "botoes.h"
#include "badges.h"
#include "catalogo.h"
#include "central.h"
#include "gfx.h"
#include "idioma.h"
#include "ilha.h"
#include "plrilha.h"
#include "juntos.h"
#include "juntosfonte.h"
#include "logotitulo.h"
#include "perfis.h"
#include "player.h"
#include "qr.h"
#include "recomenda.h"
#include "socialvis.h"
#include "tex_cache.h"
#include "text.h"
#include "tela21.h"
#include "revela.h"
#include "salvospainel.h"
#include "layout.h"
#include "teclavoltar.h"
#include "focus.h"
static GfxRect navigationRects[16];
static int navigationIds[16], navigationCount;
static void navigationTarget(int id,GfxRect r){if(navigationCount<16){navigationIds[navigationCount]=id;navigationRects[navigationCount++]=r;}}
#ifdef NV_SHOT_HOOKS
#include <assert.h>
#endif
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

typedef enum { CAMADA_NADA = 0, CAMADA_FOLHA, CAMADA_CODIGO, CAMADA_SALA, CAMADA_SAIR, CAMADA_PEDIDO, CAMADA_REACOES, CAMADA_FONTES, CAMADA_PAINEL } Camada;
static Camada camada;
static int foco, linhaReacao, focoSala;
static Camada camadaVisual;
static GfxRect corpoBase;
static int corpoDesenhando;
#define LUGAR_MS 860.0f   // seat clock: 600 ms arrival, light 160 + 700 ms
static float entrada, tempoEntrada, focoAnim[9][16], lugarAnim[5], fonteExp[5];
static Uint32 ultimoQuadro;
static float contagemAnim, prontoAnim[5], prontinhaAnim;
static char prontinha[200];   // readiness line under the seats, fades in on change
static char lugarId[5][JSP_ID];
static int lobbyAberto, escolhendoFilme;
static void abrirSala(void);
static int todosProntos(const JsMotor *m);
static int ultimaContagem, qrPendente;
static Uint32 ultimoDrift;
static double driftChip;
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
// QR and the invite panel (code + QR + friends), over the lobby
static GLuint texQr;
static char texQrDe[200];
static int mostrarQr, focoConv;
static float convAnim, convFoco[FOLHA_AMIGOS], anelAnim[5];
static int focoAcao;   // last action-row focus, restored coming down from seats

static void poeBolha(const char *quem, const char *icone, const char *txt, unsigned ms);
static const Stream *fonteLocal(const JsMotor *m);
static int contagemRestante(void);
static void pedirCorpo(void);
static float salaFolhaH(void);
static void salaFolhaDesenhar(float x, float y, float w, float a);
static float corpoAltura(float h) {
  return fminf(h, (NV_LAYOUT_REAL_H - NV_ILHA_Y - 24) / gfx_escala_ui() - NV_ILHA_H_ABERTA);
}
static float corpoAlfaAtual;
// Cinema start: the lobby recedes over the playing film until "Enter session".
static int cinema;
static float cinemaAnim;
static GfxMini cinemaVeu;
static GfxRect cinemaVeuRect;
static int cinemaVeuHdr;
static char ilhaTexto[160];   // the session line on the clock island this frame
static float painelAlvo;      // sidebar scroll target

// Submission timings normally; opt-in GPU fences only for isolated profiling.
static struct { double ms[6]; unsigned frames; Uint64 last; int sync, cinema; } custo;
static const char *custoNome[] = { "background", "header", "seats", "actions", "cinema", "overlay" };
static void custoInicio(void) {
  static int iniciou;
  if (!iniciou) {
    const char *v = getenv("NUVIO_JUNTOS_CUSTO");
    custo.sync = v && !strcmp(v, "sync"); iniciou = 1;
  }
  if (custo.cinema != cinema) {
    memset(custo.ms, 0, sizeof custo.ms); custo.frames = 0; custo.cinema = cinema;
  }
  if (custo.sync) glFinish();
  custo.last = SDL_GetPerformanceCounter();
}
static void custoCamada(int i) {
  if (custo.sync) glFinish();
  Uint64 now = SDL_GetPerformanceCounter();
  custo.ms[i] += (now - custo.last) * 1000.0 / SDL_GetPerformanceFrequency();
  custo.last = now;
}
static void custoFim(void) {
  if (++custo.frames < 120) return;
  printf("[juntos-custo] %s %s frames=%u", custo.sync ? "gpu-fenced" : "submit", cinema ? "cinema" : "lobby", custo.frames);
  for (int i = 0; i < 6; i++) { printf(" %s=%.3fms", custoNome[i], custo.ms[i] / custo.frames); custo.ms[i] = 0; }
  printf("\n"); custo.frames = 0;
}
#ifdef NV_SHOT_HOOKS
static int shotOsd;
void juntosui_shot_osd(int sim) { shotOsd = sim; }
float juntosui_shot_cinema(void) { return cinema ? cinemaAnim : -1; }
const char *juntosui_shot_ilha(void) { return ilhaTexto; }
static int osdVisivel(void) { return player_controles_visiveis() || shotOsd; }
#else
static int osdVisivel(void) { return player_controles_visiveis(); }
#endif
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
void juntosui_shot_camada(int c, int f) { camada = (Camada)c; foco = f; linhaReacao = 0; corpoAlfaAtual = 0; }
float juntosui_shot_entrada(void) { return entrada; }
float juntosui_shot_foco(int i) { return focoDe(i); }
int juntosui_shot_foco_atual(void) { return foco; }
int juntosui_shot_camada_atual(void) { return camada; }
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
      if (!strcmp(e->k, "comecar")) mostrarQr = 0;
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
  else if (!strcmp(k, "auth")) t = i18n("Entre na sua conta para enviar");
  else if (!strcmp(k, "ep invalido")) t = i18n("A sessão precisa de um ID IMDb e episódio válidos");
  else if (!strcmp(k, "json invalido")) t = i18n("Os dados da sessão não são um JSON válido");
  else if (!strcmp(k, "infantil")) t = i18n("Este é um perfil infantil: as ações de administrador não ficam disponíveis aqui.");
  else if (!strcmp(k, "juntos indisponivel")) t = i18n("Acesso ao serviço indisponível");
  else if (!strcmp(k, "tente de novo")) t = i18n("Não foi possível carregar. Tente de novo mais tarde.");
  if (noPlayer()) player_toast_ex(t, 5000, "aj_triangle-alert", 1);
  else ilha_avisar("juntos:erro", ILHA_ERRO, "aj_triangle-alert", t, 6000u, 0);
}

void juntosui_sala_abriu(void) {
  if (escolhendoFilme || camada == CAMADA_CODIGO) { foco = focoSala; escolhendoFilme = 0; }
  else if (camada != CAMADA_SALA) foco = focoAcao = 0;
  if (camada != CAMADA_SALA) printf("[juntos] lobby: shown (focus %d)\n", foco);
  camada = CAMADA_SALA;
  lobbyAberto = 1;
  qrPendente = mostrarQr && !juntos_url_celular()[0];
  if (!qrPendente) mostrarQr = 0;
}

void juntosui_sala_fechou(void) {
  if (camada == CAMADA_SALA || camada == CAMADA_SAIR || camada == CAMADA_PEDIDO || camada == CAMADA_REACOES || camada == CAMADA_FONTES)
    camada = CAMADA_NADA;
  memset(bolha, 0, sizeof bolha);
  lobbyAberto = escolhendoFilme = qrPendente = cinema = 0;
  mostrarQr = 0;
  gfx_mini_liberar(&cinemaVeu);
}

void juntosui_fonte_diferente(int v, int pal) {
  const char *t = NULL;
  if (v == JSF_NENHUMA) t = i18n("Nenhuma fonte encontrada nos seus addons para esta sessão");
  else if (v == JSF_DIFERENTE) t = pal ? i18n("Sua fonte é uma versão PAL, mais rápida. Pode desalinhar.")
                                       : i18n("Sua fonte tem duração diferente. Pode desalinhar em cenas.");
  if (t) aviso("juntos:fonte", t, "aj_triangle-alert", NULL, 7000u);
}

void juntosui_perguntar_sair(void) {
  if (camada == CAMADA_SALA) focoSala = foco;
  camada = CAMADA_SAIR; foco = 1;
}
int juntosui_escolhendo_filme(void) { return escolhendoFilme; }
int juntosui_voltar_filme(void) {
  if (!escolhendoFilme) return 0;
  juntosui_sala_abriu();
  return 1;
}

// --- folha do anfitriao --------------------------------------------------------------------------
static void carregarAmigos(void);
void juntosui_abrir_folha(int idx, int t, int e) {
  if (!juntos_disponivel()) {
    ilha_avisar("juntos:erro", ILHA_INFO, "aj_users", i18n("Ligue a conta Nuvio e o Social para assistir junto"), 5000u, 0);
    return;
  }
  if (juntos_ativo()) {
    const CatItem *ci = cat_item(idx);
    if (escolhendoFilme && ci) juntos_escolher_titulo(ci->imdb, t, e, ci->titulo, ci->poster);
    juntosui_sala_abriu(); return;
  }
  folhaIdx = idx; folhaT = t; folhaE = e;
  { const CatItem *ci = cat_item(idx); snprintf(folhaTitulo, sizeof folhaTitulo, "%s", ci ? ci->titulo : ""); }
  carregarAmigos();
  abrirSala();
}

// Friends for the sheet, the lobby invite and the sidebar invite rows.
static void carregarAmigos(void) {
  int i, n = socialvis_n_amigos();
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
  // Invalid IDs already produced a specific error; keep the current UI.
  if (!juntos_ativo()) return;
  // juntos_preparar queues this exact title/episode for paused preparation.
  juntosui_sala_abriu();
  camada = CAMADA_SALA;
}

int juntosui_pediu_reproduzir(void) { int p = pediuReproduzir; pediuReproduzir = 0; return p; }
int juntosui_pediu_escolher_filme(void) { int p = pediuEscolherFilme; pediuEscolherFilme = 0; return p; }
int juntosui_pediu_fechar_player(void) { int p = pediuFecharPlayer; pediuFecharPlayer = 0; return p; }
int juntosui_pediu_fontes(void) { int p = pediuFontes; pediuFontes = 0; return p; }
int juntosui_aberta(void) { return camada != CAMADA_NADA || okSegurando; }
int juntosui_cobre_tela(void) {
  return camadaVisual == CAMADA_SALA && !escolhendoFilme &&
         !cinema && anim_suave(entrada) >= .9999f;
}

// --- botoes do cartao da sala ------------------------------------------------------------------
enum { BT_COMECAR, BT_CELULAR, BT_FILME, BT_FONTES, BT_TRAVA, BT_SAIR, BT_CODIGO, BT_N };
static int botoesSala(int *b) {
  const JsMotor *m = juntos_motor();
  int n = 0;
  if (m && m->sala.rel.estado == JSE_LOBBY && juntos_sou_anfitriao()) b[n++] = BT_COMECAR;
  if (m) b[n++] = BT_CELULAR;
  if (m && juntos_pode_controlar()) b[n++] = BT_FILME;
  if (m) b[n++] = BT_FONTES;
  if (juntos_sou_anfitriao()) b[n++] = BT_TRAVA;
  b[n++] = BT_SAIR;
  b[n++] = BT_CODIGO;
  return n;
}
static const char *rotuloBotao(int b) {
  const JsMotor *m = juntos_motor();
  switch (b) {
    case BT_COMECAR: return m && todosProntos(m) ? i18n("Começar juntos") : i18n("Começar agora");
    case BT_CELULAR: return i18n("Convidar");
    case BT_CODIGO: return i18n("Entrar com código");
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
    case BT_CODIGO: return "aj_users";
    case BT_FILME: return "aj_film";
    case BT_FONTES: return "aj_circle-check";
    case BT_TRAVA: return "aj_users";
    default: return "aj_log-out";
  }
}

static const char *NOME_BT[BT_N] = { "start", "invite", "choose-film", "sources", "control", "leave", "join-code" };

// The invite panel: the room may still be preparing (no server room, no socket),
// so the code/QR arrive later; until then the panel shows progress, never a blank box.
static void abrirConvite(void) {
  const JsMotor *m = juntos_motor();
  mostrarQr = 1; focoConv = 0;
  memset(convFoco, 0, sizeof convFoco);
  qrPendente = !m || !m->conectado;
  if (!qrPendente && !juntos_url_celular()[0]) juntos_pedir_celular();
  printf("[juntos] lobby: invite panel open (%s, %d friend(s))\n", qrPendente ? "room preparing" : "room ready", nAmigos);
}

static void convidarAmigo(int i) {
  const char *id = amigoId[i];
  if (i < 0 || i >= nAmigos || marcado[i]) return;
  marcado[i] = juntos_convidar(&id, 1);
  printf("[juntos] lobby: invite %s %s\n", amigoNome[i], marcado[i] ? "sent" : "not accepted");
}

static void apertarSala(int b) {
  const JsMotor *m = juntos_motor();
  printf("[juntos] lobby: OK on %s\n", NOME_BT[b]);
  switch (b) {
    case BT_COMECAR: juntos_comecar(); break;
    case BT_CELULAR: abrirConvite(); break;
    case BT_FILME:
      if (!m || !m->temSala) break;
      focoSala = foco; mostrarQr = 0; escolhendoFilme = 1; camada = CAMADA_NADA; pediuEscolherFilme = 1; break;
    case BT_CODIGO: focoSala = foco; mostrarQr = 0; camada = CAMADA_CODIGO; codPos = 0; break;
    // The shared 2.0.8 Sources sheet (app.c); the lobby keeps focus on this button.
    case BT_FONTES: focoSala = foco; mostrarQr = 0; pediuFontes = 1; break;
    case BT_TRAVA: if (m) juntos_controle_anfitriao(!m->sala.controleAnf); break;
    case BT_SAIR:
      pediuFecharPlayer = juntos_precarregando();
      if (juntos_sou_anfitriao() && m && m->sala.n > 1) { juntos_sair(); }
      else if (juntos_sou_anfitriao()) juntos_encerrar(), juntos_sair();
      else juntos_sair();
      camada = CAMADA_NADA;
      break;
  }
}

// --- the session sidebar (CH+ during the film) -------------------------------------------------
// Rows with focus: the friends to invite (the lobby invite), "Only I control"
// (host), "Leave session" (the existing leave question).
static int painelLinhas(void) { return nAmigos + juntos_sou_anfitriao() + 1; }
static void abrirPainel(void) {
  if (!nAmigos) carregarAmigos();
  camada = CAMADA_PAINEL; foco = 0;
  printf("[juntos] sidebar: open (%d friend(s))\n", nAmigos);
}
static void apertarPainel(int i) {
  const JsMotor *m = juntos_motor();
  if (i < nAmigos) convidarAmigo(i);
  else if (juntos_sou_anfitriao() && i == nAmigos) { if (m) juntos_controle_anfitriao(!m->sala.controleAnf); }
  else juntosui_perguntar_sair();
}
static int ehCanal(const SDL_Event *e, int mais) {
  SDL_Keycode k = e->key.keysym.sym;
  int sc = e->key.keysym.scancode;
  // Off LG, CH+ arrives as BLUE/"s" (main.c, remapCanal), like for Saved.
  return mais ? k == SDLK_PAGEUP || sc == NV_SCANCODE_CH_UP || k == SDLK_s || sc == NV_SCANCODE_BLUE
              : k == SDLK_PAGEDOWN || sc == NV_SCANCODE_CH_DOWN;
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
  if (stream_folha_aberta()) return 0;
  k = e->key.keysym.sym;
  // SEGURAR OK no player com o OSD escondido: a regua de reacoes. O toque curto
  // continua sendo o play/pausa: ele e entregue ao player quando a tecla SOBE.
  // Escolhendo filme o player fica aberto mas escondido: o OK e da Busca/Detalhe.
  if (camada == CAMADA_NADA && juntos_ativo() && noPlayer() && !escolhendoFilme && !player_controles_visiveis() && ehOk(k)) {
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
  // CH+ in a Watch Together film: the session sidebar, not Saved.
  if (camada == CAMADA_NADA && e->type == SDL_KEYDOWN && !e->key.repeat && juntos_ativo() && noPlayer() &&
      !escolhendoFilme && !lobbyAberto && ehCanal(e, 1)) { abrirPainel(); return 1; }
  if (camada == CAMADA_NADA) return 0;
  if(camada==CAMADA_SALA&&!mostrarQr&&!cinema&&e->type==SDL_KEYDOWN&&(k==SDLK_UP||k==SDLK_DOWN||k==SDLK_LEFT||k==SDLK_RIGHT)){
    int buttons[BT_N], codigo=botoesSala(buttons)-1;
    if(k==SDLK_DOWN&&foco==codigo&&juntos_motor()&&juntos_motor()->sala.n<5){foco=-1;return 1;}
    if(k==SDLK_DOWN&&(foco<0||foco==codigo)){foco=focoAcao<codigo?focoAcao:0;return 1;}
    int current=-1;for(int i=0;i<navigationCount;i++)if(navigationIds[i]==foco)current=i;
    int next=focus_rect_nearest(navigationRects,navigationCount,current,(k==SDLK_RIGHT)-(k==SDLK_LEFT),(k==SDLK_DOWN)-(k==SDLK_UP));
    if(next>=0){foco=navigationIds[next];if(foco>=0&&foco<codigo)focoAcao=foco;return 1;}
  }
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
      if (ehVoltar(k)) { if (lobbyAberto) juntosui_sala_abriu(); else camada = CAMADA_FOLHA; }
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
      int temVazio = mo && mo->sala.n < 5, codigo = n - 1;
      if (cinema) {   // the film started: Enter session (OK/Back) or Pause/Continue
        int nb = juntos_pode_controlar() ? 2 : 1;
        if (foco < 0 || foco >= nb) foco = 0;
        if (k == SDLK_LEFT && foco > 0) foco--;
        else if (k == SDLK_RIGHT && foco < nb - 1) foco++;
        else if (ehOk(k) && foco == 1) juntos_ao_alternar(mo && mo->sala.rel.estado == JSE_TOCANDO);
        else if (ehOk(k) || ehVoltar(k)) { lobbyAberto = 0; camada = CAMADA_NADA; printf("[juntos] cinema: entered the session\n"); }
        return 1;
      }
      if (foco >= n) foco = n - 1;
      if (mostrarQr) {   // invite panel: friends list; Back returns to the control that opened it
        if (ehVoltar(k)) { mostrarQr = qrPendente = 0; printf("[juntos] lobby: invite panel closed\n"); }
        else if (k == SDLK_UP && focoConv > 0) focoConv--;
        else if (k == SDLK_DOWN && focoConv < nAmigos - 1) focoConv++;
        else if (ehOk(k)) convidarAmigo(focoConv);
        return 1;
      }
      // Three rows: "Join with a code" (top right), the open seat, the actions.
      if (ehVoltar(k)) juntosui_perguntar_sair();
      else if (k == SDLK_UP) { if (foco >= 0 && foco < codigo && temVazio) foco = -1; else if (foco != codigo) foco = codigo; }
      else if (k == SDLK_DOWN) { if (foco == codigo && temVazio) foco = -1; else if (foco == codigo || foco < 0) foco = focoAcao < codigo ? focoAcao : 0; }
      else if (foco < 0) { if (ehOk(k)) { printf("[juntos] lobby: OK on open seat\n"); abrirConvite(); } }
      else if (foco == codigo) { if (ehOk(k)) apertarSala(b[foco]); }
      else if (k == SDLK_LEFT && foco > 0) foco--;
      else if (k == SDLK_RIGHT && foco < codigo - 1) foco++;
      else if (ehOk(k)) apertarSala(b[foco]);
      if (foco >= 0 && foco < codigo) focoAcao = foco;
      return 1;
    }
    case CAMADA_PAINEL: {
      int n = painelLinhas();
      if (foco >= n) foco = n - 1;
      if (ehVoltar(k) || ehCanal(e, 1) || ehCanal(e, 0)) camada = CAMADA_NADA;
      else if (k == SDLK_UP && foco > 0) foco--;
      else if (k == SDLK_DOWN && foco < n - 1) foco++;
      else if (ehOk(k)) apertarPainel(foco);
      return 1;
    }
    case CAMADA_FONTES: {
      const JsMotor *m = juntos_motor();
      int n = m ? (m->sala.n < 5 ? m->sala.n : 5) : 0;
      if (foco > n) foco = n;
      if (ehVoltar(k) || (ehOk(k) && foco == 0)) { camada = CAMADA_SALA; foco = focoSala; }
      else if (k == SDLK_DOWN && foco < n) foco++;
      else if (k == SDLK_UP && foco > 0) foco--;
      else if (ehOk(k) && m && foco > 0 && !strcmp(m->sala.m[foco - 1].id, m->eu)
               && stream_lista_do_alvo(m->sala.ep) && stream_item(stream_atual())) {
        pediuFontes = 1; camada = CAMADA_NADA;
      }
      return 1;
    }
    case CAMADA_SAIR:
      if (ehVoltar(k)) { camada = lobbyAberto ? CAMADA_SALA : CAMADA_NADA; foco = focoSala; }
      else if (k == SDLK_LEFT) foco = 0;
      else if (k == SDLK_RIGHT) foco = 1;
      else if (ehOk(k)) {
        camada = lobbyAberto ? CAMADA_SALA : CAMADA_NADA;
        if (foco == 0) {
          if (juntos_sou_anfitriao()) { const JsMotor *m = juntos_motor(); if (m && m->sala.n <= 1) juntos_encerrar(); }
          juntos_sair();
          pediuFecharPlayer = 1;
        } else foco = focoSala;
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
  if (!juntos_ativo() && (camada == CAMADA_SALA || camada == CAMADA_REACOES || camada == CAMADA_SAIR || camada == CAMADA_PEDIDO || camada == CAMADA_FONTES || camada == CAMADA_PAINEL))
    camada = CAMADA_NADA;
  if (!noPlayer() && (camada == CAMADA_REACOES || camada == CAMADA_PAINEL)) camada = CAMADA_NADA;
  // CH+ held opens the Control Center on the island: the sidebar yields, or it
  // would stay on screen while the hidden island's controls take the keys.
  if (camada == CAMADA_PAINEL && central_aberta()) camada = CAMADA_NADA;
  const JsMotor *sala = juntos_motor();
  if (qrPendente && sala && sala->conectado) { qrPendente = 0; juntos_pedir_celular(); }
  // Cinema start (owner 26/10): at t0 the lobby does not vanish. Its art fades
  // and the film comes forward under it; the person pauses or enters the session.
  if (lobbyAberto && !cinema && !stream_folha_aberta() && sala && sala->temSala && sala->sala.rel.estado == JSE_TOCANDO &&
      jsr_servidor(&sala->rel, (double)SDL_GetTicks64()) >= sala->sala.rel.t0 &&
      sala->prontoAnterior && !sala->playArmado) {
    cinema = 1; cinemaAnim = 0; foco = 0; mostrarQr = qrPendente = 0;
    if (camada != CAMADA_SAIR) camada = CAMADA_SALA;
    printf("[juntos] cinema: film started under the lobby\n");
  }
  if (!sala || !sala->temSala || sala->sala.rel.estado == JSE_LOBBY) cinema = 0;
  cinemaAnim = ajustes_animacoes_reduzidas_usuario() ? cinema : anim_mola(cinemaAnim, cinema, dt, NV_MOLA_TELA * .5f);
  if (camada != CAMADA_NADA && camada != camadaVisual) {
    camadaVisual = camada;
    entrada = tempoEntrada = 0;
    memset(focoAnim[camadaVisual], 0, sizeof focoAnim[0]);
    memset(lugarAnim, 0, sizeof lugarAnim);
    folhaScroll = folhaVel = 0;
  }
  entrada = t21_entrada(entrada, dt, camada != CAMADA_NADA);
  float alvoScroll = camada == CAMADA_FONTES ? fmaxf(0, foco - 2) * 112 : camada == CAMADA_PAINEL ? painelAlvo :
    fmaxf(0, 48 + 40 + 72 + 48 + (foco + 1) * 76.0f + 24 - corpoAltura(948));
  folhaScroll = anim_mola2(&folhaVel, folhaScroll, alvoScroll, dt, NV_MOLA2_SCROLL);
  if (camada != CAMADA_NADA) tempoEntrada += dt;
  else if (entrada <= 0) { camadaVisual = CAMADA_NADA; if (!lobbyAberto) cinema = 0; }
  for (int c = 1; c < 9; c++) for (int i = 0; i < 16; i++) {
    int alvo = camada == c && (camada == CAMADA_CODIGO ? codPos + 1 :
               camada == CAMADA_REACOES ? foco + 1 + linhaReacao * 6 : foco + 1) == i;
    focoAnim[c][i] = anim_mola(focoAnim[c][i], alvo, dt, NV_MOLA_FOCO);
  }
  for (int i = 0; i < 5; i++) {
    const JsMotor *m = juntos_motor();
    const JsMembro *p = m && i < m->sala.n ? &m->sala.m[i] : NULL;
    const char *id = p ? p->id : "";
    if (strcmp(lugarId[i], id)) {
      snprintf(lugarId[i], sizeof lugarId[i], "%s", id);
      lugarAnim[i] = prontoAnim[i] = 0;
    }
    int presente = m && i <= m->sala.n;
    lugarAnim[i] = anim_rampa(lugarAnim[i], presente, dt, LUGAR_MS);   // own clock: arrival then light
    int pronto = p && p->on && (!strcmp(p->st, "pronto") || !strcmp(p->st, "tocando"));
    prontoAnim[i] = anim_mola(prontoAnim[i], pronto, dt, NV_MOLA_TELA);
    int local = m && i < m->sala.n && !strcmp(m->sala.m[i].id, m->eu) && fonteLocal(m);
    fonteExp[i] = anim_mola(fonteExp[i], camada == CAMADA_FONTES && foco == i + 1 && local, dt, NV_MOLA_TELA);
    anelAnim[i] = anim_mola(anelAnim[i], p && !pronto ? p->pct / 100.0f : pronto, dt, NV_MOLA_TELA);
  }
  prontinhaAnim = anim_mola(prontinhaAnim, 1, dt, NV_MOLA_TELA);
  stream_folha_sala_h = salaFolhaH; stream_folha_sala_desenhar = salaFolhaDesenhar;
  convAnim = anim_mola(convAnim, camada == CAMADA_SALA && mostrarQr, dt, NV_MOLA_TELA);
  for (int i = 0; i < FOLHA_AMIGOS; i++) convFoco[i] = anim_mola(convFoco[i], mostrarQr && focoConv == i, dt, NV_MOLA_FOCO);
  if (ultimaContagem != contagemRestante()) { ultimaContagem = contagemRestante(); contagemAnim = 0; }
  contagemAnim = anim_mola(contagemAnim, 1, dt, NV_MOLA_FOCO);
  pedirCorpo();
  // The session on the clock island while the film's controls are up.
  ilhaTexto[0] = 0;
  if (camadaVisual == CAMADA_PAINEL && !central_aberta()) plrilha_esconder();
  else if (noPlayer() && juntos_ativo() && !lobbyAberto && camada == CAMADA_NADA && sala && sala->temSala && osdVisivel()) {
    PlrIlhaPedido p = {0};
    int n = sala->sala.n;
    snprintf(ilhaTexto, sizeof ilhaTexto, "%s%s%s · %d %s", i18n("Assistir Juntos"), juntos_codigo()[0] ? " · " : "",
             juntos_codigo(), n, i18n(n == 1 ? "pessoa" : "pessoas"));
    p.icone = "aj_users"; p.texto = ilhaTexto; p.baixa = 1; p.video = 1;
    plrilha_pedir(&p);
  }
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
  if (!p->on) return i18n("ausente");
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
  GfxRecorteAnt fora = gfx_recorte_dentro(r.x, y, r.w, r.y + r.h - 32 - y);
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
  gfx_recorte_voltar(fora);
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

// --- lobby: full-bleed screening room ------------------------------------------------------------
// The film's backdrop fills the screen under the app scrim (Detail hero), the
// title block and seats sit on the left gutter, actions anchor at the bottom.
// Solid colour + art only, so it holds with the glass off (factory default).
static const CatItem *itemSala(void) {
  static char poster[NV_ARTE_URL_MAX];
  static int idx = -1;
  const JsMotor *m = juntos_motor();
  int i, n;
  if (!m || !m->sala.poster[0]) return NULL;
  if (strcmp(poster, m->sala.poster)) {
    snprintf(poster, sizeof poster, "%s", m->sala.poster);
    idx = -1;
    for (i = 0, n = cat_n(); i < n; i++) {
      const CatItem *ci = cat_item(i);
      if (ci && !strcmp(ci->poster, poster)) { idx = i; break; }
    }
  }
  return idx >= 0 ? cat_item(idx) : NULL;
}

static const char *arteSala(void) {
  const CatItem *ci = itemSala();
#ifdef NV_SHOT_HOOKS
  if (shotArte[0]) return shotArte;
#endif
  return ci && ci->backdrop[0] ? ci->backdrop : NULL;
}

// Backdrop in cover with a slow push-in, then the Detail scrims: left for the
// text, base for seats and actions.
static void veusCinema(GfxRect r, float a) {
  gfx_veu_base(r, 0, 0.78f, 0.95f * a);
  gfx_veu_css(r, 2, 1.0f, 1.0f, 0.75f * a);
  gfx_cor(r, 0, 0.03f, 0.03f, 0.05f, 0.22f * a);
}

static void telaCinema(GfxRect r, float a) {
  if (a <= .001f) return;
  const char *arte = arteSala();
  GLuint t = arte ? tex_obter_hero(arte) : 0;
  float z = ajustes_animacoes_reduzidas_usuario() ? 0 : .04f * (1 - expf(-tempoEntrada / 9));
  GfxRect zr = { r.x - r.w * z / 2, r.y - r.h * z / 2, r.w * (1 + z), r.h * (1 + z) };
  // Cover art is opaque: retain its underpaint only during fades.
  int underpaint = a < 1 || gfx_opacidade_grupo < 1;
#ifdef NV_SHOT_HOOKS
  underpaint |= getenv("NUVIO_JUNTOS_UNCACHED") != NULL;
#endif
  if (t && underpaint) gfx_cor(r, 0, 0.06f, 0.055f, 0.08f, a);
  if (t) {
    gfx_tex_aspect_atual = tex_aspecto(arte);
    gfx_card_forcar_cover_atual = 1.0f;
    gfx_rect(zr, t, GFX_CARD, 1, 0, 0, 0.0f, 1, 1, 1, a);
    gfx_card_forcar_cover_atual = 0.0f;
    gfx_tex_aspect_atual = 0.0f;
  } else {
    gfx_cor(r, 0, 0.06f, 0.055f, 0.08f, a);
    gfx_cor(r, 0, 0.14f, 0.11f, 0.18f, a);
  }
  // Only the veils are stationary; the approved art push-in keeps running.
  // During fades retain the separate alphas, which do not commute with baking.
  int cache = a == 1 && gfx_opacidade_grupo == 1 && !gfx_mini_ativo();
#ifdef NV_SHOT_HOOKS
  if (getenv("NUVIO_JUNTOS_UNCACHED")) cache = 0;
#endif
  float px = gfx_px_por_unidade();
  int w = (int)(r.w * px + .5f), h = (int)(r.h * px + .5f);
  if (cache && w > 0 && h > 0) {
    int refazer = !cinemaVeu.tex || cinemaVeu.w != w || cinemaVeu.h != h ||
                  memcmp(&cinemaVeuRect, &r, sizeof r) || cinemaVeuHdr != (gfx_hdr_ui != 1);
    if (gfx_mini_alvo(&cinemaVeu, w, h)) {
      if (refazer) {
        gfx_mini_comecar(&cinemaVeu, 0, 0, px);
        veusCinema((GfxRect){0, 0, r.w, r.h}, 1);
        gfx_mini_terminar();
        cinemaVeuRect = r; cinemaVeuHdr = gfx_hdr_ui != 1;
      }
      gfx_mini_desenhar(&cinemaVeu, r, 0, 1);
      return;
    }
  }
  veusCinema(r, a);
}

static float entradaItem(int i) {
  return revela_entra(1, revela_onda_atraso(i, 0), 1 + (Uint32)(tempoEntrada * 1000));
}

static float textoCentro(TxtEstilo es, const char *s, float cx, float y, int c, float a, float maxW) {
  TxtLinha l = txt_linha(es, s, c, c, c, 255);
  if (l.w > maxW)
    return txt_bloco_corta(es, s, c, c, c, cx - maxW / 2, y, maxW, l.h + 2, a, 3);
  if (a > 0) txt_desenhar_alpha(l, cx - l.w / 2.0f, y, a);
  return l.h;
}

// One centred line, cut to the seat width (never wraps out of the card).
static float linhaCentro(TxtEstilo es, const char *s, float cx, float y, int c, float a, float maxW) {
  TxtLinha l = txt_linha_corta(es, s, c, c, c, 255, maxW);
  txt_desenhar_alpha(l, cx - l.w / 2.0f, y, a);
  return l.h;
}

// The seat's face: the profile avatar for this TV, the friend's avatar for others.
static void rostoMembro(const JsMembro *p, const char **url, const char **nome) {
  const JsMotor *m = juntos_motor();
  *url = p->av; *nome = p->nome;
  if (m && !strcmp(p->id, m->eu)) {
    const ContaPerfil *c = perfis_item_ativo();
    if (c && !p->av[0]) *url = c->avatarUrl;
    if (c && !p->nome[0]) *nome = c->nome;
  } else if (!p->av[0])
    for (int i = 0; i < nAmigos; i++) if (!strcmp(amigoId[i], p->id)) *url = amigoAv[i];
}

// Lobby frame: the whole virtual screen minus the Detail gutter.
#define SALA_MARGEM 96.0f
static float lugarW = 248;   // shrinks only when five seats would not fit
#define LUGAR_W lugarW
#define LUGAR_H 262.0f
#define LUGAR_GAP 24.0f
#define LUGAR_AV 88.0f

// Seat state as in the mockup: icon + word; green once that TV is ready,
// amber when it needs attention.
static const char *estadoIcone(const JsMembro *p, int pronto, float *r, float *g, float *b) {
  *r = .84f; *g = .82f; *b = .85f;
  if (pronto) { *r = 0.55f; *g = 0.92f; *b = 0.65f; return "aj_check"; }
  if (!p->on) return "aj_wifi-off";
  if (!strcmp(p->st, "semfonte") || !strcmp(p->st, "erro")) { *r = 0.96f; *g = 0.78f; *b = 0.42f; return "aj_triangle-alert"; }
  if (!strcmp(p->st, "pausado")) return "aj_clock";
  return "aj_loader";
}
static int membroPronto(const JsMembro *p) {
  return p->on && (!strcmp(p->st, "pronto") || !strcmp(p->st, "tocando"));
}
static void estadoLinha(const JsMembro *p, float x, float y, int centro, float a, float maxW) {
  float r, g, b;
  int pronto = membroPronto(p);
  const char *ic = estadoIcone(p, pronto, &r, &g, &b);
  char st[120];
  if (!strcmp(p->st, "carregando") && p->pct > 0) snprintf(st, sizeof st, "%s %d%%", estadoTexto(p), p->pct);
  else snprintf(st, sizeof st, "%s", pronto ? i18n("pronto") : estadoTexto(p));
  if (st[0] >= 'a' && st[0] <= 'z') st[0] -= 32;
  TxtLinha l = txt_linha_corta(TXT_CAPTION, st, r * 255, g * 255, b * 255, 255, maxW - 34);
  if (centro == 1) x -= (l.w + 34) / 2.0f;
  else if (centro == 2) x -= l.w + 34;
  gfx_icone((GfxRect){ x, y + (l.h - 24) / 2.0f, 24, 24 }, ic, r, g, b, a);
  txt_desenhar_alpha(l, x + 34, y, a);
}

static void desenharLugar(int i, const JsMembro *p, int vazio, float f, float x, float y, float a) {
  float e = ajustes_animacoes_reduzidas_usuario() ? 1 : entradaItem(i);
  // Mockup: 600 ms arrival from above, then the light comes up over 700 ms, 160 ms in.
  float ms = lugarAnim[i] * LUGAR_MS, chega = anim_saida(ms / 600), luz = anim_suave((ms - 160) / 700);
  float s = 1 + .05f * f, avW = LUGAR_AV * (.9f + .1f * luz);
  GfxRect c = { x, y, LUGAR_W, LUGAR_H };
  GfxRect av = { x + (LUGAR_W - avW) / 2, y + 26 + (LUGAR_AV - avW) / 2, avW, avW };
  float cx = x + LUGAR_W / 2, maxW = LUGAR_W - 24, ty = y + 134;
  char nome[160];
  const JsMotor *m = juntos_motor();
  int pronto = !vazio && membroPronto(p);
  if (vazio) navigationTarget(-1,c);
  a *= e * chega;
  if (a <= 0.01f) return;
  gfx_transformar(cx, y + LUGAR_H / 2, s, origemX, origemY - NV_FOCO_LIFT * f + (1 - e) * NV_ENTRA_DY - (1 - chega) * 20);
  // The seat cushion under the backrest, a little wider than it.
  gfx_cor((GfxRect){ x - 9, y + LUGAR_H - 23, LUGAR_W + 18, 36 }, .45f, 1, 1, 1, .07f * a);
  t21_ilha_solida(c, T21_R_SHEET, a);
  plrui_linha_foco(c, T21_R_SHEET, f * a);
  if (vazio) {
    float ink = f >= .5f ? ajustes_tinta_foco() / 255.0f : 1;
    plrui_material_plano(av, av.h / 2, a);
    plrui_pilula_foco(av, f * a);
    gfx_icone((GfxRect){ av.x + 28, av.y + 28, 32, 32 }, "aj_user-plus", ink, ink, ink, a);
    linhaCentro(TXT_ILHA_NOME, i18n("Mais um lugar"), cx, ty, 245, a, maxW);
    linhaCentro(TXT_CAPTION, i18n("Convide um amigo"), cx, ty + 36, 190, a, maxW);
    gfx_transformar(960, 540, 1, origemX, origemY);
    return;
  }
  const char *url, *quem;
  rostoMembro(p, &url, &quem);
  rec_avatar(av, url, quem, p->id, (p->on ? 1 : 0.4f) * (.3f + .7f * luz) * a);
  GfxRect anel = { av.x - 8, av.y - 8, av.w + 16, av.h + 16 };
  if (pronto) {
    // Ready: green ring, a still soft glow and a check badge (no pulse).
    gfx_anel_fora(anel, .5f, 0, 12, 0.55f, 0.92f, 0.65f, .16f * prontoAnim[i] * luz * a);
    gfx_anel(anel, .5f, 3, 0.55f, 0.92f, 0.65f, prontoAnim[i] * a);
    GfxRect selo = { av.x + av.w - 22, av.y + av.h - 22, 30, 30 };
    gfx_cor(selo, .5f, 0.30f, 0.72f, 0.45f, prontoAnim[i] * a);
    gfx_icone((GfxRect){ selo.x + 6, selo.y + 6, 18, 18 }, "aj_check", 1, 1, 1, prontoAnim[i] * a);
  } else if (p->on) {
    // Loading: progress ring from the buffer, a turning arc until it reports.
    gfx_anel(anel, .5f, 3, 1, 1, 1, 0.12f * a);
    if (p->pct > 0) gfx_arco(anel, 3, 0, fmaxf(.02f, anelAnim[i]), 0.62f, 0.78f, 1, a);
    else gfx_arco(anel, 3, fmodf(agoraUlt / 1100.0f, 1), .22f, 0.62f, 0.78f, 1, a);
  }
  if (m && !strcmp(p->id, m->eu)) snprintf(nome, sizeof nome, "%s%s%s", i18n("Você"), p->anf ? " · " : "", p->anf ? i18n("anfitrião") : "");
  else snprintf(nome, sizeof nome, "%s", quem);
  linhaCentro(TXT_ILHA_NOME, nome, cx, ty, 245, a, maxW);
  estadoLinha(p, cx, ty + 36, 1, a, maxW);
  // Sync stats only once measured; source verdict only once known.
  char linha[128];
  float ly = ty + 70;
  if (p->on && p->stats) {
    snprintf(linha, sizeof linha, "RTT %.0f ms · Δ %+.0f ms", p->rtt, p->offset);
    ly += linhaCentro(TXT_ILHA_META, linha, cx, ly, 175, a, maxW) + 2;
  }
  if (p->on && p->fonte[0]) {
    snprintf(linha, sizeof linha, p->altura ? "%s · %dp" : "%s", fonteTexto(p->fonte), p->altura);
    if (largura(TXT_ILHA_META, linha) > maxW) snprintf(linha, sizeof linha, "%s", fonteTexto(p->fonte));
    linhaCentro(TXT_ILHA_META, linha, cx, ly, 175, a, maxW);
  }
  gfx_transformar(960, 540, 1, origemX, origemY);
}

static int todosProntos(const JsMotor *m) {
  int i;
  for (i = 0; i < m->sala.n; i++) if (!membroPronto(&m->sala.m[i])) return 0;
  return m->sala.n > 0;
}

// The first TV still getting ready, said the way the mockup does.
static const char *fraseEspera(const JsMembro *p) {
  if (!p->on) return i18n("%s está ausente.");
  if (!strcmp(p->st, "procurando")) return i18n("%s está procurando uma fonte.");
  if (!strcmp(p->st, "carregando")) return i18n("%s está carregando o filme.");
  if (!strcmp(p->st, "entrando")) return i18n("%s está entrando.");
  if (!strcmp(p->st, "semfonte")) return i18n("%s está sem fonte.");
  if (!strcmp(p->st, "erro")) return i18n("%s teve um erro.");
  return i18n("%s está se preparando.");
}

static void cabecalhoSala(float esquerda, float direita, float y, float a) {
  const char *c = i18n("Velvet Cinema · Sala privada");
  float w = largura(TXT_ILHA_NOME, c);
  gfx_icone((GfxRect){ esquerda, y, 36, 36 }, "aj_users", 0.92f, 0.92f, 0.9f, a);
  txt_tracking(TXT_V2_KICK, i18n("Assistir Juntos"), 245, 245, 245, esquerda + 56, y + 6, a, 2.5f);
  // Live dot: breathes while the room is open.
  float viva = ajustes_animacoes_reduzidas_usuario() ? 1 : .55f + .45f * sinf(agoraUlt / 420.0f);
  gfx_cor((GfxRect){ direita - w - 24, y + 12, 10, 10 }, 0.5f, 0.45f, 0.9f, 0.6f, viva * a);
  texto(TXT_ILHA_NOME, c, direita - w, y, 225, a, 0);
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
  texto(TXT_ILHA_CORPO, i18n("Cada um assiste pela própria conta. Se a sua fonte for outra versão, o app avisa."), 500, 218, 180, a, corpoBase.w - 80);
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

  GfxRecorteAnt fora = gfx_recorte_dentro(484, 288, corpoBase.w - 48, corpoBase.h - 310);
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
    txt_bloco_corta(TXT_ILHA_META, impacto, 190, 190, 190, statusX + 38, y + 37, statusW, 26, a, 2);
    if (fonteExp[i] > 0.01f) botao_pilula((GfxRect){ 580, y + 82, botao_largura(i18n("Trocar fonte"), "aj_monitor-cog", 0), BOTAO_H_SECUNDARIO }, i18n("Trocar fonte"), "aj_monitor-cog", focoDe(i + 1) * alvo, 0, 0, a * fonteExp[i]);
    y += h + 2;
  }
  gfx_recorte_voltar(fora);
  botao_pilula((GfxRect){ 500, corpoBase.y + corpoBase.h - 104, botao_largura(i18n("Voltar à sala"), "aj_users", 1), BOTAO_H_PRIMARIO },
               i18n("Voltar à sala"), "aj_users", focoDe(0), 1, 0, a);
}

// Sources sheet opened in a room: one row per TV with its ready state and
// source verdict, so picking a source here still shows who is ready.
#define SALA_FOLHA_LINHA 60.0f
static int salaFolhaN(void) {
  const JsMotor *m = juntos_motor();
  if (!juntos_ativo() || !m) return 0;
  return m->sala.n < 5 ? m->sala.n : 5;
}
static float salaFolhaH(void) {
  int n = salaFolhaN();
  return n ? n * SALA_FOLHA_LINHA + 28 : 0;
}
static void salaFolhaDesenhar(float x, float y, float w, float a) {
  const JsMotor *m = juntos_motor();
  int n = salaFolhaN();
  float h = n * SALA_FOLHA_LINHA + 12;
  char linha[160];
  gfx_cor((GfxRect){ x, y, w, h }, 22 / h, 1, 1, 1, .05f * a);
  for (int i = 0; i < n; i++) {
    const JsMembro *p = &m->sala.m[i];
    const char *url, *quem;
    float ry = y + 6 + i * SALA_FOLHA_LINHA, colW = w * .5f - 40;
    rostoMembro(p, &url, &quem);
    rec_avatar((GfxRect){ x + 16, ry + 8, 44, 44 }, url, quem, p->id, p->on ? a : .45f * a);
    if (!strcmp(p->id, m->eu)) quem = i18n("Você");
    texto(TXT_ILHA_NOME, quem, x + 76, ry + 2, 245, a, colW);
    if (p->on && p->fonte[0]) snprintf(linha, sizeof linha, p->altura ? "%s · %dp" : "%s", fonteTexto(p->fonte), p->altura);
    else snprintf(linha, sizeof linha, "%s", plataformaFonte(p));
    texto(TXT_ILHA_META, linha, x + 76, ry + 32, 175, a, colW);
    estadoLinha(p, x + w - 20, ry + 14, 2, a, w * .5f - 20);
  }
}

// Invite panel, sliding in from the right over the dimmed lobby: room code,
// QR for the phone (or progress while the room opens) and the friends list.
static void desenharConvite(float R, float topo, float fim, float a) {
  const JsMotor *m = juntos_motor();
  float w = 600, x = R - w + (1 - anim_suave(convAnim)) * 80, y = topo, h = fim - topo;
  float rx = x + 292, rw = w - 292 - 40, lado = 200;
  char c[64];
  a *= anim_suave(convAnim);
  if (a <= 0.01f) return;
  gfx_cor(telaReal(), 0, 0.02f, 0.02f, 0.03f, 0.45f * a);
  t21_ilha_solida((GfxRect){ x, y, w, h }, T21_R_SHEET, a);
  texto(TXT_CAPTION, i18n("Convidar"), x + 40, y + 32, 200, a, w - 80);
  GfxRect q = { x + 54, y + 94, lado, lado };
  GLuint t = qrTextura(juntos_url_celular());
  if (t) {
    gfx_cor((GfxRect){ q.x - 14, q.y - 14, q.w + 28, q.h + 28 }, 0.06f, 1, 1, 1, a);
    gfx_rect(q, t, GFX_SNAP, 0, 0, 0, 0, 1, 1, 1, a);
  } else {
    // Room still opening (source preparing) or code on its way: say so.
    GfxRect anel = { q.x + lado / 2 - 36, q.y + lado / 2 - 36, 72, 72 };
    gfx_cor((GfxRect){ q.x - 14, q.y - 14, q.w + 28, q.h + 28 }, 0.06f, 1, 1, 1, 0.06f * a);
    gfx_anel(anel, .5f, 4, 1, 1, 1, 0.12f * a);
    gfx_arco(anel, 4, fmodf(agoraUlt / 1100.0f, 1), .25f, 0.62f, 0.78f, 1, a);
  }
  texto(TXT_CAPTION, i18n("Código da sala"), rx, y + 96, 190, a, rw);
  if (juntos_codigo()[0]) {
    snprintf(c, sizeof c, "%.3s-%s", juntos_codigo(), juntos_codigo() + 3);
    texto(TXT_ILHA_TITULO, c, rx, y + 128, 250, a, rw);
  } else texto(TXT_ILHA_NOME, juntos_precisa_auth() ? i18n("Entre na sua conta para enviar") : i18n("Preparando o código…"), rx, y + 134, 215, a, rw);
  txt_bloco_corta(TXT_ILHA_META, i18n("Escaneie com o celular para mandar mensagens e controlar a sessão."),
                  185, 185, 185, rx, y + 196, rw, 26, a, 4);
  float ly = y + 340;
  texto(TXT_ILHA_CORPO, nAmigos ? i18n("Quem vai assistir com você") : i18n("Nenhum amigo ainda. Quem tiver o código entra pedindo."),
        x + 40, ly, 190, a, w - 80);
  ly += 46;
  GfxRecorteAnt fora = gfx_recorte_dentro(x, ly - 8, w, y + h - 16 - (ly - 8));
  ly -= fmaxf(0, focoConv - 2) * 72;
  for (int i = 0; i < nAmigos; i++, ly += 72) {
    GfxRect l = { x + 24, ly, w - 48, 64 };
    int dentro = 0;
    for (int j = 0; m && j < m->sala.n; j++) dentro |= !strcmp(m->sala.m[j].id, amigoId[i]);
    plrui_linha_foco(l, T21_R_ROW, convFoco[i] * a);
    rec_avatar((GfxRect){ l.x + 16, l.y + 10, 44, 44 }, amigoAv[i], amigoNome[i], amigoId[i], a);
    texto(TXT_ILHA_NOME, amigoNome[i], l.x + 76, l.y + 16, 245, a, l.w - 150);
    gfx_icone((GfxRect){ l.x + l.w - 52, l.y + 16, 32, 32 }, dentro || marcado[i] ? "aj_circle-check" : "aj_user-plus",
              dentro ? 0.55f : 1, dentro ? 0.92f : 1, dentro ? 0.65f : 1, marcado[i] || dentro ? a : .6f * a);
  }
  gfx_recorte_voltar(fora);
}

// Cinema start: the lobby art lifts off the live film (the countdown's dark
// going with it); a base scrim keeps the title and the two buttons legible.
static void desenharCinema(GfxRect tela, float L, float B, float e) {
  const JsMotor *m = juntos_motor();
  float y = B - 150, x = L;
  int i, nb = juntos_pode_controlar() ? 2 : 1, pausado = m->sala.rel.estado == JSE_PAUSADO;
  telaCinema(tela, 1 - e);
  gfx_cor(tela, 0, 0.03f, 0.03f, 0.05f, 0.55f * (1 - e));
  gfx_veu_base(tela, 0, 0.78f, 0.9f * e);
  texto(TXT_CAPTION, m->sala.titulo[0] ? m->sala.titulo : juntos_titulo(), L, y - 112, 225, e, 900);
  texto(TXT_ILHA_TITULO, i18n("O filme começou"), L, y - 76, 250, e, 1000);
  for (i = 0; i < nb; i++) {
    const char *rot = i == 0 ? i18n("Entrar na sessão") : pausado ? i18n("Continuar") : i18n("Pausar");
    const char *ic = i == 0 ? "aj_clapperboard" : pausado ? "aj_play" : "aj_pause";
    float w = botao_largura(rot, ic, i == 0), h = i == 0 ? BOTAO_H_PRIMARIO : BOTAO_H_SECUNDARIO;
    botao_pilula((GfxRect){ x, y + (BOTAO_H_PRIMARIO - h) / 2, w, h }, rot, ic, focoDe(i), i == 0, 0, e * entradaItem(i + 1));
    x += w + BOTAO_GAP;
  }
}

static void desenharSala(void) {
  const JsMotor *m = juntos_motor();
  GfxRect tela = telaReal();
  float L = tela.x + SALA_MARGEM, R = tela.x + tela.w - SALA_MARGEM, T = tela.y, B = tela.y + tela.h;
  float larguras[BT_N], total = 0, a = 1, convFundo = 1 - .35f * anim_suave(convAnim);
  int i, b[BT_N], n = botoesSala(b) - 1, nm, k;
  char linha[200];
  if (!m) return;
  nm = m->sala.n < 5 ? m->sala.n : 5;
  // One curved row like the mockup: centred, end seats sit lower.
  k = nm < 5 ? nm + 1 : 5;
  float meio = (k - 1) / 2.0f, queda = 7 * meio * meio;
  float acoesY = B - 150, statusY = acoesY - 66, lugarY = statusY - 40 - queda - LUGAR_H;
  lugarW = fminf(248, (R - L - 4 * LUGAR_GAP) / 5);
  if (cinema) { desenharCinema(tela, L, B, anim_suave(cinemaAnim)); custoCamada(4); return; }
  telaCinema(tela, a);
  custoCamada(0);
  a *= convFundo;
  cabecalhoSala(L, R, T + 54, a * entradaItem(0));
  float codigoW = botao_largura(rotuloBotao(BT_CODIGO), iconeBotao(BT_CODIGO), 0);
  navigationTarget(n,(GfxRect){R-codigoW,T+104,codigoW,BOTAO_H_SECUNDARIO});
  botao_pilula((GfxRect){ R - codigoW, T + 104, codigoW, BOTAO_H_SECUNDARIO },
               rotuloBotao(BT_CODIGO), iconeBotao(BT_CODIGO), focoDe(n), 0, 0, a * entradaItem(1));
  // Hero block like Detail: kicker, headline, then the film's logo (or title).
  {
    float e = entradaItem(1), dy = (1 - e) * NV_ENTRA_DY, logoH = 150, logoY = lugarY - 56 - logoH;
    snprintf(linha, sizeof linha, i18n("Sua sessão privada · %d de %d lugares"), nm, 5);
    texto(TXT_CAPTION, linha, L, logoY - 112 + dy, 225, a * e, 900);
    texto(TXT_ILHA_TITULO, i18n("Os melhores lugares são juntos"), L, logoY - 76 + dy, 250, a * e, 1000);
    const char *tit = m->sala.titulo[0] ? m->sala.titulo : juntos_titulo();
    logotitulo_desenhar(itemSala(), tit, TXT_TITULO1, L, logoY + dy, 620, logoH, 1000, a * e);
  }
  custoCamada(1);
  float x0 = (L + R) / 2 - (k * LUGAR_W + (k - 1) * LUGAR_GAP) / 2;
  for (i = 0; i < k; i++) {
    float x = x0 + i * (LUGAR_W + LUGAR_GAP), d = i - meio;
    if (i < nm) desenharLugar(i, &m->sala.m[i], 0, 0, x, lugarY + 7 * d * d, a);
    else desenharLugar(i, NULL, 1, focoDe(-1), x, lugarY + 7 * d * d, a);
  }
  custoCamada(2);
  {
    // Readiness, centred under the seats: who is still getting ready, green when all are.
    int pr = todosProntos(m), prontos = 0, falta = -1;
    for (int j = 0; j < m->sala.n; j++) if (membroPronto(&m->sala.m[j])) prontos++; else if (falta < 0) falta = j;
    if (pr) snprintf(linha, sizeof linha, "%s", i18n("Todos prontos. O filme está preparado em cada TV."));
    else if (falta >= 0) {
      const char *url, *quem;
      char frase[160];
      rostoMembro(&m->sala.m[falta], &url, &quem);
      snprintf(frase, sizeof frase, fraseEspera(&m->sala.m[falta]), quem);
      snprintf(linha, sizeof linha, i18n("%d de %d prontos. %s"), prontos, m->sala.n, frase);
    } else snprintf(linha, sizeof linha, "%s", i18n("Esperando os outros ficarem prontos"));
    if (strcmp(linha, prontinha)) { snprintf(prontinha, sizeof prontinha, "%s", linha); prontinhaAnim = 0; }
    float v = ajustes_animacoes_reduzidas_usuario() ? 1 : anim_suave(prontinhaAnim);
    float r = pr ? 0.55f : .92f, g = pr ? 0.92f : .92f, bl = pr ? 0.65f : .92f, ya = statusY + (1 - v) * 12;
    TxtLinha l = txt_linha_corta(TXT_ILHA_NOME, linha, r * 255, g * 255, bl * 255, 255, R - L - 48);
    float lx = (L + R) / 2 - (l.w + 48) / 2, la = a * entradaItem(2) * (.2f + .8f * v);
    GfxRect ic = { lx, ya, 32, 32 };
    if (pr) gfx_icone(ic, "aj_circle-check", r, g, bl, la);
    else { gfx_anel(ic, .5f, 3, 1, 1, 1, 0.15f * la); gfx_arco(ic, 3, fmodf(agoraUlt / 1100.0f, 1), .25f, 0.62f, 0.78f, 1, la); }
    txt_desenhar_alpha(l, lx + 48, ya, la);
  }
  for (i = 0; i < n; i++) {
    larguras[i] = botao_largura(rotuloBotao(b[i]), iconeBotao(b[i]), b[i] == BT_COMECAR && todosProntos(m));
    total += larguras[i] + (i ? BOTAO_GAP : 0);
  }
  // Actions row, anchored like Detail's: main group left, session group right.
  {
    float xe = L, xd = R, y = acoesY;
    int quebra = total > R - L;
    for (i = 0; i < n; i++) {
      int prim = b[i] == BT_COMECAR && todosProntos(m), esq = quebra || b[i] <= BT_FONTES;
      if (!esq) continue;
      float h = prim ? BOTAO_H_PRIMARIO : BOTAO_H_SECUNDARIO;
      if (xe + larguras[i] > R) { xe = L; y += BOTAO_H_PRIMARIO + BOTAO_GAP / 2; }
      GfxRect r = { xe, y + (BOTAO_H_PRIMARIO - h) / 2, larguras[i], h };
      navigationTarget(i,r);
      // All ready: the start button breathes until pressed.
      if (prim && !ajustes_animacoes_reduzidas_usuario())
        gfx_anel_fora(r, .5f, 4 + 6 * (.5f + .5f * sinf(agoraUlt / 380.0f)), 2, 0.55f, 0.92f, 0.65f, .45f * a);
      botao_pilula(r, rotuloBotao(b[i]), iconeBotao(b[i]), focoDe(i), prim, 0,
                   a * entradaItem(i + 2) * (b[i] == BT_FILME && !m->temSala ? .4f : 1));
      xe += larguras[i] + BOTAO_GAP;
    }
    for (i = n - 1; !quebra && i >= 0; i--) {
      if (b[i] <= BT_FONTES) continue;
      xd -= larguras[i];
      navigationTarget(i,(GfxRect){xd,acoesY+(BOTAO_H_PRIMARIO-BOTAO_H_SECUNDARIO)/2,larguras[i],BOTAO_H_SECUNDARIO});
      botao_pilula((GfxRect){ xd, acoesY + (BOTAO_H_PRIMARIO - BOTAO_H_SECUNDARIO) / 2, larguras[i], BOTAO_H_SECUNDARIO },
                   rotuloBotao(b[i]), iconeBotao(b[i]), focoDe(i), 0, 0, a * entradaItem(i + 2));
      xd -= BOTAO_GAP;
    }
  }
  texto(TXT_CAPTION, i18n("Setas para mover · OK para escolher · Voltar para retornar"), L, B - 64, 170, a, R - L);
  desenharConvite(R, T + 180, acoesY - 24, 1);
  custoCamada(3);
}

// The session sidebar (CH+ in the film): the Saved sidebar shell with only the
// session, the people, inviting more (the lobby invite) and the settings.
static float painelFocoY, painelFocoH;   // the focused row in content space
static float painelLinha(GfxRect A, float y, float h, int f, float a) {
  if (f == foco) { painelFocoY = y + folhaScroll - A.y; painelFocoH = h; }
  spainel_lateral_linha(A, y, h, focoDe(f), a);
  return h;
}
static void corpoPainel(GfxRect A, float a, void *u) {
  const JsMotor *m = juntos_motor();
  float y = A.y - folhaScroll, x = A.x;
  int i, f = 0, nm;
  char c[64];
  (void)u;
  if (!m) return;
  nm = m->sala.n < 5 ? m->sala.n : 5;
  GfxRecorteAnt fora = gfx_recorte_dentro(A.x - 48, A.y, A.w + 96, A.h);
  y += spainel_lateral_secao(A, y, i18n("Sessão"), a, 1);
  if (juntos_codigo()[0]) snprintf(c, sizeof c, "%.3s-%s", juntos_codigo(), juntos_codigo() + 3);
  else snprintf(c, sizeof c, "%s", juntos_precisa_auth() ? i18n("Entre na sua conta para enviar") : i18n("Preparando o código…"));
  texto(TXT_ILHA_META, i18n("Código da sala"), x, y + 6, 185, a, A.w);
  texto(TXT_ILHA_TITULO, c, x, y + 34, 250, a, A.w);
  texto(TXT_ILHA_CORPO, rotuloBotao(BT_TRAVA), x, y + 94, 215, a, A.w);
  y += 150;
  y += spainel_lateral_secao(A, y, i18n("Pessoas"), a, 0);
  for (i = 0; i < nm; i++, y += 76) {
    const JsMembro *p = &m->sala.m[i];
    const char *url, *nome;
    rostoMembro(p, &url, &nome);
    rec_avatar((GfxRect){ x, y + 12, 52, 52 }, url, nome, p->id, p->on ? a : .45f * a);
    texto(TXT_ILHA_NOME, nome, x + 70, y + 10, 245, a, A.w - 70);
    texto(TXT_ILHA_META, p->anf ? i18n("anfitrião") : estadoTexto(p), x + 70, y + 42, 170, a, A.w - 70);
  }
  y += spainel_lateral_secao(A, y, i18n("Convidar"), a, 0);
  if (!nAmigos) {
    txt_bloco_corta(TXT_ILHA_META, i18n("Nenhum amigo ainda. Quem tiver o código entra pedindo."),
                    185, 185, 185, x, y + 8, A.w, 26, a, 3);
    y += 96;
  }
  for (i = 0; i < nAmigos; i++, f++) {
    int dentro = 0;
    for (int j = 0; j < m->sala.n; j++) dentro |= !strcmp(m->sala.m[j].id, amigoId[i]);
    float h = painelLinha(A, y, 72, f, a);
    rec_avatar((GfxRect){ x, y + 14, 44, 44 }, amigoAv[i], amigoNome[i], amigoId[i], a);
    texto(TXT_ILHA_NOME, amigoNome[i], x + 62, y + 20, 245, a, A.w - 120);
    gfx_icone((GfxRect){ x + A.w - 32, y + 20, 32, 32 }, dentro || marcado[i] ? "aj_circle-check" : "aj_user-plus",
              dentro ? 0.55f : 1, dentro ? 0.92f : 1, dentro ? 0.65f : 1, marcado[i] || dentro ? a : .6f * a);
    y += h;
  }
  y += spainel_lateral_secao(A, y, i18n("Ajustes"), a, 0);
  for (int b = juntos_sou_anfitriao() ? BT_TRAVA : BT_SAIR; b <= BT_SAIR; b++, f++) {
    float h = painelLinha(A, y, 72, f, a);
    gfx_icone((GfxRect){ x, y + 20, 32, 32 }, b == BT_TRAVA ? "aj_lock" : "aj_log-out", 1, 1, 1, .8f * a);
    texto(TXT_ILHA_NOME, rotuloBotao(b), x + 52, y + 20, 245, a, A.w - 52);
    y += h;
  }
  gfx_recorte_voltar(fora);
  painelAlvo = fmaxf(0, painelFocoY + painelFocoH + 24 - A.h);   // scroll the focused row into view
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
  float x, y = 124;
  int i, n;
  char label[96];
  if (!m || !m->temSala) return;
  n = m->sala.n > 5 ? 5 : m->sala.n;
  double t = (double)SDL_GetTicks64(), stamp = t;
  if (!ultimoDrift || agoraUlt - ultimoDrift >= 1000) {
    double pos = m->p.pos ? m->p.pos(m->p.u, &stamp) : 0;
    driftChip = m->p.pos ? (jss_pos_local(pos, stamp, t, m->p.tocando(m->p.u), m->velCent / 100.0) - jsm_pos_alvo(m, t)) * 1000 : 0;
    ultimoDrift = agoraUlt;
  }
  snprintf(label, sizeof label, "%s · %+.0f ms", i18n("Juntos"), driftChip);
  float w = largura(TXT_ILHA_META, label);
  x = 1920 - 80 - n * 40.0f - w - 32;
  gfx_vidro_painel((GfxRect){ x - 14, y - 8, n * 40.0f + w + 44, 64 }, 0.5f, 0.7f, 0.9f);
  texto(TXT_ILHA_META, label, x + n * 40.0f + 16, y + 12, 235, 1, 0);
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

// The server schedules a three-second countdown on its shared clock.
static int contagemRestante(void) {
  const JsMotor *m = juntos_motor();
  double falta;
#ifdef NV_SHOT_HOOKS
  if (shotContagem) return shotContagem;
#endif
  if (!m || !m->temSala || m->sala.rel.estado != JSE_TOCANDO) return 0;
  falta = m->sala.rel.t0 - jsr_servidor(&m->rel, (double)SDL_GetTicks64());
  if (falta <= 0 || falta > 3000) return 0;
  return (int)ceil(falta / 1000.0);
}

static void desenharContagem(int num) {
  const JsMotor *m = juntos_motor();
  char n[8];
  int i, nm;
  float x0;
  if (!m) return;
  nm = m->sala.n < 5 ? m->sala.n : 5;
  telaCinema(telaReal(), 1);   // lights out: only the film behind the count
  gfx_cor(telaReal(), 0, 0.03f, 0.03f, 0.05f, 0.55f);
  textoCentro(TXT_CAPTION, i18n("Começando juntos"), 960, 190, 235, 1, 1200);
  textoCentro(TXT_ILHA_TITULO, i18n("As luzes se apagam. Juntos."), 960, 236, 250, 1, 1300);
  snprintf(n, sizeof n, "%d", num);
  gfx_transformar(960, 450, .8f + .2f * contagemAnim, origemX, origemY);
  textoCentro(TXT_RANK_GRANDE, n, 960, 340, 255, contagemAnim, 300);
  gfx_transformar(960, 540, 1, origemX, origemY);
  x0 = 960 - nm * 92.0f / 2;
  for (i = 0; i < nm; i++) {
    const JsMembro *p = &m->sala.m[i];
    const char *url, *quem;
    GfxRect av = { x0 + i * 92.0f + 7, 640, 78, 78 };
    rostoMembro(p, &url, &quem);
    rec_avatar(av, url, quem, p->id, 1);
    // The ring closes with the count: 3, 2, 1, lights out.
    gfx_arco((GfxRect){ av.x - 6, av.y - 6, av.w + 12, av.h + 12 }, 3, 0, (4 - num - 1 + contagemAnim) / 3.0f, 0.55f, 0.92f, 0.65f, 1);
  }
  textoCentro(TXT_ILHA_NOME, m->sala.titulo[0] ? m->sala.titulo : juntos_titulo(), 960, 760, 230, 1, 1200);
}

static int camadaCorpo(Camada c) {
  return c != CAMADA_NADA && c != CAMADA_SALA && c != CAMADA_PAINEL;
}

float juntosui_shot_corpo_alfa(void) { return corpoAlfaAtual; }
static void desenharCorpo(GfxRect c, float a, void *u) {
  Camada visual = (Camada)(intptr_t)u;
  float opacidade = gfx_opacidade_grupo;
  Camada anterior = camadaVisual;
  camadaVisual = visual;
  origemX = c.x - corpoBase.x;
  origemY = c.y + (noPlayer() ? 0 : NV_ILHA_H_ABERTA) - corpoBase.y;
  gfx_transformar(0, 0, 1, origemX, origemY);
  gfx_opacidade_grupo *= a;
  corpoAlfaAtual = a;
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
  navigationCount=0;
  int medir = camadaVisual == CAMADA_SALA && entrada > .005f;
  if (medir) custoInicio();
  float esc = gfx_escala_entrar();
  origemX = (NV_VTELA_W - 1920) / 2;
  origemY = (NV_VTELA_H - 1080) / 2;
  gfx_transformar(960, 540, 1, origemX, origemY);
  if (noPlayer() && juntos_ativo() && !lobbyAberto && camadaVisual != CAMADA_PAINEL) { desenharRostos(); desenharBolhas(agora); }
  float opacidade = gfx_opacidade_grupo;
  gfx_opacidade_grupo *= anim_suave(entrada);
  int folha = camadaVisual != CAMADA_SALA && camadaVisual != CAMADA_FONTES && camadaVisual != CAMADA_REACOES && camadaVisual != CAMADA_PAINEL;
  if (folha) gfx_transformar(960, 540, T21_SHEET_SCALE + (1 - T21_SHEET_SCALE) * anim_suave(entrada), origemX, origemY);
  int contagem = juntos_ativo() ? contagemRestante() : 0;
  if (camadaVisual == CAMADA_SALA && !contagem) desenharSala();
  gfx_transformar(960, 540, 1, origemX, origemY);
  gfx_opacidade_grupo = opacidade;
  if (contagem) desenharContagem(contagem);
  gfx_sem_transformar();
  gfx_escala_sair(esc);
  if (camadaVisual == CAMADA_PAINEL) {
    const JsMotor *m = juntos_motor();
    spainel_lateral(entrada, m && m->sala.titulo[0] ? m->sala.titulo : juntos_titulo(), i18n("Assistir Juntos"), corpoPainel, NULL);
  }
  if (medir) { custoCamada(5); custoFim(); }
}
