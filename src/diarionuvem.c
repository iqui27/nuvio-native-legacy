// O COFRE DO DIARIO NA NUVEM (diarionuvem.h).
//
// FIOS: no maximo UM pedido em voo (`ocupado`). O fio recebe tudo copiado
// (url, cabecalhos, corpo, perfil) e devolve so um resultado atomico; quem
// mexe no Diario com a resposta e o laco principal (diarionuvem_passo), com o
// perfil que o pedido levou — trocar de perfil no meio nao mistura diarios.
// Falha de rede: espera 60 s e tenta de novo; 409 "cofre" = o servidor nao tem
// o consentimento (outra TV desligou): o estado local volta a "nao".
#include "diarionuvem.h"
#include "diario.h"
#include "idioma.h"
#include "ilha.h"
#include "js.h"
#include "jsw.h"
#include "perfis.h"
#include "rede.h"
#include "sessao.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

#define LOTE_MAX 100          // 200 e o teto do servidor; titulos longos passam de 64 KB
#define ESPERA_FALHA_S 60
#define FOLGA_S 10

enum { J_NADA, J_COFRE, J_LOTE, J_RESTAURAR };

typedef struct {
  int tipo, perfil, ligado;
  char url[512], aut[1200], via[40], pf[40];
  char *corpo;
  // resultado
  int status;
  char *resp;
  int n; DiarioEntrada *env;       // o que subiu (para confirmar)
} Job;

static _Atomic int ocupado;        // 1 = fio em voo
static _Atomic(Job *) pronto;      // resultado esperando o laco principal
static long long esperarAte;
static int restaurarPend[CONTA_PERFIL_MAX + 1];
static long long restaurarDesde[CONTA_PERFIL_MAX + 1];

// Declaradas em diario.c, fora da API publica (so o cofre usa).
int  diario_nuvem_lote(int perfil, DiarioEntrada *v, int max);
void diario_nuvem_confirmar(int perfil, const DiarioEntrada *v, int n);

// ------------------------------------------------------------------ puro

void diarionuvem_chave(const DiarioEntrada *e, char *d, unsigned t) {
  if (e->temporada || e->episodio) snprintf(d, t, "%s_s%de%d", e->id, e->temporada, e->episodio);
  else snprintf(d, t, "%s", e->id);
}

unsigned diarionuvem_json_lote(const DiarioEntrada *v, int n, char *dst, unsigned tam) {
  Jsw w;
  int i;
  unsigned r = 0;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_chave(&w, "itens");
  jsw_arr_ini(&w);
  for (i = 0; i < n; i++) {
    const DiarioEntrada *e = &v[i];
    char chave[64], titulo[160], o[2] = { e->origem, 0 };
    int ano = 0;
    diarionuvem_chave(e, chave, sizeof chave);
    diario_nome(e->id, titulo, sizeof titulo, &ano, NULL, 0);
    jsw_obj_ini(&w);
    jsw_cs(&w, "chave", chave);
    jsw_ci(&w, "quando", e->quando);
    jsw_ci(&w, "nota10", e->nota10);
    jsw_ci(&w, "flags", e->flags & 0x1ff);
    jsw_ci(&w, "tags", e->tags & 0xfff);
    jsw_ci(&w, "cor", e->cor);
    jsw_ci(&w, "seg", e->seg);
    jsw_ci(&w, "hora", e->hora);
    jsw_cs(&w, "origem", o);
    jsw_ci(&w, "tmdb", e->tmdb);
    jsw_ci(&w, "dormiu", e->dormiuSeg);
    jsw_ci(&w, "dur", e->durSeg);
    jsw_ci(&w, "com", e->comPerfis);
    if (titulo[0]) jsw_cs(&w, "titulo", titulo);
    if (ano > 0) jsw_ci(&w, "ano", ano);
    jsw_obj_fim(&w);
  }
  jsw_arr_fim(&w);
  jsw_obj_fim(&w);
  if (!w.erro && jsw_texto_final(&w) && w.n < tam) { memcpy(dst, jsw_texto_final(&w), w.n + 1); r = (unsigned)w.n; }
  jsw_livre(&w);
  return r;
}

