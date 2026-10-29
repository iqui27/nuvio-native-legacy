// Supply a second real tab without network metadata.
#define extras_nota fixture_rating
#define extras_n_colecao fixture_collection_count
#define extras_colecao_titulo fixture_collection_title
#define extras_agenda_temporada fixture_agenda_season
#define extras_agenda_episodio fixture_agenda_episode
#include "../src/detail.c"
int fixture_agenda_season(void) { return getenv("NV_ANCHOR_UPCOMING") ? 1 : 0; }
int fixture_agenda_episode(void) { return getenv("NV_ANCHOR_UPCOMING") ? 1 : 0; }
int fixture_rating(int source) { return source==EX_IMDB?80:0; }
int fixture_collection_count(void) { return 2; }
const char *fixture_collection_title(int i) { return i?"Second film":"First film"; }
#include "escala_ancoras.h"
void fixture_detail(float scale,int section) {
  static const char *names[3][5]={{"detail-modern-hero","detail-modern-seasons","detail-modern-episodes","detail-modern-tabs","detail-modern-cast"},{"detail-standard-hero","detail-standard-seasons","detail-standard-episodes","detail-standard-tabs","detail-standard-cast"},{"detail-dynamic-hero","detail-dynamic-seasons","detail-dynamic-episodes","detail-dynamic-tabs","detail-dynamic-cast"}};
  int lay=ajustes_home_layout();
  float old=gfx_escala_txt_entrar();
  if(lay==HOME_LAYOUT_PADRAO) {
    shot_begin("detail-standard-reservation",scale);
    shot_check("text layer reserves rendered thumbnail",fabsf(epCardH()-epThumbH())<.01f);
    float ui=gfx_escala_ui_base();
    gfx_escala_ui_definir(1.2f);
    float layer=gfx_escala_entrar();
    shot_check("combined layer reserves rendered thumbnail",fabsf(epCardH()-epThumbH())<.01f);
    gfx_escala_sair(layer);
    gfx_escala_ui_definir(ui);
  }
  idx=0; aberto=1; t=1; saindo=0; carro=0; nivel=section<0?0:1; pg=section<0?0:1;
  temporada=0; abaInfo=0; foco.fileira=section<0?0:section; foco.coluna=0;
  memset(animFoco,0,sizeof animFoco); memset(scrollSec,0,sizeof scrollSec);
  recalcularLayout(); scrollY=section<0?0:conteudoSec[section]-ANC(280);
  // Make the real hero text gate fully open, without network or wall-clock waits.
  gateHero.inicio=1; gateHero.pronto=1;
  for(int f=0;f<20;f++) textogate_passo(&gateHero,0,SDL_GetTicks()+10000);
  gfx_escala_sair(old);
  for(int f=0;f<14;f++) { txt_novo_quadro(); tex_bombear(8); SDL_Delay(1); detail_desenhar(10000); }
  shot_begin(names[lay][section+1],scale); detail_desenhar(10000); shot_end();
  old=gfx_escala_txt_entrar();
  if(section<0) {
    shot_reset(); heroWeb(1,0);
    shot_no_text_overlap();
    GfxRect hero=shot_text_bounds(); shot_anchor("hero bottom",hero,1);
    // Draw the real season row in the SAME document, shifted together into view.
    shot_reset(); scrollY=ANC(800); heroWeb(1,-scrollY);
    GfxRect copy=shot_text_bounds();
    shot_reset(); desenhaSecao(SEC_TEMPORADAS,1,10000);
    GfxRect seasons=shot_text_bounds();
    shot_disjoint("hero / season selector",copy,seasons);
  } else {
    // Redraw the focused production section alone to exclude adjacent content.
    shot_reset(); desenhaSecao(section,1,10000);
    shot_no_text_overlap();
    GfxRect content=shot_text_bounds();

    if(section==SEC_TEMPORADAS) {
      GfxRect row=shot_box((GfxRect){xItem(section,1),epTempY()-scrollY,larguraItem(section,1),NV_DETP_TEMP_H});
      shot_anchor("season selector",row,0);
    }
    shot_check("focused section visible",content.y>=0 && content.y<900 && content.w>0);
    if(section==SEC_ABAS_INFO) {
      GfxRect label=shot_text(txt_linha(TXT_PLR_CORPO,ABA_ROTULO[abaIdDe(0)],255,255,255,255));
      // Labels center within the same fixed section origin.
      shot_anchor("tabs section",(GfxRect){ANC(NV_DETP_X)*scale,
        (epAbasY()-scrollY)*scale,0,0},0);
      shot_check("tab label rendered below section top",label.y>=(epAbasY()-scrollY)*scale);
    }
    if(section==SEC_EPISODIOS) {
      if(epModerna() || epApple()) {
        GfxRect text=shot_text_bounds();
        shot_check("episode text fits card height",
          text.y+text.h <= (menuOrigem.y+epCardH())*scale+1);
      }
      GfxRect card=menuOrigem;
      card=shot_box((GfxRect){card.x,card.y,card.w,epThumbH()});
      shot_check("episode artwork fits reserved section",card.y+card.h<=(menuOrigem.y+epCardH())*scale+1);
      shot_anchor("episode card",card,0);
      float documentScroll=scrollY;
      shot_reset(); scrollY=epAbasY()-ANC(800);
      desenhaSecao(SEC_EPISODIOS,1,10000); GfxRect episodes=shot_text_bounds();
      shot_reset(); desenhaSecao(SEC_ABAS_INFO,1,10000); GfxRect tabs=shot_text_bounds();
      shot_disjoint("episode copy / following tabs",episodes,tabs);
      scrollY=documentScroll;
    }
  }
  gfx_escala_sair(old); aberto=0; t=0;
}
