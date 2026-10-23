#include "xtream.h"
#include "iptvid.h"
int ajustes_livetv_formato(void);  // ajustes.h puxa SDL; o teste do xtream nao
#include "rede.h"
#include "js.h"
#include "dados.h"
#include "perfis.h"
#include "perfisessao.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <strings.h>
#include <time.h>
#include <limits.h>

#define XT_ARQ_FMT   "xtream-p%d.txt"
#define XT_PRAZO_S   20
#define XT_PRAZO_LISTA_S 60

#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif
// Proxy do Xtream so no Tizen (ver chamar). NV_XTREAM_PROXY_TESTE liga o ramo
// numa build nativa para tests/xtream.sh conferir a url montada sem emcc.
#if defined(__EMSCRIPTEN__) || defined(NV_XTREAM_PROXY_TESTE)
#define XT_PROXY 1
#else
#define XT_PROXY 0
#endif

// --- cadastro -------------------------------------------------------------
// O mesmo desenho de stalker.c: uma trava para o cadastro, lido pelo fio do
// guia e pelo de desenho (Ajustes mostra o servidor), recarregado quando o
// perfil ativo muda.
typedef struct FonteContexto {
  unsigned sid; struct FonteContexto *proximo;
  char servidor_[256];
  char usuario_[96];
  char senha_[96];
  int perfilLido_;
  int lido_;
  NvPerfilSessao sessaoLida_;
  XtreamConta conta_;
  int formatoOk_;
  int conexoesModo_; // 0 Auto, 1 On, 2 Off; belongs to this account.
  int autoLimitada_; // Keep the login decision until this account/profile changes.
  int ultimaFalha_, ultimoHttp_, ultimoTotal_;
  int retryAfterUltimo_;
  pthread_mutex_t trava_;
} FonteContexto;
#define IPTV_CTX_INICIALIZAR(c) do { (c)->perfilLido_ = -1; (c)->formatoOk_ = -1; pthread_mutex_init(&(c)->trava_, NULL); } while (0)
#define IPTV_CTX_EMERGENCIA { .sid = UINT_MAX, .perfilLido_ = -1, .formatoOk_ = -1, .trava_ = PTHREAD_MUTEX_INITIALIZER }
// NV_TPK40: rebuild this unit with pthread storage (tools/tpk.sh).
#include "iptvcontexto.inc"
#define ultimoHttp (contexto()->ultimoHttp_)
#define ultimoTotal (contexto()->ultimoTotal_)
#define ultimaFalha (contexto()->ultimaFalha_)
#define retryAfterUltimo (contexto()->retryAfterUltimo_)
#define servidor (contexto()->servidor_ )
#define usuario (contexto()->usuario_ )
#define senha (contexto()->senha_ )
#define perfilLido (contexto()->perfilLido_ )
#define lido (contexto()->lido_ )
#define sessaoLida (contexto()->sessaoLida_ )
#define conta (contexto()->conta_ )
#define formatoOk (contexto()->formatoOk_ )
#define conexoesModo (contexto()->conexoesModo_ )
#define autoLimitada (contexto()->autoLimitada_ )
#define trava (contexto()->trava_ )

static const char *arquivo(void) {
  FonteFio *f = fonteFio();
  if (contexto()->sid) snprintf(f->nome, sizeof f->nome, "xtream-p%d-s%u.txt", perfis_ativo(), contexto()->sid);
  else snprintf(f->nome, sizeof f->nome, XT_ARQ_FMT, perfis_ativo());
  return f->nome;
}

// "meu.servidor.tv:8080", "http://meu.servidor.tv:8080/", "https://x/y" —
// tudo vira "<esquema>://<host[:porta]>". Sem esquema, http: e o que os
// provedores de Xtream entregam na esmagadora maioria, e a porta 8080 nao tem
// TLS por tras.
static void normalizarServidor(const char *entrada, char *dst, unsigned tam) {
  const char *p = entrada ? entrada : "";
  const char *esquema = "http://";
  const char *barra;
  char host[256];
  unsigned n;
  while (*p == ' ') p++;
  if (!strncmp(p, "http://", 7)) p += 7;
  else if (!strncmp(p, "https://", 8)) { p += 8; esquema = "https://"; }
  barra = strchr(p, '/');
  n = barra ? (unsigned)(barra - p) : (unsigned)strlen(p);
  if (n >= sizeof host) n = sizeof host - 1;
  memcpy(host, p, n); host[n] = 0;
  while (n > 0 && (host[n - 1] == '/' || host[n - 1] == ' ')) host[--n] = 0;
  if (!n) { dst[0] = 0; return; }
  snprintf(dst, tam, "%s%s", esquema, host);
}

static void carregarTravado(void) {
  int p = perfis_ativo();
  char *b, *linha, *fim;
  NvPerfilSessao sessao = nv_perfil_sessao_ler();
  if (lido && p == perfilLido && sessao == sessaoLida) return;
  perfilLido = p; sessaoLida = sessao; lido = 1;
  servidor[0] = usuario[0] = senha[0] = 0;
  memset(&conta, 0, sizeof conta); formatoOk = -1; conexoesModo = autoLimitada = 0;
  b = dados_ler(arquivo());
  if (!b) return;
  for (linha = b; *linha; linha = fim) {
    char *sep;
    fim = strchr(linha, '\n');
    if (fim) *fim++ = 0; else fim = linha + strlen(linha);
    if (linha[0] == '#' || !linha[0]) continue;
    sep = strchr(linha, '\t');
    if (!sep) continue;
    *sep++ = 0;
    if (!strcmp(linha, "servidor")) snprintf(servidor, sizeof servidor, "%s", sep);
    else if (!strcmp(linha, "usuario")) snprintf(usuario, sizeof usuario, "%s", sep);
    else if (!strcmp(linha, "senha")) snprintf(senha, sizeof senha, "%s", sep);
    else if (!strcmp(linha, "conexoesModo")) {
      int m = atoi(sep); conexoesModo = m >= 0 && m <= 2 ? m : 0;
    }
  }
  free(b);
}

void xtream_carregar(void) {
  pthread_mutex_lock(&trava);
  carregarTravado();
  pthread_mutex_unlock(&trava);
}

