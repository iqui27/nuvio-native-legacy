// Capture refuses missing/unwritable isolation before trying any fallback.
#include "../src/dados.c"
#include <assert.h>
#include <sys/wait.h>

static void refuses(const char *path) {
  pid_t child=fork(); assert(child>=0);
  if(!child) {
    if(path) setenv("NUVIO_DADOS",path,1); else unsetenv("NUVIO_DADOS");
    dados_iniciar(NULL);
    _exit(99);
  }
  int status; assert(waitpid(child,&status,0)==child);
  assert(WIFEXITED(status) && WEXITSTATUS(status)==78);
}
int main(void) {
  const char *data=getenv("NUVIO_DADOS");assert(data && data[0]=='/');
  setenv("NV_CAPTURA","1",1);
  refuses(NULL);refuses("relative");refuses("/dev/null/no-data");
  dados_iniciar(NULL);assert(!strcmp(dados_dir(),data));
  puts("PASS: capture fails closed; explicit disposable data accepted");
}
