// Regra da sala (src/logica.js) com relogio falso, sem Cloudflare. Rapido:
//   node --test servidor/juntos/teste-logica.mjs
import assert from "node:assert/strict";
import test from "node:test";
import { SalaLogica, LIMITES, limparImpressao } from "./src/logica.js";

const SALA = "0123456789abcdef0123456789abcdef";
let nn = 0;
const bil = (pessoa, papel = "convidado", extra = {}) =>
  ({ v: 1, sala: SALA, pessoa, nome: pessoa.toUpperCase(), av: "", papel, exp: 9e9, n: "n" + ++nn, ...extra });
const anf = (extra = {}) => bil("h", "anfitriao", { cfg: { ep: "tt1", titulo: "Duna", imp: { hash: "a".repeat(40), idx: 0 },
  codigo: "K7M2QX", ...extra } });

function sala(max = 5) {
  const rel = { t: 1_000_000 };
  const l = new SalaLogica({ agora: () => rel.t, max });
  return { l, rel, andar: (ms) => { rel.t += ms; } };
}
const msgs = (out, tipo, para) => out.envios.filter((e) => e.msg.t === tipo && (!para || e.para === para)).map((e) => e.msg);
const ultimaSala = (out) => msgs(out, "sala").at(-1);
const tv = (p) => ({ pessoa: p, papel: "tv" });

function tres() {
  const x = sala();
  assert.ok(x.l.conectar(anf()).ok);
  assert.ok(x.l.conectar(bil("r")).ok);
  assert.ok(x.l.conectar(bil("f")).ok);
  for (const p of ["h", "r", "f"]) x.l.mensagem(tv(p), { t: "estado", st: "pronto" });
  return x;
}

test("sala so nasce do anfitriao; convidado antes recebe 4404; bilhete reusado e recusado", () => {
  const { l } = sala();
  assert.equal(l.conectar(bil("r")).codigo, 4404);
  const b = anf();
  const r = l.conectar(b);
  assert.ok(r.ok);
  assert.equal(ultimaSala(r).rel.estado, "lobby");
  assert.equal(ultimaSala(r).cfg.titulo, "Duna");
  assert.equal(l.conectar(b).codigo, 4401);
});

test("impressao: so os campos permitidos, nada de URL", () => {
  const i = limparImpressao({ hash: "A".repeat(40), idx: 2, url: "https://rd/x", arquivo: "https://x/y.mkv",
    bytes: 123.7, binge: "torrentio|1080p", durMs: 5000, chave: "seg" });
  assert.deepEqual(i, { hash: "a".repeat(40), idx: 2, bytes: 123, binge: "torrentio|1080p", durMs: 5000 });
});

test("comecar: play agendado no futuro com lead = max(2xRTT, 600 ms) e seq sobe", () => {
  const x = tres();
  x.l.mensagem(tv("r"), { t: "ola", plat: "wgt", nivel: "B", rtt: 450 });
  const seq0 = x.l.s.rel.seq;
  const out = x.l.mensagem(tv("f"), { t: "cmd", op: "comecar", seqBase: seq0 });
  const s = ultimaSala(out);
  assert.equal(s.rel.estado, "tocando");
  assert.equal(s.rel.t0, x.rel.t + 900);
  // a TV que demora 1,4 s de um seek ate tocar empurra a folga
  x.l.mensagem(tv("h"), { t: "estado", st: "tocando", prep: 1400 });
  assert.equal(x.l.lead(), 1550);
  assert.equal(s.seq, seq0 + 1);
  assert.equal(s.rel.autor, "f");
  // Quem entra no meio extrapola pelo mesmo par (o bug #8579 do Jellyfin).
  x.andar(900 + 61_500);
  assert.equal(x.l.posAlvo(), 61.5);
  const e = x.l.conectar(bil("d"));
  const sd = ultimaSala(e);
  assert.equal(sd.rel.pos0 + (sd.s - sd.rel.t0) / 1000, 61.5);
  assert.ok(sd.membros.find((m) => m.id === "d").alc, "quem entra no meio comeca alcancando");
});

