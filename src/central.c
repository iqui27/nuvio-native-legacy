// Ver central.h.
#define NV_ESCALA_TELA
#include "central.h"
#include "centrallista.h"
#include "chsegura.h"
#include "ajustes.h"
#include "anim.h"
#include "catalogo.h"
#include "dados.h"
#include "escala.h"
#include "estatvivo.h"
#include "estatvivoui.h"
#include "gfx.h"
#include "horafmt.h"
#include "idioma.h"
#include "idiomacod.h"
#include "layout.h"
#include "perfiltv.h"
#include "perfis.h"
#include "player.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "ilha.h"
#include "plrilha.h"
#include "redesaude.h"
#include "tex_cache.h"
#include "text.h"
#include "teclavoltar.h"
#include "video.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif

// O PAINEL e a ilha do relogio esticada, numa coluna estreita pendurada no
// canto dela (dono, 06/10: "muito grandao... menos largo e so aumentar se
// tiver mais botoes"). A altura segue o conteudo: poucos botoes, painel curto.
#define CC_W       492.0f
#define CC_PAD      20.0f
#define CC_COLS      3
#define CC_GAP      10.0f
#define CC_BOTAO_H 104.0f
#define CC_BOTAO_R  22.0f
#define CC_ICONE    26.0f
#define CC_INFO_H   NV_ILHA_H   // a faixa de cima: a linha de informacao e a hora
#define CC_TOC_H    62.0f       // "Tocando agora", uma linha
#define CC_LINHA_H  50.0f       // linha da lista de edicao
#define CC_LINHA_GAP 4.0f
#define CC_LINHAS_MAX 9         // a lista rola dentro disso
#define CC_DICAS_H  30.0f
#define CC_COR_D    40.0f       // o disco de uma cor no seletor
#define CC_COR_COLS  9
#define CC_COR_LIN_GAP 14.0f
#define CC_COR_NOME_H 26.0f
#define CC_TETO     (NV_TELA_H - NV_ILHA_Y - 36.0f)

static int aberta, editando;
static int escolhendoCor, focoCor;   // o seletor da cor de destaque (so os 18 fixos)
static int foco;                // botao em foco; == nBotoes e "Editar atalhos"
static int focoEd;              // item do catalogo em foco na edicao
// FOCO E INTERRUPTOR EM MOLA por botao (0..1): o anel nao salta de botao em
// botao e o realce do atalho liga/desliga em ~120 ms em vez de num quadro.
static float animFoco[CENTRAL_MAX + 1], animLig[CENTRAL_MAX + 1];
static void centralPassoFoco(float dt);
static float rolaEd;            // rolagem da lista de edicao (px)
static float rolaBotoes, quadroDt;
#ifdef NV_TOUCH_UI
static ToqueRolagem toqueCentral;
static int toqueCentralRolar(const PonteiroRolagem *e) {
  if (!aberta || escolhendoCor) return 0;
  return toquerol_evento(&toqueCentral, e);
}
#endif
static CentralLista lista;
static int perfilLido = -1;
// A previa do cartao de novidades (central_previa_*): sem video tocando,
// sem aviso, e o botao `previaInverte` mostrado com o interruptor trocado.
static int previa, previaInverte = -1;
static char aviso[96];
static Uint32 avisoAte;

static ChSegura chs;
static SDL_Event chDown;        // o KEYDOWN guardado para reentregar
static int chLogado;
static Uint32 chAbriuT;         // hold completion blends into island growth
static Uint32 chCancelaT;       // silence-ended short press retracts from here
static float chCancelaP;

// --- A LISTA DO PERFIL -------------------------------------------------------
static void nomeArquivo(char *d, size_t n) { snprintf(d, n, "central-p%d.txt", perfis_ativo()); }
static void carregar(void) {
  char nome[40], *t;
  if (perfis_ativo() == perfilLido) return;
  perfilLido = perfis_ativo();
  nomeArquivo(nome, sizeof nome);
  t = dados_ler(nome);
  centrallista_ler(&lista, t);
  free(t);
}
static void gravar(void) {
  char nome[40], buf[CENTRAL_MAX * 48 + 8];
  nomeArquivo(nome, sizeof nome);
  centrallista_escrever(&lista, buf, sizeof buf);
  dados_gravar(nome, buf);
}

// ESTATISTICAS (mockup stats-velocidade, tela 1): com video no player, o
// primeiro botao e sempre o das estatisticas ao vivo (estatvivoui.h). Nao e
// uma opcao de Ajustes nem mora na lista da pessoa: e uma acao, e so existe
// enquanto ha o que medir. op = CC_OP_ESTAT, item = -1.
#define CC_OP_ESTAT (-2)
#ifdef CENTRAL_TESTE
static int testeEstat;
#endif
static int temEstat(void) {
#ifdef CENTRAL_TESTE
  if (testeEstat) return !previa;
#endif
  return !previa && player_aberto() && player_com_video();
}

// Os botoes que existem neste build (a chave pode nao estar na tela daqui).
static int botoes(int *op, int *item) {
  int i, n = 0;
  if (temEstat()) {
    if (op) op[n] = CC_OP_ESTAT;
    if (item) item[n] = -1;
    n++;
  }
  for (i = 0; i < lista.n; i++) {
    const CentralItem *c = central_catalogo(lista.item[i]);
    int o = c ? ajustes_rapido_op(c->chave) : -1;
    if (o < 0) continue;
    if (op) op[n] = o;
    if (item) item[n] = lista.item[i];
    n++;
  }
  return n;
}
// (ver o comentario nas variaveis, no topo do arquivo)
static void centralPassoFoco(float dt) {
  int op[CENTRAL_MAX + 1], item[CENTRAL_MAX + 1], n = botoes(op, item), i;
  int red = ajustes_animacoes_reduzidas();
  for (i = 0; i <= n; i++) {
    float aF = i == foco ? 1.0f : 0.0f, aL = 0.0f;
    if (i < n) {
      int lig = ajustes_rapido_ligado(op[i]);
      if (previa && i == previaInverte && lig >= 0) lig = !lig;
      aL = lig == 1 ? 1.0f : 0.0f;
    }
    // dt<=0 (a previa) pousa no alvo: a cena ja traz o quadro final.
    animFoco[i] = (red || dt <= 0.0f) ? aF : anim_mola(animFoco[i], aF, dt, NV_MOLA_FOCO);
    animLig[i] = (red || dt <= 0.0f) ? aL : anim_mola(animLig[i], aL, dt, NV_MOLA_FOCO);
  }
}
// O catalogo oferecido na edicao: so o que este build tem.
static int ofertas(int *item) {
  int i, n = 0;
  for (i = 0; i < central_catalogo_n(); i++)
    if (ajustes_rapido_op(central_catalogo(i)->chave) >= 0) item[n++] = i;
  return n;
}

