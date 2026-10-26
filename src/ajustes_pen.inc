/* The approved Pencil pages bind to the original setting IDs. */
typedef struct { int op; const char *title,*help,*icon; int toggle; } AjPenRow;
typedef struct { const char *title; int x,w,n; AjPenRow rows[7]; } AjPenColumn;
typedef struct { const char *title,*section; int part,total; AjPenColumn cols[2]; } AjPenPage;
#include "ajustes_pen_avancados.inc"
#include "ajustes_pen_principais.inc"
static const AjPenPage AJ_PEN_AUDIO[] = {
  {"Áudio e legendas","AJUSTES",2,3,{
    {"IDIOMAS",200,800,6,{
      {AJ_IDIOMA,"Idioma do app","O idioma da interface da TV","aj_globe",0},
      {AJ_AUD_LINGUA,"Idioma do áudio","A faixa que você prefere","aj_volume-2",0},
      {AJ_LEG_LINGUA,"Idioma da legenda","A legenda que aparece primeiro","aj_languages",0},
      {AJ_LEG_FORCADA,"Legenda forçada automática","Liga quando o áudio é no seu idioma","aj_captions",1},
      {AJ_LEG_SYNC_AUDIO,"Sincronia por áudio","A sincronia segue o áudio do vídeo","aj_audio-lines",1},
      {AJ_LEG_MOSTRAR,"Mostrar no player","Só os seus idiomas ou todas","aj_funnel",0}}},
    {"SEGUNDA LEGENDA",1024,800,6,{
      {AJ_LEG_LINGUA2,"Idioma da legenda secundária","Uma tradução por cima da principal","aj_captions",0},
      {AJ_LEG2_POS,"Posição da segunda legenda","No topo ou junto da principal","aj_align-justify",0},
      {AJ_LEG2_TAMANHO,"Tamanho","O tamanho da segunda legenda","aj_type",0},
      {AJ_LEG2_COR,"Cor","A cor do texto da segunda","aj_palette",0},
      {AJ_LEG2_FUNDO,"Fundo","O fundo da segunda legenda","aj_square",0},
      {AJ_LEG2_BORDA,"Borda","Contorno ou sombra","aj_scan",0}}}}},
  {"Estilo das legendas","ÁUDIO E LEGENDAS",3,3,{
    {"LEGENDA PRINCIPAL",200,800,6,{
      {AJ_LEG1_TAMANHO,"Tamanho","O tamanho da letra na tela","aj_type",0},
      {AJ_LEG1_FONTE,"Fonte","A letra que a legenda usa","aj_type",0},
      {AJ_LEG1_COR,"Cor","A cor do texto","aj_palette",0},
      {AJ_LEG1_OPACIDADE,"Opacidade","Quão forte a letra aparece","aj_contrast",0},
      {AJ_LEG1_FUNDO,"Fundo","O fundo atrás da letra","aj_square",0},
      {AJ_LEG1_POSICAO,"Posição","Em que linha a legenda fica","aj_align-justify",0}}},
    {"BORDA E ATRASO",1024,800,6,{
      {AJ_LEG1_BORDA,"Borda","Contorno ou sombra","aj_scan",0},
      {AJ_LEG1_COR_BORDA,"Cor da borda","A cor do contorno","aj_pipette",0},
      {AJ_LEG1_NEGRITO,"Negrito","Letra mais grossa","aj_type",1},
      {AJ_LEG1_ATRASO,"Atraso","Adianta ou atrasa a legenda","aj_timer",0},
      {AJ_LEG1_RESTAURAR,"Restaurar o padrão","Volta o estilo que veio no arquivo","aj_rotate-ccw-clock",0},
      {AJ_LEG_SYNC_AUTO,"Sincronia automática","Ajusta a legenda ao áudio","aj_audio-lines",1}}}}}
};
static AjPenPage ajPenOther;
static int ajPenMissing(int *ops) {
  unsigned char seen[AJ_N]={0};
  for(int p=0;p<46;p++) {
    const AjPenPage *page=p<27?&AJ_PEN_ADV[p]:p<29?&AJ_PEN_AUDIO[p-27]:&AJ_PEN_MAIN[p-29];
    for(int c=0;c<2;c++)for(int i=0;i<page->cols[c].n;i++)seen[page->cols[c].rows[i].op]=1;
  }
  int n=0;for(int op=0;op<AJ_N;op++)if(!seen[op] && aj4Secao(op)>=0 && ajOpcaoDisponivel(op))ops[n++]=op;
  return n;
}
static const AjPenPage *ajPenCurrent(void) {
  if(ajPenPage>=0 && ajPenPage<27)return &AJ_PEN_ADV[ajPenPage];
  if(ajPenPage>=27 && ajPenPage<29)return &AJ_PEN_AUDIO[ajPenPage-27];
  if(ajPenPage>=29 && ajPenPage<46)return &AJ_PEN_MAIN[ajPenPage-29];
  if(ajPenPage>=46) {
    int ops[AJ_N],n=ajPenMissing(ops),start=(ajPenPage-46)*14;
    if(start>=n)return NULL;
    memset(&ajPenOther,0,sizeof ajPenOther);
    ajPenOther.title="Outros ajustes";ajPenOther.section="AJUSTES AVANÇADOS";
    ajPenOther.part=ajPenPage-45;ajPenOther.total=(n+13)/14;
    for(int c=0;c<2;c++) {
      AjPenColumn *col=&ajPenOther.cols[c];col->title="OUTROS AJUSTES";col->x=c?1024:200;col->w=800;
      for(int i=0;i<7 && start+c*7+i<n;i++) {
        int op=ops[start+c*7+i];col->rows[col->n++]=(AjPenRow){op,rotuloOpcao(op),ajudaOpcao(op),"aj_sliders-horizontal",0};
      }
    }
    return &ajPenOther;
  }
  return NULL;
}
static void ajPenOpen(int page) {
  ajPenPage=page;ajPenRow=0;ajPenReturn=-1;focoIndice=0;aj4Util=0;aj4Topo=-1;
  uxVeioBusca=0;aj4BuscaRetorno=0;aj4PerfilRetorno=0;
}
static int ajPenEvent(SDL_Keycode key) {
  const AjPenPage *page=ajPenCurrent();
  if(!page || uxEditor || aj4Util || focoIndice || aj4Topo>=0)return -1;
  if(uxVoltar(key)) { ajPenPage=-1;focoIndice=1;aj4Linha=0;return 1; }
  int n=page->cols[0].n+page->cols[1].n;
  if(key==SDLK_DOWN && ajPenRow<n-1)ajPenRow++;
  if(key==SDLK_UP) { if(ajPenRow>0)ajPenRow--;else aj4Topo=1; }
  if(key==SDLK_RIGHT) {
    if(ajPenRow<page->cols[0].n && page->cols[1].n)ajPenRow=page->cols[0].n;
    else if(ajPenPage<26)ajPenOpen(ajPenPage+1);
    else if(ajPenPage==26)ajPenOpen(46);
    else if(ajPenPage>=46) { int ops[AJ_N],n=ajPenMissing(ops);if((ajPenPage-45)*14<n)ajPenOpen(ajPenPage+1); }
    else if(ajPenPage==27)ajPenOpen(28);
    else if(ajPenPage>=29 && ajPenPage<45)ajPenOpen(ajPenPage+1);
    return 1;
  }
  if(key==SDLK_LEFT) {
    if(ajPenRow>=page->cols[0].n)ajPenRow=0;
    else if(ajPenPage>0 && ajPenPage<27)ajPenOpen(ajPenPage-1);
    else if(ajPenPage==28)ajPenOpen(27);
    else if(ajPenPage==46)ajPenOpen(26);
    else if(ajPenPage>29)ajPenOpen(ajPenPage-1);
    else { ajPenPage=-1;focoIndice=1; }
    return 1;
  }
  if(uxOk(key)) {
    int col=ajPenRow>=page->cols[0].n;
    const AjPenRow *row=&page->cols[col].rows[ajPenRow-(col?page->cols[0].n:0)];
    int op=row->op;
    if(op<0 || !ajOpcaoDisponivel(op))return 1;
    if(OPCOES[op].tipo==OP_LEITURA)return 1;
    if(inativa(op)) {
      int req=uxRequisito(op);
      if(uxLigarRequisitos(op,1))return 1;
      if(req>=0) { focarOpcao(req);uxAbrirEditor(req);return 1; }
    }
    focarOpcao(op);
    if(focoOp!=op) { focoOp=op;focoIndice=0; }
    if(OPCOES[op].tipo==OP_ACAO) { ajPenReturn=ajPenPage;return 0; }
    uxAbrirEditor(op);return 1;
  }
  return 1;
}
