// OPENSEEK: o lado com rede, fios, SDL e GL. Ver openseek.h.
//
// FIOS (todos fora do desenho; o desenho so troca ponteiros sob a trava):
//   controle   — cria o trabalho (POST /v1/previews), consulta o status (long-poll
//                curto enquanto avanca, 10 s parado, nada depois de complete),
//                baixa o VTT novo quando vtt_version sobe e o troca SEM soltar
//                texturas cuja chave (URL sem t/exp/cap) nao mudou; renova as
//                URLs de capacidade antes do exp.
//   prioridade — POST /priority com o alvo do avanco, no maximo 8/s, o ultimo
//                vence; at_ms = null quando o avanco termina. Ninguem espera.
//   busca      — um quadro por vez (o atual primeiro, depois os vizinhos):
//                baixa, confere bytes e dimensoes ANTES de decodificar, recorta.
//                JPEGs num LRU de 8, a folha decodificada (so no layout de
//                folhas) uma por vez, como no Seekr.
// O desenho (openseek_quadros) sobe os recortes prontos para texturas num LRU
// de 12 e nunca espera rede nem decodificacao. Sem quadro do ponto: placeholder
// com o anel e o tempo-alvo — nunca a ultima textura usada (SCRUBBING §5).
#include "openseek.h"
#include "openseekproto.h"
#include "osp1.h"
#include "seekrvtt.h"
#include "rede.h"
#include "dados.h"
#include "gfx.h"
#include "jpegrapido.h"
#include "perfisessao.h"
#include "idbase.h"
#include <SDL2/SDL.h>
#ifndef __EMSCRIPTEN__
#include <SDL2/SDL_image.h>
#endif
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

_Static_assert(OS_SEEKR_DESLIGADO == SEEKR_DESLIGADO && OS_SEEKR_BUSCANDO == SEEKR_BUSCANDO &&
               OS_SEEKR_PRONTO == SEEKR_PRONTO && OS_SEEKR_SEM_PREVIA == SEEKR_SEM_PREVIA &&
               OS_SEEKR_CHAVE_RECUSADA == SEEKR_CHAVE_RECUSADA &&
               OS_SEEKR_LIMITE_PROVEDOR == SEEKR_LIMITE_PROVEDOR &&
               OS_SEEKR_REDE_INDISPONIVEL == SEEKR_REDE_INDISPONIVEL,
               "openseekproto.h mirrors the SEEKR_* states");

#define OS_JPG_LRU 1024
#define OS_JPG_BYTES_MAX (48u * 1024u * 1024u)
#define OS_TEX_LRU 96
#define OS_PRONTOS 24
#define OS_QUERO 24
#define OS_UPLOAD_MAX 8        // maximo de texturas subidas por quadro de desenho
#define OS_FOLHAS_DEC 2        // ate 2 folhas (spritesheets) decodificadas em RAM
#define OS_FALHAS_MAX 16
#define OS_INICIO_MS 1500L     // 1.5 s de reproducao confirmada (ou o 1o avanco)
#define OS_PRIO_MIN_MS 125u    // <= 8/s
#define OS_FALHA_MS 2000u      // um quadro que falhou espera isto antes de tentar de novo
#define OS_IDLE_PREFETCH_MS 25u

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  sinal = PTHREAD_COND_INITIALIZER;

// --- configuracao (openseek.txt) -------------------------------------------------
static int  carregado;
static char cfgBase[OS_BASE_MAX], cfgPino[OS_PINO_MAX], cfgToken[OS_TOKEN_MAX], cfgDevice[20];
static int  cfgPapel;                          // OS_PAPEL_*: vem do servidor no pareamento
static int  uiEstado = OSU_NAO_CONFIGURADO;   // ultimo resultado de parear/verificar
static int  pareando;                          // 1 com o fio de pareamento vivo
static int  verificando;                       // 1 com o fio de conferencia vivo

static void zerar(void *p, size_t n) { volatile unsigned char *v = p; while (n--) *v++ = 0; }

static void gravarCfg(void) {   // com a trava
  char t[OS_BASE_MAX + OS_PINO_MAX + OS_TOKEN_MAX + 112];
  if (!cfgBase[0]) { dados_apagar("openseek.txt"); return; }
  snprintf(t, sizeof t, "base=%s\npino=%s\ntoken=%s\ndevice=%s\nrole=%s\n", cfgBase,
           cfgToken[0] ? cfgPino : "", cfgToken, cfgToken[0] ? cfgDevice : "",
           cfgToken[0] && cfgPapel == OS_PAPEL_CONVIDADO ? "guest" : "owner");
  if (!dados_gravar("openseek.txt", t)) { printf("[openseek] config save=failed\n"); fflush(stdout); }
  zerar(t, sizeof t);
}
static void linhaCfg(const char *txt, const char *nome, char *dst, size_t n) {
  size_t k = strlen(nome);
  const char *l = txt;
  dst[0] = 0;
  while (l && *l) {
    const char *fim = l + strcspn(l, "\r\n");
    if ((size_t)(fim - l) > k && !strncmp(l, nome, k) && l[k] == '=') {
      size_t v = (size_t)(fim - l) - k - 1;
      if (v < n) { memcpy(dst, l + k + 1, v); dst[v] = 0; }
      return;
    }
    l = fim + strspn(fim, "\r\n");
  }
}
void openseek_carregar(void) {
  char *t;
  pthread_mutex_lock(&trava);
  if (carregado) { pthread_mutex_unlock(&trava); return; }
  carregado = 1;
  t = dados_ler("openseek.txt");
  if (t) {
    char b[OS_BASE_MAX];
    linhaCfg(t, "base", b, sizeof b);
    if (!openseek_normalizar_endereco(b, cfgBase, sizeof cfgBase)) cfgBase[0] = 0;
    linhaCfg(t, "pino", cfgPino, sizeof cfgPino);
    linhaCfg(t, "token", cfgToken, sizeof cfgToken);
    linhaCfg(t, "device", cfgDevice, sizeof cfgDevice);
    { char papel[12]; linhaCfg(t, "role", papel, sizeof papel); cfgPapel = openseek_papel_de(papel); }
    if (!rede_pino_valido(cfgPino) || !cfgToken[0] || !cfgBase[0]) {
      cfgPino[0] = cfgToken[0] = cfgDevice[0] = 0; cfgPapel = OS_PAPEL_DONO;
    }
    zerar(t, strlen(t)); free(t);
  }
  uiEstado = !cfgBase[0] ? OSU_NAO_CONFIGURADO : cfgToken[0] ? OSU_PAREADO : OSU_NAO_PAREADO;
  pthread_mutex_unlock(&trava);
}
const char *openseek_endereco(void) {
  openseek_carregar();
  return cfgBase;   // escrita so pelos Ajustes, no mesmo fio
}
int openseek_papel(void) {
  int r;
  openseek_carregar();
  pthread_mutex_lock(&trava); r = cfgToken[0] ? cfgPapel : OS_PAPEL_DONO; pthread_mutex_unlock(&trava);
  return r;
}
int openseek_pareado(void) {
  int r;
  openseek_carregar();
  pthread_mutex_lock(&trava); r = cfgToken[0] && cfgPino[0] && cfgBase[0]; pthread_mutex_unlock(&trava);
  return r;
}
static void sessaoEncerrar(void);  // adiante
// A previa compartilhada conhecida (a do video atual ou a ultima): so para a tela e para os ajustes.
static struct {
  int valido, aprox, verif;
  char id[33], tipo[24];
  double a, b;
  int ultimo; char ultimoMsg[96];     // resultado da ultima acao (0 nada, 1 ok, 2 falhou)
} compart;
int openseek_definir_endereco(const char *dig) {
  char nova[OS_BASE_MAX];
  openseek_carregar();
  if (!dig || !*dig) nova[0] = 0;
  else if (!openseek_normalizar_endereco(dig, nova, sizeof nova)) return 0;
  sessaoEncerrar();
  pthread_mutex_lock(&trava);
  if (strcmp(nova, cfgBase)) {
    snprintf(cfgBase, sizeof cfgBase, "%s", nova);
    zerar(cfgPino, sizeof cfgPino); zerar(cfgToken, sizeof cfgToken); cfgDevice[0] = 0; cfgPapel = OS_PAPEL_DONO;
    uiEstado = cfgBase[0] ? OSU_NAO_PAREADO : OSU_NAO_CONFIGURADO;
    gravarCfg();
  }
  pthread_mutex_unlock(&trava);
  return 1;
}
void openseek_esquecer(void) {
  openseek_carregar();
  sessaoEncerrar();
  pthread_mutex_lock(&trava);
  zerar(cfgPino, sizeof cfgPino); zerar(cfgToken, sizeof cfgToken); cfgDevice[0] = 0; cfgPapel = OS_PAPEL_DONO;
  uiEstado = cfgBase[0] ? OSU_NAO_PAREADO : OSU_NAO_CONFIGURADO;
  compart.valido = 0;
  gravarCfg();
  pthread_mutex_unlock(&trava);
}
int openseek_status(char *host, size_t n) {
  int st;
  openseek_carregar();
  pthread_mutex_lock(&trava);
  st = pareando ? OSU_PAREANDO : uiEstado;
  if (host && n) {
    const char *h = cfgBase[0] ? cfgBase + 8 : "";
    size_t k = strlen(h);
    const char *dois = h[0] == '[' ? strstr(h, "]:") : strrchr(h, ':');
    if (dois) k = (size_t)(dois - h) + (h[0] == '[');
    if (k >= n) k = n - 1;
    memcpy(host, h, k); host[k] = 0;
  }
  pthread_mutex_unlock(&trava);
  return st;
}
const char *openseek_status_rotulo(int st) {
  switch (st) {
    case OSU_NAO_CONFIGURADO: return "Informe o endereço do servidor";
    case OSU_NAO_PAREADO: return "Não pareado";
    case OSU_PAREANDO: return "Pareando…";
    case OSU_VERIFICANDO: return "Conferindo o servidor…";
    case OSU_CONVITE: return "Convite inválido, expirado ou já usado";
    case OSU_REVOGADO: return "Acesso revogado ou expirado: peça um novo convite";
    case OSU_LIMITE: return "Muitas tentativas: aguarde um minuto";
    case OSU_PAREADO: return "Pareado";
    case OSU_SEM_CONEXAO: return "Sem conexão com o servidor";
    case OSU_CERTIFICADO: return "O certificado do servidor mudou";
    case OSU_SEM_SUPORTE: return "Este aparelho não suporta conexão segura com o OpenSeek";
    default: return "O pareamento falhou";
  }
}

// --- pedidos ---------------------------------------------------------------------
typedef struct { volatile int *fim; } Parar;
static int pararFn(void *u) { const Parar *p = u; return p && p->fim && *p->fim; }

// Um pedido do OpenSeek. Devolve o status HTTP (0 sem resposta) e preenche r.
static int chamar(const char *metodo, const char *url, const char *token, const char *pino,
                  int semPino, const char *corpo, unsigned prazo, volatile int *fim,
                  RedeResposta *r) {
  OpenSeekPedido o;
  Parar pa = { fim };
  memset(r, 0, sizeof *r);
  if (!openseek_pedido(&o, metodo, url, token, pino, semPino, corpo, corpo ? strlen(corpo) : 0, prazo)) {
    printf("[openseek] request refused locally (policy): pin=%s first_contact=%d body=%s\n",
           pino && *pino ? "yes" : "no", semPino, corpo ? "yes" : "no"); fflush(stdout);
    r->erro = REDE_ENTRADA; return 0;
  }
  if (fim) { o.p.parar = pararFn; o.p.parar_usuario = &pa; }
  rede_pedir(&o.p, r);
  zerar(o.auth, sizeof o.auth);
  return r->status;
}
static int uiDeErro(const RedeResposta *r) {
  if (r->erro == REDE_INDISPONIVEL || r->erro == REDE_ENTRADA) return OSU_SEM_SUPORTE;
  if (r->curl_erro == REDE_CURL_PINO_DIFERENTE) return OSU_CERTIFICADO;
  if (r->erro != REDE_OK || !r->status || r->status >= 500) return OSU_SEM_CONEXAO;
  return OSU_FALHOU;
}

