// O aviso do lembrete de programa — ver guialembrete.h.
#include "guialembrete.h"
#include "lembrete.h"
#include "perfis.h"
#include "player.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// 0 = nada; 1 = cartao com botoes; 2 = aviso curto.
static int    modo, ultimoModo;
static Uint32 desde, ultConferido;
static float  anim;
static int    foco;                 // 0 = Assistir, 1 = Dispensar
static int    engolirOkUp;
static char   aCanal[80], aNome[120], aTitulo[120], aBase[600], aTexto[300];
static int    pedAssistir;

static int vendoCanal(const char *id) {
  return id[0] && !strcmp(player_id_canal(), id) &&
         (player_aberto() || player_mini_ativo());
}

static void abrir(int m, Uint32 agora) {
  modo = ultimoModo = m; desde = agora; foco = 0;
}

void glem_aviso_curto(const char *texto) {
  snprintf(aTexto, sizeof aTexto, "%s", texto ? texto : "");
  aCanal[0] = 0;
  abrir(2, SDL_GetTicks());
}

void glem_teste_cartao(const char *titulo, const char *canal, int curto) {
  snprintf(aTitulo, sizeof aTitulo, "%s", titulo);
  snprintf(aNome, sizeof aNome, "%s", canal);
  snprintf(aTexto, sizeof aTexto, i18n("%s começa agora no %s"), aTitulo, aNome);
  aCanal[0] = 0;
  abrir(curto ? 2 : 1, SDL_GetTicks());
  anim = 1.0f;
}

int glem_cartao_aberto(void) { return modo == 1; }

void glem_passo(float dt, Uint32 agora) {
  if (modo && agora - desde > (modo == 1 ? GLEM_CARTAO_MS : GLEM_CURTO_MS)) modo = 0;
  anim = anim_mola(anim, modo ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  // Perfil trocado: a lista e a do perfil novo.
  if (lembrete_perfil() != perfis_ativo()) lembrete_carregar(perfis_ativo());
  // Uma vez por segundo, e so.
  if (ultConferido && agora - ultConferido < 1000u) return;
  ultConferido = agora ? agora : 1;
  if (!lembrete_n()) return;
  { time_t t = time(NULL);
    int i;
    lembrete_podar(t);
    if (modo == 1) return;   // um cartao de cada vez; o proximo espera
    i = lembrete_vencido(t);
    if (i >= 0) {
      const Lembrete *l = lembrete_item(i);
      snprintf(aCanal, sizeof aCanal, "%s", l->canal);
      snprintf(aNome, sizeof aNome, "%s", l->nome);
      snprintf(aTitulo, sizeof aTitulo, "%s", l->titulo);
      snprintf(aBase, sizeof aBase, "%s", l->base);
      snprintf(aTexto, sizeof aTexto, i18n("%s começa agora no %s"), aTitulo, aNome);
      lembrete_marcar_avisado(i);
      abrir(vendoCanal(aCanal) ? 2 : 1, agora);
      printf("[lembrete] aviso: %s (%s)\n", aTitulo, vendoCanal(aCanal) ? "ja no canal" : "cartao");
      fflush(stdout);
    } }
}

int glem_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (e->type == SDL_KEYUP) {
    k = e->key.keysym.sym;
    if (engolirOkUp && (k == SDLK_RETURN || k == SDLK_KP_ENTER)) { engolirOkUp = 0; return 1; }
    return 0;
  }
  if (modo != 1 || e->type != SDL_KEYDOWN) return 0;
  k = e->key.keysym.sym;
  if (k == SDLK_LEFT || k == SDLK_RIGHT) { foco = !foco; desde = SDL_GetTicks(); return 1; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
    engolirOkUp = 1;
    if (foco == 0) pedAssistir = 1;
    modo = 0;
    return 1;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    modo = 0;
    return 1;
  }
  return 0;
}

int glem_pediu_assistir(char *id, size_t tam, char *nome, size_t tamNome,
                        char *base, size_t tamBase) {
  if (!pedAssistir || !aCanal[0]) { pedAssistir = 0; return 0; }
  pedAssistir = 0;
  snprintf(id, tam, "%s", aCanal);
  snprintf(nome, tamNome, "%s", aNome);
  snprintf(base, tamBase, "%s", aBase);
  return 1;
}

