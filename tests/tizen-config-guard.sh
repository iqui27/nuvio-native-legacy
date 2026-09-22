#!/bin/bash
# Contrato sintético da guarda de configuração Tizen. Não compila nem imprime
# valores: prova somente o bloqueio release e o opt-out diagnóstico explícito.
set -euo pipefail
cd "$(dirname "$0")/.."

TMP=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tizen-config.XXXXXX")
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/incompleto.properties" <<'EOF'
NUVIO_SUPABASE_URL=
NUVIO_SUPABASE_ANON_KEY=
TV_LOGIN_WEB_BASE_URL=
EOF
if NUVIO_PROPERTIES="$TMP/incompleto.properties" tools/env.sh --require-core >"$TMP/out" 2>"$TMP/err"; then
  echo "tizen-config-guard: FAIL release aceitou configuração incompleta" >&2
  exit 1
fi
grep -q 'configuracao obrigatoria ausente' "$TMP/err"
for key in NUVIO_SUPABASE_URL NUVIO_SUPABASE_ANON_KEY TV_LOGIN_WEB_BASE_URL; do
  grep -q "$key" "$TMP/err"
done
if grep -Eq 'https?://|eyJ|[A-Za-z0-9_-]{24,}' "$TMP/err"; then
  echo "tizen-config-guard: FAIL erro expôs valor de configuração" >&2
  exit 1
fi

cat > "$TMP/completo.properties" <<'EOF'
NUVIO_SUPABASE_URL=https://config.example.invalid
NUVIO_SUPABASE_ANON_KEY=test-anon-key
TV_LOGIN_WEB_BASE_URL=https://login.example.invalid
EOF
NUVIO_PROPERTIES="$TMP/completo.properties" tools/env.sh --require-core >"$TMP/out" 2>"$TMP/err"

# Harness diagnóstico: o modo permissivo só passa com a opção explícita.
NUVIO_PROPERTIES="$TMP/incompleto.properties" tools/env.sh --allow-unconfigured >"$TMP/out" 2>"$TMP/err"

echo "tizen-config-guard: PASS release bloqueia config ausente; diagnostico exige opt-out explicito"
