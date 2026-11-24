#!/bin/bash
# Envio do registro do Farol (issue 18, PLANO.md §2.6/§2.7/§3.3/§5 item 18).
#   bash tests/farol_envio.sh                  (sem sanitizador, o padrao)
#   SANITIZE=1 bash tests/farol_envio.sh       (ASan+UBSan)
#   SANITIZE=thread bash tests/farol_envio.sh  (TSan; clang + setarch -R no Linux)
#
# 1. Contratos estaticos do modulo (sem SDL no header, sem TLS, sem I/O no passo).
# 2. tests/farol_envio.c contra tests/farol_envio_server.py (cenarios locais).
# 3. Contrato com o worker REAL (servidor/recomendacoes, node:sqlite): o C manda
#    o registro, recebe o codigo; uma correcao `corrigido` inserida para o fp
#    faz o segundo envio parar no GET /v1/farol/conhecido sem gravar registro.
set -eu
cd "$(dirname "$0")/.."

falha() { echo "farol_envio: FALHA ($*)" >&2; exit 1; }

# --- 1. contratos estaticos ----------------------------------------------
if grep -nE '(^|[^/])(_Thread_local|__thread)' src/farolenvio.c src/farolenvio.h | grep -vE '^[a-zA-Z0-9_./-]+:[0-9]+:[[:space:]]*(//|\*)'; then
  falha "TLS proibido no TPK40"
fi
if grep -nE '#include[[:space:]]+[<"](SDL|SDL2/)' src/farolenvio.h src/farolenvio.c; then
  falha "SDL nao entra em farolenvio"
fi
grep -q 'pthread_attr_setstacksize(&attr, FAROLENVIO_PILHA)' src/farolenvio.c || falha "fio de envio sem pilha propria"
grep -q 'malloc(cap)' src/farolenvio.c || falha "JSON do pacote precisa morar no heap"
# farolenvio_passo roda no fio da UI: nenhum dados_*/rede_* no corpo dele.
corpo_passo=$(awk '/^void farolenvio_passo\(void\)/,/^}/' src/farolenvio.c)
[ -n "$corpo_passo" ] || falha "farolenvio_passo ausente"
if printf '%s\n' "$corpo_passo" | grep -nE 'dados_(ler|gravar|apagar)|rede_(pedir|postar|baixar)|fopen'; then
  falha "I/O no fio da UI (farolenvio_passo)"
fi
grep -q 'avisos_registro_cabecalhos' src/avisos.c || falha "avisos.c nao expoe o cabecalho do registro"
grep -q '#define RESOLVER_ARQ "resolver-enviados.txt"' src/resolver.h || falha "RESOLVER_ARQ mudou: alinhar FAROLENVIO_ENVIADOS_ARQ"

# --- 2. cenarios locais ----------------------------------------------------
flags=(-Wall -Wextra -O1 -g -std=gnu11 -Isrc -pthread)
ccbin="${CC:-cc}"
runner=()
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread -fno-omit-frame-pointer)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    if [ "$(uname -s)" = Linux ] && command -v setarch >/dev/null 2>&1; then
      runner=(setarch "$(uname -m)" -R)
    fi
    ;;
  0) ;;
  *) falha "SANITIZE invalido: ${SANITIZE}" ;;
esac
libs=(-lm)
[ "$(uname -s)" = Darwin ] || libs+=(-ldl)

tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-envio.XXXXXX")
srv=""
nodepid=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; [ -z "$nodepid" ] || kill "$nodepid" 2>/dev/null || true; rm -rf "$tmp"' EXIT

# -Werror so no modulo novo e no teste (os .c antigos tem avisos de truncamento do gcc).
"$ccbin" "${flags[@]}" -Werror -fsyntax-only src/farolenvio.c
"$ccbin" "${flags[@]}" -Werror -fsyntax-only tests/farol_envio.c
"$ccbin" "${flags[@]}" \
  tests/farol_envio.c src/farolenvio.c src/farolpacote.c src/farolanel.c src/farolperfil.c \
  src/farolredator.c src/farolerro.c src/fontesonda.c src/regcodigo.c src/resolverregra.c \
  src/rede.c src/redeurl.c "${libs[@]}" -o "$tmp/envio"

python3 tests/farol_envio_server.py "$tmp/port" &
srv=$!
for _ in $(seq 100); do [ -s "$tmp/port" ] && break; sleep 0.1; done
test -s "$tmp/port" || falha "servidor local nao subiu"
base="http://127.0.0.1:$(cat "$tmp/port")"
mkdir -p "$tmp/dados"
${runner[@]+"${runner[@]}"} "$tmp/envio" "$base" "$tmp/dados" >"$tmp/log" 2>&1 || { cat "$tmp/log" >&2; falha "cenarios locais"; }
cat "$tmp/log"
# Privacidade: token sintetico e URL nunca saem em stdout/stderr.
if grep -q 'tok-farol-18' "$tmp/log" || grep -qF "$base" "$tmp/log"; then falha "token/URL vazou no log"; fi
kill "$srv" 2>/dev/null || true; srv=""

# --- 3. contrato com o worker real ------------------------------------------
if ! command -v node >/dev/null 2>&1 || ! node -e 'require("node:sqlite")' 2>/dev/null; then
  echo "farol_envio: node:sqlite ausente — contrato com o worker PULADO"
  echo "farol_envio: tudo ok (sem worker)"
  exit 0
fi
mkdir -p "$tmp/dados-w"
FAROL_BIN="$tmp/envio" FAROL_DIR="$tmp/dados-w" FAROL_DB="$tmp/w.db" FAROL_RUNNER="${runner[*]:-}" \
  node --no-warnings --input-type=module - <<'NODE' || falha "contrato com o worker"
