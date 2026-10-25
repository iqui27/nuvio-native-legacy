#include "../src/esportesui.c"
#include <assert.h>
/* Navigation/matching fixture: no SDL video/audio runtime is needed. */
Uint32 SDLCALL SDL_GetTicks(void) { return 1000; }
static int fonteAberta,aberturasFonte;
static uint64_t perfilTeste=1;
uint64_t perfis_sessao_geracao(void) { return perfilTeste; }
int perfis_sessao_atual(uint64_t v) { return v==perfilTeste; }
void stream_folha_canais_abrir(const char *titulo) { fontescanais_iniciar(titulo);fonteAberta=1;aberturasFonte++; }
int stream_folha_canais_aberta(void) { return fonteAberta; }
void stream_folha_canais_fechar(void) { fonteAberta=0;fontescanais_limpar(); }
int ctx_aberto(void) { return 0; }
int ponteiro_ok_longo(void) { return 0; }
int segurar_venceu(Uint32 a,Uint32 d,const char *t) { (void)t;return a-d>=600; }
void ilha_atividade(const char *t,float p) { (void)t;(void)p; }
float anim_clamp(float v,float a,float b) { return fmaxf(a,fminf(v,b)); }
void ponteiro_alvo_segurar(PonteiroFn f) { (void)f; }
static unsigned rev=1;
static int ligasProntas=1, visitasOcultas, qualidadeFundo, aparelhoFraco;
int esportes_fundo_qualidade(void){return qualidadeFundo;}
int esportes_fundo_definir_qualidade(int q){qualidadeFundo=q;rev++;return 1;}
int memlog_android_fraco(void){return aparelhoFraco;}
long memlog_livre_mb(void){return 4096;}
int ptv_gpu_fraca_atual(void){return 0;}
int gfx_efeitos_minimos(void){return 0;}
int ajustes_qualidade_imagem(void){return 1;}
void ajustes_acento(float *r,float *g,float *b){*r=.8f;*g=.5f;*b=.1f;}
float gfx_tex_aspect_atual;
void gfx_cor(GfxRect r,float raio,float cr,float cg,float cb,float ca){(void)r;(void)raio;(void)cr;(void)cg;(void)cb;(void)ca;}
void gfx_anel(GfxRect r,float raio,float esp,float cr,float cg,float cb,float ca){(void)r;(void)raio;(void)esp;(void)cr;(void)cg;(void)cb;(void)ca;}
float nv_layout_w=1920,nv_layout_h=1080;
static float escalaTeste=1;
float gfx_escala_ui(void) { return escalaTeste; }
static float bordaMenu;
float menu_barra_borda(void) { return bordaMenu; }
float ajustes_conteudo_x(void) { return 104; }
static int habilitados[16], pediuLigas, pediuEventos, favorito, tecladoResultado;
static EspLiga lTeste={"nba","NBA",""};
static EspEvento eTeste,favTeste[4];
static int nFavTeste,timesFavTeste,trocarFav;
void esportes_pedir_favoritos(int a) { (void)a; }
int esportes_favoritos_times(void) { return timesFavTeste; }
int esportes_favoritos_n(void) { return nFavTeste; }
const EspEvento *esportes_favoritos_evento(int i) { return i>=0 && i<nFavTeste?&favTeste[i]:NULL; }
int esportes_favoritos_carregando(void) { return 0; }
int esportes_favoritos_erro(void) { return 0; }
static EspLiga ligasCat[3]={{"nfl","NFL",""},{"nba","NBA",""},{"college-football","College football",""}};
static int catLigado[3]={1,1,1},catPedido[3],catInicio;
void esportes_catalogo_iniciar(int m,int a){(void)m;(void)a;catInicio++;}
void esportes_catalogo_suspender(void){}
int esportes_catalogo_n(void){return 3;}
int esportes_catalogo_modalidade(int l){return l==1?2:1;}
const EspLiga *esportes_catalogo_liga(int l){return l>=0 && l<3?&ligasCat[l]:NULL;}
int esportes_liga_habilitada(int m,const char *id){(void)m;for(int i=0;i<3;i++)if(!strcmp(id,ligasCat[i].id))return catLigado[i];return 1;}
int esportes_liga_habilitar(int m,const char *id,int on){(void)m;for(int i=0;i<3;i++)if(!strcmp(id,ligasCat[i].id)){catLigado[i]=on;rev++;return 1;}return 0;}
void esportes_catalogo_visiveis(void){memset(catPedido,0,sizeof catPedido);}
void esportes_catalogo_pedir(int i){assert(i>=0 && i<3 && catLigado[i]);catPedido[i]++;}
int esportes_catalogo_eventos_n(int l){return l>=0 && l<3?12:0;}
const EspEvento *esportes_catalogo_evento(int l,int e){return l>=0 && l<3 && e>=0 && e<12?&eTeste:NULL;}
int esportes_catalogo_estado(int l){(void)l;return 1;}
int esportes_catalogo_carregando(void){return 0;}
const char *i18n(const char *s) { return s; }
static int mobileTeste;
int layout_modo_mobile(void) { return mobileTeste; }
void esportes_abrir_sessao(void) { static int primeira=1;if(primeira){for(int i=0;i<16;i++)habilitados[i]=1;primeira=0;} }
int esportes_habilitado(int i) { return i>=0 && i<16 && habilitados[i]; }
int esportes_habilitar(int i,int v) { habilitados[i]=v; return 1; }
void esportes_cancelar(void) {}
int esportes_favorito(const EspEvento *e,int p) { (void)e;(void)p;return favorito; }
int esportes_alternar_favorito(const EspEvento *e,int p) { (void)e;(void)p;favorito=!favorito;rev++;return 1; }
void esportes_pedir_ligas(int m,int a) { (void)m;(void)a;pediuLigas++;ligasProntas=0;rev++; }
void esportes_pedir_eventos(int m,const EspLiga *l,int a) { (void)m;(void)l;(void)a;pediuEventos++;rev++; }
void esportes_passo(void) { if(trocarFav){EspEvento t=favTeste[0];favTeste[0]=favTeste[1];favTeste[1]=t;trocarFav=0;rev++;} }
int esportes_carregando(void) { return 0; }
int esportes_erro(void) { return 0; }
unsigned esportes_revisao(void) { return rev; }
int esportes_ligas_n(void) { return ligasProntas; }
const EspLiga *esportes_liga(int i) { return i>=0 && i<ligasProntas?&lTeste:NULL; }
int esportes_eventos_n(void) { return 1; }
const EspEvento *esportes_evento(int i) { return i==0?&eTeste:NULL; }
const char *teclado_alfabeto(void) { return "abc"; }
void teclado_abrir_com(const char *t,const char *d,int n,const char *a,const char *i) { (void)t;(void)d;(void)n;(void)a;(void)i; }
void teclado_evento(const SDL_Event *e) { (void)e; }
void teclado_atualizar(float dt,Uint32 a) { (void)dt;(void)a; }
int teclado_resultado(void) { return tecladoResultado; }
const char *teclado_texto(void) { return "does not match"; }
void teclado_esquecer(void) { tecladoResultado=0; }
void guia_carregar(void) {}
void epg_iniciar(void) {}
void epg_passo(void) {}
void xtepg_passo(void) {}
int epg_estado(void) { return EPG_PRONTO; }
unsigned epg_revisao(void) { return 1; }
unsigned guia_busca_revisao(void) { return 1; }
int guia_busca_carregando(void) { return 0; }
int guia_busca_elegivel(int i) { return i>=0 && i<3; }
int guia_canal_campos(int i,const char **id,const char **n,const char **l,const char **c,const char **b) {
  (void)l;(void)c;(void)b;
  static const char *ids[]={"wrong","match","broadcaster","hidden"},*names[]={"Sports One","Sports Two","ESPN HD","Hidden channel"};
  if(i<0 || i>=4)return 0;
  *id=ids[i];*n=names[i];return 1;
}
const char *guia_busca_conta(int i) { return i==1?"Account Two":"Account One"; }
int guia_busca_programas(int i,time_t d,time_t a,int p,EpgProg *out,int cap) {
  (void)d;(void)a;(void)p;(void)cap;
  if(i==3)visitasOcultas++;
  if(i==2)return 0;
  *out=(EpgProg){eTeste.inicio,eTeste.inicio+7200,i==0?"Celtics vs Cavaliers":"Celtics vs Magic"};return 1;
}
/* Record calls from the real draw routine, without an SDL/GL runtime. */
static int imagens,imagensDesenhadas,alvosGrade,icones,favAlvos,baixos,altos;
static int imagemPronta=1;
static float aspectoTeste=2;
static int marcaEscuraTeste, tomUnicoTeste;
static GfxModo modoLogoTeste;
static char ultimoIcone[40];
static GfxRect caixaIcone;
void gfx_icone(GfxRect r,const char *n,float cr,float cg,float cb,float ca) { (void)cr;(void)cg;(void)cb;(void)ca;assert(r.w>0 && r.h>0);icones++;snprintf(ultimoIcone,sizeof ultimoIcone,"%s",n);caixaIcone=r; }
float gfx_escala(void) { return 1; }
float gfx_escala_entrar(void) { return 1; }
void gfx_escala_sair(float v) { (void)v; }
void gfx_recorte(float x,float y,float w,float h) { (void)x;(void)y;assert(w>0 && h>0); }
void gfx_sem_recorte(void) {}
void gfx_rect(GfxRect r,GLuint t,GfxModo m,float f,float px,float py,float raio,float a,float b,float c,float d) {
  modoLogoTeste=m;(void)f;(void)px;(void)py;(void)raio;(void)a;(void)b;(void)c;(void)d;
  assert(r.w>0 && r.h>0);if(t)imagensDesenhadas++;
}
void plrui_cartao(GfxRect r,float raio,float a) { (void)raio;(void)a;assert(r.w>0 && r.h>0); }
void plrui_linha_foco(GfxRect r,float raio,float a) { plrui_cartao(r,raio,a); }
void plrui_pilula_foco(GfxRect r,float a) { plrui_cartao(r,0,a); }
void plrui_botao_repouso(GfxRect r,float a) { plrui_cartao(r,0,a); }
int plrui_tinta(void) { return 245; }
float ajustes_raio_poster_px(void) { return 20; }
int ajustes_relogio_12h(void) { return 0; }
int ajustes_idioma(void) { return IDIOMA_EN; }
TxtLinha txt_linha(TxtEstilo e,const char *s,int r,int g,int b,int a) {
  (void)e;(void)r;(void)g;(void)b;(void)a;return (TxtLinha){0,(int)strlen(s)*12,24,0,0};
}
TxtLinha txt_linha_corta(TxtEstilo e,const char *s,int r,int g,int b,int a,float w) {
  assert(w>0);TxtLinha t=txt_linha(e,s,r,g,b,a);if(t.w>w)t.w=(int)w;return t;
}
void txt_desenhar(TxtLinha t,float x,float y) { (void)t;(void)x;(void)y; }
GLuint tex_obter_larg(const char *url,float w) { assert(url[0] && w>0);if(!imagemPronta)return 0;imagens++;if(strstr(url,"-low.jpg"))baixos++;if(strstr(url,"-high.jpg"))altos++;return 1; }
float tex_aspecto(const char *url) { (void)url;return aspectoTeste; }
int tex_marca_escura(const char *url) { (void)url;return marcaEscuraTeste; }
int tex_logo_tom_unico(const char *url) { (void)url;return tomUnicoTeste; }
void ponteiro_alvo(float x,float y,float w,float h,PonteiroFn f,PonteiroFn a,int i,int b) {
  (void)x;(void)y;(void)w;(void)h;(void)f;(void)a;(void)i;(void)b;
}
void ponteiro_alvo_faixa(float x,float y,float w,float h,float y0,float y1,PonteiroFn f,PonteiroFn a,int i,int b) {
  assert(w>0 && h>0 && x>=xLista && x+w<=xLista+larguraLista+0.1f);alvosGrade++;if(i>=FOCO_FAVORITO)favAlvos++;
  (void)y0;(void)y1;ponteiro_alvo(x,y,w,h,f,a,i,b);
}
void ponteiro_rolagem(PonteiroRolagemFn fn) { (void)fn; }
void teclado_desenhar(Uint32 a) { (void)a; }
static void tecla(SDL_Keycode k) { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;esportesui_evento(&e); }
static void soltar(SDL_Keycode k) { SDL_Event e={0};e.type=SDL_KEYUP;e.key.keysym.sym=k;esportesui_evento(&e); }
int main(void) {
  /* Missing, downloading and failed logos have a local, centered icon;
   * a recovered image replaces it without changing the card geometry. */
  GfxRect caixa={20,40,320,160};int ic=icones,draw=imagensDesenhadas;
  logo("",1,caixa);assert(icones==ic+1 && !strcmp(ultimoIcone,"esp_football"));
  assert(caixaIcone.x==100 && caixaIcone.y==40 && caixaIcone.w==160 && caixaIcone.h==160);
  logo(NULL,0,caixa);assert(icones==ic+2 && !strcmp(ultimoIcone,"esp_soccer"));
  imagemPronta=0;logo("https://art.example/missing.png",1,caixa);
  logo("https://art.example/missing.png",1,caixa);
  assert(icones==ic+4 && imagensDesenhadas==draw);
  imagemPronta=1;logo("https://art.example/recovered.png",1,caixa);
  assert(icones==ic+4 && imagensDesenhadas==draw+1);
  aspectoTeste=0;logo("https://art.example/invalid.png",1,caixa);
  assert(icones==ic+5 && imagensDesenhadas==draw+1);aspectoTeste=2;
  /* Carolina Panthers is classified dark despite its blue/grey details.
   * Preserve multitone artwork; only an actual monochrome mark may tint. */
  marcaEscuraTeste=1;tomUnicoTeste=0;
  logo("https://art.example/panthers.png",1,caixa);assert(modoLogoTeste==GFX_TEXTO);
  tomUnicoTeste=1;
  logo("https://art.example/black-mark.png",1,caixa);assert(modoLogoTeste==GFX_MARCA);
  tomUnicoTeste=-1;
  logo("https://art.example/unmeasured.png",1,caixa);assert(modoLogoTeste==GFX_TEXTO);
  marcaEscuraTeste=0;tomUnicoTeste=0;
  for(int m=0;m<esportes_modalidades_n();m++) {
    char esperado[80];snprintf(esperado,sizeof esperado,"esp_%s",esportes_modalidade(m)->id);
    logo("",m,caixa);assert(!strcmp(ultimoIcone,esperado));
    imagemPronta=0;logo("https://art.example/failed.png",m,caixa);imagemPronta=1;
    assert(!strcmp(ultimoIcone,esperado));
    EspEvento falta={0};snprintf(falta.modalidade,sizeof falta.modalidade,"%s",esportes_modalidade(m)->id);
    falta.nParticipantes=2;int antes=icones;
    linhaEvento(&falta,(GfxRect){0,0,470,294},0);
    assert(icones==antes+3 && !strcmp(ultimoIcone,esperado));
  }
  logo("",-1,caixa);assert(!strcmp(ultimoIcone,"menu_sports"));
  strcpy(eTeste.id,"event");strcpy(eTeste.nome,"Celtics at Magic");strcpy(eTeste.modalidade,"basketball");strcpy(eTeste.ligaId,"nba");
  eTeste.nParticipantes=2;eTeste.estado=1;eTeste.inicio=time(NULL)-60;
  strcpy(eTeste.participantes[0].nome,"Celtics");strcpy(eTeste.participantes[1].nome,"Magic");
  struct tm dataTeste={.tm_year=126,.tm_mon=9,.tm_mday=19,.tm_hour=13,.tm_min=30};char dataTexto[48];
  horario(mktime(&dataTeste),dataTexto,sizeof dataTexto);assert(!strncmp(dataTexto,"10/19 ",6));
  bordaMenu=120;medidas();float xFechado=xLista,wFechado=larguraLista;
  bordaMenu=420;medidas();assert(xLista==xFechado && larguraLista==wFechado);
  esportesui_iniciar();assert(pagina==ESP_TODOS && catInicio && nItens==3 && foco==FOCO_TODOS && nTopo()==5);
  esportesui_desenhar(1000);assert(!catPedido[0] && !catPedido[1] && !catPedido[2] && !pediuEventos);
  assert(nGrupos==2 && grupos[0].n==2 && grupos[1].n==1);
  tecla(SDLK_DOWN);assert(foco==codLinha(2,-1));tecla(SDLK_RIGHT);assert(foco==codLinha(0,-1));
  tecla(SDLK_DOWN);assert(foco==codLinha(1,-1));
  tecla(SDLK_UP);assert(foco==codLinha(2,-1));tecla(SDLK_UP);assert(foco==FOCO_TODOS);
  /* An open league restores the exact catalog query, focus and scroll on Back. */
  strcpy(consulta,"football");montar();foco=codLinha(2,-1);rolagem=70;
  pointerAtivar(foco,0);assert(pagina==ESP_EVENTOS && pediuEventos==1 && modalidade==1);
  tecla(SDLK_ESCAPE);assert(pagina==ESP_TODOS && modalidade==-1 && foco==codLinha(2,-1) && rolagem==70 && !strcmp(consulta,"football"));
  consulta[0]=0;rolagem=0;montar();pediuEventos=0;
  /* League switches use the ordinary settings rows, retaining disabled entries. */
  pointerTopoAgir(3,0);assert(pagina==ESP_LIGAS_CONFIG && nItens==3);
  pointerAtivar(0,0);assert(!catLigado[0]);esportesui_atualizar(.016f,1000);assert(nItens==3);
  tecla(SDLK_ESCAPE);assert(pagina==ESP_TODOS && nItens==2 && indices[0]==2);
  esportesui_desenhar(1000);assert(!catPedido[0] && !catPedido[1] && !catPedido[2]);
  esportesui_fechar();esportesui_iniciar();assert(pagina==ESP_TODOS && nItens==2 && !catLigado[0]);
  pointerTopoAgir(3,0);pointerAtivar(0,0);tecla(SDLK_ESCAPE);assert(nItens==3 && catLigado[0]);
  /* Sport chips filter league cards; only opening a league requests matches. */
  pointerModalidadeAgir(2,0);assert(pagina==ESP_LIGAS && nItens==1 && indices[0]==1);
  tecla(SDLK_DOWN);assert(foco==codLinha(1,-1));
  tecla(SDLK_RETURN);assert(pagina==ESP_EVENTOS && pediuEventos==1 && foco==0);soltar(SDLK_RETURN);
  int abertas=aberturasFonte;tecla(SDLK_RETURN);assert(aberturasFonte==abertas);soltar(SDLK_RETURN);
  assert(fonteAberta && aberturasFonte==abertas+1);esportesui_atualizar(.016f,1000);
  assert(nCanais==1 && !procurando && !visitasOcultas);
  const FonteCanal *c=&fontescanais_estado()->canais[0];assert(!strcmp(c->id,"match") && !strcmp(c->conta,"Account Two"));
  assert(c->inicio==eTeste.inicio && c->fim==eTeste.inicio+7200);
  assert(fontescanais_escolher(0,time(NULL)));fonteAberta=0;char id[128];assert(esportesui_pediu_canal(id,sizeof id) && !strcmp(id,"match"));
  /* Long hold and its release never open Sources or toggle a favorite. */
  abertas=aberturasFonte;tecla(SDLK_RETURN);esportesui_atualizar(.016f,1700);
  assert(pagina==ESP_DETALHE && nItens==2 && nTopo()==1 && esperaSoltura && !fonteAberta);
  tecla(SDLK_RETURN);soltar(SDLK_RETURN);assert(!esperaSoltura && !favorito && aberturasFonte==abertas);
  tecla(SDLK_RETURN);assert(favorito && aberturasFonte==abertas);soltar(SDLK_RETURN);tecla(SDLK_ESCAPE);
  assert(pagina==ESP_EVENTOS && foco==0);
  pointerDetalhe(0,0);assert(pagina==ESP_DETALHE && !fonteAberta);tecla(SDLK_ESCAPE);
  tecla(SDLK_ESCAPE);assert(pagina==ESP_LIGAS && modalidade==2 && foco==codLinha(1,-1));
  pointerAtivar(codLinha(1,-1),0);assert(pagina==ESP_EVENTOS && pediuEventos==2);
  pointerAtivar(0,0);assert(fonteAberta);stream_folha_canais_fechar();
  /* Combined team favorites remain above league rows, independent of switches. */
  timesFavTeste=2;nFavTeste=2;favTeste[0]=eTeste;favTeste[1]=eTeste;
  strcpy(favTeste[1].id,"soccer-event");strcpy(favTeste[1].nome,"Arsenal at Chelsea");strcpy(favTeste[1].modalidade,"soccer");
  pointerTopoAgir(3,0);assert(pagina==ESP_TODOS && modalidade==-1 && nItens==3 && foco==FOCO_FAVORITO && linhaFavoritos());
  esportesui_desenhar(1000);assert(favAlvos>=2);tecla(SDLK_RIGHT);assert(foco==FOCO_FAVORITO+1);
  trocarFav=1;esportesui_atualizar(.016f,1000);assert(foco==FOCO_FAVORITO);
  abertas=aberturasFonte;pointerAtivar(FOCO_FAVORITO,0);assert(fonteAberta && aberturasFonte==abertas+1);
  stream_folha_canais_fechar();pointerDetalhe(FOCO_FAVORITO,0);assert(pagina==ESP_DETALHE && detalheOrigem==ESP_TODOS);
  int pedidosAntes=pediuEventos;esportesui_atualizar(.016f,70000);assert(pediuEventos==pedidosAntes);tecla(SDLK_ESCAPE);
  tecla(SDLK_DOWN);assert(foco==codLinha(2,-1));tecla(SDLK_UP);assert(focoFavorito());tecla(SDLK_UP);assert(foco==FOCO_TODOS);
  nFavTeste=0;foco=FOCO_FAVORITO;montar();assert(focoLinha() && linhaFoco()==2);nFavTeste=2;
  /* Horizontal row gestures and the next vertical gesture use distinct offsets. */
  nv_layout_w=600;nv_layout_h=1100;foco=codLinha(2,-1);
  esportesui_desenhar(1000);PonteiroRolagem gesto={.fase=PONT_ROL_INICIO,.eixoY=0,.x=xLista+60,.y=topoLista+alturaFavoritos()+120};
  /* A touch-capable APK in saved TV mode keeps remote navigation and never
   * captures touch scrolling. Enabling Mobile takes effect on the same screen. */
  assert(!mobileTeste && !rolar(&gesto));
  float offsetTv=linhasRolagem[1];
  gesto.fase=PONT_ROL_MOVER;gesto.delta=-200;
  assert(!rolar(&gesto) && linhasRolagem[1]==offsetTv && rolagem==0);
  mobileTeste=1;gesto.fase=PONT_ROL_INICIO;
  assert(rolar(&gesto));gesto.fase=PONT_ROL_MOVER;gesto.delta=-200;assert(rolar(&gesto));assert(linhasRolagem[1]>0);
  float offsetAntes=linhasRolagem[1];gesto.fase=PONT_ROL_INICIO;gesto.eixoY=1;assert(rolar(&gesto));
  gesto.fase=PONT_ROL_MOVER;gesto.delta=-180;assert(rolar(&gesto));assert(rolagem>0 && linhasRolagem[1]==offsetAntes);
  mobileTeste=0;float offsetMobile=rolagem;
  assert(!rolar(&gesto) && rolagem==offsetMobile && !toquerol_livre(&toque));
  limparToque();assert(!catPedido[0] && !catPedido[1] && !catPedido[2]);
  /* League/team artwork and row pointer targets fit TV/phone and UI scaling. */
  strcpy(eTeste.participantes[0].logo,"https://art.example/celtics.png");strcpy(eTeste.participantes[1].logo,"https://art.example/magic.png");
  for(int i=0;i<2;i++){strcpy(favTeste[i].participantes[0].logo,eTeste.participantes[0].logo);strcpy(favTeste[i].participantes[1].logo,eTeste.participantes[1].logo);}
  for(int i=0;i<3;i++)snprintf(ligasCat[i].logo,sizeof ligasCat[i].logo,"https://art.example/league%d.png",i);
  const float tamanhos[][3]={{1920,1080,1},{1080,2300,1},{1920,1080,1.5},{1080,2300,1.5}};
  for(int i=0;i<4;i++) {
    nv_layout_w=tamanhos[i][0];nv_layout_h=tamanhos[i][1];escalaTeste=tamanhos[i][2];
    mobileTeste=nv_layout_h>nv_layout_w;
    esportesui_fechar();esportesui_iniciar();imagens=imagensDesenhadas=0;esportesui_desenhar(1000);assert(imagens && imagens==imagensDesenhadas);
  }
  pointerTopoAgir(2,0);assert(pagina==ESP_CONFIG);pointerAtivar(2,0);tecla(SDLK_ESCAPE);assert(nItens==1 && indices[0]==1);
  /* All three quality choices use ordinary touch/remote settings rows. */
  pointerTopoAgir(2,0);assert(pagina==ESP_CONFIG && indices[0]==-1);esportesui_desenhar(1000);
  pointerAtivar(0,0);assert(pagina==ESP_QUALIDADE && nItens==3 && nTopo()==1);esportesui_desenhar(1000);
  pointerAtivar(1,0);assert(qualidadeFundo==1);tecla(SDLK_ESCAPE);assert(pagina==ESP_CONFIG);
  tecla(SDLK_ESCAPE);assert(pagina==ESP_TODOS);
  esportesui_recursos("fixture/art");baixos=altos=0;esportesui_desenhar(1000);assert(baixos && !altos);
  qualidadeFundo=2;baixos=altos=0;esportesui_desenhar(1000);assert(altos && !baixos);
  qualidadeFundo=0;aparelhoFraco=1;esportesui_iniciar();baixos=altos=0;esportesui_desenhar(1000);assert(baixos && !altos);
  aparelhoFraco=0;esportesui_desenhar(1000);assert(!altos); /* Auto cannot flicker per frame. */
  esportesui_iniciar();baixos=altos=0;esportesui_desenhar(1000);assert(altos && !baixos);
  for(int i=0;i<16;i++)habilitados[i]=0;
  timesFavTeste=nFavTeste=0;esportesui_fechar();esportesui_iniciar();assert(!nItens);esportesui_desenhar(1000);
  FonteCanal futuro={0};strcpy(futuro.id,"future");futuro.inicio=time(NULL)+3600;
  fontescanais_iniciar("Future");fontescanais_publicar(&futuro,1,0);assert(fontescanais_escolher(0,time(NULL)));
  perfilTeste++;assert(!esportesui_pediu_canal(id,sizeof id) && !fontescanais_estado()->ativo);
  esportesui_fechar();puts("sports on-demand league cards, Back restoration, independent Favorites, touch/remote navigation and source handoff: passed");return 0;
}
