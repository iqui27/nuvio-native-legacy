// A instalacao real do Guia conclui antes do aviso; falha/repeticao nao avisam.
#include "../src/guia.c"
#include <assert.h>

static void fechar(void) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  assert(registro_evento(&e)); assert(!registro_legal_aberto());
  e.type = SDL_KEYUP; assert(registro_evento(&e));
}
int main(int argc, char **argv) {
  assert(argc == 3); assert(SDL_Init(SDL_INIT_TIMER) == 0);
  assert(setenv("NUVIO_DADOS", argv[1], 1) == 0); dados_iniciar(argv[1]);
  nRec = 2;
  snprintf(rec[0].nome, sizeof rec[0].nome, "Canal teste");
  snprintf(rec[0].url, sizeof rec[0].url, "https://canal.example/manifest.json");
  snprintf(rec[1].nome, sizeof rec[1].nome, "Outra lista");
  snprintf(rec[1].url, sizeof rec[1].url, "https://lista.example/manifest.json");
  // Segura a recarga no caminho de "worker ja em andamento": nenhuma rede.
  fioVivo = 1;
  instalar(-1); assert(!registro_legal_aberto());
  instalar(0); assert(addons_n() == 1 && addons_ativo(0));
  if (!strcmp(argv[2], "primeira")) {
    assert(registro_legal_aberto() == 1); fechar();
  } else assert(!registro_legal_aberto());
  instalar(0); assert(addons_n() == 1 && !registro_legal_aberto());
  instalar(1); assert(addons_n() == 2 && !registro_legal_aberto());
  puts("addons_aviso_legal: ok"); SDL_Quit(); return 0;
}
