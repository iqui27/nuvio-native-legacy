// CARTAO DE DICA DO FAROL (P2 — PLANO-GOSTO 4.2, issue 48).
// Ver faroldica.h para o contrato completo.
//
// ORDEM DE TRAVAS (TSan-safe, sem deadlock):
//   `travaDica` protege APENAS o estado interno deste modulo (ativa, cotas,
//   callbacks, pedidos). Nenhuma funcao de modulo externo (avisodisp, dados,
//   catalogo, salvos, diario, progresso, farolsinal, farolcena, farolpeca ou
//   callbacks de usuario) e chamada com `travaDica` tomada.
//
// ZERO LOG DE CONTEUDO: nenhum titulo, nome de perfil ou gosto e impresso.
#include "faroldica.h"
#include "faroltexto.h"
#include "farolsinal.h"
#include "farolacha.h"
#include "avisodisp.h"
#include "catalogo.h"
#include "dados.h"
#include "diario.h"
#include "progresso.h"
#include "salvos.h"
#include "perfis.h"
#include "seguro.h"
#include "idioma.h"
#include "mapa.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef NV_FAROLDICA_PURO
#include "ajustes.h"
#include "text.h"
#include <math.h>
#endif

#define FD_DIA_EPOCA    1767225600LL  // 1/1/2026 UTC (mesma epoca de farol.c/farolsinal.c)
#define FD_PERFIS_MAX   16
#define FD_FATOS_MAX    12
#define FD_CHAVE_PREF   "faroldica:"
#define FD_PARADA_DIAS  21            // serie parada ha >= 3 semanas
#define FD_SALVO_DIAS   30            // salvo antigo ha >= 1 mes

typedef struct {
  int  usado;
  int  perfil;
  int  dia;
  char id[FAROL_ID_TAM];
} FdCota;

static pthread_mutex_t travaDica = PTHREAD_MUTEX_INITIALIZER;
static unsigned  dicaGeracao;
static FarolDica dicaAtiva;
static FdCota    cotas[FD_PERFIS_MAX];
static int       cotasLidas;
static int       prepPerfil = -1;
static int       prepDia    = -1;

static void (*cbComparar)(const char *imdb);
static void (*cbEscolha)(void);
static int  pedComparar;
static char pedCompararImdb[FAROL_REF_TAM];
static int  pedEscolha;

// Relogio injetavel por leitura atomica (sem trava).
static long long (*relFn)(void);

void faroldica_teste_relogio(long long (*s)(void)) {
  __atomic_store_n(&relFn, s, __ATOMIC_RELEASE);
}

static long long agoraS(void) {
  long long (*fn)(void) = __atomic_load_n(&relFn, __ATOMIC_ACQUIRE);
  return fn ? fn() : (long long)time(NULL);
}

static int diaDe(long long quando) {
  long long d = (quando - FD_DIA_EPOCA) / 86400LL;
  if (d < 0) d = 0;
  if (d > 65535) d = 65535;
  return (int)d;
}

static int perfilAtivo(void) {
  int p = perfis_ativo();
  return p > 0 ? p : 1;
}

static void bumpGeracao(void) {
  __atomic_add_fetch(&dicaGeracao, 1u, __ATOMIC_ACQ_REL);
}

unsigned faroldica_geracao(void) {
  return __atomic_load_n(&dicaGeracao, __ATOMIC_ACQUIRE);
}

int faroldica_dia_atual(void) {
  return diaDe(agoraS());
}

int faroldica_acao_disponivel(uint8_t acao) {
  if (acao == FP_ACAO_COMPARAR)
    return __atomic_load_n(&cbComparar, __ATOMIC_ACQUIRE) != NULL;
  if (acao == FP_ACAO_ESCOLHA)
    return __atomic_load_n(&cbEscolha, __ATOMIC_ACQUIRE) != NULL;
  return 1;
}

int faroldica_focavel(const FarolDica *d) {
  return d && d->ativa && d->acao != FP_ACAO_NADA &&
         faroldica_acao_disponivel(d->acao);
}

// ------------------------------------------------------------ avisodisp ----
static void chaveDisp(const char *id, char *dst, size_t cap) {
  snprintf(dst, cap, FD_CHAVE_PREF "%s", id ? id : "");
}

int faroldica_dispensada(const char *id) {
  char ch[AVD_CHAVE];
  if (!id || !id[0]) return 0;
  chaveDisp(id, ch, sizeof ch);
  return avisodisp_tem(ch);
}

// --------------------------------------------------- cota 1/dia/perfil -----
// Persiste "<perfil> <dia> <id>\n" em FAROLDICA_ARQ. I/O sempre fora da trava.
static void cotasGarantirLidas(void) {
  int ja;
  pthread_mutex_lock(&travaDica);
  ja = cotasLidas;
  pthread_mutex_unlock(&travaDica);
  if (ja) return;

  FdCota tmp[FD_PERFIS_MAX];
  int nTmp = 0;
  memset(tmp, 0, sizeof tmp);
  char *s = dados_ler(FAROLDICA_ARQ);
  if (s) {
    char *l = s;
    while (*l && nTmp < FD_PERFIS_MAX) {
      char *fim = strchr(l, '\n');
      int pf = 0, dia = -1;
      char id[FAROL_ID_TAM] = "";
      if (fim) *fim = 0;
      if (sscanf(l, "%d %d %31s", &pf, &dia, id) == 3 && pf > 0 && dia >= 0) {
        int i, achou = -1;
        for (i = 0; i < nTmp; i++) if (tmp[i].perfil == pf) { achou = i; break; }
        if (achou < 0) achou = nTmp++;
        tmp[achou].usado = 1;
        tmp[achou].perfil = pf;
        tmp[achou].dia = dia;
        snprintf(tmp[achou].id, sizeof tmp[achou].id, "%s", id);
      }
      if (!fim) break;
      l = fim + 1;
    }
    free(s);
  }
  pthread_mutex_lock(&travaDica);
  if (!cotasLidas) {
    memcpy(cotas, tmp, sizeof cotas);
    cotasLidas = 1;
  }
  pthread_mutex_unlock(&travaDica);
}

static void cotasGravarSnapshot(const FdCota snap[FD_PERFIS_MAX]) {
  char buf[FD_PERFIS_MAX * (FAROL_ID_TAM + 28)];
  char *w = buf;
  int i;
  buf[0] = 0;
  for (i = 0; i < FD_PERFIS_MAX; i++) {
    if (!snap[i].usado) continue;
    w += snprintf(w, sizeof buf - (size_t)(w - buf), "%d %d %s\n",
                  snap[i].perfil, snap[i].dia, snap[i].id);
  }
  dados_gravar(FAROLDICA_ARQ, buf);
}

