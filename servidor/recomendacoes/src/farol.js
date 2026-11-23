// FAROL — RECEPCAO DO REGISTRO ESTRUTURADO (PLANO.md secao 2.7, issue 5).
// O pacote `FarolRegistro v1` e MONTADO na TV por lista branca (farolpacote.c),
// ANINHADO como na secao 2.6 do PLANO e declarado pelo redator da issue 4
// (src/farolredator.c). Aqui a defesa e a MESMA do emissor: cada campo de
// texto tem gramatica fechada — a do redator, nao um alfabeto generico — e o
// que nao casa e texto livre, que e o que o pacote nao pode ter (PLANO 2.5).
// Chave fora do esquema, enum fora da lista ou valor fora da gramatica e 422
// e nada e gravado. O servidor nao sabe quem mandou: `pessoa_h` e
// HMAC(FAROL_SAL, conta) so para os tetos de abuso, nunca o id cru
// (DECISOES.md item 4).
//
// O codigo de 6 caracteres do recibo e a MESMA bijecao de `registro`
// (codigo.js); para os dois espacos nao colidirem os ids do Farol comecam em
// 2^29 (ver migracao-018-farol.sql e codigo-registro.mjs).
//
// Issue 13: match de correcao conhecida (farol_conhecido) na resposta do POST
// e em GET /v1/farol/conhecido, mais a pagina publica GET /v1/farol/r/<codigo>.

import { codigoRegistro as codificarId, decodificarCodigo, FAROL_ID_MIN } from "./codigo.js";

export const FAROL_CORPO_MAX = 64 * 1024;
export const FAROL_RETENCAO = 30 * 24 * 3600;
export const ESTADOS_CONHECIDO = new Set(["corrigido", "conhecido", "contorno"]);
const FAROL_HORA = 6;            // envios por CONTA por hora
const FAROL_DIA = 20;            // envios por CONTA por dia
const FAROL_DIA_GLOBAL = 500;    // envios no total por dia
const FAROL_DEDUP_JANELA = 24 * 3600;
const LINHA_MAX = 256;
const LISTA_MAX = 64;

// --- gramaticas do redator (src/farolredator.c, src/farolnativo.h @ farol-4) --
//
// Um "numero" de IPv4 para o parser WHATWG: "0x"/"0X" abre hexadecimal ("0x"
// sozinho vale 0), "0" seguido de digitos abre octal, o resto e decimal. Zeros
// a esquerda sao livres; valor acima de 32 bits satura, mas um digito invalido
// invalida o pedaco inteiro. null = nao e numero.
function ipv4Numero(p) {
  if (!p.length) return null;
  let base = 10, i = 0;
  if (p[0] === "0" && p.length > 1) {
    if (p[1] === "x" || p[1] === "X") { base = 16; i = 2; }
    else { base = 8; i = 1; }
  }
  let v = 0;
  for (; i < p.length; i++) {
    const c = p[i];
    let d = -1;
    if (c >= "0" && c <= "9") d = c.charCodeAt(0) - 48;
    else if (base === 16 && c >= "a" && c <= "f") d = c.charCodeAt(0) - 87;
    else if (base === 16 && c >= "A" && c <= "F") d = c.charCodeAt(0) - 55;
    if (d < 0 || d >= base) return null;
    if (v <= 0xffffffff) { v = v * base + d; if (v > 0xffffffff) v = 0x100000000; }
  }
  return v;
}

// 1 quando s nao e um dominio valido para um parser de URL WHATWG: ou o parser
// le como IPv4 — 1-4 partes decimais/octais/hex, a ultima expandindo para os
// octetos que faltam ("127.0.1" = 127.0.0.1, "0xc633642a" = 198.51.100.42,
// "0300.0063.052" = 192.0.2.42, "3325256746" = 198.51.100.42) — ou a ultima
// parte e um "numero" e o parse falha, o que o WHATWG marca como host
// invalido, nao dominio. Nas duas saidas o trecho nao sai, nem com hash.
function ipv4Whatwg(s) {
  let n = s.length;
  if (!n) return false;
  if (s[n - 1] === ".") n--;                       // ponto final nao conta
  if (!n) return false;
  const str = s.slice(0, n);
  const ultimo = str.slice(str.lastIndexOf(".") + 1);
  // "Termina em numero": so a ultima parte decide se o WHATWG tenta IPv4.
  if (!ultimo.length || (ipv4Numero(ultimo) === null && !/^\d+$/.test(ultimo)))
    return false;
  const partes = str.split(".");
  if (partes.length > 4) return true;
  for (let i = 0; i < partes.length; i++) {
    const v = ipv4Numero(partes[i]);
    if (!partes[i].length || v === null) return true;
    if (i < partes.length - 1 && v > 255) return true;
  }
  return true;
}

// "2.1.0-ipk", "4.9-webos": parte numerica (1-3 partes de ate 4 digitos) mais
// rotulo opcional "-tag" da lista fechada de origens — um sufixo livre
// deixaria passar PIN ("4.9-7362194") e titulo ("2.1.0-segreditulo"). A
// numerica pura que um parser de URL le como IPv4 ("4.9", "127.0.1") tambem e
// recusada: o coletor sempre manda a versao com o rotulo.
const VER_TAGS = new Set(
  "ipk webos tizen wgt tpk tpk40 tpk60 tpk65 apk android dev debug".split(" "));
