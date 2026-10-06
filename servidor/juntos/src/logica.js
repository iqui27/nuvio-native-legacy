// A SALA DO WATCH TOGETHER, SEM CLOUDFLARE. Toda a regra mora aqui: relogio da
// sala, estados, espera com prazo, "alcancando", controle, reacoes, chat,
// passagem de anfitriao. O Durable Object (sala.js) so liga isto aos sockets e
// ao storage. Assim a regra e testada em Node com relogio falso
// (teste-logica.mjs), e uma troca de plataforma nao reescreve o protocolo.
//
// Plano: docs/plans/watch-together/assistir-juntos-plano.md, secoes 6 e 7.
//
// MODELO DO RELOGIO (6.1). Ninguem transmite posicao a cada segundo: todos
// derivam a posicao do mesmo par (pos0, t0) em ms do relogio do SERVIDOR:
//   posAlvo(agora) = estado == "tocando" ? pos0 + (agora - t0) / 1000 : pos0
// Para comecar ou continuar, t0 fica NO FUTURO (play agendado, 6.4): todas as
// TVs ja estao pausadas em pos0 e soltam o play no mesmo instante.
//
// SAIDA: a logica nao conhece socket. Cada acao devolve uma lista de envios
// { para, msg } onde `para` e "*" (todos os membros, TVs e celulares),
// "tv:<id>", "cel:<id>" ou "pessoa:<id>" (a TV e o celular da pessoa), e uma
// lista de fechamentos { para, codigo, motivo }.

export const REACOES = ["risada", "susto", "coracao", "palmas", "hein"];
export const FRASES = ["pausa_rapidinho", "volta_um_pouco", "legenda", "ja_volto"];
const ESTADOS_MEMBRO = new Set(["entrando", "procurando", "carregando", "pronto", "tocando",
  "pausado", "semfonte", "erro"]);
const FONTE_TIPOS = new Set(["mesma", "parecida", "diferente", "nenhuma"]);
const PLATS = new Set(["lg", "wgt", "tpk", "android", "mac", "linux"]);

export const LIMITES = {
  carregandoMs: 3000,       // travada curta nao para ninguem (6.6)
  esperaPadraoS: 20,        // prazo da espera; o anfitriao escolhe 10/20/60
  esperasMax: 2,            // por membro a cada 10 min; a terceira vira "alcancando"
  esperasJanelaMs: 600000,
  estavelMs: 30000,         // quem esta alcancando volta ao normal depois disto
  autoProximoMs: 15000,     // mini-barreira do proximo episodio (5.6)
  leadMinMs: 600,           // play agendado: max(2 x maior RTT, 600 ms)
  vaziaMs: 600000,          // sala sem TV conectada por 10 min -> fim
  tetoMs: 12 * 3600000,     // nenhuma sala passa de 12 h
  anfitriaoMs: 30000,       // anfitriao sem socket por 30 s passa o controle
  ausenteMs: 600000,        // membro sem socket por 10 min libera a cadeira
  cmdPorS: 5,
  reacaoMs: 2000,
  chatMs: 1000,
  chatMax: 200,
  chatGuardado: 50,
  seqVelho: 2,              // cmd com seqBase mais de 2 mudancas atras...
  seqVelhoMs: 1000,         // ...em menos de 1 s e descartado (6.8)
  celularMs: 600000,        // o codigo do QR do celular vale 10 min, uma vez
};

const num = (v, a, b, p) => (Number.isFinite(v) ? Math.min(b, Math.max(a, v)) : p);
const txt = (v, n) => (typeof v === "string" ? v.slice(0, n) : "");

