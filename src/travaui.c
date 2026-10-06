// Trava de serie — as telas. Ver travaui.h.
//
// O MATERIAL E O DAS ILHAS (recenviar.c, ctxmenu.c): vidro com a opacidade e o
// Frost configurados (ou o solido equivalente), linhas sem miolo em repouso e
// a superficie um degrau mais clara no foco, pilula cheia no acento so para o
// botao em foco. O MODAL CRESCE DO MEIO subindo 40 px, como os outros.
//
// REMOTE-FIRST: setas + OK + Voltar fazem tudo. Uma parada de foco por bloco
// (Pessoas, Modo, PIN, Botoes). Numeros do controle digitam o PIN; sem eles,
// CIMA/BAIXO trocam o digito e DIREITA anda.
#include "travaui.h"
#include "trava.h"
#include "travarede.h"
#include "perfis.h"
#include "vistoep.h"
#include "detail.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "idioma.h"
#include "tex_cache.h"
#include "plrui.h"
#include "recomenda.h"
#include "ilha.h"
#include "player.h"
#define NV_ESCALA_TELA_ATIVA
#include "escala.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TU_NADA = 0, TU_CRIAR, TU_GERENCIAR, TU_ESPERA };
enum { R_PESSOAS = 0, R_MODO, R_PIN, R_ACOES };
enum { PIN_NADA = 0, PIN_DIGITANDO, PIN_CONFERINDO, PIN_ERRADO, PIN_SEM_REDE };

#define TU_W        1120.0f
#define TU_PAD        48.0f
#define TU_RAIO       32.0f
#define TU_AV         84.0f
#define TU_PESSOA_W  156.0f
#define TU_PESSOA_H  200.0f
#define TU_MODO_H    150.0f
#define TU_PIN_H      84.0f
#define TU_ESP_W    1080.0f
#define TU_ESP_ARTE_W 448.0f
#define TU_ESP_ARTE_H 252.0f

static int tela;                  // TU_*
static float anim, troca = 1.0f;
static int linha, coluna;
static float focoAnim[4][8];
static int idx = -1;              // catalogo da serie
static char serie[24], titulo[160], poster[400], arte[400];
static char idTrava[TRAVA_ID_MAX];
static int sel[TRAVA_PERFIS_MAX]; // por slot de trava_perfil(i)
static int escModo = TRAVA_SEPARADOS, escPin;
static int confirmarApagar;
static char aviso[200];
// Folha de espera
static int espT, espE, espQuem[TRAVA_MEMBROS], espN, espComPin;
static int pinEstado, pinPos;
static char pin[5];
// O pedido de PIN no fio
static SDL_mutex *mtx;
static volatile int pinResposta = -2;   // -2 nada, -1 sem rede, 0 errado, 1 ok
static int pinFioVivo;
// Episodios liberados por "Assistir mesmo assim" nesta sessao.
static struct { char serie[24]; int t, e; } liberados[16];
static int nLiberados;
static int alcancouPerfis[TRAVA_MEMBROS], nAlcancou, alcT, alcE;
static char alcSerie[24];

static void serieDe(const char *s, char *d, size_t tam) {
  size_t k = 0;
  d[0] = 0;
  if (!s) return;
  while (s[k] && s[k] != ':' && k + 1 < tam) { d[k] = s[k]; k++; }
  d[k] = 0;
}

static int teclaOk(SDL_Keycode k) { return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE; }
static int teclaVolta(const SDL_Event *e) {
  SDL_Keycode k = e->key.keysym.sym;
  return k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE ||
         e->key.keysym.scancode == (SDL_Scancode)NV_SCANCODE_BACK;
}

// ================================================================ dados

int travaui_disponivel(void) { return trava_n_perfis() >= 2; }

int travaui_tem_trava(const char *imdb) {
  return trava_da_serie(imdb, trava_perfil_ativo(), NULL, 0) > 0;
}

static int nEpsCat(void *ctx, int temporada) {
  int i, n, k = 0, ic = (int)(intptr_t)ctx;
  if (ic < 0) return 0;
  n = cat_n_episodios(ic);
  for (i = 0; i < n; i++) {
    const CatEp *ep = cat_episodio(ic, i);
    if (ep && ep->temporada == temporada && ep->episodio > k) k = ep->episodio;
  }
  return k;
}

static int liberado(const char *s, int t, int e) {
  int i;
  for (i = 0; i < nLiberados; i++)
    if (liberados[i].t == t && liberados[i].e == e && !strcmp(liberados[i].serie, s)) return 1;
  return 0;
}
static void liberar(const char *s, int t, int e) {
  if (liberado(s, t, e)) return;
  if (nLiberados == 16) { memmove(&liberados[0], &liberados[1], sizeof liberados[0] * 15); nLiberados--; }
  snprintf(liberados[nLiberados].serie, sizeof liberados[0].serie, "%s", s);
  liberados[nLiberados].t = t; liberados[nLiberados].e = e;
  nLiberados++;
}

int travaui_segura(const char *imdb, int idxCat, int t, int e, int *perfis, int *n) {
  char s[24];
  int ix[TRAVA_MAX], ni, i, k, tot = 0, ativo = trava_perfil_ativo();
  if (n) *n = 0;
  serieDe(imdb, s, sizeof s);
  if (!s[0] || t <= 0 || e <= 0) return 0;
  ni = trava_da_serie(s, ativo, ix, TRAVA_MAX);
  if (!ni || liberado(s, t, e)) return 0;
  // O LIMITE EFETIVO e o mais restritivo entre as travas da serie (plano 5.5).
  for (i = 0; i < ni; i++) {
    const Trava *tv = trava_i(ix[i]);
    int quem[TRAVA_MEMBROS], nq = 0, eu = trava_membro_do_perfil(tv, ativo);
    if (trava_liberado(tv, eu, t, e, vistoep_estado(s, t, e) == 1, nEpsCat,
                       (void *)(intptr_t)idxCat, quem, &nq)) continue;
    for (k = 0; k < nq; k++) {
      int p = tv->m[quem[k]].perfil, j, dup = 0;
      for (j = 0; perfis && j < tot; j++) if (perfis[j] == p) dup = 1;
      if (dup) continue;
      if (perfis && tot < TRAVA_MEMBROS) perfis[tot] = p;
      tot++;
    }
    if (!perfis && !tot) tot = 1;
  }
  if (n) *n = tot < TRAVA_MEMBROS ? tot : TRAVA_MEMBROS;
  return tot > 0;
}

static const char *nomeDoPerfil(int indice) {
  const TravaPerfil *p = trava_perfil_indice(indice);
  return p && p->nome[0] ? p->nome : i18n("Perfil");
}
static const char *nomeIdx(void *ctx, int i) {
  const int *v = (const int *)ctx;
  return nomeDoPerfil(v[i]);
}

void travaui_esperando(const int *perfis, int n, char *dst, size_t tam) {
  int ord[TRAVA_MEMBROS], i;
  for (i = 0; i < n && i < TRAVA_MEMBROS; i++) ord[i] = i;
  trava_esperando_texto(ord, n, nomeIdx, (void *)perfis, dst, (unsigned)tam);
}

