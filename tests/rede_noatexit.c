// #F113 (ressalva R2 do F110): o OPENSSL_init_crypto(NO_ATEXIT) tem de ir
// para a libcrypto que a libcurl carregada usa, e nao para a primeira da
// lista de sonames residente. Ver tests/rede_noatexit.sh, que monta uma
// libcurl.so.4 falsa dependente de uma libcrypto com soname fora da lista
// e uma libcrypto.so.3 isca.
//
//   unica — sem isca: so a libcrypto fora da lista existe; a marcacao so
//           acontece se o dlsym for no handle da propria libcurl;
//   dupla — isca residente ANTES: simula duas libcrypto carregadas; a
//           marcacao tem de cair na da curl, nao na isca.
#include "rede.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  if (argc > 1 && !strcmp(argv[1], "dupla")) {
    // Isca residente antes da libcurl abrir: soname DA LISTA, mas nao e a
    // libcrypto que a nossa libcurl usa.
    if (!dlopen("libcrypto.so.3", RTLD_NOW | RTLD_GLOBAL)) {
      fprintf(stderr, "isca: %s\n", dlerror());
      return 2;
    }
  }
  rede_preparar();
  return 0;
}
