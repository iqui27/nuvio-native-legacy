// Exercise the production room builder; reuse existing offline player/network edges.
#define main fluxo_main
#include "juntos_fluxo.c"
#undef main

static void emit(FILE *out, const char *id, int t, int e, int stress) {
  zerar();
  fioVivo = 1; // Hold the worker: inspect exactly what would be posted.
  const char *friends[JUNTOS_CONVIDADOS_MAX];
  char friendId[96], title[160], poster[512];
  memset(friendId, 1, sizeof friendId - 1); friendId[95] = 0;
  memset(title, 2, sizeof title - 1); title[159] = 0;
  memset(poster, 3, sizeof poster - 1); poster[511] = 0;
  for (int i = 0; i < JUNTOS_CONVIDADOS_MAX; i++) friends[i] = friendId;
  prepared = 1;
  memset(&testStream, 0, sizeof testStream);
  if (stress) {
    memset(testStream.arquivo, 4, 120); testStream.arquivo[120] = 0;
    memset(testStream.bingeGroup, 5, 80); testStream.bingeGroup[80] = 0;
    position = NAN;
  } else position = 12.5;
  juntos_preparar(id, t, e, stress ? title : "Mr. Robot", stress ? poster : "",
                  friends, stress ? JUNTOS_CONVIDADOS_MAX : 0, 0);
  assert(fase == JF_PREPARADO);
  criarSala();
  assert(nFila == 1 && fila[0].corpo);
  fprintf(out, "%s\n", fila[0].corpo);
  free(fila[0].corpo); nFila = 0;
}

int main(int argc, char **argv) {
  assert(argc == 2);
  FILE *out = fopen(argv[1], "w"); assert(out);
  emit(out, "tt4158110:1:2", 1, 2, 0);
  emit(out, "tt4158110", 1, 2, 0);
  emit(out, "tt4158110:1:2", 0, 0, 0);
  emit(out, "tt4158110:1:2", 2, 3, 0);
  assert(!strcmp(abrirImdb, "tt4158110") && abrirT == 2 && abrirE == 3);
  assert(!strcmp(motor.sala.ep, "tt4158110:2:3"));
  emit(out, "tt4158110", 0, 0, 1);
  fclose(out);
  const char *invalid[] = {"tmdb:123", "kitsu:123", "tt1:1:2:1:2", "tt", "tt12345678901", "tt1:12345:2", "tt1:1:123456", "tt1:1:x", "tt1\""};
  for (size_t i = 0; i < sizeof invalid / sizeof *invalid; i++) {
    zerar();
    juntos_preparar(invalid[i], 1, 2, "Film", "", NULL, 0, 0);
    assert(fase == JF_OCIOSO && !nFila && !strcmp(lastRoomError, "ep invalido"));
  }
  Resp bad = {.tipo = OP_CRIAR, .status = 400, .txt = "{\"erro\":\"ep invalido\"}"};
  resposta(&bad); assert(!strcmp(lastRoomError, "ep invalido"));
  bad.txt = "{\"erro\":\"json invalido\"}";
  resposta(&bad); assert(!strcmp(lastRoomError, "json invalido"));
  bad.txt = "{\"erro\":\"https://private.invalid/token-secret\"}";
  resposta(&bad); assert(!strcmp(lastRoomError, "rede"));
  return 0;
}
