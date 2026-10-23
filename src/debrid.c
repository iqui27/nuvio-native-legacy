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
static atomic_uint tbGeracao;
unsigned debrid_tb_geracao(void) { return atomic_load(&tbGeracao); }

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
  return c && c->sessao == sessaoAtual && c->busca == atomic_load(&geracao);
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

// Mesma ordem do selectDebridFile do web: padrao SxxEyy > fileIdx > maior
// video. Devolve o ELEMENTO escolhido (ponteiro para o '{' dele, dentro de
// `files`) ou NULL.
//
// POR QUE DEVOLVE O ELEMENTO E NAO UM id. Os tres servicos nomeiam os campos a
// sua maneira — o RD manda "path"/"bytes" e o arquivo tem "id"; o TorBox manda
// "name"/"size" com "id"; o Premiumize manda "path"/"size" e NAO tem id
// nenhum, o link direto vem dentro do proprio elemento. Devolvendo o elemento,
// cada resolvedor tira dali o que o seu servico usa e a ORDEM DE ESCOLHA — que
// e a unica coisa que nao pode divergir entre os tres — fica escrita uma vez
// so. Os nomes dos campos entram por parametro pelo mesmo motivo.
static const char *escolherArquivo(const char *files, int fileIdx,
                                   const char *kNome, const char *kTam) {
  char pad1[16] = "", pad2[16] = "";
  const char *p, *melhor = NULL; int idx = 0, porIndice = 0; double melhorTam = -1;
  int temporada = 0, episodio = 0;
  debrid_episodio(&temporada, &episodio);
  if (temporada >= 0 && episodio > 0) {
    snprintf(pad1, sizeof pad1, "s%02de%02d", temporada, episodio);
    snprintf(pad2, sizeof pad2, "%dx%02d", temporada, episodio);
  }
  for (p = files; p && *p == '{'; p = js_prox(js_fim(p)), idx++) {
    const char *f = js_fim(p);
    char nome[600]; double tam;
    if (!js_texto(p, f, kNome, nome, sizeof nome)) continue;
    tam = js_num(p, f, kTam, 0);
    minusc(nome);
    if (!ehVideo(nome)) continue;
    if (pad1[0] && (strstr(nome, pad1) || strstr(nome, pad2))) return p;
    if (idx == fileIdx && fileIdx >= 0) { melhor = p; melhorTam = 1e18; porIndice = 1; continue; }
    if (tam > melhorTam) { melhor = p; melhorTam = tam; }
  }
  // Sem correspondencia ou indice publicado, nao trocar um especial pelo
  // maior episodio regular de um pacote. A falha permite outra fonte.
  if (temporada == 0 && episodio > 0 && !porIndice) return NULL;
  return melhor;
}

// ---------------------------------------------------------------- Real-Debrid
//
// Rotas (api.real-debrid.com/rest/1.0, documentacao oficial "REST API"):
//   POST torrents/addMagnet, GET torrents/info/<id>,
//   POST torrents/selectFiles/<id>, POST unrestrict/link.

