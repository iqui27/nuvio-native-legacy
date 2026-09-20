#include "agendaviso.h"
#include "agenda.h"
#include "agendaui.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "idioma.h"
#include <stdio.h>
#include <string.h>

// Medidas do cartao de atualizacao, que e o irmao deste: mesma largura, mesma
// entrada, mesmo rodape. Dois cartoes com geometria diferente na mesma home
// leem como dois apps.
#define AV_MAX      4
#define AV_W      820.0f
#define AV_PAD     48.0f
#define AV_LINHA  118.0f
// A SINOPSE DO EPISODIO, quando o cache tem. Entrou com a linha do tempo da
// Agenda, pelo mesmo argumento: ela ja estava dentro do /tv/<id> que agenda.c
// baixa e era descartada. Aqui vale mais que la — este cartao aparece uma unica
// vez por episodio e a pergunta que ele deixava sem resposta era "estreou o
// que, sobre o que". Duas linhas: a terceira e texto que ninguem le num cartao
// que se fecha com um OK.
#define AV_SIN_LD   30.0f
#define AV_SIN_H   (AV_SIN_LD * 2.0f + 6.0f)
#define AV_CARTAZ  72.0f
// ALTURA DO CABECALHO, e ela e UMA constante usada nos dois lugares — a
// altura do cartao e o topo da primeira linha. Estavam separadas (208 no
// desenho, 208 na conta) e o despertador, ao empurrar o titulo para baixo,
// fez a frase de apoio cair em cima do primeiro cartaz. Uma constante so.
#define AV_CAB    292.0f
#define AV_ABRIR_MS  230.0f
#define AV_FECHAR_MS 150.0f

static int   aberto;
static float entrada;
static AgItem itens[AV_MAX];
static int    n;

static float alturaItem(int i) {
  return AV_LINHA + (itens[i].sinopse[0] ? AV_SIN_H : 0.0f);
}

static float alturaCartao(void) {
  float h = AV_CAB + 108.0f;
  int i;
  for (i = 0; i < n; i++) h += alturaItem(i);
  return h;
}

void agendaviso_mostrar_se_houver(void) {
  const AgItem *devidos[AV_MAX];
  int q, i;
  if (aberto) return;
  agenda_iniciar();
  agenda_montar();
  q = agenda_devidos(devidos, AV_MAX);
  if (q <= 0) return;
  for (i = 0; i < q; i++) itens[i] = *devidos[i];
  n = q;
  aberto = 1;
}

int agendaviso_aberto(void) { return aberto; }

// Fechar MARCA, e por isso fechar e a unica saida. A marca e por episodio: se
// o TMDB anunciar outra data para a mesma serie, agenda_registrar zera o
// avisado e o cartao volta — uma vez, para o episodio novo.
static void fechar(void) {
  int i;
  for (i = 0; i < n; i++) agenda_marcar_avisado(itens[i].imdb);
  aberto = 0;
}

void agendaviso_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) fechar();
}

void agendaviso_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? AV_ABRIR_MS : AV_FECHAR_MS);
}

