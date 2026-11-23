#include "homeassistiu.h"
#include "catalogo.h"
#include "descoberta.h"
#include "farolsinal.h"
#include "idbase.h"
#include "idioma.h"
#include "mapa.h"
#include "perfilinfantil.h"
#include "perfisessao.h"
#include "progresso.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HIST_MAX 32
#define HIST_LOTE 128
#define REDE_INTERVALO_MS 60000u
#define REDE_BACKOFF_MAX_MS (8u * REDE_INTERVALO_MS)

typedef struct {
  NvPerfilSessao sessao;
  char id[64], idioma[12];
  long tmdb;
  int serie;
} Consulta;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static Consulta consulta;
static char chaveTmdb[96];
static int ocupada, pedida, nRemotos;
static long remotos[MAPA_REC_MAX];
static unsigned remotoRev;
static uint32_t ultimoPedido;
static uint32_t backoff;
static int itens[HOME_ASSISTIU_MAX], nItens;
static char tituloSemente[sizeof ((CatItem *)0)->titulo];
static unsigned revisao;

static const CatItem *item(int i) {
  return i >= 0 && i < cat_n() ? cat_item(i) : NULL;
}

static int tipoValido(const CatItem *c) {
  return c && c->imdb[0] && (!strcmp(c->tipo, "movie") || !strcmp(c->tipo, "series"));
}

static long idTmdb(const CatItem *c) {
  const char *id = c->imdb;
  if (c->tmdb > 0) return c->tmdb;
  if (strncmp(id, "tmdb:", 5)) return 0;
  id += 5;
  if (*id == 't') id++;
  if (!*id || id[strspn(id, "0123456789")]) return 0;
  return strtol(id, NULL, 10);
}

static int mesmoTitulo(const CatItem *c, const CatItem *b) {
  if (!b) return 0;
  long tmdb = idTmdb(c);
  size_t n = idbase_len(c->imdb);
  return (n == idbase_len(b->imdb) && !strncmp(c->imdb, b->imdb, n)) ||
         (tmdb > 0 && tmdb == idTmdb(b));
}

static const CatItem *semente(int reiniciar, int infantil, int *pendente) {
  static int pos, melhor;
  static long long maisRecente;
  int total = cat_n();
  if (reiniciar) {
    ProgRegistro reg[HIST_MAX];
    int n = prog_ler(reg, HIST_MAX);
    pos = 0;
    melhor = -1;
    maisRecente = 0;
    for (int j = 0; j < n; j++) {
      const ProgRegistro *p = &reg[j];
      if (p->tocou == 0 || p->durSeg <= 0 || p->posSeg < p->durSeg * 0.9) continue;
      int i = cat_indice_por_imdb_tipo(p->contentId, p->tipo);
      const CatItem *c = item(i);
      if (tipoValido(c) && c->titulo[0] &&
          (!infantil || perfil_infantil_classif_ok(c->classificacao))) {
        melhor = i;
        pos = total;
        break;
      }
    }
  }
  for (int fim = pos + HIST_LOTE; pos < total && pos < fim; pos++) {
    const CatItem *c = item(pos);
    long long quando = 0;
    if (tipoValido(c) && c->titulo[0] &&
        (!infantil || perfil_infantil_classif_ok(c->classificacao)) &&
        cat_historico_estado_tempo(c->imdb, c->tipo, &quando) == 1 &&
        quando > maisRecente) {
      melhor = pos;
      maisRecente = quando;
    }
  }
  *pendente = pos < total;
  return *pendente ? NULL : item(melhor);
}

