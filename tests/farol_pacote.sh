#!/bin/bash
# Pacote FarolRegistro v1 (lote sense, issue 11, PLANO.md §2.6).
#   bash tests/farol_pacote.sh                  (ASan+UBSan, o padrao)
#   SANITIZE=thread bash tests/farol_pacote.sh  (clang TSan com setarch -R)
#   SANITIZE=0 bash tests/farol_pacote.sh       (sem sanitizador)
set -eu
cd "$(dirname "$0")/.."
flags=()
ccbin="${CC:-cc}"
runner=()
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    if [ "$(uname -s)" = Linux ] && command -v setarch >/dev/null 2>&1; then
      runner=(setarch "$(uname -m)" -R)
    fi
    ;;
  0) ;;
  *) echo "SANITIZE invalido: ${SANITIZE}" >&2; exit 2 ;;
esac

base_tmp="${TMPDIR:-/tmp}"
base_tmp="${base_tmp%/}"
bin=$(mktemp "${base_tmp}/nuvio-farol-pacote-test.XXXXXX")
db=$(mktemp "${base_tmp}/nuvio-farol-pacote-db.XXXXXX")
trap 'rm -f "$bin" "$db" "${db}-wal" "${db}-shm"' EXIT

"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g -pthread \
  tests/farol_pacote.c src/farolpacote.c src/farolanel.c src/farolperfil.c \
  src/farolredator.c src/farolerro.c src/fontesonda.c src/regcodigo.c \
  -Isrc -lm -o "$bin"

TMPDIR="$base_tmp" ${runner[@]+"${runner[@]}"} "$bin"

# Contratos estaticos: modulo puro, sem malloc e sem _Thread_local (TPK40).
if grep -nE '(malloc|calloc|realloc|free)[[:space:]]*\(' src/farolpacote.c; then
  echo "farol_pacote: FALHA (alocacao dinamica em src/farolpacote.c)" >&2
  exit 1
fi
if grep -nE '(^|[^/])(_Thread_local|__thread)' src/farolpacote.c src/farolpacote.h | grep -vE '^[a-zA-Z0-9_./-]+:[0-9]+:[[:space:]]*(//|\*)'; then
  echo "farol_pacote: FALHA (TLS proibido no TPK40 em src/farolpacote)" >&2
  exit 1
fi
if grep -nE '#include[[:space:]]+[<"](SDL|dados\.h|rede\.h)' src/farolpacote.c src/farolpacote.h; then
  echo "farol_pacote: FALHA (dependencia proibida em modulo puro)" >&2
  exit 1
fi

# Contrato fim-a-fim: pacotes montados em C -> servidor/recomendacoes (POST /v1/farol/registro)
# -> codigo na TV (regcodigo_de_id) == codigo do servidor == tools/farol-ler.mjs.
if command -v node >/dev/null 2>&1; then
  FAROL_BIN="$bin" FAROL_DB="$db" FAROL_RUNNER="${runner[*]:-}" node --input-type=module - <<'NODE'
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { DatabaseSync } from "node:sqlite";
import worker from "./servidor/recomendacoes/src/index.js";
import { codigoRegistro, decodificarCodigo } from "./servidor/recomendacoes/src/codigo.js";

const bin = process.env.FAROL_BIN;
const dbPath = process.env.FAROL_DB;
const runner = process.env.FAROL_RUNNER ? process.env.FAROL_RUNNER.split(" ").filter(Boolean) : [];
const runBin = (args) => {
  const cmd = runner.length ? runner[0] : bin;
  const full = runner.length ? [...runner.slice(1), bin, ...args] : args;
  return execFileSync(cmd, full, { encoding: "utf8" }).trim();
};

const MIGRACOES = [
  "schema.sql", "migracao-001-avatar-nota.sql", "migracao-002-descobrivel.sql",
  "migracao-003-registro.sql", "migracao-004-triagem.sql", "migracao-005-amigos.sql",
  "migracao-006-social.sql", "migracao-007-resposta.sql", "migracao-008-identidade.sql",
  "migracao-018-farol.sql", "migracao-019-farol-janela.sql", "migracao-020-farol-conhecido.sql",
];

