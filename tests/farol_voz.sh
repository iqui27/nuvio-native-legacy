#!/bin/bash
# Teste integrado da issue #31 (ditado em "Achar o que ver" + FAROL30-r1).
#   bash tests/farol_voz.sh
#   SANITIZE=0 bash tests/farol_voz.sh
#   CC=clang SANITIZE=1 bash tests/farol_voz.sh
set -eu
cd "$(dirname "$0")/.."

# Contratos de portabilidade e privacidade (#31).
if grep -rn '\b_Thread_local\b' src/farolvozui.c src/farolvozui.h src/farolbusca.c \
    src/farolbuscaparse.c src/farolachaui.c src/resolver.c | grep -vE '^[^:]+:[0-9]+:[[:space:]]*//'; then
  echo "farol_voz: _Thread_local proibido" >&2; exit 1
fi
if grep -rnE '\b(printf|fprintf|syslog|NSLog|__android_log|LOGD|puts|fputs)[[:space:]]*\(' \
    src/farolvozui.c src/farolachaui.c | grep -vE '^[^:]+:[0-9]+:[[:space:]]*//'; then
  echo "farol_voz: log proibido em src/farolvozui.c / src/farolachaui.c" >&2; exit 1
fi

work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-voz.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$work"' EXIT

ccbin="${CC:-cc}"
flags_base=(-Wall -Wextra -Wno-unused-function -O1 -g -std=gnu11 -pthread)
link_dl=()
[ "$(uname -s)" = Darwin ] || link_dl=(-ldl)
case "${SANITIZE:-1}" in
  1) flags_base+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags_base+=(-fsanitize=thread -fno-omit-frame-pointer)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac
flags=("${flags_base[@]}" -Werror)

# Checagem -DNV_ANDROID -fsyntax-only nos modulos alterados/criados.
sdl_cf=$(pkg-config --cflags sdl2 2>/dev/null || echo -I/usr/include/SDL2)
for f in src/farolvozui.c src/farolbusca.c src/farolbuscaparse.c src/farolachaui.c src/resolver.c; do
  "$ccbin" -DNV_ANDROID -fsyntax-only -Wall -Wextra -Werror -Wno-unused-function -Isrc $sdl_cf "$f"
done

# Servidor HTTP local com contador de pedidos e rota /privado-infantil + /misto.
cat > "$work/srv.py" <<'PY'
import http.server
import json
import sys
import time

CHAVES_PEDIDO = {
    'frase', 'generos', 'idioma', 'historico',
    'vistos', 'infantil', 'privado', 'consentiu',
}
COUNT_FILE = sys.argv[2]
req_count = 0

def bump():
    global req_count
    req_count += 1
    with open(COUNT_FILE, 'w') as cf:
        cf.write(str(req_count))

