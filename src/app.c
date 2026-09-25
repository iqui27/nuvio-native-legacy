// Roteador de telas.
//
// Antes disto o main.c decidia entre home e detalhe com um if. Com menu, busca,
// biblioteca, ajustes e player, esse if viraria um emaranhado onde cada tela
// precisa saber das outras — e a regra de "quem come a tecla" ficaria espalhada
// por seis arquivos. Aqui existe uma tela CORRENTE e uma unica ordem de
// prioridade, escrita num lugar so.
//
// Ordem de quem recebe o D-pad, de cima para baixo:
//   1. player  — cobre a tela inteira
//   2. detalhe — camada sobre a tela corrente
//   3. menu    — camada sobre a tela corrente
//   4. a tela corrente (home, busca, biblioteca ou ajustes)
#include "ponteiro.h"
#include "app.h"
#include "registro.h"
#include "addonsui.h"
#include "login.h"
#include "sessao.h"
#include "perfis.h"
#include "perfilsel.h"
#include "sync.h"
#include "traktauth.h"
#include "simklauth.h"
#include "simkl.h"
#include "listas.h"
#include "text.h"
#include "tex_cache.h"
#include "artehero.h"
#include "vertudo.h"
#include "guia.h"
#include "epg.h"
#include "posplay.h"
#include "ctxmenu.h"
#include "marco.h"
#include <string.h>
#include "home.h"
#include "trailer.h"
#include "detail.h"
#include "menu.h"
#include "busca.h"
#include "biblioteca.h"
#include "explorar.h"
#include "agendaui.h"
#include "agenda.h"
#include "agendaviso.h"
#include "perfil.h"
#include "salvos.h"
#include "recomenda.h"
#include "recenviar.h"
#include "salvospainel.h"
#include "salvosintro.h"
#include "novidades.h"
#include "novidades11.h"
#include "novidades12.h"
#include "novidades13.h"
#include "novidades131.h"
#include "novidades132.h"
#include "novidades133.h"
#include "novidades134.h"
#include "novidades139.h"
#include "novidades1312.h"
#include "novidades142.h"
#include "telemetria.h"
#include "avisos.h"
#include "recintro.h"
#include "atualizacao.h"
#include "pipintro.h"
#include "social.h"
#include "ajustes.h"
#include "anim.h"
#include "diagnostico.h"
#include "debrid.h"
#include "player.h"
#include "streams.h"
#include "stalker.h"
#include "xtream.h"
#include "fontepref.h"
#include "video.h"
#include "addons.h"
#include "idioma.h"
#include "descoberta.h"
#include "proximo.h"
#include "trakt.h"
#include "visto.h"
#include "faixas.h"
#include "episodios.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <time.h>

// Link de debrid expira em minutos; um minuto e folga suficiente para o usuario
// apertar Reproduzir logo depois de abrir o titulo sem pagar uma busca a mais.
#define NV_LINK_VALIDO_MS 60000
// Uma URL pode responder bem a sonda e ainda assim travar quando o player
// abre o conteudo real. Filme nao passa pelo watchdog de canal, entao sem este
// prazo a tela fica em "carregando" para sempre e a pessoa precisa abrir a
// folha para escolher outra fonte na mao.
#define VOD_FONTE_PRAZO_MS 30000
#define VOD_FONTE_BUFFER_MS 30000
// QUANTAS FONTES O AUTOMATICO ENTREGA AO PLAYER NUMA REPRODUCAO: a primeira
// mais as de "Outra fonte se falhar" em Ajustes. Ate a 1.4.3 eram 8 fixas, e
// cada uma e um arquivo a mais na conta de debrid (issue #130).
#define VOD_FONTE_MAX_TENTATIVAS (1 + ajustes_fonte_repor())

// PORTA DE TESTE: "abrir:tt0121955" em /tmp/nuvio-key (main.c) abre o titulo
// pelo mesmo caminho de uma recomendacao ou aviso — sem navegar ate ele por
// setas. Chegar em South Park pela busca com o teclado da tela sao dezenas de
// teclas; aqui e uma linha.
static char abrirTeste[64];
void app_abrir_titulo(const char *imdb) {
  snprintf(abrirTeste, sizeof abrirTeste, "%s", imdb ? imdb : "");
}

static int aguardandoFonte;
// Episodio que o card de "Continuar assistindo" ANUNCIAVA quando o OK pediu
// para tocar (issue #93). Armado no ramo home_pediu_tocar e consumido pelo
// ramo de detail_pediu_reproduzir, no lugar do episodioAlvo — que neste
// instante ainda nao tem a lista de episodios nem o "proximo" que
// continuar_desenhar calculou para desenhar o card.
static int cwTocarT, cwTocarE;
static pthread_t fioFonte;
static int fioFonteVivo;                  // 0 = canal escolheu sem o fio
static _Atomic int fonteEscolhida = -2;   // release/acquire entre verificacao e UI
// O create_link do Stalker bloqueia a rede. O pedido leva uma copia do id;
// nenhum fio de rede le player_id_canal(), que pode mudar durante um zap.
static _Atomic unsigned stalkerGeracao;
enum { FJOB_IDLE, FJOB_RUNNING, FJOB_DONE };
enum { FJOB_NONE, FJOB_ADDON, FJOB_CANAL, FJOB_STALKER };
typedef struct {
  _Atomic int estado;
  int tipo;
  int renovando;
  int resultado;
  Uint32 prazo;
  unsigned geracao;
  char id[80];
  char url[4096];
} FonteJob;
static FonteJob fonteJob;
static unsigned fontePedidoGeracao;
static int fontePendenteTipo;
static int fontePendenteRenovando;
static unsigned fontePendenteGeracao;
static char fontePendenteId[80];
static int stalkerRenovando;

// WATCHDOG DE CANAL. A verificacao por fonte (stream_primeira_boa) custa ate
// ~20 s com candidatas mortas — medido no log: 4 delas estouraram o timeout e
// o canal abriu depois de 19 s de "carregando". Para TV ao vivo a lista do
// addon ja vem curada (FrostView manda FHD/HD/SD na ordem), entao o canal vai
// DIRETO para a primeira fonte e este par vigia: nao abriu em ~12 s ou o
// player marcou erro, tenta a proxima da lista sem pedir nada ao dono.
// A folha foi aberta com um player ESPERANDO fonte (ajustes_fonte_manual).
// Serve para uma pergunta so: se ela fechar sem escolha, quem avisa o player?
// Sem isto, sair da folha com Voltar deixaria a tela em "carregando" para
// sempre — o pedido de fonte ja tinha sido consumido e ninguem mais viria.
static int    folhaParaTocar;
// Quantas renovacoes de link seguidas um canal de portal ganha antes de virar
// erro. Tres: um link que expirou renova na primeira; um portal fora do ar
// falha nas tres e para de tentar em ~36 s em vez de nunca.
#define CANAL_STALKER_TENTATIVAS 3
static int    stalkerTentativas;
static int    canalFonteIdx = -1;         // indice na lista de streams, -1 = fora
static Uint32 canalFonteDesde;            // quando a fonte atual foi pedida
// So o automatico usa o fallback. Uma escolha manual e uma decisao explicita
// do dono e nao pode ser trocada por outra fonte por tras da tela.
static int    fonteVODAutomatica;
static int    fonteVODTentativas;
static Uint32 fonteVODDesde;
static void limparFonteVOD(void);
#define CANAL_FONTE_PRAZO_MS 12000
// PRAZO CURTO para fonte que JA PROVOU estar ruim. A conferencia de playlist
// (stream_canal_primeira_viva) classifica cada candidata antes de tocar; quando
// a escolhida e apenas "muda" — nao devolveu a playlist em 3 s — dar a ela os
// mesmos 12 s de uma fonte sadia e somar espera sobre espera. MEDIDO na LG num
// canal fora do ar: 3 s de conferencia + 12 s de watchdog POR FONTE.
// Os 12 s continuam valendo para a fonte VIVA, que e onde eles existem para
// servir: um canal 4K pesado legitimamente demora isso para abrir.
#define CANAL_FONTE_PRAZO_MUDA_MS 4000
// TRAVA DEPOIS DE ABRIR. Um canal ao vivo com 12 s de imagem congelada ja
// perdeu — ao contrario de um filme, nao ha nada para recuperar esperando: o
// que passou, passou. O numero e o mesmo do prazo de abertura de proposito, e
// pelo mesmo motivo de escala: um pico de rede que enche o buffer de novo
// termina MUITO antes disso, entao o que sobrevive a 12 s nao e pico, e fonte
// morta. Baixar mais arrisca trocar de fonte num engasgo que ia passar.
#define CANAL_TRAVA_MS 12000
#define CANAL_ABRE_TETO_MS 45000   // com buffer cheio e sem quadro; ver o watchdog

static PerfilDados perfilPendente;
static int perfilSucesso;
static _Atomic int perfilCarga; // 0=ocioso, 1=rede, 2=snapshot pronto
static _Atomic unsigned perfilGeracao = 1;
static pthread_mutex_t perfilTrava = PTHREAD_MUTEX_INITIALIZER;
typedef struct { unsigned geracao; int perfil; char conta[96]; } PerfilPedido;
static void *carregarPerfil(void *u) {
  PerfilPedido *pedido=u;
  PerfilDados novo={0};
  int sucesso=trakt_perfil(&novo);
  pthread_mutex_lock(&perfilTrava);
  // A troca de conta/perfil invalida a resposta. O worker termina, mas nunca
  // publica uma identidade antiga nem deixa um snapshot obsoleto na fila.
  if(pedido->geracao==atomic_load_explicit(&perfilGeracao,memory_order_acquire) &&
     atomic_load_explicit(&perfilCarga,memory_order_relaxed)==1 && sessao_logada() &&
     perfis_ativo()==pedido->perfil && !strcmp(sessao_usuario(),pedido->conta)){
    perfilSucesso=sucesso;
    perfilPendente=novo;
    atomic_store_explicit(&perfilCarga,2,memory_order_release);
  }
  pthread_mutex_unlock(&perfilTrava);
  free(pedido);
  return NULL;
}
static void invalidarPerfil(void) {
  atomic_fetch_add_explicit(&perfilGeracao,1,memory_order_acq_rel);
  atomic_store_explicit(&perfilCarga,0,memory_order_release);
  pthread_mutex_lock(&perfilTrava);memset(&perfilPendente,0,sizeof perfilPendente);perfilSucesso=0;pthread_mutex_unlock(&perfilTrava);
  perfil_definir_dados(NULL);
}
static void pedirPerfil(void) {
  pthread_t t;
  int esperado = 0;
  PerfilPedido *pedido;
  perfil_definir_carregando(1);
  if (!atomic_compare_exchange_strong(&perfilCarga, &esperado, 1)) return;
  pedido=calloc(1,sizeof *pedido);
  if(!pedido){atomic_store(&perfilCarga,0);perfil_definir_erro("Nao foi possivel iniciar a consulta. Tente novamente.");return;}
  pedido->geracao=atomic_load_explicit(&perfilGeracao,memory_order_acquire);
  pedido->perfil=perfis_ativo();
  snprintf(pedido->conta,sizeof pedido->conta,"%s",sessao_usuario());
  if (pthread_create(&t, NULL, carregarPerfil, pedido) == 0) pthread_detach(t);
  else { free(pedido); atomic_store(&perfilCarga,0); perfil_definir_erro("Nao foi possivel iniciar a consulta. Tente novamente."); }
}

// A verificacao faz uma requisicao por fonte candidata e bloqueia; num fio
// proprio a tela segue em 60fps mostrando "Abrindo fonte".
static void *escolherFonte(void *u) {
  FonteJob *job = u;
  // Ate 8: numa lista tipica de 12, as primeiras costumam ser do mesmo
  // provedor e falham juntas quando o arquivo nao esta em cache. Testar poucas
  // devolvia "nenhuma fonte serve" com fontes boas logo adiante. Sao 8 NO
  // MAXIMO e uma por vez, parando na primeira que serve; com "Primeira da
  // lista" streams.c reduz para 1 (issue #130).
  job->resultado = stream_primeira_boa(8);
  atomic_store_explicit(&job->estado, FJOB_DONE, memory_order_release);
  return NULL;
}
// O MESMO PARA CANAL, fora do fio de desenho. stream_canal_primeira_viva
// cria os fios da sonda e os JOINTA — e era chamada direto do laco de
// desenho: cada canal aberto no guia custava ate CANAL_PRAZO_S (3 s) de tela
// parada, medido na C9 do dono (21/09/2026: `upd=3012` em todo canal, "o
// guia fica travando"). O prazo do watchdog e decidido aqui tambem, com a
// classe da escolhida, e lido pelo laco quando fonteEscolhida deixa de ser -2.
static Uint32 canalFontePrazo;
static void *escolherFonteCanal(void *u) {
  FonteJob *job = u;
  int e;
  e = stream_canal_primeira_viva(8);
  job->prazo = (e >= 0 && !stream_canal_prazo_longo(e))
                  ? CANAL_FONTE_PRAZO_MUDA_MS : CANAL_FONTE_PRAZO_MS;
  // -1 so acontece quando TODAS responderam dizendo que nao tem segmento.
  // Ai nao ha o que tentar, mas a primeira da lista com o watchdog ainda e
  // melhor que uma tela de erro sem nenhuma tentativa.
  if (e < 0) e = stream_automatico();
  job->resultado = e;
  atomic_store_explicit(&job->estado, FJOB_DONE, memory_order_release);
  return NULL;
}
static void *escolherFonteStalker(void *u) {
  FonteJob *job = u;
  char url[4096];
  int ok = stalker_resolver(job->id, url, sizeof url);
  // O worker publica somente no seu job. O consumidor confere a geracao e o
  // id depois do acquire; nenhuma resposta velha toca o mailbox da sessao
  // corrente.
  snprintf(job->url, sizeof job->url, "%s", ok ? url : "");
  job->resultado = ok ? 0 : -1;
  atomic_store_explicit(&job->estado, FJOB_DONE, memory_order_release);
  return NULL;
}
static unsigned novaGeracaoFonte(void) {
  return atomic_fetch_add_explicit(&stalkerGeracao, 1, memory_order_acq_rel) + 1;
}
static void limparFontePendente(void) {
  fontePendenteTipo = FJOB_NONE;
  fontePendenteRenovando = 0;
  fontePendenteGeracao = 0;
  fontePendenteId[0] = 0;
}
static void limparFonteVOD(void) {
  fonteVODAutomatica = 0;
  fonteVODTentativas = 0;
  fonteVODDesde = 0;
}
static int iniciarFonteJob(int tipo, unsigned geracao, const char *id, int renovando) {
  FonteJob *job = &fonteJob;
  if (fioFonteVivo || atomic_load_explicit(&job->estado, memory_order_acquire) != FJOB_IDLE)
    return 1; // single-flight: o chamador agenda e o polling inicia depois
  job->tipo = FJOB_NONE;
  job->renovando = 0;
  job->resultado = -1;
  job->prazo = 0;
  job->geracao = 0;
  job->id[0] = 0;
  job->url[0] = 0;
  job->tipo = tipo;
  job->renovando = renovando;
  job->geracao = geracao;
  if (id) snprintf(job->id, sizeof job->id, "%s", id);
  atomic_store_explicit(&job->estado, FJOB_RUNNING, memory_order_release);
  if (pthread_create(&fioFonte, NULL,
                     tipo == FJOB_STALKER ? escolherFonteStalker :
                     tipo == FJOB_CANAL ? escolherFonteCanal : escolherFonte,
                     job) != 0) {
    atomic_store_explicit(&job->estado, FJOB_IDLE, memory_order_release);
    return -1;
  }
  fioFonteVivo = 1;
  return 0;
}
static int pedirFonteJob(int tipo, unsigned geracao, const char *id, int renovando) {
  int r = iniciarFonteJob(tipo, geracao, id, renovando);
  if (r == 1) {
    fontePendenteTipo = tipo;
    fontePendenteRenovando = renovando;
    fontePendenteGeracao = geracao;
    snprintf(fontePendenteId, sizeof fontePendenteId, "%s", id ? id : "");
    return 1;
  }
  if (r < 0) return -1;
  return 0;
}
#include "catalogo.h"
#include "gfx.h"
#include "catalogo.h"
#include "layout.h"
#include <stdio.h>

static Tela tela = TELA_HOME;
static int sair = 0;
static int saiuPorEsquerda;   // a ultima tecla foi ESQUERDA (ver app_evento)
// A barra e uma camada de navegacao, mas nao pode disputar o Guia de TV nem
// um canal minimizado. Manter a regra aqui evita que cada tela invente sua
// propria nocao de PiP/Guia.
static int sidebar_permitida(void) {
  return tela != TELA_GUIA && !player_mini_ativo();
}
// perfis_ativo() no instante em que a tela de escolha abriu. So serve para uma
// pergunta: a pessoa TROCOU de perfil, ou confirmou o mesmo? Agora que a tela
// aparece a cada arranque, confirmar o mesmo perfil e o caso comum — e recarga
// (invalidar o Trakt, reaplicar ajustes, um ciclo de sync inteiro de ~8
// requisicoes) num perfil que nao mudou seria pagar o preco da troca em toda
// abertura do app.
static int perfilAntes = 1;

// O detalhe precisa do retangulo REAL de onde o card saiu para o voo comecar
// dali. Cada tela que abre um titulo entrega o seu; quando nenhuma entrega
// (caso do menu ou de um indice vindo de fora), cai para a tela inteira.
static void abrirTitulo(const HomeItem *it) {
  const CatItem *c;
  if (!it || !it->arte) return;
  // ITEM COM ID "tmdb:<n>" — resultado de busca de um addon do TMDB (o dono,
  // 20/09/2026: "tem titulos da busca que quando abre nao vem com as artes e
  // informacoes nenhuma"). O detalhe pede tudo ao Cinemeta por imdb, e
  // /meta/series/tmdb:456.json responde 404: sem temporadas, sem episodios,
  // addons sem fonte. O vertudo ja resolvia esse id pelo caminho da
  // filmografia (external_ids -> imdb -> meta); a busca e a home entravam por
  // aqui e nao. Um so lugar para os dois: o titulo resolvido entra no catalogo
  // e trocaDeTituloSeSolicitada abre ele.
  c = cat_item(it->indice);
  if (c && !strncmp(c->imdb, "tmdb:", 5) && desc_chave_tmdb() &&
      desc_chave_tmdb()[0] && !desc_titulo_buscando()) {
    // Sem chave do TMDB nao ha como resolver: abre como dava (arte e sinopse,
    // sem episodios) em vez de nao abrir nada.
    desc_pedir_titulo_tmdb(atol(c->imdb + 5),
                           !strcmp(c->tipo, "series") ? "tv" : "movie");
    return;
  }
  detail_abrir(it);
}

