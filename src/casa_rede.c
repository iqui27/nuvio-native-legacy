// Nuvio Casa, rede. Ver casa_rede.h.
#include "casa_rede.h"
#include "casa.h"
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
void casa_configurar(int usar, int ajudar, const CasaMotor *motor) { (void)usar; (void)ajudar; (void)motor; }
int  casa_ajudante_base(char *out, unsigned n) { if (out && n) out[0] = 0; return 0; }
int  casa_procurar(int ms) { (void)ms; return 0; }
int  casa_ajudando(void) { return 0; }
int  casa_porta(void) { return 0; }
void casa_parar(void) {}
void casa_teste(int portaUdp, const char *sonda) { (void)portaUdp; (void)sonda; }
#else

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0   // Mac: SO_NOSIGPIPE no socket
#endif

#define CASA_MAX 8
#define CASA_CAB_MAX 8192
#define CASA_CORPO_MAX 8192
#define CASA_LEITURA_S 8   // prazo total para chegar o pedido inteiro

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  cond = PTHREAD_COND_INITIALIZER;
static int portaUdp = CASA_PORTA_UDP;
static char sondaExtra[32];

static long agoraS(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long)t.tv_sec;
}
static long long agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

void casa_teste(int p, const char *sonda) {
  pthread_mutex_lock(&trava);
  if (p > 0) portaUdp = p;
  snprintf(sondaExtra, sizeof sondaExtra, "%s", sonda ? sonda : "");
  pthread_mutex_unlock(&trava);
}

// ================================================================= cliente
static CasaAjudante achados[CASA_MAX];
static int nAchados;
static pthread_t fioCliente;
static int clienteVivo, clienteSair;

static void guardar(const CasaAjudante *a) {
  int i, velho = 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < nAchados; i++)
    if (achados[i].ip == a->ip || !strcmp(achados[i].token, a->token)) break;   // o mesmo aparelho por duas placas
  if (i == nAchados) {
    if (nAchados < CASA_MAX) nAchados++;
    else {
      for (i = 1; i < CASA_MAX; i++) if (achados[i].visto < achados[velho].visto) velho = i;
      i = velho;
    }
    printf("[casa] ajudante encontrado na rede (%d conhecido(s))\n", nAchados);
  }
  // Mesmo aparelho, mesma rodada: fica o primeiro endereco que respondeu.
  if (achados[i].visto != a->visto || strcmp(achados[i].token, a->token)) achados[i] = *a;
  pthread_mutex_unlock(&trava);
}

static void perguntar(int s, in_addr_t ip, int porta) {
  struct sockaddr_in a;
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_addr.s_addr = ip; a.sin_port = htons((uint16_t)porta);
  if (sendto(s, CASA_PERGUNTA, sizeof CASA_PERGUNTA - 1, MSG_NOSIGNAL, (struct sockaddr *)&a, sizeof a) < 0) { /* rede sem broadcast: segue */ }
}

int casa_procurar(int ms) {
  struct ifaddrs *lista = NULL, *i;
  char extra[32];
  int s = socket(AF_INET, SOCK_DGRAM, 0), um = 1, porta, n;
  long long fim = agoraMs() + (ms > 0 ? ms : 1);
  if (s >= 0) {
    fcntl(s, F_SETFD, FD_CLOEXEC);
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &um, sizeof um);
    pthread_mutex_lock(&trava);
    porta = portaUdp; snprintf(extra, sizeof extra, "%s", sondaExtra);
    pthread_mutex_unlock(&trava);
    perguntar(s, htonl(INADDR_BROADCAST), porta);
    if (!getifaddrs(&lista)) {
      for (i = lista; i; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || !(i->ifa_flags & IFF_BROADCAST) ||
            !(i->ifa_flags & IFF_UP) || !i->ifa_broadaddr) continue;
        perguntar(s, ((struct sockaddr_in *)i->ifa_broadaddr)->sin_addr.s_addr, porta);
      }
      freeifaddrs(lista);
    }
    if (extra[0]) perguntar(s, inet_addr(extra), porta);
    for (;;) {
      struct pollfd p = { s, POLLIN, 0 };
      struct sockaddr_in de;
      socklen_t dl = sizeof de;
      char b[300];
      CasaAjudante a;
      long long resta = fim - agoraMs();
      ssize_t r;
      if (resta <= 0 || poll(&p, 1, (int)resta) <= 0) break;
      r = recvfrom(s, b, sizeof b - 1, 0, (struct sockaddr *)&de, &dl);
      if (r <= 0) continue;
      memset(&a, 0, sizeof a);
      if (!casa_ler_resposta(b, (int)r, &a)) continue;
      a.ip = ntohl(de.sin_addr.s_addr);
      if (!casa_ip_privado(a.ip)) continue;
      a.visto = agoraS();
      guardar(&a);
    }
    close(s);
  }
  pthread_mutex_lock(&trava);
  n = 0;
  for (int k = 0; k < nAchados; k++)
    if (agoraS() - achados[k].visto < CASA_VALIDADE_S) n++;
  pthread_mutex_unlock(&trava);
  return n;
}

