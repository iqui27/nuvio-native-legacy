#include "episodios.h"
#include "ajustes.h"   /* ajustes_acento: a cor do check da confirmacao */
#include "idioma.h"
#include "catalogo.h"
#include "descoberta.h"
#include "extras.h"
#include "gfx.h"
#include "tex_cache.h"
#include "text.h"
#include "layout.h"
#include "anim.h"
#include "vistoep.h"
#include "visto.h"
#include "botoes.h"
#include <stdio.h>
#include <string.h>

#define EP_W 720.0f
#define EP_ROW 172.0f
#define EP_TOP 216.0f
static int aberto, titulo, atualT, atualE, temporada, foco, grupo;
static int pedidoT, pedidoE;
static float anim, scroll;
// Velocidade da rolagem de 2a ordem (anim_mola2): partida macia, como na home.
static float velScroll;
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
// Segurar OK numa linha abre. Os tres gestos que o dono pediu — este episodio,
// ate aqui, a temporada inteira — sao O MESMO LOTE em tamanhos diferentes
// (vistoep.h), e o sentido (marcar ou desmarcar) sai do estado do episodio em
// foco: quem esta olhando um episodio visto quer desmarcar.
// VM_FONTES so aparece quando o menu e aberto DE FORA (da pagina de detalhe):
// la o toque curto no card ja abre as fontes e a pressao longa passou a abrir
// este menu, entao ele precisa carregar a porta que tomou. Dentro da folha de
// episodios a opcao nao existe — quem abriu a folha veio do player e ja tem
// fonte tocando; oferece-la ali seria uma linha que nao leva a lugar nenhum.
enum { VM_ESTE = 0, VM_ATE, VM_TEMP, VM_FONTES, VM_N };
// O MENU DA TEMPORADA (issue #108, "Pressing 'Season' brings up option to mark
// all as watched"): o mesmo cartao, aberto pela ABA e nao por um episodio, com
// so as duas linhas que o dono pediu. vmModoTemp escolhe qual dos dois; as
// opcoes sao indices proprios porque nao ha "este" nem "ate aqui" numa aba.
enum { VT_MARCAR = 0, VT_DESMARCAR, VT_N };
static int vmModoTemp;
static int vmQuantos[VM_N];   // tamanho do lote de cada opcao, da abertura
static int montarLote(int idx, int modo, int t, int e, VistoPar *saida, int max);
// Teto do lote de um gesto. 64 era o antigo e cortava temporada de anime.
#define VM_LOTE 256
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
static char vmNome[96];
// A MINIATURA DO EPISODIO, resolvida UMA vez na abertura. O cartao mostra a
// arte do episodio de que ele fala — sem ela o menu e quatro linhas de texto
// que poderiam ser de qualquer titulo. Procurar no catalogo a cada quadro
// custaria uma varredura por episodio 60 vezes por segundo para desenhar uma
// imagem que nao muda enquanto o menu estiver aberto.
static char vmThumb[512];   // = CatEp.thumb e CatItem.backdrop; 400 cortava URL longa
static int  vmSo;        // 1 = aberto sozinho, sobre outra tela
static int  vmFontesPed; // consumido por episodios_menu_pediu_fontes()

static int vmOpcoes(void) { return vmModoTemp ? VT_N : vmSo ? VM_N : VM_N - 1; }

