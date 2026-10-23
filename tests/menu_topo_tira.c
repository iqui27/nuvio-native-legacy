// Barra no topo = a tira de abas do PlxNative (widgets.rs: draw_tab_row,
// TabStrip, Capsule) nos tokens A27 (glassdesign/brief.md 3a): pilula 48,
// trilha 6 em volta (60 de altura), vao 2, Busca quadrada; avatar chip 60 a
// 64 da borda; a ilha do relogio com a MESMA altura, topo e margem; uma tinta
// so (gfx_chrome_tinta) que segue a "Opacidade do vidro" e nunca cai abaixo
// do piso; nenhum rotulo cortado em pt/en/de/ru/fr.
// A capsula de foco VIAJA (mola critica, x e largura) e nao salta; a tinta e
// a cobertura dela; apagada ela pousa; Animacoes reduzidas salta.
#include "ajustes.h"
#include "ilha.h"
#include "layout.h"
#include "menu.h"
#include "text.h"
#include "dados.h"
#include "escala.h"
#include "gfx.h"
#include "nvtmp.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

#define PERTO(a, b, e) (fabsf((a) - (b)) <= (e))
// Luminancia relativa sRGB (WCAG): o contraste que o relatorio mede. O solve
// do shader usa outra conta (canal ao quadrado); aqui vale o que o olho ve.
static float rel(int r, int g, int b) {
  float c[3] = { r / 255.0f, g / 255.0f, b / 255.0f };
  int n;
  for (n = 0; n < 3; n++) c[n] = c[n] <= 0.04045f ? c[n] / 12.92f : powf((c[n] + 0.055f) / 1.055f, 2.4f);
  return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  SDL_memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  menu_evento(&e);
}
static void passo(float s, int n) { while (n-- > 0) menu_atualizar(s, 0); }

static char dir[NVT_MAX], caminho[700];
static void gravar(const char *txt) {
  FILE *f = fopen(caminho, "w"); assert(f);
  fputs(txt, f); fclose(f);
  ajustes_dir(dir);
}
#define CFG "homeLayoutLocal 0\nmodernSidebar 0\nmenuTopoLocal 1\nmenuExplorarLocal 0\nmenuGuiaLocal 0\nmenuAgendaLocal 0\nmenuPerfilLocal 0\n"

