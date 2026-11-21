// CARTOES DOS ARQUIVOS DA FONTE (partescartao.c): a fileira no fim de um
// especial/OVA mostra os outros arquivos, mas NAO toca nada sozinha — so o OK
// num arquivo diferente pede troca. A contagem da parte seguinte, ao
// contrario, dispara o pedido ao zerar — e sao coisas separadas de proposito:
// foi a mistura das duas que gerou o "trocou de arquivo sem eu pedir".
// Stubs de gfx/plrui/txt: o teste so exercita estado, evento e pedido.
#include <stdio.h>
#include <string.h>
#include "partescartao.h"
#include "gfx.h"
#include "text.h"
#include "plrui.h"
#include "ponteiro.h"
#include "ajustes.h"

const char *i18n(const char *s) { return s; }
void gfx_anel(GfxRect r, float raio, float esp, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)esp; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco, float parx, float pary,
              float raio, float cr, float cg, float cb, float ca) {
  (void)r; (void)tex; (void)modo; (void)foco; (void)parx; (void)pary; (void)raio;
  (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_recorte(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }
void gfx_sem_recorte(void) {}
float gfx_escala_entrar(void) { return 1.0f; }
void gfx_escala_sair(float ant) { (void)ant; }
float gfx_escala_ui(void) { return 1.0f; }
void ajustes_acento(float *r, float *g, float *b) { *r = *g = *b = 1.0f; }
TxtLinha txt_linha_corta(TxtEstilo estilo, const char *s, int r, int g, int b, int a, float w) {
  TxtLinha l = { 0, 0, 20, 0, 0 };
  (void)estilo; (void)s; (void)r; (void)g; (void)b; (void)a; (void)w;
  return l;
}
void txt_desenhar_alpha(TxtLinha l, float x, float y, float alpha) {
  (void)l; (void)x; (void)y; (void)alpha;
}
void plrui_material_video(GfxRect r, float raioPx, float a) { (void)r; (void)raioPx; (void)a; }
float plrui_kicker(const char *s, float x, float y, int r, int g, int b, float a) {
  (void)s; (void)x; (void)y; (void)r; (void)g; (void)b; (void)a; return 10.0f;
}
float plrui_botao_video(float x, float y, const char *rotulo, const char *icone,
                        float foco, float a) {
  (void)x; (void)y; (void)rotulo; (void)icone; (void)foco; (void)a; return 120.0f;
}
float plrui_dicas_video(const char *const *teclas, const char *const *rotulos, int n,
                        float x, float y, int direita, float a) {
  (void)teclas; (void)rotulos; (void)n; (void)x; (void)y; (void)direita; (void)a;
  return 10.0f;
}
void plrui_pilula_foco_video(GfxRect r, float a) { (void)r; (void)a; }
void plrui_botao_repouso_video(GfxRect r, float a) { (void)r; (void)a; }
void ponteiro_alvo(float x, float y, float w, float h, PonteiroFn focar,
                   PonteiroFn ativar, int a, int b) {
  (void)x; (void)y; (void)w; (void)h; (void)focar; (void)ativar; (void)a; (void)b;
}

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok  " : "FAIL", o); if (!c) falhas++; }
static SDL_Event tecla(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  return e;
}

int main(void) {
  ParteArq arq[4]; PartesLista l;
  SDL_Event e;
  memset(arq, 0, sizeof arq); memset(&l, 0, sizeof l);
  l.n = 4; l.arq = arq;
  snprintf(arq[0].nome, sizeof arq[0].nome, "OVA 1.mkv"); arq[0].tipo = PARTE_ESPECIAL; arq[0].bytes = 900000000;
  snprintf(arq[1].nome, sizeof arq[1].nome, "OVA 2.mkv"); arq[1].tipo = PARTE_ESPECIAL; arq[1].bytes = 900000000;
  snprintf(arq[2].nome, sizeof arq[2].nome, "OVA 3.mkv"); arq[2].tipo = PARTE_ESPECIAL; arq[2].bytes = 900000000;
  snprintf(arq[3].nome, sizeof arq[3].nome, "Making of.mkv"); arq[3].tipo = PARTE_EXTRA; arq[3].bytes = 100000000;

  // Fileira de fim de especial: aparece, espera, e NAO toca sozinha.
  partescartao_zerar();
  partescartao_arquivos(&l, 0);
  ok(partescartao_visivel() && partescartao_modo_arquivos(), "file row shows at the special's end");
  partescartao_atualizar(0.5f, 100000);
  partescartao_atualizar(0.5f, 200000);
  ok(partescartao_pedido() == PC_PEDIDO_NENHUM, "the row does not autoplay: time passes, no request");
  ok(partescartao_visivel(), "the row stays up waiting for a choice");

  // OK num arquivo diferente pede a troca; OK no atual nao pede nada.
  e = tecla(SDLK_LEFT);       // foco estava no seguinte (1): volta ao atual (0)
  partescartao_evento(&e);
  e = tecla(SDLK_RETURN);
  ok(partescartao_evento(&e) == 1 && partescartao_pedido() == PC_PEDIDO_NENHUM,
     "OK on the file that is playing asks nothing");
  ok(partescartao_visivel(), "OK on the playing file keeps the row up");
  partescartao_arquivos(&l, 0);
  e = tecla(SDLK_RETURN);     // foco ja vem no arquivo seguinte
  ok(partescartao_evento(&e) == 1 && partescartao_pedido() == 1,
     "OK on another file requests exactly that index");
  ok(!partescartao_visivel(), "choosing hides the row");

  // Voltar so fecha — nao pede arquivo nenhum.
  partescartao_arquivos(&l, 0);
  e = tecla(SDLK_ESCAPE);
  ok(partescartao_evento(&e) == 1 && !partescartao_visivel() &&
     partescartao_pedido() == PC_PEDIDO_NENHUM, "Back just closes, requests nothing");

  // Fonte de arquivo so: sem fileira.
  l.n = 1;
  partescartao_arquivos(&l, 0);
  ok(!partescartao_visivel(), "a single-file source shows no row");
  l.n = 4;

  // Contagem da parte seguinte: ao zerar o tempo, pede VENCEU (troca quando a
  // busca assentar); o OK da pessoa pede AGORA na hora; Voltar pede FICAR.
  partescartao_contagem(2, 3, 1000, 0);   // seg < 3 -> 10 s
  ok(partescartao_visivel() && partescartao_modo_contagem(), "countdown shows between parts");
  partescartao_atualizar(0.5f, 11000 + 200);
  ok(partescartao_pedido() == PC_PEDIDO_VENCEU,
     "countdown at zero asks to switch once the seek settles");
  partescartao_contagem(2, 3, 1000, 5);
  e = tecla(SDLK_RETURN);     // foco ja vem em "Assistir agora"
  ok(partescartao_evento(&e) == 1 && partescartao_pedido() == PC_PEDIDO_AGORA,
     "the person's OK on the countdown still requests the next part now");
  partescartao_contagem(2, 3, 1000, 5);
  e = tecla(SDLK_ESCAPE);
  ok(partescartao_evento(&e) == 1 && partescartao_pedido() == PC_PEDIDO_FICAR,
     "Back on the countdown asks to stay on this part");

  printf(falhas ? "partescartao: %d FAIL\n" : "partescartao: ok\n", falhas);
  return falhas != 0;
}
