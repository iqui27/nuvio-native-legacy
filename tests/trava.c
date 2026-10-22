// TRAVA DE SERIE: regra, arquivo e Juntos contra o Diario de verdade
// (dados e perfil sao dubles; sem SDL, sem rede).
#include "trava.h"
#include "catalogo.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char pasta[256];
static int seasonCount;
int cat_indice_por_imdb(const char *id) { return !strcmp(id, "tt1") ? 0 : -1; }
int cat_n_episodios(int ix) { return ix == 0 ? seasonCount : 0; }
const CatEp *cat_episodio(int ix, int i) {
  static CatEp ep;
  (void)ix;
  memset(&ep, 0, sizeof ep); ep.temporada = 1; ep.episodio = i + 1;
  return &ep;
}
int perfis_ativo(void) { return 1; }
char *dados_ler(const char *nome) {
  char c[512]; FILE *f; long n; char *b;
  snprintf(c, sizeof c, "%s/%s", pasta, nome);
  f = fopen(c, "rb"); if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1); fread(b, 1, (size_t)n, f); b[n] = 0; fclose(f);
  return b;
}
int dados_gravar(const char *nome, const char *c) {
  char p[512]; FILE *f;
  snprintf(p, sizeof p, "%s/%s", pasta, nome);
  f = fopen(p, "wb"); if (!f) return 0; fputs(c, f); fclose(f); return 1;
}
int dados_apagar(const char *nome) { char p[512]; snprintf(p, sizeof p, "%s/%s", pasta, nome); return remove(p) == 0; }

extern long long diario_relogio_teste;
static long long relogio = 1790000000LL;

static void viu(int perfil, int t, int e) {
  DiarioEntrada d;
  memset(&d, 0, sizeof d);
  snprintf(d.id, sizeof d.id, "tt1"); d.tipo = DIARIO_SERIE; d.origem = DIARIO_ORIG_PLAYER;
  d.temporada = (short)t; d.episodio = (short)e; d.hora = -1;
  d.quando = relogio += 7 * 3600;   // fora da janela de fusao
  assert(diario_adicionar(perfil, &d) >= 0);
}

