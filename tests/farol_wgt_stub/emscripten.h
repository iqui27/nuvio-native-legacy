// emscripten.h de teste para o alvo .wgt do Farol (issue 9). O VPS nao tem
// emcc; EM_JS aqui vira so uma DECLARACAO de funcao C — o corpo JS nao roda.
// tests/farol_produtores_tv.c implementa nv_av() em C devolvendo o que o
// webapis.avplay da TV devolveria, e por isso este stub cobre unicamente o
// que src/video_tizen.c inclui.
#ifndef NV_TEST_EMSCRIPTEN_H
#define NV_TEST_EMSCRIPTEN_H
#define EM_JS(ret, name, params, ...) ret name params;
#define EM_ASM(...)
#define EM_ASM_INT(...)
#endif
