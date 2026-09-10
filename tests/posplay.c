// O CARTAO DE PROXIMO EPISODIO, do OK ate o pedido chegar ao roteador.
//
// Issue #14: "o aviso de proximo episodio aparece, eu aperto OK, e o proximo
// nao comeca". O relato veio com video, e no video o cartao NEM SUME — fica na
// tela depois do OK. Este teste cobre o caminho inteiro que o app.c consome:
// posplay_evento -> posplay_pediu_episodio.
#include "posplay.h"
#include "catalogo.h"
#include "player.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static int tecla(int k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  return posplay_evento(&e);
}

// Deixa o painel no ar com a serie em T5E3, cujo proximo e T5E4.
static void abrirPainel(void) {
  posplay_fechar();
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

  // 5. CONTAGEM FINAL. O cabecalho "A seguir em %d s", a constante de 5 s e o
  //    bloco que consome `fecharEm` existiam desde o inicio; faltava a linha
  //    que arma o relogio, entao o painel ficava em "A seguir" para sempre e
  //    nada tocava sozinho. No video do relato e o que se ve.
  posplay_fechar();
  posplay_atualizar(0.016f, 1000, 3597.0, 3600.0, 1, 0, 1);   // faltam 3 s
  assert(posplay_visivel());
  posplay_atualizar(0.016f, 1000 + 3100, 3600.0, 3600.0, 1, 0, 1);
  assert(!posplay_visivel());
  assert(posplay_pediu_episodio(&t, &e) && t == 5 && e == 4);
  puts("ok  a contagem final toca o proximo sozinha");

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
  posplay_atualizar(0.016f, 1000, 3597.0, 3600.0, 1, 0, 1);
  assert(!posplay_visivel());
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
  posplay_atualizar(0.016f, 1000, 3000.0, 3600.0, 1, 0, 1);
  assert(posplay_visivel());
  assert(tecla(SDLK_RETURN) == 1);
  assert(posplay_pediu_episodio(&t, &e));
  assert(t == 5 && e == 9);               // e nao 3, o "proximo" de E2
  puts("ok  o proximo sai do episodio que o player toca");

  // O PAINEL DE FILME NAO PODE SUBIR NO COMECO. Relato: "More Like This
  // aparece quando o filme comeca".
  //
  // A estimativa de creditos tem piso de 150 s (PP_FILME_MIN_S). Sem teto, esse
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
      {  120.0,  115.0, 1, "video de 2 min faltando 5 s" },
    };
    size_t k2;
    for (k2 = 0; k2 < sizeof casos / sizeof casos[0]; k2++) {
      char linha[256] = "";
      FILE *f;
      int decidiu = 0, salvo, arq;
      posplay_fechar();
      // dup/dup2 e nao freopen("/dev/tty"): o teste roda em pipe no
      // testa-tudo.sh, onde /dev/tty nao existe — reabrir por la perderia a
      // saida do resto do arquivo, e foi o que aconteceu na primeira versao.
      fflush(stdout);
      salvo = dup(fileno(stdout));
      arq = open("/tmp/nuvio-posplay-log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0600);
      if (arq >= 0) {
        dup2(arq, fileno(stdout));
        close(arq);
        posplay_atualizar(0.016f, 1000, casos[k2].pos, casos[k2].dur, 0, 0, 0);
        fflush(stdout);
        dup2(salvo, fileno(stdout));
      }
      close(salvo);
      f = fopen("/tmp/nuvio-posplay-log.txt", "r");
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
    remove("/tmp/nuvio-posplay-log.txt");
    posplay_fechar(); }
  puts("ok  o painel de filme nao sobe no comeco, e ainda sobe no fim");

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
      // Sem marcador nenhum a regra dos 2 minutos continua sendo a regra.
      { "sem marcador, dentro dos 2 min",      1240.0, 1350.0,    0.0, 1 },
      { "sem marcador, fora dos 2 min",        1200.0, 1350.0,    0.0, 0 },
      // Marcador absurdo (creditos no meio do episodio) e RECUSADO pela
      // sanidade e nao pode calar a regra de baixo — senao o cartao nunca
      // apareceria naquele episodio.
      { "marcador recusado, meio do episodio", 2000.0, 3000.0, 1000.0, 0 },
      { "marcador recusado, fim do episodio",  2900.0, 3000.0, 1000.0, 1 },
      // Duracao invalida: nao decide nada.
      { "sem duracao",                            0.0,    0.0,    0.0, 0 },
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

  puts("posplay: tudo ok");
  return 0;
}
