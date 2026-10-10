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
#include "arranque.h"
#include "celbotao.h"
#include "central.h"
#include "teclado.h"
#include "ponteiro.h"
#include "app.h"
#include "descanso.h"
#include "iconeapp.h"
#include "logoapp.h"
#include "registro.h"
#include "addonsui.h"
#include "pluginsui.h"
#include "login.h"
#include "sessao.h"
#include "perfis.h"
#include "fontevolta.h"
#include "fonteantecipa.h"
#include "aquecer.h"
#include "addonstats.h"
#include "inicio.h"
#include "perfilsel.h"
#include "sync.h"
#include "traktauth.h"
#include "discord.h"
#include "simklauth.h"
#include "simkl.h"
#include "listas.h"
#include "text.h"
#include "tex_cache.h"
#include "artehero.h"
#include "arteescolha.h"
#include "vertudo.h"
#include "guia.h"
#include "livetvdiag.h"
#include "proxyts.h"
#include "livetv_regras.h"
#include "livestall.h"
#include "guialembrete.h"   /* aviso do lembrete de programa do guia */
#include "epg.h"
#include "posplay.h"
#include "ctxmenu.h"
#include "ctxlista.h"
#include "marco.h"
#include <string.h>
#include "home.h"
#include "trailer.h"
#include "detail.h"
#include "menu.h"
#include "busca.h"
#include "spotlight.h"
#include "biblioteca.h"
#include "explorar.h"
#include "agendaui.h"
#include "agenda.h"
#include "agendaviso.h"
#include "perfil.h"
#include "salvos.h"
#include "contapend.h"
#include "vistonao.h"
#include "contalib.h"
#include "recomenda.h"
#include "atividade.h"
#include "recenviar.h"
#include "pessoas.h"
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
#include "novidades148.h"
#include "novidades170.h"
#include "novidades180.h"
#include "novidades20.h"
#include "novidades201.h"
#include "novidades202.h"
#include "novidades_cartao.h"
#include "apoio.h"
#include "telemetria.h"
#include "avisos.h"
#include "ilha.h"
#include "ilhasalvar.h"
#include "plrilha.h"
#include "ilhacart.h"
#include "cwretido.h"
#include "ilhasinais.h"
#include "enquete.h"
#include "recintro.h"
#include "atualizacao.h"
#include "pipintro.h"
#include "social.h"
#include "amigoperfil.h"
#include "amigosfil.h"
#include "ajustes.h"
#include "ajustes_ux.h"
#include "anim.h"
#include "diagnostico.h"
#include "debrid.h"
#include "p2p.h"
#include "p2pmotor.h"
#include "player.h"
#include "dvtela.h"
#include "legsync.h"
#include "streams.h"
#include "streamfitdur.h"
#include "streamfitpassiva.h"
#include "vazao.h"
#include "stalker.h"
#include "xtream.h"
#include "rede.h"
#include "ts_sonda.h"
#include <unistd.h>
#include "ondever.h"
#include "fontepref.h"
#include "video.h"
#include "addons.h"
#include "idioma.h"
#include "descoberta.h"
#include "ilha_voo.h"
#include "colecoes.h"
#include "posterprov.h"
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
// (o prazo da abertura mora em inicio.h: 8 s sem sinal, 30 s com dado chegando)
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
// ESCOLHA AUTOMATICA COM A LISTA AINDA ENCHENDO (#221). A busca publica cada
// addon que responde (addons.c); a escolha de VOD nao espera mais o ultimo.
// Ela sai quando a lista ja tem uma fonte boa o bastante pelos criterios de
// sempre (fonteauto_pode_decidir) ou quando passa o prazo de Ajustes ("Espera
// pelos add-ons"). Com fonte LEMBRADA neste titulo, o addon dela e esperado
// sem prazo: a escolha da pessoa nao e trocada por pressa.
//
// `autoEsperaN` > 0: a verificacao da lista parcial nao achou nenhuma que
// sirva e a busca ainda esta no ar; a proxima tentativa so sai quando a
// lista crescer alem disso (senao reconferia as mesmas a cada quadro).
static int autoEsperaN;
static void idBaseDoTitulo(char *dst, size_t tam);
static int prefFolhaN = -1;
// Por que a escolha automatica saiu com a lista ainda enchendo, para o log
// "[fonte] decisao em N ms (motivo)" (#202). Escrito por autoParcialPronto
// quando ela devolve 1.
static const char *autoMotivo = "";
static int autoParcialPronto(void) {
  char base[24];
  const FontePref *fp;
  int prazo, lembrada = -1, prefPendente = 0;
  static int ultN = -1, ultLembrada = -1;
  static unsigned ultGeracao;
  if (!addons_busca_parcial() || player_id_canal()[0]) return 0;
  if (stream_n() < 1 || stream_n() <= autoEsperaN) return 0;
  if (stream_n_candidatas() < 1) return 0;
  idBaseDoTitulo(base, sizeof base);
  fp = base[0] ? fontepref_do_titulo(base) : NULL;
  if (fp) {
    // fontepref_escolher escreve no log: uma vez por tamanho de lista.
    if (stream_n() != ultN || stream_lista_geracao() != ultGeracao) {
      ultN = stream_n(); ultGeracao = stream_lista_geracao();
      ultLembrada = fontepref_escolher(base);
    }
    lembrada = ultLembrada;
    prefPendente = lembrada < 0 && addons_pendente_nome(fp->provedor);
  }
  // Escolher a mao: a folha abre com o que ja chegou e continua enchendo.
  if (ajustes_fonte_manual() && lembrada < 0) {
    if (!prefPendente) autoMotivo = "escolha manual: a folha abre com o que chegou";
    return !prefPendente;
  }
  // 0 = Instantaneo, -1 = todos os addons (#202).
  prazo = ajustes_fonte_prazo_ms();
  // O prazo da pessoa e o limite do "lento": quem passa dele quase sempre nesta
  // TV, ou ficou mudo nas ultimas buscas, deixa de segurar a decisao. -1 ("todos
  // os add-ons") e a pessoa pedindo para esperar: ninguem sai.
  addons_definir_espera_decisao(prazo > 0 ? prazo : 0);
  { int passou = prazo > 0 && addons_busca_ms() >= (unsigned)prazo;
    int r = stream_auto_pode_decidir(lembrada, prefPendente, passou, prazo == 0);
    if (r)
      autoMotivo = lembrada >= 0 && !prefPendente ? "fonte lembrada deste titulo"
                 : passou ? "prazo de espera pelos add-ons"
                 : prazo == 0 ? "instantaneo"
                 : addons_faltam(NULL, 0) > addons_faltam_decisivos()
                     ? "so faltam add-ons mudos ou lentos nesta TV"
                     : "fonte boa o bastante";
    return r; }
}
// Episodio que o card de "Continuar assistindo" ANUNCIAVA quando o OK pediu
// para tocar (issue #93). Armado no ramo home_pediu_tocar e consumido pelo
// ramo de detail_pediu_reproduzir, no lugar do episodioAlvo — que neste
// instante ainda nao tem a lista de episodios nem o "proximo" que
// continuar_desenhar calculou para desenhar o card.
static int cwTocarT, cwTocarE;
// O MESMO OK num card cujo episodio TERMINOU e cuja lista de episodios ainda
// nao chegou: a pagina abre e o toque fica esperando a lista para achar o
// proximo, em vez de recomecar o terminado. `cwEsperaItem` e uma copia: a
// fileira pode ser refeita enquanto a lista chega.
static int cwEspera;
static CatItem cwEsperaItem;
static Uint32 cwEsperaDesde;
#define CW_ESPERA_MS 15000u
static pthread_t fioFonte;
static int fioFonteVivo;                  // 0 = canal escolheu sem o fio
static _Atomic int fonteEscolhida = -2;   // release/acquire entre verificacao e UI
// O create_link do Stalker bloqueia a rede. O pedido leva uma copia do id;
// nenhum fio de rede le player_id_canal(), que pode mudar durante um zap.
static _Atomic unsigned stalkerGeracao;
enum { FJOB_IDLE, FJOB_RUNNING, FJOB_DONE };
enum { FJOB_NONE, FJOB_ADDON, FJOB_CANAL, FJOB_STALKER, FJOB_PREPARO };
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
// DIRETO para a primeira fonte e este par vigia: nao abriu em ~25 s ou o
// player marcou erro, tenta a proxima da lista sem pedir nada ao dono.
// Uma fonte classificada viva pode levar ~20 s ate o primeiro quadro; o watchdog
// de abertura tem folga para esse caso, limitado pelo teto absoluto abaixo.
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
static NvLiveStall liveStall;             // vigia de trava do canal (livestall.h)
// CANAL DE ADDON PELO PROXY DE TS (#283). 1 = a tentativa em curso de
// canalFonteIdx foi entregue ao player por proxyts; se ela morrer, a MESMA
// fonte e tentada direto antes de passar para a proxima.
static int    canalViaProxy;
static Uint32 canalFonteDesde;            // quando a fonte atual foi pedida
// So o automatico usa o fallback. Uma escolha manual e uma decisao explicita
// do dono e nao pode ser trocada por outra fonte por tras da tela.
static int    fonteVODAutomatica;
static int    fonteVODTentativas;
// Retomar pela fonte guardada em curso (tocarFonteGuardada); 0 = caminho normal.
static int    voltaAtiva;
static Uint32 voltaDesde;
static Uint32 fonteVODDesde;
static Uint32 fonteManualDesde;   // OK na folha de Fontes: prazo ate o primeiro quadro
// TOCAR ENQUANTO CONFERE (fonteantecipa.h): a candidata que o player abriu antes
// do veredito da conferencia (-1 = nenhuma) e o que valia antes de abrir, para
// desfazer sem perder a conta das tentativas.
static int    antAberta = -1;
static int    antPreAuto, antPreTent;
static Uint32 antPreDesde;
static void limparFonteVOD(void);
#define CANAL_FONTE_PRAZO_MS 25000
// PRAZO CURTO para fonte que JA PROVOU estar ruim. A conferencia de playlist
// (stream_canal_primeira_viva) classifica cada candidata antes de tocar; quando
// a escolhida e apenas "muda" — nao devolveu a playlist em 3 s — dar a ela os
// mesmos 25 s de uma fonte sadia e somar espera sobre espera. MEDIDO na LG num
// canal fora do ar, antes do aumento: 3 s de conferencia + 12 s de watchdog
// POR FONTE. A fonte VIVA recebe 25 s, pois pode levar ~20 s ate o quadro.
#define CANAL_FONTE_PRAZO_MUDA_MS 4000
// TRAVA DEPOIS DE ABRIR. Um canal ao vivo com 12 s de imagem congelada ja
// perdeu — ao contrario de um filme, nao ha nada para recuperar esperando: o
// que passou, passou. O prazo de trava continua em 12 s (independente dos
// 25 s de abertura): um pico de rede que enche o buffer de novo
// termina MUITO antes disso, entao o que sobrevive a 12 s nao e pico, e fonte
// morta. Baixar mais arrisca trocar de fonte num engasgo que ia passar.
#define CANAL_TRAVA_MS 12000
#define CANAL_ABRE_TETO_MS 45000   // com buffer cheio e sem quadro; ver o watchdog
// DADO CHEGANDO E DECODER MUDO, so no Xtream (#158). Nos registros 6311/6314
// (LG C4, webOS 11.2) e 6362/6372 (outra TV, outro provedor) o bufferRange
// subia (9 s, 36 s) sem NENHUM videoInfo, e o teto de 45 s acima era o unico
// prazo: 45 s por formato antes de tentar o outro, e o cartao de erro saia e
// era tirado de novo pelo "voltou a entregar". Nas TVs que tocam o mesmo tipo
// de canal o videoInfo chega ~3 s depois do resourceInfo (registro 7005:
// loadCompleted em 7,9 s). 15 s e o quintuplo disso. So Xtream: o caso
// medido de "abre devagar" (Meu Futebol, 18/09, 20 s ate o quadro) e de
// addon e fica com o teto longo de sempre.
#define CANAL_SEM_DECODER_MS 15000

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
  ilhacart_esquecer_vivo();
  home_retomar_esquecer();
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
  if (e < 0) e = stream_automatico_canal();
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
  antAberta = -1;
  fonteVODAutomatica = 0;
  voltaAtiva = 0;
  fonteVODTentativas = 0;
  fonteVODDesde = 0;
  fonteManualDesde = 0;
  player_definir_tentativa(0, 0);
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
  // Toda escolha de VOD abre uma rodada nova da fonte aberta antes do veredito.
  if (tipo == FJOB_ADDON || tipo == FJOB_PREPARO) fa_nova_rodada();
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
#include "servidores.h"
#include <stdio.h>

static Tela tela = TELA_HOME;
#ifdef NV_LINUX_DESKTOP
static int uiPreview;
#endif
static int sair = 0;

// Nome curto da tela em cena, para o log de campo ("[gpu-modos] lento: ...
// tela=episodios"). O que fica POR CIMA vence: painel de Fontes, Episodios e
// Faixas sobre o player, Salvos, menu e detalhe sobre a home. Sem isto a linha
// de FPS baixo nao diz em que tela a TV caiu, e nao ha como separar o custo do
// vidro em Episodios do custo em qualquer outra. So letras minusculas e '-'.
const char *app_tela_nome(void) {
  static const char *const NOMES[] = {
    "login", "escolha-perfil", "home", "explorar", "guia", "busca", "biblioteca", "perfil",
    "ajustes", "diagnostico", "player", "social", "addons", "agenda", "livetv-diag", "plugins"
  };
  if (stream_folha_aberta()) return "fontes";
  if (episodios_aberto()) return "episodios";
  if (faixas_aberta()) return "faixas";
  if (spainel_aberto()) return "salvos";
  if (menu_aberto()) return "menu";
  if (tela == TELA_HOME && detail_aberto()) return "detalhe";
  if ((unsigned)tela < sizeof NOMES / sizeof *NOMES) return NOMES[tela];
  return "?";
}
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
// Quando a troca de perfil comecou a preparar a home nova. Ver o teto abaixo.
static Uint32 trocaPerfilDesde;
// Teto da espera pela home do perfil novo. A rede lenta nunca prende a pessoa
// na tela de escolha mais do que isto; passado ele, a home abre como estiver.
#define TROCA_PERFIL_TETO_MS 6000

// O detalhe precisa do retangulo REAL de onde o card saiu para o voo comecar
// dali. Cada tela que abre um titulo entrega o seu; quando nenhuma entrega
// (caso do menu ou de um indice vindo de fora), cai para a tela inteira.
// `exigeArte` = 0 para o pos-play e a grade (abriam mesmo sem arte);
// `descartarRetido` = 0 so no pos-play (nunca descartou a sessao retida).
static void abrirTitulo(const HomeItem *it, int exigeArte, int descartarRetido) {
  const CatItem *c;
  if (!it || (exigeArte && !it->arte)) return;
  // Uma pagina nova pode abrir trailer no mesmo plano. Retomar na ilha ja
  // foi tratado antes deste caminho, sem passar pela pagina nem por fontes.
  if (descartarRetido) player_descartar_retido();
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
  abrirTitulo(&it, 1, 1);
}

// Monta o id que os addons esperam. Para serie e "tt1234567:temporada:episodio";
// sem os dois numeros a resposta volta VAZIA com HTTP 200, e era por isso que o
// addons_buscar cravava ":1:1" — o que fazia toda a serie mostrar as fontes do
// episodio 1, qualquer que fosse o escolhido.
static void idDoAlvo(const CatItem *ci, char *dst, size_t n) {
  int t = 0, e = 0;
  if (!ci) { if (n) dst[0] = 0; return; }
  if (!strcmp(ci->tipo, "series") && detail_ep_foco(&t, &e) && t > 0 && e > 0)
    // cat_id_stream: "tt:T:E" no IMDb; o id do video do addon nos demais
    // ("kitsu:41370:5"). Os dois callers passam o item do detalhe.
    cat_id_stream(detail_indice(), t, e, dst, (unsigned)n);
  else
    snprintf(dst, n, "%s", ci->imdb);
}

// A tela de Diagnostico foi aberta pelo cartao da 1.4.2, nao por Ajustes.
static int diagDaHome;
static int ltdDoGuia;

// O perfil do amigo aberto pelo DUELO do Perfil e Stats: Voltar nele volta ao
// Perfil, nao a home. Qualquer outra troca de tela esquece.
static int socialDoPerfil;

static void trocarTela(Tela nova) {
  if (nova == tela) return;
  if (nova != TELA_SOCIAL) socialDoPerfil = 0;
  if (tela == TELA_AJUSTES) ajustes_encerrar();
  if (tela == TELA_LIVETV_DIAG) livetvdiag_encerrar();
  // ESQUERDA na borda do Perfil so pede a barra e deixa a pagina aberta (#371).
  // Se o roteador vai para OUTRA tela (barra proibida no PiP, ou um destino da
  // barra), o Perfil fecha aqui: senao perfil_atualizar/socialvis_atualizar
  // seguiam rodando todo quadro. Voltar ao Perfil passa por perfil_abrir.
  if (tela == TELA_PERFIL) perfil_fechar();
  // #318: a tela piscava e congelava logo depois de uma troca de tela
  // (Android). A linha fica para o proximo relato, com teto: as 120 primeiras
  // trocas e depois no maximo uma a cada 10 s.
  { static int nTr; static Uint32 ultTr; Uint32 agTr = SDL_GetTicks();
    if (nTr < 120 || agTr - ultTr >= 10000) {
      nTr++; ultTr = agTr;
      printf("[transicao] tela %d -> %d t=%u\n", (int)tela, (int)nova, (unsigned)agTr);
    } }
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
    case TELA_LIVETV_DIAG: livetvdiag_iniciar(); break;
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
  if (t > 0 && e > 0) cat_id_stream(player_indice(), t, e, alvo, (unsigned)tam);
  else snprintf(alvo,tam,"%s",c->imdb);
}
// --- StreamFit runtime and passive gate (F03) -------------------------------
// RUNTIME OF THE EXACT TARGET, from metadata the catalog already parsed. Asked
// by the sources sheet when it opens (UI thread) with the id it will classify;
// answers only when that id is the one the player or the detail would request
// (alvoPlayer / idDoAlvo — the same functions that stamp stream_definir_alvo),
// so another title or episode never lends its runtime. Movie: the Cinemeta/addon
// "runtime" kept in CatItem.meta. Episode: that episode's own runtime (the
// /meta video or TMDB season episode), never the show's or a season total.
static double fitRuntimeItem(int indice, int t, int e) {
  const CatItem *c = cat_item(indice);
  if (!c) return 0;
  if (t > 0 && e > 0) {
    int n = cat_n_episodios(indice);
    for (int k = 0; k < n; k++) {
      const CatEp *ep = cat_episodio(indice, k);
      if (ep && ep->temporada == t && ep->episodio == e) return streamfitdur_texto(ep->duracao);
    }
    return 0;
  }
  return !strcmp(c->tipo, "movie") ? streamfitdur_meta_filme(c->meta) : 0;
}
static double fitRuntimeMeta(const char *alvo) {
  char a[64];
  int t = 0, e = 0;
  if (!alvo || !*alvo) return 0;
  if (player_aberto() && !player_id_canal()[0]) {
    alvoPlayer(a, sizeof a);
    if (!strcmp(a, alvo)) { player_episodio_atual(&t, &e); return fitRuntimeItem(player_indice(), t, e); }
  }
  { const CatItem *ci = cat_item(detail_indice());
    idDoAlvo(ci, a, sizeof a);
    if (ci && !strcmp(a, alvo)) {
      if (!strcmp(ci->tipo, "series") && !(detail_ep_foco(&t, &e) && t > 0 && e > 0)) return 0;
      return fitRuntimeItem(detail_indice(), t, e);
    }
  }
  return 0;
}
// REAL MEDIA DURATION of the player's own source (player_duracao_midia), for
// the player's exact target. A new file or target clears the previous media
// duration first; metadata of that target stays. Every frame, cheap.
static void fitDuracaoMidia(void) {
  static char ultAlvo[64];
  static unsigned long ultUrl;
  static int enviado;
  char alvo[64];
  unsigned long h = 5381;
  double d;
  if (!player_aberto() || player_id_canal()[0]) return;
  alvoPlayer(alvo, sizeof alvo);
  for (const char *p = video_url_atual(); p && *p; p++) h = h * 33 + (unsigned char)*p;
  if (strcmp(alvo, ultAlvo) || h != ultUrl) {
    if (ultAlvo[0] && enviado) stream_fit_duracao(ultAlvo, 0, SF_DUR_MEDIA);
    snprintf(ultAlvo, sizeof ultAlvo, "%s", alvo);
    ultUrl = h; enviado = 0;
  }
  if (!enviado && alvo[0] && !vazao_url_aviso(video_url_atual()) && player_duracao_midia(&d)) {
    stream_fit_duracao(alvo, d, SF_DUR_MEDIA);
    enviado = 1;
  }
}
// PASSIVE TELEMETRY GATE: only the Android backend session that is the
// player's real source may feed StreamFit. Trailer, channel, warning clip,
// paused-in-mini or idle pipeline: generation 0, every window is stale.
static void fitPassivaPermitir(void) {
#ifdef NV_ANDROID
  int ok = player_aberto() && !player_id_canal()[0] && player_com_video() &&
           !vazao_url_aviso(video_url_atual());
  streamfitpassiva_permitir(ok ? video_android_sessao() : 0);
#endif
}

static void buscarParaPlayerModo(int renovar) {
  char alvo[64]; alvoPlayer(alvo,sizeof alvo);
  const char *idC = player_id_canal();
  // Episodio novo abre um ciclo novo de fontes. A fonte automatica do
  // episodio anterior nao pode contaminar o watchdog nem a lista de exclusao.
  limparFonteVOD();
  autoEsperaN = 0;
  // CARIMBA O ALVO ANTES DE PEDIR (issue #101). A lista que voltar passa a
  // saber de que episodio ela e; sem isto ninguem consegue distinguir "a lista
  // do E6" de "a lista do E5 que ninguem invalidou". Ver streams.h.
  stream_definir_alvo(alvo);
  if (idC[0]) {
    // SEM LEGENDA DE ADDON NO CANAL: o id do canal ia ao OpenSubtitles como
    // "movie" (Recarregar da folha). As do canal sao as do fluxo (faixas.c).
    if (renovar) addons_buscar_renovar(alvo,"tv"); else addons_buscar(alvo,"tv");
    return;
  }
  {
    const CatItem *c = cat_item(player_indice());
    if (c && alvo[0]) {
      if (renovar) addons_buscar_renovar(alvo,c->tipo); else addons_buscar(alvo,c->tipo);
      addons_buscar_legendas(alvo,c->tipo);
    }
  }
}
static void buscarParaPlayer(void) { buscarParaPlayerModo(0); }

// RENOVA POR IDADE **E POR DONO** (issue #101).
//
// O alvo tambem mudou: era idDoAlvo(), o episodio EM FOCO na pagina, e
// quem toca nem sempre e ele — "Retomar" e o card de Continuar assistindo
// abrem outro episodio (cwTocar/episodioDoDetalhe), e a busca saia para um id
// diferente do que ia reproduzir. alvoPlayer le o episodio ja definitivo,
// depois de player_abrir.
//
// E a condicao deixou de ser so o relogio: a lista podia ter 3 s de vida
// e ser do episodio anterior, e ai nada a refazia. Idade cobre o link
// assinado que expira; o dono cobre o episodio errado. Sao duas coisas.
static void renovarListaDoPlayer(void) {
  const CatItem *ci = cat_item(player_indice());
  char alvoP[64]; alvoPlayer(alvoP, sizeof alvoP);
  if (ci && ci->imdb[0] && alvoP[0] &&
      (stream_idade_ms() > NV_LINK_VALIDO_MS || !stream_lista_do_alvo(alvoP))) {
    printf("fonte: lista com %ums%s, renovando (%s)\n",
           (unsigned)stream_idade_ms(),
           stream_lista_do_alvo(alvoP) ? "" : " e de outro alvo", alvoP);
    stream_definir_alvo(alvoP);
    addons_buscar(alvoP, ci->tipo);
  }
}

