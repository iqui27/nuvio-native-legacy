/* The approved Pencil pages bind to the original setting IDs. */
typedef struct { int op; const char *title,*help,*icon; int toggle; } AjPenRow;
typedef struct { const char *title; int x,w,n; AjPenRow rows[7]; } AjPenColumn;
typedef struct { const char *title,*section; int part,total; AjPenColumn cols[2]; } AjPenPage;
#include "ajustes_pen_avancados.inc"
#include "ajustes_pen_principais.inc"
#include "ajustes_pen_parental.inc"
#define AJ_PEN_PARENTAL 100
#define AJ_PEN_RULES 101
#define AJ_PEN_TOUR 102
#define AJ_PEN_SECONDARY 103
#define AJ_PEN_COMPONENTS 104
static const AjPenPage AJ_PEN_SECOND_PAGE={"Segunda legenda","Áudio e legendas",3,3,{
  {"",200,520,6,{{AJ_LEG2_FONTE,"Igual à principal","","aj_type",0},{AJ_LEG2_TAMANHO,"Tamanho","","aj_type",0},{AJ_LEG2_COR,"Cor","","aj_palette",0},{AJ_LEG2_FUNDO,"Fundo","","aj_square",0},{AJ_LEG2_BORDA,"Borda","","aj_scan",0},{AJ_LEG_LINGUA2,"Idioma da segunda legenda","","aj_languages",0}}},
  {"",744,1080,0,{{0}}}}};
static const AjPenPage AJ_PEN_FEATURES = {"O que o Nuvio faz?","Ajustes",3,3,{
  {"",200,800,6,{
    {AJ_CW_LIGADO,"Continuar assistindo","Retoma de onde parou, em qualquer perfil","aj_tv",1},
    {AJ_DEBRID_RD,"Debrid","Link premium de verdade, sem publicidade","aj_key",0},
    {AJ_ADDONS,"Addons","As fontes que você já usa","aj_puzzle",0},
    {AJ_TRAKT,"Trakt e Simkl","Seu progresso e sua lista na conta","aj_clapperboard",0},
    {AJ_LEG1_TAMANHO,"Legendas com estilo","Tamanho, cor, borda e fonte","aj_captions",0},
    {AJ_LEG_LINGUA2,"Segunda legenda","Uma tradução por cima da principal","aj_type",0}}},
  {"",1024,800,6,{
    {AJ_PERFIL_INFANTIL,"Perfil infantil","PIN e limites por perfil","aj_baby",0},
    {AJ_JF_LIGADO,"Servidores pessoais","Jellyfin, Emby e Plex dentro do app","aj_server",0},
    {AJ_MIC_BUSCA,"Busca por voz","Fale o nome do que procura","aj_mic",0},
    {-5,"Assistir Juntos","Assista junto com quem você conhece","aj_users",0},
    {AJ_VELOCIDADE,"Teste de velocidade","Mede a banda da sua casa","aj_gauge",0},
    {AJ_ENVIAR_LOG,"Enviar registro","Ajuda a equipe do app a achar o erro","aj_file-text",0}}}}};