// Impressao do release (6.9): so estes campos, e nunca uma URL. O que vier a
// mais e descartado aqui, antes de chegar a qualquer outro membro.
export function limparImpressao(i) {
  if (!i || typeof i !== "object") return {};
  const o = {};
  if (typeof i.hash === "string" && /^[0-9a-fA-F]{40}$/.test(i.hash)) o.hash = i.hash.toLowerCase();
  if (Number.isInteger(i.idx) && i.idx >= 0 && i.idx < 100000) o.idx = i.idx;
  if (Number.isFinite(i.bytes) && i.bytes > 0) o.bytes = Math.floor(i.bytes);
  if (typeof i.arquivo === "string" && !/[/\\]|:\/\//.test(i.arquivo)) o.arquivo = i.arquivo.slice(0, 120);
  if (typeof i.binge === "string" && !/:\/\//.test(i.binge)) o.binge = i.binge.slice(0, 80);
  if (Number.isInteger(i.altura) && i.altura > 0 && i.altura <= 4320) o.altura = i.altura;
  if (Number.isFinite(i.durMs) && i.durMs > 0) o.durMs = Math.floor(i.durMs);
  if (typeof i.rotulo === "string" && !/:\/\//.test(i.rotulo)) o.rotulo = i.rotulo.slice(0, 60);
  return o;
}

export class SalaLogica {
  // `agora()` em ms. `max`: TVs por sala (SALA_MAX).
  constructor({ agora, max = 5 } = {}) {
    this.agora = agora || (() => Date.now());
    this.max = max;
    this.s = null;   // estado serializavel; null = sala ainda nao aberta
  }

  // ---- persistencia ------------------------------------------------------------
  serializar() { return this.s; }
  restaurar(s) { this.s = s || null; }
  aberta() { return !!this.s && !this.s.fim; }
  encerrada() { return !!this.s && !!this.s.fim; }

  // ---- relogio -------------------------------------------------------------------
  posAlvo(t = this.agora()) {
    const r = this.s.rel;
    return r.estado === "tocando" ? Math.max(0, r.pos0 + (t - r.t0) / 1000) : r.pos0;
  }
  // Folga do play agendado: 2 x a pior RTT (Jellyfin), e o tempo que a TV mais
  // lenta leva de um seek ate estar tocando (`prep`, medido por ela), para
  // ninguem largar atrasado depois de um seek comum.
  lead() {
    let rtt = 0, prep = 0;
    for (const m of Object.values(this.s.membros)) if (m.on) {
      if (m.rtt > rtt) rtt = m.rtt;
      if (m.prep > prep) prep = m.prep;
    }
    return Math.round(Math.max(LIMITES.leadMinMs, Math.min(3000, Math.max(2 * rtt, prep + 150))));
  }
  mudarRel(campos, autor, op) {
    const r = this.s.rel;
    Object.assign(r, campos);
    r.seq += 1;
    r.autor = autor || "";
    r.op = op || "";
    r.mudou = this.agora();
  }

  // ---- visoes enviadas ---------------------------------------------------------
  membroPub(m) {
    const o = { id: m.id, nome: m.nome, av: m.av, st: m.on ? m.st : "ausente", anf: m.id === this.s.anf,
      on: m.on };
    if (m.pct) o.pct = m.pct;
    if (m.nivel) o.nivel = m.nivel;
    if (m.plat) o.plat = m.plat;
    if (m.fonte) o.fonte = m.fonte;
    if (m.alc) o.alc = true;
    if (m.cel) o.cel = true;
    return o;
  }
  snapshot() {
    const s = this.s;
    const r = s.rel;
    const rel = { ep: r.ep, estado: r.estado, pos0: r.pos0, t0: r.t0, taxa: 1, autor: r.autor, op: r.op };
    if (r.auto) rel.auto = r.auto;
    if (s.espera.quem.length) { rel.esperando = s.espera.quem.slice(); rel.prazo = s.espera.ate; }
    return { t: "sala", seq: r.seq, s: this.agora(), rel,
      membros: Object.values(s.membros).sort((a, b) => a.ordem - b.ordem).map((m) => this.membroPub(m)),
      controle: s.controle, max: this.max,
      cfg: { ep: s.cfg.ep, titulo: s.cfg.titulo, poster: s.cfg.poster, imp: s.cfg.imp, codigo: s.cfg.codigo,
        espera: s.cfg.espera } };
  }

  // ---- entrada -----------------------------------------------------------------
  // `b` e o bilhete ja conferido. Devolve { ok, envios, fechar, cx } onde cx e a
  // identidade da conexao ({ pessoa, papel: "tv"|"cel" }), ou { ok:false, codigo }.
  conectar(b, extras = {}) {
    const out = { envios: [], fechar: [] };
    const t = this.agora();
    if (this.encerrada()) return { ok: false, codigo: 4410, motivo: "fim", ...out };
    const papel = b.papel === "celular" ? "cel" : "tv";
    if (!this.s) {
      if (b.papel !== "anfitriao" || !b.cfg) return { ok: false, codigo: 4404, motivo: "sala_fechada", ...out };
      this.criar(b, t);
    }
    const s = this.s;
    if (b.n) {
      if (s.usados[b.n]) return { ok: false, codigo: 4401, motivo: "bilhete_usado", ...out };
      s.usados[b.n] = b.exp;
    }
    const id = b.pessoa;
    if (papel === "cel") {
      const m = s.membros[id];
      if (!m) return { ok: false, codigo: 4403, motivo: "sem_tv", ...out };
      // Um celular por TV: o novo QR substitui o anterior.
      if (m.cel) out.fechar.push({ para: "cel:" + id, codigo: 4001, motivo: "substituido" });
      m.cel = true;
      m.chat = b.chat ? 1 : 0;
      m.celK = extras.chaveCelular || "";
      out.envios.push({ para: "cel:" + id, msg: { t: "sessao", k: m.celK, id, chat: m.chat } });
      out.envios.push({ para: "cel:" + id, msg: this.snapshot() });
      if (s.chat.length) out.envios.push({ para: "cel:" + id, msg: { t: "historico", itens: s.chat.slice() } });
      out.envios.push({ para: "*", msg: { t: "membro", ...this.membroPub(m) } });
      return { ok: true, cx: { pessoa: id, papel: "cel" }, ...out };
    }
    let m = s.membros[id];
    if (!m) {
      if (b.pend && !s.aceitos[id] && id !== s.anf) {
        s.pend[id] = { id, nome: txt(b.nome, 40), av: txt(b.av, 512), desde: t };
        out.envios.push({ para: "tv:" + id, msg: { t: "aguardando" } });
        out.envios.push({ para: "tv:" + s.anf, msg: { t: "pedido", quem: { id, nome: s.pend[id].nome, av: s.pend[id].av } } });
        return { ok: true, cx: { pessoa: id, papel: "tv", pend: true }, ...out };
      }
      const tvs = Object.keys(s.membros).length;
      if (tvs >= this.max) return { ok: false, codigo: 4409, motivo: "cheia", ...out };
      m = s.membros[id] = this.novoMembro(b, t);
      out.envios.push({ para: "*", msg: { t: "aviso", k: "entrou", quem: id } });
    } else {
      if (m.on) out.fechar.push({ para: "tv:" + id, codigo: 4001, motivo: "substituido" });
      if (!m.on) out.envios.push({ para: "*", msg: { t: "aviso", k: "voltou", quem: id } });
      m.nome = txt(b.nome, 40) || m.nome;
      m.av = txt(b.av, 512) || m.av;
    }
    m.on = true;
    m.saiuEm = 0;
    m.st = "entrando";
    m.pct = 0;
    if (b.inf) m.inf = 1;
    // Quem entra no meio de uma sala tocando segue o relogio sozinho ate
    // estabilizar: a entrada dele nao trava ninguem.
    if (s.rel.estado === "tocando" || s.rel.estado === "esperando") { m.alc = true; m.alcAte = 0; }
    if (id === s.anf) s.anfSaiu = 0;
    s.vaziaDesde = 0;
    out.envios.push({ para: "*", msg: this.snapshot() });
    if (id === s.anf) for (const p of Object.values(s.pend))
      out.envios.push({ para: "tv:" + id, msg: { t: "pedido", quem: { id: p.id, nome: p.nome, av: p.av } } });
    return { ok: true, cx: { pessoa: id, papel: "tv" }, ...out };
  }

  // O celular chegou com o codigo do QR: vira o "bilhete" dele, e o codigo morre.
  resgatarCelular(codigo) {
    if (!this.aberta() || typeof codigo !== "string") return null;
    const v = this.s.celCod[codigo];
    if (!v) return null;
    delete this.s.celCod[codigo];
    if (v.exp < this.agora() || !this.s.membros[v.pessoa]) return null;
    return { pessoa: v.pessoa, papel: "celular", chat: v.chat, sala: this.s.sala };
  }

  criar(b, t) {
    const c = b.cfg || {};
    const ep = txt(c.ep, 64);
    this.s = {
      sala: b.sala, criado: t, fim: 0,
      cfg: { ep, titulo: txt(c.titulo, 120), poster: txt(c.poster, 512), imp: limparImpressao(c.imp),
        codigo: txt(c.codigo, 8), espera: [10, 20, 60].includes(c.espera) ? c.espera : LIMITES.esperaPadraoS,
        inf: c.inf ? 1 : 0 },
      controle: c.controle === "anfitriao" ? "anfitriao" : "todos",
      anf: b.pessoa, anfSaiu: 0, vaziaDesde: 0, ordem: 0,
      rel: { ep, estado: "lobby", pos0: num(c.pos, 0, 86400, 0), t0: t, seq: 1, autor: b.pessoa, op: "criar",
        mudou: t, auto: 0 },
      espera: { quem: [], ate: 0 },
      membros: {}, pend: {}, aceitos: {}, usados: {}, chat: [], chatSeq: 0, celCod: {},
    };
  }

  novoMembro(b, t) {
    return { id: b.pessoa, nome: txt(b.nome, 40), av: txt(b.av, 512), ordem: ++this.s.ordem, on: true,
      st: "entrando", pct: 0, nivel: "", plat: "", fonte: "", rtt: 0, prep: 0, alc: false, alcAte: 0,
      carregDesde: 0, esperas: [], cmdT: [], reacT: 0, chatT: 0, saiuEm: 0, cel: false, chat: 0, celK: "",
      inf: b.inf ? 1 : 0 };
  }

  // ---- saida ---------------------------------------------------------------------
  desconectar(cx) {
    const out = { envios: [], fechar: [] };
    if (!this.aberta() || !cx) return out;
    const s = this.s;
    const t = this.agora();
    if (cx.pend) { delete s.pend[cx.pessoa]; return out; }
    const m = s.membros[cx.pessoa];
    if (!m) return out;
    if (cx.papel === "cel") {
      m.cel = false;
      out.envios.push({ para: "*", msg: { t: "membro", ...this.membroPub(m) } });
      return out;
    }
    m.on = false;
    m.saiuEm = t;
    m.carregDesde = 0;
    if (cx.pessoa === s.anf) s.anfSaiu = t;
    this.tirarDaEspera(m.id, out, "");
    if (!Object.values(s.membros).some((x) => x.on)) s.vaziaDesde = t;
    out.envios.push({ para: "*", msg: { t: "aviso", k: "ausente", quem: m.id } });
    out.envios.push({ para: "*", msg: this.snapshot() });
    return out;
  }

  // Saida voluntaria ("Sair da sessao"): a cadeira fica livre na hora.
  sair(id, out) {
    const s = this.s;
    const m = s.membros[id];
    if (!m) return;
    delete s.membros[id];
    this.tirarDaEspera(id, out, "");
    out.envios.push({ para: "*", msg: { t: "aviso", k: "saiu", quem: id, nome: m.nome } });
    out.fechar.push({ para: "pessoa:" + id, codigo: 1000, motivo: "saiu" });
    if (id === s.anf) this.passarAnfitriao(out);
    if (!Object.keys(s.membros).length) { this.encerrar("vazia", out); return; }
    if (!Object.values(s.membros).some((x) => x.on)) s.vaziaDesde = this.agora();
    out.envios.push({ para: "*", msg: this.snapshot() });
  }

  passarAnfitriao(out) {
    const s = this.s;
    const prox = Object.values(s.membros).filter((m) => m.on && m.id !== s.anf).sort((a, b) => a.ordem - b.ordem)[0];
    if (!prox) return false;
    s.anf = prox.id;
    s.anfSaiu = 0;
    out.envios.push({ para: "*", msg: { t: "aviso", k: "anfitriao", quem: prox.id } });
    for (const p of Object.values(s.pend))
      out.envios.push({ para: "tv:" + prox.id, msg: { t: "pedido", quem: { id: p.id, nome: p.nome, av: p.av } } });
    return true;
  }

  encerrar(motivo, out) {
    const s = this.s;
    if (!s || s.fim) return;
    out.envios.push({ para: "*", msg: { t: "fim", motivo, dur: Math.round((this.agora() - s.criado) / 1000) } });
    out.fechar.push({ para: "*", codigo: 1000, motivo: "fim" });
    // Lapide: a sala nao renasce com um bilhete atrasado. O resto (impressao,
    // chat, membros) e apagado AQUI, no fim, como a secao 7.6 promete.
    this.s = { fim: this.agora(), motivo, sala: s.sala };
  }

  // ---- mensagens -----------------------------------------------------------------
  mensagem(cx, m) {
    const out = { envios: [], fechar: [] };
    if (!this.aberta() || !cx || !m || typeof m.t !== "string") return out;
    const s = this.s;
    const t = this.agora();
    if (m.t === "ping") {
      out.envios.push({ para: (cx.papel === "cel" ? "cel:" : "tv:") + cx.pessoa,
        msg: { t: "pong", c: Number.isFinite(m.c) ? m.c : 0, s: t } });
      return out;
    }
    if (cx.pend) return out;    // esperando aceite: so ping
    const eu = s.membros[cx.pessoa];
    if (!eu) return out;
    const ehAnf = cx.pessoa === s.anf;

    switch (m.t) {
      case "ola": {
        if (cx.papel !== "tv") break;
        if (PLATS.has(m.plat)) eu.plat = m.plat;
        if (m.nivel === "A" || m.nivel === "B") eu.nivel = m.nivel;
        if (Number.isFinite(m.rtt)) eu.rtt = num(m.rtt, 0, 5000, 0);
        out.envios.push({ para: "*", msg: { t: "membro", ...this.membroPub(eu) } });
        out.envios.push({ para: "tv:" + eu.id, msg: this.snapshot() });
        break;
      }
      case "estado": {
        if (cx.papel !== "tv") break;
        this.estadoMembro(eu, m, out, t);
        break;
      }
      case "cmd": {
        if (!this.podeCmd(eu, t)) { out.envios.push({ para: dest(cx), msg: { t: "recusado", k: "rapido" } }); break; }
        this.comando(cx, eu, ehAnf, m, out, t);
        break;
      }
      case "reacao":
      case "frase": {
        const lista = m.t === "reacao" ? REACOES : FRASES;
        if (!lista.includes(m.k)) break;
        if (t - eu.reacT < LIMITES.reacaoMs) break;
        eu.reacT = t;
        out.envios.push({ para: "*", msg: { t: m.t, de: eu.id, k: m.k } });
        break;
      }
      case "chat": {
        // Texto livre so do celular, e so se o perfil deixa (infantil: desligado).
        if (cx.papel !== "cel" || !eu.chat) break;
        const tx = String(m.txt || "").replace(/[\u0000-\u001f\u007f]/g, " ").trim().slice(0, LIMITES.chatMax);
        if (!tx || t - eu.chatT < LIMITES.chatMs) break;
        eu.chatT = t;
        const item = { t: "chat", id: ++s.chatSeq, de: eu.id, txt: tx };
        s.chat.push(item);
        if (s.chat.length > LIMITES.chatGuardado) s.chat.shift();
        out.envios.push({ para: "*", msg: item });
        break;
      }
      case "celular": {
        // QR do celular (5.8): a TV pede um codigo curto de 8 caracteres, de
        // uso unico e 10 min. Curto porque o qr.c das TVs so cabe ~134 bytes.
        // Perfil infantil: chat desligado, digam o que disserem.
        if (cx.papel !== "tv" || t - (eu.celPedido || 0) < 3000) break;
        eu.celPedido = t;
        for (const [c, v] of Object.entries(s.celCod)) if (v.pessoa === eu.id || v.exp < t) delete s.celCod[c];
        const codigo = codigoCelular();
        s.celCod[codigo] = { pessoa: eu.id, chat: m.chat && !eu.inf ? 1 : 0, exp: t + LIMITES.celularMs };
        out.envios.push({ para: "tv:" + eu.id, msg: { t: "celular", codigo, exp: t + LIMITES.celularMs } });
        break;
      }
      case "pedir": {
        // "Pedir pausa" de quem nao controla (trava do anfitriao).
        if (m.k !== "pausa" || t - eu.reacT < LIMITES.reacaoMs) break;
        eu.reacT = t;
        out.envios.push({ para: "pessoa:" + s.anf, msg: { t: "pedir", k: "pausa", de: eu.id } });
        break;
      }
      case "aceitar": {
        if (!ehAnf || cx.papel !== "tv" || typeof m.quem !== "string") break;
        const p = s.pend[m.quem];
        if (!p) break;
        delete s.pend[m.quem];
        if (m.ok) {
          s.aceitos[m.quem] = 1;
          // A TV pendente reconecta com bilhete novo; o aceite fica guardado.
          out.envios.push({ para: "tv:" + m.quem, msg: { t: "aceito" } });
          out.fechar.push({ para: "tv:" + m.quem, codigo: 4100, motivo: "aceito" });
        } else {
          out.envios.push({ para: "tv:" + m.quem, msg: { t: "recusado", k: "anfitriao" } });
          out.fechar.push({ para: "tv:" + m.quem, codigo: 4403, motivo: "recusado" });
        }
        break;
      }
      case "expulsar": {
        // So o celular de alguem (QR repassado): a pessoa continua na sala.
        if (!ehAnf || typeof m.quem !== "string" || !s.membros[m.quem]) break;
        out.fechar.push({ para: "cel:" + m.quem, codigo: 4403, motivo: "expulso" });
        break;
      }
      case "sair": {
        if (cx.papel !== "tv") break;
        this.sair(eu.id, out);
        break;
      }
      default: break;
    }
    return out;
  }

  podeCmd(eu, t) {
    eu.cmdT = eu.cmdT.filter((x) => t - x < 1000);
    if (eu.cmdT.length >= LIMITES.cmdPorS) return false;
    eu.cmdT.push(t);
    return true;
  }

  estadoMembro(eu, m, out, t) {
    const s = this.s;
    const st = ESTADOS_MEMBRO.has(m.st) ? m.st : eu.st;
    const antes = eu.st;
    eu.st = st;
    eu.pct = st === "carregando" ? num(m.pct, 0, 100, 0) : 0;
    if (m.fonte && FONTE_TIPOS.has(m.fonte.tipo)) eu.fonte = m.fonte.tipo;
    if (Number.isFinite(m.rtt)) eu.rtt = num(m.rtt, 0, 5000, eu.rtt);
    if (Number.isFinite(m.prep)) eu.prep = num(m.prep, 0, 2500, 0);
    if (st === "carregando") {
      if (eu.alc) eu.alcAte = 0;
      else if (s.rel.estado === "tocando" && !eu.carregDesde) eu.carregDesde = t;
    } else {
      eu.carregDesde = 0;
      if (st === "tocando" && eu.alc && !eu.alcAte) eu.alcAte = t + LIMITES.estavelMs;
      if (st === "pronto" || st === "tocando") this.tirarDaEspera(eu.id, out, "pronto");
    }
    if (antes !== st || st === "carregando") out.envios.push({ para: "*", msg: { t: "membro", ...this.membroPub(eu) } });
    // Lobby com mini-barreira (proximo episodio): todos prontos = comeca ja.
    if (s.rel.estado === "lobby" && s.rel.auto && this.todosProntos()) this.comecar(s.anf, "auto", out);
  }

  todosProntos() {
    const on = Object.values(this.s.membros).filter((m) => m.on);
    return on.length > 0 && on.every((m) => m.st === "pronto");
  }

  tirarDaEspera(id, out, motivo) {
    const s = this.s;
    const i = s.espera.quem.indexOf(id);
    if (i < 0) return;
    s.espera.quem.splice(i, 1);
    if (!s.espera.quem.length && s.rel.estado === "esperando") {
      s.espera.ate = 0;
      this.mudarRel({ estado: "tocando", t0: this.agora() + this.lead() }, id, motivo || "retomar");
      out.envios.push({ para: "*", msg: this.snapshot() });
    }
  }

  comecar(autor, op, out) {
    const s = this.s;
    s.espera = { quem: [], ate: 0 };
    for (const m of Object.values(s.membros)) {
      m.carregDesde = 0;
      // Quem ainda nao esta pronto entra depois, alcancando (5.3).
      if (m.on && m.st !== "pronto" && m.st !== "tocando") { m.alc = true; m.alcAte = 0; }
    }
    this.mudarRel({ estado: "tocando", t0: this.agora() + this.lead(), auto: 0 }, autor, op);
    out.envios.push({ para: "*", msg: this.snapshot() });
  }

  comando(cx, eu, ehAnf, m, out, t) {
    const s = this.s;
    const r = s.rel;
    const op = m.op;
    const soAnf = ["encerrar", "controle", "espera"];
    const controle = ["play", "pausa", "seek", "proximo", "comecar"];
    if (!soAnf.includes(op) && !controle.includes(op)) return;
    if (soAnf.includes(op) && !ehAnf) { out.envios.push({ para: dest(cx), msg: { t: "recusado", k: "so_anfitriao" } }); return; }
    if (controle.includes(op) && s.controle === "anfitriao" && !ehAnf) {
      out.envios.push({ para: dest(cx), msg: { t: "recusado", k: "so_anfitriao", anf: s.anf } });
      return;
    }
    // 6.8: um comando que nasceu olhando um estado velho nao ressuscita uma
    // pausa antiga. O cliente reconcilia pelo snapshot.
    if (Number.isInteger(m.seqBase) && r.seq - m.seqBase > LIMITES.seqVelho && t - r.mudou < LIMITES.seqVelhoMs) {
      out.envios.push({ para: dest(cx), msg: { t: "recusado", k: "velho" } });
      out.envios.push({ para: dest(cx), msg: this.snapshot() });
      return;
    }
    const autor = eu.id;
    switch (op) {
      case "comecar":
        if (r.estado !== "lobby") return;
        if (Number.isFinite(m.pos)) r.pos0 = num(m.pos, 0, 86400, r.pos0);
        this.comecar(autor, "comecar", out);
        return;
      case "play":
        if (r.estado === "lobby") { this.comecar(autor, "comecar", out); return; }
        if (r.estado !== "pausado" && r.estado !== "esperando") return;
        s.espera = { quem: [], ate: 0 };
        this.mudarRel({ estado: "tocando", t0: t + this.lead() }, autor, "play");
        break;
      case "pausa": {
        if (r.estado !== "tocando" && r.estado !== "esperando") return;
        const alvo = this.posAlvo(t);
        // A pausa cai no quadro de quem apertou, se ele estava perto do relogio.
        const pos = Number.isFinite(m.pos) && Math.abs(m.pos - alvo) < 5 ? m.pos : alvo;
        s.espera = { quem: [], ate: 0 };
        this.mudarRel({ estado: "pausado", pos0: Math.max(0, pos), t0: t }, autor, "pausa");
        break;
      }
      case "seek": {
        if (!Number.isFinite(m.pos)) return;
        const pos = num(m.pos, 0, 86400, 0);
        const de = this.posAlvo(t);
        if (r.estado === "tocando") this.mudarRel({ pos0: pos, t0: t + this.lead() }, autor, "seek");
        else this.mudarRel({ pos0: pos, t0: t }, autor, "seek");
        r.delta = Math.round(pos - de);
        break;
      }
      case "proximo": {
        const ep = txt(m.ep, 64);
        if (!ep || ep === r.ep) return;
        s.espera = { quem: [], ate: 0 };
        for (const x of Object.values(s.membros)) { x.alc = false; x.alcAte = 0; x.carregDesde = 0; if (x.on) x.st = "procurando"; }
        this.mudarRel({ ep, estado: "lobby", pos0: 0, t0: t, auto: t + LIMITES.autoProximoMs }, autor, "proximo");
        break;
      }
      case "encerrar":
        this.encerrar("anfitriao", out);
        return;
      case "controle":
        if (m.valor !== "todos" && m.valor !== "anfitriao") return;
        s.controle = m.valor;
        break;
      case "espera":
        if (![10, 20, 60].includes(m.valor)) return;
        s.cfg.espera = m.valor;
        break;
      default: return;
    }
    const snap = this.snapshot();
    if (op === "seek") snap.rel.delta = s.rel.delta;
    out.envios.push({ para: "*", msg: snap });
  }

  // ---- tempo -----------------------------------------------------------------------
  // O instante (ms) do proximo prazo, ou 0 se nada pendente.
  proximoAlarme() {
    if (!this.aberta()) return 0;
    const s = this.s;
    const ts = [s.criado + LIMITES.tetoMs];
    if (s.vaziaDesde) ts.push(s.vaziaDesde + LIMITES.vaziaMs);
    if (s.anfSaiu) ts.push(s.anfSaiu + LIMITES.anfitriaoMs);
    if (s.espera.ate) ts.push(s.espera.ate);
    if (s.rel.auto) ts.push(s.rel.auto);
    for (const m of Object.values(s.membros)) {
      if (m.carregDesde) ts.push(m.carregDesde + LIMITES.carregandoMs);
      if (m.alcAte) ts.push(m.alcAte);
      if (!m.on && m.saiuEm) ts.push(m.saiuEm + LIMITES.ausenteMs);
    }
    return Math.min(...ts);
  }

  alarme() {
    const out = { envios: [], fechar: [] };
    if (!this.aberta()) return out;
    const s = this.s;
    const t = this.agora();
    if (t >= s.criado + LIMITES.tetoMs) { this.encerrar("tempo", out); return out; }
    if (s.vaziaDesde && t >= s.vaziaDesde + LIMITES.vaziaMs) { this.encerrar("vazia", out); return out; }
    let mudou = false;
    if (s.anfSaiu && t >= s.anfSaiu + LIMITES.anfitriaoMs) {
      if (!this.passarAnfitriao(out)) s.anfSaiu = 0;
      mudou = true;
    }
    for (const m of Object.values(s.membros)) {
      if (!m.on && m.saiuEm && t >= m.saiuEm + LIMITES.ausenteMs) {
        delete s.membros[m.id];
        mudou = true;
        continue;
      }
      if (m.alcAte && t >= m.alcAte) {
        m.alc = false; m.alcAte = 0;
        out.envios.push({ para: "*", msg: { t: "membro", ...this.membroPub(m) } });
      }
      if (m.carregDesde && t >= m.carregDesde + LIMITES.carregandoMs) {
        m.carregDesde = 0;
        if (s.rel.estado !== "tocando" && s.rel.estado !== "esperando") continue;
        m.esperas = m.esperas.filter((x) => t - x < LIMITES.esperasJanelaMs);
        if (m.esperas.length >= LIMITES.esperasMax) {
          // Teto de esperas: a terceira vira "alcancando" direto (6.6).
          m.alc = true; m.alcAte = 0;
          out.envios.push({ para: "*", msg: { t: "aviso", k: "seguindo", quem: m.id } });
          out.envios.push({ para: "*", msg: { t: "membro", ...this.membroPub(m) } });
          continue;
        }
        m.esperas.push(t);
        if (!s.espera.quem.includes(m.id)) s.espera.quem.push(m.id);
        if (s.rel.estado === "tocando") {
          // Todos param no mesmo quadro.
          s.espera.ate = t + s.cfg.espera * 1000;
          this.mudarRel({ estado: "esperando", pos0: this.posAlvo(t), t0: t }, m.id, "esperar");
        }
        out.envios.push({ para: "*", msg: { t: "aviso", k: "esperando", quem: m.id, prazo: s.cfg.espera } });
        mudou = true;
      }
    }
    if (s.espera.ate && t >= s.espera.ate) {
      for (const id of s.espera.quem) {
        const m = s.membros[id];
        if (m) { m.alc = true; m.alcAte = 0; }
        out.envios.push({ para: "*", msg: { t: "aviso", k: "seguindo", quem: id } });
      }
      s.espera = { quem: [], ate: 0 };
      if (s.rel.estado === "esperando") this.mudarRel({ estado: "tocando", t0: t + this.lead() }, "", "prazo");
      mudou = true;
    }
    if (s.rel.auto && t >= s.rel.auto && s.rel.estado === "lobby") {
      this.comecar(s.anf, "auto", out);
      return out;
    }
    if (mudou) out.envios.push({ para: "*", msg: this.snapshot() });
    // Nonces vencidos nao precisam mais ser lembrados.
    for (const [n, exp] of Object.entries(s.usados)) if (exp * 1000 < t) delete s.usados[n];
    return out;
  }
}

const COD_ALFA = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
function codigoCelular() {
  return [...crypto.getRandomValues(new Uint8Array(8))].map((x) => COD_ALFA[x & 31]).join("");
}

function dest(cx) { return (cx.papel === "cel" ? "cel:" : "tv:") + cx.pessoa; }
