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

// Jogos na mesma hora: cada lembrete guarda o proprio canal, porque o cartao
// de um sai um segundo depois do outro e nao da tempo de varrer a lista de novo.
#define ESPCASAR_ALVOS 4
typedef struct {
  char      alvo[80];                // Lembrete.canal do jogo que se procura
  time_t    ini;
  EspEvento ev;
  // `v` = a varredura em curso; `f` = a ultima que chegou ao fim. Enquanto uma
  // nova nao termina, o cartao ainda pode usar o canal da anterior.
  struct { int nota; char id[128], nome[200], base[600]; } v, f;
} Alvo;
static Alvo      alvos[ESPCASAR_ALVOS];
static int       nAlvos, cursor;
static unsigned  revGuia, revEpg, revEsp;
static Uint32    fimVarredura;
static int       revistas;

static int esporte(const Lembrete *l) {
  return !strncmp(l->canal, "sport:", 6) || !strncmp(l->base, "sports|", 7);
}
static int mesmo(const EspEvento *e, const char *mod, const char *id, size_t nId) {
  // A chave "sport:" guarda so 40 letras do id: ai basta o comeco bater.
  return e && !strcmp(e->modalidade, mod) && !strncmp(e->id, id, nId) && (nId >= 40 || !e->id[nId]);
}
// O evento completo (siglas, emissoras) se a agenda ainda o tem; senao, o que
// o titulo do lembrete permite refazer.
static void montarEvento(const Lembrete *l, Alvo *a) {
#define ev a->ev
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
  // Lembrete.nome e o nome da liga: sem ele a grade de um jogo feminino nao casa.
  snprintf(ev.liga, sizeof ev.liga, "%s", l->nome);
#undef ev
}
static void preparar(const Lembrete *l, Alvo *a) {
  montarEvento(l, a);
  // O horario do lembrete manda: e ele que a pessoa marcou.
  a->ev.inicio = l->ini; if (a->ev.fim <= a->ev.inicio) a->ev.fim = l->fim;
  if (a->ev.estado == 3) a->ev.estado = 0;
}

