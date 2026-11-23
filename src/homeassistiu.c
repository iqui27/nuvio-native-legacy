#include "homeassistiu.h"
#include "catalogo.h"
#include "descoberta.h"
#include "farolsinal.h"
#include "idbase.h"
#include "idioma.h"
#include "mapa.h"
#include "perfis.h"
#include "perfilinfantil.h"
#include "perfisessao.h"
#include "progresso.h"
#include "sessao.h"
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
  char usuario[80], id[64], idioma[12];
  long tmdb;
  int perfil, serie;
} Consulta;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static Consulta consulta;
static char chaveTmdb[96];
static int ocupada, pedida, recebeu;
static MapaRecomendacoes remotos;
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

int home_assistiu_sem_farol(int *recomendados, int n, const int *farol, int nFarol) {
  if (nFarol == 0) return n;
  int total = 0;
  for (int i = 0; i < n; i++) {
    const CatItem *recomendado = item(recomendados[i]);
    int repetido = 0;
    for (int j = 0; j < nFarol && !repetido; j++) {
      const CatItem *exibido = item(farol[j]);
      repetido = recomendados[i] == farol[j] ||
                 (recomendado && exibido && recomendado->imdb[0] && exibido->imdb[0] &&
                  !strcmp(recomendado->tipo, exibido->tipo) && mesmoTitulo(recomendado, exibido));
    }
    if (!repetido) recomendados[total++] = recomendados[i];
  }
  return total;
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

static int recIndice(const CatItem *c, const MapaRecomendacoes *rec) {
  long tmdb = idTmdb(c);
  int ano = mapa_ano_meta(c->meta);
  unsigned titulo = ano ? mapa_hash_titulo(c->titulo) : 0;
  for (int j = 0; j < rec->n; j++) {
    const MapaRecomendacao *r = &rec->obras[j];
    if (tmdb > 0) {
      if (tmdb == r->obra.tmdb) return j;
    } else if (r->obra.imdb[0] && idbase_e_imdb(c->imdb)) {
      size_t len = idbase_len(c->imdb);
      if (len == strlen(r->obra.imdb) && !strncmp(c->imdb, r->obra.imdb, len)) return j;
    } else if (titulo && ano == r->obra.ano &&
               (titulo == r->tituloHash || titulo == r->originalHash)) return j;
  }
  return -1;
}

#define PONTOS_TMDB ((MAPA_GEN_MAX + 1) * 128)

static int montar(const CatItem *base, const MapaRecomendacoes *rec, int infantil,
                  int saida[HOME_ASSISTIU_MAX], int *comTmdb, unsigned *noCatalogo) {
  MapaEtiqueta gen[MAPA_GEN_MAX];
  int nGen, notas[HOME_ASSISTIU_MAX], n = 0, total = cat_n();
  mapa_generos_locais(base->genero, gen, &nGen, MAPA_GEN_MAX);
  if (!nGen) {
    nGen = rec->nGen;
    memcpy(gen, rec->generos, (size_t)nGen * sizeof *gen);
    for (int g = 0; g < nGen; g++) gen[g].id = mapa_hash_nome(gen[g].nome);
  }
  for (int i = 0; i < total; i++) {
    const CatItem *c = item(i);
    if (!tipoValido(c) || strcmp(c->tipo, base->tipo)) continue;
    int remoto = rec->n ? recIndice(c, rec) : -1;
    if (remoto >= 0) *noCatalogo |= 1u << remoto;
    if (!c->titulo[0] || !c->poster[0] || c->progresso > 0 || mesmoTitulo(c, base) ||
        (infantil && !perfil_infantil_classif_ok(c->classificacao))) continue;
    int pontos = remoto < 0 ? 0 : (MAPA_REC_MAX - remoto + MAPA_GEN_MAX) * 128, j;
    if (!pontos && nGen) {
      MapaEtiqueta cg[MAPA_GEN_MAX];
      int nc;
      mapa_generos_locais(c->genero, cg, &nc, MAPA_GEN_MAX);
      for (int g = 0; g < nGen; g++)
        for (j = 0; j < nc; j++) if (gen[g].id == cg[j].id ||
            (rec->nGen && (mapa_genero_mascara(gen[g].nome) & mapa_genero_mascara(cg[j].nome)))) {
          pontos += 128;
          break;
        }
    }
    if (!pontos || cat_visto(c)) continue;
    pontos += c->nota < 0 ? 0 : c->nota > 100 ? 100 : c->nota;
    for (j = 0; j < n; j++)
      if (mesmoTitulo(c, item(saida[j])) ||
          (pontos >= PONTOS_TMDB && pontos / 128 == notas[j] / 128)) break;
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
  *comTmdb = n > 0 && notas[0] >= PONTOS_TMDB;
  return n;
}

static int acrescentarRemotos(const MapaRecomendacoes *rec, const CatItem *base,
                              unsigned noCatalogo, NvPerfilSessao sessao, unsigned *cat,
                              int saida[HOME_ASSISTIU_MAX]) {
  CatItem *lote = calloc((size_t)(rec->n < HOME_ASSISTIU_MAX ? rec->n : HOME_ASSISTIU_MAX),
                         sizeof *lote);
  ProgRegistro prog[HIST_MAX];
  int n = 0;
  if (!lote) return 0;
  int nProg = prog_ler(prog, HIST_MAX);
  for (int i = 0; i < rec->n && n < HOME_ASSISTIU_MAX; i++) {
    const MapaObra *o = &rec->obras[i].obra;
    CatItem *c = &lote[n];
    int j;
    if ((noCatalogo & (1u << i)) || !o->poster[0]) continue;
    c->tmdb = o->tmdb;
    if (idbase_e_imdb(o->imdb)) snprintf(c->imdb, sizeof c->imdb, "%s", o->imdb);
    else snprintf(c->imdb, sizeof c->imdb, "tmdb:%ld", o->tmdb);
    snprintf(c->tipo, sizeof c->tipo, "%s", o->tipo);
    if (mesmoTitulo(c, base) || cat_visto(c)) continue;
    for (j = 0; j < n; j++) if (mesmoTitulo(c, &lote[j])) break;
    if (j < n) continue;
    for (j = 0; j < nProg; j++)
      if (prog[j].posSeg > 0 && !strcmp(prog[j].contentId, c->imdb) &&
          !strcmp(prog[j].tipo, c->tipo)) break;
    if (j < nProg) continue;
    snprintf(c->titulo, sizeof c->titulo, "%s", o->titulo);
    snprintf(c->poster, sizeof c->poster, "%s", o->poster);
    snprintf(c->backdrop, sizeof c->backdrop, "%s", o->fundo);
    snprintf(c->sinopse, sizeof c->sinopse, "%s", o->sinopse);
    if (o->ano) snprintf(c->meta, sizeof c->meta, "%d", o->ano);
    c->nota = o->nota;
    n++;
  }
  int publicados = 0;
  if (n && nv_perfil_sessao_travar(sessao)) {
    if (*cat == cat_revisao_itens()) publicados = cat_acrescentar_lote(lote, n, saida);
    nv_perfil_sessao_destravar();
  }
  free(lote);
  if (publicados) ++*cat;
  return publicados;
}

static void *buscar(void *arg) {
  (void)arg;
  Consulta q = consulta;
  MapaRecomendacoes resposta;
  int ok = 0;
  if (nv_perfil_sessao_atual(q.sessao))
    ok = mapa_recomendacoes_tmdb(q.id, q.tmdb, q.serie, chaveTmdb, q.idioma, &resposta);
  pthread_mutex_lock(&trava);
  recebeu = ok && nv_perfil_sessao_atual(q.sessao);
  if (recebeu) remotos = resposta;
  memset(chaveTmdb, 0, sizeof chaveTmdb);
  ocupada = 0;
  __atomic_add_fetch(&remotoRev, 1u, __ATOMIC_RELEASE);
  pthread_mutex_unlock(&trava);
  return NULL;
}

static void recomendacoes(const Consulta *q, const char *chave, uint32_t agora,
                           MapaRecomendacoes *saida, uint32_t *aguarda) {
  pthread_t fio;
  pthread_mutex_lock(&trava);
  int mesmoContexto = pedida && consulta.perfil == q->perfil &&
                      !strcmp(consulta.usuario, q->usuario) &&
                      consulta.tmdb == q->tmdb && consulta.serie == q->serie &&
                      !strcmp(consulta.id, q->id) && !strcmp(consulta.idioma, q->idioma);
  int mesma = mesmoContexto && consulta.sessao == q->sessao;
  uint32_t intervalo = mesma ? backoff : REDE_INTERVALO_MS;
  if (recebeu && (mesma || (mesmoContexto && q->usuario[0]))) {
    *saida = remotos;
  } else if (ocupada) {
    // O fim do trabalho muda remotoRev.
  } else if (pedida && agora - ultimoPedido < intervalo) {
    *aguarda = intervalo;
  } else if (q->tmdb > 0 || idbase_e_imdb(q->id)) {
    consulta = *q;
    snprintf(chaveTmdb, sizeof chaveTmdb, "%s", chave);
    ocupada = pedida = 1;
    recebeu = 0;
    ultimoPedido = agora;
    backoff = !mesma ? REDE_INTERVALO_MS :
              backoff < REDE_BACKOFF_MAX_MS ? 2 * backoff : backoff;
    if (pthread_create(&fio, NULL, buscar, NULL) == 0) pthread_detach(fio);
    else { ocupada = 0; memset(chaveTmdb, 0, sizeof chaveTmdb); *aguarda = backoff; }
  }
  pthread_mutex_unlock(&trava);
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
      static MapaRecomendacoes rec;
      int comTmdb;
      unsigned noCatalogo = 0;
      rec.n = rec.nGen = 0;
      if (rede) {
        Consulta q = { .sessao = sessao, .perfil = perfis_ativo(), .tmdb = idTmdb(base),
                       .serie = !strcmp(base->tipo, "series") };
        snprintf(q.usuario, sizeof q.usuario, "%s", sessao_usuario());
        idbase_copiar(base->imdb, q.id, sizeof q.id);
        snprintf(q.idioma, sizeof q.idioma, "%s", idioma);
        recomendacoes(&q, chave, agora, &rec, &aguarda);
      }
      n = montar(base, &rec, infantil, saida, &comTmdb, &noCatalogo);
      if (!comTmdb && !infantil && rec.n && cat == cat_revisao_itens()) {
        int novos = acrescentarRemotos(&rec, base, noCatalogo, sessao, &cat, saida);
        if (novos) { n = novos; comTmdb = 1; }
      }
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
