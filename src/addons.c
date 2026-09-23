#include "addons.h"
#include "idioma.h"
#include "linguas.h"
#include "streams.h"
#include "debrid.h"
#include "rede.h"
#include "js.h"
#include "marco.h"
#include "fontecache.h"
// So para a cache UNICA de manifesto (desc_manifesto_cache_obter/guardar): ver
// a nota grande em sondar(), mais abaixo.
#include "descoberta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <pthread.h>
#include <stdatomic.h>

// 16, e nao 12. O sync le ate SY_ADD_MAX (16) addons da conta e entregava a
// lista inteira aqui; o laco de addons_definir_lista cortava no 12o EM SILENCIO
// — o log dizia "12 vindos da conta" como se fossem todos. Quem tem mais de
// doze addons via os ultimos sumirem sem nenhuma explicacao, que e exatamente o
// "some addons were missing (i dont know the reason)" do #42. Os dois tetos
// agora sao o mesmo numero, e o corte, se um dia voltar a acontecer, e dito.
#define ADD_MAX 16

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
  char nome[64]; char base[600];
  int fonte, catalogo, legenda;
  int ativo, sondado;
  char id[96];   // "id" do manifesto; as colecoes da conta apontam para ele
  // Catalogos de canal do manifesto (ver addons_catalogos_canal). `canalLido`
  // separa "nao declara nenhum" de "manifesto ainda nao lido".
  AddCatCanal canal[ADD_CANAL_MAX]; int nCanal, canalLido;
} addon[ADD_MAX];
static int nAddon;
static unsigned versaoLista;   // ver addons_versao
static _Atomic AddEstado estado = ADD_PARADO;
static pthread_t fio;
static char alvoId[64], alvoTipo[16];
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
static char alvoBase[600];
// A COPIA QUE O FIO LE. app.c chama addons_definir_origem(NULL) logo depois de
// addons_buscar, e o fio so acorda depois disso: lendo alvoBase direto ele
// achava a origem vazia e perguntava a TODOS os addons — MEDIDO na C9 em
// 18/09 (Debridio e AIOStreams consultados por canal do Meu Futebol com a
// origem definida). A busca copia no disparo; o que o app zere depois nao
// importa mais.
static char fioBase[600];
static int fioVivo;
static Stream *resultado;
static int nResultado;
static char pendId[64], pendTipo[16];
// O alvo corrente esta sendo buscado pelo PREFETCH do guia (fontecache.c), e
// nao por `fio`: addons_buscar o encontrou a caminho e resolveu esperar em vez
// de repetir. addons_estado e quem colhe. Ver addons_buscar.
static int adotado;
static void dispararBusca(void);

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
// Medido na TV do dono com a base cortada: o mesmo catalogo devolve 19.908
// bytes de metas com ou sem a query re-anexada, entao a query nao carrega
// configuracao e pode cair. Se um dia aparecer addon que precise dela no
// caminho de catalogo, e aqui que isso se decide.
static void baseNormalizada(const char *url, char *dst, size_t tam) {
  size_t k;
  char *q;
  snprintf(dst, tam, "%s", url);
  q = strchr(dst, '?');
  if (q) *q = 0;
  k = strlen(dst);
  if (k > 14 && !strcmp(dst + k - 14, "/manifest.json")) { k -= 14; dst[k] = 0; }
  // Barra final fora nos dois casos: "<base>//catalog" e uma URL diferente de
  // "<base>/catalog" para mais de um servidor.
  while (k && dst[k - 1] == '/') dst[--k] = 0;
}

// --- leitura do arquivo de configuracao -------------------------------------