function versaoOk(v) {
  if (typeof v !== "string" || !v.length || v.length > 23) return false;
  const h = v.indexOf("-");
  const num = h < 0 ? v : v.slice(0, h);
  if (h >= 0 && !VER_TAGS.has(v.slice(h + 1))) return false;
  if (!num.length || num.length >= 24) return false;
  if (!num.includes(".") && num.length > 2) return false;
  const partes = num.split(".");
  if (partes.length > 3) return false;
  for (const p of partes) if (!/^\d{1,4}$/.test(p)) return false;
  return !ipv4Whatwg(v);
}

// aparelho.modelo: "x" ou <prefixo do fabricante><2 digitos de tela><serie>
// <numero><sufixo>, nos tetos do redator (OLED admite 1/2/3; os demais 2/4/6).
const MODELO_PREFIXOS = ["OLED", "QN", "QE", "UE", "UN", "UA", "KD-", "XR-", "TCL"];
// exportada para o contrato dirigido tests/farol_modelo_contrato.mjs: o
// cliente so pode emitir o que ESTA funcao aceita — o resto vira "x".
export function modeloOk(v) {
  if (v === "x") return true;
  if (typeof v !== "string") return false;
  for (let i = 0; i < MODELO_PREFIXOS.length; i++) {
    if (!v.startsWith(MODELO_PREFIXOS[i])) continue;
    let p = v.slice(MODELO_PREFIXOS[i].length);
    const tela = /^\d+/.exec(p);
    if (!tela || tela[0].length !== 2) return false;
    p = p.slice(tela[0].length);
    const serie = /^[A-Z]+/.exec(p);
    if (!serie || serie[0].length > (i ? 2 : 1)) return false;
    p = p.slice(serie[0].length);
    const numero = /^\d+/.exec(p);
    if (!numero || numero[0].length > (i ? 4 : 2)) return false;
    p = p.slice(numero[0].length);
    const sufixo = /^[A-Z]+/.exec(p);
    return !p.slice(sufixo ? sufixo[0].length : 0).length &&
           (sufixo ? sufixo[0].length : 0) <= (i ? 6 : 3);
  }
  return false;
}

// Codigos nativos fechados (farolerro.c + farolnativo.h): os 28 fixos do
// classificador, ums_<100-300|40000-49999>, m3_<2000-2008 exceto 2003,2007 |
// 3000-4999> e av_<um dos 11 nomes do enum AVPlayError da Samsung>.
const CODIGOS_NATIVOS = new Set([
  "ums_pipeline", "ums_faixa_audio", "ls2_permissao",
  "m3_parado", "m3_audio", "av_conexao",
  "tpk_conexao", "tpk_audio", "tpk_conflito", "tpk_preso",
  "ums_rede", "m3_rede", "gen_rede",
  "ums_http", "av_http", "tpk_http", "m3_http", "gen_http",
  "ums_formato", "av_formato", "tpk_formato", "m3_formato", "gen_formato",
  "ums_outro", "av_outro", "tpk_outro", "m3_outro", "gen_outro",
]);
const AVPLAY_NOMES = new Set([
  "none", "invalid_parameter", "no_such_file", "invalid_operation",
  "seek_failed", "invalid_state", "not_supported_file", "not_supported_format",
  "invalid_uri", "connection_failed", "genereic",
]);
const nativoNumerico = (s, prefixo, faixa) => {
  const d = s.slice(prefixo.length);
  if (!d.length || d.length > 5 || !/^\d+$/.test(d)) return false;
  return faixa(Number(d));
};
function codNativoOk(s) {
  if (s.length < 4 || s.length > 23) return false;
  if (CODIGOS_NATIVOS.has(s)) return true;
  if (s.startsWith("ums_"))
    return nativoNumerico(s, "ums_", (v) => (v >= 100 && v <= 300) || (v >= 40000 && v <= 49999));
  if (s.startsWith("m3_"))
    return nativoNumerico(s, "m3_", (v) =>
      (v >= 2000 && v <= 2008 && v !== 2003 && v !== 2007) || (v >= 3000 && v <= 4999));
  if (s.startsWith("av_")) return AVPLAY_NOMES.has(s.slice(3));
  return false;
}

// token_seguro: o que erro.nativo e linha[].k podem trazer — a lista de
// rotulos, "h:" + 8 hex (pseudonimo com sal), codigo nativo ou o token de
// fonte "<res>_<sdr|hdr>_<dv|nodv>_<ac3|eac3|dts|outro>" que o produtor monta
// de enums (farol-10). Tudo fora disso e texto livre: recusado. A lista e a
// MESMA do token_seguro do redator (b29addd8a:src/farolredator.c): busca e o
// FE_ADDON_BUSCA de addons.c, proxima o FE_FONTE_PROXIMA de streams.c,
// mem_baixa o FE_MEM_BAIXA de memlog.c e mpv o FE_ABRIR do player desktop.
const TOK_ROTULOS = new Set(
  "x starfish avplay tpk media3 mpv dns tls prazo recusa sem_plano fora_cache busca proxima mem_baixa 2160p_hdr_dv_eac3".split(" "));
const TOK_FONTE = ["sd|480p|720p|1080p|2160p", "sdr|hdr", "dv|nodv", "ac3|eac3|dts|outro"]
  .map((l) => new Set(l.split("|")));
function tokOk(v) {
  if (typeof v !== "string") return false;
  if (TOK_ROTULOS.has(v)) return true;
  if (v.startsWith("h:")) return /^[0-9a-f]{8}$/.test(v.slice(2));
  if (codNativoOk(v)) return true;
  if (v.length >= 32) return false;
  const p = v.split("_");
  return p.length === 4 && p.every((x, i) => TOK_FONTE[i].has(x));
}

