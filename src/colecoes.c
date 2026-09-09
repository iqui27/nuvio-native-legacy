#include "colecoes.h"
#include "rede.h"
#include "js.h"
#include "addons.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
static ColFolder folders[COL_MAX];
static int count;
static void localiza(char *value,size_t cap,const char *dir) {
  if(!value[0]||strstr(value,"://")||value[0]=='/')return;
  char rel[600];snprintf(rel,sizeof rel,"%s",value);snprintf(value,cap,"%s/%s",dir,rel);
}
int col_n(void) { return count; }
// Fonte da conta vem com addonId e sem URL; a URL so existe depois que a sonda
// leu o manifesto daquele addon. Resolver no acesso deixa a pasta pronta assim
// que a sonda passar, sem ninguem precisar avisar.
static void resolverBases(ColFolder *v) {
  for (int s = 0; s < v->nSources; s++)
    if (!v->sources[s].base[0] && v->sources[s].addonId[0])
      snprintf(v->sources[s].base, sizeof v->sources[s].base, "%s", addons_base_por_id(v->sources[s].addonId));
}
const ColFolder *col_folder(int i) {
  if (i < 0 || i >= count) return NULL;
  resolverBases(&folders[i]);
  return &folders[i];
}
int col_grupo(const char *name,int *indices,int max) {
  int n=0;for(int i=0;i<count&&n<max;i++) if(!strcasecmp(name,folders[i].group)) indices[n++]=i;return n;
}
// RESOLVE A BASE ANTES DE COMPARAR, e isso e o conserto de verdade do #18.
//
// Era o unico leitor de `folders[]` que NAO passava por col_folder(), e por
// isso o unico que via a fonte crua. Uma fonte da CONTA chega com `addonId` e
// SEM URL (ver lerColecaoWeb), e a URL so existe depois que alguem le o
// manifesto daquele addon. A sequencia esta medida na C9 e anotada em
// descoberta.c (geracaoPedida): desc_iniciar() em 0,9 s, o sync aplica o que
// veio da conta em ~2 s, e os manifestos so sao lidos em ~7 s — colecoes e
// addons chegam no MESMO bloco de sync_passo, entao a colecao e sempre guardada
// antes de qualquer manifesto ter sido lido. Ou seja: no instante em que
// col_definir_json guarda a fonte, addons_base_por_id ainda devolve "" — a
// fonte fica com base VAZIA e esta funcao nunca casava com a base real que a
// descoberta lhe passa.
//
// Resultado: o catalogo que esta dentro de uma colecao nao era reconhecido como
// tal e virava fileira solta na home, exatamente o que o relator do #18 continua
// vendo depois do v1.0.11. O conserto daquela versao (pular o catalogo que ja
// aparece numa pasta) estava certo e simplesmente nunca disparava para quem tem
// colecao da CONTA — que no Tizen e o unico caminho possivel, porque o
// collections.json nao vai no .wgt (ver #10). No aparelho de quem consertou as
// pastas vinham do pacote, com `base` escrita no arquivo, e por isso funcionava.
//
// O unico caminho que resolvia a base era col_folder(), chamado do DESENHO da
// home — uma corrida contra o fio da descoberta, o que explica "as vezes".
//
// CUSTO, medido e nao suposto (tests/colcusto.c no Mac M-series, -O1, 32 pastas
// x 8 fontes = 256 fontes, 1.000.000 de chamadas no PIOR caso, em que nada casa
// e as duas varreduras vao ate o fim): 2048 e 2053 ms antes, 2101 e 2116 ms
// depois. Sao ~53 ns a mais por chamada, 2,6%, para 256 fontes — resolverBases
// vira um teste de `base[0]` por fonte e nao chama addons_base_por_id nenhuma
// vez depois que a base entrou. A montagem chama isto uma vez por catalogo
// declarado: com os 605 do Xperience sao ~32 us a mais no ciclo inteiro.
// QUANTAS FONTES DE COLECAO AINDA NAO TEM BASE. Enquanto a base e "", a fonte
// nao casa com nada e o catalogo dela vira fileira solta (#18). A base so
// aparece depois que o manifesto do addon e lido — na LG, 14 s depois do
// arranque. Este numero e o que separa "a colecao nao chegou" de "a colecao
// chegou mas ainda nao da para reconhece-la", que produzem o MESMO sintoma.
int col_fontes_sem_base(void) {
  int i, s, n = 0;
  for (i = 0; i < count; i++) {
    resolverBases(&folders[i]);
    for (s = 0; s < folders[i].nSources; s++)
      if (!folders[i].sources[s].base[0]) n++;
  }
  return n;
}