// Consulta cota ja gasta para `perfil` em `dia`: 1 se ja existe cota do dia
// (escreve o id em `idOut` se nao NULL), 0 se livre hoje.
static int cotaDoDia(int perfil, int dia, char *idOut, size_t cap) {
  int i, r = 0;
  cotasGarantirLidas();
  pthread_mutex_lock(&travaDica);
  for (i = 0; i < FD_PERFIS_MAX; i++) {
    if (cotas[i].usado && cotas[i].perfil == perfil) {
      if (cotas[i].dia == dia) {
        r = 1;
        if (idOut && cap) snprintf(idOut, cap, "%s", cotas[i].id);
      }
      break;
    }
  }
  pthread_mutex_unlock(&travaDica);
  return r;
}

static void cotaRegistrar(int perfil, int dia, const char *id) {
  FdCota snap[FD_PERFIS_MAX];
  int i, slot = -1;
  cotasGarantirLidas();
  pthread_mutex_lock(&travaDica);
  for (i = 0; i < FD_PERFIS_MAX; i++) {
    if (cotas[i].usado && cotas[i].perfil == perfil) { slot = i; break; }
    if (!cotas[i].usado && slot < 0) slot = i;
  }
  if (slot < 0) slot = FD_PERFIS_MAX - 1;
  cotas[slot].usado = 1;
  cotas[slot].perfil = perfil;
  cotas[slot].dia = dia;
  snprintf(cotas[slot].id, sizeof cotas[slot].id, "%s", id ? id : "");
  memcpy(snap, cotas, sizeof snap);
  pthread_mutex_unlock(&travaDica);
  cotasGravarSnapshot(snap);
}

// ------------------------------------------------- renderizacao de texto ---
static const char *modeloLiteral(const char *m) {
  static const struct { const char *nome, *lit; } MODS[] = {
#define FD_MOD_X(n) { "FT_" #n, FT_##n },
    FAROL_FRASES(FD_MOD_X)
#undef FD_MOD_X
  };
  size_t i;
  if (!m || !m[0]) return NULL;
  for (i = 0; i < sizeof MODS / sizeof *MODS; i++)
    if (!strcmp(MODS[i].nome, m)) return MODS[i].lit;
  return NULL;
}

// Resolve argumento de modelo a texto exibivel (nunca id cru "tt..." ou "c0").
static const char *argResolver(const char *a, int nCand,
                               const char *const *candImdb,
                               const char *const *candTitulo,
                               char *dst, size_t cap) {
  if (!a || !a[0]) return NULL;
  if (a[0] == 'c' && a[1] >= '0' && a[1] <= '9') {
    int idx = atoi(a + 1);
    if (idx < 0 || idx >= nCand) return NULL;
    if (candTitulo && candTitulo[idx] && candTitulo[idx][0]) {
      snprintf(dst, cap, "%s", candTitulo[idx]);
      return dst;
    }
    if (candImdb && candImdb[idx] && candImdb[idx][0]) {
      int ciIdx = cat_indice_por_imdb(candImdb[idx]);
      CatItem ci;
      if (ciIdx >= 0 && cat_copiar_item(ciIdx, &ci) && ci.titulo[0] &&
          !strcmp(ci.imdb, candImdb[idx])) {
        snprintf(dst, cap, "%s", ci.titulo);
        return dst;
      }
      if (diario_nome(candImdb[idx], dst, cap, NULL, NULL, 0) && dst[0])
        return dst;
    }
    return NULL;
  }
  if (a[0] == 't' && a[1] == 't' && a[2] >= '0' && a[2] <= '9') {
    int ciIdx = cat_indice_por_imdb(a);
    CatItem ci;
    if (ciIdx >= 0 && cat_copiar_item(ciIdx, &ci) && ci.titulo[0] &&
        !strcmp(ci.imdb, a)) {
      snprintf(dst, cap, "%s", ci.titulo);
      return dst;
    }
    if (diario_nome(a, dst, cap, NULL, NULL, 0) && dst[0]) return dst;
    // Fallback em salvos (titulo salvo fora do catalogo).
    {
      int i, ns = salvos_n();
      for (i = 0; i < ns; i++) {
        const SalvoItem *sv = salvos_item(i);
        if (sv && !strcmp(sv->id, a) && sv->titulo[0]) {
          snprintf(dst, cap, "%s", sv->titulo);
          return dst;
        }
      }
    }
    return NULL;
  }
  if (a[0] == 'p' || a[0] == 'g' || a[0] == 'k' || a[0] == 'u') {
    // Sem tabela local completa de nomes de pessoa/tema neste modulo: nao vaza
    // id cru (ALMA).
    return NULL;
  }
  // Tempo codificado "<n>y" -> "%d anos" (chave existente em idioma_tab.h);
  // demais unidades curtas ("<n>d", "<n>w", "<n>M") e duracoes ("1h32") passam
  // como vieram, sem introduzir literais nao traduzidos.
  {
    size_t len = strlen(a);
    if (len >= 2 && a[len - 1] == 'y') {
      int ok = 1;
      size_t i;
      for (i = 0; i + 1 < len; i++) if (a[i] < '0' || a[i] > '9') ok = 0;
      if (ok) {
        snprintf(dst, cap, i18n("%d anos"), atoi(a));
        return dst;
      }
    }
  }
  snprintf(dst, cap, "%s", a);
  return dst;
}

static int renderTexto(const FarolCena *c, const FarolTexto *t,
                       int nCand, const char *const *candImdb,
                       const char *const *candTitulo,
                       char *dst, size_t cap) {
  char ordem[FAROL_ARGS + 1], buf[FAROL_ARGS][160];
  const char *sv[FAROL_ARGS];
  int dv[FAROL_ARGS], nO = 0, i, cod = 0;
  const char *fmt, *p;
  dst[0] = 0;
  if (!t || !farolpeca_texto_tem(t)) return 0;
  if (t->usaX) {
    const char *x = farolpeca_x(c, t);
    snprintf(dst, cap, "%s", x ? x : "");
    return dst[0] != 0;
  }
  fmt = modeloLiteral(t->m);
  if (!fmt) return 0;
  fmt = i18n(fmt);
  for (p = fmt; *p && nO < FAROL_ARGS; p++)
    if (p[0] == '%' && (p[1] == 's' || p[1] == 'd')) {
      ordem[nO++] = p[1];
      p++;
    }
  ordem[nO] = 0;
  if (nO != t->nArgs) return 0;
  for (i = 0; i < nO; i++) {
    if (ordem[i] == 'd') {
      dv[i] = atoi(t->a[i]);
      sv[i] = NULL;
    } else {
      const char *s = argResolver(t->a[i], nCand, candImdb, candTitulo,
                                  buf[i], sizeof buf[i]);
      if (!s) return 0;
      sv[i] = s;
      dv[i] = 0;
    }
    cod = cod * 4 + (ordem[i] == 'd' ? 2 : 1);
  }
  switch (cod) {
    case 0:  snprintf(dst, cap, "%s", fmt); break;
    case 1:  snprintf(dst, cap, fmt, sv[0]); break;
    case 2:  snprintf(dst, cap, fmt, dv[0]); break;
    case 5:  snprintf(dst, cap, fmt, sv[0], sv[1]); break;
    case 6:  snprintf(dst, cap, fmt, sv[0], dv[1]); break;
    case 9:  snprintf(dst, cap, fmt, dv[0], sv[1]); break;
    case 10: snprintf(dst, cap, fmt, dv[0], dv[1]); break;
    case 21: snprintf(dst, cap, fmt, sv[0], sv[1], sv[2]); break;
    case 25: snprintf(dst, cap, fmt, sv[0], dv[1], sv[2]); break;
    case 26: snprintf(dst, cap, fmt, sv[0], dv[1], dv[2]); break;
    case 37: snprintf(dst, cap, fmt, dv[0], sv[1], sv[2]); break;
    case 38: snprintf(dst, cap, fmt, dv[0], sv[1], dv[2]); break;
    case 41: snprintf(dst, cap, fmt, dv[0], dv[1], sv[2]); break;
    case 42: snprintf(dst, cap, fmt, dv[0], dv[1], dv[2]); break;
    case 85: snprintf(dst, cap, fmt, sv[0], sv[1], sv[2], sv[3]); break;
    default: return 0;
  }
  return dst[0] != 0;
}

