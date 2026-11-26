#include "debrid.h"
#include "rede.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <stdatomic.h>
#include <time.h>
#include <pthread.h>

// As tres bases. A versao esta EMBUTIDA no caminho de proposito: o TorBox pede
// /v1/ antes de /api/ (api.torbox.app/v1/api/...), e deixar isso implicito na
// montagem de cada rota e o tipo de detalhe que se erra uma vez e se paga em
// 404 silencioso.
#define RD "https://api.real-debrid.com/rest/1.0"
#define TB "https://api.torbox.app/v1/api"
#define PM "https://www.premiumize.me/api"
// AllDebrid API v4 (docs.alldebrid.com). Base SEM versao: as rotas de status
// sao v4.1 e as demais v4, entao a versao vai em cada rota, pelo mesmo motivo
// do TorBox acima. `agent` segue na query mesmo a documentacao dizendo que
// deixou de ser exigido em 15/01/2025 ("Removed agent and version
// requirements"): e um identificador sem segredo, e servidor que ignora
// parametro a mais nao reclama.
#define AD "https://api.alldebrid.com"
#define AD_AGENTE "agent=nuvio"

enum { SRD = DEBRID_RD, STB = DEBRID_TB, SPM = DEBRID_PM, SAD = DEBRID_AD, SN = DEBRID_N };
static const char *nomeServ[SN] = { "Real-Debrid", "TorBox", "Premiumize", "AllDebrid" };
static const char *idServ[SN] = { "realdebrid", "torbox", "premiumize", "alldebrid" };

// UMA CHAVE POR SERVICO, e nao uma so. A conta pode trazer mais de uma
// credencial "debrid:*" (sync.c chama esta funcao uma vez por provedor), e
// guardar so a ultima faria o resultado depender da ORDEM que o servidor
// devolve as linhas — o pior tipo de defeito, porque muda sozinho.
//
// TRES ORIGENS, e `chave` e a EFETIVA: a que vale para o resolvedor.
//   chaveConta — vinda do sync ("debrid:<servico>"), some no logout;
//   chaveLocal — digitada nesta TV (Ajustes > Integracoes > Debrid), fica no
//                aparelho e SOBREVIVE ao logout: nao e da conta de ninguem.
// A local vence quando existe: e a pessoa dizendo "use esta". Sem ela vale a
// da conta. O AllDebrid so entra por aqui: o app web oficial (0.3.38) nao
// tem o servico (grep em debridProviders.js: so torbox, premiumize e
// realdebrid), entao nenhuma conta o traz hoje.
static char chave[SN][200];
static char chaveConta[SN][200];
static char chaveLocal[SN][200];
static int  alvoT, alvoE;
static pthread_mutex_t chaveTrava = PTHREAD_MUTEX_INITIALIZER;
static uint64_t sessaoAtual;
static atomic_uint tbGeracao, contaGeracao[DEBRID_N];
unsigned debrid_tb_geracao(void) { return atomic_load(&tbGeracao); }
unsigned debrid_conta_geracao(int q) { return q >= 0 && q < DEBRID_N ? atomic_load(&contaGeracao[q]) : 0; }

// RECUSA DE CONTA POR BUSCA. recusado[q] guarda o status HTTP da recusa (0 =
// nenhuma); `geracao` sobe a cada debrid_nova_busca. Atomicos porque
// debrid_resolver rodava em ate 4 fios da verificacao ao mesmo tempo; desde o
// #130 a verificacao e em serie, mas o fio dela continua sendo outro que o de
// desenho, e a trava nao custa nada.
//
// A GERACAO e o que impede um fio da busca ANTERIOR — uma verificacao de
// streams.c que ainda esta no prazo dela — de marcar recusa na busca nova:
// ele guardou a geracao ao entrar e so escreve se ela nao mudou.
//
// O LIMITE QUE ESTAVA ACEITO AQUI CAIU COM O #130: com 4 fios, ate 4
// createtorrent saiam antes de o primeiro 403 voltar. Com a verificacao em
// serie, o primeiro 403 marca a recusa antes da candidata seguinte.
static _Atomic unsigned geracao;
static _Atomic int recusado[SN];

// CONTA SEM PLANO, e isto nao e "recusa desta busca": vale a SESSAO inteira.
// Registros 1731-1774 (webOS 1.4.1): o TorBox respondeu 403
// PLAN_RESTRICTED_FEATURE ("API feature not available on your plan") e o
// Premiumize 200 com "Account not premium." — conta gratuita nos dois. Um
// plano nao muda entre uma busca e a outra, e a recusa por busca fazia a mesma
// pessoa pagar 4 createtorrent a cada titulo aberto; o Premiumize, que nem era
// reconhecido como erro de conta, foi perguntado torrent por torrent (197
// linhas). So uma chave nova (debrid_definir_chave) ou o logout limpam.
// `avisado` e o "ja mostrei o aviso" de cada servico, pelo mesmo tempo.
static _Atomic int semPlano[SN];
static _Atomic int avisado[SN];
// Uma consulta por chave a cada cinco minutos, compartilhada por Ajustes e play.
static pthread_mutex_t contaMutex = PTHREAD_MUTEX_INITIALIZER;
// ponytail: consultas serializadas; separar por provedor se houver contencao.
static pthread_mutex_t consultaMutex = PTHREAD_MUTEX_INITIALIZER;
static time_t contaConsultada[SN], contaAte[SN];
static int contaEstado[SN]; // 0 desconhecida, 1 premium, 2 sem plano, 3 chave recusada

const char *debrid_servico_nome(int q) { return q >= 0 && q < SN ? nomeServ[q] : ""; }
const char *debrid_servico_id(int q) { return q >= 0 && q < SN ? idServ[q] : ""; }
// Um resultado negativo vencido precisa chegar ao resolvedor para ser reconsultado.
// Sem rede aqui: estes leitores tambem rodam no fio da interface.
static int contaBloqueada(int q) {
  int bloqueada;
  pthread_mutex_lock(&contaMutex);
  bloqueada = atomic_load(&semPlano[q]) &&
      (!contaConsultada[q] || time(NULL) - contaConsultada[q] < 300);
  pthread_mutex_unlock(&contaMutex);
  return bloqueada;
}

// Torrents que a busca atual achou fora de cache (debrid_fora_de_cache).
static _Atomic int foraCache;

// Codigo INTERNO dos resolvedores: "fora de cache", que para quem chama e 0
// (nao tocou), mas que a escolha manual precisa separar de "falhou" para saber
// se vale pedir ao servico que baixe. Nunca sai deste arquivo.
#define FORA 3

// pthread_key, e nao TLS do compilador: o carregador das Samsung Tizen 4/5
// nao aceita _Thread_local (mesma restricao documentada em rede.c).
static pthread_key_t contextoChave;
static pthread_once_t contextoUmaVez = PTHREAD_ONCE_INIT;
static int contextoCriado;
static void contextoIniciar(void) { contextoCriado = pthread_key_create(&contextoChave, NULL) == 0; }
static const DebridContexto *contextoDoFio(void) {
  pthread_once(&contextoUmaVez, contextoIniciar);
  return contextoCriado ? pthread_getspecific(contextoChave) : NULL;
}
void debrid_contexto_capturar(DebridContexto *c) {
  if (!c) return;
  pthread_mutex_lock(&chaveTrava);
  memcpy(c->chave, chave, sizeof c->chave);
  c->temporada = alvoT; c->episodio = alvoE;
  c->sessao = sessaoAtual; c->busca = atomic_load(&geracao);
  c->conta = 0;
  pthread_mutex_unlock(&chaveTrava);
}
void debrid_contexto_entrar(const DebridContexto *c) {
  pthread_once(&contextoUmaVez, contextoIniciar);
  if (contextoCriado) pthread_setspecific(contextoChave, c);
}
void debrid_contexto_sair(void) {
  pthread_once(&contextoUmaVez, contextoIniciar);
  if (contextoCriado) pthread_setspecific(contextoChave, NULL);
}
static int contextoAbrir(DebridContexto *c) {
  if (contextoDoFio()) return 0;
  debrid_contexto_capturar(c);
  debrid_contexto_entrar(c);
  return 1;
}
static int contextoValeSemTrava(const DebridContexto *c) {
  return c && c->sessao == sessaoAtual && (c->conta || c->busca == atomic_load(&geracao));
}
static int contextoVale(void) {
  int vale;
  pthread_mutex_lock(&chaveTrava);
  vale = contextoValeSemTrava(contextoDoFio());
  pthread_mutex_unlock(&chaveTrava);
  return vale;
}
static int semPlanoMarcar(int q, int status) {
  int zero = 0, mudou;
  pthread_mutex_lock(&chaveTrava);
  mudou = contextoValeSemTrava(contextoDoFio()) &&
          atomic_compare_exchange_strong(&semPlano[q], &zero, status);
  pthread_mutex_unlock(&chaveTrava);
  return mudou;
}
static const char *chaveDoFio(int q) {
  const DebridContexto *c = contextoDoFio();
  return c && q >= 0 && q < SN ? c->chave[q] : "";
}

static int idServico(const char *s) {
  if (!strcasecmp(s, "realdebrid") || !strcasecmp(s, "real-debrid")) return SRD;
  if (!strcasecmp(s, "torbox")     || !strcasecmp(s, "tor-box"))     return STB;
  if (!strcasecmp(s, "premiumize") || !strcasecmp(s, "premiumize-me")
      || !strcasecmp(s, "premiumizeme")) return SPM;
  if (!strcasecmp(s, "alldebrid")  || !strcasecmp(s, "all-debrid")
      || !strcasecmp(s, "all_debrid") || !strcasecmp(s, "ad")) return SAD;
  return -1;
}
int debrid_servico_por_nome(const char *nome) { return nome && *nome ? idServico(nome) : -1; }

int debrid_servico_disponivel(int q) {
  const DebridContexto *c = contextoDoFio();
  int vale = 0;
  if (q < 0 || q >= SN) return 0;
  pthread_mutex_lock(&chaveTrava);
  if (!c || contextoValeSemTrava(c))
    vale = (c ? c->chave[q][0] : chave[q][0]) &&
           !atomic_load(&recusado[q]) && !contaBloqueada(q);
  pthread_mutex_unlock(&chaveTrava);
  return vale;
}

// Recalcula a efetiva de um servico. Chave nova zera "sem plano" e o aviso: a
// pessoa trocou a chave justamente para sair daquele estado.
static void efetiva(int q) {
  const char *k = chaveLocal[q][0] ? chaveLocal[q] : chaveConta[q];
  pthread_mutex_lock(&contaMutex);
  if (strcmp(chave[q], k)) {
    if (q == STB) atomic_fetch_add(&tbGeracao, 1u);
    atomic_fetch_add(&contaGeracao[q], 1u);
    atomic_fetch_add(&geracao, 1u);
    atomic_store(&recusado[q], 0);
    atomic_store(&semPlano[q], 0); atomic_store(&avisado[q], 0);
    contaConsultada[q] = contaAte[q] = 0; contaEstado[q] = 0;
  }
  snprintf(chave[q], sizeof chave[q], "%s", k);
  pthread_mutex_unlock(&contaMutex);
}

void debrid_conta_iniciar(DebridConta *c) { if (c) memset(c, 0, sizeof *c); }
void debrid_conta_definir(DebridConta *c, const char *servico, const char *k) {
  int q = servico ? idServico(servico) : -1;
  if (c && q >= 0) snprintf(c->chave[q], sizeof c->chave[q], "%s", k ? k : "");
}
int debrid_aplicar_chaves_conta(const DebridConta *c, uint64_t sessao) {
  int q;
  if (!c) return 0;
  pthread_mutex_lock(&chaveTrava);
  if (sessao != sessaoAtual) { pthread_mutex_unlock(&chaveTrava); return 0; }
  for (q = 0; q < SN; q++) {
    snprintf(chaveConta[q], sizeof chaveConta[q], "%.*s", (int)sizeof c->chave[q] - 1, c->chave[q]);
    efetiva(q);
  }
  pthread_mutex_unlock(&chaveTrava);
  return 1;
}
void debrid_aplicar_chaves_locais(const DebridConta *c) {
  int q;
  pthread_mutex_lock(&chaveTrava);
  for (q = 0; q < SN; q++) {
    if (c) snprintf(chaveLocal[q], sizeof chaveLocal[q], "%.*s", (int)sizeof c->chave[q] - 1, c->chave[q]);
    else chaveLocal[q][0] = 0;
    efetiva(q);
  }
  pthread_mutex_unlock(&chaveTrava);
}
void debrid_abrir_sessao(uint64_t sessao) {
  int q;
  pthread_mutex_lock(&chaveTrava);
  sessaoAtual = sessao;
  atomic_fetch_add(&tbGeracao, 1u);
  atomic_fetch_add(&geracao, 1u);
  memset(chaveConta, 0, sizeof chaveConta); alvoT = alvoE = 0;
  atomic_store(&foraCache, 0);
  for (q = 0; q < SN; q++) {
    atomic_store(&recusado[q], 0); atomic_store(&semPlano[q], 0); atomic_store(&avisado[q], 0);
    efetiva(q);
  }
  pthread_mutex_unlock(&chaveTrava);
}

void debrid_definir_chave(const char *servico, const char *k) {
  int q;
  if (!servico || !k || !*k) return;
  q = idServico(servico);
  if (q < 0) {
    printf("[debrid] %s: servico sem resolvedor aqui, ignorado\n", servico);
    return;
  }
  pthread_mutex_lock(&chaveTrava);
  snprintf(chaveConta[q], sizeof chaveConta[q], "%s", k);
  efetiva(q);
  pthread_mutex_unlock(&chaveTrava);
  printf("[debrid] chave do %s vinda da conta\n", nomeServ[q]);
}

