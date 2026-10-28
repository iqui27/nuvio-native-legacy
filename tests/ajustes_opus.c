// Opus regressions exercise production Settings handlers with isolated storage.
#include "../src/ajustes.c"
#include "nvtmp.h"
#include <assert.h>
static int failures;
#define CHECK(ok, name) do { int passed=!!(ok);printf("%s: %s\n",passed?"PASS":"FAIL",name);if(!passed)failures++; } while(0)
static void press(SDL_Keycode key) { aj4Evento(key); }
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
  int restored=1;const int indexRows[]={0,1,2,3,9,10,11,12};
  for(int i=0;i<8;i++){aj4Iniciar();focoIndice=1;aj4Linha=indexRows[i];press(SDLK_RETURN);press(SDLK_ESCAPE);restored&=focoIndice && aj4Linha==indexRows[i];}
  CHECK(restored,"P1 Back restores originating index card");
  aj4Iniciar();focoIndice=1;aj4Linha=7;press(SDLK_RETURN);ajPenRow=7;press(SDLK_RETURN);press(SDLK_ESCAPE);
  CHECK(ajPenPage==AJ_PEN_TOUR && ajPenRow==7,"P1 overview child returns to overview row");
  aj4Iniciar();ajPenOpen(27);ajPenRow=6;press(SDLK_RETURN);press(SDLK_ESCAPE);
  CHECK(ajPenPage==27 && ajPenRow==6,"P1 child returns to originating row");
  aj4Iniciar();focoIndice=1;aj4Linha=0;press(SDLK_RETURN);for(int i=0;i<6;i++)press(SDLK_RIGHT);for(int i=0;i<6;i++)press(SDLK_LEFT);
  int backs=0;while(!focoIndice && backs<10){press(SDLK_ESCAPE);backs++;}
  CHECK(focoIndice && backs<=2,"P1 Back after Left/Right paging returns to index in two presses");
  aj4Iniciar();ajPenOpen(AJ_PEN_SOURCES);
  CHECK(ajPenRow==-1,"P1 empty sources retain visible focused manual row");
  Stream source={0};snprintf(source.url,sizeof source.url,"https://example.invalid/video.mp4");stream_definir_lista(&source,1);
  ajPenRow=0;sair=0;press(SDLK_RETURN);CHECK(!sair && !stream_folha_escolheu(NULL),"P1 Settings source OK never queues playback");stream_definir_lista(NULL,0);
  aj4Iniciar();memcpy(valor,valorPadrao,sizeof valor);valor[AJ_QUALIDADE]=3;valor[AJ_GPU_EFEITOS]=1;
  aj4Util=1;focoIndice=0;aj4Acao=1;aj4UndoN=0;int differences=aj4Diferencas(aj4Opcoes);press(SDLK_RETURN);
  CHECK(aj4Review && aj4N==differences,"P1 reset review equals all listed profile and TV differences");
  CHECK(aj4Aplicar() && valor[AJ_QUALIDADE]==valorPadrao[AJ_QUALIDADE] && valor[AJ_GPU_EFEITOS]==valorPadrao[AJ_GPU_EFEITOS],"P1 reset applies both scopes");
  aj4Iniciar();snprintf(aj4Consulta,sizeof aj4Consulta,"legenda");int n=aj4BuscarResultados();AjusteBuscaResultado direct[AJ_N];int total=ajustes_buscar(aj4Consulta,direct,AJ_N),all=1;
  for(int i=0;i<total;i++){int found=0;for(int j=0;j<n;j++)found|=direct[i].op==aj4Resultados[j].op;all&=found;}
  CHECK(all,"P1 subtitle search preserves every individual match");
  const int secondary[]={AJ_LEG2_ATRASO,AJ_LEG2_NEGRITO,AJ_LEG2_OPACIDADE,AJ_LEG2_COR_BORDA,AJ_LEG2_POSICAO};int reachable=1;
  for(int i=0;i<5;i++){ajPenPage=-1;ajPenFind(secondary[i]);const AjPenPage *page=ajPenCurrent();int found=0;if(page)for(int c=0;c<2;c++)for(int j=0;j<page->cols[c].n;j++)found|=page->cols[c].rows[j].op==secondary[i];reachable&=found && ajPenPage==AJ_PEN_SECONDARY;}
  CHECK(reachable,"P1 secondary subtitle hits land on normal page containing setting");
  int normal=1;const int moved[]={AJ_FONTE_ORDEM,AJ_TMDB_LIGADO};
  for(int i=0;i<2;i++){ajPenPage=-1;ajPenFind(moved[i]);normal&=ajPenPage>=29 && ajPenPage<46;}
  CHECK(normal,"P2 addon order and TMDB have normal page routes");
  aj4Iniciar();snprintf(aj4Consulta,sizeof aj4Consulta,"forçada");n=aj4BuscarResultados();int hit=-1;for(int i=0;i<n;i++)if(aj4Resultados[i].op==AJ_LEG_FORCADA)hit=i;
  if(hit>=0){aj4Util=3;focoIndice=0;aj4Linha=hit;press(SDLK_RETURN);}
  CHECK(hit>=0 && uxEditor && uxOp==AJ_LEG_FORCADA,"P1 search opens matched setting editor");uxCancelar();
  perfis_esquecer();assert(dados_gravar("perfis.txt","1\t0\t1\t0\t336699\tKid\t\t\n"));perfis_carregar_ativo();
  dados_apagar("profile-pin-1.txt");
  aj4Iniciar();assert(perfis_ativo()==1 && perfis_n()==1);assert(perfil_infantil_definir(1,1));assert(perfil_regra_definir(1,PR_TEMPO,1));for(int i=0;i<61;i++)perfil_regra_tempo(1);
  CHECK(perfil_regra_tempo(0),"P1 kid fixture actually reaches time limit");
  CHECK(!perfis_sem_escolha(),"P1 expired single kid cannot auto-enter picker");
  perfil_infantil_definir(1,0);perfil_regras_esquecer();
