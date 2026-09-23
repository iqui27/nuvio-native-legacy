#include "xtream.h"
#include "rede.h"
#include "js.h"
#include "dados.h"
#include "perfis.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define XT_ARQ_FMT   "xtream-p%d.txt"
#define XT_MAX_CAT   256
#define XT_PRAZO_S   20

// --- cadastro -------------------------------------------------------------
// O mesmo desenho de stalker.c: uma trava para o cadastro, lido pelo fio do
// guia e pelo de desenho (Ajustes mostra o servidor), recarregado quando o
// perfil ativo muda.
static char servidor[256];      // com esquema, sem barra no fim
static char usuario[96];
static char senha[96];
static int  perfilLido = -1;
static int  lido;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

static const char *arquivo(void) {
  static char nome[40];
  snprintf(nome, sizeof nome, XT_ARQ_FMT, perfis_ativo());
  return nome;
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
  if (lido && p == perfilLido) return;
  perfilLido = p; lido = 1;
  servidor[0] = usuario[0] = senha[0] = 0;
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
  }
  free(b);
}

void xtream_carregar(void) {
  pthread_mutex_lock(&trava);
  carregarTravado();
  pthread_mutex_unlock(&trava);
}

static void gravar(void) {
  char txt[640];
  snprintf(txt, sizeof txt,
           "# CREDENCIAL. Usuario e senha do Xtream vao dentro de toda URL de\n"
           "# canal: valem como a assinatura. Nao versionar, nao empacotar, nao\n"
           "# colar em issue nem em log.\n"
           "servidor\t%s\nusuario\t%s\nsenha\t%s\n", servidor, usuario, senha);
  dados_gravar(arquivo(), txt);
}

// Tres setters e nao um: a tela edita um campo por vez, e um setter unico a
// obrigaria a ler a senha em claro so para regrava-la ao trocar o servidor.
void xtream_definir_servidor(const char *s) {
  pthread_mutex_lock(&trava);
  carregarTravado();
  normalizarServidor(s, servidor, sizeof servidor);
  gravar();
  pthread_mutex_unlock(&trava);
}
void xtream_definir_usuario(const char *u) {
  pthread_mutex_lock(&trava);
  carregarTravado();
  snprintf(usuario, sizeof usuario, "%s", u ? u : "");
  gravar();
  pthread_mutex_unlock(&trava);
}
void xtream_definir_senha(const char *s) {
  pthread_mutex_lock(&trava);
  carregarTravado();
  snprintf(senha, sizeof senha, "%s", s ? s : "");
  gravar();
  pthread_mutex_unlock(&trava);
}

