// Cartao de NOVIDADES DA 1.4.
//
// A pagina e deliberadamente pequena e honesta: nao inventa contagens,
// percentuais ou tempos de performance. A celebracao e visual — uma TV nativa,
// cores complementares e confete curto — e o texto agradece quem usa, testa e
// compartilha o app.
#include "novidades1312.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "botoes.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N1312_ARQ       "novidades-14-celebracao.txt"
#define N1312_W         1536.0f
#define N1312_H          840.0f
#define N1312_X         ((NV_TELA_W - N1312_W) * 0.5f)
#define N1312_Y         ((NV_TELA_H - N1312_H) * 0.5f)
#define N1312_PAD         56.0f
#define N1312_FIG_W     560.0f
#define N1312_COL_GAP     40.0f
#define N1312_TXT_X     (N1312_X + N1312_PAD + N1312_FIG_W + N1312_COL_GAP)
#define N1312_TXT_W     (N1312_X + N1312_W - N1312_PAD - N1312_TXT_X)
#define N1312_FEAT_COL_W ((N1312_TXT_W - 24.0f) * 0.5f)
#define N1312_FEAT_H    100.0f
#define N1312_ICON       50.0f
#define N1312_FEAT_GAP   24.0f
#define N1312_ABRIR_MS  280.0f
#define N1312_FECHAR_MS 160.0f
#define N1312_CONFETES    72

static int   aberto, decidido;
static float entrada, fase;
typedef struct {
  float x, y, vx, vy, atraso, lado, tamanho;
  int cor;
} N1312Confete;
static N1312Confete confetes[N1312_CONFETES];
static float confete_t;

static void confetes_preparar(void) {
  int i;
  for (i = 0; i < N1312_CONFETES; i++) {
    int faixa = i / 2;
    int lado = i & 1;
    float espalha = ((float)(faixa % 9) - 4.0f) * 0.055f;
    float velocidade = 122.0f + (float)((faixa * 17) % 70);
    confetes[i].lado = lado ? 1.0f : -1.0f;
    confetes[i].x = lado ? 502.0f : 148.0f;
    confetes[i].y = 208.0f + (float)((faixa % 7) - 3) * 13.0f;
    confetes[i].vx = confetes[i].lado * velocidade;
    confetes[i].vy = -34.0f + (float)((faixa * 23) % 84) + espalha * 100.0f;
    confetes[i].atraso = (float)(faixa % 10) * 0.018f;
    confetes[i].tamanho = 8.0f + (float)(faixa % 4) * 2.0f;
    confetes[i].cor = faixa % 4;
  }
  confete_t = 0.0f;
}

int novidades1312_aberto(void) { return aberto; }

static void marcarVisto(void) { dados_gravar(N1312_ARQ, "1\n"); }

void novidades1312_abrir(void) {
  confetes_preparar();
  aberto = 1;
  decidido = 1;
  entrada = 1.0f;
  fase = 0.0f;
}

void novidades1312_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(N1312_ARQ);
  if (s) { free(s); return; }
  confetes_preparar();
  aberto = 1;
  entrada = 0.0f;
  fase = 0.0f;
}

static void fechar(void) {
  aberto = 0;
  marcarVisto();
}

void novidades1312_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE ||
      k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK)
    fechar();
}

void novidades1312_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  if (ajustes_animacoes_reduzidas()) {
    entrada = aberto ? 1.0f : 0.0f;
    fase = 0.0f;
    confete_t = aberto ? 1.35f : 3.4f;
    return;
  }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? N1312_ABRIR_MS : N1312_FECHAR_MS);
  if (aberto) {
    fase += dt * 1.8f;
    if (fase > 6.2831853f) fase -= 6.2831853f;
    confete_t += dt;
    if (confete_t > 3.4f) confete_t = 3.4f;
  }
}

static void confete_cor(int indice, float *r, float *g, float *b) {
  static const float paleta[4][3] = {
    { 0.98f, 0.37f, 0.36f },  // coral
    { 1.00f, 0.74f, 0.18f },  // amarelo
    { 0.24f, 0.82f, 0.72f },  // turquesa
    { 0.62f, 0.46f, 0.96f }   // violeta
  };
  *r = paleta[indice & 3][0];
  *g = paleta[indice & 3][1];
  *b = paleta[indice & 3][2];
}

