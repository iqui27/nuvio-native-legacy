// Ver teclado.h para por que esta modal existe e o que ela NAO tenta ser.
#include "teclado.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "idioma.h"
#include <stdio.h>
#include <string.h>

// AS MESMAS MEDIDAS DA GRADE DA BUSCA (busca.c): tecla de 74 com vao de 12, e
// o mesmo crescimento de 10% no foco. Nao e coincidencia nem copia — e o
// tamanho ja medido para uma tecla que se acerta com o D-pad a tres metros, e
// duas grades do mesmo app com teclas de tamanhos diferentes leem como dois
// aplicativos.
#define TE_TECLA     74.0f
#define TE_GAP       12.0f
#define TE_COLS       6
// ALFABETO LONGO GANHA COLUNAS, NAO FILEIRAS (#88). O de usuario/senha Xtream
// tem 73 simbolos: em 6 colunas pedia 13 fileiras, o teto e 7 de caractere
// (8 com apagar/limpar/pronto = 1080 px de modal, a tela inteira), e tudo
// depois do indice 42 — 'Q' em diante, TODOS os digitos — ficava fora da grade,
// nem desenhado nem alcancavel. O usuario fotografou o teclado cortado em "P".
//
// 13 e metade do alfabeto latino: a grade le a-m / n-z / A-M / N-Z /
// 0-9._- / @!#$%&*+=, cada fileira uma metade de algo que a pessoa ja conhece,
// e cabe em 6 fileiras — a MESMA altura do teclado padrao (994 px). Largura:
// 13 x 74 + 12 x 12 = 1106 de grade, 1194 de modal, folga de 363 de cada lado
// em 1920. Rolagem foi descartada: com tudo a vista nao ha "tem mais embaixo"
// para a pessoa adivinhar, e cada tecla a mais de distancia no D-pad custa o
// mesmo que numa grade rolada.
//
// So entra acima de 42 (7 x 6): padrao (36), portal (39) e MAC (17) continuam
// em 6 colunas, pixel por pixel o layout de antes.
#define TE_COLS_LONGO 13
// Teto de colunas: alfabeto que nem em 13 x 7 coubesse (91+) alarga ate aqui,
// 20 x 74 + 19 x 12 = 1708 + 88 = 1796 < 1920. Acima de 140 simbolos o resto
// fica de fora, como ficava antes — nenhum chamador chega perto.
#define TE_COLS_MAX  20
// TETO das fileiras de caractere + a de apagar/limpar/pronto. O alfabeto
// PADRAO ocupa 6 (36 caracteres); o do portal IPTV precisa de ponto, dois
// pontos e hifen alem de a-z0-9, e nao cabe em 36. Quem abre escolhe o
// alfabeto, e a modal se ajusta — as fileiras de verdade sao `nFileiras`.
#define TE_FILEIRAS_MAX 8
#define TE_FILEIRAS_PAD 7          // 6 de a-z0-9 + 1 de apagar/limpar/pronto
#define TE_PASSO     (TE_TECLA + TE_GAP)

#define TE_ESCALA     0.10f

// Caixa de um caractere digitado. 6 delas com vao de 12 cabem nos 504 da
// grade de 6 colunas, e a caixa por caractere e o que torna o codigo DITAVEL ao telefone:
// separada, ninguem confunde "rn" com "m" nem conta letra errada.
#define TE_CX        76.0f
#define TE_CY        92.0f
#define TE_CGAP      12.0f
// Piso de largura da caixa por caractere: abaixo disto o glifo de TXT_TITULO2
// nao cabe e as letras se sobrepoem. Medido na captura do dono com maxN=24,
// onde a conta dava 9,5 px por caixa.
#define TE_CX_MIN    44.0f
#define TE_CAMPO_PAD 18.0f