// Abre o menu para um episodio qualquer. `t` e o NUMERO da temporada, nao o
// indice da aba: quem chama de fora nao tem abas.
static void menuAbrir(int idx, int t, int e, const char *nome, int so) {
  const CatItem *ci = cat_item(idx);
  if (!ci || !ci->imdb[0]) return;
  vmIdx = idx; vmT = t; vmE = e; vmSo = so; vmModoTemp = 0;
  snprintf(vmNome, sizeof vmNome, "%s", nome ? nome : "");
  vmThumb[0] = 0;
  { int i;
    for (i = 0; i < cat_n_episodios(idx); i++) {
      const CatEp *ce = cat_episodio(idx, i);
      if (ce && ce->temporada == t && ce->episodio == e) {
        snprintf(vmThumb, sizeof vmThumb, "%s", ce->thumb);
        break;
      }
    } }
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
  static VistoPar cat[VM_LOTE * 4];
  const CatItem *ci = cat_item(idx);
  int i, nc = 0;
  if (!ci || !ci->imdb[0]) return 0;
  if (modo == VM_ESTE) {
    if (saida && max > 0) { saida[0].temporada = (short)t; saida[0].episodio = (short)e; }
    return 1;
  }
  for (i = 0; i < cat_n_episodios(idx) && nc < VM_LOTE * 4; i++) {
    const CatEp *ce = cat_episodio(idx, i);
    if (!ce) continue;
    cat[nc].temporada = (short)ce->temporada;
    cat[nc].episodio = (short)ce->episodio;
    nc++;
  }
  return vistoep_lote(ci->imdb, modo == VM_ATE, t, e, cat, nc,
                      extras_agenda_temporada(), extras_agenda_episodio(),
                      saida, max);
}

// Aplica o gesto: monta o lote, muda o local na hora, e manda o resto para um
// fio (visto.c — Trakt, Simkl e conta, o que estiver vinculado). Devolve 0
// quando nao ha nada a fazer — e o caso de marcar o que ja esta marcado, que
// nao deve gastar uma requisicao.
//
// QUANDO ALGO MUDOU, O LOTE INTEIRO VAI, e nao so o que mudou localmente: o
// mapa pode dizer "visto" por um destino (Trakt) e o outro (Simkl) nao saber.
// Um pedido por destino, com todos os episodios — nunca um por episodio. Se
// nada mudou no local, nada sai (a regra de antes; o menu diz "ja estava").
static int aplicarVisto(int modo, int visto) {
  const CatItem *ci = cat_item(vmIdx);
  VistoPar lote[VM_LOTE];
  int n, mudou;
  if (!ci || !ci->imdb[0]) return 0;
  // A TEMPORADA DO EPISODIO (vmT), e nao a da aba selecionada. Sao a mesma
  // coisa dentro da folha, e fora dela nao ha aba nenhuma.
  n = montarLote(vmIdx, modo, vmT, vmE, lote, VM_LOTE);
  if (n < 1) return 0;
  mudou = vistoep_marcar_lote(ci->imdb, lote, n, visto);
  if (!mudou) return 0;
  visto_episodios(ci->imdb, ci->tipo[0] ? ci->tipo : "series", lote, n, visto,
                  visto_destinos());
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
  snprintf(vmThumb, sizeof vmThumb, "%s", ci->backdrop);
  n = montarLote(idx, VM_TEMP, t, 0, lote, VM_LOTE);
  for (i = 0; i < n; i++)
    if (vistoep_estado(ci->imdb, lote[i].temporada, lote[i].episodio) == 1) vistos++;
  vmFoco = (n > 0 && vistos == n) ? VT_DESMARCAR : VT_MARCAR;
}

