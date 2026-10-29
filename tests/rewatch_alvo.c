// Regressao: a reprise ignora os checks antigos, respeita parcial e estreia.
#include "rewatch.h"
#include "catalogo.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int perfis_ativo(void) { return 1; }
int sessao_logada(void) { return 0; }
const char *sessao_usuario(void) { return ""; }
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *s) { (void)n; (void)s; return 1; }
int dados_apagar(const char *n) { (void)n; return 1; }
int main(void) {
  CatEp eps[4] = {{0}};
  int t, e, pct;
  eps[0].temporada=0; eps[0].episodio=1;
  eps[1].temporada=1; eps[1].episodio=1;
  eps[2].temporada=2; eps[2].episodio=1;
  eps[3].temporada=2; eps[3].episodio=2;
  strcpy(eps[3].data,"2099-01-01");
  assert(!rewatch_alvo("tt123",eps,4,&t,&e,&pct));
  assert(rewatch_iniciar("tt123",RW_ORIGEM_TITULO));
  assert(rewatch_alvo("tt123",eps,4,&t,&e,&pct) && t==1 && e==1 && pct==0);
  rewatch_registrar("tt123",1,1);
  assert(rewatch_alvo("tt123",eps,4,&t,&e,&pct) && t==2 && e==1);
  rewatch_parcial("tt123",2,1,42);
  assert(rewatch_alvo("tt123",eps,4,&t,&e,&pct) && pct==42);
  rewatch_registrar("tt123",2,1);
  assert(!rewatch_alvo("tt123",eps,4,&t,&e,&pct));
  rewatch_encerrar("tt123");
  assert(!rewatch_vezes("tt123",1,1));
  assert(!rewatch_alvo("tt123",eps,4,&t,&e,&pct));
  puts("PASS: rewatch target, season boundary, partial, future and end");
}