test("pausa cai no quadro de quem apertou (se perto) e o seqBase velho e descartado", () => {
  const x = tres();
  x.l.mensagem(tv("h"), { t: "cmd", op: "comecar" });
  x.andar(10_600);
  const p = ultimaSala(x.l.mensagem(tv("r"), { t: "cmd", op: "pausa", pos: 9.8, seqBase: x.l.s.rel.seq }));
  assert.equal(p.rel.estado, "pausado");
  assert.equal(p.rel.pos0, 9.8);
  // longe demais do relogio: vale o relogio
  x.l.mensagem(tv("r"), { t: "cmd", op: "play" });
  x.andar(5000);
  const alvo = x.l.posAlvo();
  const p2 = ultimaSala(x.l.mensagem(tv("f"), { t: "cmd", op: "pausa", pos: alvo + 30 }));
  assert.equal(p2.rel.pos0, alvo);
  // tres mudancas em menos de 1 s e um comando que viu o seq de antes delas
  const base = x.l.s.rel.seq;
  x.l.mensagem(tv("h"), { t: "cmd", op: "play" });
  x.l.mensagem(tv("h"), { t: "cmd", op: "seek", pos: 100 });
  x.l.mensagem(tv("h"), { t: "cmd", op: "seek", pos: 120 });
  const v = x.l.mensagem(tv("r"), { t: "cmd", op: "pausa", seqBase: base });
  assert.equal(msgs(v, "recusado")[0].k, "velho");
  assert.equal(x.l.s.rel.estado, "tocando");
  // depois de 1 s o mesmo seqBase passa (ultimo vence)
  x.andar(1100);
  x.l.mensagem(tv("r"), { t: "cmd", op: "pausa", seqBase: base });
  assert.equal(x.l.s.rel.estado, "pausado");
});

test("seek tocando reagenda t0 e informa o delta (aviso 'voltou 30 s')", () => {
  const x = tres();
  x.l.mensagem(tv("h"), { t: "cmd", op: "comecar" });
  x.andar(60_600);
  const s = ultimaSala(x.l.mensagem(tv("f"), { t: "cmd", op: "seek", pos: 30 }));
  assert.equal(s.rel.pos0, 30);
  assert.equal(s.rel.delta, -30);
  assert.equal(s.rel.op, "seek");
  assert.ok(s.rel.t0 > x.rel.t);
});

test("trava do anfitriao: convidado recebe so_anfitriao; pedir pausa chega ao anfitriao", () => {
  const x = tres();
  x.l.mensagem(tv("h"), { t: "cmd", op: "controle", valor: "anfitriao" });
  const r = x.l.mensagem(tv("r"), { t: "cmd", op: "comecar" });
  assert.equal(msgs(r, "recusado")[0].k, "so_anfitriao");
  assert.equal(x.l.s.rel.estado, "lobby");
  const pp = x.l.mensagem(tv("r"), { t: "pedir", k: "pausa" });
  assert.deepEqual(pp.envios[0], { para: "pessoa:h", msg: { t: "pedir", k: "pausa", de: "r" } });
  // convidado nao pode encerrar nem trocar a trava
  assert.equal(msgs(x.l.mensagem(tv("r"), { t: "cmd", op: "encerrar" }), "recusado")[0].k, "so_anfitriao");
  x.l.mensagem(tv("r"), { t: "cmd", op: "controle", valor: "todos" });
  assert.equal(x.l.s.controle, "anfitriao");
});

test("carregando > 3 s vira esperando (todos param no mesmo quadro); voltar a pronto retoma agendado", () => {
  const x = tres();
  x.l.mensagem(tv("h"), { t: "cmd", op: "comecar" });
  x.andar(20_600);
  x.l.mensagem(tv("f"), { t: "estado", st: "carregando", pct: 40 });
  x.andar(2000);
  assert.equal(x.l.alarme().envios.length, 0, "travada curta nao para ninguem");
  assert.equal(x.l.proximoAlarme(), x.rel.t + 1000);
  x.andar(1000);
  const a = x.l.alarme();
  const s = ultimaSala(a);
  assert.equal(s.rel.estado, "esperando");
  assert.equal(s.rel.pos0, 23);
  assert.deepEqual(s.rel.esperando, ["f"]);
  assert.equal(msgs(a, "aviso")[0].k, "esperando");
  x.andar(4000);
  const r = ultimaSala(x.l.mensagem(tv("f"), { t: "estado", st: "pronto" }));
  assert.equal(r.rel.estado, "tocando");
  assert.equal(r.rel.pos0, 23);
  assert.equal(r.rel.t0, x.rel.t + 600);
});