// RETOMAR PELA FONTE QUE ESTAVA TOCANDO (fontevolta.h).
//
// O caminho de sempre e: busca em todos os addons, verificacao, abertura. Com
// a fonte da ultima sessao boa deste titulo/episodio guardada (mesma conta,
// mesmo perfil, link dentro da validade), a abertura vai DIRETO nela — o seek
// para o ponto salvo e o mesmo de qualquer abertura (retomarPct no player).
// A busca nos addons continua saindo, mas em segundo plano: e ela que enche a
// folha de Fontes e que fica pronta para o recuo abaixo.
//
// O RECUO e silencioso: conferencia em paralelo que falha (4xx/5xx, endereco
// de aviso), erro do player, clipe curto no lugar do titulo, ou o prazo
// FONTEVOLTA_PRAZO_MS sem abrir — o video para, a tela continua em "abrindo
// fonte" e a escolha normal assume (aguardandoFonte = 1), com a lista que a
// busca de fundo ja trouxe ou esta trazendo.
// A URL QUE O PLAYER RECEBE PARA UMA FONTE DE CANAL (#283).
//
// Relato #283 (QNED70B e NanoCell, 2.0.1): os canais dos addons nao tocam na
// LG e tocam no Mi Stick. Nos registros 41889/41909/41918/42119/42328 (mesma
// conta, webOS 11.2 e 6.5) o padrao e o do #158 no Xtream: a playlist do addon
// e viva para a sonda, o uMS recebe o load, ganha VDEC, enche 4-8 s de buffer
// e o decoder nunca anuncia (`buffer=4.0s decoder=0 ... 21061ms`, Fenix TV), ou
// o servidor recusa os pedidos do motor HLS do uMS (`server error:40404`, `203
// AV Type Not Founded`) numa playlist que a sonda, com os cabecalhos do addon,
// leu viva. O Xtream ja passa pelo proxy de TS desde o #158 e toca; os canais
// de addon iam sempre direto.
//
// Agora a fonte HTTP de canal de addon que nao e arquivo (.mp4/.mkv) vai pelo
// proxy, com os cabecalhos do addon em todo pedido (playlist e segmentos). O
// proxy redireciona o que ja e TS continuo ou nao e playlist, entao o caminho
// que ja tocava nao muda. Desligado em Ajustes > Live TV, como no Xtream.
static const char *urlCanal(const Stream *s, char *buf, size_t n) {
  const char *id = player_id_canal();
  canalViaProxy = 0;
  if (!s) return "";
  if (!strncmp(s->url, PROXYTS_PREFIXO, strlen(PROXYTS_PREFIXO))) {
    // Xtream: a lista ja vem marcada, e sem cabecalho de addon.
    proxyts_definir_cabecalhos(s->cabecalhos);
    return s->url;
  }
  if (id[0] && !xtream_e_id(id) && !stalker_e_id(id) && ajustes_livetv_proxy() &&
      proxyts_disponivel() && proxyts_candidata(s->url, s->mp4) &&
      strlen(s->url) + strlen(PROXYTS_PREFIXO) < n) {
    proxyts_definir_cabecalhos(s->cabecalhos);
    snprintf(buf, n, "%s%s", PROXYTS_PREFIXO, s->url);
    canalViaProxy = 1;
    printf("[canal] fonte de addon pelo proxy-ts%s\n",
           s->cabecalhos[0] ? ", com os cabecalhos do addon" : "");
    fflush(stdout);
    return buf;
  }
  proxyts_definir_cabecalhos("");
  return s->url;
}

