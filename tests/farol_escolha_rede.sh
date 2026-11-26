#!/bin/bash
# Assistente por frase P5 (Farol #54: src/farolescolha_rede + UI P5 + FAROL50-r2).
#
#   SANITIZE=0 bash tests/farol_escolha_rede.sh
#   SANITIZE=1 bash tests/farol_escolha_rede.sh
#   CC=clang SANITIZE=1 bash tests/farol_escolha_rede.sh
#   CC=clang SANITIZE=thread bash tests/farol_escolha_rede.sh   (rede + motor sob TSan)
set -eu
cd "$(dirname "$0")/.."

if [ -z "${TMPDIR:-}" ]; then
  echo "farol_escolha_rede: TMPDIR obrigatorio (sem fallback para /tmp)" >&2
  exit 2
fi
TMPDIR="${TMPDIR%/}"
mkdir -p "$TMPDIR"

san="${SANITIZE:-1}"
ccbin="${CC:-cc}"
case "$san" in
  0|1|thread) ;;
  *) echo "farol_escolha_rede: SANITIZE desconhecido: $san" >&2; exit 2 ;;
esac

work=$(mktemp -d "$TMPDIR/nuvio-farol-esc54.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$work"' EXIT

# 1. Contratos estaticos (zero _Thread_local, zero log em farolescolha*).
if grep -rn '_Thread_local' src/farolescolha*.c src/farolescolha*.h tests/farol_escolha_rede.c; then
  echo "farol_escolha_rede: FALHA (_Thread_local proibido)" >&2
  exit 1
fi
if grep -rnE '\b(printf|fprintf|puts|fputs|syslog|NSLog|__android_log|LOGD|nv_log|SDL_Log)[[:space:]]*\(' \
     src/farolescolha.c src/farolescolha.h \
     src/farolescolha_rede.c src/farolescolha_rede.h \
     src/farolescolhaui.c src/farolescolhaui.h; then
  echo "farol_escolha_rede: FALHA (log proibido em src/farolescolha*)" >&2
  exit 1
fi

# 2. Checagem -DNV_ANDROID -fsyntax-only -Wall -Wextra -Werror nos modulos alterados/criados.
sdl_cf=$(pkg-config --cflags sdl2 2>/dev/null || echo -I/usr/include/SDL2)
for f in src/farolescolha.c src/farolescolha_rede.c src/farolescolhaui.c src/resolver.c; do
  "$ccbin" -DNV_ANDROID -fsyntax-only -Wall -Wextra -Werror -Wno-unused-function -Isrc $sdl_cf "$f"
done

# 3. Servidor HTTP local espelhando CHAVES_ESCOLHA / rotaFarolEscolha + captura para auditoria.
cat > "$work/srv.py" <<'PY'
import http.server
import json
import sys
import threading
import time

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

    def _send(self, status, raw_bytes):
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(raw_bytes)))
        self.end_headers()
        try:
            self.wfile.write(raw_bytes)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def _send_json(self, status, obj):
        self._send(status, json.dumps(obj, ensure_ascii=False).encode('utf-8'))

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

        prefix = self.path[:-len('/v1/farol/escolha')]
        if prefix == '/canonico':
            self._send_json(200, {
                'origem': 'llm',
                'respostas': {'tempo': 't100', 'peso': 'leve', 'companhia': 'crianca'},
                'falta': None,
            })
        elif prefix == '/falta':
            self._send_json(200, {
                'origem': 'llm',
                'respostas': {'peso': 'leve', 'companhia': 'crianca'},
                'falta': 'humor',
            })
        elif prefix == '/falta-epoca':
            self._send_json(200, {
                'origem': 'llm',
                'respostas': {'peso': 'leve', 'companhia': 'crianca'},
                'falta': 'epoca',
            })
        elif prefix == '/429':
            self._send_json(429, {'erro': 'limite diario'})
        elif prefix == '/503':
            self._send_json(503, {'erro': 'indisponivel'})
        elif prefix == '/regras':
            self._send_json(200, {'origem': 'regras'})
        elif prefix == '/json-torto':
            self._send(200, b'{"origem":"llm","respostas":{')
        elif prefix == '/opcao-invalida':
            self._send_json(200, {
                'origem': 'llm',
                'respostas': {'tempo': 't30'},
                'falta': None,
            })
        elif prefix == '/lento-300ms':
            time.sleep(0.35)
            self._send_json(200, {
                'origem': 'llm',
                'respostas': {'tempo': 't45'},
                'falta': None,
            })
        elif prefix == '/timeout':
            time.sleep(9.0)
            self._send_json(200, {
                'origem': 'llm',
                'respostas': {'tempo': 't45'},
                'falta': None,
            })
        else:
            self._send_json(200, {'origem': 'regras'})

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

