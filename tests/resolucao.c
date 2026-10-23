// INTERFACE RESOLUTION (src/resolucao.h): the migration of the old setting and
// the 4K watch, plus the disk round trip through ajustes_dir.
//
// Owner, 06/10/2026: Automatic by default, never 720p on its own; 4K only when
// picked, falling back to 1080p when the GPU does not hold it.
#include "../src/ajustes.c"
#include "nvtmp.h"
#include <assert.h>
#include <unistd.h>

static void escrever(const char *caminho, const char *texto) {
  FILE *f = fopen(caminho, "w");
  assert(f);
  fputs(texto, f);
  fclose(f);
}

static int carregar(const char *dir, const char *texto) {
  char caminho[700];
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  escrever(caminho, texto);
  valor[AJ_RESOLUCAO] = RES_AUTO;
  dados_gravar(RES_ARQ_MIGRADA, "1\n");   // 2.0.2 one-time migration already done
  ajustes_dir(dir);
  return valor[AJ_RESOLUCAO];
}

int main(void) {
  char dir[NVT_MAX]; nvt_set(dir, sizeof dir, "/nuvio-resolucao-XXXXXX");
  ResVigia v;
  ResAuto a;
  int i;
  assert(mkdtemp(dir));
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir);

  // 1. Migration: old 0 (the old default, 1080p) is Automatic; 4K and 720p
  //    were explicit and stay.
  assert(res_migrar(0) == RES_AUTO);
  assert(res_migrar(1) == RES_4K);
  assert(res_migrar(2) == RES_720);
  assert(res_migrar(-1) == RES_AUTO && res_migrar(9) == RES_AUTO);
  puts("ok  res_migrar: 1080p antigo -> Automatica, 4K e 720p ficam");

  // 2. Factory default and the list order.
  assert(OPCOES[AJ_RESOLUCAO].n == RES_N);
  assert(!strcmp(V_RESOLUCAO[RES_AUTO], "Automática"));
  assert(!strcmp(V_RESOLUCAO[RES_4K], "4K (experimental)"));
  assert(!strcmp(V_RESOLUCAO[RES_720], "720p (leve)"));
  puts("ok  lista: Automatica primeiro, padrao de fabrica");
  assert(OPCOES[AJ_GPU_EFEITOS].n == 4);
  assert(!strcmp(V_GPU_EF[1], "Completos") && !strcmp(V_GPU_EF[2], "Leves"));
  assert(!strcmp(V_GPU_EF[3], "Mínimos") && !inativa(AJ_GPU_EFEITOS));
  int resRow = -1, effectsRow = -1;
  for (int row = 0; row < AJ_N_TELA; row++) if (TELA[row].tipo == IT_OPC) {
    if (TELA[row].op == AJ_RESOLUCAO) resRow = row;
    if (TELA[row].op == AJ_GPU_EFEITOS) effectsRow = row;
  }
  assert(resRow >= 0 && effectsRow == resRow + 1);
  // Legacy 720p + Automatic migrates once and persists to disk.
  assert(carregar(dir, "resolucaoUi 3\ngpuEfeitosLocal 0\n") == RES_720);
  assert(ajustes_gpu_efeitos() == 3);
  { char *m = dados_ler("efeitos720-205.txt"); assert(m); free(m); }
  valor[AJ_GPU_EFEITOS] = 0; ajustes_dir(dir);
  assert(ajustes_gpu_efeitos() == 3);
  valor[AJ_GPU_EFEITOS] = 1; assert(gravar());
  valor[AJ_GPU_EFEITOS] = 0; ajustes_dir(dir);
  assert(ajustes_gpu_efeitos() == 1);
  puts("ok  legacy 720p Automatic migrates once; later Full persists");
  for (int p = 0; p < 4; p++) {
    char saved[80]; snprintf(saved, sizeof saved, "resolucaoUi 3\ngpuEfeitosLocal %d\n", p);
    assert(carregar(dir, saved) == RES_720 && ajustes_gpu_efeitos() == p);
  }
  puts("ok  effects appended, visible beside resolution, saved 720p choices preserved");

  // 3. Disk: old key migrates; the new key wins over the old one.
  assert(carregar(dir, "resolucao_ui 0\n") == RES_AUTO && !ajustes_4k() && !ajustes_720p());
  assert(carregar(dir, "resolucao_ui 1\n") == RES_4K && ajustes_4k());
  assert(carregar(dir, "resolucao_ui 2\n") == RES_720 && ajustes_720p());
  assert(carregar(dir, "resolucao_ui 1\nresolucaoUi 1\n") == RES_1080 && ajustes_1080p() && !ajustes_4k());
  assert(carregar(dir, "resolucaoUi 0\nresolucao_ui 2\n") == RES_AUTO && !ajustes_1080p());
  assert(carregar(dir, "idioma 0\n") == RES_AUTO);
  puts("ok  ajustes.txt: resolucao_ui migra, resolucaoUi vence");
  dados_apagar("efeitos720-205.txt");
  assert(carregar(dir, "resolucao_ui 2\ngpuEfeitosLocal 0\n") == RES_720);
  assert(ajustes_gpu_efeitos() == 3);
  dados_apagar("efeitos720-205.txt");
  assert(carregar(dir, "resolucaoUi 1\ngpuEfeitosLocal 0\n") == RES_1080);
  assert(ajustes_gpu_efeitos() == 0);
  dados_apagar("efeitos720-205.txt");
  assert(carregar(dir, "resolucaoUi 3\ngpuEfeitosLocal 1\n") == RES_720);
  assert(ajustes_gpu_efeitos() == 1);
  dados_apagar("efeitos720-205.txt");
  { char caminho[700];
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
    assert(!unlink(caminho));
    ajustes_dir(dir); // New installs mark migration before any user choice.
    assert(carregar(dir, "resolucaoUi 3\ngpuEfeitosLocal 0\n") == RES_720);
    assert(ajustes_gpu_efeitos() == 0);
  }
  puts("ok  old resolution key migrates; 1080p, explicit Full and new choices stay");

  // 4. The 4K watch: 4 bad reports in a row (GPU clock) = fallback, once.
  memset(&v, 0, sizeof v);
  for (i = 0; i < 3; i++) assert(!res_vigia_amostra(&v, 20, 55.0, 1));
  assert(res_vigia_amostra(&v, 20, 55.0, 1));
  assert(!res_vigia_amostra(&v, 20, 55.0, 1));   // only once
  puts("ok  4K: gpu 55 ms por 4 relatorios -> recua uma vez");

  // 5. A good report resets the streak; invalid ones neither count nor reset.
  memset(&v, 0, sizeof v);
  for (i = 0; i < 3; i++) assert(!res_vigia_amostra(&v, 25, 40.0, 1));
  assert(!res_vigia_amostra(&v, 60, 14.0, 1));
  for (i = 0; i < 3; i++) assert(!res_vigia_amostra(&v, 25, 40.0, 1));
  assert(!res_vigia_amostra(&v, 10, 80.0, 0));   // player open: ignored
  assert(res_vigia_amostra(&v, 25, 40.0, 1));
  puts("ok  4K: relatorio bom zera, fora da interface nao conta");

  // 6. Without the GPU clock it goes by FPS; a GPU at 25 ms (40 fps) holds.
  memset(&v, 0, sizeof v);
  for (i = 0; i < 10; i++) assert(!res_vigia_amostra(&v, 45, 0.0, 1));
  for (i = 0; i < 10; i++) assert(!res_vigia_amostra(&v, 38, 25.0, 1));
  for (i = 0; i < 3; i++) assert(!res_vigia_amostra(&v, 30, 0.0, 1));
  assert(res_vigia_amostra(&v, 30, 0.0, 1));
  puts("ok  4K: sem relogio de GPU decide pelo FPS < 40");

  // 6b. One-time 2.0.2 migration: everyone (4K stored or not) goes to
  //     Automatic once; a choice made AFTER it is respected.
  { char caminho[700];
    dados_apagar(RES_ARQ_MIGRADA);
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
    escrever(caminho, "resolucaoUi 2\n");
    valor[AJ_RESOLUCAO] = RES_AUTO; ajustes_dir(dir);
    assert(valor[AJ_RESOLUCAO] == RES_AUTO && !ajustes_4k());
    { char *m = dados_ler(RES_ARQ_MIGRADA); assert(m); free(m); }
    valor[AJ_RESOLUCAO] = RES_4K; gravar();
    valor[AJ_RESOLUCAO] = RES_AUTO; ajustes_dir(dir);
    assert(valor[AJ_RESOLUCAO] == RES_4K && ajustes_4k()); }
  puts("ok  migracao unica 2.0.2: todos em Automatica, escolha depois fica");

  // 7. Automatic's memory: per app version.
  assert(res_auto_ler(NULL, "2.0.2") == RES_AUTO_SONDAR);
  assert(res_auto_ler("4k 2.0.2\n", "2.0.2") == RES_AUTO_4K);
  assert(res_auto_ler("1080 2.0.2\n", "2.0.2") == RES_AUTO_1080);
  assert(res_auto_ler("4k 2.0.1\n", "2.0.2") == RES_AUTO_SONDAR);
  assert(res_auto_ler("4k 2.0.22\n", "2.0.2") == RES_AUTO_SONDAR);
  assert(res_auto_ler("lixo", "2.0.2") == RES_AUTO_SONDAR);
  puts("ok  automatico: veredito por versao do app");

  // 8. Probe: two valid reports under 12 ms approve; slower ones reject.
  memset(&a, 0, sizeof a);
  assert(res_auto_amostra(&a, 60, 8.0, 0) == RES_AUTO_NADA && a.validas == 0);
  assert(res_auto_amostra(&a, 60, 8.0, 1) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 60, 9.5, 1) == RES_AUTO_APROVOU);
  memset(&a, 0, sizeof a);   // TCL: 55 ms -> at once
  assert(res_auto_amostra(&a, 14, 55.0, 1) == RES_AUTO_REBAIXOU);
  memset(&a, 0, sizeof a);   // 18 ms: not "plenty of headroom"
  assert(res_auto_amostra(&a, 50, 18.0, 1) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 50, 11.0, 1) == RES_AUTO_REBAIXOU);
  memset(&a, 0, sizeof a);   // no GPU clock: FPS decides
  assert(res_auto_amostra(&a, 60, 0.0, 1) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 58, 0.0, 1) == RES_AUTO_APROVOU);
  memset(&a, 0, sizeof a);
  assert(res_auto_amostra(&a, 45, 0.0, 1) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 45, 0.0, 1) == RES_AUTO_REBAIXOU);
  puts("ok  automatico: sonda aprova < 12 ms, recusa o resto");

  // 9. Approved 4K demotes after 3 reports above 25 ms in a row.
  memset(&a, 0, sizeof a); a.estado = RES_AUTO_4K;
  assert(res_auto_amostra(&a, 30, 30.0, 1) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 30, 30.0, 1) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 60, 10.0, 1) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 30, 30.0, 1) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 30, 30.0, 1) == RES_AUTO_NADA);
  assert(res_auto_amostra(&a, 30, 30.0, 0) == RES_AUTO_NADA);   // player: ignored
  assert(res_auto_amostra(&a, 30, 30.0, 1) == RES_AUTO_REBAIXOU);
  assert(res_auto_amostra(&a, 30, 30.0, 1) == RES_AUTO_NADA);   // only once
  puts("ok  automatico: 4K aprovado recua com > 25 ms por 3 relatorios");

  valor[AJ_RESOLUCAO] = RES_AUTO; assert(ajustes_res_auto());
  valor[AJ_RESOLUCAO] = RES_4K; assert(!ajustes_res_auto());
  valor[AJ_RESOLUCAO] = RES_AUTO;
  return 0;
}
