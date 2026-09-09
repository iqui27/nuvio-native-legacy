// Painel "Salvos" — a camada da direita que a tecla AZUL abre. Ver salvospainel.h
// para o que ele substituiu e por que.
//
// O QUE ELE MOSTRA, e a decisao nao e obvia: a UNIAO das tres fontes de "quero
// ver", nao so a lista local. As tres caem na mesma marca (`CatItem.naLista`):
// a watchlist do Trakt (descoberta.c), a biblioteca da conta (contalib.c) e a
// lista local (salvos.c). A aba "Salvos" da tela de Biblioteca ja mostra essa
// uniao, e duas telas chamadas "Salvos" mostrando conjuntos diferentes seria
// exatamente o defeito que o app irmao teve com quatro botoes "+".
//
// A lista local entra por fora do catalogo de proposito. Ela guarda titulo,
// poster e meta no proprio arquivo (ver salvos.h), entao o painel se desenha no
// primeiro quadro do arranque — antes de a descoberta responder. Sem isso o
// atalho mais rapido do controle abriria vazio por ~20 s toda vez que a TV
// liga, que e justamente quando alguem aperta.
#include "salvospainel.h"
#include "salvos.h"
#include "catalogo.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "idioma.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// Mesma pegada do painel "Sua atividade" que ele substitui (perfil.c desenhava
// em x=1120, 776x1032): quem ja tinha o gesto na memoria muscular encontra a
// camada no mesmo lugar, so com outro conteudo.
#define SP_X          1120.0f
#define SP_W           776.0f
#define SP_Y            24.0f
#define SP_H          1032.0f
#define SP_PAD          44.0f
#define SP_INTERNO    (SP_W - SP_PAD * 2.0f)
#define SP_LISTA_Y     200.0f
#define SP_LISTA_BASE (SP_Y + SP_H - 24.0f)
#define SP_POSTER_W     92.0f
#define SP_POSTER_H    138.0f
#define SP_PASSO       160.0f
#define SP_SECAO_H      54.0f
#define SP_TEXTO_X    (SP_PAD + SP_POSTER_W + 20.0f)
#define SP_TEXTO_W    (SP_INTERNO - SP_POSTER_W - 20.0f)
// Barra de progresso do card de retomada: a mesma altura da que a home usa nos
// cards de "Continuar assistindo", para as duas lerem como a mesma coisa.
#define SP_BARRA_W     360.0f
#define SP_BARRA_H       6.0f
// Entrada e saida com o MESMO relogio do menu lateral (menu.c): as duas camadas
// aparecem no mesmo app e tempos diferentes se leem como bug, nao como estilo.
#define SP_ABRIR_MS    230.0f
#define SP_FECHAR_MS   150.0f
#define SP_VEU           0.58f

#define SP_MAX 200

// Linha ja resolvida: o desenho nao volta ao catalogo nem a lista local por
// quadro. Os ponteiros de texto apontam para memoria estavel — `titulo` e
// `poster` vem ou do vetor estatico de salvos.c ou do CatItem, e os dois vivem
// enquanto o app viver.
typedef struct {
  const char *titulo, *poster, *meta;
  char  id[24];
  int   serie;
  int   nota;
  int   progresso, temporada, episodio, restanteMin;
  long long quandoS;      // 0 = veio do Trakt/conta, nao sabemos quando entrou
} SPLinha;

static SPLinha linhas[SP_MAX];
static int nLinhas;
static int nCont;            // quantas das primeiras linhas sao "Continuar"
static int aberto, foco, marcaCatN = -1;
static float entrada, scrollY;
static float animFoco[SP_MAX];
static char  pedido[24];
static int   temPedido;

int spainel_aberto(void)  { return aberto; }
int spainel_visivel(void) { return aberto || entrada > 0.002f; }

const char *spainel_pediu_abrir(void) {
  if (!temPedido) return NULL;
  temPedido = 0;
  return pedido;
}

static int ehSerie(const char *tipo, int nTemporadas) {
  return (tipo && !strcmp(tipo, "series")) || nTemporadas > 0;
}

static int jaTem(const char *id) {
  int i;
  for (i = 0; i < nLinhas; i++) if (!strcmp(linhas[i].id, id)) return 1;
  return 0;
}

