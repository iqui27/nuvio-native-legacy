#!/bin/bash
# Coletor do aparelho e do cliente do Farol (lote sense, issue 8).
#   bash tests/farol_perfil.sh                  (ASan+UBSan, o padrao)
#   SANITIZE=thread bash tests/farol_perfil.sh  (clang TSan com setarch -R)
#   SANITIZE=0 bash tests/farol_perfil.sh       (sem sanitizador)
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
bin=$(mktemp "${base_tmp}/nuvio-farol-perfil-test.XXXXXX")
trap 'rm -f "$bin" "${bin}40"' EXIT

"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g -pthread -DFAROL_PERFIL_TESTE \
  tests/farol_perfil.c src/farolperfil.c src/farolredator.c src/farolerro.c src/fontesonda.c \
  -Isrc -lm -o "$bin"
TMPDIR="$base_tmp" ${runner[@]+"${runner[@]}"} "$bin"

# Segundo binario com as macros do pacote TPK40: a identidade tizen-tpk40
# decidida em compilacao nao pode ser sobrescrita pela coleta.
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g -pthread -DFAROL_PERFIL_TESTE \
  -DNV_TPK -DNV_TPK40 \
  tests/farol_perfil.c src/farolperfil.c src/farolredator.c src/farolerro.c src/fontesonda.c \
  -Isrc -lm -o "${bin}40"
TMPDIR="$base_tmp" ${runner[@]+"${runner[@]}"} "${bin}40" tpk40

# Contratos estaticos: sem heap e sem _Thread_local (TPK40 proibe PT_TLS).
if grep -nE '(malloc|calloc|realloc|free)[[:space:]]*\(' src/farolperfil.c; then
  echo "farol_perfil: FALHA (alocacao dinamica em src/farolperfil.c)" >&2
  exit 1
fi
if grep -nE '(^|[^/])(_Thread_local|__thread)' src/farolperfil.c src/farolperfil.h | grep -vE '^[a-zA-Z0-9_./-]+:[0-9]+:[[:space:]]*(//|\*)'; then
  echo "farol_perfil: FALHA (TLS proibido no TPK40 em src/farolperfil)" >&2
  exit 1
fi

# Teste da funcao WASM na casca tools/tizen-shell.html (quando node disponivel).
if command -v node >/dev/null 2>&1; then
  node - <<'NODE'
const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const html = fs.readFileSync('tools/tizen-shell.html', 'utf8');
const m = /\/\/ NV-FAROL-TV-INICIO\n([\s\S]*?)\/\/ NV-FAROL-TV-FIM/.exec(html);
assert(m, 'bloco NV-FAROL-TV ausente em tools/tizen-shell.html');
assert(/window\.nvFarolTvInfo\s*\(\s*\)/.test(html), 'preRun deve chamar window.nvFarolTvInfo()');

// 0. Execucao no escopo global (window === globalThis), chamando window.nvFarolTvInfo()
//    SEM argumentos — trava regressao de recursao infinita e prova leitura dos globais.
{
  const ctx = {
    webapis: { productinfo: { getRealModel: () => 'QN55Q80BAGXZD', getModel: () => '22_PONTUSM_QTV' } },
    tizen: { systeminfo: { getCapability: (k) => k.includes('version') ? '6.5' : 'IGNORADO' } },
    navigator: { userAgent: 'Mozilla/5.0 (SMART-TV; LINUX; Tizen 6.0)' }
  };
  ctx.window = ctx;
  vm.runInNewContext(m[1], ctx);
  assert.strictEqual(typeof ctx.window.nvFarolTvInfo, 'function');
  const g1 = ctx.window.nvFarolTvInfo();
  assert.deepStrictEqual({ modelo: g1.modelo, versao: g1.versao }, { modelo: 'QN55Q80BAGXZD', versao: '6.5' });

  // Sem webapis/tizen no escopo global -> recuo para navigator.userAgent e modelo "?"
  const ctxUa = { navigator: { userAgent: 'Mozilla/5.0 (SMART-TV; LINUX; Tizen 7.0)' } };
  ctxUa.window = ctxUa;
  vm.runInNewContext(m[1], ctxUa);
  const g2 = ctxUa.window.nvFarolTvInfo();
  assert.deepStrictEqual({ modelo: g2.modelo, versao: g2.versao }, { modelo: '?', versao: '7.0' });
}

const nvFarolTvInfo = new Function(m[1] + '; return nvFarolTvInfo;')();

// 1. Real Samsung TV via productinfo.getRealModel + systeminfo
let r = nvFarolTvInfo(
  { productinfo: { getRealModel: () => 'QN55Q80BAGXZD', getModel: () => '22_PONTUSM_QTV' } },
  { systeminfo: { getCapability: (k) => k.includes('version') ? '6.5' : 'IGNORADO' } },
  'Mozilla/5.0 (SMART-TV; LINUX; Tizen 6.0)'
);
assert.deepStrictEqual(r, { modelo: 'QN55Q80BAGXZD', versao: '6.5' });

// 2. Recuo: getRealModel falha (ou so espacos) -> getModel + versao no userAgent
r = nvFarolTvInfo(
  { productinfo: { getRealModel: () => { throw new Error('no'); }, getModel: () => '20_KANTM2_UHD' } },
  null,
  'Mozilla/5.0 (SMART-TV; LINUX; Tizen 6.0) AppleWebKit/537.36'
);
assert.deepStrictEqual(r, { modelo: '20_KANTM2_UHD', versao: '6.0' });

r = nvFarolTvInfo(
  { productinfo: { getRealModel: () => '   ', getModel: () => 'UN50TU8000GXZD' } },
  null,
  'Mozilla/5.0 (SMART-TV; LINUX; Tizen 5.5)'
);
assert.deepStrictEqual(r, { modelo: 'UN50TU8000GXZD', versao: '5.5' });

// 3. Recuo: sem productinfo -> systeminfo model_name + platform.version
r = nvFarolTvInfo(
  null,
  { systeminfo: { getCapability: (k) => k.includes('model_name') ? 'UN50TU8000' : '5.5' } },
  ''
);
assert.deepStrictEqual(r, { modelo: 'UN50TU8000', versao: '5.5' });

// 4. Tudo ausente ou lancando -> "?"
r = nvFarolTvInfo(
  { productinfo: { getRealModel: () => { throw 1; }, getModel: () => { throw 2; } } },
  { systeminfo: { getCapability: () => { throw 3; } } },
  'Desktop Chrome/120'
);
assert.deepStrictEqual(r, { modelo: '?', versao: '?' });

// 5. Maliciosos (URL, IP decimal/hex/embutido, token hex, versao ruim) -> "?"
for (const ruim of [
  'https://evil.invalid/tv',
  'http://192.168.1.1',
  '192.168.1.50',
  '127.0.1',
  '0xc6.0x33.0x64.0x2a',
  'TV_192.168.1.1',
  'user@host.com',
  '0123456789abcdef0123456789abcdef',
  'OLED..C9',
  'www.evil.com'
]) {
  r = nvFarolTvInfo(
    { productinfo: { getRealModel: () => ruim } },
    { systeminfo: { getCapability: () => '6.5;drop' } },
    ''
  );
  assert.deepStrictEqual(r, { modelo: '?', versao: '?' }, 'devia rejeitar: ' + ruim);
}
console.log('farol_perfil: casca WASM ok');
NODE
fi

echo "farol_perfil: tudo ok"
