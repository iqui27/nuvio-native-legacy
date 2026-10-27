#include "fontescanais.h"
#include <stdio.h>
#include <string.h>

static FontesCanais estado;
const FontesCanais *fontescanais_estado(void) { return &estado; }
void fontescanais_limpar(void) { memset(&estado,0,sizeof estado); }
void fontescanais_iniciar(const char *titulo) {
  fontescanais_limpar();
  estado.ativo=estado.buscando=1;estado.perfil=nv_perfil_sessao_ler();
  snprintf(estado.titulo,sizeof estado.titulo,"%s",titulo?titulo:"");
}
void fontescanais_publicar(const FonteCanal *lista,int n,int buscando) {
  if (!estado.ativo) return;
  if (!nv_perfil_sessao_atual(estado.perfil)) { fontescanais_limpar();return; }
  if (n<0 || !lista) n=0;
  if (n>FONTESCANAIS_MAX) n=FONTESCANAIS_MAX;
  if (n) memmove(estado.canais,lista,(size_t)n*sizeof *lista);
  estado.n=n;estado.buscando=buscando!=0;
}
int fontescanais_escolher(int i,time_t agora) {
  if (!estado.ativo || !nv_perfil_sessao_atual(estado.perfil) || i<0 || i>=estado.n) return 0;
  /* A future EPG match still points to a live channel, not a recording. */
  (void)agora;
  snprintf(estado.escolha,sizeof estado.escolha,"%s",estado.canais[i].id);
  return estado.escolha[0]!=0;
}
int fontescanais_consumir(char *id,size_t n) {
  if (!id || !n || !estado.escolha[0]) return 0;
  if (!nv_perfil_sessao_atual(estado.perfil)) { fontescanais_limpar();return 0; }
  snprintf(id,n,"%s",estado.escolha);estado.escolha[0]=0;return 1;
}
void fontescanais_atualizar(void) { estado.atualizar=1; }
int fontescanais_pediu_atualizar(void) { int v=estado.atualizar;estado.atualizar=0;return v; }
