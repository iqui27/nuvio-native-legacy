if (getenv("NUVIO_CHIPFONTE2_PROOF")) {
  abrir(1,0,0,0);
  Stream source=chipfonte2_source();
  stream_definir_alvo(itens[1].imdb);
  hVaiVis=56+DET_VAI_GAP;
  int check=!getenv("NUVIO_CHIPFONTE2_BEFORE");
  static const int states[]={FA_OK,FA_CONFERINDO,FA_RUIM};
  static const char *names[]={"ready","checking","failed"};
  for (int state=0;state<3;state++) for (int focused=0;focused<2;focused++) {
    source.conferencia=states[state];
    stream_definir_lista(&source,1);
    vt_montar(&vaiTocar,itens[1].imdb,0,0);
    vaiTocarIdx=idx; vaiTocarEm=SDL_GetTicks();
    focoVersao=focused; versaoFoco=focused; botao=0; nivel=0;
    chipDotCount=chipSourceDraws=chipQualityDraws=0; chipQualityText[0]=0;
    quadros(2);
    char name[100]; snprintf(name,sizeof name,"%s-%s-%s",check ? "after" : "before",
      names[state],focused ? "focused" : "unfocused");
    gravar(name);
    if (check) {
      const char *expected=state==0 ? "4K · Dolby Vision · Atmos 5.1 · 18.4 GB" : "4K · Dolby Vision · Atmos 5.1";
      if (!chipDotCount || chipSourceDraws || strcmp(chipQualityText,expected)) {
        fprintf(stderr,"FAIL: single quality chip: dots=%d source-draws=%d label='%s'\n",chipDotCount,chipSourceDraws,chipQualityText);
        return 1;
      }
      assert(chipDotCount==2 && chipQualityDraws==2);
      assert(chipDotState==states[state]);
      assert(chipDotX>=versaoRect.x && chipDotX+10<=chipQualityX);
      assert(chipDotY>=versaoRect.y && chipDotY+10<=versaoRect.y+versaoRect.h);
      assert(chipQualityX+chipQualityLine.w<=versaoRect.x+versaoRect.w);
      assert(chipQualityY>=versaoRect.y && chipQualityY+chipQualityLine.h<=versaoRect.y+versaoRect.h);
      int size,weight; txt_estilo_medidas(chipQualityStyle,&size,&weight);
      assert(size<=16 && weight==0 && chipQualityInk<=200);
      int stops=0;
      for (int i=0;i<navigationCount;i++) if (navigation[i].row==-4) {
        GfxRect r=navigation[i].rect;
        stops++; assert(fabsf(r.x-versaoRect.x)<1 && fabsf(r.w-versaoRect.w)<1);
        assert(chipQualityX>=r.x && chipQualityX+chipQualityLine.w<=r.x+r.w);
      }
      assert(stops==1);
      SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_UP; detail_evento(&e);
      assert(!focoVersao && nivel==0 && botao==0);
      e.key.keysym.sym=SDLK_DOWN; detail_evento(&e); assert(focoVersao && nivel==0);
      e.key.keysym.sym=SDLK_RETURN; detail_evento(&e);
      e.type=SDL_KEYUP; detail_evento(&e);
      assert(detail_pediu_fontes() && !detail_pediu_reproduzir());
    }
  }
  if (check) {
    // Empty sources must retain the same focus stop and Sources action.
    stream_definir_lista(NULL,0);
    memset(&vaiTocar,0,sizeof vaiTocar); vaiTocarEm=0;
    chipDotCount=chipQualityDraws=0; chipQualityText[0]=0;
    quadros(2);
    assert(chipDotCount==2 && chipDotState==FA_RUIM && chipQualityDraws==2);
    assert(!strcmp(chipQualityText,"Sources"));
    SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_UP; detail_evento(&e);
    e.key.keysym.sym=SDLK_DOWN; detail_evento(&e); assert(focoVersao && nivel==0);
    e.key.keysym.sym=SDLK_RETURN; detail_evento(&e);
    e.type=SDL_KEYUP; detail_evento(&e); assert(detail_pediu_fontes());
    e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_DOWN; detail_evento(&e);
    assert(!focoVersao && nivel==1);
    puts("PASS: single caption chip contains status/quality/ready size; no addon name; Watch Down selects chip; OK requests Sources in every state, including empty");
  }
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(janela); SDL_Quit();
  return 0;
}
