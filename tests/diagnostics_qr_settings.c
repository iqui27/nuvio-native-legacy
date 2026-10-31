// Verify real Settings defaults, persistence key, UI entry and sync exclusion.
#include "../src/ajustes.c"
#include <assert.h>
void fontefalha_filtro(FonteFiltroConfig *s){memset(s,0,sizeof *s);}
int fontefalha_provedores(void){return 0;}
int fontefalha_addons(void){return 0;}
int fontefalha_provedor_limite(int s){(void)s;return 0;}
int fontefalha_addon_limite(const char *s){(void)s;return 0;}
int main(void) {
  assert(AJ_DIAGNOSTICS_QR==AJ_BUFFER_CONVERSION_CONNECTIONS+1 && AJ_N==AJ_DIAGNOSTICS_QR+1);
  assert(ajustes_buffer_native_connections()==1);
  assert(ajustes_buffer_conversion_connections()==4);
  assert(valor[AJ_DIAGNOSTICS_QR]==1 && valorPadrao[AJ_DIAGNOSTICS_QR]==1);
  assert(!ajustes_diagnostics_qr());
  assert(!strcmp(ajVisualIcone(AJ_DIAGNOSTICS_QR),"aj_download"));
  assert(!strcmp(CHAVE[AJ_DIAGNOSTICS_QR],"diagnosticsQrLocal"));
  assert(ajustes_escopo(AJ_DIAGNOSTICS_QR)==ESC_TV);
  assert(!ajustePortavel(AJ_DIAGNOSTICS_QR) && !ajusteAparelho(AJ_DIAGNOSTICS_QR));
  int logs=valor[AJ_ENVIO_AUTO], local=valor[AJ_LOG_REDE], seen=0;
  for(unsigned i=0;i<sizeof TELA/sizeof *TELA;i++)
    if(TELA[i].tipo==IT_OPC && TELA[i].op==AJ_DIAGNOSTICS_QR) seen++;
  assert(seen==1);
  valor[AJ_DIAGNOSTICS_QR]=0; assert(ajustes_diagnostics_qr());
  valor[AJ_DIAGNOSTICS_QR]=1; assert(!ajustes_diagnostics_qr());
  assert(valor[AJ_ENVIO_AUTO]==logs && valor[AJ_LOG_REDE]==local);
  puts("PASS: Diagnostics QR default OFF, local scope, UI entry and independent normal logs");
}