static int gravar(void) {
  char txt[640];
  snprintf(txt, sizeof txt,
           "# CREDENCIAL. Usuario e senha do Xtream vao dentro de toda URL de\n"
           "# canal: valem como a assinatura. Nao versionar, nao empacotar, nao\n"
           "# colar em issue nem em log.\n"
           "servidor\t%s\nusuario\t%s\nsenha\t%s\nconexoesModo\t%d\n", servidor, usuario, senha, conexoesModo);
  return dados_gravar(arquivo(), txt);
}

// Tres setters e nao um: a tela edita um campo por vez, e um setter unico a
// obrigaria a ler a senha em claro so para regrava-la ao trocar o servidor.
int xtream_definir_servidor(const char *s) {
  pthread_mutex_lock(&trava);
  carregarTravado();
  char anterior[sizeof servidor];memcpy(anterior,servidor,sizeof anterior);
  normalizarServidor(s, servidor, sizeof servidor);
  int ok=gravar();
  if(!ok)memcpy(servidor,anterior,sizeof anterior);
  else { autoLimitada = 0; memset(&conta, 0, sizeof conta); }
  pthread_mutex_unlock(&trava);
  return ok;
}
int xtream_conta_definir(const char *s,const char *u,const char *p) {
  if(!s || strlen(s)>=sizeof servidor || strpbrk(s,"\r\n\t") || !u || !*u || !p || !*p || strlen(u)>=sizeof usuario || strlen(p)>=sizeof senha ||
     strpbrk(u,"\r\n\t") || strpbrk(p,"\r\n\t"))return 0;
  char normal[sizeof servidor];normalizarServidor(s,normal,sizeof normal);if(!normal[0])return 0;
  pthread_mutex_lock(&trava);carregarTravado();
  char antigoS[sizeof servidor],antigoU[sizeof usuario],antigoP[sizeof senha];
  memcpy(antigoS,servidor,sizeof antigoS);memcpy(antigoU,usuario,sizeof antigoU);memcpy(antigoP,senha,sizeof antigoP);
  snprintf(servidor,sizeof servidor,"%s",normal);snprintf(usuario,sizeof usuario,"%s",u);snprintf(senha,sizeof senha,"%s",p);
  int atual=nv_perfil_sessao_travar(sessaoLida),ok=atual && gravar();
  if(atual)nv_perfil_sessao_destravar();
  if(!ok) { memcpy(servidor,antigoS,sizeof antigoS);memcpy(usuario,antigoU,sizeof antigoU);memcpy(senha,antigoP,sizeof antigoP); }
  else { autoLimitada=0; memset(&conta,0,sizeof conta);formatoOk=-1; }
  memset(antigoP,0,sizeof antigoP);pthread_mutex_unlock(&trava);return ok;
}
void xtream_definir_usuario(const char *u) {
  pthread_mutex_lock(&trava);
  carregarTravado();
  autoLimitada = 0; memset(&conta, 0, sizeof conta);
  snprintf(usuario, sizeof usuario, "%s", u ? u : "");
  gravar();
  pthread_mutex_unlock(&trava);
}
void xtream_definir_senha(const char *s) {
  pthread_mutex_lock(&trava);
  carregarTravado();
  autoLimitada = 0; memset(&conta, 0, sizeof conta);
  snprintf(senha, sizeof senha, "%s", s ? s : "");
  gravar();
  pthread_mutex_unlock(&trava);
}

void xtream_esquecer(void) {
  pthread_mutex_lock(&trava);
  dados_apagar(arquivo());
  conexoesModo = autoLimitada = 0; memset(&conta, 0, sizeof conta); formatoOk = -1;
  servidor[0] = usuario[0] = senha[0] = 0;
  lido = 1; perfilLido = perfis_ativo();
  pthread_mutex_unlock(&trava);
}

int xtream_configurado(void) {
  int ok;
  pthread_mutex_lock(&trava);
  carregarTravado();
  ok = servidor[0] && usuario[0] && senha[0];
  pthread_mutex_unlock(&trava);
  return ok;
}

const char *xtream_servidor_curto(void) {
  static char curto[128];
  const char *p;
  pthread_mutex_lock(&trava);
  carregarTravado();
  p = servidor;
  if (!strncmp(p, "http://", 7)) p += 7;
  else if (!strncmp(p, "https://", 8)) p += 8;
  snprintf(curto, sizeof curto, "%s", p[0] ? p : "-");
  pthread_mutex_unlock(&trava);
  return curto;
}

const char *xtream_usuario(void) {
  static char u[96];
  pthread_mutex_lock(&trava);
  carregarTravado();
  snprintf(u, sizeof u, "%s", usuario[0] ? usuario : "-");
  pthread_mutex_unlock(&trava);
  return u;
}

const char *xtream_senha_mascarada(void) {
  static char m[64];
  size_t n, k = 0;
  pthread_mutex_lock(&trava);
  carregarTravado();
  n = strlen(senha);
  if (!n) snprintf(m, sizeof m, "-");
  else {
    // "•" e 3 bytes em UTF-8; ate 12 bolinhas, que e o que cabe na linha e ja
    // diz "ha senha" sem dizer quanto.
    if (n > 12) n = 12;
    for (; n; n--) { m[k++] = (char)0xe2; m[k++] = (char)0x80; m[k++] = (char)0xa2; }
    m[k] = 0;
  }
  pthread_mutex_unlock(&trava);
  return m;
}

int xtream_conexoes_modo(void) {
  int m;
  pthread_mutex_lock(&trava); carregarTravado(); m = conexoesModo;
  pthread_mutex_unlock(&trava); return m;
}
void xtream_definir_conexoes_modo(int m) {
  if (m < 0 || m > 2) return;
  pthread_mutex_lock(&trava); carregarTravado(); conexoesModo = m; gravar();
  pthread_mutex_unlock(&trava);
}
static int limitadaTravado(void) {
  return conexoesModo == 1 || (conexoesModo == 0 && autoLimitada);
}
int xtream_conexoes_limitadas(void) {
  int on;
  pthread_mutex_lock(&trava); carregarTravado(); on = limitadaTravado();
  pthread_mutex_unlock(&trava); return on;
}
int xtream_url_limitada(const char *url) {
  int on = 0;
  pthread_mutex_lock(&trava); carregarTravado();
  if (url && servidor[0] && limitadaTravado()) {
    size_t n = strlen(servidor);
    if (!strncmp(url, servidor, n) && (!strncmp(url + n, "/live/", 6) ||
        !strncmp(url + n, "/movie/", 7) || !strncmp(url + n, "/series/", 8))) on = 1;
  }
  pthread_mutex_unlock(&trava); return on;
}