static void abrirPorIndice(int i) {
  const CatItem *c = cat_item(i);
  if (!c || (!c->backdrop[0] && !c->poster[0])) return;
  HomeItem it;
  GfxRect tudo = { 0, 0, NV_TELA_W, NV_TELA_H };
  // O `it` e da PILHA e esta funcao preenchia todos os campos MENOS o indice —
  // que ia como lixo. Como a biblioteca e a busca abrem por aqui, qualquer
  // titulo escolhido nelas levava ao mesmo filme. A home nao sofria porque ela
  // entrega o HomeItem inteiro, ja com o indice.
  //
  // O campo existe exatamente por causa deste defeito, e o comentario dele em
  // home.h ja avisava: "faltava, e por isso o detalhe abria sempre o item 0".
  // Zerar a struct antes garante que o proximo campo novo nasca definido em vez
  // de repetir a historia.
  memset(&it, 0, sizeof it);
  it.indice = i;
  it.rect = tudo;
  it.arte = c->backdrop[0] ? c->backdrop : c->poster;
  it.titulo = c->titulo;
  it.genero = c->genero;
  it.meta = c->meta;
  abrirTitulo(&it);
}

// Monta o id que os addons esperam. Para serie e "tt1234567:temporada:episodio";
// sem os dois numeros a resposta volta VAZIA com HTTP 200, e era por isso que o
// addons_buscar cravava ":1:1" — o que fazia toda a serie mostrar as fontes do
// episodio 1, qualquer que fosse o escolhido.
static void idDoAlvo(const CatItem *ci, char *dst, size_t n) {
  int t = 0, e = 0;
  if (!ci) { if (n) dst[0] = 0; return; }
  if (!strcmp(ci->tipo, "series") && detail_ep_foco(&t, &e) && t > 0 && e > 0)
    snprintf(dst, n, "%.*s:%d:%d", (int)strcspn(ci->imdb,":"),ci->imdb, t, e);
  else
    snprintf(dst, n, "%s", ci->imdb);
}

// A tela de Diagnostico foi aberta pelo cartao da 1.4.2, nao por Ajustes.
static int diagDaHome;

static void trocarTela(Tela nova) {
  if (nova == tela) return;
  tela = nova;
  // Cada tela zera o proprio estado ao ser aberta: voltar para a busca com o
  // texto de duas navegacoes atras seria lixo, nao memoria util.
  switch (tela) {
    case TELA_EXPLORAR:   explorar_iniciar();   break;
    case TELA_GUIA:       guia_abrir();         break;
    case TELA_BUSCA:      busca_iniciar();      break;
    case TELA_BIBLIOTECA: biblioteca_iniciar(); break;
    case TELA_AGENDA:     agendaui_iniciar();   break;
    case TELA_PERFIL:     perfil_abrir(); pedirPerfil(); break;
    case TELA_AJUSTES:    ajustes_iniciar();    break;
    case TELA_DIAGNOSTICO: diagnostico_iniciar(); break;
    default: break;
  }
}

static void alvoPlayer(char *alvo, size_t tam) {
  const CatItem *c = cat_item(player_indice());
  int t, e;
  player_episodio_atual(&t, &e);
  // Canal no ar: o id congelado na abertura vence o indice, que uma
  // republicacao do catalogo ja pode ter apontado para outro item.
  if (player_id_canal()[0]) { snprintf(alvo,tam,"%s",player_id_canal()); return; }
  if (!c) { alvo[0] = 0; return; }
  if (t > 0 && e > 0) snprintf(alvo,tam,"%.*s:%d:%d",(int)strcspn(c->imdb,":"),c->imdb,t,e);
  else snprintf(alvo,tam,"%s",c->imdb);
}
static void buscarParaPlayer(void) {
  char alvo[64]; alvoPlayer(alvo,sizeof alvo);
  const char *idC = player_id_canal();
  // Episodio novo abre um ciclo novo de fontes. A fonte automatica do
  // episodio anterior nao pode contaminar o watchdog nem a lista de exclusao.
  limparFonteVOD();
  // CARIMBA O ALVO ANTES DE PEDIR (issue #101). A lista que voltar passa a
  // saber de que episodio ela e; sem isto ninguem consegue distinguir "a lista
  // do E6" de "a lista do E5 que ninguem invalidou". Ver streams.h.
  stream_definir_alvo(alvo);
  if (idC[0]) { addons_buscar(alvo,"tv"); addons_buscar_legendas(alvo,"tv"); return; }
  {
    const CatItem *c = cat_item(player_indice());
    if (c && alvo[0]) {
      addons_buscar(alvo,c->tipo);
      addons_buscar_legendas(alvo,c->tipo);
    }
  }
}
// ID BASE DO TITULO EM JOGO, sem ":temporada:episodio".
//
// E a chave da preferencia de fonte, e ela e DO TITULO de proposito: o issue
// #56 pede que o episodio seguinte continue na fonte do anterior, entao um
// registro por episodio nao serviria para nada — ele so existiria depois de a
// pessoa ja ter escolhido naquele episodio.
//
// Canal ao vivo nao tem preferencia: a lista dele e outra a cada zapeada, o
// "provedor" e o mesmo para todos os canais do addon, e ali quem manda e o
// watchdog de fonte morta. Devolve vazio, e vazio desliga tudo isto.
static void idBaseDoTitulo(char *dst, size_t tam) {
  const CatItem *c;
  if (tam) dst[0] = 0;
  if (player_id_canal()[0]) return;
  c = cat_item(player_aberto() ? player_indice() : detail_indice());
  if (!c || !c->imdb[0]) return;
  fontepref_id_base(c->imdb, dst, (unsigned)tam);
}

static void episodioDoDetalhe(void) {
  int t=0,e=0;
  detail_ep_foco(&t,&e);
  player_definir_episodio(t,e);
}

// Canal de portal Stalker: a fonte NAO vem de addon, e o link e de uso unico.
//
// Cada reproducao pede um `create_link` novo. Guardar a URL seria o unico erro
// que este caminho nao perdoa: ela vale minutos, e o que sobrevive a pausa, ao
// zap e a reabertura do app e o `cmd`, que mora em stalker.c.
//
// Bloqueia ~200-600 ms na TV, no fio de desenho. E o mesmo custo que o ramo de
// addon ao lado ja paga: stream_canal_primeira_viva cria fios e os JOINTA
// aqui, gastando ate meio segundo por fonte morta. Uma requisicao e menos que
// isso, e o canal nao abre sem ela de qualquer jeito.
static int montarCanalStalker(const char *id, const char *url) {
  Stream s;
  if (!id || !id[0] || !url || !url[0]) return -1;
  memset(&s, 0, sizeof s);
  snprintf(s.url, sizeof s.url, "%s", url);
  snprintf(s.rotulo, sizeof s.rotulo, "%s", "Portal IPTV");
  snprintf(s.provedor, sizeof s.provedor, "%s", "stalker");
  s.fileIdx = -1;
  stream_definir_lista(&s, 1);
  return 0;
}

// Canal Xtream: a URL e estavel e nasce do cadastro (ver xtream.h). Nao vai a
// rede; e o mesmo formato de lista de UMA fonte do portal Stalker.
static int resolverCanalXtream(void) {
  Stream s;
  char url[4096];
  if (!xtream_url(player_id_canal(), url, sizeof url)) return -1;
  memset(&s, 0, sizeof s);
  snprintf(s.url, sizeof s.url, "%s", url);
  snprintf(s.rotulo, sizeof s.rotulo, "%s", "Xtream");
  snprintf(s.provedor, sizeof s.provedor, "%s", "xtream");
  s.fileIdx = -1;
  stream_definir_lista(&s, 1);
  return 0;
}

// Um unico worker pode existir. O fio de desenho so junta depois de DONE;
// enquanto RUNNING, uma troca invalida por geracao e agenda o pedido seguinte.
// Assim Xtream (sincrono) nao espera Stalker e addon nao reutiliza o handle.
static void processarFonteJob(void) {
  FonteJob *job = &fonteJob;
  int tipo, renovando, resultado, estado, mesmoId = 1;
  Uint32 prazo;
  unsigned geracao;
  char id[80], url[4096];
  int aplicar;
  if (!fioFonteVivo ||
      atomic_load_explicit(&job->estado, memory_order_acquire) != FJOB_DONE)
    return;
  pthread_join(fioFonte, NULL);
  fioFonteVivo = 0;
  estado = atomic_load_explicit(&job->estado, memory_order_relaxed);
  (void)estado;
  tipo = job->tipo; renovando = job->renovando; resultado = job->resultado;
  prazo = job->prazo; geracao = job->geracao;
  snprintf(id, sizeof id, "%s", job->id);
  snprintf(url, sizeof url, "%s", job->url);
  atomic_store_explicit(&job->estado, FJOB_IDLE, memory_order_release);
  aplicar = geracao == fontePedidoGeracao &&
            geracao == atomic_load_explicit(&stalkerGeracao, memory_order_acquire);
  if (tipo == FJOB_STALKER) {
    const char *idAtual = player_id_canal();
    mesmoId = idAtual[0] && !strcmp(id, idAtual);
    if (renovando) {
      int sessao = (player_aberto() || player_mini_ativo()) && !player_quer_sair();
      if (aplicar && mesmoId && sessao && resultado == 0 && url[0] &&
          montarCanalStalker(id, url) == 0) {
        const Stream *s = stream_item(0);
        stalkerRenovando = 0;
        stalkerTentativas++;
        stream_definir_atual(0);
        canalFonteIdx = 0;
        canalFonteDesde = SDL_GetTicks();
        if (s) player_definir_fonte(s->url);
        marco("canal: link renovado");
      } else if (aplicar && mesmoId) {
        stalkerRenovando = 0;
        canalFonteIdx = -1;
        if (sessao) {
          if (player_mini_ativo()) player_fechar_mini();
          else player_erro_fonte();
        }
      }
      aplicar = 0; // renovacao ja foi aplicada aqui, nunca no bloco generico
    } else if (aplicar && mesmoId &&
               (player_aberto() || player_mini_ativo()) && !player_quer_sair() &&
               resultado == 0 && url[0] &&
               montarCanalStalker(id, url) == 0) {
      fonteEscolhida = 0;
      canalFontePrazo = CANAL_FONTE_PRAZO_MS;
    } else if (aplicar) {
      fonteEscolhida = -1;
    }
  } else if (aplicar) {
    fonteEscolhida = resultado;
    if (tipo == FJOB_CANAL) canalFontePrazo = prazo;
  }
  // O job antigo foi recolhido. Só agora o slot pode receber o pedido que
  // ficou pendente durante a troca; uma geracao nova torna o pendente obsoleto.
  if (fontePendenteTipo &&
      (fontePendenteRenovando || aguardandoFonte == 2) &&
      fontePendenteGeracao == fontePedidoGeracao &&
      fontePendenteGeracao == atomic_load_explicit(&stalkerGeracao, memory_order_acquire)) {
    int pTipo = fontePendenteTipo, pRenovando = fontePendenteRenovando;
    unsigned pGeracao = fontePendenteGeracao;
    char pId[80];
    snprintf(pId, sizeof pId, "%s", fontePendenteId);
    limparFontePendente();
    if (iniciarFonteJob(pTipo, pGeracao, pId, pRenovando) < 0) {
      if (pRenovando) stalkerRenovando = 0;
      else fonteEscolhida = -1;
    }
  }
}
static void cancelarFonteSeSaiu(void) {
  int jobStalker = (fioFonteVivo && fonteJob.tipo == FJOB_STALKER) ||
                   fontePendenteTipo == FJOB_STALKER;
  if (jobStalker &&
      (player_quer_sair() || (!player_aberto() && !player_mini_ativo()))) {
    novaGeracaoFonte();
    stalkerRenovando = 0;
    fonteEscolhida = -1;
    canalFonteIdx = -1;
    limparFontePendente();
  }
}

// NENHUMA FONTE, COM A CAUSA NO CARTAO (issue #112). Antes daqui saia sempre
// o cartao generico: "Nao foi possivel abrir a fonte / Abra Fontes para
// escolher outra opcao" — num canal cuja lista veio vazia, nada foi aberto e
// a folha de Fontes estaria vazia, entao as duas frases mandavam a pessoa a
// um lugar sem saida. No registro 1647 (Samsung) foram oito canais assim e o
// relato foi "tela preta". A causa ja existia para a folha de fontes
// (addons_motivo_vazio); aqui ela vira o titulo do cartao. Sem causa
// conhecida (lista do cache, ou houve fonte e nenhuma serviu) o canal ainda
// ganha uma frase de canal, e o filme fica com a generica.
static void erroSemFonte(void) {
  char motivo[160];
  int canal = player_id_canal()[0] != 0;
  int semPlano = debrid_sem_plano();
  // CONTA DE DEBRID SEM PLANO vem antes de tudo: e a causa que a pessoa pode
  // resolver, e "nenhuma fonte serve" sozinho a mandava procurar outra fonte
  // do mesmo servico (registros 1731-1774). O aviso fica marcado como dado.
  if (semPlano && !canal) {
    (void)debrid_sem_plano_novo();
    player_erro_fonte_motivo(i18n(debrid_sem_plano_frase(semPlano)),
        i18n("Abra Fontes para escolher uma fonte direta."));
  } else if (!canal && debrid_fora_de_cache() > 0) {
    // NENHUMA SERVIU E HAVIA TORRENT FORA DE CACHE (o "P2P" do TorBox). O
    // automatico nao manda baixar — so toca o que esta pronto —, mas a folha
    // manda. Sem esta frase a pessoa lia "nenhuma fonte" numa lista cheia.
    player_erro_fonte_motivo(i18n("As fontes torrent desta lista não estão no cache do debrid"),
        i18n("Abra Fontes e escolha uma: o serviço começa a baixar."));
  } else if (addons_motivo_vazio(motivo, sizeof motivo))
    player_erro_fonte_motivo(motivo, canal
        ? i18n("Escolha outro canal no guia ou tente de novo mais tarde.")
        : i18n("Abra Fontes para escolher outra opção ou recarregar."));
  else if (canal)
    player_erro_fonte_motivo(i18n("Nenhuma fonte deste canal abriu agora"),
        i18n("Escolha outro canal no guia ou tente de novo mais tarde."));
  else player_erro_fonte();
}

// TORRENT SEM URL ESCOLHIDO A DEDO — o "P2P" do TorBox.
//
// A folha entregava a escolha a player_definir_fonte(s->url), e num torrent
// que so tem infoHash a url e VAZIA: player_definir_fonte volta calado e o
// player fica em "carregando" para sempre. Registros 1739 (1.4.1) e 2325
// (1.4.3): "fonte escolhida: Torrentio 1080p" e nenhum "[video] URL" depois.
// So o automatico passava pelo debrid — e o automatico, de proposito, so toca
// o que esta em cache. Um torrent fora de cache no TorBox, portanto, nao
// tocava por caminho nenhum.
//
// Aqui a escolha vai a debrid_resolver_escolhido NUM FIO PROPRIO (a rede do
// debrid leva segundos, e a TV nao pode parar de desenhar), que manda o
// servico baixar o que nao tem. Terminado: toca. Ainda baixando: o cartao
// diz quem baixa e quanto falta, em vez da tela parada.
//
// UM FIO POR VEZ, como o de fonte. Uma escolha nova com um em curso fica
// PENDENTE e sai quando o anterior termina; `torrentSessao` sobe a cada
// escolha, e a resposta que chega de uma escolha velha e descartada.
typedef struct {
  _Atomic int estado;           // 0 livre, 1 rodando, 2 pronto
  int indice, resultado, pct;
  unsigned geracaoLista, sessao;
  char url[4096], servico[32];
} TorrentJob;
static TorrentJob torrentJob;
static pthread_t fioTorrent;
static int fioTorrentVivo;
static unsigned torrentSessao;
static int torrentPendente = -1;
static unsigned torrentPendenteGeracao;

static void *resolverTorrentEscolhido(void *u) {
  TorrentJob *j = u;
  j->resultado = stream_resolver_escolhida(j->indice, j->geracaoLista, j->url,
                                           sizeof j->url, j->servico,
                                           sizeof j->servico, &j->pct);
  atomic_store_explicit(&j->estado, 2, memory_order_release);
  return NULL;
}
static int iniciarTorrentJob(int indice, unsigned geracaoLista) {
  TorrentJob *j = &torrentJob;
  if (fioTorrentVivo) {
    torrentPendente = indice;
    torrentPendenteGeracao = geracaoLista;
    return 1;
  }
  j->indice = indice; j->geracaoLista = geracaoLista; j->sessao = torrentSessao;
  j->resultado = 0; j->pct = -1; j->url[0] = 0; j->servico[0] = 0;
  atomic_store_explicit(&j->estado, 1, memory_order_release);
  if (pthread_create(&fioTorrent, NULL, resolverTorrentEscolhido, j) != 0) {
    atomic_store_explicit(&j->estado, 0, memory_order_release);
    return -1;
  }
  fioTorrentVivo = 1;
  return 0;
}
// A escolha manual de um torrent: abre o player em "carregando" (quem chama
// ja abriu) e pede a url ao debrid.
static void pedirTorrentEscolhido(int indice) {
  torrentSessao++;
  torrentPendente = -1;
  if (iniciarTorrentJob(indice, stream_lista_geracao()) < 0) {
    player_erro_fonte();
    return;
  }
  player_toast(i18n("Pedindo o torrent ao serviço de debrid…"), 5000);
}
static void processarTorrentJob(void) {
  TorrentJob *j = &torrentJob;
  int valido;
  if (!fioTorrentVivo ||
      atomic_load_explicit(&j->estado, memory_order_acquire) != 2)
    return;
  pthread_join(fioTorrent, NULL);
  fioTorrentVivo = 0;
  atomic_store_explicit(&j->estado, 0, memory_order_release);
  // A resposta so vale para a MESMA escolha, na mesma lista, com o player
  // ainda esperando por ela: um episodio novo, outra fonte escolhida ou o
  // player fechado no meio fazem a resposta velha nao tocar nada.
  valido = j->sessao == torrentSessao && player_aberto() && !player_quer_sair() &&
           stream_atual() == j->indice && stream_lista_geracao() == j->geracaoLista;
  if (valido) {
    if (j->resultado == 1 && j->url[0]) {
      player_definir_fonte(j->url);
    } else if (j->resultado == DEBRID_BAIXANDO) {
      char titulo[160];
      const char *serv = j->servico[0] ? j->servico : "debrid";
      if (j->pct >= 0)
        snprintf(titulo, sizeof titulo, i18n("O %s está baixando este torrent (%d%%)"),
                 serv, j->pct);
      else
        snprintf(titulo, sizeof titulo, i18n("O %s está baixando este torrent"), serv);
      player_erro_fonte_motivo(titulo,
          i18n("Ele fica na sua conta: escolha esta fonte de novo em alguns minutos."));
    } else if (j->resultado == 0 && debrid_sem_plano()) {
      player_erro_fonte_motivo(i18n(debrid_sem_plano_frase(debrid_sem_plano())),
          i18n("Abra Fontes para escolher uma fonte direta."));
    } else if (j->resultado == 0) {
      player_erro_fonte_motivo(i18n("O serviço de debrid não abriu este torrent"),
          i18n("Abra Fontes para escolher outra opção."));
    } else {
      player_erro_fonte();
    }
  }
  if (torrentPendente >= 0) {
    int i = torrentPendente;
    torrentPendente = -1;
    if (iniciarTorrentJob(i, torrentPendenteGeracao) < 0 && player_aberto())
      player_erro_fonte();
  }
}

