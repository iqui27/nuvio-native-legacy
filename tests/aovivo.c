// Canal ao vivo (aovivo.h): a conta que o OSD e o zapping usam.
//   - ordem do zap: volta nas pontas, deslocamento negativo/grande, lista vazia;
//   - debounce de 600 ms: soma toques, espera o ultimo, cancela +1/-1;
//   - agora / a seguir: cobertura, buraco, grade sobreposta, sem grade;
//   - o mesmo, de ponta a ponta, sobre uma grade XMLTV de molde (epg.c).
#include "stub_perfisessao.h"
#include <assert.h>
#include "nvtmp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../src/aovivo.h"
#include "../src/rede.h"

// dubles do que epg.c e aovivo.c referenciam e o teste nao exercita
char *rede_baixar_bin(const char *u, int s, long *n) { (void)u; (void)s; (void)n; return 0; }
int rede_baixar_fluxo(const char *u, int s, const char *const *c, int (*cb)(const char *, size_t, void *), void *x,
                      long long m, int *st, long long *b) { (void)u; (void)s; (void)c; (void)cb; (void)x; (void)m; (void)st; (void)b; return 0; }
char *rede_baixar_bin_medido_controle(const char *url, int segundos, const char *const *cab,
                                      const RedeControle *c, long *tam, RedeMedida *m) {
  (void)url; (void)segundos; (void)cab; (void)c; (void)tam;
  if (m) { memset(m, 0, sizeof *m); }
  return NULL;
}
char *dados_ler(const char *n)        { (void)n; return 0; }
int   dados_gravar_leve(const char *n, const char *c) { (void)n;(void)c; return 1; }
void  dados_marcar_sujo(int l)        { (void)l; }
void  dados_fs_travar(void)           {}
void  dados_fs_liberar(void)          {}
char *dados_caminho(char *d, unsigned t, const char *n) { (void)d;(void)t;(void)n; return 0; }
int   dados_apagar(const char *n)     { (void)n; return 0; }
const char *dados_dir(void)           { return nvt(""); }
int xtream_e_id(const char *id) { (void)id; return 0; }
int xtepg_faixa(const char *id, time_t de, time_t ate, EpgProg *out, int cap) {
  (void)id; (void)de; (void)ate; (void)out; (void)cap; return 0;
}

static void ordem(void) {
  assert(aovivo_ordem(0, 0, 1) == -1);
  assert(aovivo_ordem(5, 0, 1) == 1);
  assert(aovivo_ordem(5, 4, 1) == 0);      // volta ao comeco
  assert(aovivo_ordem(5, 0, -1) == 4);     // volta ao fim
  assert(aovivo_ordem(5, 2, 0) == 2);      // recarregar = o proprio canal
  assert(aovivo_ordem(5, 1, -7) == 4);     // deslocamento maior que a lista
  assert(aovivo_ordem(5, 3, 12) == 0);
  assert(aovivo_ordem(5, -1, 1) == 1);     // canal que saiu da lista conta como 0
  assert(aovivo_ordem(5, 9, 1) == 1);
  assert(aovivo_ordem(1, 0, 3) == 0);
}

static void debounce(void) {
  AoVivoZap z = {0};
  int off = 99;
  assert(!aovivo_zap_pronto(&z, 1000, &off));            // nada pendente
  aovivo_zap_apertar(&z, 1, 1000);
  assert(z.pend == 1);
  assert(!aovivo_zap_pronto(&z, 1000 + AV_ZAP_MS - 1, &off));   // ainda dentro do prazo
  aovivo_zap_apertar(&z, 1, 1500);                        // novo toque empurra o relogio
  aovivo_zap_apertar(&z, 1, 1900);
  assert(z.pend == 3);
  assert(!aovivo_zap_pronto(&z, 1900 + AV_ZAP_MS - 1, &off));
  assert(aovivo_zap_pronto(&z, 1900 + AV_ZAP_MS, &off) && off == 3 && z.pend == 0);
  assert(!aovivo_zap_pronto(&z, 9999, &off));            // so dispara uma vez
  // CH+ e CH- que se cancelam: vence a zero, nao troca nada, e limpa o estado
  aovivo_zap_apertar(&z, 1, 5000);
  aovivo_zap_apertar(&z, -1, 5100);
  assert(z.pend == 0);
  assert(!aovivo_zap_pronto(&z, 9000, &off));
  // negativo
  aovivo_zap_apertar(&z, -1, 20000);
  aovivo_zap_apertar(&z, -1, 20010);
  assert(aovivo_zap_pronto(&z, 20010 + AV_ZAP_MS, &off) && off == -2);
  // tecla presa: o teto segura o deslocamento
  { int i; for (i = 0; i < 1000; i++) aovivo_zap_apertar(&z, 1, 30000 + (Uint32)i);
    assert(z.pend == AV_ZAP_MAX); }
  // o relogio que da a volta (Uint32) nao trava o prazo
  { AoVivoZap w = {0};
    aovivo_zap_apertar(&w, 1, 0xFFFFFF00u);
    assert(aovivo_zap_pronto(&w, 0xFFFFFF00u + AV_ZAP_MS, &off) && off == 1); }
}