// Chave digitada nesta TV. Vazia apaga (volta a valer a da conta, se houver).
// O log diz so o servico: a chave nunca vai para ele.
void debrid_definir_chave_local(const char *servico, const char *k) {
  int q = servico ? idServico(servico) : -1;
  if (q < 0) return;
  pthread_mutex_lock(&chaveTrava);
  snprintf(chaveLocal[q], sizeof chaveLocal[q], "%s", k ? k : "");
  efetiva(q);
  pthread_mutex_unlock(&chaveTrava);
  printf("[debrid] chave local do %s %s\n", nomeServ[q], k && *k ? "definida" : "apagada");
}
int debrid_origem(const char *servico) {
  int r, q = servico ? idServico(servico) : -1;
  if (q < 0) return 0;
  pthread_mutex_lock(&chaveTrava);
  r = chaveLocal[q][0] ? 2 : chaveConta[q][0] ? 1 : 0;
  pthread_mutex_unlock(&chaveTrava);
  return r;
}
// "····abcd": so os 4 ultimos, o bastante para reconhecer QUAL chave e.
const char *debrid_chave_mascarada(const char *servico, char *dst, unsigned n) {
  int q = servico ? idServico(servico) : -1;
  size_t L;
  if (dst && n) dst[0] = 0;
  if (q < 0 || !dst || !n) return dst;
  pthread_mutex_lock(&chaveTrava);
  if (chave[q][0]) {
    L = strlen(chave[q]);
    snprintf(dst, n, "····%s", L > 8 ? chave[q] + L - 4 : "");
  }
  pthread_mutex_unlock(&chaveTrava);
  return dst;
}
// So conta servico que PODE resolver: com todas as chaves em conta sem plano,
// streams.c descarta os torrents sem url logo na lista (como sem debrid) e a
// verificacao vai direto as fontes diretas, em vez de gastar o lote nelas.
// As origens das APIs dos servicos com chave que PODEM resolver, para aquecer.c.
// Sem caminho, sem chave: so "https://host".
int debrid_origens(const char **o, int max) {
  static const char *const API[SN] = { "https://api.real-debrid.com", "https://api.torbox.app",
                                       "https://www.premiumize.me", "https://api.alldebrid.com" };
  const DebridContexto *c = contextoDoFio();
  int q, k = 0;
  pthread_mutex_lock(&chaveTrava);
  if (!c || contextoValeSemTrava(c))
    for (q = 0; q < SN && k < max; q++)
      if ((c ? c->chave[q][0] : chave[q][0]) && !contaBloqueada(q)) o[k++] = API[q];
  pthread_mutex_unlock(&chaveTrava);
  return k;
}
int debrid_ativo(void) {
  const DebridContexto *c = contextoDoFio();
  int q, r = 0;
  pthread_mutex_lock(&chaveTrava);
  if (!c || contextoValeSemTrava(c))
    for (q = 0; q < SN; q++)
      if ((c ? c->chave[q][0] : chave[q][0]) && !contaBloqueada(q)) { r = 1; break; }
  pthread_mutex_unlock(&chaveTrava);
  return r;
}
void debrid_esquecer(void) {
  int q;
  pthread_mutex_lock(&chaveTrava);
  atomic_fetch_add(&tbGeracao, 1u);
  atomic_fetch_add(&geracao, 1u);
  // Logout: a chave DA CONTA sai; a digitada nesta TV fica (ver chaveLocal).
  memset(chaveConta, 0, sizeof chaveConta); alvoT = alvoE = 0;
  atomic_store(&foraCache, 0);
  for (q = 0; q < SN; q++) {
    atomic_store(&recusado[q], 0);
    atomic_store(&semPlano[q], 0);
    atomic_store(&avisado[q], 0);
    pthread_mutex_lock(&contaMutex);
    contaConsultada[q] = contaAte[q] = 0; contaEstado[q] = 0;
    pthread_mutex_unlock(&contaMutex);
    efetiva(q);
  }
  pthread_mutex_unlock(&chaveTrava);
}
void debrid_nova_busca(void) {
  int q;
  pthread_mutex_lock(&chaveTrava);
  atomic_fetch_add(&geracao, 1u);
  atomic_store(&foraCache, 0);
  for (q = 0; q < SN; q++) atomic_store(&recusado[q], 0);
  pthread_mutex_unlock(&chaveTrava);
}
int debrid_recusa(char *dst, unsigned n) {
  int q;
  for (q = 0; q < SN; q++) {
    int st = atomic_load(&recusado[q]);
    if (!st && atomic_load(&semPlano[q])) {
      if (dst && n) snprintf(dst, n, "%s free-plan", nomeServ[q]);
      return 1;
    }
    if (!st) continue;
    if (dst && n) snprintf(dst, n, "%s %d", nomeServ[q], st);
    return 1;
  }
  return 0;
}

int debrid_fora_de_cache(void) { return atomic_load(&foraCache); }

int debrid_sem_plano(void) {
  const DebridContexto *c = contextoDoFio();
  int q, m = 0;
  pthread_mutex_lock(&chaveTrava);
  if (!c || contextoValeSemTrava(c))
    for (q = 0; q < SN; q++)
      if ((c ? c->chave[q][0] : chave[q][0]) && atomic_load(&semPlano[q])) m |= 1 << q;
  pthread_mutex_unlock(&chaveTrava);
  return m;
}

// Frases FIXAS por servico, uma chave de traducao cada (idioma_tab.h): texto
// montado com %s nunca casaria com a tabela.
const char *debrid_sem_plano_frase(int mascara) {
  int tb = (mascara & (1 << STB)) != 0, pm = (mascara & (1 << SPM)) != 0;
  int rd = (mascara & (1 << SRD)) != 0, ad = (mascara & (1 << SAD)) != 0;
  for (int q = 0; q < SN; q++)
    if (q != SAD && (mascara & (1 << q)) && atomic_load(&semPlano[q]) == 2)
      return "Chave de debrid recusada — confira em Ajustes > Integrações > Debrid";
  pthread_mutex_lock(&contaMutex);
  int vencida = 0;
  for (int q = 0; q < SN; q++)
    if ((mascara & (1 << q)) && contaEstado[q] == 2 && contaAte[q] > 0 && contaAte[q] <= time(NULL)) vencida = 1;
  pthread_mutex_unlock(&contaMutex);
  if (vencida) return "Sua conta de debrid expirou — renove o plano em seu provedor";
  if (tb + pm + rd + ad > 1)
    return "Suas contas de debrid são gratuitas e não permitem uso pela API — as fontes torrent ficam de fora";
  if (tb) return "Sua conta TorBox é gratuita e não permite uso pela API — fontes torrent do TorBox ficam de fora";
  if (pm) return "Sua conta Premiumize é gratuita e não permite uso pela API — fontes torrent do Premiumize ficam de fora";
  if (rd) return "Sua conta Real-Debrid não é premium e não permite uso pela API — fontes torrent do Real-Debrid ficam de fora";
  // semPlano == 2 e a chave RECUSADA (AUTH_BAD_APIKEY), nao o plano: a
  // pessoa age em lugares diferentes (trocar a chave x assinar).
  if (ad) return atomic_load(&semPlano[SAD]) == 2
      ? "Sua chave do AllDebrid foi recusada — confira em Ajustes > Integrações > Debrid"
      : "Sua conta AllDebrid não é premium — as fontes torrent do AllDebrid ficam de fora";
  return NULL;
}

const char *debrid_sem_plano_nome(int mascara) {
  int tb = (mascara & (1 << STB)) != 0, pm = (mascara & (1 << SPM)) != 0;
  int rd = (mascara & (1 << SRD)) != 0, ad = (mascara & (1 << SAD)) != 0;
  if (tb + pm + rd + ad != 1) return NULL;
  for (int q = 0; q < SN; q++)
    if ((mascara & (1 << q)) && atomic_load(&semPlano[q]) == 2) return NULL;
  if (tb) return "TorBox";
  if (pm) return "Premiumize";
  if (rd) return "Real-Debrid";
  return atomic_load(&semPlano[SAD]) == 2 ? NULL : "AllDebrid";
}

int debrid_sem_plano_novo(void) {
  int q, m = 0;
  pthread_mutex_lock(&chaveTrava);
  for (q = 0; q < SN; q++) {
    int zero = 0;
    if (!chave[q][0] || !atomic_load(&semPlano[q])) continue;
    if (atomic_compare_exchange_strong(&avisado[q], &zero, 1)) m |= 1 << q;
  }
  pthread_mutex_unlock(&chaveTrava);
  return m;
}
void debrid_definir_episodio(int t, int e) {
  // O episodio de um worker ja despachado e imutavel. Ele nao pode alterar
  // o alvo vivo depois de a pessoa abrir outro perfil/titulo.
  if (contextoDoFio()) return;
  pthread_mutex_lock(&chaveTrava); alvoT = t; alvoE = e; pthread_mutex_unlock(&chaveTrava);
}
void debrid_episodio(int *t, int *e) {
  const DebridContexto *c = contextoDoFio();
  if (c) { if (t) *t = c->temporada; if (e) *e = c->episodio; return; }
  pthread_mutex_lock(&chaveTrava);
  if (t) *t = alvoT;
  if (e) *e = alvoE;
  pthread_mutex_unlock(&chaveTrava);
}

// ---------------------------------------------------------------- http

static void urlenc(char *dst, unsigned n, const char *s) {
  unsigned k = 0;
  for (; *s && k + 4 < n; s++) {
    unsigned char c = (unsigned char)*s;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') dst[k++] = (char)c;
    else k += (unsigned)snprintf(dst + k, n - k, "%%%02X", c);
  }
  dst[k] = 0;
}

// ---------------------------------------------------------------- dicas (ver debrid.h)
// Tabela de espalhamento por hash + fileIdx, do tamanho da lista. Era um anel
// fixo de 512 (616 KB parados): com mais de 512 torrents na lista, os primeiros
// perdiam nome e trackers. Agora cada dica guarda so o que tem (texto do
// tamanho real) e a tabela cresce; passou de DICAS_MAX, recomeca vazia (a lista
// seguinte repoe as suas).
#define DICAS_MAX 8192
typedef struct { char hash[48]; int idx; char *arquivo, *fontes; } Dica;
static Dica *dicas;
static unsigned dicasCap, dicasN;
static pthread_mutex_t dicaTrava = PTHREAD_MUTEX_INITIALIZER;

static unsigned dicaChave(const char *h, int idx) {
  unsigned x = 2166136261u;
  for (; *h; h++) { x ^= (unsigned char)tolower((unsigned char)*h); x *= 16777619u; }
  return x ^ (unsigned)idx * 2654435761u;
}
// Posicao de (h, idx) na tabela: a dele, ou a vazia onde entraria.
static Dica *dicaLugar(Dica *t, unsigned cap, const char *h, int idx) {
  unsigned i = dicaChave(h, idx) & (cap - 1);
  while (t[i].hash[0] && !(t[i].idx == idx && !strcasecmp(t[i].hash, h))) i = (i + 1) & (cap - 1);
  return &t[i];
}
static void dicasLimpar(void) {
  for (unsigned i = 0; i < dicasCap; i++) { free(dicas[i].arquivo); free(dicas[i].fontes); }
  free(dicas); dicas = NULL; dicasCap = dicasN = 0;
}
// Cabe mais uma? Dobra a tabela (carga ate 1/2) ou, no teto, recomeca.
static int dicasEspaco(void) {
  if ((dicasN + 1) * 2 <= dicasCap) return 1;
  if (dicasN >= DICAS_MAX) dicasLimpar();
  unsigned nc = dicasCap ? dicasCap * 2 : 1024;
  Dica *nt = calloc(nc, sizeof *nt);
  if (!nt) return 0;
  for (unsigned i = 0; i < dicasCap; i++)
    if (dicas[i].hash[0]) *dicaLugar(nt, nc, dicas[i].hash, dicas[i].idx) = dicas[i];
  free(dicas); dicas = nt; dicasCap = nc;
  return 1;
}
static char *dicaTexto(char *velho, const char *s) {
  free(velho);
  return s && *s ? strdup(s) : NULL;
}

void debrid_dica_fonte(const char *infoHash, int fileIdx, const char *arquivo, const char *fontes) {
  if (!infoHash || !*infoHash || strlen(infoHash) >= sizeof dicas[0].hash) return;
  pthread_mutex_lock(&dicaTrava);
  Dica *d = dicasCap ? dicaLugar(dicas, dicasCap, infoHash, fileIdx) : NULL;
  if (!d || !d->hash[0]) {
    if (!dicasEspaco()) { pthread_mutex_unlock(&dicaTrava); return; }
    d = dicaLugar(dicas, dicasCap, infoHash, fileIdx);
    snprintf(d->hash, sizeof d->hash, "%s", infoHash);
    d->idx = fileIdx;
    dicasN++;
  }
  d->arquivo = dicaTexto(d->arquivo, arquivo);
  d->fontes = dicaTexto(d->fontes, fontes);
  pthread_mutex_unlock(&dicaTrava);
}
static void dicaDe(const char *infoHash, int fileIdx, char *arq, size_t na, char *fon, size_t nf) {
  if (arq && na) arq[0] = 0;
  if (fon && nf) fon[0] = 0;
  if (!infoHash || !*infoHash) return;
  pthread_mutex_lock(&dicaTrava);
  if (dicasCap) {
    const Dica *d = dicaLugar(dicas, dicasCap, infoHash, fileIdx);
    if (d->hash[0]) {
      if (arq && na && d->arquivo) snprintf(arq, na, "%s", d->arquivo);
      if (fon && nf && d->fontes) snprintf(fon, nf, "%s", d->fontes);
    }
  }
  pthread_mutex_unlock(&dicaTrava);
}

// MAGNET COMO O NuvioTV (DebridMagnetBuilder): hash + &dn=<arquivo> + um &tr=
// por tracker de "sources" ("tracker:" tirado, "dht:" fora). Tracker que nao
// cabe inteiro fica de fora. Sem dica, o magnet pelado de antes.
static void magnetMontar(const char *h, int fileIdx, const char *hashDica, char *dst, size_t n) {
  char arq[512], fon[640], enc[1600];
  size_t k;
  dicaDe(hashDica ? hashDica : h, fileIdx, arq, sizeof arq, fon, sizeof fon);
  k = (size_t)snprintf(dst, n, "magnet:?xt=urn:btih:%s", h);
  if (k >= n) { dst[n - 1] = 0; return; }
  if (arq[0]) {
    urlenc(enc, sizeof enc, arq);
    if (k + 4 + strlen(enc) < n) k += (size_t)snprintf(dst + k, n - k, "&dn=%s", enc);
  }
  for (char *p = fon; *p; ) {
    char *f = strchr(p, '\n');
    if (f) *f = 0;
    if (!strncasecmp(p, "tracker:", 8) || !strncmp(p, "udp://", 6) || !strncmp(p, "http", 4)) {
      const char *tr = !strncasecmp(p, "tracker:", 8) ? p + 8 : p;
      while (*tr == ' ') tr++;
      urlenc(enc, sizeof enc, tr);
      if (*tr && k + 4 + strlen(enc) < n) k += (size_t)snprintf(dst + k, n - k, "&tr=%s", enc);
    }
    if (!f) break;
    p = f + 1;
  }
}

// Os tres servicos aceitam a chave como "Authorization: Bearer" — e o que a
// documentacao de cada um recomenda, inclusive a do Premiumize, que ainda
// aceita ?apikey= por compatibilidade e diz em texto que o cabecalho existe
// justamente para a chave NAO entrar em registro de servidor nem em Referer.
// A unica excecao e o requestdl do TorBox, mais abaixo.
static char *get_auth(const char *base, const char *rota, int qual, int *st) {
  char url[900], auth[260];
  const char *cab[2];
  if (!contextoVale()) { if (st) *st = 0; return NULL; }
  snprintf(url, sizeof url, "%s/%s", base, rota);
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", chaveDoFio(qual));
  cab[0] = auth; cab[1] = NULL;
  return rede_baixar_st(url, 15, cab, st);
}
static char *post_form(const char *base, const char *rota, int qual,
                       const char *corpo, int *st) {
  char url[900], auth[260];
  const char *cab[3];
  if (!contextoVale()) { if (st) *st = 0; return NULL; }
  snprintf(url, sizeof url, "%s/%s", base, rota);
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", chaveDoFio(qual));
  cab[0] = auth; cab[1] = "Content-Type: application/x-www-form-urlencoded"; cab[2] = NULL;
  return rede_postar_st(url, 15, cab, corpo, st);
}
static int ok2xx(const char *r, int st) { return r && st >= 200 && st < 300; }