// --- pareamento (OSP1) -----------------------------------------------------------
typedef struct { char base[OS_BASE_MAX], codigo[OSP1_CONVITE_SEGREDO + 4], convite[40], pino[OS_PINO_MAX]; int forma; } Pareio;
static void *parearFio(void *u) {
  Pareio *pp = u;
  OpenSeekStart s;
  OpenSeekClaim c;
  RedeResposta r;
  unsigned char k[32];
  char url[OS_BASE_MAX + 32], corpo[256], prova[65], esperada[65];
  int res = OSU_FALHOU, http;
  const char *device = "Nuvio";
  snprintf(url, sizeof url, "%s/v1/pair/start", pp->base);
  if (!openseek_json_start(corpo, sizeof corpo, pp->forma == 2 ? pp->convite : NULL)) goto fim;
  // Primeiro contato: TLS sem pino, sem segredo nenhum (CONTRACT-v1). Um convite que traz o pino (p=) ja o usa aqui.
  http = chamar("POST", url, NULL, pp->pino[0] ? pp->pino : NULL, pp->pino[0] ? 0 : 1, corpo, OS_PRAZO_API_MS, NULL, &r);
  if (http != 200 || !r.corpo || !openseek_ler_start(r.corpo, &s)) {
    res = http == 429 ? OSU_LIMITE : (http == 409 && pp->forma == 2) ? OSU_CONVITE :
          http == 200 || http == 409 ? OSU_FALHOU : uiDeErro(&r);
    printf("[openseek] pair step=start http=%d result=%d rede_erro=%d curl_erro=%d (net/tls/pin/invite: %s)\n", http, res, (int)r.erro, (int)r.curl_erro,
           res == OSU_CERTIFICADO ? "pin" : res == OSU_CONVITE ? "invite" : res == OSU_LIMITE ? "rate-limit" :
           res == OSU_SEM_SUPORTE ? "local-policy-or-no-tls" : res == OSU_SEM_CONEXAO ? "net-or-tls" : "other"); fflush(stdout);
    rede_resposta_limpar(&r);
    goto fim;
  }
  rede_resposta_limpar(&r);
  if (pp->pino[0] && strcmp(pp->pino, s.pino)) {   // o convite dizia outro certificado: nada segue
    printf("[openseek] pair step=start result=pin_differs_from_invite\n"); fflush(stdout);
    res = OSU_CERTIFICADO; goto fim;
  }
  if (s.iter < OSP1_ITER_MIN) {            // rebaixamento: recusa
    printf("[openseek] pair step=kdf result=iterations_refused\n"); fflush(stdout);
    goto fim;
  }
  {
    Uint32 t0 = SDL_GetTicks();
    if (!osp1_derivar(pp->codigo, s.pairId, s.salt, s.pino, s.iter, OSP1_ITER_MIN, k, NULL, NULL)) goto fim;
    printf("[openseek] pair step=kdf ms=%u\n", SDL_GetTicks() - t0); fflush(stdout);
  }
  osp1_prova_cliente(k, s.pairId, device, prova);
  if (!openseek_json_claim(corpo, sizeof corpo, s.pairId, device, prova)) goto fim;
  snprintf(url, sizeof url, "%s/v1/pair/claim", pp->base);
  // Daqui em diante: so com o pino que o start mostrou (SR-2.4).
  http = chamar("POST", url, NULL, s.pino, 0, corpo, OS_PRAZO_API_MS, NULL, &r);
  if (http != 200 || !r.corpo || !openseek_ler_claim(r.corpo, &c)) {
    res = http == 429 ? OSU_LIMITE : (http == 403 && pp->forma == 2) ? OSU_CONVITE :
          http == 403 || http == 200 ? OSU_FALHOU : uiDeErro(&r);
    printf("[openseek] pair step=claim http=%d result=%d\n", http, res); fflush(stdout);
    rede_resposta_limpar(&r);
    goto fim;
  }
  rede_resposta_limpar(&r);
  osp1_prova_servidor(k, s.pairId, c.token, esperada);
  if (!osp1_iguais(esperada, c.prova)) {   // SR-2.6: sem prova, nada e guardado
    printf("[openseek] pair step=server_proof result=mismatch\n"); fflush(stdout);
    goto fim;
  }
  pthread_mutex_lock(&trava);
  if (!strcmp(cfgBase, pp->base)) {
    snprintf(cfgPino, sizeof cfgPino, "%s", s.pino);
    snprintf(cfgToken, sizeof cfgToken, "%s", c.token);
    snprintf(cfgDevice, sizeof cfgDevice, "%s", c.deviceId);
    cfgPapel = openseek_papel_de(c.papel);
    compart.valido = 0;
    gravarCfg();
    res = OSU_PAREADO;
  }
  pthread_mutex_unlock(&trava);
  printf("[openseek] pair result=%s\n", res == OSU_PAREADO ? "paired" : "address_changed"); fflush(stdout);
fim:
  zerar(k, sizeof k); zerar(&s, sizeof s); zerar(&c, sizeof c); zerar(corpo, sizeof corpo);
  zerar(prova, sizeof prova); zerar(esperada, sizeof esperada);
  pthread_mutex_lock(&trava);
  uiEstado = res; pareando = 0;
  pthread_mutex_unlock(&trava);
  zerar(pp, sizeof *pp); free(pp);
  return NULL;
}
int openseek_parear(const char *codigo) {
  Pareio *pp;
  pthread_t fio;
  OpenSeekEntrada ent;
  openseek_carregar();
  pp = calloc(1, sizeof *pp);
  if (!pp) return 0;
  if (!openseek_ler_entrada(codigo, &ent)) { free(pp); return 0; }           // codigo de 8, convite ou codigo de 16
  if (ent.forma == 2 && ent.base[0] && !openseek_definir_endereco(ent.base)) { free(pp); return 0; }
  snprintf(pp->codigo, sizeof pp->codigo, "%s", ent.segredo);
  snprintf(pp->convite, sizeof pp->convite, "%s", ent.convite);
  snprintf(pp->pino, sizeof pp->pino, "%s", ent.pino);
  pp->forma = ent.forma;
  sessaoEncerrar();
  pthread_mutex_lock(&trava);
  if (pareando || !cfgBase[0]) { pthread_mutex_unlock(&trava); zerar(pp, sizeof *pp); free(pp); return 0; }
  snprintf(pp->base, sizeof pp->base, "%s", cfgBase);
  // Parear de novo substitui o anterior so se der certo; ate la nada muda.
  pareando = 1;
  pthread_mutex_unlock(&trava);
  if (pthread_create(&fio, NULL, parearFio, pp) == 0) { pthread_detach(fio); return 1; }
  pthread_mutex_lock(&trava); pareando = 0; uiEstado = OSU_FALHOU; pthread_mutex_unlock(&trava);
  zerar(pp, sizeof *pp); free(pp);
  return 0;
}
typedef struct { char base[OS_BASE_MAX], pino[OS_PINO_MAX], token[OS_TOKEN_MAX]; } Credencial;
static void *verificarFio(void *u) {
  Credencial *cr = u;
  char url[OS_BASE_MAX + 32];
  RedeResposta r;
  int http, res;
  snprintf(url, sizeof url, "%s/v1/pair/status", cr->base);
  http = chamar(NULL, url, cr->token, cr->pino, 0, NULL, OS_PRAZO_API_MS, NULL, &r);
  res = http == 200 ? OSU_PAREADO : http == 401 ? OSU_REVOGADO : uiDeErro(&r);
  rede_resposta_limpar(&r);
  printf("[openseek] check http=%d result=%d\n", http, res); fflush(stdout);
  pthread_mutex_lock(&trava);
  if (!pareando && !strcmp(cfgToken, cr->token)) uiEstado = res;
  verificando = 0;
  pthread_mutex_unlock(&trava);
  zerar(cr, sizeof *cr); free(cr);
  return NULL;
}
void openseek_verificar(void) {
  Credencial *cr;
  pthread_t fio;
  openseek_carregar();
  cr = calloc(1, sizeof *cr);
  if (!cr) return;
  pthread_mutex_lock(&trava);
  if (pareando || verificando || !cfgToken[0]) { pthread_mutex_unlock(&trava); free(cr); return; }
  snprintf(cr->base, sizeof cr->base, "%s", cfgBase);
  snprintf(cr->pino, sizeof cr->pino, "%s", cfgPino);
  snprintf(cr->token, sizeof cr->token, "%s", cfgToken);
  verificando = 1; uiEstado = OSU_VERIFICANDO;
  pthread_mutex_unlock(&trava);
  if (pthread_create(&fio, NULL, verificarFio, cr) == 0) pthread_detach(fio);
  else {
    pthread_mutex_lock(&trava); verificando = 0; uiEstado = OSU_PAREADO; pthread_mutex_unlock(&trava);
    zerar(cr, sizeof *cr); free(cr);
  }
}

// --- sessao (um video tocando) -----------------------------------------------------
typedef struct {
  char fetch[2400];              // URL da folha/quadro (com exp): capacidade
  char chaveJpg[240], chaveTex[256];
  int x, y, w, h, prewarm;
} Quero;
typedef struct {
  unsigned char *px; int w, h; unsigned g;
  char chaveTex[256];
} Pronto;

typedef struct Sessao {
  int refs;                 // com a trava
  volatile int fim;
  unsigned g;               // geracao do desenho quando nasceu
  char base[OS_BASE_MAX], pino[OS_PINO_MAX], token[OS_TOKEN_MAX];
  char *url, *cabs;         // SEGREDOS: so no corpo do POST
  long durMs;
  char imdb[32], hash[24], arquivo[256];
  char idOrig[64];   // the id the player gave (tmdb:2734...): `imdb` becomes its IMDb id for the server
  int t, e, convidado;      // convidado: nunca ha URL de video nem trabalho neste servidor ...
  int naoCompart, querUrl, contribuindo;   // ... a nao ser que o DONO autorize ESTE aparelho a contribuir (querUrl copia o link so entao)
  unsigned long long tamanho;
  // dinamico (trava)
  long playheadMs, atMs, ultimoPlay, ultimoQuadrosMs;
  int scrubbing, scrubDir, iniciar, limparFolhas;
  long jogadoMs;
  int prioSujo; Uint32 prioTick, prioPlayTick;
  char id[33], vttUrl[2300];
  long versaoServidor, versaoAplicada;
  int jobEstado, renovar, temVtt;
  long long exp;
  Quero quero[OS_QUERO]; volatile int nQuero;
  char emVoo[256];          // chave de textura que o fio de busca esta trazendo
  Uint32 ultimoCriar;
} Sessao;

static Sessao *atual;
static unsigned geracao = 1;
static int  estado = SEEKR_DESLIGADO, motivo = OSM_NENHUM;
static long long retryUtc;
static unsigned ultimaLatencia; static int ultimoHttp;
static SeekrVtt vtt;                  // o VTT em uso (desenho le sob a trava)
static char (*vttChaves)[256];        // chave de textura de cada cue
static char (*vttFolhaChaves)[240];   // chave normalizada de cada folha
static Pronto prontos[OS_PRONTOS];

static void soltarSessao(Sessao *s) {   // com a trava
  if (!s || --s->refs > 0) return;
  if (s->url) { zerar(s->url, strlen(s->url)); free(s->url); }
  if (s->cabs) { zerar(s->cabs, strlen(s->cabs)); free(s->cabs); }
  zerar(s->token, sizeof s->token); zerar(s->vttUrl, sizeof s->vttUrl);
  for (int i = 0; i < OS_QUERO; i++) zerar(s->quero[i].fetch, sizeof s->quero[i].fetch);
  free(s);
}
static void soltarVtt(void) {          // com a trava
  seekr_vtt_liberar(&vtt);
  free(vttChaves); vttChaves = NULL;
  free(vttFolhaChaves); vttFolhaChaves = NULL;
}
static void soltarProntos(void) {
  for (int i = 0; i < OS_PRONTOS; i++) { free(prontos[i].px); prontos[i].px = NULL; }
}
static void definirEstado(Sessao *s, int est, int mot) {   // com a trava
  if (s != atual || est < 0) return;
  estado = est; motivo = mot;
}

// DELETE de cortesia ao sair (o servidor para o trabalho; quadros prontos ficam).
typedef struct { char url[OS_BASE_MAX + 64], pino[OS_PINO_MAX], token[OS_TOKEN_MAX]; } Cancelar;
static void *cancelarFio(void *u) {
  Cancelar *c = u;
  RedeResposta r;
  int http = chamar("DELETE", c->url, c->token, c->pino, 0, NULL, 3000, NULL, &r);
  rede_resposta_limpar(&r);
  printf("[openseek] cancel http=%d\n", http); fflush(stdout);
  zerar(c, sizeof *c); free(c);
  return NULL;
}
static void sessaoEncerrar(void) {
  Sessao *s;
  Cancelar *c = NULL;
  pthread_mutex_lock(&trava);
  s = atual; atual = NULL;
  geracao++;
  soltarVtt(); soltarProntos();
  estado = SEEKR_DESLIGADO; motivo = OSM_NENHUM;
  if (s) {
    s->fim = 1;
    if (s->id[0] && (c = calloc(1, sizeof *c)) != NULL) {
      snprintf(c->url, sizeof c->url, "%s/v1/previews/%s", s->base, s->id);
      snprintf(c->pino, sizeof c->pino, "%s", s->pino);
      snprintf(c->token, sizeof c->token, "%s", s->token);
    }
    soltarSessao(s);
  }
  pthread_cond_broadcast(&sinal);
  pthread_mutex_unlock(&trava);
  if (c) {
    pthread_t fio;
    if (pthread_create(&fio, NULL, cancelarFio, c) == 0) pthread_detach(fio);
    else { zerar(c, sizeof *c); free(c); }
  }
}

// Espera no sinal ate `ms` (ou ate a sessao acabar). Com a trava.
static void esperar(Sessao *s, unsigned ms) {
  struct timespec ts;
  if (s->fim) return;
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += ms / 1000; ts.tv_nsec += (long)(ms % 1000) * 1000000L;
  if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
  pthread_cond_timedwait(&sinal, &trava, &ts);
}

