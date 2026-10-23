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
  dados_iniciar(dir); snprintf(dirAjustes, sizeof dirAjustes, "%s", dir);
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
    key(SDLK_RETURN);
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
  puts("Settings v4: navigation, suggestions, profile routes, preset counts/scopes, persistence OK");
#endif
  SDL_Quit(); return 0;
}
