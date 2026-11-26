// POST /v1/vaitocar — registro ANONIMO de compatibilidade (Vai tocar nesta TV,
// pesquisa de mercado 4.2). Sem sessao: o registro nao pode ser ligado a uma
// conta. O corpo e UM objeto de chaves fixas; cada campo tem vocabulario
// fechado ou gramatica estreita (espelho de src/vtreg.h do app), chave a mais
// reprova e nada do corpo e guardado fora das colunas. A tabela (migracao
// 022) so tem CONTAGEM por combinacao e por dia.
//
// O IP NUNCA vai para o D1, nem em hash (P1 da auditoria de #856: SHA-256
// sem segredo de IPv4 e reversivel por forca bruta). A cota por IP vive so
// na memoria do isolate (Map por janela horaria, some com ele); no D1 fica so
// um teto GLOBAL por hora, numa chave fixa sem nada derivado do IP.
//
// Nao ha rota de leitura: o veredito por fonte fica para depois de haver dados.
export const VAITOCAR_CORPO_MAX = 1024;
export const VAITOCAR_RETENCAO_DIAS = 180;
const LIMITE_PEDIDOS = 60, JANELA_SEGUNDOS = 3600;
export const VAITOCAR_GLOBAL_HORA = 20000;
const COTA_MAX_IPS = 20000;   // teto de memoria do Map; cheio, a janela recusa IP novo
export const VAITOCAR_MODELO_MAX = 32, VAITOCAR_FIRMWARE_MAX = 15;

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

// Modelo e firmware: os unicos campos livres, com gramatica ESTRITA (espelho
// de vtreg_modelo_tecnico / vtreg_firmware_tecnico em src/vtreg.c). Fora da
// gramatica o valor vira "?" — o registro tecnico continua valendo, mas nada
// de texto livre entra no agregado de 180 dias.
//   modelo: UM token curto com letra E digito ("OLED55C9PSA", "TCL-55C735"),
//           sem 7+ digitos seguidos, sem hash, sem cara de endereco.
//   firmware: so versao numerica, 1 a 4 grupos de 1 a 3 digitos ("05.30.25"),
//           e nunca quatro grupos com cara de IPv4.
export function modeloTecnico(s) {
  if (typeof s !== "string" || s.length > VAITOCAR_MODELO_MAX) return "?";
  if (!/^[A-Za-z0-9][A-Za-z0-9._+-]*$/.test(s)) return "?";
  if (!/[A-Za-z]/.test(s) || !/[0-9]/.test(s) || /[0-9]{7,}/.test(s)) return "?";
  if (/[0-9a-f]{16,}/i.test(s) || /www\.|http|magnet/i.test(s)) return "?";
  return s;
}

export function firmwareTecnico(s) {
  if (typeof s !== "string" || s.length > VAITOCAR_FIRMWARE_MAX) return "?";
  const m = /^(\d{1,3})(?:\.(\d{1,3}))?(?:\.(\d{1,3}))?(?:\.(\d{1,3}))?$/.exec(s);
  if (!m) return "?";
  if (m[4] !== undefined && m.slice(1).some((g) => Number(g) > 99)) return "?";
  return s;
}

export function validarVaiTocar(c) {
  if (!c || typeof c !== "object" || Array.isArray(c)) return null;
  const ks = Object.keys(c);
  if (ks.length !== CHAVES.length || ks.some((k) => !CHAVES.includes(k))) return null;
  if (c.v !== 1) return null;
  if (!PLATAFORMA.includes(c.plataforma) || !CONTAINER.includes(c.container) || !VIDEO.includes(c.video) ||
      !AUDIO.includes(c.audio) || !HDR.includes(c.hdr) || !RESULTADO.includes(c.resultado)) return null;
  if (!MBPS.includes(c.mbps) || !ALTURA.includes(c.altura) || (c.trocou !== 0 && c.trocou !== 1)) return null;
  if (typeof c.modelo !== "string" || typeof c.firmware !== "string" ||
      c.modelo.length > 64 || c.firmware.length > 64) return null;
  return [c.plataforma, modeloTecnico(c.modelo), firmwareTecnico(c.firmware), c.container, c.video, c.audio, c.hdr, c.mbps, c.altura, c.resultado, c.trocou];
}

const resp = (dados, status = 200) => new Response(JSON.stringify(dados), {
  status, headers: { "content-type": "application/json; charset=utf-8", "cache-control": "no-store" },
});

// Cota por IP SO EM MEMORIA: janela horaria -> Map(ip -> pedidos). Trocou a
// janela, o Map anterior some inteiro. Por isolate (o Cloudflare tem varios),
// entao e um freio, nao uma garantia; a garantia e o teto global no D1.
let cotaJanela = -1, cotaIps = new Map();
function cotaMemoria(ip, agoraSeg) {
  const j = Math.floor(agoraSeg / JANELA_SEGUNDOS);
  if (j !== cotaJanela) { cotaJanela = j; cotaIps = new Map(); }
  const n = cotaIps.get(ip) || 0;
  if (!n && cotaIps.size >= COTA_MAX_IPS) return false;
  if (n >= LIMITE_PEDIDOS) return false;
  cotaIps.set(ip, n + 1);
  return true;
}
// Para os testes: zera a cota em memoria.
export function vaitocarZerarCota() { cotaJanela = -1; cotaIps = new Map(); }

// `cru` e o corpo ja lido com teto (corpoLimitado em index.js); null = grande demais.
export async function rotaVaiTocar(req, env, limite, cru, agoraSeg = Math.floor(Date.now() / 1000)) {
  if (cru === null || cru.length > VAITOCAR_CORPO_MAX) return resp({ erro: "corpo grande" }, 413);
  const ip = req.headers.get("cf-connecting-ip") || "0.0.0.0";
  if (!cotaMemoria(ip, agoraSeg)) return resp({ erro: "muitos pedidos" }, 429);
  // Chave FIXA: nada do IP (nem hash) entra na tabela `limite`.
  if (limite && !(await limite(env, "vaitocar:global", VAITOCAR_GLOBAL_HORA, JANELA_SEGUNDOS, agoraSeg)))
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