// Resolve o alvo da peca P2 para imdb/tipo/titulo/poster/temporada/episodio.
// Preserva tipo de serie, temporada e episodio vindos do progresso/Diario ou
// dos argumentos da peca mesmo quando o titulo esta fora do catalogo.
static int resolverAlvo(int perfil, const FarolPeca *pc,
                        const FarolRef *ref, int nCand,
                        const char *const *candImdb,
                        const char *const *candTipo,
                        const char *const *candTitulo,
                        char *imdbOut, size_t imdbCap,
                        char *tipoOut, size_t tipoCap,
                        char *titOut, size_t titCap,
                        char *posterOut, size_t posterCap,
                        int *tempOut, int *epOut) {
  const char *imdb = NULL, *tipo = "", *tit = "";
  char poster[512] = "";
  int temp = 0, ep = 0;
  CatItem ci;
  imdbOut[0] = tipoOut[0] = titOut[0] = 0;
  if (posterOut && posterCap) posterOut[0] = 0;
  if (tempOut) *tempOut = 0;
  if (epOut) *epOut = 0;
  if (!ref) return 0;
  if (ref->c >= 0 && ref->c < nCand) {
    if (candImdb && candImdb[ref->c]) imdb = candImdb[ref->c];
    if (candTipo && candTipo[ref->c]) tipo = candTipo[ref->c];
    if (candTitulo && candTitulo[ref->c]) tit = candTitulo[ref->c];
  } else if (ref->s[0]) {
    imdb = ref->s;
  }
  if (!imdb || !imdb[0]) return 0;
  snprintf(imdbOut, imdbCap, "%s", imdb);
  {
    int idx = cat_indice_por_imdb(imdb);
    if (idx >= 0 && cat_copiar_item(idx, &ci) && !strcmp(ci.imdb, imdb)) {
      if (ci.tipo[0]) tipo = ci.tipo;
      if (ci.titulo[0]) tit = ci.titulo;
      if (ci.poster[0]) snprintf(poster, sizeof poster, "%.*s", (int)(sizeof poster - 1), ci.poster);
      if (ci.temporada > 0) temp = ci.temporada;
      if (ci.episodio > 0) ep = ci.episodio;
    }
  }
  if (!tit[0] || !tipo[0] || !poster[0]) {
    int i, ns = salvos_n();
    for (i = 0; i < ns; i++) {
      const SalvoItem *sv = salvos_item(i);
      if (sv && !strcmp(sv->id, imdb)) {
        if (!tit[0] && sv->titulo[0]) tit = sv->titulo;
        if (!tipo[0] && sv->tipo[0]) tipo = sv->tipo;
        if (!poster[0] && sv->poster[0])
          snprintf(poster, sizeof poster, "%.*s", (int)(sizeof poster - 1), sv->poster);
        break;
      }
    }
  }
  // Progresso local do perfil ativo: fonte direta para serie/temporada/episodio
  // fora do catalogo (ex.: serie parada no T2E4).
  if (!tipo[0] || ep <= 0) {
    ProgRegistro pr[32];
    int np = prog_ler(pr, 32), i;
    for (i = 0; i < np; i++) {
      if (strcmp(pr[i].contentId, imdb)) continue;
      if (pr[i].tipo[0] && (!tipo[0] || !strcmp(pr[i].tipo, "series")))
        tipo = !strcmp(pr[i].tipo, "series") ? "series" : "movie";
      if (ep <= 0 && pr[i].episodio > 0) {
        temp = pr[i].temporada;
        ep = pr[i].episodio;
        if (temp > 0 && !tipo[0]) tipo = "series";
      }
      break;
    }
  }
  // Diario do perfil ativo: preserva tipo DIARIO_SERIE e ultimo episodio visto.
  if (!tipo[0] || (!strcmp(tipo, "series") && ep <= 0)) {
    DiarioTitulo dt;
    if (diario_titulo(perfil, imdb, &dt)) {
      if (!tipo[0]) {
        if (dt.tipo == DIARIO_SERIE) tipo = "series";
        else if (dt.tipo == DIARIO_FILME) tipo = "movie";
      }
      if ((!strcmp(tipo, "series") || dt.tipo == DIARIO_SERIE) && ep <= 0) {
        int dtT = 0, dtE = 0;
        if (diario_ultimo_episodio(perfil, imdb, &dtT, &dtE, NULL) && dtE > 0) {
          temp = dtT;
          ep = dtE;
          tipo = "series";
        }
      }
    }
  }
  // Cache de nomes/capa do Diario (compartilhado entre perfis).
  if (!tit[0] || !poster[0]) {
    char tbuf[160] = "", pbuf[512] = "";
    if (diario_nome(imdb, tbuf, sizeof tbuf, NULL, pbuf, sizeof pbuf)) {
      if (!tit[0] && tbuf[0]) snprintf(titOut, titCap, "%s", tbuf);
      if (!poster[0] && pbuf[0]) snprintf(poster, sizeof poster, "%s", pbuf);
    }
  }
  if (!titOut[0] && tit[0]) snprintf(titOut, titCap, "%s", tit);
  // Pistas estruturais da propria peca P2 (ex.: FT_DICA_PAROU_L1 [ref, T, E]).
  if (pc && pc->tipo == FP_DICA && !pc->u.dica.l1.usaX) {
    const FarolTexto *l1 = &pc->u.dica.l1;
    if (!strcmp(l1->m, "FT_DICA_PAROU_L1") && l1->nArgs >= 3) {
      tipo = "series";
      if (temp <= 0) temp = atoi(l1->a[1]);
      if (ep <= 0) ep = atoi(l1->a[2]);
    } else if (!strcmp(l1->m, "FT_DICA_FALTA_L1") && l1->nArgs >= 2) {
      tipo = "series";
      if (temp <= 0) temp = atoi(l1->a[1]);
    } else if (!strcmp(l1->m, "FT_DICA_TEMPORADA")) {
      tipo = "series";
    }
  }
  snprintf(tipoOut, tipoCap, "%s", tipo[0] ? tipo : "movie");
  if (posterOut && posterCap && poster[0])
    snprintf(posterOut, posterCap, "%s", poster);
  if (tempOut) *tempOut = temp > 0 ? temp : 0;
  if (epOut) *epOut = ep > 0 ? ep : 0;
  return 1;
}