int diarionuvem_ler_itens(const char *json, DiarioEntrada *v, int max, int *mais, long long *ate) {
  const char *a, *p, *fimC;
  int n = 0;
  if (mais) *mais = 0;
  if (ate) *ate = 0;
  if (!json) return -1;
  fimC = json + strlen(json);
  if (mais) *mais = (int)js_num(json, fimC, "mais", 0);
  if (ate) *ate = (long long)js_num(json, fimC, "ate", 0);
  a = js_array(json, fimC, "itens");
  if (!a) return -1;
  for (p = a; p && *p && n < max; ) {
    const char *f;
    char chave[64], orig[4], titulo[160], id[DIARIO_ID_MAX];
    DiarioEntrada *e;
    int t = 0, ep = 0;
    while (*p && *p != '{' && *p != ']') p++;
    if (*p != '{') break;
    f = js_fim(p);
    if (!f) break;
    e = &v[n];
    memset(e, 0, sizeof *e);
    if (js_texto(p, f, "chave", chave, sizeof chave)) {
      char *s = strstr(chave, "_s");
      // "tt123_s2e5": o sufixo so vale se for exatamente _s<n>e<n> no fim.
      if (s && sscanf(s, "_s%de%d", &t, &ep) == 2) {
        char fim[24]; snprintf(fim, sizeof fim, "_s%de%d", t, ep);
        if (!strcmp(s, fim)) *s = 0; else t = ep = 0;
      }
      snprintf(id, sizeof id, "%s", chave);
      snprintf(e->id, sizeof e->id, "%s", id);
      e->temporada = (short)t; e->episodio = (short)ep;
      e->tipo = (t || ep) ? DIARIO_SERIE : DIARIO_FILME;
      e->quando = (long long)js_num(p, f, "quando", 0);
      e->nota10 = (unsigned char)js_num(p, f, "nota10", 0);
      e->flags = (unsigned short)js_num(p, f, "flags", 0);
      e->tags = (unsigned short)js_num(p, f, "tags", 0);
      e->cor = (unsigned)js_num(p, f, "cor", 0);
      e->seg = (int)js_num(p, f, "seg", 0);
      e->hora = (short)js_num(p, f, "hora", -1);
      e->tmdb = (int)js_num(p, f, "tmdb", 0);
      e->dormiuSeg = (int)js_num(p, f, "dormiu", 0);
      e->durSeg = (int)js_num(p, f, "dur", 0);
      e->comPerfis = (unsigned char)js_num(p, f, "com", 0);
      e->origem = js_texto(p, f, "origem", orig, sizeof orig) && orig[0] ? orig[0] : DIARIO_ORIG_PLAYER;
      if (e->nota10 > 10) e->nota10 = 0;
      if (js_texto(p, f, "titulo", titulo, sizeof titulo) && titulo[0])
        diario_nome_definir(e->id, titulo, (int)js_num(p, f, "ano", 0), NULL);
      if (e->id[0] && e->quando > 0) n++;
    }
    p = f;
  }
  return n;
}

// ------------------------------------------------------------------ rede

int diarionuvem_disponivel(void) { return NV_REC_URL[0] && sessao_logada() && sessao_token()[0]; }

static int cabecalhos(Job *j) {
  const ContaPerfil *p = perfis_item_ativo();
  if (!diarionuvem_disponivel()) return 0;
  snprintf(j->aut, sizeof j->aut, "Authorization: Bearer %s", sessao_token());
  snprintf(j->via, sizeof j->via, "X-Nuvio-Auth: nuvio");
  j->pf[0] = 0;
  // Mesma regra de recomenda.c: o perfil principal e a pessoa sem sufixo.
  if (p && !p->primario && p->indice >= 1) snprintf(j->pf, sizeof j->pf, "X-Nuvio-Perfil: %d", p->indice);
  j->perfil = perfis_ativo();
  return 1;
}

