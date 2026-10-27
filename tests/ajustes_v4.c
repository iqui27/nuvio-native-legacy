// V4 navigation and persistence contract, isolated from real profiles/accounts.
#include "../src/ajustes.c"
#include "nvtmp.h"
#include <assert.h>
#include <unistd.h>

static void key(SDL_Keycode k) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  ajustes_evento(&e);
}
int main(void) {
  char dir[1024];
  snprintf(dir, sizeof dir, "%s/nuvio-ajustes-v4-XXXXXX", nvt_dir());
  assert(mkdtemp(dir)); assert(SDL_Init(SDL_INIT_TIMER) == 0);
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir); snprintf(dirAjustes, sizeof dirAjustes, "%s", dir);player_dir(dir);
  ajustes_iniciar(); assert(gravar());
#ifndef AJUSTES_V4
  assert(!"Settings v4 navigation is missing");
#else
  int before[AJ_N], ops[AJ_N], n;
  memcpy(before, valor, sizeof before);
  valor[AJ_AVANCADAS] = 1; // Simple, legacy inverted boolean.
  assert(aj4Lista(0, -1, ops) == 4);
  assert(ops[0] == AJ_QUALIDADE && ops[1] == AJ_FONTE_MANUAL);
  n = aj4Lista(0, 0, ops);
  assert(n == 1 && ops[0] == AJ_QUALIDADE);
  assert(!aj4Simples(AJ_FONTE_REGEX));
  aj4Topo = 1; focoIndice = 1; key(SDLK_RETURN);
  assert(lig(AJ_AVANCADAS));
  n = aj4Lista(0, -1, ops);
  assert(n > 4);
  int found = 0;
  for (int i = 0; i < n; i++) found |= ops[i] == AJ_FONTE_REGEX;
  assert(found);
  for (int i = 0; i < AJ_N; i++) if (i != AJ_AVANCADAS) assert(valor[i] == before[i]);
  // Every compiled TELA option has exactly one Advanced route, including actions.
  for (int i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_OPC) {
    int op = TELA[i].op, seen = 0;
    assert(aj4Secao(op) >= 0 && aj4Secao(op) < 8);
    n = aj4ListaBruta(aj4Secao(op), -1, ops);
    for (int j = 0; j < n; j++) seen += ops[j] == op;
    assert(seen == 1);
  }
  // Every original setting is reachable through a designed page or the continuation.
  {
    unsigned char seen[AJ_N]={0};int missing[AJ_N],count=ajPenMissing(missing);
    for(int p=0;p<46;p++) {
      ajPenOpen(p);const AjPenPage *page=ajPenCurrent();assert(page);
      for(int c=0;c<2;c++)for(int r=0;r<page->cols[c].n;r++) {
        int op=page->cols[c].rows[r].op;assert(op>=0 && op<AJ_N);seen[op]=1;
      }
    }
    for(int i=0;i<count;i++)seen[missing[i]]=1;
    for(int op=0;op<AJ_N;op++)if(aj4Secao(op)>=0 && ajOpcaoDisponivel(op))assert(seen[op]);
    ajPenPage=-1;focoIndice=1;
  }
  // Specialized Pencil pages preserve navigation and cancel pending changes.
  aj4Iniciar();focoIndice=1;aj4Linha=10;key(SDLK_RETURN);
  assert(ajPenPage==AJ_PEN_PARENTAL && ajPenCurrent()==&AJ_PEN_PARENT);
  key(SDLK_ESCAPE);assert(focoIndice && !uxEditor);
  aj4Linha=6;key(SDLK_RETURN);assert(ajPenPage==AJ_PEN_RULES);
  key(SDLK_ESCAPE);assert(focoIndice && aj4Linha==6);
  aj4Linha=7;key(SDLK_RETURN);assert(ajPenPage==AJ_PEN_TOUR);
  key(SDLK_DOWN);assert(ajPenRow==4);key(SDLK_RIGHT);assert(ajPenRow==5);
  key(SDLK_ESCAPE);assert(focoIndice);
  focarOpcao(AJ_TEMA);uxAbrirEditor(AJ_TEMA);int theme=valor[AJ_TEMA];
  key(SDLK_DOWN);assert(uxPendente>=0 && uxPendente<nValores(AJ_TEMA));
  key(SDLK_ESCAPE);assert(valor[AJ_TEMA]==theme);
  // Pencil custom presets survive reload, exclude credentials, and roll back through review.
  aj4Iniciar();focoIndice=1;
  valor[AJ_QUALIDADE]=2;
  assert(ajPenSavedSave("Friday movie"));
  ajPenSavedProfile=0;ajPenSavedLoad();
  assert(ajPenSavedCount==1 && !strcmp(ajPenSavedNames[0],"Friday movie"));
  assert(ajPenSavedValues[0][AJ_DEBRID_RD]<0);
  valor[AJ_QUALIDADE]=3;
  assert(ajPenSavedPrepare(0)>0 && aj4Aplicar());
  assert(valor[AJ_QUALIDADE]==2 && aj4Desfazer() && valor[AJ_QUALIDADE]==3);
  int subtitleSize=ajLegendaValor(AJ_LEG1_TAMANHO);
  ajLegendaEspelhar();ajPenPresetPrepare(0);assert(aj4Aplicar());
  assert(ajLegendaValor(AJ_LEG1_TAMANHO)==140 && aj4Desfazer());
  assert(ajLegendaValor(AJ_LEG1_TAMANHO)==subtitleSize);
  for(int p=0;p<4;p++) { ajPenPresetPrepare(p);assert(aj4Review==2);aj4Review=0; }
  ajPenOpen(28);key(SDLK_ESCAPE);assert(ajPenPage==27);
  ajPenOpen(34);key(SDLK_ESCAPE);assert(ajPenPage==33);
  // Local PINs gate profile entry and fail closed on a wrong PIN.
  assert(dados_gravar("perfis.txt","1\t0\t1\t0\t336699\tTest profile\t\t\n"));
  perfis_carregar_ativo();
  assert(admin_perfil_pin_definir(1,"4826"));
  assert(admin_perfil_pin_conferir(1,"4826") && !admin_perfil_pin_conferir(1,"0000"));
  assert(perfis_acao(0)==PERFIL_ACAO_PIN);
  assert(perfis_verificar_pin(1,"4826")==1 && perfis_verificar_pin(1,"0000")==0);
  admin_pin_esquecer();
  // Parental policy is validated, persisted, enforced, and remains profile-local.
  assert(perfil_regra_definir(1,PR_IDADE,12));
  assert(!perfil_regra_definir(1,PR_IDADE,99));
  assert(perfil_infantil_definir(1,1));
  assert(perfil_regra_classif("12") && !perfil_regra_classif("16") && !perfil_regra_classif(NULL));
  assert(!perfil_regra_busca("16") && perfil_regra_compras());
  assert(perfil_regra_definir(1,PR_BUSCA_LIVRE,1) && perfil_regra_busca("16"));
  assert(perfil_regra_definir(1,PR_TEMPO,1));
  for(int i=0;i<61;i++)perfil_regra_tempo(1);
  assert(perfil_regra_tempo(0));
  assert(perfil_infantil_definir(1,0) && !perfil_regra_tempo(0));
  perfil_regras_esquecer();aj4Iniciar();ajPenPage=-1;focoIndice=1;
  // Special pages preserve editors and expose left-panel actions to the D-pad.
  ajPenOpen(AJ_PEN_SECONDARY);key(SDLK_LEFT);assert(ajPenRow==5);
  key(SDLK_DOWN);assert(ajPenRow==6);key(SDLK_RETURN);assert(uxEditor && uxOp==AJ_LEG2_POS);key(SDLK_ESCAPE);
  key(SDLK_DOWN);assert(ajPenRow==7);key(SDLK_UP);assert(ajPenRow==6);
  ajPenOpen(AJ_PEN_SOURCES);key(SDLK_LEFT);assert(ajPenRow==-1);
  key(SDLK_DOWN);key(SDLK_RETURN);assert(uxEditor && uxOp==AJ_FONTE_ORDEM_ADDON);key(SDLK_ESCAPE);
  key(SDLK_ESCAPE);assert(ajPenPage==29);
  ajPenOpen(44);ajPenRow=11;key(SDLK_DOWN);assert(ajPenRow==99);
  key(SDLK_RETURN);assert(ajPenPage==AJ_PEN_COMPONENTS);key(SDLK_ESCAPE);assert(ajPenPage==44);
  aj4Iniciar();focoIndice=1;
  // APS-9: invalid IDs, absent routes and corrupt compiled sections are bounded.
  assert(nSecoes == 11 && AJ_AFR + 1 == AJ_HERO_TAMANHO);
  assert(AJ_HERO_TAMANHO + 1 == AJ_DET_TOPO && AJ_DET_TOPO + 1 == AJ_DET_ELENCO_VISTO);
  assert(AJ_DET_ELENCO_VISTO + 1 == AJ_GUIA_PREVIA_GRANDE && AJ_GUIA_PREVIA_GRANDE + 1 == AJ_N);
  assert(!strcmp(CHAVE[AJ_AFR], "afrLocal") && !strcmp(CHAVE[AJ_HERO_TAMANHO], "heroSizeLocal"));
  assert(!strcmp(CHAVE[AJ_DET_TOPO], "detalheTopoLocal"));
  assert(!strcmp(CHAVE[AJ_DET_ELENCO_VISTO], "detalheElencoVistoLocal"));
  assert(valorPadrao[AJ_DET_TOPO] == 0 && valorPadrao[AJ_DET_ELENCO_VISTO] == 0);
  assert(aj4Secao(AJ_DET_TOPO) == 4 && aj4GrupoDe(AJ_DET_TOPO) == 20);
  assert(aj4Secao(AJ_DET_ELENCO_VISTO) == 4 && aj4GrupoDe(AJ_DET_ELENCO_VISTO) == 20);
  assert(!aj4RTL() && aj4X(100, 470) == 100);
  assert(aj4Secao(-1) == -1 && aj4Secao(AJ_N) == -1);
  assert(aj4GrupoDe(-1) == -1 && aj4GrupoDe(AJ_N) == -1);
  int section = aj4Atual, group = aj4Grupo;
  aj4Focar(-1); aj4Focar(AJ_N); aj4Entrar(-1); aj4Entrar(8);
  assert(aj4Atual == section && aj4Grupo == group);
  assert(aj4ListaBruta(-1, -1, ops) == 0 && aj4ListaBruta(8, -1, ops) == 0);
  for (int i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_OPC && aj4MapaSecao(TELA[i].op) < 0) {
    int saved = secDoItem[i], op = TELA[i].op;
    secDoItem[i] = -1; assert(aj4Secao(op) == -1 && aj4GrupoDe(op) == -1);
    secDoItem[i] = nSecoes; assert(aj4Secao(op) == -1 && aj4GrupoDe(op) == -1);
    secDoItem[i] = saved;
  }
  // Search crosses mode boundary without changing saved mode or values.
  valor[AJ_AVANCADAS] = 1;
  AjusteBuscaResultado hits[AJ_N];
  n = ajustes_buscar("regex", hits, AJ_N); found = 0;
  for (int i = 0; i < n; i++) found |= hits[i].op == AJ_FONTE_REGEX;
  assert(found);
  focarOpcao(AJ_FONTE_REGEX);
  assert(aj4Atual == aj4Secao(AJ_FONTE_REGEX) && !lig(AJ_AVANCADAS));
  key(SDLK_ESCAPE); assert(aj4Grupos);
  // Scoped reset never touches the other scope; Undo restores saved changes.
  valor[AJ_QUALIDADE] = 3; valor[AJ_GPU_EFEITOS] = 1;
  int device = valor[AJ_GPU_EFEITOS];
  assert(aj4PrepararReset(-1, 0) > 0);
  for (int i = 0; i < aj4N; i++) assert(escopoDe(aj4Ops[i]) != ESC_TV);
  assert(aj4Aplicar());
  assert(valor[AJ_QUALIDADE] == valorPadrao[AJ_QUALIDADE]);
  assert(valor[AJ_GPU_EFEITOS] == device);
  assert(aj4Desfazer()); assert(valor[AJ_QUALIDADE] == 3);
  int profile = valor[AJ_QUALIDADE];
  assert(aj4PrepararReset(-1, 1) > 0 && aj4Aplicar());
  assert(valor[AJ_QUALIDADE] == profile && valor[AJ_GPU_EFEITOS] == valorPadrao[AJ_GPU_EFEITOS]);
  assert(aj4Desfazer());
  // Preset preview and cancellation never write; apply changes real values.
  memcpy(before, valor, sizeof before);
  assert(aj4PrepararPreset(1) > 0);
  assert(!memcmp(before, valor, sizeof before));
  assert(aj4Aplicar()); assert(valor[AJ_QUALIDADE] == 3);
  assert(aj4Desfazer()); assert(!memcmp(before, valor, sizeof before));
  // Failed save preserves both scopes in memory and on disk.
  aj4PrepararPreset(0);
  char saved[sizeof dirAjustes]; snprintf(saved, sizeof saved, "%s", dirAjustes);
  snprintf(dirAjustes, sizeof dirAjustes, "%s/missing/dir", dir);
  assert(!aj4Aplicar()); assert(!memcmp(before, valor, sizeof before));
  snprintf(dirAjustes, sizeof dirAjustes, "%s", saved);
  // "Mais grupos" paging: four groups per page, More groups advances and wraps.
  {
    int gs[AJ4_N_GRUPOS], ng;
    valor[AJ_AVANCADAS] = 0; focoIndice = 1; aj4Linha = 0; aj4Topo = -1; // advanced
    aj4Review = 0; aj4Util = 0; aj4Acao = 0; // clear the failed-save review state
    ajPenPage=-1;aj4Entrar(0);
    assert(aj4Grupos && aj4Atual == 0);
    ng = aj4GruposLista(aj4Atual, gs);
    assert(ng > 4);
    assert(aj4Grupo == gs[0] && aj4GrupoFoco == 0 && !aj4Mais);
    aj4GrupoFoco = 3; key(SDLK_DOWN); assert(aj4Mais);
    key(SDLK_DOWN); assert(aj4Mais); // nothing below the button
    key(SDLK_UP); assert(!aj4Mais && aj4GrupoFoco == 3);
    aj4Mais = 1; key(SDLK_RETURN);
    assert(aj4Grupo == gs[4] && aj4GrupoFoco == 4 && aj4Mais);
    for (int guard = 0; aj4Grupo != gs[0] && guard < 8; guard++) key(SDLK_RETURN);
    assert(aj4Grupo == gs[0]); // wrapped back to the first page
    key(SDLK_UP); key(SDLK_UP); key(SDLK_UP); key(SDLK_UP);
    assert(!aj4Mais); // focus never escapes the visible page
    aj4GrupoFoco = 1; key(SDLK_RIGHT);
    assert(!aj4Grupos && !aj4Mais && aj4Grupo == gs[1]);
  }
  // Home preset label derives live from stored values, not a static string.
  {
    char rot[160];
    assert(aj4PrepararPreset(1) > 0 && aj4Aplicar());
    assert(aj4PresetCombina(1));
    aj4PredefRotulo(rot, sizeof rot);
    assert(!strcmp(rot, "Economizar dados"));
    valor[AJ_QUALIDADE] = 0;
    assert(!aj4PresetCombina(1));
    aj4PredefRotulo(rot, sizeof rot);
    assert(strcmp(rot, "Economizar dados") && strstr(rot, " · ")); // derived Custom · value
    valor[AJ_QUALIDADE] = 3;
    assert(aj4Desfazer());
  }
  // Approved landing routes preserve values, including return from language/account groups.
  aj4Review=aj4Util=0; aj4Topo=2; focoIndice=1; key(SDLK_RETURN);
  assert(aj4Util==4 && !pfTela);
  key(SDLK_RETURN); assert(ajustes_pediu_trocar_perfil()==1);
  assert(!ajustes_pediu_trocar_perfil());
  key(SDLK_DOWN); key(SDLK_RETURN);
  assert(!aj4Util && aj4Opcoes[aj4Linha]==AJ_AUD_LINGUA);
  key(SDLK_ESCAPE); assert(aj4Util==4);
  aj4Acao=1; key(SDLK_RETURN); assert(pfTela==PF_MENU);
  key(SDLK_ESCAPE); assert(!pfTela && aj4Util==4);
  // Blank search offers all-mode suggestions; task chips search without writing values.
  aj4Util=3; aj4Consulta[0]=0; aj4Acao=aj4Linha=0;
  n=aj4BuscarResultados(); assert(n==4 && aj4Resultados[0].op==AJ_QUALIDADE);
  assert(aj4Resultados[1].op==AJ_FONTE_REGEX && aj4Resultados[1].avancado);
  memcpy(before,valor,sizeof before);
  key(SDLK_LEFT); key(SDLK_RETURN);
  assert(aj4Consulta[0] && !aj4Acao && aj4BuscarResultados()>0);
  assert(!memcmp(before,valor,sizeof before));
  // Displayed preset counts and scopes agree with the transaction being previewed.
  for(int p=0;p<3;p++) {
    int tv,pr,count=aj4PresetContar(p,&tv,&pr),actualTv=0,actualPr=0;
    assert(aj4PrepararPreset(p)==count);
    for(int i=0;i<aj4N;i++)if(escopoDe(aj4Ops[i])==ESC_TV)actualTv=1;else actualPr=1;
    assert(tv==actualTv && pr==actualPr);
  }
  valor[AJ_QUALIDADE]=0; char label[160];aj4PredefRotulo(label,sizeof label);
  assert(strstr(label,i18n("Vídeo automático")));
  valor[AJ_AVANCADAS]=0;aj4Review=aj4Util=0;aj4Entrar(0);
  n=aj4Lista(0,aj4GrupoDe(AJ_FONTE_MANUAL),ops);
  assert(n>=4 && ops[0]==AJ_FONTE_MANUAL && ops[1]==AJ_FONTE_PRIORIDADE && ops[2]==AJ_FONTE_PRAZO && ops[3]==AJ_FONTE_REPOR);
  // APS-10: port navigation contracts to the approved v4 tree in both directions.
  for (int rtl = 0; rtl < 2; rtl++) {
    int lang = valor[AJ_IDIOMA];
    valor[AJ_IDIOMA] = (rtl ? IDIOMA_AR : IDIOMA_PT) + 1;
    SDL_Keycode left = rtl ? SDLK_RIGHT : SDLK_LEFT, right = rtl ? SDLK_LEFT : SDLK_RIGHT;
    assert(aj4RTL() == rtl && aj4X(100,470) == (rtl ? NV_VTELA_W-570 : 100));
    // Pointer targets follow the same mirror as drawing, without a GL context.
    ponteiro_iniciar(); ponteiro_teste_janela(1920,1080); ponteiro_teste_toque(1);
    ponteiro_quadro(SDL_GetTicks()); pfTela=0;
    aj4Alvo((GfxRect){100,510,470,68},2,0); ponteiro_desenhar();
    { const PonteiroAlvo *targets; int count=ponteiro_teste_lista(&targets);
      assert(count==1 && targets[0].x==aj4X(100,470) && targets[0].w==470); }
    ponteiro_teste_toque(0);
    aj4Iniciar(); valor[AJ_AVANCADAS]=0; focoIndice=1; aj4Topo=0;
    key(left); assert(aj4Topo==0);
    key(right); assert(aj4Topo==1);
    key(right); assert(aj4Topo==2);
    key(right); assert(aj4Topo==2);
    key(left); key(left); assert(aj4Topo==0);
    key(SDLK_DOWN); assert(aj4Topo==-1 && focoIndice);
    // Pencil rows remain reachable in both reading directions; browsing never writes.
    memcpy(before,valor,sizeof before);
    for(int page=0;page<46;page++) {
      ajPenOpen(page);const AjPenPage *p=ajPenCurrent();assert(p);
      int count=p->cols[0].n+p->cols[1].n;assert(count>0 && count<=14);
      for(int i=0;i<count+1;i++)key(SDLK_DOWN);
      assert(ajPenRow==count-1);
      key(SDLK_ESCAPE);
      for(int depth=0;!focoIndice && depth<3;depth++)key(SDLK_ESCAPE);
      assert(focoIndice && ajPenPage==-1);
    }
    assert(!memcmp(before,valor,sizeof before));
    aj4Linha=2;key(right);assert(aj4Linha==3);key(left);assert(aj4Linha==2);
    key(SDLK_UP);assert(aj4Topo==0);key(SDLK_DOWN);
    key(SDLK_DOWN);assert(aj4Linha==9);
    key(right);assert(aj4Linha==10);key(SDLK_DOWN);assert(aj4Linha==12);
    key(SDLK_RETURN);assert(ajPenPage==44 && !focoIndice);
    key(SDLK_ESCAPE);assert(focoIndice);
    aj4Linha=5; key(SDLK_RETURN); assert(aj4Util==1 && !focoIndice);
    key(SDLK_ESCAPE); assert(!aj4Util && focoIndice);
    // Both modes open the same four tasks, preserving settings.
    valor[AJ_AVANCADAS]=1;
    const int taskPages[]={29,27,33,37};
    for(int task=0;task<4;task++) {
      aj4Linha=task;key(SDLK_RETURN);assert(!focoIndice && ajPenPage==taskPages[task]);
      key(SDLK_ESCAPE);assert(focoIndice);
    }
    // Editor directions follow the mirror; cancellation preserves saved values.
    focarOpcao(AJ_QUALIDADE); int quality=valor[AJ_QUALIDADE];
    key(SDLK_RETURN); assert(uxEditor);
    key(right); key(SDLK_ESCAPE); assert(!uxEditor && valor[AJ_QUALIDADE]==quality);
    aj4Iniciar(); valor[AJ_AVANCADAS]=0; focoIndice=1;
    // Mode switching preserves unrelated values and restores the remembered row.
    aj4Linha=3; aj4Topo=1; memcpy(before,valor,sizeof before); key(SDLK_RETURN);
    assert(!lig(AJ_AVANCADAS) && focoIndice);
    { char path[1200], line[1024]; int savedMode=0;
      snprintf(path,sizeof path,"%s/ajustes.txt",dirAjustes);
      FILE *file=fopen(path,"r"); assert(file);
      while(fgets(line,sizeof line,file))savedMode|=!strcmp(line,"avancadasLocal 1\n");
      fclose(file); assert(savedMode);
    }
    for(int i=0;i<AJ_N;i++)if(i!=AJ_AVANCADAS)assert(valor[i]==before[i]);
    key(SDLK_RETURN); assert(lig(AJ_AVANCADAS) && aj4Linha==3);
    aj4Topo=-1; key(SDLK_RETURN); assert(!focoIndice);
    key(SDLK_ESCAPE); assert(focoIndice);
    sair=0; key(SDLK_ESCAPE); assert(sair); sair=0;
    valor[AJ_IDIOMA]=lang;
  }
  puts("Settings v4: bounds, RTL/LTR navigation, suggestions, profile routes, preset counts/scopes, persistence OK");
#endif
  SDL_Quit(); return 0;
}
