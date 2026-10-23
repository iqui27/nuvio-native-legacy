// O CARTAO DE PROXIMO EPISODIO, do OK ate o pedido chegar ao roteador.
//
// Issue #14: "o aviso de proximo episodio aparece, eu aperto OK, e o proximo
// nao comeca". O relato veio com video, e no video o cartao NEM SUME — fica na
// tela depois do OK. Este teste cobre o caminho inteiro que o app.c consome:
// posplay_evento -> posplay_pediu_episodio.
#include "posplay.h"
#include "nvtmp.h"
#include "catalogo.h"
#include "player.h"
#include "ajustes.h"
#include "vistoep.h"
#include "intro.h"
#include "descoberta.h"
#include "extras.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "../src/posplay.c"

static Uint32 dedoAgora556 = 50000;
static Uint32 relogioDedo556(void) { return dedoAgora556; }
static void entregarDedo556(const SDL_Event *e) { (void)posplay_evento(e); }

static int tecla(int k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  return posplay_evento(&e);
}

// DURACAO ESTAVEL (#115): o painel so decide depois de a duracao ficar parada
// por alguns segundos. Os casos abaixo fingem esse tempo com dez quadros de 1 s
// no comeco do titulo, onde nada pode subir.
static void aquecer(double dur, int ehSerie) {
  int i;
  for (i = 0; i < 10; i++) posplay_atualizar(1.0f, 500, 0.0, dur, ehSerie, 0, 0);
}

