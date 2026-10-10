#include "addons.h"
#include "idbase.h"
#include "idioma.h"
#include "linguas.h"
#include "streams.h"
#include "debrid.h"
#include "rede.h"
#include "js.h"
#include "marco.h"
#include "ondever.h"
#include "addonstats.h"
#include "fontecache.h"
#include "sessao.h"
#include "perfis.h"
#include "servidores.h"
#include "badges.h"
// So para a cache UNICA de manifesto (desc_manifesto_cache_obter/guardar): ver
// a nota grande em sondar(), mais abaixo.
#include "descoberta.h"
#include "legextras.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>

// 16, e nao 12. O sync le ate SY_ADD_MAX (16) addons da conta e entregava a
// lista inteira aqui; o laco de addons_definir_lista cortava no 12o EM SILENCIO
// — o log dizia "12 vindos da conta" como se fossem todos. Quem tem mais de
// doze addons via os ultimos sumirem sem nenhuma explicacao, que e exatamente o
// "some addons were missing (i dont know the reason)" do #42. Os dois tetos
// agora sao o mesmo numero, e o corte, se um dia voltar a acontecer, e dito.
// 32 e nao 16 (30/09): o app oficial passou a 32, e quem tem muitos addons
// perdia justamente os de legenda, que costumam ser os ultimos da lista.
// 64 e nao 32 (#203, 07/10): nos logs 2.0.x, 44 de 464 pessoas leram EXATAMENTE
// 32 linhas da conta e nenhuma leu 33 ou mais — o monte no teto, nao uma
// distribuicao. Quem tem mais de 32 perdia o resto sem aviso (o de legenda,
// tipicamente o ultimo). O teto de lerAddons (sync.c) e o mesmo numero.
#define ADD_MAX 64
#define ADD_PREF_MAX 8
#define ADD_PREF_TAM 24

// `fonte` marca quem realmente entrega stream. Descoberto pelo manifesto: o
// Xperience declara resources catalog/meta/subtitles e NENHUM stream, entao
// respondia {"streams":[]} para tudo. Consultar quem nao fornece e um
// round-trip jogado fora em CADA abertura de titulo.
// `ativo` existe porque a lista precisa MOSTRAR o que esta desligado. Antes, um
// addon desligado na conta era descartado na leitura, entao nao havia como
// ve-lo nem religa-lo pela TV — so pelo celular.
// `sondado` diz se as capacidades vieram do MANIFESTO ou sao a suposicao
// inicial. A diferenca importa na tela: "ainda nao sei" e diferente de "nao
// fornece".
static struct {
  char nome[64]; char base[NV_ADDON_URL_MAX];
  int fonte, catalogo, legenda;
  int meta;      // declara o resource "meta" (ficha e lista de episodios)
  int ativo, sondado;
  char id[96];   // "id" do manifesto; as colecoes da conta apontam para ele
  // Catalogos de canal do manifesto (ver addons_catalogos_canal). `canalLido`
  // separa "nao declara nenhum" de "manifesto ainda nao lido".
  AddCatCanal canal[ADD_CANAL_MAX]; int nCanal, canalLido;
  // #182: consultas SEGUIDAS em que o addon ficou sem resposta nas duas
  // tentativas. Com 2 ou mais ele e dado como fora do ar e nao ganha a segunda
  // chance (senao um addon morto somaria o prazo dela a toda abertura).
  int mudoSeg;
  // O QUE O RESOURCE "meta" DECLARA (addons_aceita_id). `metaTipos` e "|series|movie|"
  // em minuscula, vazio = o manifesto nao disse; `metaPref` sao os idPrefixes
  // do resource (ou, na falta, os do manifesto), nMetaPref = 0 = nao disse.
  char metaTipos[64];
  char metaPref[ADD_PREF_MAX][ADD_PREF_TAM]; int nMetaPref;
} addon[ADD_MAX];
static int nAddon;
static unsigned versaoLista;   // ver addons_versao
static _Atomic AddEstado estado = ADD_PARADO;
static pthread_t fio;
static char alvoId[64], alvoTipo[16];
// The current target is a personal-server item (jfid.h: Jellyfin/Emby/Plex): sources come from the
// server's PlaybackInfo, never from addons (an opaque server id must not
// reach third-party addons).
static int jfAlvo;
// BASE DO ADDON QUE PUBLICOU O ALVO, quando se sabe (canal vindo do guia).
// Com ela, a consulta vai SO a esse addon. Vazia = todos, como sempre foi.
//
// POR QUE: um canal do "Meu Futebol" perguntava fonte ao FrostView, ao
// Debridio, ao AIOStreams — e o FrostView, fora do ar com 408 o dia inteiro,
// segurava a resposta ate o timeout dele. MEDIDO na C9 em 18/09, dono no
// controle: FrostView ligado, dezenas de segundos e cartao de erro; desligado,
// 1,6 s da tecla ate a fonte. O canal nunca foi dele. Nao ha motivo para
// perguntar a quem nao publicou o canal — o id do canal e do addon que o
// declarou, e outro addon nao o conhece (o Meu Futebol responde 404 a id
// alheio, ja se mediu hoje).
static char alvoBase[NV_ADDON_URL_MAX];
// A COPIA QUE O FIO LE. app.c chama addons_definir_origem(NULL) logo depois de
// addons_buscar, e o fio so acorda depois disso: lendo alvoBase direto ele
// achava a origem vazia e perguntava a TODOS os addons — MEDIDO na C9 em
// 18/09 (Debridio e AIOStreams consultados por canal do Meu Futebol com a
// origem definida). A busca copia no disparo; o que o app zere depois nao
// importa mais.
static char fioBase[NV_ADDON_URL_MAX];
static int fioVivo;
static Stream *resultado;
static int nResultado;
static Uint32 resultadoQuando;
static int resultadoCacheavel;
static FontecacheEscopo fioEscopo;
static char pendId[64], pendTipo[16];
// A ORIGEM DO PEDIDO QUE FICOU NA FILA. Zapear com uma busca de canal ainda no
// ar enfileira o canal novo (pendId), mas app.c zera a origem logo depois de
// addons_buscar: ao tirar o pedido da fila alvoBase estava vazio e a consulta
// ia a TODOS os add-ons de fontes. MEDIDO no relato #283 (2.0.2, LG): o canal
// do Fenix TV, que sozinho levava 0,3-0,5 s, passou a esperar ProwJack (16,4 s),
// UnioFlix (9,1 s) e tres timeouts de 12 s. A origem acompanha o pedido.
static char pendBase[NV_ADDON_URL_MAX];
static int pendRenovar;
// O alvo corrente esta sendo buscado pelo PREFETCH do guia (fontecache.c), e
// nao por `fio`: addons_buscar o encontrou a caminho e resolveu esperar em vez
// de repetir. addons_estado e quem colhe. Ver addons_buscar.
static int adotado;
static void dispararBusca(void);
static void progDrenar(void);

// A FOLHA ENCHE A CADA ADDON QUE RESPONDE (#221). Medido no D1 (1.7.0,
// tizen-tpk, 1186 consultas): a primeira fonte chega em 0,85 s (p90), a lista
// so era publicada no fim — p90 de 12,2 s, maximo de 34,7 s —, porque
// consultar() esperava o ultimo addon, e o que nao respondia em 12 s ainda
// ganhava a segunda chance de 20 s ANTES da publicacao. Na TV do relato
// (UE55RU7170, Tizen 5.0): Torrentio com 29 fontes em 0,2 s e a lista na tela
// aos 32,9 s, segurada por um StreamViX mudo nas duas rodadas.
//
// Agora a busca real (so VOD; canal continua de uma vez, a lista dele e de um
// addon so) deixa cada resposta nesta fila, no fio de rede, e addons_estado /
// addons_drenar publicam no fio da UI com stream_lista_acrescentar. A lista
// final continua sendo montada por consultar() NA ORDEM DOS ADDONS para o
// cache; a da tela e a mesma ordem (streams.c ordena a exibicao por addon).
//
// `progEstado` diz quem falta, para a folha: 1 = esperando, 2 = respondeu,
// 3 = desistiu (sem resposta e sem segunda chance pela frente).
//
// OS PLUGINS (F09) SAO MAIS ORIGENS NA MESMA FILA. Cada scraper da origem
// extra (plugins.c) tem a sua vaga em `progEstadoEx`, com o nome dele, e as
// fontes dele entram na folha com ordem ADD_MAX + k: depois de todos os
// addons, na ordem do manifesto.
#define ADD_EXTRA_MAX 160   // = PLUG_SCRAPERS_MAX (plugins.h)
typedef struct { int idx; Stream *a; int n; } Chegada;
static pthread_mutex_t progTrava = PTHREAD_MUTEX_INITIALIZER;
static unsigned char progEstado[ADD_MAX];
static unsigned char progEstadoEx[ADD_EXTRA_MAX];
static char progNomeEx[ADD_EXTRA_MAX][48];
static Chegada progFila[ADD_MAX * 2 + ADD_EXTRA_MAX];
static int progN;
static int progLigado, progPublicou, progExtraPublicou;
// O QUE JA FOI PUBLICADO NESTA BUSCA, na ordem de chegada (bloqueador 2.0.3,
// TCL do dono). Se a lista da tela for apagada com a busca no ar e o MESMO alvo
// for pedido de novo, a busca em curso nao repete a rede (buscarPedido volta
// cedo) — e sem isto as fontes de add-on ja publicadas sumiam enquanto as de
// plugin que chegassem depois entravam: "28 fontes ... descartadas", depois
// "+2 de MegaEmbed (lista com 2)" e o automatico escolhendo com 2. So o fio da
// UI mexe aqui (progDrenar, buscarPedido, progLimpar). Custo: uma copia das
// fontes enquanto a busca dura; progLimpar solta no fim.
static Chegada progPub[ADD_MAX * 2 + ADD_EXTRA_MAX];
static int progPubN;
static Uint32 progInicio;

static void progMarcar(int i, const Stream *a, int n, int estadoNovo) {
  Stream *copia = NULL;
  if (i < 0 || i >= ADD_MAX) return;
  if (a && n > 0) {
    copia = malloc(sizeof(Stream) * (size_t)n);
    if (copia) memcpy(copia, a, sizeof(Stream) * (size_t)n);
  }
  pthread_mutex_lock(&progTrava);
  progEstado[i] = (unsigned char)estadoNovo;
  if (copia && progN < (int)(sizeof progFila / sizeof *progFila)) {
    progFila[progN].idx = i; progFila[progN].a = copia; progFila[progN].n = n;
    progN++; copia = NULL;
  }
  pthread_mutex_unlock(&progTrava);
  free(copia);
}

// A parte `k` da origem extra (um scraper). Mesmo contrato de progMarcar,
// com a ordem de exibicao ADD_MAX + k.
static void progMarcarEx(int k, const char *nome, const Stream *a, int n, int estadoNovo) {
  Stream *copia = NULL;
  if (k < 0 || k >= ADD_EXTRA_MAX) return;
  if (a && n > 0) {
    copia = malloc(sizeof(Stream) * (size_t)n);
    if (copia) memcpy(copia, a, sizeof(Stream) * (size_t)n);
  }
  pthread_mutex_lock(&progTrava);
  progEstadoEx[k] = (unsigned char)estadoNovo;
  if (nome) snprintf(progNomeEx[k], sizeof progNomeEx[k], "%s", nome);
  if (copia && progN < (int)(sizeof progFila / sizeof *progFila)) {
    progFila[progN].idx = ADD_MAX + k; progFila[progN].a = copia; progFila[progN].n = n;
    progN++; copia = NULL;
  }
  pthread_mutex_unlock(&progTrava);
  free(copia);
}

static void progLimpar(void) {
  int q;
  pthread_mutex_lock(&progTrava);
  for (q = 0; q < progN; q++) free(progFila[q].a);
  progN = 0;
  memset(progEstado, 0, sizeof progEstado);
  memset(progEstadoEx, 0, sizeof progEstadoEx);
  pthread_mutex_unlock(&progTrava);
  for (q = 0; q < progPubN; q++) free(progPub[q].a);
  progPubN = 0;
}

void addons_capturar_escopo(FontecacheEscopo *e) {
  memset(e, 0, sizeof *e);
  snprintf(e->conta, sizeof e->conta, "%s", sessao_usuario());
  e->perfil = perfis_ativo();
  e->addons = versaoLista;
  e->geracao = fontecache_vod_geracao();
}

static int escopoAindaAtual(const FontecacheEscopo *e) {
  FontecacheEscopo atual;
  addons_capturar_escopo(&atual);
  return atual.perfil == e->perfil && atual.addons == e->addons &&
         atual.geracao == e->geracao && !strcmp(atual.conta, e->conta);
}

static int alvoVod(void) {
  return !strcmp(alvoTipo, "movie") || !strcmp(alvoTipo, "series");
}

static void listaMudou(void) {
  versaoLista++;
  fontecache_vod_limpar();
}

// A BASE de um addon a partir da URL guardada (arquivo local ou conta). A URL
// aponta para o manifesto; a base e ela sem o sufixo, e e dela que saem
// <base>/catalog/..., <base>/stream/... e <base>/manifest.json.
//
// A REGRA E UMA SO, E MORA AQUI. Havia tres copias dela (arquivo local, conta
// e a comparacao "a lista mudou?") e as tres tinham o mesmo furo: so
// reconheciam "/manifest.json" no FIM EXATO da string. O Bingecat entrega a
// URL como .../manifest.json?ver=N — com query string — e o sufixo nao casava.
// A "base" ficava sendo a URL inteira, e TODO pedido virava
// .../manifest.json?ver=N/catalog/movie/<id>.json. O servidor dele responde a
// isso com o proprio manifesto (HTTP 200, corpo com "catalogs" e sem "metas"),
// entao lerCatalogo via "respondeu, zero itens" e o log dizia "catalogo vazio"
// para os 21 catalogos, inclusive "Because you watched Silo" — um catalogo que,
// pedido pela base certa, devolve 12 titulos. E a issue #24 inteira: nao era
// falta de parametro, era a URL.
//
// A QUERY AGORA FICA (paridade com o Nuvio oficial, ver nv_addon_base em
// addonurl.h): o Bingecat continua certo porque todo pedido e montado com
// nv_addon_url, que poe o caminho ANTES da query
// (".../catalog/movie/<id>.json?ver=N", o que o Nuvio web e o Stremio pedem).
// Medido na TV do dono que, para o Bingecat, o catalogo responde igual com ou
// sem a query; um addon que guarde a configuracao nela precisa dela.
static void baseNormalizada(const char *url, char *dst, size_t tam) {
  nv_addon_base(url, dst, tam);
}

// O pedido ao addon `i` coube no buffer? Ver nv_addon_pedido_coube (addonurl.h).
static int pedidoCoube(int i, int w, size_t tam) {
  return nv_addon_pedido_coube(addon[i].nome, w, tam);
}

// --- leitura do arquivo de configuracao -------------------------------------

// Perfil cuja conta mandou a lista atual; 0 = pacote ou nada. Ver addons.h.
// Lida pela descoberta (outro fio) e escrita pelo sync: atomica.
static int perfilLista;
void addons_marcar_da_conta(int perfil) {
  __atomic_store_n(&perfilLista, perfil > 0 ? perfil : 0, __ATOMIC_SEQ_CST);
}
int  addons_perfil_da_lista(void) { return __atomic_load_n(&perfilLista, __ATOMIC_SEQ_CST); }

int addons_carregar(const char *dirArte) {
  // A linha e nome<TAB>url<TAB>colunas: a URL inteira mais folga para o resto.
  char caminho[600], linha[NV_ADDON_URL_MAX + 256];
  FILE *f;
  addons_marcar_da_conta(0);
  snprintf(caminho, sizeof caminho, "%s/addons.txt", dirArte ? dirArte : ".");
  f = fopen(caminho, "r");
  if (!f) { printf("[addons] sem %s\n", caminho); return 0; }
  nAddon = 0;
  while (nAddon < ADD_MAX && fgets(linha, sizeof linha, f)) {
    char *tab = strchr(linha, '\t');
    char *fim;
    // LINHA MAIOR QUE O BUFFER: o fgets devolveria o resto dela como se fosse
    // outra linha. Come o resto e pula o addon, dizendo — metade de uma URL e
    // outra URL (addonurl.h).
    if (!strchr(linha, '\n') && !feof(f)) {
      int c, extra = 0;
      while ((c = fgetc(f)) != EOF && c != '\n') extra++;
      if (tab) *tab = 0;
      nv_addon_url_cabe(tab ? linha : "addons.txt", sizeof linha - 1 + (size_t)extra);
      continue;
    }
    // TAB e nao "|" como separador: nome de addon contem "|" de verdade
    // ("AIOStreams | ElfHosted") e partir no primeiro pipe corrompia a URL.
    if (!tab) continue;
    *tab = 0;
    fim = tab + 1 + strlen(tab + 1);
    while (fim > tab + 1 && (fim[-1] == '\n' || fim[-1] == '\r' || fim[-1] == ' ')) *--fim = 0;
    if (linha[0] == '#' || !tab[1]) continue;
    { const char *fimUrl = strchr(tab + 1, '\t');
      size_t lenUrl = fimUrl ? (size_t)(fimUrl - (tab + 1)) : strlen(tab + 1);
      if (!nv_addon_url_cabe(linha, lenUrl)) continue; }
    // Terceira coluna (opcional): 1 = fornece stream. Ausente vale 1, para
    // arquivo antigo continuar funcionando.
    addon[nAddon].fonte = 1;
    addon[nAddon].catalogo = 1;
    addon[nAddon].legenda = 0;
    { char *tab2 = strchr(tab + 1, '\t');
      if (tab2) {
        char *tab3;
        *tab2 = 0;
        tab3 = strchr(tab2 + 1, '\t');
        if (tab3) {
          char *tab4 = strchr(tab3 + 1, '\t');
          *tab3 = 0;
          if (tab4) { *tab4 = 0; addon[nAddon].legenda = atoi(tab4 + 1); }
          addon[nAddon].catalogo = atoi(tab3 + 1);
        }
        addon[nAddon].fonte = atoi(tab2 + 1);
      } }
    // LIGADO. O arquivo nao tem coluna de ligado/desligado — quem o escreve
    // esta dizendo "use estes". Faltava esta linha, e a entrada nascia com
    // ativo=0 (o vetor e estatico, nasce zerado): addons_consultar exige
    // `ativo && fonte`, entao NENHUM addon do arquivo era consultado por
    // fonte, por legenda (laco de buscarLegendas) nem por catalogo
    // (addons_tem_catalogo) — apareciam na lista e nao serviam para nada. So
    // nao deu relato porque a lista da conta chega logo depois e substitui a
    // do arquivo em quase todo aparelho; quem nao tem conta ficava sem nada.
    addon[nAddon].ativo = 1;
    snprintf(addon[nAddon].nome, sizeof addon[nAddon].nome, "%s", linha);
    baseNormalizada(tab + 1, addon[nAddon].base, sizeof addon[nAddon].base);
    nAddon++;
  }
  fclose(f);
  listaMudou();
  { int f = 0, k;
    for (k = 0; k < nAddon; k++) f += addon[k].fonte;
    printf("[addons] %d configurados, %d fornecem stream\n", nAddon, f); }
  return nAddon;
}