int casa_ajudante_base(char *out, unsigned n) {
  int i, ok = 0;
  if (out && n) out[0] = 0;
  pthread_mutex_lock(&trava);
  i = casa_escolher(achados, nAchados, agoraS());
  if (i >= 0) ok = casa_base(&achados[i], out, n);
  pthread_mutex_unlock(&trava);
  return ok;
}

static void *cliente(void *u) {
  (void)u;
  for (;;) {
    struct timespec ate;
    // Sem ninguem ainda: pergunta mais vezes (a rede acabou de subir).
    int espera = casa_procurar(1200) ? CASA_SONDA_S : 5;
    pthread_mutex_lock(&trava);
    if (!clienteSair) {
      clock_gettime(CLOCK_REALTIME, &ate);
      ate.tv_sec += espera;
      pthread_cond_timedwait(&cond, &trava, &ate);
    }
    if (clienteSair) { pthread_mutex_unlock(&trava); break; }
    pthread_mutex_unlock(&trava);
  }
  return NULL;
}

static void clienteLigar(int ligar) {
  if (ligar && !clienteVivo) {
    clienteSair = 0;
    if (!pthread_create(&fioCliente, NULL, cliente, NULL)) { clienteVivo = 1; puts("[casa] procurando ajudante na rede local"); }
  } else if (!ligar && clienteVivo) {
    pthread_mutex_lock(&trava);
    clienteSair = 1;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&trava);
    pthread_join(fioCliente, NULL);
    clienteVivo = 0;
    pthread_mutex_lock(&trava);
    nAchados = 0;
    pthread_mutex_unlock(&trava);
    puts("[casa] busca de ajudante desligada");
  }
}

// ================================================================ ajudante
static const CasaMotor *motor;
static pthread_t fioAjudante;
static int ajudanteVivo, sockUdp = -1, sockHttp = -1, acorda[2] = { -1, -1 };
static int portaHttp, conexoes;
static char token[33], nomeAparelho[48];
// Sessao: o torrent que o motor esta servindo para a casa.
static pthread_mutex_t travaCriar = PTHREAD_MUTEX_INITIALIZER;
static char sesHash[41], sesUrl[512];
static uint64_t sesTam;
static long sesUltimo;
static int sesAtiva, repasses;

int casa_ajudando(void) { return ajudanteVivo; }
int casa_porta(void) { return ajudanteVivo ? portaHttp : 0; }

static int enviar(int c, const char *b, size_t n) {
  while (n) {
    ssize_t w = send(c, b, n, MSG_NOSIGNAL);
    if (w <= 0) { if (w < 0 && errno == EINTR) continue; return 0; }
    b += w; n -= (size_t)w;
  }
  return 1;
}

static void responder(int c, int st, const char *motivo, const char *corpo) {
  char cab[256];
  int n = snprintf(cab, sizeof cab,
                   "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %u\r\n"
                   "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
                   st, motivo, (unsigned)strlen(corpo));
  if (enviar(c, cab, (size_t)n)) enviar(c, corpo, strlen(corpo));
}

