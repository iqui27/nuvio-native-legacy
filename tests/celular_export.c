#define _GNU_SOURCE
#define main celular_original_tests
#include "celular.c"
#undef main
#include "logexport.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
int main(void) {
  char source[] = "/tmp/nuvio-export-test-XXXXXX", path[128], args[1024], local[192], tok[16];
  int fd = mkstemp(source); assert(fd >= 0); FILE *f = fdopen(fd, "w"); assert(f);
  fputs("[app] startup=1\n[dts] stage=worker-rate wallMs=5000\n[net] password=DO_NOT_EXPORT\n"
        "[video] pos=32.125\n<script>alert(1)</script>\n", f); fclose(f);
  assert(logexport_create(source, path, sizeof path));
  assert(celular_abrir_relatorio(path));
  snprintf(tok, sizeof tok, "%s", strrchr(celular_url(), '/') + 1);
  snprintf(local, sizeof local, "http://127.0.0.1:%d/%s", celular_porta(), tok);
  snprintf(args, sizeof args, "'%s'", local);
  assert(curl(args) == 200 && strstr(saida, "Download .txt") && strstr(saida, "textContent"));
  assert(!strstr(saida, "DO_NOT_EXPORT") && !strstr(saida, "wallMs=5000"));
  snprintf(args, sizeof args, "'%s?text=1'", local);
  assert(curl(args) == 200 && strstr(saida, "wallMs=5000") && strstr(saida, "pos=32.125"));
  assert(!strstr(saida, "DO_NOT_EXPORT") && strstr(saida, "REDACTED"));
  snprintf(args, sizeof args, "-I '%s?download=1'", local);
  assert(curl(args) == 200 && strstr(saida, "text/plain") && strstr(saida, "Content-Disposition: attachment"));
  assert(strstr(saida, "Content-Length:") && strstr(saida, "no-store") && !strstr(saida, "wallMs=5000"));
  snprintf(args, sizeof args, "'%s?download=1'", local);
  assert(curl(args) == 200 && strstr(saida, "wallMs=5000") && !strstr(saida, "DO_NOT_EXPORT"));
  snprintf(args, sizeof args, "--data 't=anything' '%s'", local); assert(curl(args) == 405);
  assert(celular_estado() == CEL_ESPERANDO && !celular_pegar(args, sizeof args));
  snprintf(args, sizeof args, "-H 'Host: evil.example' '%s?text=1'", local); assert(curl(args) == 403);
  snprintf(args, sizeof args, "-H 'Origin: https://evil.example' '%s?text=1'", local); assert(curl(args) == 403);
  snprintf(args, sizeof args, "'http://127.0.0.1:%d/incorrect?text=1'", celular_porta()); assert(curl(args) == 404);
  /* A snapshot does not change when its source appends new messages. */
  f = fopen(source, "a"); assert(f); fputs("[app] AFTER_SNAPSHOT\n", f); fclose(f);
  snprintf(args, sizeof args, "'%s?text=1'", local); assert(curl(args) == 200 && !strstr(saida, "AFTER_SNAPSHOT"));
  celular_fechar(); unlink(path);
  assert(celular_abrir_relatorio(source)); assert(strcmp(tok, strrchr(celular_url(), '/') + 1));
  /* Stable diagnostics address: the saved token and the fixed port survive a
   * reopen (bookmark on the computer); a busy port falls back, the token stays. */
  { char gen[16], first[96]; int busy, one = 1; struct sockaddr_in b = {0};
    assert(celular_gerar_token(gen, sizeof gen) && strlen(gen) == 8 && !celular_gerar_token(gen, 4));
    celular_fechar(); celular_relatorio_fixo("abcdefgh", CEL_RELATORIO_PORTA);
    assert(celular_abrir_relatorio(source)); snprintf(first, sizeof first, "%s", celular_url());
    assert(celular_porta() == CEL_RELATORIO_PORTA && !strcmp(strrchr(first, '/') + 1, "abcdefgh"));
    snprintf(args, sizeof args, "'http://127.0.0.1:%d/abcdefgh?download=1'", CEL_RELATORIO_PORTA);
    assert(curl(args) == 200 && strstr(saida, "AFTER_SNAPSHOT"));
    celular_fechar(); assert(celular_abrir_relatorio(source)); assert(!strcmp(first, celular_url()));
    /* The phone-typing server never reuses the diagnostics token or port. */
    celular_fechar(); assert(celular_abrir("x")); assert(strcmp(strrchr(celular_url(), '/') + 1, "abcdefgh"));
    assert(celular_porta() != CEL_RELATORIO_PORTA);
    celular_fechar(); busy = socket(AF_INET, SOCK_STREAM, 0); assert(busy >= 0);
    setsockopt(busy, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    b.sin_family = AF_INET; b.sin_port = htons(CEL_RELATORIO_PORTA); b.sin_addr.s_addr = htonl(INADDR_ANY);
    assert(!bind(busy, (struct sockaddr *)&b, sizeof b) && !listen(busy, 1));
    assert(celular_abrir_relatorio(source)); assert(celular_porta() != CEL_RELATORIO_PORTA);
    assert(!strcmp(strrchr(celular_url(), '/') + 1, "abcdefgh")); celular_fechar(); close(busy);
    celular_relatorio_fixo("bad!", CEL_RELATORIO_PORTA);
    assert(celular_abrir_relatorio(source)); assert(strcmp(strrchr(celular_url(), '/') + 1, "abcdefgh"));
    celular_relatorio_fixo("", 0); }
  /* With a deliberately stalled reader the stop pipe must cancel send. */
  celular_fechar(); f = fopen(source, "w"); assert(f);
  for (int i = 0; i < 8 * 1024 * 1024; i++) fputc('a', f); fclose(f);
  assert(celular_abrir_relatorio(source));
  int client = socket(AF_INET, SOCK_STREAM, 0), small = 4096; assert(client >= 0);
  setsockopt(client, SOL_SOCKET, SO_RCVBUF, &small, sizeof small);
  struct sockaddr_in a = {0}; a.sin_family = AF_INET; a.sin_port = htons(celular_porta()); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  assert(!connect(client, (struct sockaddr *)&a, sizeof a));
  snprintf(args, sizeof args, "GET %s?download=1 HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n\r\n", strrchr(celular_url(), '/'), celular_porta());
  assert(send(client, args, strlen(args), 0) == (ssize_t)strlen(args));
  usleep(150000); double begin = now(); celular_fechar(); assert(now() - begin < 1.0); close(client);
  /* Expiry prevents reuse, even when no phone connected. */
  assert(celular_abrir_relatorio(source)); sleep(CEL_VALIDADE_RELATORIO_S + 1); assert(celular_estado() == CEL_EXPIROU);
  celular_fechar(); unlink(source);
  puts("PASS: sanitized LAN export, download, HEAD, XSS isolation, Host/Origin/token, frozen snapshot, cancellation, expiry");
  return celular_original_tests();
}