// O OUTRO LADO DA COMPARACAO. Base REDIGIDA: a fonte do Xperience carrega um
// JWT no caminho e este log vai para relato de defeito.
void col_despejar_fontes(int max) {
  char seg[120];
  int i, s2, n = 0;
  for (i = 0; i < count && n < max; i++) {
    resolverBases(&folders[i]);
    for (s2 = 0; s2 < folders[i].nSources && n < max; s2++, n++) {
      const ColSource *v = &folders[i].sources[s2];
      printf("[col]   fonte[%s/%s]: base=%s tipo=%s id=%s\n",
             folders[i].group, folders[i].title,
             rede_url_publica(v->base, seg, sizeof seg), v->type, v->catId);
    }
  }
}

const ColFolder *col_por_catalogo(const char *base,const char *type,const char *id) {
  // Base vazia nao pergunta nada: sem esta guarda uma consulta sem URL casava
  // com QUALQUER fonte cuja base ainda estivesse vazia — um falso positivo que
  // esconderia a fileira errada.
  if(!base||!base[0]||!type||!id) return NULL;
  for(int i=0;i<count;i++) { resolverBases(&folders[i]);
    for(int s=0;s<folders[i].nSources;s++) {
      const ColSource *v=&folders[i].sources[s];
      if(!strcmp(v->base,base)&&!strcmp(v->type,type)&&!strcmp(v->catId,id)) return &folders[i];
    } }return NULL;
}
/* Arte editorial: JPEG primeiro, PNG depois.
 *
 * O gerador escrevia PNG 4K e os heros somavam 142 MB — 45% do pacote inteiro,
 * para imagens SEM canal alfa (colortype 2, conferido nos arquivos), ou seja
 * pagando o preco do PNG sem usar nada do que ele oferece. Em JPEG a mesma arte
 * cabe numa fracao disso e a TV nao ve diferenca.
 *
 * A ordem importa e o PNG FICA como reserva: quem ja tem o pacote antigo
 * instalado, ou quem regerar a arte com a ferramenta antiga, continua com a
 * pagina ilustrada em vez de cair no fundo chapado. */
static int arteEditorial(char *saida,size_t n,const char *dir,const char *sub,
                         const char *id,const char *sufixo) {
  snprintf(saida,n,"%s/%s/%s-%s.jpg",dir,sub,id,sufixo);
  if(!access(saida,R_OK)) return 1;
  snprintf(saida,n,"%s/%s/%s-%s.png",dir,sub,id,sufixo);
  return !access(saida,R_OK);
}

