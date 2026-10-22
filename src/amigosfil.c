// A fileira "Amigos assistindo" da home (E2 com E1 dentro). Ver amigosfil.h.
#include "amigosfil.h"
#include "socialvis.h"
#include "svdesenho.h"
#include "catalogo.h"
#include "recomenda.h"
#include "recenviar.h"
#include "celbotao.h"
#include "ajustes.h"
#include "anim.h"
#include "gfx.h"
#include "text.h"
#include "layout.h"
#include "idioma.h"
#include "rolagemtoque.h"
#include <stdio.h>
#include <string.h>

// MEDIDAS, em px de layout (1920x1080).
//   AF_D    o rosto: 8 % da largura da tela, como no desenho aprovado.
//   AF_P    passo entre rostos: o rosto e 50 px para o nome respirar e os
//           aneis (estado + foco, ~26 px por fora) nao encostarem no vizinho.
//   AF_VAO  do rosto ate o painel que abre ao lado.
#define AF_D        154.0f
#define AF_P        204.0f
#define AF_VAO       36.0f
#define AF_PAD       14.0f
#define AF_GAP_CART  16.0f
#define AF_RAIO_PAINEL 22.0f
// A mola da ilha (ilha.c, ILHA_MOLA_*): subamortecida, com o "pulo".
#define AF_MOLA_W    10.0f
#define AF_MOLA_Z    0.72f
// Quanto tempo o painel aberto apaga o anel laranja de novidade.
#define AF_VISTO_MS 1500u

static float abre[SV_AMIGOS_MAX + 1], vAbre[SV_AMIGOS_MAX + 1];
static float fRosto[SV_AMIGOS_MAX + 1];
static float fCartao[SV_TIT_MAX];
static float scroll, vScroll;
static int dentro = -1;          // -1 = no rosto; 0..2 = cartao em foco
static int colAnt = -1;
static char focoId[96];
static unsigned revFoco;
static float ultX0 = -1.0f, ultAlt = 240.0f;
static Uint32 abertoDesde;
static char pedTitulo[24], pedPerfil[96];
static int temTitulo, temPerfil, temAjustes;
// Cache do indice do catalogo do item em foco (a home pergunta varias vezes
// por quadro e cat_indice_por_imdb varre o catalogo).
static unsigned cacheRev = ~0u, cacheCat = ~0u;
static int cacheCol = -2, cacheDentro = -2, cacheIdx = -1;
#ifdef NV_TOUCH_UI
static ToqueRolagem toque;
static PonteiroFn toqueFocar;
void amigosfil_ponteiro(PonteiroFn focar) { toqueFocar = focar; }
int amigosfil_rolagem(const PonteiroRolagem *e) {
  int r = toquerol_evento(&toque, e);
  if (r && e->fase == PONT_ROL_INICIO) vScroll = 0.0f;
  return r;
}
void amigosfil_focar(int coluna, int cartao) {
  toquerol_limpar(&toque);
  colAnt = coluna; dentro = cartao;
}
#endif

int amigosfil_dentro(void) { return dentro; }
int amigosfil_convite(void) { return socialvis_n_amigos() == 0; }
int amigosfil_n_colunas(void) {
  int n = socialvis_n_amigos();
  return n > 0 ? n + 1 : 1;
}

static float molaIlha(float *v, float x, float alvo, float dt) {
  int k;
  if (anim_politica_reduzida || ajustes_animacoes_reduzidas()) { *v = 0.0f; return alvo; }
  if (dt > 0.05f) dt = 0.05f;
  for (k = 0; k < 4; k++) {
    float h = dt * 0.25f, ac = AF_MOLA_W * AF_MOLA_W * (alvo - x) - 2.0f * AF_MOLA_Z * AF_MOLA_W * (*v);
    *v += ac * h;
    x += *v * h;
  }
  return x;
}

static float alturaCartao(float alt) { return alt - 4.0f - 2.0f * AF_PAD; }
static float larguraCartao(float alt) { return alturaCartao(alt) * 16.0f / 9.0f; }
static float larguraPainel(int nTit, float alt) {
  if (nTit < 1) return 0.0f;
  return 2.0f * AF_PAD + (float)nTit * larguraCartao(alt) + (float)(nTit - 1) * AF_GAP_CART;
}

// Quanto o painel do rosto `i` empurra os de depois (a abertura ja medida).
static float empurra(int i, float alt) {
  const SvAmigo *a = socialvis_amigo(i);
  float t = abre[i] < 0.0f ? 0.0f : abre[i];
  if (!a || a->nTit < 1) return 0.0f;
  return (larguraPainel(a->nTit, alt) + AF_VAO) * t;
}

