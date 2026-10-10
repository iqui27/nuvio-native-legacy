/* Busca e comparacao locais: nao inicia UI, rede ou sincronizacao. */
#include "../src/ajustes.c"
#include <assert.h>
#include <limits.h>
#include <unistd.h>

static AjusteBuscaResultado resultados[AJ_N];

static int indiceResultado(int op, int n) {
  int i;
  for (i = 0; i < n; i++) if (resultados[i].op == op) return i;
  return -1;
}

/* #221 sobreviveu a reorganizacao da tela: default, escopo local, prazo
 * efetivo, entrada unica em Fontes e addons (era Reproducao ate 2.0.1) e
 * descoberta pela busca. */
static void prazoDosAddonsIntegrado(void) {
  int prazoAntes = valor[AJ_FONTE_PRAZO], manualAntes = valor[AJ_FONTE_MANUAL];
  int idiomaAntes = valor[AJ_IDIOMA], vezes = 0, n, indice;
  const char *categoria = "", *grupo = "";
  char texto[80];
  assert(valor[AJ_FONTE_PRAZO] == 2 && valorPadrao[AJ_FONTE_PRAZO] == 2);
  assert(ajustes_fonte_prazo_ms() == 5000);
  assert(!dePerfil(AJ_FONTE_PRAZO));
  assert(!strcmp(uxEscopo(AJ_FONTE_PRAZO), "Só nesta TV"));
  assert(uxTemPadrao(AJ_FONTE_PRAZO) && !uxDiferente(AJ_FONTE_PRAZO));
  assert(OPCOES[AJ_FONTE_PRAZO].n == 7);   /* #202: Instantaneo, 3, 5, 8, 15, 30 s, Todos */
  assert(!strcmp(OPCOES[AJ_FONTE_PRAZO].valores[2], "5 s"));
  assert(familiaPreviaOpcao(AJ_FONTE_PRAZO) == AJPV_REPRO);
  for (int i = 0; i < AJ_N_TELA; i++) {
    if (TELA[i].tipo == IT_SEC) { categoria = TELA[i].titulo; grupo = ""; }
    if (TELA[i].tipo == IT_ROT) grupo = TELA[i].titulo;
    if (TELA[i].tipo == IT_OPC && TELA[i].op == AJ_FONTE_PRAZO) {
      vezes++;
      assert(!strcmp(categoria, "Reprodução"));   // 2.0.3: a escolha da fonte mora em Reproducao
      assert(!strcmp(grupo, "Escolha da fonte"));
    }
  }
  assert(vezes == 1);
  valor[AJ_IDIOMA] = IDIOMA_PT + 1;
  valor[AJ_FONTE_MANUAL] = 1; // escolha automatica habilita o prazo
  n = ajustes_buscar("Espera pelos add-ons", resultados, AJ_N);
  indice = indiceResultado(AJ_FONTE_PRAZO, n);
  assert(indice >= 0 && !resultados[indice].bloqueado);
  assert(strstr(resultados[indice].caminho, "Reprodução"));
  assert(!strcmp(resultados[indice].valor, "5 s"));
  vezes = 0;
  for (int i = 0; i < n; i++) if (resultados[i].op == AJ_FONTE_PRAZO) vezes++;
  assert(vezes == 1);
  static const int esperado[] = {0, 3000, 5000, 8000, 15000, 30000, -1};
  for (int i = 0; i < 7; i++) {
    valor[AJ_FONTE_PRAZO] = i;
    assert(ajustes_fonte_prazo_ms() == esperado[i]);
    uxValorTexto(AJ_FONTE_PRAZO, i, texto, sizeof texto);
    assert(!strcmp(texto, i18n(V_FONTE_PRAZO[i])));
  }
  valor[AJ_FONTE_PRAZO] = prazoAntes;
  valor[AJ_FONTE_MANUAL] = manualAntes;
  valor[AJ_IDIOMA] = idiomaAntes;
  assert(ajustes_fonte_prazo_ms() == 5000);
}

static void escoposPorValor(void) {
  int copia[AJ_N], v, temaSalvo = valor[AJ_TEMA];
  int seguroAntes = SEGURO, corAntes = ajustes_cor_viva();
  float legendaAntes = legendaEspera;
  char audioAntes[32], legAntes[32];
  snprintf(audioAntes, sizeof audioAntes, "%s", ling_audio());
  snprintf(legAntes, sizeof legAntes, "%s", ling_legenda());
  memcpy(copia, valor, sizeof copia);
  assert(dePerfil(AJ_TEMA));
  for (v = 0; v < AJ_N_TEMAS_OPC; v++)
    assert(!strcmp(uxEscopoValor(AJ_TEMA, v),
      v >= AJ_TEMA_DINAMICA ? "Só nesta TV" : "Conta/perfil"));
  assert(!strcmp(uxEscopoValor(AJ_TMDB_IDIOMA, 0), "Só nesta TV"));
  assert(!strcmp(uxEscopoValor(AJ_TMDB_IDIOMA, 1), "Conta/perfil"));
  /* "Da conta" muda a origem efetiva, nao retira a escolha do perfil. */
  assert(dePerfil(AJ_LEG_LINGUA) && dePerfil(AJ_AUD_LINGUA));
  assert(!strcmp(uxEscopoValor(AJ_LEG_LINGUA, 0), "Conta/perfil"));
  assert(!strcmp(uxEscopoValor(AJ_AUD_LINGUA, 0), "Conta/perfil"));
  assert(!strcmp(uxEscopoValor(AJ_LEG_LINGUA, LING_OPC_ORIGINAL), "Conta/perfil"));
  assert(!strcmp(uxEscopoValor(AJ_AUD_LINGUA, LING_OPC_ORIGINAL), "Conta/perfil"));
  assert(!strcmp(uxEscopo(-1), "Só nesta TV"));
  assert(!strcmp(uxEscopo(AJ_N), "Só nesta TV"));
  /* Consultar candidatos/padrao nao restaura, aplica ou altera preferencias. */
  assert(!memcmp(copia, valor, sizeof copia));
  assert(SEGURO == seguroAntes && ajustes_cor_viva() == corAntes);
  assert(legendaEspera == legendaAntes);
  assert(!strcmp(audioAntes, ling_audio()) && !strcmp(legAntes, ling_legenda()));
  valor[AJ_TEMA] = AJ_TEMA_DINAMICA;
  assert(!strcmp(uxEscopo(AJ_TEMA), "Só nesta TV"));
  // O padrao da 2.0 e a Imersiva, que e desta TV (a conta nao a conhece).
  assert(!strcmp(uxEscopoValor(AJ_TEMA, uxValorPadrao(AJ_TEMA)), "Só nesta TV"));
  assert(!strcmp(uxEscopoValor(AJ_TEMA, 0), "Conta/perfil"));
  assert(valor[AJ_TEMA] == AJ_TEMA_DINAMICA);
  valor[AJ_TEMA] = AJ_TEMA_DINAMICA - 1;
  assert(!strcmp(uxEscopo(AJ_TEMA), "Conta/perfil"));
  valor[AJ_TEMA] = temaSalvo;
  assert(!memcmp(copia, valor, sizeof copia));
}

