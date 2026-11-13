// Compile the actual Settings tables, getters, scope and profile disk format.
// Adapted from Matis94 Nuvio_webOS_2.0.8.6 (tests/skip_button_settings.c);
// enum-adjacency assertion updated to this tree's append-only tail.
#include "../src/ajustes.c"
#include <assert.h>
#include <unistd.h>
static const char *fixtureDirectory;
// Unrelated Settings catalog boundaries; no accounts, external fonts or IPTV.
int iptv_contas(IptvConta *out,int cap){(void)out;(void)cap;return 0;}
int legfontes_n(void){return 0;}
int legfontes_selecionada(int slot){(void)slot;return -1;}
int selospacote_n(void){return 0;}
int fontefalha_provedores(void){return 0;}
int fontefalha_addons(void){return 0;}
void fontefalha_filtro(FonteFiltroConfig *out){memset(out,0,sizeof *out);}
int fontefalha_provedor_limite(int service){(void)service;return 1;}
int fontefalha_addon_limite(const char *key){(void)key;return 1;}
int perfis_ativo(void){return 1;}
char *dados_ler(const char *name){
  char path[1024];snprintf(path,sizeof path,"%s/%s",fixtureDirectory,name);
  FILE *f=fopen(path,"rb");if(!f)return NULL;
  assert(!fseek(f,0,SEEK_END));long n=ftell(f);assert(n>=0);rewind(f);
  char *s=malloc((size_t)n+1);assert(s);assert(fread(s,1,(size_t)n,f)==(size_t)n);
  s[n]=0;fclose(f);return s;
}
int dados_gravar(const char *name,const char *text){
  char path[1024];snprintf(path,sizeof path,"%s/%s",fixtureDirectory,name);
  FILE *f=fopen(path,"wb");if(!f)return 0;
  size_t n=strlen(text);int ok=fwrite(text,1,n,f)==n;return fclose(f)==0&&ok;
}
int main(int argc,char **argv){
  assert(argc==4);fixtureDirectory=argv[2];int mask=atoi(argv[3]);
  const int options[]={AJ_SHOW_SKIP_INTRO,AJ_SHOW_SKIP_RECAP,AJ_SHOW_SKIP_CREDITS};
  const char *keys[]={"showSkipIntroButtonLocal","showSkipRecapButtonLocal","showSkipCreditsButtonLocal"};
  assert(AJ_SHOW_SKIP_INTRO==AJ_REMEMBER_PROFILE+1&&AJ_SHOW_SKIP_CREDITS==AJ_SHOW_SKIP_INTRO+2);
  for(int k=0;k<3;k++){
    int op=options[k],seen=0;
    assert(valorPadrao[op]==0&&valor[op]==0&&!strcmp(CHAVE[op],keys[k]));
    assert(OPCOES[op].tipo==OP_ESCOLHA&&OPCOES[op].n==2);
    assert(ajustes_escopo(op)==ESC_PERFIL_LOCAL&&!ajustePortavel(op)&&!ajusteAparelho(op));
    for(size_t i=0;i<sizeof TELA/sizeof *TELA;i++)if(TELA[i].tipo==IT_OPC&&TELA[i].op==op)seen++;
    assert(seen==1);
    assert(ajCenaSecao(op)==AJC_REP_DURANTE);
  }
  assert(ajustes_show_skip_intro()&&ajustes_show_skip_recap()&&ajustes_show_skip_credits());
  int originalAuto[]={valor[AJ_PULAR_ABERTURA],valor[AJ_PULAR_RESUMO],valor[AJ_PULAR_CREDITOS],valor[AJ_PROXIMO_AUTO]};
  if(!strcmp(argv[1],"write")){
    for(int k=0;k<3;k++)valor[options[k]]=!(mask&(1<<k));
    assert(perfilGravarVetor(1,valor));
    for(int k=0;k<3;k++)valor[options[k]]=k%2;
    assert(perfilGravarVetor(2,valor));
    char legacy[256];snprintf(legacy,sizeof legacy,"%s 1\n%s 1\n","pularCreditosLocal","pularAberturaLocal");
    assert(dados_gravar("ajustes-p3.txt",legacy));
  }else{
    int saved[AJ_N];char present[AJ_N];memcpy(saved,valorPadrao,sizeof saved);
    assert(perfilLerVetor(1,saved,present));
    for(int k=0;k<3;k++){assert(present[options[k]]&&saved[options[k]]==!(mask&(1<<k)));valor[options[k]]=saved[options[k]];}
    assert(ajustes_show_skip_intro()==!!(mask&1));
    assert(ajustes_show_skip_recap()==!!(mask&2));
    assert(ajustes_show_skip_credits()==!!(mask&4));
    assert(perfilLerVetor(2,saved,present));
    for(int k=0;k<3;k++)assert(saved[options[k]]==k%2);
    memcpy(saved,valorPadrao,sizeof saved);
    assert(perfilLerVetor(3,saved,present));
    for(int k=0;k<3;k++)assert(!present[options[k]]&&saved[options[k]]==0); // existing profile defaults ON
    assert(saved[AJ_PULAR_ABERTURA]==1&&saved[AJ_PULAR_CREDITOS]==1);
  }
  assert(valor[AJ_PULAR_ABERTURA]==originalAuto[0]&&valor[AJ_PULAR_RESUMO]==originalAuto[1]);
  assert(valor[AJ_PULAR_CREDITOS]==originalAuto[2]&&valor[AJ_PROXIMO_AUTO]==originalAuto[3]);
  puts("PASS: default-ON independent skip buttons, unchanged autoplay options, profile-local scope and cross-process disk round trip");
}
