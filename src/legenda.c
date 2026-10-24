#include "legenda.h"
#include "assrender.h"
#include "rede.h"
#include "idioma.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static LegendaCue *cues;
static int nCues, ligada;
static unsigned geracao;
// A MAIOR DURACAO DO ARQUIVO, e o motivo dela existir esta em legenda_cues:
// com os blocos ordenados por INICIO, achar os que estao vivos num instante
// exige olhar para tras — e sem saber ate onde, "para tras" e o arquivo
// inteiro, a cada quadro.
static double maiorDur;

static double tempo(const char *s) {
  double h=0,m=0,seg=0,t;
  int usados=0;
  if (sscanf(s,"%lf:%lf:%lf%n",&h,&m,&seg,&usados)==3) {
    if (s[usados] && !isspace((unsigned char)s[usados])) return -1;
    t=h*3600.0+m*60.0+seg;
    return isfinite(t) && h>=0 && m>=0 && seg>=0 ? t : -1;
  }
  if (sscanf(s,"%lf:%lf%n",&m,&seg,&usados)==2) {
    if (s[usados] && !isspace((unsigned char)s[usados])) return -1;
    t=m*60.0+seg;
    return isfinite(t) && m>=0 && seg>=0 ? t : -1;
  }
  return -1;
}

static int tagsAss(const char *t, size_t n, LegendaCue *c);

static void entidade(char *s, LegendaCue *c) {
  char *r=s,*w=s;
  LegendaCue estilo = {0};
  while (*r) {
    char *antes = w;
    if (*r=='{' && r[1]=='\\' && strchr(r,'}')) {
      char *fim = strchr(r,'}');
      tagsAss(r+1,(size_t)(fim-r-1),&estilo);
      r = fim+1;
    } else if (*r=='<' ) {
      if (!strncasecmp(r,"<i>",3)) estilo.italico=1;
      else if (!strncasecmp(r,"</i>",4)) estilo.italico=0;
      else if (!strncasecmp(r,"<b>",3)) estilo.negrito=1;
      else if (!strncasecmp(r,"</b>",4)) estilo.negrito=0;
      if (!strncasecmp(r,"<br",3)) { *w++='\n'; }
      while (*r && *r!='>') r++;
      if (*r) r++;
    } else if (!strncmp(r,"&amp;",5))  { *w++='&'; r+=5; }
    else if (!strncmp(r,"&lt;",4))   { *w++='<'; r+=4; }
    else if (!strncmp(r,"&gt;",4))   { *w++='>'; r+=4; }
    else if (!strncmp(r,"&quot;",6)) { *w++='"'; r+=6; }
    else if (!strncmp(r,"&#39;",5))  { *w++='\''; r+=5; }
    else *w++=*r++;
    // Cue-wide emphasis is the fallback renderer's existing granularity.
    // Closing tags must not erase emphasis already used by visible text.
    if (w > antes && !isspace((unsigned char)w[-1])) {
      c->italico |= estilo.italico;
      c->negrito |= estilo.negrito;
    }
  }
  *w=0;
}

// Teto de blocos. Nao existia, e para SRT nunca fez falta: um filme tem ~1200
// legendas. Um ASS de anime tem as falas MAIS os letreiros, e ha arquivos de
// karaoke com uma linha por SILABA — dezenas de milhares de eventos, a
// sizeof(LegendaCue) cada um. Numa TV com pouca RAM isso e um jeito de morrer
// por causa de um arquivo de legenda.
#define LEG_MAX_CUES 8000

// Dobra o vetor quando `n` alcanca a capacidade. NULL quando nao ha mais para
// onde crescer — o chamador para de ler e fica com o que ja tem, que e melhor
// do que perder o arquivo inteiro.
static LegendaCue *crescer(LegendaCue *v, int n, int *cap) {
  LegendaCue *nv;
  if (n < *cap) return v;
  if (*cap >= LEG_MAX_CUES) return NULL;
  *cap *= 2;
  if (*cap > LEG_MAX_CUES) *cap = LEG_MAX_CUES;
  nv = realloc(v, (size_t)*cap * sizeof *v);
  return nv ? nv : NULL;
}

static int cmpCue(const void *a, const void *b);

// --- SRT / WebVTT ------------------------------------------------------------

static int extrairSrt(const char *corpo,LegendaCue **saida,int *parcial) {
  char *buf,*p,*linha; int n=0,cap=128;
  LegendaCue *v;
  if(parcial)*parcial=0;
  if (saida) *saida=NULL;
  if (!corpo || !saida) return 0;
  buf=strdup(corpo); if(!buf)return 0;
  v=calloc((size_t)cap,sizeof *v); if(!v){free(buf);return 0;}
  p=buf;
  if ((unsigned char)p[0]==0xef && (unsigned char)p[1]==0xbb && (unsigned char)p[2]==0xbf) p+=3;
  while (*p) {
    char *proxima=strchr(p,'\n');
    if(proxima)*proxima++=0;
    { char *q=strchr(p,'\r'); if(q)*q=0; }
    linha=p; p=proxima?proxima:p+strlen(p);
    if (!strstr(linha,"-->")) continue;
    char *seta=strstr(linha,"-->"); *seta=0; seta+=3;
    while(isspace((unsigned char)*seta))seta++;
    for(char *q=linha;*q;q++)if(*q==',')*q='.';
    for(char *q=seta;*q;q++)if(*q==',')*q='.';
    double ini=tempo(linha),fim=tempo(seta);
    // Um bloco com tempo ruim (fim<=inicio, comum em legenda da comunidade) cai
    // sozinho: nao torna o resto um prefixo. Parcial e so falta de memoria.
    if(ini<0||fim<=ini)continue;
    char texto[768]={0}; size_t usado=0;
    while(*p) {
      char *nl=strchr(p,'\n'); if(nl)*nl++=0;
      { char *q=strchr(p,'\r');if(q)*q=0; }
      if(!*p){p=nl?nl:p;break;}
      size_t l=strlen(p),resta=sizeof texto-usado-1;
      if(usado&&resta){texto[usado++]='\n';resta--;}
      if(l>resta)l=resta;memcpy(texto+usado,p,l);usado+=l;texto[usado]=0;
      p=nl?nl:p+strlen(p);
    }
    { LegendaCue *nv=crescer(v,n,&cap); if(!nv){if(parcial)*parcial=1;break;} v=nv; }
    memset(&v[n],0,sizeof v[n]);
    v[n].inicio=ini; v[n].fim=fim; v[n].cor=-1;
    v[n].posX=v[n].posY=-1.0f; v[n].ordem=n;
    entidade(texto,&v[n]); if(!texto[0])continue;
    snprintf(v[n].texto,sizeof v[n].texto,"%s",texto); n++;
  }
  free(buf);
  if(!n){free(v);return 0;}
  // legenda_cues usa busca binaria e a mesma janela de duracao do ASS.
  // SRT/VTT reordenado pelo servidor tambem precisa satisfazer esse contrato.
  qsort(v,(size_t)n,sizeof *v,cmpCue);
  *saida=v;return n;
}