int col_carregar(const char *dir) {
  char path[700];snprintf(path,sizeof path,"%s/collections.json",dir);
  FILE *f=fopen(path,"rb");if(!f)return 0;
  fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
  if(size<2||size>4000000){fclose(f);return 0;}
  char *body=malloc((size_t)size+1);if(!body){fclose(f);return 0;}
  size_t got=fread(body,1,(size_t)size,f);body[got]=0;fclose(f);count=0;
  for(const char *g=js_array(body,NULL,"groups");g;g=js_prox(js_fim(g))) {
    const char *end=js_fim(g);char group[64],groupId[64]="";js_texto(g,end,"title",group,sizeof group);js_texto(g,end,"id",groupId,sizeof groupId);
    for(const char *p=js_array(g,end,"folders");p&&count<COL_MAX;p=js_prox(js_fim(p))) {
      const char *pe=js_fim(p);ColFolder *v=&folders[count];memset(v,0,sizeof *v);
      snprintf(v->group,sizeof v->group,"%s",group);snprintf(v->groupId,sizeof v->groupId,"%s",groupId);
      js_texto(p,pe,"id",v->id,sizeof v->id);js_texto(p,pe,"title",v->title,sizeof v->title);
      js_texto(p,pe,"cover",v->cover,sizeof v->cover);js_texto(p,pe,"hero",v->hero,sizeof v->hero);js_texto(p,pe,"logo",v->logo,sizeof v->logo);
      localiza(v->cover,sizeof v->cover,dir);localiza(v->hero,sizeof v->hero,dir);localiza(v->logo,sizeof v->logo,dir);
      v->hideTitle=js_num(p,pe,"hideTitle",0);v->frames=js_num(p,pe,"frames",0);
      if(v->frames<0||v->frames>90)v->frames=0;
      snprintf(v->frameDir,sizeof v->frameDir,"%s/collections/%s",dir,v->id);
      /* Local paired artwork survives catalog imports. Activate only a complete pair. */
      char editorial[512];
      if(arteEditorial(editorial,sizeof editorial,dir,"editorial",v->id,"home")&&
         arteEditorial(v->detailHero,sizeof v->detailHero,dir,"editorial",v->id,"detail")) {
        snprintf(v->hero,sizeof v->hero,"%s",editorial);v->editorial=1;
      } else v->detailHero[0]=0;
      char cinematic[512],cinematicDetail[512];
      if(arteEditorial(cinematic,sizeof cinematic,dir,"cinematic",v->id,"home")&&
         arteEditorial(cinematicDetail,sizeof cinematicDetail,dir,"cinematic",v->id,"detail")) {
        snprintf(v->hero,sizeof v->hero,"%s",cinematic);
        snprintf(v->detailHero,sizeof v->detailHero,"%s",cinematicDetail);
        v->editorial=2;
      }
      for(const char *s=js_array(p,pe,"sources");s&&v->nSources<COL_SOURCE_MAX;s=js_prox(js_fim(s))) {
        const char *se=js_fim(s);ColSource *a=&v->sources[v->nSources];
        js_texto(s,se,"title",a->title,sizeof a->title);js_texto(s,se,"base",a->base,sizeof a->base);
        js_texto(s,se,"type",a->type,sizeof a->type);js_texto(s,se,"catId",a->catId,sizeof a->catId);js_texto(s,se,"genre",a->genre,sizeof a->genre);
        if(a->base[0]&&a->type[0]&&a->catId[0])v->nSources++;
      }
      v->local=1;
      if(v->nSources&&v->title[0])count++;
    }
  }free(body);return count;
}
void col_cor(const ColFolder *f,float *r,float *g,float *b) {
  *r=.16f;*g=.23f;*b=.30f;if(!f)return;
  if(strstr(f->title,"Netflix")){*r=.52f;*g=.035f;*b=.065f;}
  else if(strstr(f->title,"Prime")){*r=.025f;*g=.32f;*b=.58f;}
  else if(strstr(f->title,"Disney")){*r=.10f;*g=.13f;*b=.46f;}
  else if(strstr(f->title,"Max")||strstr(f->title,"HBO")){*r=.27f;*g=.12f;*b=.44f;}
  else if(strstr(f->title,"Letterboxd")){*r=.07f;*g=.32f;*b=.21f;}
  else if(!strcmp(f->group,"Awards")){*r=.40f;*g=.31f;*b=.095f;}
  else if(!strcmp(f->group,"Directors")){*r=.29f;*g=.24f;*b=.19f;}
}

// ---------------------------------------------------------------- conta

static void tirarManifest(char *base) {
  size_t k = strlen(base);
  if (k > 14 && !strcmp(base + k - 14, "/manifest.json")) base[k - 14] = 0;
  else while (k && base[k - 1] == '/') base[--k] = 0;
}

// Uma colecao do web -> N pastas em `folders`. Mesma traducao de
// tools/import-collections.mjs, sem baixar arte: cover/hero/logo ficam como URL
// e tex_cache baixa quando desenhar.
// POR QUE UMA PASTA SOME INTEIRA, contado em vez de silencioso.
//
// Uma pasta so entra com pelo menos UMA fonte utilizavel e um titulo. As
// descartadas nao deixavam rastro nenhum: quem instalava uma colecao na conta e
// nao a via na TV nao tinha como saber se ela nao chegou, se chegou vazia, ou se
// foi recusada aqui — e os tres tem conserto diferente. E o issue #13.
static int fPulProvedor, fPulSemFonte, fPulSemTitulo, fPulCheio;
static char fPrimeiraPulada[128];

