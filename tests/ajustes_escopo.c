// ESCOPO DOS AJUSTES (2.1): TV | perfil+conta | perfil local; migracao unica do
// ajustes.txt; copia entre perfis. Sem SDL na tela.
#include "../src/ajustes.c"
#include <assert.h>
#include <unistd.h>
#include "../src/admin.h"
#include "../src/perfilinfantil.h"

static int tem(const char *t, int op) {
  char alvo[96];
  snprintf(alvo, sizeof alvo, "%s ", CHAVE[op]);
  return t && strstr(t, alvo) != NULL;
}

static void escopos(void) {
  int i, n[4] = { 0, 0, 0, 0 };
  for (i = 0; i < AJ_N; i++) {
    int e = ajustes_escopo(i);
    assert(e >= 0 && e <= 3);
    n[e]++;
    // Nada com cara de segredo viaja com o perfil.
    if (e >= 2) {
      const char *c = CHAVE[i];
      assert(!strcasestr(c, "token") && !strcasestr(c, "senha") && !strcasestr(c, "password") &&
             !strcasestr(c, "secret") && !strstr(c, "Chave") && !strcasestr(c, "apikey"));
    }
    // Acao, leitura e chave "-" nunca tem escopo.
    if (OPCOES[i].tipo == OP_ACAO || OPCOES[i].tipo == OP_LEITURA || CHAVE[i][0] == '-') assert(e == 0);
    // O que e local nunca entra no blob da conta.
    if (e == 3) assert(somenteDesteAparelho(i));
    if (e == 2) assert(!somenteDesteAparelho(i));
  }
  assert(n[1] > 20 && n[2] > 20 && n[3] > 40);
  // Corte aprovado pelo dono (06/10): estes passam a ser do perfil...
  assert(ajustes_escopo(AJ_VIDRO) == 3 && ajustes_escopo(AJ_FUNDO) == 3 && ajustes_escopo(AJ_TAMANHO_UI) == 3);
  assert(ajustes_escopo(AJ_RELOGIO) == 3 && ajustes_escopo(AJ_HERO_TRAILER_SOM) == 3);
  assert(ajustes_escopo(AJ_NT_IMDB) == 3 && ajustes_escopo(AJ_LEG2_POS) == 3 && ajustes_escopo(AJ_CW_RETIDO_TAMBEM) == 3);
  // ...e estes continuam da TV.
  assert(ajustes_escopo(AJ_RESOLUCAO) == 1 && ajustes_escopo(AJ_TEX_MB) == 1 && ajustes_escopo(AJ_IDIOMA) == 1);
  assert(ajustes_escopo(AJ_LIVETV_RES) == 1 && ajustes_escopo(AJ_FONTE_UI) == 1 && ajustes_escopo(AJ_GPU_EFEITOS) == 1);
  assert(ajustes_escopo(AJ_TEMA) == 2 && ajustes_escopo(AJ_HERO) == 2);
  assert(ajustes_escopo(AJ_AUDIO_OFFSET) == 1 && ajustes_escopo(AJ_DV7) == 1 && ajustes_escopo(AJ_P2P_DESTINO) == 1);
  assert(!strcmp(ajSelo(AJ_VIDRO), "Este perfil"));
  assert(!strcmp(ajSelo(AJ_IDIOMA), "Esta TV"));
}

