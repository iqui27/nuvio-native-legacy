#include "badges.h"
#include "js.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "ajustes.h"
#include "idioma.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdlib.h>
static const char *ids[]={"r-4k","r-1080","r-720","r-sd","q-remux","q-bluray","q-webdl","q-webrip","q-seadex","v-dv","v-hdr10plus","v-hdr10","v-hdr","v-hlg","v-imax-enhanced","v-imax","v-sdr","a-atmos-dv","a-atmos","a-truehd-dv","a-truehd","a-dtsx","a-dtshdma","a-dtshd","a-dts","a-dd-dv","a-ddp","a-dd","c-71","c-51","co-x265","co-x264","co-av1","p-netflix","p-prime","p-appletv","p-disney","p-max","p-hulu","p-peacock","p-paramount","p-crave","p-crunchyroll"};
#define NB (sizeof ids/sizeof ids[0])
static struct {char image[700],name[64];} art[NB];
// BUSCA DO BIT LEMBRADA PELO ENDERECO DO TEXTO. Quem chama passa literal
// ("r-4k", "v-dv"...), e a folha de Fontes chama isto milhares de vezes por
// quadro (grupoRes/ehHdr/nivelHdr por fonte, 2-3 montagens por quadro):
// MEDIDO no Mac (06/10/2026) com 201 fontes, 37% do fio principal em strcmp
// aqui dentro. A casa guarda so o INDICE (um byte), e o acerto e conferido
// pelo conteudo: endereco reaproveitado com outro texto, ou duas threads
// escrevendo a mesma casa, so caem na varredura de sempre — nunca bit errado.
#define NB_MEMO 128
static unsigned char bitMemo[NB_MEMO];
static uint64_t bit(const char *id){
  unsigned c=(unsigned)(((uintptr_t)id>>2)%NB_MEMO), k=bitMemo[c];
  if(k<NB&&!strcmp(id,ids[k]))return UINT64_C(1)<<k;
  for(size_t i=0;i<NB;i++)if(!strcmp(id,ids[i])){bitMemo[c]=(unsigned char)i;return UINT64_C(1)<<i;}
  return 0;}
