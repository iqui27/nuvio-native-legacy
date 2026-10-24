// The actual production source sheet with only platform boundaries stubbed.
// No SDL initialization, drawing, network request, resolver or TV benchmark.
#include "streams.h"
#include <unistd.h>
static uint64_t fakeNow = UINT64_C(1700000000000);
static uint64_t fixtureNow(void) { return fakeNow; }
#define streamfit_agora_ms fixtureNow
#define STREAMFIT_REDE_EXISTE 1 // Android behaviour; LG/TPK/WGT hide the no-network line
#include "stub_perfisessao.h"
#include "../src/streams.c"
#undef streamfit_agora_ms
#include <assert.h>
/* Parts/P2P are unavailable in this source-sheet-only fixture. */
const char *ajustes_p2p_url(void) { return ""; }
int partes_disponivel(void) { return 0; }
const PartesLista *partes_sessao_lista(void) { return NULL; }
void partes_lista_limpar(PartesLista *l) { if(l)memset(l,0,sizeof *l); }
int partes_lista_copiar(PartesLista *d,const PartesLista *o) { (void)d;(void)o;return 0; }
int partes_hashes_texto(const char *s,char out[][41],int max) { (void)s;(void)out;(void)max;return 0; }
void partes_pedir(const Stream *s,const char *t) { (void)s;(void)t; }
void partes_pedir_cedo(const Stream *s,const char *t) { (void)s;(void)t; }
void partes_pedir_item(const Stream *s,int i) { (void)s;(void)i; }
int partes_resultado(PartesLista *l) { (void)l;return PARTES_VAZIO; }
void partes_aquecer(void) {}
int partes_airlock_estado(int *l,char *d,unsigned n) { (void)l;(void)d;(void)n;return -2; }
void partes_airlock_pedir(int u,int i,int l) { (void)u;(void)i;(void)l; }
int debrid_tb_disponivel(void) { return 0; }
void partes_cancelar(void) {}
void ctxhold_cancelar(CtxHold *h) { memset(h,0,sizeof *h); }
int ctxhold_passo(CtxHold *h,Uint32 t,int f) { (void)h;(void)t;(void)f;return 0; }
int ctxhold_evento(CtxHold *h,const SDL_Event *e,int c) { (void)h;(void)e;(void)c;return CTXH_NADA; }
float txt_bloco(TxtEstilo e,const char *s,int r,int g,int b,float x,float y,float w,float lh,float alpha,int n) {
 (void)e;(void)r;(void)g;(void)b;(void)x;(void)y;(void)w;(void)n;(void)alpha;return s&&s[0]?lh:0;
}
static int streamfitpassiva_test_ok(int v) { return v; }

