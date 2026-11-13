// Diagnostics QR card lifecycle (Blue/F10), headless: the switch gates it
// (off by default, turning it off closes an open card), the frozen redacted
// snapshot exists only while the card is open, a concrete local failure is
// explained instead of a fake address, and the cloud upload starts ONLY on
// the person's RIGHT/OK (here it fails fast: no worker URL in the test build).
#include "../src/celbotao.c"
#include <assert.h>
#include <sys/stat.h>
extern int qr_switch, qr_uploads, qr_deletes, qr_allocations;
extern const char *log_source;
static int existe(const char *p) { struct stat s; return p[0] && !stat(p, &s); }
static void tecla(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  assert(celb_evento(&e));
}
static int espera_falha(void) {
  for (int i = 0; i < 200 && logcloud_estado() != LOGCLOUD_FALHOU; i++) usleep(10000);
  return logcloud_estado() == LOGCLOUD_FALHOU;
}
int main(void) {
  char src[1024], snap[1024], m[128];
  snprintf(src, sizeof src, "%s/nuvio-celb-log-XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
  int fd = mkstemp(src); assert(fd >= 0);
  const char *fixture = "[app] start=1\n[net] password=DO_NOT_EXPORT\n";
  assert(write(fd, fixture, strlen(fixture)) == (ssize_t)strlen(fixture)); close(fd);
  log_source = src;

  /* Off (default): the card never opens, no snapshot, no server. */
  qr_switch = 0;
  assert(!celb_abrir(CELB_LOG, "Registro") && !celb_aberto() && !logSnapshot[0]);
  assert(celular_estado() != CEL_ESPERANDO);

  /* On: frozen snapshot + local page; nothing is uploaded on open. */
  qr_switch = 1;
  assert(celb_abrir(CELB_LOG, "Registro") && celb_dono() == CELB_LOG);
  assert(existe(logSnapshot) && celular_estado() == CEL_ESPERANDO && !motivoLog[0]);
  assert(logcloud_estado() == LOGCLOUD_PARADO && !nuvem);
  assert(qrTextura(celular_url()) && qr_uploads == 1 && qr_allocations == 1);
  assert(qrTextura(celular_url()) && qr_uploads == 1); /* Same URL reuses pixels. */
  assert(qrTextura("https://fixture.invalid/v1/loglink/0123456789abcdef0123456789abcdef"));
  assert(qr_uploads == 2 && qr_allocations == 1); /* New URL replaces pixels. */
  snprintf(snap, sizeof snap, "%s", logSnapshot);
  /* OK refreshes the snapshot at the same address; still no upload. */
  tecla(SDLK_RETURN);
  assert(existe(logSnapshot) && !existe(snap) && logcloud_estado() == LOGCLOUD_PARADO);
  snprintf(snap, sizeof snap, "%s", logSnapshot);

  /* Switching the setting off closes the open card and deletes the copy. */
  qr_switch = 0; celb_atualizar(0.016f);
  assert(!celb_aberto() && !existe(snap) && !logSnapshot[0] && celular_estado() != CEL_ESPERANDO);

  assert(qr_deletes == 1 && !texQr && !texQrDe[0]);

  /* RIGHT is the explicit cloud action; a failure says why, OK retries. */
  qr_switch = 1; assert(celb_abrir(CELB_LOG, "Registro"));
  tecla(SDLK_RIGHT);
  assert(nuvem && espera_falha());
  logcloud_motivo(m, sizeof m); assert(!strcmp(m, "no diagnostics server in this build"));
  tecla(SDLK_RETURN); assert(espera_falha());
  /* Back closes: card, server and snapshot gone, cloud state forgotten. */
  snprintf(snap, sizeof snap, "%s", logSnapshot);
  tecla(SDLK_ESCAPE);
  assert(!celb_aberto() && !existe(snap) && logcloud_estado() == LOGCLOUD_PARADO);

  /* Local open failure: a concrete reason, no placeholder address; OK then
   * (and only then) goes to the cloud. */
  log_source = "/nonexistent/nuvio.log";
  assert(celb_abrir(CELB_LOG, "Registro"));
  assert(!strcmp(motivoLog, "log snapshot failed") && celular_estado() != CEL_ESPERANDO && !celular_url()[0]);
  assert(logcloud_estado() == LOGCLOUD_PARADO);
  tecla(SDLK_RETURN); assert(nuvem && espera_falha());
  celb_fechar();
  unlink(src);
  puts("PASS: diagnostics QR switch gating, snapshot lifecycle, local failure reason, explicit cloud action");
  return 0;
}