// ------------------------------------------------ adocao a partir da cena --
static int adotarPecaDica(const FarolCena *cena, const FarolPeca *pc,
                          int nCand, const char *const *candImdb,
                          const char *const *candTipo,
                          const char *const *candTitulo,
                          int perfil, int dia) {
  FarolDica nova;
  char idCota[FAROL_ID_TAM] = "";
  if (!cena || !pc || pc->tipo != FP_DICA) return 0;
  if (!farolsinal_recomendar() || seguro_perfil_ativo()) return 0;
  if (!faroldica_acao_disponivel(pc->u.dica.acao)) return 0;
  if (faroldica_dispensada(pc->u.dica.id)) return 0;

  // COTA DIARIA: no maximo 1 dica por dia por perfil.
  // Se hoje este perfil ja recebeu uma dica:
  //   - mesma id (ex.: recarregou a cena no mesmo dia): pode manter se nao
  //     dispensada;
  //   - outra id: RECUSADA ate amanha!
  if (cotaDoDia(perfil, dia, idCota, sizeof idCota)) {
    if (strcmp(idCota, pc->u.dica.id)) return 0;
  }

  memset(&nova, 0, sizeof nova);
  nova.perfil = perfil;
  nova.dia = dia;
  nova.acao = pc->u.dica.acao;
  nova.focavel = (nova.acao != FP_ACAO_NADA) ? 1 : 0;
  snprintf(nova.id, sizeof nova.id, "%s", pc->u.dica.id);
  snprintf(nova.icone, sizeof nova.icone, "%s",
           pc->u.dica.icone[0] ? pc->u.dica.icone : "aj_farol");

  if (!renderTexto(cena, &pc->u.dica.l1, nCand, candImdb, candTitulo,
                   nova.l1, sizeof nova.l1))
    return 0;
  if (farolpeca_texto_tem(&pc->u.dica.l2)) {
    // Se l2 veio e nao resolve argumento, omite l2 (nunca vaza id cru).
    if (!renderTexto(cena, &pc->u.dica.l2, nCand, candImdb, candTitulo,
                     nova.l2, sizeof nova.l2))
      nova.l2[0] = 0;
  }

  if (nova.acao != FP_ACAO_NADA && nova.acao != FP_ACAO_ESCOLHA) {
    if (!pc->u.dica.temAlvo ||
        !resolverAlvo(perfil, pc, &pc->u.dica.alvo, nCand, candImdb, candTipo, candTitulo,
                      nova.alvo, sizeof nova.alvo,
                      nova.alvoTipo, sizeof nova.alvoTipo,
                      nova.alvoTitulo, sizeof nova.alvoTitulo,
                      nova.alvoPoster, sizeof nova.alvoPoster,
                      &nova.alvoTemporada, &nova.alvoEpisodio))
      return 0;
  }

  nova.ativa = 1;
  cotaRegistrar(perfil, dia, nova.id);

  pthread_mutex_lock(&travaDica);
  dicaAtiva = nova;
  prepPerfil = perfil;
  prepDia = dia;
  pthread_mutex_unlock(&travaDica);
  bumpGeracao();
  return 1;
}

int faroldica_adotar_cena_json(const char *json, int nCand,
                               const char *const *candImdb,
                               const char *const *candTipo,
                               const char *const *candTitulo) {
  // Static buffer? Nao: evita conflito em TSan quando fios chamam em paralelo.
  // 26 KB cabe folgado no fio principal/teste; no caminho de preparar() fica
  // sob travaCompor ou no chamador.
  FarolCena cena;
  int rc, p, perfil, dia;
  if (!json || !json[0]) return 0;
  if (!farolsinal_recomendar() || seguro_perfil_ativo()) return 0;
  perfil = perfilAtivo();
  dia = diaDe(agoraS());
  rc = farolpeca_ler(&cena, json, nCand);
  if (rc <= 0) return 0;
  for (p = 0; p < cena.nPecas; p++) {
    if (cena.pecas[p].tipo != FP_DICA) continue;
    if (adotarPecaDica(&cena, &cena.pecas[p], nCand, candImdb, candTipo,
                       candTitulo, perfil, dia))
      return 1;
  }
  return 0;
}

int faroldica_adotar_fatos(const FarolDicaFato *fatos, int nFatos,
                           int nivelFrio) {
  FarolDicaFato elig[FD_FATOS_MAX];
  FarolGosto gFake;
  FarolCenaCtx ctx;
  char json[FAROL_CENA_MAX + 1];
  int i, nElig = 0, perfil, dia;
  long long agora;
  char idCota[FAROL_ID_TAM] = "";
  if (!fatos || nFatos <= 0) return 0;
  if (!farolsinal_recomendar() || seguro_perfil_ativo()) return 0;
  agora = agoraS();
  perfil = perfilAtivo();
  dia = diaDe(agora);

  // Se a cota do dia ja foi usada, so a mesma id (nao dispensada) pode subir.
  int temCota = cotaDoDia(perfil, dia, idCota, sizeof idCota);
  if (temCota && faroldica_dispensada(idCota)) return 0;

  for (i = 0; i < nFatos && nElig < FD_FATOS_MAX; i++) {
    if (!fatos[i].id || !fatos[i].id[0]) continue;
    if (!faroldica_acao_disponivel((uint8_t)fatos[i].acao)) continue;
    if (faroldica_dispensada(fatos[i].id)) continue;
    if (temCota && strcmp(fatos[i].id, idCota)) continue;
    elig[nElig++] = fatos[i];
  }
  if (!nElig) return 0;

  memset(&gFake, 0, sizeof gFake);
  // farolcena_home usa ctx.g apenas para farolrec_nivel (frio 0 vs >= 1):
  // nivelFrio >= 1 -> titulos >= 3 (farolrec.h: nivel 1 a partir de 3).
  gFake.titulos = nivelFrio >= 2 ? 12u : (nivelFrio == 1 ? 4u : 0u);
  memset(&ctx, 0, sizeof ctx);
  ctx.g = &gFake;
  ctx.dia = (uint16_t)dia;
  ctx.perfil = perfil;
  ctx.feita = agora;
  ctx.valeS = 86400;
  if (farolcena_home(json, sizeof json, &ctx, elig, nElig) <= 0) return 0;
  return faroldica_adotar_cena_json(json, 0, NULL, NULL, NULL);
}

