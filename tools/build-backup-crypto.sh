#!/bin/sh
set -eu
# Runs in the target compiler environment (webOS SDK, TPK SDK or emsdk).
# Separate output directories are required for different targets/ABIs.
source_dir=${1:?usage: build-backup-crypto.sh SOURCE_DIR OUTPUT_DIR}
output=${2:?usage: build-backup-crypto.sh SOURCE_DIR OUTPUT_DIR}
config=$(CDPATH= cd -- "$(dirname "$0")" && pwd)/backup-mbedtls-config.h
compiler=${CC:-cc}
archiver=${AR:-ar}
mkdir -p "$output/include/psa" "$output/include/mbedtls" "$output/lib" "$output/obj"
fingerprint=$({ cat "$config" "$0" "$source_dir/include/mbedtls/version.h"; "$compiler" --version; printf '%s' "${CFLAGS:-}"; } | cksum)
if [ -f "$output/.stamp" ] && [ "$(cat "$output/.stamp")" = "$fingerprint" ] && [ -f "$output/lib/libnuvio-backup-crypto.a" ]; then exit 0; fi
cp "$source_dir"/include/mbedtls/*.h "$output/include/mbedtls/"
cp "$source_dir"/include/psa/*.h "$output/include/psa/"
# Headers must expose the exact same configuration to backupcrypto.c.
cp "$config" "$output/include/mbedtls/mbedtls_config.h"
for name in aes cipher cipher_wrap gcm md pkcs5 sha256 platform_util constant_time; do
  "$compiler" ${CFLAGS:--O2 -fPIC} -I"$output/include" -I"$source_dir/library" \
    -c "$source_dir/library/$name.c" -o "$output/obj/$name.o"
done
rm -f "$output/lib/libnuvio-backup-crypto.a"
"$archiver" rcs "$output/lib/libnuvio-backup-crypto.a" "$output"/obj/*.o
printf '%s\n' "$fingerprint" > "$output/.stamp"