// `pct` recebe o progresso (0-100) do torrent que o RD ficou baixando, quando
// ele nao ficou pronto nas tres olhadas; a escolha manual mostra isso.
static int resolverRD(const char *infoHash, int fileIdx, char *url, unsigned n,
                      int *pct) {
  char corpo[700], enc[600], rota[120], tid[64], status[32], link[600];
  char *r; int st = 0, id, tent;
  const char *files, *links, *el;

  snprintf(corpo, sizeof corpo, "magnet:?xt=urn:btih:%s", infoHash);
  urlenc(enc, sizeof enc, corpo);
  snprintf(corpo, sizeof corpo, "magnet=%s", enc);
  r = post_form(RD, "torrents/addMagnet", SRD, corpo, &st);
  if (!ok2xx(r, st) || !js_texto(r, NULL, "id", tid, sizeof tid)) {
    int v = falha(SRD, "addMagnet", st, r); free(r); return v;
  }
  free(r);

  snprintf(rota, sizeof rota, "torrents/info/%s", tid);
  r = get_auth(RD, rota, SRD, &st);
  if (!ok2xx(r, st) || !(files = js_array(r, NULL, "files"))) {
    int v = falha(SRD, "info", st, r); free(r); return v;
  }
  el = escolherArquivo(files, fileIdx, "path", "bytes");
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
  char url[300], auth[260], corpo[1200];
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
static int resolverTB(const char *infoHash, int fileIdx, char *url, unsigned n,
                      int baixar, int *pct) {
  char h[80], magnet[300], rota[600];
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
  snprintf(magnet, sizeof magnet, "magnet:?xt=urn:btih:%s", h);
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
            && (el = escolherArquivo(files, fileIdx, "name", "size")) != NULL)
          fid = (int)js_num(el, js_fim(el), "id", -1);
      } else if (r) {
        int v = falha(STB, "mylist", st, r); free(r); return v;
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
  if (fid < 0) { printf("[debrid] TorBox: sem video utilizavel em %s\n", h); return 0; }

  // 4) ESTA E A UNICA ROTA QUE LEVA A CHAVE NA QUERY, e nao por escolha nossa:
  //    request_download_link poe `token` em add_query, nao em cabecalho. Logo,
  //    esta URL nunca pode ir para log nem inteira nem em pedaco — o que sai
  //    no fim de debrid_resolver e so a resposta passada por rede_url_publica.
  { char u[1200]; const char *cab[1];
    cab[0] = NULL;
    snprintf(u, sizeof u, TB "/torrents/requestdl?token=%s&torrent_id=%d&file_id=%d&redirect=false",
             chaveDoFio(STB), tid, fid);
    r = contextoVale() ? rede_baixar_st(u, 15, cab, &st) : NULL; }
  if (!ok2xx(r, st) || !js_texto_raiz(r, "data", url, n)) {
    // "requestdl" e nao a URL: a URL leva o token (ver acima)
    int v = falha(STB, "requestdl", st, r); free(r); return v;
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
  char magnet[300], enc[500], corpo[600], bruto[64];
  char *r; int st = 0, emCache;
  const char *cont, *el;

  snprintf(magnet, sizeof magnet, "magnet:?xt=urn:btih:%s", infoHash);
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
  el = escolherArquivo(cont, fileIdx, "path", "size");
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

// `manual` 0 (automatico): so o que ja esta pronto (`ready`). 1: a pessoa
// escolheu o torrent; se nao esta pronto, o AllDebrid segue baixando e devolve
// DEBRID_BAIXANDO com o progresso, que fica na conta para a proxima escolha.
static int resolverAD(const char *infoHash, int fileIdx, char *url, unsigned n,
                      int manual, int *pct) {
  char corpo[700], enc[600], id[32], lnk[900], sub[16], h[80];
  char *r; int st = 0, tent, sc = -1, pronto;
  const char *files, *el;
  AdBuf b = { NULL, 0, 0, 0 };

  hashMin(h, sizeof h, infoHash);      // o servico compara TEXTO, como o TorBox
  snprintf(corpo, sizeof corpo, "magnet:?xt=urn:btih:%s", h);
  urlenc(enc, sizeof enc, corpo);
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
  el = escolherArquivo(b.p + 1, fileIdx, "path", "size");
  if (!el || !js_texto(el, js_fim(el), "link", lnk, sizeof lnk)) {
    printf("[debrid] AllDebrid: sem video utilizavel em %s\n", infoHash);
    free(b.p); return 0;
  }
  free(b.p);

  urlenc(enc, sizeof enc, lnk);
  snprintf(corpo, sizeof corpo, "link=%s", enc);
  r = adPost("v4/link/unlock", corpo, &st);
  if (!adOk(r, st)) { int v = falha(SAD, "unlock", st, r); free(r); return v; }
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
      return 0;
    } }
  falha(SAD, "unlock", st, r); free(r);
  return 0;
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