// --- rede ---------------------------------------------------------------
static void urlenc(const char *s, char *dst, unsigned tam) {
  static const char *HEX = "0123456789ABCDEF";
  unsigned k = 0;
  if (!tam) return;
  for (; s && *s && k + 4 < tam; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
      dst[k++] = (char)c;
    else { dst[k++] = '%'; dst[k++] = HEX[c >> 4]; dst[k++] = HEX[c & 15]; }
  }
  dst[k] = 0;
}

// Copia do cadastro sob a trava e a chamada fora dela: uma resposta de 2 MB
// (servidor com 20 mil canais) nao pode segurar a tela de Ajustes.
//
// COM O STATUS HTTP (#158). A primeira versao devolvia so o corpo: 401, 403,
// 458, 429, pagina HTML de Cloudflare e prazo estourado chegavam todos como
// NULL, e o log dizia "nao respondeu" para qualquer um deles. Agora a chamada
// devolve o corpo mesmo em 4xx/5xx e o codigo em *status (0 = nem houve
// resposta); quem chama decide o que e erro.
//
// EM SERIE. Os paineis Xtream respondem 429 a rajadas (a pesquisa do #158
// cita 7 chamadas paralelas derrubando um painel XUI), e agora ha dois fios
// que falam com ele: o do guia (lista) e o da grade curta (xtream_epg_curto).
// Uma trava so para a rede do Xtream faz os dois esperarem a vez.
static pthread_mutex_t travaRede = PTHREAD_MUTEX_INITIALIZER;
// Retry-After do ultimo pedido ao painel (s; 0 = nao veio). xtepg.c le depois
// de um 429.

static char *chamarSt(const char *acao, int prazo, int *status, NvPerfilSessao sessao) {
  char srv[256], u[300], s[300], url[1100];
  char *r;
  int st = 0;
  if (status) *status = 0;
  pthread_mutex_lock(&trava);
  if (!nv_perfil_sessao_travar(sessao)) { pthread_mutex_unlock(&trava); return NULL; }
  carregarTravado();
  snprintf(srv, sizeof srv, "%s", servidor);
  urlenc(usuario, u, sizeof u);
  urlenc(senha, s, sizeof s);
  nv_perfil_sessao_destravar();
  pthread_mutex_unlock(&trava);
  if (!srv[0] || !u[0] || !s[0]) return NULL;
  if (acao && acao[0])
    snprintf(url, sizeof url, "%s/player_api.php?username=%s&password=%s&action=%s",
             srv, u, s, acao);
  else
    snprintf(url, sizeof url, "%s/player_api.php?username=%s&password=%s", srv, u, s);
  pthread_mutex_lock(&travaRede);
  if (!nv_perfil_sessao_atual(sessao)) { pthread_mutex_unlock(&travaRede); return NULL; }
#if XT_PROXY
  // SAMSUNG (#112): o Chromium do Tizen barra TODO `http://` que sai do app
  // (log D1 1647, 1.4.1: a lista morria aqui com o painel respondendo CORS),
  // e quase todo painel Xtream e http. O servico de recomendacoes (https)
  // repassa so o player_api.php — ver servidor/recomendacoes/src/xtream.js,
  // que tambem diz o que ele ve (a credencial em transito, nunca guardada nem
  // logada). https:// continua direto: nao tem bloqueio e nao precisa de
  // terceiro no meio. O video NAO passa por la: xtream_url vai direto ao
  // AVPlay. Sem NV_REC_URL na build fica como antes (direto, e falha).
  //
  // POST COM A URL NO CORPO, e nao GET ?u=: com GET o `wrangler tail`
  // imprimia a url de entrada com usuario e senha (medido no deploy
  // 748f61f7). text/plain e tipo "simples" do CORS: sem preflight.
  if (NV_REC_URL[0] && !strncmp(srv, "http://", 7)) {
    static const char *const cab[] = { "Content-Type: text/plain", NULL };
    char via[300];
    snprintf(via, sizeof via, "%s/v1/xtream", NV_REC_URL);
    r = rede_postar_st(via, prazo, cab, url, &st);
    pthread_mutex_unlock(&travaRede);
    if (!nv_perfil_sessao_atual(sessao)) { free(r); return NULL; }
    if (status) *status = st;
    return r;
  }
#endif
  r = rede_baixar_st_retry(url, prazo, NULL, &st, &retryAfterUltimo);
  pthread_mutex_unlock(&travaRede);
  if (!nv_perfil_sessao_atual(sessao)) { free(r); return NULL; }
  if (status) *status = st;
  return r;
}
#define retryCanal (fonteFio()->retry)
int xtream_ultimo_retry_after(void) { return retryCanal; }

// O que veio no corpo, para o log e para a tela: JSON (comeca com { ou [),
// pagina HTML (Cloudflare, painel de erro, portal cativo) ou nada.
enum { XC_VAZIO, XC_JSON, XC_HTML, XC_OUTRO };
static int classificar(const char *c) {
  if (!c) return XC_VAZIO;
  if ((unsigned char)c[0] == 0xEF && (unsigned char)c[1] == 0xBB && (unsigned char)c[2] == 0xBF) c += 3;
  while (*c && (unsigned char)*c <= ' ') c++;
  if (!*c) return XC_VAZIO;
  if (*c == '{' || *c == '[') return XC_JSON;
  if (*c == '<') return XC_HTML;
  return XC_OUTRO;
}
static const char *nomeClasse(int k) {
  return k == XC_JSON ? "JSON" : k == XC_HTML ? "pagina HTML" : k == XC_VAZIO ? "vazio" : "texto";
}
static const char *pulaBom(const char *c) {
  if (c && (unsigned char)c[0] == 0xEF && (unsigned char)c[1] == 0xBB && (unsigned char)c[2] == 0xBF) c += 3;
  while (c && *c && (unsigned char)*c <= ' ') c++;
  return c;
}

