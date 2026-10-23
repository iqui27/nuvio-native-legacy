// O backend falso conta os custos que o retorno rapido deve eliminar. Nenhuma
// janela, conta ou fonte real: o estado de pausa e confirmado separadamente.
#include "player.h"
#include "catalogo.h"
#include "perfis.h"
#include "streams.h"
#include "fontevolta.h"
#include "ajustes.h"
#include "descoberta.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int cargas, paradas, buscas, pausas, ativo, pronto, tocando, confirmado, falha, conflito;
static double posicao = 600, duracao = 3600;
static char fonte[4096], conta[96] = "fixture-conta-a";
const char *nv_test_usuario(void) { return conta; }
int video_tocar(const char *url) {
  cargas++; ativo = pronto = tocando = 1; confirmado = falha = conflito = 0;
  snprintf(fonte, sizeof fonte, "%s", url); return 1;
}
void video_parar(void) { paradas++; ativo = pronto = tocando = confirmado = 0; fonte[0] = 0; }
void video_pausar(int p) { pausas++; tocando = !p; confirmado = 0; }
void video_buscar(double p) { buscas++; posicao = p; }
int video_pausa_confirmada(void) { return confirmado && ativo && pronto && !tocando; }
int video_tocando(void) { return tocando; }
int video_pronto(void) { return pronto; }
int video_ativo(void) { return ativo; }
int video_falhou(void) { return falha; }
int video_conflito_recurso(void) { return conflito; }
double video_pos(void) { return posicao; }
double video_duracao(void) { return duracao; }
const char *video_url_atual(void) { return fonte; }

