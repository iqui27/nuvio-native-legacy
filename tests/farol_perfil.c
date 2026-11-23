#define _POSIX_C_SOURCE 200809L
#define FAROL_PERFIL_TESTE 1

#include "farolperfil.h"
#include "farolredator.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static uint64_t agora_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static void testar_filtro_modelo(void) {
  char out[FAROL_PERFIL_MODELO_MAX + 1];
  int rc;

  /* Modelos reais das 4 plataformas */
  const char *validos[] = {
    "OLED55C9PSA", "50NANO80ASA", "OLED65G4PSA", "43LM6300PSB",
    "QN55Q80BAGXZD", "UN50TU8000GXZD", "QA55LS03B", "QE55Q60T",
    "22_PONTUSM_QTV", "TCL 55C745", "TCL Smart TV Pro",
    "Google Chromecast", "NVIDIA SHIELD Android TV",
    "KD-55X90J", "XR-65A80J", "MiBox S 4K", "AFTMM", "TV_2.0", "A"
  };
  for (size_t i = 0; i < sizeof validos / sizeof *validos; i++) {
    rc = farolperfil_filtrar_modelo(validos[i], out);
    assert(rc == 1);
    assert(strcmp(out, validos[i]) == 0);
    assert(strlen(out) <= FAROL_PERFIL_MODELO_MAX);
  }

  /* Espacos nas pontas e multiplos espacos internos */
  assert(farolperfil_filtrar_modelo("   TCL   55C745   ", out) == 1);
  assert(strcmp(out, "TCL 55C745") == 0);

  /* Exatamente 32 bytes */
  assert(farolperfil_filtrar_modelo("ABCD1234EFGH5678IJKL9012MNOP3456", out) == 1);
  assert(strlen(out) == 32);

  /* Mais de 32 bytes limpos -> corta em <= 32 sem espaco final */
  assert(farolperfil_filtrar_modelo("Samsung Smart TV QN55Q80BAGXZD  ExtraModel", out) == 1);
  assert(strlen(out) <= 32);
  assert(out[strlen(out) - 1] != ' ');

  /* Chamada in-place (entrada == out) */
  strcpy(out, "  OLED55C9PSA  ");
  assert(farolperfil_filtrar_modelo(out, out) == 1);
  assert(strcmp(out, "OLED55C9PSA") == 0);

  /* Maliciosos, URLs, IPs, tokens, vazios, controles -> TODOS viram "?" */
  const char *invalidos[] = {
    NULL, "", "   ", "?", "-", "---", "...", "._ -",
    "https://evil.invalid/OLED55C9",
    "http://192.168.1.1/secret",
    "ftp://host/file",
    "OLED55C9 https://evil.invalid",
    "OLED55C9PSA                     https://evil.invalid/alem32",
    "www.evil.example",
    "OLED55C9 www.evil.example",
    "user@host.invalid",
    "user:pass@tv",
    "OLED55C9?token=segredo",
    "OLED55C9#frag",
    "OLED55%2Fsegredo",
    "/etc/passwd",
    "C:\\Windows\\System32",
    "OLED55\nFAKE",
    "OLED55\r\nFAKE",
    "OLED55\tTAB",
    "OLED55;rm -rf",
    "OLED55\"quoted\"",
    "Título com acento",
    "OLED..C9",
    "192.168.1.50",
    "10.0.0.1",
    "127.0.1",
    "0xc6.0x33.0x64.0x2a",
    "OLED55 192.168.1.1",
    "TV_192.168.1.1",
    "0123456789abcdef0123456789abcdef",
    "TV_0123456789ABCDEF0123456789AB",
    /* > 128 bytes: lixo / linha de log inteira */
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
  };
  for (size_t i = 0; i < sizeof invalidos / sizeof *invalidos; i++) {
    rc = farolperfil_filtrar_modelo(invalidos[i], out);
    assert(rc == 0);
    assert(strcmp(out, "?") == 0);
  }
  assert(farolperfil_filtrar_modelo("OLED55C9PSA", NULL) == 0);
}