static void blobIgnoraLocal(void) {
  char blob[512];
  char *saida = NULL;
  valor[AJ_VIDRO] = 1; valor[AJ_RELOGIO] = 1;
  snprintf(blob, sizeof blob, "{\"%s\":0,\"%s\":0}", CHAVE[AJ_VIDRO], CHAVE[AJ_RELOGIO]);
  ajustes_aplicar_blob(blob);
  assert(valor[AJ_VIDRO] == 1 && valor[AJ_RELOGIO] == 1);
  // A costura tambem nao toca nas chaves locais, mesmo que o blob as traga.
  snprintf(blob, sizeof blob, "{\"%s\":true,\"%s\":true}", CHAVE[AJ_VIDRO], CHAVE[AJ_RELOGIO]);
  ajustes_mesclar_blob(blob, &saida);
  assert(!saida || (strstr(saida, "true") && !strstr(saida, "false")));
  free(saida);
  // C snapshots must omit local values and ignore them in legacy snapshots.
  char *extras = ajustesExtras();
  assert(extras);
  for (int i = 0; i < AJ_N; i++) if (ajustes_escopo(i) == 3) {
    char key[128];
    snprintf(key, sizeof key, "\"c_%s\"", CHAVE[i]);
    assert(!strstr(extras, key));
    int before = valor[i];
    snprintf(blob, sizeof blob, "{\"c_settings_v1\":{\"values\":{%s:%d}}}", key, before ? 0 : 1);
    ajustesExtrasAplicar(blob, "{}");
    assert(valor[i] == before);
  }
  free(extras);
  int hero = valor[AJ_HERO];
  snprintf(blob, sizeof blob, "{\"c_settings_v1\":{\"values\":{\"c_%s\":%d}}}", CHAVE[AJ_HERO], !hero);
  ajustesExtrasAplicar(blob, "{}");
  assert(valor[AJ_HERO] == !hero);
  valor[AJ_HERO] = hero;
}

static void migracao(void) {
  int perfis[2] = { 1, 2 };
  char *t, *a, *b;
  // TV 2.0.x: ajustes.txt com vidro desligado e relogio ligado; o perfil 2 ja
  // tinha uma copia antiga (so chaves de conta) com o destaque desligado.
  valor[AJ_VIDRO] = 1; valor[AJ_RELOGIO] = 0; valor[AJ_HERO] = 0; valor[AJ_RESOLUCAO] = RES_4K;
  assert(gravar());
  { char antigo[96]; snprintf(antigo, sizeof antigo, "%s 1\n", CHAVE[AJ_HERO]); dados_gravar("ajustes-p2.txt", antigo); }
  assert(ajustes_migrar_escopo_para(perfis, 2, 1) == 1);
  // Backup do arquivo da TV, uma vez.
  t = dados_ler("ajustes.txt.pre21");
  assert(t && tem(t, AJ_VIDRO));
  free(t);
  // Perfil 1 (ativo, sem copia): completo, com as chaves locais de agora.
  a = dados_ler("ajustes-p1.txt");
  assert(tem(a, AJ_VIDRO) && tem(a, AJ_RELOGIO) && tem(a, AJ_HERO) && !tem(a, AJ_RESOLUCAO));
  // Perfil 2: manteve o que era dele e ganhou as locais com o valor da TV.
  b = dados_ler("ajustes-p2.txt");
  { char alvo[96]; snprintf(alvo, sizeof alvo, "%s 1\n", CHAVE[AJ_HERO]); assert(strstr(b, alvo)); }
  { char alvo[96]; snprintf(alvo, sizeof alvo, "%s 1\n", CHAVE[AJ_VIDRO]); assert(strstr(b, alvo)); }
  { char alvo[96]; snprintf(alvo, sizeof alvo, "%s 0\n", CHAVE[AJ_RELOGIO]); assert(strstr(b, alvo)); }
  assert(!tem(b, AJ_RESOLUCAO));
  free(a); free(b);
  // Segunda vez: nada muda (nem o backup, mesmo com o ajustes.txt diferente).
  valor[AJ_VIDRO] = 0; gravar();
  assert(ajustes_migrar_escopo_para(perfis, 2, 1) == 0);
  t = dados_ler("ajustes.txt.pre21");
  { char alvo[96]; snprintf(alvo, sizeof alvo, "%s 1\n", CHAVE[AJ_VIDRO]); assert(strstr(t, alvo)); }
  free(t);
  // Existing active-profile values must survive, too.
  dados_apagar("ajustes-escopo.txt");
  valor[AJ_HERO] = 0;
  { char antigo[96]; snprintf(antigo, sizeof antigo, "%s 1\n", CHAVE[AJ_HERO]);
    assert(dados_gravar("ajustes-p1.txt", antigo)); }
  assert(ajustes_migrar_escopo_para(perfis, 2, 1) == 1);
  t = dados_ler("ajustes-p1.txt");
  { char alvo[96]; snprintf(alvo, sizeof alvo, "%s 1\n", CHAVE[AJ_HERO]); assert(strstr(t, alvo)); }
  free(t);
  // Sem perfis na mao, nao marca como feito.
  dados_apagar("ajustes-escopo.txt");
  assert(ajustes_migrar_escopo_para(NULL, 0, 1) == 0);
  t = dados_ler("ajustes-escopo.txt");
  assert(!t);
  // A failed destination write must leave migration retryable.
  dados_apagar("ajustes-p2.txt");
  char bloqueio[1024];
  snprintf(bloqueio, sizeof bloqueio, "%s/ajustes-p2.txt", dados_dir());
  assert(mkdir(bloqueio, 0700) == 0);
  assert(ajustes_migrar_escopo_para(perfis, 2, 1) == 0);
  assert(!dados_ler("ajustes-escopo.txt"));
  assert(rmdir(bloqueio) == 0);
  assert(ajustes_migrar_escopo_para(perfis, 2, 1) == 1);
  // Troca de perfil agora leva o vidro junto.
  valor[AJ_VIDRO] = 0;
  ajustes_perfil_guardar(1);
  valor[AJ_VIDRO] = 1;
  assert(ajustes_perfil_restaurar(1) == 1 && valor[AJ_VIDRO] == 0);
}