// erro.fingerprint: os oito componentes da secao 2.6 separados por "-" —
// plataforma, so_maior (1-2 digitos), backend, codigo nativo (ou "x"),
// container, video, hdr, audio. O redator escreve o proprio token sanitizado
// no componente nativo, entao o alfabeto de farolanel_token tambem e exigido.
// O CONTRATO DE TAMANHO e o do emissor (farolredator.c): ate 159 bytes — o
// exemplo real tem 72. O limite generico de 48 do §2.7, que vale para token
// solto, nao se aplica a este campo estruturado.
const FP_PLATAFORMA = new Set("webos tizen_wgt tizen_tpk android outra".split(" "));
const FP_BACKEND = new Set("starfish avplay tpk media3 outro".split(" "));
const FP_CONTAINER = new Set("mkv mp4 ts hls dash avi webm outro x".split(" "));
const FP_VIDEO = new Set("h264 hevc hevc10 av1 vp9 mpeg2 outro x".split(" "));
const FP_HDR = new Set("sdr hdr10 hdr10plus hlg dv x".split(" "));
const FP_AUDIO = new Set("aac ac3 eac3 dts truehd mp3 flac outro x".split(" "));
const TOK_CHARSET = /^[a-z0-9_.:+-]+$/;
function fingerprintOk(v) {
  if (typeof v !== "string" || v.length >= 160) return false;
  const p = v.split("-");
  if (p.length !== 8) return false;
  if (!FP_PLATAFORMA.has(p[0])) return false;
  if (!/^\d{1,2}$/.test(p[1])) return false;
  if (!FP_BACKEND.has(p[2])) return false;
  if (p[3].length > 39 || !TOK_CHARSET.test(p[3])) return false;
  if (p[3] !== "x" && !codNativoOk(p[3])) return false;
  return FP_CONTAINER.has(p[4]) && FP_VIDEO.has(p[5]) &&
         FP_HDR.has(p[6]) && FP_AUDIO.has(p[7]);
}

const FP_RX = /^[0-9a-f]{16}$/;              // erro.fp: 16 hex do SHA-256
const COMMIT_RX = /^[0-9a-f]{7,12}$/;        // mesmo criterio do redator (farolredator.c)

const numOk = (v) => typeof v === "number" && Number.isFinite(v);
const intOk = (v) => Number.isInteger(v) && Math.abs(v) <= 0x7fffffff;
// Booleano de verdade: a rede manda "offline": false, nao 0/1 (secao 2.6).
const boolOk = (v) => v === true || v === false;

// Enum fechado: { e: "a|b|c" }. As listas sao as MESMAS do redator da TV
// (src/farolredator.c): o servidor valida o contrato que o cliente declara.
const E = (lista) => ({ e: lista });

// Esquema de lista branca do FarolRegistro v1 ANINHADO (secao 2.6 do PLANO).
// Falta de chave e tolerado (TV velha manda menos); chave FORA daqui — so
// chave PROPRIA, nunca herdada de Object.prototype — recusa o pacote.
const ESQUEMA = {
  v: "int",
  gatilho: E("falha_sem_conserto|manual|falha_repetida"),
  sintoma: E("nao_comeca|trava|sem_som|legenda|qualidade|outro"),
  erro: {
    codigo: E("NENHUM|REDE_FORA|REDE_NAO_CHEGA|ADDON_NENHUM|ADDON_LENTO|DEBRID_DESCONECTOU|" +
              "DEBRID_SEM_PLANO|FONTE_RECUSOU|FONTE_SEM_RANGE|REDE_CURTA|TV_SEM_MEMORIA|" +
              "TV_NEGOU_PLAYER|AUDIO_DTS_SEM_SAIDA|AUDIO_OUTRA_FAIXA|AUDIO_SEM_SAIDA|IMAGEM_DV|" +
              "TV_FORMATO|TV_FORMATO_REPETE|PLAYER_PAROU|NADA_ACHADO"),
    estagio: E("internet|addons|debrid|fonte|tv|audio"),
    nativo: "tok", nativo_num: "int", http: "int",
    fingerprint: "fpq", fp: "hex16",
    tentou: [{
      acao: E("proxima_fonte|reconexao|sem_dv|sem_dts|fonte_menor|converter_ac3|isolar_addon|" +
              "aliviar_memoria|reconectar_rede|limpar_cache|buffer_maior|desligar_hdr|" +
              "login_debrid|login_addon|player_externo"),
      ok: "bool", ms: "int",
    }],
  },
  aparelho: {
    plataforma: E("webos|tizen_wgt|tizen_tpk|android|outra"),
    so: "ver", so_maior: "int", modelo: "mod",
    ram_total_mb: "int", ram_livre_mb: "int",
    tela: { w: "int", h: "int", hdr: "int", dv: "int", hevc: "int", av1: "int" },
    audio: { dts: "int", truehd: "int", eac3: "int", ac3: "int" },
    rede: { offline: "bool", hosts_com_falha: "int", vazao_mbps: "num" },
  },
  cliente: {
    versao: "ver", commit: "commit",
    build: E("webos-ipk|tizen-wgt|tizen-tpk|tizen-tpk40|tizen-tpk60|tizen-tpk65|android-apk|debug"),
    backend: E("starfish|avplay|tpk|media3|outro"),
    idioma: E("pt-BR|en|es|fr|de|it|ar|bg|ca|cs|da|el|fi|he|hi|hr|hu|id|ja|ko|nl|no|pl|ro|" +
              "ru|sk|sv|tr|uk|vi|zh-CN|zh-TW"),
    aberto_s: "int",
    gpu: E("auto:alto|auto:medio|auto:baixo|alto|medio|baixo|desconhecido"),
    ajustes: {
      glass_ui: "int", qualidade_max: E("auto|4k|1080p|720p|480p"), tamanho_max_gb: "num",
      dts: E("auto|ac3|aac|direto|desligado"), dv_mkv: "int", legenda_auto_sync: "int",
    },
    addons: { total: "int", ativos: "int", isolados: "int", tipos: [E("catalogo|fontes|legendas|meta")] },
    debrid: [E("torbox|realdebrid|alldebrid|premiumize|debridlink|easydebrid")],
    contas: [E("trakt|simkl")],
  },
  midia: {
    container: E("mkv|mp4|ts|hls|dash|avi|webm|outro|x"),
    video: E("h264|hevc|hevc10|av1|vp9|mpeg2|outro|x"),
    hdr: E("sdr|hdr10|hdr10plus|hlg|dv|x"), res: E("2160p|1080p|720p|480p|outra|x"),
    fps: "num", mbps: "num", gb: "num",
    audio: E("aac|aac_20|ac3|ac3_51|eac3|eac3_51|eac3_atmos|dts|dts_51|truehd|truehd_atmos|mp3|flac|outro|x"),
    legenda: E("srt_embutida|srt_externa|ass_embutida|ass_externa|pgs|nenhuma|outra|x"),
    entrega: E("debrid_cache|debrid|http|torrent|local|outro|x"),
    http: "int", ttfb_ms: "int",
  },
  cadeia: [{ passo: E("internet|addons|debrid|fonte|tv|audio"), estado: E("ok|falhou|nao_testado|testando"), ms: "int", http: "int" }],
  linha: [{
    t: "int",
    m: E("player|video|rede|addon|debrid|fonte|audio|legenda|memoria|farol"),
    e: E("abrir|pronto|erro_fonte|recusa_sinc|parado|erro_nativo|audio_recusado|pipeline_morreu|" +
         "reconexao|rede_estado|addon_busca|addon_lento|debrid|fonte_escolhida|escolhida|" +
         "fonte_proxima|mem_baixa|farol_veredito|farol_acao|farol_resultado|farol_envio|farol_consertou"),
    a: "int", b: "int", k: "tok",
  }],
  privacidade: { redator: "int", campos_recusados: "int" },
};

