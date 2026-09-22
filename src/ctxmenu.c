#include "ctxmenu.h"
#include "catalogo.h"
#include "descoberta.h"
#include "syncprog.h"
#include "visto.h"
#include "trakt.h"
#include "simkl.h"
#include "extras.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "layout.h"
#include "anim.h"
#include "ajustes.h"
#include "progresso.h"
#include "salvos.h"
#include "recomenda.h"
#include "recenviar.h"
#include "botoes.h"
#include "badges.h"
#include "idioma.h"
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
#define CTX_LINHA   BOTAO_H_PRIMARIO // mesma altura do botao primario do detalhe
#define CTX_GAP     BOTAO_GAP         // o mesmo ritmo entre acoes do app
#define CTX_CAB    158.0f     // titulo, estados e rotulo do grupo
#define CTX_RODAPE  70.0f

static int   aberto, idx = -1, foco, pedDetalhes = -1;
static float anim;
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
#define CTX_MAX 5
static struct { const char *rot; int acao; } ops[CTX_MAX];
static int nOps;
static float focoAnim[CTX_MAX];
static int holdObservador;
enum { OP_DETALHES, OP_LISTA, OP_ASSISTIDO, OP_TIRAR_CONTINUAR, OP_RECOMENDAR };

// --- RECOMENDAR: O FLUXO NAO MORA MAIS AQUI ---------------------------------
//
// "Para quem" e "o que dizer" foram DUAS PAGINAS DENTRO DESTE MODAL ate a tela
// de detalhe pedir o mesmo fluxo pelo botao circular. Duas copias das mesmas
// listas divergiriam na primeira mudanca — e a primeira mudanca ja estava
// pedida: o pareamento por codigo e o estado vazio honesto. As telas viraram
// recenviar.c, e este arquivo faz o que a tela de detalhe tambem faz: abre a
// modal compartilhada e sai da frente.

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

// O UNICO CAMINHO PARA DENTRO DE ops[]. Ver a nota em CTX_MAX.
static void juntar(const char *rot, int acao) {
  if (nOps >= CTX_MAX) return;
  ops[nOps].rot = rot; ops[nOps].acao = acao; nOps++;
}

static void montar(void) {
  int i = indiceAtual();
  const CatItem *ci = i >= 0 ? cat_item(i) : NULL;
  nOps = 0;
  if (!ci) return;
  juntar("Ver detalhes", OP_DETALHES);
  // Sem IMDb nao ha endpoint remoto suportado para esta acao. Nao oferecer
  // um botao que so aparentaria funcionar e inventaria estado local.
  if (ci->imdb[0]) {
    // O MESMO VERBO DO PAINEL E DO BOTAO "+". Estava "Adicionar à biblioteca",
    // e "Biblioteca" e o nome de uma TELA — a pessoa lia o rotulo, ia ate a
    // tela Biblioteca e nao encontrava relacao com o "+" que tinha apertado no
    // detalhe. Agora as tres portas da mesma acao (o "+", esta linha e o painel
    // da tecla AZUL) usam a palavra "salvar", e todas escrevem no mesmo lugar.
    juntar(estadoOperacao == CTX_PENDENTE && operacao == CTX_OP_LISTA
             ? (intencao ? "Salvando..." : "Removendo dos Salvos...")
             : (ci->naLista ? "Remover dos Salvos" : "Salvar"),
           OP_LISTA);
  }
  // O web so oferece "assistido" em filme e serie — nao em canal nem evento,
  // que sao tipos que os addons do dono tambem declaram.
  if (ci->imdb[0] && (!strcmp(ci->tipo, "movie") || !strcmp(ci->tipo, "series"))) {
    juntar(estadoOperacao == CTX_PENDENTE && operacao == CTX_OP_HISTORICO
             ? (intencao ? "Marcando como assistido..."
                         : "Desmarcando como assistido...")
             : (cat_historico_estado_item(i) == 1 ? "Desmarcar como assistido"
                                                  : "Marcar como assistido"),
           OP_ASSISTIDO);
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
    juntar("Tirar de Continuar assistindo", OP_TIRAR_CONTINUAR);
  }
  // SO EXISTE SE O PACOTE TEM O SERVICO. Sem NUVIO_REC_URL compilada,
  // recomenda_ativo() e 0 e esta linha nunca aparece — o dono publica builds
  // assim, e um item de menu que so da erro e pior que item nenhum.
  if (recomenda_ativo() && ci->imdb[0] &&
      (!strcmp(ci->tipo, "movie") || !strcmp(ci->tipo, "series"))) {
    juntar("Recomendar a um amigo", OP_RECOMENDAR);
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

// O ESPELHO LOCAL DE "ASSISTIDO", separado de quem confirma: com Trakt ele
// roda depois do 2xx (ctx_atualizar); sem Trakt roda na hora (aplicar), porque
// nao ha resposta nenhuma a esperar.
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
    trakt_playback_remover(ci->imdb);
    simkl_playback_remover(ci->imdb);
    syncprog_remover(chave);
    cat_zerar_progresso(atual);
  }
}

