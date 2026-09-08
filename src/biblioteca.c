// Biblioteca, alinhada com a tela do app web (MEDIDA rodando, perfil do dono).
//
// ------------------------------------------------------------------------
// O QUE MUDOU, E POR QUE
//
// O port tinha tres pilulas CENTRALIZADAS ("Minha Lista" / "Comprados" /
// "Gêneros") e uma grade de 6 colunas de 212. Medida a tela do web, a estrutura
// e outra e tem QUATRO faixas, todas alinhadas a esquerda em x=96:
//
//   .library-page-title    "Biblioteca" 56/600, letter-spacing 1, em (96,48)
//   .library-page-source   selo "NUVIO" 28/500 rgb(128,128,128) ls 4, a DIREITA
//   .library-view-mode-row y=136: pilulas 150x56 raio 999, 21/400 — "Salvos" e
//                          "Nuvem"; escolhida bg #303030 borda 2px #fff, as
//                          outras bg #222 borda 2px #333
//   .library-picker-row    y=212: DOIS seletores 840x110 raio 36 — "Tipo" e
//                          "Ordenar" —, cada um com rotulo 19/500 rgb(128) e
//                          valor 30/500 branco embaixo, e uma seta a direita
//   .library-grid          6 colunas de 268 (auto-fill com minimo 252 sobre os
//                          1728 uteis, gutter 24), poster 2:3 = 268x402 raio 24
//                          com borda de 4px POR DENTRO, titulo 32/500 a 16 do
//                          poster; passo de linha 487.8
//
// As duas dimensoes do web ("Salvos/Nuvem" e o filtro de Tipo) substituem as
// tres abas inventadas. "Gêneros" nao existe no web e saiu.
//
// Duas decisoes que vieram de erros ja cometidos em outras telas deste app:
//
//   1. A pilula ESCOLHIDA continua marcada quando o foco desce para a grade. Foi
//      o mesmo problema das abas de temporada do detalhe: sem o estado de
//      escolha separado do foco, o usuario perde de vista onde esta.
//   2. A rolagem move o MINIMO para a linha focada caber. Alinhar a linha focada
//      ao topo empurra o cabecalho para fora da tela na primeira descida.
#include "biblioteca.h"
#include "contalib.h"
#include "trakt.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "focus.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "catalogo.h"
#include "idioma.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// Fileiras de foco: 0 = modos (Salvos/Nuvem), 1 = seletores (Tipo/Ordenar),
// 2.. = grade.
#define BIB_FIL_MODO   0
#define BIB_FIL_PICK   1
#define BIB_FIL_GRADE  2
#define BIB_MAX_LINHAS (FOCUS_MAX_FILEIRAS - BIB_FIL_GRADE)
#define BIB_GRADE_BASE (NV_TELA_H - NV_MARGEM_Y)
// Em quantos px um poster desaparece ao subir por baixo do cabecalho. O recorte
// de tesoura resolveria, mas gfx_recorte assume alvo 1:1 com a tela e o Mac em
// retina entrega o dobro; o esmaecimento nao depende do drawable.
#define BIB_FADE       90.0f

// "Salvos" = QUERO VER, e ele tem DUAS fontes que caem na mesma marca
// (CatItem.naLista): a watchlist do Trakt, posta ali pela descoberta, e a
// biblioteca da CONTA (sync_pull_library), posta ali por contalib.c.
// "Coleção" = TENHO, e so o Trakt tem.
enum { MODO_SALVOS, MODO_NUVEM, BIB_N_MODOS };
static const char *ROT_MODO[BIB_N_MODOS] = { "Salvos", "Coleção" };

// Seletor "Tipo": os mesmos valores do web.
enum { TIPO_TODOS, TIPO_FILME, TIPO_SERIE, BIB_N_TIPOS };
static const char *ROT_TIPO[BIB_N_TIPOS] = { "Todos", "Filmes", "Séries" };
// Seletor "Ordenar".
enum { ORD_ADICIONADOS, ORD_TITULO, ORD_ANO, BIB_N_ORD };
static const char *ROT_ORD[BIB_N_ORD] = { "Ordem da lista", "Título: A a Z", "Ano: mais recentes" };

