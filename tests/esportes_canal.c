// Esporte casado com o canal: a nota de cada canal e o evento refeito do
// titulo do lembrete. Regra pura (src/esportes_canal.c).
#include "esportes_canal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static EspEvento jogo(const char *emissora) {
  EspEvento e; memset(&e, 0, sizeof e);
  snprintf(e.modalidade, sizeof e.modalidade, "basketball");
  snprintf(e.participantes[0].nome, 120, "Los Angeles Lakers");
  snprintf(e.participantes[1].nome, 120, "Boston Celtics");
  e.nParticipantes = 2; e.inicio = 1000000; e.fim = e.inicio + 3 * 3600;
  if (emissora) { snprintf(e.emissoras[0], 80, "%s", emissora); e.nEmissoras = 1; }
  return e;
}

int main(void) {
  EspEvento e = jogo("ESPN");
  // Nem grade nem emissora: nao e candidato.
  assert(!esportes_canal_nota(&e, "Globo HD", 0));
  assert(!esportes_canal_nota(&e, "", 1) && !esportes_canal_nota(NULL, "ESPN", 1));
  // A emissora exata, com os enfeites de lista IPTV, e canal certo.
  assert(esportes_canal_nota(&e, "ESPN", 0) >= ESP_CANAL_CERTO);
  assert(esportes_canal_nota(&e, "ESPN HD", 0) >= ESP_CANAL_CERTO);
  assert(esportes_canal_nota(&e, "BR: ESPN FHD [H265]", 0) >= ESP_CANAL_CERTO);
  assert(esportes_canal_nota(&e, "US | espn SD", 0) >= ESP_CANAL_CERTO);
  // Canal irmao entra na lista, mas nunca e oferecido sozinho.
  int irmao = esportes_canal_nota(&e, "ESPN 2 HD", 0);
  assert(irmao > 0 && irmao < ESP_CANAL_CERTO);
  assert(esportes_canal_nota(&e, "ESPN Brasil", 0) < ESP_CANAL_CERTO);
  assert(esportes_canal_nota(&e, "ESPN+", 0) < ESP_CANAL_CERTO);
  // Nome parecido que nao contem a emissora como palavra: fora.
  assert(!esportes_canal_nota(&e, "ESPNEWS", 0));
  // A grade confirma: ganha de qualquer nome, e com a emissora ganha mais.
  int grade = esportes_canal_nota(&e, "Canal 123", 1);
  assert(grade >= ESP_CANAL_CERTO && grade > esportes_canal_nota(&e, "ESPN 4K", 0));
  assert(esportes_canal_nota(&e, "ESPN", 1) > grade);
  assert(esportes_canal_nota(&e, "ESPN 2", 1) > grade);
  // Qualidade so desempata: FHD > HD > sem marca > SD.
  assert(esportes_canal_nota(&e, "ESPN FHD", 0) > esportes_canal_nota(&e, "ESPN HD", 0));
  assert(esportes_canal_nota(&e, "ESPN HD", 0) > esportes_canal_nota(&e, "ESPN", 0));
  assert(esportes_canal_nota(&e, "ESPN", 0) > esportes_canal_nota(&e, "ESPN SD", 0));
  assert(esportes_canal_nota(&e, "ESPN SD", 0) > irmao + 2);
  // ESPN2 e ESPN+ sao emissoras proprias.
  e = jogo("ESPN2");
  assert(esportes_canal_nota(&e, "ESPN 2 HD", 0) >= ESP_CANAL_CERTO);
  assert(!esportes_canal_nota(&e, "ESPN HD", 0));
  e = jogo("ESPN+");
  assert(esportes_canal_nota(&e, "ESPN Plus", 0) >= ESP_CANAL_CERTO);
  assert(!esportes_canal_nota(&e, "ESPN", 0));
  // Sem emissora na agenda so a grade decide.
  e = jogo(NULL);
  assert(!esportes_canal_nota(&e, "ESPN", 0) && esportes_canal_nota(&e, "ESPN", 1) >= ESP_CANAL_CERTO);
  // Um canal chamado so "BR" nao some como prefixo de pais.
  e = jogo("BR");
  assert(!esportes_canal_nota(&e, "BR", 0));   // emissora curta demais: ambigua

  // O titulo do lembrete refaz os dois lados, e a grade confere com eles.
  EspEvento t;
  assert(esportes_canal_evento_do_titulo("Los Angeles Lakers at Boston Celtics", "basketball", 1000000, 1010800, &t));
  assert(t.nParticipantes == 2 && !strcmp(t.participantes[0].nome, "Los Angeles Lakers") && !strcmp(t.participantes[1].nome, "Boston Celtics"));
  assert(esportes_corresponde(&t, "NBA: Boston Celtics x Los Angeles Lakers", 1000000 - 600, 1010800));
  assert(!esportes_corresponde(&t, "NBA: Boston Celtics x Miami Heat", 1000000 - 600, 1010800));
  assert(!esportes_corresponde(&t, "Boston Celtics x Los Angeles Lakers", 1000000 - 86400, 1000000 - 80000));
  assert(esportes_canal_evento_do_titulo("Flamengo x Vasco", "soccer", 1, 2, &t) && !strcmp(t.participantes[1].nome, "Vasco"));
  assert(esportes_canal_evento_do_titulo("Arsenal VS. Chelsea", "soccer", 1, 2, &t) && !strcmp(t.participantes[0].nome, "Arsenal") && !strcmp(t.participantes[1].nome, "Chelsea"));
  // Um nome so (corrida, luta): confere pelo titulo inteiro.
  assert(!esportes_canal_evento_do_titulo("Monaco Grand Prix", "racing", 1000000, 1010800, &t));
  assert(t.nParticipantes == 0 && !strcmp(t.nome, "Monaco Grand Prix"));
  assert(esportes_corresponde(&t, "F1: Monaco Grand Prix", 1000000, 1010800));
  // Separador na ponta nao inventa adversario.
  assert(!esportes_canal_evento_do_titulo(" at Home", "soccer", 1, 2, &t));
  assert(!esportes_canal_evento_do_titulo(NULL, NULL, 1, 2, &t) && !esportes_canal_evento_do_titulo("A x B", "", 1, 2, NULL));
  puts("esportes_canal ok");
  return 0;
}
