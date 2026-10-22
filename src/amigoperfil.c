// Perfil do amigo (tela C). Ver amigoperfil.h.
#include "amigoperfil.h"
#include "socialvis.h"
#include "svdesenho.h"
#include "ajustes.h"
#include "anim.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "layout.h"
#include "idioma.h"
#include "recomenda.h"
#include "badges.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "telefoneui.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// Tres fileiras de cartazes 2:3. 150x225 cabe as tres com titulo e legenda
// entre y=96 e y=1040 (medido na captura, amigoperfil no socialui_shot).
#define AP_PW        150.0f
#define AP_PH        225.0f
#define AP_PGAP       26.0f
#define AP_FILA_TOPO  92.0f
#define AP_FILA_PASSO 318.0f
enum { AP_ASSISTINDO = 0, AP_GOSTOU, AP_MANDOU, AP_NFILAS };

static char pessoa[96];
static SvPerfil perf;
static int temPerfil;
static unsigned revPerf = ~0u;
static int fila, col;
static float fFoco[AP_NFILAS][SV_FILA_MAX];
static float entrada;
static int sair, temPedido;
static char pedido[24];
static int nFila(int f);
#ifdef NV_TOUCH_UI
static ToqueRolagem toque[AP_NFILAS];
static float toqueX[AP_NFILAS];
static int toqueFila = -1;
static int toqueRolar(const PonteiroRolagem *e) {
  if (e->fase == PONT_ROL_INICIO) {
    toqueFila = -1;
    for (int f = 0; f < AP_NFILAS; f++) if (toquerol_evento(&toque[f], e)) { toqueFila = f; return 1; }
    return 0;
  }
  return toqueFila >= 0 && toqueFila < AP_NFILAS ? toquerol_evento(&toque[toqueFila], e) : 0;
}
static void toqueRetomar(void) {
  if (toqueFila >= 0 && toque[toqueFila].livre) {
    fila = toqueFila;
    col = (int)((toqueX[fila] + toque[fila].regiao.w * 0.35f) / (AP_PW + AP_PGAP));
    if (col >= nFila(fila)) col = nFila(fila) - 1;
    if (col < 0) col = 0;
  }
  for (int f = 0; f < AP_NFILAS; f++) toquerol_limpar(&toque[f]);
}
#endif

static int nFila(int f) {
  if (!temPerfil) return 0;
  return f == AP_ASSISTINDO ? perf.nAssistindo : f == AP_GOSTOU ? perf.nGostou : perf.nMandou;
}

static void recarregar(void) {
  temPerfil = socialvis_perfil(pessoa, &perf);
  revPerf = socialvis_revisao();
  if (fila < 0 || fila >= AP_NFILAS || nFila(fila) == 0) {
    int f;
    fila = -1;
    for (f = 0; f < AP_NFILAS; f++) if (nFila(f) > 0) { fila = f; break; }
    col = 0;
  }
  if (fila >= 0 && col >= nFila(fila)) col = nFila(fila) - 1;
}

void amigoperfil_abrir(const char *id) {
  snprintf(pessoa, sizeof pessoa, "%s", id ? id : "");
  fila = -1; col = 0;
  sair = 0; temPedido = 0;
  entrada = 0.0f;
#ifdef NV_TOUCH_UI
  memset(toque, 0, sizeof toque); memset(toqueX, 0, sizeof toqueX); toqueFila = -1;
#endif
  memset(fFoco, 0, sizeof fFoco);
  socialvis_atualizar();
  socialvis_abrir_perfil(pessoa);
  recarregar();
  socialvis_marcar_visto(pessoa);
}

int amigoperfil_quer_sair(void) { int v = sair; sair = 0; return v; }
int amigoperfil_pediu_titulo(char *imdb, size_t tam) {
  if (!temPedido) return 0;
  temPedido = 0;
  if (imdb && tam) snprintf(imdb, tam, "%s", pedido);
  return 1;
}