static int modo = MODO_SALVOS;
static int tipo = TIPO_TODOS;
static int ordem = ORD_ADICIONADOS;
static int pickSel = 0;          // qual dos dois seletores esta em foco

static int filtro[CAT_MAX];      // indices do catalogo visiveis
static int nFiltro = 0;
static int totalModo = 0;
static Foco foco;
static float animModo[BIB_N_MODOS];
static float animPick[2];
static float animFoco[BIB_MAX_LINHAS][NV_BIB_COLUNAS];
static float scrollY = 0.0f;
static int sair = 0, pedido = -1;

// Estado de conta. A lista de verdade e a do Trakt, que marca
// ci->naLista/naColecao NO ITEM — nao ha mais tabela por indice aqui: o
// catalogo e reconstruido da rede e um indice guardado aponta para outro titulo
// na volta seguinte. `comprado` sobrevive porque nao ha fonte para ele ainda.
static char comprado[CAT_MAX];

static float alturaLinha(void) {
  // poster + gap + titulo (32/500, lh 1.18 -> 37.8)
  return NV_BIB_POSTER_H + NV_BIB_TIT_GAP + 37.8f;
}
static float passoLinha(void)  { return NV_BIB_LINHA_PASSO; }
static float passoColuna(void) { return NV_BIB_CARD_W + NV_BIB_CARD_GAP; }
static int   nLinhas(void)     { return (nFiltro + NV_BIB_COLUNAS - 1) / NV_BIB_COLUNAS; }

static int ehSerie(const CatItem *ci) {
  return ci && (!strcmp(ci->tipo, "series") || ci->nTemporadas > 0
                || ci->temporada > 0);
}

// Refaz a lista visivel e o mapa de foco. Chamada a cada troca de modo, tipo ou
// ordem porque o numero de colunas da ultima linha muda com o filtro, e um foco
// apontando para uma coluna que nao existe mais desenha um retangulo vazio.
static void reconstruir(void) {
  int n = cat_n();
  if (n > CAT_MAX) n = CAT_MAX;
  nFiltro = 0;
  totalModo = 0;
  for (int i = 0; i < n; i++) {
    const CatItem *ci = cat_item(i);
    if (!ci) continue;
    // "Salvos" = QUERO VER (watchlist do Trakt). "Nuvem" = TENHO (colecao).
    //
    // Os dois modos mostravam quase a MESMA lista: ambos incluiam ci->naLista,
    // entao trocar de pilula praticamente nao mudava nada e as duas nao tinham
    // razao de existir. A divisao agora e a do proprio Trakt, que separa
    // watchlist (o que se pretende ver) de collection (o que se possui) — sao
    // perguntas diferentes e cada pilula responde uma.
    //
    // E le do ITEM, nao mais do vetor naLista[] indexado por posicao. Aquele
    // vetor era um erro conhecido e documentado: o catalogo e RECONSTRUIDO da
    // rede a cada descoberta, entao a posicao 3 de hoje e outro titulo amanha —
    // a marca "salvo" migrava sozinha para um filme que ninguem salvou. A marca
    // tem de viver no item, e vive (CatItem.naLista / .naColecao, preenchidos
    // com a lista de verdade do Trakt).
    int entra = (modo == MODO_SALVOS) ? ci->naLista
                                      : (ci->naColecao || comprado[i]);
    if (!entra) continue;
    totalModo++;
    if (tipo == TIPO_FILME && ehSerie(ci)) continue;
    if (tipo == TIPO_SERIE && !ehSerie(ci)) continue;
    // `hideUnreleasedContent`: sem ano em `meta` o titulo ainda nao estreou do
    // ponto de vista do catalogo, e a preferencia manda escondê-lo.
    if (ajustes_ocultar_nao_lancados() && !ci->meta[0]) continue;
    filtro[nFiltro++] = i;
  }

  // Ordenacao por insercao — sao poucas dezenas de itens, uma vez por troca.
  if (ordem != ORD_ADICIONADOS) {
    for (int i = 1; i < nFiltro; i++) {
      int v = filtro[i], j = i - 1;
      while (j >= 0) {
        const CatItem *a = cat_item(filtro[j]), *b = cat_item(v);
        int maior;
        if (ordem == ORD_TITULO) maior = a && b && strcmp(a->titulo, b->titulo) > 0;
        else /* ORD_ANO, decrescente */
          maior = a && b && strcmp(a->meta, b->meta) < 0;
        if (!maior) break;
        filtro[j + 1] = filtro[j]; j--;
      }
      filtro[j + 1] = v;
    }
  }

  int linhas = nLinhas();
  if (linhas > BIB_MAX_LINHAS) linhas = BIB_MAX_LINHAS;
  int cols[FOCUS_MAX_FILEIRAS];
  cols[BIB_FIL_MODO] = BIB_N_MODOS;
  cols[BIB_FIL_PICK] = 2;
  for (int r = 0; r < linhas; r++) {
    int resto = nFiltro - r * NV_BIB_COLUNAS;
    cols[BIB_FIL_GRADE + r] = resto > NV_BIB_COLUNAS ? NV_BIB_COLUNAS : resto;
  }
  focus_iniciar(&foco, BIB_FIL_GRADE + linhas, cols);
  scrollY = 0.0f;
  memset(animFoco, 0, sizeof animFoco);
}