static void rostoPerfil(GfxRect r, int indice, float a) {
  const TravaPerfil *p = trava_perfil_indice(indice);
  char id[24];
  snprintf(id, sizeof id, "perfil:%d", indice);
  rec_avatar_estilo(r, p ? p->avatar : "", p ? p->nome : "?", id, a,
                    r.w >= 70.0f ? -1 : TXT_ILHA_INICIAL);
}

void travaui_rostos(float x, float y, float d, const int *perfis, int n, float a) {
  int i;
  for (i = n - 1; i >= 0; i--) {
    GfxRect r = { x + (float)i * d * 0.72f, y, d, d };
    // O aro escuro separa um rosto do outro na sobreposicao.
    gfx_cor((GfxRect){ r.x - 3.0f, r.y - 3.0f, d + 6.0f, d + 6.0f }, 0.5f, .055f, .059f, .071f, a);
    rostoPerfil(r, perfis[i], a);
  }
}

// ================================================================ abrir

static void lerSerie(const CatItem *ci) {
  serieDe(ci ? ci->imdb : "", serie, sizeof serie);
  snprintf(titulo, sizeof titulo, "%s", ci ? ci->titulo : "");
  snprintf(poster, sizeof poster, "%s", ci ? ci->poster : "");
  snprintf(arte, sizeof arte, "%s", ci && ci->backdrop[0] ? ci->backdrop : (ci ? ci->poster : ""));
}

static void comecar(int qual) {
  tela = qual;
  linha = R_PESSOAS; coluna = 0;
  troca = 0.0f;
  confirmarApagar = 0;
  aviso[0] = 0;
  memset(focoAnim, 0, sizeof focoAnim);
}

static int slotDoAtivo(void) {
  int i;
  for (i = 0; i < trava_n_perfis(); i++)
    if (trava_perfil(i)->indice == trava_perfil_ativo()) return i;
  return 0;
}

static void carregarDaTrava(const Trava *t) {
  int i;
  memset(sel, 0, sizeof sel);
  for (i = 0; i < trava_n_perfis(); i++)
    sel[i] = trava_membro_do_perfil(t, trava_perfil(i)->indice) >= 0 &&
             t->m[trava_membro_do_perfil(t, trava_perfil(i)->indice)].estado == TRAVA_DENTRO;
  escModo = t->modo;
  escPin = t->pin;
  snprintf(idTrava, sizeof idTrava, "%s", t->id);
  snprintf(serie, sizeof serie, "%s", t->serie);
  snprintf(titulo, sizeof titulo, "%s", t->titulo);
  if (t->poster[0]) snprintf(poster, sizeof poster, "%s", t->poster);
}

int travaui_abrir(const CatItem *ci, int idxCat) {
  int ix[1];
  if (!ci || !travaui_disponivel()) return 0;
  lerSerie(ci);
  if (!serie[0]) return 0;
  idx = idxCat;
  travarede_posicao_local(serie);
  if (trava_da_serie(serie, trava_perfil_ativo(), ix, 1)) {
    comecar(TU_GERENCIAR);
    carregarDaTrava(trava_i(ix[0]));
    if (arte[0] == 0) snprintf(arte, sizeof arte, "%s", poster);
  } else {
    comecar(TU_CRIAR);
    memset(sel, 0, sizeof sel);
    idTrava[0] = 0;
    escModo = TRAVA_SEPARADOS;
    escPin = 0;
  }
  sel[slotDoAtivo()] = 1;
  travarede_atualizar_ja();
  printf("[trava] modal %s: %s\n", tela == TU_CRIAR ? "travar" : "gerenciar", serie);
  return 1;
}

int travaui_abrir_gerenciar(const char *id) {
  const Trava *t = trava_por_id(id);
  if (!t) return 0;
  idx = cat_indice_por_imdb(t->serie);
  if (idx >= 0) lerSerie(cat_item(idx));
  else { arte[0] = 0; poster[0] = 0; }
  comecar(TU_GERENCIAR);
  carregarDaTrava(t);
  if (!arte[0]) snprintf(arte, sizeof arte, "%s", t->poster);
  sel[slotDoAtivo()] = 1;
  travarede_atualizar_ja();
  return 1;
}

static int algumComPin(const int *perfis, int n) {
  int i;
  for (i = 0; i < n; i++) {
    const TravaPerfil *p = trava_perfil_indice(perfis[i]);
    if (p && p->temPin) return 1;
  }
  return 0;
}

// Os outros membros (de todas as travas da serie) que TEM PIN: qualquer um
// deles vale para "Assistir mesmo assim" (decisao do dono).
static int membrosComPin(int *out, int max) {
  int ix[TRAVA_MAX], ni = trava_da_serie(serie, trava_perfil_ativo(), ix, TRAVA_MAX), i, k, n = 0;
  for (i = 0; i < ni; i++) {
    const Trava *t = trava_i(ix[i]);
    if (!t->pin) continue;
    for (k = 0; k < t->n; k++) {
      int p = t->m[k].perfil, j, dup = 0;
      const TravaPerfil *tp = trava_perfil_indice(p);
      if (p == trava_perfil_ativo() || !tp || !tp->temPin || t->m[k].estado != TRAVA_DENTRO) continue;
      for (j = 0; j < n; j++) if (out[j] == p) dup = 1;
      if (!dup && n < max) out[n++] = p;
    }
  }
  return n;
}

int travaui_abrir_espera(int idxCat, int t, int e) {
  const CatItem *ci = cat_item(idxCat);
  int com[TRAVA_MEMBROS];
  if (!ci) return 0;
  lerSerie(ci);
  idx = idxCat;
  if (!travaui_segura(serie, idxCat, t, e, espQuem, &espN)) return 0;
  espT = t; espE = e;
  espComPin = membrosComPin(com, TRAVA_MEMBROS) > 0;
  comecar(TU_ESPERA);
  linha = R_ACOES; coluna = 0;
  pinEstado = PIN_NADA;
  { int ix[1];
    if (trava_da_serie(serie, trava_perfil_ativo(), ix, 1)) snprintf(idTrava, sizeof idTrava, "%s", trava_i(ix[0])->id); }
  printf("[trava] espera: %s T%dE%d (%d membros faltam)\n", serie, t, e, espN);
  return 1;
}

int travaui_segurar_detalhe(void) {
  int t = 0, e = 0, i = detail_aberto() ? detail_indice() : -1;
  const CatItem *ci = cat_item(i);
  if (i < 0 || !ci || strcmp(ci->tipo, "series")) return 0;
  if (!detail_ep_foco(&t, &e) || t <= 0 || e <= 0) return 0;
  if (!travaui_segura(ci->imdb, i, t, e, NULL, NULL)) return 0;
  return travaui_abrir_espera(i, t, e);
}

int travaui_proximo_segura(const char *imdb, int idxCat, int t, int e) {
  int quem[TRAVA_MEMBROS], n = 0;
  if (!travaui_segura(imdb, idxCat, t, e, quem, &n)) return 0;
  memcpy(alcancouPerfis, quem, sizeof(int) * (size_t)n);
  nAlcancou = n; alcT = t; alcE = e;
  serieDe(imdb, alcSerie, sizeof alcSerie);
  return 1;
}

int travaui_aberta(void) { return tela != TU_NADA; }