void espcasar_passo(time_t agora) {
  const Lembrete *ls[ESPCASAR_ALVOS]; int n = 0, novo = 0;
  if (!espcasar_ligado() || lembrete_perfil() != perfis_ativo()) { nAlvos = 0; return; }
  for (int i = 0; i < lembrete_n(); i++) {
    const Lembrete *c = lembrete_item(i); int at;
    if (c->avisado || !esporte(c) || agora < c->ini - ESPCASAR_ANTES_S || agora >= c->fim) continue;
    for (at = 0; at < n && ls[at]->ini <= c->ini; at++) {}
    if (at >= ESPCASAR_ALVOS) continue;
    if (n < ESPCASAR_ALVOS) n++;
    memmove(ls + at + 1, ls + at, (size_t)(n - at - 1) * sizeof *ls);
    ls[at] = c;
  }
  if (!n) { nAlvos = 0; return; }
  // Quem ja estava sendo procurado leva junto o que achou.
  { static Alvo antes[ESPCASAR_ALVOS]; int nAntes = nAlvos;
    memcpy(antes, alvos, sizeof antes);
    for (int k = 0; k < n; k++) {
      int j;
      for (j = 0; j < nAntes; j++) if (!strcmp(antes[j].alvo, ls[k]->canal) && antes[j].ini == ls[k]->ini) break;
      if (j < nAntes) { alvos[k] = antes[j]; continue; }
      memset(&alvos[k], 0, sizeof alvos[k]);
      snprintf(alvos[k].alvo, sizeof alvos[k].alvo, "%s", ls[k]->canal); alvos[k].ini = ls[k]->ini;
      preparar(ls[k], &alvos[k]);
      novo = 1;
      printf("[esporte-canal] procurando canal: %s\n", ls[k]->titulo); fflush(stdout);
    }
    nAlvos = n; }
  if (novo) {
    cursor = revistas = 0;
    revGuia = guia_busca_revisao(); revEpg = epg_revisao(); revEsp = esportes_revisao();
    guia_carregar(); epg_iniciar();
  }
  epg_passo(); xtepg_passo();
  // A agenda mudou (emissora nova, por exemplo): refaz os eventos.
  if (revEsp != esportes_revisao()) {
    revEsp = esportes_revisao();
    for (int k = 0; k < nAlvos; k++) { preparar(ls[k], &alvos[k]); alvos[k].v.nota = 0; alvos[k].v.id[0] = 0; }
    cursor = 0;
  }
  if (revGuia != guia_busca_revisao() || revEpg != epg_revisao()) {
    revGuia = guia_busca_revisao(); revEpg = epg_revisao(); cursor = 0;
    for (int k = 0; k < nAlvos; k++) { alvos[k].v.nota = 0; alvos[k].v.id[0] = 0; }
  }
  Uint32 t0 = SDL_GetTicks();
  for (int lote = 0; lote < 8 && SDL_GetTicks() - t0 < 2; lote++) {
    const char *id, *nome, *grupo, *base;
    if (!guia_canal_regra_campos(cursor, NULL, NULL, NULL, NULL, NULL)) {
      // Fim da lista. A grade curta do Xtream chega sem mudar revisao nenhuma:
      // olha de novo umas poucas vezes, guardando o melhor ja achado. Poucas
      // porque cada passada pede a grade curta de quem saiu do cache (192).
      if (!fimVarredura) {
        fimVarredura = t0 ? t0 : 1;
        for (int k = 0; k < nAlvos; k++) alvos[k].f = alvos[k].v;
      } else if (cursor && revistas < ESPCASAR_REVER_MAX && t0 - fimVarredura >= ESPCASAR_REVER_MS) {
        cursor = 0; fimVarredura = 0; revistas++;
      }
      break;
    }
    fimVarredura = 0;
    int i = cursor++;
    if (!guia_canal_campos(i, &id, &nome, NULL, &grupo, &base) || !guia_busca_elegivel(i) ||
        !g5ui_channel_allowed(id, nome, grupo, base)) continue;
    for (int k = 0; k < nAlvos; k++) {
      Alvo *a = &alvos[k];
      EpgProg ps[16]; int confirma = 0;
      time_t ate = a->ev.fim > a->ev.inicio ? a->ev.fim : a->ev.inicio + 6 * 3600;
      int np = guia_busca_programas(i, a->ev.inicio - 3 * 3600, ate, 0, ps, 16);
      for (int p = 0; p < np; p++) if (esportes_corresponde(&a->ev, ps[p].titulo, ps[p].ini, ps[p].fim)) confirma = 1;
      int v = esportes_canal_nota(&a->ev, nome, confirma);
      if (v > a->v.nota) {
        a->v.nota = v;
        snprintf(a->v.id, sizeof a->v.id, "%s", id); snprintf(a->v.nome, sizeof a->v.nome, "%s", nome);
        snprintf(a->v.base, sizeof a->v.base, "%s", base ? base : "");
      }
    }
  }
}

int espcasar_canal(const Lembrete *l, char *id, size_t nId, char *nome, size_t nNome,
                   char *base, size_t nBase) {
  const Alvo *a = NULL;
  if (!l || !espcasar_ligado()) return 0;
  for (int k = 0; k < nAlvos; k++) if (!strcmp(alvos[k].alvo, l->canal) && alvos[k].ini == l->ini) a = &alvos[k];
  if (!a) return 0;
  int f = a->f.nota > a->v.nota;
  int nota = f ? a->f.nota : a->v.nota;
  const char *aId = f ? a->f.id : a->v.id, *aNome = f ? a->f.nome : a->v.nome, *aBase = f ? a->f.base : a->v.base;
  if (nota < ESP_CANAL_CERTO || !aId[0] || strlen(aId) >= nId) return 0;
  snprintf(id, nId, "%s", aId); snprintf(nome, nNome, "%s", aNome); snprintf(base, nBase, "%s", aBase);
  printf("[esporte-canal] %s -> %s (nota %d)\n", l->titulo, aNome, nota); fflush(stdout);
  return 1;
}