// -------------------------------------------- coleta de fatos locais -------
static void idSeguro(char *dst, size_t cap, const char *pref, const char *s,
                     int a, int b) {
  char raw[64];
  size_t i, w = 0;
  if (a > 0 && b > 0) snprintf(raw, sizeof raw, "%s_%s_t%de%d", pref, s, a, b);
  else if (a > 0) snprintf(raw, sizeof raw, "%s_%s_t%d", pref, s, a);
  else snprintf(raw, sizeof raw, "%s_%s", pref, s);
  for (i = 0; raw[i] && w + 1 < cap; i++) {
    unsigned char c = (unsigned char)raw[i];
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
        (c >= 'A' && c <= 'Z') || c == '_' || c == '-')
      dst[w++] = (char)c;
    else
      dst[w++] = '_';
  }
  dst[w] = 0;
}

static int ehImdb(const char *s) {
  size_t i;
  if (!s || s[0] != 't' || s[1] != 't' || !s[2]) return 0;
  for (i = 2; s[i]; i++) if (s[i] < '0' || s[i] > '9') return 0;
  return 1;
}

static void fmtDuracao(int min, char *dst, size_t cap) {
  if (min >= 60) {
    int h = min / 60, m = min % 60;
    if (m) snprintf(dst, cap, "%dh%02d", h, m);
    else snprintf(dst, cap, "%dh", h);
  } else {
    snprintf(dst, cap, "%d min", min);
  }
}

static void fmtTempoDias(long dd, char *dst, size_t cap) {
  if (dd < 14)       snprintf(dst, cap, "%ldd", dd > 0 ? dd : 1);
  else if (dd < 63)  snprintf(dst, cap, "%ldw", dd / 7);
  else if (dd < 730) snprintf(dst, cap, "%ldM", dd / 30);
  else               snprintf(dst, cap, "%ldy", dd / 365);
}

int faroldica_preparar(void) {
  FarolDicaFato fatos[FD_FATOS_MAX];
  char ids[FD_FATOS_MAX][FAROL_ID_TAM];
  char alvos[FD_FATOS_MAX][FAROL_REF_TAM];
  char a1[FD_FATOS_MAX][32], a2[FD_FATOS_MAX][32], a3[FD_FATOS_MAX][32];
  char b1[FD_FATOS_MAX][32];
  int nF = 0, perfil, dia, nivel = 0;
  long long agora;
  FarolGosto g;

  // Portoes: recomendar desligado ou modo seguro -> desativa imediatamente.
  if (!farolsinal_recomendar() || seguro_perfil_ativo()) {
    int mudou = 0;
    pthread_mutex_lock(&travaDica);
    if (dicaAtiva.ativa) { memset(&dicaAtiva, 0, sizeof dicaAtiva); mudou = 1; }
    pthread_mutex_unlock(&travaDica);
    if (mudou) bumpGeracao();
    return 0;
  }

  agora = agoraS();
  perfil = perfilAtivo();
  dia = diaDe(agora);

  // Caminho rapido: ja preparado hoje para este perfil.
  pthread_mutex_lock(&travaDica);
  if (prepPerfil == perfil && prepDia == dia) {
    int tem = dicaAtiva.ativa && dicaAtiva.perfil == perfil && dicaAtiva.dia == dia;
    char idAt[FAROL_ID_TAM] = "";
    if (tem) snprintf(idAt, sizeof idAt, "%s", dicaAtiva.id);
    pthread_mutex_unlock(&travaDica);
    if (tem && faroldica_dispensada(idAt)) {
      pthread_mutex_lock(&travaDica);
      if (dicaAtiva.ativa && !strcmp(dicaAtiva.id, idAt))
        memset(&dicaAtiva, 0, sizeof dicaAtiva);
      pthread_mutex_unlock(&travaDica);
      bumpGeracao();
      return 0;
    }
    return tem;
  }
  pthread_mutex_unlock(&travaDica);

  // Verifica se o perfil mudou e limpa dica de outro perfil antes de compor.
  {
    int limpar = 0;
    pthread_mutex_lock(&travaDica);
    if (dicaAtiva.ativa && (dicaAtiva.perfil != perfil || dicaAtiva.dia != dia)) {
      memset(&dicaAtiva, 0, sizeof dicaAtiva);
      limpar = 1;
    }
    pthread_mutex_unlock(&travaDica);
    if (limpar) bumpGeracao();
  }

  if (farolsinal_modelo(&g)) nivel = farolrec_nivel(&g);

  // 1) Serie parada ha semanas (PLANO-GOSTO 4.2): progresso local com ultima
  //    vez ha >= 21 dias.
  if (nivel >= 1 && nF < FD_FATOS_MAX) {
    ProgRegistro pr[32];
    int np = prog_ler(pr, 32), i;
    for (i = 0; i < np && nF < FD_FATOS_MAX; i++) {
      long long ultS = pr[i].lastWatchedMs / 1000LL;
      long dd;
      int sem, t = pr[i].temporada, e = pr[i].episodio;
      if (strcmp(pr[i].tipo, "series") || t <= 0 || e <= 0) continue;
      if (!ehImdb(pr[i].contentId)) continue;
      if (ultS <= 0 || agora <= ultS) continue;
      dd = (long)((agora - ultS) / 86400LL);
      if (dd < FD_PARADA_DIAS) continue;
      sem = (int)(dd / 7);
      if (sem < 1) sem = 1;
      idSeguro(ids[nF], sizeof ids[nF], "d_parou", pr[i].contentId, t, e);
      if (faroldica_dispensada(ids[nF])) continue;
      snprintf(alvos[nF], sizeof alvos[nF], "%s", pr[i].contentId);
      snprintf(a1[nF], sizeof a1[nF], "%d", t);
      snprintf(a2[nF], sizeof a2[nF], "%d", e);
      snprintf(b1[nF], sizeof b1[nF], "%d", sem);
      memset(&fatos[nF], 0, sizeof fatos[nF]);
      fatos[nF].tipo = FC_DICA_PAROU;
      fatos[nF].acao = FP_ACAO_ABRIR;
      fatos[nF].id = ids[nF];
      fatos[nF].alvo = alvos[nF];
      fatos[nF].l1a[0] = alvos[nF];
      fatos[nF].l1a[1] = a1[nF];
      fatos[nF].l1a[2] = a2[nF];
      fatos[nF].l1na = 3;
      fatos[nF].l2a[0] = b1[nF];
      fatos[nF].l2na = 1;
      nF++;
      break;
    }
  }

  // 2) Salvo antigo com duracao conhecida (PLANO-GOSTO 4.2).
  if (nivel >= 1 && nF < FD_FATOS_MAX) {
    int ns = salvos_n(), i;
    for (i = 0; i < ns && nF < FD_FATOS_MAX; i++) {
      const SalvoItem *sv = salvos_item(i);
      DiarioTitulo dt;
      long dd;
      int min = 0;
      if (!sv || !ehImdb(sv->id) || sv->quandoS <= 0 || agora <= sv->quandoS) continue;
      dd = (long)((agora - sv->quandoS) / 86400LL);
      if (dd < FD_SALVO_DIAS) continue;
      if (diario_titulo(perfil, sv->id, &dt) && dt.vezes > 0) continue;
      min = farolacha_minutos_meta(sv->meta);
      if (min <= 0) {
        int ciIdx = cat_indice_por_imdb(sv->id);
        CatItem ci;
        if (ciIdx >= 0 && cat_copiar_item(ciIdx, &ci))
          min = farolacha_minutos_meta(ci.meta);
      }
      if (min <= 0) continue;
      idSeguro(ids[nF], sizeof ids[nF], "d_salvo", sv->id, 0, 0);
      if (faroldica_dispensada(ids[nF])) continue;
      snprintf(alvos[nF], sizeof alvos[nF], "%.*s",
               (int)(sizeof alvos[nF] - 1), sv->id);
      fmtTempoDias(dd, a1[nF], sizeof a1[nF]);
      fmtDuracao(min, b1[nF], sizeof b1[nF]);
      memset(&fatos[nF], 0, sizeof fatos[nF]);
      fatos[nF].tipo = FC_DICA_SALVO;
      fatos[nF].acao = FP_ACAO_ABRIR;
      fatos[nF].id = ids[nF];
      fatos[nF].alvo = alvos[nF];
      fatos[nF].l1a[0] = a1[nF];
      fatos[nF].l1na = 1;
      fatos[nF].l2a[0] = b1[nF];
      fatos[nF].l2na = 1;
      nF++;
      break;
    }
  }

  // 3) Curto para hoje: filme nao visto <= 100 min no catalogo.
  if (nivel >= 1 && nF < FD_FATOS_MAX) {
    FarolAchado ach[1];
    unsigned rev = 0;
    if (farolacha_listar_rev(FA_CURTO, FAROL_ACHA_CURTO_PADRAO, ach, 1,
                             (unsigned)dia ^ (unsigned)perfil, &rev) > 0) {
      const CatItem *ci = farolacha_item(&ach[0], rev);
      if (ci && ehImdb(ci->imdb)) {
        idSeguro(ids[nF], sizeof ids[nF], "d_curto", ci->imdb, 0, 0);
        if (!faroldica_dispensada(ids[nF])) {
          snprintf(alvos[nF], sizeof alvos[nF], "%.*s",
                   (int)(sizeof alvos[nF] - 1), ci->imdb);
          memset(&fatos[nF], 0, sizeof fatos[nF]);
          fatos[nF].tipo = FC_DICA_CURTO;
          fatos[nF].acao = FP_ACAO_TOCAR;
          fatos[nF].id = ids[nF];
          fatos[nF].alvo = alvos[nF];
          fatos[nF].l1a[0] = alvos[nF];
          fatos[nF].l1na = 1;
          fatos[nF].l2na = 0;
          nF++;
        }
      }
    }
  }

  // 4) Assistente de escolha: entra no rodizio SOMENTE quando a rota de #50
  //    estiver registrada (sem prometer acao sem destino ao usuario).
  if (nF < FD_FATOS_MAX && faroldica_acao_disponivel(FP_ACAO_ESCOLHA) &&
      !faroldica_dispensada("d_perguntas")) {
    snprintf(ids[nF], sizeof ids[nF], "d_perguntas");
    alvos[nF][0] = 0;
    memset(&fatos[nF], 0, sizeof fatos[nF]);
    fatos[nF].tipo = FC_DICA_PERGUNTAS;
    fatos[nF].acao = FP_ACAO_ESCOLHA;
    fatos[nF].id = ids[nF];
    fatos[nF].alvo = "";
    fatos[nF].l1na = 0;
    fatos[nF].l2na = 0;
    nF++;
  }

  (void)a3;
  if (nF > 0 && faroldica_adotar_fatos(fatos, nF, nivel)) return 1;

  // So marca (perfil, dia) como encerrado se a cota de hoje ja foi gasta;
  // sem dica adotada e com cota livre, uma carga posterior de catalogo no
  // mesmo dia continua elegivel para compor P2.
  if (cotaDoDia(perfil, dia, NULL, 0)) {
    pthread_mutex_lock(&travaDica);
    prepPerfil = perfil;
    prepDia = dia;
    pthread_mutex_unlock(&travaDica);
  }
  return 0;
}

