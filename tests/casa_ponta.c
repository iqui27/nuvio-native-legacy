// Nuvio Casa de ponta a ponta, num so processo e so em 127.0.0.1:
//
//   p2p.c (cliente, o de verdade)  --UDP-->  casa_rede.c (ajudante, o de verdade)
//        |  GET settings / POST create / GET video                |
//        +--------------------- HTTP ----------------------------+--> motor FALSO
//
// O motor falso e um servidor HTTP em 127.0.0.1 que entrega bytes conhecidos e
// respeita Range, no lugar do libtorrent. O transporte do cliente (rede_*) e um
// HTTP minimo por socket, no lugar do rede.c. NAO prova: torrent real, TV,
// Android, broadcast entre aparelhos diferentes.
#include "casa.h"
#include "casa_rede.h"
#include "idbase.h"
#include "nvtmp.h"
#include "p2p.h"
#include "p2pmotor.h"
#include "rede.h"
#include <arpa/inet.h>
#include <assert.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

// ---------------------------------------------------------------- ajustes
static int ligado = 1;
static char endereco[200];
int ajustes_p2p_ligado(void) { return ligado; }
const char *ajustes_p2p_url(void) { return endereco; }
int ajustes_p2p_prazo_metadados_s(void) { return 30; }   // 2.1: prazo configuravel
void debrid_episodio(int *t, int *e) { idbase_episodio("tt1:1:2", t, e); }
uint64_t badges_detectar(const char *m) { (void)m; return 0; }
uint64_t badges_bit(const char *id) { (void)id; return 0; }
const char *dados_dir(void) { return nvt(""); }
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; r->erro = REDE_INDISPONIVEL; return 0; }
void rede_resposta_limpar(RedeResposta *r) { (void)r; }
// 2.1 (#775): o p2p.c instala a vigia de cancelamento em volta dos pedidos.
RedePararEstado rede_parar_trocar(int (*parar)(void *), void *usuario) { (void)parar; (void)usuario; return (RedePararEstado){ .instalado = 1 }; }

// ------------------------------------------------------------ http minimo
static double agora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}
// Devolve o corpo (malloc) e o status; NULL sem conexao. `extra` = cabecalhos
// prontos ("Range: ...\r\n"); `host` NULL = o da URL.
static char *http(const char *metodo, const char *url, const char *host, const char *extra,
                  const char *corpo, int *st, long *tam) {
  unsigned a, b, c, d, porta;
  char caminho[400] = "/", ped[2400], *buf = NULL, *fim;
  struct sockaddr_in end;
  size_t n = 0, cap = 0;
  int s, k;
  if (st) *st = 0;
  if (tam) *tam = 0;
  if (sscanf(url, "http://%u.%u.%u.%u:%u%399s", &a, &b, &c, &d, &porta, caminho) < 5) return NULL;
  s = socket(AF_INET, SOCK_STREAM, 0);
  memset(&end, 0, sizeof end);
  end.sin_family = AF_INET; end.sin_port = htons((uint16_t)porta);
  end.sin_addr.s_addr = htonl((a << 24) | (b << 16) | (c << 8) | d);
  if (connect(s, (struct sockaddr *)&end, sizeof end)) { close(s); return NULL; }
  if (host) k = snprintf(ped, sizeof ped, "%s %s HTTP/1.1\r\nHost: %s\r\n", metodo, caminho, host);
  else k = snprintf(ped, sizeof ped, "%s %s HTTP/1.1\r\nHost: %u.%u.%u.%u:%u\r\n", metodo, caminho, a, b, c, d, porta);
  k += snprintf(ped + k, sizeof ped - (size_t)k, "%sContent-Length: %u\r\nConnection: close\r\n\r\n%s",
                extra ? extra : "", (unsigned)(corpo ? strlen(corpo) : 0), corpo ? corpo : "");
  assert(k < (int)sizeof ped);
  if (send(s, ped, (size_t)k, MSG_NOSIGNAL) != k) { close(s); return NULL; }
  for (;;) {
    ssize_t r;
    if (cap - n < 65536) { cap = cap ? cap * 2 : 131072; buf = realloc(buf, cap + 1); assert(buf); }
    r = recv(s, buf + n, cap - n, 0);
    if (r <= 0) break;
    n += (size_t)r;
  }
  close(s);
  buf[n] = 0;
  fim = n ? strstr(buf, "\r\n\r\n") : NULL;
  if (!fim || sscanf(buf, "HTTP/1.1 %d", st ? st : &k) != 1) { free(buf); return NULL; }
  n -= (size_t)(fim + 4 - buf);
  memmove(buf, fim + 4, n);
  buf[n] = 0;
  if (tam) *tam = (long)n;
  return buf;
}