// Espera ate o prazo de verdade: o sinal acorda a cada passo do avanco, e uma
// espera so de `esperar` encolheria os recuos. Com a trava.
static void esperarPrazo(Sessao *s, unsigned ms) {
  Uint32 ate = SDL_GetTicks() + ms;
  for (;;) {
    Sint32 falta = (Sint32)(ate - SDL_GetTicks());
    if (s->fim || falta <= 0 || s->renovar) return;
    esperar(s, falta > 1000 ? 1000u : (unsigned)falta);
  }
}
// --- FONTE ALTERNATIVA PARA MINIATURAS (opcional; DESLIGADA por padrao) --------------
// Liga com um arquivo "openseek-alt.txt" contendo 1 nos dados do app. Desligada, nada
// daqui roda: o POST de criacao e o de sempre, nao ha fio extra, nada vai ao servidor.
// Ligada: (1) o POST de criacao leva so METADADOS dos candidatos (nenhuma URL); (2) um
// fio conta 1x por segundo a saude da reproducao (buffer, rebuffering, host do CDN);
// (3) se o servidor pedir UM candidato (alt.want), a interface entrega a URL dele UMA vez
// e este fio a envia, com TLS fixado, em POST /alt-source. Nunca ha chamada a API de
// debrid, nunca resolve por miniatura, e o video em reproducao nao e tocado.
static struct {
  int carregado, ligado;
  OpenSeekAltCand play, c[OS_ALT_MAX];
  char nomes[OS_ALT_MAX + 1][200];
  int n;
  long bufMs; int rebuf; char host[128];
  char pedidoCid[24]; int pedido;        // o servidor pediu este candidato
  char *url, *cabs;                      // SEGREDOS entregues pela interface: so ate o POST
  char enviadoCid[24]; int fechado;
} alt;

static void altCarregar(void) {          // com a trava
  char *t;
  if (alt.carregado) return;
  alt.carregado = 1;
  t = dados_ler("openseek-alt.txt");
  alt.ligado = t && t[0] == '1';
  if (t) { zerar(t, strlen(t)); free(t); }
}
static void altNovaSessao(void) {        // com a trava
  altCarregar();
  alt.pedido = 0; alt.fechado = 0; alt.enviadoCid[0] = 0; alt.pedidoCid[0] = 0;
  if (alt.url) { zerar(alt.url, strlen(alt.url)); free(alt.url); alt.url = NULL; }
  if (alt.cabs) { zerar(alt.cabs, strlen(alt.cabs)); free(alt.cabs); alt.cabs = NULL; }
}
int openseek_alt_ligado(void) {
  int v;
  pthread_mutex_lock(&trava); altCarregar(); v = alt.ligado; pthread_mutex_unlock(&trava);
  return v;
}
void openseek_alt_atualizar(const OpenSeekAltCand *play, const OpenSeekAltCand *c, int n,
                            long bufferMs, int rebuffering, const char *host) {
  pthread_mutex_lock(&trava);
  altCarregar();
  if (alt.ligado) {
    alt.bufMs = bufferMs; alt.rebuf = rebuffering ? 1 : 0;
    snprintf(alt.host, sizeof alt.host, "%s", host ? host : "");
    if (play && c && n > 0) {            // lista nova (o texto dos nomes e copiado: o dono e outro fio)
      if (n > OS_ALT_MAX) n = OS_ALT_MAX;
      alt.play = *play;
      snprintf(alt.nomes[OS_ALT_MAX], sizeof alt.nomes[0], "%s", play->nome ? play->nome : "");
      alt.play.nome = alt.nomes[OS_ALT_MAX]; alt.play.codec = alt.play.container = NULL;
      for (int i = 0; i < n; i++) {
        alt.c[i] = c[i];
        snprintf(alt.nomes[i], sizeof alt.nomes[0], "%s", c[i].nome ? c[i].nome : "");
        alt.c[i].nome = alt.nomes[i]; alt.c[i].codec = alt.c[i].container = NULL;
      }
      alt.n = n;
    }
  }
  pthread_mutex_unlock(&trava);
}
int openseek_alt_pedido(char *cid, size_t n) {
  int r = 0;
  pthread_mutex_lock(&trava);
  if (alt.ligado && alt.pedido && !alt.url && cid && n) { snprintf(cid, n, "%s", alt.pedidoCid); r = 1; }
  pthread_mutex_unlock(&trava);
  return r;
}
void openseek_alt_entregar(const char *cid, const char *url, const char *cabecalhos) {
  pthread_mutex_lock(&trava);
  if (alt.ligado && alt.pedido && !alt.url && cid && !strcmp(cid, alt.pedidoCid)) {
    alt.url = strdup(url ? url : ""); alt.cabs = strdup(cabecalhos ? cabecalhos : "");
    pthread_cond_broadcast(&sinal);
  }
  pthread_mutex_unlock(&trava);
}

static void *altFio(void *u) {
  Sessao *s = u;
  Uint32 ultTel = 0, ultSt = 0;
  Uint32 telEspera = 1000u;       // 1 Hz; mais devagar (nunca desligada) se o servidor recusar
  pthread_mutex_lock(&trava);
  while (!s->fim) {
    char id[33], base[OS_BASE_MAX], pino[OS_PINO_MAX], token[OS_TOKEN_MAX], host[128], cid[24] = "";
    char *url = NULL, *cabs = NULL;
    long buf; int rebuf, ativo;
    if (!s->id[0]) { esperar(s, 500); continue; }
    snprintf(id, sizeof id, "%s", s->id); snprintf(base, sizeof base, "%s", s->base);
    snprintf(pino, sizeof pino, "%s", s->pino); snprintf(token, sizeof token, "%s", s->token);
    snprintf(host, sizeof host, "%s", alt.host);
    buf = alt.bufMs; rebuf = alt.rebuf;
    ativo = alt.n > 0 && !alt.fechado;
    if (alt.pedido && alt.url) {         // a interface entregou a URL do candidato pedido
      url = alt.url; cabs = alt.cabs; alt.url = alt.cabs = NULL;
      snprintf(cid, sizeof cid, "%s", alt.pedidoCid);
      snprintf(alt.enviadoCid, sizeof alt.enviadoCid, "%s", cid);
      alt.pedido = 0;
    }
    pthread_mutex_unlock(&trava);
    {
      char alvo[OS_BASE_MAX + 96];
      RedeResposta r;
      int http;
      Uint32 agora = SDL_GetTicks();
      if (url) {
        char *corpo = malloc(12288);
        if (corpo && url[0] && openseek_json_alt_fonte(corpo, 12288, cid, url, cabs)) {
          snprintf(alvo, sizeof alvo, "%s/v1/previews/%s/alt-source", base, id);
          http = chamar("POST", alvo, token, pino, 0, corpo, OS_PRAZO_API_MS, &s->fim, &r);
          printf("[openseek] alt source offered http=%d\n", http); fflush(stdout);   // nunca a URL
          rede_resposta_limpar(&r);
        }
        if (corpo) { zerar(corpo, 12288); free(corpo); }
        zerar(url, strlen(url)); free(url);
        if (cabs) { zerar(cabs, strlen(cabs)); free(cabs); }
      }
      if (agora - ultTel >= telEspera) {
        char tb[256];
        ultTel = agora;
        if (openseek_json_telemetria(tb, sizeof tb, buf, rebuf, host, 1)) {
          snprintf(alvo, sizeof alvo, "%s/v1/previews/%s/telemetry", base, id);
          http = chamar("POST", alvo, token, pino, 0, tb, OS_PRAZO_API_MS, &s->fim, &r);
          // 404 tambem e "trabalho renovado/servidor reiniciado": o id novo chega logo. Nunca desligar de vez.
          telEspera = (http >= 400 && http < 500) ? 10000u : http == 0 ? 3000u : 1000u;
          rede_resposta_limpar(&r);
        }
      }
      if (ativo && agora - ultSt >= 2000u) {
        ultSt = agora;
        snprintf(alvo, sizeof alvo, "%s/v1/previews/%s", base, id);
        http = chamar(NULL, alvo, token, pino, 0, NULL, OS_PRAZO_API_MS, &s->fim, &r);
        if (http == 200 && r.corpo) {
          char est[16], want[24];
          if (openseek_ler_alt(r.corpo, est, sizeof est, want, sizeof want)) {
            pthread_mutex_lock(&trava);
            if (want[0] && strcmp(want, alt.enviadoCid) && !alt.pedido) {
              snprintf(alt.pedidoCid, sizeof alt.pedidoCid, "%s", want); alt.pedido = 1;
            }
            if (!strcmp(est, "active") || !strcmp(est, "rejected") || !strcmp(est, "failed")) alt.fechado = 1;
            pthread_mutex_unlock(&trava);
          }
        } else if (http == 404 || http == 401) {
          pthread_mutex_lock(&trava); alt.fechado = 1; pthread_mutex_unlock(&trava);
        }
        rede_resposta_limpar(&r);
      }
    }
    zerar(token, sizeof token);
    pthread_mutex_lock(&trava);
    esperar(s, 250);
  }
  soltarSessao(s);
  pthread_mutex_unlock(&trava);
  return NULL;
}

// A linha de estado dos Ajustes acompanha o ultimo pedido de API. Com a trava.
static void notarUi(int http, const RedeResposta *r) {
  if (pareando || verificando) return;
  if (http == 401) uiEstado = OSU_FALHOU;
  else if (http >= 200 && http < 300) uiEstado = OSU_PAREADO;
  else if (r->erro == REDE_CANCELADO || r->erro == REDE_GERACAO) return;
  else uiEstado = uiDeErro(r) == OSU_FALHOU ? OSU_PAREADO : uiDeErro(r);
}

// Instala um VTT novo SEM soltar texturas: as chaves (URL sem t/exp/cap) nao
// mudam para o mesmo quadro, e o LRU do desenho e por chave, nao por indice.
static void trocarVtt(Sessao *s, SeekrVtt *novo) {   // com a trava
  char (*ch)[256] = NULL;
  char (*fch)[240] = NULL;
  if (s != atual) { seekr_vtt_liberar(novo); return; }
  if (novo->nCues > 0) ch = calloc((size_t)novo->nCues, sizeof *ch);
  if (novo->nFolhas > 0) fch = calloc((size_t)novo->nFolhas, sizeof *fch);
  if ((novo->nCues > 0 && !ch) || (novo->nFolhas > 0 && !fch)) {
    free(ch); free(fch); seekr_vtt_liberar(novo); return;
  }
  for (int f = 0; f < novo->nFolhas; f++)
    openseek_chave(novo->folhas[f], fch[f], sizeof fch[f]);
  for (int i = 0; i < novo->nCues; i++) {
    const SeekrCue *q = &novo->cues[i];
    const char *k = (q->folha >= 0 && q->folha < novo->nFolhas) ? fch[q->folha] : "";
    if (k[0])
      snprintf(ch[i], sizeof ch[i], "%s#%d,%d,%d,%d", k, q->x, q->y, q->w, q->h);
  }
  soltarVtt();
  vtt = *novo; memset(novo, 0, sizeof *novo);
  vttChaves = ch;
  vttFolhaChaves = fch;
}

static int criarTrabalho(Sessao *s) {
  char url[OS_BASE_MAX + 32], *corpo;
  OpenSeekFonte f;
  OpenSeekPrevia p;
  RedeResposta r;
  int http, est, mot = OSM_NENHUM, ok = 0;
  Uint32 t0 = SDL_GetTicks();
  corpo = malloc(16385);
  if (!corpo) return 0;
  pthread_mutex_lock(&trava);
  f = (OpenSeekFonte){ s->url, s->cabs, s->durMs, s->imdb, s->t, s->e, s->tamanho,
                       s->hash, s->arquivo, s->playheadMs, s->scrubbing ? s->atMs : -1 };
  altCarregar();
  if (alt.ligado && alt.n > 0) {
    if (!openseek_json_previa_alt(corpo, 16385, &f, &alt.play, alt.c, alt.n)) corpo[0] = 0;
  } else if (!openseek_json_previa(corpo, 16385, &f)) corpo[0] = 0;
  pthread_mutex_unlock(&trava);
  if (!corpo[0]) {
    free(corpo);
    pthread_mutex_lock(&trava); definirEstado(s, SEEKR_SEM_PREVIA, OSM_FONTE); pthread_mutex_unlock(&trava);
    return -1;
  }
  snprintf(url, sizeof url, "%s/v1/previews", s->base);
  // SR-4: URL e cabecalhos so neste corpo, so com pino (openseek_pedido recusa sem).
  http = chamar("POST", url, s->token, s->pino, 0, corpo, OS_PRAZO_API_MS, &s->fim, &r);
  zerar(corpo, 16385); free(corpo);
  int leu = (http == 200 || http == 202) && r.corpo && openseek_ler_previa(r.corpo, s->base, &p);
  est = openseek_mapear(http, r.erro, r.curl_erro, leu ? &p : NULL, 0, &mot);
  pthread_mutex_lock(&trava);
  ultimoHttp = http; ultimaLatencia = SDL_GetTicks() - t0;
  notarUi(http, &r);
  if (s == atual && est >= 0) {
    if (leu) {
      int mudou = strcmp(s->vttUrl, p.vtt) != 0;
      if (strcmp(s->id, p.id)) {          // trabalho NOVO no servidor (reiniciado/renovado): a negociacao recomeca
        alt.fechado = 0; alt.pedido = 0; alt.enviadoCid[0] = 0;
      }
      snprintf(s->id, sizeof s->id, "%s", p.id);
      snprintf(s->vttUrl, sizeof s->vttUrl, "%s", p.vtt);
      s->exp = p.exp; s->jobEstado = p.estado; s->renovar = 0;
      if (p.versao > s->versaoServidor || mudou) s->versaoServidor = p.versao;
      if (mudou) s->temVtt = 0;
      ok = 1;
      if (p.estado == OS_FALHOU) definirEstado(s, SEEKR_SEM_PREVIA, OSM_FONTE);
      else if (!vtt.nCues) definirEstado(s, est, mot);
      retryUtc = 0;
    } else {
      definirEstado(s, est, mot);
      retryUtc = (long long)time(NULL) + (r.retry_after_s > 0 ? r.retry_after_s : 30);
    }
  }
  pthread_mutex_unlock(&trava);
  printf("[openseek] job create http=%d state=%d reason=%d latency_ms=%u\n", http, est, mot, ultimaLatencia);
  fflush(stdout);
  rede_resposta_limpar(&r);
  if (ok) return 1;
  // 401 (pareamento revogado), 400/413 (fonte recusada): nao insistir.
  return http == 401 || http == 400 || http == 413 ? -1 : 0;
}

