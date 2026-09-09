// Explicador de primeira vez do "Salvos". Ver salvosintro.h para o porque.
//
// A PERGUNTA DO MEIO DA TELA — "Onde o + deve salvar?" — e a parte que merece
// leitura antes de mexer, porque o texto dela foi ESCRITO CONTRA a maquete.
//
// A maquete (vinda do app irmao em tvOS) descreve a opcao "Lista do Nuvio"
// como "Fica na sua conta e em todo aparelho onde você entrar". Neste app isso
// SERIA MENTIRA. A conta so tem RPC de LEITURA da biblioteca: sync.c chama
// `sync_pull_library` e o comentario dele diz, com todas as letras, que
// `sync_push_library` nao existe e nao pode ganhar de graca — um push da lista
// local antes do primeiro pull mandaria lista curta e APAGARIA itens nos outros
// aparelhos da pessoa (PLANO-CONTA-SYNC.md, secao 1.6, regra 2).
//
// Entao a opcao existe, funciona e e util — ela e a lista local de salvos.c,
// que e o que conserta "sem Trakt o + nao guarda nada" —, mas o texto na tela
// promete exatamente o que ela entrega: fica nesta TV, e o que a conta mandar
// do celular continua chegando (isso sim existe, e o pull). Prometer sincronia
// de ida seria a mesma familia de defeito do selo "NUVIO" cravado na Biblioteca
// e da classificacao "14" inventada: informacao com cara de dado.
#include "salvosintro.h"
#include "salvos.h"
#include "catalogo.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SI_ARQ "salvos-intro.txt"

// Mesma pegada do painel de Salvos, de proposito: o cartao que explica a
// novidade ocupa o lugar exato onde a novidade vai aparecer. Quem fechar o
// explicador e apertar a AZUL ve a camada nascer na mesma moldura.
#define SI_X        1120.0f
#define SI_W         776.0f
#define SI_Y          24.0f
#define SI_H        1032.0f
#define SI_PAD        44.0f
#define SI_INT      (SI_W - SI_PAD * 2.0f)
// 112 e nao 104: com 104 a descricao de DUAS linhas terminava exatamente na
// borda inferior da pilula (titulo em y+14, descricao em y+52, duas linhas de
// 26 = y+104). Na captura da TV os descendentes da segunda linha encostavam no
// limite. A opcao de uma linha nao mostrava o problema — e por isso ele passou.
#define SI_OPCAO_H   112.0f
#define SI_MINI_W    100.0f
#define SI_MINI_H    150.0f
#define SI_N_MINI      6
#define SI_ABRIR_MS  260.0f
#define SI_FECHAR_MS 150.0f

static int aberto, decidido, foco;
static float entrada, animFoco[2];
// Cartazes de quem ja esta salvo, resolvidos UMA vez na abertura. Resolver por
// quadro voltaria ao catalogo 60 vezes por segundo para desenhar seis imagens
// que nao mudam enquanto o cartao esta em pe.
//
// A URL E COPIADA, e nao apontada para dentro do CatItem. Ver a nota longa em
// salvospainel.c: o vetor de itens do catalogo troca de bloco a cada
// republicacao e o bloco antigo e liberado. Guardar o ponteiro aqui derrubou o
// app na TV poucos segundos depois do arranque — e so na TV, porque no Mac sem
// conta o catalogo nunca era republicado.
static char minis[SI_N_MINI][512];
static int nMinis;

int sintro_aberto(void) { return aberto; }

// Ate seis cartazes do que a pessoa ja tem salvo. A ordem e a mesma do painel:
// primeiro a lista local, depois o que o catalogo marcou como naLista. Com
// nenhum, o bloco inteiro some do cartao (ver o desenho) — uma fileira de seis
// retangulos cinza dizendo "Já na sua lista" seria pior que nao ter a fileira.
static void juntarMinis(void) {
  int i, n;
  nMinis = 0;
  n = salvos_n();
  for (i = 0; i < n && nMinis < SI_N_MINI; i++) {
    const SalvoItem *s = salvos_item(i);
    if (s && s->poster[0])
      snprintf(minis[nMinis++], sizeof minis[0], "%s", s->poster);
  }
  n = cat_n();
  for (i = 0; i < n && nMinis < SI_N_MINI; i++) {
    const CatItem *c = cat_item(i);
    if (!c || !c->naLista || !c->poster[0]) continue;
    if (c->imdb[0] && salvos_tem(c->imdb)) continue;   // ja entrou acima
    snprintf(minis[nMinis++], sizeof minis[0], "%s", c->poster);
  }
}

// Grava a marca. Sem pasta gravavel (dados_dir() vazio) isto e no-op e o cartao
// volta no proximo arranque; e honesto, e o log de dados.c ja disse por que nao
// ha pasta. Mesma escolha de registro.c.
static void marcarVisto(void) { dados_gravar(SI_ARQ, "1\n"); }

