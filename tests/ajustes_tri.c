// Three-island Settings contract (2.1): D-pad routes, reachability of every
// catalog option, batch review/undo and the parental route. Isolated storage,
// no window, no network, no real profiles.
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
  snprintf(dir, sizeof dir, "%s/nuvio-ajustes-tri-XXXXXX", nvt_dir());
  assert(mkdtemp(dir)); assert(SDL_Init(SDL_INIT_TIMER) == 0);
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir); snprintf(dirAjustes, sizeof dirAjustes, "%s", dir); player_dir(dir); fontefalha_dir(dir);
  ajustes_iniciar(); assert(gravar());
  int before[AJ_N];
  memcpy(before, valor, sizeof before);

  // Opens on the sections island, on the remembered section.
  assert(triIlha == TRI_SECOES && focoIndice && triSecFoco == secAtual);
  // Down/Up move the section and the list follows; Right enters, Left/Back leave.
  triSecFoco = 0; focarSecao(0);
  key(SDLK_DOWN); assert(triSecFoco == 1 && secAtual == 1 && focoIndice);
  key(SDLK_RIGHT); assert(triIlha == TRI_LISTA && !focoIndice && secAtual == 1 && focavel(focoItem));
  key(SDLK_LEFT); assert(triIlha == TRI_SECOES && focoIndice && triSecFoco == 1);
  key(SDLK_RETURN); assert(triIlha == TRI_LISTA);
  key(SDLK_ESCAPE); assert(triIlha == TRI_SECOES && triSecFoco == 1 && !sair);
  key(SDLK_UP); assert(triSecFoco == 0);
  key(SDLK_UP); assert(triIlha == TRI_CHIPS && triChip == 0);
  key(SDLK_DOWN); assert(triIlha == TRI_SECOES);
  key(SDLK_ESCAPE); assert(sair); sair = 0;
  // Up from the first row of the list reaches the header chips; Down comes back.
  triEntrar(0); assert(focoItem == primeiroDaSecao(0));
  key(SDLK_UP); assert(triIlha == TRI_CHIPS);
  key(SDLK_DOWN); assert(triIlha == TRI_SECOES && triSecFoco == 0);
  // Right in the list never changes a value.
  triEntrar(0); { int v = valor[focoOp]; key(SDLK_RIGHT); assert(valor[focoOp] == v && triIlha == TRI_LISTA); }

  // Every available option is reachable through the category and its topics
  // once More options is open. Back restores the originating topic row.
  for (int s = 0; s < nSecoes; s++) {
    unsigned char seen[AJ_N] = {0};
    int steps = 0;
    triEntrar(s); maisAberto[s] = 1;
    focar(primeiroDaSecao(s));
    for (;;) {
      int item = focoItem;
      if (focoOp >= 0) seen[focoOp] = 1;
      if (triTopico(item)) {
        key(SDLK_RETURN); assert(triSubmenu == item);
        for (int childSteps = 0; childSteps < AJ_N_TELA; childSteps++) {
          int child = focoItem;
          if (focoOp >= 0) seen[focoOp] = 1;
          key(SDLK_DOWN);
          if (child == focoItem) break;
        }
        key(SDLK_ESCAPE); assert(triSubmenu < 0 && focoItem == item);
      }
      key(SDLK_DOWN);
      if (focoItem == item || ++steps > AJ_N_TELA) break;
    }
    for (int i = secIni[s] + 1; i < secFim(s); i++)
      if (TELA[i].tipo == IT_OPC && visivel(i)) assert(seen[TELA[i].op] && "every visible option is reachable by Down");
    key(SDLK_ESCAPE); assert(triIlha == TRI_SECOES && triSecFoco == s);
  }
  // A deep link lands on the row inside its section, even behind "Mais opcoes".
  memset(maisAberto, 0, sizeof maisAberto); valor[AJ_AVANCADAS] = 1;
  ajustes_abrir_opcao(AJ_TEX_MB); ajustes_iniciar();
  assert(triIlha == TRI_LISTA && focoOp == AJ_TEX_MB && maisAberto[secAtual]);
  // The choice editor keeps the value until OK; Back cancels.
  focarOpcao(AJ_TEMA); uxAbrirEditor(AJ_TEMA); int theme = valor[AJ_TEMA];
  key(SDLK_DOWN); assert(uxPendente >= 0 && uxPendente < nValores(AJ_TEMA));
  key(SDLK_ESCAPE); assert(valor[AJ_TEMA] == theme && !uxEditor && triIlha == TRI_LISTA);
  // Switches flip on OK, without an editor, and the feature switches keep their effects.
  focarOpcao(AJ_PAUSA_OVERLAY); { int v = valor[AJ_PAUSA_OVERLAY]; key(SDLK_RETURN); assert(valor[AJ_PAUSA_OVERLAY] != v && !uxEditor); }
  debrid_definir_chave_local("realdebrid", "overview-test-key"); assert(debrid_ativo());
  focarOpcao(AJ_DEBRID_LIGADO); key(SDLK_RETURN); assert(!lig(AJ_DEBRID_LIGADO) && !debrid_ativo());
  assert(debrid_origem("realdebrid") > 0);
  key(SDLK_RETURN); assert(lig(AJ_DEBRID_LIGADO) && debrid_ativo());
  debrid_definir_chave_local("realdebrid", "");
  // The profile row opens the profile sheet; Back closes it and keeps the row.
  focarOpcao(AJ_PERFIL_ATIVO); key(SDLK_RETURN); assert(pfTela == PF_MENU);
  key(SDLK_ESCAPE); assert(!pfTela && focoOp == AJ_PERFIL_ATIVO && triIlha == TRI_LISTA);

  // Header chips: Diferente do padrao lists only reviewable differences, and
  // "Restaurar tudo" goes through the batch review before anything changes.
  memcpy(valor, before, sizeof before); valor[AJ_QUALIDADE] = 3; valor[AJ_GPU_EFEITOS] = 1; assert(gravar());
  triIniciar(); triIlha = TRI_CHIPS; triChip = TRI_C_DIF; key(SDLK_RETURN);
  assert(triUtil == TRI_U_DIF && uxNDifs >= 2 && uxIndice == 1);
  { int n = uxNDifs; key(SDLK_DOWN); for (int i = 1; i < n; i++) key(SDLK_DOWN); assert(triAcao == 1); }
  { int q = valor[AJ_QUALIDADE]; key(SDLK_RETURN); assert(triUtil == TRI_U_REVISAO && rvN == uxNDifs && valor[AJ_QUALIDADE] == q); }
  key(SDLK_RIGHT); assert(triAcao == 2); key(SDLK_RETURN); assert(triUtil == TRI_U_DIF && valor[AJ_QUALIDADE] == 3);
  key(SDLK_DOWN); for (int i = 1; i < uxNDifs; i++) key(SDLK_DOWN); key(SDLK_RETURN); assert(triUtil == TRI_U_REVISAO);
  key(SDLK_LEFT); assert(triAcao == 1); key(SDLK_RETURN);
  assert(triUtil == TRI_U_DIF && valor[AJ_QUALIDADE] == valorPadrao[AJ_QUALIDADE] && valor[AJ_GPU_EFEITOS] == valorPadrao[AJ_GPU_EFEITOS]);
  assert(rvUndoN >= 2);
  key(SDLK_DOWN); assert(triAcao == 1); key(SDLK_RIGHT); assert(triAcao == 2); key(SDLK_RETURN);
  assert(valor[AJ_QUALIDADE] == 3 && valor[AJ_GPU_EFEITOS] == 1 && !rvUndoN);
  key(SDLK_ESCAPE); assert(!triUtil && triIlha == TRI_CHIPS && triChip == TRI_C_DIF && uxIndice >= 2);
  // Presets: OK previews, Manter keeps, Aplicar applies; a saved preset round-trips.
  triChip = TRI_C_PRESETS; key(SDLK_RETURN); assert(triUtil == TRI_U_PRESETS && triLinha == 0);
  valor[AJ_DET_BLUR_NAO_VISTOS] = 1; valor[AJ_PAUSA_ELENCO] = 1; valor[AJ_HERO_TRAILER] = 1;
  key(SDLK_DOWN); key(SDLK_DOWN); assert(triLinha == 2);
  key(SDLK_RETURN); assert(triUtil == TRI_U_REVISAO && rvN == 3 && triUtilVolta == TRI_U_PRESETS);
  key(SDLK_ESCAPE); assert(triUtil == TRI_U_PRESETS && valor[AJ_HERO_TRAILER] == 1);
  key(SDLK_RETURN); key(SDLK_LEFT); key(SDLK_RETURN);
  assert(triUtil == TRI_U_PRESETS && valor[AJ_HERO_TRAILER] == 0 && valor[AJ_PAUSA_ELENCO] == 0);
  ajPenSavedProfile = 0; assert(ajPenSavedSave("Sexta")); ajPenSavedLoad(); assert(ajPenSavedCount == 1);
  valor[AJ_HERO_TRAILER] = 1;
  { int n = ajPenSavedPrepare(0), found = 0; assert(n >= 1); for (int i = 0; i < n; i++) found |= rvOps[i] == AJ_HERO_TRAILER && rvDepois[i] == 0; assert(found); }
  assert(ajPenSavedDelete(0) && !ajPenSavedCount);
  key(SDLK_ESCAPE); assert(!triUtil && triIlha == TRI_CHIPS && triChip == TRI_C_PRESETS);
  // The tour chip opens the tour; leaving it returns to the chip.
  triChip = TRI_C_TOUR; key(SDLK_RETURN); assert(tourAberto);
  tourSair(); assert(!tourAberto && triIlha == TRI_CHIPS && triChip == TRI_C_TOUR);
  // Parental: the last entry of the sections island, with its own rows.
  triIniciar(); triSecFoco = nSecoes; key(SDLK_RETURN);
  assert(triUtil == TRI_U_PARENTAL && triIlha == TRI_LISTA);
  for (int i = 0; i < TRI_P_N + 2; i++) key(SDLK_DOWN); assert(triLinha == TRI_P_N - 1);
  key(SDLK_ESCAPE); assert(!triUtil && triIlha == TRI_SECOES && triSecFoco == nSecoes);
  // Values other than the ones this test touched are untouched.
  memcpy(valor, before, sizeof before);
  puts("PASS: three-island Settings: routes, reachability, review/undo, presets, parental");
  return 0;
}