// true = valor dentro do esquema. `maxArr` vale quando `spec` e uma lista.
function valido(v, spec) {
  if (typeof spec === "string") {
    switch (spec) {
      case "tok": return tokOk(v);
      case "fpq": return fingerprintOk(v);
      case "mod": return modeloOk(v);
      case "ver": return versaoOk(v);
      case "commit": return typeof v === "string" && COMMIT_RX.test(v) && /[a-f]/.test(v);
      case "hex16": return typeof v === "string" && FP_RX.test(v);
      case "num": return numOk(v);
      case "int": return intOk(v);
      case "bool": return boolOk(v);
      default: return false;
    }
  }
  if (Array.isArray(spec)) {
    if (!Array.isArray(v)) return false;
    const lim = spec === ESQUEMA.linha ? LINHA_MAX : LISTA_MAX;
    if (v.length > lim) return false;
    return v.every((it) => valido(it, spec[0]));
  }
  if (typeof spec === "object" && spec !== null && typeof spec.e === "string")
    return typeof v === "string" && spec.e.split("|").includes(v);
  if (typeof v !== "object" || v === null || Array.isArray(v)) return false;
  for (const k of Object.keys(v)) {
    // hasOwn, nao `in`: chave herdada de Object.prototype ("constructor" e
    // cia.) nao e campo do esquema e derruba o pacote como qualquer estranha.
    if (!Object.hasOwn(spec, k)) return false;
    if (!valido(v[k], spec[k])) return false;
  }
  return true;
}

async function hmacHex(sal, texto) {
  const enc = new TextEncoder();
  const chave = await crypto.subtle.importKey(
    "raw", enc.encode(sal), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  const sig = await crypto.subtle.sign("HMAC", chave, enc.encode(texto));
  return [...new Uint8Array(sig)].map((x) => x.toString(16).padStart(2, "0")).join("");
}

async function sha256Hex(s) {
  const b = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(s));
  return [...new Uint8Array(b)].map((x) => x.toString(16).padStart(2, "0")).join("");
}

// Dedupe atomico na janela MOVEL inteira de 24 h (FAROL_DEDUP_JANELA) — mas o
// slot vivo de (pessoa_h, fp) mora na farol_dedup, NAO na farol_registro. Com
// a chave unica na propria tabela de registros a versao anterior nao dava
// conta de duas coisas ao mesmo tempo: a linha pendente de cota ja era
// visivel ao dedupe (um reenvio concorrente virava 200 e efetivava registro
// acima do teto) e a ocorrencia passadas 24 h reescrevia a linha — quando a
// cota estourava o 429 apagava o JSON de um recibo ainda valido. Com o estado
// separado: reclamacao e linha sao coisas distintas, o recibo anterior so sai
// pela retencao de 30 dias e a ocorrencia nova e um registro NOVO (id e
// codigo novos, linha anterior intacta).
//
// O UPSERT abaixo e uma instrucao so. `toques` no RETURNING diz quem tomou o
// slot: 1 = ESTE pedido (slot vazio ou vencido ha mais de 24 h — criado,
// registro e confirmado zerados na retomada); maior = o slot ja era de outro
// pedido, confirmado devolve o recibo do registro vivo, pendente e envio
// ainda em voo e NUNCA pode virar 200.
const FAROL_RECLAMAR =
  "INSERT INTO farol_dedup (pessoa_h, fp, criado, registro, confirmado, toques) " +
  "VALUES (?, ?, ?, NULL, 0, 1) " +
  "ON CONFLICT(pessoa_h, fp) DO UPDATE SET " +
  `toques = CASE WHEN farol_dedup.criado > excluded.criado - ${FAROL_DEDUP_JANELA} ` +
  "THEN farol_dedup.toques + 1 ELSE 1 END, " +
  `criado = CASE WHEN farol_dedup.criado > excluded.criado - ${FAROL_DEDUP_JANELA} ` +
  "THEN farol_dedup.criado ELSE excluded.criado END, " +
  `registro = CASE WHEN farol_dedup.criado > excluded.criado - ${FAROL_DEDUP_JANELA} ` +
  "THEN farol_dedup.registro ELSE NULL END, " +
  `confirmado = CASE WHEN farol_dedup.criado > excluded.criado - ${FAROL_DEDUP_JANELA} ` +
  "THEN farol_dedup.confirmado ELSE 0 END " +
  "RETURNING criado, registro, confirmado, toques";

