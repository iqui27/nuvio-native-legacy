// Local GL evidence for #205; synthetic sources, no network or TV required.
#include "../src/streams.c"
#include <SDL2/SDL_image.h>
#include <assert.h>

static void captura(const char *prefix, const char *state, SDL_Window *win) {
  char path[700];
  snprintf(path, sizeof path, "%s-%s.png", prefix, state);
  for (int i = 0; i < 60; i++) {
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    stream_folha_atualizar(1.0f / 60, SDL_GetTicks());
    glClearColor(.025f,.025f,.03f,1); glClear(GL_COLOR_BUFFER_BIT);
    gfx_novo_quadro();
    gfx_cor((GfxRect){0,0,1920,1080},0,.26f,.17f,.12f,1);
    gfx_cor((GfxRect){0,0,1920,360},0,.55f,.36f,.22f,1);
    stream_folha_desenhar(SDL_GetTicks());
    if (i == 59) {
      unsigned char *pixels = malloc(1920 * 1080 * 4);
      assert(pixels);
      glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
      SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);
      assert(surface);
      for (int y = 0; y < 1080; y++)
        memcpy((char *)surface->pixels + y * surface->pitch, pixels + (1079-y)*1920*4,1920*4);
      assert(IMG_SavePNG(surface,path) == 0);
      SDL_FreeSurface(surface); free(pixels);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("capture: %s\n",path);
}

int main(int argc, char **argv) {
  assert(argc == 2);
  const char *dir = getenv("NUVIO_DADOS");
  char path[700];
  snprintf(path,sizeof path,"%s/ajustes.txt",dir);
  FILE *f = fopen(path,"w"); assert(f);
  fprintf(f,"idioma 1\nselected_theme 2\nvidroLocal 0\nfonteInterface %d\n",TXT_FAMILIA_MONTSERRAT);
  fclose(f); ajustes_dir(dir);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *win = SDL_CreateWindow("Sources filter warning",SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,1920,1080,SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  SDL_GLContext gl = SDL_GL_CreateContext(win); assert(gl);
  SDL_GL_SetSwapInterval(0); glViewport(0,0,1920,1080);
  gfx_tamanho_alvo(1920,1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app",1));
  tex_iniciar(64); gfx_icones_dir("deploy/app/art"); badges_carregar("deploy/app/art");
  Stream sources[8] = {0};
  const char *providers[] = {"PlavioHD","Torrentio","Comet"};
  for (int i = 0; i < 8; i++) {
    snprintf(sources[i].provedor,sizeof sources[i].provedor,"%s",providers[i/3]);
    snprintf(sources[i].rotulo,sizeof sources[i].rotulo,"Silo S02 E05");
    snprintf(sources[i].url,sizeof sources[i].url,"https://cdn.exemplo.invalido/video/%d.mkv",i);
    sources[i].mp4 = i < 3; sources[i].altura = 1080; sources[i].fileIdx = -1;
  }
  stream_definir_alvo("tt14688458:2:5"); stream_definir_lista(sources,8);
  stream_folha_nome("Silo"); stream_folha_contexto("T2:E5 · Silo"); stream_folha_abrir();
  // Draw the sheet directly; this fixture does not drive the clock island.
  folhaRelogio = 0; soMp4 = soCache = soDub = 1; grupo = 2;
  captura(argv[1],"focused",win);
  grupo = 1; captura(argv[1],"idle",win);
  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  return 0;
}