static void fechar(void) {
  tela = TU_NADA;
  pinEstado = PIN_NADA;
}

// ================================================================ acoes

static int nPerfisSel(int *out) {
  int i, n = 0;
  for (i = 0; i < trava_n_perfis(); i++)
    if (sel[i] && trava_perfil(i)->indice != trava_perfil_ativo()) out[n++] = trava_perfil(i)->indice;
  return n;
}

static int pinVisivel(void) {
  int ps[TRAVA_PERFIS_MAX], n = nPerfisSel(ps);
  return algumComPin(ps, n);
}

static int nAcoes(void) {
  if (tela == TU_CRIAR) return 2;              // Travar série, Cancelar
  if (tela == TU_GERENCIAR) return 4;          // Salvar, Pausar, Desfazer, Cancelar
  if (tela == TU_ESPERA) return 3;             // Assistir mesmo assim, Gerenciar, Voltar
  return 0;
}

static void confirmar(void) {
  int ps[TRAVA_PERFIS_MAX], n = nPerfisSel(ps), pinOk = pinVisivel() && escPin;
  if (n < 1) { snprintf(aviso, sizeof aviso, "%s", i18n("Escolha pelo menos mais uma pessoa.")); return; }
  if (tela == TU_CRIAR) {
    int k = trava_criar(serie, titulo, poster, escModo, pinOk, ps, n);
    if (k < 0) { snprintf(aviso, sizeof aviso, "%s", i18n("Não deu para travar esta série.")); return; }
    travarede_posicao_local(serie);
    travarede_atualizar_ja();
    { char frase[200], nomes[160], forte[96];
      if (n == 1) snprintf(nomes, sizeof nomes, "%s", nomeDoPerfil(ps[0]));
      else if (n == 2) snprintf(nomes, sizeof nomes, i18n("%s e %s"), nomeDoPerfil(ps[0]), nomeDoPerfil(ps[1]));
      else snprintf(nomes, sizeof nomes, i18n("%s e mais %d"), nomeDoPerfil(ps[0]), n - 1);
      snprintf(frase, sizeof frase, i18n("%s travada com %s"), ilha_forte(forte, sizeof forte, titulo), nomes);
      ilha_avisar("trava:criada", ILHA_ACENTO, "aj_lock", frase, 6000u, 0); }
  } else {
    trava_definir_membros(idTrava, ps, n);
    trava_definir_modo(idTrava, escModo);
    trava_definir_pin(idTrava, pinOk);
    ilha_avisar("trava:salva", ILHA_OK, "aj_lock", i18n("Trava atualizada"), 4000u, 0);
  }
  fechar();
}

static int pinFio(void *u) {
  int i, r = 0, semRede = 0, *v = (int *)u;
  char p[5];
  SDL_LockMutex(mtx);
  snprintf(p, sizeof p, "%s", pin);
  SDL_UnlockMutex(mtx);
  // O PIN DE QUALQUER OUTRO MEMBRO que tenha PIN (decisao do dono): tenta um
  // por um. perfis_verificar_pin BLOQUEIA e precisa de rede; por isso aqui.
  for (i = 1; i <= v[0] && r != 1; i++) {
    int x = perfis_verificar_pin(v[i], p);
    if (x == 1) r = 1;
    else if (x < 0) semRede = 1;
  }
  free(v);
  SDL_LockMutex(mtx);
  pinResposta = r == 1 ? 1 : semRede ? -1 : 0;
  pinFioVivo = 0;
  SDL_UnlockMutex(mtx);
  return 0;
}

static void conferirPin(void) {
  int com[TRAVA_MEMBROS], n = membrosComPin(com, TRAVA_MEMBROS), *v;
  SDL_Thread *th;
  if (!mtx) mtx = SDL_CreateMutex();
  if (!mtx || pinFioVivo || n < 1) return;
  v = (int *)malloc(sizeof(int) * (size_t)(n + 1));
  if (!v) return;
  v[0] = n;
  memcpy(v + 1, com, sizeof(int) * (size_t)n);
  pinEstado = PIN_CONFERINDO;
  pinResposta = -2;
  pinFioVivo = 1;
  th = SDL_CreateThread(pinFio, "nv-trava-pin", v);
  if (th) SDL_DetachThread(th);
  else { pinFioVivo = 0; free(v); pinEstado = PIN_SEM_REDE; }
}

static void assistirMesmoAssim(void) {
  liberar(serie, espT, espE);
  printf("[trava] assistir mesmo assim: %s T%dE%d\n", serie, espT, espE);
  fechar();
  // A pagina do titulo esta com o foco no mesmo episodio (ou no botao que o
  // aponta): o pedido de reproduzir passa agora pelo gate liberado.
  if (detail_aberto()) detail_pedir_reproduzir();
}

static void acao(int k) {
  if (tela == TU_CRIAR) {
    if (k == 0) confirmar(); else fechar();
  } else if (tela == TU_GERENCIAR) {
    const Trava *t = trava_por_id(idTrava);
    if (k == 0) confirmar();
    else if (k == 1 && t) {
      trava_definir_pausada(idTrava, !t->pausada);
      ilha_avisar("trava:pausa", ILHA_INFO, t->pausada ? "aj_pause" : "aj_play",
                  t->pausada ? i18n("Trava pausada") : i18n("Trava retomada"), 4000u, 0);
    } else if (k == 2) {
      if (!confirmarApagar) { confirmarApagar = 1; return; }
      trava_apagar(idTrava);
      ilha_avisar("trava:apagada", ILHA_INFO, "aj_lock-open", i18n("Trava desfeita"), 4000u, 0);
      fechar();
    } else fechar();
  } else if (tela == TU_ESPERA) {
    if (k == 0) {
      if (espComPin) { pinEstado = PIN_DIGITANDO; pinPos = 0; memset(pin, 0, sizeof pin); }
      else assistirMesmoAssim();
    } else if (k == 1) {
      char id[TRAVA_ID_MAX];
      snprintf(id, sizeof id, "%s", idTrava);
      fechar();
      travaui_abrir_gerenciar(id);
    } else fechar();
  }
}

static void eventoPin(SDL_Keycode k) {
  if (pinEstado == PIN_CONFERINDO) return;
  if (pinEstado == PIN_ERRADO || pinEstado == PIN_SEM_REDE) {
    pinEstado = PIN_DIGITANDO; pinPos = 0; memset(pin, 0, sizeof pin);
  }
  if (k >= SDLK_0 && k <= SDLK_9) {
    if (pinPos < 4) pin[pinPos++] = (char)('0' + (k - SDLK_0));
  } else if (k >= SDLK_KP_1 && k <= SDLK_KP_0) {
    if (pinPos < 4) pin[pinPos++] = (char)(k == SDLK_KP_0 ? '0' : '1' + (k - SDLK_KP_1));
  } else if (k == SDLK_UP || k == SDLK_DOWN) {
    int p = pinPos < 4 ? pinPos : 3;
    int d = pin[p] ? pin[p] - '0' : (k == SDLK_UP ? 9 : 1);
    d = (d + (k == SDLK_UP ? 1 : 9)) % 10;
    pin[p] = (char)('0' + d);
  } else if (k == SDLK_RIGHT) {
    if (pinPos < 4 && pin[pinPos]) pinPos++;
  } else if (k == SDLK_LEFT) {
    if (pinPos > 0) { pinPos--; pin[pinPos + 1] = 0; }
  }
  if (pinPos >= 4 || (teclaOk(k) && strlen(pin) == 4)) conferirPin();
}