// Parte numerica da versao ("2.1.0-ipk" -> [2,1,0,0], "2.1" -> [2,1,0,0]).
// Sufixo de build (-ipk, -apk, -wgt...) e ignorado na comparacao, no mesmo
// espirito de versaoMaior em src/avisos.c. null = ausente ou ilegivel.
export function lerVersao(v) {
  if (typeof v !== "string") return null;
  const s = v.trim();
  if (!s || s.length > 32) return null;
  const h = s.search(/[-+]/);
  const num = h < 0 ? s : s.slice(0, h);
  if (!num) return null;
  const partes = num.split(".");
  if (partes.length > 4) return null;
  const out = [0, 0, 0, 0];
  for (let i = 0; i < partes.length; i++) {
    if (!/^\d{1,4}$/.test(partes[i])) return null;
    out[i] = Number(partes[i]);
  }
  return out;
}

export function versaoMenor(cliente, correcao) {
  const a = lerVersao(cliente);
  const b = lerVersao(correcao);
  if (!a || !b) return false;
  for (let i = 0; i < 4; i++) {
    if (a[i] !== b[i]) return a[i] < b[i];
  }
  return false;
}

// Ordem da secao 2.7:
//   1. `fp` exato
//   2. `(codigo, plataforma, so_maior)`
//   3. `(codigo, plataforma)` (com `so_maior IS NULL`)
// A regra mais especifica presente na tabela governa a chave: `corrigido` so
// vale quando a versao do cliente e estritamente menor que a da correcao.
const SQL_CONHECIDO =
  "SELECT id, " +
  "CASE WHEN fp IS NOT NULL THEN 1 WHEN so_maior IS NOT NULL THEN 2 ELSE 3 END AS nivel, " +
  "estado, versao, nota_pt, nota_en " +
  "FROM farol_conhecido " +
  "WHERE (? IS NOT NULL AND fp = ?) " +
  "   OR (? IS NOT NULL AND ? IS NOT NULL AND ? IS NOT NULL " +
  "       AND fp IS NULL AND codigo = ? AND plataforma = ? AND so_maior = ?) " +
  "   OR (? IS NOT NULL AND ? IS NOT NULL " +
  "       AND fp IS NULL AND codigo = ? AND plataforma = ? AND so_maior IS NULL) " +
  "ORDER BY nivel ASC, id DESC";

export async function buscarConhecido(db, { fp = null, codigo = null, plataforma = null, soMaior = null, versao = "" } = {}) {
  const f = typeof fp === "string" && fp.length ? fp : null;
  const c = typeof codigo === "string" && codigo.length ? codigo : null;
  const p = typeof plataforma === "string" && plataforma.length ? plataforma : null;
  const s = Number.isInteger(soMaior) ? soMaior : null;
  if (!f && !(c && p)) return null;
  const r = await db.prepare(SQL_CONHECIDO).bind(
    f, f,
    c, p, s, c, p, s,
    c, p, c, p
  ).all();
  const linhas = r?.results || [];
  if (!linhas.length) return null;
  const melhorNivel = linhas[0].nivel;
  for (const l of linhas) {
    if (l.nivel !== melhorNivel) break;
    if (!ESTADOS_CONHECIDO.has(l.estado)) continue;
    if (l.estado === "corrigido" && !versaoMenor(versao, l.versao)) continue;
    return {
      estado: l.estado,
      versao: l.versao || "",
      nota_pt: l.nota_pt || "",
      nota_en: l.nota_en || "",
    };
  }
  return null;
}