static void lerColecaoWeb(const char *c, const char *ce) {
  char group[64], groupId[64], fundo[512];
  js_texto(c, ce, "title", group, sizeof group);
  js_texto(c, ce, "id", groupId, sizeof groupId);
  js_texto(c, ce, "backdropImageUrl", fundo, sizeof fundo);
  if (!group[0]) return;
  for (const char *p = js_array(c, ce, "folders"); p && count < COL_MAX; p = js_prox(js_fim(p))) {
    const char *pe = js_fim(p); ColFolder *v = &folders[count]; memset(v, 0, sizeof *v);
    snprintf(v->group, sizeof v->group, "%s", group);
    snprintf(v->groupId, sizeof v->groupId, "%s", groupId);
    js_texto(p, pe, "id", v->id, sizeof v->id); js_texto(p, pe, "title", v->title, sizeof v->title);
    js_texto(p, pe, "coverImageUrl", v->cover, sizeof v->cover);
    if (!js_texto(p, pe, "heroBackdropUrl", v->hero, sizeof v->hero)) snprintf(v->hero, sizeof v->hero, "%s", fundo);
    js_texto(p, pe, "titleLogoUrl", v->logo, sizeof v->logo);
    { char b[8]; v->hideTitle = js_bruto(p, pe, "hideTitle", b, sizeof b) && strstr(b, "true") ? 1 : 0; }
    const char *src = js_array(p, pe, "sources");
    if (!src) src = js_array(p, pe, "catalogSources");
    for (const char *s = src; s && v->nSources < COL_SOURCE_MAX; s = js_prox(js_fim(s))) {
      const char *se = js_fim(s); ColSource *a = &v->sources[v->nSources]; char prov[16] = "";
      memset(a, 0, sizeof *a);
      js_texto(s, se, "provider", prov, sizeof prov);
      // tmdb/trakt como fonte de pasta nao tem equivalente aqui: so addon.
      // tmdb/trakt como fonte de pasta nao tem equivalente aqui: so addon.
      if (prov[0] && strcasecmp(prov, "addon")) { fPulProvedor++; continue; }
      if (!js_texto(s, se, "addonBaseUrl", a->base, sizeof a->base)) js_texto(s, se, "addon_base_url", a->base, sizeof a->base);
      tirarManifest(a->base);
      js_texto(s, se, "addonId", a->addonId, sizeof a->addonId);
      if (!a->base[0]) snprintf(a->base, sizeof a->base, "%s", addons_base_por_id(a->addonId));
      js_texto(s, se, "type", a->type, sizeof a->type);
      if (!js_texto(s, se, "catalogId", a->catId, sizeof a->catId)) js_texto(s, se, "catalog_id", a->catId, sizeof a->catId);
      if (!js_texto(s, se, "title", a->title, sizeof a->title) && !js_texto(s, se, "catalogName", a->title, sizeof a->title))
        snprintf(a->title, sizeof a->title, "%s", a->catId);
      js_texto(s, se, "genre", a->genre, sizeof a->genre);
      if (!strcmp(a->genre, "None")) a->genre[0] = 0;
      // Sem base MAS com addonId entra: a base chega quando a sonda ler o manifesto.
      if ((a->base[0] || a->addonId[0]) && a->type[0] && a->catId[0]) v->nSources++;
    }
    if (v->nSources && v->title[0]) count++;
    else {
      if (!v->nSources) fPulSemFonte++; else fPulSemTitulo++;
      if (!fPrimeiraPulada[0] && v->title[0])
        snprintf(fPrimeiraPulada, sizeof fPrimeiraPulada, "%s", v->title);
    }
  }
}

