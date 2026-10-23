#include "arteurl.h"
// A ilha do relogio — ver ilha.h.
//
// CUSTO, porque a C9 e o teto: por quadro sao no maximo uma sombra do tamanho
// da pilula (+ folga), a pilula, uma luz de canto do tamanho dela, um icone (ou
// um rosto e uma mini capa) e as linhas de texto. Nada de tela cheia, nada de
// FBO, nenhuma textura nova alem das de texto — que o cache de text.c ja guarda
// por string.
//
// SEM A VIRADA DO MINUTO (02/10). O numero velho subia e o novo vinha de baixo
// em 320 ms; o dono aprovou todos os estados do mockup MENOS esse: o minuto
// troca seco. Saiu o estado (horaAnt/horaT) e as duas linhas extras por virada.
#include "horafmt.h"
#include "ilha.h"
#include "ilha_voo.h"
#include "ajustes.h"
#include "anim.h"
#include "movimento.h"
#include "botoes.h"
#include "ponteiro.h"
#include "tex_cache.h"
#include "gfx.h"
#include "layout.h"
#include "menu.h"
#include "recomenda.h"
#include "salvosintro.h"
#include "text.h"
#include "central.h"
#include "sinopse.h"
#include "plrui.h"
#include "desempenho.h"
#include "trailer.h"
#include "idioma.h"
#include "idiomacod.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include "telefoneui.h"
#include "rolagemtoque.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

enum { M_RELOGIO = 0, M_AVISO, M_ATIVIDADE, M_CARTAO };

typedef struct {
  char chave[80], icone[32], texto[240];
  int tipo, tecla, prior, grupo, vivo, cartao;
  int conta;                    // quantos avisos este representa ("N avisos novos")
  unsigned ms, ordem;           // ordem: chegada, para o FIFO dentro da prioridade
  char rosto[256], rostoNome[64], capa[NV_ARTE_URL_MAX], meta[64];
  int temModal, abrir;
  IlhaModal modal;
  char titulo[120], kicker[48], dica[64];   // o aviso v2 (ilha.h, IlhaAvisoEx)
  int acao;
} Aviso;

#define FILA 6
static Aviso fila[FILA];
static int nFila;
static Aviso cur;               // o da tela (valido se temCur)
static int temCur;
static Uint32 curAte;           // 0 = ainda nao apareceu (o prazo conta do 1o quadro)
static Aviso mostraA;           // o aviso DESENHADO (pode atrasar o da vez)
static unsigned ordemSeq;

static char atvTexto[160], atvTitulo[96], atvDetalhes[640];
static int modalAtividade;
static IlhaAtvCarga atvCarga;   // etapa aponta para atvDetalhes
static int atvTemCarga;
static float atvBarraA;         // preenchimento animado da barra do painel
static float atvProg = -1.0f;
static int  atvV2;              // ilha_atividade_ex: o desenho de 72
static char atvIcone[32];       // "" = o ponto que respira
static Uint32 atvVisto;         // SDL_GetTicks da ultima renovacao

static int relogioQuer;
static int relogioFocado;
// Bolinha de acento no relogio em repouso: "ha enquete aberta" (enquete.c).
static int pontoEnquete;
static int pontoPediu;
#define PONTO_ENQ 10.0f
static int ancDef, ancDir;
static float ancX, ancY;

// Mola da forma e do conteudo.
static float W, vW, H, vH, A;
static float conteudoA;
static int   mostra = -1;               // o que esta DESENHADO (pode atrasar o alvo)
static char  mostraChave[80];
// MEDIDOR DE DESEMPENHO NA ILHA (desempenho.h): a forma no relogio DESENHADO
// (dsMostra, troca junto com `mostra`) e a do alvo deste quadro (dsVez). Ao
// lado de um cartao so cabe o Minimo (dsCartao), e so se a pilula nao passar
// de DS_CARTAO_MAX.
static int dsMostra, dsVez, dsCartao;
#define DS_CARTAO_MAX 1180.0f
static Uint32 ultQuadro;

// O relogio: troca seco, sem animacao (ver o topo).
static char hora[12];
static time_t horaSeg;

// Cartoes persistentes (ilha.h) e o que a pilula mostra deles.
static IlhaCartao cartoes[ILHA_N_CARTOES];
static int temCartao[ILHA_N_CARTOES];
static int cartaoVez = -1;              // o da vez neste quadro (-1 = nenhum)
static IlhaCartao mostraC;              // o DESENHADO (pode atrasar o da vez)
static int mostraQual;
static int coberta;
static GfxRect ultRect;
static int ultRectOk;

// O modal: a pilula cresce ate ele (modalT 0 -> 1, a mesma mola da forma).
// Dois donos: um CARTAO (modalAviso = 0, modalC/modalQual) ou um AVISO com
// modal proprio (modalAviso = 1, modalM/modalChave).
static int modalAberto, modalQual, modalFoco, modalAviso;
static IlhaCartao modalC;
static IlhaModal modalM;
static char modalChave[80];
static float modalT, modalV, modalFocoA[ILHA_MODAL_BOTOES];
// O CORPO PEDIDO (ilha_corpo): vale um quadro; o ultimo fica para a saida.
static int corpoPed, corpoSemSombra;
static float corpoW, corpoH, corpoT, corpoV;
static float corpoWa, corpoHa, corpoVw, corpoVh;   // o tamanho na mola (o pedido muda)
static IlhaCorpo corpoFn;
static void *corpoU;
static Uint32 modalDesde;
static int pedido, pedidoQual;
static IlhaCartao pedidoC;
static int avPedido;
static char avPedidoChave[80];
static float modalAvisoH = 414.0f;   // altura do modal de aviso (layoutModalAviso)
#ifdef NV_TOUCH_UI
static ToqueRolagem toqueModal;
static float toqueModalY;
static void modalToqueReiniciar(void) {
  toqueModalY = 0; memset(&toqueModal, 0, sizeof toqueModal);
}
static int modalToqueRolar(const PonteiroRolagem *e) {
  return modalAberto ? toquerol_evento(&toqueModal, e) : 0;
}
#endif
static int   modalAvisoMedido;       // 0 = medir no proximo quadro

// MINIMIZAR NA ILHA (ilha_minimizar): o quadro do video encolhe ate a mini capa.
static int voo;                  // 1 = em voo
static float vooT;              // 0 -> 1, sem repique
static Uint32 vooDesde;
static GfxRect vooAlvo;
static int vooAlvoOk;
static char vooArte[NV_ARTE_URL_MAX], vooCapa[NV_ARTE_URL_MAX];
// DISSOLVER (Android, sessao retida): o video parado continua no plano de
// baixo, entao o primeiro quadro do voo e ELE (a tela inteira transparente) e
// a arte + home entram por cima em VOO_DISSOLVE_MS, ja encolhendo. Sem isso o
// voo nascia com o still em tela cheia: um corte do quadro do filme para o
// fundo do titulo. (Copiar o quadro real com PixelCopy levou 603-724 ms na
// TCL, 02/10: lento demais para a saida.)
#define VOO_DISSOLVE_MS 150u
static int vooDissolve;
// SALVAR (ilha_salvar): a capa voa ate o icone do aviso "salvar-aviso".
#define SALVAR_CHAVE ILHA_ACAO_CHAVE
static int svooQuer, svoo;           // quer = pedido a espera do aviso na tela
static float svooT, svooIconeA = 1.0f;
static Uint32 svooDesde;
static GfxRect svooDe;
static char svooPoster[NV_ARTE_URL_MAX];
static Uint32 pousouEm;          // o pulso da pilula conta daqui
static Uint32 altBase;           // a alternancia dos cartoes conta daqui

static int priorDoTipo(int tipo) {
  return tipo == ILHA_ERRO ? ILHA_P1 : tipo == ILHA_ACENTO ? ILHA_P2 : ILHA_P3;
}

const char *ilha_forte(char *dst, size_t tam, const char *s) {
  snprintf(dst, tam, ILHA_FORTE "%s" ILHA_FORTE, s ? s : "");
  return dst;
}

// Quantos avisos esperam (o "+N"): o "N avisos novos" conta pelos que juntou.
static int esperando(void) {
  int i, k = 0;
  for (i = 0; i < nFila; i++) k += fila[i].conta > 0 ? fila[i].conta : 1;
  return k;
}

// Entra na fila na ORDEM DE PRIORIDADE (P1 na frente), chegada dentro dela.
// Cheia: sai o ultimo (a prioridade mais baixa e mais nova); se o que chega e
// ainda mais baixo que ele, quem fica de fora e o que chega.
static void enfileirar(const Aviso *a) {
  int i;
  if (nFila == FILA) {
    if (fila[FILA - 1].prior < a->prior) return;
    nFila--;
  }
  for (i = nFila; i > 0 && (fila[i - 1].prior > a->prior ||
                            (fila[i - 1].prior == a->prior && fila[i - 1].ordem > a->ordem)); i--)
    fila[i] = fila[i - 1];
  fila[i] = *a;
  nFila++;
}

// OS AVISOS DA CENTRAL QUE ESPERAM VIRAM UM SO. Chamado a cada entrada: com
// dois ou mais grupo = 1 na fila, eles saem e entra "N avisos novos" (a mesma
// chave e o mesmo prazo do toast da central de antes), que abre a lista. Um so
// esperando continua dizendo o assunto.
#define CHAVE_CENTRAL "avisos"
static void juntarCentral(void) {
  int i, j, k = 0, soma = 0;
  unsigned ordem = 0;
  Aviso m;
  for (i = 0; i < nFila; i++) if (fila[i].grupo) { if (!k++) ordem = fila[i].ordem; soma += fila[i].conta; }
  if (k < 2) return;
  for (i = j = 0; i < nFila; i++) if (!fila[i].grupo) fila[j++] = fila[i];
  nFila = j;
  memset(&m, 0, sizeof m);
  snprintf(m.chave, sizeof m.chave, "%s", CHAVE_CENTRAL);
  snprintf(m.icone, sizeof m.icone, "sino");
  snprintf(m.texto, sizeof m.texto, soma == 1 ? i18n("%d aviso novo") : i18n("%d avisos novos"), soma);
  m.tipo = ILHA_ACENTO; m.prior = ILHA_P2; m.grupo = 1; m.conta = soma;
  m.tecla = 1; m.ms = 20000u; m.ordem = ordem;
  enfileirar(&m);
}

void ilha_avisar_ex(const IlhaAvisoEx *e) {
  static unsigned seq;
  Aviso a;
  int i;
  if (!e || !e->texto || !e->texto[0]) return;
  memset(&a, 0, sizeof a);
  // Sem chave, uma propria: dois avisos avulsos nunca se fundem.
  if (e->chave && e->chave[0]) snprintf(a.chave, sizeof a.chave, "%s", e->chave);
  else snprintf(a.chave, sizeof a.chave, "#%u", ++seq);
  snprintf(a.icone, sizeof a.icone, "%s", e->icone ? e->icone : "");
  snprintf(a.texto, sizeof a.texto, "%s", e->texto);
  snprintf(a.rosto, sizeof a.rosto, "%s", e->rosto ? e->rosto : "");
  snprintf(a.rostoNome, sizeof a.rostoNome, "%s", e->rostoNome ? e->rostoNome : "");
  snprintf(a.capa, sizeof a.capa, "%s", e->capa ? e->capa : "");
  snprintf(a.meta, sizeof a.meta, "%s", e->meta ? e->meta : "");
  a.tipo = e->tipo; a.ms = e->ms ? e->ms : 4000u;
  a.prior = e->prior ? e->prior : priorDoTipo(e->tipo);
  a.grupo = e->grupo; a.vivo = e->vivo; a.cartao = e->cartao; a.conta = 1;
  a.tecla = e->tecla || e->modal || e->cartao;
  if (e->modal) { a.temModal = 1; a.modal = *e->modal; a.abrir = e->abrir; }
  snprintf(a.titulo, sizeof a.titulo, "%s", e->titulo ? e->titulo : "");
  snprintf(a.kicker, sizeof a.kicker, "%s", e->kicker ? e->kicker : "");
  snprintf(a.dica, sizeof a.dica, "%s", e->dica ? e->dica : "");
  a.acao = e->acao;
  if (a.acao) a.tecla = 1;
  a.ordem = ++ordemSeq;
  // Mesma chave na tela: troca no lugar e renova o prazo.
  if (temCur && !strcmp(cur.chave, a.chave)) {
    a.ordem = cur.ordem;
    cur = a;
    if (curAte) curAte = SDL_GetTicks() + a.ms;
    return;
  }
  for (i = 0; i < nFila; i++)
    if (!strcmp(fila[i].chave, a.chave)) { a.ordem = fila[i].ordem; fila[i] = a; return; }
  if (!temCur) { cur = a; temCur = 1; curAte = 0; return; }
  // FURA A FILA: um P1 com algo menos urgente na tela entra ja. O que estava
  // volta para a frente da fila (a ordem dele e a mais antiga) e reaparece
  // com o prazo inteiro — cortado no meio, ele nao foi lido. MENOS o da
  // central: esse ja apareceu, continua na lista de avisos, e o mockup mostra
  // o erro com "+2" (so os que esperavam) e depois "2 avisos novos".
  if (a.prior == ILHA_P1 && cur.prior > ILHA_P1) {
    if (!cur.grupo) enfileirar(&cur);
    cur = a; curAte = 0;
    juntarCentral();
    return;
  }
  enfileirar(&a);
  juntarCentral();
}

void ilha_avisar(const char *chave, int tipo, const char *icone,
                 const char *texto, unsigned ms, int tecla) {
  IlhaAvisoEx e;
  memset(&e, 0, sizeof e);
  e.chave = chave; e.tipo = tipo; e.icone = icone; e.texto = texto; e.ms = ms; e.tecla = tecla;
  ilha_avisar_ex(&e);
}

static void proximo(void) {
  temCur = 0;
  if (nFila > 0) {
    cur = fila[0];
    memmove(fila, fila + 1, sizeof fila[0] * (size_t)(nFila - 1));
    nFila--;
    temCur = 1; curAte = 0;
  }
}

void ilha_retirar(const char *chave) {
  int i, j;
  if (!chave || !chave[0]) return;
  for (i = j = 0; i < nFila; i++)
    if (strcmp(fila[i].chave, chave)) fila[j++] = fila[i];
  nFila = j;
  if (temCur && !strcmp(cur.chave, chave)) proximo();
}

// AS CHAVES QUE APARECERAM (ilha_mostrou). Um anel curto: quem pergunta o faz
// a cada 2 s (ilhasinais.c), muito antes de 16 avisos passarem.
#define MOSTRADOS 16
static char mostrados[MOSTRADOS][sizeof(((Aviso *)0)->chave)];
static int nMostrados;
static void marcarMostrado(const char *chave) {
  if (nMostrados == MOSTRADOS) {
    memmove(mostrados[0], mostrados[1], sizeof mostrados[0] * (MOSTRADOS - 1));
    nMostrados--;
  }
  snprintf(mostrados[nMostrados++], sizeof mostrados[0], "%s", chave);
}
int ilha_mostrou(const char *chave) {
  int i, achou = 0;
  if (!chave) return 0;
  for (i = 0; i < nMostrados; ) {
    if (!strcmp(mostrados[i], chave)) {
      memmove(mostrados[i], mostrados[i + 1], sizeof mostrados[0] * (size_t)(nMostrados - i - 1));
      nMostrados--; achou = 1;
    } else i++;
  }
  return achou;
}
// O AVISO DA VEZ ENTRA NA TELA: o prazo comeca a contar e a chave vira "dita".
// COBERTA (o painel de Salvos nascido da pilula, o cartao da atualizacao) o
// prazo NAO comeca e, se ja corria, fica parado: antes ele vencia por baixo
// do painel e o pedido de amizade sumia sem ninguem ter lido.
static void vezNaTela(Uint32 agora, Uint32 antes) {
  if (!temCur) return;
  if (coberta) {
    if (curAte && antes && agora > antes) curAte += agora - antes;
    return;
  }
  if (!curAte) { curAte = agora + cur.ms; marcarMostrado(cur.chave); }
}

int ilha_tem(const char *chave) {
  int i;
  if (!chave) return 0;
  if (temCur && !strcmp(cur.chave, chave)) return 1;
  for (i = 0; i < nFila; i++) if (!strcmp(fila[i].chave, chave)) return 1;
  return 0;
}

const char *ilha_aviso_vez(void) { return temCur ? cur.chave : ""; }
int ilha_esperando(void) { return esperando(); }

void ilha_retirar_grupo(void) {
  int i, j;
  for (i = j = 0; i < nFila; i++) if (!fila[i].grupo) fila[j++] = fila[i];
  nFila = j;
  if (temCur && cur.grupo) proximo();
}

int ilha_tecla_central(void) {
  // O episodio novo abre o modal da ESTREIA; sem o cartao na pilula (relogio
  // desligado), a mesma tecla cai na central, onde o item esta.
  int cartaoNaPilula = cur.cartao > 0 && cur.cartao <= ILHA_N_CARTOES && temCartao[cur.cartao - 1];
  return temCur && curAte && cur.tecla && !cur.temModal && !cur.acao && !cartaoNaPilula && A > 0.5f;
}

int ilha_aviso_pediu(char *chave, size_t tam) {
  int o = avPedido;
  if (!o) return 0;
  avPedido = 0;
  if (chave && tam) snprintf(chave, tam, "%s", avPedidoChave);
  return o;
}

void ilha_acao(const char *icone, const char *texto, int tipo, const char *poster,
               const GfxRect *de, const IlhaModal *modal) {
  IlhaAvisoEx e;
  if (!texto || !texto[0]) return;
  memset(&e, 0, sizeof e);
  e.chave = SALVAR_CHAVE; e.icone = icone && icone[0] ? icone : "check"; e.texto = texto;
  e.tipo = tipo;
  e.ms = 6000u;
  e.modal = modal;
  ilha_avisar_ex(&e);
  svoo = 0; svooIconeA = 1.0f;
  svooQuer = 0;
  if (poster && poster[0] && !anim_politica_reduzida && !ajustes_animacoes_reduzidas()) {
    snprintf(svooPoster, sizeof svooPoster, "%s", poster);
    svooDe = de ? *de : (GfxRect){ NV_TELA_W * 0.5f - 75.0f, 520.0f, 150.0f, 225.0f };
    svooQuer = 1;
  }
}

