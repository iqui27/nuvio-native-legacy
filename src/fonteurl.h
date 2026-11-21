#ifndef NV_FONTEURL_H
#define NV_FONTEURL_H
#include <stddef.h>
// The worker only downloads into a private staging file. FreeType admission
// and applying the font still belong to legfontes on the UI thread.
int fonteurl_iniciar(const char *url,const char *pasta);
int fonteurl_resultado(char *evento,size_t n);
void fonteurl_cancelar(void);
#endif
