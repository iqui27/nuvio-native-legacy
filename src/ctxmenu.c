#include "ctxmenu.h"
#include "catalogo.h"
#include "logotitulo.h"
#include "descoberta.h"
#include "syncprog.h"
#include "tirarremoto.h"
#include "visto.h"
#include "trakt.h"
#include "cwordem.h"
#include "simkl.h"
#include "extras.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "layout.h"
#include "segurar.h"
#include "anim.h"
#include "movimento.h"
#include "ajustes.h"
#include "progresso.h"
#include "salvos.h"
#include "ilhasalvar.h"
#include "ilhaacao.h"
#include "ilha.h"
#include "recomenda.h"
#include "listasescrita.h"
#include "homefil.h"
#include "atividade.h"
#include "recenviar.h"
#include "botoes.h"
#include "badges.h"
#include "idioma.h"
#include "fileiras.h"
#include "colecoes.h"
#include "listas.h"
#include "home.h"
#include "ctxinfo.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "telefoneui.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"

// Mantem o header publico de Trakt estavel: estas leituras sao o contrato
// interno entre a modal e as escritas assincronas do proprio port.
extern int trakt_operacao_estado(int tipo);
extern int trakt_watchlist_tipo(const char *imdb, const char *tipo, int adicionar);
extern int trakt_assistido_tipo(const char *imdb, const char *tipo, int marcar);
extern int cat_historico_estado_item(int indice);
extern int cat_historico_estado_id(const char *imdb, const char *tipo);
extern void cat_historico_definir_id(const char *imdb, const char *tipo, int visto);

enum { CTX_OP_NENHUMA, CTX_OP_LISTA = 1, CTX_OP_HISTORICO = 2 };
enum { CTX_PENDENTE = 1, CTX_CONFIRMADA = 2, CTX_FALHA = 3 };

// Ar e rodape do modal de ESTILO (a pagina 1), que tem desenho proprio.
#define CTX_PAD     44.0f
#define CTX_RODAPE  70.0f
// O MENU DO CARTAZ E UMA ILHA AO LADO DO POSTER (dono, 02/10, mockup "ilha"
// tela 7): 420 de largura, raio 32, 14 de ar; cabecalho com o nome (24/700) e
// a linha "Serie · 2024 · ..." (16 a 50 %); linhas de 60 em pilula, 4 entre
// elas. Era um dialogo de 720 no meio da tela, com selos de estado e rodape de
// teclas — os dois sairam com o mockup: o rotulo de cada opcao ja diz o
// estado ("Remover dos Salvos", "Desmarcar como assistido"). O logo ficou.
#define CTX_W        420.0f
#define CTX_ILHA_PAD  14.0f
#define CTX_ILHA_RAIO 32.0f
#define CTX_CAB       75.0f     // 12 + nome + 4 + meta + 10
// O LOGO DO TITULO NO LUGAR DO NOME (pedido do dono, 01/10, reafirmado em
// 02/10 sobre o mockup): uma caixa de 300 x 52 reservada sempre que o titulo
// TEM logo (logotitulo_url), carregado ou nao — o cabecalho nao pula quando o
// arquivo chega. Sem logo nenhum, o nome em texto e o cabecalho curto.
#define CTX_LOGO_W   300.0f
#define CTX_LOGO_H    52.0f
#define CTX_CAB_LOGO (12.0f + CTX_LOGO_H + 6.0f + 19.0f + 10.0f)
// COM A EXTENSAO AO LADO (ctxinfo.h) o logo e a meta moram nela, e o menu fica
// so com uma linha de cabecalho: o nome, ou o estado da escrita em curso.
#define CTX_CAB_CURTO 44.0f
#define CTX_LINHA     60.0f
#define CTX_GAP        4.0f
#define CTX_AO_LADO   30.0f     // do poster a ilha
#define CTX_BORDA     48.0f     // margem minima da tela

// SALVO E UM FATO DO TITULO, NAO DO CARTAO. Segurar OK num cartao do
// "Trending" de um titulo que esta nos Salvos oferecia "Salvar", porque aquela
// copia do CatItem nao tem a marca — so a da watchlist tem. Pergunta a todas as
// fontes que sabem: a copia do cartao, a lista local, qualquer outra copia do
// catalogo (watchlist do Trakt) e o Plan to Watch do Simkl.
static int tituloSalvo(const CatItem *ci) {
  if (!ci) return 0;
  if (ci->naLista) return 1;
  if (!ci->imdb[0]) return 0;
  return salvos_tem(ci->imdb) || cat_imdb_na_lista(ci->imdb) ||
         (simkl_ativo() && simkl_na_plantowatch(ci->imdb));
}

static int   aberto, idx = -1, foco;
// MODO LISTA (Biblioteca > Listas): o menu e de uma LISTA, nao de um titulo.
// `cartazFixo`: o retangulo do cartaz veio de quem abriu (biblioteca.c) e nao
// deve ser refeito a cada quadro a partir do cartaz focado da HOME.
static int       doLista, cartazFixo, listaPedida, listaAlterou;
static LstLista  lista;
// MODO FILEIRA (segurar OK numa colecao ou fileira fixada da Home): corre sobre
// o modo lista (doLista=1 junto), que ja exclui titulo/estilo/painel. As
// opcoes agem pela CHAVE da fileira (homefil.h), nunca por indice.
static int       doFil, filAlterou, filVistos;
static char      filPastaId[96];
static float anim, animV;
static int   operacao, intencao, estadoOperacao;
static int   espelhoAplicado;
// A ESCRITA EM ANDAMENTO E DO SIMKL, e nao do Trakt (issue #110): diz a
// ctx_atualizar qual estado consultar. Os numeros dos dois estados sao os
// mesmos (SMK_OP_* = CTX_*), entao o resto do jogo de estados nao muda.
static int   opSimkl;
// Frase no lugar de "Biblioteca atualizada" quando o destino e o Simkl e nao
// ha vinculo: a lista desta TV foi escrita, o Simkl nao — e a tela diz.
static const char *avisoOp;
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

// MODO PAINEL: o mesmo menu, aberto SEGURANDO OK numa linha do painel de
// Salvos (salvospainel.c, dono 25/09/2026: "segurar e remover, ou more
// infos").
//
// POR QUE UMA COPIA E NAO O INDICE. O painel mostra titulos que NAO estao no
// catalogo — a lista local se desenha do proprio arquivo, antes de a descoberta
// responder (ver salvospainel.c) — e para esses nao existe indice nenhum. A
// copia e o titulo; o catalogo, quando tem o mesmo IMDb, continua sendo quem
// responde (itemAtual), porque e ele que tem progresso, temporadas e a marca.
//
// O RESTO E O MESMO MENU: mesmo cartao, mesmas pilulas, mesma espera pelo 2xx
// do Trakt e o mesmo caminho de escrita de OP_LISTA (salvos_definir + Trakt ou
// Simkl + espelho). Um "Remover" proprio do painel seria a quinta porta
// escrevendo a mesma marca, e o defeito dos quatro "+" do app irmao (salvos.h)
// comecou exatamente assim.
static int     doPainel;
static CatItem copiaPainel;
// MODO SOCIAL (abas Atividade e Amigos do painel, dono 03/10: "falta o menu
// contextual na aba activity e friends"): o mesmo modo painel, sem "Mover para
// categoria" (isso e dos Salvos), mais as acoes da LINHA que o painel passa
// (CtxExtra: "Ja assisti", "Ver perfil"...). Sem IMDb (a linha do amigo) o
// menu tem SO as extras, com o nome no cabecalho.
static int      doSocial;
static CtxExtra extras[CTX_EXTRAS_MAX];
static int      nExtras, extraPedido = -1, confExtra = -1;
static int      pendExtra = -1;   // extra a soltar quando o historico confirmar
// Removeu pelo painel: o menu sai sozinho quando a remocao CONFIRMA — a linha
// ja nao existe mais atras dele, e o foco do painel foi para a seguinte.
static int     fecharAoConfirmar;
static float   dicaCx = -1.0f;   // centro da barra de "Segure OK"; <0 = tela
// O POSTER DE ONDE O MENU SAIU (home_item_focado): a ilha nasce ao lado dele e
// ele e redesenhado POR CIMA do veu, como no mockup — a pessoa ve de qual
// titulo sao as opcoes sem ler o nome. Sem poster (destaque, painel de
// Salvos), a ilha fica no meio.
static int     temCartaz;
static GfxRect cartazRect;
static char    cartazArte[NV_TEX_URL_MAX];
// A EXTENSAO DE INFORMACOES (ctxinfo.h): a mola dela (entra depois do menu), a
// altura suavizada (o resumo chega aos poucos) e a caixa do ultimo quadro.
static float   infoT, infoH;
static int     infoDesenhada, infoLadoUlt;
static GfxRect infoCaixa, menuCaixa;

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

// QUATRO: detalhes, salvar, assistido (so em filme/serie) e tirar de
// Continuar assistindo (so em item com progresso).
//
// ERA TRES, E O QUARTO EXISTIA MESMO ASSIM. Em filme com progresso as quatro
// condicoes valem ao mesmo tempo e montar() escrevia em ops[3] — fora do
// vetor. O sintoma que chegou (issue #36) foi o mais brando dos possiveis:
// focoAnim so era animado ate CTX_MAX, entao a ultima linha do menu NUNCA
// acendia ("doesn't go white to show it is selected"). O desenho ia ate nOps e
// lia focoAnim[3], que ninguem escrevia.
//
// A opcao de Continuar assistindo entrou depois das outras tres, e o teto
// ficou onde estava. Por isso o append agora passa por juntar(), que confere o
// teto num lugar so: uma quinta opcao deixa de aparecer, em vez de corromper
// memoria.
// CINCO desde que "Recomendar a um amigo" entrou. O teto tem de subir JUNTO
// com a opcao nova: a quarta opcao existiu antes de CTX_MAX virar 4 e o
// sintoma foi escrita fora do vetor (issue #36). juntar() confere o teto num
// lugar so, entao uma sexta opcao deixa de aparecer em vez de corromper
// memoria — mas "deixa de aparecer" tambem e defeito, e por isso o numero sobe
// aqui e nao em silencio.
// SETE desde "Estilo da fileira": seis no cartaz (as cinco + a entrada do
// estilo) e, na pagina de estilos, ate cinco formas. O contrato conta as linhas
// de juntar(), e a pagina de estilos e a sexta e setima delas.
// SEIS desde o modal de estilo: as formas sairam de ops[]. O laco que as
// juntava contava UMA linha para o contrato e escrevia sete — e o teto de sete
// era exatamente o que deixava o Destaque 4:3 e a faixa com titulo de fora do
// menu. Agora elas moram em estLin[FIL_TIPO_N], do tamanho do enum.
#define CTX_MAX 9
static struct { const char *rot; int acao; } ops[CTX_MAX];
static int nOps;
static float focoAnim[CTX_MAX];
static int holdObservador;
enum { OP_LISTA = 1, OP_ASSISTIDO, OP_TIRAR_CONTINUAR, OP_RECOMENDAR, OP_DISPENSAR,
       OP_ESTILO, OP_CATEGORIA, OP_DETALHES, OP_L_ABRIR, OP_L_FIXAR, OP_L_HOME,
       OP_F_MENU, OP_F_FIXAR, OP_F_SUBIR, OP_F_DESCER, OP_F_OCULTAR, OP_F_VISTOS,
       OP_ADD_LISTA, OP_PICK_LISTA = 50,   // OP_PICK_LISTA + k = a k-esima lista da pessoa
       OP_EXTRA = 100 };   // OP_EXTRA + k = extras[k] (modo social)
// "Mover para categoria" pedido no modo painel: o IMDb do titulo, consumido
// uma vez pelo painel (ctx_pediu_categoria), que abre a escolha dele.
static char pedCategoriaImdb[24];
// "Ver detalhes" so no card de RETOMADA da home com "OK = tocar" (#350): ali o
// OK nao abre a pagina e nao havia outra porta. Nos demais cartoes o toque ja
// abre o titulo (dono, 06/10/2026).
static int detalhesOk, detalhesPend, deOutraTela;

// --- ESTILO DA FILEIRA -------------------------------------------------------
//
// A home diz de que FILEIRA e o cartao (ctx_fileira) antes de abrir o menu, e
// o menu oferece a forma dela. Quem grava e fileiras.c, no mesmo campo da tela
// Ajustes > Fileiras da Home: as duas telas mostram sempre a mesma escolha.
//
// Duas paginas: 0 e o menu do titulo (com a entrada "Estilo da fileira"), 1 e
// a lista de formas. Um grupo de colecao nao tem titulo por tras — o cartao e
// uma pasta — e abre direto na pagina 1 (soFileira).
static char filChave[192], filTitulo[96];
// "Dispensar" do cartao "Retomar agora": pedido pela home (pend) e valido para
// o menu aberto (op).
static int dispensarPend, dispensarOp;
static int  pagina, soFileira;
// PAGINA 3: "ADICIONAR A UMA LISTA" (Trakt). As linhas sao as listas da pessoa
// (lst_minha), no mesmo ops[]; o rotulo e o titulo, que mora aqui porque ops[].rot
// so guarda o ponteiro. Sete linhas visiveis; cima/baixo rolam a janela.
#define CTX_PICK_MAX 7
static char pickRot[CTX_PICK_MAX][96];
static int  pickN, pickTotal, pickIni, pickCarregando;
// PAGINA 2: A CONFIRMACAO de "Tirar de Continuar assistindo" (dono, 02/10:
// modal de ilha do mockup "ilha" tela 7). Tirar apaga o ponto de retomada
// aqui e nas contas ligadas — nao se desfaz com um Voltar —, entao a acao so
// roda depois de um OK em "Tirar da fileira". O foco nasce nele (confFoco 0);
// Voltar e "Cancelar" (1) devolvem ao menu, com o foco na mesma opcao.
static int   confFoco;
static float confAnim[2];// A PAGINA DE ESTILOS E UM MODAL PROPRIO (dono, 01/10/2026: "tem todos os
// estilos? ... coloque ao lado o preview de como seria os cards"): a lista de
// formas a esquerda, com a atual marcada, e a direita a fileira desenhada na
// forma em foco com as artes dela (home_previa_fileira). As formas nao passam
// por ops[]: o vetor e do tamanho do enum, entao toda forma que fil_estilos
// devolve aparece.
//
// UMA LINHA POR FORMA (dono, 01/10): as que so diferem em tamanho — as tres
// paisagens, os tres 4:3 — dividem a linha, e o tamanho e um segmentado P M G
// DENTRO dela, que esquerda/direita trocam. Cima/baixo continuam sendo "outra
// forma"; a previa mostra sempre o tamanho em foco. Sub-lista foi descartada:
// um segundo nivel com Voltar proprio no D-pad esconde os tamanhos atras de um
// OK, e o segmentado os mostra o tempo todo.
static FilEstiloLinha estLin[FIL_TIPO_N];
static int   nEstilos, estFoco, estTam[FIL_TIPO_N];
static float estAnim[FIL_TIPO_N];
// A previa troca com a mola: a forma de antes sai enquanto a nova entra.
// `prevRef*` e a forma que decide a reducao de cada uma (home_previa_fileira).
static int   prevAtual = -1, prevAnt = -1, prevRefAtual = -1, prevRefAnt = -1;
static float prevT = 1.0f;
#ifdef NV_TOUCH_UI
static ToqueRolagem toqueEst;
static float estRolY;
#endif

// --- RECOMENDAR: O FLUXO NAO MORA MAIS AQUI ---------------------------------
//
// "Para quem" e "o que dizer" foram DUAS PAGINAS DENTRO DESTE MODAL ate a tela
// de detalhe pedir o mesmo fluxo pelo botao circular. Duas copias das mesmas
// listas divergiriam na primeira mudanca — e a primeira mudanca ja estava
// pedida: o pareamento por codigo e o estado vazio honesto. As telas viraram
// recenviar.c, e este arquivo faz o que a tela de detalhe tambem faz: abre a
// modal compartilhada e sai da frente.

// Os DELETE remotos de "Tirar de Continuar assistindo", fora do fio de
// desenho. Nenhum dos dois e obrigatorio: sem Trakt nao ha id de playback, sem
// conta nao ha RPC. Os dois dizem no log o que fizeram.
typedef struct { char imdb[64]; char chave[192]; int ocultar; } TirarRemoto;
static void *fioTirarRemoto(void *u) {
  TirarRemoto *tr = (TirarRemoto *)u;
  tirarremoto_executar(tr->imdb, tr->chave, tr->ocultar);
  free(tr);
  return NULL;
}
// SEM FIO, DESISTE — nunca roda no quadro. A chamada direta a fioTirarRemoto de antes
// fazia o DELETE do Trakt (ate 20 s por pausa) e a RPC do syncprog no fio de
// desenho. O efeito local ja aconteceu (registro apagado, carimbo de removido,
// prog_removido_vence), entao a refacao seguinte nao readmite o card mesmo com
// o remoto velho; so a limpeza la fora fica para quem tirar de novo.
// tests/ctxmenu_semfio.sh.
static void tirarRemotoEmFio(TirarRemoto *tr) {
  pthread_t t;
  if (!tr) return;
  if (pthread_create(&t, NULL, fioTirarRemoto, tr) == 0) { pthread_detach(t); return; }
  printf("[ctx] sem fio para tirar %s do Trakt/conta: so o efeito local vale\n", tr->imdb);
  fflush(stdout);
  free(tr);
}

static int indiceAtual(void) {
  int n = cat_n();
  int achado;
  // No modo painel o indice e so "o catalogo tem este titulo?" — pode nao ter,
  // e ai quem responde e a copia (itemAtual).
  if (doPainel) return copiaPainel.imdb[0] ? cat_indice_por_imdb(copiaPainel.imdb) : -1;
  if (n < 1 || idx < 0 || idx >= n) return -1;
  if (operacaoImdb[0]) {
    achado = cat_indice_por_imdb(operacaoImdb);
    // A resposta pode chegar depois de a descoberta trocar o bloco. Nunca
    // reutilizar `idx` nesse caso, pois ele pode ser outro titulo.
    return achado;
  }
  return idx;
}

