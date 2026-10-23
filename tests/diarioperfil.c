// PERFIL E STATS DO DIARIO: o snapshot sem Trakt. Sem SDL de verdade (so os
// tipos), sem rede, sem disco (dubles).
#include "diarioperfil.h"
#include "diario.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int perfis_ativo(void) { return 1; }
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *c) { (void)n; (void)c; return 1; }
const char *i18n(const char *s) { return s; }

static long long em(int a, int m, int d, int h) {
  struct tm t; memset(&t, 0, sizeof t);
  t.tm_year = a - 1900; t.tm_mon = m - 1; t.tm_mday = d; t.tm_hour = h; t.tm_isdst = -1;
  return (long long)mktime(&t);
}
static void por(const char *id, long long q, int seg, int dur, char orig, unsigned fl) {
  DiarioEntrada e; memset(&e, 0, sizeof e);
  snprintf(e.id, sizeof e.id, "%s", id); e.quando = q; e.seg = seg; e.durSeg = dur;
  e.origem = orig; e.hora = -1; e.flags = (unsigned short)fl;
  assert(diario_adicionar(1, &e) == 1);
}

int main(void) {
  PerfilDados d;
  long long agora = em(2026, 10, 6, 21);
  assert(!diario_perfil_dados(1, &d, agora));          // vazio: a tela fica como estava

  diario_nome_definir("tt0001", "Duna", 2021, "https://img/duna.jpg");
  diario_nome_definir("tt0900", "Ruptura", 2022, "");
  por("tt0001", em(2026, 10, 6, 20), 9000, 9300, 'p', 0);        // hoje, 150 min
  por("tt0900:1:1", em(2026, 10, 5, 22), 3000, 3100, 'p', 0);    // ontem, 50 min
  por("tt0900:1:2", em(2026, 10, 4, 22), 0, 0, 'c', DIARIO_F_DATA_APROX);   // conta: sem minutos
  por("tt0900:1:3", em(2026, 10, 4, 23), 0, 3200, 'p', 0);       // player sem seg: usa a duracao
  por("tt0002", em(2026, 9, 30, 21), 6000, 6000, 'p', 0);        // mes passado: sequencia sim, totais nao
  por("tt0003", em(2026, 10, 2, 21), 600, 7000, 'p', DIARIO_F_PARCIAL);   // sono parcial: fora
  por("tt0004", em(2025, 12, 31, 21), 6000, 6000, 'p', 0);       // ano passado

  assert(diario_perfil_dados(1, &d, agora));
  assert(!strcmp(d.periodo, "Outubro 2026"));
  assert(d.plays == 4 && d.filmes == 1 && d.episodios == 3);
  assert(d.minutos == 150 + 50 + 53);
  assert(d.nDias == 31 && d.atividade[5] == 1 && d.atividade[4] == 1 && d.atividade[3] == 2);
  assert(d.diasAtivosMes == 3);
  assert(d.streakAtual == 3 && d.streakCompleto == 1);   // 6, 5 e 4; o dia 3 ficou vazio
  assert(d.diasAtivosAno == 4 && d.anoCompleto == 1);    // 30/09 conta; o parcial nao
  assert(d.nDestaques == 2);
  assert(!strcmp(d.destaques[0].titulo, "Ruptura") && d.destaques[0].plays == 3);
  assert(!strcmp(d.destaques[0].detalhe, "T1E1"));   // o mais recente
  assert(strstr(d.destaques[0].poster, "metahub.space/poster/medium/tt0900"));
  assert(!strcmp(d.destaques[1].titulo, "Duna") && !strcmp(d.destaques[1].poster, "https://img/duna.jpg"));
  assert(d.aviso[0] && !d.parcial);
  // Sem nome conhecido e sem imdb: nao vira destaque com id cru.
  por("kitsu:1", em(2026, 10, 6, 10), 600, 600, 'p', 0);
  por("kitsu:1", em(2026, 10, 6, 20) + 30000, 600, 600, 'p', 0);
  assert(diario_perfil_dados(1, &d, agora) && d.nDestaques == 2);
  printf("diarioperfil: ok\n");
  return 0;
}
