// Lembrete de jogo casado com o canal: o caminho inteiro de esportes_casar.c
// (lembrete real em disco + regra real) sobre um guia de mentira.
#include "esportes_casar.h"
#include "esportes_canal.h"
#include "esportes.h"
#include "epg.h"
#include "dados.h"
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

// --- guia de mentira ---------------------------------------------------------
typedef struct { const char *id, *nome, *programa; int proibido; } Canal;
static Canal canais[8]; static int nCanais, carregou, perfil = 1;
static unsigned revGuia = 1, revEpg = 1, revEsp = 1;
unsigned esportes_revisao(void) { return revEsp; }
static time_t T0;
static EspEvento agenda[2]; static int nAgenda;
int perfis_ativo(void) { return perfil; }
static Uint32 relogio = 1000;
Uint32 SDL_GetTicks(void) { return relogio; }
void guia_carregar(void) { carregou++; }
void epg_iniciar(void) {}
void epg_passo(void) {}
void xtepg_passo(void) {}
unsigned guia_busca_revisao(void) { return revGuia; }
unsigned epg_revisao(void) { return revEpg; }
int guia_canal_regra_campos(int i, const char **id, const char **nome, const char **logo, const char **cat, const char **base) {
  (void)id; (void)nome; (void)logo; (void)cat; (void)base; return i >= 0 && i < nCanais;
}
int guia_canal_campos(int i, const char **id, const char **nome, const char **logo, const char **cat, const char **base) {
  if (i < 0 || i >= nCanais) return 0;
  if (id) *id = canais[i].id;
  if (nome) *nome = canais[i].nome;
  if (logo) *logo = "";
  if (cat) *cat = "Esportes";
  if (base) *base = "addon://lista";
  return 1;
}
int guia_busca_elegivel(int i) { return i >= 0 && i < nCanais; }
int g5ui_channel_allowed(const char *id, const char *name, const char *group, const char *source) {
  (void)name; (void)group; (void)source;
  for (int i = 0; i < nCanais; i++) if (!strcmp(canais[i].id, id)) return !canais[i].proibido;
  return 0;
}
int guia_busca_programas(int i, time_t de, time_t ate, int pular, EpgProg *out, int cap) {
  (void)de; (void)ate;
  if (pular || cap < 1 || !canais[i].programa) return 0;
  out[0] = (EpgProg){ T0 - 600, T0 + 3 * 3600, canais[i].programa, "" };
  return 1;
}
int esportes_jogos_seguidos_n(void) { return 0; }
const EspEvento *esportes_jogo_seguido(int i) { (void)i; return NULL; }
int esportes_favoritos_n(void) { return 0; }
const EspEvento *esportes_favoritos_evento(int i) { (void)i; return NULL; }
int esportes_eventos_n(void) { return nAgenda; }
const EspEvento *esportes_evento(int i) { return &agenda[i]; }

static void varrer(time_t agora) { for (int i = 0; i < 20; i++) espcasar_passo(agora); }
static int achou(const Lembrete *l, const char *esperado) {
  char id[80] = "", nome[120] = "", base[600] = "";
  int r = espcasar_canal(l, id, sizeof id, nome, sizeof nome, base, sizeof base);
  if (r) assert(!strcmp(base, "addon://lista"));
  return esperado ? r && !strcmp(id, esperado) : !r;
}

static const Lembrete *lem(const char *canal) {
  for (int i = 0; i < lembrete_n(); i++) if (!strcmp(lembrete_item(i)->canal, canal)) return lembrete_item(i);
  return NULL;
}