void central_abrir(void) {
  carregar();
  if (aberta) return;
  aberta = 1;
  editando = 0;
  escolhendoCor = 0;
  foco = 0;
  memset(animFoco, 0, sizeof animFoco);   // o foco acende com a abertura da ilha
  memset(animLig, 0, sizeof animLig);
  rolaBotoes = 0;
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueCentral);
#endif
  aviso[0] = 0;
  printf("[central] aberta (%d atalhos)\n", botoes(NULL, NULL));
  fflush(stdout);
}
void central_fechar(void) { aberta = 0; editando = 0; escolhendoCor = 0; }
int  central_aberta(void) { return aberta; }

static void avisar(const char *s) {
  snprintf(aviso, sizeof aviso, "%s", s);
  avisoAte = SDL_GetTicks() + 2500u;
}

// --- TECLADO -----------------------------------------------------------------
// O CH+ depois do remapeamento (Salvos na Samsung/Android, scancode no LG) e a
// AZUL: a mesma tecla que abriu fecha.
static int ehCh(const SDL_Event *e) {
  SDL_Keycode k = e->key.keysym.sym;
  int sc = e->key.keysym.scancode;
  return k == SDLK_s || k == SDLK_PAGEUP || sc == NV_SCANCODE_BLUE || sc == NV_SCANCODE_CH_UP;
}

static void ativarBotao(int i) {
  int op[CENTRAL_MAX + 1], n = botoes(op, NULL);
  if (i == n) {
    int it[64], m = ofertas(it), j;
    editando = 1; focoEd = 0; rolaEd = 0;
#ifdef NV_TOUCH_UI
    toquerol_limpar(&toqueCentral);
#endif
    for (j = 0; j < m; j++) if (centrallista_tem(&lista, it[j])) { focoEd = j; break; }
    return;
  }
  if (i < 0 || i >= n) return;
  if (op[i] == AJ_RAPIDO_COR) {
    int a = ajustes_rapido_cor_atual();
    escolhendoCor = 1;
    focoCor = a >= 0 ? a : 0;
    return;
  }
  if (op[i] == CC_OP_ESTAT) {
    central_fechar();
    estatvivo_abrir();
    return;
  }
  if (ajustes_rapido_passo(op[i], 1))
    printf("[central] %s -> %s\n", ajustes_rapido_rotulo(op[i]), ajustes_rapido_valor(op[i]));
  else avisar("Não foi possível salvar. O valor anterior foi mantido.");
  fflush(stdout);
}

static void alternarOferta(int j) {
  int it[64], m = ofertas(it), r;
  if (j < 0 || j >= m) return;
  r = centrallista_alternar(&lista, it[j]);
  if (r < 0) {
    char t[96];
    snprintf(t, sizeof t, i18n("No máximo %d atalhos"), CENTRAL_MAX);
    avisar(t);
    return;
  }
  gravar();
}

// Aplica a cor e grava como Ajustes; o painel inteiro ja se pinta com ela no
// proximo quadro (os botoes ligados e a marca leem ajustes_acento).
static void corAplicar(int i) {
  if (ajustes_rapido_cor_definir(i)) {
    printf("[central] %s -> %s\n", ajustes_rapido_rotulo(AJ_RAPIDO_COR), ajustes_rapido_cor_nome(i));
  } else avisar("Não foi possível salvar. O valor anterior foi mantido.");
  fflush(stdout);
}

void central_evento(const SDL_Event *e) {
#ifdef NV_TOUCH_UI
  if (toquerol_navegacao(e)) toquerol_limpar(&toqueCentral);
#endif
  SDL_Keycode k;
  if (!aberta || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (ehCh(e)) { central_fechar(); return; }
  if (escolhendoCor) {
    int n = ajustes_rapido_cor_n();
    if (nv_tecla_voltar(e)) { escolhendoCor = 0; return; }
    if (k == SDLK_LEFT && focoCor % CC_COR_COLS > 0) focoCor--;
    else if (k == SDLK_RIGHT && focoCor % CC_COR_COLS < CC_COR_COLS - 1 && focoCor + 1 < n) focoCor++;
    else if (k == SDLK_UP && focoCor >= CC_COR_COLS) focoCor -= CC_COR_COLS;
    else if (k == SDLK_DOWN && focoCor + CC_COR_COLS < n) focoCor += CC_COR_COLS;
    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) corAplicar(focoCor);
    return;
  }
  if (editando) {
    int it[64], m = ofertas(it);
    if (nv_tecla_voltar(e)) { editando = 0; return; }
    if (k == SDLK_UP && focoEd > 0) focoEd--;
    else if (k == SDLK_DOWN && focoEd + 1 < m) focoEd++;
    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) alternarOferta(focoEd);
    return;
  }
  {
    int n = botoes(NULL, NULL) + 1;   // + "Editar atalhos"
    if (foco >= n) foco = n - 1;
    if (nv_tecla_voltar(e)) { central_fechar(); return; }
    if (k == SDLK_LEFT && foco % CC_COLS > 0) foco--;
    else if (k == SDLK_RIGHT && foco % CC_COLS < CC_COLS - 1 && foco + 1 < n) foco++;
    else if (k == SDLK_UP && foco >= CC_COLS) foco -= CC_COLS;
    else if (k == SDLK_DOWN && foco + CC_COLS < n) foco += CC_COLS;
    else if (k == SDLK_DOWN && foco / CC_COLS < (n - 1) / CC_COLS) foco = n - 1;
    else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) ativarBotao(foco);
  }
}

