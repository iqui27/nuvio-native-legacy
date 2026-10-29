// Publish a real parsed schedule without network or a background worker.
#include "../src/epg.c"
#include <assert.h>
void setuptv_epg(const char *key,const char *url){
  gradeSoltar(&P);memset(chaveFonteP,0,sizeof chaveFonteP);memset(hashFonteP,0,sizeof hashFonteP);
  EpgExtra e={.chave=key,.url=url};epg_extras_definir(&e,1);
  assert(wAbrir(&P));EpgProc proc={.g=&P,.fonte=1,.agora=time(NULL)};
  char start[32],end[32],xml[1024];time_t a=time(NULL)-60,b=a+3*86400;
  strftime(start,sizeof start,"%Y%m%d%H%M%S +0000",gmtime(&a));
  strftime(end,sizeof end,"%Y%m%d%H%M%S +0000",gmtime(&b));
  snprintf(xml,sizeof xml,"<tv><channel id='news'><display-name>News HD</display-name></channel><programme channel='news' start='%s' stop='%s'><title>News</title></programme></tv>",start,end);
  assert(processarBuf(&proc,xml,(long)strlen(xml)));wFechar(&P);
  snprintf(chaveFonteP[1],sizeof chaveFonteP[1],"%s",key);hashUrl(url,hashFonteP[1],sizeof hashFonteP[1]);
  nChaveFonteP=2;estado=EPG_PRONTO;
}