static int token(const char *s,const char *t){size_t n=strlen(t);for(const char *p=s;(p=strstr(p,t));p++)if((p==s||!isalnum((unsigned char)p[-1]))&&!isalnum((unsigned char)p[n]))return 1;return 0;}
uint64_t badges_detectar(const char *metadata) {
  char s[4096];size_t n=0;for(;metadata&&metadata[n]&&n<sizeof s-1;n++)s[n]=(char)tolower((unsigned char)metadata[n]);s[n]=0;
  uint64_t m=0;
#define HAS(t) token(s,t)
#define ADD(id) (m|=bit(id))
  int dv=HAS("dv")||HAS("dovi")||HAS("dolbyvision")||HAS("dolby vision")||HAS("dolby.vision")||HAS("dolby-vision")||HAS("dolby_vision");
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
  else if(HAS("hdr10"))ADD("v-hdr10");else if(HAS("hlg"))ADD("v-hlg");else if(HAS("hdr"))ADD("v-hdr");
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
// Provedores comecam em p-netflix; a conta sai do nome e nao de um 31 cravado,
// que quebrou em silencio quando entrou o v-hlg no meio da tabela.
uint64_t badges_bit(const char *id){return bit(id);}
uint64_t badges_provedor(const char *name){
  static uint64_t dePartida;
  if(!dePartida){uint64_t b=bit("p-netflix");dePartida=~UINT64_C(0)<<__builtin_ctzll(b);}
  return badges_detectar(name)&dePartida;
}
// Marca do HDR que a FONTE anuncia (nao o que o painel ativou), ou -1. Trocou
// badges_fonte_hdr, que devolvia a frase "Fonte HDR10+" em texto.
int badges_fonte_hdr_marca(uint64_t mask) {
  if (mask & bit("v-hdr10plus")) return FMT_HDR10P;
  if (mask & bit("v-hdr10")) return FMT_HDR10;
  if (mask & bit("v-hlg")) return FMT_HLG;
  if (mask & bit("v-hdr")) return FMT_HDR;
  return -1;
}

// --- MARCA DE FORMATO, UMA SO PORTA (29/09/2026) -----------------------------
//
// Pedido do dono: "vamos usar as logos sempre que formos referenciar HDR,
// HDR10+, Dolby Vision, Dolby Atmos, etc." Antes cada tela escrevia a palavra
// (a folha de fontes "Dolby Vision · Atmos", o player "Dolby Vision", "HDR10",
// os Ajustes "DV"/"ATMOS") e so a fileira de badges da folha usava a arte.
//
// As artes sao as mesmas de badges/: brancas, forma no ALFA. Por isso o
// desenho vai sempre por GFX_MARCA, que tinge com a cor pedida — segue a tinta
// do foco, o vidro e o realce sem arquivo novo.
static const struct { const char *id; const char *texto; } FORMATOS[FMT_N] = {
  [FMT_4K]={"r-4k","4K"},[FMT_1080]={"r-1080","1080p"},[FMT_720]={"r-720","720p"},
  // SD nao vinha no pacote de marcas: r-sd.webp (29/09/2026) foi desenhada na
  // mesma caixa do 4K (altura de glifo 119 em 194, topo em 37), para a fileira
  // de resolucoes ter um corpo so. badges_detectar NAO a acende: "sd" solto no
  // nome de uma fonte e ruido demais; so quem sabe a resolucao (o guia, o
  // player) pede FMT_SD.
  [FMT_SD]={"r-sd","SD"},
  [FMT_SDR]={"v-sdr","SDR"},[FMT_HDR]={"v-hdr","HDR"},[FMT_HDR10]={"v-hdr10","HDR10"},
  [FMT_HDR10P]={"v-hdr10plus","HDR10+"},[FMT_HLG]={"v-hlg","HLG"},
  [FMT_DV]={"v-dv","Dolby Vision"},[FMT_ATMOS]={"a-atmos","Dolby Atmos"},
  [FMT_DTS]={"a-dts","DTS"},[FMT_DTSX]={"a-dtsx","DTS:X"},[FMT_DTSHD]={"a-dtshd","DTS-HD"},
  [FMT_TRUEHD]={"a-truehd","Dolby TrueHD"},[FMT_DD]={"a-dd","Dolby Digital"},
  [FMT_DDP]={"a-ddp","Dolby Digital+"},[FMT_IMAX]={"v-imax","IMAX"},
  [FMT_IMAX_ENH]={"v-imax-enhanced","IMAX Enhanced"},[FMT_AV1]={"co-av1","AV1"},
  [FMT_HEVC]={"co-x265","HEVC"},[FMT_AVC]={"co-x264","AVC"},[FMT_REMUX]={"q-remux","Remux"},
};
static int indiceFormato(FormatoMarca f) {
  if ((int)f < 0 || f >= FMT_N) return -1;
  for (size_t i = 0; i < NB; i++) if (!strcmp(ids[i], FORMATOS[f].id)) return (int)i;
  return -1;
}
int marca_resolucao(const char *nome) {
  if (!nome || !nome[0]) return -1;
  if (!strcasecmp(nome, "4K") || !strcasecmp(nome, "UHD") || !strcasecmp(nome, "2160p")) return FMT_4K;
  if (!strcasecmp(nome, "1080p") || !strcasecmp(nome, "FHD")) return FMT_1080;
  if (!strcasecmp(nome, "720p") || !strcasecmp(nome, "HD")) return FMT_720;
  if (!strcasecmp(nome, "SD")) return FMT_SD;
  return -1;
}
const char *marca_formato_nome(FormatoMarca f) {
  return ((int)f >= 0 && f < FMT_N) ? FORMATOS[f].texto : "";
}
// Largura sem desenhar, para o chamador alinhar a direita ou centralizar. Com a
// textura ainda nao decodificada tex_aspecto vale 0 e a largura cai no palpite
// (2,2 x altura): o quadro seguinte corrige, e a marca aparece no lugar certo.
float marca_formato_largura(FormatoMarca f, float altura) {
  int i = indiceFormato(f);
  float asp;
  if (i < 0) return 0.0f;
  if (!art[i].image[0]) return (float)txt_largura(TXT_MINI, FORMATOS[f].texto);
  asp = tex_aspecto(art[i].image);
  if (asp <= 0.0f) { tex_obter_larg(art[i].image, 128); asp = 2.2f; }
  { float w = altura * asp; return w > altura * 4.6f ? altura * 4.6f : w; }
}
float marca_formato(FormatoMarca f, float x, float y, float altura,
                    float r, float g, float b, float a) {
  int i = indiceFormato(f);
  GLuint t;
  float asp;
  if (i < 0) return 0.0f;
  if (art[i].image[0]) {
    // Decodifica na largura de uso: 128 cobre as marcas de linha, e as largas
    // (DTS-HD a 44 px de caixa passa de 180) pedem 256 para nao borrar.
    float w = altura * 4.6f;
    t = tex_obter_larg(art[i].image, altura * 4.16f > 128.0f ? 256.0f : 128.0f);
    asp = tex_aspecto(art[i].image);
    if (t && asp > 0.0f) {
      w = altura * asp;
      if (w > altura * 4.6f) w = altura * 4.6f;
      gfx_rect((GfxRect){ x, y + (altura - w / asp) * 0.5f, w, w / asp }, t,
               GFX_MARCA, 0, 0, 0, 0, r, g, b, a);
      return w;
    }
  }
  // SEM ARTE (pacote sem badges/, ou ainda decodificando): o nome em texto, na
  // mesma cor. Melhor a palavra que um buraco — e some assim que a textura vem.
  { TxtLinha l = txt_linha(TXT_MINI, FORMATOS[f].texto, (int)(r * 255.0f), (int)(g * 255.0f),
                           (int)(b * 255.0f), 255);
    txt_desenhar_alpha(l, x, y + (altura - (float)l.h) * 0.5f, a);
    return (float)l.w; }
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
// DA PARA TINGIR porque as 42 artes de badges/ sao MONOCROMATICAS BRANCAS — a
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
// A FILEIRA DA FOLHA DE FONTES: os mesmos logos, TINGIDOS por GFX_MARCA (a
// arte e branca e a forma mora no alfa, ver acima) e com folga menor. A folha
// os usa como a linha de especificacao, abaixo do titulo, entao eles entram
// num cinza da hierarquia do texto e nao no branco cheio de selo de cartaz.
// Selo sem arte cai no nome, na mesma tinta.
float badges_desenhar_tom(uint64_t mask,float x,float y,float maxW,float h,float r,float g,float b,float a) {
  float start=x;int c=(int)(r*255.0f+.5f);
  for(size_t i=0;i<NB;i++)if(mask&(UINT64_C(1)<<i)) {
    GLuint t=art[i].image[0]?tex_obter_larg(art[i].image,128):0;
    float aspect=art[i].image[0]?tex_aspecto(art[i].image):0,w;
    if(t&&aspect>0){w=h*aspect;if(w>160)w=160;if(x+w>start+maxW)break;
      float height=w/aspect;gfx_rect((GfxRect){x,y+(h-height)*.5f,w,height},t,GFX_MARCA,0,0,0,0,r,g,b,a);}
    else{TxtLinha l=txt_linha(TXT_PG_FIM,art[i].name[0]?art[i].name:ids[i],c,c,c,255);w=(float)l.w;if(x+w>start+maxW)break;
      txt_desenhar_alpha(l,x,y+(h-l.h)*.5f,a);}
    x+=w+18;
  }
  return x>start?x-start-18:0;
}

// --- SELOS COLORIDOS (#198, 01/10/2026) --------------------------------------
//
// Pedido da issue: "colorido como no Nuvio oficial". O oficial NAO tem cor
// propria: NuvioTV (StreamBadgeChips.kt) e NuvioTVSmart
// (streamScreenHelpers-03-render-image-badge-chip.js) desenham a imagem do
// pacote de selos que a pessoa importa, com tagColor/borderColor do proprio
// pacote; sem pacote nao ha selo nenhum (rules.imports = [] de fabrica). O
// colorido que a pessoa ve la vem do pacote.
//
// A PALETA E A DO PACOTE INICIAL DA WIKI DO NUVIO (NuvioBadgeEditor.vue,
// starterBadge): uma cor por GRUPO, preenchimento da cor a 20 % (#33) e borda
// na cor cheia, arte branca. Os grupos que o pacote inicial nao tem saem do
// NardBadges v3.5 (o pacote completo que a wiki usa de base de imagens):
// codec no roxo dele e cada servico na cor da marca.
//
//   resolucao  #FFBE01   origem   #27C04F   video (HDR/DV/IMAX)  #FF6B6B
//   audio      #45B7D1   canais   #FFD700   codec                #9C27B0
//
// "Atmos + Vision" e afins ficam no grupo de VIDEO (como no NardBadges): o DV
// e o que pesa na escolha.
static void corSelo(size_t i, float *r, float *g, float *b) {
  static const struct { const char *id; unsigned cor; } MARCAS[] = {
    {"p-netflix",0xE50914},{"p-prime",0x00A8E1},{"p-appletv",0xA2AAAD},
    {"p-disney",0x113CCF},{"p-max",0xB100FF},{"p-hulu",0x1CE783},
    {"p-peacock",0xFFC72C},{"p-paramount",0x0064FF},{"p-crunchyroll",0xF47521},
  };
  const char *id = ids[i];
  unsigned c = 0x858283;   // neutro do NardBadges (p-crave nao esta no pacote)
  size_t k;
  if (!strncmp(id, "r-", 2)) c = 0xFFBE01;
  else if (!strncmp(id, "q-", 2)) c = 0x27C04F;
  else if (!strncmp(id, "v-", 2)) c = 0xFF6B6B;
  else if (!strncmp(id, "a-", 2)) c = strstr(id, "-dv") ? 0xFF6B6B : 0x45B7D1;
  else if (!strncmp(id, "c-", 2)) c = 0xFFD700;
  else if (!strncmp(id, "co-", 3)) c = 0x9C27B0;
  else for (k = 0; k < sizeof MARCAS / sizeof *MARCAS; k++)
    if (!strcmp(id, MARCAS[k].id)) { c = MARCAS[k].cor; break; }
  *r = (float)((c >> 16) & 255) / 255.0f;
  *g = (float)((c >> 8) & 255) / 255.0f;
  *b = (float)(c & 255) / 255.0f;
}
// Luminancia relativa (WCAG) sem a curva exata: o quadrado basta para decidir
// se a borda some no fundo escuro.
static float luz(float r, float g, float b) {
  return 0.2126f * r * r + 0.7152f * g * g + 0.0722f * b * b;
}
int badges_cor_selo(const char *id, float *r, float *g, float *b) {
  size_t i;
  for (i = 0; i < NB; i++) if (!strcmp(ids[i], id)) { corSelo(i, r, g, b); return 1; }
  return 0;
}
// CADA SELO E UMA PECA: base escura, tinta do grupo a 20 % por cima, borda de
// 2 px na cor do grupo e a arte branca dentro. A BASE ESCURA e o que faz o
// selo ser o MESMO em todo lugar — linha em repouso, linha em foco (que e
// clara: cor de realce cheia) e vidro (veu translucido): a tinta nunca cai
// direto sobre o fundo, entao 20 % de amarelo nao vira bege na linha branca.
//
// BORDA COM PISO DE LUZ: azul do Disney (#113CCF) sobre a base escura da
// 2:1 e some a 3 m. Abaixo de ~0,12 de luminancia a borda clareia para o
// branco ate chegar la (~3:1, o minimo de contorno de componente); a tinta
// segue a cor da marca. A legibilidade e da arte branca sobre a base escura.
float badges_desenhar_selos(uint64_t mask, float x, float y, float maxW, float h, float a) {
  // Miolo de 6 px dos lados e 3 em cima/baixo, canto de 6 px. Borda de 1,5 px
  // a 60 % e tinta a 16 % (01/10, folha "muito bruta"): o selo continua
  // colorido, so deixa de gritar mais alto que o nome da fonte.
  const float padX = 6.0f, sobra = 3.0f, ch = h + sobra * 2.0f, raio = 6.0f / ch;
  float start = x;
  size_t i;
  for (i = 0; i < NB; i++) if ((mask & (UINT64_C(1) << i)) && art[i].image[0]) {
    GLuint t = tex_obter_larg(art[i].image, 128);
    float aspect = tex_aspecto(art[i].image), w = aspect > 0 ? h * aspect : 80;
    float r, g, b, br, bg, bb, l;
    GfxRect p;
    TxtLinha nome = { 0 };
    int temArte = t && aspect > 0;
    if (w > 144) w = 144;
    if (!temArte) {
      nome = txt_linha_corta(TXT_MINI, art[i].name, 235, 235, 238, 255, 144);
      w = (float)nome.w;
    }
    if (x + w + padX * 2.0f > start + maxW) break;
    corSelo(i, &r, &g, &b);
    br = r; bg = g; bb = b;
    l = luz(r, g, b);
    if (l < 0.12f) {
      // Mistura com o branco ate o piso: resolve k em luz(c + (1-c)k) ~ 0,12
      // por passos curtos (sao no maximo uns 8 por selo e so os escuros).
      float k = 0.0f;
      while (k < 1.0f && luz(br, bg, bb) < 0.12f) {
        k += 0.08f;
        br = r + (1.0f - r) * k; bg = g + (1.0f - g) * k; bb = b + (1.0f - b) * k;
      }
    }
    p = (GfxRect){ x, y - sobra, w + padX * 2.0f, ch };
    if (ajustes_vidro()) gfx_vidro_chip(p, raio, 0.88f, a);
    else gfx_cor(p, raio, 0.10f, 0.11f, 0.13f, 0.88f * a);
    gfx_cor(p, raio, r, g, b, 0.16f * a);
    gfx_anel(p, raio, 1.5f, br, bg, bb, 0.60f * a);
    if (temArte) {
      float height = w / aspect;
      gfx_rect((GfxRect){ x + padX, y + (h - height) * 0.5f, w, height }, t, GFX_TEXTO,
               0, 0, 0, 0, 1, 1, 1, a);
    } else txt_desenhar_alpha(nome, x + padX, y + (h - (float)nome.h) * 0.5f, a);
    x += p.w + 8.0f;
  }
  return x > start ? x - start - 8.0f : 0.0f;
}

// ROTULO COM A PALAVRA DO FORMATO TROCADA PELA MARCA ("Sem HDR" -> "Sem" +
// logo). O rotulo e traduzido ANTES de procurar a palavra: "Ohne HDR", "Без HDR"
// e "No HDR" saem do mesmo caminho, sem cravar a lingua. Sem a palavra na
// traducao, ou sem espaco, cai no texto puro.
static int partesRotulo(const char *rotulo, FormatoMarca f, char *ante, size_t na,
                        char *depois, size_t nd) {
  const char *t = i18n(rotulo), *nome = marca_formato_nome(f), *p;
  size_t ln = strlen(nome);
  ante[0] = depois[0] = 0;
  if (!ln || !(p = strstr(t, nome))) return 0;
  snprintf(ante, na, "%.*s", (int)(p - t), t);
  snprintf(depois, nd, "%s", p + ln);
  // Os espacos ao redor da palavra viram o respiro da marca (6 px).
  { size_t k = strlen(ante); while (k && ante[k - 1] == ' ') ante[--k] = 0; }
  { char *q = depois; while (*q == ' ') q++; memmove(depois, q, strlen(q) + 1); }
  return 1;
}
float marca_rotulo_largura(TxtEstilo est, const char *rotulo, FormatoMarca f, float altura) {
  char a[96], d[96];
  float w = 0.0f;
  if (!partesRotulo(rotulo, f, a, sizeof a, d, sizeof d))
    return (float)txt_linha(est, rotulo, 0, 0, 0, 255).w;
  if (a[0]) w += (float)txt_linha(est, a, 0, 0, 0, 255).w + 6.0f;
  w += marca_formato_largura(f, altura);
  if (d[0]) w += (float)txt_linha(est, d, 0, 0, 0, 255).w + 6.0f;
  return w;
}
float marca_rotulo(TxtEstilo est, const char *rotulo, FormatoMarca f, float x, float y,
                   float altura, int tinta, float a) {
  char an[96], de[96];
  float k = (float)tinta / 255.0f, x0 = x;
  if (!partesRotulo(rotulo, f, an, sizeof an, de, sizeof de)) {
    TxtLinha l = txt_linha(est, rotulo, tinta, tinta, tinta, 255);
    txt_desenhar_alpha(l, x, y, a);
    return (float)l.w;
  }
  {
    TxtLinha ref = txt_linha(est, an[0] ? an : de[0] ? de : "H", tinta, tinta, tinta, 255);
    float cy = y + (float)ref.h * 0.5f;
    if (an[0]) { txt_desenhar_alpha(ref, x, y, a); x += (float)ref.w + 6.0f; }
    x += marca_formato(f, x, cy - altura * 0.5f, altura, k, k, k, a);
    if (de[0]) {
      TxtLinha l = txt_linha(est, de, tinta, tinta, tinta, 255);
      x += 6.0f;
      txt_desenhar_alpha(l, x, y, a);
      x += (float)l.w;
    }
  }
  return x - x0;
}

// --- SELOS DE TEXTO ----------------------------------------------------------
float badge_largura(const char *texto) {
  TxtLinha l = txt_linha(TXT_CAPTION2, texto, 0, 0, 0, 255);
  return (float)l.w + BADGE_PADX * 2.0f;
}
float badge_desenhar(float x, float y, const char *texto, BadgeEstilo estilo, float a) {
  float fr = 0.10f, fg = 0.11f, fb = 0.13f, fa = 0.85f;
  int c = 235;
  TxtLinha l;
  GfxRect p;
  switch (estilo) {
    case BADGE_APAGADO: c = 170; break;
    case BADGE_REALCE:
      // 18 % da cor de realce sobre o painel escuro: le como "aceso" sem
      // disputar com a linha em foco, que e realce cheio.
      ajustes_acento(&fr, &fg, &fb); fa = 0.18f; break;
    case BADGE_REALCE_CHEIO:
      c = (int)(ajustes_acento_tinta(&fr, &fg, &fb) * 255.0f + 0.5f); fa = 1.0f; break;
    case BADGE_SOBRE_REALCE: {
      float t = ajustes_acento_tinta(NULL, NULL, NULL);
      fr = fg = fb = t; fa = 0.12f; c = ajustes_tinta_foco2(); break; }
    default: break;
  }
  l = txt_linha(TXT_CAPTION2, texto, c, c, c, 255);
  p.x = x; p.y = y; p.w = (float)l.w + BADGE_PADX * 2.0f; p.h = BADGE_H;
  gfx_cor(p, 0.5f, fr, fg, fb, fa * a);
  txt_desenhar_alpha(l, x + BADGE_PADX, y + (BADGE_H - (float)l.h) * 0.5f, a);
  return p.w;
}
static void notaTexto(char *dst, size_t n, int nota) {
  // Separador decimal pelo idioma: "8,4" em portugues, "8.4" em ingles — em
  // ingles a virgula le como milhar interrompido (ver recomenda.c).
  snprintf(dst, n, idioma_ponto_decimal(ajustes_idioma()) ? "%d.%d" : "%d,%d", nota / 10, nota % 10);
}
static float imdbMarcaLargura(void) {
  TxtLinha l = txt_linha(TXT_MINI, "IMDb", 10, 10, 10, 255);
  float w = (float)l.w + 16.0f;
  return w > BADGE_IMDB_W ? w : BADGE_IMDB_W;
}
float badge_imdb_largura(int nota) {
  char t[8]; TxtLinha l;
  if (nota <= 0) return 0.0f;
  notaTexto(t, sizeof t, nota);
  l = txt_linha(TXT_CAPTION2, t, 0, 0, 0, 255);
  return imdbMarcaLargura() + BADGE_IMDB_GAP + (float)l.w;
}
float badge_imdb(float x, float y, int nota, int escuro, float a) {
  char t[8]; TxtLinha l, lm; GfxRect m;
  int c = escuro ? ajustes_tinta_foco() : 235;
  if (nota <= 0) return 0.0f;
  notaTexto(t, sizeof t, nota);
  l = txt_linha(TXT_CAPTION2, t, c, c, c, 255);
  m.x = x; m.y = y; m.w = imdbMarcaLargura(); m.h = BADGE_H;
  // #F5C518 e o amarelo da marca; o texto e preto porque e assim que ela
  // existe — nao segue o realce nem a tinta da linha.
  gfx_cor(m, BADGE_IMDB_R / BADGE_H, 0.961f, 0.773f, 0.094f, a);
  lm = txt_linha(TXT_MINI, "IMDb", 10, 10, 10, 255);
  txt_desenhar_alpha(lm, m.x + (m.w - (float)lm.w) * 0.5f, m.y + (m.h - (float)lm.h) * 0.5f, a);
  txt_desenhar_alpha(l, x + m.w + BADGE_IMDB_GAP, y + (BADGE_H - (float)l.h) * 0.5f, a);
  return m.w + BADGE_IMDB_GAP + (float)l.w;
}

// CHECK DE ASSISTIDO (U4, 205): UM desenhador para todos os estilos de cartao.
// Disco no acento com o check da tinta de contraste e sombra rasa. `d` e o
// diametro; (cx,cy) o centro. Os estilos so mudam ONDE ele cai.
void badges_check_disco(float cx, float cy, float d, float a) {
  float ar, ag, ab, tinta, ic = d * 0.60f;
  tinta = ajustes_acento_tinta(&ar, &ag, &ab);
  gfx_rect((GfxRect){ cx - d * 0.5f - 1, cy - d * 0.5f + 2, d + 2, d + 2 }, 0, GFX_DISCO, 0, 0, 0, 0,
           0, 0, 0, 0.30f * a);
  gfx_rect((GfxRect){ cx - d * 0.5f - 2, cy - d * 0.5f - 2, d + 4, d + 4 }, 0, GFX_DISCO, 0, 0, 0, 0,
           0.05f, 0.055f, 0.067f, 0.85f * a);
  gfx_rect((GfxRect){ cx - d * 0.5f, cy - d * 0.5f, d, d }, 0, GFX_DISCO, 0, 0, 0, 0, ar, ag, ab, a);
  gfx_icone((GfxRect){ cx - ic * 0.5f, cy - ic * 0.5f, ic, ic }, "check", tinta, tinta, tinta, a);
}

// Diametro relativo ao cartao (26..40).
static float checkDiametro(float w) {
  float d = w * 0.13f;
  return d < 26.0f ? 26.0f : d > 40.0f ? 40.0f : d;
}

// SELO ESCRITO (opcao "Escrito"): a pilula de antes do check unico (a8c59a84),
// intacta: escura translucida, check no acento e a palavra "Assistido". Altura
// e recuo acompanham a largura do cartao (26..32); abaixo de 150 so o check.
// (mx,my) e o canto superior esquerdo; devolve a borda esquerda.
static float seloEscritoEm(float mx, float my, float w, float a, int ancora_dir, float *largura) {
  float h = w * 0.13f, ic, pw, ar, ag, ab;
  int compacto = w < 150.0f;
  TxtLinha t = { 0 };
  if (h < 26.0f) h = 26.0f;
  if (h > 32.0f) h = 32.0f;
  ic = h * 0.58f;
  if (!compacto) t = txt_linha(TXT_CAPTION, i18n("Assistido"), 240, 240, 245, 255);
  pw = compacto ? h : h * 0.36f + ic + 6.0f + (float)t.w + h * 0.46f;
  if (largura) *largura = pw;
  if (ancora_dir) mx -= pw;
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor((GfxRect){ mx, my + 3.0f, pw, h }, 0.5f, 0, 0, 0, 0.28f * a);
  gfx_vidro_ou_solido((GfxRect){ mx, my, pw, h }, 0.5f, GFX_VIDRO_DENS, a, 0.05f, 0.055f, 0.067f, 0.80f);
  { float ix = compacto ? mx + (h - ic) * 0.5f : mx + h * 0.36f;
    gfx_icone((GfxRect){ ix, my + (h - ic) * 0.5f, ic, ic }, "check", ar, ag, ab, a);
    if (!compacto) txt_desenhar_alpha(t, ix + ic + 6.0f, my + (h - t.h) * 0.5f, a); }
  return mx;
}

// Check no cartao `card` (retangulo do cartaz/disco). redondo: a 45 graus no
// canto inferior direito da circunferencia; senao: canto superior direito, no
// raio seguro. Devolve a borda esquerda do selo.
float badges_check_cartao(GfxRect card, int redondo, float a) {
  float d = checkDiametro(card.w), m = card.w * 0.06f;
  float cx, cy;
  if (m < 8.0f) m = 8.0f;
  if (m > 14.0f) m = 14.0f;
  if (ajustes_selo_visto_modo() == 2) {
    if (redondo) {   // pilula ancorada a direita no mesmo ponto a 45 graus
      float r = card.w * 0.5f, pw;
      cy = card.y + r + r * 0.7071f - d * 0.1f;
      return seloEscritoEm(card.x + r + r * 0.7071f + d * 0.4f, cy - d * 0.5f, card.w, a, 1, &pw);
    }
    return seloEscritoEm(card.x + card.w - m, card.y + m, card.w, a, 1, NULL);
  }
  if (redondo) {
    float r = card.w * 0.5f;
    cx = card.x + r + r * 0.7071f - d * 0.1f;
    cy = card.y + r + r * 0.7071f - d * 0.1f;
  } else {
    cx = card.x + card.w - m - d * 0.5f;
    cy = card.y + m + d * 0.5f;
  }
  badges_check_disco(cx, cy, d, a);
  return cx - d * 0.5f;
}

// Barra fina de "Parou em": rente a base do cartao, trilha escura + acento.
void badges_progresso(GfxRect card, float frac, float a) {
  float ar, ag, ab, h = 5.0f, m = card.w * 0.06f, w;
  if (frac <= 0.0f) return;
  if (frac > 1.0f) frac = 1.0f;
  if (m < 8.0f) m = 8.0f;
  if (m > 14.0f) m = 14.0f;
  w = card.w - m * 2;
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor((GfxRect){ card.x + m, card.y + card.h - m - h, w, h }, 0.5f, 0, 0, 0, 0.55f * a);
  gfx_cor((GfxRect){ card.x + m, card.y + card.h - m - h, w * frac < h ? h : w * frac, h }, 0.5f, ar, ag, ab, a);
}

// SELO DE ASSISTIDO do cartaz (Home e Biblioteca): o mesmo check. Devolve a
// borda esquerda dele (o chip de amigos desvia).
float badges_selo_visto(float px, float py, float w, float a) {
  return badges_check_cartao((GfxRect){ px, py, w, w * 1.5f }, 0, a);
}
