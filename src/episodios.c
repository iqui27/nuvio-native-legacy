#include "logotitulo.h"
#include "episodios.h"
#include "ajustes.h"   /* ajustes_acento: a cor do check da confirmacao */
#include "idioma.h"
#include "catalogo.h"
#include "descoberta.h"
#include "extras.h"
#include "gfx.h"
#include "tex_cache.h"
#include "text.h"
#include "sinopse.h"
#include "layout.h"
#include "anim.h"
#include "vistoep.h"
#include "vistonao.h"
#include "visto.h"
#include "simkl.h"   // SMK_LOTE_MAX: menor limite de envio dos destinos
#include "botoes.h"
#include <stdio.h>
#include <string.h>
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "telefoneui.h"
#include "plrui.h"
#include "progresso.h"
#include "plrilha.h"
#include "ctxmenu.h"
#include "epcoment.h"
#include "reacao.h"
#include "movimento.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"

// OS EPISODIOS NO PLAYER NASCEM DA ILHA DO RELOGIO (pedido do dono, 03/10),
// como Audio e Legendas (faixas.c, plrilha.h): a pilula da hora cresce ate a
// lista e a linha dela fica de cabecalho. Era uma folha colada na direita.
// Corpo de EP_W: kicker com a serie + "Episodios" e o disco de fechar, o
// segmentado das temporadas, EP_VIS linhas de EP_ROW (still 240x135 com o
// progresso e o disco de assistido sobre ela) e o rodape com a posicao e a
// dica do "Segure OK". O menu de visto continua MODAL, dentro da ilha.
#define EP_W     820.0f
#define EP_PAD_X  18.0f
#define EP_PAD_Y  22.0f
#define EP_TIT_H  42.0f
#define EP_SEG_H  54.0f
#define EP_ROW   160.0f
// Linhas a vista: 4, ou menos quando a ilha nao cabe na altura da tela virtual
// (escala.h: 3 em 120%, 2 em 150%). O miolo fixo da ilha sao 251, a pilula da
// hora por cima ~60, e as margens de cima e de baixo.
static int epVis(void) {
  int n = (int)((NV_TELA_H - 48.0f - 40.0f - 60.0f - 251.0f) / 160.0f);
  return n > 4 ? 4 : n < 2 ? 2 : n;
}
#define EP_VIS    epVis()
#define EP_PE_H   47.0f
#define EP_VAZIO 120.0f
static int aberto, titulo, atualT, atualE, temporada, foco, grupo;
static int pedidoT, pedidoE;
static float anim, scroll;
// Velocidade da rolagem de 2a ordem (anim_mola2): partida macia, como na home.
static float velScroll;
// O REALCE DA LINHA E O FOCO DOS CHIPS DE TEMPORADA em mola: o dono diz que
// salta e "duro". A linha que ganha foco acende em ~120 ms (NV_MOLA_FOCO) e a
// que perde deixa um pos-brilho curto; a pilula do segmentado esmaece ao
// entrar/sair da fileira de chips em vez de acender num quadro. So DOIS
// floats (linha atual + anterior): o custo por quadro nao depende do tamanho
// da lista — TV, nao workstation.
static float focoLin, focoLinAnt, segFoc;
static int focoLinIdx = -1, focoLinAntIdx = -1;
static void focoLinhaPasso(float dt) {
  int alvo = (grupo == 1 && aberto) ? foco : -1, red = ajustes_animacoes_reduzidas();
  if (alvo != focoLinIdx) { focoLinAntIdx = focoLinIdx; focoLinAnt = focoLin; focoLinIdx = alvo; focoLin = 0.0f; }
  focoLin = red ? (alvo >= 0 ? 1.0f : 0.0f)
                : anim_mola(focoLin, alvo >= 0 ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
  if (focoLinAntIdx >= 0) {
    focoLinAnt = red ? 0.0f : anim_mola(focoLinAnt, 0.0f, dt, NV_MOLA_DESFOCO);
    if (focoLinAnt < 0.005f) focoLinAntIdx = -1;
  }
  segFoc = red ? (grupo == 0 ? 1.0f : 0.0f)
               : anim_mola(segFoc, grupo == 0 ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
}
#ifdef NV_TOUCH_UI
static ToqueRolagem toqueEp, toqueEpTemporadas;
static float toqueEpTemporadasOffset;
static int toqueEpRolar(const PonteiroRolagem *e);
#endif
static int localizarAtual;
// ONDE O FOCO TEM DE ESTAR, POR NUMERO DE EPISODIO — issue #102.
//
// `foco` e uma LINHA, e linha depende da lista existir. Quando o catalogo
// remonta com a folha aberta (cat_definir_tudo zera nEps de proposito, e a
// descoberta republica sozinha de tempos em tempos), a lista fica vazia por
// alguns quadros: o clamp la embaixo puxava `foco` para 0 e, quando os
// episodios voltavam, a folha estava no primeiro da temporada. E exatamente o
// "volta sozinho para o E1 se a pessoa demorar" do relato — o que segura a
// pessoa parada e o menu de visto por cima, nao ele que causa o salto.
//
// Guardando o NUMERO do episodio o foco sobrevive ao desaparecimento da lista:
// quando ela volta, a linha e reencontrada.
static int alvoE;
// A ROLAGEM DO PRIMEIRO QUADRO E A FINAL, sem mola — a outra metade do #102.
// A folha nascia com scroll=0 e a mola levava ate o episodio atual: para quem
// olha, ela "carrega no E1 e depois salta". Abrir ja no lugar nao e animacao
// mais rapida, e animacao nenhuma.
static int semMolaScroll;

// --- MENU DE VISTO -----------------------------------------------------------
//
// Segurar OK numa linha abre. Este episodio, ate aqui e a temporada inteira
// usam o mesmo lote (vistoep.h), com o sentido dado pelo episodio em foco.
// Daqui em diante so DESMARCA: aparece se ha visto do foco para a frente.
// VM_FONTES so aparece quando o menu e aberto DE FORA (da pagina de detalhe):
// la o toque curto no card ja abre as fontes e a pressao longa passou a abrir
// este menu, entao ele precisa carregar a porta que tomou. Dentro da folha de
// episodios a opcao nao existe — quem abriu a folha veio do player e ja tem
// fonte tocando; oferece-la ali seria uma linha que nao leva a lugar nenhum.
enum { VM_ESTE = 0, VM_ATE, VM_TEMP, VM_FONTES, VM_DAQUI, VM_SINOPSE, VM_NOTA, VM_COMENT, VM_N };
static int vmAcoes[VM_N], vmN; // linhas visiveis -> gesto (daqui em diante e opcional)
// O MENU DA TEMPORADA (issue #108, "Pressing 'Season' brings up option to mark
// all as watched"): o mesmo cartao, aberto pela ABA e nao por um episodio, com
// so as duas linhas que o dono pediu. vmModoTemp escolhe qual dos dois; as
// opcoes sao indices proprios porque nao ha "este" nem "ate aqui" numa aba.
enum { VT_MARCAR = 0, VT_DESMARCAR, VT_N };
static int vmModoTemp;
static int vmQuantos[VM_N];   // tamanho do lote de cada opcao, da abertura
static int montarLote(int idx, int modo, int t, int e, VistoPar *saida, int max);
// Consulta a faixa inteira; a aplicacao limita o gesto e os envios.
#define VM_LOTE VE_LOTE_MAX
static int vmAberto, vmFoco, vmVisto;      // vmVisto: o sentido do gesto
static Uint32 vmDesde;                     // relogio da pressao longa
static int vmSegurando, vmConsumir;
// CONFIRMACAO NA TELA DEPOIS DE APLICAR: o menu nao some no mesmo quadro; por
// FEITO_MS ele mostra um check e "N episodios marcados" e so entao fecha. Sem
// isto a unica prova de que o OK entrou era procurar o "Visto" na lista, e o
// #70 mostrou que a pessoa apertava de novo.
#define FEITO_MS 900
static int vmFeito, vmFeitoN, vmFeitoVisto; static Uint32 vmFeitoAte;

// O ALVO DO MENU E EXPLICITO, e nao lido do estado da folha.
//
// Ele dependia de `titulo`, `foco`, `temporada` e epLinha(foco) — tudo interno
// desta tela —, e por isso so podia existir dentro dela. Guardando o alvo o
// menu passa a servir tambem a pagina de detalhe, onde a folha nem esta aberta.
static int  vmIdx = -1, vmT, vmE;
static char vmNome[120], vmSinopse[420];
// O TITULO, e nao so a posicao (#190). `titulo` e `vmIdx` sao indices no
// catalogo, e o catalogo e refeito com a folha aberta — a refacao de
// "Continuar assistindo" roda com o player tocando. A mesma posicao passava a
// ser de outra serie, e a folha listava os episodios dela ("Attack on Titan
// trocado por Knights of Guinevere no menu de episodios"). Guardado na
// abertura e conferido por revalidar() antes de cada uso.
static char idTitulo[64], vmId[64];
static void guardarId(char *dst, size_t tam, int idx) {
  const CatItem *ci = cat_item(idx);
  snprintf(dst, tam, "%s", ci ? ci->imdb : "");
}
static void revalidar(void) {
  int i;
  if (idTitulo[0] && (i = cat_indice_vivo(titulo, idTitulo)) >= 0) titulo = i;
  if (vmId[0] && (i = cat_indice_vivo(vmIdx, vmId)) >= 0) vmIdx = i;
}
// A MINIATURA DO EPISODIO, resolvida UMA vez na abertura. O cartao mostra a
// arte do episodio de que ele fala — sem ela o menu e quatro linhas de texto
// que poderiam ser de qualquer titulo. Procurar no catalogo a cada quadro
// custaria uma varredura por episodio 60 vezes por segundo para desenhar uma
// imagem que nao muda enquanto o menu estiver aberto.
static int  vmSo;        // 1 = aberto sozinho, sobre outra tela
static int  vmFontesPed; // consumido por episodios_menu_pediu_fontes()
// O MENU E DESENHADO POR ctxmenu.c (ctx_menu_desenhar): a mesma ilha do menu do
// cartaz, com as mesmas medidas. Aqui ficam so o que escrever e as molas.
// TUDO MONTADO NA ABERTURA — rotulos, cabecalho, largura — e nao a cada quadro:
// o texto so muda quando o menu abre ou a acao termina.
static char     vmRot[VM_N][120], vmCab[160], vmMsg[120];
static CtxLinha vmLin[VM_N];
static int vmComentEstado;
static int vmTroca, vmDirecao;
static float vmTransicao=1;
static CatEp vmPendente;
static int vmMudar;
static const CatEp *vmVizinho(int dir) {
  const CatEp *best=NULL;
  for (int i=0;i<cat_n_episodios(vmIdx);i++) {
    const CatEp *ep=cat_episodio(vmIdx,i);
    if (!ep) continue;
    int order=ep->temporada==vmT ? ep->episodio-vmE : ep->temporada-vmT;
    if ((dir>0 && order<=0) || (dir<0 && order>=0)) continue;
    if (!best || (dir>0 ? (ep->temporada<best->temporada || (ep->temporada==best->temporada && ep->episodio<best->episodio))
                        : (ep->temporada>best->temporada || (ep->temporada==best->temporada && ep->episodio>best->episodio)))) best=ep;
  }
  return best;
}
static char vmComentContagem[24];
static float    vmLarg;
static float vmTituloEscala=1, vmTituloLarg;
static float    vmAnim, vmAnimV, vmFocoAnim[VM_N];
// DE ONDE O MENU SAIU, na tela virtual: o card do episodio (a ilha nasce ao
// lado dele, como a do cartaz) ou a ABA da temporada (que se abre em painel).
// Quem abriu avisa a cada quadro (episodios_menu_ancora); sem ancora, o meio.
static GfxRect  vmAncora;
static int      vmTemAncora;
static GfxRect  vmCaixa;       // a caixa aberta do ultimo quadro (teste)
static int      vmTemCaixa;

static int vmOpcoes(void) { return vmModoTemp ? VT_N : vmN; }
// A ABA QUE SE ABRE: o menu da temporada, aberto sobre a pagina de titulo com a
// aba por ancora. Dentro da ilha do player a "aba" e um segmentado da propria
// ilha, e la o menu da temporada e a ilha comum.
static int vmPainel(void) { return vmModoTemp && vmSo && vmTemAncora; }

// Os rotulos do menu aberto. O NUMERO NO ROTULO: "marcar 7 episodios" e outra
// decisao que "marcar 1". Sai do MESMO montarLote da acao, contado na abertura.
static void montarRotulos(void) {
  int i, n = vmOpcoes();
  for (i = 0; i < n; i++) {
    int modo = vmAcoes[i];
    int quantos = vmModoTemp ? vmQuantos[VM_TEMP] : vmQuantos[modo];
    const char *ic;
    if (vmModoTemp) {
      snprintf(vmRot[i], sizeof vmRot[i], i == VT_MARCAR ? i18n("Marcar temporada como assistida (%d)")
                                                         : i18n("Desmarcar temporada (%d)"), quantos);
      ic = i == VT_MARCAR ? "aj_eye" : "aj_eye-off";
    } else if (modo == VM_ESTE) {
      snprintf(vmRot[i], sizeof vmRot[i], "%s", vmVisto ? i18n("Marcar este episódio") : i18n("Desmarcar este episódio"));
      ic = vmVisto ? "aj_eye" : "aj_eye-off";
    } else if (modo == VM_ATE) {
      snprintf(vmRot[i], sizeof vmRot[i], quantos == 1 ? i18n("Até aqui (%d episódio)") : i18n("Até aqui (%d episódios)"), quantos);
      ic = "pl_skip-forward";
    } else if (modo == VM_DAQUI) {
      snprintf(vmRot[i], sizeof vmRot[i], "%s", i18n("Desmarcar daqui em diante"));
      ic = "aj_eye-off";
    } else if (modo == VM_TEMP) {
      snprintf(vmRot[i], sizeof vmRot[i], quantos == 1 ? i18n("Temporada inteira (%d episódio)") : i18n("Temporada inteira (%d episódios)"), quantos);
      ic = "aj_list-video";
    } else if (modo == VM_NOTA) {
      snprintf(vmRot[i], sizeof vmRot[i], "%s", i18n("Sua nota"));
      ic = "aj_star";
    } else if (modo == VM_COMENT) {
      snprintf(vmRot[i], sizeof vmRot[i], "%s", i18n("Comentários"));
      ic = "aj_message-square";
    } else if (modo == VM_SINOPSE) {
      snprintf(vmRot[i], sizeof vmRot[i], "%s", i18n("Ler sinopse"));
      ic = "aj_file-text";
    } else {
      snprintf(vmRot[i], sizeof vmRot[i], "%s", i18n("Fontes deste episódio"));
      ic = "aj_layers";
    }
    vmLin[i].rot = vmRot[i]; vmLin[i].icone = ic;
  }
  if (vmModoTemp) snprintf(vmCab, sizeof vmCab, i18n("Temporada %d"), vmT);
  else snprintf(vmCab, sizeof vmCab, i18n("T%dE%d · %s"), vmT, vmE, vmNome);
  vmLarg = ctx_menu_largura(vmLin, n);
  vmTituloLarg=0;
}

// Abre o menu para um episodio qualquer. `t` e o NUMERO da temporada, nao o
// indice da aba: quem chama de fora nao tem abas.
static void menuAbrir(int idx, int t, int e, const char *nome, int so) {
  const CatItem *ci = cat_item(idx);
  if (!ci || !ci->imdb[0]) return;
  vmIdx = idx; vmT = t; vmE = e; vmSo = so; vmModoTemp = 0;
  vmComentEstado = -2; vmTransicao=1; vmTroca=vmMudar=0;
  if (so && e > 0) epcoment_abrir(ci->imdb, t, e);
  guardarId(vmId, sizeof vmId, idx);
  snprintf(vmNome, sizeof vmNome, "%s", nome ? nome : "");
  vmTemAncora = 0;
  // O SENTIDO SAI DO ESTADO: quem esta olhando um episodio visto quer
  // desmarcar. Desconhecido (-1) conta como nao visto.
  vmVisto = vistoep_estado(ci->imdb, t, e) == 1 ? 0 : 1;
  // vmConsumir FICA EM ZERO. O menu abre no KEYUP da pressao longa — o OK que
  // o abriu ja foi SOLTO quando se chega aqui — e com 1 o proximo KEYDOWN, que
  // e a escolha de verdade, era engolido como se fosse esse soltar. Era o
  // "precisa apertar duas vezes em toda opcao" do #70, desde o dia em que o
  // menu nasceu; a regra de ctxmenu.c que isto copiava abre no KEYDOWN, e la
  // consumir o soltar faz sentido.
  vmAberto = 1; vmFoco = 0; vmConsumir = 0; vmFontesPed = 0;
  vmFeito = 0;
  vmQuantos[VM_ESTE] = 1;
  vmQuantos[VM_ATE]  = montarLote(idx, VM_ATE, t, e, NULL, 0);
  vmQuantos[VM_TEMP] = montarLote(idx, VM_TEMP, t, e, NULL, 0);
  vmQuantos[VM_FONTES] = 0;
  vmN = 0;
  vmAcoes[vmN++] = VM_ESTE;
  vmAcoes[vmN++] = VM_ATE;
  // Pode haver posteriores vistos mesmo quando o episodio em foco nao esta.
  { VistoPar lote[VM_LOTE];
    int n = montarLote(idx, VM_DAQUI, t, e, lote, VM_LOTE), i;
    vmQuantos[VM_DAQUI] = n;
    for (i = 0; i < n; i++)
      if (vistoep_estado(ci->imdb, lote[i].temporada, lote[i].episodio) == 1) {
        vmAcoes[vmN++] = VM_DAQUI;
        break;
      }
  }
  vmAcoes[vmN++] = VM_TEMP;
  if (so && e > 0) vmAcoes[vmN++] = VM_NOTA;
  if (so) vmAcoes[vmN++] = VM_FONTES;
  vmSinopse[0] = 0;
  if (e > 0) {
    for (int i = 0; i < cat_n_episodios(idx); i++) {
      const CatEp *ep = cat_episodio(idx, i);
      if (ep && ep->temporada == t && ep->episodio == e) {
        snprintf(vmSinopse, sizeof vmSinopse, "%s", ep->sinopse); break;
      }
    }
    if (vmSinopse[0]) vmAcoes[vmN++] = VM_SINOPSE;
  }
  montarRotulos();
  float fixedWidth=vmLarg;
  // Abre do zero: a mola de um menu que acabou de fechar nao e a deste.
  vmAnim = vmAnimV = 0.0f;
  memset(vmFocoAnim, 0, sizeof vmFocoAnim);
  vmMsg[0] = 0;
  montarRotulos(); vmLarg=fixedWidth;
}

// UP from the first action focuses the header. LEFT/RIGHT there navigates
// episodes; RIGHT on an action still enters reviews. Boundaries never wrap.
static void vmNavegar(int dir) {
  if (vmTransicao<1) return; // Held keys must not restart a transition before it can advance.
  const CatEp *ep=vmVizinho(dir);
  if (!ep) return;
  vmPendente=*ep; vmMudar=1; vmDirecao=dir;
  vmTransicao=(ajustes_animacoes_reduzidas() || anim_politica_reduzida)?1:0;
}

static int nTemporadas(void) {
  const CatItem *c = cat_item(titulo);
  return c && c->nTemporadas > 0 ? c->nTemporadas : 1;
}
static int numTemporada(int i) {
  const CatItem *c = cat_item(titulo);
  return c && c->nTemporadas > 0 ? c->temporadas[i] : atualT;
}
static const CatEp *epLinha(int linha) {
  int n = cat_n_episodios(titulo);
  for (int i = 0, j = 0; i < n; i++) {
    const CatEp *ep = cat_episodio(titulo, i);
    if (ep && ep->temporada == numTemporada(temporada) && j++ == linha) return ep;
  }
  return NULL;
}
static int nLinhas(void) {
  int n = 0;
  for (int i=0;i<cat_n_episodios(titulo);i++) {
    const CatEp *e=cat_episodio(titulo,i);
    if(e && e->temporada==numTemporada(temporada)) n++;
  }
  return n;
}
// O LOTE SAI DO CATALOGO E DO MAPA, JUNTOS.
//
// Antes saia so do mapa (vistoep_temporada / vistoep_ate_aqui), e o mapa so
// ENUMERA a serie quando o Trakt respondeu /shows/<id>/progress/watched. Sem
// Trakt ele so conhece o que a conta disse que foi visto — entao "Temporada
// inteira" mostrava "(0 episodios)" e o OK nao fazia nada. Era a metade
// episodica do "sem o traktv nao ta dando o watched".
//
// O catalogo (Cinemeta `videos`) lista os episodios que existem; o mapa
// acrescenta o que o Trakt conhece e o catalogo nao (especiais). Episodio que
// AINDA NAO FOI AO AR fica fora (a agenda do TMDB da visita, a mesma regra de
// epNaoExibido em detail.c): marcar como visto o que nao saiu e o erro que o
// Trakt aceita calado. Temporada 0 do catalogo fica fora do "ate aqui" — o
// especial so entra se o mapa o trouxer, como ja era.
//
// `saida` NULA = so contar, como em vistoep.h; o rotulo precisa do numero.
// A regra mora em vistoep_lote (pura, com teste); aqui so se junta o catalogo.
static int montarLote(int idx, int modo, int t, int e, VistoPar *saida, int max) {
  static VistoPar cat[VM_LOTE];
  const CatItem *ci = cat_item(idx);
  int i, nc = 0;
  if (!ci || !ci->imdb[0]) return 0;
  if (modo == VM_ESTE) {
    if (saida && max > 0) { saida[0].temporada = (short)t; saida[0].episodio = (short)e; }
    return 1;
  }
  for (i = 0; i < cat_n_episodios(idx) && nc < VM_LOTE; i++) {
    const CatEp *ce = cat_episodio(idx, i);
    if (!ce) continue;
    cat[nc].temporada = (short)ce->temporada;
    cat[nc].episodio = (short)ce->episodio;
    nc++;
  }
  return vistoep_lote(ci->imdb, modo == VM_DAQUI ? VE_LOTE_DAQUI :
                      modo == VM_ATE ? VE_LOTE_ATE : VE_LOTE_TEMPORADA, t, e, cat, nc,
                      extras_agenda_temporada(), extras_agenda_episodio(),
                      saida, max);
}

// Aplica o gesto: monta o lote, muda o local na hora, e manda o resto para um
// fio (visto.c — Trakt, Simkl e conta, o que estiver vinculado). Devolve 0
// quando nao ha nada a fazer — e o caso de marcar o que ja esta marcado, que
// nao deve gastar uma requisicao.
//
// SO VAI O QUE MUDOU. Antes o lote inteiro ia quando algo mudava, para o caso
// de o mapa dizer "visto" por um destino (Trakt) e o outro (Simkl) nao saber.
// Mas o /sync/history do Trakt sem watched_at CRIA UM PLAY NOVO "DE AGORA" por
// episodio: "Ate aqui" em T2E5 re-postava a T1 inteira (Silo, 08/10, 15 em vez
// de 1). Agora vistoep_aplicar devolve so os episodios cujo estado mudou; um
// de estado DESCONHECIDO (mapa vazio, sem Trakt lido) conta como mudou, que e o
// caso em que o remoto ainda precisa do reparo. Pedidos em lotes por destino,
// nunca por episodio. Desmarcar idem: so sai quem estava visto. Nada mudou: nada sai.
static const char *nomeModo(int modo) {
  return modo == VM_ESTE ? "este" : modo == VM_ATE ? "ate aqui" :
         modo == VM_DAQUI ? "daqui em diante" : "temporada";
}
static int aplicarVisto(int modo, int visto) {
  const CatItem *ci = cat_item(vmIdx);
  VistoPar lote[VM_LOTE], envio[VM_LOTE];
  int n, mudou, ja = 0;
  if (!ci || !ci->imdb[0]) return 0;
  // A TEMPORADA DO EPISODIO (vmT), e nao a da aba selecionada. Sao a mesma
  // coisa dentro da folha, e fora dela nao ha aba nenhuma.
  n = montarLote(vmIdx, modo, vmT, vmE, lote, VM_LOTE);
  // Faixa grande: so o que falta mudar, para o proximo gesto poder avancar.
  if (n > VISTONAO_MAX) {
    int k = 0;
    for (int i = 0; i < n && k < VISTONAO_MAX; i++)
      if (vistoep_estado(ci->imdb, lote[i].temporada, lote[i].episodio) != visto)
        lote[k++] = lote[i];
    n = k;
  }
  if (n < 1) return 0;
  mudou = vistoep_aplicar(ci->imdb, lote, n, visto, envio, &ja);
  printf("[visto] menu: %s %s %s S%dE%d: %d enviados, %d ja estavam\n",
         nomeModo(modo), visto ? "marcar" : "desmarcar", ci->imdb, vmT, vmE, mudou, ja);
  fflush(stdout);
  if (!mudou) return 0;
  if (visto) { printf("[visto] marcado por episodios-manual pos=0 dur=0 assistido=0s id=%s n=%d\n", ci->imdb, mudou); fflush(stdout); }
  for (int i = 0; i < mudou; i += SMK_LOTE_MAX) {
    int qtd = mudou - i;
    if (qtd > SMK_LOTE_MAX) qtd = SMK_LOTE_MAX;
    visto_episodios_item(vmIdx, ci->imdb, ci->tipo[0] ? ci->tipo : "series",
                         envio + i, qtd, visto, visto_destinos());
  }
  return mudou;
}

// Abre o menu da TEMPORADA `t` (numero, nao indice de aba). O foco nasce em
// "Desmarcar" so quando a temporada inteira ja esta vista: e o unico caso em
// que marcar nao mudaria nada.
static void menuAbrirTemporada(int idx, int t, int so) {
  const CatItem *ci = cat_item(idx);
  VistoPar lote[VM_LOTE];
  int n, i, vistos = 0;
  if (!ci || !ci->imdb[0] || t < 0) return;
  menuAbrir(idx, t, 0, ci->titulo, so);
  if (!vmAberto) return;
  vmModoTemp = 1;
  n = montarLote(idx, VM_TEMP, t, 0, lote, VM_LOTE);
  for (i = 0; i < n; i++)
    if (vistoep_estado(ci->imdb, lote[i].temporada, lote[i].episodio) == 1) vistos++;
  vmFoco = (n > 0 && vistos == n) ? VT_DESMARCAR : VT_MARCAR;
  montarRotulos();
}

void episodios_abrir(int idx, int t, int e) {
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueEp); toquerol_limpar(&toqueEpTemporadas); toqueEpTemporadasOffset = 0;
#endif
  titulo = idx; atualT = t; atualE = e; aberto = 1;
  guardarId(idTitulo, sizeof idTitulo, idx);
  temporada = foco = 0; grupo = 1; pedidoE = 0; scroll = 0;
  focoLinIdx = focoLinAntIdx = -1; focoLin = focoLinAnt = 0.0f; segFoc = 0.0f;
  vmAberto = 0; vmSegurando = 0; vmConsumir = 0;
  localizarAtual = 1; alvoE = e; semMolaScroll = 1;
  for (int i = 0; i < nTemporadas(); i++) if (numTemporada(i) == t) temporada = i;
  for (int i = 0; i < nLinhas(); i++) if (epLinha(i)->episodio == e) foco = i;
  desc_episodios(titulo, t);
}
int episodios_aberto(void) { return aberto; }
float episodios_anim(void) { return anim; }
int   episodios_foco_linha(void) { return foco; }
float episodios_rolagem(void) { return scroll; }
void episodios_fechar(void) { aberto = 0; sinopse_fechar(); }
int  episodios_titulo(void) { revalidar(); return titulo; }
int episodios_escolheu(int *t, int *e) {
  if (!pedidoE) return 0;
  *t = pedidoT; *e = pedidoE; pedidoE = 0; return 1;
}
// O menu em si, separado de quem o hospeda: a folha chama daqui, e a pagina de
// detalhe chama por episodios_menu_evento(). As duas regras abaixo o menu do
// cartaz aprendeu do jeito dificil (ver ctxmenu.c): repeticao automatica nunca
// e uma segunda escolha, e o OK que ABRIU o menu nao escolhe nada.
// O MENU, DESENHADO ONDE MANDAREM. Dentro da folha ele e centrado sobre a
// COLUNA dela (centrar em NV_TELA_W poria o menu sobre o vazio da esquerda,
// longe do episodio a que se refere); aberto sobre a pagina de detalhe, a
// caixa e a tela inteira. A altura sai do numero de opcoes, que muda com isso.
// PONTEIRO (#99) no menu de visto: opcao sob o cursor ganha o foco (vmFoco,
// o mesmo de cima/baixo), clique e o OK, clique fora fecha o menu.
static void ponteiroVmOpcao(int i, int b) { (void)b; if (i >= 0 && i < vmOpcoes()) { vmFoco = i; epcoment_menu(); } }
static void ponteiroVmFora(int a, int b) { (void)a; (void)b; vmAberto = 0; if (vmSo && !vmModoTemp) extras_comentarios_ep_cancelar(); }

// O MENU DE VISTO (segurar OK) E A ILHA DO MENU DO CARTAZ (ctxmenu.c): o nome
// "T1E4 · nome", embaixo o que o gesto faz ("Marcar como assistido") e as
// acoes nas mesmas linhas de 60. Era um cartao proprio de 800 com a still no
// cabecalho e rodape de teclas (dono, 08/10: "padronizar com o menu de
// contexto"). No player ele fica DENTRO da ilha dos episodios (`area` e o
// retangulo dela, que o desenho ja recorta), logo abaixo do cabecalho; sobre a
// pagina de titulo, ao lado do card do episodio, que a pagina redesenha por
// cima do veu — o mesmo arranjo do cartaz na home.
//
// A CONFIRMACAO mora na linha de apoio do cabecalho, como o estado da escrita
// no menu do cartaz, e as linhas apagam: nao ha mais o que escolher.
#define VM_TOPO_ILHA 104.0f   // do topo da ilha ao modal: o cabecalho (64) + 40
#define VM_BORDA      48.0f   // margem minima da tela, a do menu do cartaz
static float menuAltura(void) { return ctx_menu_altura(vmOpcoes()) + (vmSo && !vmModoTemp ? 30 : 0); }
static float menuLargura(float cabe) { return vmLarg < cabe ? vmLarg : cabe; }

// As molas do menu: a de abertura e a do foco de cada linha. O painel da
// temporada anda na mola da pilula da ilha (movimento.h) — a forma repica um
// pouco, o texto nao —; o menu comum, na das telas, como o do cartaz.
static void menuAtualizar(float dt) {
  int i, red = ajustes_animacoes_reduzidas();
  if (vmTransicao<1) { vmTransicao+=dt/.180f; if (red || anim_politica_reduzida || vmTransicao>1) vmTransicao=1; }
  if (vmMudar && vmTransicao>=.5f) {
    CatEp next=vmPendente; GfxRect anchor=vmAncora; int has=vmTemAncora;
    float phase=vmTransicao, width=vmLarg, opening=vmAnim;
    extras_comentarios_ep_cancelar();
    menuAbrir(vmIdx,next.temporada,next.episodio,next.nome,1);
    vmLarg=width; vmAnim=opening; vmFoco=-1; vmTemAncora=has; vmAncora=anchor;
    vmTroca=1; vmTransicao=phase;
  }
  if (vmAberto && vmSo && !vmModoTemp) {
    epcoment_atualizar(dt);
    int n = extras_n_comentarios_ep();
    if (!n && extras_comentarios_ep_carregando()) n=-1;
    if (n != vmComentEstado) {
      if (vmN && vmAcoes[vmN - 1] == VM_COMENT) vmN--;
      if (n != 0) vmAcoes[vmN++] = VM_COMENT;
      if (vmFoco >= vmN) vmFoco = vmN - 1;
      vmComentEstado = n;
      if (n < 0) snprintf(vmComentContagem, sizeof vmComentContagem, "%s", "…");
      else snprintf(vmComentContagem, sizeof vmComentContagem, "%d", n);
      float width=vmLarg; montarRotulos(); vmLarg=width;
    }
  }
  float alvo = vmAberto ? 1.0f : 0.0f;
  if (red) { vmAnim = alvo; vmAnimV = 0.0f; }
  else if (vmPainel()) {
    vmAnim = mov_mola_assenta(&vmAnimV, vmAnim, alvo, dt, MOV_PILULA_W, MOV_PILULA_Z);
    if (vmAnim < 0.0f) { vmAnim = 0.0f; vmAnimV = 0.0f; }
  } else vmAnim = anim_mola(vmAnim, alvo, dt, NV_MOLA_TELA);
  for (i = 0; i < VM_N; i++) {
    float f = vmAberto && !vmFeito && i == vmFoco ? 1.0f : 0.0f;
    vmFocoAnim[i] = red ? f : anim_mola(vmFocoAnim[i], f, dt, NV_MOLA_FOCO);
  }
}
static int menuVisivel(void) { return vmAberto || vmAnim > 0.01f; }

// A ABA DA TEMPORADA SE ABRE EM PAINEL (dono, 08/10): segurar OK numa aba nao
// chama um modal no meio da tela — o PREENCHIMENTO DA PROPRIA ABA (a pilula
// cheia do foco, no realce) cresce para baixo e para o lado, na mesma mola, ate
// virar o painel que guarda as acoes da temporada. Nao e uma superficie nova
// por baixo dela: e a aba aumentando. Ela continua no lugar, de cabecalho (a
// pagina a redesenha por cima, com o mesmo preenchimento), e as linhas acendem
// no fim do caminho, recortadas pelo preenchimento enquanto ele cresce, na
// TINTA DO REALCE (ajustes_acento_tinta): escura sobre o realce claro, clara
// sobre o escuro — a mesma que o rotulo da aba usa.
//
// O resumo "N de M assistidos" NAO entra: a pagina ja o mostra logo acima das
// abas, e repetido ao lado da aba era a mesma frase duas vezes.
//
// CUSTO: um retangulo cheio, duas linhas e uma frase. Sem veu de tela cheia — a
// pagina continua inteira por tras, e o que diz "modal" e o foco ter saido dela.
#define PT_PAD   10.0f    // da borda do painel a aba e as linhas
#define PT_RAIO  30.0f
static GfxRect painelFinal(void) {
  float passo = ctx_menu_passo();
  GfxRect f = { vmAncora.x - PT_PAD, vmAncora.y - PT_PAD, vmLarg,
                PT_PAD + vmAncora.h + PT_PAD + (float)VT_N * passo + PT_PAD - 4.0f };
  if (f.w < vmAncora.w + PT_PAD * 2.0f) f.w = vmAncora.w + PT_PAD * 2.0f;
  if (f.x + f.w > NV_TELA_W - VM_BORDA) f.x = NV_TELA_W - VM_BORDA - f.w;
  if (f.x < VM_BORDA) f.x = VM_BORDA;
  return f;
}
static void painelDesenhar(float a) {
  GfxRect c = vmAncora, f = painelFinal(), r;
  float e = vmAnim, ec = anim_clamp(e, 0.0f, 1.0f), raio, ca, passo = ctx_menu_passo();
  float fr, fg, fb, ti = ajustes_acento_tinta(&fr, &fg, &fb);
  int i, tin = (int)(ti * 255.0f + 0.5f);
  r.x = c.x + (f.x - c.x) * e; r.y = c.y + (f.y - c.y) * e;
  r.w = c.w + (f.w - c.w) * e; r.h = c.h + (f.h - c.h) * e;
  raio = c.h * 0.5f + (PT_RAIO - c.h * 0.5f) * ec;
  if (raio > r.h * 0.5f) raio = r.h * 0.5f;
  ca = anim_clamp((e - 0.6f) / 0.4f, 0.0f, 1.0f) * a;
  if (vmAberto && e > 0.5f && ponteiro_ativo()) {
    ponteiro_camada();
    if (!vmFeito) ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ponteiroVmFora, 0, 0);
    ponteiro_alvo(f.x, f.y, f.w, f.h, NULL, NULL, 0, 0);
  }
  vmCaixa = f; vmTemCaixa = 1;
  // O PREENCHIMENTO DA ABA EM FOCO, o mesmo de desenhaTemporada (detail.c):
  // pilula cheia de vidro, ou o realce chapado. Opaco desde o primeiro quadro —
  // no comeco ele E a aba.
  { float rr = raio / (r.h > 1.0f ? r.h : 1.0f);
    if (ajustes_vidro()) gfx_vidro_pilula_cheia(r, rr, 1.0f, a);
    else gfx_cor(r, rr, fr, fg, fb, a); }
  if (ca < 0.01f) return;
  gfx_recorte(r.x, r.y, r.w, r.h);
  { float y0 = f.y + PT_PAD + c.h + PT_PAD - (1.0f - ca / (a > 0.01f ? a : 1.0f)) * 14.0f;
    if (vmFeito) {
      // A confirmacao no lugar das linhas: o check e a frase, e o painel fecha.
      // A frase ("12 episódios marcados como assistidos") e mais comprida que
      // um rotulo: quebra em ate duas linhas, no meio da area das duas opcoes.
      float lx = f.x + PT_PAD + 20.0f, tw = f.w - PT_PAD * 2.0f - 20.0f - 24.0f - 18.0f - 20.0f;
      float area = (float)VT_N * passo - 4.0f;
      int duas = (float)txt_largura(TXT_PG_ROTULO, vmMsg) > tw;
      float th = duas ? 60.0f : 30.0f, ty = y0 + (area - th) * 0.5f;
      gfx_icone((GfxRect){ lx, y0 + (area - 24.0f) * 0.5f, 24.0f, 24.0f }, "aj_check", ti, ti, ti, ca);
      txt_bloco_corta(TXT_PG_ROTULO, vmMsg, tin, tin, tin, lx + 24.0f + 18.0f, ty, tw, 30.0f, ca, 2);
    } else for (i = 0; i < VT_N; i++) {
      GfxRect ln = { f.x + PT_PAD, y0 + (float)i * passo, f.w - PT_PAD * 2.0f, passo - 4.0f };
      if (vmAberto && e > 0.5f) ponteiro_alvo(ln.x, ln.y, ln.w, ln.h, ponteiroVmOpcao, NULL, i, 0);
      ctx_menu_linha(ln, vmLin[i].rot, vmLin[i].icone, vmFocoAnim[i], ca, ti);
    } }
  gfx_sem_recorte();
}