static void desenhaConfetes(float x, float y, float w, float h, float a) {
  int i, reduzida = ajustes_animacoes_reduzidas();
  for (i = 0; i < N1312_CONFETES; i++) {
    const N1312Confete *c = &confetes[i];
    float idade = confete_t - c->atraso;
    float vida, px, py, alpha, cr, cg, cb, s;
    if (reduzida) {
      // A cena permanece colorida, mas sem deslocamento: reduzido ainda
      // comunica que houve uma comemoração, sem pedir movimento à pessoa.
      px = x + c->x + (float)((i / 2) % 3 - 1) * 14.0f;
      py = y + c->y + (float)((i / 6) % 3 - 1) * 15.0f;
      vida = 0.0f;
      alpha = 0.82f * a;
    } else {
      if (idade < 0.0f || idade > 3.15f) continue;
      vida = idade / 3.15f;
      px = x + c->x + c->vx * idade + sinf(idade * 5.0f + i) * 9.0f;
      py = y + c->y + c->vy * idade + 72.0f * idade * idade;
      alpha = (idade < 0.16f ? idade / 0.16f : 1.0f) *
              (vida > 0.78f ? (1.0f - vida) / 0.22f : 1.0f) * a;
    }
    if (px < x - 8.0f || px > x + w + 8.0f || py < y - 8.0f || py > y + h + 8.0f)
      continue;
    s = c->tamanho * (1.0f - 0.15f * vida);
    confete_cor(c->cor, &cr, &cg, &cb);
    gfx_cor((GfxRect){ px - s * 0.5f, py - s * 0.32f, s, s * 0.64f },
            0.14f, cr, cg, cb, alpha);
  }
}

// A TV desenhada com as primitivas nativas mantém a metáfora legível a 3 m:
// moldura, tela, antena, alto-falante e dois pés. O anel único atrás dela dá o
// lugar para a comemoração sem repetir a órbita genérica do cartão anterior.
static void desenhaTv(float cx, float cy, float a, float t) {
  float pulso = 0.72f + 0.08f * (0.5f + 0.5f * sinf(t * 2.2f));
  gfx_cor((GfxRect){ cx - 206.0f, cy - 206.0f, 412.0f, 412.0f }, 0.5f,
          0.20f, 0.13f, 0.36f, 0.42f * a);
  gfx_rect((GfxRect){ cx - 194.0f, cy - 194.0f, 388.0f, 388.0f },
           0, GFX_ANEL, 0, 0.014f, 0, 0.5f,
           0.62f, 0.46f, 0.96f, pulso * a);
  // Antena e seu ponto de encontro.
  gfx_cor((GfxRect){ cx - 5.0f, cy - 116.0f, 10.0f, 30.0f }, 0.5f,
          1.0f, 0.74f, 0.18f, a);
  gfx_cor((GfxRect){ cx - 28.0f, cy - 124.0f, 56.0f, 9.0f }, 0.5f,
          1.0f, 0.74f, 0.18f, a);
  gfx_cor((GfxRect){ cx - 142.0f, cy - 94.0f, 284.0f, 178.0f }, 0.10f,
          0.98f, 0.37f, 0.36f, a);
  gfx_cor((GfxRect){ cx - 128.0f, cy - 80.0f, 256.0f, 150.0f }, 0.075f,
          0.075f, 0.055f, 0.16f, a);
  gfx_cor((GfxRect){ cx - 114.0f, cy - 66.0f, 228.0f, 122.0f }, 0.055f,
          0.12f, 0.42f, 0.47f, a);
  // Brilho de tela, play pequeno e alto-falantes.
  gfx_cor((GfxRect){ cx - 96.0f, cy - 51.0f, 82.0f, 10.0f }, 0.5f,
          0.37f, 0.88f, 0.78f, 0.72f * a);
  gfx_rect((GfxRect){ cx - 16.0f, cy - 25.0f, 32.0f, 44.0f },
           0, GFX_PLAY, 0, 0, 0, 0, 1.0f, 0.74f, 0.18f, a);
  gfx_cor((GfxRect){ cx + 72.0f, cy - 14.0f, 8.0f, 8.0f }, 0.5f,
          0.98f, 0.37f, 0.36f, a);
  gfx_cor((GfxRect){ cx + 72.0f, cy + 3.0f, 8.0f, 8.0f }, 0.5f,
          1.0f, 0.74f, 0.18f, a);
  gfx_cor((GfxRect){ cx + 72.0f, cy + 20.0f, 8.0f, 8.0f }, 0.5f,
          0.37f, 0.82f, 0.72f, a);
  gfx_cor((GfxRect){ cx - 98.0f, cy + 88.0f, 42.0f, 20.0f }, 0.22f,
          0.62f, 0.46f, 0.96f, a);
  gfx_cor((GfxRect){ cx + 56.0f, cy + 88.0f, 42.0f, 20.0f }, 0.22f,
          0.62f, 0.46f, 0.96f, a);
}

