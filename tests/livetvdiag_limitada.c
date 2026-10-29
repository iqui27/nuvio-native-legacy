#include "../src/livetvdiag.c"
#include <assert.h>

static int requests, speed, stopped, limited = 1, status = 403;
int xtream_conexoes_limitadas(void) { return limited; }
int xtream_conta_ler(XtreamConta *c) { c->valido = 1; return 1; }
int xtream_url_formato(const char *id, const char *ext, char *url, unsigned n) {
  (void)id; snprintf(url, n, "http://fixture/%s", ext); return 1;
}
int xtream_formatos_canal(const char *id, const char *ext[2]) {
  (void)id; ext[0] = "m3u8"; ext[1] = "ts"; return 2;
}
int ajustes_livetv_resolucao(void) { return 0; }
int addons_consultar(const char *id, const char *tipo, const char *base, int fios,
                     int (*cancelado)(void *), void *ctx, Stream **saida) {
  (void)id; (void)tipo; (void)base; (void)fios; (void)cancelado; (void)ctx; *saida = NULL; return 0;
}
const char *i18n(const char *s) { return s; }
Uint32 SDL_GetTicks(void) { return 100; }
char *rede_baixar_trecho_st(const char *url, int segundos, long long ini, long long fim,
                           long *tam, int *http, int *erro, char *final, unsigned nf) {
  (void)url; (void)segundos; (void)ini; (void)fim; (void)final; (void)nf;
  assert(stopped); requests++; *http = status; *erro = 0; *tam = 0;
  if (status == 200 && strstr(url, "/ts")) {
    unsigned char *body = calloc(1, 188 * 3 + 1);
    assert(body);
    for (int i = 0; i < 3; i++) { body[i * 188] = 0x47; body[i * 188 + 3] = 0x10; }
    *tam = 188 * 3; return (char *)body;
  }
  return NULL;
}
char *rede_baixar_bin_medido_controle(const char *url, int segundos, const char *const *cabs,
                                      const RedeControle *ctl, long *tam, RedeMedida *md) {
  (void)url; (void)segundos; (void)cabs; (void)ctl;
  assert(stopped); requests++; *tam = 0; md->status = status; return NULL;
}
int rede_medir_vazao(const char *url, const char *const *cab, int segundos, long inicio,
                     long long maxBytes, volatile int *cancelado, int *kbps, int nMax,
                     RedeVazao *res, char *final, unsigned tamFinal) {
  (void)url; (void)cab; (void)segundos; (void)inicio; (void)maxBytes; (void)cancelado;
  (void)kbps; (void)nMax; (void)res; (void)final; (void)tamFinal;
  speed++; return 0;
}
int ajustes_idioma(void) { return 0; }
int player_mini_ativo(void) { return 1; }
void player_fechar_mini(void) { stopped |= 1; }
void player_encerrar(void) { stopped |= 2; }
void video_parar(void) { stopped |= 4; }
void proxyts_parar(void) { stopped |= 8; }
int xtream_configurado(void) { return 1; }
int xtream_e_id(const char *id) { (void)id; return 1; }
int ajustes_livetv_formato(void) { return 0; }
unsigned ajustes_livetv_espera_ms(void) { return 15000; }
int guia_canais_para_teste(GuiaVariante *out, char bases[][600], int max, char *grupo, size_t ng) {
  (void)out; (void)bases; (void)max; (void)grupo; (void)ng; return 0;
}
void vazao_fmt_mbps(char *dst, size_t n, int kbps, char sep) {
  (void)sep; snprintf(dst, n, "%d", kbps / 1000);
}

int main(void) {
  char res[180], det[180]; int cor;
  const int codes[] = {403, 458, 509};
  livetvdiag_iniciar();
  assert(stopped == 15);
  for (unsigned i = 0; i < sizeof codes / sizeof codes[0]; i++) {
    memset(&L, 0, sizeof L); L.xtConfig = 1; L.n = 1; L.it[0].xt = 1;
    status = codes[i]; requests = 0; stopped = 1;
    fioRede(&L.it[0]);
    assert(requests >= 2);
    L.it[0].pronto = 1; L.estado = E_PRONTO;
    textoResultado(&L.it[0], res, sizeof res, det, sizeof det, &cor);
    assert(!strcmp(res, "Esta linha está em uso em outra tela"));
  }
  memset(&L.it[0], 0, sizeof L.it[0]); L.it[0].xt = 1; L.it[0].pronto = 1;
  textoResultado(&L.it[0], res, sizeof res, det, sizeof det, &cor);
  assert(strcmp(res, "O provedor não respondeu"));
  L.it[0].f[F_HLS].servido = 1;
  assert(proximoFormato(&L.it[0]) == F_HLS);
  memset(&L, 0, sizeof L); L.it[0].xt = 1; status = 200;
  fioRede(&L.it[0]);
  assert(L.it[0].f[F_TS].servido);
  assert(!speed);
  // A slow HLS sample must not lower resolution while the account is at or over
  // its limit, or when any probe reports explicit throttling.
  memset(&L, 0, sizeof L);
  L.redeMedida = L.kbpsDoSegmento = 1; L.kbps = 5300;
  recomendar();
  assert(L.rec.resolucao == 4);
  L.conta.valido = 1; L.conta.maxConexoes = 1; L.conta.conexoes = 2;
  recomendar();
  assert(L.rec.resolucao == ajustes_livetv_resolucao());
  L.conta.conexoes = L.conta.maxConexoes;
  L.n = 1; L.it[0].f[F_HLS].http = 200;
  recomendar();
  assert(L.vazaoIncerta);
  assert(L.rec.resolucao == ajustes_livetv_resolucao());
  L.conta.conexoes = 0; L.n = 1;
  const int throttled[] = {429, 458, 509};
  for (unsigned i = 0; i < sizeof throttled / sizeof throttled[0]; i++) {
    L.it[0].f[F_TS].http = throttled[i];
    recomendar();
    assert(L.rec.resolucao == ajustes_livetv_resolucao());
  }
  puts("PASS: limited diagnostic requests, busy-line results, skipped result, playback selection, unreliable speed");
  return 0;
}