// O titulo do menu: a copia do catalogo quando ela existe, senao (so no modo
// painel) a copia que o painel entregou. NULL quando nao ha mais titulo.
static const CatItem *itemAtual(void) {
  int i = indiceAtual();
  const CatItem *ci = i >= 0 ? cat_item(i) : NULL;
  if (!ci && doPainel && (copiaPainel.imdb[0] || doSocial)) ci = &copiaPainel;
  return ci;
}

// A extensao existe para TITULO: nao para a linha de um amigo (sem IMDb) nem
// para o menu de lista ou de estilo.
static int infoPossivel(const CatItem *ci) {
  return ci && !doLista && !soFileira && ci->imdb[0] && ci->titulo[0] &&
         strcmp(ci->tipo, "channel") && strcmp(ci->tipo, "tv");
}

// -1 nao consultado, 0 nao visto, 1 visto. Por id, para valer tambem na copia.
static int historicoDe(const CatItem *ci) {
  return ci ? cat_historico_estado_id(ci->imdb, ci->tipo) : -1;
}

// O card e um "A seguir" (proximo episodio, sem ponto de retomada)? Mesma
// pergunta de continuar.c/home.c.
static int itemASeguir(const CatItem *ci) {
  return ci && ci->progresso == 0 &&
         (trakt_e_a_seguir(ci->imdb) || simkl_e_a_seguir(ci->imdb) || cwo_conta_a_seguir(ci->imdb));
}

// O UNICO CAMINHO PARA DENTRO DE ops[]. Ver a nota em CTX_MAX.
static void juntar(const char *rot, int acao) {
  if (nOps >= CTX_MAX) return;
  ops[nOps].rot = rot; ops[nOps].acao = acao; nOps++;
}

// "MARCAR COMO ASSISTIDO" so existe em filme e serie com IMDb; no painel, serie
// so quando o catalogo a tem (a copia nao traz as temporadas).
static int assistidoPossivel(const CatItem *ci, int i) {
  return ci && ci->imdb[0] && (!strcmp(ci->tipo, "movie") || !strcmp(ci->tipo, "series")) &&
         (!doPainel || i >= 0 || !strcmp(ci->tipo, "movie"));
}
// A extra que se junta a "Marcar como assistido" (CtxExtra.juntaAssistido), ou -1.
static int extraJunta(void) {
  int k;
  if (!doSocial) return -1;
  for (k = 0; k < nExtras; k++) if (extras[k].juntaAssistido) return k;
  return -1;
}