// `quem` e a conta ja verificada por quemE (a mesma autenticacao de
// /v1/registro), NUNCA uma pessoa criada por `registrar`: a rota do Farol nao
// faz cadastro social. `corpo` e o JSON lido pelo index.js com o teto medido
// nos bytes crus.
export async function rotaFarolRegistro(env, quem, corpo, h, limitar, codigoRegistro) {
  const { json, erro, agora } = h;
  try {
    // Sem a chave o HMAC viraria um SHA-256 de id claro — o exato cenario que
    // DECISOES.md item 4 proibe. Recusa inteira, no envelope da API.
    if (!env.FAROL_SAL) return erro("farol sem configuracao", 503);
    if (typeof corpo !== "object" || corpo === null || Array.isArray(corpo) || corpo.v !== 1)
      return erro("pacote invalido", 422);
    if (!valido(corpo, ESQUEMA)) return erro("pacote fora do esquema", 422);
    const fingerprint = corpo.erro?.fingerprint;
    const codigo = corpo.erro?.codigo;
    if (!fingerprint || !codigo) return erro("pacote invalido", 422);

    const t = agora();
    // A cota e por CONTA, nao por perfil: comPerfil (index.js) marca o perfil
    // como ":<n>" no fim do id nuvio:, e sem isso cada perfil teria cota
    // propria (32 x 6/h). O sufixo so existe para nuvio:; trakt: nao o tem.
    const conta = String(quem.id).startsWith("nuvio:") ? String(quem.id).replace(/:\d{1,2}$/, "") : String(quem.id);
    const pessoa_h = await hmacHex(env.FAROL_SAL, conta);
    const fp = (await sha256Hex(fingerprint)).slice(0, 16);
    const versaoCli = String(corpo?.cliente?.versao || "").slice(0, 48);
    const platCli = String(corpo?.aparelho?.plataforma || "").slice(0, 48);
    const soCli = intOk(corpo?.aparelho?.so_maior) ? corpo.aparelho.so_maior : null;
    const acharConhecido = () =>
      buscarConhecido(env.DB, { fp, codigo, plataforma: platCli, soMaior: soCli, versao: versaoCli })
        .catch((e) => (/no such table/i.test(String(e?.message || e)) ? null : Promise.reject(e)));

    const janela = Math.floor(t / 86400);
    const reclamar = () =>
      env.DB.prepare(FAROL_RECLAMAR).bind(pessoa_h, fp, t).first();
    // Soltar so devolve o slot DESTE pedido: o AND criado poupa o marcador de
    // um envio concorrente (outro criado) ou ja confirmado por ele.
    const soltar = () => env.DB.prepare(
      "DELETE FROM farol_dedup WHERE pessoa_h = ? AND fp = ? AND confirmado = 0 AND criado = ?"
    ).bind(pessoa_h, fp, t).run().catch(() => {});
    let ded = await reclamar();
    // Slot pendente de OUTRO pedido (o reenvio chegou enquanto o primeiro
    // envio ainda passa pela cota): espera a confirmacao — sao duas
    // instrucoes — e devolve o MESMO recibo. Persistindo pendente, o envio
    // morreu no meio: 409 para a TV reenviar; o marcador velho e retomado
    // como ocorrencia nova. Pendente NUNCA vira 200.
    //
    // Excecao: o envio original gravou a LINHA mas morreu antes do UPDATE de
    // confirmacao (uma falha ali devolvia 503 e deixava slot pendente sem
    // `registro` — reenvios batiam em 409 ate o slot vencer). Esse orfao e
    // ADOTADO: o slot conclui com a linha gravada pela MESMA reclamacao —
    // criado igual ao do slot, nao ">=" (a linha mais nova pode ser da
    // ocorrencia seguinte, que tomou o slot depois das 24 h). E a escrita
    // so vale se o slot ainda for o que este pedido reclamou: com
    // meta.changes = 0 o slot ja e de outra reclamacao ou foi confirmado
    // pelo dono — a releitura decide; a confirmacao NUNCA e forjada em
    // memoria nem devolve recibo de outra janela.
    for (let espera = 0; ded && ded.toques > 1 && !ded.confirmado && espera < 20; espera++) {
      const criado = ded.criado;
      const orfao = await env.DB.prepare(
        "SELECT id FROM farol_registro WHERE pessoa_h = ? AND fp = ? AND criado = ?"
      ).bind(pessoa_h, fp, criado).first().catch(() => null);
      if (orfao?.id) {
        const conf = await env.DB.prepare(
          "UPDATE farol_dedup SET registro = ?, confirmado = 1 " +
          "WHERE pessoa_h = ? AND fp = ? AND confirmado = 0 AND criado = ?"
        ).bind(orfao.id, pessoa_h, fp, criado).run().catch(() => null);
        if (conf?.meta?.changes) {
          ded = { ...ded, registro: orfao.id, confirmado: 1 };
          break;
        }
        // O slot virou: rele e sai — o fluxo abaixo deduplica num recibo ja
        // confirmado, responde 409 se ele esta pendente de outro pedido ou
        // segue como envio proprio se o slot recomecou vazio. Slot intacto
        // (mesmo criado, ainda pendente): falha transitoria, volta a espera.
        ded = await reclamar();
        if (!ded || ded.criado !== criado || ded.confirmado) break;
      }
      await new Promise((resolve) => setTimeout(resolve, 25));
      ded = await reclamar();
    }
    if (ded?.toques > 1) {
      if (!ded.confirmado || !ded.registro) return erro("envio em andamento", 409);
      // Reenvio deduplicado: o codigo e o do primeiro envio, o vencimento e o
      // do criado ORIGINAL e nenhuma cota e debitada.
      await env.DB.prepare("UPDATE farol_registro SET vezes = vezes + 1 WHERE id = ?")
        .bind(ded.registro).run();
      const conhecido = await acharConhecido();
      return json({
        ok: 1, codigo: codigoRegistro(ded.registro),
        vence: ded.criado + FAROL_RETENCAO, dedup: 1,
        ...(conhecido ? { conhecido } : {}),
      });
    }
    if (!ded) return erro("falha ao gravar", 500);

    // Cotas atomicas reservadas ANTES de a linha existir (`limitar` e um
    // UPSERT com RETURNING n numa instrucao so, entao dois envios simultaneos
    // nao passam do teto por corrida). Estourou, o 429 leva so o marcador:
    // nao ha linha escrita acima da cota e a linha de um recibo ja emitido
    // nem e consultada. O debito do pedido recusado volta em cada contador —
    // recusa nao consome cota — e a janela no WHERE impede mexer na contagem
    // de um ciclo que virou no meio.
    const cotas = [["farol:h:" + pessoa_h, FAROL_HORA, 3600],
                   ["farol:d:" + pessoa_h, FAROL_DIA, 86400],
                   ["farol:dia", FAROL_DIA_GLOBAL, 86400]];
    const debitos = [];
    const devolver = async (extra) => {
      for (const [c, jj] of debitos.concat(extra || []))
        await env.DB.prepare("UPDATE limite SET n = MAX(0, n - 1) WHERE chave = ? AND janela = ?")
          .bind(c, jj).run().catch(() => {});
    };
    for (const [chave, max, jan] of cotas) {
      const j = Math.floor(t / jan);
      if (await limitar(env, chave, max, jan, t)) { debitos.push([chave, j]); continue; }
      await devolver([[chave, j]]);
      await soltar();
      return erro(max === FAROL_HORA ? "limite" : "limite diario", 429);
    }
    let lin;
    try {
      lin = await env.DB.prepare(
        "INSERT INTO farol_registro (criado, janela, pessoa_h, versao, plataforma, so_maior, codigo, fp, fingerprint, vezes, json) " +
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 1, ?) RETURNING id"
      ).bind(
        t, janela, pessoa_h,
        versaoCli,
        platCli,
        soCli ?? 0,
        codigo, fp, fingerprint,
        JSON.stringify(corpo)
      ).first();
    } catch (e) { await devolver(); await soltar(); throw e; }
    if (!lin?.id) { await devolver(); await soltar(); return erro("falha ao gravar", 500); }
    await env.DB.prepare(
      "UPDATE farol_dedup SET registro = ?, confirmado = 1 " +
      "WHERE pessoa_h = ? AND fp = ? AND confirmado = 0 AND criado = ?"
    ).bind(lin.id, pessoa_h, fp, t).run();
    const conhecido = await acharConhecido();
    return json({
      ok: 1, codigo: codigoRegistro(lin.id), vence: t + FAROL_RETENCAO,
      ...(conhecido ? { conhecido } : {}),
    });
  } catch (e) {
    // Falha de armazenamento/configuracao responde 5xx no MESMO envelope JSON
    // da API — nunca texto solto nem a excecao do D1 (tabela ausente, BigInt).
    console.log("farol: " + (e && e.message ? e.message : e));
    return erro("farol indisponivel", 503);
  }
}

