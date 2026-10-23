// Enquete na ilha — ver enquete.h.
#include "enquete.h"
#include "ajustes.h"
#include "dados.h"
#include "avisodisp.h"
#include "ilha.h"
#include "ilhaacao.h"
#include "idioma.h"
#include "js.h"
#include "perfis.h"
#include "player.h"
#include "recomenda.h"
#include "rede.h"
#include "tex_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

#define ARQ_VISTO   "enquete.txt"          // "<perfil> <id>" por linha: convite ja dito
#define ARQ_PEND    "enquete-optout.txt"   // "<perfil> <0|1>": opt-out ainda nao confirmado pelo servidor
#ifndef ESPERA_MS
#define ESPERA_MS   15000u                 // o app assenta antes do primeiro pedido
#endif
#define REBUSCA_MS  (6u * 3600u * 1000u)   // sessao longa: de novo a cada 6 h
#define PRAZO_S     12
#define CH_CONVITE   "enquete:v1"
#define CH_OPCOES    "enquete:v2"
#define CH_RESULTADO "enquete:v3"
#define VISTOS_MAX   8
#define FILA_MAX     4

typedef struct {
  char id[48], pergunta[160], arte[512];
  long long fim;
  int n, voto, total, tem;           // tem = veio do servidor e e valida
  char texto[3][64];
  int idx[3], votos[3], temResultado;
} Enq;

typedef enum { OP_BUSCAR = 1, OP_VOTAR, OP_OPTOUT } OpTipo;
typedef struct { OpTipo tipo; unsigned gen; char id[48]; int valor; int perfil; } Op;
typedef struct { unsigned gen; OpTipo tipo; int status, optout, ok; Enq e; } Resp;

static SDL_mutex *mtx;
static volatile unsigned gen;           // sobe a cada troca de perfil
static Op fila[FILA_MAX];
static int nFila, fioVivo;
static Resp caixa;
static int caixaCheia;

// estado do fio principal
static Enq cur;
static int buscando, ponto;      // buscando: um GET em voo
static Uint32 proxima;           // quando a proxima busca pode sair
static Uint32 iniciouEm, agoraAtual;   // agoraAtual: o `agora` do ultimo quadro
static int abrindo;                     // modal posto e ainda sem resposta
static char abrindoChave[40];
static Uint32 abrindoDesde;
static int pediuVoto;                   // voto em voo
static struct { int perfil; char id[48]; } vistos[VISTOS_MAX];
static int nVistos = -1;

static int perfilAtual(void) {
  const ContaPerfil *p = perfis_item_ativo();
  return p && p->indice > 0 ? p->indice : 1;
}

// --- o que ja foi dito (convite uma vez) --------------------------------------------
static void carregarVistos(void) {
  char *s;
  nVistos = 0;
  s = dados_ler(ARQ_VISTO);
  if (!s) return;
  { char *l = s;
    while (*l && nVistos < VISTOS_MAX) {
      int pf = 0; char id[48];
      if (sscanf(l, "%d %47s", &pf, id) == 2) {
        vistos[nVistos].perfil = pf;
        snprintf(vistos[nVistos].id, sizeof vistos[0].id, "%s", id);
        nVistos++;
      }
      l = strchr(l, '\n');
      if (!l) break;
      l++;
    } }
  free(s);
}
static int foiVisto(const char *id) {
  int i, pf = perfilAtual();
  if (nVistos < 0) carregarVistos();
  for (i = 0; i < nVistos; i++) if (vistos[i].perfil == pf && !strcmp(vistos[i].id, id)) return 1;
  return 0;
}
static void marcarVisto(const char *id) {
  char buf[VISTOS_MAX * 64], *w = buf;
  int i;
  if (foiVisto(id)) return;
  if (nVistos == VISTOS_MAX) { memmove(&vistos[0], &vistos[1], sizeof vistos[0] * (VISTOS_MAX - 1)); nVistos--; }
  vistos[nVistos].perfil = perfilAtual();
  snprintf(vistos[nVistos].id, sizeof vistos[0].id, "%s", id);
  nVistos++;
  buf[0] = 0;
  for (i = 0; i < nVistos; i++) w += snprintf(w, sizeof buf - (size_t)(w - buf), "%d %s\n", vistos[i].perfil, vistos[i].id);
  dados_gravar(ARQ_VISTO, buf);
}

