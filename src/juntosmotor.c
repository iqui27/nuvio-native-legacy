// Watch Together: o motor do membro — ver juntosmotor.h.
#include "juntosmotor.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PING_RAJADA      8
#define PING_RAJADA_MS   150.0
#define PING_RESYNC      3
#define PING_RESYNC_MS   60000.0
#define RAJADA_SEEK_MS   700.0
#define PARADA_MS        1500.0   // tocando sem a posicao andar = carregando
#define ORDEM_REPETE_MS  1500.0
#define PRE_BUFFER_TOL   0.25     // s: pausado longe disto do pos0 = buscar

static void evento(JsMotor *m, JsEvTipo t, const char *k, const char *quem, const char *txt, int n) {
  JsEvento *e;
  if (m->evN == JSM_FILA) { m->evIni = (m->evIni + 1) % JSM_FILA; m->evN--; }   // fila cheia: perde o mais velho
  e = &m->ev[(m->evIni + m->evN) % JSM_FILA];
  memset(e, 0, sizeof *e);
  e->tipo = t;
  if (k) snprintf(e->k, sizeof e->k, "%s", k);
  if (quem) snprintf(e->quem, sizeof e->quem, "%s", quem);
  if (txt) snprintf(e->txt, sizeof e->txt, "%s", txt);
  e->n = n;
  m->evN++;
}

int jsm_evento(JsMotor *m, JsEvento *e) {
  if (!m->evN) return 0;
  *e = m->ev[m->evIni];
  m->evIni = (m->evIni + 1) % JSM_FILA;
  m->evN--;
  return 1;
}

static int mandar(JsMotor *m, const char *j) {
  if (!m->conectado || !m->enviar) return -1;
  return m->enviar(m->envU, j) == 0 ? 1 : -1;
}

void jsm_iniciar(JsMotor *m, const char *eu, const char *plat, const char *app, char nivel,
                 const JsPlayer *p, JsEnviar enviar, void *envU) {
  memset(m, 0, sizeof *m);
  snprintf(m->eu, sizeof m->eu, "%s", eu ? eu : "");
  snprintf(m->plat, sizeof m->plat, "%s", plat ? plat : "");
  snprintf(m->app, sizeof m->app, "%s", app ? app : "");
  m->p = *p;
  m->enviar = enviar;
  m->envU = envU;
  jsr_zerar(&m->rel);
  jss_zerar(&m->sync, nivel);
  m->velCent = 100;
  snprintf(m->stApp, sizeof m->stApp, "procurando");
}

static void ola(JsMotor *m) {
  char b[200];
  if (jsp_ola(b, sizeof b, m->plat, m->sync.nivel, m->rel.tem ? m->rel.melhorRtt : 0, m->app) > 0) mandar(m, b);
}

void jsm_conectou(JsMotor *m, double agora) {
  m->conectado = 1;
  m->rajada = PING_RAJADA;
  m->pongs = 0;
  m->proxPing = agora;
  m->stEnviado[0] = 0;     // reenvia o estado na proxima volta
  ola(m);
}

void jsm_caiu(JsMotor *m) { m->conectado = 0; }

const JsMembro *jsm_membro(const JsMotor *m, const char *id) {
  int i;
  for (i = 0; i < m->sala.n; i++) if (!strcmp(m->sala.m[i].id, id)) return &m->sala.m[i];
  return NULL;
}
int jsm_sou_anfitriao(const JsMotor *m) {
  const JsMembro *e = jsm_membro(m, m->eu);
  return e && e->anf;
}
int jsm_pode_controlar(const JsMotor *m) { return !m->sala.controleAnf || jsm_sou_anfitriao(m); }

double jsm_pos_alvo(const JsMotor *m, double agora) {
  return jsr_pos_alvo(&m->sala.rel, jsr_servidor(&m->rel, agora));
}

// A posicao interpolada. A amostra pode ser de ANTES do video voltar a andar
// (o LG le a cada ~200 ms): conta o tempo so a partir de quando o motor viu o
// video andando, senao o play parece ter saido mais cedo do que saiu.
static double posLocal(JsMotor *m, double agora) {
  double t = agora, p = m->p.pos(m->p.u, &t);
  int toc = m->p.tocando(m->p.u);
  if (!toc) m->tocandoDesde = 0;
  else if (!m->tocandoDesde) m->tocandoDesde = agora;
  if (toc && t < m->tocandoDesde) t = m->tocandoDesde;
  return jss_pos_local(p, t, agora, toc, m->velCent / 100.0);
}

