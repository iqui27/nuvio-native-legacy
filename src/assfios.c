// Ver assfios.h.
#include "assfios.h"

#if defined(NV_TPK) && defined(__linux__)
#include <stdio.h>
#include <sys/prctl.h>

void assfios_nomear(const char *nome) {
  char n[16];
  snprintf(n, sizeof n, "%s", nome ? nome : "");
  prctl(PR_SET_NAME, (unsigned long)n, 0, 0, 0);
}

void assfios_nome_trocar(const char *nome, char *antigo) {
  antigo[0] = 0;
  if (prctl(PR_GET_NAME, (unsigned long)antigo, 0, 0, 0) != 0) antigo[0] = 0;
  antigo[15] = 0;
  assfios_nomear(nome);
}

void assfios_nome_voltar(const char *antigo) {
  if (antigo && antigo[0]) assfios_nomear(antigo);
}

#else
void assfios_nomear(const char *nome) { (void)nome; }
void assfios_nome_trocar(const char *nome, char *antigo) { (void)nome; if (antigo) antigo[0] = 0; }
void assfios_nome_voltar(const char *antigo) { (void)antigo; }
#endif
