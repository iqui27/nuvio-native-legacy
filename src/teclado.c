// Ver teclado.h para por que esta modal existe e o que ela NAO tenta ser.
#include "teclado.h"
#include "escala.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"
#include "idioma.h"
#include "sistexto.h"
#include "ponteiro.h"
#include "celbotao.h"
#include "celular.h"
#include <ctype.h>
#include <stdlib.h>
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

// DIGITAR PELO CELULAR (celbotao.h): era um painel fixo de 440 px a direita
// da modal, com o QR sempre a vista. Virou o MESMO botao do Spotlight e da
// Busca, ao lado do campo (depois do Falar): um so gesto em todo o app, a
// modal volta a ser centrada e da largura da grade, e o servidor so sobe
// quando a pessoa pede (OK no botao), nao a cada teclado aberto.
#define TE_X        ((NV_TELA_W - TE_W) * 0.5f)


static const char *ALFABETO = "abcdefghijklmnopqrstuvwxyz0123456789";

// fileira -1 = a COLUNA DA ESQUERDA (o campo e os modos). `coluna` diz qual
// alvo dela: TE_B_CAMPO o campo de texto (OK chama o teclado da TV), e os
// segmentos TE_B_FALAR, TE_B_CEL e TE_B_IME ("Teclado da TV"). O campo e o
// segmento do teclado da TV sao alvos SEPARADOS porque sao desenhados
// separados: enquanto os dois acendiam juntos, o foco nao dizia onde estava.
//
// COMO SE CHEGA NELA (dono, 05/10, Android TV): a coluna fica a ESQUERDA da
// grade, entao ESQUERDA na primeira coluna de qualquer fileira entra nela e
// DIREITA volta para a tecla de onde o foco saiu (voltaF/voltaC). Era CIMA da
// primeira fileira, heranca de quando o campo ficava ACIMA da grade: a pessoa
// apertava para o lado onde via o campo e nada acontecia. La dentro o foco
// segue o desenho — campo em cima, segmentos embaixo, em quantas fileiras o
// segmentado quebrou (segFila[]).
enum { TE_B_CAMPO = 0, TE_B_FALAR, TE_B_CEL, TE_B_IME, TE_B_N };
static int   aberto, fileira, coluna;
static int   voltaF, voltaC, barraAntes;
static float animBarra[TE_B_N];
// Coluna de caractere de onde o foco desceu para apagar/limpar/pronto. Sem ela,
// subir de "pronto" (coluna 2) numa grade de 13 caia no 'c', a dez teclas de
// onde a pessoa estava; com ela, volta para a mesma tecla.
static int   colunaAntes;
static float anim, focoAnim[TE_FILEIRAS_MAX][TE_COLS_MAX];
static const char *alfabetoAtual = NULL;   // NULL = o padrao
static int   nFileiras = TE_FILEIRAS_PAD;
static int   nCols = TE_COLS;

// LOGIN POR E-MAIL (#216, decisao do dono): quem entra por e-mail e senha e
// justamente quem nao tem celular a mao — se tivesse, logava pelo QR. Nesses
// campos o botao "Digitar pelo celular" some.
static int semCel;
// O que existe AQUI, com a porta de teste (teclado_teste_modos): no Mac nao ha
// como desligar o servidor do celular nem ligar so a voz, e a navegacao muda
// com cada combinacao.
static int forcaIme = -1, forcaVoz = -1, forcaCel = -1;
void teclado_teste_modos(int ime, int voz, int cel) { forcaIme = ime; forcaVoz = voz; forcaCel = cel; }
static int imeOk(void) { return forcaIme >= 0 ? forcaIme : st_ime_disponivel(); }
static int vozOk(void) { return forcaVoz >= 0 ? forcaVoz : st_voz_disponivel(); }
static int celOk(void) { return (forcaCel >= 0 ? forcaCel : celb_disponivel()) && !semCel; }

static float gradeW(void) {
  return (float)nCols * TE_TECLA + (float)(nCols - 1) * TE_GAP;   // 504 com 6
}
// A coluna da esquerda existe como destino do foco se tiver algum alvo: um
// modo que seja (o campo so e alvo onde ha teclado da TV, que ja e um modo).
static int temBarra(void) { return imeOk() || vozOk() || celOk(); }

// A altura da modal depende de quantas fileiras o alfabeto pediu, entao as tres
// medidas que dela dependem viraram funcao. Continuam sendo a mesma conta.
static float gradeH(void) {
  return (float)nFileiras * TE_TECLA + (float)(nFileiras - 1) * TE_GAP;
}
// GLASS UI (mockup de Ajustes, quadro "teclado", 03/10): a modal virou uma
// ilha de DUAS COLUNAS para caber a 3 m sem rolar — a esquerda o titulo, a
// dica, o campo e os modos (teclado da TV, falar, celular); a direita a grade
// de 74 px e a fileira apagar / limpar / concluir, de 56.
#define TE_ILHA_PX   52.0f
#define TE_ILHA_PY   48.0f
#define TE_COL_GAP   56.0f
#define TE_EXTRA_H   56.0f
static float teEsqW0(void) {
  float w = 1340.0f - 2 * TE_ILHA_PX - TE_COL_GAP - gradeW();
  float teto = NV_TELA_W - 80.0f - 2 * TE_ILHA_PX - TE_COL_GAP - gradeW();
  if (w > teto) w = teto;
  if (w < 420.0f) w = 420.0f;
  return w;
}

// LAYOUT MEDIDO DA COLUNA DA ESQUERDA. Nada aqui e largura fixa: o segmentado
// (teclado da TV | falar | celular), as dicas da base e o rotulo de caixa alta
// tem a largura do texto JA TRADUZIDO, e a coluna e a modal se ajustam a ele.
// A captura do dono (TCL, ingles, chave do Seekr) mostrou "Type on your phone"
// saindo da pilula e entrando na grade, e as dicas do alemao passando da coluna.
//   1. a coluna alarga ate o teto da tela;
//   2. se ainda nao cabe: folga interna menor, depois UM degrau de tamanho de
//      letra, depois o segmentado quebra em fileiras (cada uma sua pilula);
//   3. as dicas da base quebram em fileiras.
// Medido em teMedir(), a cada quadro desenhado (a ponte de texto guarda as
// linhas em cache, medir e barato); antes do primeiro quadro valem os numeros
// de antes.
#define TE_SEG_PAD     20.0f
#define TE_SEG_PAD_MIN 12.0f
#define TE_FILA_GAP     8.0f
static int   medido, segNFilas = 1, dicasFilas = 1, segFila[3];
static float ewMed, esqHMed, segPad = TE_SEG_PAD, qrH, qrHAnt;
static TxtEstilo segEst = TXT_AJ_SEG;
static float teEsqW(void) { return medido ? ewMed : teEsqW0(); }
static float teW(void) { return 2 * TE_ILHA_PX + teEsqW() + TE_COL_GAP + gradeW(); }
static float teX(void) { return (NV_TELA_W - teW()) * 0.5f; }
static float teGradeX(void) { return teX() + TE_ILHA_PX + teEsqW() + TE_COL_GAP; }
static float teH(void) {
  float grade = (float)(nFileiras - 1) * TE_PASSO - TE_GAP + 20.0f + TE_EXTRA_H;
  // A coluna da esquerda tem altura propria (titulo, dica, campo, modos e
  // as dicas na base): com um alfabeto curto (hexadecimal, 3 fileiras) a
  // grade sozinha deixaria as dicas em cima do texto.
  float esq = medido ? esqHMed : 22 + 48 + 10 + 57 + 30 + 76 + 22 + 55 + 16 + 50 + 40 + 30;
  return 2 * TE_ILHA_PY + (grade > esq ? grade : esq);
}
static float teY(void) { return (NV_TELA_H - teH()) * 0.5f; }
static const char *alfa(void) { return alfabetoAtual ? alfabetoAtual : ALFABETO; }
static char  texto[TECLADO_LONGO + 1];
static int   n, maxN, resultado;
static char  tituloAtual[96], dicaAtual[160];
static int   celRecebido;
// SENHA (#216): o campo mostra pontos, nunca o texto. A modal fica na tela e
// a tela vira foto.
static int   mascarar, tipoIme, ehSenha;
// O que foi digitado aparece em CAIXAS (codigo curto) ou num campo de texto.
// So o campo de texto e alvo de foco: as caixas nao tem estado de foco
// desenhado, e um foco que nao se ve e um foco perdido.
static int campoCaixas(void) {
  return !mascarar && maxN > 0 && (teEsqW() - (float)(maxN - 1) * TE_CGAP) / (float)maxN >= TE_CX_MIN;
}
static int campoFocavel(void) { return imeOk() && !campoCaixas(); }
// O QR do celular aberto DENTRO da modal (celbotao.h, "hospedado"): 0..1.
static float animQr;
// ATALHOS DE E-MAIL (#216): uma fileira de teclas que digitam o pedaco
// inteiro. Com o D-pad, "@gmail.com" sao dez teclas a menos.
static const char *ATALHOS_EMAIL[] = { ".com", "@gmail.com", "@hotmail.com", "@outlook.com" };
#define TE_N_ATALHOS ((int)(sizeof ATALHOS_EMAIL / sizeof ATALHOS_EMAIL[0]))
static int   nAtalhos;