int legenda_extrair_srt(const char *corpo,LegendaCue **saida) {
  return extrairSrt(corpo,saida,NULL);
}

// --- ASS / SSA ---------------------------------------------------------------
//
// O que um ASS de fansub tem e um SRT nao tem: POSICAO (a fala embaixo, o
// letreiro traduzido em cima do cartaz), ESTILO por fala (o italico do
// pensamento), COR por estilo e varios eventos AO MESMO TEMPO. Sao essas
// quatro coisas que o relato da issue #92 descreve como "pisca" e "some
// metade das falas": o pipeline da TV ve um formato posicionado e desenha
// como se fosse texto corrido.
//
// O caminho de producao completo esta em assrender.c e usa libass quando
// NV_ASS_LIBASS foi ligado no alvo (as dependencias estaticas sao preparadas
// por tools/Dockerfile e tools/build-ass-wasm.sh). Este parser permanece como
// fallback verificavel para SRT/VTT e para diagnostico quando uma faixa ASS
// chega incompleta ou o backend nao esta presente. Nesse fallback o subconjunto
// abaixo e intencionalmente pequeno; ele nao e usado quando libass aceitou o
// documento original.

#define ASS_MAX_ESTILOS 96

typedef struct {
  char nome[72];
  int  an, negrito, italico, cor;
} AssEstilo;

static char *trim(char *s) {
  char *f;
  while (isspace((unsigned char)*s)) s++;
  f = s + strlen(s);
  while (f > s && isspace((unsigned char)f[-1])) *--f = 0;
  return s;
}

// "&H00FF8000" ou "&HFF8000&" -> 0xRRGGBB. O ASS guarda BGR, e trocar os
// canais aqui e o que impede um letreiro amarelo de sair azul.
static int corAss(const char *s, int *rgb) {
  unsigned long v; char *fim;
  while (isspace((unsigned char)*s)) s++;
  if (*s=='&' && (s[1]=='H'||s[1]=='h')) s+=2;
  else if (*s=='H'||*s=='h') s++;
  if (!isxdigit((unsigned char)*s)) return 0;
  v = strtoul(s,&fim,16);
  if (fim==s) return 0;
  *rgb = (int)(((v & 0xFFUL)<<16) | (((v>>8) & 0xFFUL)<<8) | ((v>>16) & 0xFFUL));
  return 1;
}

// Alinhamento do SSA antigo (1..11, com 9/10/11 no meio da tela) para o \an do
// ASS (1..9). Arquivos "[V4 Styles]" e a tag \a usam a tabela velha, e ler um
// pelo outro poe a fala do rodape no meio da tela.
static int anDeLegado(int a) {
  int col = ((a-1)%4)+1;      /* 1 esq, 2 centro, 3 dir */
  if (col>3) col=2;
  if (a>=9)  return 3+col;    /* meio  -> 4,5,6 */
  if (a>=5)  return 6+col;    /* topo  -> 7,8,9 */
  return col;                 /* base  -> 1,2,3 */
}

// Indice da coluna `nome` numa linha "Format: a, b, c". -1 se nao houver.
static int colunaDe(const char *fmt, const char *nome) {
  const char *p = strchr(fmt,':');
  int i = 0;
  if (!p) return -1;
  p++;
  while (*p) {
    const char *ini = p, *fim;
    while (*p && *p!=',') p++;
    fim = p;
    while (ini<fim && isspace((unsigned char)*ini)) ini++;
    while (fim>ini && isspace((unsigned char)fim[-1])) fim--;
    if ((int)strlen(nome)==(int)(fim-ini) && !strncasecmp(ini,nome,(size_t)(fim-ini)))
      return i;
    if (*p==',') p++;
    i++;
  }
  return -1;
}

// Ponteiro para o campo `idx` de uma linha "Dialogue: a,b,c,...". O ULTIMO
// campo (o texto) pode ter virgulas — quase sempre tem —, e por isso esta
// funcao devolve o resto da linha em vez de recortar.
static const char *campoAss(const char *linha, int idx) {
  const char *p = strchr(linha,':');
  int i;
  if (!p) return NULL;
  p++;
  for (i=0;i<idx;i++) {
    p = strchr(p,',');
    if (!p) return NULL;
    p++;
  }
  return p;
}

static void copiaCampo(const char *p, char *dst, size_t tam) {
  size_t n = 0;
  if (!p) { if (tam) dst[0]=0; return; }
  while (p[n] && p[n]!=',' && n<tam-1) { dst[n]=p[n]; n++; }
  dst[n]=0;
}

// Uma sequencia de tags entre chaves. Devolve 1 quando o evento deve ser
// DESCARTADO (desenho vetorial).
static int tagsAss(const char *t, size_t n, LegendaCue *c) {
  size_t i = 0;
  int descarta = 0;
  while (i < n) {
    if (t[i] != '\\') { i++; continue; }
    i++;
    if (i >= n) break;
    // A ORDEM DAS COMPARACOES E A CORRECAO: "an" antes de "a" (senao \an8 le
    // como alinhamento legado 8), "pos" antes de "p" (senao \pos vira desenho
    // vetorial e o evento inteiro some), e \c so quando vem colado no &H
    // (senao \clip casa com a cor).
    if (!strncasecmp(t+i,"an",2) && i+2<n && isdigit((unsigned char)t[i+2])) {
      int a = t[i+2]-'0';
      if (a>=1 && a<=9) c->an = (short)a;
      i += 3;
    } else if (!strncasecmp(t+i,"pos(",4)) {
      float x,y;
      if (sscanf(t+i+4,"%f,%f",&x,&y)==2) { c->posX=x; c->posY=y; }
      i += 4;
    } else if (!strncasecmp(t+i,"move(",5)) {
      // \move anima de (x1,y1) ate (x2,y2). Sem animacao, o lugar certo de
      // parar e o INICIO: e onde o fansub quis que a linha aparecesse.
      float x,y;
      if (sscanf(t+i+5,"%f,%f",&x,&y)==2) { c->posX=x; c->posY=y; }
      i += 5;
    } else if ((t[i]=='a'||t[i]=='A') && i+1<n && isdigit((unsigned char)t[i+1])) {
      int a = atoi(t+i+1);
      if (a>=1 && a<=11) c->an = (short)anDeLegado(a);
      i += 2;
    } else if ((t[i]=='i'||t[i]=='I') && i+1<n && (t[i+1]=='0'||t[i+1]=='1')) {
      c->italico = (short)(t[i+1]=='1');
      i += 2;
    } else if ((t[i]=='b'||t[i]=='B') && i+1<n && isdigit((unsigned char)t[i+1])) {
      // \b1 liga, \b0 desliga, \b700 e um peso — qualquer coisa acima de zero
      // e "mais grosso que o normal", que e tudo o que este renderizador sabe
      // fazer.
      c->negrito = (short)(atoi(t+i+1) > 0);
      i += 2;
    } else if ((t[i]=='c'||t[i]=='C') && i+1<n && (t[i+1]=='&'||t[i+1]=='H')) {
      int rgb; if (corAss(t+i+1,&rgb)) c->cor = rgb;
      i += 2;
    } else if (t[i]=='1' && i+1<n && (t[i+1]=='c'||t[i+1]=='C')) {
      int rgb; if (corAss(t+i+2,&rgb)) c->cor = rgb;
      i += 2;
    } else if ((t[i]=='p'||t[i]=='P') && i+1<n && isdigit((unsigned char)t[i+1])) {
      if (atoi(t+i+1) > 0) descarta = 1;
      i += 2;
    } else {
      i++;
    }
  }
  return descarta;
}