static void menuDesenhar(GfxRect area, float a0) {
  float a = a0 * anim_clamp(vmAnim, 0.0f, 1.0f), mh, MW;
  const char *meta;
  GfxRect m;
  vmTemCaixa = 0;
  if (!menuVisivel()) return;
  if (vmFeito && SDL_GetTicks() >= vmFeitoAte) { vmAberto = 0; vmFeito = 0; }
  if (vmPainel()) { painelDesenhar(a0); return; }
  if (a < 0.01f) return;
  mh = menuAltura();
  MW = menuLargura(area.w - 40.0f);
  m = (GfxRect){ area.x + (area.w - MW) * 0.5f, area.y + VM_TOPO_ILHA, MW, mh };
  if (vmSo) {
    m.y = (area.h - mh) * 0.5f;
    // AO LADO DO CARD, alinhada ao topo dele, e inteira dentro da tela.
    if (vmTemAncora) {
      m.x = area.w > NV_TELA_W ? vmAncora.x + vmAncora.w + 24.0f : ctx_menu_ao_lado(vmAncora, MW);
      m.y = vmAncora.y;
      if (m.y + mh > area.h - VM_BORDA) m.y = area.h - VM_BORDA - mh;
      if (m.y < VM_BORDA) m.y = VM_BORDA;
      // Entra deslizando 16 px a partir do lado do card, como a do cartaz.
      m.x += (1.0f - a) * (m.x < vmAncora.x ? 16.0f : -16.0f);
    }
  }
  GfxRect comentarios = {0};
  if (vmSo && !vmModoTemp) {
    // Keep panel on the right even when the anchor sits at the screen edge.
    float pw = 676.0f, ph = 624.0f;
    if (pw + MW + 24 > area.w - 2 * VM_BORDA) pw = area.w - 2 * VM_BORDA - MW - 24;
    if (ph > area.h - 2 * VM_BORDA) ph = area.h - 2 * VM_BORDA;
    if (m.x + MW + 24 + pw > area.w - VM_BORDA) m.x = area.w - VM_BORDA - MW - 24 - pw;
    if (m.y + ph > area.h - VM_BORDA) m.y = area.h - VM_BORDA - ph;
    float panelY=vmTemAncora ? vmAncora.y : (area.h-ph)*.5f;
    if (panelY+ph>area.h-VM_BORDA) panelY=area.h-VM_BORDA-ph;
    if (panelY<VM_BORDA) panelY=VM_BORDA;
    comentarios = (GfxRect){m.x + MW + 24, panelY, pw, ph};
  }
  if (vmAberto && ponteiro_ativo() && a > .5f) {
    ponteiro_camada();
    if (!vmFeito) ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ponteiroVmFora, 0, 0);
    ponteiro_alvo(m.x, m.y, m.w, m.h, NULL, NULL, 0, 0);
  }
  vmCaixa = m; vmTemCaixa = 1;
  // Sobre a pagina, o veu do menu do cartaz; dentro da ilha do player (que ja
  // recorta), so o bastante para a lista sair de cena.
  if (vmSo) { ESCALA_INI(); ctx_menu_veu(a); ESCALA_FIM(); }
  else gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, 0, 0, 0, 0.55f * a);
  if (vmFeito) meta = vmMsg;
  else if (vmModoTemp) { const CatItem *ci = cat_item(vmIdx); meta = ci ? ci->titulo : ""; }
  else if (vmFoco<0) meta = "← / →";
  else if (vmAcoes[vmFoco] == VM_COMENT) meta = extras_comentarios_ep_tmdb() ? "TMDB · reviews da série" : "Comentários do Trakt";
  else if (vmAcoes[vmFoco] == VM_SINOPSE) meta = "Ler sinopse";
  else if (vmAcoes[vmFoco] == VM_NOTA) meta = "Sua nota";
  else meta = vmAcoes[vmFoco] != VM_DAQUI && vmVisto ? "Marcar como assistido" : "Desmarcar como assistido";
  float content=a;
  if (vmSo && !vmModoTemp) {
    float fade=fabsf(2*vmTransicao-1);
    content*=fade*fade*(3-2*fade);
    ctx_menu_ilha(m,24,a);
    if (vmFoco<0) gfx_cor((GfxRect){m.x+14,m.y+14,m.w-28,96},.22f,1,1,1,.12f*content);
    float slide=vmDirecao*8*(1-fade), hx=m.x+34+slide;
    // Two complete title lines; no ellipsis. Reserve their height for every episode.
    if (vmTituloLarg!=m.w) {
      vmTituloLarg=m.w; vmTituloEscala=1;
      while (txt_bloco(TXT_ILHA_NOME,vmCab,243,242,239,0,0,(m.w-68)/vmTituloEscala,28,0,0)>56)
        vmTituloEscala*=.95f;
    }
    gfx_transformar(hx,m.y+26,vmTituloEscala,0,0);
    txt_bloco(TXT_ILHA_NOME,vmCab,243,242,239,hx,m.y+26,(m.w-68)/vmTituloEscala,28,content,2);
    gfx_sem_transformar();
    txt_desenhar_alpha(txt_linha(TXT_ILHA_APOIO,vmFoco<0?meta:i18n(meta),243,242,239,255),hx,m.y+86,.5f*content);
    for (int i=0;i<vmOpcoes();i++) {
      GfxRect line={m.x+14,m.y+123+i*ctx_menu_passo(),m.w-28,60};
      if (vmAberto && !vmFeito && a>.5f) ponteiro_alvo(line.x,line.y,line.w,line.h,ponteiroVmOpcao,NULL,i,0);
      line.x+=slide;
      ctx_menu_linha(line,vmLin[i].rot,vmLin[i].icone,vmFocoAnim[i],content*(vmFeito?.35f:1),1);
    }
  } else ctx_menu_desenhar(m, vmCab, i18n(meta), vmFeito ? 0.8f : 0.5f, vmLin, vmOpcoes(), vmFocoAnim,
                    vmFeito ? 0.35f : 1.0f, a, vmAberto && !vmFeito ? ponteiroVmOpcao : NULL);
  if (comentarios.w > 0) {
    if (vmN && vmAcoes[vmN - 1] == VM_COMENT) {
      TxtLinha v = txt_linha(TXT_G18R, vmComentContagem, 180, 181, 187, 255);
      txt_desenhar_alpha(v, m.x + m.w - 34 - v.w, m.y + 123 + (vmN - 1) * ctx_menu_passo() + (60 - v.h) * .5f, content);
    }
    if (vmAberto && ponteiro_ativo()) ponteiro_alvo(comentarios.x, comentarios.y, comentarios.w, comentarios.h, NULL, NULL, 0, 0);
    ctx_menu_ilha(comentarios,24,a);
    epcoment_desenhar(comentarios, content);
  }
}