// CAMADAS (dono, 05/10, chave do Real-Debrid no Android TV): quem precisa de
// maiuscula passava um alfabeto so, comprido, e a grade mostrava tudo de uma
// vez — a-m / n-z / A-M / N-Z / 0-9 e sinais, 5 x 13 = 65 teclas para achar
// uma. Quando o alfabeto tem as DUAS CAIXAS a grade passa a ter camadas, como
// todo teclado que a pessoa ja usa:
//   LETRAS    0-9 / a-j / k-t / u-z + os primeiros sinais do alfabeto
//   SINAIS    0-9 / todos os sinais (so existe se nao couberam na de letras;
//             sem o 0-9 quando os sinais sozinhos ja enchem a altura)
// em 10 colunas (848 px em vez de 1106) e 4 fileiras de 74 em vez de 5 a 7.
//
// Os digitos ficam na PRIMEIRA fileira das duas camadas: endereco e IP alternam
// digito e sinal, e trocar de camada a cada ponto custaria mais que a grade
// densa de antes. Pelo mesmo motivo as celulas que sobram depois do "z" recebem
// os primeiros sinais NA ORDEM DO CHAMADOR — os alfabetos de endereco comecam
// por ":/.-", os de conta por "._-@", que sao os que mais se digitam.
//
// AS TECLAS DE CAMADA moram numa fileira propria entre os caracteres e
// apagar/limpar/concluir (o lugar da fileira de atalhos do e-mail):
// maiusculas, sinais e espaco, cada uma so se o alfabeto tiver do que. Nao
// foram para a fileira de apagar/limpar/concluir porque ali, com a senha, ja
// sao quatro palavras traduzidas em 848 px; sete nao cabem em alemao. Na
// fileira propria BAIXO a partir de qualquer coluna chega nelas, e CIMA volta
// para a mesma tecla (colunaAntes).
//
// O QUE PODE SER DIGITADO NAO MUDA: cada celula sai do alfabeto do chamador, e
// a maiuscula de uma letra so e digitada se ELA estiver no alfabeto (teCaixa).
// Alfabeto de uma caixa so (padrao, portal, MAC, e-mail) nao tem camada
// nenhuma e continua pixel por pixel como era.
#define TE_COLS_CAMADA 10
enum { TE_K_CAIXA = 0, TE_K_SINAIS, TE_K_ESPACO };
static char celula[2][TE_FILEIRAS_MAX][TE_COLS_MAX];
static int  nCel[2][TE_FILEIRAS_MAX];
static int  emCamadas, camada, teclaCam[3], nTeclasCam;
// 0 minuscula, 1 so a PROXIMA letra (um toque), 2 travada (dois toques).
static int  caixa;

// Fileiras: as de caractere, a do meio (atalhos no e-mail, teclas de camada
// onde ha camadas) e a de apagar/limpar/(mostrar)/pronto, sempre a ultima.
static int fileirasChar(void) { return nFileiras - 1 - ((nAtalhos || emCamadas) ? 1 : 0); }
static int ehChar(int f) { return f >= 0 && f < fileirasChar(); }
static int ehAtalho(int f) { return nAtalhos && f == fileirasChar(); }
static int ehCamadas(int f) { return emCamadas && f == fileirasChar(); }
static int colunaPronto(void) { return ehSenha ? 3 : 2; }

// A letra como sai AGORA: na caixa pedida se o alfabeto a tiver, senao na que
// ele tem. Nunca devolve o que nao esta no alfabeto.
static char teCaixa(char c) {
  char quer;
  if (!emCamadas || !isalpha((unsigned char)c)) return c;
  quer = (char)(caixa ? toupper((unsigned char)c) : tolower((unsigned char)c));
  if (strchr(alfa(), quer)) return quer;
  return (char)(caixa ? tolower((unsigned char)c) : toupper((unsigned char)c));
}

