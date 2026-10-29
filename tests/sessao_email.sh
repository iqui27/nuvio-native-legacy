#!/bin/bash
# #216: login por e-mail e senha contra um servidor FALSO local. Ver o
# cabecalho de tests/sessao_email.c. Sem conta e sem credencial reais.
set -eu
cd "$(dirname "$0")/.."
dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sessao-email.XXXXXX")"
srv=""
trap '[ -n "$srv" ] && kill "$srv" 2>/dev/null; rm -rf "$dir"' EXIT
cat > "$dir/falso.py" <<'PY'
import http.server, json, sys, os
modo_arq, log_arq, porta_arq = sys.argv[1], sys.argv[2], sys.argv[3]
JWT = "x.eyJzdWIiOiJ1MSIsImV4cCI6NDEwMjQ0NDgwMH0.y"
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_POST(self):
        n = int(self.headers.get("Content-Length") or 0)
        corpo = self.rfile.read(n).decode("utf-8")
        modo = open(modo_arq).read().strip()
        with open(log_arq, "a") as f:
            f.write(json.dumps({"path": self.path, "apikey": self.headers.get("apikey"),
                                "auth": self.headers.get("Authorization"),
                                "ctype": self.headers.get("Content-Type"),
                                "corpo": json.loads(corpo)}) + "\n")
        st, r = 200, {"access_token": JWT, "refresh_token": "refresh-falso", "token_type": "bearer",
                      "user": {"id": "u1", "email": "pessoa@exemplo.test"}}
        if modo == "errado": st, r = 400, {"code": 400, "error_code": "invalid_credentials", "msg": "Invalid login credentials"}
        if modo == "errado_antigo": st, r = 400, {"error": "invalid_grant", "error_description": "Invalid login credentials"}
        if modo == "naoconfirmado": st, r = 400, {"code": 400, "error_code": "email_not_confirmed", "msg": "Email not confirmed"}
        if modo == "limite": st, r = 429, {"code": 429, "error_code": "over_request_rate_limit", "msg": "Request rate limit reached"}
        if modo == "fora503": st, r = 503, None
        b = (json.dumps(r) if r is not None else "error code: 503").encode()
        self.send_response(st)
        self.send_header("Content-Type", "application/json" if r is not None else "text/plain")
        self.send_header("Content-Length", str(len(b)))
        self.end_headers(); self.wfile.write(b)
s = http.server.ThreadingHTTPServer(("127.0.0.1", 0), H)
open(porta_arq, "w").write(str(s.server_address[1]))
s.serve_forever()
PY
echo ok > "$dir/modo"
python3 "$dir/falso.py" "$dir/modo" "$dir/pedidos" "$dir/porta" & srv=$!
for i in $(seq 50); do [ -s "$dir/porta" ] && break; sleep 0.1; done
porta=$(cat "$dir/porta")
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -pthread -Wall
       -Wno-deprecated-declarations -Wno-macro-redefined
       "-DNV_SUPABASE_URL=\"http://127.0.0.1:$porta\"" "-DNV_SUPABASE_ANON_KEY=\"anon-falsa\"")
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
bin="$dir/teste"
cc "${flags[@]}" src/sessao.c src/nuvem.c src/rede.c src/redeurl.c src/js.c src/jsw.c tests/sessao_email.c tests/sessao_boundary_stub.c -o "$bin"
roda() {  # modo frase
  echo "$1" > "$dir/modo"; : > "$dir/pedidos"
  local d; d="$(mktemp -d "$dir/dados.XXXX")"
  NV_T_DIR="$d" "$bin" "$1" "$2" > "$dir/saida" 2>&1 || { cat "$dir/saida"; echo "sessao_email.sh: FALHOU ($1)"; exit 1; }
  cat "$dir/saida"
  # NUNCA no log: e-mail, senha, token.
  if grep -E 'pessoa@exemplo|s3nh|outra|refresh-falso|eyJzdWIi' "$dir/saida"; then
    echo "sessao_email.sh: segredo no log ($1)"; exit 1; fi
}
roda ok ""
python3 - "$dir/pedidos" <<'PY'
import json, sys
p = [json.loads(l) for l in open(sys.argv[1])]
assert len(p) == 1, p
p = p[0]
assert p["path"] == "/auth/v1/token?grant_type=password", p["path"]
assert p["apikey"] == "anon-falsa" and p["auth"] == "Bearer anon-falsa", "cabecalhos"
assert p["ctype"].startswith("application/json"), p["ctype"]
assert p["corpo"] == {"email": "pessoa@exemplo.test", "password": "s3nh\"a\\fals@-ção"}, "corpo"
print("  pedido: caminho, apikey, Content-Type e corpo JSON escapado       ok")
PY
roda errado "E-mail ou senha incorretos."
roda errado_antigo "E-mail ou senha incorretos."
roda naoconfirmado "Este e-mail ainda não foi confirmado. Abra o link que a Nuvio mandou e tente de novo."
roda limite "Muitas tentativas. Espere um minuto e tente de novo."
roda fora503 "O servidor da conta Nuvio não respondeu (HTTP 503). Tente de novo em alguns minutos."
kill "$srv"; wait "$srv" 2>/dev/null || true; srv=""
roda semservidor "O servidor da conta Nuvio não respondeu. Tente de novo em alguns minutos."
echo "sessao_email.sh: ok"