#define TE_PAD       44.0f
// TITULO (46) + DICA em ate duas linhas (2 x 28) + CAIXAS (92) + folgas.
//
// ERA 176, DE CABECA, e a primeira captura mostrou o resultado: as caixas do
// que foi digitado nasciam ACIMA da linha de dica e as duas se sobrepunham. A
// altura do cabecalho de uma modal nao se estima — ela e a soma do que esta
// dentro dela.
#define TE_DICA_Y    (TE_PAD + 50.0f)
#define TE_DICA_H     56.0f
#define TE_CAIXA_Y   (TE_DICA_Y + TE_DICA_H + 10.0f)
// 44 DE FOLGA ATE A GRADE, e nao 26: as caixas do que foi digitado tem a
// mesma largura das teclas, e coladas nelas a primeira captura leu como uma
// SETIMA FILEIRA do teclado em vez de "o que voce ja digitou".
#define TE_CAB       (TE_CAIXA_Y + TE_CY + 44.0f)
#define TE_RODAPE    64.0f
#define TE_W        (gradeW() + TE_PAD * 2.0f)

#define TE_X        ((NV_TELA_W - TE_W) * 0.5f)


static const char *ALFABETO = "abcdefghijklmnopqrstuvwxyz0123456789";

static int   aberto, fileira, coluna;
// Coluna de caractere de onde o foco desceu para apagar/limpar/pronto. Sem ela,
// subir de "pronto" (coluna 2) numa grade de 13 caia no 'c', a dez teclas de
// onde a pessoa estava; com ela, volta para a mesma tecla.
static int   colunaAntes;
static float anim, focoAnim[TE_FILEIRAS_MAX][TE_COLS_MAX];
static const char *alfabetoAtual = NULL;   // NULL = o padrao
static int   nFileiras = TE_FILEIRAS_PAD;
static int   nCols = TE_COLS;

static float gradeW(void) {
  return (float)nCols * TE_TECLA + (float)(nCols - 1) * TE_GAP;   // 504 com 6
}

// A altura da modal depende de quantas fileiras o alfabeto pediu, entao as tres
// medidas que dela dependem viraram funcao. Continuam sendo a mesma conta.
static float gradeH(void) {
  return (float)nFileiras * TE_TECLA + (float)(nFileiras - 1) * TE_GAP;
}
static float teH(void) { return TE_CAB + gradeH() + TE_RODAPE + TE_PAD; }
static float teY(void) { return (NV_TELA_H - teH()) * 0.5f; }
static const char *alfa(void) { return alfabetoAtual ? alfabetoAtual : ALFABETO; }
static char  texto[TECLADO_MAX + 1];
static int   n, maxN, resultado;
static char  tituloAtual[96], dicaAtual[160];

const char *teclado_alfabeto(void) { return ALFABETO; }
int teclado_aberto(void) { return aberto; }
const char *teclado_texto(void) { return texto; }

int teclado_resultado(void) {
  int r = resultado;
  resultado = TECLADO_NADA;
  return r;
}

void teclado_abrir(const char *titulo, const char *dica, int max) {
  teclado_abrir_com(titulo, dica, max, NULL, NULL);
}

void teclado_abrir_com(const char *titulo, const char *dica, int max,
                       const char *alfabeto, const char *inicial) {
  int letras;
  alfabetoAtual = (alfabeto && *alfabeto) ? alfabeto : NULL;
  letras = (int)strlen(alfa());
  // Colunas: 6, ou 13 quando 7 fileiras de 6 nao bastam (ver TE_COLS_LONGO),
  // ou o que fizer caber em 7 fileiras, ate TE_COLS_MAX.
  nCols = TE_COLS;
  if (letras > (TE_FILEIRAS_MAX - 1) * TE_COLS) {
    nCols = TE_COLS_LONGO;
    if (letras > (TE_FILEIRAS_MAX - 1) * nCols)
      nCols = (letras + TE_FILEIRAS_MAX - 2) / (TE_FILEIRAS_MAX - 1);
    if (nCols > TE_COLS_MAX) nCols = TE_COLS_MAX;
  }
  // Fileiras de caractere = quantas o alfabeto pede, arredondando para cima,
  // mais a de apagar/limpar/pronto. O teto existe porque `focoAnim` e vetor
  // fixo e porque uma grade mais alta que isto nao cabe na tela.
  nFileiras = (letras + nCols - 1) / nCols + 1;
  if (nFileiras > TE_FILEIRAS_MAX) nFileiras = TE_FILEIRAS_MAX;
  if (nFileiras < 2) nFileiras = 2;
  aberto = 1;
  fileira = 0; coluna = 0; colunaAntes = 0;
  resultado = TECLADO_NADA;
  maxN = max > 0 && max <= TECLADO_MAX ? max : TECLADO_MAX;
  // TEXTO INICIAL: editar um portal ja cadastrado nao pode obrigar a redigitar
  // o endereco inteiro. Cortado em maxN, nunca truncado no meio de nada porque
  // o alfabeto e de um byte por caractere.
  snprintf(texto, sizeof texto, "%s", inicial ? inicial : "");
  texto[maxN] = 0;
  n = (int)strlen(texto);
  snprintf(tituloAtual, sizeof tituloAtual, "%s", titulo ? titulo : "");
  snprintf(dicaAtual,   sizeof dicaAtual,   "%s", dica   ? dica   : "");
  memset(focoAnim, 0, sizeof focoAnim);
}