static int linhaVale(const char *t, int op, int v) {
  char alvo[96];
  snprintf(alvo, sizeof alvo, "%s %d\n", CHAVE[op], v);
  return t && strstr(t, alvo) != NULL;
}

static void copia(void) {
  char *a, *b, *c, *pend;
  int n;
  // Perfil ativo = 1. Estado do 1: vidro desligado, relogio ligado, destaque desligado.
  valor[AJ_VIDRO] = 1; valor[AJ_RELOGIO] = 0; valor[AJ_HERO] = 1; valor[AJ_RESOLUCAO] = RES_4K;
  { int d[AJ_N]; memcpy(d, valorPadrao, sizeof d); d[AJ_VIDRO] = 0; d[AJ_RELOGIO] = 1; d[AJ_HERO] = 0; perfilGravarVetor(2, d); }
  dados_apagar("ajustes-locais-p2.txt");
  n = ajustes_copiar_perfil(1, 2);
  assert(n >= 3);
  b = dados_ler("ajustes-p2.txt");
  assert(linhaVale(b, AJ_VIDRO, 1) && linhaVale(b, AJ_RELOGIO, 0) && linhaVale(b, AJ_HERO, 1));
  assert(!tem(b, AJ_RESOLUCAO));   // nunca o que descreve a TV
  free(b);
  assert(valor[AJ_VIDRO] == 1 && valor[AJ_RESOLUCAO] == RES_4K);   // origem intacta
  pend = dados_ler("ajustes-locais-p2.txt");   // a parte da conta fica pendente do destino
  assert(pend && pend[0] == '1');
  free(pend);
  // Idempotente: copiar de novo nao muda nada.
  assert(ajustes_copiar_perfil(1, 2) == 0);
  // Cadeia: 2 -> 3 (a origem e o arquivo do 2, o 1 continua ativo).
  assert(ajustes_copiar_perfil(2, 3) >= 0);
  c = dados_ler("ajustes-p3.txt");
  assert(linhaVale(c, AJ_VIDRO, 1) && linhaVale(c, AJ_HERO, 1) && !tem(c, AJ_RESOLUCAO));
  free(c);
  // Invalidos.
  assert(ajustes_copiar_perfil(1, 1) == -1 && ajustes_copiar_perfil(0, 2) == -1 && ajustes_copiar_perfil(7, 2) == -1);
  // Restaurar o padrao de um perfil que nao e o ativo: so as chaves dele.
  assert(ajustes_restaurar_padrao_perfil(2) > 0);
  a = dados_ler("ajustes-p2.txt");
  assert(linhaVale(a, AJ_VIDRO, valorPadrao[AJ_VIDRO]) && linhaVale(a, AJ_HERO, valorPadrao[AJ_HERO]));
  free(a);
  assert(valor[AJ_VIDRO] == 1);   // o ativo nao foi tocado
}

