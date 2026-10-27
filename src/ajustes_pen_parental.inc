#include "perfilregras.h"
#include "admin.h"
static int ajPenParentEditor=-1,ajPenParentDraft,ajPenPinAsk,ajPenPinRepeat,ajPenPinProfile;
static char ajPenPinDraft[ADMIN_PIN_MAX + 2];
static const int AJ_PEN_TIME[]={0,30,60,90,120,180,240};
static const int AJ_PEN_AGE[]={0,10,12,14,16,18};
static const char *const AJ_PEN_PARENT_LABEL[]={"Tempo de tela por perfil","Classificação máxima","Busca sem filtro","Compras bloqueadas","Filmes permitidos"};
static void ajPenParentText(int rule,int value,char *out,size_t n) {
  if(rule==PR_TEMPO) {
    if(!value)snprintf(out,n,"%s",i18n("Sem limite"));
    else if(value%60==0)snprintf(out,n,i18n("%d h"),value/60);
    else snprintf(out,n,i18n("%d min"),value);
  } else if(rule==PR_IDADE || rule==PR_FILMES) {
    if(!value)snprintf(out,n,"%s",i18n("Livre"));
    else snprintf(out,n,i18n("%d anos"),value);
  } else snprintf(out,n,"%s",i18n(value?"Ligado":"Desligado"));
}
static void ajPenParentOpen(int rule) {
  if(pfTravado() || perfil_infantil_ativo()) { uxNotificar("Indisponível em perfil infantil.");return; }
  ajPenParentEditor=rule;ajPenParentDraft=perfil_regra(perfis_ativo(),rule);
}
static void ajPenPinOpen(void) {
  if(ajPenPinRepeat<=0)ajPenPinProfile=perfis_ativo();
  if(ajPenPinRepeat<0)teclado_abrir_com("PIN do perfil","Use o mesmo PIN do perfil.",8,"0123456789",NULL);
  else if(ajPenPinRepeat)teclado_abrir_com("Repita o PIN do perfil","Digite o mesmo PIN de novo.",4,"0123456789",NULL);
  else teclado_abrir_com("PIN do perfil","Um código de 4 dígitos",4,"0123456789",NULL);
  teclado_tipo(TECLADO_TIPO_SENHA);teclado_mascarar(1);
}
static int ajPenPinDigitado(const char *pin) {
  if(ajPenPinProfile!=perfis_ativo() || perfil_infantil_ativo() || pfTravado()) { memset(ajPenPinDraft,0,sizeof ajPenPinDraft);ajPenPinAsk=ajPenPinRepeat=0;return 0; }
  if(ajPenPinRepeat<0) {
    if(!admin_perfil_pin_conferir(perfis_ativo(),pin)) { ajPenPinAsk=ajPenPinRepeat=0;uxNotificar("PIN incorreto");return 0; }
    ajPenPinRepeat=0;ajPenPinOpen();return 1;
  }
  if(!ajPenPinRepeat) {
    size_t n=strlen(pin);int valid=n==4;
    for(size_t i=0;i<n;i++)if(pin[i]<'0' || pin[i]>'9')valid=0;
    if(!valid) { ajPenPinAsk=0;uxNotificar("Um código de 4 dígitos");return 0; }
    snprintf(ajPenPinDraft,sizeof ajPenPinDraft,"%s",pin);ajPenPinRepeat=1;ajPenPinOpen();return 1;
  }
  int equal=!strcmp(pin,ajPenPinDraft);
  memset(ajPenPinDraft,0,sizeof ajPenPinDraft);ajPenPinAsk=ajPenPinRepeat=0;
  if(!equal || !admin_perfil_pin_definir(perfis_ativo(),pin)) { uxNotificar("Não foi possível guardar o PIN.");return 0; }
  uxNotificar("PIN do perfil criado.");return 0;
}
static int ajPenParentEvent(SDL_Keycode key) {
  if(ajPenPinAsk) { if(uxVoltar(key))ajPenPinAsk=ajPenPinRepeat=0;return 1; }
  if(ajPenParentEditor<0)return 0;
  if(uxVoltar(key)) { ajPenParentEditor=-1;return 1; }
  int rule=ajPenParentEditor;
  if(uxOk(key)) {
    if(perfil_regra_definir(perfis_ativo(),rule,ajPenParentDraft)) { ajPenParentEditor=-1;uxNotificar("Ajuste salvo nesta TV."); }
    else uxNotificar("Não foi possível salvar. O valor anterior foi mantido.");
    return 1;
  }
  int direction=key==SDLK_DOWN || key==SDLK_RIGHT?1:key==SDLK_UP || key==SDLK_LEFT?-1:0;
  const int *choices=rule==PR_TEMPO?AJ_PEN_TIME:AJ_PEN_AGE;
  int count=rule==PR_TEMPO?7:rule==PR_IDADE || rule==PR_FILMES?6:2;
  if(rule==PR_BUSCA_LIVRE || rule==PR_COMPRAS) { if(direction)ajPenParentDraft=!ajPenParentDraft;return 1; }
  for(int i=0;i<count;i++)if(choices[i]==ajPenParentDraft) { int next=i+direction;if(next>=0 && next<count)ajPenParentDraft=choices[next];break; }
  return 1;
}
