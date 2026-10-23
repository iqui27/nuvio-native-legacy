#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-backup-crypto.XXXXXX")"
trap 'rm -rf "$task_dir"' EXIT
flags=(-std=gnu11 -O1 -g -Isrc -DNV_BACKUP_MBEDTLS -Wall -Wextra -Werror)
libs=(-lmbedcrypto)
if [ -n "${MBEDTLS_INCLUDE:-}" ]; then flags+=("-I$MBEDTLS_INCLUDE"); fi
if [ -n "${MBEDTLS_LIBRARY:-}" ]; then libs=("$MBEDTLS_LIBRARY"); fi
if [[ "$(uname -s)" == Darwin ]]; then
  prefix="${MBEDTLS_PREFIX:-$(brew --prefix mbedtls@3)}";flags+=("-I$prefix/include" "-L$prefix/lib")
fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/backupcrypto.c tests/backupcrypto.c "${libs[@]}" -o "$task_dir/test"
"$task_dir/test" "$task_dir/encrypted.bin"
python3 - "$task_dir/encrypted.bin" <<'PY'
import pathlib,struct,sys
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
data=pathlib.Path(sys.argv[1]).read_bytes()
assert data[:16]==b'NUVIO-AES256GCM1'
iterations=struct.unpack_from('>I',data,16)[0]
assert iterations==600000
context_length,plain_length=struct.unpack_from('>HI',data,48)
header=54+context_length
assert data[54:header]==b'user-1:p7:settings'
key=PBKDF2HMAC(algorithm=hashes.SHA256(),length=32,salt=data[20:36],iterations=iterations).derive('p\u00e4ssw\u00f6rd-Backup-123'.encode())
plain=AESGCM(key).decrypt(data[36:48],data[header:],data[:header])
assert len(plain)==plain_length and plain==b'local-provider-key=fixture-secret\n\0binary\xff\0'
print('PASS: encrypted native envelope independently decrypted with Python cryptography (UTF-8 password)')
PY