// Filme/serie nao tem o watchdog de canal porque nao ha troca de emissora.
// Ainda assim a sonda de URL nao prova que o decoder vai aceitar o arquivo:
// alguns links respondem HTTP 200 e o uMS fica em load sem erro. Se o
// automatico caiu nesse caso, tira a candidata da fila e verifica a proxima;
// escolha manual fica intacta.
// Pede a verificacao da proxima candidata automatica, com a sessao do player
// aberta. 0 quando nem deu para pedir (ja mostrou o erro).
static int pedirProximaFonteVOD(void) {
  unsigned geracao = novaGeracaoFonte();
  aguardandoFonte = 2;
  fontePedidoGeracao = geracao;
  fonteEscolhida = -2;
  limparFontePendente();
  if (pedirFonteJob(FJOB_ADDON, geracao, NULL, 0) < 0) {
    aguardandoFonte = 0;
    limparFonteVOD();
    player_erro_fonte();
    return 0;
  }
  return 1;
}
static void tentarProximaFonteVOD(void) {
  Uint32 desde;
  int atual, motivo = 0;
  if (!fonteVODAutomatica || player_id_canal()[0] || !player_aberto() ||
      player_quer_sair() || aguardandoFonte != 0 || !fonteVODDesde) return;
  desde = SDL_GetTicks() - fonteVODDesde;
  if (video_falhou() || player_fonte_falhou()) motivo = 1;
  else if (player_carregando() && desde > VOD_FONTE_PRAZO_MS) motivo = 2;
  else if (video_bufferando_ms() > VOD_FONTE_BUFFER_MS) motivo = 3;
  if (!motivo) return;

  atual = stream_atual();
  if (atual >= 0) stream_automatico_excluir(atual);
  if (fonteVODTentativas >= VOD_FONTE_MAX_TENTATIVAS ||
      stream_automatico() < 0) {
    printf("[fonte] automatico VOD sem proxima candidata (motivo=%d, %d de %d)\n",
           motivo, fonteVODTentativas, VOD_FONTE_MAX_TENTATIVAS);
    fonteVODAutomatica = 0;
    player_erro_fonte();
    return;
  }
  if (!pedirProximaFonteVOD()) return;
  printf("[fonte] automatico VOD descartou %d; verificando proxima (%d/%d)\n",
         atual, fonteVODTentativas + 1, VOD_FONTE_MAX_TENTATIVAS);
  marco("fonte VOD travou; tentando proxima");
}

// TOCAR UM CANAL, direto — o "OK assiste" do guia e o zap do CH+/-.
//
// O guia entrega um CatItem pronto (id completo do addon, tipo "channel"):
// canal que ja passou pelo catalogo ganha o indice dele, os demais entram por
// cat_acrescentar — o player le tudo de cat_item. Reusa o mesmo encerramento
// do episodio seguinte: o fluxo antigo para antes de o novo pedir fonte, e
// aguardandoFonte=1 manda a escolha para stream_primeira_boa, como o
// Reproduzir do detalhe — sem folha de fontes no meio, que para um canal ao
// vivo so atrapalharia o zapear.
static void tocarCanal(const CatItem *it) {
  int ni;
  if (!it || !it->imdb[0]) return;
  ni = cat_indice_por_imdb(it->imdb);
  if (ni < 0) ni = cat_acrescentar(it);
  if (ni < 0) return;
  limparFonteVOD();
  if (player_aberto()) player_encerrar();
  player_abrir(ni, NULL);
  // cat_acrescentar e cat_definir_tudo correm juntos: se uma republicacao
  // atravessou os dois, ni ja nao e o canal. A marca garante a sessao.
  player_marcar_canal(it);
  novaGeracaoFonte();        // invalida qualquer resolver do canal anterior
  limparFontePendente();
  canalFonteIdx = -1;             // canal novo: watchdog arma de novo no fim da busca
  stalkerTentativas = 0;          // canal novo: o teto de renovacao recomeca
  stalkerRenovando = 0;            // resposta de renovacao antiga sera descartada
  // Canal de portal nao esta em addon nenhum: perguntar seria esperar o prazo
  // de todos eles para receber lista vazia, com a pessoa olhando "carregando".
  if (!stalker_e_id(it->imdb) && !xtream_e_id(it->imdb)) {
    // So o addon que publicou o canal responde por ele; ver alvoBase em
    // addons.c e o que o FrostView fora do ar custava.
    addons_definir_origem(guia_canal_origem());
    addons_buscar(it->imdb, "tv");
    addons_definir_origem(NULL);
  }
  aguardandoFonte = 1;
  marco("guia: buscando fontes do canal");
}

// A home carregou? Sem arte no pacote ela nao carrega, e ate agora isso
// DERRUBAVA o app: app_iniciar devolvia 0 e o main saia com codigo 1. Num
// pacote de dono isso nunca acontecia porque a arte ia junto; num pacote
// distribuivel, que nao pode levar arte nem credencial de ninguem, esse era o
// comportamento da PRIMEIRA execucao de todo mundo — o app abria e fechava,
// antes mesmo da tela de login.
static int homePronta;

int app_iniciar(const char *dirArte) {
  diagnostico_recuperar_checkpoint();
  homePronta = home_iniciar(dirArte);
  if (!homePronta)
    printf("[app] sem arte no pacote: a home so aparece depois do primeiro sync\n");
  menu_iniciar();
  perfil_iniciar();
  // ANTES do primeiro sync e da primeira descoberta: salvos_aplicar_catalogo
  // marca `naLista` no catalogo do cache, entao o painel e a Biblioteca ja
  // abrem certos no primeiro quadro. Ler depois faria a lista local piscar.
  salvos_iniciar();
  // AS LISTAS FIXADAS, PELO MESMO MOTIVO E ANTES DA PRIMEIRA HOME. Uma lista do
  // Trakt que a Biblioteca levou para a Home so vira fileira quando lst_iniciar
  // reinjeta a pasta dela em colecoes.c; chamando isto so ao ABRIR a Biblioteca,
  // a fileira sumiria da home a cada reinicio ate a pessoa passar por la.
  lst_iniciar();
  agenda_iniciar();
  // A FONTE LEMBRADA, do mesmo jeito e pelo mesmo motivo: ela e lida antes do
  // primeiro Reproduzir, que pode acontecer segundos depois do arranque quando
  // a pessoa abre direto no "Continuar assistindo".
  fontepref_iniciar();
  // MESMA RAZAO, OUTRA LISTA: o cache de recomendacoes guarda titulo e poster,
  // entao a aba Social do painel AZUL desenha no primeiro quadro, antes de
  // existir catalogo e antes de a rede responder. Isto nao abre conexao — quem
  // faz isso e recomenda_verificar, la embaixo, com a home ja de pe.
  recomenda_iniciar();
  // Sem conta, o app abre no login. Com sessao gravada ele nem passa por ela —
  // pedir o codigo de novo a cada arranque seria o mesmo que nao ter gravado.
  if (sessao_logada()) {
    tela = TELA_HOME;
    // Com sessao gravada o ciclo comeca no arranque: e ele que traz os addons
    // e o Trakt da pessoa, sem os quais a home mostra so o que veio no pacote.
    sync_iniciar();
    // A ESCOLHA DE PERFIL ABRE AQUI, e nao daqui a alguns segundos.
    //
    // Ela ja aparecia em conta com mais de um perfil, mas so quando o sync
    // respondia: a home aparecia primeiro e era COBERTA depois — o pior dos
    // dois mundos, porque a pessoa via a lista de outro perfil por um instante.
    // Com o cache de perfis lido em perfis_carregar_ativo() a pergunta ja pode
    // ser feita no primeiro quadro.
    //
    // Isto NAO custa nada ao arranque medido: home_iniciar() ali em cima ja
    // montou a home do cache ("catalogo do cache na tela", 844 ms na TV) antes
    // desta linha, e o laco de atualizacao continua alimentando a home por tras
    // da tela de escolha. A tela e uma CAMADA na frente de uma home pronta, nao
    // uma etapa antes dela.
    if (perfis_precisa_escolher()) {
      tela = TELA_ESCOLHA_PERFIL;
      perfilAntes = perfis_ativo();
      perfilsel_iniciar();
    }
  } else {
    tela = TELA_LOGIN;
    login_iniciar();
  }
  return 1;
}

void app_evento(const SDL_Event *e) {
  if (e->type == SDL_QUIT) { sair = 1; return; }

  // A explicacao de primeira abertura bloqueia o restante da interface ate
  // ser reconhecida ou fechada. Ela aparece na Home, mas pertence ao fluxo de
  // Diagnostico e por isso precisa vir antes de qualquer atalho global.
  if (diagnostico_intro_aberto()) {
    diagnostico_intro_evento(e);
    return;
  }

  // O PAINEL DE LOG VEM ANTES DE TUDO, inclusive do login.
  //
  // Ele e uma ferramenta de diagnostico, e o momento em que mais se precisa
  // dela e justamente aquele em que o app esta preso numa tela — o travamento
  // que motivou o painel DOM do Tizen aconteceu NO LOGIN. Roteado depois do
  // login, a tecla vermelha nao chegaria ali e o painel seria inutil onde ele
  // mais importa. Enquanto aberto ele engole todo o teclado, incluindo o KEYUP
  // do toque que o abriu ou fechou (ver a nota da armadilha em registro.c).
  if (registro_evento(e)) return;
  // A CENTRAL DE AVISOS vem logo depois do painel de log: com o toast na tela
  // AZUL/CH+ abrem a lista, e com a lista aberta ela come o teclado. Fora
  // desses dois estados ela nao toca em nada (ver avisos.h).
  if (avisos_evento(e)) return;

  // PORTA DE TESTE: F10 abre o Guia de TV de onde quer que o app esteja.
  //
  // Existe pelo mesmo motivo que "log" (F9) e "azul" (S) em main.c: exercitar
  // uma tela por injecao de tecla (/tmp/nuvio-key) sem depender de ONDE o foco
  // esta. Em 18/09 eu naveguei as cegas ate o guia tres vezes; nas tres o foco
  // caiu num card de filme e o OK marcou "A Captura" como assistido na conta
  // do dono. Uma tecla que vai direto elimina a classe inteira de acidente.
  // Nao ha F10 em controle de TV, entao isto nao muda nada para quem usa.
  if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_F10 &&
      login_concluido() && perfilsel_concluido() && !player_aberto()) {
    menu_fechar();
    trocarTela(TELA_GUIA);
    menu_definir_destino(MENU_GUIA);
    return;
  }

  // O login vem antes de tudo, inclusive do player: enquanto nao ha conta o
  // resto do app nao tem dado nenhum para operar. A escolha de perfil vem logo
  // depois, porque e ela que define para QUEM o resto do app vai sincronizar.
  if (tela == TELA_LOGIN)          { login_evento(e);     return; }
  if (tela == TELA_ESCOLHA_PERFIL) { perfilsel_evento(e); return; }

  // ALTERNA O PAINEL DE SALVOS, COM REPOUSO — e o repouso e o conserto.
  //
  // A TECLA AZUL MUDOU DE DONO. Ate esta versao ela abria o painel "Sua
  // atividade" (perfil_abrir_lateral), um resumo de minutos assistidos. Agora
  // abre "Salvos". A troca foi pedida assim: o atalho mais rapido do controle
  // deve responder "o que eu guardei para ver?", que leva a uma acao, e nao
  // "quantos minutos assisti?", que e relatorio — e relatorio ganhou tela
  // inteira (MENU_PERFIL abre TELA_PERFIL direto). Quem apertava a AZUL por
  // habito e avisado uma vez pelo explicador de salvosintro.c.
  //
  // `!e->key.repeat` sozinho nao segura uma tecla SEGURADA num controle de TV.
  // A bandeira `repeat` do SDL vale para a repeticao que o PROPRIO SDL gera a
  // partir de um teclado; o firmware da TV manda a tecla segurada como uma
  // sequencia de KEYDOWNs SEPARADOS, cada um com repeat=0. Este bloco entao
  // alternava abre/fecha/abre/fecha varias vezes por segundo, e o que a pessoa
  // ve e o painel do nome dela "aparecendo e sumindo rapido demais para
  // escolher alguma coisa" — o relato do issue #11.
  //
  // 400 ms: acima do intervalo de repeticao de qualquer controle e bem abaixo
  // de dois toques deliberados. Vale para os dois sentidos (abrir e fechar),
  // porque o defeito nao distingue: o segundo evento e que sobra.
  if(e->type==SDL_KEYDOWN && !e->key.repeat &&
     (e->key.keysym.sym==SDLK_s || e->key.keysym.scancode==NV_SCANCODE_BLUE) &&
     tela==TELA_HOME && !player_aberto() && !player_mini_ativo() &&
     !detail_aberto() && !vertudo_aberta() && !menu_aberto()) {
    static Uint32 ultimoToque;
    Uint32 agoraTecla = SDL_GetTicks();
    if (agoraTecla - ultimoToque < 400) return;   // repeticao do firmware
    ultimoToque = agoraTecla;
    if(spainel_aberto())spainel_fechar();
    else spainel_abrir();
    return;
  }

  // O EXPLICADOR DE PRIMEIRA VEZ E O MAIS ALTO de todos, depois do login. Ele
  // aparece uma unica vez e faz uma pergunta; qualquer coisa respondendo por
  // baixo dele moveria o foco de uma tela que a pessoa nem esta vendo.
  if (sintro_aberto()) { sintro_evento(e); return; }
  // O cartao de novidades e o outro "primeira vez": mesmo motivo, mesma altura.
  // A PERGUNTA DO PiP vem ANTES dele: ela decide o destino de um video que
  // esta no ar — o Voltar aqui escolhe "fechar o video", nao so fecha cartao.
  if (pipintro_aberto()) { pipintro_evento(e); return; }
  if (novidades_aberto()) { novidades_evento(e); return; }
  // O cartao da 1.1 vem logo depois, e come esquerda/direita como o do Social:
  // sao sete paginas, e deixar a tecla vazar moveria o foco da home debaixo
  // dele. O OK dele AVANCA e so fecha na ultima pagina — ver novidades11.c.
  if (novidades11_aberto()) { novidades11_evento(e); return; }
  if (novidades12_aberto()) { novidades12_evento(e); return; }
  if (novidades13_aberto()) { novidades13_evento(e); return; }
  if (novidades131_aberto()) { novidades131_evento(e); return; }
  if (novidades132_aberto()) { novidades132_evento(e); return; }
  if (novidades133_aberto()) { novidades133_evento(e); return; }
  if (novidades134_aberto()) { novidades134_evento(e); return; }
  if (novidades139_aberto()) { novidades139_evento(e); return; }
  // O DA 1.4.2 VEM ANTES DO DA 1.4, e so um dos dois aparece (ver
  // novidades142.c). "Rodar o diagnostico" abre a tela AQUI, no mesmo evento:
  // deixar para o quadro seguinte daria ao proximo cartao da fila (telemetria)
  // a chance de abrir por cima da tela nova.
  if (novidades142_aberto()) {
    novidades142_evento(e);
    if (novidades142_pediu_diagnostico()) {
      diagDaHome = 1;
      trocarTela(TELA_DIAGNOSTICO);
      menu_definir_destino(MENU_AJUSTES);
    }
    return;
  }
  if (novidades1312_aberto()) { novidades1312_evento(e); return; }
  if (telemetria_aberto()) { telemetria_evento(e); return; }
  // O explicador do Social e da mesma familia, e come esquerda/direita:
  // deixar a tecla vazar para a home moveria o foco dela debaixo do cartao.
  if (recintro_aberto()) { recintro_evento(e); return; }
  if (atualizacao_aberta()) { atualizacao_evento(e); return; }
  if (agendaviso_aberto()) { agendaviso_evento(e); return; }
  if (recomenda_aberta()) { recomenda_evento(e); return; }
  // A MODAL DE RECOMENDAR fica acima do detalhe, do menu do cartaz e do
  // painel da tecla AZUL — as tres portas que a abrem. Abaixo do cartao de
  // aviso, que e uma pergunta sobre outra recomendacao.
  if (recenviar_aberto()) { recenviar_evento(e); return; }

  // A folha de fontes fica acima de tudo: ela e uma pergunta, e enquanto ela
  // esta em pe nada mais deve responder ao D-pad.
  if (faixas_aberta()) { faixas_evento(e); return; }
  if (episodios_aberto()) { episodios_evento(e); return; }
  if (stream_folha_aberta()) { stream_folha_evento(e); return; }
  // O OVERLAY DO GUIA fica acima do player: aberto, ele e quem recebe o D-pad
  // (e o KEYUP do OK longo que marca favorito — por isso esta guarda vem antes
  // do player e nao dentro do switch de telas).
  if (guia_overlay_aberta()) { guia_evento(e); return; }
  // MINI-PLAYER (PiP): o canal segue no canto POR TODA A HOME. Voltar numa
  // tela qualquer so a faz recuar um nivel e a miniatura segue no ar — so na
  // home, sem nada por cima, Voltar nao tem mais para onde ir e ai ele e do
  // PiP, fechando de vez (o gesto do mini-player do YouTube na TV). AZUL
  // devolve a tela cheia sem rebuscar fonte; CH+/- zapeia sem sair do canto.
  //
  // TELA_GUIA (bucket C / PiP no guia): Azul e CH+/- ja caem aqui ANTES de
  // guia_evento — restaurar e zapar com o guia tela cheia aberto. Voltar
  // continua com o guia (sair do guia, PiP segue), como um nivel de navegacao.
  if (player_mini_ativo() && e->type == SDL_KEYDOWN) {
    SDL_Keycode mk = e->key.keysym.sym;
    int msc = e->key.keysym.scancode;
    if ((mk == SDLK_AC_BACK || mk == SDLK_ESCAPE || mk == SDLK_BACKSPACE ||
         mk == SDLK_DELETE || msc == NV_SCANCODE_BACK) &&
        tela == TELA_HOME && !detail_aberto() && !spainel_aberto() &&
        !menu_aberto() && !ctx_aberto() && !vertudo_aberta()) {
      player_fechar_mini(); return;
    }
    if (mk == SDLK_s || msc == NV_SCANCODE_BLUE) { player_restaurar(); return; }
    if (msc == NV_SCANCODE_CH_UP || msc == NV_SCANCODE_CH_DOWN) {
      const char *id = player_id_canal();
      CatItem it;
      if (id[0] && aguardandoFonte != 2 &&
          guia_zap(id, msc == NV_SCANCODE_CH_UP ? 1 : -1, &it)) {
        player_manter_mini();
        tocarCanal(&it);
      }
      return;
    }
  }
  // O PiP continua podendo acompanhar a navegacao, mas a barra nao pode
  // capturar o D-pad por cima dele. Se um canal foi minimizado enquanto a
  // barra estava em animacao, fecha a camada antes de devolver as teclas.
  if (player_mini_ativo() && menu_aberto()) {
    menu_fechar();
    return;
  }
  // A BARRA LATERAL VEM DE QUALQUER TELA (dono, 21/09/2026: "hoje ela so
  // funciona na home"), menos do guia e com o PiP aberto. Cada tela sai com
  // ESQUERDA quando nao ha mais para onde ir a esquerda; aqui, se a saida foi
  // por essa tecla, a home volta JA com a barra aberta em vez de exigir um
  // segundo ESQUERDA. `saiuPorEsquerda` e o que o bloco de sair le.
  if (e->type == SDL_KEYDOWN) saiuPorEsquerda = (e->key.keysym.sym == SDLK_LEFT);
  if (player_aberto()) { player_evento(e); return; }
  if (detail_aberto()) { detail_evento(e); return; }
  if (spainel_aberto()) { spainel_evento(e); return; }
  if (menu_aberto())   { menu_evento(e);   return; }
  // "Ver tudo" fica ENTRE a home e o detalhe: ela cobre a home e o detalhe
  // cobre ela. Por isso vem depois do detalhe e antes do roteamento por tela.
  // O menu do cartaz fica ACIMA de tudo que a home mostra: ele e modal.
  if (ctx_aberto())     { ctx_evento(e);     return; }
  if (vertudo_aberta()) { vertudo_evento(e); return; }

  switch (tela) {
    case TELA_EXPLORAR:   explorar_evento(e);   break;
    case TELA_GUIA:       guia_evento(e);       break;
    case TELA_BUSCA:      busca_evento(e);      break;
    case TELA_BIBLIOTECA: biblioteca_evento(e); break;
    case TELA_AGENDA:     agendaui_evento(e);   break;
    case TELA_PERFIL:     perfil_evento(e);     break;
    case TELA_SOCIAL:     social_evento(e);     break;
    case TELA_ADDONS:     addonsui_evento(e);   break;
    case TELA_AJUSTES:    ajustes_evento(e);    break;
    case TELA_DIAGNOSTICO: diagnostico_evento(e); break;
    default:              home_evento(e);       break;
  }

  // O menu abre AQUI, no mesmo evento que o pediu, e nao no proximo
  // app_atualizar. Diferido por um quadro, as teclas que vierem logo depois do
  // ESQUERDA — e num controle elas vem — sao entregues a tela de tras, que
  // ainda acha que e a dona do foco.
  if (tela == TELA_HOME && home_pediu_menu() && sidebar_permitida()) {
    menu_abrir();
    saiuPorEsquerda = 0;
  }
}