// Texto do evento -> texto desenhavel. Tira as tags, aplica as que este
// renderizador entende e resolve \N (quebra dura), \n e \h.
static int textoAss(const char *bruto, char *dst, size_t tam, LegendaCue *c) {
  size_t w = 0;
  const char *s = bruto;
  int descarta = 0;
  while (*s && w < tam-1) {
    if (*s=='{') {
      const char *f = strchr(s,'}');
      size_t n = f ? (size_t)(f-s-1) : strlen(s+1);
      if (tagsAss(s+1,n,c)) descarta = 1;
      if (!f) break;
      s = f+1;
      continue;
    }
    if (*s=='\\' && (s[1]=='N'||s[1]=='n')) {
      // \N e quebra dura. \n so quebra quando o arquivo pede quebra manual
      // (WrapStyle 2) e, fora disso, o proprio libass trata como espaco — que
      // e o que fazemos, porque quebrar onde o fansub nao quis parte a fala
      // em duas linhas no meio de uma frase.
      if (s[1]=='N') dst[w++]='\n';
      else if (w && dst[w-1]!=' ' && dst[w-1]!='\n') dst[w++]=' ';
      s += 2;
      continue;
    }
    if (*s=='\\' && s[1]=='h') { dst[w++]=' '; s+=2; continue; }
    dst[w++] = *s++;
  }
  dst[w]=0;
  // Espaco solto nas pontas e comum depois de tirar as tags.
  { char *t = trim(dst); if (t!=dst) memmove(dst,t,strlen(t)+1); }
  return !descarta;
}

int legenda_eh_ass(const char *corpo) {
  const char *p;
  if (!corpo) return 0;
  if ((unsigned char)corpo[0]==0xef && (unsigned char)corpo[1]==0xbb &&
      (unsigned char)corpo[2]==0xbf) corpo += 3;
  // Cabecalho OU eventos: ha arquivo servido sem [Script Info] e ha arquivo
  // com a secao e sem nenhum Dialogue. Qualquer um dos dois ja descarta o
  // caminho do SRT, que exige "-->" na linha de tempo.
  p = corpo;
  while (*p) {
    const char *q = p;
    while (*q && *q != '\n' && isspace((unsigned char)*q)) q++;
    if (!strncasecmp(q, "[Script Info]", 13) ||
        !strncasecmp(q, "[Events]", 8) ||
        !strncasecmp(q, "[V4+ Styles]", 13) ||
        !strncasecmp(q, "[V4 Styles]", 12)) return 1;
    // Alguns servidores entregam um ASS/SSA reduzido, sem seções. A detecção
    // precisa acompanhar o parser (case-insensitive e aceitando recuo), senão
    // `dialogue:` cai no caminho SRT e desaparece sem diagnóstico.
    if (!strncasecmp(q, "Dialogue:", 9)) return 1;
    p = strchr(p, '\n');
    if (!p) break;
    p++;
  }
  return 0;
}

static int cmpCue(const void *a, const void *b) {
  const LegendaCue *x=a,*y=b;
  if (x->inicio < y->inicio) return -1;
  if (x->inicio > y->inicio) return 1;
  return x->ordem - y->ordem;
}

