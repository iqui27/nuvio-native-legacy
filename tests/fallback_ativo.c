// FALLBACK NA FAIXA ABERTA (correcao F102, bloqueio 2 da revisao
// PR734CD-r2): "libass=0" gravado pelo painel NOVO nao pode atravessar para
// o motor antigo — la a faixa aberta volta ao libass (letreiros e tudo) —
// e na volta ao motor novo a preferencia segue valendo, sem reabrir a faixa.
// A troca dispara o aviso do assperf no fim de assrender_motor_invalidar e a
// legenda recarrega o documento guardado: posicao, pausa e geracao ficam.
//
//   bash tests/fallback_ativo.sh
#include "assrender.h"
#include "legenda.h"
#include "assperf.h"
#include "dados.h"
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *dados_ler(const char *nome) {
  return strcmp(nome, "assperf.txt") ? NULL : strdup("libass=0\n");
}
int dados_apagar(const char *nome) { (void)nome; return 1; }
char *rede_baixar_bin(const char *url, int segundos, long *n) {
  (void)url; (void)segundos; if (n) *n = 0; return NULL;
}
const char *dados_dir(void) { return NULL; }
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  (void)dst; (void)tam; (void)nome; return NULL;
}
void gfx_tex_esquecer(GLuint tex) { (void)tex; }
void teste_glDeleteTextures(GLsizei n, const GLuint *textures) { (void)n; (void)textures; }
static int motor = 1;
static int ler_motor(void) { return __atomic_load_n(&motor, __ATOMIC_RELAXED); }

static const char doc[] =
  "[Script Info]\nScriptType: v4.00+\nPlayResX: 640\nPlayResY: 480\n"
  "[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
  "Style: Default,sans-serif,36,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,0,0,2,10,10,20,1\n"
  "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
  "Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,{\\an7\\pos(64,48)}PLACA\n"
  "Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,Fala\n";

int main(void) {
  ass_novo_registrar(ler_motor);
  assperf_iniciar();
  if (assperf_get(AP_LIBASS) != 0) return 2;
  legenda_definir_corpo(doc);
  int novo = assrender_ativo();
  __atomic_store_n(&motor, 0, __ATOMIC_RELAXED);
  assrender_motor_invalidar();
  int antigo = assrender_ativo();
  int tipo = legenda_ass_ativa();
  LegendaCue cues[2];
  int atual = legenda_cues(2.0, 0, cues, 2);
  printf("apos troca na faixa aberta: libass=%d, faixa ASS=%d, %d cue(s)", antigo, tipo, atual);
  for (int i = 0; i < atual; ++i) printf(" [%s]", cues[i].texto);
  puts("");
  legenda_definir_corpo(doc);
  int recarregado = assrender_ativo();
  int reaberto = legenda_cues(2.0, 0, cues, 2);
  printf("apos reabrir: libass=%d, %d cue(s)", recarregado, reaberto);
  for (int i = 0; i < reaberto; ++i) printf(" [%s]", cues[i].texto);
  puts("");
  __atomic_store_n(&motor, 1, __ATOMIC_RELAXED);
  assrender_motor_invalidar();
  int volta_novo = assrender_ativo();
  printf("apos voltar ao motor novo com libass=0: libass=%d\n", volta_novo);
  assrender_limpar();
  assrender_aplicar_invalidacao();
  if (novo || !tipo) return 2;
  return antigo && atual == 2 && recarregado && reaberto == 2 && !volta_novo ? 0 : 1;
}