// Ponteiro (Magic Remote, toque): mesmo foco e mesmo OK das setas.
static void ptFoco(int i, int ed) { if (ed == 2) focoCor = i; else if (ed) focoEd = i; else foco = i; }
static void ptOk(int i, int ed) {
  ptFoco(i, ed);
  if (ed == 2) corAplicar(i);
  else if (ed) alternarOferta(i);
  else ativarBotao(i);
}
static void ptFora(int a, int b) { (void)a; (void)b; central_fechar(); }

static void corpo(GfxRect r, float a, void *u);
static float altura(int comFaixa);

// A CENTRAL NAO TEM SUPERFICIE PROPRIA: ela e a ilha do relogio esticada
// (ilha_corpo) ou, com o player aberto, a ilha do player (plrilha_pedir), que
// ja cresce da pilula da hora. Pedido por quadro; fechada, a ilha recolhe
// sozinha chamando o corpo com o alfa caindo. Mudou a altura (botao posto ou
// tirado, edicao): a ilha anda ate a nova na mola dela.
void central_atualizar(float dt, Uint32 agora) {
  (void)agora;
  quadroDt = anim_clamp(dt, 0, 0.05f);
  if (!aberta) return;
  carregar();
  centralPassoFoco(dt);
  if (!player_aberto()) ilha_corpo(CC_W, altura(1), corpo, NULL);
}
// No player o pedido vai logo antes de plrilha_desenhar: o ultimo pedido com
// corpo vence, e o player faz os dele no proprio desenho. O cabecalho e o da
// plrilha ("Central de controle" e a hora); o corpo comeca abaixo dele.
void central_desenhar(Uint32 agora) {
  (void)agora;
  if (aberta && player_aberto()) {
    PlrIlhaPedido p;
    memset(&p, 0, sizeof p);
    p.texto = i18n("Central de controle");
    p.w = CC_W;
    p.h = altura(0);
    p.corpo = corpo;
    p.u = (void *)1;
    p.aberta = 1;
    p.modal = 1;
    p.ancoraTopo = 1;
    p.insetEsq = CC_PAD;
    p.tituloLeve = 1;
    plrilha_pedir(&p);
  }
}

// --- A TECLA CH+ ---------------------------------------------------------------
int central_tecla_ocupada(void) { return chs_ocupado(&chs); }

// Read the same clock that decides the hold: feedback adds no opening delay.
static float segurarFracao(Uint32 t) {
  if (t - chs.t0 < CHS_ATRASO_MS) return 0.0f;  // dead zone: a click draws nothing
  return anim_clamp((float)(t - chs.t0 - CHS_ATRASO_MS) / (float)(CHS_SEGURAR_MS - CHS_ATRASO_MS), 0.0f, 1.0f);
}
static void segurarAbrir(Uint32 t) {
  chAbriuT = t ? t : 1;
  chCancelaP = 0.0f;
  central_abrir();
}

float central_segurar_progresso(Uint32 agora) {
  float f;
  if (chs.estado == CHS_APERTADO) {
    if (!chs.solto) return segurarFracao(agora);
    if (anim_politica_reduzida) return 0.0f;
    f = (float)(agora - chs.tSolta) / (float)CHS_EMENDA_MS;
    return segurarFracao(chs.tSolta) * (1.0f - anim_suave(f));
  }
  // At the threshold, fade the completed fill as the existing island grows.
  if (chs.estado == CHS_SEGURADO && aberta && chAbriuT) {
    if (anim_politica_reduzida) return agora == chAbriuT ? 1.0f : 0.0f;
    f = (float)(agora - chAbriuT) / (float)CHS_EMENDA_MS;
    return 1.0f - anim_suave(f);
  }
  if (chs.estado == CHS_LIVRE && chCancelaP > 0.0f && !anim_politica_reduzida) {
    f = (float)(agora - chCancelaT) / (float)CHS_EMENDA_MS;
    if (f >= 1.0f) { chCancelaP = 0.0f; return 0.0f; }
    return chCancelaP * (1.0f - anim_suave(f));
  }
  return 0.0f;
}

// Both clock islands draw the fill before their text, within their own scale.
void central_segurar_desenhar(GfxRect r, float raio, float a, Uint32 agora) {
  float p = central_segurar_progresso(agora), cr, cg, cb;
  if (p <= 0.0f || a <= 0.0f) return;
  ajustes_acento(&cr, &cg, &cb);
  gfx_recorte(r.x, r.y, r.w * p, r.h);
  gfx_cor(r, raio, cr, cg, cb, a);
  gfx_sem_recorte();
}

int central_tecla(const SDL_Event *e, int pode) {
  Uint32 t;
  int r = CHS_NADA;
  if (e->type != SDL_KEYDOWN && e->type != SDL_KEYUP) return 0;
  if (!chs_ocupado(&chs) && (!pode || e->type == SDL_KEYUP)) return 0;
  t = e->key.timestamp ? e->key.timestamp : SDL_GetTicks();
  if (e->type == SDL_KEYDOWN) {
    if (!chs_ocupado(&chs)) { chDown = *e; chCancelaP = 0.0f; chAbriuT = 0; }
    r = chs_desce(&chs, t);
  } else {
    // Uma linha so por sessao: quanto o CH+ deste controle leva entre descer e
    // subir. E o que diz, num log de TV, se o KEYUP vale como "soltou".
    if (!chLogado) {
      chLogado = 1;
      printf("[central] CH+ subiu %u ms depois de descer\n", (unsigned)(t - chs.tUlt));
      fflush(stdout);
    }
    chs_sobe(&chs, t);
  }
  if (r == CHS_LONGO) segurarAbrir(t);
  return 1;
}