static float rostoX(int i, float x0, float alt) {
  float x = x0 - scroll + (float)i * AF_P;
  int j;
  for (j = 0; j < i && j < SV_AMIGOS_MAX; j++) x += empurra(j, alt);
  return x;
}
static float amigosfilDireita(void) {
#ifdef NV_TOUCH_UI
  if (NV_TELA_H > NV_TELA_W) {
    float margem = ajustes_rail_largura_fixa() > 0.0f ? fmaxf(48.0f, ajustes_conteudo_x()) : 48.0f;
    return NV_TELA_W - margem;
  }
#endif
  return NV_TELA_W - NV_HOME_SAFE_RIGHT;
}
#ifdef NV_TOUCH_UI
static void amigosfilToqueVincular(float x0, float y, float alt, float corte, int n) {
  float direita = amigosfilDireita();
  float maxX = fmaxf(0.0f, rostoX(n, x0, alt) + scroll + AF_D + 30.0f - direita);
  float topo = fmaxf(y, corte), base = fminf(y + alt, NV_TELA_H);
  toquerol_vincular(&toque, (GfxRect){x0, topo, fmaxf(0.0f, direita - x0), fmaxf(0.0f, base - topo)},
                   gfx_escala(), 0.0f, n > 0 ? maxX : 0.0f, 0, &scroll);
}
void amigosfil_retomar_foco(int *coluna) {
  if (toque.livre && coluna) {
    float ponto = toque.regiao.x + toque.regiao.w * 0.35f, melhor = 1e9f;
    int n = amigosfil_n_colunas();
    for (int i = 0; i < n; i++) {
      float d = fabsf(rostoX(i, ultX0, ultAlt) + AF_D * 0.5f - ponto);
      if (d < melhor) { melhor = d; *coluna = i; }
    }
    dentro = -1;
  }
  toquerol_limpar(&toque);
}
static void alvoToque(GfxRect r, float corte, int coluna, int cartao) {
  float x = fmaxf(r.x, ultX0), dir = fminf(r.x + r.w, toque.regiao.x + toque.regiao.w);
  if (toqueFocar && dir > x)
    ponteiro_alvo_faixa(x, r.y, dir - x, r.h, corte, NV_TELA_H, toqueFocar, NULL, coluna, cartao);
}
#endif

int amigosfil_indice_cat(int coluna) {
  const SvAmigo *a = socialvis_amigo(coluna);
  const SvEvento *e;
  int d;
  if (!a || a->nTit < 1) return -1;
  d = (coluna == colAnt && dentro >= 0 && dentro < a->nTit) ? dentro : 0;
  if (cacheRev == socialvis_revisao() && cacheCat == cat_revisao() &&
      cacheCol == coluna && cacheDentro == d) return cacheIdx;
  e = &a->tit[d];
  cacheRev = socialvis_revisao(); cacheCat = cat_revisao();
  cacheCol = coluna; cacheDentro = d;
  cacheIdx = cat_indice_por_imdb(e->imdb);
  return cacheIdx;
}

int amigosfil_tecla(SDL_Keycode k, int *coluna) {
  int n = socialvis_n_amigos();
  const SvAmigo *a;
  if (!coluna || n < 1) return 0;          // convite: a home cuida (borda/menu)
#ifdef NV_TOUCH_UI
  if (k == SDLK_UP || k == SDLK_DOWN || k == SDLK_LEFT || k == SDLK_RIGHT)
    amigosfil_retomar_foco(coluna);
#endif
  a = socialvis_amigo(*coluna);
  if (k == SDLK_RIGHT) {
    if (!a) return 0;                      // "+ Adicionar": a home bate na borda
    if (dentro + 1 < a->nTit) { dentro++; return 1; }
    // Do ultimo cartao (ou de um rosto sem titulos) para o proximo rosto.
    if (*coluna + 1 < amigosfil_n_colunas()) { (*coluna)++; dentro = -1; return 1; }
    return 0;
  }
  if (k == SDLK_LEFT) {
    if (dentro >= 0) { dentro = -1; return 1; }
    return 0;                              // rosto: a home anda (ou abre o menu)
  }
  if (k == SDLK_UP || k == SDLK_DOWN) dentro = -1;
  return 0;
}

