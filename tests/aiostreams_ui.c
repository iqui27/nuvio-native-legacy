// Exercise real setup transitions, keyboard result consumption and account exit.
#include "../src/aiostreamsui.c"
#include <assert.h>
static int keyboardResult, keyboardMasked, p2p, activeProfile=1;
static int sidebarDrawn, sidebarHighlight, sidebarOpaque, glassEnabled;
static int dashboardDrawing, summarySurfaces, dashboardLogo, cardFocus, accentHeaders, manifestLabel;
int anim_politica_reduzida;
int aiopair_start(const char *s) { (void)s; return 0; }
void aiopair_close(void) { }
int aiopair_state(char *s,size_t n) { if(n)s[0]=0;return AIOPAIR_UNAVAILABLE; }
int aiopair_take(char *a,size_t n,char *o,size_t no) { return 0; }
void ajustes_acento(float *r,float *g,float *b) { *r=*g=*b=.9f; }
float ajustes_acento_tinta(float *r,float *g,float *b) { ajustes_acento(r,g,b); return .071f; }
void plrui_material(GfxRect r,float radius,int modal,float a) {
  if(dashboardDrawing && r.y==248 && r.h==132) summarySurfaces++;
}
GfxRecorteAnt gfx_recorte_dentro(float x,float y,float w,float h) { return (GfxRecorteAnt){0}; }
void gfx_recorte_voltar(GfxRecorteAnt r) { }
float gfx_opacidade_grupo=1;
float gfx_escala_txt(void) { return 1; }
float gfx_escala_txt_entrar(void) { return 1; }
void gfx_escala_sair(float s) { (void)s; }
int ajustes_vidro(void) { return glassEnabled; }
float gfx_vidro_opacidade(void) { return 1; }
GLuint tex_obter(const char *p) { (void)p; return 1; }
void gfx_rect(GfxRect r,GLuint t,GfxModo m,float a,float b,float c,float d,float e,float f,float g,float h) {
  if(dashboardDrawing && t && m==GFX_MARCA && r.w>=56 && r.h>=56 && r.y<248) dashboardLogo++;
}
void gfx_sombra_sob(GfxRect r,float a,float b,float c,float d,float e,float f,float g,GfxRect s,float h,float i) { }
void gfx_vidro(GfxRect r,float a,int b,float c,float d,float e) { }
void gfx_luz_canto(GfxRect r,float a,float b,float c,float d,float e,float f,float g,float h) { }
void gfx_cor(GfxRect r,float a,float b,float c,float d,float e) {
  if(dashboardDrawing && r.w==48 && r.h==48 && e>.9f) accentHeaders++;
  // The open rail: its own lighter surface, then the focused item filled in the accent.
  if(r.x==72 && r.w==320 && e>0) {
    sidebarDrawn++;
    if(r.y==148 && r.h==884 && e==.06f && b==1 && c==1 && d==1) sidebarOpaque++;
  }
  if(r.x==84 && r.y==248 && e>.9f) sidebarHighlight++;
}
void gfx_icone(GfxRect r,const char *s,float a,float b,float c,float d) { }
void plrui_linha_foco(GfxRect r,float radius,float alpha) {
  if(dashboardDrawing && r.w>300 && r.h>100 && alpha>.9f) cardFocus++;
  if(r.x==86 && r.y==265 && alpha>.9f) sidebarHighlight++;
}
TxtLinha txt_linha_corta(TxtEstilo e,const char *s,int r,int g,int b,int a,float w) {
  if(dashboardDrawing && !strcmp(s,"Configuração do manifesto")) manifestLabel++;
  return (TxtLinha){0};
}
void txt_desenhar(TxtLinha l,float x,float y) { }
float txt_bloco_corta(TxtEstilo e,const char *s,int r,int g,int b,float x,float y,float w,float leading,float alpha,int lines) { return leading; }
float botao_largura(const char *s,const char *icon,int primary) { return 200; }
void botao_pilula(GfxRect r,const char *s,const char *icon,float f,int primary,int align,float alpha) { }
#undef ponteiro_alvo
void ponteiro_alvo(float x,float y,float w,float h,PonteiroFn f,PonteiroFn a,int i,int b) { }
static const char *keyboardText="";
static Stream traceStream;
int stream_n(void) { return 1; }
int stream_lista_do_alvo(const char *id) { return !strcmp(id,"tt1"); }
const Stream *stream_item(int i) { (void)i; return &traceStream; }
void stream_inspecao_ordem(int *r,int n) { for(int i=0;i<n;i++) r[i]=i+1; }
int celular_abrir_link(const char *t,const char *d) { (void)t;return d[0]!=0; }
int addons_n(void) { return 0; }
int addons_aio_history(int i,const char *type,AddonAioRequest *out,int max) { (void)i;(void)type;(void)out;(void)max; return 0; }
int addons_exportar(AddonRemoto *v,int n) { (void)v;(void)n;return 0; }
int perfis_ativo_addons(void) { return activeProfile; }
int contacache_gravar(const char *s,int p,const char *u,const char *c) { (void)s;(void)p;(void)u;(void)c;return 1; }
unsigned addons_versao(void) { return 0; }
const char *addons_nome(int i) { (void)i; return ""; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }
const char *addons_base(int i) { (void)i; return ""; }
int addons_adicionar(const char *n,const char *u) { (void)n;(void)u;return 1; }
int perfis_ativo(void) { return activeProfile; }
const char *sessao_usuario(void) { return "offline-user"; }
const char *i18n(const char *s) { return s; }
void teclado_abrir_com(const char *t,const char *d,int m,const char *a,const char *i) { (void)t;(void)d;(void)m;(void)a;(void)i; }
void teclado_mascarar(int v) { keyboardMasked=v; }
int teclado_aberto(void) { return 0; }
void teclado_evento(const SDL_Event *e) { (void)e; }
void teclado_atualizar(float d,Uint32 a) { (void)d;(void)a; }
int teclado_resultado(void) { int r=keyboardResult; keyboardResult=0; return r; }
const char *teclado_texto(void) { return keyboardText; }
void teclado_esquecer(void) { keyboardText=""; }
int celular_disponivel(void) { return 0; }
int celular_abrir(const char *t) { (void)t;return 0; }
void celular_fechar(void) { }
int celular_pegar(char *d,size_t n) { (void)d;(void)n;return 0; }
int ajustes_p2p_ligado(void) { return p2p; }
int ajustes_animacoes_reduzidas_usuario(void) { return 0; }
void ajustes_definir_p2p_ligado(int v) { p2p=v; }
int fil_n(void) { return 0; }
const char *fil_chave(int i) { (void)i;return ""; }
void fil_alternar(int i) { (void)i; }
int fil_mover(int i,int d) { (void)i;(void)d;return 0; }
void desc_remontar_fileiras(void) { }
void desc_repetir(void) { }
static int configFetches, noCache, configDenied, authorizationHeaders;
static void refreshSaved(void) {
  proximaConsulta=0; iniciarRede();
  for(;;) {
    pthread_mutex_lock(&trava); int done=!ocupado; pthread_mutex_unlock(&trava);
    if(done) break;
    SDL_Delay(1);
  }
  entregue=0;
}
int rede_pedir(const RedePedido *p,RedeResposta *r) {
  memset(r,0,sizeof *r); r->status=200; r->erro=REDE_OK;
  for(int i=0;p->cabecalhos[i];i++) {
    if(strstr(p->cabecalhos[i],"no-cache")) noCache++;
    if(!strncmp(p->cabecalhos[i],"Authorization:",14)) authorizationHeaders++;
  }
  if(strstr(p->url,"/api/v1/user")) {
    configFetches++;
    if(configDenied) { r->status=401; return 0; }
    r->corpo=strdup(configFetches==1?
      "{\"success\":true,\"data\":{\"userData\":{\"requiredKeywords\":[\"first\"]}}}":
      "{\"success\":true,\"data\":{\"userData\":{\"preferredLanguages\":[\"Portuguese\"]}}}");
  } else if(strstr(p->url,"manifest.json")) r->corpo=strdup("{\"id\":\"aiostreams.test\",\"resources\":[\"stream\"]}");
  else if(strstr(p->url,"/status")) r->corpo=strdup("{\"data\":{\"settings\":{\"searchApiDisabled\":false}}}");
  else if(strstr(p->url,"/search?")) r->corpo=strdup("{\"success\":true,\"data\":{\"results\":[],\"statistics\":[]}}");
  else r->corpo=strdup("{}");
  return 0;
}
void rede_resposta_limpar(RedeResposta *r) { (void)r; }
int main(void) {
  aioui_abrir(-1); assert(pagina==SEM);
  foco=1; acao(); assert(pagina==INSTANCIA && entrada==1);
  keyboardText="https://self.test"; keyboardResult=TECLADO_PRONTO;
  aioui_atualizar(0,100); assert(pagina==SERVICOS && !strcmp(host,"https://self.test"));
  foco=5; acao(); assert(pagina==PRONTO && !snap.valida && !pendente);
  foco=1; acao(); assert(pagina==SERVICOS);
  foco=4; acao(); assert(pagina==IMPORTAR);
  foco=1; acao(); assert(entrada==3 && keyboardMasked);
  keyboardText="https://self.test/manifest.json/evil"; keyboardResult=TECLADO_PRONTO;
  aioui_atualizar(0,100); assert(!pendente && mensagem[0]);
  pagina=FONTES; foco=0; acao(); assert(p2p);
  lateral=1; foco=5; acao(); assert(pagina==FILTROS);
  strcpy(alvo,"tt1"); strcpy(traceStream.url,"https://media.test/v");
  aio_resultados(&snap,"{\"streams\":[{\"url\":\"https://media.test/v\"},{\"externalUrl\":\"https://outside.test/private\"}]}");
  trace(); assert(snap.resultado[0].rank==1 && snap.resultado[0].decisao==4);
  traceStream.foraCache=1; trace(); assert(snap.resultado[0].decisao==3);
  pagina=LINKS; lateral=0; foco=0; acao(); assert(linkSelecionado==1);
  pagina=FILTROS; regraSelecionada=-1; foco=2; acao(); assert(regraSelecionada==2);
  AioRegra seed={.grupo=3,.nome="requiredSeederRange"}, cache={.grupo=3,.nome="excludeUncached"};
  regraSelecionada=4; assert(regraPertence(&seed) && !regraPertence(&cache));
  regraSelecionada=3; assert(!regraPertence(&seed) && regraPertence(&cache));
  regraSelecionada=6; AioRegra order={.grupo=5}; assert(regraPertence(&order));
  regraSelecionada=7; AioRegra format={.grupo=6}; assert(regraPertence(&format));
  regraSelecionada=-1;
  // Every section is reachable, including formerly display-only sort/formatter.
  linkSelecionado=-1; entrada=0;
  SDL_Event down={0}; down.type=SDL_KEYDOWN; down.key.keysym.sym=SDLK_DOWN;
  for(int i=0;i<10;i++) {
    pagina=FILTROS; lateral=0; regraSelecionada=-1; foco=i; acao();
    assert(regraSelecionada==i);
    down.key.keysym.sym=SDLK_ESCAPE; aioui_evento(&down);
    assert(regraSelecionada==-1 && foco==i);
  }
  // Back from a detail entered from the sidebar must restore content focus.
  // Services and preferences is filter group 9, not the setup page SERVICOS.
  for(int i=9;i>=0;i--) {
    mudar(FILTROS); foco=i; acao();
    down.key.keysym.sym=SDLK_LEFT; aioui_evento(&down); assert(lateral);
    down.key.keysym.sym=SDLK_ESCAPE; aioui_evento(&down);
    assert(pagina==FILTROS && regraSelecionada==-1 && foco==i && !lateral);
    down.key.keysym.sym=SDLK_RETURN; aioui_evento(&down);
    assert(pagina==FILTROS && regraSelecionada==i);
  }
  mudar(FILTROS);
  pagina=FILTROS; foco=5; down.key.keysym.sym=SDLK_DOWN;
  aioui_evento(&down); assert(foco==6); acao(); assert(regraSelecionada==6);
  regraSelecionada=-1; foco=8; aioui_evento(&down); assert(foco==9);
  foco=11; pendente=0; acao(); assert(pendente==1);
  // Groups are one list now: Down from the last group reaches the footer, Up returns.
  mudar(FILTROS); foco=9;
  down.key.keysym.sym=SDLK_DOWN; aioui_evento(&down); assert(foco==10);
  down.key.keysym.sym=SDLK_UP; aioui_evento(&down); assert(foco==9);
  pendente=0;
  pagina=CATALOGOS; foco=nLinhas()-1; pendente=0; acao();
  assert(pagina==CATALOGOS && pendente==1);
  pendente=0; foco=nLinhas()-2; acao(); assert(pagina==SERVICOS);
  pagina=LEGENDAS; snap.addons=2; snap.addon[0].legenda=1; snap.addon[1].legenda=1;
  snap.legendas=2; assert(nLinhas()==8);
  foco=1; acao(); assert(pagina==SERVICOS);
  pagina=TITULO; evidenciaSelecionada=0; foco=nLinhas()-1; acao(); assert(evidenciaSelecionada);
  foco=nLinhas()-1; acao(); assert(!evidenciaSelecionada);
  // Content swaps retain the old page until its text has faded.
  pagina=PAINEL; paginaAnterior=-1;
  for(int i=0;i<20;i++)movimento(.01f);
  mudar(CATALOGOS); movimento(.02f);
  assert(paginaAnterior==PAINEL && corpo>0 && corpo<1);
  for(int i=0;i<20;i++)movimento(.01f);
  assert(paginaAnterior==CATALOGOS && corpo==1);
  foco=3; movimento(.02f);
  assert(focoDe(3,0)>0 && focoDe(3,0)<1);
  float old=scrollFoco;
  foco=1; movimento(.01f);
  assert(scrollFoco>old && scrollVel>0);
  // Footer focus keeps the scrolled viewport instead of resetting it.
  pagina=CATALOGOS; snap.catalogos=8; scrollFoco=11;
  assert(janelaCatalogos()==3);
  snap.catalogos=500; foco=499; scrollFoco=499;
  assert(nConteudo()==504 && janelaCatalogos()==495);
  down.key.keysym.sym=SDLK_UP; aioui_evento(&down); assert(foco==498);
  foco=499; down.key.keysym.sym=SDLK_RETURN; aioui_evento(&down);
  assert(foco==499);
  // Link/subtitle windows follow the fractional spring, rows fade across one slot (no integer jump).
  scrollFoco=3.5f; float w=janela(2,8,3,1);
  assert(w>1.4f && w<1.6f && (int)w==1);
  assert(rolaAlfa(-.5f,3)>.45f && rolaAlfa(-.5f,3)<.55f && rolaAlfa(1,3)==1 && rolaAlfa(3,3)==0 && rolaAlfa(-1,3)==0);
  // Real worker path, executed synchronously: each refresh fetches a new envelope.
  // Synthetic auth only; no network or real account in this test.
  for(int i=0;i<2;i++) {
    Pedido *request=calloc(1,sizeof *request); assert(request);
    strcpy(request->host,"https://example.invalid");
    strcpy(request->base,"https://example.invalid/stremio/sample/sample/manifest.json");
    worker(request);
    assert(entrega.configurada && entrega.detalhes==1);
    assert(strstr(entrega.detalhe[0].nome,i?"preferredLanguages":"requiredKeywords"));
    entregue=0;
  }
  assert(configFetches==2 && noCache==6);
  // Sanitized manifest-only shape: embedded base64url JSON, no API identity.
  FILE *fixture=fopen("tests/fixtures/aiostreams-manifest-only.json","rb"); assert(fixture);
  unsigned char raw[4096]; size_t len=fread(raw,1,sizeof raw,fixture); fclose(fixture);
  char encoded[6000]; size_t at=0;
  const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  for(size_t i=0;i<len;i+=3) {
    unsigned a=raw[i],b=i+1<len?raw[i+1]:0,c=i+2<len?raw[i+2]:0;
    encoded[at++]=alphabet[a>>2]; encoded[at++]=alphabet[((a&3)<<4)|(b>>4)];
    if(i+1<len) encoded[at++]=alphabet[((b&15)<<2)|(c>>6)];
    if(i+2<len) encoded[at++]=alphabet[c&63];
  }
  encoded[at]=0;
  char manifest[6500],alias[2048];
  snprintf(manifest,sizeof manifest,"https://example.invalid/%s/manifest.json",encoded);
  assert(nv_addon_url_guardar("AIOStreams",manifest,alias,sizeof alias));
  Pedido *embedded=calloc(1,sizeof *embedded); assert(embedded);
  strcpy(embedded->host,"https://example.invalid");
  nv_addon_url(embedded->base,sizeof embedded->base,alias,"/manifest.json");
  worker(embedded);
  assert(entrega.configurada && entrega.addons==2 && entrega.servicos==1);
  assert(entrega.detalhes>15 && entrega.regraUncached==1 && configFetches==2 && entrega.configEmbedded);
  snap=entrega; pagina=PAINEL; lateral=0; foco=2; memset(dashboardLinha,0,sizeof dashboardLinha);
  navigationCount=0; down.key.keysym.sym=SDLK_DOWN;
  aioui_evento(&down); assert(foco==2 && dashboardLinha[2]==1);
  down.key.keysym.sym=SDLK_LEFT; aioui_evento(&down); assert(foco==1);
  down.key.keysym.sym=SDLK_DOWN; aioui_evento(&down); assert(foco==4);
  strcpy(host,"https://example.invalid");
  strcpy(base,"https://example.invalid/manifest.json");
  foco=9; acao(); assert(connectOpen && !entrada);
  down.key.keysym.sym=SDLK_RIGHT; aioui_evento(&down);
  down.key.keysym.sym=SDLK_RETURN; aioui_evento(&down); assert(!connectOpen && entrada==4 && keyboardMasked);
  receber("sample-uuid"); assert(entrada==5 && keyboardMasked);
  receber("sample-password"); assert(!entrada && configAuth[0] && !configUuid[0]);
  pagina=PAINEL; lateral=0; foco=3; acao();
  assert(pagina==FILTROS && regraSelecionada==7);
  // Both first-column cards reach the distinct sidebar and restore their card.
  for(glassEnabled=0;glassEnabled<2;glassEnabled++) for(int row=0;row<2;row++) {
    pagina=PAINEL; lateral=0; foco=row*3;
    down.key.keysym.sym=SDLK_LEFT; aioui_evento(&down);
    assert(lateral && foco==0);
    movimento(.5f); sidebarOpen=1; sidebarDrawn=sidebarHighlight=sidebarOpaque=0; navigationCount=0;
    sidebar();
    assert(pagina==PAINEL && sidebarDrawn && sidebarOpaque==1 && sidebarHighlight && navigationCount==7);
    // Moving on the rail previews the section; entering it and going Back restores the card.
    down.key.keysym.sym=SDLK_DOWN; aioui_evento(&down); assert(lateral && foco==1 && pagina==CATALOGOS);
    down.key.keysym.sym=SDLK_RIGHT; aioui_evento(&down); assert(!lateral && foco==0 && pagina==CATALOGOS);
    down.key.keysym.sym=SDLK_ESCAPE; aioui_evento(&down);
    assert(!lateral && pagina==PAINEL && foco==row*3);
  }
  lateral=0; glassEnabled=0;
  pagina=PAINEL; foco=3; dashboardFoco=3; lateral=1; foco=1; acao();
  assert(pagina==CATALOGOS && !lateral && !dashboardReturn);
  down.key.keysym.sym=SDLK_ESCAPE; aioui_evento(&down);
  assert(pagina==PAINEL && foco==3 && !lateral);
  lateral=1; foco=5; acao(); assert(pagina==FILTROS && !lateral);
  down.key.keysym.sym=SDLK_ESCAPE; aioui_evento(&down);
  assert(pagina==PAINEL && foco==3 && !lateral);
  // Same-origin refresh authenticates; importing another instance must not.
  int headers=authorizationHeaders;
  refreshSaved(); assert(authorizationHeaders==headers+1);
  entrada=3; receber("https://other.invalid/manifest.json");
  refreshSaved(); assert(authorizationHeaders==headers+1);
  assert(!configAuth[0]);
  // A different manifest on the same origin also invalidates saved credentials.
  entrada=4; receber("sample-uuid"); receber("sample-password");
  refreshSaved(); assert(authorizationHeaders==headers+2);
  entrada=3; receber("https://other.invalid/alternate/manifest.json");
  refreshSaved(); assert(authorizationHeaders==headers+2 && !configAuth[0]);
  // Changing scheme or port clears saved authorization as well.
  const char *origins[]={"https://other.invalid:8443","http://other.invalid"};
  for(int i=0;i<2;i++) {
    instanceHost("https://other.invalid");
    entrada=4; receber("sample-uuid"); receber("sample-password");
    assert(configAuth[0]); entrada=1; receber(origins[i]);
    assert(!configAuth[0]);
  }
  // Worker also refuses stale authorization if a request origin is changed.
  Pedido *mismatch=calloc(1,sizeof *mismatch); assert(mismatch);
  strcpy(mismatch->host,"https://other.invalid");
  strcpy(mismatch->base,"https://example.invalid/stremio/sample/sample/manifest.json");
  strcpy(mismatch->auth,"Authorization: Basic synthetic");
  strcpy(mismatch->authOrigin,"https://example.invalid");
  worker(mismatch); assert(authorizationHeaders==headers+2);
  char title[192],value[384];
  strcpy(snap.catalogo[0].tipo,"movie");
  dashboardRow(4,0,title,sizeof title,value,sizeof value); assert(!strcmp(value,i18n("Filmes")));
  strcpy(snap.catalogo[0].tipo,"series");
  dashboardRow(4,0,title,sizeof title,value,sizeof value); assert(!strcmp(value,i18n("Séries")));
  pendente=0;
  static AioSnapshot malformed; assert(!manifestConfig(&malformed,"https://example.invalid/not-json/manifest.json"));
  assert(!manifestConfig(&malformed,"https://example.invalid/eyJ0IjoiYWlvRW5jcnlwdCJ9/manifest.json"));
  for(int i=0;i<entrega.detalhes;i++) assert(!strstr(entrega.detalhe[i].valor,"SANITIZED_TEST_KEY"));
  entregue=0;
  configDenied=1;
  Pedido *locked=calloc(1,sizeof *locked);
  strcpy(locked->host,"https://example.invalid");
  strcpy(locked->base,"https://example.invalid/stremio/sample/encrypted/manifest.json");
  strcpy(locked->id,"tt1"); strcpy(locked->tipo,"movie"); locked->diagnostic=1;
  worker(locked); snap=entrega; entregue=0;
  assert(snap.valida && snap.http[2]==401 && snap.search && !snap.configurada);
  assert(conectada()); // Config denial must not disconnect the installation.
  assert(snap.manifestAccess==AIO_AVAILABLE && snap.searchAccess==AIO_AVAILABLE);
  assert(snap.configAccess==AIO_RESTRICTED && snap.analyticsAccess==AIO_UNKNOWN);
  // Draw the actual dashboard contract: one summary surface, visible product
  // art, standard neutral card focus, and all six existing action targets.
  pagina=PAINEL; lateral=0; foco=0; sidebarOpen=0; entradaTempo=2;
  snap=(AioSnapshot){.valida=1,.configurada=1};
  memset(focoAnim,0,sizeof focoAnim); focoAnim[0][0]=1;
  for(int state=0;state<4;state++) for(glassEnabled=0;glassEnabled<2;glassEnabled++) {
    snap.valida=state!=3; snap.configurada=state<2; snap.configEmbedded=state==1;
    dashboardDrawing=1; summarySurfaces=dashboardLogo=cardFocus=accentHeaders=manifestLabel=0; navigationCount=0;
    dashboard(); dashboardDrawing=0;
    assert(dashboardLogo==1 && "dashboard must show existing AIOStreams art beside its title");
    assert(summarySurfaces==1 && "summary counts share one surface, not four competing tiles");
    assert(cardFocus==1 && !accentHeaders && "card focus uses the standard neutral surface");
    assert((manifestLabel>0)==(state==1) && "embedded configuration is not authenticated API access");
    for(int i=0;i<6;i++) {
      int found=0; for(int j=0;j<navigationCount;j++) found+=navigationIds[j]==i;
      assert(found==1);
    }
    if(!snap.valida) {
      for(int j=0;j<navigationCount;j++) if(navigationIds[j]==4) {
        assert(navigationRects[j].w==1660 && navigationRects[j].h<=110 && "unavailable catalogs must not leave an empty half-grid");
      }
      foco=0; down.key.keysym.sym=SDLK_DOWN; dashboardTecla(SDLK_DOWN); assert(foco==4);
      dashboardTecla(SDLK_DOWN); assert(foco==5);
      dashboardTecla(SDLK_UP); assert(foco==4);
    }
  }
  activeProfile=2; aioui_atualizar(0,100); assert(aioui_aberto());
  aioui_atualizar(.2f,300); assert(!aioui_aberto() && !base[0]);
  puts("aiostreams UI: setup, masked import, P2P, navigation and profile isolation passed");
  return 0;
}
