/* Nativo apenas. Incluido em dados.c para usar a mesma politica nas gravacoes
 * e nos testes leves que ja compilam dados.c sem o restante do app. */
#ifndef __EMSCRIPTEN__
#define NV_ESPACO_IMPLEMENTADO 1
#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <sys/statvfs.h>
#if (defined(NV_TPK) || defined(NV_WEBOS)) && defined(__arm__)
_Static_assert(sizeof(off_t)==8, "dados.c precisa de -D_FILE_OFFSET_BITS=64 (espaco)");
#endif
#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif
#define ESPACO_MB (1024ULL * 1024ULL)
#define ESPACO_CACHE (20 * ESPACO_MB)
static pthread_mutex_t espacoTrava = PTHREAD_MUTEX_INITIALIZER;
static char espacoApp[PATH_MAX];
static int espacoIniciado;

typedef struct { char caminho[1200]; uint64_t bytes; time_t quando; } EspArquivo;
static int espacoPrefixo(const char *n, const char *p) { return !strncmp(n,p,strlen(p)); }
static int espacoSufixo(const char *n, const char *s) {
  size_t a=strlen(n), b=strlen(s); return a>=b && !strcmp(n+a-b,s);
}
static int espacoPacote(const char *n) {
  return (espacoPrefixo(n,"Nuvio-") || espacoPrefixo(n,"NuvioTV-") || espacoPrefixo(n,"libnuvio-")) &&
    (espacoSufixo(n,".apk") || espacoSufixo(n,".ipk") || espacoSufixo(n,".so"));
}
static int espacoVersaoAtual(const char *nome, const char *versao) {
  const char *prefixos[]={"Nuvio-","NuvioTV-","libnuvio-"};
  const char *sufixos[]={".apk",".ipk",".so"}; char atual[128];
  for(int i=0;i<3;i++) {
    snprintf(atual,sizeof atual,"%s%s%s",prefixos[i],versao,sufixos[i]);
    if(!strcmp(nome,atual)) return 1;
  }
  return 0;
}
static int espacoMaisVelho(const void *a, const void *b) {
  const EspArquivo *x=a,*y=b;
  if(x->quando != y->quando) return x->quando < y->quando ? -1 : 1;
  return strcmp(x->caminho,y->caminho);
}
/* ponytail: caches atuais planos; percorrer recursivamente se ganharem subpastas.
 * So arquivos regulares, nunca links nem gravacoes ainda em curso. */
