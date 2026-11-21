// Render the real row, then deliver actual normalized SDL finger events through
// the real pointer dispatcher. An empty activity feed must not make a friend untappable.
static int toqueColuna, toqueFocou, toqueRetornos;
static void amigosToqueFocar(int coluna,int cartao) {
  toqueColuna=coluna;toqueFocou++;amigosfil_focar(coluna,cartao);
}
static void amigosToqueEntregar(const SDL_Event *e) {
  if(e->type==SDL_KEYUP && e->key.keysym.sym==SDLK_RETURN){toqueRetornos++;amigosfil_ok(toqueColuna);}
}
static int amigosToqueAuditar(void) {
  int falhas=0, op=ajustes_rapido_op("friendsRowStyleLocal");assert(op>=0);
  if(!mtx)mtx=SDL_CreateMutex();assert(mtx);
  layout_modo_definir(1);
  socialvis_definir_feed(NULL,0);
  assert(socialvis_n_amigos()==3);
  for(int i=0;i<3;i++)assert(!socialvis_amigo(i)->nTit);
  ponteiro_iniciar();ponteiro_teste_toque(1);amigosfil_ponteiro(amigosToqueFocar);
  // Cold start in the default layout is essential: Classic used to be the
  // only style that initialized the shared hit rectangle.
  for(int style=0;style<6;style++){
    if(style)assert(ajustes_rapido_passo(op,1));
    assert(ajustes_amigos_estilo()==style);
    for(int portrait=0;portrait<2;portrait++){
      int w=portrait?1080:1920,h=portrait?2340:1080;
      SDL_SetWindowSize(janela,w,h);glViewport(0,0,w,h);gfx_tamanho_alvo(w,h);layout_tela_definir(w,h);ponteiro_teste_janela(w,h);
      tex_novo_quadro();txt_novo_quadro();gfx_novo_quadro();gfx_sem_recorte();
      ponteiro_quadro(SDL_GetTicks());
      amigosfil_desenhar(72,220,400,200,0,0,SDL_GetTicks());ponteiro_desenhar();
      const PonteiroAlvo *lista=NULL;int n=ponteiro_teste_lista(&lista),hit=-1;
      for(int i=0;i<n;i++)if(lista[i].focar==amigosToqueFocar){hit=i;break;}
      if(hit<0){printf("FAIL: cold friend style=%d portrait=%d has no touch target\n",style,portrait);falhas++;continue;}
      const PonteiroAlvo alvo=lista[hit];assert(alvo.w>0 && alvo.h>0);
      float x=alvo.x+alvo.w*.5f,y=alvo.y+alvo.h*.5f;
      toqueFocou=toqueRetornos=0;
      SDL_Event e={0};e.type=SDL_FINGERDOWN;e.tfinger.touchId=3;e.tfinger.fingerId=7;e.tfinger.x=x/w;e.tfinger.y=y/h;
      assert(ponteiro_evento(&e,amigosToqueEntregar));e.type=SDL_FINGERUP;assert(ponteiro_evento(&e,amigosToqueEntregar));
      char id[96]="";
      assert(toqueFocou==1 && toqueRetornos==1 && amigosfil_pediu_perfil(id,sizeof id));
      assert(!strcmp(id,socialvis_amigo(0)->id));
      assert(!amigosfil_pediu_perfil(id,sizeof id));
      printf("PASS: empty friend style=%d portrait=%d opens the selected profile once\n",style,portrait);
      if(style==0 && portrait){
        GfxRect before,after;assert(amigosfil_teste_cartao(0,&before)>=0);
        PonteiroRolagem drag={.fase=PONT_ROL_INICIO,.eixoY=0,.x=x,.y=y};
        assert(amigosfil_rolagem(&drag));drag.fase=PONT_ROL_MOVER;drag.delta=-120;assert(amigosfil_rolagem(&drag));
        int col=0;for(int frame=0;frame<30;frame++)amigosfil_atualizar(1.f/60,1,&col);
        amigosfil_desenhar(72,220,400,200,1,col,SDL_GetTicks());assert(amigosfil_teste_cartao(0,&after)>=0);
        assert(before.x-after.x>100);
        amigosfil_retomar_foco(&col);assert(col==1);
        puts("PASS: friend-row swipe keeps its position and remote focus returns to the nearest visible card");
      }
    }
  }
  contato(nContatos++,"nuvio:qa-amigo-quatro","Quarto","","nuvio");
  contato(nContatos++,"nuvio:qa-amigo-cinco","Quinto","","nuvio");
  socialvis_definir_feed(NULL,0);
  assert(socialvis_n_amigos()==5);
  for(int style=0;style<6;style++){
    while(ajustes_amigos_estilo()!=style)assert(ajustes_rapido_passo(op,1));
    for(int portrait=0;portrait<2;portrait++){
      int w=portrait?1080:1920,h=portrait?2340:1080;
      SDL_SetWindowSize(janela,w,h);glViewport(0,0,w,h);gfx_tamanho_alvo(w,h);layout_tela_definir(w,h);ponteiro_teste_janela(w,h);
      for(int frame=0;frame<2;frame++){
        tex_novo_quadro();txt_novo_quadro();gfx_novo_quadro();gfx_sem_recorte();ponteiro_quadro(SDL_GetTicks());
        amigosfil_desenhar(72,220,112,200,0,0,SDL_GetTicks());ponteiro_desenhar();
        if(!frame){
          PonteiroRolagem drag={.fase=PONT_ROL_INICIO,.eixoY=0,.x=260,.y=276};
          int started=amigosfil_rolagem(&drag);
          if(style==3 && portrait)assert(started);
          if(started){
            drag.fase=PONT_ROL_MOVER;drag.delta=100000;amigosfil_rolagem(&drag);
            drag.delta=-120;assert(amigosfil_rolagem(&drag));
            drag.fase=PONT_ROL_FIM;assert(amigosfil_rolagem(&drag));
          }
        }
      }
      const PonteiroAlvo *lista=NULL;int n=ponteiro_teste_lista(&lista);
      float left=style==3?204:72;
      for(int i=0;i<n;i++)if(lista[i].focar==amigosToqueFocar && lista[i].x<left){
        printf("FAIL: clipped friend style=%d portrait=%d target x=%.0f clip=%.0f\n",style,portrait,lista[i].x,left);falhas++;
      }
      if(style==3 && portrait){
        GfxRect card;assert(amigosfil_teste_cartao(0,&card)>=0);
        assert(fabsf(card.x-96)<1 && fabsf(card.x+card.w-192)<1);
        toqueFocou=toqueRetornos=0;
        SDL_Event e={0};e.type=SDL_FINGERDOWN;e.tfinger.touchId=3;e.tfinger.fingerId=7;e.tfinger.x=160.f/w;e.tfinger.y=276.f/h;
        ponteiro_evento(&e,amigosToqueEntregar);e.type=SDL_FINGERUP;ponteiro_evento(&e,amigosToqueEntregar);
        char id[96]="";int abriu=amigosfil_pediu_perfil(id,sizeof id);
        if(toqueFocou || toqueRetornos || abriu){printf("FAIL: hidden friend activated focus=%d OK=%d profile=%s\n",toqueFocou,toqueRetornos,id);falhas++;}
        else puts("PASS: header tap cannot focus or open the hidden friend after a 120-unit swipe");
      }
      printf("CHECK: clipped targets style=%d portrait=%d\n",style,portrait);
    }
  }
  fflush(stdout);return falhas?1:0;
}
