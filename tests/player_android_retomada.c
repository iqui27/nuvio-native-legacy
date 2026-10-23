// Fluxo real do player; so o pipeline e falso. Nao abre janela, rede ou TV.
#define NV_ANDROID 1
#include "player.h"
#include "catalogo.h"
#include "perfis.h"
#include "progresso.h"
#include "linguas.h"
#include "dados.h"
#include "ajustes.h"
#include <stdlib.h>
#include <assert.h>
int video_quadro_visto(void) { return video_pronto(); }
int extplayer_tentar(const char *url, const char *title, int anime, void (*internal)(const char *), void (*exit_player)(void)) {
  (void)url; (void)title; (void)anime; (void)internal; (void)exit_player; return 0;
}
#include <math.h>
#include <stdio.h>
#include <string.h>

static int cargas, buscas, ativo, pronto, tocando, ack, pausaAck;
static double posicao, duracao = 1800, inicio, inicioPct;
static char fonte[4096];
// 2.0.3 (TCL, Grand Tour): retomada vinda da conta (Trakt) so tem percentual.
// O pipeline recebe o percentual NA ABERTURA e aplica com a duracao do
// container, antes do primeiro quadro — nunca um seek depois do "pronto".
int video_tocar_retomada(const char *url, double pos, double pct) {
  cargas++; inicio = pos; inicioPct = pct; posicao = pos; ativo = tocando = 1; pronto = 0; ack = 0;
  snprintf(fonte, sizeof fonte, "%s", url); return 1;
}
int video_tocar_posicao(const char *url, double pos) { return video_tocar_retomada(url, pos, 0); }
int video_tocar(const char *url) { return video_tocar_posicao(url, 0); }
int video_retomada_inicial_estado(void) { return ack; }
void video_parar(void) { ativo = pronto = tocando = 0; fonte[0] = 0; }
void video_pausar(int p) { tocando = !p; pausaAck = p; }
void video_buscar(double pos) { buscas++; posicao = pos; }
int video_pausa_confirmada(void) { return pausaAck && ativo && pronto && !tocando; }
int video_tocando(void) { return tocando; }
int video_pronto(void) { return pronto; }
int video_superficie_estavel(void) { return 1; }
int video_ativo(void) { return ativo; }
int video_falhou(void) { return 0; }
int video_conflito_recurso(void) { return 0; }
double video_pos(void) { return posicao; }
double video_duracao(void) { return duracao; }
const char *video_url_atual(void) { return fonte; }