void central_tecla_quadro(Uint32 agora, void (*entregar)(SDL_Event *e)) {
  int r;
  float p;
  if (!chs_ocupado(&chs)) return;
  p = central_segurar_progresso(agora);
  r = chs_quadro(&chs, agora);
  if (r == CHS_LONGO) segurarAbrir(agora);
  else if (r == CHS_CURTO) {
    // Without a valid KEYUP, retract on silence while delivering the original tap.
    if (p > 0.0f) { chCancelaP = p; chCancelaT = agora ? agora : 1; }
    if (entregar) {
      SDL_Event d = chDown, u = chDown;
      u.type = SDL_KEYUP;
      u.key.state = SDL_RELEASED;
      entregar(&d);
      entregar(&u);
    }
  }
}

// --- DESENHO (dentro da ilha) ----------------------------------------------------
#ifdef CENTRAL_TESTE
static char testeTit[96], testeMeta[96];
#endif

// O VIDEO QUE ESTA TOCANDO: so o que o pipeline disse (video.h). 0 = nada.
static int tocando(char *tit, size_t nt, char *meta, size_t nm) {
  const CatItem *c;
  const char *hdr, *ep;
  size_t u = 0;
  tit[0] = meta[0] = 0;
#ifdef CENTRAL_TESTE
  if (testeTit[0]) { snprintf(tit, nt, "%s", testeTit); snprintf(meta, nm, "%s", testeMeta); return 1; }
#endif
  if (previa || !player_aberto() || !player_com_video()) return 0;
  c = cat_item(player_indice());
  if (c && c->titulo[0]) snprintf(tit, nt, "%s", c->titulo);
  ep = player_linha_episodio();
  if (ep && ep[0]) u += (size_t)snprintf(meta + u, nm - u, "%s", ep);
  if (video_altura() > 0 && u < nm)
    u += (size_t)snprintf(meta + u, nm - u, "%s%dp", u ? " · " : "", video_altura());
  hdr = video_hdr();
  if (hdr && hdr[0] && strcmp(hdr, "none") && strcmp(hdr, "SDR") && u < nm)
    u += (size_t)snprintf(meta + u, nm - u, "%s%s", u ? " · " : "",
                          !strcmp(hdr, "DolbyVision") ? "Dolby Vision" : hdr);
  if (video_tem_atmos() && u < nm)
    u += (size_t)snprintf(meta + u, nm - u, "%sAtmos", u ? " · " : "");
  // DTS tocando direto ou convertido (PR #259): a faixa diz, o estado nao muda o nome.
  if (video_dts_estado() != VIDEO_DTS_NENHUM && u < nm) {
    int d = video_faixa_dts(video_audio(video_audio_atual()));
    u += (size_t)snprintf(meta + u, nm - u, "%s%s", u ? " · " : "",
                          d == 3 ? "DTS:X" : d == 2 ? "DTS-HD" : "DTS");
  }
  // "42,8 Mbps agora" (mockup, tela 1): o numero das estatisticas ao vivo,
  // com o "≈" quando e estimado.
  { int est, k = estatvivo_kbps(&est);
    if (k >= 0 && u < nm) {
      char m[24], f[48];
      ev_fmt_mbps(m, sizeof m, k, idioma_ponto_decimal(ajustes_idioma()) ? '.' : ',');
      snprintf(f, sizeof f, i18n("%s Mbps agora"), m);
      u += (size_t)snprintf(meta + u, nm - u, "%s%s%s", u ? " · " : "", est ? "≈ " : "", f);
    } }
  return tit[0] || meta[0];
}

static int linhasBotoes(void) { return (botoes(NULL, NULL) + 1 + CC_COLS - 1) / CC_COLS; }
static int linhasEdicao(void) {
  int it[64], m = ofertas(it);
  return m < CC_LINHAS_MAX ? m : CC_LINHAS_MAX;
}

// ALTURA PELO CONTEUDO. `comFaixa` = a faixa de cima e da ilha (hora a
// direita); no player o cabecalho e da plrilha e a linha de informacao entra
// no corpo.
static float altura(int comFaixa) {
  char t[96], m[96];
  float h = comFaixa ? CC_INFO_H : CC_INFO_H - 12.0f;
  if (escolhendoCor) {
    h += 6.0f + 2.0f * CC_COR_D + CC_COR_LIN_GAP + 14.0f + CC_COR_NOME_H + 14.0f + CC_DICAS_H;
  } else if (editando) {
    int n = linhasEdicao();
    h += 6.0f + (float)n * (CC_LINHA_H + CC_LINHA_GAP) - CC_LINHA_GAP + 14.0f + CC_DICAS_H;
  } else {
    int n = linhasBotoes();
    h += 6.0f;
    if (tocando(t, sizeof t, m, sizeof m)) h += CC_TOC_H + 10.0f;
    h += (float)n * (CC_BOTAO_H + CC_GAP) - CC_GAP;
  }
  h += CC_PAD;
  return h > CC_TETO ? CC_TETO : h;
}

static void superficie(GfxRect r, float raioPx, float f, float a) {
  if (ajustes_vidro()) gfx_cor(r, raioPx / r.h, 1, 1, 1, (0.07f + 0.10f * f) * a);
  else gfx_cor(r, raioPx / r.h, 0.135f + 0.06f * f, 0.14f + 0.06f * f, 0.163f + 0.06f * f, a);
}

// Um ponto "·" fraco entre os itens da linha de informacao.
static float sep(float x, float yc, float a) {
  gfx_cor((GfxRect){ x + 9.0f, yc - 2.0f, 4.0f, 4.0f }, 0.5f, 1, 1, 1, 0.28f * a);
  return 22.0f;
}

