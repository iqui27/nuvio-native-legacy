/* Transport/thread-creation control only; actual sync.c remains unchanged. */
#include <pthread.h>
int sync_teste_criar(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
#define pthread_create sync_teste_criar