// CONVIDADO: procura miniaturas de OUTRA versao do mesmo titulo na biblioteca do dono. So ids + duracao saem;
// nunca a URL do video; o servidor nao cria trabalho nenhum. 1 = achou, 0 = tentar depois, -1 = parar.
static int procurarCompart(Sessao *s) {
  char url[OS_BASE_MAX + 32], corpo[512];
  OpenSeekCompart p;
  RedeResposta r;
  int http, ok = 0, est, mot = OSM_NENHUM;
  Uint32 t0 = SDL_GetTicks();
  pthread_mutex_lock(&trava);
  if (!openseek_json_procura(corpo, sizeof corpo, s->imdb, s->t, s->e, s->durMs, s->tamanho)) corpo[0] = 0;
  pthread_mutex_unlock(&trava);
  if (!corpo[0]) {                       // sem id de titulo nao ha o que procurar: segue sem miniaturas
    pthread_mutex_lock(&trava); definirEstado(s, SEEKR_SEM_PREVIA, OSM_NAO_COMPARTILHADO); pthread_mutex_unlock(&trava);
    return -1;
  }
  snprintf(url, sizeof url, "%s/v1/lookup", s->base);
  http = chamar("POST", url, s->token, s->pino, 0, corpo, OS_PRAZO_API_MS, &s->fim, &r);
  int leu = http == 200 && r.corpo && openseek_ler_procura(r.corpo, s->base, &p);
  est = openseek_mapear(http, r.erro, r.curl_erro, NULL, 0, &mot);
  pthread_mutex_lock(&trava);
  ultimoHttp = http; ultimaLatencia = SDL_GetTicks() - t0;
  notarUi(http, &r);
  if (http == 401) uiEstado = OSU_REVOGADO;
  if (s == atual) {
    if (leu) {
      snprintf(s->id, sizeof s->id, "%s", p.id);
      if (strcmp(s->vttUrl, p.vtt)) { snprintf(s->vttUrl, sizeof s->vttUrl, "%s", p.vtt); s->temVtt = 0; }
      s->exp = p.exp; s->jobEstado = OS_COMPLETO; s->renovar = 0;
      compart.valido = 1; compart.aprox = p.aproximado; compart.verif = p.verificado;
      snprintf(compart.id, sizeof compart.id, "%s", p.id); snprintf(compart.tipo, sizeof compart.tipo, "%s", p.tipo);
      compart.a = p.a; compart.b = p.b;
      ok = 1; retryUtc = 0;
    } else if (http == 404) {            // nada compativel na biblioteca (ainda): a reproducao segue normal
      compart.valido = 0;
      s->naoCompart = 1;
      definirEstado(s, SEEKR_SEM_PREVIA, OSM_NAO_COMPARTILHADO);
      retryUtc = (long long)time(NULL) + 120;
    } else if (http == 403) {            // compartilhamento desligado no servidor do dono
      compart.valido = 0;
      definirEstado(s, SEEKR_SEM_PREVIA, OSM_COMPARTILHAMENTO_OFF);
      retryUtc = (long long)time(NULL) + 300;
    } else {
      definirEstado(s, est, mot);
      retryUtc = (long long)time(NULL) + (r.retry_after_s > 0 ? r.retry_after_s : 30);
    }
  }
  pthread_mutex_unlock(&trava);
  printf("[openseek] shared lookup http=%d ok=%d latency_ms=%u\n", http, ok, ultimaLatencia); fflush(stdout);
  rede_resposta_limpar(&r);
  if (ok) return 1;
  return http == 401 || http == 400 ? -1 : 0;
}

// CONTRIBUINTE: o titulo nao esta na biblioteca e o dono autorizou ESTE aparelho. Pergunta a permissao viva (GET /v1/pair/status),
// pega o link so agora (querUrl) e o entrega ao servidor do dono, que gera as miniaturas uma vez para todos.
// 1 = trabalho em andamento, 0 = tentar depois, -1 = parar.
static int contribuirCriar(Sessao *s) {
  char url[OS_BASE_MAX + 32], *corpo;
  OpenSeekFonte f;
  OpenSeekPrevia p;
  RedeResposta r;
  int http, ok = 0, leu, pode;
  if (!s->naoCompart) return 0;
  snprintf(url, sizeof url, "%s/v1/pair/status", s->base);
  http = chamar(NULL, url, s->token, s->pino, 0, NULL, OS_PRAZO_API_MS, &s->fim, &r);
  pode = http == 200 && r.corpo && openseek_ler_pode_contribuir(r.corpo);
  rede_resposta_limpar(&r);
  if (!pode) return 0;                                    // leitor: nada e enviado, nunca
  pthread_mutex_lock(&trava);
  s->querUrl = 1;
  for (int i = 0; i < 50 && !s->fim && !(s->url && s->url[0]); i++) esperar(s, 100);   // pPedir() copia o link
  corpo = malloc(16385);
  if (!corpo || !s->url || !s->url[0]) { pthread_mutex_unlock(&trava); free(corpo); return 0; }
  f = (OpenSeekFonte){ s->url, s->cabs, s->durMs, s->imdb, s->t, s->e, s->tamanho, s->hash, s->arquivo, 0, -1 };
  if (!openseek_json_contribuir(corpo, 16385, &f)) corpo[0] = 0;
  pthread_mutex_unlock(&trava);
  if (!corpo[0]) { zerar(corpo, 16385); free(corpo); return 0; }      // sem id tt / link https: nada a contribuir
  snprintf(url, sizeof url, "%s/v1/contribute", s->base);
  http = chamar("POST", url, s->token, s->pino, 0, corpo, OS_PRAZO_API_MS, &s->fim, &r);
  zerar(corpo, 16385); free(corpo);
  leu = http == 202 && r.corpo && openseek_ler_previa(r.corpo, s->base, &p);
  pthread_mutex_lock(&trava);
  if (s == atual) {
    if (leu) {
      snprintf(s->id, sizeof s->id, "%s", p.id);
      snprintf(s->vttUrl, sizeof s->vttUrl, "%s", p.vtt);
      s->exp = p.exp; s->jobEstado = p.estado; s->renovar = 0; s->contribuindo = 1; s->temVtt = 0;
      if (p.versao > s->versaoServidor) s->versaoServidor = p.versao;
      compart.valido = 0;
      definirEstado(s, SEEKR_BUSCANDO, OSM_CONTRIBUINDO);
      retryUtc = 0; ok = 1;
    } else if (http == 200) {                              // ja havia uma versao compativel na biblioteca: a procura a acha
      retryUtc = (long long)time(NULL) + 1;
    } else if (http == 429) {
      definirEstado(s, SEEKR_SEM_PREVIA, OSM_OCUPADO_CONTRIB);
      retryUtc = (long long)time(NULL) + (r.retry_after_s > 0 ? r.retry_after_s : 60);
    } else if (http == 403) {                              // sem permissao agora, ou o servidor recusou esta fonte
      definirEstado(s, SEEKR_SEM_PREVIA, OSM_NAO_COMPARTILHADO);
      retryUtc = (long long)time(NULL) + 600;
    } else {
      retryUtc = (long long)time(NULL) + 60;
    }
  }
  pthread_mutex_unlock(&trava);
  printf("[openseek] contribute http=%d ok=%d\n", http, ok); fflush(stdout);
  rede_resposta_limpar(&r);
  if (ok) return 1;
  return http == 401 ? -1 : 0;
}

static int baixarVtt(Sessao *s) {
  char url[2300];
  long versao;
  RedeResposta r;
  SeekrVtt v;
  int http, n = 0;
  pthread_mutex_lock(&trava);
  snprintf(url, sizeof url, "%s", s->vttUrl); versao = s->versaoServidor;
  pthread_mutex_unlock(&trava);
  if (!url[0]) return 0;
  // Capacidade: sem cabecalho de autorizacao (CONTRACT-v1 "Data plane").
  http = chamar(NULL, url, NULL, s->pino, 0, NULL, OS_PRAZO_DADOS_MS, &s->fim, &r);
  memset(&v, 0, sizeof v);
  if (http == 200 && r.corpo) n = seekr_vtt_ler_openseek(r.corpo, s->base, &v);
  pthread_mutex_lock(&trava);
  if (http == 200 && r.corpo && s == atual) {
    trocarVtt(s, &v);
    s->temVtt = 1;
    if (versao > s->versaoAplicada) s->versaoAplicada = versao;
    if (s->jobEstado == OS_FALHOU) definirEstado(s, SEEKR_SEM_PREVIA, OSM_FONTE);
    else if (!vtt.nCues && s->jobEstado == OS_PREPARANDO) definirEstado(s, SEEKR_BUSCANDO, OSM_PREPARANDO);
    else definirEstado(s, SEEKR_PRONTO, OSM_NENHUM);
    pthread_cond_broadcast(&sinal);
  } else if (http == 403) {
    s->renovar = 1;                     // capacidade vencida: novo POST
  } else if (s == atual && !vtt.nCues) {
    int mot = OSM_NENHUM, est = openseek_mapear(http, r.erro, r.curl_erro, NULL, 0, &mot);
    definirEstado(s, est, mot);
  }
  pthread_mutex_unlock(&trava);
  seekr_vtt_liberar(&v);
  printf("[openseek] vtt http=%d cues=%d version=%ld\n", http, n, versao); fflush(stdout);
  rede_resposta_limpar(&r);
  return http == 200;
}

static const char *rotaPrevia(const Sessao *s) { return s->contribuindo ? "contribute" : "previews"; }
static struct { long bufMs; int rebuf, valida; } contribTel;
void openseek_contrib_telemetria(long bufferMs, int rebuffering) {
  pthread_mutex_lock(&trava);
  contribTel.bufMs = bufferMs; contribTel.rebuf = rebuffering ? 1 : 0; contribTel.valida = 1;
  pthread_mutex_unlock(&trava);
}
// Saude da reproducao do CONTRIBUINTE para o guarda do servidor (uma vez a cada ~1,5 s). Nunca leva URL, titulo nem id.
static void contribTelemetria(Sessao *s, Uint32 *ultimo) {
  char url[OS_BASE_MAX + 96], tb[256];
  RedeResposta r;
  long buf; int rebuf, ok;
  Uint32 agora = SDL_GetTicks();
  if (*ultimo && agora - *ultimo < 1500u) return;
  pthread_mutex_lock(&trava);
  ok = s->contribuindo && contribTel.valida && s->id[0];
  buf = contribTel.bufMs; rebuf = contribTel.rebuf;
  if (ok) snprintf(url, sizeof url, "%s/v1/contribute/%s/telemetry", s->base, s->id);
  pthread_mutex_unlock(&trava);
  if (!ok) return;
  *ultimo = agora;
  if (!openseek_json_telemetria(tb, sizeof tb, buf, rebuf, "", 1)) return;
  chamar("POST", url, s->token, s->pino, 0, tb, OS_PRAZO_API_MS, &s->fim, &r);
  rede_resposta_limpar(&r);
}
static void consultarStatus(Sessao *s, int longo) {
  char url[OS_BASE_MAX + 96];
  OpenSeekPrevia p;
  RedeResposta r;
  int http, leu;
  pthread_mutex_lock(&trava);
  if (longo && !s->contribuindo) snprintf(url, sizeof url, "%s/v1/previews/%s?wait_version=%ld&wait_ms=500", s->base, s->id, s->versaoServidor);
  else snprintf(url, sizeof url, "%s/v1/%s/%s", s->base, rotaPrevia(s), s->id);
  pthread_mutex_unlock(&trava);
  http = chamar(NULL, url, s->token, s->pino, 0, NULL, OS_PRAZO_API_MS, &s->fim, &r);
  leu = http == 200 && r.corpo && openseek_ler_previa(r.corpo, s->base, &p);
  pthread_mutex_lock(&trava);
  notarUi(http, &r);
  if (s == atual) {
    if (leu) {
      s->jobEstado = p.estado;
      if (p.versao > s->versaoServidor) s->versaoServidor = p.versao;
      if (strcmp(p.vtt, s->vttUrl)) { snprintf(s->vttUrl, sizeof s->vttUrl, "%s", p.vtt); s->exp = p.exp; s->temVtt = 0; }
      if (p.estado == OS_FALHOU) definirEstado(s, SEEKR_SEM_PREVIA, OSM_FONTE);
    } else if (http == 404) {
      s->renovar = 1;                   // trabalho esquecido pelo servidor: recriar
    } else if (http == 401) {
      definirEstado(s, SEEKR_CHAVE_RECUSADA, OSM_RECUSADO);
      s->fim = 1;
    } else if (!vtt.nCues) {
      int mot = OSM_NENHUM, est = openseek_mapear(http, r.erro, r.curl_erro, NULL, 0, &mot);
      definirEstado(s, est, mot);
    }
  }
  pthread_mutex_unlock(&trava);
  rede_resposta_limpar(&r);
}

