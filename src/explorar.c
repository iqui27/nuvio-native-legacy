// Explorar: palco preto, particulas discretas e uma unica escolha grande.
// A tela foi feita para controle remoto: esquerda/direita trocam a obra,
// cima baixa o foco na acao e OK abre a ficha existente.
#include "explorar.h"
#include "catalogo.h"
#include "tex_cache.h"
#include "gfx.h"
#include "text.h"
#include "badges.h"
#include "anim.h"
#include "ajustes.h"
#include "idioma.h"
#include "layout.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define EX_ITEM_MAX       9
#define EX_PART_MAX       24
#define EX_POSTER_W       500.0f
#define EX_POSTER_H       750.0f
#define EX_POSTER_X       72.0f
#define EX_POSTER_Y       188.0f
#define EX_VIZ_X          1650.0f
#define EX_VIZ_Y          365.0f
#define EX_VIZ_W          220.0f
#define EX_VIZ_H          330.0f
#define EX_DOSSIER_X      650.0f
#define EX_DOSSIER_W      900.0f

typedef struct {
  int indice;
  const CatItem *ci;
} ExItem;

typedef struct {
  float x, y, velocidade, tamanho, fase, orbita, angular, profundidade;
  int cor;
} ExParticula;

static ExItem itens[EX_ITEM_MAX];
static ExParticula particulas[EX_PART_MAX];
static int nItens;
static int foco;
static int focoAnterior;
static float focoPos;
static float acaoFoco;
static float tempo;
static unsigned revisao;
static int acao;
static int sair;
static int pediuAbrir;

static unsigned sorteio(unsigned *estado) {
  *estado = *estado * 1664525u + 1013904223u;
  return *estado;
}

static int pontuacao(const CatItem *ci) {
  int p = 0;
  if (!ci) return -1;
  if (ci->poster[0]) p += 100;
  if (ci->backdrop[0]) p += 20;
  if (ci->nota > 0) p += ci->nota;
  if (ci->naLista) p += 48;
  if (ci->progresso > 0 && ci->progresso < 100) p += 38;
  if (ci->sinopse[0]) p += 8;
  return p;
}

static void itensReconstruir(void) {
  int total = cat_n();
  int i;
  nItens = 0;
  // Seleciona poucos itens com dados de arte e meta. O limite e deliberado:
  // vizinhos parciais dao contexto sem virar uma grade, e mantem o cache de
  // textura focado no que cabe na faixa de 1920px.
  while (nItens < EX_ITEM_MAX) {
    int melhor = -1, melhorNota = -1;
    for (i = 0; i < total; i++) {
      const CatItem *ci = cat_item(i);
      int j, nota;
      if (!ci || !ci->titulo[0]) continue;
      for (j = 0; j < nItens; j++) if (itens[j].indice == i) break;
      if (j < nItens) continue;
      nota = pontuacao(ci);
      // Mantem o desempate na ordem do catalogo: a pagina muda quando a
      // sincronizacao muda, mas nao embaralha a experiencia a cada quadro.
      if (nota > melhorNota) { melhor = i; melhorNota = nota; }
    }
    if (melhor < 0) break;
    itens[nItens].indice = melhor;
    itens[nItens].ci = cat_item(melhor);
    nItens++;
  }
  if (foco >= nItens) foco = nItens > 0 ? nItens - 1 : 0;
  if (foco < 0) foco = 0;
  if (focoPos > (float)(nItens > 0 ? nItens - 1 : 0))
    focoPos = (float)(nItens > 0 ? nItens - 1 : 0);
}

static void particulasIniciar(void) {
  unsigned estado = 0x4e765231u;
  int i;
  for (i = 0; i < EX_PART_MAX; i++) {
    unsigned r = sorteio(&estado);
    particulas[i].x = (float)(r % 1920u);
    particulas[i].y = 110.0f + (float)((r >> 8) % 820u);
    particulas[i].profundidade = 0.30f + (float)((r >> 16) % 70u) * 0.01f;
    particulas[i].velocidade = 5.0f + particulas[i].profundidade * 18.0f;
    particulas[i].tamanho = 1.8f + particulas[i].profundidade * 4.2f;
    particulas[i].fase = (float)(r % 1000u) * 0.001f;
    particulas[i].orbita = 14.0f + (float)((r >> 20) % 66u);
    particulas[i].angular = 0.12f + particulas[i].profundidade * 0.20f;
    particulas[i].cor = (int)((r >> 27) % 4u);
  }
}

