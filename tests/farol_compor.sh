#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-compor.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$tmp"' EXIT

flags=(-Wall -Wextra -Werror -O1 -g -std=gnu11 -Isrc -pthread)
[ "$(uname -s)" = Darwin ] || flags+=(-ldl)
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);;
  thread) flags+=(-fsanitize=thread -fno-omit-frame-pointer);;
esac

mkdir -p "$tmp/dados"
python3 tests/farol_compor_server.py "$tmp/port" "$tmp/capture.jsonl" &
srv=$!
for i in $(seq 50); do [ -s "$tmp/port" ] && break; sleep 0.1; done
test -s "$tmp/port"
base="http://127.0.0.1:$(cat "$tmp/port")"

"${CC:-cc}" "${flags[@]}" \
  tests/farol_compor.c \
  src/farolcompor.c \
  src/farolgosto.c \
  src/farolrec.c \
  src/farolpeca.c \
  src/faroltexto.c \
  src/js.c \
  src/rede.c \
  src/redeurl.c \
  -o "$tmp/cliente"

"$tmp/cliente" "$base" "$tmp/dados" >"$tmp/log" 2>&1 || { cat "$tmp/log"; exit 1; }
grep '^ok  ' "$tmp/log" || true

mkdir -p "$tmp/dados_sessao"
nuvem_flags=(-O1 -g -std=gnu11 -Isrc -pthread -Wall -Wextra)
case "${SANITIZE:-0}" in
  1) nuvem_flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);;
  thread) nuvem_flags+=(-fsanitize=thread -fno-omit-frame-pointer);;
esac
"${CC:-cc}" "${nuvem_flags[@]}" \
  "-DNV_SUPABASE_URL=\"$base\"" "-DNV_SUPABASE_ANON_KEY=\"anon-teste\"" \
  -c src/nuvem.c -o "$tmp/nuvem.o"
"${CC:-cc}" "${flags[@]}" \
  tests/farol_compor_sessao_refresh.c \
  tests/sessao_boundary_stub.c \
  src/farolcompor.c \
  src/farolgosto.c \
  src/farolrec.c \
  src/farolpeca.c \
  src/faroltexto.c \
  src/sessao.c \
  "$tmp/nuvem.o" \
  src/js.c \
  src/jsw.c \
  src/rede.c \
  src/redeurl.c \
  -o "$tmp/cliente_sessao"

"$tmp/cliente_sessao" "$base" "$tmp/dados_sessao" >"$tmp/log_sessao" 2>&1 || { cat "$tmp/log_sessao"; exit 1; }
grep '^ok  ' "$tmp/log_sessao" || true
python3 - "$tmp/log_sessao" <<'PY'
import sys
log_text = open(sys.argv[1], encoding="utf-8").read()
for secret in ("t0001", "t0002", "anon-teste"):
    assert secret not in log_text, f"segredo vazou no log_sessao: {secret}"
PY

# Auditoria pos-execucao sobre corpos HTTP capturados e logs (zero canario/token/URL):
python3 - "$tmp/log" "$base" "$tmp/capture.jsonl" <<'PY'
import json
import sys

log_text = open(sys.argv[1], encoding="utf-8").read()
base_url = sys.argv[2]
capture_path = sys.argv[3]

for secret in (
    base_url,
    "tok_segredo_compor_99",
    "CANARIO_DIARIO_SEGREDO_991",
    "Perfil_Privado_Canario",
    "Amigo_Canario_Joao",
    "addon-privado.example",
):
    assert secret not in log_text, f"segredo vazou no log: {secret}"

rows = [json.loads(line) for line in open(capture_path, encoding="utf-8") if line.strip()]
assert len(rows) >= 8, f"esperava >= 8 requisicoes capturadas, vieram {len(rows)}"
for row in rows:
    assert "error" not in row, f"servidor recusou esquema/privacidade: {row}"
    raw_str = json.dumps(row["body"], ensure_ascii=False)
    for forbidden in (
        "CANARIO_DIARIO_SEGREDO_991",
        "2026-04-12",
        "Perfil_Privado_Canario",
        "Amigo_Canario_Joao",
        "999888",
        "addon-privado.example",
        "jwt_tok_canario",
        "tok_segredo_compor_99",
        "HASH_NEGATIVO",
        "-98765",
        "-424242",
        "horaResto",
    ):
        assert forbidden not in raw_str, f"canario/campo proibido no HTTP body: {forbidden}"
print(f"farol_compor: {len(rows)} requisicoes auditadas, tudo ok")
PY
