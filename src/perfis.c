void servidores_perfil_trocou(void);   // servidores.c: cancel in-flight work, load this profile
#include "perfis.h"
#include "sessao.h"
#include "nuvem.h"
#include "dados.h"
#include "fileiras.h"   /* fil_definir_perfil: a escolha de fileiras e por perfil */
// fontepref_definir_perfil DECLARADA A MAO, e nao por #include: fontepref.h
// puxa streams.h, que puxa SDL, e tests/perfilsel.sh compila perfis.c SOZINHO
// (com js.c e jsw.c) exatamente para provar que a regra de perfil nao depende
// de SDL nem de rede. Um include aqui quebraria esse teste sem que nada nesta
// tela tivesse mudado. A fonte lembrada e por perfil pela mesma razao que as
// fileiras sao — ver fontepref.h.
void fontepref_definir_perfil(int perfil);
// Mesma razao: a legenda escolhida a mao (legmemoria.h, 2.0.3) e por perfil.
void legmem_definir_perfil(int perfil);
void audiomem_definir_perfil(int perfil);
// Mesma razao (arteescolha.h nao puxa SDL, mas fica no mesmo molde).
void arteesc_definir_perfil(int perfil);
// Mesma razao: ajustes.h puxa SDL. O ajuste "Usar os addons do perfil
// principal" (Ajustes > Conta), ver perfis_ativo_addons.
int ajustes_addons_do_principal(void);
#include "js.h"
#include "jsw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

static _Atomic uint64_t sessaoGeracao = 1;
static pthread_mutex_t sessaoTrava;
static pthread_once_t sessaoUmaVez = PTHREAD_ONCE_INIT;
static void (*sessaoAntes)(void);
static void sessaoTravaIniciar(void) {
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&sessaoTrava, &attr);
  pthread_mutexattr_destroy(&attr);
}
uint64_t perfis_sessao_geracao(void) {
  return atomic_load_explicit(&sessaoGeracao, memory_order_acquire);
}
int perfis_sessao_atual(uint64_t geracao) { return geracao == perfis_sessao_geracao(); }
int perfis_sessao_travar(uint64_t geracao) {
  pthread_once(&sessaoUmaVez, sessaoTravaIniciar);
  pthread_mutex_lock(&sessaoTrava);
  if (perfis_sessao_atual(geracao)) return 1;
  pthread_mutex_unlock(&sessaoTrava);
  return 0;
}
void perfis_sessao_destravar(void) { pthread_mutex_unlock(&sessaoTrava); }
void perfis_ao_abrir_sessao(void (*antes)(void)) { sessaoAntes = antes; }
static void sessaoAvancar(void) {
  uint64_t g = atomic_fetch_add_explicit(&sessaoGeracao, 1, memory_order_acq_rel) + 1;
  if (!g) atomic_fetch_add_explicit(&sessaoGeracao, 1, memory_order_acq_rel);
}
void perfis_invalidar_sessao(void) {
  pthread_once(&sessaoUmaVez, sessaoTravaIniciar);
  pthread_mutex_lock(&sessaoTrava);
  sessaoAvancar();
  pthread_mutex_unlock(&sessaoTrava);
}

#define ARQ_ATIVO "perfil.txt"
#define ARQ_LISTA "perfis.txt"

static ContaPerfil lista[CONTA_PERFIL_MAX];
static int n;
static char dono[64];
static atomic_int ativo = ATOMIC_VAR_INIT(1);
// ESCOLHIDO E DE SESSAO, GRAVADO E DE DISCO — e a diferenca entre as duas e a
// mudanca de comportamento inteira desta tela.
//
// Antes so existia `escolhido`, e perfis_carregar_ativo() o ligava ao ler
// perfil.txt: a tela aparecia UMA VEZ POR INSTALACAO e nunca mais. Numa TV de
// sala isso significa que quem liga o aparelho herda em silencio o perfil de
// quem o desligou — e como `p_profile_id` vai em quase toda RPC, o app passa a
// ESCREVER progresso no perfil da outra pessoa sem nunca ter perguntado.
//
// Agora `escolhido` zera a cada arranque (a pergunta vale uma vez por sessao) e
// `gravado` guarda que existe uma resposta de ontem — que e o que permite ao
// Voltar dispensar a tela sem escolher nada, e o que faz o cursor nascer no
// perfil certo.
static int escolhido;      // 1 depois que o usuario decidiu NESTA sessao
static int gravado;        // 1 quando perfil.txt existia no arranque