static void padroesEValores(void) {
  int op, copia[AJ_N];
  char texto[160], pequeno[4], normal[80];
  assert(sizeof valor == sizeof valorPadrao);
  assert(!memcmp(valor, valorPadrao, sizeof valor));
  for (op = 0; op < AJ_N; op++) {
    assert(!uxDiferente(op));
    if (OPCOES[op].tipo == OP_ACAO || OPCOES[op].tipo == OP_LEITURA ||
        CHAVE[op][0] == '-' || op == AJ_PERFIL_PESQ)
      assert(!uxTemPadrao(op));
    else {
      assert(uxTemPadrao(op));
      assert(uxValorPadrao(op) == valorPadrao[op]);
    }
  }
  assert(!uxTemPadrao(-1) && !uxTemPadrao(AJ_N));
  assert(!uxDiferente(-1) && !uxDiferente(AJ_N));
  valor[AJ_FOCO_TRAILER] = 1 - valorPadrao[AJ_FOCO_TRAILER];
  assert(uxDiferente(AJ_FOCO_TRAILER));
  assert(uxValorPadrao(AJ_FOCO_TRAILER) == 1);
  valor[AJ_FOCO_TRAILER] = valorPadrao[AJ_FOCO_TRAILER];
  /* Um valor que o modo seguro suspende continua sendo a preferencia salva. */
  valor[AJ_RESOLUCAO] = RES_4K;
  SEGURO = 1;
  assert(uxDiferente(AJ_RESOLUCAO));
  assert(!ajustes_4k());
  uxValorTexto(AJ_RESOLUCAO, valor[AJ_RESOLUCAO], texto, sizeof texto);
  assert(strstr(texto, "4K"));
  SEGURO = 0;
  valor[AJ_RESOLUCAO] = valorPadrao[AJ_RESOLUCAO];
  assert(!strcmp(uxEscopo(AJ_IDIOMA), "Só nesta TV"));
  assert(!strcmp(uxEscopo(AJ_RESOLUCAO), "Só nesta TV"));
  assert(!strcmp(uxEscopo(AJ_VIDRO), "Só nesta TV"));
  assert(!strcmp(uxEscopo(AJ_AUD_LINGUA), "Conta/perfil"));
  assert(!strcmp(uxEscopo(AJ_FOCO_TRAILER), "Conta/perfil"));
  assert(!strstr(uxEscopo(AJ_FOCO_TRAILER), "sincronizado"));

  memcpy(copia, valor, sizeof copia);
  uxValorTexto(AJ_FOCO_TRAILER, 0, texto, sizeof texto);
  assert(!strcmp(texto, i18n("Ligado")));
  uxValorTexto(AJ_AUD_LINGUA, LING_OPC_ORIGINAL, texto, sizeof texto);
  assert(!strcmp(texto, i18n("Original do título")));
  uxValorTexto(AJ_HERO_TRAILER_ESPERA, 22, texto, sizeof texto);
  assert(strstr(texto, "2,2") || strstr(texto, "2.2"));
  uxValorTexto(AJ_SEEKR_AJUSTE, -5, texto, sizeof texto);
  assert(!strcmp(texto, "-5 s"));
  uxValorTexto(AJ_FIL_LIMITE, 12, texto, sizeof texto);
  assert(!strcmp(texto, "12"));
  uxValorTexto(AJ_FOCO_TRAILER, INT_MAX, texto, sizeof texto);
  assert(!strcmp(texto, i18n("Ligado")));
  uxValorTexto(AJ_N, 0, texto, sizeof texto);
  assert(!texto[0]);
  uxValorTexto(AJ_IDIOMA, 0, NULL, 0);
  assert(!memcmp(copia, valor, sizeof copia));
  uxTextoCopiar(pequeno, sizeof pequeno, "áéí");
  assert(!strcmp(pequeno, "á"));
  uxNormalizar("  MEMO\xCC\x81RIA / ÁUDIO · AÇÃO  ", normal, sizeof normal);
  assert(!strcmp(normal, "memoria audio acao"));
  uxNormalizar("\xF0\x9F", normal, sizeof normal);
  assert(!normal[0]);
}

static void buscaECaminhos(void) {
  int n, i, copia[AJ_N];
  AjusteBuscaResultado primeiro, limitado[2];
  char longa[2048];
  valor[AJ_IDIOMA] = IDIOMA_PT + 1;
  memcpy(copia, valor, sizeof copia);
  n = ajustes_buscar("MEMÓRIA PARA IMAGENS", resultados, AJ_N);
  assert(n > 0 && resultados[0].op == AJ_TEX_MB);
  assert(resultados[0].avancado);
  assert(strstr(resultados[0].caminho, "Esta TV"));
  primeiro = resultados[0];
  n = ajustes_buscar("memo\xCC\x81ria para imagens", resultados, AJ_N);
  assert(n > 0 && !memcmp(&primeiro, &resultados[0], sizeof primeiro));
  n = ajustes_buscar("DUBLADO", resultados, AJ_N);
  assert(n > 0 && resultados[0].op == AJ_AUD_LINGUA);
  assert(strstr(resultados[0].caminho, "Idiomas e legendas"));
  n = ajustes_buscar("CC", resultados, AJ_N);
  assert(n > 0 && resultados[0].op == AJ_LEG_LINGUA);
  n = ajustes_buscar("subtitle", resultados, AJ_N);
  assert(n > 0 && resultados[0].op == AJ_LEG_LINGUA);
  n = ajustes_buscar("travando", resultados, AJ_N);
  assert(indiceResultado(AJ_VIDRO, n) >= 0);
  assert(indiceResultado(AJ_TEX_MB, n) >= 0);
  n = ajustes_buscar("trailer", resultados, AJ_N);
  assert(indiceResultado(AJ_HERO_TRAILER, n) >= 0);
  assert(indiceResultado(AJ_FOCO_TRAILER, n) >= 0);
  assert(indiceResultado(AJ_TRAILER_QUAL, n) >= 0);
  for (i = 0; i < n; i++) {
    int j;
    assert(resultados[i].titulo[0] && resultados[i].caminho[0]);
    for (j = 0; j < i; j++) assert(resultados[i].op != resultados[j].op);
  }
  memset(limitado, 0xa5, sizeof limitado);
  primeiro = limitado[1];
  assert(ajustes_buscar("trailer", limitado, 1) == 1);
  assert(!memcmp(&primeiro, &limitado[1], sizeof primeiro));
  assert(ajustes_buscar("trailer", NULL, AJ_N) == 0);
  assert(ajustes_buscar("trailer", resultados, 0) == 0);
  assert(ajustes_buscar("trailer", resultados, -5) == 0);
  assert(ajustes_buscar("", resultados, AJ_N) == 0);
  assert(ajustes_buscar("  \t  ", resultados, AJ_N) == 0);
  assert(ajustes_buscar(NULL, resultados, AJ_N) == 0);
  assert(ajustes_buscar("\xF0\x9F", resultados, AJ_N) == 0);
  memset(longa, 'z', sizeof longa - 1); longa[sizeof longa - 1] = 0;
  assert(ajustes_buscar(longa, resultados, AJ_N) == 0);
  assert(!memcmp(copia, valor, sizeof copia));
}

static void bloqueadosESegredos(void) {
  int n, i, copia[AJ_N];
  const char *marcador = "sentinelaqzprivadaux2026";
  char texto[160];
  valor[AJ_HERO_TRAILER] = 1;
  valor[AJ_TMDB_LIGADO] = 1;
  memcpy(copia, valor, sizeof copia);
  n = ajustes_buscar("som do trailer no destaque", resultados, AJ_N);
  i = indiceResultado(AJ_HERO_TRAILER_SOM, n);
  assert(i >= 0 && resultados[i].bloqueado);
  assert(strstr(resultados[i].caminho, "Cartazes e trailers"));
  n = ajustes_buscar("idioma dos metadados", resultados, AJ_N);
  i = indiceResultado(AJ_TMDB_IDIOMA, n);
  assert(i >= 0 && resultados[i].bloqueado);
  /* A build de teste nao configura o servico social. */
  assert(!recomenda_ativo());
  n = ajustes_buscar("perfil", resultados, AJ_N);
  assert(indiceResultado(AJ_PERFIL_PESQ, n) < 0);
  assert(indiceResultado(AJ_PERFIL_EDITAR, n) < 0);

  snprintf(fanartChave, sizeof fanartChave, "%s", marcador);
  snprintf(seekrChave, sizeof seekrChave, "%s", marcador);
  snprintf(pstToken, sizeof pstToken, "%s", marcador);
  snprintf(pstInst, sizeof pstInst, "https://%s.example.invalid", marcador);
  snprintf(pstModelo, sizeof pstModelo, "https://%s.example.invalid/{imdb}", marcador);
  snprintf(p2pEndereco, sizeof p2pEndereco, "http://%s.example.invalid", marcador);
  xtream_definir_usuario(marcador);
  /* Prova que a ajuda legada realmente contem um valor privado nesta fixture. */
  assert(strstr(ajudaOpcao(AJ_XTREAM_USUARIO), marcador));
  assert(ajustes_buscar(marcador, resultados, AJ_N) == 0);
  n = ajustes_buscar("usuário Xtream", resultados, AJ_N);
  i = indiceResultado(AJ_XTREAM_USUARIO, n);
  assert(i >= 0 && !strcmp(resultados[i].valor, "Abrir"));
  n = ajustes_buscar("chave", resultados, AJ_N);
  assert(indiceResultado(AJ_FANART_CHAVE, n) >= 0);
  assert(indiceResultado(AJ_SEEKR_CHAVE, n) >= 0);
  for (i = 0; i < n; i++) {
    assert(!strstr(resultados[i].valor, marcador));
    assert(!strstr(resultados[i].titulo, marcador));
    assert(!strstr(resultados[i].caminho, marcador));
  }
  uxValorTexto(AJ_XTREAM_USUARIO, 0, texto, sizeof texto);
  assert(!strcmp(texto, "Abrir"));
  uxValorTexto(AJ_PERFIL_ATIVO, 0, texto, sizeof texto);
  assert(!strcmp(texto, "Ver detalhes"));
  assert(!memcmp(copia, valor, sizeof copia));
  xtream_esquecer();
}

/* F07: seek cache option. Appended, local, default off; on a TV without an
 * app-controlled disk cache (this host build, LG, Samsung) the row is
 * inactive, says so and the accessor never asks the backend for a cache. */