static void edicao(void) {
  char *t, *p2, *pend;
  // Ativo = 1 (vidro desligado, destaque desligado). O 2 tem vidro ligado, destaque ligado.
  valor[AJ_VIDRO] = 1; valor[AJ_HERO] = 1; valor[AJ_RELOGIO] = 0;
  { int d[AJ_N]; memcpy(d, valorPadrao, sizeof d); d[AJ_VIDRO] = 0; d[AJ_HERO] = 0; d[AJ_RELOGIO] = 1; perfilGravarVetor(2, d); }
  dados_apagar("ajustes-locais-p2.txt");
  assert(ajustes_edicao_iniciar(1) == 0);   // o ativo nao se edita como "outro"
  assert(ajustes_edicao_iniciar(2) == 1 && ajustes_edicao_perfil() == 2);
  assert(valor[AJ_VIDRO] == 0 && valor[AJ_HERO] == 0 && valor[AJ_RELOGIO] == 1);
  // Active-profile file editors must be unreachable while editing another profile.
  for (int op = AJ_LEG1_TAMANHO; op <= AJ_LEG1_COR_BORDA; op++) {
    assert(!ajOpcaoDisponivel(op));
    assert(!definirValorDireto(op, 1));
  }
  assert(!ajOpcaoDisponivel(AJ_LEG1_RESTAURAR));
  assert(!ajOpcaoDisponivel(AJ_FONTE_ORDEM));
  assert(!ajOpcaoDisponivel(AJ_FONTE_ADDONS_PERM));
  assert(!ajOpcaoDisponivel(AJ_FONTE_PLUGINS_PERM));
  assert(!ajOpcaoDisponivel(AJ_FONTE_REGEX_PADRAO));
  assert(!ajOpcaoDisponivel(AJ_FONTE_REGEX_MODELO));
  assert(!definirValorDireto(AJ_FONTE_REGEX_MODELO, 1));
  assert(ajustes_copiar_perfil(2, 3) == -1);   // sem copia no meio da edicao
  valor[AJ_HERO] = 1; valor[AJ_RELOGIO] = 0;
  assert(gravar());
  // O arquivo da TV continua com os valores do ativo; o do perfil editado levou a mudanca.
  t = dados_ler("ajustes.txt");
  assert(linhaVale(t, AJ_VIDRO, 1) && linhaVale(t, AJ_RELOGIO, 0) && linhaVale(t, AJ_HERO, 1) == 1);
  free(t);
  p2 = dados_ler("ajustes-p2.txt");
  assert(linhaVale(p2, AJ_HERO, 1) && linhaVale(p2, AJ_RELOGIO, 0) && linhaVale(p2, AJ_VIDRO, 0));
  free(p2);
  pend = dados_ler("ajustes-locais-p2.txt");
  assert(pend && pend[0] == '1');
  free(pend);
  // O blob da conta nao entra em valor[] enquanto ele e de outro perfil.
  assert(ajustes_aplicar_blob("{\"x\":1}") == 0);
  // guardar nao grava o valor[] do perfil editado como se fosse do ativo.
  dados_apagar("ajustes-p1.txt");
  ajustes_perfil_guardar(1);
  { char *p1 = dados_ler("ajustes-p1.txt"); assert(!p1); }
  ajustes_edicao_encerrar();
  assert(ajustes_edicao_perfil() == 0);
  assert(ajOpcaoDisponivel(AJ_FONTE_ORDEM));
  assert(ajOpcaoDisponivel(AJ_FONTE_ADDONS_PERM));
  assert(ajOpcaoDisponivel(AJ_FONTE_PLUGINS_PERM));
  assert(valor[AJ_VIDRO] == 1 && valor[AJ_HERO] == 1 && valor[AJ_RELOGIO] == 0);   // o ativo voltou inteiro
}

