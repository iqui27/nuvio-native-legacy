// emscripten/threading.h de teste: o harness roda no "fio principal", entao
// so a declaracao importa.
#ifndef NV_TEST_EMSCRIPTEN_THREADING_H
#define NV_TEST_EMSCRIPTEN_THREADING_H
int emscripten_is_main_browser_thread(void);
#endif
