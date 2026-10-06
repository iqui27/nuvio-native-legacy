#include "servidores.h"
#include "ajustes.h"
#include "jsraiz.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void servidores_carregar(void) {
  jellyfin_carregar();
  emby_carregar();
  plex_carregar();
}
void servidores_perfil_trocou(void) {
  jellyfin_perfil_trocou();
  emby_perfil_trocou();
  plex_perfil_trocou();
}
void servidores_esquecer_todos(void) {
  jellyfin_esquecer_todos();
  emby_esquecer_todos();
  plex_esquecer_todos();
}
int servidores_tem(const char *id, const char *tipo) {
  return servidores_info(id, tipo, NULL);
}

// LAN before remote, then Jellyfin, Emby, Plex: the first that has the title
// names the badge on the title page.
int servidores_info(const char *id, const char *tipo, SrvAchado *info) {
  SrvAchado a[3];
  int tem[3], i, melhor = -1;
  if (!id || !*id || jfid_e(id)) return 0;
  tem[0] = jellyfin_tem(id, tipo, &a[0]);
  tem[1] = emby_tem(id, tipo, &a[1]);
  tem[2] = plex_tem(id, tipo, &a[2]);
  for (i = 0; i < 3; i++)
    if (tem[i] && (melhor < 0 || (a[i].local && !a[melhor].local))) melhor = i;
  if (melhor >= 0 && info) *info = a[melhor];
  return melhor >= 0;
}

int servidores_casamento_ativo(void) {
  return jellyfin_indice_n() > 0 || emby_indice_n() > 0 || plex_indice_n() > 0;
}

// TITLE HINTS: the page that asked for sources says the title and year, for
// the last match rule (title + year). A few recent ones, by base id.
#define SRV_DICAS 8
static struct { char id[64]; char titulo[160]; int ano; } dicas[SRV_DICAS];
static int proxDica;
static pthread_mutex_t travaDicas = PTHREAD_MUTEX_INITIALIZER;
static void idBase(const char *id, char *dst, size_t tam) {
  const char *p = !strncmp(id, "tmdb:", 5) ? strchr(id + 5, ':') : strchr(id, ':');
  size_t n = p ? (size_t)(p - id) : strlen(id);
  if (n >= tam) n = tam - 1;
  memcpy(dst, id, n);
  dst[n] = 0;
}
void servidores_dica(const char *id, const char *titulo, int ano) {
  char b[64];
  int i;
  if (!id || !*id || !titulo || !*titulo || jfid_e(id)) return;
  idBase(id, b, sizeof b);
  pthread_mutex_lock(&travaDicas);
  for (i = 0; i < SRV_DICAS && strcmp(dicas[i].id, b); i++) {}
  if (i == SRV_DICAS) { i = proxDica; proxDica = (proxDica + 1) % SRV_DICAS; }
  snprintf(dicas[i].id, sizeof dicas[i].id, "%s", b);
  snprintf(dicas[i].titulo, sizeof dicas[i].titulo, "%s", titulo);
  dicas[i].ano = ano;
  pthread_mutex_unlock(&travaDicas);
}
static int dicaDe(const char *id, char *titulo, size_t tam) {
  char b[64];
  int i, ano = 0;
  titulo[0] = 0;
  if (!id) return 0;
  idBase(id, b, sizeof b);
  pthread_mutex_lock(&travaDicas);
  for (i = 0; i < SRV_DICAS; i++)
    if (dicas[i].id[0] && !strcmp(dicas[i].id, b)) { snprintf(titulo, tam, "%s", dicas[i].titulo); ano = dicas[i].ano; break; }
  pthread_mutex_unlock(&travaDicas);
  return ano;
}

int servidores_consultar(const char *id, const char *tipo, int (*cancelado)(void *), void *ctx,
                         Stream **saida) {
  Stream *l[3] = { NULL, NULL, NULL }, *tudo;
  int n[3], i, total = 0, ano;
  char titulo[160];
  *saida = NULL;
  if (!id || jfid_e(id)) return 0;
  ano = dicaDe(id, titulo, sizeof titulo);
  n[0] = jellyfin_consultar(id, tipo, titulo, ano, cancelado, ctx, &l[0]);
  n[1] = emby_consultar(id, tipo, titulo, ano, cancelado, ctx, &l[1]);
  n[2] = plex_consultar(id, tipo, titulo, ano, cancelado, ctx, &l[2]);
  if (n[0] < 0 || n[1] < 0 || n[2] < 0) { for (i = 0; i < 3; i++) free(l[i]); return -1; }
  for (i = 0; i < 3; i++) total += n[i];
  tudo = total > 0 ? malloc(sizeof(Stream) * (size_t)total) : NULL;
  total = 0;
  for (i = 0; i < 3; i++) {
    if (tudo && n[i] > 0) { memcpy(tudo + total, l[i], sizeof(Stream) * (size_t)n[i]); total += n[i]; }
    free(l[i]);
  }
  *saida = tudo;
  return tudo ? total : 0;
}