static int iniciado;
int biblioteca_iniciar(void) {
  // Zerado UMA vez por processo, e nao a cada entrada.
  if (!iniciado) {
    memset(comprado, 0, sizeof comprado);
    iniciado = 1;
  }
  modo = MODO_SALVOS; tipo = TIPO_TODOS; ordem = ORD_ADICIONADOS;
  pickSel = 0;
  memset(animModo, 0, sizeof animModo);
  memset(animPick, 0, sizeof animPick);
  sair = 0; pedido = -1;
  reconstruir();
  // O foco nasce na barra de modos: quem entra ainda esta escolhendo o recorte.
  foco.fileira = BIB_FIL_MODO;
  foco.coluna = modo;
  return 1;
}

void biblioteca_encerrar(void) { }

int biblioteca_na_lista(int i) {
  // A verdade e a marca DO ITEM, que a descoberta preenche com a watchlist do
  // Trakt. O vetor por indice que respondia aqui apontava para outro titulo
  // assim que o catalogo era reconstruido.
  const CatItem *c = cat_item(i);
  return c ? c->naLista : 0;
}
int biblioteca_comprado(int i) { return (i >= 0 && i < CAT_MAX) ? comprado[i] : 0; }
void biblioteca_alternar_lista(int i) {
  // So remonta a lista. Quem vira a marca e cat_definir_na_lista, no mesmo
  // ponto que fala com o Trakt (app.c) — ter DOIS donos do mesmo estado era o
  // que deixava a biblioteca discordando do botao "+" do detalhe.
  (void)i;
  reconstruir();
}

int biblioteca_quer_sair(void) { return sair; }

int biblioteca_pediu_abrir(int *indiceCatalogo) {
  if (pedido < 0) return 0;
  if (indiceCatalogo) *indiceCatalogo = pedido;
  pedido = -1;
  return 1;
}