void ilha_atividade_ex(const char *texto, float progresso, const char *icone) {
  ilha_atividade(texto, progresso);
  if (!icone) return;
  atvV2 = 1;
  snprintf(atvIcone, sizeof atvIcone, "%s", icone);
}

void ilha_atividade(const char *texto, float progresso) {
  snprintf(atvTexto, sizeof atvTexto, "%s", texto ? texto : "");
  atvProg = progresso;
  atvV2 = 0; atvIcone[0] = 0;
  atvVisto = SDL_GetTicks();
  if (!atvVisto) atvVisto = 1;
}

void ilha_atividade_detalhes(const char *titulo, const char *texto) {
  snprintf(atvTitulo, sizeof atvTitulo, "%s", titulo ? titulo : "");
  snprintf(atvDetalhes, sizeof atvDetalhes, "%s", texto ? texto : "");
  atvTemCarga = 0;
}
void ilha_atividade_carga(const IlhaAtvCarga *c) {
  if (!c) { atvTemCarga = 0; return; }
  atvCarga = *c;
  atvCarga.etapa = atvDetalhes;
  atvTemCarga = 1;
}
int ilha_atividade_expansivel(void) { return atvDetalhes[0] && atvVisto && SDL_GetTicks() - atvVisto < 400u; }

void ilha_relogio_visivel(int visivel) {
  relogioQuer = visivel;
  if (!visivel) relogioFocado = 0;
}
static void desenharFocoRelogio(GfxRect r, float raio, float alpha) {
  if (relogioFocado && relogioQuer && modalT <= 0.0f && corpoT <= 0.0f)
    gfx_anel_fora(r, raio, 4.0f, 3.0f, 1.0f, 1.0f, 1.0f, alpha);
}
void ilha_relogio_focar(int foco) { relogioFocado = foco ? 1 : 0; }
int ilha_relogio_focado(void) { return relogioFocado; }
void ilha_ponto_enquete(int aberto) { pontoEnquete = aberto ? 1 : 0; if (!pontoEnquete) pontoPediu = 0; }
int  ilha_ponto_pediu(void) { int p = pontoPediu; pontoPediu = 0; return p; }

void ilha_cartao(int qual, const IlhaCartao *c) {
  if (qual < 0 || qual >= ILHA_N_CARTOES) return;
  if (!c) { temCartao[qual] = 0; return; }
  cartoes[qual] = *c;
  temCartao[qual] = 1;
}

void ilha_cartao_invalidar(int qual) {
  if (qual < 0 || qual >= ILHA_N_CARTOES) return;
  temCartao[qual] = 0;
  memset(&cartoes[qual], 0, sizeof cartoes[qual]);
  if (cartaoVez == qual) cartaoVez = -1;
  // A pilula conserva uma copia durante a troca de conteudo. Ela tambem
  // pertence a quem saiu e nao pode dissolver sobre a tela da outra pessoa.
  if (mostraQual == qual) {
    if (mostra == M_CARTAO) { mostra = -1; conteudoA = 0.0f; mostraChave[0] = 0; }
    memset(&mostraC, 0, sizeof mostraC);
  }
  if (modalQual == qual) {
    ilha_modal_fechar(1);
    memset(&modalC, 0, sizeof modalC);
  }
  if (pedido && pedidoQual == qual) {
    pedido = 0;
    memset(&pedidoC, 0, sizeof pedidoC);
  }
  if (qual == ILHA_VIVO) {
    voo = vooDissolve = vooAlvoOk = 0; pousouEm = 0;
    vooArte[0] = vooCapa[0] = 0;
  }
}

// QUAL CARTAO E O DA VEZ. Com os dois, alternam a cada ILHA_ALTERNA_MS: a
// sessao interrompida e a estreia sao as duas "o que eu faco agora", e nenhuma
// deve esconder a outra para sempre. Um so: ele.
static int cartaoDaVez(Uint32 agora) {
  int q[ILHA_N_CARTOES], n = 0, i;
  for (i = 0; i < ILHA_N_CARTOES; i++) if (temCartao[i]) q[n++] = i;
  if (!n) return -1;
  return q[((agora - altBase) / ILHA_ALTERNA_MS) % (unsigned)n];
}

int ilha_cartao_na_tela(void) { return relogioQuer && cartaoVez >= 0; }

void ilha_coberta(int c) { coberta = c; }

int ilha_rect(float *x, float *y, float *w, float *h) {
  if (!ultRectOk) return 0;
  *x = ultRect.x; *y = ultRect.y; *w = ultRect.w; *h = ultRect.h;
  return 1;
}

// --- o modal ---------------------------------------------------------------------
// ESTREIA: Assistir · Depois · Dispensar. O par aprovado em 02/10 era
// Assistir / Depois, com "Depois" recolhendo SEM marcar e o cartao ficando na
// pilula — e ficava ate a pessoa abrir a serie (dono, 06/10: "o lembrete de
// serie so sai se eu entrar na serie"). Agora "Depois" tira o cartao ate o app
// fechar e "Dispensar" tira de vez aquele episodio (ver ILHA_PEDIU_* em ilha.h).
// AMIGO: Ver tambem · Detalhes · Fechar, o A5 de oportunidades.md.
int ilha_retomar_focado(IlhaCartao *c) {
  if (!modalAberto || modalAviso || modalAtividade || modalQual != ILHA_VIVO || modalFoco != 0) return 0;
  if (c) *c = modalC;
  return 1;
}
static int nBotoes(void) {
  if (modalAtividade) return 1;
  if (modalAviso) return modalM.nBotoes < 1 ? 1 : modalM.nBotoes > ILHA_MODAL_BOTOES ? ILHA_MODAL_BOTOES : modalM.nBotoes;
  return 3;
}
static const char *rotuloBotao(int i) {
  if (modalAtividade) return i18n("Fechar");
  if (modalAviso) return modalM.botao[i];
  if (modalQual == ILHA_ESTREIA) return i == 0 ? i18n("Assistir") : i == 1 ? i18n("Depois") : i18n("Dispensar");
  if (i == 0) return modalQual == ILHA_AMIGO ? i18n("Ver também") : i18n("Retomar");
  if (i == 1) return i18n("Detalhes");
  return i18n("Dispensar");   // a mesma palavra do menu do cartao "Retomar agora"
}
static const char *iconeBotao(int i) {
  if (modalAviso) return modalM.botaoIcone[i][0] ? modalM.botaoIcone[i] : NULL;
  if (i == 0) return "play";
  if (modalQual == ILHA_ESTREIA) return i == 1 ? "aj_clock" : "aj_x";
  if (i == 1) return "aj_info";
  return NULL;
}
static int modalTemSalvos(void) { return !modalAtividade && (modalAviso ? modalM.salvos : 1); }

static void abrirCartao(int qual) {
#ifdef NV_TOUCH_UI
  modalToqueReiniciar();
#endif
  modalAtividade = 0;
  modalAberto = 1;
  modalAviso = 0;
  modalQual = qual;
  modalC = cartoes[qual];
  modalFoco = 0;
  modalDesde = SDL_GetTicks();
  memset(modalFocoA, 0, sizeof modalFocoA);
}

int ilha_modal_abrir(void) {
  if (modalAberto || !relogioQuer) return 0;
#ifdef NV_TOUCH_UI
  modalToqueReiniciar();
#endif
  modalAtividade = ilha_atividade_expansivel();
  if (!modalAtividade && cartaoVez < 0) return 0;
  if (!modalAtividade) abrirCartao(cartaoVez);
  else { atvBarraA = 0.0f; modalAberto = 1; modalAviso = 0; modalFoco = 0; modalDesde = SDL_GetTicks(); memset(modalFocoA, 0, sizeof modalFocoA); }
  return 1;
}

// O AVISO NA TELA ABRE O SEU MODAL (AZUL/CH+ ou o clique): a pilula cresce
// dele, e o aviso sai da fila — respondido, ele nao volta quando o modal
// recolhe. Com `cartao`, o modal e o do cartao (o episodio novo abre a estreia).
static int abrirDoAviso(void) {
  if (modalAberto || !temCur || !curAte) return 0;
  modalAtividade = 0;
  if (cur.cartao > 0 && cur.cartao <= ILHA_N_CARTOES && temCartao[cur.cartao - 1]) {
    abrirCartao(cur.cartao - 1);
    proximo();
    return 1;
  }
  // AVISO COM ACAO (a atualizacao: o cartao dela nasce da propria pilula):
  // quem pos decide, pelo contrato dos botoes do modal (ilha_aviso_pediu).
  if (cur.acao) {
    avPedido = 1;
    snprintf(avPedidoChave, sizeof avPedidoChave, "%s", cur.chave);
    proximo();
    return 1;
  }
  if (!cur.temModal) return 0;
#ifdef NV_TOUCH_UI
  modalToqueReiniciar();
#endif
  modalAberto = 1;
  modalAviso = 1;
  modalAvisoMedido = 0;
  modalM = cur.modal;
  snprintf(modalChave, sizeof modalChave, "%s", cur.chave);
  modalFoco = 0;
  modalDesde = SDL_GetTicks();
  memset(modalFocoA, 0, sizeof modalFocoA);
  proximo();
  return 1;
}

void ilha_modal_fechar(int seco) {
  modalAberto = 0;
  if (seco) { modalT = 0.0f; modalV = 0.0f; }
}

int ilha_modal_aberto(void) { return modalAberto; }
int ilha_modal_visivel(void) { return modalAberto || modalT > 0.01f; }

static void pedir(int o) {
  pedido = o; pedidoC = modalC; pedidoQual = modalQual;
}

int ilha_pediu(IlhaCartao *c, int *qual) {
  int o = pedido;
  if (!o) return 0;
  pedido = 0;
  if (c) *c = pedidoC;
  if (qual) *qual = pedidoQual;
  return o;
}

static void acionar(int i) {
  if (modalAtividade) { ilha_modal_fechar(0); return; }
  if (modalAviso) {
    // Quem pos o aviso decide o que o botao faz (ilha_aviso_pediu).
    avPedido = i + 1;
    snprintf(avPedidoChave, sizeof avPedidoChave, "%s", modalChave);
    ilha_modal_fechar(0);
    return;
  }
  if (i == 0) { pedir(ILHA_PEDIU_TOCAR); ilha_modal_fechar(0); }
  else if (modalQual == ILHA_ESTREIA && i == 1) {
    // "Depois": o cartao sai da pilula ate a proxima sessao (ilhacart_adiar).
    pedir(ILHA_PEDIU_DEPOIS);
    temCartao[modalQual] = 0;
    ilha_modal_fechar(0);
  }
  else if (i == 1) { pedir(ILHA_PEDIU_DETALHES); ilha_modal_fechar(0); }
  else {
    // "Fechar" tira o cartao: o modal recolhe para uma pilula que ja nao o tem.
    pedir(ILHA_PEDIU_DISPENSAR);
    temCartao[modalQual] = 0;
    ilha_modal_fechar(0);
  }
}

static int teclaAzul(SDL_Keycode k, int sc) {
  return k == SDLK_s || sc == NV_SCANCODE_BLUE || sc == NV_SCANCODE_CH_UP || k == SDLK_PAGEUP;
}

int ilha_evento(const SDL_Event *e) {
  SDL_Keycode k;
  int sc;
  if (!modalAberto) {
    // AZUL/CH+ COM UM AVISO QUE TEM MODAL NA TELA: a pilula cresce dele. A
    // tecla da central (aviso sem modal) segue para avisos_evento.
    if (e->type == SDL_KEYDOWN && !e->key.repeat &&
        teclaAzul(e->key.keysym.sym, e->key.keysym.scancode) &&
        A > 0.5f && mostra == M_AVISO && abrirDoAviso())
      return 1;
    // AZUL/CH+ COM A BOLINHA NO RELOGIO PARADO: reabre a enquete. Quem a tem
    // (enquete.c) le o pedido por ilha_ponto_pediu e poe o convite de novo.
    if (e->type == SDL_KEYDOWN && !e->key.repeat && pontoEnquete && relogioQuer &&
        teclaAzul(e->key.keysym.sym, e->key.keysym.scancode) &&
        A > 0.5f && mostra == M_RELOGIO) {
      pontoPediu = 1;
      return 1;
    }
    return 0;
  }
  if (e->type != SDL_KEYDOWN) return e->type == SDL_KEYUP;
  k = e->key.keysym.sym; sc = e->key.keysym.scancode;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE || sc == NV_SCANCODE_BACK) {
    ilha_modal_fechar(0);
    return 1;
  }
  if (e->key.repeat && k != SDLK_LEFT && k != SDLK_RIGHT) return 1;
  if (k == SDLK_LEFT) { if (modalFoco > 0) modalFoco--; return 1; }
  // A DIREITA DO ULTIMO BOTAO (ou a AZUL de novo) e o painel de Salvos, que
  // nasce do proprio modal. A seta para o lado e o gesto natural: o painel
  // mora a direita da tela. So nos modais que mostram "Salvos ›".
  if (k == SDLK_RIGHT) {
    if (modalFoco + 1 < nBotoes()) modalFoco++;
    else if (!modalAtividade && modalTemSalvos()) pedir(ILHA_PEDIU_SALVOS);
    return 1;
  }
  // A MESMA JANELA DE 400 ms do atalho em app.c: o controle manda a AZUL
  // segurada como KEYDOWNs separados, e o segundo levaria direto ao painel.
  if (k == SDLK_s || sc == NV_SCANCODE_BLUE) {
    if (modalAtividade) ilha_modal_fechar(0);
    else if (modalTemSalvos() && SDL_GetTicks() - modalDesde >= 400u) pedir(ILHA_PEDIU_SALVOS);
    return 1;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) { acionar(modalFoco); return 1; }
  return 1;
}

// Magic Remote: passar por cima foca, o clique chega como OK (ponteiro.h).
static void pontFoco(int i, int b) { (void)b; modalFoco = i; }
static void pontFora(int a, int b) { (void)a; (void)b; ilha_modal_fechar(0); }
static void pontSalvos(int a, int b) { (void)a; (void)b; if (modalAberto) pedir(ILHA_PEDIU_SALVOS); }
// Magic Remote: o clique no relogio parado abre o painel de Salvos/Avisos
// (o mesmo pedido do modal; app.c o atende de qualquer tela fora do player).
static void pontRelogio(int a, int b) {
  (void)a; (void)b;
  pedido = ILHA_PEDIU_SALVOS; pedidoQual = -1; memset(&pedidoC, 0, sizeof pedidoC);
}
static void pontPilula(int a, int b) { (void)a; (void)b; ilha_modal_abrir(); }
static void pontAviso(int a, int b) { (void)a; (void)b; abrirDoAviso(); }

void ilha_ancorar(float x, float y, int daDireita) {
  ancDef = 1; ancX = x; ancY = y; ancDir = daDireita;
}

// O relogio fica SEMPRE no canto de cima a DIREITA, em toda tela e estado (dono,
// 03/10: ele mudava de lado conforme o que acontecia, e a opcao de lado saiu de
// Ajustes). `guia` ficou so por compatibilidade com quem chama.
void ilha_posicionar(int guia) {
  (void)guia;
  ilha_ancorar(NV_TELA_W - NV_ILHA_MARGEM_D / gfx_escala_txt(), NV_ILHA_Y / gfx_escala_txt(), 1);
}

// COM SINAL: quem renova a atividade chama SDL_GetTicks DEPOIS de o quadro ter
// pegado o seu `agora` (app.c pega no comeco do quadro), entao atvVisto pode
// estar 1-2 ms A FRENTE. Sem sinal, `agora - atvVisto` dava ~4 bilhoes e a
// atividade "morria" no quadro em que o milissegundo virava — a pilula
// alternava entre a atividade e o relogio (visto na captura do "Sincronizando
// a conta…", 02/10).
static int atividadeViva(Uint32 agora) {
  return atvVisto && atvTexto[0] && (Sint32)(agora - atvVisto) < 400;
}

int ilha_ocupada(void) { return temCur || atividadeViva(SDL_GetTicks()); }

// MOLA SUBAMORTECIDA (zeta 0,68): passa um pouco do alvo e volta, que e o
// "pulo" da Dynamic Island. As molas de anim.h sao criticas de proposito (sem
// repique) — aqui o repique E o efeito, e so na forma, nunca no texto.
// w mais baixo = mais devagar. Pilula ~0,45 s ate assentar; modal ~0,7 s
// (o dono achou 15 rad/s rapido demais: "ta abrindo muito rapido a ilha").
#define ILHA_MOLA_W   MOV_PILULA_W
#define ILHA_MOLA_Z   MOV_PILULA_Z
#define MODAL_MOLA_W  MOV_MODAL_W
#define MODAL_MOLA_Z  MOV_MODAL_Z
// A mola mora em movimento.h (a Central e o painel Social andam nela tambem).
static float molaIlhaWZ(float *v, float x, float alvo, float dt, float w, float z) {
  return mov_mola(v, x, alvo, dt, w, z);
}
static float molaIlha(float *v, float x, float alvo, float dt) {
  return molaIlhaWZ(v, x, alvo, dt, ILHA_MOLA_W, ILHA_MOLA_Z);
}

// A COR DO TIPO, que pinta o icone e a luz do canto. INFO e ACENTO saiam
// iguais (mapa.md, secao 10: "a diferenca existe so no nome"). Agora, como no
// mockup aprovado: INFO e NEUTRO (icone branco, luz branca, a mesma do relogio
// — e um estado, nao uma novidade), ACENTO leva a cor de destaque, OK o verde e
// ERRO o vermelho. Acento so para estado que pede olhar.
static void corDoTipo(int tipo, float *r, float *g, float *b) {
  if (tipo == ILHA_ERRO) { *r = 1.0f; *g = 0.45f; *b = 0.42f; return; }
  if (tipo == ILHA_OK)   { *r = 0.40f; *g = 0.86f; *b = 0.56f; return; }
  if (tipo == ILHA_INFO) { *r = 0.95f; *g = 0.95f; *b = 0.94f; return; }
  ajustes_acento(r, g, b);
}

static const char *iconeDo(const Aviso *a) {
  if (a->icone[0]) return a->icone;
  if (a->tipo == ILHA_OK) return "check";
  if (a->tipo == ILHA_ERRO) return "aj_triangle-alert";
  return "sino";
}