static void corParticula(int cor, float *r, float *g, float *b) {
  static const float paleta[][3] = {
    { 0.22f, 0.46f, 0.82f },
    { 0.38f, 0.28f, 0.78f },
    { 0.68f, 0.36f, 0.82f },
    { 0.26f, 0.70f, 0.78f }
  };
  *r = paleta[cor & 3][0];
  *g = paleta[cor & 3][1];
  *b = paleta[cor & 3][2];
}

static const char *arteDe(const CatItem *ci, int fundo) {
  if (!ci) return NULL;
  if (fundo) return ci->backdrop[0] ? ci->backdrop : (ci->poster[0] ? ci->poster : NULL);
  return ci->poster[0] ? ci->poster : (ci->backdrop[0] ? ci->backdrop : NULL);
}

static void acrescentar(char *dst, size_t n, const char *s) {
  size_t k;
  if (!dst || !n || !s || !s[0]) return;
  k = strlen(dst);
  if (k && k + 4 < n) snprintf(dst + k, n - k, "   ·   %s", s);
  else if (!k) snprintf(dst, n, "%s", s);
}

static int tamanhoPrefixoTipo(const char *s) {
  const char *tipos[] = {
    i18n("Programa de TV"), "TV Show", i18n("Filme"), "Movie",
    i18n("Série"), "Series"
  };
  int i;
  if (!s) return 0;
  for (i = 0; i < (int)(sizeof tipos / sizeof tipos[0]); i++) {
    size_t k = strlen(tipos[i]);
    if (!strncmp(s, tipos[i], k) && !strncmp(s + k, " · ", 3))
      return (int)k + 3;
  }
  return 0;
}

static void campoSemTipo(const char *src, char *dst, size_t n) {
  const char *p;
  size_t k;
  if (!dst || !n) return;
  dst[0] = 0;
  if (!src || !src[0]) return;
  p = src + tamanhoPrefixoTipo(src);
  snprintf(dst, n, "%s", p);
  // Alguns addons colocam o tipo no fim do meta ("2024 · TV Show").
  // Remover esse ultimo segmento conserva a mesma informacao sem ecoar o
  // rotulo tres vezes na linha curta do dossie.
  {
    char sufixo[96];
    const char *tipos[] = {
      i18n("Programa de TV"), "TV Show", i18n("Filme"), "Movie",
      i18n("Série"), "Series"
    };
    int i;
    for (i = 0; i < (int)(sizeof tipos / sizeof tipos[0]); i++) {
      snprintf(sufixo, sizeof sufixo, " · %s", tipos[i]);
      {
        char *q = strstr(dst, sufixo);
      if (q) { *q = 0; break; }
      }
    }
  }
  k = strlen(dst);
  while (k && dst[k - 1] == ' ') dst[--k] = 0;
}

static void metaDo(const CatItem *ci, char *dst, size_t n) {
  char meta[120];
  const char *tipo;
  if (!dst || !n) return;
  dst[0] = 0;
  if (!ci) return;
  tipo = i18n(!strcmp(ci->tipo, "series") ? "Série" : "Filme");
  acrescentar(dst, n, tipo);
  campoSemTipo(ci->meta, meta, sizeof meta);
  acrescentar(dst, n, meta);
  if (ci->restanteMin > 0 && ci->progresso > 0 && ci->progresso < 100) {
    char r[48];
    snprintf(r, sizeof r, "%d min restantes", ci->restanteMin);
    acrescentar(dst, n, r);
  }
}

static void estadoDo(const CatItem *ci, char *dst, size_t n) {
  if (!dst || !n) return;
  if (ci && ci->naLista)
    snprintf(dst, n, "%s", i18n("Na sua lista"));
  else if (ci && ci->progresso > 0 && ci->progresso < 100)
    snprintf(dst, n, "%s", i18n("Progresso salvo"));
  else
    snprintf(dst, n, "%s", i18n("Não assistido"));
}