// O contrato antigo (corpo so em 2xx), para quem nao precisa do codigo.
static char *chamar(const char *acao, NvPerfilSessao sessao) {
  int st = 0;
  char *r = chamarSt(acao, XT_PRAZO_S, &st, sessao);
  if (r && st >= 400) { free(r); r = NULL; }
  return r;
}

// --- canais -------------------------------------------------------------
typedef struct { char id[24]; char nome[64]; int ordem; } XtCat;

// Um campo que o servidor manda ora como texto ("5") ora como numero (5):
// tenta os dois. E o caso de category_id e stream_id, conforme o painel.
static int campoTextoOuNumero(const char *ini, const char *fim, const char *chave,
                              char *dst, unsigned tam) {
  double v;
  if (js_texto(ini, fim, chave, dst, tam) && dst[0]) return 1;
  v = js_num(ini, fim, chave, -1.0);
  if (v < 0.0) { dst[0] = 0; return 0; }
  snprintf(dst, tam, "%.0f", v);
  return 1;
}

static int compararCategorias(const void *aa, const void *bb) {
  const XtCat *a = aa, *b = bb;
  int c = strcmp(a->id, b->id);
  return c ? c : (a->ordem > b->ordem) - (a->ordem < b->ordem);
}
static XtCat *lerCategorias(int *quantos, NvPerfilSessao sessao) {
  char *corpo = chamar("get_live_categories", sessao);
  const char *p = corpo ? js_raiz_array(corpo) : NULL;
  XtCat *cats = NULL;
  int n = 0, cap = 0;
  *quantos = 0;
  while (p && *p) {
    const char *fim = js_fim(p);
    XtCat c = {0};
    if (!fim) break;
    if (campoTextoOuNumero(p, fim, "category_id", c.id, sizeof c.id) &&
        js_texto(p, fim, "category_name", c.nome, sizeof c.nome)) {
      if (n == cap) {
        if (cap > INT_MAX / 2) break;
        int nova = cap ? cap * 2 : 64;
        if ((size_t)nova > SIZE_MAX / sizeof *cats) break;
        XtCat *tmp = realloc(cats, (size_t)nova * sizeof *cats);
        if (!tmp) { printf("[xtream] memoria insuficiente para categorias\n"); break; }
        cats = tmp; cap = nova;
      }
      c.ordem = n; cats[n++] = c;
    }
    p = js_prox(fim);
  }
  free(corpo);
  if (n > 1) qsort(cats, (size_t)n, sizeof *cats, compararCategorias);
  *quantos = n;
  return cats;
}

// Escrito so pelo fio do guia (o unico que chama xtream_canais) e lido pelo
// mesmo fio antes de publicar: nao precisa de trava.

int xtream_ultimo_total(void) { return ultimoTotal; }
int xtream_ultima_falha(void) { return ultimaFalha; }
int xtream_ultimo_http(void) { return ultimoHttp; }

static int contaLerSessao(XtreamConta *out, NvPerfilSessao sessao);

static int catIndice(const XtCat *cats, int n, const char *id) {
  // Categories are sorted by ID; preserve the first panel definition.
  int lo = 0, hi = n;
  while (lo < hi) {
    int m = lo + (hi - lo) / 2;
    if (strcmp(cats[m].id, id) < 0) lo = m + 1;
    else hi = m;
  }
  return lo < n && !strcmp(cats[lo].id, id) ? lo : -1;
}