static void testar_android_info(void) {
  FarolTvInfoAndroid a;

  /* Entradas reais do NuvioActivity.kt */
  assert(farolperfil_parse_android_info("TCL 55C745|34|14|2.1.0", &a) == 1);
  assert(strcmp(a.modelo, "TCL 55C745") == 0);
  assert(a.sdk == 34);
  assert(strcmp(a.so, "14") == 0);
  assert(a.so_maior == 14);
  assert(strcmp(a.versao, "2.1.0") == 0);

  assert(farolperfil_parse_android_info("Sony BRAVIA 4K VH2|30|11.0.1|2.0.8", &a) == 1);
  assert(strcmp(a.modelo, "Sony BRAVIA 4K VH2") == 0);
  assert(a.sdk == 30);
  assert(strcmp(a.so, "11.0.1") == 0);
  assert(a.so_maior == 11);
  assert(strcmp(a.versao, "2.0.8") == 0);

  assert(farolperfil_parse_android_info("Amazon AFTMM|28|9|2.1.0", &a) == 1);
  assert(strcmp(a.modelo, "Amazon AFTMM") == 0);
  assert(a.sdk == 28);
  assert(strcmp(a.so, "9") == 0);
  assert(a.so_maior == 9);
  assert(strcmp(a.versao, "2.1.0") == 0);

  /* Entradas truncadas */
  assert(farolperfil_parse_android_info("Google Chromecast|31", &a) == 1);
  assert(strcmp(a.modelo, "Google Chromecast") == 0);
  assert(a.sdk == 31);
  assert(strcmp(a.so, "?") == 0);
  assert(a.so_maior == -1);
  assert(strcmp(a.versao, "?") == 0);

  assert(farolperfil_parse_android_info("TCL 55C745", &a) == 1);
  assert(strcmp(a.modelo, "TCL 55C745") == 0);
  assert(a.sdk == -1);
  assert(strcmp(a.so, "?") == 0);
  assert(a.so_maior == -1);
  assert(strcmp(a.versao, "?") == 0);

  assert(farolperfil_parse_android_info("|34||", &a) == 1);
  assert(strcmp(a.modelo, "?") == 0);
  assert(a.sdk == 34);
  assert(strcmp(a.so, "?") == 0);
  assert(a.so_maior == -1);
  assert(strcmp(a.versao, "?") == 0);

  assert(farolperfil_parse_android_info("|||", &a) == 0);
  assert(strcmp(a.modelo, "?") == 0 && a.sdk == -1 && strcmp(a.so, "?") == 0 && a.so_maior == -1);
  assert(farolperfil_parse_android_info("", &a) == 0);
  assert(farolperfil_parse_android_info(NULL, &a) == 0);

  /* Maliciosas */
  assert(farolperfil_parse_android_info("https://evil.invalid/a?k=1|34|14|2.1.0", &a) == 1);
  assert(strcmp(a.modelo, "?") == 0);
  assert(a.sdk == 34 && strcmp(a.so, "14") == 0 && a.so_maior == 14 && strcmp(a.versao, "2.1.0") == 0);

  assert(farolperfil_parse_android_info("https://evil.invalid/a|99999999|14;rm|https://bad", &a) == 0);
  assert(strcmp(a.modelo, "?") == 0 && a.sdk == -1 && strcmp(a.so, "?") == 0 &&
         a.so_maior == -1 && strcmp(a.versao, "?") == 0);

  assert(farolperfil_parse_android_info("TCL 55C745|-5|1.2.3.4|1.2.3.4", &a) == 1);
  assert(strcmp(a.modelo, "TCL 55C745") == 0 && a.sdk == -1 &&
         strcmp(a.so, "?") == 0 && a.so_maior == -1 && strcmp(a.versao, "?") == 0);

  /* Quebra de linha nao injeta campo */
  assert(farolperfil_parse_android_info("TCL 55C745|34\n|14|2.1.0", &a) == 1);
  assert(a.sdk == 34 && strcmp(a.so, "?") == 0 && strcmp(a.versao, "?") == 0);
}

static void testar_tpk_tv(void) {
  FarolTvInfoTpk t;

  /* Entradas reais do tizen-tpk/Program.cs:411 */
  assert(farolperfil_parse_tpk_tv(
      "[tv] modelo=QN55Q80BAGXZD tizen=6.5 host=api9 dotnet=.NET 6.0.21 tela=1920x1080", &t) == 1);
  assert(strcmp(t.modelo, "QN55Q80BAGXZD") == 0);
  assert(strcmp(t.so, "6.5") == 0);
  assert(t.so_maior == 6);
  assert(t.tela_w == 1920 && t.tela_h == 1080);

  assert(farolperfil_parse_tpk_tv(
      "[tv] modelo=UN50TU8000GXZD tizen=5.5 host=tpk40 dotnet=.NET Core 2.0 tela=1920x1080", &t) == 1);
  assert(strcmp(t.modelo, "UN50TU8000GXZD") == 0);
  assert(strcmp(t.so, "5.5") == 0 && t.so_maior == 5);
  assert(t.tela_w == 1920 && t.tela_h == 1080);

  /* Modelo com espaco e prefixo de log multilinhas */
  assert(farolperfil_parse_tpk_tv(
      "[arranque] inicio\n"
      "[tv] decoder 4K: hevc=1\n"
      "0.4s [tv] modelo=Samsung QN55Q80B tizen=7.0 host=api11 dotnet=.NET 8.0 tela=3840x2160\n", &t) == 1);
  assert(strcmp(t.modelo, "Samsung QN55Q80B") == 0);
  assert(strcmp(t.so, "7.0") == 0 && t.so_maior == 7);
  assert(t.tela_w == 3840 && t.tela_h == 2160);

  /* Entradas truncadas */
  assert(farolperfil_parse_tpk_tv("[tv] modelo=QE55Q60T tizen=6", &t) == 1);
  assert(strcmp(t.modelo, "QE55Q60T") == 0 && strcmp(t.so, "6") == 0 && t.so_maior == 6);
  assert(t.tela_w == -1 && t.tela_h == -1);

  assert(farolperfil_parse_tpk_tv("[tv] modelo=QE55Q60T tizen= tela=1920x", &t) == 1);
  assert(strcmp(t.modelo, "QE55Q60T") == 0 && strcmp(t.so, "?") == 0 && t.so_maior == -1);
  assert(t.tela_w == -1 && t.tela_h == -1);

  assert(farolperfil_parse_tpk_tv("[tv] modelo=? tizen=? tela=-", &t) == 0);
  assert(strcmp(t.modelo, "?") == 0 && strcmp(t.so, "?") == 0 && t.so_maior == -1 &&
         t.tela_w == -1 && t.tela_h == -1);

  assert(farolperfil_parse_tpk_tv("[tv]", &t) == 0);
  assert(farolperfil_parse_tpk_tv("", &t) == 0);
  assert(farolperfil_parse_tpk_tv(NULL, &t) == 0);

  /* Maliciosas */
  assert(farolperfil_parse_tpk_tv(
      "[tv] modelo=https://evil.invalid/x?k=1 tizen=6.5 host=api9 tela=1920x1080", &t) == 1);
  assert(strcmp(t.modelo, "?") == 0);
  assert(strcmp(t.so, "6.5") == 0 && t.so_maior == 6 && t.tela_w == 1920 && t.tela_h == 1080);

  assert(farolperfil_parse_tpk_tv(
      "[tv] modelo=192.168.0.1 tizen=6.5/evil tela=0x0", &t) == 0);
  assert(strcmp(t.modelo, "?") == 0 && strcmp(t.so, "?") == 0 && t.so_maior == -1 &&
         t.tela_w == -1 && t.tela_h == -1);

  /* Borda de palavra: nao confunde xmodelo= nem foo[tv] */
  assert(farolperfil_parse_tpk_tv("foo[tv] modelo=QN55Q80B tizen=6.5", &t) == 0);
  assert(farolperfil_parse_tpk_tv("[tv] xmodelo=QN55Q80B notizen=6.5 xtela=1920x1080", &t) == 0);
}