static void *fio(void *u) {
  Job *j = u;
  const char *cab[4] = { j->aut, j->via, j->pf[0] ? j->pf : NULL, NULL };
  j->status = 0;
  if (j->tipo == J_RESTAURAR) j->resp = rede_baixar_st(j->url, 25, cab, &j->status);
  else j->resp = rede_postar_st(j->url, 25, cab, j->corpo ? j->corpo : "{}", &j->status);
  atomic_store(&pronto, j);
  return NULL;
}

static int disparar(Job *j) {
  pthread_t t;
  int z = 0;
  if (!atomic_compare_exchange_strong(&ocupado, &z, 1)) return 0;
  if (pthread_create(&t, NULL, fio, j)) { atomic_store(&ocupado, 0); return 0; }
  pthread_detach(t);
  return 1;
}

static void jobLivre(Job *j) {
  if (!j) return;
  free(j->corpo); free(j->resp); free(j->env); free(j);
}

static void pedirCofre(int ligado) {
  Job *j = calloc(1, sizeof *j);
  if (!j) return;
  if (!cabecalhos(j)) { free(j); return; }
  j->tipo = J_COFRE; j->ligado = ligado;
  snprintf(j->url, sizeof j->url, "%s/v1/diario/cofre", NV_REC_URL);
  j->corpo = malloc(24);
  if (j->corpo) snprintf(j->corpo, 24, "{\"ligado\":%d}", ligado ? 1 : 0);
  if (!disparar(j)) { jobLivre(j); esperarAte = 0; }
}

static void pedirLote(void) {
  Job *j = calloc(1, sizeof *j);
  unsigned cap = LOTE_MAX * 520 + 64;
  if (!j) return;
  if (!cabecalhos(j)) { free(j); return; }
  j->env = calloc(LOTE_MAX, sizeof *j->env);
  j->corpo = malloc(cap);
  if (!j->env || !j->corpo) { jobLivre(j); return; }
  j->n = diario_nuvem_lote(j->perfil, j->env, LOTE_MAX);
  if (j->n <= 0 || !diarionuvem_json_lote(j->env, j->n, j->corpo, cap)) { jobLivre(j); return; }
  j->tipo = J_LOTE;
  snprintf(j->url, sizeof j->url, "%s/v1/diario/lote", NV_REC_URL);
  if (!disparar(j)) jobLivre(j);
}

static void pedirRestaurar(int perfil) {
  Job *j = calloc(1, sizeof *j);
  if (!j) return;
  if (!cabecalhos(j) || j->perfil != perfil) { free(j); return; }
  j->tipo = J_RESTAURAR;
  snprintf(j->url, sizeof j->url, "%s/v1/diario?desde=%lld&limite=500", NV_REC_URL, restaurarDesde[perfil]);
  if (!disparar(j)) jobLivre(j);
}


static void tratar(Job *j) {
  long long agora = (long long)time(NULL);
  int ok = j->status >= 200 && j->status < 300 && j->resp;
  if (j->status == 409 && j->resp && strstr(j->resp, "cofre")) {
    // O servidor nao tem o consentimento: nada mais sobe ate a pessoa ligar.
    diario_nuvem_definir(j->perfil, DIARIO_NUVEM_NAO);
    printf("[diarionuvem] cofre desligado no servidor\n");
  } else if (!ok) {
    esperarAte = agora + ESPERA_FALHA_S;
    printf("[diarionuvem] falhou (%d), tenta em %d s\n", j->status, ESPERA_FALHA_S);
  } else if (j->tipo == J_COFRE) {
    printf("[diarionuvem] cofre %s\n", j->ligado ? "ligado" : "desligado (apagado no servidor)");
    if (j->ligado && j->perfil >= 1 && j->perfil <= CONTA_PERFIL_MAX) {
      restaurarPend[j->perfil] = 1; restaurarDesde[j->perfil] = 0;
    }
  } else if (j->tipo == J_LOTE) {
    diario_nuvem_confirmar(j->perfil, j->env, j->n);
    printf("[diarionuvem] subiram %d\n", j->n);
    esperarAte = agora + 1;
  } else if (j->tipo == J_RESTAURAR) {
    static DiarioEntrada v[500];
    int mais = 0, n;
    long long ate = 0;
    n = diarionuvem_ler_itens(j->resp, v, 500, &mais, &ate);
    if (n > 0) diario_adicionar_lote(j->perfil, v, n);
    printf("[diarionuvem] restaurou %d%s\n", n > 0 ? n : 0, mais ? " (continua)" : "");
    if (mais && ate > restaurarDesde[j->perfil]) restaurarDesde[j->perfil] = ate;
    else restaurarPend[j->perfil] = 0;
  }
  fflush(stdout);
}

