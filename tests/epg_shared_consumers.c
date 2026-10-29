#include "stub_perfisessao.h"
#include "../src/guia.c"
#include <assert.h>

// Compile the production player resolver alongside the real guide and EPG.
static int epgIdx = -1;
static unsigned epgIdxRev;
static CatItem itemCanal;
const char *player_id_canal(void) { return "xtream:@00000001:7"; }
#include "player_epg.inc"

int perfis_ativo(void) { return 3; }
int ajustes_guia_fonte(void) { return 0; }
Uint32 SDL_GetTicks(void) { return 0; }
int xtream_e_id(const char *id) { return id && !strncmp(id,"xtream:",7); }
void xtepg_querer(const char *id) { (void)id; }
int xtepg_tem(const char *id) { (void)id;return 0; }
int xtepg_estado(const char *id) { (void)id;return XTEPG_FALHOU; }
int xtepg_agora(const char *id,time_t t,EpgProg *p) { (void)id;(void)t;(void)p;return 0; }
int xtepg_faixa_desde(const char *id,time_t de,time_t ate,int pular,EpgProg *out,int cap) {
  (void)id;(void)de;(void)ate;(void)pular;(void)out;(void)cap;return 0;
}

void epg_shared_consumers(void) {
  GCanal c = {.id=player_id_canal(),.nome="News HD",.epgId="news",.tvgNome="",.epg=-1};
  EpgProg p;
  nLtvF=1;strcpy(ltvF[0].chave,"xta00000001");
  assert(gEstadoGrade(&c)==0);
  assert(c.epg==0 && gAgora(&c,time(NULL),&p) && !strcmp(p.titulo,"Account two"));
  strcpy(itemCanal.titulo,c.nome);pEpgAtualizar();
  assert(epgIdx==0 && epg_agora(epgIdx,time(NULL),&p) && !strcmp(p.titulo,"Account two"));
  c.epg=-1;canais=&c;nCanais=1;
  buscaFazer("Account two");buscaPasso();
  assert(!buscaPendente.ativa && buscaNP==1 && !strcmp(buscaProg[0].titulo,"Account two"));
  c.nome=c.epgId="Unmatched";c.epg=-1;
  assert(gEstadoGrade(&c)==-3);
  strcpy(itemCanal.titulo,c.nome);epgIdx=-1;pEpgAtualizar();assert(epgIdx<0);
  canais=NULL;nCanais=0;
}
