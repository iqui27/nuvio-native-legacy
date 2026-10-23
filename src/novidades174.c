// Release 1.7.4: animated previews of episodes, actions and Home loading.
#include "novidades174.h"
#include "ajustes.h"
#include "anim.h"
#include "dados.h"
#include "gfx.h"
#include "idioma.h"
#include "layout.h"
#include "ponteiro.h"
#include "qr.h"
#include "tex_cache.h"
#include "text.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N174_ARQ          "novidades-174-ui.txt"
#define N174_W            1740.0f
#define N174_H            1000.0f
#define N174_X            ((NV_TELA_W - N174_W) * 0.5f)
#define N174_Y            ((NV_TELA_H - N174_H) * 0.5f)
#define N174_PAD            52.0f
#define N174_RAIO           32.0f
#define N174_BT_H           72.0f
#define N174_BT_GAP         16.0f
#define N174_PV_W          760.0f
#define N174_PV_H          (N174_H - 2.0f * N174_PAD - N174_BT_H - 28.0f)
#define N174_PV_RAIO        26.0f
#define N174_COL_GAP        64.0f
#define N174_TXT_X        (N174_X + N174_PAD + N174_PV_W + N174_COL_GAP)
#define N174_TXT_W        (N174_X + N174_W - N174_PAD - N174_TXT_X)
#define N174_ICONE          40.0f
#define N174_ABRIR_MS      280.0f
#define N174_FECHAR_MS     160.0f
#define N174_TRANSICAO_S     0.55f
#define N174_ARTE_W       1280.0f

// A paleta do logo (medida no logo.png do lancamento).
#define AMEIXA_R 0.106f
#define AMEIXA_G 0.039f
#define AMEIXA_B 0.098f
#define CREME_R  0.996f
#define CREME_G  0.902f
#define CREME_B  0.769f
#define LARANJA_R 0.949f
#define LARANJA_G 0.486f
#define LARANJA_B 0.118f
#define AMARELO_R 0.980f
#define AMARELO_G 0.659f
#define AMARELO_B 0.157f
#define VERMELHO_R 0.839f
#define VERMELHO_G 0.204f
#define VERMELHO_B 0.165f
// Os mesmos, em bytes, para o texto (txt_linha recebe int).
#define CREME_I    254, 230, 196
#define CREME2_I   214, 190, 170   // creme apagado: a linha de descricao
#define AMEIXA_I    34,  12,  30
#define LARANJA_I  246, 140,  52

enum { B_DEPOIS = 0, B_OK = 1, B_N };
enum { ID_NADA = 0, ID_VISUAL, ID_EMAIL, ID_FONTES, ID_SEEKR, ID_AJUSTES, ID_RETOMAR, ID_CONSERTOS };

static int   aberto, decidido, foco = B_OK, naPrevia;
static int   cena, cenaAntiga;
static float entrada, transicao = 1.0f, relogioCena, tempoAntiga;
static char  dirArte[512] = "deploy/app/art";
static char  arqAbertura[600], arqListras[600];

// ------------------------------------------------------------------ apoio
static float rr(float px, GfxRect r) { float m = r.w < r.h ? r.w : r.h; return m > 0.0f ? px / m : 0.0f; }
static float passo(float t, float ini, float dur) {
  return anim_suave(anim_clamp((t - ini) / dur, 0.0f, 1.0f));
}

static void montarCaminhos(void) {
  snprintf(arqAbertura, sizeof arqAbertura, "%s/marcas/abertura.jpg", dirArte);
  snprintf(arqListras, sizeof arqListras, "%s/marcas/login-fundo.jpg", dirArte);
}
static void pedirArtes(void) {
  tex_obter_larg(arqAbertura, N174_ARTE_W);
  tex_obter_larg(arqListras, N174_ARTE_W);
}

// Arte em "cover" num retangulo arredondado; ameixa enquanto nao chega.
static void cobrir(const char *arq, GfxRect r, float raioPx, float a) {
  GLuint t;
  if (a <= 0.003f) return;
  t = tex_obter_larg(arq, N174_ARTE_W);
  if (!t) { gfx_cor(r, rr(raioPx, r), AMEIXA_R, AMEIXA_G, AMEIXA_B, a); return; }
  gfx_tex_aspect_atual = tex_aspecto(arq);
  if (gfx_tex_aspect_atual <= 0.0f) gfx_tex_aspect_atual = 16.0f / 9.0f;
  gfx_card_forcar_cover_atual = 1.0f;
  gfx_rect(r, t, GFX_CARD, 0, 0, 0, rr(raioPx, r), 0, 0, 0, a);
  gfx_card_forcar_cover_atual = 0.0f;
  gfx_tex_aspect_atual = 0.0f;
}

