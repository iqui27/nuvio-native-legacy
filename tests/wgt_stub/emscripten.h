// emscripten.h FINGIDO para tests/wgt_voz.c: o EM_ASM vira uma chamada C que
// faz o papel do window.nvTexto do tools/tizen-shell.html. So serve para
// testar o caminho __EMSCRIPTEN__ de src/entrada_texto.c sem o emsdk.
#ifndef NV_WGT_STUB_EMSCRIPTEN_H
#define NV_WGT_STUB_EMSCRIPTEN_H
int wgt_em_int(const char *js, ...);
void wgt_em(const char *js, ...);
#define EM_ASM_INT(code, ...) wgt_em_int(#code, ##__VA_ARGS__)
#define EM_ASM(code, ...) wgt_em(#code, ##__VA_ARGS__)
#endif