// Enche uma camada, fileira por fileira, a partir de `f`; devolve a proxima.
static int teEncher(int cam, int f, const char *src, int n, int teto) {
  int i;
  for (i = 0; i < n; i++) {
    if (nCel[cam][f] == nCols) f++;
    if (f >= teto) return teto;   // o resto fica de fora, como sempre ficou
    celula[cam][f][nCel[cam][f]++] = src[i];
  }
  return n > 0 ? f + 1 : f;
}
// Monta a grade a partir do alfabeto: as celulas de cada camada, colunas e
// fileiras. `email` = a grade de 13 colunas com a fileira de atalhos.
static void teMontar(int email) {
  const char *a = alfa(), *p;
  int letras = (int)strlen(a), teto, f;
  memset(celula, 0, sizeof celula);
  memset(nCel, 0, sizeof nCel);
  emCamadas = camada = caixa = nTeclasCam = 0;
  nAtalhos = email ? TE_N_ATALHOS : 0;
  if (!email)
    for (p = a; *p; p++)
      if (isupper((unsigned char)*p) && strchr(a, tolower((unsigned char)*p))) { emCamadas = 1; break; }
  if (!emCamadas) {
    // Colunas: 6, ou 13 quando 7 fileiras de 6 nao bastam (ver TE_COLS_LONGO),
    // ou o que fizer caber em 7 fileiras, ate TE_COLS_MAX. O e-mail e sempre
    // 13 (a-m / n-z / 0-9@._ / -+) + atalhos: 6 fileiras, a altura do padrao.
    nCols = TE_COLS;
    if (email) nCols = TE_COLS_LONGO;
    else if (letras > (TE_FILEIRAS_MAX - 1) * TE_COLS) {
      nCols = TE_COLS_LONGO;
      if (letras > (TE_FILEIRAS_MAX - 1) * nCols)
        nCols = (letras + TE_FILEIRAS_MAX - 2) / (TE_FILEIRAS_MAX - 1);
      if (nCols > TE_COLS_MAX) nCols = TE_COLS_MAX;
    }
    // Fileiras de caractere = quantas o alfabeto pede, arredondando para cima,
    // mais a do meio e a de apagar/limpar/pronto. O teto existe porque
    // `focoAnim` e vetor fixo e uma grade mais alta que isto nao cabe na tela;
    // a ULTIMA fileira de caractere pode ser parcial (39 simbolos em 6 colunas
    // deixam tres na setima), e nCel[] e o que impede o foco de entrar em
    // celula vazia.
    teto = TE_FILEIRAS_MAX - 1 - (email ? 1 : 0);
    f = teEncher(0, 0, a, letras, teto);
    if (f < 1) f = 1;
    nFileiras = f + 1 + (email ? 1 : 0);
    return;
  }
  { char let[27], dig[11], sim[64];
    int nl = 0, nd = 0, ns = 0, espaco = 0, f0, f1, sobra, rap;
    for (p = a; *p; p++) {
      unsigned char c = (unsigned char)*p;
      if (c == ' ') espaco = 1;
      else if (isalpha(c)) {
        char m = (char)tolower(c);
        if (nl < 26 && !memchr(let, m, (size_t)nl)) let[nl++] = m;
      } else if (isdigit(c)) { if (nd < 10 && !memchr(dig, c, (size_t)nd)) dig[nd++] = (char)c; }
      else if (ns < (int)sizeof sim && !memchr(sim, c, (size_t)ns)) sim[ns++] = (char)c;
    }
    nCols = TE_COLS_CAMADA;
    teto = TE_FILEIRAS_MAX - 2;
    f0 = teEncher(0, 0, dig, nd, teto);
    f0 = teEncher(0, f0, let, nl, teto);
    // Os sinais rapidos: o que couber depois da ultima letra.
    sobra = f0 > 0 ? nCols - nCel[0][f0 - 1] : 0;
    rap = ns < sobra ? ns : sobra;
    if (rap > 0) { memcpy(celula[0][f0 - 1] + nCel[0][f0 - 1], sim, (size_t)rap); nCel[0][f0 - 1] += rap; }
    f1 = 0;
    if (ns > rap) {
      // Os digitos repetem em cima dos sinais enquanto isso nao fizer a camada
      // de sinais mais alta que a de letras (a senha da conta tem 32 sinais:
      // quatro fileiras so deles) — a modal nao muda de altura ao trocar.
      int soSinais = (ns + nCols - 1) / nCols, comDig = soSinais + (nd + nCols - 1) / nCols;
      if (comDig <= f0) f1 = teEncher(1, 0, dig, nd, teto);
      f1 = teEncher(1, f1, sim, ns, teto);
    }
    teclaCam[nTeclasCam++] = TE_K_CAIXA;
    if (f1) teclaCam[nTeclasCam++] = TE_K_SINAIS;
    if (espaco) teclaCam[nTeclasCam++] = TE_K_ESPACO;
    nFileiras = (f0 > f1 ? f0 : f1) + 2; }
}
static void abrirImeAgora(void);
void teclado_tipo(int tipo) {
  tipoIme = tipo == TECLADO_TIPO_EMAIL ? ST_IME_EMAIL : tipo == TECLADO_TIPO_SENHA ? ST_IME_SENHA : ST_IME_TEXTO;
  ehSenha = tipo == TECLADO_TIPO_SENHA;
  mascarar = ehSenha;
  semCel = tipo == TECLADO_TIPO_EMAIL || tipo == TECLADO_TIPO_SENHA;
  // 13 colunas (a-m / n-z / 0-9@._ / -+) + atalhos: 6 fileiras, a altura
  // do teclado padrao, em vez de 8 de 6 colunas.
  if (tipo == TECLADO_TIPO_EMAIL) teMontar(1);
  // Onde o teclado do sistema abre sozinho (Android), ele ja vem aberto: quem
  // toca no campo de e-mail quer o teclado do aparelho, nao a grade.
  if (semCel && aberto && imeOk() && st_abre_sozinho()) {
    voltaF = 0; voltaC = 0;
    fileira = -1; coluna = campoFocavel() ? TE_B_CAMPO : TE_B_IME;
    abrirImeAgora();
  }
}
void teclado_mascarar(int liga) { mascarar = liga != 0; }
int  teclado_mascarado(void) { return mascarar; }

const char *teclado_alfabeto(void) { return ALFABETO; }
int teclado_aberto(void) { return aberto; }
int teclado_foco_campo(void) { return fileira < 0 ? coluna + 1 : 0; }
int teclado_camada(void) { return camada; }
int teclado_foco_tipo(void) {
  if (fileira < 0) return TECLADO_FOCO_ESQUERDA;
  return ehChar(fileira) ? TECLADO_FOCO_CARACTERE : fileira == nFileiras - 1 ? TECLADO_FOCO_ACOES : TECLADO_FOCO_MEIO;
}
void teclado_teste_quebra(int f0, int f1, int f2) {
  segFila[0] = f0; segFila[1] = f1; segFila[2] = f2;
  segNFilas = (f2 > f1 ? f2 : f1 > f0 ? f1 : f0) + 1;
}
int teclado_caixa(void) { return caixa; }
const char *teclado_texto(void) { return texto; }
void teclado_esquecer(void) { volatile char *p = texto; size_t k = sizeof texto; while (k--) *p++ = 0; n = 0; }

int teclado_resultado(void) {
  int r = resultado;
  resultado = TECLADO_NADA;
  return r;
}

// O contexto da modal ("Contas e serviços · Chaves"), consumido pela proxima
// abertura: quem chama poe antes de teclado_abrir_com, e a seguinte nasce sem.
// Vale por um instante: quem pos e nao abriu nao contamina a proxima modal.
static char kickerPend[120], kickerAtual[120];
static Uint32 kickerQuando;
void teclado_contexto(const char *kicker) {
  snprintf(kickerPend, sizeof kickerPend, "%s", kicker ? kicker : "");
  kickerQuando = SDL_GetTicks();
}
#ifdef AJUSTES_TESTE
void teclado_teste_texto(const char *t) { snprintf(texto, sizeof texto, "%s", t); n = (int)strlen(texto); }
void teclado_teste_foco(int f, int c) { fileira = f; coluna = c; }
// O texto que chega do celular ou do teclado do sistema: o MESMO filtro de
// alfabeto e teto de definirDoSistema (#390).
static void definirDoSistema(const char *t);
void teclado_teste_sistema(const char *t) { definirDoSistema(t); }
// Poe a camada (0 letras, 1 sinais) e a caixa (0, 1, 2) direto; 0 se este
// alfabeto nao tem o que foi pedido.
int teclado_teste_camada(int cam, int cx) {
  int i, sinais = 0;
  for (i = 0; i < nTeclasCam; i++) if (teclaCam[i] == TE_K_SINAIS) sinais = 1;
  if ((cx && !emCamadas) || (cam && !sinais)) return 0;
  camada = cam; caixa = cx; fileira = 0; coluna = 0;
  return 1;
}
#endif

void teclado_abrir(const char *titulo, const char *dica, int max) {
  teclado_abrir_com(titulo, dica, max, NULL, NULL);
}

void teclado_abrir_com(const char *titulo, const char *dica, int max,
                       const char *alfabeto, const char *inicial) {
  alfabetoAtual = (alfabeto && *alfabeto) ? alfabeto : NULL;
  celb_fechar_dono(CELB_TECLADO);   // reabrir por cima nao herda o QR da anterior
  mascarar = 0; tipoIme = ST_IME_TEXTO; ehSenha = 0; semCel = 0;
  teMontar(0);
  aberto = 1;
  fileira = 0; coluna = 0; colunaAntes = 0;
  voltaF = voltaC = 0; barraAntes = -1;
  medido = 0;
  resultado = TECLADO_NADA;
  maxN = max > 0 && max <= TECLADO_LONGO ? max : TECLADO_MAX;
  // TEXTO INICIAL: editar um portal ja cadastrado nao pode obrigar a redigitar
  // o endereco inteiro. Cortado em maxN, nunca truncado no meio de nada porque
  // o alfabeto e de um byte por caractere.
  snprintf(texto, sizeof texto, "%s", inicial ? inicial : "");
  texto[maxN] = 0;
  n = (int)strlen(texto);
  snprintf(tituloAtual, sizeof tituloAtual, "%s", titulo ? titulo : "");
  snprintf(dicaAtual,   sizeof dicaAtual,   "%s", dica   ? dica   : "");
  snprintf(kickerAtual, sizeof kickerAtual, "%s", SDL_GetTicks() - kickerQuando < 1000u ? kickerPend : "");
  kickerPend[0] = 0;
  memset(focoAnim, 0, sizeof focoAnim);
  memset(animBarra, 0, sizeof animBarra);
  celRecebido = 0;
  animQr = 0.0f;
}