// Ordem ao player. Repetida (mesma vontade) no maximo a cada 1,5 s: o LG
// responde assincrono e uma ordem por quadro so enche a fila do LS2.
static void ordenar(JsMotor *m, int tocar, double agora, int forcar) {
  if (!forcar && m->queroTocar == tocar && agora - m->ordemEm < ORDEM_REPETE_MS) return;
  m->queroTocar = tocar;
  m->ordemEm = agora;
  m->p.pausar(m->p.u, !tocar);
}

static void velocidade(JsMotor *m, int cent) {
  if (m->velCent == cent) return;
  m->velCent = cent;
  m->p.velocidade(m->p.u, cent);
}

static void buscar(JsMotor *m, double pos, double agora) {
  if (pos < 0) pos = 0;
  if (m->velCent != 100) { velocidade(m, 100); m->sync.velAte = 0; }
  m->p.buscar(m->p.u, pos);
  m->seekEmCurso = 1;
  m->seekEm = agora;
  m->seekPos = pos;
  { double t; m->seekRef = m->p.pos(m->p.u, &t); }   // a amostra CRUA de antes
}

// Onde o player vai estar: o destino do seek em curso, ou a posicao agora.
static double ondeVai(JsMotor *m, double agora) { return m->seekEmCurso ? m->seekPos : posLocal(m, agora); }

// O relogio da sala mudou: leva o player ate ele.
static void aplicar(JsMotor *m, double agora) {
  const JsRel *r = &m->sala.rel;
  int pronto = m->p.pronto(m->p.u);
  if (r->estado == JSE_TOCANDO) {
    double em = jss_instante_play(&m->sync, &m->rel, r->t0);
    if (agora < em - 30) {
      // Play agendado: parado no quadro de partida, solta no instante comum.
      // No nivel B o seek vai um pouco alem (margem do keyframe): cair a
      // frente so atrasa a largada, que e calculada pela posicao onde caiu.
      ordenar(m, 0, agora, 1);
      if (pronto && fabs(ondeVai(m, agora) - r->pos0) > PRE_BUFFER_TOL)
        buscar(m, r->pos0 + (m->sync.nivel == 'B' ? m->sync.margemSeek : 0.0), agora);
      m->playArmado = 1;
      m->playEm = em;
    } else {
      // Chegou tarde (ou entrou no meio): toca ja e alinha pelo relogio.
      double alvo = jsm_pos_alvo(m, agora);
      m->playArmado = 0;
      if (pronto && fabs(ondeVai(m, agora) - alvo) > JSS_VEL_MAX)
        buscar(m, alvo + m->sync.latSeekMs / 1000.0, agora);
      ordenar(m, 1, agora, 1);
    }
    return;
  }
  // pausado, esperando, lobby, fim: todos no mesmo quadro.
  m->playArmado = 0;
  if (m->velCent != 100) { velocidade(m, 100); m->sync.velAte = 0; }
  ordenar(m, 0, agora, 1);
  if (pronto && r->estado != JSE_FIM && fabs(ondeVai(m, agora) - r->pos0) > PRE_BUFFER_TOL) buscar(m, r->pos0, agora);
}

static const char *nomeDe(const JsMotor *m, const char *id) {
  const JsMembro *x = jsm_membro(m, id);
  return x ? x->nome : "";
}

