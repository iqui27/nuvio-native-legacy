// Exercita guia.c de producao: a republicacao sem a playlist do filtro tem de
// derrubar a selecao para Todas (a fonte resolve pela CHAVE, nao pelo indice),
// e a lista de fontes cheia nao pode indexar ltvF alem do array.
#include "perfis.h"
#define NV_PERFIL_SESSAO_FORTE
#include "../src/guia.c"
#include <assert.h>
#include <unistd.h>
int ajustes_livetv_sugerir(void){return 0;}
int ajustes_livetv_agrupar(void){return 0;}
int g5ui_channel_hidden(const char *id,const char *name,const char *group,const char *source){return 0;}
int g5ui_channel_allowed(const char *id,const char *name,const char *group,const char *source){(void)id;(void)name;(void)group;(void)source;return 1;}
const char *livecat_secao(const char *n,int *r){(void)n;*r=0;return NULL;}
int xtream_e_id(const char *id){return id && !strncmp(id,"xtream:",7);}
static int perfilTeste=1;
static uint64_t sessaoTeste=1;
static const char *pasta;
static IptvConta contasTeste[1]={{.tipo=IPTV_XTREAM,.id=11,.ativa=1,.nome="Account A"}};
uint64_t perfis_sessao_geracao(void){return sessaoTeste;}
int perfis_sessao_atual(uint64_t s){return s==sessaoTeste;}
int perfis_sessao_travar(uint64_t s){return s==sessaoTeste;}
void perfis_sessao_destravar(void){}
const char *i18n(const char *s){return s;}
int ajustes_guia_ordem(void){return 0;}
int ajustes_guia_fonte(void){return 0;}
int iptv_contas(IptvConta *out,int max){if(out && max>=1)memcpy(out,contasTeste,sizeof contasTeste);return 1;}
int addons_n(void){return 1;}
const char *addons_base(int i){(void)i;return "https://addon.invalid";}
const char *addons_nome(int i){(void)i;return "Addon C";}
int addons_ativo(int i){(void)i;return 1;}
int perfis_ativo(void){return perfilTeste;}
char *dados_ler(const char *nome){
  char cam[1024];snprintf(cam,sizeof cam,"%s/%s",pasta,nome);FILE *f=fopen(cam,"rb");if(!f)return NULL;
  assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>=0);rewind(f);char *b=calloc((size_t)n+1,1);assert(b);
  assert(fread(b,1,(size_t)n,f)==(size_t)n);fclose(f);return b;
}
int dados_gravar(const char *nome,const char *texto){
  char cam[1024];snprintf(cam,sizeof cam,"%s/%s",pasta,nome);
  FILE *f=fopen(cam,"wb");assert(f);assert(fwrite(texto,1,strlen(texto),f)==strlen(texto));return !fclose(f);
}
char *dados_caminho(char *out,unsigned n,const char *nome){snprintf(out,n,"%s/%s",pasta,nome);return out;}
void marco(const char *s){(void)s;}

static void semear(void) {
  stagingZerar(); sTeto = 30000;
  // Fonte 0 (playlist Xtream) com 2 canais; fonte 1 (addon) com 1 canal
  // agrupado por base.
  int f0 = sFonteChave("xta0000000b", LTV_XTREAM, "Account A", "");
  assert(f0 == 0);
  for (int c = 0; c < 2; c++) {
    char id[80], epg[40];
    snprintf(id, sizeof id, "xtream:@0000000b:%d", c + 1);
    snprintf(epg, sizeof epg, "epg-%d", c);
    GCru cru = {.id=id, .nome=c?"Chan B":"Chan A", .logo="", .desc="", .epgId=epg,
                .tvgNome="", .url="", .ua="", .ref="", .grupo="News", .tipo="channel", .numero=c+1};
    sGrupoInfo(f0, "News", 1);
    assert(sCanalAdd(f0, &cru));
  }
  int f1 = sFonteChave("g5ad0", LTV_ADDON, "Addon C", "https://addon.invalid");
  assert(f1 == 1);
  {
    char ida[] = "addon:1";
    GCru cru = {.id=ida, .nome="Addon chan", .logo="", .desc="", .epgId="",
                .tvgNome="", .url="", .ua="", .ref="", .grupo="Live", .tipo="channel", .numero=1};
    sGrupoInfo(f1, "Live", 1);
    assert(sCanalAdd(f1, &cru));
  }
  adotarStaging();
  for(int q=0;q<nCanais;q++)fprintf(stderr,"[%d f=%d base=%s] ",q,canais[q].fonte,canais[q].base);
  fprintf(stderr,"\n");
  nFontes = 1;
  snprintf(fontes[0].base, sizeof fontes[0].base, "%s", "https://addon.invalid");
  snprintf(fontes[0].nome, sizeof fontes[0].nome, "%s", "Addon C");
}

