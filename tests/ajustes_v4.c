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
  puts("Settings v4: mode, complete catalog, search, scoped reset, presets, undo, save failure OK");
#endif
  SDL_Quit(); return 0;
}