// Monta a lista visivel. Duas passadas e uma reordenacao:
//   1. a lista LOCAL, na ordem de insercao (ela existe mesmo sem catalogo);
//   2. o que o catalogo tem marcado como naLista e ainda nao entrou;
//   3. os itens COM progresso sobem para o topo, virando a secao "Continuar".
// A reordenacao e uma insercao estavel: dentro de cada secao a ordem das duas
// passadas e preservada, senao a lista dancaria a cada reconstrucao.
static void reconstruir(void) {
  int i, n, escrita = 0;
  nLinhas = 0;
  n = salvos_n();
  for (i = 0; i < n && nLinhas < SP_MAX; i++) {
    const SalvoItem *s = salvos_item(i);
    SPLinha *l;
    int k;
    if (!s) continue;
    l = &linhas[nLinhas++];
    memset(l, 0, sizeof *l);
    snprintf(l->id, sizeof l->id, "%s", s->id);
    l->titulo = s->titulo;
    l->poster = s->poster[0] ? s->poster : NULL;
    l->meta   = s->meta;
    l->nota   = s->nota;
    l->quandoS = s->quandoS;
    l->serie  = ehSerie(s->tipo, 0);
    // O PROGRESSO SO EXISTE NO CATALOGO. A lista local guarda o que e dela
    // (titulo, poster, quando entrou); posicao de retomada e de progresso.c e
    // muda sem passar por aqui. Guardar uma copia envelheceria em minutos.
    k = cat_indice_por_imdb(s->id);
    if (k >= 0) {
      const CatItem *c = cat_item(k);
      if (c) {
        l->progresso = c->progresso;
        l->temporada = c->temporada;
        l->episodio  = c->episodio;
        l->restanteMin = c->restanteMin;
        if (c->nota > 0) l->nota = c->nota;
        if (c->poster[0]) l->poster = c->poster;
        if (c->meta[0])   l->meta = c->meta;
        if (ehSerie(c->tipo, c->nTemporadas)) l->serie = 1;
      }
    }
  }
  n = cat_n();
  for (i = 0; i < n && nLinhas < SP_MAX; i++) {
    const CatItem *c = cat_item(i);
    SPLinha *l;
    if (!c || !c->naLista || !c->imdb[0] || jaTem(c->imdb)) continue;
    l = &linhas[nLinhas++];
    memset(l, 0, sizeof *l);
    snprintf(l->id, sizeof l->id, "%s", c->imdb);
    l->titulo = c->titulo;
    l->poster = c->poster[0] ? c->poster : NULL;
    l->meta   = c->meta;
    l->nota   = c->nota;
    l->serie  = ehSerie(c->tipo, c->nTemporadas);
    l->progresso = c->progresso;
    l->temporada = c->temporada;
    l->episodio  = c->episodio;
    l->restanteMin = c->restanteMin;
  }
  // Estavel: percorre uma vez e move para a frente quem tem progresso.
  for (i = 0; i < nLinhas; i++) {
    if (linhas[i].progresso <= 0) continue;
    if (i != escrita) {
      SPLinha t = linhas[i];
      memmove(&linhas[escrita + 1], &linhas[escrita],
              sizeof(SPLinha) * (size_t)(i - escrita));
      linhas[escrita] = t;
    }
    escrita++;
  }
  nCont = escrita;
  marcaCatN = cat_n();
  if (foco >= nLinhas) foco = nLinhas > 0 ? nLinhas - 1 : 0;
}

void spainel_abrir(void) {
  if (aberto) return;
  aberto = 1;
  foco = 0;
  scrollY = 0.0f;
  memset(animFoco, 0, sizeof animFoco);
  reconstruir();
}

void spainel_fechar(void) { aberto = 0; }

// Altura ate o TOPO da linha `i`, contando o cabecalho de cada secao. Nao e
// `i * SP_PASSO`: o rotulo "Não começados" empurra tudo que vem depois dele, e
// sem contar esse empurrao a rolagem para a linha focada erra por 54px — o
// suficiente para o card focado ficar meio escondido atras do cabecalho.
static float topoDe(int i) {
  // Rotulo da primeira secao, sempre; mais o de "Não começados" para quem vem
  // depois dele. Com nCont == 0 nao existe segunda secao — a unica que aparece
  // e "Sua lista", e o segundo termo tem de ser zero para todo mundo.
  float y = SP_SECAO_H + (float)i * SP_PASSO;
  if (nCont > 0 && i >= nCont) y += SP_SECAO_H;
  return y;
}