// O minuto troca SECO (ver o topo): sem guardar o anterior, sem animacao.
static void atualizarHora(void) {
  time_t t = time(NULL);
  struct tm lt;
  if (t == horaSeg) return;
  horaSeg = t;
  if (!localtime_r(&t, &lt)) return;
  hora_tela(hora, sizeof hora, &lt);
}

#define PAD_E   22.0f
#define PAD_D   22.0f   // A27: island pad 22/22
#define ICONE   28.0f
#define VAO     12.0f
#define TECLA   38.0f
#define ROSTO   36.0f
#define MINI_W  30.0f
#define MINI_H  44.0f
#define DUO_SOBRE 8.0f          // a capa entra 8 px por cima do rosto
#define PONTO_VIVO 10.0f

// Largura da pilula em repouso, para o menu do topo reservar (TOPO_RELOGIO):
// a hora + o ponto de enquete + o MEDIDOR DE DESEMPENHO quando ligado — sem
// ele a barra do topo cobre a ilha que cresceu ~200 px a esquerda. Em Menor/
// Grande o corpo por baixo pode ser mais largo que a linha (Grande: GR_W).
float ilha_relogio_largura(void) {
  int ds = desempenho_forma();
  float w, bw, bh;
  atualizarHora();
  w = PAD_E + PAD_D + (float)txt_largura(TXT_PG_RELOGIO, hora) +
      (pontoEnquete ? 12.0f + PONTO_ENQ : 0.0f) + desempenho_linha_w(ds);
  desempenho_corpo_tam(ds, &bw, &bh);
  if (bw > w) w = bw;
  return w * gfx_escala_ui();
}

// --- a frase com enfase --------------------------------------------------------
// "Ana recomendou Fallout": o nome e o titulo em negrito (TXT_ILHA_FORTE, o
// mesmo corpo do TXT_BODY em Bold), o resto em Medium. Uma textura por trecho,
// lado a lado; o cache de text.c guarda cada uma. O teto de NV_ILHA_TEXTO_MAX
// vale para a frase inteira: o trecho que estoura e cortado com reticencias e
// o que vem depois nao entra.
//
// O ESPACO NA EMENDA: a textura de um trecho nao leva o espaco da ponta ("Você
// e " sai do rasterizador como "Você e"), entao o espaco que separa um trecho
// do outro e medido a parte (a largura de "a a" menos a de "aa") e somado aqui.
#define FRASE_TRECHOS 8
typedef struct { TxtLinha l[FRASE_TRECHOS]; float x[FRASE_TRECHOS]; int n; float w, h; } Frase;
static float larguraEspaco(void) {
  static float e = -1.0f;
  if (e < 0.0f) {
    e = (float)(txt_largura(TXT_BODY, "a a") - txt_largura(TXT_BODY, "aa"));
    if (e <= 0.0f) e = 7.0f;
  }
  return e;
}
static void fraseMontar(Frase *f, const char *s, float maxW) {
  const char *p = s;
  int forte = 0;
  float vao = 0.0f;
  f->n = 0; f->w = 0.0f; f->h = 0.0f;
  while (p && *p && f->n < FRASE_TRECHOS) {
    const char *q = strchr(p, ILHA_FORTE[0]);
    size_t len = q ? (size_t)(q - p) : strlen(p);
    const char *ini = p;
    if (len && *ini == ' ') { vao = larguraEspaco(); while (len && *ini == ' ') { ini++; len--; } }
    if (len) {
      char seg[240];
      TxtEstilo es = forte ? TXT_ILHA_FORTE : TXT_BODY;
      int fimEsp = 0;
      float resta;
      TxtLinha l;
      while (len && ini[len - 1] == ' ') { len--; fimEsp = 1; }
      if (len >= sizeof seg) len = sizeof seg - 1;
      memcpy(seg, ini, len); seg[len] = 0;
      if (f->n == 0) vao = 0.0f;
      resta = maxW - f->w - vao;
      if (resta < 8.0f) break;
      l = txt_linha(es, seg, 240, 242, 246, 255);
      if ((float)l.w > resta) l = txt_linha_corta(es, seg, 240, 242, 246, 255, resta);
      f->x[f->n] = f->w + vao;
      f->l[f->n++] = l;
      f->w += vao + (float)l.w;
      if ((float)l.h > f->h) f->h = (float)l.h;
      vao = fimEsp ? larguraEspaco() : 0.0f;
      if (f->w >= maxW - 1.0f) break;
    }
    if (!q) break;
    forte = !forte;
    p = q + 1;
  }
}
static void fraseDesenhar(const Frase *f, float x, float yc, float a) {
  int i;
  for (i = 0; i < f->n; i++)
    txt_desenhar_alpha(f->l[i], x + f->x[i], yc - (float)f->l[i].h * 0.5f, a);
}

// O ROSTO de 36 (ou o do modal): a foto, ou a inicial num disco com a cor da
// pessoa — rec_avatar, o MESMO desenho da aba Social, para a mesma pessoa ter
// a mesma cor nas duas superficies.
static void rostoEm(GfxRect r, const char *url, const char *nome, float a) {
  // A letra acompanha o disco: 15 Bold no de 28-36, 28 no de 64, 48 Bold no
  // de 150 (o mockup: 13-16/700, 26/700, 60/700).
  int es = r.w >= 100.0f ? TXT_TITULO3 : r.w >= 48.0f ? TXT_CALLOUT : TXT_MINI;
  rec_avatar_estilo(r, url, nome && nome[0] ? nome : "?", nome, a, es);
}
static void capaEm(GfxRect r, const char *url, float a) {
  // "-" = lugar de capa sem url (o duo do cartao do amigo): so o esqueleto.
  GLuint tex = url && url[0] && strcmp(url, "-") ? tex_obter_larg(url, r.w) : 0;
  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(url);
    gfx_rect(r, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, 6.0f / r.h, 0, 0, 0, a);
    gfx_tex_aspect_atual = 0.0f;
  } else gfx_cor(r, 6.0f / r.h, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
}
static int temRosto(const Aviso *v) { return v->rosto[0] || v->rostoNome[0]; }
static float larguraLead(const Aviso *v) {
  if (temRosto(v) && v->capa[0]) return ROSTO + MINI_W - DUO_SOBRE;
  if (temRosto(v)) return ROSTO;
  if (v->capa[0]) return MINI_W;
  return ICONE;
}
// O "duo" (rosto + capa) e o rosto sozinho; a capa sozinha; ou o icone.
static void desenharLead(const Aviso *v, float x, float yc, float a) {
  float cr, cg, cb;
  if (temRosto(v)) {
    GfxRect ro = { x, yc - ROSTO * 0.5f, ROSTO, ROSTO };
    if (v->capa[0]) {
      // O anel escuro em volta do rosto separa os dois onde a capa encosta.
      gfx_rect((GfxRect){ ro.x - 3.0f, ro.y - 3.0f, ROSTO + 6.0f, ROSTO + 6.0f }, 0, GFX_DISCO,
               0, 0, 0, 0, 0.055f, 0.058f, 0.068f, 0.9f * a);
    }
    rostoEm(ro, v->rosto, v->rostoNome, a);
    if (v->capa[0]) capaEm((GfxRect){ x + ROSTO - DUO_SOBRE, yc - MINI_H * 0.5f, MINI_W, MINI_H }, v->capa, a);
    return;
  }
  if (v->capa[0]) { capaEm((GfxRect){ x, yc - MINI_H * 0.5f, MINI_W, MINI_H }, v->capa, a); return; }
  corDoTipo(v->tipo, &cr, &cg, &cb);
  // A capa que voa (ilha_salvar) pousa AQUI: o icone nasce por baixo dela.
  if (!strcmp(v->chave, SALVAR_CHAVE)) a *= svooIconeA;
  gfx_icone((GfxRect){ x, yc - ICONE * 0.5f, ICONE, ICONE }, iconeDo(v), cr, cg, cb,
            v->tipo == ILHA_INFO ? a * 0.86f : a);
}

// --- o aviso v2 (duas linhas / com marca) e a atividade v2 ------------------------
// Mockup ajustes-v2 (03/10), quadros v2-upd-aviso, v2-upd-em-dia, v2-upd-ilha e
// v2-upd-procurando. Os recuos sao os do mockup, e nao PAD_E/PAD_D: a largura
// devolvida ja desconta a diferenca, para alvoW = PAD_E + PAD_D + largura dar a
// pilula certa e o desenho centrar o bloco inteiro (recuos inclusos).
#define V2_AV_H   84.0f
#define V2_ATV_H  72.0f
#define V2_BARRA 180.0f
static int avisoV2(const Aviso *v) { return v->titulo[0] || v->kicker[0]; }
static float v2PadE(const Aviso *v) { return v->titulo[0] ? 16.0f : 22.0f; }
#define V2_PAD_D 30.0f
// Verde e vermelho do mockup v2 (#4cc38a, #e5534b) para o disco e a marca.
static void corV2(int tipo, float *r, float *g, float *b) {
  if (tipo == ILHA_OK)   { *r = 0.298f; *g = 0.765f; *b = 0.541f; return; }
  if (tipo == ILHA_ERRO) { *r = 0.898f; *g = 0.325f; *b = 0.294f; return; }
  corDoTipo(tipo, r, g, b);
}
static float kickerV2(const char *s, float x, float y, float cr, float cg, float cb, float a) {
  char up[96];
  idioma_maiusc(up, sizeof up, s);
  return txt_tracking(TXT_AJ_CAPS13, up, (int)(cr * 255.0f), (int)(cg * 255.0f), (int)(cb * 255.0f),
                      x < 0.0f ? -10000.0f : x, y, x < 0.0f ? 0.0f : a, 2.0f);
}
typedef struct { TxtLinha t1, t2, dica; float kw; } LinhasV2;
static float larguraAvisoV2(const Aviso *v, LinhasV2 *L) {
  float disco = v->titulo[0] ? 56.0f : 40.0f, vao = v->titulo[0] ? 18.0f : 16.0f, col, w;
  memset(L, 0, sizeof *L);
  if (v->titulo[0]) {
    L->t1 = txt_linha_corta(TXT_G28B, v->titulo, 243, 242, 239, 255, NV_ILHA_TEXTO_MAX);
    L->t2 = txt_linha_corta(TXT_AJ_TEXTO, v->texto, 243, 242, 239, 255, NV_ILHA_TEXTO_MAX);
    col = (float)(L->t1.w > L->t2.w ? L->t1.w : L->t2.w);
  } else {
    float cr, cg, cb;
    corV2(v->tipo, &cr, &cg, &cb);
    L->kw = kickerV2(v->kicker, -1.0f, 0.0f, cr, cg, cb, 0.0f);
    L->t2 = txt_linha_corta(TXT_G28B, v->texto, 243, 242, 239, 255, NV_ILHA_TEXTO_MAX);
    col = (float)L->t2.w > L->kw ? (float)L->t2.w : L->kw;
  }
  w = disco + vao + col;
  if (v->tecla) {
    L->dica = txt_linha(TXT_AJ_TEXTO, v->dica[0] ? v->dica : i18n("abre"), 243, 242, 239, 255);
    w += 22.0f + TECLA + 10.0f + (float)L->dica.w;
  }
  return w + (v2PadE(v) - PAD_E) + (V2_PAD_D - PAD_D);
}
static void desenharAvisoV2(const Aviso *v, GfxRect r, float a) {
  LinhasV2 L;
  float pe = v2PadE(v), tot = larguraAvisoV2(v, &L) + PAD_E + PAD_D;
  float x = r.x + (r.w - tot) * 0.5f + pe, yc = r.y + r.h * 0.5f, cr, cg, cb;
  float disco = v->titulo[0] ? 56.0f : 40.0f, ic = v->titulo[0] ? 26.0f : 22.0f;
  corV2(v->tipo, &cr, &cg, &cb);
  gfx_cor((GfxRect){ x, yc - disco * 0.5f, disco, disco }, 0.5f, cr, cg, cb,
          (v->tipo == ILHA_OK || v->tipo == ILHA_ERRO ? 0.18f : 0.22f) * a);
  gfx_icone((GfxRect){ x + (disco - ic) * 0.5f, yc - ic * 0.5f, ic, ic }, iconeDo(v), cr, cg, cb, a);
  x += disco + (v->titulo[0] ? 18.0f : 16.0f);
  if (v->titulo[0]) {
    txt_desenhar_alpha(L.t1, x, yc - 13.0f - (float)L.t1.h * 0.5f, a);
    txt_desenhar_alpha(L.t2, x, yc + 17.0f - (float)L.t2.h * 0.5f, a * 0.62f);
    x += (float)(L.t1.w > L.t2.w ? L.t1.w : L.t2.w);
  } else {
    TxtLinha k = txt_linha(TXT_AJ_CAPS13, "M", 255, 255, 255, 255);
    kickerV2(v->kicker, x, yc - 16.0f - (float)k.h * 0.5f, cr, cg, cb, a);
    txt_desenhar_alpha(L.t2, x, yc + 8.5f - (float)L.t2.h * 0.5f, a);
    x += (float)L.t2.w > L.kw ? (float)L.t2.w : L.kw;
  }
  if (v->tecla) {
    x += 22.0f;
    sintro_tecla_atalho(x, yc - TECLA * 0.5f, TECLA, a);
    txt_desenhar_alpha(L.dica, x + TECLA + 10.0f, yc - (float)L.dica.h * 0.5f, a * 0.60f);
  }
}

typedef struct { Frase f; TxtLinha meta, mais, abre; int nMais; } LinhasAviso;
// Largura do aviso (sem o recuo) e as linhas dele. `nMais` = quantos esperam.
static float larguraAviso(const Aviso *v, int nMais, LinhasAviso *L) {
  float w = larguraLead(v) + VAO;
  if (avisoV2(v)) { LinhasV2 L2; memset(L, 0, sizeof *L); return larguraAvisoV2(v, &L2); }
  memset(&L->meta, 0, sizeof L->meta); memset(&L->mais, 0, sizeof L->mais);
  memset(&L->abre, 0, sizeof L->abre);
  fraseMontar(&L->f, v->texto, NV_ILHA_TEXTO_MAX);
  w += L->f.w;
  if (v->meta[0]) {
    L->meta = txt_linha(TXT_CAPTION2, v->meta, 176, 180, 190, 255);
    w += VAO + (float)L->meta.w;
  }
  if (v->vivo) w += VAO + PONTO_VIVO;
  // "+N": SO COM DOIS OU MAIS esperando (mockup: "com mais de um esperando,
  // aparece +N"). Um so esperando entra logo depois e nao precisa de anuncio.
  L->nMais = nMais >= 2 ? nMais : 0;
  if (L->nMais) {
    char b[12];
    snprintf(b, sizeof b, "+%d", L->nMais);
    L->mais = txt_linha(TXT_MINI, b, 206, 206, 203, 255);
    w += 16.0f + ((float)L->mais.w + 16.0f < 30.0f ? 30.0f : (float)L->mais.w + 16.0f);
  }
  if (v->tecla) {
    L->abre = txt_linha(TXT_CAPTION2, i18n("abre"), 176, 180, 190, 255);
    w += 18.0f + TECLA + 8.0f + (float)L->abre.w;
  }
  return w;
}

static void desenharAviso(const Aviso *v, GfxRect r, float a) {
  LinhasAviso L;
  float cw;
  if (avisoV2(v)) { desenharAvisoV2(v, r, a); return; }
  cw = larguraAviso(v, esperando(), &L);
  float x = r.x + (r.w - cw) * 0.5f, yc = r.y + r.h * 0.5f;
  desenharLead(v, x, yc, a);
  x += larguraLead(v) + VAO;
  fraseDesenhar(&L.f, x, yc, a);
  x += L.f.w;
  if (v->meta[0]) {
    x += VAO;
    txt_desenhar_alpha(L.meta, x, yc - (float)L.meta.h * 0.5f, a);
    x += (float)L.meta.w;
  }
  if (v->vivo) {
    x += VAO;
    gfx_cor((GfxRect){ x, yc - PONTO_VIVO * 0.5f, PONTO_VIVO, PONTO_VIVO }, 0.5f, 1.0f, 0.353f, 0.322f, a);
    x += PONTO_VIVO;
  }
  if (L.nMais) {
    float cwM = (float)L.mais.w + 16.0f < 30.0f ? 30.0f : (float)L.mais.w + 16.0f;
    x += 16.0f;
    gfx_cor((GfxRect){ x, yc - 13.0f, cwM, 26.0f }, 0.5f, 1.0f, 1.0f, 1.0f, 0.12f * a);
    txt_desenhar_alpha(L.mais, x + (cwM - (float)L.mais.w) * 0.5f, yc - (float)L.mais.h * 0.5f, a);
    x += cwM;
  }
  if (v->tecla) {
    x += 18.0f;
    sintro_tecla_atalho(x, yc - TECLA * 0.5f, TECLA, a);
    txt_desenhar_alpha(L.abre, x + TECLA + 8.0f, yc - (float)L.abre.h * 0.5f, a);
  }
}

// Largura do relogio e da atividade (sem o recuo), e as linhas deles.
static float larguraConteudo(int m, TxtLinha *t1, TxtLinha *t2) {
  float w;
  t1->w = t1->h = 0; t2->w = t2->h = 0;
  if (m == M_RELOGIO) {
    *t1 = txt_linha(TXT_PG_RELOGIO, hora, 244, 245, 248, 255);
    return (float)t1->w + (pontoEnquete ? 12.0f + PONTO_ENQ : 0.0f);
  }
  if (atvV2) {
    // v2: recuo de 30 dos dois lados (a diferenca para PAD_E/PAD_D entra aqui).
    float extra = (30.0f - PAD_E) + (30.0f - PAD_D);
    if (!atvIcone[0]) {
      *t1 = txt_linha_corta(TXT_G28B, atvTexto, 243, 242, 239, 255, NV_ILHA_TEXTO_MAX);
      return 12.0f + 16.0f + (float)t1->w + extra;
    }
    *t1 = txt_linha_corta(TXT_G26B, atvTexto, 243, 242, 239, 255, NV_ILHA_TEXTO_MAX);
    w = 24.0f + 18.0f + (float)t1->w;
    if (atvProg >= 0.0f) {
      char n[8];
      snprintf(n, sizeof n, "%d%%", (int)(atvProg * 100.0f + 0.5f));
      *t2 = txt_linha(TXT_ILHA_CORPO, n, 243, 242, 239, 255);
      w += 18.0f + V2_BARRA + 18.0f + (float)t2->w;
    }
    return w + extra;
  }
  *t1 = txt_linha_corta(TXT_BODY, atvTexto, 236, 238, 244, 255, NV_ILHA_TEXTO_MAX);
  w = 12.0f + VAO + (float)t1->w;
  if (atvProg >= 0.0f) {
    char n[8];
    snprintf(n, sizeof n, "%d%%", (int)(atvProg * 100.0f + 0.5f));
    *t2 = txt_linha(TXT_CAPTION2, n, 176, 180, 190, 255);
    w += 14.0f + (float)t2->w;
  }
  return w;
}