void travaui_evento(const SDL_Event *e) {
  SDL_Keycode k;
  int i, n;
  if (!tela || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (tela == TU_ESPERA && pinEstado != PIN_NADA) {
    if (teclaVolta(e)) { pinEstado = PIN_NADA; return; }
    eventoPin(k);
    return;
  }
  if (teclaVolta(e)) { fechar(); return; }
  if (k != SDLK_RETURN && k != SDLK_KP_ENTER) confirmarApagar = 0;
  aviso[0] = 0;
  if (tela == TU_ESPERA) {
    if (k == SDLK_LEFT && coluna > 0) coluna--;
    else if (k == SDLK_RIGHT && coluna + 1 < nAcoes()) coluna++;
    else if (teclaOk(k)) acao(coluna);
    return;
  }
  if (k == SDLK_UP) {
    do { linha--; } while (linha == R_PIN && !pinVisivel());
    if (linha < R_PESSOAS) linha = R_PESSOAS;
    coluna = linha == R_MODO ? (escModo == TRAVA_JUNTOS ? 0 : 1) : 0;
    return;
  }
  if (k == SDLK_DOWN) {
    do { linha++; } while (linha == R_PIN && !pinVisivel());
    if (linha > R_ACOES) linha = R_ACOES;
    coluna = linha == R_MODO ? (escModo == TRAVA_JUNTOS ? 0 : 1) : 0;
    return;
  }
  n = linha == R_PESSOAS ? trava_n_perfis() : linha == R_MODO ? 2 : linha == R_ACOES ? nAcoes() : 1;
  if (k == SDLK_LEFT && coluna > 0) { coluna--; return; }
  if (k == SDLK_RIGHT && coluna + 1 < n) { coluna++; return; }
  if (!teclaOk(k)) return;
  switch (linha) {
    case R_PESSOAS:
      if (coluna == slotDoAtivo()) break;     // "Você" fica sempre
      sel[coluna] = !sel[coluna];
      i = 0;
      { int ps[TRAVA_PERFIS_MAX]; i = nPerfisSel(ps); }
      if (i + 1 > TRAVA_MEMBROS) sel[coluna] = 0;
      break;
    case R_MODO: escModo = coluna == 0 ? TRAVA_JUNTOS : TRAVA_SEPARADOS; break;
    case R_PIN:  escPin = !escPin; break;
    case R_ACOES: acao(coluna); break;
  }
}

void travaui_atualizar(float dt, Uint32 agora) {
  int r, c;
  static Uint32 posLocalEm;
  (void)agora;
  anim = anim_mola(anim, tela ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  troca = anim_mola(troca, 1.0f, dt, NV_MOLA_TELA);
  if (ajustes_animacoes_reduzidas()) { anim = tela ? 1.0f : 0.0f; troca = 1.0f; }
  for (r = 0; r < 4; r++)
    for (c = 0; c < 8; c++)
      focoAnim[r][c] = anim_mola(focoAnim[r][c], tela && r == linha && c == coluna ? 1.0f : 0.0f,
                                 dt, NV_MOLA_FOCO);
  if (pinEstado == PIN_CONFERINDO && mtx) {
    int v;
    SDL_LockMutex(mtx); v = pinResposta; SDL_UnlockMutex(mtx);
    if (v == 1) assistirMesmoAssim();
    else if (v == 0) { pinEstado = PIN_ERRADO; printf("[trava] PIN recusado\n"); }
    else if (v == -1) pinEstado = PIN_SEM_REDE;
  }
  // A posicao do perfil ativo na serie aberta, pelo que o vistoep ja sabe
  // (a leitura do Trakt chega depois de a pagina abrir). Barato a cada 2 s.
  if (detail_aberto() && (int)(agora - posLocalEm) >= 0) {
    const CatItem *ci = cat_item(detail_indice());
    posLocalEm = agora + 2000u;
    if (ci && !strcmp(ci->tipo, "series")) travarede_posicao_local(ci->imdb);
  }
  // "Voce alcancou Jerry. O T1E5 espera por vocês." — na volta do player.
  if (nAlcancou > 0 && !player_aberto()) {
    char nomes[160], frase[260], ep[32], forte[96];
    if (nAlcancou == 1) snprintf(nomes, sizeof nomes, "%s", nomeDoPerfil(alcancouPerfis[0]));
    else if (nAlcancou == 2) snprintf(nomes, sizeof nomes, i18n("%s e %s"),
                                      nomeDoPerfil(alcancouPerfis[0]), nomeDoPerfil(alcancouPerfis[1]));
    else snprintf(nomes, sizeof nomes, i18n("%s e mais %d"), nomeDoPerfil(alcancouPerfis[0]), nAlcancou - 1);
    trava_ep_texto(alcT, alcE, ep, sizeof ep);
    snprintf(frase, sizeof frase, i18n("Você alcançou %s. O %s espera por vocês."),
             ilha_forte(forte, sizeof forte, nomes), ep);
    ilha_avisar("trava:alcancou", ILHA_ACENTO, "aj_lock", frase, 9000u, 0);
    printf("[trava] fim do episodio: %s T%dE%d segurado\n", alcSerie, alcT, alcE);
    nAlcancou = 0;
  }
}

// ================================================================ desenho

static void material(GfxRect p, float a) {
  float raio = TU_RAIO / (p.h > 1.0f ? p.h : 1.0f);
  if (ajustes_vidro()) {
    gfx_rect((GfxRect){ p.x - 18.0f, p.y - 8.0f, p.w + 36.0f, p.h + 40.0f },
             0, GFX_SOMBRA, 1.0f, 0, 0, .5f, 0, 0, 0, .38f * a);
    gfx_vidro_folha(p, raio, a);
    gfx_luz_canto(p, raio, p.w * .22f, -p.h * .40f, p.w * .62f, 1, 1, 1, .06f * a);
  } else plrui_material(p, TU_RAIO, 1, a);
}

static void acentoRGB(float *r, float *g, float *b) { ajustes_acento(r, g, b); }

static void selo(float x, float y, const char *rot, float a) {
  TxtLinha l = txt_linha(TXT_MINI, rot, 245, 199, 77, 255);
  GfxRect s = { x, y, (float)l.w + 18.0f, 26.0f };
  gfx_cor(s, .5f, .961f, .780f, .302f, .14f * a);
  gfx_anel(s, .5f, 1.5f, .961f, .780f, .302f, .55f * a);
  txt_desenhar_alpha(l, x + 9.0f, y + (26.0f - (float)l.h) * .5f, a);
}

// A posicao conhecida do perfil nesta serie (das travas que a TV ja tem).
static int posicaoConhecida(int perfil, int *t, int *e) {
  int i, k, achou = 0;
  *t = *e = 0;
  for (i = 0; i < trava_n(); i++) {
    const Trava *tv = trava_i(i);
    if (strcmp(tv->serie, serie)) continue;
    k = trava_membro_do_perfil(tv, perfil);
    if (k < 0) continue;
    achou = 1;
    if (trava_ep_cmp(tv->m[k].t, tv->m[k].e, *t, *e) > 0) { *t = tv->m[k].t; *e = tv->m[k].e; }
  }
  if (perfil == trava_perfil_ativo()) {
    VistoPar v[256];
    int n = vistoep_ate_aqui(serie, 999, 9999, v, 256), j;
    for (j = 0; j < n; j++)
      if (v[j].temporada > 0 && vistoep_estado(serie, v[j].temporada, v[j].episodio) == 1 &&
          trava_ep_cmp(v[j].temporada, v[j].episodio, *t, *e) > 0) { *t = v[j].temporada; *e = v[j].episodio; }
    achou = 1;
  }
  return achou;
}

static void desenhaPessoa(float x, float y, int slot, float a) {
  const TravaPerfil *p = trava_perfil(slot);
  float f = focoAnim[R_PESSOAS][slot < 8 ? slot : 7];
  int eu = p && p->indice == trava_perfil_ativo(), t, e;
  GfxRect fundo = { x, y, TU_PESSOA_W, TU_PESSOA_H };
  GfxRect av = { x + (TU_PESSOA_W - TU_AV) * .5f, y + 14.0f, TU_AV, TU_AV };
  char sub[48];
  float ar, ag, ab;
  if (!p) return;
  acentoRGB(&ar, &ag, &ab);
  plrui_linha_foco(fundo, 22.0f, f * a);
  rostoPerfil(av, p->indice, (sel[slot] ? 1.0f : .55f) * a);
  if (sel[slot]) {
    gfx_anel_fora(av, .5f, 4.0f, 3.0f, ar, ag, ab, a);
    { GfxRect ck = { av.x + TU_AV - 26.0f, av.y + TU_AV - 26.0f, 30.0f, 30.0f };
      gfx_cor(ck, .5f, ar, ag, ab, a);
      { float k = plrui_tinta() / 255.0f;
        gfx_icone((GfxRect){ ck.x + 6.0f, ck.y + 6.0f, 18.0f, 18.0f }, "aj_check", k, k, k, a); } }
  }
  { TxtLinha l = txt_linha_corta(TXT_ILHA_NOME, p->nome, 243, 242, 239, 255, TU_PESSOA_W - 16.0f);
    txt_desenhar_alpha(l, x + (TU_PESSOA_W - (float)l.w) * .5f, av.y + TU_AV + 12.0f,
                       (sel[slot] ? 1.0f : .66f + .34f * f) * a); }
  sub[0] = 0;
  if (eu) snprintf(sub, sizeof sub, "%s", i18n("Você"));
  if (posicaoConhecida(p->indice, &t, &e)) {
    char ep[24];
    if (t > 0) trava_ep_texto(t, e, ep, sizeof ep);
    else snprintf(ep, sizeof ep, "%s", i18n("não começou"));
    if (eu && t > 0) { size_t L = strlen(sub); snprintf(sub + L, sizeof sub - L, " · %s", ep); }
    else if (eu) {}
    else snprintf(sub, sizeof sub, "%s", ep);
  }
  if (sub[0]) {
    TxtLinha l = txt_linha_corta(TXT_ILHA_SUB, sub, 243, 242, 239, 255, TU_PESSOA_W - 12.0f);
    txt_desenhar_alpha(l, x + (TU_PESSOA_W - (float)l.w) * .5f, av.y + TU_AV + 44.0f, .62f * a);
  }
  if (!p->temPin) {
    float w = (float)txt_largura(TXT_MINI, i18n("SEM PIN")) + 18.0f;
    selo(x + (TU_PESSOA_W - w) * .5f, y + TU_PESSOA_H - 34.0f, "SEM PIN", a);
  }
}

static void desenhaModo(float x, float y, float w, int qual, float a) {
  float f = focoAnim[R_MODO][qual];
  int escolhido = (qual == 0) == (escModo == TRAVA_JUNTOS);
  GfxRect r = { x, y, w, TU_MODO_H };
  float ar, ag, ab;
  const char *nome = qual == 0 ? "Juntos" : "Separados";
  const char *txt = qual == 0
    ? "Vocês assistem juntos. Um episódio que conta como visto é marcado para todos, não importa qual perfil estava na TV."
    : "Cada um assiste no próprio perfil. Quem está à frente espera os outros.";
  acentoRGB(&ar, &ag, &ab);
  if (ajustes_vidro()) gfx_cor(r, 22.0f / TU_MODO_H, 1, 1, 1, (escolhido ? .10f : .05f) * a);
  else gfx_cor(r, 22.0f / TU_MODO_H, .141f, .149f, .173f, a);
  plrui_linha_foco(r, 22.0f, f * a);
  if (escolhido) gfx_anel(r, 22.0f / TU_MODO_H, 2.5f, ar, ag, ab, a);
  gfx_icone((GfxRect){ x + 24.0f, y + 24.0f, 30.0f, 30.0f }, qual == 0 ? "aj_users" : "aj_lock",
            escolhido ? ar : .95f, escolhido ? ag : .95f, escolhido ? ab : .95f, a);
  txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_NOME, nome, 243, 242, 239, 255, w - 100.0f),
                     x + 68.0f, y + 24.0f, a);
  txt_bloco_corta(TXT_ILHA_SUB, txt, 243, 242, 239, x + 24.0f, y + 66.0f, w - 48.0f, 26.0f,
                  (.58f + .2f * f) * a, 3);
}