// O texto do sistema passa pelo ALFABETO da modal: o codigo de pareamento e
// a-z0-9, o MAC e 0-9a-f — caixa alta vira baixa quando so a baixa existe, e
// o que nao existe nele (acento, emoji) fica de fora.
static void definirDoSistema(const char *t) {
  const char *a = alfa();
  int w = 0;
  for (; *t && w < maxN; t++) {
    unsigned char c = (unsigned char)*t;
    if (c >= 0x80 || !c) continue;
    if (strchr(a, c)) texto[w++] = (char)c;
    else if (isupper(c) && strchr(a, tolower(c))) texto[w++] = (char)tolower(c);
    else if (islower(c) && strchr(a, toupper(c))) texto[w++] = (char)toupper(c);
  }
  texto[w] = 0;
  n = w;
}

static void fechar(int r) {
  aberto = 0;
  celb_fechar_dono(CELB_TECLADO);
  resultado = r;
  st_fechar(ST_TECLADO);
}

static void abrirImeAgora(void) { st_ime_tipo(tipoIme); st_ime_abrir(ST_TECLADO, texto, maxN); }
static void okBarra(void) {
  if (coluna == TE_B_CEL) {
    // O QR abre AQUI DENTRO, embaixo do segmentado (celb_desenhar_em). OK com
    // o codigo na tela nao faz nada (quem aperta esta "confirmando"); com o
    // endereco vencido ou sem rede, gera outro.
    st_fechar(ST_TECLADO);
    if (!celb_embutido(CELB_TECLADO) || celular_estado() != CEL_ESPERANDO)
      celb_abrir_embutido(CELB_TECLADO, tituloAtual);
    return;
  }
  celb_fechar_dono(CELB_TECLADO);   // um modo de cada vez
  if (coluna == TE_B_FALAR) st_voz_iniciar(ST_TECLADO);
  else abrirImeAgora();
}

// Os modos que existem agora: rotulo e alvo de foco de cada um, na ordem em
// que sao desenhados.
static int teModos(const char **rot, int *col) {
  int k = 0;
  if (imeOk()) { rot[k] = "Teclado da TV"; col[k++] = TE_B_IME; }
  if (vozOk()) { rot[k] = "Falar"; col[k++] = TE_B_FALAR; }
  if (celOk()) { rot[k] = "Digitar pelo celular"; col[k++] = TE_B_CEL; }
  return k;
}
// O alvo `c` da coluna da esquerda existe agora?
static int barraValida(int c) {
  const char *rot[3];
  int col[3], k = teModos(rot, col), i;
  if (c == TE_B_CAMPO) return campoFocavel();
  for (i = 0; i < k; i++) if (col[i] == c) return 1;
  return 0;
}
static int barraPadrao(void) {
  const char *rot[3];
  int col[3], k = teModos(rot, col);
  return campoFocavel() ? TE_B_CAMPO : (k ? col[0] : TE_B_CAMPO);
}
// Entra na coluna da esquerda guardando de onde saiu; volta para o alvo em que
// a pessoa estava da ultima vez (campo na primeira).
static void entrarBarra(int alvo) {
  if (!temBarra()) return;
  if (fileira >= 0) { voltaF = fileira; voltaC = coluna; }
  if (alvo < 0 || !barraValida(alvo)) alvo = barraValida(barraAntes) ? barraAntes : barraPadrao();
  fileira = -1; coluna = alvo;
}
static int colunasDe(int f);
static void sairBarra(void) {
  int f = voltaF, c = voltaC;
  barraAntes = coluna;
  if (f < 0 || f >= nFileiras || colunasDe(f) < 1) { f = 0; c = 0; }
  if (c >= colunasDe(f)) c = colunasDe(f) - 1;
  fileira = f; coluna = c < 0 ? 0 : c;
}
static void focarBarra(int c, int b) { (void)b; if (aberto) entrarBarra(c); }
static void focarTecla(int f, int c) {
  if (!aberto || f < 0 || f >= nFileiras) return;
  if (fileira < 0) barraAntes = coluna;
  else if (!ehChar(f) && ehChar(fileira)) colunaAntes = coluna;
  fileira = f; coluna = c;
}

// Quantas teclas a fileira tem NA CAMADA A VISTA. Pode ser 0: a camada de
// sinais costuma ter menos fileiras que a de letras, e a navegacao pula as
// vazias (proxFileira) em vez de pousar numa celula que nao existe.
static int colunasDe(int f) {
  if (f >= nFileiras - 1) return ehSenha ? 4 : 3;   // apagar / limpar / (mostrar) / pronto
  if (ehAtalho(f)) return nAtalhos;
  if (ehCamadas(f)) return nTeclasCam;
  return f >= 0 ? nCel[camada][f] : 0;
}
static int proxFileira(int f, int d) {
  do f += d; while (f >= 0 && f < nFileiras && colunasDe(f) < 1);
  return f;
}
// AS TECLAS DE CAMADA OCUPAM DUAS COLUNAS CADA, a partir da esquerda, e o
// espaco (sempre a ultima) vai dali ate o fim da grade — como os modificadores
// do canto de um teclado de verdade, alinhados as teclas de cima. Vindo de uma
// fileira de caractere, BAIXO cai na tecla que esta EMBAIXO daquela coluna.
#define TE_CAM_COLS 2
static int camDeColuna(int c) {
  int i = c / TE_CAM_COLS;
  return i < nTeclasCam ? i : nTeclasCam - 1;
}

static GfxRect retangulo(int f, int c) {
  GfxRect r;
  r.y = teY() + TE_ILHA_PY + (float)f * TE_PASSO;
  r.h = TE_TECLA;
  if (ehChar(f)) {
    r.x = teGradeX() + (float)c * TE_PASSO;
    r.w = TE_TECLA;
  } else if (f == nFileiras - 1) {
    // apagar / limpar / (mostrar) / CONCLUIR: o ultimo e 1,3 vez os outros.
    int k = colunasDe(f), i;
    float unid = (gradeW() - (float)(k - 1) * TE_GAP) / ((float)(k - 1) + 1.3f);
    r.y = teY() + TE_ILHA_PY + (float)(nFileiras - 1) * TE_PASSO - TE_GAP + 20.0f;
    r.h = TE_EXTRA_H;
    r.x = teGradeX();
    for (i = 0; i < c; i++) r.x += unid + TE_GAP;
    r.w = c == k - 1 ? unid * 1.3f : unid;
  } else if (ehCamadas(f)) {
    r.x = teGradeX() + (float)(c * TE_CAM_COLS) * TE_PASSO;
    r.w = teclaCam[c] == TE_K_ESPACO ? teGradeX() + gradeW() - r.x
                                     : (float)TE_CAM_COLS * TE_PASSO - TE_GAP;
  } else {
    int k = colunasDe(f);
    r.w = (gradeW() - (float)(k - 1) * TE_GAP) / (float)k;
    r.x = teGradeX() + (float)c * (r.w + TE_GAP);
  }
  return r;
}

// O rotulo da ultima fileira. As tres teclas sao o unico ponto da modal com
// palavra em vez de caractere, e por isso as tres estao na tabela de i18n.
static const char *rotuloExtra(int c) {
  if (c == 0) return "apagar";
  if (c == 1) return "limpar";
  if (ehSenha && c == 2) return mascarar ? "mostrar" : "ocultar";
  return "Concluir";
}

