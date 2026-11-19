// A rejected/missing capture directory must never fall back to any profile.
#include "../src/dados.c"
#include <assert.h>
#include <sys/wait.h>
int main(void) {
  const char *base=getenv("NUVIO_TESTE_DIR");assert(base && *base);
  char home[700],missing[700],fallback[720],link[700];
  snprintf(home,sizeof home,"%s/home",base);assert(mkdir(home,0700)==0);
  snprintf(missing,sizeof missing,"%s/missing/data",base);
  snprintf(fallback,sizeof fallback,"%s/.nuvio",home);
  snprintf(link,sizeof link,"%s/link",base);assert(symlink(base,link)==0);
  assert(setenv("HOME",home,1)==0);assert(setenv("NV_CAPTURA","1",1)==0);
  for(int mode=0;mode<3;mode++) {
    pid_t child=fork();assert(child>=0);
    if(!child) {
      if(mode==0) unsetenv("NUVIO_DADOS");
      else setenv("NUVIO_DADOS",mode==1?missing:link,1);
      dados_iniciar(NULL);_exit(99);
    }
    int status;assert(waitpid(child,&status,0)==child);
    assert(WIFEXITED(status) && WEXITSTATUS(status)==78);
    assert(access(fallback,F_OK)!=0);
  }
  assert(setenv("NUVIO_DADOS",base,1)==0);
  dados_iniciar(NULL);assert(!strcmp(dados_dir(),base));
  puts("PASS: capture rejects missing, unwritable and symlink data; never falls back");
}
