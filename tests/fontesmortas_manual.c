// Recusa sincrona: video_tocar=0 -> player real em erro -> vigia real -> disco.
#define SDL_MAIN_HANDLED
#include "../src/app.c"
#include <assert.h>
#include "dados.h"
#include "linguas.h"

static int backendFalhou, chamadas, aceita, pronto;
static double pos;
int video_tocar(const char *url) {
  assert(!strcmp(url, "https://example.invalid/recusado.mp4"));
  chamadas++; backendFalhou = !aceita; return aceita;
}
int video_falhou(void) { return backendFalhou; }
int video_pronto(void) { return pronto; }
double video_pos(void) { return pos; }

int main(int argc, char **argv) {
  CatItem c = {0};
  Stream *s = calloc(1, sizeof *s); assert(s);
  assert(!SDL_Init(SDL_INIT_TIMER)); dados_iniciar(""); ling_local_legenda("none");
  strcpy(c.titulo, "Fixture"); strcpy(c.tipo, "movie"); cat_definir(&c, 1);
  strcpy(s->provedor, "Addon"); strcpy(s->url, "https://example.invalid/recusado.mp4");
  stream_definir_lista(s, 1); stream_definir_atual(0);
  player_abrir(0, NULL);
  if (argc > 1 && !strcmp(argv[1], "pick")) {
    aceita = 1;
    fonteEscolhida = 0; aguardandoFonte = 2;
    fonteVODAutomatica = 1; voltaAtiva = 1;
    unsigned before = atomic_load(&stalkerGeracao);
    abrirFonteManualDireta(s);
    assert(chamadas == 1 && !aguardandoFonte && !fonteVODAutomatica && !voltaAtiva);
    assert(atomic_load(&stalkerGeracao) != before && fonteManualDesde);
    backendFalhou = 1;
    vigiarAberturaManual();
    assert(chamadas == 1 && player_fonte_falhou());
    puts("manual pick: PASS (exact URL, stale worker invalidated, no automatic replacement)");
    return 0;
  }
  if (argc > 1) {
    int tardia = !strcmp(argv[1], "tardia");
    VideoSimulacao sim = {0};
    aceita = sim.aceita = 1;
    video_simular(&sim);
    player_definir_fonte(s->url);
    SDL_Delay(1); fonteManualDesde = SDL_GetTicks();
    if (!strcmp(argv[1], "desconhecida")) {
      // Duracao desconhecida: progresso real limpa a marca e blinda o erro tardio.
      stream_vod_falhou(0);
      assert(stream_vod_falhou_ha(0) >= 0);
      pronto = 1; pos = 0;
      vigiarAberturaManual();
      assert(fonteManualDesde && "pronto sem progresso desarmou o vigia");
      assert(stream_vod_falhou_ha(0) >= 0 && "sem progresso apagou a marca");
      for (pos = 1.0; pos <= 180.0; pos += 1.0) vigiarAberturaManual();
      assert(stream_vod_falhou_ha(0) < 0 && "tocar com duracao desconhecida manteve a marca");
      backendFalhou = 1;
      vigiarAberturaManual();
      assert(stream_vod_falhou_ha(0) < 0);
      fontesmortas_iniciar();
      assert(stream_vod_falhou_ha(0) < 0 && "limpeza nao chegou ao disco");
      stream_definir_lista(NULL, 0); free(s);
      puts("fontesmortas: PASS (duracao desconhecida, progresso limpa a marca)");
      return 0;
    }
    if (!strcmp(argv[1], "auto")) {
      // Automatico: tres minutos tocando, duracao desconhecida, depois falha.
      fonteVODAutomatica = 1; fonteVODDesde = SDL_GetTicks(); fonteVODTentativas = 1;
      pronto = 1; pos = 0;
      for (pos = 1.0; pos <= 180.0; pos += 1.0) registrarFonteVODTocando();
      backendFalhou = 1;
      tentarProximaFonteVOD();
      printf("automatico: erro aos %.0f s, idade=%ld\n", pos, stream_vod_falhou_ha(0));
      assert(stream_vod_falhou_ha(0) < 0 && "automatico marcou fonte que ja tocou");
      fontesmortas_iniciar();
      assert(stream_vod_falhou_ha(0) < 0 && "falha tardia chegou ao disco");
      stream_definir_lista(NULL, 0); free(s);
      puts("fontesmortas: PASS (automatico, duracao desconhecida, erro tardio)");
      return 0;
    }
    if (!strcmp(argv[1], "antecipada")) {
      // Aberta antes do veredito: posicao em 3 s, erro antes da sonda concluir.
      unsigned rodada = fa_nova_rodada();
      sim.duracao = 5400; video_simular(&sim);
      fa_publicar(rodada, 0, stream_lista_geracao());
      aguardandoFonte = 2; fonteEscolhida = -2;
      vigiarAntecipada();
      assert(antAberta == 0 && "candidata nao abriu antes do veredito");
      pronto = 1;
      for (pos = 0; pos <= 3.0; pos += 1.0) { registrarFonteVODTocando(); vigiarAntecipada(); }
      backendFalhou = 1;
      vigiarAntecipada();
      printf("antecipada: erro aos %.0f s, idade=%ld\n", pos - 1.0, stream_vod_falhou_ha(0));
      assert(stream_vod_falhou_ha(0) < 0 && "abertura antecipada marcou fonte que ja tocou");
      fontesmortas_iniciar();
      assert(stream_vod_falhou_ha(0) < 0 && "falha tardia chegou ao disco");
      stream_definir_lista(NULL, 0); free(s);
      puts("fontesmortas: PASS (aberta antes do veredito, erro tardio)");
      return 0;
    }
    if (tardia) {
      fonteManualDesde -= FONTE_MANUAL_PRAZO_MS + 1;
      vigiarAberturaManual();
      assert(!fonteManualDesde && player_fonte_falhou());
    } else stream_vod_falhou(0);
    assert(stream_vod_falhou_ha(0) >= 0);
    pronto = 1; pos = 1.0;
    // Menos de 2 s de progresso nao prova nada; clipe curto anda mas nao e o titulo.
    sim.duracao = 30; video_simular(&sim);
    vigiarAberturaManual();
    assert(stream_vod_falhou_ha(0) >= 0 && "clipe de aviso apagou a marca");
    sim.duracao = 0; video_simular(&sim);
    vigiarAberturaManual();
    assert(stream_vod_falhou_ha(0) >= 0 && "menos de 2 s de progresso apagou a marca");
    pos = 4.0; sim.duracao = 30; video_simular(&sim);
    vigiarAberturaManual();
    assert(stream_vod_falhou_ha(0) >= 0 && "clipe de aviso com progresso apagou a marca");
    sim.duracao = 5400; video_simular(&sim);
    // O loop tira o cartao quando o video volta, antes do vigia manual.
    if (tardia) player_limpar_erro_fonte();
    vigiarAberturaManual();
    assert(!fonteManualDesde && stream_vod_falhou_ha(0) < 0 && "titulo tocando manteve a marca");
    fontesmortas_iniciar();
    assert(stream_vod_falhou_ha(0) < 0 && "limpeza nao chegou ao disco");
    stream_definir_lista(NULL, 0); free(s);
    printf("fontesmortas: PASS (%s, player e vigia reais, disco)\n", argv[1]);
    return 0;
  }
  player_definir_fonte(s->url);
  SDL_Delay(1); fonteManualDesde = SDL_GetTicks();
  assert(chamadas == 1 && video_falhou() && player_fonte_falhou());
  assert(stream_vod_falhou_ha(0) < 0);
  vigiarAberturaManual();
  assert(!fonteManualDesde);
  assert(stream_vod_falhou_ha(0) >= 0 && "recusa sincrona manual escapou da marcacao");
  fontesmortas_iniciar();
  // Outra busca, token renovado e indice novo ainda encontram a falha no disco.
  strcat(s->url, "?token=novo"); stream_definir_lista(s, 1);
  assert(stream_vod_falhou_ha(0) >= 0 && !stream_automatico_disponivel(0));
  stream_definir_lista(NULL, 0); free(s);
  puts("fontesmortas: PASS (video_tocar=0, player e vigia reais, disco e busca seguinte)");
}
