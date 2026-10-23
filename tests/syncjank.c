// Offline performance regression: real settings/sync code, account HTTP stubbed.
#define SYNCJANK_TEST
#include "ajustes_sync.c"
#include <time.h>

static double milliseconds(void) {
  struct timespec t; assert(clock_gettime(CLOCK_MONOTONIC, &t) == 0);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1000000.0;
}

int main(int argc, char **argv) {
  assert(argc == 2);
  setenv("NUVIO_DADOS", argv[1], 1);
  dados_iniciar(NULL); player_dir(argv[1]); ajustes_dir(argv[1]); fil_definir_perfil(1);
  login();
  // Large allowlisted device file; comment content is ignored by its reader.
  size_t n = 100000;
  char *text = malloc(n + 1); assert(text);
  memset(text, '#', n); text[n - 1] = '\n'; text[n] = 0;
  assert(dados_gravar("posteres.txt", text));
  char shared[10001]; memset(shared, '#', sizeof shared - 1);
  shared[sizeof shared - 2] = '\n'; shared[sizeof shared - 1] = 0;
  assert(dados_gravar("fonteregra.txt", shared));
  double t = milliseconds();
  char *snapshot = ajustesDeviceSnapshot(); assert(snapshot);
  printf("device snapshot: %.3f ms\n", milliseconds() - t); free(snapshot);
  t = milliseconds();
  ajustes_mesclar_blob("{}", &cloud); assert(cloud);
  printf("build/serialize/merge: %.3f ms, %zu bytes\n", milliseconds() - t, strlen(cloud));
  // Match the owner's blob size, preserving an unknown web field verbatim.
  const size_t target = 237546;
  size_t len = strlen(cloud); assert(len + 13 < target);
  char *padded = malloc(target + 1); assert(padded);
  memcpy(padded, cloud, len - 1);
  memcpy(padded + len - 1, ",\"future\":\"", 11);
  memset(padded + len + 10, 'x', target - len - 12);
  memcpy(padded + target - 2, "\"}", 3);
  free(cloud); cloud = padded; assert(strlen(cloud) == target);

  const char *v, *f;
  assert(acharValor(cloud, cloud + target, "file4", &v, &f));
  t = milliseconds(); char *decoded = ajusteUnhex(v, f); assert(decoded);
  printf("hex decode: %.3f ms\n", milliseconds() - t);
  assert(!strcmp(decoded, text)); free(decoded);

  // Fresh login restores the file, then an unchanged cycle must not upload.
  dados_apagar("posteres.txt"); dados_apagar("fonteregra.txt");
  t = milliseconds(); sync_iniciar(); double start = milliseconds() - t;
  for (int i = 0; i < 5000 && sync_estado() == SYNC_RODANDO; i++) usleep(1000);
  assert(sync_estado() == SYNC_PRONTO);
  t = milliseconds(); sync_passo(SDL_GetTicks()); double apply = milliseconds() - t;
  char *restored = dados_ler("posteres.txt"); assert(restored && !strcmp(restored, text)); free(restored);
  restored = dados_ler("fonteregra.txt"); assert(restored && !strcmp(restored, shared)); free(restored);
  t = milliseconds();
  check(ajustes_aplicar_blob(cloud) == 0, "identical restore changes no keys or files");
  double identical = milliseconds() - t;
  printf("identical restore: %.3f ms\n", identical);
  t = milliseconds(); sync_iniciar(); double repeat = milliseconds() - t;
  for (int i = 0; i < 5000 && sync_estado() == SYNC_RODANDO; i++) usleep(1000);
  assert(sync_estado() == SYNC_PRONTO);
  t = milliseconds(); sync_passo(SDL_GetTicks()); double unchanged = milliseconds() - t;
  printf("main thread: start %.3f, restore %.3f, repeat start %.3f, unchanged apply %.3f ms (limit 50 ms)\n",
         start, apply, repeat, unchanged);
  check(pushes == 0, "unchanged settings do not upload");
  // A file-only edit must upload once; protecting identical data must not push.
  text[0] = '!'; assert(dados_gravar("posteres.txt", text));
  t = milliseconds(); sync_iniciar(); double edit = milliseconds() - t;
  for (int i = 0; i < 5000 && sync_estado() == SYNC_RODANDO; i++) usleep(1000);
  assert(sync_estado() == SYNC_PRONTO); sync_passo(SDL_GetTicks());
  check(pushes == 1 && strstr(cloud, "212323"), "changed file uploaded with exact bytes");
  check(strstr(cloud, "\"future\":\"xxx") != NULL, "unknown web field preserved");
  sync_proteger_ajustes_locais(); cycle();
  check(pushes == 1, "identical upload skipped");
  const char badHex[] = "\"0g\"", nulHex[] = "\"00\"", oddHex[] = "\"1\"";
  assert(!ajusteUnhex(badHex, badHex + sizeof badHex - 1));
  assert(!ajusteUnhex(nulHex, nulHex + sizeof nulHex - 1));
  assert(!ajusteUnhex(oddHex, oddHex + sizeof oddHex - 1));
  printf("main thread file edit detection: %.3f ms\n", edit);
  free(text); free(cloud);
  if (identical > 50 || edit > 50 || start > 50 || apply > 50 || repeat > 50 || unchanged > 50) {
    fprintf(stderr, "FAIL: settings sync blocked main thread over 50 ms\n"); return 1;
  }
  puts("PASS: syncjank"); return 0;
}