void jsm_receber(JsMotor *m, const char *json, double agora) {
  JsMsg g;
  static JsSala nova;   // grande demais para a pilha das TVs; o motor e um so
  JsMsgTipo t = jsp_ler(json, &g, &nova);
  switch (t) {
    case JSM_PONG:
      jsr_amostra(&m->rel, g.c, g.s, agora);
      if (++m->pongs == PING_RAJADA) ola(m);   // fim da rajada: o servidor fica sabendo a RTT
      break;
    case JSM_SALA: {
      int eraSeq = m->sala.seq, mudouRel, mudouEp;
      if (m->temSala && nova.seq < eraSeq) break;    // retrato velho atrasado na rede
      mudouRel = !m->temSala || nova.seq != eraSeq;
      mudouEp = m->temSala && strcmp(nova.ep, m->sala.ep) != 0;
      m->sala = nova;
      m->temSala = 1;
      if (mudouEp) { evento(m, JSV_EPISODIO, NULL, nova.autor, nova.ep, 0); snprintf(m->stApp, sizeof m->stApp, "procurando"); }
      if (mudouRel) {
        aplicar(m, agora);
        if (eraSeq && nova.op[0] && strcmp(nova.op, "criar"))
          evento(m, JSV_ACAO, nova.op, nova.autor, nomeDe(m, nova.autor), (int)lround(nova.delta));
      }
      evento(m, JSV_SALA, NULL, NULL, NULL, 0);
      break;
    }
    case JSM_MEMBRO: {
      int i;
      for (i = 0; i < m->sala.n; i++) if (!strcmp(m->sala.m[i].id, g.membro.id)) { m->sala.m[i] = g.membro; break; }
      evento(m, JSV_SALA, NULL, NULL, NULL, 0);
      break;
    }
    case JSM_AVISO: evento(m, JSV_AVISO, g.k, g.quem, g.nome[0] ? g.nome : nomeDe(m, g.quem), g.prazo); break;
    case JSM_REACAO: evento(m, JSV_REACAO, g.k, g.quem, nomeDe(m, g.quem), 0); break;
    case JSM_FRASE: evento(m, JSV_FRASE, g.k, g.quem, nomeDe(m, g.quem), 0); break;
    case JSM_CHAT: evento(m, JSV_CHAT, NULL, g.quem, g.txt, 0); break;
    case JSM_PEDIDO: {
      JsEvento *e;
      evento(m, JSV_PEDIDO, NULL, g.quem, g.av, 0);
      e = &m->ev[(m->evIni + m->evN - 1) % JSM_FILA];
      snprintf(e->nome, sizeof e->nome, "%s", g.nome);
      break;
    }
    case JSM_PEDIR: evento(m, JSV_PEDIR_PAUSA, g.k, g.quem, nomeDe(m, g.quem), 0); break;
    case JSM_RECUSADO:
      // A pausa otimista (ou o seek local) nao valeu: volta ao relogio da sala.
      if (m->temSala) aplicar(m, agora);
      evento(m, JSV_RECUSADO, g.k, g.quem, nomeDe(m, g.quem), 0);
      break;
    case JSM_FIM:
      m->sala.rel.estado = JSE_FIM;
      m->playArmado = 0;
      evento(m, JSV_FIM, g.k, NULL, NULL, g.dur);
      break;
    case JSM_ERRO: evento(m, JSV_ERRO, g.k, NULL, NULL, 0); break;
    case JSM_AGUARDANDO: evento(m, JSV_AGUARDANDO, NULL, NULL, NULL, 0); break;
    case JSM_ACEITO: evento(m, JSV_ACEITO, NULL, NULL, NULL, 0); break;
    default: break;
  }
}

void jsm_estado_app(JsMotor *m, const char *st, int pct) {
  snprintf(m->stApp, sizeof m->stApp, "%s", st ? st : "");
  m->pctApp = pct;
}
void jsm_fonte(JsMotor *m, const char *tipo) { snprintf(m->fonte, sizeof m->fonte, "%s", tipo ? tipo : ""); }

static void reportar(JsMotor *m, double agora, int parado) {
  const char *st;
  int pct = 0;
  char b[256];
  int pronto = m->p.pronto(m->p.u);
  if (!pronto) {
    st = m->stApp[0] ? m->stApp : "carregando";
    pct = m->pctApp;
  } else if (m->p.bufferando_ms(m->p.u) > 1000 || parado || (m->seekEmCurso && agora - m->seekEm > 1500)) {
    st = "carregando";
  } else if (m->p.tocando(m->p.u)) st = "tocando";
  else st = "pronto";
  if (!strcmp(st, m->stEnviado) && (strcmp(st, "carregando") || pct == m->pctEnviado || agora - m->estadoEm < 1000))
    return;
  if (jsp_estado(b, sizeof b, st, pct, posLocal(m, agora), m->p.duracao(m->p.u), m->fonte,
                 m->rel.tem ? m->rel.melhorRtt : 0, m->sync.latSeekMs + m->sync.latPlayMs) > 0 && mandar(m, b) == 1) {
    snprintf(m->stEnviado, sizeof m->stEnviado, "%s", st);
    m->pctEnviado = pct;
    m->estadoEm = agora;
  }
}