static void aplicar(void) {
  if (ehChar(fileira)) {
    if (coluna >= colunasDe(fileira)) return;
    if (n < maxN) { texto[n++] = teCaixa(celula[camada][fileira][coluna]); texto[n] = 0; }
    if (caixa == 1) caixa = 0;   // um toque vale para UM caractere
    return;
  }
  if (ehCamadas(fileira)) {
    int k = teclaCam[coluna < nTeclasCam ? coluna : 0];
    if (k == TE_K_CAIXA) caixa = (caixa + 1) % 3;
    else if (k == TE_K_SINAIS) camada = !camada;
    else if (n < maxN) { texto[n++] = ' '; texto[n] = 0; }
    return;
  }
  if (ehAtalho(fileira)) {
    const char *a = ATALHOS_EMAIL[coluna < nAtalhos ? coluna : 0];
    // "@gmail.com" depois de um "@" ja digitado nao duplica a arroba.
    if (a[0] == '@' && strchr(texto, '@')) a++;
    if (n + (int)strlen(a) <= maxN) { memcpy(texto + n, a, strlen(a) + 1); n += (int)strlen(a); }
    return;
  }
  if (coluna == 0) { if (n > 0) texto[--n] = 0; return; }
  if (coluna == 1) { n = 0; texto[0] = 0; return; }
  if (ehSenha && coluna == 2) { mascarar = !mascarar; return; }
  // "pronto" com o campo vazio nao e uma confirmacao de nada: quem chega ali
  // sem digitar quis olhar o teclado, e fechar a modal com resultado PRONTO
  // mandaria a tela de amigos tentar vincular uma string vazia.
  if (n < 1) return;
  fechar(TECLADO_PRONTO);
}

void teclado_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (aberto && st_evento(e)) return;   // teclado da TV: valor inteiro por sistexto
  if (!aberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    fechar(TECLADO_CANCELOU);
    return;
  }
  if (fileira < 0) {
    const char *rot[3];
    int col[3], nm = teModos(rot, col), i, ini, fim;
    if (!barraValida(coluna)) coluna = barraPadrao();   // o modo sumiu com o foco nele
    if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && !e->key.repeat) { okBarra(); return; }
    if (coluna == TE_B_CAMPO) {
      // O CAMPO, em cima: BAIXO desce para o segmentado, DIREITA volta a grade.
      if (k == SDLK_DOWN && nm) coluna = col[0];
      else if (k == SDLK_RIGHT) sairBarra();
      return;
    }
    for (i = 0; i < nm && col[i] != coluna; i++) {}
    if (i >= nm) return;
    // A fileira do segmentado em que este segmento caiu (medida no desenho).
    for (ini = i; ini > 0 && segFila[ini - 1] == segFila[i]; ini--) {}
    for (fim = i; fim + 1 < nm && segFila[fim + 1] == segFila[i]; fim++) {}
    if (k == SDLK_LEFT) { if (i > ini) coluna = col[i - 1]; }
    else if (k == SDLK_RIGHT) { if (i < fim) coluna = col[i + 1]; else sairBarra(); }
    else if (k == SDLK_UP) {
      // Fileira de cima do segmentado, na mesma posicao; da primeira, o campo.
      if (ini > 0) {
        int pini = ini - 1, j;
        while (pini > 0 && segFila[pini - 1] == segFila[ini - 1]) pini--;
        j = pini + (i - ini);
        coluna = col[j < ini ? j : ini - 1];
      } else if (campoFocavel()) coluna = TE_B_CAMPO;
    } else if (k == SDLK_DOWN && fim + 1 < nm) {
      int pfim = fim + 1, j;
      while (pfim + 1 < nm && segFila[pfim + 1] == segFila[fim + 1]) pfim++;
      j = fim + 1 + (i - ini);
      coluna = col[j > pfim ? pfim : j];
    }
    return;
  }
  if (k == SDLK_UP) {
    // CIMA da primeira fileira nao faz nada: a coluna do campo fica a
    // ESQUERDA, nao acima.
    int nf = proxFileira(fileira, -1);
    if (nf < 0) return;
    if (!ehChar(fileira) && ehChar(nf)) coluna = colunaAntes;
    fileira = nf;
    if (coluna >= colunasDe(fileira)) coluna = colunasDe(fileira) - 1;
    return;
  }
  if (k == SDLK_DOWN) {
    int nf = proxFileira(fileira, 1);
    if (nf >= nFileiras) return;
    if (ehChar(fileira) && !ehChar(nf)) colunaAntes = coluna;
    if (ehChar(fileira) && ehCamadas(nf)) coluna = camDeColuna(coluna);
    fileira = nf;
    if (coluna >= colunasDe(fileira)) coluna = colunasDe(fileira) - 1;
    return;
  }
  // ESQUERDA na primeira coluna de QUALQUER fileira entra na coluna do campo.
  if (k == SDLK_LEFT)  { if (coluna > 0) coluna--; else entrarBarra(-1); return; }
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
  if (aberto) {
    char t[TECLADO_LONGO + 1];
    int voz = st_estado() == ST_OUVINDO || st_estado() == ST_PERMISSAO || st_estado() == ST_VOZ_SISTEMA;
    int r;
    if (celb_pegar(CELB_TECLADO, t, sizeof t)) {
      // O texto do celular e o VALOR INTEIRO do campo, como o do teclado da
      // TV: passa pelo mesmo filtro de alfabeto. Nao confirma sozinho — a
      // pessoa ve o que chegou, e o foco vai para "pronto": um OK salva.
      st_fechar(ST_TECLADO);
      definirDoSistema(t);
      memset(t, 0, sizeof t);
      celRecebido = 1;
      fileira = nFileiras - 1; coluna = colunaPronto();
    }
    r = st_ler(ST_TECLADO, t, sizeof t);
    if (r == ST_PEDE_TECLADO) {
      if (fileira < 0) coluna = campoFocavel() ? TE_B_CAMPO : TE_B_IME;
      abrirImeAgora();
    }
    else if (r == ST_TEXTO || r == ST_FIM) {
      definirDoSistema(t);
      // "Concluir" no teclado da TV e o "pronto" da modal; o fim da FALA nao —
      // a pessoa confere o que o reconhecedor entendeu antes de enviar.
      if (r == ST_FIM && !voz && n > 0) fechar(TECLADO_PRONTO);
    }
  }
  { float alvo = (aberto && celb_embutido(CELB_TECLADO)) ? 1.0f : 0.0f;
    animQr = ajustes_animacoes_reduzidas() ? alvo : anim_mola(animQr, alvo, dt, NV_MOLA_TELA); }
  for (c = 0; c < TE_B_N; c++)
    animBarra[c] = anim_mola(animBarra[c], (aberto && fileira < 0 && coluna == c) ? 1.0f : 0.0f,
                             dt, NV_MOLA_FOCO);
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

// Material da ilha (o mesmo de ajustes_ux_desenho.inc): vidro a 86% com a
// luz do canto, solido #15161A; foco de tecla/chip = cheio no acento.
static void teNeutro(GfxRect r, float raioPx, float vid, float sr, float sg, float sb, float a) {
  if (ajustes_vidro()) gfx_cor(r, raioPx / r.h, 1, 1, 1, vid * a);
  else gfx_cor(r, raioPx / r.h, sr, sg, sb, a);
}
static void teAcento(GfxRect r, float raioPx, float k, float a) {
  float ar, ag, ab;
  ajustes_acento(&ar, &ag, &ab);
  gfx_rect((GfxRect){ r.x - 12, r.y - 2, r.w + 24, r.h + 26 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, ar, ag, ab, 0.35f * k * a);
  gfx_cor(r, raioPx / r.h, ar, ag, ab, k * a);
}
// Rotulo em caixa alta: se o texto traduzido passa da coluna, o espacamento
// entre letras diminui, e se ainda passar o recorte para na borda da coluna.
static float teCaps(const char *s, float x, float y, float ew, float a) {
  char up[200];
  float trk = 2.1f, w;
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(s));
  w = txt_tracking(TXT_MINI, up, 243, 242, 239, -1, 0, 0, trk);
  if (w > ew) { trk = 0.8f; w = txt_tracking(TXT_MINI, up, 243, 242, 239, -1, 0, 0, trk); }
  if (w > ew) {
    float r;
    gfx_recorte(x, y - 4, ew, 30);
    r = txt_tracking(TXT_MINI, up, 243, 242, 239, x, y, 0.45f * a, trk);
    gfx_sem_recorte();
    return r;
  }
  return txt_tracking(TXT_MINI, up, 243, 242, 239, x, y, 0.45f * a, trk);
}
static void teDica(float *x, float y, const char *k, const char *l, float a) {
  TxtLinha tk = txt_linha(TXT_AJ_KBD, k, 243, 242, 239, 255), tl = txt_linha(TXT_ILHA_APOIO, l, 243, 242, 239, 255);
  float kw = tk.w + 18.0f < 34.0f ? 34.0f : tk.w + 18.0f;
  teNeutro((GfxRect){ *x, y, kw, 30 }, 15, 0.09f, 0.141f, 0.149f, 0.173f, a);
  txt_desenhar_alpha(tk, *x + (kw - tk.w) * 0.5f, y + (30 - tk.h) * 0.5f, 0.82f * a);
  txt_desenhar_alpha(tl, *x + kw + 9, y + (30 - tl.h) * 0.5f, 0.45f * a);
  *x += kw + 9 + tl.w + 20;
}

