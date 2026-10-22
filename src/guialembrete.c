// O aviso do lembrete de programa — ver guialembrete.h.
#include "guialembrete.h"
#include "plrui.h"
#include "ilha.h"
#include "lembrete.h"
#include "perfis.h"
#include "player.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include "ponteiro.h"
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

// PONTEIRO (#99): foco pela mesma variavel das setas (que tambem reiniciam o
// relogio do cartao); o OK do clique segue por glem_evento.
static void ponteiroFoco(int i, int b) {
  (void)b;
  if (modo != 1 || (i != 0 && i != 1) || foco == i) return;
  foco = i; desde = SDL_GetTicks();
}
int glem_teste_foco(void) { return foco; }

static void glem_desenharCorpo_(Uint32 agora);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void glem_desenhar(Uint32 agora) {
  ESCALA_INI();
  glem_desenharCorpo_(agora);
  ESCALA_FIM();
}
static void glem_desenharCorpo_(Uint32 agora) {
  float a = anim, ar, ag, ab;
  (void)agora;
  if (a < 0.01f) return;
  ajustes_acento(&ar, &ag, &ab);
  if ((modo ? modo : ultimoModo) == 2) {
    // AVISO CURTO: uma pilula no alto, sino + frase. Fora do player ele sai
    // pela ILHA do relogio (ilha.h); com o player na frente fica esta pilula
    // propria, discreta, porque a ilha nao se desenha sobre o video.
    if (modo == 2 && !player_aberto()) {
      ilha_avisar("lembrete-curto", ILHA_ACENTO, "sino", aTexto, GLEM_CURTO_MS, 0);
      modo = 0; anim = 0.0f; ultimoModo = 0;
      return;
    }
    TxtLinha t = txt_linha_corta(TXT_BODY, aTexto, 240, 241, 246, 255, 900.0f);
    float w = (float)t.w + 96.0f, h = 60.0f;
    GfxRect r = { NV_TELA_W - NV_MARGEM_X - w, 44.0f - 16.0f * (1.0f - a), w, h };
    gfx_cor(r, 0.5f, 0.055f, 0.058f, 0.068f, 0.96f * a);
    gfx_icone((GfxRect){ r.x + 24.0f, r.y + (h - 28.0f) * 0.5f, 28.0f, 28.0f }, "sino",
              ar, ag, ab, a);
    txt_desenhar_alpha(t, r.x + 68.0f, r.y + (h - (float)t.h) * 0.5f, a);
    return;
  }
  // GLASS UI (mockup de 03/10, "aovivo-mini"): ilha no topo direito, a 40
  // das bordas — sino e "COMECA AGORA" no acento (estado), a frase 28/700, os
  // dois botoes do codigo e o tempo que resta num trilho dentro da ilha.
  { const float W = 680.0f;
    float lead = (float)txt_linha(TXT_G28B, "Ag", 0, 0, 0, 255).h + 4.0f;
    float hT = txt_bloco_corta(TXT_G28B, aTexto, 0, 0, 0, -1.0f, 0.0f, W - 60.0f, lead, 0.0f, 2);
    float H = 28.0f + 22.0f + 10.0f + hT + 22.0f + 60.0f + 22.0f + 3.0f + 28.0f;
    GfxRect r = { NV_TELA_W - 40.0f - W, 36.0f - 20.0f * (1.0f - a), W, H };
    float x = r.x + 30.0f, y = r.y + 28.0f;
    float resta = 1.0f - (float)(SDL_GetTicks() - desde) / (float)GLEM_CARTAO_MS;
    plrui_material(r, 32.0f, 0, a);
    gfx_icone((GfxRect){ x, y, 22.0f, 22.0f }, "pl_bell", ar, ag, ab, a);
    plrui_kicker("Começa agora", x + 34.0f, y + 2.0f, (int)(ar * 255), (int)(ag * 255), (int)(ab * 255), a);
    y += 22.0f + 10.0f;
    txt_bloco_corta(TXT_G28B, aTexto, 243, 242, 239, x, y, W - 60.0f, lead, a, 2);
    y += hT + 22.0f;
    { float bx = x;
      if (modo == 1 && a > 0.99f) {   // so assentado (sem o deslize da entrada)
        float w0 = plrui_botao_largura("Assistir", "pl_play-f");
        ponteiro_alvo(bx, y, w0, 60.0f, ponteiroFoco, NULL, 0, 0);
        ponteiro_alvo(bx + w0 + 12.0f, y, plrui_botao_largura("Dispensar", NULL), 60.0f,
                      ponteiroFoco, NULL, 1, 0);
      }
      bx += plrui_botao(bx, y, "Assistir", "pl_play-f", foco == 0 ? 1.0f : 0.0f, a) + 12.0f;
      plrui_botao(bx, y, "Dispensar", NULL, foco == 1 ? 1.0f : 0.0f, a); }
    y += 60.0f + 22.0f;
    if (resta < 0.0f) resta = 0.0f;
    plrui_trilho((GfxRect){ x, y, W - 60.0f, 3.0f }, resta, 0.953f, 0.949f, 0.937f, a * 0.5f); }
}
