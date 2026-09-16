#include "badges.h"
#include "js.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
static const char *ids[]={"r-4k","r-1080","r-720","q-remux","q-bluray","q-webdl","q-webrip","q-seadex","v-dv","v-hdr10plus","v-hdr10","v-hdr","v-imax-enhanced","v-imax","v-sdr","a-atmos-dv","a-atmos","a-truehd-dv","a-truehd","a-dtsx","a-dtshdma","a-dtshd","a-dts","a-dd-dv","a-ddp","a-dd","c-71","c-51","co-x265","co-x264","co-av1","p-netflix","p-prime","p-appletv","p-disney","p-max","p-hulu","p-peacock","p-paramount","p-crave","p-crunchyroll"};
#define NB (sizeof ids/sizeof ids[0])
static struct {char image[700],name[64];} art[NB];
static uint64_t bit(const char *id){for(size_t i=0;i<NB;i++)if(!strcmp(id,ids[i]))return UINT64_C(1)<<i;return 0;}
static int token(const char *s,const char *t){size_t n=strlen(t);for(const char *p=s;(p=strstr(p,t));p++)if((p==s||!isalnum((unsigned char)p[-1]))&&!isalnum((unsigned char)p[n]))return 1;return 0;}
uint64_t badges_detectar(const char *metadata) {
  char s[4096];size_t n=0;for(;metadata&&metadata[n]&&n<sizeof s-1;n++)s[n]=(char)tolower((unsigned char)metadata[n]);s[n]=0;
  uint64_t m=0;
#define HAS(t) token(s,t)
#define ADD(id) (m|=bit(id))
  int dv=HAS("dv")||HAS("dovi")||HAS("dolby vision")||HAS("dolby.vision")||HAS("dolby-vision")||HAS("dolby_vision");
  int atmos=HAS("atmos"),thd=HAS("truehd")||HAS("true-hd")||HAS("true hd");
  int ddp=HAS("ddp")||strstr(s,"ddp5")||strstr(s,"ddp7")||HAS("dd+")||HAS("eac3")||HAS("eac-3")||HAS("e-ac-3");
  int dd=HAS("ac3")||HAS("ac-3")||HAS("dd5.1")||HAS("dd2.0");
  if((HAS("4k")||HAS("2160p")||HAS("2160")||HAS("uhd"))&&!HAS("1080p")&&!HAS("720p"))ADD("r-4k");
  if(HAS("1080p")||HAS("1080i")||HAS("1080"))ADD("r-1080");
  if(HAS("720p")||HAS("720"))ADD("r-720");
  if(HAS("remux"))ADD("q-remux");else if(HAS("bluray")||HAS("blu-ray"))ADD("q-bluray");
  if(HAS("web-dl")||HAS("webdl")||HAS("web.dl")||HAS("web dl"))ADD("q-webdl");
  if(HAS("webrip")||HAS("web-rip")||HAS("web.rip"))ADD("q-webrip");
  if(HAS("seadex"))ADD("q-seadex");
  if(HAS("hdr10+")||HAS("hdr10plus")||HAS("hdr10p"))ADD("v-hdr10plus");
  else if(HAS("hdr10"))ADD("v-hdr10");else if(HAS("hdr")||HAS("hlg"))ADD("v-hdr");
  if(HAS("imax enhanced")||HAS("imax.enhanced"))ADD("v-imax-enhanced");else if(HAS("imax"))ADD("v-imax");
  if(HAS("sdr"))ADD("v-sdr");
  if(atmos)ADD(dv?"a-atmos-dv":"a-atmos");
  else if(thd)ADD(dv?"a-truehd-dv":"a-truehd");
  else if(ddp||dd)ADD(dv?"a-dd-dv":ddp?"a-ddp":"a-dd");
  else if(dv)ADD("v-dv");
  if(HAS("dts:x")||HAS("dts-x")||HAS("dts.x"))ADD("a-dtsx");
  else if(HAS("dts-hd ma")||HAS("dts-hd.ma")||HAS("dts.hd.ma")||HAS("dts-ma"))ADD("a-dtshdma");
  else if(HAS("dts-hd")||HAS("dts.hd"))ADD("a-dtshd");else if(HAS("dts"))ADD("a-dts");
  if(HAS("7.1")||HAS("7.0"))ADD("c-71");else if(HAS("5.1")||HAS("5.0"))ADD("c-51");
  if(HAS("hevc")||HAS("x265")||HAS("h265")||HAS("h.265"))ADD("co-x265");
  if(HAS("avc")||HAS("x264")||HAS("h264")||HAS("h.264"))ADD("co-x264");if(HAS("av1"))ADD("co-av1");
  if(HAS("netflix")||HAS("nflx"))ADD("p-netflix");
  if(HAS("amzn")||HAS("amazon")||HAS("prime video")||HAS("primevideo"))ADD("p-prime");
  if(HAS("atvp")||HAS("atv+")||HAS("apple tv")||HAS("appletv"))ADD("p-appletv");
  if(HAS("dsnp")||HAS("disney"))ADD("p-disney");if(HAS("hmax")||HAS("hbo")||HAS("max"))ADD("p-max");
  if(HAS("hulu"))ADD("p-hulu");if(HAS("pcok")||HAS("peacock"))ADD("p-peacock");
  if(HAS("pmtp")||HAS("paramount"))ADD("p-paramount");if(HAS("crave"))ADD("p-crave");
  if(HAS("crunchyroll"))ADD("p-crunchyroll");
#undef HAS
#undef ADD
  return m;
}
uint64_t badges_provedor(const char *name){return badges_detectar(name)&(~UINT64_C(0)<<31);}
const char *badges_fonte_hdr(uint64_t mask) {
  if (mask & bit("v-hdr10plus")) return "Fonte HDR10+";
  if (mask & bit("v-hdr10")) return "Fonte HDR10";
  if (mask & bit("v-hdr")) return "Fonte HDR";
  return NULL;
}
void badges_carregar(const char *dir) {
  char path[700];snprintf(path,sizeof path,"%s/badges/index.json",dir);FILE *f=fopen(path,"rb");if(!f)return;
  char body[24000];size_t n=fread(body,1,sizeof body-1,f);body[n]=0;fclose(f);
  for(const char *p=js_array(body,NULL,"badges");p;p=js_prox(js_fim(p))){char id[48],name[64],file[128];const char *e=js_fim(p);
    js_texto(p,e,"id",id,sizeof id);js_texto(p,e,"name",name,sizeof name);js_texto(p,e,"image",file,sizeof file);
    if(strchr(file,'/')||strstr(file,".."))continue;
    for(size_t i=0;i<NB;i++)if(!strcmp(ids[i],id)){snprintf(art[i].image,sizeof art[i].image,"%s/badges/%s",dir,file);snprintf(art[i].name,sizeof art[i].name,"%s",name);}
  }
}
// FILEIRA DE BADGES, clara ou escura. A escura existe porque desde 16/09 a
// linha SELECIONADA e uma superficie CLARA (ver o comentario do preenchimento
// em streams.c): arte branca sobre linha branca nao se ve. Relato do dono:
// "nas fontes as badges inferiores ... nao mudaram de cor para preto quando
// selecionadas".
//
// DA PARA TINGIR porque as 41 artes de badges/ sao MONOCROMATICAS BRANCAS — a
// forma mora no alfa e o RGB e 255 em todas (conferido arquivo por arquivo).
// Entao o escuro vai por GFX_MARCA, que pega a forma do alfa e a cor de uCor.
//
// O CLARO CONTINUA EM GFX_TEXTO de proposito. Os dois modos dao o mesmo pixel
// para arte branca opaca, mas divergem na borda reduzida (o GFX_TEXTO filtra o
// RGB junto, o GFX_MARCA nao), e o claro e o que detail.c, home.c e vertudo.c
// ja desenham hoje. Trocar o modo deles nao e o defeito relatado.
static float fileira(uint64_t mask,float x,float y,float maxW,float h,float a,int escuro) {
  float start=x;
  // A TINTA ESCURA E A SUPERFICIE DE REPOUSO da linha nao selecionada
  // (.135,.135,.14 em streams.c). Sobre a linha clara ela e a tinta que o app
  // ja usa, e nao um cinza inventado so para esta fileira.
  float cr=escuro?.135f:1.f,cg=escuro?.135f:1.f,cb=escuro?.14f:1.f;
  int t1=escuro?26:225,t2=escuro?28:228,t3=escuro?34:235;
  for(size_t i=0;i<NB;i++)if((mask&(UINT64_C(1)<<i))&&art[i].image[0]) {
    GLuint t=tex_obter_larg(art[i].image,128);float aspect=tex_aspecto(art[i].image),w=aspect>0?h*aspect:80;
    if(w>144)w=144;if(x+w>start+maxW)break;
    if(t&&aspect>0){float height=w/aspect;gfx_rect((GfxRect){x,y+(h-height)*.5f,w,height},t,escuro?GFX_MARCA:GFX_TEXTO,0,0,0,0,cr,cg,cb,a);}
    else {TxtLinha l=txt_linha_corta(TXT_MINI,art[i].name,t1,t2,t3,255,w);txt_desenhar_alpha(l,x,y+(h-l.h)*.5f,a);}
    x+=w+14;
  }return x-start;
}
float badges_desenhar(uint64_t mask,float x,float y,float maxW,float h,float a) {
  return fileira(mask,x,y,maxW,h,a,0);
}
float badges_desenhar_escura(uint64_t mask,float x,float y,float maxW,float h,float a) {
  return fileira(mask,x,y,maxW,h,a,1);
}
