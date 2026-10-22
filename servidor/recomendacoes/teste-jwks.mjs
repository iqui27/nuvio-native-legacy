// Worker inteiro, SQLite real e ES256 gerado aqui; nenhuma rede real.
import assert from 'node:assert/strict';
import { DatabaseSync } from 'node:sqlite';
import { readFileSync, readdirSync } from 'node:fs';
import test from 'node:test';
const par = await crypto.subtle.generateKey({ name: 'ECDSA', namedCurve: 'P-256' }, true, ['sign', 'verify']);
const jwk = { ...await crypto.subtle.exportKey('jwk', par.publicKey), kid: 'chave-1', alg: 'ES256' };
const b64 = x => Buffer.from(typeof x === 'string' ? x : JSON.stringify(x)).toString('base64url');
async function token(payload, header = { alg: 'ES256', kid: 'chave-1' }) {
  const texto = b64(header) + '.' + b64(payload);
  const sig = await crypto.subtle.sign({ name: 'ECDSA', hash: 'SHA-256' }, par.privateKey, new TextEncoder().encode(texto));
  return texto + '.' + Buffer.from(sig).toString('base64url');
}
let modulo = 0;
async function cenario(t) {
  const { default: worker } = await import(`./src/index.js?jwks=${++modulo}`);
  const sqlite = new DatabaseSync(':memory:');
  for (const arquivo of ['schema.sql', ...readdirSync(new URL('.', import.meta.url)).filter(x => /^migracao-.*sql$/.test(x)).sort()]) {
    for (const cmd of readFileSync(new URL(arquivo, import.meta.url), 'utf8').split(/;\s*\n/)) {
      if (!cmd.trim()) continue;
      try { sqlite.exec(cmd); } catch (e) { if (!/duplicate column/.test(e.message)) throw e; }
    }
  }
  const stmt = (sql, args = []) => ({
    bind: (...a) => stmt(sql, a),
    first: async () => sqlite.prepare(sql).get(...args) ?? null,
    all: async () => ({ results: sqlite.prepare(sql).all(...args) }),
    run: async () => ({ meta: { changes: Number(sqlite.prepare(sql).run(...args).changes) } }),
  });
  const env = { DB: { prepare: stmt, batch: async a => Promise.all(a.map(s => s.run())) }, SUPABASE_URL: 'https://sb.test', SUPABASE_ANON_KEY: 'anon' };
  let agora = 1800000000;
  const originalDate = Date.now, originalFetch = globalThis.fetch;
  Date.now = () => agora * 1000;
  const rede = { jwks: 0, user: 0, status: 200, jwksStatus: 200, formatoRuim: false };
  globalThis.fetch = async (url, op) => {
    assert.equal(op.headers['user-agent'], 'nuvio-recomendacoes/1 (+https://github.com/iqui27/nuvio-native-legacy)');
    if (url === (env.NUVIO_JWKS_URL || 'https://sb.test/auth/v1/.well-known/jwks.json')) {
      rede.jwks++;
      return Response.json(rede.formatoRuim ? {} : { keys: [jwk] }, { status: rede.jwksStatus });
    }
    assert.equal(url, 'https://sb.test/auth/v1/user');
    rede.user++;
    return Response.json({ id: 'conta', user_metadata: { name: 'Nome conhecido' } }, { status: rede.status });
  };
  t.after(() => { Date.now = originalDate; globalThis.fetch = originalFetch; sqlite.close(); });
  const pede = async (tk, perfil) => worker.fetch(new Request('https://w.test/v1/eu', { method: 'POST', headers: {
    authorization: 'Bearer ' + tk, 'x-nuvio-auth': 'nuvio', ...(perfil ? { 'x-nuvio-perfil': perfil } : {}),
  } }), env);
  return { env, sqlite, rede, pede, avanca: s => { agora += s; }, payload: extra => ({ sub: 'conta', iat: agora - 10, exp: agora + 7 * 86400, ...extra }) };
}
test('50 tokens diferentes da mesma conta: uma JWKS e um user; nome e perfil', async t => {
  const c = await cenario(t);
  for (let i = 0; i < 50; i++) {
    const r = await c.pede(await token(c.payload({ jti: String(i), user_metadata: { full_name: 'Nome JWT' } })), '2');
    assert.equal(r.status, 200);
    const d = await r.json(); assert.equal(d.id, 'nuvio:conta:2'); assert.equal(d.nome, 'Nome JWT');
  }
  assert.equal(c.rede.user, 1); assert.equal(c.rede.jwks, 1);
  assert.equal(c.sqlite.prepare('SELECT SUM(assinatura) n FROM ident_hora').get().n, 50);
});
test('kid desconhecido: uma rebusca, recusa e limite persistido entre isolates', async t => {
  const c = await cenario(t);
  await c.pede(await token(c.payload()));
  for (let i = 0; i < 4; i++) assert.equal((await c.pede(await token(c.payload({ jti: i }), { alg: 'ES256', kid: 'desconhecido' }))).status, 401);
  assert.equal(c.rede.jwks, 2); assert.equal(c.rede.user, 1);
  const { default: outro } = await import(`./src/index.js?jwks=${++modulo}`);
  const tk = await token(c.payload(), { alg: 'ES256', kid: 'outra' });
  assert.equal((await outro.fetch(new Request('https://w.test/v1/eu', { method: 'POST', headers: { authorization: 'Bearer ' + tk, 'x-nuvio-auth': 'nuvio' } }), c.env)).status, 401);
  assert.equal(c.rede.jwks, 2);
});
test('HS256/none e assinatura adulterada recusados sem user e com cache negativo', async t => {
  const c = await cenario(t);
  for (const alg of ['HS256', 'none']) assert.equal((await c.pede(await token(c.payload(), { alg, kid: jwk.kid }))).status, 401);
  assert.equal(c.rede.jwks, 0); assert.equal(c.rede.user, 0);
  const bom = await token(c.payload());
  const partes = bom.split('.'); const sig = Buffer.from(partes[2], 'base64url'); sig[0] ^= 1; partes[2] = sig.toString('base64url');
  const ruim = partes.join('.');
  assert.equal((await c.pede(ruim)).status, 401);
  const chamadas = c.rede.jwks;
  assert.equal((await c.pede(ruim)).status, 401);
  assert.equal(c.rede.jwks, chamadas); assert.equal(c.rede.user, 0);
  assert.equal(c.sqlite.prepare("SELECT COUNT(*) n FROM sessao WHERE id = ''").get().n, 3);
});
test('JWKS 500/formato ruim/rede: caminho antigo e cache sessao', async t => {
  const c = await cenario(t); c.rede.jwksStatus = 500;
  const tk = await token(c.payload());
  assert.equal((await c.pede(tk)).status, 200); assert.equal((await c.pede(tk)).status, 200);
  assert.equal(c.rede.user, 1); assert.equal(c.rede.jwks, 2);
  c.rede.jwksStatus = 200; c.rede.formatoRuim = true;
  assert.equal((await c.pede(await token(c.payload({ jti: 'formato' })))).status, 200);
  const fetchAnterior = globalThis.fetch;
  globalThis.fetch = (u, op) => u.includes('jwks') ? Promise.reject(new Error('offline')) : fetchAnterior(u, op);
  assert.equal((await c.pede(await token(c.payload({ jti: 'rede' })))).status, 200);
  assert.equal(c.rede.user, 3);
});
test('6h: verifica de novo, 401 bloqueia antigos e iat novo recupera sem ressuscitar antigos', async t => {
  const c = await cenario(t);
  const antigo = await token(c.payload());
  assert.equal((await c.pede(antigo)).status, 200);
  c.avanca(21600); c.rede.status = 401;
  assert.equal((await c.pede(antigo)).status, 401);
  assert.equal((await c.pede(await token(c.payload({ iat: 1800000000 - 1 })))).status, 401);
  assert.equal(c.rede.user, 2);
  c.avanca(1); c.rede.status = 200;
  const novo = await token(c.payload({ iat: 1800021601 }));
  assert.equal((await c.pede(novo)).status, 200); assert.equal(c.rede.user, 3);
  assert.equal((await c.pede(await token(c.payload({ iat: 1800000000 - 2 })))).status, 401);
  c.avanca(21600);
  assert.equal((await c.pede(novo)).status, 200); assert.equal(c.rede.user, 4);
});
test('token vencido recusa sem rede; conta limita pedidos concorrentes', async t => {
  const c = await cenario(t);
  assert.equal((await c.pede(await token(c.payload({ exp: 1800000000 })))).status, 401);
  assert.equal(c.rede.jwks, 0); assert.equal(c.rede.user, 0);
  await c.pede(await token(c.payload()));
  c.avanca(21600);
  const tokens = await Promise.all(Array.from({ length: 10 }, (_, i) => token(c.payload({ jti: i }))));
  const respostas = await Promise.all(tokens.map(tk => c.pede(tk)));
  assert.ok(respostas.every(r => r.status === 200));
  assert.equal(c.rede.user, 2);
});
test('JWKS e nome conhecidos persistem entre isolates; URL configuravel', async t => {
  const c = await cenario(t); c.env.NUVIO_JWKS_URL = 'https://sb.test/chaves';
  assert.equal((await c.pede(await token(c.payload()))).status, 200);
  const { default: outro } = await import(`./src/index.js?jwks=${++modulo}`);
  const r = await outro.fetch(new Request('https://w.test/v1/eu', { method: 'POST', headers: { authorization: 'Bearer ' + await token(c.payload({ jti: 'outro' })), 'x-nuvio-auth': 'nuvio' } }), c.env);
  assert.equal(r.status, 200); assert.equal((await r.json()).nome, 'Nome conhecido');
  assert.equal(c.rede.jwks, 1); assert.equal(c.rede.user, 1);
});
