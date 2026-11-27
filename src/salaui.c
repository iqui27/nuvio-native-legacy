// A tela da Sessao e a pagina que o celular abre. Ver salaui.h.
#include "salaui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sala.h"
#include "celular.h"
#include "celbotao.h"
#include "catalogo.h"
#include "perfilinfantil.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "idioma.h"
#include "tex_cache.h"
#include "ponteiro.h"

#define SU_CARTAS      20     // cabe em celular.c (pagina de 24 KB) com folga
#define SU_VALIDADE_S  1800   // a sala fica de pe meia hora
#define SU_VISTOS_MAX  120    // "outra rodada" nao repete estes

typedef struct { char imdb[64], titulo[160], meta[96], poster[200]; } Carta;

static Sala sala;
static Carta cartas[SU_CARTAS];
static int nCartas, aberta, semBaralho;
static float entrada;
static char pediu[64];
static char vistos[SU_VISTOS_MAX][64];
static int nVistos;
static char pag[24000];

int salaui_disponivel(void) { return celular_disponivel(); }
int salaui_aberta(void) { return aberta; }

static int jaTem(const char *imdb) {
  int i;
  for (i = 0; i < nCartas; i++) if (!strcmp(cartas[i].imdb, imdb)) return 1;
  for (i = 0; i < nVistos; i++) if (!strcmp(vistos[i], imdb)) return 1;
  return 0;
}

// Uma carta do item, se servir: id do IMDb, com titulo, ainda nao mostrado e,
// num perfil infantil, de classificacao livre/10 anos.
static void cartaDe(const CatItem *it, const char *tipo, int infantil) {
  Carta *c;
  // Id com ':' e episodio (Continuar assistindo): a serie ja entra pelo id dela.
  if (nCartas >= SU_CARTAS || !it || strncmp(it->imdb, "tt", 2) || strchr(it->imdb, ':') || strcmp(it->tipo, tipo) ||
      !it->titulo[0] || jaTem(it->imdb)) return;
  if (infantil && !perfil_infantil_classif_ok(it->classificacao)) return;
  c = &cartas[nCartas++];
  memset(c, 0, sizeof *c);
  snprintf(c->imdb, sizeof c->imdb, "%s", it->imdb);
  snprintf(c->titulo, sizeof c->titulo, "%s", it->titulo);
  snprintf(c->meta, sizeof c->meta, "%s", it->meta);
  // So https, e curto e sem parametros: endereco de cartaz com chave embutida
  // (PostersPlus, ate 2 KB) nao vai para o celular de um visitante.
  if (!strncmp(it->poster, "https://", 8) && strlen(it->poster) < sizeof c->poster && !strchr(it->poster, '?'))
    snprintf(c->poster, sizeof c->poster, "%.*s", (int)(sizeof c->poster - 1), it->poster);
  // Cartaz local ou com chave: o endereco publico do mesmo id (metahub, o do
  // Cinemeta), que nao diz nada sobre a conta.
  else snprintf(c->poster, sizeof c->poster, "https://images.metahub.space/poster/medium/%.20s/img", it->imdb);
}

// O BARALHO: o que a Home ja mostra. Filmes primeiro, uma carta de cada
// fileira por volta (variedade), comecando num ponto sorteado; depois o resto
// do catalogo; e series so se os filmes nao encherem a rodada.
static void montarBaralho(void) {
  static const char *const TIPOS[] = { "movie", "series" };
  int nf = cat_n_fileiras(), n = cat_n(), volta, f, t, i, infantil = perfil_infantil_ativo();
  nCartas = 0;
  for (t = 0; t < 2 && nCartas < SU_CARTAS; t++) {
    for (volta = 0; volta < 40 && nCartas < SU_CARTAS; volta++)
      for (f = 0; f < nf && nCartas < SU_CARTAS; f++) {
        const CatFileira *fl = cat_fileira(f);
        if (!fl || fl->n <= volta || strstr(fl->chave, "continue_watching")) continue;
        cartaDe(cat_item(fl->ini + (volta + (int)((unsigned)rand() % (unsigned)fl->n)) % fl->n), TIPOS[t], infantil);
      }
    if (n > 0) {
      int ini = (int)((unsigned)rand() % (unsigned)n);
      for (i = 0; i < n && nCartas < SU_CARTAS; i++) cartaDe(cat_item((ini + i) % n), TIPOS[t], infantil);
    }
  }
  for (f = 0; f < nCartas && nVistos < SU_VISTOS_MAX; f++)
    memcpy(vistos[nVistos++], cartas[f].imdb, sizeof vistos[0]);
}