// A PILULA NA COR DO LOGO. Primario: laranja com texto ameixa; com foco ganha
// o anel creme e clareia. Secundario: vidro creme; com foco vira creme cheio.
static float pilulaLargura(const char *rot, const char *icone) {
  return (float)txt_largura(TXT_BODY, rot) + 2.0f * 34.0f + (icone ? 38.0f : 0.0f);
}
static void pilula(GfxRect r, const char *rot, const char *icone, float foco, int primario, float a) {
  TxtLinha claro = txt_linha(TXT_BODY, rot, CREME_I, 255);
  TxtLinha escuro = txt_linha(TXT_BODY, rot, AMEIXA_I, 255);
  int textoEscuro = primario || foco > 0.5f;
  TxtLinha l = textoEscuro ? escuro : claro;
  float raio = 0.5f, x;
  if (primario) {
    float k = 0.86f + 0.14f * foco;
    gfx_cor(r, raio, LARANJA_R * k + (1.0f - k) * 0.3f, LARANJA_G * k, LARANJA_B * k, a);
  } else {
    gfx_cor(r, raio, CREME_R, CREME_G, CREME_B, (0.10f + 0.82f * foco) * a);
  }
  if (foco > 0.0f) {
    GfxRect anel = { r.x - 5.0f, r.y - 5.0f, r.w + 10.0f, r.h + 10.0f };
    gfx_rect(anel, 0, GFX_ANEL, 0, 3.0f / anel.h, 0, 0.5f, CREME_R, CREME_G, CREME_B, foco * a);
  }
  x = r.x + (r.w - (float)l.w - (icone ? 38.0f : 0.0f)) * 0.5f;
  if (icone) {
    float c = textoEscuro ? 0.13f : CREME_R;
    gfx_icone((GfxRect){ x, r.y + (r.h - 28.0f) * 0.5f, 28.0f, 28.0f }, icone,
              c, textoEscuro ? 0.05f : CREME_G, textoEscuro ? 0.12f : CREME_B, a);
    x += 38.0f;
  }
  txt_desenhar_alpha(l, x, r.y + (r.h - (float)l.h) * 0.5f, a);
}

// As tres listras do logo (laranja, amarelo, vermelho), em pilulas finas.
static void listras(float x, float y, float w, float a) {
  float h = 6.0f, gap = 4.0f;
  gfx_cor((GfxRect){ x, y, w, h }, 0.5f, LARANJA_R, LARANJA_G, LARANJA_B, a);
  gfx_cor((GfxRect){ x, y + h + gap, w * 0.82f, h }, 0.5f, AMARELO_R, AMARELO_G, AMARELO_B, a);
  gfx_cor((GfxRect){ x, y + 2.0f * (h + gap), w * 0.64f, h }, 0.5f, VERMELHO_R, VERMELHO_G, VERMELHO_B, a);
}

// ------------------------------------------------------------------- cenas
// Cada cena desenha dentro da previa (x, y) .. (x + PV_W, y + PV_H), com o
// relogio dela em `t` segundos.