// A lista que chegou e IGUAL a que ja esta valendo?
//
// Sem esta pergunta, `remontar` era ligado a cada ciclo de sync so porque a
// resposta chegou — e a resposta chega a cada cinco minutos, identica. O
// resultado era um ciclo de descoberta completo (Trakt, todos os manifestos,
// todos os catalogos) a cada cinco minutos, para sempre, e a home reassentando
// junto. Tambem e ele que republica o catalogo por baixo de quem esta parado
// numa pagina de titulo.
static int listaIgual(const AddonRemoto *nova, int n) {
  int i, k = 0;
  for (i = 0; i < n && k < ADD_MAX; i++) {
    char base[NV_ADDON_URL_MAX];
    if (!nova[i].url[0]) continue;
    baseNormalizada(nova[i].url, base, sizeof base);
    if (k >= nAddon) return 0;
    if (strcmp(addon[k].base, base)) return 0;
    if (addon[k].ativo != (nova[i].ativo ? 1 : 0)) return 0;
    k++;
  }
  return k == nAddon;
}

int addons_definir_lista(const AddonRemoto *nova, int n) {
  int i, aceitos = 0;
  if (!nova || n <= 0) {
    // Vazio nao substitui. Ver o comentario no cabecalho: uma resposta vazia
    // nao se distingue de uma delecao, e a diferenca entre as duas e a pessoa
    // ficar ou nao sem nenhuma fonte.
    printf("[addons] lista da conta veio vazia; mantendo a local (%d)\n", nAddon);
    return 0;
  }
  // NADA MUDOU: nao substitui e diz que nao mudou. Substituir seria pior do que
  // inutil — o laco abaixo zera `sondado` e o `id` do manifesto, entao reaplicar
  // uma lista identica jogaria fora o que a sonda aprendeu e faria as colecoes
  // da conta perderem a URL dos addons ate a proxima leitura.
  if (listaIgual(nova, n)) return 0;
  for (i = 0; i < n && aceitos < ADD_MAX; i++) {
    // Addon DESLIGADO tambem entra: ele aparece na lista e pode ser religado
    // aqui. So nao e consultado (ver ativoParaConsulta).
    if (!nova[i].url[0]) continue;
    // ZERAR A ENTRADA INTEIRA, e nao so os campos que a conta traz.
    //
    // `id` (o do manifesto) so e preenchido pela sonda, e este laco nunca o
    // tocava: numa segunda chamada — e ela acontece a cada ciclo de sync — o
    // slot herdava o id do addon que estava ANTES naquela posicao, agora com
    // uma base diferente. addons_base_por_id passava a devolver a base ERRADA
    // para aquele id, e quem consulta esse mapa sao as fontes das colecoes da
    // conta (colecoes.c): a pasta abria o catalogo de outro addon, ou nenhum.
    memset(&addon[aceitos], 0, sizeof addon[aceitos]);
    snprintf(addon[aceitos].nome, sizeof addon[aceitos].nome, "%s",
             nova[i].nome[0] ? nova[i].nome : "Addon");
    baseNormalizada(nova[i].url, addon[aceitos].base, sizeof addon[aceitos].base);
    // A conta nao diz o que cada addon fornece; o manifesto e que diria, e
    // consultar todos no arranque custaria uma viagem por addon. Assumir que
    // fornece tudo faz no maximo uma consulta vazia a mais por titulo — o
    // contrario (assumir que nao fornece) esconderia fontes de verdade.
    // Ate o manifesto responder, assume-se que fornece tudo — inclusive
    // LEGENDA, que antes ficava em 0 e contradizia o comentario acima. O
    // efeito de legenda=0 era pior do que uma consulta a mais: buscarLegendas
    // pula quem nao declara legenda, entao numa conta sincronizada o
    // OpenSubtitles nunca era consultado e nao havia legenda nenhuma.
    addon[aceitos].fonte = 1;
    addon[aceitos].catalogo = 1;
    addon[aceitos].legenda = 1;
    addon[aceitos].sondado = 0;
    addon[aceitos].canalLido = 0; addon[aceitos].nCanal = 0; addon[aceitos].mudoSeg = 0;
    addon[aceitos].ativo = nova[i].ativo ? 1 : 0;
    aceitos++;
  }
  if (aceitos == 0) {
    printf("[addons] a conta veio sem addons utilizaveis; mantendo a local\n");
    return 0;
  }
  nAddon = aceitos;
  printf("[addons] %d vindos da conta\n", nAddon);
  { static int avisou; int q, off = 0;
    for (q = 0; q < nAddon; q++) if (!addon[q].ativo) off++;
    if (off && !avisou) {
      avisou = 1;
      printf("[addons] %d desligados na conta: nao consultados\n", off);
    } }
  // DIZER QUANDO CORTOU. Um addon que some sem uma linha de log e indistinguivel
  // de um addon que a conta nao tem.
  { int uteis = 0, q;
    for (q = 0; q < n; q++) if (nova[q].url[0]) uteis++;
    if (uteis > aceitos)
      printf("[addons] %d da conta ficaram de fora: o app guarda no maximo %d\n",
             uteis - aceitos, ADD_MAX); }
  listaMudou();
  return 1;
}

int addons_exportar(AddonRemoto *saida, int max) {
  int i, k = 0;
  for (i = 0; i < nAddon && k < max; i++) {
    snprintf(saida[k].nome, sizeof saida[k].nome, "%s", addon[i].nome);
    snprintf(saida[k].url, sizeof saida[k].url, "%s", addon[i].base);
    saida[k].ativo = addon[i].ativo;
    k++;
  }
  return k;
}

void addons_esquecer(void) {
  memset(addon, 0, sizeof addon);
  nAddon = 0;
  addons_marcar_da_conta(0);
  listaMudou();
  printf("[addons] lista esquecida (saiu da conta)\n");
}

int addons_n(void) { return nAddon; }
// Sobe a cada mudanca na LISTA (conta, liga/desliga, adicao, esquecer). Quem
// ja consultou fontes com a lista antiga refaz a consulta ao ver mudar.
unsigned addons_versao(void) { return versaoLista; }

// O id do manifesto ("com.frostview"), ou "" enquanto a sonda nao o leu. E o
// prefixo da homeCatalogKey dos catalogos deste addon — e por isso a poda de
// fantasmas de fileiras.c pede por ele.
const char *addons_id_manifesto(int i) {
  return (i >= 0 && i < nAddon) ? addon[i].id : "";
}
// Nome de exibicao pelo id do manifesto; "" quando nenhum addon da lista tem
// esse id (addon removido, ou sonda ainda nao leu o manifesto).
const char *addons_nome_por_id(const char *id) {
  int i;
  if (!id || !id[0]) return "";
  for (i = 0; i < nAddon; i++) if (addon[i].id[0] && !strcmp(addon[i].id, id)) return addon[i].nome;
  return "";
}

const char *addons_base(int i) {
  return (i >= 0 && i < nAddon) ? addon[i].base : "";
}

// Quarta coluna de addons.txt. Como a de stream, ausente vale 1 — arquivo
// antigo continua funcionando, so faz uma consulta a mais que pode dar vazio.
int addons_tem_catalogo(int i) {
  return (i >= 0 && i < nAddon) ? (addon[i].ativo && addon[i].catalogo) : 0;
}
AddEstado addons_estado(void) {
  AddEstado e = atomic_load(&estado);
  if (jfAlvo) {
    Stream *l = NULL;
    int n = 0, r = servidores_fontes_colher(alvoId, &l, &n), i;
    if (r == JF_FONTES_PENDENTE) return ADD_BUSCANDO;
    jfAlvo = 0;
    if (r == JF_FONTES_PRONTO) {
      for (i = 0; i < n; i++) {
        char t[2400];
        snprintf(t, sizeof t, "%s %s", l[i].rotulo, l[i].descricao);
        l[i].badges = badges_detectar(t);
      }
      stream_definir_lista(l, n);
      printf("[addons] %d personal-server source(s)\n", n);
    } else {
      stream_definir_lista(NULL, 0);
      printf("[addons] personal-server sources unavailable\n");
    }
    free(l);
    estado = n ? ADD_PRONTO : ADD_VAZIO;
    return atomic_load(&estado);
  }
  // As respostas que ja chegaram vao para a folha antes de tudo (#221).
  progDrenar();
  // Publica no fio da UI: nenhum desenho observa uma lista parcialmente escrita.
  if (fioVivo && e != ADD_BUSCANDO) {
    pthread_join(fio, NULL);
    fioVivo = 0;
    // O que o fio deixou na fila depois da ultima drenagem.
    progDrenar();
    if (alvoVod() && !escopoAindaAtual(&fioEscopo)) {
      // Conta/perfil/configuracao mudaram durante a rede. Essa resposta nao
      // pertence mais a tela, nem pode recriar o cache depois do logout.
      free(resultado); resultado = NULL; nResultado = 0;
      estado = ADD_PARADO;
      if (progPublicou && !pendId[0]) stream_invalidar("account or profile changed during the search");
    } else {
      if (alvoVod() && resultadoCacheavel)
        fontecache_vod_guardar(alvoId, alvoTipo, fioBase, &fioEscopo,
                              resultado, nResultado, resultadoQuando);
      // PUBLICADA AOS POUCOS, a lista da tela ja e a inteira: substitui-la
      // agora zeraria o foco da folha, a fonte tocando e a verificacao em
      // curso — exatamente o que a publicacao por addon existe para preservar.
      if (!pendId[0] && !progPublicou) {
        if (alvoVod())
          stream_definir_lista_idade(resultado, nResultado, SDL_GetTicks() - resultadoQuando);
        else stream_definir_lista(resultado, nResultado);
      }
      if (progPublicou)
        printf("[addons] busca completa em %u ms\n", (unsigned)(SDL_GetTicks() - progInicio));
    }
    progLigado = 0; progPublicou = 0; progExtraPublicou = 0;
    progLimpar();
    free(resultado); resultado = NULL; nResultado = 0;
    if (pendId[0]) {
      char id[64], tipo[16];
      int renovar = pendRenovar;
      snprintf(id, sizeof id, "%s", pendId);
      snprintf(tipo, sizeof tipo, "%s", pendTipo);
      pendId[0] = 0;
      pendRenovar = 0;
      snprintf(alvoBase, sizeof alvoBase, "%s", pendBase);
      pendBase[0] = 0;
      if (renovar) addons_buscar_renovar(id, tipo);
      else addons_buscar(id, tipo);
      return ADD_BUSCANDO;
    }
    e = atomic_load(&estado);
  }
  // O alvo esta a caminho pelo prefetch: colhe quando chegar. Se o prefetch
  // cedeu ou nao trouxe nada, a busca real sai daqui — o pedido nunca fica
  // pendurado em "buscando" por causa de um atalho que nao deu certo.
  if (adotado && !fioVivo) {
    Stream *l; int n;
    int r = fontecache_pegar(alvoId, alvoTipo, &l, &n);
    if (r == FC_ACERTO) {
      adotado = 0;
      printf("[addons] %s: %d fontes do prefetch\n", alvoId, n);
      stream_definir_lista(l, n);
      free(l);
      estado = n ? ADD_PRONTO : ADD_VAZIO;
      return atomic_load(&estado);
    }
    if (r == FC_NADA) { adotado = 0; dispararBusca(); }
    return ADD_BUSCANDO;
  }
  // Busca principal ociosa: e a vez do prefetch pendente, se houver.
  if (e != ADD_BUSCANDO && !fioVivo) fontecache_avancar();
  return e;
}

// --- publicacao por addon (#221) ---------------------------------------------
// No fio da UI. Resposta de busca que ja nao e a da tela (pedido novo na fila,
// conta/perfil trocados) e jogada fora aqui, sem nunca chegar a lista.
static void progDrenar(void) {
  Chegada local[ADD_MAX * 2 + ADD_EXTRA_MAX];
  int q, k;
  if (!progLigado) return;
  pthread_mutex_lock(&progTrava);
  k = progN;
  memcpy(local, progFila, sizeof(Chegada) * (size_t)k);
  progN = 0;
  pthread_mutex_unlock(&progTrava);
  for (q = 0; q < k; q++) {
    if (!pendId[0] && escopoAindaAtual(&fioEscopo)) {
      if (!progPublicou)
        printf("[addons] primeira resposta em %u ms: %s\n",
               (unsigned)(SDL_GetTicks() - progInicio),
               local[q].idx < ADD_MAX ? addon[local[q].idx].nome : local[q].a[0].provedor);
      if (local[q].idx >= ADD_MAX && !progExtraPublicou) {
        progExtraPublicou = 1;
        printf("[plugins] primeira fonte de plugin em %u ms: %s\n",
               (unsigned)(SDL_GetTicks() - progInicio), local[q].a[0].provedor);
      }
      stream_lista_acrescentar(local[q].a, local[q].n, local[q].idx);
      progPublicou = 1;
      if (progPubN < (int)(sizeof progPub / sizeof *progPub)) {
        progPub[progPubN++] = local[q];
        continue;
      }
    }
    free(local[q].a);
  }
}

// A lista da tela nao e mais a desta busca (apagada no meio: troca de alvo
// desfeita, "episode changed"), e o mesmo alvo foi pedido de novo: ela volta a
// ser desta busca, com tudo o que ja chegou — add-on e plugin juntos, na ordem
// em que chegaram — e o resto continua entrando aos poucos.
static void progRepublicar(void) {
  int q, k = 0;
  stream_definir_lista(NULL, 0);
  for (q = 0; q < progPubN; q++) {
    stream_lista_acrescentar(progPub[q].a, progPub[q].n, progPub[q].idx);
    k += progPub[q].n;
  }
  printf("[addons] %s: lista apagada com a busca no ar; %d fonte(s) ja recebida(s) de volta\n",
         alvoId, k);
  fflush(stdout);
}

void addons_drenar(void) { progDrenar(); }

int addons_busca_parcial(void) {
  return fioVivo && progLigado && atomic_load(&estado) == ADD_BUSCANDO;
}

unsigned addons_busca_ms(void) {
  return addons_busca_parcial() ? SDL_GetTicks() - progInicio : 0;
}

static void juntarNome(char *nomes, unsigned tam, size_t *usado, int k, const char *nome) {
  if (nomes && tam && *usado + 1 < tam) {
    int w = snprintf(nomes + *usado, tam - *usado, "%s%s", k ? ", " : "", nome);
    if (w > 0) *usado += (size_t)w;
    if (*usado >= tam) *usado = tam - 1;
  }
}

int addons_faltam_tipo(char *nomes, unsigned tam, int *plugins) {
  int i, k = 0, p = 0;
  size_t usado = 0;
  if (nomes && tam) nomes[0] = 0;
  if (plugins) *plugins = 0;
  if (!addons_busca_parcial()) return 0;
  pthread_mutex_lock(&progTrava);
  for (i = 0; i < nAddon && i < ADD_MAX; i++) {
    if (progEstado[i] != 1) continue;
    juntarNome(nomes, tam, &usado, k, addon[i].nome);
    k++;
  }
  // Os plugins depois dos addons, como na folha.
  for (i = 0; i < ADD_EXTRA_MAX; i++) {
    if (progEstadoEx[i] != 1) continue;
    juntarNome(nomes, tam, &usado, k, progNomeEx[i]);
    k++; p++;
  }
  pthread_mutex_unlock(&progTrava);
  if (plugins) *plugins = p;
  return k;
}

int addons_faltam(char *nomes, unsigned tam) { return addons_faltam_tipo(nomes, tam, NULL); }

// QUEM NAO SEGURA MAIS A DECISAO AUTOMATICA (#202, addonstats.h). A espera de
// Ajustes ("Espera pelos add-ons") vale como limite do "lento": um add-on que
// nesta TV passa dela na maioria das buscas quase nunca chega a tempo, e
// esperar por ele so gasta o prazo inteiro. 0 = ninguem e ignorado (Instantaneo
// nao espera ninguem; "Todos os add-ons" e a pessoa pedindo para esperar).
// Ele segue sendo consultado e, quando responde, entra na lista como sempre.
static unsigned espDecisaoMs;
void addons_definir_espera_decisao(int ms) { espDecisaoMs = ms > 0 ? (unsigned)ms : 0; }
static int naoSegura(const char *nome) {
  return nome && nome[0] && addonstats_ignoravel(nome, espDecisaoMs);
}