static const char *TE_FRASE_CEL = "Pelo celular, o texto chega aqui para você conferir antes de concluir.";
static float teDicaLarg(const char *k, const char *l) {
  TxtLinha tk = txt_linha(TXT_AJ_KBD, k, 243, 242, 239, 255), tl = txt_linha(TXT_ILHA_APOIO, l, 243, 242, 239, 255);
  float kw = tk.w + 18.0f < 34.0f ? 34.0f : tk.w + 18.0f;
  return kw + 9 + tl.w + 20;
}
// Fileiras que n dicas de largura w[] ocupam em ew (a ultima nao paga o vao).
static int teDicasFilas(const float *w, int n, float ew) {
  int i, filas = 1;
  float x = 0;
  for (i = 0; i < n; i++) {
    if (x > 0 && x + w[i] - 20.0f > ew) { filas++; x = 0; }
    x += w[i];
  }
  return filas;
}
static float teChipsLarg(TxtEstilo e, float pad, const char **rot, int a, int b) {
  float w = 10.0f;
  int i;
  for (i = a; i < b; i++) w += txt_linha(e, rot[i], 0, 0, 0, 255).w + 2 * pad + (i > a ? 4.0f : 0.0f);
  return w;
}
static void teMedir(void) {
  const char *rot[3];
  int col[3], k = teModos(rot, col), i;
  float base = teEsqW0(), teto = NV_TELA_W - 80.0f - 2 * TE_ILHA_PX - TE_COL_GAP - gradeW();
  float ew, segNeed = k ? teChipsLarg(TXT_AJ_SEG, TE_SEG_PAD, rot, 0, k) : 0.0f;
  float wA[3], wB[3], dicaMax = 0, h;
  const char *lc[3] = { "Digitar pelo celular", "Falar", "Teclado da TV" };
  // Dicas: a fileira com o foco na barra (OK muda de rotulo: vale o maior) e a
  // das teclas.
  for (i = 0; i < 3; i++) { float w = teDicaLarg("OK", lc[i]); if (w > dicaMax) dicaMax = w; }
  wA[0] = dicaMax; wA[1] = teDicaLarg("→", "Teclado"); wA[2] = teDicaLarg("Voltar", "Cancelar");
  wB[0] = teDicaLarg("Setas", "Navegar"); wB[1] = teDicaLarg("OK", "Digitar"); wB[2] = wA[2];
  if (emCamadas) { float w = teDicaLarg("OK", "Maiúsculas"); if (w > wB[1]) wB[1] = w; }
  { float na = wA[0] + wA[1] + wA[2] - 20.0f, nb = wB[0] + wB[1] + wB[2] - 20.0f;
    float need = segNeed > na ? segNeed : na;
    if (nb > need) need = nb;
    ew = need > base ? need : base;
    if (ew > teto) ew = teto; }
  ewMed = ew;
  // O segmentado: folga, degrau de letra, depois fileiras.
  segEst = TXT_AJ_SEG; segPad = TE_SEG_PAD; segNFilas = 1;
  for (i = 0; i < 3; i++) segFila[i] = 0;
  if (k && segNeed > ew) {
    if (teChipsLarg(TXT_AJ_SEG, TE_SEG_PAD_MIN, rot, 0, k) <= ew) segPad = TE_SEG_PAD_MIN;
    else if (teChipsLarg(TXT_ILHA_SEG, TE_SEG_PAD_MIN, rot, 0, k) <= ew) { segEst = TXT_ILHA_SEG; segPad = TE_SEG_PAD_MIN; }
    else {
      int ini = 0;
      for (i = 1; i <= k; i++)
        if (i == k || teChipsLarg(TXT_AJ_SEG, TE_SEG_PAD, rot, ini, i + 1) > ew) {
          int j;
          for (j = ini; j < i; j++) segFila[j] = segNFilas - 1;
          if (i < k) { segNFilas++; ini = i; }
        }
    }
  }
  dicasFilas = teDicasFilas(wA, 3, ew);
  { int fb = teDicasFilas(wB, 3, ew); if (fb > dicasFilas) dicasFilas = fb; }
  // Altura da coluna, somada na ordem em que ela e desenhada.
  h = 0;
  if (kickerAtual[0]) h += 22.0f;
  h += 48.0f;
  if (dicaAtual[0]) h += 10.0f + txt_bloco(TXT_AJ_SUB, dicaAtual, 243, 242, 239, 0, 0, ew, 28.5f, 0.0f, 3);
  h += 30.0f;
  h += (!mascarar && maxN > 0 && (ew - (float)(maxN - 1) * TE_CGAP) / (float)maxN >= TE_CX_MIN) ? TE_CY : 76.0f;
  if (k) h += 22.0f + 55.0f + (float)(segNFilas - 1) * (55.0f + TE_FILA_GAP);
  { float base = 24.0f + 30.0f + (float)(dicasFilas - 1) * (30.0f + TE_FILA_GAP);
    if (k && celOk()) {
      // Embaixo do segmentado mora a frase do celular e, com o QR aberto, o
      // proprio QR: a coluna (e a modal) cresce para ele caber no tamanho de
      // sempre, ate o que a tela deixa — celb escolhe o arranjo que cabe.
      float frase = txt_bloco(TXT_ILHA_GENERO, TE_FRASE_CEL, 243, 242, 239, 0, 0, ew, 25.0f, 0.0f, 3);
      float teto = NV_TELA_H - 48.0f - 2 * TE_ILHA_PY - h - 16.0f - base;
      qrH = celb_embutido(CELB_TECLADO) ? celb_embutido_altura(CELB_TECLADO, ew, teto) : 0.0f;
      if (qrH > 0) qrHAnt = qrH;   // fechando, a altura encolhe a partir da ultima
      h += 16.0f + anim_mistura(frase, qrHAnt, anim_suave(animQr));
    }
    h += base; }
  esqHMed = h;
  medido = 1;
}
// Desenha n dicas (rotulo da tecla, texto) quebrando em fileiras de largura ew;
// a ultima fileira fica em `by`, as outras acima dela.
static void teDicas(const char **ks, const char **ls, int n, float x0, float by, float ew, float a) {
  float w[4], x;
  int i, filas, f = 0;
  for (i = 0; i < n; i++) w[i] = teDicaLarg(ks[i], ls[i]);
  filas = teDicasFilas(w, n, ew);
  x = x0;
  for (i = 0; i < n; i++) {
    if (x > x0 && x + w[i] - 20.0f > x0 + ew) { f++; x = x0; }
    { float xx = x;
      teDica(&xx, by - (float)(filas - 1 - f) * (30.0f + TE_FILA_GAP), ks[i], ls[i], a); }
    x += w[i];
  }
}