// A tela de detalhe pode pedir para abrir OUTRO titulo (um credito da
// filmografia de um ator, um item de "Mais como este"). Quem troca e aqui, e
// nao ela: reabrir a si mesma no meio do proprio desenho e o tipo de coisa que
// quebra em silencio, e o roteador ja e o unico lugar que sabe abrir titulo.
// O botao do olho: marcar como ASSISTIDO. Grava progresso cheio no arquivo do
// app e avisa o Trakt, que e a fonte que o dono usa nos outros aparelhos. Fica
// no roteador pelo mesmo motivo de tudo mais: e ele que conhece catalogo e
// Trakt, e a tela de detalhe nao precisa conhecer nenhum dos dois.
extern void cat_historico_definir_id(const char *imdb, const char *tipo, int visto);
static void marcarAssistidoSeSolicitado(void) {
  const CatItem *c;
  int i;
  if (!detail_pediu_assistido()) return;
  i = detail_indice();
  c = cat_item(i);
  if (!c) return;
  // ALTERNA, e manda para o HISTORICO do Trakt.
  //
  // Estava chamando trakt_marcar (que e /scrobble/pause) com duracao 1.0 — e
  // aquela funcao comeca com `durSeg <= 1.0 -> return`. O botao mudava so o
  // espelho local e o Trakt NUNCA era informado: parecia funcionar e nao
  // funcionava. Agora vai por /sync/history, que e o endpoint de "assisti".
  //
  // E alterna em vez de so marcar: o icone ja mostra os dois estados, entao um
  // botao que so soma nao teria como desfazer um toque errado.
  // Antes: cat_salvar_progresso(i, ..., 1.0). A guarda `durSeg <= 1.0` daquela
  // funcao devolvia antes de gravar — o espelho local nunca mudava, so o Trakt.
  // Sem duracao conhecida do item, usa-se uma hora inteira como sentinela: o
  // que importa e a porcentagem (100% ou 0%), e e isso que sync e fileira leem.
  { int visto = (c->progresso >= 90);
    const double dur = 3600.0;
    cat_salvar_progresso(i, visto ? 0.0 : dur, dur);
    if (c->imdb[0]) {
      trakt_assistido(c->imdb, !visto);
      // SIMKL E CONTA NUVIO tambem (visto.c), cada um se vinculado. O Trakt
      // segue pela linha acima, que nao mudou; sem ele a unica marca era o
      // progresso de 100% (que a conta ja entende como concluido), e o
      // historico do titulo — o que o menu do cartaz le para dizer "Desmarcar"
      // — nunca era escrito. Com Trakt quem escreve o historico e o 2xx dele.
      visto_titulo(c->imdb, c->tipo, c->temporadas, c->nTemporadas, !visto,
                   visto_destinos());
      if (!trakt_ativo()) cat_historico_definir_id(c->imdb, c->tipo, !visto);
    }
    printf("[app] assistido %s: %s\n", visto ? "desmarcado" : "marcado",
           c->titulo); fflush(stdout); }
}

static void trocaDeTituloSeSolicitada(void) {
  int alvo = detail_pediu_abrir();
  if (alvo >= 0) { abrirPorIndice(alvo); return; }
  // Titulo que veio de FORA do catalogo: a descoberta buscou o meta num fio e
  // avisa aqui quando ele entrou. Abrir no fio da rede seria mexer na tela de
  // outro fio; este e o unico lugar que abre titulo.
  { int novo = desc_titulo_pronto();
    // Pedido de dentro do PLAYER (relacionado que nao esta no catalogo): o
    // detalhe abria por baixo do video e ninguem via — "clico e nao faz nada".
    // O caminho do titulo que JA esta no catalogo (posplay_pediu_titulo)
    // encerra o player antes; este faz o mesmo.
    if (novo >= 0) { if (player_aberto()) player_encerrar(); abrirPorIndice(novo); } }
}

void app_atualizar(float dt, Uint32 agora) {
  // Animacoes reduzidas valem para TODA mola e rampa do app (anim.h), nao so
  // para as telas que lembravam de perguntar. Uma leitura por quadro.
  anim_politica_reduzida = ajustes_animacoes_reduzidas();
  diagnostico_intro_atualizar(dt, agora);
  cancelarFonteSeSaiu();
  processarFonteJob();
  // A QUALIDADE DA IMAGEM CHEGA AOS DOIS MODULOS QUE A CONSOMEM, e so quando
  // muda. tex_cache e artehero nao incluem ajustes.h de proposito: o cache de
  // texturas e a politica de url nao tem por que saber que existe uma tela de
  // Ajustes — quem sabe as duas coisas e o roteador.
  { static int qualAplicada = -1;
    int q = ajustes_qualidade_imagem();
    if (q != qualAplicada) {
      qualAplicada = q;
      tex_qualidade(q);
      artehero_qualidade(q);
    } }
  // O backend precisa progredir mesmo no login, perfis e transicoes que
  // retornam cedo: seek pendente no Tizen e prazo de recuo DV no webOS.
  video_bombear();
  if (tela == TELA_LOGIN) {
    login_atualizar(dt, agora);
    // A troca so acontece AQUI, quando a sessao existe de verdade — nao no
    // instante em que o servidor respondeu. Assim a home nunca abre com uma
    // sessao pela metade.
    if (login_concluido()) {
      // Logo apos entrar, o primeiro ciclo de sync: e ele que descobre quantos
      // perfis a conta tem, e sem isso a tela de escolha nao teria o que
      // mostrar.
      sync_reaplicar_ajustes();
      sync_iniciar();
      tela = TELA_ESCOLHA_PERFIL;
      perfilAntes = perfis_ativo();
      perfilsel_iniciar();
      menu_definir_destino(MENU_INICIO);
    }
    return;
  }

  // Sessao perdida no meio do uso (renovacao recusada): voltar ao login e a
  // unica saida honesta. Continuar na home mostraria o catalogo de exemplo do
  // pacote como se fosse o da pessoa.
  if (!sessao_logada()) {
    invalidarPerfil();
    tela = TELA_LOGIN;
    login_iniciar();
    return;
  }

  // Os vinculos de Trakt e Simkl avancam TODO quadro, em qualquer tela: os dois
  // fazem poll. MEDIDO na TV: estas duas linhas viviam dentro do `if` da tela
  // de perfil (a indentacao enganava), entao o poll so rodava ali — em Ajustes,
  // onde o vinculo e feito, "Aguardando a autorizacao" nunca saia do lugar
  // mesmo com a pessoa ja tendo autorizado no celular.
  traktauth_passo((unsigned)agora);
  simklauth_passo((unsigned)agora);

  if (tela == TELA_ESCOLHA_PERFIL) {
    sync_passo((unsigned)agora);
    // A HOME CONTINUA SE MONTANDO POR TRAS DA PERGUNTA.
    //
    // home_atualizar() comeca com sincronizarFileiras(), que e quem transforma
    // o catalogo em fileiras. Sem esta linha, o catalogo que chega do cache e
    // da rede enquanto a tela de escolha esta em pe ficaria parado, e a home so
    // comecaria a montar DEPOIS da escolha — a pergunta cobraria do arranque os
    // segundos que o cache de 844 ms existe para economizar. Custa uma
    // atualizacao sem desenho: a tela de escolha e opaca e a home nao e pintada
    // aqui (ver desenharTelas).
    home_atualizar(dt, agora);
    perfilsel_atualizar(dt, agora);
    if (perfilsel_pediu_repetir()) { sync_iniciar(); return; }
    if (perfilsel_quer_sair()) {
      // Voltar = "segue com quem ja estava". So vale quando ha uma escolha
      // gravada e ela nao esta atras de um PIN — senao o Voltar entraria no
      // perfil 1 por acidente, ou seria a chave da fechadura.
      if (perfis_pode_dispensar()) {
        perfis_manter_ativo();
        perfilsel_continuar_ativo();
        // A tela de escolha pode ter parado o primeiro ciclo enquanto ainda
        // aguardava uma resposta. Idempotente quando ele ja esta rodando.
        sync_iniciar();
        tela = TELA_HOME;
        menu_definir_destino(MENU_INICIO);
      }
      return;
    }
    if (perfilsel_concluido()) {
      // O perfil mudou o destino do sync: rodar de novo traz os addons e o
      // progresso DESTE perfil, e nao os do perfil anterior que o primeiro
      // ciclo pegou. So quando MUDOU: confirmar o mesmo perfil e o caso comum
      // agora que a tela abre a cada arranque, e recarregar tudo ali seria
      // cobrar o preco de uma troca em toda abertura do app.
      // O CICLO PRECISA CONTINUAR MESMO QUANDO O PERFIL NAO MUDOU, e foi isso
      // que eu errei ao economizar aqui.
      //
      // rodar() PARA depois de perfis_puxar() quando ha escolha pendente — o
      // sync fica interrompido justamente esperando esta tela. Se confirmar o
      // mesmo perfil nao reinicia nada, o ciclo nunca termina e a conta NAO
      // SINCRONIZA a sessao inteira: os addons, o Trakt e o progresso ficam nos
      // do cache da abertura anterior. Medido na TV do dono: ele acrescentou um
      // addon na conta, confirmou o mesmo perfil, e o app seguiu listando os 4
      // addons antigos, com "[sync] ciclo interrompido" como ultima linha.
      // Confirmar o mesmo perfil e o caso COMUM, entao o defeito valia para
      // quase toda abertura de quem tem mais de um perfil.
      //
      // O que continua condicionado a troca e so o CARO: invalidarPerfil()
      // joga fora o que ja foi carregado, e reaplicar ajustes so faz sentido
      // quando o destino mudou.
      if (perfis_ativo() != perfilAntes) {
        invalidarPerfil();
        sync_reaplicar_ajustes();
        // E A FILEIRA DE CONTINUAR, que invalidarPerfil() nao alcanca.
        //
        // invalidarPerfil() so zera a tela de Perfil/Stats. Quem refaz o
        // "Continuar assistindo" e desc_refazer_continuar(), chamada em apenas
        // dois lugares: o fim de uma reproducao (player.c) e sync.c, sob
        // `if (syncprog_aplicar(NULL) > 0)`. E ai esta o defeito: progresso.txt
        // guarda TODOS os perfis num arquivo so, e prog_aplicar_remoto devolve
        // 0 quando o registro que veio nao e mais novo que o que ja esta em
        // disco. Voltar para um perfil JA SINCRONIZADO nao traz novidade
        // nenhuma -> 0 -> a fileira nunca era refeita e continuava mostrando a
        // do perfil anterior. Relatado por um testador na exp.4: abriu no
        // perfil 2, trocou para o 1, e o "Continuar assistindo" seguiu sendo o
        // do 2.
        //
        // Aqui NAO se pergunta se o sync trouxe algo: o perfil mudou, e isso
        // por si so ja torna a fileira na tela a fileira errada. A leitura em
        // si sempre esteve certa (prog_ler filtra por perfis_ativo()); o que
        // faltava era o pedido de refazer.
        desc_refazer_continuar();
      }
      // O TRAKT E O SIMKL TAMBEM SAO DO PERFIL. O vinculo local era um so por
      // aparelho e o perfil 2 seguia com o Trakt do 1 — watchlist, historico,
      // "continuar" e as fileiras do Trakt (relato do dono na C9). Aqui a
      // credencial do perfil anterior sai da memoria e entra a deste, ou
      // nenhuma; a da conta, se o perfil tiver, chega pelo sync_iniciar abaixo.
      // Quando algum dos dois estava ou ficou ligado, as fileiras na tela sao do
      // perfil errado e a home inteira e remontada — desc_refazer_continuar so
      // refaz o "Continuar", e a watchlist e as listas do Trakt ficariam.
      //
      // FORA do `if` acima de proposito: as duas comparam com o perfil DELAS, nao
      // com perfilAntes, e devolvem 0 sem mexer em nada quando ja estao certas.
      { int tk = traktauth_trocar_perfil(perfis_ativo());
        int sk = simklauth_trocar_perfil(perfis_ativo());
        if (sk) simkl_esquecer();
        if (tk || sk) desc_repetir(); }
      sync_iniciar();
      tela = TELA_HOME;
    }
    return;
  }

  // Um ciclo por vez, e so quando a conta existe. O passo e barato: sem fio
  // terminado ele nao faz nada.
  sync_passo((unsigned)agora);

  // A REDE DESCOBRIU PERFIS QUE O CACHE NAO TINHA: perguntar mesmo assim.
  //
  // app_iniciar ja faz esta pergunta com o cache em disco, e e por ali que ela
  // passa em toda abertura depois da primeira. Esta segunda porta cobre o caso
  // que o cache nao cobre: a PRIMEIRA vez que a conta e usada neste aparelho
  // (ou a primeira depois de um "sair"), quando quem descobre que existem dois
  // perfis e o sync, segundos depois. Sem ela o app assumiria o perfil 1 para
  // sempre nessa primeira sessao.
  if (tela == TELA_HOME && !player_aberto() && !detail_aberto() &&
      perfis_precisa_escolher()) {
    tela = TELA_ESCOLHA_PERFIL;
    perfilAntes = perfis_ativo();
    perfilsel_iniciar();
    return;
  }
  // AVISO DE PRIMEIRA EXECUCAO, e AQUI e nao antes.
  //
  // Ele ensina a tecla vermelha, e o lugar errado para isso e a frente do
  // login: la a pessoa esta tentando enquadrar um QR com o celular, e um cartao
  // por cima atrapalha a unica coisa que ela precisa fazer. Depois da escolha
  // de perfil, na primeira vez que a home aparece de verdade (homePronta, sem
  // player nem detalhe por cima), o cartao nao disputa com nada.
  //
  // Chamar em todo quadro nao custa: a decisao acontece uma vez e o modulo a
  // guarda — a leitura do arquivo de bandeira nao se repete.
  if (tela == TELA_HOME && homePronta && !player_aberto() && !detail_aberto()) {
    // Esta e a primeira explicacao da versao: aparece antes dos demais
    // cartoes de onboarding. Depois de OK, o bloco abaixo continua a fila
    // antiga no quadro seguinte.
    // Com o cartao da 1.4.2 ainda por vir, a apresentacao espera: ele mesmo
    // convida para o diagnostico e, ao fechar, dispensa a apresentacao nesta
    // sessao (novidades142.c) — dois avisos seguidos sobre a mesma coisa.
    if (!novidades142_pendente()) diagnostico_intro_primeira_vez();
    if (!diagnostico_intro_aberto()) {
    registro_aviso_primeira_vez();
    // Mesmo lugar e mesma razao: o explicador de "Salvos" fala de uma tecla do
    // controle, e ensinar tecla enquanto a pessoa enquadra um QR no celular nao
    // ensina nada. Ele tambem espera o cartao do log sair — dois cartoes de
    // primeira vez ao mesmo tempo seria um em cima do outro.
    if (!registro_aberto()) sintro_primeira_vez();
    // O cartao do Guia de TV espera o de Salvos sair — dois cartoes de
    // primeira vez ao mesmo tempo seria um em cima do outro.
    if (!registro_aberto() && !sintro_aberto()) novidades_primeira_vez();
    // O CARTAO DA 1.1 VEM DEPOIS DO DO GUIA, e nao antes: quem instala esta
    // versao vindo de uma anterior a do Guia recebe os dois, e o do Guia fala
    // de um recurso que a 1.1 pressupoe (o botao AZUL abrindo o guia aparece na
    // pagina dos botoes coloridos). Ler na ordem em que as coisas chegaram e o
    // unico jeito de a segunda explicacao fazer sentido.
    //
    // Ele vem ANTES do aviso de versao nova, e isso importa: o de versao nova
    // fala da PROXIMA atualizacao, e a pagina 5 deste explica como ela se
    // instala. Invertido, a pessoa apertaria "Atualizar agora" antes de saber
    // o que acontece sem o Homebrew Channel.
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !pipintro_aberto())
      novidades11_primeira_vez();
    // O cartao da 1.2 so depois do da 1.1: quem nunca viu nenhum ve na ordem
    // em que as coisas chegaram, e quem ja viu o da 1.1 ve so o da 1.2.
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !pipintro_aberto())
      novidades12_primeira_vez();
    // O da 1.3 depois do da 1.2, pela mesma razao.
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() &&
        !pipintro_aberto())
      novidades13_primeira_vez();
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() &&
        !novidades131_aberto() && !pipintro_aberto())
      novidades131_primeira_vez();
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() &&
        !novidades131_aberto() && !novidades132_aberto() && !pipintro_aberto())
      novidades132_primeira_vez();
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() &&
        !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() && !pipintro_aberto())
      novidades133_primeira_vez();
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() &&
        !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() &&
        !novidades134_aberto() && !pipintro_aberto())
      novidades134_primeira_vez();
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() &&
        !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() &&
        !novidades134_aberto() && !novidades139_aberto() && !pipintro_aberto())
      novidades139_primeira_vez();
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() &&
        !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() &&
        !novidades134_aberto() && !novidades139_aberto() && !novidades142_aberto() &&
        !pipintro_aberto())
      novidades142_primeira_vez();
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() &&
        !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() &&
        !novidades134_aberto() && !novidades139_aberto() && !novidades142_aberto() &&
        !novidades1312_aberto() && !pipintro_aberto())
      novidades1312_primeira_vez();
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() &&
        !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() &&
        !novidades134_aberto() && !novidades139_aberto() && !novidades1312_aberto() && !novidades142_aberto() && !telemetria_aberto() && !pipintro_aberto())
      telemetria_primeira_vez();
    // AVISO DE VERSAO NOVA: a consulta ao GitHub so parte quando a home esta
    // de pe (nao disputa a rede com o catalogo), e o cartao so abre quando
    // nenhum outro cartao de primeira vez esta aberto.
    atualizacao_verificar();
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() && !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() && !novidades142_aberto() && !pipintro_aberto())
      atualizacao_mostrar_se_houver();
    // RECOMENDACAO DE UM AMIGO: a sondagem parte daqui pelo mesmo motivo que a
    // do GitHub — com a home de pe ela nao disputa a rede com o catalogo. Sem
    // NUVIO_REC_URL compilada as duas chamadas sao no-op e nenhuma conexao
    // abre. O cartao obedece as MESMAS guardas dos outros, mais a do proprio
    // aviso de versao: dois cartoes ao mesmo tempo seria um por cima do outro.