int main(void) {
  static const int idiomas[] = { 0, 1, 6, 4, 5 };   // pt, en, de, ru, fr
  MenuTopoGeo g;
  char cfg[300];
  int j, k, i;
  float x0, x1, w1, ultimo;
  nvt_set(dir, sizeof dir, "/nuvio-menu-tira-XXXXXX");
  assert(mkdtemp(dir));
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir);
  // Larguras reais dos rotulos: a fonte precisa de um contexto GL (os glifos).
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  assert(SDL_GL_CreateContext(SDL_CreateWindow("t", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN)));
  assert(txt_iniciar("deploy/app", 1));
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);

  // GEOMETRIA, em cada idioma de rotulo longo.
  for (j = 0; j < 5; j++) {
    float ilhaX, cy;
    snprintf(cfg, sizeof cfg, CFG "idioma %d\n", idiomas[j]);
    gravar(cfg);
    assert(ajustes_menu_topo());
    menu_topo_geo(&g);
    printf("idioma %d: %d pilulas, trilha x %.1f w %.1f, vao %.1f, cortado %d\n", idiomas[j], g.n,
           g.trilha[0], g.trilha[2], g.n > 1 ? g.x[1] - g.x[0] - g.w[0] : 0.0f, g.cortado);
    assert(g.n >= 5 && !g.cortado);
    assert(PERTO(g.h, 48.0f, 0.01f));                                  // capsula 48
    assert(PERTO(g.trilha[3], 60.0f, 0.01f));                          // + inset 6
    // A27: avatar chip, trilha e ilha do relogio sao uma familia: mesmo topo,
    // mesma altura (capsulas de raio h/2), margens 64/64.
    assert(PERTO(g.trilha[1], NV_ILHA_Y, 0.01f) && PERTO(g.trilha[3], NV_ILHA_H, 0.01f));
    assert(PERTO(g.trilha[1], 32.0f, 0.01f) && PERTO(NV_ILHA_MARGEM_D, g.avatar[0], 0.01f));
    cy = NV_ILHA_Y + NV_ILHA_H * 0.5f;                                 // a linha da ilha
    assert(PERTO(g.trilha[1] + g.trilha[3] * 0.5f, cy, 0.01f));
    assert(PERTO(g.y + g.h * 0.5f, cy, 0.01f));
    assert(PERTO(g.avatar[1], g.trilha[1], 0.01f) && PERTO(g.avatar[3], g.trilha[3], 0.01f));
    assert(PERTO(g.avatar[2], g.avatar[3], 0.01f) && PERTO(g.avatar[0], 64.0f, 0.01f));
    // Pilulas concentricas na trilha: o mesmo recuo nas pontas e em cima.
    assert(PERTO(g.x[0] - g.trilha[0], 6.0f, 0.01f));
    assert(PERTO(g.trilha[0] + g.trilha[2] - (g.x[g.n - 1] + g.w[g.n - 1]), 6.0f, 0.01f));
    // Centrada, entre o avatar e a ilha do relogio, e acima de onde as fileiras comecam.
    assert(PERTO(g.trilha[0] + g.trilha[2] * 0.5f, NV_LAYOUT_REAL_W * 0.5f, 1.0f));
    assert(g.trilha[0] > g.avatar[0] + g.avatar[2]);
    ilhaX = NV_LAYOUT_REAL_W - NV_ILHA_MARGEM_D - ilha_relogio_largura();
    assert(g.trilha[0] + g.trilha[2] < ilhaX);
    assert(g.trilha[1] + g.trilha[3] < 100.0f);
    // Busca por ultimo, quadrada; rotulos com o respiro dos dois lados.
    assert(g.busca[g.n - 1] && PERTO(g.w[g.n - 1], g.h, 0.01f));
    for (k = 0; k + 1 < g.n; k++) {
      float vao = g.x[k + 1] - g.x[k] - g.w[k];
      assert(!g.busca[k] && g.w[k] > 2.0f * 10.0f * 0.9f);
      // Vao A27 de 2 px em todo idioma (quem aperta e o respiro).
      assert(PERTO(vao, 2.0f, 0.01f));
    }
  }

  // TINTA A27: a "Opacidade do vidro" existente e o parametro claro->tinto
  // (78 % -> ~0,47), crescente, e nunca abaixo do piso de legibilidade.
  { static const char *op[] = { "60", "70", "78", "86", "92" };
    float ant = 0.0f, t;
    for (k = 0; k < 5; k++) {
      setenv("NUVIO_SHOT_VIDRO_OPAC", op[k], 1);
      ajustes_teste_vidro_env();
      t = gfx_chrome_tinta();
      printf("opacidade %s: tinta %.3f\n", op[k], t);
      assert(t >= GFX_CHROME_TINTA_MIN && t >= ant && t <= 0.80f);
      if (k == 2) assert(PERTO(t, 0.47f, 0.01f));
      ant = t;
    }
    assert(ant > GFX_CHROME_TINTA_MIN + 0.2f);   // a faixa e real, nao um valor so
    unsetenv("NUVIO_SHOT_VIDRO_OPAC"); }

  // DENSIDADE: o slider engrossa o scrim sobre arte clara. Teto de luminancia
  // desce e os tetos de alfa sobem; 78% fica nos numeros de sempre
  // (0,125 / 0,72 / 0,82). O inverso antigo (0,125/k^2, tetos*k) fazia os 60%
  // lavarem o relogio #F4F5F8 e as abas #F3F2EF .74 sobre branco (~1,9:1).
  { static const char *op[] = { "60", "78", "92" };
    float ant = 0.0f;
    for (k = 0; k < 3; k++) {
      float d[3] = { 0, 0, 0 }, t, need, base, a, R, G, B, cr, ca;
      setenv("NUVIO_SHOT_VIDRO_OPAC", op[k], 1);
      ajustes_teste_vidro_env();
      gfx_chrome_dens(d);
      t = gfx_chrome_tinta();
      need = 1.0f > d[0] ? 1.0f - sqrtf(d[0] / 1.0f) : 0.0f;   // arte branca, luma 1
      base = 0.90f * t;
      a = base > (need < d[1] ? need : d[1]) ? base : (need < d[1] ? need : d[1]);
      // mix do shader: scrim #121418 sobre branco, depois a tinta por cima.
      R = (0x12 / 255.0f) * a + (1.0f - a);
      G = (0x14 / 255.0f) * a + (1.0f - a);
      B = (0x18 / 255.0f) * a + (1.0f - a);
      cr = (rel(0xf4, 0xf5, 0xf8) + 0.05f) / (rel((int)(R * 255 + 0.5f), (int)(G * 255 + 0.5f), (int)(B * 255 + 0.5f)) + 0.05f);
      R = (0xf3 / 255.0f) * 0.74f + R * 0.26f;   // aba ociosa #F3F2EF a .74
      G = (0xf2 / 255.0f) * 0.74f + G * 0.26f;
      B = (0xef / 255.0f) * 0.74f + B * 0.26f;
      ca = (rel(0xf3, 0xf2, 0xef) + 0.05f) / (rel((int)(R * 255 + 0.5f), (int)(G * 255 + 0.5f), (int)(B * 255 + 0.5f)) + 0.05f);
      printf("dens %s: teto %.4f cap %.3f/%.3f alfa %.3f relogio %.2f aba %.2f\n",
             op[k], d[0], d[1], d[2], a, cr, ca);
      assert(d[2] + 1e-5f >= d[1]);
      // O solve antigo subia o teto para 0,211 nos 60% e o relogio caia a
      // ~1,9:1 sobre branco. Abaixo de 78% o teto nao passa de 0,125, e o
      // contraste do relogio nunca volta atras quando o slider sobe.
      assert(d[0] <= 0.125f + 1e-4f && cr >= 4.5f && ca >= 1.4f);
      if (k) assert(cr + 1e-3f >= ant);
      if (k == 1)
        assert(PERTO(d[0], 0.125f, 1e-4f) && PERTO(d[1], 0.72f, 1e-4f) && PERTO(d[2], 0.82f, 1e-4f));
      ant = cr;
    }
    unsetenv("NUVIO_SHOT_VIDRO_OPAC"); }

  // MOVIMENTO.
  gravar(CFG "idioma 0\nanimacoes 0\n");
  assert(!ajustes_animacoes_reduzidas());
  menu_iniciar();
  menu_definir_destino(MENU_INICIO);
  menu_abrir();
  passo(0.016f, 80);
  menu_topo_geo(&g);
  assert(PERTO(g.foc[0], g.x[0], 0.01f) && PERTO(g.foc[1], g.w[0], 0.01f) && PERTO(g.foc[2], 1.0f, 0.001f));
  assert(PERTO(g.sel[0], g.x[0], 0.01f) && PERTO(g.sel[2], 1.0f, 0.001f));
  x0 = g.x[0]; x1 = g.x[1]; w1 = g.w[1];

  // DIREITA: a capsula parte macia (mola critica), passa pelo meio, nao passa do alvo.
  tecla(SDLK_RIGHT);
  passo(0.016f, 1);
  menu_topo_geo(&g);
  printf("1 quadro: foc x %.1f (de %.1f a %.1f), w %.1f, tinta %.2f/%.2f\n",
         g.foc[0], x0, x1, g.foc[1], g.foco[0], g.foco[1]);
  assert(g.foc[0] > x0 && g.foc[0] < x0 + 0.25f * (x1 - x0));
  assert(g.foco[0] > 0.0f && g.foco[0] < 1.0f && g.foco[1] < 1.0f);
  ultimo = g.foc[0];
  for (i = 0; i < 40; i++) {
    passo(0.016f, 1);
    menu_topo_geo(&g);
    assert(g.foc[0] >= ultimo - 0.001f && g.foc[0] <= x1 + 0.001f);   // monotona, sem repique
    ultimo = g.foc[0];
  }
  assert(PERTO(g.foc[0], x1, 0.5f) && PERTO(g.foc[1], w1, 0.5f));
  assert(g.foco[1] > 0.98f && g.foco[0] < 0.02f);
  assert(PERTO(g.sel[0], x0, 0.01f));   // a selecao e a pagina, nao o foco

  // O mesmo tempo de parede em passos de 33 ms chega ao mesmo lugar.
  tecla(SDLK_RIGHT);
  passo(0.033f, 20);
  menu_topo_geo(&g);
  assert(PERTO(g.foc[0], g.x[2], 0.5f));

  // Fechada: a capsula de foco apaga. Ao reabrir em outra pilula ela POUSA la.
  tecla(SDLK_ESCAPE);
  passo(0.016f, 80);
  menu_topo_geo(&g);
  assert(g.foc[2] < 0.02f);
  menu_definir_destino(MENU_INICIO);
  menu_abrir();
  passo(0.016f, 1);
  menu_topo_geo(&g);
  assert(PERTO(g.foc[0], g.x[0], 0.01f));

  // Animacoes reduzidas: salta no primeiro quadro.
  gravar(CFG "performanceAutoVersaoLocal 1\nidioma 0\nanimacoes 1\n");
  assert(ajustes_animacoes_reduzidas());
  tecla(SDLK_RIGHT);
  passo(0.016f, 1);
  menu_topo_geo(&g);
  assert(PERTO(g.foc[0], g.x[1], 0.01f) && PERTO(g.foc[1], g.w[1], 0.01f));

  unlink(caminho);
  { char t[700]; snprintf(t, sizeof t, "%s/ajustes.tmp", dir); unlink(t); }
  rmdir(dir);
  puts("menu_topo_tira: ok");
  return 0;
}
