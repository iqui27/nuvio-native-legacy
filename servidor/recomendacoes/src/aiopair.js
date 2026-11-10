// Phone pairing extends the app's HTTPS QR/session flow. Credentials live only
// in a five-minute Durable Object inbox, never storage, URLs, logs or analytics.
// The phone calls its own AIOStreams origin; the TV independently validates it.
export const PAIR_TTL = 300000;
const keyOK = s => typeof s === 'string' && /^[a-f0-9-]{36}$/.test(s);
const headers = { 'cache-control': 'no-store', 'referrer-policy': 'no-referrer',
  'x-content-type-options': 'nosniff', 'access-control-allow-origin': '*',
  'content-type': 'application/json' };
const reply = (body, status = 200) => new Response(JSON.stringify(body), { status, headers });
export function validOrigin(s) {
  try {
    const u = new URL(s);
    return u.protocol === 'https:' && !u.username && !u.password && u.origin === s && s.length < 256;
  } catch { return false; }
}
export class AioPair {
  constructor(ctx) { this.ctx = ctx; this.session = null; }
  async alarm() { this.session = null; }
  async fetch(req) {
    if (req.method !== 'POST') return reply({ error: 'Use POST' }, 405);
    const raw = await req.text();
    if (raw.length > 4096) return reply({ error: 'Too large' }, 413);
    let b;
    try { b = JSON.parse(raw); } catch { return reply({ error: 'Invalid request' }, 400); }
    if (!b || typeof b !== 'object' || Array.isArray(b)) return reply({ error: 'Invalid request' }, 400);
    if (b.op === 'create') {
      if (this.session || !validOrigin(b.origin) || !keyOK(b.tv) || !/^[a-f0-9]{32}$/.test(b.phone))
        return reply({ error: 'Invalid session' }, 400);
      this.session = { origin: b.origin, tv: b.tv, phone: b.phone, expires: Date.now() + PAIR_TTL,
        state: 'waiting', pending: null, attempts: 0 };
      await this.ctx.storage.setAlarm(this.session.expires);
      return reply({ state: 'waiting' });
    }
    const s = this.session;
    if (!s || Date.now() >= s.expires) { this.session = null; return reply({ error: 'Session expired' }, 410); }
    const tv = b.key === s.tv, phone = b.key === s.phone;
    if (!tv && !phone) return reply({ error: 'Invalid session' }, 403);
    if (b.op === 'status') return reply({ state: s.state, origin: s.origin, reason: s.reason });
    if (b.op === 'send' && phone) {
      if (s.pending || s.state === 'checking' || s.state === 'synced') return reply({ error: 'Already sent' }, 409);
      if (++s.attempts > 5) return reply({ error: 'Too many attempts' }, 429);
      if (b.origin !== s.origin || typeof b.uuid !== 'string' || !/^[a-zA-Z0-9_-]{1,127}$/.test(b.uuid) ||
          typeof b.password !== 'string' || !b.password.length || new TextEncoder().encode(b.password).length > 768 || /[\x00-\x1f\x7f]/.test(b.password))
        return reply({ error: 'Invalid credentials' }, 400);
      s.pending = { origin: s.origin, uuid: b.uuid, password: b.password };
      s.state = 'checking'; s.reason = undefined;
      return reply({ state: s.state });
    }
    if (b.op === 'take' && tv) {
      // Retain until confirmation: a lost HTTP response must not lose delivery.
      const credentials = s.pending;
      return reply(credentials ? { state: s.state, ...credentials } : { state: s.state });
    }
    if (b.op === 'result' && tv && ['synced', 'error'].includes(b.state)) {
      if (s.state !== 'checking' && s.state !== b.state) return reply({ error: 'Invalid state' }, 409);
      s.pending = null; s.state = b.state;
      s.reason = ['auth', 'origin', 'network', 'server', 'response'].includes(b.reason) ? b.reason : undefined;
      return reply({ state: s.state });
    }
    if (b.op === 'close' && tv) { this.session = null; return reply({ state: 'closed' }); }
    return reply({ error: 'Invalid request' }, 400);
  }
}
export async function routeAioPair(req, env) {
  const u = new URL(req.url);
  if (u.pathname === '/aio/' && req.method === 'GET') return new Response(phonePage(), {
    headers: { ...headers, 'content-type': 'text/html; charset=utf-8',
      'content-security-policy': "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self' https:; form-action 'none'; base-uri 'none'; frame-ancestors 'none'" }
  });
  const m = u.pathname.match(/^\/v1\/aiopair\/([a-f0-9]{8})$/);
  if (!m || u.search || !env.AIOPAIR) return reply({ error: 'Pairing unavailable' }, 503);
  return env.AIOPAIR.get(env.AIOPAIR.idFromName(m[1])).fetch(req);
}
export function phonePage() {
  return `<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="referrer" content="no-referrer"><title>AIOStreams · Nuvio</title>
<style>*{box-sizing:border-box}body{margin:0;background:#0d0d10;color:#f3f2ef;font:18px system-ui}main{max-width:540px;margin:40px auto;padding:32px;background:#15161a;border-radius:28px}h1{font-size:32px}p{line-height:1.5;color:#c1c1c7;overflow-wrap:anywhere}label{display:block;margin:24px 0 10px}input,button{width:100%;font:inherit;padding:16px;border-radius:16px;border:1px solid #34363e;background:#24262c;color:#f3f2ef}button{margin-top:28px;background:#f3f2ef;color:#121316;border-radius:40px;font-weight:700}#status{min-height:54px}a{color:inherit}</style>
<main><p>NUVIO / AIOSTREAMS</p><h1>Connect your instance</h1><p id="origin"></p>
<p>Sign in with your AIOStreams UUID and password. Your TV syncs automatically.</p>
<form id="form"><label for="uuid">UUID</label><input id="uuid" required maxlength="127" autocomplete="username" autocapitalize="off" spellcheck="false">
<label for="password">Password</label><input id="password" type="password" required maxlength="768" autocomplete="current-password">
<button>Connect and sync</button></form><p id="status" role="status" aria-live="polite">Connecting to TV…</p></main>
<script>
const key=location.hash.slice(1),id=key.slice(0,8);history.replaceState(null,'',location.pathname);
const form=document.querySelector('#form'),status=document.querySelector('#status'),button=form.querySelector('button');let origin='',timer,lastState,submitting=false;
button.disabled=true;
const errors={auth:'Instance rejected access. Check UUID and password.',origin:'Instance origin changed. Open a new QR on TV.',network:'Could not reach your instance. Check network, TLS and browser CORS access for Authorization.',server:'Instance unavailable or rate limited. Try again later.',response:'Instance returned an invalid user response.'};
async function api(op,extra={}){if(!/^[a-f0-9]{32}$/.test(key)){const e=Error('Invalid pairing link. Open a new QR on TV.');e.terminal=true;throw e;}let r;try{r=await fetch('/v1/aiopair/'+id,{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify({op,key,...extra}),cache:'no-store',redirect:'error',credentials:'omit',referrerPolicy:'no-referrer',signal:AbortSignal.timeout(12000)});}catch{throw Error('Pairing service unavailable. Try again.');}if(!r.ok){const e=Error(r.status===410?'Pairing expired or cancelled. Open a new QR on TV.':'Pairing service unavailable. Try again or open a new QR on TV.');e.terminal=r.status===410||r.status===403;throw e;}try{return await r.json();}catch{throw Error('Pairing service returned an invalid response. Open a new QR on TV.');}}
async function poll(){try{const s=await api('status');let u;try{u=new URL(s.origin);}catch{}if(!u||u.protocol!=='https:'||u.username||u.password||u.origin!==s.origin||(origin&&origin!==s.origin)){const e=Error(errors.origin);e.terminal=true;throw e;}
origin=s.origin;document.querySelector('#origin').textContent=origin;
const stopped=s.state==='error'&&(s.reason==='network'||s.reason==='server');
if(!submitting&&(s.state!==lastState||button.disabled))status.textContent=s.state==='synced'?'Synced. Your TV is ready.':stopped?'TV could not reach or verify your instance. Check instance availability, then open a new QR on TV.':s.state==='error'?(errors[s.reason]||'TV could not verify access. Check UUID and password, then retry.'):s.state==='checking'?'TV is checking your instance…':'Ready to connect.';lastState=s.state;
form.hidden=s.state==='synced'||stopped;button.disabled=submitting||s.state==='checking'||form.hidden;if(s.state!=='synced')timer=setTimeout(poll,1500);}catch(e){status.textContent=e.terminal?e.message:'Pairing service unavailable. Retrying…';button.disabled=true;if(e.terminal)form.hidden=true;else timer=setTimeout(poll,1500);}}
form.onsubmit=async e=>{e.preventDefault();if(!origin||button.disabled||submitting)return;const uuid=document.querySelector('#uuid').value,password=document.querySelector('#password').value,instance=origin;
if(!/^[a-zA-Z0-9_-]{1,127}$/.test(uuid)||!password.length||new TextEncoder().encode(password).length>768||/[\\x00-\\x1f\\x7f]/.test(password)){status.textContent=errors.auth;return;}
submitting=true;button.disabled=true;status.textContent='Checking your instance…';let sent=false;
try{let r;try{r=await fetch(instance+'/api/v1/user',{headers:{Authorization:'Basic '+btoa(unescape(encodeURIComponent(uuid+':'+password)))},redirect:'error',cache:'no-store',credentials:'omit',referrerPolicy:'no-referrer',signal:AbortSignal.timeout(12000)});}catch{throw Error(errors.network);}
if(!r.ok)throw Error(r.status===401||r.status===403?errors.auth:r.status===429||r.status>=500?errors.server:errors.response);
let data;try{data=await r.json();}catch{throw Error(errors.response);}const user=data?.data?.userData;if(data?.success!==true||!user||typeof user!=='object'||Array.isArray(user))throw Error(errors.response);
if(form.hidden||origin!==instance)throw Error(errors.origin);
await api('send',{origin:instance,uuid,password});sent=true;lastState='checking';status.textContent='TV is syncing…';}
catch(e){status.textContent=e.message;if(e.terminal)form.hidden=true;}finally{document.querySelector('#password').value='';submitting=false;button.disabled=sent||form.hidden;}};
poll();
</script></html>`;
}
