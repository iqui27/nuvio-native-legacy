// #492: one decoder-free warm request. All entry points except the worker are
// called on the app thread. The network job owns cancellation while unwinding.
#ifndef NV_RETOMAMORNA_H
#define NV_RETOMAMORNA_H
void retomamorna_foco(const char *url);
void retomamorna_cancelar(void);
// Android snapshot in the video overlay; other backends have no capture API.
void cacheboost_backend_morno(int ligado);
int cacheboost_backend_quadro(int guardar); // 1 capture, 2 show, 3 visible, 0 forget
#endif