static void testar_meminfo(void) {
  int tot = 0, liv = 0;

  /* Entrada real Linux / webOS / Android */
  const char *real =
    "MemTotal:        1915220 kB\n"
    "MemFree:          112340 kB\n"
    "MemAvailable:     217088 kB\n"
    "Buffers:           18432 kB\n";
  assert(farolperfil_parse_meminfo(real, &tot, &liv) == 1);
  assert(tot == 1870 && liv == 212);

  /* CRLF e sem quebra de linha no fim */
  assert(farolperfil_parse_meminfo("MemTotal: 4096000 kB\r\nMemAvailable: 1024000 kB", &tot, &liv) == 1);
  assert(tot == 4000 && liv == 1000);

  /* So MemTotal (kernel antigo sem MemAvailable) */
  assert(farolperfil_parse_meminfo("MemTotal: 1048576 kB\nMemFree: 65536 kB\n", &tot, &liv) == 1);
  assert(tot == 1024 && liv == -1);

  /* So MemAvailable (inclusive 0 kB em OOM) */
  assert(farolperfil_parse_meminfo("MemAvailable: 0 kB\n", &tot, &liv) == 1);
  assert(tot == -1 && liv == 0);

  /* Truncadas e maliciosas */
  assert(farolperfil_parse_meminfo("MemTotal: 1915220", &tot, &liv) == 0);
  assert(tot == -1 && liv == -1);
  assert(farolperfil_parse_meminfo("MemTotal: 0 kB\nMemAvailable: -10 kB\n", &tot, &liv) == 0);
  assert(tot == -1 && liv == -1);
  assert(farolperfil_parse_meminfo("FakeMemTotal: 2048000 kB\nMemTotal: 2048 kB extra\n", &tot, &liv) == 0);
  assert(tot == -1 && liv == -1);
  assert(farolperfil_parse_meminfo("MemTotal: 99999999999999999999999 kB\n", &tot, &liv) == 0);
  assert(tot == -1 && liv == -1);
  assert(farolperfil_parse_meminfo("MemTotal: 2048 MB\n", &tot, &liv) == 0);
  assert(farolperfil_parse_meminfo("", &tot, &liv) == 0);
  assert(farolperfil_parse_meminfo(NULL, &tot, &liv) == 0);

  /* Linha gigante de lixo antes de MemTotal real nao desloca o parser */
  char grande[1600];
  memset(grande, 'A', 1200);
  strcpy(grande + 1200, "\nMemTotal: 2097152 kB\nMemAvailable: 524288 kB\n");
  assert(farolperfil_parse_meminfo(grande, &tot, &liv) == 1);
  assert(tot == 2048 && liv == 512);
}

static void testar_wgt_webos_tela(void) {
  FarolTvInfoWgt w;
  char so[24];
  int maior = 0, tw = 0, th = 0;

  /* WASM */
  assert(farolperfil_parse_wgt_info("QN55Q80BAGXZD|6.5", &w) == 1);
  assert(strcmp(w.modelo, "QN55Q80BAGXZD") == 0 && strcmp(w.so, "6.5") == 0 && w.so_maior == 6);
  assert(farolperfil_parse_wgt_info("https://evil.invalid|6.5", &w) == 1);
  assert(strcmp(w.modelo, "?") == 0 && strcmp(w.so, "6.5") == 0 && w.so_maior == 6);
  assert(farolperfil_parse_wgt_info("QN55Q80B|", &w) == 1);
  assert(strcmp(w.modelo, "QN55Q80B") == 0 && strcmp(w.so, "?") == 0 && w.so_maior == -1);
  assert(farolperfil_parse_wgt_info("?|?", &w) == 0);
  assert(farolperfil_parse_wgt_info(NULL, &w) == 0);

  /* webOS nyx + starfish */
  assert(farolperfil_parse_webos_nyx("{\"webos_release\":\"4.9.7\",\"webos_build_id\":\"123\"}", so, &maior) == 1);
  assert(strcmp(so, "4.9.7") == 0 && maior == 4);
  assert(farolperfil_parse_webos_nyx("{\"webos_release\":\"6.3.2-42\"}", so, &maior) == 1);
  assert(strcmp(so, "6.3.2") == 0 && maior == 6);
  assert(farolperfil_parse_webos_nyx("{\"webos_release\":\"4.9", so, &maior) == 0);
  assert(strcmp(so, "?") == 0 && maior == -1);
  assert(farolperfil_parse_webos_nyx("{\"webos_release\":\"https://bad\"}", so, &maior) == 0);

  assert(farolperfil_parse_webos_starfish("Starfish release 4.9.7 (goldilocks2)\n", so, &maior) == 1);
  assert(strcmp(so, "4.9.7") == 0 && maior == 4);
  assert(farolperfil_parse_webos_starfish("prerelease 9.1\n", so, &maior) == 0);
  assert(strcmp(so, "?") == 0 && maior == -1);

  /* Tela */
  assert(farolperfil_parse_tela("3840x2160", &tw, &th) == 1 && tw == 3840 && th == 2160);
  assert(farolperfil_parse_tela("1920X1080", &tw, &th) == 1 && tw == 1920 && th == 1080);
  assert(farolperfil_parse_tela("0x1080", &tw, &th) == 0 && tw == -1 && th == -1);
  assert(farolperfil_parse_tela("1920x", &tw, &th) == 0);
  assert(farolperfil_parse_tela("1920x1080extra", &tw, &th) == 0);
  assert(farolperfil_parse_tela("99999x1080", &tw, &th) == 0);
}