test("prazo de 20 s: quem trava vira alcancando e a sala segue; teto de 2 esperas em 10 min", () => {
  const x = tres();
  x.l.mensagem(tv("h"), { t: "cmd", op: "comecar" });
  x.andar(10_000);
  x.l.mensagem(tv("f"), { t: "estado", st: "carregando" });
  x.andar(3000); x.l.alarme();
  assert.equal(x.l.s.rel.estado, "esperando");
  x.andar(20_000);
  const a = x.l.alarme();
  assert.equal(x.l.s.rel.estado, "tocando");
  assert.equal(msgs(a, "aviso").at(-1).k, "seguindo");
  assert.ok(x.l.s.membros.f.alc);
  // alcancando nao trava a sala
  x.l.mensagem(tv("f"), { t: "estado", st: "carregando" });
  x.andar(5000); x.l.alarme();
  assert.equal(x.l.s.rel.estado, "tocando");
  // 30 s estavel tocando: volta ao normal
  x.l.mensagem(tv("f"), { t: "estado", st: "tocando" });
  x.andar(30_000); x.l.alarme();
  assert.equal(x.l.s.membros.f.alc, false);
  // segunda espera (ainda dentro dos 10 min) e retomada
  x.l.mensagem(tv("f"), { t: "estado", st: "carregando" });
  x.andar(3000); x.l.alarme();
  assert.equal(x.l.s.rel.estado, "esperando");
  x.l.mensagem(tv("f"), { t: "estado", st: "pronto" });
  // terceira: alcancando direto, ninguem para
  x.l.mensagem(tv("f"), { t: "estado", st: "tocando" });
  x.l.mensagem(tv("f"), { t: "estado", st: "carregando" });
  x.andar(3000);
  const t3 = x.l.alarme();
  assert.equal(x.l.s.rel.estado, "tocando");
  assert.ok(x.l.s.membros.f.alc);
  assert.equal(msgs(t3, "aviso")[0].k, "seguindo");
});

test("comecar com alguem carregando: ele entra alcancando", () => {
  const x = sala();
  x.l.conectar(anf()); x.l.conectar(bil("r"));
  x.l.mensagem(tv("h"), { t: "estado", st: "pronto" });
  x.l.mensagem(tv("r"), { t: "estado", st: "carregando", pct: 10 });
  x.l.mensagem(tv("h"), { t: "cmd", op: "comecar" });
  assert.ok(x.l.s.membros.r.alc);
  assert.ok(!x.l.s.membros.h.alc);
});

test("anfitriao cai: 30 s depois o proximo que entrou controla; sair passa na hora", () => {
  const x = tres();
  x.l.desconectar(tv("h"));
  x.andar(29_000);
  assert.equal(x.l.alarme().envios.length, 0);
  x.andar(1000);
  const a = x.l.alarme();
  assert.deepEqual(msgs(a, "aviso")[0], { t: "aviso", k: "anfitriao", quem: "r" });
  assert.equal(x.l.s.anf, "r");
  const s = x.l.mensagem(tv("r"), { t: "sair" });
  assert.equal(x.l.s.anf, "f");
  assert.ok(!x.l.s.membros.r);
  assert.ok(s.fechar.some((f) => f.para === "pessoa:r"));
});

test("sala cheia: a 6a TV recebe 4409; celular nao conta", () => {
  const x = sala(5);
  x.l.conectar(anf());
  for (const p of ["a", "b", "c", "d"]) assert.ok(x.l.conectar(bil(p)).ok);
  assert.ok(x.l.conectar(bil("a", "celular")).ok);
  assert.equal(x.l.conectar(bil("e")).codigo, 4409);
  // quem ja esta reconecta mesmo cheia
  assert.ok(x.l.conectar(bil("b")).ok);
});

test("entrada por codigo sem amizade: pedido ao anfitriao, aceitar e reconectar", () => {
  const x = sala();
  x.l.conectar(anf());
  const p = x.l.conectar(bil("z", "convidado", { pend: 1 }));
  assert.ok(p.cx.pend);
  assert.deepEqual(msgs(p, "pedido", "tv:h")[0].quem, { id: "z", nome: "Z", av: "" });
  assert.equal(x.l.mensagem(p.cx, { t: "cmd", op: "comecar" }).envios.length, 0);
  const ac = x.l.mensagem(tv("h"), { t: "aceitar", quem: "z", ok: 1 });
  assert.equal(ac.fechar[0].codigo, 4100);
  x.l.desconectar(p.cx);
  const de = x.l.conectar(bil("z", "convidado", { pend: 1 }));
  assert.ok(de.ok && !de.cx.pend);
  assert.ok(x.l.s.membros.z);
  // recusar fecha com 4403
  const q = x.l.conectar(bil("w", "convidado", { pend: 1 }));
  const rc = x.l.mensagem(tv("h"), { t: "aceitar", quem: "w", ok: 0 });
  assert.equal(rc.fechar[0].codigo, 4403);
  x.l.desconectar(q.cx);
  assert.ok(!x.l.s.pend.w);
});

