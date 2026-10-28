#include "servidores.h"
#include "ajustes.h"
#include "idioma.h"
#include "jsraiz.h"
#include "fileiras.h"
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

// Watched flag the servers report for a catalogue title, index only. Any
// server saying watched counts; the person's own unmark still wins, through
// contapend inside cat_historico_estado_id (#617).
int servidores_visto(const char *id, const char *tipo) {
  SrvAchado a;
  if (!id || jfid_e(id)) return 0;
  return (jellyfin_tem(id, tipo, &a) && a.visto) ||
         (emby_tem(id, tipo, &a) && a.visto) ||
         (plex_tem(id, tipo, &a) && a.visto);
}

// Each server's list; a server that lacks the series adds nothing, one that
// fails does not block the others (#617).
int servidores_episodios(const char *id, SrvEp *out, int max) {
  SrvEp lote[2048];
  int n = 0, m, falhou = 0;
  if (!id || jfid_e(id) || !out || max <= 0 || !ajustes_jellyfin_ligado()) return 0;
  for (m = 0; m < 3; m++) {
    int k = m == 0 ? jellyfin_episodios(id, lote, (int)(sizeof lote / sizeof *lote))
          : m == 1 ? emby_episodios(id, lote, (int)(sizeof lote / sizeof *lote))
                   : plex_episodios(id, lote, (int)(sizeof lote / sizeof *lote));
    int i;
    if (k < 0) { falhou = 1; continue; }
    for (i = 0; i < k && n < max; i++) {
      int j;
      for (j = 0; j < n; j++)
        if (out[j].temporada == lote[i].temporada && out[j].episodio == lote[i].episodio) break;
      if (j == n) out[n++] = lote[i];
      else if (lote[i].vistoMs > out[j].vistoMs) out[j] = lote[i];
    }
  }
  return n ? n : (falhou ? -1 : 0);
}

// LAN before remote, then Jellyfin, Emby, Plex: the first that has the title
// names the badge on the title page.
int servidores_info(const char *id, const char *tipo, SrvAchado *info) {
  SrvAchado a[3];
  int tem[3], i, melhor = -1;
  if (!id || !*id) return 0;
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

int servidores_contem_item(const CatItem *it, int provider) {
  if (!it || !ajustes_jellyfin_ligado()) return 0;
  if (provider == 0) return plex_tem(it->imdb, it->tipo, NULL);
  if (provider == 1) return jellyfin_tem(it->imdb, it->tipo, NULL);
  if (provider == 2) return emby_tem(it->imdb, it->tipo, NULL);
  return servidores_tem(it->imdb, it->tipo);
}
int servidores_catalogo_copiar(CatItem *out, int max) {
  CatFileira fils[SRV_FIL_MAX]; int n = 0;
  if (!out || max <= 0 || !ajustes_jellyfin_ligado()) return 0;
  for (int p = 0; p < 3 && n < max; p++) {
    int ni = 0;
    if (p == 0) plex_fileiras_copiar(out + n, max - n, fils, SRV_FIL_MAX, &ni);
    else if (p == 1) jellyfin_fileiras_copiar(out + n, max - n, fils, SRV_FIL_MAX, &ni);
    else emby_fileiras_copiar(out + n, max - n, fils, SRV_FIL_MAX, &ni);
    n += ni;
  }
  return n;
}

unsigned servidores_fileiras_versao(void) {
  return jellyfin_fileiras_versao() * 3u + emby_fileiras_versao() * 7u + plex_fileiras_versao() * 13u;
}
int servidores_chave_fileira(const char *chave) {
  return jellyfin_chave_fileira(chave) || emby_chave_fileira(chave) || plex_chave_fileira(chave);
}

int servidores_bibliotecas(CatFileira *fils, int max) {
  int n = 0, ni;
  if (!ajustes_jellyfin_ligado()) return 0;
  n += jellyfin_fileiras_copiar(NULL, SRV_ITENS_MAX, fils, max, &ni);
  n += emby_fileiras_copiar(NULL, SRV_ITENS_MAX, fils + n, max - n, &ni);
  n += plex_fileiras_copiar(NULL, SRV_ITENS_MAX, fils + n, max - n, &ni);
  for (int m = 0; m < 3; m++) {
    int ligado = m == 0 ? jellyfin_conectado() : m == 1 ? emby_conectado() : plex_conectado();
    const char *prefixo = m == 0 ? "jellyfin_" : m == 1 ? "emby_" : "plex_";
    const char *nome = m == 0 ? "Jellyfin" : m == 1 ? "Emby" : "Plex";
    if (!ligado) continue;
    for (int k = 0; k < 2 && n < max; k++) {
      memset(&fils[n], 0, sizeof fils[n]);
      snprintf(fils[n].chave, sizeof fils[n].chave, "%s%s", prefixo, k ? "playlists" : "collections");
      snprintf(fils[n].titulo, sizeof fils[n].titulo, "%s · %s", i18n(k ? "Listas" : "Coleções"), nome);
      snprintf(fils[n].tipo, sizeof fils[n].tipo, "%s", k ? "list" : "group");
      n++;
    }
  }
  return n;
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
    // Home only receives catalogue identities (D9). Keep the module's full
    // snapshot, including private items, for the server view.
    { int r, w = 0, ff = 0;
      for (r = 0; r < f; r++) {
        CatFileira fila = fils[nf + r];
        int ini = w;
        if (fil_estado_chave(fila.chave) == FIL_FORA) continue;
        if (strstr(fila.chave, "_nextup"))
          snprintf(fila.titulo, sizeof fila.titulo, "%s · %s", i18n("Próximos episódios"), fils[nf + r].titulo);
        for (k = 0; k < fila.n; k++) {
          CatItem *it = &itens[ni + fila.ini + k];
          if (!it->imdb[0] || jfid_e(it->imdb)) continue;
          if (w != fila.ini + k) itens[ni + w] = *it;
          w++;
        }
        if (w == ini) continue;
        fila.ini = ni + ini;
        fila.n = w - ini;
        fils[nf + ff++] = fila;
      }
      nf += ff;
      ni += w;
    }
  }
  if (nItens) *nItens = ni;
  return nf;
}

