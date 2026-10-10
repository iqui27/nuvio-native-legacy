// #412: video_tpk.c real, relogio controlado e host sem TV.
#include "../src/video_tpk.c"
#include <assert.h>
static Uint32 agora;
Uint32 SDL_GetTicks(void) { return agora; }
const char *i18n(const char *s) { return s; }
const char *ling_nome(const char *s) { return s; }
const char *ling_audio(void) { return ""; }
int ling_casa(const char *a, const char *b) { (void)a;(void)b;return 0; }
const char *ling_do_nome(const char *s) { (void)s;return NULL; }
int ling_letreiro(const char *s,int n) { (void)s;(void)n;return 0; }
void mkvass_aceitar_texto(int s) { (void)s; }
int mkvass_cabecalho(const char *u,unsigned char **b,long *n) { (void)u;(void)b;(void)n;return 0; }
int mkv_faixas(const char *u,MkvFaixa *f,int n) { (void)u;(void)f;(void)n;return 0; }
void rede_lateral(int s) { (void)s; }
void capmkv_iniciar(const char *s) { (void)s; }
void capmkv_zerar(void) {}
int ling_tipo_legenda(const char *s,int f,int d) { (void)s;(void)f;(void)d;return 0; }
const char *ling_tipo_legenda_rotulo(int t) { (void)t;return ""; }
static int aberturas, paradas, posicao, busca;
static void abrir(const char *u,const char *c) { (void)c;assert(!strcmp(u,"http://test/movie"));aberturas++; }
static void parar(void) { paradas++; }
static void buscar(int ms) { busca=ms; }
static int pos(void) { return posicao; }
static void passo(unsigned ms) { agora+=ms;video_bombear(); }
static void prontoHost(void) { nv_tpk_video_evento(1,3600000,0);nv_tpk_video_evento(2,0,0); }
int main(void) {
  nv_tpk_video_registrar(abrir,parar,NULL,buscar,NULL,NULL,pos);
  video_definir_mp4(1); // nao ha sonda lateral nesta fixture
  video_definir_reconexao(1);video_tocar("http://test/movie");prontoHost();posicao=1334892;passo(1);
  // Pausa e buffering, inclusive sem um quadro entre os eventos, nao sao travas.
  nv_tpk_video_evento(3,0,0);passo(20000);assert(!paradas);
  nv_tpk_video_evento(2,0,0);passo(1);passo(14000);assert(!paradas);
  nv_tpk_video_evento(7,50,0);passo(20000);assert(!paradas);
  nv_tpk_video_evento(7,100,0);passo(1);passo(14000);assert(!paradas);
  video_buscar(1334.892);passo(14000);assert(!paradas);passo(2000);
  assert(paradas==1 && video_reconectando()==1);
  passo(3000);assert(aberturas==2);prontoHost();posicao=0;passo(1);assert(busca==1334892);
  posicao=1334892;passo(1);
  // Sem progresso real as tentativas acabam, em vez de um loop infinito.
  for(int n=2;n<=3;n++) {
    passo(15001);assert(recon.tentativa==n);passo(n==2?10000:25000);
    prontoHost();posicao=0;passo(1);posicao=1334892;passo(1);
  }
  passo(15001);assert(video_falhou() && paradas==4);
  video_parar();int antes=aberturas;passo(60000);assert(aberturas==antes);
  puts("ok: pausa/buffer/seek, retomada na posicao, limite e cancelamento");
}
