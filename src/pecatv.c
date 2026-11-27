// Ver pecatv.h.
#include "pecatv.h"
#include "peca.h"
#include "pecaserv.h"
#include "pecafio.h"
#include "ajustes.h"
#include "anim.h"
#include "app.h"
#include "catalogo.h"
#include "celbotao.h"
#include "dados.h"
#include "idioma.h"
#include "idiomacod.h"
#include "jsw.h"
#include "layout.h"
#include "perfilinfantil.h"
#include "ponteiro.h"
#include "tex_cache.h"
#include "text.h"
#define NV_ESCALA_TELA
#include "escala.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARQ_TOKEN "peca_token.txt"
#define ARQ_CHAVE "gemini.txt"

enum { T_FECHADO = 0, T_PAREAR, T_PENSANDO, T_PRONTO, T_ERRO };

static int tela, iniciou, foco;
static float animT;
static const char *estado = "livre";   // o que o celular le
static PecaResultado res;
static PecaSugestao tit[PECA_N];
static int nTit;
static char pedido[PECA_PEDIDO_MAX + 1];
static char chave[96];
static int idiomaPagina = -1;

int pecatv_disponivel(void) { return pecaserv_disponivel(); }
int pecatv_tem_chave(void) { return chave[0] != 0; }
int pecatv_aberto(void) { return tela != T_FECHADO; }
void pecatv_fechar(void) { tela = T_FECHADO; animT = 0.0f; }

static int lerToken(char *t, size_t n) {
  char *s = dados_ler(ARQ_TOKEN);
  t[0] = 0;
  if (s) { snprintf(t, n, "%.*s", PECA_TOKEN_N, s); free(s); }
  return peca_token_valido(t);
}
int pecatv_pareado(void) { char t[PECA_TOKEN_N + 4]; return lerToken(t, sizeof t); }

static void carregarChave(void) {
  char *s = dados_ler(ARQ_CHAVE);
  peca_chave_limpar(s, chave, sizeof chave);
  free(s);
}

static const char *msgErro(PecaResultado r) {
  switch (r) {
    case PECA_SEM_CHAVE: return i18n("Falta a chave do Gemini. Abra a página no celular para salvar.");
    case PECA_CHAVE:     return i18n("O Gemini recusou a chave. Salve outra pelo celular.");
    case PECA_COTA:      return i18n("Limite de uso da sua chave do Gemini. Tente daqui a pouco.");
    case PECA_REDE:      return i18n("Sem resposta do Gemini. Confira a internet da TV.");
    case PECA_VAZIO:     return i18n("Não achei títulos para esse pedido. Tente com outras palavras.");
    case PECA_INFANTIL:  return i18n("Indisponível em perfil infantil.");
    default:             return i18n("O Gemini não atendeu esse pedido.");
  }
}

// --- a pagina do celular -------------------------------------------------------
// Os textos entram como JSON (jsw escapa) e a pagina so escreve com
// textContent: nem a traducao nem a resposta do modelo viram HTML.
#include "pecapagina.inc"

static char *montarPagina(void) {
  Jsw w;
  const char *l;
  char *out = NULL;
  jsw_iniciar(&w);
  jsw_obj_ini(&w);
  jsw_cs(&w, "h", i18n("Peça como fala"));
  jsw_cs(&w, "kt", i18n("Cole sua chave do Gemini (aistudio.google.com/apikey). Ela fica só na TV e só é enviada ao Google."));
  jsw_cs(&w, "kb", i18n("Salvar chave"));
  jsw_cs(&w, "b", i18n("Pedir"));
  jsw_cs(&w, "ph", i18n("Algo curto e engraçado para ver com minha mãe"));
  jsw_cs(&w, "p", i18n("Pensando…"));
  jsw_cs(&w, "o", i18n("Abrir na TV"));
  jsw_chave(&w, "err"); jsw_arr_ini(&w);
  for (int r = 0; r <= PECA_ERRO; r++) jsw_str(&w, msgErro((PecaResultado)r));
  jsw_arr_fim(&w);
  jsw_obj_fim(&w);
  l = jsw_texto_final(&w);
  if (l && (out = malloc(sizeof PAGINA_A + strlen(l) + sizeof PAGINA_B)))
    sprintf(out, "%s%s%s", PAGINA_A, l, PAGINA_B);
  jsw_livre(&w);
  return out;
}