// -------------------------------------------------- consulta e dispensa ----
int faroldica_atual(FarolDica *out) {
  FarolDica snap;
  int perfil, dia;
  if (out) memset(out, 0, sizeof *out);
  if (!farolsinal_recomendar() || seguro_perfil_ativo()) return 0;
  perfil = perfilAtivo();
  dia = diaDe(agoraS());
  pthread_mutex_lock(&travaDica);
  snap = dicaAtiva;
  if (snap.ativa && snap.perfil == perfil &&
      (snap.dia != dia || !faroldica_acao_disponivel(snap.acao))) {
    memset(&dicaAtiva, 0, sizeof dicaAtiva);
    pthread_mutex_unlock(&travaDica);
    bumpGeracao();
    return 0;
  }
  pthread_mutex_unlock(&travaDica);
  if (!snap.ativa || snap.perfil != perfil || snap.dia != dia) return 0;
  if (faroldica_dispensada(snap.id)) {
    pthread_mutex_lock(&travaDica);
    if (dicaAtiva.ativa && !strcmp(dicaAtiva.id, snap.id) &&
        dicaAtiva.perfil == perfil)
      memset(&dicaAtiva, 0, sizeof dicaAtiva);
    pthread_mutex_unlock(&travaDica);
    bumpGeracao();
    return 0;
  }
  if (out) *out = snap;
  return 1;
}

int faroldica_dispensar_id(const char *id) {
  char ch[AVD_CHAVE];
  int mudou = 0, perfil;
  if (!id || !id[0]) return 0;
  perfil = perfilAtivo();
  chaveDisp(id, ch, sizeof ch);
  avisodisp_por(ch);
  pthread_mutex_lock(&travaDica);
  if (dicaAtiva.ativa && dicaAtiva.perfil == perfil &&
      !strcmp(dicaAtiva.id, id)) {
    memset(&dicaAtiva, 0, sizeof dicaAtiva);
    mudou = 1;
  }
  pthread_mutex_unlock(&travaDica);
  if (mudou) bumpGeracao();
  return 1;
}

int faroldica_dispensar(void) {
  char id[FAROL_ID_TAM] = "";
  int perfil = perfilAtivo();
  pthread_mutex_lock(&travaDica);
  if (dicaAtiva.ativa && dicaAtiva.perfil == perfil && dicaAtiva.id[0])
    snprintf(id, sizeof id, "%s", dicaAtiva.id);
  pthread_mutex_unlock(&travaDica);
  if (!id[0]) return 0;
  return faroldica_dispensar_id(id);
}