static void elencoDo(const CatItem *ci, char *dst, size_t n) {
  int i;
  if (!dst || !n) return;
  dst[0] = 0;
  if (!ci || ci->nElenco <= 0) return;
  for (i = 0; i < ci->nElenco && i < 2; i++) {
    if (!ci->elenco[i].nome[0]) continue;
    if (dst[0]) acrescentar(dst, n, ci->elenco[i].nome);
    else snprintf(dst, n, "%s", ci->elenco[i].nome);
  }
}

static void motivoDo(const CatItem *ci, char *dst, size_t n) {
  if (!dst || !n) return;
  if (!ci) { snprintf(dst, n, "%s", i18n("Uma curadoria por tema e atmosfera")); return; }
  if (ci->naLista)
    snprintf(dst, n, "%s", i18n("Na sua lista"));
  else if (ci->progresso > 0 && ci->progresso < 100) {
    char p[64];
    snprintf(p, sizeof p, i18n("%d%% assistido"), ci->progresso);
    snprintf(dst, n, "%s   ·   %s", i18n("Você já começou este título"), p);
  }
  else if (ci->nota >= 80)
    snprintf(dst, n, "%s", i18n("Uma nota forte para começar"));
  else if (ci->genero[0])
    snprintf(dst, n, "%s", i18n("Uma curadoria por tema e atmosfera"));
  else
    snprintf(dst, n, "%s", i18n("Uma curadoria por tema e atmosfera"));
}

static void desenharParticulas(float alfa, int reduzida) {
  int i;
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  for (i = 0; i < EX_PART_MAX; i++) {
    const ExParticula *p = &particulas[i];
    float t = reduzida ? 0.0f : tempo;
    float angulo = p->fase * 6.2831853f + t * p->angular;
    float ciclo = NV_TELA_W + p->orbita * 2.0f;
    float x = p->x + fmodf(p->velocidade * t, ciclo) - p->orbita;
    float y, campo, r, g, b;
    x += cosf(angulo) * p->orbita;
    y = p->y + sinf(angulo * 0.91f) * p->orbita * 0.64f
           + cosf(t * 0.20f + p->fase * 6.2831853f) * 14.0f;
    while (x < -p->orbita) x += ciclo;
    while (x > NV_TELA_W + p->orbita) x -= ciclo;
    // O miolo do dossie fica limpo: a referencia existe, mas nao disputa
    // titulo, nota e sinopse a tres metros da TV.
    campo = (x > EX_DOSSIER_X - 70.0f && y > 160.0f && y < 870.0f) ? 0.24f : 1.0f;
    corParticula(p->cor, &r, &g, &b);
    r = r * 0.70f + ar * 0.30f;
    g = g * 0.70f + ag * 0.30f;
    b = b * 0.70f + ab * 0.30f;
    gfx_cor((GfxRect){ x - p->tamanho, y - p->tamanho,
                       p->tamanho * 2.0f, p->tamanho * 2.0f },
            0.48f, r, g, b, alfa * campo * (0.05f + p->profundidade * 0.07f));
    gfx_cor((GfxRect){ x, y, p->tamanho, p->tamanho * 0.72f },
            0.38f, r, g, b, alfa * campo * (0.34f + p->profundidade * 0.30f));
  }
}

