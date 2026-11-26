#!/bin/bash
# Prova o servidor de falhas sinteticas (tests/farol_falhas_server.py) em porta
# efemera: 403, 404, 200 sem Range (ignora Range e sem Accept-Ranges), banda
# limitada a 8 Mbps sobre arquivo de ~40 Mbps, MKV truncado com cabecalho EBML
# valido, manifest Stremio em 12 s e debrid 401.
set -eu
cd "$(dirname "$0")/.."

dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-falhas.XXXXXX")"
port_file="$dir/port"
srv_pid=""
limpar() {
  if [ -n "$srv_pid" ]; then
    kill "$srv_pid" 2>/dev/null || true
    wait "$srv_pid" 2>/dev/null || true
  fi
  rm -rf "$dir"
}
trap limpar EXIT

python3 tests/farol_falhas_server.py --host 127.0.0.1 --port 0 --port-file "$port_file" &
srv_pid=$!

for _ in $(seq 1 100); do
  [ -s "$port_file" ] && break
  sleep 0.05
done
[ -s "$port_file" ] || { echo "farol_falhas: servidor nao publicou porta" >&2; exit 1; }
port="$(tr -d '\r\n' < "$port_file")"
base="http://127.0.0.1:${port}"

# Prontidao
code="$(curl -sS -o "$dir/health" -w '%{http_code}' "$base/health")"
[ "$code" = "200" ] || { echo "farol_falhas: /health -> $code" >&2; exit 1; }

# 1. Fonte 403 (FONTE_RECUSOU)
code="$(curl -sS -o /dev/null -w '%{http_code}' "$base/fonte/403")"
[ "$code" = "403" ] || { echo "farol_falhas: /fonte/403 -> $code" >&2; exit 1; }

# 2. Fonte 404 (FONTE_RECUSOU)
code="$(curl -sS -o /dev/null -w '%{http_code}' "$base/fonte/404")"
[ "$code" = "404" ] || { echo "farol_falhas: /fonte/404 -> $code" >&2; exit 1; }

# 3. Fonte 200 sem Range (FONTE_SEM_RANGE): ignora Range, sem Accept-Ranges nem Content-Range
code="$(curl -sS -D "$dir/semrange.hdr" -o "$dir/semrange.bin" -w '%{http_code}' \
  -H 'Range: bytes=0-1023' "$base/fonte/sem-range")"
[ "$code" = "200" ] || { echo "farol_falhas: /fonte/sem-range -> $code (esperado 200)" >&2; exit 1; }
if grep -qi '^accept-ranges:' "$dir/semrange.hdr"; then
  echo "farol_falhas: /fonte/sem-range enviou Accept-Ranges" >&2; exit 1
fi
if grep -qi '^content-range:' "$dir/semrange.hdr"; then
  echo "farol_falhas: /fonte/sem-range enviou Content-Range" >&2; exit 1
fi

# 4. Debrid 401 (DEBRID_DESCONECTOU)
code="$(curl -sS -o "$dir/debrid.json" -w '%{http_code}' "$base/debrid/user")"
[ "$code" = "401" ] || { echo "farol_falhas: /debrid/user -> $code" >&2; exit 1; }
grep -q '"error"' "$dir/debrid.json" || { echo "farol_falhas: /debrid/user sem JSON de erro" >&2; exit 1; }

# 5. MKV truncado (TV_FORMATO): 200, Content-Length declarado > bytes recebidos, magia EBML valida
curl -s -D "$dir/trunc.hdr" -o "$dir/trunc.mkv" "$base/fonte/mkv-truncado" || true
python3 - "$dir/trunc.hdr" "$dir/trunc.mkv" <<'PY'
import re, sys
hdr = open(sys.argv[1], "r", encoding="latin1").read()
data = open(sys.argv[2], "rb").read()
assert re.search(r"^HTTP/1\.[01]\s+200\b", hdr, re.M), f"status nao 200: {hdr!r}"
m = re.search(r"^Content-Length:\s*(\d+)", hdr, re.I | re.M)
assert m, "sem Content-Length em /fonte/mkv-truncado"
decl = int(m.group(1))
assert len(data) > 40, f"corpo curto demais: {len(data)}"
assert len(data) < decl, f"nao truncado: recebido {len(data)} vs declarado {decl}"
assert data[:4] == b"\x1a\x45\xdf\xa3", f"magia EBML invalida: {data[:4]!r}"
assert b"matroska" in data[:64], "DocType matroska ausente"
PY

# 6. Banda limitada a 8 Mbps sobre arquivo gerado de ~40 Mbps (REDE_CURTA)
curl -sS -I "$base/fonte/lenta-8mbps" > "$dir/lenta.head"
python3 - "$dir/lenta.head" <<'PY'
import re, sys
hdr = open(sys.argv[1], "r", encoding="latin1").read()
m = re.search(r"^Content-Length:\s*(\d+)", hdr, re.I | re.M)
assert m and int(m.group(1)) >= 40_000_000, f"arquivo ~40 Mbps esperado: {hdr!r}"
PY
meta="$(curl -sS -o "$dir/lenta.bin" -w '%{http_code} %{size_download} %{time_total}' \
  -H 'Range: bytes=0-2999999' "$base/fonte/lenta-8mbps")"
python3 - "$meta" "$dir/lenta.bin" <<'PY'
import sys
code, sz_s, t_s = sys.argv[1].split()
sz = int(sz_s)
t = float(t_s)
assert code == "206", f"status inesperado: {code}"
assert sz == 3_000_000, f"tamanho inesperado: {sz}"
mbps = (sz * 8.0) / (t * 1_000_000.0)
assert 6.8 <= mbps <= 9.2, f"taxa fora de 8 Mbps +-15%: {mbps:.2f} Mbps ({t:.3f} s)"
data = open(sys.argv[2], "rb").read(64)
assert data[:4] == b"\x1a\x45\xdf\xa3" and b"matroska" in data, "cabecalho MKV ausente"
PY

# 7. Manifest de add-on Stremio em 12 s (ADDON_LENTO)
meta="$(curl -sS --max-time 25 -o "$dir/manifest.json" -w '%{http_code} %{time_total}' \
  "$base/addon/manifest.json")"
python3 - "$meta" "$dir/manifest.json" <<'PY'
import json, sys
code, t_s = sys.argv[1].split()
t = float(t_s)
assert code == "200", f"manifest status inesperado: {code}"
assert t >= 11.9, f"manifest respondeu cedo demais: {t:.3f} s (esperado >= 12 s)"
doc = json.load(open(sys.argv[2], encoding="utf-8"))
assert doc.get("id") and "stream" in doc.get("resources", []), f"manifest invalido: {doc!r}"
PY

kill "$srv_pid"
wait "$srv_pid" 2>/dev/null || true
srv_pid=""
echo "farol_falhas: tudo ok"
