#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #F113 (ressalva R2 do F110, src/rede.c segurarSaidaRede): o
# OPENSSL_init_crypto(NO_ATEXIT) tem de ir para a libcrypto que a libcurl
# carregada USA — dlsym no handle do dlopen dela — e nao para a primeira da
# lista de sonames que estiver residente. Monta, so para o teste:
#   libcrypto-f113fake.so — soname fora da lista (o "libcrypto.3.dylib do
#     Homebrew" da ressalva), unica dependencia crypto da libcurl falsa;
#   libcrypto.so.3 — isca com soname DA LISTA, para o cenario de duas
#     libcrypto residentes;
#   libcurl.so.4 falsa — depende so da f113fake e expoe os simbolos que
#     abrirReal procura; o global_init chama OPENSSL_init_ssl como a real.
# Cada libcrypto falsa grava num arquivo ("marca") quem chamou o seu
# OPENSSL_init_crypto e com quais opcoes; o teste confere a marca.
#   Em 5f605710c: modo "unica" nao marca nada (a lista nao acha o soname) e
#   modo "dupla" marca a ISCA — a libcrypto errada. Na ponta: marca FAKE nos
#   dois.  bash tests/rede_noatexit.sh
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "$TMPDIR/nuvio-rede-noatexit.XXXXXX")
trap 'rm -rf "$DIR"' EXIT

# libcrypto falsa: registra MARCA + opcoes de cada OPENSSL_init_crypto.
# A isca nao exporta OPENSSL_init_ssl, para o init da curl falsa amarrar na
# f113fake sempre.
cat > "$DIR/libfake.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
int OPENSSL_init_crypto(unsigned long long op, const void *s) {
  const char *m = getenv("NVT_MARCA");
  FILE *f;
  (void)s;
  if (m && (f = fopen(m, "a")) != NULL) {
    fprintf(f, "%s 0x%llx\n", MARCA, op);
    fclose(f);
  }
  return 1;
}
#ifndef SEM_SSL
int OPENSSL_init_ssl(unsigned long long op, const void *s) {
  (void)op; (void)s;
  return 1;
}
#endif
EOF
cc -shared -fPIC -O1 '-DMARCA="FAKE"' -Wl,-soname,libcrypto-f113fake.so \
  "$DIR/libfake.c" -o "$DIR/libcrypto-f113fake.so"
cc -shared -fPIC -O1 '-DMARCA="ISCA"' -DSEM_SSL -Wl,-soname,libcrypto.so.3 \
  "$DIR/libfake.c" -o "$DIR/libcrypto.so.3"

# libcurl falsa: NEEDED so na libcrypto-f113fake (o --no-as-needed garante a
# entrada mesmo com o linker de Ubuntu) e simbolos no-op para o abrirReal.
cat > "$DIR/curlfake.c" <<'EOF'
extern int OPENSSL_init_ssl(unsigned long long, const void *);
int   curl_global_init(long f) { (void)f; return OPENSSL_init_ssl(0, (const void *)0); }
void *curl_version_info(int i) { (void)i; return (void *)0; }
void *curl_easy_init(void) { return (void *)0; }
int   curl_easy_setopt(void) { return 0; }
int   curl_easy_perform(void) { return 0; }
int   curl_easy_getinfo(void) { return 0; }
int   curl_easy_send(void) { return 0; }
int   curl_easy_recv(void) { return 0; }
void  curl_easy_cleanup(void) {}
void  curl_easy_reset(void) {}
void *curl_slist_append(void) { return (void *)0; }
void  curl_slist_free_all(void) {}
long  curl_getdate(void) { return -1; }
void *curl_multi_init(void) { return (void *)0; }
int   curl_multi_add_handle(void) { return 0; }
int   curl_multi_remove_handle(void) { return 0; }
int   curl_multi_perform(void) { return 0; }
int   curl_multi_poll(void) { return 0; }
int   curl_multi_wait(void) { return 0; }
int   curl_multi_cleanup(void) { return 0; }
int   curl_multi_setopt(void) { return 0; }
int   curl_multi_info_read(void) { return 0; }
EOF
cc -shared -fPIC -O1 -Wl,-soname,libcurl.so.4 "$DIR/curlfake.c" \
  -L"$DIR" -Wl,--no-as-needed -lcrypto-f113fake -o "$DIR/libcurl.so.4"

# Binario de teste: o rede.c de producao inteiro, so com o ponto de entrada
# rede_preparar() — o mesmo que os fios do app usam.
cc -Isrc -O1 -g tests/rede_noatexit.c src/rede.c src/redeurl.c -lpthread \
  -o "$DIR/t"

falhas=0
confere() {  # confere <modo> <descricao>
  modo=$1; shift
  : > "$DIR/marca"
  NVT_MARCA="$DIR/marca" LD_LIBRARY_PATH="$DIR" "$DIR/t" "$modo" > "$DIR/saida-$modo.log" 2>&1
  marcas=$(cat "$DIR/marca")
  echo "-- $modo: marca=[${marcas:-vazia}]"
  if [ "$marcas" = "FAKE 0x80000" ]; then
    echo "ok   $modo — OPENSSL_init_crypto(0x80000) na libcrypto DA CURL"
  else
    echo "FALHOU $modo — esperado so 'FAKE 0x80000' (a libcrypto da libcurl)"
    falhas=$((falhas+1))
  fi
}
confere unica
confere dupla
[ "$falhas" = 0 ] && echo "PASS" || { echo "$falhas falha(s)"; exit 1; }