static void desenharFundo(Uint32 agora) {
  float ar, ag, ab, cr, cg, cb;
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  (void)agora;
  gfx_cor(tela, 0.0f, NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
  ajustes_acento(&ar, &ag, &ab);
  cr = ar; cg = ag; cb = ab;
  if (nItens > 0) {
    const CatItem *ci = itens[foco].ci;
    const char *arte = arteDe(ci, 1);
    if (arte) tex_cor_fundo(arte, &cr, &cg, &cb);
  }
  // So ha luz difusa e particulas: nenhuma arte fica solta atras do dossie.
  // As duas manchas sao limitadas para manter o fill-rate sob controle.
  gfx_rect((GfxRect){ -120.0f, 210.0f, 800.0f, 650.0f }, 0, GFX_SOMBRA,
           1.0f, 0, 0, 0.5f, cr, cg, cb, 0.20f);
  gfx_rect((GfxRect){ 1010.0f, 20.0f, 680.0f, 500.0f }, 0, GFX_SOMBRA,
           1.0f, 0, 0, 0.5f, ar, ag, ab, 0.10f);
  desenharParticulas(1.0f, ajustes_animacoes_reduzidas());
  gfx_rect((GfxRect){ 0, 0, NV_TELA_W, 180.0f }, 0, GFX_VEU_TOPO,
           0, 0, 0, 0, 0, 0, 0, 0.86f);
  gfx_rect((GfxRect){ 0, NV_TELA_H - 100.0f, NV_TELA_W, 100.0f }, 0,
           GFX_VEU_BAIXO, 0, 0, 0, 0, 0, 0, 0, 0.72f);
}

static void desenharPoster(const CatItem *ci, GfxRect r, int ativo, float alfa) {
  const char *arte = arteDe(ci, 0);
  GLuint tex = arte ? tex_obter_larg(arte, r.w) : 0;
  float ar, ag, ab;
  if (ativo) {
    ajustes_acento(&ar, &ag, &ab);
    gfx_rect((GfxRect){ r.x - 22.0f, r.y - 22.0f, r.w + 44.0f, r.h + 44.0f },
             0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.35f * alfa);
  }
  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(arte);
    gfx_rect(r, tex, GFX_CARD, ativo ? 1.0f : 0.0f, 0, 0,
             ativo ? 0.028f : 0.035f, 1, 1, 1, alfa);
    gfx_tex_aspect_atual = 0.0f;
  } else {
    gfx_cor(r, ativo ? 0.028f : 0.035f, 0.12f, 0.13f, 0.16f, alfa);
  }
}

static void desenharFaixa(void) {
  int viz = foco + 1 < nItens ? foco + 1 : (foco > 0 ? foco - 1 : -1);
  // O vizinho entra primeiro e fica ancorado na margem direita. O cartaz
  // focado ocupa a margem esquerda; entre os dois sobra uma coluna editorial
  // continua, sem cards fantasma atravessando texto ou nota.
  if (viz >= 0)
    desenharPoster(itens[viz].ci,
                   (GfxRect){ EX_VIZ_X, EX_VIZ_Y, EX_VIZ_W, EX_VIZ_H },
                   0, 0.76f);
  if (nItens > 0)
    desenharPoster(itens[foco].ci,
                   (GfxRect){ EX_POSTER_X, EX_POSTER_Y,
                              EX_POSTER_W, EX_POSTER_H },
                   1, 1.0f);
}

static void sinopseLinhas(const char *src, char *a, size_t na,
                          char *b, size_t nb) {
  size_t len, corte, inicio;
  if (!a || !na || !b || !nb) return;
  a[0] = 0; b[0] = 0;
  if (!src || !src[0]) return;
  len = strlen(src);
  corte = len > 126 ? 126 : len;
  if (corte < len) {
    while (corte && src[corte] != ' ') corte--;
    if (!corte) corte = len > 126 ? 126 : len;
  }
  while (corte && ((unsigned char)src[corte] & 0xc0) == 0x80) corte--;
  snprintf(a, na, "%.*s", (int)corte, src);
  inicio = corte;
  while (src[inicio] == ' ') inicio++;
  if (src[inicio]) snprintf(b, nb, "%s", src + inicio);
}

static void campoDossie(const char *rotulo, const char *valor,
                        float x, float y, float w) {
  TxtLinha r, v;
  if (!rotulo || !valor || !valor[0]) return;
  r = txt_linha(TXT_CAPTION2, rotulo, 158, 162, 173, 255);
  v = txt_linha_corta(TXT_BODY, valor, 226, 228, 233, 255, w);
  txt_desenhar(r, x, y);
  txt_desenhar(v, x, y + 28.0f);
}

