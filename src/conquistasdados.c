#include "conquistasdados.h"
#include "dados.h"
#include "perfis.h"
#include "sessao.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Conta (sub do JWT, ate 79 bytes em sessao.c) e perfil. Hex evita caminhos
// e colisoes por remover caracteres; arquivos antigos sem conta nao migram.
static const char *arquivo(void) {
  static char nome[192];
  char conta[160];
  const unsigned char *u = (const unsigned char *)sessao_usuario();
  size_t n = 0;
  while (*u && n + 2 < sizeof conta) {
    snprintf(conta + n, 3, "%02x", *u++); n += 2;
  }
  conta[n] = 0;
  snprintf(nome, sizeof nome, "conquistas-%s-p%d.txt", n ? conta : "anon", perfis_ativo());
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
