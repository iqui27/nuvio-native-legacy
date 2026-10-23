// This is the scalar settings component, not a full installation backup.
// Separate settings stores and Drive transport are handled by their owners.
#include <limits.h>
#define AB_MAX_TEXTO 65536u
#define AB_N (AJ_N + 3)
static const char *const abExtras[] = {
  "idiomaAutoLocal", "idiomaFonteLocal", "texturasEscolhaLocal"
};
static const char *abNome(int op) { return op < AJ_N ? CHAVE[op] : abExtras[op-AJ_N]; }
static int abOp(const char *chave) {
  for(int i=0;i<AJ_N;i++)if(CHAVE[i] && !strcmp(CHAVE[i],chave))return i;
  for(int i=0;i<3;i++)if(!strcmp(abExtras[i],chave))return AJ_N+i;
  return -1;
}
static int abDesteDispositivo(int op) {
  if(op>=0 && op<AJ_N && CHAVE[op] && !strcmp(CHAVE[op],"epgMemoriaLocal"))return 1;
  switch(op) {
    // A phone backup must not force TV mode, memory/panel limits, input
    // routing, account-sharing policy or telemetry consent on another device.
    case AJ_INTERFACE_MODO:case AJ_RESOLUCAO:case AJ_TEX_MB:
    case AJ_AMPLIAR_UI:case AJ_AMPLIACAO_ALGO:case AJ_AMPLIACAO_NITIDEZ:
    case AJ_HDR_INTERFACE:case AJ_TUNNELING:case AJ_DV7:case AJ_AUDIO_OFFSET:
    case AJ_QUALIDADE_IMG:case AJ_ITENS_FILEIRA:case AJ_TAMANHO_UI:case AJ_TAMANHO_AJUSTES:
    case AJ_ESCALA_TEXTO:case AJ_DTS_AC3:case AJ_PROPORCAO_PADRAO:
    case AJ_GPU_EFEITOS:case AJ_VIDRO_QUALIDADE:case AJ_BUFFER:case AJ_CACHE_SEEK:
    case AJ_HDR_UI:case AJ_ESMAECER:case AJ_BRILHO_PLAYER:case AJ_DV_MKV:
    case AJ_ENVIO_AUTO:case AJ_TECLADO_ANDROID:case AJ_PROVEDORES_ESCOPO:
    case AJ_PLAYER_EXTERNO:case AJ_PLAYER_ANIME:
    case AJ_P2P_LIGADO:case AJ_P2P_LIMITE:case AJ_P2P_DESTINO:
      return 1;
    default:return op==AJ_N+2; // explicit image-memory budget marker
  }
}
static int abLocal(int op,int perfil,int v) {
  (void)v;
  if(op<0 || op>=AB_N || op==AJ_PROVEDORES_ESCOPO || op==AJ_ENVIO_AUTO)return 0;
  // Use the actual account-sync policy, not a second list of portable keys.
  // Language/row/source/subtitle choices now travel in c_settings_v1.
  if(op<AJ_N && (ajustePortavel(op) || !CHAVE[op] || CHAVE[op][0]=='-' ||
      OPCOES[op].tipo==OP_ACAO || OPCOES[op].tipo==OP_LEITURA))return 0;
  if(op>=AJ_N) {
    if(op==AJ_N+2)return perfil==AJ_BACKUP_HARDWARE;
    return perfil==AJ_BACKUP_APARELHO && !ajustePortavel(AJ_IDIOMA);
  }
  if(abDesteDispositivo(op))return perfil==AJ_BACKUP_HARDWARE;
  return perfil!=AJ_BACKUP_HARDWARE && !!dePerfil(op)==perfil;
}
static int abValido(int op,int v) {
  if(op==AJ_IDIOMA)return v>=0 && v<IDIOMA_N; // persisted language ID, not picker index
  if(op==AJ_N+1)return v>=IDA_TMDB && v<=IDA_PADRAO;
  if(op>=AJ_N)return v==0 || v==1;
  const Opcao *o=&OPCOES[op];
  return o->tipo==OP_NUMERO ? v>=o->min && v<=o->max : v>=0 && v<nValores(op);
}
// A bounded line view; keep original non-backup lines byte-for-byte on merge.
static const char *abLinha(const char *p,const char **fim,char chave[80],const char **valorIni) {
  *fim=strchr(p,'\n');if(!*fim)*fim=p+strlen(p);
  const char *q=p;while(q<*fim && (*q==' ' || *q=='\t' || *q=='\r'))q++;
  size_t n=0;
  while(q<*fim && (unsigned char)*q>32) {
    if(n<79)chave[n++]=*q;else { chave[0]=0;*valorIni=*fim;return **fim?*fim+1:*fim; }
    q++;
  }
  chave[n]=0;while(q<*fim && (*q==' ' || *q=='\t'))q++;
  *valorIni=q;return **fim?*fim+1:*fim;
}
static int abNumero(const char *p,const char *fim,int *out) {
  int negativo=0;unsigned long long n=0,limite;
  if(p<fim && (*p=='-' || *p=='+')) { negativo=*p=='-';p++; }
  if(p==fim || *p<'0' || *p>'9')return 0;
  limite=negativo?(unsigned long long)INT_MAX+1u:(unsigned long long)INT_MAX;
  while(p<fim && *p>='0' && *p<='9') {
    unsigned d=(unsigned)(*p++-'0');if(n>(limite-d)/10u)return 0;n=n*10u+d;
  }
  while(p<fim && (*p==' ' || *p=='\t' || *p=='\r'))p++;
  if(p!=fim)return 0;
  *out=negativo?(n==(unsigned long long)INT_MAX+1u?INT_MIN:-(int)n):(int)n;
  return 1;
}
static int abTexto(const char *s) { return s && strnlen(s,AB_MAX_TEXTO+1u)<=AB_MAX_TEXTO; }
static int abJuntar(char *out,size_t cap,size_t *n,const char *p,size_t len) {
  if(len>=cap-*n)return 0;
  memcpy(out+*n,p,len);*n+=len;out[*n]=0;return 1;
}
static int abEscrever(char *out,size_t cap,size_t *n,int op,int v) {
  char linha[128];int len=snprintf(linha,sizeof linha,"%s %d\n",abNome(op),v);
  return len>0 && (size_t)len<sizeof linha && abJuntar(out,cap,n,linha,(size_t)len);
}
static void abIgnorado(int perfil,int op,const char *motivo) {
  // Never log backup values or unrecognised text, which may contain secrets.
  printf("[backup] setting skipped: scope=%d key=%s reason=%s; keeping current value\n",
    perfil,op>=0?abNome(op):"unknown",motivo);
}
int ajustes_backup_exportar(int perfil,const char *origem,char **saida) {
  if(saida)*saida=NULL;
  if(!saida || (perfil<AJ_BACKUP_APARELHO || perfil>AJ_BACKUP_HARDWARE) || !abTexto(origem))return 0;
  unsigned char viu[AB_N]={0};int valores[AB_N]={0};
  for(const char *p=origem;*p;) {
    char chave[80];const char *fim,*v;const char *proximo=abLinha(p,&fim,chave,&v);
    int op=abOp(chave),valorLido;
    if(op>=0) {
      // Values belonging to Nuvio or the other scope need not be numeric
      // for this component to preserve/ignore them.
      if(abLocal(op,perfil,AJ_N_TEMAS)) {
        if(!abNumero(v,fim,&valorLido))return 0;
        if(abLocal(op,perfil,valorLido)) {
          if(!abValido(op,valorLido))return 0;
          viu[op]=1;valores[op]=valorLido;
        }
      }
    }
    p=proximo;
  }
  char *out=calloc(AB_MAX_TEXTO+1u,1);if(!out)return 0;
  size_t n=(size_t)snprintf(out,AB_MAX_TEXTO+1u,"NUVIO-LOCAL-SETTINGS 1\nscope %s\n",perfil==AJ_BACKUP_HARDWARE?"hardware":perfil?"profile":"device");
  for(int i=0;i<AB_N;i++)if(viu[i] && !abEscrever(out,AB_MAX_TEXTO+1u,&n,i,valores[i])) { free(out);return 0; }
  *saida=out;return 1;
}
static int abMesclar(int perfil,const char *atual,const char *backup,char **saida,int hardwarePerfil) {
  if(saida)*saida=NULL;
  if(!saida || (perfil<AJ_BACKUP_APARELHO || perfil>AJ_BACKUP_HARDWARE) || !abTexto(atual) || !abTexto(backup))return 0;
  char cab[80];int k=snprintf(cab,sizeof cab,"NUVIO-LOCAL-SETTINGS 1\nscope %s\n",perfil==AJ_BACKUP_HARDWARE?"hardware":perfil?"profile":"device");
  if(strncmp(backup,cab,(size_t)k))return 0;
  unsigned char viu[AB_N]={0},escreveu[AB_N]={0};int valores[AB_N]={0};
  for(const char *p=backup+k;*p;) {
    char chave[80];const char *fim,*v;const char *proximo=abLinha(p,&fim,chave,&v);
    int op=abOp(chave),valorLido;
    p=proximo;
    if(op<0){abIgnorado(perfil,op,"unknown setting");continue;}
    if(viu[op])return 0;
    viu[op]=2;
    // Old encrypted previews contained native choices which Nuvio now syncs.
    // Leave current account values alone, even if their representation changed.
    if((op<AJ_N && ajustePortavel(op)) || ((op==AJ_N || op==AJ_N+1) && ajustePortavel(AJ_IDIOMA)))continue;
    if(!abNumero(v,fim,&valorLido)){abIgnorado(perfil,op,"unsupported value format");continue;}
    if(!abLocal(op,perfil,valorLido)){abIgnorado(perfil,op,"changed scope or excluded setting");continue;}
    if(hardwarePerfil && !(op<AJ_N && dePerfil(op)))continue;
    if(!abValido(op,valorLido)){abIgnorado(perfil,op,"unsupported value");continue;}
    viu[op]=1;valores[op]=valorLido;
  }
  // Complete validation precedes output construction. The caller later owns
  // atomic persistence, identity checks and module reloads.
  size_t cap=AB_MAX_TEXTO+1u,n=0;char *out=calloc(cap,1);if(!out)return 0;
  for(const char *p=atual;*p;) {
    char chave[80];const char *fim,*v;const char *proximo=abLinha(p,&fim,chave,&v);
    int op=abOp(chave),ok=1;
    if(op>=0 && viu[op]==1) {
      if(!escreveu[op])ok=abEscrever(out,cap,&n,op,valores[op]);
      escreveu[op]=1;
    } else ok=abJuntar(out,cap,&n,p,(size_t)(proximo-p));
    if(!ok) { free(out);return 0; }p=proximo;
  }
  for(int i=0;i<AB_N;i++)if(viu[i]==1 && !escreveu[i]) {
    if(n && out[n-1]!='\n' && !abJuntar(out,cap,&n,"\n",1)) { free(out);return 0; }
    if(!abEscrever(out,cap,&n,i,valores[i])) { free(out);return 0; }
  }
  *saida=out;return 1;
}
int ajustes_backup_mesclar(int perfil,const char *atual,const char *backup,char **saida) {
  return abMesclar(perfil,atual,backup,saida,0);
}
int ajustes_backup_hardware_perfil(const char *atual,const char *backup,char **saida) {
  return abMesclar(AJ_BACKUP_HARDWARE,atual,backup,saida,1);
}
// Migrate known old scopes, and omit individual unsupported settings before
// validation. Do not guess meanings for renamed keys or changed values.
// Account-owned values never depend on the destination's dynamic choices.
int ajustes_backup_selecionar(const char *const entrada[3],int hardware,char *saida[3]) {
  if(!entrada || !saida)return 0;
  for(int i=0;i<3;i++)saida[i]=NULL;
  const char *escopos[]={"device","profile","hardware"};
  unsigned char vistos[AB_N]={0};size_t usado[3]={0};
  for(int i=0;i<3;i++) {
    if(i==AJ_BACKUP_HARDWARE && !hardware)continue;
    saida[i]=calloc(AB_MAX_TEXTO+1u,1);if(!saida[i])goto falha;
    usado[i]=(size_t)snprintf(saida[i],AB_MAX_TEXTO+1u,"NUVIO-LOCAL-SETTINGS 1\nscope %s\n",escopos[i]);
  }
  for(int origem=0;origem<3;origem++) {
    if(!entrada[origem])continue;
    char cab[80];int k=snprintf(cab,sizeof cab,"NUVIO-LOCAL-SETTINGS 1\nscope %s\n",escopos[origem]);
    if(!abTexto(entrada[origem]) || strncmp(entrada[origem],cab,(size_t)k))goto falha;
    for(const char *p=entrada[origem]+k;*p;) {
      char chave[80];const char *fim,*v;const char *proximo=abLinha(p,&fim,chave,&v);
      int op=abOp(chave),valorLido;
      p=proximo;
      if(op<0){abIgnorado(origem,op,"unknown setting");continue;}
      if(vistos[op]) {
        printf("[backup] duplicate scalar rejected: scope=%d key=%s\n",origem,abNome(op));goto falha;
      }
      vistos[op]=1;
      if((op<AJ_N && ajustePortavel(op)) || ((op==AJ_N || op==AJ_N+1) && ajustePortavel(AJ_IDIOMA)))continue;
      if(!abNumero(v,fim,&valorLido)){abIgnorado(origem,op,"unsupported value format");continue;}
      int destino=abDesteDispositivo(op)?AJ_BACKUP_HARDWARE:(op<AJ_N && dePerfil(op));
      int legado=origem==AJ_BACKUP_APARELHO && destino==AJ_BACKUP_HARDWARE &&
        (op==AJ_QUALIDADE_IMG || op==AJ_ITENS_FILEIRA || op==AJ_TAMANHO_UI || op==AJ_TAMANHO_AJUSTES);
      if((origem!=destino && !legado) || !abLocal(op,destino,valorLido)) {
        abIgnorado(origem,op,"changed scope or excluded setting");continue;
      }
      if(destino==AJ_BACKUP_HARDWARE && !hardware)continue;
      if(!abValido(op,valorLido)){abIgnorado(origem,op,"unsupported value");continue;}
      if(!abEscrever(saida[destino],AB_MAX_TEXTO+1u,&usado[destino],op,valorLido))goto falha;
    }
  }
  return 1;
falha:
  for(int i=0;i<3;i++){free(saida[i]);saida[i]=NULL;}
  return 0;
}

