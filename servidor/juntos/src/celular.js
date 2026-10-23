// A PAGINA DO CELULAR do Watch Together (plano 5.8, decisao 4 do dono).
//
// Uma pagina so, HTML + JS inline, sem framework, sem build e sem nada de fora
// (a CSP de index.js nao deixa carregar nada). Abre pelo QR da TV:
//   https://<nuvio-juntos>/j/<sala: 32 hex><codigo: 8>
// ENDERECO CURTO DE PROPOSITO: o qr.c das TVs codifica ate ~134 bytes, e um
// bilhete assinado nao cabe. O codigo de 8 caracteres e emitido pelo proprio
// Durable Object a pedido da TV (mensagem "celular"), vale 10 min e UMA vez;
// no primeiro uso a sala devolve uma chave de sessao, guardada no
// sessionStorage desta aba, para reconectar sem o QR.
//
// Faz: quem esta e em que estado, a posicao ao vivo (pelo relogio da sala),
// reacoes e frases rapidas (chaves, nunca texto), chat de 200 caracteres (so
// se o perfil deixa) e o controle remoto da sala respeitando a trava do
// anfitriao ("Pedir pausa" para quem nao controla).
//
// Textos: pt, en e es pelo idioma do navegador.

const TEXTOS = {
  pt: {
    conectando: "Conectando…", caiu: "Conexão perdida. Tentando de novo…",
    fim: "A sessão terminou.", invalido: "Este QR já foi usado ou venceu. Peça um novo na TV.",
    cheia: "A sessão está cheia.", com: "Sessão com", voce: "você", anfitriao: "anfitrião",
    st: { entrando: "entrando", procurando: "procurando fonte", carregando: "carregando", pronto: "pronto",
      tocando: "assistindo", pausado: "pausado", semfonte: "sem fonte", erro: "erro", ausente: "fora" },
    estado: { lobby: "Esperando começar", tocando: "Tocando", pausado: "Pausado", esperando: "Esperando alguém", fim: "Fim" },
    play: "Continuar", pausa: "Pausar", comecar: "Começar", voltar30: "−30 s", voltar10: "−10 s", avancar10: "+10 s",
    pedirPausa: "Pedir pausa", soAnf: "Só {n} controla nesta sessão.", pedido: "Pedido enviado.",
    reacoes: "Reações", chat: "Mensagem", enviar: "Enviar",
    chatOff: "O chat está desligado para este perfil.", rapido: "Calma, um de cada vez.",
    fr: { pausa_rapidinho: "Pausa rapidinho", volta_um_pouco: "Volta um pouco", legenda: "Legenda?", ja_volto: "Já volto" },
    reac: { risada: "Risada", susto: "Susto", coracao: "Amei", palmas: "Palmas", hein: "Hã?" },
    aviso: { entrou: "{n} entrou", voltou: "{n} voltou", ausente: "{n} saiu por um momento", saiu: "{n} saiu",
      anfitriao: "Agora {n} controla", esperando: "Esperando {n}", seguindo: "{n} continua carregando. Seguindo sem {n}." },
    acao: { pausa: "{n} pausou", play: "{n} continuou", seek: "{n} pulou", proximo: "{n} foi para o próximo episódio",
      comecar: "{n} começou" },
  },
  en: {
    conectando: "Connecting…", caiu: "Connection lost. Trying again…",
    fim: "The session ended.", invalido: "This QR code was already used or expired. Get a new one on the TV.",
    cheia: "The session is full.", com: "Watching with", voce: "you", anfitriao: "host",
    st: { entrando: "joining", procurando: "finding a source", carregando: "loading", pronto: "ready",
      tocando: "watching", pausado: "paused", semfonte: "no source", erro: "error", ausente: "away" },
    estado: { lobby: "Waiting to start", tocando: "Playing", pausado: "Paused", esperando: "Waiting for someone", fim: "Ended" },
    play: "Play", pausa: "Pause", comecar: "Start", voltar30: "−30 s", voltar10: "−10 s", avancar10: "+10 s",
    pedirPausa: "Ask to pause", soAnf: "Only {n} controls this session.", pedido: "Request sent.",
    reacoes: "Reactions", chat: "Message", enviar: "Send",
    chatOff: "Chat is turned off for this profile.", rapido: "Easy, one at a time.",
    fr: { pausa_rapidinho: "Quick pause", volta_um_pouco: "Go back a bit", legenda: "Subtitles?", ja_volto: "Be right back" },
    reac: { risada: "Laugh", susto: "Scared", coracao: "Love", palmas: "Clap", hein: "Huh?" },
    aviso: { entrou: "{n} joined", voltou: "{n} is back", ausente: "{n} stepped away", saiu: "{n} left",
      anfitriao: "{n} is in control now", esperando: "Waiting for {n}", seguindo: "{n} is still loading. Going on without them." },
    acao: { pausa: "{n} paused", play: "{n} resumed", seek: "{n} skipped", proximo: "{n} moved to the next episode",
      comecar: "{n} started" },
  },
  es: {
    conectando: "Conectando…", caiu: "Se perdió la conexión. Intentando de nuevo…",
    fim: "La sesión terminó.", invalido: "Este QR ya se usó o venció. Pide uno nuevo en la TV.",
    cheia: "La sesión está llena.", com: "Sesión con", voce: "tú", anfitriao: "anfitrión",
    st: { entrando: "entrando", procurando: "buscando fuente", carregando: "cargando", pronto: "listo",
      tocando: "viendo", pausado: "en pausa", semfonte: "sin fuente", erro: "error", ausente: "fuera" },
    estado: { lobby: "Esperando para empezar", tocando: "Reproduciendo", pausado: "En pausa", esperando: "Esperando a alguien", fim: "Fin" },
    play: "Seguir", pausa: "Pausar", comecar: "Empezar", voltar30: "−30 s", voltar10: "−10 s", avancar10: "+10 s",
    pedirPausa: "Pedir pausa", soAnf: "Solo {n} controla esta sesión.", pedido: "Pedido enviado.",
    reacoes: "Reacciones", chat: "Mensaje", enviar: "Enviar",
    chatOff: "El chat está apagado para este perfil.", rapido: "Calma, de a uno.",
    fr: { pausa_rapidinho: "Pausa rapidito", volta_um_pouco: "Vuelve un poco", legenda: "¿Subtítulos?", ja_volto: "Ya vuelvo" },
    reac: { risada: "Risa", susto: "Susto", coracao: "Me encanta", palmas: "Aplausos", hein: "¿Eh?" },
    aviso: { entrou: "{n} entró", voltou: "{n} volvió", ausente: "{n} salió un momento", saiu: "{n} salió",
      anfitriao: "Ahora controla {n}", esperando: "Esperando a {n}", seguindo: "{n} sigue cargando. Seguimos sin {n}." },
    acao: { pausa: "{n} pausó", play: "{n} siguió", seek: "{n} saltó", proximo: "{n} pasó al siguiente episodio",
      comecar: "{n} empezó" },
  },
};