// A refacao de Continuar assistindo pode substituir o fixture antes de abrir.
// Espere a publicacao terminar antes de definir o catalogo do teste.
static void esperar_catalogo(void) {
  Uint32 ini = SDL_GetTicks();
  while (desc_montando() && SDL_GetTicks() - ini < 5000u) SDL_Delay(1);
  assert(!desc_montando());
}
static void abrir(const char *tipo) {
  CatItem c = {0};
  esperar_catalogo();
  snprintf(c.imdb, sizeof c.imdb, "fixture-titulo");
  snprintf(c.titulo, sizeof c.titulo, "Titulo ficticio");
  snprintf(c.tipo, sizeof c.tipo, "%s", tipo);
  cat_definir(&c, 1);
  posicao = 600; duracao = 3600;
  player_abrir(0, "https://example.invalid/fixture.mp4");
  player_atualizar(.016f, SDL_GetTicks());
}
static void guardar(void) {
  player_preparar_retencao();
  confirmado = 1; // o evento do backend, depois da intencao
  assert(player_suspender());
  assert(player_retido() && !player_aberto() && !tocando);
}

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  puts("player_retido: inicio");
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  perfis_definir_ativo(1);

  // De fabrica (sem ajustes.txt, instalacao antiga ou nova) nao retem.
  assert(!ajustes_manter_video());
  // O ajuste ligado (ajustes.txt desta TV) volta ao comportamento retido.
  { char cam[600]; const char *d = getenv("NUVIO_DADOS"); FILE *f;
    assert(d && *d);
    snprintf(cam, sizeof cam, "%s/ajustes.txt", d);
    f = fopen(cam, "w"); assert(f); fputs("manterVideoLocal 0\n", f); fclose(f);
    ajustes_dir(d); }
  assert(ajustes_manter_video());
  abrir("movie");
  player_preparar_retencao();
  assert(!player_suspender()); // tocando local=0 nao e confirmacao
  assert(!player_retido());
  confirmado = 1;
  assert(player_suspender());
  assert(cargas == 1 && paradas == 0 && buscas == 0 && !tocando);
  assert(player_retomar_retido("fixture-titulo", 0, 0));
  assert(player_aberto() && !player_retido() && tocando);
  assert(cargas == 1 && paradas == 0 && buscas == 0);
  player_atualizar(.016f, SDL_GetTicks());
  assert(buscas == 0); // nao reaplica progresso salvo na suspensao
  puts("ok retorno: 1 load total, 0 stop, 0 seek, pausa confirmada");

  guardar();
  int p = paradas;
  player_validar_retido(SDL_GetTicks() + 119000u);
  assert(player_retido() && paradas == p);
  player_validar_retido(SDL_GetTicks() + 120000u);
  assert(!player_retido() && paradas == p + 1);
  puts("ok teto de 120 s libera pipeline");

  // Both options ON: keep-alive wins; once released, the saved source survives.
  assert(ajustes_retomada_morna());
  { char cam[600]; const char *d = getenv("NUVIO_DADOS");
    snprintf(cam, sizeof cam, "%s/ajustes.txt", d);
    FILE *f = fopen(cam, "w"); assert(f);
    fputs("manterVideoLocal 0\nmanterVideoPrazoLocal 4\nretomadaMornaLocal 0\n", f); fclose(f); ajustes_dir(d);
  }
  assert(ajustes_manter_video_ms() == 0 && ajustes_manter_video() && ajustes_retomada_morna());
  abrir("movie"); guardar(); p = paradas;
  player_validar_retido(SDL_GetTicks() + 86400000u);
  assert(player_retido() && paradas == p); // unlimited has no timer expiry
  player_ciclo_evento(SDL_APP_LOWMEMORY); assert(!player_retido() && paradas == p + 1);
  abrir("movie"); guardar(); p = paradas;
  player_ciclo_evento(SDL_APP_WILLENTERBACKGROUND); assert(!player_retido() && paradas == p + 1);
  puts("ok sem limite nao vence; descarte por ciclo/memoria libera");

  abrir("movie"); guardar(); p = paradas;
  perfis_definir_ativo(2); player_validar_retido(SDL_GetTicks());
  assert(!player_retido() && paradas == p + 1); perfis_definir_ativo(1);
  abrir("movie"); guardar(); p = paradas;
  snprintf(conta, sizeof conta, "fixture-conta-b"); player_validar_retido(SDL_GetTicks());
  assert(!player_retido() && paradas == p + 1);
  puts("ok troca de conta e perfil descarta");

  abrir("movie"); guardar(); p = paradas;
  assert(!player_retomar_retido("outro-titulo", 0, 0));
  assert(paradas == p + 1 && !player_retido());
  abrir("movie"); guardar(); p = paradas;
  assert(!player_retomar_retido("fixture-titulo", 1, 2));
  assert(paradas == p + 1 && !player_retido());
  puts("ok identidade e episodio diferentes usam fluxo normal");

  abrir("movie"); guardar(); p = paradas; falha = 1;
  player_validar_retido(SDL_GetTicks()); assert(paradas == p + 1 && !player_retido());
  abrir("movie"); guardar(); p = paradas; conflito = 1;
  player_validar_retido(SDL_GetTicks()); assert(paradas == p + 1 && !player_retido());
  abrir("movie"); guardar(); p = paradas;
  snprintf(fonte, sizeof fonte, "https://example.invalid/substituto.mp4");
  player_validar_retido(SDL_GetTicks()); assert(paradas == p && !player_retido());
  puts("ok erro/recurso perdido e backend substituido nao retomam");

  abrir("movie"); guardar(); p = paradas;
  player_abrir(0, "https://example.invalid/novo.mp4");
  assert(paradas == p + 1 && !player_retido()); player_encerrar();
  abrir("channel"); player_preparar_retencao(); confirmado = 1;
  assert(!player_suspender()); player_encerrar();
  abrir("movie"); duracao = 30; player_atualizar(.016f, SDL_GetTicks());
  player_preparar_retencao(); confirmado = 1;
  assert(!player_suspender()); player_encerrar();
  puts("ok novo player, canal e clipe curto seguem fechamento normal");

  // FONTE PARA O RETOMAR (fontevolta.h): a sessao que tocou guarda a fonte da
  // LISTA quando a url bate com a do video; falha e fim apagam.
  { Stream st; memset(&st, 0, sizeof st);
    snprintf(st.url, sizeof st.url, "https://example.invalid/fixture.mp4");
    snprintf(st.cabecalhos, sizeof st.cabecalhos, "Referer: https://example.invalid/");
    st.mp4 = 1;
    snprintf(conta, sizeof conta, "fixture-conta-a"); perfis_definir_ativo(1);
    fontevolta_esquecer("teste");
    stream_definir_lista(&st, 1); stream_definir_atual(0);
    abrir("movie"); stream_definir_atual(0); guardar();
    Stream got;
    assert(fontevolta_pegar("fixture-titulo", "fixture-conta-a", 1, SDL_GetTicks(), &got));
    assert(!strcmp(got.url, st.url) && !strcmp(got.cabecalhos, st.cabecalhos) && got.mp4);
    player_descartar_retido();   // soltar a retida nao apaga nem regrava
    assert(fontevolta_pegar("fixture-titulo", "fixture-conta-a", 1, SDL_GetTicks(), &got));
    puts("ok suspensao guarda a fonte com cabecalhos; soltar a retida mantem");

    abrir("movie"); stream_definir_atual(0); falha = 1; player_encerrar(); falha = 0;
    assert(!fontevolta_pegar("fixture-titulo", "fixture-conta-a", 1, SDL_GetTicks(), &got));
    abrir("movie"); stream_definir_atual(0); player_encerrar();
    assert(fontevolta_pegar("fixture-titulo", "fixture-conta-a", 1, SDL_GetTicks(), &got));
    abrir("movie"); stream_definir_atual(0); posicao = 3590; player_atualizar(.016f, SDL_GetTicks());
    player_encerrar();
    assert(!fontevolta_pegar("fixture-titulo", "fixture-conta-a", 1, SDL_GetTicks(), &got));
    puts("ok sessao que falhou ou terminou apaga; fechamento normal guarda");

    // Lista trocada por baixo (busca de fundo): a entrada que toca nao some.
    abrir("movie"); stream_definir_atual(0); player_encerrar();
    stream_definir_lista(NULL, 0);
    abrir("movie"); player_encerrar();
    assert(fontevolta_pegar("fixture-titulo", "fixture-conta-a", 1, SDL_GetTicks(), &got));
    abrir("channel"); stream_definir_lista(&st, 1); stream_definir_atual(0); player_encerrar();
    assert(fontevolta_pegar("fixture-titulo", "fixture-conta-a", 1, SDL_GetTicks(), &got));
    puts("ok lista trocada e canal nao mexem na entrada"); }
  // DESLIGADO (o padrao de fabrica, 05/10): sem "Manter o video pronto ao sair" nada e retido. A saida
  // solta o pipeline na hora e a fonte fica guardada para o Retomar.
  { char cam[600]; const char *d = getenv("NUVIO_DADOS"); FILE *f;
    snprintf(cam, sizeof cam, "%s/ajustes.txt", d);
    f = fopen(cam, "w"); assert(f); fputs("manterVideoLocal 1\n", f); fclose(f);
    ajustes_dir(d); }
  assert(!ajustes_manter_video());
  { Stream st0; Stream got0; memset(&st0, 0, sizeof st0);
    snprintf(st0.url, sizeof st0.url, "https://example.invalid/fixture.mp4");
    stream_definir_lista(&st0, 1); stream_definir_atual(0);
    abrir("movie"); stream_definir_atual(0);
    int p0 = paradas, pa0 = pausas;
    player_preparar_retencao();
    assert(pausas == pa0);           // nem pausa para reter
    confirmado = 1;
    assert(!player_suspender() && !player_retido());
    player_encerrar();
    assert(paradas == p0 + 1 && !player_retido() && !player_aberto());
    assert(fontevolta_pegar("fixture-titulo", "fixture-conta-a", 1, SDL_GetTicks(), &got0));
    assert(!strcmp(got0.url, st0.url));
    assert(!player_retomar_retido("fixture-titulo", 0, 0));
    fontevolta_esquecer("teste"); stream_definir_lista(NULL, 0); }
  puts("ok padrao: sair libera o pipeline e guarda a fonte para o Retomar");

  SDL_Quit(); puts("player_retido: tudo ok");
}
