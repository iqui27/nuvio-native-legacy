// CODIGO DE 6 DIGITOS PARA ENTRAR NA TV DE OUTRA PESSOA (2.1).
//
// O login da TV ja e por codigo, mas o codigo do servidor da conta tem 32
// digitos hexadecimais: so o QR o carrega, e o QR exige alguem com celular NA
// FRENTE da TV. Quem configura a TV de um parente esta longe. Aqui a TV troca
// o endereco de autorizacao por 6 digitos que cabem numa ligacao; quem ajuda
// abre /c, digita os 6 digitos e cai na MESMA pagina de autorizacao do QR.
//
// O QUE ESTE SERVICO NAO FAZ: nao autoriza nada, nao ve conta nem token. Ele
// so guarda "codigo -> endereco" por PAREAR_VALIDADE segundos. Quem decide se
// a TV entra continua sendo a pagina de login da conta.
//
// RISCOS E FREIOS
//   - redirecionamento aberto: so entra endereco que comeca por um prefixo de
//     PAREAR_BASES (lista separada por virgula). Sem a variavel: 503.
//   - adivinhar codigo: 10 tentativas por IP a cada 10 minutos, e o codigo
//     vale 10 minutos. Acertar um codigo alheio so leva a pagina de
//     autorizacao de uma TV alheia — poria a conta DE QUEM ADIVINHOU naquela
//     TV, nunca o contrario.
//   - golpe "digite este codigo": e o mesmo risco de qualquer login por
//     codigo (QR incluso); a pagina de autorizacao da conta e quem confirma.
//   - lotar a tabela: 20 codigos por IP por hora; o mesmo endereco devolve o
//     mesmo codigo enquanto valer.
export const PAREAR_VALIDADE = 600;
const URL_MAX = 400;

const bases = (env) => String(env.PAREAR_BASES || "").split(",").map((s) => s.trim()).filter(Boolean);

