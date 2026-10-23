// Compatibility for native-account callers. Programme storage remains in the
// streaming, source-keyed engine; account IDs never select another account.
static int contaChave(int tipo, unsigned id, char chave[24]) {
  if (tipo == 1) { if (!id) snprintf(chave,24,"xt"); else snprintf(chave,24,"xta%08x",id); }
  else if (tipo == 2) snprintf(chave,24,"ma%08x",id);
  else if (tipo == 3) { if (!id) snprintf(chave,24,"st"); else snprintf(chave,24,"sta%08x",id); }
  else return 0;
  return 1;
}
static int canalChave(const char *canal, char chave[24]) {
  unsigned id;
  if (iptvid_ler("xtream:",canal,&id,NULL,0)) return contaChave(1,id,chave);
  if (iptvid_ler("nvm3u:",canal,&id,NULL,0)) return contaChave(2,id,chave);
  if (iptvid_ler("stalker:",canal,&id,NULL,0)) return contaChave(3,id,chave);
  chave[0]=0; return 0;
}
void epg_contas_configurar(const EpgContaFonte *fontes, int n) {
  EpgExtra v[EPG_MAX_EXTRAS]; char chaves[EPG_MAX_EXTRAS][24]; int k=0;
  if (!fontes || n<0) n=0;
  for (int i=0;i<n && k<EPG_MAX_EXTRAS;i++) {
    if (!fontes[i].url[0] || !contaChave(fontes[i].tipo,fontes[i].id,chaves[k])) continue;
    v[k]=(EpgExtra){.chave=chaves[k],.url=fontes[i].url}; k++;
  }
  epg_extras_definir(v,k);
}
void epg_conta_ids(int tipo, unsigned id, const char *const *ids, int n) {
  char chave[24]; if (!contaChave(tipo,id,chave)) return;
  pthread_mutex_lock(&trava);
  for (int i=0;i<nExtras;i++) if (!strcmp(extras[i].chave,chave)) {
    EpgExtraInt novo;
    extraMontar(&novo,chave,extras[i].url,ids,n,NULL,0);
    int mudou=novo.hFiltro!=extras[i].hFiltro;
    extraSoltar(&extras[i]); extras[i]=novo;
    if (mudou) {
      extraMudou=1;
      if (fioVivo) { carga.cancelada=1; reiniciarPend=1; }
    }
    break;
  }
  pthread_mutex_unlock(&trava);
}
int epg_match_canal(const char *canal, const char *epgId, const char *nome) {
  char chave[24];
  return epg_casar(canalChave(canal,chave)?chave:NULL,epgId,NULL,nome,NULL);
}
int epg_fonte_estado(const char *chave) {
  if (!chave || !chave[0]) return epg_estado();
  for (int i=0;i<nInfoPub;i++) if (!strcmp(infoPub[i].chave,chave) && fontePorChave(chave)>=0)
    return infoPub[i].ok ? EPG_PRONTO : EPG_FALHOU;
  for (int i=0;i<nExtras;i++) if (!strcmp(extras[i].chave,chave))
    return fioVivo || extraMudou ? EPG_BAIXANDO : EPG_FALHOU;
  return epg_estado();  // no private XMLTV: the public country schedule may match
}
int epg_canal_estado(const char *canal) {
  char chave[24];
  return epg_fonte_estado(canalChave(canal,chave)?chave:NULL);
}
unsigned epg_revisao(void) { return epgRevisao; }
void epg_fonte_m3u(const char *url) {
  if (!url) url="";
  pthread_mutex_lock(&trava);
  if (strcmp(url,legadoM3uUrl)) {
    snprintf(legadoM3uUrl,sizeof legadoM3uUrl,"%s",url); extraMudou=1;
    if (fioVivo) { carga.cancelada=1; reiniciarPend=1; }
  }
  pthread_mutex_unlock(&trava);
}
void epg_so_provedores(int so) {
  pthread_mutex_lock(&trava);
  if (soProvedores!=!!so) {
    soProvedores=!!so; fontesMudaram=1;
    if (fioVivo) { carga.cancelada=1; reiniciarPend=1; }
  }
  pthread_mutex_unlock(&trava);
}
void epg_recarregar(void) {
  // Force the next request, retaining every last working cache as a fallback.
  pthread_mutex_lock(&trava);
  forcarRede=1; extraMudou=1;
  if (fioVivo) { carga.cancelada=1; reiniciarPend=1; }
  pthread_mutex_unlock(&trava);
  if (!fioVivo) epg_iniciar();
}
