/* Exercise the actual shared Sources sheet's channel mode. No SDL runtime. */
#include "../src/streams.c"
#include <assert.h>
static uint64_t perfilTeste=1;
uint64_t perfis_sessao_geracao(void) { return perfilTeste; }
int perfis_sessao_atual(uint64_t g) { return g==perfilTeste; }
int video_pode_forcar_sdr(void) { return 0; }
int player_aberto(void) { return 0; }
int ajustes_relogio_ligado(void) { return 0; }
static int relogio12,idiomaTeste=IDIOMA_EN;
int ajustes_idioma(void) { return idiomaTeste; }
int ajustes_relogio_12h(void) { return relogio12; }
int ilha_rect(float *x,float *y,float *w,float *h) { (void)x;(void)y;(void)w;(void)h;return 0; }
const char *i18n(const char *s) { return s; }
static void teclaCanal(SDL_Keycode k) { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;canaisEvento(&e); }
static void horarios(void) {
  setenv("TZ","UTC0",1);tzset();
  struct tm t={.tm_year=126,.tm_mon=9,.tm_mday=19,.tm_hour=13,.tm_min=30};
  time_t inicio=mktime(&t);FonteCanal c={.inicio=inicio,.fim=inicio+7200};char s[64];
  assert(!canaisHorario(&c,inicio-3600,s,sizeof s) && !strcmp(s,"13:30"));
  assert(canaisHorario(&c,inicio,s,sizeof s) && !strcmp(s,"Ao vivo"));
  assert(canaisHorario(&c,c.fim-1,s,sizeof s));
  assert(!canaisHorario(&c,c.fim,s,sizeof s) && !strcmp(s,"13:30"));
  relogio12=1;
  assert(!canaisHorario(&c,inicio-3600,s,sizeof s) && !strcmp(s,"1:30 PM"));
  assert(!canaisHorario(&c,inicio-86400,s,sizeof s) && !strcmp(s,"10/19 1:30 PM"));
  relogio12=0;
  assert(!canaisHorario(&c,inicio-86400,s,sizeof s) && !strcmp(s,"10/19 13:30"));
  idiomaTeste=IDIOMA_PT;assert(!canaisHorario(&c,inicio-86400,s,sizeof s) && !strcmp(s,"19/10 13:30"));idiomaTeste=IDIOMA_EN;
  c.fim=0;assert(!canaisHorario(&c,inicio+60,s,sizeof s)); /* No invented Live state. */
  c.inicio=0;assert(!canaisHorario(&c,inicio,s,sizeof s) && !s[0]);
}
int main(void) {
  horarios();
  Stream tocando={0};strcpy(tocando.url,"https://playback.example/existing.m3u8");
  lista=&tocando;n=1;atual=0;listaGeracao=42;canalFolha=0;
  strcpy(alvoLista,"existing");strcpy(alvoPedido,"existing");
  stream_folha_canais_abrir("Selected event");assert(stream_folha_canais_aberta());
  assert(lista==&tocando && n==1 && atual==0 && listaGeracao==42 && !canalFolha);
  assert(!strcmp(alvoPedido,"existing") && !strcmp(alvoLista,"existing"));
  assert(nBotoes()==2 && botaoDe(0)==BT_RECARREGAR && botaoDe(1)==BT_FECHAR);
  FonteCanal cs[2]={0};strcpy(cs[0].id,"first");strcpy(cs[1].id,"second");
  cs[0].inicio=time(NULL)-60;cs[1].inicio=time(NULL)+3600;
  fontescanais_publicar(cs,2,0);
  teclaCanal(SDLK_DOWN);assert(foco==1);teclaCanal(SDLK_RETURN);
  assert(!aberta); /* Upcoming event: tune its live channel now. */
  char id[128];assert(fontescanais_consumir(id,sizeof id) && !strcmp(id,"second"));
  stream_folha_canais_abrir("Current event");fontescanais_publicar(cs,2,0);
  canaisEscolher(0,0);assert(!aberta); /* Same callback used by touch. */
  assert(fontescanais_consumir(id,sizeof id) && !strcmp(id,"first"));
  assert(!fontescanais_consumir(id,sizeof id));
  stream_folha_canais_abrir("Another event");assert(!fontescanais_estado()->n);
  teclaCanal(SDLK_r);assert(fontescanais_pediu_atualizar() && !fontescanais_pediu_atualizar());
  teclaCanal(SDLK_ESCAPE);assert(!stream_folha_canais_aberta());
  assert(lista==&tocando && n==1 && atual==0 && listaGeracao==42);
  assert(!strcmp(lista[0].url,"https://playback.example/existing.m3u8"));
  stream_folha_canais_fechar();assert(!fontescanais_estado()->ativo);
  puts("shared Sources sheet: channel choices, cancel/reload and active playback ownership passed");return 0;
}