/* --- Teste de coleta assincrona, nao-bloqueio, cache unico e concorrencia - */

static _Atomic int gancho_entrou;
static _Atomic int gancho_liberar;

static void gancho_lento(void) {
  atomic_store(&gancho_entrou, 1);
  struct timespec req = { 0, 2000000L }; /* 2 ms */
  while (!atomic_load(&gancho_liberar)) nanosleep(&req, NULL);
}

static void escrever_arq(const char *path, const char *conteudo) {
  FILE *f = fopen(path, "wb");
  assert(f != NULL);
  assert(fwrite(conteudo, 1, strlen(conteudo), f) == strlen(conteudo));
  fclose(f);
}

static void *fio_leitor_stress(void *arg) {
  int id = *(int *)arg;
  for (int i = 0; i < 400; i++) {
    FarolPerfil p;
    farolperfil_iniciar();
    (void)farolperfil_obter(&p);
    (void)farolperfil_pronto();
    if ((i + id) % 37 == 0) {
      farolperfil_definir_modelo((i & 1) ? "OLED55C9PSA" : "QN55Q80BAGXZD");
    }
    if ((i + id) % 53 == 0) {
      FarolPerfilTela tl = { 3840, 2160, 1, 1, 1, -1 };
      farolperfil_definir_tela(&tl);
    }
    if ((i + id) % 71 == 0) {
      FarolJson j;
      char buf[4096];
      assert(farolredator_abrir(&j, buf, sizeof buf));
      assert(farolperfil_escrever(&j, &p));
      assert(farolredator_fechar(&j));
      assert(j.campos_recusados == 0);
    }
  }
  return NULL;
}