static void publicar(void) {
  char *j = peca_estado_json(estado, res, chave[0] != 0, pedido, tit, nTit);
  pecaserv_estado(j);
  free(j);
}

static int subir(void) {
  char t[PECA_TOKEN_N + 4], *html;
  int ok;
  if (!lerToken(t, sizeof t)) return 0;
  html = montarPagina();
  ok = html && pecaserv_iniciar(t, html);
  free(html);
  memset(t, 0, sizeof t);
  if (ok) { idiomaPagina = ajustes_idioma(); publicar(); }
  return ok;
}

static void novoToken(void) {
  char t[PECA_TOKEN_N + 4];
  if (pecaserv_gerar_token(t, sizeof t)) dados_gravar(ARQ_TOKEN, t);
  memset(t, 0, sizeof t);
}

void pecatv_parear(void) {
  if (!pecatv_disponivel() || !ajustes_peca()) return;
  if (!pecatv_pareado()) novoToken();
  subir();
  tela = T_PAREAR; animT = 0.0f;
  puts("[peca] cartao de pareamento aberto");
}

// --- o contexto: o que a conta ja viu, esta vendo e salvou ------------------------
// Do catalogo em memoria (historico da conta/Trakt, Continuar assistindo e
// Salvos). So os NOMES vao ao modelo. Os ponteiros valem ate o fim do quadro;
// pecafio_pedir copia.
static void por(const char **v, int *n, const char *nome) {
  if (*n >= PECA_CTX_MAX || !nome[0]) return;
  for (int i = 0; i < *n; i++) if (!strcmp(v[i], nome)) return;
  v[(*n)++] = nome;
}
static void contexto(PecaContexto *c) {
  int n = cat_n();
  memset(c, 0, sizeof *c);
  for (int i = 0; i < n; i++) {
    const CatItem *it = cat_item(i);
    if (!it || strncmp(it->imdb, "tt", 2)) continue;
    if (cat_visto(it)) por(c->vistos, &c->nVistos, it->titulo);
    else if (it->progresso > 0) por(c->andamento, &c->nAndamento, it->titulo);
    else if (it->naLista) por(c->lista, &c->nLista, it->titulo);
  }
}

static void falhar(PecaResultado r, int podeMostrar) {
  res = r; nTit = 0; estado = "erro";
  publicar();
  if (podeMostrar) { tela = T_ERRO; animT = 0.0f; }
}

static void atender(const char *txt, int podeMostrar) {
  PecaContexto ctx;
  snprintf(pedido, sizeof pedido, "%.*s", PECA_PEDIDO_MAX, txt);
  if (perfil_infantil_ativo()) { falhar(PECA_INFANTIL, podeMostrar); return; }
  carregarChave();   // outra tela pode ter salvo a mesma chave
  if (!chave[0]) { falhar(PECA_SEM_CHAVE, podeMostrar); return; }
  contexto(&ctx);
  printf("[peca] pedindo: contexto %d vistos, %d em andamento, %d salvos\n", ctx.nVistos, ctx.nAndamento, ctx.nLista);
  if (!pecafio_pedir(chave, pedido, idioma_iso(ajustes_idioma()), &ctx)) { falhar(PECA_ERRO, podeMostrar); return; }
  res = PECA_OK; nTit = 0; estado = "pensando";
  publicar();
  if (podeMostrar) { tela = T_PENSANDO; animT = 0.0f; }
}