static void accountActionsIsolation(void) {
  const char *names[] = { "Admin", "Ana" };
  const int actions[] = { AJ_TRAKT, AJ_SIMKL, AJ_DISCORD, AJ_SAIR };
  perfis_teste_definir(names, NULL, 2, 1);
  assert(ajustes_edicao_iniciar(2));
  uxCancelar();
  for (int i = 0; i < 4; i++) {
    assert(!ajOpcaoDisponivel(actions[i]));
    uxAbrirEditor(actions[i]);
    assert(!uxEditor);
  }
  ajustes_edicao_encerrar();
  assert(perfis_ativo() == 1);
  for (int i = 0; i < 4; i++) assert(ajOpcaoDisponivel(actions[i]));
}

static void identityControlsIsolation(void) {
  const char *names[] = { "Admin", "Ana" };
  const int controls[] = { AJ_PERFIL_PESQ, AJ_PERFIL_EDITAR, AJ_PERFIL_INFANTIL };
  perfis_teste_definir(names, NULL, 2, 1);
  for (int initial = 0; initial <= 1; initial++) {
    assert(perfil_infantil_definir(1, initial));
    assert(ajustes_edicao_iniciar(2));
    uxCancelar();
    for (int i = 0; i < 3; i++) {
      int op = controls[i];
      assert(!ajOpcaoDisponivel(op));
      uxAbrirEditor(op);
      assert(!uxEditor);
      if (op != AJ_PERFIL_EDITAR)
        assert(!definirValorDireto(op, initial));
    }
    assert(!pessoas_aberto() && pfPend == PA_NADA);
    ajustes_edicao_encerrar();
    assert(perfis_ativo() == 1 && perfil_infantil(1) == initial);
    for (int i = 0; i < 3; i++) assert(ajOpcaoDisponivel(controls[i]));
  }
  perfil_infantil_esquecer();
}

static void pollOptoutIsolation(void) {
  const char *nomes[] = { "Admin", "Ana", "Other" };
  perfis_teste_definir(nomes, NULL, 3, 1);
  for (int initial = 0; initial <= 1; initial++) {
    char *pending;
    valor[AJ_ENQUETES] = initial;
    assert(dados_gravar("enquete-optout.txt", "3 1\n"));
    assert(ajustes_edicao_iniciar(2));
    valor[AJ_ENQUETES] = initial;
    int changed = definirValorDireto(AJ_ENQUETES, !initial);
    ajustes_edicao_encerrar();
    pending = dados_ler("enquete-optout.txt");
    assert(pending && !strcmp(pending, "3 1\n"));
    free(pending);
    assert(!changed && valor[AJ_ENQUETES] == initial);
    assert(perfis_ativo() == 1 && ajOpcaoDisponivel(AJ_ENQUETES));
  }
  dados_apagar("enquete-optout.txt");
}