// A frase da confirmacao, montada quando a acao termina.
static void menuFeito(int n, int visto) {
  vmFeitoN = n; vmFeitoVisto = visto;
  if (n < 1) snprintf(vmMsg, sizeof vmMsg, "%s", i18n("Nada a mudar: já estava assim"));
  else if (visto) snprintf(vmMsg, sizeof vmMsg, n == 1 ? i18n("%d episódio marcado como assistido") : i18n("%d episódios marcados como assistidos"), n);
  else snprintf(vmMsg, sizeof vmMsg, n == 1 ? i18n("%d episódio desmarcado") : i18n("%d episódios desmarcados"), n);
  vmFeito = 1; vmFeitoAte = SDL_GetTicks() + FEITO_MS;
}

static void menuEvento(const SDL_Event *ev) {
  SDL_Keycode ko = ev->key.keysym.sym;
  int ehOk = (ko == SDLK_RETURN || ko == SDLK_KP_ENTER || ko == SDLK_SPACE);
  if (ev->type == SDL_KEYUP && ehOk) { vmConsumir = 0; return; }
  if (ev->type != SDL_KEYDOWN) return;
  if (ehOk && (ev->key.repeat || vmConsumir)) return;
  if (vmSo && !vmModoTemp && !vmFeito) {
    if (epcoment_evento(ev)) return;
    if (vmFoco<0 && (ko==SDLK_RIGHT || ko==SDLK_LEFT)) {
      if (!ev->key.repeat) vmNavegar(ko==SDLK_RIGHT?1:-1);
      return;
    }
    if (ko == SDLK_RIGHT) {
      if (epcoment_entrar()) for (int i = 0; i < vmN; i++) if (vmAcoes[i] == VM_COMENT) vmFoco = i;
      return;
    }
    if (ko == SDLK_LEFT) return;
  }
  if (ko == SDLK_UP)   { if (vmFoco > (vmSo && !vmModoTemp ? -1 : 0)) vmFoco--; return; }
  if (ko == SDLK_DOWN) { if (vmFoco < vmOpcoes() - 1) vmFoco++; return; }
  if (vmFeito) { vmAberto = 0; vmFeito = 0; return; }   // qualquer tecla encerra a confirmacao
  if (ehOk) {
    if (vmModoTemp) {
      int v = vmFoco == VT_MARCAR;
      menuFeito(aplicarVisto(VM_TEMP, v), v);
      return;
    }
    if (vmFoco<0) { vmFoco=0; return; }
    int modo = vmAcoes[vmFoco], v = modo == VM_DAQUI ? 0 : vmVisto;
    if (modo == VM_COMENT) { epcoment_entrar(); return; }
    if (modo == VM_NOTA) {
      reacao_episodio_abrir(cat_item(vmIdx), vmT, vmE); vmAberto = 0; return;
    }
    if (modo == VM_FONTES) { vmFontesPed = 1; vmAberto = 0; return; }
    if (modo == VM_SINOPSE) { sinopse_abrir(vmNome, vmSinopse); vmAberto = 0; return; }
    menuFeito(aplicarVisto(modo, v), v);
    return;
  }
  vmAberto = 0;
  if (vmSo && !vmModoTemp) extras_comentarios_ep_cancelar(); // Back cancels pending pages.
}