static int colunasDe(int f) {
  int n;
  if (f >= nFileiras - 1) return 3;         // apagar / limpar / pronto
  // A ULTIMA FILEIRA DE CARACTERE PODE SER PARCIAL: um alfabeto de 39 enche
  // seis colunas em seis fileiras e deixa tres na setima. Sem isto o foco
  // entraria em celula vazia e "digitaria" o byte depois do fim da string.
  n = (int)strlen(alfa()) - f * nCols;
  return n > nCols ? nCols : (n > 0 ? n : 1);
}

static GfxRect retangulo(int f, int c) {
  GfxRect r;
  r.y = teY() + TE_CAB + (float)f * TE_PASSO;
  r.h = TE_TECLA;
  if (f < nFileiras - 1) {
    r.x = TE_X + TE_PAD + (float)c * TE_PASSO;
    r.w = TE_TECLA;
  } else {
    r.w = (gradeW() - 2.0f * TE_GAP) / 3.0f;
    r.x = TE_X + TE_PAD + (float)c * (r.w + TE_GAP);
  }
  return r;
}

// O rotulo da ultima fileira. As tres teclas sao o unico ponto da modal com
// palavra em vez de caractere, e por isso as tres estao na tabela de i18n.
static const char *rotuloExtra(int c) {
  return c == 0 ? "apagar" : (c == 1 ? "limpar" : "pronto");
}

static void aplicar(void) {
  if (fileira < nFileiras - 1) {
    if (n < maxN) { texto[n++] = alfa()[fileira * nCols + coluna]; texto[n] = 0; }
    return;
  }
  if (coluna == 0) { if (n > 0) texto[--n] = 0; return; }
  if (coluna == 1) { n = 0; texto[0] = 0; return; }
  // "pronto" com o campo vazio nao e uma confirmacao de nada: quem chega ali
  // sem digitar quis olhar o teclado, e fechar a modal com resultado PRONTO
  // mandaria a tela de amigos tentar vincular uma string vazia.
  if (n < 1) return;
  aberto = 0;
  resultado = TECLADO_PRONTO;
}

void teclado_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    aberto = 0;
    resultado = TECLADO_CANCELOU;
    return;
  }
  if (k == SDLK_UP) {
    if (fileira > 0) {
      if (fileira == nFileiras - 1) coluna = colunaAntes;
      fileira--;
    }
    if (coluna >= colunasDe(fileira)) coluna = colunasDe(fileira) - 1;
    return;
  }
  if (k == SDLK_DOWN) {
    if (fileira + 1 < nFileiras) {
      if (fileira + 1 == nFileiras - 1) colunaAntes = coluna;
      fileira++;
    }
    if (coluna >= colunasDe(fileira)) coluna = colunasDe(fileira) - 1;
    return;
  }
  if (k == SDLK_LEFT)  { if (coluna > 0) coluna--; return; }
  if (k == SDLK_RIGHT) { if (coluna + 1 < colunasDe(fileira)) coluna++; return; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (e->key.repeat) return;   // manter OK apertado nao digita a mesma letra
    aplicar();
    return;
  }
}