static void failureBackendIsolation(void) {
  const char *files[] = { "fontefalha.txt", "fontefalha-test-p1.txt" };
  const char *markers[] = { "ajustes-locais.txt", "ajustes-locais-p1.txt", "ajustes-locais-p2.txt" };
  const char *pending[] = { "0\n", "0\n", "1\n" };
  const char *names[] = { "Admin", "Ana" };
  perfis_teste_definir(names, NULL, 2, 1);
  snprintf(faAddonEditar, sizeof faAddonEditar, "test-addon");
  for (int file = 0; file < 2; file++) {
    assert(fontefalha_arquivo(files[file]));
    assert(fontefalha_definir_provedores(1));
    char *before = dados_ler(files[file]);
    assert(before);
    for (int op = 0; op < AJ_N; op++) if (ajFalhaBackend(op)) {
      int initial = ajFalhaValor(op);
      for (int i = 0; i < 3; i++) assert(dados_gravar(markers[i], pending[i]));
      assert(ajustes_edicao_iniciar(2));
      int available = ajOpcaoDisponivel(op);
      int changed = definirValorDireto(op, initial ? 0 : 1);
      ajustes_edicao_encerrar();
      char *after = dados_ler(files[file]);
      assert(after && !strcmp(after, before));
      free(after);
      for (int i = 0; i < 3; i++) {
        after = dados_ler(markers[i]);
        assert(after && !strcmp(after, pending[i]));
        free(after);
      }
      assert(!available && !changed && ajFalhaValor(op) == initial);
      assert(perfis_ativo() == 1 && !ajustes_edicao_perfil());
      assert(ajOpcaoDisponivel(op));
    }
    free(before);
  }
  faAddonEditar[0] = 0;
  assert(fontefalha_arquivo("fontefalha.txt"));
}

static void failureDialogIsolation(void) {
  const char *names[] = { "Admin", "Ana", "Other" };
  const char *files[] = { "fontefalha.txt", "fontefalha-test-p1.txt" };
  perfis_teste_definir(names, NULL, 3, 1);
  for (int f = 0; f < 2; f++) {
    assert(fontefalha_arquivo(files[f]));
    assert(fontefalha_definir_provedores(1));
    const char *keys[] = { "addon-a", "addon-b" };
    fontefalha_addons_registrar(keys, 2);
    char *before = dados_ler(files[f]);
    assert(before);
    for (int dialog = 0; dialog < 3; dialog++) {
      assert(dados_gravar("ajustes-locais-p1.txt", "0\n"));
      assert(dados_gravar("ajustes-locais-p2.txt", "0\n"));
      assert(dados_gravar("ajustes-locais-p3.txt", "1\n"));
      assert(ajustes_edicao_iniciar(2));
      frAberta = 0;
      if (!dialog) {
        ffAbrir();
        ffCategoria = FONTEFILTRO_RESOLUCAO; ffValorFoco = 0;
        ffEscolher();
      } else {
        faAbrir(dialog == 1);
        if (frAberta) {
          if (dialog == 2) {
            faN = 2;
            for (int i = 0; i < 2; i++) {
              memset(&faItem[i], 0, sizeof faItem[i]);
              faItem[i].servico = -1;
              snprintf(faItem[i].chave, sizeof faItem[i].chave, "%s", keys[i]);
            }
          }
          faMover(0, 1);
        }
      }
      ajustes_edicao_encerrar();
      char *after = dados_ler(files[f]);
      assert(after && !strcmp(before, after)); free(after);
      for (int i = 1; i <= 3; i++) {
        char marker[64]; snprintf(marker, sizeof marker, "ajustes-locais-p%d.txt", i);
        after = dados_ler(marker);
        assert(after && !strcmp(after, i == 3 ? "1\n" : "0\n")); free(after);
      }
      assert(!frAberta && perfis_ativo() == 1);
    }
    free(before);
  }
  assert(fontefalha_arquivo("fontefalha.txt"));
}