Uint32 SDL_GetTicks(void) { return (Uint32)fakeNow; }
float gfx_escala_ui(void) { return 1; }
const char *i18n(const char *s) { return s; }
int debrid_ativo(void) { return 0; }
int p2p_ativo(void) { return 0; }
int selospacote_ativo(void) { return -1; }
unsigned selospacote_versao(void) { return 1; }
int selospacote_casar(const char *const *p,int n,unsigned short *o,int m) { (void)p;(void)n;(void)o;(void)m;return 0; }
const SeloFiltro *selospacote_filtro(unsigned short id) {
  (void)id;
  abort(); // Badge packs are disabled; an attempted lookup is unexpected.
}
// selosAgendar (chamado por stream_definirir_lista) puxa os tres abaixo; sem
// os stubs eles resolvem para NULL (dynamic_lookup) e o teste morre antes da
// primeira linha — ja quebrava no HEAD, sem relacao com #205.
void selospacote_colorido(int ligado) { (void)ligado; }
int selospacote_embutidos_ok(void) { return 0; }
int ajustes_selos_coloridos(void) { return 0; }
static const char *qualidadeTeste = "Automatica";
static int prioridadeTeste;
const char *ajustes_qualidade(void) { return qualidadeTeste; }
int ajustes_fonte_texto_addon(void) { return 0; }
// Externos que o caminho de montar/automatico puxa desde o #402/#409: sem
// stub, dynamic_lookup os deixa NULL e a folha morre antes da primeira
// assert — o sheet ja quebrava no HEAD, sem relacao com #205.
int ajustes_dolby_atmos(void) { return 0; }
int ajustes_dolby_vision(void) { return 0; }
int ajustes_dv_mkv(void) { return 0; }
int ajustes_fonte_escopo(void) { return 0; }
int ajustes_fonte_hdr(void) { return 0; }
int ajustes_fonte_manual(void) { return 0; }
int ajustes_fonte_ordem_addon(void) { return 0; }
int ajustes_fonte_prioridade(void) { return prioridadeTeste; }
int ajustes_fonte_regex_modo(void) { return 0; }
int ajustes_fonte_usar_outros(void) { return 0; }
int ajustes_fonte_primeira(void) { return prioridadeTeste == 3; }
int ajustes_fonte_ordem_uso(void) { return 0; }
int ajustes_fonte_idioma_audio(void) { return 0; }
int fonteregra_ordem_rank(const char *nome) { (void)nome; return 0; }
int ajustes_idioma(void) { return 0; }
int ajustes_tamanho_max_gb(void) { return 0; }
int ajustes_tamanho_min_gb(void) { return 0; }
unsigned fonteregra_versao(void) { return 0; }
int fonteregra_grupo(const FonteRegraCfg *c, const char *nome, int plugin, const char *texto)
  { (void)c;(void)nome;(void)plugin;(void)texto;return 0; }
int naovideo_nome(const char *rotulo, const char *descricao, int altura, long tamanhoMB)
  { (void)rotulo;(void)descricao;(void)altura;(void)tamanhoMB;return 0; }