# 4. Parte 1: rede pura + motor (-DFER_SO_REDE; suporta SANITIZE=0, 1 e thread/TSan).
rede_fl=(-O1 -g -std=gnu11 -Isrc -DFER_SO_REDE -Wall -Wextra -Werror -pthread)
[ "$(uname -s)" = Darwin ] || rede_fl+=(-ldl)
case "$san" in
  1) rede_fl+=(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer) ;;
  thread) rede_fl+=(-fsanitize=thread -fno-omit-frame-pointer) ;;
esac

"$ccbin" "${rede_fl[@]}" \
  tests/farol_escolha_rede.c \
  src/farolescolha_rede.c \
  src/farolescolha.c \
  src/farolgosto.c \
  src/rede.c \
  src/redeurl.c \
  -o "$work/rede"

rc=0
if [ "$san" = "thread" ] && command -v setarch >/dev/null 2>&1 &&
   setarch "$(uname -m)" -R true 2>/dev/null; then
  setarch "$(uname -m)" -R "$work/rede" "$base" "$work/count" >"$work/log_rede" 2>&1 || rc=$?
else
  "$work/rede" "$base" "$work/count" >"$work/log_rede" 2>&1 || rc=$?
fi
cat "$work/log_rede"
[ "$rc" = 0 ] || exit "$rc"

