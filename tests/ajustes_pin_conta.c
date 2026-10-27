// Account PINs remain usable even when this TV has a local profile override.
#include "../src/perfis.c"
#include <assert.h>
static int localPin=1, principalPin=1, profilePin=1, offline;
int admin_perfil_pin_definido(int p){return localPin && p==2;}
int admin_perfil_pin_conferir(int p,const char *pin){return p==2 && !strcmp(pin,"2222");}
int admin_pin_conferir(const char *pin){return !strcmp(pin,"9999");}
char *sessao_rpc(const char *name,const char *body,int *status){
  assert(!strcmp(name,"verify_profile_pin"));
  if(offline){*status=503;return NULL;}
  *status=200;
  int ok=(profilePin && strstr(body,"\"p_profile_id\":2") && strstr(body,"3333")) ||
         (principalPin && strstr(body,"\"p_profile_id\":1") && strstr(body,"1111"));
  return strdup(ok?"true":"false");
}
int main(void){
  n=2;lista[0].indice=1;lista[0].primario=1;lista[0].temPin=1;lista[1].indice=2;lista[1].temPin=1;
  assert(perfis_verificar_pin(2,"2222")==1);
  assert(perfis_verificar_pin(2,"9999")==1);
  assert(perfis_verificar_pin(2,"3333")==1);
  assert(perfis_verificar_pin(2,"1111")==1);
  assert(perfis_verificar_pin(2,"0000")==0);
  offline=1;assert(perfis_verificar_pin(2,"3333")==-1);
  assert(perfis_verificar_pin(2,"2222")==1);
  puts("Account and local PIN recovery: OK");return 0;
}