#ifndef NV_LEVE
    recomenda_verificar();
#endif
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() && !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() && !novidades134_aberto() && !novidades139_aberto() && !novidades1312_aberto() && !novidades142_aberto() && !telemetria_aberto() && !pipintro_aberto() && !atualizacao_aberta())
      recomenda_mostrar_se_houver();
    // EXPLICADOR DAS TELAS SOCIAIS: mesmas guardas de todos os outros, mais
    // a do cartao de recomendacao recebida — dois cartoes ao mesmo tempo
    // seria um por cima do outro. Ele proprio nao abre num pacote sem
    // NUVIO_REC_URL (recomenda_ativo), e por isso nao ha guarda aqui: um
    // anuncio de recurso que nao esta no pacote e pior que silencio.
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() && !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() && !novidades134_aberto() && !novidades139_aberto() && !novidades1312_aberto() && !novidades142_aberto() && !telemetria_aberto() && !pipintro_aberto() && !atualizacao_aberta() &&
        !recomenda_aberta())
      recintro_primeira_vez();
    // LEMBRETE VENCIDO: o unico aviso que esta TV consegue dar. Ultimo da fila
    // pela mesma razao de todos os outros — dois cartoes juntos seria um por
    // cima do outro —, e sem consulta de rede nenhuma: o que ele mostra ja
    // esta em disco desde que o dono apertou "Lembrar-me".
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() && !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() && !novidades134_aberto() && !novidades139_aberto() && !novidades1312_aberto() && !novidades142_aberto() && !telemetria_aberto() && !pipintro_aberto() && !atualizacao_aberta() &&
        !recomenda_aberta() && !recintro_aberto())
      agendaviso_mostrar_se_houver();
    // O CARTAO DO CRASH, depois do lembrete e pelas mesmas regras: um cartao
    // por vez, com a home de pe.
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() && !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() && !novidades134_aberto() && !novidades139_aberto() && !novidades1312_aberto() && !novidades142_aberto() && !telemetria_aberto() && !pipintro_aberto() && !atualizacao_aberta() &&
        !recomenda_aberta() && !recintro_aberto() && !agendaviso_aberto())
      avisos_mostrar_se_houver();
    }
  }

  // E o ciclo automatico — nunca com o player aberto: rajada de HTTP no meio
  // do video disputa CPU e rede com o decodificador.
#ifndef NV_LEVE
  if (!player_aberto()) sync_periodico((unsigned)agora);
