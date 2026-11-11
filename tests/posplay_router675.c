#include <assert.h>
#include <stdio.h>
#include <string.h>
#define player_aberto teste_player_aberto
#define player_indice teste_player_indice
#define player_id_canal teste_player_id_canal
#define player_quer_sair teste_player_quer_sair
#define player_episodio_atual teste_player_episodio_atual
#define proximopreparo_consumir teste_preparo_consumir
#define proximopreparo_em_curso teste_preparo_em_curso
#define proximopreparo_geracao teste_preparo_geracao
#define proximopreparo_passo teste_preparo_passo
#define proximopreparo_cancelar teste_preparo_cancelar
#include "../src/app.c"
static int indice = 0;
static unsigned geracao = 1;
int teste_player_aberto(void) { return 1; }
int teste_player_indice(void) { return indice; }
const char *teste_player_id_canal(void) { return ""; }
int teste_player_quer_sair(void) { return 0; }
void teste_player_episodio_atual(int *t, int *e) { *t = 1; *e = 1; }
int teste_preparo_consumir(const char *alvo, ProximoPreparoResultado *r) { (void)alvo; (void)r; return 0; }
int teste_preparo_em_curso(const char *alvo) { (void)alvo; return 1; }
unsigned teste_preparo_geracao(void) { return geracao; }
void teste_preparo_passo(const char *atual) { (void)atual; }
void teste_preparo_cancelar(void) { geracao++; }
int main(void) {
  CatItem c = {0};
  snprintf(c.tipo, sizeof c.tipo, "series");
  snprintf(c.imdb, sizeof c.imdb, "tt0000675");
  cat_definir(&c, 1);
  posplay_fechar();
  abrirProximoEpisodio(1, 2, 1);
  assert(proximoIntento.tem);
  assert(app_proximo_pendente() || posplay_aguarda_escolha(100)); // manual Next is preparing
  geracao++;
  proximoPreparoPasso();
  assert(!proximoIntento.tem);
  assert(!app_proximo_pendente() && !posplay_aguarda_escolha(100)); // cancelled worker releases EOF
  abrirProximoEpisodio(1, 2, 1);
  assert(proximoIntento.tem);
  c.imdb[0] = 0;
  cat_definir(&c, 1);
  abrirProximoEpisodio(1, 2, 1); // lookup fails after intent was created
  assert(!proximoIntento.tem);
  assert(!app_proximo_pendente() && !posplay_aguarda_escolha(100));
  puts("PASS #675 router intent and failure");
  return 0;
}