void pecatv_atualizar(float dt, int podeMostrar) {
  char txt[PECA_CORPO_MAX + 1];
  int i;
  if (!pecatv_disponivel()) return;
  if (!ajustes_peca()) {
    if (pecaserv_no_ar()) { pecaserv_parar(); pecafio_cancelar(); estado = "livre"; nTit = 0; }
    if (tela) pecatv_fechar();
    iniciou = 0;
    return;
  }
  if (!iniciou) { iniciou = 1; carregarChave(); if (pecatv_pareado()) subir(); }
  if (tela && !podeMostrar) pecatv_fechar();
  if (tela) animT = ajustes_animacoes_reduzidas() ? 1.0f : anim_mola(animT, 1.0f, dt, NV_MOLA_TELA);
  if (!pecaserv_no_ar()) return;
  if (idiomaPagina != ajustes_idioma()) subir();   // a pagina fala o idioma da TV
  if (pecaserv_pegar_chave(txt, sizeof txt)) {
    peca_chave_limpar(txt, chave, sizeof chave);
    if (chave[0]) dados_gravar(ARQ_CHAVE, chave); else dados_apagar(ARQ_CHAVE);
    ajustes_gemini_recarregar();   // mesma chave do "O que eu perdi?" (gemini.txt)
    memset(txt, 0, sizeof txt);
    if (res == PECA_SEM_CHAVE || res == PECA_CHAVE) { estado = "livre"; res = PECA_OK; if (tela == T_ERRO) pecatv_fechar(); }
    publicar();
  }
  // Trocou para o perfil infantil com pedido ou resultado do adulto: some tudo.
  if (perfil_infantil_ativo() && (!strcmp(estado, "pronto") || !strcmp(estado, "pensando"))) {
    pecafio_cancelar();
    nTit = 0; pedido[0] = 0; estado = "livre"; res = PECA_OK;
    publicar();
    if (tela == T_PRONTO || tela == T_PENSANDO) pecatv_fechar();
  }
  if (pecaserv_pegar_pedido(txt, sizeof txt)) atender(txt, podeMostrar);
  {
    PecaResultado r; PecaSugestao v[PECA_N]; int n = 0, mg, mc;
    if (pecafio_pegar(&r, v, &n, &mg, &mc)) {
      if (r == PECA_OK && n > 0) {
        res = r; nTit = n; memcpy(tit, v, sizeof(PecaSugestao) * (size_t)n);
        estado = "pronto"; foco = 0;
        publicar();
        // So troca o "pensando" que ainda esta na tela: quem fechou, fechou.
        if (tela == T_PENSANDO) { tela = T_PRONTO; animT = 0.0f; }
      } else falhar(r, tela == T_PENSANDO);
    }
  }
  if (pecaserv_pegar_abrir(&i) && podeMostrar && !strcmp(estado, "pronto") && i >= 0 && i < nTit) {
    printf("[peca] abrir pelo celular: %s\n", tit[i].id);
    app_abrir_titulo(tit[i].id);
    pecatv_fechar();
  }
}

int pecatv_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!tela) return 0;
  if (e->type == SDL_TEXTINPUT || e->type == SDL_TEXTEDITING || e->type == SDL_KEYUP) return 1;
  if (e->type != SDL_KEYDOWN) return 0;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    pecatv_fechar();
    return 1;
  }
  if (e->key.repeat) return 1;
  if (tela == T_PAREAR) {
    if (k == SDLK_UP) { novoToken(); subir(); puts("[peca] endereco trocado"); }
    else if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && !pecaserv_no_ar()) subir();
  } else if (tela == T_PRONTO) {
    if (k == SDLK_LEFT && foco > 0) foco--;
    else if (k == SDLK_RIGHT && foco < nTit - 1) foco++;
    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
      printf("[peca] abrir: %s\n", tit[foco].id);
      app_abrir_titulo(tit[foco].id);
      pecatv_fechar();
    }
  }
  return 1;
}

