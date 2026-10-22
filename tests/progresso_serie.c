// "Seu progresso" / "% assistido" tem de acompanhar o mapa depois de marcar e
// desmarcar pelo menu (cenario do Silo no log da TCL, 08/10 15:58).
#include "vistoep.h"
#include "temporadas_grafico.h"
#include "extras.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
static int falhas;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %d: %s\n", __LINE__, #c); falhas++; } } while (0)
static int vistosTemp(const char *id, int t) {
  TgEp eps[64]; TgDados d; int k = 0, e, tt, c;
  for (tt = 1; tt <= 3; tt++) for (e = 1; e <= 10; e++) {
    eps[k].temporada = (short)tt; eps[k].episodio = (short)e;
    eps[k].visto = (signed char)vistoep_estado(id, tt, e); k++;
  }
  memset(&d, 0, sizeof d);
  tgraf_montar(&d, eps, k, vistoep_conhecido(id), 0, 0, 0, NULL);
  c = tgraf_coluna(&d, t);
  return c < 0 ? -1 : d.t[c].vistos;
}
int main(int argc, char **argv) {
  (void)argc; (void)argv;
  const char *id = "tt14688458"; int t, e, ja;
  VistoPar lote[32], envio[32];
  // Estado do Trakt: S1 10, S2 9 (falta E8), S3 8 = 27 vistos.
  for (t = 1; t <= 3; t++) for (e = 1; e <= 10; e++)
    vistoep_definir(id, t, e, !((t == 2 && e == 8) || (t == 3 && e > 8)));
  CHECK(vistoep_contar(id) == 27);
  int base = vistoep_contar(id), trakt = 27;   // o que extras.c guarda no fetch
  lote[0].temporada = 3; lote[0].episodio = 1;
  vistoep_aplicar(id, lote, 1, 0, envio, &ja);                // desmarcar S3E1
  for (e = 1; e <= 10; e++) { lote[e-1].temporada = 1; lote[e-1].episodio = e; }
  for (e = 1; e <= 6; e++) { lote[10+e-1].temporada = 2; lote[10+e-1].episodio = e; }
  vistoep_aplicar(id, lote, 16, 0, envio, &ja);               // ate aqui desmarcar
  vistoep_aplicar(id, lote, 16, 1, envio, &ja);               // ate aqui marcar
  lote[0].temporada = 2; lote[0].episodio = 7;
  vistoep_aplicar(id, lote, 1, 0, envio, &ja);                // desmarcar S2E7
  CHECK(vistosTemp(id, 1) == 10);
  CHECK(vistosTemp(id, 2) == 8);
  CHECK(vistosTemp(id, 3) == 7);
  CHECK(vistoep_contar(id) == 25);
  // O contador do hero (extras_progresso_serie) tem de seguir o mapa.
  CHECK(vistoep_ajustar_vistos(trakt, base, vistoep_contar(id), 30) == 25);
  CHECK(vistoep_ajustar_vistos(trakt, base, 40, 30) == 30);
  CHECK(vistoep_ajustar_vistos(2, 27, 0, 30) == 0);
  // BOTAO PRINCIPAL / PROXIMO: depois do fetch, o Trakt diz que o proximo e S2E8
  // (S2E1-7 vistos). O dono desmarca S2E7 sem reabrir a pagina.
  { int pt = 0, pe = 0;
    for (t = 1; t <= 3; t++) for (e = 1; e <= 10; e++)
      vistoep_definir(id, t, e, t == 1 || (t == 2 && e <= 7));
    extras_teste_progresso(id, 17, 30, 2, 8);
    CHECK(extras_ep_visto(2, 7) == 1);
    CHECK(extras_proximo_episodio(&pt, &pe) && pt == 2 && pe == 8);
    lote[0].temporada = 2; lote[0].episodio = 7;
    vistoep_aplicar(id, lote, 1, 0, envio, &ja);
    CHECK(extras_ep_visto(2, 7) == 0);
    CHECK(extras_proximo_episodio(&pt, &pe) && pt == 2 && pe == 7);
  }
  // CHAVES: o id de contexto "tt:S:E" e o id puro sao a MESMA serie; nunca
  // entradas duplicadas (a suspeita de 60 entradas era vistoep_n global).
  { const char *outra = "tt2707408:1:1";
    vistoep_esquecer();
    for (t = 1; t <= 3; t++) for (e = 1; e <= 10; e++) {
      vistoep_definir(id, t, e, 1);
      vistoep_definir("tt14688458:2:7", t, e, 0);
      vistoep_definir(outra, 1, e, 1);
    }
    CHECK(vistoep_total(id) == 30);
    CHECK(vistoep_total("tt14688458:3:1") == 30);
    CHECK(vistoep_n() == 40);
    CHECK(vistoep_contar(id) == 0);
  }
  printf(falhas ? "FALHAS: %d\n" : "OK\n", falhas);
  return falhas != 0;
}