const PARAM_TOK_RX = /^[A-Za-z0-9_.:+-]{1,48}$/;

// GET /v1/farol/conhecido?fp=...&c=...&p=...&v=... (autenticada por conta,
// sem gravar nada em farol_registro, farol_dedup nem pessoa).
export async function rotaFarolConhecido(env, url, h) {
  const { json, erro } = h;
  try {
    if (!env?.DB) return erro("farol indisponivel", 503);
    const fpCru = (url.searchParams.get("fp") ?? "").trim().toLowerCase();
    const codCru = (url.searchParams.get("c") ?? url.searchParams.get("codigo") ?? "").trim();
    const platCru = (url.searchParams.get("p") ?? url.searchParams.get("plataforma") ?? "").trim();
    const verCru = (url.searchParams.get("v") ?? url.searchParams.get("versao") ?? "").trim();
    const soCru = (url.searchParams.get("so_maior") ?? url.searchParams.get("so") ?? url.searchParams.get("s") ?? "").trim();

    if (fpCru && !FP_RX.test(fpCru)) return erro("fp invalido", 400);
    if (codCru && !PARAM_TOK_RX.test(codCru)) return erro("codigo invalido", 400);
    if (platCru && !PARAM_TOK_RX.test(platCru)) return erro("plataforma invalida", 400);
    if (verCru && !PARAM_TOK_RX.test(verCru)) return erro("versao invalida", 400);
    let soMaior = null;
    if (soCru) {
      if (!/^\d{1,2}$/.test(soCru)) return erro("so_maior invalido", 400);
      soMaior = Number(soCru);
    }
    if (!fpCru && !(codCru && platCru)) return erro("parametros insuficientes", 400);

    const conhecido = await buscarConhecido(env.DB, {
      fp: fpCru || null,
      codigo: codCru || null,
      plataforma: platCru || null,
      soMaior,
      versao: verCru,
    });
    return json({ ok: 1, conhecido });
  } catch (e) {
    console.log("farol conhecido: " + (e && e.message ? e.message : e));
    return erro("farol indisponivel", 503);
  }
}

// --- Pagina publica do QR: GET /v1/farol/r/<codigo> (sem autenticacao) -------
// Mostra apenas o codigo de 6 caracteres, quando vence e os botoes "Copiar",
// "Abrir Discord" e "Abrir issue". Nenhum campo do registro e sequer lido do D1
// (SELECT so `criado`), e 404 responde byte a byte igual para codigo invalido,
// fora da faixa do Farol, inexistente ou ja vencido.
const URL_DISCORD = "https://discord.gg/9NWr6SHyzJ";
const URL_ISSUE = "https://github.com/iqui27/nuvio-native-legacy/issues/new";

const CABECALHOS_PAGINA = {
  "content-type": "text/html; charset=utf-8",
  "cache-control": "no-store",
  "referrer-policy": "no-referrer",
  "x-robots-tag": "noindex, nofollow",
  "content-security-policy": "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'",
};

const PAGINA_404 = `<!doctype html><html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Nuvio · Farol</title>
<style>body{margin:0;font:16px/1.5 system-ui,-apple-system,sans-serif;background:#10131a;color:#eef2f7;display:grid;place-items:center;min-height:100vh}main{max-width:420px;padding:24px;text-align:center}h1{font-size:20px;margin:0 0 8px}p{margin:0;opacity:.72}</style></head>
<body><main><h1>Código não encontrado</h1><p>Este código não existe ou já venceu.</p></main></body></html>`;

