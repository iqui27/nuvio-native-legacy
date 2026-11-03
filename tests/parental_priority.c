// Offline temporal regression for the opening content guide, not PIN/lock UI.
// Include the real state machines to assert selection and deadlines directly.
#include "../src/player.c"
#include "../src/plrilha.c"
#include <SDL2/SDL_image.h>
#include <assert.h>

static Uint32 now = 100000;
static const char *labels[] = { "Violence", "Language", "Frightening scenes", "Alcohol" };
static const char *severity[] = { "Moderate", "Mild", "Severe", "Mild" };

static void opening(void) {
  aberto = 1; saindo = pediuSair = 0;
  temFixo = 1; memset(&itemFixo, 0, sizeof itemFixo);
  snprintf(itemFixo.tipo, sizeof itemFixo.tipo, "movie");
  snprintf(itemFixo.classificacao, sizeof itemFixo.classificacao, "12");
  inicioImagem = now; pgDesde = pgQuadro = 0;
  toastAte = durFonteAcaoAte = 0; toastTexto[0] = 0;
  entrada = 1; anim = 0; visivel = skipFoco = 0;
  esperandoFonte = erroFonte = 0;
  posSeg = posVis = 30; duracaoSeg = 7200; tocando = 1;
  temPed = temUlt = baixaVisivel = prevVolta = 0;
  W = H = A = corpoA = vW = vH = 0;
  player_shot_video(1);
  parental_shot(labels, severity, 4);
}

static void frame(void) {
  now += 20;
  txt_novo_quadro();
  player_desenhar(now);
  plrilha_desenhar(now);
}

static void notice(int critical, unsigned ms) {
  player_shot_toast(now, critical ? "Offline critical warning" : "Offline ordinary notice",
                    critical ? "aj_triangle-alert" : "aj_info", critical, 0);
  toastAte = now + ms;
}

static void back(void) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE;
  player_evento(&e);
}

int main(void) {
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Offline parental timing", 0, 0, 64, 64,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window);
  SDL_GLContext context = SDL_GL_CreateContext(window); assert(context);
  gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  ajustes_dir(getenv("NUVIO_DADOS")); ajustes_iniciar();
  ajustes_shot_valor("classifPlayerLocal", 0);
  VideoSimulacao video = {0};
  video.largura = 1920; video.altura = 1080; video.pronto = 1;
  video.duracao = 7200; video.pos = 30;
  video_simular(&video);

  // Ordinary requests cannot replace the guide in either submission order.
  opening();
  PlrIlhaPedido ordinary = {0}; ordinary.texto = "Ordinary"; ordinary.aberta = 1;
  plrilha_pedir(&ordinary); plrguia_pedir(0, 0, "12");
  assert(temPed && ped.baixa);
  temPed = 0;
  plrguia_pedir(0, 0, "12"); plrilha_pedir(&ordinary);
  assert(temPed && ped.baixa);
  temPed = 0;

  // Six seconds remain the guide's window. The existing toast slot waits,
  // then receives its original four seconds instead of expiring underneath.
  frame(); assert(plrilha_baixa_visivel());
  Uint32 opened = pgDesde;
  while (now < opened + 1000) { frame(); assert(plrilha_baixa_visivel()); }
  notice(0, 4000);
  while (now < opened + 5980) { frame(); assert(plrilha_baixa_visivel()); }
  frame(); assert(!plrilha_baixa_visivel());
  assert(temUlt && ult.baixa); // retained only for the closing animation
  assert(toastAte >= now + 3980);
  while (now + 20 < toastAte) { frame(); assert(!plrilha_baixa_visivel()); }
  frame(); assert(!plrilha_baixa_visivel());
  assert(toastAte <= now);

  // A warning outlives the original guide deadline and still wins immediately.
  // The unread part of the guide returns after the warning's existing window.
  opening(); frame(); opened = pgDesde;
  while (now < opened + 1000) frame();
  notice(1, 7000); Uint32 criticalEnd = toastAte;
  frame(); assert(!plrilha_baixa_visivel());
  while (now + 20 < criticalEnd) { frame(); assert(!plrilha_baixa_visivel()); }
  frame(); assert(plrilha_baixa_visivel());
  assert(toastAte == criticalEnd);
  assert(now - pgDesde == 1020); // the last guide frame precedes the warning by 20 ms
  while (now - pgDesde < 5980) { frame(); assert(plrilha_baixa_visivel()); }
  frame(); assert(!plrilha_baixa_visivel());

  // Interactive panels and the existing source-error panel retain precedence.
  opening(); frame();
  PlrIlhaPedido panel = {0}; panel.w = 680; panel.h = 200; panel.modal = 1;
  plrilha_pedir(&panel); plrguia_pedir(0, 0, "12"); assert(!ped.baixa);
  temPed = 0;
  plrguia_pedir(0, 0, "12"); panel.corIcone = 1;
  plrilha_pedir(&panel); assert(!ped.baixa && ped.corIcone == 1);
  temPed = 0;
  erroFonte = 1;
  now += 20; player_desenhar(now);
  assert(temPed && ped.corpo == corpoErro && !ped.baixa);
  plrilha_desenhar(now); assert(!plrilha_baixa_visivel());

  // A covering layer also pauses the guide, even before its first visible frame.
  opening();
  for (int i = 0; i < 100; i++) {
    now += 20; player_desenhar(now); plrilha_esconder(); plrilha_desenhar(now);
    assert(!plrilha_baixa_visivel() && now == pgDesde);
  }
  frame(); assert(plrilha_baixa_visivel() && now == pgDesde);

  // Late data still respects the existing 45-second opening admission limit.
  opening(); parental_shot(NULL, NULL, 0); frame();
  now += 46000; parental_shot(labels, severity, 4); frame();
  assert(!pgDesde && !plrilha_baixa_visivel());

  // Back keeps its current one-layer semantics: hide controls, then exit.
  // Explicit close also stops guide requests during the player's exit fade.
  opening(); frame(); notice(0, 4000); visivel = 1; anim = 1;
  back(); assert(!saindo && !visivel);
  anim = 0; back(); assert(saindo && pediuSair);
  frame(); assert(!plrilha_baixa_visivel() && toastAte > now);
  opening(); frame(); notice(0, 4000); player_sair();
  frame(); assert(saindo && !plrilha_baixa_visivel());

  parental_shot(NULL, NULL, 0);
  SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
  puts("PASS: parental guide timing, ordinary deferral, critical priority, panels and Back/close");
  return 0;
}