// Stream channels without a full XtreamCanal vector. Each call owns its
// category/count buffers so different accounts can load concurrently.
int xtream_canais_cb_sessao(void (*cat)(const char *nome, int ordem, int n, void *u),
                     XtreamCb cb, void *u, int *totalOut, uint64_t sessao) {
  XtCat *cats;
  int *contagens, *ordem;
  int nCat, n = 0, total = 0, st = 0, classe, i, passo;
  char *corpo;
  const char *p, *ini;
  ultimaFalha = XT_OK; ultimoHttp = 0; ultimoTotal = 0;
  if (totalOut) *totalOut = 0;
  if (!nv_perfil_sessao_atual(sessao) || !xtream_configurado() || !cb) return 0;
  // A conta ANTES da lista: e pequena, e o que ela diz (expirada, telas em
  // uso, formatos) e o que a tela precisa quando a lista falha. Sem resposta
  // aqui a lista ainda e tentada — o login pode ter caido por prazo.
  { XtreamConta c; contaLerSessao(&c, sessao); }
  cats = lerCategorias(&nCat, sessao);
  // PRAZO MAIOR SO PARA A LISTA. get_live_streams de um provedor europeu passa
  // de 7 MB (pesquisa do #158); 20 s num painel lento estoura antes do fim.
  corpo = chamarSt("get_live_streams", XT_PRAZO_LISTA_S, &st, sessao);
  ultimoHttp = st;
  classe = classificar(corpo);
  if (!corpo || st == 0) {
    printf("[xtream] servidor nao respondeu a lista de canais\n");
    ultimaFalha = XT_SEM_RESPOSTA;
    free(corpo); free(cats);
    return 0;
  }
  if (st >= 400) {
    // 401/403 com credencial certa e o provedor barrando (IP, UA, conta);
    // 458 e o limite de telas; 429 e rajada. O numero vai para a tela.
    printf("[xtream] lista de canais: HTTP %d (%s, %ld B)\n", st, nomeClasse(classe),
           (long)strlen(corpo));
    ultimaFalha = (strstr(corpo, "\"auth\":0") || st == 401) ? XT_RECUSOU
                : (st == 502 || st == 504) ? XT_SEM_RESPOSTA : XT_HTTP;
    free(corpo); free(cats);
    return 0;
  }
  // Credencial errada nao e "[]": o servidor responde {"user_info":{"auth":0}}
  // ou uma pagina de erro. Pagina HTML (WAF, Cloudflare, portal cativo) NAO e
  // "senha errada".
  p = pulaBom(corpo);
  if (*p != '[') {
    if (strstr(corpo, "\"auth\":0")) {
      printf("[xtream] servidor recusou a credencial (auth 0)\n");
      ultimaFalha = XT_RECUSOU;
    } else {
      printf("[xtream] lista de canais: HTTP %d mas %s em vez da lista (%ld B)\n",
             st, nomeClasse(classe), (long)strlen(corpo));
      ultimaFalha = classe == XC_HTML ? XT_PAGINA : XT_SEM_RESPOSTA;
    }
    free(corpo); free(cats);
    return 0;
  }
  contagens = calloc((size_t)nCat + 1, sizeof *contagens);
  ordem = malloc((size_t)(nCat ? nCat : 1) * sizeof *ordem);
  if (!contagens || !ordem) { free(contagens); free(ordem); free(corpo); free(cats); return 0; }
  for (i = 0; i < nCat; i++) ordem[cats[i].ordem] = i;
  ini = p + 1;
  // Passo 0 conta por categoria (o guia decide antes de receber canal); passo 1
  // entrega. Ler o JSON duas vezes custa menos que guardar a lista inteira.
  for (passo = 0; passo < 2; passo++) {
    int parar = 0;
    if (passo == 1 && cat) {
      for (i = 0; i < nCat; i++) { int j = ordem[i]; cat(cats[j].nome, i, contagens[j], u); }
      if (contagens[nCat]) cat("Outros", nCat, contagens[nCat], u);
    }
    p = ini;
    while (nv_perfil_sessao_atual(sessao) && p && *p && !parar) {
      const char *fim;
      char sid[24], cid[24], num[16];
      XtreamCanal c;
      while (*p && (unsigned char)*p <= ' ') p++;
      if (*p == ',') { p++; continue; }
      if (*p != '{') break;
      fim = js_fim(p);
      if (!fim) break;
      if (campoTextoOuNumero(p, fim, "stream_id", sid, sizeof sid) &&
          js_texto(p, fim, "name", c.nome, sizeof c.nome) && c.nome[0]) {
        int ci;
        if (!campoTextoOuNumero(p, fim, "category_id", cid, sizeof cid)) cid[0] = 0;
        ci = catIndice(cats, nCat, cid);
        if (passo == 0) { contagens[ci >= 0 ? ci : nCat]++; total++; }
        else {
          XtreamItem it;
          char arq[8];
          memset(&it, 0, sizeof it);
          snprintf(c.id, sizeof c.id, "xtream:%s", sid);
          c.logo[0] = c.epgId[0] = 0;
          js_texto(p, fim, "stream_icon", c.logo, sizeof c.logo);
          // Trim panel whitespace; invalid URLs must allow guide fallbacks.
          { char *s = c.logo, *e;
            while (*s && (unsigned char)*s <= ' ') s++;
            e = s + strlen(s);
            while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
            if (strncmp(s, "http://", 7) && strncmp(s, "https://", 8)) *s = 0;
            memmove(c.logo, s, strlen(s) + 1); }
          js_texto(p, fim, "epg_channel_id", c.epgId, sizeof c.epgId);
          it.id = c.id; it.nome = c.nome; it.logo = c.logo; it.epgId = c.epgId;
          it.categoria = ci >= 0 ? cats[ci].nome : "Outros";
          it.catOrdem = ci >= 0 ? cats[ci].ordem : nCat;
          it.semCategoria = ci < 0;
          it.num = campoTextoOuNumero(p, fim, "num", num, sizeof num) ? atoi(num) : 0;
          it.arquivo = campoTextoOuNumero(p, fim, "tv_archive", arq, sizeof arq) && atoi(arq) > 0;
          it.arquivoDias = campoTextoOuNumero(p, fim, "tv_archive_duration", arq, sizeof arq) ? atoi(arq) : 0;
          if (it.epgId[0]) n++;
          if (!cb(&it, u)) parar = 1;
        }
      }
      p = js_prox(fim);
    }
  }
  free(ordem); free(contagens); free(corpo); free(cats);
  ultimoTotal = total;
  // Contagem, e so: servidor, usuario e senha nao entram em log (registro.c
  // desenha o stdout na tela).
  printf("[xtream] %d canal(is) em %d categoria(s), %d com epg_channel_id\n", total, nCat, n);
  if (totalOut) *totalOut = total;
  return total;
}

int xtream_canais_cb(void (*cat)(const char *, int, int, void *),
                     XtreamCb cb, void *u, int *totalOut) {
  return xtream_canais_cb_sessao(cat, cb, u, totalOut, nv_perfil_sessao_ler());
}

typedef struct { XtreamCanal *v; int n, max, total; } XtVetor;
static int xtNoVetor(const XtreamItem *c, void *u) {
  XtVetor *x = u;
  x->total++;
  if (x->n >= x->max) return 1;
  memset(&x->v[x->n], 0, sizeof x->v[0]);
  snprintf(x->v[x->n].id, sizeof x->v[0].id, "%s", c->id);
  snprintf(x->v[x->n].nome, sizeof x->v[0].nome, "%s", c->nome);
  snprintf(x->v[x->n].logo, sizeof x->v[0].logo, "%s", c->logo);
  snprintf(x->v[x->n].categoria, sizeof x->v[0].categoria, "%s", c->categoria);
  snprintf(x->v[x->n].epgId, sizeof x->v[0].epgId, "%s", c->epgId);
  x->n++;
  return 1;
}
int xtream_canais_sessao(XtreamCanal *saida, int max, uint64_t sessao) {
  XtVetor x;
  if (!nv_perfil_sessao_atual(sessao)) return 0;
  if (!saida || max < 1) return 0;
  x.v = saida; x.n = 0; x.max = max; x.total = 0;
  xtream_canais_cb_sessao(NULL, xtNoVetor, &x, NULL, sessao);
  if (x.total > x.n)
    printf("[xtream] lista cortada: %d de %d canais cabem no guia\n", x.n, x.total);
  return nv_perfil_sessao_atual(sessao) ? x.n : 0;
}
int xtream_canais(XtreamCanal *saida, int max) {
  return xtream_canais_sessao(saida, max, nv_perfil_sessao_ler());
}