#include "salapagina.h"

static size_t poe(size_t k, const char *s) {
  size_t n = strlen(s);
  if (!k || k + n >= sizeof pag) return 0;
  memcpy(pag + k, s, n + 1);
  return k + n;
}
static size_t poeTexto(size_t k, const char *chave, const char *s) {
  char t[1200];
  t[0] = 0;
  if (s && s[0] && !sala_json_texto(t, sizeof t, s)) return 0;
  k = poe(k, "\""); k = poe(k, chave); k = poe(k, "\":\""); k = poe(k, t);
  return poe(k, "\"");
}

static int montarPagina(void) {
  static const char *const T[][2] = {
    { "t", "Sessão" }, { "nome", "Seu nome" }, { "entrar", "Participar" }, { "sim", "Sim" }, { "nao", "Não" },
    { "esp", "Esperando a TV começar…" }, { "outros", "Esperando os outros…" }, { "tv", "Olhe para a TV!" },
    { "fim", "A sessão terminou." }, { "de", "%d de %d" } };
  size_t k, i;
  pag[0] = 0;
  k = (size_t)snprintf(pag, sizeof pag, "%s", PAG_A);
  k = poe(k, "[");
  for (i = 0; i < (size_t)nCartas; i++) {
    k = poe(k, i ? ",{" : "{");
    k = poeTexto(k, "t", cartas[i].titulo); k = poe(k, ",");
    k = poeTexto(k, "m", cartas[i].meta); k = poe(k, ",");
    k = poeTexto(k, "p", cartas[i].poster);
    k = poe(k, "}");
  }
  k = poe(k, "],T={");
  for (i = 0; i < sizeof T / sizeof *T; i++) {
    if (i) k = poe(k, ",");
    k = poeTexto(k, T[i][0], i18n(T[i][1]));
  }
  k = poe(k, "}");
  k = poe(k, PAG_B);
  return k > 0;
}

static void publicar(void) {
  char j[2048];
  if (sala_estado_json(&sala, j, sizeof j)) celular_status(j);
}

static void novaRodada(void) {
  celular_fechar();
  montarBaralho();
  if (nCartas < 3 && nVistos) { nVistos = 0; montarBaralho(); }   // acabou o catalogo: recomeca
  semBaralho = nCartas < 1;
  sala_iniciar(&sala, nCartas);
  if (semBaralho) return;
  if (!montarPagina()) { semBaralho = 1; puts("[sessao] pagina nao coube"); return; }
  celular_sessao_opcoes(SU_VALIDADE_S, 1);
  if (celular_abrir_sessao("Nuvio", pag)) publicar();
  printf("[sessao] rodada com %d cartas%s\n", nCartas, perfil_infantil_ativo() ? " (perfil infantil)" : "");
}

void salaui_abrir(void) {
  if (aberta || !salaui_disponivel()) return;
  aberta = 1;
  pediu[0] = 0;
  novaRodada();
  puts("[sessao] aberta");
}

void salaui_fechar(void) {
  if (!aberta) return;
  aberta = 0;
  celular_fechar();
  memset(&sala, 0, sizeof sala);
  puts("[sessao] fechada");
}

int salaui_pediu_titulo(char *imdb, size_t n) {
  if (!pediu[0]) return 0;
  snprintf(imdb, n, "%s", pediu);
  pediu[0] = 0;
  return 1;
}

void salaui_atualizar(float dt) {
  char m[160];
  entrada = ajustes_animacoes_reduzidas() ? (float)aberta : anim_rampa(entrada, (float)aberta, dt, aberta ? 260 : 160);
  if (!aberta) return;
  // Um pedido por quadro: o servidor segura o proximo (409) ate este sair.
  if (celular_pegar(m, sizeof m)) {
    int antes = sala.fase, r = sala_mensagem(&sala, m);
    if (r == SALA_OK) publicar();
    // Sem o texto do visitante no log: so o que a regra respondeu.
    if (r != SALA_OK && r != SALA_IGUAL) printf("[sessao] pedido recusado (%d)\n", r);
    if (antes != sala.fase)
      printf("[sessao] fase %d, vence %d, unanime %d, %d pessoas\n", sala.fase, sala.vence, sala.unanime, sala.nGente);
    memset(m, 0, sizeof m);
  }
}

