// Item, primeira adicao real pelo teclado e marca local entre processos.
#include "../src/ajustes.c"
#include <assert.h>
#include <unistd.h>
void teclado_teste_texto(const char *t);
void teclado_teste_foco(int f, int c);

static void key(SDL_Keycode k) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  ajustes_evento(&e);
}
static void fechar(void) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  assert(registro_evento(&e)); assert(!registro_legal_aberto());
  e.type = SDL_KEYUP; assert(registro_evento(&e));
}
int main(int argc, char **argv) {
  char *s; int i, item = -1;
  assert(argc == 3);
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  assert(setenv("NUVIO_DADOS", argv[1], 1) == 0);
  dados_iniciar(argv[1]); snprintf(dirAjustes, sizeof dirAjustes, "%s", argv[1]);
  ajustes_iniciar();
  for (i = 0; i < AJ_N_TELA; i++)
    if (TELA[i].tipo == IT_OPC && TELA[i].op == AJ_AVISO_LEGAL) item = i;
  assert(item >= 0 && visivel(item) && focavel(item));
  assert(!strcmp(uxCaminho(AJ_AVISO_LEGAL), "Sobre e ajuda"));
  // O mesmo OK gerado pelo clique na linha do Magic Remote.
  ajPontLista = 1; ajPonteiroItem(item, 0); assert(focoOp == AJ_AVISO_LEGAL); key(SDLK_RETURN);
  assert(registro_legal_aberto() == 2); fechar();
  if (!strcmp(argv[2], "primeira")) assert(!dados_ler("aviso-legal.txt")); // leitura nao marca aviso
  // Gravar Xtream via teclado: a operacao termina antes do aviso informativo.
  ajIptvNovaAbrir(AJ_IPTV_ADD_XTREAM);
  assert(teclado_aberto()); teclado_teste_texto("https://tv.example");
  teclado_teste_foco(0, 0);
  { SDL_Event e = {0}; e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_DOWN; for (i = 0; i < 20; i++) teclado_evento(&e);
    e.key.keysym.sym = SDLK_RIGHT; teclado_evento(&e); teclado_evento(&e);
    e.key.keysym.sym = SDLK_RETURN; teclado_evento(&e); }
  assert(!teclado_aberto());
  ajustes_atualizar(0.016f, SDL_GetTicks());
  assert(iptvForm.nova && !strcmp(iptvForm.servidor, "https://tv.example"));
  snprintf(iptvForm.usuario, sizeof iptvForm.usuario, "fixture-user");
  snprintf(iptvForm.senha, sizeof iptvForm.senha, "fixture-password");
  ajIptvSalvar();
  ajustes_atualizar(0.016f, SDL_GetTicks());
  if (!strcmp(argv[2], "primeira")) {
    assert(registro_legal_aberto() == 1);
    s = dados_ler("aviso-legal.txt"); assert(s && !strcmp(s, "1\n")); free(s);
    assert(strstr(xtream_servidor_curto(), "tv.example"));
    fechar();
  } else assert(!registro_legal_aberto());
  registro_aviso_legal(); assert(!registro_legal_aberto());
  SDL_Quit(); puts("ajustes_aviso_legal: ok"); return 0;
}