// Puro: o endereco pode ser guardado? https (http so em localhost, para teste),
// sem credencial embutida, e comecando por um dos prefixos permitidos.
export function enderecoAceito(endereco, permitidos) {
  if (typeof endereco !== "string" || !endereco || endereco.length > URL_MAX) return false;
  if (/[\s<>"'\\]/.test(endereco)) return false;
  let u;
  try { u = new URL(endereco); } catch { return false; }
  if (u.username || u.password) return false;
  const local = u.hostname === "localhost" || u.hostname === "127.0.0.1";
  if (u.protocol !== "https:" && !(u.protocol === "http:" && local)) return false;
  return permitidos.some((b) => {
    if (!endereco.startsWith(b)) return false;
    // O prefixo tem de acabar numa fronteira: "https://a.com" nao libera
    // "https://a.com.golpe.net".
    const resto = endereco.slice(b.length);
    return resto === "" || /[/?#]$/.test(b) || /^[/?#]/.test(resto);
  });
}

// Puro: 6 digitos uniformes a partir de bytes aleatorios (rejeicao, sem vies).
export function codigoDe(aleatorio = (n) => crypto.getRandomValues(new Uint32Array(n))) {
  for (;;) {
    const [v] = aleatorio(1);
    if (v < 4294000000) return String(v % 1000000).padStart(6, "0");
  }
}

const soDigitos = (s) => String(s ?? "").replace(/\D/g, "");

async function rotaCriar(req, env, h, limitar, ip, origem) {
  let corpo;
  try { corpo = JSON.parse((await req.text()).slice(0, 2000) || "{}"); } catch { return h.erro("json invalido", 400); }
  const endereco = corpo?.url;
  if (!enderecoAceito(endereco, bases(env))) return h.erro("endereco recusado", 400);
  const t = h.agora();
  if (!(await limitar(env, `parear:${ip}`, 20, 3600, t))) return h.erro("limite", 429);
  const pagina = String(env.PAREAR_PAGINA || "").trim() || `${origem}/c`;
  const pronto = (codigo, expira) => h.json({ ok: 1, codigo, validade: Math.max(1, expira - t), pagina });
  const ja = await env.DB.prepare("SELECT codigo, expira FROM parear WHERE url = ? AND expira > ?")
    .bind(endereco, t + 60).first();
  if (ja) return pronto(ja.codigo, ja.expira);
  for (let i = 0; i < 8; i++) {
    const codigo = codigoDe();
    // Linha vencida com o mesmo codigo e reaproveitada; viva, tenta outro.
    const r = await env.DB.prepare(
      "INSERT INTO parear (codigo, url, expira) VALUES (?, ?, ?) " +
      "ON CONFLICT(codigo) DO UPDATE SET url = excluded.url, expira = excluded.expira " +
      "WHERE parear.expira <= ?"
    ).bind(codigo, endereco, t + PAREAR_VALIDADE, t).run();
    if (r?.meta?.changes) return pronto(codigo, t + PAREAR_VALIDADE);
  }
  return h.erro("sem codigo livre", 503);
}

async function rotaAbrir(req, env, h, limitar, ip) {
  let corpo;
  try { corpo = JSON.parse((await req.text()).slice(0, 200) || "{}"); } catch { return h.erro("json invalido", 400); }
  const t = h.agora();
  if (!(await limitar(env, `parear-abrir:${ip}`, 10, 600, t))) return h.erro("limite", 429);
  const codigo = soDigitos(corpo?.codigo);
  if (codigo.length !== 6) return h.erro("codigo invalido", 400);
  const f = await env.DB.prepare("SELECT url FROM parear WHERE codigo = ? AND expira > ?").bind(codigo, t).first();
  // O prefixo e conferido de novo: a lista pode ter encolhido desde a gravacao.
  if (!f || !enderecoAceito(f.url, bases(env))) return h.erro("codigo desconhecido", 404);
  return h.json({ ok: 1, url: f.url });
}

// null = rota nao e daqui.
export async function rotaParear(rota, req, env, url, h, limitar) {
  if (rota === "/c" || rota === "/c/") return req.method === "GET" ? paginaParear() : null;
  if (!rota.startsWith("/v1/parear") || req.method !== "POST") return null;
  if (rota !== "/v1/parear" && rota !== "/v1/parear/abrir") return null;
  if (!bases(env).length) return h.erro("parear desligado", 503);
  const ip = req.headers.get("cf-connecting-ip") || "0.0.0.0";
  try {
    return rota === "/v1/parear" ? await rotaCriar(req, env, h, limitar, ip, url.origin)
                                 : await rotaAbrir(req, env, h, limitar, ip);
  } catch {
    // Migracao 017 ainda nao aplicada: a TV esconde o codigo, como no 503 acima.
    return h.erro("parear indisponivel", 503);
  }
}

export const limpezaParear = (env, t) => [env.DB.prepare("DELETE FROM parear WHERE expira < ?").bind(t)];

// A pagina de quem ajuda. Um campo, um botao. Sem biblioteca, sem rastreio.
const PAGINA = `<!doctype html><html lang="pt-BR"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="robots" content="noindex"><title>Nuvio</title>
<style>
:root{color-scheme:dark}*{box-sizing:border-box}
body{margin:0;min-height:100vh;display:grid;place-items:center;background:#0b0c10;color:#f2f3f7;
font:17px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;padding:24px}
main{width:100%;max-width:420px;text-align:center}
h1{font-size:26px;margin:0 0 8px}p{color:#a9acb8;margin:0 0 24px}
input{width:100%;font:600 40px/1 ui-monospace,Menlo,Consolas,monospace;letter-spacing:.2em;text-align:center;
padding:18px 8px;border-radius:16px;border:2px solid #2a2d38;background:#14161d;color:#fff;outline:0}
input:focus{border-color:#8ab4ff}
button{width:100%;margin-top:16px;padding:16px;border:0;border-radius:999px;background:#f2f3f7;color:#0b0c10;
font:600 18px system-ui,sans-serif}button:disabled{opacity:.5}
#e{min-height:1.5em;margin-top:16px;color:#ec6c6c}small{display:block;margin-top:28px;color:#6f7280}
</style></head><body><main>
<h1 id="t"></h1><p id="s"></p>
<form id="f"><input id="c" inputmode="numeric" autocomplete="one-time-code" maxlength="7" placeholder="000 000" autofocus>
<button id="b"></button></form><div id="e" role="alert"></div><small id="n"></small>
</main><script>
const pt=/^pt/i.test(navigator.language||"");
const T=pt?{t:"Entrar em uma TV",s:"Digite os 6 números que aparecem na tela da TV.",b:"Continuar",
n:"Você vai para a página da conta Nuvio para autorizar a TV. Só digite um código de alguém que você conhece.",
x:"Código não encontrado ou vencido. Peça para conferir os números na TV.",l:"Muitas tentativas. Espere alguns minutos.",
r:"Sem resposta do servidor. Tente de novo."}:
{t:"Sign in on a TV",s:"Type the 6 digits shown on the TV screen.",b:"Continue",
n:"You will go to the Nuvio account page to authorize the TV. Only type a code from someone you know.",
x:"Code not found or expired. Ask them to check the digits on the TV.",l:"Too many attempts. Wait a few minutes.",
r:"No answer from the server. Try again."};
const $=(i)=>document.getElementById(i);
$("t").textContent=T.t;$("s").textContent=T.s;$("b").textContent=T.b;$("n").textContent=T.n;
$("c").addEventListener("input",()=>{const d=$("c").value.replace(/\\D/g,"").slice(0,6);
$("c").value=d.length>3?d.slice(0,3)+" "+d.slice(3):d;$("e").textContent=""});
$("f").addEventListener("submit",async(ev)=>{ev.preventDefault();
const d=$("c").value.replace(/\\D/g,"");if(d.length!==6){$("e").textContent=T.x;return}
$("b").disabled=true;
try{const r=await fetch("/v1/parear/abrir",{method:"POST",body:JSON.stringify({codigo:d})});
if(r.ok){const j=await r.json();if(j&&typeof j.url==="string"&&/^https?:\\/\\//.test(j.url)){location.href=j.url;return}}
$("e").textContent=r.status===429?T.l:r.status>=500?T.r:T.x}catch{$("e").textContent=T.r}
$("b").disabled=false});
</script></body></html>`;

export const paginaParear = () => new Response(PAGINA, {
  headers: {
    "content-type": "text/html; charset=utf-8", "cache-control": "public, max-age=3600",
    "referrer-policy": "no-referrer", "x-content-type-options": "nosniff",
    "content-security-policy": "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; " +
      "connect-src 'self'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'",
  },
});
