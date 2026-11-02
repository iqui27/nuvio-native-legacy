#include "gfx.h"
#include "plrui.h"
#include "ponteiro.h"
#include "botoes.h"
void t21_checked_cor(GfxRect, float, float, float, float, float);
void t21_checked_material(GfxRect, float, int, float);
void t21_checked_button(GfxRect, float);
void t21_checked_target(float, float, float, float, PonteiroFn, PonteiroFn, int, int);
void t21_checked_transform(float, float, float, float, float);
void t21_checked_untransform(void);
// Filmography pills draw through botoes.c since 555f019fe.
void t21_checked_pill(GfxRect, const char *, const char *, float, int, int, float);
#define gfx_cor t21_checked_cor
#define plrui_material t21_checked_material
#define plrui_botao_repouso t21_checked_button
#define botao_pilula_sem_aro t21_checked_pill
#define ponteiro_alvo t21_checked_target
#define gfx_transformar t21_checked_transform
#define gfx_sem_transformar t21_checked_untransform