static int extrairAss(const char *corpo,LegendaCue **saida,int *parcial) {
  char *buf,*p;
  LegendaCue *v;
  AssEstilo est[ASS_MAX_ESTILOS];
  int nEst=0, n=0, cap=128;
  int secao=0;            /* 1 info, 2 estilos, 3 eventos */
  int legado=0;           /* [V4 Styles] usa o alinhamento antigo */
  float resX=0, resY=0;
  int cNome=0,cCor=1,cNeg=2,cIta=3,cAlin=4;   /* colunas de Style: */
  int temFmtEstilo=0;
  int cIni=1,cFim=2,cEstilo=3,cTexto=9;       /* colunas de Dialogue: */

  if(parcial)*parcial=0;
  if (saida) *saida=NULL;
  if (!corpo || !saida) return 0;
  buf=strdup(corpo); if(!buf) return 0;
  v=calloc((size_t)cap,sizeof *v); if(!v){free(buf);return 0;}
  p=buf;
  if ((unsigned char)p[0]==0xef && (unsigned char)p[1]==0xbb && (unsigned char)p[2]==0xbf) p+=3;

  while (*p) {
    char *prox=strchr(p,'\n'), *linha;
    if (prox) *prox++=0;
    { char *q=strchr(p,'\r'); if(q)*q=0; }
    linha=trim(p);
    p = prox ? prox : p+strlen(p);
    if (!*linha) continue;

    if (linha[0]=='[') {
      if (!strncasecmp(linha,"[Script Info]",13)) secao=1;
      else if (!strncasecmp(linha,"[V4+ Styles]",13) ||
               !strncasecmp(linha,"[V4 Styles]",12)) {
        secao=2; legado = !strchr(linha,'+');
      }
      else if (!strncasecmp(linha,"[Events]",8)) secao=3;
      else secao=0;
      continue;
    }
    if (secao==1) {
      if (!strncasecmp(linha,"PlayResX:",9)) resX=(float)atof(linha+9);
      else if (!strncasecmp(linha,"PlayResY:",9)) resY=(float)atof(linha+9);
      continue;
    }
    if (secao==2) {
      if (!strncasecmp(linha,"Format:",7)) {
        // As colunas de Style: NAO sao fixas — o SSA v4 tem TertiaryColour
        // onde o ASS v4+ tem OutlineColour, e ler por posicao fixa troca a cor
        // da fala pela cor do contorno em metade dos arquivos.
        int k;
        temFmtEstilo=1;
        k=colunaDe(linha,"Name");          if(k>=0) cNome=k;
        k=colunaDe(linha,"PrimaryColour"); if(k>=0) cCor=k;
        k=colunaDe(linha,"Bold");          if(k>=0) cNeg=k;
        k=colunaDe(linha,"Italic");        if(k>=0) cIta=k;
        k=colunaDe(linha,"Alignment");     if(k>=0) cAlin=k;
        continue;
      }
      if (!strncasecmp(linha,"Style:",6) && nEst<ASS_MAX_ESTILOS && temFmtEstilo) {
        AssEstilo *e=&est[nEst];
        char tmp[96];
        memset(e,0,sizeof *e);
        e->cor=-1;
        copiaCampo(campoAss(linha,cNome),e->nome,sizeof e->nome);
        { char *t=trim(e->nome); if(t!=e->nome) memmove(e->nome,t,strlen(t)+1); }
        copiaCampo(campoAss(linha,cCor),tmp,sizeof tmp);
        corAss(tmp,&e->cor);
        copiaCampo(campoAss(linha,cNeg),tmp,sizeof tmp); e->negrito = atoi(trim(tmp))!=0;
        copiaCampo(campoAss(linha,cIta),tmp,sizeof tmp); e->italico = atoi(trim(tmp))!=0;
        copiaCampo(campoAss(linha,cAlin),tmp,sizeof tmp);
        { int a=atoi(trim(tmp));
          if (a>=1 && a<=11) e->an = legado ? anDeLegado(a) : (a<=9?a:0); }
        if (e->nome[0]) nEst++;
        continue;
      }
      continue;
    }
    // Ha SSA reduzido sem secoes nem Format: em addons antigos. Quando a
    // linha ja se identifica como Dialogue, os indices padrao acima bastam;
    // em qualquer outro lugar preservamos a separacao normal das secoes.
    if (secao!=3 && strncasecmp(linha,"Dialogue:",9)) continue;

    if (!strncasecmp(linha,"Format:",7)) {
      int k;
      k=colunaDe(linha,"Start"); if(k>=0) cIni=k;
      k=colunaDe(linha,"End");   if(k>=0) cFim=k;
      k=colunaDe(linha,"Style"); if(k>=0) cEstilo=k;
      k=colunaDe(linha,"Text");  if(k>=0) cTexto=k;
      continue;
    }
    // "Comment:" e a linha que o fansub DESLIGOU. Desenha-la e mostrar o
    // rascunho de quem traduziu.
    if (strncasecmp(linha,"Dialogue:",9)) continue;
    {
      char ini[64],fim[64],nomeEst[72],texto[768];
      LegendaCue c;
      const char *pt;
      double a,b;
      copiaCampo(campoAss(linha,cIni),ini,sizeof ini);
      copiaCampo(campoAss(linha,cFim),fim,sizeof fim);
      copiaCampo(campoAss(linha,cEstilo),nomeEst,sizeof nomeEst);
      pt = campoAss(linha,cTexto);
      if (!pt) {if(parcial)*parcial=1;continue;}
      a=tempo(trim(ini)); b=tempo(trim(fim));
      if (a<0 || b<=a) continue;   // so o bloco ruim cai (ver extrairSrt)

      memset(&c,0,sizeof c);
      c.cor=-1; c.posX=c.posY=-1.0f;
      { char *nm=trim(nomeEst); int i;
        for (i=0;i<nEst;i++)
          if (!strcasecmp(est[i].nome,nm)) {
            c.an=(short)est[i].an; c.negrito=(short)est[i].negrito;
            c.italico=(short)est[i].italico; c.cor=est[i].cor;
            break;
          } }
      // As tags da PROPRIA LINHA vem depois do estilo e mandam nele: e assim
      // que um {\i1} num dialogo normal vira pensamento.
      if (!textoAss(pt,texto,sizeof texto,&c)) continue;
      if (!texto[0]) continue;
      c.inicio=a; c.fim=b; c.resX=resX; c.resY=resY; c.ordem=n;
      snprintf(c.texto,sizeof c.texto,"%s",texto);
      { LegendaCue *nv=crescer(v,n,&cap); if(!nv){if(parcial)*parcial=1;break;} v=nv; }
      v[n++]=c;
    }
  }
  free(buf);
  if (!n) { free(v); return 0; }
  // ORDENA POR TEMPO. O arquivo costuma vir em ordem, mas "costuma" nao serve
  // de invariante para a busca binaria de legenda_cues — e ASS com letreiros
  // inseridos depois da traducao sai fora de ordem com frequencia.
  qsort(v,(size_t)n,sizeof *v,cmpCue);
  *saida=v; return n;
}

int legenda_extrair_ass(const char *corpo,LegendaCue **saida) {
  return extrairAss(corpo,saida,NULL);
}

int legenda_extrair(const char *corpo, LegendaCue **saida) {
  if (legenda_eh_ass(corpo)) return legenda_extrair_ass(corpo,saida);
  return legenda_extrair_srt(corpo,saida);
}

// --- CHARSET (pedido do dono, 28/09, comparando com o fork do Corby7) ---------
//
// O parser e o desenho falam UTF-8. Legenda do OpenSubtitles em portugues,
// espanhol ou frances chega muitas vezes em Windows-1252 (Latin-1), e ai todo
// acento e um byte solto que nao forma UTF-8: o texto saia com quadrados.
// UTF-16 (com BOM) tem zeros no meio e nem chegava ao parser — rede_baixar
// devolve texto e o strlen parava no primeiro zero.
//
// A ORDEM: BOM de UTF-16 -> converte; BOM de UTF-8 -> tira; UTF-8 valido ->
// fica; o resto e um codepage de 8 bits. Entre os de 8 bits so da para
// ADIVINHAR, e a adivinhacao aqui e uma so: texto cirilico (Windows-1251) e
// feito de PALAVRAS inteiras de bytes altos, enquanto o latino tem o acento
// solto entre letras ASCII. Mais da metade dos bytes altos com vizinho alto ->
// 1251; senao 1252. Grego, turco etc. em codepage proprio NAO estao cobertos
// (sairiam como letras latinas erradas, nao quadrados).
//
// ARABE (Windows-1256, #247 #261) tambem e feito de palavras de bytes altos, e
// se separa do cirilico pela FAIXA: no 1256 as letras ficam em 0xC1-0xDF mais
// seis soltas (lam, mim, nun, ha, waw, ya: E1 E3 E4 E5 E6 EC ED), e as posicoes
// E0 E2 E7-EB EE EF sao letras francesas que quase nao aparecem. No 1251 essas
// mesmas posicoes sao а в з и й к л о п, perto de metade de um texto russo.
static const unsigned short CP1252_80[32] = {
  0x20AC,0xFFFD,0x201A,0x0192,0x201E,0x2026,0x2020,0x2021,0x02C6,0x2030,0x0160,0x2039,0x0152,0xFFFD,0x017D,0xFFFD,
  0xFFFD,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0x02DC,0x2122,0x0161,0x203A,0x0153,0xFFFD,0x017E,0x0178 };
static const unsigned short CP1251_80[64] = {
  0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021,0x20AC,0x2030,0x0409,0x2039,0x040A,0x040C,0x040B,0x040F,
  0x0452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0xFFFD,0x2122,0x0459,0x203A,0x045A,0x045C,0x045B,0x045F,
  0x00A0,0x040E,0x045E,0x0408,0x00A4,0x0490,0x00A6,0x00A7,0x0401,0x00A9,0x0404,0x00AB,0x00AC,0x00AD,0x00AE,0x0407,
  0x00B0,0x00B1,0x0406,0x0456,0x0491,0x00B5,0x00B6,0x00B7,0x0451,0x2116,0x0454,0x00BB,0x0458,0x0405,0x0455,0x0457 };