// --- opt-out ainda nao confirmado (nao perde o toque feito sem rede) -------------------
static int pendente(int perfil, int *valor) {
  char *s = dados_ler(ARQ_PEND);
  int pf = 0, v = 0, ok = 0;
  if (s) { ok = sscanf(s, "%d %d", &pf, &v) == 2 && pf == perfil; free(s); }
  if (ok && valor) *valor = v;
  return ok;
}
static void gravarPendente(int perfil, int valor) {
  char b[24];
  snprintf(b, sizeof b, "%d %d\n", perfil, valor);
  dados_gravar(ARQ_PEND, b);
}

// --- o fio de rede ------------------------------------------------------------------
static int enquete_ativo(void) { return NV_REC_URL[0] != 0; }

static void jsonEsc(char *dst, size_t tam, const char *s) {
  size_t k = 0;
  for (; s && *s && k + 7 < tam; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') { dst[k++] = '\\'; dst[k++] = (char)c; }
    else if (c < 0x20) k += (size_t)snprintf(dst + k, tam - k, "\\u%04x", c);
    else dst[k++] = (char)c;
  }
  dst[k] = 0;
}

// Le o corpo do servico. 1 = a resposta tinha o formato esperado.
static int lerCorpo(const char *r, Resp *out) {
  const char *o, *fim, *a;
  int i;
  memset(&out->e, 0, sizeof out->e);
  if (!r || !js_tem(r, NULL, "optout")) return 0;
  out->optout = (int)js_num(r, NULL, "optout", 0.0) ? 1 : 0;
  o = strstr(r, "\"enquete\"");
  if (!o) return 1;
  o += 9;
  while (*o == ' ' || *o == ':' || *o == '\n' || *o == '\t') o++;
  if (*o != '{') return 1;        // null: nenhuma enquete
  fim = js_fim(o);
  if (!js_texto_raiz_em(o, fim, "id", out->e.id, sizeof out->e.id) || !out->e.id[0]) return 1;
  js_texto_raiz_em(o, fim, "pergunta", out->e.pergunta, sizeof out->e.pergunta);
  js_texto_raiz_em(o, fim, "arte", out->e.arte, sizeof out->e.arte);
  out->e.fim = (long long)js_num(o, fim, "fim", 0.0);
  out->e.voto = (int)js_num(o, fim, "voto", 0.0);
  a = js_array(o, fim, "opcoes");
  while (a && *a == '{' && out->e.n < 3) {
    const char *f = js_fim(a);
    int k = out->e.n;
    out->e.idx[k] = (int)js_num(a, f, "idx", (double)(k + 1));
    js_texto(a, f, "texto", out->e.texto[k], sizeof out->e.texto[k]);
    out->e.n++;
    a = js_prox(f);
  }
  if (out->e.n < 2 || !out->e.pergunta[0]) { memset(&out->e, 0, sizeof out->e); return 1; }
  out->e.tem = 1;
  a = js_array(o, fim, "resultado");
  if (a) {
    out->e.temResultado = 1;
    while (a && *a == '{') {
      const char *f = js_fim(a);
      int idx = (int)js_num(a, f, "idx", 0.0), v = (int)js_num(a, f, "votos", 0.0);
      for (i = 0; i < out->e.n; i++) if (out->e.idx[i] == idx) out->e.votos[i] = v;
      a = js_prox(f);
    }
    out->e.total = (int)js_num(o, fim, "total", 0.0);
  }
  return 1;
}

