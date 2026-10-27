/* Per-profile custom presets use stable setting keys, excluding credentials and actions. */
#define AJ_PEN_SAVED 8
static int ajPenSaveKeyboard,ajPenSaveProfile;
static char ajPenSavedNames[AJ_PEN_SAVED][64];
static int ajPenSavedValues[AJ_PEN_SAVED][AJ_N],ajPenSavedCount,ajPenSavedProfile;
static void ajPenSavedForget(void) {
  ajPenSavedProfile=ajPenSavedCount=ajPenSaveKeyboard=0;
  memset(ajPenSavedNames,0,sizeof ajPenSavedNames);memset(ajPenSavedValues,-1,sizeof ajPenSavedValues);
}
static void ajPenSavedLoad(void) {
  int profile=edPerfil?edPerfil:perfis_ativo();
  if(ajPenSavedProfile==profile)return;
  ajPenSavedProfile=profile;ajPenSavedCount=0;
  memset(ajPenSavedNames,0,sizeof ajPenSavedNames);
  memset(ajPenSavedValues,-1,sizeof ajPenSavedValues);
  char file[80];snprintf(file,sizeof file,"settings-presets-%d.txt",profile);
  char *data=dados_ler(file);if(!data)return;
  int slot=-1;char *save=NULL;
  for(char *line=strtok_r(data,"\n",&save);line;line=strtok_r(NULL,"\n",&save)) {
    if(!strncmp(line,"name=",5)) {
      if(ajPenSavedCount>=AJ_PEN_SAVED)break;
      slot=ajPenSavedCount++;snprintf(ajPenSavedNames[slot],64,"%s",line+5);
    } else if(slot>=0) {
      char key[96];int value;
      if(sscanf(line,"%95[^=]=%d",key,&value)==2)
        for(int op=0;op<AJ_N;op++)if(aj4Mutavel(op) && !strcmp(key,CHAVE[op]) && (OPCOES[op].tipo==OP_NUMERO?limita(op,value)==value:value>=0 && value<nValores(op))) { ajPenSavedValues[slot][op]=value;break; }
    }
  }
  free(data);
}
static int ajPenSavedWrite(void) {
  char file[80],*data=malloc(AJ_PEN_SAVED*AJ_N*128);if(!data)return 0;
  size_t pos=0,cap=AJ_PEN_SAVED*AJ_N*128;
  for(int slot=0;slot<ajPenSavedCount;slot++) {
    pos+=snprintf(data+pos,cap-pos,"name=%s\n",ajPenSavedNames[slot]);
    for(int op=0;op<AJ_N;op++)if(ajPenSavedValues[slot][op]>=0)pos+=snprintf(data+pos,cap-pos,"%s=%d\n",CHAVE[op],ajPenSavedValues[slot][op]);
  }
  snprintf(file,sizeof file,"settings-presets-%d.txt",ajPenSavedProfile);
  int ok=dados_gravar(file,data);free(data);return ok;
}
static int ajPenSavedSave(const char *name) {
  ajPenSavedLoad();
  if(pfTravado() || ajPenSavedCount>=AJ_PEN_SAVED || !name || !*name || strchr(name,'\n') || strchr(name,'\r'))return 0;
  int slot=ajPenSavedCount;
  snprintf(ajPenSavedNames[slot],64,"%s",name);
  for(int op=0;op<AJ_N;op++)ajPenSavedValues[slot][op]=aj4Mutavel(op)?valor[op]:-1;
  ajPenSavedCount++;
  if(ajPenSavedWrite())return 1;
  ajPenSavedCount--;return 0;
}
static void ajPenSavedAsk(void) {
  ajPenSavedLoad();
  if(pfTravado() || ajPenSavedCount>=AJ_PEN_SAVED)return;
  ajPenSaveProfile=perfis_ativo();ajPenSaveKeyboard=1;
  teclado_abrir_com("Salvar preset","Nome do preset",63,CUR_ALFA_URL,NULL);
  teclado_aceitar_utf8(1);
}
static int ajPenSavedPrepare(int slot) {
  ajPenSavedLoad();aj4N=0;
  if(slot<0 || slot>=ajPenSavedCount)return 0;
  for(int op=0;op<AJ_N;op++) {
    int value=ajPenSavedValues[slot][op];
    if(value>=0 && aj4Mutavel(op) && ajOpcaoDisponivel(op) && valor[op]!=value) {
      aj4Ops[aj4N]=op;aj4Antes[aj4N]=valor[op];aj4Depois[aj4N++]=value;
    }
  }
  aj4Review=2;aj4Linha=aj4Acao=0;return aj4N;
}
static const int AJ_PEN_PRESET_OPS[4][8]={
  {AJ_QUALIDADE,AJ_DTS_AC3,AJ_ESCALA_TEXTO,-1},
  {AJ_LEG_SYNC_AUDIO,AJ_LEG_SYNC_AUTO,AJ_AUDIO_OFFSET,-1},
  {AJ_DET_BLUR_NAO_VISTOS,AJ_PAUSA_ELENCO,AJ_HERO_TRAILER,-1},
  {AJ_HERO,AJ_HERO_TRAILER,AJ_DET_TRAILER_AUTO,AJ_FOCO_TRAILER,-1}
};
static const int AJ_PEN_PRESET_VALS[4][8]={{0,0,4},{0,0,0},{0,0,0},{1,1,1,0}};
static const char *const AJ_PEN_PRESET_NAMES[]={"Sala de TV","Voz clara","Filme em casa","Só o áudio"};
static int ajPenPresetPrepare(int preset) {
  aj4N=0;if(preset<0 || preset>=4)return 0;
  for(int i=0;i<8 && AJ_PEN_PRESET_OPS[preset][i]>=0;i++) {
    int op=AJ_PEN_PRESET_OPS[preset][i],value=AJ_PEN_PRESET_VALS[preset][i];
    if(ajOpcaoDisponivel(op) && valor[op]!=value) {
      aj4Ops[aj4N]=op;aj4Antes[aj4N]=valor[op];aj4Depois[aj4N++]=value;
    }
  }
  aj4Review=2;aj4Linha=aj4Acao=0;return aj4N;
}
