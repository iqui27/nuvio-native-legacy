#include "episodios.h"
#include "idioma.h"
#include "catalogo.h"
#include "descoberta.h"
#include "extras.h"
#include "gfx.h"
#include "tex_cache.h"
#include "text.h"
#include "layout.h"
#include "anim.h"
#include "vistoep.h"
#include "trakt.h"
#include "syncprog.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define EP_W 720.0f
#define EP_ROW 172.0f
#define EP_TOP 216.0f
static int aberto, titulo, atualT, atualE, temporada, foco, grupo;
static int pedidoT, pedidoE;
static float anim, scroll;
static int localizarAtual;

// --- MENU DE VISTO -----------------------------------------------------------
//
// Segurar OK numa linha abre. Os tres gestos que o dono pediu — este episodio,
// ate aqui, a temporada inteira — sao O MESMO LOTE em tamanhos diferentes
// (vistoep.h), e o sentido (marcar ou desmarcar) sai do estado do episodio em
// foco: quem esta olhando um episodio visto quer desmarcar.
// VM_FONTES so aparece quando o menu e aberto DE FORA (da pagina de detalhe):
// la o toque curto no card ja abre as fontes e a pressao longa passou a abrir
// este menu, entao ele precisa carregar a porta que tomou. Dentro da folha de
// episodios a opcao nao existe — quem abriu a folha veio do player e ja tem
// fonte tocando; oferece-la ali seria uma linha que nao leva a lugar nenhum.
enum { VM_ESTE = 0, VM_ATE, VM_TEMP, VM_FONTES, VM_N };
static int vmAberto, vmFoco, vmVisto;      // vmVisto: o sentido do gesto
static Uint32 vmDesde;                     // relogio da pressao longa
static int vmSegurando, vmConsumir;

// O ALVO DO MENU E EXPLICITO, e nao lido do estado da folha.
//
// Ele dependia de `titulo`, `foco`, `temporada` e epLinha(foco) — tudo interno
// desta tela —, e por isso so podia existir dentro dela. Guardando o alvo o
// menu passa a servir tambem a pagina de detalhe, onde a folha nem esta aberta.
static int  vmIdx = -1, vmT, vmE;
static char vmNome[96];
static int  vmSo;        // 1 = aberto sozinho, sobre outra tela
static int  vmFontesPed; // consumido por episodios_menu_pediu_fontes()

static int vmOpcoes(void) { return vmSo ? VM_N : VM_N - 1; }

// Abre o menu para um episodio qualquer. `t` e o NUMERO da temporada, nao o
// indice da aba: quem chama de fora nao tem abas.
static void menuAbrir(int idx, int t, int e, const char *nome, int so) {
  const CatItem *ci = cat_item(idx);
  if (!ci || !ci->imdb[0]) return;
  vmIdx = idx; vmT = t; vmE = e; vmSo = so;
  snprintf(vmNome, sizeof vmNome, "%s", nome ? nome : "");
  // O SENTIDO SAI DO ESTADO: quem esta olhando um episodio visto quer
  // desmarcar. Desconhecido (-1) conta como nao visto.
  vmVisto = vistoep_estado(ci->imdb, t, e) == 1 ? 0 : 1;
  vmAberto = 1; vmFoco = 0; vmConsumir = 1; vmFontesPed = 0;
}

// O ENVIO VAI PARA UM FIO. trakt_episodios_marcar e syncep_empurrar sao
// sincronas de proposito (esta escrito nos dois cabecalhos), e cada uma pode
// levar 20 s de timeout. No fio do desenho isso congela a TV.
//
// O efeito LOCAL ja aconteceu antes de o fio nascer: a lista redesenha no mesmo
// quadro e este fio so leva a noticia ao servidor.
typedef struct { char imdb[24], tipo[12]; VistoPar pares[64]; int n, visto; } Envio;
static void *enviarVisto(void *u) {
  Envio *e = (Envio *)u;
  trakt_episodios_marcar(e->imdb, e->pares, e->n, e->visto);
  syncep_empurrar(e->imdb, e->tipo, e->pares, e->n, e->visto);
  free(e);
  return NULL;
}