static void desenharDossie(void) {
  const CatItem *ci = nItens > 0 ? itens[foco].ci : NULL;
  char meta[360], motivo[180], sinA[260], sinB[260];
  char genero[180], estado[96], temporadas[48], elenco[180];
  float x = EX_DOSSIER_X, y = 196.0f;
  int branco = 246, cinza = 200, cinza2 = 174;
  if (!ci) {
    TxtLinha v = txt_linha(TXT_TITULO2, i18n("Explorar"), branco, branco, branco, 255);
    TxtLinha s = txt_linha(TXT_BODY, i18n("Uma curadoria por tema e atmosfera"), cinza2, cinza2, cinza2, 255);
    txt_desenhar(v, x, y + 150.0f);
    txt_desenhar(s, x, y + 230.0f);
    return;
  }
  metaDo(ci, meta, sizeof meta);
  motivoDo(ci, motivo, sizeof motivo);
  campoSemTipo(ci->genero, genero, sizeof genero);
  estadoDo(ci, estado, sizeof estado);
  elencoDo(ci, elenco, sizeof elenco);
  {
    TxtLinha t = txt_linha_corta(TXT_TITULO1, ci->titulo, branco, branco, branco, 255, EX_DOSSIER_W);
    txt_desenhar(t, x, y);
  }
  {
    TxtLinha m = txt_linha_corta(TXT_HERO_META, meta, cinza, cinza, cinza, 255, 680.0f);
    txt_desenhar(m, x, y + 100.0f);
    if (ci->nota > 0)
      badge_imdb(x + 748.0f, y + 95.0f, ci->nota, 0, 1.0f);
  }
  gfx_cor((GfxRect){ x, y + 148.0f, 900.0f, 1.0f }, 0.5f,
          0.32f, 0.34f, 0.39f, 0.48f);
  if (ci->sinopse[0]) {
    sinopseLinhas(ci->sinopse, sinA, sizeof sinA, sinB, sizeof sinB);
    {
      TxtLinha s = txt_linha_corta(TXT_HERO_SIN, sinA, 232, 233, 237, 255, EX_DOSSIER_W);
      txt_desenhar(s, x, y + 174.0f);
      if (sinB[0]) {
        s = txt_linha_corta(TXT_HERO_SIN, sinB, 232, 233, 237, 255, EX_DOSSIER_W);
        txt_desenhar(s, x, y + 206.0f);
      }
    }
  }
  // Estado e genero sempre ocupam a primeira linha tecnica. Assim a coluna
  // continua informativa mesmo quando o feed ainda nao trouxe direcao ou pais.
  campoDossie(i18n("Estado"), estado, x, y + 262.0f, 350.0f);
  if (genero[0]) campoDossie(i18n("GÊNERO"), genero, x + 400.0f, y + 262.0f, 350.0f);
  if (ci->direcao[0]) campoDossie(i18n("Direção"), ci->direcao, x, y + 340.0f, 350.0f);
  if (ci->pais[0]) campoDossie(i18n("País de Origem"), ci->pais, x + 400.0f, y + 340.0f, 350.0f);
  if (ci->provNome[0]) campoDossie(i18n("Fonte"), ci->provNome, x, y + 418.0f, 350.0f);
  if (ci->classificacao[0]) campoDossie(i18n("Classificação"), ci->classificacao,
                                         x + 400.0f, y + 418.0f, 350.0f);
  if (ci->nTemporadas > 0) {
    snprintf(temporadas, sizeof temporadas, "%d", ci->nTemporadas);
    campoDossie(i18n("Temporadas"), temporadas, x, y + 496.0f, 350.0f);
  }
  if (elenco[0]) campoDossie(i18n("Elenco"), elenco, x + 400.0f, y + 496.0f, 350.0f);
  {
    TxtLinha h = txt_linha(TXT_CAPTION2, i18n("Por que assistir"), cinza2, cinza2, cinza2, 255);
    TxtLinha m = txt_linha_corta(TXT_CALLOUT, motivo, branco, branco, branco, 255, EX_DOSSIER_W);
    txt_desenhar(h, x, y + 574.0f);
    txt_desenhar(m, x, y + 612.0f);
  }
  if (ci->progresso > 0 && ci->progresso < 100) {
    GfxRect trilho = { x, y + 678.0f, 360.0f, 6.0f };
    GfxRect ativo = trilho;
    float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    ativo.w *= (float)ci->progresso / 100.0f;
    gfx_cor(trilho, 0.5f, 0.19f, 0.20f, 0.23f, 0.90f);
    gfx_cor(ativo, 0.5f, ar, ag, ab, 0.95f);
    {
      char p[80];
      TxtLinha l;
      snprintf(p, sizeof p, "%d%% assistido", ci->progresso);
      l = txt_linha(TXT_CAPTION2, p, cinza2, cinza2, cinza2, 255);
      txt_desenhar(l, x + 382.0f, y + 664.0f);
    }
  }
}

