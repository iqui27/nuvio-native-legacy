/* One TV setup page: source actions, guide options and category visibility. */
static float sources_on[4],sources_velocity[4];
static GuiaCategoria sources_categories[30000];
static int sources_category_count;
static unsigned sources_category_revision=~0u;
static uint64_t sources_category_session;
static int sources_category_language=-1;
static void sources_switch(int id,const char *name,int on,float x,float y,float w){
  button(id,name,NULL,x,y,w,64,1);
  ajustes_ui_interruptor(x+w-18,y+32,on,1);
}
static void sources_screen(void){
  /* Follow source identity if a background publication changes list order. */
  if(focus>=100&&focus<116&&sources_key[0]){
    GuiaFonteSaude row;
    for(int i=0;guia_fonte_saude(i,&row);i++)if(!strcmp(row.chave,sources_key)){
      sources_offset=i/4*4;focus=100+(i%4)*4+(focus-100)%4;break;
    }
  }
  t21_ilha_folha((GfxRect){48,176,1824,832},28,1);
  plrui_kicker(i18n("Guia TV"),80,204,154,156,158,1);
  label(TXT_ILHA_TITULO,i18n("Fontes"),80,232,850,0);
  button(8,i18n("Voltar"),"aj_arrow-left",1530,210,310,56,0);
  label(TXT_ILHA_NOME,i18n("Canais ao vivo"),80,306,980,0);
  button(0,i18n("Adicionar fonte"),"aj_plus",780,294,350,56,0);
  GuiaFonteSaude source;if(!guia_fonte_saude(sources_offset,&source))sources_offset=0;
  int n=0;
  for(;n<4&&guia_fonte_saude(sources_offset+n,&source);n++){
    float y=374+n*136;int on=guia_fonte_ligada(sources_offset+n),id=100+n*4;
    if(focus>=id&&focus<id+4&&nav<0)snprintf(sources_key,sizeof sources_key,"%s",source.chave);
    char status[180],stamp[48]="";
    if(source.atualizada){time_t t=source.atualizada;struct tm tm;localtime_r(&t,&tm);strftime(stamp,sizeof stamp," · %d/%m %H:%M",&tm);}
    snprintf(status,sizeof status,"%s · %d %s · EPG %d/%d%s",i18n(on?"Ligada":"Desligada"),source.canais,i18n("Canais"),source.comGuia,source.canais,stamp);
    if(nav<0&&focus==id)plrui_linha_foco(foco_rect((GfxRect){80,y,450,64}),20,1);
    label(TXT_ILHA_NOME,source.nome,104,y+17,330,!on);
    ponteiro_alvo(80,y,450,64,point_focus,point_activate,id,0);reg_focus(id,80,y,450,64);
    sources_on[n]=anim_mola2(&sources_velocity[n],sources_on[n],on,gv_dt,T21_FOCO);
    ajustes_ui_interruptor(512,y+32,sources_on[n],1);
    label(TXT_ILHA_SUB,status,104,y+76,1000,1);
    button(id+1,i18n("Atualizar"),"aj_rotate-cw",550,y+4,190,56,0);
    button(id+2,i18n("Editar"),"aj_pencil",752,y+4,160,56,0);
    button(id+3,i18n(sources_remove==sources_offset+n?"Confirmar":"Remover"),"aj_trash-2",924,y+4,208,56,0);
    if(sources_remove==sources_offset+n)label(TXT_ILHA_SUB,i18n("Confirmar remoção"),650,y+76,470,1);
  }
  if(!n)label(TXT_ILHA_CORPO,i18n("Nenhuma fonte"),104,398,900,1);
  if(sources_offset)button(6,i18n("Anterior"),"aj_chevron-left",80,936,200,56,0);
  if(guia_fonte_saude(sources_offset+4,&source))button(7,i18n("Próximo"),"aj_chevron-right",900,936,230,56,0);
  gfx_cor((GfxRect){1176,306,1,658},0,1,1,1,.08f);
  plrui_kicker(i18n("Opções da grade"),1212,306,154,156,158,1);
  button(200,i18n("Alcance do guia"),"aj_calendar-days",1212,354,600,64,1);
  label(TXT_ILHA_SUB,i18n(ajustes_guia_dias()==7?"Sete dias":ajustes_guia_dias()==3?"Três dias":"Um dia"),1534,374,240,1);
  button(201,i18n("URL XMLTV"),"aj_calendar",1212,424,600,56,1);
  button(204,i18n("Opções"),"aj_sliders-horizontal",1280,210,230,56,0);
  sources_switch(202,i18n("Agrupar variantes dos canais"),ajustes_livetv_agrupar(),1212,494,600);
  button(203,i18n("Ordem do guia"),"aj_arrow-down",1212,564,600,64,1);
  label(TXT_ILHA_SUB,i18n(ajustes_guia_ordem()?"Conta":"Categoria"),1236,624,550,1);
  gfx_cor((GfxRect){1236,672,552,1},0,1,1,1,.08f);
  label(TXT_ILHA_NOME,i18n("Categorias"),1236,694,550,0);
  unsigned revision=guia_busca_revisao();uint64_t session=perfis_sessao_geracao();
  int language=ajustes_idioma();
  if(sources_category_revision!=revision||sources_category_session!=session||sources_category_language!=language){
    sources_category_count=guia_categorias(sources_categories,30000);
    sources_category_revision=revision;sources_category_session=session;sources_category_language=language;
  }
  if(sources_category_offset>=sources_category_count)sources_category_offset=0;
  for(int i=0;i<3&&sources_category_offset+i<sources_category_count;i++){
    GuiaCategoria *c=&sources_categories[sources_category_offset+i];
    sources_switch(300+i,c->nome,!c->oculta,1212,738+i*66,600);
  }
  if(sources_category_offset)button(9,i18n("Anterior"),"aj_chevron-left",1212,942,200,50,0);
  if(sources_category_offset+3<sources_category_count)button(10,i18n("Próximo"),"aj_chevron-right",1590,942,222,50,0);
}
static int sources_activate(int id){
  if(id==8){sources_key[0]=0;pg_tras();return 1;}
  if(id==0){
    if(atomic_load(&busy))return 1;
    sources_saved_draft=draft;sources_saved_use=use_saved;
    sources_saved_inventory=inventory;sources_saved_count=count;sources_saved_total=total;sources_saved_loaded=source_loaded;
    sources_saved_groups=group_count;memcpy(sources_saved_names,groups,sizeof groups);memcpy(sources_saved_on,group_on,sizeof group_on);
    inventory=NULL;count=total=source_loaded=group_count=0;sources_adding=1;
    sources_key[0]=0;use_saved=0;erase(&draft,sizeof draft);go("setup-tipo");return 1;
  }
  if(id==6||id==7){sources_offset+=id==6?-4:4;sources_key[0]=0;focus=100;return 1;}
  if(id==9||id==10){sources_category_offset+=id==9?-3:3;focus=300;return 1;}
  if(id>=300&&id<303){int i=sources_category_offset+id-300;if(i<sources_category_count&&guia_categoria_definir(&sources_categories[i],!sources_categories[i].oculta))sources_category_revision=~0u;gv_snap_force=1;return 1;}
  if(id==204){sources_options=1;gv_go("ajustes");return 1;}
  if(id==200){int d=ajustes_guia_dias();ajustes_definir_guia_dias(d==1?3:d==3?7:1);gv_snap_force=1;epg_recarregar();return 1;}
  if(id==201){input(4,"URL XMLTV",draft.xmltv,1);return 1;}
  if(id==202||id==203){ajustes_rapido_passo(ajustes_rapido_op(id==202?"liveTvGroupVariantsLocal":"guiaOrdemLocal"),1);gv_snap_force=1;return 1;}
  if(id>=100&&id<116){
    int i=sources_offset+(id-100)/4,action=(id-100)%4;GuiaFonteSaude source;
    if(!guia_fonte_saude(i,&source))return 1;
    snprintf(sources_key,sizeof sources_key,"%s",source.chave);
    if(action!=3)sources_remove=-1;
    if(action==0){guia_fonte_alternar(i);gv_snap_force=dirty=1;}
    else if(action==1)guia_recarregar(0);
    else if(action==2){snprintf(sources_edit,sizeof sources_edit,"%s",source.chave);input(8,"Nome da fonte",source.nome,0);}
    else if(sources_remove!=i)sources_remove=i;
    else{if(guia_fonte_remover(i)){sources_key[0]=0;focus=100;sources_remove=-1;}else snprintf(message,sizeof message,"%s",i18n("Não foi possível salvar a lista"));}
    return 1;
  }
  return 0;
}