// Existing isolated fixtures omit sessao.c. A complete app always captures
// both fields locally, without refreshing or doing HTTP on the UI thread.
#if defined(__APPLE__) && (defined(__GNUC__) || defined(__clang__))
// This producer must keep its epoch definitions strong. Import only the two
// optional session helpers; Mach-O needs weak_import for an absent provider.
extern int sessao_identidade_local_copiar(char *, size_t, char *, size_t) __attribute__((weak_import));
extern void sessao_token_recusado(const char *) __attribute__((weak_import));
#elif defined(__GNUC__) || defined(__clang__)
#pragma weak sessao_identidade_local_copiar
#pragma weak sessao_token_recusado
#endif
typedef struct {
  uint64_t sessao;
  char usuario[80], jwt[3000];
  int legado;
} PedidoPerfis;
static int pedidoPerfisAtual(const PedidoPerfis *p) {
  return perfis_sessao_atual(p->sessao) && !strcmp(p->usuario, sessao_usuario());
}
static char *rpcPerfis(PedidoPerfis *p, const char *funcao, int *st) {
  char *r;
  int status = 0;
  if (st) *st = 0;
  if (!pedidoPerfisAtual(p) || (!p->legado && !p->jwt[0])) return NULL;
  r = p->legado ? sessao_rpc(funcao, "{}", &status)
                : nuvem_rpc_com(funcao, "{}", p->jwt, &status);
  if (!p->legado && status == 401 && pedidoPerfisAtual(p) &&
      sessao_identidade_local_copiar && sessao_token_recusado) {
    char usuario[80], jwt[3000];
    sessao_token_recusado(p->jwt);
    if (sessao_identidade_local_copiar(usuario, sizeof usuario, jwt, sizeof jwt) &&
        !strcmp(usuario, p->usuario) && pedidoPerfisAtual(p)) {
      snprintf(p->jwt, sizeof p->jwt, "%s", jwt);
      free(r); r = nuvem_rpc_com(funcao, "{}", p->jwt, &status);
    }
    memset(jwt, 0, sizeof jwt);
  }
  if (!pedidoPerfisAtual(p)) { free(r); return NULL; }
  if (st) *st = status;
  return r;
}
static void lerDono(PedidoPerfis *pedido, char destino[64]) {
  char *r;
  int st = 0;
  if (destino[0]) return;
  r = rpcPerfis(pedido, "get_sync_owner", &st);
  if (r && st >= 200 && st < 300) {
    // get_sync_owner returns a JSON string, rather than an object.
    const char *a = strchr(r, '"');
    const char *b = a ? strchr(a + 1, '"') : NULL;
    if (a && b && b > a + 1 && (size_t)(b - a - 1) < 64) {
      memcpy(destino, a + 1, (size_t)(b - a - 1));
      destino[b - a - 1] = 0;
    }
  }
  free(r);
  if (!destino[0]) {
    // The fallback belongs to the request that failed, never the current login.
    snprintf(destino, 64, "%.63s", pedido->usuario);
    printf("[perfis] get_sync_owner nao respondeu; usando o sub do token\n");
  }
}

// --- CACHE EM DISCO DA LISTA -------------------------------------------------
//
// Uma linha por perfil, campos separados por TAB:
//
//   indice \t temPin \t primario \t usaAddons \t corHex \t nome \t avatar \t fundo
//
// Formato de linha e nao JSON de proposito: e o mesmo estilo de perfil.txt e
// progresso.txt, nao precisa do leitor de JSON no caminho do arranque, e um
// arquivo truncado pela metade custa UMA linha, nao o arquivo inteiro.
//
// O TAB e o separador porque nenhum dos campos pode conte-lo: a gravacao troca
// tab e quebra de linha por espaco antes de escrever. Sem isso um nome de
// perfil com um tab dentro deslocaria todos os campos seguintes.
static void limpo(char *dst, size_t tam, const char *src) {
  size_t i = 0;
  if (!tam) return;
  for (; src && src[i] && i + 1 < tam; i++)
    dst[i] = (src[i] == '\t' || src[i] == '\n' || src[i] == '\r') ? ' ' : src[i];
  dst[i] = 0;
}