// ---------------------------------------------------------------- diagnostico
//
// O CORPO DA RESPOSTA DE ERRO VAI PARA O LOG. No registro 1541 (webOS 1.4.0)
// o TorBox respondeu `createtorrent: HTTP 403` oito vezes e o log so tinha o
// numero: o JSON do TorBox traz `error` (codigo) e `detail` (frase), e era ali
// que estava a diferenca entre "chave sem plano", "limite de torrents ativos"
// e "add_only_if_cached recusado". Sem o corpo nao da para escolher.
//
// 160 BYTES: cabe o {"success":false,"error":"...","detail":"..."} do TorBox
// e o {"error":"...","error_code":N} do Real-Debrid inteiros, e uma pagina
// HTML de WAF mostra o <title>, que e o que a identifica.
//
// A CHAVE E CORTADA DO TEXTO mesmo nao devendo estar la: ela vai no cabecalho
// Authorization (ou na query do requestdl), nao no corpo — mas uma API que
// ecoa o pedido no erro a poria no log, e o log sai da TV. O corte roda numa
// janela de 160 + 200 bytes (200 = o maior tamanho de chave) ANTES de truncar,
// para que uma chave atravessando o byte 160 nao deixe o comeco dela no log.
//
// SO RESPOSTA NAO-2xx VAI CRUA. Corpo 2xx pode trazer link direto (o `links`
// do torrents/info do RD e credencial, ver rede_url_publica); de 2xx que nao
// serviu saem so os campos de erro (error/detail/message), passados pelo
// mesmo corte.
#define CORPO_LOG 160

static void corpoSeguro(const char *r, char *dst, unsigned n) {
  char jan[CORPO_LOG + sizeof chave[0] + 1];
  size_t L = strlen(r), i;
  int q;
  if (L > sizeof jan - 1) L = sizeof jan - 1;
  memcpy(jan, r, L); jan[L] = 0;
  // quebra de linha e tab viram espaco: uma linha de log por falha
  for (i = 0; i < L; i++) if ((unsigned char)jan[i] < ' ') jan[i] = ' ';
  for (q = 0; q < SN; q++) {
    const char *k = chaveDoFio(q);
    size_t K = strlen(k);
    char *p;
    if (!K) continue;
    while ((p = strstr(jan, k)) != NULL) {
      memcpy(p, "***", 3);
      memmove(p + 3, p + K, strlen(p + K) + 1);
    }
  }
  if (strlen(jan) > CORPO_LOG) jan[CORPO_LOG] = 0;
  snprintf(dst, n, "%s", jan);
}

// strcasestr nao e C padrao e o conjunto de ferramentas do webOS nao o declara
// sem _GNU_SOURCE; sao tres usos, nao vale o define.
static int contem(const char *r, const char *s) {
  size_t L = strlen(s);
  for (; r && *r; r++) if (!strncasecmp(r, s, L)) return 1;
  return 0;
}

// ERRO DA CONTA OU DO TORRENT? A regra:
//   - o corpo diz que o item nao esta em cache -> do TORRENT: o proximo
//     torrent da mesma busca pode estar, segue tentando o servico;
//   - 401, 403 ou 429 -> da CONTA: chave, plano ou limite valem igual para
//     todo torrent, e repetir so gasta ate 15 s por tentativa;
//   - os codigos de erro de conta do TorBox em qualquer status (ACTIVE_LIMIT,
//     MONTHLY_LIMIT, COOLDOWN_LIMIT, PLAN_RESTRICTED_FEATURE, BAD_TOKEN,
//     AUTH_ERROR, NO_AUTH — lista da documentacao/SDK do TorBox, NAO
//     conferida contra uma resposta real ainda: o log novo e que vai mostrar
//     qual deles o 403 do registro 1541 traz);
//   - o resto (400, 404, 5xx, sem resposta) -> do torrent.
// Vale para os tres servicos: o 403 "permission_denied" do Real-Debrid tambem
// e da conta.
// CONTA SEM PLANO QUE PERMITA A API. Os textos sao os que os servicos
// mandaram de verdade (registros 1731-1774): TorBox `"error":
// "PLAN_RESTRICTED_FEATURE"` com HTTP 403, Premiumize `"Account not
// premium."` com HTTP 200. Se o Real-Debrid mandar um texto desses, entra pelo
// mesmo caminho — mas nenhum registro mostrou o RD assim ainda; o 403 dele
// continua sendo recusa por busca, como antes.
int debrid_eh_sem_plano(int st, const char *r) {
  (void)st;
  if (!r) return 0;
  return strstr(r, "PLAN_RESTRICTED") != NULL || contem(r, "not premium")
      || contem(r, "not_premium") || contem(r, "premium account required")
      || strstr(r, "MAGNET_MUST_BE_PREMIUM") != NULL;
}

static int erroDeConta(int st, const char *r) {
  if (r && (contem(r, "not cached") || contem(r, "not_cached")
            || contem(r, "uncached")))
    return 0;
  if (st == 401 || st == 403 || st == 429) return 1;
  if (debrid_eh_sem_plano(st, r)) return 1;
  if (r && (strstr(r, "ACTIVE_LIMIT") || strstr(r, "MONTHLY_LIMIT")
            || strstr(r, "COOLDOWN_LIMIT") || strstr(r, "PLAN_RESTRICTED")
            || strstr(r, "BAD_TOKEN") || strstr(r, "AUTH_ERROR")
            || strstr(r, "NO_AUTH")
            // AllDebrid responde erro de conta com HTTP 200 e o codigo no
            // corpo (docs.alldebrid.com, "Error codes")
            || strstr(r, "AUTH_BAD_APIKEY") || strstr(r, "AUTH_MISSING_APIKEY")
            || strstr(r, "AUTH_BLOCKED") || strstr(r, "MAGNET_TOO_MANY_ACTIVE")))
    return 1;
  return 0;
}

// O AllDebrid devolve HTTP 200 com {"status":"error","error":{"code":...}}, e
// o "status HTTP" que o resto deste arquivo guarda em recusado[] (e mostra no
// log como "AllDebrid 401") tem de ser um numero. Traduz o codigo para o
// equivalente: chave = 401, plano = 403, limite = 429. Outro codigo (do
// torrent, nao da conta) fica como veio.
static int adStatusSintetico(int st, const char *r) {
  if (!r) return st;
  if (strstr(r, "AUTH_BAD_APIKEY") || strstr(r, "AUTH_MISSING_APIKEY")) return 401;
  if (strstr(r, "AUTH_BLOCKED") || strstr(r, "MAGNET_MUST_BE_PREMIUM")) return 403;
  if (strstr(r, "MAGNET_TOO_MANY_ACTIVE")) return 429;
  return st;
}

// Registra a falha de uma chamada e devolve o que o resolvedor devolve: 0
// (falhou este torrent) ou -st (falhou pela CONTA; debrid_resolver para de
// usar o servico nesta busca). `rota` e o nome da chamada, NUNCA a URL — a do
// requestdl leva o token.
static int falha(int q, const char *rota, int st, const char *r) {
  char txt[CORPO_LOG + 1];
  if (!contextoVale()) return 0;
  if (!r) {
    printf("[debrid] %s %s: HTTP %d sem corpo\n", nomeServ[q], rota, st);
  } else if (st < 200 || st >= 300) {
    corpoSeguro(r, txt, sizeof txt);
    printf("[debrid] %s %s: HTTP %d (%u bytes) %s\n", nomeServ[q], rota, st,
           (unsigned)strlen(r), txt);
  } else {
    char campo[CORPO_LOG + 1], e[64] = "", d[CORPO_LOG + 1] = "";
    if (js_texto(r, NULL, "error", campo, sizeof campo)) corpoSeguro(campo, e, sizeof e);
    // AllDebrid: "error" e um objeto {code,message}
    else if (js_texto(r, NULL, "code", campo, sizeof campo)) corpoSeguro(campo, e, sizeof e);
    if (js_texto(r, NULL, "detail", campo, sizeof campo)
        || js_texto(r, NULL, "message", campo, sizeof campo))
      corpoSeguro(campo, d, sizeof d);
    printf("[debrid] %s %s: HTTP %d sem o esperado; error=%s detail=%s\n",
           nomeServ[q], rota, st, e[0] ? e : "-", d[0] ? d : "-");
  }
  if (debrid_eh_sem_plano(st, r)) {
    if (semPlanoMarcar(q, 1))
      printf("[debrid] %s: conta sem plano para a API; fora pelo resto da sessao\n",
             nomeServ[q]);
  }
  if (q == SAD) {
    // chave recusada e diferente de plano (ver debrid_sem_plano_frase)
    if (r && (strstr(r, "AUTH_BAD_APIKEY") || strstr(r, "AUTH_MISSING_APIKEY"))) {
      if (semPlanoMarcar(q, 2))
        printf("[debrid] AllDebrid: chave recusada; fora pelo resto da sessao\n");
    }
    if (st >= 200 && st < 300) st = adStatusSintetico(st, r);
  }
  return erroDeConta(st, r) ? -(st > 0 ? st : 1) : 0;
}

static void minusc(char *s) { for (; *s; s++) *s = (char)tolower((unsigned char)*s); }

// Copia do hash em minusculas. O TorBox guarda o infohash em minusculo e a
// consulta de cache compara TEXTO: um hash que o addon mandou em maiusculas
// volta "nao esta em cache" mesmo estando.
static void hashMin(char *dst, unsigned n, const char *h) {
  snprintf(dst, n, "%s", h);
  minusc(dst);
}

// ---------------------------------------------------------------- arquivo

static int ehVideo(const char *nome) {
  static const char *ext[] = { ".mp4", ".mkv", ".webm", ".avi", ".mov", ".m4v", ".ts", ".m2ts", ".wmv", NULL };
  size_t L = strlen(nome); int i;
  for (i = 0; ext[i]; i++) {
    size_t e = strlen(ext[i]);
    if (L > e && !strcasecmp(nome + L - e, ext[i])) return 1;
  }
  return 0;
}

// ESCOLHA DO ARQUIVO, na ordem do NuvioTV (DebridFileSelection.kt
// selectDebridFile) enquanto ele acha algo, so entre os videos:
//   1. o nome que o addon deu (behaviorHints/clientResolve.filename): caminho
//      igual, ou terminando em "/<nome>", depois so o nome do arquivo; com e
//      depois sem diferenca de caixa.
//   2. o episodio: s<T>e<E> ou <T>x<E> (zeros a esquerda livres) no nome do
//      arquivo, sem letra/numero colado antes nem numero depois.
//   3. o fileIdx do addon (posicao na lista inteira), se for video.
// Onde o NuvioTV desiste, aqui ainda toca (decisao do dono, 2026-11-18: "tocar
// todos"):
//   - em 1 e 2, mais de um igual = o MAIOR deles (o arquivo principal, nao o
//     sample), em vez de nenhum;
//   4. o episodio so no nome da PASTA ("Show.S01E05/video.mkv");
//   5. um video so no torrent: ele (torrent de um arquivo, "Show - 05.mkv"
//      sem SxxEyy nem fileIdx);
//   6. sem episodio procurado (filme): o maior video, como antes do NuvioTV.
// Episodio de PACOTE em que nada aponta o arquivo continua sem nenhum: o maior
// seria quase sempre o episodio errado, e a falha deixa outra fonte tentar.
// Devolve o ELEMENTO escolhido (ponteiro para o '{' dele, dentro de `files`)
// ou NULL.
//
// POR QUE DEVOLVE O ELEMENTO E NAO UM id. Os servicos nomeiam os campos a sua
// maneira — o RD manda "path"/"bytes" e o arquivo tem "id"; o TorBox manda
// "name"/"size" com "id"; o Premiumize manda "path"/"size" e NAO tem id
// nenhum, o link direto vem dentro do proprio elemento. Devolvendo o elemento,
// cada resolvedor tira dali o que o seu servico usa e a ORDEM DE ESCOLHA fica
// escrita uma vez so. Os nomes dos campos entram por parametro.
static const char *baseNome(const char *c) {
  const char *b = strrchr(c, '/');
  return b ? b + 1 : c;
}
static void caminhoNormal(char *c) {
  for (char *p = c; *p; p++) if (*p == '\\') *p = '/';
  while (*c == ' ' || *c == '/') memmove(c, c + 1, strlen(c));
  size_t n = strlen(c);
  while (n && c[n - 1] == ' ') c[--n] = 0;
}
static int igual(const char *a, const char *b, int ci) { return ci ? !strcasecmp(a, b) : !strcmp(a, b); }
static int terminaEm(const char *c, const char *sufixo, int ci) {
  size_t a = strlen(c), b = strlen(sufixo);
  return a > b && c[a - b - 1] == '/' && igual(c + a - b, sufixo, ci);
}
static int numeroEm(const char *p, int valor, const char **fim) {
  // zeros a esquerda livres, depois exatamente `valor`
  long v = 0; int d = 0;
  while (*p == '0' && isdigit((unsigned char)p[1])) p++;
  while (isdigit((unsigned char)*p) && d < 6) { v = v * 10 + (*p++ - '0'); d++; }
  *fim = p;
  return d && v == valor;
}
static int casaEpisodio(const char *nome, int t, int e) {
  for (const char *p = nome; *p; p++) {
    const char *q, *r;
    if (p > nome && isalnum((unsigned char)p[-1])) continue;
    if ((*p == 's' || *p == 'S') && numeroEm(p + 1, t, &q) && (*q == 'e' || *q == 'E') &&
        numeroEm(q + 1, e, &r) && !isdigit((unsigned char)*r)) return 1;
    if (isdigit((unsigned char)*p) && numeroEm(p, t, &q) && (*q == 'x' || *q == 'X') &&
        numeroEm(q + 1, e, &r) && !isdigit((unsigned char)*r)) return 1;
  }
  return 0;
}
// O maior entre os que casaram: `*el` fica com o maior visto ate aqui.
static void ficaMaior(const char **el, double *tamEl, const char *p, double tam) {
  if (!*el || tam > *tamEl) { *el = p; *tamEl = tam; }
}
static const char *escolherArquivo(const char *files, int fileIdx,
                                   const char *kNome, const char *kTam, const char *dica) {
  const char *p, *maior = NULL, *noIndice = NULL, *epNome = NULL, *epPasta = NULL;
  int idx = 0, temporada = -1, episodio = 0, ep, videos = 0;
  double maiorTam = -1, epNomeTam = -1, epPastaTam = -1;
  char alvo[512] = "";
  debrid_episodio(&temporada, &episodio);
  ep = temporada >= 0 && episodio > 0;
  if (dica && *dica) { snprintf(alvo, sizeof alvo, "%s", dica); caminhoNormal(alvo); }
  // 1. pelo nome: quatro passadas (caminho / nome do arquivo x caixa)
  for (int modo = 0; alvo[0] && modo < 4; modo++) {
    const char *achado = NULL; double achadoTam = -1; int ci = modo & 1, soBase = modo >> 1;
    for (p = files; p && *p == '{'; p = js_prox(js_fim(p))) {
      char nome[600];
      if (!js_texto(p, js_fim(p), kNome, nome, sizeof nome) || !ehVideo(nome)) continue;
      caminhoNormal(nome);
      int ok = soBase ? igual(baseNome(nome), baseNome(alvo), ci)
                      : igual(nome, alvo, ci) || (strchr(alvo, '/') && terminaEm(nome, alvo, ci));
      if (ok) ficaMaior(&achado, &achadoTam, p, js_num(p, js_fim(p), kTam, 0));
    }
    if (achado) return achado;
  }
  // 2-6 numa volta so
  for (p = files, idx = 0; p && *p == '{'; p = js_prox(js_fim(p)), idx++) {
    char nome[600];
    if (!js_texto(p, js_fim(p), kNome, nome, sizeof nome) || !ehVideo(nome)) continue;
    caminhoNormal(nome);
    double tam = js_num(p, js_fim(p), kTam, 0);
    videos++;
    if (ep && casaEpisodio(baseNome(nome), temporada, episodio)) ficaMaior(&epNome, &epNomeTam, p, tam);
    else if (ep && casaEpisodio(nome, temporada, episodio)) ficaMaior(&epPasta, &epPastaTam, p, tam);
    if (idx == fileIdx) noIndice = p;
    if (tam > maiorTam) { maior = p; maiorTam = tam; }
  }
  if (epNome) return epNome;                             // 2
  // 3. #556: o fileIdx publicado vale mesmo sem nome/episodio casar (pacote
  // com "Show - 05.mkv", sem SxxEyy). O NuvioTV falha ai.
  if (fileIdx >= 0 && noIndice) return noIndice;
  if (epPasta) return epPasta;                           // 4
  if (videos == 1) return maior;                         // 5
  return ep ? NULL : maior;                              // 6
}