static void desenhaCelebracao(float x, float y, float w, float h,
                              float a, float t) {
  float ar, ag, ab;
  float cx = x + w * 0.50f;
  float cy = y + h * 0.54f + sinf(t * 1.7f) * 3.0f;
  ajustes_acento(&ar, &ag, &ab);

  gfx_cor((GfxRect){ x, y, w, h }, 28.0f / h,
          0.035f, 0.055f, 0.105f, a);
  // A ilustração nativa é intencional: o aviso fala do próprio Nuvio, não de
  // um título específico. O disco concentra a cor e a TV conta a história
  // sem depender de uma foto genérica ou de uma arte de catálogo aleatória.
  gfx_rect((GfxRect){ cx - 204.0f, cy - 204.0f, 408.0f, 408.0f },
           0, GFX_DISCO, 0, 0, 0, 0, ar, ag, ab, 0.075f * a);
  desenhaConfetes(x, y, w, h, a);
  desenhaTv(cx, cy, a, t);
}

typedef struct {
  const char *icone, *titulo, *descricao;
} N1312Feature;

static const N1312Feature n1312_features[] = {
  { "legenda", "Legendas ASS",
    "Cores, posições e falas ao mesmo tempo nas legendas ASS." },
  { "fontes", "Filtro MP4",
    "Encontre as fontes MP4 direto na lista de reprodução." },
  { "menu_guide", "Guia TV",
    "O Guia de TV abre na hora" },
  { "play", "Trailers dentro do app",
    "Trailer toca no lugar da arte, sem sair do Nuvio." },
  { "recomendar", "A aba Social",
    "Recomendações de amigos agora chegam em Salvos." },
  { "menu_profile", "Fundo de perfil",
    "A tela de perfis pode mostrar artes dos seus títulos." },
  { "avancar", "Episódio certo",
    "As fontes e o carrossel param de voltar para o episódio anterior." },
  { "fluxo", "TorBox e Premiumize",
    "O app resolve os torrents dessas contas, não só do Real-Debrid." },
  { "home", "Cards grandes 4:3",
    "Novo tamanho maior para destaques, com mais espaço entre os cards." }
};