static void executar(const Op *op) {
  const char *cab[4];
  char aut[3200], via[40], pf[40], url[300], corpo[200], id[100];
  Resp r;
  int st = 0;
  char *txt;
  memset(&r, 0, sizeof r);
  r.gen = op->gen; r.tipo = op->tipo;
  if (op->gen != gen || !recomenda_cabecalhos(cab, aut, sizeof aut, via, sizeof via, pf, sizeof pf)) goto fim;
  corpo[0] = 0;
  if (op->tipo == OP_BUSCAR) {
    snprintf(url, sizeof url, "%s/v1/enquete", NV_REC_URL);
    txt = rede_baixar_st(url, PRAZO_S, cab, &st);
  } else if (op->tipo == OP_VOTAR) {
    jsonEsc(id, sizeof id, op->id);
    snprintf(url, sizeof url, "%s/v1/enquete/voto", NV_REC_URL);
    snprintf(corpo, sizeof corpo, "{\"id\":\"%s\",\"opcao\":%d}", id, op->valor);
    txt = rede_postar_st(url, PRAZO_S, cab, corpo, &st);
  } else {
    snprintf(url, sizeof url, "%s/v1/enquete/optout", NV_REC_URL);
    snprintf(corpo, sizeof corpo, "{\"optout\":%d}", op->valor ? 1 : 0);
    txt = rede_postar_st(url, PRAZO_S, cab, corpo, &st);
  }
  r.status = st;
  if (txt && st >= 200 && st < 300) {
    if (op->tipo == OP_OPTOUT) r.ok = js_tem(txt, NULL, "optout");
    else r.ok = lerCorpo(txt, &r);
    if (op->tipo == OP_OPTOUT) r.optout = (int)js_num(txt, NULL, "optout", 0.0) ? 1 : 0;
  }
  free(txt);
  if (op->tipo == OP_OPTOUT && r.ok && op->gen == gen) {
    int v;
    // So some o pendente se ainda e o desta escolha (outro toque pode ter vindo).
    if (pendente(op->perfil, &v) && v == op->valor) dados_apagar(ARQ_PEND);
  }
fim:
  SDL_LockMutex(mtx);
  if (!caixaCheia || op->tipo != OP_OPTOUT) { caixa = r; caixaCheia = 1; }
  SDL_UnlockMutex(mtx);
}

static int fioLaco(void *arg) {
  (void)arg;
  for (;;) {
    Op op;
    SDL_LockMutex(mtx);
    if (!nFila) { fioVivo = 0; SDL_UnlockMutex(mtx); return 0; }
    op = fila[0];
    memmove(&fila[0], &fila[1], sizeof fila[0] * (size_t)(--nFila));
    SDL_UnlockMutex(mtx);
    // A caixa tem um lugar so: espera o quadro principal esvazia-la.
    for (;;) {
      int cheia;
      SDL_LockMutex(mtx); cheia = caixaCheia; SDL_UnlockMutex(mtx);
      if (!cheia || op.gen != gen) break;
      SDL_Delay(20);
    }
    executar(&op);
  }
}

static void enfileirar(OpTipo tipo, const char *id, int valor) {
  SDL_Thread *t;
  Op op;
  if (!enquete_ativo()) return;
  if (!mtx) mtx = SDL_CreateMutex();
  if (!mtx) return;
  memset(&op, 0, sizeof op);
  op.tipo = tipo; op.gen = gen; op.valor = valor; op.perfil = perfilAtual();
  if (id) snprintf(op.id, sizeof op.id, "%s", id);
  SDL_LockMutex(mtx);
  if (nFila == FILA_MAX) { SDL_UnlockMutex(mtx); return; }
  fila[nFila++] = op;
  if (!fioVivo) {
    fioVivo = 1;
    SDL_UnlockMutex(mtx);
    t = SDL_CreateThread(fioLaco, "nv-enquete", NULL);
    if (t) SDL_DetachThread(t);
    else { SDL_LockMutex(mtx); fioVivo = 0; nFila = 0; SDL_UnlockMutex(mtx); }
    return;
  }
  SDL_UnlockMutex(mtx);
}

// --- a ilha --------------------------------------------------------------------------
static int vencida(const Enq *e) { return e->fim > 0 && (long long)time(NULL) >= e->fim; }

static void prazoTexto(const Enq *e, char *dst, size_t n) {
  long long s = e->fim - (long long)time(NULL);
  int d = (int)((s + 86399) / 86400);
  if (e->fim <= 0 || s <= 0) { dst[0] = 0; return; }
  if (s < 86400) snprintf(dst, n, "%s", i18n("Termina hoje"));
  else if (d <= 1) snprintf(dst, n, "%s", i18n("Termina amanhã"));
  else snprintf(dst, n, i18n("Termina em %d dias"), d);
}

static void posto(const char *chave) {
  abrindo = 1; abrindoDesde = agoraAtual;
  snprintf(abrindoChave, sizeof abrindoChave, "%s", chave);
}