void episodios_abrir(int idx, int t, int e) {
  titulo = idx; atualT = t; atualE = e; aberto = 1;
  temporada = foco = 0; grupo = 1; pedidoE = 0; scroll = 0;
  vmAberto = 0; vmSegurando = 0; vmConsumir = 0;
  localizarAtual = 1; alvoE = e; semMolaScroll = 1;
  for (int i = 0; i < nTemporadas(); i++) if (numTemporada(i) == t) temporada = i;
  for (int i = 0; i < nLinhas(); i++) if (epLinha(i)->episodio == e) foco = i;
  desc_episodios(titulo, t);
}
int episodios_aberto(void) { return aberto; }
int   episodios_foco_linha(void) { return foco; }
float episodios_rolagem(void) { return scroll; }
void episodios_fechar(void) { aberto = 0; }
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
static void menuDesenhar(float x, float larg, float anim) {
  if (!vmAberto) return;
  {
    const CatItem *ci=cat_item(vmIdx);
    // CENTRADO SOBRE A FOLHA, e nao sobre a tela. A folha ocupa so os EP_W da
    // direita; centrar em NV_TELA_W punha o menu sobre o vazio da esquerda,
    // longe do episodio a que ele se refere — visivel na captura de revisao.
    //
    // TETO NA LARGURA. Dentro da folha `larg` e a coluna dela e o menu fica
    // justo; sobre a pagina de detalhe `larg` e a tela inteira, e sem teto o
    // menu virava uma faixa de 1848 px atravessando tudo — visto na captura de
    // revisao. O teto e a largura que ele ja tinha na folha, entao os dois
    // lugares mostram o MESMO menu.
    //
    // A ALTURA E SOMADA, e nao cravada. Antes era `120+n*62+62`, um numero que
    // nao dizia de onde vinha, e a dica de teclas encostava na ultima pilula —
    // 8 px de folga na captura da TV, que na tela le como texto grudado no
    // botao. Agora cada pedaco entra na conta com nome.
    const float PAD=32.0f, TH_W=176.0f, TH_H=99.0f, OPT_H=54.0f, OPT_PASSO=62.0f;
    float ar, ag, ab;
    float tinta = ajustes_acento_tinta(&ar, &ag, &ab);
    int focoTxt = (int)(tinta * 255.0f + 0.5f);
    const float CAB_H=TH_H+22.0f;              // cabecalho: a miniatura manda
    float mw=larg-72.0f;
    float optTop, mh;
    char cab[140];
    int i;
    if (mw > EP_W-72.0f) mw = EP_W-72.0f;
    optTop = PAD + CAB_H;
    mh = optTop + (float)(vmOpcoes()-1)*OPT_PASSO + OPT_H
         + 22.0f    // respiro antes da dica
         + 26.0f    // a dica
         + PAD;     // rodape
    if (vmFeito) mh = optTop + 60.0f + PAD;   // so o check e a frase
    { GfxRect m={x+(larg-mw)*.5f,(NV_TELA_H-mh)*.5f,mw,mh};
    // Veu proprio: a lista atras tem texto pequeno em tres colunas.
    gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,0,0,0,.72f*anim);
    // Cartao opaco e neutro; o accent fica reservado a opcao focada, sem luz
    // atravessando o painel e tingindo a arte atras.
    gfx_cor(m,.05f,.052f,.055f,.068f,.99f*anim);

    // CABECALHO COM A ARTE DO EPISODIO. Sem miniatura o texto ocupa a linha
    // inteira, em vez de deixar um buraco do tamanho da imagem que nao veio.
    { float tx=m.x+PAD, tw=mw-PAD*2.0f;
      GLuint th=vmThumb[0]?tex_obter_larg(vmThumb,TH_W):0;
      if (th) {
        GfxRect r={m.x+PAD,m.y+PAD,TH_W,TH_H};
        gfx_tex_aspect_atual=tex_aspecto(vmThumb);
        gfx_rect(r,th,GFX_CARD,0,0,0,10.0f/TH_H,0,0,0,anim);
        gfx_tex_aspect_atual=0.0f;
        tx=m.x+PAD+TH_W+22.0f; tw=mw-(TH_W+22.0f)-PAD*2.0f;
      }
      // NO MENU DA TEMPORADA o sobrescrito e a temporada e a linha grande e a
      // serie: nao ha episodio de que falar, e o sentido esta nas opcoes.
      if (vmModoTemp) {
        char sob[48];
        snprintf(sob,sizeof sob,i18n("Temporada %d"),vmT);
        txt_desenhar_alpha(txt_linha(TXT_CAPTION2,sob,174,178,188,255),
                           tx,m.y+PAD+6.0f,anim);
        snprintf(cab,sizeof cab,"%s",vmNome);
      } else {
      txt_desenhar_alpha(txt_linha(TXT_CAPTION2,
          vmVisto?i18n("MARCAR COMO ASSISTIDO"):i18n("DESMARCAR COMO ASSISTIDO"),
          174,178,188,255),tx,m.y+PAD+6.0f,anim);
      snprintf(cab,sizeof cab,i18n("T%dE%d · %s"),vmT,vmE,vmNome);
      }
      txt_desenhar_alpha(txt_linha_corta(TXT_HEADLINE,cab,245,248,255,255,tw),
                         tx,m.y+PAD+36.0f,anim);
      (void)tw; }

    if (vmFeito) {
      // A confirmacao ocupa a area das opcoes: check grande na cor de acento
      // e a frase com o NUMERO que a acao mudou.
      float ar, ag, ab;
      char fr[120];
      GfxRect ck={m.x+PAD,m.y+optTop+8.0f,44.0f,44.0f};
      ajustes_acento(&ar,&ag,&ab);
      gfx_cor((GfxRect){ck.x-8.0f,ck.y-8.0f,60.0f,60.0f},0.5f,ar,ag,ab,anim);
      gfx_icone(ck,"check",focoTxt/255.0f,focoTxt/255.0f,focoTxt/255.0f,anim);
      if (vmFeitoN < 1) snprintf(fr,sizeof fr,"%s",i18n("Nada a mudar: já estava assim"));
      else if (vmFeitoVisto) snprintf(fr,sizeof fr,vmFeitoN==1?i18n("%d episódio marcado como assistido"):i18n("%d episódios marcados como assistidos"),vmFeitoN);
      else snprintf(fr,sizeof fr,vmFeitoN==1?i18n("%d episódio desmarcado"):i18n("%d episódios desmarcados"),vmFeitoN);
      txt_desenhar_alpha(txt_linha_corta(TXT_PLR_CORPO,fr,240,242,247,255,mw-PAD*2.0f-72.0f),
                         m.x+PAD+72.0f,m.y+optTop+8.0f+(44.0f-28.0f)*.5f,anim);
      if (SDL_GetTicks() >= vmFeitoAte) { vmAberto = 0; vmFeito = 0; }
    } else
    for(i=0;i<vmOpcoes();i++) {
      GfxRect r={m.x+24,m.y+optTop+(float)i*OPT_PASSO,mw-48,OPT_H};
      float f=(i==vmFoco)?1.0f:0.0f;
      const char *nomeIcone;
      char rot[120];
      int quantos;
      // O NUMERO NO ROTULO, e nao so o verbo: "marcar 7 episodios" e uma
      // decisao diferente de "marcar 1", e a pessoa tem de ver qual das duas
      // esta prestes a tomar. Sai do mapa, que e o mesmo que a acao vai usar.
      // O MESMO montarLote da acao: contar de uma fonte e agir sobre outra
      // faria o rotulo prometer um numero e o OK mudar outro.
      // CONTADO UMA VEZ, na abertura (vmQuantos): a conta varre o mapa e o
      // catalogo inteiros e deduplica, e fazer isso por opcao a 60 quadros
      // por segundo era trabalho jogado fora — o lote nao muda com o menu
      // aberto.
      quantos=vmModoTemp?vmQuantos[VM_TEMP]:(i<VM_FONTES?vmQuantos[i]:0);
      (void)ci;
      if(vmModoTemp) {
        // As duas frases do pedido do dono, e o numero junto: e a mesma regra
        // de "Temporada inteira (N episodios)" — a pessoa ve o tamanho do
        // gesto antes de aplicar.
        snprintf(rot,sizeof rot,
                 i==VT_MARCAR?i18n("Marcar temporada como assistida (%d)")
                             :i18n("Desmarcar temporada (%d)"),quantos);
        nomeIcone=i==VT_MARCAR?"visto":"naovisto";
      } else if(i==VM_ESTE) {
        // O ROTULO MUDA COM O SENTIDO. As duas metades do ternario eram
        // identicas ("Este episódio" dos dois lados) e nenhuma passava por
        // i18n — em ingles a linha saia em portugues.
        snprintf(rot,sizeof rot,"%s",
                 vmVisto?i18n("Marcar este episódio"):i18n("Desmarcar este episódio"));
        nomeIcone=vmVisto?"visto":"naovisto";
      } else if(i==VM_ATE) {
        snprintf(rot,sizeof rot,
                 quantos==1?i18n("Até aqui (%d episódio)")
                          :i18n("Até aqui (%d episódios)"),quantos);
        nomeIcone="avancar";
      } else if(i==VM_TEMP) {
        snprintf(rot,sizeof rot,
                 quantos==1?i18n("Temporada inteira (%d episódio)")
                          :i18n("Temporada inteira (%d episódios)"),quantos);
        nomeIcone="episodios";
      } else {
        snprintf(rot,sizeof rot,"%s",i18n("Fontes deste episódio"));
        nomeIcone="fontes";
      }
      // O mesmo botao primario do app: accent solido, tinta por contraste e
      // uma luz curta atras do alvo, sem vidro, aro ou segunda moldura.
      botao_pilula(r,rot,nomeIcone,f,1,1,anim);
    }
    // A DICA FICA ABAIXO DA ULTIMA OPCAO, com 22 de respiro e nao 8. Com 8 ela
    // encostava na pilula "Fontes deste episódio" — na captura da TV as duas
    // linhas leem como uma coisa so. O deslocamento sai da MESMA conta que
    // dimensionou o cartao, entao mudar o numero de opcoes nao volta a
    // desalinhar (foi assim que o rodape ja passou POR CIMA da terceira linha).
    if (!vmFeito)
    txt_bloco(TXT_CAPTION,"↑ ↓  Escolher   ·   OK  Aplicar   ·   Voltar  Fechar",
              155,159,169,m.x+28,
              m.y+optTop+(float)(vmOpcoes()-1)*OPT_PASSO+OPT_H+22.0f,
              mw-56,26,anim*.86f,1); }
  }
}

