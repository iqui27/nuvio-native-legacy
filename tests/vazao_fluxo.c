// O TESTE DE VELOCIDADE DE PONTA A PONTA contra o servidor local de
// tests/vazao.sh: dois addons (um responde fontes, o outro 404), a escolha das
// candidatas (torrent sem link, link de aviso e fonte fora de cache ficam de
// fora), hosts diferentes (127.0.0.1 e localhost), o resultado, o relatorio
// SEM url nem host, e o Voltar no meio.
//
// Inclui o .c para chegar no estado static (mesma tecnica de diagnostico_fluxo).
#include "../src/diagnostico.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void esperar(Uint32 maxMs) {
  Uint32 t0 = SDL_GetTicks();
  while (atomic_load(&vz.estado) == 1 && SDL_GetTicks() - t0 < maxMs) {
    SDL_Delay(30);
    diagnostico_atualizar(0.016f, SDL_GetTicks());
  }
  diagnostico_atualizar(0.016f, SDL_GetTicks());
}

int main(int argc, char **argv) {
  char dir[] = "/tmp/nuvio-vazao-fluxo-XXXXXX", url[200];
  int porta;
  Uint32 t0;
  assert(argc > 1);
  porta = atoi(argv[1]);
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(dir);
  rede_preparar();
  snprintf(url, sizeof url, "http://127.0.0.1:%d/addon/manifest.json", porta);
  assert(addons_adicionar("Local", url));
  snprintf(url, sizeof url, "http://127.0.0.1:%d/sem-fontes/manifest.json", porta);
  assert(addons_adicionar("Vazio", url));
  diagnostico_iniciar();
  d.intro = 0;

  // 1. Do botao da tela de objetivo: baixo + OK.
  { SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_DOWN; diagnostico_evento(&e);
    e.key.keysym.sym = SDLK_RETURN; diagnostico_evento(&e); }
  assert(vz.aberto && atomic_load(&vz.estado) == 1);
  esperar(40000);
  assert(atomic_load(&vz.estado) == 2);
  assert(vz.addon[0].medido && vz.addon[0].ok && vz.addon[0].fontes == 6);
  assert(vz.addon[1].medido && !vz.addon[1].ok && vz.addon[1].http == 404);
  // 6 fontes: 3 medivel (4K, 1080p em localhost, 720p), torrent, aviso e
  // fora de cache de fora. Ficam as 3 primeiras da ordem do automatico.
  assert(vz.addon[0].candidatas == 3 && vz.nCand == 3);
  { int k;
    for (k = 0; k < vz.nCand; k++)
      assert(!strstr(vz.cand[k].url, "slate") && !strstr(vz.cand[k].url, "d.mkv"));
    assert(!vz.falhas[VR_AVISO]); }
  assert(vz.resultado == VR_OK);
  // Hosts diferentes: 127.0.0.1 e localhost; o 720p (mesmo host do 4K) nao.
  assert(atomic_load(&vz.nFonte) == 2);
  assert(vz.resumo.medianaKbps > 6000 && vz.resumo.medianaKbps < 10000);
  assert(vz.resumo.otimoKbps < vz.resumo.maximoKbps);
  puts("ok  addons medidos, candidatas filtradas, 2 hosts, ~8 Mbps");

  // 2. Relatorio: so numeros.
  atomic_store(&d.estado, 2);
  montarRelatorio();
  assert(strstr(d.relatorio, "vazao=v1\nvazao_resultado=ok\n"));
  assert(strstr(d.relatorio, "vazao_fonte=2|"));
  assert(strstr(d.relatorio, "vazao_addon=2|ms="));
  assert(!strstr(d.relatorio, "127.0.0.1") && !strstr(d.relatorio, "localhost") &&
         !strstr(d.relatorio, "/lento") && !strstr(d.relatorio, "Local"));
  puts("ok  relatorio com a vazao e sem url, host ou nome");
  atomic_store(&d.estado, 0);

  // 3. OK de novo, e Voltar no meio: cancela sem esperar a janela.
  { SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_RETURN; diagnostico_evento(&e);
    assert(atomic_load(&vz.estado) == 1);
    SDL_Delay(1500);
    t0 = SDL_GetTicks();
    e.key.keysym.sym = SDLK_ESCAPE; diagnostico_evento(&e);
    esperar(15000);
    assert(atomic_load(&vz.estado) == 3 && vz.resultado == VR_CANCELADO);
    assert(SDL_GetTicks() - t0 < 3000);
    assert(vz.aberto);
    diagnostico_evento(&e);           // Voltar de novo fecha o resultado
    assert(!vz.aberto && !diagnostico_quer_sair()); }
  puts("ok  Voltar cancela o teste em curso; Voltar de novo fecha");

  // 4. O ATALHO DE AJUSTES: abre direto no teste (sem apresentacao nem
  // objetivo, mesmo com a apresentacao nunca vista), mede de verdade, e o
  // Voltar do resultado sai da tela em vez de cair na escolha do objetivo.
  diagnostico_abrir_velocidade();
  diagnostico_iniciar();
  assert(!d.intro && vz.aberto && atomic_load(&vz.estado) == 1);
  assert(!diagnostico_quer_sair());
  esperar(40000);
  assert(atomic_load(&vz.estado) == 2 && vz.resultado == VR_OK);
  { SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_ESCAPE; diagnostico_evento(&e); }
  assert(!vz.aberto && diagnostico_quer_sair());
  // A abertura seguinte, pelo diagnostico, e a normal.
  diagnostico_iniciar();
  assert(!vz.aberto && !soVelocidade && !diagnostico_quer_sair());
  puts("ok  atalho abre direto no teste; Voltar do resultado sai da tela");

  diagnostico_encerrar();
  return 0;
}
