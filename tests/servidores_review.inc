// Observe production text and pointer targets; no network or substitute renderer.
static char reviewText[16384];
static void reviewRecord(const char *s) {
  size_t n = strlen(reviewText);
  snprintf(reviewText+n, sizeof reviewText-n, "%s\n", s);
}
static TxtLinha reviewLine(TxtEstilo style, const char *s, int r, int g, int b, int a) {
  reviewRecord(s); return txt_linha(style,s,r,g,b,a);
}
static TxtLinha reviewCut(TxtEstilo style, const char *s, int r, int g, int b, int a, float width) {
  reviewRecord(s); return txt_linha_corta(style,s,r,g,b,a,width);
}
static int reviewErrors;
#define REVIEW(condition, name) do { if (!(condition)) { fprintf(stderr,"FAIL: %s\n",name); reviewErrors++; } } while (0)
static void reviewDraw(void) {
  reviewText[0]=0; entry=page=1; pageSeconds=2;
  txt_novo_quadro(); gfx_novo_quadro();
  glBindFramebuffer(GL_FRAMEBUFFER,target); glViewport(0,0,1920,1080); glClear(GL_COLOR_BUFFER_BIT);
  ponteiro_camada(); srvsetup_desenhar(shotTime); ponteiro_desenhar();
}
static void reviewClick(float x, float y) {
  const PonteiroAlvo *v; int n=ponteiro_teste_lista(&v);
  for (int i=n-1;i>=0;i--) if(x>=v[i].x&&x<v[i].x+v[i].w&&y>=v[i].y&&y<v[i].y+v[i].h) {
    if(v[i].focar)v[i].focar(v[i].a,v[i].b);
    if(v[i].ativar)v[i].ativar(v[i].a,v[i].b);
    return;
  }
  REVIEW(0,"missing keyboard hit target");
}
static void reviewPixel(int id, unsigned char pixel[4]) {
  reviewDraw();
  const PonteiroAlvo *v; int n=ponteiro_teste_lista(&v);
  for (int i=0;i<n;i++) if(v[i].a==id) {
    glFinish();
    glReadPixels((int)(v[i].x+10),1079-(int)(v[i].y+8),1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    return;
  }
  REVIEW(0,"missing focus sample target");
}
static void reviewFocus(void) {
  const int ids[]={KEY_FIRST,KEY_FIRST+1,KEY_SHIFT,KEY_DELETE,KEY_DONE};
  profile=perfis_ativo(); provider=1; shotLoginPending=1;
  for(int reduced=0;reduced<2;reduced++) {
    dados_gravar("ajustes.txt",reduced?"idioma 0\nperformanceAutoVersaoLocal 1\nanimacoes 1\n":"idioma 0\nperformanceAutoVersaoLocal 1\nanimacoes 0\n"); ajustes_dir(dados_dir());
    REVIEW(ajustes_animacoes_reduzidas()==reduced,"focus motion preference");
    for(int glass=0;glass<2;glass++) for(int secret=0;secret<2;secret++) {
      ajustes_definir_vidro(glass); opened=1; screen=secret?PASSWORD:ADDRESS;
      selected=KEY_DONE; worker.alive=0; field=0;
      for(int f=0;f<90;f++) { shotTime+=16; srvsetup_atualizar(shotTime); }
      for(unsigned i=0;i<sizeof ids/sizeof *ids;i++) {
        unsigned char rest[4],start[4],middle[4],end[4];
        reviewPixel(ids[i],rest);
        selected=ids[i]; reviewPixel(ids[i],start);
        shotTime+=80; srvsetup_atualizar(shotTime); reviewPixel(ids[i],middle);
        for(int f=0;f<90;f++) { shotTime+=16; srvsetup_atualizar(shotTime); }
        reviewPixel(ids[i],end);
        int ok=memcmp(rest,end,3)!=0 && (reduced ? !memcmp(middle,end,3) :
          !memcmp(rest,start,3) && memcmp(middle,rest,3) && memcmp(middle,end,3));
        char name[128]; snprintf(name,sizeof name,"%s %s focus id %d, %s, %s",
          secret?"password":"address",i<2?"key":"footer",ids[i],glass?"glass":"solid",reduced?"reduced":"animated");
        REVIEW(ok,name);
      }
    }
  }
}
static int reviewP1(void) {
  ponteiro_teste_toque(1); opened=1; provider=2; screen=CONNECT_PASSWORD;
  worker.alive=0; worker.n=1; strcpy(worker.connect[0].nome,"Estúdio");
  strcpy(username,"Marina"); reviewDraw();
  REVIEW(strstr(reviewText,"Estúdio")&&strstr(reviewText,"Compartilhado com")&&strstr(reviewText,"›"),"Emby shared-server preview");

  const char *resources="[{\"provides\":\"server\",\"name\":\"Início\",\"clientIdentifier\":\"aaaa\",\"owned\":true,\"presence\":true,\"connections\":[{\"uri\":\"http://192.168.1.2:32400\",\"local\":true}]},{\"provides\":\"server\",\"name\":\"Cinema da família\",\"clientIdentifier\":\"bbbb\",\"owned\":false,\"presence\":true,\"connections\":[{\"uri\":\"https://remote.example.test\",\"local\":false}]},{\"provides\":\"server\",\"name\":\"Arquivo\",\"clientIdentifier\":\"cccc\",\"presence\":false,\"lastSeenAt\":1700000000,\"connections\":[]}]";
  provider=0; screen=PLEX_SERVERS; worker.n=px_ler_recursos(resources,strlen(resources),worker.plex,PX_SERVIDORES_MAX);
  reviewDraw();
  REVIEW(worker.n==3&&strstr(reviewText,"Seu servidor · Conexão local")&&strstr(reviewText,"Compartilhado · Conexão remota")&&strstr(reviewText,"Visto por último")&&strstr(reviewText,"Desconectado")&&strstr(reviewText,"Conectado"),"Plex ownership, connection, offline and last-seen rows");

  provider=1; screen=ADDRESS; selected=0; addressText[0]=0; reviewDraw();
  reviewClick(px(1085),sy(482));
  REVIEW(!strcmp(addressText,"1")&&!teclado_aberto(),"address key enters character without modal");
  teclado_esquecer(); field=0; reviewDraw(); reviewClick(px(1090),sy(748));
  REVIEW(strstr(addressText,"https://"),"URL shortcut");
  reviewDraw(); reviewClick(px(1205),sy(748));
  REVIEW(!strcmp(addressText,"1https:/"),"address Delete");
  reviewDraw(); reviewClick(px(1330),sy(748));
  REVIEW(selected==1&&!teclado_aberto(),"address Done focuses Connect");
  screen=PASSWORD; password[0]=0; selected=1; reviewDraw(); reviewClick(px(1090),sy(748));
  reviewDraw(); reviewClick(px(1085),sy(544));
  REVIEW(!strcmp(password,"Q")&&!teclado_aberto(),"password Shift and key entry");
  reviewDraw(); reviewClick(px(1240),sy(748));
  REVIEW(!password[0],"password Delete");
  reviewDraw(); reviewClick(px(1370),sy(748));
  REVIEW(selected==2,"password Done focuses Sign in");
  screen=ADDRESS; selected=2; key(SDLK_RIGHT); key(SDLK_RETURN);
  REVIEW(!strcmp(addressText,"1https:/1"),"D-pad reaches inline keyboard");
  key(SDLK_LEFT); REVIEW(selected==2,"D-pad leaves keyboard without phantom control");
  selected=KEY_DELETE; strcpy(addressText,"á"); key(SDLK_RETURN);
  REVIEW(!addressText[0],"Delete removes a complete UTF-8 character");
  memset(addressText,'a',sizeof addressText-1); addressText[sizeof addressText-1]=0;
  selected=KEY_FIRST; key(SDLK_RETURN); selected=KEY_SHIFT; key(SDLK_RETURN);
  REVIEW(strlen(addressText)==sizeof addressText-1,"inline keys preserve buffer bounds");
  reviewFocus();
  printf("P1 scenarios: %s (%d failures)\n",reviewErrors?"FAIL":"PASS",reviewErrors);
  return reviewErrors?1:0;
}