static int montar(const CatItem *base, const long *ids, int nIds, int infantil,
                  int saida[HOME_ASSISTIU_MAX], int *comTmdb) {
  MapaEtiqueta gen[MAPA_GEN_MAX];
  int nGen, notas[HOME_ASSISTIU_MAX], n = 0, total = cat_n();
  mapa_generos_locais(base->genero, gen, &nGen, MAPA_GEN_MAX);
  for (int i = 0; i < total; i++) {
    const CatItem *c = item(i);
    if (!tipoValido(c) || strcmp(c->tipo, base->tipo) || !c->titulo[0] ||
        !c->poster[0] || c->progresso > 0 || mesmoTitulo(c, base) ||
        (infantil && !perfil_infantil_classif_ok(c->classificacao))) continue;
    int pontos = 0, j;
    long tmdb = idTmdb(c);
    for (j = 0; tmdb > 0 && j < nIds; j++) if (ids[j] == tmdb) {
      pontos = (MAPA_REC_MAX - j + MAPA_GEN_MAX) * 128;
      break;
    }
    if (!pontos && nGen) {
      MapaEtiqueta cg[MAPA_GEN_MAX];
      int nc;
      mapa_generos_locais(c->genero, cg, &nc, MAPA_GEN_MAX);
      for (int g = 0; g < nGen; g++)
        for (j = 0; j < nc; j++) if (gen[g].id == cg[j].id) {
          pontos += 128;
          break;
        }
    }
    if (!pontos || cat_visto(c)) continue;
    pontos += c->nota < 0 ? 0 : c->nota > 100 ? 100 : c->nota;
    for (j = 0; j < n; j++) if (mesmoTitulo(c, item(saida[j]))) break;
    if (j < n) {
      if (pontos <= notas[j]) continue;
      for (; j + 1 < n; j++) { notas[j] = notas[j+1]; saida[j] = saida[j+1]; }
      n--;
    }
    for (j = 0; j < n && notas[j] >= pontos; j++) {}
    if (j == HOME_ASSISTIU_MAX) continue;
    if (n < HOME_ASSISTIU_MAX) n++;
    for (int k = n - 1; k > j; k--) { notas[k] = notas[k-1]; saida[k] = saida[k-1]; }
    notas[j] = pontos;
    saida[j] = i;
  }
  *comTmdb = n > 0 && notas[0] >= (MAPA_GEN_MAX + 1) * 128;
  return n;
}

static void *buscar(void *arg) {
  (void)arg;
  Consulta q = consulta;
  long ids[MAPA_REC_MAX];
  int n = 0;
  if (nv_perfil_sessao_atual(q.sessao))
    n = mapa_recomendacoes_tmdb(q.id, q.tmdb, q.serie, chaveTmdb,
                                 q.idioma, ids, MAPA_REC_MAX);
  pthread_mutex_lock(&trava);
  nRemotos = nv_perfil_sessao_atual(q.sessao) ? n : 0;
  memcpy(remotos, ids, (size_t)nRemotos * sizeof *ids);
  memset(chaveTmdb, 0, sizeof chaveTmdb);
  ocupada = 0;
  __atomic_add_fetch(&remotoRev, 1u, __ATOMIC_RELEASE);
  pthread_mutex_unlock(&trava);
  return NULL;
}

static int recomendacoes(const Consulta *q, const char *chave, uint32_t agora,
                          long *ids, uint32_t *aguarda) {
  pthread_t fio;
  int n = 0;
  pthread_mutex_lock(&trava);
  int mesma = pedida && consulta.sessao == q->sessao && consulta.tmdb == q->tmdb &&
              consulta.serie == q->serie && !strcmp(consulta.id, q->id) &&
              !strcmp(consulta.idioma, q->idioma);
  uint32_t intervalo = mesma ? backoff : REDE_INTERVALO_MS;
  if (mesma && nRemotos) {
    n = nRemotos;
    memcpy(ids, remotos, (size_t)n * sizeof *ids);
  } else if (ocupada) {
    // O fim do trabalho muda remotoRev.
  } else if (pedida && agora - ultimoPedido < intervalo) {
    *aguarda = intervalo;
  } else if (q->tmdb > 0 || idbase_e_imdb(q->id)) {
    consulta = *q;
    snprintf(chaveTmdb, sizeof chaveTmdb, "%s", chave);
    ocupada = pedida = 1;
    nRemotos = 0;
    ultimoPedido = agora;
    backoff = !mesma ? REDE_INTERVALO_MS :
              backoff < REDE_BACKOFF_MAX_MS ? 2 * backoff : backoff;
    if (pthread_create(&fio, NULL, buscar, NULL) == 0) pthread_detach(fio);
    else { ocupada = 0; memset(chaveTmdb, 0, sizeof chaveTmdb); *aguarda = backoff; }
  }
  pthread_mutex_unlock(&trava);
  return n;
}