// A CARA NOVA: a abertura, o logo se dissolvendo e o login sobre as listras.
#define CA_DUR 7.0f
// Animated previews illustrate the UI; they do not use account or media data.
static void previewBase(float x,float y,float a) {
  GfxRect r={x,y,N174_PV_W,N174_PV_H};
  cobrir(arqListras,r,N174_PV_RAIO,.25f*a);
  gfx_cor(r,rr(N174_PV_RAIO,r),.045f,.05f,.065f,.90f*a); // vidro-ok: preview art backing
  gfx_luz_canto(r,rr(N174_PV_RAIO,r),380,120,520,
                LARANJA_R,LARANJA_G,LARANJA_B,.18f*a);
}
static void previewTitle(float x,float y,const char *name,const char *body,float a) {
  txt_bloco(TXT_TITULO3,i18n(name),CREME_I,x+48,y+105,N174_PV_W-96,48,a,2);
  txt_bloco(TXT_CAPTION,i18n(body),CREME2_I,x+48,y+205,N174_PV_W-96,30,a,3);
}
static void cenaEpisodios(float x,float y,float t,float a) {
  previewBase(x,y,a);
  previewTitle(x,y,"Cada episódio, no seu ritmo",
    "Duração à vista. Tempo restante para continuar de onde você parou.",a);
  for(int i=0;i<3;i++) {
    float px=x+48+i*224, py=y+340;
    GfxRect r={px,py,208,210};
    float appear=passo(t,.2f+i*.15f,.65f);
    gfx_cor(r,rr(20,r),.12f+i*.015f,.14f,.17f,a*appear);
    gfx_luz_canto(r,rr(20,r),100,20,180,.35f,.22f,.16f,.7f*a*appear);
    gfx_icone((GfxRect){px+18,py+165,20,20},
              i==0?"aj_rotate-ccw-clock":"play",CREME_R,CREME_G,CREME_B,a*appear);
    const char *duration=i==0?"42min":i==1?"24min":"45min";
    float tx=px+48;
    if(i==1) {
      GfxRect bar={tx,py+174,40,4};gfx_cor(bar,.5f,1,1,1,.25f*a*appear);
      bar.w*=.45f;gfx_cor(bar,.5f,1,1,1,.9f*a*appear);tx+=50;
    }
    txt_desenhar_alpha(txt_linha(TXT_CAPTION2,duration,CREME_I,255),tx,py+162,a*appear);
    char label[32];snprintf(label,sizeof label,i18n("EPISÓDIO %d"),i+1);
    txt_desenhar_alpha(txt_linha(TXT_CAPTION2,label,CREME2_I,255),px,py+230,a*appear);
    if(i==0) {
      TxtLinha w=txt_linha(TXT_CAPTION2,i18n("Assistido"),CREME_I,255);
      gfx_cor((GfxRect){px+16,py+18,w.w+24,34},.5f,.12f,.28f,.23f,a*appear);
      txt_desenhar_alpha(w,px+28,py+22,a*appear);
    }
  }
}
static void cenaAcoes(float x,float y,float t,float a) {
  previewBase(x,y,a);
  previewTitle(x,y,"Mais espaço para o filme",
    "Foque em adicionar para revelar as ações. Tudo perto, sem ocupar a cena.",a);
  float reveal=passo(t,1.1f,.9f);
  GfxRect primary={x+52,y+410,250,68};
  pilula(primary,i18n("Reproduzir"),"play",0,1,a);
  float anchorX=x+326;
  for(int i=3;i>=1;i--) {
    float p=anim_suave(anim_clamp(reveal-i*.06f,0,1));
    GfxRect r={anchorX+i*82*p,y+410,68,68};
    gfx_cor(r,.5f,.19f,.20f,.23f,a*p);
    gfx_icone((GfxRect){r.x+22,r.y+22,24,24},
              i==1?"visto":i==2?"fontes":"arte",CREME_R,CREME_G,CREME_B,a*p);
  }
  GfxRect anchor={anchorX,y+410,68,68};
  gfx_cor(anchor,.5f,CREME_R,CREME_G,CREME_B,a);
  gfx_icone((GfxRect){anchor.x+22,anchor.y+22,24,24},"mais",AMEIXA_R,AMEIXA_G,AMEIXA_B,a);
  txt_bloco(TXT_CALLOUT,i18n("Apple TV e Moderno"),CREME2_I,x+52,y+530,N174_PV_W-104,32,a,1);
}
static void cenaHome(float x,float y,float t,float a) {
  previewBase(x,y,a);
  previewTitle(x,y,"Você sabe o que está acontecendo",
    "A ilha acompanha o carregamento da Home e abre mais informações.",a);
  float p=passo(t,.4f,3.0f);
  const char *label=i18n(p>.98f?"Home carregada":"Carregando fileiras…");
  TxtLinha text=txt_linha(TXT_CALLOUT,label,CREME_I,255);
  GfxRect island={x+(N174_PV_W-text.w-88)*.5f,y+340,text.w+88,64};
  gfx_cor(island,.5f,.16f,.17f,.20f,a);
  gfx_icone((GfxRect){island.x+22,island.y+20,24,24},p>.98f?"check":"menu_home",
            LARANJA_R,LARANJA_G,LARANJA_B,a);
  txt_desenhar_alpha(text,island.x+58,island.y+(64-text.h)*.5f,a);
  for(int row=0;row<3;row++) {
    float show=passo(t,.6f+row*.55f,.5f);
    for(int j=0;j<5;j++) {
      GfxRect r={x+52+j*134,y+452+row*72,118,54};
      gfx_cor(r,rr(10,r),.16f,.18f,.22f,(.18f+.75f*show)*a);
    }
  }
}

