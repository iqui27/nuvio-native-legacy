#include "streams.h"
#include "badges.h"
#include <pthread.h>
#include "rede.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "addons.h"
#include "marco.h"
#include "debrid.h"
#include "video.h"

#define FOLHA_W       720.0f
#define FOLHA_LINHA   228.0f
#define FOLHA_TOPO    272.0f

static Stream *lista;
static int n = 0;
static int atual = -1, recarregar;
static char contexto[320];
void stream_definir_atual(int i) { atual = i >= 0 && i < n ? i : -1; }
int stream_atual(void) { return atual; }
void stream_folha_contexto(const char *s) { snprintf(contexto, sizeof contexto, "%s", s ? s : ""); }
int stream_folha_recarregar(void) { int r = recarregar; recarregar = 0; return r; }

static int aberta = 0, foco = 0, escolha = -1;
static float anim = 0.0f, rolagem = 0.0f;
// Linha do realce, em unidades de ITEM (2.4 = entre o terceiro e o quarto). O
// realce escorrega entre as linhas em vez de saltar: com o salto seco a folha
// parecia trocar de conteudo a cada tecla, e num D-pad e a continuidade do
// realce que diz "ainda e a mesma lista, voce so andou".


static const char *containerDa(const Stream *s) {
  if (s->mp4 || strstr(s->url, ".mp4") || strstr(s->rotulo, ".mp4")) return "MP4";
  if (strstr(s->url, ".mkv") || strstr(s->arquivo, ".mkv") || strstr(s->descricao, ".mkv")) return "MKV";
  if (strstr(s->url, ".m3u8") || strstr(s->rotulo, "HLS")) return "HLS";
  return "ARQUIVO";
}

static Uint32 recebidaEm;

Uint32 stream_idade_ms(void) {
  return recebidaEm ? SDL_GetTicks() - recebidaEm : 0xFFFFFFFFu;
}

void stream_definir_lista(const Stream *l, int qtd) {
  int i, k = 0;
  recebidaEm = SDL_GetTicks();
  Stream *nova = l && qtd > 0 ? malloc(sizeof(Stream) * (size_t)qtd) : NULL;
  if (l && qtd > 0 && !nova) return;
  // Torrent sem url so fica se ha debrid para resolve-lo; senao seria uma linha
  // que nunca toca (shouldListStream do web).
  for (i = 0; i < qtd && nova; i++)
    if (l[i].url[0] || debrid_ativo()) nova[k++] = l[i];
  if (nova && qtd - k) printf("[fonte] %d torrents sem debrid descartados\n", qtd - k);
  free(lista); lista = nova; n = nova ? k : 0; atual = -1;
  foco = 0;
}

int stream_n(void) {
  return n;
}

const Stream *stream_item(int i) {
  return i >= 0 && i < n ? &lista[i] : NULL;
}

// Pontuacao da regra do dono, do mais forte para o mais fraco:
//   MP4 4K Dolby Vision  >  4K Dolby Vision (qualquer container)
//   >  4K  >  Dolby Vision  >  resolucao  >  ordem de chegada
//
// Somar pesos em vez de comparar campo a campo deixa a regra num lugar so e
// legivel: mudar a preferencia e mexer num numero, nao reescrever um encadeado
// de ifs onde a ordem das comparacoes vira a regra escondida.
static long pontos(const Stream *s) {
  long p = 0;
  // DOLBY VISION SO VALE PONTO EM MP4 — e isto e medida, nao teoria.
  //
  // Marcado no aparelho do dono (LG C9, webOS 4.10) tocando um MKV que o addon
  // anunciava como DV:
  //   hdr do pipeline: HDR10 (fonte DV=1)
  // A TV REBAIXOU para HDR10. E o comportamento ja relatado para Matroska —
  // webOS aciona DV nativo em MP4 e cai para HDR10 em MKV — agora confirmado
  // aqui em vez de citado.
  //
  // O que isso significa na pratica: num perfil 5 a camada base NAO e
  // compativel com HDR10 (e IPT-PQ), entao decodifica-la como HDR10 produz
  // exatamente as cores lavadas que o dono relatou. Preferir a versao DV em
  // MKV era escolher, de proposito, o arquivo que fica PIOR nesta TV.
  //
  // Nao ha como consertar a decodificacao pelo caminho da URI: o Kodi so
  // resolve descartando a camada de realce e reescrevendo o RPU, o que exige
  // demuxar e alimentar o pipeline por buffer — outro projeto, ja registrado em
  // video.c. O que ESTA ao alcance e parar de premiar a fonte que nao serve.
  if (s->mp4 && s->altura >= 2160 && s->dolbyVision) p += 100000;
  if (s->altura >= 2160)                             p +=  20000;
  if (s->mp4 && s->dolbyVision)                      p +=  10000;
  if (s->dolbyAtmos)                                 p +=   2000;
  p += s->altura;
  return p;
}