static void gravarCache(void) {
  // 8 perfis x (2 URLs de 300 + nome + cor + quatro numeros) cabe com folga.
  char buf[8192];
  size_t p = 0;
  int i;
  if (n <= 0) return;
  for (i = 0; i < n && p < sizeof buf; i++) {
    char nome[64], av[300], fu[300], cor[10];
    limpo(nome, sizeof nome, lista[i].nome);
    limpo(av, sizeof av, lista[i].avatarUrl);
    limpo(fu, sizeof fu, lista[i].fundoUrl);
    limpo(cor, sizeof cor, lista[i].corHex);
    p += (size_t)snprintf(buf + p, sizeof buf - p, "%d\t%d\t%d\t%d\t%s\t%s\t%s\t%s\n",
                          lista[i].indice, lista[i].temPin, lista[i].primario,
                          lista[i].usaAddonsDoPrimario, cor, nome, av, fu);
  }
  if (p >= sizeof buf) return;   // nao gravar um arquivo truncado
  dados_gravar(ARQ_LISTA, buf);
}

// Le um campo ate o proximo TAB ou fim de linha e avanca `*p`.
static void campo(const char **p, char *dst, size_t tam) {
  const char *s = *p;
  size_t i = 0;
  while (*s && *s != '\t' && *s != '\n') {
    if (i + 1 < tam) dst[i++] = *s;
    s++;
  }
  if (tam) dst[i] = 0;
  if (*s == '\t') s++;
  *p = s;
}

static void lerCache(void) {
  char *b = dados_ler(ARQ_LISTA);
  const char *p;
  int novos = 0;
  if (!b) return;
  memset(lista, 0, sizeof lista);
  for (p = b; *p && novos < CONTA_PERFIL_MAX; ) {
    char num[16];
    ContaPerfil *d = &lista[novos];
    campo(&p, num, sizeof num); d->indice = atoi(num);
    campo(&p, num, sizeof num); d->temPin = atoi(num);
    campo(&p, num, sizeof num); d->primario = atoi(num);
    campo(&p, num, sizeof num); d->usaAddonsDoPrimario = atoi(num);
    campo(&p, d->corHex, sizeof d->corHex);
    campo(&p, d->nome, sizeof d->nome);
    campo(&p, d->avatarUrl, sizeof d->avatarUrl);
    campo(&p, d->fundoUrl, sizeof d->fundoUrl);
    while (*p == '\n' || *p == '\r') p++;
    // Linha sem indice e lixo (arquivo de outra versao, escrita interrompida):
    // pular uma linha e melhor que desistir do arquivo inteiro.
    if (d->indice > 0) novos++;
    else memset(d, 0, sizeof *d);
  }
  free(b);
  if (novos > 0) {
    n = novos;
    printf("[perfis] %d perfil(is) do cache: a tela de escolha ja pode abrir\n", n);
  }
}

// --- CATALOGO DE AVATARES OFICIAIS -------------------------------------------
//
// get_avatar_catalog devolve 42 linhas com `id` e `storage_path`
// ("avatar_lalo" -> "animals/bram-v1.png"), e a imagem publica mora em
// <url>/storage/v1/object/public/avatars/<storage_path>. MEDIDO: a RPC responde
// com a chave ANONIMA, sem token de usuario — o catalogo e o mesmo para todo
// mundo, entao nao ha o que autorizar.
//
// Buscado UMA VEZ por sessao e guardado inteiro: sao ~4 KB de JSON, e pedir por
// perfil faria a mesma chamada ate oito vezes no arranque.
#define AV_MAX 64
typedef struct { char id[64], caminho[128]; } AvatarCat;
static AvatarCat avCat[AV_MAX];
static int nAvCat, avCatTentado;

static void avatarCatalogoCarregar(void) {
  char *r;
  int st = 0;
  const char *p;
  if (avCatTentado) return;
  avCatTentado = 1;
  r = nuvem_rpc_com("get_avatar_catalog", "{}", NULL, &st);
  if (!r || st < 200 || st >= 300) {
    if (st) printf("[perfis] catalogo de avatares: HTTP %d\n", st);
    free(r);
    return;
  }
  for (p = js_raiz_array(r); p && nAvCat < AV_MAX; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    AvatarCat *a = &avCat[nAvCat];
    a->id[0] = a->caminho[0] = 0;
    js_texto(p, f, "id", a->id, sizeof a->id);
    js_texto(p, f, "storage_path", a->caminho, sizeof a->caminho);
    if (a->id[0] && a->caminho[0]) nAvCat++;
  }
  free(r);
  printf("[perfis] %d avatares oficiais no catalogo\n", nAvCat);
  fflush(stdout);
}

