// #344: epg_faixa/xtepg_faixa devolviam o TOTAL de programas da janela mas so
// preenchiam out[] ate `cap`. A busca do guia (buscaFazer) declara ps[24] e lia
// ps[k] para k < total: canal com mais de 24 programas em 6 h estourava a pilha
// e o ponteiro `titulo` era lixo (SIGSEGV em nv_dobrar, teclado "pronto").
//
// Contrato: com `out`, o retorno e min(total, cap); sem `out`, e o total.
// Roda com SANITIZE=1 (ASan): antes do conserto, stack-buffer-overflow.
#include "../src/xtepg.c"
#include "../src/epg.h"
#include "../src/rede.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// --- dubles ------------------------------------------------------------------
char *rede_baixar_bin(const char *u, int s, long *n) { (void)u; (void)s; (void)n; return 0; }
char *rede_baixar_bin_medido_controle(const char *url, int segundos, const char *const *cab,
                                      const RedeControle *c, long *tam, RedeMedida *m) {
  (void)url; (void)segundos; (void)cab; (void)c; (void)tam; if (m) memset(m, 0, sizeof *m); return 0;
}
char *dados_ler(const char *n)        { (void)n; return 0; }
int   dados_gravar_leve(const char *n, const char *c) { (void)n;(void)c; return 1; }
void  dados_marcar_sujo(int l)        { (void)l; }
void  dados_fs_travar(void)           {}
void  dados_fs_liberar(void)          {}
char *dados_caminho(char *d, unsigned t, const char *n) { (void)d;(void)t;(void)n; return 0; }
int   dados_apagar(const char *n)     { (void)n; return 0; }
const char *dados_dir(void)           { return "/tmp"; }
int xtream_e_id(const char *id) { return id && !strncmp(id, "xtream:", 7); }
int xtream_ultimo_retry_after(void) { return 0; }
int xtream_epg_curto(const char *id, XtreamProg *out, int cap, int *st) {
  time_t t = time(NULL) - 60; int i;
  (void)id; *st = 200;
  for (i = 0; i < cap; i++) {
    out[i].ini = t + i * 600; out[i].fim = t + (i + 1) * 600;
    snprintf(out[i].titulo, sizeof out[i].titulo, "Programa %d", i);
  }
  return cap;
}

// O laco de buscaFazer (src/guia.c) com o buffer de 24 e a leitura de titulo.
static int buscaPrograma(int (*faixa)(time_t, time_t, EpgProg *, int), time_t agora, const char *alvo) {
  EpgProg ps[24];
  int n = faixa(agora, agora + 6 * 3600, ps, 24), k, achou = 0;
  for (k = 0; k < n; k++)
    if (ps[k].titulo && strstr(ps[k].titulo, alvo)) achou++;
  return achou;
}
static int faixaXt(time_t de, time_t ate, EpgProg *o, int cap) { return xtepg_faixa("xtream:1", de, ate, o, cap); }

int main(void) {
  time_t agora = time(NULL);
  EpgProg ps[5];
  int k, e;
  // XMLTV: 1 canal, 40 programas de 10 min.
  { char *xml = malloc(40 * 200 + 400), *p; size_t o;
    p = xml;
    o = (size_t)sprintf(p, "<?xml version=\"1.0\"?><tv><channel id=\"Cheio.br\"><display-name>Cheio</display-name></channel>");
    for (k = 0; k < 40; k++) {
      char a[24], b[24]; time_t i0 = agora - 60 + k * 600, i1 = i0 + 600;
      strftime(a, sizeof a, "%Y%m%d%H%M%S +0000", gmtime(&i0));
      strftime(b, sizeof b, "%Y%m%d%H%M%S +0000", gmtime(&i1));
      o += (size_t)sprintf(p + o, "<programme channel=\"Cheio.br\" start=\"%s\" stop=\"%s\"><title>Show %d</title></programme>", a, b, k);
    }
    strcat(p, "</tv>");
    e = epg_xml_processar(xml);
    assert(e == 41 || e > 0);
    free(xml); }
  { int g = epg_match_id("Cheio.br"); EpgProg q[24]; int n, achou = 0;
    assert(g >= 0);
    // o laco da busca (ps[24]) nao le alem do buffer
    { EpgProg ps24[24]; int m = epg_faixa(g, agora, agora + 6 * 3600, ps24, 24), z, c = 0;
      for (z = 0; z < m; z++) if (ps24[z].titulo && strstr(ps24[z].titulo, "Show")) c++;
      assert(c == 24); }
    assert(epg_faixa(g, agora, agora + 6 * 3600, NULL, 0) == 37);                 // contagem (6 h = 36 + 1 que atravessa o inicio)
    assert(epg_faixa(g, agora, agora + 6 * 3600, q, 24) == 24);                   // cap
    assert(epg_faixa(g, agora, agora + 6 * 3600, q, 0) == 0);
    n = epg_faixa(g, agora, agora + 6 * 3600, q, 24);
    for (k = 0; k < n; k++) if (strstr(q[k].titulo, "Show 23")) achou++;
    assert(achou == 1);
    // lotes por POSICAO (epg_faixa_desde), como faz buscaFazer
    { EpgProg lote[24]; int r, achou30 = 0, total = 0;
      for (r = 0; r < 16; r++) {
        int m = epg_faixa_desde(g, agora, agora + 6 * 3600, total, lote, 24), z;
        for (z = 0; z < m; z++) if (strstr(lote[z].titulo, "Show 30")) achou30++;
        total += m;
        if (m < 24) break;
      }
      assert(total == 37 && achou30 == 1); } }
  puts("epg_faixa: retorno limitado ao cap; programa #30 achado em lotes");
  // Xtream: XE_PROG (12) programas de 10 min guardados.
  xtepg_querer("xtream:1");
  for (k = 0; k < 400 && !xtepg_tem("xtream:1"); k++) { usleep(10000); xtepg_passo(); }
  assert(xtepg_tem("xtream:1"));
  assert(xtepg_faixa("xtream:1", agora, agora + 6 * 3600, NULL, 0) == XE_PROG);   // contagem
  assert(xtepg_faixa("xtream:1", agora, agora + 6 * 3600, ps, 5) == 5);           // cap
  assert(xtepg_faixa("xtream:1", agora, agora + 6 * 3600, ps, 0) == 0);
  assert(buscaPrograma(faixaXt, agora, "Programa 5") == 1);
  assert(buscaPrograma(faixaXt, agora, "Programa 11") == 1);
  puts("xtepg_faixa: retorno limitado ao cap");

  puts("PASS: guiabusca_faixa");
  return 0;
}