int ajustes_backup_capturar(int perfil,char **saida) {
  if(saida)*saida=NULL;
  if(!saida || (perfil<AJ_BACKUP_APARELHO || perfil>AJ_BACKUP_HARDWARE))return 0;
  char *origem=calloc(AB_MAX_TEXTO+1u,1);if(!origem)return 0;
  size_t n=0;int ok=1;
  for(int i=0;i<AJ_N && ok;i++)if(abLocal(i,perfil,valor[i])) {
    int v=i==AJ_IDIOMA?ajustes_idioma():valor[i];
    ok=abValido(i,v) && abEscrever(origem,AB_MAX_TEXTO+1u,&n,i,v);
  }
  if(ok && perfil==AJ_BACKUP_APARELHO && !ajustePortavel(AJ_IDIOMA))
    ok=abEscrever(origem,AB_MAX_TEXTO+1u,&n,AJ_N,valor[AJ_IDIOMA]==0) &&
       abEscrever(origem,AB_MAX_TEXTO+1u,&n,AJ_N+1,idiomaFonteGravada);
  if(ok && perfil==AJ_BACKUP_HARDWARE)ok=abEscrever(origem,AB_MAX_TEXTO+1u,&n,AJ_N+2,memoriaEscolhida);
  if(ok)ok=ajustes_backup_exportar(perfil,origem,saida);
  free(origem);return ok;
}