static void montar(void) {
  int i = indiceAtual();
  const CatItem *ci = itemAtual();
  nOps = 0;
  if (doLista && doFil) {
    if (filPastaId[0])
      juntar(cf_fixada(filPastaId) ? "Tirar da Home" : "Fixar na Home", OP_F_FIXAR);
    juntar("Mover para cima", OP_F_SUBIR);
    juntar("Mover para baixo", OP_F_DESCER);
    if (filVistos) juntar(ov_ativa(filChave) ? "Mostrar vistos" : "Ocultar vistos", OP_F_VISTOS);
    if (fil_estilos(filChave, NULL, NULL, FIL_TIPO_N) >= 1) juntar("Estilo da fileira", OP_ESTILO);
    juntar("Ocultar fileira", OP_F_OCULTAR);
    if (foco >= nOps) foco = nOps - 1;
    if (foco < 0) foco = 0;
    return;
  }
  if (doLista) {
    const char *porque;
    juntar("Abrir lista", OP_L_ABRIR);
    juntar(lst_fixada(&lista) ? "Tirar da Biblioteca" : "Fixar na Biblioteca", OP_L_FIXAR);
    // So onde a Home sabe buscar a lista (o Simkl nao): oferecer a acao que so
    // responde "nao" seria o botao que aparenta funcionar.
    if (lst_aceita_home(&lista, &porque))
      juntar(lst_na_home(&lista) ? "Tirar da Home" : "Adicionar à Home", OP_L_HOME);
    if (foco >= nOps) foco = nOps - 1;
    if (foco < 0) foco = 0;
    return;
  }
  if (pagina == 3) {
    int k;
    pickTotal = lst_minhas_n();
    pickCarregando = lst_minhas_carregando();
    if (pickIni >= pickTotal) pickIni = pickTotal > 0 ? pickTotal - 1 : 0;
    pickN = pickTotal - pickIni;
    if (pickN > CTX_PICK_MAX) pickN = CTX_PICK_MAX;
    for (k = 0; k < pickN; k++) {
      LstLista l;
      pickRot[k][0] = 0;
      if (lst_minha(pickIni + k, &l)) snprintf(pickRot[k], sizeof pickRot[k], "%s", l.titulo);
      juntar(pickRot[k], OP_PICK_LISTA + k);
    }
    // Sem listas (ainda): uma linha so de aviso, sem acao (aplicar() a ignora).
    if (pickN == 0)
      juntar(pickCarregando ? "Carregando listas"
                            : "Você ainda não criou listas no Trakt", 0);
    if (foco >= nOps) foco = nOps > 0 ? nOps - 1 : 0;
    if (foco < 0) foco = 0;
    return;
  }
  if (pagina == 1) {
    int k, j, atual = fil_tipo(filChave);
    nEstilos = fil_estilo_linhas(filChave, estLin, FIL_TIPO_N);
    // O foco nasce na forma que vale agora, no TAMANHO que vale agora; as
    // outras linhas de tamanho nascem no menor (o de sempre).
    if (estFoco < 0 || estFoco >= nEstilos) {
#ifdef NV_TOUCH_UI
      estRolY = 0.0f;
      toquerol_limpar(&toqueEst);
#endif
      for (estFoco = 0, k = 0; k < nEstilos; k++) {
        estTam[k] = 0;
        for (j = 0; j < estLin[k].n; j++)
          if (estLin[k].tipos[j] == atual) { estFoco = k; estTam[k] = j; }
      }
    }
    return;
  }
  if (!ci) return;
  // A LINHA DO AMIGO nao e titulo: so as acoes dela.
  if (doSocial && !ci->imdb[0]) {
    int k;
    for (k = 0; k < nExtras; k++) juntar(extras[k].rot, OP_EXTRA + k);
    if (foco >= nOps) foco = nOps > 0 ? nOps - 1 : 0;
    if (foco < 0) foco = 0;
    return;
  }
  // SEM "VER DETALHES" / "MAIS INFORMAÇÕES" (dono, 06/10/2026): o toque no
  // cartaz ja abre o titulo, e o que a pessoa queria ver de relance agora esta
  // na extensao ao lado do menu (ctxinfo.h).
  // Sem IMDb nao ha endpoint remoto suportado para esta acao. Nao oferecer
  // um botao que so aparentaria funcionar e inventaria estado local.
  if (detalhesOk && !doPainel && ci->imdb[0]) juntar("Ver detalhes", OP_DETALHES);
  if (ci->imdb[0]) {
    // O MESMO VERBO DO PAINEL E DO BOTAO "+". Estava "Adicionar à biblioteca",
    // e "Biblioteca" e o nome de uma TELA — a pessoa lia o rotulo, ia ate a
    // tela Biblioteca e nao encontrava relacao com o "+" que tinha apertado no
    // detalhe. Agora as tres portas da mesma acao (o "+", esta linha e o painel
    // da tecla AZUL) usam a palavra "salvar", e todas escrevem no mesmo lugar.
    juntar(estadoOperacao == CTX_PENDENTE && operacao == CTX_OP_LISTA
             ? (intencao ? "Salvando..." : "Removendo dos Salvos...")
             : (tituloSalvo(ci) ? "Remover dos Salvos" : "Salvar"),
           OP_LISTA);
    // Listas do Trakt: so com conta e so onde a lista aceita o tipo (filme/serie).
    if (!doPainel && !doSocial && trakt_ativo() &&
        (!strcmp(ci->tipo, "movie") || !strcmp(ci->tipo, "series")))
      juntar("Adicionar a uma lista", OP_ADD_LISTA);
  }
  // AS CATEGORIAS DA PESSOA (salvosorg.h) so existem no painel de Salvos: e
  // la que o titulo esta salvo e e la que a escolha abre.
  if (doPainel && !doSocial && ci->imdb[0]) juntar("Mover para categoria", OP_CATEGORIA);
  // O web so oferece "assistido" em filme e serie — nao em canal nem evento,
  // que sao tipos que os addons do dono tambem declaram.
  //
  // NO PAINEL, SERIE SO COM O CATALOGO. A copia da lista local nao tem as
  // temporadas, e desmarcar uma serie no Simkl sem elas apaga a serie da
  // biblioteca de la (ver visto.c/simkl.c). Filme nao tem esse risco.
  if (assistidoPossivel(ci, i) && extraJunta() < 0) {
    juntar(estadoOperacao == CTX_PENDENTE && operacao == CTX_OP_HISTORICO
             ? (intencao ? "Marcando como assistido..."
                         : "Desmarcando como assistido...")
             : (historicoDe(ci) == 1 ? "Desmarcar como assistido"
                                     : "Marcar como assistido"),
           OP_ASSISTIDO);
  } else if (extraJunta() >= 0) {
    // "JA ASSISTI" DA RECOMENDACAO: UMA acao so, no lugar de "Marcar como
    // assistido". Enquanto o historico espera o 2xx, o rotulo diz isso.
    int k = extraJunta();
    juntar(estadoOperacao == CTX_PENDENTE && operacao == CTX_OP_HISTORICO
             ? "Marcando como assistido..." : extras[k].rot, OP_EXTRA + k);
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
  // O menu do painel e o curto que o dono pediu (remover, mais informacoes,
  // assistido): esta e a de baixo sao acoes do cartaz da home.
  // "A SEGUIR" TAMBEM (#203): o proximo episodio tem progresso 0 e a condicao
  // antiga nunca o oferecia; sem a opcao ele nao saia da fileira.
  if (!doPainel && (ci->progresso > 0 || itemASeguir(ci)) && ci->imdb[0]) {
    juntar("Tirar de Continuar assistindo", OP_TIRAR_CONTINUAR);
  }
  // SO EXISTE SE O PACOTE TEM O SERVICO. Sem NUVIO_REC_URL compilada,
  // recomenda_ativo() e 0 e esta linha nunca aparece — o dono publica builds
  // assim, e um item de menu que so da erro e pior que item nenhum.
  if (!doPainel && recomenda_ativo() && ci->imdb[0] &&
      (!strcmp(ci->tipo, "movie") || !strcmp(ci->tipo, "series"))) {
    juntar("Recomendar a um amigo", OP_RECOMENDAR);
  }
  // Por ULTIMO: e da fileira, nao do titulo. So quando a home disse qual e a
  // fileira e ela tem forma para escolher (Continuar assistindo, Top 10 e o
  // destaque ficam com o visual deles — a home nem passa a chave).
  if (!doPainel && filChave[0] && fil_estilos(filChave, NULL, NULL, FIL_TIPO_N) > 0)
    juntar("Estilo da fileira", OP_ESTILO);
  // Mover/ocultar/"vistos ocultos": fileira que a Home registrou (Ajustes a religa).
  if (!doPainel && filChave[0] && homefil_indice(filChave) >= 0) juntar("Opções da fileira", OP_F_MENU);
  // Cartao "Retomar agora": soltar o titulo da faixa, sem apagar progresso.
  if (!doPainel && dispensarOp) juntar("Dispensar", OP_DISPENSAR);
  // As acoes da linha social por ULTIMO: sao dela, nao do titulo.
  if (doSocial) {
    int k;
    for (k = 0; k < nExtras; k++) if (k != extraJunta()) juntar(extras[k].rot, OP_EXTRA + k);
  }
  // O FOCO TEM DE CABER NA LISTA QUE ACABOU DE SER MONTADA.
  //
  // montar() roda de novo a cada confirmacao, e a lista ENCOLHE em casos
  // reais: marcar como assistido apaga a posicao de retomada, e com isso
  // "Tirar de Continuar assistindo" deixa de existir. Se o foco estava nela,
  // `foco` passa a apontar para fora — e ai o desenho nao pinta linha nenhuma
  // ali (o laco vai ate nOps) e aplicar() sai cedo em `foco >= nOps`. Na TV
  // isso e exatamente "ele pula e nao faz nada".
  if (foco >= nOps) foco = nOps > 0 ? nOps - 1 : 0;
  if (foco < 0) foco = 0;
}

static void abrirComum(int indice);
static int inlineOn, inlinePend;
static int modoInline(void);
// O cartaz vem da HOME, em pixels da tela real; o menu e camada ampliada e
// mede pela tela virtual (escala.h). Desenhado na virtual ele cai no mesmo
// lugar e no mesmo tamanho da tela.
static GfxRect ctxDaHome(GfxRect r) {
  float e = gfx_escala_ui();
  return (GfxRect){ r.x / e, r.y / e, r.w / e, r.h / e };
}

// Coloca o menu FORA do cartaz selecionado. Tenta o lado direito primeiro,
// depois o esquerdo; se nenhum lado comporta a ilha inteira, escolhe o lado
// com mais espaco e prende a ilha dentro da viewport. A mesma geometria vale
// para cartazes retrato e paisagem e para todos os chamadores.
static float ctxXCartaz(GfxRect r) {
  float minX = CTX_BORDA;
  float maxX = NV_TELA_W - CTX_BORDA - CTX_W;
  float direita = r.x + r.w + CTX_AO_LADO;
  float esquerda = r.x - CTX_AO_LADO - CTX_W;
  if (maxX < minX) {
    minX = 0.0f;
    maxX = NV_TELA_W > CTX_W ? NV_TELA_W - CTX_W : 0.0f;
  }
  if (direita <= maxX) return direita;
  if (esquerda >= minX) return esquerda;
  if (maxX - direita >= esquerda - minX) {
    if (direita < minX) return minX;
    if (direita > maxX) return maxX;
    return direita;
  }
  if (esquerda < minX) return minX;
  if (esquerda > maxX) return maxX;
  return esquerda;
}

void ctx_abrir(int indice) {
  if (holdCancelado) {
    holdCancelado = 0;
    holdPronto = 0;
    dispensarPend = 0;
    return;
  }
  if (indice < 0 || indice >= cat_n() || !cat_item(indice)) { filChave[0] = 0; dispensarPend = 0; return; }
  doPainel = 0;
  doSocial = 0;
  soFileira = 0;
  doLista = 0;
  detalhesPend = !deOutraTela && ajustes_cw_ok_toca() && home_foco_retomada();
  abrirComum(indice);
  { HomeItem hi;
    // So um CARTAZ: o destaque tambem e "item focado", mas o retangulo dele e
    // a arte de tela cheia, e ao lado dela nao ha lugar.
    if (home_item_focado(&hi) && hi.indice == indice && hi.rect.w > 8.0f && hi.rect.h > 8.0f &&
        hi.rect.w < 1920.0f * 0.5f && hi.rect.h < 1080.0f * 0.7f) {
      temCartaz = 1;
      cartazRect = ctxDaHome(hi.rect);
      snprintf(cartazArte, sizeof cartazArte, "%s", hi.arte ? hi.arte : "");
    }
  }
}

void ctx_abrir_cartaz(int indice, GfxRect r, const char *arte) {
  deOutraTela = 1;
  ctx_abrir(indice);
  deOutraTela = 0;
  if (!aberto || doLista) return;
  cartazFixo = 0;
  if (r.w > 8.0f && r.h > 8.0f && arte && arte[0]) {
    temCartaz = 1;
    cartazFixo = 1;
    cartazRect = r;
    snprintf(cartazArte, sizeof cartazArte, "%s", arte);
  }
}

void ctx_abrir_lista(const LstLista *l) {
  if (holdCancelado) { holdCancelado = 0; holdPronto = 0; return; }
  if (!l) return;
  filChave[0] = 0;
  doPainel = 0; soFileira = 0; doSocial = 0;
  lista = *l;
  doLista = 1;
  doFil = 0;
  abrirComum(-1);
  // Baixa os itens para o resumo ter capas e a mistura. Os da lista aberta de
  // antes (se havia) nao servem: lst_abrir comeca do zero.
  lst_abrir(&lista, lista.midia[0] ? lista.midia : "MOVIE");
}
int ctx_pediu_lista(void) { int v = listaPedida; listaPedida = 0; return v; }
int ctx_lista_alterou(void) { int v = listaAlterou; listaAlterou = 0; return v; }

void ctx_dispensar_retomar(int on) { dispensarPend = on ? 1 : 0; }

void ctx_fileira(const char *chave, const char *titulo) {
  snprintf(filChave, sizeof filChave, "%s", chave ? chave : "");
  snprintf(filTitulo, sizeof filTitulo, "%s", titulo ? titulo : "");
}

void ctx_abrir_filmenu(const char *chave, const char *titulo, const char *pastaId, int comVistos) {
  if (holdCancelado) { holdCancelado = 0; holdPronto = 0; return; }
  if (!chave || !chave[0]) return;
  ctx_fileira(chave, titulo);
  snprintf(filPastaId, sizeof filPastaId, "%s", pastaId ? pastaId : "");
  filVistos = comVistos ? 1 : 0;
  doPainel = 0; doSocial = 0; soFileira = 0;
  doLista = 1; doFil = 1;
  abrirComum(-1);
}
int ctx_fil_alterou(void) { int v = filAlterou; filAlterou = 0; return v; }

void ctx_abrir_fileira(const char *chave, const char *titulo) {
  if (holdCancelado) {
    holdCancelado = 0;
    holdPronto = 0;
    return;
  }
  ctx_fileira(chave, titulo);
  if (fil_estilos(filChave, NULL, NULL, FIL_TIPO_N) < 1) { filChave[0] = 0; return; }
  doPainel = 0;
  doSocial = 0;
  soFileira = 1;
  doLista = 0; doFil = 0;
  abrirComum(-1);
}

// O que as duas portas fazem igual. Separado para o modo painel nao ser uma
// segunda copia da inicializacao que diverge na primeira correcao.
static void abrirComum(int indice) {
  // A longa ja consumiu o gesto na home. Limpar a sentinela aqui evita que o
  // KEYUP seguinte seja reaproveitado como uma selecao dentro da modal.
  holdPronto = 0;
  esperandoSoltura = !ponteiro_ok_longo();   // dedo ja entregou o par completo
  idx = indice; foco = 0; aberto = 1;
  dispensarOp = dispensarPend; dispensarPend = 0;
  detalhesOk = detalhesPend; detalhesPend = 0;
  temCartaz = 0; cartazFixo = 0;
  pagina = soFileira ? 1 : 0;
  estFoco = -1;               // montar() poe o foco na forma atual
  prevAtual = prevAnt = prevRefAtual = prevRefAnt = -1; prevT = 1.0f;
  memset(estAnim, 0, sizeof estAnim);
  fecharAoConfirmar = 0;
  operacao = CTX_OP_NENHUMA; intencao = 0; estadoOperacao = 0;
  espelhoAplicado = 0;
  operacaoImdb[0] = 0;
  memset(focoAnim, 0, sizeof focoAnim);
  montar();
  infoT = 0.0f; infoH = 0.0f; infoDesenhada = 0;
  // Sem "Ver detalhes", um titulo sem id nenhum nao tem acao: um menu vazio
  // seria pior que nenhum (o toque continua abrindo o titulo).
  if (!doLista && pagina == 0 && nOps == 0) { aberto = 0; return; }
  if (!doLista && !soFileira) ctxinfo_abrir(itemAtual());
}

void ctx_abrir_salvo(const CatItem *titulo) {
  if (holdCancelado) {
    holdCancelado = 0;
    holdPronto = 0;
    return;
  }
  if (!titulo || !titulo->imdb[0]) return;
  filChave[0] = 0;   // o painel de Salvos nao e fileira da home
  soFileira = 0;
  copiaPainel = *titulo;
  // O ID DO TITULO, nunca o do episodio: e o que a lista local, a watchlist e
  // a conta guardam (salvos.h), e o que o Trakt recebe no DELETE.
  salvos_id_titulo(titulo->imdb, copiaPainel.imdb, sizeof copiaPainel.imdb);
  if (!copiaPainel.tipo[0]) snprintf(copiaPainel.tipo, sizeof copiaPainel.tipo, "movie");
  doPainel = 1;
  doSocial = 0;
  doLista = 0;
  inlineOn = inlinePend; inlinePend = 0;
  abrirComum(-1);
}

void ctx_abrir_social(const CatItem *titulo, const CtxExtra *ex, int n) {
  int k;
  if (holdCancelado) { holdCancelado = 0; holdPronto = 0; return; }
  if (!titulo || (!titulo->imdb[0] && n < 1)) return;
  filChave[0] = 0;
  soFileira = 0;
  copiaPainel = *titulo;
  if (copiaPainel.imdb[0]) {
    salvos_id_titulo(titulo->imdb, copiaPainel.imdb, sizeof copiaPainel.imdb);
    if (!copiaPainel.tipo[0]) snprintf(copiaPainel.tipo, sizeof copiaPainel.tipo, "movie");
  }
  nExtras = 0;
  for (k = 0; ex && k < n && k < CTX_EXTRAS_MAX; k++) extras[nExtras++] = ex[k];
  extraPedido = -1;
  confExtra = -1;
  pendExtra = -1;
  doPainel = 1;
  doSocial = 1;
  doLista = 0;
  abrirComum(-1);
}

int ctx_pediu_extra(void) { int v = extraPedido; extraPedido = -1; return v; }

int ctx_do_painel(void) { return aberto && doPainel; }
void ctx_centro_dica(float cx) { dicaCx = cx; }

int ctx_aberto(void) { return aberto; }
int ctx_info_caixa(GfxRect *info, GfxRect *menu, int *lado) {
  if (!infoDesenhada) return 0;
  if (info) *info = infoCaixa;
  if (menu) *menu = menuCaixa;
  if (lado) *lado = infoLadoUlt;
  return 1;
}
const CatItem *ctx_titulo(void) { return aberto && !doLista && !soFileira ? itemAtual() : NULL; }
const char *ctx_pediu_categoria(void) {
  static char s[24];
  if (!pedCategoriaImdb[0]) return NULL;
  snprintf(s, sizeof s, "%s", pedCategoriaImdb);
  pedCategoriaImdb[0] = 0;
  return s;
}

// O ESPELHO LOCAL DE "ASSISTIDO", separado de quem confirma: com Trakt ele
// roda depois do 2xx (ctx_atualizar); sem Trakt roda na hora (aplicar), porque
// nao ha resposta nenhuma a esperar.

// --- ILHA: a confirmacao de assistido / nao assistido (ilhaacao.h) -----------------
// Desfazer so quando ha inverso de verdade: DESMARCAR e puro (nada some), e
// MARCAR so quando o titulo nao tinha posicao de retomada — marcar apaga o
// ponto de retomada local e nas contas, e esse nao volta.
typedef struct { CatItem ci; int intencao; } AssistidoCtx;
static CatItem opItem;      // copia do titulo da operacao em curso (o bloco do catalogo troca)
static int opTinhaRetomada;
static void desfazerAssistido(const void *p) {
  const AssistidoCtx *c = p;
  int volta = !c->intencao;
  if (volta) { printf("[visto] marcado por desfazer-menu-manual pos=0 dur=0 assistido=0s id=%s\n", c->ci.imdb); fflush(stdout); }
  visto_titulo(c->ci.imdb, c->ci.tipo, c->ci.temporadas, c->ci.nTemporadas, volta, visto_destinos());
  if (trakt_ativo()) trakt_assistido_tipo(c->ci.imdb, c->ci.tipo, volta);
  cat_historico_definir_id(c->ci.imdb, c->ci.tipo, volta);
  desc_remontar_fileiras();
}
static void anunciarAssistido(int intencao) {
  static AssistidoCtx c;
  IlhaAcao a;
  if (!opItem.imdb[0]) return;
  c.ci = opItem; c.intencao = intencao;
  memset(&a, 0, sizeof a);
  a.icone = intencao ? "check" : "aj_x";
  a.frase = intencao ? i18n("Marcado como assistido") : i18n("Marcado como não assistido");
  a.titulo = opItem.titulo; a.thumb = opItem.poster; a.arte = opItem.backdrop;
  a.tipo = intencao ? ILHA_OK : ILHA_INFO;
  if (!(intencao && opTinhaRetomada)) { a.desfazer = desfazerAssistido; a.ctx = &c; a.ctxN = sizeof c; }
  ilhaacao_feita(&a);
}
static void espelharAssistido(int atual, const CatItem *ci, int intencao) {
  cat_historico_definir_id(ci->imdb, ci->tipo, intencao);
  // MARCAR COMO ASSISTIDO APAGA A POSICAO DE RETOMADA.
  //
  // cat_historico_definir_id so escreve numa tabela lateral de
  // historico, e a fileira "Continuar assistindo" nao le dela: ela
  // le progresso/restanteMin/temporada/episodio do proprio item. Sem
  // isto o card continuava ali com a barra cheia depois de o titulo
  // ter sido marcado como visto — o "removo do watch e o card nao
  // sai" do relato.
  //
  // E o MESMO par que "Tirar de Continuar assistindo" faz logo
  // abaixo, e pelo mesmo motivo: quem terminou nao tem o que
  // retomar. So na direcao "assistido"; desmarcar nao inventa uma
  // posicao que ninguem gravou.
  if (intencao) {
    char chave[192];
    prog_chave(chave, sizeof chave, ci->imdb, ci->temporada, ci->episodio);
    prog_remover(chave);
    // As mesmas tres fontes de "Tirar de Continuar assistindo": quem
    // marcou como visto tambem nao quer o card de retomada de volta
    // no proximo ciclo.
    // Em fio (fioTirarRemoto): o DELETE do Trakt e uma chamada de rede por
    // pausa (ate 20 s cada) e o syncprog e RPC; nenhum pode parar o desenho.
    { TirarRemoto *tr = (TirarRemoto *)malloc(sizeof *tr);
      if (tr) {
        snprintf(tr->imdb, sizeof tr->imdb, "%s", ci->imdb);
        snprintf(tr->chave, sizeof tr->chave, "%s", chave);
        tr->ocultar = 0;   // marcar como visto NAO esconde a serie no Trakt
        tirarRemotoEmFio(tr);
      } }
    simkl_playback_remover(ci->imdb);
    // O CARIMBO DE "TIRAR DE CONTINUAR" (#244). Sem ele a proxima passada
    // readmitia o item pelo /sync/playback que o servidor ainda devolvia (ou
    // por uma pausa que o DELETE nao alcancou), so sem a barra.
    prog_marcar_removido(ci->imdb);
    // E O CARD SAI DA FILEIRA NA HORA (#244): zerar a barra deixava os cards da
    // obra ali, "marca todos mas nao remove nenhum". Mesma remocao do "Tirar".
    // Depois de zerar: tirar desloca a janela e `atual` deixaria de valer.
    cat_zerar_progresso(atual);
    { char obra[sizeof ci->imdb];   // `ci` aponta para o bloco que a poda desloca
      snprintf(obra, sizeof obra, "%s", ci->imdb);
      cat_tirar_continuar(obra); }
  }
}

// OK na pagina de estilos. Grava e sai: a home remonta pela revisao de
// fileiras.c e a pessoa ve a forma nova no lugar, sem o veu do menu por cima.
// A forma da linha `i` no tamanho escolhido nela, e a maior da linha (a que
// decide a reducao da previa).
static int estTipo(int i) {
  int j;
  if (i < 0 || i >= nEstilos) return -1;
  j = estTam[i];
  if (j < 0 || j >= estLin[i].n) j = 0;
  return estLin[i].tipos[j];
}
static int estRef(int i) {
  return (i >= 0 && i < nEstilos) ? estLin[i].tipos[estLin[i].n - 1] : -1;
}

static void aplicarEstilo(void) {
  if (estFoco >= 0 && estFoco < nEstilos) fil_definir_tipo(filChave, estTipo(estFoco));
  aberto = 0;
}

// MARCAR/DESMARCAR NO HISTORICO DA CONTA (o corpo de OP_ASSISTIDO, e tambem o
// passo de historico do "Ja assisti" da recomendacao: `soMarcar` nunca desmarca).
static void iniciarAssistido(int atual, const CatItem *ci, int soMarcar) {
      intencao = soMarcar ? 1 : (historicoDe(ci) == 1 ? 0 : 1);
      if (intencao) { printf("[visto] marcado por menu-manual pos=0 dur=0 assistido=0s id=%s\n", ci->imdb); fflush(stdout); }
      opItem = *ci; opTinhaRetomada = ci->progresso > 0 || ci->restanteMin > 0;
      snprintf(operacaoImdb, sizeof operacaoImdb, "%s", ci->imdb);
      operacao = CTX_OP_HISTORICO;
      opSimkl = 0;
      avisoOp = NULL;
      espelhoAplicado = 0;
      estadoOperacao = CTX_PENDENTE;
      // SIMKL E CONTA NUVIO, em fio (visto.c), com ou sem Trakt. Antes daqui
      // so havia o Trakt, e sem ele esta acao era "[trakt] historico recusado:
      // Trakt desligado" e CTX_FALHA — o "sem o traktv nao ta dando o watched"
      // do dono. As temporadas vao junto porque desmarcar serie no Simkl sem
      // elas apagaria a serie da biblioteca de la (ver simkl.c).
      visto_titulo(ci->imdb, ci->tipo, ci->temporadas, ci->nTemporadas, intencao,
                   visto_destinos());
      if (trakt_ativo()) {
        // O caminho do Trakt NAO MUDOU: mesmo pedido, mesma espera, espelho so
        // depois do 2xx em ctx_atualizar.
        if (!trakt_assistido_tipo(ci->imdb, ci->tipo, intencao))
          estadoOperacao = CTX_FALHA;
      } else {
        // SEM TRAKT NAO HA RESPOSTA A ESPERAR (o mesmo raciocinio do "+" na
        // Lista do Nuvio, acima): o local e a verdade desta TV, aplicado agora,
        // e a conta o devolve no proximo pull (sync.c aplica os vistos da
        // conta justamente quando o Trakt esta desligado).
        espelharAssistido(atual, ci, intencao);
        estadoOperacao = CTX_CONFIRMADA;
        espelhoAplicado = 1;
        desc_remontar_fileiras();
        anunciarAssistido(intencao);
      }
}

static void aplicar(void) {
  int atual = indiceAtual();
  const CatItem *ci = itemAtual();
  int acao;
  if (foco < 0 || foco >= nOps) return;
  acao = ops[foco].acao;
  if (doLista && doFil) {
    if (acao == OP_ESTILO) {
      // "Estilo da fileira": segue para a pagina de formas, no mesmo modal.
      doLista = 0; doFil = 0; soFileira = 1;
      pagina = 1; estFoco = -1; prevAtual = prevAnt = prevRefAtual = prevRefAnt = -1; prevT = 1.0f;
      memset(estAnim, 0, sizeof estAnim);
      montar(); return;
    }
    if (acao == OP_F_FIXAR) {
      const ColFolder *pf = NULL;
      int k, n = col_n();
      for (k = 0; k < n; k++) { const ColFolder *f = col_folder(k); if (f && !strcmp(f->id, filPastaId)) pf = f; }
      if (pf) cf_alternar(pf);
      filAlterou = 1;
      // Tirar da Home some com a fileira fixada; nada mais a fazer aqui.
      if (!cf_fixada(filPastaId)) { aberto = 0; return; }
      montar();
    } else if (acao == OP_F_SUBIR || acao == OP_F_DESCER) {
      if (homefil_mover(filChave, acao == OP_F_SUBIR ? -1 : 1)) filAlterou = 1;
    } else if (acao == OP_F_VISTOS) {
      ov_alternar(filChave); filAlterou = 1; montar();
    } else if (acao == OP_F_OCULTAR) {
      if (homefil_ocultar(filChave)) filAlterou = 1;
      aberto = 0;
    }
    return;
  }
  if (doLista) {
    if (acao == OP_L_ABRIR) { listaPedida = 1; aberto = 0; }
    else if (acao == OP_L_FIXAR) { lst_alternar_fixada(&lista); listaAlterou = 1; montar(); }
    else if (acao == OP_L_HOME) { lst_alternar_home(&lista); listaAlterou = 1; montar(); }
    return;
  }
  if (acao == OP_ESTILO) {
    pagina = 1; estFoco = -1; prevAtual = prevAnt = prevRefAtual = prevRefAnt = -1; prevT = 1.0f;
    memset(estAnim, 0, sizeof estAnim);
    montar(); return;
  }
  if (acao == OP_F_MENU) {
    char ch[sizeof filChave], ti[sizeof filTitulo];
    snprintf(ch, sizeof ch, "%s", filChave); snprintf(ti, sizeof ti, "%s", filTitulo);
    ctx_abrir_filmenu(ch, ti, "", 1);
    return;
  }
  if (!ci) return;
  if (acao == OP_ADD_LISTA) {
    pagina = 3; foco = 0; pickIni = 0; lst_minhas_pedir();
    montar(); return;
  }
  if (acao >= OP_PICK_LISTA && acao < OP_PICK_LISTA + CTX_PICK_MAX) {
    LstLista l;
    if (lst_minha(pickIni + acao - OP_PICK_LISTA, &l) && lstesc_adicionar_titulo(ci, &l)) {
      aberto = 0; pagina = 0;
    }
    return;
  }
  if (acao == 0) return;
  if (acao >= OP_EXTRA && acao < OP_EXTRA + nExtras) {
    int k = acao - OP_EXTRA;
    if (extras[k].juntaAssistido && assistidoPossivel(ci, indiceAtual()) &&
        historicoDe(ci) != 1) {
      // "JA ASSISTI": primeiro o historico da conta (so marca). A extra sai
      // quando ele confirma — agora, sem Trakt, ou em ctx_atualizar com o 2xx.
      if (estadoOperacao == CTX_PENDENTE) return;
      iniciarAssistido(indiceAtual(), ci, 1);
      if (estadoOperacao == CTX_PENDENTE) { pendExtra = k; montar(); return; }
      if (estadoOperacao == CTX_FALHA) { montar(); return; }
    }
    // A EXTRA QUE NAO SE DESFAZ (remover amigo) passa pela mesma pagina de
    // confirmacao de "Tirar de Continuar assistindo", com o foco em Cancelar.
    if (extras[k].confirmar && pagina != 2) {
      pagina = 2; confFoco = 1; confExtra = k;
      confAnim[0] = confAnim[1] = 0.0f;
      return;
    }
    extraPedido = k;
    confExtra = -1;
    pagina = 0;
    aberto = 0;
    return;
  }
  // SO A ESPERA BLOQUEIA, e nao "ja houve uma operacao".
  //
  // A guarda antiga era `operacao != CTX_OP_NENHUMA && estado != FALHA`, e
  // como `operacao` nunca volta a NENHUMA enquanto o modal esta aberto, a
  // PRIMEIRA acao confirmada trancava todas as outras: depois de adicionar a
  // biblioteca, "Desmarcar como assistido" no mesmo modal simplesmente nao
  // fazia nada. Era preciso fechar e reabrir, e ninguem adivinha isso.
  //
  // Enquanto a requisicao esta no ar continua valendo esperar: duas escritas
  // simultaneas na mesma superficie e que nao podem acontecer.
  if (estadoOperacao == CTX_PENDENTE) return;
  // Tirar so depois da confirmacao: o primeiro OK abre a pagina 2, e e ela
  // que chama aplicar() de novo, com a pagina ainda em 2.
  if (acao == OP_TIRAR_CONTINUAR && pagina != 2) {
    pagina = 2; confFoco = 0;
    confAnim[0] = confAnim[1] = 0.0f;
    return;
  }
  switch (acao) {
    case OP_LISTA:
      // Captura a intencao ANTES de qualquer escrita. O mesmo valor segue para
      // o POST e so chega ao espelho local depois de uma resposta 2xx.
      intencao = !tituloSalvo(ci);
      // Primeira vez: a ilha pergunta onde o + salva; o menu sai e o titulo e
      // gravado depois da resposta (ilhasalvar.c).
      if (ilhasalvar_perguntar(ci, intencao)) { aberto = 0; return; }
      snprintf(operacaoImdb, sizeof operacaoImdb, "%s", ci->imdb);
      operacao = CTX_OP_LISTA;
      fecharAoConfirmar = doPainel && !intencao;
      opSimkl = 0;
      avisoOp = NULL;
      espelhoAplicado = 0;
      estadoOperacao = CTX_PENDENTE;
      // LOCAL PRIMEIRO E SEMPRE. E sincrono e nao pode falhar por rede, entao
      // acontece fora do jogo de estados abaixo; ver salvos.h para por que ele
      // e o unico destino que sobrevive ao fechamento do app.
      salvos_definir(ci, intencao);
      atividade_salvo(ci, intencao);   // ev=salvo para o Social (atividade.h)
      ilhasalvar_aviso(ci, intencao);  // voo da capa + "Salvo em ..." / "Removido da lista"

      // TRAKT ESCOLHIDO E SEM VINCULO cai no ramo local, como o Simkl sem
      // vinculo ja caia. Antes era CTX_FALHA com a lista local JA escrita: o
      // titulo saia do arquivo, a marca do catalogo ficava, e o "Remover" do
      // painel de Salvos (o padrao de "Onde o + salva" e o Trakt) deixava a
      // linha na tela com "Nao foi possivel". O "+" do detalhe (app.c) sempre
      // marcou o local nesse caso; agora as duas portas concordam.
      if (ajustes_salvos_no_trakt() && trakt_ativo()) {
        if (!trakt_watchlist_tipo(ci->imdb, ci->tipo, intencao))
          estadoOperacao = CTX_FALHA;
      } else if (ajustes_salvos_no_simkl() && simkl_ativo() &&
                 (intencao || simkl_na_plantowatch(ci->imdb))) {
        // PLAN TO WATCH DO SIMKL. O "-" so vai ao Simkl quando o titulo esta
        // no Plan to Watch conhecido: /sync/history/remove apaga o historico
        // do titulo inteiro (ver simkl.h). Fora dele o "-" e so local, pelo
        // ramo de baixo — o mesmo de quem salva na lista do Nuvio.
        opSimkl = 1;
        if (!simkl_lista_tipo(ci->imdb, ci->tipo, intencao))
          estadoOperacao = CTX_FALHA;
      } else {
        // Simkl escolhido e sem vinculo: a lista local ja foi escrita, e o
        // modal diz por que o Simkl nao recebeu.
        avisoOp = simkl_aviso_sem_vinculo(ajustes_salvos_no_simkl());
        // SEM TRAKT NAO HA O QUE ESPERAR, e deixar CTX_PENDENTE aqui seria um
        // modal travado para sempre: ctx_atualizar so sai da espera consultando
        // trakt_operacao_estado, e nenhuma operacao foi aberta la. A escrita
        // local ja terminou, entao o estado correto e "confirmada" — e e ele
        // que faz o espelho (cat_definir_na_lista) rodar no proximo quadro.
        estadoOperacao = CTX_CONFIRMADA;
        espelhoAplicado = 1;
        cat_definir_na_lista(atual, intencao);
        cat_definir_na_lista_imdb(operacaoImdb, intencao);
        desc_remontar_fileiras();
        if (fecharAoConfirmar) { aberto = 0; fecharAoConfirmar = 0; }
      }
      montar();
      break;
    case OP_ASSISTIDO:
      iniciarAssistido(atual, ci, 0);
      montar();
      break;
    case OP_CATEGORIA:
      snprintf(pedCategoriaImdb, sizeof pedCategoriaImdb, "%s", ci->imdb);
      aberto = 0;
      break;
    case OP_RECOMENDAR:
      // FECHA ESTE MODAL E ABRE O COMPARTILHADO. O menu do cartaz falou de um
      // INDICE da home; dali em diante quem manda e uma copia do CatItem, pela
      // mesma razao que salvospainel.c copia: o vetor do catalogo troca de
      // bloco a cada republicacao da descoberta.
      if (recenviar_abrir(ci)) aberto = 0;
      break;
    case OP_DETALHES:
      home_pedir_abrir();
      aberto = 0;
      pagina = 0;
      break;
    case OP_DISPENSAR:
      home_retomar_dispensar();
      aberto = 0;
      pagina = 0;
      break;
    case OP_TIRAR_CONTINUAR: {
      // COPIA ANTES: `ci` aponta para dentro do bloco do catalogo, e
      // desc_tirar_continuar desloca esse bloco (o item seguinte ocupa o
      // lugar). Depois dela `ci->imdb` ja e OUTRO titulo.
      TirarRemoto *tr = (TirarRemoto *)calloc(1, sizeof *tr);
      char imdb[sizeof ci->imdb];
      int temp = ci->temporada, ep = ci->episodio;
      char tit[sizeof ci->titulo];
      snprintf(imdb, sizeof imdb, "%s", ci->imdb);
      snprintf(tit, sizeof tit, "%s", ci->titulo);
      // Efeito local e imediato, ANTES da rede: zera o que a legenda desenha
      // neste indice (o card pode estar numa fileira de catalogo com barra) e
      // desc_tirar_continuar apaga o registro, carimba a remocao e tira o card
      // de "Continuar assistindo" por identidade, subindo a revisao — a home
      // remonta neste mesmo quadro. Antes a revisao nao subia e a home (guarda
      // curto da 1.4) seguia com a contagem velha: o card era coberto pelo
      // vizinho e o ultimo aparecia repetido ate a proxima republicacao
      // (medido em tests/cwremover.sh; ver tirarDaJanela em catalogo.c).
      cat_zerar_progresso(atual);
      desc_tirar_continuar(imdb, temp, ep);
      prog_ocultar_continuar(imdb);   // em disco; volta com episodio novo (progresso.c)
      // AS TRES FONTES, e nao so a local — issue #22.
      //
      // A fileira de retomada e a fusao do registro local, do /sync/playback
      // do Trakt (e do Simkl, #110) e do progresso da conta Nuvio. Apagar so a
      // local fazia a entrada voltar no ciclo seguinte, vinda de qualquer uma
      // das outras: "seleciono remover, o prompt some e nada e removido".
      //
      // EM FIO, e nao aqui: os dois pedidos eram sincronos no fio de DESENHO.
      // Na Samsung isso e XHR sincrono no fio principal do navegador — a tela
      // congela ate os dois servidores responderem, e o menu so fechava
      // depois. Com o carimbo de desc_tirar_continuar a ordem deixou de
      // importar: uma refacao que leia o Trakt antes do DELETE chegar recebe o
      // paused_at velho, e a remocao vence (prog_removido_vence).
      if (tr) {
        snprintf(tr->imdb, sizeof tr->imdb, "%s", imdb);
        prog_chave(tr->chave, sizeof tr->chave, imdb, temp, ep);
        tr->ocultar = 1;   // so o "Tirar de Continuar assistindo" explicito esconde
        tirarRemotoEmFio(tr);   // sem fio: desiste (log), nunca no quadro
      }
      // O Simkl tambem guarda o pausado (issue #110). Ja sai em fio proprio;
      // sem id conhecido (item que nao veio do Simkl) nao faz nada.
      simkl_playback_remover(imdb);
      // SEM DESFAZER: o ponto de retomada foi apagado aqui e nas contas, e nao
      // ha como restaura-lo; por isso a confirmacao do menu continua.
      { IlhaAcao a;
        memset(&a, 0, sizeof a);
        a.icone = "aj_x"; a.frase = i18n("Removido de Continuar assistindo");
        a.titulo = tit;
        a.tipo = ILHA_INFO;
        ilhaacao_feita(&a); }
      aberto = 0;
      pagina = 0;
      break;
    }
  }
}

void ctx_acao_rapida(const CatItem *titulo, int visto) {
  if (aberto || estadoOperacao == CTX_PENDENTE || !titulo) return;
  ctx_abrir_salvo(titulo);
  if (!aberto) return;
  esperandoSoltura = 0;
  for (foco = 0; foco < nOps; foco++)
    if (ops[foco].acao == (visto ? OP_ASSISTIDO : OP_LISTA)) break;
  if (foco >= nOps) { aberto = 0; return; }
  aplicar();
  // Pending writes/errors stay in the existing menu, so their outcome and
  // retry remain visible. Immediate actions leave the preview in place.
  if (estadoOperacao == CTX_CONFIRMADA && pagina == 0) aberto = 0;
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
      e->key.keysym.scancode == NV_SCANCODE_BACK) {
    // Da pagina de estilos, Voltar volta ao menu do titulo — com o foco na
    // entrada de onde a pessoa veio. Sem titulo por tras (colecao), fecha.
    // Voltar CANCELA: nada foi gravado ao mover o foco, so a previa mudou.
    if (pagina == 2) { pagina = 0; confExtra = -1; return; }   // Voltar = Cancelar
    // Da escolha de lista, Voltar volta ao menu, com o foco na entrada de onde veio.
    if (pagina == 3) {
      pagina = 0; montar();
      for (foco = 0; foco < nOps && ops[foco].acao != OP_ADD_LISTA; foco++) {}
      if (foco >= nOps) foco = 0;
      return;
    }
    if (pagina == 1 && !soFileira) {
      pagina = 0; montar();
      for (foco = 0; foco < nOps && ops[foco].acao != OP_ESTILO; foco++) {}
      if (foco >= nOps) foco = 0;
      return;
    }
    aberto = 0; return;
  }
  // CONFIRMACAO: os dois botoes lado a lado; OK no primeiro tira, no
  // segundo cancela. Cima/baixo nao fazem nada (nao ha para onde ir).
  if (pagina == 2) {
    if (k == SDLK_LEFT)  { confFoco = 0; return; }
    if (k == SDLK_RIGHT) { confFoco = 1; return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
      if (confFoco == 0) aplicar(); else { pagina = 0; confExtra = -1; }
    }
    return;
  }
  // Pagina de estilos: cima/baixo trocam a forma, esquerda/direita o tamanho
  // (so nas linhas que tem tamanhos); nada disso grava. OK grava.
  if (pagina == 1) {
#ifdef NV_TOUCH_UI
    if (toquerol_navegacao(e)) toquerol_limpar(&toqueEst);
#endif
    if (k == SDLK_UP)   { if (estFoco > 0) estFoco--; return; }
    if (k == SDLK_DOWN) { if (estFoco + 1 < nEstilos) estFoco++; return; }
    if (estFoco >= 0 && estFoco < nEstilos && estLin[estFoco].n > 1) {
      if (k == SDLK_LEFT)  { if (estTam[estFoco] > 0) estTam[estFoco]--; return; }
      if (k == SDLK_RIGHT) { if (estTam[estFoco] + 1 < estLin[estFoco].n) estTam[estFoco]++; return; }
    }
    if (teclaOk(k)) aplicarEstilo();
    return;
  }
  // Enquanto a requisicao esta no ar, OK nao repete a escrita. O foco continua
  // sendo o do modal e Voltar sempre pode cancelar a espera visual.
  //
  // DEPOIS que ela termina, o OK volta a ser OK. Antes ele virava "fechar" —
  // o modal ficava com os botoes na tela, respondendo ao foco, e o unico
  // efeito de aperta-los era sumir. Somado a guarda de aplicar() logo acima,
  // era a metade visivel do "nao faz nada" no botao de desmarcar.
  if (operacao != CTX_OP_NENHUMA && estadoOperacao == CTX_PENDENTE) return;
  if (modoInline() && k == SDLK_LEFT)  { if (foco > 0) foco--; return; }
  if (modoInline() && k == SDLK_RIGHT) { if (foco + 1 < nOps) foco++; return; }
  if (k == SDLK_UP) {
    if (foco > 0) foco--;
    else if (pagina == 3 && pickIni > 0) { pickIni--; montar(); }
    return;
  }
  if (k == SDLK_DOWN) {
    if (foco + 1 < nOps) foco++;
    else if (pagina == 3 && pickIni + pickN < pickTotal) { pickIni++; montar(); }
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) { aplicar(); return; }
}