void biblioteca_evento(const SDL_Event *e) {
  if (e->type != SDL_KEYDOWN) return;
  SDL_Keycode k = e->key.keysym.sym;
  if (k == SDLK_ESCAPE || k == SDLK_AC_BACK || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE) { sair = 1; return; }

  // Barra de modos: esquerda/direita TROCA o modo, e trocar refaz o mapa de
  // foco. Por isso o modo muda AQUI e nao por focus_mover — chamar os dois na
  // ordem errada devolvia o foco para a coluna 0 a cada movimento.
  if (foco.fileira == BIB_FIL_MODO) {
    if (k == SDLK_RIGHT && modo < BIB_N_MODOS - 1) {
      modo++; reconstruir(); foco.fileira = BIB_FIL_MODO; foco.coluna = modo; return;
    }
    if (k == SDLK_LEFT && modo > 0) {
      modo--; reconstruir(); foco.fileira = BIB_FIL_MODO; foco.coluna = modo; return;
    }
    if (k == SDLK_DOWN || k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
      foco.fileira = BIB_FIL_PICK; foco.coluna = pickSel;
    }
    return;
  }

  // Linha de seletores: esquerda/direita anda ENTRE os dois; OK cicla o valor do
  // que esta em foco. O web abre um menu suspenso; num D-pad, ciclar no proprio
  // seletor poupa a viagem de ida e volta ate a lista.
  if (foco.fileira == BIB_FIL_PICK) {
    if (k == SDLK_RIGHT && pickSel == 0) { pickSel = 1; foco.coluna = 1; return; }
    if (k == SDLK_LEFT  && pickSel == 1) { pickSel = 0; foco.coluna = 0; return; }
    if (k == SDLK_UP)   { foco.fileira = BIB_FIL_MODO; foco.coluna = modo; return; }
    if (k == SDLK_DOWN) { if (nFiltro) focus_mover_grade(&foco, 0, 1); return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
      if (pickSel == 0) tipo = (tipo + 1) % BIB_N_TIPOS;
      else              ordem = (ordem + 1) % BIB_N_ORD;
      reconstruir();
      foco.fileira = BIB_FIL_PICK; foco.coluna = pickSel;
    }
    return;
  }

  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    int i = (foco.fileira - BIB_FIL_GRADE) * NV_BIB_COLUNAS + foco.coluna;
    if (i >= 0 && i < nFiltro) pedido = filtro[i];
    return;
  }
  // A grade da biblioteca e uma GRADE: manter a coluna ao subir e descer, e
  // nao voltar para a coluna onde o cursor esteve por ultimo naquela linha.
  if (k == SDLK_RIGHT)     focus_mover_grade(&foco, 1, 0);
  else if (k == SDLK_LEFT) focus_mover_grade(&foco, -1, 0);
  else if (k == SDLK_DOWN) focus_mover_grade(&foco, 0, 1);
  else if (k == SDLK_UP) {
    if (foco.fileira == BIB_FIL_GRADE) { foco.fileira = BIB_FIL_PICK; foco.coluna = pickSel; }
    else focus_mover_grade(&foco, 0, -1);
  }
}