int video_pode_forcar_sdr(void) { return 0; }
void video_forcar_sdr(void) {}
// No persisted failed sources in this sheet fixture (24185059).
long fontesmortas_idade(const Stream *s) { (void)s; return -1; }
int player_aberto(void) { return 0; }
int ajustes_relogio_ligado(void) { return 0; }
int ilha_rect(float *x, float *y, float *w, float *h) {
  (void)x; (void)y; (void)w; (void)h;
  return 0;
}
// The sheet owns only a token: playback itself stays outside this fixture.
static unsigned sourceContext;
static int sourceReloads;
unsigned player_recarregar_contexto(void) { return sourceContext; }
int player_recarregar_fonte(unsigned contexto) {
  assert(contexto && contexto == sourceContext);
  sourceReloads++;
  return 1;
}
void plrui_decimal(char *s) { (void)s; }
static char lastLookup[64]; static int lookups; static double lookupValue;
static double catalogRuntime(const char *alvo) {
  snprintf(lastLookup,sizeof lastLookup,"%s",alvo); lookups++;
  return !strcmp(alvo,"tt3") ? lookupValue : 0; // answers only for its own id
}
void ondever_apps_atualizar(void) {}
int ondever_n(const char *id) { (void)id;return 2; }
int ondever_item(const char *id,int ix,OndeVer *o) { (void)id;(void)ix;(void)o;return 0; }
int ondever_abrir(const char *nome) { (void)nome;return ONDE_INFO; }
TxtLinha txt_linha(TxtEstilo estilo,const char *s,int r,int g,int b,int a) {
  (void)estilo;(void)r;(void)g;(void)b;(void)a;TxtLinha l={0};l.w=(int)strlen(s)*8;return l;
}
static Stream source(int id,long mb,uint64_t exact,int h) {
  Stream s={0};s.altura=h;s.mp4=1;s.fileIdx=-1;s.tamanhoMB=mb;s.tamanhoBytes=exact;
  snprintf(s.url,sizeof s.url,"https://media.invalid/%d.mp4",id);
  snprintf(s.provedor,sizeof s.provedor,"Addon");snprintf(s.rotulo,sizeof s.rotulo,"Source %d",id);
  snprintf(s.arquivo,sizeof s.arquivo,"file-%d.mp4",id);return s;
}
static uint64_t sizeAt(int kbps) { return (uint64_t)kbps*1000*7200/8; }
static void expect(const int *expected,int count) {
  assert(nOrdem==count);for(int i=0;i<count;i++) assert(ordem[i]==expected[i]);
}
static void sourceKey(Uint32 type, SDL_Keycode key, int repeat) {
  SDL_Event event = {0};
  event.type = type; event.key.keysym.sym = key; event.key.repeat = repeat;
  stream_folha_evento(&event);
}
static void sourceAction(int button) {
  stream_folha_abrir();
  grupo = -1;
  for (foco = 0; foco < nBotoes(); foco++) if (botaoDe(foco) == button) return;
  assert(!"source action is missing");
}
static void playbackActions(void) {
  int selected = -1;
  // No current pipeline: the existing refresh remains available and separate.
  sourceContext = 0; sourceReloads = 0;
  sourceAction(BT_RECARREGAR);
  sourceKey(SDL_KEYDOWN, SDLK_RETURN, 0);
  sourceKey(SDL_KEYUP, SDLK_RETURN, 0);
  assert(stream_folha_recarregar() && !stream_folha_recarregar());
  assert(!sourceReloads && stream_folha_aberta());

  sourceContext = 17; sourceAction(BT_RECARREGAR_FONTE);
  sourceKey(SDL_KEYDOWN, SDLK_RETURN, 0);
  sourceKey(SDL_KEYDOWN, SDLK_RETURN, 0); // firmware repeats without repeat=1
  sourceKey(SDL_KEYDOWN, SDLK_RETURN, 1);
  assert(!sourceReloads && stream_folha_aberta());
  sourceKey(SDL_KEYUP, SDLK_KP_ENTER, 0); // only the matching release commits
  assert(!sourceReloads);
  sourceKey(SDL_KEYUP, SDLK_RETURN, 0);
  assert(sourceReloads == 1 && !stream_folha_aberta());
  assert(!stream_folha_escolheu(&selected) && selected == -1 && !stream_folha_recarregar());

  sourceAction(BT_RECARREGAR_FONTE);
  sourceKey(SDL_KEYDOWN, SDLK_RETURN, 0);
  sourceContext = 18; // the player changed source while the sheet was open
  sourceKey(SDL_KEYUP, SDLK_RETURN, 0);
  assert(sourceReloads == 1 && stream_folha_aberta());
  assert(!stream_folha_escolheu(&selected) && selected == -1 && !stream_folha_recarregar());

  sourceAction(BT_RECARREGAR_FONTE);
  sourceKey(SDL_KEYDOWN, SDLK_RETURN, 0);
  sourceKey(SDL_KEYDOWN, SDLK_ESCAPE, 0);
  sourceKey(SDL_KEYUP, SDLK_RETURN, 0);
  assert(sourceReloads == 1 && !stream_folha_aberta());
  sourceContext = 0;
  puts("source playback actions: PASS (refresh, release, repeat, stale token, Back)");
}
static void liveTvActions(void) {
  int oldMp4 = soMp4, oldCache = soCache, oldDub = soDub;
  char dir[] = "/tmp/nuvio-live-filters-XXXXXX", path[160];
  assert(mkdtemp(dir)); fontefalha_dir(dir);
  FonteFiltroConfig original, blocked = {0}; fontefalha_filtro(&original);
  for (int c = 0; c < FONTEFILTRO_N; c++)
    blocked.excluir[c] = (UINT64_C(1) << fontefiltro_opcoes_n(c)) - 1;
  blocked.maxGB = 1;
  assert(fontefalha_filtro_definir(&blocked));
  assert(!stream_filtro_permite(&lista[0]) && stream_filtro_bloqueou());
  int sourceMp4, sourceOutsideCache;
  assert(n > 0);
  sourceMp4 = lista[0].mp4; sourceOutsideCache = lista[0].foraCache;
  stream_folha_canal(1);
  stream_filtro_tipo("arbitrary-live-resource");
  stream_definir_lista(lista, n);
  assert(stream_filtro_permite(&lista[0]) && !stream_filtro_bloqueou());
  soMp4 = soCache = soDub = 1;
  assert(nBotoes() == 2);
  assert(botaoDe(0) == BT_RECARREGAR && botaoDe(1) == BT_FECHAR);
  for (int i = 0; i < nBotoes(); i++)
    assert(botaoDe(i) != BT_SO_MP4 && botaoDe(i) != BT_CACHE && botaoDe(i) != BT_DUB);
  lista[0].mp4 = 0;
  lista[0].foraCache = 1;
  assert(passaChips(0));
  assert(stream_automatico_canal() >= 0);
  stream_folha_canal(0);
  stream_filtro_tipo("movie");
  stream_definir_lista(lista, n);
  assert(!stream_filtro_permite(&lista[0]) && stream_filtro_bloqueou());
  assert(!passaChips(0));
  // A movie-labelled live addon bypasses every exclusion and quick chip;
  // another addon in the same list remains filtered.
  char key[FONTEFALHA_CHAVE];
  fontefalha_addon_chave("https://live.invalid/manifest.json", key, sizeof key);
  snprintf(lista[0].addonChave, sizeof lista[0].addonChave, "%s", key);
  assert(fontefalha_addon_definir_ao_vivo(key, 1));
  stream_filtro_atualizar();
  assert(stream_filtro_permite(&lista[0]) && passaChips(0));
  if (n > 1) assert(!stream_filtro_permite(&lista[1]) && !passaChips(1));
  stream_definir_lista(lista, n);
  assert(stream_filtro_permite(&lista[0]) && !stream_filtro_bloqueou());
  assert(fontefalha_addon_definir_ao_vivo(key, 0));
  stream_filtro_atualizar();
  assert(!stream_filtro_permite(&lista[0]) && !passaChips(0));
  lista[0].addonChave[0] = 0;
  assert(fontefalha_filtro_definir(&original));
  snprintf(path, sizeof path, "%s/fontefalha.txt", dir); assert(!unlink(path));
  assert(!rmdir(dir)); fontefalha_dir("");
  lista[0].mp4 = sourceMp4; lista[0].foraCache = sourceOutsideCache;
  soMp4 = oldMp4; soCache = oldCache; soDub = oldDub;
  puts("live TV source actions: PASS (no movie filters, channel source remains visible)");
}
static void addonOrder(void) {
  Stream sources[4] = {source(0,50000,0,720),source(1,10000,0,2160),
                       source(2,70000,0,1080),source(3,1000,0,1080)};
  sources[0].foraCache = 1;
  prioridadeTeste = 3; qualidadeTeste = "1080p";
  stream_definir_alvo("tt-addon-order");stream_definir_lista(sources,4);
  stream_folha_abrir();
  const int raw[] = {0,1,2,3};expect(raw,4);
  // The first playable result wins even above the soft resolution preference;
  // the cached-negative row is displayed in place but is not auto-played.
  assert(automaticaDaFolha() == 1 && stream_automatico() == 1);
  stream_automatico_excluir(1);
  assert(automaticaDaFolha() == 2); // Genuine failure advances in original order.
  montar(automaticaDaFolha());expect(raw,4); // No quality regrouping after failure.
  prioridadeTeste = 0; qualidadeTeste = "Automatica";
  puts("add-on order sheet: PASS (raw order, matching badge, soft cap, next link)");
}
static void threeSourceExample(void) {
  Stream sources[3] = {source(0,20000,UINT64_C(20)<<30,2160),
                       source(1,30000,UINT64_C(30)<<30,2160),
                       source(2,2000,UINT64_C(2)<<30,1080)};
  sources[1].badges=badges_bit("v-hdr10");sources[1].dolbyVision=1;
  strcpy(sources[1].rotulo,"4K DV HDR10");
  streamfit_limpar();stream_definir_alvo("tt618-example");
  stream_definir_lista(sources,3);
  const int ranked[] = {1,1,2};
  for(int i=0;i<3;i++) {
    prioridadeTeste=i;
    assert(stream_automatico()==ranked[i]);
  }
  prioridadeTeste=3;stream_folha_abrir();
  const int raw[]={0,1,2};expect(raw,3);
  for(int i=0;i<3;i++) {
    assert(stream_automatico()==i && automaticaDaFolha()==i);
    stream_automatico_excluir(i);
  }
  assert(stream_automatico()==-1);
  prioridadeTeste=0;
  puts("AIOStreams example: PASS (Balanced=2, Maximum quality=2, Quick start=3; From addon=1 then2 then3 on failure)");
}
int main(void) {
  Stream sources[5]={source(0,50000,sizeAt(50000),2160),source(1,10000,sizeAt(10000),2160),
    source(2,10000,sizeAt(10000),2160),source(3,70000,0,2160),source(4,80000,sizeAt(50000),1080)};
  int speed[8]={20000,20000,20000,20000,20000,20000,20000,20000};
  streamfit_limpar();stream_definir_alvo("tt1");stream_fit_duracao("tt1",7200,SF_DUR_METADATA);
  stream_definir_lista(sources,5);stream_folha_abrir();
  const int original[]={0,3,1,2,4};expect(original,5);
  assert(stream_fit_folha_estado(0,NULL)==SF_DESCONHECIDA);
  streamfit_rede(1);assert(streamfit_diagnostico(1,"https://media.invalid/probe",speed,8,fakeNow)==8);
  montar(automaticaDaFolha());expect(original,5); // open sheet freezes unknown
  stream_folha_abrir();const int learned[]={3,1,2,0,4};expect(learned,5);
  assert(stream_fit_folha_estado(0,NULL)==SF_PESADA && stream_fit_folha_estado(3,NULL)==SF_DESCONHECIDA);
  // A 0 foi medida pesada (diagnostico acima) e o automatico a rebaixa: a
  // escolha crua fica com a 3 (evidencia de lancamento, fbc26a071). A
  // sonda de 200 Mbps logo abaixo absolve a 0, e a "Melhor para esta TV"
  // volta a ser ela — por isso o incremento comeca na 0 (e o foco, preso na
  // carta 2, nao acompanha o melhor: a folha so pula para o melhor ao ABRIR).
  assert(stream_n()==5 && stream_folha_n()==5 && stream_automatico()==3); // autoplay respects the measured penalty
  int focused=2;foco=linhaDe(focused);grupo=1;
  fakeNow++;int faster[8]={200000,200000,200000,200000,200000,200000,200000,200000};
  assert(streamfit_diagnostico(1,"https://media.invalid/probe",faster,8,fakeNow)==8);
  stream_fit_duracao("tt1",14400,SF_DUR_MEDIA); // cannot mutate an open sheet
  montar(automaticaDaFolha());expect(learned,5);assert(filtrado(foco)==focused);
  Stream late=source(5,20000,sizeAt(40000),2160);
  stream_lista_acrescentar(&late,1,0);
  stream_folha_atualizar(.016f,0);
  const int incremental[]={3,1,2,0,5,4};expect(incremental,6);
  assert(filtrado(foco)==focused && stream_fit_folha_estado(5,NULL)==SF_PESADA);
  // A service tab never enters the source partition or source-index space.
  filtro=-1;montar(automaticaDaFolha());const int services[]={-2,-3};expect(services,2);
  filtro=0;montar(automaticaDaFolha());expect(incremental,6);
  // #205: uma carta "fora do cache" fica no FIM do proprio grupo de qualidade,
  // nao despejada depois de todos os grupos. E a carta em foco que acaba de
  // virar fora-do-cache leva o foco junto (foraCacheFoco, como verificarUma faz).
  lista[2].foraCache=1;
  { const int regra[]={3,1,0,5,2,4};montar(automaticaDaFolha());expect(regra,6); }
  // O foco segue a carta: a virada chega como chega de verdade — o fio marca
  // foraCache e o indice, sem montar no meio — e a atualizacao prende o foco
  // pelo indice com o Y do layout de ANTES.
  lista[2].foraCache=0;montar(automaticaDaFolha());expect(incremental,6);
  lista[2].foraCache=1;
  atomic_store(&foraCacheFoco,2);
  stream_folha_atualizar(.016f,0);
  assert(filtrado(foco)==2); // o foco seguiu a carta para o lugar novo
  lista[2].foraCache=0;montar(automaticaDaFolha());expect(incremental,6);
  foco=linhaDe(2); // a secao seguinte parte do foco na carta 2, como antes
  stream_atualizar_lista(sources,5);assert(filtrado(foco)==focused);
  { const int reaberta[]={3,1,2,0,4};expect(reaberta,5); } // o melhor (3) volta como primeira linha
  stream_folha_abrir();assert(stream_fit_folha_estado(0,NULL)==SF_ADEQUADA);
  // Real media duration has precedence over delayed metadata.
  stream_fit_duracao("tt1",1,SF_DUR_METADATA);stream_folha_abrir();assert(fitFotoSeg==14400);
  stream_definir_alvo("tt2:1:1");stream_definir_lista(sources,5);stream_folha_abrir();
  assert(stream_fit_folha_estado(0,NULL)==SF_DESCONHECIDA); // episode has no runtime
  stream_fit_duracao("tt2:1:1",NAN,SF_DUR_MEDIA);stream_folha_abrir();assert(fitFotoSeg==0);
  streamfit_rede(2);stream_folha_abrir();assert(stream_fit_folha_estado(0,NULL)==SF_DESCONHECIDA);
  // --- F03 runtime provenance per exact target ------------------------------
  streamfit_rede(3);int pspeed[8]={8000,8000,8000,8000,8000,8000,8000,8000};
  assert(streamfitpassiva_test_ok(streamfit_passiva(3,"https://media.invalid",pspeed,8,fakeNow)==8));
  stream_fit_fonte_metadados(catalogRuntime);
  // Catalog lookup is asked for the EXACT target and only when nothing was pushed.
  lookupValue=7200;stream_definir_alvo("tt3");stream_definir_lista(sources,5);stream_folha_abrir();
  assert(!strcmp(lastLookup,"tt3") && fitFotoSeg==7200 && fitFotoOrigem==SF_DUR_METADATA);
  stream_definir_alvo("tt3:1:2");stream_definir_lista(sources,5);stream_folha_abrir();
  assert(!strcmp(lastLookup,"tt3:1:2") && fitFotoSeg==0); // episode never borrows the title's
  // Pushed metadata (TMDB sheet) beats the catalog; media beats both; the
  // producers of different targets do not evict each other.
  stream_definir_alvo("tt3");stream_fit_duracao("tt3",6000,SF_DUR_METADATA);
  stream_fit_duracao("tt9",5400,SF_DUR_METADATA);
  int before=lookups;stream_definir_lista(sources,5);stream_folha_abrir();
  assert(fitFotoSeg==6000 && lookups==before);
  stream_fit_duracao("tt3",6600,SF_DUR_MEDIA);stream_fit_duracao("tt3",9000,SF_DUR_METADATA);
  stream_folha_abrir();assert(fitFotoSeg==6600 && fitFotoOrigem==SF_DUR_MEDIA);
  // A new file clears ONLY the media duration; metadata of the target stays.
  stream_fit_duracao("tt3",0,SF_DUR_MEDIA);stream_folha_abrir();assert(fitFotoSeg==9000);
  stream_fit_duracao("tt3",0,SF_DUR_DESCONHECIDA);lookupValue=0;stream_folha_abrir();
  assert(fitFotoSeg==0 && fitFotoOrigem==SF_DUR_DESCONHECIDA);
  stream_definir_alvo("tt9");stream_definir_lista(sources,5);stream_folha_abrir();assert(fitFotoSeg==5400);
  for(int k=0;k<20;k++){char id[16];snprintf(id,sizeof id,"tt%d",100+k);stream_fit_duracao(id,3000,SF_DUR_METADATA);}
  stream_folha_abrir();assert(fitFotoSeg==0); // bounded store: the oldest target is evicted, never mixed
  // --- F03 sheet text: origin, age, sustained, budget vs demand; explicit unknown
  stream_definir_alvo("tt3");stream_fit_duracao("tt3",3600,SF_DUR_MEDIA);
  Stream fitSources[3]={source(10,1000,(uint64_t)5000*1000*3600/8,1080),source(11,1000,(uint64_t)7000*1000*3600/8,1080),source(12,1000,0,1080)};
  snprintf(fitSources[2].url,sizeof fitSources[2].url,"https://resolver.invalid/resolve/x");fitSources[2].tamanhoBytes=1000000;
  fakeNow+=5*60000;stream_definir_lista(fitSources,3);stream_folha_abrir();
  char txt[256];
  assert(fitTexto(0,txt,sizeof txt)==SF_ADEQUADA);
  assert(!strcmp(txt,"Reprodução recente · há 5 min · 8.0 Mbps sustentados · precisa ~5.0 de 6.0 Mbps disponíveis"));
  assert(fitTexto(1,txt,sizeof txt)==SF_PESADA && fitPesada(1) && !fitPesada(0));
  assert(fitTexto(2,txt,sizeof txt)==SF_DESCONHECIDA && !strcmp(txt,"Conexão: servidor ainda não medido"));
  // Heavy rows keep their place inside the group (moved after the adequate/unknown).
  { int seen0=linhaDe(0),seen1=linhaDe(1),seen2=linhaDe(2); assert(seen0>=0 && seen2>=0 && seen1>seen0 && seen1>seen2); }
  // The focused row opens the connection line even when there is no file name.
  { int r=linhaDe(2); lista[2].arquivo[0]=0; assert(!arquivoDa(&lista[2])[0]);
    foco=r; grupo=1; abreFoco=1; nOrdem=r; float h=alturaLinha(2,-1);
    assert(h>=FOLHA_LINHA_H+FOLHA_FIT_H-.01f); nOrdem=r+1; float o=alturaLinha(2,-1);
    assert(o<FOLHA_LINHA_H+FOLHA_MARCA_H+.01f); montar(automaticaDaFolha()); }
  stream_fit_duracao("tt3",0,SF_DUR_DESCONHECIDA);stream_folha_abrir();
  assert(fitTexto(0,txt,sizeof txt)==SF_DESCONHECIDA && !strcmp(txt,"Conexão: duração do título desconhecida"));
  fitSources[0].tamanhoBytes=0;stream_definir_lista(fitSources,3);stream_fit_duracao("tt3",3600,SF_DUR_MEDIA);stream_folha_abrir();
  assert(fitTexto(0,txt,sizeof txt)==SF_DESCONHECIDA && !strcmp(txt,"Conexão: tamanho do arquivo não informado"));
  streamfit_rede(0);stream_folha_abrir();
  assert(fitTexto(1,txt,sizeof txt)==SF_DESCONHECIDA && !strcmp(txt,"Conexão: sem dados desta rede"));
  stream_fit_fonte_metadados(NULL);
  // The separate PRIMEIRA queue keeps the add-on's order and one probe.
  long points[3]={1,99,99};int queue[3];unsigned char excluded[3]={0};
  assert(fonteauto_fila(FONTEAUTO_PRIMEIRA,3,-1,points,NULL,excluded,3,queue)==3);
  assert(queue[0]==0 && queue[1]==1 && queue[2]==2 && fonteauto_tentativas(FONTEAUTO_PRIMEIRA,9)==1);
  liveTvActions();
  playbackActions();
  addonOrder();
  threeSourceExample();
  stream_definir_lista(NULL,0);free(ordem);free(linhaY);free(linhaH);free(grupoTmp);free(grupoFitTmp);free(fitResultados);free(fitClasses);
  puts("streamfit sheet: PASS (stable groups, raw identities, freeze, incremental/focus, OndeVer, first-source)");return 0;
}
