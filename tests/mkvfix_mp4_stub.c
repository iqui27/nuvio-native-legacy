#include "mkvfix.h"
#include <stdio.h>
#include <string.h>

static int estado = MKVFIX_NADA, ocultas;

void mkvfix_sondar(const char *u, const char *c, const char *o) {
  (void)c; (void)o;
  if (strstr(u, ".mp4")) return;
  estado = MKVFIX_SONDANDO;
}
void mkvfix_sondar_relay(const char *u, const char *c, const char *o) { mkvfix_sondar(u, c, o); }
void mkvfix_sondar_arquivo(const char *u, const char *c, int usenet, int item, int file) {
  (void)c; (void)usenet; (void)item; (void)file; mkvfix_sondar(u, NULL, NULL);
}
void mkvfix_sondar_arquivo_relay(const char *u, const char *c, int usenet, int item, int file) {
  mkvfix_sondar_arquivo(u, c, usenet, item, file);
}
void mkvfix_sondar_relay_tls(const char *u, const char *c, const char *o, int usenet, int item, int file) {
  (void)usenet; (void)item; (void)file; mkvfix_sondar(u, c, o);
}
void mkvfix_arquivo_atualizar(const char *u, int usenet, int item, int file) {
  (void)u; (void)usenet; (void)item; (void)file;
}
int mkvfix_relay(void) { return 0; }
int mkvfix_estado(void) { return estado; }
int mkvfix_ocultas(void) { return ocultas; }
int mkvfix_url(char *out, size_t n) {
  if (!ocultas || !out || n < 24) return 0;
  snprintf(out, n, "http://127.0.0.1:7/proxy.mp4");
  return 1;
}
int mkvfix_e_url(const char *u) { return u && strstr(u, "127.0.0.1") != NULL; }
void mkvfix_parar(void) { estado = MKVFIX_NADA; ocultas = 0; }
void mkvfix_mp4_stub_ocultar(int n) { ocultas = n; estado = MKVFIX_NORMAL; }
void mkvfix_mp4_stub_ocultas(int n) { ocultas = n; }
void mkvfix_mp4_stub_estado(int e) { estado = e; }