static char ultimoCreate[400];
char *rede_baixar_st(const char *url, int s, const char *const *c, int *st) {
  (void)s; (void)c;
  return http("GET", url, NULL, NULL, NULL, st, NULL);
}
char *rede_postar_st(const char *url, int s, const char *const *c, const char *corpo, int *st) {
  (void)s; (void)c;
  snprintf(ultimoCreate, sizeof ultimoCreate, "%s", url);
  return http("POST", url, NULL, NULL, corpo, st, NULL);
}
char *rede_baixar_trecho_st(const char *url, int s, long long ini, long long fim, long *tam,
                            int *st, int *erro, char *final, unsigned tf) {
  char r[80];
  (void)s; (void)erro; (void)final; (void)tf;
  snprintf(r, sizeof r, "Range: bytes=%lld-%lld\r\n", ini, fim);
  return http("GET", url, NULL, r, NULL, st, tam);
}

// ------------------------------------------------------------ motor falso
#define VIDEO_TAM (48u * 1024 * 1024)
static unsigned char *video;
static int motorPorta, motorSock, motorPedidos;
static pthread_mutex_t mt = PTHREAD_MUTEX_INITIALIZER;

static void *motorFio(void *u) {
  (void)u;
  for (;;) {
    char cab[2048], resp[300];
    unsigned long long ini = 0, fim = VIDEO_TAM - 1;
    int c = accept(motorSock, NULL, NULL), n = 0, parcial = 0, k;
    const char *r;
    if (c < 0) break;
    while (n < (int)sizeof cab - 1) {
      ssize_t x = recv(c, cab + n, sizeof cab - 1 - (size_t)n, 0);
      if (x <= 0) break;
      n += (int)x; cab[n] = 0;
      if (strstr(cab, "\r\n\r\n")) break;
    }
    cab[n] = 0;
    pthread_mutex_lock(&mt); motorPedidos++; pthread_mutex_unlock(&mt);
    if (strncmp(cab, "GET /stream/7 ", 14) && strncmp(cab, "HEAD /stream/7 ", 15)) {
      k = snprintf(resp, sizeof resp, "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n");
      send(c, resp, (size_t)k, MSG_NOSIGNAL);
      close(c);
      continue;
    }
    if ((r = strstr(cab, "Range: bytes="))) {
      parcial = 1;
      if (sscanf(r + 13, "%llu-%llu", &ini, &fim) < 2) fim = VIDEO_TAM - 1;
      if (fim >= VIDEO_TAM) fim = VIDEO_TAM - 1;
    }
    k = parcial
      ? snprintf(resp, sizeof resp, "HTTP/1.1 206 Partial Content\r\nContent-Type: video/x-matroska\r\nAccept-Ranges: bytes\r\n"
                 "Content-Range: bytes %llu-%llu/%u\r\nContent-Length: %llu\r\n\r\n", ini, fim, VIDEO_TAM, fim - ini + 1)
      : snprintf(resp, sizeof resp, "HTTP/1.1 200 OK\r\nContent-Type: video/x-matroska\r\nAccept-Ranges: bytes\r\n"
                 "Content-Length: %u\r\n\r\n", VIDEO_TAM);
    send(c, resp, (size_t)k, MSG_NOSIGNAL);
    if (cab[0] == 'G') {
      size_t falta = (size_t)(fim - ini + 1), o = (size_t)ini;
      while (falta) {
        ssize_t w = send(c, video + o, falta > 65536 ? 65536 : falta, MSG_NOSIGNAL);
        if (w <= 0) break;
        o += (size_t)w; falta -= (size_t)w;
      }
    }
    close(c);
  }
  return NULL;
}

static void motorSubir(void) {
  struct sockaddr_in a;
  socklen_t al = sizeof a;
  pthread_t t;
  video = malloc(VIDEO_TAM);
  assert(video);
  for (unsigned i = 0; i < VIDEO_TAM; i++) video[i] = (unsigned char)((i * 2654435761u) >> 24);
  motorSock = socket(AF_INET, SOCK_STREAM, 0);
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  assert(!bind(motorSock, (struct sockaddr *)&a, sizeof a) && !listen(motorSock, 8));
  assert(!getsockname(motorSock, (struct sockaddr *)&a, &al));
  motorPorta = ntohs(a.sin_port);
  assert(!pthread_create(&t, NULL, motorFio, NULL));
  pthread_detach(t);
}

// O que o ajudante pediu ao motor.
static char mHash[41], mFontes[400];
static int mIdx, mTemp, mEp, mErro, mOcupado, mSegurado, mResolucoes;
static int motorResolver(const char *hash, int idx, const char *fontes, int temporada, int episodio,
                         char *url, unsigned n, uint64_t *tam) {
  mResolucoes++;
  snprintf(mHash, sizeof mHash, "%s", hash);
  snprintf(mFontes, sizeof mFontes, "%s", fontes ? fontes : "");
  mIdx = idx; mTemp = temporada; mEp = episodio;
  if (mErro) return mErro;
  snprintf(url, n, "http://127.0.0.1:%d/stream/7", motorPorta);
  *tam = VIDEO_TAM;
  return P2P_OK;
}
static void motorSegurar(int s) { mSegurado = s; }
static int motorOcupado(void) { return mOcupado; }
static const CasaMotor MOTOR = { motorResolver, motorSegurar, motorOcupado };