static const char *imdbDe(int f, int c) {
  if (f == AP_ASSISTINDO && c < perf.nAssistindo) return perf.assistindo[c].imdb;
  if (f == AP_GOSTOU && c < perf.nGostou) return perf.gostou[c].imdb;
  if (f == AP_MANDOU && c < perf.nMandou) return perf.mandou[c].imdb;
  return "";
}

// PONTEIRO (#99). O cartaz sob o cursor vira o foco pelas MESMAS variaveis
// das setas (fila, col); o OK do clique abre o titulo pelo caminho de sempre.
static void ponteiroCartaz(int f, int c) {
  if (!temPerfil || f < 0 || f >= AP_NFILAS || c < 0 || c >= nFila(f) || c >= SV_FILA_MAX) return;
#ifdef NV_TOUCH_UI
  for (int k = 0; k < AP_NFILAS; k++) toquerol_limpar(&toque[k]);
#endif
  if (f == fila && c == col) return;
  fila = f; col = c;
}
int amigoperfil_teste_foco(int *coluna) { if (coluna) *coluna = col; return fila; }

void amigoperfil_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!e || e->type != SDL_KEYDOWN) return;
#ifdef NV_TOUCH_UI
  if (toquerol_navegacao(e)) toqueRetomar();
#endif
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) { sair = 1; return; }
  if (fila < 0) return;
  if (k == SDLK_LEFT && col > 0) col--;
  else if (k == SDLK_LEFT) { sair = 1; return; }   // comeco da fileira: o app abre a barra
  else if (k == SDLK_RIGHT && col + 1 < nFila(fila)) col++;
  else if (k == SDLK_UP || k == SDLK_DOWN) {
    int d = k == SDLK_UP ? -1 : 1, f;
    for (f = fila + d; f >= 0 && f < AP_NFILAS; f += d)
      if (nFila(f) > 0) { fila = f; if (col >= nFila(f)) col = nFila(f) - 1; break; }
  } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    const char *id = imdbDe(fila, col);
    if (id[0]) { snprintf(pedido, sizeof pedido, "%s", id); temPedido = 1; }
  }
}

void amigoperfil_atualizar(float dt, Uint32 agora) {
  int f, c, red = ajustes_animacoes_reduzidas();
  socialvis_atualizar();
  // O MODELO MUDOU, ou passou um segundo: o perfil do servidor chega no fio
  // (recomenda_amigo) sem mexer na revisao do modelo.
  { static Uint32 ult;
    if (revPerf != socialvis_revisao() || agora - ult > 1000u) { ult = agora; recarregar(); } }
  entrada = red ? 1.0f : anim_mola(entrada, 1.0f, dt, NV_MOLA_TELA);
  for (f = 0; f < AP_NFILAS; f++)
    for (c = 0; c < SV_FILA_MAX; c++) {
      float alvo = (f == fila && c == col) ? 1.0f : 0.0f;
      fFoco[f][c] = red ? alvo : anim_mola(fFoco[f][c], alvo, dt, NV_MOLA_FOCO);
    }
}

// Cartaz com o aro de foco da home por fora.
static void cartaz(GfxRect r, const char *url, float f, float a) {
  float raio = NV_RAIO_CARD;
  if (f > 0.01f) {
    float ar, ag, ab, menor = r.w;
    GfxRect b = { r.x - NV_ANEL_FOCO, r.y - NV_ANEL_FOCO, r.w + 2 * NV_ANEL_FOCO, r.h + 2 * NV_ANEL_FOCO };
    ajustes_acento(&ar, &ag, &ab);
    if (ajustes_vidro()) gfx_vidro_cartao(r, raio, f, a);
    else gfx_cor(b, (raio * menor + NV_ANEL_FOCO) / (menor + 2 * NV_ANEL_FOCO), ar, ag, ab, f * a);
  }
  svd_poster(r, url, raio * r.w / r.h, a);
}

