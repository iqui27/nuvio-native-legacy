// Included by ajustes_sync.c: real settings, session, sync, disk; only HTTP replaced.
// Losing any device scalar/store, leaking it to another TV, or dropping unknown
// fields breaks this test. Each reinstall starts a fresh process and directory.
#include <dirent.h>
static void fileWrite(const char *path, const void *buf, size_t n) {
  FILE *f = fopen(path, "wb"); assert(f);
  assert(fwrite(buf, 1, n, f) == n); assert(!fclose(f));
}
static char *fileRead(const char *path) {
  FILE *f = fopen(path, "rb"); assert(f);
  assert(!fseek(f, 0, SEEK_END)); long n = ftell(f); assert(n >= 0);
  rewind(f); char *s = malloc((size_t)n + 1); assert(s);
  assert(fread(s, 1, (size_t)n, f) == (size_t)n); fclose(f); s[n] = 0; return s;
}
static void checkFile(const char *name, const char *expected) {
  char *s = dados_ler(name); check(s && !strcmp(s, expected), name); free(s);
}
static int recoveryStage(const char *dir, const char *stage) {
  char path[1024]; int expected[AJ_N];
  setenv("NUVIO_DEVICE_ID", !strcmp(stage, "device-other") ? "tv-B" : "tv-A", 1);
  setenv("NUVIO_DADOS", dir, 1); dados_iniciar(NULL); player_dir(dir);
  ajustes_dir(dir); fil_definir_perfil(1);
  if (!strcmp(stage, "device-reopen")) {
    snprintf(path, sizeof path, "%s/../expected.bin", dir); char *raw = fileRead(path);
    memcpy(expected, raw, sizeof expected); free(raw);
    for (int i = 0; i < AJ_N; i++) {
      if (!CHAVE[i] || CHAVE[i][0] == '-' ||
          (OPCOES[i].tipo != OP_ESCOLHA && OPCOES[i].tipo != OP_NUMERO)) continue;
      check(valor[i] == expected[i], CHAVE[i]);
    }
    puts("PASS: device-reopen"); return 0;
  }
  login();
  if (!strcmp(stage, "device-save")) {
    cloud = strdup("{\"future_web\":{\"keep\":42},\"c_settings_v1\":{\"future_ext\":42,\"values\":{\"c_future\":42}}}");
    cycle();
    for (int i = 0; i < AJ_N; i++) {
      if (!CHAVE[i] || CHAVE[i][0] == '-' ||
          (OPCOES[i].tipo != OP_ESCOLHA && OPCOES[i].tipo != OP_NUMERO)) continue;
      int v = OPCOES[i].tipo == OP_ESCOLHA ? (valor[i] ? 0 : 1) :
              (valor[i] == OPCOES[i].min ? OPCOES[i].max : OPCOES[i].min);
      valor[i] = limita(i, v);
    }
    valor[AJ_RESOLUCAO] = RES_4K; valor[AJ_VIDRO] = 0;
    valor[AJ_GPU_EFEITOS] = 1; valor[AJ_TEMA] = AJ_TEMA_DINAMICA;
    valor[AJ_IDIOMA] = 4; valor[AJ_PROVEDORES_ESCOPO] = 1;
    valor[AJ_DET_TRAILER_SOM] = 1; // Retired setting; only unified trailer sound is configurable.
    valor[AJ_PLAYER_EXTERNO] = 2; valor[AJ_PLAYER_ANIME] = 3; // Uninstalled package asks on reopening.
    // Save every scalar plus the extra records written by gravar().
    for (int i = 0; i < EX_NFONTES; i++) notaFontes[i] = EX_NFONTES - 1 - i;
    bibliotecaFonteEscolhida = memoriaEscolhida = 1;
    gravar(); memcpy(expected, valor, sizeof expected);
    expected[AJ_DV7] = valorPadrao[AJ_DV7]; // Experimental conversion is excluded from device backup.
    snprintf(path, sizeof path, "%s/../expected.bin", dir); fileWrite(path, expected, sizeof expected);
    dados_gravar("fanart.txt", "0123456789abcdef");
    dados_gravar("seekr.txt", "own-seekr-key");
    dados_gravar("curadoria.txt", "https://curated.invalid/feed");
    dados_gravar("player-externo.txt", "test.player\ntest.anime\n");
    dados_gravar("ltv-urls-p1.txt", "configured-streams\n");
    dados_gravar("xtream-p1.txt", "portal=test.invalid\nusuario=alice\nsenha=secret\n");
    dados_gravar("sessao-private.txt", "DO-NOT-UPLOAD");
    char nome[128]; ajustes_provedores_arquivo(nome, sizeof nome, "fontefalha");
    dados_gravar(nome, "provedores 1\nfiltro_maxgb 10\n");
    fil_registrar("org.test_movie_top", "Top", "Test", "movie", 10);
    fil_registrar("org.test_series_new", "New", "Test", "series", 10);
    fonteregra_definir_regex("1080p");
    const char *order[] = {"Addon A", "Addon B"}; fonteregra_ordem_definir(order, 2);
    fil_definir_limite(3); fil_gravar_registro();
    player_leg_estilo_carregar();
    VideoLegendaEstilo style = *player_leg_estilo(); style.tamanho = 160;
    assert(player_leg_estilo_definir(&style, PLR_LEG_FONTE));
    player_aspecto_definir(PLR_ASP_ESTICAR);
    // Deliberately no sync_proteger: direct file editors must still upload.
    cycle();
    check(pushes > 0, "external/device edits uploaded without dirty notification");
    check(strstr(cloud, "future_web") && strstr(cloud, "future_ext") && strstr(cloud, "c_future"), "unknown web and C fields preserved");
    check(!strstr(cloud, "DO-NOT-UPLOAD") && !strstr(cloud, "444f2d4e4f542d55504c4f4144"), "session files excluded");
    const char *devices, *de, *device, *end, *files, *fe, *v, *f;
    assert(ajJsonGet(cloud, cloud + strlen(cloud), "c_devices_v1", &devices, &de));
    assert(ajJsonGet(devices, de, dados_aparelho_id(), &device, &end));
    assert(ajJsonGet(device, end, "files", &files, &fe));
    assert(ajJsonGet(files, fe, "file8", &v, &f));
    char *rows = ajusteUnhex(v, f);
    check(rows && strstr(rows, "limite 3\n") && !strstr(rows, "linha ") && !strstr(rows, "ordem "), "device snapshot holds only row device fields"); free(rows);
    for (int i = 0; i < 3; i++) {
      const char *keys[] = {"file22", "file24", "file25"};
      assert(ajJsonGet(files, fe, keys[i], &v, &f));
      check(f - v == 4 && !strncmp(v, "null", 4), "device snapshot excludes shared rules and filters");
    }
    // Keep an old client's whole-file device snapshot in the account too.
    Jsw legacy; jsw_iniciar(&legacy); jsw_obj_ini(&legacy);
    for (int i = 0; i < 4; i++) {
      const char *keys[] = {"file8", "file22", "file24", "file25"};
      const char *names[] = {"fileirasui-p1.txt", nome, "fonteregra.txt", nome};
      char *text = dados_ler(names[i]); ajusteHex(&legacy, keys[i], text); free(text);
    }
    jsw_obj_fim(&legacy);
    char *patch = malloc(strlen(cloud) + strlen(jsw_texto_final(&legacy)) + 1); assert(patch);
    sprintf(patch, "%.*s%s%s", (int)(device - cloud), cloud, "{\"files\":", jsw_texto_final(&legacy));
    strcat(patch, "}"); strcat(patch, end);
    char *updated = ajJsonMerge(cloud, cloud + strlen(cloud), patch, patch + strlen(patch)); assert(updated);
    free(patch); jsw_livre(&legacy); free(cloud); cloud = updated;
    snprintf(path, sizeof path, "%s/../cloud.json", dir); fileWrite(path, cloud, strlen(cloud));
  } else {
    snprintf(path, sizeof path, "%s/../cloud.json", dir); cloud = fileRead(path);
    snprintf(path, sizeof path, "%s/../expected.bin", dir); char *raw = fileRead(path);
    memcpy(expected, raw, sizeof expected); free(raw);
    cycle();
    int other = !strcmp(stage, "device-other");
    for (int i = 0; i < AJ_N; i++) {
      if (!CHAVE[i] || CHAVE[i][0] == '-' ||
          (OPCOES[i].tipo != OP_ESCOLHA && OPCOES[i].tipo != OP_NUMERO)) continue;
      if (other && ajusteAparelho(i)) continue;
      check(valor[i] == expected[i], CHAVE[i]);
    }
    if (other) {
      check(valor[AJ_RESOLUCAO] == valorPadrao[AJ_RESOLUCAO], "second TV keeps own resolution");
      check(valor[AJ_GPU_EFEITOS] == valorPadrao[AJ_GPU_EFEITOS], "second TV keeps own GPU setting");
      check(valor[AJ_VIDRO] == valorPadrao[AJ_VIDRO], "second TV keeps own glass setting");
      ajustes_aplicar_blob("{\"c_settings_v1\":{\"values\":{\"c_vidroLocal\":1,\"c_modernSidebarBlur\":1}}}");
      check(valor[AJ_VIDRO] == valorPadrao[AJ_VIDRO] && valor[AJ_RAIL_BLUR] == valorPadrao[AJ_RAIL_BLUR], "legacy shared blob cannot override device GPU choices");
      check(!dados_ler("fanart.txt"), "second TV receives no device credentials");
      // B edits shared files after A's upload, with a different provider scope/quota.
      fil_mover(1, -1); fil_definir_limite(5); fil_gravar_registro();
      fonteregra_definir_regex("4K");
      const char *order[] = {"Addon B", "Addon A"}; fonteregra_ordem_definir(order, 2);
      fontefalha_definir_provedores(0);
      FonteFiltroConfig filter; fontefalha_filtro(&filter); filter.maxGB = 25;
      assert(fontefalha_filtro_definir(&filter));
      // A second TV writes its section without losing TV A's snapshot.
      valor[AJ_RESOLUCAO] = 0; sync_proteger_ajustes_locais(); cycle();
      snprintf(path, sizeof path, "%s/../cloud.json", dir); fileWrite(path, cloud, strlen(cloud));
    } else {
      checkFile("fanart.txt", "0123456789abcdef"); checkFile("seekr.txt", "own-seekr-key");
      checkFile("ltv-urls-p1.txt", "configured-streams\n");
      checkFile("xtream-p1.txt", "portal=test.invalid\nusuario=alice\nsenha=secret\n");
      char protectedPath[1024]; struct stat mode;
      snprintf(protectedPath, sizeof protectedPath, "%s/xtream-p1.txt", dir);
      check(!stat(protectedPath, &mode) && !(mode.st_mode & 077), "restored credentials retain owner-only permissions");
      check(!strcmp(curadoria_url(), "https://curated.invalid/feed"), "curation applied in memory");
      check(fil_limite_gravado() == 3, "same TV row quota restored");
      check(!strcmp(fil_chave(0), "org.test_series_new"), "A receives B's newer shared home row order");
      char rule[FR_REGEX_MAX], first[FR_NOME_MAX]; fonteregra_regex(rule, sizeof rule);
      fonteregra_ordem_nome(0, first, sizeof first);
      check(!strcmp(rule, "4K") && !strcmp(first, "Addon B"), "A receives B's newer addon rules");
      FonteFiltroConfig filter; fontefalha_filtro(&filter);
      check(!fontefalha_provedores() && filter.maxGB == 25, "A receives B's shared filters in A's provider scope");
      check(player_aspecto() == PLR_ASP_ESTICAR && player_leg_estilo()->tamanho == 160, "player and subtitle prefs restored");
      check(ajustes_nota_fonte(0) == EX_NFONTES - 1, "rating order restored");
      check(bibliotecaFonteEscolhida && memoriaEscolhida, "explicit choice markers restored");
      // Partial blobs never reset missing shared/device keys or stores.
      ajustes_aplicar_blob("{\"features\":{\"hero_section_enabled\":true}}");
      check(valor[AJ_RESOLUCAO] == RES_4K && valor[AJ_IDIOMA] == 4, "partial blob preserves absent keys");
      sessao_sair(); sync_esquecer_usuario();
      memcpy(valor, valorPadrao, sizeof valor); login(); cycle();
      check(valor[AJ_RESOLUCAO] == RES_4K && valor[AJ_IDIOMA] == 4, "logout/login restores same TV");
      check(!strcmp(fil_chave(0), "org.test_series_new") && fil_limite_gravado() == 3, "logout keeps shared order and device quota");
      char *before = ajustesExtras(); assert(before);
      sync_proteger_ajustes_locais(); cycle();
      const char *shared, *end;
      assert(ajJsonGet(cloud, cloud + strlen(cloud), "c_settings_v1", &shared, &end));
      for (int i = 0; i < 3; i++) {
        char key[16]; const char *v, *f, *next, *last;
        snprintf(key, sizeof key, "store%d", i);
        assert(ajJsonGet(before, before + strlen(before), key, &v, &f));
        assert(ajJsonGet(shared, end, key, &next, &last));
        check(f - v == last - next && !memcmp(v, next, (size_t)(f - v)), "A never re-uploads stale shared files");
      }
      free(before);
    }
  }
  printf("PASS: %s\n", stage); free(cloud); return 0;
}
static int recovery(const char *exe, const char *root) {
  const char *stages[] = {"device-save", "device-other", "device-restore", "device-reinstall", "device-reopen"};
  for (int i = 0; i < 5; i++) {
    char dir[1024]; snprintf(dir, sizeof dir, "%s/%s", root, i == 4 ? "device-reinstall" : stages[i]);
    if (i != 4) assert(!mkdir(dir, 0700));
    pid_t pid = fork(); assert(pid >= 0);
    if (!pid) { execl(exe, exe, dir, stages[i], (char *)NULL); _exit(127); }
    int status; assert(waitpid(pid, &status, 0) == pid);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) return 1;
  }
  return 0;
}
