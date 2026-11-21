// FAROL — RECEPCAO DO REGISTRO ESTRUTURADO (PLANO.md secao 2.7, issue 5).
// O pacote `FarolRegistro v1` e MONTADO na TV por lista branca (farolpacote.c),
// ANINHADO como na secao 2.6 do PLANO e declarado pelo redator da issue 4
// (src/farolredator.c:18): so enums, numeros e tokens do alfabeto de
// `farolanel_token`. Aqui a defesa e a mesma: chave fora do esquema ou valor
// com sinal de vazamento ("://", "@", IPv4/IPv6, hash cru) e 422 e nada e
// gravado. O servidor nao sabe quem mandou: `pessoa_h` e HMAC(FAROL_SAL, conta)
// so para os tetos de abuso, nunca o id cru (DECISOES.md item 4).
//
// O codigo de 6 caracteres do recibo e a MESMA bijecao de `registro`
// (codigo.js); para os dois espacos nao colidirem os ids do Farol comecam em
// 2^29 (ver migracao-018-farol.sql e codigo-registro.mjs).
//
// Match de correcao conhecida (tabela farol_conhecido), GET /v1/farol/conhecido
// e a pagina /v1/farol/r/<codigo> sao da issue 13 — a tabela ja nasce aqui
// porque mora no mesmo arquivo de migracao.

export const FAROL_CORPO_MAX = 64 * 1024;
const FAROL_RETENCAO = 30 * 24 * 3600;
const FAROL_HORA = 6;            // envios por CONTA por hora
const FAROL_DIA = 20;            // envios por CONTA por dia
const FAROL_DIA_GLOBAL = 500;    // envios no total por dia
const FAROL_DEDUP_JANELA = 24 * 3600;
const LINHA_MAX = 256;
const LISTA_MAX = 64;

// O alfabeto seguro: o que o redator da TV ja produz, mais espaco e maiusculas
// (modelo "OLED55C9PSA", versao "2.1.0"). Um valor que nao cabe aqui e texto
// livre — e texto livre e o que o pacote nao pode ter.
const STR_RX = /^[A-Za-z0-9_.:+ \-]*$/;
const IPV4_RX = /\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}/;
// Sinais de dado proibido em claro: IPv6 (grupos de ate 4 hex separados por
// ":"), uma cadeia hex longa (SHA/segredo colado no lugar de um token) e o
// PIN de 4-8 digitos que `cliente.commit` poderia carregar (ver "commit").
const HEXLONGO_RX = /^[0-9a-f]{17,}$/i;
const MOD_RX = /^[A-Za-z0-9_.\- ]{1,32}$/;   // modelo: sem ":" nem "+" (secao 2.7)
const COMMIT_RX = /^[0-9a-f]{7,12}$/;        // mesmo criterio do redator (farolredator.c)
const FP_RX = /^[0-9a-f]{16}$/;              // erro.fp: 16 hex do SHA-256
const VER_RX = /^\d{1,4}(\.\d{1,4}){0,2}(-[a-z0-9]{1,12})?$/;

const pareceIPv6 = (s) => {
  if (!s.includes(":")) return false;
  const p = s.split(":");
  return p.length >= 3 && p.every((g) => g === "" || /^[0-9a-fA-F]{1,4}$/.test(g));
};
const vazamento = (v) =>
  v.includes("://") || v.includes("@") || IPV4_RX.test(v) || pareceIPv6(v) || HEXLONGO_RX.test(v);

function strOk(v, max) {
  return typeof v === "string" && v.length <= max && STR_RX.test(v) && !vazamento(v);
}
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
    nativo: "s", nativo_num: "int", http: "int",
    fingerprint: "s", fp: "hex16",
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
    a: "int", b: "int", k: "s",
  }],
  privacidade: { redator: "int", campos_recusados: "int" },
};