// Endereco de aviso e nao de conteudo. Estes dois foram MEDIDOS no aparelho:
// o AIOStreams manda para slate.m3u8/slate.mp4 ("This playback link couldn't be
// verified") quando o link expirou, e o Debridio para downloading.mp4 quando o
// arquivo ainda nao esta em cache no Real-Debrid. Os dois sao MP4 validos de
// ~120s que TOCAM NORMALMENTE — nao ha erro para detectar, so o endereco.
static int enderecoDeAviso(const char *u) {
  return strstr(u, "downloading.mp4") || strstr(u, "/slate") ||
         strstr(u, "slate.mp4") || strstr(u, "slate.m3u8") ? 1 : 0;
}

// PLAYLIST HLS SEM UM SEGMENTO SEQUER e fonte MORTA, e ela passava na
// verificacao.
//
// MEDIDO na LG, canal da FrostView que o dono relatou como "nao toca mais": o
// proxy devolve HTTP 200, `application/vnd.apple.mpegurl`, 98 bytes:
//     #EXTM3U
//     #EXT-X-VERSION:3
//     #EXT-X-TARGETDURATION:2
//     #EXT-X-MEDIA-SEQUENCE:0
//     #EXT-X-PLAYLIST-TYPE:LIVE
// Cabecalho e mais nada — nenhum #EXTINF, nenhum .ts, nenhuma variante. A
// origem (praia13.com) responde 522, ou seja o canal caiu e o proxy passou a
// servir um esqueleto. rede_url_final so pergunta "a URL resolve?", e resolve:
// a fonte era marcada OK, ia para o pipeline, e o webOS respondia
// `errorCode 100 "Playing error"` — que e o sintoma sem nenhuma pista.
// Outro canal do mesmo addon devolve 1585 bytes e toca, entao isto separa
// fonte morta de fonte viva no mesmo servidor.
//
// So para .m3u8: o custo e um GET de poucos KB na fonte que ja ia ser usada, e
// so acontece na candidata que chegou ate aqui. MP4 continua julgado pelo
// endereco, como antes.
static int playlistVazia(const char *url) {
  char *corpo;
  long n = 0;
  int vazia;
  if (!strstr(url, ".m3u8") && !strstr(url, "m3u8")) return 0;
  corpo = rede_baixar_bin(url, 8, &n);
  if (!corpo) return 0;    // nao baixou: nao e prova de vazia, deixa passar
  // Um segmento (#EXTINF) ou uma variante (#EXT-X-STREAM-INF) bastam. A lista
  // mestre so tem variantes; a de midia so tem segmentos.
  vazia = !strstr(corpo, "#EXTINF") && !strstr(corpo, "#EXT-X-STREAM-INF");
  if (vazia)
    printf("[fonte] playlist sem segmento (%ld B)\n", n);
  free(corpo);
  return vazia;
}