static int tocarFonteGuardada(void) {
  Stream s;
  char alvo[64];
  if (player_id_canal()[0] || !player_aberto()) return 0;
  alvoPlayer(alvo, sizeof alvo);
  if (!alvo[0] || !fontevolta_pegar(alvo, sessao_usuario(), perfis_ativo(), SDL_GetTicks(), &s))
    return 0;
  marco("abrir: fonte guardada, sem busca");
  stream_definir_atual(-1);
  video_definir_dv(s.dolbyVision);
  video_definir_cabecalhos(s.cabecalhos);
  video_definir_mp4(stream_e_mp4(&s));
  fontevolta_conferir(s.url, s.cabecalhos);
  player_definir_fonte(s.url);
  voltaAtiva = 1;
  voltaDesde = SDL_GetTicks();
  return 1;
}
static void vigiarFonteGuardada(void) {
  const char *motivo = NULL;
  Uint32 desde;
  if (!voltaAtiva) return;
  if (!player_aberto() || player_quer_sair() || player_id_canal()[0] || aguardandoFonte) {
    voltaAtiva = 0; return;
  }
  { FontevoltaSinais g;
    int d;
    g.falhou = video_falhou() || player_fonte_falhou();
    g.pronto = video_pronto();
    g.duracao = video_duracao();
    g.carregando = player_carregando();
    g.conferencia = fontevolta_conferencia();
    g.desdeMs = desde = SDL_GetTicks() - voltaDesde;
    d = fontevolta_decidir(&g, &motivo);
    if (d == FV_ABRIU) {
      printf("[voltafonte] abriu em %u ms sem busca\n", (unsigned)desde);
      fflush(stdout);
      voltaAtiva = 0;
      return;
    } }
  if (!motivo) return;
  printf("[voltafonte] recuo para a busca: %s (%u ms)\n", motivo, (unsigned)desde);
  fflush(stdout);
  marco("abrir: fonte guardada falhou, busca normal");
  voltaAtiva = 0;
  fontevolta_esquecer(motivo);
  player_voltar_a_esperar();
  limparFonteVOD();
  renovarListaDoPlayer();
  aguardandoFonte = 1;
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

// O nome que a folha de Fontes poe no titulo de cada linha: o do item que
// idBaseDoTitulo usa (o player aberto, senao o detalhe). Canal fica sem nome
// e a folha cai no addon.
static void nomeParaFolha(void) {
  const CatItem *c = player_id_canal()[0] ? NULL
                   : cat_item(player_aberto() ? player_indice() : detail_indice());
  stream_folha_nome(c ? c->titulo : "");
  stream_folha_canal(player_id_canal()[0] != 0);
  stream_folha_item(c ? (player_aberto() ? player_indice() : detail_indice()) : -1);
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
//
// DUAS FONTES, .m3u8 e .ts (#158), na ordem de xtream_formatos: o que a conta
// declara em allowed_output_formats e o que ja tocou nesta sessao. A troca
// de uma para a outra e o watchdog de canal de sempre (fonte morta ->
// proxima), sem caminho novo. Um painel que nao gera HLS para um canal
// responde "Media Not Found" no .m3u8 e toca no .ts.
// AS FONTES DE UM CANAL XTREAM: o mesmo canal em cada formato que a conta
// aceita (xtream_formatos) e, com a Live TV > Resolucao principal, as OUTRAS
// RESOLUCOES do mesmo canal na lista ("CINEMAX FHD" / "CINEMAX HD", ver
// guia_variantes). A da resolucao escolhida vai primeiro; se nao abrir, o
// watchdog passa para a seguinte. Em Automatica o canal escolhido vai primeiro
// e as variantes ficam de reserva.
#define XT_VARIANTES_MAX 3
static int resolverCanalXtream(void) {
  static Stream s[3 * XT_VARIANTES_MAX];
  GuiaVariante v[XT_VARIANTES_MAX], ord[XT_VARIANTES_MAX];
  const char *ext[2];
  int k = xtream_formatos(ext), i, j, n = 0, nv, no = 0;
  int alvo = nv_res_opcao_altura(ajustes_livetv_resolucao());
  memset(s, 0, sizeof s);
  nv = guia_variantes(player_id_canal(), v, XT_VARIANTES_MAX);
  if (nv < 1) {
    snprintf(v[0].id, sizeof v[0].id, "%s", player_id_canal());
    v[0].nome[0] = 0; v[0].altura = 0; nv = 1;
  }
  for (j = 0; j < nv; j++) if (nv_res_preferida(v[j].altura, alvo)) ord[no++] = v[j];
  for (j = 0; j < nv; j++) if (!nv_res_preferida(v[j].altura, alvo)) ord[no++] = v[j];
  for (j = 0; j < no; j++)
    for (i = 0; i < k && n < (int)(sizeof s / sizeof *s); i++) {
      char url[4096];
      const char *f = !strcmp(ext[i], "ts") ? "TS" : "HLS";
      if (!xtream_url_formato(ord[j].id, ext[i], url, sizeof url)) continue;
      // PROXY DE TS (#158, proxyts.h): a primeira fonte de cada variante vai
      // pelo proxy local — HLS (ou .ts que o painel responde com playlist) vira
      // TS continuo; o que ja e TS continuo o proxy redireciona direto. As
      // fontes diretas ficam atras, de reserva.
      if (!i && ajustes_livetv_proxy() && proxyts_disponivel() && n < (int)(sizeof s / sizeof *s) - 1) {
        snprintf(s[n].url, sizeof s[n].url, "%s%s", PROXYTS_PREFIXO, url);
        if (strcmp(ord[j].id, player_id_canal()) && ord[j].nome[0])
          snprintf(s[n].rotulo, sizeof s[n].rotulo, "Xtream (%s, proxy) · %s", f, ord[j].nome);
        else snprintf(s[n].rotulo, sizeof s[n].rotulo, "Xtream (%s, proxy)", f);
        snprintf(s[n].provedor, sizeof s[n].provedor, "%s", "xtream");
        s[n].altura = ord[j].altura;
        s[n].fileIdx = -1;
        n++;
      }
      snprintf(s[n].url, sizeof s[n].url, "%s", url);
      if (strcmp(ord[j].id, player_id_canal()) && ord[j].nome[0])
        snprintf(s[n].rotulo, sizeof s[n].rotulo, "Xtream (%s) · %s", f, ord[j].nome);
      else snprintf(s[n].rotulo, sizeof s[n].rotulo, "Xtream (%s)", f);
      snprintf(s[n].provedor, sizeof s[n].provedor, "%s", "xtream");
      s[n].altura = ord[j].altura;
      s[n].fileIdx = -1;
      n++;
    }
  if (!n) return -1;
  if (nv > 1 || alvo)
    printf("[livetv] xtream: %d variante(s) do canal, %d fonte(s); resolucao principal %dp, primeira %s\n",
           nv, n, alvo, s[0].rotulo);
  stream_definir_lista(s, n);
  return 0;
}

// O CARTAO DE ERRO DO CANAL XTREAM diz o que se sabe (#158). O generico
// "nao foi possivel abrir a fonte" era tudo o que a pessoa via — e no registro
// 6314 nem isso ficava, porque o "voltou a entregar" tirava o cartao. A conta
// (xtream_conta_ler, no fio do guia) explica os casos em que NENHUM canal
// toca; o erro do pipeline, o deste canal.
static void motivoCanalXtream(char *t, size_t nt, char *d, size_t nd) {
  XtreamConta c;
  const char *err = video_erro_texto();
  int aviso = xtream_conta(&c) ? xtream_conta_aviso(&c, (long long)time(NULL)) : XA_NADA;
  // SO UM FORMATO TENTADO (#158, registro 13776): a conta declara
  // allowed_output_formats=["ts"], a lista do canal tem uma fonte so, e o
  // cartao dizia "nao abriu em HLS nem em TS" sem HLS nenhum ter sido pedido.
  const char *ext[2], *umFormato = NULL;
  if (xtream_formatos(ext) == 1) umFormato = !strcmp(ext[0], "ts") ? "TS" : "HLS";
  if (aviso == XA_EXPIRADA) {
    snprintf(t, nt, "%s", i18n("A assinatura Xtream venceu."));
    snprintf(d, nd, "%s", i18n("Renove com o seu provedor. Os canais voltam sozinhos depois disso."));
  } else if (aviso == XA_DESATIVADA || aviso == XA_RECUSOU) {
    snprintf(t, nt, "%s", i18n("O provedor desativou esta conta Xtream."));
    snprintf(d, nd, "%s", i18n("Fale com o seu provedor ou confira o cadastro em Ajustes."));
  } else if (aviso == XA_TELAS_CHEIAS) {
    snprintf(t, nt, i18n("Todas as telas da conta Xtream estão em uso (%d de %d)."),
             c.conexoes, c.maxConexoes);
    snprintf(d, nd, "%s", i18n("Feche o Xtream em outro aparelho e tente de novo."));
  } else if (err && err[0]) {
    snprintf(t, nt, "%s", i18n("O provedor não entregou este canal."));
    if (umFormato) snprintf(d, nd, i18n("Resposta do servidor: %s. A conta só permite %s."), err, umFormato);
    else snprintf(d, nd, i18n("Resposta do servidor: %s. HLS e TS foram tentados."), err);
  } else {
    if (umFormato) snprintf(t, nt, i18n("O canal não abriu em %s."), umFormato);
    else snprintf(t, nt, "%s", i18n("O canal não abriu em HLS nem em TS."));
    snprintf(d, nd, "%s", i18n("O vídeo chegou, mas a TV não começou a tocar. Envie o registro em Ajustes."));
  }
}
static void erroCanalXtream(void) {
  char t[160], d[200];
  motivoCanalXtream(t, sizeof t, d, sizeof d);
  player_erro_fonte_motivo(t, d);
}
// O QUE O PROVEDOR MANDA DE VERDADE (#158). Quando o canal Xtream morre por
// "dado chegando e decoder mudo" e nao ha outra fonte, o pipeline e fechado e
// um fio le o comeco do mesmo .ts e a playlist .m3u8 do mesmo canal, e escreve
// no registro: e TS? que codecs a PMT declara? o SPS e 10 bits? a .m3u8 existe
// mesmo com allowed_output_formats=ts? Na C4 do pasha o uMS fica 45 s com 36 s
// de buffer sem nunca dizer o que recebeu (registro 14195), e outro app na
// mesma TV toca o canal — sem isto nao ha como saber o que muda. So diagnostico:
// nada aqui decide reproducao. Uma vez por canal; o pipeline fecha antes porque
// a conta tem 1 tela so (telas=0/1) e uma segunda conexao seria recusada.
static void *sondarCanalXtreamFio(void *u) {
  char *id = u, url[4096], fin[4096], r[600];
  long n = 0;
  int st = 0, er = 0;
  char *b;
  sleep(1);
  if (xtream_url_formato(id, "ts", url, sizeof url)) {
    fin[0] = 0;
    b = rede_baixar_trecho_st(url, 8, 0, 786431, &n, &st, &er, fin, sizeof fin);
    if (b && n > 0) {
      TsSonda ts;
      ts_sondar((const unsigned char *)b, n, &ts);
      ts_resumo(&ts, r, sizeof r);
    } else memcpy(r, "-", 2);
    printf("[xtream] sonda ts: HTTP %d curl=%d %ld B%s | %s\n", st, er, n,
           fin[0] && strcmp(fin, url) ? " (redirecionado)" : "", r);
    free(b);
  }
  if (xtream_url_formato(id, "m3u8", url, sizeof url)) {
    int linhas = 0, ts = 0;
    n = 0; st = 0; er = 0;
    b = rede_baixar_trecho_st(url, 6, 0, 65535, &n, &st, &er, NULL, 0);
    if (b && n > 0) {
      const char *q;
      for (q = b; q && *q; q = strchr(q, '\n')) {
        if (*q == '\n') q++;
        if (!*q) break;
        if (*q != '#') { linhas++; if (strstr(q, ".ts")) ts++; }
      }
    }
    printf("[xtream] sonda m3u8: HTTP %d curl=%d %ld B, %s, %d entrada(s) (%d .ts)\n", st, er, n,
           b && n >= 7 && !strncmp(b, "#EXTM3U", 7) ? "comeca com #EXTM3U" : "NAO e playlist",
           linhas, ts);
    free(b);
  }
  fflush(stdout);
  free(id);
  return NULL;
}
static void sondarCanalXtream(const char *id) {
  static char ultimo[80];
  pthread_t t;
  char *c;
  if (!id || !id[0] || !strcmp(ultimo, id)) return;
  snprintf(ultimo, sizeof ultimo, "%s", id);
  c = strdup(id);
  if (!c) return;
  if (pthread_create(&t, NULL, sondarCanalXtreamFio, c) == 0) pthread_detach(t);
  else free(c);
}
// No PREVIEW do guia a miniatura morre quieta (ver o watchdog); para o Xtream
// sai ao menos a frase curta por cima do guia — no registro 6314 a pessoa
// ficava olhando um preview preto sem saber por que.
static void avisoCanalXtreamMini(void) {
  char t[160], d[200];
  motivoCanalXtream(t, sizeof t, d, sizeof d);
  glem_aviso_curto(t);
}

// Preparar a fonte ao abrir o titulo (ver prepararAoAbrir, mais abaixo).
static int preparoRodando, preparoEspera, preparoLembradaPlay, preparoLembradaRod;
static void preparoGuardar(const char *alvo, unsigned lista, int idx);
static int  preparoUsar(const char *alvoPlay, int lembrada, int *idx);

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
  if (tipo == FJOB_PREPARO) {
    // Resposta do preparo (ver prepararAoAbrir): guarda, e se o Play ja estava
    // esperando por ela, entrega no ato. Nunca passa pelo bloco generico.
    preparoRodando = 0;
    preparoGuardar(id, geracao, resultado);
    if (preparoEspera) {
      char alvoP[64];
      int idx;
      preparoEspera = 0;
      alvoPlayer(alvoP, sizeof alvoP);
      if (aguardandoFonte == 2 && preparoUsar(alvoP, preparoLembradaPlay, &idx)) {
        fonteEscolhida = idx;
        limparFontePendente();   // o pedido normal que esperava na fila nao e mais preciso
        printf("[fonte] preparada ao abrir: o Play esperou por ela e abriu\n");
        fflush(stdout);
      }
    }
  }
  aplicar = geracao == fontePedidoGeracao &&
            geracao == atomic_load_explicit(&stalkerGeracao, memory_order_acquire);
  if (tipo == FJOB_PREPARO) aplicar = 0;   // a resposta do preparo ja foi tratada acima
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
        canalViaProxy = 0;
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
  } else if (!canal && p2p_ativo() && !debrid_ativo() && stream_qtd_torrents() > 0) {
    // So ha torrent na lista e o automatico nao toca P2P (sem peers a TV
    // ficaria parada). A folha toca.
    player_erro_fonte_motivo(i18n("As fontes desta lista são P2P (torrent)"),
        i18n("Abra Fontes e escolha uma: ela toca pelo servidor P2P."));
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
  // Sem debrid, quem responde e o servidor P2P, que espera peers: dizer que
  // pode levar meio minuto evita a pessoa achar que travou.
  if (!debrid_ativo() && p2p_usa_motor())
    player_toast_ex(i18n("Baixando o torrent nesta TV… pode levar até um minuto"), 30000, "aj_download", 0);
  else if (!debrid_ativo() && p2p_ativo())
    player_toast_ex(i18n("Pedindo o torrent ao servidor P2P… pode levar até um minuto"), 30000, "aj_download", 0);
  else
    player_toast_ex(i18n("Pedindo o torrent ao serviço de debrid…"), 5000, "aj_download", 0);
}
// MOTOR P2P EMBUTIDO (p2pmotor.h), a cada quadro e ANTES de qualquer return
// de app_atualizar (login, perfis e outras telas saem cedo de la). So le
// atomicos e pega a trava curta do motor: a vigia (disco/RAM/statvfs) e o
// destroy rodam em fios proprios.
static void vigiarMotorP2p(void) {
  int m;
  // #334: o limite de Ajustes vale na proxima subida do motor (um atomico).
  p2pmotor_definir_limite_mb(ajustes_p2p_limite_mb());
  // Sem p2pmotor_disponivel() aqui: no .tpk ele abre a libnuvio_engine.so
  // (dlopen), e isso so deve acontecer quando alguem pede P2P. ativo() e
  // motivo_parada() so leem estado.
  // Player fechado (nem cheio nem mini): nada de baixar/compartilhar com
  // ninguem assistindo. Pedido em curso e cancelado (sai em <= 20 ms); depois
  // que ele sai, o motor para em fio solto e o cache e apagado.
  if (!p2pmotor_segurado() && !player_aberto() && !player_mini_ativo() && p2pmotor_ativo()) {
    if (fioTorrentVivo) p2pmotor_cancelar();
    else p2pmotor_parar_fundo();
  }
  // A vigia parou o motor com o filme tocando: dizer por que.
  if ((m = p2pmotor_motivo_parada()) && player_aberto()) {
    if (m == P2P_ERR_RAM)
      player_erro_fonte_motivo(i18n("O P2P passou do limite de memória"),
          i18n("O vídeo parou para proteger o app. Abra Fontes para escolher outra opção."));
    else if (m == P2P_ERR_DISCO)
      player_erro_fonte_motivo(i18n("Não foi possível medir o espaço livre da TV"),
          i18n("Sem essa medida o P2P não baixa nada. Abra Fontes para escolher outra opção."));
    else {
      // #334: dizer os numeros. O motor guarda o arquivo inteiro: um 4K de
      // 15 GB nao cabe em 0,7 GB, e "escolha uma fonte menor" sem o tamanho
      // nao explicava por que.
      uint64_t tam = 0, teto = 0, livre = 0;
      char corpo[300];
      p2pmotor_arquivo_atual(&tam, &teto, &livre);
      if (tam > 0 && teto > 0) {
        snprintf(corpo, sizeof corpo,
                 i18n("O vídeo tem %.1f GB e o P2P só pode guardar %.1f GB nesta TV (%.1f GB livres). Escolha uma fonte menor ou mude o limite em Ajustes > P2P."),
                 (double)tam / 1073741824.0, (double)teto / 1073741824.0, (double)livre / 1073741824.0);
        player_erro_fonte_motivo(i18n("O P2P encheu o espaço livre da TV"), corpo);
      } else
        player_erro_fonte_motivo(i18n("O P2P encheu o espaço livre da TV"),
            i18n("O vídeo parou para não lotar a TV. Escolha uma fonte menor."));
    }
  }
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
      // E A ILHA DIZ DE NOVO NA HOME (02/10, mockup "Fonte baixando no
      // debrid"): o cartao do player some com o Voltar, e o aviso espera na
      // fila da ilha ate a pilula voltar a aparecer.
      { const CatItem *pc = cat_item(player_indice());
        ilhasinais_debrid_baixando(serv, pc ? pc->titulo : NULL); }
    } else if (j->resultado == STREAM_P2P_FALHOU) {
      // O SERVIDOR P2P respondeu por ultimo (o debrid nao resolveu ou nao
      // existe), entao o motivo que a pessoa precisa e o dele: cada um tem um
      // conserto diferente (endereco, torrent sem peers, arquivo errado).
      switch (p2p_ultimo_erro()) {
        case P2P_ERR_SERVIDOR:
          player_erro_fonte_motivo(i18n("Servidor P2P sem resposta"),
              i18n("Confira o endereço e se o servidor está ligado, em Ajustes > Avançado."));
          break;
        case P2P_ERR_NAO_STREMIO:
          player_erro_fonte_motivo(i18n("O endereço respondeu, mas não é um servidor Stremio"),
              i18n("Confira o endereço e a porta (11470) em Ajustes > Avançado."));
          break;
        case P2P_ERR_SEM_PEERS:
          player_erro_fonte_motivo(i18n("Este torrent não tem peers agora"),
              i18n("Os dados não chegaram a tempo. Escolha outra fonte ou tente mais tarde."));
          break;
        case P2P_ERR_SEM_VIDEO:
          player_erro_fonte_motivo(i18n("Este torrent não tem arquivo de vídeo"),
              i18n("Abra Fontes para escolher outra opção."));
          break;
        case P2P_ERR_MOTOR:
          player_erro_fonte_motivo(i18n("O P2P desta TV não iniciou"),
              i18n("Abra Fontes para escolher outra opção."));
          break;
        case P2P_ERR_SEM_ESPACO:
          player_erro_fonte_motivo(i18n("Falta espaço livre na TV para o P2P"),
              i18n("O P2P precisa de 256 MB livres. Abra Fontes para escolher outra opção."));
          break;
        case P2P_ERR_DISCO:
          player_erro_fonte_motivo(i18n("Não foi possível medir o espaço livre da TV"),
              i18n("Sem essa medida o P2P não baixa nada. Abra Fontes para escolher outra opção."));
          break;
        case P2P_ERR_CANCELADO:
          break;   // outra escolha/fechamento ja cuidou da tela
        default:
          player_erro_fonte_motivo(i18n("O servidor P2P não abriu este torrent"),
              i18n("Abra Fontes para escolher outra opção."));
      }
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
// PREPARAR A FONTE AO ABRIR O TITULO (opcional, desligado de fabrica). Com a
// busca de fontes completa e a pagina do titulo aberta, roda a MESMA escolha do
// Play (stream_primeira_boa, num job FJOB_PREPARO) e guarda o resultado; o Play
// seguinte, se a lista, o episodio e a fonte lembrada forem os mesmos e o link
// ainda valer, toma a fonte pronta e abre na hora. Custo avisado na ajuda: o
// arquivo entra no painel do debrid mesmo que a pessoa nao assista.
static struct { int tem, idx, lembrada; unsigned lista; Uint32 quando; char alvo[64]; } preparo;
static void preparoGuardar(const char *alvo, unsigned lista, int idx) {
  memset(&preparo, 0, sizeof preparo);
  if (idx < 0) return;
  snprintf(preparo.alvo, sizeof preparo.alvo, "%s", alvo);
  preparo.lista = lista; preparo.idx = idx; preparo.lembrada = preparoLembradaRod;
  preparo.quando = SDL_GetTicks(); preparo.tem = 1;
  { const Stream *s = stream_item(idx);
    printf("[fonte] preparada ao abrir: \"%s\" (%d)\n", s ? s->rotulo : "?", idx);
    fflush(stdout); }
}
// 1 e *idx quando a preparada serve a ESTE Play; consome (vale uma vez).
static int preparoUsar(const char *alvoPlay, int lembrada, int *idx) {
  int ok;
  if (!preparo.tem) return 0;
  { FaPreparada fp = { preparo.alvo, preparo.lista, preparo.lembrada, preparo.idx, SDL_GetTicks() - preparo.quando };
    FaAgora fa = { alvoPlay, stream_lista_geracao(), lembrada, stream_n(),
                   stream_automatico_disponivel(preparo.idx), stream_idade_ms() };
    ok = fa_preparada_vale(&fp, &fa); }
  if (!ok) {
    printf("[fonte] preparada ao abrir descartada (lista, episodio ou fonte lembrada mudaram, ou o link envelheceu)\n");
    fflush(stdout);
    preparo.tem = 0;
    return 0;
  }
  *idx = preparo.idx;
  preparo.tem = 0;
  return 1;
}
static void prepararAoAbrir(void) {
  const CatItem *ci;
  char alvo[64], base[24];
  static char feito[64];
  static unsigned feitoGer;
  unsigned g;
  int lembrada;
  if (!ajustes_fonte_preparar() || ajustes_fonte_manual() || player_aberto() || aguardandoFonte ||
      !detail_aberto() || preparoRodando) return;
  if (addons_estado() == ADD_BUSCANDO || stream_n() < 1 || stream_n_candidatas() < 1) return;
  ci = cat_item(detail_indice());
  if (!ci || !ci->imdb[0]) return;
  idDoAlvo(ci, alvo, sizeof alvo);
  if (!stream_lista_do_alvo(alvo)) return;
  g = stream_lista_geracao();
  if (!strcmp(feito, alvo) && feitoGer == g) return;
  idBaseDoTitulo(base, sizeof base);
  lembrada = base[0] ? fontepref_escolher(base) : -1;
  stream_preferir(lembrada);
  preparoLembradaRod = lembrada;
  if (iniciarFonteJob(FJOB_PREPARO, g, alvo, 0) != 0) return;   // ocupado: tenta no proximo quadro
  snprintf(feito, sizeof feito, "%s", alvo);
  feitoGer = g;
  preparoRodando = 1;
  marco("fonte: preparando ao abrir o titulo");
}
// AQUECER CONEXOES (aquecer.h). Sem efeito no .wgt (o navegador cuida das dele).
// Na pagina do titulo: a API do debrid; quando a lista de fontes chega ou cresce:
// o host de cada uma das primeiras. Nunca resolve link nem cria arquivo.
static void aquecerAoAbrirTitulo(void) {
#ifndef __EMSCRIPTEN__
  const char *api[4];
  int n;
  if (!ajustes_fonte_aquecer()) return;
  n = debrid_origens(api, 4);
  if (n > 0) aquecer_pedir(api, n);
#endif
}
static void aquecerQuandoChegarem(void) {
#ifndef __EMSCRIPTEN__
  static int visto = -1;
  int sn;
  if (!detail_aberto() || player_aberto() || aguardandoFonte || !ajustes_fonte_aquecer()) { visto = -1; return; }
  sn = stream_n();
  if (sn == visto) return;
  visto = sn;
  if (sn > 0) {
    char u[6][256];
    const char *p[6];
    int k, q = stream_urls_para_aquecer(u, 6);
    for (k = 0; k < q; k++) p[k] = u[k];
    if (q > 0) aquecer_pedir(p, q);
  }
#endif
}
// TOCAR ENQUANTO CONFERE. O fio que confere publica a primeira candidata da
// fila (streams.c); aqui ela entra no player na hora, SEM esperar o veredito.
// Mesmo estado de uma fonte automatica aberta (fonteVODAutomatica, a contagem
// de tentativas e o relogio dos 15 s sem sinal), so que aguardandoFonte fica em
// 2 ate o veredito: e ele que impede o resto do app de trocar a fonte por baixo.
static void abrirAntecipada(int i) {
  const Stream *s = stream_item(i);
  if (!s || !s->url[0]) return;
  antPreAuto = fonteVODAutomatica; antPreTent = fonteVODTentativas; antPreDesde = fonteVODDesde;
  antAberta = i;
  printf("[fonte] tocando enquanto confere: \"%s\" (%d)\n", s->rotulo, i);
  fflush(stdout);
  marco("fonte: tocando enquanto confere");
  stream_definir_atual(i);
  video_definir_dv(s->dolbyVision);
  video_definir_cabecalhos(s->cabecalhos);
  video_definir_mp4(stream_e_mp4(s));
  player_definir_fonte(s->url);
  fonteVODAutomatica = 1;
  fonteVODTentativas++;
  fonteVODDesde = SDL_GetTicks();
  player_definir_tentativa(fonteVODTentativas, VOD_FONTE_MAX_TENTATIVAS);
}
// A candidata aberta nao serve (veredito, erro do player ou outra escolhida):
// o player volta a "abrindo fonte" e a conta volta ao que era. A fila segue.
static void desfazerAntecipada(const char *porque) {
  if (antAberta < 0) return;
  printf("[fonte] aberta antes do veredito e descartada: %s (%d)\n", porque, antAberta);
  fflush(stdout);
  antAberta = -1;
  if (player_aberto()) player_voltar_a_esperar();
  fonteVODAutomatica = antPreAuto; fonteVODTentativas = antPreTent; fonteVODDesde = antPreDesde;
  player_definir_tentativa(antPreTent, antPreTent ? VOD_FONTE_MAX_TENTATIVAS : 0);
}
static void vigiarAntecipada(void) {
  int est = FA_NADA, i;
  if (aguardandoFonte != 2 || fonteEscolhida != -2 || player_id_canal()[0] ||
      !player_aberto() || player_quer_sair()) {
    if (antAberta >= 0 && aguardandoFonte != 2) antAberta = -1;
    return;
  }
  i = stream_antecipada(&est);
  switch (fa_acao(antAberta, i, est, video_falhou() || player_fonte_falhou())) {
    case FA_ACAO_ABRIR: abrirAntecipada(i); break;
    case FA_ACAO_DESFAZER_VEREDITO: desfazerAntecipada("a conferencia reprovou"); break;
    case FA_ACAO_DESFAZER_PLAYER:
      printf("[fonte] erro do player antes do veredito: %s\n", video_erro_texto());
#ifdef NV_ANDROID
      stream_automatico_erro_decoder(antAberta, video_erro_decoder_codigo(), 0);
#endif
      fa_player_falhou(antAberta);
      stream_automatico_excluir(antAberta);
      stream_automatico_excluir_irmas(antAberta);
      desfazerAntecipada("o player errou primeiro");
      break;
    default: break;
  }
}
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
// ESCOLHA MANUAL QUE NAO ABRE: prazo ate o primeiro quadro (fontevolta.h).
// Vence -> o erro de fonte do player, com "A fonte nao respondeu" e as saidas
// de sempre. O automatico ja tem o proprio prazo (tentarProximaFonteVOD).
static void vigiarAberturaManual(void) {
  FontevoltaSinais g;
  Uint32 desde;
  if (!fonteManualDesde) return;
  if (!player_aberto() || player_quer_sair() || player_id_canal()[0] ||
      fonteVODAutomatica || aguardandoFonte != 0 || player_fonte_falhou() || video_pronto()) {
    fonteManualDesde = 0; return;
  }
  desde = SDL_GetTicks() - fonteManualDesde;
  memset(&g, 0, sizeof g);
  g.falhou = video_falhou() || player_fonte_falhou();
  g.pronto = video_pronto();
  g.carregando = player_carregando();
  g.desdeMs = desde;
  // Erro do player no preparo (Media3 2004 etc., video_android.c): sem isto a
  // tela ficava em "Abrindo fonte" ate o prazo, ou para sempre.
  if (video_falhou()) {
    printf("[fonte] escolha manual: erro do player em %u ms (%s)\n", (unsigned)desde, video_erro_texto());
    fflush(stdout);
    fonteManualDesde = 0;
    player_erro_fonte_motivo(i18n("Não foi possível abrir a fonte"), NULL);
    return;
  }
  if (!fontevolta_abertura_vencida(&g, FONTE_MANUAL_PRAZO_MS)) return;
  printf("[fonte] escolha manual nao abriu em %u ms: erro de fonte\n", (unsigned)desde);
  fflush(stdout);
  fonteManualDesde = 0;
  player_erro_fonte_motivo(i18n("A fonte não respondeu a tempo."), NULL);
}
// PRAZO DE ABERTURA (#202, inicio.h): 15 s sem nenhum sinal de vida, 30 s com
// dado chegando — e o curto so quando a proxima candidata nao e pior que esta
// (nunca baixar a qualidade por pressa; sem proxima, 30 s).
static int aberturaVencida(Uint32 desde) {
  static int chaveAt = -1, chaveN = -1, chaveT = -1, semPerda;
  InicioAbertura g;
  int at;
  if (!player_carregando() || desde <= INICIO_ABRE_SEM_SINAL_MS || video_fonte_tocou()) return 0;
  // O CAMINHO DO DV ABRINDO nao e fonte muda: o nosso demux abre o arquivo de
  // novo (22,8 s numa fonte lenta da C9, 08/10) e tem o recuo proprio (fonte
  // lenta ou que nao abre volta ao player da TV, video_dv_recuo_consumir).
  if (video_dv_ativo()) return 0;
  at = stream_atual();
  if (at != chaveAt || stream_n() != chaveN || fonteVODTentativas != chaveT) {
    chaveAt = at; chaveN = stream_n(); chaveT = fonteVODTentativas;
    semPerda = fonteVODTentativas < VOD_FONTE_MAX_TENTATIVAS && stream_proxima_sem_perda(at);
  }
  memset(&g, 0, sizeof g);
  g.desdeMs = desde;
  g.proximaSemPerda = semPerda;
  g.dadoChegando = video_pronto() || video_buffer_fim() > 0.5;
#if defined(NV_ANDROID) || defined(NV_TPK)
  // Trilhas do player ja provam resposta antes do primeiro quadro (DV na TCL).
  g.dadoChegando |= video_n_audio() > 0 || video_n_legenda() > 0;
#endif
  if (!inicio_abre_vencida(&g)) return 0;
  printf("[fonte] sem sinal de abertura em %u ms (prazo %u ms%s)\n", (unsigned)desde,
         inicio_abre_prazo_ms(&g), semPerda ? ", proxima nao e pior" : "");
  fflush(stdout);
  return 1;
}
// A EXPLICACAO DA ESPERA NA ILHA (#202, inicio.h). Por quadro enquanto o cartao
// "Abrindo fonte" esta de pe: o que a abertura esta esperando, em palavras, para
// a pessoa mudar "Espera pelos add-ons" / "Escolha da fonte" se quiser. So
// texto e a dica OK > Ajustes: nunca segura nem decide nada.
// (strcasestr e extensao GNU que a toolchain da webOS nao declara.)
static int contemSemCaixa(const char *h, const char *n) {
  size_t ln = strlen(n);
  for (; *h; h++) if (!strncasecmp(h, n, ln)) return 1;
  return 0;
}
static int fonteViaDebrid(const Stream *s) {
  char u[160];
  if (!s) return 0;
  if (s->infoHash[0]) return 1;
  snprintf(u, sizeof u, "%s", s->url);
  return contemSemCaixa(u, "debrid") || contemSemCaixa(u, "torbox") ||
         contemSemCaixa(u, "premiumize") || contemSemCaixa(u, "/resolve/");
}
static void atualizarMotivoInicio(void) {
  static int ultMotivo = -1;
  static char ultTxt[112];
  InicioSinais sn;
  AddonsInicioInfo ai;
  char txt[112];
  const Stream *st = NULL;
  int m;
  txt[0] = 0;
  if (!player_aberto() || player_id_canal()[0] || !player_carregando() || player_quer_sair()) {
    if (ultTxt[0]) { ultTxt[0] = 0; ultMotivo = -1; player_definir_motivo_inicio(""); }
    return;
  }
  memset(&sn, 0, sizeof sn);
  memset(&ai, 0, sizeof ai);
  // Com a candidata ja aberta antes do veredito (antAberta) a espera e de ABRIR:
  // a conferencia corre por tras e nao e o que a pessoa esta esperando.
  sn.fase = aguardandoFonte == 1 ? INI_FASE_BUSCA
          : aguardandoFonte == 2 && antAberta < 0 ? INI_FASE_VERIFICA : INI_FASE_ABRE;
  sn.desdeMs = sn.fase == INI_FASE_ABRE && fonteVODDesde ? SDL_GetTicks() - fonteVODDesde
                                                          : player_aberto_ha_ms();
  sn.tentativa = fonteVODAutomatica ? fonteVODTentativas : 1;
  if (sn.fase == INI_FASE_BUSCA) {
    addons_inicio_info(&ai);
    sn.pendentes = ai.pendentes; sn.pendenteMudo = ai.pendenteMudo; sn.semResposta = ai.semResposta;
  } else if (sn.fase == INI_FASE_ABRE) {
    st = stream_item(stream_atual());
    sn.viaDebrid = fonteViaDebrid(st);
  }
  m = inicio_motivo(&sn);
  switch (m) {
    case INI_ESPERA_UM:
      snprintf(txt, sizeof txt, i18n("Esperando %s (lento)"), ai.pendente); break;
    case INI_ESPERA_MUDO:
      snprintf(txt, sizeof txt, i18n("%s não respondeu da última vez"), ai.pendente); break;
    case INI_ESPERA_VARIOS:
      snprintf(txt, sizeof txt, i18n("Esperando %d add-ons"), sn.pendentes); break;
    case INI_NAO_RESPONDEU:
      snprintf(txt, sizeof txt, i18n("%s não respondeu"), ai.semRespostaNome); break;
    case INI_VERIFICANDO:
      snprintf(txt, sizeof txt, "%s", i18n("Verificando a fonte…")); break;
    case INI_FALLBACK:
      snprintf(txt, sizeof txt, "%s", i18n("Fonte falhou, tentando a próxima")); break;
    case INI_ABRINDO_DEBRID: {
      char q[16];
      if (st && st->altura >= 2160) snprintf(q, sizeof q, "4K");
      else if (st && st->altura > 0) snprintf(q, sizeof q, "%dp", st->altura);
      else q[0] = 0;
      if (q[0]) snprintf(txt, sizeof txt, i18n("Abrindo %s pelo debrid…"), q);
      else snprintf(txt, sizeof txt, "%s", i18n("Abrindo pelo debrid…"));
      break; }
    default: break;
  }
  player_definir_motivo_inicio(txt);
  // O que a ilha disse vai para o log uma vez por mudanca: e como se prova, num
  // registro, o que a pessoa viu esperando.
  if (m != ultMotivo || strcmp(txt, ultTxt)) {
    ultMotivo = m;
    snprintf(ultTxt, sizeof ultTxt, "%s", txt);
    if (txt[0]) { printf("[fonte] ilha: %s (%u ms)\n", txt, (unsigned)sn.desdeMs); fflush(stdout); }
  }
}
static void tentarProximaFonteVOD(void) {
  Uint32 desde;
  int atual, motivo = 0;
  if (!fonteVODAutomatica || player_id_canal()[0] || !player_aberto() ||
      player_quer_sair() || aguardandoFonte != 0 || !fonteVODDesde) return;
  // Rede caiu com o video andando: a mesma fonte esta sendo recarregada
  // (video_reconexao.h). So vira troca de fonte quando ela desistir (falhou).
  if (video_reconectando()) return;
  desde = SDL_GetTicks() - fonteVODDesde;
  if (video_falhou() || player_fonte_falhou()) motivo = 1;
  // Reloading a source that already played (Dolby Vision path handing back
  // to the TV player, track switch) is not a source that never opened: on
  // the C9 the 30 s deadline, counted from the first open, swapped a playing
  // film to another source and lost the position.
  else if (aberturaVencida(desde)) motivo = 2;
  else if (video_bufferando_ms() > VOD_FONTE_BUFFER_MS) motivo = 3;
  if (!motivo) return;

  atual = stream_atual();
  if (atual >= 0) stream_automatico_excluir(atual);
  // Erro do PLAYER (nao prazo nem buffer): as irmas desta fonte saem junto,
  // senao as vagas que sobram vao todas para links que falham igual.
  if (atual >= 0 && motivo == 1) {
#ifdef NV_ANDROID
    stream_automatico_erro_decoder(atual, video_erro_decoder_codigo(), 0);
#endif
    int k = stream_automatico_excluir_irmas(atual);
    if (k) printf("[fonte] automatico VOD pulou %d link(s) irmao(s) de %d\n", k, atual);
  }
  if (fonteVODTentativas >= VOD_FONTE_MAX_TENTATIVAS ||
      stream_automatico() < 0) {
    printf("[fonte] automatico VOD sem proxima candidata (motivo=%d, %d de %d)\n",
           motivo, fonteVODTentativas, VOD_FONTE_MAX_TENTATIVAS);
    fonteVODAutomatica = 0;
    if (motivo == 2) player_erro_fonte_motivo(i18n("A fonte não respondeu a tempo."), NULL);
    else player_erro_fonte();
    return;
  }
  if (!pedirProximaFonteVOD()) return;
  player_definir_tentativa(fonteVODTentativas + 1, VOD_FONTE_MAX_TENTATIVAS);
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
  if (player_aberto() || player_retido()) player_encerrar();
  diagnostico_cancelar_vazao();
  player_abrir(ni, NULL);
  // cat_acrescentar e cat_definir_tudo correm juntos: se uma republicacao
  // atravessou os dois, ni ja nao e o canal. A marca garante a sessao.
  player_marcar_canal(it);
  novaGeracaoFonte();        // invalida qualquer resolver do canal anterior
  limparFontePendente();
  canalFonteIdx = -1;             // canal novo: watchdog arma de novo no fim da busca
  canalViaProxy = 0;
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

// O CANAL NO AR VAI PARA O GUIA, sem recarregar (25/09/2026). O mesmo fluxo
// muda so de retangulo: da tela cheia (ou do PiP de canto) para o preview
// 800x450, com a janela andando em degraus (player_minimizar_para_guia). O
// guia foca a linha do canal e deixa de seguir o foco com o preview — so OK
// em outro canal troca o que toca. E o caminho de: Voltar num canal aberto
// pelo guia, Azul na faixa do mini guia, e Guia na barra com o PiP no ar.
// UM TITULO, UM LUGAR (cwretido.h). Quem segura o titulo que acabou de sair
// do player: o cartao da ilha (relogio ligado) ou a faixa "Retomar agora"
// (desligado). Uma comparacao de string por quadro. O titulo SOLTO (cartao
// dispensado, vencido, trocado por outro titulo, relogio trocado) volta a
// valer em "Continuar assistindo" e vai para a FRENTE dela, no mesmo quadro e
// sem rede (cwfrente.h) — a home so remonta a fileira, sem refazer o resto.
static void cwRetidoSincronizar(void) {
  char antes[64];
  cw_retido_tambem_definir(ajustes_cw_retido_tambem());
  snprintf(antes, sizeof antes, "%s", cw_retido());
  if (!cw_retido_definir(cw_retido_escolher(ajustes_relogio_ligado(), ilhacart_vivo_imdb(),
                                            home_retomar_imdb())))
    return;
  if (antes[0] && !cw_retido_exclui(antes)) {
    int i = cat_indice_por_imdb(antes);
    if (i >= 0) desc_continuar_otimista(i);
  }
}

static void guiaComCanalNoAr(void) {
  char id[80];
  float x, y, w, h;
  snprintf(id, sizeof id, "%s", player_id_canal());
  guia_preview_rect(&x, &y, &w, &h);
  if (player_mini_ativo()) player_mini_no_guia(x, y, w, h);
  else player_minimizar_para_guia(x, y, w, h);
  if (tela != TELA_GUIA || !guia_aberta()) {
    if (tela == TELA_GUIA) guia_abrir();
    else trocarTela(TELA_GUIA);
  }
  guia_adotar_canal(id);
  printf("[guia] canal no ar foi para o preview, sem recarregar\n");
  fflush(stdout);
}

// A home carregou? Sem arte no pacote ela nao carrega, e ate agora isso
// DERRUBAVA o app: app_iniciar devolvia 0 e o main saia com codigo 1. Num
// pacote de dono isso nunca acontecia porque a arte ia junto; num pacote
// distribuivel, que nao pode levar arte nem credencial de ninguem, esse era o
// comportamento da PRIMEIRA execucao de todo mundo — o app abria e fechava,
// antes mesmo da tela de login.
static int homePronta;

static void trocarSalvosDePerfil(void) {
  int i, n = salvos_n();
  char (*antes)[24] = n > 0 ? malloc(sizeof *antes * (size_t)n) : NULL;
  for (i = 0; antes && i < n; i++)
    snprintf(antes[i], sizeof antes[i], "%s", salvos_item(i)->id);
  salvos_perfil(perfis_ativo());
  for (i = 0; antes && i < n; i++)
    if (!salvos_tem(antes[i])) cat_definir_na_lista_imdb(antes[i], 0);
  free(antes);
  salvos_aplicar_catalogo();
}

// Cada "+"/tirar (salvos_definir, de qualquer tela) entra no jornal da conta e
// sai num fio. Sem conta, contapend nao registra nada.
static void salvoParaConta(const CatItem *ci, int salvo) {
  if (!ci || !sessao_logada()) return;
  if (contapend_lista(ci->imdb, ci->tipo, ci->titulo, ci->poster, salvo))
    contapend_chutar();
}

int app_iniciar(const char *dirArte) {
#ifdef NV_LINUX_DESKTOP
  const char *preview = getenv("NUVIO_UI_PREVIEW");
  uiPreview = preview && !strcmp(preview, "1");
#endif
  arranque_etapa("app_iniciar/ajustes_recursos");
  ajustes_recursos(dirArte);
  arranque_etapa("app_iniciar/login_recursos");
  login_recursos(dirArte);
  arranque_etapa("app_iniciar/diagnostico_recuperar_checkpoint");
  diagnostico_recuperar_checkpoint();
  arranque_etapa("app_iniciar/stream_fit_fonte_metadados");
  stream_fit_fonte_metadados(fitRuntimeMeta);
  arranque_etapa("app_iniciar/home_iniciar");
  homePronta = home_iniciar(dirArte);
  arranque_etapa("app_iniciar/novidades148_dir");
  novidades148_dir(dirArte);
  novidades170_dir(dirArte);
  novidades180_dir(dirArte);
  arranque_etapa("app_iniciar/novidades20_dir");
  novidades20_dir(dirArte);
  novidades201_dir(dirArte);
  novidades202_dir(dirArte);
  novcartao_dir(dirArte);
  arranque_etapa("app_iniciar/apoio_dir");
  apoio_dir(dirArte);
  if (!homePronta)
    printf("[app] sem arte no pacote: a home so aparece depois do primeiro sync\n");
  arranque_etapa("app_iniciar/menu_iniciar");
  menu_iniciar();
  arranque_etapa("app_iniciar/perfil_iniciar");
  perfil_iniciar();
  // ANTES do primeiro sync e da primeira descoberta: salvos_aplicar_catalogo
  // marca `naLista` no catalogo do cache, entao o painel e a Biblioteca ja
  // abrem certos no primeiro quadro. Ler depois faria a lista local piscar.
  // A LISTA E DO PERFIL (salvos-p<N>.txt); perfis_carregar_ativo ja rodou.
  salvos_perfil(perfis_ativo());
  arranque_etapa("app_iniciar/salvos_iniciar");
  salvos_iniciar();
  // O JORNAL DA CONTA (contapend.h): todo "+"/tirar vira entrada por perfil, e
  // o pull da biblioteca e dos vistos respeita o que a pessoa mudou aqui.
  salvos_ao_definir(salvoParaConta);
  contalib_filtros(contapend_lista_oculta, contapend_visto_oculto);
  // DESMARCAR GANHA (vistonao.h): o que a pessoa desmarcou nesta TV fica
  // guardado por perfil e barra o "visto" de qualquer fonte (Trakt, conta,
  // jornal) que nao seja mais novo que o gesto. Antes da primeira leitura.
  vistoep_lapides(vistonao_barra, vistonao_gesto);
  // E o "a seguir" do Continuar assistindo recua para o primeiro desmarcado.
  desc_lapides_primeira(vistonao_primeira);
  // AS LISTAS FIXADAS, PELO MESMO MOTIVO E ANTES DA PRIMEIRA HOME. Uma lista do
  // Trakt que a Biblioteca levou para a Home so vira fileira quando lst_iniciar
  // reinjeta a pasta dela em colecoes.c; chamando isto so ao ABRIR a Biblioteca,
  // a fileira sumiria da home a cada reinicio ate a pessoa passar por la.
  arranque_etapa("app_iniciar/lst_iniciar");
  lst_iniciar();
  arranque_etapa("app_iniciar/agenda_iniciar");
  agenda_iniciar();
  // A FONTE LEMBRADA, do mesmo jeito e pelo mesmo motivo: ela e lida antes do
  // primeiro Reproduzir, que pode acontecer segundos depois do arranque quando
  // a pessoa abre direto no "Continuar assistindo".
  arranque_etapa("app_iniciar/ondever_iniciar");
  ondever_iniciar();
  arranque_etapa("app_iniciar/fontepref_iniciar");
  fontepref_iniciar();
  arranque_etapa("app_iniciar/addonstats_ler");
  addonstats_ler();   // #202: latencia de cada add-on NESTA TV (addonstats.h)
  // A ARTE ESCOLHIDA A MAO (#142) tambem, antes do primeiro destaque: sem ela
  // lida, o hero abriria na foto automatica e trocaria no quadro seguinte.
  arranque_etapa("app_iniciar/arteesc_iniciar");
  arteesc_iniciar();
  // MESMA RAZAO, OUTRA LISTA: o cache de recomendacoes guarda titulo e poster,
  // entao a aba Social do painel AZUL desenha no primeiro quadro, antes de
  // existir catalogo e antes de a rede responder. Isto nao abre conexao — quem
  // faz isso e recomenda_verificar, la embaixo, com a home ja de pe.
  arranque_etapa("app_iniciar/recomenda_iniciar");
  recomenda_iniciar();
  arranque_etapa("app_iniciar/ilhasinais_iniciar");
  ilhasinais_iniciar();  // a saude da rede passa a ouvir os pedidos (ilha, 02/10)
  arranque_etapa("app_iniciar/atividade_iniciar");
  atividade_iniciar();   // a fila de POST /v1/atividade que nao saiu (atividade.h)
  // O envio da atividade segue o nivel de privacidade (alcance) do Social: a
  // funcao e chamada agora e a cada mudanca; "nao perguntado" chega como 0.
  recomenda_ao_mudar_alcance(atividade_definir_permitido);
  // Sem conta, o app abre no login. Com sessao gravada ele nem passa por ela —
  // pedir o codigo de novo a cada arranque seria o mesmo que nao ter gravado.
#ifdef NV_LINUX_DESKTOP
  if (uiPreview) {
    tela = TELA_HOME;
    printf("[linux] UI preview: no account sync or video playback\n");
  } else
#endif
  if (sessao_logada()) {
    tela = TELA_HOME;
    // Com sessao gravada o ciclo comeca no arranque: e ele que traz os addons
    // e o Trakt da pessoa, sem os quais a home mostra so o que veio no pacote.
    arranque_etapa("app_iniciar/sync_iniciar");
    arranque_etapa("app_iniciar/sync_iniciar");
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
      arranque_etapa("app_iniciar/perfilsel_iniciar");
      perfilsel_iniciar();
    } else {
      // NO CHOOSER = THE CHOICE IS ALREADY MADE (#228). A restored session
      // with one profile never passed through perfilsel, so its `concluido`
      // stayed 0 for the whole session, and home_trailer_passo (which requires
      // perfilsel_concluido) logged "autoplay gate=top-overlay" forever: the
      // hero and focused-poster trailers never started, while the title page
      // (no such guard) played. Registros 23154 (Q80A) and 41271 (S90C): one
      // profile, gate stuck on top-overlay. If the sync later finds a second
      // profile, the chooser opens via perfilsel_iniciar, which resets this.
      arranque_etapa("app_iniciar/perfilsel_continuar_ativo");
      perfilsel_continuar_ativo();
    }
  } else {
    tela = TELA_LOGIN;
    arranque_etapa("app_iniciar/login_iniciar");
    login_iniciar();
  }
  arranque_etapa("app_iniciar/fim");
  return 1;
}

// --- SPOTLIGHT (spotlight.h) ---------------------------------------------------
// Atalho: F6 (microfone traduzido por plataforma), F5 (amarela traduzida) e a
// AMARELA da LG direto — menos no Guia, onde ela alterna a vista. Nao abre por
// cima do player em tela cheia nem do painel de Salvos (que tem fundo parado
// proprio, no mesmo FBO).
static int spotAtalho(const SDL_Event *e) {
  SDL_Keycode k;
  if (e->type != SDL_KEYDOWN) return 0;
  k = e->key.keysym.sym;
  if (k == SPOT_TECLA_VOZ || k == SPOT_TECLA_ABRIR) return 1;
  return e->key.keysym.scancode == NV_SCANCODE_YELLOW && tela != TELA_GUIA;
}
static int spotPode(void) {
  return tela != TELA_LOGIN && tela != TELA_ESCOLHA_PERFIL && !player_aberto() &&
         !spainel_aberto() && !guia_overlay_aberta();
}
static void spotAbrir(int voz) {
  static Uint32 ultimo;
  Uint32 t = SDL_GetTicks();
  // Segurar a tecla manda KEYDOWNs separados (ver a AZUL em app_evento): um so abre.
  if (t - ultimo < 400) return;
  ultimo = t;
  if (menu_aberto()) menu_fechar();
  if (ctx_aberto()) return;
  spot_abrir(voz);
}
// Pessoa do TMDB esperando a pagina do titulo dela chegar (ver SPOT_PESSOA).
// Vale SPOT_PESSOA_PRAZO_MS: um titulo que chegue depois disso e outro pedido.
#define SPOT_PESSOA_PRAZO_MS 20000u
static SpotPedido spotPessoaDepois;
static Uint32 spotPessoaDesde;
// Abre a filmografia da pessoa: do Spotlight e da Busca (busca_pediu_pessoa).
static void abrirPessoa(const SpotPedido *pp) {
  SpotPedido p = *pp;
  // A pagina do titulo de onde a pessoa veio, com a filmografia por cima.
  if (p.indice >= 0) {
    abrirPorIndice(p.indice);
    detail_mostrar_pessoa(p.tmdb, p.nome, p.arte);
    return;
  }
  // Pessoa do TMDB (spotpessoa.h): a filmografia mora na pagina de um
  // titulo, entao abre a do primeiro titulo pelo qual ela e conhecida.
  // No catalogo ja abre agora; fora dele a descoberta busca o meta e
  // trocaDeTituloSeSolicitada mostra a pessoa quando a pagina abrir.
  { int i, idx = -1;
    for (i = 0; i < cat_n() && idx < 0; i++) {
      const CatItem *ci = cat_item(i);
      if (ci && ci->tmdb == p.tituloTmdb && strcmp(ci->tipo, "channel")) idx = i;
    }
    if (idx >= 0) { abrirPorIndice(idx); detail_mostrar_pessoa(p.tmdb, p.nome, p.arte); }
    else if (p.tituloTmdb > 0) {
      spotPessoaDepois = p;
      spotPessoaDesde = SDL_GetTicks();
      desc_pedir_titulo_tmdb(p.tituloTmdb, p.tituloTipo);
    } }
}
// O que a pessoa escolheu no Spotlight, ja com a caixa fechada.
static void spotAtender(void) {
  SpotPedido p;
  if (!spot_pediu(&p)) return;
  switch (p.tipo) {
    case SPOT_TITULO:
      abrirPorIndice(p.indice);
      break;
    case SPOT_PESSOA:
      abrirPessoa(&p);
      break;
    case SPOT_COLECAO: {
      const ColFolder *f = col_folder(p.indice);
      if (f) { detail_fechar(); vertudo_colecao(f); }
      break; }
    case SPOT_CATALOGO: {
      const CatFileira *cf = cat_fileira(p.indice);
      if (cf && cf->base[0]) { detail_fechar(); vertudo_abrir(cf->base, cf->tipo, cf->catId, cf->titulo); }
      break; }
    case SPOT_ADDONS:
      detail_fechar();
      addonsui_abrir();
      trocarTela(TELA_ADDONS);
      break;
    case SPOT_GUIA:
      if (tela != TELA_AJUSTES) { ajustes_abrir_no_guia(0); trocarTela(TELA_AJUSTES); menu_definir_destino(MENU_AJUSTES); }
      ajustes_guia_ir(p.indice);
      break;
    case SPOT_AJUSTE:
      detail_fechar();
      ajustes_abrir_opcao(p.indice);
      if (tela == TELA_AJUSTES) ajustes_iniciar();
      else trocarTela(TELA_AJUSTES);
      menu_definir_destino(MENU_AJUSTES);
      break;
    case SPOT_CANAL: {
      CatItem it;
      if (aguardandoFonte != 2 && guia_item_do_canal(p.id, p.nome, p.base, &it)) tocarCanal(&it);
      break; }
    default: break;
  }
  printf("[spotlight] escolheu tipo=%d\n", p.tipo);
  fflush(stdout);
}

int app_no_login(void) { return tela == TELA_LOGIN; }

int app_na_home(void) {
  return tela == TELA_HOME && homePronta && !player_aberto() && !detail_aberto() &&
         !novidades170_aberto() && !novidades180_aberto() && !novidades20_aberto() &&
         !novcartao_aberto();
}

// ONDE A AZUL/CH+ ABRE O PAINEL DE SALVOS/AVISOS (dono, 04/10: "tem que dar
// para abrir as notificacoes e a sidebar social da parte de titulos tambem e em
// outros lugares"). Antes so na Home. Agora tambem na pagina do titulo (e a
// ficha da pessoa, que vive nela), "Ver tudo"/colecao e nas telas de menu
// (Explorar, Busca, Biblioteca, Perfil, Social, Add-ons, Plugins, Agenda).
// Nunca sobre o player, o guia, Ajustes ou diagnosticos, e nunca com uma
// folha/menu do cartaz por cima: elas tem a tecla. A tecla "S" de teclado de
// Mac nao vale com um campo de texto ativo (e a letra, nao a AZUL).
static int azulPodeAbrirSalvos(const SDL_Event *e) {
  if (player_aberto() || player_mini_ativo() || menu_aberto() || ctx_aberto() ||
      faixas_aberta() || episodios_aberto() || stream_folha_aberta() ||
      guia_overlay_aberta() || pessoas_aberto() || recomenda_aberta() || recenviar_aberto())
    return 0;
  // So no teclado de verdade (Mac/desktop). No Android a AZUL, o CH+ e o
  // MENU/BOOKMARK CHEGAM como "s" (NuvioActivity/main.c) e o SDL pode deixar o
  // text input ligado: a regra recusava o atalho e o CH+ parou de abrir a ilha.
#ifndef NV_ANDROID
  if (e->key.keysym.sym == SDLK_s && e->key.keysym.scancode != NV_SCANCODE_BLUE &&
      SDL_IsTextInputActive()) return 0;
#endif
  switch (tela) {
    case TELA_HOME: case TELA_EXPLORAR: case TELA_BUSCA: case TELA_BIBLIOTECA:
    case TELA_PERFIL: case TELA_SOCIAL: case TELA_ADDONS: case TELA_PLUGINS:
    case TELA_AGENDA: return 1;
    default: return 0;
  }
}

// Abre/fecha o painel de Salvos+Avisos pelo caminho da AZUL. `comModal` 0 e o
// "Cima na pagina do titulo": abre direto, sem o modal de cartao da ilha.
static void alternarSalvos(int comModal) {
  static Uint32 ultimoToque;
  Uint32 agoraTecla = SDL_GetTicks();
  float ix, iy, iw, ih;
  if (agoraTecla - ultimoToque < 400) return;   // repeticao do firmware
  ultimoToque = agoraTecla;
  if (spainel_aberto()) spainel_fechar();
  else if (comModal && (ilha_cartao_na_tela() || ilha_atividade_expansivel()) && ilha_modal_abrir()) {}
  else if (ajustes_relogio_ligado() && ilha_rect(&ix, &iy, &iw, &ih)) spainel_abrir_de(ix, iy, iw, ih);
  else spainel_abrir();
}

// OK NA ILHA DE AMIGOS DA PAGINA DO TITULO (detail.c): o mesmo painel da tecla
// azul (nascendo da ilha do relogio quando ela esta na tela), ja na aba
// Atividade e no primeiro evento de amigo daquele titulo.
static void abrirSocialNoTitulo(const char *imdb) {
  float ix, iy, iw, ih;
  if (!spainel_aberto()) {
    if (ajustes_relogio_ligado() && ilha_rect(&ix, &iy, &iw, &ih)) spainel_abrir_de(ix, iy, iw, ih);
    else spainel_abrir();
  }
  spainel_abrir_titulo(imdb);
}

// O circular "Explorar" da pagina do titulo: a pagina fecha e a toca do coelho
// comeca nele. Voltar no primeiro degrau da toca pede a pagina de volta
// (explorar_pediu_abrir), com a grade de climas por baixo.
static void abrirExplorarNoTitulo(void) {
  MapaObra o;
  if (!mapa_obra_do_catalogo(detail_indice(), &o)) return;
  detail_fechar_seco();
  trocarTela(TELA_EXPLORAR);
  menu_definir_destino(MENU_EXPLORAR);
  explorar_abrir_titulo(&o);
}

void app_evento(const SDL_Event *e) {
  if (e->type == SDL_QUIT) { sair = 1; return; }
  ilhacart_validar_identidade();
  // Qualquer gesto da pessoa: a atividade ao vivo da ilha conta 30 min daqui.
  if (e->type == SDL_KEYDOWN || e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEWHEEL)
    ilhacart_tecla(SDL_GetTicks());

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
  // O painel de envio do registro (Ajustes > Enviar registro, ou o "Enviar
  // agora" do painel): modal, come o teclado enquanto aberto.
  if (registro_envio_evento(e)) return;
  // A CENTRAL DE CONTROLE (central.h, CH+ segurado) e uma camada: aberta, come
  // o teclado todo, por cima do player, das folhas e das ilhas.
  if (central_aberta()) { central_evento(e); return; }
  // O MODAL DA ILHA (ilha.h) e uma camada: aberto, come o teclado todo.
  if (ilha_evento(e)) return;
  // A CENTRAL DE AVISOS vem logo depois do painel de log: com o toast na tela
  // AZUL/CH+ abrem a lista, e com a lista aberta ela come o teclado. Fora
  // desses dois estados ela nao toca em nada (ver avisos.h).
  if (avisos_evento(e)) return;
  // O CARTAO DO LEMBRETE DE PROGRAMA, em qualquer tela: com ele de pe as
  // setas laterais, o OK e o Voltar sao dele (ver guialembrete.h).
  if (glem_evento(e)) return;
  // O CARTAO DO CELULAR (celbotao.h), aberto de qualquer campo de texto: come
  // o teclado todo ate fechar.
  if (celb_evento(e)) return;
  // O GUIA DA 2.0 (novidades20.h) e tela inteira e come o teclado todo,
  // inclusive a AZUL (pula para o resumo). "Abrir o Guia de uso" leva a
  // Ajustes › Sobre e ajuda › Guia de uso.
  // O CARTAO DA 2.0.1 (novidades201.h): come o teclado todo; "Abrir a
  // Central" fecha o cartao e abre a Central de controle no lugar.
  // O DA 2.0.2 (novidades202.h) e igual, sem a Central.
  // O CARTAO DA VERSAO ATUAL (novidades_cartao.h, 2.0.3 em diante) tambem.
  if (novcartao_aberto()) {
    novcartao_evento(e);
    return;
  }
  if (novidades202_aberto()) {
    novidades202_evento(e);
    return;
  }
  if (novidades201_aberto()) {
    novidades201_evento(e);
    if (novidades201_pedido() == N201_PEDIU_CENTRAL && app_central_pode()) central_abrir();
    return;
  }
  if (novidades20_aberto()) {
    novidades20_evento(e);
    if (novidades20_pedido() == N20_PEDIU_GUIA) {
      menu_fechar();
      ajustes_abrir_no_guia(0);
      trocarTela(TELA_AJUSTES);
      menu_definir_destino(MENU_AJUSTES);
    }
    return;
  }

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

  // O SPOTLIGHT ABERTO e dono de todo o teclado: e uma caixa modal por cima da
  // tela, e o que ele escolhe so e feito depois de ele fechar.
  if (spot_aberto()) { spot_evento(e); spotAtender(); return; }

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
  //
  // CH+ NA LG (dono, 07/10: "junta o CH+ e o AZUL"): nas outras plataformas o
  // CH+ chega aqui como AZUL/"s" (main.c, remapCanal) e o toque curto sempre
  // abriu este painel; segurado abre a central. Na LG o CH+ curto so valia com
  // um cartao na ilha — e a AZUL era a unica porta. Agora o CH+ curto e o
  // mesmo toque, e segurado segue para a central (central.h). Com canal na
  // tela (zap, PiP) ele nao chega aqui: azulPodeAbrirSalvos recusa.
  //
  // COM O RELOGIO NA TELA a mesma tecla passa pela ilha (ilha.h, pedido do
  // dono de 01/10): com um cartao na pilula (atividade ao vivo, estreia) ela
  // abre o MODAL — e ai o CH+ tambem, como no toast da central —; sem cartao
  // o painel NASCE da pilula (spainel_abrir_de) em vez de entrar pela borda.
  // Relogio desligado: o de sempre.
  if(e->type==SDL_KEYDOWN && !e->key.repeat &&
     (e->key.keysym.sym==SDLK_s || e->key.keysym.scancode==NV_SCANCODE_BLUE ||
      e->key.keysym.scancode==NV_SCANCODE_CH_UP || e->key.keysym.sym==SDLK_PAGEUP) &&
     azulPodeAbrirSalvos(e)) {
    alternarSalvos(1);
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
  // O DA 1.4.8 abre Ajustes (na cor) ou o teste de velocidade AQUI, no mesmo
  // evento, pelo mesmo motivo do da 1.4.2 logo acima.
  if (novidades148_aberto()) {
    novidades148_evento(e);
    switch (novidades148_pedido()) {
      case N148_PEDIU_COR:
        ajustes_abrir_na_cor();
        trocarTela(TELA_AJUSTES);
        menu_definir_destino(MENU_AJUSTES);
        break;
      case N148_PEDIU_VELOCIDADE:
        // Mesmo caminho do atalho de Ajustes, mas o Voltar devolve a home, de
        // onde a pessoa veio (diagDaHome), como o do cartao da 1.4.2.
        diagDaHome = 1;
        diagnostico_abrir_velocidade();
        trocarTela(TELA_DIAGNOSTICO);
        menu_definir_destino(MENU_AJUSTES);
        break;
      default: break;
    }
    return;
  }
  // O DA 1.7 (no lugar do da 1.6.6, que nunca saiu): "Abrir o Spotlight" fecha
  // o cartao e abre a busca por cima da home. Aqui, no mesmo evento, pelo
  // mesmo motivo do da 1.4.2 acima.
  // O DA 1.8.0, o unico cartao de primeira vez daqui em diante
  // (novidadesfila.h). "Abrir o guia" leva a Ajustes › Sobre e ajuda › Guia
  // de uso, com "Voltar às novidades"; "Vidro ou sólido" pousa na Interface de
  // vidro. Aqui, no mesmo evento, pelo mesmo motivo do da 1.4.2.
  if (novidades180_aberto()) {
    novidades180_evento(e);
    switch (novidades180_pedido()) {
      case N180_PEDIU_GUIA:
        ajustes_abrir_no_guia(1);
        trocarTela(TELA_AJUSTES);
        menu_definir_destino(MENU_AJUSTES);
        break;
      case N180_PEDIU_VIDRO:
        ajustes_abrir_no_vidro();
        trocarTela(TELA_AJUSTES);
        menu_definir_destino(MENU_AJUSTES);
        break;
      default: break;
    }
    return;
  }
  if (novidades170_aberto()) {
    novidades170_evento(e);
    if (novidades170_pedido() == N170_PEDIU_SPOT && spotPode()) spotAbrir(0);
    return;
  }
  if (novidades1312_aberto()) { novidades1312_evento(e); return; }
  if (telemetria_aberto()) { telemetria_evento(e); return; }
  // O explicador do Social e da mesma familia, e come esquerda/direita:
  // deixar a tecla vazar para a home moveria o foco dela debaixo do cartao.
  if (recintro_aberto()) { recintro_evento(e); return; }
  if (atualizacao_aberta()) { atualizacao_evento(e); return; }
  if (agendaviso_aberto()) { agendaviso_evento(e); return; }
  if (recomenda_tecla(e)) return;
  // A MODAL DE RECOMENDAR fica acima do detalhe, do menu do cartaz e do
  // painel da tecla AZUL — as tres portas que a abrem. Abaixo do cartao de
  // aviso, que e uma pergunta sobre outra recomendacao.
  if (recenviar_aberto()) { recenviar_evento(e); return; }
  // ENCONTRAR PESSOAS (busca, perfil, pedidos): mesma altura da modal de
  // recomendar — abre da aba Social e de Ajustes, e devolve o foco a quem abriu.
  if (pessoas_aberto()) { pessoas_evento(e); return; }

  // A folha de fontes fica acima de tudo: ela e uma pergunta, e enquanto ela
  // esta em pe nada mais deve responder ao D-pad.
  if (faixas_pilula_tecla(e)) return;
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
  if (player_mini_ativo() && !player_mini_no_guia_ativo() && e->type == SDL_KEYDOWN) {
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
  if (spotAtalho(e) && spotPode()) { spotAbrir(e->key.keysym.sym == SPOT_TECLA_VOZ); return; }
  if (player_aberto()) { player_evento(e); return; }
  // A BARRA LATERAL POR CIMA DA PAGINA DO TITULO (dono, 03/10: "pode abrir por
  // cima"): ESQUERDA no comeco de uma fileira pede a barra e a pagina fica
  // viva embaixo. Aberta, a tecla e dela; recolhendo, ja volta para a pagina.
  // O PAINEL DE SALVOS/AVISOS POR CIMA DA PAGINA DO TITULO: aberto, a tecla e
  // dele; Voltar fecha e o foco da pagina fica onde estava (ela nao ve nada).
  if (detail_aberto() && spainel_aberto()) {
    if (ctx_aberto()) ctx_evento(e); else spainel_evento(e);
    return;
  }
  // O MENU DO CARTAZ ABERTO DE DENTRO DA PAGINA DO TITULO (segurar OK nas
  // Recomendacoes, na filmografia ou na lista da saga): modal, por cima dela.
  if (detail_aberto() && ctx_aberto()) { ctx_evento(e); return; }
  if (detail_aberto() && menu_aberto()) { menu_evento(e); return; }
  if (detail_aberto()) {
    detail_evento(e);
    // CIMA no alto da pagina (controle sem tecla colorida): o mesmo painel.
    if (detail_aberto() && detail_pediu_social() && azulPodeAbrirSalvos(e)) alternarSalvos(0);
    // No MESMO evento, como a home: diferido um quadro, as teclas seguintes
    // do controle ainda cairiam na pagina.
    if (detail_aberto() && detail_pediu_menu() && sidebar_permitida()) {
      menu_abrir_sobre(1);
      saiuPorEsquerda = 0;
    }
    { char im[24];
      if (detail_aberto() && detail_pediu_amigos(im, sizeof im)) abrirSocialNoTitulo(im); }
    if (detail_aberto() && detail_pediu_explorar()) abrirExplorarNoTitulo();
    return;
  }
  // O MENU DO CARTAZ ABERTO PELO PAINEL (segurar OK numa linha de Salvos)
  // fica por cima dele: com os dois no ar, a tecla e do menu.
  if (spainel_aberto()) {
    if (ctx_aberto()) ctx_evento(e); else spainel_evento(e);
    return;
  }
  if (menu_aberto())   { menu_evento(e);   return; }
  // "Ver tudo" fica ENTRE a home e o detalhe: ela cobre a home e o detalhe
  // cobre ela. Por isso vem depois do detalhe e antes do roteamento por tela.
  // O menu do cartaz fica ACIMA de tudo que a home mostra: ele e modal.
  if (ctx_aberto())     { ctx_evento(e);     return; }
  if (vertudo_aberta()) {
    vertudo_evento(e);
    // ESQUERDA na coluna 0 de "Ver tudo"/colecao: a barra por cima dela.
    // Por cima: DIREITA/Voltar voltam para a lista, um destino a fecha.
    if (vertudo_aberta() && vertudo_pediu_menu() && sidebar_permitida()) {
      menu_abrir_sobre(0);
      saiuPorEsquerda = 0;
    }
    return;
  }

  switch (tela) {
    case TELA_EXPLORAR:   explorar_evento(e);   break;
    case TELA_GUIA:       guia_evento(e);       break;
    case TELA_BUSCA:      busca_evento(e);      break;
    case TELA_BIBLIOTECA: biblioteca_evento(e); break;
    case TELA_AGENDA:     agendaui_evento(e);   break;
    case TELA_PERFIL:     perfil_evento(e);     break;
    case TELA_SOCIAL:     amigoperfil_evento(e); break;
    case TELA_ADDONS:     addonsui_evento(e);   break;
    case TELA_PLUGINS:    pluginsui_evento(e);  break;
    case TELA_AJUSTES:    ajustes_evento(e);    break;
    case TELA_DIAGNOSTICO: diagnostico_evento(e); break;
    case TELA_LIVETV_DIAG: livetvdiag_evento(e); break;
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
  // O ESTADO DE PARTIDA e o mesmo que o olho desenha (cat_visto, #212). Era
  // `progresso >= 90`: filme visto no Trakt mas sem progresso local mostrava o
  // olho aberto e o toque "marcava" de novo em vez de desmarcar.
  { int visto = cat_visto(c);
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
      // NA HORA, com ou sem Trakt (#212): o olho e o selo leem o historico, e
      // esperar o 2xx deixava o olho no estado velho por um ou dois segundos
      // — ou para sempre, se o historico conhecido dizia o contrario do
      // progresso. O 2xx do Trakt (trakt.c) reescreve o mesmo valor.
      cat_historico_definir_id(c->imdb, c->tipo, !visto);
    }
    printf("[app] assistido %s: %s\n", visto ? "desmarcado" : "marcado",
           c->titulo); fflush(stdout); }
}

static void trocaDeTituloSeSolicitada(void) {
  int alvo = detail_pediu_abrir();
  if (alvo >= 0) { detail_volta_notar(alvo); abrirPorIndice(alvo); return; }
  // Titulo que veio de FORA do catalogo: a descoberta buscou o meta num fio e
  // avisa aqui quando ele entrou. Abrir no fio da rede seria mexer na tela de
  // outro fio; este e o unico lugar que abre titulo.
  { int novo = desc_titulo_pronto();
    // Pedido de dentro do PLAYER (relacionado que nao esta no catalogo): o
    // detalhe abria por baixo do video e ninguem via — "clico e nao faz nada".
    // O caminho do titulo que JA esta no catalogo (posplay_pediu_titulo)
    // encerra o player antes; este faz o mesmo.
    // Pedido de um MENU (segurar OK numa lista): o titulo chegou, abre o menu.
    if (novo >= 0 && ctxlista_tomar(novo)) novo = -1;
    if (novo >= 0) {
      if (player_aberto() || player_retido()) player_encerrar();
      detail_volta_notar(novo);
      abrirPorIndice(novo);
      if (spotPessoaDepois.tmdb > 0) {
        if (SDL_GetTicks() - spotPessoaDesde < SPOT_PESSOA_PRAZO_MS)
          detail_mostrar_pessoa(spotPessoaDepois.tmdb, spotPessoaDepois.nome, spotPessoaDepois.arte);
        spotPessoaDepois.tmdb = 0;
      }
    } }
}

// EXTRAS DE LEGENDA DA FONTE QUE TOCA (#201). Quando o video de uma fonte de
// addon de fato comeca, a busca de legendas do titulo e refeita com o nome, o
// tamanho e o hash do arquivo (addons_legendas_fonte). Um ponto so, olhando o
// que toca, em vez de um aviso em cada caminho que escolhe fonte (automatica,
// manual, guardada, torrent resolvido): esquecer um deles seria o mesmo
// defeito de antes so naquele caminho. Esperar o video (player_com_video)
// tambem poupa a busca das candidatas que o automatico testa e descarta.
static void legendasDaFonte(void) {
  static char ultimo[4096 + 64];
  char chave[sizeof ultimo], alvo[64];
  const Stream *s;
  const CatItem *c;
  if (!player_aberto() || player_id_canal()[0] || !player_com_video()) return;
  s = stream_item(stream_atual());
  c = cat_item(player_indice());
  if (!s || !c || !c->imdb[0]) return;
  alvoPlayer(alvo, sizeof alvo);
  if (!alvo[0]) return;
  snprintf(chave, sizeof chave, "%s|%s", alvo, s->url[0] ? s->url : s->infoHash);
  if (!strcmp(chave, ultimo)) return;
  snprintf(ultimo, sizeof ultimo, "%s", chave);
  addons_legendas_fonte(alvo, c->tipo, s->arquivo, (unsigned long long)s->tamanhoBytes,
                        s->videoHash, s->url, s->cabecalhos[0] != 0);
}

// TRAILER NO DESTAQUE: so com a home na frente de tudo. A lista e a mesma
// ordem de app_evento — o que come tecla antes da home tambem esta na frente
// dela na tela. Devolve NULL com a home no topo, ou o NOME do primeiro
// bloqueio: o registro dizia so "top-overlay" e escondeu por semanas que era a
// escolha de perfil que nunca concluia (#228).
static const char *homeTrailerBloqueio(void) {
  if (tela != TELA_HOME) return "screen";
  if (!homePronta) return "home-not-ready";
  if (!login_concluido()) return "login";
  if (!perfilsel_concluido()) return "profile-choice";
  if (player_aberto() || player_retido() || player_mini_ativo()) return "player";
  if (detail_aberto()) return "detail";
  if (spainel_aberto()) return "saved-panel";
  if (menu_aberto() || ctx_aberto()) return "menu";
  if (vertudo_aberta()) return "see-all";
  if (avisos_aberto() || avisos_cartao_aberto()) return "notices";
  if (sintro_aberto() || pipintro_aberto() || recintro_aberto() || telemetria_aberto()) return "intro";
  if (novidades_aberto() || novidades11_aberto() || novidades12_aberto() ||
      novidades13_aberto() || novidades131_aberto() || novidades132_aberto() ||
      novidades133_aberto() || novidades134_aberto() || novidades139_aberto() || novidades1312_aberto() ||
      novidades142_aberto() || novidades148_aberto() || novidades170_aberto() || novidades180_aberto() ||
      novidades20_aberto() || novidades201_aberto() || novidades202_aberto() ||
      novcartao_aberto()) return "whats-new";
  if (atualizacao_aberta() || agendaviso_aberto()) return "update-notice";
  if (recomenda_aberta() || recenviar_aberto() || pessoas_aberto()) return "recommend";
  if (faixas_aberta() || episodios_aberto() || stream_folha_aberta()) return "sheet";
  if (guia_overlay_aberta()) return "guide";
  if (registro_aberto()) return "log";
  return NULL;
}

void app_atualizar(float dt, Uint32 agora) {
  if (player_aberto() || player_mini_ativo()) diagnostico_cancelar_vazao();
  // Login e escolha de perfil retornam cedo; a atividade da conta anterior
  // precisa sair antes deles, junto de qualquer modal/pedido ja enfileirado.
  ilhacart_validar_identidade();
  // Animacoes reduzidas valem para TODA mola e rampa do app (anim.h), nao so
  // para as telas que lembravam de perguntar. Uma leitura por quadro.
  anim_politica_reduzida = ajustes_animacoes_reduzidas();
  central_atualizar(dt, agora);
  // OK na vitrine da tela de descanso (descanso.h): abre o titulo, so com a
  // Home na frente. Em outra tela o OK so acordou.
  { int k = descanso_pedido_abrir();
    if (k >= 0 && app_na_home()) abrirPorIndice(k); }
  vigiarMotorP2p();
  diagnostico_intro_atualizar(dt, agora);
  // Spotlight: a mola de entrada/saida e o ditado correm em qualquer tela; o
  // OK segurado em "Buscar" (menu.c) abre por aqui, no quadro em que cruza.
  spot_atualizar(dt, agora);
  celb_atualizar(dt);
  if (menu_pediu_spotlight() && spotPode()) spotAbrir(0);
  if (spot_aberto() && !spotPode()) spot_fechar();
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
  // ARTE DO ADDON (Ajustes > Layout > Arte do addon): o mesmo roteamento, pelo
  // mesmo motivo — artehero, posterprov e colecoes nao incluem ajustes.h. O
  // logo e lido direto por descoberta.c, que ja depende dos Ajustes.
  artehero_fundo_addon(ajustes_fundo_addon());
  posterprov_preferir_addon(ajustes_poster_addon());
  col_arte_conta(ajustes_col_arte_conta());
  // O backend precisa progredir mesmo no login, perfis e transicoes que
  // retornam cedo: seek pendente no Tizen e prazo de recuo DV no webOS.
  video_bombear();
  // Dolby Vision in MKV handed playback back to the TV player (HDR10).
  { int dvr = video_dv_recuo_consumir();
    if (dvr) {
      const char *t = dvr == 1
        ? i18n("A conexão não acompanhou o Dolby Vision. Continuando em HDR10.")
        : i18n("Dolby Vision não abriu nesta fonte. Continuando em HDR10.");
      // The island is hidden while a film plays: the player's own toast is
      // what the person sees there (the island alone showed it after exit).
      if (player_aberto()) player_toast(t, 6000u);
      else ilha_avisar("dv-mkv", ILHA_INFO, NULL, t, 6000u, 0);
    } }
  player_validar_retido(agora);
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
  if (!sessao_logada()
#ifdef NV_LINUX_DESKTOP
      && !uiPreview
#endif
     ) {
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
  discord_passo((unsigned)agora);
  // PERSONAL SERVERS (servidores.h: Jellyfin, Emby, Plex): the modules fetches on its own workers; the
  // Home is rebuilt when its row snapshot changes or the option flips. One
  // integer compare per frame, no network here.
  if (servidores_passo()) desc_repetir_silencioso();
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
    home_oculta(1);          // montada, nao pintada: o destaque nao gira
    home_atualizar(dt, agora);
    home_oculta(0);
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
    if (perfilsel_concluido() && perfilsel_preparando()) {
      // A HOME DO PERFIL NOVO SE MONTA AQUI ATRAS, e so aparece pronta. Antes a
      // home abria no instante da escolha e se reconstruia duas ou tres vezes
      // na frente da pessoa: a do perfil anterior, depois a dos addons novos,
      // depois a do Continuar e das colecoes do perfil. Pronta = o ciclo de
      // sync DESTE perfil aplicado e nenhuma montagem no ar (catalogos ou
      // Continuar). O teto existe para uma rede lenta nunca prender a pessoa
      // na tela de escolha: passado ele, a home abre como estiver e termina de
      // se montar ali, como antes.
      if ((sync_perfil_pronto() && !desc_montando()) ||
          agora - trocaPerfilDesde >= TROCA_PERFIL_TETO_MS) {
        printf("[perfis] home do perfil %d %s em %u ms\n", perfis_ativo(),
               (sync_perfil_pronto() && !desc_montando()) ? "pronta" : "aberta pelo teto",
               (unsigned)(agora - trocaPerfilDesde));
        fflush(stdout);
        perfilsel_preparar(0, agora);
        home_ir_topo();
        tela = TELA_HOME;
      }
      return;
    }
    if (perfilsel_concluido()) {
      int trocou = perfis_ativo() != perfilAntes;
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
      if (trocou) {
        invalidarPerfil();
        // Os ajustes DESTE perfil nesta TV (ou os do principal, na primeira
        // visita), e o blob da conta dele por cima. Ver sync_trocar_perfil.
        sync_trocar_perfil(perfilAntes);
        ilhasinais_perfil_trocado();   // "Agora no perfil Lia" na ilha (02/10)
        enquete_perfil_trocado();      // a enquete do perfil anterior sai (N3)
        // AS COLECOES DO PERFIL ANTERIOR SAEM JA. col_definir_json so roda
        // quando a conta manda linhas, entao um perfil sem colecoes ficava
        // com as do anterior para sempre.
        col_esquecer_perfil();
        desc_esquecer_catalogo_perfil();   // #294: fileiras do outro perfil saem da Home
        // OS SALVOS SAO DO PERFIL: a lista do anterior sai da tela (so os
        // titulos que o novo nao tem) e entra a deste.
        trocarSalvosDePerfil();
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
      if (trocou) {
        // Fica nesta tela, com o indicador no cartao, ate a home estar pronta
        // (bloco acima). Confirmar o mesmo perfil nao espera nada: a home ja e
        // a dele.
        perfilAntes = perfis_ativo();
        trocaPerfilDesde = agora;
        perfilsel_preparar(1, agora);
        return;
      }
      tela = TELA_HOME;
    }
    return;
  }

  // O fundo das listras (login e escolha de perfil) sai da memoria quando a
  // pessoa passa das duas telas. Sem textura carregada, e um teste e nada mais.
  login_soltar();

  // Um ciclo por vez, e so quando a conta existe. O passo e barato: sem fio
  // terminado ele nao faz nada.
  sync_passo((unsigned)agora);
  // SERVIDOR DA CONTA FORA DO AR (#215): um aviso discreto na ilha, uma vez
  // por queda, e retirado quando ele volta. Sem isto a pessoa via a home sem
  // addons (ou com os de ontem) e concluia que o app quebrou — e saia da conta
  // para "consertar", que e a unica coisa que piora o caso.
  { static int avisado;
    int fora = sync_addons_fora();
    if (fora && !avisado) {
      ilha_avisar("conta-fora", ILHA_INFO, NULL,
                  fora == 1
                    ? i18n("Servidor da conta Nuvio fora do ar — usando seus addons salvos")
                    : i18n("Servidor da conta Nuvio fora do ar — seus addons voltam quando ele responder"),
                  9000u, 0);
      avisado = 1;
    } else if (!fora && avisado) {
      // A CONTA VOLTOU (02/10, mockup): antes so retirava o aviso. Agora diz,
      // com a MESMA chave — se o "fora" ainda estiver na pilula, troca no lugar.
      ilha_avisar("conta-fora", ILHA_OK, "aj_cloud", i18n("A conta voltou. Addons atualizados."), 4000u, 0);
      avisado = 0;
    } }

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
    player_descartar_retido();
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
  // A 1.8.0 PRIMEIRO, e antes da fila antiga: novidades180_primeira_vez
  // prepara a fila (grava as marcas antigas, novidadesfila.h) e abre o cartao;
  // os *_primeira_vez() de baixo ja encontram tudo visto.
  // DA 2.0 EM DIANTE o guia novo e o unico "primeira vez" (novidades20.h):
  // ele prepara a fila antiga, grava a marca da 1.8.0 e abre uma vez, para
  // quem atualiza e para quem instala do zero. novidades180_primeira_vez nao
  // e mais chamada: o cartao da 1.8.0 so vive como cena do Guia de uso.
  // A 2.0.1 decide ANTES do guia da 2.0, no mesmo quadro: ela olha se o guia
  // ja foi visto antes de ele gravar a marca (novidades201.h).
  // O CARTAO DA VERSAO ATUAL (novidades_cartao.h) decide antes de todos: ele
  // grava as marcas da 2.0.1 e da 2.0.2, que entao nao abrem nunca mais. A
  // condicao (Home pronta, sem player nem pagina do titulo) mora nele, onde o
  // teste a exercita.
  novcartao_decidir(tela == TELA_HOME && homePronta, player_aberto(), detail_aberto());
  if (tela == TELA_HOME && homePronta && !player_aberto() && !detail_aberto()) {
    // A 2.0.2 antes da 2.0.1: ela substitui o cartao da 2.0.1.
    novidades202_primeira_vez();
    novidades201_primeira_vez();
    novidades20_primeira_vez();
  }
  if (tela == TELA_HOME && homePronta && !player_aberto() && !detail_aberto() && !novidades170_aberto() && !novidades180_aberto() && !novidades20_aberto() && !novidades201_aberto() && !novidades202_aberto() && !novcartao_aberto()) {
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
    // O da 1.4.8 depois do da 1.4.2: na ordem em que as coisas chegaram.
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() &&
        !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() &&
        !novidades134_aberto() && !novidades139_aberto() && !novidades142_aberto() &&
        !novidades148_aberto() && !novidades170_aberto() && !novidades180_aberto() && !novidades20_aberto() && !pipintro_aberto())
      novidades148_primeira_vez();
    // A 1.7 no lugar da 1.6.0 (a 1.6.6 nunca saiu; como a 1.6.0 tomou o da
    // 1.5.2): uma apresentacao por aparelho, depois da cor viva. Quem nao viu
    // a da 1.6.0 nao a recebe mais; a desta versao cobre.
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() &&
        !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() &&
        !novidades134_aberto() && !novidades139_aberto() && !novidades142_aberto() &&
        !novidades148_aberto() && !novidades170_aberto() && !novidades180_aberto() && !novidades20_aberto() && !pipintro_aberto())
      novidades170_primeira_vez();
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() &&
        !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() &&
        !novidades134_aberto() && !novidades139_aberto() && !novidades142_aberto() && !novidades148_aberto() && !novidades170_aberto() && !novidades180_aberto() &&
        !novidades1312_aberto() && !pipintro_aberto())
      novidades1312_primeira_vez();
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() &&
        !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() &&
        !novidades134_aberto() && !novidades139_aberto() && !novidades1312_aberto() && !novidades142_aberto() && !novidades148_aberto() && !novidades170_aberto() && !novidades180_aberto() && !novidades20_aberto() && !telemetria_aberto() && !pipintro_aberto())
      telemetria_primeira_vez();
    // AVISO DE VERSAO NOVA: a consulta ao GitHub so parte quando a home esta
    // de pe (nao disputa a rede com o catalogo), e o cartao so abre quando
    // nenhum outro cartao de primeira vez esta aberto. Chamada a cada quadro:
    // a agenda (atualizacao_agenda_vence) decide se ja e hora de reconsultar
    // (~6 h), e nunca com o mini player tocando.
    if (!player_mini_ativo()) atualizacao_verificar();
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() && !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() && !novidades142_aberto() && !novidades148_aberto() && !novidades170_aberto() && !novidades180_aberto() && !novidades20_aberto() && !pipintro_aberto())
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
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() && !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() && !novidades134_aberto() && !novidades139_aberto() && !novidades1312_aberto() && !novidades142_aberto() && !novidades148_aberto() && !novidades170_aberto() && !novidades180_aberto() && !novidades20_aberto() && !telemetria_aberto() && !pipintro_aberto() && !atualizacao_aberta())
      recomenda_mostrar_se_houver();
    // EXPLICADOR DAS TELAS SOCIAIS: mesmas guardas de todos os outros, mais
    // a do cartao de recomendacao recebida — dois cartoes ao mesmo tempo
    // seria um por cima do outro. Ele proprio nao abre num pacote sem
    // NUVIO_REC_URL (recomenda_ativo), e por isso nao ha guarda aqui: um
    // anuncio de recurso que nao esta no pacote e pior que silencio.
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() && !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() && !novidades134_aberto() && !novidades139_aberto() && !novidades1312_aberto() && !novidades142_aberto() && !novidades148_aberto() && !novidades170_aberto() && !novidades180_aberto() && !novidades20_aberto() && !telemetria_aberto() && !pipintro_aberto() && !atualizacao_aberta() &&
        !recomenda_aberta())
      recintro_primeira_vez();
    // LEMBRETE VENCIDO: o unico aviso que esta TV consegue dar. Ultimo da fila
    // pela mesma razao de todos os outros — dois cartoes juntos seria um por
    // cima do outro —, e sem consulta de rede nenhuma: o que ele mostra ja
    // esta em disco desde que o dono apertou "Lembrar-me".
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() && !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() && !novidades134_aberto() && !novidades139_aberto() && !novidades1312_aberto() && !novidades142_aberto() && !novidades148_aberto() && !novidades170_aberto() && !novidades180_aberto() && !novidades20_aberto() && !telemetria_aberto() && !pipintro_aberto() && !atualizacao_aberta() &&
        !recomenda_aberta() && !recintro_aberto())
      agendaviso_mostrar_se_houver();
    // O CARTAO DO CRASH, depois do lembrete e pelas mesmas regras: um cartao
    // por vez, com a home de pe.
    if (!registro_aberto() && !sintro_aberto() && !novidades_aberto() &&
        !novidades11_aberto() && !novidades12_aberto() && !novidades13_aberto() && !novidades131_aberto() && !novidades132_aberto() && !novidades133_aberto() && !novidades134_aberto() && !novidades139_aberto() && !novidades1312_aberto() && !novidades142_aberto() && !novidades148_aberto() && !novidades170_aberto() && !novidades180_aberto() && !novidades20_aberto() && !telemetria_aberto() && !pipintro_aberto() && !atualizacao_aberta() &&
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
  // ACRESCENTAR pode (#221): os indices de quem ja estava nao mudam.
  addons_drenar();
  if (aguardandoFonte != 2) addons_estado();
  // A marca "Sua escolha anterior" na folha que abriu antes de o addon da
  // lembrada responder: confere de novo quando a lista cresce.
  if (stream_folha_aberta() && stream_preferida() < 0 && stream_n() != prefFolhaN) {
    char base[24];
    prefFolhaN = stream_n();
    idBaseDoTitulo(base, sizeof base);
    if (base[0] && fontepref_tem(base)) stream_preferir(fontepref_escolher(base));
  } else if (!stream_folha_aberta()) prefFolhaN = -1;
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
    else if (c == AVISOS_ABRIR_AMIGOS) spainel_ir_aba(2);   // 2 = aba Amigos
    else if (c == AVISOS_ABRIR_ATUALIZACAO) atualizacao_abrir(); }
  // A ILHA: cartoes (ilhacart.c) e o que o modal pediu.
  { const CatItem *ab = detail_aberto() ? cat_item(detail_indice()) : NULL;
    IlhaCartao ic;
    int qual = 0, o;
    ilhacart_atualizar(agora, ab && ab->imdb[0] ? ab->imdb : NULL);
    ilhasinais_passo(agora);
    // O "Reconectar" do Trakt (modal da ilha): Ajustes, na linha do Trakt.
    if (ilhasinais_pediu_trakt() && !player_aberto()) {
      if (detail_aberto()) detail_fechar_seco();
      ajustes_abrir_no_trakt();
      trocarTela(TELA_AJUSTES); menu_definir_destino(MENU_AJUSTES);
    }
    // Pedidos de amizade (varios) e amigos vendo agora: a aba do painel.
    if (ilhasinais_pediu_amigos()) spainel_ir_aba(2);   // 2 = aba Amigos
    { char im[32];
      if (ilhacart_pediu_atividade(im, sizeof im)) {
        if (im[0]) spainel_abrir_titulo(im);
        else spainel_ir_aba(1);                       // 1 = aba Atividade
      } }
    o = ilha_pediu(&ic, &qual);
    if (o && qual == ILHA_VIVO && !ilhacart_vivo_vale(&ic)) o = ILHA_PEDIU_NADA;
    if (o == ILHA_PEDIU_SALVOS) {
      // O PAINEL NASCE DO MODAL: o retangulo de agora e o do modal, que some
      // seco no mesmo quadro — o painel toma a forma dele e cresce.
      float ix, iy, iw, ih;
      if (!spainel_aberto()) {
        if (ilha_rect(&ix, &iy, &iw, &ih)) spainel_abrir_de(ix, iy, iw, ih);
        else spainel_abrir();
      }
      ilha_modal_fechar(1);
    } else if ((o == ILHA_PEDIU_TOCAR || o == ILHA_PEDIU_DETALHES) && !player_aberto()) {
      int idx = cat_indice_por_imdb(ic.imdb);
      if (qual == ILHA_ESTREIA) avisos_marcar_visto(ic.avisoId);
      if (o == ILHA_PEDIU_TOCAR && qual == ILHA_VIVO &&
          player_retomar_retido(ic.imdb, ic.serie ? ic.t : 0, ic.serie ? ic.e : 0)) {
        if (detail_aberto()) detail_fechar_seco();
        if (menu_aberto()) menu_fechar();
        ilha_modal_fechar(1);
      } else if (idx >= 0) {
        // O mesmo caminho do OK no card de "Continuar assistindo" com "OK no
        // card" = Retomar: a pagina abre e a reproducao e pedida junto, no
        // episodio que o cartao mostrava (cwTocarT/E, issue #93).
        cwTocarT = cwTocarE = 0; cwEspera = 0;
        if (o == ILHA_PEDIU_TOCAR && ic.serie && ic.t > 0 && ic.e > 0) { cwTocarT = ic.t; cwTocarE = ic.e; }
        abrirPorIndice(idx);
        if (o == ILHA_PEDIU_TOCAR) detail_pedir_reproduzir();
      } else desc_pedir_titulo(ic.imdb);
    } else if (o == ILHA_PEDIU_DISPENSAR) {
      if (qual == ILHA_VIVO) player_descartar_retido();
      ilhacart_dispensar(qual);
    } else if (o == ILHA_PEDIU_DEPOIS) ilhacart_adiar(qual);
  }
  // Da pagina do titulo, um titulo escolhido no painel de Salvos/avisos TROCA a
  // pagina (a de baixo fecha seca). So esse pedido: os demais seguem de fora.
  if (detail_aberto() && !player_aberto()) {
    const char *alvo = spainel_pediu_abrir();
    if (alvo && alvo[0]) {
      int k = cat_indice_por_imdb(alvo);
      detail_fechar_seco();
      if (k >= 0) abrirPorIndice(k); else desc_pedir_titulo(alvo);
    }
  }
  if (!detail_aberto() && !player_aberto()) {
    const char *alvo = spainel_pediu_abrir();
    if (!alvo) alvo = recomenda_pediu_abrir();   // mesmo contrato, outra origem
    if (!alvo) alvo = avisos_pediu_abrir();
    { static char afImdb[24];
      if (!alvo && amigosfil_pediu_titulo(afImdb, sizeof afImdb)) alvo = afImdb; }
    if (!alvo && tela == TELA_AGENDA) alvo = agendaui_pediu_abrir();
    // "ASSISTIR T<n>E<n>" DO MODAL DA AGENDA: abre o titulo e pede a
    // reproducao do episodio no mesmo passe — o caminho do cartao de Continuar
    // assistindo (issue #93): cwTocarT/E valem sobre o episodio que o detalhe
    // adivinharia. Titulo fora do catalogo so abre (a descoberta o busca num
    // fio e o episodio ja nao tem para onde ir); tocar la e um OK a mais.
    if (!alvo && tela == TELA_AGENDA) {
      int tt = 0, te = 0;
      const char *tc = agendaui_pediu_tocar(&tt, &te);
      if (tc && tc[0]) {
        int k = cat_indice_por_imdb(tc);
        if (k >= 0) {
          abrirPorIndice(k);
          if (detail_aberto()) { cwTocarT = tt; cwTocarE = te; detail_pedir_reproduzir(); }
        } else desc_pedir_titulo(tc);
      }
    }
    // COPIA antes de esvaziar: `alvo = abrirTeste; abrirTeste[0] = 0` deixava
    // `alvo` apontando para a string que acabara de virar "" — a porta de
    // teste "abrir:<tt>" (main.c) nao abria nada.
    { static char abrirAgora[sizeof abrirTeste];
      if (!alvo && abrirTeste[0]) {
        snprintf(abrirAgora, sizeof abrirAgora, "%s", abrirTeste);
        abrirTeste[0] = 0;
        alvo = abrirAgora;
      } }
    if (alvo && alvo[0]) {
      int k = cat_indice_por_imdb(alvo);
      if (k >= 0) abrirPorIndice(k); else desc_pedir_titulo(alvo);
    }
  }

  // O PERFIL DO AMIGO (amigoperfil.c): OK num rosto da fileira de amigos ou
  // na linha do amigo no painel. Substitui a ficha do Trakt de social.c.
  { char pid[96];
    if ((tela == TELA_HOME && amigosfil_pediu_perfil(pid, sizeof pid)) ||
        spainel_pediu_perfil(pid, sizeof pid)) {
      if (spainel_aberto()) spainel_fechar();
      amigoperfil_abrir(pid); trocarTela(TELA_SOCIAL);
    }
    if (tela == TELA_PERFIL && perfil_pediu_amigo(pid, sizeof pid)) {
      amigoperfil_abrir(pid); trocarTela(TELA_SOCIAL);
      socialDoPerfil = 1;
    } }
  // "ABRIR A AGENDA COMPLETA" da aba Agenda do painel Social.
  if (spainel_pediu_agenda()) {
    if (spainel_aberto()) spainel_fechar();
    trocarTela(TELA_AGENDA); menu_definir_destino(MENU_AGENDA);
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
  // 2.0.3: Voltar ou Esquerda devolvem a categoria Fontes e addons com o foco
  // na linha "Addons" (antes a Esquerda abria a barra lateral e o Voltar caia
  // no indice dos Ajustes).
  if (tela == TELA_ADDONS && addonsui_quer_sair()) {
    saiuPorEsquerda = 0;
    ajustes_voltar_de_lista(0);
    trocarTela(TELA_AJUSTES); menu_definir_destino(MENU_AJUSTES);
  }
  if (tela == TELA_AJUSTES) {
    int buscaAjustes = ajustes_pediu_busca();
    if (buscaAjustes == 2) spot_reabrir_ajustes();
    else if (buscaAjustes == 1) spot_abrir_ajustes(0);
  }
  // "Voltar às novidades" no Guia de uso aberto pelo cartao da 1.8.0: a home
  // e o cartao de novo, com o foco no guia.
  // Ajustes › Sobre e ajuda › Novidades 2.0: o guia por cima de Ajustes, do
  // comeco; ao fechar, Ajustes continua onde estava.
  if (tela == TELA_AJUSTES && ajustes_pediu_novidades20()) novidades20_abrir(0);
  if (tela == TELA_AJUSTES && ajustes_pediu_novidades()) {
    trocarTela(TELA_HOME);
    menu_definir_destino(MENU_INICIO);
    novidades180_reabrir();
  }
  // Os atalhos de tela do Guia de uso ("Abrir o Guia de TV" e cia.): a tela e
  // o item do menu, como se a pessoa tivesse ido pelo menu.
  if (tela == TELA_AJUSTES) {
    switch (ajustes_pediu_tela()) {
      case AJ_TELA_GUIA_TV:    trocarTela(TELA_GUIA);       menu_definir_destino(MENU_GUIA);       break;
      case AJ_TELA_BIBLIOTECA: trocarTela(TELA_BIBLIOTECA); menu_definir_destino(MENU_BIBLIOTECA); break;
      case AJ_TELA_AGENDA:     trocarTela(TELA_AGENDA);     menu_definir_destino(MENU_AGENDA);     break;
      case AJ_TELA_EXPLORAR:   trocarTela(TELA_EXPLORAR);   menu_definir_destino(MENU_EXPLORAR);   break;
      case AJ_TELA_PERFIL:     trocarTela(TELA_PERFIL);     menu_definir_destino(MENU_PERFIL);     break;
      default: break;
    }
  }
  if (tela == TELA_AJUSTES && ajustes_pediu_addons()) {
    addonsui_abrir();
    trocarTela(TELA_ADDONS);
  }
  // Plugins (F09): o mesmo caminho da lista de addons (abre de Ajustes, volta a ela).
  if (tela == TELA_PLUGINS && pluginsui_quer_sair()) {
    saiuPorEsquerda = 0;
    ajustes_voltar_de_lista(1);
    trocarTela(TELA_AJUSTES); menu_definir_destino(MENU_AJUSTES);
  }
  if (tela == TELA_AJUSTES && ajustes_pediu_plugins()) {
    pluginsui_abrir();
    trocarTela(TELA_PLUGINS);
  }
  if (tela == TELA_AJUSTES && ajustes_pediu_diagnostico()) {
    diagDaHome = 0;
    trocarTela(TELA_DIAGNOSTICO);
  }
  // O ATALHO DO TESTE DE VELOCIDADE (Ajustes › Diagnóstico, logo abaixo do
  // diagnostico): a mesma tela, ja no teste, sem apresentacao nem objetivo. O
  // pedido vai ANTES da troca porque e diagnostico_iniciar quem o le. O Voltar
  // do resultado sai da tela, e diagDaHome = 0 devolve a Ajustes.
  // DIAGNOSTICO DA LIVE TV: de Ajustes (Conteudo > Live TV) ou do VERDE no
  // guia. O Voltar devolve a quem abriu.
  if (tela == TELA_AJUSTES && ajustes_pediu_livetv_diag()) {
    ltdDoGuia = 0;
    trocarTela(TELA_LIVETV_DIAG);
  }
  if (tela == TELA_GUIA && guia_pediu_livetv_diag()) {
    ltdDoGuia = 1;
    trocarTela(TELA_LIVETV_DIAG);
  }
  if (tela == TELA_LIVETV_DIAG && livetvdiag_quer_sair()) {
    trocarTela(ltdDoGuia ? TELA_GUIA : TELA_AJUSTES);
    menu_definir_destino(ltdDoGuia ? MENU_GUIA : MENU_AJUSTES);
  }
  if (tela == TELA_AJUSTES && ajustes_pediu_velocidade()) {
    diagDaHome = 0;
    diagnostico_abrir_velocidade();
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
              || (tela == TELA_SOCIAL      && amigoperfil_quer_sair())
              || (tela == TELA_AJUSTES    && ajustes_quer_sair())
              || (tela == TELA_DIAGNOSTICO && diagnostico_quer_sair());
    if (fechar) {
      // ESQUERDA na borda abre a barra SOBRE a tela atual. Voltar continua
      // sendo a saída normal para a Home; assim o foco da tela não é perdido
      // só para alcançar a navegação principal.
      if (saiuPorEsquerda && sidebar_permitida()) {
        menu_abrir();
      } else if (tela == TELA_SOCIAL && socialDoPerfil) {
        trocarTela(TELA_PERFIL);
        menu_definir_destino(MENU_PERFIL);
        perfil_voltar_ao_duelo();
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
  // UM DESTINO ESCOLHIDO NA BARRA ABERTA POR CIMA DA PAGINA fecha a pagina,
  // como qualquer navegacao (mesmo o destino atual: "Inicio" com a pagina
  // aberta sobre a home e voltar para a home). Voltar/DIREITA nao passam aqui.
  // "Ver tudo" idem: a barra tambem abre por cima dela (coluna 0), e sem isto
  // a lista ficaria cobrindo a tela nova.
  if (menu_escolheu()) {
    if (detail_aberto()) detail_fechar_seco();
    if (vertudo_aberta()) vertudo_fechar_seco();
  }
  if (menu_pediu_trocar()) {
    player_descartar_retido();
    fontevolta_esquecer("troca de usuario");
    invalidarPerfil();
    tela = TELA_ESCOLHA_PERFIL;
    perfilAntes = perfis_ativo();
    perfilsel_iniciar();
    return;
  }

  { int c = menu_pediu_colecao();
    const ColFolder *pf = c >= 0 ? col_folder(c) : NULL;
    if (pf) vertudo_colecao(pf); }
  if (menu_mudou_destino()) {
    switch (menu_destino()) {
      case MENU_GUIA:
        // PiP ativo: overlay sobre o mini (recomendacao backlog C) em vez da
        // tela cheia opaca — zap/Azul/Voltar do PiP continuam coerentes.
        // PiP no ar: o guia COMPLETO abre com o canal deslizando do canto
        // para o preview (sem recarregar) — o PiP nao tem mais lugar no guia.
        if (player_mini_ativo() && player_id_canal()[0]) guiaComCanalNoAr();
        else trocarTela(TELA_GUIA);
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
    SpotPedido pedPessoa;
    if (tela == TELA_HOME && home_pediu_abrir()) {
      if (home_item_focado(&it)) abrirTitulo(&it, 1, 1);
    } else if (tela == TELA_EXPLORAR && explorar_pediu_abrir(&idx)) {
      abrirPorIndice(idx);
    } else if (tela == TELA_HOME && home_pediu_tocar()) {
      // OK no card da retomada com "OK no card" = Retomar (issue #93): abre a
      // pagina do titulo E pede reproducao no mesmo passe. A pagina fica
      // aberta por baixo — Voltar do player cai nela, como o dono espera.
      if (home_item_focado(&it)) {
        // O episodio a tocar e o que o CARD mostrava, nao o que o detalhe
        // adivinharia: para serie o card pode anunciar o PROXIMO episodio
        // (continuar_desenhar troca T/E pelo prox_seguinte quando o semeado
        // ja terminou), e episodioAlvo chegaria a outro numero — ou a nenhum,
        // porque a lista de episodios ainda nao chegou da rede.
        const CatItem *cw = cat_item(it.indice);
        int tocar = 1;
        cwTocarT = cwTocarE = 0;
        cwEspera = 0;
        if (cw && !strcmp(cw->tipo, "series")) {
          cwTocarT = cw->temporada; cwTocarE = cw->episodio;
          // EPISODIO TERMINADO NUNCA TOCA DE NOVO DO COMECO. O card ja anuncia
          // o proximo quando a lista existe; sem lista (a copia do card nao
          // tem), a pagina abre e o toque espera a lista (cwEsperarProximo).
          // Sem proximo — fim da serie, ou o seguinte ainda nao foi ao ar —,
          // so a pagina abre.
          if (cw->progresso >= ajustes_cw_concluido() && cw->temporada > 0 &&
              cw->episodio > 0) {
            int k = cat_indice_titulo(cw->imdb, it.indice);
            ProxSugestao prox;
            tocar = 0;
            cwTocarT = cwTocarE = 0;
            if (k >= 0 && cat_n_episodios(k) > 0) {
              if (prox_seguinte(cw, cat_episodio(k, 0), cat_n_episodios(k),
                                ajustes_cw_concluido(),
                                (long long)time(NULL) * 1000LL, &prox)) {
                cwTocarT = prox.temporada; cwTocarE = prox.episodio;
                tocar = 1;
              } else
                printf("[cw] %s terminado e sem proximo: abrindo so a pagina\n", cw->imdb);
            } else {
              cwEsperaItem = *cw;
              cwEsperaDesde = SDL_GetTicks();
              cwEspera = 1;
              printf("[cw] %s terminado, lista a caminho: o toque espera o proximo\n", cw->imdb);
            }
          }
        }
        abrirTitulo(&it, 1, 1);
        if (tocar) detail_pedir_reproduzir();
      }
    } else if (tela == TELA_BUSCA && busca_pediu_pessoa(&pedPessoa)) {
      abrirPessoa(&pedPessoa);
    } else if (tela == TELA_BUSCA && busca_pediu_abrir(&idx)) {
      if (busca_item_focado(&it)) abrirTitulo(&it, 1, 1); else abrirPorIndice(idx);
    } else if (tela == TELA_BIBLIOTECA && biblioteca_pediu_abrir(&idx)) {
      abrirPorIndice(idx);
    } else if (tela == TELA_PERFIL) {
      PerfilDestaque p;
      if (perfil_item_selecionado(&p) && p.id[0]) {
        idx = cat_indice_por_imdb(p.id);
        if (idx >= 0) abrirPorIndice(idx); else desc_pedir_titulo(p.id);
      }
    } else if (tela == TELA_SOCIAL) {
      char si[24];
      if (amigoperfil_pediu_titulo(si, sizeof si) && si[0]) {
        idx=cat_indice_por_imdb(si);
        if(idx>=0)abrirPorIndice(idx); else desc_pedir_titulo(si);
      }
    }
  }

  // A ESPERA DO PROXIMO (cwEspera, acima). Pagina fechada, player aberto ou
  // prazo vencido desistem: a pagina fica aberta e nada toca.
  if (cwEspera) {
    if (!detail_aberto() || player_aberto() ||
        SDL_GetTicks() - cwEsperaDesde > CW_ESPERA_MS) {
      printf("[cw] %s: lista nao chegou a tempo; fica so a pagina\n", cwEsperaItem.imdb);
      cwEspera = 0; cwTocarT = cwTocarE = 0;
    } else {
      int k = cat_indice_titulo(cwEsperaItem.imdb, detail_indice());
      if (k >= 0 && cat_n_episodios(k) > 0) {
        ProxSugestao prox;
        cwEspera = 0; cwTocarT = cwTocarE = 0;
        if (prox_seguinte(&cwEsperaItem, cat_episodio(k, 0), cat_n_episodios(k),
                          ajustes_cw_concluido(),
                          (long long)time(NULL) * 1000LL, &prox)) {
          cwTocarT = prox.temporada; cwTocarE = prox.episodio;
          detail_pedir_reproduzir();
        } else
          printf("[cw] %s terminado e sem proximo: abrindo so a pagina\n", cwEsperaItem.imdb);
      }
    }
  }

  legendasDaFonte();

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
    { static char ultimoAlvo[64] = "";
      static unsigned ultimaVersao = 0;
      int i = detail_indice();
      const CatItem *ci = cat_item(i);
      char alvo[64];
      idDoAlvo(ci, alvo, sizeof alvo);
      if (!player_aberto() && aguardandoFonte != 2 && ci && ci->imdb[0] &&
          (strcmp(alvo, ultimoAlvo) || (detail_aberto() && ultimaVersao != addons_versao()))) {
        if (!strcmp(alvo, ultimoAlvo)) printf("[addons] lista mudou: refazendo a busca de fontes de %s\n", alvo);
        snprintf(ultimoAlvo, sizeof ultimoAlvo, "%s", alvo);
        ultimaVersao = addons_versao();
        // A FICHA (meta) SAI ANTES DAS FONTES (R2): a pagina espera a ficha, nao
        // a lista de streams; o pedido de fontes segue logo depois, em paralelo.
        // Episodios do titulo aberto, na temporada onde o dono parou. Sai da
        // rede na hora: guardar a lista de episodios de 40 titulos no pacote
        // envelhecia a cada temporada nova.
        // No FILME o mesmo fio busca o /meta/movie quando o catalogo ainda nao
        // tem elenco: e de la que saem atores, direcao e generos da pagina.
        // Tipo incerto ("anime" do AIOMetadata) tambem: e o /meta que diz se
        // e serie, mesmo quando o catalogo ja trouxe elenco.
        if (!strcmp(ci->tipo, "series") || strcmp(ci->tipo, "movie") ||
            ci->nElenco == 0) desc_episodios(i, 0);
        { stream_definir_alvo(alvo); addons_buscar(alvo, ci->tipo); }
        aquecerAoAbrirTitulo();
        // Legendas do OpenSubtitles junto: sao dezenas por titulo e a busca
        // leva segundos. Pedir so quando o dono abre a folha de faixas faria
        // ele esperar de olho numa lista vazia.
        addons_buscar_legendas(alvo, ci->tipo);
      } }
    aquecerQuandoChegarem();
    prepararAoAbrir();
    // "Assistir do comeco" (issue #46) segue o MESMO caminho do primario; a
    // unica diferenca e a trava de retomada, armada depois de o episodio ficar
    // definitivo — player_do_inicio sobrevive as re-chamadas tardias de
    // player_definir_episodio.
    int doInicio = detail_pediu_do_inicio();
    if ((detail_pediu_reproduzir() || doInicio) && aguardandoFonte != 2) {
      marco("abrir: reproduzir pedido");
      // A tela abre JA, no estado "abrindo fonte", e a escolha acontece depois.
      // Escolher antes deixaria o botao sem resposta por segundos, e escolher
      // sem verificar entregava o video de aviso do debrid — que toca normal e
      // por isso passa por sucesso.
      const CatItem *ci = cat_item(detail_indice());
      limparFonteVOD();
      diagnostico_cancelar_vazao();
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
      // A busca de fontes sai SEMPRE (renovarListaDoPlayer): com a fonte
      // guardada ela corre em segundo plano e so serve a folha e ao recuo.
      renovarListaDoPlayer();
      if (!tocarFonteGuardada()) aguardandoFonte = 1;
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
      // Primeira vez: a ilha pergunta onde o + salva e grava DEPOIS (ilhasalvar.c).
      if (c && ilhasalvar_perguntar(c, entrar)) c = NULL;
      biblioteca_alternar_lista(i);
      // LOCAL SEMPRE, e primeiro. E o unico destino que sobrevive ao
      // fechamento do app sem depender de conta nenhuma; ver salvos.h. Sem
      // isto, quem nao tem Trakt vinculado apertava "+" e nao guardava nada.
      if (c) salvos_definir(c, entrar);
      if (c) atividade_salvo(c, entrar);   // ev=salvo para o Social (atividade.h)
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
      if (c) ilhasalvar_aviso(c, entrar);   // o voo da capa e o "Salvo em ..." na ilha
    }
    if (detail_pediu_fontes()) {
      // A lembrada e localizada ANTES de abrir, para a folha ja desenhar a
      // marca no primeiro quadro. E so uma varredura da lista em memoria.
      char base[24];
      idBaseDoTitulo(base, sizeof base);
      stream_preferir(base[0] ? fontepref_escolher(base) : -1);
      nomeParaFolha(); stream_folha_abrir();
    }
  }
  // Escolher uma fonte na folha inicia a reproducao DELA. Trocar de fonte com o
  // player ja aberto tambem vale: fecha a sessao atual e abre na nova, senao
  // duas ficariam presas no mesmo pipeline.
  // A busca disparada por Reproduzir terminou: agora VERIFICA as fontes, em
  // ordem, ate achar uma que leve ao arquivo — e so entao liga o video.
  if (aguardandoFonte == 1 &&
      (addons_estado() != ADD_BUSCANDO || autoParcialPronto())) {
    unsigned geracao = novaGeracaoFonte();
    // QUANTO O PLAY ESPEROU PELA DECISAO, e por que ela saiu (#202). Medido no
    // D1 so havia a decisao parcial; a completa nao deixava rastro.
    printf("[fonte] decisao em %u ms (%s)\n", (unsigned)player_aberto_ha_ms(),
           addons_busca_parcial() ? autoMotivo : "todos os add-ons responderam");
    fflush(stdout);
    if (addons_busca_parcial()) {
      char faltam[160];
      int k = addons_faltam(faltam, sizeof faltam);
      printf("[fonte] escolha com %d fontes aos %u ms; faltam %d addon(s): %s\n",
             stream_n(), addons_busca_ms(), k, faltam);
      marco("fonte: escolha antes do ultimo addon");
    }
    autoEsperaN = 0;
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
        if (r < 0) fonteEscolhida = stream_automatico_canal();
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
        nomeParaFolha(); stream_folha_abrir();
      } else {
        char alvoP[64];
        int idx;
        alvoPlayer(alvoP, sizeof alvoP);
        if (preparoUsar(alvoP, lembrada, &idx)) {
          // A fonte ja foi escolhida e conferida ao abrir o titulo: o Play abre
          // na hora, sem nova conferencia nem novo arquivo no debrid.
          fonteEscolhida = idx;
          printf("[fonte] preparada ao abrir: usada, o Play abriu sem conferir de novo\n");
          fflush(stdout);
        } else {
          preparoEspera = preparoRodando;    // um preparo em curso pode responder por este Play
          preparoLembradaPlay = lembrada;
          if (pedirFonteJob(FJOB_ADDON, geracao, NULL, 0) < 0) { aguardandoFonte = 0; player_erro_fonte(); }
        }
      }
    }
  }
  vigiarAntecipada();
  if (aguardandoFonte == 2 && fonteEscolhida != -2) {
    const Stream *s = fonteEscolhida >= 0 ? stream_item(fonteEscolhida) : NULL;
    // O player ja esta tocando a escolhida (aberta antes do veredito)?
    int jaTocando = fa_ja_tocando(antAberta, fonteEscolhida) && s != NULL;
    if (antAberta >= 0 && !jaTocando) desfazerAntecipada("outra fonte foi a escolhida");
    antAberta = -1;
    aguardandoFonte = 0;
    printf("automatico (verificado): %s\n", s ? s->rotulo : "(nenhuma fonte serve)");
    // A afirmacao de HDR/DV vai ANTES do tocar: e ela que o bind do ACB
    // descreve ao tv.display. Sem isto o C9 exibe tudo mapeado em SDR.
    if (s) video_definir_dv(s->dolbyVision);
    if (s) video_definir_cabecalhos(s->cabecalhos);
    // Anuncia o CONTENTOR pelo mesmo caminho: e o que dispensa a sonda de
    // Matroska num arquivo que nunca teria um cabecalho desses.
    if (s) video_definir_mp4(stream_e_mp4(s));
    { int esperarMais = !s && !player_id_canal()[0] && !ajustes_fonte_primeira() &&
                        addons_busca_parcial();
      marco(s ? "fonte escolhida" : esperarMais ? "fonte: nenhuma das parciais serve"
                                                : "nenhuma fonte serve"); }
    // Nenhuma fonte e um debrid recusou a CONTA (403 de plano/limite, registro
    // 1541): diz qual no log, que e onde se separa "o addon nao tinha" de "o
    // TorBox nao deixou". Ver debrid_recusa em debrid.h.
    if (!s) { char rec[64];
      if (debrid_recusa(rec, sizeof rec)) printf("[fonte] o debrid recusou (%s)\n", rec); }
    // NENHUMA DAS QUE JA CHEGARAM SERVE, MAS A BUSCA AINDA ESTA NO AR (#221):
    // nao e "nenhuma fonte serve" — e esperar os addons que faltam. As que
    // falharam ja sairam da fila (stream_automatico_excluir); a proxima
    // escolha so sai com lista maior. "Primeira da lista" segue o caminho dela
    // abaixo (uma conferida por vez, dentro do orcamento de tentativas).
    if (!s && !player_id_canal()[0] && !ajustes_fonte_primeira() &&
        addons_busca_parcial() && (player_aberto() || player_mini_ativo()) &&
        !player_quer_sair()) {
      printf("[fonte] nenhuma das %d que chegaram serve; esperando os addons que faltam\n",
             stream_n());
      autoEsperaN = stream_n();
      aguardandoFonte = 1;
    } else {
    // PiP conta como sessao viva: o zap dentro da miniatura depende desta
    // fonte chegar — com a guarda antiga ela seria descartada.
    if ((player_aberto() || player_mini_ativo()) && !player_quer_sair()) {
      stream_definir_atual(fonteEscolhida);
      if (s) {
        if (player_id_canal()[0]) {
          static char viaProxy[4200];
          player_definir_fonte(urlCanal(s, viaProxy, sizeof viaProxy));
        } else if (jaTocando) {
          printf("[fonte] conferencia confirmou a fonte que ja tocava (%u ms)\n",
                 (unsigned)(SDL_GetTicks() - fonteVODDesde));
          fflush(stdout);
        } else player_definir_fonte(s->url);
        // Tocou por outra fonte, mas um servico de debrid ficou de fora por
        // conta sem plano: diz uma vez por sessao, sem bloquear nada.
        { int novo = debrid_sem_plano_novo();
          if (novo) player_toast_ex(i18n(debrid_sem_plano_frase(novo)), 7000, "aj_triangle-alert", 1); }
        if (!player_id_canal()[0]) {
          fonteVODAutomatica = 1;
          // Aberta antes do veredito: a contagem e o relogio dos 15 s sem sinal
          // ja correm desde a abertura (abrirAntecipada), e nao recomecam.
          if (!jaTocando) {
            fonteVODTentativas++;
            fonteVODDesde = SDL_GetTicks();
            // "Fonte 2 de 3" na ilha do player: a troca deixa de ser muda.
            player_definir_tentativa(fonteVODTentativas, VOD_FONTE_MAX_TENTATIVAS);
          }
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
        player_definir_tentativa(fonteVODTentativas + 1, VOD_FONTE_MAX_TENTATIVAS);
        (void)pedirProximaFonteVOD();
      }
      // AS REGRAS DE AUTO-PLAY NAO DEIXARAM NENHUMA (#202: so add-ons
      // permitidos sem "usar os outros", ou regex exigida que nada casa): a
      // lista existe, so o automatico nao pode escolher. Como o oficial, abre
      // a folha de fontes para a pessoa escolher, em vez do erro.
      else if (!player_id_canal()[0] && stream_regra_bloqueou() && stream_n() > 0) {
        limparFonteVOD();
        folhaParaTocar = 1;
        nomeParaFolha(); stream_folha_abrir();
      }
      else { limparFonteVOD(); erroSemFonte(); }
    }
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
  //
  // E O PIPELINE TEM DE TER CARREGADO (#158). No registro 6314 esta linha saiu
  // com "buffer 9.0s" numa fonte que nunca chegou ao loadCompleted: o buffer
  // enchia, o decoder nao comecava, e o cartao era tirado para uma tela
  // preta. O caso que motivou o conserto (18/09) tinha loadCompleted.
  if ((player_aberto() || player_mini_ativo()) && player_fonte_falhou() &&
      player_tem_video() && video_buffer_fim() > 0.5 && !video_falhou() &&
      video_pronto()) {
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
    int xt = xtream_e_id(player_id_canal());
    // ESPERA DE AJUSTES (Live TV > Espera para abrir o canal, ou o "Aplicar"
    // do diagnostico da Live TV): 25 ou 45 s no lugar dos 15 s do Xtream sem
    // decoder e dos 25 s da fonte viva sem dado. O prazo curto da fonte MUDA
    // (4 s) fica: ela ja provou que nao responde.
    Uint32 espera = ajustes_livetv_espera_ms();
    Uint32 semDecMs = espera ? espera : CANAL_SEM_DECODER_MS;
    Uint32 prazo = (espera && canalFontePrazo == CANAL_FONTE_PRAZO_MS) ? espera : canalFontePrazo;
    Uint32 teto = espera > CANAL_ABRE_TETO_MS ? espera : CANAL_ABRE_TETO_MS;
    int semDecoder = (xt || canalViaProxy) && player_carregando() && desde > semDecMs &&
        video_buffer_fim() > 0.5 && !video_decoder_anunciou();
    int morta = player_fonte_falhou() || video_falhou() ||
        (player_carregando() && desde > prazo && video_buffer_fim() <= 0.5) ||
        (player_carregando() && desde > teto) ||
        video_bufferando_ms() > CANAL_TRAVA_MS || semDecoder;
    // CANAL QUE PAROU DEPOIS DE TOCAR (#302): fim de fluxo (o provedor fechou a
    // conexao), posicao parada ou buffering por 8 s. Reabre a MESMA fonte, ate
    // NV_LS_MAX vezes (livestall.h); esgotou, cai no caminho de sempre (proxima
    // fonte). Pausa da pessoa nunca entra. So roda se o watchdog acima nao ja
    // declarou a fonte morta (erro de verdade tem o seu proprio caminho).
    { static int lsIdx = -1;
      static char lsId[64];
      int ls;
      if (lsIdx != canalFonteIdx || strncmp(lsId, player_id_canal(), sizeof lsId - 1)) {
        nv_ls_zerar(&liveStall);
        lsIdx = canalFonteIdx;
        snprintf(lsId, sizeof lsId, "%s", player_id_canal());
      }
      ls = nv_ls_passo(&liveStall, SDL_GetTicks(), video_pronto() ? video_pos() : 0.0,
                       video_pronto(), player_pausa_pessoa(), video_terminou(),
                       video_bufferando_ms());
      if (morta) {
        nv_ls_zerar(&liveStall);
      } else if (ls == NV_LS_AGENDOU) {
        printf("[canal] parou de tocar (fim de fluxo=%d, posicao %.1fs, buffering %u ms): "
               "reabrindo a fonte %d, tentativa %d/%d em %u ms\n", video_terminou(),
               video_pos(), video_bufferando_ms(), canalFonteIdx, liveStall.tentativa,
               NV_LS_MAX, nv_ls_espera_ms(liveStall.tentativa));
        fflush(stdout);
        player_toast_ex(i18n("Conexão caiu, reconectando…"), 4000, "aj_wifi-off", 1);
      } else if (ls == NV_LS_REABRIR) {
        const Stream *mesma = stream_item(canalFonteIdx);
        if (mesma) {
          static char viaProxy[4200];
          printf("[canal] reabrindo a fonte %d (tentativa %d/%d)\n", canalFonteIdx,
                 liveStall.tentativa, NV_LS_MAX);
          fflush(stdout);
          marco("canal: parou, reabrindo a mesma fonte");
          canalFonteDesde = SDL_GetTicks();
          player_definir_fonte(urlCanal(mesma, viaProxy, sizeof viaProxy));
          nv_ls_reaberto(&liveStall, SDL_GetTicks());
        } else {
          morta = 1;
        }
      } else if (ls == NV_LS_DESISTIR) {
        printf("[canal] fonte %d parou %d vezes seguidas: proxima fonte\n", canalFonteIdx, NV_LS_MAX);
        fflush(stdout);
        morta = 1;
      }
    }
    // O formato que tocou vai na frente nos proximos canais (xtream.h).
    { static char tocouUrl[64];
      if (xt && video_pronto() && strncmp(tocouUrl, player_id_canal(), sizeof tocouUrl - 1)) {
        snprintf(tocouUrl, sizeof tocouUrl, "%s", player_id_canal());
        xtream_formato_funcionou(video_url_atual());
      }
      if (!video_pronto()) tocouUrl[0] = 0; }
    if (morta) {
      // POR QUE MORREU, numa linha (#158): o registro so dizia "fonte 0 nao
      // abriu", e a diferenca entre "erro do servidor", "sem dado" e "dado
      // sem decoder" e o que decide o proximo conserto.
      printf("[guia] fonte %d morta: falhou=%d erro='%s' buffer=%.1fs decoder=%d "
             "carregando=%d %ums\n", canalFonteIdx, video_falhou(), video_erro_texto(),
             video_buffer_fim(), video_decoder_anunciou(), player_carregando(), (unsigned)desde);
      fflush(stdout);
      int prox = stream_canal_proxima(canalFonteIdx);
      const Stream *s;
      // PELO PROXY NAO ABRIU: a MESMA fonte direto, antes da proxima (#283).
      // Cobre o HLS que o proxy nao converte (cifrado, fMP4, audio a parte) e
      // a TV em que o motor HLS do uMS toca melhor que o TS continuo. Se o
      // proxy so redirecionou, a tentativa direta ja foi esta.
      if (canalViaProxy) {
        const Stream *mesma = stream_item(canalFonteIdx);
        int redir = proxyts_redirecionou();
        canalViaProxy = 0;
        if (mesma && !redir) {
          printf("[canal] fonte %d pelo proxy nao abriu (%s); tentando direto\n", canalFonteIdx,
                 video_falhou() ? video_erro_texto()
                                : semDecoder ? "dado sem decoder" : "sem dado no prazo");
          fflush(stdout);
          marco("canal: proxy falhou, fonte direto");
          proxyts_definir_cabecalhos("");
          canalFonteDesde = SDL_GetTicks();
          player_definir_fonte(mesma->url);
          return;
        }
      }
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
        { static char viaProxy[4200];
          player_definir_fonte(urlCanal(s, viaProxy, sizeof viaProxy)); }
      } else {
        canalFonteIdx = -1;
        // Sem mais fonte na lista: na tela cheia vira o erro de sempre; no
        // PiP a miniatura morre quieta em vez de prender um quadro morto.
        if (player_mini_ativo()) { if (xt) avisoCanalXtreamMini(); player_fechar_mini(); }
        else if (xt) erroCanalXtream();
        else player_erro_fonte();
        if (semDecoder) { video_parar(); sondarCanalXtream(player_id_canal()); }
      }
    }
  } else {
    nv_ls_zerar(&liveStall);   // fora de canal / fonte em troca: o vigia recomeca do zero
  }

  vigiarFonteGuardada();
  fitDuracaoMidia();
  fitPassivaPermitir();
  tentarProximaFonteVOD();
  atualizarMotivoInicio();
  // O OK na explicacao da ilha: sai do player e cai em Ajustes > Fontes e addons
  // > Escolha da fonte, na linha da espera. A pessoa pediu: e o unico caso em
  // que a explicacao leva a algum lugar.
  if (player_pediu_ajustes_fonte() && player_aberto()) {
    printf("[fonte] ilha: OK abriu Ajustes > Escolha da fonte\n"); fflush(stdout);
    aguardandoFonte = 0;
    limparFonteVOD();
    novaGeracaoFonte();
    limparFontePendente();
    diagnostico_cancelar_vazao();
    player_definir_motivo_inicio("");
    player_encerrar();
    if (detail_aberto()) detail_fechar_seco();
    ajustes_abrir_na_espera_fonte();
    trocarTela(TELA_AJUSTES); menu_definir_destino(MENU_AJUSTES);
  }
  vigiarAberturaManual();
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
      diagnostico_cancelar_vazao();
      player_abrir(titulo,NULL);
      player_definir_episodio(t,e);
      stream_definir_atual(fonte);
      // Qualquer escolha nova invalida a resposta de um torrent anterior; o
      // motor P2P corta o pedido velho ja (sem isso a escolha nova esperaria
      // ate 55 s na fila do fio unico).
      torrentSessao++;
      p2pmotor_cancelar();
      if (!s->url[0] && s->infoHash[0]) {
        pedirTorrentEscolhido(fonte);
      } else {
        // Fonte direta: o torrent do motor (se havia) para de baixar.
        p2pmotor_parar_fundo();
        player_definir_fonte(s->url);
        if (!player_id_canal()[0]) fonteManualDesde = SDL_GetTicks();
        // FONTE QUE O ADDON MARCA COMO FORA DE CACHE ("⏳", "[TB download]"):
        // tocar o link e o que manda o servico baixar, e o que o addon devolve
        // enquanto baixa e um clipe de aviso de ~8 s (registros 1136, 2191,
        // 2501). O clipe fecha o player sem nada na tela; o aviso diz o que
        // esta acontecendo e o que fazer.
        if (s->foraCache)
          player_toast_ex(i18n("Fonte fora do cache: o debrid começa a baixar. Se tocar um aviso curto, tente de novo em alguns minutos."), 9000, "aj_download", 0);
      }
      // Escolha manual num canal tambem entra no watchdog: fonte viva escolhida
      // a dedo pode morrer igual.
      if (player_id_canal()[0]) { canalFonteIdx = fonte; canalFonteDesde = SDL_GetTicks(); canalViaProxy = 0; }
    }
  }
  if (aguardandoFonte != 2 && player_pediu_fontes()) {
    char base[24];
    stream_folha_contexto(player_linha_episodio());
    idBaseDoTitulo(base, sizeof base);
    stream_preferir(base[0] ? fontepref_escolher(base) : -1);
    nomeParaFolha(); stream_folha_abrir();
  }

  // CANAL ESCOLHIDO NO GUIA — tela cheia ou faixa, mesma acao: toca direto.
  { CatItem it;
    if (aguardandoFonte != 2 && guia_pediu_canal(&it)) tocarCanal(&it); }
  // O PREVIEW DO GUIA e uma sessao "mini no guia" do player: mesma busca de
  // fonte, mesmo watchdog, e o OK nele depois so faz a janela crescer.
  { CatItem it;
    if (aguardandoFonte != 2 && guia_pediu_preview(&it)) {
      float x, y, w, h;
      guia_preview_rect(&x, &y, &w, &h);
      player_mini_no_guia(x, y, w, h);
      player_manter_mini();
      tocarCanal(&it);
    } }
  if (guia_pediu_parar_preview() && player_mini_no_guia_ativo()) player_fechar_mini();
  // OK no canal que ja toca no preview: tela cheia crescendo, SEM carregar.
  if (guia_pediu_restaurar() && player_mini_no_guia_ativo()) {
    player_restaurar();
    printf("[guia] preview -> tela cheia no mesmo fluxo\n");
    fflush(stdout);
  }
  // Azul na faixa do mini guia: o guia completo, com o canal no preview.
  if (guia_pediu_guia_cheio() && player_aberto() && player_id_canal()[0]) guiaComCanalNoAr();
  // O botao "Guia" do OSD do canal: o mesmo guia completo, com o canal no
  // preview e tocando.
  if (player_pediu_guia_cheio() && player_aberto() && player_id_canal()[0]) guiaComCanalNoAr();
  // Sessao do preview sem guia na tela (saiu por um caminho que nao passou
  // pelo sair() do guia): nao fica tocando escondida.
  if (player_mini_no_guia_ativo() && !player_janela_animando(NULL, NULL, NULL, NULL) &&
      (tela != TELA_GUIA || !guia_aberta()))
    player_fechar_mini();

  // CH+/- COM CANAL NO AR: zap na ordem do guia (`dir` e o deslocamento que o
  // debounce do player somou, nao so +1/-1). A lista pode ainda nao ter
  // sido carregada (guia nunca aberto nesta sessao): a primeira tecla dispara
  // a carga e nao troca nada, a seguinte ja zapeia.
  { int dir = player_pediu_zap();
    if (dir && player_aberto() && aguardandoFonte != 2) {
      const char *id = player_id_canal();
      CatItem it;
      if (id[0] && guia_zap(id, dir, &it)) tocarCanal(&it);
      else guia_carregar();
    } }

  // RECARREGAR (botao do OSD do canal): mesma acao do zap com deslocamento 0 —
  // o proprio canal, fonte buscada de novo.
  if (player_pediu_recarregar() && player_aberto() && aguardandoFonte != 2) {
    const char *id = player_id_canal();
    CatItem it;
    if (id[0] && guia_zap(id, 0, &it)) tocarCanal(&it);
  }

  // BAIXO/AZUL COM CANAL NO AR: o overlay do guia abre focado no canal que
  // esta tocando. player_id_canal e o id congelado na abertura — o indice no
  // catalogo pode ja ter sido remapeado por uma republicacao da descoberta.
  if (player_pediu_guia() && player_aberto()) {
    const char *id = player_id_canal();
    guia_overlay_abrir();
    if (id[0]) guia_focar_id(id);
  }
  if (aguardandoFonte != 2 && stream_folha_recarregar()) {
    if (player_aberto()) buscarParaPlayerModo(1);
    else {
      const CatItem *ci=cat_item(detail_indice()); char id[64];
      idDoAlvo(ci,id,sizeof id);
      if (ci) { stream_definir_alvo(id); addons_buscar_renovar(id,ci->tipo); }
    }
  }
  { int t,e;
    if (aguardandoFonte != 2 && episodios_escolheu(&t,&e) && player_aberto()) {
      int titulo=player_indice();
      diagnostico_cancelar_vazao();
      player_encerrar(); player_abrir(titulo,NULL);
      player_definir_episodio(t,e);
      marco("buscando fontes"); buscarParaPlayer(); aguardandoFonte=1;
    }
  }
  { int t,e;
    if (aguardandoFonte != 2 && player_pediu_proximo(&t,&e) && player_aberto()) {
      int titulo=player_indice();
      diagnostico_cancelar_vazao();
      player_encerrar(); player_abrir(titulo,NULL); player_definir_episodio(t,e);
      marco("proximo episodio: buscando fontes"); buscarParaPlayer(); aguardandoFonte=1;
    }
  }
  episodios_atualizar(dt);
  // O player devolve 1 para a coluna de audio e 2 para a de legenda.
  { int q = player_pediu_faixas();
    if (q) faixas_abrir_em(q == 2 ? 1 : 0); }
  if (!player_retido()) faixas_atualizar(dt, agora);
  stream_folha_atualizar(dt, agora);
  // FOLHA FECHADA SEM ESCOLHER, com o player esperando por ela. Voltar na
  // folha e "desisti", nao "tente sozinho": abrir a fonte que a pessoa acabou
  // de recusar seria contradizer o gesto. O player mostra o erro de fonte, que
  // ja tem saida pela mesma tecla.
  if (folhaParaTocar && !stream_folha_aberta()) {
    folhaParaTocar = 0;
    if (player_aberto()) player_erro_fonte();
  }

  // SAIR DO PLAYER COLOCA O TITULO NA FRENTE DE "CONTINUAR ASSISTINDO".
  //
  // Relato antigo: "assisto um filme ou episodio, saio do player, e o titulo so
  // aparece em Continuar assistindo depois de fechar e reabrir o app". A
  // resposta foi desc_repetir() quando o titulo nao estava na fileira — o ciclo
  // inteiro (~20 s nesta TV), que republicava a home e punha "Carregando
  // fileiras…" na ilha bem no voo do player ate a pilula. E o card so entrava
  // depois do pouso (dono, 03/10). Hoje fecharSessao ja refaz so o Continuar
  // num fio (desc_refazer_continuar), e o card entra LOCAL no mesmo quadro
  // (desc_continuar_otimista, logo depois de player_encerrar/suspender, com o
  // progresso ja no item): na frente, onde a refacao tambem o poe.
  //
  // VOLTAR NUM CANAL ABERTO PELO GUIA: o video encolhe para o preview do guia,
  // na hora (sem esperar o fade da interface do player) e sem recarregar. O
  // PiP de canto ficou para os canais abertos fora do guia.
  if (player_quer_sair() && player_minimizavel() && tela == TELA_GUIA && guia_aberta())
    guiaComCanalNoAr();
  if (player_quer_sair() && !player_minimizavel() && ajustes_saida_player_home())
    player_preparar_retencao();
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
    // O fundo do voo, copiado ANTES da saida: desc_continuar_otimista troca o
    // bloco do catalogo e `ci` deixa de valer.
    char fundoVoo[sizeof ci->backdrop], imdbSaiu[sizeof ci->imdb];
    snprintf(fundoVoo, sizeof fundoVoo, "%s", ci ? ci->backdrop : "");
    snprintf(imdbSaiu, sizeof imdbSaiu, "%s", ci ? ci->imdb : "");
    { unsigned vivoAntes = ilhacart_vivo_seq();
      if (!ajustes_saida_player_home() || !player_suspender()) player_encerrar();
      // UM LUGAR SO (cwretido.h): o cartao da ilha / a faixa "Retomar agora"
      // acabaram de pegar o titulo (player_encerrar -> ilhacart_player_saiu,
      // home_registrar_retorno) e ele NAO entra em "Continuar assistindo"
      // agora; entra quando sair de la. Sem cartao nem faixa (sem conta, ou
      // titulo terminado), o de sempre: na frente, local.
      cwRetidoSincronizar();
      // O indice CORRENTE do titulo (o player grava por ele, idxAtual).
      if (imdbSaiu[0] && !cw_retido_exclui(imdbSaiu))
        desc_continuar_otimista(cat_indice_vivo(idx, imdbSaiu));
      // MINIMIZAR NA ILHA (pedido do dono, 02/10): saiu no MEIO (esta saida
      // virou a atividade ao vivo, o criterio de home_retorno_vale) com o
      // relogio ligado e "Ao sair do player" = home. A pagina do titulo e o
      // que mais estiver por cima fecham SECOS — quem anima e o quadro do
      // video encolhendo ate a pilula. Terminou o titulo: o fluxo de sempre.
      if (ajustes_saida_player_home() && ilhacart_vivo_seq() != vivoAntes) {
        if (detail_aberto()) detail_fechar_seco();
        if (vertudo_aberta()) vertudo_fechar_seco();
        if (menu_aberto()) menu_fechar();
        trocarTela(TELA_HOME);
        menu_definir_destino(MENU_INICIO);
        ilha_minimizar(fundoVoo[0] ? fundoVoo : NULL);
#ifdef NV_ANDROID
        // A sessao ficou pausada atras da home: o voo comeca DISSOLVENDO a
        // partir do proprio video parado (ilha_minimizar_dissolver).
        ilha_minimizar_dissolver(player_retido());
#endif
      } }
    }
  }

  cwRetidoSincronizar();
  player_atualizar(dt, agora);
  // A barra por cima da pagina: o trailer do fundo fica mudo enquanto ela
  // esta aberta (detail_sob_menu). O pedido de barra da pagina e lido em
  // app_evento, no mesmo evento da ESQUERDA, e abre por cima dela.
  detail_sob_menu(detail_aberto() && menu_aberto());
  detail_atualizar(dt, agora);
  menu_atualizar(dt, agora);
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
      diagnostico_cancelar_vazao();
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
      // Mesma resolucao de "tmdb:<n>" do funil, sem o descarte da sessao
      // retida (o pos-reproducao nunca fez) nem a exigencia de arte.
      abrirTitulo(&it, 0, 0);
    } }

  vertudo_atualizar(dt, agora);
  // O guia publica o fio de carga e bombeia o EPG mesmo fechado — barato, e e
  // o que deixa o zap por CH+/- responder na segunda tecla em vez de na
  // vigesima. Com o player num canal, epg_passo alimenta o banner "agora".
  guia_atualizar(dt, agora);
  if (player_aberto()) epg_passo();
  ctx_atualizar(dt, agora);
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
      abrirTitulo(&it, 0, 1);
    } }
  switch (tela) {
    case TELA_EXPLORAR:   explorar_atualizar(dt, agora);   break;
    case TELA_BUSCA:      busca_atualizar(dt, agora);      break;
    case TELA_BIBLIOTECA: biblioteca_atualizar(dt, agora); break;
    case TELA_AGENDA:     agendaui_atualizar(dt, agora);   break;
    case TELA_PERFIL:     break;
    case TELA_AJUSTES:    ajustes_atualizar(dt, agora);    break;
    case TELA_DIAGNOSTICO: diagnostico_atualizar(dt, agora); break;
    case TELA_LIVETV_DIAG: livetvdiag_atualizar(dt, agora); break;
    default:              home_atualizar(dt, agora);       break;
  }
  // TRAILER NO DESTAQUE: so com a home na frente de tudo. A lista e a mesma
  // ordem de app_evento — o que come tecla antes da home tambem esta na
  // frente dela na tela.
  { const char *porque = homeTrailerBloqueio();
    home_trailer_topo_motivo(porque);
    home_trailer_passo(porque == NULL, dt, agora); }
  trailer_atualizar(agora);
  perfil_atualizar(dt, agora);
  spainel_atualizar(dt, agora);
  recomenda_atualizar(dt, agora);
  recenviar_atualizar(dt, agora);
  pessoas_atualizar(dt, agora);
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
  novidades148_atualizar(dt, agora);
  novidades170_atualizar(dt, agora);
  novidades180_atualizar(dt, agora);
  novidades20_atualizar(dt, agora);
  novidades201_atualizar(dt, agora);
  novidades202_atualizar(dt, agora);
  novcartao_atualizar(dt, agora);
  telemetria_atualizar(dt, agora);
  recintro_atualizar(dt, agora);
  atualizacao_atualizar(dt, agora);
  agendaviso_atualizar(dt, agora);
  avisos_atualizar(dt, agora);
  // Lembretes de programa: so com alguem dentro do app (nao no login nem na
  // escolha de perfil), uma conferencia por segundo no maximo.
  if (sessao_logada() && tela != TELA_LOGIN && tela != TELA_ESCOLHA_PERFIL) {
    char lid[80], lnome[120], lbase[600];
    CatItem it;
    glem_passo(dt, agora);
    // "Assistir" no cartao: o canal em tela cheia, de onde a pessoa estiver.
    if (aguardandoFonte != 2 && glem_pediu_assistir(lid, sizeof lid, lnome, sizeof lnome,
                                                     lbase, sizeof lbase) &&
        guia_item_do_canal(lid, lnome, lbase, &it)) {
      if (player_mini_ativo()) player_fechar_mini();
      tocarCanal(&it);
    }
  }
  pipintro_atualizar(dt, agora);
  if(tela==TELA_SOCIAL) amigoperfil_atualizar(dt, agora);
  if(tela==TELA_ADDONS) addonsui_atualizar(dt, agora);
  if(tela==TELA_PLUGINS) pluginsui_atualizar(dt, agora);
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
      case TELA_SOCIAL:     amigoperfil_desenhar(agora);     break;
      case TELA_ADDONS:     addonsui_desenhar(agora);   break;
      case TELA_PLUGINS:    pluginsui_desenhar(agora);  break;
      case TELA_AJUSTES:    ajustes_desenhar(agora);    break;
      case TELA_DIAGNOSTICO: diagnostico_desenhar(agora); break;
      case TELA_LIVETV_DIAG: livetvdiag_desenhar(agora); break;
      default:              home_desenhar(agora);       break;
    }
  }
  CAMADA_SE(vertudo_aberta());
  if (!detail_cobre_tela()) vertudo_desenhar(agora);
  // Com o painel de Salvos na tela o menu do cartaz e desenhado DEPOIS dele
  // (desenharTelas): e o painel que o abre, e por baixo ele ficaria sob o veu.
  if (!spainel_visivel() && !detail_aberto()) {
    CAMADA_SE(ctx_aberto());
    ctx_desenhar(agora);
  }
  CAMADA_SE(detail_aberto());
  detail_desenhar(agora);
  // Aberto de dentro da pagina do titulo, o menu vai POR CIMA dela.
  if (!spainel_visivel() && detail_aberto() && ctx_aberto()) {
    CAMADA_SE(1);
    ctx_desenhar(agora);
  }
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
  // Com a pagina do titulo aberta, so a barra aberta POR CIMA dela
  // (menu_sobre), ate recolher; a rail fixa nao aparece na pagina.
  if (menu_visivel() && sidebar_permitida() && (!detail_aberto() || menu_sobre())) {
    // Layout Dinamica: a pilula da barra fica no canto das telas do menu, no
    // lugar do titulo delas (que nao e desenhado: menu_pilula_titulo). Na
    // home some com a pagina rolada. Telas fora do menu (Addons, Social,
    // diagnosticos) tem titulo proprio nesse canto: la ela some na hora, e a
    // barra abre pelo ESQUERDA na borda e pela faixa do ponteiro.
    menu_pilula_mostrar(vertudo_aberta() || detail_aberto() ? -1.0f   // "Ver tudo"/colecao/pagina tem cabecalho proprio
                        : tela == TELA_HOME ? home_topo_fracao()
                        : (tela == TELA_EXPLORAR || tela == TELA_BUSCA ||
                           tela == TELA_BIBLIOTECA || tela == TELA_AGENDA ||
                           tela == TELA_PERFIL || tela == TELA_AJUSTES) ? 1.0f : -1.0f);
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
    // A ABERTURA com a marca do icone escolhido (apoiadores); com o Original,
    // a tela de sempre, so com o texto.
    logoapp_marca((GfxRect){ (NV_TELA_W - 160.0f) * 0.5f, 260.0f, 160.0f, 160.0f }, 1.0f);
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
  // ...exceto enquanto o video do preview do guia CRESCE para a tela cheia: ai
  // o guia continua em volta do furo que anda (player_desenhar).
  if (!player_aberto() || player_janela_animando(NULL, NULL, NULL, NULL)) {
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
                      (!ctx_aberto() || ctx_do_painel()) && !menu_aberto() &&
                      !player_mini_ativo();
      spainel_fundo(podeParar, cat_revisao(), desenharAtrasDoPainel, &agora); }
    // Depois do menu: as duas camadas de "Salvos" escurecem a tela inteira e
    // tem de ficar por cima de tudo que a home desenhou, inclusive da rail.
    CAMADA_SE(spainel_aberto());
    if (spainel_visivel()) {
      spainel_desenhar(agora);
      CAMADA_SE(ctx_aberto());
      ctx_desenhar(agora);
    }
  }
  // BRILHO DA INTERFACE DO PLAYER (esmaecer.h): tudo daqui ate a ilha do player
  // multiplica a cor pelo fator (a legenda se isenta em player.c).
  if (player_aberto()) gfx_osd_mult = player_osd_brilho();
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
  // A ILHA DO RELOGIO DENTRO DO PLAYER (plrilha.h), por cima de todas as
  // camadas dele: a pilula da hora, os avisos e o que nasce dela (Audio,
  // Legendas, estilo, carregando, erro) — e a folha de Fontes, que cresce
  // dela (streams.c) e por isso nao a esconde mais.
  if (player_aberto()) { central_desenhar(agora); plrilha_desenhar(agora); }
  gfx_osd_mult = 1.0f;
  // A TELA DO DOLBY VISION EM MKV (dvtela.h) por cima de TUDO do player, ilha
  // inclusive: ela cobre a troca do HDR10 pelo caminho do DV e esvai sobre o
  // filme. Sem o brilho do OSD (e tela, nao controle).
  if (player_aberto()) dvtela_desenhar(agora);
}