int addons_faltam_decisivos(void) {
  int i, k = 0;
  if (!addons_busca_parcial()) return 0;
  pthread_mutex_lock(&progTrava);
  for (i = 0; i < nAddon && i < ADD_MAX; i++)
    if (progEstado[i] == 1 && !naoSegura(addon[i].nome)) k++;
  for (i = 0; i < ADD_EXTRA_MAX; i++)
    if (progEstadoEx[i] == 1 && !naoSegura(progNomeEx[i])) k++;
  pthread_mutex_unlock(&progTrava);
  return k;
}

// O que a ilha do player diz sobre a espera (inicio.h): o primeiro que a
// decisao ainda espera, e o primeiro que desistiu nesta busca.
void addons_inicio_info(AddonsInicioInfo *o) {
  int i;
  memset(o, 0, sizeof *o);
  if (!addons_busca_parcial()) return;
  pthread_mutex_lock(&progTrava);
  for (i = 0; i < nAddon && i < ADD_MAX; i++) {
    if (progEstado[i] == 1 && !naoSegura(addon[i].nome)) {
      if (!o->pendentes++) {
        snprintf(o->pendente, sizeof o->pendente, "%s", addon[i].nome);
        o->pendenteMudo = addonstats_mudo_seguidas(addon[i].nome) > 0;
      }
    } else if (progEstado[i] == 3 && !o->semResposta++)
      snprintf(o->semRespostaNome, sizeof o->semRespostaNome, "%s", addon[i].nome);
  }
  for (i = 0; i < ADD_EXTRA_MAX; i++) {
    if (progEstadoEx[i] == 1 && !naoSegura(progNomeEx[i])) {
      if (!o->pendentes++) {
        snprintf(o->pendente, sizeof o->pendente, "%s", progNomeEx[i]);
        o->pendenteMudo = addonstats_mudo_seguidas(progNomeEx[i]) > 0;
      }
    } else if (progEstadoEx[i] == 3 && !o->semResposta++)
      snprintf(o->semRespostaNome, sizeof o->semRespostaNome, "%s", progNomeEx[i]);
  }
  pthread_mutex_unlock(&progTrava);
}

int addons_pendente_antes(int idx) {
  int i, r = 0;
  if (!addons_busca_parcial()) return 0;
  pthread_mutex_lock(&progTrava);
  for (i = 0; i < idx && i < nAddon && i < ADD_MAX; i++)
    if (progEstado[i] == 1 && !naoSegura(addon[i].nome)) { r = 1; break; }
  for (i = 0; !r && i < idx - ADD_MAX && i < ADD_EXTRA_MAX; i++)
    if (progEstadoEx[i] == 1 && !naoSegura(progNomeEx[i])) r = 1;
  pthread_mutex_unlock(&progTrava);
  return r;
}

int addons_pendente_nome(const char *nome) {
  int i, r = 0;
  if (!nome || !*nome || !addons_busca_parcial()) return 0;
  pthread_mutex_lock(&progTrava);
  for (i = 0; i < nAddon && i < ADD_MAX; i++)
    if (progEstado[i] == 1 && !strcasecmp(addon[i].nome, nome)) { r = 1; break; }
  for (i = 0; !r && i < ADD_EXTRA_MAX; i++)
    if (progEstadoEx[i] == 1 && !strcasecmp(progNomeEx[i], nome)) r = 1;
  pthread_mutex_unlock(&progTrava);
  return r;
}

// O MELHOR GRUPO (fonteregra.h, #202) que algum addon/plugin que ainda nao
// respondeu pode trazer: o menor `f(nome, plugin)` >= 0 entre os pendentes;
// 99 quando nenhum pendente pode mudar a escolha.
int addons_pendente_grupo_min(int (*f)(const char *nome, int plugin, void *u), void *u) {
  int i, m = 99;
  if (!f || !addons_busca_parcial()) return m;
  pthread_mutex_lock(&progTrava);
  for (i = 0; i < nAddon && i < ADD_MAX; i++)
    if (progEstado[i] == 1 && !naoSegura(addon[i].nome)) { int g = f(addon[i].nome, 0, u); if (g >= 0 && g < m) m = g; }
  for (i = 0; i < ADD_EXTRA_MAX; i++)
    if (progEstadoEx[i] == 1 && !naoSegura(progNomeEx[i])) { int g = f(progNomeEx[i], 1, u); if (g >= 0 && g < m) m = g; }
  pthread_mutex_unlock(&progTrava);
  return m;
}

int addons_ocupado(void) {
  return fioVivo || adotado || atomic_load(&estado) == ADD_BUSCANDO;
}

// --- leitura tolerante de JSON ----------------------------------------------
// Um analisador completo nao se paga aqui: o formato e conhecido e raso, e o
// que importa e nunca travar com campo faltando. Cada funcao devolve o que
// achou ou nada, e quem chama decide.

static const char *pulaEspaco(const char *p) {
  while (*p && (unsigned char)*p <= ' ') p++;
  return p;
}

// Copia o valor textual de "chave" dentro do objeto que comeca em `obj`,
// respeitando escapes. Devolve 1 se achou.
static int campoTexto(const char *obj, const char *fimObj, const char *chave,
                      char *dst, size_t tam) {
  char busca[48];
  const char *p;
  size_t k = 0;
  snprintf(busca, sizeof busca, "\"%s\"", chave);
  p = strstr(obj, busca);
  if (!p || p >= fimObj) return 0;
  p = pulaEspaco(p + strlen(busca));
  if (*p != ':') return 0;
  p = pulaEspaco(p + 1);
  if (*p != '"') return 0;
  p++;
  while (*p && *p != '"' && k + 1 < tam) {
    if (*p == '\\' && p[1]) {
      p++;
      // \u..... vira "?" de proposito: os nomes vem cheios de emoji e o texto
      // e so para exibicao. Decodificar UTF-16 aqui seria trabalho sem retorno.
      if (*p == 'u') { p += 5; dst[k++] = ' '; continue; }
      if (*p == 'n' || *p == 't' || *p == 'r') { p++; dst[k++] = ' '; continue; }
    }
    dst[k++] = *p++;
  }
  dst[k] = 0;
  return k > 0;
}

// Acha o fim do objeto JSON que comeca em `p` (que aponta para '{').
static const char *fimObjeto(const char *p) {
  int prof = 0, texto = 0;
  for (; *p; p++) {
    if (texto) { if (*p == '\\') p++; else if (*p == '"') texto = 0; continue; }
    if (*p == '"') texto = 1;
    else if (*p == '{') prof++;
    else if (*p == '}' && --prof == 0) return p + 1;
  }
  return p;
}

// --- legendas ---------------------------------------------------------------

static Legenda legs[LEG_MAX];
static int nLegs;
static pthread_t fioLeg;
static int fioLegVivo, fioLegCriado, legParar;
static char legId[64], legTipo[16];
static unsigned legGeracao;
// Extras do Stremio do arquivo que toca (#201). Valem so para `id`: o titulo
// seguinte nao herda o nome nem o hash do anterior.
typedef struct {
  char id[64];
  char arquivo[256];
  unsigned long long tamanho;
  char hash[20];
  char video[4096];
  int exigeCab;
} LegExtras;
static LegExtras legExt;
// A busca em curso e um REFAZER com extras: a lista anterior fica na tela ate
// esta terminar inteira (publicarLegendas so publica no fim).
static int legManter;
// Hash ja medido, para a mesma fonte nao ir de novo ao CDN a cada busca.
static char hashDeUrl[4096], hashMedido[20];
static unsigned long long hashTam;
static pthread_mutex_t legTrava = PTHREAD_MUTEX_INITIALIZER;

int addons_n_legendas(void) {
  int n;
  pthread_mutex_lock(&legTrava); n = nLegs; pthread_mutex_unlock(&legTrava);
  return n;
}
int addons_legendas_prontas(void) {
  int r;
  pthread_mutex_lock(&legTrava); r = !fioLegVivo && legId[0]; pthread_mutex_unlock(&legTrava);
  return r;
}
const Legenda *addons_legenda(int i) {
  const Legenda *r = NULL;
  pthread_mutex_lock(&legTrava);
  if (i >= 0 && i < nLegs) r = &legs[i];
  pthread_mutex_unlock(&legTrava);
  return r;
}

int addons_legendas_copiar(Legenda *dst, int max, unsigned *geracao, int *prontas) {
  int n, i;
  pthread_mutex_lock(&legTrava);
  n = nLegs < max ? nLegs : max;
  if (n < 0) n = 0;
  for (i = 0; i < n; i++) dst[i] = legs[i];
  if (geracao) *geracao = legGeracao;
  if (prontas) *prontas = !fioLegVivo && legId[0];
  pthread_mutex_unlock(&legTrava);
  return n;
}

#ifdef NV_SHOT_HOOKS
// Captures (tests/legendas_shot.c): a subtitle list without network.
void addons_shot_legendas(const Legenda *v, int n) {
  pthread_mutex_lock(&legTrava);
  if (n > LEG_MAX) n = LEG_MAX;
  if (n < 0) n = 0;
  memcpy(legs, v, (size_t)n * sizeof *v);
  nLegs = n;
  legGeracao++;
  pthread_mutex_unlock(&legTrava);
}
#endif

// Grupos de idioma da busca de legenda, NA ORDEM em que aparecem.
//
// O QUE ESTAVA AQUI: duas listas cravadas ("pob","pt-br",... e "eng","en",...)
// com o comentario "o usuario pediu explicitamente estes dois grupos". Toda
// legenda de outro idioma era descartada sem aviso — quem instala o pacote e
// fala espanhol abria o player e nao achava legenda nenhuma.
//
// AGORA: os grupos vem da preferencia (Ajustes desta TV, senao a conta) e o
// INGLES entra por ultimo quando ha preferencia. Sem preferencia (inclusive
// "Todas" nesta TV), a lista aceita qualquer idioma ate LEG_MAX. Com idiomas
// escolhidos, o ingles continua como reserva para quem nao acha o proprio.
static int gruposIdioma(const char *g[3]) {
  const char *a = ling_legenda(), *b = ling_legenda2();
  int n = 0;
  // Empty preferences include the explicit local "All" choice. One
  // unfiltered group must keep every language, rather than only English.
  if (!a[0] && !b[0]) { g[0] = ""; return 1; }
  if (a[0] && strcasecmp(a, "none")) g[n++] = a;
  if (b[0] && strcasecmp(b, "none") && !(n && ling_casa(b, a))) g[n++] = b;
  { int i, tem = 0;
    for (i = 0; i < n; i++) if (ling_casa("en", g[i])) tem = 1;
    if (!tem) g[n++] = "en"; }
  return n;
}


static int pedidoMudou(unsigned geracao) {
  int mudou;
  pthread_mutex_lock(&legTrava);
  mudou = legParar || geracao != legGeracao;
  pthread_mutex_unlock(&legTrava);
  return mudou;
}

static void episodioPedido(const char *id, int *temporada, int *episodio) {
  // idbase_episodio, e nao o corte no primeiro ':': "kitsu:41370:5" lia
  // temporada 41370 e filtrava toda fonte fora (idbase.h).
  idbase_episodio(id, temporada, episodio);
}

static int episodioCorreto(const char *obj, const char *fim, int temporada, int episodio) {
  int t, e;
  if (temporada <= 0 || episodio <= 0) return 1;
  t = (int)js_num(obj, fim, "season", -1);
  e = (int)js_num(obj, fim, "episode", -1);
  // Alguns addons antigos nao devolvem os campos. Quando devolvem, eles sao
  // uma garantia: nunca mostre T2E3 numa busca por T2E4.
  if ((t >= 0 && t != temporada) || (e >= 0 && e != episodio)) return 0;
  if (t >= 0 || e >= 0) return 1;
  // Alguns addons omitem season/episode mas devolvem o episodio no nome do
  // arquivo. Antes aceitavamos S02E03 numa busca por T2E4 e depois fabricavamos
  // o rotulo T2E4 com base no pedido, escondendo o erro. Se o nome traz uma
  // identidade verificavel, ela precisa casar; nome sem marcador segue aceito.
  { char nome[160] = "", baixo[160]; size_t i;
    if (!js_texto(obj, fim, "subtitleFileName", nome, sizeof nome))
      js_texto(obj, fim, "movieReleaseName", nome, sizeof nome);
    for (i = 0; nome[i] && i + 1 < sizeof baixo; i++)
      baixo[i] = (char)tolower((unsigned char)nome[i]);
    baixo[i] = 0;
    for (i = 0; baixo[i]; i++) {
      int nt = -1, ne = -1;
      if (sscanf(baixo + i, "s%2de%2d", &nt, &ne) == 2 ||
          sscanf(baixo + i, "%2dx%2d", &nt, &ne) == 2)
        return nt == temporada && ne == episodio;
    }
  }
  return 1;
}

// Um balde limitado por addon, antes do teto da folha. O primeiro addon pode
// responder com centenas de legendas; isso nao lhe da todos os 12 lugares
// nem impede a consulta dos seguintes (#158).
typedef struct {
  Legenda itens[LEG_MAX];
  unsigned char grupo[LEG_MAX];
  int n;
} LegLote;

static int arrayDeLegendas(const char *corpo) {
  const char *p;
  if (!corpo) return 0;
  for (p = corpo; *p; p++) {
    const char *fim, *valor;
    if (*p != '"') continue;
    for (fim = p + 1; *fim && *fim != '"'; fim++)
      if (*fim == '\\' && fim[1]) fim++;
    if (!*fim) return 0;
    valor = pulaEspaco(fim + 1);
    if (fim - p == 10 && !strncmp(p + 1, "subtitles", 9) && *valor == ':')
      return *pulaEspaco(valor + 1) == '[';
    p = fim;
  }
  return 0;
}

static int distribuirLegendas(const LegLote *lotes, int nLotes, int nGrupos,
                              Legenda *saida) {
  int gi, n = 0;
  for (gi = 0; gi < nGrupos; gi++) {
    int prox[ADD_MAX] = {0}, noGrupo = 0, avancou = 1;
    int teto = LEG_MAX / nGrupos;
    // Um resultado por provider por volta, dentro de cada idioma. Mantem a
    // ordem principal -> secundario -> ingles e preenche lugares vagos com
    // quem ainda tem candidatos quando algum addon respondeu vazio/falhou.
    while (noGrupo < teto && avancou) {
      int i;
      avancou = 0;
      for (i = 0; i < nLotes && noGrupo < teto; i++) {
        while (prox[i] < lotes[i].n && lotes[i].grupo[prox[i]] != gi) prox[i]++;
        if (prox[i] >= lotes[i].n) continue;
        saida[n++] = lotes[i].itens[prox[i]++];
        noGrupo++; avancou = 1;
      }
    }
  }
  return n;
}

// BUSCA DE LEGENDAS EM PARALELO (legenda automatica, 04/10).
//
// Era SERIAL: um addon por vez, 25 s de teto cada, e a lista so aparecia para o
// resto do app quando o ULTIMO terminava. Um addon lento segurava a legenda
// automatica (addons_legendas_prontas) e a folha por ate 75 s, e no log da TV
// os tres addons respondiam um depois do outro (0,5 + 0,4 + 0,6 s).
// Agora cada addon tem o seu fio, o teto e de LEG_TETO_S, e a lista e PUBLICADA
// a cada resposta (a folha ganha linhas ao vivo); "prontas" so quando todos
// voltaram.
//
// 30 s e nao 8 (#202, "o add-on Subtitlesync.stream nao esta carregando"). O
// Nuvio web da 20 s por addon de legenda (subtitleRepository,
// PER_ADDON_TIMEOUT_MS) e o Stremio espera mais. O Subtitle Sync alinha a
// legenda ao video ANTES de responder e documenta "usually a few seconds, at
// most about 25" na primeira vez de cada video (faq.wait em
// subtitlesync.stream/web/i18n.mjs): com 8 s ele era cortado sempre que o
// video era novo. Como a lista sai a cada resposta, o prazo longo so pesa no
// "prontas", e a legenda automatica decide sozinha em FX_AUTO_FIM_MS (faixas.c).
#define LEG_TETO_S 30

typedef struct {
  LegLote *lotes;          // um por addon
  int nLotes, nGrupos;
  int manter;              // refazer com extras: publica so no fim
  char extras[1200];       // segmento legextras_segmento; "" = sem extras
  const char *grupos[3];
  char gruposTexto[3][16];
  char id[64], tipo[16];
  int temporada, episodio;
  unsigned geracao;
  pthread_mutex_t m;       // guarda lotes
} LegBusca;

typedef struct { LegBusca *B; int i; } LegFio;

static void publicarLegendas(LegBusca *B, int final) {
  Legenda *achadas;
  int n;
  if (B->manter && !final) return;
  achadas = calloc(LEG_MAX, sizeof *achadas);
  if (!achadas) return;
  pthread_mutex_lock(&B->m);
  n = distribuirLegendas(B->lotes, B->nLotes, B->nGrupos, achadas);
  pthread_mutex_unlock(&B->m);
  pthread_mutex_lock(&legTrava);
  if (!legParar && B->geracao == legGeracao) {
    memcpy(legs, achadas, sizeof legs);
    nLegs = n;
  }
  pthread_mutex_unlock(&legTrava);
  free(achadas);
}