static void desenharConteudo(int m, GfxRect r, float a, Uint32 agora) {
  TxtLinha t1, t2;
  float cw, x, yc = r.y + r.h * 0.5f;
  if (a < 0.01f) return;
  if (m == M_AVISO) { desenharAviso(&mostraA, r, a); return; }
  cw = larguraConteudo(m, &t1, &t2);
  // Centrado na pilula: durante a mola o conteudo nao fica grudado num lado.
  x = r.x + (r.w - cw) * 0.5f;
  if (m == M_RELOGIO) {
    // Com o medidor: a hora e o trecho dele na mesma linha; em Menor/Grande a
    // linha sobe para o topo da ilha crescida e o corpo vai embaixo.
    float dw = desempenho_linha_w(dsMostra), bw, bh;
    desempenho_corpo_tam(dsMostra, &bw, &bh);
    if (bh > 0.0f) yc = r.y + NV_ILHA_H * 0.5f;
    x = dsMostra == DS_GRANDE ? r.x + 28.0f : r.x + (r.w - cw - dw) * 0.5f;
    txt_desenhar_alpha(t1, x, yc - (float)t1.h * 0.5f, a);
    if (pontoEnquete) {
      float cr, cg, cb;
      ajustes_acento(&cr, &cg, &cb);
      gfx_cor((GfxRect){ x + (float)t1.w + 12.0f, yc - PONTO_ENQ * 0.5f, PONTO_ENQ, PONTO_ENQ }, 0.5f, cr, cg, cb, a);
    }
    desempenho_linha(dsMostra, x + (float)t1.w + (pontoEnquete ? 12.0f + PONTO_ENQ : 0.0f), yc, a);
    if (bh > 0.0f) desempenho_corpo(dsMostra, (GfxRect){ r.x, r.y + NV_ILHA_H, r.w, r.h - NV_ILHA_H }, a);
    return;
  }
  if (atvV2) {
    float cr, cg, cb;
    x += ((30.0f - PAD_E) + (30.0f - PAD_D)) * 0.5f;   // o bloco medido inclui os recuos v2
    ajustes_acento(&cr, &cg, &cb);
    if (!atvIcone[0]) {
      plrui_respira(x + 6.0f, yc, 12.0f, agora, a);
      txt_desenhar_alpha(t1, x + 28.0f, yc - (float)t1.h * 0.5f, a);
      return;
    }
    gfx_icone((GfxRect){ x, yc - 12.0f, 24.0f, 24.0f }, atvIcone, cr, cg, cb, a);
    x += 24.0f + 18.0f;
    txt_desenhar_alpha(t1, x, yc - (float)t1.h * 0.5f, a);
    x += (float)t1.w;
    if (atvProg >= 0.0f) {
      x += 18.0f;
      plrui_trilho((GfxRect){ x, yc - 3.0f, V2_BARRA, 6.0f }, atvProg, -1.0f, 0, 0, a);
      x += V2_BARRA + 18.0f;
      txt_desenhar_alpha(t2, x, yc - (float)t2.h * 0.5f, a * 0.70f);
    }
    return;
  }
  { float cr, cg, cb, p;
    ajustes_acento(&cr, &cg, &cb);
    // Ponto que respira: "esta acontecendo", sem girar nada. O halo fraco em
    // volta e o do mockup (o mesmo acento a 22%), num disco so.
    p = ajustes_animacoes_reduzidas() ? 1.0f
        : 0.55f + 0.45f * sinf((float)agora * (2.0f * 3.14159265f / 1200.0f));
    gfx_rect((GfxRect){ x - 5.0f, yc - 11.0f, 22.0f, 22.0f }, 0, GFX_DISCO, 0, 0, 0, 0, cr, cg, cb, 0.22f * a * p);
    gfx_cor((GfxRect){ x, yc - 6.0f, 12.0f, 12.0f }, 0.5f, cr, cg, cb, a * p);
    x += 12.0f + VAO;
    txt_desenhar_alpha(t1, x, yc - (float)t1.h * 0.5f, a);
    if (atvProg >= 0.0f) {
      float pr = atvProg > 1.0f ? 1.0f : atvProg;
      GfxRect trilho = { r.x + PAD_E, r.y + r.h - 11.0f, r.w - PAD_E - PAD_D, 3.0f };
      txt_desenhar_alpha(t2, x + (float)t1.w + 14.0f, yc - (float)t2.h * 0.5f, a);
      if (ajustes_vidro()) gfx_vidro_chip(trilho, 0.5f, 0.55f, a);
      else gfx_cor(trilho, 0.5f, 1.0f, 1.0f, 1.0f, 0.12f * a);
      if (trilho.w * pr > 3.0f)
        gfx_cor((GfxRect){ trilho.x, trilho.y, trilho.w * pr, 3.0f }, 0.5f, cr, cg, cb, a);
    } }
}

// --- o cartao na pilula --------------------------------------------------------
#define CT_TIT_MAX 300.0f
#define CT_CAPA_W   30.0f
#define CT_CAPA_H   44.0f
#define CT_VAO      14.0f
typedef struct { TxtLinha hora, tit, meta; float ds; } LinhasCartao;

// "T1E3 · 32 min restantes" / "T2E5 · hoje". O FORMATO passa por i18n inteiro
// quando existe; na estreia sao duas partes ja traduzidas juntadas por " · ",
// que nao e palavra.
static void metaCartao(const IlhaCartao *c, int qual, char *b, size_t n) {
  if (qual == ILHA_AMIGO) { snprintf(b, n, i18n("%s · agora"), c->titulo); return; }
  if (qual == ILHA_ESTREIA) {
    char te[32] = "";
    if (c->serie && c->t > 0 && c->e > 0) snprintf(te, sizeof te, i18n("T%dE%d"), c->t, c->e);
    if (te[0] && c->quando[0]) snprintf(b, n, "%s · %s", te, c->quando);
    else snprintf(b, n, "%s", te[0] ? te : c->quando);
    return;
  }
  { int m = c->restanteMin < 1 ? 1 : c->restanteMin;
    if (c->serie && c->t > 0 && c->e > 0) snprintf(b, n, i18n("T%dE%d · %d min restantes"), c->t, c->e, m);
    else snprintf(b, n, i18n("%d min restantes"), m); }
}

static float larguraCartao(const IlhaCartao *c, int qual, LinhasCartao *L) {
  char meta[96];
  float w;
  L->hora = txt_linha(TXT_PG_RELOGIO, hora, 244, 245, 248, 255);
  // No cartao do amigo o nome vai em destaque e o titulo vira a meta.
  L->tit = qual == ILHA_AMIGO
         ? txt_linha_corta(TXT_ILHA_FORTE, c->pessoa, 240, 242, 246, 255, CT_TIT_MAX)
         : txt_linha_corta(TXT_BODY, c->titulo, 240, 242, 246, 255, CT_TIT_MAX);
  metaCartao(c, qual, meta, sizeof meta);
  L->meta = txt_linha_corta(TXT_CAPTION2, meta, 176, 180, 190, 255, CT_TIT_MAX);
  w = (float)L->hora.w + CT_VAO * 2.0f + 1.5f +
      (qual == ILHA_VIVO ? CT_CAPA_W : qual == ILHA_AMIGO ? ROSTO + MINI_W - DUO_SOBRE : ICONE) + VAO +
      (float)L->tit.w + 10.0f + (float)L->meta.w;
  if (qual != ILHA_VIVO) w += 12.0f + 10.0f;   // o ponto de nao lido / de "agora"
  // O medidor em Minimo depois do cartao, so se a pilula ainda couber.
  L->ds = dsCartao ? desempenho_linha_w(DS_MINIMO) : 0.0f;
  if (L->ds > 0.0f && PAD_E + PAD_D + w + L->ds > DS_CARTAO_MAX) L->ds = 0.0f;
  return w + L->ds;
}

// Onde a mini capa fica numa pilula de retangulo r: o mesmo passo a passo de
// desenharCartao, e o alvo do voo (ilha_minimizar).
static GfxRect capaNaPilula(const IlhaCartao *c, GfxRect r) {
  LinhasCartao L;
  float cw = larguraCartao(c, ILHA_VIVO, &L);
  float x = r.x + (r.w - cw) * 0.5f + (float)L.hora.w + CT_VAO + 1.5f + CT_VAO;
  return (GfxRect){ x, r.y + r.h * 0.5f - CT_CAPA_H * 0.5f, CT_CAPA_W, CT_CAPA_H };
}