void episodios_evento(const SDL_Event *ev) {
#ifdef NV_TOUCH_UI
  if (toquerol_navegacao(ev)) { toquerol_limpar(&toqueEp); toquerol_limpar(&toqueEpTemporadas); }
#endif
  if (!aberto) return;
  revalidar();
  if (sinopse_aberta()) { sinopse_evento(ev); return; }
  { SDL_Keycode ko = ev->key.keysym.sym;
    int ehOk = (ko == SDLK_RETURN || ko == SDLK_KP_ENTER || ko == SDLK_SPACE);

    // O MENU COME A TECLA ENQUANTO ESTA NO AR. Mesma regra do menu do cartaz:
    // ele e a coisa mais recente na tela e e para ela que a pessoa esta
    // olhando.
    if (vmAberto) { menuEvento(ev); return; }

    // PRESSAO LONGA NA ABA DA TEMPORADA (issue #108): abre o menu da temporada.
    // O toque curto continua descendo para a lista, e passou para o KEYUP pelo
    // mesmo motivo da linha de episodio abaixo — so o soltar conhece a duracao.
    if (ehOk && grupo == 0) {
      if (ev->type == SDL_KEYDOWN && !ev->key.repeat) {
        vmSegurando = 1; vmDesde = SDL_GetTicks(); return;
      }
      if (ev->type == SDL_KEYUP) {
        int foiAqui = vmSegurando;
        Uint32 dur = ponteiro_ok_longo() ? NV_HOLD_MS : (foiAqui ? SDL_GetTicks() - vmDesde : 0);
        vmSegurando = 0;
        if (!foiAqui) return;
        if (dur >= NV_HOLD_MS) { menuAbrirTemporada(titulo, numTemporada(temporada), 0); return; }
        grupo = 1;
        if (!nLinhas()) desc_episodios(titulo, numTemporada(temporada));
        return;
      }
    }
    // PRESSAO LONGA SOBRE UMA LINHA DE EPISODIO. So no grupo da lista: em cima
    // do cabecalho nao ha episodio para marcar.
    if (ehOk && grupo == 1) {
      if (ev->type == SDL_KEYDOWN && !ev->key.repeat) {
        vmSegurando = 1; vmDesde = SDL_GetTicks(); return;
      }
      if (ev->type == SDL_KEYUP) {
        // A GUARDA OLHA SE FOI PRESSIONADO AQUI, e nao a duracao. Soltar sem
        // ter pressionado nesta camada nao e clique — a de cima pode ter
        // fechado no KEYDOWN e o KEYUP vazar para ca; e a mesma guarda que
        // home.c, detail.c e ctxmenu.c ja tem, pelo mesmo defeito.
        //
        // TESTAR `dur != 0` NO LUGAR DISSO ENGOLE O TOQUE RAPIDO: descida e
        // subida no mesmo milissegundo dao dur=0, que e legitimo. Foi assim na
        // primeira versao, e tests/player.sh pegou na primeira rodada.
        int foiAqui = vmSegurando;
        Uint32 dur = ponteiro_ok_longo() ? NV_HOLD_MS : (foiAqui ? SDL_GetTicks() - vmDesde : 0);
        vmSegurando = 0;
        if (!foiAqui) return;
        if (dur >= NV_HOLD_MS) {
          const CatEp *e2 = epLinha(foco);
          if (e2) menuAbrir(titulo, e2->temporada, e2->episodio, e2->nome, 0);
          return;
        }
        // TOQUE CURTO ABRE O EPISODIO, e a acao acontece AQUI e nao no ramo
        // antigo la embaixo: o KEYUP e o unico momento que conhece a DURACAO, e
        // e ela que separa abrir de segurar. Deixar isso para o KEYDOWN faria a
        // pressao longa nunca existir, porque o episodio ja teria aberto.
        { const CatEp *e2 = epLinha(foco);
          if (e2) {
            if (e2->temporada != atualT || e2->episodio != atualE) {
              pedidoT = e2->temporada; pedidoE = e2->episodio;
            }
            aberto = 0;
          } }
        return;
      }
    }
    if (ev->type == SDL_KEYUP) { vmSegurando = 0; return; }
  }
  if (ev->type != SDL_KEYDOWN) return;
  SDL_Keycode k = ev->key.keysym.sym;
  // O OK JA FOI DECIDIDO NO KEYUP acima (e o unico jeito de conhecer a
  // DURACAO). Deixar o KEYDOWN cair no ramo antigo abriria o episodio antes de
  // a pressao longa poder existir.
  if ((k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) && grupo >= 0)
    return;
  if(k==SDLK_r) { desc_episodios(titulo,numTemporada(temporada)); return; }
  if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_DELETE || k == SDLK_AC_BACK) {
    aberto = 0; return;
  }
  int nt = nTemporadas(), n = nLinhas();
  if (k == SDLK_UP) { if (grupo == 1 && foco > 0) foco--; else grupo--; }
  if (k == SDLK_DOWN) { if (grupo < 1) grupo++; else if (foco < n - 1) foco++; }
  if (grupo < -1) grupo = -1;
  // ESQUERDA/DIREITA TROCAM DE TEMPORADA TAMBEM COM O FOCO NUM EPISODIO (dono,
  // 03/10): a lista e vertical, entao os lados nao tem outro uso ali. O foco
  // continua na lista (no primeiro episodio da temporada nova); nas pontas
  // (primeira/ultima temporada) nada acontece.
  if (grupo >= 0 && (k == SDLK_LEFT || k == SDLK_RIGHT)) {
    int nova = temporada + (k == SDLK_RIGHT ? 1 : -1);
    if (nova >= 0 && nova < nt) {
      temporada = nova; foco = 0; scroll = 0;
      // TROCAR DE ABA E UM PEDIDO EXPLICITO de recomecar a lista: aqui o topo
      // e o lugar certo, e o alvo passa a ser o primeiro episodio da temporada
      // nova (o sincronizador em episodios_atualizar o escreve no proximo
      // quadro, quando a lista ja e a dela).
      localizarAtual = 0; semMolaScroll = 1;
      desc_episodios(titulo, numTemporada(temporada));
    }
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    if (grupo == -1) aberto = 0;
    else if (grupo == 0) {
      grupo = 1;
      if (!n) desc_episodios(titulo,numTemporada(temporada));
    }
    else {
      const CatEp *ep = epLinha(foco);
      if (ep) {
        if (ep->temporada != atualT || ep->episodio != atualE) {
          pedidoT = ep->temporada; pedidoE = ep->episodio;
        }
        aberto = 0;
      }
    }
  }
}
void episodios_atualizar(float dt) {
  anim = anim_mola(anim, aberto ? 1 : 0, dt, NV_MOLA_TELA);
  if (!vmSo) menuAtualizar(dt);   // o menu sobre a pagina anda em episodios_menu_atualizar
  if(!aberto && anim<.005f) return;
  revalidar();
  desc_episodios_pendente();
  int n = nLinhas();
  // A LISTA SUMIU: NAO E "VOLTE PARA O COMECO", E "ESPERE" — issue #102.
  // Rearma a localizacao em vez de deixar o clamp abaixo zerar o foco; quem
  // sabe onde a pessoa estava e alvoE, e ele nao depende da lista.
  if (!n) localizarAtual = 1;
  else if (localizarAtual) {
    for (int i = 0; i < n; i++) {
      const CatEp *l = epLinha(i);
      if (l && l->episodio == alvoE) { foco = i; break; }
    }
    // SEM MOLA no quadro em que a linha e encontrada: a lista chegou depois da
    // folha abrir, e animar daqui e o salto que o relato descreve.
    // A PRIMEIRA LINHA E A ANTERIOR A QUE TOCA (mockup do Glass UI): a pessoa
    // ve de onde veio e o que vem, e o foco fica na segunda linha.
    localizarAtual = 0; semMolaScroll = 1;
#ifdef NV_TOUCH_UI
    if (!toquerol_livre(&toqueEp))
#endif
    scroll = foco > 0 ? (float)(foco - 1) * EP_ROW : 0.0f;
  }
  if (foco >= n) foco = n > 0 ? n - 1 : 0;
  // O ALVO SEGUE O FOCO enquanto a lista existe — e assim que andar com o
  // direcional (ou trocar de aba) atualiza o que sera reencontrado depois.
  if (n) { const CatEp *l = epLinha(foco); if (l) alvoE = l->episodio; }
  float area = EP_VIS * EP_ROW;
  float max = n * EP_ROW - area;
  float alvo = scroll;
  if(foco*EP_ROW<scroll) alvo=foco*EP_ROW;
  if((foco+1)*EP_ROW>scroll+area) alvo=(foco+1)*EP_ROW-area;
  if (alvo > max) alvo = max;
  if (alvo < 0) alvo = 0;
  #ifdef NV_TOUCH_UI
  if (toquerol_livre(&toqueEp)) { scroll = toquerol_clamp(scroll, 0.0f, max > 0 ? max : 0); velScroll = 0.0f; }
  else
  #endif
  if (semMolaScroll) { scroll = alvo; velScroll = 0.0f; }
  else scroll = anim_mola2(&velScroll, scroll, alvo, dt, NV_MOLA2_SCROLL);
  semMolaScroll = 0;
  focoLinhaPasso(dt);
}
// PONTEIRO (#99) na folha: linha e "Fechar" focam como as setas; a aba de
// temporada troca so no CLIQUE (trocar ao passar por cima recarregaria a lista
// debaixo da mao). Clicar fora do painel fecha.
static void ponteiroEpLinha(int i, int b) { (void)b; if (i >= 0 && i < nLinhas()) { grupo = 1; foco = i; } }
static void ponteiroEpFechar(int a, int b) { (void)a; (void)b; grupo = -1; }
static void ponteiroEpFora(int a, int b) { (void)a; (void)b; aberto = 0; }
static void ponteiroEpTemporada(int i, int b) {
  (void)b;
  if (i < 0 || i >= nTemporadas()) return;
  grupo = 0;
  if (i == temporada) return;
  temporada = i; foco = 0; scroll = 0;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueEp);