// --- desenho ---------------------------------------------------------------------
static void fecharPonteiro(int a, int b) { (void)a; (void)b; pecatv_fechar(); }
static void abrirPonteiro(int a, int b) {
  (void)b;
  if (a >= 0 && a < nTit) { app_abrir_titulo(tit[a].id); pecatv_fechar(); }
}
static void focarPonteiro(int a, int b) { (void)b; if (a >= 0 && a < nTit) foco = a; }

static void painel(GfxRect r, float a) {
  float ar, ag, ab;
  const float raio = 28.0f / r.h;
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, 0, 0, 0, .55f * a);
  gfx_rect((GfxRect){ r.x - 18.0f, r.y - 8.0f, r.w + 36.0f, r.h + 40.0f }, 0, GFX_SOMBRA,
           1.0f, 0, 0, 0.5f, 0, 0, 0, 0.42f * a);
  if (ajustes_vidro()) gfx_vidro_material(r, raio, GFX_VIDRO_DENS, a);
  else {
    gfx_cor(r, raio, .098f, .102f, .118f, .99f * a);
    gfx_luz_canto(r, raio, r.w * .25f, -r.h * .25f, r.w * .9f, ar, ag, ab, .08f * a);
  }
}

static float cabecalho(GfxRect r, float a, int comPedido) {
  float ar, ag, ab, y = r.y + 30.0f;
  TxtLinha t = txt_linha_corta(TXT_CALLOUT, i18n("Peça como fala"), 245, 248, 255, 255, r.w - 110.0f);
  ajustes_acento(&ar, &ag, &ab);
  gfx_icone((GfxRect){ r.x + 30.0f, y + (t.h - 28.0f) * 0.5f, 28.0f, 28.0f }, "aj_smartphone", ar, ag, ab, a);
  txt_desenhar_alpha(t, r.x + 70.0f, y, a);
  y += t.h + 10.0f;
  if (comPedido && pedido[0]) {
    char q[PECA_PEDIDO_MAX + 8];
    snprintf(q, sizeof q, "\xE2\x80\x9C%s\xE2\x80\x9D", pedido);
    y += txt_bloco_bruto(TXT_CAPTION, q, 170, 174, 184, r.x + 30.0f, y, r.w - 60.0f, 30.0f, a, 2) + 8.0f;
  }
  return y;
}

static void desenharParear(float a) {
  const float w = 440.0f, lado = celb_qr_lado();
  const char *u = pecaserv_url(), *curta = !strncmp(u, "http://", 7) ? u + 7 : u;
  GfxRect r = { (NV_TELA_W - w) * 0.5f, 0, w, u[0] ? 640.0f : 220.0f };
  float y;
  r.y = (NV_TELA_H - r.h) * 0.5f - (1.0f - a) * 10.0f;
  painel(r, a);
  y = cabecalho(r, a, 0) + 8.0f;
  if (!u[0]) {
    txt_bloco(TXT_BODY, i18n("Sem rede local. Conecte a TV ao Wi-Fi e aperte OK."), 200, 204, 212,
              r.x + 30.0f, y, w - 60.0f, 32.0f, a, 3);
    return;
  }
  celb_qr_desenhar(u, r.x + (w - lado) * 0.5f, y, a);
  y += lado + 16.0f;
  { TxtLinha t = txt_linha_corta_bruta(TXT_CAPTION, curta, 232, 236, 244, 255, w - 40.0f);
    txt_desenhar_alpha(t, r.x + (w - t.w) * 0.5f, y, a); y += t.h + 12.0f; }
  y += txt_bloco(TXT_CAPTION2, i18n("Aponte a câmera do celular e diga o que quer assistir. Mesma rede Wi-Fi. O endereço não muda: salve a página."),
                 170, 174, 184, r.x + 30.0f, y, w - 60.0f, 26.0f, a * 0.9f, 4) + 10.0f;
  txt_bloco(TXT_CAPTION2, i18n("Para cima troca o endereço e desconecta os celulares pareados."),
            130, 134, 146, r.x + 30.0f, y, w - 60.0f, 26.0f, a * 0.9f, 3);
}