void amigosfil_ok(int coluna) {
  const SvAmigo *a;
  if (amigosfil_convite()) {
    // O QR DO CELULAR quando o pacote tem o servidor dele; senao a tela de
    // amigos com o codigo; sem o servico de recomendacoes, Ajustes.
    if (recomenda_ativo() && celb_disponivel()) { celb_abrir(CELB_AMIGO, "Código do amigo"); return; }
    if (recomenda_ativo() && recenviar_abrir_amigos()) return;
    temAjustes = 1;
    return;
  }
  a = socialvis_amigo(coluna);
  if (!a) {
    if (!recomenda_ativo() || !recenviar_abrir_amigos()) temAjustes = 1;
    return;
  }
  if (dentro >= 0 && dentro < a->nTit) {
    snprintf(pedTitulo, sizeof pedTitulo, "%s", a->tit[dentro].imdb);
    temTitulo = 1;
    return;
  }
  snprintf(pedPerfil, sizeof pedPerfil, "%s", a->id);
  temPerfil = 1;
  socialvis_marcar_visto(a->id);
}

int amigosfil_pediu_titulo(char *imdb, size_t tam) {
  if (!temTitulo) return 0;
  temTitulo = 0;
  if (imdb && tam) snprintf(imdb, tam, "%s", pedTitulo);
  return 1;
}
int amigosfil_pediu_perfil(char *id, size_t tam) {
  if (!temPerfil) return 0;
  temPerfil = 0;
  if (id && tam) snprintf(id, tam, "%s", pedPerfil);
  return 1;
}
int amigosfil_pediu_ajustes(void) { int v = temAjustes; temAjustes = 0; return v; }

static float alvoScroll(int coluna, float x0, float alt) {
  float alvo, util = amigosfilDireita();
  const SvAmigo *a = socialvis_amigo(coluna);
  int j;
  // Um rosto inteiro antes do focado fica a vista, e o painel aberto inteiro
  // cabe ate a margem direita.
  alvo = coluna >= 1 ? (float)(coluna - 1) * AF_P : 0.0f;
  for (j = 0; j < coluna - 1 && j < SV_AMIGOS_MAX; j++) alvo += empurra(j, alt);
  { float fim = x0 + (coluna >= 1 ? AF_P : 0.0f) + AF_D + 30.0f;
    if (a && a->nTit > 0) fim += AF_VAO + larguraPainel(a->nTit, alt);
    if (fim > util) alvo += fim - util; }
  return alvo < 0.0f ? 0.0f : alvo;
}

void amigosfil_atualizar(float dt, int focada, int *colunaP) {
  int n, i, red = ajustes_animacoes_reduzidas(), coluna;
  const SvAmigo *a;
  socialvis_atualizar();
  n = socialvis_n_amigos();
  coluna = colunaP ? *colunaP : 0;
  // O MODELO MUDOU (chegou atividade): o foco segue a PESSOA, e nao o indice —
  // senao o dedo parado num rosto passaria a apontar para outro amigo.
  if (revFoco != socialvis_revisao()) {
    revFoco = socialvis_revisao();
    if (focoId[0] && colunaP) {
      int k = socialvis_amigo_indice(focoId);
      if (k >= 0 && k != coluna) { coluna = *colunaP = k; colAnt = k; }
    }
  }
  if (coluna > n) coluna = n;
  // O codigo que o celular mandou vira pedido de vinculo.
  { char t[64];
    if (celb_pegar(CELB_AMIGO, t, sizeof t)) recomenda_vincular(t); }
  if (!focada) dentro = -1;
  // A COLUNA MUDOU POR FORA (a home andou): o foco volta ao rosto.
  if (coluna != colAnt) { dentro = -1; colAnt = coluna; abertoDesde = 0; }
  a = socialvis_amigo(coluna);
  if (a && strcmp(a->id, focoId)) { snprintf(focoId, sizeof focoId, "%s", a->id); abertoDesde = 0; }
  if (a && dentro >= a->nTit) dentro = a->nTit - 1;
  for (i = 0; i <= n && i <= SV_AMIGOS_MAX; i++) {
    float alvo = (focada && i == coluna) ? 1.0f : 0.0f;
    const SvAmigo *ai = socialvis_amigo(i);
    float alvoAbre = (alvo > 0.0f && ai && ai->nTit > 0) ? 1.0f : 0.0f;
    fRosto[i] = red ? alvo : anim_mola(fRosto[i], (alvo > 0.0f && dentro < 0) ? 1.0f : 0.0f, dt,
                                       NV_MOLA_FOCO);
    abre[i] = molaIlha(&vAbre[i], abre[i], alvoAbre, dt);
  }
  for (i = 0; i < SV_TIT_MAX; i++) {
    float alvo = (focada && i == dentro) ? 1.0f : 0.0f;
    fCartao[i] = red ? alvo : anim_mola(fCartao[i], alvo, dt, NV_MOLA_FOCO);
  }
  if (ultX0 >= 0.0f) {
    float alvo = n > 0 && focada ? alvoScroll(coluna, ultX0, ultAlt) : (n > 0 ? scroll : 0.0f);
#ifdef NV_TOUCH_UI
    if (!toque.livre)
#endif
      scroll = anim_mola2_reduzida(&vScroll, scroll, alvo, dt, NV_MOLA2_SCROLL, red);
  }
  // A NOVIDADE FOI VISTA quando o painel ficou aberto um instante — e nao no
  // primeiro quadro: passar pela fileira com o dedo na seta nao e olhar.
  if (focada && a && a->novo && abre[coluna] > 0.9f) {
    if (!abertoDesde) abertoDesde = SDL_GetTicks() ? SDL_GetTicks() : 1;
    else if (SDL_GetTicks() - abertoDesde >= AF_VISTO_MS) socialvis_marcar_visto(a->id);
  }
}

