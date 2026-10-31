#ifndef NV_DESKTOP_INPUT_LOG_H
#define NV_DESKTOP_INPUT_LOG_H
#ifdef NV_DESKTOP_UI
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
// Numeric input diagnostics only. Never include catalog or account strings.
static inline FILE *desktop_input_file(void) {
  const char *home=getenv("HOME");
  if (!home) return NULL;
  char path[4096], old[4104];
  snprintf(path,sizeof path,"%s/Library/Logs/Nuvio",home);
  mkdir(path,0700);
  snprintf(path,sizeof path,"%s/Library/Logs/Nuvio/input.log",home);
  struct stat st;
  if (!stat(path,&st) && st.st_size>=2*1024*1024) {
    snprintf(old,sizeof old,"%s.1",path);rename(path,old);
  }
  return fopen(path,"a");
}
// Gesture decisions: momentum source, swipe start/action, sidebar toggle.
static inline void desktop_input_note(const char *format, ...) {
  FILE *f=desktop_input_file();
  if (!f) return;
  struct timespec now;clock_gettime(CLOCK_REALTIME,&now);
  fprintf(f,"time=%.3f ",now.tv_sec+now.tv_nsec/1e9);
  va_list args;va_start(args,format);vfprintf(f,format,args);va_end(args);
  fputc('\n',f);fclose(f);
}
static inline void desktop_input_log(const SDL_Event *e, float x, float y,
                                     int row, int item, float offset) {
  FILE *f=desktop_input_file();
  if (!f) return;
  int wheel=e->type==SDL_MOUSEWHEEL;
  int native=wheel && e->wheel.which==NV_DESKTOP_PIXEL_WHEEL;
  int ph=e->wheel.y, packedPhases=(ph&~0xffff)==0;
  int ww=0,wh=0,dw=0,dh=0;
  SDL_Window *window=SDL_GetWindowFromID(wheel?e->wheel.windowID:e->button.windowID);
  if(window) { SDL_GetWindowSize(window,&ww,&wh);SDL_GL_GetDrawableSize(window,&dw,&dh); }
  struct timespec now;clock_gettime(CLOCK_REALTIME,&now);
  fprintf(f,"time=%.3f event=%s button=%u window=%u point=%d,%d layout=%.4f,%.4f row=%d item=%d precise=%d phase=%u momentumPhase=%u delta=%.6f,%.6f offset=%.6f windowSize=%d,%d drawable=%d,%d\n",
    now.tv_sec+now.tv_nsec/1e9,
    wheel?"scroll":e->type==SDL_MOUSEBUTTONDOWN?"down":"up",
    wheel?0:e->button.button,wheel?e->wheel.windowID:e->button.windowID,
    wheel?e->wheel.mouseX:e->button.x,wheel?e->wheel.mouseY:e->button.y,
    x,y,row,item,native && !(packedPhases && (ph&(1<<16))),packedPhases?ph&255:0,packedPhases?(ph>>8)&255:0,
    wheel?e->wheel.preciseX:0,wheel?e->wheel.preciseY:0,offset,ww,wh,dw,dh);
  fclose(f);
}
#endif
#endif
