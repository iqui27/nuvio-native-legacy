/* Category-sized transfers. No live list or provider URL enters this cache. */
static int vod_array(const char *body){
  if(!body)return 0;
  if(!strncmp(body,"\xef\xbb\xbf",3))body+=3;
  body+=strspn(body," \t\r\n");const char *end=js_fim(body);
  return *body=='['&&end&&end[-1]==']'&&!end[strspn(end," \t\r\n")];
}
static void vod_cache_file(char *out,size_t cap,int kind,int categories){
  snprintf(out,cap,"guiav5-vod-%s-%d-%d.json",vod_work_key,kind,categories);
}
static char *vod_safe_items(const char *body,int kind){
  Jsw w;jsw_iniciar(&w);jsw_arr_ini(&w);
  for(const char *p=js_raiz_array(body);p;){
    const char *end=js_fim(p);G5Item item;
    if(!end)break;
    if(g5_parse_xtream_item(p,end,kind?G5_SERIES:G5_MOVIE,&item)){
      jsw_obj_ini(&w);jsw_cs(&w,kind?"series_id":"stream_id",item.id);jsw_cs(&w,"name",item.name);
      jsw_cs(&w,"category_id",item.group);jsw_cs(&w,"container_extension",item.ext);
      jsw_ci(&w,"is_adult",item.adult);
      /* Panels sometimes put account-bearing playback links in image fields. */
      if(item.poster[0]&&!strchr(item.poster,'?')&&!strchr(item.poster,'@')&&!strstr(item.poster,"/movie/")&&!strstr(item.poster,"/series/")&&!strstr(item.poster,work_draft.user)&&!strstr(item.poster,work_draft.password))
        jsw_cs(&w,kind?"cover":"stream_icon",item.poster);
      jsw_cs(&w,"year",item.year);jsw_cs(&w,"rating",item.rating);jsw_cs(&w,"genre",item.genre);jsw_obj_fim(&w);
    }
    p=js_prox(end);
  }
  jsw_arr_fim(&w);const char *s=jsw_texto_final(&w);char *out=s?strdup(s):NULL;jsw_livre(&w);return out;
}
static char *vod_merge_items(const char *old,const char *fresh,int kind,const char *category){
  Jsw w;jsw_iniciar(&w);jsw_arr_ini(&w);
  for(int pass=0;pass<2;pass++)for(const char *p=js_raiz_array(pass?fresh:old);p;){
    const char *end=js_fim(p);if(!end)break;G5Item item;
    if(g5_parse_xtream_item(p,end,kind?G5_SERIES:G5_MOVIE,&item)&&(pass||strcmp(item.group,category))){
      size_t size=(size_t)(end-p);char *record=malloc(size+1);
      if(!record){jsw_livre(&w);return NULL;}memcpy(record,p,size);record[size]=0;jsw_bruto(&w,record);free(record);
    }
    p=js_prox(end);
  }
  jsw_arr_fim(&w);const char *s=jsw_texto_final(&w);char *out=s?strdup(s):NULL;jsw_livre(&w);return out;
}
static void vod_load_worker(void){
  Uint32 began=SDL_GetTicks();int status=0,items=0,categories=0;worker_ok=1;
  worker_snapshot_served=0;
  if(operation==8){
    for(int k=0;k<2;k++)for(int cat=0;cat<2;cat++){
      char file[160];int fresh=0;vod_cache_file(file,sizeof file,k,cat);char *body=catsnap_read(file,vod_work_stamp,&fresh);
      if(body&&!vod_array(body)){free(body);body=NULL;}
      if(body&&fresh)worker_snapshot_served=1;
      if(cat&&(k?work_draft.series:work_draft.movies)&&(!body||!fresh))worker_categories_missing=1;
      if(cat){free(vod_pending.categories[k]);vod_pending.categories[k]=body;}
      else{free(vod_pending.json[k]);vod_pending.json[k]=body;}
    }
  }else if(operation==6){
    for(int k=0;k<2&&!worker_cancel;k++){
      if(k?!work_draft.series:!work_draft.movies)continue;
      char file[160];int fresh=0;vod_cache_file(file,sizeof file,k,1);
      char *cached=catsnap_read(file,vod_work_stamp,&fresh);
      if(cached&&fresh&&vod_array(cached)){
        worker_snapshot_served=1;
        free(vod_pending.categories[k]);vod_pending.categories[k]=cached;continue;
      }
      free(cached);
      char *body=xtream_consultar(work_draft.server,work_draft.user,work_draft.password,k?"get_series_categories":"get_vod_categories",15,&status);
      if(status<200||status>=300||!vod_array(body)){free(body);worker_ok=0;continue;}
      Jsw w;jsw_iniciar(&w);jsw_arr_ini(&w);
      for(const char *p=js_raiz_array(body);p;){
        const char *end=js_fim(p);if(!end)break;
        char id[64]="",name[64]="";js_texto(p,end,"category_id",id,sizeof id);
        if(!id[0])snprintf(id,sizeof id,"%.0f",js_num(p,end,"category_id",-1));
        js_texto(p,end,"category_name",name,sizeof name);
        if(id[0]&&strspn(id,"0123456789")==strlen(id)&&name[0]){
          jsw_obj_ini(&w);jsw_cs(&w,"category_id",id);jsw_cs(&w,"category_name",name);jsw_obj_fim(&w);
        }
        p=js_prox(end);
      }
      jsw_arr_fim(&w);const char *safe=jsw_texto_final(&w);char *clean=safe?strdup(safe):NULL;
      jsw_livre(&w);free(body);
      if(!clean){worker_ok=0;continue;}
      if(!worker_cancel)catsnap_write(file,clean,vod_work_stamp);
      free(vod_pending.categories[k]);vod_pending.categories[k]=clean;
    }
  }else{
    char request[160];snprintf(request,sizeof request,"%s&category_id=%s",vod_request_kind?"get_series":"get_vod_streams",vod_request_id);
    char file[240];int fresh=0;
    snprintf(file,sizeof file,"guiav5-vod-%s-%d-item-%s.json",vod_work_key,vod_request_kind,vod_request_id);
    char *body=catsnap_read(file,vod_work_stamp,&fresh);
    if(fresh&&vod_array(body))worker_snapshot_served=1;
    if(!fresh||!vod_array(body)){free(body);body=xtream_consultar(work_draft.server,work_draft.user,work_draft.password,request,30,&status);fresh=0;}
    if((!fresh&&(status<200||status>=300))||!vod_array(body))worker_ok=0;
    else{
      char *safe=vod_safe_items(body,vod_request_kind);
      if(safe&&!fresh&&!worker_cancel)catsnap_write(file,safe,vod_work_stamp);
      char *merged=safe?vod_merge_items(vod_pending.json[vod_request_kind],safe,vod_request_kind,vod_request_id):NULL;
      free(safe);if(!merged)worker_ok=0;
      else{free(vod_pending.json[vod_request_kind]);vod_pending.json[vod_request_kind]=merged;}
    }
    free(body);
  }
  for(int k=0;k<2;k++){
    for(const char *p=js_raiz_array(vod_pending.json[k]);p;p=js_prox(js_fim(p)))items++;
    for(const char *p=js_raiz_array(vod_pending.categories[k]);p;p=js_prox(js_fim(p)))categories++;
    if(operation==7&&worker_ok&&!worker_cancel){
      const char *body=vod_pending.json[k];char file[160];vod_cache_file(file,sizeof file,k,0);
      if(body)catsnap_write(file,body,vod_work_stamp);
    }
  }
  if(!worker_cancel)vod_index(&vod_pending,0);
  printf("[vod-load] phase=%s http=%d categories=%d items=%d ok=%d ms=%u\n",operation==8?"cache":operation==6?"categories":"items",status,categories,items,worker_ok,SDL_GetTicks()-began);
}
static int vod_fallback_required(void){
  return !worker_ok&&(operation==6||operation==7)&&!vod_snapshot_served;
}
static void vod_begin(void){
  if(atomic_load(&busy)){vod_begin_pending=1;return;}
  use_saved=1;
  vod_refresh_revision=guia_catalog_refresh_revision();
  vod_snapshot_served=0;
  vod_refresh_stamp=catsnap_refresh_stamp(profile);
  vod_cache_checked=0;vod_failed=0;
  memset(vod_done_count,0,sizeof vod_done_count);
  vod_lazy=1;
  if(!committed)draft.movies=draft.series=1;
  char server[256],user[96],password[96],identity[700];
  xtream_dados_copiar(server,sizeof server,user,sizeof user,password,sizeof password);
  snprintf(identity,sizeof identity,"%d\n%s\n%s\n%s\n%s",profile,account,server,user,password);
  admin_sha256_hex((const unsigned char*)identity,strlen(identity),vod_cache_key);erase(password,sizeof password);erase(identity,sizeof identity);
  action[0]=0;
  if(!vod_cache_checked){vod_cache_checked=1;start(8);}
  else if(!source_loaded&&!vod_failed)start(6);
}
static void vod_tick(void){
  if(!vod_lazy||vod_failed||atomic_load(&busy)||strncmp(screen,"vod-",4)||!strcmp(screen,"vod-titulo")||!strcmp(screen,"vod-serie-temporadas"))return;
  if(!source_loaded){start(6);return;}
  int active=vod_rows?vod_row_at(vod_position()):0,eligible=0;
  for(int r=0;r<vod_data.nrows;r++){
    VodRow *row=&vod_data.rows[r];int k=row->kind==G5_SERIES;
    if((k?!draft.series:!draft.movies)||(!strcmp(screen,"vod-series")&&!k)||!group_allowed(row->name)||(rules.hide_adult&&adult_group(row->name))||(catalog_group[0]&&strcmp(catalog_group,row->name)))continue;
    int done=0;for(int i=0;i<vod_done_count[k];i++)done|=!strcmp(vod_done[k][i],row->id);
    /* Completed empty categories have no navigable row. */
    if(done&&!row->count)continue;
    int position=eligible++;if(!query[0]&&(position<active||position>active+1))continue;
    if(done)continue;
    if(strspn(row->id,"0123456789")!=strlen(row->id)||!row->id[0])continue;
    vod_request_kind=k;snprintf(vod_request_id,sizeof vod_request_id,"%s",row->id);start(7);return;
  }
}