// THE PLAYING FILE BY NAME, THEN SIZE (tbabrir.c's recovery and RD's reuse):
// the account item is known, the add-on said the file's name and size. Exact
// base name (case free; several = the one of that size, else the first), else
// the only video of exactly that size. NULL = let escolherArquivo decide.
static const char *arquivoExato(const char *files, const char *kNome, const char *kTam, const char *arquivo,
                                uint64_t bytes) {
  const char *p, *porNome = NULL, *porTam = NULL, *alvo = arquivo ? baseNome(arquivo) : "";
  int nTam = 0;
  for (p = files; p && *p == '{'; p = js_prox(js_fim(p))) {
    char nome[600];
    double tam;
    if (!js_texto(p, js_fim(p), kNome, nome, sizeof nome) || !ehVideo(nome)) continue;
    caminhoNormal(nome);
    tam = js_num(p, js_fim(p), kTam, 0);
    if (alvo[0] && !strcasecmp(baseNome(nome), alvo)) {
      if (!porNome || (bytes && (uint64_t)tam == bytes)) porNome = p;
    }
    if (bytes && (uint64_t)tam == bytes) { porTam = p; nTam++; }
  }
  if (porNome) return porNome;
  return nTam == 1 ? porTam : NULL;
}

// ---------------------------------------------------------------- Real-Debrid
//
// Rotas (api.real-debrid.com/rest/1.0, documentacao oficial "REST API"):
//   POST torrents/addMagnet, GET torrents/info/<id>,
//   POST torrents/selectFiles/<id>, POST unrestrict/link.

// `pct` recebe o progresso (0-100) do torrent que o RD ficou baixando, quando
// ele nao ficou pronto nas tres olhadas; a escolha manual mostra isso.
// A TORRENT ALREADY IN THE ACCOUNT (owner 25/11/2026). Every addMagnet makes
// a new entry on Real-Debrid, even for a hash the account already has, so each
// search and each reopen used to add the same torrent again. rdVisto keeps the
// hash -> torrent id of what this app added or found in the account list
// (debrid_conta_achar); resolverRD first asks that torrent for the file and
// adds the magnet only when it cannot serve it (gone, another file selected).
static int rdVisto(const char *hash, char *id, unsigned n);
static void rdLembrar(const char *hash, const char *id);
static void rdEsquecer(const char *hash);
static int rdLinkDoItem(const char *id, const char *arquivo, uint64_t bytes, int fileIdx, char *url, unsigned n,
                        int *sumiu);

static int resolverRD(const char *infoHash, int fileIdx, char *url, unsigned n,
                      int *pct) {
  char corpo[12400], enc[12300], magnet[4096], arquivo[512], rota[120], tid[64], status[32], link[600];
  char *r; int st = 0, id, tent;
  const char *files, *links, *el;

  magnetMontar(infoHash, fileIdx, NULL, magnet, sizeof magnet);
  dicaDe(infoHash, fileIdx, arquivo, sizeof arquivo, NULL, 0);
  if (rdVisto(infoHash, tid, sizeof tid)) {
    int sumiu = 0;
    if (rdLinkDoItem(tid, arquivo, 0, fileIdx, url, n, &sumiu) == 1) {
      printf("[debrid] Real-Debrid: %s already in the account, no new addMagnet\n", infoHash);
      return 1;
    }
    if (sumiu) rdEsquecer(infoHash);
  }
  urlenc(enc, sizeof enc, magnet);
  snprintf(corpo, sizeof corpo, "magnet=%s", enc);
  r = post_form(RD, "torrents/addMagnet", SRD, corpo, &st);
  if (!ok2xx(r, st) || !js_texto(r, NULL, "id", tid, sizeof tid)) {
    int v = falha(SRD, "addMagnet", st, r); free(r); return v;
  }
  free(r);
  rdLembrar(infoHash, tid);

  snprintf(rota, sizeof rota, "torrents/info/%s", tid);
  r = get_auth(RD, rota, SRD, &st);
  if (!ok2xx(r, st) || !(files = js_array(r, NULL, "files"))) {
    int v = falha(SRD, "info", st, r); free(r); return v;
  }
  el = escolherArquivo(files, fileIdx, "path", "bytes", arquivo);
  id = el ? (int)js_num(el, js_fim(el), "id", -1) : -1;
  free(r);
  if (id < 0) { printf("[debrid] torrent sem video utilizavel\n"); return 0; }

  snprintf(rota, sizeof rota, "torrents/selectFiles/%s", tid);
  snprintf(corpo, sizeof corpo, "files=%d", id);
  r = post_form(RD, rota, SRD, corpo, &st);
  if (!(st == 204 || st == 202 || (st >= 200 && st < 300))) {
    int v = falha(SRD, "selectFiles", st, r); free(r); return v;
  }
  free(r);

  // Em cache o RD marca "downloaded" quase na hora; fora de cache ele
  // comecaria a BAIXAR — e isso nao e "tocar agora". Tres olhadas e desiste.
  link[0] = 0;
  for (tent = 0; tent < 3 && !link[0]; tent++) {
    snprintf(rota, sizeof rota, "torrents/info/%s", tid);
    r = get_auth(RD, rota, SRD, &st);
    if (ok2xx(r, st) && pct) *pct = (int)js_num(r, NULL, "progress", -1);
    if (ok2xx(r, st) && js_texto(r, NULL, "status", status, sizeof status)
        && !strcmp(status, "downloaded") && (links = js_array(r, NULL, "links"))
        && *links == '"') {
      const char *fim = strchr(links + 1, '"');
      if (fim && (size_t)(fim - links - 1) < sizeof link) {
        memcpy(link, links + 1, (size_t)(fim - links - 1)); link[fim - links - 1] = 0;
      }
    } else if (r && !ok2xx(r, st)) {
      // so o nao-2xx para: 2xx "ainda nao baixou" e o caso das tres olhadas
      int v = falha(SRD, "info", st, r); free(r); return v;
    }
    free(r);
    if (!link[0]) sleep(1);
  }
  if (!link[0]) {
    printf("[debrid] %s nao esta em cache no Real-Debrid\n", infoHash);
    // ponytail: o torrent fica na lista do RD. Apagar exige DELETE
    // /torrents/delete/<id>, e rede_apagar ja existe — mas remover aqui muda o
    // comportamento visivel da conta de quem usa, e isso e decisao de quem
    // manda, nao efeito colateral de uma correcao de resolvedor.
    return FORA;
  }

  urlenc(enc, sizeof enc, link);
  snprintf(corpo, sizeof corpo, "link=%s", enc);
  r = post_form(RD, "unrestrict/link", SRD, corpo, &st);
  if (!ok2xx(r, st) || !js_texto(r, NULL, "download", url, n)) {
    int v = falha(SRD, "unrestrict", st, r); free(r); return v;
  }
  free(r);
  return 1;
}

// ---------------------------------------------------------------- TorBox
//
// Rotas (api.torbox.app/v1/api, documentacao oficial em api-docs.torbox.app e
// o SDK oficial torbox-sdk-py/src/torbox_api/services/torrents.py):
//   GET  torrents/checkcached?hash=&format=list   (get_torrent_cached_availability)
//   POST torrents/createtorrent                   (create_torrent)
//   GET  torrents/mylist?id=&bypass_cache=true    (get_torrent_list)
//   GET  torrents/requestdl?token=&torrent_id=&file_id=  (request_download_link)
//
// O createtorrent vai como multipart/form-data e NAO como urlencoded: e o que
// o SDK oficial manda (set_body(request_body, "multipart/form-data")) e a
// documentacao nao promete que o outro formato sirva. Montar o multipart a mao
// aqui e feio, mas e o unico formato confirmado — e chutar urlencoded daria um
// 4xx que so aparece na casa de quem tem TorBox.
#define BND "----nuvio-debrid"

// `soCache` 1 = add_only_if_cached (o automatico); 0 = a pessoa escolheu o
// torrent fora de cache e o TorBox deve BAIXAR — e o "P2P" do TorBox.
static char *tbCriarCom(const char *chaveTB, const char *magnet, int soCache, int *st) {
  char url[300], auth[260], corpo[5400];   // magnet com dn e trackers: ate 4 KB
  const char *cab[3];
  snprintf(url, sizeof url, TB "/torrents/createtorrent");
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", chaveTB);
  cab[0] = auth;
  cab[1] = "Content-Type: multipart/form-data; boundary=" BND;
  cab[2] = NULL;
  // add_only_if_cached=true e o que garante a regra desta casa: o servico
  // RECUSA o que nao esta em cache em vez de comecar a baixar. A consulta de
  // cache acima ja filtrou, mas entre uma chamada e outra o item pode sair do
  // cache, e sem isto a TV ficaria esperando um download comecar.
  snprintf(corpo, sizeof corpo,
           "--" BND "\r\nContent-Disposition: form-data; name=\"magnet\"\r\n\r\n%s\r\n"
           "--" BND "\r\nContent-Disposition: form-data; name=\"add_only_if_cached\"\r\n\r\n%s\r\n"
           // allow_zip=false: como o NuvioTV (TorboxDirectDebridResolver.kt)
           "--" BND "\r\nContent-Disposition: form-data; name=\"allow_zip\"\r\n\r\nfalse\r\n"
           "--" BND "--\r\n", magnet, soCache ? "true" : "false");
  return rede_postar_st(url, 15, cab, corpo, st);
}
static char *tb_criar(const char *magnet, int soCache, int *st) {
  if (!contextoVale()) { if (st) *st = 0; return NULL; }
  return tbCriarCom(chaveDoFio(STB), magnet, soCache, st);
}

// PRONTO PARA requestdl? Em cache o mylist ja vem com download_finished (ou
// download_present) true. Torrent que o TorBox ainda baixa traz a lista de
// arquivos assim que le os metadados — e o requestdl dele falha. Por isso,
// quando a pessoa mandou baixar, arquivo listado nao basta: vale a bandeira.
static int tbPronto(const char *r) {
  char b[16];
  return (js_bruto(r, NULL, "download_finished", b, sizeof b) && !strncmp(b, "true", 4))
      || (js_bruto(r, NULL, "download_present", b, sizeof b) && !strncmp(b, "true", 4));
}

// `baixar` 0: so em cache (o automatico). 1: a pessoa escolheu o torrent e o
// TorBox deve baixar o que nao tem — sem checkcached (quem chama ja
// perguntou) e sem add_only_if_cached. `pct` recebe o progresso quando o
// torrent ainda esta baixando.
// FALLBACK DO PAINEL (owner 19/11/2026): the normal resolve had an issue
// (createtorrent refused, the file list not there yet, requestdl failed).
// The file is looked up as it is on the TorBox dashboard: the account's
// torrents (fresh lists, up to 10 pages) by this hash, then its file and a
// requestdl. Not in the account -> 0 and the source is dropped, as before:
// something not cached is never downloaded. `tidVisto` = the item the normal
// resolve already tried (its files are read again, fresh).
static int tbRequestdl(int tid, int fid, char *url, unsigned n);
static int tbDoPainel(const char *h, int fileIdx, const char *arquivo, int tidVisto, const char *porque,
                      char *url, unsigned n) {
  int pg, tid = -1, fid = -1, st = 0;
  char *r = NULL;
  for (pg = 0; pg < 10 && tid < 0 && contextoVale(); pg++) {
    const char *el; int itens = 0;
    r = debrid_tb_mylist(0, 0, pg, 1, &st);
    if (!ok2xx(r, st)) { free(r); r = NULL; break; }
    for (el = js_array(r, NULL, "data"); el && *el == '{'; el = js_prox(js_fim(el)), itens++) {
      char hh[64] = "";
      if (js_texto_raiz_em(el, js_fim(el), "hash", hh, sizeof hh) && !strcasecmp(hh, h)) {
        const char *files = js_array(el, js_fim(el), "files"), *f;
        tid = (int)js_num(el, js_fim(el), "id", -1);
        if (files && (f = escolherArquivo(files, fileIdx, "name", "size", arquivo)) != NULL)
          fid = (int)js_num(f, js_fim(f), "id", -1);
        break;
      }
    }
    free(r); r = NULL;
    if (itens < DEBRID_TB_PAGINA) break;
  }
  if (tid < 0) {
    printf("[debrid] TorBox: %s %s; not on your TorBox dashboard either: source dropped\n", h, porque);
    return 0;
  }
  if (fid < 0) {
    printf("[debrid] TorBox: %s %s; on your dashboard (item %d%s) but no usable video yet: source dropped\n",
           h, porque, tid, tid == tidVisto ? ", the one just added" : "");
    return 0;
  }
  printf("[debrid] TorBox: %s %s; found on your TorBox dashboard (item %d, file %d): playing that\n", h, porque, tid, fid);
  return tbRequestdl(tid, fid, url, n);
}