static const unsigned short CP1256_80[128] = {
  0x20AC,0x067E,0x201A,0x0192,0x201E,0x2026,0x2020,0x2021,0x02C6,0x2030,0x0679,0x2039,0x0152,0x0686,0x0698,0x0688,
  0x06AF,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0x06A9,0x2122,0x0691,0x203A,0x0153,0x200C,0x200D,0x06BA,
  0x00A0,0x060C,0x00A2,0x00A3,0x00A4,0x00A5,0x00A6,0x00A7,0x00A8,0x00A9,0x06BE,0x00AB,0x00AC,0x00AD,0x00AE,0x00AF,
  0x00B0,0x00B1,0x00B2,0x00B3,0x00B4,0x00B5,0x00B6,0x00B7,0x00B8,0x00B9,0x061B,0x00BB,0x00BC,0x00BD,0x00BE,0x061F,
  0x06C1,0x0621,0x0622,0x0623,0x0624,0x0625,0x0626,0x0627,0x0628,0x0629,0x062A,0x062B,0x062C,0x062D,0x062E,0x062F,
  0x0630,0x0631,0x0632,0x0633,0x0634,0x0635,0x0636,0x00D7,0x0637,0x0638,0x0639,0x063A,0x0640,0x0641,0x0642,0x0643,
  0x00E0,0x0644,0x00E2,0x0645,0x0646,0x0647,0x0648,0x00E7,0x00E8,0x00E9,0x00EA,0x00EB,0x0649,0x064A,0x00EE,0x00EF,
  0x064B,0x064C,0x064D,0x064E,0x00F4,0x064F,0x0650,0x00F7,0x0651,0x00F9,0x0652,0x00FB,0x00FC,0x200E,0x200F,0x06D2 };

static int poeUtf8(char *d, unsigned cp) {
  if (cp < 0x80)    { d[0]=(char)cp; return 1; }
  if (cp < 0x800)   { d[0]=(char)(0xC0|cp>>6); d[1]=(char)(0x80|(cp&63)); return 2; }
  if (cp < 0x10000) { d[0]=(char)(0xE0|cp>>12); d[1]=(char)(0x80|((cp>>6)&63));
                      d[2]=(char)(0x80|(cp&63)); return 3; }
  d[0]=(char)(0xF0|cp>>18); d[1]=(char)(0x80|((cp>>12)&63));
  d[2]=(char)(0x80|((cp>>6)&63)); d[3]=(char)(0x80|(cp&63)); return 4;
}

static int utf8Valido(const unsigned char *p, long n) {
  long i = 0;
  while (i < n) {
    unsigned c = p[i];
    int k = c < 0x80 ? 0 : (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
    if (k < 0 || (k == 1 && c < 0xC2) || i + k >= n + (k ? 0 : 1)) return 0;
    for (int j = 1; j <= k; j++) if ((p[i + j] & 0xC0) != 0x80) return 0;
    i += k + 1;
  }
  return 1;
}

char *legenda_utf8(const char *bytes, long n, const char **origem) {
  const unsigned char *b = (const unsigned char *)bytes;
  const char *o = "utf-8";
  char *out, *d;
  long i;
  if (!bytes || n < 0) return NULL;
  if (n >= 2 && ((b[0] == 0xFF && b[1] == 0xFE) || (b[0] == 0xFE && b[1] == 0xFF))) {
    int le = b[0] == 0xFF;
    o = le ? "utf-16le" : "utf-16be";
    d = out = malloc((size_t)(n / 2) * 3 + 4);
    if (!out) return NULL;
    for (i = 2; i + 1 < n; i += 2) {
      unsigned cp = le ? (unsigned)(b[i] | b[i+1] << 8) : (unsigned)(b[i] << 8 | b[i+1]);
      if (cp >= 0xD800 && cp <= 0xDBFF && i + 3 < n) {
        unsigned lo = le ? (unsigned)(b[i+2] | b[i+3] << 8) : (unsigned)(b[i+2] << 8 | b[i+3]);
        if (lo >= 0xDC00 && lo <= 0xDFFF) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); i += 2; }
        else cp = 0xFFFD;
      } else if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;
      if (!cp) continue;
      d += poeUtf8(d, cp);
    }
    *d = 0;
  } else {
    if (n >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) { b += 3; n -= 3; }
    if (utf8Valido(b, n)) {
      out = malloc((size_t)n + 1);
      if (!out) return NULL;
      memcpy(out, b, (size_t)n); out[n] = 0;
    } else {
      long altos = 0, colados = 0, soRusso = 0, soltasArabe = 0;
      int cir, ara;
      for (i = 0; i < n; i++)
        if (b[i] >= 0xC0) {
          unsigned c = b[i];
          altos++;
          if ((i && b[i-1] >= 0xC0) || (i + 1 < n && b[i+1] >= 0xC0)) colados++;
          if (c == 0xE0 || c == 0xE2 || (c >= 0xE7 && c <= 0xEB) || c == 0xEE || c == 0xEF) soRusso++;
          else if (c == 0xE1 || (c >= 0xE3 && c <= 0xE6) || c == 0xEC || c == 0xED) soltasArabe++;
        }
      cir = altos >= 8 && colados * 2 > altos;
      ara = cir && soRusso * 10 < altos && soltasArabe * 10 >= altos;
      if (ara) cir = 0;
      o = ara ? "windows-1256" : cir ? "windows-1251" : "windows-1252";
      d = out = malloc((size_t)n * 3 + 1);
      if (!out) return NULL;
      for (i = 0; i < n; i++) {
        unsigned c = b[i], cp;
        if (!c) continue;
        if (c < 0x80) cp = c;
        else if (ara) cp = CP1256_80[c - 0x80];
        else if (cir) cp = c >= 0xC0 ? 0x0410 + (c - 0xC0) : CP1251_80[c - 0x80];
        else cp = c < 0xA0 ? CP1252_80[c - 0x80] : c;
        d += poeUtf8(d, cp);
      }
      *d = 0;
    }
  }
  if (origem) *origem = o;
  return out;
}

/* LTR tracks keep the existing overlay. Arabic plain text uses libass. */
static char *documentoTexto(const LegendaCue *v, int n) {
#ifdef NV_ASS_LIBASS
  return plainass_document(v, n);
#else
  (void)v; (void)n; return NULL;
#endif
}

typedef struct PreDownload {
  char url[1400];
  char *bruto;
  long nb;
  int refs, terminou;
  unsigned sessao;
  struct PreDownload *next;
} PreDownload;
static PreDownload *downloadsSessao;
static unsigned preGeracao;
static PreDownload *preDownload;
static int preTentou;
static pthread_cond_t prePronto = PTHREAD_COND_INITIALIZER;