static void desenhaChave(float x, float y, float w, float a) {
  float f = focoAnim[R_PIN][0];
  GfxRect r = { x, y, w, TU_PIN_H };
  GfxRect trilho = { x + w - 24.0f - 64.0f, y + (TU_PIN_H - 36.0f) * .5f, 64.0f, 36.0f };
  float ar, ag, ab;
  acentoRGB(&ar, &ag, &ab);
  plrui_linha_foco(r, 22.0f, f * a);
  gfx_icone((GfxRect){ x + 24.0f, y + (TU_PIN_H - 26.0f) * .5f, 26.0f, 26.0f }, "aj_key-round", .95f, .95f, .95f, a);
  txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_NOME, "Pedir PIN para Assistir mesmo assim", 243, 242, 239, 255, w - 200.0f),
                     x + 66.0f, y + 12.0f, a);
  txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_SUB, "Vale o PIN de qualquer outro membro que tenha PIN.", 243, 242, 239, 255, w - 200.0f),
                     x + 66.0f, y + 46.0f, .55f * a);
  if (escPin) gfx_cor(trilho, .5f, ar, ag, ab, a);
  else gfx_cor(trilho, .5f, 1, 1, 1, .16f * a);
  { float d = 28.0f, cx = escPin ? trilho.x + trilho.w - d - 4.0f : trilho.x + 4.0f;
    gfx_cor((GfxRect){ cx, trilho.y + 4.0f, d, d }, .5f, 1, 1, 1, a); }
}