void jsm_passo(JsMotor *m, double agora) {
  double pos;
  int tocando, pronto, parado = 0;
  char b[160];
  const JsRel *r = &m->sala.rel;

  if (m->conectado && agora >= m->proxPing) {
    if (!m->rajada) m->rajada = PING_RESYNC;
    if (jsp_ping(b, sizeof b, agora) > 0) mandar(m, b);
    m->rajada--;
    m->proxPing = agora + (m->rajada ? PING_RAJADA_MS : PING_RESYNC_MS);
  }
  if (m->rajadaSeek && agora - m->rajadaEm >= RAJADA_SEEK_MS) {
    m->rajadaSeek = 0;
    if (jsp_cmd(b, sizeof b, "seek", m->rajadaPos, m->sala.seq, NULL, NULL) > 0 && mandar(m, b) == 1) {
      m->ecoSeq = m->sala.seq;
      m->ecoAte = agora + 2500;
    }
  }
  if (!m->temSala) return;

  pronto = m->p.pronto(m->p.u);
  tocando = m->p.tocando(m->p.u);
  pos = posLocal(m, agora);

  // Play armado (largada agendada ou "segurar"): solta quando o relogio da sala
  // chegar a posicao em que o player ESTA parado. Recalculado a cada quadro,
  // porque o seek pode cair depois (keyframe, LG assincrono).
  if (m->playArmado && r->estado == JSE_TOCANDO && pronto)
    m->playEm = jss_instante_play(&m->sync, &m->rel, r->t0 + (ondeVai(m, agora) - r->pos0) * 1000.0);
  if (m->playArmado && agora >= m->playEm) {
    m->playArmado = 0;
    ordenar(m, 1, agora, 1);
    m->medindoPlay = 1;
    m->playPedidoEm = agora;
    m->playPedidoPos = pos;
  }
  if (m->medindoPlay && tocando && pos > m->playPedidoPos + 0.04) {
    m->medindoPlay = 0;
    jss_play_concluiu(&m->sync, agora - m->playPedidoEm - 40.0);
  }
  if (m->seekEmCurso) {
    // CAIU quando a amostra CRUA do backend fica mais perto do destino que da
    // origem (o LG le a cada ~200 ms, e ate a pausa pegar a posicao ainda anda:
    // "mudou" nao prova nada). Keyframe pode deixar atras do destino; se nunca
    // chegar perto, o prazo (2 x latencia + 0,5 s) encerra.
    double tb, cru = m->p.pos(m->p.u, &tb), dist = fabs(m->seekPos - m->seekRef);
    int perto = fabs(cru - m->seekPos) < (dist / 2 > 0.05 ? dist / 2 : 0.05);
    int prazo = agora - m->seekEm > 2 * m->sync.latSeekMs + 500;
    if (agora - m->seekEm > 100 && (perto || (tocando && prazo))) {
      // Fechado pelo prazo, o tempo nao e latencia de seek (seria realimentar o
      // proprio prazo): so o residuo conta.
      double ms = perto ? agora - m->seekEm : -1;
      m->seekEmCurso = 0;
      if (r->estado == JSE_TOCANDO && tocando) jss_seek_concluiu(&m->sync, ms, pos - jsm_pos_alvo(m, agora));
      else jss_seek_concluiu(&m->sync, ms, cru - m->seekPos);
    } else if (agora - m->seekEm > 8000) m->seekEmCurso = 0;
  }
  // Parada de posicao: tocando para a sala e para o motor, e a posicao nao anda.
  if (fabs(pos - m->ultPos) > 0.01 || !m->queroTocar || m->playArmado) { m->ultPos = pos; m->ultPosEm = agora; }
  else if (r->estado == JSE_TOCANDO && pronto && agora - m->ultPosEm > PARADA_MS) parado = 1;

  // O que a sala quer e o que o player faz precisam bater; ordem repetida no
  // maximo a cada 1,5 s (o LG responde assincrono).
  if (pronto && !m->playArmado) {
    int quer = r->estado == JSE_TOCANDO;
    if (quer != m->queroTocar || (quer && !tocando && !m->seekEmCurso)) ordenar(m, quer, agora, quer != m->queroTocar);
  }

  if (m->conectado) reportar(m, agora, parado);

  if (agora < m->proxTick) return;
  m->proxTick = agora + JSS_PASSO_MS;
  if (r->estado != JSE_TOCANDO || m->playArmado || !pronto || !m->rel.tem) return;
  // Comando proprio ainda sem eco da sala: o relogio que o motor conhece e o
  // de ANTES dele, e corrigir por ele desfaria o que a pessoa acabou de fazer.
  if (m->ecoSeq && m->sala.seq <= m->ecoSeq && agora < m->ecoAte) return;
  m->ecoSeq = 0;
  {
    int valido = tocando && !m->seekEmCurso && !parado && m->p.bufferando_ms(m->p.u) <= 1000;
    double alvo = jsm_pos_alvo(m, agora);
    int nudge = m->sync.velAte != 0;
    JsAcao a = jss_decidir(&m->sync, agora, pos, alvo, valido);
    if (a.tipo == JSA_VELOCIDADE) {
      if (a.cent == 100 && nudge && valido && jss_vel_medida(&m->sync, agora, pos) && m->sync.nivel == 'A') {
        m->sync.nivel = 'B';
        printf("[juntos] velocidade nao anda nesta TV: nivel B (so seek)\n");
        evento(m, JSV_NIVEL_B, NULL, NULL, NULL, 0);
        ola(m);
      }
      velocidade(m, a.cent);
    } else if (a.tipo == JSA_SEEK) {
      buscar(m, a.pos, agora);
      if (a.aviso) evento(m, JSV_SINCRONIZANDO, NULL, NULL, NULL, 0);
    } else if (a.tipo == JSA_SEGURAR) {
      // Adiantado: para aqui e solta quando o relogio da sala chegar.
      ordenar(m, 0, agora, 1);
      m->playArmado = 1;
      m->playEm = agora + a.ms - m->sync.latPlayMs;   // recalculado no proximo quadro
      if (a.aviso) evento(m, JSV_SINCRONIZANDO, NULL, NULL, NULL, 0);
    }
    if (a.frouxo && !m->frouxoAvisado) {
      m->frouxoAvisado = 1;
      printf("[juntos] sync frouxo (seeks demais; tolerancia 2 s)\n");
      evento(m, JSV_FROUXO, NULL, NULL, NULL, 0);
    }
  }
}