#endif
  localizarAtual = 0; semMolaScroll = 1;
  desc_episodios(titulo, numTemporada(temporada));
}

// O CORPO DA ILHA: o mesmo desenho que a folha tinha — material, foco e
// segmentado de plrui.h, sem contorno —, agora dentro do retangulo que a ilha
// da. O que esta tocando leva o equalizador no acento; so a linha focada abre
// a sinopse.
static void equalizador(float x, float yb, float a) {
  float ar, ag, ab;
  static const float H[3] = { 6.0f, 12.0f, 8.0f };
  int i;
  ajustes_acento(&ar, &ag, &ab);
  for (i = 0; i < 3; i++) gfx_cor((GfxRect){ x + i * 5.0f, yb - H[i], 3.0f, H[i] }, 0.0f, ar, ag, ab, a);
}

static float alturaLista(void) {
  int n = nLinhas(), v = n < EP_VIS ? n : EP_VIS;
  return v > 0 ? v * EP_ROW : EP_VAZIO;
}
static float alturaCorpo(void) {
  float h = EP_PAD_Y + EP_TIT_H + 14.0f + EP_SEG_H + 14.0f + alturaLista() + 14.0f + EP_PE_H + EP_PAD_Y;
  // Com o menu de visto aberto a ilha tem de caber o modal inteiro.
  if (vmAberto && !vmSo) {
    float m = VM_TOPO_ILHA - 64.0f + menuAltura() + 40.0f;
    if (m > h) h = m;
  }
  return h;
}