void diarionuvem_passo(void) {
  static long long ultimo;
  Job *j = atomic_exchange(&pronto, NULL);
  long long agora;
  int p;
  if (j) { tratar(j); jobLivre(j); atomic_store(&ocupado, 0); }
  if (atomic_load(&ocupado) || !NV_REC_URL[0]) return;
  agora = (long long)time(NULL);
  if (agora < esperarAte || agora == ultimo) return;     // no maximo 1 decisao por segundo
  ultimo = agora;
  if (!diarionuvem_disponivel()) return;
  p = perfis_ativo();
  if (diario_nuvem_estado(p) != DIARIO_NUVEM_SIM) return;
  if (p >= 1 && p <= CONTA_PERFIL_MAX && restaurarPend[p]) { pedirRestaurar(p); return; }
  if (diario_nuvem_pendentes(p) > 0) { esperarAte = agora + FOLGA_S; pedirLote(); }
}

int diarionuvem_ligado(void) { return diario_nuvem_estado(0) == DIARIO_NUVEM_SIM; }

void diarionuvem_definir(int ligado) {
  if (!!ligado == diarionuvem_ligado()) return;
  diario_nuvem_definir(0, ligado ? DIARIO_NUVEM_SIM : DIARIO_NUVEM_NAO);
  // Desligar avisa o servidor (que APAGA a copia). Se a rede falhar agora, a
  // proxima vez que ligar manda o "sim" de novo; o servidor nunca recebe
  // entradas sem o "sim" (409).
  if (diarionuvem_disponivel()) pedirCofre(ligado);
}

void diarionuvem_perguntar_se_preciso(void) {
  static IlhaModal m;
  IlhaAvisoEx a;
  if (!diarionuvem_disponivel() || diario_nuvem_estado(0) != DIARIO_NUVEM_NAO_PERGUNTADO) return;
  if (diario_n(0) <= 0 || ilha_tem("diario-cofre")) return;
  memset(&m, 0, sizeof m);
  memset(&a, 0, sizeof a);
  snprintf(m.kicker, sizeof m.kicker, "%s", i18n("Diário"));
  snprintf(m.titulo, sizeof m.titulo, "%s", i18n("Guardar seu diário na sua conta?"));
  snprintf(m.texto, sizeof m.texto, "%s",
           i18n("Para não perder ao trocar de TV. Só você vê. Dá para desligar em Ajustes, e desligar apaga a cópia da conta."));
  snprintf(m.icone, sizeof m.icone, "aj_shield-check");
  m.tipo = ILHA_ACENTO; m.nBotoes = 2;
  snprintf(m.botao[0], sizeof m.botao[0], "%s", i18n("Guardar"));
  snprintf(m.botao[1], sizeof m.botao[1], "%s", i18n("Agora não"));
  a.chave = "diario-cofre"; a.tipo = ILHA_ACENTO; a.icone = "aj_shield-check";
  a.texto = i18n("Guardar seu diário na sua conta?"); a.ms = 15000u; a.modal = &m; a.abrir = 1;
  ilha_avisar_ex(&a);
}

void diarionuvem_acao(int botao) {
  if (botao == 1) {
    diarionuvem_definir(1);
    ilha_avisar("diario-cofre-ok", ILHA_OK, "aj_shield-check", i18n("Seu diário fica guardado na sua conta."), 4000u, 0);
  } else if (botao == 2) {
    // "Agora nao" e resposta: nao pergunta de novo. Ajustes continua tendo a opcao.
    diario_nuvem_definir(0, DIARIO_NUVEM_NAO);
  }
}
