#ifndef NV_GUIAV5UI_H
#define NV_GUIAV5UI_H
#include <SDL2/SDL.h>
#include "catalogo.h"
#include "guiav5.h"
void g5ui_open(const char *screen);
void g5ui_close(void);
void g5ui_forget(void);
int g5ui_active(void);
int g5ui_event(const SDL_Event *e);
void g5ui_update(void);
int g5ui_draw(void);
int g5ui_changed(void);
int g5ui_play(CatItem *item, char *url, size_t cap);
int g5ui_channel_allowed(const char *id, const char *name, const char *group, const char *source);
int g5ui_check_channel(const CatItem *item);
int g5ui_channel_hidden(const char *id,const char *name,const char *group,const char *source);
int g5ui_live(const char *id, char *url, size_t cap);
int g5ui_multiview_channel(CatItem *item);
int g5ui_m3u_channels(G5Item *out, int cap);
const char *g5ui_xmltv(void);
int g5ui_provider_epg(void);
void g5ui_multiview_rect(float *x,float *y,float *w,float *h);
#endif