static void linhaEp(int i, const CatItem *ci, float lx, float lw, float y, float topo, float base,
                    int ptr, float a) {
  const CatEp *ep = epLinha(i);
  // REALCE EM MOLA: a linha em foco acende, a anterior apaga (pos-brilho);
  // cores de texto trocam no ponto medio (rasterizado nao recolore por quadro).
  float selF = (i == focoLinIdx) ? focoLin : (i == focoLinAntIdx ? focoLinAnt : 0.0f);
  int sel = selF > 0.5f;
  int vid = ajustes_vidro();
  GfxRect row = { lx, y, lw, EP_ROW - 4.0f };
  // A LISTA SOME NA BASE (mask-image do mockup): as linhas que entram no
  // rodape apagam nos ultimos 70 px em vez de cortar seco.
  float fade = anim_clamp((base - (y + 30.0f)) / 70.0f, 0.0f, 1.0f), ra = a * fade;
  if (!ep) return;
  if (ptr) {
    float t = y < topo ? topo : y;
    float b = y + row.h > base ? base : y + row.h;
    if (b > t) ponteiro_alvo(row.x, t, row.w, b - t, ponteiroEpLinha, NULL, i, 0);
  }
  if (selF > 0.005f) plrui_linha_foco(row, 22.0f, ra * selF);
  const char *arte = ep->thumb[0] ? ep->thumb : (ci ? ci->backdrop : "");
  GLuint tex = tex_obter_larg(arte, 240);
  // Still que nao existe (nem metahub nem TMDB): o fundo da serie.
  if (!tex && ep->thumb[0] && tex_falhou(ep->thumb) && ci) {
    const char *res = ci->backdrop[0] ? ci->backdrop : ci->poster;
    GLuint t3 = res[0] ? tex_obter_larg(res, 240) : 0;
    if (t3) { arte = res; tex = t3; }
  }
  GfxRect tr = { lx + 12.0f, y + 10.0f, 240.0f, 135.0f };
  int atual = ep->temporada == atualT && ep->episodio == atualE;
  int visto = ci ? vistoep_estado(ci->imdb, ep->temporada, ep->episodio) : -1;
  if (vid) gfx_cor(tr, 16.0f / 135.0f, 1, 1, 1, 0.06f * ra);
  else gfx_cor(tr, 16.0f / 135.0f, 0.125f, 0.129f, 0.153f, ra);
  // DESFOCAR NAO ASSISTIDOS (#133): so o que o mapa afirma como visto fica
  // nitido — e o que esta tocando agora, que a pessoa ja esta vendo.
  if (tex && ajustes_desfocar_nao_assistidos() && !atual && visto != 1) tex = gfx_desfocado(tex, arte);
  if (tex) { gfx_tex_aspect_atual = tex_aspecto(arte); gfx_rect(tr, tex, GFX_CARD, 0, 0, 0, 16.0f / 135.0f, 0, 0, 0, ra); gfx_tex_aspect_atual = 0; }
  // O PROGRESSO NA STILL: o ponto salvo deste episodio, branco; no que esta
  // tocando, no acento.
  { char chave[96]; ProgRegistro pr;
    if (ci && visto != 1) {
      prog_chave(chave, sizeof chave, ci->imdb, ep->temporada, ep->episodio);
      if (prog_por_chave(chave, &pr) && pr.durSeg > 1.0 && pr.posSeg > 1.0) {
        float f = (float)(pr.posSeg / pr.durSeg), ar, ag, ab;
        if (f > 1.0f) f = 1.0f;
        if (atual) ajustes_acento(&ar, &ag, &ab); else ar = ag = ab = 1.0f;
        plrui_trilho((GfxRect){ tr.x + 10.0f, tr.y + tr.h - 14.0f, tr.w - 20.0f, 4.0f }, f, ar, ag, ab, ra);
      } } }
  // O ASSISTIDO e um disco com o check sobre a still.
  if (visto == 1) {
    GfxRect d = { tr.x + tr.w - 44.0f, tr.y + 10.0f, 34.0f, 34.0f };
    if (vid) gfx_vidro_chip(d, 0.5f, 0.78f, ra);
    else gfx_cor(d, 0.5f, 0.082f, 0.086f, 0.102f, ra);
    gfx_icone((GfxRect){ d.x + 8.0f, d.y + 8.0f, 18.0f, 18.0f }, "pl_check", 1, 1, 1, ra);
  }
  { float tx = tr.x + tr.w + 20.0f, w = row.x + row.w - 16.0f - tx, ty = y + 14.0f;
    char num[48], estado[96], dur[32];
    snprintf(num, sizeof num, i18n("Episódio %d"), ep->episodio);
    plrui_kicker(num, tx, ty, 243, 242, 239, ra * 0.42f);
    ty += 22.0f;
    { TxtLinha l = txt_linha_corta(TXT_ILHA_NOME, ep->nome[0] ? ep->nome : num, 243, 242, 239, sel ? 255 : 224, w);
      txt_desenhar_alpha(l, tx, ty, ra); ty += (float)l.h + 6.0f; }
    desc_duracao_txt(ep->duracao, dur, sizeof dur);   // "45 min" -> forma do idioma da UI
    if (atual) {
      float ar, ag, ab;
      ajustes_acento(&ar, &ag, &ab);
      equalizador(tx, ty + 15.0f, ra);
      plrui_kicker("Reproduzindo agora", tx + 22.0f, ty + 1.0f, (int)(ar * 255), (int)(ag * 255), (int)(ab * 255), ra);
    } else {
      if (visto == 1) snprintf(estado, sizeof estado, "%s%s%s", i18n("Assistido"), dur[0] ? " \xc2\xb7 " : "", dur);
      else snprintf(estado, sizeof estado, "%s%s%s", ep->data, ep->data[0] && dur[0] ? " \xc2\xb7 " : "", dur);
      txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_APOIO, estado, 243, 242, 239, 128, w), tx, ty, ra);
    }
    ty += 20.0f + 8.0f;
    if (selF > 0.02f && ep->sinopse[0])
      sinopse_bloco_corta(TXT_ILHA_APOIO, ep->sinopse, 243, 242, 239, tx, ty, w, 23.0f, ra * 0.5f * selF, 2);
  }
}