// ================================================================ as tabelas
typedef struct {
  const char *nome;
  void (*desenhar)(float x, float y, float t, float a);
  float duracao, estatico;   // s; o quadro das animacoes reduzidas
  int id;
} Cena;

static const Cena CENAS[] = {
  { "Episódios", cenaEpisodios, 7.0f, 4.0f, ID_RETOMAR },
  { "Ações mais leves", cenaAcoes, 7.0f, 4.0f, ID_VISUAL },
  { "Home em andamento", cenaHome, 7.0f, 4.0f, ID_FONTES },
};
#define N174_NC ((int)(sizeof CENAS / sizeof *CENAS))

typedef struct { int id; const char *icone, *nome, *linha; } Item;

static const Item ITENS[] = {
  { ID_VISUAL,"aj_palette","Detalhes mais elegantes",
    "Ações ao focar, elenco alinhado e temporadas mais compactas." },
  { ID_RETOMAR,"play","Episódios mais claros",
    "Duração, reassistir e tempo restante na barra de progresso." },
  { ID_FONTES,"menu_home","Home com mais informação",
    "A ilha mostra o carregamento das fileiras e avisa quando a Home está pronta." },
  { ID_EMAIL,"aj_keyboard","Discord na TV",
    "Vincule sua conta e mostre o que está assistindo no seu perfil." },
  { ID_AJUSTES,"menu_settings","Sua conta, mais organizada",
    "Correções na leitura da ordem e dos catálogos ocultos da conta." },
  { ID_SEEKR,"aj_images","Legendas e notas",
    "Mais idiomas reconhecidos e notas IMDb consistentes entre as telas." },
  { ID_CONSERTOS,"check","Player e navegação",
    "Próximo episódio com blur, avanço mais limpo e Voltar corrigido na filmografia." },
};
#define N174_NI ((int)(sizeof ITENS / sizeof *ITENS))

// A cor do disco de cada linha, na ordem das listras do logo.
static void corItem(int i, float *r, float *g, float *b) {
  switch (i % 3) {
    case 0:  *r = LARANJA_R;  *g = LARANJA_G;  *b = LARANJA_B;  break;
    case 1:  *r = AMARELO_R;  *g = AMARELO_G;  *b = AMARELO_B;  break;
    default: *r = VERMELHO_R; *g = VERMELHO_G; *b = VERMELHO_B; break;
  }
}

// ------------------------------------------------------------------- estado
int novidades174_itens(void) { return N174_NI; }
int novidades174_item_largura(int i, int *limite, const char **nome) {
  float tx = N174_TXT_X + N174_ICONE + 20.0f;
  if (i < 0 || i >= N174_NI) return 0;
  if (limite) *limite = (int)(N174_TXT_X + N174_TXT_W - tx);
  if (nome) *nome = ITENS[i].nome;
  return txt_largura(TXT_CAPTION, i18n(ITENS[i].linha));
}
int novidades174_cenas(void) { return N174_NC; }
int novidades174_aberto(void) { return aberto; }

void novidades174_dir(const char *d) {
  if (d && d[0]) snprintf(dirArte, sizeof dirArte, "%s", d);
  montarCaminhos();
}

static void mudarCena(int nova) {
  if (nova < 0) nova = N174_NC - 1;
  if (nova >= N174_NC) nova = 0;
  if (nova == cena) return;
  tempoAntiga = relogioCena;
  cenaAntiga = cena;
  cena = nova;
  transicao = ajustes_animacoes_reduzidas() ? 1.0f : 0.0f;
  relogioCena = 0.0f;
}

void novidades174_ir(int c, float t) {
  cena = cenaAntiga = (c % N174_NC + N174_NC) % N174_NC;
  transicao = 1.0f;
  relogioCena = t;
}