static void menuEvento(const SDL_Event *ev) {
  SDL_Keycode ko = ev->key.keysym.sym;
  int ehOk = (ko == SDLK_RETURN || ko == SDLK_KP_ENTER || ko == SDLK_SPACE);
  if (ev->type == SDL_KEYUP && ehOk) { vmConsumir = 0; return; }
  if (ev->type != SDL_KEYDOWN) return;
  if (ehOk && (ev->key.repeat || vmConsumir)) return;
  if (ko == SDLK_UP)   { if (vmFoco > 0) vmFoco--; return; }
  if (ko == SDLK_DOWN) { if (vmFoco < vmOpcoes() - 1) vmFoco++; return; }
  if (vmFeito) { vmAberto = 0; vmFeito = 0; return; }   // qualquer tecla encerra a confirmacao
  if (ehOk) {
    if (vmModoTemp) {
      int v = vmFoco == VT_MARCAR;
      vmFeitoN = aplicarVisto(VM_TEMP, v);
      vmFeitoVisto = v;
      vmFeito = 1; vmFeitoAte = SDL_GetTicks() + FEITO_MS;
      return;
    }
    if (vmFoco == VM_FONTES) { vmFontesPed = 1; vmAberto = 0; return; }
    vmFeitoN = aplicarVisto(vmFoco, vmVisto);
    vmFeitoVisto = vmVisto;
    vmFeito = 1; vmFeitoAte = SDL_GetTicks() + FEITO_MS;
    return;
  }
  vmAberto = 0;   // qualquer outra tecla fecha
}