static void desenharCartao(const IlhaCartao *c, int qual, GfxRect r, float a) {
  LinhasCartao L;
  float cw = larguraCartao(c, qual, &L);
  float x = r.x + (r.w - cw) * 0.5f, yc = r.y + r.h * 0.5f, cr, cg, cb, xTexto;
  int barra = qual == ILHA_VIVO && c->progresso >= 0.0f;
  float yt = barra ? yc - 3.0f : yc;
  if (a < 0.01f) return;
  ajustes_acento(&cr, &cg, &cb);
  txt_desenhar_alpha(L.hora, x, yc - (float)L.hora.h * 0.5f, a);
  x += (float)L.hora.w + CT_VAO;
  // Fio entre o relogio e o cartao: e a mesma pilula, com duas coisas dentro.
  gfx_cor((GfxRect){ x, yc - 13.0f, 1.5f, 26.0f }, 0.0f, 1.0f, 1.0f, 1.0f, 0.22f * a);
  x += 1.5f + CT_VAO;
  if (qual == ILHA_VIVO) {
    GfxRect capa = { x, yc - CT_CAPA_H * 0.5f, CT_CAPA_W, CT_CAPA_H };
    GLuint tex = c->poster[0] ? tex_obter_larg(c->poster, CT_CAPA_W) : 0;
    if (voo) {
      // Em voo a capa e o quadro que esta pousando: desenhar as duas dobraria.
    } else if (tex) {
      // COVER FORCADO: cartaz de addon quadrado ou deitado tambem preenche a
      // mini capa (recorta), nunca fica com faixas dentro dela.
      gfx_tex_aspect_atual = tex_aspecto(c->poster);
      gfx_card_forcar_cover_atual = 1.0f;
      gfx_rect(capa, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, 6.0f / CT_CAPA_H, 0, 0, 0, a);
      gfx_card_forcar_cover_atual = 0.0f;
      gfx_tex_aspect_atual = 0.0f;
    } else gfx_cor(capa, 6.0f / CT_CAPA_H, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
    x += CT_CAPA_W + VAO;
  } else if (qual == ILHA_AMIGO) {
    Aviso duo;
    memset(&duo, 0, sizeof duo);
    snprintf(duo.rosto, sizeof duo.rosto, "%s", c->rosto);
    snprintf(duo.rostoNome, sizeof duo.rostoNome, "%s", c->pessoa[0] ? c->pessoa : "?");
    snprintf(duo.capa, sizeof duo.capa, "%s", c->poster[0] ? c->poster : "-");
    desenharLead(&duo, x, yc, a);
    x += ROSTO + MINI_W - DUO_SOBRE + VAO;
  } else {
    gfx_icone((GfxRect){ x, yc - ICONE * 0.5f, ICONE, ICONE }, "aj_calendar", cr, cg, cb, a);
    x += ICONE + VAO;
  }
  xTexto = x;
  txt_desenhar_alpha(L.tit, x, yt - (float)L.tit.h * 0.5f, a);
  x += (float)L.tit.w + 10.0f;
  txt_desenhar_alpha(L.meta, x, yt - (float)L.meta.h * 0.5f, a);
  x += (float)L.meta.w;
  if (qual == ILHA_ESTREIA)
    gfx_cor((GfxRect){ x + 12.0f, yc - 5.0f, 10.0f, 10.0f }, 0.5f, cr, cg, cb, a);
  else if (qual == ILHA_AMIGO)   // vermelho de "ao vivo", como no aviso
    gfx_cor((GfxRect){ x + 12.0f, yc - 5.0f, 10.0f, 10.0f }, 0.5f, 1.0f, 0.353f, 0.322f, a);
  if (L.ds > 0.0f) desempenho_linha(DS_MINIMO, x + (qual != ILHA_VIVO ? 22.0f : 0.0f), yc, a);
  if (barra) {
    // A BARRA FINA vai sob o texto (nao sob a capa): ela mede o titulo.
    float pr = c->progresso > 1.0f ? 1.0f : c->progresso;
    GfxRect trilho = { xTexto, r.y + r.h - 12.0f, x - xTexto, 3.0f };
    if (ajustes_vidro()) gfx_vidro_chip(trilho, 0.5f, 0.55f, a);
    else gfx_cor(trilho, 0.5f, 1.0f, 1.0f, 1.0f, 0.14f * a);
    if (trilho.w * pr > 3.0f)
      gfx_cor((GfxRect){ trilho.x, trilho.y, trilho.w * pr, 3.0f }, 0.5f, cr, cg, cb, a);
  }
}

// --- o modal -----------------------------------------------------------------------
// Um cartao de 1120 x 414: a arte do episodio (16:9) a esquerda, logo ou
// titulo, T/E e nome, sinopse curta e o tempo a direita, os botoes embaixo.
// Mesmo canto da pilula: ancorada a esquerda cresce para a direita e para
// baixo, ancorada a direita cresce para a esquerda.
#define MD_W      1120.0f
#define MD_H       414.0f
#define MD_PAD      32.0f
#define MC_W       880.0f   // modal com cabecalho (layoutModalCabecalho)
#define MD_ARTE_W  480.0f
#define MD_ARTE_H  270.0f
#define MD_RAIO     30.0f
#define MD_LOGO_W  380.0f
#define MD_LOGO_H   80.0f
#define AT_W       640.0f   // painel da atividade (Home loading)
#define AT_H       292.0f

static float modalSinopseExtra(void) {
  return !modalAviso && !modalAtividade && modalC.sinopse[0]
       ? fmaxf(0, sinopse_entrelinha(TXT_CAPTION, 30) - 30) : 0;
}

static GfxRect modalAlvo(GfxRect p, int dir) {
  int compacto = modalAtividade && atvTemCarga;
  float mw = compacto ? AT_W : modalAviso && modalM.cabecalho ? MC_W : MD_W;
  GfxRect m = { dir ? p.x + p.w - mw : p.x, p.y, mw, compacto ? AT_H : modalAviso ? modalAvisoH : MD_H };
  if (!telefoneui_ativo()) m.h += modalSinopseExtra();
  if (telefoneui_ativo()) {
    m.w = telefoneui_largura(mw, NV_TELA_W, 40);
    m.h = fminf(m.h, NV_TELA_H - 80);
    m.x = (NV_TELA_W - m.w) * .5f;
    m.y = fmaxf(40, fminf(m.y, NV_TELA_H - 40 - m.h));
    return m;
  }
  if (m.x + m.w > NV_TELA_W - 40.0f) m.x = NV_TELA_W - 40.0f - m.w;
  if (m.x < 40.0f) m.x = 40.0f;
  if (modalSinopseExtra() > 0) m.y = fmaxf(40, fminf(m.y, NV_TELA_H - 40 - m.h));
  return m;
}

// --- o modal de AVISO (generico) -------------------------------------------------
// A MESMA superficie do modal dos cartoes (1120 de largura, raio 30, recuo 32,
// botoes de 56 embaixo), com o lado esquerdo trocado conforme o assunto: a
// arte 480x270 (recomendacao: a capa do titulo, com o rosto de quem mandou no
// canto), o ROSTO de 150 (pedido de amizade) ou um LADRILHO de 150 com o icone
// (versao nova, Trakt, aviso do dono). A ALTURA SAI DO CONTEUDO: o modal do
// pedido de amizade nao tem por que ter os 414 do cartao com arte. E medida uma
// vez por abertura (desenha = 0) antes da mola, para a pilula crescer direto
// para o tamanho certo.
#define MG_LADO   150.0f
static float arteLadoW(const char *url);
#ifdef NV_TOUCH_UI
/* Media and text share a scrollable body; the real action buttons stay
 * inside its bounded footer. The original TV renderer remains below. */
static float modalTelefone(GfxRect m, const IlhaModal *c, float a, int desenha, int ehSinopse) {
  float w = m.w - 64, x = m.x + 32, footer = 92, bx = 0, by = 0;
  GfxRect buttons[ILHA_MODAL_BOTOES];
  int n = nBotoes(), salvos = modalTemSalvos();
  for (int i = 0; i < n; i++) {
    float bw = fminf(w, plrui_botao_largura(rotuloBotao(i), iconeBotao(i)));
    if (bx > 0 && bx + bw > w) { bx = 0; by += 72; }
    buttons[i] = (GfxRect){x + bx, 0, bw, 60};
    buttons[i].y = by; bx += bw + 12;
  }
  footer += by + (salvos || c->rodape[0] ? 48 : 0);
  float area = fmaxf(60, m.h - footer - 32), y = m.y + 32;
  if (desenha) {
    float natural = modalTelefone(m, c, a, 0, ehSinopse);
    toqueModalY = toquerol_clamp(toqueModalY, 0, fmaxf(0, natural - footer - 32 - area));
  }
  float scroll = desenha ? toqueModalY : 0;
  if (desenha) {
    if (a > .3f) {
      ponteiro_camada();
      ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, pontFora, 0, 0);
      ponteiro_alvo(m.x, m.y, m.w, m.h, NULL, NULL, 0, 0);
    }
    gfx_recorte(x, m.y + 32, w, area);
  }
  y -= scroll;
  float ad = desenha ? a : 0;
  if (c->arte[0]) {
    float aw = fminf(w, arteLadoW(c->arte)), ah = aw / (arteLadoW(c->arte) / MD_ARTE_H);
    GfxRect r = {x, y, aw, ah}; GLuint tex = desenha ? tex_obter_larg(c->arte, aw) : 0;
    if (desenha) {
      gfx_tex_aspect_atual = tex_aspecto(c->arte);
      gfx_rect(r, tex, GFX_CARD, 0, 0, 0, 18 / ah, 0.1f, .1f, .12f, a);
      gfx_tex_aspect_atual = 0;
    }
    y += ah + 24;
  } else if (c->rosto[0] || c->rostoNome[0]) {
    if (desenha) rostoEm((GfxRect){x, y, MG_LADO, MG_LADO}, c->rosto, c->rostoNome, a);
    y += MG_LADO + 24;
  } else if (c->icone[0]) {
    if (desenha) gfx_icone((GfxRect){x, y, 40, 40}, c->icone, 1, 1, 1, a);
    y += 56;
  }
  const char *lines[] = {c->kicker, c->titulo, c->linha, c->nota, c->texto, c->fala};
  for (int i = 0; i < 6; i++) if (lines[i][0]) {
    TxtEstilo e = i == 1 ? TXT_TITULO3 : i == 0 ? TXT_MINI : TXT_CAPTION;
    float lead = i == 1 ? 52 : 30;
    if (ehSinopse && i == 4) { e = sinopse_estilo(e); lead = sinopse_entrelinha(TXT_CAPTION, lead); }
    y += txt_bloco_corta(e, lines[i], 243, 242, 239, x, y, w, lead, ad, 20) + 12;
  }
  for (int i = 0; i < 3 && c->lista[i][0]; i++) {
    y += txt_bloco_corta(TXT_CAPTION, c->lista[i], 243, 242, 239, x, y, w, 30, ad, 8) + 8;
    if (c->resultado) {
      float pct = anim_clamp(c->pct[i] / 100.0f, 0, 1);
      if (desenha) plrui_trilho((GfxRect){x, y, w, 6}, pct, -1, 0, 0, a);
      y += 20;
    }
  }
  for (int i = 0; i < 3 && c->chips[i][0]; i++)
    y += txt_bloco_corta(TXT_CAPTION, c->chips[i], 200, 202, 206, x, y, w, 30, ad, 3) + 8;
  if (!modalAviso && !modalAtividade && modalQual == ILHA_VIVO) {
    if (desenha) plrui_trilho((GfxRect){x, y, w, 6}, anim_clamp(modalC.progresso, 0, 1), -1, 0, 0, a);
    y += 22;
  }
  if (c->estado[0]) y += txt_bloco_corta(TXT_CAPTION, c->estado, 168, 170, 176, x, y, w, 30, ad, 3) + 8;
  float bodyH = y + scroll - (m.y + 32), natural = 32 + bodyH + footer;
  if (!desenha) return natural;
  gfx_sem_recorte();
  toquerol_vincular(&toqueModal, (GfxRect){x, m.y + 32, w, area}, gfx_escala(), 0, bodyH - area, 1, &toqueModalY);
  if (a > .3f) ponteiro_rolagem(modalToqueRolar);
  float footY = m.y + m.h - footer + 16;
  for (int i = 0; i < n; i++) {
    GfxRect r = buttons[i]; r.y += footY;
    if (modalFocoA[i] > .5f) plrui_pilula_foco(r, a); else plrui_botao_repouso(r, a);
    int cr = 243, cg = 242, cb = 239;
    if (modalFocoA[i] > .5f) cr = cg = cb = ajustes_tinta_foco();
    TxtLinha l = txt_linha_corta(TXT_G21B, rotuloBotao(i), cr, cg, cb, 255, r.w - 32);
    txt_desenhar_alpha(l, r.x + 16, r.y + (r.h - l.h) * .5f, a);
    if (a > .3f) ponteiro_alvo(r.x, r.y, r.w, r.h, pontFoco, NULL, i, 0);
  }
  if (salvos || c->rodape[0]) {
    GfxRect r = {x, footY + by + 72, w, 40};
    TxtLinha l = txt_linha_corta(TXT_CAPTION2, salvos ? i18n("Salvos") : c->rodape, 176, 180, 190, 255, w);
    txt_desenhar_alpha(l, r.x, r.y + (40 - l.h) * .5f, a);
    if (salvos && a > .3f) ponteiro_alvo(r.x, r.y, r.w, r.h, NULL, pontSalvos, 0, 0);
  }
  return natural;
}
#endif
// A variante com cabecalho (IlhaModal.cabecalho): a ilha do relogio crescida.
static float layoutModalCabecalho(GfxRect m, float a, int desenha) {
  const IlhaModal *c = &modalM;
  float x = m.x + 36.0f, cw = m.w - 80.0f, y = m.y, th, by;
  float ad = desenha ? a : 0.0f;
  int i;
  th = txt_bloco_corta(TXT_AJ_TEXTO, c->texto, 0, 0, 0, x, -4000, cw, 30.0f, 0.0f, 3);
  if (desenha) {
    float cr, cg, cb, hx = m.x + 28.0f;
    char hora[16];
    time_t tt = time(NULL);
    struct tm tmv;
    localtime_r(&tt, &tmv);
    hora_tela(hora, sizeof hora, &tmv);
    corDoTipo(c->tipo == ILHA_ERRO ? ILHA_ERRO : ILHA_ACENTO, &cr, &cg, &cb);
    if (c->icone[0]) {
      gfx_icone((GfxRect){ hx, y + 20.0f, 24.0f, 24.0f }, c->icone, cr, cg, cb, ad);
      hx += 24.0f + 12.0f;
    }
    if (c->kicker[0]) {
      TxtLinha k = txt_linha(TXT_ILHA_NOME, c->kicker, 243, 242, 239, 255);
      txt_desenhar_alpha(k, hx, y + 32.0f - (float)k.h * 0.5f, ad);
      hx += (float)k.w + 12.0f;
      gfx_cor((GfxRect){ hx, y + 21.0f, 1.0f, 22.0f }, 0, 1, 1, 1, 0.18f * ad);
      hx += 1.0f + 12.0f;
    }
    { TxtLinha h = txt_linha(TXT_ILHA_NOME, hora, 243, 242, 239, 255);
      txt_desenhar_alpha(h, hx, y + 32.0f - (float)h.h * 0.5f, ad); }
    gfx_cor((GfxRect){ m.x, y + 63.0f, m.w, 1.0f }, 0, 1, 1, 1, 0.07f * ad);
  }
  y += 64.0f + 26.0f;
  { TxtLinha t = txt_linha_corta(TXT_LOG_T34, c->titulo, 243, 242, 239, 255, cw);
    if (desenha) txt_desenhar_alpha(t, x, y, ad);
    y += 41.0f + 14.0f; }
  if (desenha) txt_bloco_corta(TXT_AJ_TEXTO, c->texto, 243, 242, 239, x, y, cw, 30.0f, 0.62f * ad, 3);
  y += th + 28.0f;
  by = y;
  if (!desenha) return by + 60.0f + 32.0f - m.y;
  if (a > 0.3f) {
    ponteiro_camada();
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, pontFora, 0, 0);
    ponteiro_alvo(m.x, m.y, m.w, m.h, NULL, NULL, 0, 0);
  }
  { float bx = x;
    for (i = 0; i < nBotoes(); i++) {
      const char *rot = rotuloBotao(i), *ic = iconeBotao(i);
      float w = plrui_botao_largura(rot, ic);
      plrui_botao(bx, by, rot, ic, modalFocoA[i], a);
      if (a > 0.3f) ponteiro_alvo(bx, by, w, 60.0f, pontFoco, NULL, i, 0);
      bx += w + BOTAO_GAP;
    } }
  return by + 60.0f + 32.0f - m.y;
}
// A moldura da arte do modal segue a PROPORCAO DA IMAGEM: paisagem (still,
// fundo) nos 480x270 de sempre; cartaz em pe (< 1) numa moldura 2:3 da mesma
// altura (180x270). Nunca um cartaz espremido/recortado dentro do 16:9.
static float arteLadoW(const char *url) {
  float asp = url && url[0] ? tex_aspecto(url) : 0.0f;
  return asp > 0.05f && asp < 1.0f ? MD_ARTE_H * (2.0f / 3.0f) : MD_ARTE_W;
}
static float layoutModalAviso(GfxRect m, float a, int desenha) {
  const IlhaModal *c = &modalM;
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) return modalTelefone(m, c, a, desenha, 0);
#endif
  if (c->cabecalho) return layoutModalCabecalho(m, a, desenha);
  float ax = m.x + MD_PAD, ay = m.y + MD_PAD;
  int arte = c->arte[0] != 0, rosto = !arte && (c->rosto[0] || c->rostoNome[0]);
  int tile = !arte && !rosto && c->icone[0];
  float ladoW = arte ? arteLadoW(c->arte) : (rosto || tile) ? MG_LADO : 0.0f;
  float ladoH = arte ? MD_ARTE_H : (rosto || tile) ? MG_LADO : 0.0f;
  float cx = ax + (ladoW > 0.0f ? ladoW + 32.0f : 0.0f), cw = m.x + m.w - MD_PAD - cx;
  float y, colH, corpoH, y0, by;
  float ad = desenha ? a : 0.0f;
  int i, passo;
  // Com o ROSTO a coluna fica centrada nele, e para isso ela e medida antes
  // (passo 0, alfa 0); com arte ou ladrilho ela vai no topo e o desenho e um
  // passo so.
  y0 = ay; colH = 0.0f;
  for (passo = desenha && rosto ? 0 : 1; passo < 2; passo++) {
    float aa = passo == 0 ? 0.0f : ad;
    int vale = passo == 1 && desenha;
    y = y0;
    if (c->kicker[0]) {
      char up[160];
      float kx = cx;
      idioma_maiusc(up, sizeof up, c->kicker);
      if (arte && (c->rosto[0] || c->rostoNome[0])) {
        if (vale) rostoEm((GfxRect){ cx, y - 5.0f, 28.0f, 28.0f }, c->rosto, c->rostoNome, aa);
        kx += 38.0f;
      }
      if (vale) txt_tracking(TXT_MINI, up, 124, 124, 124, kx, y, aa, 1.8f);
      y += 18.0f + 8.0f;
    }
    { TxtLinha t = txt_linha(TXT_TITULO3, c->titulo, 246, 247, 252, 255);
      if ((float)t.w <= cw) {
        if (vale) txt_desenhar_alpha(t, cx, y, aa);
        y += (float)t.h;
      } else {
        // Titulo que nao cabe em uma linha (a pergunta da enquete, "Voce nao vai
        // mais receber enquetes") quebra em ate duas, em vez de virar reticencias.
        y += txt_bloco_corta(TXT_TITULO3, c->titulo, 246, 247, 252, cx, y, cw, (float)t.h * 1.02f, vale ? aa : 0.0f, 2);
      } }
    if (c->linha[0]) {
      TxtLinha t = txt_linha_corta(TXT_DET_META2, c->linha, 190, 192, 198, 255, cw);
      y += 10.0f;
      if (vale) txt_desenhar_alpha(t, cx, y, aa);
      y += (float)t.h;
    }
    if (c->nota[0]) {
      TxtLinha t = txt_linha_corta(TXT_CAPTION2, c->nota, 128, 130, 136, 255, cw);
      y += 4.0f;
      if (vale) txt_desenhar_alpha(t, cx, y, aa);
      y += (float)t.h;
    }
    if (c->texto[0]) {
      y += 10.0f;
      y += txt_bloco_corta(TXT_CAPTION, c->texto, 176, 180, 190, cx, y, cw, 30.0f, vale ? aa : 0.0f, 3);
    }
    if (c->fala[0]) {
      float h;
      y += 14.0f;
      h = txt_bloco_corta(TXT_HERO_SIN, c->fala, 220, 221, 224, cx + 21.0f, y, cw - 21.0f, 30.0f, vale ? aa : 0.0f, 3);
      if (vale) gfx_cor((GfxRect){ cx, y + 2.0f, 3.0f, h - 4.0f }, 0.0f, 1.0f, 1.0f, 1.0f, 0.16f * aa);
      y += h;
    }
    if (c->lista[0][0] && c->resultado) {
      // RESULTADO DA ENQUETE: texto, porcentagem na ponta e o trilho de 6 px.
      y += 14.0f;
      for (i = 0; i < 3 && c->lista[i][0]; i++) {
        char pc[8];
        int p = c->pct[i] < 0 ? 0 : c->pct[i] > 100 ? 100 : c->pct[i];
        float pw, cr, cg, cb;
        snprintf(pc, sizeof pc, "%d%%", p);
        { TxtLinha n = txt_linha(TXT_CAPTION, pc, c->escolha == i + 1 ? 246 : 190, c->escolha == i + 1 ? 247 : 192, c->escolha == i + 1 ? 252 : 198, 255);
          TxtLinha t = txt_linha_corta(TXT_CAPTION, c->lista[i], c->escolha == i + 1 ? 246 : 190, c->escolha == i + 1 ? 247 : 192, c->escolha == i + 1 ? 252 : 198, 255, cw - (float)n.w - 24.0f);
          pw = (float)n.w;
          if (vale) {
            txt_desenhar_alpha(t, cx, y, aa);
            txt_desenhar_alpha(n, cx + cw - pw, y, aa);
            ajustes_acento(&cr, &cg, &cb);
            gfx_cor((GfxRect){ cx, y + (float)t.h + 8.0f, cw, 6.0f }, 0.5f, 1.0f, 1.0f, 1.0f, 0.12f * aa);
            if (p > 0) gfx_cor((GfxRect){ cx, y + (float)t.h + 8.0f, cw * (float)p / 100.0f < 6.0f ? 6.0f : cw * (float)p / 100.0f, 6.0f }, 0.5f, cr, cg, cb, (c->escolha == i + 1 ? 1.0f : 0.55f) * aa);
          }
          y += (float)t.h + 8.0f + 6.0f + 16.0f;
        }
      }
    } else if (c->lista[0][0]) {
      y += 12.0f;
      for (i = 0; i < 3 && c->lista[i][0]; i++) {
        TxtLinha t = txt_linha_corta(TXT_CAPTION, c->lista[i], 190, 192, 198, 255, cw - 22.0f);
        if (vale) {
          gfx_cor((GfxRect){ cx + 4.0f, y + 14.0f - 3.5f, 7.0f, 7.0f }, 0.5f, 0.95f, 0.95f, 0.94f, 0.40f * aa);
          txt_desenhar_alpha(t, cx + 22.0f, y + 14.0f - (float)t.h * 0.5f, aa);
        }
        y += 28.5f;
      }
    }
    if (c->chips[0][0]) {
      float x = cx;
      y += 14.0f;
      for (i = 0; i < 3 && c->chips[i][0]; i++) {
        TxtLinha t = txt_linha(TXT_HERO_META, c->chips[i], 200, 202, 206, 255);
        float w = (float)t.w + 32.0f;
        if (x + w > cx + cw) break;
        if (vale) {
          gfx_cor((GfxRect){ x, y, w, 38.0f }, 0.5f, 1.0f, 1.0f, 1.0f, 0.08f * aa);
          txt_desenhar_alpha(t, x + 16.0f, y + 19.0f - (float)t.h * 0.5f, aa);
        }
        x += w + 10.0f;
      }
      y += 38.0f;
    }
    if (c->estado[0]) {
      TxtLinha t = txt_linha_corta(TXT_CAPTION2, c->estado, 168, 170, 176, 255, cw);
      // Na base da coluna (o margin-top:auto do mockup): alinhada ao pe da arte.
      float ye = ladoH > 0.0f && y0 + ladoH - (float)t.h > y + 12.0f ? y0 + ladoH - (float)t.h : y + 12.0f;
      if (vale) txt_desenhar_alpha(t, cx, ye, aa);
      y = ye + (float)t.h;
    }
    colH = y - y0;
    // Rosto: a coluna fica centrada nele (align-items:center no mockup).
    if (passo == 0 && rosto && colH < ladoH) y0 = ay + (ladoH - colH) * 0.5f;
  }
  corpoH = colH > ladoH ? colH : ladoH;
  by = ay + corpoH + 24.0f;
  if (!desenha) return MD_PAD + corpoH + 24.0f + BOTAO_H_SECUNDARIO + MD_PAD;
  // O lado esquerdo.
  if (arte) {
    GfxRect ra = { ax, ay, ladoW, MD_ARTE_H };
    GLuint tex = tex_obter_larg(c->arte, MD_ARTE_W);
    if (tex) {
      gfx_tex_aspect_atual = tex_aspecto(c->arte);
      gfx_rect(ra, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, 18.0f / MD_ARTE_H, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
    } else gfx_cor(ra, 18.0f / MD_ARTE_H, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
    if (c->rosto[0] || c->rostoNome[0]) {
      GfxRect ro = { ax + 18.0f, ay + MD_ARTE_H - 18.0f - 64.0f, 64.0f, 64.0f };
      gfx_rect((GfxRect){ ro.x - 4.0f, ro.y - 4.0f, 72.0f, 72.0f }, 0, GFX_DISCO, 0, 0, 0, 0,
               0.055f, 0.058f, 0.068f, 0.85f * a);
      rostoEm(ro, c->rosto, c->rostoNome, a);
    }
  } else if (rosto) {
    rostoEm((GfxRect){ ax, ay, MG_LADO, MG_LADO }, c->rosto, c->rostoNome, a);
  } else if (tile) {
    // O icone do ladrilho no acento (versao nova, aviso do dono) e no
    // vermelho no erro (Trakt, queda), como no mockup.
    float cr, cg, cb;
    corDoTipo(c->tipo == ILHA_ERRO ? ILHA_ERRO : ILHA_ACENTO, &cr, &cg, &cb);
    gfx_cor((GfxRect){ ax, ay, MG_LADO, MG_LADO }, 36.0f / MG_LADO, 1.0f, 1.0f, 1.0f, 0.08f * a);
    gfx_icone((GfxRect){ ax + (MG_LADO - 64.0f) * 0.5f, ay + (MG_LADO - 64.0f) * 0.5f, 64.0f, 64.0f },
              c->icone, cr, cg, cb, a);
  }
  // Botoes. O ponteiro: o fundo inteiro fecha, o modal absorve, cada botao foca.
  if (a > 0.3f) {
    ponteiro_camada();
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, pontFora, 0, 0);
    ponteiro_alvo(m.x, m.y, m.w, m.h, NULL, NULL, 0, 0);
  }
  { float x = ax;
    for (i = 0; i < nBotoes(); i++) {
      const char *rot = rotuloBotao(i), *ic = iconeBotao(i);
      float w = plrui_botao_largura(rot, ic);
      GfxRect r = { x, by, w, BOTAO_H_SECUNDARIO };
      // O BOTAO DE ILHA do Glass UI (plrui_botao): repouso branco 8% (solido
      // #24262C), foco cheio no acento com a luz, sem aro.
      plrui_botao(x, by + (BOTAO_H_SECUNDARIO - 60.0f) * 0.5f, rot, ic, modalFocoA[i], a);
      if (a > 0.3f) ponteiro_alvo(r.x, r.y, r.w, r.h, pontFoco, NULL, i, 0);
      x += w + BOTAO_GAP;
    } }
  // Na ponta: "Salvos ›" (o lado para onde a seta leva) ou a nota do rodape.
  if (c->salvos || c->rodape[0]) {
    TxtLinha t = txt_linha(TXT_CAPTION2, c->salvos ? i18n("Salvos") : c->rodape, 150, 152, 158, 255);
    TxtLinha v = txt_linha(TXT_CALLOUT, "›", 176, 180, 190, 255);
    float xr = m.x + m.w - MD_PAD, yc = by + BOTAO_H_SECUNDARIO * 0.5f;
    float x0 = c->salvos ? xr - (float)v.w - 8.0f - (float)t.w : xr - (float)t.w;
    txt_desenhar_alpha(t, x0, yc - (float)t.h * 0.5f, a * 0.9f);
    if (c->salvos) {
      txt_desenhar_alpha(v, xr - (float)v.w, yc - (float)v.h * 0.5f - 2.0f, a * 0.9f);
      if (a > 0.3f) ponteiro_alvo(x0 - 10.0f, by, xr - x0 + 20.0f, BOTAO_H_SECUNDARIO, NULL, pontSalvos, 0, 0);
    }
  }
  return MD_PAD + corpoH + 24.0f + BOTAO_H_SECUNDARIO + MD_PAD;
}

