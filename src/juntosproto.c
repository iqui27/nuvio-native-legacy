// Watch Together: mensagens da sala — ver juntosproto.h.
#include "juntosproto.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *jsp_esc(char *dst, size_t tam, const char *s) {
  size_t k = 0;
  if (!tam) return dst;
  for (; s && *s && k + 7 < tam; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') { dst[k++] = '\\'; dst[k++] = (char)c; }
    else if (c < 0x20) k += (size_t)snprintf(dst + k, tam - k, "\\u%04x", c);
    else dst[k++] = (char)c;
  }
  dst[k] = 0;
  return dst;
}

// O objeto de nome `chave` em [ini,fim): ponteiro para o '{' ou NULL. As
// mensagens sao nossas (JSON.stringify, sem espaco), e cada nome de objeto
// aparece uma vez so.
static const char *objeto(const char *ini, const char *fim, const char *chave) {
  char pad[40];
  const char *p;
  size_t n;
  snprintf(pad, sizeof pad, "\"%s\":", chave);
  n = strlen(pad);
  if (!fim) fim = ini + strlen(ini);
  for (p = ini; p && p + n < fim; p++) {
    p = strstr(p, pad);
    if (!p || p + n >= fim) return NULL;
    if (p[n] == '{') return p + n;
  }
  return NULL;
}

static int bool_(const char *ini, const char *fim, const char *chave) {
  char pad[40];
  const char *p;
  snprintf(pad, sizeof pad, "\"%s\":true", chave);
  p = strstr(ini, pad);
  return p && (!fim || p < fim);
}

void jsp_ler_impressao(const char *ini, const char *fim, JsImpressao *i) {
  const char *o = objeto(ini, fim, "imp"), *f;
  memset(i, 0, sizeof *i);
  i->idx = -1;
  if (!o) return;
  f = js_fim(o);
  js_texto(o, f, "hash", i->hash, sizeof i->hash);
  i->idx = (int)js_num(o, f, "idx", -1);
  i->bytes = (long long)js_num(o, f, "bytes", 0);
  js_texto(o, f, "arquivo", i->arquivo, sizeof i->arquivo);
  js_texto(o, f, "binge", i->binge, sizeof i->binge);
  i->altura = (int)js_num(o, f, "altura", 0);
  i->durMs = (long long)js_num(o, f, "durMs", 0);
  js_texto(o, f, "rotulo", i->rotulo, sizeof i->rotulo);
}

static void lerMembro(const char *o, const char *f, JsMembro *m) {
  char nv[4] = "";
  memset(m, 0, sizeof *m);
  js_texto(o, f, "id", m->id, sizeof m->id);
  js_texto(o, f, "nome", m->nome, sizeof m->nome);
  js_texto(o, f, "av", m->av, sizeof m->av);
  js_texto(o, f, "st", m->st, sizeof m->st);
  m->pct = (int)js_num(o, f, "pct", 0);
  js_texto(o, f, "nivel", nv, sizeof nv);
  m->nivel = nv[0] == 'A' || nv[0] == 'B' ? nv[0] : 0;
  js_texto(o, f, "plat", m->plat, sizeof m->plat);
  js_texto(o, f, "fonte", m->fonte, sizeof m->fonte);
  m->anf = bool_(o, f, "anf");
  m->on = bool_(o, f, "on");
  m->alc = bool_(o, f, "alc");
  m->cel = bool_(o, f, "cel");
}

