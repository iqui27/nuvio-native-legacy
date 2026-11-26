// POST /v1/vaitocar — registro ANONIMO de compatibilidade (Vai tocar nesta TV,
// pesquisa de mercado 4.2). Sem sessao: o registro nao pode ser ligado a uma
// conta. O corpo e UM objeto de chaves fixas; cada campo tem vocabulario
// fechado ou gramatica estreita (espelho de src/vtreg.h do app), chave a mais
// reprova e nada do corpo e guardado fora das colunas. A tabela (migracao
// 022) so tem CONTAGEM por combinacao e por dia.
//
// O IP so entra na cota, e ja como HMAC/SHA-256 truncado na tabela `limite`
// (que expira sozinha); nunca na tabela vaitocar.
//
// Nao ha rota de leitura: o veredito por fonte fica para depois de haver dados.
export const VAITOCAR_CORPO_MAX = 1024;
export const VAITOCAR_RETENCAO_DIAS = 180;
const LIMITE_PEDIDOS = 60, JANELA_SEGUNDOS = 3600;

const PLATAFORMA = ["webos", "tizen", "android", "linux", "macos", "outro"];
const CONTAINER = ["mkv", "webm", "mp4", "ts", "hls", "dash", "avi", "flv", "outro", "?"];
const VIDEO = ["hevc", "av1", "vp9", "vp8", "h264", "mpeg4", "mpeg2", "vc1", "outro", "?"];
const AUDIO = ["truehd", "eac3", "ac3", "dts", "aac", "opus", "vorbis", "flac", "mp3", "pcm", "outro", "?"];
const HDR = ["dv", "hdr10p", "hdr10", "hlg", "sdr", "outro", "?"];
const RESULTADO = ["tocou", "nao_abriu", "caiu", "prazo", "travou", "lenta", "decoder", "tela_preta", "sem_audio"];
const MBPS = [0, 2, 5, 10, 20, 40, 80, 999];
const ALTURA = [0, 480, 720, 1080, 1440, 2160, 4320];
const CHAVES = ["v", "plataforma", "modelo", "firmware", "container", "video", "audio", "hdr",
  "mbps", "altura", "resultado", "trocou"];

// Modelo e firmware: os unicos campos livres. Mesmo filtro de vtreg_texto_seguro.
export function textoSeguro(s, max) {
  if (typeof s !== "string" || !s || s.length > max) return false;
  if (s === "?") return true;
  if (!/^[A-Za-z0-9._+-]+( [A-Za-z0-9._+-]+)*$/.test(s)) return false;
  if (/[0-9a-f]{16,}/i.test(s) || /www\.|http|magnet/i.test(s)) return false;
  const ip = /^(\d+)\.(\d+)\.(\d+)\.(\d+)$/.exec(s);
  if (ip && s.length >= 7 && ip.slice(1).every((g) => Number(g) <= 255) && ip.slice(1).some((g) => Number(g) > 99))
    return false;
  return true;
}

export function validarVaiTocar(c) {
  if (!c || typeof c !== "object" || Array.isArray(c)) return null;
  const ks = Object.keys(c);
  if (ks.length !== CHAVES.length || ks.some((k) => !CHAVES.includes(k))) return null;
  if (c.v !== 1) return null;
  if (!PLATAFORMA.includes(c.plataforma) || !CONTAINER.includes(c.container) || !VIDEO.includes(c.video) ||
      !AUDIO.includes(c.audio) || !HDR.includes(c.hdr) || !RESULTADO.includes(c.resultado)) return null;
  if (!MBPS.includes(c.mbps) || !ALTURA.includes(c.altura) || (c.trocou !== 0 && c.trocou !== 1)) return null;
  if (!textoSeguro(c.modelo, 47) || !textoSeguro(c.firmware, 31)) return null;
  return [c.plataforma, c.modelo, c.firmware, c.container, c.video, c.audio, c.hdr, c.mbps, c.altura, c.resultado, c.trocou];
}

const resp = (dados, status = 200) => new Response(JSON.stringify(dados), {
  status, headers: { "content-type": "application/json; charset=utf-8", "cache-control": "no-store" },
});

async function ipCurto(ip) {
  const d = await crypto.subtle.digest("SHA-256", new TextEncoder().encode("vaitocar:" + ip));
  return [...new Uint8Array(d).slice(0, 8)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

// `cru` e o corpo ja lido com teto (corpoLimitado em index.js); null = grande demais.
export async function rotaVaiTocar(req, env, limite, cru, agoraSeg = Math.floor(Date.now() / 1000)) {
  if (cru === null || cru.length > VAITOCAR_CORPO_MAX) return resp({ erro: "corpo grande" }, 413);
  const ip = req.headers.get("cf-connecting-ip") || "0.0.0.0";
  if (limite && !(await limite(env, "vaitocar:" + (await ipCurto(ip)), LIMITE_PEDIDOS, JANELA_SEGUNDOS, agoraSeg)))
    return resp({ erro: "muitos pedidos" }, 429);
  let corpo;
  try { corpo = JSON.parse(cru); } catch { return resp({ erro: "json invalido" }, 400); }
  const v = validarVaiTocar(corpo);
  if (!v) return resp({ erro: "registro invalido" }, 400);
  await env.DB.prepare(
    "INSERT INTO vaitocar (dia, plataforma, modelo, firmware, container, video, audio, hdr, mbps, altura, resultado, trocou, n) " +
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1) " +
    "ON CONFLICT(dia, plataforma, modelo, firmware, container, video, audio, hdr, mbps, altura, resultado, trocou) " +
    "DO UPDATE SET n = n + 1"
  ).bind(Math.floor(agoraSeg / 86400), ...v).run();
  return resp({ ok: 1 });
}

export const limparVaiTocar = (env, agoraSeg) =>
  env.DB.prepare("DELETE FROM vaitocar WHERE dia < ?").bind(Math.floor(agoraSeg / 86400) - VAITOCAR_RETENCAO_DIAS).run();
