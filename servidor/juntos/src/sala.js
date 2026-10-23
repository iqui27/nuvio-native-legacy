// O Durable Object `Sala`: liga a regra (logica.js) aos websockets e ao storage.
//
// HIBERNACAO (7.4). Os sockets sao aceitos com ctx.acceptWebSocket, entao o
// objeto pode dormir com todos abertos sem cobrar duracao. Ao acordar, o
// construtor relê o estado do storage; a identidade de cada socket mora no
// "attachment" dele. O estado so e gravado quando muda (nunca por ping).
import { DurableObject } from "cloudflare:workers";
import { conferir } from "./bilhete.js";
import { SalaLogica } from "./logica.js";

const MSG_MAX = 2048;   // mensagem maior fecha o socket (1009)

export class Sala extends DurableObject {
  constructor(ctx, env) {
    super(ctx, env);
    this.l = new SalaLogica({ max: Math.max(2, Math.min(10, parseInt(env.SALA_MAX || "5", 10) || 5)) });
    this.salvo = "";
    ctx.blockConcurrencyWhile(async () => {
      const s = await ctx.storage.get("s");
      this.l.restaurar(s || null);
      this.salvo = JSON.stringify(s || null);
    });
  }

  async fetch(req) {
    if ((req.headers.get("upgrade") || "").toLowerCase() !== "websocket")
      return new Response("websocket", { status: 426 });
    const url = new URL(req.url);
    const agoraS = Math.floor(Date.now() / 1000);
    let b = null;
    let chaveCelular = "";
    if (url.searchParams.get("b")) {
      b = await conferir([this.env.SALA_SEGREDO, this.env.SALA_SEGREDO_ANTIGO], url.searchParams.get("b"), agoraS);
    } else if (url.searchParams.get("c")) {
      b = this.l.resgatarCelular(url.searchParams.get("c"));
      if (b) chaveCelular = chaveNova();
    } else if (url.searchParams.get("k")) {
      // Celular reconectando: o QR so vale uma vez; depois e esta chave de sessao.
      const k = url.searchParams.get("k");
      const s = this.l.serializar();
      const m = s && s.membros ? Object.values(s.membros).find((x) => x.celK && x.celK === k) : null;
      if (m) { b = { pessoa: m.id, papel: "celular", chat: m.chat, sala: s.sala }; chaveCelular = k; }
    }
    const par = new WebSocketPair();
    const [cliente, servidor] = Object.values(par);
    if (!b) return recusar(par, 4401, "bilhete");
    const r = this.l.conectar(b, { chaveCelular });
    if (!r.ok) {
      await this.depois(r);
      return recusar(par, r.codigo, r.motivo);
    }
    // Fecha os sockets substituidos ANTES de aceitar o novo: eles tem a mesma tag.
    this.fechar(r.fechar);
    this.ctx.acceptWebSocket(servidor, [r.cx.pessoa]);
    servidor.serializeAttachment(r.cx);
    await this.depois({ envios: r.envios, fechar: [] });
    return new Response(null, { status: 101, webSocket: cliente });
  }

  async webSocketMessage(ws, dados) {
    const cx = ws.deserializeAttachment();
    if (!cx || cx.morto) return;
    if (typeof dados !== "string" || dados.length > MSG_MAX) { this.matar(ws, 1009, "grande"); return; }
    let m;
    try { m = JSON.parse(dados); } catch { return; }
    await this.depois(this.l.mensagem(cx, m));
  }

  async webSocketClose(ws) { await this.caiu(ws); }
  async webSocketError(ws) { await this.caiu(ws); }

  async caiu(ws) {
    const cx = ws.deserializeAttachment();
    if (this.env.JUNTOS_DEPURAR) console.log("[juntos] caiu", JSON.stringify(cx));
    if (!cx || cx.morto) return;
    ws.serializeAttachment({ ...cx, morto: 1 });
    // Ainda ha outro socket vivo do mesmo papel (reconexao chegou antes do
    // fechamento do velho): a pessoa nao saiu.
    const vivo = this.ctx.getWebSockets(cx.pessoa).some((o) => {
      if (o === ws) return false;
      const a = o.deserializeAttachment();
      return a && !a.morto && a.papel === cx.papel;
    });
    if (vivo) return;
    await this.depois(this.l.desconectar(cx));
  }

  async alarm() {
    await this.depois(this.l.alarme());
  }

  matar(ws, codigo, motivo) {
    const cx = ws.deserializeAttachment();
    ws.serializeAttachment({ ...(cx || {}), morto: 1 });
    try { ws.close(codigo, motivo); } catch {}
    if (cx && !cx.morto) this.depois(this.l.desconectar(cx));
  }

  alvos(para) {
    const todos = this.ctx.getWebSockets();
    const vivos = (lista) => lista.filter((w) => { const a = w.deserializeAttachment(); return a && !a.morto; });
    if (para === "*") {
      const s = this.l.serializar();
      // Sala encerrada: o "fim" vai para todo socket que ainda era membro.
      return vivos(todos).filter((w) => {
        const a = w.deserializeAttachment();
        return a.pessoa && !a.pend && (!s || !s.membros || s.membros[a.pessoa]);
      });
    }
    const i = para.indexOf(":");
    const tipo = para.slice(0, i), id = para.slice(i + 1);
    return vivos(this.ctx.getWebSockets(id)).filter((w) => {
      const a = w.deserializeAttachment();
      return tipo === "pessoa" || a.papel === tipo;
    });
  }

  fechar(lista) {
    for (const f of lista || []) {
      const ws = f.para === "*" ? this.ctx.getWebSockets() : this.alvos(f.para);
      for (const w of ws) {
        const a = w.deserializeAttachment() || {};
        if (this.env.JUNTOS_DEPURAR) console.log("[juntos] fechar", f.para, f.codigo, a.pessoa, a.papel);
        w.serializeAttachment({ ...a, morto: 1 });
        try { w.close(f.codigo, f.motivo); } catch {}
      }
    }
  }

  async depois(out) {
    for (const e of out.envios || []) {
      const txt = JSON.stringify(e.msg);
      for (const w of this.alvos(e.para)) { try { w.send(txt); } catch {} }
    }
    this.fechar(out.fechar);
    const s = this.l.serializar();
    const j = JSON.stringify(s);
    if (j !== this.salvo) {
      if (this.l.encerrada()) {
        await this.ctx.storage.deleteAll();
        await this.ctx.storage.put("s", s);   // so a lapide
      } else {
        await this.ctx.storage.put("s", s);
      }
      this.salvo = j;
    }
    const prox = this.l.proximoAlarme();
    if (prox) {
      const atual = await this.ctx.storage.getAlarm();
      if (atual !== prox) await this.ctx.storage.setAlarm(prox);
    } else {
      await this.ctx.storage.deleteAlarm();
    }
  }
}

// Recusa com motivo legivel: o socket recusado NAO entra na hibernacao (nao e
// membro de nada); aceito a moda antiga, recebe o erro e o codigo, e acabou.
function recusar(par, codigo, motivo) {
  const [cliente, servidor] = Object.values(par);
  servidor.accept();
  servidor.send(JSON.stringify({ t: "erro", k: motivo }));
  servidor.close(codigo, motivo);
  return new Response(null, { status: 101, webSocket: cliente });
}

function chaveNova() {
  const b = crypto.getRandomValues(new Uint8Array(18));
  return [...b].map((x) => x.toString(16).padStart(2, "0")).join("");
}