static int nTemporadas(void) {
  const CatItem *c = cat_item(titulo);
  return c && c->nTemporadas > 0 ? c->nTemporadas : 1;
}
static int numTemporada(int i) {
  const CatItem *c = cat_item(titulo);
  return c && c->nTemporadas > 0 ? c->temporadas[i] : atualT;
}
static const CatEp *epLinha(int linha) {
  int n = cat_n_episodios(titulo);
  for (int i = 0, j = 0; i < n; i++) {
    const CatEp *ep = cat_episodio(titulo, i);
    if (ep && ep->temporada == numTemporada(temporada) && j++ == linha) return ep;
  }
  return NULL;
}
static int nLinhas(void) {
  int n = 0;
  for (int i=0;i<cat_n_episodios(titulo);i++) {
    const CatEp *e=cat_episodio(titulo,i);
    if(e && e->temporada==numTemporada(temporada)) n++;
  }
  return n;
}
// Aplica o gesto: monta o lote, muda o local na hora, e manda o resto para um
// fio. Devolve 0 quando nao ha nada a fazer — e o caso de marcar o que ja esta
// marcado, que nao deve gastar uma requisicao.
static int aplicarVisto(int modo, int visto) {
  const CatItem *ci = cat_item(vmIdx);
  VistoPar lote[64];
  int n = 0, mudou;
  if (!ci || !ci->imdb[0]) return 0;
  if (modo == VM_ESTE) {
    lote[0].temporada = (short)vmT;
    lote[0].episodio = (short)vmE;
    n = 1;
  } else if (modo == VM_ATE) {
    n = vistoep_ate_aqui(ci->imdb, vmT, vmE, lote, 64);
  } else {
    // A TEMPORADA DO EPISODIO, e nao a da aba selecionada. Sao a mesma coisa
    // dentro da folha, e fora dela nao ha aba nenhuma.
    n = vistoep_temporada(ci->imdb, vmT, lote, 64);
  }
  if (n < 1) return 0;
  mudou = vistoep_marcar_lote(ci->imdb, lote, n, visto);
  if (!mudou) return 0;
  { Envio *env = (Envio *)calloc(1, sizeof *env);
    pthread_t fio;
    if (!env) return mudou;
    snprintf(env->imdb, sizeof env->imdb, "%s", ci->imdb);
    snprintf(env->tipo, sizeof env->tipo, "%s", ci->tipo[0] ? ci->tipo : "series");
    memcpy(env->pares, lote, sizeof(VistoPar) * (size_t)n);
    env->n = n; env->visto = visto;
    if (pthread_create(&fio, NULL, enviarVisto, env) == 0) pthread_detach(fio);
    else free(env); }
  return mudou;
}

