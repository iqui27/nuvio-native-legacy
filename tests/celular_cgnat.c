/* Log page address: private ranges first, carrier-grade NAT (100.64/10) as the fallback. */
#include "../src/celular.c"
#include <assert.h>
void ajustes_acento(float*r,float*g,float*b){*r=*g=*b=1;}
int ajustes_tinta_foco(void){return 0;}
int ajustes_idioma(void){return 0;}
const char *i18n(const char*s){return s;}
static in_addr_t A(const char*s){in_addr_t a; inet_pton(AF_INET,s,&a); return a;}
int main(void){
  assert(cgnat(A("100.64.0.0")) && cgnat(A("100.80.125.104")) && cgnat(A("100.127.255.255")));
  assert(!cgnat(A("100.63.255.255")) && !cgnat(A("100.128.0.0")) && !cgnat(A("192.168.1.2")) && !cgnat(A("8.8.8.8")));
  assert(privado(A("192.168.1.2")) && !privado(A("100.80.125.104")));
  puts("cgnat ok"); return 0; }
