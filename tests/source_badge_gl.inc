// Real GLES regression: transparent black logo on imported chip backgrounds.
static void importedBadgePixel(int x,int y,unsigned char *pixel) {
  glReadPixels(x,1080-y-1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
}
static void importedBadgePixels(const char *dir) {
  char image[512],json[2048];snprintf(image,sizeof image,"%s/badge-black.png",dir);
  SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,96,32,32,SDL_PIXELFORMAT_RGBA32);
  assert(surface);SDL_FillRect(surface,NULL,SDL_MapRGBA(surface->format,0,0,0,0));
  SDL_Rect ink={8,8,28,16};SDL_FillRect(surface,&ink,SDL_MapRGBA(surface->format,0,0,0,255));
  assert(!IMG_SavePNG(surface,image));SDL_FreeSurface(surface);
  assert(tex_iniciar(64));GLuint texture=0;
  for(int retry=0;retry<200&&!texture;retry++) {
    tex_bombear(8);texture=tex_obter_larg_qualquer(image,128);if(!texture)SDL_Delay(5);
  }
  assert(texture);unsigned short id=0;
  for(int pass=0;pass<4;pass++) {
    const char *style=pass==1?"bordered":"filled";
    const char *color=pass==2?"#40FFFF00":pass==3?"#00000000":"#FFFFFF00";
    snprintf(json,sizeof json,"{\"filters\":[{\"name\":\"Black logo\",\"pattern\":\"logo\","
      "\"imageURL\":\"%s\",\"tagStyle\":\"%s\",\"tagColor\":\"%s\",\"borderColor\":\"%s\"}]}",
      image,style,color,pass==3?"#00000000":"#FF0000FF");
    assert(selospacote_adicionar(json,"https://fixture.invalid/badge-style.json")==SELOS_OK);
    glViewport(0,0,1920,1080);gfx_sem_recorte();
    glClearColor(.1f,.1f,.1f,1);glClear(GL_COLOR_BUFFER_BIT);
    assert(fabsf(selosDesenhar(&id,1,10,20,300,24,1,1,0,0)-78)<.01f);glFinish();
    unsigned char pixel[4];importedBadgePixel(28,32,pixel);
    assert(pixel[0]<3&&pixel[1]<3&&pixel[2]<3); // Logo retains original black pixels.
    importedBadgePixel(68,32,pixel);
    if(pass==0)assert(pixel[0]>250&&pixel[1]>250&&pixel[2]<3);
    else if(pass==2)assert(abs(pixel[0]-83)<5&&abs(pixel[1]-83)<5&&abs(pixel[2]-19)<5);
    else assert(abs(pixel[0]-26)<3&&abs(pixel[1]-26)<3&&abs(pixel[2]-26)<3);
    unsigned char edge[80*36*4];
    glReadPixels(8,1080-53,80,36,GL_RGBA,GL_UNSIGNED_BYTE,edge);
    int blue=0;for(size_t i=0;i<sizeof edge;i+=4)blue+=edge[i]<50&&edge[i+1]<50&&edge[i+2]>180;
    assert(pass==3?blue==0:blue>10);
    assert(selospacote_remover(selospacote_ativo()));
  }
  tex_encerrar();unlink(image);
  puts("PASS: real GLES imported black logos, filled/outline chips, ARGB opacity and transparent backgrounds");
}