// Cada recurso tem uma pequena cena própria. Ícone único em disco fazia a
// coluna parecer uma lista de ajustes; estas formas continuam legíveis a três
// metros e contam visualmente o que mudou antes mesmo da leitura do texto.
static void miniGrafico(float x, float y, int tipo,
                        float ar, float ag, float ab, float a) {
  float xr = 0.93f, xg = 0.95f, xb = 0.99f;
  GfxRect p = { x, y, N1312_ICON, N1312_ICON };
  // O DISCO DO ICONE E A COR DE REALCE A 18 % (dono, 21/09/2026), e nao um
  // quadrado cinza: e o mesmo "aceso" dos selos positivos da tabela
  // (BADGE_REALCE em badges.h), e o realce e a unica cor que o cartao usa.
  gfx_rect(p, 0, GFX_DISCO, 0, 0, 0, 0, ar, ag, ab, 0.18f * a);
  switch (tipo) {
    case 0: // ASS: linhas simultâneas e cores de fala.
      gfx_cor((GfxRect){ x + 10, y + 12, 30, 4 }, 0.5f, xr, xg, xb, a);
      gfx_cor((GfxRect){ x + 10, y + 20, 20, 4 }, 0.5f, 0.98f, 0.38f, 0.40f, a);
      gfx_cor((GfxRect){ x + 10, y + 28, 27, 4 }, 0.5f, 0.32f, 0.84f, 0.75f, a);
      gfx_cor((GfxRect){ x + 10, y + 36, 14, 4 }, 0.5f, 1.00f, 0.74f, 0.18f, a);
      break;
    case 1: // MP4: rolo de filme dentro de uma fonte selecionável.
      gfx_cor((GfxRect){ x + 9, y + 11, 32, 28 }, 5.0f / 32.0f,
              ar, ag, ab, 0.30f * a);
      gfx_cor((GfxRect){ x + 13, y + 16, 24, 4 }, 0.5f, xr, xg, xb, a);
      gfx_cor((GfxRect){ x + 13, y + 24, 18, 4 }, 0.5f, xr, xg, xb, 0.72f * a);
      gfx_cor((GfxRect){ x + 13, y + 32, 12, 4 }, 0.5f, ar, ag, ab, a);
      break;
    case 2: // Guia: categorias, canais e uma faixa de horário.
      gfx_cor((GfxRect){ x + 9, y + 11, 32, 3 }, 0.5f, xr, xg, xb, a);
      gfx_cor((GfxRect){ x + 9, y + 19, 8, 4 }, 0.5f, ar, ag, ab, a);
      gfx_cor((GfxRect){ x + 20, y + 19, 17, 4 }, 0.5f, 0.32f, 0.84f, 0.75f, a);
      gfx_cor((GfxRect){ x + 9, y + 28, 14, 4 }, 0.5f, 0.98f, 0.38f, 0.40f, a);
      gfx_cor((GfxRect){ x + 26, y + 28, 11, 4 }, 0.5f, xr, xg, xb, 0.72f * a);
      gfx_cor((GfxRect){ x + 9, y + 37, 24, 3 }, 0.5f, xr, xg, xb, 0.50f * a);
      break;
    case 3: // Trailer: quadro, play e linha de duração.
      gfx_cor((GfxRect){ x + 9, y + 9, 32, 27 }, 5.0f / 32.0f,
              ar, ag, ab, 0.36f * a);
      gfx_rect((GfxRect){ x + 21, y + 14, 12, 18 }, 0, GFX_PLAY,
               0, 0, 0, 0, xr, xg, xb, a);
      gfx_cor((GfxRect){ x + 10, y + 41, 28, 3 }, 0.5f, xr, xg, xb, 0.28f * a);
      gfx_cor((GfxRect){ x + 10, y + 41, 17, 3 }, 0.5f, ar, ag, ab, a);
      break;
    case 4: // Social: duas pessoas e uma recomendação indo de uma à outra.
      gfx_cor((GfxRect){ x + 13, y + 12, 10, 10 }, 0.5f, xr, xg, xb, a);
      gfx_cor((GfxRect){ x + 27, y + 17, 8, 8 }, 0.5f, ar, ag, ab, a);
      gfx_cor((GfxRect){ x + 10, y + 29, 17, 4 }, 0.5f, xr, xg, xb, 0.75f * a);
      gfx_cor((GfxRect){ x + 25, y + 33, 14, 3 }, 0.5f, ar, ag, ab, a);
      gfx_rect((GfxRect){ x + 34, y + 28, 8, 10 }, 0, GFX_PLAY,
               0, 0, 0, 0, ar, ag, ab, a);
      break;
    case 5: // Perfil: avatar sobre um fundo de catálogo.
      gfx_cor((GfxRect){ x + 9, y + 10, 32, 28 }, 5.0f / 32.0f,
              ar, ag, ab, 0.25f * a);
      gfx_cor((GfxRect){ x + 12, y + 13, 26, 5 }, 0.5f, 0.32f, 0.84f, 0.75f, a);
      gfx_rect((GfxRect){ x + 15, y + 20, 12, 12 }, 0, GFX_DISCO,
               0, 0, 0, 0, xr, xg, xb, a);
      gfx_cor((GfxRect){ x + 12, y + 35, 24, 4 }, 0.5f, xr, xg, xb, 0.72f * a);
      break;
    case 6: // Retomada: progresso e o próximo passo.
      gfx_cor((GfxRect){ x + 9, y + 19, 29, 5 }, 0.5f, xr, xg, xb, 0.20f * a);
      gfx_cor((GfxRect){ x + 9, y + 19, 19, 5 }, 0.5f, ar, ag, ab, a);
      gfx_rect((GfxRect){ x + 31, y + 11, 12, 20 }, 0, GFX_PLAY,
               0, 0, 0, 0, ar, ag, ab, a);
      gfx_cor((GfxRect){ x + 11, y + 33, 15, 3 }, 0.5f, xr, xg, xb, 0.50f * a);
      break;
    case 8: // Destaque 4:3: cards maiores com respiro entre as molduras.
      gfx_cor((GfxRect){ x + 8, y + 10, 23, 29 }, 5.0f / 29.0f,
              ar, ag, ab, 0.28f * a);
      gfx_cor((GfxRect){ x + 12, y + 14, 16, 21 }, 3.0f / 21.0f,
              xr, xg, xb, a);
      gfx_cor((GfxRect){ x + 32, y + 14, 10, 21 }, 3.0f / 21.0f,
              0.32f, 0.84f, 0.75f, 0.80f * a);
      gfx_cor((GfxRect){ x + 12, y + 39, 30, 3 }, 0.5f,
              ar, ag, ab, 0.78f * a);
      break;
    default: // Sync: duas rotas que se encontram no mesmo perfil.
      gfx_rect((GfxRect){ x + 11, y + 12, 27, 27 }, 0, GFX_ANEL,
               0, 2.0f / 27.0f, 0, 0.5f, ar, ag, ab, 0.70f * a);
      gfx_rect((GfxRect){ x + 30, y + 10, 10, 14 }, 0, GFX_PLAY,
               0, 0, 0, 0, xr, xg, xb, a);
      gfx_rect((GfxRect){ x + 10, y + 27, 10, 14 }, 0, GFX_PLAY,
               0, 0, 0, 0, ar, ag, ab, a);
      break;
  }
}

