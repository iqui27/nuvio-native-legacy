// Keep the real request wrappers and cancellation code; disable transport before workers start.
#include "../src/rede.c"
void nav21_offline(void){__atomic_store_n(&pronto,-1,__ATOMIC_RELEASE);}
