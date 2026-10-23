// O TESTE DE VELOCIDADE DE PONTA A PONTA contra o servidor local de
// tests/vazao.sh: dois addons (um responde fontes, o outro 404), a escolha das
// candidatas (torrent sem link, link de aviso e fonte fora de cache ficam de
// fora), hosts diferentes (127.0.0.1 e localhost), o resultado, o relatorio
// SEM url nem host, e o Voltar no meio.
//
// Inclui o .c para chegar no estado static (mesma tecnica de diagnostico_fluxo).
#include "../src/diagnostico.c"
#include "nvtmp.h"
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
  char dir[NVT_MAX], url[200]; nvt_set(dir, sizeof dir, "/nuvio-vazao-fluxo-XXXXXX");
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
  assert(vz.addon[0].medido && vz.addon[0].ok && vz.addon[0].fontes == 7);
  assert(vz.addon[1].medido && !vz.addon[1].ok && vz.addon[1].http == 404);
  // 7 fontes: 4 mediveis (4K, 1080p em localhost, 720p, 480p), torrent, aviso e
  // fora de cache de fora. O rapido guarda as 3 primeiras da ordem do
  // automatico; as do debrid ficam so para serem listadas (sem link).
  assert(vz.addon[0].candidatas == 3 && vz.addon[0].mediveis == 4);
  assert(candidatasMediveis() == 3 && vz.mediveisTotal == 4 && vz.debridTotal == 2);
  { int k;
    for (k = 0; k < vz.nCand; k++) {
      if (!vz.cand[k].medivel) { assert(!vz.cand[k].url); continue; }
      assert(!strstr(vz.cand[k].url, "slate") && !strstr(vz.cand[k].url, "d.mkv") &&
             !strstr(vz.cand[k].url, "e.mkv"));
    }
    assert(!vz.falhas[VR_AVISO]); }
  assert(vz.resultado == VR_OK);
  // Hosts diferentes: 127.0.0.1 e localhost; o 720p (mesmo host do 4K) nao.
  assert(atomic_load(&vz.nFonte) == 2);
  assert(vz.resumo.medianaKbps > 6000 && vz.resumo.medianaKbps < 10000);
  assert(vz.resumo.otimoKbps < vz.resumo.maximoKbps);
  puts("ok  addons medidos, candidatas filtradas, 2 hosts, ~8 Mbps");

  // Mais de 3 fontes de link direto: o ciclo completo aparece; com um add-on
  // so com fonte, o "por add-on" nao.
  { int lista[VB_N];
    assert(vazBotoes(lista) == 2 && lista[0] == VB_RAPIDO && lista[1] == VB_CICLO); }
  puts("ok  botao Ciclo completo com mais de 3 fontes; Por add-on so com 2 add-ons");

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

  // 5. CICLO COMPLETO pelo botao: direita foca "Ciclo completo", OK inicia.
  diagnostico_abrir_velocidade();
  diagnostico_iniciar();
  esperar(40000);
  assert(atomic_load(&vz.estado) == 2 && vz.ciclo == VCM_RAPIDO);
  { SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_RIGHT; diagnostico_evento(&e);
    e.key.keysym.sym = SDLK_RETURN; diagnostico_evento(&e); }
  assert(atomic_load(&vz.estado) == 1 && vz.ciclo == VCM_COMPLETO);
  esperar(60000);
  assert(atomic_load(&vz.estado) == 2 && vz.resultado == VR_OK);
  // As 4 mediveis, TODAS medidas (nada de host distinto), e as 2 do debrid so
  // listadas.
  assert(vz.nFila == 4 && vz.agenda.medidas == 4 && vz.agenda.tentadas == 4);
  assert(vz.sel.debrid == 2 && vz.sel.foraDoLimite == 0);
  assert(atomic_load(&vz.nCic) == 6);
  { int k, ok = 0, debrid = 0, mesmoHost = 0;
    for (k = 0; k < 6; k++) {
      const VazCicloRes *x = &vz.cic[k];
      if (x->sit == VS_OK) {
        ok++;
        assert(x->r.medianaKbps > 4000 && x->r.medianaKbps < 12000);
        assert(x->host[0] && !strchr(x->host, '/') && !strstr(x->host, "http"));
        assert(x->necessarioKbps > 0 && x->suf != VSU_SEM_REF);
        if (!strncmp(x->host, "127.0.0.1", 9)) mesmoHost++;
      } else if (x->sit == VS_DEBRID) {
        debrid++;
        assert(!x->host[0] && !x->r.medianaKbps);
      }
    }
    assert(ok == 4 && debrid == 2 && mesmoHost == 3); }   // 3 no 127.0.0.1: nenhum foi pulado por host
  puts("ok  ciclo completo: 4 fontes medidas uma a uma, 2 do debrid listadas sem teste");

  // O relatorio leva os numeros e o HOST publico, e nem rastro do link.
  atomic_store(&d.estado, 2);
  montarRelatorio();
  assert(strstr(d.relatorio, "vazao_ciclo=completo\n"));
  assert(strstr(d.relatorio, "vazao_ciclo_fonte=4|") && strstr(d.relatorio, "vazao_ciclo_fonte=6|"));
  assert(strstr(d.relatorio, "|situacao=debrid"));
  { char host[48];
    snprintf(host, sizeof host, "host=127.0.0.1:%d|", porta);
    assert(strstr(d.relatorio, host)); }
  assert(strstr(d.relatorio, "vazao_ciclo_addon=1|"));
  assert(!strstr(d.relatorio, "CHAVE-SECRETA") && !strstr(d.relatorio, "token=") &&
         !strstr(d.relatorio, "/lento") && !strstr(d.relatorio, "http:") && !strstr(d.relatorio, "https:") &&
         !strstr(d.relatorio, "Local") && !strstr(d.relatorio, "e.mkv"));
  puts("ok  relatorio do ciclo: host e numeros, sem link, chave, token nem nome");
  atomic_store(&d.estado, 0);

  // 6. POR ADD-ON: uma fonte por add-on com fonte (so o "Local"), a melhor
  // (a 4K); Voltar fecha.
  { SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_ESCAPE; diagnostico_evento(&e); }
  iniciarVazaoModo(VCM_ADDON);
  esperar(60000);
  assert(atomic_load(&vz.estado) == 2 && vz.ciclo == VCM_ADDON && vz.nFila == 1);
  assert(vz.cic[0].sit == VS_OK && vz.cic[0].altura == 2160 && vz.cic[0].suf == VSU_NAO);
  puts("ok  por add-on: a melhor fonte de cada add-on (a 4K), veredito de 25 Mbps");

  // 7. Voltar no meio do ciclo cancela sem esperar a fila; o parcial fica.
  iniciarVazaoModo(VCM_COMPLETO);
  { SDL_Event e;
    Uint32 t1;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN;
    while (atomic_load(&vz.fase) < 2 && atomic_load(&vz.estado) == 1) SDL_Delay(20);
    SDL_Delay(1400);
    t1 = SDL_GetTicks();
    e.key.keysym.sym = SDLK_ESCAPE; diagnostico_evento(&e);
    esperar(15000);
    assert(atomic_load(&vz.estado) == 3 && vz.resultado == VR_CANCELADO);
    assert(SDL_GetTicks() - t1 < 3000);
    assert(atomic_load(&vz.nCic) < 4 && vz.agenda.cancelado); }
  puts("ok  Voltar cancela o ciclo no meio; as fontes seguintes nem comecam");

  diagnostico_encerrar();
  return 0;
}