static void feature(float x, float y, float w, const N1312Feature *f,
                    int tipo, float atraso, float a) {
  float ar, ag, ab, entradaLocal, yy, alpha;
  float tx = x + N1312_ICON + 18.0f;
  float tw = w - N1312_ICON - 30.0f;
  ajustes_acento(&ar, &ag, &ab);
  entradaLocal = ajustes_animacoes_reduzidas()
      ? 1.0f : anim_clamp((a - atraso) * 4.0f, 0.0f, 1.0f);
  yy = y + (1.0f - entradaLocal) * 16.0f;
  alpha = a * entradaLocal;

  // Cards discretos separam o mapa de recursos sem transformar o aviso numa
  // grade pesada. O accent fica reservado ao ícone e à animação de entrada.
  // SEM caixa por item e SEM o filete de realce na borda esquerda: caixa
  // dentro de cartao e cartao aninhado, e um traco colorido na lateral e a
  // marca de "aba ativa" da barra lateral — aqui nao ha nada ativo. O disco
  // do icone e o que ancora cada linha.
  miniGrafico(x + 12.0f, yy + 14.0f, tipo, ar, ag, ab, alpha);
  { TxtLinha t = txt_linha_corta(TXT_CALLOUT, i18n(f->titulo),
                                 246, 247, 252, 255, tw);
    txt_desenhar_alpha(t, tx, yy + 10.0f, alpha); }
  txt_bloco(TXT_CAPTION, i18n(f->descricao), 178, 186, 201,
            tx, yy + 44.0f, tw, 24.0f, alpha * 0.96f, 2);
}