#endif
  // "CONTINUAR ASSISTINDO" ENVELHECE (issue #66, "it's not updating"). A
  // fileira so era refeita no arranque, ao sair do player e na troca de
  // perfil; quem assiste no celular via a TV parada no que tinha de manha. A
  // cada 10 min na home, sem player e sem detalhe, so ESSA fileira e refeita
  // (desc_refazer_continuar: playback + historico do Trakt e progresso da
  // conta, num fio; cat_trocar_continuar so mexe nela). Nao e o ciclo inteiro
  // de descoberta.
  { static Uint32 ultContinuar;
    if (!ultContinuar) ultContinuar = agora;
    if (tela == TELA_HOME && homePronta && !player_aberto() && !player_mini_ativo() &&
        !detail_aberto() && agora - ultContinuar >= 10u * 60u * 1000u) {
      ultContinuar = agora;
      printf("[home] 10 min na home: refazendo Continuar assistindo\n"); fflush(stdout);
      desc_refazer_continuar();
    } }

  // A LISTA LOCAL TEM DE SOBREVIVER A REPUBLICACAO DO CATALOGO.
  //
  // cat_definir_tudo apaga `naLista` de todo item, e a descoberta o chama
  // varias vezes por ciclo. Sem esta linha o titulo salvo aparecia por alguns
  // segundos e sumia sozinho — exatamente o defeito que contalib_reconciliar ja
  // tinha resolvido para a biblioteca da conta (a chamada dele vive em sync.c,
  // no fim do ciclo; esta precisa ser por quadro porque a lista local tambem
  // muda por acao do dono, e nao so quando o sync termina).
  salvos_reconciliar();

  // Durante a verificacao nao substituir a lista que os workers consultam.
  if (aguardandoFonte != 2) addons_estado();
  trocaDeTituloSeSolicitada();
  marcarAssistidoSeSolicitado();
  if (atomic_load_explicit(&perfilCarga, memory_order_acquire) == 2) {
    PerfilDados snapshot; int sucesso;
    pthread_mutex_lock(&perfilTrava); snapshot=perfilPendente; sucesso=perfilSucesso; pthread_mutex_unlock(&perfilTrava);
    if (sucesso) perfil_definir_dados(&snapshot);
    else if (!trakt_ativo()) perfil_definir_estado(PERFIL_ESTADO_DESCONECTADO,
                                                    "Trakt desconectado. Vincule a conta para ver seu perfil.");
    else perfil_definir_estado(PERFIL_ESTADO_INDISPONIVEL,
                               "Perfil indisponível. O último resumo continua seguro, se houver.");
    atomic_store_explicit(&perfilCarga, 0, memory_order_release);
  }
  if (tela == TELA_PERFIL && perfil_pediu_atualizar()) pedirPerfil();
  // Titulo escolhido no painel de Salvos. Ele entrega o IMDb e nao um indice:
  // a lista dele inclui titulos que NAO estao no catalogo (vieram do arquivo
  // local, ver salvospainel.c), e um indice para esses nao existe. Sem item no
  // catalogo, pede o titulo a descoberta — o mesmo caminho que perfil.c e
  // social.c ja usam para um id que so eles conhecem.
  // Acoes pedidas pela central de avisos: abrir Salvos (recomendacao) ou o
  // cartao de atualizacao. O titulo (agenda) segue pelo contrato de baixo.
  { int c = avisos_pediu();
    if (c == AVISOS_ABRIR_SALVOS && !spainel_aberto()) spainel_abrir();
    else if (c == AVISOS_ABRIR_ATUALIZACAO) atualizacao_abrir(); }
  if (!detail_aberto() && !player_aberto()) {
    const char *alvo = spainel_pediu_abrir();
    if (!alvo) alvo = recomenda_pediu_abrir();   // mesmo contrato, outra origem
    if (!alvo) alvo = avisos_pediu_abrir();
    if (!alvo && tela == TELA_AGENDA) alvo = agendaui_pediu_abrir();
    if (!alvo && abrirTeste[0]) { alvo = abrirTeste; abrirTeste[0] = 0; }
    if (alvo && alvo[0]) {
      int k = cat_indice_por_imdb(alvo);
      if (k >= 0) abrirPorIndice(k); else desc_pedir_titulo(alvo);
    }
  }

  if (tela==TELA_HOME) {
    CatItem pessoa;
    if(home_pediu_pessoa_social(&pessoa)) {
      social_abrir(&pessoa);trocarTela(TELA_SOCIAL);
    }
  }
  if (tela==TELA_HOME && home_pediu_social()) {
    trocarTela(TELA_AJUSTES);menu_definir_destino(MENU_AJUSTES);
  }
  // "Ver tudo" de uma fileira de canais (ou OK num cartao de canal) abre o
  // GUIA — o canal nao tem grade de cartazes para mostrar; tem programacao.
  { char gid[80] = "";
    if (tela == TELA_HOME && home_pediu_guia(gid, sizeof gid)) {
      trocarTela(TELA_GUIA);
      menu_definir_destino(MENU_GUIA);
      if (gid[0]) guia_focar_id(gid);
    } }
  // A lista de addons foi aberta DE Ajustes, entao o Back dela volta para
  // Ajustes. Cair na home aqui faria a pessoa refazer o caminho inteiro so
  // para ligar dois addons seguidos.
  if (tela == TELA_ADDONS && addonsui_quer_sair() && saiuPorEsquerda &&
      sidebar_permitida()) {
    saiuPorEsquerda = 0;
    menu_abrir();
  } else if (tela == TELA_ADDONS && addonsui_quer_sair()) {
    trocarTela(TELA_AJUSTES); menu_definir_destino(MENU_AJUSTES);
  }
  if (tela == TELA_AJUSTES && ajustes_pediu_addons()) {
    addonsui_abrir();
    trocarTela(TELA_ADDONS);
  }
  if (tela == TELA_AJUSTES && ajustes_pediu_diagnostico()) {
    diagDaHome = 0;
    trocarTela(TELA_DIAGNOSTICO);
  }
  // O diagnóstico nasce em Ajustes; voltar deve devolver a pessoa ao mesmo
  // ponto do menu para que ela possa conferir ou alterar o restante do perfil.
  // Aberto pelo cartao da 1.4.2 (diagDaHome), o Voltar devolve a home, de
  // onde a pessoa veio.
  if (tela == TELA_DIAGNOSTICO && diagnostico_quer_sair()) {
    if (diagDaHome) {
      trocarTela(TELA_HOME);
      menu_definir_destino(MENU_INICIO);
    } else {
      trocarTela(TELA_AJUSTES);
      menu_definir_destino(MENU_AJUSTES);
    }
    diagDaHome = 0;
  }

  // Fora da home, o Back tem para onde voltar: a home. So nela ele fecha o app.
  if (tela != TELA_HOME) {
    int fechar = (tela == TELA_EXPLORAR   && explorar_quer_sair())
              || (tela == TELA_GUIA       && guia_quer_sair())
              || (tela == TELA_BUSCA      && busca_quer_sair())
              || (tela == TELA_BIBLIOTECA && biblioteca_quer_sair())
              || (tela == TELA_AGENDA     && agendaui_quer_sair())
              || (tela == TELA_PERFIL      && perfil_quer_sair())
              || (tela == TELA_SOCIAL      && social_quer_sair())
              || (tela == TELA_AJUSTES    && ajustes_quer_sair())
              || (tela == TELA_DIAGNOSTICO && diagnostico_quer_sair());
    if (fechar) {
      // ESQUERDA na borda abre a barra SOBRE a tela atual. Voltar continua
      // sendo a saída normal para a Home; assim o foco da tela não é perdido
      // só para alcançar a navegação principal.
      if (saiuPorEsquerda && sidebar_permitida()) {
        menu_abrir();
      } else {
        trocarTela(TELA_HOME);
        menu_definir_destino(MENU_INICIO);
      }
      saiuPorEsquerda = 0;
    }
  } else if (home_quer_sair()) {
    sair = 1;
  }

  // Trocar de usuario, pedido pelo rodape da barra lateral. Vem ANTES do
  // destino: as duas coisas saem do mesmo menu, e quem pediu troca nao quer
  // mudar de aba.
  if (menu_pediu_trocar()) {
    invalidarPerfil();
    tela = TELA_ESCOLHA_PERFIL;
    perfilAntes = perfis_ativo();
    perfilsel_iniciar();
    return;
  }

  if (menu_mudou_destino()) {
    switch (menu_destino()) {
      case MENU_GUIA:
        // PiP ativo: overlay sobre o mini (recomendacao backlog C) em vez da
        // tela cheia opaca — zap/Azul/Voltar do PiP continuam coerentes.
        if (player_mini_ativo()) {
          const char *id = player_id_canal();
          guia_overlay_abrir();
          if (id[0]) guia_focar_id(id);
        } else {
          trocarTela(TELA_GUIA);
        }
        break;
      case MENU_EXPLORAR:   trocarTela(TELA_EXPLORAR);   break;
      case MENU_BUSCAR:     trocarTela(TELA_BUSCA);      break;
      case MENU_BIBLIOTECA: trocarTela(TELA_BIBLIOTECA); break;
      case MENU_AGENDA:     trocarTela(TELA_AGENDA);     break;
      // DIRETO PARA A TELA, e nao mais para o painel lateral. O item do menu
      // se chama "Perfil e Stats" e abria um resumo de tres botoes por cima da
      // home — para ver as estatisticas de verdade era preciso descer ate "Ver
      // perfil completo" e apertar OK, dois passos para chegar onde o rotulo ja
      // prometia. trocarTela(TELA_PERFIL) ja chama perfil_abrir + pedirPerfil.
      case MENU_PERFIL:     trocarTela(TELA_PERFIL);     break;
      case MENU_AJUSTES:    trocarTela(TELA_AJUSTES);    break;
      default:              trocarTela(TELA_HOME);       break;
    }
  }
  // Pedidos de abrir um titulo, vindos de qualquer tela.
  if (!detail_aberto() && !player_aberto()) {
    int idx = -1;
    HomeItem it;
    if (tela == TELA_HOME && home_pediu_abrir()) {
      if (home_item_focado(&it)) abrirTitulo(&it);
    } else if (tela == TELA_EXPLORAR && explorar_pediu_abrir(&idx)) {
      abrirPorIndice(idx);
    } else if (tela == TELA_HOME && home_pediu_tocar()) {
      // OK no card da retomada com "OK no card" = Retomar (issue #93): abre a
      // pagina do titulo E pede reproducao no mesmo passe. A pagina fica
      // aberta por baixo — Voltar do player cai nela, como o dono espera.
      if (home_item_focado(&it)) {
        // O episodio a tocar e o que o CARD mostrava, nao o que o detalhe
        // adivinharia: para serie o card pode anunciar o PROXIMO episodio
        // (continuar_desenhar troca T/E pelo prox_para_item quando o semeado
        // ja terminou), e episodioAlvo chegaria a outro numero — ou a nenhum,
        // porque a lista de episodios ainda nao chegou da rede.
        const CatItem *cw = cat_item(it.indice);
        cwTocarT = cwTocarE = 0;
        if (cw && !strcmp(cw->tipo, "series")) {
          ProxSugestao prox;
          cwTocarT = cw->temporada; cwTocarE = cw->episodio;
          if (prox_para_item(cw, cat_episodio(it.indice, 0),
                             cat_n_episodios(it.indice),
                             (long long)time(NULL) * 1000LL, &prox)) {
            cwTocarT = prox.temporada; cwTocarE = prox.episodio;
          }
        }
        abrirTitulo(&it);
        detail_pedir_reproduzir();
      }
    } else if (tela == TELA_BUSCA && busca_pediu_abrir(&idx)) {
      if (busca_item_focado(&it)) abrirTitulo(&it); else abrirPorIndice(idx);
    } else if (tela == TELA_BIBLIOTECA && biblioteca_pediu_abrir(&idx)) {
      abrirPorIndice(idx);
    } else if (tela == TELA_PERFIL) {
      PerfilDestaque p;
      if (perfil_item_selecionado(&p) && p.id[0]) {
        idx = cat_indice_por_imdb(p.id);
        if (idx >= 0) abrirPorIndice(idx); else desc_pedir_titulo(p.id);
      }
    } else if (tela == TELA_SOCIAL) {
      SocialItemSelecionado s;
      if (social_item_selecionado(&s) && s.imdb[0]) {
        idx=cat_indice_por_imdb(s.imdb);
        if(idx>=0)abrirPorIndice(idx); else desc_pedir_titulo(s.imdb);
      }
    }
  }

  // Botoes do detalhe: quem sabe que existe player e biblioteca e o roteador,
  // nao a tela de detalhe.
  if (detail_aberto()) {
    // Reproduzir sem escolher = modo automatico: a regra do stream_automatico
    // (MP4 4K Dolby Vision primeiro, senao o primeiro da lista) decide sozinha.
    // Sem lista, o player abre sem video em vez de nao abrir — a tela dizendo
    // que nao ha fonte e melhor que um botao que parece nao responder.
    // Ao abrir um titulo, perguntar as fontes JA — a busca leva segundos e
    // esperar o usuario apertar Reproduzir para so entao comecar faria a
    // primeira reproducao parecer travada.
    // O gatilho e o ID, nao o indice do titulo. Com o indice, mudar de EPISODIO
    // nao repetia a busca e a lista continuava a do episodio anterior — meia
    // correcao seria pior que nenhuma, porque a tela mostraria fontes de um
    // episodio com o nome de outro.
    // E DE NOVO QUANDO A LISTA DE ADDONS MUDA. Medido na C9 (20/09/2026): o
    // titulo aberto 3 s depois do arranque consultava os 4 addons do pacote
    // ("total 0"), a lista da conta chegava aos 5 s com 12, e ninguem repetia
    // a consulta — a folha de fontes ficava vazia ate trocar de titulo.
    { static char ultimoAlvo[32] = "";
      static unsigned ultimaVersao = 0;
      int i = detail_indice();
      const CatItem *ci = cat_item(i);
      char alvo[32];
      idDoAlvo(ci, alvo, sizeof alvo);
      if (!player_aberto() && aguardandoFonte != 2 && ci && ci->imdb[0] &&
          (strcmp(alvo, ultimoAlvo) || (detail_aberto() && ultimaVersao != addons_versao()))) {
        if (!strcmp(alvo, ultimoAlvo)) printf("[addons] lista mudou: refazendo a busca de fontes de %s\n", alvo);
        snprintf(ultimoAlvo, sizeof ultimoAlvo, "%s", alvo);
        ultimaVersao = addons_versao();
        { stream_definir_alvo(alvo); addons_buscar(alvo, ci->tipo); }
        // Episodios do titulo aberto, na temporada onde o dono parou. Sai da
        // rede na hora: guardar a lista de episodios de 40 titulos no pacote
        // envelhecia a cada temporada nova.
        // No FILME o mesmo fio busca o /meta/movie quando o catalogo ainda nao
        // tem elenco: e de la que saem atores, direcao e generos da pagina.
        // Tipo incerto ("anime" do AIOMetadata) tambem: e o /meta que diz se
        // e serie, mesmo quando o catalogo ja trouxe elenco.
        if (!strcmp(ci->tipo, "series") || strcmp(ci->tipo, "movie") ||
            ci->nElenco == 0) desc_episodios(i, 0);
        // Legendas do OpenSubtitles junto: sao dezenas por titulo e a busca
        // leva segundos. Pedir so quando o dono abre a folha de faixas faria
        // ele esperar de olho numa lista vazia.
        addons_buscar_legendas(alvo, ci->tipo);
      } }
    // "Assistir do comeco" (issue #46) segue o MESMO caminho do primario; a
    // unica diferenca e a trava de retomada, armada depois de o episodio ficar
    // definitivo — player_do_inicio sobrevive as re-chamadas tardias de
    // player_definir_episodio.
    int doInicio = detail_pediu_do_inicio();
    if ((detail_pediu_reproduzir() || doInicio) && aguardandoFonte != 2) {
      // A tela abre JA, no estado "abrindo fonte", e a escolha acontece depois.
      // Escolher antes deixaria o botao sem resposta por segundos, e escolher
      // sem verificar entregava o video de aviso do debrid — que toca normal e
      // por isso passa por sucesso.
      const CatItem *ci = cat_item(detail_indice());
      limparFonteVOD();
      player_abrir(detail_indice(), NULL);
      episodioDoDetalhe();
      // CW direto (issue #93): o episodio que o card anunciava vale sobre o
      // que episodioAlvo resolveu — ele e a unica copia fiel do "T/E do card"
      // quando a lista de episodios ainda esta a caminho.
      if (cwTocarT > 0 && cwTocarE > 0) {
        player_definir_episodio(cwTocarT, cwTocarE);
        cwTocarT = cwTocarE = 0;
      }
      if (doInicio) player_do_inicio();
      // O episodio so fica definitivo DEPOIS de abrir o player. Refaça sempre
      // o pedido de legenda nesse ponto; a busca de prefetch pode ter comecado
      // no episodio anteriormente focado e o worker agora troca para o pedido
      // mais recente sem publicar resultados velhos.
      if (ci && ci->imdb[0]) {
        char alvoLeg[64]; alvoPlayer(alvoLeg, sizeof alvoLeg);
        addons_buscar_legendas(alvoLeg, ci->tipo);
      }
      // RENOVA POR IDADE **E POR DONO** (issue #101).
      //
      // O alvo tambem mudou: era idDoAlvo(), o episodio EM FOCO na pagina, e
      // quem toca nem sempre e ele — "Retomar" e o card de Continuar assistindo
      // abrem outro episodio (cwTocar/episodioDoDetalhe, logo acima), e a busca
      // saia para um id diferente do que ia reproduzir. alvoPlayer le o
      // episodio ja definitivo, depois de player_abrir.
      //
      // E a condicao deixou de ser so o relogio: a lista podia ter 3 s de vida
      // e ser do episodio anterior, e ai nada a refazia. Idade cobre o link
      // assinado que expira; o dono cobre o episodio errado. Sao duas coisas.
      { char alvoP[64]; alvoPlayer(alvoP, sizeof alvoP);
        if (ci && ci->imdb[0] && alvoP[0] &&
            (stream_idade_ms() > NV_LINK_VALIDO_MS || !stream_lista_do_alvo(alvoP))) {
          printf("fonte: lista com %ums%s, renovando (%s)\n",
                 (unsigned)stream_idade_ms(),
                 stream_lista_do_alvo(alvoP) ? "" : " e de outro alvo", alvoP);
          stream_definir_alvo(alvoP);
          addons_buscar(alvoP, ci->tipo);
        } }
      aguardandoFonte = 1;
    }
    if (detail_pediu_marcar()) {
      // Alterna no Trakt E no espelho local. O estado de partida vem de
      // ci->naLista, que a descoberta preencheu com a watchlist de verdade;
      // sem ele o botao adicionava de novo um titulo que ja estava la.
      int i = detail_indice();
      const CatItem *c = cat_item(i);
      // A INTENCAO E CAPTURADA ANTES DE QUALQUER ESCRITA, e as tres escritas
      // usam o MESMO valor. Antes cada linha relia `c->naLista`, e a segunda
      // ja lia o campo que a primeira tinha mudado — o "+" mandava ao Trakt o
      // oposto do que gravava no espelho local sempre que a ordem mudasse. E a
      // mesma disciplina que ctxmenu.c ja aplica (e que o teste de contrato
      // cobra la).
      int entrar = c ? !c->naLista : 0;
      biblioteca_alternar_lista(i);
      // LOCAL SEMPRE, e primeiro. E o unico destino que sobrevive ao
      // fechamento do app sem depender de conta nenhuma; ver salvos.h. Sem
      // isto, quem nao tem Trakt vinculado apertava "+" e nao guardava nada.
      if (c) salvos_definir(c, entrar);
      // O TRAKT SO SE A PESSOA PEDIU. A escolha vem do explicador de primeira
      // vez e continua em Ajustes › Interface e conta ("Onde o + salva"). O
      // padrao e ligado, entao para quem ja usava o app nada muda.
      if (c && c->imdb[0] && ajustes_salvos_no_trakt())
        trakt_watchlist(c->imdb, entrar);
      // O SIMKL, pelo mesmo criterio (issue #110). Sem vinculo nao sai nada e o
      // log diz — a Biblioteca e o painel de Salvos dizem "Vincule o Simkl em
      // Ajustes" (simkl.h), e o titulo ficou na lista desta TV de qualquer jeito.
      if (c && c->imdb[0] && ajustes_salvos_no_simkl()) {
        if (!simkl_ativo()) printf("[simkl] + sem vinculo: so na lista desta TV\n");
        else simkl_lista_tipo(c->imdb, c->tipo, entrar);
      }
      if (c) cat_definir_na_lista(i, entrar);
    }
    if (detail_pediu_fontes()) {
      // A lembrada e localizada ANTES de abrir, para a folha ja desenhar a
      // marca no primeiro quadro. E so uma varredura da lista em memoria.
      char base[24];
      idBaseDoTitulo(base, sizeof base);
      stream_preferir(base[0] ? fontepref_escolher(base) : -1);
      stream_folha_abrir();
    }
  }
  // Escolher uma fonte na folha inicia a reproducao DELA. Trocar de fonte com o
  // player ja aberto tambem vale: fecha a sessao atual e abre na nova, senao
  // duas ficariam presas no mesmo pipeline.
  // A busca disparada por Reproduzir terminou: agora VERIFICA as fontes, em
  // ordem, ate achar uma que leve ao arquivo — e so entao liga o video.
  if (aguardandoFonte == 1 && addons_estado() != ADD_BUSCANDO) {
    unsigned geracao = novaGeracaoFonte();
    aguardandoFonte = 2;
    fontePedidoGeracao = geracao;
    limparFontePendente();
    fonteEscolhida = -2;
    if (player_id_canal()[0]) {
      // Canal: a playlist de cada candidata e conferida em paralelo (custa
      // meio segundo por fonte morta) e entra a primeira VIVA, na ordem do
      // addon. Sem isto entrava a primeira da lista sem conferir, e cada morta
      // custava os 12 s do watchdog abaixo — medido: 12, 24, 36, 48, 60 s num
      // canal com seis mortas, quase dois minutos ate tocar.
      //
      // Nenhuma viva NAO e motivo para desistir: pode ter sido lentidao de
      // rede, e a primeira da lista com o watchdog de sempre e melhor que uma
      // tela de erro. Por isso o fallback.
      if (xtream_e_id(player_id_canal())) {
        fonteEscolhida = resolverCanalXtream();
        canalFontePrazo = CANAL_FONTE_PRAZO_MS;
      } else if (stalker_e_id(player_id_canal())) {
        // UMA fonte, e ela acabou de nascer: nao ha lista para conferir nem
        // ranking para aplicar. O create_link bloqueia centenas de ms, entao
        // captura o id e deixa a rede fora do fio de desenho. Sem `return` de
        // proposito — quem liga o video e o bloco abaixo, o mesmo dos outros
        // caminhos.
        if (pedirFonteJob(FJOB_STALKER, geracao, player_id_canal(), 0) < 0)
          fonteEscolhida = -1;
        canalFontePrazo = CANAL_FONTE_PRAZO_MS;
      } else {
        int r = pedirFonteJob(FJOB_CANAL, geracao, NULL, 0);
        if (r < 0) fonteEscolhida = stream_automatico();
        canalFontePrazo = CANAL_FONTE_PRAZO_MS;
      }
      canalFonteDesde = SDL_GetTicks();
    } else {
      // A FONTE LEMBRADA DESTE TITULO ENTRA NA FRENTE DA FILA (issues #56/#57).
      //
      // AQUI e nao no botao: a lista so existe depois de os addons
      // responderem, e este e o unico ponto por onde passam TODOS os caminhos
      // que pedem fonte — Reproduzir/Retomar do detalhe, "assistir do comeco",
      // trocar de episodio na folha, e o proximo episodio automatico. Um so
      // lugar e o que impede a correcao de valer em tres telas e faltar na
      // quarta.
      //
      // Nada lembrado devolve -1, stream_preferir(-1) desliga, e a verificacao
      // roda exatamente como rodava antes. Quem nunca abriu a folha de fontes
      // nao ve diferenca nenhuma.
      char base[24];
      int lembrada;
      idBaseDoTitulo(base, sizeof base);
      lembrada = base[0] ? fontepref_escolher(base) : -1;
      stream_preferir(lembrada);
      // QUEM ESCOLHE E A PESSOA, quando ela pediu isso em Ajustes.
      //
      // A folha abre AQUI e nao no botao pelo mesmo motivo que a fonte
      // lembrada e aplicada aqui: antes desta linha a lista nao existe — os
      // addons acabaram de responder. Abrir no botao mostraria uma folha
      // vazia.
      //
      // O ramo do canal fica de fora de proposito (ele esta no `if` acima):
      // uma folha entre um zap e outro e o oposto do que se quer de TV ao
      // vivo, e la a escolha ja e feita pela playlist que responde.
      //
      // ISSUE #62: COM "ESCOLHER A FONTE AO REPRODUZIR" LIGADO, "RETOMAR"
      // ABRIA A FOLHA DE NOVO. A pessoa ja tinha escolhido a fonte deste
      // titulo na folha — e a escolha esta guardada (fontepref) e presente na
      // lista de agora. Perguntar de novo e o que o relator chamou de "o bug
      // antigo": o ajuste quer que a PESSOA escolha, e ela escolheu. So
      // pergunta quando nao ha escolha lembrada que sirva para esta lista;
      // trocar de fonte continua a um hold de distancia (menu do episodio,
      // hold no botao primario).
      if (ajustes_fonte_manual() && stream_n() > 0 && lembrada < 0) {
        aguardandoFonte = 0;
        folhaParaTocar = 1;
        stream_folha_abrir();
      } else if (pedirFonteJob(FJOB_ADDON, geracao, NULL, 0) < 0) {
        aguardandoFonte = 0; player_erro_fonte();
      }
    }
  }
  if (aguardandoFonte == 2 && fonteEscolhida != -2) {
    const Stream *s = fonteEscolhida >= 0 ? stream_item(fonteEscolhida) : NULL;
    aguardandoFonte = 0;
    printf("automatico (verificado): %s\n", s ? s->rotulo : "(nenhuma fonte serve)");
    // A afirmacao de HDR/DV vai ANTES do tocar: e ela que o bind do ACB
    // descreve ao tv.display. Sem isto o C9 exibe tudo mapeado em SDR.
    if (s) video_definir_dv(s->dolbyVision);
    if (s) video_definir_cabecalhos(s->cabecalhos);
    // Anuncia o CONTENTOR pelo mesmo caminho: e o que dispensa a sonda de
    // Matroska num arquivo que nunca teria um cabecalho desses.
    if (s) video_definir_mp4(s->mp4 || strstr(s->url, ".mp4") != NULL);
    marco(s ? "fonte escolhida" : "nenhuma fonte serve");
    // Nenhuma fonte e um debrid recusou a CONTA (403 de plano/limite, registro
    // 1541): diz qual no log, que e onde se separa "o addon nao tinha" de "o
    // TorBox nao deixou". Ver debrid_recusa em debrid.h.
    if (!s) { char rec[64];
      if (debrid_recusa(rec, sizeof rec)) printf("[fonte] o debrid recusou (%s)\n", rec); }
    // PiP conta como sessao viva: o zap dentro da miniatura depende desta
    // fonte chegar — com a guarda antiga ela seria descartada.
    if ((player_aberto() || player_mini_ativo()) && !player_quer_sair()) {
      stream_definir_atual(fonteEscolhida);
      if (s) {
        player_definir_fonte(s->url);
        // Tocou por outra fonte, mas um servico de debrid ficou de fora por
        // conta sem plano: diz uma vez por sessao, sem bloquear nada.
        { int novo = debrid_sem_plano_novo();
          if (novo) player_toast(i18n(debrid_sem_plano_frase(novo)), 7000); }
        if (!player_id_canal()[0]) {
          fonteVODAutomatica = 1;
          fonteVODTentativas++;
          fonteVODDesde = SDL_GetTicks();
        }
        // Armado so em sessao de canal: o indice passa a responder ao
        // watchdog de fonte morta ate a lista acabar ou o canal trocar.
        if (player_id_canal()[0]) { canalFonteIdx = fonteEscolhida; canalFonteDesde = SDL_GetTicks(); }
      }
      // "PRIMEIRA DA LISTA" CONFERE UMA SO (issue #130), entao a conferencia
      // que falha conta como uma tentativa do mesmo orcamento da reproducao
      // que trava: a proxima da lista so e conferida se ainda cabe em "Outra
      // fonte se falhar". No modo "Melhor fonte" a conferencia ja percorreu a
      // fila dela, e aqui e erro como sempre foi.
      else if (!player_id_canal()[0] && ajustes_fonte_primeira() &&
               ++fonteVODTentativas < VOD_FONTE_MAX_TENTATIVAS &&
               stream_automatico() >= 0) {
        printf("[fonte] primeira da lista nao serviu; conferindo a seguinte (%d/%d)\n",
               fonteVODTentativas + 1, VOD_FONTE_MAX_TENTATIVAS);
        (void)pedirProximaFonteVOD();
      }
      else { limparFonteVOD(); erroSemFonte(); }
    }
  }

  // Fonte de canal que nao abre (upstream morto no proxy) nao pode deixar o
  // dono olhando "carregando" para sempre: passa o prazo ou a fonte falhou,
  // entra a proxima na ORDEM DO ADDON — a ordem dele e o ranking dele.
  // Vale no PiP tambem: a miniatura com a fonte morta tenta a proxima.
  // O CARTAO DE ERRO SAI SE O VIDEO VOLTOU A ANDAR — e isto roda TODO quadro
  // com o player aberto, FORA do watchdog de canal abaixo.
  //
  // A primeira versao deste conserto (41962e1) morava dentro do watchdog, que
  // so roda enquanto canalFonteIdx >= 0. Com uma fonte so, o watchdog declara
  // morta, mostra o erro, poe canalFonteIdx = -1 e SAI DO LACO — e o conserto
  // saia junto. MEDIDO na C9 em 18/09 com o dono no controle: loadCompleted em
  // 596 s, primeiro buffer em 622 s (26 s depois), "voltou a entregar" = 0
  // vezes no log, cartao na tela com audio e currentTime andando por tras.
  //
  // Agora a condicao e so "o player esta aberto, marcou erro, e o pipeline
  // esta entregando". Quem entrega desmente o cartao, seja canal ou filme.
  //
  // E O VIDEO TEM DE SER DESTA SESSAO. MEDIDO no registro 1518 (webOS 1.4.0):
  // a busca terminou em "nenhuma fonte serve" — nenhum video aberto — e no
  // mesmo quadro o cartao saiu com "buffer 5596.0s". O buffer era do pipeline
  // compartilhado (o trailer HLS do detalhe acabara de tocar), nao de fonte
  // nenhuma do player. Sem player_tem_video() a pessoa ficava numa tela preta
  // sem erro e sem fonte.
  if ((player_aberto() || player_mini_ativo()) && player_fonte_falhou() &&
      player_tem_video() && video_buffer_fim() > 0.5 && !video_falhou()) {
    printf("[player] a fonte voltou a entregar (buffer %.1fs): tirando o erro da tela\n",
           video_buffer_fim());
    fflush(stdout);
    player_limpar_erro_fonte();
    canalFonteDesde = SDL_GetTicks();
  }
  if (canalFonteIdx >= 0 && (player_aberto() || player_mini_ativo()) &&
      !player_quer_sair() && player_id_canal()[0]) {
    // TRAVOU DEPOIS DE ABRIR conta como morta, e sem isto nao contava.
    //
    // O prazo acima so corre enquanto player_carregando(), que e
    // `esperandoFonte || (comVideo && !video_pronto())` — depois do
    // loadCompleted ele e 0 e o ramo do prazo morre junto. Uma fonte que abre
    // e para de entregar no meio (upstream caindo, link de canal que expira)
    // nao dispara `paused` nem erro: fica imagem congelada com video_falhou()
    // em 0 para sempre, e o watchdog olhava para o outro lado.
    //
    // video_bufferando_ms() e o par bufferingStart/bufferingEnd do uMS, que o
    // video.c ja recebia e so registrava em marco.
    // ABRIR DEVAGAR NAO E MORRER. MEDIDO na C9 em 18/09 (Meu Futebol, HLS ao
    // vivo a 5,8 Mbps): o pipeline encheu 39 s de buffer em ~10 s e SO ENTAO
    // ficou mais 10 s parado antes do primeiro quadro — 20 s da fonte
    // escolhida ate "hdr do pipeline". O prazo de 12 s declarava a fonte
    // morta, mostrava o cartao de erro, e o conserto "voltou a entregar"
    // tirava o cartao logo depois: erro na tela para uma fonte que estava
    // baixando o tempo inteiro. Buffer que ENCHE e prova de vida; o prazo
    // curto so vale enquanto nada chegou. Com dados e sem quadro, vale o teto
    // longo — o uMS que engole 39 s e nao toca em 45 s esta mesmo travado.
    Uint32 desde = SDL_GetTicks() - canalFonteDesde;
    int morta = player_fonte_falhou() || video_falhou() ||
        (player_carregando() && desde > canalFontePrazo && video_buffer_fim() <= 0.5) ||
        (player_carregando() && desde > CANAL_ABRE_TETO_MS) ||
        video_bufferando_ms() > CANAL_TRAVA_MS;
    if (morta) {
      int prox = stream_canal_proxima(canalFonteIdx);
      const Stream *s;
      // PORTAL STALKER: nao existe "proxima fonte" — cada canal tem uma so, e
      // o que morre nao e o canal, e o LINK, que vale minutos. Avancar o indice
      // aqui cairia direto no ramo de desistir, entao o conserto e pedir um
      // link novo para o MESMO canal.
      //
      // Com teto: `stalkerTentativas` impede que um portal fora do ar vire um
      // laco de create_link a cada 12 s para sempre. O teto zera quando o canal
      // troca (canalFonteIdx = -1 em tocarCanal).
      if (stalker_e_id(player_id_canal())) {
        if (stalkerRenovando) return;
        if (stalkerTentativas < CANAL_STALKER_TENTATIVAS) {
          unsigned geracao = novaGeracaoFonte();
          fontePedidoGeracao = geracao;
          fonteEscolhida = -2;
          stalkerRenovando = 1;
          if (pedirFonteJob(FJOB_STALKER, geracao, player_id_canal(), 1) >= 0) {
          // A resposta e aplicada no bloco acima depois do join. Enquanto
          // isso, o watchdog nao dispara outro pedido em paralelo.
          canalFonteIdx = -2;
          canalFonteDesde = SDL_GetTicks();
          printf("[stalker] link do canal expirou; renovando (%d/%d)\n",
                 stalkerTentativas + 1, CANAL_STALKER_TENTATIVAS);
          return;
          }
          stalkerRenovando = 0;
        }
        {
          canalFonteIdx = -1;
          if (player_mini_ativo()) player_fechar_mini();
          else player_erro_fonte();
        }
        return;
      }
      // A MESMA URL DE NOVO NAO E "PROXIMA". MEDIDO na C9 em 19/09 (Fenix TV,
      // canal com 4 fontes, duas com a mesma url de proxy): a fonte travou
      // por 12 s a 600 kbps, o watchdog declarou morta e a "proxima" era a
      // mesma url — recarregou o mesmo fluxo lento e travou de novo. Pula as
      // repetidas; a lista continua na ordem do addon.
      while (prox >= 0 && prox < stream_n()) {
        const Stream *cand = stream_item(prox), *atual = stream_item(canalFonteIdx);
        if (!cand || !atual || strcmp(cand->url, atual->url) != 0) break;
        prox = stream_canal_proxima(prox);
      }
      s = (prox >= 0 && prox < stream_n()) ? stream_item(prox) : NULL;
      if (s) {
        printf("[guia] fonte %d nao abriu; tentando %d\n", canalFonteIdx, prox);
        marco("canal: fonte morta, proxima");
        stream_definir_atual(prox);
        canalFonteIdx = prox; canalFonteDesde = SDL_GetTicks();
        // Prazo pela classe da PROXIMA, nao por "a lista ja falhou uma vez":
        // com todas mudas o curto matava a 4K que abre em 11,8 s (25/09).
        canalFontePrazo = stream_canal_prazo_longo(prox)
                            ? CANAL_FONTE_PRAZO_MS : CANAL_FONTE_PRAZO_MUDA_MS;
        player_definir_fonte(s->url);
      } else {
        canalFonteIdx = -1;
        // Sem mais fonte na lista: na tela cheia vira o erro de sempre; no
        // PiP a miniatura morre quieta em vez de prender um quadro morto.
        if (player_mini_ativo()) player_fechar_mini();
        else player_erro_fonte();
      }
    }
  }

  tentarProximaFonteVOD();
  processarTorrentJob();

  int fonte;
  if (aguardandoFonte != 2 && stream_folha_escolheu(&fonte)) {
    const Stream *s = stream_item(fonte);
    folhaParaTocar = 0;
    printf("fonte escolhida: %s\n", s ? s->rotulo : "?");
    // GUARDAR A ESCOLHA, e SO a manual. O que o automatico escolhe nao vira
    // preferencia: quem nunca abriu esta folha continua com a regra da
    // pontuacao para sempre, que e a condicao de nao quebrar o app de quem
    // nunca escolheu nada.
    if (s) {
      char base[24];
      idBaseDoTitulo(base, sizeof base);
      if (base[0]) fontepref_guardar(base, s);
      // A MARCA DA FOLHA SEGUE A ESCOLHA NOVA. Sem esta linha, reabrir a folha
      // logo depois de trocar de fonte mostraria "Sua escolha anterior" na
      // fonte antiga — a tela contradizendo o que a pessoa acabou de fazer.
      stream_preferir(fonte);
    }
    if (s) video_definir_dv(s->dolbyVision);
    if (s) video_definir_cabecalhos(s->cabecalhos);
    if (s) {
      aguardandoFonte=0;
      limparFonteVOD();
      int titulo=player_aberto()?player_indice():detail_indice(), t=0,e=0;
      if (player_aberto()) { player_episodio_atual(&t,&e); player_encerrar(); }
      else detail_ep_foco(&t,&e);
      player_abrir(titulo,NULL);
      player_definir_episodio(t,e);
      stream_definir_atual(fonte);
      // Qualquer escolha nova invalida a resposta de um torrent anterior.
      torrentSessao++;
      if (!s->url[0] && s->infoHash[0]) {
        pedirTorrentEscolhido(fonte);
      } else {
        player_definir_fonte(s->url);
        // FONTE QUE O ADDON MARCA COMO FORA DE CACHE ("⏳", "[TB download]"):
        // tocar o link e o que manda o servico baixar, e o que o addon devolve
        // enquanto baixa e um clipe de aviso de ~8 s (registros 1136, 2191,
        // 2501). O clipe fecha o player sem nada na tela; o aviso diz o que
        // esta acontecendo e o que fazer.
        if (s->foraCache)
          player_toast(i18n("Fonte fora do cache: o debrid começa a baixar. Se tocar um aviso curto, tente de novo em alguns minutos."), 9000);
      }
      // Escolha manual num canal tambem entra no watchdog: fonte viva escolhida
      // a dedo pode morrer igual.
      if (player_id_canal()[0]) { canalFonteIdx = fonte; canalFonteDesde = SDL_GetTicks(); }
    }
  }
  if (aguardandoFonte != 2 && player_pediu_fontes()) {
    char base[24];
    stream_folha_contexto(player_linha_episodio());
    idBaseDoTitulo(base, sizeof base);
    stream_preferir(base[0] ? fontepref_escolher(base) : -1);
    stream_folha_abrir();
  }

  // CANAL ESCOLHIDO NO GUIA — tela cheia ou overlay, mesma acao: toca direto.
  { CatItem it;
    if (aguardandoFonte != 2 && guia_pediu_canal(&it)) tocarCanal(&it); }

  // CH+/- COM CANAL NO AR: zap na ordem do guia. A lista pode ainda nao ter
  // sido carregada (guia nunca aberto nesta sessao): a primeira tecla dispara
  // a carga e nao troca nada, a seguinte ja zapeia.
  { int dir = player_pediu_zap();
    if (dir && player_aberto() && aguardandoFonte != 2) {
      const char *id = player_id_canal();
      CatItem it;
      if (id[0] && guia_zap(id, dir, &it)) tocarCanal(&it);
      else guia_carregar();
    } }

  // BAIXO/AZUL COM CANAL NO AR: o overlay do guia abre focado no canal que
  // esta tocando. player_id_canal e o id congelado na abertura — o indice no
  // catalogo pode ja ter sido remapeado por uma republicacao da descoberta.
  if (player_pediu_guia() && player_aberto()) {
    const char *id = player_id_canal();
    guia_overlay_abrir();
    if (id[0]) guia_focar_id(id);
  }
  if (aguardandoFonte != 2 && stream_folha_recarregar()) {
    if (player_aberto()) buscarParaPlayer();
    else {
      const CatItem *ci=cat_item(detail_indice()); char id[64];
      idDoAlvo(ci,id,sizeof id);
      if (ci) { stream_definir_alvo(id); addons_buscar(id,ci->tipo); }
    }
  }
  { int t,e;
    if (aguardandoFonte != 2 && episodios_escolheu(&t,&e) && player_aberto()) {
      int titulo=player_indice();
      player_encerrar(); player_abrir(titulo,NULL);
      player_definir_episodio(t,e);
      marco("buscando fontes"); buscarParaPlayer(); aguardandoFonte=1;
    }
  }
  { int t,e;
    if (aguardandoFonte != 2 && player_pediu_proximo(&t,&e) && player_aberto()) {
      int titulo=player_indice();
      player_encerrar(); player_abrir(titulo,NULL); player_definir_episodio(t,e);
      marco("proximo episodio: buscando fontes"); buscarParaPlayer(); aguardandoFonte=1;
    }
  }
  episodios_atualizar(dt);
  // O player devolve 1 para a coluna de audio e 2 para a de legenda.
  { int q = player_pediu_faixas();
    if (q) faixas_abrir_em(q == 2 ? 1 : 0); }
  faixas_atualizar(dt, agora);
  stream_folha_atualizar(dt, agora);
  // FOLHA FECHADA SEM ESCOLHER, com o player esperando por ela. Voltar na
  // folha e "desisti", nao "tente sozinho": abrir a fonte que a pessoa acabou
  // de recusar seria contradizer o gesto. O player mostra o erro de fonte, que
  // ja tem saida pela mesma tecla.
  if (folhaParaTocar && !stream_folha_aberta()) {
    folhaParaTocar = 0;
    if (player_aberto()) player_erro_fonte();
  }

  // SAIR DO PLAYER PODE PRECISAR REFAZER "CONTINUAR ASSISTINDO".
  //
  // Relato: "assisto um filme ou episodio, saio do player, e o titulo so
  // aparece em Continuar assistindo depois de fechar e reabrir o app".
  // Certissimo: o progresso e gravado na hora, mas a FILEIRA e montada durante
  // a descoberta (montarContinuar), e nada a refazia ao sair.
  //
  // POR QUE desc_repetir() E NAO UMA REMONTAGEM BARATA, que era o meu primeiro
  // reflexo: montarContinuar chama a rede (trakt_continuar, trakt_enfeitar_lote)
  // e usa buffers `static` do fio de descoberta — chama-la daqui seria I/O
  // bloqueante no fio de desenho E corrida com aquele fio. desc_repetir() ja e
  // a resposta da casa para "esta fileira precisa ser refeita": ajustes.c a usa
  // quando a FONTE do Continuar assistindo muda, que e o mesmo problema.
  //
  // SO QUANDO O TITULO NAO ESTA LA. Continuar algo que ja esta na fileira e o
  // caso comum, e ali nada mudou de lugar — pagar um ciclo inteiro (~20 s nesta
  // TV) a cada saida do player seria cobrar do comum o preco do raro.
  if (player_quer_sair() && !player_aberto()) {
    if (player_minimizavel()) {
      // CANAL AO VIVO sai para PiP: o fluxo fica num canto da tela em vez de
      // morrer. O caminho de baixo e o dos filmes/series — progresso a
      // gravar e nada para continuar no canto.
      //
      // A PRIMEIRA vez pergunta: a miniatura ja abre atras do cartao, e a
      // pessoa decide vendo a coisa funcionando — "continuar no canto" ou
      // "fechar o video". A escolha fica gravada (pipintro.c).
      int d = pipintro_decisao();
      if (d < 0) { player_minimizar(); pipintro_abrir(); }
      else if (d) player_minimizar();
      else player_encerrar();
    } else {
    int idx = player_indice();
    const CatItem *ci = idx >= 0 ? cat_item(idx) : NULL;
    int naFileira = 0;
    if (ci && ci->imdb[0]) {
      int r;
      for (r = 0; r < cat_n_fileiras() && !naFileira; r++) {
        const CatFileira *f = cat_fileira(r);
        if (!f || strcmp(f->chave, "continue_watching")) continue;
        { int k;
          for (k = 0; k < f->n; k++) {
            const CatItem *it = cat_item(f->ini + k);
            if (it && !strcmp(it->imdb, ci->imdb)) { naFileira = 1; break; }
          } }
      }
      if (!naFileira) {
        printf("[home] %s nao estava em Continuar assistindo: refazendo\n",
               ci->imdb);
        fflush(stdout);
        desc_repetir();
      }
    }
    player_encerrar();
    }
  }

  player_atualizar(dt, agora);
  detail_atualizar(dt, agora);
  menu_atualizar(dt, agora);
  // A PAGINA DE TITULO saiu por ESQUERDA (detail_pediu_menu): a barra entra
  // quando a mola de saida terminou e a tela de baixo voltou a ser dona do
  // foco. Era `saiuPorEsquerda` cru aqui — e isso abria a barra em QUALQUER
  // ESQUERDA de qualquer tela, inclusive andando numa fileira da home.
  if (!detail_aberto() && detail_pediu_menu() && sidebar_permitida() &&
      !menu_aberto()) {
    menu_abrir();
    saiuPorEsquerda = 0;
  }
  // PÓS-REPRODUÇÃO. O proximo episodio reabre a busca de fonte com o id novo;
  // o titulo relacionado sai do player e abre o detalhe, que e onde o dono
  // escolhe se quer mesmo assistir.
  { int t = 0, e = 0;
    if (aguardandoFonte != 2 && posplay_pediu_episodio(&t, &e) && player_aberto()) {
      // O MESMO CAMINHO do botao de proximo episodio do player, logo acima, e
      // nao um atalho proprio.
      //
      // Este bloco tinha um caminho curto (compor o id na mao e chamar
      // addons_buscar) que NUNCA rodou: posplay_evento zerava o pedido ao
      // fechar o painel, e so a correcao em posplay.c abriu este portao. Ao
      // abrir, tres coisas que o atalho nao fazia passaram a importar:
      //
      //   - player_encerrar SALVA o progresso do episodio que acabou. Sem ele,
      //     maratonar nunca marcava nada como visto.
      //   - player_definir_episodio atualiza epT/epE, o rotulo da barra e os
      //     marcos de introducao. Sem ele o player seguia se dizendo no
      //     episodio anterior, e o proximo player_encerrar gravaria o
      //     progresso do episodio NOVO por cima do ANTIGO.
      //   - buscarParaPlayer pede LEGENDA tambem, e compoe o id por alvoPlayer
      //     (que ja corta no ':' — era o que o atalho reimplementava a mao).
      int titulo = player_indice();
      player_encerrar();
      player_abrir(titulo, NULL);
      player_definir_episodio(t, e);
      // DEPOIS de player_definir_episodio, e a ordem e o conserto de um bug
      // que so existe nesta ordem invertida: definir_episodio retoma a posicao
      // quando o episodio pedido E o que o item aponta. Apontar antes faria o
      // episodio NOVO herdar o progresso do que acabou — quem apertasse OK aos
      // 85% comecaria o seguinte aos 85%.
      //
      // Apontar, porem, tem de acontecer: e de ci->temporada/ci->episodio que
      // o proximo pos-reproducao descobre qual episodio oferecer.
      cat_apontar_episodio(titulo, t, e);
      marco("posplay: proximo episodio");
      buscarParaPlayer();
      aguardandoFonte = 1;
    } }
  { int i = posplay_pediu_titulo();
    if (i >= 0) {
      const CatItem *ci = cat_item(i);
      HomeItem it;
      player_encerrar();
      memset(&it, 0, sizeof it);
      it.indice = i;
      it.rect = (GfxRect){ NV_TELA_W * 0.5f - 124.0f, NV_TELA_H * 0.5f - 186.0f,
                           248.0f, 372.0f };
      it.arte   = ci ? (ci->poster[0] ? ci->poster : ci->backdrop) : NULL;
      it.titulo = ci ? ci->titulo : NULL;
      it.genero = ci ? ci->genero : NULL;
      it.meta   = ci ? ci->meta : NULL;
      detail_abrir(&it);
    } }

  vertudo_atualizar(dt, agora);
  // O guia publica o fio de carga e bombeia o EPG mesmo fechado — barato, e e
  // o que deixa o zap por CH+/- responder na segunda tecla em vez de na
  // vigesima. Com o player num canal, epg_passo alimenta o banner "agora".
  guia_atualizar(dt, agora);
  if (player_aberto()) epg_passo();
  ctx_atualizar(dt, agora);
  { int i = ctx_pediu_detalhes();
    if (i >= 0) {
      const CatItem *ci = cat_item(i);
      HomeItem it;
      memset(&it, 0, sizeof it);
      it.indice = i;
      it.rect = (GfxRect){ NV_TELA_W * 0.5f - 124.0f, NV_TELA_H * 0.5f - 186.0f,
                           248.0f, 372.0f };
      it.arte   = ci ? (ci->poster[0] ? ci->poster : ci->backdrop) : NULL;
      it.titulo = ci ? ci->titulo : NULL;
      it.genero = ci ? ci->genero : NULL;
      it.meta   = ci ? ci->meta : NULL;
      detail_abrir(&it);
    } }
  // Titulo escolhido na grade: abre o detalhe, como se tivesse vindo da home.
  { int idx = vertudo_pediu_abrir();
    if (idx >= 0) {
      const CatItem *ci = cat_item(idx);
      // A grade nao tem retangulo de origem para a transicao crescer a partir
      // dele: o card fica na tela que esta saindo. Entra centrado, do tamanho
      // de um cartaz — o detalhe cobre a tela em seguida de qualquer forma.
      HomeItem it;
      memset(&it, 0, sizeof it);
      it.indice = idx;
      it.rect = (GfxRect){ NV_TELA_W * 0.5f - 124.0f, NV_TELA_H * 0.5f - 186.0f,
                           248.0f, 372.0f };
      it.arte   = ci ? (ci->poster[0] ? ci->poster : ci->backdrop) : NULL;
      it.titulo = ci ? ci->titulo : NULL;
      it.genero = ci ? ci->genero : NULL;
      it.meta   = ci ? ci->meta : NULL;
      detail_abrir(&it);
    } }
  switch (tela) {
    case TELA_EXPLORAR:   explorar_atualizar(dt, agora);   break;
    case TELA_BUSCA:      busca_atualizar(dt, agora);      break;
    case TELA_BIBLIOTECA: biblioteca_atualizar(dt, agora); break;
    case TELA_AGENDA:     agendaui_atualizar(dt, agora);   break;
    case TELA_PERFIL:     break;
    case TELA_AJUSTES:    ajustes_atualizar(dt, agora);    break;
    case TELA_DIAGNOSTICO: diagnostico_atualizar(dt, agora); break;
    default:              home_atualizar(dt, agora);       break;
  }
  // TRAILER NO DESTAQUE: so com a home na frente de tudo. A lista e a mesma
  // ordem de app_evento — o que come tecla antes da home tambem esta na
  // frente dela na tela.
  home_trailer_passo(tela == TELA_HOME && homePronta && login_concluido() && perfilsel_concluido() &&
                     !player_aberto() && !player_mini_ativo() && !detail_aberto() && !spainel_aberto() &&
                     !menu_aberto() && !ctx_aberto() && !vertudo_aberta() && !avisos_aberto() &&
                     !avisos_cartao_aberto() && !sintro_aberto() && !pipintro_aberto() &&
                     !novidades_aberto() && !novidades11_aberto() && !novidades12_aberto() &&
                     !novidades13_aberto() && !novidades131_aberto() && !novidades132_aberto() &&
                     !novidades133_aberto() && !novidades134_aberto() && !novidades139_aberto() && !novidades1312_aberto() && !novidades142_aberto() && !telemetria_aberto() &&
                     !recintro_aberto() && !atualizacao_aberta() && !agendaviso_aberto() &&
                     !recomenda_aberta() && !recenviar_aberto() && !faixas_aberta() &&
                     !episodios_aberto() && !stream_folha_aberta() && !guia_overlay_aberta() &&
                     !registro_aberto(),
                     dt, agora);
  trailer_atualizar(agora);
  perfil_atualizar(dt, agora);
  spainel_atualizar(dt, agora);
  recomenda_atualizar(dt, agora);
  recenviar_atualizar(dt, agora);
  sintro_atualizar(dt, agora);
  novidades_atualizar(dt, agora);
  novidades11_atualizar(dt, agora);
  novidades12_atualizar(dt, agora);
  novidades13_atualizar(dt, agora);
  novidades131_atualizar(dt, agora);
  novidades132_atualizar(dt, agora);
  novidades133_atualizar(dt, agora);
  novidades134_atualizar(dt, agora);
  novidades139_atualizar(dt, agora);
  novidades1312_atualizar(dt, agora);
  novidades142_atualizar(dt, agora);
  telemetria_atualizar(dt, agora);
  recintro_atualizar(dt, agora);
  atualizacao_atualizar(dt, agora);
  agendaviso_atualizar(dt, agora);
  avisos_atualizar(dt, agora);
  pipintro_atualizar(dt, agora);
  if(tela==TELA_SOCIAL) social_atualizar(dt, agora);
  if(tela==TELA_ADDONS) addonsui_atualizar(dt, agora);
}

