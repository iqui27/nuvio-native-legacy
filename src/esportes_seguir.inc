/* Follow preferences share the existing account/profile Sports file. */
static int seguirId(const char *s) {
  if(!s || !*s)return 0;
  for(;*s;s++)if(!isalnum((unsigned char)*s)&&*s!='-'&&*s!='.'&&*s!='_')return 0;
  return 1;
}
int esportes_segue_time(const EspEvento *e,int p){return esportes_favorito(e,p);}
int esportes_seguir_time(const EspEvento *e,int p,int on){
  esportes_abrir_sessao();
  if(!e || !seguirId(e->modalidade)||!seguirId(e->ligaId)||p<0||p>=e->nParticipantes||p>=ESP_MAX_PARTICIPANTES||!seguirId(e->participantes[p].id))return 0;
  return esportes_favorito(e,p)==!!on ? 1 : esportes_alternar_favorito(e,p);
}
static int jogoIndice(const EspEvento *e){
  if(e)for(int i=0;i<nJogosSeguidos;i++)if(!strcmp(e->id,jogosSeguidos[i].id)&&!strcmp(e->ligaId,jogosSeguidos[i].ligaId)&&!strcmp(e->modalidade,jogosSeguidos[i].modalidade))return i;
  return -1;
}
/* All provider surfaces update the followed snapshot by fetch time, not publish order. */
static void atualizarSeguidos(const EspJob *j){
  int changed=0;
  for(int k=0;k<j->n;k++){
    int i=jogoIndice(&j->eventos[k]);
    if(i<0 || j->eventos[k].obtido<=jogosSeguidos[i].obtido)continue;
    jogosSeguidos[i]=j->eventos[k];
    if(jogosSeguidos[i].fim<=jogosSeguidos[i].inicio)jogosSeguidos[i].fim=jogosSeguidos[i].inicio+3*3600;
    changed=1;
  }
  if(changed)gravar();
}
int esportes_segue_jogo(const EspEvento *e){return jogoIndice(e)>=0;}
int esportes_segue_evento(const EspEvento *e){return esportes_segue_jogo(e)||esportes_favorito(e,0)||esportes_favorito(e,1);}
int esportes_jogos_seguidos_n(void){return nJogosSeguidos;}
const EspEvento *esportes_jogo_seguido(int i){return i>=0&&i<nJogosSeguidos?&jogosSeguidos[i]:NULL;}
int esportes_seguir_jogo(const EspEvento *e,int on){
  esportes_abrir_sessao();
  if(!e||!seguirId(e->id)||!seguirId(e->modalidade)||!seguirId(e->ligaId)||!e->nome[0]||strpbrk(e->nome,"\n\r|"))return 0;
  int i=jogoIndice(e);if((i>=0)==!!on)return 1;
  if(on && (nJogosSeguidos==64 || e->estado>1 || e->inicio<=0))return 0;
  EspEvento old={0};
  if(on){i=nJogosSeguidos++;jogosSeguidos[i]=*e;if(jogosSeguidos[i].fim<=e->inicio)jogosSeguidos[i].fim=e->inicio+3*3600;}
  else{old=jogosSeguidos[i];jogosSeguidos[i]=jogosSeguidos[--nJogosSeguidos];}
  if(!gravar()){
    if(on)nJogosSeguidos--;
    else{jogosSeguidos[nJogosSeguidos++]=jogosSeguidos[i];jogosSeguidos[i]=old;}
    return 0;
  }
  revisao++;return 1;
}