// `estilo` e o MESMO para os quatro rotulos (ver a grade): um corpo por cartao
// deixaria a grade com dois tamanhos de letra lado a lado. O ROTULO QUEBRA em
// ate duas linhas em vez de virar "recommendatio…" (Montserrat, a fonte da TV
// do dono, e mais larga que a Inter): quem dimensiona o cartao e quem decide
// quantas linhas cabem e a grade (rotuloLinhas / alturaCartao).
static int rotuloLinhas(TxtEstilo estilo, const char *rotulo, float larg) {
  return (float)txt_largura(estilo, rotulo) > larg ? 2 : 1;
}
static void numero(GfxRect r, const char *valor, const char *rotulo, TxtEstilo estilo, int compacto, float a) {
  // Cartao compacto (rotulo em duas linhas, ou perfil com "assistindo agora"
  // que empurra a grade): um corpo menor no numero, para o rotulo nao encostar.
  TxtLinha v = txt_linha(compacto ? TXT_HEADLINE : TXT_TITULO3, valor, 246, 246, 250, 255);
  float lw = r.w - 36.0f;
  TxtLinha um = txt_linha(estilo, "Hg", 167, 164, 178, 255);
  float lead = (float)um.h + 2.0f;
  int n = rotuloLinhas(estilo, rotulo, lw);
  gfx_cor(r, 18.0f / r.h, 0.118f, 0.114f, 0.141f, 0.94f * a);
  txt_desenhar_alpha(v, r.x + 20.0f, r.y + (compacto ? 8.0f : 10.0f), a);
  txt_bloco_corta(estilo, rotulo, 167, 164, 178,
                  r.x + 20.0f, r.y + r.h - (compacto ? 10.0f : 12.0f) - lead * (float)n + 2.0f,
                  lw, lead, a * 0.95f, 2);
}

static void tituloFila(float x, float y, const char *t, float a) {
  TxtLinha l = txt_linha(TXT_CALLOUT, t, 207, 205, 216, 255);
  txt_desenhar_alpha(l, x, y, a);
}

static void vazioFila(float x, float y, const char *t, float a) {
#ifdef NV_TOUCH_UI
  if (telefoneui_ativo()) {
    float w = fminf(4.0f * (AP_PW + AP_PGAP) - AP_PGAP, NV_TELA_W - 40 - x);
    float h = txt_bloco_corta(TXT_CAPTION, t, 140, 138, 150, 0, 0, w - 48, 30, 0, 0);
    h = fmaxf(90, h + 32);
    gfx_cor((GfxRect){x, y, w, h}, 18 / h, 1, 1, 1, .04f * a);
    txt_bloco_corta(TXT_CAPTION, t, 140, 138, 150, x + 24, y + 16, w - 48, 30, a, 0);
    return;
  }
#endif
  TxtLinha l = txt_linha_corta(TXT_CAPTION, t, 140, 138, 150, 255, 4.0f * (AP_PW + AP_PGAP) - AP_PGAP - 48.0f);
  gfx_cor((GfxRect){ x, y, 4.0f * (AP_PW + AP_PGAP) - AP_PGAP, 90.0f }, 18.0f / 90.0f,
          1.0f, 1.0f, 1.0f, 0.04f * a);
  txt_desenhar_alpha(l, x + 24.0f, y + (90.0f - (float)l.h) * 0.5f, a);
}

// SO O QUE A FONTE DA PESSOA DA: o servidor do Nuvio nao a conhece (404 em
// /v1/amigo — "trakt:kevin" nao e uma `pessoa` da conta Nuvio), entao so ha o
// que o feed trouxe daquela fonte. E um estado CORRETO, nao uma falha.
static int soFonte(void) {
  return perf.estado == SV_PERFIL_NAO_ACHOU && perf.porOnde != SV_FONTE_NUVIO;
}
// Nenhum dos quatro numeros do mes existe e nao vai existir enquanto o estado
// for este (pessoa fora do servidor, perfil negado ou nao compartilhado).
static int semNumeros(void) {
  int final = perf.estado == SV_PERFIL_NAO_ACHOU || perf.estado == SV_PERFIL_NEGADO || perf.compartilha == 0;
  return final && perf.minutosMes < 0 && perf.filmesMes < 0 && perf.seriesCurso < 0 &&
         !(perf.recsVistas >= 0 && perf.recsTotal > 0);
}

