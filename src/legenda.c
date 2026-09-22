#include "legenda.h"
#include "rede.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

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
  int h=0,m=0; double seg=0;
  if (sscanf(s,"%d:%d:%lf",&h,&m,&seg)==3) return h*3600.0+m*60.0+seg;
  if (sscanf(s,"%d:%lf",&m,&seg)==2) return m*60.0+seg;
  return -1;
}

static void entidade(char *s) {
  char *r=s,*w=s;
  while (*r) {
    if (*r=='<' ) {
      if (!strncasecmp(r,"<br",3)) { *w++='\n'; }
      while (*r && *r!='>') r++;
      if (*r) r++;
    } else if (!strncmp(r,"&amp;",5))  { *w++='&'; r+=5; }
    else if (!strncmp(r,"&lt;",4))   { *w++='<'; r+=4; }
    else if (!strncmp(r,"&gt;",4))   { *w++='>'; r+=4; }
    else if (!strncmp(r,"&quot;",6)) { *w++='"'; r+=6; }
    else if (!strncmp(r,"&#39;",5))  { *w++='\''; r+=5; }
    else *w++=*r++;
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

// --- SRT / WebVTT ------------------------------------------------------------

int legenda_extrair_srt(const char *corpo, LegendaCue **saida) {
  char *buf,*p,*linha; int n=0,cap=128;
  LegendaCue *v;
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
    entidade(texto); if(!texto[0])continue;
    { LegendaCue *nv=crescer(v,n,&cap); if(!nv)break; v=nv; }
    memset(&v[n],0,sizeof v[n]);
    v[n].inicio=ini; v[n].fim=fim; v[n].cor=-1;
    v[n].posX=v[n].posY=-1.0f; v[n].ordem=n;
    snprintf(v[n].texto,sizeof v[n].texto,"%s",texto); n++;
  }
  free(buf);
  if(!n){free(v);return 0;}
  *saida=v;return n;
}

// --- ASS / SSA ---------------------------------------------------------------
//
// SUBSET DELIBERADO, e a lista do que fica de fora esta no fim deste bloco.
//
// O que um ASS de fansub tem e um SRT nao tem: POSICAO (a fala embaixo, o
// letreiro traduzido em cima do cartaz), ESTILO por fala (o italico do
// pensamento), COR por estilo e varios eventos AO MESMO TEMPO. Sao essas
// quatro coisas que o relato da issue #92 descreve como "pisca" e "some
// metade das falas": o pipeline da TV ve um formato posicionado e desenha
// como se fosse texto corrido.
//
// NAO ha libass aqui, e nao e por preguica: o sysroot ARM do webOS (o SDK de
// tools/Dockerfile) tem freetype e fontconfig mas NAO tem fribidi nem
// harfbuzz, entao libass obrigaria a vendorizar duas bibliotecas novas; e no
// alvo Tizen o mesmo codigo entraria no .wasm que cada um dos 20 workers
// instancia — o eixo exato de #72/#84. O renderizador de legenda ja existe
// (player.c), funciona e e nosso; o que faltava era ALGUEM QUE ENTENDESSE O
// ARQUIVO.
//
// FICA DE FORA, de proposito: karaoke (\k e parentes viram texto normal),
// animacao (\t e \fad ignorados, \move usa a posicao inicial), rotacao e
// escala (\frx, \fscx), recorte (\clip), desenho vetorial (\p1 — o evento
// inteiro e DESCARTADO, porque o "texto" dele sao coordenadas e imprimi-las e
// pior do que nao desenhar nada), contorno e sombra do arquivo (quem manda e
// a preferencia da pessoa) e o tamanho de fonte do arquivo (idem).

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

int legenda_extrair_ass(const char *corpo, LegendaCue **saida) {
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
      if (!pt) continue;
      a=tempo(trim(ini)); b=tempo(trim(fim));
      if (a<0 || b<=a) continue;

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
      { LegendaCue *nv=crescer(v,n,&cap); if(!nv) break; v=nv; }
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

int legenda_extrair(const char *corpo, LegendaCue **saida) {
  if (legenda_eh_ass(corpo)) return legenda_extrair_ass(corpo,saida);
  return legenda_extrair_srt(corpo,saida);
}

typedef struct { char url[1400]; unsigned g; } Pedido;
static void *baixar(void *u) {
  Pedido *p=u; char *corpo=rede_baixar(p->url,20); LegendaCue *v=NULL;
  int ass=corpo?legenda_eh_ass(corpo):0;
  int n=corpo?legenda_extrair(corpo,&v):0;
  double dur=0;
  int i;
  free(corpo);
  for(i=0;i<n;i++){ double d=v[i].fim-v[i].inicio; if(d>dur)dur=d; }
  pthread_mutex_lock(&trava);
  if(p->g==geracao&&ligada){free(cues);cues=v;nCues=n;maiorDur=dur;v=NULL;}
  pthread_mutex_unlock(&trava);
  free(v);
  printf("[legenda] %s: %d blocos%s\n",ass?"ASS/SSA":"SubRip",n,n?"":" (falha)");
  fflush(stdout);
  free(p);return NULL;
}

void legenda_carregar(const char *url) {
  Pedido *p; pthread_t fio;
  if(!url||!*url)return;
  p=calloc(1,sizeof *p);if(!p)return;
  pthread_mutex_lock(&trava);
  ligada=1;p->g=++geracao;free(cues);cues=NULL;nCues=0;maiorDur=0;
  pthread_mutex_unlock(&trava);
  snprintf(p->url,sizeof p->url,"%s",url);
  if(pthread_create(&fio,NULL,baixar,p)==0)pthread_detach(fio);else free(p);
}

// O MESMO caminho de baixar(), sem rede: o corpo ja esta na mao. Serve ao
// teste de captura (tests/legenda_ass_shot.c) e a quem um dia entregar cues
// vindos de dentro do MKV (#92, fase 3).
void legenda_definir_corpo(const char *corpo) {
  LegendaCue *v=NULL; int n, i; double dur=0;
  if(!corpo)return;
  n=legenda_extrair(corpo,&v);
  for(i=0;i<n;i++){ double d=v[i].fim-v[i].inicio; if(d>dur)dur=d; }
  pthread_mutex_lock(&trava);
  ligada=1;geracao++;free(cues);cues=v;nCues=n;maiorDur=dur;
  pthread_mutex_unlock(&trava);
}

void legenda_desligar(void) {
  pthread_mutex_lock(&trava);
  ligada=0;geracao++;free(cues);cues=NULL;nCues=0;maiorDur=0;
  pthread_mutex_unlock(&trava);
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