static void desenhaConvite(float x0, float y, float alt, int focada, Uint32 agora) {
  float w = amigosfilDireita() - x0, h = alt - 24.0f;
  float f = focada ? 1.0f : 0.0f, raio = 26.0f / h;
  GfxRect r;
  const char *cod = recomenda_meu_codigo();
  int cel = recomenda_ativo() && celb_disponivel();
  float tx, tw, bd = 92.0f;
  (void)agora;
  if (w > 1180.0f) w = 1180.0f;
  r = (GfxRect){ x0, y + 12.0f, w, h };
  if (f > 0.01f) {
    float ar, ag, ab;
    GfxRect b = { r.x - NV_ANEL_FOCO, r.y - NV_ANEL_FOCO, r.w + 2 * NV_ANEL_FOCO, r.h + 2 * NV_ANEL_FOCO };
    ajustes_acento(&ar, &ag, &ab);
    if (ajustes_vidro()) gfx_vidro_cartao(r, raio, f, 1.0f);
    else gfx_cor(b, (26.0f + NV_ANEL_FOCO) / b.h, ar, ag, ab, f);
  }
  if (ajustes_vidro()) gfx_vidro_folha(r, raio, 1.0f);
  else gfx_cor(r, raio, 0.155f, 0.15f, 0.185f, 0.96f);
  // Tres rostos vazios a esquerda: o lugar deles e aqui, e sem a fileira de
  // rostos o convite nao diria o que vai aparecer.
  { int k;
    for (k = 0; k < 3; k++) {
      GfxRect c = { r.x + 34.0f + (float)k * 58.0f, r.y + (r.h - 96.0f) * 0.5f, 96.0f, 96.0f };
      gfx_anel_fora(c, 0.5f, 0.0f, 4.0f, 0.155f, 0.15f, 0.185f, 1.0f);
      gfx_rect(c, 0, GFX_DISCO, 0, 0, 0, 0, 0.23f + 0.04f * (float)k, 0.225f + 0.04f * (float)k,
               0.27f + 0.04f * (float)k, 1.0f);
    }
    gfx_icone((GfxRect){ r.x + 34.0f + 2 * 58.0f + 30.0f, r.y + (r.h - 36.0f) * 0.5f, 36.0f, 36.0f },
              "mais", 0.85f, 0.84f, 0.88f, 1.0f); }
  tx = r.x + 34.0f + 2.0f * 58.0f + 96.0f + 40.0f;
  tw = r.x + r.w - tx - (cel ? bd + 64.0f : 40.0f);
  { TxtLinha t1 = txt_linha_corta(TXT_TITULO3, "Adicione amigos", 246, 246, 250, 255, tw);
    float yy = r.y + 30.0f;
    txt_desenhar(t1, tx, yy);
    yy += (float)t1.h + 8.0f;
    yy += txt_bloco(TXT_CAPTION, "Veja o que eles estão assistindo e troquem recomendações.",
                    200, 198, 210, tx, yy, tw, 30.0f, 1.0f, 2) + 10.0f;
    { float yOk = r.y + r.h - 30.0f - 30.0f;
    { char linha[160];
      if (cel) snprintf(linha, sizeof linha, "%s", i18n("OK · digite o código de um amigo pelo celular"));
      else snprintf(linha, sizeof linha, "%s", i18n("OK · adicionar um amigo"));
      { TxtLinha t = txt_linha_corta(TXT_CAPTION, linha, 247, 192, 138, 255, tw);
        txt_desenhar(t, tx, yOk); } }
    if (cod[0]) {
      char c2[64];
      snprintf(c2, sizeof c2, i18n("Seu código: %s"), cod);
      { TxtLinha t = txt_linha_corta(TXT_CAPTION, c2, 168, 166, 178, 255, tw);
        if (yy + (float)t.h < yOk - 4.0f) txt_desenhar(t, tx, yy); }
    } } }
  if (cel)
    celb_botao(CELB_AMIGO, (GfxRect){ r.x + r.w - 48.0f - bd, r.y + (r.h - bd) * 0.5f, bd, bd },
               focada, NULL, 0, 0, 1.0f);
}

