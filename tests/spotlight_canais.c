// CANAIS DA LIVE TV NO SPOTLIGHT, com um guia de exemplo de verdade.
//
// Um addon Stremio FALSO (tests/spotlight_canais_servidor.py) publica tres
// canais num catalogo "tv". O caminho e o do app, sem atalho:
//
//   rede   guia_carregar -> fio do guia baixa o catalogo -> guia_atualizar
//          publica (e grava o cache). Spotlight: "espn" acha o canal, OK pede
//          SPOT_CANAL; o que app.c faz com ele (guia_item_do_canal +
//          addons_buscar com a origem do canal, ver tocarCanal) devolve a
//          fonte do addon, e a playlist dela responde.
//   cache  OUTRO processo, servidor desligado, guia nunca carregado nesta
//          sessao: o Spotlight acha o canal pela lista do cache do guia
//          (guia_preparar_busca) e o OK leva ao mesmo canal e a mesma origem.
//
// O que NAO prova: o video decodificando. O player so roda na TV (ver
// tests/livetvdiag_shot.c); aqui a prova termina na URL da fonte respondendo.
#include "spotlight.h"
#include "guia.h"
#include "addons.h"
#include "streams.h"
#include "catalogo.h"
#include "dados.h"
#include "ajustes.h"
#include "rede.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { T_CANAL = 6 };

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  spot_evento(&e);
  e.type = SDL_KEYUP;
  spot_evento(&e);
}
static void digitar(const char *s) { for (; *s; s++) tecla(*s == ' ' ? SDLK_SPACE : (SDL_Keycode)*s); }

static int achar(int tipo, const char *texto) {
  int i;
  for (i = 0; i < spot_n_linhas(); i++)
    if (spot_linha_tipo(i) == tipo && (!texto || strstr(spot_linha_texto(i), texto))) return i;
  return -1;
}

// "espn" no Spotlight, foco no canal, OK: devolve o pedido.
static SpotPedido escolherCanal(void) {
  SpotPedido p;
  int alvo, k;
  memset(&p, 0, sizeof p);
  spot_abrir(0);
  digitar("espn");
  alvo = achar(T_CANAL, "ESPN Brasil");
  printf("spotlight: canal ESPN na linha %d de %d\n", alvo, spot_n_linhas());
  assert(alvo >= 0);
  assert(achar(T_CANAL, "Globo") < 0);         // filtra pelo nome
  tecla(SDLK_DOWN);                            // barra -> lista
  for (k = 0; k < 30 && spot_linha_focada() != alvo; k++) tecla(SDLK_DOWN);
  assert(spot_linha_focada() == alvo);
  tecla(SDLK_RETURN);
  assert(spot_pediu(&p));
  assert(!spot_aberto());
  assert(p.tipo == SPOT_CANAL);
  assert(!strcmp(p.id, "teste:espn"));
  return p;
}

int main(int argc, char **argv) {
  const char *modo = argc > 1 ? argv[1] : "rede";
  const char *base = argc > 2 ? argv[2] : "http://127.0.0.1:8766";
  const char *dir = getenv("NUVIO_DADOS");
  SpotPedido p;
  CatItem it;
  if (!dir || !*dir) return 2;
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(dir);
  ajustes_iniciar();

  if (!strcmp(modo, "rede")) {
    static CatFileira fl[1];
    char man[700];
    Uint32 t0;
    int idx[4];
    // Sem lista nenhuma (nem cache, nem guia carregado): nenhum canal.
    spot_abrir(0);
    digitar("espn");
    assert(achar(T_CANAL, NULL) < 0);
    spot_fechar();

    snprintf(man, sizeof man, "%s/manifest.json", base);
    assert(addons_adicionar("Canais Teste", man) == 1);
    memset(fl, 0, sizeof fl);
    snprintf(fl[0].chave, sizeof fl[0].chave, "teste.canais");
    snprintf(fl[0].titulo, sizeof fl[0].titulo, "Canais");
    snprintf(fl[0].tipo, sizeof fl[0].tipo, "tv");
    snprintf(fl[0].base, sizeof fl[0].base, "%s", base);
    snprintf(fl[0].catId, sizeof fl[0].catId, "canais");
    cat_republicar_fileiras(fl, 1);

    guia_carregar();
    t0 = SDL_GetTicks();
    while (SDL_GetTicks() - t0 < 20000 && guia_buscar_canais("espn", idx, 4) < 1) {
      guia_atualizar(0.016f, SDL_GetTicks());
      SDL_Delay(16);
    }
    assert(guia_buscar_canais("espn", idx, 4) == 1);
    printf("guia: lista publicada em %u ms\n", SDL_GetTicks() - t0);

    p = escolherCanal();
    assert(!strcmp(p.base, base));
    // Preserve the fixture addon's declared TV type, as the guide does.
    assert(guia_item_do_canal(p.id, p.nome, p.base, &it));
    assert(!strcmp(it.imdb, "teste:espn") && !strcmp(it.tipo, "tv"));
    assert(!strcmp(guia_canal_origem(), base));
    addons_definir_origem(guia_canal_origem());
    addons_buscar(it.imdb, "tv");
    addons_definir_origem(NULL);
    t0 = SDL_GetTicks();
    while (addons_estado() == ADD_BUSCANDO && SDL_GetTicks() - t0 < 20000) SDL_Delay(10);
    assert(addons_estado() == ADD_PRONTO);
    assert(stream_n() >= 1);
    printf("fonte: %s\n", stream_item(0)->url);
    assert(strstr(stream_item(0)->url, "/ao-vivo/espn.m3u8"));
    { char *pl = rede_baixar(stream_item(0)->url, 5);
      assert(pl && strstr(pl, "#EXTM3U"));
      free(pl); }
    puts("PASS rede: guia carregado do addon, Spotlight acha ESPN, OK resolve a fonte do canal.");
    return 0;
  }

  // Cache loading keeps only configured sources. Restore the installed addon
  // identity in this fresh process without requesting its offline manifest.
  AddonRemoto installed = {0};
  snprintf(installed.nome, sizeof installed.nome, "Canais Teste");
  snprintf(installed.url, sizeof installed.url, "%s/manifest.json", base);
  installed.ativo = 1;
  assert(addons_definir_lista(&installed, 1) == 1);
  p = escolherCanal();
  assert(!strcmp(p.base, base));
  assert(guia_item_do_canal(p.id, p.nome, p.base, &it));
  assert(!strcmp(it.imdb, "teste:espn") && !strcmp(it.tipo, "tv"));
  assert(!strcmp(guia_canal_origem(), base));
  puts("PASS cache: sem rede e sem abrir o guia, Spotlight acha ESPN pela lista do cache.");
  return 0;
}