with open(COUNT_FILE, 'w') as cf:
    cf.write('0')

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def _send_json(self, status, obj):
        body = json.dumps(obj).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def do_POST(self):
        bump()
        if not self.path.endswith('/v1/farol/busca'):
            self._send_json(404, {'erro': 'nao encontrado'})
            return
        if not self.headers.get('Authorization') or not self.headers.get('X-Nuvio-Auth'):
            self._send_json(401, {'erro': 'nao autenticado'})
            return
        length = int(self.headers.get('Content-Length', '0'))
        raw = self.rfile.read(length) if length > 0 else b''
        data = json.loads(raw.decode('utf-8'))
        for k in data.keys():
            if k not in CHAVES_PEDIDO:
                self._send_json(400, {'erro': 'campo extra'})
                return
        prefix = self.path[:-len('/v1/farol/busca')]
        if prefix == '/slow':
            time.sleep(10)
            self._send_json(200, {'ok': 1, 'ids': [], 'origem': 'llm', 'llm_status': 200})
            return
        if prefix == '/429':
            self._send_json(429, {'erro': 'limite'})
            return
        if prefix == '/privado-infantil':
            if set(data.keys()) != {'frase', 'infantil', 'consentiu'} or data.get('infantil') != 1 or data.get('consentiu') != 0:
                self._send_json(418, {'erro': 'privado+infantil invalido'})
                return
            self._send_json(200, {
                'ok': 1,
                'ids': [{'tmdb': 778, 'imdb': 'tt0000105', 'tipo': 'movie'}],
                'origem': 'llm', 'llm_status': 200,
            })
            return
        if prefix == '/sem-consent':
            if data.get('consentiu') != 0 or 'generos' in data or 'historico' in data or 'vistos' in data:
                self._send_json(418, {'erro': 'esperava consentiu:0'})
                return
            self._send_json(200, {
                'ok': 1,
                'ids': [{'tmdb': 550, 'imdb': 'tt0000101', 'tipo': 'movie'}],
                'origem': 'llm', 'llm_status': 200,
            })
            return
        if prefix == '/so-consent1':
            if data.get('consentiu') != 1:
                self._send_json(418, {'erro': 'esperava consentiu:1'})
                return
            self._send_json(200, {
                'ok': 1,
                'ids': [{'tmdb': 550, 'imdb': 'tt0000101', 'tipo': 'movie'}],
                'origem': 'llm', 'llm_status': 200,
            })
            return
        if prefix == '/misto':
            self._send_json(200, {
                'ok': 1,
                'ids': [
                    {'tmdb': 550, 'imdb': 'tt0000101', 'tipo': 'movie'},
                    {'tmdb': 778, 'imdb': 'tt0000105', 'tipo': 'movie'},
                    {'tmdb': 999001, 'imdb': 'tt9990001', 'tipo': 'movie'},
                    {'tmdb': 1396, 'imdb': '', 'tipo': 'series'},
                ],
                'origem': 'llm', 'llm_status': 200,
            })
            return
        self._send_json(200, {
            'ok': 1,
            'ids': [
                {'tmdb': 550, 'imdb': 'tt0000101', 'tipo': 'movie'},
                {'tmdb': 1396, 'imdb': '', 'tipo': 'series'},
                {'tmdb': 777, 'imdb': 'tt0000103', 'tipo': 'movie'},
            ],
            'origem': 'llm', 'llm_status': 200,
        })

server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
with open(sys.argv[1], 'w') as pf:
    pf.write(str(server.server_port))
server.serve_forever()
PY

python3 "$work/srv.py" "$work/port" "$work/count" &
srv=$!
for i in $(seq 50); do [ -s "$work/port" ] && break; sleep 0.1; done
test -s "$work/port"
base="http://127.0.0.1:$(cat "$work/port")"

python3 tests/farol_resolver_ponte.py "$work"

# Compila modulos da #31 com -Werror e demais modulos de apoio com -Wall -Wextra.
strict_objs=()
for f in src/farolvozui.c src/farolbusca.c src/farolbuscaparse.c \
         src/resolver.c src/farolachaui.c tests/farol_voz.c; do
  o="$work/$(echo "$f" | tr / _).o"
  "$ccbin" "${flags[@]}" -DRESOLVER_TESTE -DNV_ANDROID -DNV_SHOT_HOOKS \
    -I"$work" -Isrc -Itests $sdl_cf -c "$f" -o "$o"
  strict_objs+=("$o")
done

"$ccbin" "${flags_base[@]}" -DRESOLVER_TESTE -DNV_ANDROID -DNV_SHOT_HOOKS \
  -I"$work" -Isrc -Itests $sdl_cf -c src/extplayer.c -o "$work/extplayer.o"

"$ccbin" "${flags_base[@]}" -DRESOLVER_TESTE -DNV_SHOT_HOOKS \
  "${strict_objs[@]}" "$work/extplayer.o" \
  src/farolacha.c \
  src/resolverregra.c src/farolregra.c src/farol.c \
  src/farolerro.c src/farolanel.c src/fontesonda.c \
  src/addonguia.c src/addonmodo.c src/farolacao_addon.c src/addonstats.c src/sistexto.c src/entrada_texto.c \
  src/rede.c src/redeurl.c \
  -I"$work" -Isrc -Itests $sdl_cf \
  $(pkg-config --libs sdl2 2>/dev/null || echo -lSDL2) "${link_dl[@]+"${link_dl[@]}"}" -lm \
  -Wl,--wrap=SDL_GetTicks \
  -o "$work/farol-voz-test"

rc=0
if [ "${SANITIZE:-1}" = thread ] && command -v setarch >/dev/null 2>&1 &&
   setarch "$(uname -m)" -R true 2>/dev/null; then
  setarch "$(uname -m)" -R "$work/farol-voz-test" "$base" "$work/count" >"$work/log" 2>&1 || rc=$?