test("reacoes: so chaves conhecidas, 1 a cada 2 s; chat so do celular e so se liberado", () => {
  const x = tres();
  assert.equal(x.l.mensagem(tv("r"), { t: "reacao", k: "risada" }).envios[0].msg.k, "risada");
  assert.equal(x.l.mensagem(tv("r"), { t: "reacao", k: "coracao" }).envios.length, 0);
  x.andar(2000);
  assert.equal(x.l.mensagem(tv("r"), { t: "reacao", k: "<script>" }).envios.length, 0);
  assert.equal(x.l.mensagem(tv("r"), { t: "frase", k: "ja_volto" }).envios[0].msg.k, "ja_volto");
  // chat da TV: ignorado
  assert.equal(x.l.mensagem(tv("r"), { t: "chat", txt: "oi" }).envios.length, 0);
  x.l.conectar(bil("r", "celular", { chat: 1 }), { chaveCelular: "k1" });
  x.l.conectar(bil("f", "celular", { chat: 0 }), { chaveCelular: "k2" });
  const c = x.l.mensagem({ pessoa: "r", papel: "cel" }, { t: "chat", txt: "  oi\u0000 gente " + "x".repeat(300) });
  assert.equal(c.envios[0].msg.txt.length, 200);
  assert.ok(c.envios[0].msg.txt.startsWith("oi  gente"));
  assert.equal(x.l.mensagem({ pessoa: "f", papel: "cel" }, { t: "chat", txt: "oi" }).envios.length, 0);
  // celular de quem nao esta na sala
  assert.equal(x.l.conectar(bil("q", "celular")).codigo, 4403);
});

test("proximo episodio: lobby com mini-barreira de 15 s; todos prontos comeca antes", () => {
  const x = tres();
  x.l.mensagem(tv("h"), { t: "cmd", op: "comecar" });
  x.andar(5000);
  const p = ultimaSala(x.l.mensagem(tv("r"), { t: "cmd", op: "proximo", ep: "tt1:1:2" }));
  assert.equal(p.rel.estado, "lobby");
  assert.equal(p.rel.ep, "tt1:1:2");
  assert.equal(p.rel.auto, x.rel.t + 15_000);
  for (const q of ["h", "r"]) x.l.mensagem(tv(q), { t: "estado", st: "pronto" });
  assert.equal(x.l.s.rel.estado, "lobby");
  const ult = x.l.mensagem(tv("f"), { t: "estado", st: "pronto" });
  assert.equal(ultimaSala(ult).rel.estado, "tocando");
  // e sem todos prontos, o prazo comeca sozinho
  x.l.mensagem(tv("r"), { t: "cmd", op: "proximo", ep: "tt1:1:3" });
  x.l.mensagem(tv("h"), { t: "estado", st: "pronto" });
  x.andar(15_000);
  x.l.alarme();
  assert.equal(x.l.s.rel.estado, "tocando");
  assert.ok(x.l.s.membros.f.alc);
});

test("vazia 10 min encerra e deixa so a lapide; encerrar pelo anfitriao; sem renascer", () => {
  const x = tres();
  for (const p of ["h", "r", "f"]) x.l.desconectar(tv(p));
  x.andar(LIMITES.vaziaMs);
  const a = x.l.alarme();
  assert.equal(msgs(a, "fim")[0].motivo, "vazia");
  assert.deepEqual(Object.keys(x.l.serializar()).sort(), ["fim", "motivo", "sala"]);
  assert.equal(x.l.conectar(anf()).codigo, 4410);
  const y = tres();
  const e = y.l.mensagem(tv("h"), { t: "cmd", op: "encerrar" });
  assert.equal(msgs(e, "fim")[0].motivo, "anfitriao");
  assert.ok(y.l.encerrada());
});

test("QR do celular: codigo curto de uso unico e 10 min; crianca sem chat; so membro", () => {
  const x = tres();
  const r = x.l.mensagem(tv("r"), { t: "celular", chat: 1 });
  const c = r.envios[0].msg.codigo;
  assert.match(c, /^[0-9A-HJKMNP-TV-Z]{8}$/);
  assert.equal(("https://nuvio-juntos.henriquef29.workers.dev/j/" + "0".repeat(32) + c).length < 134, true);
  assert.equal(x.l.mensagem(tv("r"), { t: "celular", chat: 1 }).envios.length, 0, "1 a cada 3 s");
  const b = x.l.resgatarCelular(c);
  assert.deepEqual([b.pessoa, b.papel, b.chat], ["r", "celular", 1]);
  assert.equal(x.l.resgatarCelular(c), null, "uso unico");
  x.andar(3000);
  const c2 = x.l.mensagem(tv("r"), { t: "celular", chat: 1 }).envios[0].msg.codigo;
  x.andar(LIMITES.celularMs + 1);
  assert.equal(x.l.resgatarCelular(c2), null, "vencido");
  const k = x.l.conectar(bil("k", "convidado", { inf: 1 }));
  assert.ok(k.ok);
  const ck = x.l.mensagem(tv("k"), { t: "celular", chat: 1 }).envios[0].msg.codigo;
  assert.equal(x.l.resgatarCelular(ck).chat, 0, "perfil infantil: chat desligado");
});

test("rajada de comandos: mais de 5 por segundo e recusada", () => {
  const x = tres();
  let recusados = 0;
  for (let i = 0; i < 8; i++)
    recusados += msgs(x.l.mensagem(tv("r"), { t: "cmd", op: "seek", pos: i }), "recusado").length;
  assert.equal(recusados, 3);
});