// Valor do cabecalho `nome` (sem os dois pontos) em `cab`; 1 se achou.
static int cabecalho(const char *cab, const char *nome, char *out, unsigned n) {
  size_t L = strlen(nome);
  for (const char *p = strstr(cab, "\r\n"); p; p = strstr(p, "\r\n")) {
    unsigned k = 0;
    p += 2;
    if (strncasecmp(p, nome, L) || p[L] != ':') continue;
    p += L + 1;
    while (*p == ' ') p++;
    while (*p && *p != '\r' && k + 1 < n) out[k++] = *p++;
    out[k] = 0;
    return 1;
  }
  return 0;
}

static void prazos(int s, int seg) {
  struct timeval tv = { seg, 0 };
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
#ifdef SO_NOSIGPIPE
  { int um = 1; setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &um, sizeof um); }
#endif
}

// O video: o mesmo pedido (metodo e Range) vai ao servidor do motor em
// 127.0.0.1 e a resposta volta como veio. Nenhum outro destino e possivel.
static void repassar(int c, const char *metodo, const char *range, const char *url) {
  const char *caminho = NULL;
  int porta = casa_url_do_motor(url, &caminho), m, n;
  struct sockaddr_in a;
  char ped[900], buf[32768];
  ssize_t r;
  if (!porta) { responder(c, 502, "Bad Gateway", "{\"error\":\"motor\"}"); return; }
  m = socket(AF_INET, SOCK_STREAM, 0);
  if (m < 0) { responder(c, 502, "Bad Gateway", "{\"error\":\"motor\"}"); return; }
  fcntl(m, F_SETFD, FD_CLOEXEC);
  prazos(m, 30);
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = htons((uint16_t)porta);
  n = snprintf(ped, sizeof ped, "%s %s HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n%s%s%sConnection: close\r\n\r\n",
               metodo, caminho, porta, range[0] ? "Range: " : "", range, range[0] ? "\r\n" : "");
  if (n <= 0 || n >= (int)sizeof ped || connect(m, (struct sockaddr *)&a, sizeof a) || !enviar(m, ped, (size_t)n)) {
    close(m);
    responder(c, 502, "Bad Gateway", "{\"error\":\"motor\"}");
    return;
  }
  while ((r = recv(m, buf, sizeof buf, 0)) > 0) {
    if (!enviar(c, buf, (size_t)r)) break;
    pthread_mutex_lock(&trava);
    sesUltimo = agoraS();
    pthread_mutex_unlock(&trava);
  }
  close(m);
}

static void criar(int c, const CasaPedido *p, const char *corpo) {
  char fontes[4096], url[512] = "", json[160];
  uint64_t tam = 0;
  int e, mesma, tinha, tocando;
  casa_fontes_do_corpo(corpo, fontes, sizeof fontes);
  pthread_mutex_lock(&travaCriar);
  pthread_mutex_lock(&trava);
  tinha = sesAtiva; tocando = repasses > 0;
  mesma = sesAtiva && !strcmp(sesHash, p->hash);
  if (mesma) { tam = sesTam; sesUltimo = agoraS(); }
  pthread_mutex_unlock(&trava);
  if (!mesma) {
    // O dono do aparelho vem primeiro: motor em uso por ele, a casa espera. E
    // uma TV assistindo nao perde o video porque outra pediu.
    if ((!tinha && motor->ocupado && motor->ocupado()) || tocando) {
      pthread_mutex_unlock(&travaCriar);
      puts("[casa] pedido recusado: motor em uso");
      responder(c, 503, "Busy", "{\"error\":\"ocupado\"}");
      return;
    }
    if (motor->segurar) motor->segurar(1);
    printf("[casa] pedido %.8s: resolvendo no motor\n", p->hash);
    e = motor->resolver(p->hash, p->idx, fontes, p->temporada, p->episodio, url, sizeof url, &tam);
    if (e != 0 || !casa_url_do_motor(url, NULL)) {
      pthread_mutex_lock(&trava);
      sesAtiva = 0; sesHash[0] = sesUrl[0] = 0;
      pthread_mutex_unlock(&trava);
      if (motor->segurar) motor->segurar(0);
      pthread_mutex_unlock(&travaCriar);
      printf("[casa] pedido %.8s: motor devolveu erro %d\n", p->hash, e);
      // 504 = sem peers/metadados (p2p.c traduz); 503 = ocupado; o resto, 500.
      snprintf(json, sizeof json, "{\"error\":%d}", e);
      responder(c, e == 5 ? 504 : e == 13 ? 503 : 500, "Error", json);
      return;
    }
    pthread_mutex_lock(&trava);
    snprintf(sesHash, sizeof sesHash, "%s", p->hash);
    snprintf(sesUrl, sizeof sesUrl, "%s", url);
    sesTam = tam; sesAtiva = 1; sesUltimo = agoraS();
    pthread_mutex_unlock(&trava);
    printf("[casa] pedido %.8s: pronto\n", p->hash);
  }
  pthread_mutex_unlock(&travaCriar);
  if (casa_json_arquivos(tam, json, sizeof json)) responder(c, 200, "OK", json);
  else responder(c, 500, "Error", "{}");
}

