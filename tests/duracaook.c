// duracaook.c sem disco de verdade: "Manter esta fonte" guarda o titulo por
// conta e perfil, sobrevive a releitura, e o mais antigo sai no teto.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "duracaook.h"

static char *disco;
int dados_gravar(const char *nome, const char *c) { (void)nome; free(disco); disco = strdup(c); return 1; }
char *dados_ler(const char *nome) { (void)nome; return disco ? strdup(disco) : NULL; }

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok  " : "FAIL", o); if (!c) falhas++; }

int main(void) {
  ok(!duracaook_tem("conta", 1, "tt23770418"), "nothing kept yet");
  duracaook_guardar("conta", 1, "tt23770418");
  ok(duracaook_tem("conta", 1, "tt23770418"), "kept for this title");
  ok(!duracaook_tem("conta", 2, "tt23770418"), "another profile: not kept");
  ok(!duracaook_tem("outra", 1, "tt23770418"), "another account: not kept");
  ok(!duracaook_tem("conta", 1, "tt0000001"), "another title: not kept");
  ok(disco && !strstr(disco, "conta"), "the account is not written in clear");
  duracaook_zerar();
  ok(duracaook_tem("conta", 1, "tt23770418"), "survives a reload from disk");
  for (int i = 0; i < DURACAOOK_MAX; i++) { char t[32]; snprintf(t, sizeof t, "tt%d", i); duracaook_guardar("conta", 1, t); }
  ok(!duracaook_tem("conta", 1, "tt23770418"), "the oldest goes out at the limit");
  ok(duracaook_tem("conta", 1, "tt0") && duracaook_tem("conta", 1, "tt199"), "the newest stay");
  duracaook_guardar("conta", 1, "tt0");   // again: becomes the newest
  duracaook_guardar("conta", 1, "novo");
  ok(duracaook_tem("conta", 1, "tt0") && !duracaook_tem("conta", 1, "tt1"), "keeping a title again moves it to the newest");
  ok(!duracaook_tem("conta", 1, "") && !duracaook_tem("conta", 1, NULL), "empty title: never");
  if (falhas) { printf("duracaook: %d falha(s)\n", falhas); return 1; }
  printf("duracaook: ok\n");
  return 0;
}