void biblioteca_atualizar(float dt, Uint32 agora) {
  (void)agora;
  for (int a = 0; a < BIB_N_MODOS; a++) {
    float alvo = (foco.fileira == BIB_FIL_MODO && foco.coluna == a) ? 1.0f : 0.0f;
    animModo[a] = anim_mola(animModo[a], alvo, dt,
                            alvo > animModo[a] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  for (int p = 0; p < 2; p++) {
    float alvo = (foco.fileira == BIB_FIL_PICK && foco.coluna == p) ? 1.0f : 0.0f;
    animPick[p] = anim_mola(animPick[p], alvo, dt,
                            alvo > animPick[p] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
  }
  int linhas = nLinhas();
  if (linhas > BIB_MAX_LINHAS) linhas = BIB_MAX_LINHAS;
  for (int r = 0; r < linhas; r++)
    for (int c = 0; c < NV_BIB_COLUNAS; c++) {
      float alvo = focus_indice(&foco, BIB_FIL_GRADE + r, c) ? 1.0f : 0.0f;
      animFoco[r][c] = anim_mola(animFoco[r][c], alvo, dt,
                                 alvo > animFoco[r][c] ? NV_MOLA_FOCO : NV_MOLA_DESFOCO);
    }

  // Rola o MINIMO para a linha focada caber inteira na area util. Com o foco no
  // cabecalho o alvo e 0 — voltar ao topo faz parte de voltar para a barra.
  float alvo = scrollY;
  if (foco.fileira >= BIB_FIL_GRADE) {
    float topo = NV_BIB_GRADE_Y + (foco.fileira - BIB_FIL_GRADE) * passoLinha();
    float base = topo + alturaLinha();
    if (base - alvo > BIB_GRADE_BASE)  alvo = base - BIB_GRADE_BASE;
    if (topo - alvo < NV_BIB_GRADE_Y)  alvo = topo - NV_BIB_GRADE_Y;
  } else {
    alvo = 0.0f;
  }
  if (alvo < 0.0f) alvo = 0.0f;
  scrollY = anim_mola(scrollY, alvo, dt, NV_MOLA_SCROLL);
}

// Pilula de modo. Escolhida sem foco fica com fundo #303030 e borda branca; com
// foco clareia e o texto escurece. As duas leituras tem de continuar distintas —
// e o erro que as abas de temporada do detalhe ja cometeram.
static void desenhaModo(int a, float f) {
  GfxRect r = { NV_BIB_X + a * NV_BIB_MODO_PASSO, NV_BIB_MODO_Y,
                NV_BIB_MODO_W, NV_BIB_MODO_H };
  int sel = (a == modo);
  float raio = NV_RAIO_PILL;
  if (f > 0.02f) {
    GfxRect b = { r.x - 2.0f, r.y - 2.0f, r.w + 4.0f, r.h + 4.0f };
    gfx_cor(b, raio, 0.961f, 0.961f, 0.961f, f);
  }
  float lum = sel ? 0.188f : 0.133f;        // #303030 contra #222
  gfx_cor(r, raio, lum, lum, lum, 1.0f);
  if (sel && f < 0.98f)
    gfx_rect(r, 0, GFX_ANEL, 0, 1.0f / r.h, 0, raio,
             0.70f, 0.70f, 0.72f, 1.0f - f);
  int cor = 245;
  TxtLinha l = txt_linha(TXT_CAPTION2, ROT_MODO[a], cor, cor, cor, 255);
  txt_desenhar_alpha(l, r.x + (r.w - l.w) * 0.5f, r.y + (r.h - l.h) * 0.5f,
                     sel ? 1.0f : 0.82f);
}

// Seletor "Tipo" / "Ordenar": rotulo pequeno em cinza e valor grande em branco,
// com a seta encostada na direita.
static void desenhaPicker(int p, float f) {
  GfxRect r = { NV_BIB_X + p * NV_BIB_PICK_PASSO, NV_BIB_PICK_Y,
                NV_BIB_PICK_W, NV_BIB_PICK_H };
  float raio = NV_BIB_PICK_RAIO / (NV_BIB_PICK_H * 0.5f) * 0.5f;
  if (f > 0.02f) {
    GfxRect b = { r.x - 2.0f, r.y - 2.0f, r.w + 4.0f, r.h + 4.0f };
    gfx_cor(b, raio, 0.961f, 0.961f, 0.961f, f);
  }
  float lum = anim_mistura(0.133f, 0.188f, f);   // #222 -> #303030 no foco
  gfx_cor(r, raio, lum, lum, lum, 1.0f);

  const char *rot = (p == 0) ? "Tipo" : "Ordenar";
  const char *val = (p == 0) ? ROT_TIPO[tipo] : ROT_ORD[ordem];
  // 19/500 rgb(128,128,128) em cima, 30/500 branco embaixo com 4 de folga.
  TxtLinha tr = txt_linha(TXT_MINI, rot, 179, 179, 179, 255);
  TxtLinha tv = txt_linha(TXT_CALLOUT, val, 255, 255, 255, 255);
  float tx = r.x + NV_BIB_PICK_PADX;
  float ty = r.y + NV_BIB_PICK_PADY;
  txt_desenhar_alpha(tr, tx, ty, 0.95f);
  txt_desenhar_alpha(tv, tx, ty + tr.h + 4.0f, 1.0f);

  TxtLinha seta = txt_linha(TXT_CAPTION2, "OK: alterar", 196, 197, 202, 255);
  txt_desenhar_alpha(seta, r.x + r.w - NV_BIB_PICK_PADX - seta.w,
                     r.y + (r.h - seta.h) * 0.5f, 0.85f);
}

// O selo de origem e desenhado em CAIXA ALTA (letter-spacing 4, como no web).
// "TRAKT" e "LOCAL" ja sao caixa alta em qualquer idioma; "Conta" nao.
//
// POR QUE NAO ESCREVER "CONTA" DIRETO. A chave da tabela de traducao e o
// portugues como ele se escreve, e "Conta" ja esta la (idioma_tab.h -> a
// "Account"). Um literal "CONTA" seria uma SEGUNDA chave para a mesma palavra,
// que alguem teria de lembrar de traduzir de novo — e tools/varredura-i18n.py
// acusa exatamente isso. Traduz primeiro, levanta a caixa depois.
//
// A subida e so de a..z: as duas traducoes de hoje sao ASCII, e uma letra
// acentuada que nao subir fica minuscula em vez de virar outra letra — que e o
// que aconteceria mexendo em bytes de UTF-8 um a um.
static const char *seloCaixaAlta(const char *chave) {
  static char buf[32];
  const char *s = i18n(chave);
  size_t k = 0;
  for (; s[k] && k + 1 < sizeof buf; k++)
    buf[k] = (s[k] >= 'a' && s[k] <= 'z') ? (char)(s[k] - 'a' + 'A') : s[k];
  buf[k] = 0;
  return buf;
}

// Estado vazio: 46/500 branco e 28/400 rgb(179,179,179), centrado na largura
// util. Uma grade em branco parece tela quebrada.
static void desenhaVazio(void) {
  const char *l1 = totalModo ? "Nenhum título neste filtro"
      : modo == MODO_NUVEM ? "Sua coleção aparece aqui" : "Sua próxima sessão começa aqui";
  const char *l2 = totalModo
      ? "Em Tipo, escolha Todos. Confira também os filtros em Ajustes."
      : modo == MODO_NUVEM
        ? "Os filmes e séries da sua coleção no Trakt ficam reunidos nesta aba."
        : "Abra um filme ou série e escolha Adicionar à lista para guardar.";
  TxtLinha t1 = txt_linha(TXT_TITULO2, l1, 255, 255, 255, 255);
  TxtLinha t2 = txt_linha(TXT_CALLOUT, l2, 179, 179, 179, 255);
  float cx = NV_BIB_X + NV_BIB_W * 0.5f;
  float y = NV_BIB_VAZIO_Y + 190.0f;
  gfx_icone((GfxRect){cx - 32.0f, y - 100.0f, 64.0f, 64.0f},
             "menu_library", 0.70f, 0.70f, 0.72f, 1.0f);
  txt_desenhar_alpha(t1, cx - t1.w * 0.5f, y, 0.96f);
  txt_desenhar_alpha(t2, cx - t2.w * 0.5f, y + t1.h + 18.0f, 0.85f);
  TxtLinha dica = txt_linha(TXT_CAPTION2,
      "↑ Voltar aos filtros   ·   Voltar: menu", 179, 179, 179, 255);
  txt_desenhar(dica, cx - dica.w * 0.5f, y + t1.h + t2.h + 58.0f);
}

void biblioteca_desenhar(Uint32 agora) {
  (void)agora;
  // Fundo opaco proprio: a biblioteca cobre a tela inteira e nao pode contar com
  // quem desenhou antes dela.
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  // A tela ja foi limpa com ESTA MESMA COR por glClearColor/glClear em
  // main.c antes de app_desenhar. Pintar por cima era uma camada de tela
  // cheia jogada fora por quadro — e o custo dominante nesta GPU e fill
  // rate (gfx.c registra que DUAS camadas de tela cheia derrubavam a
  // Mali-G71 para ~40fps). Nao repor sem antes mudar a cor do clear.
  (void)tela;

  TxtLinha tit = txt_linha(TXT_TITULO2, "Biblioteca", 255, 255, 255, 255);
  txt_desenhar(tit, NV_BIB_X, NV_BIB_Y);
  // Selo de origem, alinhado a direita da area util. Espacado de proposito: no
  // web ele tem letter-spacing 4 e le como etiqueta, nao como palavra.
  //
  // O SELO DIZ A ORIGEM DE VERDADE. Estava cravado em "NUVIO", que e o nome do
  // app e nao a fonte dos dados — a lista vem do TRAKT, e o log confirma
  // ("[trakt] credencial carregada", "[trakt] watchlist: 118"). Selo de origem
  // que nao reflete a origem e da mesma familia da classificacao "14" e do
  // elenco de demonstracao: informacao inventada com cara de dado.
  //
  // Sem credencial do Trakt a biblioteca e local, e o selo diz isso.
  //
  // AGORA ELE E POR MODO, e nao um selo so para a tela inteira. Os dois modos
  // tem fontes diferentes: "Salvos" pode vir da CONTA (sync_pull_library, via
  // contalib.c) e "Coleção" so existe no Trakt. Um selo unico acabava dizendo
  // "LOCAL" sobre uma lista que veio da conta — que foi exatamente o relato
  // ("Top right of library says 'Local'").
  //
  // "LOCAL" tambem cobre "a conta ainda nao respondeu". Nao ha estado
  // intermediario desenhado de proposito: o selo diz de onde veio o que ESTA na
  // tela, e enquanto a conta nao respondeu o que esta na tela e local.
  //
  // COM CONTA E TRAKT AO MESMO TEMPO, "Salvos" e uma MISTURA das duas listas e
  // nenhum rotulo unico e completo. O web nao tem esse caso (la a fonte e um
  // seletor, `sourceMode`, e a lista e uma so), entao a ordem dele
  // (libraryController.getSourceLabel) nao ajuda a decidir. Fica a conta na
  // frente: e ela que responde a pergunta que o selo existe para responder —
  // "isto acompanha meus outros aparelhos?".
  { const char *fonte = modo == MODO_NUVEM
        ? (trakt_ativo() ? "TRAKT" : "LOCAL")
        : (contalib_tem_conta() ? seloCaixaAlta("Conta")
                                : trakt_ativo() ? "TRAKT" : "LOCAL");
    float wSelo = txt_tracking(TXT_CALLOUT, fonte, 128, 128, 128,
                               -1.0f, 0.0f, 0.0f, 4.0f);
    txt_tracking(TXT_CALLOUT, fonte, 128, 128, 128,
                 NV_BIB_DIR - wSelo, NV_BIB_Y + 10.0f, 0.9f, 4.0f); }

  for (int a = 0; a < BIB_N_MODOS; a++) desenhaModo(a, animModo[a]);
  {
    char resumo[160];
    // AS PARTES passam por i18n, e nao a frase pronta: a chave da tabela e o
    // portugues INTEIRO de uma string, e "0 títulos   ·   Sua lista para
    // assistir" nunca vai existir como chave. Era este o defeito visto na
    // biblioteca com a interface em inglês (issue #3) — o resto da tela
    // traduzia porque cada texto chega em text.c sozinho.
    snprintf(resumo, sizeof resumo, "%d %s   ·   %s", nFiltro,
             i18n(nFiltro == 1 ? "título" : "títulos"),
             i18n(modo == MODO_SALVOS ? "Sua lista para assistir"
                                      : "Sua coleção no Trakt"));
    TxtLinha info = txt_linha(TXT_CAPTION2, resumo, 179, 179, 179, 255);
    txt_desenhar(info, NV_BIB_DIR - info.w,
                 NV_BIB_MODO_Y + (NV_BIB_MODO_H - info.h) * 0.5f);
  }
  for (int p = 0; p < 2; p++)           desenhaPicker(p, animPick[p]);

  if (nFiltro == 0) { desenhaVazio(); return; }

  int linhas = nLinhas();
  if (linhas > BIB_MAX_LINHAS) linhas = BIB_MAX_LINHAS;
  float passoC = passoColuna(), passoL = passoLinha();

  // Dois passes: o item focado escala 2% e precisa ser desenhado por ULTIMO,
  // senao o vizinho da direita corta a borda dele.
  for (int passe = 0; passe < 2; passe++)
    for (int r = 0; r < linhas; r++) {
      float topo = NV_BIB_GRADE_Y + r * passoL - scrollY;
      if (topo > NV_TELA_H || topo + alturaLinha() < -80.0f) continue;
      // O que sobe para baixo do cabecalho some antes de cruza-lo: sem o
      // esmaecimento, poster e seletor se leem um sobre o outro.
      float a = anim_clamp((topo - (NV_BIB_PICK_Y + NV_BIB_PICK_H * 0.5f)) / BIB_FADE,
                           0.0f, 1.0f);
      if (a <= 0.005f) continue;

      for (int c = 0; c < NV_BIB_COLUNAS; c++) {
        int i = r * NV_BIB_COLUNAS + c;
        if (i >= nFiltro) break;
        float f = animFoco[r][c];
        if ((passe == 0) == (f > 0.01f)) continue;

        // `.library-grid-card.focused { transform: scale(1.02) }` com origem no
        // TOPO — e a unica escala de foco que o web tem, e ela e de 2%, nao dos
        // 14% que estavam aqui (numero das tabelas de Top Shelf do tvOS).
        float esc = 1.0f + NV_BIB_FOCO_ESCALA * f;
        float bw = NV_BIB_CARD_W * esc, bh = NV_BIB_POSTER_H * esc;
        float bx = NV_BIB_X + c * passoC - (bw - NV_BIB_CARD_W) * 0.5f;
        GfxRect card = { bx, topo, bw, bh };
        float raio = 24.0f / NV_BIB_CARD_W;

        const CatItem *ci = cat_item(filtro[i]);
        const char *arte = (ci && ci->poster[0]) ? ci->poster : NULL;
        GLuint tex = arte ? tex_obter(arte) : 0;
        if (tex) {
          gfx_tex_aspect_atual = tex_aspecto(arte);
          gfx_rect(card, tex, GFX_CARD, f, 0.0f, 0.0f, raio, 0, 0, 0, a);
          gfx_tex_aspect_atual = 0.0f;
        } else {
          // Placeholder na cor dos cards: o poster ainda esta decodificando em
          // outra thread e a grade nao pode piscar buraco.
          // Esqueleto VISIVEL, o mesmo da home: #2C2C2C. Ver a nota la — placeholder
            // do tom do fundo le como card quebrado, nao como carregando.
            gfx_cor(card, raio, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
                  NV_COR_ESQUELETO_B, a);
        }
        // A borda de foco do web e de 4px POR DENTRO do poster (o card ja
        // reserva `border: 4px solid transparent`), e nao um halo por fora —
        // "Android TV uses the inside focus border, not an outer halo", diz o
        // proprio comentario da folha.
        if (f > 0.01f) {
          gfx_rect(card, 0, GFX_ANEL, 0, NV_BIB_POSTER_BORDA / card.w,
                   0, raio, 0.961f, 0.961f, 0.961f, f * a);
        }

        // Titulo 32/500, uma linha, cortado com reticencias — o web usa
        // white-space:nowrap + text-overflow:ellipsis.
        if (ci) {
          TxtLinha tl = txt_linha_corta(TXT_CALLOUT, ci->titulo, 255, 255, 255, 255,
                                        NV_BIB_CARD_W);
          txt_desenhar_alpha(tl, NV_BIB_X + c * passoC,
                             topo + NV_BIB_POSTER_H + NV_BIB_TIT_GAP, a * 0.98f);
        }
      }
    }
}