static EpgProg P(time_t ini, time_t fim, const char *t) { EpgProg p; p.ini = ini; p.fim = fim; p.titulo = t; return p; }

static void selecao(void) {
  EpgProg l[4];
  int ag, px;
  l[0] = P(100, 200, "A"); l[1] = P(200, 300, "B"); l[2] = P(300, 400, "C");
  aovivo_selecionar(250, l, 3, &ag, &px);
  assert(ag == 1 && px == 2);
  aovivo_selecionar(200, l, 3, &ag, &px);                // o inicio pertence ao novo
  assert(ag == 1 && px == 2);
  aovivo_selecionar(399, l, 3, &ag, &px);                // ultimo programa: sem proximo
  assert(ag == 2 && px == -1);
  aovivo_selecionar(400, l, 3, &ag, &px);                // fim e exclusivo
  assert(ag == -1 && px == -1);
  aovivo_selecionar(50, l, 3, &ag, &px);                 // antes de tudo: so o proximo
  assert(ag == -1 && px == 0);
  aovivo_selecionar(50, l, 0, &ag, &px);                 // sem grade
  assert(ag == -1 && px == -1);
  // BURACO entre programas: nada no ar, o proximo e o que ainda vai comecar
  l[0] = P(100, 200, "A"); l[1] = P(260, 300, "B");
  aovivo_selecionar(230, l, 2, &ag, &px);
  assert(ag == -1 && px == 1);
  // grade SOBREPOSTA: vale o que comecou por ultimo; o proximo e o que comeca
  // quando ele acaba, nao o que ja estava correndo
  l[0] = P(100, 300, "Longo"); l[1] = P(150, 200, "Curto"); l[2] = P(300, 400, "Depois");
  aovivo_selecionar(160, l, 3, &ag, &px);
  assert(ag == 1 && px == 2);
}

static char *xmltv(void) {
  static char b[1024];
  time_t agora = time(NULL);
  char a0[32], a1[32], a2[32], a3[32];
  struct tm t;
  time_t v[4] = { agora - 30 * 60, agora + 30 * 60, agora + 90 * 60, agora + 150 * 60 };
  char *dst[4] = { a0, a1, a2, a3 };
  int i;
  for (i = 0; i < 4; i++) { gmtime_r(&v[i], &t); strftime(dst[i], 32, "%Y%m%d%H%M%S +0000", &t); }
  snprintf(b, sizeof b,
    "<?xml version=\"1.0\"?><tv><channel id=\"Teste.br\"><display-name>Canal Teste</display-name></channel>"
    "<programme start=\"%s\" stop=\"%s\" channel=\"Teste.br\"><title>Jornal</title></programme>"
    "<programme start=\"%s\" stop=\"%s\" channel=\"Teste.br\"><title>Novela</title></programme>"
    "<programme start=\"%s\" stop=\"%s\" channel=\"Teste.br\"><title>Filme</title></programme></tv>",
    a0, a1, a1, a2, a2, a3);
  return b;
}

static void grade(void) {
  AoVivoEpg e;
  int idx;
  char *x = strdup(xmltv());
  epg_teste_limpar();
  assert(epg_xml_processar(x) >= 1);
  free(x);
  idx = epg_match("Canal Teste");
  assert(idx >= 0);
  assert(aovivo_epg_montar(idx, NULL, time(NULL), &e));
  assert(e.temAgora && !strcmp(e.agoraTit, "Jornal"));
  assert(e.temProx && !strcmp(e.proxTit, "Novela"));
  assert(e.progresso > 0.45f && e.progresso < 0.55f);     // 30 de 60 min
  assert(e.agoraFim - e.agoraIni == 3600 && e.proxIni == e.agoraFim);
  // daqui a 1h: no ar a novela, a seguir o filme
  assert(aovivo_epg_montar(idx, NULL, time(NULL) + 3600, &e));
  assert(!strcmp(e.agoraTit, "Novela") && !strcmp(e.proxTit, "Filme"));
  // depois do fim da grade: nada no ar, sem proximo
  assert(!aovivo_epg_montar(idx, NULL, time(NULL) + 6 * 3600, &e));
  assert(!e.temAgora && !e.temProx);
  // sem canal na grade e sem Xtream: vazio, e nao quebra
  assert(!aovivo_epg_montar(-2, "cs:qualquer", time(NULL), &e));
  assert(!aovivo_epg_montar(-1, NULL, time(NULL), &e));
}

int main(void) {
  ordem(); debounce(); selecao(); grade();
  puts("aovivo: ok");
  return 0;
}