static int ehVoltar(const SDL_Event *e) {
  SDL_Keycode k = e->key.keysym.sym;
  return k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_DELETE ||
         e->key.keysym.scancode == NV_SCANCODE_BACK;
}

static void ok(void) {
  // O vencedor na tela abre mesmo com o endereco ja vencido.
  if (sala.fase != SALA_DECIDIDO && (semBaralho || !celular_url()[0])) { novaRodada(); return; }
  switch (sala.fase) {
    case SALA_ENTRANDO: if (sala_comecar(&sala)) { publicar(); puts("[sessao] votando"); } break;
    case SALA_VOTANDO:
      sala_encerrar(&sala); publicar();
      printf("[sessao] encerrada pela TV: fase %d, vence %d\n", sala.fase, sala.vence);
      break;
    case SALA_DECIDIDO:
      if (sala.vence >= 0 && sala.vence < nCartas) snprintf(pediu, sizeof pediu, "%s", cartas[sala.vence].imdb);
      salaui_fechar();
      break;
    default: novaRodada(); break;
  }
}
static void okPonteiro(int a, int b) { (void)a; (void)b; ok(); }
static void fecharPonteiro(int a, int b) { (void)a; (void)b; salaui_fechar(); }

int salaui_evento(const SDL_Event *e) {
  if (!aberta) return 0;
  if (e->type == SDL_TEXTINPUT || e->type == SDL_TEXTEDITING || e->type == SDL_KEYUP) return 1;
  if (e->type != SDL_KEYDOWN) return 0;
  if (e->key.repeat) return 1;
  if (ehVoltar(e)) { salaui_fechar(); return 1; }
  if (e->key.keysym.sym == SDLK_RETURN || e->key.keysym.sym == SDLK_KP_ENTER) ok();
  return 1;
}

// --- desenho -------------------------------------------------------------------
static float texto(TxtEstilo est, const char *s, int tom, float x, float y, float w, float lead, float a, int linhas) {
  return txt_bloco_corta(est, s, tom, tom, tom, x, y, w, lead, a, linhas);
}

static void pessoas(float x, float y, float w, float a) {
  float ar, ag, ab;
  int p;
  ajustes_acento(&ar, &ag, &ab);
  if (!sala.nGente) { texto(TXT_BODY, i18n("Ninguém entrou ainda."), 170, x, y, w, 36, a, 2); return; }
  for (p = 0; p < sala.nGente; p++) {
    float ly = y + p * 74.0f;
    gfx_cor((GfxRect){ x, ly, w, 62 }, 0.3f, 1, 1, 1, 0.07f * a);
    txt_bloco_bruto(TXT_BODY, sala.g[p].nome, 243, 242, 239, x + 24, ly + 13, w * 0.5f, 36, a, 1);
    if (sala.fase != SALA_ENTRANDO && sala.nCartas > 0) {
      char t[48];
      float f = (float)sala_votos(&sala, p) / (float)sala.nCartas, bw = w * 0.26f;
      snprintf(t, sizeof t, i18n("%d de %d"), sala_votos(&sala, p), sala.nCartas);
      texto(TXT_CAPTION2, t, 190, x + w - bw - 24 - 130, ly + 19, 120, 26, a, 1);
      gfx_cor((GfxRect){ x + w - bw - 24, ly + 26, bw, 10 }, 0.5f, 1, 1, 1, 0.16f * a);
      if (f > 0) gfx_cor((GfxRect){ x + w - bw - 24, ly + 26, bw * f, 10 }, 0.5f, ar, ag, ab, a);
    }
  }
}