# 5. Parte 2: UI completa + PNG 1920x1080 (pula em SANITIZE=thread pois Mesa/GL nao roda sob TSan).
if [ "$san" != "thread" ]; then
  if ! pkg-config --exists sdl2 SDL2_image SDL2_ttf glesv2; then
    echo "farol_escolha_rede: SDL2/SDL2_image/SDL2_ttf/GLESv2 indisponivel" >&2
    exit 1
  fi
  read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
  read -r -a sdk_libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf)"
  san_fl=()
  if [ "$san" = "1" ]; then
    san_fl=(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
  fi
  fl=(-O1 -g -Isrc "${sdk_flags[@]}" -DNV_LINUX_DESKTOP -Wall -Wextra -Wno-deprecated-declarations "${san_fl[@]}")

  objdir="$TMPDIR/nuvio-farol54-obj-s${san}-$(basename "$ccbin")"
  mkdir -p "$objdir"
  objs=()
  todo="$work/todo"
  : > "$todo"
  for s in src/*.c src/dts/*.c; do
    case "$s" in src/main.c) continue ;; esac
    o="$objdir/$(basename "$s" .c).o"
    objs+=("$o")
    if [ ! -f "$o" ] || [ "$s" -nt "$o" ] || [ -n "$(find src -name '*.h' -newer "$o" -print -quit)" ]; then
      printf '%s\n' "$s" >> "$todo"
    fi
  done
  if [ -s "$todo" ]; then
    printf '%s\0' "${fl[@]}" > "$work/flags"
    jobs="${FAROL_TEST_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
    # shellcheck disable=SC2016
    ccbin="$ccbin" objdir="$objdir" xargs -a "$todo" -P "$jobs" -I{} bash -c '
      mapfile -d "" f < "$0"
      o="$objdir/$(basename "{}" .c).o"
      "$ccbin" "${f[@]}" -c "{}" -o "$o"' "$work/flags"
  fi
  for s in src/farolescolha.c src/farolescolha_rede.c src/farolescolhaui.c src/resolver.c; do
    "$ccbin" "${fl[@]}" -Werror -c "$s" -o "$work/$(basename "$s" .c).o"
  done

  "$ccbin" "${fl[@]}" -Werror tests/farol_escolha_rede.c "${objs[@]}" -o "$work/ui" \
    "${sdk_libs[@]}" -lz -lGLESv2 -pthread -lm -ldl

  dados="$work/dados"
  shots="$work/shots"
  mkdir -p "$dados" "$shots"
  export NUVIO_DADOS="$dados"
  if [ "${SDL_VIDEODRIVER:-}" = dummy ]; then unset SDL_VIDEODRIVER; fi
  if [ -z "${DISPLAY:-}" ] && command -v xvfb-run >/dev/null 2>&1; then
    ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1}" \
      xvfb-run -a -s "-screen 0 1920x1080x24" "$work/ui" "$base" "$work/count" "$shots" >"$work/log_ui" 2>&1 || rc=$?
  else
    ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1}" \
      "$work/ui" "$base" "$work/count" "$shots" >"$work/log_ui" 2>&1 || rc=$?
  fi
  cat "$work/log_ui"
  [ "$rc" = 0 ] || exit "$rc"

  for f in esc54-mic esc54-ouvindo esc54-previa esc54-espera; do
    p="$shots/$f.png"
    [ -s "$p" ] || { echo "farol_escolha_rede: PNG ausente: $p" >&2; exit 1; }
    python3 - "$p" <<'PY'
import struct, sys
d = open(sys.argv[1], 'rb').read(24)
assert d[:8] == b'\x89PNG\r\n\x1a\n', "PNG invalido"
assert d[12:16] == b'IHDR', "IHDR ausente"
w, h = struct.unpack('>II', d[16:24])
assert (w, h) == (1920, 1080), f"dimensao inesperada: {w}x{h}"
PY
  done
  echo "farol_escolha_rede: capturas PNG 1920x1080 validadas e removidas na saida"
fi

# 6. Auditoria de logs e corpos HTTP capturados (zero vazamento, so CHAVES_ESCOLHA).
python3 - "$work/log_rede" "${work}/log_ui" "$base" "$work/capture.jsonl" <<'PY'
import json
import os
import sys

log_text = open(sys.argv[1], encoding='utf-8').read()
if os.path.exists(sys.argv[2]):
    log_text += "\n" + open(sys.argv[2], encoding='utf-8').read()
base_url = sys.argv[3]
cap_path = sys.argv[4]

for secret in (
    base_url,
    "tok_segredo_esc54_99",
    "tok_segredo_esc54_auth",
    "tok_segredo_ui54",
    "algo leve de uma hora para ver com meu filho",
    "-98765",
    "-424242",
):
    assert secret not in log_text, f"segredo/frase vazou no log: {secret}"

CHAVES_ESCOLHA = {'v', 'idioma', 'frase', 'perguntas', 'gosto', 'infantil', 'consentiu'}
rows = [json.loads(line) for line in open(cap_path, encoding='utf-8') if line.strip()]
assert len(rows) >= 7, f"esperava >= 7 POSTs capturados, vieram {len(rows)}"
for row in rows:
    assert 'error' not in row, f"servidor recusou contrato: {row}"
    body = row['body']
    assert set(body.keys()) == CHAVES_ESCOLHA, f"chaves fora de CHAVES_ESCOLHA: {body.keys()}"
    raw = json.dumps(body, ensure_ascii=False)
    for forb in ('-98765', '-424242', '-77777', 'diario', 'amigos', 'addons', 'perfil', 'horaResto', 'recusado', 'mostrado', 'http://', 'https://', 'www.'):
        assert forb not in raw.lower(), f"dado proibido no corpo HTTP: {forb}"
print(f"farol_escolha_rede: {len(rows)} POSTs auditados, tudo ok (SANITIZE={os.environ.get('SANITIZE','1')})")
PY