static void aplicar(void) {
  int atual = indiceAtual();
  const CatItem *ci = atual >= 0 ? cat_item(atual) : NULL;
  int acao;
  if (!ci || foco < 0 || foco >= nOps) return;
  acao = ops[foco].acao;
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
  if (acao != OP_DETALHES && estadoOperacao == CTX_PENDENTE) return;
  switch (acao) {
    case OP_DETALHES: pedDetalhes = idx; break;
    case OP_LISTA:
      // Captura a intencao ANTES de qualquer escrita. O mesmo valor segue para
      // o POST e so chega ao espelho local depois de uma resposta 2xx.
      intencao = !ci->naLista;
      snprintf(operacaoImdb, sizeof operacaoImdb, "%s", ci->imdb);
      operacao = CTX_OP_LISTA;
      opSimkl = 0;
      avisoOp = NULL;
      espelhoAplicado = 0;
      estadoOperacao = CTX_PENDENTE;
      // LOCAL PRIMEIRO E SEMPRE. E sincrono e nao pode falhar por rede, entao
      // acontece fora do jogo de estados abaixo; ver salvos.h para por que ele
      // e o unico destino que sobrevive ao fechamento do app.
      salvos_definir(ci, intencao);
      if (ajustes_salvos_no_trakt()) {
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
        desc_remontar_fileiras();
      }
      montar();
      break;
    case OP_ASSISTIDO:
      // Progresso e posicao de retomada, nao historico. So um retrato de
      // historico confirmado pode inverter a acao para "desmarcar".
      intencao = cat_historico_estado_item(atual) == 1 ? 0 : 1;
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
      }
      montar();
      break;
    case OP_RECOMENDAR:
      // FECHA ESTE MODAL E ABRE O COMPARTILHADO. O menu do cartaz falou de um
      // INDICE da home; dali em diante quem manda e uma copia do CatItem, pela
      // mesma razao que salvospainel.c copia: o vetor do catalogo troca de
      // bloco a cada republicacao da descoberta.
      if (recenviar_abrir(ci)) aberto = 0;
      break;
    case OP_TIRAR_CONTINUAR: {
      // A chave e montada do mesmo jeito que progresso.c monta ao gravar —
      // com temporada e episodio quando ha —, senao a linha apagada seria
      // outra e o card continuaria na fileira.
      char chave[192];
      prog_chave(chave, sizeof chave, ci->imdb, ci->temporada, ci->episodio);
      prog_remover(chave);
      // AS TRES FONTES, e nao so a local — issue #22.
      //
      // A fileira de retomada e a fusao de tres coisas: o registro local, o
      // /sync/playback do Trakt e o progresso da conta Nuvio. Apagar so a
      // local fazia a entrada voltar no ciclo seguinte, vinda de qualquer uma
      // das outras duas: "seleciono remover, o prompt some e nada e removido...
      // nao consigo remover".
      //
      // Nenhuma das duas remotas e obrigatoria: quem nao tem Trakt nao tem id
      // de playback, quem nao tem conta nao tem RPC. As duas dizem no log o que
      // fizeram, e a local acontece de qualquer jeito.
      trakt_playback_remover(ci->imdb);
      // O Simkl tambem guarda o pausado (issue #110). Em fio proprio; sem id
      // conhecido (item que nao veio do Simkl) nao faz nada.
      simkl_playback_remover(ci->imdb);
      syncprog_remover(chave);
      // Efeito local e imediato: sem zerar o campo, o card so sairia da fileira
      // na proxima remontagem do catalogo, e para quem apertou parece que nada
      // aconteceu.
      cat_zerar_progresso(atual);
      // E TIRA O CARD DA FILEIRA, que zerar o progresso nao faz: sem isto ele
      // fica ali sem barra de progresso ate a proxima remontagem do catalogo, e
      // o relator do #22 via a remocao so depois de fechar e reabrir o app.
      cat_tirar_item_da_fileira(atual);
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
  //
  // DEPOIS que ela termina, o OK volta a ser OK. Antes ele virava "fechar" —
  // o modal ficava com os botoes na tela, respondendo ao foco, e o unico
  // efeito de aperta-los era sumir. Somado a guarda de aplicar() logo acima,
  // era a metade visivel do "nao faz nada" no botao de desmarcar.
  if (operacao != CTX_OP_NENHUMA && estadoOperacao == CTX_PENDENTE) return;
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
    int novo = opSimkl ? simkl_lista_estado() : trakt_operacao_estado(operacao);
    if (novo == CTX_CONFIRMADA || novo == CTX_FALHA) {
      estadoOperacao = novo;
      if (!espelhoAplicado && atual >= 0) {
        const CatItem *ci = cat_item(atual);
        if (ci && novo == CTX_CONFIRMADA) {
          if (operacao == CTX_OP_LISTA) {
            cat_definir_na_lista(atual, intencao);
          } else {
            espelharAssistido(atual, ci, intencao);
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
  if (estadoOperacao == CTX_CONFIRMADA && operacao == CTX_OP_LISTA && avisoOp)
    mensagem = avisoOp;

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

  // CARTAO FLUTUANTE (a "cara nova" da barra lateral, dono, 21/09/2026):
  // cantos de 28 px de verdade (raio normalizado pelo menor lado, senao o
  // canto muda com o numero de opcoes), fundo translucido e UMA luz difusa na
  // cor de realce entrando pelo canto superior esquerdo, presa aos cantos do
  // cartao (GFX_LUZ). Com o veu de tela cheia ja pago, e a ultima camada
  // grande daqui — e mede o cartao, nao a tela.
  float ar_, ag_, ab_; ajustes_acento(&ar_, &ag_, &ab_);
  { GfxRect p = { x, y, CTX_W, alt };
    float menor = alt < CTX_W ? alt : CTX_W, raio = 28.0f / menor;
    gfx_cor(p, raio, 0.055f, 0.058f, 0.068f, 0.94f * a);
    gfx_luz_canto(p, raio, CTX_W * 0.1f, -CTX_W * 0.1f, CTX_W * 0.65f, ar_, ag_, ab_, 0.22f * a); }

  { TxtLinha t = txt_linha(TXT_CAPTION2, "TÍTULO SELECIONADO", 174, 178, 188, 255);
    txt_desenhar_alpha(t, x + CTX_PAD, y + CTX_PAD, a * 0.95f); }
  { TxtLinha t = txt_linha_corta(TXT_HEADLINE, ci->titulo, 245, 248, 255, 255,
                                 CTX_W - CTX_PAD * 2.0f);
    txt_desenhar_alpha(t, x + CTX_PAD, y + CTX_PAD + 28.0f, a); }
  { const char *subtitulo = mensagem ? mensagem : "Opções do título";
    TxtLinha t = txt_linha(TXT_DET_META2, subtitulo, 150, 154, 163, 255);
    txt_desenhar_alpha(t, x + CTX_PAD, y + CTX_PAD + 70.0f, a * 0.9f); }

  // OS SELOS DE ESTADO SAO DA TABELA (badges.h) e TEM HIERARQUIA: o estado
  // POSITIVO ("Na biblioteca", "Assistido") acende em realce a 18 %; o
  // negativo ou desconhecido ("Fora da biblioteca", "Historico nao
  // consultado") e cinza com texto apagado. Antes eram duas pilulas cinza
  // iguais e a pessoa tinha de LER para saber se o titulo ja era dela.
  { float sx = x + CTX_PAD;
    float sy = y + CTX_PAD + 106.0f;
    int historico = cat_historico_estado_item(indiceAtual());
    for (i = 0; i < nEstados; i++) {
      int positivo = i == 0 ? ci->naLista : historico == 1;
      // "Progresso salvo" e o unico estado nem positivo nem negativo: neutro.
      int fraco = i == 0 ? !ci->naLista : !(historico < 0 && ci->progresso > 0);
      sx += badge_desenhar(sx, sy, estados[i],
                           positivo ? BADGE_REALCE : fraco ? BADGE_APAGADO : BADGE_NEUTRO,
                           a) + BADGE_GAP;
    } }

  for (i = 0; i < nOps; i++) {
    float by = y + CTX_PAD + CTX_CAB + (float)i * (CTX_LINHA + CTX_GAP);
    GfxRect r = { x + CTX_PAD, by, CTX_W - CTX_PAD * 2.0f, CTX_LINHA };
    float f = focoAnim[i];
    // O menu usava uma pilula propria: 86px, cinza fixo, TXT_PLR_CORPO e uma
    // seta desenhada a mao. Isso fazia as acoes parecerem de outra tela. O
    // componente comum concentra altura, raio, luz, acento e tinta legivel;
    // aqui ele so recebe a opcao como uma acao primaria alinhada a esquerda.
    // O ICONE DIZ O QUE A OPCAO FAZ antes de a pessoa ler — os mesmos PNG
    // dos botoes redondos da tela de titulo (gfx.h), para o menu e o detalhe
    // falarem o mesmo vocabulario: seta = abrir, "+"/olho = biblioteca,
    // olho riscado/aberto = historico, oculto = tirar da fileira, aviao =
    // recomendar.
    const char *icone = "avancar";
    switch (ops[i].acao) {
      case OP_LISTA:     icone = ci->naLista ? "visto" : "mais"; break;
      case OP_ASSISTIDO: icone = cat_historico_estado_item(indiceAtual()) == 1
                                 ? "naovisto" : "visto"; break;
      case OP_TIRAR_CONTINUAR: icone = "oculto"; break;
      case OP_RECOMENDAR: icone = "recomendar"; break;
      default: break;
    }
    botao_pilula(r, ops[i].rot, icone, f, 1, 1, a);
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