void ctx_atualizar(float dt, Uint32 agora) {
  int i;
  int atual;
  if (!holdObservador) {
    SDL_AddEventWatch(observarHold, NULL);
    holdObservador = 1;
  }
  if (holdAtivo && segurar_venceu(agora, holdDesde ? holdDesde : 1, "menu")) holdPronto = 1;
  if (ajustes_animacoes_reduzidas())
    anim = aberto ? 1.0f : 0.0f;
  else if (inlineOn && doPainel) {
    // O ACORDEAO do painel Social/Salvos anda na mola da pilula da ilha
    // (movimento.h), como o resto do painel; o repique fica na folga de 1.0.
    anim = mov_mola_assenta(&animV, anim, aberto ? 1.0f : 0.0f, dt, MOV_PILULA_W, MOV_PILULA_Z);
    if (anim > 1.0f) anim = 1.0f;
  } else
    anim = anim_mola(anim, aberto ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  // A extensao sai da borda do menu DEPOIS que ele assentou (meio caminho da
  // mola): primeiro as opcoes, logo em seguida as informacoes.
  { float alvo = (aberto && pagina == 0 && infoPossivel(itemAtual()) && anim > 0.5f) ? 1.0f : 0.0f;
    if (!aberto) alvo = 0.0f;
    infoT = ajustes_animacoes_reduzidas() ? alvo : anim_mola(infoT, alvo, dt, NV_MOLA_TELA); }
  for (i = 0; i < CTX_MAX; i++)
    focoAnim[i] = ajustes_animacoes_reduzidas()
      ? (aberto && foco == i ? 1.0f : 0.0f)
      : anim_mola(focoAnim[i], aberto && foco == i ? 1.0f : 0.0f,
                  dt, NV_MOLA_FOCO);
  for (i = 0; i < 2; i++)
    confAnim[i] = ajustes_animacoes_reduzidas()
      ? (pagina == 2 && confFoco == i ? 1.0f : 0.0f)
      : anim_mola(confAnim[i], pagina == 2 && confFoco == i ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
  // Pagina de estilos: o foco da lista e a troca da previa. So aqui, e so com
  // a pagina aberta — fechada, nada disto anda nem desenha.
  if (aberto && pagina == 1) {
    for (i = 0; i < FIL_TIPO_N; i++)
      estAnim[i] = ajustes_animacoes_reduzidas()
        ? (estFoco == i ? 1.0f : 0.0f)
        : anim_mola(estAnim[i], estFoco == i ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
    if (estFoco >= 0 && estFoco < nEstilos && estTipo(estFoco) != prevAtual) {
      // A primeira forma entra com o proprio modal; as seguintes, com a mola.
      prevAnt = prevAtual; prevRefAnt = prevRefAtual;
      prevAtual = estTipo(estFoco); prevRefAtual = estRef(estFoco);
      prevT = prevAnt < 0 ? 1.0f : 0.0f;
    }
    prevT = ajustes_animacoes_reduzidas() ? 1.0f : anim_mola(prevT, 1.0f, dt, NV_MOLA_FOCO);
  }

  atual = indiceAtual();
  if (aberto && !soFileira && !doLista && !itemAtual()) { aberto = 0; return; }
  // As listas chegam depois do OK: remonta quando o que a pagina 3 mostra mudou.
  if (aberto && pagina == 3 &&
      (lst_minhas_carregando() != pickCarregando ||
       lst_minhas_n() != pickTotal))
    montar();

  if (operacao != CTX_OP_NENHUMA && estadoOperacao == CTX_PENDENTE) {
    int novo = opSimkl ? simkl_lista_estado() : trakt_operacao_estado(operacao);
    if (novo == CTX_CONFIRMADA || novo == CTX_FALHA) {
      estadoOperacao = novo;
      if (pendExtra >= 0) {
        if (novo == CTX_CONFIRMADA) { extraPedido = pendExtra; aberto = 0; pagina = 0; }
        pendExtra = -1;
      }
      if (!espelhoAplicado && itemAtual()) {
        const CatItem *ci = itemAtual();
        if (ci && novo == CTX_CONFIRMADA) {
          if (operacao == CTX_OP_LISTA) {
            cat_definir_na_lista(atual, intencao);
            cat_definir_na_lista_imdb(operacaoImdb, intencao);
            // O servico confirmou: o titulo ja entra no filtro dele.
            if (intencao)
              cat_marcar_fonte_imdb(operacaoImdb, opSimkl ? CAT_FONTE_SIMKL : CAT_FONTE_TRAKT);
          } else {
            espelharAssistido(atual, ci, intencao);
            anunciarAssistido(intencao);
          }
        }
        espelhoAplicado = 1;
        // A HOME TEM DE MUDAR NA HORA.
        //
        // cat_historico_definir_id so mexe na tabela lateral de historico, e
        // nenhuma fileira le dela: quem monta as fileiras e a descoberta, a
        // partir do que o Trakt respondeu. Sem este pedido a mudanca so
        // aparecia no ciclo seguinte — o "tiro de assistido e a home nao da
        // refresh, tenho que sair e voltar" do relato.
        //
        // desc_remontar_fileiras remonta SEM REDE, a partir do que ja esta em
        // memoria; e a mesma porta que a mudanca de limite de fileiras usa.
        desc_remontar_fileiras();
        montar();
      }
      // Remocao pelo painel confirmada: o menu sai e a lista, que ja se
      // remontou pela revisao, fica com o foco na linha seguinte. Falhou: o
      // menu fica, com o "Nao foi possivel" do subtitulo.
      if (fecharAoConfirmar) {
        if (novo == CTX_CONFIRMADA) aberto = 0;
        fecharAoConfirmar = 0;
      }
    }
  }
}

// PONTEIRO (#99): passar por cima de uma opcao a foca (a mesma variavel de
// cima/baixo); o clique e o OK. Clicar fora do cartao fecha, como o Voltar.
static void ponteiroCtxOpcao(int i, int b) { (void)b; if (i >= 0 && i < nOps) foco = i; }
static void ponteiroConfFoco(int i, int b) { (void)b; confFoco = i; }
static void ponteiroConfOk(int i, int b) {
  (void)b; confFoco = i;
  if (i == 0) aplicar(); else pagina = 0;
}
static void ponteiroCtxFora(int a, int b) { (void)a; (void)b; aberto = 0; }
// `b` > 0 e o segmento de tamanho b-1 da linha (o alvo menor, por cima dela).
static void ponteiroEstFoco(int i, int b) {
  if (i < 0 || i >= nEstilos) return;
  estFoco = i;
  if (b > 0 && b - 1 < estLin[i].n) estTam[i] = b - 1;
}
static void ponteiroEstOk(int i, int b) {
  if (i < 0 || i >= nEstilos) return;
  ponteiroEstFoco(i, b);
  aplicarEstilo();
}

// GLASS UI (dono, 02/10, mockup "ilha" tela 7): TODO MODAL E UMA ILHA — o
// material da folha de Fontes e da ilha do relogio. Sombra curta, miolo de
// vidro (gfx_vidro_folha) ou solido, uma luz larga e fraca BRANCA no canto de
// cima (era a luz na cor do realce: o acento agora so marca estado) e sem aro.
static void ilhaCtx(GfxRect p, float raioPx, float a) {
  const int vid = ajustes_vidro();
  float raio = raioPx / p.h;
  gfx_sombra_sob((GfxRect){ p.x - 18.0f, p.y - 8.0f, p.w + 36.0f, p.h + 40.0f }, 1.0f, 0, 0.5f,
                 0, 0, 0, .42f * a, p, raioPx, vid ? 0.0f : .98f * a);
  // O vidro le a textura assada e deixava o texto da fileira aparecer por
  // tras; uma base quase solida por baixo (a cor do modo sem vidro) fecha.
  if (vid) { gfx_cor(p, raio, .071f, .075f, .086f, .985f * a); gfx_vidro_folha(p, raio, a); }
  else gfx_cor(p, raio, .071f, .075f, .086f, .98f * a);
  gfx_luz_canto(p, raio, p.w * .25f, -p.h * .25f, p.w * .9f, 1, 1, 1, (vid ? .06f : .04f) * a);
}

// KICKER das ilhas: 15/700 em caixa alta (a do idioma), espacado .14em,
// branco a 45 % — o "CONTINUAR ASSISTINDO" da confirmacao do mockup.
static void kickerCtx(const char *t, float x, float y, float a) {
  char up[160];
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(t));
  txt_tracking(TXT_MINI, up, 243, 242, 239, x, y, .45f * a, 2.1f);
}

// UMA LINHA DE LISTA dentro da ilha (as opcoes do cartaz, as formas da
// fileira): a .it do mockup — pilula de 60, icone de 24 a 20 da borda, 18 ate
// o rotulo (22/500 a 72 %). O FOCO = SUPERFICIE UM DEGRAU MAIS CLARA (branco
// a 14 % no vidro, cinza opaco no solido) e o rotulo acende inteiro, sem
// pilula cheia nem contorno — como o menu e o painel de Salvos. A pilula
// cheia no acento fica para botao.
#define CTX_ICONE 24.0f
// `tinta` < 0 = a linha da ilha escura (texto claro, foco por superficie). De 0
// a 1 = a linha SOBRE UMA SUPERFICIE CHEIA (o painel da aba, no realce): texto
// e icone nessa tinta e o foco e a propria tinta a 12 %, que escurece o
// realce claro e clareia o escuro.
static void linhaCtxTinta(GfxRect r, const char *rot, const char *icone, float f, float a,
                          float tinta) {
  float k = f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
  float lum = .72f + .28f * k;
  int cr = 243, cg = 242, cb = 239;
  if (tinta >= 0.0f) {
    cr = cg = cb = (int)(tinta * 255.0f + 0.5f);
    lum = .80f + .20f * k;
    if (k > 0.01f) gfx_cor(r, 0.5f, tinta, tinta, tinta, .12f * k * a);
  } else if (k > 0.01f) {
    if (ajustes_vidro()) gfx_cor(r, 0.5f, 1, 1, 1, .14f * k * a);
    else gfx_cor(r, 0.5f, .19f, .195f, .215f, k * a);
  }
  if (icone && icone[0])
    gfx_icone((GfxRect){ r.x + 20.0f, r.y + (r.h - CTX_ICONE) * 0.5f, CTX_ICONE, CTX_ICONE },
              icone, cr / 255.0f, cg / 255.0f, cb / 255.0f, lum * a);
  { TxtLinha t = txt_linha_corta(TXT_PG_ROTULO, rot, cr, cg, cb, 255,
                                 r.w - 20.0f - CTX_ICONE - 18.0f - 20.0f);
    txt_desenhar_alpha(t, r.x + 20.0f + CTX_ICONE + 18.0f, r.y + (r.h - t.h) * 0.5f, lum * a); }
}
static void linhaCtx(GfxRect r, const char *rot, const char *icone, float f, float a) {
  linhaCtxTinta(r, rot, icone, f, a, -1.0f);
}

// --- A MESMA ILHA, PARA OS MENUS QUE NAO SAO DE CARTAZ ----------------------
//
// O menu de visto do episodio (episodios.c) era um cartao proprio — 800 de
// largura, still no cabecalho, linhas de 62, rodape de teclas — e ao lado do
// menu do cartaz parecia de outro app (dono, 08/10). Em vez de uma segunda
// copia das medidas, quem tem um menu entrega o nome, a linha de apoio e os
// rotulos, e ESTE arquivo desenha: a ilha, o cabecalho e as linhas sao os
// mesmos de cima, com as mesmas constantes. O estado (foco, molas, teclas,
// acoes) continua de quem abriu.
//
// A LARGURA e a do menu do cartaz (CTX_W) e so cresce quando um rotulo nao
// cabe — "Temporada inteira (10 episódios)" nao cabe em 420 —, ate CTX_W_MAX;
// dali em diante o rotulo corta com reticencias, como em todo lugar. O nome
// nunca alarga: titulo de episodio comprido corta.
#define CTX_W_MAX 600.0f
float ctx_menu_largura(const CtxLinha *l, int n) {
  float w = CTX_W;
  int i;
  for (i = 0; i < n; i++) {
    float p = CTX_ILHA_PAD * 2.0f + 20.0f + CTX_ICONE + 18.0f +
              (float)txt_largura(TXT_PG_ROTULO, l[i].rot) + 2.0f + 20.0f;
    if (p > w) w = p;
  }
  return w > CTX_W_MAX ? CTX_W_MAX : w;
}
float ctx_menu_altura(int n) {
  return CTX_ILHA_PAD * 2.0f + CTX_CAB + (float)n * (CTX_LINHA + CTX_GAP);
}
float ctx_menu_ao_lado(GfxRect cartaz, float w) {
  float maxX = NV_TELA_W - CTX_BORDA - w;
  float direita = cartaz.x + cartaz.w + CTX_AO_LADO;
  float esquerda = cartaz.x - CTX_AO_LADO - w;
  if (direita <= maxX) return direita;
  if (esquerda >= CTX_BORDA) return esquerda;
  return maxX < CTX_BORDA ? CTX_BORDA : maxX;
}
void ctx_menu_veu(float a) {
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.94f * a);
}
void ctx_menu_ilha(GfxRect p, float raioPx, float a) { ilhaCtx(p, raioPx, a); }
void ctx_menu_linha(GfxRect r, const char *rot, const char *icone, float f, float a,
                    float tinta) {
  linhaCtxTinta(r, rot, icone, f, a, tinta);
}
float ctx_menu_passo(void) { return CTX_LINHA + CTX_GAP; }
// O cabecalho de duas linhas, como o do menu do cartaz sem logo: o nome
// (24/700) e o apoio embaixo (16), 4 px depois.
static void cabecalhoCtx(float hx, float hy, float hw, const char *nome,
                         const char *meta, float metaA, float a) {
  TxtLinha n = txt_linha_corta(TXT_ILHA_NOME, nome, 243, 242, 239, 255, hw);
  TxtLinha m = txt_linha_corta(TXT_ILHA_APOIO, meta, 243, 242, 239, 255, hw);
  txt_desenhar_alpha(n, hx, hy, a);
  txt_desenhar_alpha(m, hx, hy + n.h + 4.0f, metaA * a);
}
void ctx_menu_desenhar(GfxRect m, const char *nome, const char *meta, float metaA,
                       const CtxLinha *l, int n, const float *focoL, float linhasA,
                       float a, PonteiroFn focar) {
  int i;
  ilhaCtx(m, CTX_ILHA_RAIO, a);
  cabecalhoCtx(m.x + CTX_ILHA_PAD + 20.0f, m.y + CTX_ILHA_PAD + 12.0f,
               m.w - 2.0f * (CTX_ILHA_PAD + 20.0f), nome, meta, metaA, a);
  for (i = 0; i < n; i++) {
    GfxRect r = { m.x + CTX_ILHA_PAD,
                  m.y + CTX_ILHA_PAD + CTX_CAB + CTX_GAP + (float)i * (CTX_LINHA + CTX_GAP),
                  m.w - CTX_ILHA_PAD * 2.0f, CTX_LINHA };
    if (focar && a > 0.5f) ponteiro_alvo(r.x, r.y, r.w, r.h, focar, NULL, i, 0);
    linhaCtx(r, l[i].rot, l[i].icone, focoL ? focoL[i] : 0.0f, linhasA * a);
  }
}

// --- O MODAL DE ESTILO ------------------------------------------------------
//
// O mesmo cartao flutuante do menu (vidro ou folha com a luz do realce), mais
// largo: a lista de formas a esquerda e, a direita, um PALCO na cor do fundo da
// Home com o nome da fileira e a fileira desenhada na forma em foco. O palco e
// a Home em miniatura de proposito — a pergunta da pessoa e "como fica la".
#define EST_W       1680.0f
#define EST_LISTA_W  500.0f
#define EST_LINHA     60.0f
#define EST_GAP       10.0f
#define EST_COL_GAP   48.0f
#define EST_PALCO_H  520.0f   // altura minima do palco da previa
#define EST_PALCO_Y  140.0f   // do topo da coluna ao palco: nome + frase

// O SEGMENTADO DE TAMANHO, na ponta direita da pilula da linha `i`: uma letra
// por tamanho (a primeira da palavra traduzida — P M G, S M L), o escolhido
// cheio na cor do texto da pilula e o gravado com um anel. Na tinta da pilula:
// escura quando ela esta acesa (branca), clara em repouso.
#define EST_SEG_W   46.0f
#define EST_SEG_H   40.0f
#define EST_SEG_GAP  6.0f
static void desenhaTamanhos(GfxRect r, int i, int atual, float a) {
  float fr, fg, fb, ti = botao_cor_foco(&fr, &fg, &fb);
  // A linha agora acende por SUPERFICIE (linhaCtx), nunca no acento: a tinta
  // e sempre a clara de repouso.
  int aceso = 0, j, n = estLin[i].n;
  (void)estAnim;
  // A tinta e a do rotulo da pilula (botao_pilula): a de botao_cor_foco acesa,
  // clara em repouso. A letra escolhida vai no fundo da pilula — o realce
  // aceso, o escuro do cartao em repouso — sobre um disco na tinta.
  float tinta = aceso ? ti : 0.93f;
  float lr = aceso ? fr : 0.08f, lg = aceso ? fg : 0.085f, lb = aceso ? fb : 0.1f;
  float x0 = r.x + r.w - 10.0f - (float)n * EST_SEG_W - (float)(n - 1) * EST_SEG_GAP;
  float y0 = r.y + (r.h - EST_SEG_H) * 0.5f;
  for (j = 0; j < n; j++) {
    GfxRect c = { x0 + (float)j * (EST_SEG_W + EST_SEG_GAP), y0, EST_SEG_W, EST_SEG_H };
    const char *pal = i18n(fil_estilo_tam_palavra(j));
    char letra[8];
    int k = 0, esc = j == estTam[i], t8 = (int)(tinta * 255.0f + 0.5f);
    TxtLinha l;
    // A primeira letra, inteira: um caractere UTF-8 e o lider mais as
    // continuacoes (10xxxxxx).
    if (pal[0]) { letra[k++] = pal[0];
      while (k < 6 && ((unsigned char)pal[k] & 0xC0) == 0x80) { letra[k] = pal[k]; k++; } }
    letra[k] = 0;
    if (aberto && a > 0.5f)
      ponteiro_alvo(c.x, c.y, c.w, c.h, ponteiroEstFoco, ponteiroEstOk, i, j + 1);
    if (esc) gfx_cor(c, 0.5f, tinta, tinta, tinta, 0.95f * a);
    else if (estLin[i].tipos[j] == atual)
      gfx_anel(c, 0.5f, 1.5f, tinta, tinta, tinta, 0.6f * a);
    l = esc ? txt_linha(TXT_DET_BOTAO, letra, (int)(lr * 255.0f + 0.5f),
                        (int)(lg * 255.0f + 0.5f), (int)(lb * 255.0f + 0.5f), 255)
            : txt_linha(TXT_DET_BOTAO, letra, t8, t8, t8, 255);
    txt_desenhar_alpha(l, c.x + (c.w - (float)l.w) * 0.5f, c.y + (c.h - (float)l.h) * 0.5f,
                       (esc ? 1.0f : 0.72f) * a);
  }
}

#ifdef NV_TOUCH_UI
#define EST_TOQUE_LINHA 72.0f
#define EST_TOQUE_SEG   56.0f
#define EST_TOQUE_GAP    8.0f
typedef struct { GfxRect modal, lista, previa, cancelar; } EstTelefoneGeo;

static float estListaAltura(void) {
  return fmaxf(0.0f, (float)nEstilos * (EST_TOQUE_LINHA + EST_GAP) - EST_GAP);
}

static EstTelefoneGeo estTelefoneGeo(float a) {
  EstTelefoneGeo g;
  int retrato = NV_LAYOUT_REAL_H > NV_LAYOUT_REAL_W;
  const float pad = 28.0f, cab = 100.0f, rod = 64.0f;
  float w = fminf(EST_W, NV_TELA_W - 64.0f);
  float h = fminf(retrato ? pad * 2 + cab + rod + estListaAltura() + 24.0f + 440.0f
                         : 934.0f, NV_TELA_H - 64.0f);
  g.modal = (GfxRect){ (NV_TELA_W - w) * .5f,
                       (NV_TELA_H - h) * .5f + (1.0f - a) * 40.0f, w, h };
  g.lista = (GfxRect){ g.modal.x + pad, g.modal.y + pad + cab,
                       w - pad * 2, h - pad * 2 - cab - rod };
  if (retrato) {
    float ph = fminf(440.0f, g.lista.h * .46f);
    g.lista.h -= ph + 24.0f;
    g.previa = (GfxRect){ g.lista.x, g.lista.y + g.lista.h + 24.0f, g.lista.w, ph };
  } else {
    g.lista.w = fminf(EST_LISTA_W, g.lista.w * .42f);
    g.previa = (GfxRect){ g.lista.x + g.lista.w + 32.0f, g.lista.y,
                         w - pad * 2 - g.lista.w - 32.0f, g.lista.h };
  }
  g.cancelar = (GfxRect){ g.modal.x + w - pad - 160.0f,
                         g.modal.y + h - pad - EST_TOQUE_SEG, 160.0f, EST_TOQUE_SEG };
  return g;
}

static int toqueEstRolar(const PonteiroRolagem *e) {
  if (!aberto || pagina != 1 || !telefoneui_ativo()) return 0;
  return toquerol_evento(&toqueEst, e);
}

static void estToqueAlvo(GfxRect r, GfxRect janela, int i, int b) {
  if (!aberto) return;
  ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, janela.y, janela.y + janela.h,
                      ponteiroEstFoco, ponteiroEstOk, i, b);
}

