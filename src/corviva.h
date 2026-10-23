// COR VIVA: o tema "Dinâmica" de "Cor de destaque" (dono, 25/09/2026: "a cor
// do accent e a cor predominante do hero [...] vai ser uma interface viva").
//
// Tres pecas, e so tres:
//   1. EXTRACAO (corviva_extrair): uma cor de destaque e uma cor de base a
//      partir dos pixels que o fio de decode de tex_cache.c JA TEM na mao. Nao
//      ha leitura de volta da GPU nem decode a mais: e a mesma superficie que
//      vira textura, amostrada numa grade de 32x18.
//   2. QUEM MANDA NA COR (corviva_definir): home, detalhe e player dizem, no
//      desenho, qual arte e a do titulo em cena. Vale o pedido de MAIOR
//      prioridade do quadro (o player ganha do detalhe, que ganha da home), e
//      so depois de ele ficar parado 150 ms — rolar o destaque depressa nao
//      pisca a interface inteira.
//   3. MOVIMENTO (corviva_quadro): uma vez por quadro, no laco principal, a
//      cor corrente anda ate o alvo em 450 ms com saida suave, interpolada em
//      OKLab. ajustes_acento() so LE o resultado — o caminho quente continua
//      sendo tres floats copiados.
//
// Fora de home, detalhe e player (Ajustes, guia, busca, biblioteca) ninguem
// pede nada, e a cor FICA a do ultimo titulo: e o que faz "viva" nao virar
// "piscando" ao abrir os Ajustes. Sem titulo nenhum ainda (primeiro arranque),
// e a cor padrao (branco) — mas o ultimo titulo sobrevive ao fechamento em
// corviva.txt, entao o primeiro quadro do arranque seguinte ja nasce colorido.
//
// DEPOIS DO RELATO DO DONO (25/09/2026, "as cores muito parecidas, nao ta
// pegando direito"), mais tres coisas: o destaque pode vir do LOGO do titulo
// ("Cor da logo"), as superficies de destaque podem ser um DEGRADE das cores
// dele ("Dinâmica gradiente") e as cores da arte podem VAZAR na interface como
// luz ambiente ("Dinâmica imersiva", gfx_ambiente).
//
// Este modulo NAO depende de ajustes, gfx nem SDL: quem chama passa o modo e o
// relogio. E o que deixa tests/corviva.sh compilar so ele.
#ifndef NV_CORVIVA_H
#define NV_CORVIVA_H

// Modo, na ordem das opcoes de "Cor de destaque" que o ligam. Cada um contem
// o anterior.
//
// CORES DE DESTAQUE 03/10/2026 (acentos-mockup.html, aprovado): o 2 era a
// "Dinâmica estilizada" (base tingida); saiu da lista — a base tingida virou o
// Fundo "Frost" (fundo.h), que vale para qualquer acento. O numero fica vago
// para ninguem reaproveita-lo com outro sentido.
#define CORVIVA_DESLIGADA  0
#define CORVIVA_SIMPLES    1   // "Da arte": o destaque segue a arte
#define CORVIVA_GRADIENTE  3   // "Gradiente": + superficies de destaque em degrade
#define CORVIVA_IMERSIVA   4   // "Imersiva": + a cor da arte vazando como luz
#define CORVIVA_TEXTURA    5   // "Textura": foco/progresso feitos do material do titulo
#define CORVIVA_TEXTURA_SUTIL 6 // "Textura sutil": a textura a 35% sobre o Da arte
#define CORVIVA_SIMPLES_CLARO 7 // Pastel art accent; Vivid remains CORVIVA_SIMPLES.

// Prioridade de quem pede (maior ganha dentro do mesmo quadro).
#define CORVIVA_HOME     1
#define CORVIVA_DETALHE  2
#define CORVIVA_PLAYER   3

typedef struct {
  int   ok;           // 0 = arte sem cor que preste (cinza, preto e branco)
  int   transparente; // >= 20% das amostras transparentes: e um LOGO
  float acento[3];    // sRGB 0..1, ja com luminosidade/croma no limite
  float base[3];      // sRGB 0..1, o fundo escuro tingido (estilizado em diante)
  float grad[3][3];   // tres paradas do degrade, da mais clara a mais escura
  float regiao[4][3]; // luz de esquerda, direita, topo e base (imersiva)
  // TEXTURA: o recorte da arte (ou do logo) que vira o material da pilula em
  // foco. x, y, w, h em fracao da imagem, janela 3,2:1; a luminancia relativa
  // do recorte nos percentis 10 e 90 (o transparente do logo composto sobre o
  // destaque, que e o que fica por baixo na pilula) e a cor media.
  int   txOk;
  float tx[4];
  float txY[2];
  float txMedia[3];
} CorvivaPaleta;

// AS QUATRO CORES QUE SAEM DE UM DESTAQUE (acentos-mockup.html, C.tok): a
// marca sobre o escuro (texto/selo na cor do acento sobre a ilha #121316), o
// tom do "HDR" de grupo (metade marca, metade cinza #a3a19c), a luz do Frost e
// da Imersiva (o mesmo matiz com L 0,42 e croma <= 0,11) e a tinta: branca
// quando o branco contrasta mais com o preenchimento que a tinta escura
// #121316 — e assim que claros levam texto escuro e profundos texto branco.
typedef struct {
  float marca[3], hdr[3], luz[3];
  int   tintaBranca;
} CorvivaTokens;
void corviva_tokens(const float fill[3], CorvivaTokens *t);
// "Da arte": a cor crua da arte (a media do balde vencedor) passada pelas
// travas dos acentos fixos. Croma 0,08-0,24 (o da arte, mantido — dono,
// 14/10: "mais amplo as cores"); matiz amarelo (70-115 graus) vira
// CLARO (L >= 0,86, tinta escura) em vez de oliva; o resto e PROFUNDO: o maior L
// em que o branco ainda le a 4,6:1. Croma < 0,04 (arte cinza) = Branco.
void corviva_da_arte(const float bruto[3], float fill[3]);
// Contraste WCAG entre duas luminancias relativas.
float corviva_contraste_y(float ya, float yb);

