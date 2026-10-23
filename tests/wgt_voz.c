// BOTAO DE MICROFONE no .wgt: o caminho __EMSCRIPTEN__ de src/entrada_texto.c
// contra um window.nvTexto fingido (tests/wgt_stub/emscripten.h). Confere o
// teclado da TV para digitar, independente do ajuste do microfone;
// o ajuste x o que a TV disse do microfone, o aviso, e que a voz so chega ao
// campo quando o teclado da Samsung fecha.
//   bash tests/wgt_voz.sh
#include "sistexto.h"
#include "entrada_texto.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *desc_tmdb_idioma(void) { return "pt-BR"; }
int ajustes_teclado_android(void) { return 0; }

// --- o window.nvTexto fingido ---
static int temCampo = 1, mic = -1, seq, fim = -1, abriu, fechou, abriuVoz;
static char valorJs[256];
int wgt_em_int(const char *js, ...) {
  if (strstr(js, "microfone")) return mic;
  if (strstr(js, "t.seq")) return seq;
  if (strstr(js, "t.fim")) return fim;
  if (strstr(js, "window.nvTexto")) return temCampo;
  fprintf(stderr, "EM_ASM_INT inesperado: %s\n", js);
  abort();
}
void wgt_em(const char *js, ...) {
  va_list a;
  va_start(a, js);
  if (strstr(js, "nvTexto.abrir")) { (void)va_arg(a, const char *); abriuVoz = va_arg(a, int); abriu++; valorJs[0] = 0; }
  else if (strstr(js, "nvTexto.fechar")) fechou++;
  else if (strstr(js, "stringToUTF8")) { char *d = va_arg(a, char *); int n = va_arg(a, int); snprintf(d, (size_t)n, "%s", valorJs); }
  else if (strstr(js, "fim = -1")) fim = -1;
  else { fprintf(stderr, "EM_ASM inesperado: %s\n", js); abort(); }
  va_end(a);
}
static void jsDigita(const char *v) { snprintf(valorJs, sizeof valorJs, "%s", v); seq++; }

static int ler(int dono, char *t, size_t n) {
  SDL_Event e;
  texto_sistema_quadro();
  while (SDL_PollEvent(&e)) st_evento(&e);
  return st_ler(dono, t, n);
}
static const char *HINT = "Aperte o microfone do controle e fale.";

int main(int argc, char **argv) {
  char t[256];
  int r;
  // argv: o que a TV disse (getCapability): 1, 0 ou -1; "sem" = sem o <input>.
  if (argc > 1 && !strcmp(argv[1], "sem")) temCampo = 0;
  else if (argc > 1) mic = atoi(argv[1]);
  assert(SDL_Init(SDL_INIT_EVENTS) == 0);
  st_teste_ligar(0);

  // O campo continua abrindo o IME da TV; o botao de microfone e adicional.
  assert(texto_sistema_disponivel() == (temCampo ? TS_TECLADO | (mic != 0 ? TS_VOZ : 0) : 0));
  assert(st_ime_disponivel() == temCampo);
  assert(st_ime_abrir(ST_BUSCA, "ma", 64) == temCampo);
  if (temCampo) {
    assert(abriu == 1 && abriuVoz == 0);
    jsDigita("matrix");
    assert(ler(ST_BUSCA, t, sizeof t) == ST_TEXTO && !strcmp(t, "matrix"));
    fim = 1;
    assert(ler(ST_BUSCA, t, sizeof t) == ST_FIM && !strcmp(t, "matrix"));
  }
  abriu = 0;

  texto_sistema_mic_modo(TS_MIC_DESLIGADO);
  assert(!st_voz_disponivel() && !st_voz_iniciar(ST_BUSCA) && abriu == 0);
  assert(texto_sistema_disponivel() == (temCampo ? TS_TECLADO : 0));
  assert(st_ime_disponivel() == temCampo);
  texto_sistema_mic_modo(TS_MIC_LIGADO);
  assert(st_voz_disponivel() == temCampo);
  texto_sistema_mic_modo(TS_MIC_AUTO);
  assert(st_voz_disponivel() == (temCampo && mic != 0));
  if (!temCampo || mic == 0) {
    printf("PASS: wgt_voz %s (sem botao no Automatico)\n", temCampo ? "mic=0" : "sem campo");
    return 0;
  }

  // OK no botao: o <input> recebe foco (teclado da Samsung), vazio, com voz.
  assert(st_voz_iniciar(ST_BUSCA) && abriu == 1 && abriuVoz == 1);
  assert(texto_sistema_voz_pelo_teclado());
  assert(st_estado() == ST_DIGITANDO && !strcmp(st_aviso(), HINT));
  // Texto enquanto o teclado cobre a tela: nada chega ao campo.
  jsDigita("o poderoso");
  r = ler(ST_BUSCA, t, sizeof t);
  assert(r == ST_NADA);
  jsDigita("o poderoso chefão");
  r = ler(ST_BUSCA, t, sizeof t);
  assert(r == ST_NADA);
  // Concluido (65376) ou o teclado fechando sozinho com texto: fim = 1.
  fim = 1;
  r = ler(ST_BUSCA, t, sizeof t);
  assert(r == ST_FIM && !strcmp(t, "o poderoso chefão") && fim == -1);
  assert(!texto_sistema_aberto() && !st_aviso()[0]);

  // Voltar (10009) / Cancel (65385): fim = 0, nada chega.
  assert(st_voz_iniciar(ST_SPOT) && abriu == 2);
  jsDigita("matrix");
  fim = 0;
  r = ler(ST_SPOT, t, sizeof t);
  assert(r == ST_CANCELOU && !t[0] && !st_aviso()[0]);

  // A tela fechou com o teclado aberto: o JS esconde o teclado.
  assert(st_voz_iniciar(ST_BUSCA) && abriu == 3);
  st_fechar(ST_BUSCA);
  assert(fechou == 1 && !texto_sistema_aberto() && !st_aviso()[0]);
  r = ler(ST_BUSCA, t, sizeof t);
  assert(r == ST_NADA);

  assert(st_voz_iniciar(ST_BUSCA));
  fim = 2;
  assert(ler(ST_BUSCA, t, sizeof t) == ST_CANCELOU);
  assert(st_estado() == ST_PARADO && st_aviso()[0] && !texto_sistema_aberto());

  SDL_Quit();
  printf("PASS: wgt_voz mic=%d (teclado da TV para digitar e botao de microfone)\n", mic);
  return 0;
}