void episodios_evento(const SDL_Event *ev) {
  if (!aberto) return;
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
        Uint32 dur = foiAqui ? SDL_GetTicks() - vmDesde : 0;
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
        Uint32 dur = foiAqui ? SDL_GetTicks() - vmDesde : 0;
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
  if (grupo == 0 && (k == SDLK_LEFT || k == SDLK_RIGHT)) {
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
  if(!aberto && anim<.005f) return;
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
    localizarAtual = 0; semMolaScroll = 1;
  }
  if (foco >= n) foco = n > 0 ? n - 1 : 0;
  // O ALVO SEGUE O FOCO enquanto a lista existe — e assim que andar com o
  // direcional (ou trocar de aba) atualiza o que sera reencontrado depois.
  if (n) { const CatEp *l = epLinha(foco); if (l) alvoE = l->episodio; }
  float area = NV_TELA_H - EP_TOP - 36;
  float max = n * EP_ROW - area;
  float alvo = scroll;
  if(foco*EP_ROW<scroll) alvo=foco*EP_ROW;
  if((foco+1)*EP_ROW>scroll+area) alvo=(foco+1)*EP_ROW-area;
  if (alvo > max) alvo = max;
  if (alvo < 0) alvo = 0;
  if (semMolaScroll) { scroll = alvo; velScroll = 0.0f; }
  else scroll = anim_mola2(&velScroll, scroll, alvo, dt, NV_MOLA2_SCROLL);
  semMolaScroll = 0;
}
void episodios_desenhar(void) {
  if (anim < .005f) return;
  float x = NV_TELA_W - EP_W + (1 - anim) * EP_W;
  float ar, ag, ab;
  float tinta = ajustes_acento_tinta(&ar, &ag, &ab);
  int focoTxt = (int)(tinta * 255.0f + 0.5f);
  gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,.02f,.02f,.025f,.35f*anim);
  gfx_cor((GfxRect){x,0,EP_W,NV_TELA_H},.025f,.038f,.041f,.052f,anim);
  // A hierarquia vem de tipografia e superfícies, nao de um halo no topo:
  // a luz colorida lavava o fundo e competia com a temporada e a linha focadas.
  txt_desenhar_alpha(txt_linha(TXT_PAINEL_TITULO,"Episódios",240,241,243,255),x+40,44,anim);
  { int cor=grupo==-1?focoTxt:190;
    // Acao ghost: sem caixa permanente, a superficie aparece apenas quando
    // recebe foco, com o mesmo tom contido usado pela selecao da temporada.
    if (grupo==-1)
      gfx_cor((GfxRect){x+EP_W-146,44,110,50},.3f,
              tinta>.5f?.15f+ar*.10f:.78f,
              tinta>.5f?.065f+ag*.03f:.79f,
              tinta>.5f?.09f+ab*.045f:.82f,anim);
    txt_desenhar_alpha(txt_linha(TXT_PG_ROTULO,"Fechar",cor,cor,cor,255),x+EP_W-130,55,anim); }
  gfx_recorte(x+36,120,EP_W-72,64);
  int primeira = temporada > 1 ? temporada - 1 : 0;
  for (int i = primeira; i < nTemporadas() && i < primeira+3; i++) {
    float tx = x+40+(i-primeira)*212;
    int sel = i == temporada;
    int br=sel?focoTxt:210, bg=sel?focoTxt:210, bb=sel?focoTxt:210;
    // A temporada e uma tab, nao um botao preenchido: o acento no texto e o
    // traço curto mostram a selecao sem competir com as miniaturas da lista.
    char s[48]; snprintf(s,sizeof s,i18n("Temporada %d"),numTemporada(i));
    TxtLinha l=txt_linha(TXT_PG_ROTULO,s,br,bg,bb,255);
    txt_desenhar_alpha(l,tx+(196-l.w)*.5f,138,anim);
    if (sel) {
      float trilho = l.w + 16.0f;
      gfx_cor((GfxRect){tx+(196-trilho)*.5f,180,trilho,3},1.5f,ar,ag,ab,
              anim*(grupo==0?1.0f:.62f));
    }
  }
  gfx_sem_recorte();
  gfx_recorte(x+36,EP_TOP,EP_W-72,NV_TELA_H-EP_TOP-32);
  int n=nLinhas();
  for (int i=0;i<n;i++) {
    float y=EP_TOP+i*EP_ROW-scroll;
    if (y+EP_ROW<EP_TOP || y>NV_TELA_H-32) continue;
    const CatEp *ep=epLinha(i);
    int sel=grupo==1 && i==foco;
    GfxRect row={x+40,y,EP_W-80,EP_ROW-14};
    GfxRect r=row;
    // Cada episodio recebe uma base escura, quase fundida ao painel; o foco
    // sobe para ameixa. A diferenca curta preserva o ritmo dos cartoes sem
    // empilhar bordas nem deixar o acento rosa dominar a folha.
    if (sel) {
      if (tinta < .5f) {
        // Acento branco pede uma superficie clara: a tinta escura devolvida
        // por ajustes_acento_tinta passa a ter contraste real, nao so teorico.
        gfx_cor(row,.12f,.78f,.79f,.82f,.98f*anim);
      } else {
        gfx_cor(row,.12f,.088f+ar*.055f,.075f+ag*.035f,
                .09f+ab*.045f,.98f*anim);
      }
    } else {
      gfx_cor(row,.12f,.062f,.066f,.079f,.92f*anim);
    }
    r.x+=3; r.y+=3; r.w-=6; r.h-=6;
    const CatItem *ci=cat_item(titulo);
    const char *arte=ep->thumb[0]?ep->thumb:(ci?ci->backdrop:"");
    GLuint tex=tex_obter_larg(arte,184);
    GfxRect tr={x+54,y+14,184,130};
    gfx_cor(tr,.10f,sel?.16f:.145f,sel?.17f:.15f,sel?.20f:.17f,anim);
    if(tex){gfx_tex_aspect_atual=tex_aspecto(arte);gfx_rect(tr,tex,GFX_CARD,0,0,0,.10f,0,0,0,anim);gfx_tex_aspect_atual=0;}
    char num[40];snprintf(num,sizeof num,i18n("T%dE%d"),ep->temporada,ep->episodio);
    gfx_cor((GfxRect){tr.x+8,tr.y+92,72,30},.15f,
            sel?.045f:.025f,sel?.048f:.025f,sel?.06f:.03f,.92f*anim);
    { int badgeTxt = sel && tinta < .5f ? 242 : sel ? focoTxt : 240;
      txt_desenhar_alpha(txt_linha(TXT_MINI,num,badgeTxt,badgeTxt,
                                   sel && tinta < .5f ? 245 : badgeTxt,255),
                         tr.x+15,tr.y+97,anim); }
    float tx=x+260, w=EP_W-310;
    txt_desenhar_alpha(txt_linha_corta(TXT_PAINEL_ITEM,ep->nome[0]?ep->nome:num,
                                       sel?focoTxt:242,sel?focoTxt:243,
                                       sel?focoTxt:245,255,w),tx,y+16,anim);
    int atual=ep->temporada==atualT && ep->episodio==atualE;
    // O ESTADO SAI DO MAPA, e nao mais da matriz [20][40] de extras.c. A
    // matriz so guarda o "sim": ela nao distingue "nao viu" de "nao sei", e
    // cortava em silencio a temporada 21 e o episodio 40. O mapa distingue, e
    // e ele que as acoes de marcar em lote mudam — desenhar de uma fonte e
    // agir sobre outra faria a linha nao mudar depois do gesto.
    const CatItem *cim=cat_item(titulo);
    int visto=cim?vistoep_estado(cim->imdb,ep->temporada,ep->episodio):-1;
    char estado[96];
    if(atual) snprintf(estado,sizeof estado,"Reproduzindo agora");
    else if(visto==1) snprintf(estado,sizeof estado,i18n("✓ Assistido%s%s"),ep->duracao[0]?" · ":"",ep->duracao);
    else snprintf(estado,sizeof estado,"%s%s%s",ep->data,ep->data[0]&&ep->duracao[0]?" · ":"",ep->duracao);
    if (atual)
      gfx_cor((GfxRect){tx-18,y+25,8,8},.5f,ar,ag,ab,.95f*anim);
    { int er,eg,eb;
      if (atual) { er=sel?focoTxt:(int)(ar*255.0f+0.5f); eg=sel?focoTxt:(int)(ag*255.0f+0.5f); eb=sel?focoTxt:(int)(ab*255.0f+0.5f); }
      else if (visto==1) er=eg=eb=sel?focoTxt:198;
      else er=eg=eb=sel?focoTxt:180;
      TxtLinha le=txt_linha_corta(TXT_PG_FIM,estado,er,eg,eb,255,w);
      txt_desenhar_alpha(le,tx,y+48,anim);
      // Voto do TMDB por episodio (issue #87), no mesmo selo escuro que o card
      // de episodio da pagina de detalhe usa para Trakt e TMDB.
      if(ep->nota>0){
        char valor[32];
        snprintf(valor,sizeof valor,ajustes_idioma_ingles()?"TMDB %d.%d":"TMDB %d,%d",ep->nota/10,ep->nota%10);
        TxtLinha ln=txt_linha(TXT_PG_FIM,valor,229,231,236,255);
        GfxRect selo={tx+le.w+10,y+45,ln.w+14,26};
        gfx_cor(selo,.18f,sel?.08f:.15f,sel?.085f:.15f,sel?.10f:.17f,.94f*anim);
        txt_desenhar_alpha(ln,selo.x+7,y+48,anim);
      } }
    txt_bloco(TXT_PG_FIM,ep->sinopse,sel?focoTxt*.78f:186,sel?focoTxt*.80f:188,
             sel?focoTxt*.84f:194,tx,y+78,w,25,anim,3);
  }
  if(!n) txt_bloco(TXT_PG_FIM,desc_episodios_carregando(titulo)?
    "Carregando episódios…":"Episódios indisponíveis. Selecione a temporada e pressione OK para tentar novamente.",
    196,198,204,x+56,EP_TOP+40,EP_W-112,28,anim,4);
  gfx_sem_recorte();
  if(n) {
    char contador[48];snprintf(contador,sizeof contador,i18n("%d de %d episódios"),foco+1,n);
    txt_desenhar_alpha(txt_linha(TXT_MINI,contador,170,173,181,255),x+40,NV_TELA_H-26,anim);
  }
  // DICA DO GESTO, escrita na tela. Pressao longa nao se descobre sozinha num
  // D-pad — foi a licao do menu do cartaz, que ganhou a mesma linha.
  if(n && grupo==1 && !vmAberto) {
    txt_desenhar_alpha(txt_linha(TXT_MINI,"Segure OK para marcar como assistido",
                                 174,177,186,255),x+300,NV_TELA_H-26,anim*.9f);
  }
  // A mesma dica na aba: o gesto da temporada tambem nao se descobre sozinho.
  if(grupo==0 && !vmAberto) {
    txt_desenhar_alpha(txt_linha(TXT_MINI,i18n("Segure OK para marcar a temporada"),
                                 174,177,186,255),x+300,NV_TELA_H-26,anim*.9f);
  }

  menuDesenhar(x, EP_W, anim);
  
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
int  episodios_menu_aberto_qualquer(void) { return vmAberto; }
void episodios_menu_evento(const SDL_Event *e) {
  if (vmAberto && vmSo) menuEvento(e);
}
void episodios_menu_desenhar(void) {
  if (vmAberto && vmSo) menuDesenhar(0.0f, (float)NV_TELA_W, 1.0f);
}
int  episodios_menu_pediu_fontes(void) { int v = vmFontesPed; vmFontesPed = 0; return v; }
