// "Oculto de Continuar assistindo" (#203): lista em disco por perfil, que volta
// com episodio novo. Dados em memoria, por nome de arquivo.
#include "progresso.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *arqs[4]; static char nomes[4][32];
static int perfil = 1;
static long long agora = 1757000000000LL;
static int slot(const char *n, int criar) {
  int i;
  for (i = 0; i < 4; i++) if (nomes[i][0] && !strcmp(nomes[i], n)) return i;
  if (!criar) return -1;
  for (i = 0; i < 4; i++) if (!nomes[i][0]) { snprintf(nomes[i], 32, "%s", n); return i; }
  return -1;
}
char *dados_ler(const char *n) { int i = slot(n, 0); return i >= 0 && arqs[i] ? strdup(arqs[i]) : NULL; }
int dados_gravar(const char *n, const char *c) { int i = slot(n, 1); free(arqs[i]); arqs[i] = strdup(c); return 1; }
int dados_apagar(const char *n) { int i = slot(n, 0); if (i >= 0) { free(arqs[i]); arqs[i] = NULL; } return 1; }
int perfis_ativo(void) { return perfil; }
static long long relogio(void) { return agora; }

int main(void) {
  prog_definir_relogio(relogio);
  // Nada oculto: nunca vence.
  assert(!prog_oculto_vence("tt100", 0));
  prog_ocultar_continuar("tt100:2:5");
  // O "a seguir" velho (instante anterior ao carimbo) fica fora; instante 0 tambem.
  assert(prog_oculto_vence("tt100", agora - 1000));
  assert(prog_oculto_vence("tt100:2:6", 0));
  assert(prog_oculto_vence("tt100", agora));          // empate fica oculto
  // Outra obra e outro perfil nao sao afetados.
  assert(!prog_oculto_vence("tt200", agora - 1000));
  perfil = 2; assert(!prog_oculto_vence("tt100", agora - 1000)); perfil = 1;
  // Sobrevive ao reinicio (recarrega do "disco").
  prog_invalidar();
  assert(prog_oculto_vence("tt100", agora - 1000));
  // Episodio novo (instante mais novo) devolve a obra; soltar apaga o registro.
  assert(!prog_oculto_vence("tt100", agora + 5000));
  assert(prog_oculto_soltar("tt100", agora + 5000));
  assert(!prog_oculto_vence("tt100", agora - 1000));
  assert(!prog_oculto_soltar("tt100", agora + 5000));
  // Assistir de novo AQUI (registro local mais novo) tambem solta.
  prog_ocultar_continuar("tt300");
  agora += 10000;
  assert(prog_gravar_local("tt300", 1, 2, 60.0, 3000.0, -1));
  assert(!prog_oculto_vence("tt300", 0));
  // Logout apaga tudo.
  prog_ocultar_continuar("tt400");
  prog_esquecer_tudo();
  assert(!prog_oculto_vence("tt400", 0));
  printf("cwoculto: ok\n");
  return 0;
}
