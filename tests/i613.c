// Real Android C recovery + JNI transport boundary; loopback HTTP only.
#ifdef I613_APP
#define main app_worker_regressions
#include "fontefalha_playback.c"
#undef main
static int released;
const CatItem *cat_item(int i) { (void)i; static CatItem c = {.imdb = "tt613", .tipo = "movie"}; return &c; }
const char *video_erro_texto(void) { return "2004 player error:-1"; }
void stream_definir_atual(int i) { assert(i == 0); }
void video_definir_cabecalhos(const char *s) { assert(!strcmp(s, "Authorization: renewed")); }
const PartesLista *partes_sessao_lista(void) { return NULL; }
AddonsConsulta *addons_consulta_capturar(void) { return (AddonsConsulta *)1; }
int addons_consulta_atual(const AddonsConsulta *c) { return c != NULL; }
void addons_consulta_liberar(AddonsConsulta *c) { if (c) released++; }
int stream_vod_renovar(const AddonsConsulta *c, const char *id, const char *tipo,
                       int i, unsigned g, int (*cancelled)(void *), void *ctx, Stream *s) {
  assert(c && !strcmp(id, "tt613") && !strcmp(tipo, "movie") && i == 0);
  if (cancelled(ctx) || g != lista) return 0;
  *s = fontes[0]; strcpy(s->url, "http://127.0.0.1/fresh"); strcpy(s->cabecalhos, "Authorization: renewed");
  return 1;
}
int stream_vod_renovada_aplicar(int i, unsigned g, const Stream *s, uint64_t session) {
  if (g != lista || session != 1) return 0;
  fontes[i] = *s; return 1;
}
int main(void) {
  for (int automatic = 0; automatic <= 1; automatic++) {
    resetar(); fonteVODAutomatica = automatic; posicao = 900; guardarPosicaoFonteFalha();
    falhou = 1; renovarVodInterrompido();
    assert(worker && aguardandoFonte == 2 && paradas == 1);
    concluirWorker(); processarFonteJob();
    assert(aberturas == 1 && retomada == 900 && aguardandoFonte == 0 && vodRenovou);
    assert(strstr(fontes[0].url, "/fresh"));
    renovarVodInterrompido(); assert(!worker);
  }
  resetar(); posicao = 900; guardarPosicaoFonteFalha(); falhou = 1;
  renovarVodInterrompido(); agora += 30001; renovarVodInterrompido();
  assert(aguardandoFonte == 0 && !vodRenovandoDesde);
  concluirWorker(); processarFonteJob(); assert(aberturas == 0);
  resetar(); posicao = 900; guardarPosicaoFonteFalha(); falhou = 1;
  renovarVodInterrompido(); lista++;
  concluirWorker(); processarFonteJob(); assert(aberturas == 0 && aguardandoFonte == 0);
  assert(released == 4);
  limparFonteVOD();
  puts("PASS app: manual/automatic renewal resumes at 900s; timeout/stale result cannot reopen");
  return 0;
}
#else
#ifdef I613_STREAMS
#define main source_worker_regressions
#include "fontefalha_worker.c"
#undef main
#else
#define main android_resume_regressions
#include "video_android_retomada.c"
#undef main
#endif
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>