static void testar_assincrono_cache_e_tsan(const char *dir) {
  char p_mem[320], p_tpk[320], p_nyx[320];
  long pid = (long)getpid();
  snprintf(p_mem, sizeof p_mem, "%s/nuvio-fp-%ld-meminfo", dir, pid);
  snprintf(p_tpk, sizeof p_tpk, "%s/nuvio-fp-%ld-tpk.log", dir, pid);
  snprintf(p_nyx, sizeof p_nyx, "%s/nuvio-fp-%ld-os_info.json", dir, pid);

  escrever_arq(p_mem, "MemTotal: 1915220 kB\nMemAvailable: 217088 kB\n");
  escrever_arq(p_tpk,
      "[arranque] ok\n"
      "[tv] modelo=QN55Q80BAGXZD tizen=6.5 host=api9 dotnet=.NET 6.0 tela=1920x1080\n");
  escrever_arq(p_nyx, "{\"webos_release\":\"4.9.7\"}\n");

  /* 1. Prova de nao-bloqueio + leitura unica (cache) no caminho TPK */
  farolperfil_teste_reiniciar();
  atomic_store(&gancho_entrou, 0);
  atomic_store(&gancho_liberar, 0);
  FarolPerfilTesteFontes cfg = {0};
  cfg.plataforma = "tizen_tpk";
  cfg.meminfo_caminho = p_mem;
  cfg.tpk_log_caminho = p_tpk;
  cfg.usar_env_real = 0;
  cfg.antes_de_ler = gancho_lento;
  farolperfil_teste_configurar(&cfg);

  uint64_t t0 = agora_ms();
  farolperfil_iniciar();
  FarolPerfil snap;
  int pronto = farolperfil_obter(&snap);
  uint64_t dt = agora_ms() - t0;
  /* O fio trabalhador esta preso no gancho; o fio principal volta na hora! */
  assert(pronto == 0);
  assert(dt < 200);
  assert(strcmp(snap.aparelho.modelo, "?") == 0);
  assert(snap.aparelho.ram_total_mb == -1);

  /* Varias chamadas a iniciar/obter nunca disparam segunda leitura */
  for (int i = 0; i < 10; i++) {
    farolperfil_iniciar();
    assert(farolperfil_obter(&snap) == 0);
  }

  /* Libera o trabalhador e espera concluir */
  atomic_store(&gancho_liberar, 1);
  assert(farolperfil_teste_esperar(5000) == 1);
  assert(farolperfil_teste_leituras() == 1);
  assert(farolperfil_obter(&snap) == 1);
  assert(strcmp(snap.aparelho.plataforma, "tizen_tpk") == 0);
  assert(strcmp(snap.aparelho.modelo, "QN55Q80BAGXZD") == 0);
  assert(strcmp(snap.aparelho.so, "6.5") == 0);
  assert(snap.aparelho.so_maior == 6);
  assert(snap.aparelho.ram_total_mb == 1870);
  assert(snap.aparelho.ram_livre_mb == 212);
  assert(snap.aparelho.tela.w == 1920 && snap.aparelho.tela.h == 1080);

  /* Chamadas adicionais depois de pronto continuam usando o cache (leituras == 1) */
  farolperfil_iniciar();
  assert(farolperfil_obter(&snap) == 1);
  assert(farolperfil_teste_leituras() == 1);
  farolperfil_encerrar();

  /* 2. Caminho LG webOS: nyx + receptor farolperfil_definir_modelo */
  farolperfil_teste_reiniciar();
  memset(&cfg, 0, sizeof cfg);
  cfg.plataforma = "webos";
  cfg.meminfo_caminho = p_mem;
  cfg.nyx_caminho = p_nyx;
  cfg.usar_env_real = 0;
  farolperfil_teste_configurar(&cfg);

  /* Receptor LG chamado ANTES da coleta deve sobreviver a ela */
  assert(farolperfil_definir_modelo("OLED55C9PSA") == 1);
  farolperfil_iniciar();
  assert(farolperfil_teste_esperar(5000) == 1);
  assert(farolperfil_obter(&snap) == 1);
  assert(strcmp(snap.aparelho.plataforma, "webos") == 0);
  assert(strcmp(snap.aparelho.modelo, "OLED55C9PSA") == 0);
  assert(strcmp(snap.aparelho.so, "4.9.7") == 0 && snap.aparelho.so_maior == 4);
  assert(snap.aparelho.ram_total_mb == 1870 && snap.aparelho.ram_livre_mb == 212);

  /* Receptor LG com URL maliciosa -> "?" */
  assert(farolperfil_definir_modelo("https://evil.invalid/OLED55C9") == 0);
  assert(farolperfil_obter(&snap) == 1);
  assert(strcmp(snap.aparelho.modelo, "?") == 0);
  farolperfil_encerrar();

  /* 3. Caminho Android via NUVIO_TV_INFO + NUVIO_PANEL_SIZE */
  farolperfil_teste_reiniciar();
  memset(&cfg, 0, sizeof cfg);
  cfg.plataforma = "android";
  cfg.meminfo_caminho = p_mem;
  cfg.env_android_info = "TCL 55C745|34|14|2.1.0";
  cfg.env_panel_size = "3840x2160";
  cfg.usar_env_real = 0;
  farolperfil_teste_configurar(&cfg);
  farolperfil_iniciar();
  assert(farolperfil_teste_esperar(5000) == 1);
  assert(farolperfil_obter(&snap) == 1);
  assert(strcmp(snap.aparelho.plataforma, "android") == 0);
  assert(strcmp(snap.aparelho.modelo, "TCL 55C745") == 0);
  assert(snap.aparelho.sdk == 34 && strcmp(snap.aparelho.so, "14") == 0 && snap.aparelho.so_maior == 14);
  assert(snap.aparelho.tela.w == 3840 && snap.aparelho.tela.h == 2160);
  assert(strcmp(snap.cliente.versao, "2.1.0") == 0);
  farolperfil_encerrar();

  /* 4. Caminho WASM via NUVIO_FAROL_WGT (sem /proc/meminfo -> RAM = -1) */
  farolperfil_teste_reiniciar();
  memset(&cfg, 0, sizeof cfg);
  cfg.plataforma = "tizen_wgt";
  cfg.env_wgt_info = "QN55Q80B|6.5";
  cfg.usar_env_real = 0;
  farolperfil_teste_configurar(&cfg);
  farolperfil_iniciar();
  assert(farolperfil_teste_esperar(5000) == 1);
  assert(farolperfil_obter(&snap) == 1);
  assert(strcmp(snap.aparelho.plataforma, "tizen_wgt") == 0);
  assert(strcmp(snap.aparelho.modelo, "QN55Q80B") == 0);
  assert(strcmp(snap.aparelho.so, "6.5") == 0 && snap.aparelho.so_maior == 6);
  assert(snap.aparelho.ram_total_mb == -1 && snap.aparelho.ram_livre_mb == -1);
  farolperfil_encerrar();

  /* 5. Corrida entre 6 fios leitores/escritores + trabalhador (TSan) */
  farolperfil_teste_reiniciar();
  atomic_store(&gancho_entrou, 0);
  atomic_store(&gancho_liberar, 0);
  memset(&cfg, 0, sizeof cfg);
  cfg.plataforma = "webos";
  cfg.meminfo_caminho = p_mem;
  cfg.nyx_caminho = p_nyx;
  cfg.usar_env_real = 0;
  cfg.antes_de_ler = gancho_lento;
  farolperfil_teste_configurar(&cfg);

  pthread_t fios[6];
  int ids[6];
  for (int i = 0; i < 6; i++) {
    ids[i] = i;
    assert(pthread_create(&fios[i], NULL, fio_leitor_stress, &ids[i]) == 0);
  }
  struct timespec pausa = { 0, 5000000L }; /* 5 ms */
  nanosleep(&pausa, NULL);
  atomic_store(&gancho_liberar, 1);
  for (int i = 0; i < 6; i++) assert(pthread_join(fios[i], NULL) == 0);
  assert(farolperfil_teste_esperar(5000) == 1);
  assert(farolperfil_teste_leituras() == 1);
  farolperfil_encerrar();

  unlink(p_mem);
  unlink(p_tpk);
  unlink(p_nyx);
}