// O corpo do desenho de TELA. Saiu de app_desenhar para uma funcao propria por
// um motivo unico: os `return` daqui (login, escolha de perfil, estado vazio da
// home) impediam qualquer coisa de ser desenhada DEPOIS deles, e o painel de
// log tem de aparecer tambem nessas telas — que sao exatamente onde o app ja
// travou uma vez.
// CAMADAS DO PONTEIRO (ponteiro.h): cada folha ou modal que tem o teclado
// descarta, antes de se desenhar, os alvos de quem ficou por baixo — o
// ponteiro so pode focar o que as setas focariam. A pergunta e a mesma que o
// roteador de app_evento faz ("esta aberta?"), so que na ordem do desenho.
#define CAMADA_SE(aberta) do { if (aberta) ponteiro_camada(); } while (0)

// TUDO QUE FICA ATRAS DO PAINEL DE SALVOS: a tela, "Ver tudo", o cartaz com
// menu, o detalhe e o menu lateral. Funcao propria para spainel_fundo poder
// pinta-la direto ou uma vez so, dentro do FBO do fundo parado.
static void desenharAtrasDoPainel(void *ctx) {
  Uint32 agora = *(const Uint32 *)ctx;
  // "Ver tudo" cobre a tela de tras por completo (fundo opaco), entao a home
  // nao precisa ser desenhada por baixo — a mesma conta do detail_cobre_tela.
  if (!detail_cobre_tela() && !vertudo_aberta()) {
    switch (tela) {
      case TELA_EXPLORAR:   explorar_desenhar(agora);   break;
      case TELA_GUIA:       guia_desenhar(agora);       break;
      case TELA_BUSCA:      busca_desenhar(agora);      break;
      case TELA_BIBLIOTECA: biblioteca_desenhar(agora); break;
      case TELA_AGENDA:     agendaui_desenhar(agora);   break;
      case TELA_PERFIL:     perfil_desenhar(agora);     break;
      case TELA_SOCIAL:     social_desenhar(agora);     break;
      case TELA_ADDONS:     addonsui_desenhar(agora);   break;
      case TELA_AJUSTES:    ajustes_desenhar(agora);    break;
      case TELA_DIAGNOSTICO: diagnostico_desenhar(agora); break;
      default:              home_desenhar(agora);       break;
    }
  }
  CAMADA_SE(vertudo_aberta());
  if (!detail_cobre_tela()) vertudo_desenhar(agora);
  CAMADA_SE(ctx_aberto());
  ctx_desenhar(agora);
  CAMADA_SE(detail_aberto());
  detail_desenhar(agora);
  // A rail NAO existe na tela de detalhe do app web: ela e full-bleed e a
  // coluna de conteudo comeca em x=72, ou seja, DENTRO do que a rail ocuparia.
  // Com a rail por cima, o logo, o botao "Reproduzir" e a linha de duracao
  // ficavam cortados pela faixa preta de 144px — foi o primeiro defeito que
  // apareceu na captura do aparelho depois do port.
  // A rail some com o detalhe aberto (o web nao a tem nessa tela) e some
  // tambem quando `collapseSidebar` esta ligado, que e o estado do perfil do
  // dono. Recolhida ela nao ocupa largura nenhuma: o conteudo passa a comecar
  // em 104, e quem devolve esse x e ajustes_conteudo_x().
  // A guarda de `collapseSidebar` NAO entra aqui. Ela ja existe DENTRO do
  // menu_desenhar, e la ela pula so a RAIL FIXA — que e o correto: recolhida,
  // a barra nao ocupa largura, mas continua abrindo como CAMADA ao ganhar
  // foco, exatamente como o web faz.
  //
  // Com a guarda tambem neste ponto, o menu_desenhar nunca era chamado no
  // perfil do dono (collapseSidebar ligado): o menu abria, engolia as teclas
  // e nao desenhava nada. Ficava sem menu e sem caminho para os Ajustes — foi
  // o defeito relatado como "nao ta mostrando o menu e nao tem os ajustes".
  // Guarda repetida em dois lugares para a mesma regra: no de dentro ela
  // significa "nao pinte a faixa", no de fora significava "nao exista".
  if (menu_visivel() && sidebar_permitida() && !detail_aberto()) {
    CAMADA_SE(menu_aberto());
    menu_desenhar(agora);
  }
}