static void legenda(float x, float y, const char *t, int r, int g, int b, float a) {
  TxtLinha l = txt_linha_corta(TXT_MINI, t, r, g, b, 255, AP_PW + AP_PGAP - 8.0f);
  txt_desenhar_alpha(l, x, y, a);
}

void amigoperfil_desenhar(Uint32 agora) {
  float a = anim_suave(entrada);
  float R = ajustes_rail_largura_fixa();
  float lx = 96.0f + R, lw = 500.0f, rx = lx + lw + 80.0f;
  char buf[200];
  int f, c;
  float yBase = 920.0f;   // onde comecam as linhas de estado/foco, abaixo da grade
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, NV_COR_FUNDO_R, NV_COR_FUNDO_G,
          NV_COR_FUNDO_B, 1.0f);
  if (!temPerfil) {
    TxtLinha t = txt_linha(TXT_TITULO3, i18n("Perfil indisponível"), 240, 240, 245, 255);
    txt_desenhar_alpha(t, lx, 200.0f, a);
    return;
  }
  // A ARTE DO QUE ELE ESTA VENDO (ou viu por ultimo) atras, bem baixa: e o
  // "ambiente" da pessoa. Uma textura so, ja decodificada na fileira da home.
  { const SvEvento *e = perf.a.nTit > 0 ? &perf.a.tit[0] : NULL;
    const char *arte = e ? (e->arte[0] ? e->arte : e->poster) : NULL;
    GLuint t = arte ? tex_obter_larg(arte, 960.0f) : 0;
    if (t) {
      gfx_tex_aspect_atual = tex_aspecto(arte);
      gfx_rect((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H * 0.62f }, t, GFX_HERO, 0, 0, 0, 0,
               0, 0, 0, 0.22f * a);
      gfx_tex_aspect_atual = 0.0f;
    } }

  // --- coluna da pessoa ---
  { GfxRect av = { lx + 6.0f, 96.0f, 200.0f, 200.0f };
    float y;
    svd_rosto(av, &perf.a, 0.0f, a, agora);
    y = av.y + av.h + 40.0f;
    // O NOME E O SELO DE CRIADOR (o servidor decide; ver rec_selo_pessoa): o
    // nome e cortado para o selo caber ao lado.
    { float sw = rec_selo_pessoa_largura(perf.a.selo);
      TxtLinha t = txt_linha_corta(TXT_TITULO2, perf.a.nome, 246, 246, 250, 255, sw > 0.0f ? lw - sw - 12.0f : lw);
      txt_desenhar_alpha(t, lx, y, a);
      if (sw > 0.0f) rec_selo_pessoa(lx + (float)t.w + 12.0f, y + ((float)t.h - BADGE_H) * 0.5f, perf.a.selo, a);
      y += (float)t.h + 6.0f; }
    // "Amigos ha 2 meses · pelo codigo" — a FONTE pequena, so aqui.
    { char q[80] = "", por[80];
      if (perf.desde > 0) {
        long long d = ((long long)time(NULL) - perf.desde) / 86400;
        if (d >= 60) snprintf(q, sizeof q, i18n("Amigos há %d meses"), (int)(d / 30));
        else if (d >= 2) snprintf(q, sizeof q, i18n("Amigos há %d dias"), (int)d);
      }
      if (perf.porOnde == SV_FONTE_NUVIO) snprintf(por, sizeof por, "%s", i18n("pelo código"));
      else snprintf(por, sizeof por, i18n("pelo %s"), socialvis_fonte_nome(perf.porOnde));
      if (q[0]) snprintf(buf, sizeof buf, "%s \xc2\xb7 %s", q, por);
      else snprintf(buf, sizeof buf, "%s", por);
      // O @APELIDO na frente, quando a linha de cima e o nome e nao ele mesmo.
      if (perf.a.apelido[0] && SDL_strcasecmp(perf.a.apelido, perf.a.nome)) {
        char t2[sizeof buf];
        snprintf(t2, sizeof t2, "@%s \xc2\xb7 %s", perf.a.apelido, buf);
        snprintf(buf, sizeof buf, "%s", t2);
      }
      { TxtLinha t = txt_linha_corta(TXT_CAPTION, buf, 168, 166, 178, 255, lw);
        txt_desenhar_alpha(t, lx, y, a); y += (float)t.h + 28.0f; } }
    if (perf.a.agora && perf.a.nTit > 0) {
      char st[160];
      socialvis_status(&perf.a.tit[0], st, sizeof st);
      snprintf(buf, sizeof buf, "%s \xc2\xb7 %s", st, perf.a.tit[0].titulo);
      svd_ponto_vivo(lx + 8.0f, y + 15.0f, 14.0f, 0.0f, a, agora);
      { TxtLinha t = txt_linha_corta(TXT_CAPTION, buf, 163, 230, 186, 255, lw - 26.0f);
        txt_desenhar_alpha(t, lx + 26.0f, y, a); y += (float)t.h + 22.0f; }
    }
    // COMPARACAO (F08): os cinco cartoes SEMPRE aparecem, cada um com o dado
    // ou com o motivo de nao haver dado (privado, poucos pares, servidor
    // antigo, sem fonte). Nada vira zero: "0 %" seria uma afirmacao falsa sobre
    // duas pessoas. O tamanho da amostra vai junto do numero.
    { int q;
      float rotW = 0.0f;
      TxtLinha tit = txt_linha(TXT_MINI, i18n("Comparação"), 168, 166, 178, 255);
      txt_desenhar_alpha(tit, lx, y, a);
      y += (float)tit.h + 6.0f;
      for (q = 0; q < SV_CMP_N; q++) {
        TxtLinha l = txt_linha(TXT_CAPTION, socialvis_cmp_rotulo(q), 150, 148, 160, 255);
        if ((float)l.w > rotW) rotW = (float)l.w;
      }
      rotW += 20.0f;
      for (q = 0; q < SV_CMP_N; q++) {
        char val[96];
        int ok = 0;
        TxtLinha l = txt_linha(TXT_CAPTION, socialvis_cmp_rotulo(q), 150, 148, 160, 255), v;
        socialvis_cmp_texto(q, &perf.cmp[q], val, sizeof val, &ok);
        v = ok ? txt_linha_corta(TXT_CAPTION, val, 246, 246, 250, 255, lw - rotW)
               : txt_linha_corta(TXT_CAPTION, val, 128, 126, 138, 255, lw - rotW);
        txt_desenhar_alpha(l, lx, y, a);
        txt_desenhar_alpha(v, lx + rotW, y, a);
        y += (float)l.h + 2.0f;
      }
      y += 12.0f; }
    // OS QUATRO NUMEROS DO MES, numa grade 2x2. "—" quando nao ha dado.
    // SEM NENHUM dos quatro e sem ter de onde vir (a pessoa so existe no Trakt,
    // o servidor nao a conhece, ou ela nao compartilha): a grade some e entra
    // UMA frase que diz o motivo. Quatro "—" lado a lado nao dizem nada.
    if (semNumeros()) {
      const char *msg = soFonte() ? i18n("Aqui só aparece o que %s compartilha.")
                                  : i18n("Perfil indisponível");
      if (soFonte()) snprintf(buf, sizeof buf, msg, socialvis_fonte_nome(perf.porOnde));
      else snprintf(buf, sizeof buf, "%s", msg);
      { float bh = txt_bloco_corta(TXT_CAPTION, buf, 168, 166, 178, lx, y + 4.0f, lw, 28.0f, a, 3);
        yBase = y + 4.0f + bh + 18.0f; }
    } else {
      char v[4][32];
      const char *rot[4] = { "assistidas neste mês", "filmes vistos", "séries em curso",
                             "recomendações vistas" };
      float bw = (lw - 16.0f) * 0.5f, bh = 108.0f;
      TxtEstilo est = TXT_CAPTION;
      int i, compacto = 0, k, nl;
      if (perf.minutosMes >= 0) snprintf(v[0], sizeof v[0], i18n("%d h"), perf.minutosMes / 60);
      else snprintf(v[0], sizeof v[0], "\xe2\x80\x94");
      if (perf.filmesMes >= 0) snprintf(v[1], sizeof v[1], "%d", perf.filmesMes);
      else snprintf(v[1], sizeof v[1], "\xe2\x80\x94");
      if (perf.seriesCurso >= 0) snprintf(v[2], sizeof v[2], "%d", perf.seriesCurso);
      else snprintf(v[2], sizeof v[2], "\xe2\x80\x94");
      if (perf.recsVistas >= 0 && perf.recsTotal > 0)
        snprintf(v[3], sizeof v[3], i18n("%d de %d"), perf.recsVistas, perf.recsTotal);
      else snprintf(v[3], sizeof v[3], "\xe2\x80\x94");
      if (y < 640.0f) y = 640.0f;
      // O ROTULO INTEIRO, em ate duas linhas, no corpo de legenda: com Montserrat
      // (a fonte da TV do dono) "recomendações vistas" nao cabe em 206 px numa
      // linha so, e antes virava "recommendatio…". A grade cabe ate y=920 (onde
      // comecam as linhas de estado e de foco, que nao podem encostar no
      // rodape); se duas linhas no corpo de legenda nao cabem, os quatro
      // descem para o corpo pequeno, e so depois o cartao encolhe (minimo 92).
      for (k = 0; k < 2; k++) {
        TxtEstilo e = k == 0 ? TXT_CAPTION : TXT_MINI;
        float lead = (float)txt_linha(e, "Hg", 167, 164, 178, 255).h + 2.0f, need;
        nl = 1;
        for (i = 0; i < 4; i++) { int n = rotuloLinhas(e, i18n(rot[i]), bw - 36.0f); if (n > nl) nl = n; }
        compacto = nl > 1 || k == 1;
        need = (compacto ? 8.0f + (float)NV_FT_HEADLINE * 1.15f : 10.0f + (float)NV_FT_TITULO3 * 1.15f)
               + 4.0f + lead * (float)nl + 12.0f;
        if (need < 108.0f) need = 108.0f;
        est = e; bh = need;
        if (y + 2.0f * bh + 12.0f <= 920.0f) break;
      }
      if (y + 2.0f * bh + 12.0f > 920.0f) {
        bh = (920.0f - 12.0f - y) * 0.5f;
        if (bh < 92.0f) bh = 92.0f;
        compacto = 1;
      }
      for (i = 0; i < 4; i++) {
        GfxRect r = { lx + (float)(i % 2) * (bw + 16.0f), y + (float)(i / 2) * (bh + 12.0f), bw, bh };
        numero(r, v[i], i18n(rot[i]), est, compacto, a);
      }
      yBase = y + 2.0f * bh + 12.0f + 18.0f; } }
  if (yBase < 920.0f) yBase = 920.0f;

  // Cached cards stay visible during refresh, but loading/privacy/network
  // failures are explicit rather than all looking like an empty history.
  if (perf.estado == SV_PERFIL_INDO || perf.estado == SV_PERFIL_FALHA || perf.compartilha == 0) {
    const char *st = perf.estado == SV_PERFIL_INDO ? "Carregando atividade…"
                   : perf.estado == SV_PERFIL_FALHA ? (perf.compartilha >= 0 ? "Não foi possível atualizar. Mostrando os dados salvos."
                                                        : "Não foi possível atualizar. Tente novamente.")
                   : "Esta pessoa não compartilha sua atividade.";
    TxtLinha t = txt_linha_corta(TXT_CAPTION, i18n(st), 168, 166, 178, 255, lw);
    txt_desenhar_alpha(t, lx, yBase, a);
  }

  // Focus on a shared event compares only the progress that is actually known.
  // A missing local history is unknown, not evidence the viewer never watched.
  if (fila == AP_ASSISTINDO || fila == AP_GOSTOU) {
    const SvEvento *e = fila == AP_ASSISTINDO ? &perf.assistindo[col] : &perf.gostou[col];
    char st[160], meu[160], ep[24];
    int pct, t, epn, m = socialvis_meu_estado(e->imdb, &pct, &t, &epn);
    socialvis_status(e, st, sizeof st);
    snprintf(buf, sizeof buf, "%s: %s", perf.a.nome, st);
    { TxtLinha l = txt_linha_corta(TXT_MINI, buf, 188, 185, 198, 255, lw);
      txt_desenhar_alpha(l, lx, yBase + 36.0f, a); }
    if (m == 2) snprintf(meu, sizeof meu, "%s", i18n("Você viu"));
    else if (m == 1) {
      SvEvento eu = {0}; eu.temporada = t; eu.episodio = epn;
      socialvis_ep(&eu, ep, sizeof ep);
      snprintf(meu, sizeof meu, "%s%s%s · %d%%", i18n("Você está assistindo"), ep[0] ? " · " : "", ep, pct);
    } else snprintf(meu, sizeof meu, "%s", i18n("Seu progresso não está disponível"));
    { TxtLinha l = txt_linha_corta(TXT_MINI, meu, 188, 185, 198, 255, lw);
      txt_desenhar_alpha(l, lx, yBase + 68.0f, a); }
  }

  // A RESPOSTA DE QUEM RECEBEU, no cartaz de "Você mandou" em foco: o que ela
  // fez com o título e, se escreveu, a mensagem curta (so texto limpo, entre aspas).
  if (fila == AP_MANDOU && col < perf.nMandou) {
    const SvEnviada *m = &perf.mandou[col];
    if (m->estado == SV_REC_VIU || m->respondido > 0) {
      snprintf(buf, sizeof buf, "%s: %s", perf.a.nome, socialvis_enviada_rotulo(m, NULL));
      { TxtLinha l = txt_linha_corta(TXT_MINI, buf, 188, 185, 198, 255, lw);
        txt_desenhar_alpha(l, lx, yBase + 36.0f, a); }
      if (m->resposta[0]) {
        snprintf(buf, sizeof buf, "\xe2\x80\x9c%s\xe2\x80\x9d", m->resposta);
        { TxtLinha l = txt_linha_corta(TXT_MINI, buf, 247, 192, 138, 255, lw);
          txt_desenhar_alpha(l, lx, yBase + 68.0f, a); }
      }
    }
  }

  // --- as tres fileiras ---
