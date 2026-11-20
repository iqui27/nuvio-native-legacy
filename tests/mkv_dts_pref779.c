/* #779: nova fonte com a mesma contagem de faixas ainda dispara reordenacao. */
#include <assert.h>
#include <stdio.h>

static int ante = -1;
static unsigned sessaoVista;

static int mkv_audio_mudou(int n, unsigned sessao) {
  int mudou = n != ante;
  if (sessao != sessaoVista) {
    sessaoVista = sessao;
    ante = -1;
    mudou = 1;
  }
  if (mudou) ante = n;
  return mudou;
}

int main(void) {
  assert(mkv_audio_mudou(2, 1));
  assert(!mkv_audio_mudou(2, 1));
  assert(mkv_audio_mudou(2, 2));
  printf("mkv_dts_pref779: mesma contagem no titulo seguinte ainda muda a lista\n");
  return 0;
}