static void desenharModal(GfxRect m, float a) {
  const IlhaCartao *c = &modalC;
  if (modalAviso) { if (a >= 0.01f) layoutModalAviso(m, a, 1); return; }
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo() && !modalAtividade) {
    IlhaModal mobile = {0};
    snprintf(mobile.titulo, sizeof mobile.titulo, "%s", c->titulo);
    snprintf(mobile.arte, sizeof mobile.arte, "%s", c->arte[0] ? c->arte : c->poster);
    snprintf(mobile.texto, sizeof mobile.texto, "%s", c->sinopse);
    if (c->serie && c->t > 0 && c->e > 0)
      snprintf(mobile.linha, sizeof mobile.linha, i18n("T%dE%d · %s"), c->t, c->e, c->epNome);
    if (modalQual == ILHA_VIVO)
      snprintf(mobile.estado, sizeof mobile.estado, i18n("%d min restantes"), c->restanteMin < 1 ? 1 : c->restanteMin);
    else if (modalQual == ILHA_AMIGO)
      snprintf(mobile.estado, sizeof mobile.estado, i18n("%s · agora"), c->pessoa);
    else snprintf(mobile.estado, sizeof mobile.estado, "%s", c->quando);
    modalTelefone(m, &mobile, a, 1, 1);
    return;
  }
#endif
  float ax = m.x + MD_PAD, ay = m.y + MD_PAD;
  // Cartaz em pe (o amigo assistindo traz so o poster) numa moldura 2:3, como
  // no modal de aviso; antes ia centrado no 16:9 com sobra preta dos lados.
  float ladoW = arteLadoW(c->arte[0] ? c->arte : c->poster);
  float cx = ax + ladoW + 32.0f, cw = m.x + m.w - MD_PAD - cx;
  float extraSin = modalSinopseExtra();
  float y = ay, by = ay + MD_ARTE_H + extraSin + 24.0f, sy, cr, cg, cb;
  int i;
  if (a < 0.01f) return;
  ajustes_acento(&cr, &cg, &cb);
  if (modalAtividade && atvTemCarga) {
    // PAINEL COMPACTO da carga da Home: titulo + etapa a esquerda, o tempo
    // como contador ao vivo (m:ss) a direita, a barra fina dos add-ons e dois
    // chips (Fileiras, Falhas). Sem botao: Voltar, OK ou tocar fora fecham.
    const IlhaAtvCarga *k = &atvCarga;
    float iw = m.w - MD_PAD * 2, x0 = m.x + MD_PAD, y0 = m.y + MD_PAD;
    unsigned seg = k->ms / 1000u;
    char tm[24], num[32];
    TxtLinha title = txt_linha_corta(TXT_CALLOUT, atvTitulo, 246, 247, 252, 255, iw - 150.0f);
    TxtLinha stage = txt_linha_corta(TXT_CAPTION, k->etapa, 190, 194, 204, 255, iw - 150.0f);
    snprintf(tm, sizeof tm, "%u:%02u", seg / 60u, seg % 60u);
    TxtLinha tl = txt_linha(TXT_TITULO3, tm, 246, 247, 252, 255);
    txt_desenhar_alpha(title, x0, y0, a);
    txt_desenhar_alpha(stage, x0, y0 + (float)title.h + 6.0f, a);
    txt_desenhar_alpha(tl, x0 + iw - (float)tl.w, y0 + ((float)(title.h + stage.h) + 6.0f - (float)tl.h) * 0.5f, a);
    float by0 = y0 + (float)title.h + (float)stage.h + 34.0f;
    GfxRect tr = { x0, by0, iw, 8.0f };
    float alvoB = k->total > 0 ? (float)k->prontos / (float)k->total : 0.0f;
    int quieto = anim_politica_reduzida || ajustes_animacoes_reduzidas();
    if (!k->ativo) alvoB = 1.0f;
    if (alvoB > 1.0f) alvoB = 1.0f;
    atvBarraA = quieto ? alvoB : atvBarraA + (alvoB - atvBarraA) * 0.14f;
    if (ajustes_vidro()) gfx_vidro_chip(tr, 0.5f, 0.55f, a);
    else gfx_cor(tr, 0.5f, 1.0f, 1.0f, 1.0f, 0.14f * a);
    { float fw = tr.w * atvBarraA;
      if (k->ativo && k->total <= 0) fw = tr.w * 0.18f;
      if (fw > 8.0f) gfx_cor((GfxRect){ tr.x, tr.y, fw, tr.h }, 0.5f, cr, cg, cb, a);
      // A luz que varre o trecho cheio: so enquanto carrega.
      if (k->ativo && !quieto && fw > 40.0f) {
        float ph = (float)(SDL_GetTicks() % 1400u) / 1400.0f, lw = 70.0f;
        float lx = tr.x - lw + (fw + lw) * ph, lx1 = lx + lw;
        if (lx < tr.x) lx = tr.x;
        if (lx1 > tr.x + fw) lx1 = tr.x + fw;
        if (lx1 - lx > 4.0f) gfx_cor((GfxRect){ lx, tr.y, lx1 - lx, tr.h }, 0.5f, 1.0f, 1.0f, 1.0f, 0.38f * a);
      } }
    { TxtLinha ad = txt_linha(TXT_CAPTION2, i18n("Add-ons"), 176, 180, 190, 255);
      snprintf(num, sizeof num, "%d / %d", k->prontos, k->total);
      TxtLinha nl = txt_linha(TXT_CAPTION2, num, 226, 228, 234, 255);
      float ly = by0 + 8.0f + 14.0f;
      txt_desenhar_alpha(ad, x0, ly, a);
      txt_desenhar_alpha(nl, x0 + iw - (float)nl.w, ly, a);
      by0 = ly + (float)ad.h + 26.0f; }
    { const char *rot[2] = { i18n("Fileiras"), i18n("Falhas") };
      int val[2] = { k->fileiras, k->falhas }, c2;
      float cw2 = (iw - 16.0f) * 0.5f, ch2 = 60.0f;
      for (c2 = 0; c2 < 2; c2++) {
        GfxRect chip = { x0 + c2 * (cw2 + 16.0f), by0, cw2, ch2 };
        int ruim = c2 == 1 && val[c2] > 0;
        TxtLinha lb = txt_linha(TXT_CAPTION2, rot[c2], 176, 180, 190, 255);
        snprintf(num, sizeof num, "%d", val[c2]);
        TxtLinha vl = txt_linha(TXT_CALLOUT, num, ruim ? 255 : 246, ruim ? 90 : 247, ruim ? 82 : 252, 255);
        gfx_cor(chip, 0.5f, 1.0f, 1.0f, 1.0f, 0.08f * a);
        txt_desenhar_alpha(lb, chip.x + 24.0f, chip.y + (ch2 - (float)lb.h) * 0.5f, a);
        txt_desenhar_alpha(vl, chip.x + chip.w - 24.0f - (float)vl.w, chip.y + (ch2 - (float)vl.h) * 0.5f, a);
      } }
    if (a > 0.3f) {
      ponteiro_camada();
      ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, pontFora, 0, 0);
      ponteiro_alvo(m.x, m.y, m.w, m.h, NULL, pontFora, 0, 0);
    }
    return;
  }
  if (modalAtividade) {
    GfxRect box = {ax, ay, 120, 120};
    gfx_cor(box, 0.22f, 0.14f, 0.15f, 0.17f, a);
    TxtLinha mark = txt_linha(TXT_TITULO3, "…", 190, 200, 220, 255);
    txt_desenhar_alpha(mark, ax + (120 - mark.w) * 0.5f, ay + 30, a);
    float tx = ax + 152, width = m.w - MD_PAD * 2 - 152;
    TxtLinha title = txt_linha_corta(TXT_TITULO3, atvTitulo, 246, 247, 252, 255, width);
    txt_desenhar_alpha(title, tx, ay, a);
    txt_bloco_corta(TXT_CAPTION, atvDetalhes, 190, 194, 204,
                    tx, ay + title.h + 18, width, 32, a, 6);
    GfxRect button = { ax, by, botao_largura("Fechar", NULL, 1), BOTAO_H_SECUNDARIO };
    plrui_botao(button.x, button.y, "Fechar", NULL, modalFocoA[0], a);
    if (a > 0.3f) {
      ponteiro_camada();
      ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, pontFora, 0, 0);
      ponteiro_alvo(m.x, m.y, m.w, m.h, NULL, NULL, 0, 0);
      ponteiro_alvo(button.x, button.y, button.w, button.h, NULL, pontFora, 0, 0);
    }
    return;
  }
  // A arte: o still do episodio quando ha, senao o fundo do titulo, senao o cartaz.
  { const char *arte = c->arte[0] ? c->arte : c->poster;
    GfxRect ra = { ax, ay, ladoW, MD_ARTE_H };
    GLuint tex = arte[0] ? tex_obter_larg(arte, MD_ARTE_W) : 0;
    if (tex) {
      gfx_tex_aspect_atual = tex_aspecto(arte);
      gfx_rect(ra, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, 18.0f / MD_ARTE_H, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
    } else gfx_cor(ra, 18.0f / MD_ARTE_H, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a); }
  // Logo do titulo (como a pagina do titulo), ou o nome.
  { GLuint lt = c->logo[0] ? tex_obter_larg_qualquer(c->logo, MD_LOGO_W) : 0;
    float asp = lt ? tex_aspecto(c->logo) : 0.0f;
    if (lt && asp > 0.0f) {
      float h = MD_LOGO_H, w = h * asp;
      GfxModo md = tex_marca_escura(c->logo) ? GFX_MARCA : GFX_TEXTO;
      if (w > MD_LOGO_W) { w = MD_LOGO_W; h = w / asp; }
      gfx_tex_aspect_atual = 0.0f;
      gfx_rect((GfxRect){ cx, y + MD_LOGO_H - h, w, h }, lt, md, 0, 0, 0, 0.0f, 1, 1, 1, a);
      y += MD_LOGO_H;
    } else {
      TxtLinha t = txt_linha_corta(TXT_TITULO3, c->titulo, 246, 247, 252, 255, cw);
      txt_desenhar_alpha(t, cx, y, a);
      y += (float)t.h;
    } }
  y += 14.0f;
  if (c->serie && c->t > 0 && c->e > 0) {
    char b[200];
    TxtLinha t;
    if (c->epNome[0]) snprintf(b, sizeof b, i18n("T%dE%d · %s"), c->t, c->e, c->epNome);
    else snprintf(b, sizeof b, i18n("T%dE%d"), c->t, c->e);
    t = txt_linha_corta(TXT_CALLOUT, b, 226, 228, 234, 255, cw);
    txt_desenhar_alpha(t, cx, y, a);
    y += (float)t.h + 10.0f;
  }
  // A linha de estado fica na base da arte; a sinopse usa o que sobra entre.
  { char b[120];
    TxtLinha t;
    if (modalQual == ILHA_VIVO) {
      int mi = c->restanteMin < 1 ? 1 : c->restanteMin;
      snprintf(b, sizeof b, i18n("%d min restantes"), mi);
    } else if (modalQual == ILHA_AMIGO) snprintf(b, sizeof b, i18n("%s · agora"), c->pessoa);
    else if (c->quando[0]) snprintf(b, sizeof b, "%s · %s", i18n("Episódio novo"), c->quando);
    else snprintf(b, sizeof b, "%s", i18n("Episódio novo"));
    t = txt_linha(TXT_CAPTION, b, 200, 204, 212, 255);
    sy = ay + MD_ARTE_H + extraSin - (float)t.h;
    if (modalQual == ILHA_ESTREIA || modalQual == ILHA_AMIGO) {
      // O ponto: acento no episodio novo, vermelho de "agora" no amigo.
      if (modalQual == ILHA_AMIGO) gfx_cor((GfxRect){ cx, sy + (float)t.h * 0.5f - 5.0f, 10.0f, 10.0f }, 0.5f, 1.0f, 0.353f, 0.322f, a);
      else gfx_cor((GfxRect){ cx, sy + (float)t.h * 0.5f - 5.0f, 10.0f, 10.0f }, 0.5f, cr, cg, cb, a);
      txt_desenhar_alpha(t, cx + 20.0f, sy, a);
    } else {
      // O trilho de 220 primeiro e o tempo depois (mockup do player, 03/10).
      float pr = c->progresso < 0.0f ? 0.0f : c->progresso > 1.0f ? 1.0f : c->progresso;
      GfxRect tr = { cx, sy + (float)t.h * 0.5f - 2.0f, 220.0f, 4.0f };
      if (ajustes_vidro()) gfx_vidro_chip(tr, 0.5f, 0.55f, a);
      else gfx_cor(tr, 0.5f, 1.0f, 1.0f, 1.0f, 0.16f * a);
      if (tr.w * pr > 4.0f) gfx_cor((GfxRect){ tr.x, tr.y, tr.w * pr, 4.0f }, 0.5f, cr, cg, cb, a);
      txt_desenhar_alpha(t, cx + 220.0f + 14.0f, sy, a);
    } }
  if (c->sinopse[0]) {
    int linhas = (int)((sy - 12.0f - y) / 30.0f);
    if (linhas > 3) linhas = 3;
    if (linhas >= 1)
      sinopse_bloco_corta(TXT_CAPTION, c->sinopse, 176, 180, 190, cx, y, cw, 30.0f, a, linhas);
  }
  // Botoes. O ponteiro: o fundo inteiro fecha, o modal absorve, cada botao foca.
  if (a > 0.3f) {
    ponteiro_camada();
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, pontFora, 0, 0);
    ponteiro_alvo(m.x, m.y, m.w, m.h, NULL, NULL, 0, 0);
  }
  { float x = ax;
    for (i = 0; i < nBotoes(); i++) {
      const char *rot = rotuloBotao(i), *ic = iconeBotao(i);
      float w = plrui_botao_largura(rot, ic);
      GfxRect r = { x, by, w, BOTAO_H_SECUNDARIO };
      // O BOTAO DE ILHA do Glass UI (plrui_botao): repouso branco 8% (solido
      // #24262C), foco cheio no acento com a luz, sem aro.
      plrui_botao(x, by + (BOTAO_H_SECUNDARIO - 60.0f) * 0.5f, rot, ic, modalFocoA[i], a);
      if (a > 0.3f) ponteiro_alvo(r.x, r.y, r.w, r.h, pontFoco, NULL, i, 0);
      x += w + BOTAO_GAP;
    } }
  // "Salvos ›" na ponta: o lado para onde a seta leva.
  { TxtLinha t = txt_linha(TXT_CAPTION2, "Salvos", 176, 180, 190, 255);
    TxtLinha v = txt_linha(TXT_CALLOUT, "›", 176, 180, 190, 255);
    float xr = m.x + m.w - MD_PAD, yc = by + BOTAO_H_SECUNDARIO * 0.5f;
    float x0 = xr - (float)v.w - 8.0f - (float)t.w;
    txt_desenhar_alpha(t, x0, yc - (float)t.h * 0.5f, a * 0.9f);
    txt_desenhar_alpha(v, xr - (float)v.w, yc - (float)v.h * 0.5f - 2.0f, a * 0.9f);
    if (a > 0.3f) ponteiro_alvo(x0 - 10.0f, by, xr - x0 + 20.0f, BOTAO_H_SECUNDARIO, NULL, pontSalvos, 0, 0); }
}

