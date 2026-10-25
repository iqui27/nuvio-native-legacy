// Arte 3D e tinta de cada conquista (deploy/app/art/conquistas/<objeto>-<material>.png).
// A tinta so vale nos cards e no detalhe de Conquistas; o resto do app usa ajustes_acento().
#ifndef NV_CONQUISTASARTE_H
#define NV_CONQUISTASARTE_H
#include <stddef.h>

typedef struct { int r, g, b; } CqTinta;

// Objeto da conquista: "director", "noir", "top250"... (chave + familia).
const char *cqarte_objeto(const char *chave, int familia);
// Material pelo nivel: 0 locked, 1 bronze, 2-3 silver, 4-5 gold.
const char *cqarte_material(int nivel);
// "director-n-gold.png"; diretor sem inicial pronta cai no clapperboard generico.
int  cqarte_arquivo(const char *chave, int familia, int nivel, char *dst, size_t n);
// Cor do acento do objeto; nivel 0 = cinza neutro.
CqTinta cqarte_tinta(const char *chave, int familia, int nivel);

#endif