static int resolverTB(const char *infoHash, int fileIdx, char *url, unsigned n,
                      int baixar, int *pct) {
  char h[80], magnet[4096], arquivo[512], rota[600];
  char *r; int st = 0, fid = -1, tent, tid, pronto = !baixar;
  const char *files, *el;

  hashMin(h, sizeof h, infoHash);
  if (baixar) goto criar;

  // 1) So conteudo JA EM CACHE toca na hora. `data` volta como lista de
  //    {name,size,hash}; lista ausente ou vazia significa fora de cache, e ai
  //    nao se pede nada ao servico.
  snprintf(rota, sizeof rota, "torrents/checkcached?hash=%s&format=list", h);
  r = get_auth(TB, rota, STB, &st);
  // Nao-2xx aqui NAO e "fora de cache": era como o 1.4.0 escrevia, e uma chave
  // recusada (401/403) aparecia no log como se o conteudo so nao estivesse la.
  if (!ok2xx(r, st)) { int v = falha(STB, "checkcached", st, r); free(r); return v; }
  if (!js_array(r, NULL, "data")) {
    printf("[debrid] TorBox: %s fora de cache (HTTP %d)\n", h, st);
    free(r); return FORA;
  }
  free(r);

criar:
  magnetMontar(h, fileIdx, infoHash, magnet, sizeof magnet);
  dicaDe(infoHash, fileIdx, arquivo, sizeof arquivo, NULL, 0);
  r = tb_criar(magnet, !baixar, &st);
  tid = ok2xx(r, st) ? (int)js_num(r, NULL, "torrent_id", -1) : -1;
  // O 403 DO REGISTRO 1541 SAI AQUI. A linha continua comecando por
  // "TorBox createtorrent: HTTP" (o que se procura no D1), agora com o corpo.
  //
  // HIPOTESE DESCARTADA ANTES DE ESCREVER ISTO: "a libcurl do app nao manda
  // User-Agent e o WAF do TorBox recusa cliente sem UA" (medido com o Trakt:
  // 401 com UA, 403 sem). Nao se aplica — rede.c:617 (GET) e rede.c:768
  // (POST com status) poem "Nuvio/1.0 (webOS)" em TODA requisicao da libcurl,
  // e no Tizen o XHR nao deixa definir UA e o navegador manda o dele. Se o
  // corpo novo vier como pagina HTML de WAF em vez do JSON do TorBox, a
  // pergunta volta a ser o UA (o valor, nao a ausencia).
  // Mandado baixar com a conta no limite de downloads ativos, o TorBox ENFILA
  // o torrent e devolve `queued_id` no lugar de `torrent_id` (campo da
  // documentacao, NAO conferido contra uma resposta real). Nao e falha: e
  // "baixando", so que ainda na fila.
  if (tid < 0 && baixar && ok2xx(r, st) && js_num(r, NULL, "queued_id", -1) >= 0) {
    printf("[debrid] TorBox: %s na fila de downloads do TorBox; fica na conta\n", h);
    if (pct) *pct = -1;
    free(r); return DEBRID_BAIXANDO;
  }
  if (tid < 0) {
    // "Nao esta em cache" no createtorrent do automatico (o item saiu do
    // cache entre as duas chamadas) e fora de cache tambem, nao falha.
    int foraC = !baixar && r && (contem(r, "not cached") || contem(r, "not_cached"));
    int v = falha(STB, "createtorrent", st, r); free(r);
    if (!baixar && !foraC && v == 0 && tbDoPainel(h, fileIdx, arquivo, -1, "createtorrent had an issue", url, n)) return 1;
    return v == 0 && foraC ? FORA : v;
  }
  free(r);

  // 3) Os campos aqui sao "name"/"size", e nao "path"/"bytes" do RD — por isso
  //    escolherArquivo recebe os nomes. Tres olhadas de 1 s, como no RD: em
  //    cache a lista ja vem pronta, e se nao vier nao vale travar a TV.
  //    Mandado baixar: as mesmas tres olhadas, mas so vale o torrent PRONTO
  //    (tbPronto). O que o TorBox ainda baixa vira DEBRID_BAIXANDO com o
  //    progresso — a TV nao fica presa minutos num fio de rede, e o torrent
  //    fica na conta para a proxima escolha tocar.
  { char estado[48] = "";
    double prog = -1;
    for (tent = 0; tent < 3 && (fid < 0 || !pronto); tent++) {
      snprintf(rota, sizeof rota, "torrents/mylist?id=%d&bypass_cache=true", tid);
      r = get_auth(TB, rota, STB, &st);
      if (ok2xx(r, st)) {
        if (baixar) {
          pronto = tbPronto(r);
          prog = js_num(r, NULL, "progress", -1);
          js_texto(r, NULL, "download_state", estado, sizeof estado);
        }
        if ((files = js_array(r, NULL, "files"))
            && (el = escolherArquivo(files, fileIdx, "name", "size", arquivo)) != NULL)
          fid = (int)js_num(el, js_fim(el), "id", -1);
      } else if (r) {
        int v = falha(STB, "mylist", st, r); free(r);
        if (!baixar && v == 0 && tbDoPainel(h, fileIdx, arquivo, tid, "the new item's file list failed", url, n)) return 1;
        return v;
      }
      free(r);
      if (fid < 0 || !pronto) sleep(1);
    }
    if (baixar && !pronto) {
      // progress do TorBox e fracao 0-1; algum cliente antigo mandava 0-100
      int p = prog < 0 ? -1 : prog <= 1.0 ? (int)(prog * 100 + 0.5) : (int)prog;
      if (pct) *pct = p;
      printf("[debrid] TorBox: %s baixando no TorBox (%d%%, estado=%s); fica na conta, "
             "a proxima escolha toca quando terminar\n", h, p, estado[0] ? estado : "-");
      return DEBRID_BAIXANDO;
    }
  }
  if (fid < 0) {
    printf("[debrid] TorBox: sem video utilizavel em %s\n", h);
    // TorBox may need a while to list a just-added torrent's files.
    return !baixar && tbDoPainel(h, fileIdx, arquivo, tid, "no file list in 3 s", url, n) ? 1 : 0;
  }
  { int v = tbRequestdl(tid, fid, url, n);
    if (v == 0 && !baixar && tbDoPainel(h, fileIdx, arquivo, tid, "requestdl failed", url, n)) return 1;
    return v; }
}

// 4) ESTA E A UNICA ROTA QUE LEVA A CHAVE NA QUERY, e nao por escolha nossa:
//    request_download_link poe `token` em add_query, nao em cabecalho. Logo,
//    esta URL nunca pode ir para log nem inteira nem em pedaco — o que sai
//    no fim de debrid_resolver e so a resposta passada por rede_url_publica.
// 1 = `url` pronta; outro valor = o de falha().
static int tbRequestdl(int tid, int fid, char *url, unsigned n) {
  char *r; int st = 0;
  { char u[1200]; const char *cab[1];
    cab[0] = NULL;
    snprintf(u, sizeof u, TB "/torrents/requestdl?token=%s&torrent_id=%d&file_id=%d&redirect=false",
             chaveDoFio(STB), tid, fid);
    r = contextoVale() ? rede_baixar_st(u, 15, cab, &st) : NULL; }
  if (!ok2xx(r, st) || !js_texto_raiz(r, "data", url, n)) {
    // "requestdl" e nao a URL: a URL leva o token (ver acima)
    int v = falha(STB, "requestdl", st, r); free(r);
    if (url && n) url[0] = 0;
    return v;
  }
  free(r);
  return 1;
}

// ---------------------------------------------------------------- Premiumize
//
// Rotas (www.premiumize.me/api, documentacao oficial):
//   POST cache/check          items[]=<link>  -> {"status","response":[bool],...}
//   POST transfer/directdl    src=<link>      -> {"status","content":[{path,size,link}]}
//
// Nao ha passo de "selecionar arquivo" como no RD: o directdl ja devolve TODOS
// os arquivos do torrent com o link direto de cada um, e a escolha e local.

static int resolverPM(const char *infoHash, int fileIdx, char *url, unsigned n) {
  char magnet[4096], arquivo[512], enc[12300], corpo[12400], bruto[64];
  char *r; int st = 0, emCache;
  const char *cont, *el;

  magnetMontar(infoHash, fileIdx, NULL, magnet, sizeof magnet);
  dicaDe(infoHash, fileIdx, arquivo, sizeof arquivo, NULL, 0);
  urlenc(enc, sizeof enc, magnet);

  // O magnet inteiro como item, e nao o hash pelado: a documentacao descreve o
  // parametro como "links to check", e o directdl logo abaixo recebe esse
  // mesmo texto em `src` — usar a mesma forma nos dois evita que "esta em
  // cache" e "me da o link" falem de coisas diferentes.
  snprintf(corpo, sizeof corpo, "items%%5B%%5D=%s", enc);
  r = post_form(PM, "cache/check", SPM, corpo, &st);
  // O Premiumize responde erro de conta como 200 {"status":"error",
  // "message":...}, sem "response": falha() cobre os dois (nao-2xx cru, 2xx so
  // com a mensagem).
  if (!ok2xx(r, st) || !js_bruto(r, NULL, "response", bruto, sizeof bruto)) {
    int v = falha(SPM, "cache/check", st, r); free(r); return v;
  }
  // O array de "response" e de BOOLEANOS, e js_array so sabe abrir array de
  // objeto ou de texto — dai a leitura crua.
  emCache = 1;
  {
    const char *q = bruto;
    if (*q == '[') q++;
    while (*q && (unsigned char)*q <= ' ') q++;
    emCache = !strncmp(q, "true", 4);
  }
  free(r);
  if (!emCache) {
    printf("[debrid] Premiumize: %s fora de cache (HTTP %d)\n", infoHash, st);
    return FORA;
  }

  snprintf(corpo, sizeof corpo, "src=%s", enc);
  r = post_form(PM, "transfer/directdl", SPM, corpo, &st);
  if (!ok2xx(r, st) || !(cont = js_array(r, NULL, "content"))) {
    int v = falha(SPM, "directdl", st, r); free(r); return v;
  }
  el = escolherArquivo(cont, fileIdx, "path", "size", arquivo);
  // "link" e nao "stream_link": o segundo e a versao transcodificada, que nem
  // sempre existe e nem sempre e o arquivo que se pediu.
  if (!el || !js_texto(el, js_fim(el), "link", url, n)) {
    printf("[debrid] Premiumize: sem video utilizavel em %s\n", infoHash);
    free(r); return 0;
  }
  free(r);
  return 1;
}

// ---------------------------------------------------------------- AllDebrid
//
// Rotas (documentacao oficial, docs.alldebrid.com; a auth e o cabecalho
// "Authorization: Bearer", entao a chave NUNCA entra na URL):
//   POST v4/magnet/upload      magnets[]=<magnet>     -> data.magnets[0]{id,ready,error?}
//   POST v4.1/magnet/status    id=<id>                -> data.magnets{statusCode,downloaded,size}
//   POST v4/magnet/files       id[]=<id>              -> data.magnets[0].files (arvore n/s/l/e)
//   POST v4/link/unlock        link=<l>               -> data.link (ou data.delayed)
//   POST v4/link/delayed       id=<delayed>           -> data.status 2 = pronto, data.link
//   POST v4/magnet/delete      id=<id>
//   GET  v4/user                                      -> data.user{isPremium,premiumUntil}
//
// SEM CONSULTA DE CACHE SEPARADA. O RD/TorBox/Premiumize tem uma rota de "esta
// em cache?"; a do AllDebrid (magnet/instant) nao consta mais na documentacao
// atual. O jeito documentado e o proprio upload: `ready` true = "already
// available". Isso quer dizer que perguntar JA ADICIONA o magnet a conta (como
// o addMagnet do RD). Para nao encher o limite de 30 magnets ativos
// (MAGNET_TOO_MANY_ACTIVE) com cada candidata fora de cache do automatico, ver
// adDescartar.
//
// AS ROTAS FORAM LIDAS NA DOCUMENTACAO E TESTADAS CONTRA UMA REDE FALSA; nada
// disto foi medido contra uma conta AllDebrid real (hipotese ate alguem ter uma).

// Dois formatos de "urlencoded" para o mesmo campo: `id` simples e `id[]`.
static char *adPost(const char *rota, const char *corpo, int *st) {
  char r2[200];
  snprintf(r2, sizeof r2, "%s%c" AD_AGENTE, rota, strchr(rota, '?') ? '&' : '?');
  return post_form(AD, r2, SAD, corpo, st);
}

// Resposta 2xx de sucesso? O AllDebrid erra com HTTP 200 e "status":"error".
static int adOk(const char *r, int st) {
  char s[16];
  return ok2xx(r, st) && js_texto(r, NULL, "status", s, sizeof s) && !strcmp(s, "success");
}

// Buffer crescente para a lista achatada de arquivos.
typedef struct { char *p; size_t n, cap; int itens; } AdBuf;
static void adAdd(AdBuf *b, const char *t, size_t L) {
  if (!b->p) return;
  if (b->n + L + 1 > b->cap) {
    size_t c = (b->cap + L + 1) * 2;
    char *q = realloc(b->p, c);
    if (!q) { free(b->p); b->p = NULL; return; }
    b->p = q; b->cap = c;
  }
  memcpy(b->p + b->n, t, L); b->n += L; b->p[b->n] = 0;
}
static void adEsc(AdBuf *b, const char *t) {
  for (; *t; t++) {
    unsigned char c = (unsigned char)*t;
    if (c == '"' || c == '\\') { char e[2] = { '\\', (char)c }; adAdd(b, e, 2); }
    else if (c < ' ') adAdd(b, " ", 1);
    else adAdd(b, t, 1);
  }
}

// A arvore {"n":nome,"e":[...]} (pasta) / {"n","s","l"} (arquivo) vira a lista
// plana [{"path","size","link"}] em ORDEM DE DOCUMENTO, que e o que
// escolherArquivo sabe ler — a ordem de escolha continua escrita uma vez so.
// O indice do fileIdx do addon e o do torrent inteiro; a ordem da arvore do
// AllDebrid e HIPOTESE de que coincide (numa temporada em pastas pode nao).
#define AD_MAX_ARQUIVOS 2000
#define AD_PROF 8
static void adAchatar(const char *arr, AdBuf *b, int prof) {
  const char *p;
  for (p = arr; p && (*p == '{') && b->itens < AD_MAX_ARQUIVOS; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p), *sub = js_array(p, f, "e");
    char nome[600], link[900];
    if (sub) { if (prof < AD_PROF) adAchatar(sub, b, prof + 1); continue; }
    if (!js_texto(p, f, "n", nome, sizeof nome)) continue;
    if (!js_texto(p, f, "l", link, sizeof link)) continue;
    if (b->itens) adAdd(b, ",", 1);
    adAdd(b, "{\"path\":\"", 9); adEsc(b, nome);
    { char t[48]; snprintf(t, sizeof t, "\",\"size\":%.0f,\"link\":\"", js_num(p, f, "s", 0));
      adAdd(b, t, strlen(t)); }
    adEsc(b, link); adAdd(b, "\"}", 2);
    b->itens++;
  }
}

// Apaga um magnet que ESTE pedido acabou de criar e nao serve. So o que ainda
// nao baixou nada e esta na fila/comecando (statusCode 0 ou 1, 0 bytes): um
// magnet que a pessoa ja tinha na conta, andando, nao e apagado. E hipotese
// que o upload de um magnet ja existente devolve o id dele; se devolver outro
// o pior caso e apagar um download recem-comecado de 0 bytes.
static void adDescartar(const char *id) {
  char corpo[64], *r; int st = 0;
  snprintf(corpo, sizeof corpo, "id=%s", id);
  r = adPost("v4/magnet/delete", corpo, &st);
  free(r);
}

// link/unlock of a magnet file's link (`delayed` waited up to 3 s). 1 = `url`.
static int adDesbloquear(const char *lnk, char *url, unsigned n) {
  char enc[2800], corpo[2900], *r; int st = 0;
  urlenc(enc, sizeof enc, lnk);
  snprintf(corpo, sizeof corpo, "link=%s", enc);
  r = adPost("v4/link/unlock", corpo, &st);
  if (!adOk(r, st)) { falha(SAD, "unlock", st, r); free(r); return 0; }
  if (js_texto(r, NULL, "link", url, n)) { free(r); return 1; }
  // "delayed": links que precisam de tempo para ser gerados. Raro em torrent
  // (o link de arquivo pronto costuma sair na hora), mas e o contrato.
  { char did[48];
    if (js_bruto(r, NULL, "delayed", did, sizeof did)) {
      char *q = did; int t2;
      while (*q == '"') q++;
      { char *e = q; while (*e && *e != '"') e++; *e = 0; }
      free(r);
      for (t2 = 0; t2 < 3; t2++) {
        snprintf(corpo, sizeof corpo, "id=%s", q);
        r = adPost("v4/link/delayed", corpo, &st);
        if (adOk(r, st) && js_num(r, NULL, "status", 0) == 2 && js_texto(r, NULL, "link", url, n)) {
          free(r); return 1;
        }
        free(r);
        sleep(1);
      }
      if (url && n) url[0] = 0;
      return 0;
    } }
  falha(SAD, "unlock", st, r); free(r);
  return 0;
}