static void providerSharingIsolation(void) {
  const char *names[] = { "Admin", "Ana" };
  const char *files[] = { "provider-scope-test-p1.txt", "provider-scope-test-p2.txt",
    "debrid-test-p1.txt", "debrid-test-p2.txt", "fontefalha-test-p1.txt", "fontefalha-test-p2.txt",
    "ajustes-locais-p1.txt", "ajustes-locais-p2.txt" };
  perfis_teste_definir(names, NULL, 2, 1);
  assert(dados_gravar("sessao.txt", "test.eyJzdWIiOiJ0ZXN0In0.sig\n\n0\n"));
  sessao_iniciar();
  assert(!strcmp(sessao_usuario(), "test"));
  for (int separate = 0; separate <= 1; separate++) {
    char *before[8], path[128], afterPath[128];
    assert(dados_gravar(files[0], separate ? "1\n" : "0\n"));
    assert(dados_gravar(files[1], "1\n"));
    assert(dados_gravar(files[2], ""));
    assert(dados_gravar(files[3], ""));
    assert(dados_gravar(files[5], "provedores 1\n"));
    assert(dados_gravar(files[6], "0\n"));
    assert(dados_gravar(files[7], "0\n"));
    ajustes_provedores_abrir_sessao();
    assert(ajustes_provedores_arquivo(path, sizeof path, "fontefalha"));
    FonteFalhaConfig config, afterConfig;
    fontefalha_config(&config);
    for (int i = 0; i < 8; i++) { before[i] = dados_ler(files[i]); assert(before[i]); }
    assert(ajustes_edicao_iniciar(2));
    int available = ajOpcaoDisponivel(AJ_PROVEDORES_ESCOPO);
    int changed = definirValorDireto(AJ_PROVEDORES_ESCOPO, !separate);
    ajustes_edicao_encerrar();
    for (int i = 0; i < 8; i++) {
      char *after = dados_ler(files[i]);
      assert(after && !strcmp(after, before[i]));
      free(after); free(before[i]);
    }
    fontefalha_config(&afterConfig);
    assert(!memcmp(&config, &afterConfig, sizeof config));
    assert(ajustes_provedores_arquivo(afterPath, sizeof afterPath, "fontefalha"));
    assert(!strcmp(path, afterPath));
    assert(!available && !changed && perfis_ativo() == 1);
    assert(valor[AJ_PROVEDORES_ESCOPO] == separate);
    assert(ajOpcaoDisponivel(AJ_PROVEDORES_ESCOPO));
  }
}