// A trava "como ficaria": a escolha do modal com as posicoes que a TV conhece.
static void travaDaEscolha(Trava *t, int *eu, int *ordem) {
  int i;
  memset(t, 0, sizeof *t);
  t->modo = escModo;
  *eu = 0;
  for (i = 0; i < trava_n_perfis() && t->n < TRAVA_MEMBROS; i++) {
    int te, ee;
    if (!sel[i]) continue;
    t->m[t->n].perfil = trava_perfil(i)->indice;
    t->m[t->n].estado = TRAVA_DENTRO;
    posicaoConhecida(t->m[t->n].perfil, &te, &ee);
    t->m[t->n].t = te; t->m[t->n].e = ee;
    if (t->m[t->n].perfil == trava_perfil_ativo()) *eu = t->n;
    ordem[t->n] = t->m[t->n].perfil;
    t->n++;
  }
  if (tela == TU_GERENCIAR) { const Trava *v = trava_por_id(idTrava); if (v) t->pausada = v->pausada; }
}

static const char *rotuloAcao(int k) {
  if (tela == TU_CRIAR) return k == 0 ? "Travar série" : "Cancelar";
  if (tela == TU_GERENCIAR) {
    const Trava *t = trava_por_id(idTrava);
    if (k == 0) return "Salvar";
    if (k == 1) return t && t->pausada ? "Retomar" : "Pausar";
    if (k == 2) return confirmarApagar ? "OK de novo desfaz" : "Desfazer trava";
    return "Cancelar";
  }
  if (k == 0) return "Assistir mesmo assim";
  if (k == 1) return "Gerenciar trava";
  return "Voltar";
}
static const char *iconeAcao(int k) {
  if (tela == TU_CRIAR) return k == 0 ? "aj_lock" : NULL;
  if (tela == TU_GERENCIAR) {
    const Trava *t = trava_por_id(idTrava);
    if (k == 0) return "aj_check";
    if (k == 1) return t && t->pausada ? "aj_play" : "aj_pause";
    if (k == 2) return "aj_lock-open";
    return NULL;
  }
  if (k == 0) return espComPin ? "aj_key-round" : "aj_play";
  if (k == 1) return "aj_settings-2";
  return NULL;
}

static float desenhaAcoes(float x, float y, float a) {
  int k;
  for (k = 0; k < nAcoes(); k++)
    x += plrui_botao(x, y, rotuloAcao(k), iconeAcao(k),
                     linha == R_ACOES && coluna == k ? 1.0f : 0.0f, a) + 14.0f;
  return x;
}

static float alturaModal(void) {
  float h = TU_PAD + 120.0f + 34.0f + TU_PESSOA_H + 30.0f + 34.0f + TU_MODO_H + 24.0f;
  if (pinVisivel()) h += TU_PIN_H + 16.0f;
  h += 56.0f + 60.0f + 28.0f + 34.0f + TU_PAD;
  return h;
}

static void desenhaModal(float a, Uint32 agora) {
  float h = alturaModal(), x, y, cx = NV_TELA_W * .5f, cy = NV_TELA_H * .5f + (1.0f - a) * 40.0f;
  float lx, ly, iw = TU_W - 2.0f * TU_PAD;
  int i, n = trava_n_perfis();
  (void)agora;
  x = cx - TU_W * .5f; y = cy - h * .5f;
  material((GfxRect){ x, y, TU_W, h }, a);
  lx = x + TU_PAD; ly = y + TU_PAD;
  // CABECALHO: a capa pequena e o titulo.
  { GLuint tx = poster[0] ? tex_obter_larg(poster, 80.0f) : 0;
    GfxRect cap = { lx, ly, 80.0f, 120.0f };
    if (tx) { gfx_tex_aspect_atual = tex_aspecto(poster);
              gfx_rect(cap, tx, GFX_CARD, 0, 0, 0, 12.0f / 120.0f, 0, 0, 0, a);
              gfx_tex_aspect_atual = 0.0f; }
    else gfx_cor(cap, 12.0f / 120.0f, .133f, .133f, .133f, a);
    plrui_kicker(tela == TU_CRIAR ? "Trava de série" : "Gerenciar trava", lx + 104.0f, ly + 8.0f,
                 243, 242, 239, .45f * a);
    txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_TITULO, tela == TU_CRIAR ? "Travar série com…" : titulo,
                                       243, 242, 239, 255, iw - 104.0f), lx + 104.0f, ly + 34.0f, a);
    if (tela == TU_CRIAR)
      txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_CORPO, titulo, 243, 242, 239, 255, iw - 104.0f),
                         lx + 104.0f, ly + 84.0f, .62f * a); }
  ly += 120.0f + 24.0f;
  // PESSOAS
  plrui_kicker("Pessoas", lx, ly, 243, 242, 239, .45f * a);
  ly += 34.0f;
  for (i = 0; i < n && i < 7; i++) desenhaPessoa(lx + (float)i * (TU_PESSOA_W + 12.0f), ly, i, a);
  ly += TU_PESSOA_H + 30.0f;
  // MODO
  plrui_kicker("Modo", lx, ly, 243, 242, 239, .45f * a);
  ly += 34.0f;
  { float w = (iw - 20.0f) * .5f;
    desenhaModo(lx, ly, w, 0, a);
    desenhaModo(lx + w + 20.0f, ly, w, 1, a); }
  ly += TU_MODO_H + 24.0f;
  if (pinVisivel()) { desenhaChave(lx, ly, iw, a); ly += TU_PIN_H + 16.0f; }
  // A LINHA DE ESTADO, viva.
  { Trava t; int eu, ordem[TRAVA_MEMBROS];
    char frase[260];
    float ar, ag, ab;
    acentoRGB(&ar, &ag, &ab);
    travaDaEscolha(&t, &eu, ordem);
    if (t.n >= 2) {
      // nomeIdx le o profile_index pelo indice do membro: `ordem` e isso.
      trava_frase(&t, eu, nomeIdx, ordem, frase, sizeof frase);
    } else snprintf(frase, sizeof frase, "%s", i18n("Escolha com quem travar."));
    if (aviso[0]) snprintf(frase, sizeof frase, "%s", aviso);
    gfx_cor((GfxRect){ lx + 4.0f, ly + 20.0f, 12.0f, 12.0f }, .5f, ar, ag, ab, a);
    txt_bloco_corta(TXT_ILHA_TEXTO, frase, 243, 242, 239, lx + 30.0f, ly + 8.0f, iw - 30.0f, 28.0f, .9f * a, 2); }
  ly += 56.0f;
  desenhaAcoes(lx, ly, a);
  { const char *teclas[] = { "← → ↑ ↓", "OK", "Voltar" };
    const char *rot[] = { "Navegar", "Escolher", "Fechar" };
    plrui_dicas(teclas, rot, 3, lx, y + h - TU_PAD - 4.0f, 0, a); }
}

