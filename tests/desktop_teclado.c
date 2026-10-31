// Desktop text fields: the physical keyboard (SDL_TEXTINPUT, IME, clipboard)
// types into teclado.c and the TV grid never draws.
#define SDL_GetClipboardText fakeClipLer
#define SDL_SetClipboardText fakeClipGravar
#define SDL_free fakeLiberar
#include <SDL2/SDL.h>
static char clip[256];
char *fakeClipLer(void) { return SDL_strdup(clip); }
int fakeClipGravar(const char *t) { SDL_strlcpy(clip, t, sizeof clip); return 0; }
void fakeLiberar(void *p) { free(p); }
#include "../src/teclado.c"
#include <assert.h>
#undef SDL_free

// Just enough of the renderer for teclado_desenhar to run headless.
float gfx_escala_txt_entrar(void) { return 1; }
void gfx_escala_sair(float e) { (void)e; }
int ponteiro_ativo(void) { return 0; }
int ajustes_vidro(void) { return 0; }
int ajustes_animacoes_reduzidas(void) { return 1; }
int ajustes_tinta_foco(void) { return 20; }
int ajustes_idioma(void) { return 0; }
void ajustes_acento(float *r, float *g, float *b) { *r = *g = *b = 1; }
void gfx_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) { (void)r; (void)raio; (void)cr; (void)cg; (void)cb; (void)ca; }
void gfx_rect(GfxRect r, GLuint t, GfxModo m, float f, float px, float py, float raio, float cr, float cg, float cb, float ca) {
  (void)r; (void)t; (void)m; (void)f; (void)px; (void)py; (void)raio; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_anel_fora(GfxRect r, float raio, float folga, float esp, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)folga; (void)esp; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_recorte(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }
void gfx_sem_recorte(void) {}
static char desenhado[512];
static TxtLinha linha(const char *s) { TxtLinha l = { 0 }; snprintf(desenhado, sizeof desenhado, "%s", s); l.w = (float)strlen(s) * 10; l.h = 30; return l; }
TxtLinha txt_linha(TxtEstilo e, const char *s, int r, int g, int b, int a) { (void)e; (void)r; (void)g; (void)b; (void)a; TxtLinha l = { 0 }; l.w = (float)strlen(s) * 10; l.h = 30; return l; }
TxtLinha txt_linha_bruta(TxtEstilo e, const char *s, int r, int g, int b, int a) { (void)e; (void)r; (void)g; (void)b; (void)a; return linha(s); }
TxtLinha txt_linha_corta(TxtEstilo e, const char *s, int r, int g, int b, int a, float w) { (void)e; (void)r; (void)g; (void)b; (void)a; (void)w; TxtLinha l = { 0 }; (void)s; return l; }
void txt_desenhar_alpha(TxtLinha l, float x, float y, float a) { (void)l; (void)x; (void)y; (void)a; }
int txt_largura(TxtEstilo e, const char *s) { (void)e; return (int)strlen(s) * 10; }
float txt_bloco(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float w, float p, float a, int m) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)w; (void)p; (void)a; (void)m; return 30;
}

static void tecla(SDL_Keycode k, Uint16 mod) {
  SDL_Event e = { 0 };
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k; e.key.keysym.mod = mod;
  teclado_evento(&e);
}
static void digitar(const char *s) {
  SDL_Event e = { 0 };
  e.type = SDL_TEXTINPUT; snprintf(e.text.text, sizeof e.text.text, "%s", s);
  teclado_evento(&e);
}
static void compor(const char *s) {
  SDL_Event e = { 0 };
  e.type = SDL_TEXTEDITING; snprintf(e.edit.text, sizeof e.edit.text, "%s", s);
  teclado_evento(&e);
}
#define LIVRE "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .-"

