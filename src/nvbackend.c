#include "nvbackend.h"
#include "dados.h"
#include "js.h"
#include "rede.h"
#include <pthread.h>
#include <stdatomic.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

/* Extrai host de http(s) URL; 1 se há host utilizável. */
static int host_valido(const char *u) {
  const char *h;
  size_t n;
  if (!u || !u[0]) return 0;
  if (!strncmp(u, "https://", 8)) h = u + 8;
  else if (!strncmp(u, "http://", 7)) h = u + 7;
  else return 0;
  n = strcspn(h, "/:?#");
  if (n == 0) return 0;
  if (h[0] == '[') return 1; /* IPv6 literal */
  for (size_t i = 0; i < n; i++)
    if (h[i] != '.' && (h[i] < '0' || h[i] > '9') && h[i] != '-' && h[i] != ':')
      return 1;
  return 1; /* IP numérico ou localhost */
}

static int ip_lan(const char *host, size_t len) {
  unsigned a, b, c, d;
  char buf[64];
  if (len >= sizeof buf) return 0;
  memcpy(buf, host, len);
  buf[len] = 0;
  if (sscanf(buf, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return 0;
  if (a == 10) return 1;
  if (a == 172 && b >= 16 && b <= 31) return 1;
  if (a == 192 && b == 168) return 1;
  if (a == 127) return 1;
  return 0;
}

static int normalizar(char *u, size_t n, const char *in) {
  const char *host;
  size_t hn;
  size_t i = 0;
  if (!in || !in[0] || n < 12) return 0;
  while (in[i] == ' ' || in[i] == '\t') i++;
  snprintf(u, n, "%s", in + i);
  u[strcspn(u, " \t\n\r")] = 0;
  if (strncmp(u, "http://", 7) && strncmp(u, "https://", 8)) return 0;
  while (u[0] && u[strlen(u) - 1] == '/') u[strlen(u) - 1] = 0;
  if (!host_valido(u)) return 0;
  if (!strncmp(u, "https://", 8)) host = u + 8;
  else host = u + 7;
  hn = strcspn(host, "/:?#");
  if (!strncmp(u, "http://", 7) && !ip_lan(host, hn)) return 0;
  return 1;
}

// Estado proprio: o getter nao pode devolver ponteiro para um buffer global
// mutavel enquanto o teclado e os workers leem ao mesmo tempo. A trava vale
// para {url,urlLida,descGeracao}; quem precisa do texto copia para o proprio
// buffer. I/O de disco NUNCA fica sob `trava`: o getter e chamado a cada
// quadro pela linha de Ajustes e a cada pedido em rede.c, e nao pode esperar
// uma gravacao. Quem serializa a persistencia e travaGravar (abaixo).
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static char url[400];
static int urlLida;
// Geracao da descoberta: nvbackend_definir_url e cada iniciar incrementam;
// o worker so adota a resposta se a dele ainda for a atual.
static unsigned descGeracao;
// Serializa conferencia de geracao -> persistencia -> adocao, nos caminhos da
// descoberta e do usuario: uma gravacao de descoberta nao pode chegar ao disco
// depois de uma gravacao do usuario iniciada mais tarde (achado sol-f31r2 —
// a conferencia soltava a trava antes de dados_gravar e o worker atrasado
// regravava a URL que o usuario tinha acabado de limpar/trocar; ela voltava
// no arranque). Quem chega depois espera a gravacao em curso e grava por
// ultimo. Todo bump de descGeracao acontece sob travaGravar, entao entre a
// conferencia e a adocao a geracao nao pode ficar velha.
static pthread_mutex_t travaGravar = PTHREAD_MUTEX_INITIALIZER;

static void carregar_travado(void) {
  char *t = dados_ler("nuvio-backend.txt");
  char nova[sizeof url] = "";
  if (t) {
    // Mesma validacao da gravacao: um arquivo editado por fora nao pode
    // reintroduzir um endereco que normalizar() recusaria.
    if (!normalizar(nova, sizeof nova, t)) nova[0] = 0;
    free(t);
  }
  snprintf(url, sizeof url, "%s", nova);
}

size_t nv_rec_url_copiar(char *dst, size_t tam) {
  const char *fonte;
  size_t n;
  if (!dst || !tam) return 0;
  pthread_mutex_lock(&trava);
  if (!urlLida) { carregar_travado(); urlLida = 1; }
  fonte = url[0] ? url : NV_REC_URL;
  n = strlen(fonte);
  snprintf(dst, tam, "%s", fonte);
  pthread_mutex_unlock(&trava);
  return n < tam ? n : tam - 1;
}

int nv_rec_url_ativo(void) {
  char tmp[8];
  nv_rec_url_copiar(tmp, sizeof tmp);
  return tmp[0] != 0;
}

// Gravacao com geracao: so adota `u` se `gen` ainda for a geracao de
// descoberta atual (UINT_MAX = caminho do usuario, sempre vale).
// travaGravar fica tomada da conferencia ate o fim da adocao — o I/O acontece
// sob ela, nao sob `trava`, entao o getter nunca espera o disco. Devolve 1
// adotou, 0 recusou (URL invalida ou falha no disco), -1 a geracao ja estava
// velha na chegada (o usuario mexeu no campo durante a descoberta).
static int definir_geracao(const char *u, unsigned gen) {
  char nova[sizeof url] = "";
  if (u && u[0] && !normalizar(nova, sizeof nova, u)) return 0;
  pthread_mutex_lock(&travaGravar);
  pthread_mutex_lock(&trava);
  if (gen != UINT_MAX && gen != descGeracao) {
    pthread_mutex_unlock(&trava);
    pthread_mutex_unlock(&travaGravar);
    return -1;
  }
  pthread_mutex_unlock(&trava);
  // Memoria e disco tem de concordar: so adota a nova URL se a gravacao
  // persistiu; caso contrario quem reinicia veria outro valor.
  if (nova[0]) {
    if (!dados_gravar("nuvio-backend.txt", nova)) { pthread_mutex_unlock(&travaGravar); return 0; }
  } else {
    char *velho = dados_ler("nuvio-backend.txt");
    free(velho);
    // So exige sucesso quando havia algo gravado: apagar o inexistente ja e
    // o estado desejado (dados_apagar devolve 0 no remove() falho).
    if (velho && !dados_apagar("nuvio-backend.txt")) { pthread_mutex_unlock(&travaGravar); return 0; }
  }
  pthread_mutex_lock(&trava);
  snprintf(url, sizeof url, "%s", nova);
  urlLida = 1;
  // Toda gravacao invalida a descoberta em curso: a resposta atrasada nao
  // pode ressuscitar a URL que o usuario acabou de apagar/trocar.
  descGeracao++;
  pthread_mutex_unlock(&trava);
  pthread_mutex_unlock(&travaGravar);
  return 1;
}

int nvbackend_definir_url(const char *u) {
  int r = definir_geracao(u, UINT_MAX);
  return r == 1; /* -1 = geracao velha: no caminho do usuario nao acontece */
}

// Descoberta /.well-known/nuvio: a rede fica num fio proprio para o teclado
// nunca esperar. Um fio VIVO por vez; ajustes_atualizar() recolhe o resultado.
// Iniciar de novo com um fio ainda na rede DESTACA o antigo (detach + geracao
// nova): o pedido recente e atendido sem travar o teclado na resposta velha,
// e o fio orfao termina sem publicar nada.
static pthread_t descFio;
static int descFioVivo;
static _Atomic int descEstado;      // 0 livre, 1 descobrindo, 2 ok, -1 falha
static _Atomic unsigned descAtiva;  // geracao cujo estado vale em descEstado

typedef struct { unsigned gen; char origin[280]; } DescPedido;

static void *descFioMain(void *u) {
  char base[280], pedido[320], *corpo, found[400];
  int st = 0, ok = 0;
  unsigned gen = ((DescPedido *)u)->gen;
  // O pedido viaja no arg: um iniciar posterior nao pode trocar a origem de
  // um fio ja destacado.
  snprintf(base, sizeof base, "%s", ((DescPedido *)u)->origin);
  free(u);
  snprintf(pedido, sizeof pedido, "%s/.well-known/nuvio", base);
  corpo = rede_baixar_st(pedido, 12, NULL, &st);
  found[0] = 0;
  if (corpo && st >= 200 && st < 300) {
    if (!js_texto_raiz(corpo, "backend_url", found, sizeof found) || !found[0])
      js_texto_raiz(corpo, "url", found, sizeof found);
  }
  free(corpo);
  if (found[0]) {
    // A geracao e conferida SOB A TRAVA antes de adotar e antes de persistir:
    // se o usuario salvou/limpou o campo depois do pedido, a resposta vai
    // para o estado -1 sem tocar na URL.
    ok = definir_geracao(found, gen) == 1;
  }
  // Um fio destacado (gen != ativa) nao publica estado: o dono dele morreu.
  if (gen == atomic_load_explicit(&descAtiva, memory_order_acquire))
    atomic_store_explicit(&descEstado, ok ? 2 : -1, memory_order_release);
  return NULL;
}

int nvbackend_descobrir_iniciar(const char *origin) {
  char base[280];
  unsigned gen;
  DescPedido *pd;
  if (!origin || !origin[0] || !normalizar(base, sizeof base, origin)) return 0;
  // Origem nova com worker ativo: o pedido ANTIGO e abandonado (detach), nao
  // mantido — o teclado nao espera o GET em curso e a resposta velha cai na
  // geracao errada.
  if (descFioVivo) { pthread_detach(descFio); descFioVivo = 0; }
  pd = malloc(sizeof *pd);
  if (!pd) return 0;
  // O bump tambem vai sob travaGravar: uma descoberta nova nao pode invalidar
  // a geracao de um worker que esta no meio da propria gravacao.
  pthread_mutex_lock(&travaGravar);
  pthread_mutex_lock(&trava);
  gen = ++descGeracao;
  pthread_mutex_unlock(&trava);
  pthread_mutex_unlock(&travaGravar);
  pd->gen = gen;
  snprintf(pd->origin, sizeof pd->origin, "%s", base);
  atomic_store_explicit(&descAtiva, gen, memory_order_release);
  atomic_store_explicit(&descEstado, 1, memory_order_release);
  if (pthread_create(&descFio, NULL, descFioMain, pd) != 0) {
    free(pd);
    atomic_store_explicit(&descEstado, -1, memory_order_release);
    return 0;
  }
  descFioVivo = 1;
  return 1;
}

// 0 nunca pediu, 1 descobrindo, 2 encontrou e gravou, -1 falhou.
int nvbackend_descobrir_estado(void) {
  int e = atomic_load_explicit(&descEstado, memory_order_acquire);
  if (descFioVivo && e != 1) { pthread_join(descFio, NULL); descFioVivo = 0; }
  return e;
}

void nvbackend_descobrir_limpar(void) {
  if (descFioVivo) { pthread_join(descFio, NULL); descFioVivo = 0; }
  atomic_store_explicit(&descEstado, 0, memory_order_release);
}