// VERIFICACAO DAS CANDIDATAS EM PARALELO.
//
// Eram ate 8 rede_url_final EM SERIE, 20 s cada — a segunda metade dos 16,5 s
// medidos entre abrir o titulo e ter fonte. E desperdicio duplo: a maioria das
// tentativas RESOLVE, entao esperar a 1a terminar para so entao comecar a 2a so
// tem valor quando a 1a falha.
//
// A REGRA DE ESCOLHA NAO MUDA: continua sendo "a de maior pontuacao que
// resolve". Os fios verificam as N melhores de uma vez e o resultado e lido NA
// ORDEM DE PONTUACAO, entao a fonte escolhida e exatamente a mesma que a versao
// em serie escolheria — so que sem esperar as anteriores falharem uma a uma.
#define VER_FIOS 4

typedef struct { int idx; int ok; } Verificacao;
static Verificacao *verifs;
static int nVerifs, proxVerif;
static pthread_mutex_t verTrava = PTHREAD_MUTEX_INITIALIZER;

static void *fioVerificar(void *u) {
  (void)u;
  for (;;) {
    int meu, i;
    char fim[900];
    pthread_mutex_lock(&verTrava);
    if (proxVerif >= nVerifs) { pthread_mutex_unlock(&verTrava); return NULL; }
    meu = proxVerif++;
    pthread_mutex_unlock(&verTrava);
    i = verifs[meu].idx;
    if (!lista[i].url[0] && lista[i].infoHash[0]) {
      // Link recem-saido do unrestrict: nao precisa da segunda viagem abaixo.
      if (debrid_resolver(lista[i].infoHash, lista[i].fileIdx, lista[i].url, sizeof lista[i].url))
        verifs[meu].ok = 1;
      else printf("[fonte] %d torrent nao resolveu no debrid\n", i);
      continue;
    }
    if (!lista[i].url[0]) continue;
    // 10 s e nao 20: em paralelo o timeout deixa de ser somado, mas continua
    // sendo o tempo que o dono espera pela mais lenta.
    if (!rede_url_final(lista[i].url, 10, fim, sizeof fim)) {
      printf("[fonte] %d nao resolveu\n", i);
      continue;
    }
    if (enderecoDeAviso(fim)) {
      printf("[fonte] %d e aviso (%.60s)\n", i, fim);
      continue;
    }
    if (playlistVazia(fim)) {
      printf("[fonte] %d tem playlist vazia (canal fora do ar)\n", i);
      continue;
    }
    verifs[meu].ok = 1;
  }
}

int stream_primeira_boa(int tentativas) {
  int *usados, nu = 0;
  int total = stream_n();
  int escolhida = -1;
  if (total < 1) return -1;
  if (tentativas < 1) tentativas = 1;
  if (tentativas > total) tentativas = total;
  usados = calloc((size_t)tentativas, sizeof *usados);
  if (!usados) return -1;

  // Seleciona as `tentativas` melhores, EM ORDEM DE PONTUACAO — a mesma ordem
  // que o laco em serie percorria.
  while (nu < tentativas) {
    int melhor = -1, i, j;
    long maiorP = 0;
    for (i = 0; i < total; i++) {
      int visto = 0;
      for (j = 0; j < nu; j++) if (usados[j] == i) { visto = 1; break; }
      if (visto) continue;
      { long p = pontos(&lista[i]);
        if (melhor < 0 || p > maiorP) { melhor = i; maiorP = p; } }
    }
    if (melhor < 0) break;
    usados[nu++] = melhor;
  }
  if (nu < 1) { free(usados); return -1; }

  marco("fonte: verificacao inicio");
  verifs = calloc((size_t)nu, sizeof(Verificacao));
  if (!verifs) { free(usados); return -1; }
  { int q;
    for (q = 0; q < nu; q++) verifs[q].idx = usados[q];
    nVerifs = nu; proxVerif = 0;
    { pthread_t fios[VER_FIOS];
      int criados = 0;
      for (q = 0; q < VER_FIOS && q < nu; q++)
        if (pthread_create(&fios[criados], NULL, fioVerificar, NULL) == 0) criados++;
      if (!criados) fioVerificar(NULL);   // sem fios: em serie, mesmo resultado
      for (q = 0; q < criados; q++) pthread_join(fios[q], NULL);
    }
    // Primeira que passou, na ordem de pontuacao.
    for (q = 0; q < nu; q++)
      if (verifs[q].ok) { escolhida = verifs[q].idx; break; }
  }
  marco(escolhida >= 0 ? "fonte: verificacao ok" : "fonte: verificacao sem resultado");
  free(verifs); verifs = NULL; nVerifs = 0; free(usados);
  if (escolhida >= 0) printf("[fonte] %d ok\n", escolhida);
  return escolhida;
}

