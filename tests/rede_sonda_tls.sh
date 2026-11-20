#!/bin/bash
# #737: o arquivo exporta rede_sonda_tls e seta rede_discord_ca; sem curl real.
set -euo pipefail
cd "$(dirname "$0")/.."
grep -q 'rede_sonda_tls' src/rede.c src/rede.h
grep -q 'rede_sonda_tls' src/sessao.c
grep -q 'r == 35' src/sessao.c
python3 - <<'PY'
from pathlib import Path
s = Path("src/rede.c").read_text()
assert "SONDA TLS (#737)" in s
assert "OPT_CONNECT_ONLY" in s.split("rede_sonda_tls")[1][:2500]
print("PASS: sonda TLS presente e ligada ao login curl 35")
PY