// A DECISAO DO FILME, pela linha de log (ver a nota longa no main: sem
// relacionados, posplay_visivel() diria "nao" em qualquer caso). Roda um
// quadro com a saida desviada e diz se "[posplay] relacionados em" saiu.
static int quadroFilmeDecide(float dt, double pos, double dur) {
  char linha[256];
  FILE *f;
  int decidiu = 0, salvo, arq;
  fflush(stdout);
  salvo = dup(fileno(stdout));
  arq = open(nvt("/nuvio-posplay-log.txt"), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (arq >= 0) {
    dup2(arq, fileno(stdout));
    close(arq);
    posplay_atualizar(dt, 1000, pos, dur, 0, 0, 0);
    fflush(stdout);
    dup2(salvo, fileno(stdout));
  }
  close(salvo);
  f = fopen(nvt("/nuvio-posplay-log.txt"), "r");
  if (f) {
    while (fgets(linha, sizeof linha, f))
      if (strstr(linha, "[posplay] relacionados em")) decidiu = 1;
    fclose(f);
  }
  remove(nvt("/nuvio-posplay-log.txt"));
  return decidiu;
}

// Deixa o painel no ar com a serie em T5E3, cujo proximo e T5E4.
static void abrirPainel(void) {
  posplay_fechar();
  aquecer(3600.0, 1);
  posplay_atualizar(0.016f, 1000, 3000.0, 3600.0, 1, 0, 1);
  assert(posplay_visivel());
}

int main(void) {
  CatItem c = { 0 };
  CatEp eps[10] = { 0 };
  int i, t = 0, e = 0;

  snprintf(c.tipo, sizeof c.tipo, "series");
  snprintf(c.titulo, sizeof c.titulo, "Game of Thrones");
  snprintf(c.imdb, sizeof c.imdb, "tt0944947");
  c.temporada = 5; c.episodio = 3;      // o que esta tocando
  cat_definir(&c, 1);
  for (i = 0; i < 10; i++) {
    eps[i].temporada = 5; eps[i].episodio = i + 1;
    snprintf(eps[i].nome, sizeof eps[i].nome, "Episodio %d", i + 1);
  }
  cat_definir_episodios(0, eps, 10);
  assert(cat_n_episodios(0) == 10);

  // 1. OK ARMA O PEDIDO. E o issue #14: posplay_evento gravava pedT/pedE e
  //    chamava posplay_fechar em seguida, que zerava os dois.
  abrirPainel();
  assert(tecla(SDLK_RETURN) == 1);
  assert(!posplay_visivel());
  assert(posplay_pediu_episodio(&t, &e));
  assert(t == 5 && e == 4);
  assert(!posplay_pediu_episodio(&t, &e));   // consumido uma vez so
  puts("ok  OK arma o proximo episodio");

  // 2. E O CARTAO NAO VOLTA. Sem marcar dispensado, a condicao de aparecer
  //    continua verdadeira e o painel reabre no quadro seguinte — que e
  //    exatamente o que o video do relato mostra.
  posplay_atualizar(0.016f, 1016, 3016.0, 3600.0, 1, 0, 1);
  assert(!posplay_visivel());
  puts("ok  o cartao nao reabre depois do OK");

  // 3. VOLTAR fica no episodio: dispensa sem pedir nada.
  posplay_fechar();
  abrirPainel();
  assert(tecla(SDLK_AC_BACK) == 1);
  assert(!posplay_visivel() && !posplay_pediu_episodio(&t, &e));
  posplay_atualizar(0.016f, 1016, 3016.0, 3600.0, 1, 0, 1);
  assert(!posplay_visivel());
  puts("ok  Voltar dispensa sem pedir episodio");

  // 4. BAIXO devolve os controles (2) e tambem nao pede nada.
  posplay_fechar();
  abrirPainel();
  assert(tecla(SDLK_DOWN) == 2);
  assert(!posplay_visivel() && !posplay_pediu_episodio(&t, &e));
  puts("ok  Baixo devolve o player sem pedir episodio");

  // Owner #256: factory default waits for OK, even at the end.
  assert(!ajustes_proximo_auto() && ajustes_proximo_contagem() == 5);
  posplay_fechar();
  aquecer(3600.0, 1);
  posplay_atualizar(0.016f, 1000, 3597.0, 3600.0, 1, 0, 1);
  posplay_atualizar(0.016f, 10000, 3600.0, 3600.0, 1, 0, 1);
  assert(posplay_visivel() && !posplay_pediu_episodio(&t, &e));
  ajustes_shot_valor("proximoAutoLocal", 0); // Enable for countdown cases below.

  // 4b. 2.0.3, SAMSUNG: A LEGENDA SEGUE O CARTAO. Ela subia por ofertaProximo()
  //     (a JANELA, verdadeira ate o fim do episodio) e ficava no meio da tela
  //     depois de o cartao sumir. Agora sobe so com o cartao NA TELA e volta no
  //     quadro em que ele some: Voltar, Baixo, OK, contagem.
  { const float normal = 1000.0f;
    int tec[3] = { SDLK_AC_BACK, SDLK_DOWN, SDLK_RETURN }, k;
    for (k = 0; k < 3; k++) {
      posplay_fechar();
      abrirPainel();
      assert(posplay_sobre_video());
      assert(player_base_legenda(normal, posplay_sobre_video()) == 690.0f);
      tecla(tec[k]);
      posplay_pediu_episodio(&t, &e);
      // a janela continua aberta (janelaSerie = 1) nos quadros seguintes
      posplay_atualizar(0.016f, 1016, 3016.0, 3600.0, 1, 0, 1);
      posplay_atualizar(0.016f, 1032, 3032.0, 3600.0, 1, 0, 1);
      assert(!posplay_sobre_video());
      assert(player_base_legenda(normal, posplay_sobre_video()) == normal);
    }
    // Contagem encerrada: o cartao some e a legenda desce junto.
    posplay_fechar();
    aquecer(3600.0, 1);
    posplay_atualizar(0.016f, 1000, 3597.0, 3600.0, 1, 0, 1);
    assert(player_base_legenda(normal, posplay_sobre_video()) == 690.0f);
    posplay_atualizar(0.016f, 1000 + 3100, 3600.0, 3600.0, 1, 0, 1);
    assert(player_base_legenda(normal, posplay_sobre_video()) == normal);
    posplay_pediu_episodio(&t, &e);
    posplay_fechar();
  }
  puts("ok  2.0.3: a legenda sobe com o cartao e desce no quadro em que ele some");

  // 5. CONTAGEM FINAL. O cabecalho "A seguir em %d s", a constante de 5 s e o
  //    bloco que consome `fecharEm` existiam desde o inicio; faltava a linha
  //    que arma o relogio, entao o painel ficava em "A seguir" para sempre e
  //    nada tocava sozinho. No video do relato e o que se ve.
  posplay_fechar();
  aquecer(3600.0, 1);
  posplay_atualizar(0.016f, 1000, 3597.0, 3600.0, 1, 0, 1);   // faltam 3 s
  assert(posplay_visivel());
  posplay_atualizar(0.016f, 1000 + 3100, 3600.0, 3600.0, 1, 0, 1);
  assert(!posplay_visivel());
  assert(posplay_pediu_episodio(&t, &e) && t == 5 && e == 4);
  puts("ok  a contagem final toca o proximo sozinha");

  // Recuar durante a contagem cancela sem tirar o cartao manual. Mesmo um
  // recuo que continua na janela final nao pode rearmar o relogio antigo.
  posplay_fechar();
  aquecer(3600.0, 1);
  posplay_atualizar(0.016f, 1000, 3597.0, 3600.0, 1, 0, 1);
  posplay_recuar();
  posplay_atualizar(0.016f, 6000, 3596.0, 3600.0, 1, 0, 1);
  posplay_atualizar(0.016f, 11000, 3600.0, 3600.0, 1, 0, 1);
  assert(posplay_visivel() && !posplay_pediu_episodio(&t, &e));
  puts("ok  recuo cancela a contagem e nao a rearma na mesma janela");

  // Sair da janela libera uma nova passagem pelo fim, e outro titulo tambem.
  posplay_atualizar(0.016f, 12000, 3000.0, 3600.0, 1, 0, 0);
  posplay_atualizar(0.016f, 13000, 3597.0, 3600.0, 1, 0, 1);
  posplay_atualizar(0.016f, 16100, 3600.0, 3600.0, 1, 0, 1);
  assert(posplay_pediu_episodio(&t, &e) && t == 5 && e == 4);
  posplay_fechar();
  aquecer(3600.0, 1);
  posplay_atualizar(0.016f, 1000, 3597.0, 3600.0, 1, 0, 1);
  posplay_atualizar(0.016f, 4100, 3600.0, 3600.0, 1, 0, 1);
  assert(!posplay_visivel());

  // O roteador pode ainda nao ter consumido o pedido automatico. Recuar
  // cancela esse pedido ja enfileirado e devolve a escolha manual no cartao.
  posplay_recuar();
  assert(!posplay_pediu_episodio(&t, &e) && posplay_visivel());
  posplay_atualizar(0.016f, 10000, 3598.0, 3600.0, 1, 0, 1);
  assert(!posplay_pediu_episodio(&t, &e));
  assert(tecla(SDLK_RETURN) == 1);
  posplay_recuar();                    // pedido explicito nao e automatico
  assert(posplay_pediu_episodio(&t, &e) && t == 5 && e == 4);
  assert(!posplay_pediu_episodio(&t, &e));
  puts("ok  recuo cancela Next automatico pendente e preserva o OK manual");

  // 6. FORA DA JANELA nao arma contagem nenhuma.
  posplay_fechar();
  posplay_atualizar(0.016f, 1000, 1800.0, 3600.0, 1, 0, 0);
  assert(!posplay_visivel());
  posplay_atualizar(0.016f, 1000 + 9000, 1809.0, 3600.0, 1, 0, 0);
  assert(!posplay_pediu_episodio(&t, &e));
  puts("ok  fora da janela nada e armado");

  // 7. ULTIMO EPISODIO: nao ha proximo, o painel nao sobe.
  posplay_fechar();
  { CatItem u = *cat_item(0);
    u.temporada = 5; u.episodio = 10;
    cat_definir(&u, 1);
    cat_definir_episodios(0, eps, 10); }
  aquecer(3600.0, 1);
  posplay_atualizar(0.016f, 1000, 3597.0, 3600.0, 1, 0, 1);
  assert(!posplay_visivel());
  // 2.0.3: janela aberta SEM cartao: a legenda nem sobe.
  assert(player_base_legenda(1000.0f, posplay_sobre_video()) == 1000.0f);
  posplay_atualizar(0.016f, 1000 + 9000, 3600.0, 3600.0, 1, 0, 1);
  assert(!posplay_pediu_episodio(&t, &e));
  puts("ok  ultimo episodio nao oferece proximo");

  // 8. QUEM MANDA E O PLAYER, e nao o item do catalogo. Aparecido so na TV:
  //    tocando T2E8 com o item ainda apontando para T2E6 (foi assim que a
  //    serie entrou, por "Continuar assistindo"), o painel oferecia T2E7.
  posplay_fechar();
  { CatItem u = *cat_item(0);
    u.temporada = 5; u.episodio = 2;      // o item ficou para tras
    cat_definir(&u, 1);
    cat_definir_episodios(0, eps, 10); }
  player_abrir(0, NULL);
  player_definir_episodio(5, 8);          // o player esta em T5E8
  aquecer(3600.0, 1);
  posplay_atualizar(0.016f, 1000, 3000.0, 3600.0, 1, 0, 1);
  assert(posplay_visivel());
  assert(tecla(SDLK_RETURN) == 1);
  assert(posplay_pediu_episodio(&t, &e));
  assert(t == 5 && e == 9);               // e nao 3, o "proximo" de E2
  puts("ok  o proximo sai do episodio que o player toca");

  // O PAINEL DE FILME NAO PODE SUBIR NO COMECO. Relato: "More Like This
  // aparece quando o filme comeca".
  //
  // A estimativa de creditos tinha piso de 150 s. Sem teto, esse
  // piso virava a regra em qualquer coisa mais curta: no segundo ZERO ja sobrava
  // menos que a janela e o painel decidia subir. O mesmo no instante inicial em
  // que o pipeline ainda informa uma duracao pequena.
  //
  // A ASSERCAO E SOBRE A LINHA DE LOG, e nao sobre posplay_visivel(). O caminho
  // de filme so fica visivel quando ha titulos relacionados, que vem da rede e
  // aqui sao zero — entao a visibilidade diria "nao apareceu" em TODOS os casos
  // e o teste passaria com qualquer coisa, inclusive com a funcao desligada. A
  // linha "[posplay] relacionados em ..." e impressa exatamente quando a decisao
  // dispara, que e o que este teste quer medir.
  { struct { double dur, pos; int esperado; const char *nome; } casos[] = {
      { 3600.0,    0.0, 0, "filme de 1h no segundo zero" },
      { 3600.0,   30.0, 0, "filme de 1h aos 30 s" },
      {  120.0,    0.0, 0, "video de 2 min no zero — mais curto que o piso" },
      {   80.0,    5.0, 0, "video de 80 s logo no comeco" },
      { 3600.0, 3550.0, 1, "filme de 1h faltando 50 s" },
      // 2.0.3: sem marcador, tempo fixo (intro_fim_estimado_filme): 3 min em
      // filme de 1 h ou mais, 90 s abaixo de 1 h, nada abaixo de 10 min.
      {  120.0,  115.0, 0, "video de 2 min faltando 5 s: curto demais" },
      { 3600.0, 3430.0, 1, "filme de 1h faltando 170 s" },
      { 3600.0, 3400.0, 0, "filme de 1h faltando 200 s" },
      { 3000.0, 2915.0, 1, "filme de 50 min faltando 85 s" },
      { 3000.0, 2880.0, 0, "filme de 50 min faltando 120 s" },
    };
    size_t k2;
    for (k2 = 0; k2 < sizeof casos / sizeof casos[0]; k2++) {
      char linha[256] = "";
      FILE *f;
      int decidiu = 0, salvo, arq;
      posplay_fechar();
      aquecer(casos[k2].dur, 0);
      // dup/dup2 e nao freopen("/dev/tty"): o teste roda em pipe no
      // testa-tudo.sh, onde /dev/tty nao existe — reabrir por la perderia a
      // saida do resto do arquivo, e foi o que aconteceu na primeira versao.
      fflush(stdout);
      salvo = dup(fileno(stdout));
      arq = open(nvt("/nuvio-posplay-log.txt"), O_WRONLY | O_CREAT | O_TRUNC, 0600);
      if (arq >= 0) {
        dup2(arq, fileno(stdout));
        close(arq);
        posplay_atualizar(0.016f, 1000, casos[k2].pos, casos[k2].dur, 0, 0, 0);
        fflush(stdout);
        dup2(salvo, fileno(stdout));
      }
      close(salvo);
      f = fopen(nvt("/nuvio-posplay-log.txt"), "r");
      if (f) {
        while (fgets(linha, sizeof linha, f))
          if (strstr(linha, "[posplay] relacionados em")) decidiu = 1;
        fclose(f);
      }
      if (decidiu != casos[k2].esperado) {
        fprintf(stderr, "FALHOU: %s -> decidiu=%d, esperava %d\n",
                casos[k2].nome, decidiu, casos[k2].esperado);
        assert(0);
      }
    }
    remove(nvt("/nuvio-posplay-log.txt"));
    posplay_fechar(); }
  puts("ok  o painel de filme nao sobe no comeco, e ainda sobe no fim");

  // #115, Owlphibia29: "More like this aparece cedo demais — no comeco do
  // filme". O marcador de creditos do filme passava sem conferencia: um
  // capitulo "Opening Credits" aos 90 s (ou um trecho de creditos de abertura
  // no TheIntroDB) abria o painel aos 90 s. A regra pura primeiro.
  { struct { const char *nome; double pos, dur, cred; int esperado; } cf[] = {
      // O defeito: marcador no comeco. Antes daqui saia 1.
      { "creditos de abertura aos 90 s, em 95 s",  95.0, 7200.0,   90.0, 0 },
      { "creditos de abertura aos 90 s, em 1 h", 3600.0, 7200.0,   90.0, 0 },
      // Marcador recusado nao cala o painel: a estimativa vale no fim.
      { "marcador recusado, estimativa no fim",  7050.0, 7200.0,   90.0, 1 },
      { "marcador recusado, 200 s do fim",       7000.0, 7200.0,   90.0, 0 },
      // 2.0.3: creditos a 15 min do fim de um filme de 3 h valem (janela de filme).
      { "3 h, creditos a 14 min do fim",        9960.0, 10800.0, 9960.0, 1 },
      { "2 h, creditos a 20 min do fim",        6000.0, 7200.0, 6000.0, 0 },
      // Marcador no meio do filme tambem nao e credito final.
      { "marcador no meio do filme",             3700.0, 7200.0, 3600.0, 0 },
      // Marcador bom: manda, e antes dele nada sobe (nem a estimativa).
      { "marcador aceito, antes dele",           6800.0, 7200.0, 6900.0, 0 },
      { "marcador aceito, chegou nele",          6900.0, 7200.0, 6900.0, 1 },
      // Nunca antes da metade, mesmo em video curto onde piso e janela se
      // confundem.
      { "video de 30 s aos 10 s",                  10.0,   30.0,    0.0, 0 },
      { "video de 30 s aos 16 s: curto, sem estimativa", 16.0, 30.0, 0.0, 0 },
      { "sem duracao",                              0.0,    0.0,    0.0, 0 },
    };
    size_t k4;
    for (k4 = 0; k4 < sizeof cf / sizeof *cf; k4++) {
      int r = posplay_regra_filme(cf[k4].pos, cf[k4].dur, cf[k4].cred);
      if (r != cf[k4].esperado) {
        fprintf(stderr, "FALHOU: %s -> %d, esperava %d\n",
                cf[k4].nome, r, cf[k4].esperado);
        assert(0);
      }
    } }
  puts("ok  #115: marcador de creditos no comeco do filme e recusado");

  // Filme: a cena mantem o cartao compacto sobre o video, nunca os cartazes
  // que recuam a imagem. OK continua o filme, sem abrir outro titulo.
  { CatItem filme = {0};
    const char *titulos[] = {"Relacionado"}, *anos[] = {"2026"}, *posters[] = {""};
    snprintf(filme.imdb, sizeof filme.imdb, "tt0000100");
    snprintf(filme.tipo, sizeof filme.tipo, "movie");
    filme.posCreditos = 1;
    cat_definir(&filme, 1);
    extras_shot_relacionados(titulos, anos, posters, 1);
    posplay_fechar();
    aquecer(7200, 0);
    posplay_atualizar(.016f, 1000, 7100, 7200, 0, 0, 0);
    assert(posplay_visivel() && posplay_sobre_video());
    assert(posplay_teste_foco() == 0);
    assert(tecla(SDLK_RETURN) == 1);
    assert(!posplay_visivel() && posplay_pediu_titulo() < 0);
    posplay_atualizar(.016f, 2000, 7150, 7200, 0, 0, 0);
    assert(!posplay_visivel());
    posplay_atualizar(.016f, 3000, 7200, 7200, 0, 0, 0);
    assert(posplay_visivel() && !posplay_sobre_video());
    assert(tecla(SDLK_RETURN) == 1 && posplay_pediu_titulo() == 0);
    filme.posCreditos = 0;
    cat_definir(&filme, 1);
    posplay_fechar(); aquecer(7200, 0);
    posplay_atualizar(.016f, 1000, 7100, 7200, 0, 0, 0);
    assert(posplay_visivel() && !posplay_sobre_video());
    assert(tecla(SDLK_RETURN) == 1 && posplay_pediu_titulo() == 0);
    extras_shot_relacionados(titulos, anos, posters, 0);
    cat_definir(&c, 1); cat_definir_episodios(0, eps, 10);
    posplay_fechar();
  }
  puts("ok  filme com cena pos-creditos oferece continuar; sem sinal abre relacionado");

  // OK num relacionado FORA do catalogo abre o titulo (semente, como o detalhe):
  // antes ia por desc_pedir_titulo, um fio de rede, e nada abria no quadro.
  { CatItem filme = {0};
    const char *titulos[] = {"Um", "Fora"}, *anos[] = {"2026", "2025"},
               *posters[] = {"", "https://x/p.jpg"};
    snprintf(filme.imdb, sizeof filme.imdb, "tt0000100");
    snprintf(filme.tipo, sizeof filme.tipo, "movie");
    cat_definir(&filme, 1);
    extras_shot_relacionados(titulos, anos, posters, 2);
    posplay_fechar(); aquecer(7200, 0);
    posplay_atualizar(.016f, 1000, 7100, 7200, 0, 0, 0);
    assert(posplay_visivel());
    assert(tecla(SDLK_RIGHT) == 1 && tecla(SDLK_RETURN) == 1);
    assert(!posplay_visivel() && desc_titulo_pronto() >= 0);
    extras_shot_relacionados(titulos, anos, posters, 0);
    cat_definir(&c, 1); cat_definir_episodios(0, eps, 10);
    posplay_fechar();
  }
  puts("ok  OK em relacionado fora do catalogo abre o titulo");

  // #419: recomendacao TMDB ("tmdb:<id>") no pos-play abre com a semente que
  // ja temos, mesmo sem IMDb no catalogo e sem depender do /meta imediato.
  { CatItem filme = {0};
    const char *titulos[] = {"Recomendado"}, *anos[] = {"2026"},
               *posters[] = {"https://image.tmdb.org/t/p/w342/rec.jpg"},
               *ids[] = {"tmdb:778"};
    snprintf(filme.imdb, sizeof filme.imdb, "tt0000100");
    snprintf(filme.tipo, sizeof filme.tipo, "movie");
    cat_definir(&filme, 1);
    extras_shot_relacionados_ids(titulos, anos, posters, ids, 1);
    posplay_fechar(); aquecer(7200, 0);
    posplay_atualizar(.016f, 1000, 7100, 7200, 0, 0, 0);
    assert(posplay_visivel());
    assert(tecla(SDLK_RETURN) == 1);
    { int w = 0, k;
      while (desc_titulo_buscando() && w++ < 500) usleep(10 * 1000);
      k = desc_titulo_pronto();
      assert(k >= 0);
      assert(!strcmp(cat_item(k)->imdb, "tmdb:778"));
      assert(cat_item(k)->tmdb == 778);
      assert(!strcmp(cat_item(k)->titulo, "Recomendado")); }
    extras_shot_relacionados(titulos, anos, posters, 0);
    cat_definir(&c, 1); cat_definir_episodios(0, eps, 10);
    posplay_fechar();
  }
  puts("ok  #419: OK em recomendacao tmdb abre com semente");

  // E a DURACAO PROVISORIA: se o pipeline informa 30 s no primeiro instante,
  // "video de 30 s aos 16 s" acima e verdade para a regra — quem segura e a
  // exigencia de duracao estavel. Um quadro com duracao nova nao decide nada,
  // nem no filme nem na serie (cuja regra de 2 min valeria na hora).
  posplay_fechar();
  aquecer(7200.0, 0);
  assert(!quadroFilmeDecide(0.5f, 16.0, 30.0));   // duracao mudou agora
  assert(!quadroFilmeDecide(0.5f, 16.5, 30.0));
  // E a trava nao e para sempre: com a duracao parada, o fim decide.
  posplay_fechar();
  aquecer(1200.0, 0);
  assert(quadroFilmeDecide(0.5f, 1150.0, 1200.0));   // 2.0.3: video < 10 min nao estima; 20 min, 50 s do fim
  posplay_fechar();
  aquecer(3600.0, 1);
  posplay_atualizar(0.5f, 1000, 16.0, 60.0, 1, 0, 1);   // serie: 60 s provisorios
  assert(!posplay_visivel());
  posplay_fechar();
  puts("ok  #115: duracao que acabou de mudar nao abre painel nenhum");

  // O CARTAO DE PROXIMO EPISODIO, issue #34: "the play next option kicks in
  // before the show has got to the credits". A causa era ordem de regras, nao
  // marcador ausente: o marcador era lido e aprovado, e a regra dos 2 minutos
  // logo abaixo dele abria o cartao primeiro em todo episodio cujos creditos
  // comecassem a menos de 120 s do fim — que e a maioria.
  { struct { const char *nome; double pos, dur, cred; int esperado; } cs[] = {
      // 22,5 min com creditos a 60 s do fim. A 110 s do fim a regra velha ja
      // abria o cartao; o marcador diz que ainda falta um minuto de episodio.
      { "marcador aceito, ainda antes dele",   1240.0, 1350.0, 1290.0, 0 },
      { "marcador aceito, chegou nele",        1290.0, 1350.0, 1290.0, 1 },
      { "marcador aceito, depois dele",        1330.0, 1350.0, 1290.0, 1 },
      // SEM MARCADOR (2.0.3): tempo FIXO antes do fim, nunca porcentagem —
      // 50 s (intro_fim_estimado, eram 40 s ate 08/10); era "os 2 min finais".
      { "sem marcador, 40 s finais",           1311.0, 1350.0,    0.0, 1 },
      { "sem marcador, 50 s finais",           1300.0, 1350.0,    0.0, 1 },
      { "sem marcador, 110 s do fim (antes: 2 min)", 1240.0, 1350.0, 0.0, 0 },
      { "sem marcador, 1 h, 39 s do fim",      3561.0, 3600.0,    0.0, 1 },
      { "sem marcador, 1 h, 51 s do fim",      3549.0, 3600.0,    0.0, 0 },
      // SILO T2E6 NA TCL DO DONO (08/10): o TheIntroDB marca creditos em
      // 2519 s, o arquivo tem 3077 s e os creditos reais comecam ~50 s antes do
      // fim. O marcador e RECUSADO (sobram 558 s) e o cartao cai na estimativa
      // fixa — no marcador nada sobe, aos 50 s finais sobe.
      { "Silo T2E6, no marcador recusado",     2519.0, 3077.0, 2519.0, 0 },
      { "Silo T2E6, 51 s do fim",              3026.0, 3077.0, 2519.0, 0 },
      { "Silo T2E6, 50 s do fim",              3027.0, 3077.0, 2519.0, 1 },
      // Episodio < 10 min: 15 s.
      { "8 min, 15 s finais",                   465.0,  480.0,    0.0, 1 },
      { "8 min, 20 s do fim",                   460.0,  480.0,    0.0, 0 },
      // 22 min com "creditos" a 5 min do fim (17:00): a janela antiga (300 s)
      // aceitava; a nova (15% = 198 s) recusa — e o cartao do meio do episodio.
      { "22 min, marcador a 5 min do fim",     1020.0, 1320.0, 1020.0, 0 },
      { "22 min, marcador a 3 min do fim",     1140.0, 1320.0, 1140.0, 1 },
      // Marcador absurdo (creditos no meio do episodio) e RECUSADO pela
      // sanidade e nao pode calar a regra de baixo — senao o cartao nunca
      // apareceria naquele episodio.
      { "marcador recusado, meio do episodio", 2000.0, 3000.0, 1000.0, 0 },
      { "marcador recusado, fim do episodio",  2951.0, 3000.0, 1000.0, 1 },
      { "marcador recusado, 100 s do fim",     2900.0, 3000.0, 1000.0, 0 },
      // Duracao invalida: nao decide nada.
      { "sem duracao",                            0.0,    0.0,    0.0, 0 },
      // 2.0.1, casos tirados de log. TCL: "2 min finais: pos 0s de 30s".
      { "30 s informados, segundo zero",          0.0,   30.0,    0.0, 0 },
      { "30 s informados, fim",                  29.0,   30.0,    0.0, 0 },
      // webOS: fonte MP4 com "duration":536033 e creditos em 1335 s.
      { "536 s falsos, marcador adiante",       416.0,  536.0, 1335.0, 0 },
      { "536 s falsos, chegou no marcador",    1335.0,  536.0, 1335.0, 1 },
      // Uma hora com creditos a 369 s do fim: sobra mais que a janela, quem
      // decide a estimativa fixa de 50 s.
      { "1 h, marcador fora da janela",        3231.0, 3600.0, 3231.0, 0 },
      { "1 h, 2 min finais (antes abria)",     3480.0, 3600.0, 3231.0, 0 },
      { "1 h, 40 s finais",                    3560.0, 3600.0, 3231.0, 1 },
    };
    for (size_t k3 = 0; k3 < sizeof cs / sizeof *cs; k3++) {
      int r = player_regra_proximo(cs[k3].pos, cs[k3].dur, cs[k3].cred);
      if (r != cs[k3].esperado) {
        fprintf(stderr, "FALHOU: %s -> %d, esperava %d\n",
                cs[k3].nome, r, cs[k3].esperado);
        assert(0);
      }
    } }
  puts("ok  o cartao de proximo episodio obedece ao marcador de creditos");

  // 2.0.1: a duracao falsa tambem nao pode dar o episodio por visto.
  assert(!player_regra_concluiu(476.0, 536.0, 1335.0));
  assert( player_regra_concluiu(1335.0, 536.0, 1335.0));
  assert(!player_regra_concluiu(0.0, 30.0, 0.0));
  assert( player_regra_concluiu(1300.0, 1350.0, 0.0));
  assert( player_duracao_suspeita(536.0, 38.0 * 60.0));
  assert(!player_duracao_suspeita(1290.0, 38.0 * 60.0));   // catalogo arredondado
  assert(!player_duracao_suspeita(536.0, 0.0));            // sem minutagem
  assert(!player_duracao_suspeita(200.0, 10.0 * 60.0));    // catalogo curto demais
  puts("ok  duracao falsa nao abre o cartao nem conclui o episodio");

  // #177: o still do proximo episodio segue "desfocar nao assistidos".
  ajustes_aplicar_blob("{\"features\":{\"layout_settings\":{"
    "\"blur_unwatched_episodes\":{\"type\":\"boolean\",\"value\":false}}}}");
  assert(posplay_desfocar_thumb(0, 5, 4) == 0);            // ajuste desligado
  ajustes_aplicar_blob("{\"features\":{\"layout_settings\":{"
    "\"blur_unwatched_episodes\":{\"type\":\"boolean\",\"value\":true}}}}");
  assert(posplay_desfocar_thumb(0, 5, 4) == 1);            // nao visto: desfoca
  vistoep_definir("tt0944947", 5, 4, 0);
  assert(posplay_desfocar_thumb(0, 5, 4) == 1);            // sabido NAO visto
  vistoep_definir("tt0944947", 5, 4, 1);
  assert(posplay_desfocar_thumb(0, 5, 4) == 0);            // reassistindo: nitido
  puts("ok  o still do proximo episodio obedece a desfocar nao assistidos");

  // Janela REAL do player antecipada: mesmas guardas/marcadores do cartao.
  player_definir_episodio(5, 3);
  intro_shot_definir(NULL, 0);
  player_shot_estado(1000, 3539, 3600, 1, 0, 0, 0);
  assert(!player_janela_proximo(10));
  player_shot_estado(1000, 3540, 3600, 1, 0, 0, 0);
  assert(player_janela_proximo(10) && !player_janela_proximo(0));
  intro_definir_duracao(3600, 0);
  { IntroTrecho cred = {3500, 3600, INTRO_CREDITOS};
    intro_shot_definir(&cred, 1);
    player_shot_estado(1000, 3489, 3600, 1, 0, 0, 0);
    assert(!player_janela_proximo(10));
    player_shot_estado(1000, 3490, 3600, 1, 0, 0, 0);
    assert(player_janela_proximo(10) && !player_janela_proximo(0));
  }
  intro_shot_definir(NULL, 0);
  puts("ok  prefetch antecipa a janela real em 10 s, com e sem creditos");
  // #470: a contagem comeca com o cartao, para toda fonte aceita pelo player.
  assert(posplay_contagem(1000, 0, 120.0, 1, 5) == 6000);        // conta ao subir
  assert(posplay_contagem(4999, 0, 120.0, 1, 5) == 9999);        // sem atraso
  assert(posplay_contagem(5000, 0, 120.0, 1, 5) == 10000);       // marcador real: 5 s a partir de agora
  assert(posplay_contagem(5000, 0, 120.0, 1, 5) == 10000);       // mesma janela aceita pelo cartao
  assert(posplay_contagem(60000, 0, 100.0, 1, 5) == 65000);       // estimativa tambem conta
  assert(posplay_contagem(5500, 10000, 100.0, 1, 5) == 10000);   // armado: nao se move
  assert(posplay_contagem(5000, 0, 120.0, 0, 5) == 0);           // autoplay desligado: nunca
  assert(posplay_contagem(2000, 0, 3.0, 1, 5) == 5000);          // estimativa, 3 s de episodio: so o fim
  assert(posplay_contagem(2000, 0, -1.0, 1, 5) == 2000);         // passou do fim: dispara ja
  assert(posplay_contagem(5000, 0, 120, 1, 12) == 17000);
  assert(posplay_contagem(5000, 0, 120, 1, 1) == 6000);
  assert(posplay_contagem(5000, 17000, 100, 0, 12) == 0);
  assert(posplay_contagem(5000, 0, 3, 1, 12) == 8000);
  puts("ok  contagem do proximo episodio: imediata, configuravel, autoplay desligado");

  // Cena apos os creditos: o cartao oferece continuar, mas nao troca o
  // episodio enquanto a cena ainda roda. Sem sinal, a contagem antiga vale.
  { CatItem u = *cat_item(0);
    u.temporada = 5; u.episodio = 3;
    u.posCreditos = desc_tmdb_pos_creditos("{\"keywords\":{\"keywords\":[{\"id\":179431,\"name\":\"aftercreditsstinger\"}]}}");
    assert(u.posCreditos);
    assert(desc_tmdb_pos_creditos("{\"keywords\":{\"keywords\":[{\"id\":179430,\"name\":\"duringcreditsstinger\"}]}}"));
    assert(desc_tmdb_pos_creditos("{\"keywords\":{\"results\":[{\"id\":179431,\"name\":\"aftercreditsstinger\"}]}}"));
    assert(!desc_tmdb_pos_creditos("{\"keywords\":null,\"results\":[{\"id\":179431}]}"));
    assert(!desc_tmdb_pos_creditos("{\"keywords\":null,\"recommendations\":{\"results\":[{\"id\":179431}]}}"));
    assert(!desc_tmdb_pos_creditos("{\"keywords\" : null , \"x\":{\"keywords\":[{\"id\":179430}]}}"));
    assert(!desc_tmdb_pos_creditos("{\"keywords\":{\"keywords\":[{\"id\":1,\"name\":\"credits\"}]}}"));
    cat_definir(&u, 1);
    cat_definir_episodios(0, eps, 10);
    posplay_fechar();
    aquecer(3600.0, 1);
    posplay_atualizar(0.016f, 1000, 3450.0, 3600.0, 1, 0, 1);
    assert(posplay_visivel() && posplay_sobre_video());
    posplay_atualizar(0.016f, 12000, 3470.0, 3600.0, 1, 0, 1);
    assert(posplay_visivel() && !posplay_pediu_episodio(&t, &e));
    assert(tecla(SDLK_RETURN) == 1); // continuar assistindo, nao trocar
    assert(!posplay_visivel() && !posplay_pediu_episodio(&t, &e));
    posplay_atualizar(0.016f, 15000, 3480.0, 3600.0, 1, 0, 1);
    assert(!posplay_visivel());
    posplay_atualizar(0.016f, 16000, 3598.8, 3600.0, 1, 0, 1);   // pipeline para antes de 3600
    assert(posplay_visivel() && !posplay_pediu_episodio(&t, &e)); // volta inteiro, conta o que resta
    posplay_atualizar(0.016f, 17300, 3600.0, 3600.0, 1, 0, 1);
    assert(!posplay_visivel() && posplay_pediu_episodio(&t, &e) && e == 4);
    posplay_fechar();   // durante a cena a pessoa ainda pode ir ao proximo na mao
    aquecer(3600.0, 1);
    posplay_atualizar(0.016f, 1000, 3450.0, 3600.0, 1, 0, 1);
    assert(posplay_visivel() && posplay_teste_foco() == 0);
    assert(tecla(SDLK_RIGHT) == 1 && posplay_teste_foco() == 1);
    assert(tecla(SDLK_RETURN) == 1);
    assert(!posplay_visivel() && posplay_pediu_episodio(&t, &e) && e == 4);
    posplay_fechar();
    { CatItem dup[2] = { u, u }, velha = u;   // marca chega depois, em todas as fileiras
      dup[0].posCreditos = dup[1].posCreditos = 0;
      cat_definir(dup, 2);
      cat_marcar_pos_creditos(u.imdb, 1);
      assert(cat_item(0)->posCreditos && cat_item(1)->posCreditos);
      velha.posCreditos = 0;
      cat_atualizar_item(1, &velha);
      cat_atualizar_item_sem_abas(0, &velha);
      assert(cat_item(0)->posCreditos && cat_item(1)->posCreditos); }
    u.posCreditos = 0;
    cat_definir(&u, 1);
    cat_definir_episodios(0, eps, 10);
    aquecer(3600.0, 1);
    posplay_atualizar(0.016f, 1000, 3450.0, 3600.0, 1, 0, 1);
    posplay_atualizar(0.016f, 12000, 3470.0, 3600.0, 1, 0, 1);
    posplay_atualizar(0.016f, 18000, 3490.0, 3600.0, 1, 0, 1);
    assert(!posplay_visivel() && posplay_pediu_episodio(&t, &e) && e == 4);
    assert(player_regra_concluiu(3450.0, 3600.0, 3450.0)); // visto segue nos creditos
  }
  puts("ok  cena pos-creditos espera o fim; sem sinal avanca; visto permanece nos creditos");

  // #556: zero e uma temporada real em todos os caminhos de Next.
  {
    CatItem u = {0}; CatEp esp[2] = {{0}};
    strcpy(u.imdb, "tt12409194"); strcpy(u.tipo, "series"); u.episodio = 1;
    esp[0].episodio = 1; esp[1].episodio = 2;
    cat_definir(&u, 1); cat_definir_episodios(0, esp, 2);
    player_abrir(0, NULL); player_definir_episodio(0, 1);
    extras_teste_progresso(u.imdb, 0, 21, 0, 2);
    assert(extras_proximo_episodio(&t, &e) && t == 0 && e == 2);

    posplay_fechar(); aquecer(3600, 1);
    posplay_atualizar(.016f, 1000, 3450, 3600, 1, 0, 1);
    assert(posplay_visivel() && temProximo());
    assert(tecla(SDLK_RETURN) == 1);
    assert(posplay_pediu_episodio(&t, &e) && t == 0 && e == 2);
    assert(!posplay_pediu_episodio(NULL, NULL));

    posplay_fechar(); aquecer(3600, 1);
    posplay_atualizar(.016f, 1000, 3597, 3600, 1, 0, 1);
    posplay_atualizar(.016f, 4100, 3600, 3600, 1, 0, 1);
    assert(posplay_pediu_episodio(&t, &e) && t == 0 && e == 2);
    posplay_busca_usuario(); // um Next ja entregue tambem pode ser revogado
    assert(posplay_visivel());

    posplay_fechar(); aquecer(3600, 1);
    posplay_atualizar(.016f, 1000, 3597, 3600, 1, 0, 1);
    posplay_atualizar(.016f, 4100, 3600, 3600, 1, 0, 1);
    posplay_recuar(); // revoga o pedido ainda nao consumido, mantendo o OK
    assert(posplay_visivel() && !posplay_pediu_episodio(NULL, NULL));
    assert(tecla(SDLK_RETURN) == 1);
    assert(posplay_pediu_episodio(&t, &e) && t == 0 && e == 2);

    u.posCreditos = 1; cat_definir(&u, 1); cat_definir_episodios(0, esp, 2);
    posplay_fechar(); aquecer(3600, 1);
    posplay_atualizar(.016f, 1000, 3450, 3600, 1, 0, 1);
    assert(aguardaCena && temProximo());
    assert(tecla(SDLK_RIGHT) == 1 && posplay_teste_foco() == 1);
    assert(tecla(SDLK_RETURN) == 1);
    assert(posplay_pediu_episodio(&t, &e) && t == 0 && e == 2);

    // O dedo passa pelo roteador real e pelo callback real da cena, sem GL.
    posplay_fechar(); aquecer(3600, 1);
    posplay_atualizar(.016f, 1000, 3450, 3600, 1, 0, 1);
    ponteiro_iniciar(); ponteiro_teste_janela(1920, 1080);
    ponteiro_teste_relogio(relogioDedo556); ponteiro_teste_toque(1);
    ponteiro_quadro(dedoAgora556);
    assert(aguardaCena && temProximo()); // a mesma guarda que registra os dois botoes
    ponteiro_alvo(100, 100, 100, 80, ponteiroCena, NULL, 0, 0);
    ponteiro_alvo(300, 100, 100, 80, ponteiroCena, NULL, 1, 0);
    ponteiro_desenhar();
    { SDL_Event ev = {0};
      ev.type = SDL_FINGERDOWN; ev.tfinger.x = 350.0f / 1920; ev.tfinger.y = 140.0f / 1080;
      ev.tfinger.touchId = 1; ev.tfinger.fingerId = 1;
      assert(ponteiro_evento(&ev, entregarDedo556));
      dedoAgora556 += 40; ev.type = SDL_FINGERUP;
      assert(ponteiro_evento(&ev, entregarDedo556)); }
    assert(posplay_pediu_episodio(&t, &e) && t == 0 && e == 2);
    ponteiro_teste_relogio(NULL); ponteiro_teste_toque(0);
    player_encerrar(); posplay_fechar();
  }
  puts("ok  #556: S00 Next por OK, contagem, recuo, cena e toque");
  puts("posplay: tudo ok");
  return 0;
}
