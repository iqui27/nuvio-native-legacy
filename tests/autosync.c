#include "autosync.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

char *rede_baixar_bin(const char *url,int segundos,long *n) {
  (void)url;(void)segundos;(void)n;return NULL;
}
const char *i18n(const char *s) { return s; }
static LegendaCue corpus[120];
static int casos;
static void criarCorpus(void) {
  unsigned seed=982451653;double pos=45;
  for(int i=0;i<120;i++) {
    seed=seed*1664525u+1013904223u;pos+=3+(seed%4000)/1000.0;
    seed=seed*1664525u+1013904223u;
    corpus[i]=(LegendaCue){.inicio=pos,.fim=pos+1+(seed%1500)/1000.0,
      .cor=-1,.posX=-1,.posY=-1,.ordem=i};
    snprintf(corpus[i].texto,sizeof corpus[i].texto,"Dialogue number %d about something different",i);
  }
}
static LegendaDocumento *documento(const char *id,const char *idioma,double atraso,
                                  double escala,unsigned flags,uint64_t sessao,int variante) {
  LegendaCue v[120];memcpy(v,corpus,sizeof v);
  for(int i=0;i<120;i++) {
    double delta=atraso+(variante==1&&i>=60?5:0);
    v[i].inicio=v[i].inicio*escala+delta;v[i].fim=v[i].fim*escala+delta;
    if(variante==2){v[i].inicio=45+i*7+atraso;v[i].fim=v[i].inicio+2;}
    if(variante==3)snprintf(v[i].texto,sizeof v[i].texto,"Same repeated dialogue");
    if(variante==4)snprintf(v[i].texto,sizeof v[i].texto,"[music]");
    if(variante==5){v[i].an=8;v[i].posY=100;v[i].resY=1080;}
    if(variante==6&&i==119){v[i].inicio+=2;v[i].fim+=2;}
    if(variante==7&&i==60){v[i].inicio+=1;v[i].fim+=1;}
    if(variante==8){v[i].inicio+=i*.007;v[i].fim+=i*.007;}
    if(variante==9){double ruido=((i*7)%21-10)*.01;v[i].inicio+=ruido;v[i].fim+=ruido;}
    if(variante==10){double ruido=((i*11)%41-20)*.02;v[i].inicio+=ruido;v[i].fim+=ruido;}
    if(!strcmp(idioma,"pt"))snprintf(v[i].texto,sizeof v[i].texto,"Uma tradução diferente para o diálogo número %d",i);
  }
  LegendaDocumentoInfo info={.sessao=sessao,.flags=flags};
  snprintf(info.idioma,sizeof info.idioma,"%s",idioma);
  snprintf(info.origem,sizeof info.origem,"%s",!strcmp(id,"embedded")?"Embedded":"Addon");
  snprintf(info.identidade,sizeof info.identidade,"%s",id);
  return legenda_documento_de_cues(v,120,&info);
}
static AutoSyncResultado comparar(LegendaDocumento *a,LegendaDocumento *b,AutoSyncModo modo) {
  AutoSyncConfig cfg=autosync_config(modo);
  return autosync_comparar(a,b,&cfg,NULL,NULL);
}
static void aceite(LegendaDocumento *a,LegendaDocumento *b,int esperado,AutoSyncModo modo) {
  AutoSyncResultado r=comparar(a,b,modo);
  if(r.estado!=AUTOSYNC_ACCEPTED)fprintf(stderr,"reject %s confidence %.3f alternative %.3f regions %d\n",
    autosync_motivo(r.motivo),r.confianca,r.alternativa,r.regioes);
  assert(r.estado==AUTOSYNC_ACCEPTED);assert(abs(r.offsetMs-esperado)<=25);
  assert(r.erroMs<=250);assert(r.regioes==(modo==AUTOSYNC_QUICK?3:6));casos++;
}
static void recusa(LegendaDocumento *a,LegendaDocumento *b,AutoSyncMotivo motivo) {
  for(int modo=0;modo<2;modo++) {
    AutoSyncResultado r=comparar(a,b,(AutoSyncModo)modo);
    assert(r.estado!=AUTOSYNC_ACCEPTED);
    if(motivo!=AUTOSYNC_OK&&r.motivo!=motivo)fprintf(stderr,"expected %s got %s\n",autosync_motivo(motivo),autosync_motivo(r.motivo));
    assert(motivo==AUTOSYNC_OK||r.motivo==motivo);assert(r.offsetMs==0);casos++;
  }
}
static int cancelar(void *u) {int *n=u;return ++*n>4;}
static int consumirOrcamento(void *u) {
  (void)u;struct timespec t={0,3000000};nanosleep(&t,NULL);return 0;
}
static void stress(void) {
  LegendaCue original[120];memcpy(original,corpus,sizeof corpus);
  unsigned seed=75913043;int maiorTempo=0;
  for(int rodada=0;rodada<100;rodada++) {
    double pos=45;
    for(int i=0;i<120;i++) {
      seed=seed*1664525u+1013904223u;pos+=2.5+(seed%6000)/1000.0;
      seed=seed*1664525u+1013904223u;
      corpus[i].inicio=pos;corpus[i].fim=pos+.7+(seed%2300)/1000.0;
    }
    seed=seed*1664525u+1013904223u;
    int offset=((int)(seed%2401)-1200)*25;
    LegendaDocumento *a=documento("stress-addon","pt",offset/1000.0,1,LEGENDA_DOC_COMPLETO,77,0);
    LegendaDocumento *b=documento("stress-reference","en",0,1,LEGENDA_DOC_COMPLETO,77,0);
    for(int modo=0;modo<2;modo++) {
      AutoSyncResultado r=comparar(a,b,(AutoSyncModo)modo);
      assert(r.estado==AUTOSYNC_ACCEPTED&&abs(r.offsetMs-offset)<=25);
      if(r.tempoMs>maiorTempo)maiorTempo=r.tempoMs;casos++;
    }
    /* One line 1 s off amidst an otherwise exact timeline: the shared offset
     * may apply (that line stays as the file had it), nothing else may. */
    LegendaDocumento *cut=documento("stress-cut","pt",offset/1000.0,1,LEGENDA_DOC_COMPLETO,77,7);
    for(int modo=0;modo<2;modo++) {
      AutoSyncResultado r=comparar(cut,b,(AutoSyncModo)modo);
      assert(r.estado!=AUTOSYNC_ACCEPTED||(r.tipo==AUTOSYNC_T_OFFSET&&abs(r.offsetMs-offset)<=25));casos++;
    }
    legenda_documento_liberar(a);legenda_documento_liberar(b);legenda_documento_liberar(cut);
  }
  memcpy(corpus,original,sizeof corpus);
  int n=7999;LegendaCue *v=calloc((size_t)n,sizeof *v);assert(v);double pos=45;
  for(int i=0;i<n;i++) {
    seed=seed*1664525u+1013904223u;pos+=.85+(seed%500)/1000.0;
    seed=seed*1664525u+1013904223u;
    v[i]=(LegendaCue){.inicio=pos,.fim=pos+.25+(seed%150)/1000.0,
      .cor=-1,.posX=-1,.posY=-1,.ordem=i};
    snprintf(v[i].texto,sizeof v[i].texto,"Long-film sentence %d",i);
  }
  LegendaDocumentoInfo info={.sessao=99,.flags=LEGENDA_DOC_COMPLETO,.idioma="en",.identidade="long-reference"};
  LegendaDocumento *a=legenda_documento_de_cues(v,n,&info);assert(a);
  for(int i=0;i<n;i++){v[i].inicio+=30;v[i].fim+=30;}
  snprintf(info.identidade,sizeof info.identidade,"long-addon");
  LegendaDocumento *b=legenda_documento_de_cues(v,n,&info);assert(b);
  for(int modo=0;modo<2;modo++) {
    AutoSyncResultado r=comparar(b,a,(AutoSyncModo)modo);
    assert(r.estado==AUTOSYNC_ACCEPTED&&r.offsetMs==30000&&r.erroMs==0);casos++;
    if(r.tempoMs>maiorTempo)maiorTempo=r.tempoMs;
  }
  AutoSyncConfig cfg=autosync_config(AUTOSYNC_QUICK);cfg.orcamentoMs=1;
  AutoSyncResultado r=autosync_comparar(b,a,&cfg,consumirOrcamento,NULL);
  assert(r.estado==AUTOSYNC_REJECTED&&r.motivo==AUTOSYNC_BUDGET&&r.offsetMs==0);casos++;
  AutoSync *s=autosync_criar();assert(s);
  for(int i=0;i<100;i++) {
    autosync_iniciar(s,99);assert(autosync_selecionar(s,0,b));
    assert(autosync_solicitar(s,0,a,NULL));
    autosync_iniciar(s,100);usleep(500);
    assert(autosync_estado(s,0).estado==AUTOSYNC_UNAVAILABLE&&autosync_offset_ms(s,0)==0);
    casos++;
  }
  autosync_destruir(s);free(v);legenda_documento_liberar(a);legenda_documento_liberar(b);
  printf("autosync synthetic host stress: max analysis %d ms; 7999 cues/document\n",maiorTempo);
}
static AutoSyncResultado esperar(AutoSync *s,int slot) {
  for(int i=0;i<2000;i++) {
    AutoSyncResultado r=autosync_estado(s,slot);
    if(r.estado!=AUTOSYNC_ANALYSING)return r;
    usleep(1000);
  }
  assert(!"analysis did not finish");return (AutoSyncResultado){0};
}
static void parsers(void) {
  char srt[32000]={0},vtt[32000]="WEBVTT\n\n",ass[32000]="[Script Info]\nPlayResX: 1920\nPlayResY: 1080\n[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
  size_t ns=0,nv=strlen(vtt),na=strlen(ass);
  for(int i=0;i<120;i++) {
    int a=(int)lround(corpus[i].inicio*1000),b=(int)lround(corpus[i].fim*1000);
    ns+=(size_t)snprintf(srt+ns,sizeof srt-ns,"%d\n00:%02d:%02d,%03d --> 00:%02d:%02d,%03d\nDialogue %d\n\n",i+1,a/60000,(a/1000)%60,a%1000,b/60000,(b/1000)%60,b%1000,i);
    nv+=(size_t)snprintf(vtt+nv,sizeof vtt-nv,"00:%02d:%02d.%03d --> 00:%02d:%02d.%03d align:middle\nDialogue %d\n\n",a/60000,(a/1000)%60,a%1000,b/60000,(b/1000)%60,b%1000,i);
    na+=(size_t)snprintf(ass+na,sizeof ass-na,"Dialogue: 0,0:%02d:%02d.%02d,0:%02d:%02d.%02d,Default,,0,0,0,,Dialogue %d\n",a/60000,(a/1000)%60,(a%1000)/10,b/60000,(b/1000)%60,(b%1000)/10,i);
  }
  LegendaDocumentoInfo info={.sessao=88,.flags=LEGENDA_DOC_COMPLETO,.idioma="en",.origem="addon",.identidade="srt"};
  LegendaDocumento *a=legenda_documento_criar(srt,&info);
  snprintf(info.identidade,sizeof info.identidade,"vtt");
  LegendaDocumento *b=legenda_documento_criar(vtt,&info);
  snprintf(info.identidade,sizeof info.identidade,"ass");
  LegendaDocumento *c=legenda_documento_criar(ass,&info);
  assert(a&&b&&c);aceite(a,b,0,AUTOSYNC_QUICK);aceite(a,c,0,AUTOSYNC_THOROUGH);
  legenda_definir_corpo(srt);unsigned g=legenda_geracao();
  LegendaDocumento *snapshot=legenda_documento_ativo(g,&info);assert(snapshot);
  LegendaCue cue;
  assert(legenda_documento_cues(snapshot,corpus[10].inicio+.2,0,&cue,1)==1);
  legenda_desligar();assert(!legenda_documento_ativo(g,&info));
  /* Immutable snapshot still works after overlay ownership changes. */
  assert(legenda_documento_cues(snapshot,corpus[10].inicio+.2,0,&cue,1)==1);
  LegendaDocumento *keep=legenda_documento_reter(snapshot);legenda_documento_liberar(snapshot);
  assert(legenda_documento_cues(keep,corpus[10].inicio+.2,0,&cue,1)==1);
  legenda_documento_liberar(keep);legenda_documento_liberar(a);legenda_documento_liberar(b);legenda_documento_liberar(c);casos++;
  static const char bytes[]="1\n00:00:01,000 --> 00:00:03,000\nOl\xe1\n\n";
  a=legenda_documento_bytes(bytes,(long)sizeof bytes-1,&info);assert(a);
  int total=0;const LegendaCue *dados=legenda_documento_dados(a,&total);
  assert(total==1&&!strcmp(dados[0].texto,"Olá"));legenda_documento_liberar(a);casos++;
  a=legenda_documento_criar("1\n00:00:01,000 --> 00:00:03,000\nGood\n\n2\n00:00:nan --> 00:00:04,000\nBad\n\n",&info);
  {int nb=0;legenda_documento_dados(a,&nb);assert(nb==1&&(legenda_documento_info(a)->flags&LEGENDA_DOC_COMPLETO));}  /* bloco ruim cai sozinho */
  legenda_documento_liberar(a);casos++;
  a=legenda_documento_criar("Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,Good\nDialogue: 0,0:00:nan,0:00:04.00,Default,,0,0,0,,Bad\n",&info);
  {int nb=0;legenda_documento_dados(a,&nb);assert(nb==1&&(legenda_documento_info(a)->flags&LEGENDA_DOC_COMPLETO));}  /* bloco ruim cai sozinho */
  legenda_documento_liberar(a);casos++;
}
// Prova-janela: o span parcial nao pode ampliar o raio local para CD1/CD2.
static void janela(void) {
  LegendaCue ref[42]={0},ext[40]={0};
  unsigned seed=42;double t=1000;
  for(int i=0;i<40;i++) {
    seed=seed*1664525u+1013904223u;t+=1.5+(seed%500)/1000.0;
    seed=seed*1664525u+1013904223u;
    ref[i]=(LegendaCue){.inicio=t,.fim=t+1+(seed%1500)/1000.0,.posX=-1,.posY=-1};
    snprintf(ref[i].texto,sizeof ref[i].texto,"Fala distinta numero %d",i);
    ext[i]=ref[i];ext[i].inicio+=30;ext[i].fim+=30;
  }
  ref[40]=(LegendaCue){.inicio=1000,.fim=1001,.texto="Inicio distinto",.posY=-1};
  ref[41]=(LegendaCue){.inicio=1118,.fim=1119,.texto="Fim distinto",.posY=-1};
  LegendaDocumentoInfo ri={.sessao=1,.flags=LEGENDA_DOC_COMPLETO|LEGENDA_DOC_JANELA,
    .janelaInicio=1000,.janelaFim=1120,.identidade="mkv-track:5"};
  LegendaDocumentoInfo ei={.sessao=1,.flags=LEGENDA_DOC_COMPLETO,.identidade="externa"};
  LegendaDocumento *a=legenda_documento_de_cues(ext,40,&ei),*b=legenda_documento_de_cues(ref,42,&ri);
  AutoSync *s=autosync_criar();assert(a&&b&&s);autosync_iniciar(s,1);
  for(int modo=0;modo<2;modo++) {
    AutoSyncConfig cfg=autosync_config((AutoSyncModo)modo);
    assert(autosync_selecionar(s,0,a)&&autosync_solicitar(s,0,b,&cfg));
    AutoSyncResultado r=esperar(s,0);
    printf("janela +30 s: modo=%d estado=%d motivo=%s offset=%d aplicado=%d\n",
      modo,r.estado,autosync_motivo(r.motivo),r.offsetMs,autosync_offset_ms(s,0));fflush(stdout);
    assert(r.estado==AUTOSYNC_REJECTED&&r.offsetMs==0&&autosync_offset_ms(s,0)==0);casos++;
  }
  legenda_documento_liberar(a);
  // O refino pode sair da busca grossa: 10 s passa, 10,1 s deve recusar.
  for(int offset=10000;offset<=10100;offset+=100) {
    LegendaCue limite[40];memcpy(limite,ref,sizeof limite);
    for(int i=0;i<40;i++){limite[i].inicio+=offset/1000.0;limite[i].fim+=offset/1000.0;}
    a=legenda_documento_de_cues(limite,40,&ei);assert(a);
    assert(autosync_selecionar(s,0,a)&&autosync_solicitar(s,0,b,NULL));
    AutoSyncResultado r=esperar(s,0);
    if(offset==10000)assert(r.estado==AUTOSYNC_ACCEPTED&&r.offsetMs==10000&&autosync_offset_ms(s,0)==10000);
    else assert(r.estado==AUTOSYNC_REJECTED&&r.motivo==AUTOSYNC_REGION_DISAGREEMENT&&r.offsetMs==0&&autosync_offset_ms(s,0)==0);
    legenda_documento_liberar(a);casos++;
  }
  autosync_destruir(s);
  // Poucas falas, span curto e nenhuma intersecao: falta evidencia local.
  for(int variante=0;variante<3;variante++) {
    LegendaCue rec[41];memcpy(rec,ext,sizeof ext);
    rec[40]=(LegendaCue){.inicio=3500,.fim=3501,.texto="Fala distante da janela",.posY=-1};
    for(int i=0;i<40;i++) {
      if(variante==1){rec[i].inicio=1030+i;rec[i].fim=1030.5+i;}
      if(variante==2||(variante==0&&i>=18)){rec[i].inicio+=2000;rec[i].fim+=2000;}
    }
    a=legenda_documento_de_cues(rec,41,&ei);assert(a);
    AutoSyncResultado r=autosync_comparar(a,b,NULL,NULL,NULL);
    assert(r.estado==AUTOSYNC_REJECTED&&r.motivo==AUTOSYNC_SPARSE&&r.referenciaInvalida&&r.offsetMs==0);
    legenda_documento_liberar(a);casos++;
  }
  // Falha global da externa nao vira falha da referencia.
  a=legenda_documento_de_cues(ext,18,&ei);assert(a);
  AutoSyncResultado r=autosync_comparar(a,b,NULL,NULL,NULL);
  assert(r.estado==AUTOSYNC_REJECTED&&r.motivo==AUTOSYNC_SPARSE&&!r.referenciaInvalida);
  legenda_documento_liberar(a);legenda_documento_liberar(b);casos++;
}
int main(void) {
  criarCorpus();
  janela();
  LegendaDocumento *ref=documento("embedded","en",0,1,LEGENDA_DOC_COMPLETO,11,0);assert(ref);
  {
    LegendaCue ruim[120]; memcpy(ruim, corpus, sizeof ruim);
    LegendaDocumentoInfo info={.sessao=11,.flags=LEGENDA_DOC_COMPLETO};
    for (int i = 0; i < 120; i++) {
      if (i % 17 == 0) { ruim[i].inicio += 1.2; ruim[i].fim += 1.2; }
      snprintf(ruim[i].texto, sizeof ruim[i].texto, "Traducao diferente numero %d", i);
    }
    snprintf(info.idioma, sizeof info.idioma, "ro");
    snprintf(info.origem, sizeof info.origem, "Addon");
    snprintf(info.identidade, sizeof info.identidade, "external-ro");
    LegendaDocumento *doc=legenda_documento_de_cues(ruim,120,&info);
    AutoSyncResultado r=autosync_comparar(doc,ref,NULL,NULL,NULL);
    assert(r.estado==AUTOSYNC_ACCEPTED&&r.tipo==AUTOSYNC_T_OFFSET&&abs(r.offsetMs)<=25);
    legenda_documento_liberar(doc); casos++;
  }
  const int offsets[]={250,-250,1000,-1000,5000,-5000,30000,-30000};
  for(int m=0;m<2;m++)for(int i=0;i<8;i++) {
    LegendaDocumento *doc=documento("external","pt",offsets[i]/1000.0,1,LEGENDA_DOC_COMPLETO,11,0);
    assert(doc);aceite(doc,ref,offsets[i],(AutoSyncModo)m);legenda_documento_liberar(doc);
  }
  const struct {int variante;double escala;unsigned flags;AutoSyncMotivo motivo;} ruins[]={
    {0,1,0,AUTOSYNC_INCOMPLETE},
    {0,1,LEGENDA_DOC_COMPLETO|LEGENDA_DOC_FORCED,AUTOSYNC_FORCED_SIGNS},
    {3,1,LEGENDA_DOC_COMPLETO,AUTOSYNC_REPEATED},
    {4,1,LEGENDA_DOC_COMPLETO,AUTOSYNC_FORCED_SIGNS},
    {5,1,LEGENDA_DOC_COMPLETO,AUTOSYNC_FORCED_SIGNS},
  };
  for(size_t i=0;i<sizeof ruins/sizeof *ruins;i++) {
    LegendaDocumento *doc=documento("bad","en",0,ruins[i].escala,ruins[i].flags,11,ruins[i].variante);
    recusa(doc,ref,ruins[i].motivo);legenda_documento_liberar(doc);
  }
  /* Since 2.1 these are corrected instead of refused: a cut (two pieces), a
   * framerate (scale) and a linear drift. Every line must land on its
   * reference time through the map. One shifted line among 119 correct ones
   * leaves the timing alone (it is not dragged, the rest is already right). */
  {
    const struct {int variante;double escala;AutoSyncTipo tipo;} corrige[]={
      {1,1,AUTOSYNC_T_TRECHOS},{0,25.0/23.976,AUTOSYNC_T_ESCALA},{0,23.976/24,AUTOSYNC_T_ESCALA},
      {8,1,AUTOSYNC_T_ESCALA}};
    for(size_t k=0;k<sizeof corrige/sizeof *corrige;k++)for(int modo=0;modo<2;modo++) {
      LegendaDocumento *doc=documento("fix","pt",0,corrige[k].escala,LEGENDA_DOC_COMPLETO,11,corrige[k].variante);
      AutoSyncConfig c=autosync_config((AutoSyncModo)modo);AutoSyncMapa *m=NULL;
      AutoSyncResultado r=autosync_alinhar(doc,ref,&c,NULL,NULL,&m);
      if(r.estado!=AUTOSYNC_ACCEPTED)fprintf(stderr,"fix %zu rejected %s\n",k,autosync_motivo(r.motivo));
      assert(r.estado==AUTOSYNC_ACCEPTED&&r.tipo==corrige[k].tipo&&m);
      int n=0;const LegendaCue *v=legenda_documento_dados(doc,&n);
      int ok=0;
      for(int i=0;i<n;i++)if(fabs(autosync_mapa_tempo(m,corpus[i].inicio)-v[i].inicio)<=.05)ok++;
      /* a cut hides at most the lines right at the seam */
      assert(ok>=n-(corrige[k].tipo==AUTOSYNC_T_TRECHOS?3:0));
      autosync_mapa_liberar(m);legenda_documento_liberar(doc);casos++;
    }
    for(int variante=6;variante<=7;variante++) {
      LegendaDocumento *doc=documento("one-line","pt",0,1,LEGENDA_DOC_COMPLETO,11,variante);
      AutoSyncResultado r=comparar(doc,ref,AUTOSYNC_QUICK);
      assert(r.estado!=AUTOSYNC_ACCEPTED||(r.tipo==AUTOSYNC_T_OFFSET&&r.offsetMs==0));
      legenda_documento_liberar(doc);casos++;
    }
    /* Already in sync within manterMs: accepted as kept, nothing moves. */
    LegendaDocumento *doc=documento("kept","pt",.08,1,LEGENDA_DOC_COMPLETO,11,0);
    AutoSyncResultado r=comparar(doc,ref,AUTOSYNC_QUICK);
    assert(r.estado==AUTOSYNC_ACCEPTED&&r.mantida&&r.offsetMs==0);
    legenda_documento_liberar(doc);casos++;
  }
  LegendaDocumento *periodico=documento("periodic","en",0,1,LEGENDA_DOC_COMPLETO,11,2);
  LegendaDocumento *periodico2=documento("periodic2","pt",1,1,LEGENDA_DOC_COMPLETO,11,2);
  recusa(periodico2,periodico,AUTOSYNC_AMBIGUOUS);
  LegendaDocumentoInfo sparseInfo={.sessao=11,.flags=LEGENDA_DOC_COMPLETO};
  LegendaDocumento *sparse=legenda_documento_de_cues(corpus,12,&sparseInfo);recusa(sparse,ref,AUTOSYNC_SPARSE);
  LegendaDocumento *stale=documento("stale","en",0,1,LEGENDA_DOC_COMPLETO,10,0);
  recusa(stale,ref,AUTOSYNC_SESSION_CHANGED);
  AutoSyncConfig cfg=autosync_config(AUTOSYNC_QUICK);int polls=0;
  AutoSyncResultado r=autosync_comparar(ref,periodico,&cfg,cancelar,&polls);
  assert(r.estado==AUTOSYNC_CANCELLED&&r.motivo==AUTOSYNC_SESSION_CHANGED);casos++;
  LegendaDocumento *far=documento("far","en",30,1,LEGENDA_DOC_COMPLETO,11,0);
  cfg.raioBuscaMs=5000;r=autosync_comparar(far,ref,&cfg,NULL,NULL);assert(r.estado!=AUTOSYNC_ACCEPTED);casos++;
  /* Lower tolerance never reduces confidence threshold or extends radius. */
  cfg=autosync_config(AUTOSYNC_QUICK);cfg.toleranciaMs=50;r=autosync_comparar(far,ref,&cfg,NULL,NULL);
  assert(r.estado==AUTOSYNC_ACCEPTED&&r.offsetMs==30000);casos++;
  /* Realistic timing noise remains inside tolerance; a stricter user residual
   * tolerance rejects the SAME input, without widening the search radius. */
  LegendaDocumento *noise=documento("noise","pt",1,1,LEGENDA_DOC_COMPLETO,11,9);
  cfg=autosync_config(AUTOSYNC_QUICK);r=autosync_comparar(noise,ref,&cfg,NULL,NULL);
  assert(r.estado==AUTOSYNC_ACCEPTED&&abs(r.offsetMs-1000)<=100);
  /* erroMs = worst segment median |residual| (the +-100 ms noise). */
  assert(r.erroMs>=30&&r.erroMs<=100);casos++;
  /* The gate of a third is AS_SEG_ERRO (3) x the tolerance: +-100 ms of
   * author noise stays inside 50 ms x 3, same answer and same radius. */
  int erro=r.erroMs;
  cfg.toleranciaMs=50;r=autosync_comparar(noise,ref,&cfg,NULL,NULL);
  assert(erro<=150&&r.estado==AUTOSYNC_ACCEPTED&&abs(r.offsetMs-1000)<=100);casos++;
  /* The knob still refuses: +-400 ms is inside 250 ms x 3, outside 50 ms x 3. */
  LegendaDocumento *ruido=documento("ruido","pt",1,1,LEGENDA_DOC_COMPLETO,11,10);
  cfg=autosync_config(AUTOSYNC_QUICK);r=autosync_comparar(ruido,ref,&cfg,NULL,NULL);
  assert(r.estado==AUTOSYNC_ACCEPTED&&abs(r.offsetMs-1000)<=400&&r.erroMs<=750);casos++;
  cfg.toleranciaMs=50;r=autosync_comparar(ruido,ref,&cfg,NULL,NULL);
  assert(r.estado!=AUTOSYNC_ACCEPTED&&r.motivo==AUTOSYNC_REGION_DISAGREEMENT);casos++;
  AutoSync *s=autosync_criar();assert(s);autosync_iniciar(s,11);
  LegendaDocumento *p=documento("primary","pt",5,1,LEGENDA_DOC_COMPLETO,11,0);
  LegendaDocumento *q=documento("secondary","en",-1,1,LEGENDA_DOC_COMPLETO,11,0);
  assert(autosync_selecionar(s,0,p)&&autosync_selecionar(s,1,q));
  assert(autosync_manual(s,0,350)&&autosync_manual(s,1,-200));
  assert(autosync_solicitar(s,0,ref,NULL)&&autosync_solicitar(s,1,ref,NULL));
  assert(esperar(s,0).estado==AUTOSYNC_ACCEPTED&&esperar(s,1).estado==AUTOSYNC_ACCEPTED);
  assert(autosync_offset_ms(s,0)==5350&&autosync_offset_ms(s,1)==-1200);casos++;
  LegendaCue c;assert(legenda_documento_cues(p,corpus[10].inicio+.5,5000,&c,1)==1);casos++;
  autosync_desfazer(s,0);assert(autosync_offset_ms(s,0)==350&&autosync_offset_ms(s,1)==-1200);casos++;
  assert(autosync_tentar_outra(s,1));assert(!autosync_referencia_permitida(s,1,ref));
  assert(!autosync_solicitar(s,1,ref,NULL)&&autosync_offset_ms(s,1)==-200);
  assert(autosync_referencia_permitida(s,0,ref));casos++;
  /* Session/source changes invalidate queued/running work before publication. */
  for(int i=0;i<20;i++) {
    assert(autosync_solicitar(s,0,ref,NULL));
    autosync_selecionar(s,0,q);assert(autosync_offset_ms(s,0)==0);
    usleep(3000);assert(autosync_estado(s,0).estado==AUTOSYNC_UNAVAILABLE);
    autosync_selecionar(s,0,p);
  }
  /* Framerate on slot 1: no offset is added, the position goes through the
   * map; undo returns to the identity with the manual offset intact. */
  {
    LegendaDocumento *f=documento("fps","en",0,25.0/23.976,LEGENDA_DOC_COMPLETO,11,0);
    assert(autosync_selecionar(s,1,f)&&autosync_manual(s,1,-200)&&autosync_solicitar(s,1,ref,NULL));
    AutoSyncResultado e=esperar(s,1);
    assert(e.estado==AUTOSYNC_ACCEPTED&&e.tipo==AUTOSYNC_T_ESCALA&&autosync_offset_ms(s,1)==-200);
    assert(fabs(autosync_posicao(s,1,corpus[50].inicio)-corpus[50].inicio*25.0/23.976)<.05);
    autosync_desfazer(s,1);
    assert(autosync_posicao(s,1,100)==100&&autosync_offset_ms(s,1)==-200);
    autosync_selecionar(s,1,q);legenda_documento_liberar(f);casos++;
  }
  autosync_iniciar(s,12);assert(autosync_offset_ms(s,0)==0&&!autosync_selecionar(s,0,p));
  assert(!autosync_solicitar(s,0,ref,NULL));casos++;
  autosync_iniciar(s,11);assert(autosync_referencia_permitida(s,1,ref));casos++;
  autosync_destruir(s);
  parsers();
  stress();
  legenda_documento_liberar(ref);legenda_documento_liberar(periodico);legenda_documento_liberar(periodico2);
  legenda_documento_liberar(sparse);legenda_documento_liberar(stale);legenda_documento_liberar(far);
  legenda_documento_liberar(p);legenda_documento_liberar(q);
  legenda_documento_liberar(noise);
  printf("autosync: %d corpus/state/parser checks passed\n",casos);return 0;
}
