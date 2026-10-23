// Owned by the existing social worker and profile-session publication guard.
static AmigoPerfil contaPerfis[CONTA_PERFIL_MAX];
static int contaPerfisN, contaPerfisEstado, contaPerfisOp, contaPerfilAdicionar, contaPerfisCompativel;

static void limparPerfisConta(void) {
  // limparEstado calls before taking the module mutex.
  if (mtx) SDL_LockMutex(mtx);
  contaPerfisN=contaPerfisEstado=contaPerfisOp=contaPerfilAdicionar=contaPerfisCompativel=0;
  memset(contaPerfis,0,sizeof contaPerfis);
  if (mtx) SDL_UnlockMutex(mtx);
}

int amigosperfis_estado(void) {
  if (!recomenda_ativo() || !mtx) return AP_NADA;
  SDL_LockMutex(mtx); int e=contaPerfisEstado; SDL_UnlockMutex(mtx); return e;
}

int amigosperfis_listar(AmigoPerfil *out,int cap) {
  if (!out || cap<1 || !recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx);
  int n=contaPerfisN<cap?contaPerfisN:cap;
  memcpy(out,contaPerfis,(size_t)n*sizeof *out);
  SDL_UnlockMutex(mtx); return n;
}

int amigosperfis_carregar(void) {
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx);
  if (contaPerfisOp || contaPerfisEstado==AP_CARREGANDO || vincEstado==REC_VINC_INDO) { SDL_UnlockMutex(mtx); return 0; }
  contaPerfisOp=1; contaPerfisN=0; contaPerfisEstado=AP_CARREGANDO;
  SDL_UnlockMutex(mtx); recomenda_pedir_agora(); return 1;
}

int amigosperfis_adicionar(int indice) {
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx); int ok=0;
  if (!contaPerfisOp && vincEstado!=REC_VINC_INDO && contaPerfisEstado==AP_PRONTO)
    for (int i=0;i<contaPerfisN;i++)
      if (contaPerfis[i].perfil.indice==indice && contaPerfis[i].estado==AP_DISPONIVEL) ok=1;
  if (ok) {
    contaPerfisOp=2; contaPerfilAdicionar=indice; vincEstado=REC_VINC_INDO; vincNome[0]=0;
  }
  SDL_UnlockMutex(mtx);
  if (ok) recomenda_pedir_agora();
  return ok;
}

// Older Social servers already support profile-scoped registration and code
// pairing. Use those routes only after an explicit selection; never activate a
// profile, change its sharing preferences or retain/log its pairing code.
static int perfisContaAtual(unsigned ger) {
  if (!recTravarFio()) return 0;
  int atual=ger==geracao;
  recDestravarFio(); return atual;
}

static int perfisContaTemLista(const char *r,const char *chave) {
  if (js_array(r,NULL,chave)) return 1;
  char vazio[16];
  if (!js_bruto(r,NULL,chave,vazio,sizeof vazio)) return 0;
  const char *p=vazio+strspn(vazio," \t\r\n");
  if (*p++!='[') return 0;
  p+=strspn(p," \t\r\n");
  if (*p++!=']') return 0;
  return !p[strspn(p," \t\r\n")];
}

static int perfisContaPuxar(const char *aut,unsigned ger,const char *eu,
                           AmigoPerfil *saida,int *n) {
  int st=0, achouEu=0, linhas=0;
  char base[96];
  snprintf(base,sizeof base,"%s",eu);
  char *sufixo=strchr(base+6,':');
  if (sufixo) *sufixo=0;
  if (!base[6] || !perfisContaAtual(ger)) return 0;
  char *r=nuvem_rpc_com("sync_pull_profiles","{}",aut+sizeof("Authorization: Bearer ")-1,&st);
  printf("[recomenda] perfis da conta lista autenticada HTTP %d\n",st);
  if (!r || st<200 || st>=300 || !perfisContaAtual(ger)) { free(r); return 0; }
  *n=0;
  for (const char *p=js_raiz_array(r); p && *p=='{'; p=js_prox(js_fim(p))) {
    const char *f=js_fim(p);
    double idx=js_num(p,f,"profile_index",0);
    if (!idx) idx=js_num(p,f,"id",0);
    if (++linhas>32) { free(r); return 0; }
    if (!(idx>=1 && idx<=32) || idx!=(int)idx) continue;
    AmigoPerfil a; memset(&a,0,sizeof a);
    a.perfil.indice=(int)idx;
    char primario[16];
    a.perfil.primario=js_bruto(p,f,"is_primary",primario,sizeof primario)
      ? (!strcmp(primario,"true") || !strcmp(primario,"1")) : idx==1;
    int k=snprintf(a.id,sizeof a.id,a.perfil.primario?"%s":"%s:%d",base,(int)idx);
    if (k<0 || (size_t)k>=sizeof a.id) { free(r); return 0; }
    if (!strcmp(a.id,eu)) { achouEu=1; continue; }
    int duplicado=0;
    for (int i=0;i<*n;i++) if (saida[i].perfil.indice==(int)idx || !strcmp(saida[i].id,a.id)) duplicado=1;
    if (duplicado) continue;
    if (*n>=CONTA_PERFIL_MAX) { free(r); return 0; }
    js_texto(p,f,"name",a.perfil.nome,sizeof a.perfil.nome);
    js_texto(p,f,"avatar_url",a.perfil.avatarUrl,sizeof a.perfil.avatarUrl);
    if (!a.perfil.nome[0]) snprintf(a.perfil.nome,sizeof a.perfil.nome,"Perfil %d",(int)idx);
    a.estado=AP_DISPONIVEL; saida[(*n)++]=a;
  }
  free(r); return achouEu;
}

