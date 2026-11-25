// Owned by the existing social worker and profile-session publication guard.
static AmigoPerfil contaPerfis[CONTA_PERFIL_MAX];
static int contaPerfisN, contaPerfisEstado, contaPerfisOp, contaPerfilAdicionar;

static void limparPerfisConta(void) {
  // limparEstado calls before taking the module mutex.
  if (mtx) SDL_LockMutex(mtx);
  contaPerfisN=contaPerfisEstado=contaPerfisOp=contaPerfilAdicionar=0;
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

// "perfis" exists as a key even when the list is empty: {} on a 200 is an
// answer the route never gives, so it means the server spoke another protocol.
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

// The only paths are GET (list) and POST {perfil:n} (connect). There is NO
// fallback for servers without /v1/contatos/perfis: a 404 means the option is
// not offered (AP_INDISPONIVEL), never a pairing-code workaround — that would
// register the other profile and read its code without it being active.
static void tratarPerfisConta(const char **cab) {
  if (!recTravarFio()) return;
  unsigned ger=geracao;
  int op=contaPerfisOp, indice=contaPerfilAdicionar; contaPerfisOp=0;
  char eu[96], aut[3100]="", via[]="X-Nuvio-Auth: nuvio", pf[40]="";
  snprintf(eu,sizeof eu,"%s",meuId);
  int nuvio=op && !strncmp(eu,"nuvio:",6) && bearerSessaoLocal(aut,sizeof aut);
  if (nuvio) perfilCab(pf,sizeof pf,1);
  recDestravarFio();
  if (!op) return;
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
  else {
    char corpo[48]; snprintf(corpo,sizeof corpo,"{\"perfil\":%d}",indice);
    r=recPostar(fioUrl,REC_TEMPO_REDE,headers,corpo,&st);
  }
  memset(aut,0,sizeof aut);
  printf("[recomenda] perfis da conta op=%d HTTP %d\n",op,st);
  if (!recTravarFio()) { free(r); return; }
  if (ger!=geracao) { recDestravarFio(); free(r); return; }
  int ok=st>=200 && st<300 && r;
  if (op==1) {
    contaPerfisN=0;
    contaPerfisEstado=ok?AP_PRONTO:st==404?AP_INDISPONIVEL:AP_ERRO;
    const char *p=ok?js_array(r,NULL,"perfis"):NULL;
    if (ok && !p && !perfisContaTemLista(r,"perfis")) contaPerfisEstado=AP_ERRO;
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
  recDestravarFio(); free(r);
  if (op==2 && ok) lerContatos(cab);
}
