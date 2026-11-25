#!/bin/bash
# Auditoria ponta a ponta do gosto e privacidade (farol-56):
#   1. Varredura estatica de logs e _Thread_local em src/farol*.c e servidor/recomendacoes/src/farol*.js
#   2. Canarios CANARIO_* nos clientes C (compor, busca, escolha_rede, FarolRegistro) em servidor HTTP local
#   3. Manipulador Node real (rotaFarolCompor / rotaFarolEscolha) em servidor/recomendacoes/src/farolgosto.js
#   4. Medicao real de RAM do gosto <= 96 KB (estatica + heap)
#   5. ASan+UBSan (GCC e Clang) e TSan (Clang) nos modulos com fios
set -eu
cd "$(dirname "$0")/.."

base_tmp="${TMPDIR:-/tmp}"
base_tmp="${base_tmp%/}"
work=$(mktemp -d "$base_tmp/nuvio-farol-auditoria.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$work"' EXIT

# 1. Varredura estatica de logs / _Thread_local.
python3 - <<'PY'
import glob
import re
import sys

bad = []
c_files = sorted(glob.glob("src/farol*.c"))
js_files = sorted(glob.glob("servidor/recomendacoes/src/farol*.js"))

# Sem _Thread_local em src/farol*.c/.h
for path in sorted(glob.glob("src/farol*.[ch]")):
    with open(path, encoding="utf-8", errors="replace") as f:
        for i, line in enumerate(f, 1):
            s = line.strip()
            if s.startswith("//") or s.startswith("*"):
                continue
            if re.search(r'\b(_Thread_local|__thread)\b', s):
                bad.append(f"{path}:{i}: _Thread_local proibido")

SENSIVEIS = re.compile(
    r'\b(titulo|title|perfil|profile|gosto|taste|frase|phrase|token|auth|bearer|url|uri|cab|senha|secret)\b',
    re.I,
)

for path in c_files:
    with open(path, encoding="utf-8", errors="replace") as f:
        lines = f.readlines()
    for i, line in enumerate(lines, 1):
        s = line.strip()
        if s.startswith("//") or s.startswith("/*") or s.startswith("*"):
            continue
        # Ignora fprintf em FILE* de persistencia; audita logs de saida/console/SDK.
        m = re.search(r'\b(printf|fprintf\s*\(\s*(?:stderr|stdout)\b|SDL_Log|nv_log|syslog|NSLog|LOGD|__android_log\w*)\s*\(', s)
        if not m:
            continue
        stmt = s
        j = i
        while ";" not in stmt and j < len(lines):
            stmt += " " + lines[j].strip()
            j += 1
        if "%" in stmt and SENSIVEIS.search(stmt):
            bad.append(f"{path}:{i}: log com possivel dado sensivel: {s}")
        elif "%s" in stmt:
            bad.append(f"{path}:{i}: log com %%s proibido em Farol: {s}")

for path in js_files:
    with open(path, encoding="utf-8", errors="replace") as f:
        lines = f.readlines()
    for i, line in enumerate(lines, 1):
        s = line.strip()
        if s.startswith("//") or s.startswith("/*") or s.startswith("*"):
            continue
        if re.search(r'\bconsole\.(log|warn|error|info|debug)\s*\(', s):
            stmt = s
            j = i
            while ")" not in stmt and j < len(lines):
                stmt += " " + lines[j].strip()
                j += 1
            if SENSIVEIS.search(stmt) or "${" in stmt or "+" in stmt and "e.message" not in stmt:
                bad.append(f"{path}:{i}: console.* interpolando dado proibido: {s}")

if bad:
    for b in bad:
        print(b, file=sys.stderr)
    sys.exit(1)
print("auditoria estatica: 0 logs proibidos / 0 _Thread_local")
PY

# 2. Servidor HTTP local de captura para canarios reais.
cat > "$work/srv.py" <<'PY'
import http.server
import json
import sys

PORT_FILE, CAP_FILE = sys.argv[1], sys.argv[2]

class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass
    def do_POST(self):
        n = int(self.headers.get("Content-Length", "0") or "0")
        body = self.rfile.read(n).decode("utf-8", errors="replace")
        rec = {"path": self.path, "headers": dict(self.headers.items()), "body": body}
        with open(CAP_FILE, "a", encoding="utf-8") as f:
            f.write(json.dumps(rec, ensure_ascii=False) + "\n")
        if self.path.startswith("/v1/farol/compor"):
            out = b'{"origem":"regras"}'
        elif self.path.startswith("/v1/farol/escolha"):
            out = b'{"origem":"llm","respostas":{"duracao":"curto"},"falta":"humor"}'
        elif self.path.startswith("/v1/farol/busca"):
            out = b'{"ok":1,"ids":[{"tmdb":1001,"imdb":"tt1001","tipo":"movie"}],"origem":"llm","llm_status":200}'
        else:
            out = b'{"ok":1}'
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(out)))
        self.end_headers()
        self.wfile.write(out)

srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), H)
with open(PORT_FILE, "w", encoding="utf-8") as f:
    f.write(str(srv.server_address[1]))
srv.serve_forever()
PY

python3 "$work/srv.py" "$work/port" "$work/capture.jsonl" &
srv=$!
for _ in $(seq 50); do [ -s "$work/port" ] && break; sleep 0.1; done
test -s "$work/port"
base="http://127.0.0.1:$(cat "$work/port")"