static void desenhaEspera(float a, Uint32 agora) {
  float h = TU_PAD * 2.0f + TU_ESP_ARTE_H + 120.0f, x, y;
  float cx = NV_TELA_W * .5f, cy = NV_TELA_H * .5f + (1.0f - a) * 40.0f;
  float rx, rw, ry;
  const CatEp *ep = NULL;
  const char *still = NULL;
  int i, n = idx >= 0 ? cat_n_episodios(idx) : 0;
  const Trava *tv = trava_por_id(idTrava);
  for (i = 0; i < n; i++) {
    const CatEp *x2 = cat_episodio(idx, i);
    if (x2 && x2->temporada == espT && x2->episodio == espE) { ep = x2; break; }
  }
  still = ep && ep->thumb[0] ? ep->thumb : (arte[0] ? arte : NULL);
  x = cx - TU_ESP_W * .5f; y = cy - h * .5f;
  material((GfxRect){ x, y, TU_ESP_W, h }, a);
  // A MINIATURA BORRADA, sempre (independe do ajuste de desfocar): e o
  // episodio que a pessoa ainda nao pode ver.
  { GfxRect r = { x + TU_PAD, y + TU_PAD, TU_ESP_ARTE_W, TU_ESP_ARTE_H };
    GLuint t2 = still ? tex_obter_larg(still, TU_ESP_ARTE_W) : 0;
    GLuint tb = t2 ? gfx_desfocado(t2, still) : 0;
    float raio = 20.0f / TU_ESP_ARTE_H;
    if (tb) { gfx_tex_aspect_atual = tex_aspecto(still);
              gfx_rect(r, tb, GFX_CARD, 0, 0, 0, raio, 0, 0, 0, a);
              gfx_tex_aspect_atual = 0.0f; }
    else gfx_cor(r, raio, .133f, .133f, .133f, a);
    gfx_cor(r, raio, 0, 0, 0, .30f * a);
    { GfxRect d = { r.x + (r.w - 88.0f) * .5f, r.y + (r.h - 88.0f) * .5f, 88.0f, 88.0f };
      gfx_cor(d, .5f, .055f, .059f, .071f, .72f * a);
      gfx_icone((GfxRect){ d.x + 26.0f, d.y + 26.0f, 36.0f, 36.0f }, "aj_lock", .95f, .95f, .95f, a); } }
  rx = x + TU_PAD + TU_ESP_ARTE_W + 40.0f;
  rw = x + TU_ESP_W - TU_PAD - rx;
  ry = y + TU_PAD;
  { char k[96], epn[32], t1[200];
    snprintf(k, sizeof k, "%s · %s", i18n("Trava de série"),
             tv && tv->modo == TRAVA_JUNTOS ? i18n("Juntos") : i18n("Separados"));
    plrui_kicker(k, rx, ry + 2.0f, 243, 242, 239, .45f * a);
    trava_ep_texto(espT, espE, epn, sizeof epn);
    snprintf(t1, sizeof t1, "%s · %s", titulo, epn);
    txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_TITULO, t1, 243, 242, 239, 255, rw), rx, ry + 28.0f, a); }
  ry += 92.0f;
  txt_bloco_corta(TXT_ILHA_TEXTO, "A sinopse e a imagem ficam escondidas até vocês chegarem aqui.",
                  243, 242, 239, rx, ry, rw, 28.0f, .62f * a, 2);
  ry += 70.0f;
  { char esp[200];
    float d = 52.0f;
    travaui_rostos(rx, ry, d, espQuem, espN, a);
    travaui_esperando(espQuem, espN, esp, sizeof esp);
    txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_NOME, esp, 243, 242, 239, 255,
                                       rw - (d + (float)(espN - 1) * d * .72f) - 20.0f),
                       rx + d + (float)(espN - 1) * d * .72f + 18.0f, ry + (d - 30.0f) * .5f, a); }
  ry = y + TU_PAD + TU_ESP_ARTE_H + 40.0f;
  if (pinEstado != PIN_NADA) {
    // O PIN: quatro caixas, a linha de quem vale e o estado.
    int com[TRAVA_MEMBROS], nc = membrosComPin(com, TRAVA_MEMBROS), j;
    char quem[160], linhaPin[220];
    float bx = x + TU_PAD;
    if (nc == 1) snprintf(quem, sizeof quem, "%s", nomeDoPerfil(com[0]));
    else if (nc == 2) snprintf(quem, sizeof quem, i18n("%s ou %s"), nomeDoPerfil(com[0]), nomeDoPerfil(com[1]));
    else snprintf(quem, sizeof quem, i18n("%s ou mais %d"), nc > 0 ? nomeDoPerfil(com[0]) : "", nc - 1);
    snprintf(linhaPin, sizeof linhaPin, i18n("PIN de %s"), quem);
    for (j = 0; j < 4; j++) {
      GfxRect b = { bx + (float)j * 76.0f, ry, 64.0f, 76.0f };
      float ar, ag, ab;
      acentoRGB(&ar, &ag, &ab);
      gfx_cor(b, 14.0f / 76.0f, 1, 1, 1, .08f * a);
      if (j == pinPos && pinEstado == PIN_DIGITANDO) gfx_anel(b, 14.0f / 76.0f, 2.5f, ar, ag, ab, a);
      if (pin[j]) gfx_cor((GfxRect){ b.x + 22.0f, b.y + 28.0f, 20.0f, 20.0f }, .5f, .95f, .95f, .95f, a);
    }
    { const char *st = pinEstado == PIN_CONFERINDO ? "Conferindo…"
                     : pinEstado == PIN_ERRADO ? "PIN incorreto. Tente de novo."
                     : pinEstado == PIN_SEM_REDE ? "Sem conexão para conferir o PIN."
                     : "Números do controle, ou ↑ ↓ para trocar o dígito.";
      txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_NOME, linhaPin, 243, 242, 239, 255, 560.0f),
                         bx + 4.0f * 76.0f + 24.0f, ry + 6.0f, a);
      txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_SUB, st,
                                         pinEstado == PIN_ERRADO ? 245 : 243,
                                         pinEstado == PIN_ERRADO ? 160 : 242,
                                         pinEstado == PIN_ERRADO ? 140 : 239, 255, 560.0f),
                         bx + 4.0f * 76.0f + 24.0f, ry + 42.0f, .7f * a);
      if (pinEstado == PIN_CONFERINDO)
        plrui_anel_solto(bx + 4.0f * 76.0f + 24.0f + 590.0f, ry + 38.0f, 28.0f, agora, a); }
  } else desenhaAcoes(x + TU_PAD, ry + 8.0f, a);
}

void travaui_desenhar(Uint32 agora) {
  float a = anim_suave(anim);
  if (anim < 0.01f) return;
  { ESCALA_SE_COUBER_INI(TU_W, tela == TU_ESPERA ? 600.0f : alturaModal());
    gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, (ajustes_vidro() ? .40f : .52f) * anim);
    if (tela == TU_ESPERA) desenhaEspera(a * anim_suave(troca), agora);
    else if (tela) desenhaModal(a * anim_suave(troca), agora);
    ESCALA_SE_COUBER_FIM(); }
}

// ================================================================ perfil, pagina 2

static int pFoco;
static float pFocoAnim[TRAVA_MAX];

void travaui_perfil_entrar(void) { pFoco = 0; travarede_atualizar_ja(); }

static int nTravasDoPerfil(int *ix) {
  int i, n = 0;
  for (i = 0; i < trava_n(); i++) {
    const Trava *t = trava_i(i);
    int k = trava_membro_do_perfil(t, trava_perfil_ativo());
    if (k >= 0 && t->m[k].estado == TRAVA_DENTRO) ix[n++] = i;
  }
  return n;
}

