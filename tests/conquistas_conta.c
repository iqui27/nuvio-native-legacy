#include "conquistasdados.h"
#include "progresso.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *conta = "account-A";
static int perfil = 1;
const char *sessao_usuario(void) { return conta; }
int perfis_ativo(void) { return perfil; }
int ajustes_conquistas(void) { return 1; }
int prog_ler(ProgRegistro *r, int max) { (void)r; (void)max; return 0; }
int cat_historico_estado_tempo(const char *id, const char *tipo, long long *ms) { (void)id; (void)tipo; *ms = 0; return -1; }
long long prog_agora_ms(void) { return 1000; }
char *dados_ler(const char *nome) {
  FILE *f = fopen(nome, "rb");
  char *s = calloc(1, 32768);
  assert(s);
  if (f) { fread(s, 1, 32767, f); fclose(f); }
  return s;
}
int dados_gravar(const char *nome, const char *txt) {
  FILE *f = fopen(nome, "wb");
  assert(f); fputs(txt, f); fclose(f); return 1;
}
int main(void) {
  CqEstado a = {0}, b;
  cqregra_registrar(&a, "trilha:local", CQ_FAM_TRILHA, "Local", 2, 3, 6, 1);
  assert(cqd_gravar(&a));
  conta = "account-B";
  cqd_carregar(&b);
  assert(b.n == 0 && cqregra_pendentes(&b) == 0);
  conta = "account-A"; perfil = 2;
  cqd_carregar(&b); assert(b.n == 0);
  perfil = 1;
  cqd_carregar(&b);
  assert(b.n == 1 && b.e[0].nivel == 2 && b.e[0].vistos == 3 && cqregra_pendentes(&b) == 1);
  conta = "";
  cqd_carregar(&b); assert(b.n == 0);
  puts("PASS: conquistas_conta");
}
