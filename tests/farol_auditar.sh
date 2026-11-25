#!/bin/bash
# Prova tools/farol-auditar.mjs em fixtures limpas e sujas (PLANO §2.5):
# - aceita JSON unico, array JSON, NDJSON e envelope de export D1;
# - rejeita (rc != 0) todos os 9 padroes proibidos (://, @, IPv4, IPv6,
#   20+ hex fora de h:/fingerprint, %2F, Bearer, JWT, CANARIO);
# - imprime APENAS contagens (nunca vaza trechos nem valores na saida);
# - nao contem chamadas a wrangler nem rede.
set -eu
cd "$(dirname "$0")/.."
command -v node >/dev/null 2>&1 || { echo "farol_auditar: sem node" >&2; exit 1; }

# Garante que o auditor e estritamente offline (sem wrangler nem cliente HTTP)
if grep -nE 'wrangler|d1 execute|fetch\(|node:http|node:https|child_process' tools/farol-auditar.mjs; then
  echo "farol_auditar: FALHA (auditor nao pode chamar wrangler/rede/exec)" >&2
  exit 1
fi

dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-auditar.XXXXXX")"
trap 'rm -rf "$dir"' EXIT

# --- 1. Fixture dourada do repositorio (JSON unico limpo) ---
out="$(node tools/farol-auditar.mjs tests/farol_registro_v1.json)"
printf '%s\n' "$out" | grep -qx 'registros: 1'
printf '%s\n' "$out" | grep -qx 'total: 0'

# --- 2. NDJSON limpo com h:<hex>, erro.fp (16 hex), fingerprint e versoes ---
python3 - "$dir" <<'PY'
import json, pathlib, sys
d = pathlib.Path(sys.argv[1])
base = json.load(open("tests/farol_registro_v1.json", encoding="utf-8"))

r2 = json.loads(json.dumps(base))
r2["linha"].append({"t": -500, "m": "addon", "e": "addon_lento", "a": 12000, "b": 0, "k": "h:9c1e77ab"})
r2["erro"]["fingerprint"] = "webos-4-starfish-ums_204-mkv-hevc10-dv-eac3"
(d / "limpo.ndjson").write_text(json.dumps(base) + "\n" + json.dumps(r2) + "\n", encoding="utf-8")

# Envelope tipico de export JSON do D1 (pessoa_h server-side de 64 hex fica fora do FarolRegistro)
d1_clean = [{
    "results": [
        {"id": 536870912, "pessoa_h": "a" * 64, "json": json.dumps(base)},
        {"id": 536870913, "pessoa_h": "b" * 64, "json": json.dumps(r2)},
    ],
    "success": True,
}]
(d / "d1_limpo.json").write_text(json.dumps(d1_clean), encoding="utf-8")

# Fixtures sujas: uma por padrao proibido, com marcadores secretos que NAO podem aparecer na saida
sujos = {
    "url_esquema": ("https://segredo-nao-imprimir.invalid/stream", "segredo-nao-imprimir"),
    "arroba":      ("fulano-secreto@exemplo.invalid", "fulano-secreto"),
    "ipv4":        ("198.51.100.42", "198.51.100.42"),
    "ipv6":        ("2001:db8::cafe:42", "cafe:42"),
    "hex_longo":   ("0123456789abcdef0123456789abcdef", "0123456789abcdef0123"),
    "pct_2f":      ("caminho-oculto%2Fprivado", "caminho-oculto"),
    "bearer":      ("Bearer token-secreto-xyz", "token-secreto-xyz"),
    "jwt":         ("eyJhbGciOiJIUzI1NiJ9.eyJ1c2VyIjoic2VncmVkbyJ9.assinaturaSecreto", "eyJhbGciOiJIUzI1NiJ9"),
    "canario":     ("CANARIO_SECRETO_77", "CANARIO_SECRETO_77"),
}
for nome, (val, _seg) in sujos.items():
    sujo = json.loads(json.dumps(base))
    sujo["linha"][0]["k"] = val
    (d / f"sujo_{nome}.json").write_text(json.dumps(sujo), encoding="utf-8")