// Icones das reacoes, no traco da Lucide; os mesmos cinco da TV. Sem emoji.
const ICONES = {
  risada: '<circle cx="12" cy="12" r="10"/><path d="M8 13s1.5 3 4 3 4-3 4-3"/><path d="M8 9.5l1.5-1 1.5 1M13 9.5l1.5-1 1.5 1"/>',
  susto: '<circle cx="12" cy="12" r="10"/><circle cx="12" cy="16" r="2"/><circle cx="9" cy="9.5" r="1"/><circle cx="15" cy="9.5" r="1"/>',
  coracao: '<path d="M19 14c1.49-1.46 3-3.21 3-5.5A5.5 5.5 0 0 0 16.5 3c-1.76 0-3 .5-4.5 2-1.5-1.5-2.74-2-4.5-2A5.5 5.5 0 0 0 2 8.5c0 2.3 1.5 4.05 3 5.5l7 7Z"/>',
  palmas: '<path d="M7 11V7a2 2 0 0 1 4 0v4"/><path d="M11 9V5a2 2 0 0 1 4 0v6"/><path d="M15 10a2 2 0 0 1 4 0v3a8 8 0 0 1-8 8h-1a7 7 0 0 1-6-3.4L2.3 15a2 2 0 0 1 3.4-2L7 15"/>',
  hein: '<circle cx="12" cy="12" r="10"/><path d="M9.1 9a3 3 0 0 1 5.8 1c0 2-3 3-3 3"/><path d="M12 17h.01"/>',
};