// Monta a URL publica do avatar `id`. Devolve 1 quando achou.
static int avatarOficial(const char *id, char *dst, size_t tam) {
  int i;
  if (!id || !id[0] || !dst || !tam) return 0;
  avatarCatalogoCarregar();
  for (i = 0; i < nAvCat; i++) {
    if (strcmp(avCat[i].id, id)) continue;
    snprintf(dst, tam, "%s/storage/v1/object/public/avatars/%s",
             nuvem_url(), avCat[i].caminho);
    return 1;
  }
  return 0;
}

static int puxarPerfis(PedidoPerfis *pedido) {
  char *r;
  int st = 0;
  const char *p;
  ContaPerfil tmp[CONTA_PERFIL_MAX];
  char novoDono[64];
  int novos, publicarCache = 0;
  if (!perfis_sessao_travar(pedido->sessao)) return 0;
  if (!pedidoPerfisAtual(pedido)) { perfis_sessao_destravar(); return 0; }
  memcpy(tmp, lista, sizeof tmp); novos = n;
  snprintf(novoDono, sizeof novoDono, "%s", dono);
  perfis_sessao_destravar();

  lerDono(pedido, novoDono);
  r = rpcPerfis(pedido, "sync_pull_profiles", &st);
  if (!r || st < 200 || st >= 300) { free(r); goto publicar; }

  // Lista vazia NAO apaga o que ja esta em memoria: e a mesma regra que o app
  // web aplica em toda superficie. Resposta vazia pode ser perfil errado, 401
  // mal tratado ou servidor fora do ar, e nenhum desses e "o usuario apagou os
  // perfis".
  { int recebidos = 0;
    ContaPerfil recebida[CONTA_PERFIL_MAX];
    memset(recebida, 0, sizeof recebida);
    for (p = js_raiz_array(r); p && recebidos < CONTA_PERFIL_MAX; p = js_prox(js_fim(p))) {
      const char *f = js_fim(p);
      double idx = js_num(p, f, "profile_index", 0);
      if (idx <= 0) idx = js_num(p, f, "id", 0);
      if (idx <= 0) continue;
      recebida[recebidos].indice = (int)idx;
      // "ContaPerfil %d" ficou aqui quando a struct Perfil virou ContaPerfil
      // para nao colidir com o trakt.h legado. O tipo mudou de nome; o que o
      // usuario le, nao.
      if (!js_texto(p, f, "name", recebida[recebidos].nome, sizeof recebida[recebidos].nome))
        snprintf(recebida[recebidos].nome, sizeof recebida[recebidos].nome, "Perfil %d", (int)idx);
      js_texto(p, f, "avatar_url", recebida[recebidos].avatarUrl, sizeof recebida[recebidos].avatarUrl);
      // O AVATAR OFICIAL VEM POR ID, e nao por URL. `avatar_url` so e
      // preenchido para foto que a pessoa subiu; quem escolhe um dos avatares
      // do Nuvio guarda `avatar_id` ("avatar_lalo") e a imagem sai do catalogo.
      // Ver avatarOficial: e uma RPC, get_avatar_catalog, e nao a tabela
      // `avatars` — foi por procurar a tabela que uma medicao antiga concluiu
      // que nao havia como resolver o id, e os perfis ficaram todos com a
      // inicial num circulo colorido.
      if (!recebida[recebidos].avatarUrl[0]) {
        char aid[64] = "";
        if (js_texto(p, f, "avatar_id", aid, sizeof aid) && aid[0] &&
            !avatarOficial(aid, recebida[recebidos].avatarUrl, sizeof recebida[recebidos].avatarUrl))
          // O perfil PEDE um avatar que o catalogo nao tem. Sem esta linha o
          // circulo cai na inicial e nada diz por que — indistinguivel de um
          // perfil que nunca escolheu avatar nenhum.
          printf("[perfis] avatar \"%s\" nao esta no catalogo\n", aid);
      }
      js_texto(p, f, "profile_background_url", recebida[recebidos].fundoUrl,
               sizeof recebida[recebidos].fundoUrl);
      if (!js_texto(p, f, "avatar_color_hex", recebida[recebidos].corHex, sizeof recebida[recebidos].corHex))
        snprintf(recebida[recebidos].corHex, sizeof recebida[recebidos].corHex, "#1E88E5");
      { char b[16];
        // Sem o campo, o perfil 1 e o primario — e a mesma regra do web.
        recebida[recebidos].primario = js_bruto(p, f, "is_primary", b, sizeof b)
                              ? (strcmp(b, "true") == 0) : ((int)idx == 1); }
      { char b[16];
        // `uses_primary_addons`, NAO `uses_primary_plugins`. Sao DUAS colunas
        // diferentes no servidor e este campo decide de qual perfil vem a lista
        // de ADDONS; a de plugins nao existe aqui. Relato do Mane155 que pegou
        // isto: o perfil 2 dele mostrava "0 addons" no nativo e 3 no app web.
        // Com uses_primary_addons=true e uses_primary_plugins=false, o web
        // resolvia para o perfil 1 (e achava as 3) enquanto o nativo pedia pelo
        // perfil 2, que nao tem linha nenhuma na tabela `addons` — resposta
        // vazia, sem erro, e a tela de ajustes dizendo zero.
        // O nome antigo fica como reserva: linha gravada por uma versao que so
        // conhecia aquela coluna continua sendo lida.
        recebida[recebidos].usaAddonsDoPrimario =
          js_bruto(p, f, "uses_primary_addons", b, sizeof b)
          ? (strcmp(b, "true") == 0)
          : (js_bruto(p, f, "uses_primary_plugins", b, sizeof b)
             ? (strcmp(b, "true") == 0) : 0); }
      recebidos++;
    }
    if (recebidos > 0) { memcpy(tmp, recebida, sizeof tmp); novos = recebidos; }
  }
  free(r);

  // Travas: um perfil com PIN nao pode ser aberto so por estar na lista.
  r = rpcPerfis(pedido, "sync_pull_profile_locks", &st);
  if (r && st >= 200 && st < 300) {
    for (p = js_raiz_array(r); p; p = js_prox(js_fim(p))) {
      const char *f = js_fim(p);
      int idx = (int)js_num(p, f, "profile_id", 0);
      char b[16];
      int i, travado;
      if (!idx) idx = (int)js_num(p, f, "profile_index", 0);
      // MEDIDO: esta RPC devolve UMA LINHA POR PERFIL, com `pin_enabled` false
      // quando nao ha PIN — nao e uma lista so dos travados. Marcar todo perfil
      // que aparece aqui trancava TODOS eles, e como nenhum tem PIN nenhuma
      // digitacao seria aceita: ninguem entraria na propria conta.
      travado = js_bruto(p, f, "pin_enabled", b, sizeof b)
                ? (strcmp(b, "true") == 0) : 0;
      for (i = 0; i < novos; i++)
        if (tmp[i].indice == idx) tmp[i].temPin = travado;
    }
  }
  free(r);

  publicarCache = 1;
publicar:
  if (!perfis_sessao_travar(pedido->sessao)) return 0;
  if (!pedidoPerfisAtual(pedido)) { perfis_sessao_destravar(); return 0; }
  memcpy(lista, tmp, sizeof lista); n = novos;
  snprintf(dono, sizeof dono, "%s", novoDono);
  if (publicarCache) gravarCache();
  printf("[perfis] %d perfil(is), dono=%s, ativo=%d\n", n, dono, ativo);
  int quantidade = n;
  perfis_sessao_destravar();
  return quantidade;
}