// --- minimizar na ilha ----------------------------------------------------------
// Pedido do dono (02/10): "quando sair do filme, minimizasse para a ilha do
// relogio e voltasse para a home". O plano de video e hardware e nao se le de
// volta (LG), entao a transicao usa a arte do modal (still do
// episodio ou fundo do titulo): nasce em tela cheia e encolhe numa mola de 560 ms (ilha_voo.h)
// ate o retangulo exato da mini capa da pilula, onde troca para o
// cartaz que a capa mostra. A home aparece por tras com o veu preto apagando.
//
// CUSTO: a arte (1 quad), o cartaz no fim (1 quad, so no cruzamento), uma
// sombra do tamanho dela e o veu em ate 4 faixas AO REDOR da arte — nunca por
// baixo dela, entao em pixel nao ha uma tela cheia a mais sobre a home.
int ilha_minimizar(const char *fundoReserva) {
  const IlhaCartao *c = &cartoes[ILHA_VIVO];
  // relogioQuer ainda e o do ultimo quadro desenhado (o da pagina, antes do
  // player): quem vale e o do proximo, conferido em ilha_desenhar.
  if (!temCartao[ILHA_VIVO] || !ajustes_relogio_ligado()) return 0;
  altBase = SDL_GetTicks();
  if (anim_politica_reduzida || ajustes_animacoes_reduzidas()) { voo = 0; return 1; }
  snprintf(vooArte, sizeof vooArte, "%s", c->arte[0] ? c->arte : fundoReserva ? fundoReserva : "");
  // A arte do episodio pode nunca ter sido decodificada nesta sessao (serie
  // aberta pelo Continuar): o fundo do titulo, que a pagina acabou de mostrar.
  if (vooArte[0] && !tex_obter_larg_qualquer(vooArte, 960.0f) && fundoReserva && fundoReserva[0])
    snprintf(vooArte, sizeof vooArte, "%s", fundoReserva);
  snprintf(vooCapa, sizeof vooCapa, "%s", c->poster);
  vooDissolve = 0;
  // vooDesde = 0: o relogio do voo comeca no primeiro quadro DESENHADO. O
  // ultimo quadro da ilha foi antes do player, e um dt de minutos daria o
  // primeiro passo inteiro de uma vez.
  voo = 1; vooT = 0.0f; vooDesde = 0; vooAlvoOk = 0; pousouEm = 0;
  printf("[ilha] minimizar: %s -> mini capa\n", c->imdb);
  return 1;
}

int ilha_minimizando(void) { return voo; }

void ilha_minimizar_dissolver(int sim) {
  if (voo) vooDissolve = sim ? 1 : 0;
  if (voo && sim) printf("[ilha] minimizar: dissolve a partir do video parado\n");
}

static void vooFim(const char *por, Uint32 agora) {
  if (!voo) return;
  voo = 0;
  altBase = agora;
  pousouEm = strcmp(por, "pousou") ? 0 : agora ? agora : 1;
  printf("[ilha] minimizar: fim (%s, %u ms)\n", por, vooDesde ? (unsigned)(agora - vooDesde) : 0u);
}

static float suave01(float a, float b, float x) {
  float t = (x - a) / (b - a);
  t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
  return t * t * (3.0f - 2.0f * t);
}

// O retangulo do quadro no instante t: tamanho e centro na MESMA fracao, em
// linha reta. A curva acelera e desacelera sem atravessar o destino: tamanho
// e centro chegam juntos, sem um cartaz grande sobre o texto da pilula.
static GfxRect vooRect(GfxRect alvo, float t, float *f) {
  return ilha_voo_rect(alvo, t, NV_TELA_W, NV_TELA_H, f);
}

// Veu: o preto do player apagando, so ao redor da arte.
static void vooVeu(GfxRect q, float a) {
  float W0 = (float)NV_TELA_W, H0 = (float)NV_TELA_H;
  float x0 = q.x < 0.0f ? 0.0f : q.x, x1 = q.x + q.w > W0 ? W0 : q.x + q.w;
  float y0 = q.y < 0.0f ? 0.0f : q.y, y1 = q.y + q.h > H0 ? H0 : q.y + q.h;
  if (a < 0.01f) return;
  if (y0 > 0.0f) gfx_cor((GfxRect){ 0, 0, W0, y0 }, 0.0f, 0, 0, 0, a);
  if (y1 < H0)   gfx_cor((GfxRect){ 0, y1, W0, H0 - y1 }, 0.0f, 0, 0, 0, a);
  if (x0 > 0.0f) gfx_cor((GfxRect){ 0, y0, x0, y1 - y0 }, 0.0f, 0, 0, 0, a);
  if (x1 < W0)   gfx_cor((GfxRect){ x1, y0, W0 - x1, y1 - y0 }, 0.0f, 0, 0, 0, a);
}