static void quadro(void) { player_atualizar(.016f, SDL_GetTicks()); }
static void abrir(const char *id, const char *tipo, int pct, int t, int e) {
  CatItem c = {0};
  // Cada saida grava progresso e o fio do "Continuar assistindo" refaz a
  // fileira, trocando o item do indice 0 (ver o comentario la embaixo). Com
  // mais registros salvos a troca demora mais: deixa assentar sempre.
  SDL_Delay(300);
  snprintf(c.imdb, sizeof c.imdb, "%s", id);
  snprintf(c.titulo, sizeof c.titulo, "Fixture");
  snprintf(c.tipo, sizeof c.tipo, "%s", tipo);
  c.progresso = pct; c.temporada = t; c.episodio = e;
  // De proposito: meta anuncia duracao diferente da gravada. Nao pode ser
  // usada para preparar uma posicao que a pessoa nunca salvou.
  snprintf(c.meta, sizeof c.meta, "2026 · 240 min");
  cat_definir(&c, 1);
  player_abrir(0, NULL); player_definir_episodio(t, e);
}
static void fonteAbrir(void) { player_definir_fonte("https://example.invalid/fixture.mp4"); }
int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar("");
  perfis_definir_ativo(1); ling_local_legenda("none");
  assert(prog_gravar_local("fixture-filme", 0, 0, 612.345, 1800, -1));
  abrir("fixture-filme", "movie", 34, 0, 0); fonteAbrir();
  assert(cargas == 1 && fabs(inicio - 612.345) < .0001 && inicioPct == 0 && buscas == 0);
  pronto = 1; quadro(); assert(buscas == 0); // ack ainda em voo
  ack = 1; quadro(); quadro(); assert(buscas == 0);
  puts("ok posicao salva chega no prepare, sem segundo seek ou uso de meta");

  // Reter e opt-in (05/10, "Manter o video pronto ao sair"): de fabrica a
  // sessao fica pausada SO ate o voo da saida pousar (o quadro parado e o
  // fundo do dissolve) e o pipeline e solto logo depois — nada de 2 min.
  player_preparar_retencao(); assert(player_suspender());
  assert(player_retido() && player_retido_so_voo() && ativo);
  player_validar_retido(SDL_GetTicks() + 100); assert(player_retido() && ativo);
  player_validar_retido(SDL_GetTicks() + 5000);
  assert(!player_retido() && !ativo && !fonte[0]);
  puts("ok sem o ajuste: retida so durante o voo, solta depois do pouso");
  abrir("fixture-filme", "movie", 34, 0, 0); fonteAbrir();
  pronto = 1; ack = 1; quadro(); quadro();
  { char cam[600]; const char *d = getenv("NUVIO_DADOS"); FILE *f;
    assert(d && *d);
    snprintf(cam, sizeof cam, "%s/ajustes.txt", d);
    f = fopen(cam, "w"); assert(f); fputs("manterVideoLocal 0\n", f); fclose(f);
    ajustes_dir(d); }
  player_preparar_retencao(); assert(player_suspender());
  assert(!player_retido_so_voo());
  player_validar_retido(SDL_GetTicks() + 5000); assert(player_retido());
  int n = cargas;
  assert(player_retomar_retido("fixture-filme", 0, 0)); quadro();
  assert(cargas == n && buscas == 0);
  player_descartar_retido(); player_encerrar();
  puts("ok sessao retida nao recarrega nem busca");

  abrir("fixture-filme", "movie", 34, 0, 0); fonteAbrir();
  pronto = 1; ack = -1; quadro(); quadro();
  assert(buscas == 1 && fabs(posicao - 612) < .01); player_encerrar();
  puts("ok preparacao recusada recua a um seek normal");

  n = buscas;
  // O fio de "Continuar assistindo" (catalogo.c) refaz a fileira sozinho depois
  // de cada gravacao de progresso e troca o item do indice 0 pelo do registro
  // salvo (fixture-filme, 34%): se a troca cair entre abrir() e fonteAbrir() a
  // fixture sem registro herda a posicao dele. Deixa o fio assentar antes.
  SDL_Delay(300);
  abrir("fixture-percentual", "movie", 34, 0, 0); fonteAbrir();
  assert(inicio == 0 && fabs(inicioPct - 34) < .0001);
  pronto = 1; quadro(); assert(buscas == n);       // ack em voo: nada de seek
  // O Media3 aplicou o percentual sobre a duracao do container.
  ack = 1; posicao = duracao * inicioPct / 100.0;
  quadro(); quadro(); assert(buscas == n); player_encerrar();
  puts("ok percentual sozinho vai na abertura, sem seek depois do pronto");

  SDL_Delay(300);   // o mesmo fio do "Continuar assistindo" (ver acima)
  abrir("fixture-conta", "movie", 34, 0, 0); fonteAbrir();
  assert(fabs(inicioPct - 34) < .0001);
  pronto = 1; ack = -1; quadro(); quadro();
  assert(buscas == n + 1 && fabs(posicao - 612) < .01); player_encerrar();
  puts("ok percentual recusado na abertura recua ao seek com a duracao real");

  abrir("fixture-filme", "movie", 34, 0, 0); player_do_inicio();
  player_definir_episodio(0, 0); fonteAbrir(); assert(inicio == 0 && inicioPct == 0);
  n = buscas; pronto = 1; quadro(); assert(buscas == n); player_encerrar();
  puts("ok assistir do inicio persiste na definicao tardia do episodio");

  abrir("fixture-filme", "channel", 34, 0, 0); fonteAbrir();
  assert(inicio == 0 && inicioPct == 0); n = buscas; pronto = 1; quadro();
  assert(buscas == n); player_encerrar(); puts("ok ao vivo nao busca");

  assert(prog_gravar_local("fixture-serie", 2, 4, 612.345, 1800, -1));
  abrir("fixture-serie", "series", 34, 2, 4);
  player_definir_episodio(2, 5); fonteAbrir(); assert(inicio == 0 && inicioPct == 0);
  n = buscas; pronto = 1; quadro(); assert(buscas == n); player_encerrar();
  // Sair do E5 salva um registro novo dele. Recoloca o E4 como o episodio
  // atual antes de verificar sua retomada (mesmo contrato do catalogo real).
  assert(prog_gravar_local("fixture-serie", 2, 4, 612.345, 1800, -1));
  abrir("fixture-serie", "series", 34, 2, 4); fonteAbrir();
  assert(fabs(inicio - 612.345) < .0001); pronto = 1; ack = 1; quadro(); player_encerrar();
  puts("ok episodio salvo nao vaza para o seguinte");

  // Troca de fonte do MESMO titulo usa o instante atual, nao o antigo ponto
  // salvo. Mesmo "Assistir do comeco" deve preservar o que ja foi assistido.
  abrir("fixture-filme", "movie", 34, 0, 0); player_do_inicio(); fonteAbrir();
  assert(inicio == 0 && inicioPct == 0);
  pronto = 1; ack = 1; posicao = 471.625; quadro();
  player_definir_retomada_fonte(posicao); // antes de video_parar zerar o backend
  player_voltar_a_esperar(); posicao = 0;
  player_definir_episodio(0, 0); // metadado tardio nao perde o instante da troca
  n = buscas;
  player_definir_fonte("https://example.invalid/next-source.mp4");
  assert(fabs(inicio - 471.625) < .0001 && inicioPct == 0);
  pronto = 1; quadro(); assert(buscas == n); // aguarda ack do prepare
  ack = 1; quadro(); quadro(); assert(buscas == n);
  // A abertura seguinte recusou o prepare: um seek absoluto, sem percentual
  // arredondado e sem substituir o instante por zero da fonte que falhou.
  player_voltar_a_esperar(); posicao = 0;
  player_definir_retomada_fonte(NAN);
  player_definir_retomada_fonte(INFINITY);
  player_definir_retomada_fonte(-1);
  player_definir_fonte("https://example.invalid/third-source.mp4");
  assert(fabs(inicio - 471.625) < .0001 && inicioPct == 0);
  pronto = 1; ack = -1; quadro(); quadro();
  assert(buscas == n + 1 && fabs(posicao - 471.625) < .0001);
  player_encerrar();
  puts("ok failover preserva segundos atuais antes de parar e recua uma vez se prepare recusar");

  abrir("fixture-filme", "movie", 34, 0, 0); fonteAbrir();
  pronto = 1; ack = 1; quadro();
  posicao = 0; // a pessoa voltou ao inicio, nao um zero de backend em erro
  player_definir_retomada_fonte(posicao);
  player_voltar_a_esperar(); player_definir_episodio(0, 0);
  n = buscas;
  player_definir_fonte("https://example.invalid/seek-back-zero.mp4");
  assert(inicio == 0 && inicioPct == 0); // o antigo percentual nao reaparece
  pronto = 1; ack = -1; quadro(); quadro();
  assert(buscas == n + 1 && posicao == 0);
  player_encerrar();
  puts("ok failover preserva inicio valido sem reintroduzir percentual antigo");

  abrir("fixture-filme", "channel", 34, 0, 0);
  player_definir_retomada_fonte(471.625); fonteAbrir();
  assert(inicio == 0 && inicioPct == 0); player_encerrar();
  abrir("fixture-serie", "series", 34, 2, 4);
  player_definir_retomada_fonte(471.625);
  player_definir_episodio(2, 5); fonteAbrir();
  assert(inicio == 0 && inicioPct == 0); player_encerrar();
  puts("ok instante de troca nao vaza para canal nem episodio novo");

  perfis_definir_ativo(2);
  abrir("fixture-filme", "movie", 34, 0, 0); fonteAbrir(); assert(inicio == 0);
  player_encerrar(); SDL_Quit();
  puts("player Android retomada: PASS (prepare, fallback, retido, live, episodio e perfil)");
}
