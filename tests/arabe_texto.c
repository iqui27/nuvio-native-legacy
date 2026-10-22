// Texto arabe do TMDB/addon por TODO caminho de desenho (txt_linha, corte,
// bloco): moldado, em ordem visual, sem quadradinho. A reserva arabe da Samsung
// (NotoNaskhArabic-Subset) nao tem latim; o resto sai da fonte da interface em
// outra corrida. Rode com NUVIO_SEM_RESERVA_DE_SISTEMA=1 (so fontes embarcadas,
// como na Samsung). Requer GL.
#include "gfx.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

static int cps(const char *s, unsigned *cp, int max) {
  const unsigned char *p = (const unsigned char *)s;
  int n = 0;
  while (*p && n < max) {
    unsigned c = *p;
    int k = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4, i;
    if (k > 1) c &= 0xFF >> (k + 1);
    for (i = 1; i < k; i++) c = (c << 6) | (p[i] & 0x3F);
    cp[n++] = c; p += k;
  }
  return n;
}

static int temCp(const char *s, unsigned alvo) {
  unsigned cp[512]; int n = cps(s, cp, 512), i;
  for (i = 0; i < n; i++) if (cp[i] == alvo) return 1;
  return 0;
}

int main(void) {
  TxtFamilia fam = TXT_FAMILIA_MONTSERRAT;
  char v[2048];
  int nc, falta;
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *win = SDL_CreateWindow("t", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win);
  SDL_GLContext gl = SDL_GL_CreateContext(win); assert(gl);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  txt_definir_fonte_interface(fam);

  // 1. Arabe puro: "كيانو ريفز" moldado e invertido, uma corrida, sem caixa.
  { static const unsigned esperado[] = { 0xFEB0, 0xFED4, 0xFEF3, 0xFEAD, ' ',
                                         0xFEEE, 0xFEE7, 0xFE8E, 0xFEF4, 0xFEDB };
    unsigned cp[64]; int n, i;
    falta = txt_visual_da_linha(fam, TXT_DET_META, "كيانو ريفز", v, sizeof v, &nc);
    n = cps(v, cp, 64);
    CHECK(falta == 0 && nc == 1);
    CHECK(n == (int)(sizeof esperado / sizeof *esperado));
    for (i = 0; i < n && i < (int)(sizeof esperado / sizeof *esperado); i++) CHECK(cp[i] == esperado[i]); }

  // 2. Meta com "·" e Latim no meio: corridas, nenhum quadradinho.
  falta = txt_visual_da_linha(fam, TXT_DET_META, "2023 · أكشن · إثارة", v, sizeof v, &nc);
  CHECK(falta == 0 && nc >= 2);
  falta = txt_visual_da_linha(fam, TXT_DET_SIN, "مسلسل Breaking Bad من إنتاج AMC، الموسم 5", v, sizeof v, &nc);
  CHECK(falta == 0 && nc >= 3);
  CHECK(strstr(v, "Breaking Bad") != NULL);    // o latim fica legivel, da esquerda para a direita
  // Aspas, parenteses, hifen e reticencias que a reserva nao tem.
  falta = txt_visual_da_linha(fam, TXT_DET_SIN, "ليواجه «الطاولة العليا» (كيانو ريفز) - من جديد…", v, sizeof v, &nc);
  CHECK(falta == 0);

  // 3. Marcas de direcao e embutimento: somem da saida, sem caixa.
  falta = txt_visual_da_linha(fam, TXT_DET_META,
                              "\xe2\x80\x8f" "\xe2\x80\xab" "جون ويك" "\xe2\x80\xac" " \xe2\x80\x8e(2014)\xef\xbb\xbf",
                              v, sizeof v, &nc);
  CHECK(falta == 0);
  CHECK(!temCp(v, 0x200F) && !temCp(v, 0x200E) && !temCp(v, 0x202B) && !temCp(v, 0x202C) && !temCp(v, 0xFEFF));

  // 4. Parenteses ficam com o latim (BD16 simplificado): "John Wick (2014)" inteiro.
  falta = txt_visual_da_linha(fam, TXT_DET_META, "جون ويك / John Wick (2014)", v, sizeof v, &nc);
  CHECK(falta == 0 && !strncmp(v, "John Wick (2014) / ", 19));

  // 5. Latim: identico, uma corrida.
  falta = txt_visual_da_linha(fam, TXT_DET_SIN, "Ação · Aventura — “John Wick 4” (2023) …", v, sizeof v, &nc);
  CHECK(falta == 0 && nc == 1 && !strcmp(v, "Ação · Aventura — “John Wick 4” (2023) …"));

  // 6. Medida sem rasterizar == linha rasterizada, inclusive com corridas.
  { const char *am[] = { "كيانو ريفز", "2023 · أكشن · إثارة · 2س 49د",
                         "مسلسل Breaking Bad من إنتاج AMC، الموسم 5",
                         "الحلقة 3: المواجهة الأخيرة", "Ação · Aventura" };
    unsigned i;
    for (i = 0; i < sizeof am / sizeof *am; i++) {
      txt_novo_quadro();
      int m = txt_largura(TXT_DET_SIN, am[i]);
      TxtLinha l = txt_linha(TXT_DET_SIN, am[i], 255, 255, 255, 255);
      if (m != l.w) printf("DIVERGE '%s': medida %d, linha %d\n", am[i], m, l.w);
      CHECK(m == l.w && l.w > 0);
    } }

  // 7. Quebra e corte: a sinopse quebra em varias linhas, o corte cabe.
  { const char *sin = "بعد أن فقد كل شيء في عام 2008، يعود جون ويك (كيانو ريفز) إلى نيويورك "
                      "ليواجه «الطاولة العليا» من جديد - هل سينجو هذه المرة؟ مُسَلْسَلٌ رائــع من إنتاج "
                      "Lionsgate بميزانية 100 مليون دولار.";
    txt_novo_quadro();
    float h = txt_bloco(TXT_DET_SIN, sin, 255, 255, 255, 0, 0, 700.0f, 40.0f, 1.0f, 0);
    CHECK(h >= 120.0f);
    txt_novo_quadro();
    TxtLinha c = txt_linha_corta(TXT_DET_SIN, sin, 255, 255, 255, 255, 600.0f);
    CHECK(c.w > 0 && c.w <= 600);
    // O corte poe "…" no fim LOGICO: na linha RTL ele fica na ponta esquerda.
    { falta = txt_visual_da_linha(fam, TXT_DET_SIN, "يعود جون ويك…", v, sizeof v, &nc);
      CHECK(falta == 0 && !strncmp(v, "…", 3)); } }

  // 6. #335: arabe em negrito usa a face Bold REAL (largura diferente da
  // Regular), nunca o TTF_STYLE_BOLD sintetico; o latim da mesma linha segue sintetico.
  { const char *ar = "يعود جون ويك إلى الشاشة الكبيرة";
    TxtLinha r = txt_linha_corta_enfase(TXT_DET_SIN, ar, 255, 255, 255, 255, 1800.0f, fam, 0);
    TxtLinha b = txt_linha_corta_enfase(TXT_DET_SIN, ar, 255, 255, 255, 255, 1800.0f, fam, TXT_ENF_NEGRITO);
    printf("[teste] arabe regular w=%d, negrito w=%d\n", r.w, b.w);
    CHECK(r.w > 0 && b.w > 0 && b.w != r.w); }

  if (falhas) { printf("arabe_texto: %d falha(s)\n", falhas); return 1; }
  puts("arabe_texto: ok");
  return 0;
}