// Pilula de acao do cartao: preenchida na cor de realce com texto escuro em
// foco (a linguagem das pilulas do app), vidro translucido em repouso.
static float botao(const char *rot, float x, float y, int f, float a) {
  float ar, ag, ab;
  int tf = ajustes_tinta_foco();
  TxtLinha t = f ? txt_linha(TXT_PG_ROTULO, rot, tf, tf, tf, 255)
                 : txt_linha(TXT_PG_ROTULO, rot, 230, 231, 236, 255);
  GfxRect r = { x, y, (float)t.w + 40.0f, 44.0f };
  ajustes_acento(&ar, &ag, &ab);
  if (f) gfx_cor(r, 0.5f, ar, ag, ab, a);
  else   gfx_cor(r, 0.5f, 1, 1, 1, 0.10f * a);
  txt_desenhar_alpha(t, r.x + 20.0f, r.y + (r.h - (float)t.h) * 0.5f, a);
  return r.w;
}

void glem_desenhar(Uint32 agora) {
  float a = anim, ar, ag, ab;
  (void)agora;
  if (a < 0.01f) return;
  ajustes_acento(&ar, &ag, &ab);
  if ((modo ? modo : ultimoModo) == 2) {
    // AVISO CURTO: uma pilula no alto, sino + frase.
    TxtLinha t = txt_linha_corta(TXT_BODY, aTexto, 240, 241, 246, 255, 900.0f);
    float w = (float)t.w + 96.0f, h = 60.0f;
    GfxRect r = { NV_TELA_W - NV_MARGEM_X - w, 44.0f - 16.0f * (1.0f - a), w, h };
    gfx_cor(r, 0.5f, 0.055f, 0.058f, 0.068f, 0.96f * a);
    gfx_icone((GfxRect){ r.x + 24.0f, r.y + (h - 28.0f) * 0.5f, 28.0f, 28.0f }, "sino",
              ar, ag, ab, a);
    txt_desenhar_alpha(t, r.x + 68.0f, r.y + (h - (float)t.h) * 0.5f, a);
    return;
  }
  { const float W = 680.0f, H = 212.0f;
    GfxRect r = { NV_TELA_W - NV_MARGEM_X - W, 44.0f - 20.0f * (1.0f - a), W, H };
    float x = r.x + 32.0f, y = r.y + 26.0f;
    float resta = 1.0f - (float)(SDL_GetTicks() - desde) / (float)GLEM_CARTAO_MS;
    // Uma luz difusa na cor de realce por tras: o cartao chama o olho sem
    // precisar piscar.
    gfx_rect((GfxRect){ r.x - 40.0f, r.y - 40.0f, r.w + 80.0f, r.h + 80.0f }, 0, GFX_SOMBRA,
             1.0f, 0, 0, 0.5f, ar, ag, ab, 0.16f * a);
    gfx_cor(r, 24.0f / H, 0.055f, 0.058f, 0.068f, 0.98f * a);
    gfx_icone((GfxRect){ x, y + 2.0f, 34.0f, 34.0f }, "sino", ar, ag, ab, a);
    { TxtLinha c = txt_linha(TXT_PG_ROTULO, i18n("Começa agora"), 160, 163, 172, 255);
      txt_desenhar_alpha(c, x + 50.0f, y + 2.0f + (34.0f - (float)c.h) * 0.5f, a); }
    y += 50.0f;
    txt_bloco(TXT_CW_TITULO, aTexto, 244, 245, 250, x, y, W - 64.0f, 34.0f, a, 2);
    y = r.y + H - 70.0f;
    { float bx = x;
      bx += botao(i18n("Assistir"), bx, y, foco == 0, a) + 12.0f;
      botao(i18n("Dispensar"), bx, y, foco == 1, a); }
    // O tempo que falta para o cartao sumir, numa linha fina na base.
    if (resta > 0.0f)
      gfx_cor((GfxRect){ r.x + 24.0f, r.y + H - 10.0f, (W - 48.0f) * resta, 3.0f }, 0.5f,
              ar, ag, ab, 0.55f * a); }
}