static void estLinhaTelefone(GfxRect r, GfxRect janela, int i, int atual, float a) {
  int n = estLin[i].n, marcado = 0;
  float seg = n > 1 ? n * EST_TOQUE_SEG + (n - 1) * EST_TOQUE_GAP + 20.0f : 0;
  float k = fmaxf(0.0f, fminf(1.0f, estAnim[i]));
  for (int j = 0; j < n; j++) if (estLin[i].tipos[j] == atual) marcado = 1;
  if (a > .5f) estToqueAlvo(r, janela, i, 0);
  if (k > .01f) {
    if (ajustes_vidro()) gfx_cor(r, .5f, 1, 1, 1, .14f * k * a);
    else gfx_cor(r, .5f, .19f, .195f, .215f, k * a);
  }
  if (marcado) gfx_icone((GfxRect){r.x + 20, r.y + (r.h - CTX_ICONE) * .5f,
                                  CTX_ICONE, CTX_ICONE}, "check", .95f, .95f, .94f, a);
  TxtLinha t = txt_linha_corta(TXT_PG_ROTULO, estLin[i].rotulo, 243, 242, 239, 255,
                              r.w - 62.0f - 20.0f - seg);
  txt_desenhar_alpha(t, r.x + 62, r.y + (r.h - t.h) * .5f, (.72f + .28f * k) * a);
  if (n < 2) return;
  float sx = r.x + r.w - 10.0f - n * EST_TOQUE_SEG - (n - 1) * EST_TOQUE_GAP;
  for (int j = 0; j < n; j++) {
    GfxRect c = {sx + j * (EST_TOQUE_SEG + EST_TOQUE_GAP),
                  r.y + (r.h - EST_TOQUE_SEG) * .5f, EST_TOQUE_SEG, EST_TOQUE_SEG};
    const char *pal = i18n(fil_estilo_tam_palavra(j));
    char letra[8]; int p = 0, esc = j == estTam[i];
    if (pal[0]) { letra[p++] = pal[0];
      while (p < 6 && ((unsigned char)pal[p] & 0xC0) == 0x80) { letra[p] = pal[p]; p++; } }
    letra[p] = 0;
    if (a > .5f) estToqueAlvo(c, janela, i, j + 1);
    if (esc) gfx_cor(c, .5f, .93f, .93f, .93f, .95f * a);
    else if (estLin[i].tipos[j] == atual) gfx_anel(c, .5f, 1.5f, .93f, .93f, .93f, .6f * a);
    t = txt_linha(TXT_DET_BOTAO, letra, esc ? 20 : 237, esc ? 22 : 237, esc ? 26 : 237, 255);
    txt_desenhar_alpha(t, c.x + (c.w - t.w) * .5f, c.y + (c.h - t.h) * .5f, a);
  }
}