static void comecar(float e) {
  aberto = decidido = 1;
  foco = B_OK;
  naPrevia = 0;
  cena = cenaAntiga = 0;
  entrada = e;
  transicao = 1.0f;
  relogioCena = tempoAntiga = 0.0f;
  if (!arqAbertura[0]) montarCaminhos();
  pedirArtes();
}

void novidades174_abrir(void) { comecar(1.0f); }

void novidades174_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N174_ARQ);
  if (s) { free(s); return; }
  comecar(0.0f);
}

static void fechar(void) {
  aberto = 0;
  dados_gravar(N174_ARQ, "1\n");
  // O QR so existe enquanto o cartao esta na tela.
}

void novidades174_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || !e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_UP)   { naPrevia = 1; return; }
  if (k == SDLK_DOWN) { naPrevia = 0; return; }
  if (k == SDLK_LEFT) {
    if (naPrevia) mudarCena(cena - 1);
    else if (foco > 0) foco--;
    return;
  }
  if (k == SDLK_RIGHT) {
    if (naPrevia) mudarCena(cena + 1);
    else if (foco < B_N - 1) foco++;
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (naPrevia) { mudarCena(cena + 1); return; }
    fechar();
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK)
    fechar();
}

void novidades174_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (aberto) pedirArtes();
  if (ajustes_animacoes_reduzidas()) {
    entrada = aberto ? 1.0f : 0.0f;
    transicao = 1.0f;
    if (aberto) {
      relogioCena += dt;
      if (relogioCena >= CENAS[cena].duracao) mudarCena(cena + 1);
    }
    return;
  }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt, aberto ? N174_ABRIR_MS : N174_FECHAR_MS);
  if (aberto) {
    if (transicao < 1.0f) {
      transicao += dt / N174_TRANSICAO_S;
      if (transicao > 1.0f) transicao = 1.0f;
      tempoAntiga += dt;
    }
    relogioCena += dt;
    if (relogioCena >= CENAS[cena].duracao) mudarCena(cena + 1);
  }
}

// ------------------------------------------------------------------ desenho
static void desenhaCena(int c, float x, float y, float t, float a) {
  if (a <= 0.003f) return;
  CENAS[c].desenhar(x, y, ajustes_animacoes_reduzidas() ? CENAS[c].estatico : t, a);
}

// O selo com o nome da cena e os tracos do ciclo, no alto da previa.
static GfxRect seloRect;
static void selo(float x, float y, float a) {
  const char *nome = i18n(CENAS[cena].nome);
  TxtLinha l = txt_linha(TXT_CAPTION, nome, CREME_I, 255);
  TxtLinha lf = txt_linha(TXT_CAPTION, nome, AMEIXA_I, 255);
  GfxRect s = { x + 24.0f, y + 24.0f, (float)l.w + 36.0f, 44.0f };
  float tA = anim_suave(transicao);
  int i;
  if (naPrevia) gfx_cor(s, 0.5f, LARANJA_R, LARANJA_G, LARANJA_B, a);
  else gfx_cor(s, 0.5f, AMEIXA_R, AMEIXA_G, AMEIXA_B, 0.72f * a);
  txt_desenhar_alpha(naPrevia ? lf : l, s.x + 18.0f, s.y + (s.h - (float)l.h) * 0.5f, a * tA);
  seloRect = s;
  for (i = 0; i < N174_NC; i++) {
    float tw = 26.0f, gap = 8.0f;
    float tx = x + N174_PV_W - 24.0f - (float)(N174_NC - i) * (tw + gap) + gap;
    GfxRect tr = { tx, y + 44.0f, tw, 4.0f };
    gfx_cor(tr, 0.5f, CREME_R, CREME_G, CREME_B, 0.28f * a);
    if (i < cena) gfx_cor(tr, 0.5f, CREME_R, CREME_G, CREME_B, 0.70f * a);
    if (i == cena) {
      float p = anim_clamp(relogioCena / CENAS[cena].duracao, 0.0f, 1.0f);
      tr.w *= p;
      if (tr.w > 1.0f) gfx_cor(tr, 0.5f, LARANJA_R, LARANJA_G, LARANJA_B, a);
    }
  }
}