static void zoomTpk(void) {
  int i, vezes = 0;
  { int n = 0;   // F06: sincronia pelo audio so na tela do Android
    for (i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_OPC && TELA[i].op == AJ_LEG_SYNC_AUDIO) n++;
#ifdef NV_ANDROID
    assert(n == 1);
#else
    assert(n == 0 && !ajustes_legenda_sync_audio());
#endif
  }
  assert(!strcmp(CHAVE[AJ_TRAILER_ZOOM_TPK], "trailerZoomTpkLocal"));
  assert(valorPadrao[AJ_TRAILER_ZOOM_TPK] == 1);   // Desligado
  assert(!strcmp(OPCOES[AJ_TRAILER_ZOOM_TPK].valores[1], "Desligado"));
  assert(somenteDesteAparelho(AJ_TRAILER_ZOOM_TPK) && !dePerfil(AJ_TRAILER_ZOOM_TPK));
  for (i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_OPC && TELA[i].op == AJ_TRAILER_ZOOM_TPK) vezes++;
#ifdef NV_TPK
  assert(vezes == 1);
#else
  assert(vezes == 0);   // hidden outside the native .tpk
#endif
}
static void cacheSeek(void) {
  int antes = valor[AJ_CACHE_SEEK], vezes = 0, n, i;
  const char *categoria = "";
  assert(!strcmp(CHAVE[AJ_CACHE_SEEK], "cacheSeekLocal"));
  assert(valorPadrao[AJ_CACHE_SEEK] == 0 && OPCOES[AJ_CACHE_SEEK].n == 4);
  assert(!strcmp(OPCOES[AJ_CACHE_SEEK].valores[0], "Desligado"));
  assert(!strcmp(OPCOES[AJ_CACHE_SEEK].valores[3], "1 GB"));
  assert(!dePerfil(AJ_CACHE_SEEK) && somenteDesteAparelho(AJ_CACHE_SEEK));
  assert(!strcmp(uxEscopo(AJ_CACHE_SEEK), "Só nesta TV"));
  assert(familiaPreviaOpcao(AJ_CACHE_SEEK) == AJPV_REPRO);
  for (i = 0; i < AJ_N_TELA; i++) {
    if (TELA[i].tipo == IT_SEC) categoria = TELA[i].titulo;
    if (TELA[i].tipo == IT_OPC && TELA[i].op == AJ_CACHE_SEEK) { vezes++; assert(!strcmp(categoria, "Reprodução")); }
  }
  assert(vezes == 1);
  assert(inativa(AJ_CACHE_SEEK));
  assert(!strcmp(textoValor(AJ_CACHE_SEEK), "Não disponível nesta TV"));
  assert(strstr(ajudaOpcao(AJ_CACHE_SEEK), "Não disponível nesta TV"));
  for (i = 0; i < 4; i++) { valor[AJ_CACHE_SEEK] = i; assert(ajustes_cache_seek_mb() == 0); }
  valor[AJ_CACHE_SEEK] = antes;
  n = ajustes_buscar("cache de seek", resultados, AJ_N);
  i = indiceResultado(AJ_CACHE_SEEK, n);
  assert(i >= 0 && resultados[i].bloqueado);
}

// Cada categoria e cada submenu (ROT) tem a SUA arte: nenhum indice de
// AJ_ARTE_SEC se repete, todo bloco de ajustes_ux_tela.inc tem coluna, e a
// opcao devolve a arte do bloco dela (o primeiro bloco fica com a da categoria).
static void artePorSubmenu(void) {
  int visto[40] = { 0 }, s, g, i;
  montarTela();
  assert(nSecoes == AJS_N && nSecoes == 11);
  for (s = 0; s < AJS_N; s++) {
    int blocos = 0, ultimo = -1, basicos = 0;
    assert(AJ_ARTE_SEC[s][0] >= 0);
    // 2.0.3: so os blocos basicos (antes do MAIS) tem arte unica; os de
    // "Mais opcoes" repetem a do bloco basico de mesmo nome.
    for (i = secIni[s] + 1; i < secFim(s) && TELA[i].tipo != IT_MAIS; i++) if (TELA[i].tipo == IT_ROT) basicos++;
    for (g = 0; g < AJ_ARTE_BLOCOS && g < (basicos ? basicos : 1); g++) {
      int n = AJ_ARTE_SEC[s][g];
      if (n < 0) continue;
      assert(n < 40 && !visto[n]);
      visto[n] = 1;
    }
    for (i = secIni[s] + 1; i < secFim(s); i++) {
      if (TELA[i].tipo == IT_ROT) { blocos++; continue; }
      if (TELA[i].tipo != IT_OPC) continue;
      g = blocos > 0 ? blocos - 1 : 0;
      assert(g < AJ_ARTE_BLOCOS && AJ_ARTE_SEC[s][g] >= 0);
      assert(ajCenaArteOp(TELA[i].op) == AJ_ARTE_SEC[s][g]);
      if (g != ultimo) { assert(g == ultimo + 1); ultimo = g; }
    }
  }
  // A mesma imagem nao pede troca; submenu ou categoria diferente pede.
  assert(ajCenaChave(AJS_REPRODUCAO, AJ_DV) != ajCenaChave(AJS_REPRODUCAO, AJ_ATMOS));  /* 2.0.2: one generated scene per option */
  assert(ajCenaChave(AJS_REPRODUCAO, AJ_DV) != ajCenaChave(AJS_REPRODUCAO, AJ_PAUSA_OVERLAY));
  // O primeiro bloco tem cena propria (o que ele controla), diferente da
  // visao geral da categoria; cada bloco com cena tem a sua.
  assert(ajCenaChave(AJS_FONTES, AJ_FONTE_MANUAL) != ajCenaChave(AJS_FONTES, -1));
  assert(ajCenaChave(AJS_FONTES, AJ_FONTE_MANUAL) != ajCenaChave(AJS_FONTES, AJ_FONTE_PRAZO));
  assert(ajCenaChave(AJS_FONTES, AJ_ADDONS) != ajCenaChave(AJS_FONTES, AJ_FONTE_MANUAL));
  { int vistoC[AJC_N] = { 0 }, ultimaC = -1, k2;
    for (k2 = 0; k2 < AJ_N_TELA; k2++) {
      int c;
      if (TELA[k2].tipo != IT_OPC || uxAvancada(TELA[k2].op)) continue;   // "Mais opcoes" repete as cenas do bloco basico
      c = ajCenaSecao(TELA[k2].op);
      if (c < 0 || c == ultimaC) continue;
      assert(c < AJC_N && (!vistoC[c] || TELA[k2].op == AJ_TMDB_IDIOMA));   // um bloco, uma cena: nada repetido em outro lugar
      vistoC[c] = 1; ultimaC = c;
    } }
  assert(ajCenaChave(AJS_CONTAS, -1) != ajCenaChave(AJS_CARTAZES, -1));
  assert(ajCenaChave(AJS_HOME, AJ_CW_LIGADO) != ajCenaChave(AJS_HOME, AJ_CW_ORDEM));
}