int main(void) {
  Trava t;
  TravaEstado s;
  TravaPos v[TRAVA_MEMBROS];
  int p3[3] = {1, 2, 3}, p2[2] = {1, 2}, solo[1] = {1}, alvos[8], ids[8], id, id2;
  char cmd[600];

  snprintf(pasta, sizeof pasta, "%s/nuvio-trava-teste-%d", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (int)getpid());
  snprintf(cmd, sizeof cmd, "rm -rf '%s' && mkdir -p '%s'", pasta, pasta); assert(system(cmd) == 0);
  diario_relogio_teste = relogio;
  diario_dono("conta-a");
  trava_dono("conta-a");

  // ---- gestao
  assert(trava_criar("tt1", "Severance", TRAVA_SEPARADOS, 0, solo, 1) == -1);         // sozinho nao e trava
  assert(trava_criar("", "x", 0, 0, p2, 2) == -1);
  id = trava_criar("tt1", "Severance\tX", TRAVA_SEPARADOS, 1, p3, 3);
  assert(id > 0 && trava_n() == 1);
  assert(trava_obter(id, &t) && t.n == 3 && t.pin == 1 && !strcmp(t.titulo, "Severance X"));
  assert(trava_da_serie("tt1", 2, ids, 8) == 1 && ids[0] == id);
  assert(trava_da_serie("tt1", 4, ids, 8) == 0 && trava_da_serie("tt9", 0, ids, 8) == 0);

  // ---- regra Separados: ninguem comecou -> so o T1E1
  trava_obter(id, &t);
  assert(trava_liberado(&t, 1, 1, 1, 9) && !trava_liberado(&t, 1, 1, 2, 9));
  assert(trava_bloqueio("tt1", 1, 1, 2) == id);
  assert(!trava_bloqueio("tt1", 1, 1, 1));
  assert(!trava_bloqueio("tt1", 4, 1, 2));
  assert(trava_liberado(&t, 1, 0, 3, 9));                    // especial fora da regra
  viu(1, 1, 1);                                              // perfil 1 viu o E1
  assert(trava_estado(&t, 1, &s) && s.esperando && s.minT == 0 && s.minE == 0 && s.quem == 6);
  assert(!trava_liberado(&t, 1, 1, 2, 9));                   // 2 e 3 ainda nao comecaram
  assert(trava_liberado(&t, 1, 1, 1, 9));                    // rever e livre
  viu(2, 1, 1); viu(3, 1, 1);
  assert(trava_estado(&t, 1, &s) && !s.esperando && s.minE == 1);
  assert(trava_liberado(&t, 1, 1, 2, 9) && !trava_liberado(&t, 1, 1, 3, 9));   // ve o proximo, nao o seguinte
  viu(1, 1, 2); viu(1, 1, 3);
  assert(trava_estado(&t, 1, &s) && s.esperando && s.atras == 2 && s.quem == 6);
  assert(trava_quem_segura(&t, 1, 1, 4, 9) == 6);
  viu(2, 1, 2);
  assert(trava_estado(&t, 1, &s) && s.quem == 4 && s.atras == 2);        // 3 ainda no E1
  assert(trava_quem_segura(&t, 1, 1, 4, 9) == 4);
  assert(trava_quem_segura(&t, 2, 1, 3, 9) == 4);                         // 2 espera o 3
  assert(trava_liberado(&t, 3, 1, 3, 9));                                 // o que esta atras anda
  assert(trava_liberado(&t, 3, 1, 2, 9) && !trava_liberado(&t, 3, 1, 4, 9));
  // virada de temporada: so com a contagem da temporada
  viu(3, 1, 9);
  viu(2, 1, 9);
  assert(trava_estado(&t, 1, &s) && s.minE == 9);
  assert(!trava_liberado(&t, 1, 2, 1, 0));                  // contagem desconhecida: nao adivinha
  assert(trava_liberado(&t, 1, 1, 10, 0));                  // E+1 da mesma temporada
  seasonCount = 9;
  assert(!trava_bloqueio("tt1", 1, 2, 1));
  assert(trava_bloqueio("tt1", 1, 2, 2) == id);
  seasonCount = 0;
  assert(trava_bloqueio("tt1", 1, 2, 1) == id);
  assert(trava_liberado(&t, 1, 2, 1, 9));                   // T1 tem 9 -> T2E1 e o proximo
  assert(!trava_liberado(&t, 1, 2, 2, 9));
  // pausar congela as esperas
  assert(trava_definir_pausada(id, 1));
  trava_obter(id, &t);
  assert(trava_liberado(&t, 1, 2, 5, 9));
  trava_definir_pausada(id, 0);

  // ---- Juntos: ninguem espera; alvos = os outros membros
  assert(trava_definir_modo(id, TRAVA_JUNTOS));
  trava_obter(id, &t);
  assert(trava_liberado(&t, 1, 3, 1, 9));
  assert(trava_juntos_alvos("tt1", 1, alvos, 8) == 2 && alvos[0] == 2 && alvos[1] == 3);
  assert(trava_juntos_alvos("tt9", 1, alvos, 8) == 0);
  id2 = trava_criar("tt1", "", TRAVA_JUNTOS, 0, p2, 2);                  // 2a trava, sobreposta
  assert(trava_juntos_alvos("tt1", 1, alvos, 8) == 2);                    // sem repeticao
  trava_definir_pausada(id2, 1);
  assert(trava_juntos_alvos("tt1", 2, alvos, 8) == 2);                    // so a 1a vale
  assert(trava_apagar(id2) && trava_n() == 1);

  // ---- membros
  assert(trava_adicionar_membro(id, 4) && !trava_adicionar_membro(id, 4));
  assert(trava_remover_membro(id, 4) == 1);
  assert(trava_posicoes((trava_obter(id, &t), &t), v) == 3 && v[0].perfil == 1 && v[0].e == 3);

  // ---- arquivo: sobrevive; outra conta nao le
  trava_esquecer(); trava_iniciar();
  assert(trava_n() == 1 && trava_obter(id, &t) && t.modo == TRAVA_JUNTOS && t.n == 3 && t.pin == 1);
  trava_dono("conta-b"); trava_iniciar();
  assert(trava_n() == 0);
  trava_dono("conta-a"); trava_iniciar();
  assert(trava_n() == 1);
  id2 = trava_criar("tt2", "Dark", TRAVA_SEPARADOS, 0, p2, 2);
  assert(id2 > id);
  // tirar o penultimo membro desfaz a trava
  assert(trava_remover_membro(id2, 2) == 2 && trava_n() == 1);

  snprintf(cmd, sizeof cmd, "rm -rf '%s'", pasta); (void)!system(cmd);
  puts("trava: ok");
  return 0;
}