int stream_automatico(void) {
  if (!stream_n()) return -1;
  int melhor = 0;
  long maior = pontos(&lista[0]);
  for (int i = 1; i < n; i++) {
    long p = pontos(&lista[i]);
    // `>` e nao `>=`: em empate fica o PRIMEIRO da lista, que e a ordem em que
    // o addon devolveu — e ele costuma saber algo que a pontuacao nao ve.
    if (p > maior) { maior = p; melhor = i; }
  }
  return melhor;
}


static int grupo, filtro;

// BOTOES DO CABECALHO. "Sem HDR" so existe onde ha o que renegociar (webOS);
// ver o bloco "TELA PRETA COM AUDIO TOCANDO" em video.h. Oferecer um botao que
// nao faz nada seria pior que nao oferecer: a pessoa aperta, nada muda, e passa
// a duvidar dos outros dois.
enum { BT_RECARREGAR, BT_SEM_HDR, BT_FECHAR };
static int botaoDe(int i) {
  if (video_pode_forcar_sdr()) return i;          // 0,1,2
  return i == 0 ? BT_RECARREGAR : BT_FECHAR;      // 0,1
}
static int nBotoes(void) { return video_pode_forcar_sdr() ? 3 : 2; }
static const char *rotuloBotao(int b) {
  return b == BT_RECARREGAR ? "Recarregar" : b == BT_SEM_HDR ? "Sem HDR" : "Fechar";
}
static char provedores[13][96];
static int nProvedores;