// 2.0.2: a reorganizacao dos Ajustes. Categorias pela tarefa (Fontes e addons
// separada da Conta), as opcoes mais usadas a dois degraus (categoria > linha,
// basicas), as que mudaram de lugar onde devem estar, a busca achando pelo
// nome antigo e a estrutura traduzida de verdade (nao o ingles copiado).
static const char *secaoDe(int op, const char **grupo) {
  const char *sec = "", *g = "";
  int i;
  for (i = 0; i < AJ_N_TELA; i++) {
    if (TELA[i].tipo == IT_SEC) { sec = TELA[i].titulo; g = ""; }
    if (TELA[i].tipo == IT_ROT) g = TELA[i].titulo;
    if (TELA[i].tipo == IT_OPC && TELA[i].op == op) { if (grupo) *grupo = g; return sec; }
  }
  return NULL;
}
static void reorganizacao202(void) {
  static const char *const ORDEM[11] = {
    "Reprodução", "Idiomas e legendas", "Fontes e addons", "TV ao vivo",
    "Tela inicial", "Aparência", "Página do título", "Cartazes e trailers",
    "Conta e serviços", "Esta TV", "Sobre e ajuda" };
  static const struct { int op; const char *sec; } COMUNS[] = {
    { AJ_HOME_LAYOUT, "Tela inicial" }, { AJ_FIL_ORDEM, "Tela inicial" },
    { AJ_HERO_TRAILER, "Cartazes e trailers" }, { AJ_TEMA, "Aparência" }, { AJ_TAMANHO_UI, "Aparência" },
    { AJ_QUALIDADE, "Reprodução" }, { AJ_DV, "Reprodução" }, { AJ_PAUSA_OVERLAY, "Reprodução" },
    { AJ_ADDONS, "Fontes e addons" }, { AJ_FONTE_MANUAL, "Reprodução" }, { AJ_FONTE_PRIORIDADE, "Reprodução" },
    { AJ_IDIOMA, "Idiomas e legendas" }, { AJ_AUD_LINGUA, "Idiomas e legendas" }, { AJ_LEG_LINGUA, "Idiomas e legendas" },
    { AJ_TRAKT, "Conta e serviços" }, { AJ_PERFIL_ATIVO, "Conta e serviços" }, { AJ_SYNC, "Conta e serviços" },
    { AJ_ATUALIZAR, "Sobre e ajuda" },
  };
  static const struct { int op; const char *sec, *grp; } MUDARAM[] = {
    { AJ_PLUGINS, "Fontes e addons", "Addons" }, { AJ_ADDONS_PRINCIPAL, "Fontes e addons", "Addons" },
    { AJ_BUSCA_CINEMETA, "Fontes e addons", "Addons" }, { AJ_TAM_MAX, "Reprodução", "Escolha da fonte" },
    { AJ_DEBRID_RD, "Fontes e addons", "Debrid" }, { AJ_P2P_LIGADO, "Reprodução", "P2P" },
    { AJ_JF_LIGADO, "Fontes e addons", "Servidores pessoais" },
    { AJ_MDB_CHAVE, "Página do título", "Notas" }, { AJ_FANART_CHAVE, "Cartazes e trailers", "De onde vem a arte" },
    { AJ_LEG_LINGUA2, "Idiomas e legendas", "Segunda legenda" },
  };
  int i, s = 0, n, idiomaAntes = valor[AJ_IDIOMA];
  const char *g;
  montarTela();
  for (i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_SEC) assert(!strcmp(TELA[i].titulo, ORDEM[s++]));
  assert(s == 11);
  for (i = 0; i < (int)(sizeof COMUNS / sizeof *COMUNS); i++) {
    const char *sec = secaoDe(COMUNS[i].op, NULL);
    assert(sec && !strcmp(sec, COMUNS[i].sec));
    assert(!uxAvancada(COMUNS[i].op));   // a vista sem ligar "Avancadas"
  }
  for (i = 0; i < (int)(sizeof MUDARAM / sizeof *MUDARAM); i++) {
    const char *sec = secaoDe(MUDARAM[i].op, &g);
    assert(sec && !strcmp(sec, MUDARAM[i].sec) && !strcmp(g, MUDARAM[i].grp));
  }
  // A busca acha pelo lugar antigo e pelo rotulo antigo.
  valor[AJ_IDIOMA] = IDIOMA_PT + 1;
  n = ajustes_buscar("contas", resultados, AJ_N);
  assert(indiceResultado(AJ_TRAKT, n) >= 0 && indiceResultado(AJ_ADDONS, n) >= 0);
  n = ajustes_buscar("chaves", resultados, AJ_N);
  assert(indiceResultado(AJ_DEBRID_RD, n) >= 0 && indiceResultado(AJ_MDB_CHAVE, n) >= 0);
  n = ajustes_buscar("addons", resultados, AJ_N);
  assert(indiceResultado(AJ_ADDONS, n) >= 0);
  // 2.0.3: a escolha da fonte voltou para Reproducao; acha pelo lugar da 2.0.2.
  i = indiceResultado(AJ_FONTE_MANUAL, ajustes_buscar("fontes e addons", resultados, AJ_N));
  assert(i >= 0 && strstr(resultados[i].caminho, "Reprodução"));
  assert(indiceResultado(AJ_HERO_TRAILER, ajustes_buscar("cartazes e arte trailer", resultados, AJ_N)) < 0 ||
         indiceResultado(AJ_TRAILER_QUAL, ajustes_buscar("trailers qualidade", resultados, AJ_N)) >= 0);
  assert(indiceResultado(AJ_RESOLUCAO, ajustes_buscar("desempenho desta tv", resultados, AJ_N)) >= 0);
  n = ajustes_buscar("idioma", resultados, AJ_N);
  assert(n > 0 && resultados[0].op == AJ_IDIOMA);
  assert(!strcmp(rotuloOpcao(AJ_IDIOMA), "Idioma do app"));
  // Toda categoria, subtitulo, bloco e frase da estrutura traduzidos em cada
  // idioma: nem o portugues, nem o ingles copiado (o que o alemao mostrava).
  { int lg, copiados = 0;
    for (lg = IDIOMA_RO; lg < IDIOMA_N; lg++) {
      if (lg == IDIOMA_PTPT) continue;   // portugues europeu: o texto pode coincidir com a chave
      for (i = 0; i < AJ_N_TELA; i++) {
        const char *t = TELA[i].tipo == IT_SEC || TELA[i].tipo == IT_ROT ? TELA[i].titulo : NULL;
        const char *en;
        // Uma palavra so pode coincidir de verdade ("Style", "Trailer"); frase nao.
        if (!t || !strchr(t, ' ')) continue;
        valor[AJ_IDIOMA] = IDIOMA_EN + 1; en = i18n(t);
        valor[AJ_IDIOMA] = lg + 1;
        if (!strcmp(i18n(t), en)) { printf("FALHA: \"%s\" no idioma %d e o ingles copiado\n", t, lg); copiados++; }
      }
    }
    assert(copiados == 0); }
  valor[AJ_IDIOMA] = idiomaAntes;
}

/* #202: o "Auto-play streams" do oficial e a migracao da espera. */
static void autoplay202(const char *dir) {
  // #202: o auto-play do oficial depois do Apoiar. Escopo e regex por PERFIL
  // (o escopo com a chave do oficial na conta); listas e padrao sao acoes.
  assert(AJ_FONTE_ESCOPO == AJ_PLR_CLASSIF + 1 && AJ_FONTE_REGEX_MODELO == AJ_FONTE_ESCOPO + 6);
  assert(!strcmp(CHAVE[AJ_FONTE_ESCOPO], "streamAutoPlaySource") && OPCOES[AJ_FONTE_ESCOPO].n == 3 && valorPadrao[AJ_FONTE_ESCOPO] == 0);
  { char sn[64]; camelParaSnake(CHAVE[AJ_FONTE_ESCOPO], sn, sizeof sn); assert(!strcmp(sn, "stream_auto_play_source")); }
  assert(dePerfil(AJ_FONTE_ESCOPO) && dePerfil(AJ_FONTE_OUTROS) && dePerfil(AJ_FONTE_REGEX));
  assert(!dePerfil(AJ_FONTE_REGEX_MODELO) && !dePerfil(AJ_FONTE_ADDONS_PERM));
  assert(valorPadrao[AJ_FONTE_OUTROS] == 0 && ajustes_fonte_usar_outros());     /* Ligado de fabrica */
  assert(valorPadrao[AJ_FONTE_REGEX] == 0 && ajustes_fonte_regex_modo() == FR_REGEX_DESLIGADA);
  assert(OPCOES[AJ_FONTE_REGEX_MODELO].n == fonteregra_modelos());
  for (int u = AJ_FONTE_ESCOPO; u <= AJ_FONTE_REGEX_MODELO; u++) {
    const char *cat = "", *grp = ""; int vz = 0, k;
    for (k = 0; k < AJ_N_TELA; k++) {
      if (TELA[k].tipo == IT_SEC) cat = TELA[k].titulo;
      if (TELA[k].tipo == IT_ROT) grp = TELA[k].titulo;
      if (TELA[k].tipo == IT_OPC && TELA[k].op == u) { vz++; assert(!strcmp(cat, "Reprodução") && !strcmp(grp, "Escolha da fonte")); }
    }
    assert(vz == 1 && familiaPreviaOpcao(u) == AJPV_REPRO && ajudaOpcao(u)[0]);
  }
  assert(indiceResultado(AJ_FONTE_ADDONS_PERM, ajustes_buscar("permitidos", resultados, AJ_N)) >= 0);
  assert(indiceResultado(AJ_FONTE_REGEX, ajustes_buscar("regex", resultados, AJ_N)) >= 0);
  // A conta: o escopo pelo enum do oficial (sem caixa), ida e volta.
  { int e0 = valor[AJ_FONTE_ESCOPO]; char *out = NULL;
    const char *blob = "{\"version\":1,\"features\":{\"player_settings\":{\"stream_auto_play_source\":{\"type\":\"string\",\"value\":\"INSTALLED_ADDONS_ONLY\"}}}}";
    ajustes_aplicar_blob(blob);
    assert(ajustes_fonte_escopo() == FR_ESCOPO_ADDONS);
    valor[AJ_FONTE_ESCOPO] = FR_ESCOPO_PLUGINS;
    assert(ajustes_mesclar_blob(blob, &out) >= 1 && out && strstr(out, "\"value\":\"ENABLED_PLUGINS_ONLY\""));
    free(out);
    valor[AJ_FONTE_ESCOPO] = e0; }
  // Modelo de regex: espelho do padrao; escolher um troca o padrao.
  fonteregra_definir_regex(fonteregra_modelo(4));
  assert(fonteregra_modelo_atual() == 4);
  fonteregra_definir_regex("");
  // "Espera pelos add-ons": o indice antigo (fontePrazoLocal) migra para a
  // lista nova; com a chave nova no arquivo, ela vale.
  { static const struct { const char *arq; int esperado; } C[] = {
      { "fontePrazoLocal 0\n", 1 }, { "fontePrazoLocal 1\n", 2 }, { "fontePrazoLocal 2\n", 3 },
      { "fontePrazoLocal 3\n", 6 }, { "fontePrazoLocal 3\nfonteEsperaLocal 0\n", 0 },
    };
    char cam[600]; FILE *f; int k, p0 = valor[AJ_FONTE_PRAZO];
    for (k = 0; k < (int)(sizeof C / sizeof *C); k++) {
      snprintf(cam, sizeof cam, "%s/ajustes.txt", dir);
      f = fopen(cam, "w"); assert(f); fputs(C[k].arq, f); fclose(f);
      valor[AJ_FONTE_PRAZO] = valorPadrao[AJ_FONTE_PRAZO];
      ajustes_dir(dir);
      assert(valor[AJ_FONTE_PRAZO] == C[k].esperado);
    }
    assert(ajustes_fonte_prazo_ms() == 0);           /* o ultimo: Instantaneo */
    valor[AJ_FONTE_PRAZO] = p0; remove(cam); }
}