static void *buscarUmAddon(void *u) {
  LegFio *F = u;
  LegBusca *B = F->B;
  int i = F->i, array = 0, recebidas = 0, gi, comExtras = 0, recuou = 0, cortadas = 0;
  char url[NV_ADDON_PEDIDO_MAX], *corpo = NULL;
  const char *p, *q;
  RedeMedida medida = {0};
  LegLote *lote = calloc(1, sizeof *lote);
  if (!lote) return NULL;
  if (pedidoMudou(B->geracao) || !addon[i].ativo) { free(lote); return NULL; }
  // Com extras (#201) quando ha; o formato antigo continua sendo o pedido de
  // quem nao sabe o arquivo. O protocolo diz que extra e opcional, mas um addon
  // que responder 4xx/5xx ao caminho com extras ganha o pedido antigo em
  // seguida: legenda generica e melhor que nenhuma.
  if (B->extras[0] && legextras_url(url, sizeof url, addon[i].base, B->tipo, B->id, B->extras)) {
    comExtras = 1;
    corpo = rede_baixar_medido_controle(url, LEG_TETO_S, NULL, NULL, &medida);
    if (pedidoMudou(B->geracao)) { free(corpo); free(lote); return NULL; }
    if (medida.status >= 400) { free(corpo); corpo = NULL; recuou = 1; }
  }
  if (!comExtras || recuou) {
    memset(&medida, 0, sizeof medida);
    corpo = pedidoCoube(i, legextras_url(url, sizeof url, addon[i].base, B->tipo, B->id, NULL)
                             ? (int)strlen(url) : (int)sizeof url, sizeof url)
            ? rede_baixar_medido_controle(url, LEG_TETO_S, NULL, NULL, &medida) : NULL;
  }
  if (pedidoMudou(B->geracao)) { free(corpo); free(lote); return NULL; }
  p = js_array(corpo, NULL, "subtitles");
  // js_array devolve NULL tambem para []: o diagnostico precisa distinguir
  // um array vazio de uma resposta sem o resource esperado.
  array = arrayDeLegendas(corpo);
  for (q = p; q;) {
    const char *f = js_fim(q);
    if (!f || f <= q) break;
    recebidas++;
    q = js_prox(f);
  }
  for (gi = 0; gi < B->nGrupos; gi++) {
    int noGrupo = 0, teto = LEG_MAX / B->nGrupos;
    for (q = p; q && noGrupo < teto;) {
      const char *f = js_fim(q);
      char l[64] = "", cod[16], nome[120] = "", link[2048];
      Legenda *d = &lote->itens[lote->n];
      if (!f || f <= q) break;
      if (*q == '{' && episodioCorreto(q, f, B->temporada, B->episodio) &&
          js_texto(q, f, "lang", l, sizeof l)) {
        // "PORTUGUESE" / "Portuguese (Brazil)" / "por" / "pt-BR" viram um
        // codigo so; antes o nome entrava cortado em 8 bytes ("PORTUGU") e
        // nao casava com a preferencia.
        ling_normalizar(l, cod, sizeof cod);
        // Link lido num buffer MAIOR que o campo: o que nao cabe em d->url e
        // descartado e contado, em vez de virar um link cortado que so falha
        // na hora de baixar.
        if (ling_casa(cod, B->grupos[gi]) && js_texto(q, f, "url", link, sizeof link) &&
            !(strlen(link) >= sizeof d->url && ++cortadas)) {
          snprintf(d->url, sizeof d->url, "%s", link);
          js_texto(q, f, "subtitleFileName", nome, sizeof nome);
          if (!nome[0]) js_texto(q, f, "movieReleaseName", nome, sizeof nome);
          snprintf(d->idioma, sizeof d->idioma, "%s", cod);
          snprintf(d->provedor, sizeof d->provedor, "%s", addon[i].nome);
          snprintf(d->arquivo, sizeof d->arquivo, "%s", nome);
          if (B->temporada > 0 && B->episodio > 0)
            snprintf(d->rotulo, sizeof d->rotulo, i18n("T%dE%d  \xc2\xb7  %s%s%.22s"),
                     B->temporada, B->episodio, i18n(ling_nome(cod)), nome[0] ? "  \xc2\xb7  " : "", nome);
          else
            snprintf(d->rotulo, sizeof d->rotulo, "%s%s%.36s",
                     i18n(ling_nome(cod)), nome[0] ? "  \xc2\xb7  " : "", nome);
          lote->grupo[lote->n++] = (unsigned char)gi;
          noGrupo++;
        }
      }
      q = js_prox(f);
    }
  }
  // Somente medidas e enumeracoes publicas. Nao registrar URL, id do
  // titulo, corpo, nome de arquivo, nome configurado ou cabecalhos.
  printf("[addon-recurso] addon=%d resource=subtitles tipo=%s http=%d bytes=%ld ms=%lu array=%d recebidas=%d candidatas=%d extras=%d%s cortadas=%d\n",
         i + 1, !strcmp(B->tipo, "movie") ? "movie" : !strcmp(B->tipo, "series") ? "series" : "outro",
         medida.status, medida.bytes, medida.ms, array, recebidas, lote->n,
         comExtras && !recuou, recuou ? " (recusou extras: pedido antigo)" : "", cortadas);
  // A MESMA LINHA DAS FONTES, com o nome (#202). "addon=3" nao dizia qual era o
  // Subtitle Sync num log de usuario, e "[legendas] concluida: 3" nao separava
  // "respondeu vazio" de "estourou o prazo". O nome do addon ja vai no log das
  // fontes ("[addons] X: N fontes"); URL, id e corpo continuam fora (o corpo de
  // erro pode repetir a configuracao, ver tests/addons_legendas.sh). O texto da
  // libcurl so diz o tipo da falha e o host.
  { const char *erro = medida.status ? "" : rede_ultimo_erro();
    printf("[legendas] %s: %d legendas, %d no idioma (HTTP %d, %ld bytes, %lu ms)%s%s%s\n",
           addon[i].nome, recebidas, lote->n, medida.status, medida.bytes, medida.ms,
           comExtras && !recuou ? ", com extras" : "",
           erro && erro[0] ? ": " : "", erro ? erro : ""); }
  fflush(stdout);
  free(corpo);
  pthread_mutex_lock(&B->m);
  B->lotes[i] = *lote;
  pthread_mutex_unlock(&B->m);
  free(lote);
  publicarLegendas(B, 0);
  return NULL;
}

static unsigned relogioMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned)(t.tv_sec * 1000u + t.tv_nsec / 1000000u);
}

// HASH DO OPENSUBTITLES PELA REDE (#201): dois Range de 64 KiB, comeco e fim.
// So 206 com o trecho inteiro vale — servidor que ignora o Range devolve o
// comeco do arquivo com 200, e somar isso como "o fim" daria um hash errado,
// pior que nenhum (o addon casaria a legenda de OUTRO arquivo).
static int medirHash(const char *url, unsigned long long tam, char out[17],
                     const char **motivo) {
  long n1 = 0, n2 = 0;
  int st1 = 0, st2 = 0, e = 0, ok = 0;
  char *a, *b = NULL;
  a = rede_baixar_trecho_st(url, 6, 0, LEGEXTRAS_BLOCO - 1, &n1, &st1, &e, NULL, 0);
  if (a && st1 == 206 && n1 == LEGEXTRAS_BLOCO)
    b = rede_baixar_trecho_st(url, 6, (long)(tam - LEGEXTRAS_BLOCO), (long)(tam - 1),
                              &n2, &st2, &e, NULL, 0);
  if (b && st2 == 206 && n2 == LEGEXTRAS_BLOCO)
    ok = legextras_hash((const unsigned char *)a, (size_t)n1, (const unsigned char *)b,
                        (size_t)n2, tam, out);
  *motivo = ok ? "measured" : !a ? "network failure" : st1 != 206 || (b && st2 != 206) ? "no Range support" : "short range";
  free(a); free(b);
  return ok;
}

// Prepara o segmento de extras de uma busca, com o hash quando da para
// medir. Roda no fio da busca, antes dos addons: o hash custa dois pedidos
// curtos (prazo de 6 s cada) e o resultado vale para todos eles.
static void prepararExtras(LegBusca *B) {
  LegExtras e;
  char hash[20] = "", nome[256];
  const char *motivo = "no source";
  pthread_mutex_lock(&legTrava);
  e = legExt;
  pthread_mutex_unlock(&legTrava);
  B->extras[0] = 0;
  if (!e.id[0] || strcmp(e.id, B->id)) return;
  snprintf(nome, sizeof nome, "%s", e.arquivo);
  if (!nome[0]) legextras_nome_da_url(e.video, nome, sizeof nome);
  if (e.hash[0]) { snprintf(hash, sizeof hash, "%s", e.hash); motivo = "from the addon"; }
  else if (!e.video[0] || !legextras_url_remota(e.video)) motivo = "local or P2P";
  else if (e.exigeCab) motivo = "source needs headers";
  else if (e.tamanho < 2ull * LEGEXTRAS_BLOCO) motivo = "no exact size";
  else {
    int cache;
    pthread_mutex_lock(&legTrava);
    cache = hashMedido[0] && hashTam == e.tamanho && !strcmp(hashDeUrl, e.video);
    if (cache) snprintf(hash, sizeof hash, "%s", hashMedido);
    pthread_mutex_unlock(&legTrava);
    if (cache) motivo = "measured (cached)";
    else if (medirHash(e.video, e.tamanho, hash, &motivo)) {
      pthread_mutex_lock(&legTrava);
      snprintf(hashDeUrl, sizeof hashDeUrl, "%s", e.video);
      snprintf(hashMedido, sizeof hashMedido, "%s", hash);
      hashTam = e.tamanho;
      pthread_mutex_unlock(&legTrava);
    }
  }
  if (legextras_segmento(B->extras, sizeof B->extras, nome, e.tamanho, hash) < 0)
    legextras_segmento(B->extras, sizeof B->extras, NULL, e.tamanho, hash);   // nome enorme: sem ele
  // Sem valores no log: nome de arquivo e hash identificam o que a pessoa ve.
  printf("[legendas] extras: nome=%d tamanho=%d hash=%d (%s)\n", nome[0] != 0,
         e.tamanho != 0, hash[0] != 0, motivo);
  fflush(stdout);
}

static void *buscarLegendas(void *u) {
  (void)u;
  for (;;) {
    LegBusca B;
    LegFio fio[ADD_MAX];
    pthread_t th[ADD_MAX];
    int criado[ADD_MAX] = {0};
    unsigned t0 = relogioMs();
    int i, nAtivos = 0;

    memset(&B, 0, sizeof B);
    pthread_mutex_init(&B.m, NULL);
    pthread_mutex_lock(&legTrava);
    if (legParar) { fioLegVivo = 0; pthread_mutex_unlock(&legTrava); pthread_mutex_destroy(&B.m); return NULL; }
    snprintf(B.id, sizeof B.id, "%s", legId);
    snprintf(B.tipo, sizeof B.tipo, "%s", legTipo);
    B.geracao = legGeracao;
    B.manter = legManter;
    pthread_mutex_unlock(&legTrava);
    prepararExtras(&B);
    episodioPedido(B.id, &B.temporada, &B.episodio);
    B.nGrupos = gruposIdioma(B.grupos);
    for (i = 0; i < B.nGrupos; i++) {
      snprintf(B.gruposTexto[i], sizeof B.gruposTexto[i], "%s", B.grupos[i]);
      B.grupos[i] = B.gruposTexto[i];
    }
    B.nLotes = nAddon < ADD_MAX ? nAddon : ADD_MAX;
    B.lotes = calloc((size_t)(B.nLotes > 0 ? B.nLotes : 1), sizeof *B.lotes);
    if (!B.lotes) printf("[legendas] candidatos: memoria indisponivel\n");

    for (i = 0; B.lotes && i < B.nLotes; i++) {
      // Addon que nao declara legenda nao e consultado.
      if (!addon[i].ativo || !addon[i].legenda) continue;
      fio[i].B = &B; fio[i].i = i;
      if (pthread_create(&th[i], NULL, buscarUmAddon, &fio[i]) == 0) { criado[i] = 1; nAtivos++; }
      else buscarUmAddon(&fio[i]);      // sem fio novo: faz aqui mesmo
    }
    for (i = 0; i < B.nLotes; i++) if (criado[i]) pthread_join(th[i], NULL);

    // Refazer com extras: a lista nova entra de uma vez, agora que todos
    // responderam (publicarLegendas descarta se a geracao ja mudou).
    if (B.manter && B.lotes) publicarLegendas(&B, 1);
    free(B.lotes);
    pthread_mutex_destroy(&B.m);
    pthread_mutex_lock(&legTrava);
    if (legParar) { fioLegVivo = 0; pthread_mutex_unlock(&legTrava); return NULL; }
    if (B.geracao != legGeracao) { pthread_mutex_unlock(&legTrava); continue; }
    fioLegVivo = 0;
    legManter = 0;
    i = nLegs;
    pthread_mutex_unlock(&legTrava);
    printf("[legendas] concluida: %d (%d addon(s) em paralelo, %u ms)\n", i, nAtivos,
           relogioMs() - t0);
    fflush(stdout);
    return NULL;
  }
}


// ------------------------------------------------------------ lista e sonda

int addons_ativo(int i)   { return (i >= 0 && i < nAddon) ? addon[i].ativo : 0; }

// O PREDICADO UNICO DE "ESTE ADDON FOI DESLIGADO NA CONTA". Todo pedido a um
// addon por BASE (catalogo, busca, "ver tudo", guia, meta) passa por aqui. O
// log do dono (D1 58666) mostrou "Minha TV: desligado na conta" e, depois de um
// OK, a TV pedindo e esperando 20 s o minhatv: desligar so tirava a fileira.
// Base que nenhum addon da lista reclama (portal Stalker, Cinemeta, addon
// removido) NAO e desligada.
int addons_base_desligada(const char *base) {
  char alvo[NV_ADDON_URL_MAX], mine[NV_ADDON_URL_MAX];
  int i;
  if (!base || !*base) return 0;
  baseNormalizada(base, alvo, sizeof alvo);
  for (i = 0; i < nAddon; i++) {
    baseNormalizada(addon[i].base, mine, sizeof mine);
    if (!strcmp(mine, alvo)) return !addon[i].ativo;
  }
  return 0;
}
int addons_sondado(int i) { return (i >= 0 && i < nAddon) ? addon[i].sondado : 0; }
int addons_catalogos_canal(int i, AddCatCanal *saida, int max) {
  int k;
  if (i < 0 || i >= nAddon || !addon[i].canalLido) return -1;
  for (k = 0; k < addon[i].nCanal && k < max; k++) saida[k] = addon[i].canal[k];
  return k;
}
const char *addons_nome(int i) {
  return (i >= 0 && i < nAddon) ? addon[i].nome : "";
}
int addons_fornece(int i, int oque) {
  if (i < 0 || i >= nAddon) return 0;
  if (oque == ADD_CATALOGO) return addon[i].catalogo;
  if (oque == ADD_STREAM)   return addon[i].fonte;
  if (oque == ADD_META)     return addon[i].meta;
  return addon[i].legenda;
}
int addons_alternar(int i) {
  if (i < 0 || i >= nAddon) return 0;
  addon[i].ativo = !addon[i].ativo;
  listaMudou();
  printf("[addons] %s: %s\n", addon[i].nome, addon[i].ativo ? "ligado" : "desligado");
  fflush(stdout);
  return addon[i].ativo;
}

// ACRESCENTA UM ADDON, sem passar por addons_definir_lista.
//
// POR QUE NAO REUSAR addons_definir_lista: ela REFAZ a lista inteira, e com
// isso zera `sondado` e o `id` de manifesto de TODOS os addons — inclusive dos
// que ja estavam sondados e nada tinham a ver com a instalacao. O id do
// manifesto e a chave que as colecoes da conta usam (addons_base_por_id), entao
// perde-lo faz colecao abrir vazia ate a proxima sonda. Ela tambem registra
// "[addons] N vindos da conta", que seria mentira para uma lista que veio do
// guia.
//
// Capacidades nascem SUPOSTAS como no carregador do arquivo (fonte e catalogo
// sim, legenda nao) e `sondado` em 0: a sonda do manifesto corrige depois, e
// ate la o custo de supor e uma consulta vazia.
//
// Devolve 1 se entrou, 0 se a lista esta cheia ou o addon ja existe. Comparacao
// por base NORMALIZADA, e nao pela URL crua: "<base>", "<base>/" e
// "<base>/manifest.json" sao o mesmo addon.
int addons_adicionar(const char *nome, const char *urlManifest) {
  char nova[NV_ADDON_URL_MAX];
  int i;
  if (!urlManifest || !*urlManifest) return 0;
  // Antes de normalizar: o que nao cabe nao entra, nem cortado (addonurl.h).
  // #203: URL grande vira apelido (addonurl.h) em vez de ser recusada.
  { static char ap[NV_ADDON_URL_MAX];   // so o fio principal instala
    if (!nv_addon_url_guardar(nome, urlManifest, ap, sizeof ap)) return 0;
    urlManifest = ap; }
  if (nAddon >= ADD_MAX) {
    printf("[addons] nao coube: a lista ja tem %d\n", ADD_MAX);
    fflush(stdout);
    return 0;
  }
  baseNormalizada(urlManifest, nova, sizeof nova);
  if (!nova[0]) return 0;
  for (i = 0; i < nAddon; i++) {
    char base[NV_ADDON_URL_MAX];
    baseNormalizada(addon[i].base, base, sizeof base);
    if (!strcmp(base, nova)) {
      printf("[addons] ja instalado: %s\n", addon[i].nome);
      fflush(stdout);
      return 0;
    }
  }
  memset(&addon[nAddon], 0, sizeof addon[nAddon]);
  snprintf(addon[nAddon].nome, sizeof addon[nAddon].nome, "%s",
           nome && *nome ? nome : nova);
  snprintf(addon[nAddon].base, sizeof addon[nAddon].base, "%s", nova);
  addon[nAddon].fonte = 1;
  addon[nAddon].catalogo = 1;
  addon[nAddon].legenda = 0;
  addon[nAddon].ativo = 1;
  addon[nAddon].sondado = 0;
  addon[nAddon].canalLido = 0; addon[nAddon].nCanal = 0; addon[nAddon].mudoSeg = 0;
  nAddon++;
  listaMudou();
  // So o host: a base inteira ia para o log aqui, com a chave do addon no
  // caminho (ver rede_url_publica em rede.h).
  { char seg[120];
    printf("[addons] instalado pelo guia: %s (%s)\n",
           addon[nAddon - 1].nome, rede_url_publica(nova, seg, sizeof seg)); }
  fflush(stdout);
  return 1;
}

