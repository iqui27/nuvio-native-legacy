// Follow-up to #532: add-ons often put audio hints in release labels instead
// of audioLanguages. Keep these separate from declared tracks and parse once.
static int audioRotuloPalavra(unsigned char c) { return isalnum(c) || c>=0x80; }
static const char *audioRotuloIdioma(const char *p,const char *fim,char token[128]) {
  size_t n=(size_t)(fim-p);if(!n || n>=128)return NULL;
  memcpy(token,p,n);token[n]=0;
  static const struct {const char *nome,*codigo;} aliases[]={
    {"Türkçe","tr"},{"TÜRKÇE","tr"},{"Turkce","tr"},{"Dublaj","tr"},{"Dublajlı","tr"},
    {"Deutsch","de"},{"Polski","pl"},{"Français","fr"},{"Francais","fr"},
    {"VFF","fr"},{"VFQ","fr"},{"TrueFrench","fr"},{"Castellano","es"},
    {"Espanol","es"},{"ESP","es"},{"Latino","es"},{"LATAM","es"},{"💃🏻","es"},
    {"CHS","zh"},{"CHT","zh"},{"JP","ja"},{"Nederlands","nl"},{"Svenska","sv"},{"Dansk","da"},
    {"Norsk","no"},{"Suomi","fi"},{"Magyar","hu"}
  };
  for(size_t i=0;i<sizeof aliases/sizeof *aliases;i++)
    if(!strcasecmp(token,aliases[i].nome))return aliases[i].codigo;
  return faixaIdiomaTextoConhecido(p,fim)?token:NULL;
}
static int audioRotuloSubs(const char *p,const char *fim) {
  static const char *const nomes[]={"sub","subs","subtitle","subtitles","subbed","softsub","softsubs",
    "hardsub","hardsubs","leg","legenda","legendas","legendado","altyazi","altyazı","altyazılı","vostfr"};
  for(size_t i=0;i<sizeof nomes/sizeof *nomes;i++) {
    size_t n=strlen(nomes[i]);
    if((size_t)(fim-p)>=n && !strncasecmp(p,nomes[i],n) &&
       (p+n==fim || !audioRotuloPalavra((unsigned char)p[n])))return 1;
  }
  return 0;
}
static size_t audioRotuloBandeira(const char *p,const char *fim) {
  if(fim-p<8 || memcmp(p,"\xF0\x9F\x87",3) || memcmp(p+4,"\xF0\x9F\x87",3))return 0;
  return (unsigned char)p[3]>=0xA6 && (unsigned char)p[3]<=0xBF &&
    (unsigned char)p[7]>=0xA6 && (unsigned char)p[7]<=0xBF?8:0;
}
// Flags and formatter icons delimit words even without a space (1080p🇹🇷).
// Keep UTF-8 letters together so Türkçe remains a complete language token.
static const char *audioRotuloFimPalavra(const char *p,const char *fim) {
  while(p<fim && audioRotuloPalavra((unsigned char)*p)) {
    int tipo;
    if(audioRotuloBandeira(p,fim) || faixasMarcadorTexto(p,fim,&tipo))break;
    p++;
  }
  return p;
}
static const char *audioRotuloSeparadores(const char *p,const char *fim) {
  while(p<fim && (isspace((unsigned char)*p) || strchr(".,/|;_+-[]()",*p)))p++;
  return p;
}
// A suffix such as ENG/TR SUBS labels the preceding languages as subtitles.
// Torrentio's separate "...flags... Subs / ..." starts a new section instead.
static int audioRotuloSufixoSubs(const char *p,const char *fim) {
  for(int i=0;i<16;i++) {
    p=audioRotuloSeparadores(p,fim);if(p==fim)return 0;
    if(audioRotuloSubs(p,fim)) {
      const char *q=audioRotuloFimPalavra(p,fim);
      q=faixasEspacos(q,fim);return q==fim || *q!='/';
    }
    size_t flag=audioRotuloBandeira(p,fim);if(flag){p+=flag;continue;}
    const char *q=audioRotuloFimPalavra(p,fim);
    char token[128];if(q==p || !audioRotuloIdioma(p,q,token))return 0;
    p=q;
  }
  return 0;
}
static void audioRotulosTexto(const char *texto,char *dst,unsigned tam) {
  if(!texto || !faixasUtf8((const unsigned char *)texto))return;
  int tipo=1;
  for(const char *linha=texto;*linha;) {
    const char *fim=linha+strcspn(linha,"\r\n");
    for(const char *p=linha;p<fim;) {
      int rotulo=tipo;
      const char *q=faixasRotuloTexto(p,fim,&rotulo);
      if(q!=p){tipo=rotulo;p=q;continue;}
      if(audioRotuloSubs(p,fim)){tipo=2;p=audioRotuloFimPalavra(p,fim);continue;}
      size_t n=audioRotuloBandeira(p,fim);
      if(n) {
        const char *idioma=faixaIdiomaEmoji(p,p+n);
        if(!idioma && !memcmp(p,"🇺🇸",n))idioma="English";
        if(!idioma && !memcmp(p,"🇲🇽",n))idioma="Spanish";
        if(tipo!=2 && idioma && !audioRotuloSufixoSubs(p+n,fim))
          faixaIdiomaAdicionar(dst,tam,idioma,idioma+strlen(idioma));
        p+=n;continue;
      }
      n=faixasMarcadorTexto(p,fim,&rotulo);
      if(n){if(rotulo)tipo=rotulo==2?2:1;p+=n;continue;}
      // Separators and line breaks do not turn a subtitle list into audio.
      // A following explicit audio label/icon can start a new audio section.
      if(*p=='|' || *p==';'){p++;continue;}
      if(!audioRotuloPalavra((unsigned char)*p)){p++;continue;}
      q=audioRotuloFimPalavra(p,fim);
      // URLs are transport details, not language tags, even inside a title.
      if(fim-q>=3 && !memcmp(q,"://",3)) {
        while(q<fim && !isspace((unsigned char)*q) && *q!='|')q++;
        p=q;continue;
      }
      char token[128];const char *idioma=audioRotuloIdioma(p,q,token);
      // Avoid treating ordinary lowercase words (it/is/as/hi) as ISO codes.
      int curto=q-p==2 && !(isupper((unsigned char)p[0]) && isupper((unsigned char)p[1]));
      int isolado=(p==linha && q==fim) || (p>linha && strchr("._[(/+-|",p[-1])) || (q<fim && strchr("._])",*q));
      if(tipo!=2 && idioma && (!curto || isolado) && !audioRotuloSufixoSubs(q,fim))
        faixaIdiomaAdicionar(dst,tam,idioma,idioma+strlen(idioma));
      p=q;
    }
    linha=*fim?fim+1:fim;
  }
}
