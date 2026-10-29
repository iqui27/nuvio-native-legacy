// Included once by each native backend after defining FonteContexto,
// IPTV_CTX_INICIALIZAR and IPTV_CTX_EMERGENCIA. Contexts remain allocated so
// workers never retain pointers to an account object freed by the UI.
#include <stdatomic.h>
#include <limits.h>
static pthread_mutex_t fontesTrava = PTHREAD_MUTEX_INITIALIZER;
static FonteContexto *fontesContextos;
static FonteContexto fonteEmergencia = IPTV_CTX_EMERGENCIA;
static atomic_uint contaEditada;
typedef struct {
  unsigned explicita, lidaId;
  FonteContexto *lida;
  char nome[64];
  int retry;
} FonteFio;
#if defined(NV_TPK40) || defined(NV_UM_FIO)
static pthread_key_t fonteFioChave;
static pthread_once_t fonteFioUma = PTHREAD_ONCE_INIT;
static void fonteFioCriar(void) {
  if (pthread_key_create(&fonteFioChave, free)) abort();
}
static FonteFio *fonteFio(void) {
  pthread_once(&fonteFioUma, fonteFioCriar);
  FonteFio *f = pthread_getspecific(fonteFioChave);
  if (!f) {
    f = calloc(1, sizeof *f);
    if (!f) abort(); // Never share account selection between workers on OOM.
    f->explicita = f->lidaId = UINT_MAX;
    if (pthread_setspecific(fonteFioChave, f)) { free(f); abort(); }
  }
  return f;
}
#else
static FonteFio *fonteFio(void) {
  static _Thread_local FonteFio f = { .explicita = UINT_MAX, .lidaId = UINT_MAX };
  return &f;
}
#endif
#define fonteExplicita (fonteFio()->explicita)
#define fonteLidaId (fonteFio()->lidaId)
#define fonteLida (fonteFio()->lida)

static FonteContexto *contexto(void) {
  unsigned id = fonteExplicita == UINT_MAX ? atomic_load(&contaEditada) : fonteExplicita;
  if (fonteLida && fonteLidaId == id) return fonteLida;
  pthread_mutex_lock(&fontesTrava);
  FonteContexto *c = fontesContextos;
  while (c && c->sid != id) c = c->proximo;
  if (!c) {
    c = calloc(1, sizeof *c);
    if (c) {
      c->sid = id; IPTV_CTX_INICIALIZAR(c);
      c->proximo = fontesContextos; fontesContextos = c;
    } else c = &fonteEmergencia; // Never borrow another account on allocation failure.
  }
  pthread_mutex_unlock(&fontesTrava);
  fonteLidaId = id; fonteLida = c;
  return c;
}
static unsigned usarFonte(unsigned id) {
  unsigned anterior = fonteExplicita; fonteExplicita = id; return anterior;
}
#undef IPTV_CTX_INICIALIZAR
#undef IPTV_CTX_EMERGENCIA