// DISPENSAR A ENQUETE (06/10, "muito alerta sem dispensar"): a bolinha do
// relogio ficava ate a pessoa votar ou a enquete vencer. A AZUL nela agora
// reabre o CONVITE com "Dispensar" no lugar de "Agora nao": dispensada, a
// bolinha apaga e aquela enquete nao volta (avisodisp.h, por perfil); a
// proxima enquete convida de novo. Na primeira vez continua "Agora nao".
static int conviteDoPonto;
static void chaveDisp(char *dst, size_t tam) { snprintf(dst, tam, "enquete:%s", cur.id); }
static void mostrarConvite(int doPonto) {
  static IlhaModal m;
  IlhaAvisoEx e;
  memset(&m, 0, sizeof m);
  snprintf(m.kicker, sizeof m.kicker, "%s", i18n("Enquete"));
  snprintf(m.titulo, sizeof m.titulo, "%s", cur.pergunta);
  snprintf(m.texto, sizeof m.texto, "%s", i18n("Leva poucos segundos. Ninguém vê o seu voto, só o total."));
  snprintf(m.icone, sizeof m.icone, "aj_bell");
  m.cabecalho = 1; m.tipo = ILHA_ACENTO; m.nBotoes = 3;
  snprintf(m.botao[0], sizeof m.botao[0], "%s", i18n("Responder"));
  snprintf(m.botaoIcone[0], sizeof m.botaoIcone[0], "check");
  snprintf(m.botao[1], sizeof m.botao[1], "%s", doPonto ? i18n("Dispensar") : i18n("Agora não"));
  conviteDoPonto = doPonto;
  snprintf(m.botao[2], sizeof m.botao[2], "%s", i18n("Não receber mais enquetes"));
  snprintf(m.botaoIcone[2], sizeof m.botaoIcone[2], "aj_x");
  memset(&e, 0, sizeof e);
  e.chave = CH_CONVITE; e.tipo = ILHA_ACENTO; e.icone = "aj_bell";
  e.texto = i18n("Tem uma enquete para você"); e.ms = 60000u; e.modal = &m; e.abrir = 1;
  ilha_avisar_ex(&e);
  posto(CH_CONVITE);
  printf("[enquete] convite na ilha: %s\n", cur.id);
}

// As opcoes (1..3 botoes): o texto de cada uma e o rotulo do botao. A arte do
// servidor 480x270 vai a esquerda; o prazo na base da coluna.
static void mostrarOpcoes(void) {
  static IlhaModal m;
  IlhaAvisoEx e;
  int i;
  memset(&m, 0, sizeof m);
  snprintf(m.kicker, sizeof m.kicker, "%s", i18n("Enquete"));
  snprintf(m.titulo, sizeof m.titulo, "%s", cur.pergunta);
  snprintf(m.arte, sizeof m.arte, "%s", cur.arte);
  if (cur.arte[0]) tex_obter_larg(cur.arte, 480.0f);   // aquece antes de a pilula crescer
  if (!cur.arte[0]) snprintf(m.icone, sizeof m.icone, "aj_bell");
  prazoTexto(&cur, m.estado, sizeof m.estado);
  snprintf(m.rodape, sizeof m.rodape, "%s", i18n("Voltar deixa para depois."));
  m.tipo = ILHA_ACENTO; m.nBotoes = cur.n;
  for (i = 0; i < cur.n; i++) snprintf(m.botao[i], sizeof m.botao[i], "%s", cur.texto[i]);
  memset(&e, 0, sizeof e);
  e.chave = CH_OPCOES; e.tipo = ILHA_ACENTO; e.icone = "aj_bell";
  e.texto = i18n("Tem uma enquete para você"); e.ms = 60000u; e.modal = &m; e.abrir = 1;
  ilha_avisar_ex(&e);
  posto(CH_OPCOES);
}