function respostaPagina404(metodo) {
  return new Response(metodo === "HEAD" ? null : PAGINA_404, {
    status: 404,
    headers: CABECALHOS_PAGINA,
  });
}

function paginaReciboHtml(codigo, vence) {
  const d = new Date(vence * 1000);
  const iso = d.toISOString();
  const dd = String(d.getUTCDate()).padStart(2, "0");
  const mm = String(d.getUTCMonth() + 1).padStart(2, "0");
  const aaaa = String(d.getUTCFullYear());
  return `<!doctype html><html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Nuvio · Farol</title>
<style>body{margin:0;font:16px/1.5 system-ui,-apple-system,sans-serif;background:#10131a;color:#eef2f7;display:grid;place-items:center;min-height:100vh}main{max-width:420px;width:100%;box-sizing:border-box;padding:24px;text-align:center}.cod{font:700 42px/1.2 ui-monospace,SFMono-Regular,Menlo,monospace;letter-spacing:.14em;padding:18px 16px;margin:12px 0;border-radius:14px;background:#181d27;border:1px solid #ffffff1f;user-select:all}.vence{margin:0 0 22px;opacity:.75;font-size:15px}.acoes{display:flex;flex-direction:column;gap:10px}button,a.btn{display:block;width:100%;box-sizing:border-box;padding:13px 16px;border-radius:12px;border:1px solid #ffffff24;background:#1f2633;color:#eef2f7;font:600 16px system-ui,-apple-system,sans-serif;text-decoration:none;cursor:pointer}button.pri{background:#3b82f6;border-color:#3b82f6;color:#fff}</style></head>
<body><main>
<div id="codigo" class="cod">${codigo}</div>
<p class="vence">Vence em <time datetime="${iso}">${dd}/${mm}/${aaaa}</time></p>
<div class="acoes">
<button type="button" id="copiar" class="pri">Copiar</button>
<a class="btn" href="${URL_DISCORD}" target="_blank" rel="noopener noreferrer">Abrir Discord</a>
<a class="btn" href="${URL_ISSUE}" target="_blank" rel="noopener noreferrer">Abrir issue</a>
</div>
</main>
<script>(function(){var b=document.getElementById("copiar"),c="${codigo}";b.addEventListener("click",function(){function ok(){b.textContent="Copiado";setTimeout(function(){b.textContent="Copiar"},2000)}if(navigator.clipboard&&navigator.clipboard.writeText){navigator.clipboard.writeText(c).then(ok).catch(fb)}else fb();function fb(){var t=document.createElement("textarea");t.value=c;t.style.position="fixed";t.style.opacity="0";document.body.appendChild(t);t.select();try{document.execCommand("copy");ok()}catch(e){}document.body.removeChild(t)}})})();</script>
</body></html>`;
}

export async function rotaFarolPagina(req, env, codigoCru, agoraFn = () => Math.floor(Date.now() / 1000)) {
  if (req.method !== "GET" && req.method !== "HEAD")
    return new Response("metodo nao suportado", { status: 405, headers: { "content-type": "text/plain; charset=utf-8" } });
  if (!env?.DB)
    return new Response("farol indisponivel", { status: 503, headers: { "content-type": "text/plain; charset=utf-8" } });
  const id = decodificarCodigo(codigoCru);
  if (id === null || id < FAROL_ID_MIN) return respostaPagina404(req.method);
  try {
    const linha = await env.DB.prepare("SELECT criado FROM farol_registro WHERE id = ?")
      .bind(id).first();
    const t = agoraFn();
    const vence = linha && Number.isFinite(linha.criado) ? linha.criado + FAROL_RETENCAO : 0;
    if (!linha || vence <= t) return respostaPagina404(req.method);
    const cod = codificarId(id);
    if (!cod) return respostaPagina404(req.method);
    const html = paginaReciboHtml(cod, vence);
    return new Response(req.method === "HEAD" ? null : html, {
      status: 200,
      headers: CABECALHOS_PAGINA,
    });
  } catch (e) {
    console.log("farol pagina: " + (e && e.message ? e.message : e));
    return new Response("farol indisponivel", { status: 503, headers: { "content-type": "text/plain; charset=utf-8" } });
  }
}

// Retencao de 30 dias — a promessa que a TV faz na tela de enviados. Em lotes
// como `registro` (#203): DELETE empedrado segura o unico escritor do D1.
const FAROL_LOTE = 200, FAROL_LOTES_MAX = 20;
export async function limparFarol(env, t) {
  for (let n = 0; n < FAROL_LOTES_MAX; n++) {
    const r = await env.DB.prepare(
      "DELETE FROM farol_registro WHERE id IN (SELECT id FROM farol_registro WHERE criado < ? LIMIT ?)"
    ).bind(t - FAROL_RETENCAO, FAROL_LOTE).run();
    if ((r.meta?.changes || 0) < FAROL_LOTE) break;
  }
  // Marcadores de dedupe sem registro vivo na retencao nao apontam mais para
  // recibo nenhum — saem junto (um slot velho ja seria retomado de qualquer
  // jeito; isso so evita a tabela crescer). O NOT IN poupa os pendentes, que
  // tem registro NULL e nao casam na subconsulta.
  await env.DB.prepare("DELETE FROM farol_dedup WHERE criado < ?").bind(t - FAROL_RETENCAO).run();
  await env.DB.prepare(
    "DELETE FROM farol_dedup WHERE registro NOT IN (SELECT id FROM farol_registro)").run();
}