static void desenhaEstilosTelefone(float a) {
  EstTelefoneGeo g = estTelefoneGeo(a);
  int atual = fil_tipo(filChave);
  float maximo = fmaxf(0, estListaAltura() - g.lista.h);
  estRolY = toquerol_clamp(estRolY, 0, maximo);
  if (!toqueEst.livre && estFoco >= 0 && estFoco < nEstilos) {
    float y = estFoco * (EST_TOQUE_LINHA + EST_GAP);
    if (y < estRolY) estRolY = y;
    if (y + EST_TOQUE_LINHA > estRolY + g.lista.h) estRolY = y + EST_TOQUE_LINHA - g.lista.h;
    estRolY = toquerol_clamp(estRolY, 0, maximo);
  }
  toquerol_vincular(&toqueEst, g.lista, gfx_escala(), 0, maximo, 1, &estRolY);
  ponteiro_rolagem(toqueEstRolar);
  gfx_cor((GfxRect){0, 0, NV_TELA_W, NV_TELA_H}, 0, 0, 0, 0, .55f * a);
  if (aberto && a > .5f && ponteiro_ativo()) {
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ponteiroCtxFora, 0, 0);
    ponteiro_alvo(g.modal.x, g.modal.y, g.modal.w, g.modal.h, NULL, NULL, 0, 0);
  }
  ilhaCtx(g.modal, 36, a);
  kickerCtx("Estilo da fileira", g.modal.x + 28, g.modal.y + 28, a);
  TxtLinha titulo = txt_linha_corta(TXT_ROW_TITULO, filTitulo, 243, 242, 239, 255, g.modal.w - 56);
  txt_desenhar_alpha(titulo, g.modal.x + 28, g.modal.y + 54, a);
  gfx_recorte(g.lista.x, g.lista.y, g.lista.w, g.lista.h);
  for (int i = 0; i < nEstilos; i++) {
    GfxRect r = {g.lista.x, g.lista.y + i * (EST_TOQUE_LINHA + EST_GAP) - estRolY,
                 g.lista.w, EST_TOQUE_LINHA};
    if (r.y + r.h <= g.lista.y || r.y >= g.lista.y + g.lista.h) continue;
    estLinhaTelefone(r, g.lista, i, atual, a);
  }
  gfx_sem_recorte();
  if (estFoco >= 0 && estFoco < nEstilos) {
    int jt = estTam[estFoco] >= 0 && estTam[estFoco] < estLin[estFoco].n ? estTam[estFoco] : 0;
    int salvo = estTipo(estFoco) == atual;
    float bw = salvo ? badge_largura("Atual") + 18.0f : 0;
    TxtLinha nome = txt_linha_corta(TXT_TITULO3, estLin[estFoco].nomes[jt], 245, 248, 255, 255, g.previa.w - bw);
    txt_desenhar_alpha(nome, g.previa.x, g.previa.y, a);
    if (salvo) badge_desenhar(g.previa.x + g.previa.w - bw + 18,
                              g.previa.y + (nome.h - BADGE_H) * .5f, "Atual", BADGE_REALCE, a);
    txt_bloco_corta(TXT_CAPTION2, i18n(fil_estilo_ajuda(filChave, estTipo(estFoco))),
                    170, 174, 184, g.previa.x, g.previa.y + nome.h + 8, g.previa.w, 30, a * .92f, 2);
  }
  GfxRect palco = {g.previa.x, g.previa.y + EST_PALCO_Y, g.previa.w, g.previa.h - EST_PALCO_Y};
  gfx_cor(palco, 26.0f / palco.h, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, .96f * a);
  titulo = txt_linha_corta(TXT_ROW_TITULO, filTitulo, 245, 246, 249, 255, palco.w - 48);
  txt_desenhar_alpha(titulo, palco.x + 24, palco.y + 20, a);
  GfxRect area = {palco.x + 24, palco.y + 40 + titulo.h, palco.w - 48,
                  palco.h - 60 - titulo.h};
  if (prevAnt >= 0 && prevT < .995f) {
    GfxRect s = area; s.x -= 24 * prevT;
    home_previa_fileira(filChave, prevAnt, prevRefAnt, s, a * (1 - prevT));
  }
  if (prevAtual >= 0) {
    GfxRect e = area; e.x += 24 * (1 - prevT);
    e.w = fminf(e.w, palco.x + palco.w - 24 - e.x);
    home_previa_fileira(filChave, prevAtual, prevRefAtual, e, a * prevT);
  }
  titulo = txt_linha_corta(TXT_ILHA_APOIO, "Toque para aplicar", 243, 242, 239, 255,
                           g.modal.w - 56 - g.cancelar.w - 24);
  txt_desenhar_alpha(titulo, g.modal.x + 28, g.cancelar.y + (g.cancelar.h - titulo.h) * .5f, a * .6f);
  gfx_cor(g.cancelar, .5f, 1, 1, 1, .08f * a);
  titulo = txt_linha_corta(TXT_PG_ROTULO, "Fechar", 243, 242, 239, 255, g.cancelar.w - 28);
  txt_desenhar_alpha(titulo, g.cancelar.x + (g.cancelar.w - titulo.w) * .5f,
                     g.cancelar.y + (g.cancelar.h - titulo.h) * .5f, a);
  if (aberto && a > .5f) ponteiro_alvo(g.cancelar.x, g.cancelar.y, g.cancelar.w, g.cancelar.h,
                                      NULL, ponteiroCtxFora, 0, 0);
}
#endif

static void desenhaEstilos(float a) {
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) { desenhaEstilosTelefone(a); return; }
#endif
  float lista = (float)nEstilos * (EST_LINHA + EST_GAP) - EST_GAP;
  float corpo = lista > EST_PALCO_H + EST_PALCO_Y ? lista : EST_PALCO_H + EST_PALCO_Y;
  float alt = CTX_PAD * 2.0f + 116.0f + corpo + CTX_RODAPE;
  float x = (NV_TELA_W - EST_W) * 0.5f, y = (NV_TELA_H - alt) * 0.5f;
  float topo, dx, dw;
  int i, atual = fil_tipo(filChave);
  float ar_, ag_, ab_;
  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, 0, 0, 0, 0.55f * a); }
  if (aberto && a > 0.5f && ponteiro_ativo()) {
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ponteiroCtxFora, 0, 0);
    ponteiro_alvo(x, y, EST_W, alt, NULL, NULL, 0, 0);
  }
  y += (1.0f - a) * 40.0f;
  ajustes_acento(&ar_, &ag_, &ab_);
  ilhaCtx((GfxRect){ x, y, EST_W, alt }, 36.0f, a);
  (void)ar_; (void)ag_; (void)ab_;
  // CABECALHO DE MODAL DA ILHA (o da confirmacao do mockup "ilha" tela 7):
  // kicker em caixa alta espacada a 45 % e o nome da fileira em negrito.
  kickerCtx("Estilo da fileira", x + CTX_PAD, y + CTX_PAD, a);
  { TxtLinha t = txt_linha_corta(TXT_ROW_TITULO, filTitulo, 243, 242, 239, 255,
                                 EST_W - CTX_PAD * 2.0f);
    txt_desenhar_alpha(t, x + CTX_PAD, y + CTX_PAD + 26.0f, a); }
  topo = y + CTX_PAD + 116.0f;

  // A LISTA. O visto marca a forma que vale agora; o foco e so a previa.
  for (i = 0; i < nEstilos; i++) {
    GfxRect r = { x + CTX_PAD, topo + (float)i * (EST_LINHA + EST_GAP), EST_LISTA_W, EST_LINHA };
    int j, temAtual = 0;
    for (j = 0; j < estLin[i].n; j++) if (estLin[i].tipos[j] == atual) temAtual = 1;
    if (aberto && a > 0.5f)
      ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroEstFoco, ponteiroEstOk, i, 0);
    linhaCtx(r, estLin[i].rotulo, temAtual ? "check" : "", estAnim[i], a);
    if (estLin[i].n > 1) desenhaTamanhos(r, i, atual, a);
  }

  // A DIREITA: o nome da forma em foco, a frase dela e o palco.
  dx = x + CTX_PAD + EST_LISTA_W + EST_COL_GAP;
  dw = x + EST_W - CTX_PAD - dx;
  if (estFoco >= 0 && estFoco < nEstilos) {
    float ty = topo;
    int jt = estTam[estFoco] >= 0 && estTam[estFoco] < estLin[estFoco].n ? estTam[estFoco] : 0;
    TxtLinha nome = txt_linha(TXT_TITULO3, estLin[estFoco].nomes[jt], 245, 248, 255, 255);
    txt_desenhar_alpha(nome, dx, ty, a);
    if (estTipo(estFoco) == atual)
      badge_desenhar(dx + nome.w + 18.0f, ty + (nome.h - BADGE_H) * 0.5f, "Atual",
                     BADGE_REALCE, a);
    ty += nome.h + 8.0f;
    txt_bloco_corta(TXT_CAPTION2, i18n(fil_estilo_ajuda(filChave, estTipo(estFoco))),
                    170, 174, 184, dx, ty, dw, 30.0f, a * 0.92f, 2);
  }
  // O palco comeca abaixo das duas linhas da frase e vai ate a base da lista.
  { GfxRect palco = { dx, topo + EST_PALCO_Y, dw, corpo - EST_PALCO_Y };
    TxtLinha tl;
    GfxRect area;
    // O palco e uma superficie dentro da ilha, sem aro (regra 1 do Glass UI).
    gfx_cor(palco, 26.0f / palco.h, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 0.96f * a);
    tl = txt_linha_corta(TXT_ROW_TITULO, filTitulo, 245, 246, 249, 255, palco.w - 64.0f);
    txt_desenhar_alpha(tl, palco.x + 32.0f, palco.y + 28.0f, a);
    area = (GfxRect){ palco.x + 32.0f, palco.y + 28.0f + tl.h + 20.0f,
                      palco.w - 32.0f, palco.h - (28.0f + tl.h + 20.0f) - 28.0f };
    // A forma que sai desliza para a esquerda e apaga; a nova entra pela
    // direita. 24 px: o bastante para ler "trocou", pouco para competir.
    if (prevAnt >= 0 && prevT < 0.995f) {
      GfxRect s = area; s.x -= 24.0f * prevT;
      home_previa_fileira(filChave, prevAnt, prevRefAnt, s, a * (1.0f - prevT));
    }
    if (prevAtual >= 0) {
      GfxRect e = area; e.x += 24.0f * (1.0f - prevT);
      // O recorte acompanha a area deslocada; sem o corte de volta a direita
      // a arte que entra passaria da borda do palco.
      if (e.x + e.w > palco.x + palco.w) e.w = palco.x + palco.w - e.x;
      home_previa_fileira(filChave, prevAtual, prevRefAtual, e, a * prevT);
    } }

  // O gesto de tamanho so aparece no rodape quando a linha em foco tem um.
  { int comTam = estFoco >= 0 && estFoco < nEstilos && estLin[estFoco].n > 1;
    TxtLinha t = txt_linha(TXT_ILHA_APOIO, comTam
                   ? "↑ ↓ Escolher   ·   ← → Tamanho   ·   OK Aplicar   ·   Voltar Cancelar"
                   : "↑ ↓ Escolher   ·   OK Aplicar   ·   Voltar Cancelar",
                           243, 242, 239, 255);
    txt_desenhar_alpha(t, x + CTX_PAD, y + alt - CTX_PAD - t.h, a * 0.42f); }
}

// --- A CONFIRMACAO (pagina 2) -----------------------------------------------
//
// A ilha centralizada do mockup "ilha" tela 7: 720 de largura, raio 36, 44 de
// ar; kicker "CONTINUAR ASSISTINDO", a pergunta em 36/700, o texto em 20/400 a
// 62 % com entrelinha 1,5, e os botoes de 60 em pilula a 34 do texto. Botao em
// foco = pilula cheia no acento, o mesmo do chip das Fontes (streams.c); o
// outro, a pilula de vidro (branco a 8 %).
//
// O TEXTO DIZ O QUE ACONTECE DE VERDADE (desc_tirar_continuar e os DELETE de
// aplicar): o ponto de retomada e apagado aqui, no Trakt, no Simkl e na conta
// — nao "fica guardado" —, e o carimbo prog_marcar_removido segura o card fora
// ate haver um registro MAIS NOVO que a remocao, isto e, ate a pessoa assistir
// de novo (aqui ou em outro aparelho). Episodio novo sozinho nao o traz.
#define CONF_W     720.0f
#define CONF_PAD    44.0f
#define CONF_BOT_H  60.0f
static float botaoConf(int i, float x, float y, const char *rot, float f, float a) {
  TxtLinha t = txt_linha(TXT_HERO_META, rot, 243, 242, 239, 255);
  GfxRect r = { x, y, t.w + 56.0f, CONF_BOT_H };
  float ar, ag, ab;
  int tinta = ajustes_tinta_foco();
  int c = (int)(243.0f + ((float)tinta - 243.0f) * (f > 0.5f ? 1.0f : 0.0f));
  ajustes_acento(&ar, &ag, &ab);
  if (ajustes_vidro()) gfx_cor(r, .5f, 1, 1, 1, .08f * (1.0f - f) * a);
  else gfx_cor(r, .5f, .125f, .13f, .153f, (1.0f - f) * a);
  if (f > 0.01f) {
    if (ajustes_vidro()) gfx_vidro_pilula_cheia(r, .5f, f, a);
    else { botao_luz(r, .55f * f, a); gfx_cor(r, .5f, ar, ag, ab, f * a); }
  }
  t = txt_linha(TXT_HERO_META, rot, c, c, c, 255);
  txt_desenhar_alpha(t, r.x + 28.0f, r.y + (r.h - t.h) * 0.5f, (f > 0.5f ? 1.0f : .88f) * a);
  if (aberto && a > 0.5f && ponteiro_ativo())
    ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroConfFoco, ponteiroConfOk, i, 0);
  return r.w;
}

static void desenhaConfirmar(const CatItem *ci, float a) {
  char pergunta[240];
  const char *texto = itemASeguir(ci)
    ? i18n("A série sai da fileira. Ela volta quando você assistir um episódio novo.")
    : i18n("O ponto onde você parou é apagado. O título volta para a fileira se você assistir de novo.");
  const char *kicker = "Continuar assistindo", *botao = "Tirar da fileira";
  float tw = CONF_W - 2.0f * CONF_PAD, hTit, hTxt, alt, x, y, bx;
  snprintf(pergunta, sizeof pergunta, i18n("Tirar %s da fileira?"), ci->titulo);
  if (confExtra >= 0 && confExtra < nExtras) {
    const CtxExtra *x0 = &extras[confExtra];
    snprintf(pergunta, sizeof pergunta, i18n(x0->pergunta ? x0->pergunta : "%s"), ci->titulo);
    texto = i18n(x0->texto ? x0->texto : "");
    kicker = x0->kicker ? x0->kicker : "";
    botao = x0->rot;
  }
  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, 0, 0, 0, 0.55f * a); }
  // Medir antes de desenhar (alfa 0): a pergunta pode quebrar com um nome
  // longo, e o texto ocupa mais linhas em alemao que em portugues.
  hTit = txt_bloco_corta(TXT_ILHA_PERGUNTA, pergunta, 243, 242, 239, 0, 0, tw, 44.0f, 0.0f, 2);
  hTxt = txt_bloco_corta(TXT_ILHA_TEXTO, texto, 243, 242, 239, 0, 0, tw, 30.0f, 0.0f, 4);
  alt = CONF_PAD + 18.0f + 8.0f + hTit + 14.0f + hTxt + 34.0f + CONF_BOT_H + CONF_PAD;
  x = (NV_TELA_W - CONF_W) * 0.5f;
  y = (NV_TELA_H - alt) * 0.5f;
  if (aberto && a > 0.5f && ponteiro_ativo()) {
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ponteiroCtxFora, 0, 0);
    ponteiro_alvo(x, y, CONF_W, alt, NULL, NULL, 0, 0);
  }
  ilhaCtx((GfxRect){ x, y, CONF_W, alt }, 36.0f, a);
  y += CONF_PAD;
  kickerCtx(kicker, x + CONF_PAD, y, a);
  y += 18.0f + 8.0f;
  txt_bloco_corta(TXT_ILHA_PERGUNTA, pergunta, 243, 242, 239, x + CONF_PAD, y, tw, 44.0f, a, 2);
  y += hTit + 14.0f;
  txt_bloco_corta(TXT_ILHA_TEXTO, texto, 243, 242, 239, x + CONF_PAD, y, tw, 30.0f, .62f * a, 4);
  y += hTxt + 34.0f;
  bx = x + CONF_PAD;
  bx += botaoConf(0, bx, y, i18n(botao), confAnim[0], a) + 12.0f;
  botaoConf(1, bx, y, i18n("Cancelar"), confAnim[1], a);
}