// Recv so ate `ate` (ms monotonicos): o prazo do socket vira o que ainda resta.
static int prazoAte(int c, long long ate) {
  long long resta = ate - agoraMs();
  struct timeval tv;
  if (resta <= 0) return 0;
  tv.tv_sec = (time_t)(resta / 1000); tv.tv_usec = (suseconds_t)((resta % 1000) * 1000);
  setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  return 1;
}

static void *conexao(void *u) {
  int c = (int)(intptr_t)u, n = 0, corpoTem = 0, cl = 0;
  char *cab = malloc(CASA_CAB_MAX + CASA_CORPO_MAX + 1), *fim = NULL;
  char metodo[8] = "", alvo[256] = "", v[128], range[96] = "";
  CasaPedido p;
  // Prazo TOTAL para ler o pedido (CASA_LEITURA_S), nao so por recv: um vizinho
  // que pinga um byte a cada 29 s seguraria uma das CASA_CONEXOES por horas.
  long long ate = agoraMs() + CASA_LEITURA_S * 1000LL;
  if (!cab) goto sai;
  while (n < CASA_CAB_MAX) {
    ssize_t r;
    if (!prazoAte(c, ate)) goto sai;
    r = recv(c, cab + n, (size_t)(CASA_CAB_MAX - n), 0);
    if (r <= 0) goto sai;
    n += (int)r; cab[n] = 0;
    if ((fim = strstr(cab, "\r\n\r\n"))) break;
  }
  if (!fim || sscanf(cab, "%7s %255s", metodo, alvo) != 2) goto sai;
  fim[2] = 0;                                   // cabecalhos terminam aqui
  corpoTem = n - (int)(fim + 4 - cab);
  if (cabecalho(cab, "Origin", v, sizeof v) || !cabecalho(cab, "Host", v, sizeof v) ||
      !casa_host_ok(v, portaHttp) || !casa_rota(metodo, alvo, token, &p)) {
    responder(c, 404, "Not Found", "{}");
    goto sai;
  }
  if (p.rota == CASA_ROTA_SETTINGS) {
    responder(c, 200, "OK", "{\"values\":{\"serverVersion\":\"" CASA_VERSAO "\"}}");
  } else if (p.rota == CASA_ROTA_CRIAR) {
    char *corpo = fim + 4;
    if (cabecalho(cab, "Content-Length", v, sizeof v)) cl = atoi(v);
    if (cl < 0 || cl > CASA_CORPO_MAX) { responder(c, 413, "Too Large", "{}"); goto sai; }
    while (corpoTem < cl) {
      ssize_t r;
      if (!prazoAte(c, ate)) goto sai;
      r = recv(c, corpo + corpoTem, (size_t)(cl - corpoTem), 0);
      if (r <= 0) goto sai;
      corpoTem += (int)r;
    }
    corpo[cl] = 0;
    prazos(c, 30);   // pedido lido: o create espera o motor com o prazo de sempre
    criar(c, &p, corpo);
  } else {
    char url[512] = "";
    pthread_mutex_lock(&trava);
    if (sesAtiva && !strcmp(sesHash, p.hash)) { snprintf(url, sizeof url, "%s", sesUrl); repasses++; sesUltimo = agoraS(); }
    pthread_mutex_unlock(&trava);
    if (!url[0]) { responder(c, 404, "Not Found", "{}"); goto sai; }
    prazos(c, 30);   // o repasse do video usa o prazo de sempre
    if (cabecalho(cab, "Range", v, sizeof v) && strlen(v) < sizeof range && !strncmp(v, "bytes=", 6) &&
        strspn(v + 6, "0123456789-") == strlen(v + 6))
      memcpy(range, v, strlen(v) + 1);
    repassar(c, metodo, range, url);
    pthread_mutex_lock(&trava);
    repasses--; sesUltimo = agoraS();
    pthread_mutex_unlock(&trava);
  }
sai:
  free(cab);
  shutdown(c, SHUT_RDWR);
  close(c);
  pthread_mutex_lock(&trava);
  conexoes--;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&trava);
  return NULL;
}