#ifdef NV_TOUCH_UI
  ponteiro_rolagem(toqueRolar);
#endif
  for (f = 0; f < AP_NFILAS; f++) {
    float y = AP_FILA_TOPO + (float)f * AP_FILA_PASSO;
    const char *tit = f == AP_ASSISTINDO ? "Assistindo" : f == AP_GOSTOU ? "Gostou recentemente"
                                                                       : "Você mandou";
    int n = nFila(f);
#ifdef NV_TOUCH_UI
    float util = fmaxf(0.0f, NV_TELA_W - 40.0f - rx);
    float maxX = fmaxf(0.0f, (n - 1) * (AP_PW + AP_PGAP) + AP_PW - util);
    toquerol_vincular(&toque[f], (GfxRect){ rx, y + 46.0f, util, AP_PH + 40.0f }, gfx_escala(), 0.0f, maxX, 0, &toqueX[f]);
    if (f == fila && !toque[f].livre) {
      float esq = col * (AP_PW + AP_PGAP), dir = esq + AP_PW;
      if (esq < toqueX[f]) toqueX[f] = esq;
      if (dir > toqueX[f] + util) toqueX[f] = dir - util;
      toqueX[f] = toquerol_clamp(toqueX[f], 0.0f, maxX);
    }
#endif
    tituloFila(rx, y, i18n(tit), a);
    y += 46.0f;
    if (n == 0) {
      const char *st = f == AP_MANDOU ? "Você ainda não mandou nada para essa pessoa."
                    : perf.estado == SV_PERFIL_INDO ? "Carregando atividade…"
                    : soFonte() ? "Nada compartilhado por aqui ainda"
                    : (perf.estado == SV_PERFIL_NAO_ACHOU || perf.estado == SV_PERFIL_NEGADO) ? "Perfil indisponível"
                    : perf.estado == SV_PERFIL_FALHA ? (perf.compartilha >= 0 ? "Não foi possível atualizar. Mostrando os dados salvos."
                                                        : "Não foi possível atualizar. Tente novamente.")
                    : perf.compartilha == 0 ? "Esta pessoa não compartilha sua atividade."
                    : "Nada compartilhado por aqui ainda";
      vazioFila(rx, y, i18n(st), a);
      continue;
    }
#ifdef NV_TOUCH_UI
    gfx_recorte(rx, y - 4.0f, util, AP_PH + 48.0f);
#endif
    for (c = 0; c < n && c < SV_FILA_MAX; c++) {
      float x = rx + (float)c * (AP_PW + AP_PGAP);
#ifdef NV_TOUCH_UI
      x -= toqueX[f];
#endif
      GfxRect r = { x, y, AP_PW, AP_PH };
      float fc = fFoco[f][c];
#ifdef NV_TOUCH_UI
      if (x >= rx + util || x + AP_PW <= rx) continue;
      if (a > 0.99f) {
        float ax = fmaxf(r.x, rx), dir = fminf(r.x + r.w, rx + util);
        ponteiro_alvo(ax, r.y, dir - ax, r.h, ponteiroCartaz, NULL, f, c);
      }
#else
      if (x + AP_PW > NV_TELA_W - 40.0f) break;
      if (a > 0.99f) ponteiro_alvo(r.x, r.y, r.w, r.h, ponteiroCartaz, NULL, f, c);
#endif
      if (f == AP_MANDOU) {
        const SvEnviada *m = &perf.mandou[c];
        cartaz(r, m->poster, fc, a);
        // O ESTADO, que e o motivo desta fileira existir.
        { int ok = 0;
          const char *st = socialvis_enviada_rotulo(m, &ok);
          legenda(x, y + AP_PH + 10.0f, st,
                  ok ? 111 : 160, ok ? 207 : 158, ok ? 151 : 170, a); }
      } else {
        const SvEvento *e = f == AP_ASSISTINDO ? &perf.assistindo[c] : &perf.gostou[c];
        char ep[24];
        cartaz(r, e->poster[0] ? e->poster : e->arte, fc, a);
        if (e->pct >= 0 && f == AP_ASSISTINDO)
          svd_barra((GfxRect){ x + 10.0f, y + AP_PH - 14.0f, AP_PW - 20.0f, 5.0f }, e->pct, a);
        socialvis_ep(e, ep, sizeof ep);
        if (f == AP_ASSISTINDO && e->acao == SV_AGORA) {
          snprintf(buf, sizeof buf, "%s%s%s", i18n("Agora"), ep[0] ? " \xc2\xb7 " : "", ep);
          legenda(x, y + AP_PH + 10.0f, buf, 138, 225, 168, a);
        } else if (f == AP_ASSISTINDO) {
          legenda(x, y + AP_PH + 10.0f, ep[0] ? ep : e->titulo, 190, 188, 200, a);
        } else {
          legenda(x, y + AP_PH + 10.0f, e->titulo, 190, 188, 200, a);
        }
      }
    }
#ifdef NV_TOUCH_UI
    gfx_sem_recorte();
#endif
  }
  { TxtLinha t = txt_linha(TXT_MINI, i18n("OK · abrir o título   ·   Voltar · fechar"), 150, 148, 160, 255);
    txt_desenhar_alpha(t, lx, NV_TELA_H - 44.0f, a * 0.9f); }
}