// --------------------------------------------------- execucao das acoes ----
void faroldica_ao_comparar(void (*fn)(const char *imdb)) {
  int mudou = 0;
  pthread_mutex_lock(&travaDica);
  __atomic_store_n(&cbComparar, fn, __ATOMIC_RELEASE);
  if (!fn && dicaAtiva.ativa && dicaAtiva.acao == FP_ACAO_COMPARAR) {
    memset(&dicaAtiva, 0, sizeof dicaAtiva);
    mudou = 1;
  }
  pthread_mutex_unlock(&travaDica);
  if (mudou) bumpGeracao();
}

void faroldica_ao_escolha(void (*fn)(void)) {
  int mudou = 0;
  pthread_mutex_lock(&travaDica);
  __atomic_store_n(&cbEscolha, fn, __ATOMIC_RELEASE);
  if (!fn && dicaAtiva.ativa && dicaAtiva.acao == FP_ACAO_ESCOLHA) {
    memset(&dicaAtiva, 0, sizeof dicaAtiva);
    mudou = 1;
  }
  pthread_mutex_unlock(&travaDica);
  if (mudou) bumpGeracao();
}

int faroldica_pediu_comparar(char *imdb, size_t cap) {
  int r = 0;
  pthread_mutex_lock(&travaDica);
  if (pedComparar) {
    r = 1;
    if (imdb && cap) snprintf(imdb, cap, "%s", pedCompararImdb);
    pedComparar = 0;
    pedCompararImdb[0] = 0;
  }
  pthread_mutex_unlock(&travaDica);
  return r;
}

int faroldica_pediu_escolha(void) {
  int r = 0;
  pthread_mutex_lock(&travaDica);
  if (pedEscolha) {
    r = 1;
    pedEscolha = 0;
  }
  pthread_mutex_unlock(&travaDica);
  return r;
}

static int garantirItemNoCatalogo(const FarolDica *d, CatItem *ciOut) {
  int idx;
  CatItem tmp;
  if (!d || !d->alvo[0]) return -1;
  idx = cat_indice_por_imdb(d->alvo);
  if (idx >= 0 && cat_copiar_item(idx, &tmp) && !strcmp(tmp.imdb, d->alvo)) {
    if (ciOut) *ciOut = tmp;
    return idx;
  }
  memset(&tmp, 0, sizeof tmp);
  snprintf(tmp.imdb, sizeof tmp.imdb, "%s", d->alvo);
  snprintf(tmp.tipo, sizeof tmp.tipo, "%s",
           d->alvoTipo[0] ? d->alvoTipo : "movie");
  snprintf(tmp.titulo, sizeof tmp.titulo, "%s",
           d->alvoTitulo[0] ? d->alvoTitulo : d->alvo);
  if (d->alvoPoster[0])
    snprintf(tmp.poster, sizeof tmp.poster, "%s", d->alvoPoster);
  if (!strcmp(tmp.tipo, "series")) {
    if (d->alvoTemporada > 0) tmp.temporada = d->alvoTemporada;
    if (d->alvoEpisodio > 0) tmp.episodio = d->alvoEpisodio;
  }
  idx = cat_acrescentar(&tmp);
  if (idx >= 0 && ciOut) *ciOut = tmp;
  return idx;
}

FarolDicaExec faroldica_executar(const FarolDica *d) {
  FarolDicaExec res;
  memset(&res, 0, sizeof res);
  res.tipo = FD_EXEC_NADA;
  res.catIdx = -1;
  if (!d || !d->ativa || d->acao == FP_ACAO_NADA) return res;
  if (!farolsinal_recomendar() || seguro_perfil_ativo()) return res;
  if (d->perfil != perfilAtivo() || d->dia != diaDe(agoraS())) return res;
  if (faroldica_dispensada(d->id)) return res;
  if (!faroldica_acao_disponivel(d->acao)) return res;

  switch (d->acao) {
    case FP_ACAO_ABRIR: {
      CatItem ci;
      int idx = garantirItemNoCatalogo(d, &ci);
      res.tipo = FD_EXEC_ABRIR;
      snprintf(res.imdb, sizeof res.imdb, "%s", d->alvo);
      res.catIdx = idx;
      res.ok = (idx >= 0 && d->alvo[0] != 0) ? 1 : 0;
      return res;
    }
    case FP_ACAO_TOCAR: {
      CatItem ci;
      int idx = garantirItemNoCatalogo(d, &ci);
      res.tipo = FD_EXEC_TOCAR;
      snprintf(res.imdb, sizeof res.imdb, "%s", d->alvo);
      res.catIdx = idx;
      res.ok = (idx >= 0 && d->alvo[0] != 0) ? 1 : 0;
      return res;
    }
    case FP_ACAO_SALVAR: {
      CatItem ci;
      int idx = cat_indice_por_imdb(d->alvo);
      res.tipo = FD_EXEC_SALVAR;
      snprintf(res.imdb, sizeof res.imdb, "%s", d->alvo);
      if (!d->alvo[0]) return res;
      if (idx >= 0 && cat_copiar_item(idx, &ci) && !strcmp(ci.imdb, d->alvo)) {
        salvos_definir(&ci, 1);
        cat_definir_na_lista_imdb(d->alvo, 1);
        res.catIdx = idx;
      } else {
        memset(&ci, 0, sizeof ci);
        snprintf(ci.imdb, sizeof ci.imdb, "%s", d->alvo);
        snprintf(ci.tipo, sizeof ci.tipo, "%s",
                 d->alvoTipo[0] ? d->alvoTipo : "movie");
        snprintf(ci.titulo, sizeof ci.titulo, "%s",
                 d->alvoTitulo[0] ? d->alvoTitulo : d->alvo);
        if (d->alvoPoster[0])
          snprintf(ci.poster, sizeof ci.poster, "%s", d->alvoPoster);
        salvos_definir(&ci, 1);
        cat_definir_na_lista_imdb(d->alvo, 1);
        res.catIdx = -1;
      }
      res.ok = 1;
      return res;
    }
    case FP_ACAO_COMPARAR: {
      void (*fn)(const char *) = NULL;
      res.tipo = FD_EXEC_COMPARAR;
      if (!d->alvo[0]) return res;
      snprintf(res.imdb, sizeof res.imdb, "%s", d->alvo);
      res.catIdx = cat_indice_por_imdb(d->alvo);
      pthread_mutex_lock(&travaDica);
      fn = __atomic_load_n(&cbComparar, __ATOMIC_ACQUIRE);
      if (fn) {
        pedComparar = 1;
        snprintf(pedCompararImdb, sizeof pedCompararImdb, "%s", d->alvo);
      }
      pthread_mutex_unlock(&travaDica);
      if (!fn) return res;
      fn(res.imdb);
      res.ok = 1;
      return res;
    }
    case FP_ACAO_ESCOLHA: {
      void (*fn)(void) = NULL;
      res.tipo = FD_EXEC_ESCOLHA;
      pthread_mutex_lock(&travaDica);
      fn = __atomic_load_n(&cbEscolha, __ATOMIC_ACQUIRE);
      if (fn) pedEscolha = 1;
      pthread_mutex_unlock(&travaDica);
      if (!fn) return res;
      fn();
      res.ok = 1;
      return res;
    }
    default:
      return res;
  }
}