const sqlite = new DatabaseSync(dbPath);
try {
  for (const m of MIGRACOES) {
    const sql = readFileSync(`./servidor/recomendacoes/${m}`, "utf8");
    for (const cmd of sql.split(/;\s*\n/)) {
      const c = cmd.replace(/^\s*--.*$/gm, "").trim();
      if (!c) continue;
      try { sqlite.exec(c); } catch (e) { if (!/duplicate column/.test(String(e.message))) throw e; }
    }
  }
  sqlite.prepare("INSERT OR REPLACE INTO sessao (hash, id, nome, expira) VALUES (?, ?, ?, 9999999999)")
    .run(createHash("sha256").update("nuvio:tok-farol-11").digest("hex"), "nuvio:pessoa-farol-11", "");

  const stmt = (sql, valores) => {
    const st = sqlite.prepare(sql);
    return {
      first: async () => st.get(...valores) ?? null,
      all: async () => ({ results: st.all(...valores) }),
      run: async () => {
        const r = st.run(...valores);
        return { meta: { changes: Number(r.changes), last_row_id: Number(r.lastInsertRowid) } };
      },
      _exec: () => st.run(...valores),
    };
  };
  const DB = {
    prepare(sql) { return { ...stmt(sql, []), bind: (...v) => stmt(sql, v) }; },
    async batch(lista) {
      sqlite.exec("BEGIN");
      try { for (const s of lista) s._exec(); sqlite.exec("COMMIT"); }
      catch (e) { sqlite.exec("ROLLBACK"); throw e; }
      return [];
    },
  };
  const env = {
    DB, FAROL_SAL: "sal-farol-11",
    SUPABASE_URL: "https://sb.test", SUPABASE_ANON_KEY: "anon", TRAKT_CLIENT_ID: "tc",
  };

  const linhas = runBin(["--emitir-contrato"]).split("\n").filter(Boolean);
  assert.equal(linhas.length, 3, "esperava 3 pacotes do binario C");

  for (const raw of linhas) {
    assert.ok(Buffer.byteLength(raw, "utf8") <= 35000, "pacote passou de 35 000 B");
    const obj = JSON.parse(raw);
    const res = await worker.fetch(new Request("https://w.test/v1/farol/registro", {
      method: "POST",
      headers: { authorization: "Bearer tok-farol-11", "x-nuvio-auth": "nuvio" },
      body: raw,
    }), env);
    const body = await res.json();
    assert.equal(res.status, 200, "servidor recusou pacote do C: " + JSON.stringify(body));
    assert.equal(body.ok, 1);

    const id = decodificarCodigo(body.codigo);
    assert.ok(id !== null && id >= 536870912, "id fora da faixa do Farol");

    // Codigo calculado na TV (src/regcodigo.c) == codigo do servidor
    const codTv = runBin(["--regcodigo", String(id)]);
    assert.equal(codTv, body.codigo, "codigo na TV divergiu do servidor");
    assert.equal(codTv, codigoRegistro(id));

    // tools/farol-ler.mjs le o registro pelo MESMO codigo da TV
    const saidaLer = execFileSync(process.execPath, ["tools/farol-ler.mjs", "--sqlite", dbPath, codTv], {
      encoding: "utf8",
    });
    const lido = JSON.parse(saidaLer);
    assert.equal(lido.codigo, codTv);
    assert.equal(lido.id, id);
    assert.equal(lido.v, 1);
    assert.equal(lido.erro.fingerprint, obj.erro.fingerprint);
    assert.equal(lido.erro.fp, obj.erro.fp);
    assert.equal("pessoa_h" in lido, false);
  }

  // Confere varios ids na faixa do Farol (2^29 .. 2^30-1) entre regcodigo.c e codigo.js
  for (const id of [536870912, 536870913, 600000000, 1073741823]) {
    const codTv = runBin(["--regcodigo", String(id)]);
    assert.equal(codTv, codigoRegistro(id));
    assert.equal(decodificarCodigo(codTv), id);
  }
  console.log("farol_pacote: contrato TV <-> servidor <-> farol-ler.mjs ok");
} finally {
  sqlite.close();
}
NODE
fi

echo "farol_pacote: tudo ok"