// A LINHA DE INFORMACAO, uma so: perfil (a cor do avatar e o nome), a rede (o
// icone diz; sem internet o icone cortado e a palavra) e a versao. A hora fica
// na ponta direita, que e a da ilha.
static void linhaInfo(float x, float yc, float wMax, float a) {
  const ContaPerfil *p = perfis_item_ativo();
  int off = rede_saude_offline();
  float x0 = x;
  if (p && p->nome[0]) {
    float pr = 0.12f, pg = 0.53f, pb = 0.90f;
    unsigned v;
    TxtLinha n;
    if (p->corHex[0] == '#' && sscanf(p->corHex + 1, "%6x", &v) == 1) {
      pr = (float)((v >> 16) & 255) / 255.0f; pg = (float)((v >> 8) & 255) / 255.0f; pb = (float)(v & 255) / 255.0f;
    }
    gfx_cor((GfxRect){ x, yc - 6.0f, 12.0f, 12.0f }, 0.5f, pr, pg, pb, a);
    x += 20.0f;
    n = txt_linha_corta(TXT_ILHA_SUB, p->nome, 243, 242, 239, 255, wMax * 0.45f);
    txt_desenhar_alpha(n, x, yc - (float)n.h * 0.5f, 0.9f * a);
    x += (float)n.w;
    x += sep(x, yc, a);
  }
  gfx_icone((GfxRect){ x, yc - 9.0f, 18.0f, 18.0f }, off ? "aj_wifi-off" : "aj_wifi",
            off ? 0.94f : 0.953f, off ? 0.72f : 0.949f, off ? 0.42f : 0.937f, (off ? 1.0f : 0.7f) * a);
  x += 18.0f;
  if (off) {
    TxtLinha o = txt_linha(TXT_ILHA_HORA, "Sem internet", 240, 184, 107, 255);
    txt_desenhar_alpha(o, x + 6.0f, yc - (float)o.h * 0.5f, a);
    x += 6.0f + (float)o.w;
  }
  x += sep(x, yc, a);
  if (x - x0 < wMax - 40.0f) {
    TxtLinha v = txt_linha_corta(TXT_ILHA_HORA, NV_VERSAO, 243, 242, 239, 255, wMax - (x - x0));
    txt_desenhar_alpha(v, x, yc - (float)v.h * 0.5f, 0.45f * a);
  }
}

static void cartaoTocando(GfxRect r, const char *tit, const char *meta, float a) {
  float tx = r.x + 14.0f + 32.0f + 12.0f, tw = r.x + r.w - 14.0f - tx;
  superficie(r, 16.0f, 0.0f, a);
  gfx_icone((GfxRect){ r.x + 14.0f, r.y + (r.h - 32.0f) * 0.5f, 32.0f, 32.0f }, "aj_circle-play",
            0.953f, 0.949f, 0.937f, 0.85f * a);
  { TxtLinha t = txt_linha_corta(TXT_ILHA_ITEM, tit[0] ? tit : "Tocando agora", 243, 242, 239, 255, tw);
    TxtLinha m = txt_linha_corta(TXT_ILHA_HORA, meta, 243, 242, 239, 255, tw);
    float hy = r.y + (r.h - (float)t.h - (meta[0] ? 2.0f + (float)m.h : 0.0f)) * 0.5f;
    txt_desenhar_alpha(t, tx, hy, a);
    if (meta[0]) txt_desenhar_alpha(m, tx, hy + (float)t.h + 2.0f, 0.55f * a); }
}

// UM BOTAO: icone em cima, nome curto e, quando nao e interruptor, o valor.
// Active tiles keep their accent; focus adds frost, a top light and the ring.
// `ligF` (0..1) e `f` (0..1) sao MOLAS: o realce liga/desliga e o anel entra
// em ~120 ms (NV_MOLA_FOCO), em vez de trocar num quadro. A tinta do TEXTO
// troca no ponto medio (rasterizado nao recolor por quadro); icone, superficies
// e anel crossfadezam de verdade.
static void botao(GfxRect r, const char *icone, const char *rot, const char *val,
                  int ligado, float ligF, float f, float a) {
  float ar, ag, ab, w = r.w - 24.0f, y;   // 14 a esquerda, 10 a direita
  int acesoV = ligado >= 0 ? ligF > 0.5f : 0;
  int tinta = acesoV ? ajustes_tinta_foco() : 243;
  float ti = ligado >= 0 ? anim_mistura(243.0f, (float)ajustes_tinta_foco(), ligF) / 255.0f
                        : (float)tinta / 255.0f;
  TxtLinha l, v;
  ajustes_acento(&ar, &ag, &ab);
  // Share the cached frost even with interface glass off; never blur per tile.
  if (f > 0.01f) gfx_vidro_folha(r, CC_BOTAO_R / r.h, f * a);
  if (ligado >= 0 && ligF > 0.005f) gfx_cor(r, CC_BOTAO_R / r.h, ar, ag, ab, a * ligF * (1.0f - 0.08f * f));
  if (ligado < 0 || ligF < 0.995f) superficie(r, CC_BOTAO_R, f, a * (1.0f - ligF) * (1.0f - f));
  if (f > 0.01f) {
    gfx_brilho_topo(r, CC_BOTAO_R / r.h, 0.45f, 1, 1, 1, 0.16f * f * a);
    gfx_anel_fora(r, CC_BOTAO_R / r.h, 3.0f, 2.5f, 1, 1, 1, 0.95f * f * a);
  }
  gfx_icone((GfxRect){ r.x + 14.0f, r.y + 14.0f, CC_ICONE, CC_ICONE }, icone, ti, ti, ti,
            anim_mistura(0.88f, 1.0f, ligado >= 0 ? ligF : 0.0f) * a);
  // Nome em 16/600; se nao couber (alemao, russo), um degrau menor; sem
  // valor embaixo (interruptor) e com mais de uma palavra, duas linhas em
  // 16/600 ("Choisir la / source"); o corte e o ultimo recurso.
  l = txt_linha(TXT_G16B, rot, tinta, tinta, tinta, 255);
  if ((float)l.w > w) {
    TxtLinha m = txt_linha(TXT_ILHA_HORA, rot, tinta, tinta, tinta, 255);
    if ((float)m.w <= w) l = m;
    else if (ligado >= 0 && strchr(i18n(rot), ' ')) {
      float g = gfx_opacidade_grupo, h;
      gfx_opacidade_grupo = 0.0f;
      h = txt_bloco_corta(TXT_G16B, rot, tinta, tinta, tinta, 0, 0, w, 19.0f, 1.0f, 2);
      gfx_opacidade_grupo = g;
      txt_bloco_corta(TXT_G16B, rot, tinta, tinta, tinta, r.x + 14.0f, r.y + r.h - 14.0f - h, w, 19.0f, a, 2);
      return;
    } else l = txt_linha_corta(TXT_ILHA_HORA, rot, tinta, tinta, tinta, 255, w);
  }
  y = r.y + r.h - 14.0f;
  if (ligado < 0 && val && val[0]) {
    v = txt_linha_corta(TXT_ILHA_HORA, val, tinta, tinta, tinta, 255, w);
    y -= (float)v.h;
    txt_desenhar_alpha(v, r.x + 14.0f, y, 0.6f * a);
    y -= 1.0f;
  }
  txt_desenhar_alpha(l, r.x + 14.0f, y - (float)l.h, a);
}