int addons_carregar(const char *dirArte) {
  char caminho[600], linha[900];
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/addons.txt", dirArte ? dirArte : ".");
  f = fopen(caminho, "r");
  if (!f) { printf("[addons] sem %s\n", caminho); return 0; }
  nAddon = 0;
  while (nAddon < ADD_MAX && fgets(linha, sizeof linha, f)) {
    char *tab = strchr(linha, '\t');
    char *fim;
    // TAB e nao "|" como separador: nome de addon contem "|" de verdade
    // ("AIOStreams | ElfHosted") e partir no primeiro pipe corrompia a URL.
    if (!tab) continue;
    *tab = 0;
    fim = tab + 1 + strlen(tab + 1);
    while (fim > tab + 1 && (fim[-1] == '\n' || fim[-1] == '\r' || fim[-1] == ' ')) *--fim = 0;
    if (linha[0] == '#' || !tab[1]) continue;
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
    char base[600];
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
    addon[aceitos].canalLido = 0; addon[aceitos].nCanal = 0;
    addon[aceitos].ativo = nova[i].ativo ? 1 : 0;
    aceitos++;
  }
  if (aceitos == 0) {
    printf("[addons] a conta veio sem addons utilizaveis; mantendo a local\n");
    return 0;
  }
  nAddon = aceitos;
  printf("[addons] %d vindos da conta\n", nAddon);
  // DIZER QUANDO CORTOU. Um addon que some sem uma linha de log e indistinguivel
  // de um addon que a conta nao tem.
  { int uteis = 0, q;
    for (q = 0; q < n; q++) if (nova[q].url[0]) uteis++;
    if (uteis > aceitos)
      printf("[addons] %d da conta ficaram de fora: o app guarda no maximo %d\n",
             uteis - aceitos, ADD_MAX); }
  versaoLista++;
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
  versaoLista++;
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
  // Publica no fio da UI: nenhum desenho observa uma lista parcialmente escrita.
  if (fioVivo && e != ADD_BUSCANDO) {
    pthread_join(fio, NULL);
    fioVivo = 0;
    if (!pendId[0]) stream_definir_lista(resultado, nResultado);
    free(resultado); resultado = NULL; nResultado = 0;
    if (pendId[0]) {
      char id[64], tipo[16];
      snprintf(id, sizeof id, "%s", pendId);
      snprintf(tipo, sizeof tipo, "%s", pendTipo);
      pendId[0] = 0;
      addons_buscar(id, tipo);
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
static pthread_mutex_t legTrava = PTHREAD_MUTEX_INITIALIZER;

int addons_n_legendas(void) {
  int n;
  pthread_mutex_lock(&legTrava); n = nLegs; pthread_mutex_unlock(&legTrava);
  return n;
}
const Legenda *addons_legenda(int i) {
  const Legenda *r = NULL;
  pthread_mutex_lock(&legTrava);
  if (i >= 0 && i < nLegs) r = &legs[i];
  pthread_mutex_unlock(&legTrava);
  return r;
}

// Grupos de idioma da busca de legenda, NA ORDEM em que aparecem.
//
// O QUE ESTAVA AQUI: duas listas cravadas ("pob","pt-br",... e "eng","en",...)
// com o comentario "o usuario pediu explicitamente estes dois grupos". Toda
// legenda de outro idioma era descartada sem aviso — quem instala o pacote e
// fala espanhol abria o player e nao achava legenda nenhuma.
//
// AGORA: os grupos vem da preferencia (Ajustes desta TV, senao a conta). SEM
// preferencia nenhuma, ha UM grupo vazio, e grupo vazio casa com tudo: a lista
// sai sem filtro. Ver linguas.h.
static int gruposIdioma(const char *g[2]) {
  const char *a = ling_legenda(), *b = ling_legenda2();
  int n = 0;
  if (a[0] && strcasecmp(a, "none")) g[n++] = a;
  if (b[0] && strcasecmp(b, "none") && !ling_casa(b, a)) g[n++] = b;
  if (!n) { g[n++] = ""; }
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
  const char *p = strchr(id, ':');
  *temporada = *episodio = 0;
  if (p) sscanf(p + 1, "%d:%d", temporada, episodio);
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

static void *buscarLegendas(void *u) {
  (void)u;
  for (;;) {
    Legenda achadas[LEG_MAX] = {{0}};
    char id[64], tipo[16];
    unsigned geracao;
    int nAchadas = 0, temporada, episodio, i;

    pthread_mutex_lock(&legTrava);
    if (legParar) { fioLegVivo = 0; pthread_mutex_unlock(&legTrava); return NULL; }
    snprintf(id, sizeof id, "%s", legId);
    snprintf(tipo, sizeof tipo, "%s", legTipo);
    geracao = legGeracao;
    pthread_mutex_unlock(&legTrava);
    episodioPedido(id, &temporada, &episodio);

    for (i = 0; i < nAddon && nAchadas < LEG_MAX; i++) {
      char url[900], *corpo;
      const char *p;
    // Addon que nao declara legenda nao e consultado: o AIOStreams responderia
    // vazio e o Xperience tambem, dois round-trips sem retorno.
      if (!addon[i].ativo || !addon[i].legenda) continue;
      snprintf(url, sizeof url, "%s/subtitles/%s/%s.json",
               addon[i].base, tipo, id);
      corpo = rede_baixar(url, 25);
      if (pedidoMudou(geracao)) { free(corpo); break; }
      if (!corpo) continue;
      p = js_array(corpo, NULL, "subtitles");
      {
        const char *grupos[2];
        int nGrupos = gruposIdioma(grupos), gi;
        // Uma passada por grupo garante a ordem preferido -> alternativo e
        // evita que doze resultados do primeiro idioma consumam a lista inteira
        // antes do segundo. O teto por grupo e deliberado para navegacao por
        // D-pad — e vira a lista toda quando ha um grupo so.
        for (gi = 0; gi < nGrupos; gi++) {
          const char *grupo = grupos[gi];
          int teto = nGrupos > 1 ? LEG_MAX / 2 : LEG_MAX;
          const char *q = p;
          int noGrupo = 0, j;
          for (j = 0; j < nAchadas; j++)
            if (ling_casa(achadas[j].idioma, grupo)) noGrupo++;
          while (q && nAchadas < LEG_MAX && noGrupo < teto) {
            const char *f = js_fim(q);
            char l[16] = "", nome[120] = "";
            Legenda *d = &achadas[nAchadas];
            if (episodioCorreto(q, f, temporada, episodio) &&
                js_texto(q, f, "lang", l, sizeof l) && ling_casa(l, grupo) &&
                js_texto(q, f, "url", d->url, sizeof d->url)) {
              js_texto(q, f, "subtitleFileName", nome, sizeof nome);
              if (!nome[0]) js_texto(q, f, "movieReleaseName", nome, sizeof nome);
              snprintf(d->idioma, sizeof d->idioma, "%s", l);
              // i18n() no NOME DO IDIOMA tambem, e nao so no formato: o
              // formato traduzido nao traduz o que entra em %s — "Português"
              // continuava aparecendo dentro de "T1E1 · Português · arquivo"
              // com o app em ingles, porque so a moldura tinha chave.
              if (temporada > 0 && episodio > 0)
                snprintf(d->rotulo, sizeof d->rotulo, i18n("T%dE%d  \xc2\xb7  %s%s%.22s"),
                         temporada, episodio, i18n(ling_nome(l)), nome[0] ? "  \xc2\xb7  " : "", nome);
              else
                snprintf(d->rotulo, sizeof d->rotulo, "%s%s%.36s",
                         i18n(ling_nome(l)), nome[0] ? "  \xc2\xb7  " : "", nome);
              nAchadas++; noGrupo++;
            }
            q = js_prox(f);
          }
        }
      }
      free(corpo);
    }

    pthread_mutex_lock(&legTrava);
    if (legParar) { fioLegVivo = 0; pthread_mutex_unlock(&legTrava); return NULL; }
    if (geracao != legGeracao) { pthread_mutex_unlock(&legTrava); continue; }
    memcpy(legs, achadas, sizeof achadas);
    nLegs = nAchadas;
    fioLegVivo = 0;
    pthread_mutex_unlock(&legTrava);
    printf("[legendas] %s: %d\n", id, nAchadas);
    fflush(stdout);
    return NULL;
  }
}


// ------------------------------------------------------------ lista e sonda

int addons_ativo(int i)   { return (i >= 0 && i < nAddon) ? addon[i].ativo : 0; }
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
  return addon[i].legenda;
}
int addons_alternar(int i) {
  if (i < 0 || i >= nAddon) return 0;
  addon[i].ativo = !addon[i].ativo;
  versaoLista++;
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
  char nova[600];
  int i;
  if (!urlManifest || !*urlManifest) return 0;
  if (nAddon >= ADD_MAX) {
    printf("[addons] nao coube: a lista ja tem %d\n", ADD_MAX);
    fflush(stdout);
    return 0;
  }
  baseNormalizada(urlManifest, nova, sizeof nova);
  if (!nova[0]) return 0;
  for (i = 0; i < nAddon; i++) {
    char base[600];
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
  addon[nAddon].canalLido = 0; addon[nAddon].nCanal = 0;
  nAddon++;
  versaoLista++;
  printf("[addons] instalado pelo guia: %s (%s)\n",
         addon[nAddon - 1].nome, nova);
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

static void capacidadesDoManifesto(int i, const char *corpo) {
  const char *r = strstr(corpo, "\"resources\"");
  int cat = 0, str = 0, leg = 0;
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
      free(trecho); } }
  addon[i].catalogo = cat;
  addon[i].fonte    = str;
  addon[i].legenda  = leg;
  addon[i].sondado  = 1;
  printf("[addons] %s: catalogo=%d stream=%d legenda=%d\n",
         addon[i].nome, cat, str, leg);
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
    char url[700], *corpo;
    if (addon[i].sondado) continue;
    snprintf(url, sizeof url, "%s/manifest.json", addon[i].base);
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

void addons_buscar_legendas(const char *imdb, const char *tipo) {
  int serie, juntar = 0;
  char id[64], tp[16];
  if (!nAddon || !imdb || !*imdb) return;
  serie = tipo && !strcmp(tipo, "series");
  if (serie && !strchr(imdb, ':'))
    snprintf(id, sizeof id, "%s:1:1", imdb);
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
  nLegs = 0;
  if (fioLegVivo) { pthread_mutex_unlock(&legTrava); return; }
  juntar = fioLegCriado;
  pthread_mutex_unlock(&legTrava);

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
#define ADD_FIOS 4
#endif

typedef struct {
  int    idx;                 // qual addon
  Stream *achados;
  int    n;
  int    respondeu;           // 1 = veio corpo (mesmo com 0 fontes)
} BaldeFonte;

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

static void *fioFontes(void *u) {
  Consulta *c = u;
  for (;;) {
    int meu, i, n;
    char url[900], *corpo;
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
    // O NOME QUE O MANIFESTO DECLARA VAI PRIMEIRO (issue #112). Ver
    // tipoCanalDeclarado: o FrostView declara "channel" e o app perguntava
    // "tv" antes, gastando uma viagem inteira por canal aberto so para
    // receber a lista vazia que agora dispara o segundo nome logo abaixo.
    { const char *dec = c->tipoAlt && c->tipoAlt[0] ? tipoCanalDeclarado(i) : NULL;
      t1 = c->tipo; t2 = c->tipoAlt;
      if (dec && !strcmp(dec, c->tipoAlt)) { t1 = c->tipoAlt; t2 = c->tipo; } }
    snprintf(url, sizeof url, "%s/stream/%s/%s.json", addon[i].base, t1, c->id);
    // 12 s e nao 25: com os addons em paralelo o timeout deixa de ser somado,
    // mas continua sendo o tempo que o dono espera pelo mais lento.
    corpo = rede_baixar(url, 12);
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
      snprintf(url, sizeof url, "%s/stream/%s/%s.json", addon[i].base, t2, c->id);
      alt = rede_baixar(url, 12);
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
    if (!corpo) { free(achados); printf("[addons] %s: sem resposta\n", addon[i].nome); continue; }
    c->baldes[meu].respondeu = 1;
    c->baldes[meu].n = n;
    c->baldes[meu].achados = achados;
    printf("[addons] %s: %d fontes (%u bytes)\n",
           addon[i].nome, c->baldes[meu].n, (unsigned)strlen(corpo));
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

// O segundo nome de tipo de canal ao vivo; "" para os demais. Ver fioFontes.
static const char *tipoAlternativo(const char *tipo) {
  if (!strcmp(tipo, "tv"))      return "channel";
  if (!strcmp(tipo, "channel")) return "tv";
  return "";
}

static int consultar(const char *id, const char *tipo, const char *base, int fios,
                     int (*cancelado)(void *), void *ctx, Stream **saida,
                     Resumo *rs) {
  Consulta c;
  Stream *achados = NULL;
  int n = 0, i, q;
  *saida = NULL;
  if (!id || !*id || !tipo || !*tipo || nAddon <= 0) return 0;
  memset(&c, 0, sizeof c);
  c.id = id; c.tipo = tipo; c.tipoAlt = tipoAlternativo(tipo);
  c.cancelado = cancelado; c.ctx = ctx;
  pthread_mutex_init(&c.trava, NULL);
  c.baldes = calloc((size_t)nAddon, sizeof(BaldeFonte));
  if (c.baldes) {
    // SO O ADDON DE ORIGEM, quando se sabe qual e. Comparacao por base
    // normalizada — "<base>" e "<base>/manifest.json" sao o mesmo addon.
    if (base && *base) {
      char alvo[600], mine[600];
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

  if (c.baldes && c.nBaldes > 0) {
    pthread_t f[ADD_FIOS];
    int criados = 0;
    if (fios > ADD_FIOS) fios = ADD_FIOS;
    for (q = 0; q < fios && q < c.nBaldes; q++)
      if (pthread_create(&f[criados], NULL, fioFontes, &c) == 0) criados++;
    if (!criados) fioFontes(&c);          // sem fios: em serie, mesmo resultado
    for (q = 0; q < criados; q++) pthread_join(f[q], NULL);
    // Junta NA ORDEM DOS ADDONS, que e a ordem em que o dono os instalou.
    for (q = 0; q < c.nBaldes; q++) {
      int k = c.baldes[q].n;
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
  // Cancelada, a lista pode estar pela metade: nao e resposta, e lixo.
  if (cancelado && cancelado(ctx)) { free(achados); return -1; }
  *saida = achados;
  return n;
}

int addons_consultar(const char *id, const char *tipo, const char *base, int fios,
                     int (*cancelado)(void *), void *ctx, Stream **saida) {
  return consultar(id, tipo, base, fios, cancelado, ctx, saida, NULL);
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
  n = consultar(alvoId, alvoTipo, fioBase, ADD_FIOS, NULL, NULL, &achados, &rs);
  if (n < 0) n = 0;
  resumo = rs;
  marco(n ? "addons: fontes recebidas" : "addons: nenhuma fonte");
  // O canal que vai ao ar fica no cache para o zap de VOLTA. Filme e serie
  // nao entram (fontecache_guardar decide pelo tipo).
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
  snprintf(fioBase, sizeof fioBase, "%s", alvoBase);
  alvoBase[0] = 0;   // consumida: origem e do pedido, nao de sessao
  if (pthread_create(&fio, NULL, buscar, NULL) != 0) { fioVivo = 0; estado = ADD_PARADO; }
}

// Diz de que addon o PROXIMO alvo veio. Chamar ANTES de addons_buscar; a
// busca seguinte consome e zera — origem e do pedido, nao de sessao.
void addons_definir_origem(const char *base) {
  snprintf(alvoBase, sizeof alvoBase, "%s", base ? base : "");
}

void addons_buscar(const char *imdb, const char *tipo) {
  int serie;
  if (!imdb || !*imdb) return;
  resumo.valido = 0;
  // Recusa de conta do debrid vale por busca: a nova volta a tentar todos.
  debrid_nova_busca();
  if (!nAddon) { stream_definir_lista(NULL, 0); resumoDaLista(&resumo); estado = ADD_VAZIO; return; }
  if (fioVivo) {
    if (strcmp(imdb, alvoId) || strcmp(tipo ? tipo : "movie", alvoTipo)) {
      snprintf(pendId, sizeof pendId, "%s", imdb);
      snprintf(pendTipo, sizeof pendTipo, "%s", tipo ? tipo : "movie");
    }
    return;
  }
  // Um pedido novo desfaz a espera pelo prefetch do anterior; o prefetch em si
  // segue ou cede conforme o que vem abaixo.
  adotado = 0;
  stream_definir_lista(NULL, 0);
  serie = tipo && !strcmp(tipo, "series");
  // Serie SEM episodio devolve lista vazia, com HTTP 200 e sem erro nenhum
  // (medido: 14 bytes de resposta). O identificador tem de ser
  // "tt1234567:temporada:episodio". Como o catalogo ainda nao traz lista de
  // episodios, assume T1E1 — e o mesmo lugar onde o episodio real entra quando
  // houver.
  if (serie && !strchr(imdb, ':'))
    snprintf(alvoId, sizeof alvoId, "%s:1:1", imdb);
  else
    snprintf(alvoId, sizeof alvoId, "%s", imdb);
  snprintf(alvoTipo, sizeof alvoTipo, "%s", tipo && *tipo ? tipo : "movie");
  { int t = 0, e = 0; const char *dp = strchr(alvoId, ':');
    if (dp) sscanf(dp + 1, "%d:%d", &t, &e);
    debrid_definir_episodio(t, e); }
  // O CACHE ANTES DA REDE. Canal que o guia engatilhou (ou que acabou de sair
  // do ar) responde daqui, sem fio nenhum; canal cujo prefetch esta na rede
  // AGORA e adotado — esperar o que ja esta a caminho e mais curto que repetir
  // as mesmas requisicoes, e addons_estado publica quando chegar.
  { Stream *l; int n;
    int r = fontecache_pegar(alvoId, alvoTipo, &l, &n);
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

void addons_encerrar(void) {
  int juntarLeg;
  fontecache_encerrar();
  adotado = 0;
  if (fioVivo) pthread_join(fio, NULL);
  fioVivo = 0;
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