// ONDE O RELOGIO DA ILHA CABE (ilha.h). Em toda tela de menu (Home, Explorar,
// Guia, Busca, Biblioteca, Perfil, Social, Add-ons, Agenda), no carrossel
// ampliado ("Ver tudo") e na pagina do titulo — pedido do dono (03/10): antes
// so a Home mostrava. Ele fica sempre na DIREITA. Nunca com uma camada de tela cheia na frente
// (explicadores, cartoes de
// atualizacao/crash/lembrete, menu de contexto, folhas de fontes/episodios/
// faixas): a pilula ficaria boiando sobre o veu de outra coisa. Os AVISOS da
// ilha nao passam por aqui — eles aparecem em qualquer tela fora do player.
static int spotVeuPronto;   // o veu do Spotlight ja esta na copia congelada
static int relogioCabe(void) {
  // AJUSTES NO GLASS UI (mockup de 03/10): o relogio fica no canto, em cima da
  // ilha de categorias — a tela nao tem mais titulo ali. Com folha, vinculo ou
  // teclado por cima ele some, como na home com um cartao na frente.
  if (tela == TELA_AJUSTES) return !menu_aberto() && ajustes_relogio_cabe();
  // CARROSSEL DE TITULOS DA APPLE TV (cartao de vizinhos, sem expandir): sem
  // relogio (dono, 04/10). Ele volta, pela animacao da ilha, quando o cartao
  // estica para a pagina.
  if (detail_aberto() && detail_relogio_oculto()) return 0;
  if (tela == TELA_HOME) { if (!homePronta) return 0; }
  else if (tela != TELA_EXPLORAR && tela != TELA_BUSCA &&
           tela != TELA_BIBLIOTECA && tela != TELA_PERFIL && tela != TELA_SOCIAL &&
           tela != TELA_ADDONS && tela != TELA_PLUGINS && tela != TELA_AGENDA) return 0;
  // O MENU ABERTO NAO ESCONDE MAIS O RELOGIO (Glass UI, mockup "ilha" tela 1):
  // a ilha do menu nasce logo abaixo da do relogio, na mesma margem
  // (ilha_posicionar), e as duas formam a coluna da esquerda.
  if (ctx_aberto() || stream_folha_aberta() || faixas_aberta() || episodios_aberto()) return 0;
  if (sintro_aberto() || novidades_aberto() || novidades11_aberto() || novidades12_aberto() ||
      novidades13_aberto() || novidades131_aberto() || novidades132_aberto() ||
      novidades133_aberto() || novidades134_aberto() || novidades139_aberto() ||
      novidades1312_aberto() || novidades142_aberto() || novidades148_aberto() ||
      novidades170_aberto() || novidades180_aberto() || novidades20_aberto() || novidades201_aberto() || novidades202_aberto() || novcartao_aberto() || telemetria_aberto() || recintro_aberto() ||
      atualizacao_aberta() || agendaviso_aberto() || avisos_cartao_aberto() ||
      glem_cartao_aberto() || recenviar_aberto() || pessoas_aberto() ||
      recomenda_aberta() || pipintro_aberto() || diagnostico_intro_aberto())
    return 0;
  return 1;
}