else
  "$work/farol-voz-test" "$base" "$work/count" >"$work/log" 2>&1 || rc=$?
fi
cat "$work/log"
[ "$rc" = 0 ] || exit "$rc"

python3 - "$work/log" "$base" <<'PY'
import sys
t = open(sys.argv[1]).read()
assert sys.argv[2] not in t, "URL vazou no log"
assert 'sint_tok_voz' not in t, "token vazou no log"
assert 'um suspense curto' not in t, "frase vazou no log"
PY

# Captura PNG 1920x1080 (quando display/GLES2 disponivel; limpa SDL_VIDEODRIVER=dummy).
shot_fl=(-O1 -g -Isrc -DAJUSTES_TESTE -DAVISOS_TESTE_ENVIO -DRESOLVER_TESTE
         -DNV_VOZ_TELA_SHOT -Wall -Wextra -Wno-deprecated-declarations)
case "$("$ccbin" --version 2>/dev/null | head -n1)" in
  *clang*|*Clang*) shot_fl+=(-Wno-macro-redefined) ;;
esac
if [ "${SANITIZE:-1}" = 1 ]; then
  shot_fl+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi

libs=()
pode_shot=1
if [ "$(uname -s)" = "Darwin" ]; then
  shot_fl+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
  libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
elif pkg-config --exists sdl2 SDL2_image SDL2_ttf && pkg-config --exists glesv2; then
  read -r -a cf <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf glesv2)"
  read -r -a lf <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf glesv2)"
  shot_fl+=("${cf[@]}" -DNV_LINUX_DESKTOP)
  libs=("${lf[@]}" -lz -pthread -lm -ldl)
else
  pode_shot=0
fi

runner=()
if [ "$pode_shot" = 1 ]; then
  if [ "${SDL_VIDEODRIVER:-}" = dummy ]; then unset SDL_VIDEODRIVER; fi
  if [ "$(uname -s)" != "Darwin" ] && [ -z "${DISPLAY:-}" ]; then
    if command -v xvfb-run >/dev/null 2>&1; then
      runner=(xvfb-run -a -s "-screen 0 1920x1080x24")
    else
      pode_shot=0
    fi
  fi
fi

if [ "$pode_shot" = 1 ]; then
  NUVIO_DADOS="$work/dados"
  mkdir -p "$NUVIO_DADOS" "$work/shot" "$work/obj"
  export NUVIO_DADOS
  ls src/*.c src/dts/*.c | grep -v '/main\.c$' > "$work/fontes"
  export ccbin objdir="$work/obj"
  printf '%s\0' "${shot_fl[@]}" > "$work/flags"
  jobs="${FAROL_SHOT_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
  # shellcheck disable=SC2016
  xargs -a "$work/fontes" -P "$jobs" -I{} bash -c '
    mapfile -d "" f < "$0"
    o="$objdir/$(echo "{}" | tr / _).o"
    "$ccbin" "${f[@]}" -c "{}" -o "$o"' "$work/flags"
  "$ccbin" "${shot_fl[@]}" tests/farol_voz.c "$work/obj"/*.o \
    -o "$work/farol-voz-shot" "${libs[@]}"
  ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1}" \
    "${runner[@]+"${runner[@]}"}" "$work/farol-voz-shot" "$work/shot/cap" "$base/ok"

  for f in "$work/shot/cap-tela1.png" "$work/shot/cap-ouvindo.png" \
           "$work/shot/cap-revisao.png" "$work/shot/cap-tela7.png"; do
    [ -s "$f" ] || { echo "farol_voz: captura ausente ou vazia: $f" >&2; exit 1; }
    python3 - "$f" <<'PY'
import struct, sys
d = open(sys.argv[1], 'rb').read(24)
assert d[:8] == b'\x89PNG\r\n\x1a\n', "PNG invalido"
assert d[12:16] == b'IHDR', "IHDR ausente"
w, h = struct.unpack('>II', d[16:24])
assert (w, h) == (1920, 1080), f"dimensao inesperada: {w}x{h}"
PY
  done
  echo "farol_voz: capturas PNG 1920x1080 validadas e removidas na saida"
else
  echo "farol_voz: AVISO - sem display/GLES2 para captura PNG; logica validada" >&2
fi

echo "farol_voz: tudo ok"