// The file of ready magnet `id` (magnet/files, the tree flattened) and its
// unlocked link. `bytes` > 0 = the add-on's size: name/size first
// (arquivoExato), then the usual order. `rotulo` names the torrent in the log.
static int adLinkDoMagnet(const char *id, int fileIdx, const char *arquivo, uint64_t bytes, char *url, unsigned n,
                          const char *rotulo) {
  char corpo[64], lnk[900], *r; int st = 0;
  const char *files, *el;
  AdBuf b = { NULL, 0, 0, 0 };
  if (url && n) url[0] = 0;
  snprintf(corpo, sizeof corpo, "id%%5B%%5D=%s", id);
  r = adPost("v4/magnet/files", corpo, &st);
  if (!adOk(r, st) || !(files = js_array(r, NULL, "files"))) {
    int v = falha(SAD, "files", st, r); free(r); return v;
  }
  b.p = malloc(4096); b.cap = 4096;
  if (b.p) { b.p[0] = '['; b.p[1] = 0; b.n = 1; }
  adAchatar(files, &b, 0);
  free(r);
  if (!b.p) return 0;
  adAdd(&b, "]", 1);
  if (!b.p) return 0;
  el = arquivoExato(b.p + 1, "path", "size", arquivo, bytes);
  if (!el) el = escolherArquivo(b.p + 1, fileIdx, "path", "size", arquivo);
  if (!el || !js_texto(el, js_fim(el), "link", lnk, sizeof lnk)) {
    printf("[debrid] AllDebrid: sem video utilizavel em %s\n", rotulo ? rotulo : "-");
    free(b.p); return 0;
  }
  free(b.p);
  return adDesbloquear(lnk, url, n);
}

// `manual` 0 (automatico): so o que ja esta pronto (`ready`). 1: a pessoa
// escolheu o torrent; se nao esta pronto, o AllDebrid segue baixando e devolve
// DEBRID_BAIXANDO com o progresso, que fica na conta para a proxima escolha.
static int resolverAD(const char *infoHash, int fileIdx, char *url, unsigned n,
                      int manual, int *pct) {
  char corpo[12400], enc[12300], magnet[4096], arquivo[512], id[32], sub[16], h[80];
  char *r; int st = 0, tent, sc = -1, pronto;

  hashMin(h, sizeof h, infoHash);      // o servico compara TEXTO, como o TorBox
  magnetMontar(h, fileIdx, infoHash, magnet, sizeof magnet);
  dicaDe(infoHash, fileIdx, arquivo, sizeof arquivo, NULL, 0);
  urlenc(enc, sizeof enc, magnet);
  snprintf(corpo, sizeof corpo, "magnets%%5B%%5D=%s", enc);
  r = adPost("v4/magnet/upload", corpo, &st);
  if (!adOk(r, st)) { int v = falha(SAD, "upload", st, r); free(r); return v; }
  // erro POR MAGNET (MAGNET_INVALID_URI...) vem dentro de magnets[0]
  // O id do AllDebrid e NUMERO no JSON ("id":123), nao texto.
  if (js_texto(r, NULL, "code", sub, sizeof sub) || js_num(r, NULL, "id", -1) < 0) {
    int v = falha(SAD, "upload", st, r); free(r); return v;
  }
  snprintf(id, sizeof id, "%.0f", js_num(r, NULL, "id", -1));
  { char rd[16];
    pronto = js_bruto(r, NULL, "ready", rd, sizeof rd) && !strncmp(rd, "true", 4); }
  free(r);

  if (!pronto && !manual) {
    // fora de cache: o automatico nao espera. Descarta o que acabou de criar
    // (ver adDescartar) e diz "fora de cache".
    int sc0 = -1; double baixado = 0;
    snprintf(corpo, sizeof corpo, "id=%s", id);
    r = adPost("v4.1/magnet/status", corpo, &st);
    if (adOk(r, st)) { sc0 = (int)js_num(r, NULL, "statusCode", -1); baixado = js_num(r, NULL, "downloaded", 0); }
    free(r);
    if (sc0 >= 0 && sc0 <= 1 && baixado <= 0) adDescartar(id);
    printf("[debrid] AllDebrid: %s fora de cache\n", infoHash);
    return FORA;
  }

  // statusCode 4 = Ready. 5-15 = erro do torrent (sem peer, muito grande, 20
  // min sem baixar...). Tres olhadas de 1 s, como os outros; em cache a
  // primeira ja diz 4.
  for (tent = 0; tent < 3; tent++) {
    snprintf(corpo, sizeof corpo, "id=%s", id);
    r = adPost("v4.1/magnet/status", corpo, &st);
    if (!adOk(r, st)) { int v = falha(SAD, "status", st, r); free(r); return v; }
    sc = (int)js_num(r, NULL, "statusCode", -1);
    if (pct) {
      double tam = js_num(r, NULL, "size", 0), bx = js_num(r, NULL, "downloaded", 0);
      *pct = tam > 0 ? (int)(bx * 100.0 / tam) : -1;
    }
    free(r);
    if (sc == 4) break;
    if (sc >= 5) {
      printf("[debrid] AllDebrid: %s falhou no servico (statusCode %d)\n", infoHash, sc);
      return 0;
    }
    if (tent < 2) sleep(1);
  }
  if (sc != 4) {
    if (!manual) return FORA;
    printf("[debrid] AllDebrid: %s baixando (%d%%, statusCode %d); fica na conta, "
           "a proxima escolha toca quando terminar\n", infoHash, pct ? *pct : -1, sc);
    return DEBRID_BAIXANDO;
  }

  return adLinkDoMagnet(id, fileIdx, arquivo, 0, url, n, infoHash);
}

// "Testar chave" (Ajustes): GET v4/user. Devolve 1 e a frase da conta em
// `msg` (nunca a chave, nem o e-mail/usuario: esta tela vira foto de issue),
// ou 0 com o motivo. BLOQUEIA; quem chama usa um fio.
static int testarAllDebrid(char *msg, unsigned n, char *data, unsigned nd) {
  char *r; int st = 0;
  if (msg && n) msg[0] = 0;
  if (data && nd) data[0] = 0;
  if (!chaveDoFio(SAD)[0]) { if (msg && n) snprintf(msg, n, "sem chave"); return 0; }
  r = get_auth(AD, "v4/user?" AD_AGENTE, SAD, &st);
  if (!r) { if (msg && n) snprintf(msg, n, "sem resposta do servidor"); return 0; }
  if (!adOk(r, st)) {
    if (msg && n) snprintf(msg, n, "%s",
        strstr(r, "AUTH_BAD_APIKEY") || strstr(r, "AUTH_MISSING_APIKEY") ? "chave recusada"
        : strstr(r, "AUTH_BLOCKED") ? "bloqueada por região ou IP" : "sem resposta válida");
    falha(SAD, "user", st, r);
    free(r); return 0;
  }
  { char b[16]; double ate = js_num(r, NULL, "premiumUntil", 0);
    int premium = js_bruto(r, NULL, "isPremium", b, sizeof b) && !strncmp(b, "true", 4);
    if (premium && ate > 0) {
      time_t t = (time_t)ate; struct tm *m = localtime(&t); char d[16] = "";
      if (m) strftime(d, sizeof d, "%d/%m/%Y", m);
      // `msg` e chave de i18n; com data, quem mostra usa "premium até %s"
      if (msg && n) snprintf(msg, n, "premium");
      if (data && nd) snprintf(data, nd, "%s", d);
    } else if (premium) { if (msg && n) snprintf(msg, n, "premium"); }
    else if (msg && n) snprintf(msg, n, "conta sem premium");
    free(r);
    return premium; }
}

int debrid_testar_alldebrid(char *msg, unsigned n, char *data, unsigned nd) {
  DebridContexto c;
  int abriu = contextoAbrir(&c);
  int r = testarAllDebrid(msg, n, data, nd);
  if (!contextoVale()) { r = 0; if (msg && n) msg[0] = 0; if (data && nd) data[0] = 0; }
  if (abriu) debrid_contexto_sair();
  return r;
}

// RD e TorBox usam Bearer; Premiumize recebe apikey no formulario. Nenhum corpo de conta vai ao log (pode conter e-mail/token).
int debrid_testar_conta(int q, char *msg, unsigned n, char *data, unsigned nd) {
  int e = 0;
  time_t agora = time(NULL), ate = 0;
  if (msg && n) msg[0] = 0;
  if (data && nd) data[0] = 0;
  if (q == SAD) return debrid_testar_alldebrid(msg, n, data, nd);
  if (q < 0 || q >= SN) return 0;
  pthread_mutex_lock(&consultaMutex);
  pthread_mutex_lock(&contaMutex);
  if (!chave[q][0]) {
    if (msg && n) snprintf(msg, n, "sem chave");
    pthread_mutex_unlock(&contaMutex);
    pthread_mutex_unlock(&consultaMutex); return 0;
  }
  if (!contaConsultada[q] || agora - contaConsultada[q] >= 300) {
    char *r, b[64] = "", status[32] = "", k[200], auth[260];
    const char *cab[] = { auth, "Content-Type: application/x-www-form-urlencoded", NULL };
    int st = 0;
    snprintf(k, sizeof k, "%s", chave[q]);
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", k);
    pthread_mutex_unlock(&contaMutex);
    if (q == SPM) {
      char enc[600], corpo[620];
      urlenc(enc, sizeof enc, k);
      snprintf(corpo, sizeof corpo, "apikey=%s", enc);
      r = rede_postar_st(PM "/account/info", 15, cab, corpo, &st);
    } else r = rede_baixar_st(q == SRD ? RD "/user" : TB "/user/me?settings=false", 15, cab, &st);
    if (st == 401 || (r && (strstr(r, "BAD_TOKEN") || strstr(r, "Invalid API key")
        || strstr(r, "Invalid API Key") || strstr(r, "Not logged in")))) e = 3;
    else if (ok2xx(r, st)) {
      if (q == SRD && js_texto(r, NULL, "type", status, sizeof status)
          && js_tem(r, NULL, "premium") && js_texto(r, NULL, "expiration", b, sizeof b)) {
        ate = (time_t)(js_ms_iso(b) / 1000);
        if (ate) e = !strcmp(status, "premium") && js_num(r, NULL, "premium", 0) > 0 && ate > agora ? 1 : 2;
        if (!strcmp(status, "free")) { e = 2; ate = 0; }
      } else if (q == SPM && js_texto(r, NULL, "status", status, sizeof status)
                 && !strcmp(status, "success") && js_tem(r, NULL, "premium_until")) {
        ate = (time_t)js_num(r, NULL, "premium_until", -1);
        if (ate >= 0) e = ate > agora ? 1 : 2;
      } else if (q == STB && js_bruto(r, NULL, "success", status, sizeof status)
                 && !strcmp(status, "true") && js_num(r, NULL, "plan", -1) >= 0) {
        if (js_num(r, NULL, "plan", -1) == 0) e = 2;
        else if (js_texto(r, NULL, "premium_expires_at", b, sizeof b)) {
          ate = (time_t)(js_ms_iso(b) / 1000);
          if (ate) e = ate > agora ? 1 : 2;
        }
      }
    }
    free(r);
    pthread_mutex_lock(&contaMutex);
    // A pessoa pode trocar a chave ou sair da conta durante a consulta.
    if (strcmp(k, chave[q])) {
      pthread_mutex_unlock(&contaMutex);
      pthread_mutex_unlock(&consultaMutex);
      if (msg && n) snprintf(msg, n, "sem resposta válida");
      return 0;
    }
    contaConsultada[q] = agora; contaAte[q] = ate; contaEstado[q] = e;
    if (e) {
      atomic_store(&semPlano[q], e == 1 ? 0 : e == 3 ? 2 : 1);
      if (e == 1) atomic_store(&avisado[q], 0);
    }
  }
  e = contaEstado[q]; ate = contaAte[q];
  // Mesmo dentro do TTL, nao deixa passar um vencimento conhecido.
  if (e == 1 && ate && ate <= agora) {
    e = contaEstado[q] = 2; atomic_store(&semPlano[q], 1);
  }
  if (msg && n) snprintf(msg, n, "%s", e == 1 ? "premium" : e == 2 ? (ate > 0 ? "vencida" : "conta sem premium")
                                               : e == 3 ? "chave recusada" : "sem resposta válida");
  if (e == 1 && ate && data && nd) {
    struct tm m;
    if (localtime_r(&ate, &m)) strftime(data, nd, "%d/%m/%Y", &m);
  }
  pthread_mutex_unlock(&contaMutex);
  pthread_mutex_unlock(&consultaMutex);
  return e == 1;
}

// ---------------------------------------------------------------- resolver

// Uma volta pelos servicos com chave, na ORDEM FIXA. `baixarTB` 1 = o TorBox
// entra no modo "baixar" (escolha manual, segunda volta); `so` >= 0 limita a
// volta a um servico. Devolve 1 (url pronta), DEBRID_BAIXANDO (quem e quanto
// em *qb/*pct), ou 0. `*fora` recebe a mascara dos servicos que responderam
// "fora de cache".
static int volta(const char *infoHash, int fileIdx, char *url, unsigned n,
                 int baixarTB, int so, int *qb, int *pct, int *fora, int *winner) {
  int q;
  const DebridContexto *ctx = contextoDoFio();
  unsigned g = ctx ? ctx->busca : 0;
  url[0] = 0;

  // ORDEM FIXA: Real-Debrid, TorBox, Premiumize; ganha o PRIMEIRO QUE
  // RESOLVER, nao o primeiro que tem chave. Quem tem duas contas costuma ter
  // uma principal, e daqui nao ha como saber qual — entao a ordem e sempre a
  // mesma (previsivel no log, que e o que se le no relato de defeito) e cada
  // um so custa uma consulta de cache quando nao tem o conteudo.
  for (q = 0; q < SN; q++) {
    int deu, p = -1;
    if (!contextoVale()) { url[0] = 0; return 0; }
    if (!chaveDoFio(q)[0] || (so >= 0 && q != so)) continue;
    // Recusado pela conta nesta busca: nem tenta, passa ao proximo servico.
    // E o que faltava no registro 1541 — o Premiumize so era perguntado
    // depois de cada 403 do TorBox, torrent por torrent.
    if (q != SAD) debrid_testar_conta(q, NULL, 0, NULL, 0);
    if (atomic_load(&recusado[q]) || atomic_load(&semPlano[q])) continue;
    url[0] = 0;
    // `qb != NULL` e a escolha MANUAL (ver debrid_resolver_escolhido): o
    // AllDebrid so deixa o download andar quando a pessoa escolheu o torrent.
    deu = (q == SRD) ? resolverRD(infoHash, fileIdx, url, n, &p)
        : (q == STB) ? resolverTB(infoHash, fileIdx, url, n, baixarTB, &p)
        : (q == SAD) ? resolverAD(infoHash, fileIdx, url, n, qb != NULL, &p)
                     : resolverPM(infoHash, fileIdx, url, n);
    if (!contextoVale()) { url[0] = 0; return 0; }
    if (deu < 0) {
      int zero = 0;
      // so o primeiro fio a ver a recusa escreve a linha, e so se a busca
      // ainda e a mesma (ver `geracao`)
      pthread_mutex_lock(&chaveTrava);
      int recusou = contextoValeSemTrava(ctx) && atomic_load(&geracao) == g
          && atomic_compare_exchange_strong(&recusado[q], &zero, -deu);
      pthread_mutex_unlock(&chaveTrava);
      if (recusou)
        printf("[debrid] %s: recusa da conta (HTTP %d), fora do resto desta busca\n",
               nomeServ[q], -deu);
      url[0] = 0;
      continue;
    }
    if (deu == FORA) {
      if (fora) *fora |= 1 << q;
      // O Real-Debrid ja COMECOU a baixar (addMagnet + selectFiles): para a
      // escolha manual isso e "baixando", com o progresso que ele deu.
      if (q == SRD && qb && *qb < 0) { *qb = SRD; if (pct) *pct = p; }
      url[0] = 0;
      continue;
    }
    if (deu == DEBRID_BAIXANDO) {
      if (qb && *qb < 0) { *qb = q; if (pct) *pct = p; }
      url[0] = 0;
      continue;
    }
    if (deu && url[0]) {
      // O CAMINHO DESTA URL E A CREDENCIAL: e o link direto que o servico
      // devolve, e quem o tem baixa usando a conta de quem pediu. Ver
      // rede_url_publica em rede.h. Vale para os tres — o /d/<chave>/ do RD,
      // o link assinado do TorBox e o do Premiumize.
      char seg[120];
      printf("[debrid] %s: %s -> %s\n", nomeServ[q], infoHash,
             rede_url_publica(url, seg, sizeof seg));
      if (winner) *winner = q;
      return 1;
    }
    url[0] = 0;
  }
  return qb && *qb >= 0 ? DEBRID_BAIXANDO : 0;
}

