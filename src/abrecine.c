#include "abrecine.h"
#include "anim.h"

void abrecine_passo(AbreCine *a, int abrindo, int imagem, int detalhadaPadrao,
                    Uint32 agora, float dt, int reduzidas) {
  if (abrindo) {
    if (!a->viva) { a->detalhada = detalhadaPadrao; a->desde = agora; a->foco = 0.0f; a->calado = 1; }
    a->viva = 1;
    a->veu = 1.0f;
    // O recuo da arte leva ~1 s: o corte para a tela de espera nao se ve.
    a->foco = reduzidas ? 1.0f : anim_mola(a->foco, 1.0f, dt, 3.0f);
    return;
  }
  if (a->viva) a->fim = agora;
  a->viva = 0;
  if (a->veu <= 0.0f) return;
  // Sem quadro real a tela segura (no .tpk/LG o furo mostraria o vazio).
  if (!imagem && agora - a->fim < ABRECINE_SEGURA_MS) return;
  a->veu = reduzidas ? 0.0f : anim_mola(a->veu, 0.0f, dt, 6.0f);
  if (a->veu < 0.01f) a->veu = 0.0f;
}

int abrecine_osd_calado(const AbreCine *a) { return a->calado; }

void abrecine_acordou(AbreCine *a, int carregando) {
  if (!carregando && !a->viva) a->calado = 0;
}

int abrecine_cinema(const AbreCine *a) { return a->veu > 0.0f && !a->detalhada; }

int abrecine_demorando(const AbreCine *a, Uint32 agora) {
  return a->viva && agora - a->desde >= ABRECINE_DEMORA_MS;
}

int abrecine_tecla(AbreCine *a, SDL_Keycode k) {
  if (!a->viva || a->detalhada || (k != SDLK_UP && k != SDLK_DOWN)) return 0;
  a->detalhada = 1;
  return 1;
}

AbreCineEtapa abrecine_etapa(int esperandoFonte) {
  return esperandoFonte ? ABRECINE_RESOLVENDO : ABRECINE_CARREGANDO;
}
