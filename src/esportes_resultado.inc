/* Parse once with the card, never infer a winner from a lexicographic score. */
static int esp_score(const char *s,double *value){
  char *end;*value=strtod(s,&end);return end!=s&&!*end&&isfinite(*value);
}
static void esp_team_color(const EspParticipante *p,float *r,float *g,float *b){
  const char *s=p->cor;unsigned value=0;
  if(*s=='#')s++;
  int valid=strlen(s)==6;
  for(int i=0;valid&&i<6;i++){
    unsigned c=(unsigned char)s[i];int hex=c>='0'&&c<='9'?(int)c-'0':c>='a'&&c<='f'?(int)c-'a'+10:c>='A'&&c<='F'?(int)c-'A'+10:-1;
    if(hex<0)valid=0;else value=(value<<4)|(unsigned)hex;
  }
  *r=valid?((value>>16)&255)/255.f:.35f;*g=valid?((value>>8)&255)/255.f:.37f;*b=valid?(value&255)/255.f:.42f;
}
static void resultadoEvento(const EspEvento *e,GfxRect r){
  float half=r.w*.5f,cr,cg,cb;double scores[2];
  int numeric=esp_score(e->participantes[0].pontos,&scores[0])&&esp_score(e->participantes[1].pontos,&scores[1]);
  for(int p=0;p<2;p++){
    const EspParticipante *team=&e->participantes[p];float x=r.x+p*half;
    esp_team_color(team,&cr,&cg,&cb);
    gfx_cor((GfxRect){x+8,r.y+8,half-16,r.h-16},20/r.h,cr*.24f+.04f,cg*.24f+.04f,cb*.24f+.04f,1);
    int winner=numeric&&scores[p]>scores[1-p];
    logo(team->logo,modalidadeEvento(e),(GfxRect){x+(half-76)*.5f,r.y+24,76,76});
    txt(TXT_ILHA_NOME,team->curto[0]?team->curto:team->nome,x+16,r.y+116,half-32,winner?245:180);
    txt(winner?TXT_AJ_NUM64:TXT_AJ_NUM58,team->pontos,x+half*.5f-32,r.y+158,half-42,winner?245:180);
  }
  txt(TXT_ILHA_APOIO,e->liga,r.x+22,r.y+r.h-62,r.w-44,210);
  txt(TXT_ILHA_APOIO,i18n("Encerrado"),r.x+22,r.y+r.h-34,r.w-44,170);
}