// Center smoothly, while keeping the entire focused tile inside the viewport.
static float rolagemBotoes(float h) {
  float passo = CC_BOTAO_H + CC_GAP;
  float top = (float)(foco / CC_COLS) * passo;
  float maxR = fmaxf(0, (float)linhasBotoes() * passo - CC_GAP - h);
  float alvo = anim_clamp(top - (h - CC_BOTAO_H) * 0.5f, 0, maxR);
  float r = anim_mola(rolaBotoes, alvo, quadroDt, 12.0f);
  return anim_clamp(r, fmaxf(0, top + CC_BOTAO_H - h), fminf(top, maxR));
}

static void desenhaBotoes(float x, float y, float w, float h, float a) {
  int op[CENTRAL_MAX + 1], item[CENTRAL_MAX + 1], n = botoes(op, item), i;
  float bw = (w - CC_GAP * (CC_COLS - 1)) / CC_COLS;
  float scroll = 0;
  if (foco > n) foco = n;
  if (!previa) {
#ifdef NV_TOUCH_UI
    float maxR = fmaxf(0, (float)linhasBotoes() * (CC_BOTAO_H + CC_GAP) - CC_GAP - h);
    toquerol_vincular(&toqueCentral, (GfxRect){x, y, w, h}, gfx_escala(), 0, maxR, 1, &rolaBotoes);
    if (aberta && a > 0.5f) ponteiro_rolagem(toqueCentralRolar);
    if (!toqueCentral.livre)
#endif
    rolaBotoes = rolagemBotoes(h);
    scroll = rolaBotoes;
  }
  gfx_recorte(x - 8.0f, y - 6.0f, w + 16.0f, h + 12.0f);
  for (i = 0; i <= n; i++) {
    GfxRect r = { x + (float)(i % CC_COLS) * (bw + CC_GAP), y + (float)(i / CC_COLS) * (CC_BOTAO_H + CC_GAP) - scroll,
                  bw, CC_BOTAO_H };
    if (r.y + r.h < y || r.y > y + h) continue;
    if (i < n && op[i] == CC_OP_ESTAT) botao(r, "aj_gauge", "Estatísticas", "", -1, 0.0f, animFoco[i], a);
    else if (i < n) {
      const CentralItem *c = central_catalogo(item[i]);
      int lig = ajustes_rapido_ligado(op[i]);
      if (previa && i == previaInverte && lig >= 0) lig = !lig;
      botao(r, c->icone, c->curto, ajustes_rapido_valor(op[i]), lig, animLig[i], animFoco[i], a);
      if (op[i] == AJ_RAPIDO_COR) {   // a cor de agora, num ponto no canto
        float cr, cg, cb;
        ajustes_acento_marca(&cr, &cg, &cb);
        gfx_cor((GfxRect){ r.x + r.w - 14.0f - 14.0f, r.y + 16.0f, 14.0f, 14.0f }, 0.5f, cr, cg, cb, a);
      }
    } else botao(r, "aj_sliders-horizontal", "Editar", "", 0, 0.0f, animFoco[i], a);
    if (aberta && a > 0.5f && ponteiro_ativo())
      ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, y, y + h, ptFoco, ptOk, i, 0);
  }
  gfx_sem_recorte();
}

// O SELETOR: 2 fileiras de 9 discos (claros em cima, profundos embaixo, a
// ordem da regua de Ajustes). O atual leva o visto na tinta que contrasta; o
// foco, o anel dos botoes. O nome da cor em foco fica embaixo.
static void desenhaCores(float x, float y, float w, float a) {
  int n = ajustes_rapido_cor_n(), atual = ajustes_rapido_cor_atual(), i;
  float gap = (w - CC_COR_D * CC_COR_COLS) / (CC_COR_COLS - 1);
  if (focoCor >= n) focoCor = n - 1;
  for (i = 0; i < n; i++) {
    float cr, cg, cb, tinta = ajustes_rapido_cor(i, &cr, &cg, &cb);
    GfxRect r = { x + (float)(i % CC_COR_COLS) * (CC_COR_D + gap),
                  y + (float)(i / CC_COR_COLS) * (CC_COR_D + CC_COR_LIN_GAP), CC_COR_D, CC_COR_D };
    gfx_cor(r, 0.5f, cr, cg, cb, a);
    if (i == atual)
      gfx_icone((GfxRect){ r.x + 9.0f, r.y + 9.0f, 22.0f, 22.0f }, "aj_check", tinta, tinta, tinta, a);
    if (i == focoCor) gfx_anel_fora(r, 0.5f, 3.0f, 2.5f, 1, 1, 1, 0.95f * a);
    if (aberta && a > 0.5f && ponteiro_ativo()) ponteiro_alvo(r.x, r.y, r.w, r.h, ptFoco, ptOk, i, 2);
  }
  { float ny = y + 2.0f * CC_COR_D + CC_COR_LIN_GAP + 14.0f;
    TxtLinha l = txt_linha_corta(TXT_ILHA_SUB, ajustes_rapido_cor_nome(focoCor), 243, 242, 239, 255, w - 90.0f);
    txt_desenhar_alpha(l, x, ny + (CC_COR_NOME_H - (float)l.h) * 0.5f, a);
    if (focoCor == atual) {
      TxtLinha m = txt_linha(TXT_ILHA_HORA, "Atual", 243, 242, 239, 255);
      txt_desenhar_alpha(m, x + (float)l.w + 12.0f, ny + (CC_COR_NOME_H - (float)m.h) * 0.5f, 0.55f * a);
    } }
}