// Todas as referencias e o corpo ficam sob trava; o fio nunca toca a UI.
static void preLiberar(PreDownload *p) {
  if (!--p->refs) { free(p->bruto); free(p); }
}
static void preDescartar(const char *motivo) {
  if (!preDownload) return;
  printf("[legenda] pre-download: descartado (%s)\n", motivo);
  preGeracao++; preLiberar(preDownload); preDownload = NULL;
}
void legenda_pre_descartar(const char *motivo) {
  pthread_mutex_lock(&trava);
  preDescartar(motivo);
  pthread_mutex_unlock(&trava);
}
void legenda_pre_reiniciar(void) {
  pthread_mutex_lock(&trava);
  preDescartar("novo play"); preGeracao++; preTentou = 0;
  while (downloadsSessao) {
    PreDownload *p = downloadsSessao;
    downloadsSessao = p->next;
    preLiberar(p);
  }
  pthread_mutex_unlock(&trava);
}

typedef struct { char url[1400]; unsigned g; LegendaBaixada pronto; void *u; PreDownload *pre; } Pedido;

/* So o host do link, para o log de tempo: caminho e query levam a chave do
 * addon/debrid e nunca saem daqui; "usuario:senha@" tambem cai fora. */
static void hostDoLink(const char *url, char *dst, size_t n) {
  const char *a = strstr(url, "://"), *f, *arroba;
  size_t k;
  a = a ? a + 3 : url;
  f = a + strcspn(a, "/?#");
  for (arroba = a; arroba < f; arroba++) if (*arroba == '@') a = arroba + 1;
  k = (size_t)(f - a);
  if (k >= n) k = n - 1;
  memcpy(dst, a, k); dst[k] = 0;
  if (!k) snprintf(dst, n, "-");
}