// --- reproducao ---------------------------------------------------------
int xtream_e_id(const char *id) {
  return iptvid_ler("xtream:", id, NULL, NULL, 0);
}

int xtream_url_xmltv(char *url, unsigned n) {
  char u[300], s[300];
  if (!url || n < 2) return 0;
  url[0] = 0;
  pthread_mutex_lock(&trava);
  carregarTravado();
  if (!servidor[0] || !usuario[0] || !senha[0]) { pthread_mutex_unlock(&trava); return 0; }
#ifdef __EMSCRIPTEN__
  // O Chromium do Tizen barra http:// (ver chamar), e o proxy do servico de
  // recomendacoes so repassa o player_api.php: a grade do provedor so vem
  // quando o painel e https.
  if (strncmp(servidor, "https://", 8)) { pthread_mutex_unlock(&trava); return 0; }
#endif
  urlenc(usuario, u, sizeof u);
  urlenc(senha, s, sizeof s);
  snprintf(url, n, "%s/xmltv.php?username=%s&password=%s", servidor, u, s);
  pthread_mutex_unlock(&trava);
  return 1;
}

// --- conta (#158) --------------------------------------------------------
// player_api.php SEM action devolve user_info (auth, status, exp_date,
// active_cons, max_connections, allowed_output_formats) e server_info. E o
// que deixa a tela dizer "conta expirada", "2 de 2 telas em uso" ou "este
// servidor so entrega .ts" em vez de "o canal nao abriu". Os paineis mandam
// quase tudo como TEXTO ("active_cons":"2"), alguns como numero: le os dois.
// `conta`, declarada com o cadastro, pertence a sessao que a produziu.

static int intDe(const char *ini, const char *fim, const char *chave, int padrao) {
  char b[32];
  if (!campoTextoOuNumero(ini, fim, chave, b, sizeof b) || !b[0]) return padrao;
  if (b[0] < '0' || b[0] > '9') return padrao;
  return atoi(b);
}

int xtream_conta_parse(const char *json, XtreamConta *c) {
  const char *u, *fim;
  char b[32];
  if (!c) return 0;
  memset(c, 0, sizeof *c);
  c->conexoes = c->maxConexoes = -1;
  if (!json) return 0;
  u = strstr(json, "\"user_info\"");
  if (!u) return 0;
  u = strchr(u, '{');
  if (!u || !(fim = js_fim(u))) return 0;
  c->valido = 1;
  c->auth = intDe(u, fim, "auth", 0);
  js_texto(u, fim, "status", c->status, sizeof c->status);
  if (campoTextoOuNumero(u, fim, "exp_date", b, sizeof b) && b[0] >= '0' && b[0] <= '9')
    c->expira = atoll(b);
  c->conexoes = intDe(u, fim, "active_cons", -1);
  c->maxConexoes = intDe(u, fim, "max_connections", -1);
  c->teste = intDe(u, fim, "is_trial", 0);
  { const char *f = strstr(u, "\"allowed_output_formats\"");
    if (f && f < fim && (f = strchr(f, '[')) && f < fim) {
      const char *g = strchr(f, ']');
      if (g && g < fim) {
        char lista[128];
        size_t k = (size_t)(g - f);
        if (k >= sizeof lista) k = sizeof lista - 1;
        memcpy(lista, f, k); lista[k] = 0;
        c->formatosDeclarados = 1;
        c->temM3u8 = strstr(lista, "\"m3u8\"") != NULL;
        c->temTs = strstr(lista, "\"ts\"") != NULL;
      }
    } }
  return 1;
}

static int contaLerSessao(XtreamConta *out, NvPerfilSessao sessao) {
  XtreamConta c;
  int st = 0, classe;
  char *corpo = chamarSt("", XT_PRAZO_S, &st, sessao);
  if (!nv_perfil_sessao_atual(sessao)) {
    free(corpo); if (out) memset(out, 0, sizeof *out); return 0;
  }
  classe = classificar(corpo);
  if (!xtream_conta_parse(corpo, &c)) {
    memset(&c, 0, sizeof c);
    c.conexoes = c.maxConexoes = -1;
  }
  c.http = st;
  // Uma linha, sem nada da pessoa: e a primeira coisa a olhar num registro de
  // "nao toca" (conta vencida, telas cheias, so .ts).
  if (c.valido)
    printf("[xtream] conta: auth=%d status=%s vence=%lld telas=%d/%d formatos=%s%s%s\n",
           c.auth, c.status[0] ? c.status : "?", c.expira, c.conexoes, c.maxConexoes,
           !c.formatosDeclarados ? "?" : c.temM3u8 ? "m3u8" : "",
           c.formatosDeclarados && c.temM3u8 && c.temTs ? "," : "",
           c.formatosDeclarados && c.temTs ? "ts" : "");
  else
    printf("[xtream] conta: HTTP %d, %s sem user_info\n", st, nomeClasse(classe));
  fflush(stdout);
  free(corpo);
  pthread_mutex_lock(&trava);
  int atual = nv_perfil_sessao_travar(sessao);
  if (atual) {
    conta = c;
    if (c.valido && c.maxConexoes > 0 && (c.maxConexoes <= 1 || c.conexoes >= c.maxConexoes)) autoLimitada = 1;
    nv_perfil_sessao_destravar();
  }
  pthread_mutex_unlock(&trava);
  if (!atual) { if (out) memset(out, 0, sizeof *out); return 0; }
  if (out) *out = c;
  return c.valido;
}

int xtream_conta_ler(XtreamConta *out) {
  return contaLerSessao(out, nv_perfil_sessao_ler());
}

int xtream_conta(XtreamConta *out) {
  int v;
  pthread_mutex_lock(&trava);
  carregarTravado();
  if (out) *out = conta;
  v = conta.valido;
  pthread_mutex_unlock(&trava);
  return v;
}