// The server only knows IMDb ids. A title opened as tmdb:/tvdb: (Nuvio's own rows) is asked by the IMDb id the
// app resolves (descoberta.c, cached; one TMDB request at most), here on the worker, never on the player thread.
// Not linked (unit tests) or not found: the id stays and the lookup says "not in the shared library" as before.
extern int desc_imdb_para(const char *id, int serie, char *tt, size_t n) NV_PERFIL_OPCIONAL;
static void imdbDaSessao(Sessao *s) {
  char id[64], tt[32];
  int serie;
  pthread_mutex_lock(&trava);
  idbase_copiar(s->idOrig, id, sizeof id);
  serie = s->t > 0 || s->e > 0;
  pthread_mutex_unlock(&trava);
  if (!id[0] || (id[0] == 't' && id[1] == 't') || !desc_imdb_para) return;
  if (!desc_imdb_para(id, serie, tt, sizeof tt)) {
    printf("[openseek] title id %.12s: no IMDb id found\n", id); fflush(stdout);
    return;
  }
  pthread_mutex_lock(&trava);
  snprintf(s->imdb, sizeof s->imdb, "%s", tt);
  pthread_mutex_unlock(&trava);
  printf("[openseek] title id %.12s -> %s\n", id, tt); fflush(stdout);
}

static void *controleFio(void *u) {
  Sessao *s = u;
  int falhas = 0;
  Uint32 ultimoStatus = 0;
  long ultimoVersaoPoll = -1;
  Uint32 ultTelContrib = 0;
  pthread_mutex_lock(&trava);
  while (!s->fim && !s->iniciar) esperar(s, 1000);
  pthread_mutex_unlock(&trava);
  imdbDaSessao(s);
  while (!s->fim) {
    // Renovar em laco (relogio do servidor torto, 403 seguidos) custa no
    // maximo um POST a cada 5 s.
    pthread_mutex_lock(&trava);
    while (!s->fim && s->ultimoCriar && SDL_GetTicks() - s->ultimoCriar < 5000u) esperar(s, 1000);
    s->ultimoCriar = SDL_GetTicks() | 1u;
    pthread_mutex_unlock(&trava);
    if (s->fim) break;
    int r = s->convidado ? procurarCompart(s) : criarTrabalho(s);
    if (s->convidado && r == 0 && !s->contribuindo) { int c = contribuirCriar(s); if (c != 0) r = c; }
    if (r < 0) break;
    if (r == 0) {
      unsigned espera = 30000;
      pthread_mutex_lock(&trava);
      long long falta = retryUtc - (long long)time(NULL);
      if (falta > 0 && falta < 600) espera = (unsigned)falta * 1000u;
      if (++falhas > 1 && espera < 30000) espera = 30000;
      s->renovar = 0;                   // pTentar() pode encurtar o recuo
      esperarPrazo(s, espera);
      pthread_mutex_unlock(&trava);
      continue;
    }
    falhas = 0;
    // Laco do trabalho: VTT novo quando a versao sobe; status ate complete.
    while (!s->fim) {
      int precisaVtt, completo, rolando, renovar;
      long long agora = (long long)time(NULL);
      pthread_mutex_lock(&trava);
      precisaVtt = !s->temVtt || s->versaoServidor > s->versaoAplicada;
      completo = s->jobEstado == OS_COMPLETO || s->jobEstado == OS_FALHOU;
      rolando = s->scrubbing;
      renovar = s->renovar || (s->exp && s->exp - agora < 120);
      pthread_mutex_unlock(&trava);
      if (renovar) break;               // volta ao POST: capacidades novas
      if (!completo) contribTelemetria(s, &ultTelContrib);
      if (precisaVtt) {
        if (!baixarVtt(s)) {
          pthread_mutex_lock(&trava); esperarPrazo(s, 2000); pthread_mutex_unlock(&trava);
        }
        continue;
      }
      if (completo) {
        // Nada a consultar; acorda para renovar a capacidade antes do exp.
        pthread_mutex_lock(&trava);
        while (!s->fim && !s->renovar && s->temVtt && !(s->exp && s->exp - (long long)time(NULL) < 120)) esperar(s, 5000);
        pthread_mutex_unlock(&trava);
        continue;
      }
      if (rolando) {
        // Long-poll curto; quando uma versao nova acabou de ser aplicada, re-entra apos 120 ms.
        Uint32 desde = SDL_GetTicks() - ultimoStatus;
        Uint32 minEspera;
        pthread_mutex_lock(&trava);
        minEspera = (s->versaoAplicada > ultimoVersaoPoll) ? 120u : 500u;
        pthread_mutex_unlock(&trava);
        if (ultimoStatus && desde < minEspera) {
          pthread_mutex_lock(&trava); esperar(s, minEspera - desde); pthread_mutex_unlock(&trava);
          continue;
        }
        pthread_mutex_lock(&trava); ultimoVersaoPoll = s->versaoAplicada; pthread_mutex_unlock(&trava);
        ultimoStatus = SDL_GetTicks();
        consultarStatus(s, 1);
      }
      else {
        pthread_mutex_lock(&trava);
        Uint32 intervalo = s->contribuindo ? 1500u : (vtt.nCues < 180) ? 1500u : (vtt.nCues < 360) ? 3000u : 10000u;   // contribuinte: a telemetria nao pode ficar velha
        Uint32 ate = SDL_GetTicks() + intervalo;
        while (!s->fim && !s->scrubbing && !s->renovar && (Sint32)(ate - SDL_GetTicks()) > 0 &&
               s->versaoServidor <= s->versaoAplicada) esperar(s, 500);
        int pular = s->versaoServidor > s->versaoAplicada || s->renovar;
        pthread_mutex_unlock(&trava);
        if (!pular && !s->fim) consultarStatus(s, 0);
      }
    }
  }
  pthread_mutex_lock(&trava); soltarSessao(s); pthread_mutex_unlock(&trava);
  return NULL;
}

static void *prioridadeFio(void *u) {
  Sessao *s = u;
  char url[OS_BASE_MAX + 64], corpo[96];
  pthread_mutex_lock(&trava);
  while (!s->fim) {
    long at, play;
    RedeResposta r;
    int http;
    Uint32 desde = SDL_GetTicks() - s->prioTick;
    if (!s->prioSujo || !s->id[0]) { esperar(s, 1000); continue; }
    if (desde < OS_PRIO_MIN_MS) { esperar(s, OS_PRIO_MIN_MS - desde); continue; }
    at = s->scrubbing ? s->atMs : -1; play = s->playheadMs;
    s->prioSujo = 0; s->prioTick = SDL_GetTicks();
    snprintf(url, sizeof url, "%s/v1/previews/%s/priority", s->base, s->id);
    pthread_mutex_unlock(&trava);
    openseek_json_prioridade(corpo, sizeof corpo, at, play);
    http = chamar("POST", url, s->token, s->pino, 0, corpo, OS_PRAZO_API_MS, &s->fim, &r);
    pthread_mutex_lock(&trava);
    if (http == 200 && r.corpo && s == atual) {
      long v; int est;
      if (openseek_ler_versao(r.corpo, &v, &est)) {
        if (v > s->versaoServidor) { s->versaoServidor = v; pthread_cond_broadcast(&sinal); }
        if (est) s->jobEstado = est;
      }
    } else if (http == 404 && s == atual) { s->renovar = 1; pthread_cond_broadcast(&sinal); }
    pthread_mutex_unlock(&trava);
    rede_resposta_limpar(&r);
    pthread_mutex_lock(&trava);
  }
  soltarSessao(s);
  pthread_mutex_unlock(&trava);
  return NULL;
}

// --- busca e recorte ----------------------------------------------------------------
static SDL_Surface *decodificar(const unsigned char *b, size_t n) {
  SDL_Surface *s, *c;
  int w0 = 0, h0 = 0;
  s = jpeg_rapido_carregar_mem(b, n, 1 << 20, &w0, &h0);
#ifndef __EMSCRIPTEN__
  if (!s) { SDL_RWops *rw = SDL_RWFromConstMem(b, (int)n); s = rw ? IMG_Load_RW(rw, 1) : NULL; }
#endif
  if (!s) return NULL;
  if (s->format->format == SDL_PIXELFORMAT_ABGR8888) return s;
  c = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(s);
  return c;
}
static unsigned char *recorte(SDL_Surface *s, int x, int y, int w, int h) {
  unsigned char *px;
  if (!s || x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > s->w || y + h > s->h) return NULL;
  px = malloc((size_t)w * (size_t)h * 4u);
  if (!px) return NULL;
  if (SDL_MUSTLOCK(s)) SDL_LockSurface(s);
  for (int k = 0; k < h; k++)
    memcpy(px + (size_t)k * (size_t)w * 4u,
           (const unsigned char *)s->pixels + (size_t)(y + k) * (size_t)s->pitch + (size_t)x * 4u,
           (size_t)w * 4u);
  if (SDL_MUSTLOCK(s)) SDL_UnlockSurface(s);
  return px;
}

typedef struct { Sessao *s; const char *chaveJpg; } PararPrefetch;
static int pararPrefetchFn(void *u) {
  const PararPrefetch *pp = u;
  int parar = 0;
  if (!pp || !pp->s || pp->s->fim) return 1;
  if (!pp->s->nQuero) return 0;
  pthread_mutex_lock(&trava);
  if (pp->s->fim) parar = 1;
  else if (pp->s->nQuero > 0 && (!pp->chaveJpg || strcmp(pp->s->quero[0].chaveJpg, pp->chaveJpg) != 0))
    parar = 1;
  pthread_mutex_unlock(&trava);
  return parar;
}

static int chamarPrefetch(const char *url, const char *pino, Sessao *s,
                          const char *chaveJpg, RedeResposta *r) {
  OpenSeekPedido o;
  PararPrefetch pp = { s, chaveJpg };
  memset(r, 0, sizeof *r);
  if (!openseek_pedido(&o, NULL, url, NULL, pino, 0, NULL, 0, OS_PRAZO_DADOS_MS)) {
    r->erro = REDE_ENTRADA; return 0;
  }
  o.p.parar = pararPrefetchFn;
  o.p.parar_usuario = &pp;
  rede_pedir(&o.p, r);
  return r->status;
}

static int guardarJpg(OsLru *jl, char **jpg, size_t *jpgN, size_t *totalBytes,
                      int *nItens, const char *chaveJpg, char *corpo, size_t nCorpo,
                      const char (*folhaChaves)[240]) {
  const char *prot[OS_FOLHAS_DEC] = { folhaChaves[0], folhaChaves[1] };
  int slot = openseek_lru_lugar(jl, chaveJpg, prot, OS_FOLHAS_DEC);
  if (slot < 0) return -1;
  if (jpg[slot]) { *totalBytes -= jpgN[slot]; free(jpg[slot]); }
  else (*nItens)++;
  jpg[slot] = corpo; jpgN[slot] = nCorpo;
  *totalBytes += nCorpo;
  while (*totalBytes > OS_JPG_BYTES_MAX) {
    int velho = -1;
    for (int i = 0; i < OS_JPG_LRU; i++) {
      if (i == slot || !jl->it[i].ocupado) continue;
      if ((folhaChaves[0][0] && !strcmp(jl->it[i].chave, folhaChaves[0])) ||
          (folhaChaves[1][0] && !strcmp(jl->it[i].chave, folhaChaves[1]))) continue;
      if (velho < 0 || jl->it[i].uso < jl->it[velho].uso) velho = i;
    }
    if (velho < 0) break;
    *totalBytes -= jpgN[velho]; free(jpg[velho]); jpg[velho] = NULL; jpgN[velho] = 0;
    jl->it[velho].ocupado = 0; jl->it[velho].chave[0] = 0;
    (*nItens)--;
  }
  return slot;
}

static void registrarFalha(char (*falhou)[240], Uint32 *falhouTick, int *pos,
                           const char *chaveJpg, Uint32 agora) {
  if (!chaveJpg || !*chaveJpg) return;
  for (int i = 0; i < OS_FALHAS_MAX; i++) {
    if (falhou[i][0] && !strcmp(falhou[i], chaveJpg)) { falhouTick[i] = agora; return; }
  }
  snprintf(falhou[*pos], sizeof falhou[*pos], "%s", chaveJpg);
  falhouTick[*pos] = agora;
  *pos = (*pos + 1) % OS_FALHAS_MAX;
}

