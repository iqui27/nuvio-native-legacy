/* #418: real session code, only transport/clock wait/storage doubled.
 * Catches lost transport classes, credential retries, unlimited retries,
 * changed nonce/body on retry, and QR failing over to anonymous grant on DNS. */
#include <assert.h>
#include <errno.h>
#include <time.h>
static int waits;
static long delays[8];
static int testSleep(const struct timespec *req, struct timespec *rem) {
  (void)rem; assert(waits < 8); delays[waits++] = req->tv_sec; return 0;
}
#define nanosleep testSleep
#include "../src/sessao.c"
#undef nanosleep

static int calls, failCount, failStatus, writes;
static const char *transport;
static char firstBody[2048], firstBearer[3000], lastPath[300];
static int rejectSignup;
int nuvem_pronta(void) { return 1; }
const char *nuvem_base_login(void) { return "https://example.test/tv"; }
const char *nuvem_ultimo_erro(void) { return transport; }
int nuvem_erro_ausente(const char *r) { (void)r; return 0; }
void nuvem_falhou(void) {}
void nuvem_ok(void) {}
char *dados_ler(const char *name) { (void)name; return NULL; }
int dados_gravar(const char *name, const char *body) { (void)name; (void)body; writes++; return 1; }
int dados_apagar(const char *name) { (void)name; return 1; }
void dados_uuid(char *dst, unsigned n) { snprintf(dst, n, "00000000-0000-4000-8000-000000000000"); }
const char *i18n(const char *s) { return s; }
char *nuvem_post(const char *path, const char *body, const char *bearer, int *st) {
  if (!calls || strcmp(lastPath, path)) {
    snprintf(lastPath, sizeof lastPath, "%s", path);
    snprintf(firstBody, sizeof firstBody, "%s", body);
    snprintf(firstBearer, sizeof firstBearer, "%s", bearer ? bearer : "");
  } else {
    assert(!strcmp(body, firstBody));
    assert(!strcmp(bearer ? bearer : "", firstBearer));
  }
  calls++;
  if (rejectSignup && !strcmp(path, "/auth/v1/signup")) {
    *st = 403; return strdup("{\"message\":\"SECRET_RESPONSE\"}");
  }
  if (calls <= failCount) {
    *st = failStatus;
    return *st ? strdup("{\"message\":\"SECRET_RESPONSE\"}") : NULL;
  }
  *st = 200;
  if (strstr(path, "start_tv_login_session"))
    return strdup("{\"code\":\"abc\",\"web_url\":\"https://example.test/tv?code=abc\"}");
  return strdup("{\"access_token\":\"x.eyJzdWIiOiJ1MSIsImV4cCI6NDEwMjQ0NDgwMH0.y\",\"refresh_token\":\"SECRET_TOKEN\"}");
}
char *nuvem_rpc_com(const char *f, const char *b, const char *t, int *s) { return nuvem_post(f, b, t, s); }
char *nuvem_tabela(const char *a, const char *b, const char *c, int *s) { return nuvem_post(a, b, c, s); }
static void reset(int st, const char *err, int failures) {
  calls = waits = writes = 0; failStatus = st; transport = err; failCount = failures;
  limpar(); estado = SES_DESLOGADO; emailPedido = rejectSignup = 0;
  erro[0] = erroEmail[0] = 0;
}
static void email(void) {
  snprintf(emailPend, sizeof emailPend, "SECRET_EMAIL");
  snprintf(senhaPend, sizeof senhaPend, "SECRET_PASSWORD");
  fioEmail(NULL);
  assert(!emailPend[0] && !senhaPend[0]);
}
int main(void) {
  const struct { int st; const char *err, *label; int attempts; } cases[] = {
    {0, "curl 28: SECRET_URL", "timeout", 3},
    {0, "curl 6: SECRET_URL", "DNS", 3},
    {0, "curl 5: SECRET_URL", "DNS", 3},
    {0, "curl 35: SECRET_URL", "TLS", 3},
    {0, "curl 60: certificate is not yet valid SECRET_URL", "TLS", 1},
    {0, "curl 77: SECRET_URL", "TLS", 1},
    {0, "curl 7: SECRET_URL", "connection", 3},
    {0, "curl 56: SECRET_URL", "connection", 3},
    {0, "curl 42: SECRET_URL", "transport", 1},
    {503, "curl 28: stale", "HTTP 503", 3},
    {401, "curl 6: stale", "HTTP 401", 1},
    {200, "curl 6: stale", "response", 1}
  };
  size_t i;
  for (i = 0; i < sizeof cases / sizeof *cases; i++) {
    reset(cases[i].st, cases[i].err, 10);
    email();
    assert(strstr(sessao_erro_email(), cases[i].label));
    assert(calls == cases[i].attempts && waits == calls - 1);
    if (waits) assert(delays[0] == 1 && delays[1] == 2);
    assert(!sessao_logada() && !writes);
    assert(!strstr(sessao_erro_email(), "SECRET"));
  }
  reset(0, "curl 28: SECRET_URL", 2);
  email();
  assert(calls == 3 && waits == 2 && sessao_logada() && writes == 1);
  reset(400, "", 10); email();
  assert(calls == 1 && !waits && strstr(erroEmail, "E-mail ou senha"));
  reset(429, "", 10); email();
  assert(calls == 3 && waits == 2 && strstr(erroEmail, "Muitas tentativas"));
  reset(0, "curl 6: SECRET_URL", 10);
  fioPedir(NULL);
  assert(calls == 3 && waits == 2 && estado == SES_ERRO);
  assert(strstr(sessao_erro(), "DNS") && !writes);
  reset(503, "", 2);
  snprintf(acesso, sizeof acesso, "existing-anonymous-token"); anonima = 1;
  fioPedir(NULL);
  assert(calls == 3 && waits == 2 && estado == SES_AGUARDANDO);
  assert(!strcmp(sessao_codigo(), "abc") && !writes);
  assert(strstr(firstBody, "p_device_nonce"));
  reset(400, "", 10);
  snprintf(acesso, sizeof acesso, "existing-anonymous-token"); anonima = 1;
  fioPedir(NULL);
  assert(calls == 1 && !waits && estado == SES_ERRO);
  assert(strstr(sessao_erro(), "HTTP 400") && !strstr(sessao_erro(), "SECRET"));
  reset(0, "curl 6: SECRET_URL", 10); emailPedido = 1; estado = SES_EMAIL;
  fioPedir(NULL);
  assert(calls == 1 && !waits && estado == SES_EMAIL);
  reset(0, "", 0); rejectSignup = 1;
  fioPedir(NULL);
  assert(calls == 3 && !waits && estado == SES_AGUARDANDO && writes == 1);
  puts("login418: ok");
  return 0;
}
