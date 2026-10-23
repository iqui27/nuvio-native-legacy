#include "musicaui.h"
#include "video.h"
#include "player.h"
#include "trailer.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
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
  GLuint qr, ambiente;
  int coresProntas;
  float focoLinha[MUS_POR_PAGINA];
} ui;

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
  mus_previa_parar(&ui.p); ui.pendente = 0;
  if (ui.foco == 4) ui.foco = ui.suporta ? 1 : 3;
}
void musicaui_fechar(void) {
  parar(); ui.aberta = 0; trailer_bloquear_musica(0);
  if (ui.qr) { gfx_tex_esquecer(ui.qr); glDeleteTextures(1, &ui.qr); ui.qr = 0; }
  if (ui.ambiente) { gfx_tex_esquecer(ui.ambiente); glDeleteTextures(1, &ui.ambiente); ui.ambiente = 0; }
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
  ui.entrada = ui.mudou = SDL_GetTicks();
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
  if (k >= 0) { ui.pendente = 1; ui.mudou = SDL_GetTicks(); ui.foco = 5 + k; }
}
void musicaui_evento(const SDL_Event *e) {
  if (!ui.aberta || e->type != SDL_KEYDOWN) return;
  SDL_Keycode k = e->key.keysym.sym;
  if (nv_tecla_voltar(e)) {
    if (ui.p.ativa) parar(); else musicaui_fechar(); return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (e->key.repeat) return;
    if (ui.p.ativa) { parar(); return; }
    if (ui.foco == 0) musicaui_fechar();
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
  if (!ui.aberta) return;
  ui.agora = agora;
  if (!ui.coresProntas) {
    unsigned char px[16*16*4], ambient[32*18*4];
    if (tex_cores(ui.a.capa, px)) {
      mus_cores(px, 16, 16, 64, ambient);
      glGenTextures(1, &ui.ambiente); glBindTexture(GL_TEXTURE_2D, ui.ambiente);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 18, 0, GL_RGBA, GL_UNSIGNED_BYTE, ambient);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      ui.coresProntas = 1;
    }
  }
  if (ui.p.ativa) {
    if (ocupado() || trailer_aberto()) { parar(); return; }
    if (ui.pendente) {
      // Wait for asynchronous trailer release; never overlap native sessions.
      if (video_ativo()) {
        if (agora - ui.p.desde > 2000u) parar();
        return;
      }
      video_definir_cabecalhos("");
      video_definir_mp4(1); video_definir_modo_live(2);
      if (!video_tocar(ui.a.faixa[ui.p.faixa].previa)) {
        video_parar(); video_volume(ui.som ? 100 : 0); parar(); return;
      }
      video_volume(100); ui.volumePronto = 0; ui.pendente = 0; ui.p.desde = agora;
    } else {
      // uMS and TPK may ignore volume before their load/playing event.
      if (!ui.volumePronto && (video_pronto() || video_tocando())) {
        video_volume(100); ui.volumePronto = 1;
      }
      int old = ui.p.faixa;
      int next = mus_previa_passo(&ui.p, agora, video_pos(), video_tocando() || video_pronto() || video_pos() > 0,
                                  video_falhou() ? MUS_PREV_ERRO : video_terminou() ? MUS_PREV_FIM : MUS_PREV_CONTINUA);
      if (next != old) {
        video_parar(); video_volume(ui.som ? 100 : 0);
        if (next >= 0) { ui.pendente = 1; ui.foco = 5 + next; ui.mudou = agora; }
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
static void ilha(GfxRect r) {
  gfx_cor(r, r.h > 60 ? 26.0f/r.h : .5f, .055f,.059f,.071f, ajustes_vidro() ? .9f : 1);
}
void musicaui_desenhar(Uint32 agora) {
  if (!ui.aberta) return;
  gfx_sem_recorte();
  gfx_cor((GfxRect){0,0,1920,1080},0,.031f,.051f,.09f,1);
  GLuint capa = tex_obter_larg(ui.a.capa, 600);
  if (ui.ambiente) gfx_rect((GfxRect){0,0,1920,1080},ui.ambiente,GFX_SNAP,0,1,0,0,0,0,0,1);
  float ent = ajustes_animacoes_reduzidas() ? 1 : ease((agora-ui.entrada)/360.0f);
  float header = ajustes_animacoes_reduzidas() ? 1 : ease(((float)(agora-ui.entrada)-80)/300.0f);
  float cx = 108 - 28*(1-ent), cy = 185, hy = 16*(1-header);
  ilha((GfxRect){72,48,56,56}); icone("mus_back",88,64,24,1);
  char buf[512];
  const char *crumbs[] = {i18n(ui.temp ? "Séries" : "Filmes"), ui.titulo, i18n("Música")};
  float bx = 152;
  for (int i = 0; i < 3; i++) {
    if (i) { texto(TXT_G22M,"/",bx,64,20,.3f); bx += 34; }
    TxtLinha l = txt_linha(TXT_G22M,crumbs[i],243,242,239,255);
    txt_desenhar_alpha(l,bx,64,i==2?1:.66f); bx+=l.w+22;
  }
  time_t now = time(NULL); struct tm tm; localtime_r(&now,&tm); strftime(buf,sizeof buf,"%H:%M",&tm);
  ilha((GfxRect){1740,48,108,56}); texto(TXT_G20M,buf,1764,65,80,1);
  if (ui.foco == 0) gfx_cor((GfxRect){72,48,56,56},.5f,1,1,1,.13f);
  gfx_rect((GfxRect){cx-25,cy+10,500,520},0,GFX_SOMBRA,.36f,0,0,.052f,0,0,0,ent);
  if (capa) gfx_rect((GfxRect){cx,cy,450,450},capa,GFX_ARTE,0,0,0,26.0f/450,0,0,0,ent);
  txt_tracking(TXT_MUS_KICK,i18n("A MÚSICA POR TRÁS DA HISTÓRIA"),243,242,239,cx,673,.66f*ent,2.24f);
  const char *note = i18n("Reviva a história, uma faixa de cada vez.");
  if (!strcmp(ui.imdb,"tt0468569")) note = i18n("Da primeira nota de tensão ao último acerto de contas em Gotham.");
  if (!strcmp(ui.imdb,"tt0944947")) note = i18n("Volte a Westeros, um tema de cada vez.");
  float nh = txt_bloco_corta(TXT_G22M,note,243,242,239,cx,710,425,33,ent*.66f,2);
  float phoneY = 710+nh+32;
  gfx_cor((GfxRect){108,phoneY,450,1},0,1,1,1,.1f);
  if (ui.qr) {
    gfx_cor((GfxRect){108,phoneY+25,96,96},10.0f/96,.953f,.949f,.937f,1);
    gfx_rect((GfxRect){115,phoneY+32,82,82},ui.qr,GFX_SNAP,0,0,0,0,0,0,0,1);
    texto(TXT_G20M,i18n("Abrir no celular"),224,phoneY+36,330,1);
    texto(TXT_MUS_17,"Spotify / Apple Music / YouTube",224,phoneY+69,340,.66f);
    texto(TXT_MUS_17,i18n("Aponte a câmera para abrir o álbum"),224,phoneY+96,340,.66f);
  }
  const float x = 650, w = 1162;
  char kicker[160], season[64];
  snprintf(season,sizeof season,i18n("Temporada %d"),ui.temp);
  snprintf(kicker,sizeof kicker,"%s%s%s",ui.temp ? season : "",ui.temp ? " · " : "",i18n("Trilha sonora original"));
  for (char *p=kicker; *p; p++) if (*p>='a' && *p<='z') *p-=32;
  txt_tracking(TXT_MUS_KICK,kicker,243,242,239,x,165+hy,.66f*header,2.4f);
  texto(TXT_MUS_TIT,ui.titulo,x,197+hy,w,header);
  texto(TXT_MUS_ART,ui.a.artista,x,272+hy,w,.9f*header);
  int dur = 0; for (int i = 0; i < ui.a.n; i++) dur += ui.a.faixa[i].dur;
  char tracks[64], minutes[64];
  snprintf(tracks,sizeof tracks,i18n("%d faixas"),ui.a.n);
  snprintf(minutes,sizeof minutes,i18n("%d min"),(dur+30)/60);
  snprintf(buf,sizeof buf,"%d · %s · %s · %s",ui.a.ano ? ui.a.ano : ui.ano,i18n("Trilha sonora"),tracks,minutes);
  texto(TXT_G20M,buf,x,316+hy,w,.66f*header);
  if (ui.suporta) {
    botao(1,i18n("Tocar prévias"),"aj_play",x,370,219);
    botao(2,i18n("Aleatório"),"aj_shuffle",881,370,176);
  }
  TxtLinha save = txt_linha(TXT_G22M,i18n(ui.guardado ? "Salvo" : "Salvar"),243,242,239,255);
  botao(3,i18n(ui.guardado ? "Salvo" : "Salvar"),ui.guardado ? "aj_check" : "aj_bookmark",1812-save.w-89,370,save.w+89);
  char upper[256];
  snprintf(upper,sizeof upper,"%s",i18n("Faixas"));
  for (char *p=upper;*p;p++) if(*p>='a'&&*p<='z')*p-=32;
  txt_tracking(TXT_MUS_16,upper,243,242,239,670,455,.66f,1.6f);
  gfx_cor((GfxRect){1490,447,120,34},8.0f/34,.055f,.059f,.071f,.72f); texto(TXT_MUS_16,"Apple Music",1503,455,106,1);
  snprintf(buf,sizeof buf,"%s · %s",i18n("Prévia"),i18n("Duração"));
  for (char *p=buf;*p;p++) if(*p>='a'&&*p<='z')*p-=32;
  texto(TXT_MUS_16,buf,1630,455,183,.66f);
  int page = pagina(), first = page*MUS_POR_PAGINA;
  float ar,ag,ab; ajustes_acento(&ar,&ag,&ab);
  for (int j = 0; j < MUS_POR_PAGINA && first+j < ui.a.n; j++) {
    int k = first+j, playing = ui.p.ativa && ui.p.faixa == k;
    float y = 482+j*56+hy, f = ui.focoLinha[j];
    if (f > 0) gfx_cor((GfxRect){650,y,1162,56},20.0f/56,1,1,1,.13f*f);
    else gfx_cor((GfxRect){732,y+55,1060,1},0,1,1,1,.09f);
    if (playing) {
      for (int bar = 0; bar < 4; bar++) {
        float h = ajustes_animacoes_reduzidas() ? 16 : 7+16*(.5f+.5f*sinf(agora*(6.283185f/(600+bar*110))+bar));
        gfx_cor((GfxRect){681+bar*8,y+28-h*.5f,4,h},.5f,ar,ag,ab,1);
      }
    } else { snprintf(buf,sizeof buf,"%d",ui.a.faixa[k].n ? ui.a.faixa[k].n : k+1); texto(TXT_G20M,buf,686,y+17,45,.66f); }
    TxtLinha title = txt_linha_corta(TXT_MUS_TRACK,ui.a.faixa[k].titulo,243,242,239,255,830);
    txt_desenhar_tom(title,732,y+1,playing?ar:1,playing?ag:1,playing?ab:1,header);
    texto(TXT_MUS_17,ui.a.faixa[k].artista,732,y+31,830,.66f*header);
    const char *tag = ui.suporta && ui.a.faixa[k].previa[0] ? "30 s" : i18n("Indisponível");
    TxtLinha lt = txt_linha(TXT_MUS_16,tag,243,242,239,255);
    txt_desenhar_alpha(lt,1718-lt.w,y+20,.66f);
    snprintf(buf,sizeof buf,"%d:%02d",ui.a.faixa[k].dur/60,ui.a.faixa[k].dur%60);
    lt = txt_linha(TXT_G20M,buf,243,242,239,255); txt_desenhar_alpha(lt,1792-lt.w,y+17,.66f);
  }
  int pages = (ui.a.n+6)/7;
  float dx = 1231-(pages*16+14)*.5f;
  for (int j=0;j<pages;j++) { gfx_cor((GfxRect){dx,892,j==page?20:6,6},.5f,j==page?ar:1,j==page?ag:1,j==page?ab:1,j==page?1:.27f); dx+=j==page?30:16; }
  snprintf(buf,sizeof buf,i18n("%d–%d de %d"),first+1,first+7<ui.a.n?first+7:ui.a.n,ui.a.n);
  TxtLinha range = txt_linha(TXT_MUS_16,buf,243,242,239,255); txt_desenhar_alpha(range,1792-range.w,886,.66f);
  if (ui.p.ativa) {
    float anim = ajustes_animacoes_reduzidas() ? 1 : ease((agora-ui.mudou)/220.0f);
    float y = 914+12*(1-anim);
    ilha((GfxRect){650,y,1162,88});
    if(capa) gfx_rect((GfxRect){672,y+16,56,56},capa,GFX_ARTE,0,0,0,8.0f/56,0,0,0,1);
    texto(TXT_G20M,ui.a.faixa[ui.p.faixa].titulo,746,y+18,460,1);
    texto(TXT_MUS_16,i18n("Tocando prévia de 30 segundos"),746,y+49,460,.66f);
    double sec = ui.pendente ? 0 : video_pos(); if(sec<0)sec=0;if(sec>30)sec=30;
    gfx_cor((GfxRect){1220,y+42,320,4},.5f,1,1,1,.16f);
    gfx_cor((GfxRect){1220,y+42,320*(float)sec/30,4},.5f,ar,ag,ab,1);
    snprintf(buf,sizeof buf,"0:%02d / 0:30",(int)sec);
    txt_glifos(TXT_MUS_17,buf,243,242,239,1560,y+34,.66f);
    gfx_cor((GfxRect){1742,y+20,48,48},.5f,1,1,1,ui.foco==4?.2f:.08f);
    icone("mus_square",1756,y+34,20,1);
  }
  if (ui.toast && agora-ui.toast<3500u && (ui.guardado || ui.erro)) {
    float width = ui.erro ? 528 : 310, tx = 1848-width;
    gfx_cor((GfxRect){tx,114,width,84},28.0f/84,.055f,.059f,.071f,ajustes_vidro()?.94f:1);
    icone("aj_check",tx+26,143,24,.85f);
    texto(TXT_G20M,i18n(ui.erro ? "Pare o vídeo antes de tocar prévias" : "Álbum salvo"),tx+65,132,width-85,1);
    if (!ui.erro) texto(TXT_MUS_17,i18n("Encontre em Listas › Músicas"),tx+65,163,width-85,.66f);
  }
  gfx_cor((GfxRect){108,1010,36,28},.25f,1,1,1,.09f); texto(TXT_G14B,"OK",116,1017,36,1);
  texto(TXT_G18R,i18n(ui.p.ativa ? "Parar prévia" : ui.suporta ? "Tocar prévia de 30 segundos" : "Selecionar"),153,1015,450,.66f);
  gfx_cor((GfxRect){409,1010,50,28},.25f,1,1,1,.09f); texto(TXT_G14B,i18n("Voltar"),416,1017,50,1);
  texto(TXT_G18R,i18n("Voltar ao título"),468,1015,350,.66f);
  texto(TXT_MUS_16,i18n("Trilha original · Prévias de áudio"),1595,1017,290,.66f);
}