static void desenharTelas(Uint32 agora) {
  if (tela == TELA_LOGIN)          { login_desenhar(agora);     return; }
  if (tela == TELA_ESCOLHA_PERFIL) { perfilsel_desenhar(agora); return; }

  // A HOME PODE FICAR PRONTA DEPOIS DO ARRANQUE, e ate agora ninguem reparava.
  //
  // homePronta saia de home_iniciar() e nunca mais era reavaliada. Num pacote
  // COM arte prebaked ela nasce 1 e o defeito nao existe — que e o caso de todo
  // pacote que este projeto ja montou. Num pacote SEM arte (o distribuivel, e o
  // do alvo Tizen, onde o .wgt e read-only) ela nasce 0, o catalogo chega pelo
  // sync segundos depois, home_atualizar monta as fileiras — e o app segue
  // desenhando "Preparando seu catalogo..." para sempre, com 32 fileiras
  // prontas atras. MEDIDO no navegador: "[home] 32 fileiras vindas do catalogo"
  // no log, estado vazio na tela.
  if (!homePronta && home_tem_fileiras()) {
    homePronta = 1;
    printf("[app] catalogo chegou depois do arranque: home liberada\n");
    fflush(stdout);
  }

  // Estado vazio de verdade, em vez de uma tela preta que parece travamento.
  if (!homePronta && tela == TELA_HOME && !player_aberto() && !detail_aberto()) {
    GfxRect fundo = { 0, 0, NV_TELA_W, NV_TELA_H };
    TxtLinha t, sb;
    gfx_cor(fundo, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    t = txt_linha(TXT_TITULO2, "Preparando seu catálogo…", 255, 255, 255, 255);
    txt_desenhar(t, (NV_TELA_W - t.w) * 0.5f, 460.0f);
    sb = txt_linha(TXT_BODY,
                   sync_estado() == SYNC_RODANDO
                     ? "Buscando seus addons e o que você estava assistindo."
                     : "Se isto não sair daqui, confira seus addons na conta.",
                   160, 162, 170, 255);
    txt_desenhar(sb, (NV_TELA_W - sb.w) * 0.5f, 546.0f);
    if (menu_visivel() && sidebar_permitida()) menu_desenhar(agora);
    return;
  }

  // O player cobre tudo; desenhar o que esta atras dele e trabalho jogado fora
  // — a mesma conta que ja valia para o cartao de detalhe esticado.
  if (!player_aberto()) {
    // O FUNDO PARADO DO PAINEL DE SALVOS: com o painel inteiro na tela, tudo
    // que fica atras dele e pintado UMA vez num FBO, ja com o veu, e os quadros
    // seguintes desenham so a copia e o painel. Ver spainel_fundo em
    // salvospainel.c para a medida da C9 que motivou isto.
    //
    // SO NA HOME e so sem outra camada no meio: menu, cartaz com menu, "Ver
    // tudo" e detalhe mudam por baixo do painel ou pedem o proprio desenho. A
    // copia e refeita quando o catalogo troca (fileiras novas por baixo) e cai
    // assim que o painel comeca a fechar.
    { int podeParar = tela == TELA_HOME && !detail_aberto() && !vertudo_aberta() &&
                      !ctx_aberto() && !menu_aberto() && !player_mini_ativo();
      spainel_fundo(podeParar, cat_revisao(), desenharAtrasDoPainel, &agora); }
    // Depois do menu: as duas camadas de "Salvos" escurecem a tela inteira e
    // tem de ficar por cima de tudo que a home desenhou, inclusive da rail.
    CAMADA_SE(spainel_aberto());
    if (spainel_visivel() && !detail_aberto()) spainel_desenhar(agora);
  }
  CAMADA_SE(player_aberto());
  player_desenhar(agora);
  // O overlay do guia vai POR CIMA do player — o video continua atras do fundo
  // a 94%, que e o que diz "a TV nao parou" enquanto se troca de canal.
  if (guia_overlay_aberta()) { ponteiro_camada(); guia_desenhar(agora); }
  CAMADA_SE(episodios_aberto());
  episodios_desenhar();
  CAMADA_SE(stream_folha_aberta());
  stream_folha_desenhar(agora);
  CAMADA_SE(faixas_aberta());
  faixas_desenhar(agora);
}

void app_desenhar(Uint32 agora) {
  // COM O PAINEL DE LOG ABERTO A INTERFACE NAO E PINTADA.
  //
  // O painel e um cartao de tela quase cheia e opaco (alpha 0.94): pintar a
  // home por baixo dele seria uma SEGUNDA camada de tela cheia, e gfx.c ja
  // mediu que duas dessas derrubam a Mali-G71 da TV para ~40fps. Nada se perde
  // visualmente — nao ha nada visivel por baixo — e o que aparece na moldura de
  // 56px e a cor de limpeza do quadro, que o main.c ja pintou.
  //
  // As animacoes nao param por isso: elas avancam em app_atualizar, que
  // continua rodando. O video tambem continua tocando; o que ele perde e o furo
  // de alpha que o revela, entao o plano de video fica coberto enquanto o
  // painel esta em pe — que e o comportamento desejado para quem parou o app
  // para LER o log.
  if (!registro_aberto()) desenharTelas(agora);
  // O explicador fica ACIMA de qualquer tela (menos do painel de log, que e
  // ferramenta de diagnostico): ele e a primeira coisa que a pessoa ve depois
  // desta atualizacao, e nada pode aparecer por cima dele.
  player_mini_desenhar(agora);
  CAMADA_SE(sintro_aberto());
  if (!registro_aberto()) sintro_desenhar(agora);
  CAMADA_SE(novidades_aberto());
  if (!registro_aberto()) novidades_desenhar(agora);
  CAMADA_SE(novidades11_aberto());
  if (!registro_aberto()) novidades11_desenhar(agora);
  CAMADA_SE(novidades12_aberto());
  if (!registro_aberto()) novidades12_desenhar(agora);
  CAMADA_SE(novidades13_aberto());
  if (!registro_aberto()) novidades13_desenhar(agora);
  CAMADA_SE(novidades131_aberto());
  if (!registro_aberto()) novidades131_desenhar(agora);
  CAMADA_SE(novidades132_aberto());
  if (!registro_aberto()) novidades132_desenhar(agora);
  CAMADA_SE(novidades133_aberto());
  if (!registro_aberto()) novidades133_desenhar(agora);
  CAMADA_SE(novidades134_aberto());
  if (!registro_aberto()) novidades134_desenhar(agora);
  CAMADA_SE(novidades139_aberto());
  if (!registro_aberto()) novidades139_desenhar(agora);
  CAMADA_SE(novidades1312_aberto());
  if (!registro_aberto()) novidades1312_desenhar(agora);
  CAMADA_SE(novidades142_aberto());
  if (!registro_aberto()) novidades142_desenhar(agora);
  CAMADA_SE(telemetria_aberto());
  if (!registro_aberto()) telemetria_desenhar(agora);
  CAMADA_SE(recintro_aberto());
  if (!registro_aberto()) recintro_desenhar(agora);
  CAMADA_SE(atualizacao_aberta());
  if (!registro_aberto()) atualizacao_desenhar(agora);
  CAMADA_SE(agendaviso_aberto());
  if (!registro_aberto()) agendaviso_desenhar(agora);
  // A central so aparece com a pessoa DENTRO do app: no login e na escolha de
  // perfil nao ha para quem avisar, e o relogio do toast so comeca a contar
  // no primeiro quadro em que ele pode ser visto (ver avisos.c).
  // (perfilsel_concluido NAO serve de guarda: "Voltar: continuar como X" sai
  // da escolha com `sair`, nunca com `concluido`, e a central ficaria muda.)
  if (!registro_aberto() && !player_aberto() && sessao_logada() &&
      tela != TELA_LOGIN && tela != TELA_ESCOLHA_PERFIL)
    avisos_desenhar(agora);
  CAMADA_SE(recenviar_aberto());
  if (!registro_aberto()) recenviar_desenhar(agora);
  CAMADA_SE(recomenda_aberta());
  if (!registro_aberto()) recomenda_desenhar(agora);
  CAMADA_SE(pipintro_aberto());
  if (!registro_aberto()) pipintro_desenhar(agora);
  CAMADA_SE(diagnostico_intro_aberto());
  if (!registro_aberto()) diagnostico_intro_desenhar(agora);
  CAMADA_SE(registro_aberto());
  registro_desenhar();
}

int app_quer_sair(void) { return sair; }

void app_encerrar(void) {
  avisos_encerrar();   // saida limpa: apaga a marca de sessao viva
  if (fioFonteVivo) {
    // O encerramento acontece depois do laco de desenho: aqui podemos
    // aguardar o transporte para fechar player/AVPlay e os demais modulos em
    // ordem segura. Se ainda estiver RUNNING, invalida a resposta antes do
    // join; o worker so escreve seu proprio slot. A espera pode durar
    // ST_PRAZO_S quando a rede nao responde, e e uma limitacao de teardown,
    // nao um join bloqueante no fio de desenho durante a UI.
    if (atomic_load_explicit(&fonteJob.estado, memory_order_acquire) == FJOB_RUNNING) {
      novaGeracaoFonte();
      limparFontePendente();
    }
    pthread_join(fioFonte, NULL);
    fioFonteVivo = 0;
    atomic_store_explicit(&fonteJob.estado, FJOB_IDLE, memory_order_release);
  }
  aguardandoFonte = 0;
  player_encerrar();
  video_encerrar();    // solta o nome LS2 antes do processo sumir (deploy mata sem aviso)
  ajustes_encerrar();
  diagnostico_encerrar();
  biblioteca_encerrar();
  explorar_encerrar();
  perfil_encerrar();
  busca_encerrar();
  home_encerrar();
}