static const AjPenPage AJ_PEN_PARENT = {"Controle parental","Controle parental",3,3,{
  {"PERFIL INFANTIL",200,800,6,{
    {AJ_PERFIL_INFANTIL,"Perfil infantil","Marca o perfil em uso como infantil","aj_baby",0},
    {-2,"Ajustes travados","Só o PIN abre os ajustes","aj_lock",1},
    {-3,"PIN do perfil","Um código de 4 dígitos","aj_pin",0},
    {AJ_ENQUETES,"Sem enquetes no perfil infantil","","aj_eye-off",1},
    {AJ_PLR_CLASSIF,"Classificação no player","A etiqueta de idade no vídeo","aj_shield",0},
    {-4,"Filmes permitidos","O que o perfil pode abrir","aj_tv",0}}},
  {"LIMITES",1024,800,6,{
    {-10,"Tempo de tela por perfil","Quanto tempo o perfil pode usar","aj_alarm-clock",0},
    {-11,"Classificação máxima","A idade mínima do perfil","aj_film",0},
    {-12,"Busca sem filtro","","aj_search",1},
    {-13,"Compras bloqueadas","Não deixa comprar na loja","aj_shopping-bag",1},
    {AJ_PROXIMO_AUTO,"Reprodução automática","","aj_play",1},
    {AJ_HERO_TRAILER,"Mostrar trailer","","aj_eye",1}}}}};
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
      {AJ_LEG1_ATRASO,"Ajuste fino","Corrige o milissegundo","aj_sliders-horizontal",0}}}}}
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
  if(ajPenPage==AJ_PEN_SECONDARY)return &AJ_PEN_SECOND_PAGE;
  if(ajPenPage==AJ_PEN_PARENTAL)return &AJ_PEN_PARENT;
  if(ajPenPage==AJ_PEN_RULES)return NULL;
  if(ajPenPage==AJ_PEN_TOUR)return &AJ_PEN_FEATURES;
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
static void ajPenFind(int op) {
  for(int p=0;p<46;p++) {
    const AjPenPage *page=p<27?&AJ_PEN_ADV[p]:p<29?&AJ_PEN_AUDIO[p-27]:&AJ_PEN_MAIN[p-29];
    int row=0;
    for(int c=0;c<2;c++)for(int i=0;i<page->cols[c].n;i++,row++)if(page->cols[c].rows[i].op==op) { ajPenOpen(p);ajPenRow=row;return; }
  }
  int ops[AJ_N],n=ajPenMissing(ops);
  for(int i=0;i<n;i++)if(ops[i]==op) { ajPenOpen(46+i/14);ajPenRow=i%14;return; }
}
static int ajPenEvent(SDL_Keycode key) {
  if(ajPenParentEvent(key))return 1;
  if(ajPenPage==AJ_PEN_COMPONENTS) {
    if(uxVoltar(key))ajPenOpen(44);
    else if(key==SDLK_DOWN || key==SDLK_RIGHT)ajPenRow=(ajPenRow+1)%4;
    else if(key==SDLK_UP || key==SDLK_LEFT)ajPenRow=(ajPenRow+3)%4;
    else if(uxOk(key)) { if(ajPenRow==0)aj4Buscar();else if(ajPenRow==1) { aj4Util=2;aj4Linha=0; }else if(ajPenRow==2)ajPenSavedAsk();else ajPenOpen(29); }
    return 1;
  }
  if(ajPenPage==44 && key==SDLK_c) { ajPenOpen(AJ_PEN_COMPONENTS);return 1; }
  const AjPenPage *page=ajPenCurrent();
  if(ajPenPage==AJ_PEN_RULES && !uxEditor && !aj4Util && !focoIndice) {
    if(uxVoltar(key) || uxOk(key)) { ajPenPage=-1;focoIndice=1;aj4Linha=6; }
    return 1;
  }
  if(!page || uxEditor || aj4Util || focoIndice || aj4Topo>=0)return -1;
  if(uxVoltar(key)) {
    int parent=ajPenPage==AJ_PEN_SECONDARY || ajPenPage==28?27:ajPenPage>=30 && ajPenPage<=32?29:ajPenPage==34?33:ajPenPage>=38 && ajPenPage<=40?37:ajPenPage==42?41:-1;
    if(parent>=0)ajPenOpen(parent);else { ajPenPage=-1;focoIndice=1;aj4Linha=0; }
    return 1;
  }
  int n=page->cols[0].n+page->cols[1].n;
  if(ajPenPage==AJ_PEN_TOUR) {
    if(key==SDLK_DOWN && ajPenRow+4<n)ajPenRow+=4;
    if(key==SDLK_UP && ajPenRow>=4)ajPenRow-=4;
    if(key==SDLK_RIGHT && ajPenRow<n-1)ajPenRow++;
    if(key==SDLK_LEFT && ajPenRow>0)ajPenRow--;
    if(!uxOk(key))return 1;
  }
  if(key==SDLK_DOWN && ajPenRow<n-1)ajPenRow++;
  if(key==SDLK_UP) { if(ajPenRow>0)ajPenRow--;else aj4Topo=1; }
  if(key==SDLK_RIGHT) {
    if(ajPenRow<page->cols[0].n && page->cols[1].n)ajPenRow=page->cols[0].n;
    else if(ajPenPage<26)ajPenOpen(ajPenPage+1);
    else if(ajPenPage==26)ajPenOpen(46);
    else if(ajPenPage>=46 && ajPenPage<AJ_PEN_PARENTAL) { int ops[AJ_N],n=ajPenMissing(ops);if((ajPenPage-45)*14<n)ajPenOpen(ajPenPage+1); }
    else if(ajPenPage==27)ajPenOpen(28);
    else if(ajPenPage>=29 && ajPenPage<45)ajPenOpen(ajPenPage+1);
    return 1;
  }
  if(key==SDLK_LEFT) {
    if(ajPenRow>=page->cols[0].n)ajPenRow=0;
    else if(ajPenPage>0 && ajPenPage<27)ajPenOpen(ajPenPage-1);
    else if(ajPenPage==28)ajPenOpen(27);
    else if(ajPenPage==46)ajPenOpen(26);
    else if(ajPenPage>29 && ajPenPage<AJ_PEN_PARENTAL)ajPenOpen(ajPenPage-1);
    else { ajPenPage=-1;focoIndice=1; }
    return 1;
  }
  if(uxOk(key)) {
    int col=ajPenRow>=page->cols[0].n;
    const AjPenRow *row=&page->cols[col].rows[ajPenRow-(col?page->cols[0].n:0)];
    int op=row->op;
    if(op<0) {
      if(op<=-10 && op>=-13)ajPenParentOpen(-10-op);
      else if(op==-4)ajPenParentOpen(PR_IDADE);
      else if(op==-2)pfExigirAdmin(PA_INFANTIL,perfis_ativo());
      else if(op==-3) { if(!admin_perfil_pin_definido(perfis_ativo())) { ajPenPinAsk=1;ajPenPinRepeat=0;ajPenPinOpen(); } else uxNotificar("Use Trocar de perfil para trocar o PIN."); }
      else if(op==-5) { ajPenPage=-1;tourAbrir(); }
      else uxNotificar(op==-4?"Perfil infantil: só classificação livre e até 10 anos.":"Ainda não disponível.");
      return 1;
    }
    if(ajPenPage==27 && op==AJ_LEG_LINGUA2) { ajPenOpen(AJ_PEN_SECONDARY);return 1; }
    if(ajPenPage==AJ_PEN_TOUR) {
      if(ajPenRow==0) { definirValorDireto(AJ_CW_LIGADO,lig(AJ_CW_LIGADO)?1:0);return 1; }
      if(ajPenRow==1) { ajPenOpen(39);return 1; }
      if(ajPenRow==2) { ajPenOpen(38);return 1; }
      if(ajPenRow==4) { ajPenOpen(28);return 1; }
      if(ajPenRow==6) { ajPenOpen(AJ_PEN_PARENTAL);return 1; }
      if(ajPenRow==7) { ajPenOpen(40);return 1; }
    }
    if(!ajOpcaoDisponivel(op))return 1;
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