int main(void) {
  // Free text: accents, dead-key composition, paste, caret editing.
  teclado_abrir_com("Nome da nova lista", "", 60, LIVRE, NULL);
  digitar("Ação");
  compor("´");                       // dead key pending: arrows belong to the IME
  tecla(SDLK_LEFT, 0);
  assert(deCaret == (int)strlen("Ação"));
  digitar("é");
  assert(!strcmp(teclado_texto(), "Açãoé") && !deComp[0]);
  tecla(SDLK_BACKSPACE, 0);          // one character, not one byte
  assert(!strcmp(teclado_texto(), "Ação"));
  strcpy(clip, " de çãé\n2");
  tecla(SDLK_v, KMOD_GUI);
  assert(!strcmp(teclado_texto(), "Ação de çãé 2"));
  tecla(SDLK_HOME, 0); tecla(SDLK_RIGHT, KMOD_SHIFT); tecla(SDLK_RIGHT, KMOD_SHIFT);
  tecla(SDLK_c, KMOD_CTRL);
  assert(!strcmp(clip, "Aç"));
  digitar("Ó");                      // replaces the selection
  assert(!strcmp(teclado_texto(), "Óão de çãé 2"));
  tecla(SDLK_a, KMOD_GUI); tecla(SDLK_x, KMOD_GUI);
  assert(!teclado_texto()[0] && !strcmp(clip, "Óão de çãé 2"));
  digitar("Lista"); tecla(SDLK_END, 0); tecla(SDLK_DELETE, 0); tecla(SDLK_LEFT, 0); tecla(SDLK_DELETE, 0);
  assert(!strcmp(teclado_texto(), "List"));
  // The modal draws as a text field; the TV grid never runs on desktop.
  anim = 1; teclado_desenhar(1000);
  assert(deGradeDesenhos == 0 && !strcmp(desenhado, "List"));
  tecla(SDLK_RETURN, 0);
  assert(!teclado_aberto() && teclado_resultado() == TECLADO_PRONTO && !strcmp(teclado_texto(), "List"));

  // Codes keep the caller's alphabet: lowercase fold, no accents, no letters in a PIN.
  teclado_abrir_com("E-mail", "", 120, "abcdefghijklmnopqrstuvwxyz0123456789@._-+", NULL);
  digitar("Nome@Exemplo.com"); digitar("ã");
  assert(!strcmp(teclado_texto(), "nome@exemplo.com"));
  tecla(SDLK_ESCAPE, 0);
  assert(!teclado_aberto() && teclado_resultado() == TECLADO_CANCELOU);
  teclado_abrir_com("PIN do perfil", "", 4, "0123456789", NULL);
  strcpy(clip, "12ab345");
  tecla(SDLK_v, KMOD_CTRL);
  assert(!strcmp(teclado_texto(), "1234"));
  tecla(SDLK_AC_BACK, 0);
  assert(teclado_resultado() == TECLADO_CANCELOU);

  // Password: masked, never copied out, Tab reaches Show and Return toggles it.
  teclado_abrir_com("Senha", "", 128, LIVRE, NULL);
  teclado_tipo(TECLADO_TIPO_SENHA);
  digitar("segredo"); strcpy(clip, "x");
  tecla(SDLK_a, KMOD_GUI); tecla(SDLK_c, KMOD_GUI);
  assert(!strcmp(clip, "x"));
  anim = 1; teclado_desenhar(1000);
  assert(!strcmp(desenhado, "\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2"));
  tecla(SDLK_TAB, 0); assert(deFoco == TE_D_MASCARA);
  tecla(SDLK_RETURN, 0); assert(!teclado_mascarado() && teclado_aberto());
  tecla(SDLK_TAB, KMOD_SHIFT); assert(deFoco == TE_D_CAMPO);
  tecla(SDLK_RETURN, 0);
  assert(teclado_resultado() == TECLADO_PRONTO && !strcmp(teclado_texto(), "segredo"));

  // Empty required field: Return does nothing until something is typed.
  teclado_abrir("Código do amigo", "", 6);
  tecla(SDLK_RETURN, 0);
  assert(teclado_aberto());
  assert(!celOk());                  // no "type on your phone" on desktop
  tecla(SDLK_ESCAPE, 0);
  puts("PASS: desktop text field types accents/IME/paste, edits by caret, keeps alphabets, masks passwords, no on-screen grid");
}
int st_ime_disponivel(void) { return 0; }
void st_fechar(int dono) { (void)dono; }
float gfx_escala_tela_txt(void) { return 1; }
void celular_fechar(void) {}
const char *i18n(const char *s) { return s; }
float nv_layout_w = 1920, nv_layout_h = 1080;
int celb_digitar_disponivel(void) { return 0; }   // celbotao.c: always 0 on NV_DESKTOP_UI
void celb_fechar_dono(int d) { (void)d; }
void gfx_luz_canto(GfxRect r, float raio, float x, float y, float rr, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)x; (void)y; (void)rr; (void)cr; (void)cg; (void)cb; (void)ca;
}
void gfx_vidro_material(GfxRect r, float raio, float d, float a) { (void)r; (void)raio; (void)d; (void)a; }
int layout_modo_mobile(void) { return 0; }
void ponteiro_alvo(float x, float y, float w, float h, void (*f)(int, int), void (*o)(int, int), int a, int b) {
  (void)x; (void)y; (void)w; (void)h; (void)f; (void)o; (void)a; (void)b;
}
void ponteiro_camada(void) {}
void ponteiro_cancelar_toque(void) {}
float ponteiro_x(void) { return 0; }
int st_abre_sozinho(void) { return 0; }
int st_ime_abrir(int d, const char *v, int m) { (void)d; (void)v; (void)m; return 0; }
void st_ime_tipo(int t) { (void)t; }
int st_teclado_modo(void) { return 0; }
float txt_tracking(TxtEstilo e, const char *s, int r, int g, int b, float x, float y, float a, float t) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)a; (void)t; return 0;
}
