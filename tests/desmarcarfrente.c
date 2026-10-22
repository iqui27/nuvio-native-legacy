// Menu real e caso do dono (TCL, 10/10): voltar de T3 para T2E6.
// So o envio externo e capturado; mapa, lapides, grafico e eventos sao reais.
#define visto_episodios capturarEnvio
#define visto_destinos destinosTeste
#include "../src/episodios.c"
#undef visto_episodios
#undef visto_destinos
#include "vistonao.h"
#include "temporadas_grafico.h"
#include <assert.h>

#define SILO "tt14688458"
static int falhas, enviados, sentido, chamadas, totalEnviados;
static VistoPar paresEnviados[VE_LOTE_MAX];
#define CHECK(c) do { if (!(c)) { printf("FALHOU %d: %s\n", __LINE__, #c); falhas++; } } while (0)
int destinosTeste(void) { return VISTO_TRAKT | VISTO_SIMKL | VISTO_CONTA; }
int capturarEnvio(const char *id, const char *tipo, const VistoPar *p, int n, int v, int d) {
  CHECK(!strcmp(id, SILO) && !strcmp(tipo, "series"));
  CHECK(d == destinosTeste());
  CHECK(n <= 256);
  assert(totalEnviados + n <= VE_LOTE_MAX);
  memcpy(paresEnviados + totalEnviados, p, n * sizeof *p);
  totalEnviados += n;
  enviados = n; sentido = v; chamadas++;
  return 1;
}
static void teclaTeste(SDL_Keycode k) {
  SDL_Event ev = {0}; ev.type = SDL_KEYDOWN; ev.key.keysym.sym = k;
  episodios_menu_evento(&ev);
}
static int opcaoFrente(void) {
  for (int i = 0; i < vmOpcoes(); i++)
    if (!strcmp(vmLin[i].rot, i18n("Desmarcar daqui em diante"))) return i;
  return -1;
}
static void abrir(void) { episodios_menu_visto(0, 2, 6, "T2E6"); }
static void serieLonga(void) {
  CatEp eps[300] = {0};
  for (int i = 0; i < 300; i++) {
    eps[i].temporada = 1; eps[i].episodio = i + 1;
  }
  cat_definir_episodios(0, eps, 300);
  extras_teste_progresso(SILO, 300, 300, 0, 0);
  // Frente com 300 vistos; depois so os ultimos 44; ate aqui nos dois sentidos.
  for (int caso = 0; caso < 4; caso++) {
    int primeiro = caso == 1 ? 257 : 1, v = caso == 3;
    int esperado = 301 - primeiro;
    vistoep_esquecer();
    for (int e = 1; e <= 300; e++) vistoep_definir(SILO, 1, e, !v && e >= primeiro);
    chamadas = totalEnviados = 0;
    episodios_menu_visto(0, 1, caso < 2 ? 1 : 300, "Serie longa");
    int pos = caso < 2 ? opcaoFrente() : 1; // VM_ATE e a segunda linha
    CHECK(pos >= 0);
    if (pos >= 0) {
      for (int i = 0; i < pos; i++) teclaTeste(SDLK_DOWN);
      teclaTeste(SDLK_RETURN);
      CHECK(vmFeito && vmFeitoN == esperado && vmFeitoVisto == v);
    }
    CHECK(totalEnviados == esperado && chamadas == (esperado + 255) / 256);
    CHECK(sentido == v);
    CHECK(vistoep_contar(SILO) == (v ? 300 : 0));
    for (int i = 0; i < totalEnviados; i++)
      CHECK(paresEnviados[i].temporada == 1 && paresEnviados[i].episodio == primeiro + i);
    if (caso < 2) {
      episodios_menu_visto(0, 1, 1, "Serie longa");
      CHECK(opcaoFrente() < 0);
    }
    printf("300 episodios: caso %d, enviados %d, vistos restantes %d\n",
           caso, totalEnviados, vistoep_contar(SILO));
  }
}
static void limiteLapides(void) {
  const int total = VISTONAO_MAX + 100;
  extras_teste_progresso(SILO, total, total, 0, 0);
  for (int modo = 0; modo < 3; modo++) {
    vistoep_esquecer();
    vistonao_gesto(SILO, NULL, 0, 1);
    for (int e = 1; e <= total; e++) vistoep_definir(SILO, 1, e, 1);
    for (int gesto = 0; gesto < 2; gesto++) {
      int esperado = gesto ? total - VISTONAO_MAX : VISTONAO_MAX;
      chamadas = totalEnviados = 0;
      episodios_menu_visto(0, 1, modo == 1 ? total : 1, "Limite de lapides");
      CHECK(opcaoFrente() >= 0);
      int pos = modo == 0 ? opcaoFrente() : 1;
      if (modo == 2) menuAbrirTemporada(0, 1, 1);
      if (pos >= 0) {
        vmFoco = pos;
        teclaTeste(SDLK_RETURN);
        CHECK(vmFeito && vmFeitoN == esperado && vmFeitoVisto == 0);
      }
      CHECK(totalEnviados == esperado);
      CHECK(chamadas == (esperado + SMK_LOTE_MAX - 1) / SMK_LOTE_MAX);
      CHECK(vistoep_contar(SILO) == (gesto ? 0 : total - VISTONAO_MAX));
      int primeiro = gesto ? VISTONAO_MAX + 1 : 1;
      for (int i = 0; i < esperado; i++) {
        CHECK(paresEnviados[i].temporada == 1 && paresEnviados[i].episodio == primeiro + i);
        CHECK(vistonao_barra(SILO, 1, primeiro + i, 0) == 1);
        vistoep_fonte(SILO, 1, primeiro + i, 1, 0, NULL);
        CHECK(vistoep_estado(SILO, 1, primeiro + i) == 0);
      }
      if (!gesto)
        for (int e = VISTONAO_MAX + 1; e <= total; e++)
          CHECK(vistoep_estado(SILO, 1, e) == 1);
      printf("limite lapides: modo %d, gesto %d, enviados %d, vistos restantes %d\n",
             modo, gesto + 1, totalEnviados, vistoep_contar(SILO));
    }
    episodios_menu_visto(0, 1, 1, "Limite de lapides");
    CHECK(opcaoFrente() < 0);
  }
}
int main(void) {
  CatItem c = {0}; CatEp eps[30] = {0}; TgEp graf[30]; TgDados d;
  int i, pos, vistos, exibidos, t = 0, e = 0;
  snprintf(c.imdb, sizeof c.imdb, "%s", SILO);
  snprintf(c.tipo, sizeof c.tipo, "series");
  c.nTemporadas = 3;
  for (i = 0; i < 3; i++) c.temporadas[i] = i + 1;
  cat_definir(&c, 1);
  for (i = 0; i < 30; i++) {
    eps[i].temporada = i / 10 + 1; eps[i].episodio = i % 10 + 1;
    vistoep_definir(SILO, eps[i].temporada, eps[i].episodio, i < 29);
  }
  cat_definir_episodios(0, eps, 30);
  extras_teste_progresso(SILO, 29, 30, 3, 10);
  vistoep_lapides(vistonao_barra, vistonao_gesto);
  unsigned rev = vistoep_revisao();
  abrir(); pos = opcaoFrente(); CHECK(pos >= 0);
  // Antes da mudanca a opcao falta e a linha 2 e "Temporada inteira":
  // executa-la tambem demonstra por que o resultado nao atende ao pedido.
  if (pos < 0) pos = 2;
  for (i = 0; i < pos; i++) teclaTeste(SDLK_DOWN);
  teclaTeste(SDLK_RETURN);
  CHECK(vmFeito && vmFeitoN == 14 && vmFeitoVisto == 0);
  CHECK(chamadas == 1 && enviados == 14 && sentido == 0);
  for (i = 0; i < enviados; i++)
    CHECK(paresEnviados[i].temporada > 2 ||
          (paresEnviados[i].temporada == 2 && paresEnviados[i].episodio >= 6));
  CHECK(vistoep_contar(SILO) == 15 && vistoep_revisao() > rev);
  for (i = 0; i < 30; i++) {
    int tt = eps[i].temporada, ee = eps[i].episodio;
    CHECK(vistoep_estado(SILO, tt, ee) == (i < 15));
    CHECK(vistonao_barra(SILO, tt, ee, 0) == (i >= 15));
    graf[i] = (TgEp){.temporada = tt, .episodio = ee,
                    .visto = vistoep_estado(SILO, tt, ee)};
  }
  memset(&d, 0, sizeof d);
  tgraf_montar(&d, graf, 30, 1, 0, 0, 0, NULL);
  CHECK(d.t[tgraf_coluna(&d, 1)].vistos == 10);
  CHECK(d.t[tgraf_coluna(&d, 2)].vistos == 5);
  CHECK(d.t[tgraf_coluna(&d, 3)].vistos == 0);
  CHECK(extras_progresso_serie(&vistos, &exibidos) && vistos == 15 && exibidos == 30);
  CHECK(extras_proximo_episodio(&t, &e) && t == 2 && e == 6);
  abrir(); CHECK(opcaoFrente() < 0); // so anteriores vistos: nao oferece
  vistoep_definir(SILO, 3, 9, 1);
  abrir(); pos = opcaoFrente(); CHECK(pos >= 0 && vmVisto == 1);
  if (pos >= 0) {
    for (i = 0; i < pos; i++) teclaTeste(SDLK_DOWN);
    teclaTeste(SDLK_RETURN);
    CHECK(chamadas == 2 && enviados == 1 && sentido == 0);
    CHECK(vistoep_contar(SILO) == 15); // foco nao visto: continua DESMARCANDO
  }
  // Fonte antiga nao ressuscita o que foi desmarcado.
  vistoep_fonte(SILO, 2, 6, 1, 0, NULL);
  CHECK(vistoep_estado(SILO, 2, 6) == 0);
  // A ausencia da opcao nao desloca "Fontes deste episodio".
  abrir(); for (i = 0; i < 10; i++) teclaTeste(SDLK_DOWN);
  teclaTeste(SDLK_RETURN); CHECK(episodios_menu_pediu_fontes());
  serieLonga();
  limiteLapides();
  printf("desmarcarfrente: %s (%d falhas)\n", falhas ? "FALHOU" : "PASS", falhas);
  return falhas != 0;
}
