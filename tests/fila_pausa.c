// FILA ANTECIPADA NA TROCA DE MOTOR EM PAUSA (correcao F102, bloqueio 1 da
// revisao PR734CD-r2): motor novo ligado, video parado em 2.95 s e o worker
// JA entregou o quadro de >=3000 ms (a "PLACA FUTURA" do render adiantado).
// Desligar a chave nao pode desenha-lo: assrender_motor_invalidar sobe a
// epoca, o quadro velho morre no consumo e o primeiro desenho do motor
// antigo volta a ser o do instante certo — a placa continua no MESMO pts.
//
// A sonda trava assTrava antes da troca para o primeiro desenho correr ANTES
// de o worker produzir o quadro novo: sem o filtro de epoca, o quadro de
// 3000 ms era servido na hora (first_ms >= 3000, first_y >= 500 -> rc 1).
//
//   bash tests/fila_pausa.sh
#include "../src/assrender.c"
#define main main_ass_area
#include "ass_area.c"
#undef main

static int motor = 1;
static int ler_motor(void) { return __atomic_load_n(&motor, __ATOMIC_RELAXED); }

static const char doc[] = CAB(640, 480)
  "Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,{\\an7\\pos(64,48)}PLACA ATUAL\n"
  "Dialogue: 0,0:00:03.00,0:00:05.00,Default,,0,0,0,,{\\an7\\pos(400,380)}PLACA FUTURA\n";

int main(void) {
  size_t bytes;
  char *font = ler("deploy/app/fonts/InterDisplay-Regular.ttf", &bytes);
  if (!font) return 2;
  ass_novo_registrar(ler_motor);
  assrender_geracao(11);
  assrender_limpar_fontes();
  assrender_adicionar_fonte("InterDisplay-Regular.ttf", font, bytes);
  free(font);
  if (assrender_carregar(doc, strlen(doc), 11) != 1) return 2;
  assrender_definir_layout(240, 0, 1440, 1080, 640, 480, 1.0);
  if (caixa(2.0) < 1) return 2;

  const double pts = 2.95;
  desenhos = 0;
  int before = assrender_desenhar(pts, 0, 1.0f, 0, 0, 1920, 1080);
  if (!before || !desenhos || cy0 >= 250) return 2;
  printf("antes da troca: pts=%.2f, imagens=%d, y=%.1f\n", pts, before, cy0);

  long long queued = -1;
  int n = 0;
  for (int i = 0; i < 1000; ++i) {
    pthread_mutex_lock(&assFilaTrava);
    n = assFilaN;
    if (n) queued = assFila[n-1].ms;
    pthread_mutex_unlock(&assFilaTrava);
    if (queued >= 3000) break;
    usleep(1000);
  }
  if (queued < 3000) { printf("fixture sem quadro antecipado: %lld\n", queued); return 2; }
  printf("fila antes da troca: %d quadro(s), pts=%lld ms\n", n, queued);

  /* O primeiro desenho da interface corre ANTES de o worker refazer o quadro:
   * com a epoca velha na fila, era ele quem entrava — a placa futura. */
  pthread_mutex_lock(&assTrava);
  __atomic_store_n(&motor, 0, __ATOMIC_RELAXED);
  assrender_motor_invalidar();
  desenhos = 0;
  int after = assrender_desenhar(pts, 0, 1.0f, 0, 0, 1920, 1080);
  float first_y = cy0;
  long long first_ms = assAtualValido ? assAtual.ms : -1;
  pthread_mutex_unlock(&assTrava);
  printf("primeiro desenho do motor antigo: pts=%.2f, imagens=%d, quadro=%lld ms, y=%.1f\n", pts, after, first_ms, first_y);
  int settled = caixa(pts);
  printf("apos o worker refazer: imagens=%d, y=%.1f\n", settled, cy0);
  assrender_limpar();
  assrender_aplicar_invalidacao();
  if (!settled || cy0 >= 250) return 2;
  return after && first_ms >= 3000 && first_y >= 500 ? 1 : 0;
}