static void admin(void) {
  char h[65], *arq;
  admin_sha256_hex((const unsigned char *)"abc", 3, h);
  assert(!strcmp(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  admin_pin_esquecer();
  assert(!admin_pin_definido() && !admin_pin_conferir("1234"));
  assert(!admin_pin_definir("12") && !admin_pin_definir("12a45") && !admin_pin_definir("123456789"));
  assert(admin_pin_definir("4821") && admin_pin_definido());
  assert(admin_pin_conferir("4821") && !admin_pin_conferir("4822") && !admin_pin_conferir("") && !admin_pin_conferir(NULL));
  arq = dados_ler("admin-pin.txt");
  assert(arq && !strstr(arq, "4821"));   // nunca em claro
  free(arq);
  assert(admin_pin_definir("90210") && admin_pin_conferir("90210") && !admin_pin_conferir("4821"));
  ajustes_perfil_esquecer();   // sair da conta apaga o PIN e o marcador de escopo
  assert(!admin_pin_definido());
}

static void gate(void) {
  static const char *const nomes[] = { "Admin", "Guest", "Kid" };
  static const int pins[] = { 1, 0, 0 };
  PfLinha linhas[PF_MAX];
  perfis_teste_definir(nomes, NULL, 3, 1);
  perfil_infantil_esquecer();
  admin_pin_esquecer();
  pfEntrada();
  focoIndice = 1; uxTopo = AJ2_T_PERFIL;
  assert(uxEvento(SDLK_RETURN) && pfTela == PF_MENU && ajModalAberto());
  pfExigirAdmin(PA_EDITAR, 2);
  assert(pfFase == 3 && !edPerfil);
  assert(pfDigitado("4821") == 1 && pfFase == 4);
  pfDigitado("4822");
  assert(!admin_pin_definido() && !edPerfil);
  pfExigirAdmin(PA_EDITAR, 2);
  assert(pfDigitado("4821") == 1);
  pfDigitado("4821");
  assert(admin_pin_definido() && edPerfil == 2);
  pfSaida();
  pfEntrada();
  pfExigirAdmin(PA_EDITAR, 2);
  assert(pfFase == 2 && !edPerfil);
  pfDigitado("0000");
  assert(!edPerfil && !pfAdminOk);
  pfExigirAdmin(PA_EDITAR, 2);
  pfDigitado("4821");
  assert(edPerfil == 2);
  pfSaida();
  // Account PIN takes precedence over the TV PIN. Cancel never authorizes.
  perfis_teste_definir(nomes, pins, 3, 1);
  pfEntrada(); pfExigirAdmin(PA_EDITAR, 2);
  assert(pfFase == 1 && !edPerfil);
  pfPinCancelado(); assert(!pfPend && !edPerfil);
  // A late verification result cannot authorize a new visit.
  admPerfil = 1; atomic_store(&admEstado, 2); pfSaida(); pfRecolher();
  assert(!pfAdminOk && !edPerfil);
  perfis_teste_definir(nomes, NULL, 3, 3);
  assert(perfil_infantil_definir(3, 1) && perfil_ajustes_travar(3, 1));
  pfEntrada(); pfVigiar();
  assert(pfTela == PF_BLOQUEIO);
  pfExigirAdmin(PA_EDITAR, 2); assert(!pfPend && !edPerfil);
  pfMudarInfantil(0); assert(perfil_infantil_ativo() && !pfPend);
  pfEvento(SDLK_ESCAPE); assert(sair); sair = 0;
  pfExigirAdmin(PA_DESBLOQ, 0); pfDigitado("0000");
  assert(pfTravado());
  pfExigirAdmin(PA_DESBLOQ, 0); pfDigitado("4821");
  assert(!pfTravado());
  pfSaida(); pfEntrada(); assert(pfTravado());
  pfAbrir();
  int n = pfLinhas(linhas);
  for (int i = 0; i < n; i++)
    assert(linhas[i].id != PL_EDITAR_OUTRO && linhas[i].id != PL_COPIAR && linhas[i].id != PL_INFANTIL);
  pfSaida(); perfil_infantil_esquecer();
  perfis_teste_definir(nomes, NULL, 3, 1);
}

static void infantil(void) {
  perfil_infantil_esquecer();
  assert(!perfil_ajustes_travados(2));
  assert(perfil_ajustes_travar(2, 1) && !perfil_ajustes_travados(2));   // so vale com a marca de infantil
  assert(perfil_infantil_definir(2, 1) && perfil_ajustes_travados(2) && !perfil_ajustes_travados(3));
  assert(perfil_ajustes_travar(2, 0) && !perfil_ajustes_travados(2));
  perfil_ajustes_travar(2, 1);
  perfil_infantil_esquecer();
  assert(!perfil_infantil(2) && !perfil_ajustes_travados(2));
  // Padroes seguros no perfil infantil (nao ativo).
  { int d[AJ_N]; memcpy(d, valorPadrao, sizeof d); perfilGravarVetor(2, d); }
  ajustes_padroes_infantis(2);
  { char *t = dados_ler("ajustes-p2.txt");
    assert(linhaVale(t, AJ_ENQUETES, 1) && linhaVale(t, AJ_SEEKR_LIGADO, 1) && linhaVale(t, AJ_NT_IMDB, 1) && linhaVale(t, AJ_PLR_CLASSIF, 0));
    free(t); }
}

int main(void) {
  char dir[] = "/tmp/nuvio-aj-escopo-XXXXXX";
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(dir);
  ajustes_dir(dir);
  escopos();
  blobIgnoraLocal();
  migracao();
  copia();
  edicao();
  accountActionsIsolation();
  identityControlsIsolation();
  pollOptoutIsolation();
  failureBackendIsolation();
  failureDialogIsolation();
  providerSharingIsolation();
  admin();
  infantil();
  gate();
  puts("ajustes_escopo: tudo ok");
  return 0;
}