int perfis_puxar_sessao(uint64_t sessao, const char *usuario, const char *jwt) {
  PedidoPerfis pedido = { .sessao = sessao };
  if (!usuario || !*usuario || !jwt || !*jwt ||
      strlen(usuario) >= sizeof pedido.usuario || strlen(jwt) >= sizeof pedido.jwt) return 0;
  snprintf(pedido.usuario, sizeof pedido.usuario, "%s", usuario);
  snprintf(pedido.jwt, sizeof pedido.jwt, "%s", jwt);
  int quantidade = puxarPerfis(&pedido);
  memset(pedido.jwt, 0, sizeof pedido.jwt);
  return quantidade;
}
int perfis_puxar(void) {
  PedidoPerfis pedido = { .sessao = perfis_sessao_geracao() };
  if (sessao_identidade_local_copiar) {
    if (!sessao_identidade_local_copiar(pedido.usuario, sizeof pedido.usuario,
                                      pedido.jwt, sizeof pedido.jwt)) return 0;
  } else {
    // Fixtures that omit sessao.c provide their original synchronous RPC fake.
    pedido.legado = 1;
    snprintf(pedido.usuario, sizeof pedido.usuario, "%s", sessao_usuario());
  }
  int quantidade = puxarPerfis(&pedido);
  memset(pedido.jwt, 0, sizeof pedido.jwt);
  return quantidade;
}