// A TEXTURA DO TITULO EM CENA, decidida uma vez por quadro em corviva_quadro.
// Fonte: o logo, se ele tem cor e recorte; senao a arte. `base` e o Da arte da
// fonte (a cor lisa por baixo e o que marcas e chips usam). Tinta pelo pior
// pixel do recorte (p90 para branca, p10 para escura); abaixo de 4,5:1, `veu`
// e o menor alfa de um veu macio atras do rotulo (preto para tinta branca,
// branco para escura) que chega la, em passos de 5%.
typedef struct {
  int   ok, logo, tintaBranca, fraca;   // fraca: recorte chapado, sem material
  char  url[512];
  float janela[4];
  float veu, contraste, forca;   // forca: 1 = Textura, 0,35 = Textura sutil
  float base[3], media[3];
} CorvivaTextura;
extern CorvivaTextura nv_textura_viva;
// A tinta e o veu de um recorte (p10/p90 de luminancia relativa), sobre a
// base `base` quando `forca` < 1 (a textura a `forca` sobre a cor lisa).
void corviva_textura_tinta(float p10, float p90, const float base[3], float forca,
                           int *tintaBranca, float *veu, float *contraste);

// Pixels R,G,B,A (ABGR8888 do SDL em little endian), `pitch` em bytes. Pura:
// sem estado, sem alocacao. Devolve p->ok.
int  corviva_extrair(const unsigned char *px, int w, int h, int pitch,
                     CorvivaPaleta *p);
// Guarda a paleta de `chave` (a url da arte, a mesma do cache de texturas).
// Pode ser chamada de QUALQUER fio: o fio de decode e quem chama.
void corviva_anotar(const char *chave, const CorvivaPaleta *p);
// A paleta ja anotada de `chave`, sem pedir nada nem mexer na cena: 1 se ha.
// E o que a previa do cartao da 1.4.8 (novidades148.c) usa para mostrar tres
// titulos com as cores de cada um sem trocar a cor da interface.
int  corviva_paleta(const char *chave, CorvivaPaleta *p);
// A paleta do titulo em cena (o que corviva_definir assentou), do fio de desenho.
int  corviva_cena_paleta(CorvivaPaleta *p);
// Diz qual arte esta em cena neste quadro. Do fio de desenho. Barato: um hash
// de string e uma comparacao; chamar todo quadro e o uso esperado.
void corviva_definir(const char *chave, int prioridade);
// O LOGO do mesmo titulo (url do clearlogo). Vale junto do pedido de mesma
// prioridade; com "Cor da logo" ligado, o destaque e o degrade saem dele, e a
// arte fica com a base e a luz ambiente. Logo branco/preto cai na arte.
void corviva_definir_logo(const char *chave, int prioridade);
// Uma vez por quadro, antes do desenho. `dt` em segundos.
void corviva_quadro(float dt, int modo, int usarLogo, int reduzido);

// A cor corrente (animada). So faz sentido com o modo ligado.
void corviva_acento(float *r, float *g, float *b);
// A cor do logo do titulo em cena (ou da arte, se o logo e branco/preto), como a
// Imersiva a escolhe, valha o tema que valer. 0 = sem paleta ainda.
int  corviva_logo_acento(float *r, float *g, float *b);
// Fundo corrente: NV_COR_FUNDO (#0D0D0D) (o estilizado que o tingia saiu). E este vetor que
// NV_COR_FUNDO_R/G/B leem (layout.h), e gfx.c o passa as rampas do destaque e
// do detalhe — por isso o fundo tingido nao exige editar cada tela.
extern float nv_cor_fundo_viva[3];
// O MESMO destaque que corviva_acento devolve, e o degrade dele: gfx.c pinta em
// degrade todo GFX_COR/GFX_ANEL cuja cor seja EXATAMENTE nv_acento_viva quando
// nv_grad_ativo — e assim que as superficies de destaque de ~40 arquivos viram
// degrade sem nenhum deles mudar. Ver gfx_rect.
extern float nv_acento_viva[3];
extern float nv_grad_viva[3][3];
extern int   nv_grad_ativo;
// Luz ambiente (imersiva): as quatro cores de regiao, a forca 0..1 (entra e
// sai com o modo) e o relogio em segundos para a "respiracao" (0 com
// animacoes reduzidas: a luz fica parada).
extern float nv_ambiente_viva[4][3];
extern float nv_ambiente_forca;
extern float nv_tempo_viva;
// A luz do destaque corrente (CorvivaTokens.luz), para o vidro da Imersiva:
// as ilhas ganham 16% deste matiz junto com nv_ambiente_forca.
extern float nv_luz_viva[3];

// corviva.txt: carregar no arranque (depois de dados_iniciar) e gravar quando
// houver novidade. A gravacao se limita sozinha a uma a cada 20 s.
void corviva_carregar(void);
void corviva_gravar_se_preciso(int forcar);

// Para os testes: conversoes e a contagem de retargets.
void corviva_srgb_para_oklab(const float rgb[3], float lab[3]);
void corviva_oklab_para_srgb(const float lab[3], float rgb[3]);
float corviva_contraste(const float a[3], const float b[3]);   // WCAG, >= 1
int  corviva_retargets(void);
void corviva_zerar(void);

#endif