int col_definir_json(const char *json) {
  char *solto = NULL;
  const char *arr, *fim;
  int antes = count, novas;
  if (!json || !*json) return 0;
  // Linha da RPC: [{collections_json: ...}] ou {collections_json: ...}.
  { const char *linha = *json == '[' ? js_raiz_array(json) : json;
    const char *cj = linha ? strstr(linha, "\"collections_json\"") : NULL;
    if (cj) {
      const char *v = strchr(cj + 18, ':');
      while (v && (*v == ':' || *v == ' ')) v++;
      if (v && *v == '"') {               // string escapada
        size_t n = strlen(v);
        solto = malloc(n + 1);
        if (!solto) return 0;
        if (!js_texto(cj, NULL, "collections_json", solto, (unsigned)n + 1)) { free(solto); return 0; }
        json = solto;
      } else if (v) json = v;
    } }
  fim = json + strlen(json);
  // MEDIDO na conta real: collections_json e o ARRAY direto, nao {collections}.
  // js_raiz_array pula o '[' e para no primeiro elemento, como js_array faz.
  arr = *json == '[' ? js_raiz_array(json) : js_array(json, fim, "collections");
  if (!arr) { free(solto); return 0; }
  // A conta manda o CONJUNTO e a ordem. Mas o pacote traz as mesmas pastas
  // (mesmo id: o collections.json e gerado do perfil do dono) com arte editorial,
  // quadros de animacao e ajustes curados que a conta nao tem — a versao local
  // da pasta e a que fica, com grupo e titulo da conta. Sem isto cada pull
  // trocava a arte curada pela capa crua do CDN.
  ColFolder *antigas = malloc(sizeof(ColFolder) * (size_t)(antes > 0 ? antes : 1));
  if (antigas) memcpy(antigas, folders, sizeof(ColFolder) * (size_t)antes);
  count = 0;
  fPulProvedor = fPulSemFonte = fPulSemTitulo = fPulCheio = 0;
  fPrimeiraPulada[0] = 0;
  { const char *c = arr;
    for (; c && *c == '{'; c = js_prox(js_fim(c))) {
      if (count >= COL_MAX) { fPulCheio++; continue; }
      lerColecaoWeb(c, js_fim(c));
    } }
  novas = count;
  // UMA LINHA QUE RESPONDE "cade a colecao que eu instalei". Cada contagem e um
  // conserto diferente: provedor sem equivalente e falta de recurso, pasta sem
  // fonte e dado incompleto do lado da conta, e teto cheio e limite nosso.
  if (fPulProvedor || fPulSemFonte || fPulSemTitulo || fPulCheio)
    printf("[colecoes] descartadas: %d fonte(s) de provedor nao-addon, "
           "%d pasta(s) sem fonte utilizavel, %d sem titulo, %d alem do teto de "
           "%d%s%s\n",
           fPulProvedor, fPulSemFonte, fPulSemTitulo, fPulCheio, COL_MAX,
           fPrimeiraPulada[0] ? " | primeira: " : "", fPrimeiraPulada);
  if (novas && antigas) {
    int casadas = 0;
    for (int i = 0; i < count; i++) for (int j = 0; j < antes; j++) {
      if (!antigas[j].local || strcmp(antigas[j].id, folders[i].id)) continue;
      ColFolder v = antigas[j];
      snprintf(v.group, sizeof v.group, "%s", folders[i].group);
      snprintf(v.groupId, sizeof v.groupId, "%s", folders[i].groupId);
      snprintf(v.title, sizeof v.title, "%s", folders[i].title);
      folders[i] = v; casadas++; break;
    }
    printf("[colecoes] %d pastas da conta casaram com a arte do pacote\n", casadas);
  }
  free(antigas);
  if (!novas) {
    count = antes;
    printf("[colecoes] conta veio vazia; mantendo as locais (%d) | %u bytes, comeca \"%.60s\", arr=%s\n",
           antes, (unsigned)strlen(json), json, arr ? "sim" : "nao");
  }
  else printf("[colecoes] %d pastas vindas da conta\n", novas);
  free(solto);
  return novas;
}

void col_chave_grupo(const char *group, char *dst, unsigned n) {
  for (int i = 0; i < count; i++)
    if (!strcasecmp(folders[i].group, group) && folders[i].groupId[0]) {
      snprintf(dst, n, "collection_%s", folders[i].groupId); return; }
  snprintf(dst, n, "collection_%s", group);
}