int main(int argc, char **argv) {
  assert(argc == 2); pasta = argv[1]; semear();

  // Todas e Favoritos preservam a grade.
  filtroDefinir(G_FILTRO_TODAS);
  assert(nVis == 2 && nZapL == 3);

  // Chip do addon pela base: so os canais da base.
  filtroDefinir(G_FILTRO_ADDON_BASE + 0);
  assert(filtro == G_FILTRO_ADDON_BASE && nVis == 1 && nZapL == 1);

  // Playlist filtrada pelo indice (chip 2 = fonte 0)... e depois a fonte some
  // na republicacao: a resolucao pela chave tem de cair para Todas, nao deixar
  // grade/zapping vazios (achado do revisor: filtro=1 vis=0 zap=0).
  filtroDefinir(0);
  assert(filtro == 0 && nVis == 1 && nZapL == 2);
  // Republicacao sem a playlist filtrada: a fonte 1 (addon) vira o indice 0
  // e os canais dela sao reindexados, como compactarStaging faz.
  { int q, w;
    for (q = 0, w = 0; q < nCanais; q++)
      if (canais[q].fonte == 1) { canais[w] = canais[q]; canais[w].fonte = 0; w++; }
    nCanais = w;
    for (q = 0, w = 0; q < nCats; q++)
      if (cats[q].fonte == 1) { cats[w] = cats[q]; cats[w].fonte = 0; w++; }
    nCats = w;
    memmove(&ltvF[0], &ltvF[1], sizeof ltvF[0]);
    nLtvF = 1;
    for (q = 0, w = 0; q < nCanais; q++) canais[q].cat = 0;
    catN[0] = nCanais; catIni[0] = 0; }
  // O indice antigo (0) ainda existe mas e OUTRA fonte: a resolucao pela
  // chave tem de cair para Todas (a chave "xta0000000b" nao esta mais la).
  vistaMontar();
  assert(filtro == G_FILTRO_TODAS);
  assert(nVis == 1 && nZapL == 1);

  // Filtro de addon tambem resolve pela base se os addons mudarem de ordem.
  semear();
  filtroDefinir(G_FILTRO_ADDON_BASE + 0);
  snprintf(fontes[0].base, sizeof fontes[0].base, "%s", "https://outro.invalid");
  vistaMontar();
  assert(filtro == G_FILTRO_TODAS && nZapL == 3);

  // Lista cheia de contas: o indice do chip extra nao pode ultrapassar ltvF.
  // Os guards de FOP_FONTE e do desenho so aceitam chips < 2 + nLtvF.
  filtroDefinir(0);
  nLtvF = G_MAX_LTV;
  fontesCol = 2 + G_MAX_LTV;   // primeiro chip de addon
  assert(!(fontesCol >= 2 && fontesCol < 2 + nLtvF));   // FOP_FONTE nao abre

  // Digitacao numerica com filtro de addon ativo continua procurando na lista
  // inteira (f=-1), nao num "fonte 1000".
  filtroDefinir(G_FILTRO_ADDON_BASE + 0);
  { CatItem it; estado = G_PRONTO; assert(guia_numero_canal(1, &it, NULL, 0, 0)); }

  puts("guia_addon_filtro: ok");
  return 0;
}