export function paginaCelular() {
  return `<!doctype html>
<html lang="pt">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="referrer" content="no-referrer">
<meta name="theme-color" content="#0b0d12">
<title>Watch Together</title>
<style>
:root{--fundo:#0b0d12;--vidro:rgba(255,255,255,.07);--borda:rgba(255,255,255,.12);--texto:#f3f4f7;--fraco:rgba(243,244,247,.62);
--acento:#7c9cff;--ok:#5fd39a;--alerta:#ffcf6b;--raio:22px}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;background:var(--fundo);color:var(--texto);font:16px/1.4 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
body{min-height:100dvh;padding:max(16px,env(safe-area-inset-top)) 16px max(20px,env(safe-area-inset-bottom));
background:radial-gradient(120% 60% at 50% -10%,#24305a 0%,transparent 60%),var(--fundo)}
main{max-width:520px;margin:0 auto;display:flex;flex-direction:column;gap:14px}
.cartao{background:var(--vidro);border:1px solid var(--borda);border-radius:var(--raio);padding:16px;backdrop-filter:blur(18px);-webkit-backdrop-filter:blur(18px)}
h1{font-size:13px;letter-spacing:.08em;text-transform:uppercase;color:var(--fraco);margin:0 0 8px;font-weight:600}
#nome{font-size:22px;font-weight:700;margin:0;overflow-wrap:anywhere}
#com{color:var(--fraco);margin:2px 0 0;font-size:14px}
.relogio{display:flex;align-items:baseline;justify-content:space-between;margin-top:14px}
#pos{font-variant-numeric:tabular-nums;font-size:30px;font-weight:700}
#est{font-size:14px;color:var(--fraco)}
.pessoas{list-style:none;margin:0;padding:0;display:flex;flex-direction:column;gap:10px}
.pessoas li{display:flex;align-items:center;gap:12px}
.rosto{width:36px;height:36px;border-radius:50%;flex:none;display:grid;place-items:center;font-weight:700;background:#33406e;overflow:hidden}
.rosto img{width:100%;height:100%;object-fit:cover}
.pessoas .q{flex:1;min-width:0}.pessoas .n{font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.s{font-size:13px;color:var(--fraco)}.s.pronto,.s.tocando{color:var(--ok)}.s.carregando,.s.procurando{color:var(--alerta)}
.controles{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
button{appearance:none;border:1px solid var(--borda);background:rgba(255,255,255,.08);color:var(--texto);border-radius:16px;
min-height:52px;font:600 15px/1 inherit;padding:0 10px;cursor:pointer;transition:transform .12s ease,background .12s ease}
button:active{transform:scale(.96);background:rgba(255,255,255,.16)}
#tocar,#pedir{grid-column:span 3;background:var(--acento);border-color:transparent;color:#0b0d12;font-size:17px}
.reacoes{display:flex;justify-content:space-between;gap:8px}
.reacoes button{flex:1;min-height:56px;display:grid;place-items:center}
.reacoes svg{width:26px;height:26px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.frases{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px}
.frases button{min-height:40px;font-size:14px;border-radius:999px;padding:0 14px}
#mensagens{list-style:none;margin:0 0 10px;padding:0;max-height:38dvh;overflow:auto;display:flex;flex-direction:column;gap:6px}
#mensagens li{font-size:15px;overflow-wrap:anywhere}#mensagens b{font-weight:600;margin-right:6px}#mensagens .ev{color:var(--fraco);font-size:13px}
form{display:flex;gap:8px}
input{flex:1;min-width:0;min-height:48px;border-radius:14px;border:1px solid var(--borda);background:rgba(0,0,0,.25);color:var(--texto);
padding:0 14px;font:16px inherit}
#aviso{min-height:20px;font-size:14px;color:var(--alerta);text-align:center;margin:0}
[hidden]{display:none!important}
@media (prefers-reduced-motion:reduce){button{transition:none}}
</style>
</head>
<body>
<main>
  <section class="cartao" aria-live="polite">
    <h1>Watch Together</h1>
    <p id="nome">…</p>
    <p id="com"></p>
    <div class="relogio"><span id="pos">0:00</span><span id="est"></span></div>
  </section>
  <p id="aviso" role="status"></p>
  <section class="cartao">
    <div class="controles">
      <button id="tocar" type="button"></button>
      <button id="pedir" type="button" hidden></button>
      <button data-pulo="-30" type="button"></button>
      <button data-pulo="-10" type="button"></button>
      <button data-pulo="10" type="button"></button>
    </div>
  </section>
  <section class="cartao">
    <h1 id="tReacoes"></h1>
    <div class="reacoes" id="reacoes"></div>
    <div class="frases" id="frases"></div>
  </section>
  <section class="cartao">
    <h1 id="tChat"></h1>
    <ul id="mensagens"></ul>
    <form id="form" autocomplete="off" hidden><input id="txt" maxlength="200" enterkeyhint="send"><button id="enviar" type="submit"></button></form>
    <p id="chatOff" class="s" hidden></p>
  </section>
  <section class="cartao"><ul class="pessoas" id="pessoas"></ul></section>
</main>
<script>
"use strict";
const TEXTOS = ${JSON.stringify(TEXTOS)};
const ICONES = ${JSON.stringify(ICONES)};
const lang = (navigator.language || "en").slice(0, 2);
const T = TEXTOS[lang] || TEXTOS.en;
document.documentElement.lang = TEXTOS[lang] ? lang : "en";
const $ = (id) => document.getElementById(id);
const cap = (s) => (s ? s.charAt(0).toUpperCase() + s.slice(1) : s);
const fmt = (s, n) => cap(s.replace(/\\{n\\}/g, n));
let ws = null, sala = null, eu = "", offset = 0, melhorRtt = 1e9, tentativa = 0, acabou = false;

// /j/<sala 32 hex><codigo 8>: o codigo e de uso unico; depois, a chave de sessao.
const m0 = location.pathname.match(/^\\/j\\/([0-9a-f]{32})([0-9A-Z]{8})\\/?$/);
const salaId = m0 ? m0[1] : "";
let codigo = m0 ? m0[2] : "";
const guardada = (() => { try { return JSON.parse(sessionStorage.getItem("juntos") || "null"); } catch { return null; } })();
if (guardada && guardada.s === salaId && guardada.c === codigo) codigo = "";   // recarregou a aba: usa a chave

function aviso(t) { $("aviso").textContent = t || ""; }
(function textos() {
  $("pedir").textContent = T.pedirPausa;
  document.querySelector('[data-pulo="-30"]').textContent = T.voltar30;
  document.querySelector('[data-pulo="-10"]').textContent = T.voltar10;
  document.querySelector('[data-pulo="10"]').textContent = T.avancar10;
  $("tReacoes").textContent = T.reacoes; $("tChat").textContent = T.chat; $("enviar").textContent = T.enviar;
  $("txt").placeholder = T.chat; $("chatOff").textContent = T.chatOff;
  for (const k of Object.keys(ICONES)) {
    const b = document.createElement("button");
    b.type = "button"; b.setAttribute("aria-label", T.reac[k]);
    b.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true">' + ICONES[k] + "</svg>";
    b.onclick = () => mandar({ t: "reacao", k });
    $("reacoes").appendChild(b);
  }
  for (const k of Object.keys(T.fr)) {
    const b = document.createElement("button");
    b.type = "button"; b.textContent = T.fr[k];
    b.onclick = () => mandar({ t: "frase", k });
    $("frases").appendChild(b);
  }
})();

function conectar() {
  if (acabou) return;
  let qs;
  if (salaId && codigo) qs = "s=" + salaId + "&c=" + codigo;
  else if (guardada && guardada.s === salaId && guardada.k) qs = "s=" + salaId + "&k=" + guardada.k;
  else { aviso(T.invalido); return; }
  const usado = codigo;
  codigo = "";
  aviso(tentativa ? T.caiu : T.conectando);
  ws = new WebSocket((location.protocol === "https:" ? "wss://" : "ws://") + location.host + "/ws?" + qs);
  ws.onopen = () => { tentativa = 0; aviso(""); for (let i = 0; i < 6; i++) setTimeout(ping, i * 200); };
  ws.onmessage = (e) => { let m; try { m = JSON.parse(e.data); } catch { return; } receber(m, usado); };
  ws.onclose = (e) => {
    if (acabou) return;
    if (e.code === 4401 || e.code === 4403 || e.code === 4001) { aviso(T.invalido); try { sessionStorage.removeItem("juntos"); } catch {} return; }
    if (e.code === 4409) { aviso(T.cheia); return; }
    if (e.code === 4410) { aviso(T.fim); return; }
    tentativa++;
    setTimeout(conectar, Math.min(30000, 1000 * 2 ** Math.min(tentativa, 5)));
  };
}
function mandar(m) { if (ws && ws.readyState === 1) ws.send(JSON.stringify(m)); }
function ping() { mandar({ t: "ping", c: performance.now() }); }
setInterval(ping, 30000);

function nome(id) { const p = sala && sala.membros.find((x) => x.id === id); return p ? (id === eu ? T.voce : p.nome) : ""; }
function souAnf() { const p = sala && sala.membros.find((x) => x.id === eu); return !!(p && p.anf); }
function posAlvo() {
  if (!sala) return 0;
  const r = sala.rel, agoraS = performance.now() + offset;
  return r.estado === "tocando" ? Math.max(r.pos0, r.pos0 + (agoraS - r.t0) / 1000) : r.pos0;
}
function relogio(s) {
  s = Math.max(0, Math.floor(s));
  const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), x = String(s % 60).padStart(2, "0");
  return h ? h + ":" + String(m).padStart(2, "0") + ":" + x : m + ":" + x;
}
function linha(texto, de) {
  const li = document.createElement("li");
  if (de) { const b = document.createElement("b"); b.textContent = cap(de); li.append(b, document.createTextNode(texto)); }
  else { li.className = "ev"; li.textContent = texto; }
  $("mensagens").appendChild(li);
  while ($("mensagens").children.length > 60) $("mensagens").firstChild.remove();
  $("mensagens").scrollTop = $("mensagens").scrollHeight;
}

function receber(m, usado) {
  switch (m.t) {
    case "pong": {
      const rtt = performance.now() - m.c;
      if (rtt < melhorRtt * 1.5) { offset = m.s - (m.c + rtt / 2); melhorRtt = Math.min(melhorRtt, rtt); }
      break;
    }
    case "sessao":
      eu = m.id;
      try { sessionStorage.setItem("juntos", JSON.stringify({ s: salaId, k: m.k, c: usado || (guardada && guardada.c) || "" })); } catch {}
      $("form").hidden = !m.chat; $("chatOff").hidden = !!m.chat;
      break;
    case "historico": for (const c of m.itens) linha(c.txt, nome(c.de)); break;
    case "sala": sala = m; desenhar(); break;
    case "membro":
      if (sala) { const i = sala.membros.findIndex((x) => x.id === m.id); if (i >= 0) { sala.membros[i] = { ...sala.membros[i], ...m }; desenhar(); } }
      break;
    case "chat": linha(m.txt, nome(m.de)); break;
    case "reacao": linha(nome(m.de) + " · " + (T.reac[m.k] || ""), ""); break;
    case "frase": linha(T.fr[m.k] || "", nome(m.de)); break;
    case "aviso": if (T.aviso[m.k]) linha(fmt(T.aviso[m.k], nome(m.quem) || m.nome || ""), ""); break;
    case "recusado":
      if (m.k === "so_anfitriao") aviso(fmt(T.soAnf, nome(m.anf) || T.anfitriao));
      else if (m.k === "rapido") aviso(T.rapido);
      break;
    case "fim": acabou = true; aviso(T.fim); try { sessionStorage.removeItem("juntos"); } catch {} break;
    case "erro": if (m.k === "fim") { acabou = true; aviso(T.fim); } break;
  }
}

let ultSeq = 0;
function desenhar() {
  const r = sala.rel;
  $("nome").textContent = sala.cfg.titulo || "";
  const outros = sala.membros.filter((p) => p.id !== eu).map((p) => p.nome);
  $("com").textContent = outros.length ? T.com + " " + outros.join(", ") : "";
  $("est").textContent = T.estado[r.estado] || "";
  if (ultSeq && sala.seq !== ultSeq && r.op && T.acao[r.op] && r.autor) linha(fmt(T.acao[r.op], nome(r.autor)), "");
  ultSeq = sala.seq;
  const travado = sala.controle === "anfitriao" && !souAnf();
  $("tocar").textContent = r.estado === "lobby" ? T.comecar : r.estado === "tocando" ? T.pausa : T.play;
  for (const b of document.querySelectorAll(".controles button")) b.hidden = travado !== (b.id === "pedir");
  const ul = $("pessoas");
  ul.textContent = "";
  for (const p of sala.membros) {
    const li = document.createElement("li");
    const rosto = document.createElement("span");
    rosto.className = "rosto";
    if (p.av && /^https:\\/\\//.test(p.av)) { const img = document.createElement("img"); img.alt = ""; img.referrerPolicy = "no-referrer"; img.src = p.av; rosto.appendChild(img); }
    else rosto.textContent = (p.nome || "?").slice(0, 1).toUpperCase();
    const q = document.createElement("span"); q.className = "q";
    const n = document.createElement("div"); n.className = "n"; n.textContent = cap(p.id === eu ? T.voce : p.nome) + (p.anf ? " · " + T.anfitriao : "");
    const s = document.createElement("div"); s.className = "s " + p.st; s.textContent = T.st[p.st] || p.st;
    q.append(n, s); li.append(rosto, q); ul.appendChild(li);
  }
}
setInterval(() => { if (sala) $("pos").textContent = relogio(posAlvo()); }, 250);

$("tocar").onclick = () => {
  if (!sala) return;
  const r = sala.rel;
  if (r.estado === "lobby") mandar({ t: "cmd", op: "comecar", seqBase: sala.seq });
  else mandar({ t: "cmd", op: r.estado === "tocando" ? "pausa" : "play", pos: posAlvo(), seqBase: sala.seq });
};
for (const b of document.querySelectorAll("[data-pulo]"))
  b.onclick = () => sala && mandar({ t: "cmd", op: "seek", pos: Math.max(0, posAlvo() + Number(b.dataset.pulo)), seqBase: sala.seq });
$("pedir").onclick = () => { mandar({ t: "pedir", k: "pausa" }); aviso(T.pedido); };
$("form").onsubmit = (e) => {
  e.preventDefault();
  const t = $("txt").value.trim();
  if (!t) return;
  mandar({ t: "chat", txt: t.slice(0, 200) });
  $("txt").value = "";
};
conectar();
</script>
</body>
</html>`;
}