int main(void) {
  char dir[] = "/tmp/nuvio-aj-ux-dados-XXXXXX";
  assert(AJ_DISCORD == AJ_ICONE_APP + 1 && AJ_TAMANHO_AJUSTES == AJ_DISCORD + 1 && AJ_LOGO_TRAILER == AJ_TAMANHO_AJUSTES + 1 && AJ_LEG_LINGUA2 == AJ_LOGO_TRAILER + 1 && AJ_LEG_SYNC_AUDIO == AJ_LEG_LINGUA2 + 1 && AJ_CACHE_SEEK == AJ_LEG_SYNC_AUDIO + 1 && AJ_TRAILER_ZOOM_TPK == AJ_CACHE_SEEK + 1 && AJ_PLUGINS == AJ_TRAILER_ZOOM_TPK + 1 && AJ_JF_LIGADO == AJ_PLUGINS + 1 && AJ_JF_SAIR == AJ_PLUGINS + 4 && AJ_AVANCADAS == AJ_JF_SAIR + 1 && AJ_LEG2_POS == AJ_AVANCADAS + 1 && AJ_EM_SERVIDOR == AJ_LEG2_BORDA + 1 && AJ_PX_SAIR == AJ_EM_SERVIDOR + 5 && AJ_FONTE_PRIORIDADE == AJ_PX_SAIR + 1 && AJ_FONTE_HDR == AJ_ABERTURA - 2 && AJ_LOGO_APP == AJ_FONTE_HDR + 1 && AJ_ENQUETES == AJ_ABERTURA + 1 && AJ_ENQUETES == AJ_FONTE_ESCOPO - 14);
  assert(!strcmp(CHAVE[AJ_LOGO_APP], "logoAppLocal") && !strcmp(CHAVE[AJ_ABERTURA], "aberturaAppLocal") && OPCOES[AJ_LOGO_APP].n == 2 && OPCOES[AJ_ABERTURA].n == 3 && valorPadrao[AJ_LOGO_APP] == 0 && valorPadrao[AJ_ABERTURA] == 0 && somenteDesteAparelho(AJ_LOGO_APP) && somenteDesteAparelho(AJ_ABERTURA));
  { int u; for (u = AJ_LOGO_APP; u <= AJ_ABERTURA; u++) { int vz = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == u) vz++; assert(vz == 1); } }
  assert(!strcmp(CHAVE[AJ_FONTE_PRIORIDADE], "fontePrioridadeLocal") && !strcmp(CHAVE[AJ_FONTE_HDR], "fonteHdrLocal") && OPCOES[AJ_FONTE_PRIORIDADE].n == 3 && OPCOES[AJ_FONTE_HDR].n == 3 && valorPadrao[AJ_FONTE_PRIORIDADE] == 0 && valorPadrao[AJ_FONTE_HDR] == 0 && somenteDesteAparelho(AJ_FONTE_HDR));
  // N3: "Receber enquetes" is the LAST option: local, On by default, in "Notificações".
  assert(AJ_ENQUETES == AJ_ABERTURA + 1 && AJ_ENQUETES == AJ_FONTE_ESCOPO - 14);
  assert(!strcmp(CHAVE[AJ_ENQUETES], "enquetesLocal") && OPCOES[AJ_ENQUETES].n == 2 && valorPadrao[AJ_ENQUETES] == 0);
  assert(somenteDesteAparelho(AJ_ENQUETES) && !dePerfil(AJ_ENQUETES));
  { int vezesE = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_ENQUETES) vezesE++; assert(vezesE == 1); }
  assert(indiceResultado(AJ_ENQUETES, ajustes_buscar("enquete", resultados, AJ_N)) >= 0);
  /* #231: Cinemeta fora da busca. Ultima opcao, local, Ligado de fabrica (comportamento de antes). */
  assert(AJ_BUSCA_CINEMETA == AJ_ENQUETES + 1 && AJ_BUSCA_CINEMETA == AJ_FONTE_ESCOPO - 13);
  assert(!strcmp(CHAVE[AJ_BUSCA_CINEMETA], "buscaCinemetaLocal") && OPCOES[AJ_BUSCA_CINEMETA].n == 2);
  assert(valorPadrao[AJ_BUSCA_CINEMETA] == 0 && ajustes_busca_cinemeta());
  assert(somenteDesteAparelho(AJ_BUSCA_CINEMETA) && !dePerfil(AJ_BUSCA_CINEMETA));
  { int vz = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_BUSCA_CINEMETA) vz++; assert(vz == 1); }
  /* #311: sem o add-on Cinemeta instalado (o teste nao tem add-ons) o interruptor Ligado nao aparece. */
  assert(indiceResultado(AJ_BUSCA_CINEMETA, ajustes_buscar("cinemeta", resultados, AJ_N)) < 0);
  /* #311: Nuvio na busca (Primeiro/Por ultimo/Desligado) e a fonte nos grupos: locais, no padrao de antes. */
  /* Fim do enum na 2.0.3: FONTE_PREPARAR, CW_PROXIMO (upnext), BUSCA_NUVIO, BUSCA_ORIGEM (#311), LAYOUT_AJUSTES (#339). */
  assert(AJ_CW_PROXIMO == AJ_FONTE_PREPARAR + 1 && AJ_BUSCA_NUVIO == AJ_CW_PROXIMO + 1);
  assert(AJ_BUSCA_ORIGEM == AJ_BUSCA_NUVIO + 1 && AJ_LAYOUT_AJUSTES == AJ_BUSCA_ORIGEM + 1);
  assert(!strcmp(CHAVE[AJ_BUSCA_NUVIO], "buscaNuvioLocal") && OPCOES[AJ_BUSCA_NUVIO].n == 3 && valorPadrao[AJ_BUSCA_NUVIO] == 0);
  assert(!strcmp(CHAVE[AJ_BUSCA_ORIGEM], "buscaOrigemLocal") && OPCOES[AJ_BUSCA_ORIGEM].n == 2 && valorPadrao[AJ_BUSCA_ORIGEM] == 0);
  assert(ajustes_busca_nuvio() == 0 && ajustes_busca_origem());
  assert(somenteDesteAparelho(AJ_BUSCA_NUVIO) && somenteDesteAparelho(AJ_BUSCA_ORIGEM));
  assert(indiceResultado(AJ_BUSCA_NUVIO, ajustes_buscar("nuvio catalogo", resultados, AJ_N)) >= 0);
  assert(indiceResultado(AJ_BUSCA_ORIGEM, ajustes_buscar("fonte resultados", resultados, AJ_N)) >= 0);
  /* OLED: esmaecer quando parado (padrao 5 min) e brilho da interface do player (padrao 80%). Ultimas, locais. */
  assert(AJ_ESMAECER == AJ_BUSCA_CINEMETA + 1 && AJ_BRILHO_PLAYER == AJ_FONTE_ESCOPO - 11 && AJ_BRILHO_PLAYER == AJ_ESMAECER + 1);
  assert(!strcmp(CHAVE[AJ_ESMAECER], "esmaecerLocal") && !strcmp(CHAVE[AJ_BRILHO_PLAYER], "brilhoPlayerLocal"));
  assert(OPCOES[AJ_ESMAECER].n == 6 && OPCOES[AJ_BRILHO_PLAYER].n == 4);   /* 2.0: Desligado, 30 s, 1, 2, 5, 10 min */
  assert(valorPadrao[AJ_ESMAECER] == 3 && valorPadrao[AJ_BRILHO_PLAYER] == 1);
  assert(ajustes_esmaecer() == 3 && ajustes_brilho_player() == 1);
  assert(somenteDesteAparelho(AJ_ESMAECER) && somenteDesteAparelho(AJ_BRILHO_PLAYER));
  { int vE = 0, vB = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC) { vE += TELA[k].op == AJ_ESMAECER; vB += TELA[k].op == AJ_BRILHO_PLAYER; } assert(vE == 1 && vB == 1); }
  assert(indiceResultado(AJ_ESMAECER, ajustes_buscar("oled", resultados, AJ_N)) >= 0);
  /* 2.0: "Novidades 2.0" e a ULTIMA opcao: acao em Sobre e ajuda, sem chave gravada. */
  assert(AJ_NOVIDADES20 == AJ_BRILHO_PLAYER + 1 && AJ_NOVIDADES20 == AJ_FONTE_ESCOPO - 10);
  assert(!strcmp(CHAVE[AJ_NOVIDADES20], "-novidades20") && valorPadrao[AJ_NOVIDADES20] == 0);
  { int vz = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_NOVIDADES20) vz++; assert(vz == 1); }
  assert(indiceResultado(AJ_NOVIDADES20, ajustes_buscar("novidades", resultados, AJ_N)) >= 0);
  /* Retomada (05/10): "Manter o video pronto ao sair" e a ULTIMA opcao. Avancada, local,
     DESLIGADA de fabrica (instalacao antiga sem a chave = nao retem), nunca no perfil seguro,
     e so com a saida para a home valendo. */
  assert(AJ_MANTER_VIDEO == AJ_NOVIDADES20 + 1 && AJ_MANTER_VIDEO == AJ_FONTE_ESCOPO - 9);
  /* 2.0: tela de descanso (estilo e fonte da vitrine), LOCAIS, no fim. */
  assert(AJ_DESCANSO_ESTILO == AJ_MANTER_VIDEO + 1 && AJ_DESCANSO_FONTE == AJ_RELOGIO_12H - 1);
  // Formato do relogio (2.0): local, no fim, padrao 24 h.
  assert(AJ_RELOGIO_12H == AJ_FONTE_ESCOPO - 6 && !strcmp(CHAVE[AJ_RELOGIO_12H], "relogio12hLocal") && valorPadrao[AJ_RELOGIO_12H] == 0);
  // Faixa de tamanho da escolha automatica: as DUAS ultimas, locais, padrao "Sem limite" (0 GB).
  assert(AJ_TAM_MAX == AJ_RELOGIO_12H + 1 && AJ_TAM_MIN == AJ_FONTE_ESCOPO - 4 && AJ_TAM_MIN == AJ_TAM_MAX + 1);
  assert(!strcmp(CHAVE[AJ_TAM_MAX], "tamanhoMaxLocal") && !strcmp(CHAVE[AJ_TAM_MIN], "tamanhoMinLocal"));
  assert(valorPadrao[AJ_TAM_MAX] == 0 && valorPadrao[AJ_TAM_MIN] == 0 && OPCOES[AJ_TAM_MAX].n == 7 && OPCOES[AJ_TAM_MIN].n == 7);
  // Espaco da Home: duas NUM locais depois do Apoiar, padrao 100 %, preso em 50..150.
  assert(AJ_ESPACO_FILEIRAS == AJ_FONTE_REGEX_MODELO + 1 && AJ_ESPACO_TITULOS == AJ_FONTE_ESCOPO + 8 && AJ_ESPACO_TITULOS == AJ_ESPACO_FILEIRAS + 1);
  assert(!strcmp(CHAVE[AJ_ESPACO_FILEIRAS], "espacoFileirasLocal") && !strcmp(CHAVE[AJ_ESPACO_TITULOS], "espacoTitulosLocal"));
  assert(valorPadrao[AJ_ESPACO_FILEIRAS] == 100 && valorPadrao[AJ_ESPACO_TITULOS] == 100);
  assert(somenteDesteAparelho(AJ_ESPACO_FILEIRAS) && somenteDesteAparelho(AJ_ESPACO_TITULOS));
  assert(ajustes_espaco_fileiras() == 1.0f && ajustes_espaco_titulos() == 1.0f);
  assert(ajustes_espaco_fator(0) == 0.5f && ajustes_espaco_fator(50) == 0.5f && ajustes_espaco_fator(100) == 1.0f);
  assert(ajustes_espaco_fator(150) == 1.5f && ajustes_espaco_fator(900) == 1.5f && ajustes_espaco_fator(-5) == 0.5f);
  // Apoiar o projeto (apoio.h): acao no fim, sem valor, uma vez na tela.
  assert(AJ_APOIAR == AJ_FONTE_ESCOPO - 3 && AJ_APOIAR == AJ_TAM_MIN + 1 && !strcmp(CHAVE[AJ_APOIAR], "-apoiar") && valorPadrao[AJ_APOIAR] == 0);
  assert(AJ_PLR_CLASSIF == AJ_LEG_FORCADA + 1 && AJ_PLR_CLASSIF == AJ_FONTE_ESCOPO - 1 && !strcmp(CHAVE[AJ_PLR_CLASSIF], "classifPlayerLocal") && valorPadrao[AJ_PLR_CLASSIF] == 0);
  assert(AJ_PROPORCAO_PADRAO == AJ_FONTE_ESCOPO + 9 && AJ_PROPORCAO_PADRAO == AJ_ESPACO_TITULOS + 1 && !strcmp(CHAVE[AJ_PROPORCAO_PADRAO], "proporcaoPadraoLocal") && valorPadrao[AJ_PROPORCAO_PADRAO] == 0 && OPCOES[AJ_PROPORCAO_PADRAO].n == 9 && ajustes_proporcao_padrao() == -1 && somenteDesteAparelho(AJ_PROPORCAO_PADRAO) && !dePerfil(AJ_PROPORCAO_PADRAO));
  // 2.0.2: "Tambem em Continuar assistindo". Local, last, default OFF (= one title, one place), once on screen.
  assert(AJ_CW_RETIDO_TAMBEM == AJ_PROPORCAO_PADRAO + 1 && AJ_CW_RETIDO_TAMBEM == AJ_FONTE_ESCOPO + 10 && AJ_DV_MKV == AJ_FONTE_ESCOPO + 13);
  assert(!strcmp(CHAVE[AJ_CW_RETIDO_TAMBEM], "cwRetidoTambemLocal") && OPCOES[AJ_CW_RETIDO_TAMBEM].n == 2 && valorPadrao[AJ_CW_RETIDO_TAMBEM] == 1);
  assert(somenteDesteAparelho(AJ_CW_RETIDO_TAMBEM) && !dePerfil(AJ_CW_RETIDO_TAMBEM));
  { int vz = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_CW_RETIDO_TAMBEM) vz++; assert(vz == 1); }
  // 2.0.2: "Dolby Vision em MKV (experimental)". Local, last, default OFF, once on screen (webOS/Mac).
  assert(!strcmp(CHAVE[AJ_DV_MKV], "dvMkvLocal") && OPCOES[AJ_DV_MKV].n == 2 && valorPadrao[AJ_DV_MKV] == 1 && !ajustes_dv_mkv());
  assert(somenteDesteAparelho(AJ_DV_MKV) && !dePerfil(AJ_DV_MKV));
  { int vz = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_DV_MKV) vz++; assert(vz == 1); }
  // 2.0.2: "Enviar histórico para a conta Nuvio" and "Recursos sociais". Per PROFILE on this
  // TV (ajustes-p<N>.txt) but never in the account blob; default ON; last; once on screen.
  assert(AJ_HIST_CONTA == AJ_FONTE_ESCOPO + 14 && AJ_SOCIAL == AJ_FONTE_ESCOPO + 15);
  assert(!strcmp(CHAVE[AJ_HIST_CONTA], "histContaLocal") && !strcmp(CHAVE[AJ_SOCIAL], "socialLocal"));
  assert(OPCOES[AJ_HIST_CONTA].n == 2 && OPCOES[AJ_SOCIAL].n == 2 && valorPadrao[AJ_HIST_CONTA] == 0 && valorPadrao[AJ_SOCIAL] == 0);
  assert(ajustes_hist_conta() && ajustes_social());
  assert(somenteDesteAparelho(AJ_HIST_CONTA) && dePerfil(AJ_HIST_CONTA) && somenteDesteAparelho(AJ_SOCIAL) && dePerfil(AJ_SOCIAL));
  assert(somenteDesteAparelho(AJ_CW_FONTE) && dePerfil(AJ_CW_FONTE) && somenteDesteAparelho(AJ_SALVOS_DEST) && dePerfil(AJ_SALVOS_DEST));
  { int a = 0, b = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC) { a += TELA[k].op == AJ_HIST_CONTA; b += TELA[k].op == AJ_SOCIAL; } assert(a == 1 && b == 1); }
  // 2.0.2: ordem dos add-ons no automatico: acao + "Usar a ordem" (Nao | Para desempatar | Ordem estrita).
  // Por perfil, so desta TV, logo depois de AJ_SOCIAL, uma vez na tela, achavel.
  assert(AJ_FONTE_ORDEM == AJ_FONTE_ESCOPO + 16 && AJ_FONTE_ORDEM_USO == AJ_FONTE_ESCOPO + 17 && AJ_FONTE_ORDEM == AJ_SOCIAL + 1);
  assert(!strcmp(CHAVE[AJ_FONTE_ORDEM], "-fonteOrdem") && !strcmp(CHAVE[AJ_FONTE_ORDEM_USO], "fonteOrdemUsoLocal"));
  assert(OPCOES[AJ_FONTE_ORDEM].tipo == OP_ACAO && OPCOES[AJ_FONTE_ORDEM_USO].n == 3 && valorPadrao[AJ_FONTE_ORDEM_USO] == 0);
  assert(!strcmp(OPCOES[AJ_FONTE_ORDEM_USO].valores[2], "Ordem estrita"));
  assert(dePerfil(AJ_FONTE_ORDEM_USO) && !dePerfil(AJ_FONTE_ORDEM) && ajustes_fonte_ordem_uso() == 0);
  { int a = 0, b = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC) { a += TELA[k].op == AJ_FONTE_ORDEM; b += TELA[k].op == AJ_FONTE_ORDEM_USO; } assert(a == 1 && b == 1); }
  { const char *ordem[2] = { "A", "B" }; int antes = valor[AJ_FONTE_ORDEM_USO];
    valor[AJ_FONTE_ORDEM_USO] = 2;
    assert(ajustes_fonte_ordem_uso() == 0);       /* sem ordem definida, o modo nao age */
    fonteregra_ordem_definir(ordem, 2);
    assert(ajustes_fonte_ordem_uso() == 2);
    fonteregra_ordem_definir(NULL, 0);
    valor[AJ_FONTE_ORDEM_USO] = antes; }
  assert(indiceResultado(AJ_FONTE_ORDEM, ajustes_buscar("ordem add-ons", resultados, AJ_N)) >= 0);
  // 2.0.2: start-up speed of a stream, under Escolha da fonte. Local, after the add-on order
  // options, once on screen, findable.
  // Play while checking the first source: default ON.
  assert(AJ_FONTE_TOCAR_CONFERINDO == AJ_FONTE_ESCOPO + 18 && AJ_FONTE_TOCAR_CONFERINDO == AJ_FONTE_ORDEM_USO + 1);
  assert(!strcmp(CHAVE[AJ_FONTE_TOCAR_CONFERINDO], "fonteTocarConferindoLocal") && OPCOES[AJ_FONTE_TOCAR_CONFERINDO].n == 2);
  assert(valorPadrao[AJ_FONTE_TOCAR_CONFERINDO] == 0 && ajustes_fonte_tocar_conferindo());
  assert(somenteDesteAparelho(AJ_FONTE_TOCAR_CONFERINDO) && !dePerfil(AJ_FONTE_TOCAR_CONFERINDO));
  { int vz = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_FONTE_TOCAR_CONFERINDO) vz++; assert(vz == 1); }
  assert(indiceResultado(AJ_FONTE_TOCAR_CONFERINDO, ajustes_buscar("tocar enquanto confere", resultados, AJ_N)) >= 0);
  // Warm connections when a title opens: default ON.
  assert(AJ_FONTE_AQUECER == AJ_FONTE_ESCOPO + 19 && AJ_FONTE_AQUECER == AJ_FONTE_TOCAR_CONFERINDO + 1);
  assert(!strcmp(CHAVE[AJ_FONTE_AQUECER], "fonteAquecerLocal") && OPCOES[AJ_FONTE_AQUECER].n == 2);
  assert(valorPadrao[AJ_FONTE_AQUECER] == 0 && ajustes_fonte_aquecer());
  assert(somenteDesteAparelho(AJ_FONTE_AQUECER) && !dePerfil(AJ_FONTE_AQUECER));
  { int vz = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_FONTE_AQUECER) vz++; assert(vz == 1); }
  assert(indiceResultado(AJ_FONTE_AQUECER, ajustes_buscar("aquecer conexoes", resultados, AJ_N)) >= 0);
  // Check several sources at once: default ON (only sources already cached on the debrid run together).
  assert(AJ_FONTE_CONFERIR_VARIAS == AJ_FONTE_ESCOPO + 20 && AJ_FONTE_CONFERIR_VARIAS == AJ_FONTE_AQUECER + 1);
  assert(!strcmp(CHAVE[AJ_FONTE_CONFERIR_VARIAS], "fonteConferirVariasLocal") && OPCOES[AJ_FONTE_CONFERIR_VARIAS].n == 2);
  assert(valorPadrao[AJ_FONTE_CONFERIR_VARIAS] == 0 && ajustes_fonte_conferir_varias());
  assert(somenteDesteAparelho(AJ_FONTE_CONFERIR_VARIAS) && !dePerfil(AJ_FONTE_CONFERIR_VARIAS));
  { int vz = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_FONTE_CONFERIR_VARIAS) vz++; assert(vz == 1); }
  assert(indiceResultado(AJ_FONTE_CONFERIR_VARIAS, ajustes_buscar("conferir varias fontes", resultados, AJ_N)) >= 0);
  // Prepare the source when a title opens: default OFF (adds the file to the debrid panel even if not watched).
  assert(AJ_FONTE_PREPARAR == AJ_FONTE_ESCOPO + 21 && AJ_FONTE_PREPARAR == AJ_FONTE_CONFERIR_VARIAS + 1);
  assert(!strcmp(CHAVE[AJ_FONTE_PREPARAR], "fontePrepararLocal") && OPCOES[AJ_FONTE_PREPARAR].n == 2);
  assert(valorPadrao[AJ_FONTE_PREPARAR] == 1 && !ajustes_fonte_preparar());
  assert(somenteDesteAparelho(AJ_FONTE_PREPARAR) && !dePerfil(AJ_FONTE_PREPARAR));
  { int vz = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_FONTE_PREPARAR) vz++; assert(vz == 1); }
  assert(indiceResultado(AJ_FONTE_PREPARAR, ajustes_buscar("preparar fonte", resultados, AJ_N)) >= 0);
  // #339: Settings layout. Local, last, default Painel (today's grid), once on screen, findable.
  assert(AJ_LAYOUT_AJUSTES == AJ_BUSCA_ORIGEM + 1 && AJ_LAYOUT_AJUSTES == AJ_P2P_LIMITE - 1);
  assert(!strcmp(CHAVE[AJ_LAYOUT_AJUSTES], "ajustesLayoutLocal") && OPCOES[AJ_LAYOUT_AJUSTES].n == 2);
  assert(valorPadrao[AJ_LAYOUT_AJUSTES] == 0 && !ajustes_layout_lista());
  assert(somenteDesteAparelho(AJ_LAYOUT_AJUSTES) && !dePerfil(AJ_LAYOUT_AJUSTES));
  { int vz = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_LAYOUT_AJUSTES) vz++; assert(vz == 1); }
  assert(indiceResultado(AJ_LAYOUT_AJUSTES, ajustes_buscar("lista", resultados, AJ_N)) >= 0);
  // #334: limite de espaco do P2P, local, Automatico por padrao.
  assert(AJ_P2P_LIMITE == AJ_LAYOUT_AJUSTES + 1 && AJ_FONTE_ORDEM_ADDON == AJ_P2P_LIMITE + 1);
  assert(!strcmp(CHAVE[AJ_P2P_LIMITE], "p2pLimiteLocal") && OPCOES[AJ_P2P_LIMITE].n == 5);
  assert(valorPadrao[AJ_P2P_LIMITE] == 0 && ajustes_p2p_limite_mb() == 0);
  assert(somenteDesteAparelho(AJ_P2P_LIMITE) && !dePerfil(AJ_P2P_LIMITE));
  { int antes = valor[AJ_P2P_LIMITE]; valor[AJ_P2P_LIMITE] = 4; assert(ajustes_p2p_limite_mb() == 16384);
    valor[AJ_P2P_LIMITE] = 9; assert(ajustes_p2p_limite_mb() == 0); valor[AJ_P2P_LIMITE] = antes; }
  { int vz = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_P2P_LIMITE) vz++; assert(vz == 1); }
  assert(indiceResultado(AJ_P2P_LIMITE, ajustes_buscar("limite p2p", resultados, AJ_N)) >= 0);
  // #400: acrescentado no fim, sem deslocar os indices antigos.
  assert(AJ_LEG_SYNC_AUTO == AJ_FONTE_ORDEM_ADDON + 1 && AJ_N == AJ_LEG_SYNC_AUTO + 1);
  assert(!strcmp(CHAVE[AJ_FONTE_ORDEM_ADDON], "fonteOrdemLocal"));
  assert(OPCOES[AJ_FONTE_ORDEM_ADDON].n == 2 && valorPadrao[AJ_FONTE_ORDEM_ADDON] == 0);
  assert(somenteDesteAparelho(AJ_FONTE_ORDEM_ADDON) && !dePerfil(AJ_FONTE_ORDEM_ADDON));
  { int vz = 0; for (int k = 1; k < AJ_N_TELA; k++)
      if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_FONTE_ORDEM_ADDON) {
        assert(TELA[k - 1].op == AJ_FONTE_TEXTO); vz++;
      }
    assert(vz == 1); }
  assert(indiceResultado(AJ_FONTE_ORDEM_ADDON, ajustes_buscar("ordem fontes", resultados, AJ_N)) >= 0);
  // #303: "Continuar na escolha de perfil". Local, last, default ON (= today), once on screen, findable.
  assert(AJ_PS_CONTINUAR == AJ_CW_RETIDO_TAMBEM + 1 && AJ_PS_CONTINUAR == AJ_FONTE_ESCOPO + 11);
  assert(!strcmp(CHAVE[AJ_PS_CONTINUAR], "psContinuarLocal") && OPCOES[AJ_PS_CONTINUAR].n == 2 && valorPadrao[AJ_PS_CONTINUAR] == 0 && ajustes_ps_continuar());
  assert(somenteDesteAparelho(AJ_PS_CONTINUAR) && !dePerfil(AJ_PS_CONTINUAR));
  { int vz = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_PS_CONTINUAR) vz++; assert(vz == 1); }
  assert(indiceResultado(AJ_PS_CONTINUAR, ajustes_buscar("continuar escolha", resultados, AJ_N)) >= 0);
  // DTS convertido em: local, last, default Estereo (AAC) = today, once on screen, findable.
  assert(AJ_DTS_AC3 == AJ_PS_CONTINUAR + 1 && AJ_DTS_AC3 == AJ_FONTE_ESCOPO + 12);
  assert(!strcmp(CHAVE[AJ_DTS_AC3], "dtsSaidaLocal") && OPCOES[AJ_DTS_AC3].n == 2 && valorPadrao[AJ_DTS_AC3] == 0 && !ajustes_dts_ac3());
  assert(somenteDesteAparelho(AJ_DTS_AC3) && !dePerfil(AJ_DTS_AC3));
  { int vz = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_DTS_AC3) vz++; assert(vz == 1); }
  assert(indiceResultado(AJ_DTS_AC3, ajustes_buscar("dolby digital", resultados, AJ_N)) >= 0);
  { int vz = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_APOIAR) vz++; assert(vz == 1); }
  // #287: forced subtitle when the audio is in your language. Local, last, default ON, once on screen.
  assert(AJ_LEG_FORCADA == AJ_FONTE_ESCOPO - 2 && AJ_LEG_FORCADA == AJ_APOIAR + 1 && !strcmp(CHAVE[AJ_LEG_FORCADA], "legendaForcadaLocal"));
  assert(valorPadrao[AJ_LEG_FORCADA] == 0 && ajustes_legenda_forcada_auto() && somenteDesteAparelho(AJ_LEG_FORCADA));
  { int vz = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_LEG_FORCADA) vz++; assert(vz == 1); }
  assert(somenteDesteAparelho(AJ_TAM_MAX) && somenteDesteAparelho(AJ_TAM_MIN));
  assert(ajustes_tamanho_max_gb() == 0 && ajustes_tamanho_min_gb() == 0);
  assert(!strcmp(CHAVE[AJ_DESCANSO_ESTILO], "descansoEstiloLocal") && !strcmp(CHAVE[AJ_DESCANSO_FONTE], "descansoFonteLocal"));
  assert(valorPadrao[AJ_DESCANSO_ESTILO] == 0 && valorPadrao[AJ_DESCANSO_FONTE] == 0);
  assert(somenteDesteAparelho(AJ_DESCANSO_ESTILO) && somenteDesteAparelho(AJ_DESCANSO_FONTE));
  assert(!strcmp(CHAVE[AJ_MANTER_VIDEO], "manterVideoLocal") && OPCOES[AJ_MANTER_VIDEO].n == 2);
  assert(valorPadrao[AJ_MANTER_VIDEO] == 1 && !ajustes_manter_video());
  assert(somenteDesteAparelho(AJ_MANTER_VIDEO) && !dePerfil(AJ_MANTER_VIDEO) && uxAvancada(AJ_MANTER_VIDEO));
  assert(familiaPreviaOpcao(AJ_MANTER_VIDEO) == AJPV_INTERFACE);
  { int vz = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == AJ_MANTER_VIDEO) vz++; assert(vz == 1); }
  { int m0 = valor[AJ_MANTER_VIDEO], r0 = valor[AJ_RELOGIO], s0 = valor[AJ_SAIDA_PLAYER], ps0 = perfilSeguro;
    valor[AJ_RELOGIO] = 0; valor[AJ_SAIDA_PLAYER] = 0; valor[AJ_MANTER_VIDEO] = 0; perfilSeguro = 0;
    assert(ajustes_manter_video() && !inativa(AJ_MANTER_VIDEO));
    perfilSeguro = 1; assert(!ajustes_manter_video()); perfilSeguro = 0;
    valor[AJ_SAIDA_PLAYER] = 1; assert(!ajustes_manter_video() && inativa(AJ_MANTER_VIDEO));
    valor[AJ_SAIDA_PLAYER] = 0; valor[AJ_RELOGIO] = 1; assert(!ajustes_manter_video() && inativa(AJ_MANTER_VIDEO));
    valor[AJ_MANTER_VIDEO] = m0; valor[AJ_RELOGIO] = r0; valor[AJ_SAIDA_PLAYER] = s0; perfilSeguro = ps0; }
  assert(strstr(ajudaOpcao(AJ_MANTER_VIDEO), "2 minutos") && strstr(ajudaOpcao(AJ_MANTER_VIDEO), "trailer"));
  assert(indiceResultado(AJ_MANTER_VIDEO, ajustes_buscar("retomar", resultados, AJ_N)) >= 0);
  // R4: second subtitle position/style, local, appended; default = as before (top, same as primary).
  assert(!strcmp(CHAVE[AJ_LEG2_POS], "legenda2PosLocal") && OPCOES[AJ_LEG2_POS].n == 2 && valorPadrao[AJ_LEG2_POS] == 0);
  assert(AJ_LEG2_TAMANHO == AJ_LEG2_POS + 1 && AJ_LEG2_COR == AJ_LEG2_POS + 2 && AJ_LEG2_FUNDO == AJ_LEG2_POS + 3 && AJ_LEG2_BORDA == AJ_LEG2_POS + 4);
  assert(somenteDesteAparelho(AJ_LEG2_POS) && !dePerfil(AJ_LEG2_BORDA));
  for (int i = AJ_LEG2_POS; i <= AJ_LEG2_BORDA; i++) { int vezes2 = 0, k; for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC && TELA[k].op == i) vezes2++; assert(vezes2 == 1 && valorPadrao[i] == 0); }
  { int a0 = valor[AJ_LEG2_POS], t0 = valor[AJ_LEG2_TAMANHO];
    assert(!ajustes_leg2_junto() && ajustes_leg2_tamanho() == 0 && ajustes_leg2_cor() == -1 && ajustes_leg2_fundo() == -1 && ajustes_leg2_borda() == -1);
    valor[AJ_LEG2_POS] = 1; valor[AJ_LEG2_TAMANHO] = 3; valor[AJ_LEG2_COR] = 2; valor[AJ_LEG2_FUNDO] = 5; valor[AJ_LEG2_BORDA] = 3;
    assert(ajustes_leg2_junto() && ajustes_leg2_tamanho() == 100 && ajustes_leg2_cor() == 1 && ajustes_leg2_fundo() == 4 && ajustes_leg2_borda() == 2);
    valor[AJ_LEG2_POS] = a0; valor[AJ_LEG2_TAMANHO] = t0; valor[AJ_LEG2_COR] = valor[AJ_LEG2_FUNDO] = valor[AJ_LEG2_BORDA] = 0; }
  assert(!strcmp(CHAVE[AJ_DISCORD], "-discord"));
  assert(!uxTemPadrao(AJ_DISCORD));
  assert(familiaPreviaOpcao(AJ_DISCORD) == AJPV_RASTREIO);
  int nDiscord = ajustes_buscar("Discord", resultados, AJ_N);
  assert(indiceResultado(AJ_DISCORD, nDiscord) >= 0);
  int discordCount=0;
  for (int i=0;i<AJ_N_TELA;i++) if(TELA[i].tipo==IT_OPC && TELA[i].op==AJ_DISCORD) discordCount++;
  assert(discordCount==1);
  artePorSubmenu();
  reorganizacao202();
  cacheSeek();
  zoomTpk();
  prazoDosAddonsIntegrado();
  padroesEValores();
  escoposPorValor();
  assert(mkdtemp(dir));
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir);
  autoplay202(dir);
  buscaECaminhos();
  bloqueadosESegredos();
  puts("ajustes_ux_dados: busca, padroes, escopo, dependencias e segredos ok");
  return 0;
}
