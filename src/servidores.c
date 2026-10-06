#include "servidores.h"
#include "ajustes.h"
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
int servidores_tem(const char *id, const char *tipo) { return plex_tem(id, tipo); }
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
    unsigned v = servidores_fileiras_versao();
    if (v != versao) { versao = v; refazer = 1; }
  }
  return refazer;
}