void teclado_atualizar(float dt, Uint32 agora) {
  int f, c;
  (void)agora;
  if (!aberto && anim < 0.002f) { anim = 0.0f; return; }
  anim = ajustes_animacoes_reduzidas()
           ? (aberto ? 1.0f : 0.0f)
           : anim_mola(anim, aberto ? 1.0f : 0.0f, dt, NV_MOLA_TELA);
  for (f = 0; f < nFileiras; f++)
    for (c = 0; c < nCols; c++) {
      float alvo = (aberto && f == fileira && c == coluna) ? 1.0f : 0.0f;
      focoAnim[f][c] = ajustes_animacoes_reduzidas()
        ? alvo : anim_mola(focoAnim[f][c], alvo, dt, NV_MOLA_FOCO);
    }
}

void teclado_desenhar(Uint32 agora) {
  float a = anim_suave(anim), dy, x, y;
  int f, c, i;
  if (anim < 0.01f) return;
  dy = (1.0f - a) * 36.0f;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.76f * anim);
  { GfxRect p = { TE_X, teY() + dy, TE_W, teH() };
    gfx_cor(p, 24.0f / teH(), 0.075f, 0.078f, 0.088f, 0.99f * a); }

  x = TE_X + TE_PAD;
  y = teY() + dy + TE_PAD;
  { TxtLinha t = txt_linha(TXT_HEADLINE, tituloAtual, 245, 248, 255, 255);
    txt_desenhar_alpha(t, x, y, a); }
  // EM BLOCO: a dica nao cabe numa linha de 504px em portugues, e na captura
  // ela saiu terminando em "...que ele te...".
  if (dicaAtual[0])
    txt_bloco(TXT_CAPTION2, dicaAtual, 160, 164, 175,
              x, teY() + dy + TE_DICA_Y, gradeW(), 28.0f, a * 0.9f, 2);

  // O QUE FOI DIGITADO — em CAIXAS ou em LINHA, e quem decide e a conta, nao
  // quem chamou.
  //
  // Uma caixa por caractere so funciona enquanto a caixa couber o glifo. Com
  // maxN = 4 (o codigo do amigo) cada caixa tem 117 px e a fileira de casas
  // vazias diz "faltam tres" sem precisar de frase nenhuma. Com maxN = 24 (a
  // busca de listas publicas) a mesma conta da 9,5 px por caixa, e o glifo de
  // TXT_TITULO2 tem mais de 30: as letras se sobrepunham umas nas outras e o
  // dono fotografou o resultado — tres "a" viraram uma mancha em cima de uma
  // cerca de barrinhas.
  //
  // Entao: caixa so quando ela cabe o glifo (TE_CX_MIN), senao CAMPO DE TEXTO
  // com cursor, que e a forma certa para texto livre de qualquer tamanho. E a
  // fileira de casas vazias nao faz falta aqui — numa busca nao ha numero de
  // caracteres a completar.
  if ((gradeW() - (float)(maxN - 1) * TE_CGAP) / (float)maxN >= TE_CX_MIN) {
    float bw = (gradeW() - (float)(maxN - 1) * TE_CGAP) / (float)maxN;
    float bx, by = teY() + dy + TE_CAIXA_Y;
    if (bw > TE_CX) bw = TE_CX;
    bx = TE_X + (TE_W - ((float)maxN * bw + (float)(maxN - 1) * TE_CGAP)) * 0.5f;
    for (i = 0; i < maxN; i++) {
      GfxRect b = { bx, by, bw, TE_CY };
      char ch[2];
      // 0.055 do menor lado, como NV_RAIO_CARD: o raio do gfx_cor e FRACAO,
      // nao pixel, e um 12 aqui viraria uma pilula.
      // A CHEIA BEM MAIS CLARA QUE A VAZIA, pelo mesmo motivo: com 0,13
      // contra 0,09 de uma tecla em repouso, cheia e vazia eram a mesma
      // mancha a tres metros.
      gfx_cor(b, NV_RAIO_CARD, 1.0f, 1.0f, 1.0f, (i < n ? 0.22f : 0.04f) * a);
      if (i < n) {
        TxtLinha t;
        ch[0] = texto[i]; ch[1] = 0;
        t = txt_linha(TXT_TITULO2, ch, 246, 248, 255, 255);
        txt_desenhar_alpha(t, b.x + (b.w - t.w) * 0.5f,
                           b.y + (b.h - t.h) * 0.5f, a);
      }
      bx += bw + TE_CGAP;
    }
  } else {
    GfxRect campo = { TE_X + TE_PAD, teY() + dy + TE_CAIXA_Y,
                      gradeW(), TE_CY };
    float tx = campo.x + TE_CAMPO_PAD, cursorX = tx;
    gfx_cor(campo, NV_RAIO_CARD, 1.0f, 1.0f, 1.0f, 0.07f * a);
    if (n) {
      TxtLinha t = txt_linha(TXT_TITULO2, texto, 246, 248, 255, 255);
      // TEXTO MAIS LARGO QUE O CAMPO ROLA PELO FIM, nao pelo comeco: quem
      // digita precisa ver a ultima letra que apertou, nao a primeira.
      float larg = campo.w - TE_CAMPO_PAD * 2.0f;
      float ox = t.w > larg ? t.w - larg : 0.0f;
      gfx_recorte(campo.x + TE_CAMPO_PAD, campo.y,
                  larg, campo.h);
      txt_desenhar_alpha(t, tx - ox, campo.y + (campo.h - t.h) * 0.5f, a);
      gfx_sem_recorte();
      cursorX = tx + (t.w - ox);
    }
    // CURSOR SEM PISCA-PISCA quando as animacoes estao reduzidas — piscar e
    // movimento, e a regra vale aqui como vale no resto do app.
    { float op = ajustes_animacoes_reduzidas()
                   ? 0.85f
                   : 0.35f + 0.5f * (((agora / 500) % 2) ? 0.0f : 1.0f);
      GfxRect cur = { cursorX + 3.0f, campo.y + 22.0f, 3.0f, campo.h - 44.0f };
      if (cur.x > campo.x + campo.w - TE_CAMPO_PAD)
        cur.x = campo.x + campo.w - TE_CAMPO_PAD;
      gfx_cor(cur, 0.5f, 0.95f, 0.96f, 0.99f, op * a); }
  }

  for (f = 0; f < nFileiras; f++) {
    for (c = 0; c < colunasDe(f); c++) {
      float k = focoAnim[f][c];
      GfxRect base = retangulo(f, c);
      float esc = 1.0f + TE_ESCALA * k;
      GfxRect t;
      const char *s;
      char ch[2];
      int tom;
      base.y += dy;
      t.w = base.w * esc; t.h = base.h * esc;
      t.x = base.x - (t.w - base.w) * 0.5f;
      t.y = base.y - (t.h - base.h) * 0.5f;
      // INVERTE no foco, como a grade da busca: a tres metros, numa grade de
      // 39 alvos iguais, a inversao e o unico contraste que se ve de relance.
      gfx_cor(t, NV_RAIO_CARD, 1.0f, 1.0f, 1.0f, anim_mistura(0.09f, 1.0f, k) * a);
      if (f < nFileiras - 1) {
        ch[0] = alfa()[f * nCols + c]; ch[1] = 0;
        s = ch;
      } else {
        s = rotuloExtra(c);
      }
      // A COR DO TEXTO EM DEGRAU e nao interpolada: ela faz parte da chave do
      // cache de linhas de text.c, e uma cor por quadro em 39 teclas estoura o
      // orcamento de rasterizacao — e ai a tecla sai SEM GLIFO (a nota longa
      // esta em ctxmenu.c). O degrau cai em k=0,5, onde o fundo esta a 0,55 de
      // luminancia e as duas cores ainda sao legiveis.
      tom = k >= 0.5f ? 26 : 236;
      { TxtLinha l = txt_linha(f < nFileiras - 1 ? TXT_TITULO3 : TXT_BODY,
                               s, tom, tom, tom, 255);
        txt_desenhar_alpha(l, t.x + (t.w - l.w) * 0.5f,
                           t.y + (t.h - l.h) * 0.5f, a); }
    }
  }

  { TxtLinha t = txt_linha(TXT_CAPTION2,
        "Setas Navegar   OK Digitar   Voltar Cancelar", 155, 159, 169, 255);
    txt_desenhar_alpha(t, x, teY() + dy + teH() - TE_PAD - t.h, a * 0.86f); }
}
