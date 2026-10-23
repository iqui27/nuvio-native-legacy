// "REUSAR O ULTIMO LINK" LIGADO (fontevolta.h): uma entrada por titulo/episodio
// e perfil, no disco, validade no relogio de parede; desligado, o de sempre
// (tests/fontevolta.c continua cobrindo esse modo). Disco falso em memoria.
#include "../src/fontevolta.c"
#include <assert.h>

static char disco[1 << 20];
static int temArquivo, apagou;
static char *ler(const char *n) {
  (void)n;
  return temArquivo ? strdup(disco) : NULL;
}
static int gravar(const char *n, const char *c) {
  (void)n;
  snprintf(disco, sizeof disco, "%s", c);
  temArquivo = 1;
  return 1;
}
static int apagarArq(const char *n) { (void)n; temArquivo = 0; disco[0] = 0; apagou++; return 1; }

static Stream fonte(const char *url) {
  Stream s;
  memset(&s, 0, sizeof s);
  snprintf(s.url, sizeof s.url, "%s", url);
  snprintf(s.rotulo, sizeof s.rotulo, "AIOStreams\n2160p");
  snprintf(s.provedor, sizeof s.provedor, "AIOStreams");
  snprintf(s.cabecalhos, sizeof s.cabecalhos, "Referer: https://a.invalido/\nUser-Agent: x\ty");
  snprintf(s.arquivo, sizeof s.arquivo, "Show.S01E01.2160p.WEB-DL-GRP.mkv");
  snprintf(s.bingeGroup, sizeof s.bingeGroup, "aio|2160p");
  s.altura = 2160; s.dolbyVision = 1; s.mp4 = 0; s.fileIdx = 3; s.tamanhoMB = 4200;
  return s;
}
static void arranqueFrio(void) { nEnt = 0; carregado = 0; }