static int port, opens, httpStatus, lastStart, bodyBytes, dropped;
static unsigned char body[4096];
static int request(const char *path, int start) {
  int fd = socket(AF_INET, SOCK_STREAM, 0); assert(fd >= 0);
  struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons(port)};
  a.sin_addr.s_addr = htonl(0x7f000001u);
  assert(connect(fd, (struct sockaddr *)&a, sizeof a) == 0);
  char req[512], response[8192];
  int n = snprintf(req, sizeof req, "GET %s HTTP/1.1\r\nHost: localhost\r\nRange: bytes=%d-\r\nConnection: close\r\n\r\n", path, start);
  assert(write(fd, req, n) == n);
  int used = 0, got;
  while ((got = (int)read(fd, response + used, sizeof response - used - 1)) > 0) used += got;
  close(fd); response[used] = 0;
  int code = 0; assert(sscanf(response, "HTTP/1.1 %d", &code) == 1);
  char *p = strstr(response, "\r\n\r\n"); assert(p);
  p += 4; bodyBytes = used - (int)(p - response);
  assert(bodyBytes <= sizeof body); memcpy(body, p, bodyBytes);
  return code;
}
static pid_t server(void) {
  int fd = socket(AF_INET, SOCK_STREAM, 0); assert(fd >= 0);
  struct sockaddr_in a = {.sin_family = AF_INET}; a.sin_addr.s_addr = htonl(0x7f000001u);
  assert(bind(fd, (struct sockaddr *)&a, sizeof a) == 0 && listen(fd, 8) == 0);
  socklen_t size = sizeof a; assert(getsockname(fd, (struct sockaddr *)&a, &size) == 0);
  port = ntohs(a.sin_port);
  pid_t child = fork(); assert(child >= 0);
  if (!child) {
    signal(SIGPIPE, SIG_IGN);
    int seen[2] = {0};
    for (;;) {
      int c = accept(fd, NULL, NULL); if (c < 0) _exit(0);
      char req[1024] = "", path[80], hdr[256]; int used = 0, n;
      while (!strstr(req, "\r\n\r\n") && (n = (int)read(c, req + used, sizeof req - used - 1)) > 0) {
        used += n; req[used] = 0;
      }
      assert(sscanf(req, "GET %79s", path) == 1);
      int expired = !strcmp(path, "/expired"), plain = !strcmp(path, "/drop");
      int start = 0; char *r = strstr(req, "Range: bytes="); if (r) sscanf(r, "Range: bytes=%d-", &start);
      if (!strcmp(path, "/addon/stream/movie/tt613.json")) {
        const char *url = "/fresh";
        n = snprintf(hdr, sizeof hdr, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n%s", strlen(url), url);
        write(c, hdr, n);
      } else if (expired && seen[1]) {
        n = snprintf(hdr, sizeof hdr, "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n");
        write(c, hdr, n);
      } else {
        int cut = (plain || expired) && !seen[expired]++;
        int length = 4096 - start; assert(length >= 0);
        n = snprintf(hdr, sizeof hdr, "HTTP/1.1 206 Partial Content\r\nContent-Length: %d\r\nContent-Range: bytes %d-4095/4096\r\n\r\n", length, start);
        write(c, hdr, n);
        if (cut) length = 900;
        unsigned char bytes[4096]; for (int i = 0; i < length; i++) bytes[i] = (start + i) % 251;
        write(c, bytes, length);
      }
      close(c);
    }
  }
  close(fd); return child;
}
#ifndef I613_STREAMS
static void JNICALL transport(JNIEnv *e, jclass c, jmethodID m, ...) {
  (void)e; (void)c;
  if (m != mAbrirPosicao && m != mAbrir) return;
  va_list ap; va_start(ap, m); (void)va_arg(ap, jstring); (void)va_arg(ap, jstring);
  int ms = m == mAbrirPosicao ? va_arg(ap, jint) : 0; va_end(ap);
  lastStart = ms / 1000; opens++;
  const char *path = strrchr(urlAtual, '/'); assert(path);
  httpStatus = request(path, lastStart);
  dropped = httpStatus == 206 && bodyBytes < 4096 - lastStart;
}
static void start(const char *path) {
  char url[256]; snprintf(url, sizeof url, "http://127.0.0.1:%d%s", port, path);
  video_parar(); video_definir_reconexao(1); assert(video_tocar(url));
  assert(dropped && bodyBytes == 900);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_PRONTO, 4096000, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL, NULL, 900000, -1, JNI_FALSE, -1, video_android_sessao());
  video_bombear();
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 2001, -1);
  video_bombear(); assert(video_reconectando());
  relogio += 3000; video_bombear();
}
int main(void) {
  pid_t child = server();
  // Kill server even when a regression assertion aborts the test process.
  // Parent shell owns the process group and cleans up on failure.
  printf("server pid=%d\n", (int)child); fflush(stdout);
  jni.NewGlobalRef = global; jni.GetStaticMethodID = metodo;
  jni.NewString = texto; jni.DeleteLocalRef = apagar;
  jni.ExceptionCheck = temExcecao; jni.ExceptionClear = limparExcecao;
  jni.CallStaticVoidMethod = transport;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeIniciar(&env, (jclass)(uintptr_t)3);
  mParar = (jmethodID)(uintptr_t)4;
  start("/drop");
  assert(lastStart == 900); // RED: backend used to open at zero, seek after PLAYING.
  assert(httpStatus == 206 && !dropped && bodyBytes == 3196);
  for (int i = 0; i < bodyBytes; i++) assert(body[i] == (900 + i) % 251);
  puts("PASS connection drop: Range resume at 900s, contiguous bytes");
  start("/expired");
  assert(lastStart == 900 && httpStatus == 403);
  for (int i = 1; i <= NV_RECON_MAX; i++) {
    Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 2004, -1);
    video_bombear();
    assert(video_pos() == 900);
    if (i < NV_RECON_MAX) { relogio += nv_recon_espera_ms(i + 1); video_bombear(); }
  }
  assert(video_falhou() && !video_reconectando());
  puts("PASS expired URL: bounded retries, saved position retained for addon renewal");
  // Silent reopen: no READY/error callbacks, including across tick wrap.
  nv_recon_zerar(&recon); reconPermitida = reconIniciou = 1;
  prontoLoad = 0; falhou = 0; relogio = UINT32_MAX - 1000;
  assert(nv_recon_erro(&recon, 1, relogio, 900));
  for (int i = 1; i <= NV_RECON_MAX; i++) {
    relogio += nv_recon_espera_ms(i); video_bombear();
    relogio += NV_RECON_PRAZO_MS; video_bombear();
  }
  assert(video_falhou() && recon.esgotou && video_pos() == 900);
  puts("PASS silent reopen: deadline exhausts retries; no permanent reconnect");
  kill(child, SIGTERM); waitpid(child, NULL, 0);
  return 0;
}
#else
static Stream original;
static int addonCalls, ambiguous, stale;
int addons_consulta_consultar(const AddonsConsulta *c, const char *id, const char *tipo,
                              int threads, int (*cancelled)(void *), void *ctx, void *out) {
  (void)c; (void)threads; (void)cancelled; (void)ctx;
  assert(!strcmp(id, "tt613") && !strcmp(tipo, "movie"));
  addonCalls++;
  assert(request("/addon/stream/movie/tt613.json", 0) == 200);
  body[bodyBytes] = 0;
  Stream *s = calloc(ambiguous ? 2 : 1, sizeof *s); assert(s);
  s[0] = original;
  snprintf(s[0].url, sizeof s[0].url, "http://127.0.0.1:%d%s", port, body);
  snprintf(s[0].cabecalhos, sizeof s[0].cabecalhos, "Authorization: renewed");
  if (ambiguous) s[1] = s[0];
  *(Stream **)out = s;
  if (stale) stream_definir_lista(&original, 1);
  return ambiguous ? 2 : 1;
}
int main(void) {
  pid_t child = server(); printf("server pid=%d\n", (int)child); fflush(stdout);
  snprintf(original.url, sizeof original.url, "http://127.0.0.1:%d/expired", port);
  strcpy(original.addonChave, "addon-613"); strcpy(original.arquivo, "movie.mp4");
  original.tamanhoBytes = 4096;
  stream_definir_lista(&original, 1);
  unsigned generation = stream_lista_geracao();
  assert(request("/expired", 0) == 206 && bodyBytes == 900);
  assert(request("/expired", 900) == 403);
  Stream renewed;
  assert(stream_vod_renovar((AddonsConsulta *)1, "tt613", "movie", 0, generation, NULL, NULL, &renewed));
  assert(addonCalls == 1 && strstr(renewed.url, "/fresh"));
  assert(stream_vod_renovada_aplicar(0, generation, &renewed, 1));
  assert(!strcmp(stream_item(0)->cabecalhos, "Authorization: renewed"));
  assert(request(strrchr(renewed.url, '/'), 900) == 206 && bodyBytes == 3196);
  for (int i = 0; i < bodyBytes; i++) assert(body[i] == (900 + i) % 251);
  puts("PASS 403: fresh addon query, same file/headers, Range resumes contiguous bytes at 900s");
  ambiguous = 1;
  assert(!stream_vod_renovar((AddonsConsulta *)1, "tt613", "movie", 0, generation, NULL, NULL, &renewed));
  ambiguous = 0; stale = 1;
  assert(!stream_vod_renovar((AddonsConsulta *)1, "tt613", "movie", 0, generation, NULL, NULL, &renewed));
  assert(!stream_vod_renovada_aplicar(0, generation, &renewed, 1));
  puts("PASS ambiguous file and stale generation rejected");
  kill(child, SIGTERM); waitpid(child, NULL, 0); stream_definir_lista(NULL, 0);
  return 0;
}
#endif
#endif