static void testar_escrita_redator(void) {
  FarolPerfil p;
  FarolJson j;
  char buf[FAROL_JSON_N];

  /* Caso 1: perfil completo (espelha PLANO §2.6 / farol_registro_v1.json) */
  farolperfil_padrao(&p);
  strcpy(p.aparelho.plataforma, "webos");
  strcpy(p.aparelho.so, "4.9");
  p.aparelho.so_maior = 4;
  strcpy(p.aparelho.modelo, "OLED55C9PSA");
  p.aparelho.ram_total_mb = 1870;
  p.aparelho.ram_livre_mb = 212;
  p.aparelho.tela = (FarolPerfilTela){ 3840, 2160, 1, 1, 1, -1 };
  p.aparelho.audio = (FarolPerfilAudio){ 1, -1, 1, 1 };
  p.aparelho.rede = (FarolPerfilRede){ 0, 0, 0.0 };

  strcpy(p.cliente.versao, "2.1.0");
  strcpy(p.cliente.commit, "c18c21756");
  strcpy(p.cliente.build, "webos-ipk");
  strcpy(p.cliente.backend, "starfish");
  strcpy(p.cliente.idioma, "pt-BR");
  p.cliente.aberto_s = 1842;
  strcpy(p.cliente.gpu, "auto:alto");
  p.cliente.ajustes.glass_ui = 1;
  strcpy(p.cliente.ajustes.qualidade_max, "4k");
  p.cliente.ajustes.tamanho_max_gb = 40.0;
  strcpy(p.cliente.ajustes.dts, "ac3");
  p.cliente.ajustes.dv_mkv = 0;
  p.cliente.ajustes.legenda_auto_sync = 1;
  p.cliente.addons = (FarolPerfilAddons){
    12, 11, 1, FAROL_ADDON_CATALOGO | FAROL_ADDON_FONTES | FAROL_ADDON_LEGENDAS
  };
  p.cliente.debrid_mask = FAROL_DEBRID_TORBOX;
  p.cliente.contas_mask = FAROL_CONTA_TRAKT | FAROL_CONTA_SIMKL;

  assert(farolredator_abrir(&j, buf, sizeof buf));
  assert(farolperfil_escrever(&j, &p));
  assert(farolredator_fechar(&j));
  assert(j.campos_recusados == 0);
  assert(strstr(buf, "\"plataforma\":\"webos\"") != NULL);
  assert(strstr(buf, "\"so\":\"4.9-webos\"") != NULL);
  assert(strstr(buf, "\"modelo\":\"OLED55C9PSA\"") != NULL);
  assert(strstr(buf, "\"ram_total_mb\":1870") != NULL);
  assert(strstr(buf, "\"ram_livre_mb\":212") != NULL);
  assert(strstr(buf, "\"versao\":\"2.1.0-ipk\"") != NULL);
  assert(strstr(buf, "\"commit\":\"c18c21756\"") != NULL);
  assert(strstr(buf, "\"tipos\":[\"catalogo\",\"fontes\",\"legendas\"]") != NULL);
  assert(strstr(buf, "\"debrid\":[\"torbox\"]") != NULL);
  assert(strstr(buf, "\"contas\":[\"trakt\",\"simkl\"]") != NULL);

  /* Caso 2: tudo desconhecido (-1 / "?") -> 0 campos recusados, modelo="x" */
  farolperfil_padrao(&p);
  assert(farolredator_abrir(&j, buf, sizeof buf));
  assert(farolperfil_escrever(&j, &p));
  assert(farolredator_fechar(&j));
  assert(j.campos_recusados == 0);
  assert(strstr(buf, "\"modelo\":\"x\"") != NULL);
  assert(strstr(buf, "\"so_maior\":-1") != NULL);
  assert(strstr(buf, "\"ram_total_mb\":-1") != NULL);

  /* Caso 3: modelo malicioso ou fora da gramatica do redator nunca vaza */
  farolperfil_padrao(&p);
  strcpy(p.aparelho.modelo, "https://CANARIO_URL/segredo");
  assert(farolredator_abrir(&j, buf, sizeof buf));
  assert(farolperfil_escrever(&j, &p));
  assert(farolredator_fechar(&j));
  assert(j.campos_recusados == 0);
  assert(strstr(buf, "CANARIO") == NULL);
  assert(strstr(buf, "://") == NULL);
  assert(strstr(buf, "\"modelo\":\"x\"") != NULL);

  /* Caso 4: modelo com espaco (Android "<MFR> <MODEL>") sai na forma canonica
   * da gramatica fechada, sem espaco — "TCL 55C745" -> "TCL55C745" */
  farolperfil_padrao(&p);
  strcpy(p.aparelho.modelo, "TCL 55C745");
  assert(farolredator_abrir(&j, buf, sizeof buf));
  assert(farolperfil_escrever(&j, &p));
  assert(farolredator_fechar(&j));
  assert(j.campos_recusados == 0);
  assert(strstr(buf, "\"modelo\":\"TCL55C745\"") != NULL);
  assert(strstr(buf, "\"modelo\":\"x\"") == NULL);

  /* Caso 5: LG fora da linha OLED comeca com digito — a gramatica publicada
   * do servidor nao tem ramo para ela, entao emitir o modelo recusa o
   * registro inteiro (422). O coletor guarda o modelo real; o JSON sai "x"
   * e o valor nao vaza — o comportamento do pai. */
  farolperfil_padrao(&p);
  strcpy(p.aparelho.modelo, "50NANO80ASA");
  assert(farolredator_abrir(&j, buf, sizeof buf));
  assert(farolperfil_escrever(&j, &p));
  assert(farolredator_fechar(&j));
  assert(j.campos_recusados == 0);
  assert(strstr(buf, "\"modelo\":\"x\"") != NULL);
  assert(strstr(buf, "50NANO80ASA") == NULL);

  farolperfil_padrao(&p);
  strcpy(p.aparelho.modelo, "43LM6300PSB");
  assert(farolredator_abrir(&j, buf, sizeof buf));
  assert(farolperfil_escrever(&j, &p));
  assert(farolredator_fechar(&j));
  assert(j.campos_recusados == 0);
  assert(strstr(buf, "\"modelo\":\"x\"") != NULL);
  assert(strstr(buf, "43LM6300PSB") == NULL);

  /* Caso 6: a gramatica segue fechada — forma parecida mas fora dela, ou
   * valor barrado pela privacidade do coletor, sai "x" sem vazar */
  for (int k = 0; k < 6; k++) {
    static const char *const fora[] = {
      "12ABCDEXYZ",              /* digitos + letras, sem prefixo da lista */
      "Sony BRAVIA 4K VH2",      /* marca fora da lista, com espaco */
      "123456",                  /* so digitos (PIN) */
      "user@host.invalid",       /* @: a privacidade recusa no coletor */
      "www.evil.invalid",        /* www.: idem */
      "TV 192.168.0.1"           /* IPv4 embutido: idem */
    };
    farolperfil_padrao(&p);
    strcpy(p.aparelho.modelo, fora[k]);
    assert(farolredator_abrir(&j, buf, sizeof buf));
    assert(farolperfil_escrever(&j, &p));
    assert(farolredator_fechar(&j));
    assert(j.campos_recusados == 0);
    assert(strstr(buf, "\"modelo\":\"x\"") != NULL);
    assert(strstr(buf, fora[k]) == NULL);
  }
}

