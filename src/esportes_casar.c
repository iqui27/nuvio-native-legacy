// Ver esportes_casar.h.
#include "esportes_casar.h"
#include "esportes_canal.h"
#include "esportes.h"
#include "guia.h"
#include "guiav5ui.h"
#include "epg.h"
#include "xtepg.h"
#include "perfis.h"
#include "dados.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int prefPerfil = -1, desligado;
static void prefLer(void) {
  if (prefPerfil == perfis_ativo()) return;
  char arq[64], *t;
  prefPerfil = perfis_ativo(); desligado = 0;
  snprintf(arq, sizeof arq, "esportes-casar-p%d.txt", prefPerfil);
  if ((t = dados_ler(arq))) { desligado = t[0] == '0'; free(t); }
}
int espcasar_ligado(void) { prefLer(); return !desligado; }
int espcasar_definir(int ligado) {
  char arq[64];
  prefLer();
  snprintf(arq, sizeof arq, "esportes-casar-p%d.txt", prefPerfil);
  if (!dados_gravar(arq, ligado ? "1\n" : "0\n")) return 0;
  desligado = !ligado;
  return 1;
}

static char      alvo[80];           // Lembrete.canal do jogo que se procura
static time_t    alvoIni;
static EspEvento ev;
static int       cursor, nota;
static unsigned  revGuia, revEpg;
static char      mId[128], mNome[200], mBase[600];

static int esporte(const Lembrete *l) {
  return !strncmp(l->canal, "sport:", 6) || !strncmp(l->base, "sports|", 7);
}
static int mesmo(const EspEvento *e, const char *mod, const char *id, size_t nId) {
  // A chave "sport:" guarda so 40 letras do id: ai basta o comeco bater.
  return e && !strcmp(e->modalidade, mod) && !strncmp(e->id, id, nId) && (nId >= 40 || !e->id[nId]);
}
// O evento completo (siglas, emissoras) se a agenda ainda o tem; senao, o que
// o titulo do lembrete permite refazer.
static void montarEvento(const Lembrete *l) {
  char mod[32] = "", liga[80] = "", id[100] = "";
  if (!strncmp(l->base, "sports|", 7)) sscanf(l->base + 7, "%31[^|]|%79[^|]|%99[^|]", mod, liga, id);
  else sscanf(l->canal + 6, "%31[^:]:%79[^:]:%99[^\n]", mod, liga, id);
  size_t n = strlen(id);
  if (n) {
    for (int i = 0; i < esportes_jogos_seguidos_n(); i++) if (mesmo(esportes_jogo_seguido(i), mod, id, n)) { ev = *esportes_jogo_seguido(i); return; }
    for (int i = 0; i < esportes_favoritos_n(); i++) if (mesmo(esportes_favoritos_evento(i), mod, id, n)) { ev = *esportes_favoritos_evento(i); return; }
    for (int i = 0; i < esportes_eventos_n(); i++) if (mesmo(esportes_evento(i), mod, id, n)) { ev = *esportes_evento(i); return; }
  }
  esportes_canal_evento_do_titulo(l->titulo, mod, l->ini, l->fim, &ev);
}

void espcasar_passo(time_t agora) {
  const Lembrete *l = NULL;
  if (!espcasar_ligado() || lembrete_perfil() != perfis_ativo()) { alvo[0] = 0; return; }
  for (int i = 0; i < lembrete_n(); i++) {
    const Lembrete *c = lembrete_item(i);
    if (c->avisado || !esporte(c) || agora < c->ini - ESPCASAR_ANTES_S || agora >= c->fim) continue;
    if (!l || c->ini < l->ini) l = c;
  }
  if (!l) { alvo[0] = 0; return; }
  if (strcmp(alvo, l->canal) || alvoIni != l->ini) {
    snprintf(alvo, sizeof alvo, "%s", l->canal); alvoIni = l->ini;
    montarEvento(l);
    // O horario do lembrete manda: e ele que a pessoa marcou.
    ev.inicio = l->ini; if (ev.fim <= ev.inicio) ev.fim = l->fim;
    if (ev.estado == 3) ev.estado = 0;
    cursor = nota = 0; mId[0] = 0;
    revGuia = guia_busca_revisao(); revEpg = epg_revisao();
    guia_carregar(); epg_iniciar();
    printf("[esporte-canal] procurando canal: %s\n", l->titulo); fflush(stdout);
  }
  epg_passo(); xtepg_passo();
  if (revGuia != guia_busca_revisao() || revEpg != epg_revisao()) {
    revGuia = guia_busca_revisao(); revEpg = epg_revisao(); cursor = nota = 0; mId[0] = 0;
  }
  Uint32 t0 = SDL_GetTicks();
  for (int lote = 0; lote < 8 && SDL_GetTicks() - t0 < 2; lote++) {
    const char *id, *nome, *grupo, *base;
    if (!guia_canal_regra_campos(cursor, NULL, NULL, NULL, NULL, NULL)) break;
    int i = cursor++;
    if (!guia_canal_campos(i, &id, &nome, NULL, &grupo, &base) || !guia_busca_elegivel(i) ||
        !g5ui_channel_allowed(id, nome, grupo, base)) continue;
    EpgProg ps[16]; int confirma = 0;
    time_t ate = ev.fim > ev.inicio ? ev.fim : ev.inicio + 6 * 3600;
    int n = guia_busca_programas(i, ev.inicio - 3 * 3600, ate, 0, ps, 16);
    for (int p = 0; p < n; p++) if (esportes_corresponde(&ev, ps[p].titulo, ps[p].ini, ps[p].fim)) confirma = 1;
    int v = esportes_canal_nota(&ev, nome, confirma);
    if (v > nota) {
      nota = v;
      snprintf(mId, sizeof mId, "%s", id); snprintf(mNome, sizeof mNome, "%s", nome);
      snprintf(mBase, sizeof mBase, "%s", base ? base : "");
    }
  }
}

int espcasar_canal(const Lembrete *l, char *id, size_t nId, char *nome, size_t nNome,
                   char *base, size_t nBase) {
  if (!l || !espcasar_ligado() || !alvo[0] || strcmp(alvo, l->canal) || alvoIni != l->ini) return 0;
  if (nota < ESP_CANAL_CERTO || !mId[0] || strlen(mId) >= nId) return 0;
  snprintf(id, nId, "%s", mId); snprintf(nome, nNome, "%s", mNome); snprintf(base, nBase, "%s", mBase);
  printf("[esporte-canal] %s -> %s (nota %d)\n", l->titulo, mNome, nota); fflush(stdout);
  return 1;
}