static double msAgora(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

static void *preBaixar(void *u) {
  PreDownload *p = u;
  double t0 = msAgora(); long nb = 0;
  char *bruto = rede_baixar_bin(p->url, 20, &nb);
  char host[128]; hostDoLink(p->url, host, sizeof host);
  printf("[legenda] download: host=%s %ld bytes em %.0f ms%s\n",
         host, bruto ? nb : 0L, msAgora() - t0, bruto ? "" : " (falhou)");
  fflush(stdout);
  pthread_mutex_lock(&trava);
  p->bruto = bruto; p->nb = nb; p->terminou = 1;
  if (preDownload == p && p->sessao == preGeracao) {
    if (bruto) printf("[legenda] pre-download: pronto em %.0f ms antes do video\n", msAgora() - t0);
    else preDescartar("falha no download");
  }
  pthread_cond_broadcast(&prePronto);
  preLiberar(p);
  pthread_mutex_unlock(&trava);
  return NULL;
}
/* Cache owns one reference; each consumer and the worker own another. */
static PreDownload *downloadSessao(const char *url) {
  PreDownload *p; pthread_t fio;
  for (p = downloadsSessao; p; p = p->next)
    if (!strcmp(p->url, url) && (!p->terminou || p->bruto)) { p->refs++; return p; }
  p = calloc(1, sizeof *p);
  if (!p) return NULL;
  snprintf(p->url, sizeof p->url, "%s", url);
  p->refs = 3; p->sessao = preGeracao;
  if (pthread_create(&fio, NULL, preBaixar, p)) { free(p); return NULL; }
  pthread_detach(fio);
  p->next = downloadsSessao; downloadsSessao = p;
  return p;
}
void legenda_pre_baixar(const char *url) {
  PreDownload *p;
  if (!url || !*url) return;
  pthread_mutex_lock(&trava);
  if (preTentou) { pthread_mutex_unlock(&trava); return; }
  preTentou = 1;
  p = downloadSessao(url);
  if (p) preDownload = p;
  pthread_mutex_unlock(&trava);
}

static void *baixar(void *u) {
  Pedido *p=u; long nb=0; const char *cs="utf-8";
  char host[128]; double t0=msAgora();
  hostDoLink(p->url,host,sizeof host);
  char *bruto;
  int reaproveitado = p->pre != NULL;
  if (p->pre) {
    pthread_mutex_lock(&trava);
    while (!p->pre->terminou) pthread_cond_wait(&prePronto, &trava);
    nb = p->pre->nb;
    bruto = p->pre->bruto ? malloc((size_t)nb + 1) : NULL;
    if (bruto) { memcpy(bruto, p->pre->bruto, (size_t)nb); bruto[nb] = 0; }
    preLiberar(p->pre); p->pre = NULL;
    pthread_mutex_unlock(&trava);
  } else bruto = rede_baixar_bin(p->url,20,&nb);
  if (!reaproveitado) {
    printf("[legenda] download: host=%s %ld bytes em %.0f ms%s\n",host,bruto?nb:0L,msAgora()-t0,bruto?"":" (falhou)");
    fflush(stdout);
  }
  char *corpo=bruto?legenda_utf8(bruto,nb,&cs):NULL; LegendaCue *v=NULL;
  free(bruto);
  if (corpo && strcmp(cs,"utf-8")) { printf("[legenda] texto em %s, convertido para UTF-8\n",cs); fflush(stdout); }
  int ass=corpo?legenda_eh_ass(corpo):0;
  int n=corpo?legenda_extrair(corpo,&v):0;
  char *plain = !ass ? documentoTexto(v, n) : NULL;
  double dur=0;
  int aceitarAss=0;
  int i;
  for(i=0;i<n;i++){ double d=v[i].fim-v[i].inicio; if(d>dur)dur=d; }
  pthread_mutex_lock(&trava);
  if(p->g==geracao&&ligada){
    free(cues);cues=v;nCues=n;maiorDur=dur;v=NULL;
    aceitarAss=ass ? 1 : plain ? 2 : 0;
    assrender_geracao(geracao);
  }
  pthread_mutex_unlock(&trava);
  /* O parser legado continua preenchendo cues para SRT/VTT e para o
   * diagnostico. Quando o documento ASS chegou inteiro, libass recebe o
   * corpo original, sem passar pelo limite de 768 bytes de uma cue. */
  if (aceitarAss) {
    if (aceitarAss == 2) assrender_carregar_texto(plain, strlen(plain), p->g);
    else assrender_carregar(corpo, strlen(corpo), p->g);
    fprintf(stderr, "[legenda] %s\n", assrender_diagnostico());
  } else if (ass && p->g == geracao) {
    // So o dono atual limpa: um download ATRASADO de outra faixa (a pessoa ja
    // trocou) apagava o libass da faixa nova.
    assrender_limpar();
  }
  /* Preserve the new AutoSync download callback and its ownership. */
  if (p->pronto) p->pronto(corpo, p->g, legenda_ligada_em(p->g), p->u);
  free(plain);
  free(corpo);
  free(v);
  printf("[legenda] %s: %d blocos%s\n",ass?"ASS/SSA":"SubRip",n,n?"":" (falha)");
  fflush(stdout);
  free(p);return NULL;
}

void legenda_carregar(const char *url) { legenda_carregar_com(url, NULL, NULL); }
unsigned legenda_carregar_com(const char *url, LegendaBaixada pronto, void *u) {
  Pedido *p; pthread_t fio; unsigned g;
  if(!url||!*url){ if(pronto)pronto(NULL,0,0,u); return 0; }
  p=calloc(1,sizeof *p);if(!p){ if(pronto)pronto(NULL,0,0,u); return 0; }
  p->pronto=pronto;p->u=u;
  pthread_mutex_lock(&trava);
  if (preDownload && !strcmp(preDownload->url, url)) {
    p->pre = preDownload; preDownload = NULL; // transfere a referencia ao pedido
  } else {
    preDescartar("outra escolha");
    p->pre = downloadSessao(url);
  }
  ligada=1;p->g=++geracao;free(cues);cues=NULL;nCues=0;maiorDur=0;
  pthread_mutex_unlock(&trava);
  assrender_geracao(p->g);
  assrender_limpar_fontes();
  snprintf(p->url,sizeof p->url,"%s",url);
  g=p->g;
  if(pthread_create(&fio,NULL,baixar,p)==0)pthread_detach(fio);
  else {
    pthread_mutex_lock(&trava);
    if (p->pre) preLiberar(p->pre);
    pthread_mutex_unlock(&trava);
    if(pronto)pronto(NULL,g,0,u); free(p);
  }
  return g;
}

// O MESMO caminho de baixar(), sem rede: o corpo ja esta na mao. Serve ao
// teste de captura (tests/legenda_ass_shot.c) e a quem um dia entregar cues
// vindos de dentro do MKV (#92, fase 3).
static unsigned definirCorpo(const char *corpo, int checar, unsigned dono) {
  LegendaCue *v=NULL; int n, i; double dur=0; int ass; unsigned g; char *plain;
  if(!corpo)return 0;
  ass=legenda_eh_ass(corpo);
  n=legenda_extrair(corpo,&v);
  plain = !ass ? documentoTexto(v, n) : NULL;
  for(i=0;i<n;i++){ double d=v[i].fim-v[i].inicio; if(d>dur)dur=d; }
  pthread_mutex_lock(&trava);
  if(checar && geracao!=dono){ pthread_mutex_unlock(&trava); free(v); free(plain); return 0; }
  ligada=1;geracao++;g=geracao;free(cues);cues=v;nCues=n;maiorDur=dur;
  pthread_mutex_unlock(&trava);
  assrender_geracao(g);
  assrender_limpar();
  if (ass) {
    assrender_carregar(corpo, strlen(corpo), g);
    fprintf(stderr, "[legenda] %s\n", assrender_diagnostico());
  }
  if (plain) { assrender_carregar_texto(plain, strlen(plain), g); free(plain); }
  return g;
}

void legenda_definir_corpo(const char *corpo) { definirCorpo(corpo, 0, 0); }

unsigned legenda_definir_corpo_se(const char *corpo, unsigned dono) {
  return definirCorpo(corpo, 1, dono);
}

unsigned legenda_geracao(void) {
  unsigned g;
  pthread_mutex_lock(&trava); g=geracao; pthread_mutex_unlock(&trava);
  return g;
}

int legenda_ligada_em(unsigned dono) {
  int ok;
  pthread_mutex_lock(&trava); ok=ligada&&geracao==dono; pthread_mutex_unlock(&trava);
  return ok;
}

/* Lote seguinte da faixa `dono`. Diferente de legenda_atualizar_corpo, NUNCA
 * cai no caminho cheio: legenda desligada ou de outro dono e resposta de uma
 * faixa que ja saiu, e descarta. */
int legenda_atualizar_corpo_se(const char *corpo, unsigned dono) {
  LegendaCue *v=NULL; int n, i; double dur=0; int ass;
  if(!corpo||!legenda_ligada_em(dono))return 0;
  ass=legenda_eh_ass(corpo);
  n=legenda_extrair(corpo,&v);
  for(i=0;i<n;i++){ double d=v[i].fim-v[i].inicio; if(d>dur)dur=d; }
  pthread_mutex_lock(&trava);
  if(!ligada||geracao!=dono){ pthread_mutex_unlock(&trava); free(v); return 0; }
  free(cues);cues=v;nCues=n;maiorDur=dur;
  pthread_mutex_unlock(&trava);
  if (ass) assrender_atualizar(corpo, strlen(corpo), dono);
  return 1;
}

/* Lote seguinte da MESMA faixa (#92): troca os cues e o documento do libass
 * sem mudar a geracao. legenda_definir_corpo a cada lote apagava o quadro em
 * tela, desligava o libass por alguns quadros (o overlay antigo desenhava a
 * fala com outra fonte nesse intervalo) e reenviava as fontes: um pisca por
 * lote. Desligada (primeiro lote, ou apos uma troca), cai no caminho cheio. */
void legenda_atualizar_corpo(const char *corpo) {
  LegendaCue *v=NULL; int n, i; double dur=0; int ass, ok; unsigned g=0;
  if(!corpo)return;
  ass=legenda_eh_ass(corpo);
  n=legenda_extrair(corpo,&v);
  for(i=0;i<n;i++){ double d=v[i].fim-v[i].inicio; if(d>dur)dur=d; }
  pthread_mutex_lock(&trava);
  ok=ligada;
  if(ok){ g=geracao; free(cues);cues=v;nCues=n;maiorDur=dur; v=NULL; }
  pthread_mutex_unlock(&trava);
  if(!ok){ free(v); legenda_definir_corpo(corpo); return; }
  if (ass) assrender_atualizar(corpo, strlen(corpo), g);
}

void legenda_desligar(void) {
  pthread_mutex_lock(&trava);
  preDescartar(i18n("legenda desligada"));
  ligada=0;geracao++;free(cues);cues=NULL;nCues=0;maiorDur=0;
  pthread_mutex_unlock(&trava);
  assrender_geracao(geracao);
  assrender_limpar_fontes();
}

// Primeiro bloco cujo INICIO passa de `t`. Com o vetor ordenado, tudo o que
// pode estar vivo em `t` esta ANTES daqui.
static int primeiroDepois(double t) {
  int lo=0,hi=nCues;
  while(lo<hi){int m=(lo+hi)/2; if(cues[m].inicio<=t) lo=m+1; else hi=m;}
  return lo;
}

int legenda_cues(double posSeg, int atrasoMs, LegendaCue *dst, int max) {
  double t = posSeg + (double)atrasoMs/1000.0;
  int achados=0, i, k;
  if (!dst || max<=0) return 0;
  pthread_mutex_lock(&trava);
  k = primeiroDepois(t);
  // ANDA PARA TRAS e para no primeiro bloco que comecou antes da janela da
  // maior duracao do arquivo: dali para tras nao existe bloco que ainda possa
  // estar no ar. Sem esse limite a varredura seria o arquivo inteiro, a cada
  // quadro — e um ASS de anime tem milhares de eventos.
  for (i=k-1; i>=0 && achados<max; i--) {
    if (cues[i].inicio < t - maiorDur) break;
    if (t >= cues[i].inicio && t <= cues[i].fim) dst[achados++]=cues[i];
  }
  pthread_mutex_unlock(&trava);
  // A varredura devolve do mais NOVO para o mais antigo; quem desenha espera a
  // ordem do arquivo.
  for (i=0;i<achados/2;i++) {
    LegendaCue tmp=dst[i]; dst[i]=dst[achados-1-i]; dst[achados-1-i]=tmp;
  }
  return achados;
}

int legenda_texto(double posSeg,int atrasoMs,char *dst,size_t tam) {
  LegendaCue c;
  if(!dst||!tam)return 0;
  dst[0]=0;
  if(legenda_cues(posSeg,atrasoMs,&c,1)!=1)return 0;
  snprintf(dst,tam,"%s",c.texto);
  return 1;
}

struct LegendaDocumento {
  pthread_mutex_t trava;
  unsigned refs;
  LegendaDocumentoInfo info;
  LegendaCue *cues;
  int n;
  double maiorDur;
  uint64_t hash;
};

static uint64_t documentoHash(uint64_t h, const void *bytes, size_t n) {
  const unsigned char *p=bytes;
  while(n--) { h ^= *p++; h *= UINT64_C(1099511628211); }
  return h;
}

/* Takes ownership even on error; sort/hash never run under the overlay lock. */
static LegendaDocumento *documentoAdotar(LegendaCue *v,int n,
                                        const LegendaDocumentoInfo *info) {
  LegendaDocumento *d; int i;
  if (!info || !v || n<=0 || n>LEG_MAX_CUES ||
      !isfinite(info->duracaoSeg) || info->duracaoSeg<0) {free(v);return NULL;}
  for(i=0;i<n;i++)
    if(!isfinite(v[i].inicio)||!isfinite(v[i].fim)||
       v[i].inicio<0||v[i].fim<=v[i].inicio) {free(v);return NULL;}
  d=calloc(1,sizeof *d); if(!d){free(v);return NULL;}
  d->cues=v;
  if(pthread_mutex_init(&d->trava,NULL)) {
    free(v);free(d);return NULL;
  }
  d->info=*info;
  d->info.idioma[sizeof d->info.idioma-1]=0;
  d->info.origem[sizeof d->info.origem-1]=0;
  d->info.identidade[sizeof d->info.identidade-1]=0;
  d->n=n;d->refs=1;
  qsort(d->cues,(size_t)n,sizeof *v,cmpCue);
  d->hash=UINT64_C(14695981039346656037);
  d->hash=documentoHash(d->hash,d->info.idioma,strlen(d->info.idioma));
  d->hash=documentoHash(d->hash,d->info.origem,strlen(d->info.origem));
  d->hash=documentoHash(d->hash,d->info.identidade,strlen(d->info.identidade));
  for(i=0;i<n;i++) {
    LegendaCue *c=&d->cues[i]; c->texto[sizeof c->texto-1]=0;
    double dur=c->fim-c->inicio;
    if(dur>d->maiorDur)d->maiorDur=dur;
    d->hash=documentoHash(d->hash,&c->inicio,sizeof c->inicio);
    d->hash=documentoHash(d->hash,&c->fim,sizeof c->fim);
    d->hash=documentoHash(d->hash,c->texto,strlen(c->texto));
  }
  return d;
}

LegendaDocumento *legenda_documento_de_cues(const LegendaCue *v,int n,
                                           const LegendaDocumentoInfo *info) {
  if(!v||!info||n<=0||n>LEG_MAX_CUES)return NULL;
  LegendaCue *copia=malloc((size_t)n*sizeof *v);if(!copia)return NULL;
  memcpy(copia,v,(size_t)n*sizeof *v);
  return documentoAdotar(copia,n,info);
}

LegendaDocumento *legenda_documento_criar(const char *corpo,
                                        const LegendaDocumentoInfo *info) {
  /* Keep a worker-side parse from duplicating an unbounded server response. */
  if(!corpo || strnlen(corpo,16u*1024u*1024u+1)>16u*1024u*1024u)return NULL;
  if(!info)return NULL;
  LegendaCue *v=NULL;int parcial=0;
  int n=legenda_eh_ass(corpo)?extrairAss(corpo,&v,&parcial):extrairSrt(corpo,&v,&parcial);
  LegendaDocumentoInfo real=*info;
  /* Legacy playback can render a valid prefix after OOM/malformed timings;
   * AutoSync must never mistake that useful prefix for a complete reference. */
  if(parcial)real.flags&=~LEGENDA_DOC_COMPLETO;
  return documentoAdotar(v,n,&real);
}

LegendaDocumento *legenda_documento_bytes(const char *bytes,long n,
                                         const LegendaDocumentoInfo *info) {
  if(!bytes||n<=0||n>16L*1024L*1024L)return NULL;
  char *corpo=legenda_utf8(bytes,n,NULL);
  LegendaDocumento *d=legenda_documento_criar(corpo,info);
  free(corpo);return d;
}

LegendaDocumento *legenda_documento_ativo(unsigned dono,
                                         const LegendaDocumentoInfo *info) {
  int n=0;LegendaCue *v;
  pthread_mutex_lock(&trava);
  if(ligada&&geracao==dono)n=nCues;
  pthread_mutex_unlock(&trava);
  if(n<=0||!info)return NULL;
  v=malloc((size_t)n*sizeof *v);if(!v)return NULL;
  pthread_mutex_lock(&trava);
  if(!ligada||geracao!=dono||nCues!=n){pthread_mutex_unlock(&trava);free(v);return NULL;}
  memcpy(v,cues,(size_t)n*sizeof *v);
  pthread_mutex_unlock(&trava);
  return documentoAdotar(v,n,info);
}

LegendaDocumento *legenda_documento_reter(LegendaDocumento *d) {
  if(!d)return NULL;
  pthread_mutex_lock(&d->trava);d->refs++;pthread_mutex_unlock(&d->trava);
  return d;
}
void legenda_documento_liberar(LegendaDocumento *d) {
  int fim;
  if(!d)return;
  pthread_mutex_lock(&d->trava);fim=!--d->refs;pthread_mutex_unlock(&d->trava);
  if(fim){pthread_mutex_destroy(&d->trava);free(d->cues);free(d);}
}
const LegendaDocumentoInfo *legenda_documento_info(const LegendaDocumento *d) {
  return d?&d->info:NULL;
}
const LegendaCue *legenda_documento_dados(const LegendaDocumento *d,int *n) {
  if(n)*n=d?d->n:0;return d?d->cues:NULL;
}
uint64_t legenda_documento_hash(const LegendaDocumento *d) {return d?d->hash:0;}
int legenda_documento_cues(const LegendaDocumento *d,double posSeg,
                           int atrasoMs,LegendaCue *dst,int max) {
  double t=posSeg+(double)atrasoMs/1000;int lo=0,hi,i,n=0;
  if(!d||!dst||max<=0||!isfinite(t))return 0;
  hi=d->n;
  while(lo<hi){int m=(lo+hi)/2;if(d->cues[m].inicio<=t)lo=m+1;else hi=m;}
  for(i=lo-1;i>=0&&n<max;i--){
    if(d->cues[i].inicio<t-d->maiorDur)break;
    if(t<=d->cues[i].fim)dst[n++]=d->cues[i];
  }
  for(i=0;i<n/2;i++){LegendaCue tmp=dst[i];dst[i]=dst[n-1-i];dst[n-1-i]=tmp;}
  return n;
}
