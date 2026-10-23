#define main existing_phone_tests
#include "celular.c"
#undef main
int main(void){
  char local[160],args[512],value[4096];
  assert(celular_abrir_pagina("Guia TV","<html><h1>source setup</h1><form method=post><input name=t></form></html>","<html>Review on TV</html>"));
  const char *token=strrchr(celular_url(),'/')+1;
  snprintf(local,sizeof local,"http://127.0.0.1:%d/%s",celular_porta(),token);
  snprintf(args,sizeof args,"'%s'",local);assert(curl(args)==200&&strstr(saida,"source setup"));
  snprintf(args,sizeof args,"--data-urlencode 't={\"type\":0,\"server\":\"https://provider.test\",\"user\":\"fixture\",\"password\":\"fixture\"}' '%s'",local);
  assert(curl(args)==200&&strstr(saida,"Review on TV"));
  assert(celular_pegar(value,sizeof value)&&strstr(value,"provider.test"));
  assert(!celular_pegar(value,sizeof value));celular_fechar();
  assert(celular_abrir("Busca"));
  token=strrchr(celular_url(),'/')+1;snprintf(local,sizeof local,"http://127.0.0.1:%d/%s",celular_porta(),token);
  snprintf(args,sizeof args,"'%s'",local);assert(curl(args)==200&&strstr(saida,"<textarea")&&!strstr(saida,"source setup"));
  celular_fechar();puts("PASS: guided setup reuses single-use pairing; generic keyboard restored");
}