void app_desenhar(Uint32 agora) {
  // O GUIA DA 2.0 e tela inteira e opaco: nada do app por baixo (uma camada
  // de tela cheia a menos; o fundo dele e o assado do Borrada).
  if (novidades20_aberto() && !registro_aberto() && !player_aberto()) {
    gfx_sem_recorte();
    gfx_cor((GfxRect){0, 0, NV_TELA_W, NV_TELA_H}, 0,
            NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    ponteiro_camada();
    novidades20_desenhar(agora);
    return;
  }
  // O CARTAO DA 2.0.1 cobre quase a tela inteira: o app por baixo nao e
  // pintado (uma camada de tela cheia a menos na Mali). O esmaecer de saida
  // vem pelo caminho de baixo, por cima do app que ja voltou.
  if (novidades201_aberto() && !registro_aberto() && !player_aberto()) {
    gfx_sem_recorte();
    gfx_cor((GfxRect){0, 0, NV_TELA_W, NV_TELA_H}, 0,
            NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    ponteiro_camada();
    novidades201_desenhar(agora);
    return;
  }
  if (novidades202_aberto() && !registro_aberto() && !player_aberto()) {
    gfx_sem_recorte();
    gfx_cor((GfxRect){0, 0, NV_TELA_W, NV_TELA_H}, 0,
            NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    ponteiro_camada();
    novidades202_desenhar(agora);
    return;
  }
  if (novcartao_aberto() && !registro_aberto() && !player_aberto()) {
    gfx_sem_recorte();
    gfx_cor((GfxRect){0, 0, NV_TELA_W, NV_TELA_H}, 0,
            NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    ponteiro_camada();
    novcartao_desenhar(agora);
    return;
  }
  // The highlights modal owns input and covers almost the whole screen.
  // Do not rasterize Home text or punch trailer holes through its backdrop.
  if (novidades180_aberto() && !registro_aberto() && !player_aberto()) {
    gfx_sem_recorte();
    gfx_cor((GfxRect){0, 0, NV_TELA_W, NV_TELA_H}, 0,
            NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    ponteiro_camada();
    novidades180_desenhar(agora);
    return;
  }
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
  // COM O SPOTLIGHT ASSENTADO O FUNDO CONGELA, como o do painel de Salvos
  // (spainel_fundo): a tela de tras e o veu sao pintados UMA vez no FBO do
  // snapshot e os quadros seguintes so copiam — uma camada de tela cheia a
  // menos por quadro (o veu) e a tela de tras inteira a menos. A copia e
  // refeita quando o catalogo muda e em 0,4 / 1,5 / 4 s, para a arte que
  // ainda chegava aparecer. Nada de congelar com video no ar (o PiP pinta o
  // furo) nem com o painel de Salvos, que usa o mesmo FBO.
  { static int pronto, refeitas, regCongelado;
    static unsigned revPronta;
    static Uint32 desde;
    static const Uint32 REFAZ[] = { 400, 1500, 4000 };
    int parar = !registro_aberto() && spot_cheio() && gfx_snap_ok() &&
                !player_mini_ativo() && !player_aberto() && !spainel_visivel();
    spotVeuPronto = 0;
    if (!parar) { pronto = 0; refeitas = 0; }
    else if (revPronta != cat_revisao()) pronto = 0;
    else if (pronto && refeitas < 3 && SDL_GetTicks() - desde >= REFAZ[refeitas]) {
      pronto = 0; refeitas++;
    }
    if (parar && pronto) { gfx_snap_desenhar(); spotVeuPronto = 1; }
    else if (parar) {
      gfx_snap_comecar();
      gfx_sem_recorte();
      glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      desenharTelas(agora);
      spot_veu();
      gfx_sem_recorte();
      gfx_snap_terminar();
      gfx_snap_desenhar();
      if (!refeitas) desde = SDL_GetTicks();
      pronto = 1; revPronta = cat_revisao();
      spotVeuPronto = 1;
    } else if (registro_aberto()) {
      // O PAINEL DE REGISTRO E VIDRO (03/10): a tela de tras e pintada UMA vez
      // no FBO do snapshot ao abrir e os quadros seguintes so copiam. Com o
      // video no ar (o furo do plano de video nao vai para a textura) ou sem
      // FBO, fica a cor de limpeza, como antes.
      if (!regCongelado && gfx_snap_ok() && !player_aberto() && !player_mini_ativo()) {
        gfx_snap_comecar();
        gfx_sem_recorte();
        glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        desenharTelas(agora);
        gfx_sem_recorte();
        gfx_snap_terminar();
        regCongelado = 1;
      }
      if (regCongelado) gfx_snap_desenhar();
    } else desenharTelas(agora);
    if (!registro_aberto()) regCongelado = 0; }
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
  CAMADA_SE(novidades148_aberto());
  if (!registro_aberto()) novidades148_desenhar(agora);
  CAMADA_SE(novidades170_aberto());
  if (!registro_aberto()) novidades170_desenhar(agora);
  CAMADA_SE(novidades180_aberto());
  if (!registro_aberto()) novidades180_desenhar(agora);
  // O esmaecer de saida do guia da 2.0, por cima do app que ja voltou.
  if (!registro_aberto() && novidades20_visivel()) novidades20_desenhar(agora);
  if (!registro_aberto()) novidades201_desenhar(agora);   // so o esmaecer de saida
  if (!registro_aberto()) novidades202_desenhar(agora);
  if (!registro_aberto()) novcartao_desenhar(agora);      // so o esmaecer de saida
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
  // A ILHA DO RELOGIO (ilha.h): mesmas guardas da central — com a pessoa
  // dentro do app. Com o player aberto quem fala e a ilha DO PLAYER
  // (plrilha.h, desenhada em desenharTelas): a mesma pilula, no mesmo canto,
  // com a hora e o "termina as" do titulo. No Guia ela vai para o topo direito
  // (o titulo do guia ocupa o esquerdo).
#ifndef HOME_CARGA_AVISO_MAX_MS
#define HOME_CARGA_AVISO_MAX_MS 20000u
#endif
  static int homeCarregando;
  static Uint32 homeConcluida;
  // O VOO DO PLAYER ATE A PILULA NAO DIVIDE A ILHA COM A CARGA DA HOME
  // (ilha_voo_silencio, tests/ilha_voo_cw.sh): calada enquanto voa e logo
  // depois do pouso; a carga que continuar no ar aparece em seguida.
  static IlhaVooSilencio vooSilencio;
  int vooCalado = ilha_voo_silencio(&vooSilencio, ilha_minimizando(), agora);
  if (tela == TELA_HOME && !detail_aberto() && vooCalado) {
    // Sem "Home carregada" atrasado depois do pouso por uma carga que acabou
    // durante o voo: a que ainda estiver no ar volta a contar no quadro seguinte.
    homeCarregando = 0; homeConcluida = 0;
    ilha_atividade_detalhes(NULL, NULL);
  }
  else if (tela == TELA_HOME && !detail_aberto()) {
    DescHomeCarga load; desc_home_carga(&load);
    // TETO DO AVISO: um catalogo que nunca responde (ou voltas que recomecam)
    // mantem `buscando` ligado e a pilula ficava em "Carregando fileiras…" com
    // a Home ja pronta. Passado o teto o aviso se encerra e a ilha volta ao relogio.
    if (load.ativo && load.ms > HOME_CARGA_AVISO_MAX_MS) load.ativo = 0;
    if (load.ativo) { homeCarregando = 1; homeConcluida = 0; }
    else if (homeCarregando) { homeCarregando = 0; homeConcluida = agora; }
    int concluida = homeConcluida && agora - homeConcluida < 1000u;
    if (load.ativo || concluida || ilha_modal_aberto()) {
      const char *stage = !load.ativo ? i18n("Home carregada") :
        load.fase == 2 ? i18n("Carregando fileiras…") : i18n("Sincronizando a Home…");
      IlhaAtvCarga carga = { NULL, load.ms, load.addonsProntos, load.addonsTotal, load.fileiras, load.falhas, load.ativo };
      ilha_atividade_detalhes(i18n("Carregamento da Home"), stage);
      ilha_atividade_carga(&carga);
      if (load.ativo) {
        char shortStatus[160];
        snprintf(shortStatus, sizeof shortStatus, "%s · %s", i18n("Carregando fileiras…"), i18n("Detalhes"));
        ilha_atividade(shortStatus, -1.0f);
      } else if (concluida) ilha_atividade(i18n("Home carregada"), -1.0f);
    } else ilha_atividade_detalhes(NULL, NULL);
  } else ilha_atividade_detalhes(NULL, NULL);
  if (guia_atualizando_lista()) ilha_atividade(i18n("Atualizando a lista de canais…"), -1.0f);
  if (!registro_aberto() && !player_aberto() && sessao_logada() &&
      tela != TELA_LOGIN && tela != TELA_ESCOLHA_PERFIL) {
    ilha_relogio_visivel(relogioCabe() && ajustes_relogio_ligado());
    ilha_posicionar(1);
    // O painel de Salvos que nasceu da pilula recolhe para ela (o retangulo
    // do quadro anterior) e, enquanto esta na tela, a cobre.
    { float ix = 0, iy = 0, iw = 0, ih = 0;
      int ok = ajustes_relogio_ligado() && ilha_rect(&ix, &iy, &iw, &ih);
      spainel_recolher_para(ok, ix, iy, iw, ih); }
    // O CARTAO DA ATUALIZACAO e a pilula crescida (atualizacao.c): enquanto
    // ele esta na tela, ela fica por baixo, medindo, sem se desenhar.
    ilha_coberta(spainel_da_ilha() || atualizacao_cobre_ilha());
    ilha_desenhar(agora);
  }
  // O cartao do lembrete fica acima do player e da tela: e um aviso com hora.
  CAMADA_SE(glem_cartao_aberto());
  if (!registro_aberto() && sessao_logada() && tela != TELA_LOGIN &&
      tela != TELA_ESCOLHA_PERFIL)
    glem_desenhar(agora);
  CAMADA_SE(recenviar_aberto());
  if (!registro_aberto()) recenviar_desenhar(agora);
  CAMADA_SE(pessoas_aberto());
  if (!registro_aberto()) pessoas_desenhar(agora);
  CAMADA_SE(recomenda_aberta());
  if (!registro_aberto()) recomenda_desenhar(agora);
  CAMADA_SE(pipintro_aberto());
  if (!registro_aberto()) pipintro_desenhar(agora);
  if (!registro_aberto()) spot_desenhar(agora, spotVeuPronto);
  if (!registro_aberto()) celb_desenhar();
  CAMADA_SE(diagnostico_intro_aberto());
  if (!registro_aberto()) diagnostico_intro_desenhar(agora);
  // O MEDIDOR DE DESEMPENHO (Ajustes > Desempenho desta TV) nao e mais camada
  // propria: e conteudo da ilha do relogio (ilha.c, plrilha.c; desempenho.h).
  CAMADA_SE(registro_aberto());
  registro_desenhar();
  // A ilha do relogio fica acima do painel de registro, no canto de sempre.
  if (registro_aberto() && sessao_logada()) {
    ilha_relogio_visivel(ajustes_relogio_ligado());
    ilha_posicionar(1);
    ilha_desenhar(agora);
  }
}

int app_quer_sair(void) { return sair; }
int app_zap_ativo(void) {
  if (tela == TELA_GUIA) return 1;
  return (tela == TELA_PLAYER || player_mini_ativo()) && player_id_canal()[0];
}
int app_central_pode(void) {
  if (central_aberta()) return 1;   // o CH+ que fecha passa pelo mesmo caminho
  return tela != TELA_LOGIN && tela != TELA_ESCOLHA_PERFIL && login_concluido() &&
         perfilsel_concluido() && !registro_aberto() && !spot_aberto() &&
         !teclado_aberto() && !diagnostico_intro_aberto() && !novidades20_aberto() &&
         !novidades201_aberto() && !novidades202_aberto() && !novcartao_aberto();
}

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
  legsync_destruir();   // F05: join dos fios do AutoSync, depois do player
  discord_encerrar();   // a atividade some na hora, sem esperar o Discord notar a queda
  video_encerrar();    // solta o nome LS2 antes do processo sumir (deploy mata sem aviso)
  ajustes_encerrar();
  diagnostico_encerrar();
  livetvdiag_encerrar();
  biblioteca_encerrar();
  explorar_encerrar();
  perfil_encerrar();
  busca_encerrar();
  home_encerrar();
}
