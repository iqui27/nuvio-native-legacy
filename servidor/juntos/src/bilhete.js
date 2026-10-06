// BILHETE DA SALA (Watch Together). Quem emite e o nuvio-recomendacoes, que
// conhece a pessoa (quemE, amigos, bloqueios); quem confere e o nuvio-juntos,
// que nao tem banco nenhum. Os dois so dividem o segredo SALA_SEGREDO.
//
// POR QUE BILHETE E NAO CABECALHO: o cliente websocket da TV (discordws.c) nao
// manda cabecalho proprio, e o WebSocket do navegador (.wgt, celular) tambem
// nao. Entao a identidade vai na URL, assinada e curta.
//
// Formato: base64url(JSON) "." base64url(HMAC-SHA256(segredo, parte1)).
// Campos: v, sala, pessoa, nome, av, papel (anfitriao|convidado), cfg? (so o
// do anfitriao), pend? (entrada por codigo esperando aceite), inf? (perfil
// infantil), exp (s), n (nonce).
//
// Este arquivo e importado pelos DOIS Workers (servidor/recomendacoes importa
// daqui): um formato so, sem copia que diverge em silencio.

export const BILHETE_V = 1;
const enc = new TextEncoder();

function b64url(bytes) {
  let s = "";
  for (const b of bytes) s += String.fromCharCode(b);
  return btoa(s).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
}
function deB64url(s) {
  const t = s.replace(/-/g, "+").replace(/_/g, "/");
  const bin = atob(t + "===".slice((t.length + 3) % 4));
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
  return out;
}

async function chave(segredo) {
  return crypto.subtle.importKey("raw", enc.encode(segredo), { name: "HMAC", hash: "SHA-256" }, false,
    ["sign", "verify"]);
}

export function nonce() {
  return b64url(crypto.getRandomValues(new Uint8Array(12)));
}

export async function assinar(segredo, carga) {
  if (!segredo) throw new Error("sem segredo");
  const corpo = b64url(enc.encode(JSON.stringify({ v: BILHETE_V, ...carga })));
  const sig = new Uint8Array(await crypto.subtle.sign("HMAC", await chave(segredo), enc.encode(corpo)));
  return corpo + "." + b64url(sig);
}

// A carga se a assinatura bate com um dos segredos e nao venceu; senao null.
// `segredos`: o atual e (opcional) o anterior, para trocar sem derrubar ninguem.
export async function conferir(segredos, bilhete, agoraS) {
  if (typeof bilhete !== "string" || bilhete.length > 4096) return null;
  const ponto = bilhete.indexOf(".");
  if (ponto < 1) return null;
  const corpo = bilhete.slice(0, ponto);
  let sig;
  try { sig = deB64url(bilhete.slice(ponto + 1)); } catch { return null; }
  let ok = false;
  for (const s of segredos) {
    if (!s) continue;
    if (await crypto.subtle.verify("HMAC", await chave(s), sig, enc.encode(corpo))) { ok = true; break; }
  }
  if (!ok) return null;
  let c;
  try { c = JSON.parse(new TextDecoder().decode(deB64url(corpo))); } catch { return null; }
  if (!c || typeof c !== "object") return null;
  // O campo `v` existe para trocar o formato sem derrubar TVs: aceita o atual
  // e o anterior quando houver um.
  if (c.v !== BILHETE_V) return null;
  if (typeof c.sala !== "string" || !/^[0-9a-f]{32}$/.test(c.sala)) return null;
  if (typeof c.pessoa !== "string" || !c.pessoa) return null;
  // O celular nao usa bilhete: entra pelo codigo curto do QR (logica.js).
  if (!["anfitriao", "convidado"].includes(c.papel)) return null;
  if (!Number.isFinite(c.exp) || c.exp < agoraS) return null;
  return c;
}
