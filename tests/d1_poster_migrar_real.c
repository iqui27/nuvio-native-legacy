// #739: prov= em posteres.txt precisa chegar via gravar() da troca de
// provedor (AJ_POSTER_PROV nos Ajustes), nao so via pstDefinir de campo.
// Linka o ajustes.c de verdade, nao um espelho: o teste dirige o fluxo
// "escolheu Posters+ -> gravar() -> reinicia -> continua Posters+".
#include "../src/ajustes.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  char dir[] = "/tmp/nuvio-d1-739-XXXXXX";
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(dir);
  ajustes_dir(dir);

  // Instalacao NOVA: escolhe Posters+ (3) sem editar campo de texto e grava.
  valor[AJ_POSTER_PROV] = PP_POSTERSPLUS;
  assert(ajustes_teste_poster_prov_persiste(PP_POSTERSPLUS) == PP_POSTERSPLUS);
  { char *t = dados_ler("posteres.txt");
    assert(t && strstr(t, "prov=3")); free(t); }

  // Reinicia (le os arquivos do disco): posterProvLocal=3 fica Posters+,
  // NAO migra para Modelo.
  assert(ajustes_teste_poster_migrar(PP_POSTERSPLUS, "", "") == PP_POSTERSPLUS);

  // Instalacao ANTIGA (sem prov=): inst+modelo juntos -> migra 3 para Modelo.
  assert(ajustes_teste_poster_migrar(-1, "posters.meu.com", "https://x/{imdb}.jpg") == PP_MODELO);
  assert(ajustes_teste_poster_migrar(-1, "", "") == PP_MODELO);

  // Arquivo NOVO com prov=4 (Modelo): mesmo com inst velha nao volta atras.
  assert(ajustes_teste_poster_migrar(PP_MODELO, "posters.meu.com", "https://x/{imdb}.jpg") == PP_MODELO);

  puts("d1_poster_migrar_real: ok");
  return 0;
}
