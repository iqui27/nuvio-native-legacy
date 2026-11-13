// PRESETS: os quatro prontos (tabela abaixo) e ate 8 salvos por perfil
// (settings-presets-<perfil>.txt, chaves estaveis; credenciais e acoes ficam fora).
// Tudo passa pela revisao em lote (rvN/rvOps): nada muda sem o OK em "Aplicar".
#define AJ_PEN_SAVED 8
static int ajPenSaveKeyboard,ajPenSaveProfile;
static char ajPenSavedNames[AJ_PEN_SAVED][64];
static int ajPenSavedValues[AJ_PEN_SAVED][AJ_N],ajPenSavedCount,ajPenSavedProfile;
static void ajPenSavedForget(void) {
  ajPenSavedProfile=ajPenSavedCount=ajPenSaveKeyboard=0;
  memset(ajPenSavedNames,0,sizeof ajPenSavedNames);memset(ajPenSavedValues,-1,sizeof ajPenSavedValues);
}
static int ajPenPresetStored(int op);
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
      if(sscanf(line,"%95[^=]=%d",key,&value)==2) {
        // Keep the primary opacity's persisted IDs stable, just like player.txt.
        if(!strcmp(key,CHAVE[AJ_LEG1_OPACIDADE]) && value>=0 && value<LEG_OPAC_N)
          value=leg_opacidade_indice(value);
        if(!strcmp(key,"legenda2OpacidadeLocal") && value>=0 && value<=4) {
          if(ajPenSavedValues[slot][AJ_LEG2_OPACIDADE]<0)
            ajPenSavedValues[slot][AJ_LEG2_OPACIDADE]=leg_opacidade_secundaria_migrar(value);
          continue;
        }
        for(int op=0;op<AJ_N;op++)if(ajPenPresetStored(op) && !strcmp(key,CHAVE[op]) && (OPCOES[op].tipo==OP_NUMERO?limita(op,value)==value:value>=0 && value<nValores(op))) { ajPenSavedValues[slot][op]=value;break; }
      }
    }
  }
  free(data);
}
static int ajPenSavedWrite(void) {
  char file[80],*data=malloc(AJ_PEN_SAVED*AJ_N*128);if(!data)return 0;
  size_t pos=0,cap=AJ_PEN_SAVED*AJ_N*128;data[0]=0;
  for(int slot=0;slot<ajPenSavedCount;slot++) {
    pos+=snprintf(data+pos,cap-pos,"name=%s\n",ajPenSavedNames[slot]);
    for(int op=0;op<AJ_N;op++)if(ajPenSavedValues[slot][op]>=0) {
      int value=ajPenSavedValues[slot][op];
      if(op==AJ_LEG1_OPACIDADE)value=leg_opacidade_do_indice(value);
      pos+=snprintf(data+pos,cap-pos,"%s=%d\n",CHAVE[op],value);
    }
  }
  snprintf(file,sizeof file,"settings-presets-%d.txt",ajPenSavedProfile);
  int ok=dados_gravar(file,data);free(data);return ok;
}
static int ajPenPresetStored(int op) { return triMutavel(op) || ajLegendaPrincipal(op); }
static int ajPenSavedSave(const char *name) {
  ajPenSavedLoad();
  if(pfTravado() || ajPenSavedCount>=AJ_PEN_SAVED || !name || !*name || strchr(name,'\n') || strchr(name,'\r'))return 0;
  int slot=ajPenSavedCount;
  snprintf(ajPenSavedNames[slot],64,"%s",name);
  for(int op=0;op<AJ_N;op++)ajPenSavedValues[slot][op]=ajPenPresetStored(op)?uxValorAtual(op):-1;
  ajPenSavedCount++;
  if(ajPenSavedWrite()) { sync_marcar_ajustes_pendentes_perfil(ajPenSavedProfile);return 1; }
  ajPenSavedCount--;return 0;
}
static void ajPenSavedAsk(void) {
  ajPenSavedLoad();
  if(pfTravado())return;
  if(ajPenSavedCount>=AJ_PEN_SAVED) { uxNotificar("Limite de 8 presets. Exclua um para salvar outro.");return; }
  ajPenSaveProfile=perfis_ativo();ajPenSaveKeyboard=1;
  teclado_abrir_com("Salvar preset","Nome do preset",63,CUR_ALFA_URL,NULL);
  teclado_aceitar_utf8(1);
}
static int ajPenSavedDelete(int slot) {
  ajPenSavedLoad();
  if(pfTravado() || slot<0 || slot>=ajPenSavedCount)return 0;
  char name[64];int values[AJ_N];
  memcpy(name,ajPenSavedNames[slot],sizeof name);memcpy(values,ajPenSavedValues[slot],sizeof values);
  memmove(ajPenSavedNames+slot,ajPenSavedNames+slot+1,(ajPenSavedCount-slot-1)*sizeof *ajPenSavedNames);
  memmove(ajPenSavedValues+slot,ajPenSavedValues+slot+1,(ajPenSavedCount-slot-1)*sizeof *ajPenSavedValues);
  ajPenSavedCount--;
  if(ajPenSavedWrite()) { sync_marcar_ajustes_pendentes_perfil(ajPenSavedProfile);return 1; }
  memmove(ajPenSavedNames+slot+1,ajPenSavedNames+slot,(ajPenSavedCount-slot)*sizeof *ajPenSavedNames);
  memmove(ajPenSavedValues+slot+1,ajPenSavedValues+slot,(ajPenSavedCount-slot)*sizeof *ajPenSavedValues);
  memcpy(ajPenSavedNames[slot],name,sizeof name);memcpy(ajPenSavedValues[slot],values,sizeof values);ajPenSavedCount++;return 0;
}
static int ajPenSavedPrepare(int slot) {
  ajPenSavedLoad();rvN=0;
  if(slot<0 || slot>=ajPenSavedCount)return 0;
  for(int op=0;op<AJ_N;op++) {
    int value=ajPenSavedValues[slot][op];
    if(value>=0 && ajPenPresetStored(op) && ajOpcaoDisponivel(op) && valor[op]!=value) {
      rvOps[rvN]=op;rvAntes[rvN]=valor[op];rvDepois[rvN++]=value;
    }
  }
  return rvN;
}
static const int AJ_PEN_PRESET_OPS[4][8]={
  {AJ_QUALIDADE,AJ_DTS_AC3,AJ_LEG1_TAMANHO,-1},
  {AJ_LEG_SYNC_AUDIO,AJ_LEG_SYNC_AUTO,AJ_AUDIO_OFFSET,AJ_AUDIO_NORMALIZAR,-1},
  {AJ_DET_BLUR_NAO_VISTOS,AJ_PAUSA_ELENCO,AJ_HERO_TRAILER,-1},
  {AJ_HERO,AJ_HERO_TRAILER,AJ_DET_TRAILER_AUTO,AJ_FOCO_TRAILER,-1}
};
static const int AJ_PEN_PRESET_VALS[4][8]={{0,0,140},{0,0,0,0},{0,0,0},{1,1,1,0}};
static const char *const AJ_PEN_PRESET_NAMES[]={"Sala de TV","Voz clara","Filme em casa","Só o áudio"};
#define TRI_PRESET_N 4
static int triPresetPreparar(int preset) {
  rvN=0;if(preset<0 || preset>=4)return 0;
  for(int i=0;i<8 && AJ_PEN_PRESET_OPS[preset][i]>=0;i++) {
    int op=AJ_PEN_PRESET_OPS[preset][i],value=AJ_PEN_PRESET_VALS[preset][i];
    if(ajOpcaoDisponivel(op) && valor[op]!=value) {
      rvOps[rvN]=op;rvAntes[rvN]=valor[op];rvDepois[rvN++]=value;
    }
  }
  return rvN;
}
static const char *const TRI_PRESET_DESC[]={"Qualidade automática, áudio estéreo e legendas grandes","Sincronia ligada e volume normalizado","Sem spoiler, com o elenco e o trailer no destaque","Rádio e música: sem cartaz e sem trailer"};
static const char *const TRI_PRESET_ICONE[]={"aj_sparkles","aj_volume-2","aj_clapperboard","aj_radio"};
// O preset "em uso": todos os valores dele batem com os de agora.
static int triPresetCombina(int preset) {
  int dispo=0;
  if(preset<0 || preset>=TRI_PRESET_N)return 0;
  for(int i=0;i<8 && AJ_PEN_PRESET_OPS[preset][i]>=0;i++) {
    int op=AJ_PEN_PRESET_OPS[preset][i];
    if(ajOpcaoDisponivel(op)) { dispo=1;if(valor[op]!=AJ_PEN_PRESET_VALS[preset][i])return 0; }
  }
  return dispo;
}