void sintro_primeira_vez(void) {
  char *s;
  if (decidido) return;
  decidido = 1;
  s = dados_ler(SI_ARQ);
  if (s) { free(s); return; }
  aberto = 1;
  // O foco NASCE na opcao em vigor, e nao sempre na primeira: o cartao e um
  // mapa de onde a pessoa esta, e comecar em outra linha faria ela ler que a
  // escolha ja mudou. Mesma regra de menu_abrir.
  foco = ajustes_salvos_no_trakt() ? 1 : 0;
  memset(animFoco, 0, sizeof animFoco);
  juntarMinis();
}

void sintro_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_DOWN) { if (foco < 1) foco++; return; }
  if (k == SDLK_UP)   { if (foco > 0) foco--; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    ajustes_definir_salvos_no_trakt(foco == 1);
    aberto = 0;
    marcarVisto();
    return;
  }
  // VOLTAR TAMBEM ENCERRA, e tambem grava a marca. Um cartao de boas-vindas que
  // reaparece a cada arranque ate ser respondido deixa de ser explicacao e vira
  // obstaculo; quem dispensou fica com o ajuste que ja estava valendo, e a
  // linha em Ajustes › Interface e conta continua la para mudar depois.
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    aberto = 0;
    marcarVisto();
  }
}

