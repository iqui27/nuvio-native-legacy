#ifndef NV_BADGES_H
#define NV_BADGES_H
#include <stdint.h>
void badges_carregar(const char *dir);
uint64_t badges_detectar(const char *metadata);
// Rotulo da fonte, sem afirmar o modo HDR ativo no painel. NULL sem HDR.
const char *badges_fonte_hdr(uint64_t mask);
uint64_t badges_provedor(const char *name);
float badges_desenhar(uint64_t mask,float x,float y,float maxW,float height,float alpha);
// A MESMA fileira em tinta ESCURA, para desenhar SOBRE superficie clara
// (linha selecionada). Ver a nota em badges.c.
float badges_desenhar_escura(uint64_t mask,float x,float y,float maxW,float height,float alpha);
#endif