static int perfisContaCompat(const char *aut,const char **headers,unsigned ger,
                            const char *eu,int indice,AmigoPerfil *lista,int *n) {
  if (!perfisContaPuxar(aut,ger,eu,lista,n)) return 0;
  int st=0;
  if (!indice) {
    if (!perfisContaAtual(ger)) return 0;
    url("/v1/contatos");
    char *r=recBaixar(fioUrl,REC_TEMPO_REDE,headers,&st);
    if (!r || st<200 || st>=300 || !perfisContaAtual(ger) || !perfisContaTemLista(r,"contatos")) { free(r); return 0; }
    for (const char *p=js_array(r,NULL,"contatos"); p && *p=='{'; p=js_prox(js_fim(p))) {
      char id[96]=""; js_texto(p,js_fim(p),"id",id,sizeof id);
      for (int i=0;i<*n;i++) if (!strcmp(id,lista[i].id)) lista[i].estado=AP_AMIGO;
    }
    free(r); return 1;
  }
  AmigoPerfil *alvo=NULL;
  for (int i=0;i<*n;i++) if (lista[i].perfil.indice==indice) alvo=&lista[i];
  if (!alvo || !perfisContaAtual(ger)) return 0;
  // Verify the current Nuvio identity before acting for a second profile. The
  // normal worker may be using a linked Trakt identity; never pair two others.
  url("/v1/eu");
  char *r=recPostar(fioUrl,REC_TEMPO_REDE,headers,"{}",&st), id[96]="";
  int ok=r && st>=200 && st<300 && js_texto_raiz(r,"id",id,sizeof id) && !strcmp(id,eu);
  free(r);
  if (!ok || !perfisContaAtual(ger)) return 0;
  char perfil[40], nome[400], avatar[1900], corpo[2400], codigo[16]="";
  snprintf(perfil,sizeof perfil,"X-Nuvio-Perfil: %d",indice);
  const char *destino[]={aut,"X-Nuvio-Auth: nuvio",alvo->perfil.primario?NULL:perfil,NULL};
  jsonEsc(nome,sizeof nome,alvo->perfil.nome);
  jsonEsc(avatar,sizeof avatar,alvo->perfil.avatarUrl);
  snprintf(corpo,sizeof corpo,"{\"nome\":\"%s\",\"avatar\":\"%s\"}",nome,avatar);
  url("/v1/eu");
  r=recPostar(fioUrl,REC_TEMPO_REDE,destino,corpo,&st);
  printf("[recomenda] perfis da conta registrar selecionado HTTP %d\n",st);
  id[0]=0;
  ok=r && st>=200 && st<300 && js_texto_raiz(r,"id",id,sizeof id) && !strcmp(id,alvo->id)
    && js_texto_raiz(r,"codigo",codigo,sizeof codigo) && strlen(codigo)==6
    && strspn(codigo,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789")==6;
  free(r);
  if (ok && perfisContaAtual(ger)) {
    snprintf(corpo,sizeof corpo,"{\"codigo\":\"%s\"}",codigo);
    url("/v1/contatos");
    r=recPostar(fioUrl,REC_TEMPO_REDE,headers,corpo,&st);
    printf("[recomenda] perfis da conta vincular selecionado HTTP %d\n",st);
    id[0]=0;
    char contato[1024]="";
    int temContato=r && js_bruto(r,NULL,"contato",contato,sizeof contato);
    ok=r && st>=200 && st<300 && js_num(r,NULL,"ok",0)==1 && temContato
      && js_texto_raiz(contato,"id",id,sizeof id) && !strcmp(id,alvo->id);
    free(r);
  } else ok=0;
  memset(codigo,0,sizeof codigo); memset(corpo,0,sizeof corpo);
  if (ok && perfisContaAtual(ger)) {
    if (!recTravarFio()) return 0;
    if (ger!=geracao) { recDestravarFio(); return 0; }
    snprintf(vincNome,sizeof vincNome,"%s",alvo->perfil.nome);
    recDestravarFio();
    return 1;
  }
  return 0;
}

static void tratarPerfisConta(const char **cab) {
  if (!recTravarFio()) return;
  unsigned ger=geracao;
  int op=contaPerfisOp, indice=contaPerfilAdicionar, compativel=contaPerfisCompativel; contaPerfisOp=0;
  char eu[96], aut[3100]="", via[]="X-Nuvio-Auth: nuvio", pf[40]="";
  snprintf(eu,sizeof eu,"%s",meuId);
  int nuvio=op && !strncmp(eu,"nuvio:",6) && bearerSessaoLocal(aut,sizeof aut);
  if (nuvio) perfilCab(pf,sizeof pf,1);
  recDestravarFio();
  if (!op) return;
  if (op==1) compativel=0;
  if (!nuvio) {
    if (recTravarFio()) {
      contaPerfisEstado=AP_CONTA_NUVIO;
      if (op==2) vincEstado=REC_VINC_FALHA;
      recDestravarFio();
    }
    return;
  }
  const char *headers[]={aut,via,pf[0]?pf:NULL,NULL};
  int st=0; char *r=NULL;
  url("/v1/contatos/perfis");
  if (op==1) r=recBaixar(fioUrl,REC_TEMPO_REDE,headers,&st);
  else if (!compativel) {
    char corpo[48]; snprintf(corpo,sizeof corpo,"{\"perfil\":%d}",indice);
    r=recPostar(fioUrl,REC_TEMPO_REDE,headers,corpo,&st);
  }
  AmigoPerfil *legado=NULL; int legadoN=0, legadoOk=0;
  if (st==404 || (op==2 && compativel)) {
    free(r); r=NULL; compativel=1;
    legado=calloc(CONTA_PERFIL_MAX,sizeof *legado);
    if (legado) legadoOk=perfisContaCompat(aut,headers,ger,eu,op==2?indice:0,legado,&legadoN);
  }
  memset(aut,0,sizeof aut);
  printf("[recomenda] perfis da conta op=%d HTTP %d compat=%d resultado=%d\n",op,st,compativel,legadoOk);
  if (!recTravarFio()) { free(r); free(legado); return; }
  if (ger!=geracao) { recDestravarFio(); free(r); free(legado); return; }
  int ok=legado?legadoOk:st>=200 && st<300 && r;
  if (compativel && !legado) ok=0;
  if (op==1) {
    contaPerfisN=0;
    contaPerfisCompativel=compativel;
    contaPerfisEstado=ok?AP_PRONTO:AP_ERRO;
    if (legado && ok) {
      memcpy(contaPerfis,legado,(size_t)legadoN*sizeof *legado); contaPerfisN=legadoN;
    }
    const char *p=ok && !legado?js_array(r,NULL,"perfis"):NULL;
    if (ok && !legado && !p && !perfisContaTemLista(r,"perfis")) contaPerfisEstado=AP_ERRO;
    while (p && *p=='{' && contaPerfisN<CONTA_PERFIL_MAX) {
      const char *f=js_fim(p); AmigoPerfil a; memset(&a,0,sizeof a);
      double idx=js_num(p,f,"indice",0); int duplicado=0;
      if (!(idx>=1 && idx<=32)) { p=js_prox(f); continue; }
      a.perfil.indice=(int)idx;
      for (int i=0;i<contaPerfisN;i++) if(contaPerfis[i].perfil.indice==a.perfil.indice) duplicado=1;
      if (idx>=1 && idx<=32 && idx==a.perfil.indice && a.perfil.indice!=recPerfil && !duplicado) {
        char estado[24]="";
        js_texto(p,f,"id",a.id,sizeof a.id);
        js_texto(p,f,"nome",a.perfil.nome,sizeof a.perfil.nome);
        js_texto(p,f,"avatar",a.perfil.avatarUrl,sizeof a.perfil.avatarUrl);
        js_texto(p,f,"estado",estado,sizeof estado);
        a.estado=!strcmp(estado,"amigo")?AP_AMIGO:!strcmp(estado,"disponivel")?AP_DISPONIVEL:AP_BLOQUEADO;
        if (a.id[0] && a.perfil.nome[0]) contaPerfis[contaPerfisN++]=a;
      }
      p=js_prox(f);
    }
  } else {
    vincEstado=ok?REC_VINC_OK:REC_VINC_FALHA;
    if (ok) {
      if (r) js_texto(r,r+strlen(r),"nome",vincNome,sizeof vincNome);
      for(int i=0;i<contaPerfisN;i++) if(contaPerfis[i].perfil.indice==indice) contaPerfis[i].estado=AP_AMIGO;
    }
  }
  recDestravarFio(); free(r); free(legado);
  if (op==2 && ok) lerContatos(cab);
}