int servidores_eh_fonte(const Stream *s) {
  return s && (!strncmp(s->bingeGroup, "jellyfin|", 9) || !strncmp(s->bingeGroup, "emby|", 5) ||
               !strncmp(s->bingeGroup, "plex|", 5));
}

void servidores_atualizar_periodico(void) {
  static unsigned long long ultimo;
  unsigned long long agora = agoraMs();
  if (!ultimo) { ultimo = agora; return; }
  if (agora - ultimo < SRV_REFRESCO_MS) return;
  ultimo = agora;
  // Rows and the matching index (incremental): what changed on the server
  // in the last half hour reaches the Home and the source lists.
  if (jellyfin_conectado()) jellyfin_recarregar_bibliotecas();
  if (emby_conectado()) emby_recarregar_bibliotecas();
  if (plex_conectado()) plex_recarregar_bibliotecas();
}
int servidores_conectado(void) { return jellyfin_conectado() || emby_conectado() || plex_conectado(); }

unsigned servidores_fileiras_versao(void) {
  return jellyfin_fileiras_versao() * 3u + emby_fileiras_versao() * 7u + plex_fileiras_versao() * 13u;
}
int servidores_chave_fileira(const char *chave) {
  return jellyfin_chave_fileira(chave) || emby_chave_fileira(chave) || plex_chave_fileira(chave);
}

// Appends each module's rows after the previous ones; item windows are rebased.
int servidores_fileiras_copiar(CatItem *itens, int maxItens, CatFileira *fils, int maxFils, int *nItens) {
  int nf = 0, ni = 0, m;
  if (nItens) *nItens = 0;
  for (m = 0; m < 3; m++) {
    int n = 0, k, f;
    if (maxItens - ni <= 0 || maxFils - nf <= 0) break;
    f = m == 0 ? jellyfin_fileiras_copiar(itens + ni, maxItens - ni, fils + nf, maxFils - nf, &n)
      : m == 1 ? emby_fileiras_copiar(itens + ni, maxItens - ni, fils + nf, maxFils - nf, &n)
               : plex_fileiras_copiar(itens + ni, maxItens - ni, fils + nf, maxFils - nf, &n);
    for (k = 0; k < f; k++) fils[nf + k].ini += ni;
    nf += f;
    ni += n;
  }
  if (nItens) *nItens = ni;
  return nf;
}

int servidores_ficha(CatItem *item, CatEp *eps, int maxEps) {
  if (!item) return -1;
  if (!strncmp(item->imdb, JFID_PREFIXO_PLEX, 3)) return plex_ficha(item, eps, maxEps);
  if (!strncmp(item->imdb, JFID_PREFIXO_EMBY, 3)) return emby_ficha(item, eps, maxEps);
  return jellyfin_ficha(item, eps, maxEps);
}

int servidores_fontes_pedir(const char *alvo) {
  if (!alvo) return 0;
  if (!strncmp(alvo, JFID_PREFIXO_PLEX, 3)) return plex_fontes_pedir(alvo);
  if (!strncmp(alvo, JFID_PREFIXO_EMBY, 3)) return emby_fontes_pedir(alvo);
  return jellyfin_fontes_pedir(alvo);
}
int servidores_fontes_colher(const char *alvo, Stream **lista, int *n) {
  if (!alvo) { if (lista) *lista = NULL; if (n) *n = 0; return JF_FONTES_FALHOU; }
  // PX_FONTES_* and JF_FONTES_* are the same four values on purpose.
  if (!strncmp(alvo, JFID_PREFIXO_PLEX, 3)) return plex_fontes_colher(alvo, lista, n);
  if (!strncmp(alvo, JFID_PREFIXO_EMBY, 3)) return emby_fontes_colher(alvo, lista, n);
  return jellyfin_fontes_colher(alvo, lista, n);
}

void servidores_reproducao_tick(const char *url, double posSeg, double durSeg, int tocando) {
  jellyfin_reproducao_tick(url, posSeg, durSeg, tocando);
  emby_reproducao_tick(url, posSeg, durSeg, tocando);
  plex_reproducao_tick(url, posSeg, durSeg, tocando);
}
void servidores_reproducao_fim(const char *url, double posSeg, double durSeg) {
  jellyfin_reproducao_fim(url, posSeg, durSeg);
  emby_reproducao_fim(url, posSeg, durSeg);
  plex_reproducao_fim(url, posSeg, durSeg);
}

int servidores_passo(void) {
  static int ligado = -1;
  static unsigned versao;
  int lig = ajustes_jellyfin_ligado(), refazer = 0;
  if (lig != ligado) {
    if (lig) servidores_carregar();
    refazer = ligado >= 0;
    ligado = lig;
  }
  if (lig) {
    unsigned v;
    servidores_atualizar_periodico();
    v = servidores_fileiras_versao();
    if (v != versao) { versao = v; refazer = 1; }
  }
  return refazer;
}