static void mostrarResultado(void) {
  static IlhaModal m;
  IlhaAvisoEx e;
  char fr[40];
  int i;
  memset(&m, 0, sizeof m);
  snprintf(m.kicker, sizeof m.kicker, "%s", i18n("Obrigado pelo voto"));
  snprintf(m.titulo, sizeof m.titulo, "%s", cur.pergunta);
  snprintf(m.arte, sizeof m.arte, "%s", cur.arte);
  if (!cur.arte[0]) snprintf(m.icone, sizeof m.icone, "aj_bell");
  m.resultado = 1; m.tipo = ILHA_OK;
  for (i = 0; i < cur.n; i++) {
    snprintf(m.lista[i], sizeof m.lista[i], "%s", cur.texto[i]);
    m.pct[i] = cur.total > 0 ? (cur.votos[i] * 100 + cur.total / 2) / cur.total : 0;
    if (cur.voto == cur.idx[i]) m.escolha = i + 1;
  }
  if (cur.total == 1) snprintf(fr, sizeof fr, "%s", i18n("1 voto"));
  else snprintf(fr, sizeof fr, i18n("%d votos"), cur.total);
  prazoTexto(&cur, m.linha, sizeof m.linha);
  snprintf(m.estado, sizeof m.estado, "%s", fr);
  m.nBotoes = 1;
  snprintf(m.botao[0], sizeof m.botao[0], "%s", i18n("Ok"));
  memset(&e, 0, sizeof e);
  e.chave = CH_RESULTADO; e.tipo = ILHA_OK; e.icone = "check";
  e.texto = i18n("Obrigado pelo voto"); e.ms = 30000u; e.modal = &m; e.abrir = 1;
  ilha_avisar_ex(&e);
  posto(CH_RESULTADO);
}

static void atualizarPonto(void) {
  char ch[64];
  int p;
  chaveDisp(ch, sizeof ch);
  p = cur.tem && !cur.voto && !vencida(&cur) && foiVisto(cur.id) && ajustes_enquetes() && !avisodisp_tem(ch);
  if (p != ponto) { ponto = p; ilha_ponto_enquete(p); }
}

int enquete_aberta(void) { return ponto; }

static void desfazerOptout(const void *c) {
  (void)c;
  ajustes_espelhar_enquetes(1);
  enquete_definir_optout(0);
}

static void optoutFeito(void) {
  IlhaAcao a;
  ajustes_espelhar_enquetes(0);
  enquete_definir_optout(1);
  memset(&a, 0, sizeof a);
  a.icone = "aj_bell"; a.frase = i18n("Você não vai mais receber enquetes");
  a.titulo = ""; a.onde = i18n("Dá para ligar de novo em Ajustes.");
  a.tipo = ILHA_INFO; a.desfazer = desfazerOptout;
  ilhaacao_feita(&a);
}

void enquete_definir_optout(int sair) {
  int pf = perfilAtual();
  gravarPendente(pf, sair ? 1 : 0);
  enfileirar(OP_OPTOUT, NULL, sair ? 1 : 0);
  if (sair) {
    ilha_retirar(CH_CONVITE); ilha_retirar(CH_OPCOES); ilha_retirar(CH_RESULTADO);
    abrindo = 0;
  } else proxima = agoraAtual;   // voltou a receber: busca de novo
  atualizarPonto();
}

void enquete_perfil_trocado(void) {
  gen++;
  SDL_LockMutex(mtx ? mtx : (mtx = SDL_CreateMutex()));
  nFila = 0;
  caixaCheia = 0;
  SDL_UnlockMutex(mtx);
  memset(&cur, 0, sizeof cur);
  buscando = 0; proxima = 0; pediuVoto = 0; abrindo = 0;
  iniciouEm = agoraAtual ? agoraAtual : 1;     // o convite do novo perfil espera o app assentar de novo
  ilha_retirar(CH_CONVITE); ilha_retirar(CH_OPCOES); ilha_retirar(CH_RESULTADO);
  atualizarPonto();
}