static void lerSala(const char *j, JsSala *s) {
  const char *r = objeto(j, NULL, "rel"), *rf, *a, *cfg, *cf;
  char est[16] = "", ctl[16] = "";
  memset(s, 0, sizeof *s);
  s->seq = (int)js_num(j, NULL, "seq", 0);
  s->s = js_num(j, NULL, "s", 0);
  s->max = (int)js_num(j, NULL, "max", 5);
  js_texto(j, NULL, "controle", ctl, sizeof ctl);
  s->controleAnf = !strcmp(ctl, "anfitriao");
  if (r) {
    rf = js_fim(r);
    js_texto(r, rf, "estado", est, sizeof est);
    s->rel.estado = jsr_estado_de(est);
    s->rel.pos0 = js_num(r, rf, "pos0", 0);
    s->rel.t0 = js_num(r, rf, "t0", 0);
    s->rel.seq = s->seq;
    js_texto(r, rf, "ep", s->ep, sizeof s->ep);
    js_texto(r, rf, "autor", s->autor, sizeof s->autor);
    js_texto(r, rf, "op", s->op, sizeof s->op);
    s->delta = js_num(r, rf, "delta", 0);
    s->autoMs = js_num(r, rf, "auto", 0);
    s->prazo = js_num(r, rf, "prazo", 0);
    a = js_array(r, rf, "esperando");
    while (a && *a == '"' && s->nEsperando < JSP_MEMBROS) {
      const char *q = a + 1;
      js_cadeia(a, s->esperando[s->nEsperando++], JSP_ID);
      while (*q && *q != '"') q += *q == '\\' && q[1] ? 2 : 1;   // fim desta string
      if (!*q) break;
      a = q + 1;
      if (*a != ',') break;
      a++;
    }
  }
  a = js_array(j, NULL, "membros");
  while (a && *a == '{' && s->n < JSP_MEMBROS) {
    const char *f = js_fim(a);
    lerMembro(a, f, &s->m[s->n++]);
    a = js_prox(f);
  }
  cfg = objeto(j, NULL, "cfg");
  if (cfg) {
    cf = js_fim(cfg);
    js_texto(cfg, cf, "titulo", s->titulo, sizeof s->titulo);
    js_texto(cfg, cf, "poster", s->poster, sizeof s->poster);
    js_texto(cfg, cf, "codigo", s->codigo, sizeof s->codigo);
    s->espera = (int)js_num(cfg, cf, "espera", 20);
    jsp_ler_impressao(cfg, cf, &s->imp);
  }
}

JsMsgTipo jsp_ler(const char *j, JsMsg *m, JsSala *sala) {
  char t[16] = "";
  memset(m, 0, sizeof *m);
  if (!j || !js_texto_raiz(j, "t", t, sizeof t)) return JSM_NADA;
  if (!strcmp(t, "pong")) {
    m->tipo = JSM_PONG;
    m->c = js_num(j, NULL, "c", 0);
    m->s = js_num(j, NULL, "s", 0);
  } else if (!strcmp(t, "sala")) {
    m->tipo = JSM_SALA;
    if (sala) lerSala(j, sala);
  } else if (!strcmp(t, "membro")) {
    m->tipo = JSM_MEMBRO;
    lerMembro(j, NULL, &m->membro);
  } else if (!strcmp(t, "aviso")) {
    m->tipo = JSM_AVISO;
    js_texto(j, NULL, "k", m->k, sizeof m->k);
    js_texto(j, NULL, "quem", m->quem, sizeof m->quem);
    js_texto(j, NULL, "nome", m->nome, sizeof m->nome);
    m->prazo = (int)js_num(j, NULL, "prazo", 0);
  } else if (!strcmp(t, "reacao") || !strcmp(t, "frase")) {
    m->tipo = t[0] == 'r' ? JSM_REACAO : JSM_FRASE;
    js_texto(j, NULL, "k", m->k, sizeof m->k);
    js_texto(j, NULL, "de", m->quem, sizeof m->quem);
  } else if (!strcmp(t, "chat")) {
    m->tipo = JSM_CHAT;
    js_texto(j, NULL, "de", m->quem, sizeof m->quem);
    js_texto(j, NULL, "txt", m->txt, sizeof m->txt);
  } else if (!strcmp(t, "pedido")) {
    const char *q = objeto(j, NULL, "quem"), *qf;
    m->tipo = JSM_PEDIDO;
    if (q) {
      qf = js_fim(q);
      js_texto(q, qf, "id", m->quem, sizeof m->quem);
      js_texto(q, qf, "nome", m->nome, sizeof m->nome);
      js_texto(q, qf, "av", m->av, sizeof m->av);
    }
  } else if (!strcmp(t, "pedir")) {
    m->tipo = JSM_PEDIR;
    js_texto(j, NULL, "k", m->k, sizeof m->k);
    js_texto(j, NULL, "de", m->quem, sizeof m->quem);
  } else if (!strcmp(t, "recusado") || !strcmp(t, "erro")) {
    m->tipo = t[0] == 'r' ? JSM_RECUSADO : JSM_ERRO;
    js_texto(j, NULL, "k", m->k, sizeof m->k);
    js_texto(j, NULL, "anf", m->quem, sizeof m->quem);
  } else if (!strcmp(t, "fim")) {
    m->tipo = JSM_FIM;
    js_texto(j, NULL, "motivo", m->k, sizeof m->k);
    m->dur = (int)js_num(j, NULL, "dur", 0);
  } else if (!strcmp(t, "aguardando")) m->tipo = JSM_AGUARDANDO;
  else if (!strcmp(t, "aceito")) m->tipo = JSM_ACEITO;
  else if (!strcmp(t, "celular")) {
    m->tipo = JSM_CELULAR;
    js_texto(j, NULL, "codigo", m->k, sizeof m->k);
  }
  return m->tipo;
}

