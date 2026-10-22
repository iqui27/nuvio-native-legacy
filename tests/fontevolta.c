// Regras da fonte guardada para o Retomar (fontevolta.h), sem rede: a sonda
// e trocada por uma falsa e o relogio e passado a mao.
#include "fontevolta.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int sondaOk, sondaChamadas;
static char sondaUrl[4096];
static int sondaFalsa(const char *url, const char *cab) {
  (void)cab; sondaChamadas++; snprintf(sondaUrl, sizeof sondaUrl, "%s", url); return sondaOk;
}
static int esperarConferencia(void) {
  int i, e = FV_CONFERINDO;
  for (i = 0; i < 400 && (e = fontevolta_conferencia()) == FV_CONFERINDO; i++) usleep(5000);
  return e;
}
static Stream fonte(const char *url) {
  Stream s; memset(&s, 0, sizeof s);
  snprintf(s.url, sizeof s.url, "%s", url);
  snprintf(s.cabecalhos, sizeof s.cabecalhos, "Referer: https://example.invalid/");
  s.dolbyVision = 1;
  return s;
}

int main(void) {
  Stream s = fonte("https://example.invalid/a.mkv"), g;
  const Uint32 t0 = 1000000u;
  const char *m;
  setvbuf(stdout, NULL, _IONBF, 0);

  // Vazio: nada a usar, cai na busca.
  assert(!fontevolta_pegar("tt1", "conta-a", 1, t0, &g));
  fontevolta_guardar("", "conta-a", 1, &s, 0, t0);
  fontevolta_guardar("tt1", "", 1, &s, 0, t0);
  assert(!fontevolta_pegar("tt1", "conta-a", 1, t0, &g));
  puts("ok sem entrada (ou sem alvo/conta) usa a busca");

  // Mesmo alvo, conta e perfil, dentro da validade: usa, com tudo que reabre.
  fontevolta_guardar("tt1:2:3", "conta-a", 1, &s, 5000, t0);
  memset(&g, 0, sizeof g);
  assert(fontevolta_pegar("tt1:2:3", "conta-a", 1, t0 + 60000, &g));
  assert(!strcmp(g.url, s.url) && !strcmp(g.cabecalhos, s.cabecalhos) && g.dolbyVision == 1);
  assert(fontevolta_pegar("tt1:2:3", "conta-a", 1, t0 + 60000, &g)); // nao consome
  assert(fontevolta_tem_url(s.url) && !fontevolta_tem_url("https://example.invalid/b"));
  puts("ok mesmo titulo/episodio, conta e perfil: fonte guardada");

  // Outro episodio: busca, mas a entrada fica para o Retomar do anterior.
  assert(!fontevolta_pegar("tt1:2:4", "conta-a", 1, t0 + 60000, &g));
  assert(!fontevolta_pegar("tt1", "conta-a", 1, t0 + 60000, &g));
  assert(fontevolta_pegar("tt1:2:3", "conta-a", 1, t0 + 60000, &g));
  puts("ok outro titulo/episodio usa a busca e nao apaga");

  // Validade a partir da EMISSAO do link (5 s antes de guardar).
  assert(fontevolta_pegar("tt1:2:3", "conta-a", 1, t0 - 5000 + FONTEVOLTA_VALIDADE_MS - 1, &g));
  assert(!fontevolta_pegar("tt1:2:3", "conta-a", 1, t0 - 5000 + FONTEVOLTA_VALIDADE_MS, &g));
  assert(!fontevolta_pegar("tt1:2:3", "conta-a", 1, t0 + 1, &g)); // vencida foi apagada
  fontevolta_guardar("tt1", "conta-a", 1, &s, 0, t0);
  assert(!fontevolta_pegar("tt1", "conta-a", 1, t0 - 1, &g));      // relogio para tras
  puts("ok validade de 3 h contada da emissao; vencida apaga");

  // Troca de perfil e de conta invalidam e apagam.
  fontevolta_guardar("tt1", "conta-a", 1, &s, 0, t0);
  assert(!fontevolta_pegar("tt1", "conta-a", 2, t0, &g));
  assert(!fontevolta_pegar("tt1", "conta-a", 1, t0, &g));
  fontevolta_guardar("tt1", "conta-a", 1, &s, 0, t0);
  assert(!fontevolta_pegar("tt1", "conta-b", 1, t0, &g));
  assert(!fontevolta_pegar("tt1", "conta-a", 1, t0, &g));
  fontevolta_guardar("tt1", "conta-a", 1, &s, 0, t0);
  fontevolta_esquecer("teste");
  assert(!fontevolta_pegar("tt1", "conta-a", 1, t0, &g));
  puts("ok troca de perfil/conta e esquecer invalidam");

  // Guardar de novo substitui (uma entrada so).
  fontevolta_guardar("tt1", "conta-a", 1, &s, 0, t0);
  { Stream s2 = fonte("https://example.invalid/b.mp4");
    fontevolta_guardar("tt2", "conta-a", 1, &s2, 0, t0); }
  assert(!fontevolta_pegar("tt1", "conta-a", 1, t0, &g));
  assert(fontevolta_pegar("tt2", "conta-a", 1, t0, &g) && strstr(g.url, "b.mp4"));
  puts("ok uma entrada so: a ultima sessao substitui");

  // Conferencia em paralelo, com sonda falsa.
  fontevolta_definir_sonda(sondaFalsa);
  sondaOk = 1; fontevolta_conferir(s.url, s.cabecalhos);
  assert(esperarConferencia() == FV_OK && sondaChamadas == 1 && !strcmp(sondaUrl, s.url));
  sondaOk = 0; fontevolta_conferir(s.url, s.cabecalhos);
  assert(esperarConferencia() == FV_FALHOU);
  puts("ok conferencia em fio proprio: 200 serve, 4xx/aviso nao");

  // O vigia.
  { FontevoltaSinais g0 = {0};
    g0.carregando = 1; g0.conferencia = FV_CONFERINDO; g0.desdeMs = 500;
    assert(fontevolta_decidir(&g0, &m) == FV_ESPERAR && !m);
    g0.conferencia = FV_FALHOU;
    assert(fontevolta_decidir(&g0, &m) == FV_RECUAR && !strcmp(m, "conferencia falhou"));
    g0.conferencia = FV_OK; g0.desdeMs = FONTEVOLTA_PRAZO_MS + 1;
    assert(fontevolta_decidir(&g0, &m) == FV_RECUAR && !strcmp(m, "prazo"));
    g0.quadro = 1;   // ja desenhou quadro: o prazo nao descarta (12/10)
    assert(fontevolta_decidir(&g0, &m) == FV_ESPERAR && !m);
    g0.quadro = 0;
    g0.desdeMs = 900; g0.falhou = 1;
    assert(fontevolta_decidir(&g0, &m) == FV_RECUAR && !strcmp(m, "erro do player"));
    g0.falhou = 0; g0.pronto = 1; g0.carregando = 0; g0.duracao = 30;
    assert(fontevolta_decidir(&g0, &m) == FV_RECUAR && !strcmp(m, "clipe curto"));
    g0.duracao = 5400; g0.conferencia = FV_CONFERINDO;
    assert(fontevolta_decidir(&g0, &m) == FV_ESPERAR);
    g0.conferencia = FV_FALHOU;   // tocando: a sonda nao derruba
    assert(fontevolta_decidir(&g0, &m) == FV_ABRIU);
    g0.conferencia = FV_OK;
    assert(fontevolta_decidir(&g0, &m) == FV_ABRIU);
    g0.conferencia = FV_NADA;     // sem fio para conferir: o player decide
    assert(fontevolta_decidir(&g0, &m) == FV_ABRIU);
  }
  puts("ok vigia: erro, clipe curto, sonda e prazo recuam; tocando vence a sonda");
  // Prazo da escolha manual: sem "pronto" (nenhum Prepared), so vence carregando
  // e depois do prazo; pronto, erro ja mostrado ou buffering pos-abertura nao.
  { FontevoltaSinais g1 = {0};
    g1.carregando = 1; g1.desdeMs = FONTE_MANUAL_PRAZO_MS;
    assert(!fontevolta_abertura_vencida(&g1, FONTE_MANUAL_PRAZO_MS));
    g1.desdeMs = FONTE_MANUAL_PRAZO_MS + 1;
    assert(fontevolta_abertura_vencida(&g1, FONTE_MANUAL_PRAZO_MS));
    g1.quadro = 1; assert(!fontevolta_abertura_vencida(&g1, FONTE_MANUAL_PRAZO_MS));
    g1.quadro = 0;
    g1.falhou = 1; assert(!fontevolta_abertura_vencida(&g1, FONTE_MANUAL_PRAZO_MS));
    g1.falhou = 0; g1.pronto = 1; assert(!fontevolta_abertura_vencida(&g1, FONTE_MANUAL_PRAZO_MS));
    g1.pronto = 0; g1.carregando = 0; assert(!fontevolta_abertura_vencida(&g1, FONTE_MANUAL_PRAZO_MS));
    assert(!fontevolta_abertura_vencida(NULL, 1));
  }
  puts("ok prazo manual: so sem pronto, carregando e vencido");
  puts("fontevolta: tudo ok");
  return 0;
}
