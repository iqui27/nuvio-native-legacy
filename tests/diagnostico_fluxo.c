// O FLUXO DA APLICACAO AUTOMATICA contra o cache de texturas de verdade:
// aplica -> reteste pior -> restaura; aplica -> reteste melhor -> mantem e
// grava; escolha manual de Ajustes respeitada; perfil lido do disco limitado
// pela RAM; Voltar no meio desfaz. Sem janela e sem rede: as medidas dos passes
// sao injetadas, e o que se confere e o estado do tex_cache depois.
//
// Inclui o .c para chegar nas funcoes static (mesma tecnica de texfila.c).
// No Mac nao ha /proc/meminfo: a RAM e "desconhecida", o que na tabela da LG
// da automatico 96 MB, teto 160, 4 fios e heroi 1920 (perfiltv.c).
#include "../src/diagnostico.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int orcamento(void) { int mb = 0; tex_orcamento_info(&mb, NULL, NULL, NULL); return mb; }

static void reset(DiagnosticoModo modo) {
  free(d.cfgAntes);
  memset(&d, 0, sizeof d);
  d.modo = modo;
}

static PtvMedida med(int ms, int falhas) {
  PtvMedida m;
  memset(&m, 0, sizeof m);
  m.artesMs = ms; m.falhas = falhas; m.prontas = 10 - falhas; m.piorQuadroMs = 20;
  return m;
}

int main(void) {
  char dir[] = "/tmp/nuvio-diag-fluxo-XXXXXX";
  char *cfg;
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(dir);
  assert(tex_iniciar(64));
  assert(orcamento() == 96 && tex_fios_rede() == 4 && tex_teto_heroi() == 1920);

  // 1. QUALIDADE, reteste PIOR: o candidato (160 MB) sai e o anterior volta.
  reset(DIAG_QUALIDADE);
  assert(aplicarCandidato() == 1);
  assert(orcamento() == 160 && atomic_load(&d.experimento) == 1);
  cfg = dados_ler("diagnostico-otimizacao.checkpoint");
  assert(cfg && strstr(cfg, "experiment_pending")); free(cfg);
  d.medAntes = med(1000, 0);
  d.medDepois = med(2000, 0);
  concluirComparacao();
  assert(d.aplicacao == DA_RESTAURADO_AUTO && d.motivo);
  assert(orcamento() == 96 && atomic_load(&d.experimento) == 0);
  assert(!dados_ler("diagnostico-otimizacao.checkpoint"));
  assert(!dados_ler("diagnostico-otimizacao.cfg"));
  puts("ok  aplicar -> reteste pior -> restaura sozinho (sem gravar perfil)");

  // 2. DESEMPENHO, reteste MELHOR: fica, e vai para o disco.
  reset(DIAG_DESEMPENHO);
  assert(aplicarCandidato() == 1);
  assert(tex_fios_rede() == 2 && tex_teto_heroi() == 1280 && orcamento() == 96);
  d.medAntes = med(1000, 0);
  d.medDepois = med(900, 0);
  concluirComparacao();
  assert(d.aplicacao == DA_MANTIDO);
  assert(tex_fios_rede() == 2 && tex_teto_heroi() == 1280);
  cfg = dados_ler("diagnostico-otimizacao.cfg");
  assert(cfg && strstr(cfg, "fios_rede=2") && strstr(cfg, "heroi=1280")); free(cfg);
  puts("ok  aplicar -> reteste melhor -> mantem e grava o perfil");

  // 3. Restaurar anterior (botao): volta ao que valia e apaga o perfil.
  restaurarManual();
  assert(d.aplicacao == DA_RESTAURADO_MANUAL);
  assert(tex_fios_rede() == 4 && tex_teto_heroi() == 1920);
  assert(!dados_ler("diagnostico-otimizacao.cfg"));
  puts("ok  restaurar anterior desfaz o perfil mantido");

  // 4. Uma falha a mais no reteste reprova mesmo mais rapido.
  reset(DIAG_DESEMPENHO);
  assert(aplicarCandidato() == 1);
  d.medAntes = med(1000, 0);
  d.medDepois = med(500, 1);
  concluirComparacao();
  assert(d.aplicacao == DA_RESTAURADO_AUTO && tex_fios_rede() == 4);
  puts("ok  falha a mais no reteste restaura");

  // 5. Voltar no meio: o experimento sai, uma vez so.
  reset(DIAG_QUALIDADE);
  assert(aplicarCandidato() == 1 && orcamento() == 160);
  assert(desfazerExperimento() == 1 && orcamento() == 96);
  assert(desfazerExperimento() == 0);
  puts("ok  cancelar no meio desfaz o candidato");

  // 6. ESCOLHA MANUAL em Ajustes (300 pedido, 160 e o teto sem MemTotal): o
  // candidato nao mexe no orcamento, so no resto.
  tex_definir_orcamento_mb(300);
  assert(orcamento() == 160);
  reset(DIAG_DESEMPENHO);
  assert(aplicarCandidato() == 1);
  assert(orcamento() == 160 && tex_fios_rede() == 2);
  assert(desfazerExperimento() == 1);
  tex_definir_orcamento_mb(0);
  assert(orcamento() == 96);
  puts("ok  memoria escolhida em Ajustes nao e tocada pelo otimizador");

  // 7. O candidato igual ao que vale nao aplica nada.
  reset(DIAG_QUALIDADE);
  tex_definir_orcamento_auto_mb(160);
  assert(aplicarCandidato() == 0 && d.aplicacao == DA_IGUAL);
  tex_definir_orcamento_auto_mb(0);
  puts("ok  candidato igual ao perfil atual: nada aplicado");

  // 8. ARRANQUE: perfil do disco acima do teto (TV trocada, arquivo antigo) e
  // limitado; checkpoint pendente (experimento interrompido) e descartado.
  dados_gravar("diagnostico-otimizacao.cfg", "versao=2\nmodo=qualidade\ntex_mb=999\nfios_rede=9\nheroi=3840\n");
  dados_gravar("diagnostico-otimizacao.checkpoint", "versao=2\nestado=experiment_pending\n");
  diagnostico_recuperar_checkpoint();
  assert(orcamento() == 160 && tex_fios_rede() == 4 && tex_teto_heroi() == 1920);
  assert(!dados_ler("diagnostico-otimizacao.checkpoint"));
  puts("ok  perfil do disco limitado pela RAM; experimento interrompido descartado");

  // 9. SUGESTAO DO DESTAQUE: o reteste pior desfaz a troca do ajuste.
  reset(DIAG_QUALIDADE);
  ajustes_definir_destaque(3, 1);          // TMDB, outra arte ligada
  d.sugEstado = DS_TESTANDO;
  d.sugFonteAntes = 3; d.sugDifAntes = 1;
  ajustes_definir_destaque(2, 1);          // o que a sugestao aplicou
  d.sugA = med(1000, 0);
  d.sugB = med(3000, 0);
  concluirSugestao();
  assert(d.sugEstado == DS_DESFEITA && ajustes_hero_fonte() == 3 && ajustes_hero_arte_diferente());
  d.sugEstado = DS_TESTANDO;
  ajustes_definir_destaque(0, 0);
  d.sugA = med(3000, 0);
  d.sugB = med(1000, 0);
  concluirSugestao();
  assert(d.sugEstado == DS_MANTIDA && ajustes_hero_fonte() == 0 && !ajustes_hero_arte_diferente());
  puts("ok  sugestao do destaque: pior desfaz, melhor mantem");

  tex_encerrar();
  { char cmd[128]; snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir); if (system(cmd)) {} }
  return 0;
}