int xtream_conta_aviso(const XtreamConta *c, long long agora) {
  if (!c || !c->valido) return XA_NADA;
  if (!c->auth) return XA_RECUSOU;
  if (!strcasecmp(c->status, "Expired") || (c->expira > 0 && c->expira < agora)) return XA_EXPIRADA;
  if (!strcasecmp(c->status, "Banned") || !strcasecmp(c->status, "Disabled")) return XA_DESATIVADA;
  if (c->maxConexoes > 0 && c->conexoes >= c->maxConexoes) return XA_TELAS_CHEIAS;
  if (c->expira > 0 && c->expira - agora < 7LL * 86400) return XA_VENCE_LOGO;
  return XA_NADA;
}

// --- formato do fluxo (#158) ------------------------------------------------
// .m3u8 PRIMEIRO, .ts DEPOIS, e a lista de fontes do canal leva as duas: o
// watchdog de canal (app.c) ja pula para a proxima fonte quando uma nao abre.
// O que o servidor declara em allowed_output_formats manda: formato que ele
// nao declara nao e tentado — a nao ser que ele nao declare nenhum dos dois
// (painel antigo, ou so "rtmp"), e ai vao os dois.
//
// E o formato que tocou fica em memoria para os proximos canais da sessao: um
// painel que so entrega .ts de verdade nao deve custar o prazo do .m3u8 a
// cada troca de canal.
// formatoOk tambem zera quando o cadastro muda de sessao.

int xtream_formatos(const char *ext[2]) {
  XtreamConta c;
  int m = 1, t = 1, n = 0;
  xtream_conta(&c);
  if (c.valido && c.formatosDeclarados && (c.temM3u8 || c.temTs)) { m = c.temM3u8; t = c.temTs; }
  // ESCOLHA DE AJUSTES (Live TV > Formato do Xtream). Pedir um formato poe ele
  // na frente MESMO que a conta so declare o outro: muitos paineis servem
  // .m3u8 com allowed_output_formats=["ts"] (#158, hipotese a medir no
  // diagnostico da Live TV). O declarado continua como segunda fonte.
  { int pref = ajustes_livetv_formato();
    if (pref == 1) { ext[n++] = "m3u8"; if (t) ext[n++] = "ts"; return n; }
    if (pref == 2) { ext[n++] = "ts"; if (m) ext[n++] = "m3u8"; return n; } }
  if (formatoOk == 1 && t) { ext[n++] = "ts"; if (m) ext[n++] = "m3u8"; }
  else { if (m) ext[n++] = "m3u8"; if (t) ext[n++] = "ts"; }
  return n;
}

void xtream_formato_funcionou(const char *url) {
  const char *q;
  size_t n;
  if (!url) return;
  q = strchr(url, '?');
  n = q ? (size_t)(q - url) : strlen(url);
  if (n > 3 && !strncmp(url + n - 3, ".ts", 3)) formatoOk = 1;
  else if (n > 5 && !strncmp(url + n - 5, ".m3u8", 5)) formatoOk = 0;
}

static int urlFormato(const char *id, const char *ext, char *url, unsigned n) {
  char u[300], s[300];
  if (!xtream_e_id(id) || !url || n < 2 || !ext || !ext[0]) return 0;
  pthread_mutex_lock(&trava);
  carregarTravado();
  if (!servidor[0] || !usuario[0] || !senha[0]) { pthread_mutex_unlock(&trava); return 0; }
  urlenc(usuario, u, sizeof u);
  urlenc(senha, s, sizeof s);
  snprintf(url, n, "%s/live/%s/%s/%s.%s", servidor, u, s, id + 7, ext);
  pthread_mutex_unlock(&trava);
  return 1;
}

// --- grade curta por canal (#158) -------------------------------------------
// get_short_epg devolve os proximos N programas de UM canal, sem baixar o
// XMLTV inteiro do provedor — que no registro 6311 passou do teto de 32 MB e
// foi ignorado ("grade do provedor: maior que o teto"). Titulo e descricao vem
// em BASE64; o horario vale por start_timestamp/stop_timestamp (epoch UTC). O
// "start":"2026-09-29 20:00:00" que vem junto e hora LOCAL do servidor e fica
// de fora: sem o fuso do painel, ler aquilo seria errar a hora de todo mundo.
static int b64v(int c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+' || c == '-') return 62;
  if (c == '/' || c == '_') return 63;
  return -1;
}
static void b64dec(const char *s, char *dst, unsigned tam) {
  unsigned k = 0, bits = 0, acc = 0;
  if (!tam) return;
  for (; *s && k + 1 < tam; s++) {
    int v;
    if (*s == '\\' && s[1] == '/') { s++; v = 63; }    // "\/" do json_encode
    else v = b64v((unsigned char)*s);
    if (v < 0) { if (*s == '=') break; continue; }
    acc = (acc << 6) | (unsigned)v; bits += 6;
    if (bits >= 8) { bits -= 8; dst[k++] = (char)((acc >> bits) & 0xFF); }
  }
  dst[k] = 0;
  // Controle vira espaco: o titulo vai direto para o desenho de texto.
  for (k = 0; dst[k]; k++) if ((unsigned char)dst[k] < ' ') dst[k] = ' ';
}

int xtream_epg_parse(const char *json, XtreamProg *out, int cap) {
  const char *p, *lista;
  int n = 0;
  if (!json || !out || cap < 1) return 0;
  lista = strstr(json, "\"epg_listings\"");
  if (!lista) return 0;
  p = strchr(lista, '[');
  if (!p) return 0;
  p++;
  while (p && *p && n < cap) {
    const char *fim;
    char b[40], tit[400];
    long long ini = 0, fimT = 0;
    while (*p && (unsigned char)*p <= ' ') p++;
    if (*p != '{') break;
    fim = js_fim(p);
    if (!fim) break;
    if (campoTextoOuNumero(p, fim, "start_timestamp", b, sizeof b)) ini = atoll(b);
    if (campoTextoOuNumero(p, fim, "stop_timestamp", b, sizeof b)) fimT = atoll(b);
    else if (campoTextoOuNumero(p, fim, "end_timestamp", b, sizeof b)) fimT = atoll(b);
    if (ini > 0 && fimT > ini && js_texto(p, fim, "title", tit, sizeof tit)) {
      out[n].ini = (time_t)ini;
      out[n].fim = (time_t)fimT;
      b64dec(tit, out[n].titulo, sizeof out[n].titulo);
      if (out[n].titulo[0]) n++;
    }
    p = js_prox(fim);
  }
  return n;
}