void xtream_esquecer(void) {
  pthread_mutex_lock(&trava);
  dados_apagar(arquivo());
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
static char *chamar(const char *acao) {
  char srv[256], u[300], s[300], url[1100];
  pthread_mutex_lock(&trava);
  carregarTravado();
  snprintf(srv, sizeof srv, "%s", servidor);
  urlenc(usuario, u, sizeof u);
  urlenc(senha, s, sizeof s);
  pthread_mutex_unlock(&trava);
  if (!srv[0] || !u[0] || !s[0]) return NULL;
  snprintf(url, sizeof url, "%s/player_api.php?username=%s&password=%s&action=%s",
           srv, u, s, acao);
  return rede_baixar(url, XT_PRAZO_S);
}

// --- canais -------------------------------------------------------------
typedef struct { char id[24]; char nome[64]; } XtCat;

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

static int lerCategorias(XtCat *cats, int max) {
  char *corpo = chamar("get_live_categories");
  const char *p;
  int n = 0;
  if (!corpo) return 0;
  p = strchr(corpo, '[');
  p = p ? p + 1 : NULL;
  while (p && *p && n < max) {
    const char *fim;
    while (*p && (unsigned char)*p <= ' ') p++;
    if (*p != '{') break;
    fim = js_fim(p);
    if (!fim) break;
    if (campoTextoOuNumero(p, fim, "category_id", cats[n].id, sizeof cats[n].id) &&
        js_texto(p, fim, "category_name", cats[n].nome, sizeof cats[n].nome))
      n++;
    p = js_prox(fim);
  }
  free(corpo);
  return n;
}

static const char *nomeDaCategoria(const XtCat *cats, int n, const char *id) {
  int i;
  for (i = 0; i < n; i++) if (!strcmp(cats[i].id, id)) return cats[i].nome;
  return "Outros";
}

// Escrito so pelo fio do guia (o unico que chama xtream_canais) e lido pelo
// mesmo fio antes de publicar: nao precisa de trava.
static int ultimaFalha = XT_OK;
int xtream_ultima_falha(void) { return ultimaFalha; }

int xtream_canais(XtreamCanal *saida, int max) {
  static XtCat cats[XT_MAX_CAT];
  int nCat, n = 0;
  char *corpo;
  const char *p;
  ultimaFalha = XT_OK;
  if (!xtream_configurado() || !saida || max < 1) return 0;
  nCat = lerCategorias(cats, XT_MAX_CAT);
  corpo = chamar("get_live_streams");
  if (!corpo) {
    printf("[xtream] servidor nao respondeu a lista de canais\n");
    ultimaFalha = XT_SEM_RESPOSTA;
    return 0;
  }
  // Credencial errada nao e "[]": o servidor responde {"user_info":{"auth":0}}
  // ou uma pagina de erro. Sem array na raiz, e isso.
  p = strchr(corpo, '[');
  if (!p || (corpo[0] != '[' && strstr(corpo, "\"auth\":0"))) {
    printf("[xtream] servidor recusou a credencial (auth 0)\n");
    ultimaFalha = XT_RECUSOU;
    free(corpo);
    return 0;
  }
  p++;
  while (p && *p && n < max) {
    const char *fim;
    char sid[24], cat[24];
    XtreamCanal c;
    while (*p && (unsigned char)*p <= ' ') p++;
    if (*p != '{') break;
    fim = js_fim(p);
    if (!fim) break;
    memset(&c, 0, sizeof c);
    if (campoTextoOuNumero(p, fim, "stream_id", sid, sizeof sid) &&
        js_texto(p, fim, "name", c.nome, sizeof c.nome) && c.nome[0]) {
      snprintf(c.id, sizeof c.id, "xtream:%s", sid);
      js_texto(p, fim, "stream_icon", c.logo, sizeof c.logo);
      js_texto(p, fim, "epg_channel_id", c.epgId, sizeof c.epgId);
      if (!campoTextoOuNumero(p, fim, "category_id", cat, sizeof cat)) cat[0] = 0;
      snprintf(c.categoria, sizeof c.categoria, "%s", nomeDaCategoria(cats, nCat, cat));
      saida[n++] = c;
    }
    p = js_prox(fim);
  }
  free(corpo);
  // Contagem, e so: servidor, usuario e senha nao entram em log (registro.c
  // desenha o stdout na tela).
  printf("[xtream] %d canal(is) em %d categoria(s)\n", n, nCat);
  return n;
}

// --- reproducao ---------------------------------------------------------
int xtream_e_id(const char *id) {
  return id && !strncmp(id, "xtream:", 7);
}

int xtream_url(const char *id, char *url, unsigned n) {
  char u[300], s[300];
  if (!xtream_e_id(id) || !url || n < 2) return 0;
  pthread_mutex_lock(&trava);
  carregarTravado();
  if (!servidor[0] || !usuario[0] || !senha[0]) { pthread_mutex_unlock(&trava); return 0; }
  urlenc(usuario, u, sizeof u);
  urlenc(senha, s, sizeof s);
  // .m3u8 e nao .ts: o pipeline da LG toca HLS ao vivo bem (e o que os
  // addons de canal entregam); o .ts cru e um fluxo sem indice, que o uMS
  // aceita mas sem buffer previsivel.
  snprintf(url, n, "%s/live/%s/%s/%s.m3u8", servidor, u, s, id + 7);
  pthread_mutex_unlock(&trava);
  return 1;
}