void episodios_abrir(int idx, int t, int e) {
  titulo = idx; atualT = t; atualE = e; aberto = 1;
  temporada = foco = 0; grupo = 1; pedidoE = 0; scroll = 0;
  vmAberto = 0; vmSegurando = 0; vmConsumir = 0;
  localizarAtual = 1;
  for (int i = 0; i < nTemporadas(); i++) if (numTemporada(i) == t) temporada = i;
  for (int i = 0; i < nLinhas(); i++) if (epLinha(i)->episodio == e) foco = i;
  desc_episodios(titulo, t);
}
int episodios_aberto(void) { return aberto; }
void episodios_fechar(void) { aberto = 0; }
int episodios_escolheu(int *t, int *e) {
  if (!pedidoE) return 0;
  *t = pedidoT; *e = pedidoE; pedidoE = 0; return 1;
}
// O menu em si, separado de quem o hospeda: a folha chama daqui, e a pagina de
// detalhe chama por episodios_menu_evento(). As duas regras abaixo o menu do
// cartaz aprendeu do jeito dificil (ver ctxmenu.c): repeticao automatica nunca
// e uma segunda escolha, e o OK que ABRIU o menu nao escolhe nada.
// O MENU, DESENHADO ONDE MANDAREM. Dentro da folha ele e centrado sobre a
// COLUNA dela (centrar em NV_TELA_W poria o menu sobre o vazio da esquerda,
// longe do episodio a que se refere); aberto sobre a pagina de detalhe, a
// caixa e a tela inteira. A altura sai do numero de opcoes, que muda com isso.
static void menuDesenhar(float x, float larg, float anim) {
  if (!vmAberto) return;
  {
    const CatItem *ci=cat_item(vmIdx);
    // CENTRADO SOBRE A FOLHA, e nao sobre a tela. A folha ocupa so os EP_W da
    // direita; centrar em NV_TELA_W punha o menu sobre o vazio da esquerda,
    // longe do episodio a que ele se refere — visivel na captura de revisao.
    // A largura tambem cabe DENTRO da folha, para o menu nao parecer de outra
    // tela.
    // TETO NA LARGURA. Dentro da folha `larg` e a coluna dela e o menu fica justo;
    // sobre a pagina de detalhe `larg` e a tela inteira, e sem teto o menu virava
    // uma faixa de 1848 px atravessando tudo — visto na captura de revisao. O
    // teto e a largura que ele ja tinha na folha, entao os dois lugares mostram
    // o MESMO menu.
    float mw=larg-72.0f, mh=120.0f+vmOpcoes()*62.0f+62.0f;
    if (mw > EP_W-72.0f) mw = EP_W-72.0f;
    GfxRect m={x+(larg-mw)*.5f,(NV_TELA_H-mh)*.5f,mw,mh};
    char cab[140];
    int i;
    // Veu proprio: a lista atras tem texto pequeno em tres colunas.
    gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,0,0,0,.72f*anim);
    gfx_cor(m,.05f,.11f,.11f,.13f,.99f*anim);
    txt_desenhar_alpha(txt_linha(TXT_CAPTION2,
        vmVisto?"MARCAR COMO ASSISTIDO":"DESMARCAR COMO ASSISTIDO",
        174,178,188,255),m.x+32,m.y+28,anim);
    snprintf(cab,sizeof cab,i18n("T%dE%d · %s"),vmT,vmE,vmNome);
    txt_desenhar_alpha(txt_linha_corta(TXT_HEADLINE,cab,245,248,255,255,mw-64),
                       m.x+32,m.y+56,anim);
    for(i=0;i<vmOpcoes();i++) {
      GfxRect r={m.x+24,m.y+120+i*62,mw-48,54};
      float f=(i==vmFoco)?1.0f:0.0f;
      float lum=f>.5f?.961f:.176f;
      int c=f>.5f?17:240;
      char rot[120];
      int quantos;
      // O NUMERO NO ROTULO, e nao so o verbo: "marcar 7 episodios" e uma
      // decisao diferente de "marcar 1", e a pessoa tem de ver qual das duas
      // esta prestes a tomar. Sai do mapa, que e o mesmo que a acao vai usar.
      if(i==VM_ESTE) quantos=1;
      else if(ci&&i!=VM_FONTES) quantos=(i==VM_ATE)
        ? vistoep_ate_aqui(ci->imdb,vmT,vmE,NULL,0)
        : vistoep_temporada(ci->imdb,vmT,NULL,0);
      else quantos=0;
      gfx_cor(r,14.0f/54.0f,lum,lum,lum,anim);
      if(i==VM_ESTE) snprintf(rot,sizeof rot,"%s",
                              vmVisto?"Este episódio":"Este episódio");
      else if(i==VM_ATE) snprintf(rot,sizeof rot,
                                  quantos==1?i18n("Até aqui (%d episódio)")
                                           :i18n("Até aqui (%d episódios)"),quantos);
      else if(i==VM_TEMP) snprintf(rot,sizeof rot,
                    quantos==1?i18n("Temporada inteira (%d episódio)")
                             :i18n("Temporada inteira (%d episódios)"),quantos);
      else snprintf(rot,sizeof rot,"%s",i18n("Fontes deste episódio"));
      txt_desenhar_alpha(txt_linha_corta(TXT_PLR_CORPO,rot,c,c,c,255,mw-96),
                         r.x+28,r.y+(54-28)*.5f,anim);
    }
    // A DICA FICA ABAIXO DA ULTIMA OPCAO, e a conta e explicita: as tres linhas
    // terminam em m.y+120+3*62-8, e o rodape cravado em mh-52 caia POR CIMA da
    // terceira — a captura de revisao mostrou "Whole season" atravessado pelo
    // texto de ajuda. Mesmo erro de deslocamento fixo que a folha de fileiras
    // teve hoje.
    txt_bloco(TXT_CAPTION,"↑ ↓  Escolher   ·   OK  Aplicar   ·   Voltar  Fechar",
              155,159,169,m.x+28,m.y+120+vmOpcoes()*62+8,mw-56,26,anim*.86f,1);
  }
}