static void teDesenhar(Uint32 agora);
// O teclado fica em 1080p em qualquer "Tamanho da interface": em 100% ele ja
// ocupa a largura da tela (ver a conta no alto), e ampliado nao caberia.
void teclado_desenhar(Uint32 agora) {
  ESCALA_REAL_INI();
  teDesenhar(agora);
  ESCALA_REAL_FIM();
}
static void teDesenhar(Uint32 agora) {
  float a = anim_suave(anim), dy, x, y, ew;
  int f, c, i;
  teMedir();
  if (anim < 0.01f) return;
  dy = (1.0f - a) * 36.0f;
  if (aberto && ponteiro_ativo()) {
    ponteiro_camada();
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, NULL, 0, 0);
  }
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0, 0.84f * anim);
  { GfxRect p = { teX(), teY() + dy, teW(), teH() };
    float raio = 36.0f / p.h;
    gfx_rect((GfxRect){ p.x - 20, p.y - 6, p.w + 40, p.h + 46 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, 0.40f * a);
    if (ajustes_vidro()) {
      gfx_cor(p, raio, 0.055f, 0.059f, 0.071f, 0.86f * a);
      gfx_luz_canto(p, raio, p.w * 0.22f, -p.h * 0.40f, p.h * 0.62f, 1, 1, 1, 0.10f * a);
    } else gfx_cor(p, raio, 0.082f, 0.086f, 0.102f, a); }

  // COLUNA DA ESQUERDA
  x = teX() + TE_ILHA_PX; ew = teEsqW();
  y = teY() + dy + TE_ILHA_PY;
  if (kickerAtual[0]) { teCaps(kickerAtual, x, y + 2, ew, a); y += 22.0f; }
  { TxtLinha t = txt_linha_corta(TXT_ILHA_TITULO, tituloAtual, 243, 242, 239, 255, ew);
    txt_desenhar_alpha(t, x, y, a); y += 48.0f; }
  if (dicaAtual[0]) {
    y += 10.0f;
    y += txt_bloco(TXT_AJ_SUB, dicaAtual, 243, 242, 239, x, y, ew, 28.5f, 0.58f * a, 3);
  }
  y += 30.0f;
  { float ar, ag, ab;
    ajustes_acento(&ar, &ag, &ab);
    // O QUE FOI DIGITADO: caixas quando cada uma cabe o glifo (codigos
    // curtos), senao o campo de texto com cursor.
    if (!mascarar && (ew - (float)(maxN - 1) * TE_CGAP) / (float)maxN >= TE_CX_MIN) {
      float bw = (ew - (float)(maxN - 1) * TE_CGAP) / (float)maxN, bx = x;
      if (bw > TE_CX) bw = TE_CX;
      for (i = 0; i < maxN; i++) {
        GfxRect b = { bx, y, bw, TE_CY };
        teNeutro(b, 20, i < n ? 0.16f : 0.05f, i < n ? 0.20f : 0.12f, i < n ? 0.205f : 0.125f, i < n ? 0.23f : 0.145f, a);
        if (i < n) {
          char ch[2] = { texto[i], 0 };
          TxtLinha t = txt_linha(TXT_TITULO2, ch, 243, 242, 239, 255);
          txt_desenhar_alpha(t, b.x + (b.w - t.w) * 0.5f, b.y + (b.h - t.h) * 0.5f, a);
        }
        bx += bw + TE_CGAP;
      }
      y += TE_CY;
    } else {
      GfxRect campo = { x, y, ew, 76.0f };
      float tx = campo.x + 24.0f, cursorX = tx, larg;
      char q[48];
      TxtLinha lq;
      teNeutro(campo, 24, 0.07f, 0.125f, 0.129f, 0.153f, a);
      // Foco no campo: o mesmo degrau de claridade de antes, um pouco mais
      // alto — agora ele acende SOZINHO (o segmento do teclado da TV e outro
      // alvo) e precisa ser lido a 3 m sem a pilula do lado.
      if (animBarra[TE_B_CAMPO] > 0.01f) {
        float kf = animBarra[TE_B_CAMPO];
        gfx_rect((GfxRect){ campo.x - 12, campo.y - 2, campo.w + 24, campo.h + 26 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, 0, 0, 0, 0.45f * kf * a);
        teNeutro(campo, 24, 0.07f, 0.125f, 0.129f, 0.153f, a);   // por cima da propria sombra
        teNeutro(campo, 24, 0.17f * kf, 0.255f, 0.263f, 0.298f, kf * a);
      }
      if (imeOk() && ponteiro_ativo()) ponteiro_alvo(campo.x, campo.y, campo.w, campo.h, focarBarra, NULL, TE_B_CAMPO, 0);
      snprintf(q, sizeof q, n == 1 ? i18n("%d caractere") : i18n("%d caracteres"), n);
      lq = txt_linha(TXT_ILHA_APOIO, q, 243, 242, 239, 255);
      txt_desenhar_alpha(lq, campo.x + campo.w - 24 - lq.w, campo.y + (campo.h - lq.h) * 0.5f, 0.4f * a);
      larg = campo.w - 48.0f - lq.w - 20.0f;
      if (n) {
        char pontos[TECLADO_LONGO * 3 + 1];
        TxtLinha t;
        float ox;
        if (mascarar) {
          int i2;
          for (i2 = 0; i2 < n && i2 < TECLADO_LONGO; i2++) memcpy(pontos + i2 * 3, "\xE2\x80\xA2", 3);
          pontos[i2 * 3] = 0;
        }
        t = txt_linha(TXT_AJ_INSP, mascarar ? pontos : texto, 243, 242, 239, 255);
        ox = t.w > larg ? t.w - larg : 0.0f;
        gfx_recorte(tx, campo.y, larg, campo.h);
        txt_desenhar_alpha(t, tx - ox, campo.y + (campo.h - t.h) * 0.5f, a);
        gfx_sem_recorte();
        cursorX = tx + (t.w - ox);
      }
      { float op = ajustes_animacoes_reduzidas() ? 0.95f : (((agora / 500) % 2) ? 0.35f : 0.95f);
        gfx_cor((GfxRect){ cursorX + 3.0f, campo.y + 20.0f, 2.0f, 36.0f }, 0.5f, ar, ag, ab, op * a); }
      y += 76.0f;
    } }
  // MODOS: teclado da TV, falar e celular, num segmentado (o que existir).
  { const char *rot[3]; int col[3], k = teModos(rot, col);
    if (k) {
      float h = 55.0f, ih = 45.0f;
      int fila, ini = 0;
      y += 22.0f;
      for (fila = 0; fila < segNFilas; fila++) {
        int fim = ini;
        float sx = x, py = y + (float)fila * (h + TE_FILA_GAP);
        while (fim < k && segFila[fim] == fila) fim++;
        teNeutro((GfxRect){ sx, py, teChipsLarg(segEst, segPad, rot, ini, fim), h }, h * 0.5f, 0.06f, 0.114f, 0.118f, 0.137f, a);
        sx += 5.0f;
        for (i = ini; i < fim; i++) {
          int foco = aberto && fileira < 0 && coluna == col[i];
          int ti = foco ? ajustes_tinta_foco() : 243;
          TxtLinha t = txt_linha(segEst, rot[i], ti, ti, ti, 255);
          GfxRect r = { sx, py + 5.0f, t.w + 2 * segPad, ih };
          if (aberto && ponteiro_ativo()) ponteiro_alvo(r.x, r.y, r.w, r.h, focarBarra, NULL, col[i], 0);
          if (foco) teAcento(r, ih * 0.5f, animBarra[col[i]] > 0.5f ? 1.0f : animBarra[col[i]] * 2.0f, a);
          else if (col[i] == TE_B_CEL && (celRecebido || celb_embutido(CELB_TECLADO)))
            teNeutro(r, ih * 0.5f, 0.14f, 0.204f, 0.212f, 0.243f, a);   // o modo em uso fica marcado
          txt_desenhar_alpha(t, r.x + segPad, r.y + (ih - t.h) * 0.5f, (foco ? 1.0f : 0.55f) * a);
          sx += r.w + 4.0f;
        }
        ini = fim;
      }
      y += h + (float)(segNFilas - 1) * (h + TE_FILA_GAP);
      if (celOk()) {
        float t = anim_suave(animQr);
        y += 16.0f;
        if (t < 0.99f) txt_bloco(TXT_ILHA_GENERO, TE_FRASE_CEL, 243, 242, 239, x, y, ew, 25.0f, 0.45f * a * (1.0f - t), 3);
        // O QR DO CELULAR, AQUI: no lugar da frase, embaixo do segmento que o
        // abriu. Era um cartao solto no canto da tela (celb_desenhar), sem
        // ligacao com a modal.
        if (qrH > 0 && celb_embutido(CELB_TECLADO))
          celb_desenhar_em(CELB_TECLADO, (GfxRect){ x, y, ew, qrH }, a * t);
      }
    } }
  // Dicas na base da coluna.
  { const char *av = st_dono() == ST_TECLADO ? st_aviso() : "";
    float by = teY() + dy + teH() - TE_ILHA_PY - 30.0f;
    if (av[0]) {
      TxtLinha t = txt_linha_corta(TXT_ILHA_GENERO, av, 240, 196, 140, 255, ew);
      txt_desenhar_alpha(t, x, by + (30 - t.h) * 0.5f, a);
    } else if (celRecebido && fileira == nFileiras - 1) {
      TxtLinha t = txt_linha_corta(TXT_ILHA_GENERO, "Recebido do celular. Confira e aperte Concluir.", 243, 242, 239, 255, ew);
      txt_desenhar_alpha(t, x, by + (30 - t.h) * 0.5f, 0.62f * a);
    } else if (fileira < 0) {
      const char *ks[3] = { "OK", "→", "Voltar" };
      const char *ls[3] = { coluna == TE_B_CEL ? "Digitar pelo celular" : coluna == TE_B_FALAR ? "Falar" : "Teclado da TV", "Teclado", "Cancelar" };
      teDicas(ks, ls, 3, x, by, ew, a);
    } else {
      const char *ks[3] = { "Setas", "OK", "Voltar" };
      const char *ls[3] = { "Navegar", "Digitar", "Cancelar" };
      // Na tecla das maiusculas o OK diz o que ela e: a tecla so tem a seta.
      if (ehCamadas(fileira) && teclaCam[coluna < nTeclasCam ? coluna : 0] == TE_K_CAIXA) ls[1] = "Maiúsculas";
      teDicas(ks, ls, 3, x, by, ew, a);
    } }

  // COLUNA DA DIREITA: a grade.
  for (f = 0; f < nFileiras; f++) {
    for (c = 0; c < colunasDe(f); c++) {
      float k = focoAnim[f][c];
      GfxRect base = retangulo(f, c);
      int extra = f == nFileiras - 1, concluir = extra && c == colunasDe(f) - 1;
      int cam = ehCamadas(f), kc = cam ? teclaCam[c] : -1;
      // Maiusculas ligadas (um toque ou travada) e a camada de sinais a vista:
      // a tecla fica ERGUIDA, e o estado dela, nao o foco.
      int ligada = (kc == TE_K_CAIXA && caixa) || (kc == TE_K_SINAIS && camada);
      // As teclas de camada sao largas: 10% de 848 px passaria da ilha.
      float esc = 1.0f + (extra ? 0.0f : TE_ESCALA * k * (cam ? 0.3f : 1.0f));
      float alfaTxt = 0.9f;
      TxtEstilo est = TXT_AJ_TIT28;
      GfxRect t;
      const char *s;
      char ch[2];
      int tom;
      base.y += dy;
      if (aberto && ponteiro_ativo()) ponteiro_alvo(base.x, base.y, base.w, base.h, focarTecla, NULL, f, c);
      t.w = base.w * esc; t.h = base.h * esc;
      t.x = base.x - (t.w - base.w) * 0.5f;
      t.y = base.y - (t.h - base.h) * 0.5f;
      if (extra) teNeutro(t, t.h * 0.5f, 0.08f, 0.141f, 0.149f, 0.173f, a);
      else if (ligada) teNeutro(t, 20.0f * esc, 0.17f, 0.235f, 0.243f, 0.275f, a);
      else teNeutro(t, 20.0f * esc, 0.07f, 0.125f, 0.129f, 0.153f, a);
      if (k > 0.01f) teAcento(t, extra ? t.h * 0.5f : 20.0f * esc, k, a);
      if (ehAtalho(f)) { s = ATALHOS_EMAIL[c]; est = TXT_AJ_ESTADO; }
      else if (cam) {
        est = TXT_AJ_SEG;
        if (kc == TE_K_CAIXA) {
          // A seta do Shift, e a do Caps Lock quando TRAVADA (com o ponto na
          // cor do realce); um toque so ergue a tecla. Sem palavra: a tecla
          // tem duas colunas e "Großbuchstaben" nao cabe nelas — a palavra
          // esta na dica da base. Na camada de sinais nao ha letra para
          // mudar: a seta esmaece.
          s = caixa == 2 ? "\xE2\x87\xAA" : "\xE2\x87\xA7";   // U+21EA, U+21E7 (Inter)
          est = TXT_AJ_TIT28;
          alfaTxt = camada ? 0.4f : (caixa ? 1.0f : 0.85f);
        } else if (kc == TE_K_SINAIS) { s = camada ? "abc" : "#+="; alfaTxt = 0.85f; }
        else { s = i18n("espaço"); alfaTxt = 0.85f; }
        // Rotulo traduzido numa tecla de largura fixa: um degrau de letra
        // antes de cortar.
        if (txt_linha(est, s, 243, 243, 243, 255).w > t.w - 28.0f) est = TXT_AJ_ESTADO;
      } else if (ehChar(f)) {
        ch[0] = teCaixa(celula[camada][f][c]); ch[1] = 0;
        s = ch;
        if (ch[0] == ' ') { s = i18n("espaço"); est = TXT_AJ_ESTADO; }
      } else { s = rotuloExtra(c); est = TXT_AJ_SEG; alfaTxt = 0.85f; }
      // A cor do texto em DEGRAU (a chave do cache de linhas de text.c).
      tom = k >= 0.5f ? ajustes_tinta_foco() : 243;
      { TxtLinha l = cam ? txt_linha_corta(est, s, tom, tom, tom, 255, t.w - 28.0f) : txt_linha(est, s, tom, tom, tom, 255);
        const char *ic = extra ? (c == 0 ? "aj_delete" : concluir ? "aj_check" : NULL) : NULL;
        int ponto = kc == TE_K_CAIXA && caixa == 2;
        float iw = ic ? 22.0f + 10.0f : ponto ? 8.0f + 10.0f : 0.0f, lx = t.x + (t.w - l.w - iw) * 0.5f;
        if (ic) gfx_icone((GfxRect){ lx, t.y + (t.h - 22) * 0.5f, 22, 22 }, ic, tom / 255.0f, tom / 255.0f, tom / 255.0f, (k >= 0.5f ? 1.0f : 0.85f) * a);
        if (ponto) {
          float ar, ag, ab;
          ajustes_acento(&ar, &ag, &ab);
          if (k >= 0.5f) ar = ag = ab = (float)tom / 255.0f;   // em cima do realce, o ponto vai na tinta
          gfx_cor((GfxRect){ lx, t.y + (t.h - 8.0f) * 0.5f, 8.0f, 8.0f }, 0.5f, ar, ag, ab, a);
        }
        txt_desenhar_alpha(l, lx + iw, t.y + (t.h - l.h) * 0.5f, (k >= 0.5f ? 1.0f : alfaTxt) * a); }
    }
  }
}
