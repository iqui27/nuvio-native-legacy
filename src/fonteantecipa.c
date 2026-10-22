#include "fonteantecipa.h"
#include <pthread.h>
#include <string.h>

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static unsigned rodada;
static int pubIdx = -1, pubEstado = FA_NADA, falhouIdx = -1;
static unsigned pubGer;

unsigned fa_nova_rodada(void) {
  unsigned r;
  pthread_mutex_lock(&trava);
  r = ++rodada;
  pubIdx = -1; pubEstado = FA_NADA; falhouIdx = -1; pubGer = 0;
  pthread_mutex_unlock(&trava);
  return r;
}

unsigned fa_rodada_atual(void) {
  unsigned r;
  pthread_mutex_lock(&trava);
  r = rodada;
  pthread_mutex_unlock(&trava);
  return r;
}

void fa_publicar(unsigned r, int indice, unsigned geracao) {
  pthread_mutex_lock(&trava);
  if (r == rodada && indice >= 0) {
    pubIdx = indice; pubEstado = FA_CONFERINDO; pubGer = geracao; falhouIdx = -1;
  }
  pthread_mutex_unlock(&trava);
}

void fa_concluir(unsigned r, int indice, int servi) {
  pthread_mutex_lock(&trava);
  if (r == rodada && indice == pubIdx && pubEstado == FA_CONFERINDO)
    pubEstado = servi ? FA_OK : FA_RUIM;
  pthread_mutex_unlock(&trava);
}

int fa_ver(int *estado, unsigned *geracao) {
  int i;
  pthread_mutex_lock(&trava);
  i = pubIdx;
  if (estado) *estado = pubEstado;
  if (geracao) *geracao = pubGer;
  pthread_mutex_unlock(&trava);
  return i;
}

void fa_player_falhou(int indice) {
  pthread_mutex_lock(&trava);
  if (indice == pubIdx) falhouIdx = indice;
  pthread_mutex_unlock(&trava);
}

int fa_player_falhou_foi(unsigned r, int indice) {
  int v;
  pthread_mutex_lock(&trava);
  v = r == rodada && indice >= 0 && indice == falhouIdx;
  pthread_mutex_unlock(&trava);
  return v;
}

int fa_acao(int abertaIdx, int pubIdx, int estado, int playerFalhou) {
  if (abertaIdx < 0)
    return pubIdx >= 0 && estado == FA_CONFERINDO ? FA_ACAO_ABRIR : FA_ACAO_NADA;
  if (pubIdx != abertaIdx || estado == FA_RUIM) return FA_ACAO_DESFAZER_VEREDITO;
  if (playerFalhou) return FA_ACAO_DESFAZER_PLAYER;
  return FA_ACAO_NADA;
}

int fa_ja_tocando(int abertaIdx, int escolhida) {
  return abertaIdx >= 0 && escolhida == abertaIdx;
}

int fa_preparada_vale(const FaPreparada *p, const FaAgora *a) {
  if (!p || !a || !p->alvo || !a->alvo) return 0;
  return !strcmp(p->alvo, a->alvo) && p->lista == a->lista && p->lembrada == a->lembrada &&
         p->idx >= 0 && p->idx < a->n && a->idxDisponivel &&
         p->idadeMs <= FA_PREPARADA_VALE_MS && a->idadeListaMs <= FA_LINK_VALE_MS;
}