static void espacoPodar(const char *dir, const char *prefixo, uint64_t teto,
                         size_t max, const char *atual) {
  struct stat pasta;
  if(lstat(dir,&pasta)!=0 || !S_ISDIR(pasta.st_mode)) return;
  DIR *d=opendir(dir); struct dirent *e;
  EspArquivo *lista=NULL; size_t n=0; uint64_t total=0;
  if(!d) return;
  while((e=readdir(d))) {
    char p[1200]; struct stat st; EspArquivo *nova;
    if(e->d_name[0]=='.' || espacoSufixo(e->d_name,".tmp") ||
       (prefixo ? !espacoPrefixo(e->d_name,prefixo) : !espacoPacote(e->d_name))) continue;
    if(snprintf(p,sizeof p,"%s/%s",dir,e->d_name)>=(int)sizeof p ||
       lstat(p,&st)!=0 || !S_ISREG(st.st_mode)) continue;
    nova=realloc(lista,(n+1)*sizeof *lista);
    if(!nova) { free(lista); closedir(d); return; }
    lista=nova; snprintf(lista[n].caminho,sizeof lista[n].caminho,"%s",p);
    lista[n].bytes=(uint64_t)st.st_size; lista[n].quando=st.st_mtime;
    total+=lista[n].bytes; n++;
  }
  closedir(d);
  if(n>1) qsort(lista,n,sizeof *lista,espacoMaisVelho);
  size_t restam=n;
  uint64_t alvo=total>teto ? teto-teto/4 : teto; /* mesma folga de podarCacheDisco */
  for(size_t i=0;i<n && (total>alvo || restam>max);i++) {
    const char *nome=strrchr(lista[i].caminho,'/')+1;
    if(atual && espacoVersaoAtual(nome,atual)) continue;
    if(unlink(lista[i].caminho)==0) { total-=lista[i].bytes; restam--; }
  }
  free(lista);
}
static void espacoLimpar(const char *d, const char *versao) {
  char p[1200];
  if(!d || !d[0]) return;
  espacoPodar(d,"mkvass-",ESPACO_CACHE,512,NULL); /* ASS, texto e fontes */
  espacoPodar(d,"noticias-",ESPACO_CACHE,512,NULL);
  espacoPodar(d,"noticias2-",ESPACO_CACHE,512,NULL);
  espacoPodar(d,"extras-",ESPACO_CACHE,512,NULL);
  snprintf(p,sizeof p,"%s/trailer",d); espacoPodar(p,"",ESPACO_CACHE,512,NULL);
  snprintf(p,sizeof p,"%s/extras",d); espacoPodar(p,"",ESPACO_CACHE,512,NULL);
  snprintf(p,sizeof p,"%s/atualizacao",d);
  /* Versao em execucao + a mais recente restante, independente do mtime
   * da atual (o instalador pode preservar a data do pacote). */
  if(versao) espacoPodar(p,NULL,UINT64_MAX,2,versao);
}
static int espacoGrupo(const char *n) {
  if(!strcmp(n,"cache") || !strcmp(n,"art")) return 0;
  if(espacoPrefixo(n,"mkvass-") || !strcmp(n,"legendas")) return 1;
  if(espacoPrefixo(n,"catalogo")) return 2;
  if(!strcmp(n,"trailer")) return 3;
  if(!strcmp(n,"atualizacao") || espacoPrefixo(n,"libnuvio.staged") ||
      espacoPrefixo(n,"libnuvio.previous") || !strcmp(n,"libnuvio.download")) return 4;
  if(!strcmp(n,"p2p")) return 5;
  if(espacoSufixo(n,".log") || espacoSufixo(n,".log.1") || espacoPrefixo(n,"tpk-etapas")) return 6;
  return 7;
}
static void espacoSomar(const char *p, const char *ignorar, int grupo,
                         unsigned fundo, uint64_t *totais) {
  struct stat st;
  if(fundo>32 || (ignorar && !strcmp(p,ignorar)) || lstat(p,&st)!=0) return;
  if(S_ISREG(st.st_mode)) { totais[grupo<0 ? 7 : grupo]+=(uint64_t)st.st_blocks*512; return; }
  if(!S_ISDIR(st.st_mode)) return;
  DIR *d=opendir(p); struct dirent *e;
  if(!d) return;
  while((e=readdir(d))) {
    char f[1200];
    if(!strcmp(e->d_name,".") || !strcmp(e->d_name,"..")) continue;
    if(snprintf(f,sizeof f,"%s/%s",p,e->d_name)>=(int)sizeof f) continue;
    espacoSomar(f,ignorar,grupo<0 ? (!strcmp(e->d_name,"res") ? -1 : espacoGrupo(e->d_name)) : grupo,fundo+1,totais);
  }
  closedir(d);
}
static void espacoRelatar(FILE *f, const char *dados, const char *app, double livre) {
  uint64_t t[8]={0}; char d[PATH_MAX],a[PATH_MAX]="";
  if(dados && realpath(dados,d)) espacoSomar(d,NULL,-1,0,t); else d[0]=0;
  if(app && realpath(app,a) && strcmp(a,d) &&
      !(d[0] && !strncmp(a,d,strlen(d)) && a[strlen(d)]=='/')) espacoSomar(a,d[0]?d:NULL,-1,0,t);
#ifdef NV_TPK
  // O pacote fica em apps_ro e data/ em apps_rw. Inclui os irmaos de data/
  // (cache do host/player etc.) como "outros", sem varrer a home no desktop.
  char rw[PATH_MAX]; snprintf(rw,sizeof rw,"%s",d);
  char *barra=strrchr(rw,'/'); if(barra) *barra=0;
  const char *pkg=strstr(rw,"/apps_rw/");
  if(pkg && pkg[9] && !strchr(pkg+9,'/') && strcmp(rw,a))
    espacoSomar(rw,d,7,0,t);
#endif
  // LG grava logs em /tmp, fora das duas arvores. Conta cada um so uma vez.
  const char *logs[]={getenv("NUVIO_LOG"),getenv("NUVIO_LOG_ANTERIOR")};
  for(int i=0;i<2;i++) {
    char p[PATH_MAX];
    if(!logs[i] || !realpath(logs[i],p)) continue;
    if((d[0] && !strncmp(p,d,strlen(d)) && (p[strlen(d)]=='/' || !p[strlen(d)])) ||
       (a[0] && !strncmp(p,a,strlen(a)) && (p[strlen(a)]=='/' || !p[strlen(a)]))) continue;
    if(i && logs[0] && !strcmp(logs[0],logs[1])) continue;
    espacoSomar(p,NULL,6,0,t);
  }
  fprintf(f,"[espaco] livre=%.1f app: arte=%.1f legendas=%.1f catalogo=%.1f trailer=%.1f atualizacao=%.1f p2p=%.1f logs=%.1f outros=%.1f\n",
      livre,t[0]/(double)ESPACO_MB,t[1]/(double)ESPACO_MB,t[2]/(double)ESPACO_MB,
      t[3]/(double)ESPACO_MB,t[4]/(double)ESPACO_MB,t[5]/(double)ESPACO_MB,
      t[6]/(double)ESPACO_MB,t[7]/(double)ESPACO_MB);
  fflush(f);
}
/* Um log anterior por plataforma; maior que 2 MB e descartavel. Nunca toca
 * no arquivo aberto da sessao nem em arquivos pessoais. */