int debrid_resolver_servico_cache(int servico, const char *infoHash, int fileIdx,
                                  char *url, unsigned n, int *foraResultado) {
  DebridContexto c;
  int fora = 0, r = 0, abriu;
  if (foraResultado) *foraResultado = 0;
  if (url && n) url[0] = 0;
  if (servico < 0 || servico >= SN || !infoHash || !*infoHash || !url || !n) return 0;
  abriu = contextoAbrir(&c);
  if (debrid_servico_disponivel(servico))
    r = volta(infoHash, fileIdx, url, n, 0, servico, NULL, NULL, &fora, NULL);
  pthread_mutex_lock(&chaveTrava);
  if (!contextoValeSemTrava(contextoDoFio())) { r = 0; fora = 0; url[0] = 0; }
  if (r != 1 && (fora & (1 << servico))) {
    atomic_fetch_add(&foraCache, 1);
    if (foraResultado) *foraResultado = 1;
  }
  pthread_mutex_unlock(&chaveTrava);
  if (abriu) debrid_contexto_sair();
  return r == 1;
}

int debrid_resolver_cache(const char *infoHash, int fileIdx, char *url, unsigned n, int *foraResultado) {
  DebridContexto c;
  int fora = 0, r, abriu;
  if (foraResultado) *foraResultado = 0;
  if (url && n) url[0] = 0;
  if (!infoHash || !*infoHash || !url || n == 0) return 0;
  abriu = contextoAbrir(&c);
  r = volta(infoHash, fileIdx, url, n, 0, -1, NULL, NULL, &fora, NULL);
  pthread_mutex_lock(&chaveTrava);
  if (!contextoValeSemTrava(contextoDoFio())) { r = 0; fora = 0; url[0] = 0; }
  if (r != 1 && fora) atomic_fetch_add(&foraCache, 1);
  pthread_mutex_unlock(&chaveTrava);
  if (foraResultado && r != 1 && fora) {
    int ativos = 0;
    for (int q = 0; q < SN; q++) if (chaveDoFio(q)[0]) ativos |= 1 << q;
    // Negativa de um servico + erro/sem resposta de outro ainda e desconhecido.
    *foraResultado = (fora & ativos) == ativos;
  }
  if (abriu) debrid_contexto_sair();
  return r == 1;
}
int debrid_resolver(const char *infoHash, int fileIdx, char *url, unsigned n) {
  return debrid_resolver_cache(infoHash, fileIdx, url, n, NULL);
}

int debrid_resolver_escolhido(const char *infoHash, int fileIdx, char *url,
                              unsigned n, char *servico, unsigned ns, int *pct) {
  DebridContexto c;
  int fora = 0, qb = -1, p = -1, r, abriu, winner = -1;
  if (servico && ns) servico[0] = 0;
  if (pct) *pct = -1;
  if (url && n) url[0] = 0;
  if (!infoHash || !*infoHash || !url || n == 0) return 0;
  abriu = contextoAbrir(&c);
  printf("[debrid] escolha manual do torrent %s: em cache primeiro, depois baixar\n", infoHash);
  r = volta(infoHash, fileIdx, url, n, 0, -1, &qb, &p, &fora, &winner);
  if (r != 1 && (fora & (1 << STB)) && contextoVale()) {
    int qb2 = -1, p2 = -1;
    r = volta(infoHash, fileIdx, url, n, 1, STB, &qb2, &p2, NULL, &winner);
    if (r == DEBRID_BAIXANDO) { qb = qb2; p = p2; }
  }
  if (!contextoVale()) { r = 0; qb = -1; url[0] = 0; }
  if (r == 1 && servico && ns && winner >= 0 && winner < SN)
    snprintf(servico, ns, "%s", nomeServ[winner]);
  if (r != 1 && qb >= 0) {
    if (servico && ns) snprintf(servico, ns, "%s", nomeServ[qb]);
    if (pct) *pct = p;
    r = DEBRID_BAIXANDO;
  }
  if (abriu) debrid_contexto_sair();
  return r == 1 || r == DEBRID_BAIXANDO ? r : 0;
}

// ---------------------------------------------------------------- TorBox: arquivos de uma fonte
//
// A LISTA DE ARQUIVOS DE UMA FONTE (partes.h, dono 16/10/2026): o addon manda
// UM link por titulo, e um filme cortado em 4 episodios dentro do mesmo
// torrent aparecia so com a parte 1. Estas quatro chamadas leem o item na
// conta TorBox da pessoa e pedem o link de qualquer arquivo dele.
//
// NAO DEPENDEM DA BUSCA EM CURSO (contextoVale): a lista e pedida com a folha
// aberta, e o link da parte seguinte no meio do filme — uma busca nova (o
// Recarregar, a busca de fundo do Retomar) nao muda a conta nem a chave. A
// chave e copiada sob a trava na hora; logout a apaga e a chamada seguinte
// sai "sem chave".
static int contaEntrar(DebridContexto *c);
static void contaSair(int aberto);
static int tbChave(char *dst, size_t n) {
  int ok;
  pthread_mutex_lock(&chaveTrava);
  snprintf(dst, n, "%s", chave[STB]);
  ok = dst[0] != 0;
  pthread_mutex_unlock(&chaveTrava);
  return ok;
}

int debrid_tb_disponivel(void) {
  char k[200];
  if (!tbChave(k, sizeof k)) return 0;
  return atomic_load(&semPlano[STB]) ? -1 : 1;
}

char *debrid_tb_mylist(int usenet, int id, int pagina, int fresco, int *st) {
  char url[300], auth[260], k[200];
  const char *cab[2];
  if (st) *st = 0;
  if (!tbChave(k, sizeof k)) return NULL;
  if (id > 0)
    snprintf(url, sizeof url, TB "/%s/mylist?id=%d%s", usenet ? "usenet" : "torrents", id,
             fresco ? "&bypass_cache=true" : "");
  else
    snprintf(url, sizeof url, TB "/%s/mylist?offset=%d&limit=%d%s", usenet ? "usenet" : "torrents",
             pagina * DEBRID_TB_PAGINA, DEBRID_TB_PAGINA, fresco ? "&bypass_cache=true" : "");
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", k);
  cab[0] = auth; cab[1] = NULL;
  return rede_baixar_st(url, 20, cab, st);
}

char *debrid_tb_api_get(const char *rota, int prazo, int *st) {
  char url[1400], auth[260], k[200];
  const char *cab[2];
  if (st) *st = 0;
  if (!rota || !tbChave(k, sizeof k)) return NULL;
  snprintf(url, sizeof url, TB "/%s", rota);
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", k);
  cab[0] = auth; cab[1] = NULL;
  return rede_baixar_st(url, prazo > 0 ? prazo : 20, cab, st);
}

void debrid_tb_redigir(char *texto) {
  char k[200];
  size_t n;
  char *p;
  if (!texto || !tbChave(k, sizeof k)) return;
  n = strlen(k);
  if (n < 8) return;
  while ((p = strstr(texto, k)) != NULL) {
    memcpy(p, "***", 3);
    memmove(p + 3, p + n, strlen(p + n) + 1);
  }
}

// O requestdl leva o token NA QUERY (ver resolverTB): esta URL nunca vai para
// log. O que volta em `url` e o link do CDN (*.tb-cdn.st), que toca direto.
int debrid_tb_link(int usenet, int id, int arquivo, char *url, unsigned n) {
  char u[1200], k[200];
  const char *cab[1];
  char *r; int st = 0, ok;
  if (url && n) url[0] = 0;
  if (!url || n == 0 || id <= 0 || arquivo < 0 || !tbChave(k, sizeof k)) return 0;
  cab[0] = NULL;
  if (usenet)
    snprintf(u, sizeof u, TB "/usenet/requestdl?token=%s&usenet_id=%d&file_id=%d&redirect=false", k, id, arquivo);
  else
    snprintf(u, sizeof u, TB "/torrents/requestdl?token=%s&torrent_id=%d&file_id=%d&redirect=false", k, id, arquivo);
  r = rede_baixar_st(u, 15, cab, &st);
  ok = ok2xx(r, st) && js_texto_raiz(r, "data", url, n) && !strncmp(url, "http", 4);
  if (!ok) {
    // O texto do proprio TorBox (sem chave nem URL): "error"/"detail" dizem se
    // foi o item que sumiu, o plano, ou o servidor (dono, 16/10, log 19).
    char erro[96] = "", det[160] = "";
    if (r) { js_texto_raiz(r, "error", erro, sizeof erro); js_texto_raiz(r, "detail", det, sizeof det); }
    printf("[partes] TorBox requestdl (%s): HTTP %d%s%s%s%s%s\n", usenet ? "usenet" : "torrent", st,
           r && debrid_eh_sem_plano(st, r) ? " (conta sem plano para a API)" : "",
           erro[0] ? ", error=" : "", erro, det[0] ? ", detail=" : "", det);
    if (url && n) url[0] = 0;
  }
  free(r);
  return ok;
}

int debrid_tb_adicionar(const char *hash) {
  char h[80], magnet[300], k[200];
  char *r; int st = 0, tid;
  if (!hash || !*hash || !tbChave(k, sizeof k)) return -1;
  hashMin(h, sizeof h, hash);
  snprintf(magnet, sizeof magnet, "magnet:?xt=urn:btih:%s", h);
  r = tbCriarCom(k, magnet, 1, &st);
  tid = ok2xx(r, st) ? (int)js_num(r, NULL, "torrent_id", -1) : -1;
  if (tid < 0) {
    char erro[96] = "", det[160] = "";
    if (r) { js_texto_raiz(r, "error", erro, sizeof erro); js_texto_raiz(r, "detail", det, sizeof det); }
    printf("[partes] TorBox createtorrent (so em cache): HTTP %d%s%s%s%s\n", st,
           erro[0] ? ", error=" : "", erro, det[0] ? ", detail=" : "", det);
  }
  free(r);
  return tid;
}

// IS THE HASH CACHED ON TORBOX (tbabrir.c, owner 25/11/2026): asked before a
// stuck item is deleted, so a file TorBox no longer has cached is never taken
// out of the account (the re-add is cached only and would be refused).
// 1 cached, 0 not cached, -1 no answer / no key. BLOCKS.
int debrid_tb_em_cache(const char *hash) {
  char h[80], rota[200];
  char *r; int st = 0, v;
  if (!hash || strlen(hash) < 40) return -1;
  hashMin(h, sizeof h, hash);
  snprintf(rota, sizeof rota, "torrents/checkcached?hash=%s&format=list", h);
  { // get_auth needs a context: the recovery thread has none (it always
    // answered "no answer" before, so a stuck item was never re-added).
    DebridContexto c;
    int aberto = contaEntrar(&c);
    r = get_auth(TB, rota, STB, &st);
    contaSair(aberto); }
  if (!ok2xx(r, st)) { free(r); return -1; }
  v = js_array(r, NULL, "data") != NULL;
  free(r);
  return v;
}

// DELETE a torrent from the account (owner 24/11/2026, log 19/20, Fate PMR):
// an item TorBox kept listing but whose stream never sent a segment started
// working only after the owner removed it on the dashboard and Nuvio re-added
// it by its hash. POST torrents/controltorrent {torrent_id, operation:delete}.
// 1 = TorBox said yes. BLOCKS.
int debrid_tb_apagar(int item) {
  char url[300], auth[260], corpo[96], k[200], ok_[16];
  const char *cab[3];
  char *r; int st = 0, ok;
  if (item <= 0 || !tbChave(k, sizeof k)) return 0;
  snprintf(url, sizeof url, TB "/torrents/controltorrent");
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", k);
  cab[0] = auth; cab[1] = "Content-Type: application/json"; cab[2] = NULL;
  snprintf(corpo, sizeof corpo, "{\"torrent_id\":%d,\"operation\":\"delete\"}", item);
  r = rede_postar_st(url, 15, cab, corpo, &st);
  ok = ok2xx(r, st) && !(js_bruto(r, NULL, "success", ok_, sizeof ok_) && strncmp(ok_, "true", 4));
  { char erro[96] = "", det[160] = "";
    if (r) { js_texto_raiz(r, "error", erro, sizeof erro); js_texto_raiz(r, "detail", det, sizeof det); }
    printf("[partes] TorBox controltorrent delete %d: HTTP %d, %s%s%s%s%s\n", item, st, ok ? "ok" : "refused",
           erro[0] ? ", error=" : "", erro, det[0] ? ", detail=" : "", det);
    fflush(stdout); }
  free(r);
  return ok;
}

// AIRLOCK (api.torbox.app/openapi.json, EditTorrent/EditUsenetDownload): o item
// fica de fora da purga de 30 dias sem download. E o mesmo PUT de edicao dos
// dois tipos, com `airlocked` e so o id — nome/tags/hashes ficam como estao
// (campos ausentes, nao null). Cota por plano (Free 0 GB): quando passa, o
// TorBox recusa e a frase dele vai em `detalhe`.
int debrid_tb_airlock(int usenet, int item, int ligar, char *detalhe, unsigned n) {
  char url[300], auth[260], corpo[96], k[200], ok_[16];
  const char *cab[2];
  char *r; int st = 0, ok;
  if (detalhe && n) detalhe[0] = 0;
  if (item <= 0 || !tbChave(k, sizeof k)) return 0;
  snprintf(url, sizeof url, TB "/%s", usenet ? "usenet/editusenetdownload" : "torrents/edittorrent");
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", k);
  cab[0] = auth; cab[1] = NULL;
  snprintf(corpo, sizeof corpo, "{\"%s\":%d,\"airlocked\":%s}",
           usenet ? "usenet_download_id" : "torrent_id", item, ligar ? "true" : "false");
  r = rede_colocar_st(url, 15, cab, corpo, &st);
  // `success` false com 2xx tambem e recusa: o TorBox responde assim a varios erros de conta.
  ok = ok2xx(r, st) && !(js_bruto(r, NULL, "success", ok_, sizeof ok_) && strncmp(ok_, "true", 4));
  if (!ok) {
    char erro[96] = "", det[160] = "";
    if (r) { js_texto_raiz(r, "error", erro, sizeof erro); js_texto_raiz(r, "detail", det, sizeof det); }
    printf("[partes] TorBox airlock %s (%s): HTTP %d%s%s%s%s\n", ligar ? "on" : "off",
           usenet ? "usenet" : "torrent", st, erro[0] ? ", error=" : "", erro, det[0] ? ", detail=" : "", det);
    if (detalhe && n) snprintf(detalhe, n, "%s", det[0] ? det : erro);
  } else {
    printf("[partes] TorBox airlock %s (%s): ok\n", ligar ? "on" : "off", usenet ? "usenet" : "torrent");
  }
  fflush(stdout);
  free(r);
  return ok;
}