// ---- comandos da pessoa ---------------------------------------------------------------
int jsm_cmd(JsMotor *m, const char *op, double pos, const char *ep, double agora) {
  char b[200];
  int controle = strcmp(op, "encerrar") != 0;
  if (!m->conectado) return -1;
  if (controle && !jsm_pode_controlar(m)) return 0;
  if (!strcmp(op, "pausa")) {
    // Otimista: para na hora, no quadro em que a pessoa apertou.
    pos = posLocal(m, agora);
    ordenar(m, 0, agora, 1);
    m->playArmado = 0;
  }
  if (!strcmp(op, "seek")) { m->rajadaSeek = 0; buscar(m, pos, agora); }
  if (jsp_cmd(b, sizeof b, op, pos, m->sala.seq, ep, NULL) <= 0) return -1;
  m->ecoSeq = m->sala.seq;
  m->ecoAte = agora + 2500;
  return mandar(m, b);
}

int jsm_seek_rajada(JsMotor *m, double pos, double agora) {
  if (!m->conectado) return -1;
  if (!jsm_pode_controlar(m)) return 0;
  // A barra ja andou no player: ate o seek sair, o motor nao corrige.
  m->ecoSeq = m->sala.seq;
  m->ecoAte = agora + RAJADA_SEEK_MS + 2500;
  m->rajadaSeek = 1;
  m->rajadaPos = pos < 0 ? 0 : pos;
  m->rajadaEm = agora;
  return 1;
}

static int simples(JsMotor *m, const char *t, const char *k) {
  char b[96];
  return jsp_simples(b, sizeof b, t, k) > 0 ? mandar(m, b) : -1;
}
int jsm_reacao(JsMotor *m, const char *k) { return simples(m, "reacao", k); }
int jsm_frase(JsMotor *m, const char *k) { return simples(m, "frase", k); }
int jsm_pedir_pausa(JsMotor *m) { return simples(m, "pedir", "pausa"); }
int jsm_aceitar(JsMotor *m, const char *quem, int ok) {
  char b[300];
  return jsp_aceitar(b, sizeof b, quem, ok) > 0 ? mandar(m, b) : -1;
}
int jsm_controle(JsMotor *m, int soAnfitriao) {
  char b[160];
  if (!jsm_sou_anfitriao(m)) return 0;
  return jsp_cmd(b, sizeof b, "controle", -1, m->sala.seq, NULL, soAnfitriao ? "anfitriao" : "todos") > 0 ? mandar(m, b) : -1;
}
int jsm_sair(JsMotor *m) {
  char b[32];
  return jsp_sair(b, sizeof b) > 0 ? mandar(m, b) : -1;
}