static const char *HASH = "DD8255ECDC7CA55FB0BBF81323D87062DB1F6D1C";
static const char *HASH2 = "aa8255ecdc7ca55fb0bbf81323d87062db1f6d1c";

int main(void) {
  char base[200], url[400], outra[400], *r;
  int udp = 40000 + (int)(getpid() % 20000), st = 0, antes;
  long tam = 0;
  double t0, t1;

  motorSubir();
  casa_teste(udp, "127.0.0.1");
  p2p_definir_casa(casa_ajudante_base);

  // 1) Ninguem na rede: nada muda. P2P ligado sem endereco e sem motor segue inativo.
  assert(!p2pmotor_disponivel());
  assert(casa_procurar(300) == 0 && !casa_ajudante_base(base, sizeof base));
  assert(!p2p_ativo() && !p2p_usa_casa());
  assert(p2p_resolver(HASH, -1, NULL, url, sizeof url) == P2P_ERR_DESLIGADO);

  // 2) O ajudante sobe e a descoberta o acha.
  casa_configurar(0, 1, &MOTOR);
  assert(casa_ajudando() && casa_porta() > 0);
  t0 = agora();
  assert(casa_procurar(1500) == 1);
  t1 = agora();
  assert(casa_ajudante_base(base, sizeof base));
  printf("descoberta: ajudante achado (a busca fica aberta 1500 ms; levou %.0f ms)\n", (t1 - t0) * 1000);
  assert(!strncmp(base, "http://", 7));   // 127.0.0.1 ou o IP da placa, quem respondeu primeiro
  assert(p2p_ativo() && p2p_usa_casa() && !p2p_usa_motor());

  // 3) Um torrent sem link, resolvido pelo motor do ajudante.
  assert(p2p_testar(outra, sizeof outra) == P2P_OK && !strcmp(outra, CASA_VERSAO));
  t0 = agora();
  assert(p2p_resolver(HASH, 3, "tracker:udp://t.example:80\ndht:abcd\nhttp://evil/", url, sizeof url) == P2P_OK);
  t1 = agora();
  assert(mResolucoes == 1 && !strcmp(mHash, "dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c"));
  assert(mIdx == 3 && mTemp == 1 && mEp == 2 && mSegurado == 1);
  assert(strstr(mFontes, "tracker:udp://t.example:80\n") && strstr(mFontes, "dht:abcd\n") && !strstr(mFontes, "evil"));
  assert(strstr(ultimoCreate, "/create?idx=3&s=1&e=2"));
  snprintf(outra, sizeof outra, "%s/dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c/0", base);
  assert(!strcmp(url, outra));
  printf("resolver: url pronta em %.0f ms (settings + create + 64 KB)\n", (t1 - t0) * 1000);

  // 4) O video chega igual, inteiro e por Range (o player pula).
  r = http("GET", url, NULL, "Range: bytes=1000000-1999999\r\n", NULL, &st, &tam);
  assert(r && st == 206 && tam == 1000000 && !memcmp(r, video + 1000000, 1000000));
  free(r);
  r = http("GET", url, NULL, "Range: bytes=50331000-\r\n", NULL, &st, &tam);   // fim do arquivo
  assert(r && st == 206 && tam == (long)(VIDEO_TAM - 50331000u) && !memcmp(r, video + 50331000u, (size_t)tam));
  free(r);
  t0 = agora();
  r = http("GET", url, NULL, NULL, NULL, &st, &tam);
  t1 = agora();
  assert(r && st == 200 && tam == (long)VIDEO_TAM && !memcmp(r, video, VIDEO_TAM));
  free(r);
  printf("repasse: %u MB em %.0f ms = %.0f MB/s (loopback, um fluxo)\n", VIDEO_TAM >> 20, (t1 - t0) * 1000,
         (VIDEO_TAM / 1048576.0) / (t1 - t0));
  r = http("HEAD", url, NULL, NULL, NULL, &st, &tam);
  assert(r && st == 200 && tam == 0);
  free(r);
  // O mesmo torrent de novo nao resolve duas vezes.
  assert(p2p_resolver(HASH, 3, NULL, outra, sizeof outra) == P2P_OK && mResolucoes == 1);

  // 5) Quem nao entra: token errado, Host por nome, pagina de navegador, rota
  // fora do contrato, torrent que ninguem criou. O motor nao ve nenhum deles.
  pthread_mutex_lock(&mt); antes = motorPedidos; pthread_mutex_unlock(&mt);
  snprintf(outra, sizeof outra, "http://127.0.0.1:%d/ffffffffffffffffffffffff/settings", casa_porta());
  r = http("GET", outra, NULL, NULL, NULL, &st, NULL); assert(r && st == 404); free(r);
  snprintf(outra, sizeof outra, "%s/settings", base);
  r = http("GET", outra, "casa.example:47471", NULL, NULL, &st, NULL); assert(r && st == 404); free(r);
  r = http("GET", outra, NULL, "Origin: http://evil.example\r\n", NULL, &st, NULL); assert(r && st == 404); free(r);
  snprintf(outra, sizeof outra, "%s/%s/0", base, HASH2);
  r = http("GET", outra, NULL, NULL, NULL, &st, NULL); assert(r && st == 404); free(r);
  snprintf(outra, sizeof outra, "%s/proxy?url=http://example.com/", base);
  r = http("GET", outra, NULL, NULL, NULL, &st, NULL); assert(r && st == 404); free(r);
  pthread_mutex_lock(&mt); assert(motorPedidos == antes); pthread_mutex_unlock(&mt);
  assert(mResolucoes == 1);

  // 5b) Conexao lenta (um byte a cada 2 s) e solta pelo prazo TOTAL de leitura,
  // nao fica segurando uma das conexoes do ajudante por horas.
  {
    int s = socket(AF_INET, SOCK_STREAM, 0), fechou = 0;
    struct sockaddr_in a;
    const char *ped = "GET /";
    double ini = agora();
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons((uint16_t)casa_porta());
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(s >= 0 && !connect(s, (struct sockaddr *)&a, sizeof a));
    for (int i = 0; ped[i] && !fechou; i++) {
      char lixo;
      if (send(s, ped + i, 1, MSG_NOSIGNAL) != 1) { fechou = 1; break; }
      for (int k = 0; k < 20 && !fechou; k++) {   // 2 s em passos de 100 ms
        struct timeval tv = { 0, 100000 };
        fd_set f; FD_ZERO(&f); FD_SET(s, &f);
        if (select(s + 1, &f, NULL, NULL, &tv) > 0 && recv(s, &lixo, 1, 0) <= 0) fechou = 1;
      }
    }
    if (!fechou) {   // o pedido nunca termina: espera o ajudante desistir
      struct timeval tv = { 12, 0 };
      char lixo;
      setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
      fechou = recv(s, &lixo, 1, 0) <= 0;
    }
    close(s);
    fprintf(stderr, "conexao lenta: solta=%d em %.1f s\n", fechou, agora() - ini);
    assert(fechou && agora() - ini < 12.0);
  }

  // 6) Erros do motor chegam ao cliente com o nome certo.
  mErro = P2P_ERR_SEM_PEERS;
  assert(p2p_resolver(HASH2, -1, NULL, outra, sizeof outra) == P2P_ERR_SEM_PEERS && !outra[0]);
  assert(mResolucoes == 2 && mSegurado == 0);
  mErro = 0; mOcupado = 1;   // o dono do aparelho esta usando o motor
  assert(p2p_resolver(HASH2, -1, NULL, outra, sizeof outra) == P2P_ERR_RECUSOU && mResolucoes == 2);
  mOcupado = 0;
  assert(p2p_resolver(HASH2, -1, NULL, outra, sizeof outra) == P2P_OK && mResolucoes == 3 && mSegurado == 1);

  // 7) O endereco digitado vence o ajudante.
  snprintf(endereco, sizeof endereco, "http://127.0.0.1:1");
  assert(p2p_ativo() && !p2p_usa_casa());
  assert(p2p_resolver(HASH, -1, NULL, outra, sizeof outra) == P2P_ERR_SERVIDOR);
  endereco[0] = 0;

  // 8) Ajuste desligado no ajudante: portas fechadas, motor solto, e o cliente
  // recebe "servidor fora do ar" em vez de travar.
  casa_configurar(0, 0, &MOTOR);
  assert(!casa_ajudando() && casa_porta() == 0 && mSegurado == 0);
  assert(p2p_resolver(HASH, -1, NULL, outra, sizeof outra) == P2P_ERR_SERVIDOR);
  ligado = 0;
  assert(!p2p_ativo() && !p2p_usa_casa());

  // 9) O fio de busca do cliente acha sozinho e para limpo.
  ligado = 1;
  casa_configurar(1, 1, &MOTOR);
  for (int i = 0; i < 40 && !p2p_usa_casa(); i++) usleep(100000);
  assert(p2p_usa_casa());
  casa_parar();
  assert(!casa_ajudando() && !casa_ajudante_base(base, sizeof base));

  free(video);
  puts("casa: ponta a ponta ok");
  return 0;
}
