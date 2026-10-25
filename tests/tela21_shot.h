#include "gfx.h"
#include "plrui.h"
#include "ponteiro.h"
void t21_checked_cor(GfxRect, float, float, float, float, float);
void t21_checked_material(GfxRect, float, int, float);
void t21_checked_button(GfxRect, float);
void t21_checked_target(float, float, float, float, PonteiroFn, PonteiroFn, int, int);
void t21_checked_transform(float, float, float, float, float);
void t21_checked_untransform(void);
#define gfx_cor t21_checked_cor
#define plrui_material t21_checked_material
#define plrui_botao_repouso t21_checked_button
#define ponteiro_alvo t21_checked_target
#define gfx_transformar t21_checked_transform
#define gfx_sem_transformar t21_checked_untransform
