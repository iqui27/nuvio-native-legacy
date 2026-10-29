if (getenv("NUVIO_CHIPFONTE2_PROOF")) {
  abrir(1,0,0,0);
  Stream source=chipfonte2_source();
  stream_definir_alvo(itens[1].imdb); stream_definir_lista(&source,1);
  vt_montar(&vaiTocar,itens[1].imdb,0,0);
  vaiTocarIdx=idx; vaiTocarEm=SDL_GetTicks();
  hVaiVis=56+DET_VAI_GAP;
  int check=!getenv("NUVIO_CHIPFONTE2_BEFORE");
  for (int state=0;state<2;state++) for (int focused=0;focused<2;focused++) {
    source.conferencia=state ? FA_CONFERINDO : FA_OK;
    stream_definir_lista(&source,1);
    vt_montar(&vaiTocar,itens[1].imdb,0,0);
    focoVersao=focused; versaoFoco=focused; botao=0; nivel=0;
    chipDotCount=0; chipSourceText[0]=0;
    quadros(2);
    char name[100]; snprintf(name,sizeof name,"%s-%s-%s",check ? "after" : "before",
      state ? "checking" : "ready",focused ? "focused" : "unfocused");
    gravar(name);
    if (check) {
      if (!chipDotCount || strcmp(chipSourceText,"AIOStreams Stable · 18.4 GB")) {
        fprintf(stderr,"FAIL: resolved AIOStreams renderer: dots=%d label='%s'\n",chipDotCount,chipSourceText);
        return 1;
      }
      assert(chipDotState==source.conferencia);
      assert(chipDotX>=versaoRect.x && chipDotX+10<=chipSourceX);
      assert(chipDotY>=versaoRect.y && chipDotY+10<=versaoRect.y+versaoRect.h);
      assert(chipSourceX+chipSourceLine.w<=versaoRect.x+versaoRect.w);
      assert(chipQualityX+chipQualityLine.w<versaoRect.x);
      // SDL_ttf aligns lines by their font boxes; match their bottom baseline.
      assert(fabsf(chipQualityY+chipQualityLine.h-chipSourceY-chipSourceLine.h)<1);
      int size,weight; txt_estilo_medidas(chipQualityStyle,&size,&weight);
      assert(size<=16 && weight==0 && chipQualityInk<=170);
      int stops=0;
      for (int i=0;i<navigationCount;i++) {
        GfxRect r=navigation[i].rect;
        if (navigation[i].row==-4) {
          stops++; assert(fabsf(r.x-versaoRect.x)<1 && fabsf(r.w-versaoRect.w)<1);
        }
        assert(!(chipQualityX>=r.x && chipQualityX<r.x+r.w && chipQualityY>=r.y && chipQualityY<r.y+r.h));
      }
      assert(stops==1);
    }
  }
  if (check) {
    SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_UP; detail_evento(&e);
    assert(!focoVersao && nivel==0 && botao==0);
    e.key.keysym.sym=SDLK_DOWN; detail_evento(&e); assert(focoVersao && nivel==0);
    e.key.keysym.sym=SDLK_RETURN; detail_evento(&e);
    e.type=SDL_KEYUP; detail_evento(&e);
    assert(detail_pediu_fontes() && !detail_pediu_reproduzir());
    e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_DOWN; detail_evento(&e);
    assert(!focoVersao && nivel==1);
    puts("PASS: parsed AIOStreams dot/size rendered; quality informational; Watch Down selects source; OK requests Sources");
  }
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(janela); SDL_Quit();
  return 0;
}