// Uma linha da lista. `vivo` 0..1: a cena na previa fala desta linha.
static void desenhaItem(int i, float y, float vivo, int maxL, float a) {
  float tx = N174_TXT_X + N174_ICONE + 20.0f, tw = N174_TXT_X + N174_TXT_W - tx;
  GfxRect d = { N174_TXT_X, y + 4.0f, N174_ICONE, N174_ICONE };
  GfxRect ic = { d.x + 9.0f, d.y + 9.0f, 22.0f, 22.0f };
  float cr, cg, cb;
  corItem(i, &cr, &cg, &cb);
  gfx_cor(d, 0.5f, cr, cg, cb, (0.20f + 0.80f * vivo) * a);
  gfx_icone(ic, ITENS[i].icone, CREME_R + (0.13f - CREME_R) * vivo, CREME_G + (0.05f - CREME_G) * vivo,
            CREME_B + (0.12f - CREME_B) * vivo, a);
  { TxtLinha n = txt_linha_corta(TXT_BODY, i18n(ITENS[i].nome), CREME_I, 255, tw);
    txt_desenhar_alpha(n, tx, y, a); }
  txt_bloco_corta(TXT_CAPTION, i18n(ITENS[i].linha), CREME2_I, tx, y + 33.0f, tw, 27.0f, a, maxL);
}

// Mede a lista e distribui o espaco: quem nao cabe numa linha ganha a segunda
// enquanto houver altura; o vao entre as linhas e o que sobrar.
static float linhaH[16];
static int linhaMax[16];
static void medirLista(float alto, float *gap) {
  float tx = N174_TXT_X + N174_ICONE + 20.0f, tw = N174_TXT_X + N174_TXT_W - tx;
  float soma = 60.0f * (float)N174_NI, folga;
  int i;
  folga = alto - soma - 8.0f * (float)(N174_NI - 1);
  for (i = 0; i < N174_NI; i++) {
    int larga = txt_largura(TXT_CAPTION, i18n(ITENS[i].linha)) > (int)tw;
    linhaMax[i] = 1;
    linhaH[i] = 60.0f;
    if (larga && folga >= 27.0f) {
      linhaMax[i] = 2; linhaH[i] += 27.0f; soma += 27.0f; folga -= 27.0f;
    }
  }
  *gap = N174_NI > 1 ? anim_clamp((alto - soma) / (float)(N174_NI - 1), 8.0f, 34.0f) : 0.0f;
}

static void ponteiroFoco(int b, int nada) { (void)nada; foco = b; naPrevia = 0; }
static void ponteiroOk(int b, int nada) { (void)b; (void)nada; fechar(); }
static void focarSelo(int nada, int nada2) { (void)nada; (void)nada2; naPrevia = 1; }
static void ponteiroSelo(int nada, int nada2) { (void)nada; (void)nada2; naPrevia = 1; mudarCena(cena + 1); }