void novidades1312_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy;
  float ar, ag, ab;
  (void)agora;
  if (entrada < 0.002f) return;
  ajustes_acento(&ar, &ag, &ab);

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0,
          0.74f * entrada);
  dy = (1.0f - a) * 34.0f;
  // O painel flutuante da cara nova (menu.c): 0.055/0.058/0.068 a 94 %.
  gfx_cor((GfxRect){ N1312_X, N1312_Y + dy, N1312_W, N1312_H },
          30.0f / N1312_H, 0.055f, 0.058f, 0.068f, 0.94f * a);
  gfx_luz_canto((GfxRect){ N1312_X, N1312_Y + dy, N1312_W, N1312_H },
                30.0f / N1312_H, N1312_W * 0.10f, -N1312_H * 0.10f,
                N1312_H * 0.65f, ar, ag, ab, 0.22f * a);
  gfx_recorte(N1312_X, N1312_Y + dy, N1312_W, N1312_H);

  // Sem filete no topo: a luz de realce no canto (gfx_luz_canto acima) ja
  // liga o cartao ao tema, e uma barra chapada de 8 px em cima de um canto
  // de 30 px saia quadrada.

  { float fx = N1312_X + N1312_PAD;
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("NOVO NA 1.4"),
                             155, 166, 185, 255);
      txt_desenhar_alpha(t, fx, N1312_Y + dy + 56.0f, a * 0.92f); }
    txt_bloco(TXT_TITULO2,
              i18n("O que mudou para você"),
              248, 249, 252, fx, N1312_Y + dy + 88.0f,
              N1312_FIG_W, 58.0f, a, 2);
    txt_bloco(TXT_BODY,
              i18n("A experiência segue recebendo cuidado para abrir, trocar e navegar com mais leveza."),
              205, 213, 226, fx, N1312_Y + dy + 218.0f,
              N1312_FIG_W, 34.0f, a * 0.98f, 3);
    desenhaCelebracao(fx, N1312_Y + dy + 292.0f, N1312_FIG_W, 500.0f,
                      a, fase); }

  // O lado direito agora é um mapa completo das mudanças que a pessoa sente,
  // em duas colunas. A entrada escalonada dá ritmo sem exigir interação.
  { int i;
    float y0 = N1312_Y + dy + 58.0f;
    for (i = 0; i < (int)(sizeof n1312_features / sizeof n1312_features[0]); i++) {
      int linha = i / 2, coluna = i & 1;
      float x = N1312_TXT_X + coluna * (N1312_FEAT_COL_W + 24.0f);
      float y = y0 + linha * N1312_FEAT_H;
      feature(x, y, N1312_FEAT_COL_W, &n1312_features[i],
              i, 0.035f * (float)i, a);
    } }

  // O RODAPE E UM BOTAO DA TABELA (botoes.h), primario e sempre em foco —
  // e o unico controle do cartao, e "OK" precisa de um lugar para apontar. A
  // dica "OK ou Voltar" fica a esquerda dele em cinza 170, e nao em foco2:
  // foco2 e a tinta SOBRE o realce (60 no tema branco) e sobre o painel
  // escuro sairia ilegivel.
  { const char *rot = i18n("Continuar");
    float bw = botao_largura(rot, NULL, 1);
    GfxRect b = { N1312_X + N1312_W - N1312_PAD - bw,
                  N1312_Y + dy + N1312_H - N1312_PAD - BOTAO_H_PRIMARIO + 16.0f,
                  bw, BOTAO_H_PRIMARIO };
    TxtLinha t = txt_linha(TXT_CAPTION2, i18n("OK ou Voltar para fechar este aviso"),
                           170, 174, 184, 255);
    botao_pilula(b, rot, NULL, 1.0f, 1, 0, a);
    txt_desenhar_alpha(t, b.x - 24.0f - (float)t.w,
                       b.y + (BOTAO_H_PRIMARIO - (float)t.h) * 0.5f, a * 0.92f); }
  gfx_sem_recorte();
}