static void atualizarProvedores(void) {
  nProvedores = 1;
  snprintf(provedores[0],sizeof provedores[0],"Todos");
  for (int i=0;i<n;i++) {
    int j;
    for(j=1;j<nProvedores;j++) if(!strcmp(provedores[j],lista[i].provedor)) break;
    if(j==nProvedores && nProvedores<13)
      snprintf(provedores[nProvedores++],96,"%s",lista[i].provedor);
  }
  if(filtro>=nProvedores) filtro=0;
}
static int filtrado(int linha) {
  for(int i=0,j=0;i<n;i++)
    if(!filtro || !strcmp(lista[i].provedor,provedores[filtro]))
      if(j++==linha) return i;
  return -1;
}
static int nFiltrados(void) {
  int k=0;
  for(int i=0;i<n;i++) if(!filtro || !strcmp(lista[i].provedor,provedores[filtro])) k++;
  return k;
}
void stream_folha_abrir(void) {
  aberta=1; escolha=-1; foco=0; grupo=1; filtro=0; recarregar=0;
  atualizarProvedores();
  if(atual>=0) foco=atual;
  rolagem=0;
}
int stream_folha_aberta(void) { return aberta; }
void stream_folha_evento(const SDL_Event *e) {
  if(!aberta || e->type!=SDL_KEYDOWN) return;
  SDL_Keycode k=e->key.keysym.sym;
  if(k==SDLK_ESCAPE || k==SDLK_AC_BACK || k==SDLK_BACKSPACE || k==SDLK_DELETE) {aberta=0;return;}
  if(k==SDLK_r) {recarregar=1;return;}
  int nf=nFiltrados();
  if(k==SDLK_UP) {if(grupo==1 && foco>0) foco--; else if(grupo>-1) grupo--;}
  if(k==SDLK_DOWN) {if(grupo<1) grupo++; else if(foco<nf-1) foco++;}
  if(grupo==0 && (k==SDLK_LEFT || k==SDLK_RIGHT)) {
    filtro+=k==SDLK_RIGHT?1:-1;
    if(filtro<0) filtro=0;
    if(filtro>=nProvedores) filtro=nProvedores-1;
    foco=0;rolagem=0;
  }
  if(grupo==-1 && (k==SDLK_LEFT || k==SDLK_RIGHT)) {
    foco+=k==SDLK_RIGHT?1:-1;
    if(foco<0) foco=0;
    if(foco>=nBotoes()) foco=nBotoes()-1;
  }
  if(k==SDLK_RETURN || k==SDLK_KP_ENTER) {
    if(grupo==-1) {
      switch(botaoDe(foco)) {
        case BT_RECARREGAR: recarregar=1; break;
        // Fecha a folha junto: a imagem volta (ou nao) na propria tela do
        // player, e deixar a folha aberta em cima esconderia o resultado.
        case BT_SEM_HDR:    video_forcar_sdr(); aberta=0; break;
        default:            aberta=0; break;
      }
    }
    else if(grupo==0) {grupo=1;foco=0;}
    else {escolha=filtrado(foco);if(escolha>=0) aberta=0;}
  }
}
void stream_folha_atualizar(float dt, Uint32 agora) {
  (void)agora;
  anim=anim_mola(anim,aberta?1:0,dt,NV_MOLA_TELA);
  atualizarProvedores();
  int nf=nFiltrados();
  if(grupo==1 && foco>=nf) foco=nf>0?nf-1:0;
  float area=NV_TELA_H-FOLHA_TOPO-32;
  float max=nf*FOLHA_LINHA-area;
  float alvo=foco*FOLHA_LINHA-(area-FOLHA_LINHA)*.5f;
  if(alvo>max) alvo=max;
  if(alvo<0) alvo=0;
  rolagem=anim_mola(rolagem,alvo,dt,NV_MOLA_SCROLL);
}
int stream_folha_escolheu(int *out) {
  if(escolha<0) return 0;
  if(out) *out=escolha;
  escolha=-1;return 1;
}
void stream_folha_desenhar(Uint32 agora) {
  (void)agora;
  if(anim<.005f) return;
  float x=NV_TELA_W-FOLHA_W+(1-anim)*FOLHA_W;
  gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,.02f,.02f,.025f,.35f*anim);
  gfx_cor((GfxRect){x,0,FOLHA_W,NV_TELA_H},.025f,.095f,.095f,.10f,anim);
  txt_desenhar_alpha(txt_linha(TXT_PAINEL_TITULO,"Fontes",240,241,243,255),x+40,44,anim);
  int nbt=nBotoes();
  for(int i=0;i<nbt;i++) {
    // Ancorado a DIREITA: com dois ou tres botoes a fileira termina sempre no
    // mesmo ponto, 36 px antes da borda do painel.
    float bx=x+FOLHA_W-36-(nbt-i)*128+8;
    int sel=grupo==-1 && foco==i;
    gfx_cor((GfxRect){bx,44,120,50},.3f,sel?.94f:.14f,sel?.94f:.14f,sel?.95f:.15f,anim);
    int c=sel?24:224;
    TxtLinha l=txt_linha(TXT_PG_FIM,rotuloBotao(botaoDe(i)),c,c,c,255);
    txt_desenhar_alpha(l,bx+(120-l.w)*.5f,58,anim);
  }
  // A LINHA DE CONTEXTO EXPLICA O BOTAO EM FOCO. "Sem HDR" nao se explica pelo
  // rotulo, e o rotulo nao pode crescer sem estourar a pilula de 120 px.
  { const char *ajuda=contexto;
    if(grupo==-1 && botaoDe(foco)==BT_SEM_HDR)
      ajuda="Imagem preta com o áudio tocando? Recarrega esta fonte sem HDR nem Dolby Vision.";
    else if(grupo==-1 && botaoDe(foco)==BT_RECARREGAR)
      ajuda="Pergunta as fontes de novo a todos os addons.";
    txt_desenhar_alpha(txt_linha_corta(TXT_PG_FIM,ajuda,184,187,193,255,FOLHA_W-80),x+40,126,anim); }
  gfx_recorte(x+40,180,FOLHA_W-80,62);
  int ini=filtro>1?filtro-1:0;
  float tx=x+40;
  for(int i=ini;i<nProvedores && i<ini+3;i++) {
    float w=i?232:108;int sel=i==filtro,c=sel?24:202;
    gfx_cor((GfxRect){tx,182,w,50},.5f,sel?.94f:.14f,sel?.94f:.14f,sel?.95f:.15f,anim);
    TxtLinha l=txt_linha_corta(TXT_PG_FIM,provedores[i],c,c,c,255,w-24);
    txt_desenhar_alpha(l,tx+(w-l.w)*.5f,196,anim);
    if(sel && grupo==0) gfx_cor((GfxRect){tx+16,237,w-32,2},0,.94f,.94f,.95f,anim);
    tx+=w+12;
  }
  gfx_sem_recorte();
  gfx_recorte(x+36,FOLHA_TOPO,FOLHA_W-72,NV_TELA_H-FOLHA_TOPO-32);
  int nf=nFiltrados();
  for(int row=0;row<nf;row++) {
    float y=FOLHA_TOPO+row*FOLHA_LINHA-rolagem;
    if(y+FOLHA_LINHA<FOLHA_TOPO || y>NV_TELA_H-32) continue;
    int i=filtrado(row),sel=grupo==1 && foco==row;
    const Stream *s=&lista[i];
    GfxRect r={x+40,y,FOLHA_W-80,FOLHA_LINHA-14};
    if(sel) gfx_cor(r,.10f,.94f,.94f,.95f,anim);
    r.x+=2;r.y+=2;r.w-=4;r.h-=4;
    gfx_cor(r,.09f,.135f,.135f,.14f,anim);
    float lx=x+62,w=FOLHA_W-124;
    char nome[sizeof s->rotulo],descricao[sizeof s->descricao];
    snprintf(nome,sizeof nome,"%s",s->rotulo);snprintf(descricao,sizeof descricao,"%s",s->descricao);
    // SDL_ttf nao interpreta quebras de linha; nao renderizar glifos .notdef.
    for(char *p=nome;*p;p++)if((unsigned char)*p<32)*p=' ';
    for(char *p=descricao;*p;p++)if((unsigned char)*p<32)*p=' ';
    txt_desenhar_alpha(txt_linha_corta(TXT_PAINEL_ITEM,nome,240,241,243,255,w),lx,y+16,anim);
    txt_desenhar_alpha(txt_linha_corta(TXT_PG_FIM,i==atual?"Reproduzindo agora":s->provedor,175,178,185,255,w),lx,y+46,anim);
    txt_bloco(TXT_PG_FIM,descricao,194,197,202,lx,y+76,w,25,anim,2);
    char meta[192],qual[24]="";
    if(s->altura) snprintf(qual,sizeof qual," · %dp",s->altura);
    snprintf(meta,sizeof meta,"%s%s%s%s",containerDa(s),qual,s->dolbyVision?" · Dolby Vision":"",s->dolbyAtmos?" · Atmos":"");
    if(s->tamanhoMB) {size_t p=strlen(meta);snprintf(meta+p,sizeof meta-p," · %.1f GB",s->tamanhoMB/1024.0);}
    txt_desenhar_alpha(txt_linha_corta(TXT_MINI,meta,224,226,232,255,w),lx,y+140,anim);
    badges_desenhar(s->badges,lx,y+171,w,26,anim);
  }
  if(!nf) {
    const char *s=addons_estado()==ADD_BUSCANDO?"Buscando fontes nos addons…":"Nenhuma fonte direta disponível. Use Recarregar para tentar novamente.";
    txt_bloco(TXT_PG_FIM,s,196,199,204,x+56,FOLHA_TOPO+40,FOLHA_W-112,28,anim,3);
  }
  gfx_sem_recorte();
}