unsigned home_assistiu_passo(uint32_t agora) {
  static NvPerfilSessao ultimaSessao;
  static unsigned ultimoCat, ultimoRemoto;
  static int ultimoLigado, ultimoInfantil, ultimaRede, pendente;
  static uint32_t aguarda;
  static char ultimoIdioma[12];
  NvPerfilSessao sessao = nv_perfil_sessao_ler();
  unsigned cat = cat_revisao_itens(), remoto = __atomic_load_n(&remotoRev, __ATOMIC_ACQUIRE);
  int ligado = farolsinal_recomendar(), infantil = perfil_infantil_ativo();
  const char *chave = desc_chave_tmdb(), *idioma = desc_tmdb_idioma();
  int rede = chave[0] != 0;
  int reiniciar = sessao != ultimaSessao || cat != ultimoCat ||
                 ligado != ultimoLigado || infantil != ultimoInfantil;
  if (!reiniciar && !pendente && remoto == ultimoRemoto && rede == ultimaRede &&
      !strcmp(ultimoIdioma, idioma) && (!aguarda || agora - ultimoPedido < aguarda))
    return revisao;
  int saida[HOME_ASSISTIU_MAX], n = 0;
  char titulo[sizeof tituloSemente] = "";
  aguarda = pendente = 0;
  if (ligado && cat_n()) {
    const CatItem *base = semente(reiniciar, infantil, &pendente);
    if (base && nv_perfil_sessao_atual(sessao)) {
      long ids[MAPA_REC_MAX];
      int nIds = 0, comTmdb;
      if (rede) {
        Consulta q = { .sessao = sessao, .tmdb = idTmdb(base),
                       .serie = !strcmp(base->tipo, "series") };
        idbase_copiar(base->imdb, q.id, sizeof q.id);
        snprintf(q.idioma, sizeof q.idioma, "%s", idioma);
        nIds = recomendacoes(&q, chave, agora, ids, &aguarda);
      }
      n = montar(base, ids, nIds, infantil, saida, &comTmdb);
      if (comTmdb) snprintf(titulo, sizeof titulo, "%s", base->titulo);
    }
  }
  if (!nv_perfil_sessao_atual(sessao) || cat != cat_revisao_itens()) { n = 0; titulo[0] = 0; }
  if (sessao != ultimaSessao || n != nItens || strcmp(titulo, tituloSemente) ||
      memcmp(itens, saida, (size_t)n * sizeof *itens)) {
    memcpy(itens, saida, (size_t)n * sizeof *itens);
    nItens = n;
    memcpy(tituloSemente, titulo, sizeof titulo);
    revisao++;
  }
  ultimaSessao = sessao;
  ultimoCat = cat;
  ultimoRemoto = remoto;
  ultimoLigado = ligado;
  ultimoInfantil = infantil;
  ultimaRede = rede;
  snprintf(ultimoIdioma, sizeof ultimoIdioma, "%s", idioma);
  return revisao;
}

int home_assistiu_itens(int saida[HOME_ASSISTIU_MAX]) {
  memcpy(saida, itens, (size_t)nItens * sizeof *saida);
  return nItens;
}

void home_assistiu_rotulo(char *saida, size_t tam) {
  if (tituloSemente[0])
    snprintf(saida, tam, i18n("Porque você assistiu %s"), tituloSemente);
  else
    snprintf(saida, tam, "%s", i18n("Parecidos no seu catálogo"));
}