static int fim_(int r, size_t n) { return r < 0 || (size_t)r >= n ? -1 : r; }

int jsp_ping(char *b, size_t n, double c) { return fim_(snprintf(b, n, "{\"t\":\"ping\",\"c\":%.0f}", c), n); }

int jsp_ola(char *b, size_t n, const char *plat, char nivel, double rtt, const char *app) {
  char a[40];
  return fim_(snprintf(b, n, "{\"t\":\"ola\",\"v\":1,\"plat\":\"%s\",\"nivel\":\"%c\",\"rtt\":%.0f,\"app\":\"%s\"}",
                       plat, nivel == 'A' ? 'A' : 'B', rtt, jsp_esc(a, sizeof a, app ? app : "")), n);
}

int jsp_estado(char *b, size_t n, const char *st, int pct, double pos, double dur, const char *fonte, double rtt,
               double prep) {
  char f[64] = "";
  if (fonte && *fonte) snprintf(f, sizeof f, ",\"fonte\":{\"tipo\":\"%s\"}", fonte);
  return fim_(snprintf(b, n, "{\"t\":\"estado\",\"st\":\"%s\",\"pct\":%d,\"pos\":%.3f,\"dur\":%.3f,\"rtt\":%.0f,"
                       "\"prep\":%.0f%s}", st, pct, pos, dur, rtt, prep, f), n);
}

int jsp_cmd(char *b, size_t n, const char *op, double pos, int seqBase, const char *ep, const char *valor) {
  char p[48] = "", e[96] = "", v[48] = "", esc[72];
  if (pos >= 0) snprintf(p, sizeof p, ",\"pos\":%.3f", pos);
  if (ep && *ep) snprintf(e, sizeof e, ",\"ep\":\"%s\"", jsp_esc(esc, sizeof esc, ep));
  // "espera" vai como numero (10/20/60); "controle", como texto.
  if (valor && *valor) {
    if (!strcmp(op, "espera")) snprintf(v, sizeof v, ",\"valor\":%d", atoi(valor));
    else snprintf(v, sizeof v, ",\"valor\":\"%s\"", jsp_esc(esc, sizeof esc, valor));
  }
  return fim_(snprintf(b, n, "{\"t\":\"cmd\",\"op\":\"%s\"%s,\"seqBase\":%d%s%s}", op, p, seqBase, e, v), n);
}

int jsp_simples(char *b, size_t n, const char *t, const char *k) {
  char e[48];
  return fim_(snprintf(b, n, "{\"t\":\"%s\",\"k\":\"%s\"}", t, jsp_esc(e, sizeof e, k)), n);
}

int jsp_aceitar(char *b, size_t n, const char *quem, int ok) {
  char e[JSP_ID * 2];
  return fim_(snprintf(b, n, "{\"t\":\"aceitar\",\"quem\":\"%s\",\"ok\":%d}", jsp_esc(e, sizeof e, quem), ok ? 1 : 0), n);
}

int jsp_sair(char *b, size_t n) { return fim_(snprintf(b, n, "{\"t\":\"sair\"}"), n); }

int jsp_impressao(char *b, size_t n, const JsImpressao *i) {
  char arq[260], bg[180], rot[140], h[64] = "", x[32] = "", by[40] = "", al[24] = "", du[40] = "";
  if (i->hash[0]) snprintf(h, sizeof h, "\"hash\":\"%.40s\",", i->hash);
  if (i->idx >= 0) snprintf(x, sizeof x, "\"idx\":%d,", i->idx);
  if (i->bytes > 0) snprintf(by, sizeof by, "\"bytes\":%lld,", i->bytes);
  if (i->altura > 0) snprintf(al, sizeof al, "\"altura\":%d,", i->altura);
  if (i->durMs > 0) snprintf(du, sizeof du, "\"durMs\":%lld,", i->durMs);
  return fim_(snprintf(b, n, "{%s%s%s%s%s\"arquivo\":\"%s\",\"binge\":\"%s\",\"rotulo\":\"%s\"}", h, x, by, al, du,
                       jsp_esc(arq, sizeof arq, i->arquivo), jsp_esc(bg, sizeof bg, i->binge),
                       jsp_esc(rot, sizeof rot, i->rotulo)), n);
}

int jsp_celular(char *b, size_t n, int chat) {
  return fim_(snprintf(b, n, "{\"t\":\"celular\",\"chat\":%d}", chat ? 1 : 0), n);
}