void sintro_atualizar(float dt, Uint32 agora) {
  int i;
  (void)agora;
  if (!aberto && entrada < 0.002f) { entrada = 0.0f; return; }
  entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt,
                       aberto ? SI_ABRIR_MS : SI_FECHAR_MS);
  for (i = 0; i < 2; i++) {
    float a = (aberto && i == foco) ? 1.0f : 0.0f;
    animFoco[i] = ajustes_animacoes_reduzidas()
      ? a
      : anim_mola(animFoco[i], a, dt,
                  a > animFoco[i] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
}

// A ILUSTRACAO. Nao ha renderizador de SVG neste app (ver gfx.h), entao "figura"
// quer dizer primitivas compostas: tres cartazes em leque, cada um um pouco
// mais claro que o de tras, e o disco branco do "+" encostado no da frente.
//
// Tres retangulos e um disco custam quatro desenhos de area pequena. A conta
// importa: gfx.c registra que DUAS camadas de tela cheia ja derrubavam esta
// Mali-G71 para ~40 fps, e este cartao ja paga um veu de tela cheia.
static void desenhaFigura(float cx, float y, float a) {
  static const float lum[3] = { 0.15f, 0.20f, 0.27f };
  int i;
  // O leque vai de cx-135 a cx+105 e o disco de cx+67 a cx+135: o conjunto fica
  // simetrico em cx, e o disco ENCOSTA no cartao da frente. Na primeira versao
  // ele comecava em cx+96, depois da borda do ultimo cartao — na captura da TV
  // o "+" aparecia solto no ar, sem relacao visivel com a pilha, e o conjunto
  // inteiro puxava para a direita.
  for (i = 0; i < 3; i++) {
    float w = 92.0f, h = 132.0f + (float)i * 7.0f;
    GfxRect c = { cx - 135.0f + (float)i * 74.0f, y + (float)(2 - i) * 7.0f, w, h };
    gfx_cor(c, 0.09f, lum[i], lum[i] + 0.004f, lum[i] + 0.014f, a);
  }
  { GfxRect d = { cx + 67.0f, y + 90.0f, 68.0f, 68.0f };
    gfx_cor(d, 0.5f, 0.95f, 0.96f, 0.98f, a);
    gfx_icone((GfxRect){ d.x + 19.0f, d.y + 19.0f, 30.0f, 30.0f },
              "mais", 0.07f, 0.07f, 0.09f, a); }
}

// Uma das duas opcoes de destino. O escolhido em vigor fica marcado MESMO SEM
// FOCO: sao dois estados diferentes e os dois precisam existir, senao mover o
// foco apaga a indicacao de qual esta valendo — o mesmo erro que as abas de
// temporada do detalhe ja cometeram.
static void desenhaOpcao(float x, float y, float f, int vigor,
                         const char *tit, const char *desc, float a) {
  GfxRect r = { x, y, SI_INT, SI_OPCAO_H };
  float lum = anim_mistura(vigor ? 0.155f : 0.115f, 0.20f, f);
  gfx_cor(r, 0.16f, lum, lum + 0.004f, lum + 0.016f, a);
  if (f > 0.01f)
    gfx_rect(r, 0, GFX_ANEL, 0, NV_ANEL_FOCO / r.w, 0, 0.16f,
             0.96f, 0.96f, 0.98f, f * a);
  else if (vigor)
    gfx_rect(r, 0, GFX_ANEL, 0, 1.5f / r.w, 0, 0.16f, 0.55f, 0.56f, 0.60f, a);
  { TxtLinha t = txt_linha_corta(TXT_CALLOUT, tit, 245, 246, 250, 255, SI_INT - 44.0f);
    txt_desenhar_alpha(t, x + 22.0f, y + 14.0f, a); }
  txt_bloco(TXT_CAPTION, desc, 166, 170, 180, x + 22.0f, y + 50.0f,
            SI_INT - 44.0f, 25.0f, a * 0.95f, 2);
}

void sintro_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dx, x, y;
  int i;
  (void)agora;
  if (entrada < 0.002f) return;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.66f * entrada);

  dx = (1.0f - a) * (NV_TELA_W - SI_X);
  x = SI_X + dx + SI_PAD;
  { GfxRect p = { SI_X + dx, SI_Y, SI_W, SI_H };
    gfx_cor(p, 0.035f, 0.075f, 0.078f, 0.088f, 0.98f * a); }
  gfx_recorte(SI_X + dx, SI_Y, SI_W, SI_H);

  // Na maquete o lugar deste rotulo e o nome do titulo aberto atras do cartao.
  // Aqui o explicador nasce na HOME, nao sobre um titulo — e nao ha nome para
  // pedir emprestado. O rotulo diz entao o que ele e de verdade.
  { TxtLinha t = txt_linha(TXT_CAPTION2, "NOVO NO NUVIO", 150, 154, 165, 255);
    txt_desenhar_alpha(t, x, SI_Y + 38.0f, a * 0.92f); }
  { TxtLinha t = txt_linha(TXT_TITULO2, "Salvando com +", 246, 247, 252, 255);
    txt_desenhar_alpha(t, x, SI_Y + 70.0f, a); }

  y = SI_Y + 152.0f;
  desenhaFigura(SI_X + dx + SI_W * 0.5f, y, a);
  y += 176.0f;

  y += txt_bloco(TXT_CAPTION,
        "+ põe um título na sua lista para você achar de novo sem procurar. "
        "Ele não marca nada como assistido.",
        196, 200, 210, x, y, SI_INT, 30.0f, a * 0.95f, 3);
  y += 30.0f;

  // QUAL TECLA REABRE A LISTA — e ela NAO E A MESMA nos dois aparelhos.
  //
  // Sem esta linha o explicador ensinava a guardar e nao a encontrar: a pessoa
  // salvava e ficava sem saber como ver o que salvou. Foi o dono quem notou.
  //
  // Na LG e a AZUL, que e um botao de verdade no controle. No Tizen nao pode
  // ser: os One Remote novos nao tem fileira de cores e chegar na azul exige
  // abrir a barra de cores pelo botao de numeros. La e CANAL +, que e fisico em
  // todo controle Samsung (ver tools/tizen-shell.html).
  //
  // Escolha em tempo de COMPILACAO porque cada build serve um alvo so — decidir
  // em execucao exigiria perguntar ao aparelho algo que ele nao sabe responder.
  y += txt_bloco(TXT_CAPTION,
#ifdef __EMSCRIPTEN__
        "Depois, o botão CANAL + do controle abre sua lista a qualquer momento.",
#else
        "Depois, o botão AZUL do controle abre sua lista a qualquer momento.",
#endif
        196, 200, 210, x, y, SI_INT, 30.0f, a * 0.95f, 2);
  y += 30.0f;

  // O bloco de cartazes SO EXISTE se houver cartaz. Ver juntarMinis.
  if (nMinis > 0) {
    float px = x, passo = (SI_INT - SI_MINI_W) / (float)(SI_N_MINI - 1);
    { TxtLinha t = txt_linha(TXT_CAPTION2, "Já na sua lista", 150, 154, 165, 255);
      txt_desenhar_alpha(t, x, y, a * 0.9f); }
    y += 34.0f;
    for (i = 0; i < nMinis; i++) {
      GfxRect r = { px, y, SI_MINI_W, SI_MINI_H };
      GLuint tex = tex_obter(minis[i]);
      if (tex) {
        gfx_tex_aspect_atual = tex_aspecto(minis[i]);
        gfx_rect(r, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, 0.08f, 0, 0, 0, a);
        gfx_tex_aspect_atual = 0.0f;
      } else {
        gfx_cor(r, 0.08f, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
                NV_COR_ESQUELETO_B, a);
      }
      px += passo;
    }
    y += SI_MINI_H + 34.0f;
  }

  { TxtLinha t = txt_linha(TXT_CAPTION2, "Onde o + deve salvar?", 150, 154, 165, 255);
    txt_desenhar_alpha(t, x, y, a * 0.9f); }
  y += 36.0f;

  desenhaOpcao(x, y, animFoco[0], !ajustes_salvos_no_trakt(),
               "Lista do Nuvio",
               "Fica guardada nesta TV. O que você salvar no celular continua "
               "chegando aqui pela sua conta.", a);
  y += SI_OPCAO_H + 12.0f;
  desenhaOpcao(x, y, animFoco[1], ajustes_salvos_no_trakt(),
               "Watchlist do Trakt",
               "Aparece também nos apps e no site que leem essa conta.", a);
  y += SI_OPCAO_H + 26.0f;

  { TxtLinha t = txt_linha_corta(TXT_CAPTION,
        "Você pode mudar isso depois em Ajustes › Interface e conta.",
        144, 148, 158, 255, SI_INT);
    txt_desenhar_alpha(t, x, y, a * 0.85f); }
  { TxtLinha t = txt_linha(TXT_CAPTION2, "↑ ↓ Escolher   ·   OK Confirmar",
                           132, 136, 146, 255);
    txt_desenhar_alpha(t, x, SI_Y + SI_H - 52.0f, a * 0.8f); }

  gfx_sem_recorte();
}