void salaui_desenhar(void) {
  float a = anim_suave(entrada), W = NV_TELA_W, H = NV_TELA_H;
  float mx = W * 0.075f, y = H * 0.13f;
  const char *u;
  if (a <= .002f) return;
  gfx_sem_recorte();
  gfx_cor((GfxRect){ 0, 0, W, H }, 0, 0.035f, 0.043f, 0.06f, 0.985f * a);
  if (!aberta) return;
  ponteiro_camada();
  ponteiro_alvo(0, 0, W, H, NULL, okPonteiro, 0, 0);
  // Toque e mouse nao tem tecla Voltar: um alvo de fechar no canto.
  if (ponteiro_ativo()) {
    float fw = 150, fh = 56, fx = W - mx - fw, fy = H * 0.13f - 6;
    gfx_cor((GfxRect){ fx, fy, fw, fh }, 0.5f, 1, 1, 1, 0.12f * a);
    texto(TXT_CAPTION2, i18n("Fechar"), 243, fx + (fw - txt_largura(TXT_CAPTION2, i18n("Fechar"))) * 0.5f, fy + 15, fw, 26, a, 1);
    ponteiro_alvo(fx, fy, fw, fh, NULL, fecharPonteiro, 0, 0);
  }
  y += texto(TXT_ILHA_TITULO, i18n("Sessão"), 243, mx, y, W * 0.5f, 52, a, 1) + 18;
  u = celular_url();

  if (semBaralho) { texto(TXT_BODY, i18n("Não há títulos para escolher agora."), 200, mx, y, W * 0.6f, 38, a, 3); return; }
  if (sala.fase == SALA_DECIDIDO && sala.vence >= 0 && sala.vence < nCartas) {
    const Carta *c = &cartas[sala.vence];
    float ph = H * 0.56f, pw = ph * 2.0f / 3.0f, tx = mx;
    if (c->poster[0]) {
      GLuint t = tex_obter_larg(c->poster, pw);
      gfx_cor((GfxRect){ mx, y + 10, pw, ph }, 0.05f, 1, 1, 1, 0.06f * a);
      if (t) {
        gfx_tex_aspect_atual = tex_aspecto(c->poster);
        gfx_rect((GfxRect){ mx, y + 10, pw, ph }, t, GFX_ARTE, 0, 0, 0, 0.05f, 0, 0, 0, a);
        gfx_tex_aspect_atual = 0;
      }
      tx = mx + pw + 56;
    }
    y += 40;
    y += texto(TXT_BODY, i18n(sala.unanime ? "Todos disseram sim" : "A maioria escolheu"), 190, tx, y, W * 0.5f, 38, a, 1) + 14;
    y += txt_bloco_bruto(TXT_ILHA_TITULO, c->titulo, 243, 242, 239, tx, y, W * 0.5f, 52, a, 3) + 10;
    if (c->meta[0]) y += txt_bloco_bruto(TXT_BODY, c->meta, 190, 194, 204, tx, y, W * 0.5f, 38, a, 1);
    texto(TXT_BODY, i18n("Aperte OK para abrir."), 243, tx, y + 34, W * 0.5f, 38, a, 2);
    return;
  }
  if (sala.fase == SALA_SEM_ACORDO) {
    texto(TXT_BODY, i18n("Ninguém escolheu nada desta vez. Aperte OK para outra rodada."), 220, mx, y, W * 0.6f, 38, a, 3);
    return;
  }
  if (!u[0]) { texto(TXT_BODY, i18n("Sem rede local. Conecte a TV ao Wi-Fi e aperte OK."), 220, mx, y, W * 0.6f, 38, a, 3); return; }

  { float lado = H * 0.40f, cx = mx + lado + 72, cw = W - cx - mx, ty = y + 6;
    const char *linha = sala.fase == SALA_ENTRANDO
      ? "A TV mostra um código. Cada pessoa entra pelo celular, sem instalar nada, e diz sim ou não aos mesmos títulos. A TV abre o primeiro em que todos disseram sim."
      : "Cada um escolhe no próprio celular.";
    celb_qr_desenhar_lado(u, mx, y + 10, lado, a);
    texto(TXT_CAPTION2, i18n("Aponte a câmera para o código"), 200, mx, y + lado + 28, lado, 28, a, 2);
    texto(TXT_CAPTION2, i18n("Celular e TV na mesma rede Wi-Fi."), 150, mx, y + lado + 62, lado, 28, a, 2);
    texto(TXT_CAPTION2, u + 7, 150, mx, y + lado + 96, lado + 60, 28, a, 1);
    ty += texto(TXT_BODY, i18n(linha), 215, cx, ty, cw, 38, a, 5) + 26;
    pessoas(cx, ty, cw, a);
    ty += (sala.nGente ? sala.nGente * 74.0f : 44.0f) + 22;
    if (sala.nGente)
      texto(TXT_BODY, i18n(sala.fase == SALA_ENTRANDO ? "Aperte OK quando todos tiverem entrado."
                                                      : "Aperte OK para decidir com os votos de agora."),
            243, cx, ty, cw, 38, a, 2);
  }
}
