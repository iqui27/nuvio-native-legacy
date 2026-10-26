// Exercise real setup transitions, keyboard result consumption and account exit.
#include "../src/aiostreamsui.c"
#include <assert.h>
static int keyboardResult, keyboardMasked, p2p, activeProfile=1;
static const char *keyboardText="";
static Stream traceStream;
int stream_n(void) { return 1; }
int stream_lista_do_alvo(const char *id) { return !strcmp(id,"tt1"); }
const Stream *stream_item(int i) { (void)i; return &traceStream; }
void stream_inspecao_ordem(int *r,int n) { for(int i=0;i<n;i++) r[i]=i+1; }
int celular_abrir_link(const char *t,const char *d) { (void)t;return d[0]!=0; }
int addons_n(void) { return 0; }
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
static int configFetches, noCache;
int rede_pedir(const RedePedido *p,RedeResposta *r) {
  memset(r,0,sizeof *r); r->status=200; r->erro=REDE_OK;
  for(int i=0;p->cabecalhos[i];i++) if(strstr(p->cabecalhos[i],"no-cache")) noCache++;
  if(strstr(p->url,"/api/v1/user")) {
    configFetches++;
    r->corpo=strdup(configFetches==1?
      "{\"success\":true,\"data\":{\"userData\":{\"requiredKeywords\":[\"first\"]}}}":
      "{\"success\":true,\"data\":{\"userData\":{\"preferredLanguages\":[\"Portuguese\"]}}}");
  } else if(strstr(p->url,"manifest.json")) r->corpo=strdup("{\"id\":\"aiostreams.test\",\"resources\":[]}");
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
  aioui_evento(&down); assert(foco==7); acao(); assert(regraSelecionada==7);
  regraSelecionada=-1; foco=8; aioui_evento(&down); assert(foco==10);
  foco=11; pendente=0; acao(); assert(pendente==1);
  // Both footer buttons retain their column when entering/leaving the grid.
  for(int col=0;col<2;col++) {
    mudar(FILTROS); foco=8+col;
    down.key.keysym.sym=SDLK_DOWN; aioui_evento(&down); assert(foco==10+col);
    down.key.keysym.sym=SDLK_UP; aioui_evento(&down); assert(foco==8+col);
  }
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
  activeProfile=2; aioui_atualizar(0,100); assert(aioui_aberto());
  aioui_atualizar(.2f,300); assert(!aioui_aberto() && !base[0]);
  puts("aiostreams UI: setup, masked import, P2P, navigation and profile isolation passed");
  return 0;
}