static void *buscaFio(void *u) {
  Sessao *s = u;
  OsLru *jl = calloc(1, sizeof *jl);
  OsLru *prewarmLru = calloc(1, sizeof *prewarmLru);
  char **jpg = calloc(OS_JPG_LRU, sizeof *jpg);
  size_t *jpgN = calloc(OS_JPG_LRU, sizeof *jpgN);
  size_t jpgTotalBytes = 0;
  int jpgItens = 0;
  SDL_Surface *folhas[OS_FOLHAS_DEC] = { 0 };
  char folhaChaves[OS_FOLHAS_DEC][240] = { { 0 } };
  unsigned folhaUso[OS_FOLHAS_DEC] = { 0 }, folhaRelogio = 0;
  Uint32 ultimoTrabalho = 0;
  char falhou[OS_FALHAS_MAX][240] = { { 0 } };
  Uint32 falhouTick[OS_FALHAS_MAX] = { 0 };
  int falhouPos = 0;
  unsigned cntSheetHits = 0, cntJpgHits = 0, cntNetFetches = 0;
  unsigned cntPrefetches = 0, cntPrefetchAborts = 0;

  if (!jl || !prewarmLru || !jpg || !jpgN) {
    free(jl); free(prewarmLru); free(jpg); free(jpgN);
    pthread_mutex_lock(&trava); soltarSessao(s); pthread_mutex_unlock(&trava);
    return NULL;
  }
  openseek_lru_ini(jl, OS_JPG_LRU);
  openseek_lru_ini(prewarmLru, 48);

  pthread_mutex_lock(&trava);
  while (!s->fim) {
    Quero q;
    int achou = -1, nFalhas = 0;
    char falhasAtivas[OS_FALHAS_MAX][240];
    unsigned g = s->g;
    Uint32 agora = SDL_GetTicks();

    if (s->limparFolhas) {
      SDL_Surface *f0 = folhas[0], *f1 = folhas[1];
      folhas[0] = folhas[1] = NULL;
      folhaChaves[0][0] = folhaChaves[1][0] = 0;
      s->limparFolhas = 0;
      if (f0 || f1) {
        pthread_mutex_unlock(&trava);
        if (f0) SDL_FreeSurface(f0);
        if (f1) SDL_FreeSurface(f1);
        pthread_mutex_lock(&trava);
        continue;
      }
    }

    for (int i = 0; i < OS_FALHAS_MAX; i++) {
      if (falhou[i][0] && agora - falhouTick[i] < OS_FALHA_MS) {
        memcpy(falhasAtivas[nFalhas++], falhou[i], sizeof falhasAtivas[0]);
      } else falhou[i][0] = 0;
    }

    for (int i = 0; i < s->nQuero; i++) {
      int emFalha = 0;
      for (int j = 0; j < nFalhas; j++)
        if (!strcmp(s->quero[i].chaveJpg, falhasAtivas[j])) { emFalha = 1; break; }
      if (!emFalha) { achou = i; break; }
    }

    if (achou < 0) {
      int temPrewarm = 0;
      // 1. Pre-recorte silencioso em torno do playhead/alvo quando o JPEG ja esta em cache.
      if (s->iniciar && s == atual && vtt.nCues > 0 && vttChaves && vttFolhaChaves) {
        int vagas = 0;
        for (int k = 0; k < OS_PRONTOS; k++) if (!prontos[k].px) vagas++;
        if (vagas > 2) {
          long anchor = (s->scrubbing && s->atMs >= 0) ? s->atMs : s->playheadMs;
          int cands[16];
          int nc = openseek_fila_busca(&vtt, anchor, s->scrubbing ? s->atMs : -1,
                                       s->scrubDir ? s->scrubDir : 1, 15, cands);
          for (int k = 0; k < nc; k++) {
            int c = cands[k], f = vtt.cues[c].folha;
            if (f < 0 || f >= vtt.nFolhas || !vttFolhaChaves[f][0] || !vttChaves[c][0]) continue;
            if (openseek_lru_tem(jl, vttFolhaChaves[f]) < 0) continue;
            if (openseek_lru_tem(prewarmLru, vttChaves[c]) >= 0) continue;
            int jaPronto = 0;
            for (int j = 0; j < OS_PRONTOS; j++)
              if (prontos[j].px && !strcmp(prontos[j].chaveTex, vttChaves[c])) { jaPronto = 1; break; }
            if (jaPronto) continue;
            memset(&q, 0, sizeof q);
            snprintf(q.fetch, sizeof q.fetch, "%s", vtt.folhas[f]);
            snprintf(q.chaveJpg, sizeof q.chaveJpg, "%s", vttFolhaChaves[f]);
            snprintf(q.chaveTex, sizeof q.chaveTex, "%s", vttChaves[c]);
            q.x = vtt.cues[c].x; q.y = vtt.cues[c].y; q.w = vtt.cues[c].w; q.h = vtt.cues[c].h;
            q.prewarm = 1;
            openseek_lru_lugar(prewarmLru, q.chaveTex, NULL, 0);
            temPrewarm = 1;
            break;
          }
        }
      }

      // 2. Prefetch das folhas/quadros ja publicados no VTT (interrompido na hora se nQuero > 0).
      if (!temPrewarm && s->iniciar && s == atual && vtt.nCues > 0 && vtt.nFolhas > 0 &&
          jpgItens < OS_JPG_LRU && jpgTotalBytes < (OS_JPG_BYTES_MAX * 15u) / 16u) {
        long anchor = (s->scrubbing && s->atMs >= 0) ? s->atMs : s->playheadMs;
        int f = openseek_escolher_prefetch(&vtt, vttFolhaChaves, jl, falhasAtivas, nFalhas,
                                           anchor, s->scrubDir ? s->scrubDir : 1);
        if (f >= 0 && openseek_na_origem(vtt.folhas[f], s->base)) {
          char pfUrl[2400], pfKey[240];
          int rolando = s->scrubbing;
          snprintf(pfUrl, sizeof pfUrl, "%s", vtt.folhas[f]);
          if (vttFolhaChaves && vttFolhaChaves[f][0])
            snprintf(pfKey, sizeof pfKey, "%s", vttFolhaChaves[f]);
          else openseek_chave(pfUrl, pfKey, sizeof pfKey);
          if (!rolando) {
            esperar(s, OS_IDLE_PREFETCH_MS);
            if (s->fim || s->nQuero > 0) { zerar(pfUrl, sizeof pfUrl); continue; }
          }
          pthread_mutex_unlock(&trava);

          RedeResposta r;
          Uint32 t0 = SDL_GetTicks();
          int w = 0, h = 0;
          int http = chamarPrefetch(pfUrl, s->pino, s, pfKey, &r);
          Uint32 fetchMs = SDL_GetTicks() - t0;
          size_t nBytes = r.n_corpo;
          int ok = 0;
          if (http == 200 && r.corpo && r.n_corpo > 0 && r.n_corpo <= OS_TILE_BYTES_MAX &&
              openseek_jpeg_dimensoes((unsigned char *)r.corpo, r.n_corpo, &w, &h) &&
              w > 0 && h > 0 && w <= OS_TILE_LADO_MAX && h <= OS_TILE_ALTO_MAX) {
            if (guardarJpg(jl, jpg, jpgN, &jpgTotalBytes, &jpgItens, pfKey, r.corpo, r.n_corpo, folhaChaves) >= 0) {
              r.corpo = NULL;
              ok = 1;
              cntPrefetches++;
            }
          }
          int cancelado = (r.erro == REDE_CANCELADO || r.erro == REDE_GERACAO);
          if (cancelado) cntPrefetchAborts++;
          rede_resposta_limpar(&r);
          zerar(pfUrl, sizeof pfUrl);

          pthread_mutex_lock(&trava);
          if (http == 403 && s == atual) { s->renovar = 1; pthread_cond_broadcast(&sinal); }
          if (!ok && !cancelado && !s->fim)
            registrarFalha(falhou, falhouTick, &falhouPos, pfKey, SDL_GetTicks());
          if (ok || (!cancelado && http != 200)) {
            printf("[openseek] prefetch ok=%d http=%d fetch_ms=%u bytes=%zu jpg_items=%d jpg_kb=%zu total_prefetch=%u aborted=%u\n",
                   ok, http, fetchMs, nBytes, jpgItens, jpgTotalBytes / 1024u, cntPrefetches, cntPrefetchAborts);
            fflush(stdout);
          }
          continue;
        }
      }

      if (!temPrewarm) {
        // Fim do avanco: libera as folhas decodificadas apos periodo ocioso.
        unsigned limiteOcioso = s->scrubbing ? 10000u : 2000u;
        if ((folhas[0] || folhas[1]) && SDL_GetTicks() - ultimoTrabalho > limiteOcioso) {
          SDL_Surface *f0 = folhas[0], *f1 = folhas[1];
          folhas[0] = folhas[1] = NULL;
          folhaChaves[0][0] = folhaChaves[1][0] = 0;
          pthread_mutex_unlock(&trava);
          if (f0) SDL_FreeSurface(f0);
          if (f1) SDL_FreeSurface(f1);
          pthread_mutex_lock(&trava);
          continue;
        }
        esperar(s, 500);
        continue;
      }
    } else {
      q = s->quero[achou];
      memmove(&s->quero[achou], &s->quero[achou + 1], (size_t)(s->nQuero - achou - 1) * sizeof q);
      s->nQuero--;
      q.prewarm = 0;
      openseek_lru_lugar(prewarmLru, q.chaveTex, NULL, 0);
    }

    snprintf(s->emVoo, sizeof s->emVoo, "%s", q.chaveTex);
    pthread_mutex_unlock(&trava);

    unsigned char *px = NULL;
    const char *fonte = "network";
    int slot = openseek_lru_achar(jl, q.chaveJpg), http = 200, w = 0, h = 0;
    Uint32 t0 = SDL_GetTicks(), fetchMs = 0, decodeMs = 0, cropMs = 0;
    if (slot >= 0) {
      fonte = "jpg_cache";
      cntJpgHits++;
    } else if (!openseek_na_origem(q.fetch, s->base)) {
      http = -2;
    } else {
      RedeResposta r;
      cntNetFetches++;
      http = chamar(NULL, q.fetch, NULL, s->pino, 0, NULL, OS_PRAZO_DADOS_MS, &s->fim, &r);
      fetchMs = SDL_GetTicks() - t0;
      // Bytes e dimensoes conferidos ANTES de decodificar (NUVIO-REVIEW 4).
      if (http == 200 && r.corpo && r.n_corpo > 0 && r.n_corpo <= OS_TILE_BYTES_MAX &&
          openseek_jpeg_dimensoes((unsigned char *)r.corpo, r.n_corpo, &w, &h) &&
          w > 0 && h > 0 && w <= OS_TILE_LADO_MAX && h <= OS_TILE_ALTO_MAX) {
        slot = guardarJpg(jl, jpg, jpgN, &jpgTotalBytes, &jpgItens, q.chaveJpg, r.corpo, r.n_corpo, folhaChaves);
        if (slot >= 0) r.corpo = NULL;
      } else if (http == 200) http = -1;   // grande demais ou nao e JPEG
      rede_resposta_limpar(&r);
    }

    if (slot >= 0 && jpg[slot]) {
      SDL_Surface *sup = NULL;
      int supAvulsa = 0;
      for (int m = 0; m < OS_FOLHAS_DEC; m++) {
        if (folhas[m] && !strcmp(folhaChaves[m], q.chaveJpg)) {
          sup = folhas[m];
          folhaUso[m] = ++folhaRelogio;
          fonte = "sheet_ram";
          cntSheetHits++;
          break;
        }
      }
      if (!sup) {
        Uint32 td = SDL_GetTicks();
        sup = decodificar((unsigned char *)jpg[slot], jpgN[slot]);
        decodeMs = SDL_GetTicks() - td;
        if (sup) {
          // Folha multi-quadro fica num dos 2 slots; quadro avulso (320x180) nao desaloja folha.
          if (sup->w > q.w || sup->h > q.h) {
            int m = !folhas[0] ? 0 : !folhas[1] ? 1 : (folhaUso[0] <= folhaUso[1] ? 0 : 1);
            if (folhas[m]) SDL_FreeSurface(folhas[m]);
            folhas[m] = sup;
            snprintf(folhaChaves[m], sizeof folhaChaves[m], "%s", q.chaveJpg);
            folhaUso[m] = ++folhaRelogio;
          } else {
            supAvulsa = 1;
          }
        }
      }
      if (sup) {
        Uint32 tc = SDL_GetTicks();
        px = recorte(sup, q.x, q.y, q.w, q.h);
        cropMs = SDL_GetTicks() - tc;
        if (supAvulsa) SDL_FreeSurface(sup);
      }
    }

    Uint32 totalMs = SDL_GetTicks() - t0;
    if (!q.prewarm || !px) {
      printf("[openseek] tile http=%d ok=%d source=%s fetch_ms=%u decode_ms=%u crop_ms=%u latency_ms=%u jpg_items=%d jpg_kb=%zu sheet_hits=%u jpg_hits=%u net_fetches=%u prefetch=%u\n",
             http, px != NULL, fonte, fetchMs, decodeMs, cropMs, totalMs,
             jpgItens, jpgTotalBytes / 1024u, cntSheetHits, cntJpgHits, cntNetFetches, cntPrefetches);
      fflush(stdout);
    }
    zerar(q.fetch, sizeof q.fetch);
    ultimoTrabalho = SDL_GetTicks();

    pthread_mutex_lock(&trava);
    s->emVoo[0] = 0;
    if (!px && !s->fim)
      registrarFalha(falhou, falhouTick, &falhouPos, q.chaveJpg, SDL_GetTicks());
    if (http == 403 && s == atual) { s->renovar = 1; pthread_cond_broadcast(&sinal); }
    if (px && s == atual && g == geracao) {
      int k, livre = -1;
      for (k = 0; k < OS_PRONTOS; k++)
        if (prontos[k].px && !strcmp(prontos[k].chaveTex, q.chaveTex)) { livre = k; break; }
      for (k = 0; livre < 0 && k < OS_PRONTOS; k++)
        if (!prontos[k].px) { livre = k; break; }
      if (livre < 0 && !q.prewarm) {
        free(prontos[0].px);
        memmove(&prontos[0], &prontos[1], (OS_PRONTOS - 1) * sizeof prontos[0]);
        memset(&prontos[OS_PRONTOS - 1], 0, sizeof prontos[0]);
        livre = OS_PRONTOS - 1;
      }
      if (livre >= 0) {
        free(prontos[livre].px);
        prontos[livre].px = px; prontos[livre].w = q.w; prontos[livre].h = q.h; prontos[livre].g = g;
        snprintf(prontos[livre].chaveTex, sizeof prontos[livre].chaveTex, "%s", q.chaveTex);
        px = NULL;
      }
    }
    free(px);
  }
  soltarSessao(s);
  pthread_mutex_unlock(&trava);
  for (int m = 0; m < OS_FOLHAS_DEC; m++) if (folhas[m]) SDL_FreeSurface(folhas[m]);
  for (int i = 0; i < OS_JPG_LRU; i++) free(jpg[i]);
  free(jpg); free(jpgN); free(jl); free(prewarmLru);
  return NULL;
}

