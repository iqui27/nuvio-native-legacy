// Exercita o contrato do controle com persistência real numa pasta descartável.
#include "../src/ajustes.c"
#include "nvtmp.h"
#include "../src/addonsui.h"
#include "../src/pluginsui.h"
#include <assert.h>
#include <unistd.h>

static void key(SDL_Keycode k) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  ajustes_evento(&e);
}
static char *arquivo(void) {
  char path[700]; long n; FILE *f; char *b;
  snprintf(path, sizeof path, "%s/ajustes.txt", dirAjustes);
  f = fopen(path, "rb"); assert(f);
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
  b = calloc((size_t)n + 1, 1); assert(b);
  assert(fread(b, 1, n, f) == (size_t)n); fclose(f); return b;
}
static void igual(const char *antes) { char *depois = arquivo(); assert(!strcmp(antes, depois)); free(depois); }
static void abrir(int op) { focarOpcao(op); key(SDLK_RETURN); assert(uxEditor); }

int main(void) {
  char dir[1024], bad[1400]; char *antes;
  int original, foco, n, i;
  snprintf(dir, sizeof dir, "%s/nuvio-ajustes-interacao-XXXXXX", nvt_dir());
  assert(mkdtemp(dir)); assert(SDL_Init(SDL_INIT_TIMER) == 0);
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir); assert(!strcmp(dados_dir(), dir)); snprintf(dirAjustes, sizeof dirAjustes, "%s", dir);
  ajustes_iniciar(); assert(gravar());

  // Seekr: sem chave avisa sem gravar; chave propria permite ligar e recarregar.
  {
    assert(!ajustes_seekr_habilitado());
    antes = arquivo();
    assert(uxAlternarDireto(AJ_SEEKR_LIGADO));
    assert(strstr(uxAviso, "seekr.tv") && !ajustes_seekr_ligado());
    igual(antes); free(antes);
    seekrDefinir("seekr-chave-ficticia");
    assert(uxAlternarDireto(AJ_SEEKR_LIGADO) && ajustes_seekr_ligado());
    ajustes_iniciar();
    assert(ajustes_seekr_ligado() && seekr_tem_chave());
    seekrDefinir("");
    assert(!ajustes_seekr_ligado() && !seekr_tem_chave());
    assert(uxValorAtual(AJ_SEEKR_LIGADO) == 1);
    assert(definirValorDireto(AJ_SEEKR_LIGADO, 1));
  }

  // Fonte da biblioteca: o editor so navega entre servicos ligados; Nuvio
  // (LOCAL) fica sempre. Simkl desligado nao pode ser focado nem confirmado.
  {
    int ant = valor[AJ_SALVOS_DEST];
    valor[AJ_SALVOS_DEST] = AJ_SALVOS_LOCAL;
    trakt_definir("token-de-teste", "cliente-de-teste");
    simklauth_esquecer();
    abrir(AJ_SALVOS_DEST);
    assert(uxPendente == AJ_SALVOS_LOCAL);
    key(SDLK_DOWN); assert(uxPendente == AJ_SALVOS_TRAKT);
    key(SDLK_DOWN); assert(uxPendente == AJ_SALVOS_TRAKT);   // Simkl nao entra
    key(SDLK_UP); assert(uxPendente == AJ_SALVOS_LOCAL);
    key(SDLK_RETURN); assert(!uxEditor && valor[AJ_SALVOS_DEST] == AJ_SALVOS_LOCAL);
    abrir(AJ_SALVOS_DEST); key(SDLK_DOWN); key(SDLK_RETURN);
    assert(!uxEditor && valor[AJ_SALVOS_DEST] == AJ_SALVOS_TRAKT);
    trakt_definir("", "");
    valor[AJ_SALVOS_DEST] = ant;
  }

  // #427: a saved request above the RAM cap stays visible, but cannot be selected again.
  {
    char t[160];
    int lingua = valor[AJ_IDIOMA];
    valor[AJ_IDIOMA] = IDIOMA_PT + 1;
    int cap = tex_teto_ram_mb();
    assert(cap == 160);   // no tex_iniciar: unknown RAM uses the conservative cap
    valor[AJ_TEX_MB] = 6;
    assert(gravar());
    assert(!strcmp(textoValor(AJ_TEX_MB), "512 MB → 160 MB nesta TV"));
    uxValorTexto(AJ_TEX_MB, 6, t, sizeof t);
    assert(!strcmp(t, textoValor(AJ_TEX_MB)));
    assert(ajustes_tex_mb() == 512 && valor[AJ_TEX_MB] == 6);
    assert(passoAdiante(AJ_TEX_MB, -1) == 2);
    assert(passoAdiante(AJ_TEX_MB, 1) == 0);
    uxAbrirEditor(AJ_TEX_MB);
    key(SDLK_UP); assert(uxPendente == 2);
    key(SDLK_DOWN); assert(uxPendente == 2);
    ajPontEdValor(6, 0); assert(uxPendente == 2);
    // Clicking a visible unavailable memory size must not confirm 160 MB.
    {
      const PonteiroAlvo *alvos;
      SDL_Event e = {0};
      int alvo, total;
      antes = arquivo();
      ponteiro_iniciar(); ponteiro_teste_janela(1920, 1080);
      ponteiro_teste_toque(1);
      ponteiro_quadro(SDL_GetTicks());
      ajPontEditor = 1;
      gfx_opacidade_grupo = 0;  // register real editor targets without a GL context
      GfxRect painel = aj2EditorR(); painel.w = 1100; painel.h = 1400;  // all RAM rows visible, no GPU preview
      aj2EditorEscolha(painel);
      ponteiro_desenhar();
      gfx_opacidade_grupo = 1;
      total = ponteiro_teste_lista(&alvos);
      for (alvo=0;alvo<total;alvo++) {
        int v=alvos[alvo].a;
        if(v>=0 && v<nValores(AJ_TEX_MB) && texAcima(v) && !alvos[alvo].focar && !alvos[alvo].ativar)break;
      }
      assert(alvo < total);
      e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_LEFT;
      e.button.x = (int)(alvos[alvo].x + alvos[alvo].w / 2);
      e.button.y = (int)(alvos[alvo].y + alvos[alvo].h / 2);
      assert(ponteiro_evento(&e, ajustes_evento));
      assert(uxEditor == 1 && uxPendente == 2 && valor[AJ_TEX_MB] == 6);
      igual(antes); free(antes);
      e.type = SDL_MOUSEBUTTONUP;
      assert(ponteiro_evento(&e, ajustes_evento));
      ponteiro_teste_toque(0); ajPontEditor = 0;
    }
    assert(!definirValorDireto(AJ_TEX_MB, 5));
    key(SDLK_ESCAPE);
    valor[AJ_TEX_MB] = 2;
    assert(passoAdiante(AJ_TEX_MB, 1) == 0);
    assert(!strcmp(textoValor(AJ_TEX_MB), "160 MB"));
    valor[AJ_TEX_MB] = 0;
    assert(passoAdiante(AJ_TEX_MB, -1) == 2);
    assert(!strcmp(textoValor(AJ_TEX_MB), "Automático"));
    valor[AJ_IDIOMA] = lingua;
  }

  // Glass and outline are reachable without advanced settings, even when off.
  assert(!uxAvancada(AJ_VIDRO) && !uxAvancada(AJ_VIDRO_CONTORNO));
  original = valor[AJ_VIDRO]; valor[AJ_VIDRO] = 1;
  for (i = 0; i < AJ_N_TELA; i++) {
    if (TELA[i].tipo != IT_OPC ||
        (TELA[i].op != AJ_VIDRO && TELA[i].op != AJ_VIDRO_CONTORNO)) continue;
    valor[AJ_AVANCADAS] = 1;   // global advanced toggle Desligado
    assert(visivel(i) && focavel(i));
  }
  assert(inativa(AJ_VIDRO_CONTORNO));
  assert(uxRequisito(AJ_VIDRO_CONTORNO) == AJ_VIDRO);
  assert(strstr(ajudaOpcao(AJ_VIDRO_CONTORNO), "Ative a interface de vidro"));
  // 2.0.3 (M3): the line says what it needs and OK turns it on ("OK liga").
  // The dry run touches nothing; the modal stays for shortcuts.
  antes = arquivo(); focarOpcao(AJ_VIDRO_CONTORNO);
  assert(uxLigarRequisitos(AJ_VIDRO_CONTORNO, 0) == 1); igual(antes);
  assert(ajLinhaAviso(focoItem));
  key(SDLK_RETURN); assert(!uxEditor && lig(AJ_VIDRO) && !inativa(AJ_VIDRO_CONTORNO) && focoOp == AJ_VIDRO_CONTORNO);
  { char *d = arquivo(); assert(strcmp(antes, d)); free(d); } free(antes);
  valor[AJ_VIDRO] = 1; antes = arquivo(); uxAbrirEditor(AJ_VIDRO_CONTORNO); assert(uxEditor == 2); igual(antes);
  key(SDLK_ESCAPE); igual(antes); free(antes);
  // "liga as duas": Desfocar o proximo episodio needs Miniatura do episodio.
  valor[AJ_CW_LIGADO] = 0; valor[AJ_CW_THUMB] = 1; valor[AJ_CW_BLUR_PROX] = 1;
  assert(inativa(AJ_CW_BLUR_PROX) && uxLigarRequisitos(AJ_CW_BLUR_PROX, 0) == 2);
  focarOpcao(AJ_CW_BLUR_PROX); key(SDLK_RETURN);
  assert(lig(AJ_CW_THUMB) && lig(AJ_CW_BLUR_PROX) && !inativa(AJ_CW_BLUR_PROX));
  // A child right under its parent is indented; the first row of a block is not.
  { int k, ti = -1, tb = -1;
    for (k = 0; k < AJ_N_TELA; k++) if (TELA[k].tipo == IT_OPC) {
      if (TELA[k].op == AJ_CW_THUMB) ti = k;
      if (TELA[k].op == AJ_CW_LIGADO) tb = k; }
    assert(ti > 0 && tb > 0 && ajLinhaFilha(ti) && !ajLinhaFilha(tb)); }
  valor[AJ_VIDRO] = original;

  // V4 toggles preview before committing, with the same persistence guard.
  valor[AJ_RELOGIO] = 0; assert(gravar()); assert(ehInterruptor(AJ_RELOGIO));
  focarOpcao(AJ_RELOGIO); antes = arquivo();
  key(SDLK_RETURN); assert(uxEditor && valor[AJ_RELOGIO] == 0); igual(antes);
  key(SDLK_RIGHT); key(SDLK_RETURN);
  assert(!uxEditor && !uxAvisoRisco && !uxRestaurar && valor[AJ_RELOGIO] == 1);
  { char *depois = arquivo(); assert(strcmp(antes, depois)); free(depois); } free(antes);
  assert(strstr(uxAviso, "salvo"));
  key(SDLK_RETURN); key(SDLK_LEFT); key(SDLK_RETURN);
  assert(!uxEditor && valor[AJ_RELOGIO] == 0);
  { char falho[1400]; snprintf(falho, sizeof falho, "%s/ausente", dir); snprintf(dirAjustes, sizeof dirAjustes, "%s", falho);
    key(SDLK_RETURN); key(SDLK_RIGHT); key(SDLK_RETURN);
    assert(valor[AJ_RELOGIO] == 0 && uxEditor); assert(strstr(uxAviso, "salvar"));
    snprintf(dirAjustes, sizeof dirAjustes, "%s", dir); key(SDLK_ESCAPE); }
  // Interruptor inativo (depende de outro): 2.0.3, o OK liga o requisito.
  valor[AJ_VIDRO] = 1; focarOpcao(AJ_VIDRO_CONTORNO); key(SDLK_RETURN); assert(!uxEditor && lig(AJ_VIDRO));
  valor[AJ_VIDRO] = 0;
  focarOpcao(AJ_RELOGIO);

  // #238: opcao inativa explica a dependencia DELA, nunca a da profundidade.
  { int i0 = valor[AJ_FONTE_MANUAL], r0 = valor[AJ_RELOGIO], s0 = valor[AJ_SEEKR_LIGADO];
    int ops[] = { AJ_SAIDA_PLAYER, AJ_SEEKR_FITA, AJ_SEEKR_AJUSTE };
    valor[AJ_FONTE_MANUAL] = 0; valor[AJ_RELOGIO] = 1; valor[AJ_SEEKR_LIGADO] = 1;  // 0 = ligado (lig)
    for (i = 0; i < (int)(sizeof ops / sizeof ops[0]); i++) {
      assert(inativa(ops[i]));
      assert(!strstr(ajudaOpcao(ops[i]), "profundidade"));
    }
    assert(!inativa(AJ_FONTE_PRAZO));   /* #238: sem dependencia, nem da profundidade nem de Escolher a fonte */
    valor[AJ_FONTE_MANUAL] = 1; assert(!inativa(AJ_FONTE_PRAZO)); valor[AJ_FONTE_MANUAL] = 0;
    assert(strstr(ajudaOpcao(AJ_SAIDA_PLAYER), "Relógio na tela"));
    assert(strstr(ajudaOpcao(AJ_SEEKR_FITA), "Miniaturas na barra de tempo"));
    valor[AJ_FONTE_MANUAL] = i0; valor[AJ_RELOGIO] = r0; valor[AJ_SEEKR_LIGADO] = s0;
    for (i = 0; i < AJ_N; i++)
      if (inativa(i) && strstr(ajudaOpcao(i), "profundidade"))
        assert(!ajustes_profundidade());
  }

  // Foco e rascunho (escolha, nao interruptor) não alteram nem o valor nem o arquivo; Back cancela.
  valor[AJ_HOME_LAYOUT] = 0; assert(gravar());
  original = valor[AJ_HOME_LAYOUT]; antes = arquivo(); abrir(AJ_HOME_LAYOUT);
  key(SDLK_DOWN); assert(uxPendente != original); assert(valor[AJ_HOME_LAYOUT] == original); igual(antes);
  SDL_Event repetida = {0}; repetida.type = SDL_KEYDOWN;
  repetida.key.keysym.sym = SDLK_RETURN; repetida.key.repeat = 1;
  ajustes_evento(&repetida); assert(uxEditor && valor[AJ_HOME_LAYOUT] == original); igual(antes);
  key(SDLK_ESCAPE); assert(!uxEditor); assert(valor[AJ_HOME_LAYOUT] == original); igual(antes); free(antes);
  abrir(AJ_HOME_LAYOUT); key(SDLK_DOWN); key(SDLK_RETURN);
  assert(!uxEditor && valor[AJ_HOME_LAYOUT] != original);
  antes = arquivo(); key(SDLK_LEFT); assert(aj4Grupos); key(SDLK_LEFT); assert(focoIndice); igual(antes); free(antes);

  // Número só aplica o candidato final, sem gravar cada passo.
  original = valor[AJ_LARGURA_DP]; antes = arquivo(); abrir(AJ_LARGURA_DP);
  key(SDLK_RIGHT); key(SDLK_RIGHT); key(SDLK_RIGHT);
  assert(valor[AJ_LARGURA_DP] == original); igual(antes); free(antes);
  key(SDLK_RETURN); assert(valor[AJ_LARGURA_DP] == original + 3 * OPCOES[AJ_LARGURA_DP].passo);

  // Eixo horizontal, limites, cancelamento e estado visual continuo.
  abrir(AJ_LARGURA_DP); original = valor[AJ_LARGURA_DP];
  key(SDLK_LEFT); assert(uxPendente == original-OPCOES[AJ_LARGURA_DP].passo);
  key(SDLK_DOWN); assert(uxRodape); assert(uxPendente == original-OPCOES[AJ_LARGURA_DP].passo);
  key(SDLK_UP); assert(!uxRodape);
  for (i=0;i<1000;i++) key(SDLK_LEFT);
  assert(uxPendente == OPCOES[AJ_LARGURA_DP].min);
  for (i=0;i<1000;i++) key(SDLK_RIGHT);
  assert(uxPendente == OPCOES[AJ_LARGURA_DP].max);
  key(SDLK_ESCAPE); assert(valor[AJ_LARGURA_DP] == original);
  valor[AJ_ANIM] = 0;
  ajMovPronto[AJ_RELOGIO] = 0; valor[AJ_RELOGIO] = 0;
  assert(ajMov(AJ_RELOGIO,0) == 0);
  valor[AJ_RELOGIO] = 1; ajMovAtualizar(0.016f);
  assert(ajMovValor[AJ_RELOGIO] > 0 && ajMovValor[AJ_RELOGIO] < 1);
  float meio = ajMovValor[AJ_RELOGIO];
  valor[AJ_RELOGIO] = 0; ajMovAtualizar(0.016f);
  assert(ajMovValor[AJ_RELOGIO] < meio && ajMovValor[AJ_RELOGIO] > 0);
  valor[AJ_ANIM] = 1; valor[AJ_RELOGIO] = 1; ajMovAtualizar(0.016f);
  assert(ajMovValor[AJ_RELOGIO] == 1); valor[AJ_ANIM] = 0;

  // Diferenças permanecem estáveis após restaurar; confirmação começa em Cancelar.
  uxListarDiferencas(); n = uxNDifs; assert(n >= 2);
  for (i = 0; i < uxNDifs && uxDifs[i] != AJ_LARGURA_DP; i++) { }
  assert(i < uxNDifs);
  uxIndice = 1; focoIndice = 0; uxDifFoco = i; foco = i;
  uxAbrirEditor(AJ_LARGURA_DP); key(SDLK_DOWN); key(SDLK_RETURN);
  assert(uxRestaurar && !uxConfirmar); antes = arquivo();
  key(SDLK_RETURN); assert(uxEditor && !uxRestaurar); igual(antes); free(antes);
  key(SDLK_RETURN); assert(uxRestaurar); key(SDLK_LEFT); key(SDLK_RETURN);
  assert(!uxEditor && valor[AJ_LARGURA_DP] == valorPadrao[AJ_LARGURA_DP]);
  assert(uxIndice == 1 && uxDifFoco == foco && uxNDifs == n);

  // Falha de disco deixa o editor aberto e preserva o valor anterior.
  valor[AJ_HOME_LAYOUT] = 0; abrir(AJ_HOME_LAYOUT); original = valor[AJ_HOME_LAYOUT]; key(SDLK_DOWN);
  snprintf(bad, sizeof bad, "%s/ausente", dir); snprintf(dirAjustes, sizeof dirAjustes, "%s", bad);
  key(SDLK_RETURN); assert(uxEditor && valor[AJ_HOME_LAYOUT] == original); assert(strstr(uxAviso, "salvar"));
  snprintf(dirAjustes, sizeof dirAjustes, "%s", dir); key(SDLK_ESCAPE);

  // Mudança externa durante edição exige rever o candidato antes de aplicar.
  valor[AJ_HOME_LAYOUT] = 0; abrir(AJ_HOME_LAYOUT); key(SDLK_DOWN); valor[AJ_HOME_LAYOUT] = 2; assert(gravar());
  key(SDLK_RETURN); assert(uxEditor && uxPendente == 2); assert(strstr(uxAviso, "outra tela")); key(SDLK_ESCAPE);

  // Avançado encontrado pela busca é revelado e alcançável, sem ciclos extras.
  ajustes_abrir_opcao(AJ_TEX_MB); ajustes_iniciar();
  assert(focoOp == AJ_TEX_MB && !focoIndice && visivel(focoItem));
  assert(!lig(AJ_AVANCADAS) && maisAberto[secAtual]);   // 2.0.3: the deep link opens that category's "Mais opcoes", not the global toggle
  key(SDLK_ESCAPE); assert(ajustes_pediu_busca() == 2); assert(!ajustes_pediu_busca());
  assert(!uxVeioBusca);
  ajustes_abrir_opcao(AJ_TEX_MB); ajustes_iniciar(); assert(uxVeioBusca);
  ajustes_encerrar(); ajustes_iniciar(); assert(!uxVeioBusca);
  focarOpcao(AJ_TEX_MB);
  aj4Topo=0; key(SDLK_RETURN); assert(aj4Util==3 && teclado_aberto());
  key(SDLK_ESCAPE); ajustes_atualizar(0,SDL_GetTicks()); aj4Util=0; aj4Topo=-1;

  // Dependência (2.0.3): requisito interruptor, OK liga e o foco fica; requisito
  // que nao e interruptor (o layout), OK leva ate ele e Voltar devolve.
  valor[AJ_HERO_TRAILER] = 1; focarOpcao(AJ_HERO_TRAILER_ESPERA); key(SDLK_RETURN);
  assert(lig(AJ_HERO_TRAILER) && !inativa(AJ_HERO_TRAILER_ESPERA) && focoOp == AJ_HERO_TRAILER_ESPERA && !uxEditor);
  valor[AJ_HOME_LAYOUT] = 1; focarOpcao(AJ_HERO_CHEIO); assert(inativa(AJ_HERO_CHEIO) && !uxLigarRequisitos(AJ_HERO_CHEIO, 0));
  key(SDLK_RETURN); assert(focoOp == AJ_HOME_LAYOUT && !uxEditor && uxRetornarOp == AJ_HERO_CHEIO);
  key(SDLK_ESCAPE); assert(focoOp == AJ_HERO_CHEIO && !focoIndice);

  // v4: OK on an inactive row moves focus to its requirement; Back returns.
  focarOpcao(AJ_HERO_CHEIO); key(SDLK_RETURN);
  assert(focoOp == AJ_HOME_LAYOUT && uxRetornarOp == AJ_HERO_CHEIO);
  key(SDLK_DOWN); key(SDLK_ESCAPE); key(SDLK_ESCAPE);
  assert(focoOp == AJ_HERO_CHEIO && !focoIndice && uxRetornarOp == -1);
  valor[AJ_HOME_LAYOUT] = 0;

  // V4 replaces the three-column v3 tree with four task rows and two sets.
  valor[AJ_AVANCADAS]=0; focoIndice=1; aj4Topo=-1; aj4Linha=0; aj4Set=0;
  key(SDLK_RIGHT); assert(aj4Set==1 && aj4Linha==0);
  key(SDLK_LEFT); assert(aj4Set==0);
  key(SDLK_RETURN); assert(!focoIndice && aj4Atual==0 && aj4Grupos);
  key(SDLK_RIGHT); assert(!aj4Grupos);
  key(SDLK_ESCAPE); assert(aj4Grupos); key(SDLK_ESCAPE); assert(focoIndice);
  aj4Topo=2; key(SDLK_RETURN); assert(pfTela==PF_MENU); pfTela=PF_FECHADA;
  aj4Topo=-1; aj4Linha=5; key(SDLK_RETURN); assert(aj4Util==1 && !focoIndice);
  key(SDLK_ESCAPE); assert(focoIndice && !aj4Util);
  aj4Topo=1; key(SDLK_RETURN); assert(!lig(AJ_AVANCADAS));
  key(SDLK_RETURN); assert(lig(AJ_AVANCADAS)); aj4Topo=-1;
  // 2.0.3: as listas de addons/plugins voltam para a linha de onde sairam
  // (Fontes e addons), e nao para o indice; Voltar/ESC/Esquerda saem delas.
  { SDL_Event ev = { 0 };
    ajustes_voltar_de_lista(0); ajustes_iniciar();
    assert(!focoIndice && focoOp == AJ_ADDONS && !uxVeioBusca);
    ajustes_voltar_de_lista(1); ajustes_iniciar();
    assert(!focoIndice && focoOp == AJ_PLUGINS);
    ev.type = SDL_KEYDOWN;
    addonsui_abrir(); ev.key.keysym.sym = SDLK_ESCAPE; addonsui_evento(&ev); assert(addonsui_quer_sair());
    addonsui_abrir(); ev.key.keysym.sym = SDLK_LEFT; addonsui_evento(&ev); assert(addonsui_quer_sair());
    pluginsui_abrir(); ev.key.keysym.sym = SDLK_ESCAPE; pluginsui_evento(&ev); assert(pluginsui_quer_sair());
    uxIndice = 1; focoIndice = 1; uxTopo = -1; }
  valor[AJ_ANIM] = 1; ajustes_atualizar(1.0f, SDL_GetTicks());
  assert(animItem[focoItem] == 0);

  // Memória: uma confirmação para o valor final; cancelar não salva.
  valor[AJ_ITENS_FILEIRA] = 0; abrir(AJ_ITENS_FILEIRA); key(SDLK_DOWN); key(SDLK_DOWN);
  antes = arquivo(); key(SDLK_RETURN); assert(uxAvisoRisco && !uxConfirmar); igual(antes);
  key(SDLK_ESCAPE); key(SDLK_ESCAPE); assert(valor[AJ_ITENS_FILEIRA] == 0); igual(antes); free(antes);
  abrir(AJ_ITENS_FILEIRA); key(SDLK_DOWN); key(SDLK_RETURN);
  key(SDLK_LEFT); key(SDLK_RETURN); assert(!uxEditor && valor[AJ_ITENS_FILEIRA] == 1);

  // Guide source selection uses the existing explicit Confirm/Cancel editor.
  valor[AJ_GUIA_FONTE]=0;abrir(AJ_GUIA_FONTE);key(SDLK_DOWN);
  assert(uxEditor && ajustes_guia_fonte()==0);key(SDLK_ESCAPE);assert(ajustes_guia_fonte()==0);
  abrir(AJ_GUIA_FONTE);key(SDLK_DOWN);key(SDLK_RETURN);key(SDLK_LEFT);key(SDLK_RETURN);
  assert(!uxEditor && ajustes_guia_fonte()==1);
  assert(perfilLocal(AJ_GUIA_FONTE) && somenteDesteAparelho(AJ_GUIA_FONTE));
  valor[AJ_GUIA_FONTE]=0;assert(gravar());

  // Keep coverage for every explicit icon, including the new Live TV rows.
  // Unchanged upstream cc87bec3 already uses the generic icon for these two
  // unrelated settings; a Live TV fixture must not require changing their UI.
  for (i=0;i<AJ_N_TELA;i++) if (TELA[i].tipo == IT_OPC) {
    int op=TELA[i].op;
    if(op==AJ_MENU_CARTAZ || op==AJ_TRAILER_AUDIO)continue;
    if(!strcmp(ajVisualIcone(op),"aj_settings-2"))fprintf(stderr,"Missing icon: %s\n",OPCOES[op].rotulo);
    assert(strcmp(ajVisualIcone(op), "aj_settings-2"));
  }

  // Apoiar o projeto: OK abre o painel dos QRs, o foco fica na linha, Voltar e OK fecham.
  assert(apoio_n() > 0);
  focarOpcao(AJ_APOIAR); assert(!apoioAberto);
  key(SDLK_RETURN); assert(apoioAberto && !uxEditor);
  key(SDLK_DOWN); assert(apoioAberto && focoOp == AJ_APOIAR);   // setas nao andam por tras
  key(SDLK_ESCAPE); assert(!apoioAberto && focoOp == AJ_APOIAR);
  key(SDLK_RETURN); assert(apoioAberto); key(SDLK_RETURN); assert(!apoioAberto && focoOp == AJ_APOIAR);

  // Reabrir a tela preserva valores confirmados e não confirma rascunhos.
  int confirmado = valor[AJ_RELOGIO]; ajustes_encerrar(); ajustes_iniciar();
  assert(valor[AJ_RELOGIO] == confirmado);

  // Sair da tela descarta rascunho (Spotlight / troca de tela / fechamento).
  abrir(AJ_HOME_LAYOUT); original = valor[AJ_HOME_LAYOUT]; key(SDLK_DOWN);
  ajustes_encerrar(); assert(!uxEditor && valor[AJ_HOME_LAYOUT] == original);
  char path[700]; snprintf(path, sizeof path, "%s/ajustes.txt", dir); unlink(path);
  snprintf(path, sizeof path, "%s/ajustes-locais.txt", dir); unlink(path);
  rmdir(dir); SDL_Quit();
  puts("ajustes_ux_interacao: cenários OK"); return 0;
}