const char *faroldica_rotulo_acao(uint8_t acao) {
  if (!faroldica_acao_disponivel(acao)) return "";
  switch (acao) {
    case FP_ACAO_ABRIR:    return i18n("Abrir");
    case FP_ACAO_TOCAR:    return i18n("Reproduzir");
    case FP_ACAO_COMPARAR: return i18n(FT_CMP_TITULO);
    case FP_ACAO_ESCOLHA:  return i18n(FT_ACHAR_O_QUE_VER);
    case FP_ACAO_SALVAR:   return i18n("Salvar");
    default:               return "";
  }
}

// --------------------------------------------------------- ciclo de vida ---
void faroldica_esquecer(void) {
  pthread_mutex_lock(&travaDica);
  memset(&dicaAtiva, 0, sizeof dicaAtiva);
  prepPerfil = -1;
  prepDia = -1;
  pedComparar = 0;
  pedCompararImdb[0] = 0;
  pedEscolha = 0;
  pthread_mutex_unlock(&travaDica);
  bumpGeracao();
}

void faroldica_esquecer_disco(void) {
  pthread_mutex_lock(&travaDica);
  memset(&dicaAtiva, 0, sizeof dicaAtiva);
  memset(cotas, 0, sizeof cotas);
  cotasLidas = 1;
  prepPerfil = -1;
  prepDia = -1;
  pedComparar = 0;
  pedCompararImdb[0] = 0;
  pedEscolha = 0;
  pthread_mutex_unlock(&travaDica);
  dados_gravar(FAROLDICA_ARQ, "");
  bumpGeracao();
}

void faroldica_teste_reiniciar(void) {
  pthread_mutex_lock(&travaDica);
  memset(&dicaAtiva, 0, sizeof dicaAtiva);
  memset(cotas, 0, sizeof cotas);
  cotasLidas = 0;
  prepPerfil = -1;
  prepDia = -1;
  __atomic_store_n(&cbComparar, NULL, __ATOMIC_RELEASE);
  __atomic_store_n(&cbEscolha, NULL, __ATOMIC_RELEASE);
  pedComparar = 0;
  pedCompararImdb[0] = 0;
  pedEscolha = 0;
  pthread_mutex_unlock(&travaDica);
  faroldica_teste_relogio(NULL);
  bumpGeracao();
}

// --------------------------------------------------------------- desenho ---
#ifndef NV_FAROLDICA_PURO
void faroldica_desenhar(const FarolDica *d, GfxRect r, float raio,
                        float foco, float alpha, int reduzido, int salvo) {
  float ar, ag, ab, pad, innerW, y, kFoco;
  const char *rot;
  if (!d || !d->ativa || alpha <= 0.004f || r.w <= 8.0f || r.h <= 8.0f) return;

  ajustes_acento(&ar, &ag, &ab);
  kFoco = d->focavel ? (reduzido ? (foco >= 0.5f ? 1.0f : 0.0f)
                                 : (foco < 0.0f ? 0.0f : (foco > 1.0f ? 1.0f : foco)))
                     : 0.0f;

  // Superficie --ilha (ALMA 4): vidro frio ou solido #121316 sem FBO extra.
  gfx_vidro_ou_solido(r, raio, GFX_VIDRO_DENS, alpha,
                      0.071f, 0.075f, 0.086f, 0.94f * alpha);

  // Luz suave no canto superior no acento (assinatura Glass 2.1 do Farol).
  gfx_cor(r, raio, ar, ag, ab, (0.06f + 0.08f * kFoco) * alpha);

  // Borda sutil em repouso; anel branco/acento quando focavel e focado.
  gfx_anel(r, raio, 1.5f, 1.0f, 1.0f, 1.0f, (0.10f + 0.14f * kFoco) * alpha);
  if (kFoco > 0.01f)
    gfx_anel(r, raio, 3.0f, ar, ag, ab, 0.85f * kFoco * alpha);

  pad = fmaxf(14.0f, r.w * 0.08f);
  innerW = r.w - pad * 2.0f;
  if (innerW <= 16.0f) return;

  // Cabecalho: ponto de luz de 10 px (PONTO_ENQ, ALMA 4) + "Farol" + icone.
  y = r.y + pad;
  {
    TxtLinha lt = txt_linha_corta(TXT_MINI, i18n(FT_NOME),
                                  243, 242, 239, 255, innerW - 36.0f);
    float cy = y + lt.h * 0.5f;
    gfx_cor((GfxRect){ r.x + pad, cy - 5.0f, 10.0f, 10.0f }, 5.0f,
            ar, ag, ab, (0.85f + 0.15f * kFoco) * alpha);
    txt_desenhar_alpha(lt, r.x + pad + 16.0f, y, 0.92f * alpha);
    if (d->icone[0]) {
      float isz = 18.0f;
      gfx_icone((GfxRect){ r.x + r.w - pad - isz, cy - isz * 0.5f, isz, isz },
                d->icone, ar, ag, ab, 0.85f * alpha);
    }
    y += lt.h + 14.0f;
  }

  // Corpo: linha 1 (primaria #F3F2EF) e linha 2 opcional (secundaria).
  if (d->l1[0] && y < r.y + r.h - pad - 24.0f) {
    float h1 = txt_bloco_corta(TXT_ILHA_ITEM, d->l1, 243, 242, 239,
                               r.x + pad, y, innerW, 28.0f, alpha, 3);
    y += h1 + 8.0f;
  }
  if (d->l2[0] && y < r.y + r.h - pad - 36.0f) {
    txt_bloco_corta(TXT_MINI, d->l2, 168, 173, 181,
                    r.x + pad, y, innerW, 22.0f, 0.92f * alpha, 2);
  }

  // Rodape de acao: apenas para cartoes focaveis (acao != FP_ACAO_NADA).
  if (d->focavel) {
    rot = (d->acao == FP_ACAO_SALVAR && salvo)
        ? i18n("Salvo") : faroldica_rotulo_acao(d->acao);
    if (rot && rot[0]) {
      TxtLinha la = txt_linha_corta(TXT_MINI, rot, 243, 242, 239, 255,
                                    innerW - 20.0f);
      float ph = la.h + 10.0f;
      float pw = fminf(innerW, la.w + 22.0f);
      GfxRect pill = { r.x + pad, r.y + r.h - pad - ph, pw, ph };
      if (kFoco > 0.5f)
        gfx_cor(pill, 0.5f, ar, ag, ab, 0.92f * alpha);
      else
        gfx_cor(pill, 0.5f, 1.0f, 1.0f, 1.0f, 0.14f * alpha);
      txt_desenhar_alpha(la, pill.x + (pill.w - la.w) * 0.5f,
                         pill.y + (pill.h - la.h) * 0.5f, alpha);
    }
  }
}
#endif // !NV_FAROLDICA_PURO