void spainel_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  // Mesmo conjunto de "voltar" que o menu lateral aceita, mais a ESQUERDA: o
  // painel encosta na borda direita da tela, entao sair por ele e ir para a
  // esquerda. E o gesto que perfil.c ja tinha nesta mesma posicao.
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || k == SDLK_LEFT ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) { spainel_fechar(); return; }
  if (k == SDLK_DOWN) { if (foco + 1 < nLinhas) foco++; return; }
  if (k == SDLK_UP)   { if (foco > 0) foco--; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (foco >= 0 && foco < nLinhas) {
      snprintf(pedido, sizeof pedido, "%s", linhas[foco].id);
      temPedido = 1;
      aberto = 0;
    }
    return;
  }
}

void spainel_atualizar(float dt, Uint32 agora) {
  int i;
  float alvo, topo, base;
  (void)agora;
  if (!aberto && entrada < 0.002f) {
    if (entrada != 0.0f) entrada = 0.0f;
    return;
  }
  // O catalogo pode ter sido republicado com o painel aberto (a descoberta faz
  // isso varias vezes por ciclo). Sem esta reconstrucao a lista continuaria a
  // do instante da abertura, com ponteiros de titulo apontando para CatItem que
  // ja mudou de conteudo — texto de outro filme no card certo.
  if (aberto && cat_n() != marcaCatN) reconstruir();

  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? SP_ABRIR_MS : SP_FECHAR_MS);
  for (i = 0; i < nLinhas && i < SP_MAX; i++) {
    float a = (aberto && i == foco) ? 1.0f : 0.0f;
    animFoco[i] = ajustes_animacoes_reduzidas()
      ? a
      : anim_mola(animFoco[i], a, dt,
                  a > animFoco[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  // Rola o MINIMO para a linha focada caber inteira, como a grade da
  // Biblioteca. Alinhar a focada ao topo joga o cabecalho para fora na primeira
  // descida e a pessoa perde de vista em que painel esta.
  alvo = scrollY;
  if (nLinhas > 0 && foco >= 0 && foco < nLinhas) {
    topo = topoDe(foco);
    base = topo + SP_POSTER_H;
    if (base - alvo > SP_LISTA_BASE - SP_LISTA_Y) alvo = base - (SP_LISTA_BASE - SP_LISTA_Y);
    if (topo - alvo < 0.0f) alvo = topo;
  }
  if (alvo < 0.0f) alvo = 0.0f;
  scrollY = ajustes_animacoes_reduzidas()
    ? alvo : anim_mola(scrollY, alvo, dt, NV_MOLA_SCROLL);
}

// "Salvo há 2 horas". A FRASE INTEIRA passa por i18n como FORMATO, nao montada
// de pedacos: "há" e "atrás" trocam de lugar na traducao e uma frase remendada
// aqui sairia "2 horas ago" em ingles.
static void quandoTexto(char *dst, size_t tam, long long quandoS) {
  long long agora = (long long)time(NULL);
  long long d = agora - quandoS;
  if (quandoS <= 0) { dst[0] = 0; return; }
  if (d < 0) d = 0;
  if (d < 90)            snprintf(dst, tam, "%s", i18n("Salvo agora"));
  else if (d < 5400)     snprintf(dst, tam, i18n("Salvo há %d min"), (int)(d / 60));
  else if (d < 172800)   snprintf(dst, tam, i18n("Salvo há %d h"),   (int)(d / 3600));
  else                   snprintf(dst, tam, i18n("Salvo há %d dias"),(int)(d / 86400));
}

// "Série · 2004 · ★ 8,1". As PARTES passam por i18n e a juncao nao: a chave da
// tabela e o portugues inteiro de uma string, e a frase montada nunca existiria
// como chave. E a mesma correcao que a biblioteca ja levou (issue #3).
static void metaTexto(char *dst, size_t tam, const SPLinha *l) {
  const char *tipo = i18n(l->serie ? "Série" : "Filme");
  if (l->meta && l->meta[0] && l->nota > 0)
    snprintf(dst, tam, "%s · %s · \xe2\x98\x85 %d,%d", tipo, l->meta,
             l->nota / 10, l->nota % 10);
  else if (l->meta && l->meta[0])
    snprintf(dst, tam, "%s · %s", tipo, l->meta);
  else if (l->nota > 0)
    snprintf(dst, tam, "%s · \xe2\x98\x85 %d,%d", tipo, l->nota / 10, l->nota % 10);
  else
    snprintf(dst, tam, "%s", tipo);
}

// `dx` e o deslocamento da animacao de entrada. Ele PRECISA chegar ate aqui: as
// linhas sao desenhadas em coordenada absoluta, e sem somar o mesmo `dx` do
// painel elas ficariam paradas no lugar final enquanto a moldura ainda desliza
// — o conteudo apareceria antes da caixa que o contem.
static void desenhaLinha(int i, float dx, float y, float a) {
  const SPLinha *l = &linhas[i];
  float f = animFoco[i];
  float px = SP_X + dx + SP_PAD, tx = SP_X + dx + SP_TEXTO_X;
  char buf[192];
  GfxRect poster = { px, y, SP_POSTER_W, SP_POSTER_H };

  if (f > 0.01f) {
    // ANEL BRANCO POR FORA, e nao pilula clara com texto escuro. E o foco da
    // referencia (o card focado carrega um contorno branco) e o mesmo que o
    // resto deste app usa fora dos menus — inverter aqui faria a camada parecer
    // de outro aplicativo.
    GfxRect r = { px - 12.0f, y - 10.0f, SP_INTERNO + 24.0f, SP_POSTER_H + 20.0f };
    gfx_cor(r, 0.06f, 0.16f, 0.165f, 0.19f, f * a);
    gfx_rect(r, 0, GFX_ANEL, 0, NV_ANEL_FOCO / r.w, 0, 0.06f,
             0.96f, 0.96f, 0.98f, f * a);
  }

  { GLuint tex = l->poster ? tex_obter(l->poster) : 0;
    if (tex) {
      gfx_tex_aspect_atual = tex_aspecto(l->poster);
      gfx_rect(poster, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, 0.08f, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
    } else {
      // Esqueleto VISIVEL (#2C2C2C), o mesmo da home e da biblioteca: um
      // retangulo da cor do fundo le como card quebrado, nao como carregando.
      gfx_cor(poster, 0.08f, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
              NV_COR_ESQUELETO_B, a);
    } }

  { TxtLinha t = txt_linha_corta(TXT_CALLOUT, l->titulo ? l->titulo : "",
                                 245, 246, 250, 255, SP_TEXTO_W);
    txt_desenhar_alpha(t, tx, y + 4.0f, a); }
  metaTexto(buf, sizeof buf, l);
  { TxtLinha t = txt_linha_corta(TXT_CAPTION2, buf, 168, 172, 182, 255, SP_TEXTO_W);
    txt_desenhar_alpha(t, tx, y + 42.0f, a * 0.95f); }

  if (l->progresso > 0) {
    float p = anim_clamp(l->progresso / 100.0f, 0.0f, 1.0f);
    GfxRect trilho = { tx, y + 84.0f, SP_BARRA_W, SP_BARRA_H };
    GfxRect cheio  = { tx, y + 84.0f, SP_BARRA_W * p, SP_BARRA_H };
    gfx_cor(trilho, 0.5f, 0.24f, 0.25f, 0.28f, a);
    if (cheio.w > 1.0f) gfx_cor(cheio, 0.5f, 0.93f, 0.94f, 0.97f, a);
    // "T1E3 · 29 min restantes" para serie; so o tempo para filme. Formatos
    // inteiros em i18n: a ordem de "T"/"E" e de "min restantes" nao sobrevive a
    // uma montagem por pedacos.
    if (l->temporada > 0 && l->episodio > 0 && l->restanteMin > 0)
      snprintf(buf, sizeof buf, i18n("T%dE%d · %d min restantes"),
               l->temporada, l->episodio, l->restanteMin);
    else if (l->temporada > 0 && l->episodio > 0)
      snprintf(buf, sizeof buf, i18n("T%dE%d · retomar"), l->temporada, l->episodio);
    else if (l->restanteMin > 0)
      snprintf(buf, sizeof buf, i18n("%d min restantes"), l->restanteMin);
    else
      snprintf(buf, sizeof buf, "%s", i18n("Retomar"));
    { TxtLinha t = txt_linha_corta(TXT_CAPTION, buf, 198, 202, 212, 255, SP_TEXTO_W);
      txt_desenhar_alpha(t, tx, y + 102.0f, a * 0.95f); }
  } else {
    quandoTexto(buf, sizeof buf, l->quandoS);
    if (buf[0]) {
      TxtLinha t = txt_linha_corta(TXT_CAPTION, buf, 150, 154, 165, 255, SP_TEXTO_W);
      txt_desenhar_alpha(t, tx, y + 92.0f, a * 0.9f);
    }
  }
}

static void desenhaVazio(float dx, float a) {
  float cx = SP_X + dx + SP_W * 0.5f;
  TxtLinha t1 = txt_linha(TXT_CALLOUT, "Nada salvo por enquanto", 240, 242, 248, 255);
  TxtLinha t2 = txt_linha_corta(TXT_CAPTION,
      "Aperte + em um filme ou série e ele aparece aqui.",
      168, 172, 182, 255, SP_INTERNO);
  gfx_icone((GfxRect){ cx - 30.0f, SP_LISTA_Y + 140.0f, 60.0f, 60.0f },
            "mais", 0.55f, 0.57f, 0.62f, a);
  txt_desenhar_alpha(t1, cx - t1.w * 0.5f, SP_LISTA_Y + 232.0f, a * 0.96f);
  txt_desenhar_alpha(t2, cx - t2.w * 0.5f, SP_LISTA_Y + 278.0f, a * 0.85f);
}

void spainel_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), x, y;
  int i;
  char buf[160];
  (void)agora;
  if (entrada < 0.002f) return;

  // O veu usa a rampa CRUA e o painel a suavizada, pelo mesmo motivo do menu
  // lateral: a medida da referencia para o escurecimento e uma reta, e um bloco
  // deste tamanho parando de vez no fim do percurso le como corte.
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, SP_VEU * entrada);

  // Entra deslizando da BORDA DIREITA. `x` e o deslocamento: em a=0 o painel
  // esta inteiro fora da tela.
  x = (1.0f - a) * (NV_TELA_W - SP_X);
  { GfxRect p = { SP_X + x, SP_Y, SP_W, SP_H };
    gfx_cor(p, 0.035f, 0.075f, 0.078f, 0.088f, 0.98f * a); }

  // Tudo daqui para baixo fica preso ao painel: sem o recorte, a lista rolada
  // desenha por cima do cabecalho e por baixo da borda inferior.
  gfx_recorte(SP_X + x, SP_Y, SP_W, SP_H);

  // Cabecalho: a linha de resumo em cima e o nome grande embaixo, como na
  // referencia. As PARTES passam por i18n; a juncao, nao (ver metaTexto).
  snprintf(buf, sizeof buf, "%d %s   ·   %d %s", nLinhas,
           i18n(nLinhas == 1 ? "título" : "títulos"),
           nCont, i18n("para retomar"));
  { TxtLinha t = txt_linha(TXT_CAPTION2, buf, 160, 164, 175, 255);
    txt_desenhar_alpha(t, SP_X + x + SP_PAD, SP_Y + 38.0f, a * 0.95f); }
  { TxtLinha t = txt_linha(TXT_TITULO2, "Salvos", 246, 247, 252, 255);
    txt_desenhar_alpha(t, SP_X + x + SP_PAD, SP_Y + 74.0f, a); }

  if (nLinhas == 0) { desenhaVazio(x, a); gfx_sem_recorte(); return; }

  // A lista rola dentro da propria janela, com um segundo recorte: o cabecalho
  // fica de fora dele e por isso nunca e coberto por um card subindo.
  gfx_recorte(SP_X + x, SP_LISTA_Y, SP_W, SP_LISTA_BASE - SP_LISTA_Y);
  y = SP_LISTA_Y - scrollY;

  { TxtLinha t = txt_linha(TXT_CAPTION2,
        nCont > 0 ? "Continuar" : "Sua lista", 150, 154, 165, 255);
    txt_desenhar_alpha(t, SP_X + x + SP_PAD, y + SP_SECAO_H - t.h - 12.0f, a * 0.9f); }
  y += SP_SECAO_H;

  for (i = 0; i < nLinhas; i++) {
    if (i == nCont && nCont > 0) {
      TxtLinha t = txt_linha(TXT_CAPTION2, "Não começados", 150, 154, 165, 255);
      txt_desenhar_alpha(t, SP_X + x + SP_PAD, y + SP_SECAO_H - t.h - 12.0f, a * 0.9f);
      y += SP_SECAO_H;
    }
    // Fora da janela nao custa texto nem textura: numa lista de 200 titulos
    // rasterizar as 195 invisiveis estouraria o orcamento de linhas por quadro
    // de text.c e as visiveis sairiam EM BRANCO (ver a nota em ctxmenu.c).
    if (y + SP_POSTER_H >= SP_LISTA_Y && y <= SP_LISTA_BASE)
      desenhaLinha(i, x, y, a);
    y += SP_PASSO;
  }

  gfx_sem_recorte();
}
