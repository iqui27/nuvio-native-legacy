// Opus regressions exercise production Settings handlers with isolated storage.
// 2.1: the three-island Settings replaced the Pencil pages; every P0/P1 keeps
// its intent (Back returns where it came from, OK never plays, batch review,
// search, parental) on the new routes.
#include "../src/ajustes.c"
#include "nvtmp.h"
#include <assert.h>
static int failures;
#define CHECK(ok, name) do { int passed=!!(ok);printf("%s: %s\n",passed?"PASS":"FAIL",name);if(!passed)failures++; } while(0)
static void press(SDL_Keycode key) { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=key;ajustes_evento(&e); }
int main(void) {
  char dir[1024];snprintf(dir,sizeof dir,"%s/nuvio-opus-XXXXXX",nvt_dir());assert(mkdtemp(dir));
  assert(!SDL_Init(SDL_INIT_TIMER));setenv("NUVIO_DADOS",dir,1);dados_iniciar(dir);
  snprintf(dirAjustes,sizeof dirAjustes,"%s",dir);player_dir(dir);fontefalha_dir(dir);ajustes_iniciar();assert(gravar());
  assert(dados_gravar("perfis.txt","1\t0\t1\t0\t336699\tParent\t\t\n2\t0\t0\t0\t336699\tKid\t\t\n"));perfis_carregar_ativo();
  admin_pin_definir("9147");admin_perfil_pin_definir(1,"4826");
  CHECK(perfis_verificar_pin(1,"9147")==1,"P0 account administrator recovers local profile PIN");
  for(int profile=2;profile<=32;profile++)assert(admin_perfil_pin_definir(profile,"4826"));
  perfis_esquecer();int clean=!admin_pin_definido();for(int profile=1;profile<=32;profile++)clean&=!admin_perfil_pin_definido(profile);
  CHECK(clean,"P0 logout forgets every account PIN");
  assert(dados_gravar("perfis.txt","1\t0\t1\t0\t336699\tParent\t\t\n2\t0\t0\t0\t336699\tKid\t\t\n"));perfis_carregar_ativo();
  int restored=1;
  for(int s=0;s<=nSecoes;s++){triIniciar();triFocarSecao(s);press(SDLK_RETURN);press(SDLK_ESCAPE);restored&=triIlha==TRI_SECOES && focoIndice && triSecFoco==s;}
  CHECK(restored,"P1 Back restores originating section row");
  triIniciar();triIlha=TRI_CHIPS;triChip=TRI_C_TOUR;press(SDLK_RETURN);int wasOpen=tourAberto;press(SDLK_ESCAPE);
  CHECK(wasOpen && !tourAberto && triIlha==TRI_CHIPS && triChip==TRI_C_TOUR,"P1 tour returns to its chip");
  // Palco: a section holds block rows; the child editor lives inside a block blade,
  // so open the first block and walk to its first choice row.
  triIniciar();triEntrar(1);
  if(triBladeLayout()){press(SDLK_RIGHT);for(int i=0;i<12 && !(focoOp>=0 && ajTipo(focoOp)==AJL_ESC && !inativa(focoOp));i++)press(SDLK_DOWN);}
  else for(int i=0;i<6;i++)press(SDLK_DOWN);
  int row=focoItem;press(SDLK_RETURN);press(SDLK_ESCAPE);
  CHECK(triIlha==TRI_LISTA && focoItem==row && !uxEditor,"P1 child editor returns to originating row");
  // Palco (mock claude-c): Right drills one blade per press by design and the
  // palette blade owns <- ->, so Back costs one press per open blade.
  triIniciar();triEntrar(0);for(int i=0;i<6;i++)press(SDLK_RIGHT);
  int depth=triBladeLayout()?triProfundidade():1;for(int i=0;i<6;i++)press(SDLK_LEFT);
  int backs=0;while(!focoIndice && backs<10){press(SDLK_ESCAPE);backs++;}
  CHECK(focoIndice && backs<=depth,"P1 Right never leaves the row; Left returns to the sections in one press");
  Stream source={0};snprintf(source.url,sizeof source.url,"https://example.invalid/video.mp4");stream_definir_lista(&source,1);
  triIniciar();focarOpcao(AJ_FONTE_MANUAL);sair=0;int manual=valor[AJ_FONTE_MANUAL];press(SDLK_RETURN);
  CHECK(!sair && !stream_folha_escolheu(NULL) && valor[AJ_FONTE_MANUAL]!=manual,"P1 Settings source row only flips the switch, never queues playback");stream_definir_lista(NULL,0);
  triIniciar();memcpy(valor,valorPadrao,sizeof valor);valor[AJ_QUALIDADE]=3;valor[AJ_GPU_EFEITOS]=1;
  triUtilAbrir(TRI_U_DIF);triAcao=1;rvUndoN=0;int differences=uxNDifs;press(SDLK_RETURN);
  CHECK(triUtil==TRI_U_REVISAO && rvN==differences,"P1 reset review equals all listed profile and TV differences");
  CHECK(rvAplicar() && valor[AJ_QUALIDADE]==valorPadrao[AJ_QUALIDADE] && valor[AJ_GPU_EFEITOS]==valorPadrao[AJ_GPU_EFEITOS],"P1 reset applies both scopes");
  AjusteBuscaResultado direct[AJ_N];int total=ajustes_buscar("legenda",direct,AJ_N),all=total>0;
  for(int i=0;i<total;i++){int op=direct[i].op;triIniciar();focarOpcao(op);all&=focoOp==op && triIlha==TRI_LISTA && visivel(focoItem);}
  CHECK(all,"P1 every subtitle search hit lands on its row");
  const int secondary[]={AJ_LEG2_ATRASO,AJ_LEG2_NEGRITO,AJ_LEG2_OPACIDADE,AJ_LEG2_COR_BORDA,AJ_LEG2_POSICAO};int reachable=1;
  for(int i=0;i<5;i++){triIniciar();focarOpcao(secondary[i]);reachable&=focoOp==secondary[i] && visivel(focoItem);}
  CHECK(reachable,"P1 secondary subtitle hits land on the section containing the setting");
  int normal=1;const int moved[]={AJ_FONTE_ORDEM,AJ_TMDB_LIGADO};
  for(int i=0;i<2;i++){triIniciar();focarOpcao(moved[i]);normal&=focoOp==moved[i] && uxSecaoDe(moved[i])==secAtual;}
  CHECK(normal,"P2 addon order and TMDB have normal section routes");
  ajustes_abrir_opcao(AJ_LEG_FORCADA);ajustes_iniciar();int hit=focoOp==AJ_LEG_FORCADA && uxVeioBusca;
  CHECK(hit,"P1 search (Spotlight) opens the matched setting row");
  press(SDLK_ESCAPE);CHECK(ajustes_pediu_busca()==2 && !uxVeioBusca,"P1 Back from a search hit returns to the search");uxCancelar();
  perfis_esquecer();assert(dados_gravar("perfis.txt","1\t0\t1\t0\t336699\tKid\t\t\n"));perfis_carregar_ativo();
  dados_apagar("profile-pin-1.txt");
  triIniciar();assert(perfis_ativo()==1 && perfis_n()==1);assert(perfil_infantil_definir(1,1));assert(perfil_regra_definir(1,PR_TEMPO,1));for(int i=0;i<61;i++)perfil_regra_tempo(1);
  CHECK(perfil_regra_tempo(0),"P1 kid fixture actually reaches time limit");
  CHECK(!perfis_sem_escolha(),"P1 expired single kid cannot auto-enter picker");
  perfil_infantil_definir(1,0);perfil_regras_esquecer();
#ifdef OPUS_PARENT_TARGET
  perfis_esquecer();assert(dados_gravar("perfis.txt","1\t0\t1\t0\t336699\tParent\t\t\n2\t0\t0\t0\t336699\tKid\t\t\n"));perfis_carregar_ativo();
  triIniciar();triEntrar(nSecoes);triLinha=TRI_P_PERFIL;press(SDLK_RETURN);
  CHECK(pfTela==PF_LIMITES,"P1 parent opens target profile chooser");pfAdminOk=perfis_ativo();pfFoco=1;pfEvento(SDLK_RETURN);
  CHECK(pfParentalTarget==2 && pfTela==PF_FECHADA,"P1 parent selects kid through admin gate");
  assert(admin_perfil_pin_definir(1,"4826"));pfExigirAdmin(PA_REMOVER_PIN,1);
  CHECK(!admin_perfil_pin_definido(1),"P0 administrator removes forgotten local profile PIN");
  triLinha=TRI_P_TEMPO;press(SDLK_RETURN);CHECK(ajPenParentEditor==PR_TEMPO,"P1 parental limit opens its chooser");
  ajPenParentDraft=60;press(SDLK_RETURN);
  CHECK(perfil_regra(2,PR_TEMPO)==60 && perfil_regra(1,PR_TEMPO)==0 && ajPenParentEditor<0,"P1 parental editor writes target only");
  triLinha=TRI_P_BUSCA;int searchRule=perfil_regra(2,PR_BUSCA_LIVRE);press(SDLK_RETURN);
  CHECK(perfil_regra(2,PR_BUSCA_LIVRE)!=searchRule && ajPenParentEditor<0,"P2 parental switch flips selected child rule directly");
  perfil_infantil_definir(1,1);ajPenParentOpen(PR_TEMPO);CHECK(ajPenParentEditor<0,"P1 kid cannot edit parental limits");perfil_infantil_definir(1,0);
  press(SDLK_ESCAPE);CHECK(!triUtil && triIlha==TRI_SECOES && triSecFoco==nSecoes,"P1 Back from parental returns to its section row");
#else
  CHECK(0,"P1 parent opens target profile chooser");
  CHECK(0,"P1 parent selects kid through admin gate");
  CHECK(0,"P1 parental editor writes target only");
  CHECK(0,"P1 kid cannot edit parental limits");
#endif
  triIniciar();focarOpcao(AJ_PAUSA_OVERLAY);int previous=valor[AJ_PAUSA_OVERLAY];press(SDLK_RETURN);CHECK(valor[AJ_PAUSA_OVERLAY]!=previous && !uxEditor,"P2 switch flips directly");
  triIniciar();ajPenSavedProfile=0;CHECK(ajPenSavedSave("Delete me"),"P2 custom preset saved");
#ifdef OPUS_PARENT_TARGET
  CHECK(ajPenSavedDelete(0) && !ajPenSavedCount,"P2 custom preset deleted");ajPenSavedProfile=0;ajPenSavedLoad();CHECK(!ajPenSavedCount,"P2 deletion persists");
#endif
  printf("%d regression failure(s)\n",failures);return failures?1:0;
}
