// A RESERVA DO RELOGIO PRECISA COBRIR A ILHA REAL: TOPO_RELOGIO (menu do topo,
// src/menu.c) usa ilha_relogio_largura() para nao cobrir a ilha. Ligado o
// "Medidor de desempenho" em Ajustes, a ilha cresce a linha do medidor
// (desempenho_linha_w, ~200 px) e, em Grande, para a largura do painel — a
// reserva tem que crescer junto. Estado real de ilha.c + desempenho.c; o
// texto e dublado com 10 px por caractere (so largura interessa aqui).
//
//   bash tests/ilha_relogio_largura.sh
#include "../src/ilha.c"
#include "../src/desempenho.c"
#include <assert.h>
#include <stdio.h>

Uint32 SDL_GetTicks(void) { return 1000; }
const char *i18n(const char *s) { return s; }
float gfx_tex_aspect_atual;
float gfx_card_forcar_cover_atual;
int txt_pendentes;
int anim_politica_reduzida;
// Control Center hold fill (central.c), sem segurar aqui.
float central_segurar_progresso(Uint32 a) { (void)a; return 0.0f; }
void  central_segurar_desenhar(GfxRect r, float ra, float a, Uint32 ag) {
  (void)r; (void)ra; (void)a; (void)ag;
}
static float escalaUi = 1.0f;
float gfx_escala_ui(void) { return escalaUi; }
static int medidor = 0;
int ajustes_medidor_desempenho(void) { return medidor; }
int ajustes_relogio_12h(void) { return 0; }
int ajustes_idioma(void) { return 0; }
int cacheboost_suportado(void) { return 0; }
int txt_largura(TxtEstilo e, const char *s) { (void)e; return (int)strlen(s) * 10; }
TxtLinha txt_linha(TxtEstilo e, const char *s, int r, int g, int b, int a) {
  TxtLinha l;
  (void)e; (void)r; (void)g; (void)b; (void)a;
  // Idle UI frames must not be mistaken for the hardware video's frame rate.
  if (strstr(s, "fps")) assert(strcmp(s, "5 UI fps") == 0);
  memset(&l, 0, sizeof l);
  l.w = (int)strlen(s) * 10; l.h = 18;
  return l;
}

int main(void) {
  float base, dw, comMedidor;
  // Sem amostra (arranque) a reserva e so hora + pontos, em qualquer ajuste.
  base = ilha_relogio_largura();
  assert(base > 0.0f);
  medidor = DS_MINIMO;
  assert(desempenho_forma() == DS_DESLIGADO);   // sem amostra: nada muda
  assert(fabsf(ilha_relogio_largura() - base) < 0.01f);
  desempenho_amostra(5.0f, 3.2f, 0, 120, 64.0f, 4, 0, 0, 220.0f);
  // Minimo: a linha inteira (fps + pior quadro + RAM) entra na reserva.
  assert(desempenho_forma() == DS_MINIMO);
  dw = desempenho_linha_w(DS_MINIMO);
  assert(dw > 100.0f);
  comMedidor = ilha_relogio_largura();
  assert(fabsf(comMedidor - (base + dw)) < 0.01f);
  // A escala de interface amplia a reserva junto.
  escalaUi = 1.5f;
  assert(fabsf(ilha_relogio_largura() - (base + dw) * 1.5f) < 0.01f);
  escalaUi = 1.0f;
  // Grande: a ilha vale a largura do painel (GR_W), a reserva nao pode ser menor.
  medidor = DS_GRANDE;
  assert(desempenho_forma() == DS_GRANDE);
  assert(ilha_relogio_largura() >= 520.0f);
  printf("ilha_relogio_largura ok: base %.0f px, +medidor %.0f px, grande >= 520 px\n", base, dw);
  return 0;
}