static void espacoLogs(void) {
  const char *anterior=getenv("NUVIO_LOG_ANTERIOR");
  if(!anterior || !*anterior) anterior="/tmp/nuvio-anterior.log";
  struct stat st;
  if(lstat(anterior,&st)==0 && S_ISREG(st.st_mode) && st.st_size>2*(long)ESPACO_MB) unlink(anterior);
}
static void *espacoFio(void *arg) {
  (void)arg; sleep(5);
  struct statvfs fs;
  double livre=statvfs(dados_dir(),&fs)==0 ? (double)fs.f_bavail*fs.f_frsize/ESPACO_MB : -1;
  espacoRelatar(stdout,dados_dir(),espacoApp,livre);
  pthread_mutex_lock(&espacoTrava);
  espacoLimpar(dados_dir(),NV_VERSAO);
  pthread_mutex_unlock(&espacoTrava);
  for(;;) {
    pthread_mutex_lock(&espacoTrava);
    espacoLimpar(dados_dir(),NULL); espacoLogs();
    pthread_mutex_unlock(&espacoTrava);
    sleep(30);
  }
  return NULL;
}
void dados_espaco_iniciar(const char *arte) {
  if(espacoIniciado || !dados_dir()[0]) return;
  /* LG: <app>/art; TPK: <app>/res/art. Android: assets extraidos. */
  if(arte && realpath(arte,espacoApp)) {
    char *b=strrchr(espacoApp,'/');
    if(b && !strcmp(b+1,"art")) {
      *b=0; b=strrchr(espacoApp,'/');
      if(b && !strcmp(b+1,"res")) *b=0;
    }
  }
  pthread_t t;
  if(pthread_create(&t,NULL,espacoFio,NULL)==0) { pthread_detach(t); espacoIniciado=1; }
}
static void espacoGravado(const char *nome) {
  if(!espacoPrefixo(nome,"mkvass-") && !espacoPrefixo(nome,"noticias-") &&
      !espacoPrefixo(nome,"noticias2-") && !espacoPrefixo(nome,"extras-")) return;
  pthread_mutex_lock(&espacoTrava);
  const char *p=espacoPrefixo(nome,"mkvass-")?"mkvass-":espacoPrefixo(nome,"noticias2-")?"noticias2-":espacoPrefixo(nome,"noticias-")?"noticias-":"extras-";
  espacoPodar(dados_dir(),p,ESPACO_CACHE,512,NULL);
  pthread_mutex_unlock(&espacoTrava);
}
#else
void dados_espaco_iniciar(const char *arte) { (void)arte; }
#endif