#ifdef AJUSTES_TESTE
// Capturas de Ajustes: uma lista de perfis de mentira, sem conta nem rede.
void perfis_teste_definir(const char *const *nomes, const int *pin, int quantos, int ativo_) {
  int i;
  n = quantos > CONTA_PERFIL_MAX ? CONTA_PERFIL_MAX : quantos;
  memset(lista, 0, sizeof lista);
  for (i = 0; i < n; i++) {
    lista[i].indice = i + 1;
    snprintf(lista[i].nome, sizeof lista[i].nome, "%s", nomes[i]);
    lista[i].primario = i == 0;
    lista[i].temPin = pin ? pin[i] : 0;
  }
  ativo = ativo_;
}
#endif
int           perfis_n(void)         { return n; }
const ContaPerfil *perfis_item(int i)     { return (i >= 0 && i < n) ? &lista[i] : NULL; }

const ContaPerfil *perfis_item_ativo(void) {
  int i;
  for (i = 0; i < n; i++) if (lista[i].indice == ativo) return &lista[i];
  return NULL;
}
const char   *perfis_dono(void)      { return dono; }
int           perfis_ativo(void)     { int p = atomic_load_explicit(&ativo, memory_order_acquire); return p > 0 ? p : 1; }
int           perfis_principal(void) {
  int i;
  for (i = 0; i < n; i++) if (lista[i].primario) return lista[i].indice;
  return 1;
}

// Addons NAO sao por perfil quando o perfil diz herdar os do primario.
//
// MEDIDO no app web (js/data/local/pluginStore.js:26):
//   return profile?.usesPrimaryPlugins && normalized !== "1" ? "1" : normalized;
// O nativo ignorava isso e pedia sempre profile_id=<indice>, entao um perfil
// com a marca voltava com ZERO addons — o relato "os perfis nao sincronizam os
// addons". O perfil 1 nunca e redirecionado: ele E a origem.
//
// O AJUSTE LOCAL "Usar os addons do perfil principal" (padrao LIGADO) soma com a
// marca da conta, nao a substitui. Sem o campo no servidor a marca ficava 0 e o
// perfil 2 abria sem addon nenhum; com o ajuste ligado ele le os do principal
// mesmo assim. Desligado, vale so o que a conta diz: `uses_primary_addons=true`
// continua sendo respeitado, porque foi a pessoa que marcou isso no app web, e
// um ajuste desta TV nao deve desfazer a escolha da conta.
int perfis_ativo_addons(void) {
  const ContaPerfil *p = perfis_item_ativo();
  int a = perfis_ativo();
  if (a == 1) return 1;
  return ((p && p->usaAddonsDoPrimario) || ajustes_addons_do_principal()) ? 1 : a;
}

void perfis_carregar_ativo(void) {
  char *b = dados_ler(ARQ_ATIVO);
  if (b) {
    int v = atoi(b);
    // `gravado`, e NAO `escolhido`: ler o arquivo prova que alguem escolheu
    // ontem, nao que quem esta na frente da TV agora e a mesma pessoa.
    if (v > 0) { ativo = v; gravado = 1; }
    free(b);
  }
  // A escolha de fileiras acompanha o perfil desde o arranque, senao a home
  // do perfil 1 e montada com o arquivo do perfil 2 ate a primeira troca.
  fil_definir_perfil(ativo);
  // A FONTE LEMBRADA ACOMPANHA O PERFIL desde o arranque, pela mesma razao das
  // fileiras: sem isto o perfil 2 retomaria na fonte dublada que o perfil 1
  // escolheu, ate a primeira troca.
  fontepref_definir_perfil(ativo);
  legmem_definir_perfil(ativo);
  audiomem_definir_perfil(ativo);
  arteesc_definir_perfil(ativo);
  lerCache();
}

