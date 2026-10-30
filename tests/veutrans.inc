// Real Home/Detail rendering, fixed 60 Hz clock, local artwork, no animation cuts.
// Included by the existing carousel capture harness.
static void veuFrame(Uint32 now, const char *path) {
  SDL_PumpEvents();
  tex_bombear(32);
  veuNow = now;
  detail_atualizar(1.0f / 60.0f, now);
  txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
  gfx_ambiente_preparar();
  glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  gfx_ambiente(1);
  if (!detail_cobre_tela()) home_desenhar(now);
  // Isolate the veil/backdrop from text, focus and loading indicators.
  detalheFundo(suave(t));
  // Production content flushes a deferred Frost/ambient pass after the backdrop.
  gfx_ambiente_descarregar();
  if (path) {
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);
    unsigned char *pixels = malloc(1920*1080*4);
    assert(surface && pixels);
    glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    for (int y=0;y<1080;y++) memcpy((char *)surface->pixels+y*surface->pitch,pixels+(1079-y)*1920*4,1920*4);
    assert(IMG_SavePNG(surface,path)==0);
    SDL_FreeSurface(surface); free(pixels);
  }
  SDL_GL_SwapWindow(janela);
}

static void veuTrans(const char *prefix) {
  int layout = atoi(getenv("NV_VEU_TRANS"));
  int glass = getenv("NUVIO_SHOT_VIDRO") && atoi(getenv("NUVIO_SHOT_VIDRO"));
  ajusta(layout, glass);
  ajustes_definir_vidro(glass);
  assert(ajustes_home_layout()==layout && ajustes_vidro()==glass);
  quadros(90,NULL);
  for (int i=0;i<14;i++) tecla(SDLK_UP);
  quadros(60,NULL);
  for (int i=0;i<3;i++) { tecla(SDLK_DOWN); quadros(40,NULL); }
  tecla(SDLK_RIGHT);
  quadros(90,NULL);
  HomeItem it;
  assert(home_item_focado(&it));
  // Warm the real title backdrop before measuring the shader hand-off.
  const char *art=home_backdrop(it.indice);
  for (int i=0;i<120;i++) { if (art) tex_obter_hero(art); tex_bombear(32); SDL_Delay(4); }
  detail_abrir(&it);
  assert(carro == (layout==HOME_LAYOUT_DINAMICA));
  if (carro) {
    for (int i=0;i<120;i++) veuFrame(10000+i*17,NULL);
    carCheia=1;
  }
  char path[1024], csv[1024];
  if (getenv("NV_VEU_LATE_ONLY")) goto late_art;
  snprintf(csv,sizeof csv,"%s-frames.csv",prefix);
  FILE *log=fopen(csv,"w"); assert(log);
  fputs("frame,t,card,window,glass,background\n",log);
  for (int i=0;i<100;i++) {
    snprintf(path,sizeof path,"%s-frame-%03d.png",prefix,i);
    veuFrame(13000+i*17,path);
    fprintf(log,"%d,%.8f,%.8f,%d,%d,%d\n",i,t,cartao,carDesenhaFundo(),ajustes_vidro(),fundo_modo());
  }
  fclose(log);
  // Adjacent states straddling the real renderer boundary; identical clock,
  // texture, scroll and ambient values. Only subpixel geometry may differ.
  if (carro) {
    t=1; pg=scrollY=0; cartaoVel=0;
    for (int i=0;i<2;i++) {
      cartao=i ? .0019f : .0021f;
      // Render without stepping the spring past the chosen boundary.
      txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro(); gfx_ambiente_preparar();
      glClearColor(NV_COR_FUNDO_R,NV_COR_FUNDO_G,NV_COR_FUNDO_B,1); glClear(GL_COLOR_BUFFER_BIT);
      gfx_ambiente(1); detalheFundo(1);
      snprintf(path,sizeof path,"%s-endpoint-%d.bmp",prefix,i); gravar(path);
      SDL_GL_SwapWindow(janela);
    }
  }
late_art:
  // Texture arriving after the zoom: hold the same tinted base at the
  // renderer boundary, then sample every frame of the 220 ms reveal.
  memset(&revFundo,0,sizeof revFundo);
  veuMissing=1;
  t=1; pg=scrollY=0; cartao=0;
  for (int i=0;i<24;i++) {
    if (i==4) veuMissing=0;
    snprintf(path,sizeof path,"%s-late-%03d.png",prefix,i);
    veuFrame(17000+i*17,path);
  }
  veuMissing=0;
  printf("veil capture: layout=%d glass=%d background=%d frames=100 late=24\n",layout,glass,fundo_modo());
}
