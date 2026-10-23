// Synopsis-only typography and the full-text reader (#477).
#ifndef NV_SINOPSE_H
#define NV_SINOPSE_H
#include "text.h"
#include <SDL2/SDL.h>

TxtEstilo sinopse_estilo(TxtEstilo original);
float sinopse_entrelinha(TxtEstilo original, float leading);
// Compact previews use fewer lines at larger sizes, retaining at least one.
// Containers reserve the chosen leading; the full reader has no line cap.
int sinopse_linhas(TxtEstilo original, float leading, int maxLinhas);
float sinopse_bloco(TxtEstilo original, const char *texto, int r, int g, int b,
                    float x, float y, float larg, float leading, float alpha, int maxLinhas);
float sinopse_bloco_corta(TxtEstilo original, const char *texto, int r, int g, int b,
                          float x, float y, float larg, float leading, float alpha, int maxLinhas);
void sinopse_abrir(const char *titulo, const char *texto);
void sinopse_comentario(const char *autor, const char *texto, const char *data,
                        int nota, int curtidas, int respostas, int spoiler,
                        int posicao, int total);
int sinopse_comentario_aberto(void);
void sinopse_fechar(void);
int sinopse_aberta(void);
void sinopse_evento(const SDL_Event *e);
// Draw last: owns its pointer layer, clipped text area and fixed Close button.
void sinopse_desenhar(void);
#endif