import assert from "node:assert/strict";
import { execFile } from "node:child_process";
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import http from "node:http";
import { promisify } from "node:util";
import { DatabaseSync } from "node:sqlite";
import worker from "./servidor/recomendacoes/src/index.js";
import { decodificarCodigo } from "./servidor/recomendacoes/src/codigo.js";

const run = promisify(execFile);
const runner = process.env.FAROL_RUNNER ? process.env.FAROL_RUNNER.split(" ").filter(Boolean) : [];
const MIGRACOES = [
  "schema.sql", "migracao-001-avatar-nota.sql", "migracao-002-descobrivel.sql",
  "migracao-003-registro.sql", "migracao-004-triagem.sql", "migracao-005-amigos.sql",
  "migracao-006-social.sql", "migracao-007-resposta.sql", "migracao-008-identidade.sql",
  "migracao-018-farol.sql", "migracao-019-farol-janela.sql", "migracao-020-farol-conhecido.sql",
];
const sqlite = new DatabaseSync(process.env.FAROL_DB);
for (const m of MIGRACOES) {
  const sql = readFileSync(`./servidor/recomendacoes/${m}`, "utf8");
  for (const cmd of sql.split(/;\s*\n/)) {
    const c = cmd.replace(/^\s*--.*$/gm, "").trim();
    if (!c) continue;
    try { sqlite.exec(c); } catch (e) { if (!/duplicate column/.test(String(e.message))) throw e; }
  }
}
sqlite.prepare("INSERT OR REPLACE INTO sessao (hash, id, nome, expira) VALUES (?, ?, ?, 9999999999)")
  .run(createHash("sha256").update("nuvio:tok-farol-18").digest("hex"), "nuvio:pessoa-farol-18", "");
const stmt = (sql, v) => {
  const st = sqlite.prepare(sql);
  return {
    first: async () => st.get(...v) ?? null,
    all: async () => ({ results: st.all(...v) }),
    run: async () => { const r = st.run(...v); return { meta: { changes: Number(r.changes), last_row_id: Number(r.lastInsertRowid) } }; },
    _exec: () => st.run(...v),
  };
};
const DB = {
  prepare(sql) { return { ...stmt(sql, []), bind: (...v) => stmt(sql, v) }; },
  async batch(l) { sqlite.exec("BEGIN"); try { for (const s of l) s._exec(); sqlite.exec("COMMIT"); } catch (e) { sqlite.exec("ROLLBACK"); throw e; } return []; },
};
const env = { DB, FAROL_SAL: "sal-farol-18", SUPABASE_URL: "https://sb.test", SUPABASE_ANON_KEY: "anon", TRAKT_CLIENT_ID: "tc" };

const rotas = [];
const srv = http.createServer(async (req, res) => {
  const partes = [];
  for await (const p of req) partes.push(p);
  const corpo = Buffer.concat(partes);
  const u = new URL(req.url, "https://w.test");
  rotas.push(req.method + " " + u.pathname);
  const h = new Headers();
  for (const [k, v] of Object.entries(req.headers)) if (typeof v === "string") h.set(k, v);
  const r = await worker.fetch(new Request(u.href, {
    method: req.method, headers: h, body: req.method === "GET" ? undefined : corpo,
  }), env);
  const b = Buffer.from(await r.arrayBuffer());
  res.writeHead(r.status, { "content-type": r.headers.get("content-type") || "application/json", "content-length": b.length });
  res.end(b);
});
await new Promise((ok) => srv.listen(0, "127.0.0.1", ok));
const base = `http://127.0.0.1:${srv.address().port}`;
const rodar = async (fase) => {
  const bin = process.env.FAROL_BIN, args = ["--worker", base, process.env.FAROL_DIR, fase];
  const { stdout } = runner.length ? await run(runner[0], [...runner.slice(1), bin, ...args])
                                   : await run(bin, args);
  return stdout.trim();
};
try {
  const contar = () => Number(sqlite.prepare("SELECT COUNT(*) AS n FROM farol_registro").get().n);
  const s1 = await rodar("fase1");
  const m = /^fp=([0-9a-f]{16}) codigo=([0-9A-Z]{6})$/m.exec(s1);
  assert.ok(m, "saida da fase1: " + s1);
  const id = decodificarCodigo(m[2]);
  assert.ok(id !== null && id >= 536870912, "codigo fora da faixa do Farol");
  const lin = sqlite.prepare("SELECT fp, codigo, plataforma, versao FROM farol_registro WHERE id = ?").get(id);
  assert.equal(lin.fp, m[1], "fp da TV == fp do servidor (sha256(fingerprint)[:16])");
  assert.equal(lin.codigo, "TV_FORMATO_REPETE");
  assert.equal(lin.plataforma, "webos");
  assert.deepEqual(rotas, ["GET /v1/farol/conhecido", "POST /v1/farol/registro"]);
  const n1 = contar();
  assert.equal(n1, 1);
  sqlite.prepare("INSERT INTO farol_conhecido (fp, estado, versao, nota_pt, nota_en) VALUES (?, 'corrigido', '99.0.0', 'Corrigido no 99', 'Fixed in 99')").run(m[1]);
  rotas.length = 0;
  const s2 = await rodar("fase2");
  assert.ok(s2.includes("estado=corrigido fp=" + m[1]), "saida da fase2: " + s2);
  assert.deepEqual(rotas, ["GET /v1/farol/conhecido"], "corrigido nao pode POSTar");
  assert.equal(contar(), n1, "nenhum registro novo");
  console.log("farol_envio: contrato TV <-> worker real ok (codigo " + m[2] + ")");
} finally {
  srv.close();
  sqlite.close();
}
NODE

echo "farol_envio: tudo ok"