int travaui_perfil_evento(const SDL_Event *e) {
  int ix[TRAVA_MAX], n = nTravasDoPerfil(ix);
  SDL_Keycode k;
  if (e->type != SDL_KEYDOWN) return 1;
  k = e->key.keysym.sym;
  if (teclaVolta(e)) return 0;
  if (k == SDLK_UP) { if (pFoco >= 2) { pFoco -= 2; return 1; } return 0; }
  if (k == SDLK_DOWN) { if (pFoco + 2 < n) pFoco += 2; return 1; }
  if (k == SDLK_LEFT) { if (pFoco % 2 == 1) pFoco--; return 1; }
  if (k == SDLK_RIGHT) { if (pFoco % 2 == 0 && pFoco + 1 < n) pFoco++; return 1; }
  if (teclaOk(k) && n > 0 && pFoco < n) travaui_abrir_gerenciar(trava_i(ix[pFoco])->id);
  return 1;
}

void travaui_perfil_atualizar(float dt) {
  int i;
  for (i = 0; i < TRAVA_MAX; i++)
    pFocoAnim[i] = anim_mola(pFocoAnim[i], i == pFoco && !tela ? 1.0f : 0.0f, dt, NV_MOLA_FOCO);
}

static void cartaoTrava(GfxRect r, const Trava *t, float f, float a) {
  int eu = trava_membro_do_perfil(t, trava_perfil_ativo()), k, ordem[TRAVA_MEMBROS], np = 0;
  char frase[260], chip[64];
  float ar, ag, ab, tx = r.x + 24.0f + 120.0f + 24.0f, tw = r.x + r.w - 24.0f - tx;
  acentoRGB(&ar, &ag, &ab);
  if (ajustes_vidro()) gfx_cor(r, 26.0f / r.h, 1, 1, 1, (.05f + .07f * f) * a);
  else gfx_cor(r, 26.0f / r.h, .141f + .03f * f, .149f + .03f * f, .173f + .03f * f, a);
  if (f > .01f) gfx_anel_fora(r, 26.0f / r.h, 4.0f, 3.0f, ar, ag, ab, f * a);
  { GLuint tex = t->poster[0] ? tex_obter_larg(t->poster, 120.0f) : 0;
    GfxRect cap = { r.x + 24.0f, r.y + 20.0f, 120.0f, 180.0f };
    if (tex) { gfx_tex_aspect_atual = tex_aspecto(t->poster);
               gfx_rect(cap, tex, GFX_CARD, 0, 0, 0, 12.0f / 180.0f, 0, 0, 0, a);
               gfx_tex_aspect_atual = 0.0f; }
    else gfx_cor(cap, 12.0f / 180.0f, .133f, .133f, .133f, a); }
  txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_NOME, t->titulo[0] ? t->titulo : t->serie,
                                     243, 242, 239, 255, tw), tx, r.y + 22.0f, a);
  snprintf(chip, sizeof chip, "%s", t->pausada ? i18n("Pausada")
           : t->modo == TRAVA_JUNTOS ? i18n("Juntos") : i18n("Separados"));
  { TxtLinha l = txt_linha(TXT_MINI, chip, 243, 242, 239, 255);
    GfxRect c = { tx, r.y + 60.0f, (float)l.w + 22.0f, 28.0f };
    gfx_cor(c, .5f, ar, ag, ab, .22f * a);
    txt_desenhar_alpha(l, c.x + 11.0f, c.y + (28.0f - (float)l.h) * .5f, a); }
  // Rostos com a posicao de cada um embaixo.
  for (k = 0; k < t->n && np < 6; k++) {
    char ep[24];
    float rx = tx + (float)np * 74.0f;
    if (t->m[k].estado != TRAVA_DENTRO || t->m[k].perfil < 1) continue;
    rostoPerfil((GfxRect){ rx + 8.0f, r.y + 100.0f, 46.0f, 46.0f }, t->m[k].perfil, a);
    if (t->m[k].t > 0) trava_ep_texto(t->m[k].t, t->m[k].e, ep, sizeof ep);
    else snprintf(ep, sizeof ep, "—");
    { TxtLinha l = txt_linha_corta(TXT_ILHA_HORA, ep, 243, 242, 239, 255, 70.0f);
      txt_desenhar_alpha(l, rx + 31.0f - (float)l.w * .5f, r.y + 150.0f, .62f * a); }
    np++;
  }
  for (k = 0; k < t->n; k++) ordem[k] = t->m[k].perfil;
  trava_frase(t, eu, nomeIdx, ordem, frase, sizeof frase);
  txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_SUB, frase, 243, 242, 239, 255, r.x + r.w - 24.0f - (r.x + 24.0f)),
                     r.x + 24.0f, r.y + r.h - 36.0f, .78f * a);
  if (f > .5f) {
    TxtLinha l = txt_linha(TXT_MINI, "OK · Gerenciar", 243, 242, 239, 255);
    txt_desenhar_alpha(l, r.x + r.w - 24.0f - (float)l.w, r.y + 26.0f, .7f * f * a);
  }
}

void travaui_perfil_desenhar(float x, float y, float w, float h, float a) {
  int ix[TRAVA_MAX], n = nTravasDoPerfil(ix), i;
  float cw = (w - 24.0f) * .5f, ch = 252.0f, yy = y + 110.0f;
  (void)h;
  plrui_kicker("Perfil", x, y + 8.0f, 243, 242, 239, .45f * a);
  txt_desenhar_alpha(txt_linha(TXT_ILHA_TITULO, "Travas de série", 243, 242, 239, 255), x, y + 32.0f, a);
  if (!n) {
    GfxRect r = { x, yy, w, 200.0f };
    if (ajustes_vidro()) gfx_cor(r, 26.0f / 200.0f, 1, 1, 1, .05f * a);
    else gfx_cor(r, 26.0f / 200.0f, .141f, .149f, .173f, a);
    gfx_icone((GfxRect){ x + 40.0f, yy + 64.0f, 64.0f, 64.0f }, "aj_lock", .9f, .9f, .9f, .8f * a);
    txt_desenhar_alpha(txt_linha_corta(TXT_ILHA_NOME, "Nenhuma trava ainda", 243, 242, 239, 255, w - 180.0f),
                       x + 136.0f, yy + 58.0f, a);
    txt_bloco_corta(TXT_ILHA_SUB, travaui_disponivel()
                      ? "Na página de uma série, o cadeado trava a série com alguém da casa: ninguém vê spoiler de quem está atrás."
                      : "A trava de série precisa de dois ou mais perfis na conta Nuvio.",
                    243, 242, 239, x + 136.0f, yy + 96.0f, w - 180.0f, 26.0f, .62f * a, 2);
    return;
  }
  if (pFoco >= n) pFoco = n - 1;
  for (i = 0; i < n; i++) {
    GfxRect r = { x + (float)(i % 2) * (cw + 24.0f), yy + (float)(i / 2) * (ch + 24.0f), cw, ch };
    if (r.y > NV_TELA_H) break;
    cartaoTrava(r, trava_i(ix[i]), pFocoAnim[i], a);
  }
}

#ifdef NV_SHOT_HOOKS
void travaui_shot_foco(int l, int c) { linha = l; coluna = c; }
void travaui_shot_pin(const char *d, int estado) {
  snprintf(pin, sizeof pin, "%s", d ? d : "");
  pinPos = (int)strlen(pin);
  pinEstado = estado;
}
#endif