// --- O MENU DE UMA LISTA ------------------------------------------------------
//
// Segurar OK num cartao de lista (Biblioteca > Listas) abre uma ilha mais larga
// que a do cartaz, porque ela carrega um RESUMO: a arte da pasta quando a fonte
// tem (so a conta Nuvio; ver col_capa — Trakt e Simkl nao dao imagem de lista),
// o nome, o que ha nela (a mistura "12 filmes · 4 séries" quando os itens
// baixados trazem os dois tipos; o Trakt devolve UM tipo por pedido, entao uma
// lista dele diz so o tipo que esta aberto), quem fez e quantas curtidas, e as
// capas dos primeiros itens. Tudo o que a fonte nao informa fica de fora: nada
// de "0 títulos" onde o dado nao existe.
#define CTXL_W       560.0f
#define CTXL_ARTE_H  150.0f
#define CTXL_CAPAS     6
#define CTXL_CAPA_GAP 10.0f

// A arte da lista, quando a fonte tem uma: a capa da pasta de colecao da conta.
static const char *arteDaLista(const LstLista *l) {
  int i;
  if (l->fonte != LST_NUVIO || !l->colId[0]) return NULL;
  for (i = 0; i < col_n(); i++) {
    const ColFolder *f = col_folder(i);
    if (f && !strcmp(f->id, l->colId)) {
      const char *c = col_capa(f);
      return c && c[0] ? c : NULL;
    }
  }
  return NULL;
}

// "12 filmes · 4 séries", a partir dos itens ja baixados. Vazio ate chegarem.
static void misturaDaLista(const LstLista *l, char *dst, size_t n) {
  int total = lst_itens_n(), nf = 0, ns = 0, i;
  CatItem it;
  dst[0] = 0;
  for (i = 0; i < total && i < LST_ITENS_MAX; i++) {
    if (!lst_item(i, &it)) continue;
    if (!strcmp(it.tipo, "series") || it.nTemporadas > 0) ns++; else nf++;
  }
  if (nf + ns == 0) return;
  // So a primeira pagina chegou: a mistura dela nao e a da lista, e dizer
  // "8 filmes · 2 séries" de uma lista de 138 seria inventar o resto. Fica a
  // contagem que a fonte informa (quem chama).
  if (l->itens > nf + ns) return;
  if (nf && ns) {
    snprintf(dst, n, "%d %s \xc2\xb7 %d %s", nf, i18n(nf == 1 ? "filme" : "filmes"),
             ns, i18n(ns == 1 ? "série" : "séries"));
    return;
  }
  { int q = nf ? nf : ns;
    snprintf(dst, n, "%d %s", q, nf ? i18n(q == 1 ? "filme" : "filmes")
                                    : i18n(q == 1 ? "série" : "séries")); }
}

static void desenhaFil(float a) {
  const float pad = CTX_ILHA_PAD;
  const float larg = 560.0f;
  char kick[96];
  TxtLinha n;
  float x, y, alt, hx, hy;
  int i;
  snprintf(kick, sizeof kick, "%s", i18n(filPastaId[0] ? "Coleção" : "Fileira"));
  n = txt_linha_corta(TXT_ILHA_NOME, filTitulo, 243, 242, 239, 255, larg - 2.0f * (pad + 20.0f));
  alt = pad * 2.0f + 12.0f + 22.0f + n.h + 18.0f + (float)nOps * (CTX_LINHA + CTX_GAP);
  x = (NV_TELA_W - larg) * 0.5f;
  y = (NV_TELA_H - alt) * 0.5f;
  if (y < CTX_BORDA) y = CTX_BORDA;
  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, 0, 0, 0, 0.55f * a); }
  if (aberto && a > 0.5f && ponteiro_ativo()) {
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ponteiroCtxFora, 0, 0);
    ponteiro_alvo(x, y, larg, alt, NULL, NULL, 0, 0);
  }
  y += (1.0f - a) * 16.0f;
  ilhaCtx((GfxRect){ x, y, larg, alt }, CTX_ILHA_RAIO, a);
  hx = x + pad + 20.0f;
  hy = y + pad + 12.0f;
  kickerCtx(kick, hx, hy, a);
  hy += 22.0f;
  txt_desenhar_alpha(n, hx, hy, a);
  hy += n.h + 18.0f;
  for (i = 0; i < nOps; i++) {
    float by = hy + CTX_GAP + (float)i * (CTX_LINHA + CTX_GAP) - CTX_GAP;
    GfxRect r = { x + pad, by, larg - pad * 2.0f, CTX_LINHA };
    int ac = ops[i].acao;
    const char *icone = ac == OP_F_FIXAR ? "aj_folders" : ac == OP_ESTILO ? "aj_rows-3"
                      : ac == OP_F_OCULTAR ? "aj_eye-off" : ac == OP_F_VISTOS ? "aj_eye" : "aj_rows-3";
    if (aberto && a > 0.5f)
      ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroCtxOpcao, NULL, i, 0);
    linhaCtx(r, ops[i].rot, icone, focoAnim[i], a);
  }
}