// ======================================================= account items AD/RD
//
// tbabrir.c (owner 25/11/2026): the early lookup and the recovery of a file
// that never shows a picture, on AllDebrid and Real-Debrid as on TorBox.
// Account calls: a context of their own (conta = 1) so a new search started
// meanwhile (Reload, Resume's background search) does not void them; a new
// session (logout, other profile) does.
static int contaEntrar(DebridContexto *c) {
  if (contextoDoFio()) return 0;
  debrid_contexto_capturar(c);
  c->conta = 1;
  debrid_contexto_entrar(c);
  return 1;
}
static void contaSair(int aberto) { if (aberto) debrid_contexto_sair(); }

int debrid_conta_disponivel(int q) {
  int tem;
  if (q < 0 || q >= SN) return 0;
  pthread_mutex_lock(&chaveTrava);
  tem = chave[q][0] != 0;
  pthread_mutex_unlock(&chaveTrava);
  if (!tem) return 0;
  return atomic_load(&semPlano[q]) ? -1 : 1;
}

static int hashValido(const char *h, char out[41]) {
  int i;
  if (!h || strlen(h) < 40) return 0;
  for (i = 0; i < 40 && isxdigit((unsigned char)h[i]); i++) out[i] = (char)tolower((unsigned char)h[i]);
  out[i == 40 ? 40 : 0] = 0;
  return i == 40;
}

// ---- the account list, kept 60 s (one list read serves every file lookup
// of the source list while a person scrolls it)
#define CONTA_LISTA_MS 60
typedef struct { char hash[41], id[24]; char pronto; } ContaItem;
static struct { ContaItem *v; int n; time_t em; unsigned geracao; } contaLista[SN];
static pthread_mutex_t contaListaTrava = PTHREAD_MUTEX_INITIALIZER;

static void contaListaGuardar(int q, ContaItem *v, int n) {
  pthread_mutex_lock(&contaListaTrava);
  free(contaLista[q].v);
  contaLista[q].v = v; contaLista[q].n = n; contaLista[q].em = time(NULL);
  contaLista[q].geracao = debrid_conta_geracao(q);
  pthread_mutex_unlock(&contaListaTrava);
}
static void contaListaAdd(ContaItem **v, int *n, int *cap, const char *hash, const char *id, int pronto) {
  char h[41];
  if (!hashValido(hash, h) || !id[0]) return;
  if (*n == *cap) {
    int c = *cap ? *cap * 2 : 128;
    ContaItem *q2 = realloc(*v, (size_t)c * sizeof **v);
    if (!q2) return;
    *v = q2; *cap = c;
  }
  snprintf((*v)[*n].hash, sizeof (*v)[*n].hash, "%s", h);
  snprintf((*v)[*n].id, sizeof (*v)[*n].id, "%s", id);
  (*v)[*n].pronto = (char)(pronto ? 1 : 0);
  (*n)++;
}

// AllDebrid: POST v4.1/magnet/status with no id = every magnet of the account
// (data.magnets[], each with id, hash, statusCode; 4 = Ready).
static int adListar(ContaItem **v, int *n) {
  char *r; int st = 0, cap = 0;
  const char *p;
  *v = NULL; *n = 0;
  r = adPost("v4.1/magnet/status", "", &st);
  if (!adOk(r, st)) { falha(SAD, "status (list)", st, r); free(r); return -1; }
  for (p = js_array(r, NULL, "magnets"); p && *p == '{'; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    char h[64] = "", id[24];
    double i = js_num(p, f, "id", -1);
    if (i < 0 || !js_texto(p, f, "hash", h, sizeof h)) continue;
    snprintf(id, sizeof id, "%.0f", i);
    contaListaAdd(v, n, &cap, h, id, (int)js_num(p, f, "statusCode", -1) == 4);
  }
  free(r);
  return 1;
}

// Real-Debrid: GET torrents?limit=1000&page=N (a root array, each with id,
// hash, status). Up to 3 pages.
static int rdListar(ContaItem **v, int *n) {
  int cap = 0;
  *v = NULL; *n = 0;
  for (int pg = 1; pg <= 3; pg++) {
    char rota[64], *r; int st = 0, k = 0;
    const char *p;
    snprintf(rota, sizeof rota, "torrents?limit=1000&page=%d", pg);
    r = get_auth(RD, rota, SRD, &st);
    if (st == 204) { free(r); break; }       // no (more) torrents
    if (!ok2xx(r, st)) { falha(SRD, "torrents (list)", st, r); free(r); if (pg == 1) { free(*v); *v = NULL; return -1; } break; }
    for (p = js_raiz_array(r); p && *p == '{'; p = js_prox(js_fim(p)), k++) {
      const char *f = js_fim(p);
      char h[64] = "", id[24] = "", stt[32] = "";
      if (!js_texto(p, f, "hash", h, sizeof h) || !js_texto(p, f, "id", id, sizeof id)) continue;
      js_texto(p, f, "status", stt, sizeof stt);
      contaListaAdd(v, n, &cap, h, id, !strcmp(stt, "downloaded"));
    }
    free(r);
    if (k < 1000) break;
  }
  return 1;
}

int debrid_conta_achar(int q, const char *hash, char *id, unsigned n, int *pronto) {
  char h[41];
  int achou = 0, i, aberto, v;
  DebridContexto c;
  if (id && n) id[0] = 0;
  if (pronto) *pronto = 0;
  if ((q != SAD && q != SRD) || !hashValido(hash, h) || debrid_conta_disponivel(q) <= 0) return -1;
  pthread_mutex_lock(&contaListaTrava);
  v = contaLista[q].v && time(NULL) - contaLista[q].em < CONTA_LISTA_MS &&
      contaLista[q].geracao == debrid_conta_geracao(q);
  pthread_mutex_unlock(&contaListaTrava);
  if (!v) {
    ContaItem *lst; int ln, ok;
    aberto = contaEntrar(&c);
    ok = q == SAD ? adListar(&lst, &ln) : rdListar(&lst, &ln);
    contaSair(aberto);
    if (ok < 0) return -1;
    contaListaGuardar(q, lst, ln);
  }
  pthread_mutex_lock(&contaListaTrava);
  for (i = 0; i < contaLista[q].n; i++) {
    ContaItem *it = &contaLista[q].v[i];
    if (strcmp(it->hash, h)) continue;
    // several entries of the hash (RD keeps duplicates): a ready one wins
    if (!achou || (it->pronto && !(pronto && *pronto))) {
      if (id && n) snprintf(id, n, "%s", it->id);
      if (pronto) *pronto = it->pronto;
      achou = 1;
    }
  }
  pthread_mutex_unlock(&contaListaTrava);
  if (achou && q == SRD && id) rdLembrar(h, id);
  return achou;
}

static void contaListaEsquecer(int q) {
  pthread_mutex_lock(&contaListaTrava);
  contaLista[q].em = 0;
  pthread_mutex_unlock(&contaListaTrava);
}

int debrid_ad_enviar(const char *hash, char *id, unsigned n) {
  char h[41], corpo[200], sub[16], rd[16], *r;
  int st = 0, aberto, pronto;
  DebridContexto c;
  if (id && n) id[0] = 0;
  if (!hashValido(hash, h) || debrid_conta_disponivel(SAD) <= 0) return -1;
  snprintf(corpo, sizeof corpo, "magnets%%5B%%5D=magnet%%3A%%3Fxt%%3Durn%%3Abtih%%3A%s", h);
  aberto = contaEntrar(&c);
  r = adPost("v4/magnet/upload", corpo, &st);
  if (!adOk(r, st) || js_texto(r, NULL, "code", sub, sizeof sub) || js_num(r, NULL, "id", -1) < 0) {
    falha(SAD, "upload", st, r); free(r); contaSair(aberto); return -1;
  }
  if (id && n) snprintf(id, n, "%.0f", js_num(r, NULL, "id", -1));
  pronto = js_bruto(r, NULL, "ready", rd, sizeof rd) && !strncmp(rd, "true", 4);
  free(r);
  contaSair(aberto);
  contaListaEsquecer(SAD);
  return pronto;
}

int debrid_ad_apagar(const char *id) {
  char corpo[64], *r; int st = 0, ok, aberto;
  DebridContexto c;
  if (!id || !id[0] || debrid_conta_disponivel(SAD) <= 0) return 0;
  snprintf(corpo, sizeof corpo, "id=%s", id);
  aberto = contaEntrar(&c);
  r = adPost("v4/magnet/delete", corpo, &st);
  ok = adOk(r, st);
  if (!ok) falha(SAD, "delete", st, r);
  free(r);
  contaSair(aberto);
  contaListaEsquecer(SAD);
  return ok;
}

// ---- Real-Debrid: hash -> torrent id this app added or found
#define RD_VISTOS 64
static struct { char hash[41], id[24]; unsigned geracao; } rdVistos[RD_VISTOS];
static int rdVistosProx;
static pthread_mutex_t rdVistosTrava = PTHREAD_MUTEX_INITIALIZER;
static int rdVisto(const char *hash, char *id, unsigned n) {
  char h[41]; int ok = 0;
  if (!hashValido(hash, h)) return 0;
  pthread_mutex_lock(&rdVistosTrava);
  for (int i = 0; i < RD_VISTOS && !ok; i++)
    if (rdVistos[i].hash[0] && !strcmp(rdVistos[i].hash, h) && rdVistos[i].geracao == debrid_conta_geracao(SRD)) {
      snprintf(id, n, "%s", rdVistos[i].id); ok = 1;
    }
  pthread_mutex_unlock(&rdVistosTrava);
  return ok;
}
static void rdLembrar(const char *hash, const char *id) {
  char h[41]; int i;
  if (!hashValido(hash, h) || !id || !id[0] || strlen(id) >= sizeof rdVistos[0].id) return;
  pthread_mutex_lock(&rdVistosTrava);
  for (i = 0; i < RD_VISTOS; i++) if (!strcmp(rdVistos[i].hash, h)) break;
  if (i == RD_VISTOS) { i = rdVistosProx; rdVistosProx = (rdVistosProx + 1) % RD_VISTOS; }
  snprintf(rdVistos[i].hash, sizeof rdVistos[i].hash, "%s", h);
  snprintf(rdVistos[i].id, sizeof rdVistos[i].id, "%s", id);
  rdVistos[i].geracao = debrid_conta_geracao(SRD);
  pthread_mutex_unlock(&rdVistosTrava);
}
static void rdEsquecer(const char *hash) {
  char h[41];
  if (!hashValido(hash, h)) return;
  pthread_mutex_lock(&rdVistosTrava);
  for (int i = 0; i < RD_VISTOS; i++) if (!strcmp(rdVistos[i].hash, h)) rdVistos[i].hash[0] = 0;
  pthread_mutex_unlock(&rdVistosTrava);
}

// torrents/info/<id>: a downloaded torrent's `links` follow its SELECTED files
// in order (one link per selected file; a single link for several files is
// RD's archive, not playable here). The playing file must be one of them.
// 1 = `url` from unrestrict/link (a fresh link, the old one untouched); 0 =
// the torrent cannot serve it (*sumiu = 1: RD no longer has the torrent).
static int rdLinkDoItem(const char *id, const char *arquivo, uint64_t bytes, int fileIdx, char *url, unsigned n,
                        int *sumiu) {
  char rota[120], status[32], link[900], enc[2800], corpo[2900], *r, *sel = NULL;
  int st = 0, ok = 0, k, alvo = -1, nSel = 0, nLinks = 0;
  const char *files, *links, *el, *p;
  size_t selN = 0;
  if (sumiu) *sumiu = 0;
  if (url && n) url[0] = 0;
  snprintf(rota, sizeof rota, "torrents/info/%s", id);
  r = get_auth(RD, rota, SRD, &st);
  if (st == 404) { if (sumiu) *sumiu = 1; free(r); return 0; }
  if (!ok2xx(r, st)) { int v = falha(SRD, "info", st, r); free(r); return v < 0 ? v : 0; }
  if (!js_texto(r, NULL, "status", status, sizeof status) || strcmp(status, "downloaded") ||
      !(files = js_array(r, NULL, "files")) || !(links = js_array(r, NULL, "links")) || *links != '"') {
    free(r); return 0;
  }
  // The selected files only, as a list escolherArquivo can read.
  sel = malloc(strlen(r) + 4);
  if (!sel) { free(r); return 0; }
  sel[selN++] = '[';
  for (p = files; p && *p == '{'; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    if (js_num(p, f, "selected", 0) != 1) continue;
    if (nSel) sel[selN++] = ',';
    memcpy(sel + selN, p, (size_t)(f - p)); selN += (size_t)(f - p);
    nSel++;
  }
  sel[selN++] = ']'; sel[selN] = 0;
  el = nSel ? arquivoExato(sel + 1, "path", "bytes", arquivo, bytes) : NULL;
  if (!el && nSel) {
    // the add-on's index counts every file: pick in the whole list, then find it among the selected
    const char *todo = escolherArquivo(files, fileIdx, "path", "bytes", arquivo);
    if (todo && js_num(todo, js_fim(todo), "selected", 0) == 1) {
      double fid = js_num(todo, js_fim(todo), "id", -1);
      for (p = sel + 1, k = 0; p && *p == '{'; p = js_prox(js_fim(p)), k++)
        if (js_num(p, js_fim(p), "id", -2) == fid) { el = p; break; }
    }
  }
  if (el) for (p = sel + 1, k = 0; p && *p == '{'; p = js_prox(js_fim(p)), k++) if (p == el) { alvo = k; break; }
  // the alvo-th link
  for (p = links, k = 0; p && *p == '"'; k++) {
    const char *q = p + 1;
    while (*q && *q != '"') { if (*q == '\\' && q[1]) q++; q++; }
    if (k == alvo && js_cadeia(p, link, sizeof link)) ok = 1;
    nLinks++;
    if (!*q) break;
    p = js_prox(q + 1);
  }
  free(sel);
  free(r);
  if (!ok || nLinks != nSel) return 0;
  urlenc(enc, sizeof enc, link);
  snprintf(corpo, sizeof corpo, "link=%s", enc);
  r = post_form(RD, "unrestrict/link", SRD, corpo, &st);
  ok = ok2xx(r, st) && js_texto(r, NULL, "download", url, n) && !strncmp(url, "http", 4);
  if (!ok) { int v = falha(SRD, "unrestrict", st, r); if (url && n) url[0] = 0; free(r); return v < 0 ? v : 0; }
  free(r);
  return 1;
}

int debrid_conta_link(int q, const char *id, const char *arquivo, uint64_t bytes, int fileIdx, char *url,
                      unsigned n) {
  int aberto, ok, sumiu = 0;
  DebridContexto c;
  if (url && n) url[0] = 0;
  if ((q != SAD && q != SRD) || !id || !id[0] || debrid_conta_disponivel(q) <= 0) return 0;
  aberto = contaEntrar(&c);
  ok = q == SAD ? adLinkDoMagnet(id, fileIdx, arquivo, bytes, url, n, id) == 1
                : rdLinkDoItem(id, arquivo, bytes, fileIdx, url, n, &sumiu) == 1;
  contaSair(aberto);
  return ok && url[0];
}