int main(void) {
  T0 = time(NULL) + 200;
  assert(getenv("NUVIO_DADOS")); dados_iniciar(getenv("NUVIO_DADOS"));
  lembrete_carregar(perfil);
  assert(espcasar_ligado());   // ligado de fabrica

  // 1. Lembrete da ficha do jogo (so titulo): a grade decide.
  canais[nCanais++] = (Canal){ "c-globo", "Globo HD", "Jornal", 0 };
  canais[nCanais++] = (Canal){ "c-adulto", "Canal Bloqueado", "NBA: Los Angeles Lakers x Boston Celtics", 1 };
  canais[nCanais++] = (Canal){ "c-espn2", "ESPN 2 HD", "NBA: Los Angeles Lakers x Boston Celtics", 0 };
  canais[nCanais++] = (Canal){ "c-espn", "ESPN HD", "SportsCenter", 0 };
  assert(lembrete_alternar("sport:basketball:nba:401", "NBA", "Boston Celtics at Los Angeles Lakers", "", T0, T0 + 7200) == 1);
  const Lembrete *l = lembrete_item(0);
  // Longe do inicio nada se procura (nem o guia e carregado).
  varrer(T0 - ESPCASAR_ANTES_S - 60);
  assert(!carregou && achou(l, NULL));
  varrer(T0 - 120);
  assert(carregou == 1 && achou(l, "c-espn2"));   // o canal proibido nunca entra
  // Desligado no ajuste: o lembrete volta a ser so aviso. Religa e acha de novo.
  assert(espcasar_definir(0) && !espcasar_ligado());
  varrer(T0 - 120); assert(achou(l, NULL));
  assert(espcasar_definir(1) && espcasar_ligado());
  varrer(T0 - 120); assert(achou(l, "c-espn2"));
  // A grade mudou e o jogo saiu dela: sem canal certo, sem cartao.
  canais[2].programa = "Poker"; revEpg++;
  varrer(T0 - 100); assert(achou(l, NULL));

  // 2. Jogo que a agenda conhece: a emissora informada basta, sem grade.
  agenda[0] = (EspEvento){ .inicio = T0, .fim = T0 + 7200, .nParticipantes = 2, .nEmissoras = 1 };
  snprintf(agenda[0].id, sizeof agenda[0].id, "401"); snprintf(agenda[0].modalidade, 32, "basketball");
  snprintf(agenda[0].participantes[0].nome, 120, "Boston Celtics");
  snprintf(agenda[0].participantes[1].nome, 120, "Los Angeles Lakers");
  snprintf(agenda[0].emissoras[0], 80, "ESPN"); nAgenda = 1;
  assert(lembrete_alternar("sport:basketball:nba:401", "NBA", "Boston Celtics at Los Angeles Lakers", "", T0, T0 + 7200) == 0);
  assert(lembrete_alternar("sports-00aa", "NBA", "Boston Celtics at Los Angeles Lakers", "sports|basketball|nba|401|2|13", T0, T0 + 7200) == 1);
  l = lembrete_item(0);
  varrer(T0 - 90); assert(achou(l, "c-espn"));     // ESPN HD, nao o irmao ESPN 2
  canais[3].nome = "ESPN 4"; revGuia++;
  varrer(T0 - 80); assert(achou(l, NULL));         // so irmaos: nao arrisca
  // Lembrete de programa comum nunca passa por aqui.
  assert(lembrete_alternar("c-globo", "Globo HD", "Jornal", "", T0, T0 + 1800) == 1);
  for (int i = 0; i < lembrete_n(); i++) if (!strcmp(lembrete_item(i)->canal, "c-globo")) assert(achou(lembrete_item(i), NULL));
  // Ja avisado: para de procurar.
  for (int i = 0; i < lembrete_n(); i++) if (!strncmp(lembrete_item(i)->canal, "sports-", 7)) lembrete_marcar_avisado(i);
  canais[3].nome = "ESPN"; revGuia++;
  varrer(T0 - 70);
  for (int i = 0; i < lembrete_n(); i++) assert(achou(lembrete_item(i), NULL));

  // 3. Dois jogos na mesma hora: cada um com o seu canal, e o segundo nao
  // perde o dele quando o cartao do primeiro sai.
  nAgenda = 0;
  canais[0].programa = "Women's Super League: Arsenal x Chelsea";
  canais[2].programa = "NBA: Chicago Bulls x Miami Heat";
  assert(lembrete_alternar("sport:soccer:eng.w.1:900", "Women's Super League", "Arsenal vs Chelsea", "", T0, T0 + 7200) == 1);
  assert(lembrete_alternar("sport:basketball:nba:402", "NBA", "Miami Heat at Chicago Bulls", "", T0, T0 + 7200) == 1);
  varrer(T0 - 60);
  // Grade de jogo feminino so casa porque a liga do lembrete foi junto.
  assert(achou(lem("sport:soccer:eng.w.1:900"), "c-globo"));
  assert(achou(lem("sport:basketball:nba:402"), "c-espn2"));
  for (int i = 0; i < lembrete_n(); i++) if (!strcmp(lembrete_item(i)->canal, "sport:soccer:eng.w.1:900")) lembrete_marcar_avisado(i);
  espcasar_passo(T0 - 59);
  assert(achou(lem("sport:basketball:nba:402"), "c-espn2"));
  // A agenda passa a conhecer o jogo e a emissora: o evento e refeito.
  canais[2].programa = "Poker"; revEpg++;
  varrer(T0 - 50); assert(achou(lem("sport:basketball:nba:402"), NULL));
  snprintf(agenda[0].id, sizeof agenda[0].id, "402");
  snprintf(agenda[0].participantes[0].nome, 120, "Miami Heat");
  snprintf(agenda[0].participantes[1].nome, 120, "Chicago Bulls");
  nAgenda = 1; revEsp++;
  varrer(T0 - 40); assert(achou(lem("sport:basketball:nba:402"), "c-espn"));
  // Grade curta do Xtream chega sem mudar revisao: a varredura se repete.
  canais[2].programa = "NBA: Chicago Bulls x Miami Heat";
  varrer(T0 - 30); assert(achou(lem("sport:basketball:nba:402"), "c-espn"));
  relogio += ESPCASAR_REVER_MS;
  varrer(T0 - 20); assert(achou(lem("sport:basketball:nba:402"), "c-espn2"));
  puts("esportes_casar ok");
  return 0;
}
