#!/bin/bash
# Assistente por frase P5 nas demais TVs (Farol #55: Samsung .tpk/.wgt, LG QR, WASM + /v1/farol/escolha).
#
#   SANITIZE=0 bash tests/farol_escolha_tv.sh
#   SANITIZE=1 bash tests/farol_escolha_tv.sh
#   CC=clang SANITIZE=1 bash tests/farol_escolha_tv.sh
set -eu
cd "$(dirname "$0")/.."

if [ -z "${TMPDIR:-}" ]; then
  echo "farol_escolha_tv: TMPDIR obrigatorio (sem fallback para /tmp)" >&2
  exit 2
fi
TMPDIR="${TMPDIR%/}"
mkdir -p "$TMPDIR"

san="${SANITIZE:-1}"
ccbin="${CC:-cc}"
case "$san" in
  0|1) ;;
  *) echo "farol_escolha_tv: SANITIZE desconhecido: $san" >&2; exit 2 ;;
esac

work=$(mktemp -d "$TMPDIR/nuvio-farol-esc55.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$work"' EXIT

# 1. Contratos estaticos (zero _Thread_local, zero log em farolescolha*).
if grep -rn '\b_Thread_local\b' src/farolescolha*.c src/farolescolha*.h tests/farol_escolha_tv.c; then
  echo "farol_escolha_tv: FALHA (_Thread_local proibido)" >&2
  exit 1
fi
if grep -rnE '\b(printf|fprintf|puts|fputs|syslog|NSLog|__android_log|LOGD|nv_log|SDL_Log)[[:space:]]*\(' \
     src/farolescolha.c src/farolescolha.h \
     src/farolescolha_rede.c src/farolescolha_rede.h \
     src/farolescolhaui.c src/farolescolhaui.h; then
  echo "farol_escolha_tv: FALHA (log proibido em src/farolescolha*)" >&2
  exit 1
fi

# 2. Checagem -fsyntax-only nas 4 plataformas.
sdl_cf=$(pkg-config --cflags sdl2 2>/dev/null || echo -I/usr/include/SDL2)
for plat in -DNV_ANDROID -DNV_TPK -DNV_WEBOS -D__EMSCRIPTEN__; do
  for f in src/farolescolha.c src/farolescolha_rede.c src/farolescolhaui.c src/resolver.c; do
    "$ccbin" "$plat" -fsyntax-only -Wall -Wextra -Werror -Wno-unused-function -Isrc $sdl_cf "$f"
  done
done

# 3. Servidor HTTP local espelhando /v1/farol/escolha + contador + auditoria.
cat > "$work/srv.py" <<'PY'
import http.server
import json
import sys
import threading

CHAVES_ESCOLHA = {'v', 'idioma', 'frase', 'perguntas', 'gosto', 'infantil', 'consentiu'}
CHAVES_GOSTO = {'generos', 'temas', 'pessoas', 'ancoras', 'nivel', 'fim_semana'}
PORT_FILE = sys.argv[1]
COUNT_FILE = sys.argv[2]
CAP_FILE = sys.argv[3]
lock = threading.Lock()
req_count = 0

with open(COUNT_FILE, 'w') as cf:
    cf.write('0')
open(CAP_FILE, 'w').close()

def record(path, data, err=None):
    global req_count
    with lock:
        req_count += 1
        with open(COUNT_FILE, 'w') as cf:
            cf.write(str(req_count))
        with open(CAP_FILE, 'a', encoding='utf-8') as cap:
            row = {'path': path, 'body': data}
            if err:
                row['error'] = err
            cap.write(json.dumps(row, ensure_ascii=False) + '\n')

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def _send_json(self, status, obj):
        raw = json.dumps(obj, ensure_ascii=False).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(raw)))
        self.end_headers()
        try:
            self.wfile.write(raw)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def do_POST(self):
        if not self.path.endswith('/v1/farol/escolha'):
            record(self.path, {}, 'rota_errada')
            self._send_json(404, {'erro': 'nao encontrado'})
            return
        if not self.headers.get('Authorization') or not self.headers.get('X-Nuvio-Auth'):
            record(self.path, {}, 'sem_auth')
            self._send_json(401, {'erro': 'nao autenticado'})
            return
        length = int(self.headers.get('Content-Length', '0'))
        raw = self.rfile.read(length) if length > 0 else b''
        try:
            data = json.loads(raw.decode('utf-8'))
        except Exception:
            record(self.path, {}, 'json_invalido')
            self._send_json(400, {'erro': 'json'})
            return

        err = None
        if set(data.keys()) != CHAVES_ESCOLHA:
            err = f'chaves_invalidas:{sorted(data.keys())}'
        elif data.get('v') != 1 or data.get('infantil') != 0 or data.get('consentiu') != 1:
            err = 'flags_invalidas'
        elif not isinstance(data.get('gosto'), dict) or not set(data['gosto'].keys()).issubset(CHAVES_GOSTO):
            err = 'gosto_invalido'
        elif not isinstance(data.get('perguntas'), list) or not (1 <= len(data['perguntas']) <= 9):
            err = 'perguntas_invalidas'

        record(self.path, data, err)
        if err:
            self._send_json(400, {'erro': err})
            return

        self._send_json(200, {
            'origem': 'llm',
            'respostas': {'tempo': 't100', 'peso': 'leve', 'companhia': 'crianca'},
            'falta': None,
        })