static void desenhaEdicao(float x, float y, float w, float h, float a) {
  int it[64], m = ofertas(it), j;
  float passo = CC_LINHA_H + CC_LINHA_GAP;
  float alvo = (float)focoEd * passo - (h - CC_LINHA_H) * 0.5f;
  float maxR = (float)m * passo - CC_LINHA_GAP - h;
  if (alvo > maxR) alvo = maxR;
  if (alvo < 0) alvo = 0;
#ifdef NV_TOUCH_UI
  toquerol_vincular(&toqueCentral, (GfxRect){x, y, w, h}, gfx_escala(), 0, maxR, 1, &rolaEd);
  if (aberta && a > 0.5f) ponteiro_rolagem(toqueCentralRolar);
  if (!toquerol_livre(&toqueCentral))
#endif
  rolaEd = alvo;
  gfx_recorte(x - 8.0f, y, w + 16.0f, h);
  for (j = 0; j < m; j++) {
    const CentralItem *c = central_catalogo(it[j]);
    int op = ajustes_rapido_op(c->chave), tem = centrallista_tem(&lista, it[j]);
    GfxRect r = { x, y + (float)j * passo - rolaEd, w, CC_LINHA_H };
    GfxRect ic = { r.x + r.w - 14.0f - 22.0f, r.y + (r.h - 22.0f) * 0.5f, 22, 22 };
    if (r.y + r.h < y || r.y > y + h) continue;
    if (j == focoEd) superficie(r, r.h * 0.5f, 1.0f, a);
    gfx_icone((GfxRect){ r.x + 14.0f, r.y + (r.h - 22.0f) * 0.5f, 22, 22 }, c->icone,
              0.953f, 0.949f, 0.937f, (j == focoEd ? 1.0f : 0.72f) * a);
    { TxtLinha t = txt_linha_corta(TXT_ILHA_SUB, ajustes_rapido_rotulo(op), 243, 242, 239, 255,
                                   r.w - 14.0f - 22.0f - 12.0f - 50.0f);
      txt_desenhar_alpha(t, r.x + 48.0f, r.y + (r.h - (float)t.h) * 0.5f, (j == focoEd ? 1.0f : 0.72f) * a); }
    if (tem) {
      float ar, ag, ab;
      ajustes_acento_marca(&ar, &ag, &ab);
      gfx_icone(ic, "aj_circle-check", ar, ag, ab, a);
    } else gfx_icone(ic, "aj_plus", 0.953f, 0.949f, 0.937f, 0.45f * a);
    if (aberta && a > 0.5f && ponteiro_ativo()) {
#ifdef NV_TOUCH_UI
      if (layout_modo_mobile())
      ponteiro_alvo_faixa(r.x, r.y, r.w, r.h, y, y + h, ptFoco, ptOk, j, 1);
      else
#endif
      ponteiro_alvo(r.x, r.y, r.w, r.h, ptFoco, ptOk, j, 1);
    }
  }
  gfx_sem_recorte();
}

// Teclas do rodape (a peca das dicas de Ajustes, com alfa para entrar junto).
static void dicas(const char *const *k, const char *const *l, int n, float x, float y, float a) {
  int i;
  for (i = 0; i < n; i++) {
    TxtLinha tk = txt_linha(TXT_AJ_KBD, k[i], 243, 242, 239, 255);
    TxtLinha tl = txt_linha(TXT_ILHA_GENERO, l[i], 243, 242, 239, 255);
    float kw = (float)tk.w + 18.0f < 34.0f ? 34.0f : (float)tk.w + 18.0f;
    superficie((GfxRect){ x, y, kw, 28 }, 14.0f, 0.3f, a);
    txt_desenhar_alpha(tk, x + (kw - (float)tk.w) * 0.5f, y + (28.0f - (float)tk.h) * 0.5f, 0.82f * a);
    txt_desenhar_alpha(tl, x + kw + 9.0f, y + (28.0f - (float)tl.h) * 0.5f, 0.5f * a);
    x += kw + 9.0f + (float)tl.w + 20.0f;
  }
}