// plugins.h puxa streams.h (SDL); perfis.c compila sem SDL nos testes.
void plugins_perfil_mudou(void);

void perfis_definir_ativo(int indice) {
  char linha[32];
  if (indice <= 0) return;
  if (sessaoAntes) sessaoAntes();
  pthread_once(&sessaoUmaVez, sessaoTravaIniciar);
  pthread_mutex_lock(&sessaoTrava);
  sessaoAvancar();
  atomic_store_explicit(&ativo, indice, memory_order_release);
  pthread_mutex_unlock(&sessaoTrava);
  fil_definir_perfil(indice);
  fontepref_definir_perfil(indice);
  legmem_definir_perfil(indice);
  audiomem_definir_perfil(indice);
  arteesc_definir_perfil(indice);
  // O estado dos plugins e da conta+perfil: trocar avanca a geracao, corta os
  // scrapers em voo e relê o liga/desliga e os repositorios do perfil novo.
  plugins_perfil_mudou();
  // Personal servers are per profile: drop the previous profile's in-flight
  // requests and load this profile's connection (jellyfin.h).
  servidores_perfil_trocou();
  escolhido = 1;
  gravado = 1;
  snprintf(linha, sizeof linha, "%d\n", indice);
  dados_gravar(ARQ_ATIVO, linha);
  printf("[perfis] perfil ativo: %d\n", indice);
}

void perfis_manter_ativo(void) { escolhido = 1; }

int perfis_sem_escolha(void) {
  if (n <= 0) return 1;
  if (n == 1) return !lista[0].temPin;
  return 0;
}

int perfis_precisa_escolher(void) {
  return !escolhido && !perfis_sem_escolha();
}

int perfis_pode_dispensar(void) {
  const ContaPerfil *p;
  if (escolhido) return 1;
  if (!gravado) return 0;
  p = perfis_item_ativo();
  // Sem o perfil na lista nao da para saber se ele esta travado; recusar e a
  // resposta segura. Com ele, o Voltar so vale quando nao ha PIN — senao a
  // tecla de voltar seria a chave da fechadura.
  return p && !p->temPin;
}

int perfis_indice_sugerido(void) {
  int i;
  for (i = 0; i < n; i++) if (lista[i].indice == ativo) return i;
  return 0;
}

PerfilAcao perfis_acao(int i) {
  if (i < 0 || i >= n) return PERFIL_ACAO_NADA;
  return lista[i].temPin ? PERFIL_ACAO_PIN : PERFIL_ACAO_ENTRAR;
}

int perfis_verificar_pin(int indice, const char *pin) {
  Jsw w;
  char *r;
  int st = 0, ok = 0;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_ci(&w, "p_profile_id", indice);
  jsw_cs(&w, "p_pin", pin ? pin : "");
  jsw_obj_fim(&w);
  r = sessao_rpc("verify_profile_pin", jsw_texto_final(&w), &st);
  jsw_livre(&w);
  // TRES respostas e nao duas. "Nao consegui perguntar" nao e "voce errou": com
  // dois valores, uma TV sem rede acusava a pessoa de errar o PIN que ela
  // digitou certo — e a tela ate tinha o aviso de conexao, so que inalcancavel.
  if (!r || st < 200 || st >= 300) { free(r); return -1; }
  // A RPC devolve um booleano; aceitar so o HTTP 200 deixaria passar um PIN
  // errado, que responde 200 com `false`.
  ok = (strstr(r, "true") != NULL);
  free(r);
  return ok;
}

void perfis_esquecer(void) {
  pthread_once(&sessaoUmaVez, sessaoTravaIniciar);
  pthread_mutex_lock(&sessaoTrava);
  sessaoAvancar();
  memset(lista, 0, sizeof lista);
  n = 0;
  dono[0] = 0;
  ativo = 1;
  pthread_mutex_unlock(&sessaoTrava);
  escolhido = 0;
  gravado = 0;
  dados_apagar(ARQ_ATIVO);
  // O cache da LISTA sai junto. Ele guarda os nomes das pessoas da conta
  // anterior e as URLs dos avatares delas — deixa-lo no aparelho faria a tela
  // de escolha da proxima conta abrir mostrando a familia da conta passada.
  dados_apagar(ARQ_LISTA);
  printf("[perfis] perfis esquecidos (saiu da conta)\n");
}