void amigosfil_desenhar(float x0, float y, float alt, float corte, int focada,
                        int coluna, Uint32 agora) {
  int n = socialvis_n_amigos(), i;
  float fy = y + 24.0f;
  // A rolagem anda em _atualizar, com a margem e a altura do ultimo desenho
  // (a rail fixa muda x0).
  if (ultX0 < 0.0f && focada) scroll = alvoScroll(coluna, x0, alt);
  ultX0 = x0; ultAlt = alt;
#ifdef NV_TOUCH_UI
  amigosfilToqueVincular(x0, y, alt, corte, n);
#endif
  if (n < 1) {
#ifdef NV_TOUCH_UI
    alvoToque((GfxRect){ x0, y, toque.regiao.w, alt }, corte, 0, -1);
#endif
    desenhaConvite(x0, y, alt, focada, agora); return;
  }

  for (i = 0; i <= n && i <= SV_AMIGOS_MAX; i++) {
    float rx = rostoX(i, x0, alt);
    const SvAmigo *a = socialvis_amigo(i);
    float fr = fRosto[i];
    GfxRect face = { rx, fy, AF_D, AF_D };
    if (rx > NV_TELA_W + 40.0f) break;
    // O PAINEL QUE ABRE AO LADO, antes do rosto seguinte. Recortado pela
    // abertura: o conteudo fica no lugar final e aparece por dentro, sem
    // escala nem textura nova (a mesma ideia do morph do painel, spRecorte).
    if (a && a->nTit > 0 && abre[i] > 0.01f) {
      float pw = larguraPainel(a->nTit, alt), t = abre[i];
      float px = rx + AF_D + AF_VAO, py = y + 2.0f, ph = alt - 4.0f;
      float vis = pw * (t > 1.0f ? 1.0f : t);
      float ca = anim_clamp((t - 0.25f) / 0.6f, 0.0f, 1.0f);
      int k;
      GfxRect pr = { px, py, vis + (t > 1.0f ? pw * (t - 1.0f) : 0.0f), ph };
      if (pr.w > 2.0f * AF_RAIO_PAINEL) {
        if (ajustes_vidro()) gfx_vidro_folha(pr, AF_RAIO_PAINEL / ph, 1.0f);
        else gfx_cor(pr, AF_RAIO_PAINEL / ph, 0.18f, 0.172f, 0.212f, 0.92f);
        { float cy0 = py > corte ? py : corte;
          gfx_recorte(pr.x, cy0, pr.w, py + ph - cy0); }
        for (k = 0; k < a->nTit; k++) {
          GfxRect c = { px + AF_PAD + (float)k * (larguraCartao(alt) + AF_GAP_CART),
                        py + AF_PAD, larguraCartao(alt), alturaCartao(alt) };
          float fc = (focada && i == coluna) ? fCartao[k] : 0.0f;
#ifdef NV_TOUCH_UI
          { GfxRect alvo = c;
            if (alvo.x + alvo.w > pr.x + pr.w) alvo.w = pr.x + pr.w - alvo.x;
            if (ca > 0.5f) alvoToque(alvo, corte, i, k); }
#endif
          svd_cartao(c, &a->tit[k], fc, ca, agora);
        }
        gfx_recorte(0.0f, corte, NV_TELA_W, NV_TELA_H - corte);
      }
    }
    if (rx + AF_D < -40.0f) continue;
    if (a) svd_rosto(face, a, fr, 1.0f, agora);
    else svd_rosto_acao(face, "mais", fr, 1.0f);
#ifdef NV_TOUCH_UI
    alvoToque((GfxRect){ face.x, face.y, face.w, face.h + 64.0f }, corte, i, -1);
#endif
    { const char *nome = a ? a->nome : "Adicionar";
      TxtLinha rep = txt_linha_corta(TXT_CAPTION, nome, 214, 212, 222, 255, AF_P - 16.0f);
      TxtLinha foc = txt_linha_corta(TXT_CAPTION, nome, 255, 255, 255, 255, AF_P - 16.0f);
      float nx = rx + (AF_D - (float)rep.w) * 0.5f;
      // O nome sobe com o foco do rosto, que cresce 8 %.
      svd_txt_foco(rep, foc, nx, fy + AF_D + 30.0f, svd_foco_visual(fr), 1.0f); }
  }
}