static void desenharAcao(void) {
  float ar, ag, ab;
  float a = acaoFoco;
  GfxRect r = { EX_DOSSIER_X, 905.0f, 252.0f, 52.0f };
  ajustes_acento(&ar, &ag, &ab);
  if (a > 0.01f) {
    // Uma linha de comando, nao um CTA: o foco tem contraste suficiente para
    // o remoto, mas nao cria uma segunda placa competindo com o cartaz.
    gfx_luz_canto(r, 0.30f, 24.0f, 26.0f, 190.0f, ar, ag, ab, 0.20f * a);
    gfx_cor((GfxRect){ r.x, r.y + r.h - 3.0f, r.w, 3.0f }, 0.5f,
            ar, ag, ab, 0.95f * a);
  } else {
    gfx_cor((GfxRect){ r.x, r.y + r.h - 2.0f, 92.0f, 2.0f }, 0.5f,
            0.35f, 0.36f, 0.40f, 0.70f);
  }
  {
    int c = a > 0.5f ? ajustes_tinta_foco() : 208;
    TxtLinha l = txt_linha(TXT_BODY, i18n("Abrir página"), c, c, c, 255);
    TxtLinha s = txt_linha(TXT_HEADLINE, "›", c, c, c, 255);
    txt_desenhar(l, r.x + 18.0f, r.y + (r.h - l.h) * 0.5f);
    txt_desenhar(s, r.x + r.w - s.w - 18.0f, r.y + (r.h - s.h) * 0.5f);
  }
  {
    TxtLinha ajuda = txt_linha(TXT_CAPTION2,
                               i18n("← →   Navegar   ·   ↑ ↓   Dossiê   ·   OK   Abrir"),
                               154, 157, 166, 255);
    txt_desenhar(ajuda, EX_DOSSIER_X, 992.0f);
  }
}

void explorar_iniciar(void) {
  nItens = 0;
  foco = 0;
  focoAnterior = 0;
  focoPos = 0.0f;
  acaoFoco = 0.0f;
  tempo = 0.0f;
  revisao = 0;
  acao = 0;
  sair = 0;
  pediuAbrir = 0;
  particulasIniciar();
  itensReconstruir();
  revisao = cat_revisao();
}

void explorar_encerrar(void) {
  nItens = 0;
  pediuAbrir = 0;
  sair = 0;
}

void explorar_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE) { sair = 1; return; }
  if (k == SDLK_LEFT) {
    if (acao) { acao = 0; return; }
    if (foco > 0) foco--;
    else sair = 1;
    return;
  }
  if (k == SDLK_RIGHT) {
    if (!acao && foco + 1 < nItens) foco++;
    return;
  }
  if (k == SDLK_UP) { acao = 1; return; }
  if (k == SDLK_DOWN) { acao = 0; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (nItens > 0) pediuAbrir = 1;
  }
}

void explorar_atualizar(float dt, Uint32 agora) {
  int reduzida = ajustes_animacoes_reduzidas();
  (void)agora;
  if (cat_revisao() != revisao) {
    itensReconstruir();
    revisao = cat_revisao();
  }
  if (foco != focoAnterior) {
    focoAnterior = foco;
  }
  tempo += reduzida ? 0.0f : dt;
  focoPos = reduzida ? (float)foco : anim_mola(focoPos, (float)foco, dt, 18.0f);
  acaoFoco = reduzida ? (float)acao : anim_mola(acaoFoco, (float)acao, dt, 24.0f);
}

void explorar_desenhar(Uint32 agora) {
  TxtLinha titulo;
  desenharFundo(agora);
  titulo = txt_linha(TXT_TITULO2, i18n("Explorar"), 246, 246, 248, 255);
  txt_desenhar(titulo, ajustes_conteudo_x(), 54.0f);
  {
    TxtLinha sub = txt_linha(TXT_BODY, i18n("Uma curadoria por tema e atmosfera"),
                             174, 177, 187, 255);
    txt_desenhar(sub, ajustes_conteudo_x(), 130.0f);
  }
  desenharFaixa();
  desenharDossie();
  desenharAcao();
}

int explorar_quer_sair(void) {
  int v = sair;
  sair = 0;
  return v;
}

int explorar_pediu_abrir(int *indice) {
  if (!pediuAbrir || nItens <= 0) return 0;
  pediuAbrir = 0;
  if (indice) *indice = itens[foco].indice;
  return 1;
}