int servidores_pagina(const char *chave, int inicio, int limite, CatItem *out, int max, int *total, int *raw) {
  if (raw) *raw = 0;
  if (total) *total = 0;
  if (!ajustes_jellyfin_ligado() || !out || max <= 0 || inicio < 0 || limite <= 0) return -1;
  if (limite > 100) limite = 100;
  char recent[192];
  const char *marker=chave?strstr(chave,"_recent_"):NULL;
  if(marker) {
    size_t p=(size_t)(marker-chave)+1;
    if(p+strlen(marker+8)>=sizeof recent)return -1;
    memcpy(recent,chave,p);snprintf(recent+p,sizeof recent-p,"%s",marker+8);chave=recent;
  }
  if (jellyfin_chave_fileira(chave)) return jellyfin_pagina(chave, inicio, limite, out, max, total, raw);
  if (emby_chave_fileira(chave)) return emby_pagina(chave, inicio, limite, out, max, total, raw);
  if (plex_chave_fileira(chave)) return plex_pagina(chave, inicio, limite, out, max, total, raw);
  return -1;
}

int servidores_continuar(CatItem *out, int max) {
  CatItem *lote;
  int n = 0;
  if (!ajustes_jellyfin_ligado() || max <= 0 || !servidores_conectado()) return 0;
  lote = calloc((size_t)max, sizeof *lote);
  if (!lote) return 0;
  for (int m = 0; m < 3; m++) {
    int k = m == 0 ? jellyfin_atividade("resume", lote, max)
          : m == 1 ? emby_atividade("resume", lote, max) : plex_atividade("resume", lote, max);
    for (int i = 0; i < k; i++) {
      int j;
      if (jfid_e(lote[i].imdb) || !lote[i].imdb[0] || lote[i].progresso <= 0) continue;
      for (j = 0; j < n; j++)
        if (!strcmp(out[j].imdb, lote[i].imdb) && !strcmp(out[j].tipo, lote[i].tipo)) break;
      if (j == n && n < max) out[n++] = lote[i];
      else if (j < n && lote[i].retomadoMs > out[j].retomadoMs) out[j] = lote[i];
    }
  }
  free(lote);
  return n;
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