SRCS=(
  tests/farol_gosto_auditoria.c
  src/farolcompor.c
  src/farolbusca.c
  src/farolbuscaparse.c
  src/farolacha.c
  src/farolescolha_rede.c
  src/farolescolha.c
  src/farolconsent.c
  src/farolpeca.c
  src/faroltexto.c
  src/farolrec.c
  src/farolgosto.c
  src/farolinfo.c
  src/farolanel.c
  src/farolpacote.c
  src/farolperfil.c
  src/farolredator.c
  src/farolerro.c
  src/fontesonda.c
  src/farolsinal.c
  src/farolficha.c
  src/mapa.c
  src/js.c
  src/rede.c
  src/redeurl.c
)
CFLAGS_COMMON=(-Wall -Wextra -O1 -g -std=gnu11 -Isrc -DNV_MAPA_PURO -pthread)
LIBS_COMMON=(-lm -ldl)

# 3. Medicao real de RAM do gosto (build limpo sem sombra de sanitizador).
mkdir -p "$work/dados_ram"
cc -DAUDITORIA_RAM "${CFLAGS_COMMON[@]}" "${SRCS[@]}" -o "$work/ram_bin" "${LIBS_COMMON[@]}"
ram_out=$("$work/ram_bin" --ram "$work/dados_ram")
echo "$ram_out"
# Soma real das secoes estaticas (.data + .bss) dos modulos de modelo/uso no host Linux:
mkdir -p "$work/objs"
for m in farolgosto farolrec farolconsent farolsinal farolficha; do
  cc -O2 -DNV_MAPA_PURO -Isrc -c "src/${m}.c" -o "$work/objs/${m}.o"
done
size "$work/objs/"*.o

rodar_c() {
  local tag="$1" ccbin="$2" san="$3"; shift 3
  local d="$work/dados_${tag}"
  mkdir -p "$d"
  : > "$work/capture_${tag}.jsonl"
  rm -f "$work/capture.jsonl"
  : > "$work/capture.jsonl"
  echo "=== AUDITORIA $tag ($ccbin $san) ==="
  "$ccbin" "$@" "${CFLAGS_COMMON[@]}" "${SRCS[@]}" -o "$work/bin_${tag}" "${LIBS_COMMON[@]}"
  if [ "$san" = "tsan" ]; then
    local runner=()
    command -v setarch >/dev/null 2>&1 && runner=(setarch "$(uname -m)" -R)
    TSAN_OPTIONS="halt_on_error=1:exitcode=66" "${runner[@]+"${runner[@]}"}" \
      "$work/bin_${tag}" "$base" "$d"
  else
    ASAN_OPTIONS="abort_on_error=1" UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1" \
      "$work/bin_${tag}" "$base" "$d"
  fi
  cp "$work/capture.jsonl" "$work/capture_${tag}.jsonl"
  python3 - "$work/capture_${tag}.jsonl" <<'PY'
import json, sys
rows = [json.loads(l) for l in open(sys.argv[1], encoding="utf-8") if l.strip()]
assert len(rows) >= 3, f"esperava >=3 POSTs capturados, vieram {len(rows)}"
for r in rows:
    body = r["body"]
    for forbidden in ("CANARIO_PERFIL_901", "CANARIO_TOKEN_904", "canario905.example",
                      "cenasTv", "cenasServidor", "expVistas", "expAbertas",
                      "normAbertas", "farol-medida"):
        assert forbidden not in body, f"vazamento {forbidden} em {r['path']}: {body}"
    if r["path"].startswith("/v1/farol/compor"):
        assert "CANARIO_FRASE_903" not in body, "frase vazou em compor"
print(f"canarios rede ok ({len(rows)} POSTs auditados)")
PY
}

san_req="${SANITIZE:-all}"
case "$san_req" in
  0)
    rodar_c "gcc_plain" "${CC:-cc}" "plain"
    ;;
  1)
    rodar_c "asan" "${CC:-cc}" "asan" -fsanitize=address,undefined -fno-omit-frame-pointer
    ;;
  thread)
    cclang="${CC:-clang}"
    command -v clang >/dev/null 2>&1 && cclang=clang
    rodar_c "clang_tsan" "$cclang" "tsan" -fsanitize=thread -fno-omit-frame-pointer -fPIE -pie
    ;;
  all)
    rodar_c "gcc_asan" gcc "asan" -fsanitize=address,undefined -fno-omit-frame-pointer
    if command -v clang >/dev/null 2>&1; then
      rodar_c "clang_asan" clang "asan" -fsanitize=address,undefined -fno-omit-frame-pointer
      rodar_c "clang_tsan" clang "tsan" -fsanitize=thread -fno-omit-frame-pointer -fPIE -pie
    fi
    ;;
  *)
    echo "SANITIZE invalido: $san_req" >&2
    exit 2
    ;;
esac

# 4. Manipulador Node real de servidor/recomendacoes/src/farolgosto.js.
if command -v node >/dev/null 2>&1; then
  node tests/farol_gosto_auditoria_node.mjs
else
  echo "farol_gosto_auditoria: node ausente (FALHA)" >&2
  exit 1
fi

echo "farol_gosto_auditoria: tudo ok"
