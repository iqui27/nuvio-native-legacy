// #412: video_tpk.c real, relogio controlado e host sem TV.
#include "../src/video_tpk.c"
#include <assert.h>
static Uint32 agora;
Uint32 SDL_GetTicks(void) { return agora; }
// These fixtures never resolve a TorBox file link. Fail closed if requested.
int debrid_tb_link(int usenet, int item, int file, char *dst, unsigned n) {
  (void)usenet; (void)item; (void)file;
  if (n) dst[0] = 0;
  return 0;
}
int ajustes_audio_offset_ms(void) { return 0; }
void dts_remux_set_audio_offset_ms(int ms) { (void)ms; }
const char *i18n(const char *s) { return s; }
const char *ling_nome(const char *s) { return s; }
const char *ling_audio(void) { return ""; }
const char *ling_audio2(void) { return ""; }
const char *ling_original(void) { return ""; }
int ling_casa(const char *a, const char *b) { (void)a;(void)b;return 0; }
const char *ling_do_nome(const char *s) { (void)s;return NULL; }
int ling_letreiro(const char *s,int n) { (void)s;(void)n;return 0; }
void mkvass_aceitar_texto(int s) { (void)s; }
void mkvass_aceitar_imagem(int s) { (void)s; }
int mkvass_cabecalho(const char *u,unsigned char **b,long *n) { (void)u;(void)b;(void)n;return 0; }
int mkv_faixas(const char *u,MkvFaixa *f,int n) { (void)u;(void)f;(void)n;return 0; }
void rede_lateral(int s) { (void)s; }
void capmkv_iniciar(const char *s) { (void)s; }
void capmkv_zerar(void) {}
int ling_tipo_legenda(const char *s,int f,int d) { (void)s;(void)f;(void)d;return 0; }
const char *ling_tipo_legenda_rotulo(int t) { (void)t;return ""; }
static int aberturas, paradas, posicao, busca, pausa;
// Evento que o host emite durante o proprio hAbrir: 9 = EV_PRESO tardio da
// sessao velha; 5 = descarte recusado (EV_ERRO -3), n vezes.
static int abrirEvento, abrirVezes;
static void abrir(const char *u,const char *c) {
  (void)c;assert(!strcmp(u,"http://test/movie"));aberturas++;
  if (abrirVezes > 0) { abrirVezes--; nv_tpk_video_evento(abrirEvento, abrirEvento==5?-3:-1, 0); }
}
static void parar(void) { paradas++; }
static void pausar(int p) { pausa=p; }
static void buscar(int ms) { busca=ms; }
static int pos(void) { return posicao; }
static void passo(unsigned ms) { agora+=ms;video_bombear(); }
static void prontoHost(void) { nv_tpk_video_evento(1,3600000,0);nv_tpk_video_evento(2,0,0); }
int main(void) {
  nv_tpk_video_registrar(abrir,parar,pausar,buscar,NULL,NULL,pos);
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
  // InvalidOperation no pause: recuperar mesmo com pausaPedida, e reaplicar.
  video_definir_reconexao(1);video_tocar("http://test/movie");prontoHost();
  posicao=1337033;passo(1);video_pausar(1);
  int stops=paradas;nv_tpk_video_evento(9,-1,0);passo(1);
  assert(paradas==stops+1 && recon.pendente && recon.alvo==1337.033);
  passo(3000);prontoHost();passo(1);assert(pausa==1 && busca==1337033);
  nv_tpk_video_evento(3,0,0);assert(recon.tentativa && !tocando);
  video_pausar(0);assert(pausa==0);nv_tpk_video_evento(2,0,0);
  passo(1);assert(tocando && recon.tentativa && !falhou);
  video_pausar(1);nv_tpk_video_evento(3,0,0);
  video_buscar(1327.033);assert(busca==1327033 && recon.alvo==1327.033);
  video_pausar(0);assert(pausa==0);nv_tpk_video_evento(2,0,0);
  posicao=1327033;passo(1);assert(tocando && recon.tentativa && !falhou);
  video_parar();video_definir_reconexao(1);video_tocar("http://test/movie");prontoHost();
  posicao=1337033;passo(1);nv_tpk_video_evento(9,-1,0);passo(1);
  // Seek forward durante a recuperacao atualiza o alvo, nao volta ao stall.
  video_buscar(1347.033);video_pausar(0);passo(3000);prontoHost();passo(1);
  assert(busca==1347033);video_parar();
  // GetState/position de 5000 ms + relogio parado, mesmo em buffer/pausa pedida.
  video_definir_reconexao(1);video_tocar("http://test/movie");prontoHost();
  posicao=1337033;passo(1);video_pausar(1);nv_tpk_video_evento(7,50,0);
  // Player preso: cada consulta leva 5 s e avisa de novo.
  nv_tpk_video_evento(10,0,0);passo(1);stops=paradas;
  for(int i=0;i<3;i++){passo(5000);nv_tpk_video_evento(10,0,0);}passo(1);
  assert(paradas==stops+1 && recon.pendente);video_parar();
  // P2: UMA consulta lenta que cai numa pausa legitima nao arma o watchdog.
  video_definir_reconexao(1);video_tocar("http://test/movie");prontoHost();
  posicao=1337033;passo(1);video_pausar(1);nv_tpk_video_evento(3,0,0);
  nv_tpk_video_evento(10,0,0);stops=paradas;passo(1);for(int i=0;i<10;i++)passo(3000);
  assert(paradas==stops && !recon.pendente);video_parar();
  // P1-a: EV_PRESO pouco antes do Voltar nao reabre o video parado.
  video_definir_reconexao(1);video_tocar("http://test/movie");prontoHost();
  posicao=1337033;passo(1);nv_tpk_video_evento(9,-1,0);video_parar();
  antes=aberturas;stops=paradas;passo(1);passo(3000);passo(30000);
  assert(aberturas==antes && paradas==stops && !video_falhou());
  // P1-b: seek tocando que nunca conclui; o watchdog retoma no ALVO, nao no stall.
  video_definir_reconexao(1);video_tocar("http://test/movie");prontoHost();
  posicao=1337033;passo(1);video_buscar(1200.0);stops=paradas;
  passo(15001);passo(1);assert(paradas==stops+1 && recon.pendente && recon.alvo==1200.0);
  nv_tpk_video_evento(9,1200000,0);passo(1); // o prazo do host chega depois: so um hParar
  assert(paradas==stops+1);
  passo(3000);prontoHost();posicao=0;passo(1);assert(busca==1200000);
  posicao=1200000;passo(1);assert(tocando && !falhou);video_parar();
  // Seek perto da posicao antiga e um prazo atrasado tambem conservam o alvo.
  video_definir_reconexao(1);video_tocar("http://test/movie");prontoHost();
  posicao=1337033;passo(1);video_buscar(1338.0);stops=paradas;
  passo(22000);passo(1);assert(paradas==stops+1 && recon.alvo==1338.0);video_parar();
  // Confirmacao de um seek antigo nao apaga o alvo do pedido mais novo.
  video_definir_reconexao(1);video_tocar("http://test/movie");prontoHost();
  posicao=1337033;passo(1);video_buscar(1200.0);video_buscar(1210.0);
  nv_tpk_video_evento(11,1200000,0);assert(buscaAlvoMs==1210000);
  posicao=1210000;nv_tpk_video_evento(11,1210000,0);passo(1);
  assert(buscaAlvoMs==-1);nv_tpk_video_evento(9,-1,0);passo(1);
  assert(recon.alvo==1210.0);video_parar();
  // P2: watchdog + EV_PRESO no mesmo quadro chamam hParar uma vez so.
  video_definir_reconexao(1);video_tocar("http://test/movie");prontoHost();
  posicao=1337033;passo(1);stops=paradas;
  for(int i=0;i<2;i++){passo(5000);nv_tpk_video_evento(10,0,0);}
  agora+=5001;nv_tpk_video_evento(10,0,0);nv_tpk_video_evento(9,-1,0);
  passo(0);assert(paradas==stops+1);video_parar();
  // P2: EV_PRESO da sessao velha atravessando o hAbrir nao mata a nova.
  video_definir_reconexao(1);video_tocar("http://test/movie");prontoHost();
  posicao=1337033;passo(1);nv_tpk_video_evento(9,-1,0);passo(1);
  abrirEvento=9;abrirVezes=1;passo(3000);prontoHost();stops=paradas;passo(1);
  assert(paradas==stops && !recon.pendente && recon.tentativa==1);video_parar();
  // P1-c: descarte recusado (EV_ERRO -3) a cada reabertura: falhou limitado.
  video_definir_reconexao(1);video_tocar("http://test/movie");prontoHost();
  posicao=1337033;passo(1);nv_tpk_video_evento(9,-1,0);passo(1);
  abrirEvento=5;abrirVezes=99;passo(3000);passo(1);passo(10000);passo(1);passo(25000);passo(1);
  assert(video_falhou() && recon.esgotou);abrirVezes=0;video_parar();
  // P1-c: recusado uma vez, o Dispose seguinte confirma: sessao nova toca no alvo.
  video_definir_reconexao(1);video_tocar("http://test/movie");prontoHost();
  posicao=1337033;passo(1);nv_tpk_video_evento(9,-1,0);passo(1);
  abrirEvento=5;abrirVezes=1;passo(3000);passo(1);assert(recon.tentativa==2 && recon.pendente);
  passo(10000);prontoHost();passo(1);assert(busca==1337033 && tocando && !falhou);video_parar();
  // Ao vivo (sem reconexao): tecla recusada e ignorada, a sessao nao morre.
  video_tocar("http://test/movie");prontoHost();posicao=5000;passo(1);stops=paradas;
  video_pausar(1);nv_tpk_video_evento(9,-1,0);passo(1);
  video_buscar(60.0);nv_tpk_video_evento(9,60000,0);passo(1);
  video_pausar(0);passo(1);passo(20000);
  assert(paradas==stops && !video_falhou() && video_ativo());video_parar();
  // Prazo do seek vence com PlaybackInterrupted/Paused, sem consulta lenta.
  video_definir_reconexao(1);video_tocar("http://test/movie");prontoHost();
  posicao=1337033;passo(1);video_pausar(1);nv_tpk_video_evento(3,0,0);
  nv_tpk_video_evento(9,1347033,0);nv_tpk_video_evento(9,-1,0);passo(1);
  assert(recon.pendente && recon.alvo==1347.033);
  passo(3000);prontoHost();passo(1);assert(busca==1347033 && pausa==1);video_parar();
  // EOS wins over a queued recovery and late polling failure.
  video_definir_reconexao(1);video_tocar("http://test/movie");prontoHost();
  posicao=3599750;passo(1);nv_tpk_video_evento(9,-1,0);passo(1);
  antes=aberturas;stops=paradas;
  nv_tpk_video_evento(4,0,0);nv_tpk_video_evento(9,-1,0);passo(30000);
  assert(video_terminou() && video_pos()==3600.0 && !video_reconectando() && !video_falhou());
  assert(aberturas==antes && paradas==stops);
  video_buscar(3500);assert(!video_terminou() && busca==3500000);
  video_parar();
  puts("ok: pause/buffer/seek, bounded recovery, EOS cancels reconnect");
}