void novidades174_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, y0;
  GfxRect card;
  (void)agora;
  if (entrada < 0.002f) return;
  if (aberto) ponteiro_camada();
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.78f * entrada);
  dy = (1.0f - a) * 34.0f;
  y0 = N174_Y + dy;
  card = (GfxRect){ N174_X, y0, N174_W, N174_H };
  // O cartao: ameixa, as listras do login bem baixas e o brilho laranja que
  // vem do canto da previa.
  gfx_cor(card, N174_RAIO / N174_H, AMEIXA_R, AMEIXA_G, AMEIXA_B, 0.97f * a);
  cobrir(arqListras, card, N174_RAIO, 0.24f * a);
  gfx_rect(card, 0, GFX_ANEL, 0, 1.4f / N174_H, 0, N174_RAIO / N174_H, CREME_R, CREME_G, CREME_B, 0.14f * a);
  gfx_luz_canto(card, N174_RAIO / N174_H, N174_PAD + N174_PV_W * 0.5f, -120.0f, 820.0f,
                LARANJA_R, LARANJA_G, LARANJA_B, 0.10f * a);
  gfx_recorte(card.x, card.y, card.w, card.h);

  // ----- a previa
  { float px = N174_X + N174_PAD, py = y0 + N174_PAD;
    float t = anim_suave(transicao);
    GfxRect pv = { px, py, N174_PV_W, N174_PV_H };
    gfx_recorte(pv.x, pv.y, pv.w, pv.h);
    gfx_cor(pv, rr(N174_PV_RAIO, pv), AMEIXA_R, AMEIXA_G, AMEIXA_B, a);
    if (transicao < 1.0f && cenaAntiga != cena)
      desenhaCena(cenaAntiga, px - t * 24.0f, py, tempoAntiga, a * (1.0f - anim_clamp(t / 0.5f, 0.0f, 1.0f)));
    desenhaCena(cena, px + (1.0f - t) * 24.0f, py, relogioCena,
                a * (cenaAntiga != cena ? anim_clamp((t - 0.38f) / 0.62f, 0.0f, 1.0f) : t));
    gfx_rect(pv, 0, GFX_ANEL, 0, 1.4f / pv.h, 0, rr(N174_PV_RAIO, pv), CREME_R, CREME_G, CREME_B, 0.16f * a);
    selo(px, py, a);
    if (aberto)
      ponteiro_alvo(seloRect.x, seloRect.y, seloRect.w, seloRect.h, focarSelo, ponteiroSelo, 0, 0);
    gfx_recorte(card.x, card.y, card.w, card.h); }

  // ----- a coluna da direita: marca, titulo, listras e a lista.
  { char tit[64];
    float ya = y0 + N174_PAD, gap, alto, yy;
    int i;
    snprintf(tit, sizeof tit, i18n("Novidades da %s"), N174_VERSAO);
    { TxtLinha k = txt_linha(TXT_CAPTION2, "NUVIO LEGACY", LARANJA_I, 255);
      txt_desenhar_alpha(k, N174_TXT_X, ya, a); }
    txt_bloco(TXT_TITULO2, tit, CREME_I, N174_TXT_X, ya + 30.0f, N174_TXT_W, 62.0f, a, 1);
    listras(N174_TXT_X, ya + 30.0f + 74.0f, 210.0f, a);
    yy = ya + 30.0f + 74.0f + 26.0f + 26.0f;
    alto = (y0 + N174_PAD + N174_PV_H) - yy;
    medirLista(alto, &gap);
    for (i = 0; i < N174_NI; i++) {
      float vivo = 0.0f, tA = anim_suave(transicao);
      float local = ajustes_animacoes_reduzidas() ? 1.0f
                  : anim_clamp((a - 0.04f * (float)i) * 3.0f, 0.0f, 1.0f);
      if (ITENS[i].id == CENAS[cena].id) vivo += tA;
      if (transicao < 1.0f && ITENS[i].id == CENAS[cenaAntiga].id) vivo += 1.0f - tA;
      if (vivo > 1.0f) vivo = 1.0f;
      desenhaItem(i, yy + (1.0f - local) * 12.0f, vivo, linhaMax[i], a * local);
      yy += linhaH[i] + gap;
    } }

  // ----- o rodape: a dica do D-pad a esquerda, os dois botoes a direita.
  { float yBase = y0 + N174_H - N174_PAD;
    const char *rotOk = i18n("Entendi");
    const char *rotD = i18n("Agora não");
    float wOk = pilulaLargura(rotOk, "check"), wD = pilulaLargura(rotD, NULL);
    GfxRect bOk = { N174_X + N174_W - N174_PAD - wOk, yBase - N174_BT_H, wOk, N174_BT_H };
    GfxRect bD = { bOk.x - N174_BT_GAP - wD, yBase - N174_BT_H * 0.5f - 30.0f, wD, 60.0f };
    { TxtLinha h = txt_linha_corta(TXT_CAPTION2,
                     i18n(naPrevia ? "← → Trocar a prévia  ·  ↓ Botões" : "↑ Escolher a prévia"),
                     CREME2_I, 255, bD.x - 32.0f - (N174_X + N174_PAD));
      txt_desenhar_alpha(h, N174_X + N174_PAD + 4.0f, yBase - N174_BT_H * 0.5f - (float)h.h * 0.5f,
                         a * 0.9f); }
    pilula(bD, rotD, NULL, !naPrevia && foco == B_DEPOIS ? 1.0f : 0.0f, 0, a);
    pilula(bOk, rotOk, "check", !naPrevia && foco == B_OK ? 1.0f : 0.0f, 1, a);
    if (aberto) {
      ponteiro_alvo(bD.x, bD.y, bD.w, bD.h, ponteiroFoco, ponteiroOk, B_DEPOIS, 0);
      ponteiro_alvo(bOk.x, bOk.y, bOk.w, bOk.h, ponteiroFoco, ponteiroOk, B_OK, 0);
    } }
  gfx_sem_recorte();
}