static void desenhaLista(float a) {
  const LstLista *l = &lista;
  const char *arte = arteDaLista(l);
  char mix[96], quem[160], kick[96];
  TxtLinha n, m, q;
  float x, y, alt, hx, hy, hw, cy;
  int i, temQuem, carregando = lst_itens_carregando() > 0;
  const float pad = CTX_ILHA_PAD, esc = gfx_escala_ui();
  const float capaW = (CTXL_W - 2.0f * (pad + 20.0f) - (CTXL_CAPAS - 1) * CTXL_CAPA_GAP) / CTXL_CAPAS;
  const float capaH = capaW * 1.5f;

  misturaDaLista(l, mix, sizeof mix);
  if (!mix[0]) {
    if (l->itens >= 0) snprintf(mix, sizeof mix, "%d %s", l->itens, i18n(l->itens == 1 ? "título" : "títulos"));
    else snprintf(mix, sizeof mix, "%s", carregando ? i18n("Carregando…") : "");
  }
  quem[0] = 0;
  if (l->autor[0]) snprintf(quem, sizeof quem, "%s %s", i18n("por"), l->autor);
  if (l->curtidas > 0) {
    char c[48];
    snprintf(c, sizeof c, "%d %s", l->curtidas, i18n(l->curtidas == 1 ? "curtida" : "curtidas"));
    if (quem[0]) { size_t k = strlen(quem); snprintf(quem + k, sizeof quem - k, " \xc2\xb7 %s", c); }
    else snprintf(quem, sizeof quem, "%s", c);
  }
  temQuem = quem[0] != 0;
  snprintf(kick, sizeof kick, "%s \xc2\xb7 %s", i18n("Lista"),
           l->fonte == LST_TRAKT ? "Trakt" : l->fonte == LST_SIMKL ? "Simkl" : "Nuvio");

  hw = CTXL_W - 2.0f * (pad + 20.0f);
  n = txt_linha_corta(TXT_ILHA_NOME, l->titulo, 243, 242, 239, 255, hw);
  m = txt_linha_corta(TXT_ILHA_APOIO, mix, 243, 242, 239, 255, hw);
  q = txt_linha_corta(TXT_ILHA_APOIO, quem, 243, 242, 239, 255, hw);

  alt = pad * 2.0f + (arte ? CTXL_ARTE_H + 12.0f : 0.0f)
      + 12.0f + 22.0f + n.h + 6.0f + m.h + (temQuem ? 4.0f + q.h : 0.0f)
      + 18.0f + capaH + 14.0f + (float)nOps * (CTX_LINHA + CTX_GAP);
  x = (NV_TELA_W - CTXL_W) * 0.5f;
  y = (NV_TELA_H - alt) * 0.5f;
  if (y < CTX_BORDA) y = CTX_BORDA;

  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, 0, 0, 0, 0.55f * a); }
  if (aberto && a > 0.5f && ponteiro_ativo()) {
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ponteiroCtxFora, 0, 0);
    ponteiro_alvo(x, y, CTXL_W, alt, NULL, NULL, 0, 0);
  }
  y += (1.0f - a) * 16.0f;
  ilhaCtx((GfxRect){ x, y, CTXL_W, alt }, CTX_ILHA_RAIO, a);

  cy = y + pad;
  if (arte) {
    GfxRect r = { x + pad, cy, CTXL_W - 2.0f * pad, CTXL_ARTE_H };
    GLuint t = tex_obter_larg(arte, r.w * esc);
    float raio = 22.0f / r.h;
    if (t) {
      gfx_tex_aspect_atual = tex_aspecto(arte);
      gfx_rect(r, t, GFX_CARD, 0, 0, 0, raio, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
    } else gfx_cor(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
    cy += CTXL_ARTE_H + 12.0f;
  }
  hx = x + pad + 20.0f;
  hy = cy + 12.0f;
  kickerCtx(kick, hx, hy, a);
  hy += 22.0f;
  txt_desenhar_alpha(n, hx, hy, a);
  hy += n.h + 6.0f;
  txt_desenhar_alpha(m, hx, hy, .72f * a);
  hy += m.h + 4.0f;
  if (temQuem) { txt_desenhar_alpha(q, hx, hy, .5f * a); hy += q.h; }
  hy += 18.0f;

  // AS CAPAS DOS PRIMEIROS ITENS. Sem item ainda, o esqueleto marca o lugar.
  for (i = 0; i < CTXL_CAPAS; i++) {
    GfxRect r = { hx + i * (capaW + CTXL_CAPA_GAP), hy, capaW, capaH };
    float raio = 10.0f / capaH;
    CatItem it;
    const char *u = NULL;
    GLuint t = 0;
    if (lst_item(i, &it)) u = it.poster[0] ? it.poster : it.backdrop[0] ? it.backdrop : NULL;
    if (u) t = tex_obter_larg(u, capaW * esc);
    if (t) {
      gfx_tex_aspect_atual = tex_aspecto(u);
      gfx_rect(r, t, GFX_CARD, 0, 0, 0, raio, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
    } else if (u || carregando || i == 0 || lst_itens_n() > i)
      gfx_cor(r, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
  }
  hy += capaH + 14.0f;

  for (i = 0; i < nOps; i++) {
    float by = hy + CTX_GAP + (float)i * (CTX_LINHA + CTX_GAP) - CTX_GAP;
    GfxRect r = { x + pad, by, CTXL_W - pad * 2.0f, CTX_LINHA };
    const char *icone = ops[i].acao == OP_L_ABRIR ? "aj_library"
                      : ops[i].acao == OP_L_FIXAR ? "aj_folders" : "aj_rows-3";
    if (aberto && a > 0.5f)
      ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroCtxOpcao, NULL, i, 0);
    linhaCtx(r, ops[i].rot, icone, focoAnim[i], a);
  }
}

// ---- A LINHA EXPANDIDA DO PAINEL DE SALVOS (ver ctxmenu.h) ----
#define IL_PAD    18.0f
#define IL_PILULA 50.0f
#ifdef NV_TOUCH_UI
static GfxRect inlineRecorte;
static int inlineRecorteValido;
void ctx_inline_recorte(GfxRect r) { inlineRecorte = r; inlineRecorteValido = r.w > 0 && r.h > 0; }
int ctx_inline_painel_ativo(void) { return aberto && modoInline() && pagina == 0; }
#endif
void ctx_inline_pedir(int on) { inlinePend = on; }
static int modoInline(void) { return inlineOn && doPainel && !doSocial && !doLista && !soFileira; }
float ctx_inline_t(void) { return modoInline() ? (anim < 0.0f ? 0.0f : anim > 1.0f ? 1.0f : anim) : 0.0f; }
static CtxInfoEstado estInline(const CatItem *ci) {
  CtxInfoEstado e; e.salvo = tituloSalvo(ci); e.visto = historicoDe(ci); return e;
}
// Rotulo CURTO da pilula (o menu flutuante usa o longo): cabe sem reticencias.
static const char *pilulaRot(int i, const CatItem *ci) {
  switch (ops[i].acao) {
    case OP_LISTA:     return tituloSalvo(ci) ? "Remover" : "Salvar";
    case OP_ASSISTIDO: return historicoDe(ci) == 1 ? "Não assistido" : "Assistido";
    case OP_CATEGORIA: return "Categoria";
    default:           return ops[i].rot;
  }
}
#ifdef NV_TOUCH_UI
static void inlineAcoes(const CatItem *ci, float w, float faixaH, float hi, GfxRect *r) {
  const float gap = 10, cw = w - 2 * (IL_PAD + 6);
  float nat[CTX_MAX], total = 0, k = 1, x = IL_PAD + 6;
  for (int i = 0; i < nOps; i++) {
    nat[i] = 20 + 10 + (float)txt_linha(TXT_CAPTION2, pilulaRot(i, ci), 255, 255, 255, 255).w + 22;
    total += nat[i];
  }
  total += gap * (nOps > 1 ? nOps - 1 : 0);
  if (total > cw) k = (cw - gap * (nOps > 1 ? nOps - 1 : 0)) / (total - gap * (nOps > 1 ? nOps - 1 : 0));
  for (int i = 0; i < nOps; i++) {
    r[i] = (GfxRect){x, IL_PAD + faixaH + 14 + hi + 14, nat[i] * k, IL_PILULA};
    x += r[i].w + gap;
  }
}
int ctx_inline_foco_rect(float w, float faixaH, GfxRect *r) {
  const CatItem *ci = itemAtual();
  if (!r || !ci || !ctx_inline_painel_ativo() || foco < 0 || foco >= nOps) return 0;
  CtxInfoEstado e = estInline(ci); GfxRect acoes[CTX_MAX];
  float hi = ctxinfo_compacto(ci, &e, 0, 0, w - 2 * (IL_PAD + 6), 0, 0);
  inlineAcoes(ci, w, faixaH, hi, acoes); *r = acoes[foco];
  return foco + 1;
}
#endif
float ctx_inline_altura(float w, float faixaH) {
  const CatItem *ci = itemAtual();
  CtxInfoEstado e;
  if (!ci || !modoInline()) return 0.0f;
  e = estInline(ci);
  return IL_PAD + faixaH + 14.0f + ctxinfo_compacto(ci, &e, 0, 0, w - 2.0f * (IL_PAD + 6.0f), 0, 0) +
         14.0f + IL_PILULA + IL_PAD;
}
void ctx_inline_desenhar(float x, float y, float w, float faixaH, float a) {
#ifdef NV_TOUCH_UI
  GfxRect recorteAtual = inlineRecorte;
  int temRecorte = inlineRecorteValido;
  inlineRecorteValido = 0;
#endif
  const CatItem *ci = itemAtual();
  CtxInfoEstado e;
  float cx = x + IL_PAD + 6.0f, cw = w - 2.0f * (IL_PAD + 6.0f), py, hi, gap = 10.0f;
  int i;
  float ar, ag, ab;
  if (!ci || !modoInline() || a < 0.01f) return;
  e = estInline(ci);
  logotitulo_desenhar(ci, ci->titulo, TXT_ILHA_NOME, cx + 6.0f, y + IL_PAD + faixaH - 16.0f - 52.0f,
                      300.0f, 52.0f, cw - 12.0f, a);
  hi = ctxinfo_compacto(ci, &e, cx, y + IL_PAD + faixaH + 14.0f, cw, a, 1);
  py = y + IL_PAD + faixaH + 14.0f + hi + 14.0f;
  ajustes_acento(&ar, &ag, &ab);
#ifdef NV_TOUCH_UI
  GfxRect phoneAcoes[CTX_MAX];
  if (telefoneui_ativo()) inlineAcoes(ci, w, faixaH, hi, phoneAcoes);
#endif
  { float nat[CTX_MAX], tot = 0.0f, k = 1.0f, px = cx;
    for (i = 0; i < nOps; i++) {
      nat[i] = 20.0f + 10.0f + (float)txt_linha(TXT_CAPTION2, pilulaRot(i, ci), 255, 255, 255, 255).w + 22.0f;
      tot += nat[i];
    }
    tot += gap * (float)(nOps > 1 ? nOps - 1 : 0);
    if (tot > cw) k = (cw - gap * (float)(nOps > 1 ? nOps - 1 : 0)) / (tot - gap * (float)(nOps > 1 ? nOps - 1 : 0));
    for (i = 0; i < nOps; i++) {
      GfxRect r = { px, py, nat[i] * k, IL_PILULA };
#ifdef NV_TOUCH_UI
      if (telefoneui_ativo()) { r = phoneAcoes[i]; r.x += x; r.y += y; }
#endif
      float f = focoAnim[i];
      const char *icone = "aj_info";
      TxtLinha t;
      float tw;
      px += r.w + gap;
      switch (ops[i].acao) {
        case OP_LISTA: icone = "aj_library"; break;
        case OP_ASSISTIDO: icone = historicoDe(ci) == 1 ? "aj_eye-off" : "aj_eye"; break;
        case OP_CATEGORIA: icone = "aj_folders"; break;
        default: break;
      }
      gfx_cor(r, 0.5f, 1, 1, 1, (.07f + .05f * f) * a);
      if (f > 0.01f) gfx_cor(r, 0.5f, ar, ag, ab, .30f * f * a);
      if (aberto && a > 0.5f) {
#ifdef NV_TOUCH_UI
        if (telefoneui_ativo()) {
          if (temRecorte && ctx_inline_painel_ativo()) {
            float ax = fmaxf(r.x, recorteAtual.x), ay = fmaxf(r.y, recorteAtual.y);
            float ar = fminf(r.x + r.w, recorteAtual.x + recorteAtual.w);
            float ab = fminf(r.y + r.h, recorteAtual.y + recorteAtual.h);
            if (ar > ax && ab > ay) ponteiro_alvo(ax, ay, ar - ax, ab - ay, ponteiroCtxOpcao, NULL, i, 0);
          }
        } else
#endif
        ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroCtxOpcao, NULL, i, 0);
      }
      t = txt_linha_corta(TXT_CAPTION2, pilulaRot(i, ci), 243, 242, 239, 255, r.w - 22.0f - 30.0f);
      tw = 30.0f + (float)t.w;
      gfx_icone((GfxRect){ r.x + (r.w - tw) * 0.5f, r.y + (r.h - 20.0f) * 0.5f, 20.0f, 20.0f },
                icone, .953f, .949f, .937f, (.7f + .3f * f) * a);
      txt_desenhar_alpha(t, r.x + (r.w - tw) * 0.5f + 30.0f, r.y + (r.h - (float)t.h) * 0.5f,
                         (.78f + .22f * f) * a);
    }
  }
}

static void ctx_desenharCorpo_(Uint32 agora);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void ctx_desenhar(Uint32 agora) {
  ESCALA_INI();
  ctx_desenharCorpo_(agora);
  ESCALA_FIM();
}
// O cartao da home e em pixels reais; o menu desenha na tela virtual.
static int cartaoDaHome(void) {
  int ok;
  ESCALA_REAL_INI();
  ok = home_cartao_foco_por_cima(idx);
  ESCALA_REAL_FIM();
  return ok;
}
static void ctx_desenharCorpo_(Uint32 agora) {
  const CatItem *ci;
  const char *mensagem = NULL;
  float a = anim, alt, x, y, cab, grupoH;
  int i, comLogo, infoOn, morph = 0, grande = 0;
  CtxInfoGeo geo;
  CtxCartaoGeo cg;
  CtxInfoEstado est;
  (void)agora;
  infoDesenhada = 0;
  // So no painel de Salvos (dicaCx): na home o cartao pressionado ja tem a
  // propria barra (home.c), e esta, no meio da tela, era a sobra duplicada.
  if (!aberto && holdAtivo && dicaCx >= 0.0f) {
    float p = (float)(SDL_GetTicks() - holdDesde) / (float)NV_HOLD_MS;
    TxtLinha t;
    // O centro e o da tela, ou o do painel de Salvos quando e nele que o dedo
    // esta (ctx_centro_dica): centrada na tela a barra ficava metade no veu,
    // metade sob o painel.
    float cx = dicaCx >= 0.0f ? dicaCx : NV_TELA_W * 0.5f;
    if (p > 1.0f) p = 1.0f;
    t = txt_linha(TXT_CAPTION2,
                  p >= 1.0f ? "Solte para abrir opções" : "Segure OK para opções",
                  220, 224, 232, 255);
    // No painel a dica cai EM CIMA da ultima linha da lista (ela vai ate a
    // borda de baixo), e o texto dos dois se misturava: "Salvo agoraSegure
    // OK". Uma placa na cor do cartao por baixo separa as duas leituras.
    if (dicaCx >= 0.0f)
      gfx_cor((GfxRect){ cx - 250.0f, NV_TELA_H - 140.0f, 500.0f, 84.0f },
              0.3f, 0.055f, 0.058f, 0.068f, 0.94f);
    txt_desenhar_alpha(t, cx - t.w * 0.5f, NV_TELA_H - 124.0f, 0.94f);
    gfx_cor((GfxRect){ cx - 210.0f, NV_TELA_H - 82.0f,
                       420.0f, 8.0f }, 4.0f, 0.18f, 0.2f, 0.23f, 0.96f);
    gfx_cor((GfxRect){ cx - 210.0f, NV_TELA_H - 82.0f,
                       420.0f * p, 8.0f }, 4.0f, 0.78f, 0.84f, 0.96f, 0.98f);
  }
  if (a < 0.01f) return;
  if (modoInline() && pagina == 0) return;   // a linha do painel se abre sozinha (salvospainel.c)
  if (pagina == 1) { desenhaEstilos(a); return; }
  if (doLista) { if (doFil) desenhaFil(a); else desenhaLista(a); return; }
  ci = itemAtual();
  if (!ci) return;
  if (pagina == 2) { desenhaConfirmar(ci, a); return; }
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
  if (estadoOperacao == CTX_CONFIRMADA && operacao == CTX_OP_LISTA && avisoOp)
    mensagem = avisoOp;

  // O poster acompanha o cartao na home (a mola de foco ainda pode estar
  // andando quando o menu abre): o retangulo e o deste quadro.
  if (temCartaz && !doPainel && !cartazFixo) {
    HomeItem hi;
    if (home_item_focado(&hi) && hi.indice == idx && hi.rect.w > 8.0f && hi.rect.h > 8.0f)
      cartazRect = ctxDaHome(hi.rect);
  }

  { GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    gfx_cor(tela, 0.0f, 0, 0, 0, 0.94f * a); }

  infoOn = infoPossivel(ci);
  est.salvo = tituloSalvo(ci);
  est.visto = historicoDe(ci);
  comLogo = !infoOn && logotitulo_url(ci, CTX_LOGO_W) != NULL;
  cab = infoOn ? CTX_CAB_CURTO : comLogo ? CTX_CAB_LOGO : CTX_CAB;
  alt = CTX_ILHA_PAD * 2.0f + cab + (float)nOps * (CTX_LINHA + CTX_GAP);
  // AO LADO DO CARTAZ, alinhada ao topo: tenta a direita, depois a esquerda.
  // Se ambos os lados forem estreitos, ctxXCartaz escolhe o de maior espaco e
  // prende a ilha na viewport para manter todas as opcoes acessiveis.
  // Sem cartaz: no meio da tela, ou do painel de Salvos (dicaCx).
  x = -1.0f;
  y = (NV_TELA_H - alt) * 0.5f;
  if (temCartaz && !doPainel) {
    x = ctxXCartaz(cartazRect);
    y = cartazRect.y;
  }
  if (x < 0.0f) {
    float cx = doPainel && dicaCx >= 0.0f ? dicaCx : NV_TELA_W * 0.5f;
    x = cx - CTX_W * 0.5f;
  }
  // A EXTENSAO muda o lugar do menu: os dois juntos ao lado do cartaz (ou no
  // meio), e o grupo inteiro dentro da tela. Ver ctxinfo_geometria.
  memset(&geo, 0, sizeof geo);
  memset(&cg, 0, sizeof cg);
  grupoH = alt;
  // O CARTAZ VIRA O CARTAO: com cartaz focado o cartao de informacoes e o
  // proprio cartaz crescido (ctxinfo_cartao_geo) e o menu fica colado a ele.
  morph = infoOn && temCartaz && !doPainel;
  if (morph) {
    float hAlvo, wCart;
    grande = cartazRect.w > cartazRect.h * 1.2f && cartazRect.w < cartazRect.h * 1.5f;   // so o 4:3 (Destaque)
    // Cartao grande LEVANTA ao abrir o menu: ~+22% de largura e de altura.
    wCart = grande ? (cartazRect.w * 1.22f < 480.0f ? 480.0f : cartazRect.w * 1.22f)
                   : (CTXI_CARTAO_W < cartazRect.w ? cartazRect.w : CTXI_CARTAO_W);
    hAlvo = grande ? ctxinfo_sobre_altura(ci, &est, wCart) : ctxinfo_altura(ci, &est, wCart);
    if (grande && hAlvo < cartazRect.h * 1.22f) hAlvo = cartazRect.h * 1.22f;
    if (hAlvo < cartazRect.h) hAlvo = cartazRect.h;   // do tamanho do conteudo
    if (infoH < 1.0f || ajustes_animacoes_reduzidas()) infoH = hAlvo;
    else infoH += (hAlvo - infoH) * 0.22f;
    ctxinfo_cartao_geo_w(&cartazRect, wCart, infoH, CTX_W, &cg);
    x = cg.menuX; y = cg.cartao.y; grupoH = cg.cartao.h;
  } else
  if (infoOn) {
    float pedido = doPainel && dicaCx >= 0.0f ? dicaCx : -1.0f;
    ctxinfo_geometria(temCartaz && !doPainel ? &cartazRect : NULL, x, pedido, CTX_W, &geo);
    x = geo.menuX;
    if (geo.infoW > 0.0f) {
      float hAlvo = ctxinfo_altura(ci, &est, geo.infoW);
      if (hAlvo < alt) hAlvo = alt;          // nunca mais baixa que o menu
      // A altura anda com o que chega (notas, sinopse), sem pular.
      if (infoH < 1.0f || ajustes_animacoes_reduzidas()) infoH = hAlvo;
      else infoH += (hAlvo - infoH) * 0.22f;
      if (infoH > grupoH) grupoH = infoH;
      if (!(temCartaz && !doPainel)) y = (NV_TELA_H - grupoH) * 0.5f;
    }
  }
  if (y + grupoH > NV_TELA_H - CTX_BORDA) y = NV_TELA_H - CTX_BORDA - grupoH;
  if (y < CTX_BORDA) y = CTX_BORDA;
  if (aberto && a > 0.5f && ponteiro_ativo()) {
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ponteiroCtxFora, 0, 0);
    ponteiro_alvo(x, y, CTX_W, alt, NULL, NULL, 0, 0);
    // A extensao e parte do menu: clicar nela nao fecha nada.
    if (geo.infoW > 0.0f) ponteiro_alvo(geo.infoX, y, geo.infoW, infoH, NULL, NULL, 0, 0);
  }

  // O POSTER POR CIMA DO VEU, na mesma caixa e no mesmo raio do cartao da
  // home (ajustes_raio_poster_px): com `a` ele so desvela o que ja estava la.
  // DA HOME: o proprio cartao, pintado pela mesma funcao da fileira (arte,
  // recorte, veus, rotulo, logo, anel), em pixels reais — nada de refazer a
  // arte aqui com outro aspecto. So o que nao e da home (Biblioteca) cai na
  // arte solta abaixo.
  if (morph) {
    // O CARTAZ CRESCE ATE O CARTAO. `e` anda com a mola de abertura, entao
    // fechar desfaz o mesmo caminho de volta ate o cartaz. A arte do cartaz
    // some na primeira metade e o conteudo so acende no fim (ele e posto na
    // caixa final: antes disso a caixa ainda nao o contem).
    float e = a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a;
    GfxRect P = cartazRect, C = cg.cartao, r;
    float raioP = ajustes_raio_poster_px() / gfx_escala_ui(), raioPx, sa, ca, artA;
    r.x = P.x + (C.x - P.x) * e; r.y = P.y + (C.y - P.y) * e;
    r.w = P.w + (C.w - P.w) * e; r.h = P.h + (C.h - P.h) * e;
    raioPx = raioP + (CTX_ILHA_RAIO - raioP) * e;
    if (raioPx > 0.5f * (r.h < r.w ? r.h : r.w)) raioPx = 0.5f * (r.h < r.w ? r.h : r.w);
    sa = anim_clamp(e * 3.0f, 0.0f, 1.0f);
    ca = anim_clamp((e - 0.8f) / 0.2f, 0.0f, 1.0f);
    artA = grande ? 1.0f : 1.0f - anim_clamp((e - 0.15f) / 0.55f, 0.0f, 1.0f);
    ilhaCtx(r, raioPx, sa);
    if (artA > 0.01f && cartazArte[0]) {
      GLuint t = tex_obter_larg(cartazArte, P.w * gfx_escala_ui());
      if (t) {
        gfx_tex_aspect_atual = tex_aspecto(cartazArte);
        gfx_card_forcar_cover_atual = 1.0f;
        gfx_rect(r, t, GFX_CARD, 0, 0, 0, raioPx / r.h, 0, 0, 0, artA);
        gfx_card_forcar_cover_atual = 0.0f;
        gfx_tex_aspect_atual = 0.0f;
      }
    }
    if (ca > 0.01f) {
      if (grande) ctxinfo_sobre_arte(ci, &est, C, raioPx, ca);
      else ctxinfo_desenhar(ci, &est, C.x, C.y, C.w, sa, ca);
    }
    infoDesenhada = 1; infoLadoUlt = cg.lado;
    infoCaixa = C;
    menuCaixa = (GfxRect){ cg.menuX, y, CTX_W, alt };
    // O menu sai da borda do cartao, acendendo depois dele.
    x -= (float)cg.lado * (1.0f - e) * 40.0f;
    a = anim_clamp((e - 0.35f) / 0.65f, 0.0f, 1.0f);
  } else
  if (temCartaz && !doPainel && !cartazFixo && cartaoDaHome()) {
    /* feito */
  } else
  if (temCartaz && !doPainel && cartazArte[0]) {
    GLuint t = tex_obter_larg(cartazArte, cartazRect.w * gfx_escala_ui());
    if (t) {
      float raio = ajustes_raio_poster_px() / gfx_escala_ui() / cartazRect.h, teto = 0.5f * cartazRect.w / cartazRect.h;
      if (raio > teto) raio = teto;
      gfx_tex_aspect_atual = tex_aspecto(cartazArte);
      gfx_rect(cartazRect, t, GFX_CARD, 0, 0, 0, raio, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
    }
  }

  // A EXTENSAO, POR BAIXO DO MENU: sai da borda dele (ou do cartaz, quando
  // ficou do outro lado) deslizando 40 px e acendendo; o conteudo entra um
  // pouco depois da superficie. Mesmo vidro do menu (ilhaCtx), sem borrao novo.
  if (geo.infoW > 0.0f && infoT > 0.01f) {
    float e = infoT > 1.0f ? 1.0f : infoT;
    float ix = geo.infoX - (float)geo.lado * (1.0f - e) * 40.0f;
    float sa = a * (e * 1.6f > 1.0f ? 1.0f : e * 1.6f);
    float ca = a * anim_clamp((e - 0.3f) / 0.7f, 0.0f, 1.0f);
    ilhaCtx((GfxRect){ ix, y, geo.infoW, infoH }, CTX_ILHA_RAIO, sa);
    ctxinfo_desenhar(ci, &est, ix, y, geo.infoW, sa, ca);
    infoDesenhada = 1; infoLadoUlt = -geo.lado;
    infoCaixa = (GfxRect){ geo.infoX, y, geo.infoW, infoH };
    menuCaixa = (GfxRect){ geo.menuX, y, CTX_W, alt };
  }

  // Entra deslizando 16 px a partir do lado do poster, como as folhas do app.
  if (!morph) x += (1.0f - a) * (temCartaz && !doPainel && x < cartazRect.x ? 16.0f : -16.0f);
  ilhaCtx((GfxRect){ x, y, CTX_W, alt }, CTX_ILHA_RAIO, a);

  // CABECALHO: o nome e, embaixo, o que o titulo e ("Serie · 2024 · ...") —
  // ou, enquanto ha uma escrita, o estado dela. Com a extensao ao lado, uma
  // linha so: o nome (ou o estado), porque o resto esta nela.
  { float hx = x + CTX_ILHA_PAD + 20.0f, hy = y + CTX_ILHA_PAD + 12.0f, hw = CTX_W - 2.0f * (CTX_ILHA_PAD + 20.0f);
    char meta[200];
    const char *tp = !strcmp(ci->tipo, "movie") ? i18n("Filme")
                   : !strcmp(ci->tipo, "series") ? i18n("Série") : "";
    TxtLinha m;
    float my;
    int falha = estadoOperacao == CTX_FALHA;
    if (mensagem) snprintf(meta, sizeof meta, "%s", i18n(mensagem));
    else if (infoOn) snprintf(meta, sizeof meta, "%s", ci->titulo);
    else snprintf(meta, sizeof meta, "%s%s%s", tp, tp[0] && ci->meta[0] ? " \xc2\xb7 " : "", ci->meta);
    m = txt_linha_corta(TXT_ILHA_APOIO, meta, falha ? 240 : 243, falha ? 190 : 242, falha ? 130 : 239, 255, hw);
    if (infoOn) {
      my = hy + (CTX_CAB_CURTO - 22.0f - (float)m.h) * 0.5f;
    } else if (comLogo) {
      // Logo (ou, enquanto ele chega, o nome) na caixa reservada.
      logotitulo_desenhar(ci, ci->titulo, TXT_ILHA_NOME, hx, hy, CTX_LOGO_W, CTX_LOGO_H, hw, a);
      my = hy + CTX_LOGO_H + 6.0f;
    } else {
      TxtLinha n = txt_linha_corta(TXT_ILHA_NOME, ci->titulo, 243, 242, 239, 255, hw);
      txt_desenhar_alpha(n, hx, hy, a);
      my = hy + n.h + 4.0f;
    }
    txt_desenhar_alpha(m, hx, my, (mensagem ? .8f : infoOn ? .62f : .5f) * a); }

  for (i = 0; i < nOps; i++) {
    float by = y + CTX_ILHA_PAD + cab + CTX_GAP + (float)i * (CTX_LINHA + CTX_GAP);
    GfxRect r = { x + CTX_ILHA_PAD, by, CTX_W - CTX_ILHA_PAD * 2.0f, CTX_LINHA };
    float f = focoAnim[i];
    if (aberto && a > 0.5f)
      ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroCtxOpcao, NULL, i, 0);
    // O ICONE DIZ O QUE A OPCAO FAZ antes de a pessoa ler — o traco fino do
    // Lucide (aj_*, tools/icones-lucide.sh), o mesmo do mockup: info =
    // detalhes, a estante = salvar/remover dos Salvos, olho (riscado) =
    // historico, X = tirar da fileira, duas pessoas = recomendar.
    const char *icone = "aj_info";
    switch (ops[i].acao) {
      case OP_LISTA:     icone = "aj_library"; break;
      case OP_ASSISTIDO: icone = historicoDe(ci) == 1
                                 ? "aj_eye-off" : "aj_eye"; break;
      case OP_TIRAR_CONTINUAR: icone = "aj_x"; break;
      case OP_DISPENSAR:  icone = "aj_x"; break;
      case OP_RECOMENDAR: icone = "aj_users"; break;
      case OP_DETALHES:   icone = "aj_info"; break;
      case OP_ESTILO:     icone = "aj_rows-3"; break;
      case OP_F_MENU:     icone = "aj_rows-3"; break;
      case OP_CATEGORIA:  icone = "aj_folders"; break;
      case OP_ADD_LISTA:  icone = "aj_rows-3"; break;
      default:
        if (pagina == 3) { icone = ops[i].acao ? "aj_rows-3" : "aj_info"; break; }
        if (ops[i].acao >= OP_EXTRA && ops[i].acao < OP_EXTRA + nExtras &&
            extras[ops[i].acao - OP_EXTRA].icone)
          icone = extras[ops[i].acao - OP_EXTRA].icone;
        break;
    }
    linhaCtx(r, ops[i].rot, icone, f, a);
  }
}