static void aplicar(const Resp *r) {
  if (r->tipo == OP_OPTOUT) return;   // ja foi gravado; nada a mostrar
  if (r->tipo == OP_VOTAR) {
    pediuVoto = 0;
    if (!r->ok || !r->e.tem || !r->e.voto) {
      // 410 = encerrou no meio; 409 = ja votou noutra TV; senao: rede.
      const char *t = r->status == 410 ? i18n("Esta enquete já terminou.")
                                       : i18n("Não deu para enviar o voto. Tente de novo.");
      if (r->status == 410) memset(&cur, 0, sizeof cur);
      ilha_avisar("enquete:e1", ILHA_ERRO, "aj_triangle-alert", t, 6000u, 0);
      atualizarPonto();
      return;
    }
    cur = r->e;
    atualizarPonto();
    mostrarResultado();
    printf("[enquete] voto %d enviado (%d votos)\n", cur.voto, cur.total);
    return;
  }
  // OP_BUSCAR
  buscando = 0;
  if (!r->ok) {
    // Missing identity or rejected credentials: wait five minutes, not one.
    proxima = agoraAtual + ((!r->status || r->status == 401) ? 300000u : 60000u);
    return;
  }
  proxima = agoraAtual + REBUSCA_MS;
  { int local = !ajustes_enquetes(), pf = perfilAtual(), v = 0;
    if (pendente(pf, &v)) { if (v != r->optout) enfileirar(OP_OPTOUT, NULL, v); }
    else if (r->optout != local) ajustes_espelhar_enquetes(!r->optout);   // outra TV mudou
  }
  if (!ajustes_enquetes() || r->optout) { memset(&cur, 0, sizeof cur); atualizarPonto(); return; }
  if (cur.tem && !strcmp(cur.id, r->e.id) && abrindo) { cur.voto = r->e.voto; }   // nao mexe no modal aberto
  else cur = r->e;
  atualizarPonto();
  if (cur.tem && !cur.voto && !vencida(&cur) && !foiVisto(cur.id) && !abrindo &&
      !player_aberto() && agoraAtual - iniciouEm > ESPERA_MS) {
    marcarVisto(cur.id);        // "uma vez": dito agora, mesmo que o app feche antes da resposta
    mostrarConvite(0);
    atualizarPonto();
  }
}

void enquete_acao(const char *chave, int botao) {
  if (!chave) return;
  abrindo = 0;
  if (!strcmp(chave, CH_CONVITE)) {
    if (botao == 1 && cur.tem) mostrarOpcoes();
    else if (botao == 3) optoutFeito();
    else if (botao == 2 && conviteDoPonto && cur.tem) {
      char ch[64];
      chaveDisp(ch, sizeof ch);
      avisodisp_por(ch);
      printf("[enquete] dispensada: %s\n", cur.id);
    }
    // 2 (Agora nao, na primeira vez): a bolinha ja esta ligada (convite marcado como dito)
    atualizarPonto();
  } else if (!strcmp(chave, CH_OPCOES)) {
    if (botao >= 1 && botao <= cur.n && !pediuVoto && cur.tem && !cur.voto) {
      pediuVoto = 1;
      enfileirar(OP_VOTAR, cur.id, cur.idx[botao - 1]);
    }
  }
}

void enquete_passo(Uint32 agora) {
  if (!enquete_ativo()) return;
  agoraAtual = agora;
  if (!iniciouEm) iniciouEm = agora ? agora : 1;
  // O que o fio entregou.
  if (mtx) {
    Resp r;
    int tem = 0;
    SDL_LockMutex(mtx);
    if (caixaCheia) { r = caixa; caixaCheia = 0; tem = 1; }
    SDL_UnlockMutex(mtx);
    if (tem && r.gen == gen) aplicar(&r);
  }
  // A busca: depois que o app assentou, uma por sessao/perfil e a cada 6 h.
  if (agora - iniciouEm > ESPERA_MS && !buscando && (int)(agora - proxima) >= 0) {
    buscando = 1;
    enfileirar(OP_BUSCAR, NULL, 0);
  }
  // VOLTAR: o modal recolheu sem entregar botao. Vale como "Agora nao".
  if (abrindo && agora - abrindoDesde > 600u && !ilha_tem(abrindoChave) &&
      !ilha_modal_aberto() && !ilha_modal_visivel()) {
    abrindo = 0;
    atualizarPonto();
  }
  // A AZUL no relogio com a bolinha: reabre o convite, agora com "Dispensar".
  if (ilha_ponto_pediu() && cur.tem && !cur.voto && !vencida(&cur) && !abrindo && !pediuVoto)
    mostrarConvite(1);
  if (ponto && (vencida(&cur) || !ajustes_enquetes())) { memset(&cur, 0, sizeof cur); atualizarPonto(); }
}
