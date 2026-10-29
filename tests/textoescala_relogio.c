// Linked into the keyboard shot by tests/textoescala_prova.sh. The keyboard
// shows the last key's label for one second of wall clock (kicker, teclado.c):
// a slower frame flips a capture. This clock advances 16 ms per cleared frame,
// so both trees see the same instant at every capture.
#include <SDL.h>

extern unsigned nvspy_quadros;
Uint32 SDL_GetTicks(void) { return 1000u + nvspy_quadros * 16u; }