/* Releitura limitada do log do TPK: [tv] tardia, [tv] alem do prazo (recoleta
 * no proximo iniciar/obter) e [tv] alem do prefixo de 8 KiB. */
static void testar_tpk_releitura(const char *dir) {
  char p_tpk[320], p_mem[320];
  long pid = (long)getpid();
  FarolPerfilTesteFontes cfg = {0};
  FarolPerfil snap;
  snprintf(p_tpk, sizeof p_tpk, "%s/nuvio-fp-%ld-tpk-re.log", dir, pid);
  snprintf(p_mem, sizeof p_mem, "%s/nuvio-fp-%ld-tpk-re-meminfo", dir, pid);

  escrever_arq(p_mem, "MemTotal: 1048576 kB\nMemAvailable: 524288 kB\n");

  /* (a) [tv] tardia DENTRO do prazo: o fio trabalhador rele o log ate achar */
  escrever_arq(p_tpk, "[arranque] inicio do log\n");
  farolperfil_teste_reiniciar();
  cfg.plataforma = "tizen_tpk";
  cfg.meminfo_caminho = p_mem;
  cfg.tpk_log_caminho = p_tpk;
  cfg.usar_env_real = 0;
  farolperfil_teste_configurar(&cfg);
  farolperfil_iniciar();
  {
    struct timespec pausa = { 0, 300000000L }; /* 300 ms << prazo */
    nanosleep(&pausa, NULL);
    FILE *f = fopen(p_tpk, "ab");
    assert(f != NULL);
    assert(fputs("[host] [tv] modelo=QN55Q80BAGXZD tizen=6.5 host=api9 "
                 "dotnet=.NET 6.0 tela=1920x1080\n", f) >= 0);
    assert(fclose(f) == 0);
  }
  assert(farolperfil_teste_esperar(8000) == 1);
  assert(farolperfil_teste_leituras() == 1);
  assert(farolperfil_obter(&snap) == 1);
  assert(strcmp(snap.aparelho.modelo, "QN55Q80BAGXZD") == 0);
  assert(strcmp(snap.aparelho.so, "6.5") == 0 && snap.aparelho.so_maior == 6);
  assert(snap.aparelho.tela.w == 1920 && snap.aparelho.tela.h == 1080);
  assert(strcmp(snap.cliente.build, "tizen-tpk") == 0);
  farolperfil_encerrar();

  /* (b) [tv] alem do prazo: a leitura sem a linha nao congela — a consulta
   * seguinte dispara a recoleta e acha a linha publicada depois */
  escrever_arq(p_tpk, "[arranque] inicio do log\n");
  farolperfil_teste_reiniciar();
  farolperfil_teste_configurar(&cfg);
  farolperfil_iniciar();
  assert(farolperfil_teste_esperar(8000) == 1);  /* o prazo fecha sem [tv] */
  assert(farolperfil_obter(&snap) == 1);         /* cache parcial e servido */
  assert(strcmp(snap.aparelho.so, "?") == 0);
  {
    FILE *f = fopen(p_tpk, "ab");
    assert(f != NULL);
    assert(fputs("[host] [tv] modelo=QN55Q80BAGXZD tizen=6.5 host=api9 "
                 "dotnet=.NET 6.0 tela=1920x1080\n", f) >= 0);
    assert(fclose(f) == 0);
  }
  farolperfil_iniciar();                         /* pendente: recoleta */
  assert(farolperfil_teste_esperar(8000) == 1);
  assert(farolperfil_teste_leituras() == 2);     /* uma recoleta de verdade */
  assert(farolperfil_obter(&snap) == 1);
  assert(strcmp(snap.aparelho.modelo, "QN55Q80BAGXZD") == 0);
  assert(strcmp(snap.aparelho.so, "6.5") == 0 && snap.aparelho.so_maior == 6);
  assert(snap.aparelho.tela.w == 1920 && snap.aparelho.tela.h == 1080);
  /* A linha encontrada encerra a pendencia: novas consultas nao releem */
  farolperfil_iniciar();
  assert(farolperfil_teste_esperar(2000) == 1);
  assert(farolperfil_teste_leituras() == 2);
  farolperfil_encerrar();

  /* (c) [tv] alem do prefixo de 8 KiB: a varredura do arquivo inteiro acha a
   * ultima ocorrencia na primeira leitura */
  {
    FILE *f = fopen(p_tpk, "wb");
    assert(f != NULL);
    assert(fputs("[host] [tv] modelo=UE43DU7700KXZD tizen=8.0 host=api9 "
                 "dotnet=.NET 8.0 tela=1280x720\n", f) >= 0);
    for (int i = 0; i < 280; i++)
      assert(fputs("[host] inicializacao do player\n", f) >= 0);
    assert(fputs("[host] [tv] modelo=UN50TU8000GXZD tizen=5.5 host=tpk40 "
                 "dotnet=.NET Core 2.0 tela=1920x1080\n", f) >= 0);
    assert(fclose(f) == 0);
  }
  farolperfil_teste_reiniciar();
  farolperfil_teste_configurar(&cfg);
  farolperfil_iniciar();
  assert(farolperfil_teste_esperar(5000) == 1);
  assert(farolperfil_teste_leituras() == 1);
  assert(farolperfil_obter(&snap) == 1);
  assert(strcmp(snap.aparelho.modelo, "UN50TU8000GXZD") == 0);
  assert(strcmp(snap.aparelho.so, "5.5") == 0 && snap.aparelho.so_maior == 5);
  assert(snap.aparelho.tela.w == 1920 && snap.aparelho.tela.h == 1080);
  farolperfil_encerrar();

  unlink(p_tpk);
  unlink(p_mem);
}