static int epgCurto(const char *id, XtreamProg *out, int cap, int *status) {
  char acao[128];
  const char *d;
  char *corpo;
  int st = 0, n;
  NvPerfilSessao sessao = nv_perfil_sessao_ler();
  if (status) *status = 0;
  if (!xtream_e_id(id) || !out || cap < 1) return -1;
  // So digitos: o id vai direto para a query.
  for (d = id + 7; *d; d++) if (*d < '0' || *d > '9') return -1;
  if (!id[7]) return -1;
  snprintf(acao, sizeof acao, "get_short_epg&stream_id=%s&limit=%d", id + 7, cap);
  corpo = chamarSt(acao, 6, &st, sessao);
  if (status) *status = st;
  if (!corpo || st == 0 || st >= 400) { free(corpo); return -1; }
  n = xtream_epg_parse(corpo, out, cap);
  free(corpo);
  return nv_perfil_sessao_atual(sessao) ? n : -1;
}

int xtream_url(const char *id, char *url, unsigned n) {
  const char *ext[2];
  // O primeiro formato da ordem (ver xtream_formatos). Era sempre .m3u8.
  int k = xtream_formatos_canal(id, ext);
  return xtream_url_formato(id, k > 0 ? ext[0] : "m3u8", url, n);
}

void xtream_fonte_selecionar(unsigned id) { atomic_store(&contaEditada, id); }
int xtream_fonte_configurado(unsigned id) {
  unsigned a = usarFonte(id); int r = xtream_configurado(); usarFonte(a); return r;
}
int xtream_fonte_canais(unsigned id, XtreamCanal *out, int max, uint64_t sessao) {
  unsigned a = usarFonte(id); int n = xtream_canais_sessao(out, max, sessao);
  for (int i = 0; i < n; i++) { char original[80]; snprintf(original, sizeof original, "%s", out[i].id);
    if (!iptvid_montar("xtream:", id, original, out[i].id, sizeof out[i].id)) { n = i; break; } }
  usarFonte(a); return n;
}
typedef struct {
  unsigned id;
  void (*cat)(const char *, int, int, void *);
  XtreamCb cb;
  void *u;
} XtFonteFluxo;
static void fonteCategoria(const char *nome, int ordem, int n, void *u) {
  XtFonteFluxo *f = u;
  if (f->cat) f->cat(nome, ordem, n, f->u);
}
static int fonteCanal(const XtreamItem *item, void *u) {
  XtFonteFluxo *f = u;
  XtreamItem c = *item;
  char id[80];
  if (!iptvid_montar("xtream:", f->id, item->id, id, sizeof id)) return 0;
  c.id = id;
  return f->cb(&c, f->u);
}
int xtream_fonte_canais_cb(unsigned id, void (*cat)(const char *, int, int, void *),
                         XtreamCb cb, void *u, int *total, uint64_t sessao) {
  if (!cb) { if (total) *total = 0; return 0; }
  XtFonteFluxo f = {id, cat, cb, u};
  unsigned a = usarFonte(id);
  int n = xtream_canais_cb_sessao(fonteCategoria, fonteCanal, &f, total, sessao);
  usarFonte(a);
  return n;
}
int xtream_fonte_xmltv(unsigned id, char *out, unsigned n) {
  unsigned a = usarFonte(id); int r = xtream_url_xmltv(out, n); usarFonte(a); return r;
}
int xtream_fonte_total(unsigned id) {
  unsigned a = usarFonte(id); int r = xtream_ultimo_total(); usarFonte(a); return r;
}
int xtream_url_formato(const char *id, const char *ext, char *url, unsigned n) {
  unsigned f; char original[80]; if (!iptvid_ler("xtream:", id, &f, original, sizeof original)) return 0;
  unsigned a = usarFonte(f); int r = urlFormato(original, ext, url, n); usarFonte(a); return r;
}
int xtream_epg_curto(const char *id, XtreamProg *out, int max, int *status) {
  unsigned f; char original[80]; if (status) *status = 0;
  if (!iptvid_ler("xtream:", id, &f, original, sizeof original)) return -1;
  unsigned a = usarFonte(f); int r = epgCurto(original, out, max, status); retryCanal = retryAfterUltimo; usarFonte(a); return r;
}
int xtream_formatos_canal(const char *id, const char *ext[2]) {
  unsigned f; if (!iptvid_ler("xtream:", id, &f, NULL, 0)) return 0;
  unsigned a = usarFonte(f); int r = xtream_formatos(ext); usarFonte(a); return r;
}
int xtream_conta_canal(const char *id, XtreamConta *out) {
  unsigned f; if (!iptvid_ler("xtream:", id, &f, NULL, 0)) return 0;
  unsigned a = usarFonte(f); int r = xtream_conta(out); usarFonte(a); return r;
}
void xtream_formato_canal_funcionou(const char *id, const char *url) {
  unsigned f; if (!iptvid_ler("xtream:", id, &f, NULL, 0)) return;
  unsigned a = usarFonte(f); xtream_formato_funcionou(url); usarFonte(a);
}
void xtream_fonte_esquecer(unsigned id) {
  unsigned a = usarFonte(id); xtream_esquecer(); usarFonte(a);
}
int xtream_fonte_estado(unsigned id, int *http) {
  unsigned a = usarFonte(id); int r = xtream_ultima_falha(); if (http) *http = xtream_ultimo_http(); usarFonte(a); return r;
}

void xtream_fontes_invalidar(void) {
  pthread_mutex_lock(&fontesTrava);
  for(FonteContexto *c=fontesContextos;c;c=c->proximo){ pthread_mutex_lock(&c->trava_); c->lido_=0;
    c->servidor_[0]=c->usuario_[0]=c->senha_[0]=0;memset(&c->conta_,0,sizeof c->conta_);c->formatoOk_=-1;
    c->conexoesModo_=c->autoLimitada_=0;
    pthread_mutex_unlock(&c->trava_); }
  pthread_mutex_unlock(&fontesTrava);
}