static void soltarOcioso(void) {
  int soltar = 0;
  pthread_mutex_lock(&trava);
  if (sesAtiva && repasses == 0 && agoraS() - sesUltimo > CASA_OCIOSO_S) {
    sesAtiva = 0; sesHash[0] = sesUrl[0] = 0; soltar = 1;
  }
  pthread_mutex_unlock(&trava);
  if (soltar) {
    if (motor && motor->segurar) motor->segurar(0);
    puts("[casa] sem pedidos: motor liberado");
  }
}

static void *ajudante(void *u) {
  (void)u;
  for (;;) {
    struct pollfd p[3] = { { acorda[0], POLLIN, 0 }, { sockUdp, POLLIN, 0 }, { sockHttp, POLLIN, 0 } };
    int r = poll(p, 3, 1000);
    if (r < 0 && errno != EINTR) break;
    if (p[0].revents) break;
    if (p[1].revents & POLLIN) {
      struct sockaddr_in de;
      socklen_t dl = sizeof de;
      char b[128], resp[200];
      ssize_t k = recvfrom(sockUdp, b, sizeof b, 0, (struct sockaddr *)&de, &dl);
      int n;
      if (k > 0 && casa_e_pergunta(b, (int)k) && casa_ip_privado(ntohl(de.sin_addr.s_addr)) &&
          (n = casa_montar_resposta(portaHttp, token, nomeAparelho, 1, resp, sizeof resp)) > 0)
        if (sendto(sockUdp, resp, (size_t)n, MSG_NOSIGNAL, (struct sockaddr *)&de, dl) < 0) { /* segue */ }
    }
    if (p[2].revents & POLLIN) {
      struct sockaddr_in de;
      socklen_t dl = sizeof de;
      int c = accept(sockHttp, (struct sockaddr *)&de, &dl), cheio;
      if (c >= 0) {
        pthread_t t;
        pthread_attr_t at;
        fcntl(c, F_SETFD, FD_CLOEXEC);
        prazos(c, 30);
        pthread_mutex_lock(&trava);
        cheio = conexoes >= CASA_CONEXOES;
        if (!cheio) conexoes++;
        pthread_mutex_unlock(&trava);
        if (cheio || !casa_ip_privado(ntohl(de.sin_addr.s_addr))) {
          if (!cheio) { pthread_mutex_lock(&trava); conexoes--; pthread_mutex_unlock(&trava); }
          close(c);
        } else {
          pthread_attr_init(&at);
          pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
          if (pthread_create(&t, &at, conexao, (void *)(intptr_t)c)) {
            close(c);
            pthread_mutex_lock(&trava); conexoes--; pthread_mutex_unlock(&trava);
          }
          pthread_attr_destroy(&at);
        }
      }
    }
    soltarOcioso();
  }
  return NULL;
}

static int sortearToken(void) {
  unsigned char b[12];
  FILE *f = fopen("/dev/urandom", "rb");
  size_t n = f ? fread(b, 1, sizeof b, f) : 0;
  if (f) fclose(f);
  if (n != sizeof b) return 0;
  for (unsigned i = 0; i < sizeof b; i++) snprintf(token + i * 2, 3, "%02x", b[i]);
  return 1;
}