// COVER FORCADO: o retangulo passa de 16:9 para a proporcao da capa no fim,
// e o GFX_CARD trocava cover por contain (com faixas cinza) assim que a
// moldura fugia 25% da arte (issue #89) — no meio do voo a imagem pulava para
// uma tarja. Aqui ela recorta sempre; nunca estica nem ganha faixa.
static void vooTexEm(GLuint tex, float asp, GfxRect q, float raio, float a) {
  float antes = gfx_card_forcar_cover_atual;
  gfx_tex_aspect_atual = asp;
  gfx_card_forcar_cover_atual = 1.0f;
  gfx_rect(q, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, raio, 0, 0, 0, a);
  gfx_card_forcar_cover_atual = antes;
  gfx_tex_aspect_atual = 0.0f;
}
static void vooArteEm(const char *url, GfxRect q, float raio, float a) {
  GLuint tex = url[0] ? tex_obter_larg_qualquer(url, 960.0f) : 0;
  if (a < 0.01f) return;
  if (tex) vooTexEm(tex, tex_aspecto(url), q, raio, a);
  else gfx_cor(q, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
}

// fase 0 = o veu (vai por baixo da pilula), 1 = o quadro (por cima dela).
static void desenharVoo(GfxRect pilulaFinal, int fase) {
  GfxRect q;
  float f, t = vooT, raioPx, cruza;
  if (!vooAlvoOk) {
    vooAlvo = capaNaPilula(&cartoes[ILHA_VIVO], pilulaFinal);
    vooAlvoOk = 1;
  }
  q = vooRect(vooAlvo, t, &f);
  // Veu: no primeiro quadro e o preto exato em volta do video (a tarja do
  // player); escurece a home de leve e se desfaz antes do pouso.
  if (fase == 0) { float v = 1.0f - suave01(0.0f, 0.7f, t); vooVeu(q, v * v); return; }
  // Canto: reto no primeiro quadro (igual ao player), arredonda cedo e
  // assenta no da capa (6 px), em pixels e nunca acima de meia altura.
  raioPx = 6.0f * f + 30.0f * suave01(0.0f, 0.25f, f) * (1.0f - f);
  if (raioPx > q.h * 0.5f) raioPx = q.h * 0.5f;
  if (f > 0.02f)
    gfx_rect((GfxRect){ q.x - 18.0f, q.y - 8.0f, q.w + 36.0f, q.h + 40.0f }, 0, GFX_SOMBRA,
             1.0f, 0, 0, 0.5f, 0, 0, 0, 0.40f * suave01(0.0f, 0.3f, f));
  // O cartaz entra enquanto o quadro assume a forma da capa: no pouso ja e ele.
  cruza = vooCapa[0] ? suave01(0.72f, 0.98f, t) : 0.0f;
  if (cruza < 0.99f) vooArteEm(vooArte, q, raioPx / q.h, 1.0f);
  if (cruza > 0.0f) vooArteEm(vooCapa, q, raioPx / q.h, cruza);
  // Por ultimo: tudo o que ja esta no quadro (home, veu, arte) entra em
  // fracao `d`, e o resto e o video parado no plano de baixo.
  if (vooDissolve && vooDesde) {
    float d = (float)(SDL_GetTicks() - vooDesde) / (float)VOO_DISSOLVE_MS;
    if (d >= 1.0f) vooDissolve = 0;
    else gfx_dissolver_tela(d * d * (3.0f - 2.0f * d));
  }
}

static void vooPasso(Uint32 agora) {
  if (!voo) return;
  if (!vooDesde) { vooDesde = agora ? agora : 1; return; }
  vooT = ilha_voo_fracao(agora - vooDesde);
  if (vooT >= 1.0f) vooFim("pousou", agora);
}

// A capa de ilha_salvar em voo ate o icone do aviso. O relogio do voo comeca
// no primeiro quadro DESENHADO com o aviso na pilula (como o minimizar), e a
// capa se dissolve no icone nos ultimos 30% enquanto ele aparece por baixo.
static void salvarVoo(Uint32 agora, float x, float y, int dir) {
  LinhasAviso L;
  float cw, pw, f, raioPx;
  GfxRect pf, alvo, q;
  if (svooQuer && mostra == M_AVISO && !strcmp(mostraA.chave, SALVAR_CHAVE)) {
    svooQuer = 0; svoo = 1; svooT = 0.0f; svooDesde = 0; svooIconeA = 0.0f;
  }
  if (!svoo) { svooIconeA = 1.0f; return; }
  if (mostra != M_AVISO || strcmp(mostraA.chave, SALVAR_CHAVE)) { svoo = 0; svooIconeA = 1.0f; return; }
  if (!svooDesde) svooDesde = agora ? agora : 1;
  svooT = ilha_voo_fracao(agora - svooDesde);
  cw = larguraAviso(&mostraA, 0, &L);
  pw = PAD_E + PAD_D + cw;
  pf = (GfxRect){ dir ? x - pw : x, y, pw, NV_ILHA_H_ABERTA };
  alvo = (GfxRect){ pf.x + (pf.w - cw) * 0.5f, pf.y + pf.h * 0.5f - ICONE * 0.5f, ICONE, ICONE };
  q = ilha_voo_rect_de(svooDe, svooDe.w / svooDe.h, alvo, svooT, &f);
  svooIconeA = suave01(0.70f, 1.0f, svooT);
  raioPx = 12.0f + (ICONE * 0.25f - 12.0f) * f;
  if (svooT < 1.0f) {
    // Sombra caida curta, para a capa se descolar da tela de baixo.
    gfx_rect((GfxRect){ q.x - 10.0f, q.y - 2.0f, q.w + 20.0f, q.h + 22.0f }, 0, GFX_SOMBRA,
             1.0f, 0, 0, 0.5f, 0, 0, 0, 0.30f * (1.0f - svooIconeA));
    vooArteEm(svooPoster, q, raioPx / (q.h > 1.0f ? q.h : 1.0f), 1.0f - svooIconeA);
  }
  if (svooT >= 1.0f) { svoo = 0; svooIconeA = 1.0f; pousouEm = agora ? agora : 1; }
}

static void ilha_desenharCorpo_(Uint32 agora);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void ilha_corpo(float w, float h, IlhaCorpo corpo, void *u) {
  corpoPed = 1; corpoW = w; corpoH = h; corpoFn = corpo; corpoU = u; corpoSemSombra = 0;
  if (modalAberto) ilha_modal_fechar(1);
}
float ilha_corpo_t(void) { return corpoT; }
void ilha_corpo_sem_sombra(void) { corpoSemSombra = 1; }

// A hora da pilula em repouso, no lugar exato dela: com o corpo aberto e a
// pilula mostrando outra coisa (aviso, cartao), e ela que fica no cabecalho.
static void horaNoLugar(float x, int dir, float y, float a) {
  TxtLinha t1, t2;
  float w = PAD_E + PAD_D + larguraConteudo(M_RELOGIO, &t1, &t2);
  GfxRect r = { dir ? x - w : x, y, w, NV_ILHA_H };
  if (a < 0.01f) return;
  txt_desenhar_alpha(t1, r.x + (r.w - (w - PAD_E - PAD_D)) * 0.5f, r.y + (r.h - (float)t1.h) * 0.5f, a);
}

void ilha_desenhar(Uint32 agora) {
  ESCALA_INI();
  ilha_desenharCorpo_(agora);
  ESCALA_FIM();
}
static void ilha_desenharCorpo_(Uint32 agora) {
  float dt = ultQuadro ? (float)(agora - ultQuadro) / 1000.0f : 1.0f / 60.0f;
  int alvo, vis, dir, trocando = 0;
  float alvoW, alvoH, x, y;
  TxtLinha t1, t2;
  GfxRect vooPf;
  Uint32 antes = ultQuadro;
  ultQuadro = agora;
  if (dt > 0.1f) dt = 0.1f;

  // Aviso vencido sai; o prazo so comeca a contar quando ele aparece.
  vezNaTela(agora, antes);
  if (temCur && curAte && !coberta && (Sint32)(agora - curAte) >= 0) { proximo(); vezNaTela(agora, antes); }
  // Aviso que pediu `abrir`: a pilula cresce ate o modal no primeiro quadro.
  if (temCur && cur.abrir && curAte) { cur.abrir = 0; abrirDoAviso(); }
  atualizarHora();

  cartaoVez = cartaoDaVez(agora);
  // EM VOO o cartao e o da sessao que acabou de sair; se ela sumiu, a pilula
  // saiu da tela ou o modal abriu por cima, o voo acaba seco.
  if (voo) {
    if (!temCartao[ILHA_VIVO]) vooFim("cartao", agora);
    else if (!relogioQuer && vooDesde && agora - vooDesde > 200u) vooFim("relogio", agora);
    else if (modalAberto) vooFim("modal", agora);
    else cartaoVez = ILHA_VIVO;
  }
  // O MODAL DE CARTAO SO EXISTE COM O RELOGIO NA TELA. Saiu dela (o detalhe
  // abriu pelo "Retomar", outra camada entrou): some seco, sem recolher por
  // cima dela. O de AVISO nao depende do relogio — o aviso que o abriu aparece
  // em qualquer tela fora do player, e o modal dele tambem.
  if (!relogioQuer && !modalAviso && (modalAberto || modalT > 0.0f)) ilha_modal_fechar(1);
  if (modalAviso && modalAberto && !modalAvisoMedido) {
    // Linha que o orcamento de texto do quadro recusou mede 0: mede de novo
    // no quadro seguinte, senao o modal nasceria curto para sempre.
    int antes = txt_pendentes;
    modalAvisoH = layoutModalAviso((GfxRect){ 0, 0, telefoneui_largura(modalM.cabecalho ? MC_W : MD_W, NV_TELA_W, 40), MD_H }, 0.0f, 0);
    modalAvisoMedido = txt_pendentes == antes;
  }
  alvo = temCur ? M_AVISO : atividadeViva(agora) ? M_ATIVIDADE
       : (relogioQuer && cartaoVez >= 0) ? M_CARTAO : M_RELOGIO;
  // O CORPO (ilha_corpo): a mola do modal; efeitos leves sem repique e mais
  // curta, minimos direto ao alvo.
  { int quer = corpoPed;
    float alvoC = quer ? 1.0f : 0.0f;
    corpoPed = 0;
    if (gfx_efeitos_minimos()) { corpoT = alvoC; corpoV = 0.0f; }
    else if (gfx_efeitos_leves()) corpoT = molaIlhaWZ(&corpoV, corpoT, alvoC, dt, MODAL_MOLA_W * 1.5f, 1.0f);
    else corpoT = molaIlhaWZ(&corpoV, corpoT, alvoC, dt, MODAL_MOLA_W, MODAL_MOLA_Z);
    if (!quer && corpoT < 0.01f) { corpoT = 0.0f; corpoV = 0.0f; }
    // O TAMANHO PEDIDO MUDA com o painel aberto (botao posto ou tirado, a
    // edicao): anda na mola da pilula em vez de saltar. Nascendo, ja e o alvo.
    if (corpoT <= 0.0f || corpoWa <= 0.0f || gfx_efeitos_minimos()) {
      corpoWa = corpoW; corpoHa = corpoH; corpoVw = corpoVh = 0.0f;
    } else {
      corpoWa = molaIlha(&corpoVw, corpoWa, corpoW, dt);
      corpoHa = molaIlha(&corpoVh, corpoHa, corpoH, dt);
    } }
  vis = alvo != M_RELOGIO || relogioQuer || central_segurar_progresso(agora) > 0.0f ||
        (modalAviso && (modalAberto || modalT > 0.01f)) || corpoT > 0.0f;
  // O MEDIDOR so ocupa a ilha LIVRE: com aviso, atividade, modal, voo ou o
  // TRAILER tocando (destaque da home, pagina do titulo — dono no S18, 2.0.6)
  // ele sai (o pouso e os avisos nunca esperam por ele) e volta depois.
  { int ds = desempenho_forma(), livre = !modalAberto && modalT <= 0.0f && !voo && !trailer_tocando();
    dsVez = alvo == M_RELOGIO && livre ? ds : DS_DESLIGADO;
    dsCartao = alvo == M_CARTAO && livre && ds != DS_DESLIGADO; }
  // O que esta desenhado so troca quando o conteudo velho ja apagou: a pilula
  // muda de forma com o texto antigo saindo, e o novo entra com ela perto do
  // tamanho final — a troca nunca acontece com o texto cheio na tela.
  if (mostra < 0) { mostra = alvo; conteudoA = 0.0f; dsMostra = dsVez; }
  { static const char *const CH_DS[4] = { "", "ds1", "ds2", "ds3" };
    const char *ch = alvo == M_AVISO ? cur.chave : alvo == M_CARTAO ? cartoes[cartaoVez].chave : CH_DS[dsVez & 3];
    trocando = mostra != alvo || strcmp(mostraChave, ch);
    // EM VOO a pilula ja esta aberta com o cartao esperando o quadro (mockup
    // do player, "saida-voo"): a troca de conteudo nao espera o fade.
    if (trocando && voo && alvo == M_CARTAO) {
      mostra = alvo; conteudoA = 1.0f; trocando = 0;
      snprintf(mostraChave, sizeof mostraChave, "%s", ch);
      mostraC = cartoes[cartaoVez]; mostraQual = cartaoVez;
    }
    if (trocando) {
      conteudoA = ajustes_animacoes_reduzidas() ? 0.0f : anim_mola(conteudoA, 0.0f, dt, 26.0f);
      if (conteudoA < 0.06f) {
        mostra = alvo; conteudoA = 0.0f; dsMostra = dsVez;
        snprintf(mostraChave, sizeof mostraChave, "%s", ch);
        if (alvo == M_CARTAO) { mostraC = cartoes[cartaoVez]; mostraQual = cartaoVez; }
        if (alvo == M_AVISO) mostraA = cur;
      }
    } else if (alvo == M_CARTAO) mostraC = cartoes[cartaoVez];   // tempo e barra ao vivo
    else if (alvo == M_AVISO) mostraA = cur;                       // texto trocado no lugar
  }

  alvoH = alvo == M_RELOGIO ? NV_ILHA_H : NV_ILHA_H_ABERTA;
  if (alvo == M_AVISO && avisoV2(&cur)) alvoH = V2_AV_H;
  if (alvo == M_ATIVIDADE && atvV2) alvoH = V2_ATV_H;
  if (alvo == M_CARTAO) { LinhasCartao L; alvoW = PAD_E + PAD_D + larguraCartao(&cartoes[cartaoVez], cartaoVez, &L); }
  else if (alvo == M_AVISO) { LinhasAviso L; alvoW = PAD_E + PAD_D + larguraAviso(&cur, esperando(), &L); }
  else alvoW = PAD_E + PAD_D + larguraConteudo(alvo, &t1, &t2);
  if (alvo == M_RELOGIO && dsVez != DS_DESLIGADO) {
    float bw, bh;
    desempenho_corpo_tam(dsVez, &bw, &bh);
    alvoW += desempenho_linha_w(dsVez);
    if (dsVez == DS_GRANDE) alvoW = 28.0f * 2.0f + (float)t1.w + desempenho_linha_w(dsVez);
    if (bw > alvoW) alvoW = bw;
    if (bh > 0.0f) alvoH = NV_ILHA_H + bh;
  }
  // Sumindo, ela encolhe para uma gota antes de apagar (e nasce dela).
  if (!vis) alvoW = alvoH = NV_ILHA_H * 0.6f;
  if (W <= 0.0f) { W = NV_ILHA_H * 0.6f; H = W; }
  W = molaIlha(&vW, W, alvoW, dt);
  H = molaIlha(&vH, H, alvoH, dt);
  A = anim_mola(A, vis ? 1.0f : 0.0f, dt, vis ? 9.0f : 12.0f);
  // TROCANDO DE AVISO PARA AVISO (mesmo modo, outra chave) o texto velho tem
  // de apagar ate o fim antes de o novo entrar. Sem `trocando`, as duas molas
  // puxavam conteudoA para lados opostos assim que a forma chegava perto da
  // largura nova, e ele parava em ~0,4 com o aviso VELHO meio apagado na
  // pilula do novo (visto no erro que fura a fila, 02/10).
  if (mostra == alvo && vis && !trocando) {
    float perto = fabsf(W - alvoW) < 0.18f * alvoW ? 1.0f : 0.0f;
    conteudoA = anim_mola(conteudoA, perto, dt, 10.0f);
  }
  modalT = molaIlhaWZ(&modalV, modalT, modalAberto ? 1.0f : 0.0f, dt, MODAL_MOLA_W, MODAL_MOLA_Z);
  if (!modalAberto && modalT < 0.01f) { modalT = 0.0f; modalV = 0.0f; }
  for (int i = 0; i < ILHA_MODAL_BOTOES; i++)
    modalFocoA[i] = anim_mola(modalFocoA[i], modalAberto && i == modalFoco ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
  // POSICAO, num ponto so (ilha_ancorar ou o padrao, que e a direita).
  if (ancDef) { x = ancX; y = ancY; dir = ancDir; }
  else { x = NV_TELA_W - NV_ILHA_MARGEM_D / gfx_escala_txt(); y = NV_ILHA_Y / gfx_escala_txt(); dir = 1; }
  // Quem ancora em outro x (testes) desliza em vez de saltar; troca de lado ou
  // ilha apagada assenta seco.
  { static float xSuave; static int xDir = -1;
    if (dir != xDir || A < 0.01f || fabsf(xSuave - x) < 0.5f) { xSuave = x; xDir = dir; }
    else { xSuave = anim_mola(xSuave, x, dt, NV_MOLA_TELA); x = xSuave; } }
  // O alvo do voo e a pilula ASSENTADA (a mola da forma e mais rapida que a
  // do voo: quando o quadro pousa, ela ja esta la).
  { LinhasCartao Lv;
    float pw = voo ? PAD_E + PAD_D + larguraCartao(&cartoes[ILHA_VIVO], ILHA_VIVO, &Lv) : alvoW;
    GfxRect pf = { dir ? x - pw : x, y, pw, NV_ILHA_H_ABERTA };
    vooPasso(agora);
    if (A < 0.01f) {
      if (!vis) { mostra = alvo; dsMostra = dsVez; conteudoA = 0.0f; W = H = NV_ILHA_H * 0.6f; vW = vH = 0.0f; }
      ancDef = 0; ultRectOk = 0; coberta = 0;
      if (voo) { desenharVoo(pf, 0); desenharVoo(pf, 1); }
      return;
    }
    if (voo) desenharVoo(pf, 0);
    vooPf = pf; }
  ancDef = 0;
  { float w = W < H ? H : W, h = H < 8.0f ? 8.0f : H;
    GfxRect r = { dir ? x - w : x, y, w, h }, R = r;
    float raio = 0.5f, cr, cg, cb, fundo = 0.80f, solido = 0.86f, aPil = 1.0f, aMod = 0.0f, aHora = 0.0f;
    int repouso;
    // Crescida pelo medidor (Menor/Grande), o raio para em 32 px: a pilula
    // vira um cartao de cantos redondos, nao uma capsula.
    if (h > NV_ILHA_H_ABERTA) raio = 32.0f / h;
    // O MODAL E A MESMA PILULA CRESCIDA: um retangulo so que vai da forma da
    // pilula ate a do modal na mola subamortecida (o repique e o "pulo" da
    // Dynamic Island), com o raio em pixels indo de meia altura a MD_RAIO. O
    // texto da pilula apaga no comeco, o do modal entra no fim. Sao os mesmos
    // quads da pilula com outro tamanho: nenhuma camada de tela cheia.
    if (modalT > 0.0f) {
      GfxRect M = modalAlvo(r, dir);
      float t = modalT > 1.06f ? 1.06f : modalT, tr = t > 1.0f ? 1.0f : t, rpx;
      R.x = r.x + (M.x - r.x) * t; R.y = r.y + (M.y - r.y) * t;
      R.w = r.w + (M.w - r.w) * t; R.h = r.h + (M.h - r.h) * t;
      rpx = r.h * 0.5f + (MD_RAIO - r.h * 0.5f) * tr;
      // O raio e fracao da ALTURA do retangulo (o SDF de gfx.c mede por ela).
      raio = rpx / (R.h > 1.0f ? R.h : 1.0f);
      fundo = 0.80f + 0.08f * tr;
      solido = 0.86f + 0.08f * tr;
      aPil = 1.0f - modalT * 3.0f; if (aPil < 0.0f) aPil = 0.0f;
      aMod = (modalT - 0.55f) / 0.40f; aMod = aMod < 0.0f ? 0.0f : aMod > 1.0f ? 1.0f : aMod;
      if (anim_politica_reduzida || ajustes_animacoes_reduzidas()) { aPil = modalAberto ? 0.0f : 1.0f; aMod = modalAberto ? 1.0f : 0.0f; }
    }
    // O CORPO E A MESMA PILULA ESTICADA, como o modal: um retangulo so, da
    // pilula ao painel ancorado no canto dela. A hora (relogio sozinho) nao
    // apaga: fica onde esta e vira o cabecalho; o resto da pilula sai no
    // comeco e o corpo entra quando a forma ja esta perto do fim.
    else if (corpoT > 0.0f && corpoFn) {
      int soHora = mostra == M_RELOGIO && dsMostra == DS_DESLIGADO;
      float k3 = (corpoT - 0.08f) * 5.0f;
      GfxRect C = { dir ? x - corpoWa : x, y, corpoWa, corpoHa };
      float t = corpoT > 1.06f ? 1.06f : corpoT, tr = t > 1.0f ? 1.0f : t, rpx;
      R.x = r.x + (C.x - r.x) * t; R.y = r.y + (C.y - r.y) * t;
      R.w = r.w + (C.w - r.w) * t; R.h = r.h + (C.h - r.h) * t;
      rpx = r.h * 0.5f + (MD_RAIO - r.h * 0.5f) * tr;
      raio = rpx / (R.h > 1.0f ? R.h : 1.0f);
      fundo = 0.80f + 0.08f * tr;
      solido = 0.86f + 0.08f * tr;
      // A hora passa a ser desenhada por horaNoLugar (no mesmo ponto): a da
      // pilula sai, a outra fica com o alfa que ela tinha, sem piscar.
      k3 = k3 < 0.0f ? 0.0f : k3 > 1.0f ? 1.0f : k3;
      aPil = soHora ? 0.0f : 1.0f - corpoT * 10.0f;
      if (aPil < 0.0f) aPil = 0.0f;
      aHora = soHora && conteudoA > k3 ? conteudoA : k3;
      // O painel e muito maior que o modal: o conteudo so entra com a forma
      // quase assentada, senao ele aparece cortado pela borda que ainda cresce.
      aMod = (corpoT - 0.78f) / 0.20f; aMod = aMod < 0.0f ? 0.0f : aMod > 1.0f ? 1.0f : aMod;
      if (anim_politica_reduzida || ajustes_animacoes_reduzidas() || gfx_efeitos_minimos())
        aMod = corpoT >= 0.5f ? 1.0f : 0.0f;
    }
    // A PILULA RECEBE O QUADRO: depois do pouso ela cresce ~6% e assenta
    // (ilha_voo_pulso), em volta do proprio centro. So o vidro; o conteudo
    // fica onde estava, para o texto nao tremer.
    if (pousouEm && modalT <= 0.0f && !anim_politica_reduzida && !ajustes_animacoes_reduzidas()) {
      unsigned d = agora - pousouEm;
      float k = ilha_voo_pulso(d);
      if (d >= NV_ILHA_PULSO_MS) pousouEm = 0;
      else { float cx = R.x + R.w * 0.5f, cy = R.y + R.h * 0.5f;
             R.w *= k; R.h *= k; R.x = cx - R.w * 0.5f; R.y = cy - R.h * 0.5f; }
    }
    ultRect = R; ultRectOk = 1;
    repouso = modalT <= 0.0f && corpoT <= 0.0f;
    if (coberta) { coberta = 0; return; }
    // O MODAL ESCURECE A TELA DE TRAS (veu do mockup, 40% / solido 42%): a
    // ilha crescida e a coisa na frente, a home fica atras.
    if (modalT > 0.0f || corpoT > 0.0f) {
      float k = modalT > corpoT ? modalT : corpoT;
      if (k > 1.0f) k = 1.0f;
      gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, (ajustes_vidro() ? 0.40f : 0.42f) * k * A);
    }
    // Sombra caida, curta: separa a pilula de arte clara sem virar halo. No
    // modal ela cresce junto (e o tamanho dele + folga, nunca a tela).
    { float k = modalT > corpoT ? modalT : corpoT;
      k = k > 1.0f ? 1.0f : k < 0.0f ? 0.0f : k;
      if (!(corpoSemSombra && corpoT > 0.0f)) gfx_rect((GfxRect){ R.x - 16.0f - 24.0f * k, R.y - 6.0f - 10.0f * k,
                          R.w + 32.0f + 48.0f * k, R.h + 34.0f + 50.0f * k }, 0, GFX_SOMBRA,
               1.0f, 0, 0, 0.5f, 0, 0, 0, (0.34f + 0.16f * k) * A); }
    // At rest (no modal, no body) the island is the A27 top chrome: the same
    // material and tint as the bar and the avatar chip (gfx_chrome).
    if (repouso) gfx_chrome(R, raio, A);
    else if (ajustes_vidro()) gfx_vidro_material(R, raio, fundo, A);
    else {
      // MATERIAL SOLIDO: a pilula em repouso tem de LER como pilula em paginas
      // escuras. Com 0,055 de cinza ela ficava igual ao fundo da pagina (0,051)
      // e ao escurecido da arte (perfil do amigo, pagina do titulo na TV do
      // dono: "o relogio nao tem pilula aqui"): fica no cinza dos cartoes
      // (~0,12) mais um fio claro, e volta ao escuro do modal ao crescer.
      float mk = modalT > corpoT ? modalT : corpoT;
      float em = mk > 0.0f ? 1.0f - (mk > 1.0f ? 1.0f : mk) : 1.0f;
      float c0 = 0.055f + 0.065f * em, c1 = 0.058f + 0.066f * em, c2 = 0.068f + 0.075f * em;
      gfx_cor(R, raio, c0, c1, c2, (solido + 0.06f * em) * A);
      if (em > 0.01f) gfx_anel(R, raio, 1.5f, 1.0f, 1.0f, 1.0f, 0.085f * em * A);
    }
    desenharFocoRelogio(R, raio, A);
    // O "vidro": um brilho largo e fraco por cima, branco no relogio e na cor
    // do aviso quando ele abre.
    // INFO fica com a luz BRANCA do relogio, parada: e estado, nao novidade
    // (ver corDoTipo). No modal, a luz e a do assunto: vermelha so no erro (o
    // Trakt desconectado), branca no resto — como no mockup.
    { int tipoLuz = -1;
      float luz = 0.07f;
      if (modalT > 0.3f && modalAviso) tipoLuz = modalM.tipo == ILHA_ERRO ? ILHA_ERRO : -1;
      else if (mostra == M_AVISO && mostraA.tipo != ILHA_INFO) tipoLuz = mostraA.tipo;
      if (tipoLuz >= 0) corDoTipo(tipoLuz, &cr, &cg, &cb);
      else { cr = cg = cb = 1.0f; }
      // Aberta, a luz RESPIRA a ~1 Hz (a mesma chamada do toast antigo): "tem
      // algo aqui" sem piscar, que num canto de TV le como defeito.
      if (tipoLuz >= 0)
        luz = ajustes_animacoes_reduzidas() || modalT > 0.3f ? 0.20f
              : 0.14f + 0.10f * (0.5f + 0.5f * sinf((float)agora * (2.0f * 3.14159265f / 1100.0f)));
      // Resting chrome has no white glow; expanded glass has the shared edge light.
      if (tipoLuz >= 0 || (!repouso && !ajustes_vidro()))
        gfx_luz_canto(R, raio, R.w * 0.25f, -R.h * 0.9f, R.w * 0.85f, cr, cg, cb, luz * A); }
    central_segurar_desenhar(R, raio, A, agora);
    gfx_recorte(R.x + 6.0f, R.y, R.w - 12.0f, R.h);
    if (aPil > 0.0f) {
      if (mostra == M_CARTAO) desenharCartao(&mostraC, mostraQual, r, A * conteudoA * aPil);
      else desenharConteudo(mostra, r, A * conteudoA * aPil, agora);
    }
    if (aMod > 0.0f && modalT > 0.0f) desenharModal(modalAlvo(r, dir), A * aMod);
    if (corpoT > 0.0f && corpoFn && modalT <= 0.0f) {
      horaNoLugar(x, dir, y, A * aHora);
      if (aMod > 0.0f) corpoFn((GfxRect){ dir ? x - corpoWa : x, y, corpoWa, corpoHa }, A * aMod, corpoU);
    }
    gfx_sem_recorte();
    // Magic Remote: o clique na pilula com um cartao abre o modal.
    if (modalT <= 0.0f && corpoT <= 0.0f && (mostra == M_CARTAO || (mostra == M_ATIVIDADE && ilha_atividade_expansivel())) && A > 0.5f) ponteiro_alvo(r.x, r.y, r.w, r.h, NULL, pontPilula, 0, 0);
    if (modalT <= 0.0f && corpoT <= 0.0f && mostra == M_RELOGIO && relogioQuer && A > 0.5f) ponteiro_alvo(r.x, r.y, r.w, r.h, NULL, pontRelogio, 0, 0);
    // E num aviso com modal (ou que abre um cartao), o mesmo clique abre o dele.
    if (modalT <= 0.0f && corpoT <= 0.0f && mostra == M_AVISO && temCur && (cur.temModal || cur.cartao || cur.acao) && A > 0.5f)
      ponteiro_alvo(r.x, r.y, r.w, r.h, NULL, pontAviso, 0, 0);
    if (voo) desenharVoo(vooPf, 1);
    salvarVoo(agora, x, y, dir); }
}

static int avisoDoAparelho(const Aviso *a) {
  return !strcmp(a->chave, "atualizacao") || !strcmp(a->chave, "rede") ||
         !strncmp(a->chave, "av:update:", 10) ||
         !strncmp(a->chave, "av:crash:", 9) ||
         !strncmp(a->chave, "av:canal:", 9);
}

void ilha_abrir_sessao(void) {
  Aviso aparelho[FILA + 1];
  int n = 0;
  if (temCur && avisoDoAparelho(&cur)) aparelho[n++] = cur;
  for (int i = 0; i < nFila; i++)
    if (avisoDoAparelho(&fila[i])) aparelho[n++] = fila[i];
  memset(fila, 0, sizeof fila); nFila = 0;
  memset(&cur, 0, sizeof cur); temCur = 0; curAte = 0;
  memset(&mostraA, 0, sizeof mostraA); mostra = -1; mostraChave[0] = 0;
  memset(mostrados, 0, sizeof mostrados); nMostrados = 0;
  for (int i = 0; i < n; i++) {
    if (!temCur) { cur = aparelho[i]; temCur = 1; }
    else enfileirar(&aparelho[i]);
  }
  // Os tres cartoes e suas copias levam titulos/rostos do perfil anterior.
  memset(cartoes, 0, sizeof cartoes); memset(temCartao, 0, sizeof temCartao);
  memset(&mostraC, 0, sizeof mostraC); cartaoVez = mostraQual = -1;
  memset(&modalC, 0, sizeof modalC); memset(&modalM, 0, sizeof modalM);
  modalAberto = modalAviso = modalAtividade = modalFoco = modalAvisoMedido = 0;
  modalQual = -1; modalChave[0] = 0; modalDesde = 0; modalT = modalV = 0.0f;
  memset(modalFocoA, 0, sizeof modalFocoA);
  pedido = avPedido = 0; pedidoQual = -1; avPedidoChave[0] = 0;
  memset(&pedidoC, 0, sizeof pedidoC);
  atvTexto[0] = atvTitulo[0] = atvDetalhes[0] = atvIcone[0] = 0;
  atvVisto = 0; atvV2 = atvTemCarga = 0; atvProg = -1.0f; atvBarraA = 0.0f;
  memset(&atvCarga, 0, sizeof atvCarga);
  corpoPed = 0; corpoFn = NULL; corpoU = NULL;
  corpoW = corpoH = corpoT = corpoV = corpoWa = corpoHa = corpoVw = corpoVh = 0.0f;
  voo = vooDissolve = vooAlvoOk = svooQuer = svoo = 0;
  vooDesde = svooDesde = pousouEm = altBase = 0; vooT = svooT = 0.0f;
  vooArte[0] = vooCapa[0] = svooPoster[0] = 0; svooIconeA = 1.0f;
  pontoEnquete = pontoPediu = coberta = ultRectOk = relogioFocado = 0;
  W = vW = H = vH = A = conteudoA = 0.0f; ultQuadro = 0;
  dsMostra = dsVez = dsCartao = 0;
#ifdef NV_TOUCH_UI
  modalToqueReiniciar();
#endif
}