# D1 sujo com varios vazamentos numa linha NDJSON
d1_sujo = [{
    "results": [
        {"id": 536870914, "pessoa_h": "c" * 64, "json": json.dumps({
            "v": 1,
            "erro": {"nativo": "https://vazou@192.0.2.9/a%2Fb"},
            "linha": [{"k": "Bearer CANARIO_D1_99"}],
        })},
    ],
    "success": True,
}]
(d / "d1_sujo.json").write_text(json.dumps(d1_sujo), encoding="utf-8")
PY

# Conferir limpos
out="$(node tools/farol-auditar.mjs "$dir/limpo.ndjson")"
printf '%s\n' "$out" | grep -qx 'registros: 2'
printf '%s\n' "$out" | grep -qx 'total: 0'

out="$(node tools/farol-auditar.mjs "$dir/d1_limpo.json")"
printf '%s\n' "$out" | grep -qx 'registros: 2'
printf '%s\n' "$out" | grep -qx 'total: 0'

# Conferir cada fixture suja: RC != 0, contagem do padrao >= 1, total >= 1, e ZERO vazamento de trecho
check_sujo() {
  local padrao="$1"
  local segredo="$2"
  local arq="$dir/sujo_${padrao}.json"
  local rc=0 out=""
  out="$(node tools/farol-auditar.mjs "$arq" 2>&1)" || rc=$?
  if [ "$rc" -eq 0 ]; then
    echo "farol_auditar: FALHA ($padrao retornou rc=0)" >&2
    exit 1
  fi
  if ! printf '%s\n' "$out" | grep -qE "^${padrao}: [1-9][0-9]*$"; then
    echo "farol_auditar: FALHA ($padrao nao contou >= 1): $out" >&2
    exit 1
  fi
  if ! printf '%s\n' "$out" | grep -qE '^total: [1-9][0-9]*$'; then
    echo "farol_auditar: FALHA ($padrao total nao >= 1): $out" >&2
    exit 1
  fi
  if printf '%s\n' "$out" | grep -qiF "$segredo"; then
    echo "farol_auditar: FALHA ($padrao vazou trecho na saida!)" >&2
    exit 1
  fi
}

check_sujo url_esquema "segredo-nao-imprimir"
check_sujo arroba      "fulano-secreto"
check_sujo ipv4        "198.51.100.42"
check_sujo ipv6        "cafe:42"
check_sujo hex_longo   "0123456789abcdef0123"
check_sujo pct_2f      "caminho-oculto"
check_sujo bearer      "token-secreto-xyz"
check_sujo jwt         "eyJhbGciOiJIUzI1NiJ9"
check_sujo canario     "CANARIO_SECRETO_77"

# Conferir D1 sujo: RC != 0, multiplas ocorrencias, zero trecho vazado
rc=0
out="$(node tools/farol-auditar.mjs "$dir/d1_sujo.json" 2>&1)" || rc=$?
[ "$rc" -ne 0 ] || { echo "farol_auditar: FALHA (d1_sujo retornou 0)" >&2; exit 1; }
printf '%s\n' "$out" | grep -qx 'url_esquema: 1'
printf '%s\n' "$out" | grep -qx 'arroba: 1'
printf '%s\n' "$out" | grep -qx 'ipv4: 1'
printf '%s\n' "$out" | grep -qx 'pct_2f: 1'
printf '%s\n' "$out" | grep -qx 'bearer: 1'
printf '%s\n' "$out" | grep -qx 'canario: 1'
printf '%s\n' "$out" | grep -qx 'total: 6'
if printf '%s\n' "$out" | grep -qE 'vazou|192\.0\.2\.9|CANARIO_D1_99'; then
  echo "farol_auditar: FALHA (d1_sujo vazou trecho na saida!)" >&2
  exit 1
fi

echo "farol_auditar: tudo ok"