static void fecharSocks(void) {
  if (sockUdp >= 0) close(sockUdp);
  if (sockHttp >= 0) close(sockHttp);
  if (acorda[0] >= 0) { close(acorda[0]); close(acorda[1]); }
  sockUdp = sockHttp = acorda[0] = acorda[1] = -1;
}

static int ajudanteSubir(void) {
  struct sockaddr_in a;
  socklen_t al = sizeof a;
  int um = 1, pu;
  if (!sortearToken()) return 0;
  if (gethostname(nomeAparelho, sizeof nomeAparelho) || !nomeAparelho[0] || !strcmp(nomeAparelho, "localhost"))
    snprintf(nomeAparelho, sizeof nomeAparelho, "Nuvio");
  nomeAparelho[sizeof nomeAparelho - 1] = 0;
  pthread_mutex_lock(&trava);
  pu = portaUdp;
  pthread_mutex_unlock(&trava);
  sockUdp = socket(AF_INET, SOCK_DGRAM, 0);
  sockHttp = socket(AF_INET, SOCK_STREAM, 0);
  if (sockUdp < 0 || sockHttp < 0 || pipe(acorda)) { fecharSocks(); return 0; }
  fcntl(sockUdp, F_SETFD, FD_CLOEXEC); fcntl(sockHttp, F_SETFD, FD_CLOEXEC);
  setsockopt(sockUdp, SOL_SOCKET, SO_REUSEADDR, &um, sizeof um);
  setsockopt(sockHttp, SOL_SOCKET, SO_REUSEADDR, &um, sizeof um);
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons((uint16_t)pu);
  if (bind(sockUdp, (struct sockaddr *)&a, sizeof a)) { fecharSocks(); return 0; }
  a.sin_port = htons(CASA_PORTA_HTTP);
  if (bind(sockHttp, (struct sockaddr *)&a, sizeof a)) {
    a.sin_port = 0;
    if (bind(sockHttp, (struct sockaddr *)&a, sizeof a)) { fecharSocks(); return 0; }
  }
  if (listen(sockHttp, 16) || getsockname(sockHttp, (struct sockaddr *)&a, &al)) { fecharSocks(); return 0; }
  portaHttp = ntohs(a.sin_port);
  if (pthread_create(&fioAjudante, NULL, ajudante, NULL)) { fecharSocks(); return 0; }
  ajudanteVivo = 1;
  printf("[casa] ajudante no ar (porta %d)\n", portaHttp);
  return 1;
}

static void ajudanteDescer(void) {
  struct timespec ate;
  int tinha;
  if (!ajudanteVivo) return;
  if (write(acorda[1], "x", 1) < 0) { /* o poll acorda em 1 s */ }
  pthread_join(fioAjudante, NULL);
  // Derruba quem estiver esperando o cliente e espera as conexoes (ate 3 s).
  shutdown(sockHttp, SHUT_RDWR);
  pthread_mutex_lock(&trava);
  clock_gettime(CLOCK_REALTIME, &ate);
  ate.tv_sec += 3;
  while (conexoes > 0 && !pthread_cond_timedwait(&cond, &trava, &ate)) {}
  tinha = sesAtiva;
  sesAtiva = 0; sesHash[0] = sesUrl[0] = 0;
  pthread_mutex_unlock(&trava);
  if (tinha && motor && motor->segurar) motor->segurar(0);
  fecharSocks();
  ajudanteVivo = 0; portaHttp = 0;
  puts("[casa] ajudante fora do ar");
}

// ============================================================== configurar
void casa_configurar(int usar, int ajudar, const CasaMotor *m) {
  static long proxima;
  clienteLigar(usar != 0);
  if (!ajudar || !m || !m->resolver) { ajudanteDescer(); return; }
  if (ajudanteVivo) return;
  if (agoraS() < proxima) return;
  motor = m;
  if (!ajudanteSubir()) { proxima = agoraS() + 10; puts("[casa] ajudante nao subiu (porta ocupada?)"); }
}

void casa_parar(void) {
  clienteLigar(0);
  ajudanteDescer();
}
#endif