// --- BIBLIOTECA COMPARTILHADA: informacao e ajustes (so deste aparelho) ------------------------
int openseek_compart_info(OpenSeekCompartInfo *o) {
  OpenSeekCompart c;
  int v;
  if (!o) return 0;
  memset(o, 0, sizeof *o);
  pthread_mutex_lock(&trava);
  v = compart.valido && cfgPapel == OS_PAPEL_CONVIDADO;
  if (v) {
    memset(&c, 0, sizeof c);
    snprintf(c.id, sizeof c.id, "%s", compart.id); snprintf(c.tipo, sizeof c.tipo, "%s", compart.tipo);
    c.aproximado = compart.aprox; c.verificado = compart.verif;
    o->valido = 1; o->aproximado = compart.aprox; o->verificado = compart.verif;
    snprintf(o->tipo, sizeof o->tipo, "%s", compart.tipo);
    o->offsetMs = (long)(compart.b < 0 ? compart.b - 0.5 : compart.b + 0.5);
    snprintf(o->rotulo, sizeof o->rotulo, "%s", openseek_rotulo_compart(&c));
  }
  pthread_mutex_unlock(&trava);
  return v;
}
int openseek_compart_ultimo(char *msg, size_t n) {
  int r;
  pthread_mutex_lock(&trava);
  r = compart.ultimo;
  if (msg && n) snprintf(msg, n, "%s", compart.ultimoMsg);
  compart.ultimo = 0;
  pthread_mutex_unlock(&trava);
  return r;
}
typedef struct { int op; long delta; char base[OS_BASE_MAX], pino[OS_PINO_MAX], token[OS_TOKEN_MAX], id[33]; double b; } CompartOp;
static int compartOcupado;
static void *compartFio(void *u) {
  CompartOp *o = u;
  char url[OS_BASE_MAX + 96], corpo[64];
  RedeResposta r;
  int http = 0, ok = 0;
  long nb = 0;
  snprintf(url, sizeof url, "%s/v1/previews/%s/%s", o->base, o->id, o->op == 3 ? "reject" : "offset");
  if (o->op == 1) {
    nb = openseek_offset_apos(o->b, o->delta);
    if (!openseek_json_offset(corpo, sizeof corpo, nb)) goto fim;
    http = chamar("PUT", url, o->token, o->pino, 0, corpo, OS_PRAZO_API_MS, NULL, &r);
  } else if (o->op == 2) http = chamar("DELETE", url, o->token, o->pino, 0, NULL, OS_PRAZO_API_MS, NULL, &r);
  else http = chamar("POST", url, o->token, o->pino, 0, "{}", OS_PRAZO_API_MS, NULL, &r);
  ok = http == 200 || http == 204;
  rede_resposta_limpar(&r);
fim:
  pthread_mutex_lock(&trava);
  if (ok && !strcmp(compart.id, o->id)) {
    if (o->op == 1) { compart.b = (double)nb; snprintf(compart.tipo, sizeof compart.tipo, "user_adjusted"); }
    if (o->op == 3) compart.valido = 0;
    if (atual && atual->convidado && !strcmp(atual->id, o->id)) {       // a barra passa a usar o VTT novo
      atual->temVtt = 0;
      if (o->op != 1) atual->renovar = 1;                              // zerar/rejeitar: procurar de novo (mapeamento e fonte)
      pthread_cond_broadcast(&sinal);
    }
  }
  compart.ultimo = ok ? 1 : 2;
  snprintf(compart.ultimoMsg, sizeof compart.ultimoMsg, "%s",
           ok ? (o->op == 1 ? "Miniaturas ajustadas" : o->op == 2 ? "Ajuste desfeito" : "Correspondência rejeitada")
              : (http == 401 ? "Acesso revogado ou expirado" : http == 403 ? "Compartilhamento desligado no servidor" : "Não foi possível ajustar"));
  compartOcupado = 0;
  pthread_mutex_unlock(&trava);
  printf("[openseek] shared op=%d http=%d\n", o->op, http); fflush(stdout);
  zerar(o, sizeof *o); free(o);
  return NULL;
}
static int compartIniciar(int op, long delta) {
  CompartOp *o;
  pthread_t fio;
  openseek_carregar();
  o = calloc(1, sizeof *o);
  if (!o) return 0;
  pthread_mutex_lock(&trava);
  if (cfgPapel != OS_PAPEL_CONVIDADO || !compart.valido || compartOcupado || !cfgToken[0]) {
    pthread_mutex_unlock(&trava); free(o); return 0;
  }
  o->op = op; o->delta = delta; o->b = compart.b;
  snprintf(o->base, sizeof o->base, "%s", cfgBase); snprintf(o->pino, sizeof o->pino, "%s", cfgPino);
  snprintf(o->token, sizeof o->token, "%s", cfgToken); snprintf(o->id, sizeof o->id, "%s", compart.id);
  compartOcupado = 1;
  pthread_mutex_unlock(&trava);
  if (pthread_create(&fio, NULL, compartFio, o) == 0) { pthread_detach(fio); return 1; }
  pthread_mutex_lock(&trava); compartOcupado = 0; pthread_mutex_unlock(&trava);
  zerar(o, sizeof *o); free(o);
  return 0;
}
int openseek_compart_ajustar(long deslocaMs) { return compartIniciar(1, deslocaMs); }
int openseek_compart_zerar(void) { return compartIniciar(2, 0); }
int openseek_compart_rejeitar(void) { return compartIniciar(3, 0); }

// --- provedor (seekr.h) -------------------------------------------------------------
static int pTemChave(void) { return openseek_pareado(); }
static int pEstado(void) { int r; pthread_mutex_lock(&trava); r = estado; pthread_mutex_unlock(&trava); return r; }
static const char *pRotulo(int est) {
  int m;
  pthread_mutex_lock(&trava); m = motivo; pthread_mutex_unlock(&trava);
  switch (est) {
    case SEEKR_BUSCANDO: return m == OSM_CONTRIBUINDO ? "Título novo: o servidor está gerando as miniaturas (você contribui)" : "Preparando miniaturas no OpenSeek";
    case SEEKR_PRONTO:
      if (openseek_papel() == OS_PAPEL_CONVIDADO) {
        static _Thread_local OpenSeekCompartInfo ci;
        if (openseek_compart_info(&ci) && ci.rotulo[0]) return ci.rotulo;
      }
      return "Miniaturas disponíveis";
    case SEEKR_SEM_PREVIA:
      return m == OSM_FONTE ? "O OpenSeek não conseguiu ler este vídeo" :
             m == OSM_NAO_COMPARTILHADO ? "Este título não está na biblioteca compartilhada" :
             m == OSM_COMPARTILHAMENTO_OFF ? "O compartilhamento está desligado no servidor" :
             m == OSM_OCUPADO_CONTRIB ? "O servidor está ocupado com outra contribuição; tente de novo em instantes" :
             "Este título não tem miniaturas";
    case SEEKR_CHAVE_RECUSADA: return "O OpenSeek recusou este aparelho; pareie de novo";
    case SEEKR_LIMITE_PROVEDOR: return "Servidor OpenSeek ocupado";
    case SEEKR_REDE_INDISPONIVEL:
      return m == OSM_PINO_MUDOU ? "O certificado do servidor mudou" :
             m == OSM_SEM_PINO ? "Este aparelho não suporta conexão segura com o OpenSeek" :
             "Servidor OpenSeek sem conexão";
    default: return openseek_pareado() ? "Miniaturas não solicitadas" : "OpenSeek não pareado";
  }
}
static void pUso(SeekrUso *u) {
  memset(u, 0, sizeof *u);
  pthread_mutex_lock(&trava);
  u->retryUtc = retryUtc; u->http = ultimoHttp; u->latenciaMs = ultimaLatencia;
  u->persistente = 1; u->chavePropria = 1;   // sem cota local: o servidor e da pessoa
  pthread_mutex_unlock(&trava);
}
static void pTentar(void) {
  pthread_mutex_lock(&trava);
  retryUtc = 0;
  if (atual && estado != SEEKR_PRONTO && estado != SEEKR_BUSCANDO) { atual->renovar = 1; pthread_cond_broadcast(&sinal); }
  pthread_mutex_unlock(&trava);
}
static void pDesligar(void) { sessaoEncerrar(); }

static char *dup(const char *s) {
  size_t n = s ? strlen(s) : 0;
  char *d = malloc(n + 1);
  if (d) { if (n) memcpy(d, s, n); d[n] = 0; }
  return d;
}

