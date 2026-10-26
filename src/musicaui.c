#include "musicaui.h"
#include "anim.h"
#include "video.h"
#include "player.h"
#include "trailer.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "corviva.h"
#include "qrtex.h"
#include "idioma.h"
#include "ajustes.h"
#include "teclavoltar.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static struct {
  MusAlbum a;
  MusPrevia p;
  char imdb[16], titulo[128], qrUrl[512];
  int aberta, temp, ano, foco, guardado, suporta, pendente, som, erro, volumePronto;
  Uint32 entrada, mudou, toast, agora;
  unsigned previaPedido;
  int atualizada, retentou, indisponivel[MUS_FAIXAS_MAX];
  GLuint qr;
  float focoLinha[MUS_POR_PAGINA];
} ui;

static void parar(void);
static GfxRect origem;
static float abertura, aberturaVel;
static Uint32 aberturaTick;
void musicaui_origem(GfxRect r) { origem=r; }
int musicaui_visivel(void) { return ui.aberta || abertura>.001f; }
static void voltar(void) { parar(); ui.aberta=0; }

static int suporta(void) {
#if defined(NV_MUSICA_SHOT)
  return 1;
#elif defined(__APPLE__) || defined(NV_LINUX_DESKTOP) || defined(NV_TIZEN3_LITE)
  return 0;
#else
  return video_iniciar_auto();
#endif
}
static int ocupado(void) { return player_aberto() || player_retido() || player_mini_ativo(); }
static void parar(void) {
  if (ui.p.ativa && !ui.pendente && !ocupado()) { video_parar(); video_volume(ui.som ? 100 : 0); }
  mus_previa_parar(&ui.p); ui.pendente = 0; ui.previaPedido = 0;
  if (ui.foco == 4) ui.foco = ui.suporta ? 1 : 3;
}
void musicaui_fechar(void) {
  parar(); ui.aberta = 0; abertura=aberturaVel=0; trailer_bloquear_musica(0);
  if (ui.qr) { gfx_tex_esquecer(ui.qr); glDeleteTextures(1, &ui.qr); ui.qr = 0; }
}
int musicaui_aberta(void) { return ui.aberta; }
void musicaui_abrir(const MusAlbum *a, const char *imdb, const char *titulo, int ano, int temp) {
  musicaui_fechar(); memset(&ui, 0, sizeof ui);
  ui.a = *a; ui.aberta = 1; ui.ano = ano; ui.temp = temp;
  trailer_bloquear_musica(1);
  snprintf(ui.imdb, sizeof ui.imdb, "%s", imdb);
  snprintf(ui.titulo, sizeof ui.titulo, "%s", titulo);
  ui.guardado = mus_guardado(imdb, temp); ui.suporta = suporta();
  int tem = 0;
  for (int i = 0; i < a->n; i++) tem |= a->faixa[i].previa[0] != 0;
  ui.suporta &= tem; ui.foco = ui.suporta ? 1 : 3;
  ui.entrada = ui.mudou = SDL_GetTicks(); aberturaTick=ui.entrada;
  ui.som = trailer_aberto() ? trailer_com_som() : 1;
  // Retained main playback owns the backend; closing a trailer is safe only otherwise.
  if (!ocupado()) trailer_fechar();
  mus_url_celular(ui.qrUrl, sizeof ui.qrUrl, imdb, titulo, ano, temp);
  // The worker resolves canonical metadata from IMDb; keep long/CJK titles
  // within the existing QR version-6 capacity without changing identity.
  if (strlen(ui.qrUrl) > 134) mus_url_celular(ui.qrUrl, sizeof ui.qrUrl, imdb, ".", ano, temp);
  if (ui.qrUrl[0]) qr_textura(&ui.qr, ui.qrUrl);
}
static void tocar(int faixa, int shuffle) {
  if (!ui.suporta) return;
  if (ocupado() || (!trailer_aberto() && video_ativo())) { ui.erro = 1; ui.toast = SDL_GetTicks(); return; }
  if (trailer_aberto()) { ui.som = trailer_com_som(); trailer_fechar(); }
  int k = mus_previa_iniciar(&ui.p, &ui.a, faixa, shuffle, SDL_GetTicks(), SDL_GetTicks(), 0);
  if (k >= 0) { ui.atualizada = ui.retentou = 0; ui.indisponivel[k] = 0; ui.pendente = 1; ui.mudou = SDL_GetTicks(); ui.foco = 5 + k; }
}
void musicaui_evento(const SDL_Event *e) {
  if (!ui.aberta || e->type != SDL_KEYDOWN) return;
  SDL_Keycode k = e->key.keysym.sym;
  if (nv_tecla_voltar(e)) {
    if (ui.p.ativa) parar(); else voltar(); return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (e->key.repeat) return;
    if (ui.p.ativa) { parar(); return; }
    if (ui.foco == 0) voltar();
    else if (ui.foco == 3) {
      ui.guardado = mus_alternar_guardado(ui.imdb, ui.temp, ui.a.titulo, ui.a.artista);
      ui.erro = 0; ui.toast = SDL_GetTicks();
    } else if (ui.foco == 1 || ui.foco == 2) tocar(-1, ui.foco == 2);
    else if (ui.foco >= 5) tocar(ui.foco - 5, 0);
    return;
  }
  int f = ui.foco;
  if (f >= 5) {
    if (k == SDLK_UP) ui.foco = f == 5 ? (ui.suporta ? 1 : 3) : f - 1;
    if (k == SDLK_DOWN && f < ui.a.n + 4) ui.foco++;
    if (k == SDLK_LEFT) ui.foco = ui.suporta ? 1 : 3;
    if (k == SDLK_RIGHT && ui.p.ativa) ui.foco = 4;
  } else if (f == 4) {
    if (k == SDLK_LEFT || k == SDLK_UP) ui.foco = 5 + ui.p.faixa;
  } else {
    if (k == SDLK_UP) ui.foco = 0;
    if (k == SDLK_DOWN) ui.foco = f == 0 ? (ui.suporta ? 1 : 3) : 5;
    if (k == SDLK_LEFT) ui.foco = ui.suporta ? (f > 0 ? f - 1 : 0) : 0;
    if (k == SDLK_RIGHT) ui.foco = ui.suporta ? (f < 3 ? f + 1 : 3) : 3;
  }
  if (f != ui.foco) ui.mudou = SDL_GetTicks();
}
static int pagina(void) {
  return ui.foco >= 5 ? (ui.foco - 5) / MUS_POR_PAGINA :
         ui.p.ativa ? ui.p.faixa / MUS_POR_PAGINA : 0;
}
void musicaui_atualizar(Uint32 agora) {
  if (!musicaui_visivel()) return;
  float dt=aberturaTick ? (agora-aberturaTick)*.001f : 0;
  aberturaTick=agora;
  abertura=anim_mola2_reduzida(&aberturaVel,abertura,ui.aberta ? 1 : 0,dt,18,ajustes_animacoes_reduzidas());
  if (!ui.aberta && abertura<.001f) { musicaui_fechar(); return; }
  ui.agora = agora;
  if (ui.p.ativa) {
    if (ocupado() || trailer_aberto()) { parar(); return; }
    if (ui.pendente) {
      // Wait for asynchronous trailer release; never overlap native sessions.
      if (video_ativo()) {
        if (agora - ui.p.desde > 2000u) parar();
        return;
      }
      // Deezer album metadata can outlive signed CDN URLs by weeks.
      if (strstr(ui.a.faixa[ui.p.faixa].previa, ".dzcdn.net/") && !ui.atualizada) {
        if (!ui.previaPedido) ui.previaPedido = mus_previa_pedir(&ui.a, ui.p.faixa);
        char url[768];
        int estado = ui.previaPedido ? mus_previa_resultado(ui.previaPedido, url, sizeof url) : -1;
        if (estado == 1 && ui.retentou && !strcmp(url, ui.a.faixa[ui.p.faixa].previa)) estado = -1;
        if (!estado) return;
        ui.previaPedido = 0;
        if (estado < 0) { ui.indisponivel[ui.p.faixa] = 1; parar(); return; }
        snprintf(ui.a.faixa[ui.p.faixa].previa, sizeof ui.a.faixa[0].previa, "%s", url);
        ui.atualizada = 1;
      }
      video_definir_cabecalhos(strstr(ui.a.faixa[ui.p.faixa].previa, ".dzcdn.net/") ?
          "User-Agent: Mozilla/5.0\nReferer: https://www.deezer.com/" : "");
      video_definir_mp4(1); video_definir_modo_live(2);
      if (!video_tocar(ui.a.faixa[ui.p.faixa].previa)) {
        ui.indisponivel[ui.p.faixa] = 1;
        video_parar(); video_volume(ui.som ? 100 : 0); parar(); return;
      }
      video_volume(100); ui.volumePronto = 0; ui.pendente = 0; ui.p.desde = agora;
    } else {
      // uMS and TPK may ignore volume before their load/playing event.
      if (!ui.volumePronto && (video_pronto() || video_tocando())) {
        video_volume(100); ui.volumePronto = 1;
      }
      int old = ui.p.faixa;
      if (video_falhou()) {
        if (!ui.retentou && strstr(ui.a.faixa[old].previa, ".dzcdn.net/")) {
          video_parar(); ui.retentou = 1; ui.atualizada = 0;
          ui.pendente = 1; ui.p.desde = agora; return;
        }
        ui.indisponivel[old] = 1;
      }
      int next = mus_previa_passo(&ui.p, agora, video_pos(), video_tocando() || video_pronto() || video_pos() > 0,
                                  video_falhou() ? MUS_PREV_ERRO : video_terminou() ? MUS_PREV_FIM : MUS_PREV_CONTINUA);
      if (next != old) {
        video_parar(); video_volume(ui.som ? 100 : 0);
        if (next >= 0) { ui.atualizada = ui.retentou = 0; ui.pendente = 1; ui.foco = 5 + next; ui.mudou = agora; }
        else if (ui.foco == 4) ui.foco = ui.suporta ? 1 : 3;
      }
    }
  }
  for (int i = 0; i < MUS_POR_PAGINA; i++) {
    float alvo = ui.foco == 5 + pagina() * MUS_POR_PAGINA + i ? 1 : 0;
    float step = ajustes_animacoes_reduzidas() ? 1 : 1.0f / 10.8f;
    float d = alvo - ui.focoLinha[i];
    ui.focoLinha[i] += d > step ? step : d < -step ? -step : d;
  }
}
static float ease(float t) { if (t < 0) return 0; if (t > 1) return 1; return 1 - powf(1 - t, 5); }
static void texto(TxtEstilo estilo, const char *s, float x, float y, float w, float alpha) {
  TxtLinha l = txt_linha_corta(estilo, s, 243, 242, 239, 255, w);
  txt_desenhar_alpha(l, x, y, alpha);
}
static void icone(const char *nome, float x, float y, float size, float tinta) {
  gfx_icone((GfxRect){x,y,size,size}, nome, tinta, tinta, tinta, 1);
}
static void botao(int foco, const char *label, const char *icon, float x, float y, float w) {
  float r,g,b; ajustes_acento(&r,&g,&b);
  int ativo = ui.foco == foco;
  gfx_cor((GfxRect){x,y,w,60}, .5f, ativo ? r : 1, ativo ? g : 1, ativo ? b : 1, ativo ? 1 : .08f);
  int tinta = ativo ? ajustes_tinta_foco() : 243;
  icone(icon, x+26,y+17,25,tinta/255.0f);
  TxtLinha l = txt_linha(TXT_G22M, label, tinta,tinta,tinta,255);
  txt_desenhar_alpha(l,x+63,y+(60-l.h)*.5f,1);
}
// One cached cover gradient shared by the Detail card and album page.
GLuint musicaui_ambiente(const char *capa) {
  static char chave[512];
  static GLuint textura;
  if (strcmp(chave,capa)) {
    if (textura) { gfx_tex_esquecer(textura); glDeleteTextures(1,&textura); textura=0; }
    snprintf(chave,sizeof chave,"%s",capa);
  }
  if (!textura) {
    unsigned char px[16*16*4], ambient[32*18*4];
    if (!tex_cores(capa,px)) return 0;
    mus_cores(px,16,16,64,ambient);
    CorvivaPaleta paleta;
    tex_publicar_paleta(capa);
    if (corviva_paleta(capa,&paleta) && paleta.ok) {
      for (int y=0;y<18;y++) for (int x=0;x<32;x++) {
        float k=(x/31.0f+y/17.0f)*.5f;
        for (int c=0;c<3;c++) ambient[(y*32+x)*4+c]=(unsigned char)
          (255*.36f*(paleta.grad[0][c]*(1-k)+paleta.grad[2][c]*k));
      }
    }
    glGenTextures(1,&textura); glBindTexture(GL_TEXTURE_2D,textura);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,32,18,0,GL_RGBA,GL_UNSIGNED_BYTE,ambient);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  }
  return textura;
}
static void ilha(GfxRect r) {
  gfx_cor(r, r.h > 60 ? 26.0f/r.h : .5f, .055f,.059f,.071f, ajustes_vidro() ? .9f : 1);
}
// The album opens as a modal island (DESIGN.md modal radius 36) that grows
// out of the Detail card. Content is laid out 1:1 in a 1600x900 canvas pinned
// to the panel's top-left, so the close button never leaves the panel while
// it grows; the cover flies from the card as the shared element. Detail stays
// alive behind a light veil, and closing runs the same spring backwards.
#define MW 1600.0f
static const GfxRect MODAL = {160,90,1600,900};
static GfxRect misturar(GfxRect a, GfxRect b, float k) {
  return (GfxRect){a.x+(b.x-a.x)*k,a.y+(b.y-a.y)*k,a.w+(b.w-a.w)*k,a.h+(b.h-a.h)*k};
}
GfxRect musicaui_painel(void) {
  return misturar(origem.w>0 ? origem : (GfxRect){280,158,1360,765},MODAL,abertura);
}
static void rotulo(TxtEstilo e, const char *s, float x, float y, float w, float tom, float a, int direita) {
  TxtLinha l = txt_linha_corta(e,s,243,242,239,255,w);
  txt_desenhar_tom(l,direita ? x-l.w : x,y,tom,tom,tom,a);
}
void musicaui_desenhar(Uint32 agora) {
  if (!musicaui_visivel()) return;
  float previousScale=gfx_escala();
  gfx_escala_sair(1);
  float previousAlpha=gfx_opacidade_grupo;
  float k=abertura;
  GfxRect p=musicaui_painel();
  float s=p.w/MW, raio=(24+12*k)/p.h;
  gfx_sem_recorte();
  gfx_cor((GfxRect){0,0,1920,1080},0,0,0,0,(ajustes_vidro() ? .30f : .42f)*k);
  gfx_rect((GfxRect){p.x-p.w*.03f,p.y+p.h*.02f,p.w*1.06f,p.h*1.08f},0,GFX_SOMBRA,.36f,0,0,36/p.w,0,0,0,k);
  // Same material as the card, so the morph reads as one surface.
  gfx_cor(p,raio,.04f,.05f,.07f,1);
  GLuint capa = tex_obter_larg(ui.a.capa, 600);
  GLuint ambiente=musicaui_ambiente(ui.a.capa);
  if (ambiente) gfx_rect(p,ambiente,GFX_ARTE,0,0,0,raio,0,0,0,ajustes_vidro() ? .9f : 1);
  gfx_anel(p,raio,1,1,1,1,.09f);
  GfxRecorteAnt corte=gfx_recorte_dentro(p.x,p.y,p.w,p.h);
  gfx_transformar(0,0,s,p.x,p.y);
  gfx_opacidade_grupo=previousAlpha*(ajustes_animacoes_reduzidas() ? k : ease((k-.4f)/.6f));
  float ent = ajustes_animacoes_reduzidas() ? 1 : ease((agora-ui.entrada)/360.0f);
  float header = ajustes_animacoes_reduzidas() ? 1 : ease(((float)(agora-ui.entrada)-80)/300.0f);
  const float cx = 72, cy = 124, cs = 380, hy = 16*(1-header);
  char buf[512];
  ilha((GfxRect){40,36,56,56}); icone("aj_x",56,52,24,1);
  if (ui.foco == 0) gfx_cor((GfxRect){40,36,56,56},.5f,1,1,1,.2f);
  txt_tracking(TXT_MUS_KICK,i18n("A MÚSICA POR TRÁS DA HISTÓRIA"),243,242,239,cx,536,.7f*ent,2.24f);
  const char *note = i18n("Reviva a história, uma faixa de cada vez.");
  if (!strcmp(ui.imdb,"tt0468569")) note = i18n("Da primeira nota de tensão ao último acerto de contas em Gotham.");
  if (!strcmp(ui.imdb,"tt0944947")) note = i18n("Volte a Westeros, um tema de cada vez.");
  float nh = txt_bloco_corta(TXT_G22M,note,243,242,239,cx,568,cs,33,ent*.78f,2);
  float phoneY = 568+nh+28;
  gfx_cor((GfxRect){cx,phoneY,cs,1},0,1,1,1,.1f);
  if (ui.qr) {
    gfx_cor((GfxRect){cx,phoneY+25,96,96},10.0f/96,.953f,.949f,.937f,1);
    gfx_rect((GfxRect){cx+7,phoneY+32,82,82},ui.qr,GFX_SNAP,0,0,0,0,0,0,0,1);
    texto(TXT_G20M,i18n("Abrir no celular"),cx+116,phoneY+36,cs-116,1);
    texto(TXT_MUS_17,"Spotify / Apple Music / YouTube",cx+116,phoneY+69,cs-116,.72f);
    texto(TXT_MUS_17,i18n("Aponte a câmera para abrir o álbum"),cx+116,phoneY+96,cs-116,.72f);
  }
  const float x = 508, w = 1036, R = x+w;
  char kicker[160], season[64];
  snprintf(season,sizeof season,i18n("Temporada %d"),ui.temp);
  snprintf(kicker,sizeof kicker,"%s%s%s",ui.temp ? season : "",ui.temp ? " · " : "",i18n("Trilha sonora original"));
  for (char *p=kicker; *p; p++) if (*p>='a' && *p<='z') *p-=32;
  txt_tracking(TXT_MUS_KICK,kicker,243,242,239,x,40+hy,.72f*header,2.4f);
  texto(TXT_MUS_TIT,ui.titulo,x,66+hy,w,header);
  texto(TXT_MUS_ART,ui.a.artista,x,138+hy,w,.92f*header);
  int dur = 0; for (int i = 0; i < ui.a.n; i++) dur += ui.a.faixa[i].dur;
  char tracks[64], minutes[64];
  snprintf(tracks,sizeof tracks,i18n("%d faixas"),ui.a.n);
  snprintf(minutes,sizeof minutes,i18n("%d min"),(dur+30)/60);
  snprintf(buf,sizeof buf,"%d · %s · %s · %s",ui.a.ano ? ui.a.ano : ui.ano,i18n("Trilha sonora"),tracks,minutes);
  texto(TXT_G20M,buf,x,178+hy,w,.75f*header);
  if (ui.suporta) {
    botao(1,i18n("Tocar prévias"),"aj_play",x,214,219);
    botao(2,i18n("Aleatório"),"aj_shuffle",x+231,214,176);
  }
  TxtLinha save = txt_linha(TXT_G22M,i18n(ui.guardado ? "Salvo" : "Salvar"),243,242,239,255);
  botao(3,i18n(ui.guardado ? "Salvo" : "Salvar"),ui.guardado ? "aj_check" : "aj_bookmark",R-save.w-89,214,save.w+89);
  // Dark well under the track list: rows stay readable on any album colour.
  gfx_cor((GfxRect){x-16,294,w+32,494},26.0f/494,.03f,.035f,.045f,.62f);
  char upper[256];
  snprintf(upper,sizeof upper,"%s",i18n("Faixas"));
  for (char *p=upper;*p;p++) if(*p>='a'&&*p<='z')*p-=32;
  txt_tracking(TXT_MUS_16,upper,243,242,239,x+20,314,.78f,1.6f);
  snprintf(buf,sizeof buf,"%s · %s",i18n("Prévia"),i18n("Duração"));
  for (char *p=buf;*p;p++) if(*p>='a'&&*p<='z')*p-=32;
  TxtLinha cols = txt_linha(TXT_MUS_16,buf,243,242,239,255);
  txt_desenhar_alpha(cols,R-20-cols.w,314,.78f);
  gfx_cor((GfxRect){R-20-cols.w-138,306,120,34},8.0f/34,.055f,.059f,.071f,.72f); texto(TXT_MUS_16,"Apple Music",R-20-cols.w-125,314,106,1);
  int page = pagina(), first = page*MUS_POR_PAGINA;
  float ar,ag,ab; ajustes_acento(&ar,&ag,&ab);
  float tinta = ajustes_tinta_foco()/243.0f;
  for (int j = 0; j < MUS_POR_PAGINA && first+j < ui.a.n; j++) {
    int k = first+j, playing = ui.p.ativa && ui.p.faixa == k;
    float y = 344+j*58+hy, f = ui.focoLinha[j], tom = 1+(tinta-1)*f;
    // Focused row takes the accent fill like the buttons, text flips to the focus ink.
    if (f > 0) gfx_cor((GfxRect){x,y,w,58},20.0f/58,ar,ag,ab,f);
    if (f < 1) gfx_cor((GfxRect){x+82,y+57,w-102,1},0,1,1,1,.12f*(1-f));
    if (playing) {
      for (int bar = 0; bar < 4; bar++) {
        float h = ajustes_animacoes_reduzidas() ? 16 : 7+16*(.5f+.5f*sinf(agora*(6.283185f/(600+bar*110))+bar));
        gfx_cor((GfxRect){x+31+bar*8,y+29-h*.5f,4,h},.5f,f>.5f?tom:ar,f>.5f?tom:ag,f>.5f?tom:ab,1);
      }
    } else { snprintf(buf,sizeof buf,"%d",ui.a.faixa[k].n ? ui.a.faixa[k].n : k+1); rotulo(TXT_G20M,buf,x+36,y+18,45,tom,.8f,0); }
    TxtLinha title = txt_linha_corta(TXT_MUS_TRACK,ui.a.faixa[k].titulo,243,242,239,255,w-322);
    int acc = playing && f < .5f;
    txt_desenhar_tom(title,x+82,y+1,acc?ar:tom,acc?ag:tom,acc?ab:tom,header);
    rotulo(TXT_G18R,ui.a.faixa[k].artista,x+82,y+33,w-322,tom,.8f*header,0);
    const char *tag = ui.suporta && ui.a.faixa[k].previa[0] && !ui.indisponivel[k] ? "30 s" : i18n("Indisponível");
    rotulo(TXT_G18R,tag,R-94,y+19,200,tom,.8f,1);
    snprintf(buf,sizeof buf,"%d:%02d",ui.a.faixa[k].dur/60,ui.a.faixa[k].dur%60);
    rotulo(TXT_G20M,buf,R-20,y+18,80,tom,.9f,1);
  }
  int pages = (ui.a.n+6)/7;
  float dx = x+w*.5f-(pages*16+14)*.5f;
  for (int j=0;j<pages;j++) { gfx_cor((GfxRect){dx,766,j==page?20:6,6},.5f,j==page?ar:1,j==page?ag:1,j==page?ab:1,j==page?1:.27f); dx+=j==page?30:16; }
  snprintf(buf,sizeof buf,i18n("%d–%d de %d"),first+1,first+7<ui.a.n?first+7:ui.a.n,ui.a.n);
  TxtLinha range = txt_linha(TXT_MUS_16,buf,243,242,239,255); txt_desenhar_alpha(range,R-20-range.w,760,.78f);
  if (ui.p.ativa) {
    float anim = ajustes_animacoes_reduzidas() ? 1 : ease((agora-ui.mudou)/220.0f);
    float y = 800+12*(1-anim);
    ilha((GfxRect){x,y,w,76});
    if(capa) gfx_rect((GfxRect){x+18,y+10,56,56},capa,GFX_ARTE,0,0,0,8.0f/56,0,0,0,1);
    texto(TXT_G20M,ui.a.faixa[ui.p.faixa].titulo,x+92,y+12,430,1);
    texto(TXT_MUS_16,i18n("Tocando prévia de 30 segundos"),x+92,y+43,430,.75f);
    double sec = ui.pendente ? 0 : video_pos(); if(sec<0)sec=0;if(sec>30)sec=30;
    gfx_cor((GfxRect){x+540,y+36,260,4},.5f,1,1,1,.16f);
    gfx_cor((GfxRect){x+540,y+36,260*(float)sec/30,4},.5f,ar,ag,ab,1);
    snprintf(buf,sizeof buf,"0:%02d / 0:30",(int)sec);
    txt_glifos(TXT_MUS_17,buf,243,242,239,x+820,y+28,.75f);
    gfx_cor((GfxRect){R-66,y+14,48,48},.5f,1,1,1,ui.foco==4?.2f:.08f);
    icone("mus_square",R-52,y+28,20,1);
  }
  if (ui.toast && agora-ui.toast<3500u && (ui.guardado || ui.erro)) {
    float width = ui.erro ? 528 : 310, tx = R-width;
    gfx_cor((GfxRect){tx,36,width,84},28.0f/84,.055f,.059f,.071f,ajustes_vidro()?.94f:1);
    icone("aj_check",tx+26,65,24,.85f);
    texto(TXT_G20M,i18n(ui.erro ? "Pare o vídeo antes de tocar prévias" : "Álbum salvo"),tx+65,54,width-85,1);
    if (!ui.erro) texto(TXT_MUS_17,i18n("Encontre em Listas › Músicas"),tx+65,85,width-85,.72f);
  }
  gfx_cor((GfxRect){cx,808,36,28},.25f,1,1,1,.12f); texto(TXT_G14B,"OK",cx+8,815,36,1);
  texto(TXT_G18R,i18n(ui.p.ativa ? "Parar prévia" : ui.suporta ? "Tocar prévia de 30 segundos" : "Selecionar"),cx+45,813,cs-45,.75f);
  TxtLinha back = txt_linha(TXT_G14B,i18n("Voltar"),243,242,239,255);
  gfx_cor((GfxRect){cx,846,back.w+16,28},.25f,1,1,1,.12f); txt_desenhar_alpha(back,cx+8,853,1);
  texto(TXT_G18R,i18n("Voltar ao título"),cx+back.w+25,851,cs-back.w-25,.75f);
  gfx_sem_transformar();
  gfx_recorte_voltar(corte);
  gfx_opacidade_grupo=previousAlpha;
  GfxRect fim={p.x+cx*s,p.y+cy*s,cs*s,cs*s}, ini=fim;
  float q=origem.w/960, ri=26*s;
  if (origem.w>0) { ini=(GfxRect){origem.x+28*q,origem.y+46*q,200*q,200*q}; ri=12*q; }
  GfxRect art=misturar(ini,fim,k);
  gfx_rect((GfxRect){art.x-art.w*.055f,art.y+art.w*.022f,art.w*1.11f,art.w*1.155f},0,GFX_SOMBRA,.36f,0,0,.052f,0,0,0,k);
  if (capa) gfx_rect(art,capa,GFX_ARTE,0,0,0,(ri+(26*s-ri)*k)/art.w,0,0,0,1);
  gfx_escala_sair(previousScale);
}