// true = valor dentro do esquema. `maxArr` vale quando `spec` e uma lista.
function valido(v, spec) {
  if (typeof spec === "string") {
    switch (spec) {
      case "s": return strOk(v, 48);
      case "mod": return typeof v === "string" && MOD_RX.test(v) && !vazamento(v);
      case "ver": return typeof v === "string" && VER_RX.test(v);
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

    // Reenvio da mesma falha em 24 h = o MESMO registro: devolve o codigo que
    // ja existe e so soma `vezes`, sem gastar cota. `vence` nasce de `criado`,
    // o mesmo relogio da limpeza — nao da hora do reenvio.
    const ja = await env.DB.prepare(
      "SELECT id, criado FROM farol_registro WHERE pessoa_h = ? AND fp = ? AND criado > ? ORDER BY criado LIMIT 1"
    ).bind(pessoa_h, fp, t - FAROL_DEDUP_JANELA).first();
    if (ja?.id) {
      await env.DB.prepare("UPDATE farol_registro SET vezes = vezes + 1 WHERE id = ?").bind(ja.id).run();
      return json({ ok: 1, codigo: codigoRegistro(ja.id), vence: ja.criado + FAROL_RETENCAO, dedup: 1 });
    }

    // Cotas: le primeiro SEM debitar — um 429 nao pode consumir contador, e um
    // 5xx de gravacao idem. Os tres contadores so sobem DEPOIS de gravar.
    const cotas = [["farol:h:" + pessoa_h, FAROL_HORA, 3600],
                   ["farol:d:" + pessoa_h, FAROL_DIA, 86400],
                   ["farol:dia", FAROL_DIA_GLOBAL, 86400]];
    for (const [chave, max, jan] of cotas) {
      const l = await env.DB.prepare("SELECT n FROM limite WHERE chave = ? AND janela = ?")
        .bind(chave, Math.floor(t / jan)).first();
      if ((l?.n || 0) >= max) return erro(max === FAROL_HORA ? "limite" : "limite diario", 429);
    }

    // Dedupe atomico: indice unico (pessoa_h, fp, janela) + INSERT ON CONFLICT.
    // Duas gravacoes concorrentes da mesma falha na mesma janela viram UMA
    // linha — a segunda cai no DO UPDATE e ganha o codigo da primeira.
    const janela = Math.floor(t / FAROL_DEDUP_JANELA);
    await env.DB.prepare(
      "INSERT INTO farol_registro (criado, janela, pessoa_h, versao, plataforma, so_maior, codigo, fp, fingerprint, vezes, json) " +
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 1, ?) " +
      "ON CONFLICT(pessoa_h, fp, janela) DO UPDATE SET vezes = vezes + 1"
    ).bind(
      t, janela, pessoa_h,
      String(corpo?.cliente?.versao || "").slice(0, 48),
      String(corpo?.aparelho?.plataforma || "").slice(0, 48),
      intOk(corpo?.aparelho?.so_maior) ? corpo.aparelho.so_maior : 0,
      codigo, fp, fingerprint,
      JSON.stringify(corpo)
    ).run();

    const lin = await env.DB.prepare(
      "SELECT id, criado, vezes FROM farol_registro WHERE pessoa_h = ? AND fp = ? AND janela = ?"
    ).bind(pessoa_h, fp, janela).first();
    if (!lin?.id) return erro("falha ao gravar", 500);
    // vezes > 1 = outra gravacao ganhou a corrida pelo ON CONFLICT: devolve o
    // codigo dela (idempotente), nao cria segundo recibo.
    if (lin.vezes > 1)
      return json({ ok: 1, codigo: codigoRegistro(lin.id), vence: lin.criado + FAROL_RETENCAO, dedup: 1 });
    for (const [chave, max, jan] of cotas) await limitar(env, chave, max, jan, t);
    return json({ ok: 1, codigo: codigoRegistro(lin.id), vence: lin.criado + FAROL_RETENCAO });
  } catch (e) {
    // Falha de armazenamento/configuracao responde 5xx no MESMO envelope JSON
    // da API — nunca texto solto nem a excecao do D1 (tabela ausente, BigInt).
    console.log("farol: " + (e && e.message ? e.message : e));
    return erro("farol indisponivel", 503);
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
}