#ifdef OPUS_PARENT_TARGET
  perfis_esquecer();assert(dados_gravar("perfis.txt","1\t0\t1\t0\t336699\tParent\t\t\n2\t0\t0\t0\t336699\tKid\t\t\n"));perfis_carregar_ativo();
  aj4Iniciar();focoIndice=0;ajPenOpen(AJ_PEN_PARENTAL);ajPenRow=7;press(SDLK_RETURN);
  CHECK(pfTela==PF_LIMITES,"P1 parent opens target profile chooser");pfAdminOk=perfis_ativo();pfFoco=1;pfEvento(SDLK_RETURN);
  CHECK(pfParentalTarget==2 && pfTela==PF_FECHADA,"P1 parent selects kid through admin gate");
  assert(admin_perfil_pin_definir(1,"4826"));pfExigirAdmin(PA_REMOVER_PIN,1);
  CHECK(!admin_perfil_pin_definido(1),"P0 administrator removes forgotten local profile PIN");
  ajPenParentOpen(PR_TEMPO);ajPenParentDraft=60;ajPenParentEvent(SDLK_RETURN);
  CHECK(perfil_regra(2,PR_TEMPO)==60 && perfil_regra(1,PR_TEMPO)==0,"P1 parental editor writes target only");
  ajPenRow=10;int searchRule=perfil_regra(2,PR_BUSCA_LIVRE);press(SDLK_RETURN);
  CHECK(perfil_regra(2,PR_BUSCA_LIVRE)!=searchRule && ajPenParentEditor<0,"P2 parental switch flips selected child rule directly");
  perfil_infantil_definir(1,1);ajPenParentOpen(PR_TEMPO);CHECK(ajPenParentEditor<0,"P1 kid cannot edit parental limits");perfil_infantil_definir(1,0);
#else
  CHECK(0,"P1 parent opens target profile chooser");
  CHECK(0,"P1 parent selects kid through admin gate");
  CHECK(0,"P1 parental editor writes target only");
  CHECK(0,"P1 kid cannot edit parental limits");
#endif
  aj4Iniciar();ajPenOpen(30);int previous=valor[AJ_PAUSA_OVERLAY];press(SDLK_RETURN);CHECK(valor[AJ_PAUSA_OVERLAY]!=previous && !uxEditor,"P2 switch flips directly");
  aj4Iniciar();ajPenSavedProfile=0;CHECK(ajPenSavedSave("Delete me"),"P2 custom preset saved");
#ifdef OPUS_PARENT_TARGET
  CHECK(ajPenSavedDelete(0) && !ajPenSavedCount,"P2 custom preset deleted");ajPenSavedProfile=0;ajPenSavedLoad();CHECK(!ajPenSavedCount,"P2 deletion persists");
#endif
  printf("%d regression failure(s)\n",failures);return failures?1:0;
}