void agendaviso_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy, x, y, h;
  int i;
  if (entrada < 0.002f) return;

  h = alturaCartao();
  dy = (1.0f - a) * 36.0f;
  x = (NV_TELA_W - AV_W) * 0.5f;
  y = (NV_TELA_H - h) * 0.5f + dy;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.72f * entrada);
  { GfxRect c = { x, y, AV_W, h };
    // O raio do gfx_cor e FRACAO DA ALTURA. 24 px sobre a altura do cartao.
    gfx_cor(c, 24.0f / h, 0.075f, 0.078f, 0.088f, 0.98f * a); }
  gfx_recorte(x, y, AV_W, h);

  { float tx = x + AV_PAD, ty = y + 44.0f;
    // O DESPERTADOR abre o cartao, com o mesmo desenho e a mesma animacao da
    // tela Agenda e do botao do hero: e por ele que o dono reconhece de que
    // cartao se trata antes de ler uma palavra.
    // VERDE, como em toda parte onde este app fala de lembrete LIGADO — e este
    // cartao so existe porque um lembrete estava ligado. Sobre a superficie
    // escura do cartao entra o esmeralda cheio; ver agendaui_cor_lembrete.
    { GfxRect ic = { tx, ty, 56.0f, 56.0f };
      float cr, cg, cb;
      agendaui_cor_lembrete(1, 0, &cr, &cg, &cb);
      agendaui_despertador(ic, 1, cr, cg, cb, a, agora, 0); }
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("LEMBRETE"), 150, 154, 165, 255);
      txt_desenhar_alpha(t, tx + 74.0f, ty + 12.0f, a * 0.92f); }
    ty += 74.0f;
    { const char *tit = n == 1 ? i18n("Estreou hoje") : i18n("Estrearam hoje");
      TxtLinha t = txt_linha(TXT_TITULO1, tit, 246, 247, 252, 255);
      txt_desenhar_alpha(t, tx, ty, a); ty += (float)t.h + 8.0f; }
    // A FRASE QUE DIZ A VERDADE SOBRE O MECANISMO. Sem ela o cartao poderia ser
    // lido como "a TV me avisou", e a TV nao avisou nada: ele apareceu porque o
    // app foi aberto. Dizer isso uma vez, no lugar onde a duvida nasce.
    { TxtLinha t = txt_linha(TXT_CAPTION,
        i18n("Você pediu para ser lembrado destes episódios."),
        176, 180, 190, 255);
      txt_desenhar_alpha(t, tx, ty, a * 0.9f); }

    ty = y + AV_CAB;
    for (i = 0; i < n; i++) {
      const AgItem *it = &itens[i];
      GfxRect cz = { tx, ty + (AV_LINHA - AV_CARTAZ * 1.5f) * 0.5f,
                     AV_CARTAZ, AV_CARTAZ * 1.5f };
      GLuint tex = it->poster[0] ? tex_obter_larg(it->poster, AV_CARTAZ) : 0;
      float px = tx + AV_CARTAZ + 24.0f;
      char linha[220];
      if (tex) {
        gfx_tex_aspect_atual = 1.0f / 1.5f;
        gfx_rect(cz, tex, GFX_CARD, 0, 0, 0, 0.14f, 0, 0, 0, a);
      } else gfx_cor(cz, 0.14f, 0.18f, 0.19f, 0.21f, a);
      { TxtLinha t = txt_linha_corta(TXT_HEADLINE,
          it->titulo[0] ? it->titulo : i18n("Série"), 246, 247, 252, 255,
          AV_W - (px - x) - AV_PAD);
        txt_desenhar_alpha(t, px, ty + 26.0f, a); }
      linha[0] = 0;
      if (it->temporada > 0 && it->episodio > 0) {
        if (it->nomeEp[0])
          snprintf(linha, sizeof linha, i18n("T%dE%d · %s"),
                   it->temporada, it->episodio, it->nomeEp);
        else
          snprintf(linha, sizeof linha, i18n("T%dE%d"), it->temporada, it->episodio);
      }
      if (linha[0]) {
        float ar, ag, ab;
        ajustes_acento(&ar, &ag, &ab);
        TxtLinha t = txt_linha_corta(TXT_CAPTION, linha,
                                     (int)(ar * 255.0f + 0.5f),
                                     (int)(ag * 255.0f + 0.5f),
                                     (int)(ab * 255.0f + 0.5f), 255,
                                     AV_W - (px - x) - AV_PAD);
        txt_desenhar_alpha(t, px, ty + 68.0f, a);
      }
      // A sinopse comeca ALINHADA COM O TITULO, e nao sob o cartaz: o cartaz e
      // uma coluna propria, e texto correndo por baixo dele desfaz a unica
      // estrutura que este cartao tem.
      if (it->sinopse[0])
        agendaui_sinopse(TXT_CAPTION, it->sinopse, px, ty + AV_LINHA - 8.0f,
                         AV_W - (px - x) - AV_PAD, AV_SIN_LD, 2,
                         168, 172, 182, a * 0.92f);
      ty += alturaItem(i);
    }

    // Um botao so, sempre em foco: nao ha escolha a fazer aqui. Pilula clara
    // com texto escuro, o vocabulario de modal deste app.
    { const char *rot = i18n("Entendi");
      float fr, fg, fb, ti = ajustes_acento_tinta(&fr, &fg, &fb);
      int c = (int)(ti * 255.0f + 0.5f);
      TxtLinha t = txt_linha(TXT_CALLOUT, rot, c, c, c, 255);
      GfxRect b = { tx, y + h - 96.0f, (float)t.w + 64.0f, 64.0f };
      gfx_cor(b, NV_RAIO_PILL, fr, fg, fb, a);
      txt_desenhar_alpha(t, b.x + 32.0f, b.y + (64.0f - (float)t.h) * 0.5f, a); }
    { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("Ver tudo em Agenda"),
                             150, 154, 165, 255);
      txt_desenhar_alpha(t, x + AV_W - AV_PAD - (float)t.w, y + h - 72.0f,
                         a * 0.85f); } }
  gfx_sem_recorte();
}