static void desenharAviso(float a, const char *msg) {
  const float w = 760.0f;
  GfxRect r = { (NV_TELA_W - w) * 0.5f, 0, w, 250.0f };
  float y;
  r.y = (NV_TELA_H - r.h) * 0.5f - (1.0f - a) * 10.0f;
  painel(r, a);
  y = cabecalho(r, a, 1) + 6.0f;
  txt_bloco_bruto(TXT_BODY, msg, 232, 236, 244, r.x + 30.0f, y, w - 60.0f, 34.0f, a, 3);
}

#define PT_COL_W 400.0f
#define PT_CARTAZ_W 220.0f
#define PT_CARTAZ_H 330.0f
static void desenharTitulos(float a) {
  const float w = 60.0f + PT_COL_W * PECA_N, h = 760.0f;
  GfxRect r = { (NV_TELA_W - w) * 0.5f, (NV_TELA_H - h) * 0.5f - (1.0f - a) * 10.0f, w, h };
  float y, x0, ar, ag, ab;
  painel(r, a);
  ajustes_acento(&ar, &ag, &ab);
  y = cabecalho(r, a, 1) + 10.0f;
  x0 = r.x + 30.0f + (PECA_N - nTit) * PT_COL_W * 0.5f;
  for (int i = 0; i < nTit; i++) {
    const PecaSugestao *s = &tit[i];
    float x = x0 + i * PT_COL_W, cy = y + 12.0f;
    GfxRect col = { x + 8.0f, y, PT_COL_W - 16.0f, r.y + h - 24.0f - y };
    GfxRect c = { x + (PT_COL_W - PT_CARTAZ_W) * 0.5f, cy, PT_CARTAZ_W, PT_CARTAZ_H };
    GLuint tex = s->poster[0] && !tex_falhou(s->poster) ? tex_obter_larg(s->poster, PT_CARTAZ_W) : 0;
    char ano[16];
    if (i == foco) gfx_cor(col, 20.0f / col.h, ar, ag, ab, .22f * a);
    if (ponteiro_ativo()) ponteiro_alvo(col.x, col.y, col.w, col.h, focarPonteiro, abrirPonteiro, i, 0);
    if (tex) {
      gfx_tex_aspect_atual = tex_aspecto(s->poster);
      gfx_card_forcar_cover_atual = 1.0f;
      gfx_rect(c, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, 14.0f / c.h, 0, 0, 0, a);
      gfx_card_forcar_cover_atual = 0.0f;
      gfx_tex_aspect_atual = 0.0f;
    } else gfx_cor(c, 14.0f / c.h, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G, NV_COR_ESQUELETO_B, a);
    cy += PT_CARTAZ_H + 16.0f;
    cy += txt_bloco_bruto(TXT_BODY, s->titulo, 245, 248, 255, col.x + 16.0f, cy, col.w - 32.0f, 32.0f, a, 2) + 2.0f;
    if (s->ano) {
      TxtLinha t;
      snprintf(ano, sizeof ano, "%d", s->ano);
      t = txt_linha_corta_bruta(TXT_CAPTION2, ano, 150, 154, 166, 255, col.w - 32.0f);
      txt_desenhar_alpha(t, col.x + 16.0f, cy, a); cy += t.h + 6.0f;
    }
    txt_bloco_bruto(TXT_CAPTION2, s->motivo, 200, 204, 212, col.x + 16.0f, cy, col.w - 32.0f, 26.0f, a, 4);
  }
}

void pecatv_desenhar(void) {
  float a;
  if (!tela) return;
  ESCALA_INI();
  a = anim_suave(animT);
  ponteiro_camada();
  if (ponteiro_ativo()) ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, fecharPonteiro, 0, 0);
  if (tela == T_PAREAR) desenharParear(a);
  else if (tela == T_PENSANDO) desenharAviso(a, i18n("Pensando…"));
  else if (tela == T_ERRO) desenharAviso(a, msgErro(res));
  else if (nTit > 0) desenharTitulos(a);
  ESCALA_FIM();
}
