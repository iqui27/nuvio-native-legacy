// Real guide search; deterministic EPG and clock, without a window or network.
#include "../src/guia.c"
#include <assert.h>
static Uint32 tick, custo;
static int chamadas, paginasPorCanal=1, semResultado;
static unsigned revisao;
Uint32 SDL_GetTicks(void) { return tick; }
unsigned epg_revisao(void) { return revisao; }
int epg_canal_estado(const char *id) { (void)id;return EPG_PRONTO; }
int epg_match_canal(const char *id,const char *epgId,const char *nome) { (void)id;(void)epgId;(void)nome;return 0; }
int xtream_e_id(const char *id) { (void)id;return 0; }
void xtepg_querer(const char *id) { (void)id; }
int xtepg_faixa_desde(const char *id,time_t de,time_t ate,int pular,EpgProg *out,int cap) {
  (void)id;(void)de;(void)ate;(void)pular;(void)out;(void)cap;assert(0);return 0;
}
int epg_faixa_desde(int canal,time_t de,time_t ate,int pular,EpgProg *out,int cap) {
  (void)ate;chamadas++;tick+=custo;
  int n=paginasPorCanal==1?1:(pular<24*(paginasPorCanal-1)?24:1);
  if(n>cap)n=cap;
  for(int i=0;i<n;i++) out[i]=(EpgProg){.ini=de+(canal%2?300:-100),.fim=de+600,
      .titulo=semResultado?"Other programme":"Jaguars"};
  return n;
}
int ajustes_guia_fonte(void) { return 0; }
int perfis_ativo(void) { return 1; }
char *dados_ler(const char *n) {(void)n;return NULL;}
int epg_fonte_estado(const char *n) {(void)n;return EPG_PRONTO;}
int epg_casar(const char *f,const char *id,const char *tv,const char *nm,int *met) {(void)f;(void)id;(void)tv;(void)nm;*met=0;return 0;}
int epg_achar(const char *f,const char *id) {(void)f;(void)id;return 0;}
static void semear(int n) {
  free(canais);ltv_arena_soltar(&arena);
  canais=calloc((size_t)n,sizeof *canais);assert(canais);nCanais=capCanais=n;
  if(!cats)cats=calloc(1,sizeof *cats);assert(cats);nCats=1;cats[0]=(GCat){.nome="Sports",.fonte=0};
  nLtvF=1;ltvF[0].tipo=LTV_ADDON;ltvF[0].base="";strcpy(ltvF[0].chave,"fixture");
  for(int i=0;i<n;i++) { char nome[80],id[80];snprintf(nome,sizeof nome,"Channel %d",i+1);snprintf(id,sizeof id,"fixture:%d",i);
    canais[i]=(GCanal){.id=ltv_dup(&arena,id),.nome=ltv_dup(&arena,nome),.epg=i,.cat=0,.fonte=0,.numero=i+1}; }
  canais[0].nome=ltv_dup(&arena,"Jaguars channel");canaisRevisao++;chamadas=0;tick=0;
}
static void concluir(void) { int passos=0;while(buscaPendente.ativa){assert(++passos<1000);buscaPasso();} }
int main(void) {
  // No scan in submit; yielding after one slow page keeps the UI available.
  semear(10);custo=5;buscaFazer(" Jaguars ");
  assert(buscaNC==1 && buscaNP==0 && chamadas==0 && buscaPendente.ativa);
  buscaPasso();assert(chamadas==1 && buscaNP==0 && buscaPendente.ativa);
  concluir();assert(buscaNP==10);
  for(int i=0;i<5;i++)assert(buscaProg[i].ini<=buscaPendente.de);
  for(int i=5;i<10;i++)assert(buscaProg[i].ini>buscaPendente.de);
  // Pagination yields within a channel, preserving the positional cursor.
  semear(1);paginasPorCanal=2;buscaFazer("Jaguars");buscaPasso();
  assert(buscaPendente.vistos==24 && buscaPendente.canal==0 && buscaNP==0);
  concluir();assert(buscaNP==25 && chamadas==2);paginasPorCanal=1;
  // A coarse/frozen clock still yields before scanning a large provider.
  semear(21202);custo=0;semResultado=1;buscaFazer("Jaguars");buscaPasso();
  assert(chamadas==256 && buscaPendente.ativa && buscaNP==0);
  buscaFechar();int antes=chamadas;buscaPasso();assert(chamadas==antes);
  // Starting a new query discards the old staged programmes.
  semResultado=0;buscaFazer("Jaguars");buscaPasso();assert(buscaNP==40);
  buscaFazer("missing");assert(buscaNC==0 && buscaNP==0 && buscaPendente.n==0);
  buscaFechar();
  // Channel-number lookup does not scan programme schedules.
  chamadas=0;buscaFazer("2");assert(buscaNC==G_BUSCA_CANAIS && buscaCanal[0]==1 && !buscaPendente.ativa && !chamadas);
  // Restarting the saved query must not overwrite its own input buffer.
  buscaFazer("Jaguars");buscaFazer(buscaTexto);assert(!strcmp(buscaTexto,"Jaguars") && buscaNC==1);
  free(canais);free(cats);ltv_arena_soltar(&arena);
  puts("PASS: bounded search, pagination, cancellation, replacement, ordering, result cap and channel numbers");
}
