#ifndef NV_NAV21_TEST_H
#define NV_NAV21_TEST_H
#include "gfx.h"
#include <SDL2/SDL.h>
#define NAV21_MAX 512
typedef struct { int id; GfxRect r; } Nav21Target;
typedef struct {
  const char *name;
  void (*open)(void);
  void (*key)(const SDL_Event *);
  void (*step)(float);
  void (*draw)(void);
  int (*focus)(void);
  void (*select)(int);
  int (*page)(void);
  int primary;
} Nav21Screen;
void nav21_target(int id, GfxRect rect);
void nav21_check(Nav21Screen screen);
void nav21_expect(const char *screen, const char *path, int actual, int expected);
void nav21_servers(void);
void nav21_achievements(void);
void nav21_start(void);
#endif