static void menuEvento(const SDL_Event *ev) {
  SDL_Keycode ko = ev->key.keysym.sym;
  int ehOk = (ko == SDLK_RETURN || ko == SDLK_KP_ENTER || ko == SDLK_SPACE);
  if (ev->type == SDL_KEYUP && ehOk) { vmConsumir = 0; return; }
  if (ev->type != SDL_KEYDOWN) return;
  if (ehOk && (ev->key.repeat || vmConsumir)) return;
  if (ko == SDLK_UP)   { if (vmFoco > 0) vmFoco--; return; }
  if (ko == SDLK_DOWN) { if (vmFoco < vmOpcoes() - 1) vmFoco++; return; }
  if (ehOk) {
    if (vmFoco == VM_FONTES) vmFontesPed = 1;
    else aplicarVisto(vmFoco, vmVisto);
    vmAberto = 0;
    return;
  }
  vmAberto = 0;   // qualquer outra tecla fecha
}

void episodios_evento(const SDL_Event *ev) {
  if (!aberto) return;
  { SDL_Keycode ko = ev->key.keysym.sym;
    int ehOk = (ko == SDLK_RETURN || ko == SDLK_KP_ENTER || ko == SDLK_SPACE);

    // O MENU COME A TECLA ENQUANTO ESTA NO AR. Mesma regra do menu do cartaz:
    // ele e a coisa mais recente na tela e e para ela que a pessoa esta
    // olhando.
    if (vmAberto) { menuEvento(ev); return; }

    // PRESSAO LONGA SOBRE UMA LINHA DE EPISODIO. So no grupo da lista: em cima
    // das temporadas ou do cabecalho nao ha episodio para marcar.
    if (ehOk && grupo == 1) {
      if (ev->type == SDL_KEYDOWN && !ev->key.repeat) {
        vmSegurando = 1; vmDesde = SDL_GetTicks(); return;
      }
      if (ev->type == SDL_KEYUP) {
        // A GUARDA OLHA SE FOI PRESSIONADO AQUI, e nao a duracao. Soltar sem
        // ter pressionado nesta camada nao e clique — a de cima pode ter
        // fechado no KEYDOWN e o KEYUP vazar para ca; e a mesma guarda que
        // home.c, detail.c e ctxmenu.c ja tem, pelo mesmo defeito.
        //
        // TESTAR `dur != 0` NO LUGAR DISSO ENGOLE O TOQUE RAPIDO: descida e
        // subida no mesmo milissegundo dao dur=0, que e legitimo. Foi assim na
        // primeira versao, e tests/player.sh pegou na primeira rodada.
        int foiAqui = vmSegurando;
        Uint32 dur = foiAqui ? SDL_GetTicks() - vmDesde : 0;
        vmSegurando = 0;
        if (!foiAqui) return;
        if (dur >= NV_HOLD_MS) {
          const CatEp *e2 = epLinha(foco);
          if (e2) menuAbrir(titulo, e2->temporada, e2->episodio, e2->nome, 0);
          return;
        }
        // TOQUE CURTO ABRE O EPISODIO, e a acao acontece AQUI e nao no ramo
        // antigo la embaixo: o KEYUP e o unico momento que conhece a DURACAO, e
        // e ela que separa abrir de segurar. Deixar isso para o KEYDOWN faria a
        // pressao longa nunca existir, porque o episodio ja teria aberto.
        { const CatEp *e2 = epLinha(foco);
          if (e2) {
            if (e2->temporada != atualT || e2->episodio != atualE) {
              pedidoT = e2->temporada; pedidoE = e2->episodio;
            }
            aberto = 0;
          } }
        return;
      }
    }
    if (ev->type == SDL_KEYUP) { vmSegurando = 0; return; }
  }
  if (ev->type != SDL_KEYDOWN) return;
  SDL_Keycode k = ev->key.keysym.sym;
  // O OK JA FOI DECIDIDO NO KEYUP acima (e o unico jeito de conhecer a
  // DURACAO). Deixar o KEYDOWN cair no ramo antigo abriria o episodio antes de
  // a pressao longa poder existir.
  if ((k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) && grupo == 1)
    return;
  if(k==SDLK_r) { desc_episodios(titulo,numTemporada(temporada)); return; }
  if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_DELETE || k == SDLK_AC_BACK) {
    aberto = 0; return;
  }
  int nt = nTemporadas(), n = nLinhas();
  if (k == SDLK_UP) { if (grupo == 1 && foco > 0) foco--; else grupo--; }
  if (k == SDLK_DOWN) { if (grupo < 1) grupo++; else if (foco < n - 1) foco++; }
  if (grupo < -1) grupo = -1;
  if (grupo == 0 && (k == SDLK_LEFT || k == SDLK_RIGHT)) {
    int nova = temporada + (k == SDLK_RIGHT ? 1 : -1);
    if (nova >= 0 && nova < nt) {
      temporada = nova; foco = 0; scroll = 0;
      localizarAtual = 0;
      desc_episodios(titulo, numTemporada(temporada));
    }
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    if (grupo == -1) aberto = 0;
    else if (grupo == 0) {
      grupo = 1;
      if (!n) desc_episodios(titulo,numTemporada(temporada));
    }
    else {
      const CatEp *ep = epLinha(foco);
      if (ep) {
        if (ep->temporada != atualT || ep->episodio != atualE) {
          pedidoT = ep->temporada; pedidoE = ep->episodio;
        }
        aberto = 0;
      }
    }
  }
}
void episodios_atualizar(float dt) {
  anim = anim_mola(anim, aberto ? 1 : 0, dt, NV_MOLA_TELA);
  if(!aberto && anim<.005f) return;
  desc_episodios_pendente();
  int n = nLinhas();
  if(localizarAtual && n) {
    for(int i=0;i<n;i++) if(epLinha(i)->episodio==atualE) foco=i;
    localizarAtual=0;
  }
  if (foco >= n) foco = n > 0 ? n - 1 : 0;
  float area = NV_TELA_H - EP_TOP - 36;
  float max = n * EP_ROW - area;
  float alvo = scroll;
  if(foco*EP_ROW<scroll) alvo=foco*EP_ROW;
  if((foco+1)*EP_ROW>scroll+area) alvo=(foco+1)*EP_ROW-area;
  if (alvo > max) alvo = max;
  if (alvo < 0) alvo = 0;
  scroll = anim_mola(scroll, alvo, dt, NV_MOLA_SCROLL);
}
void episodios_desenhar(void) {
  if (anim < .005f) return;
  float x = NV_TELA_W - EP_W + (1 - anim) * EP_W;
  gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,.02f,.02f,.025f,.35f*anim);
  gfx_cor((GfxRect){x,0,EP_W,NV_TELA_H},.025f,.095f,.095f,.10f,anim);
  txt_desenhar_alpha(txt_linha(TXT_PAINEL_TITULO,"Episódios",240,241,243,255),x+40,44,anim);
  gfx_cor((GfxRect){x+EP_W-146,44,110,50},.3f,grupo==-1?.94f:.14f,grupo==-1?.94f:.14f,grupo==-1?.95f:.15f,anim);
  int cor = grupo == -1 ? 25 : 230;
  txt_desenhar_alpha(txt_linha(TXT_PG_ROTULO,"Fechar",cor,cor,cor,255),x+EP_W-130,55,anim);
  gfx_recorte(x+36,120,EP_W-72,64);
  int primeira = temporada > 1 ? temporada - 1 : 0;
  for (int i = primeira; i < nTemporadas() && i < primeira+3; i++) {
    float tx = x+40+(i-primeira)*212;
    int sel = i == temporada;
    gfx_cor((GfxRect){tx,126,196,52},.5f,sel?.94f:.14f,sel?.94f:.14f,sel?.95f:.15f,anim);
    char s[48]; snprintf(s,sizeof s,i18n("Temporada %d"),numTemporada(i));
    int b=sel?24:210;
    TxtLinha l=txt_linha(TXT_PG_ROTULO,s,b,b,b,255);
    txt_desenhar_alpha(l,tx+(196-l.w)*.5f,138,anim);
    if (sel && grupo==0) gfx_cor((GfxRect){tx+30,184,136,2},0,.94f,.94f,.95f,anim);
  }
  gfx_sem_recorte();
  gfx_recorte(x+36,EP_TOP,EP_W-72,NV_TELA_H-EP_TOP-32);
  int n=nLinhas();
  for (int i=0;i<n;i++) {
    float y=EP_TOP+i*EP_ROW-scroll;
    if (y+EP_ROW<EP_TOP || y>NV_TELA_H-32) continue;
    const CatEp *ep=epLinha(i);
    int sel=grupo==1 && i==foco;
    GfxRect r={x+40,y,EP_W-80,EP_ROW-14};
    if(sel) gfx_cor(r,.13f,.94f,.94f,.95f,anim);
    r.x+=2; r.y+=2; r.w-=4; r.h-=4;
    gfx_cor(r,.12f,.135f,.135f,.14f,anim);
    const CatItem *ci=cat_item(titulo);
    const char *arte=ep->thumb[0]?ep->thumb:(ci?ci->backdrop:"");
    GLuint tex=tex_obter_larg(arte,184);
    GfxRect tr={x+54,y+14,184,130};
    gfx_cor(tr,.10f,.19f,.19f,.20f,anim);
    if(tex){gfx_tex_aspect_atual=tex_aspecto(arte);gfx_rect(tr,tex,GFX_CARD,0,0,0,.10f,0,0,0,anim);gfx_tex_aspect_atual=0;}
    char num[40];snprintf(num,sizeof num,i18n("T%dE%d"),ep->temporada,ep->episodio);
    gfx_cor((GfxRect){tr.x+8,tr.y+92,72,30},.15f,.025f,.025f,.03f,.9f*anim);
    txt_desenhar_alpha(txt_linha(TXT_MINI,num,240,240,242,255),tr.x+15,tr.y+97,anim);
    float tx=x+260, w=EP_W-310;
    txt_desenhar_alpha(txt_linha_corta(TXT_PAINEL_ITEM,ep->nome[0]?ep->nome:num,242,243,245,255,w),tx,y+16,anim);
    int atual=ep->temporada==atualT && ep->episodio==atualE;
    // O ESTADO SAI DO MAPA, e nao mais da matriz [20][40] de extras.c. A
    // matriz so guarda o "sim": ela nao distingue "nao viu" de "nao sei", e
    // cortava em silencio a temporada 21 e o episodio 40. O mapa distingue, e
    // e ele que as acoes de marcar em lote mudam — desenhar de uma fonte e
    // agir sobre outra faria a linha nao mudar depois do gesto.
    const CatItem *cim=cat_item(titulo);
    int visto=cim?vistoep_estado(cim->imdb,ep->temporada,ep->episodio):-1;
    char estado[96];
    if(atual) snprintf(estado,sizeof estado,"Reproduzindo agora");
    else if(visto==1) snprintf(estado,sizeof estado,i18n("✓ Assistido%s%s"),ep->duracao[0]?" · ":"",ep->duracao);
    else snprintf(estado,sizeof estado,"%s%s%s",ep->data,ep->data[0]&&ep->duracao[0]?" · ":"",ep->duracao);
    txt_desenhar_alpha(txt_linha_corta(TXT_PG_FIM,estado,atual?236:180,atual?237:182,atual?240:188,255,w),tx,y+48,anim);
    txt_bloco(TXT_PG_FIM,ep->sinopse,186,188,194,tx,y+78,w,25,anim,3);
  }
  if(!n) txt_bloco(TXT_PG_FIM,desc_episodios_carregando(titulo)?
    "Carregando episódios…":"Episódios indisponíveis. Selecione a temporada e pressione OK para tentar novamente.",
    196,198,204,x+56,EP_TOP+40,EP_W-112,28,anim,4);
  gfx_sem_recorte();
  if(n) {
    char contador[48];snprintf(contador,sizeof contador,i18n("%d de %d episódios"),foco+1,n);
    txt_desenhar_alpha(txt_linha(TXT_MINI,contador,166,168,174,255),x+40,NV_TELA_H-26,anim);
  }
  // DICA DO GESTO, escrita na tela. Pressao longa nao se descobre sozinha num
  // D-pad — foi a licao do menu do cartaz, que ganhou a mesma linha.
  if(n && grupo==1 && !vmAberto) {
    txt_desenhar_alpha(txt_linha(TXT_MINI,"Segure OK para marcar como assistido",
                                 150,153,162,255),x+300,NV_TELA_H-26,anim*.9f);
  }

  menuDesenhar(x, EP_W, anim);
  
}

// --- O MENU SOZINHO, SOBRE OUTRA TELA ----------------------------------------
//
// A pagina de detalhe usa isto. Ver o comentario de VM_FONTES: la o toque curto
// no card abre as fontes e a pressao longa passou a abrir este menu, entao ele
// carrega a porta que tomou.
void episodios_menu_visto(int idxCat, int temporada, int episodio,
                          const char *nome) {
  menuAbrir(idxCat, temporada, episodio, nome, 1);
}
int  episodios_menu_aberto(void) { return vmAberto && vmSo; }
void episodios_menu_evento(const SDL_Event *e) {
  if (vmAberto && vmSo) menuEvento(e);
}
void episodios_menu_desenhar(void) {
  if (vmAberto && vmSo) menuDesenhar(0.0f, (float)NV_TELA_W, 1.0f);
}
int  episodios_menu_pediu_fontes(void) { int v = vmFontesPed; vmFontesPed = 0; return v; }
