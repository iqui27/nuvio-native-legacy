// Leitor lateral: limite pelo playhead, inclusive depois da adocao e seek.
#define main teste_pool_original
#include "mkvass_pool_obsoleto.c"
#undef main
static int escolherPrimeira(const char *const *ids, int n) { (void)ids; return n ? 0 : -1; }
int main(void) {
  Fio f = {0}; Ponto pontos[4] = {{0}}; int noJanela;
  f.pontos = pontos; f.nPontos = 4; f.escala = 1000000000UL; f.folgaJan = -1;
  pontos[0].tempo = 10; pontos[1].tempo = 12;
  pontos[2].tempo = 13; pontos[3].tempo = 40;
  f.prebusca = 1; f.escolher = escolherPrimeira; f.lateral = 1;
  printf("side reader window: before=150000 ms, observed=%.0f ms\n", alcanceFrente(&f)*1000);
  assert(alcanceFrente(&f) == 12);
  assert(proximoPendente(&f, -8, 12, &noJanela) == 0 && noJanela);
  pontos[0].colhido = pontos[1].colhido = 1;
  assert(proximoPendente(&f, -8, 12, &noJanela) == -1);
  pontos[0].cluster = pontos[1].cluster = pontos[2].cluster = 10;
  f.posRef = 0;
  pontos[0].colhido = pontos[1].colhido = 0;
  assert(grupoFim(&f, 0) == 1); // Range agrupado nao ultrapassa 12 s.
  pontos[0].colhido = pontos[1].colhido = 1;
  assert(proximoVarre(&f, 0) == -1);
  f.prebusca = 0; // A adocao conserva o pacing.
  assert(alcanceFrente(&f) == 12);
  assert(proximoPendente(&f, 5, 25, &noJanela) == 2 && noJanela);
  pontos[2].colhido = 1;
  assert(proximoPendente(&f, 5, 25, &noJanela) == -1);
  assert(proximoPendente(&f, 22, 42, &noJanela) == 3 && noJanela);
  pontos[0].colhido = 0;
  assert(proximoPendente(&f, -8, 12, &noJanela) == 0 && noJanela);
  f.escolher = NULL; // Retomada sem seletor ainda conserva a marca lateral.
  assert(alcanceFrente(&f) == 12);
  f.lateral = 0; // Colheita explicita antiga conserva seu comportamento.
  assert(proximoPendente(&f, 50, 70, &noJanela) >= 0);
  puts("plx subtitle window/seek: PASS");
  return 0;
}
