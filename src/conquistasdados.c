#include "conquistasdados.h"
#include "dados.h"
#include "perfis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Um arquivo por perfil, como agenda/central. Perfil <= 0 usa o nome sem sufixo.
static const char *arquivo(void) {
  static char nome[48];
  int p = perfis_ativo();
  if (p <= 0) snprintf(nome, sizeof nome, "conquistas.txt");
  else snprintf(nome, sizeof nome, "conquistas-p%d.txt", p);
  return nome;
}

void cqd_carregar(CqEstado *s) {
  char *txt = dados_ler(arquivo());
  cqregra_ler(txt, s);
  free(txt);
}

int cqd_gravar(const CqEstado *s) {
  static char buf[CQ_MAX * (CQ_CHAVE_MAX + CQ_NOME_MAX + 64)];
  if (!cqregra_serializar(s, buf, sizeof buf)) return 0;
  return dados_gravar(arquivo(), buf);
}

int cqd_raridade(const char *chave) { (void)chave; return -1; }

void cqd_mapa(CqMapa *m) { memset(m, 0, sizeof *m); }

int cqd_amigos_na_trilha(const char *chave) { (void)chave; return 0; }

int cqd_comunidade(void) { return 0; }
int cqd_temporada(void) { return 0; }