server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
with open(PORT_FILE, 'w') as pf:
    pf.write(str(server.server_port))
server.serve_forever()
PY

python3 "$work/srv.py" "$work/port" "$work/count" "$work/capture.jsonl" &
srv=$!
for i in $(seq 50); do [ -s "$work/port" ] && break; sleep 0.1; done
test -s "$work/port"
base="http://127.0.0.1:$(cat "$work/port")"

if ! pkg-config --exists sdl2 SDL2_image SDL2_ttf glesv2; then
  echo "farol_escolha_tv: SDL2/SDL2_image/SDL2_ttf/GLESv2 indisponivel" >&2
  exit 1
fi
read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
read -r -a sdk_libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf)"
san_fl=()
if [ "$san" = "1" ]; then
  san_fl=(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
fi

compile_objs() {
  local objdir="$1"
  shift
  local fl=("$@")
  mkdir -p "$objdir"
  local todo="$work/todo-$(basename "$objdir")"
  : > "$todo"
  for s in src/*.c src/dts/*.c; do
    case "$s" in src/main.c|src/tpk.c) continue ;; esac
    local o="$objdir/$(basename "$s" .c).o"
    if [ ! -f "$o" ] || [ "$s" -nt "$o" ] || [ -n "$(find src -name '*.h' -newer "$o" -print -quit)" ]; then
      printf '%s\n' "$s" >> "$todo"
    fi
  done
  if [ -s "$todo" ]; then
    printf '%s\0' "${fl[@]}" > "$work/flags-$(basename "$objdir")"
    local jobs="${FAROL_TEST_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
    # shellcheck disable=SC2016
    ccbin="$ccbin" objdir="$objdir" xargs -a "$todo" -P "$jobs" -I{} bash -c '
      mapfile -d "" f < "$0"
      o="$objdir/$(basename "{}" .c).o"
      "$ccbin" "${f[@]}" -c "{}" -o "$o"' "$work/flags-$(basename "$objdir")"
  fi
}

# 4. Teste real compilado e executado com -DNV_TPK (release .tpk: Voz.cs + QR alternativo).
tpk_fl=(-O1 -g -Isrc "${sdk_flags[@]}" -DNV_TPK -DNV_CEL_TEST_LOOPBACK -DCEL_VALIDADE_S=2
        -Wall -Wextra -Wno-deprecated-declarations "${san_fl[@]}")
tpk_objdir="$TMPDIR/nuvio-farol55-tpk-s${san}-$(basename "$ccbin")"
compile_objs "$tpk_objdir" "${tpk_fl[@]}"
for s in src/farolescolha.c src/farolescolha_rede.c src/farolescolhaui.c src/resolver.c; do
  "$ccbin" "${tpk_fl[@]}" -Werror -c "$s" -o "$work/tpk_$(basename "$s" .c).o"
done
"$ccbin" "${tpk_fl[@]}" -Werror tests/farol_escolha_tv.c "$tpk_objdir"/*.o -o "$work/tpk_ui" \
  "${sdk_libs[@]}" -lz -lGLESv2 -pthread -lm -ldl

tpk_dados="$work/dados-tpk"
mkdir -p "$tpk_dados"
rc=0
NUVIO_DADOS="$tpk_dados" ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1}" \
  "$work/tpk_ui" "$base" "$work/count" >"$work/log_tpk" 2>&1 || rc=$?
cat "$work/log_tpk"
[ "$rc" = 0 ] || exit "$rc"

# 5. Teste completo Samsung/WASM IME, LG QR, combinado, Android, privacidade, UTF-8 + PNGs 1920x1080.
desk_fl=(-O1 -g -Isrc "${sdk_flags[@]}" -DNV_LINUX_DESKTOP -DNV_CEL_TEST_LOOPBACK -DCEL_VALIDADE_S=2
         -Wall -Wextra -Wno-deprecated-declarations "${san_fl[@]}")
desk_objdir="$TMPDIR/nuvio-farol55-desk-s${san}-$(basename "$ccbin")"
compile_objs "$desk_objdir" "${desk_fl[@]}"
for s in src/farolescolha.c src/farolescolha_rede.c src/farolescolhaui.c src/resolver.c; do
  "$ccbin" "${desk_fl[@]}" -Werror -c "$s" -o "$work/desk_$(basename "$s" .c).o"
done
"$ccbin" "${desk_fl[@]}" -Werror tests/farol_escolha_tv.c "$desk_objdir"/*.o -o "$work/desk_ui" \
  "${sdk_libs[@]}" -lz -lGLESv2 -pthread -lm -ldl

dados="$work/dados-desk"
shots="$work/shots"
mkdir -p "$dados" "$shots"
export NUVIO_DADOS="$dados"
# O driver "dummy" do SDL nao abre GL: limpar antes de xvfb-run.
if [ "${SDL_VIDEODRIVER:-}" = dummy ]; then unset SDL_VIDEODRIVER; fi
rc=0
if [ -z "${DISPLAY:-}" ] && command -v xvfb-run >/dev/null 2>&1; then
  ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1}" \
    xvfb-run -a -s "-screen 0 1920x1080x24" "$work/desk_ui" "$base" "$work/count" "$shots" >"$work/log_desk" 2>&1 || rc=$?
else
  ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1}" \
    "$work/desk_ui" "$base" "$work/count" "$shots" >"$work/log_desk" 2>&1 || rc=$?
fi
cat "$work/log_desk"
[ "$rc" = 0 ] || exit "$rc"

for f in esc55-pergunta esc55-qr esc55-previa-qr esc55-ime esc55-aviso; do
  p="$shots/$f.png"
  [ -s "$p" ] || { echo "farol_escolha_tv: PNG ausente: $p" >&2; exit 1; }
  python3 - "$p" <<'PY'
import struct, sys
d = open(sys.argv[1], 'rb').read(24)
assert d[:8] == b'\x89PNG\r\n\x1a\n', "PNG invalido"
assert d[12:16] == b'IHDR', "IHDR ausente"
w, h = struct.unpack('>II', d[16:24])
assert (w, h) == (1920, 1080), f"dimensao inesperada: {w}x{h}"
PY
done
echo "farol_escolha_tv: capturas PNG 1920x1080 validadas e removidas na saida"

# 6. Auditoria de logs e corpos HTTP capturados (zero vazamento, so CHAVES_ESCOLHA).
python3 - "$work/log_tpk" "$work/log_desk" "$base" "$work/capture.jsonl" <<'PY'
import json, os, sys

log_text = open(sys.argv[1], encoding='utf-8').read() + "\n" + open(sys.argv[2], encoding='utf-8').read()
base_url = sys.argv[3]
cap_path = sys.argv[4]

for secret in (
    base_url,
    "tok_segredo_tv55",
    "algo leve de uma hora para ver com meu filho",
    "parcial tpk",
    "parcial ime",
    "frase combinada",
):
    assert secret not in log_text, f"segredo/frase vazou no log: {secret}"

CHAVES_ESCOLHA = {'v', 'idioma', 'frase', 'perguntas', 'gosto', 'infantil', 'consentiu'}
rows = [json.loads(line) for line in open(cap_path, encoding='utf-8') if line.strip()]
assert len(rows) >= 4, f"esperava >= 4 POSTs capturados, vieram {len(rows)}"
for row in rows:
    assert 'error' not in row, f"servidor recusou contrato: {row}"
    body = row['body']
    assert set(body.keys()) == CHAVES_ESCOLHA, f"chaves fora de CHAVES_ESCOLHA: {body.keys()}"
print(f"farol_escolha_tv: {len(rows)} POSTs auditados, tudo ok (SANITIZE={os.environ.get('SANITIZE','1')} CC={os.environ.get('CC','cc')})")
PY
