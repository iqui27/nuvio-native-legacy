/* Presentation-only groups retain every provider ID and playback record. */
typedef struct { int root, tail, fonte; char nome[160], epg[160]; } GVarKey;
static LtvArena varianteArena;
static int variantesAtivas;
static int *canalIndice;
static unsigned canalIndiceCap;
static int varianteAlturaScore(int h) { return h==1080?4:h==720?3:h==2160?2:h>0?1:0; }
static int varianteScore(const GCanal *c) { return varianteAlturaScore(nv_res_do_texto(c->nome)); }
static void variantesMontar(void) {
  ltv_arena_soltar(&varianteArena);variantesAtivas=0;
  for(int i=0;i<nCanais;i++){
    canais[i].varianteGrupo=canais[i].varianteProxima=0;canais[i].nomeGrupo=NULL;
  }
  /* Rebuild once per publication/repack; snapshots only read these caches. */
  free(canalIndice);canalIndice=NULL;canalIndiceCap=0;
  unsigned cap=1;while(cap<(unsigned)nCanais*2+1)cap*=2;
  canalIndice=calloc(cap,sizeof *canalIndice);
  if(canalIndice){
    canalIndiceCap=cap;
    for(int i=0;i<nCanais;i++){
      unsigned slot=ltv_hash(canais[i].id)&(cap-1);
      while(canalIndice[slot])slot=(slot+1)&(cap-1);
      canalIndice[slot]=i+1;
    }
  }
  if(!ajustes_livetv_agrupar())return;
  GVarKey *keys=calloc(cap,sizeof *keys);if(!keys)return;
  for(int i=0;i<nCanais;i++){
    char display[160],nome[160],epg[160];
    nv_nome_limpo(canais[i].nome,display,sizeof display);
    canais[i].nomeGrupo=ltv_dup(&varianteArena,display[0]?display:canais[i].nome);
    nv_nome_base(canais[i].nome,nome,sizeof nome);
    /* Distinct explicit EPG identities never collapse on a generic name. */
    nv_dobrar(canais[i].epgId,epg,sizeof epg);
    if(!nome[0])continue;
    unsigned slot=(ltv_hash(nome)*31UL+ltv_hash(epg)*7UL+(unsigned)canais[i].fonte)&(cap-1);
    while(keys[slot].root&&(keys[slot].fonte!=canais[i].fonte||
          strcmp(keys[slot].nome,nome)||strcmp(keys[slot].epg,epg)))slot=(slot+1)&(cap-1);
    GVarKey *key=&keys[slot];
    if(!key->root){
      key->root=key->tail=i+1;key->fonte=canais[i].fonte;
      snprintf(key->nome,sizeof key->nome,"%s",nome);snprintf(key->epg,sizeof key->epg,"%s",epg);
    }else{
      int root=key->root-1;
      if(!canais[root].varianteGrupo)variantesAtivas++;
      canais[root].varianteGrupo=canais[i].varianteGrupo=root+1;
      canais[key->tail-1].varianteProxima=i+1;key->tail=i+1;
    }
  }
  free(keys);
}
static GCanal *varianteMelhor(const GCanal *c) {
  int root=varianteRaiz((int)(c-canais));
  if(!canais[root].varianteGrupo)return (GCanal *)c;
  GCanal *best=NULL;
  char file[48];arquivoPerfil("variantes",file,sizeof file);char *saved=dados_ler(file);
  for(int i=root;i>=0;i=canais[i].varianteProxima-1){
    GCanal *v=&canais[i];if(!gLiberado(v)||!fonteAtiva(v->fonte))continue;
    if(!best||varianteScore(v)>varianteScore(best))best=v;
    if(saved){
      size_t len=strlen(v->id);const char *p=saved;
      while(*p){size_t n=strcspn(p,"\n");if(n==len&&!memcmp(p,v->id,n)){best=v;goto done;}p+=n;if(*p)p++;}
    }
  }
done:
  free(saved);return best?best:(GCanal *)c;
}
int guia_lembrete_achar(const char *id,time_t ini,const char *titulo) {
  const char *group=guia_canal_grupo_id(id);
  for(int i=0;i<lembrete_n();i++){
    const Lembrete *r=lembrete_item(i);
    if(!strcmp(group,guia_canal_grupo_id(r->canal))){
      int found=lembrete_achar(r->canal,ini,titulo);if(found>=0)return found;
    }
  }
  return -1;
}
int guia_lembrete_alternar(const char *id,const char *nome,const char *titulo,const char *base,time_t ini,time_t fim) {
  int i=guia_lembrete_achar(id,ini,titulo);
  return lembrete_alternar(i>=0?lembrete_item(i)->canal:guia_canal_grupo_id(id),nome,titulo,base,ini,fim);
}
/* Called only after a source produces video; failed opens never become preferred. */
void guia_variante_tocou(const char *id) {
  GCanal *c=NULL;
  for(int i=0;id&&i<nCanais;i++)if(!strcmp(canais[i].id,id)){c=&canais[i];break;}
  if(!c)return;
  char file[48];arquivoPerfil("variantes",file,sizeof file);
  char *old=dados_ler(file),buf[400*81];size_t used=(size_t)snprintf(buf,sizeof buf,"%s\n",id);
  const char *p=old;int root=varianteRaiz((int)(c-canais));
  while(p&&*p&&used+81<sizeof buf){
    size_t n=strcspn(p,"\n");int same=0;
    for(int i=root;i>=0;i=canais[i].varianteProxima-1)
      if(strlen(canais[i].id)==n&&!memcmp(canais[i].id,p,n)){same=1;break;}
    if(!same&&n<80){memcpy(buf+used,p,n);used+=n;buf[used++]='\n';}
    p+=n;if(*p)p++;
  }
  buf[used]=0;if(!old||strcmp(old,buf))dados_gravar(file,buf);free(old);
}
int guia_tem_variantes(const char *id) {
  for(int i=0;id&&i<nCanais;i++)if(!strcmp(canais[i].id,id))return canais[i].varianteGrupo!=0;
  return 0;
}
const char *guia_canal_grupo_id(const char *id) {
  for(int i=0;id&&i<nCanais;i++)if(!strcmp(canais[i].id,id))return canais[varianteRaiz(i)].id;
  return id?id:"";
}