#ifdef NV_TOUCH_UI
static int toqueEpRolar(const PonteiroRolagem *e) {
  int r;
  if (!aberto || vmAberto) return 0;
  r = toquerol_evento(e->eixoY ? &toqueEp : &toqueEpTemporadas, e);
  if (r) { if (e->eixoY) velScroll = 0.0f; vmSegurando = 0; }
  return r;
}
static float toqueEpTemporada(int i, int desenhar, float x, float y, float a) {
  char nome[40], numero[16];
  const char *rot[1] = {nome};
  int t = numTemporada(i), n = 0, cont[1], sel = i == temporada, foc = sel && grupo == 0;
  float sw;
  for (int k = 0; k < cat_n_episodios(titulo); k++) {
    const CatEp *e = cat_episodio(titulo, k);
    if (e && e->temporada == t) n++;
  }
  cont[0] = n > 0 ? n : -1;
  snprintf(nome, sizeof nome, i18n("Temporada %d"), t);
  sw = plrui_seg(rot, cont, 1, -1, 0, -1, 0, a);
  if (desenhar) {
    int cor = foc ? plrui_tinta() : 243;
    TxtLinha lt = txt_linha(TXT_ILHA_SEG, nome, cor, cor, cor, foc || sel ? 255 : 140);
    GfxRect r = {x + 5, y + 5, sw - 10, 44};
    /* plrui_seg reserves negative x for measurement; draw the moving pill
     * explicitly so a partly visible pill also renders left of the origin. */
    gfx_cor((GfxRect){x, y, sw, 54}, .5f, .114f, .118f, .137f, a);
    if (foc) plrui_pilula_foco(r, a);
    else if (sel) gfx_cor(r, .5f, .204f, .212f, .243f, a);
    txt_desenhar_alpha(lt, x + 25, y + 5 + (44 - lt.h) * .5f, a);
    if (n > 0) {
      TxtLinha ln;
      snprintf(numero, sizeof numero, "%d", n);
      ln = txt_linha(TXT_ILHA_NUM, numero, cor, cor, cor, foc ? 150 : 89);
      txt_desenhar_alpha(ln, x + 25 + lt.w + 9, y + 5 + (44 - lt.h) * .5f + lt.h - ln.h - 1, a);
    }
  }
  return sw;
}
#endif
static void corpoIlha(GfxRect c, float a, void *u) {
  const CatItem *ci;
  GfxRect ilha = { c.x, c.y - 64.0f, c.w, c.h + 64.0f };
  float x0 = c.x + EP_PAD_X, w = c.w - EP_PAD_X * 2.0f, y = c.y + EP_PAD_Y, topo, base;
  int ptr, n, i;
  (void)u;
  revalidar();
  ci = cat_item(titulo);
  ptr = aberto && a > .5f && !vmAberto && ponteiro_ativo();
  plrilha_rect(&ilha);
  if (ptr) {
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ponteiroEpFora, 0, 0);
    ponteiro_alvo(ilha.x, ilha.y, ilha.w, ilha.h, NULL, NULL, 0, 0);
  }
  // CABECALHO: kicker com a serie, "Episodios"; o fechar e um disco (grupo -1).
  txt_desenhar_alpha(txt_linha(TXT_ILHA_PERGUNTA, "Episódios", 243, 242, 239, 255), x0 + 10.0f, y, a);
  { GfxRect d = { x0 + w - 10.0f - 56.0f, y - 7.0f, 56.0f, 56.0f };
    int f = grupo == -1;
    float k = f ? plrui_tinta() / 255.0f : 0.85f;
    if (f) plrui_pilula_foco(d, a); else plrui_botao_repouso(d, a);
    gfx_icone((GfxRect){ d.x + 16.0f, d.y + 16.0f, 24.0f, 24.0f }, "pl_x", k, k, k, a);
    if (ptr) ponteiro_alvo(d.x, d.y, d.w, d.h, ponteiroEpFechar, NULL, 0, 0); }
  y += EP_TIT_H + 14.0f;
  // TEMPORADAS no segmentado, com a contagem de episodios das que a lista ja
  // tem; uma janela que cabe na ilha, comecando antes da selecionada.
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) {
    float total = 0, selecionada = 0, vista = w - 20.0f, sx = x0 + 10;
    GfxRect regiao = {sx, y, vista, EP_SEG_H};
    for (i = 0; i < nTemporadas(); i++) {
      if (i == temporada) selecionada = total;
      total += toqueEpTemporada(i, 0, 0, 0, a) + 4;
    }
    if (!toquerol_livre(&toqueEpTemporadas)) toqueEpTemporadasOffset = fminf(selecionada, fmaxf(0, total - vista));
    toquerol_vincular(&toqueEpTemporadas, regiao, gfx_escala(), 0, total - vista, 0, &toqueEpTemporadasOffset);
    gfx_recorte(sx, y, vista, EP_SEG_H);
    sx -= toqueEpTemporadasOffset;
    for (i = 0; i < nTemporadas(); i++) {
      float sw, l, d;
      sw = toqueEpTemporada(i, 1, sx, y, a);
      l = fmaxf(sx, regiao.x); d = fminf(sx + sw, regiao.x + regiao.w);
      if (ptr && d > l) {
        ponteiro_alvo(l, y + 5, d - l, 44, NULL, ponteiroEpTemporada, i, 0);
        ponteiro_alvo_segurar(ponteiroEpTemporada);
      }
      sx += sw + 4;
    }
    gfx_recorte(ilha.x, ilha.y, ilha.w, ilha.h);
  } else
