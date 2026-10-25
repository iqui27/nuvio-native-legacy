// Real Library opening route, Detail frames and extras parser with a silent add-on.
// Only account gates and optional TMDB HTTP responses are fixtures.
#define sessao_logada testLoggedIn
#define desc_sessao_aberta testDiscoveryOpen
#include "../src/app.c"
int testLoggedIn(void) { return 1; }
int testDiscoveryOpen(void) { return 1; }
#include "dados.h"
#include "addons.h"
#include "extras.h"
#include <assert.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdatomic.h>

static atomic_int received, movieParsed;
// Keep the real extras worker and parser; supply its optional TMDB response.
const char *detailExtrasKey(void) { return "fixture"; }
char *detailExtrasDownload(const char *url, int seconds) {
  (void)seconds;
  assert(strstr(url, "api.themoviedb.org"));
  atomic_store(&movieParsed, 1);
  return strdup("{\"id\":123,\"runtime\":100,\"credits\":{\"crew\":[{\"id\":42,\"job\":\"Novel\",\"name\":\"Test Author\"}]}}");
}
static int listener;
static void *silentAddon(void *arg) {
  (void)arg;
  int client = accept(listener, NULL, NULL);
  assert(client >= 0);
  char request[4096];
  assert(read(client, request, sizeof request) > 0);
  atomic_store(&received, 1);
  // Never send a response. Main closes the socket after exercising frames.
  while (atomic_load(&received) == 1) SDL_Delay(5);
  close(client);
  return NULL;
}
int main(void) {
  alarm(12);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Detail stall test", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(16);
  dados_iniciar("deploy/app/art"); ajustes_iniciar(); ajustes_dir(dados_dir());
  listener = socket(AF_INET, SOCK_STREAM, 0); assert(listener >= 0);
  struct sockaddr_in addr = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
  assert(bind(listener, (struct sockaddr *)&addr, sizeof addr) == 0);
  socklen_t len = sizeof addr;
  assert(getsockname(listener, (struct sockaddr *)&addr, &len) == 0);
  assert(listen(listener, 8) == 0);
  pthread_t server; assert(pthread_create(&server, NULL, silentAddon, NULL) == 0);
  AddonRemoto addon = {.ativo = 1};
  snprintf(addon.nome, sizeof addon.nome, "Silent addon");
  snprintf(addon.url, sizeof addon.url, "http://127.0.0.1:%u/manifest.json", ntohs(addr.sin_port));
  addons_definir_lista(&addon, 1);
  const char *manifest = "{\"id\":\"silent\",\"name\":\"Silent addon\",\"resources\":[\"stream\"],\"types\":[\"movie\"]}";
  assert(addons_manifesto_recebido(addons_base(0), addons_versao(), nv_perfil_sessao_ler(), manifest));
  CatItem title = {.tmdb = 123, .nElenco = 1};
  snprintf(title.imdb, sizeof title.imdb, "tt30825738");
  snprintf(title.tipo, sizeof title.tipo, "movie");
  snprintf(title.titulo, sizeof title.titulo, "Stalled Detail");
  snprintf(title.backdrop, sizeof title.backdrop, "deploy/app/art/aiostreams/logo.png");
  cat_definir_tudo(&title, 1, NULL, 0);
  tela = TELA_BIBLIOTECA;
  perfilSessaoAplicada = perfis_sessao_geracao();
  abrirPorIndice(0);
  app_atualizar(1.0f/60, SDL_GetTicks());
  Uint32 start = SDL_GetTicks();
  while (!atomic_load(&received) && SDL_GetTicks() - start < 2000) SDL_Delay(5);
  assert(atomic_load(&received));
  for (int frame = 0; frame < 120; frame++) {
    Uint32 before = SDL_GetTicks();
    app_atualizar(1.0f/60, before);
    gfx_opacidade_grupo = 0;
    detail_desenhar(before);
    cat_quadro();
    assert(addons_estado() == ADD_BUSCANDO);
    assert(SDL_GetTicks() - before < 100);
    SDL_Delay(5);
  }
  assert(atomic_load(&movieParsed));
  assert(!strcmp(extras_livro_autor(), "Test Author"));
  SDL_Event back = {.type = SDL_KEYDOWN}; back.key.keysym.sym = SDLK_ESCAPE;
  detail_evento(&back);
  for (int frame = 0; frame < 120; frame++) detail_atualizar(1.0f/60, SDL_GetTicks());
  assert(!detail_aberto());
  puts("PASS: 120 Library/Detail frames, TMDB author publication and Back while add-on never responds");
  atomic_store(&received, 2); pthread_join(server, NULL); close(listener);
  return 0;
}