int main(void) {
  Stream g, a = fonte("https://debrid.invalido/a\\b?x=1"), b = fonte("https://debrid.invalido/b");
  Uint32 t0 = 1000;
  int i;
  char alvo[64];

  fontevolta_definir_arquivo(ler, gravar, apagarArq);
  fontevolta_configurar(1, 24u * 3600u);

  strcpy(a.audioIdiomas, "en");
  strcpy(a.legendaIdiomas, "pt");
  strcpy(a.descricao, "Structured metadata only");
  a.filtroLido = 1;
  a.filtro.valores[FONTEFILTRO_LINGUAS] = fontefiltro_valores(FONTEFILTRO_LINGUAS, "en");
  a.filtro.tamanhoBytes = a.tamanhoBytes = 4404000000ULL;
  strcpy(a.filtro.grupo, "StructuredGroup");

  // Uma por episodio, e todas valem ao mesmo tempo.
  fontevolta_guardar("tt9:1:1", "conta-a", 1, &a, 2000, t0);
  fontevolta_guardar("tt9:1:2", "conta-a", 1, &b, 0, t0);
  assert(fontevolta_pegar("tt9:1:1", "conta-a", 1, t0, &g) && !strcmp(g.url, a.url));
  assert(fontevolta_pegar("tt9:1:2", "conta-a", 1, t0, &g) && !strcmp(g.url, b.url));
  assert(!fontevolta_pegar("tt9:1:3", "conta-a", 1, t0, &g));
  assert(fontevolta_tem_url(a.url) && fontevolta_tem_url(b.url));
  puts("ok uma entrada por episodio");

  // Sobrevive ao arranque, com cabecalhos (quebra de linha, TAB) e barra intactos.
  assert(temArquivo && strstr(disco, "debrid.invalido/a\\\\b?x") != NULL);   // a barra, escapada
  { int linhas = 0; const char *c; for (c = disco; *c; c++) linhas += *c == '\n';
    assert(linhas == 3); }   // cabecalho + 2: a quebra dos cabecalhos nao vira linha
  arranqueFrio();
  assert(fontevolta_pegar("tt9:1:1", "conta-a", 1, t0, &g));
  assert(!strcmp(g.url, a.url) && !strcmp(g.cabecalhos, a.cabecalhos) && !strcmp(g.rotulo, a.rotulo));
  assert(!strcmp(g.arquivo, a.arquivo) && !strcmp(g.bingeGroup, a.bingeGroup) && !strcmp(g.provedor, a.provedor));
  assert(g.altura == 2160 && g.dolbyVision == 1 && g.fileIdx == 3 && g.tamanhoMB == 4200);
  assert(g.filtroLido && !memcmp(&g.filtro, &a.filtro, sizeof a.filtro));
  assert(!strcmp(g.audioIdiomas, "en") && !strcmp(g.legendaIdiomas, "pt"));
  assert(g.tamanhoBytes == a.tamanhoBytes && !strcmp(g.descricao, a.descricao));
  { FonteFiltroConfig excluded = {0};
    excluded.excluir[FONTEFILTRO_LINGUAS] = fontefiltro_valores(FONTEFILTRO_LINGUAS, "en");
    assert(!fontefiltro_permite(&excluded, &g.filtro));
  }
  puts("ok sobrevive ao arranque, campos intactos");

  // Legacy entries cannot recover structured metadata; force fresh lookup.
  strstr(disco, "v2")[1] = '1';
  arranqueFrio();
  assert(!fontevolta_pegar("tt9:1:1", "conta-a", 1, t0, &g));
  fontevolta_guardar("tt9:1:1", "conta-a", 1, &a, 0, t0);
  fontevolta_guardar("tt9:1:2", "conta-a", 1, &b, 0, t0);

  // Outra conta / outro perfil nao veem, e nao apagam a entrada.
  assert(!fontevolta_pegar("tt9:1:1", "conta-b", 1, t0, &g));
  assert(!fontevolta_pegar("tt9:1:1", "conta-a", 2, t0, &g));
  assert(fontevolta_pegar("tt9:1:1", "conta-a", 1, t0, &g));
  // Trocar de perfil (fontevolta_esquecer) nao apaga nada quando ligado.
  fontevolta_esquecer("nova sessao de perfil");
  assert(fontevolta_pegar("tt9:1:1", "conta-a", 1, t0, &g));
  puts("ok por conta e perfil; troca de perfil nao apaga");

  // Validade no relogio de parede, contada da emissao do link.
  i = acharSemTrava("tt9:1:1", "conta-a", 1);
  ent[i].obtidoS = (long long)time(NULL) - 25LL * 3600;
  assert(!fontevolta_pegar("tt9:1:1", "conta-a", 1, t0, &g));
  assert(acharSemTrava("tt9:1:1", "conta-a", 1) < 0);   // vencida sai
  arranqueFrio();
  assert(!fontevolta_pegar("tt9:1:1", "conta-a", 1, t0, &g));   // e saiu do disco
  fontevolta_configurar(1, 48u * 3600u);
  fontevolta_guardar("tt9:1:1", "conta-a", 1, &a, 0, t0);
  i = acharSemTrava("tt9:1:1", "conta-a", 1);
  ent[i].obtidoS = (long long)time(NULL) - 25LL * 3600;
  assert(fontevolta_pegar("tt9:1:1", "conta-a", 1, t0, &g));    // 48 h: ainda vale
  ent[i].obtidoS = (long long)time(NULL) + 600;                  // relogio para tras
  assert(!fontevolta_pegar("tt9:1:1", "conta-a", 1, t0, &g));
  puts("ok validade do ajuste; relogio para tras recua para a busca");

  // Esquecer um alvo so apaga ele.
  fontevolta_guardar("tt9:1:1", "conta-a", 1, &a, 0, t0);
  fontevolta_esquecer_alvo("tt9:1:2", "conta-a", 1, "sessao falhou");
  assert(!fontevolta_pegar("tt9:1:2", "conta-a", 1, t0, &g));
  assert(fontevolta_pegar("tt9:1:1", "conta-a", 1, t0, &g));
  puts("ok esquecer um alvo");

  // Cheia: sai a mais antiga.
  for (i = 0; i < FONTEVOLTA_MAX + 10; i++) {
    snprintf(alvo, sizeof alvo, "tt8:1:%d", i + 1);
    fontevolta_guardar(alvo, "conta-a", 1, &b, 0, t0);
    ent[acharSemTrava(alvo, "conta-a", 1)].obtidoS = 1000000 + i;   // ordem de chegada
  }
  assert(nEnt == FONTEVOLTA_MAX);
  assert(acharSemTrava("tt8:1:1", "conta-a", 1) < 0);
  snprintf(alvo, sizeof alvo, "tt8:1:%d", FONTEVOLTA_MAX + 10);
  assert(acharSemTrava(alvo, "conta-a", 1) >= 0);
  puts("ok teto de entradas, sai a mais antiga");

  // Sair da conta apaga tudo, inclusive o arquivo.
  apagou = 0;
  fontevolta_esquecer_tudo();
  assert(!temArquivo && apagou == 1 && nEnt == 0);
  assert(!fontevolta_pegar("tt9:1:1", "conta-a", 1, t0, &g));
  puts("ok sair da conta apaga tudo");

  // Desligar: o arquivo some e volta o de sempre (uma, em memoria, ticks).
  fontevolta_guardar("tt9:1:1", "conta-a", 1, &a, 0, t0);
  assert(temArquivo);
  fontevolta_configurar(0, 24u * 3600u);
  assert(!temArquivo && ent == &umaSo && nEnt == 0);
  fontevolta_guardar("tt9:1:1", "conta-a", 1, &a, 0, t0);
  fontevolta_guardar("tt9:1:2", "conta-a", 1, &b, 0, t0);
  assert(!temArquivo);
  assert(!fontevolta_pegar("tt9:1:1", "conta-a", 1, t0, &g));   // so a ultima
  assert(fontevolta_pegar("tt9:1:2", "conta-a", 1, t0, &g));
  assert(!fontevolta_pegar("tt9:1:2", "conta-a", 1, t0 + FONTEVOLTA_VALIDADE_MS, &g));
  puts("ok desligado: o arquivo some e volta o de sempre");

  puts("fontevolta_persistir: tudo ok");
  return 0;
}