// O CONTEUDO. Na ilha (u == NULL) `r` e o painel inteiro e a faixa de cima
// (CC_INFO_H) e da hora, que a ilha mantem no lugar, a direita: a linha de
// informacao (ou o titulo da edicao) fica a esquerda dela. No player (u !=
// NULL) o cabecalho e da plrilha e `r` comeca abaixo dele.
static void corpo(GfxRect r, float a, void *u) {
  float cx = r.x + CC_PAD, cw = r.w - 2.0f * CC_PAD, y, yc;
  float infoW = u ? cw : cw - 110.0f;   // a hora ocupa a ponta direita da faixa
  Uint32 agora = SDL_GetTicks();
  if (a < 0.01f) return;
  if (aberta && a > 0.5f && ponteiro_ativo()) {
    ponteiro_camada();
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ptFora, 0, 0);
    ponteiro_alvo(r.x, r.y, r.w, r.h, NULL, NULL, 0, 0);
  }
  yc = u ? r.y + (CC_INFO_H - 12.0f) * 0.5f : r.y + CC_INFO_H * 0.5f;
  y = r.y + (u ? CC_INFO_H - 12.0f : CC_INFO_H) + 6.0f;

  if (escolhendoCor) {
    float base = r.y + r.h - CC_PAD - CC_DICAS_H;
    ajustes_ui_kicker(i18n("Cor de destaque"), cx, yc - 8.0f, a);
    desenhaCores(cx, y, cw, a);
    if (aviso[0] && (Sint32)(avisoAte - agora) > 0) {
      TxtLinha l = txt_linha_corta(TXT_ILHA_HORA, aviso, 240, 184, 107, 255, cw);
      txt_desenhar_alpha(l, cx + cw - (float)l.w, base + (CC_DICAS_H - (float)l.h) * 0.5f, a);
    }
    { static const char *const tk[] = { "OK", "Voltar" };
      static const char *const rot[] = { "Escolher", "Pronto" };
      dicas(tk, rot, 2, cx, base, a); }
    return;
  }

  if (editando) {
    char t[24];
    float kw = ajustes_ui_kicker(i18n("Editar atalhos"), cx, yc - 8.0f, a);
    float base = r.y + r.h - CC_PAD - CC_DICAS_H;
    snprintf(t, sizeof t, "%d / %d", botoes(NULL, NULL) - temEstat(), ofertas((int[CENTRAL_MAX]){0}));
    { TxtLinha l = txt_linha(TXT_ILHA_HORA, t, 243, 242, 239, 255);
      txt_desenhar_alpha(l, cx + kw + 12.0f, yc - (float)l.h * 0.5f, 0.55f * a); }
    desenhaEdicao(cx, y, cw, base - 14.0f - y, a);
    if (aviso[0] && (Sint32)(avisoAte - agora) > 0) {
      TxtLinha l = txt_linha_corta(TXT_ILHA_HORA, aviso, 240, 184, 107, 255, cw);
      txt_desenhar_alpha(l, cx + cw - (float)l.w, base + (CC_DICAS_H - (float)l.h) * 0.5f, a);
    }
    { static const char *const tk[] = { "OK", "Voltar" };
      static const char *const rot[] = { "Pôr ou tirar", "Pronto" };
      dicas(tk, rot, 2, cx, base, a); }
    return;
  }

  linhaInfo(cx, yc, infoW, a);
  { char tit[96], meta[96];
    if (tocando(tit, sizeof tit, meta, sizeof meta)) {
      cartaoTocando((GfxRect){ cx, y, cw, CC_TOC_H }, tit, meta, a);
      y += CC_TOC_H + 10.0f;
    } }
  desenhaBotoes(cx, y, cw, fmaxf(CC_BOTAO_H, r.y + r.h - CC_PAD - 6.0f - y), a);
  // Falha ao gravar: some sozinha; mora na faixa de cima, no lugar da linha.
  if (!previa && aviso[0] && (Sint32)(avisoAte - agora) > 0) {
    TxtLinha l = txt_linha_corta(TXT_ILHA_HORA, aviso, 240, 184, 107, 255, cw);
    txt_desenhar_alpha(l, cx, r.y + r.h - CC_PAD * 0.5f - (float)l.h, a);
  }
}

// A PREVIA (novidades201.c): o mesmo desenho da central aberta, com os
// atalhos de fabrica, sem ler nem gravar a lista da pessoa, sem alvo de
// ponteiro e sem mexer no foco dela.
static void previaEntrar(CentralLista *salva, int *est) {
  *salva = lista;
  est[0] = foco; est[1] = aberta; est[2] = editando;
  centrallista_padrao(&lista);
  aberta = 0; editando = 0; previa = 1;
}
static void previaSair(const CentralLista *salva, const int *est) {
  lista = *salva;
  foco = est[0]; aberta = est[1]; editando = est[2];
  previa = 0; previaInverte = -1;
}
float central_previa_altura(void) {
  CentralLista s;
  int e[3];
  float h;
  previaEntrar(&s, e);
  h = altura(1);
  previaSair(&s, e);
  return h;
}
void central_previa_desenhar(float x, float y, float a, int f, int inverte) {
  CentralLista s;
  int e[3];
  previaEntrar(&s, e);
  foco = f;
  previaInverte = inverte;
  // A previa nao passa por central_atualizar, entao as molas ficariam onde
  // a ultima sessao real as deixou. O demo pede o quadro final (dt=0).
  centralPassoFoco(0.0f);
  corpo((GfxRect){ x, y, CC_W, altura(1) }, a, NULL);
  previaSair(&s, e);
}

float central_rotulo_w(void) {
  return (CC_W - 2.0f * CC_PAD - CC_GAP * (CC_COLS - 1)) / CC_COLS - 24.0f;   // o `w` de botao()
}

#ifdef CENTRAL_TESTE
void central_teste_tocando(const char *tit, const char *meta) {
  snprintf(testeTit, sizeof testeTit, "%s", tit ? tit : "");
  snprintf(testeMeta, sizeof testeMeta, "%s", meta ? meta : "");
}
void central_teste_estatisticas(int sim) { testeEstat = sim; }
void central_teste_lista(const char *texto) { carregar(); centrallista_ler(&lista, texto); }
void central_teste_foco(int f, int editar) {
  central_abrir();
  editando = escolhendoCor = 0;
  foco = f < 0 ? botoes(NULL, NULL) : f;
  if (editar) ativarBotao(botoes(NULL, NULL));
}
// Captura do seletor de cores, com o foco no disco `foco`.
void central_teste_cor(int f) { central_abrir(); editando = 0; escolhendoCor = 1; focoCor = f; }
#endif
