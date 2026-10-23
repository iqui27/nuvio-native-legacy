// Android TV must keep the calendar and episode panel inside 1920x1080.
#include "../src/agendaui.c"
#include <assert.h>

float gfx_escala_ui(void) { return 1.0f; }
float gfx_escala_ui_base(void) { return 1.0f; }
float gfx_escala_txt(void) { return 1.0f; }
float ajustes_rail_largura_fixa(void) { return 144.0f; }
int agenda_n(void) { return 0; }
const AgItem *agenda_lista(int i) { (void)i; return NULL; }
int agenda_dias(const char *data) { (void)data; return -1; }

// Drag a scrollable Agenda list: only Mobile may capture it (TV drag = arrows).
static void agendaArrasto(int mobile) {
  PonteiroRolagem e = { PONT_ROL_INICIO, 1, 0.0f, 0.0f, 500.0f, 500.0f };
  layout_modo_definir(mobile);
  ctxAberto = 0; scrollY = 0.0f; toqueAgAtiva = NULL; toquerol_limpar(&toqueAgenda);
  toquerol_vincular(&toqueAgenda, (GfxRect){0, 0, 1920, 1080}, 1.0f, 0.0f, 400.0f, 1, &scrollY);
  int captura = toqueAgendaRolar(&e);
  e.fase = PONT_ROL_MOVER; e.delta = -200.0f;
  toqueAgendaRolar(&e);
  if (mobile) assert(captura == 1 && scrollY == 200.0f && toqueAgenda.livre);
  else assert(captura == 0 && scrollY == 0.0f && !toqueAgenda.livre && !toqueAgAtiva);
}

int main(void) {
  agendaArrasto(0);
  agendaArrasto(1);
  layout_tela_definir(1920, 1080);
  layout_modo_definir(0);
  AgMes m = agMesMedir();
  float calendarBottom = (m.gradeY + 6 * m.celH + 5 * m.espacY) * agEscala();
  printf("TV: calendar bottom=%.0f, episode panel top=%.0f, bottom=%.0f\n",
         calendarBottom, m.painel.y * agEscala(),
         (m.painel.y + m.painel.h) * agEscala());
  fflush(stdout);
  assert(calendarBottom <= 1080.0f);
  assert(m.painel.y >= 0 && m.painel.h > 0);
  assert((m.painel.y + m.painel.h) * agEscala() <= 1080.0f);
  assert(m.painel.x >= m.x + m.gradeW);
  AgC1 l = c1();
  assert(l.artW > 0 && l.artH > 0 && l.pnX >= l.x0 + l.artW);

  // Same aspect ratio in Mobile retains the stacked, scrollable calendar.
  layout_modo_definir(1);
  m = agMesMedir(); l = c1();
  assert(m.painel.y > m.gradeY + 6 * m.celH + 5 * m.espacY);
  assert(m.painel.x == m.x && l.artW == 0 && l.artH == 0);
  layout_tela_definir(1080, 2400);
  m = agMesMedir();
  assert(m.painel.y > m.gradeY + 6 * m.celH + 5 * m.espacY);
  escForcada = 1.0f;
  assert(!agColunaUnica()); // Social panel retains its own geometry.
  puts("PASS: Agenda TV fits 1080; Mobile calendar remains stacked");
  return 0;
}