/* TPK40 (binario compilado com -DNV_TPK -DNV_TPK40): o build decidido em
 * compilacao nao e sobrescrito pela coleta; com [tv] no log saem modelo, SO e
 * tela; sem ela o pacote segue tizen-tpk40. */
static void testar_tpk40_build(const char *dir) {
#if defined(NV_TPK) && defined(NV_TPK40)
  char p_tpk[320];
  long pid = (long)getpid();
  FarolPerfilTesteFontes cfg = {0};
  FarolPerfil snap;
  snprintf(p_tpk, sizeof p_tpk, "%s/nuvio-fp-%ld-tpk40.log", dir, pid);

  escrever_arq(p_tpk,
      "[arranque] inicio\n"
      "[host] [tv] modelo=UN50TU8000GXZD tizen=5.5 host=tpk40 "
      "dotnet=.NET Core 2.0 tela=1920x1080\n");
  farolperfil_teste_reiniciar();
  cfg.plataforma = "tizen_tpk";
  cfg.tpk_log_caminho = p_tpk;
  cfg.usar_env_real = 0;
  farolperfil_teste_configurar(&cfg);
  farolperfil_iniciar();
  assert(farolperfil_teste_esperar(5000) == 1);
  assert(farolperfil_obter(&snap) == 1);
  assert(strcmp(snap.cliente.build, "tizen-tpk40") == 0);
  assert(strcmp(snap.cliente.backend, "tpk") == 0);
  assert(strcmp(snap.aparelho.plataforma, "tizen_tpk") == 0);
  assert(strcmp(snap.aparelho.modelo, "UN50TU8000GXZD") == 0);
  assert(strcmp(snap.aparelho.so, "5.5") == 0 && snap.aparelho.so_maior == 5);
  assert(snap.aparelho.tela.w == 1920 && snap.aparelho.tela.h == 1080);
  farolperfil_encerrar();

  /* Sem [tv] nem variaveis: a identidade do pacote nao e perdida */
  escrever_arq(p_tpk, "[arranque] inicio\n");
  farolperfil_teste_reiniciar();
  farolperfil_teste_configurar(&cfg);
  farolperfil_iniciar();
  assert(farolperfil_teste_esperar(8000) == 1);
  assert(farolperfil_obter(&snap) == 1);
  assert(strcmp(snap.cliente.build, "tizen-tpk40") == 0);
  assert(strcmp(snap.aparelho.modelo, "?") == 0);
  farolperfil_encerrar();

  unlink(p_tpk);
  printf("farol_perfil: tpk40 build e identidade ok\n");
#else
  (void)dir;
  printf("farol_perfil: tpk40 pulado (binario sem NV_TPK40)\n");
#endif
}

int main(int argc, char **argv) {
  const char *tmp = getenv("TMPDIR");
  if (!tmp || !tmp[0]) tmp = ".";

  if (argc > 1 && strcmp(argv[1], "tpk40") == 0) {
    testar_tpk40_build(tmp);
    return 0;
  }

  testar_filtro_modelo();
  testar_android_info();
  testar_tpk_tv();
  testar_meminfo();
  testar_wgt_webos_tela();
  testar_assincrono_cache_e_tsan(tmp);
  testar_tpk_releitura(tmp);
  testar_escrita_redator();

  printf("farol_perfil: parsers, cache e redator ok\n");
  return 0;
}