static void pPedir(const SeekrFonte *f) {
  Sessao *s;
  int mesma, novo = 0;
  if (!f || !f->url || strncmp(f->url, "http", 4) || f->durMs <= 0) return;
  if (!openseek_pareado()) return;
  int conv = openseek_papel() == OS_PAPEL_CONVIDADO;
  pthread_mutex_lock(&trava);
  s = atual;
  mesma = s && (conv ? (s->convidado && !strcmp(s->idOrig, f->imdb ? f->imdb : "") && s->t == f->t && s->e == f->e)
                     : !strcmp(s->url, f->url))
          && labs(s->durMs - f->durMs) < 2000;
  if (s && !mesma) { pthread_mutex_unlock(&trava); sessaoEncerrar(); pthread_mutex_lock(&trava); s = NULL; }
  if (!s) {
    s = calloc(1, sizeof *s);
    if (!s || !(s->url = dup(conv ? "" : f->url)) || !(s->cabs = dup(conv && 1 ? "" : (f->cabecalhos ? f->cabecalhos : "")))) {
      if (s) { free(s->url); free(s); }
      pthread_mutex_unlock(&trava); return;
    }
    snprintf(s->base, sizeof s->base, "%s", cfgBase);
    snprintf(s->pino, sizeof s->pino, "%s", cfgPino);
    snprintf(s->token, sizeof s->token, "%s", cfgToken);
    // The player's id can carry the episode ("tt0098844:1:5", "tmdb:2734:1:5"); season and episode travel on their
    // own, and the server's ids are the title's only. Taken whole, the guest's lookup refused every episode before
    // asking: "not in the shared library" for all series.
    idbase_copiar(f->imdb ? f->imdb : "", s->imdb, sizeof s->imdb);
    snprintf(s->idOrig, sizeof s->idOrig, "%s", f->imdb ? f->imdb : "");
    snprintf(s->hash, sizeof s->hash, "%s", f->videoHash ? f->videoHash : "");
    snprintf(s->arquivo, sizeof s->arquivo, "%s", f->arquivo ? f->arquivo : "");
    s->durMs = f->durMs; s->t = f->t; s->e = f->e; s->tamanho = f->tamanho; s->convidado = conv;
    s->atMs = -1; s->ultimoQuadrosMs = -1; s->scrubDir = 1;
    s->ultimoPlay = f->playheadMs; s->g = ++geracao;
    s->refs = 1;   // a de `atual`
    atual = s; novo = 1;
    soltarVtt(); soltarProntos();
    estado = SEEKR_BUSCANDO; motivo = OSM_NENHUM; retryUtc = 0;
  }
  // SCRUBBING §1: so depois de 8 s tocando ou no primeiro avanco.
  if (!f->scrubbing) {
    long d = f->playheadMs - s->ultimoPlay;
    if (d > 0 && d < 2000) s->jogadoMs += d;
  }
  s->ultimoPlay = f->playheadMs;
  s->playheadMs = f->playheadMs;
  if (s->convidado && s->querUrl && s->url && !s->url[0] && f->url && !strncmp(f->url, "https://", 8)) {   // so a contribuinte autorizado, so depois da procura
    char *u = dup(f->url), *c = dup(f->cabecalhos ? f->cabecalhos : "");
    if (u && c) { free(s->url); free(s->cabs); s->url = u; s->cabs = c; } else { free(u); free(c); }
  }
  if (!s->iniciar && (f->scrubbing || s->jogadoMs >= OS_INICIO_MS)) { s->iniciar = 1; pthread_cond_broadcast(&sinal); }
  // Prioridade: o alvo do avanco (ultimo vence) e o fim do avanco (null).
  if (f->scrubbing && (!s->scrubbing || labs(f->posMs - s->atMs) >= 1)) {
    if (s->scrubbing && s->atMs >= 0 && f->posMs != s->atMs)
      s->scrubDir = f->posMs > s->atMs ? 1 : -1;
    else if (!s->scrubbing && f->posMs != f->playheadMs)
      s->scrubDir = f->posMs >= f->playheadMs ? 1 : -1;
    s->atMs = f->posMs; s->prioSujo = 1;
    if (!s->scrubbing) pthread_cond_broadcast(&sinal);   // o controle passa ao long-poll
  } else if (!f->scrubbing && s->scrubbing) { s->atMs = -1; s->prioSujo = 1; pthread_cond_broadcast(&sinal); }
  else if (!f->scrubbing && SDL_GetTicks() - s->prioPlayTick > 60000u) {
    s->prioPlayTick = SDL_GetTicks();     // janela P1 em volta do playhead
    if (s->id[0]) s->prioSujo = 1;
  }
  s->scrubbing = f->scrubbing;
  if (s->prioSujo) pthread_cond_broadcast(&sinal);
  if (novo) {
    pthread_t a, b, c, d;
    int comAlt;
    altNovaSessao();
    comAlt = alt.ligado && !conv;                      // convidado: nada de prioridade, telemetria nem fonte alternativa
    s->refs += conv ? 2 : comAlt ? 4 : 3; s->prioPlayTick = SDL_GetTicks();
    pthread_mutex_unlock(&trava);
    if (pthread_create(&a, NULL, controleFio, s) == 0) pthread_detach(a);
    else { pthread_mutex_lock(&trava); soltarSessao(s); pthread_mutex_unlock(&trava); }
    if (!conv) {
      if (pthread_create(&b, NULL, prioridadeFio, s) == 0) pthread_detach(b);
      else { pthread_mutex_lock(&trava); soltarSessao(s); pthread_mutex_unlock(&trava); }
    }
    if (pthread_create(&c, NULL, buscaFio, s) == 0) pthread_detach(c);
    else { pthread_mutex_lock(&trava); soltarSessao(s); pthread_mutex_unlock(&trava); }
    if (comAlt) {
      if (pthread_create(&d, NULL, altFio, s) == 0) pthread_detach(d);
      else { pthread_mutex_lock(&trava); soltarSessao(s); pthread_mutex_unlock(&trava); }
    }
    printf("[openseek] session start%s\n", conv ? " (shared library)" : ""); fflush(stdout);
    return;
  }
  pthread_mutex_unlock(&trava);
}

// Desenho: LRU de texturas por chave (so este fio mexe).
static OsLru texLru;
static GLuint texs[OS_LRU_MAX];
static unsigned texG;
static int dispCueIdx = -1, dispCueLogado = 0, quadroEsparso = 0;
static Uint32 dispPedidoTick = 0;
static unsigned cntTexHits = 0, cntPrewarmHits = 0, cntAsyncHits = 0, cntDispMisses = 0;

static long cuePtsMs(const SeekrCue *q) {
  return q->rotulo >= 0 ? q->rotulo : q->ini;
}

static int pQuadros(double posSeg, int n, GLuint *out, double *cueSeg) {
  Pronto pr[OS_UPLOAD_MAX];
  int np = 0, i, k, nFila = 0, fila[OS_QUERO], achou = 0, subiuAtual = 0, esparso = 0;
  long posMs = (long)(posSeg * 1000.0);
  char chaveAtual[256] = "";
  unsigned g;
  if (n < 1) return 0;
  if (n > 3) n = 3;
  for (k = 0; k < n; k++) { out[k] = 0; if (cueSeg) cueSeg[k] = -1.0; }
  pthread_mutex_lock(&trava);
  quadroEsparso = 0;
  g = geracao;
  if (texG != g) {
    openseek_lru_ini(&texLru, OS_TEX_LRU);
    texG = g;
    dispCueIdx = -1; dispCueLogado = 0;
    cntTexHits = cntPrewarmHits = cntAsyncHits = cntDispMisses = 0;
  }
  if (estado != SEEKR_PRONTO || !atual || !vtt.nCues || !vttChaves) {
    soltarProntos();
    pthread_mutex_unlock(&trava);
    return 0;
  }
  if (atual->ultimoQuadrosMs >= 0 && labs(posMs - atual->ultimoQuadrosMs) >= 10)
    atual->scrubDir = posMs > atual->ultimoQuadrosMs ? 1 : -1;
  atual->ultimoQuadrosMs = posMs;

  // Cue estrita [ini, fim) (esparso=0) ou cue esparsa da grade 60s/30s dentro
  // de OS_SPARSE_TOL_MS (esparso=1); fora disso devolve -1 (sem imagem velha).
  i = openseek_cue_visivel(&vtt, posMs, &esparso);
  if (i >= 0 && i < vtt.nCues)
    snprintf(chaveAtual, sizeof chaveAtual, "%s", vttChaves[i]);
  nFila = openseek_fila_busca(&vtt, posMs, atual->scrubbing ? atual->atMs : -1,
                              atual->scrubDir ? atual->scrubDir : 1, OS_QUERO, fila);

  // Drena ate OS_UPLOAD_MAX recortes prontos por quadro, priorizando os visiveis.
  for (int passo = 0; passo < 2 && np < OS_UPLOAD_MAX; passo++) {
    for (k = 0; k < OS_PRONTOS && np < OS_UPLOAD_MAX; k++) {
      if (!prontos[k].px) continue;
      if (prontos[k].g != g) { free(prontos[k].px); memset(&prontos[k], 0, sizeof prontos[k]); continue; }
      int visivel = (i >= 0 && (!strcmp(prontos[k].chaveTex, vttChaves[i]) ||
                    (i + 1 < vtt.nCues && !strcmp(prontos[k].chaveTex, vttChaves[i + 1])) ||
                    (i > 0 && !strcmp(prontos[k].chaveTex, vttChaves[i - 1]))));
      if ((passo == 0 && !visivel) || (passo == 1 && visivel)) continue;
      pr[np++] = prontos[k];
      memset(&prontos[k], 0, sizeof prontos[k]);
    }
  }
  pthread_mutex_unlock(&trava);

  // Sobe o que ficou pronto (fio de desenho: aqui pode GL).
  for (k = 0; k < np; k++) {
    if (!pr[k].px) continue;
    if (pr[k].g == g && openseek_lru_achar(&texLru, pr[k].chaveTex) < 0) {
      const char *prot[6]; int nProt = 0, sl;
      pthread_mutex_lock(&trava);
      for (int j = 0; j < nFila && j < 6 && vttChaves; j++) prot[nProt++] = vttChaves[fila[j]];
      sl = openseek_lru_lugar(&texLru, pr[k].chaveTex, prot, nProt);
      pthread_mutex_unlock(&trava);
      if (sl >= 0) {
        if (!texs[sl]) glGenTextures(1, &texs[sl]);
        glBindTexture(GL_TEXTURE_2D, texs[sl]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pr[k].w, pr[k].h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pr[k].px);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gfx_tex_esquecer(0);
        if (chaveAtual[0] && !strcmp(pr[k].chaveTex, chaveAtual)) subiuAtual = 1;
      }
    }
    free(pr[k].px);
  }

  pthread_mutex_lock(&trava);
  if (g != geracao || !vttChaves || !atual) { pthread_mutex_unlock(&trava); return 0; }
  if (i >= 0 && i < vtt.nCues) {
    int vis[3], nVis = 0;
    long ptsMs = cuePtsMs(&vtt.cues[i]);
    vis[nVis++] = i;
    if (i + 1 < vtt.nCues && labs(cuePtsMs(&vtt.cues[i + 1]) - ptsMs) <= 70000L) vis[nVis++] = i + 1;
    if (i > 0 && labs(ptsMs - cuePtsMs(&vtt.cues[i - 1])) <= 70000L) vis[nVis++] = i - 1;
    for (k = 0; k < nVis; k++) {
      int c = vis[k], sl = vttChaves[c][0] ? openseek_lru_achar(&texLru, vttChaves[c]) : -1;
      int pos = n == 1 ? (c == i ? 0 : -1) : (c == i ? 1 : c < i ? 0 : 2);
      if (sl >= 0 && pos >= 0) {
        out[pos] = texs[sl];
        if (cueSeg) cueSeg[pos] = cuePtsMs(&vtt.cues[c]) / 1000.0;
        if (c == i) { achou = 1; quadroEsparso = esparso; }
      }
    }
    if (i != dispCueIdx) {
      dispCueIdx = i;
      dispPedidoTick = SDL_GetTicks();
      if (achou) {
        dispCueLogado = 1;
        if (subiuAtual) cntPrewarmHits++; else cntTexHits++;
        printf("[openseek] display cue=%d pts_ms=%ld sparse=%d source=%s display_latency_ms=0 tex_hits=%u prewarm_hits=%u async_hits=%u misses=%u\n",
               i, ptsMs, esparso, subiuAtual ? "prewarmed_crop" : "tex_cache",
               cntTexHits, cntPrewarmHits, cntAsyncHits, cntDispMisses);
        fflush(stdout);
      } else {
        dispCueLogado = 0;
        cntDispMisses++;
      }
    } else if (achou && !dispCueLogado) {
      Uint32 lat = SDL_GetTicks() - dispPedidoTick;
      dispCueLogado = 1;
      cntAsyncHits++;
      printf("[openseek] display cue=%d pts_ms=%ld sparse=%d source=async_load display_latency_ms=%u tex_hits=%u prewarm_hits=%u async_hits=%u misses=%u\n",
             i, ptsMs, esparso, lat, cntTexHits, cntPrewarmHits, cntAsyncHits, cntDispMisses);
      fflush(stdout);
    }
  }
  {
    Sessao *s = atual;
    Quero novos[OS_QUERO];
    int nn = 0;
    for (k = 0; k < nFila && nn < OS_QUERO; k++) {
      int c = fila[k];
      if (c < 0 || c >= vtt.nCues || !vttChaves[c][0]) continue;
      if (openseek_lru_achar(&texLru, vttChaves[c]) >= 0) continue;
      if (!strcmp(s->emVoo, vttChaves[c])) continue;
      int pronto = 0;
      for (int j = 0; j < OS_PRONTOS; j++)
        if (prontos[j].px && !strcmp(prontos[j].chaveTex, vttChaves[c])) { pronto = 1; break; }
      if (pronto) continue;
      const SeekrCue *q = &vtt.cues[c];
      Quero *w = &novos[nn++];
      memset(w, 0, sizeof *w);
      snprintf(w->fetch, sizeof w->fetch, "%s", vtt.folhas[q->folha]);
      if (vttFolhaChaves && q->folha >= 0 && q->folha < vtt.nFolhas && vttFolhaChaves[q->folha][0])
        snprintf(w->chaveJpg, sizeof w->chaveJpg, "%s", vttFolhaChaves[q->folha]);
      else openseek_chave(vtt.folhas[q->folha], w->chaveJpg, sizeof w->chaveJpg);
      snprintf(w->chaveTex, sizeof w->chaveTex, "%s", vttChaves[c]);
      w->x = q->x; w->y = q->y; w->w = q->w; w->h = q->h;
    }
    // O ultimo pedido vence: a fila e trocada inteira (latest wins).
    int igual = s && nn == s->nQuero;
    for (k = 0; igual && k < nn; k++) igual = !strcmp(novos[k].chaveTex, s->quero[k].chaveTex);
    if (s && !igual) {
      memcpy(s->quero, novos, (size_t)nn * sizeof *novos);
      s->nQuero = nn;
      if (nn) pthread_cond_broadcast(&sinal);
    }
    for (k = 0; k < nn; k++) zerar(novos[k].fetch, sizeof novos[k].fetch);
  }
  pthread_mutex_unlock(&trava);
  return achou;
}
static int pVelho(void) {
  int r;
  pthread_mutex_lock(&trava); r = quadroEsparso; pthread_mutex_unlock(&trava);
  return r;
}
static void pOcioso(void) {
  pthread_mutex_lock(&trava);
  if (atual) { atual->limparFolhas = 1; pthread_cond_broadcast(&sinal); }
  pthread_mutex_unlock(&trava);
}

static const SeekrProvedor PROVEDOR = {
  pTemChave, pEstado, pRotulo, pUso, pTentar, pPedir, pDesligar, pQuadros, pVelho, pOcioso
};
const SeekrProvedor *openseek_provedor(void) { return &PROVEDOR; }