#endif
  { const char *rot[8];
    char nomes[8][40];
    int cont[8], ns = 0, ini = temporada > 1 ? temporada - 1 : 0;
    for (i = ini; i < nTemporadas() && ns < 4; i++) {
      int t = numTemporada(i), k, cc = 0;
      snprintf(nomes[ns], sizeof nomes[ns], i18n("Temporada %d"), t);
      for (k = 0; k < cat_n_episodios(titulo); k++) { const CatEp *e = cat_episodio(titulo, k); if (e && e->temporada == t) cc++; }
      rot[ns] = nomes[ns]; cont[ns] = cc > 0 ? cc : -1;
      if (plrui_seg(rot, cont, ns + 1, -1, 0, -1.0f, 0, a) > w - 20.0f) break;
      ns++;
    }
    { float sx = x0 + 10.0f, xx = sx + 5.0f;
      plrui_seg(rot, cont, ns, (float)(temporada - ini), segFoc, sx, y, a);
      if (ptr) for (i = 0; i < ns; i++) {
        float sw = plrui_seg(&rot[i], &cont[i], 1, -1, 0, -1.0f, 0, a) - 10.0f;
        ponteiro_alvo(xx, y + 5.0f, sw, 44.0f, NULL, ponteiroEpTemporada, ini + i, 0);
        ponteiro_alvo_segurar(ponteiroEpTemporada);
        xx += sw + 4.0f;
      } } }
  y += EP_SEG_H + 14.0f;
  topo = y; base = y + alturaLista();
  n = nLinhas();
  #ifdef NV_TOUCH_UI
  toquerol_vincular(&toqueEp, (GfxRect){c.x, topo, c.w, base - topo},
                   gfx_escala(), 0.0f, n * EP_ROW - (base - topo), 1, &scroll);
  if (ptr && telefoneui_ativo()) ponteiro_rolagem(toqueEpRolar);
  #endif
  gfx_recorte(c.x, topo - 4.0f, c.w, base - topo + 4.0f);
  for (i = 0; i < n; i++) {
    float yl = topo + i * EP_ROW - scroll;
    if (yl + EP_ROW < topo || yl > base) continue;
    linhaEp(i, ci, x0, w, yl, topo, base, ptr, a);
  }
  if (!n) txt_bloco(TXT_ILHA_TEXTO, desc_episodios_carregando(titulo) ?
    "Carregando episódios…" : "Episódios indisponíveis. Selecione a temporada e pressione OK para tentar novamente.",
    196, 198, 204, x0 + 10.0f, topo + 12.0f, w - 20.0f, 30, a, 3);
  gfx_recorte(ilha.x, ilha.y, ilha.w, ilha.h);   // de volta ao recorte da ilha
  y = base + 14.0f;
  // RODAPE sob um fio: "4 de 8 episodios" e a dica do gesto (pressao longa
  // nao se descobre sozinha num D-pad).
  gfx_cor((GfxRect){ x0, y, w, 1.0f }, 0.0f, 1, 1, 1, 0.07f * a);
  { float yc = y + 16.0f + 15.0f;
    if (n) {
      char contador[48];
      snprintf(contador, sizeof contador, i18n("%d de %d episódios"), foco + 1, n);
      { TxtLinha l = txt_linha(TXT_ILHA_APOIO, contador, 243, 242, 239, 115);
        txt_desenhar_alpha(l, x0 + 10.0f, yc - (float)l.h * 0.5f, a); }
    }
    if (!vmAberto && (grupo == 0 || (n && grupo == 1))) {
      const char *k[1] = { "Segure OK" };
      const char *rt[1] = { grupo == 0 ? "Marcar a temporada" : "Marcar como assistido" };
      plrui_dicas(k, rt, 1, x0 + w - 10.0f, yc, 1, a);
    } }
  // O MENU DE VISTO: modal dentro da ilha, por cima da lista.
  if (!vmSo) menuDesenhar(ilha, a);
}

static void episodios_desenharCorpo_(void);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void episodios_desenhar(void) {
  ESCALA_INI();
  episodios_desenharCorpo_();
  ESCALA_FIM();
}
static void episodios_desenharCorpo_(void) {
  if (anim < .005f) return;
  revalidar();
  // O video fica sem veu cheio: so o degrade do lado da ilha, como nas faixas.
  gfx_veu_css((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, plrilha_direita() ? 3 : 2, 1.38f, 1.0f, 0.42f * anim);
  if (!aberto) return;   // fechando: a ilha encolhe com o ultimo corpo
  { PlrIlhaPedido p;
    memset(&p, 0, sizeof p);
    p.w = EP_W;
    p.h = alturaCorpo();
    p.corpo = corpoIlha;
    p.modal = vmAberto && !vmSo;
    plrilha_pedir(&p); }
}

// --- O MENU SOZINHO, SOBRE OUTRA TELA ----------------------------------------
//
// A pagina de detalhe usa isto. Ver o comentario de VM_FONTES: la o toque curto
// no card abre as fontes e a pressao longa passou a abrir este menu, entao ele
// carrega a porta que tomou.
void episodios_menu_visto(int idxCat, int temporada, int episodio,
                          const char *nome) {
  menuAbrir(idxCat, temporada, episodio, nome, 1);
}
void episodios_menu_temporada(int idxCat, int temporada) {
  menuAbrirTemporada(idxCat, temporada, 1);
}
int  episodios_lote(int idxCat, int temporada, VistoPar *saida, int max) {
  return montarLote(idxCat, VM_TEMP, temporada, 0, saida, max);
}
int  episodios_menu_modo_temporada(void) { return vmAberto && vmModoTemp; }
int  episodios_menu_aberto(void) { return vmAberto && vmSo; }
int  episodios_menu_visivel(void) { return vmSo && menuVisivel(); }
int  episodios_menu_painel(void) { return vmSo && menuVisivel() && vmPainel(); }
void episodios_menu_atualizar(float dt) { if (vmSo) menuAtualizar(dt); }
int  episodios_menu_caixa(GfxRect *r) { if (vmTemCaixa && r) *r = vmCaixa; return vmTemCaixa; }
float episodios_menu_anim(void) { return vmAnim; }
int  episodios_menu_foco(void) { return vmFoco; }
int episodios_menu_trocou(int *t,int *e) {
  if (!vmTroca) return 0; vmTroca=0; *t=vmT; *e=vmE; return 1;
}
float episodios_menu_transicao(int *dir) { *dir=vmDirecao; return vmTransicao; }
void episodios_menu_ancora(GfxRect r) {
  if (!vmSo || !menuVisivel() || r.w < 8.0f || r.h < 8.0f) return;
  if (!vmTemAncora || vmModoTemp) vmAncora = r;
  vmTemAncora = 1;
}
int  episodios_menu_aberto_qualquer(void) { return vmAberto; }
void episodios_menu_evento(const SDL_Event *e) {
  if (vmAberto && vmSo) { revalidar(); menuEvento(e); }
}
static void episodios_menu_desenharCorpo_(void);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void episodios_menu_desenhar(void) {
  ESCALA_INI();
  episodios_menu_desenharCorpo_();
  ESCALA_FIM();
}
static void episodios_menu_desenharCorpo_(void) {
  if (vmAberto && vmSo) revalidar();
  if (vmSo) {
    float scale = gfx_escala(), fitted = scale;
    GfxRect anchor = vmAncora;
    if (!vmModoTemp && scale > 1.0f) {
      // Fit complete controls beside the card; shrinking only the panel box
      // leaves its fixed-width rating chart and review contents overflowing.
      float right = vmTemAncora ? (anchor.x + anchor.w) * scale + 24.0f : 48.0f;
      // An anchor near the right edge cannot reserve a right-hand column.
      if (right > NV_LAYOUT_REAL_W * .5f) right = 48.0f;
      fitted = fminf(fitted, (NV_LAYOUT_REAL_W - 48.0f - right) / (vmLarg + 24.0f + 676.0f));
      fitted = fminf(fitted, (NV_LAYOUT_REAL_H - 96.0f) / fmaxf(menuAltura(), 624.0f));
      gfx_escala_sair(fitted);
      vmAncora = (GfxRect){anchor.x * scale / fitted, anchor.y * scale / fitted,
                           anchor.w * scale / fitted, anchor.h * scale / fitted};
    }
    menuDesenhar((GfxRect){0, 0, NV_LAYOUT_REAL_W / fitted, NV_LAYOUT_REAL_H / fitted}, 1.0f);
    if (vmTemCaixa && fitted != scale)
      vmCaixa = (GfxRect){vmCaixa.x * fitted / scale, vmCaixa.y * fitted / scale,
                          vmCaixa.w * fitted / scale, vmCaixa.h * fitted / scale};
    vmAncora = anchor;
    gfx_escala_sair(scale);
  }
}
int  episodios_menu_pediu_fontes(void) { int v = vmFontesPed; vmFontesPed = 0; return v; }
// O EPISODIO QUE O MENU ALMOFAVA quando "Fontes deste episodio" foi pedido.
// A folha abre por cima da pagina e rouba o foco do teclado: quando a pessoa
// escolhe, detail_ep_foco ja nao ve o card — sem esta entrega o player
// montava o alvo do episodio em ANDAMENTO, nao do escolhido (#513).
int  episodios_menu_fontes(int *temporada, int *episodio) {
  if (temporada) *temporada = vmT;
  if (episodio)  *episodio = vmE;
  return vmT >= 0 && vmE > 0;
}

#ifdef NV_SHOT_HOOKS
// Capturas: o menu de visto do episodio em foco, como a pressao longa abre.
void episodios_shot_menu(void) {
  const CatEp *e = epLinha(foco);
  if (e) menuAbrir(titulo, e->temporada, e->episodio, e->nome, 0);
}
// Capturas: o foco numa linha da lista.
void episodios_shot_foco(int linha) { grupo = 1; foco = linha; }
#endif
