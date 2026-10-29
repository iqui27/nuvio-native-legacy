#include "stub_perfisessao.h"
// #316: finished or empty schedules must stop showing the loading state.
#include "../src/guia.c"
#include "../src/xtepg.c"
#include <assert.h>

static int xmlEstado = EPG_PRONTO, xmlMatch = -1;
int epg_canal_estado(const char *id) { (void)id;return xmlEstado; }
int epg_match_canal(const char *id,const char *epgId,const char *nome) { (void)id;(void)epgId;(void)nome;return xmlMatch; }
int epg_estado(void) { return xmlEstado; }
int epg_fonte_estado(const char *chave) {(void)chave;return xmlEstado;}
int epg_match(const char *nome) { (void)nome; return xmlMatch; }
int epg_match_id(const char *id) { (void)id; return xmlMatch; }
int epg_casar(const char *f, const char *id, const char *tv, const char *nm, int *m) {
  (void)f; (void)id; (void)tv; (void)nm; *m = 0; return xmlMatch;
}
int epg_achar(const char *f, const char *id) { (void)f; (void)id; return xmlMatch; }
int perfis_ativo(void) { return 1; }
char *dados_ler(const char *n) { (void)n; return NULL; }
int xtream_e_id(const char *id) { return id && !strncmp(id, "xtream:", 7); }
int xtream_ultimo_retry_after(void) { return 0; }
int xtream_epg_curto(const char *id, XtreamProg *out, int cap, int *st) {
  (void)id; (void)out; (void)cap; *st = 200; return 0;
}

int main(void) {
  GCanal c = {0};
  c.id = "addon:1"; c.nome = c.epgId = c.tvgNome = ""; c.epg = -1;
  assert(gEstadoGrade(&c) == -2); // XMLTV finished, no matching schedule.
  xmlEstado = EPG_FALHOU; c.epg = -1;
  assert(gEstadoGrade(&c) == -3);
  xmlEstado = EPG_BAIXANDO; c.epg = -1;
  assert(gEstadoGrade(&c) == -1);
  xmlEstado = EPG_PRONTO; xmlMatch = 5; c.epg = -1;
  assert(gEstadoGrade(&c) == 0);

  xmlMatch = -1; c.id = "xtream:1"; c.epg = -1;
  // Keep requests queued locally, then publish actual cache results.
  fioVivo = 1;
  xtepg_querer(c.id);
  assert(gEstadoGrade(&c) == -1);
  xmlEstado = EPG_BAIXANDO; c.epg = -1;
  prontos[0] = (XeResp){.n = 0, .st = 200};
  strcpy(prontos[0].id, c.id); prontos[0].pedido = achar(c.id)->pedido; nProntos = 1; xtepg_passo();
  assert(gEstadoGrade(&c) == -2); // Panel has no programs.
  achar(c.id)->estado = XE_PEDIDO;
  prontos[0].n = -1; prontos[0].st = 503; nProntos = 1; xtepg_passo();
  assert(gEstadoGrade(&c) == -3); // Panel failed; no request still pending.
  achar(c.id)->estado = XE_PEDIDO;
  prontos[0].n = 1; prontos[0].st = 200;
  prontos[0].p[0].ini = time(NULL) - 60;
  prontos[0].p[0].fim = time(NULL) + 3600;
  nProntos = 1; xtepg_passo();
  assert(gEstadoGrade(&c) == 0);
  puts("PASS: schedule loading ends after empty, failed or successful responses");
}
