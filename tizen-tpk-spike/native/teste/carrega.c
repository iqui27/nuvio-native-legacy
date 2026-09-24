// Prova, sem TV, que libnvspike.so carrega numa userland ARM soft-float com
// glibc antiga (container arm32v5/debian:buster, glibc 2.28) e que os testes
// 1 e 2 passam. Roda em tools/tizen-tpk-spike.sh. As funcoes GL vem de um
// libGLESv2.so.2 falso gerado a partir dos simbolos que a .so pede.
#include <dlfcn.h>
#include <stdio.h>

int main(void) {
  void *h = dlopen("./libnvspike.so", RTLD_NOW);
  if (!h) { printf("dlopen FALHOU: %s\n", dlerror()); return 1; }
  int (*ping)(void) = (int (*)(void))dlsym(h, "nv_ping");
  int (*thr)(void) = (int (*)(void))dlsym(h, "nv_thread_test");
  if (!ping || !thr) { printf("simbolo ausente\n"); return 1; }
  int p = ping(), t = thr();
  printf("nv_ping=%d nv_thread_test=%d\n", p, t);
  return (p == 42 && t == 1) ? 0 : 1;
}