// SONDA DO MANIFESTO. Ate ela responder, o app assume que todo addon fornece
// tudo — e essa suposicao custa no maximo uma consulta vazia. O manifesto diz a
// verdade, e e o que a tela mostra: sem isso a lista so poderia repetir a
// suposicao, que e o mesmo que nao informar nada.
//
// Roda em fio proprio e UMA vez por lista: sao N viagens, e faze-las no
// arranque atrasaria a primeira tela por addon configurado.
static pthread_t fioSonda;
static int sondaViva;

// --- O QUE O RESOURCE "meta" DECLARA (idPrefixes e types) -------------------
//
// Um manifesto Stremio pode dizer PARA QUE IDS cada resource serve, de dois
// jeitos: na raiz ("idPrefixes":["tt"]) ou dentro do resource, como objeto
// ({"name":"meta","types":["anime"],"idPrefixes":["kitsu:"]}). O de dentro
// vence o da raiz. Sem isto o app so sabia que o addon TEM "meta", nunca de
// quais ids — e para decidir a quem perguntar a ficha de "kitsu:41370" e o
// prefixo que responde.

// Valor (posicao do '[' , '{' ou do texto) de `chave` na PROFUNDIDADE 1 de
// [ini,fim): as chaves de dentro de catalogs[]/resources[] nao contam. NULL sem
// ela. `fim` NULL = ate o fim da string.
static const char *chaveNaRaiz(const char *ini, const char *fim, const char *chave) {
  size_t kl = strlen(chave);
  int prof = 0;
  const char *p;
  if (!ini) return NULL;
  if (!fim) fim = ini + strlen(ini);
  for (p = ini; p < fim; p++) {
    if (*p == '"') {
      const char *a = p + 1, *q = a;
      while (q < fim && *q != '"') { if (*q == '\\' && q + 1 < fim) q++; q++; }
      if (prof == 1 && (size_t)(q - a) == kl && !strncmp(a, chave, kl)) {
        const char *v = q + 1;
        while (v < fim && (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r')) v++;
        if (v < fim && *v == ':') {          // era chave, nao valor de outra
          v++;
          while (v < fim && (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r')) v++;
          return v < fim ? v : NULL;
        }
      }
      p = q;
    } else if (*p == '{' || *p == '[') prof++;
    else if (*p == '}' || *p == ']') prof--;
  }
  return NULL;
}

// Strings de um array JSON (v aponta para o '['). Devolve quantas copiou.
static int lerStringsDoArray(const char *v, char (*out)[ADD_PREF_TAM], int max) {
  const char *fim, *p;
  int n = 0;
  if (!v || *v != '[') return 0;
  fim = js_fim(v);
  if (!fim) return 0;
  for (p = v + 1; p < fim && n < max; p++) {
    if (*p == '"') {
      size_t k = 0;
      for (p++; p < fim && *p != '"'; p++) {
        if (*p == '\\' && p + 1 < fim) p++;
        if (k + 1 < ADD_PREF_TAM) out[n][k++] = *p;
      }
      out[n][k] = 0;
      if (k) n++;
    }
  }
  return n;
}

static void lerDeclaracaoMeta(int i, const char *corpo, const char *resources) {
  char lista[ADD_PREF_MAX][ADD_PREF_TAM];
  char tiposTopo[ADD_PREF_MAX][ADD_PREF_TAM];
  char prefTopo[ADD_PREF_MAX][ADD_PREF_TAM];
  int nT, nPT, n, k;
  addon[i].nMetaPref = 0;
  addon[i].metaTipos[0] = 0;
  nT  = lerStringsDoArray(chaveNaRaiz(corpo, NULL, "types"), tiposTopo, ADD_PREF_MAX);
  nPT = lerStringsDoArray(chaveNaRaiz(corpo, NULL, "idPrefixes"), prefTopo, ADD_PREF_MAX);
  // Comeca pelo da raiz; o resource "meta", se declarar, troca.
  for (k = 0; k < nPT; k++)
    snprintf(addon[i].metaPref[k], ADD_PREF_TAM, "%s", prefTopo[k]);
  addon[i].nMetaPref = nPT;
  n = nT;
  for (k = 0; k < nT; k++) snprintf(lista[k], ADD_PREF_TAM, "%s", tiposTopo[k]);
  if (resources && *resources == '[') {
    const char *fimR = js_fim(resources), *e;
    e = resources + 1;
    while (fimR && e < fimR) {
      while (e < fimR && (*e == ' ' || *e == ',' || *e == '\n' || *e == '\t' || *e == '\r')) e++;
      if (e >= fimR) break;
      if (*e == '{') {
        const char *fe = js_fim(e);
        char nome[24] = "";
        if (!fe) break;
        js_texto_raiz_em(e, fe, "name", nome, sizeof nome);
        if (!strcasecmp(nome, "meta")) {
          const char *tp = chaveNaRaiz(e, fe, "types");
          const char *pf = chaveNaRaiz(e, fe, "idPrefixes");
          if (tp) n = lerStringsDoArray(tp, lista, ADD_PREF_MAX);
          if (pf) {
            char pr[ADD_PREF_MAX][ADD_PREF_TAM];
            int np = lerStringsDoArray(pf, pr, ADD_PREF_MAX);
            for (k = 0; k < np; k++) snprintf(addon[i].metaPref[k], ADD_PREF_TAM, "%s", pr[k]);
            addon[i].nMetaPref = np;
          }
          break;
        }
        e = fe + 1;
      } else if (*e == '"') {       // forma curta: "meta" herda os da raiz
        for (e++; e < fimR && *e != '"'; e++) if (*e == '\\') e++;
        e++;
      } else e++;
    }
  }
  if (n > 0) {
    size_t o = 0;
    addon[i].metaTipos[o++] = '|';
    for (k = 0; k < n; k++) {
      const char *t = lista[k];
      for (; *t && o + 2 < sizeof addon[i].metaTipos; t++)
        addon[i].metaTipos[o++] = (char)tolower((unsigned char)*t);
      addon[i].metaTipos[o++] = '|';
    }
    addon[i].metaTipos[o] = 0;
  }
}

// O addon `i` aceita pedir /meta/<tipo>/<id> ?
//   1  = declara meta para esse tipo E esse prefixo de id;
//   0  = nao serve: nao tem "meta", ou declarou tipos/prefixos e este nao esta;
//  -1  = nao da para saber (manifesto ainda nao lido, ou sem idPrefixes) —
//        quem chama decide se vale tentar UMA vez.
// O tipo "" ou NULL nao filtra por tipo.
int addons_aceita_id(int i, const char *tipo, const char *id) {
  int k;
  if (i < 0 || i >= nAddon || !id || !id[0]) return 0;
  if (!addon[i].sondado) return -1;
  if (!addon[i].meta) return 0;
  if (tipo && tipo[0] && addon[i].metaTipos[0]) {
    char marca[40];
    snprintf(marca, sizeof marca, "|%s|", tipo);
    for (k = 0; marca[k]; k++) marca[k] = (char)tolower((unsigned char)marca[k]);
    if (!strstr(addon[i].metaTipos, marca)) return 0;
  }
  if (addon[i].nMetaPref <= 0) return -1;
  for (k = 0; k < addon[i].nMetaPref; k++) {
    size_t l = strlen(addon[i].metaPref[k]);
    if (l && !strncmp(id, addon[i].metaPref[k], l)) return 1;
  }
  return 0;
}

static void capacidadesDoManifesto(int i, const char *corpo) {
  const char *r = strstr(corpo, "\"resources\"");
  int cat = 0, str = 0, leg = 0, met = 0;
  // O ID E O NOME VEM PRIMEIRO, ANTES DE QUALQUER RETORNO CEDO.
  //
  // Estavam no fim da funcao, depois de tres `return` que dependem de
  // "resources" — um campo que o protocolo pede mas que addon real as vezes
  // omite ou escreve de forma que este leitor nao alcanca. Nesse caso o addon
  // ficava PARA SEMPRE sem id, addons_base_por_id devolvia "" e as pastas de
  // colecao da conta abriam sem nenhuma fileira (issue #10). As capacidades
  // seguem sendo suposicao otimista quando o campo nao da para ler, que e o
  // que `sondado` distingue.
  // O "id" DA RAIZ, e nao o primeiro "id" do documento: um manifesto Stremio
  // tem "id" tambem dentro de catalogs[] e de behaviorHints. Ver js_texto_raiz
  // em js.h, que e onde este leitor mora agora — o TMDB precisou do mesmo.
  // O HOST, redigido, ANTES do resto. Sem ele nao da para dizer de onde um
  // addon fala, e "de onde?" e a primeira pergunta quando um addon funciona
  // num aparelho e nao no outro. O caminho fica de fora porque nele viaja
  // credencial — o Xperience embute um JWT ali; ver rede_url_publica em rede.h.
  { char seg[120];
    printf("[addons] %s: de %s\n", addon[i].nome,
           rede_url_publica(addons_base(i), seg, sizeof seg)); }
  if (js_texto_raiz(corpo, "id", addon[i].id, sizeof addon[i].id))
    printf("[addons] %s: id do manifesto = %s\n", addon[i].nome, addon[i].id);
  // js_texto_raiz, e nao js_texto: exatamente a mesma armadilha que o
  // comentario acima descreve para o "id", repetida na instrucao seguinte. Um
  // manifesto Stremio tem "name" tambem dentro de cada catalogs[], e o leitor
  // solto pega o PRIMEIRO do documento. O Bingecat declara um catalogo chamado
  // "search" e o addon inteiro passava a se chamar "search" — no log, na folha
  // de fileiras dos Ajustes e em qualquer lugar que mostre de onde a fileira
  // veio.
  { char nome[64];
    if (js_texto_raiz(corpo, "name", nome, sizeof nome) && nome[0])
      snprintf(addon[i].nome, sizeof addon[i].nome, "%s", nome); }
  // CATALOGOS DE CANAL, antes do retorno cedo de "resources": um manifesto
  // sem resources legivel ainda declara catalogs[], e o guia precisa deles.
  { const char *p = js_array(corpo, NULL, "catalogs");
    addon[i].nCanal = 0;
    while (p && addon[i].nCanal < ADD_CANAL_MAX) {
      const char *f = js_fim(p);
      AddCatCanal c;
      memset(&c, 0, sizeof c);
      js_texto(p, f, "type", c.tipo, sizeof c.tipo);
      js_texto(p, f, "id",   c.id,   sizeof c.id);
      js_texto_raiz_em(p, f, "name", c.nome, sizeof c.nome);
      if (c.id[0] && (!strcasecmp(c.tipo, "channel") || !strcasecmp(c.tipo, "tv") ||
                      !strcasecmp(c.tipo, "channels") || !strcasecmp(c.tipo, "live") ||
                      !strcasecmp(c.tipo, "iptv")))
        addon[i].canal[addon[i].nCanal++] = c;
      p = js_prox(f);
    }
    addon[i].canalLido = 1; }
  if (!r) {
    printf("[addons] %s: manifesto sem \"resources\" legivel; capacidades ficam supostas\n",
           addon[i].nome);
    fflush(stdout);
    return;
  }
  // Pular a CHAVE e ir ao valor. MEDIDO na TV: js_fim sobre a aspa de
  // "resources" devolve o proprio ponteiro, o trecho ficava vazio e TODO addon
  // virava catalogo=0 stream=0 legenda=0 — a primeira busca de fontes (antes
  // da sonda) achava 38, e a partir da segunda nenhum addon era consultado.
  r = strchr(r + 11, ':');
  if (!r) return;
  r++;
  while (*r == ' ' || *r == '\n' || *r == '\t') r++;
  // O campo aceita duas formas no protocolo Stremio: lista de strings
  // ("catalog") e lista de objetos ({"name":"stream",...}). Procurar o NOME
  // solto cobre as duas sem escrever dois analisadores.
  //
  // SEM CAIXA (#83): alguns manifestos usam "Stream"/"Catalog". strstr
  // case-sensitive marcava stream=0 e o addon sumia da folha de fontes.
  { const char *fim = js_fim(r);
    if (!fim) fim = corpo + strlen(corpo);
    { size_t n = (size_t)(fim - r);
      char *trecho = malloc(n + 1);
      size_t k;
      if (!trecho) return;
      memcpy(trecho, r, n); trecho[n] = 0;
      for (k = 0; k < n; k++)
        trecho[k] = (char)tolower((unsigned char)trecho[k]);
      cat = strstr(trecho, "catalog")   != NULL;
      str = strstr(trecho, "stream")    != NULL;
      leg = strstr(trecho, "subtitles") != NULL;
      met = strstr(trecho, "\"meta\"") != NULL;
      free(trecho); } }
  lerDeclaracaoMeta(i, corpo, r);
  addon[i].catalogo = cat;
  // Atomico: a descoberta pode republicar o manifesto (addons_manifesto_lido)
  // enquanto um fio da busca le fonte em semStreamSondado.
  __atomic_store_n(&addon[i].fonte, str, __ATOMIC_RELAXED);
  addon[i].legenda  = leg;
  addon[i].meta     = met;
  // Publica DEPOIS das capacidades: quem le sondado com acquire (semStreamSondado,
  // nos fios da busca) ve o fonte certo.
  __atomic_store_n(&addon[i].sondado, 1, __ATOMIC_RELEASE);
  printf("[addons] %s: catalogo=%d stream=%d legenda=%d meta=%d\n",
         addon[i].nome, cat, str, leg, met);
  fflush(stdout);
}

// UNIFICACAO COM O CACHE DE MANIFESTO DA DESCOBERTA (descoberta.c: maniCache).
//
// Antes esta sonda baixava manifest.json por conta propria, sem saber que a
// descoberta (desc_iniciar/desc_repetir) muitas vezes ja tinha acabado de
// baixar o MESMO manifesto na mesma versao de lista — dois GET identicos, as
// vezes na mesma rodada de arranque. `desc_manifesto_cache_obter` primeiro
// evita o download quando a descoberta ja fez o trabalho; `_guardar` no fim
// deixa o corpo disponivel para a descoberta reaproveitar, se ela pedir depois
// (mesma url + mesma addons_versao()).
//
// FIO: continua sendo o fio proprio da sonda (fioSonda). A cache em si e
// protegida por uma trava dentro de descoberta.c (maniTrava) — as duas
// funcoes publicas tomam e soltam essa trava sozinhas, entao chamar daqui, de
// um fio diferente do da descoberta, e seguro por construcao. Nenhuma rede
// nasce dentro da trava: so memcpy de um buffer pequeno.
static void *sondar(void *u) {
  int i;
  unsigned versao = addons_versao();
  (void)u;
  for (i = 0; i < nAddon; i++) {
    char url[NV_ADDON_PEDIDO_MAX], *corpo;
    if (addon[i].sondado) continue;
    if (!pedidoCoube(i, nv_addon_url(url, sizeof url, addon[i].base, "/manifest.json"),
                     sizeof url)) continue;
    corpo = desc_manifesto_cache_obter(url, versao);
    if (corpo) {
      printf("[addons] %s: manifesto do cache da descoberta (sem rede)\n", addon[i].nome);
    } else {
      corpo = rede_baixar(url, 12);
      if (!corpo) {
        // Sem resposta NAO vira "nao fornece nada": ficaria um addon bom apagado
        // da lista por uma falha de rede. Fica como estava, por sondar.
        printf("[addons] manifesto sem resposta: %s\n", addon[i].nome);
        continue;
      }
      // Deixa a copia para a descoberta, se ela pedir depois. Nao toma posse
      // de `corpo`: a sonda continua dona dele e libera embaixo, como sempre.
      desc_manifesto_cache_guardar(url, versao, corpo);
    }
    capacidadesDoManifesto(i, corpo);
    free(corpo);
  }
  sondaViva = 0;
  return NULL;
}

void addons_manifesto_lido(int i, const char *corpo) {
  if (i < 0 || i >= nAddon || !corpo || !*corpo) return;
  capacidadesDoManifesto(i, corpo);
}

void addons_sondar_manifestos(void) {
  if (sondaViva || nAddon <= 0) return;
  sondaViva = 1;
  if (pthread_create(&fioSonda, NULL, sondar, NULL) != 0) sondaViva = 0;
  else pthread_detach(fioSonda);
}

void addons_legendas_reiniciar(void) {
  char id[64], tp[16];
  pthread_mutex_lock(&legTrava);
  snprintf(id, sizeof id, "%s", legId);
  snprintf(tp, sizeof tp, "%s", legTipo);
  // Zerar o alvo e o que desarma a guarda de "mesmo pedido" logo abaixo; a
  // geracao nova faz o fio vivo, se houver, descartar o que ja tinha juntado.
  legId[0] = 0; legTipo[0] = 0;
  nLegs = 0;
  legGeracao++;
  pthread_mutex_unlock(&legTrava);
  if (id[0]) addons_buscar_legendas(id, tp[0] ? tp : "movie");
}

static void dispararLegendas(int juntar);

void addons_buscar_legendas(const char *imdb, const char *tipo) {
  int serie, juntar = 0;
  char id[64], tp[16];
  if (!nAddon || !imdb || !*imdb || jfid_e(imdb)) return;   // never send server ids to addons
  serie = tipo && !strcmp(tipo, "series");
  if (serie && !idbase_tem_episodio(imdb))
    snprintf(id, sizeof id, idbase_e_imdb(imdb) ? "%s:1:1" : "%s:1", imdb);
  else
    snprintf(id, sizeof id, "%s", imdb);
  snprintf(tp, sizeof tp, "%s", serie ? "series" : "movie");

  pthread_mutex_lock(&legTrava);
  if (!strcmp(id, legId) && !strcmp(tp, legTipo) && (fioLegVivo || nLegs > 0)) {
    pthread_mutex_unlock(&legTrava);
    return;
  }
  snprintf(legId, sizeof legId, "%s", id);
  snprintf(legTipo, sizeof legTipo, "%s", tp);
  legGeracao++;
  legManter = 0;
  nLegs = 0;
  if (fioLegVivo) { pthread_mutex_unlock(&legTrava); return; }
  juntar = fioLegCriado;
  pthread_mutex_unlock(&legTrava);
  dispararLegendas(juntar);
}

// Ver addons.h. Mesmo id normalizado de addons_buscar_legendas, para que os
// extras e a lista falem do MESMO pedido.
void addons_legendas_fonte(const char *imdb, const char *tipo, const char *arquivo,
                           unsigned long long tamanho, const char *hash,
                           const char *urlVideo, int exigeCabecalhos) {
  int serie, juntar, manter;
  char id[64], tp[16];
  LegExtras novo;
  if (!nAddon || !imdb || !*imdb || jfid_e(imdb)) return;
  serie = tipo && !strcmp(tipo, "series");
  if (serie && !idbase_tem_episodio(imdb))
    snprintf(id, sizeof id, idbase_e_imdb(imdb) ? "%s:1:1" : "%s:1", imdb);
  else
    snprintf(id, sizeof id, "%s", imdb);
  snprintf(tp, sizeof tp, "%s", serie ? "series" : "movie");
  memset(&novo, 0, sizeof novo);
  snprintf(novo.id, sizeof novo.id, "%s", id);
  snprintf(novo.arquivo, sizeof novo.arquivo, "%s", arquivo ? arquivo : "");
  novo.tamanho = tamanho;
  snprintf(novo.hash, sizeof novo.hash, "%s", hash ? hash : "");
  snprintf(novo.video, sizeof novo.video, "%s", urlVideo ? urlVideo : "");
  novo.exigeCab = exigeCabecalhos != 0;
  // Nada que um addon possa usar: o pedido sem extras ja e o que esta feito.
  if (!novo.arquivo[0] && !novo.tamanho && !novo.hash[0] &&
      !legextras_url_remota(novo.video)) return;

  pthread_mutex_lock(&legTrava);
  if (!memcmp(&novo, &legExt, sizeof novo) && !strcmp(id, legId) && !strcmp(tp, legTipo)) {
    pthread_mutex_unlock(&legTrava);
    return;
  }
  legExt = novo;
  // Mesmo titulo: a lista atual fica ate a nova terminar. Titulo outro (a
  // fonte chegou antes da busca do episodio): busca normal.
  manter = !strcmp(id, legId) && !strcmp(tp, legTipo) && nLegs > 0;
  snprintf(legId, sizeof legId, "%s", id);
  snprintf(legTipo, sizeof legTipo, "%s", tp);
  legGeracao++;
  legManter = manter;
  if (!manter) nLegs = 0;
  printf("[legendas] fonte conhecida: refazendo a busca com extras%s\n",
         manter ? " (lista atual fica ate a nova chegar)" : "");
  fflush(stdout);
  if (fioLegVivo) { pthread_mutex_unlock(&legTrava); return; }
  juntar = fioLegCriado;
  pthread_mutex_unlock(&legTrava);
  dispararLegendas(juntar);
}

static void dispararLegendas(int juntar) {
  if (juntar) pthread_join(fioLeg, NULL);
  pthread_mutex_lock(&legTrava);
  fioLegCriado = 0;
  legParar = 0;
  fioLegVivo = 1;
  if (pthread_create(&fioLeg, NULL, buscarLegendas, NULL) != 0) fioLegVivo = 0;
  else fioLegCriado = 1;
  pthread_mutex_unlock(&legTrava);
}

// UM FIO POR ADDON DE FONTE.
//
// MEDIDO NA TV, na sessao do dono: 16,5 s entre abrir o titulo e ter uma fonte
// escolhida (detail_abrir 51098 -> fonte escolhida 67659). Eram consultas em
// SERIE com 25 s de timeout cada; um addon lento atrasa todos os outros, e a
// tela fica com "buscando" o tempo todo.
//
// Os addons sao independentes e `extrair` so escreve no balde que recebe, entao
// cada um le no proprio. A ORDEM e preservada na juncao: ela decide qual fonte
// o automatico ve primeiro, e trocar a ordem trocaria a fonte escolhida.
//
// QUATRO TAMBEM NO TIZEN. Na 1.3.4-rc1 eram 2 la (#72): a hipotese era que
// os fios/Workers travavam o fio principal. Os dados a derrubaram — a causa
// era o decode de imagem, e com ele fora do fio principal a rc1 ficou fluida
// com os mesmos addons. O que sobrou de 2 fios foi o efeito colateral: com
// seis addons e um deles preso nos 25 s, a lista de fontes so fechava na
// terceira rodada ("some streams take a very long time to open, especially
// the first time", rawldon na rc1). O fio de rede passa a vida esperando o
// socket; nao e ele que custa.
// NV_ADD_FIOS por -D: a build de comparacao do #80 (rawldon: "v1.3.4, now
// it's back to freezing") volta a 2 no Tizen para isolar se foi isto.
#ifdef NV_ADD_FIOS
#define ADD_FIOS NV_ADD_FIOS
#elif defined(__EMSCRIPTEN__)
// 2 NO TIZEN (1.3.5): a rc1 (2 fios) foi "lisa" no AU7000 e a 1.3.4 (4 fios,
// junto com o GIF a 3 quadros) voltou a travar (#80). Nao esta provado qual
// dos dois foi; os dois voltam ao valor da rc1 e o longtask do [navegador]
// e quem separa.
#define ADD_FIOS 2
#else
// UM FIO POR ADDON (#221), ate 12. Com 4, a quinta consulta so saia quando um
// dos quatro primeiros soltasse — e um addon mudo segura o fio dele os 12 s
// inteiros. No D1 da 1.7.0 o .tpk tem de 4 a 13 addons de fonte por consulta;
// com fila, o addon rapido instalado por ultimo esperava o lento da frente.
// O fio passa a vida esperando socket (ver acima): doze nao custam CPU.
#define ADD_FIOS 12
#endif

typedef struct {
  int    idx;                 // qual addon
  Stream *achados;
  int    n;
  int    respondeu;           // 1 = veio corpo (mesmo com 0 fontes)
  unsigned ms;                // do disparo da consulta ate a resposta (addonstats)
} BaldeFonte;

// SO DE LEGENDA (ou catalogo) SEGUNDO A SONDA (TCL do dono, 09/10). Quem vem da
// conta entra com fonte=1 ate sondar() ler o manifesto, que pode chegar com a
// busca ja no ar: o OpenSubtitles v3 recebia /stream/ na 1a rodada e na
// segunda chance, tomava 404 e a folha dizia que ele "nao respondeu". So
// LEITURA dos campos que a sonda publica: o balde dele sai da segunda chance e
// do resumo (nao conta como consultado, mudo nem vazio).
static int semStreamSondado(int i) {
  return __atomic_load_n(&addon[i].sondado, __ATOMIC_ACQUIRE) &&
         !__atomic_load_n(&addon[i].fonte, __ATOMIC_RELAXED);
}

// O QUE A ULTIMA BUSCA REAL VIU, para a folha de fontes vazia dizer a causa
// (B6/#107 e D5). A folha so dizia "Nenhuma fonte direta disponivel", e tres
// situacoes diferentes davam essa mesma frase:
//   - id 1504 (#107): 4 addons de fonte consultados, os 4 responderam
//     {"streams":[]} (14 bytes) — o titulo nao existe neles;
//   - id 1220 (rel. 22 §2): a conta tem 2 addons e NENHUM declara stream — nao
//     havia a quem perguntar;
//   - addon fora do ar (FrostView com 408, 18/09): "sem resposta".
// Cada uma pede uma acao diferente de quem esta no sofa (esperar, instalar
// um addon, recarregar), e a frase unica nao dizia qual.
//
// Escrito pelo fio de `buscar` ANTES do atomic_store de `estado`, lido pela UI
// depois de ver o estado mudar: a mesma ordem que ja publica `resultado`.
// `valido` = 0 quando a lista veio do cache/prefetch (nao se sabe quem
// respondeu) — ai a folha cai na frase generica.
typedef struct {
  int valido;
  int instalados, comFonte, ligadosComFonte;   // da lista
  int consultados, responderam, semResposta, comFontes;   // da consulta
  int extraIncompleta;
  char mudo[64], vazio[64];   // um nome de exemplo de cada, para o singular
} Resumo;
static Resumo resumo;

// UMA CONSULTA INTEIRA, com tudo que os fios dela compartilham. Era um punhado
// de estaticos (baldes, proxBalde, alvoId, alvoTipo), o que amarrava o modulo
// a UMA consulta por vez: o prefetch dos vizinhos do guia (fontecache.c)
// precisa consultar sem tocar no alvo da busca real, entao o estado passou a
// viajar aqui e cada consulta tem o seu. A trava e por consulta tambem: duas
// consultas ao mesmo tempo nao disputam nada.
typedef struct {
  const char *id, *tipo, *tipoAlt;
  BaldeFonte *baldes;
  int nBaldes, proxBalde;
  int (*cancelado)(void *);   // NULL = nunca cancela
  void *ctx;
  int timeout;                // segundos por requisicao (12 na 1a rodada)
  int progresso;              // 1 = a busca real que publica aos poucos (#221)
  int rodada;                 // 0/1 = primeira, 2 = segunda chance
  Uint32 inicio;              // SDL_GetTicks do disparo, para o log por addon
  pthread_mutex_t trava;
} Consulta;

// O NOME DE TIPO DE CANAL QUE O MANIFESTO DO ADDON USA: "tv", "channel" ou
// NULL (manifesto nao lido, ou nenhum catalogo de canal). Sai de catalogs[],
// que capacidadesDoManifesto ja guarda para o guia: FrostView declara
// "channel", IPTV Bridge e Meu Futebol "tv". Devolve a literal, nunca o campo
// do addon, porque o fio que le nao segura trava nenhuma.
static const char *tipoCanalDeclarado(int i) {
  int k;
  if (i < 0 || i >= nAddon || !addon[i].canalLido) return NULL;
  for (k = 0; k < addon[i].nCanal; k++) {
    if (!strcasecmp(addon[i].canal[k].tipo, "channel")) return "channel";
    if (!strcasecmp(addon[i].canal[k].tipo, "tv")) return "tv";
  }
  return NULL;
}

#define ADD_PRAZO_VOD_S 30   // 1a rodada de filme/serie; a 2a soma mais 30 = 60 s do oficial
// O QUE A SEGUNDA TENTATIVA DE UM ADD-ON MUDO CUSTA (#202). Medido no D1:
// 396 linhas "sem resposta: segunda tentativa (30 s)" — quem nao respondeu em
// 30 s ganhava mais 30 s, e a busca completa so fechava depois. Quem ja foi
// mudo na ultima busca desta TV tenta de novo por 10 s; mudo as ultimas tres
// vezes nem tenta (addonstats.h). Responder rapido na segunda continua
// possivel para quem so estava frio (AIOStreams, #182): a primeira falha dele
// nao o marca como mudo seguido.
static int semSegunda(int i) {
  return addon[i].mudoSeg >= 2 || addonstats_segunda_s(addon[i].nome, ADD_PRAZO_VOD_S) == 0;
}
static int prazoDoAddon(const Consulta *c, int i) {
  int s = c->timeout > 0 ? c->timeout : 12;
  if (c->rodada == 2) {
    int m = addonstats_segunda_s(addon[i].nome, s);
    if (m > 0) s = m;
  }
  return s;
}

static void *fioFontes(void *u) {
  Consulta *c = u;
  for (;;) {
    int meu, i, n, prazoS;
    char url[NV_ADDON_PEDIDO_MAX], idUrl[768], *corpo;
    const char *t1, *t2;
    Stream *achados;
    pthread_mutex_lock(&c->trava);
    if (c->proxBalde >= c->nBaldes) { pthread_mutex_unlock(&c->trava); return NULL; }
    meu = c->proxBalde++;
    pthread_mutex_unlock(&c->trava);
    // Cancelamento entre um addon e outro: o download em curso nao se
    // interrompe (libcurl), mas o proximo nem comeca.
    if (c->cancelado && c->cancelado(c->ctx)) continue;
    i = c->baldes[meu].idx;
    if (!addon[i].ativo) continue;   // desligado na conta: nunca consultado
    if (semStreamSondado(i)) continue;   // a sonda terminou depois dos baldes
    // Id codificado como o Nuvio web (nv_addon_id): "tt123:1:2" sai igual.
    if (!nv_addon_id(idUrl, sizeof idUrl, c->id)) idUrl[0] = 0;
    // O NOME QUE O MANIFESTO DECLARA VAI PRIMEIRO (issue #112). Ver
    // tipoCanalDeclarado: o FrostView declara "channel" e o app perguntava
    // "tv" antes, gastando uma viagem inteira por canal aberto so para
    // receber a lista vazia que agora dispara o segundo nome logo abaixo.
    { const char *dec = c->tipoAlt && c->tipoAlt[0] ? tipoCanalDeclarado(i) : NULL;
      t1 = c->tipo; t2 = c->tipoAlt;
      if (dec && !strcmp(dec, c->tipoAlt)) { t1 = c->tipoAlt; t2 = c->tipo; } }
    // Prazo da consulta (c->timeout): 30 s para filme/serie, 12 s para canal e
    // prefetch. Ver ADD_PRAZO_VOD_S em consultar().
    prazoS = prazoDoAddon(c, i);
    corpo = pedidoCoube(i, nv_addon_url(url, sizeof url, addon[i].base, "/stream/%s/%s.json",
                                    t1, idUrl), sizeof url) && idUrl[0]
            ? rede_baixar(url, prazoS) : NULL;
    achados = NULL; n = 0;
    if (corpo) n = stream_extrair(corpo, addon[i].nome, &achados);
    // CANAL AO VIVO TEM DOIS NOMES DE TIPO NO PROTOCOLO, e addons diferentes
    // usam nomes diferentes.
    //
    // MEDIDO em 18/09 contra o addon de um relato ("canal aparece no guia e nao
    // abre", com "Could not open the source" na tela):
    //   /stream/tv/meufutebol:premiereclubes.json       -> 200
    //   /stream/channel/meufutebol:premiereclubes.json  -> 404
    // O manifesto dele declara "types":["tv"]. O app pedia SEMPRE "channel"
    // (app.c), entao a resposta era 404, "sem resposta", nenhuma fonte, e o
    // erro aparecia sem o player nunca ter tentado.
    //
    // Trocar "channel" por "tv" e so mover o defeito para quem usa o outro
    // nome. Entao pergunta-se o SEGUNDO nome ao addon que nao trouxe fonte
    // com o primeiro — E "NAO TROUXE" INCLUI A LISTA VAZIA, nao so a falta de
    // resposta (issue #112). MEDIDO em 22/09 com curl contra o FrostView TV,
    // manifesto "types":["channel"]:
    //   /stream/tv/cs:channel:axn.json       -> 200 {"streams":[]}  (14 bytes)
    //   /stream/channel/cs:channel:axn.json  -> 200 com 4 fontes
    // O 200 vazio passava pelo `if (!corpo)` antigo, o segundo nome nunca era
    // perguntado e TODO canal do FrostView dava "nenhuma fonte serve" — nos
    // dois alvos, porque este caminho nao tem ramo de plataforma (o log do
    // #112 e de uma Samsung, mas a URL e montada igual na LG). Com e sem
    // Origin: null e User-Agent a resposta e a mesma, byte a byte.
    if (n <= 0 && t2 && t2[0] && !(c->cancelado && c->cancelado(c->ctx))) {
      char *alt;
      alt = pedidoCoube(i, nv_addon_url(url, sizeof url, addon[i].base, "/stream/%s/%s.json",
                                    t2, idUrl), sizeof url) && idUrl[0]
            ? rede_baixar(url, prazoS) : NULL;
      if (alt) {
        Stream *a2 = NULL;
        int n2 = stream_extrair(alt, addon[i].nome, &a2);
        // O segundo so substitui o primeiro quando traz fonte, ou quando o
        // primeiro nem respondeu: um 404 no segundo nome nao apaga o "respondeu
        // vazio" do primeiro, que e o que a mensagem de erro vai citar.
        if (n2 > 0 || !corpo) {
          free(achados); free(corpo);
          achados = a2; n = n2; corpo = alt;
          printf("[addons] %s: respondeu como \"%s\" (nao como \"%s\")\n",
                 addon[i].nome, t2, t1);
        } else { free(a2); free(alt); }
      }
    }
    if (!corpo) {
      free(achados);
      // O MOTIVO NA MESMA LINHA (#202): "curl 28: Operation timed out after
      // 12002 ms" ou o HTTP (a linha "[rede] HTTP 403 em <host>" sai antes, sem
      // o nome do addon). Prazo junto: diz se foi a 1a ou a 2a rodada.
      { const char *erro = rede_ultimo_erro();
        printf("[addons] %s: sem resposta (%u ms, prazo %d s)%s%s\n", addon[i].nome,
               (unsigned)(SDL_GetTicks() - c->inicio), prazoS,
               erro && erro[0] ? ": " : "", erro ? erro : ""); }
      // Desistiu de vez quando nao ha segunda chance pela frente: a mesma regra
      // de segundaChance (mudoSeg ainda e o da consulta anterior aqui).
      if (c->progresso)
        progMarcar(i, NULL, 0, c->rodada == 2 || semSegunda(i) ? 3 : 1);
      continue;
    }
    if (c->progresso) progMarcar(i, achados, n, 2);
    c->baldes[meu].respondeu = 1;
    c->baldes[meu].ms = (unsigned)(SDL_GetTicks() - c->inicio);
    c->baldes[meu].n = n;
    c->baldes[meu].achados = achados;
    // O TEMPO DE CADA ADDON NO LOG (#221): sem ele o D1 so dava o total da
    // consulta, e "quem segura" tinha de ser adivinhado pela ordem das linhas.
    printf("[addons] %s: %d fontes (%u bytes, %u ms)\n",
           addon[i].nome, c->baldes[meu].n, (unsigned)strlen(corpo),
           (unsigned)(SDL_GetTicks() - c->inicio));
    // RESPOSTA CURTA SEM FONTE VAI PARA O LOG. No registro 1504 havia
    // "Torrentio TB: 0 fontes (75 bytes)": 75 bytes nao sao {"streams":[]}
    // (14), e provavelmente e o addon dizendo por que (chave de debrid
    // invalida, limite) — mas o log nao guardava o texto. Ate 200 bytes
    // porque acima disso e lista de verdade que o parser recusou, e o que
    // falta ali e outra coisa; 80 bytes cabem numa mensagem de erro de addon.
    // Corpo de addon nao leva a chave dele (ela vai no caminho da URL, que
    // aqui nao se imprime); quebra de linha vira espaco para ficar numa linha.
    if (c->baldes[meu].n == 0 && strlen(corpo) <= 200) {
      char amostra[81];
      size_t k;
      snprintf(amostra, sizeof amostra, "%s", corpo);
      for (k = 0; amostra[k]; k++)
        if ((unsigned char)amostra[k] < ' ') amostra[k] = ' ';
      printf("[addons] %s: resposta sem fonte: %s\n", addon[i].nome, amostra);
    }
    free(corpo);
  }
}

// --- saude por addon (ver addons_fora_do_ar) ------------------------------------
static pthread_mutex_t foraTrava = PTHREAD_MUTEX_INITIALIZER;
static char foraNome[64];
static unsigned foraSeq;
static void foraAnotar(const char *nome) {
  pthread_mutex_lock(&foraTrava);
  snprintf(foraNome, sizeof foraNome, "%s", nome ? nome : "");
  foraSeq++;
  pthread_mutex_unlock(&foraTrava);
}
unsigned addons_fora_do_ar(char *nome, unsigned tam) {
  unsigned s;
  pthread_mutex_lock(&foraTrava);
  s = foraSeq;
  if (nome && tam) snprintf(nome, tam, "%s", foraNome);
  pthread_mutex_unlock(&foraTrava);
  return s;
}

// O segundo nome de tipo de canal ao vivo; "" para os demais. Ver fioFontes.
static const char *tipoAlternativo(const char *tipo) {
  if (!strcmp(tipo, "tv"))      return "channel";
  if (!strcmp(tipo, "channel")) return "tv";
  return "";
}

// SEGUNDA CHANCE (#182: "addons sometimes not loading, I must reload source a
// few times to see the addon fetch (AIOStreams)"). Um addon que agrega varios
// scrapers (AIOStreams) demora mais que os 12 s na primeira consulta de um
// titulo e responde rapido na seguinte, porque ja guardou o resultado — o que
// o dono fazia a mao com Recarregar. A lista era publicada sem ele e nada mais
// o perguntava. Agora quem NAO respondeu (timeout ou erro; lista vazia conta
// como resposta) e perguntado UMA vez mais, em paralelo, com 20 s (30 s em
// filme/serie, #202), antes de a
// lista ser publicada. Custo limitado: um addon que falha nas duas rodadas
// duas consultas seguidas deixa de ganhar a segunda (mudoSeg).
static void segundaChance(Consulta *c, int fios) {
  Consulta c2;
  int q, m = 0, criados = 0, pulados = 0;
  pthread_t f[ADD_FIOS];
  int *orig;
  if (c->cancelado && c->cancelado(c->ctx)) return;
  orig = calloc((size_t)c->nBaldes, sizeof *orig);
  memset(&c2, 0, sizeof c2);
  c2.baldes = calloc((size_t)c->nBaldes, sizeof(BaldeFonte));
  if (!orig || !c2.baldes) { free(orig); free(c2.baldes); return; }
  for (q = 0; q < c->nBaldes; q++) {
    int i = c->baldes[q].idx;
    if (c->baldes[q].respondeu || semStreamSondado(i)) continue;
    if (semSegunda(i)) { pulados++; continue; }
    c2.baldes[m].idx = i; orig[m++] = q;
  }
  if (m > 0) {
    c2.id = c->id; c2.tipo = c->tipo; c2.tipoAlt = c->tipoAlt;
    c2.nBaldes = m; c2.cancelado = c->cancelado; c2.ctx = c->ctx;
    c2.timeout = c->timeout > 20 ? c->timeout : 20;
    c2.progresso = c->progresso; c2.rodada = 2; c2.inicio = c->inicio;
    pthread_mutex_init(&c2.trava, NULL);
    printf("[addons] %d sem resposta: segunda tentativa (%d s%s)", m, c2.timeout,
           pulados ? "; " : "");
    if (pulados) printf("%d mudo(s) seguidos pulado(s)", pulados);
    printf("\n");
    fflush(stdout);
    if (fios > ADD_FIOS) fios = ADD_FIOS;
    for (q = 0; q < fios && q < m; q++)
      if (pthread_create(&f[criados], NULL, fioFontes, &c2) == 0) criados++;
    if (!criados) fioFontes(&c2);
    for (q = 0; q < criados; q++) pthread_join(f[q], NULL);
    for (q = 0; q < m; q++) {
      if (!c2.baldes[q].respondeu) continue;
      c->baldes[orig[q]] = c2.baldes[q];
      printf("[addons] %s: respondeu na segunda tentativa\n", addon[c2.baldes[q].idx].nome);
    }
    pthread_mutex_destroy(&c2.trava);
  }
  free(c2.baldes); free(orig);
}

// MAIS UMA ORIGEM DE FONTES: os plugins Nuvio (plugins.c, F09), ligados por
// ponteiro para este modulo continuar compilando sozinho nos testes. Roda num
// fio proprio AO MESMO TEMPO que os addons; cada scraper entra na folha como
// mais uma origem (progMarcarEx) e a lista final soma as fontes dele depois
// das dos addons, com o nome do scraper como provedor.
// Ate duas origens (plugins Nuvio e o servidor Plex casado por IMDb/TMDB), cada
// uma no proprio fio: a consulta rapida ao Plex nao espera os scrapers e os
// scrapers nao seguram a fonte do servidor. As listas se somam na ordem de
// registro.
#define ORIGENS_EXTRA_MAX 2
static OrigemExtra origensExtra[ORIGENS_EXTRA_MAX];
static int (*origensExtraAtiva[ORIGENS_EXTRA_MAX])(void);
static int nOrigensExtra;
void addons_definir_origem_extra(OrigemExtra f, int (*ativa)(void)) {
  int i;
  if (!f) return;
  for (i = 0; i < nOrigensExtra; i++)
    if (origensExtra[i] == f) { origensExtraAtiva[i] = ativa; return; }
  if (nOrigensExtra < ORIGENS_EXTRA_MAX) {
    origensExtra[nOrigensExtra] = f;
    origensExtraAtiva[nOrigensExtra] = ativa;
    nOrigensExtra++;
  }
}
static int origemExtraViva(int i) {
  return origensExtra[i] && origensExtraAtiva[i] && origensExtraAtiva[i]();
}
int addons_origem_extra_ativa(void) {
  int i;
  for (i = 0; i < nOrigensExtra; i++) if (origemExtraViva(i)) return 1;
  return 0;
}

typedef struct {
  const char *id, *tipo;
  int (*cancelado)(void *); void *ctx;
  int progresso;
  Stream *l; int n;
  _Atomic int pendentes, incompleta;
} PedidoExtra;
static int cancelaExtra(void *u) {
  PedidoExtra *p = u;
  return p->cancelado && p->cancelado(p->ctx) ? 1 : 0;
}
static void avisoExtra(void *u, int k, const char *nome, int estado, const void *fontes, int n) {
  PedidoExtra *p = u;
  // Cada parte anuncia inicio (1) e fim (2/3), inclusive no prefetch.
  // Contadores atomicos cobrem scrapers paralelos e indices de origens distintas.
  if (estado == 1) atomic_fetch_add(&p->pendentes, 1);
  if (estado == 2 || estado == 3) atomic_fetch_sub(&p->pendentes, 1);
  if (estado == 3) atomic_store(&p->incompleta, 1);
  if (!p->progresso) return;
  // Scraper que terminou (2) ou desistiu (3): entra na memoria de latencia.
  if ((estado == 2 || estado == 3) && nome && !(p->cancelado && p->cancelado(p->ctx)))
    addonstats_registrar(nome, (unsigned)(SDL_GetTicks() - progInicio), estado == 2);
  progMarcarEx(k, nome, (const Stream *)fontes, n, estado);
}
typedef struct { PedidoExtra *pai; OrigemExtra f; Stream *l; int n; } UmaOrigem;
static void *fioUmaOrigem(void *u) {
  UmaOrigem *o = u;
  o->n = o->f(o->pai->id, o->pai->tipo, cancelaExtra, o->pai, avisoExtra, o->pai, &o->l);
  // ponytail: zero tambem pode ser falha sem callbacks (Plex/TMDB).
  // Nao cachear ate as origens distinguirem falha de resposta vazia.
  if (o->n <= 0) atomic_store(&o->pai->incompleta, 1);
  return NULL;
}
static void *fioExtra(void *u) {
  PedidoExtra *p = u;
  UmaOrigem o[ORIGENS_EXTRA_MAX];
  pthread_t f[ORIGENS_EXTRA_MAX];
  int vivo[ORIGENS_EXTRA_MAX], i, total = 0;
  memset(o, 0, sizeof o);
  for (i = 0; i < nOrigensExtra; i++) {
    vivo[i] = 0;
    if (!origemExtraViva(i)) continue;
    o[i].pai = p; o[i].f = origensExtra[i];
    // A ultima origem roda neste fio; as anteriores em fios proprios.
    if (i + 1 < nOrigensExtra) {
      pthread_attr_t a;
      pthread_attr_init(&a);
      pthread_attr_setstacksize(&a, 512u * 1024);
      vivo[i] = pthread_create(&f[i], &a, fioUmaOrigem, &o[i]) == 0;
      pthread_attr_destroy(&a);
      if (!vivo[i]) fioUmaOrigem(&o[i]);
    } else fioUmaOrigem(&o[i]);
  }
  for (i = 0; i < nOrigensExtra; i++) {
    if (vivo[i]) pthread_join(f[i], NULL);
    if (o[i].n > 0) total += o[i].n;
  }
  if (total > 0) {
    Stream *l = malloc(sizeof(Stream) * (size_t)total);
    int k = 0;
    if (l) {
      for (i = 0; i < nOrigensExtra; i++)
        if (o[i].n > 0) { memcpy(l + k, o[i].l, sizeof(Stream) * (size_t)o[i].n); k += o[i].n; }
      p->l = l; p->n = k;
    }
  }
  // Scraper que o corte (30 s) deixou sem resposta: mudo nesta busca. Cancelada
  // a busca, nao prova nada.
  if (p->progresso && !(p->cancelado && p->cancelado(p->ctx))) {
    pthread_mutex_lock(&progTrava);
    for (i = 0; i < ADD_EXTRA_MAX; i++)
      if (progEstadoEx[i] == 1 && progNomeEx[i][0])
        addonstats_registrar(progNomeEx[i], (unsigned)(SDL_GetTicks() - progInicio), 0);
    pthread_mutex_unlock(&progTrava);
  }
  for (i = 0; i < nOrigensExtra; i++) free(o[i].l);
  return NULL;
}

static int consultar(const char *id, const char *tipo, const char *base, int fios,
                     int (*cancelado)(void *), void *ctx, Stream **saida,
                     Resumo *rs, int progresso) {
  Consulta c;
  Stream *achados = NULL;
  int n = 0, i, q;
  PedidoExtra extra;
  pthread_t fioEx;
  int temExtra = 0;
  *saida = NULL;
  if (!id || !*id || !tipo || !*tipo) return 0;
  // So na busca ampla: canal com origem conhecida pergunta a um addon so.
  memset(&extra, 0, sizeof extra);
  if (!(base && *base) && addons_origem_extra_ativa()) {
    pthread_attr_t a;
    extra.id = id; extra.tipo = tipo; extra.cancelado = cancelado; extra.ctx = ctx;
    extra.progresso = progresso;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 512u * 1024);
    temExtra = pthread_create(&fioEx, &a, fioExtra, &extra) == 0;
    pthread_attr_destroy(&a);
    if (!temExtra) fioExtra(&extra);
  }
  if (nAddon <= 0) {
    if (temExtra) pthread_join(fioEx, NULL);
    if (rs) rs->extraIncompleta = extra.pendentes != 0 || extra.incompleta;
    if (cancelado && cancelado(ctx)) { free(extra.l); return -1; }
    if (extra.n > 0) { *saida = extra.l; return extra.n; }
    free(extra.l);
    return 0;
  }
  memset(&c, 0, sizeof c);
  c.id = id; c.tipo = tipo; c.tipoAlt = tipoAlternativo(tipo);
  c.cancelado = cancelado; c.ctx = ctx;
  c.progresso = progresso;
  // PRAZO DA BUSCA DE FILME/SERIE (paridade com o Nuvio oficial, #202). O
  // Nuvio web espera 60 s por addon (streamRepository,
  // STREAM_SOURCE_REQUEST_TIMEOUT_MS) num pedido so. Aqui eram 12 s e, para
  // quem nao respondeu, um pedido NOVO de 20 s: addon que raspa indexadores na
  // hora (StreamFusion, WAStream, Comet sem cache) era cortado no meio do
  // trabalho e perguntado de novo do zero. Log do .tpk (2.0.0):
  // "StreamFusionReborn falha 28 ... after 12002 milliseconds with 0 bytes",
  // e so a segunda volta trouxe fonte. Com a lista publicada aos poucos (#221)
  // o prazo longo nao segura quem ja respondeu, e a fonte automatica tem prazo
  // proprio (fonteauto). Canal ao vivo e prefetch continuam com 12 s.
  c.timeout = progresso ? ADD_PRAZO_VOD_S : 12;
  c.inicio = SDL_GetTicks();
  pthread_mutex_init(&c.trava, NULL);
  c.baldes = calloc((size_t)nAddon, sizeof(BaldeFonte));
  if (c.baldes) {
    // SO O ADDON DE ORIGEM, quando se sabe qual e. Comparacao por base
    // normalizada — "<base>" e "<base>/manifest.json" sao o mesmo addon.
    if (base && *base) {
      char alvo[NV_ADDON_URL_MAX], mine[NV_ADDON_URL_MAX];
      baseNormalizada(base, alvo, sizeof alvo);
      for (i = 0; i < nAddon; i++) {
        if (!addon[i].ativo || !addon[i].fonte) continue;
        baseNormalizada(addon[i].base, mine, sizeof mine);
        if (!strcmp(mine, alvo)) c.baldes[c.nBaldes++].idx = i;
      }
    }
    // Sem origem conhecida, ou origem que nao esta (mais) na lista: todos,
    // como sempre. Melhor perguntar a mais do que nao perguntar a ninguem.
    if (c.nBaldes == 0)
      for (i = 0; i < nAddon; i++)
        if (addon[i].ativo && addon[i].fonte) c.baldes[c.nBaldes++].idx = i;
  }

  // QUEM FICOU DE FORA, E POR QUE (issue #83).
  //
  // Um addon nao consultado nao imprime NADA: nem "N fontes", nem "sem
  // resposta". No log do usuario ele some, e "desligado na conta" fica
  // indistinguivel de "consultado e nao respondeu" — que e exatamente a
  // duvida do #83 ("either the addons aren't being searched or frostview
  // might be overriding the two others"). MEDIDO nos registros 1.3.12 do D1:
  // no id 1383 o manifesto do PenguPlay diz stream=1 e ele nao aparece em
  // NENHUMA das sete consultas da sessao; nao ha como dizer, pelo log, se foi
  // `ativo` ou `fonte` que o barrou.
  //
  // UMA VEZ POR VERSAO DE LISTA, e nao por consulta: a mesma resposta em toda
  // abertura de titulo seria ruido, e a lista so muda quando muda.
  //
  // SO NA BUSCA AMPLA. Com origem conhecida (canal do guia) a consulta vai de
  // proposito a um addon so, e contar "1 de 16" ali seria alarme falso.
  //
  // Os dois estaticos ficam sem trava de proposito: o prefetch e a busca real
  // podem entrar aqui ao mesmo tempo, e o pior que acontece e a folha sair
  // duas vezes. Uma trava para nao repetir uma linha de log custaria mais do
  // que vale.
  { static unsigned ultimaFolha;
    static int folhaFeita;
    if (!(base && *base) && (!folhaFeita || ultimaFolha != versaoLista)) {
      ultimaFolha = versaoLista; folhaFeita = 1;
      for (i = 0; i < nAddon; i++) {
        if (addon[i].ativo && addon[i].fonte) continue;
        printf("[addons] fora da busca de fontes: %s (%s)\n", addon[i].nome,
               !addon[i].ativo ? "desligado" :
               addon[i].sondado ? "o manifesto nao declara stream"
                                : "ainda sem manifesto");
      }
      printf("[addons] %d de %d consultados por fonte\n", c.nBaldes, nAddon);
      fflush(stdout);
    } }

  if (progresso && c.baldes) {
    pthread_mutex_lock(&progTrava);
    for (q = 0; q < c.nBaldes; q++)
      if (c.baldes[q].idx < ADD_MAX) progEstado[c.baldes[q].idx] = 1;
    pthread_mutex_unlock(&progTrava);
  }

  if (c.baldes && c.nBaldes > 0) {
    pthread_t f[ADD_FIOS];
    int criados = 0;
    if (fios > ADD_FIOS) fios = ADD_FIOS;
    for (q = 0; q < fios && q < c.nBaldes; q++)
      if (pthread_create(&f[criados], NULL, fioFontes, &c) == 0) criados++;
    if (!criados) fioFontes(&c);          // sem fios: em serie, mesmo resultado
    for (q = 0; q < criados; q++) pthread_join(f[q], NULL);
    segundaChance(&c, fios);
    // Junta NA ORDEM DOS ADDONS, que e a ordem em que o dono os instalou.
    for (q = 0; q < c.nBaldes; q++) {
      int k = c.baldes[q].n;
      // Tambem quem respondeu 200 vazio: sem recurso stream, "veio vazio" nao
      // diz nada. Limite que fica: a sonda terminando DEPOIS deste resumo.
      if (k <= 0 && semStreamSondado(c.baldes[q].idx)) {
        printf("[addons] %s: o manifesto nao declara stream; fora do resumo da busca\n",
               addon[c.baldes[q].idx].nome);
        if (c.progresso && c.baldes[q].idx < ADD_MAX) progMarcar(c.baldes[q].idx, NULL, 0, 0);
        free(c.baldes[q].achados);
        continue;
      }
      // A LATENCIA DESTA BUSCA fica na memoria desta TV (addonstats.h): so a
      // busca real e nao cancelada — prefetch e busca interrompida nao provam
      // nada sobre o add-on.
      if (c.progresso && !(c.cancelado && c.cancelado(c.ctx)))
        addonstats_registrar(addon[c.baldes[q].idx].nome,
                             c.baldes[q].respondeu ? c.baldes[q].ms
                                                   : (unsigned)(SDL_GetTicks() - c.inicio),
                             c.baldes[q].respondeu);
      // Cancelamento nao prova falha nem recuperacao, nem dos baldes pulados.
      if (!(c.cancelado && c.cancelado(c.ctx))) {
        if (c.baldes[q].respondeu) addon[c.baldes[q].idx].mudoSeg = 0;
        else {
          addon[c.baldes[q].idx].mudoSeg++;
          // ADDON FORA DO AR (ilha, 02/10): a busca de verdade (rs) terminou e
          // ESTE addon nao respondeu nem na segunda chance — transporte ou HTTP,
          // nunca "respondeu sem fonte", que e `respondeu` com n = 0. Uma vez por
          // queda: so na PRIMEIRA consulta muda (mudoSeg 0 -> 1); responder de
          // novo zera e rearma. Cancelada no meio nao conta.
          if (rs && rs->valido && addon[c.baldes[q].idx].mudoSeg == 1)
            foraAnotar(addon[c.baldes[q].idx].nome);
        }
      }
      if (rs) {
        const char *nome = addon[c.baldes[q].idx].nome;
        rs->consultados++;
        if (!c.baldes[q].respondeu) {
          if (!rs->semResposta++) snprintf(rs->mudo, sizeof rs->mudo, "%s", nome);
        } else if (k > 0) rs->comFontes++;
        else if (!rs->responderam++) snprintf(rs->vazio, sizeof rs->vazio, "%s", nome);
      }
      if (k > 0) {
        Stream *tmp = realloc(achados, sizeof(Stream) * (size_t)(n + k));
        if (tmp) { achados = tmp;
          memcpy(achados + n, c.baldes[q].achados, sizeof(Stream) * (size_t)k);
          n += k;
        } else printf("[addons] memoria insuficiente para %d fontes\n", k);
      }
      free(c.baldes[q].achados);
    }
  }
  free(c.baldes);
  pthread_mutex_destroy(&c.trava);
  if (temExtra) pthread_join(fioEx, NULL);
  if (rs) rs->extraIncompleta = extra.pendentes != 0 || extra.incompleta;
  if (progresso) addonstats_salvar();
  if (extra.n > 0) {
    Stream *tmp = realloc(achados, sizeof(Stream) * (size_t)(n + extra.n));
    if (tmp) { achados = tmp; memcpy(achados + n, extra.l, sizeof(Stream) * (size_t)extra.n); n += extra.n; }
  }
  free(extra.l);
  // Cancelada, a lista pode estar pela metade: nao e resposta, e lixo.
  if (cancelado && cancelado(ctx)) { free(achados); return -1; }
  *saida = achados;
  return n;
}

int addons_consultar(const char *id, const char *tipo, const char *base, int fios,
                     int (*cancelado)(void *), void *ctx, Stream **saida) {
  // Uma resposta VOD incompleta nao pode esconder o addon que falhou na
  // busca real seguinte. O cache da busca principal aplica a mesma guarda.
  Resumo rs = {0};
  int vod = tipo && !strcmp(tipo, "series");
  int n = consultar(id, tipo, base, fios, cancelado, ctx, saida, vod ? &rs : NULL, 0);
  if (vod && (rs.semResposta || rs.extraIncompleta)) { free(*saida); *saida = NULL; return -1; }
  return n;
}

// Contagens da LISTA (nao da consulta), para "nao ha a quem perguntar".
static void resumoDaLista(Resumo *rs) {
  int i;
  memset(rs, 0, sizeof *rs);
  rs->valido = 1;
  rs->instalados = nAddon;
  for (i = 0; i < nAddon; i++) {
    if (!addon[i].fonte) continue;
    rs->comFonte++;
    if (addon[i].ativo) rs->ligadosComFonte++;
  }
}

// Frases curtas, uma por causa. `responderam` conta so quem respondeu SEM
// fonte: quem trouxe fonte nao explica lista vazia (se ha fonte e a lista
// esta vazia, a causa e o descarte de torrent sem debrid, e quem diz isso e
// streams.c).
//
// CANAL TEM FRASE PROPRIA (issue #112). "nao tem este titulo" le como se o
// canal nao existisse no addon; o que o FrostView diz com {"streams":[]} e que
// AGORA nao ha link para ele — os mesmos canais voltam a ter fonte depois.
int addons_motivo_vazio(char *dst, unsigned n) {
  const Resumo *r = &resumo;
  int canal = !strcmp(alvoTipo, "tv") || !strcmp(alvoTipo, "channel");
  if (!dst || !n || !r->valido) return 0;
  if (!r->instalados || !r->comFonte)
    snprintf(dst, n, "%s", i18n("Nenhum add-on de fontes instalado"));
  else if (!r->ligadosComFonte)
    snprintf(dst, n, "%s", i18n("Os add-ons de fontes estão desligados"));
  else if (!r->consultados || r->comFontes)
    return 0;
  else if (!r->semResposta && canal)
    r->responderam == 1
      ? snprintf(dst, n, i18n("%s não tem fonte para este canal agora"), r->vazio)
      : snprintf(dst, n, i18n("%d add-ons responderam: nenhum tem fonte para este canal agora"), r->responderam);
  else if (!r->semResposta)
    r->responderam == 1
      ? snprintf(dst, n, i18n("%s respondeu: não tem este título"), r->vazio)
      : snprintf(dst, n, i18n("%d add-ons responderam: nenhum tem este título"), r->responderam);
  else if (!r->responderam)
    r->semResposta == 1
      ? snprintf(dst, n, i18n("%s não respondeu"), r->mudo)
      : snprintf(dst, n, i18n("%d add-ons não responderam"), r->semResposta);
  else
    snprintf(dst, n, i18n("%d sem este título · %d sem resposta"),
             r->responderam, r->semResposta);
  return 1;
}

static void *buscar(void *u) {
  Stream *achados = NULL;
  int n;
  (void)u;
  Resumo rs;
  marco("addons: consulta inicio");
  resumoDaLista(&rs);
  n = consultar(alvoId, alvoTipo, fioBase, ADD_FIOS, NULL, NULL, &achados, &rs,
                progLigado);
  resultadoQuando = SDL_GetTicks();
  resultadoCacheavel = n > 0 && rs.semResposta == 0 && !rs.extraIncompleta;
  if (n < 0) n = 0;
  resumo = rs;
  marco(n ? "addons: fontes recebidas" : "addons: nenhuma fonte");
  // O canal fica no cache para o zap de volta. VOD so entra na publicacao
  // pela UI, depois de conferir a conta/perfil/configuracao capturados.
  fontecache_guardar(alvoId, alvoTipo, achados, n);
  resultado = achados; nResultado = n;
  printf("[addons] total %d\n", n);
  fflush(stdout);
  atomic_store(&estado, n ? ADD_PRONTO : ADD_VAZIO);
  return NULL;
}

// A busca real do alvo corrente vai a rede. Chamado com fioVivo == 0.
static void dispararBusca(void) {
  // Pedido real tem prioridade: o prefetch em curso (de OUTRO canal — o deste
  // teria sido adotado em addons_buscar) larga os addons que faltam.
  fontecache_ceder();
  estado = ADD_BUSCANDO;
  fioVivo = 1;
  progLimpar();
  progLigado = alvoVod();
  progPublicou = 0; progExtraPublicou = 0;
  progInicio = SDL_GetTicks();
  addons_capturar_escopo(&fioEscopo);
  snprintf(fioBase, sizeof fioBase, "%s", alvoBase);
  alvoBase[0] = 0;   // consumida: origem e do pedido, nao de sessao
  if (pthread_create(&fio, NULL, buscar, NULL) != 0) { fioVivo = 0; progLigado = 0; estado = ADD_PARADO; }
}

// Diz de que addon o PROXIMO alvo veio. Chamar ANTES de addons_buscar; a
// busca seguinte consome e zera — origem e do pedido, nao de sessao.
void addons_definir_origem(const char *base) {
  snprintf(alvoBase, sizeof alvoBase, "%s", base ? base : "");
}

static void buscarPedido(const char *imdb, const char *tipo, int forcar) {
  int serie, renovar;
  char id[sizeof alvoId];
  if (!imdb || !*imdb) return;
  if (jfid_e(imdb)) {
    // PERSONAL SERVER ITEM. No addon, no source cache, no "where to watch":
    // the target is already the exact movie/episode item on that server.
    if (fioVivo) {
      snprintf(pendId, sizeof pendId, "%s", imdb);
      snprintf(pendTipo, sizeof pendTipo, "%s", tipo ? tipo : "movie");
      pendRenovar = forcar;
      pendBase[0] = 0;
      return;
    }
    resumo.valido = 0;
    adotado = 0;
    snprintf(alvoId, sizeof alvoId, "%s", imdb);
    snprintf(alvoTipo, sizeof alvoTipo, "%s", tipo && *tipo ? tipo : "movie");
    alvoBase[0] = 0;
    stream_definir_lista(NULL, 0);
    jfAlvo = servidores_fontes_pedir(alvoId);
    estado = jfAlvo ? ADD_BUSCANDO : ADD_VAZIO;
    return;
  }
  jfAlvo = 0;
  ondever_pedir(imdb, tipo && (!strcmp(tipo, "tv") || !strcmp(tipo, "series")), 0);
  resumo.valido = 0;
  // Recusa de conta do debrid vale por busca: a nova volta a tentar todos.
  debrid_nova_busca();
  if (!nAddon && !addons_origem_extra_ativa()) { stream_definir_lista(NULL, 0); resumoDaLista(&resumo); estado = ADD_VAZIO; return; }
  // Serie SEM episodio devolve lista vazia, com HTTP 200 e sem erro nenhum
  // (medido: 14 bytes de resposta). O identificador tem de ser
  // "tt1234567:temporada:episodio". Como o catalogo ainda nao traz lista de
  // episodios, assume T1E1 — e o mesmo lugar onde o episodio real entra quando
  // houver.
  // Serie de addon de anime ("kitsu:41370") pede "id:episodio", nao "id:1:1".
  serie = tipo && !strcmp(tipo, "series");
  if (serie && !idbase_tem_episodio(imdb))
    snprintf(id, sizeof id, idbase_e_imdb(imdb) ? "%s:1:1" : "%s:1", imdb);
  else
    snprintf(id, sizeof id, "%s", imdb);
  // O CARIMBO E O ID QUE VAI AOS ADDONS (bloqueador 2.0.3, TCL do dono). O
  // detalhe aberto antes de a lista de episodios chegar carimba o id cru
  // ("tt8714904"; app.c idDoAlvo) e a pergunta sai como "tt8714904:1:1"; o
  // player confere "tt8714904:1:1", acha a lista "de outro alvo" e descartava
  // as 28 fontes do MESMO episodio ("episode changed"). Quem pediu carimbou o
  // id cru agora mesmo; aqui ele vira o que de fato foi perguntado.
  if (strcmp(id, imdb)) stream_definir_alvo(id);
  if (fioVivo) {
    if (forcar || strcmp(id, alvoId) || strcmp(tipo ? tipo : "movie", alvoTipo)) {
      snprintf(pendId, sizeof pendId, "%s", imdb);
      snprintf(pendTipo, sizeof pendTipo, "%s", tipo ? tipo : "movie");
      pendRenovar = forcar;
      snprintf(pendBase, sizeof pendBase, "%s", alvoBase);
    } else if (progLigado && !pendId[0] && !stream_lista_do_alvo(id))
      // O MESMO alvo, com a busca dele no ar, e a lista da tela nao e mais
      // dela: devolve o que ja chegou em vez de esperar (ou perder) — senao so
      // as fontes que chegarem daqui em diante entram (as de plugin, quase
      // sempre as mais lentas) e o automatico escolhe entre elas.
      progRepublicar();
    return;
  }
  // Um pedido novo desfaz a espera pelo prefetch do anterior; o prefetch em si
  // segue ou cede conforme o que vem abaixo.
  adotado = 0;
  snprintf(alvoId, sizeof alvoId, "%s", id);
  snprintf(alvoTipo, sizeof alvoTipo, "%s", tipo && *tipo ? tipo : "movie");
  // Pedir de novo a lista que ainda esta ativa e renovar/recarregar, inclusive
  // depois de falha de reproducao: esse pedido continua indo a rede.
  renovar = forcar || (stream_n() > 0 && stream_lista_do_alvo(alvoId));
  stream_definir_lista(NULL, 0);
  { int t = 0, e = 0;
    idbase_episodio(alvoId, &t, &e);
    debrid_definir_episodio(t, e); }
  if (alvoVod()) {
    FontecacheEscopo escopo;
    Stream *l;
    int n;
    Uint32 idade;
    addons_capturar_escopo(&escopo);
    if (renovar) fontecache_vod_apagar(alvoId, alvoTipo, alvoBase, &escopo);
    else if (fontecache_vod_pegar(alvoId, alvoTipo, alvoBase, &escopo,
                            &l, &n, &idade) == FC_ACERTO) {
      stream_definir_lista_idade(l, n, idade);
      free(l);
      alvoBase[0] = 0;
      estado = ADD_PRONTO;
      printf("[addons] %d fontes VOD reaproveitadas (%u ms)\n", n, (unsigned)idade);
      return;
    }
  }
  // O CACHE ANTES DA REDE. Canal que o guia engatilhou (ou que acabou de sair
  // do ar) responde daqui, sem fio nenhum; canal cujo prefetch esta na rede
  // AGORA e adotado — esperar o que ja esta a caminho e mais curto que repetir
  // as mesmas requisicoes, e addons_estado publica quando chegar.
  { Stream *l; int n;
    int r = fontecache_pegar(alvoId, alvoTipo, &l, &n);
    if (forcar && r == FC_ACERTO) { free(l); r = FC_NADA; }
    if (forcar && r == FC_EM_CURSO) r = FC_NADA;
    if (r == FC_ACERTO) {
      printf("[addons] %s: %d fontes do cache\n", alvoId, n);
      stream_definir_lista(l, n);
      free(l);
      estado = n ? ADD_PRONTO : ADD_VAZIO;
      return;
    }
    if (r == FC_EM_CURSO) {
      printf("[addons] %s: prefetch em curso, esperando por ele\n", alvoId);
      adotado = 1;
      estado = ADD_BUSCANDO;
      return;
    } }
  dispararBusca();
}

void addons_buscar(const char *imdb, const char *tipo) { buscarPedido(imdb, tipo, 0); }
void addons_buscar_renovar(const char *imdb, const char *tipo) { buscarPedido(imdb, tipo, 1); }

void addons_encerrar(void) {
  int juntarLeg;
  fontecache_encerrar();
  adotado = 0;
  if (fioVivo) pthread_join(fio, NULL);
  fioVivo = 0;
  progLigado = 0; progPublicou = 0;
  progLimpar();
  pthread_mutex_lock(&legTrava);
  legParar = 1; legGeracao++; juntarLeg = fioLegCriado;
  pthread_mutex_unlock(&legTrava);
  if (juntarLeg) pthread_join(fioLeg, NULL);
  pthread_mutex_lock(&legTrava);
  fioLegCriado = fioLegVivo = 0; nLegs = 0;
  pthread_mutex_unlock(&legTrava);
  free(resultado); resultado = NULL; nResultado = 0;
  estado = ADD_PARADO;
}

// As fontes de colecao da conta trazem addonId (o "id" do manifesto), nao a
// URL. So a sonda sabe o id, entao a resposta e vazia ate ela passar por
// aquele addon — quem chama tenta de novo depois.
const char *addons_base_por_id(const char *id) {
  int i;
  if (!id || !*id) return "";
  for (i = 0; i < nAddon; i++)
    if (addon[i].id[0] && !strcmp(addon[i].id, id)) return addon[i].base;
  return "";
}
