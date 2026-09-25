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
// Este modulo NAO depende de ajustes, gfx nem SDL: quem chama passa o modo e o
// relogio. E o que deixa tests/corviva.sh compilar so ele.
#ifndef NV_CORVIVA_H
#define NV_CORVIVA_H

// Modo, na ordem das opcoes de "Cor de destaque" que o ligam.
#define CORVIVA_DESLIGADA  0
#define CORVIVA_SIMPLES    1   // "Dinâmica": so o destaque segue a arte
#define CORVIVA_ESTILIZADA 2   // "Dinâmica estilizada": destaque + base tingida

// Prioridade de quem pede (maior ganha dentro do mesmo quadro).
#define CORVIVA_HOME     1
#define CORVIVA_DETALHE  2
#define CORVIVA_PLAYER   3

typedef struct {
  int   ok;          // 0 = arte sem cor que preste (cinza, preto e branco)
  float acento[3];   // sRGB 0..1, ja com luminosidade/croma no limite
  float base[3];     // sRGB 0..1, o fundo escuro tingido (so o estilizado usa)
} CorvivaPaleta;

// Pixels R,G,B,A (ABGR8888 do SDL em little endian), `pitch` em bytes. Pura:
// sem estado, sem alocacao. Devolve p->ok.
int  corviva_extrair(const unsigned char *px, int w, int h, int pitch,
                     CorvivaPaleta *p);
// Guarda a paleta de `chave` (a url da arte, a mesma do cache de texturas).
// Pode ser chamada de QUALQUER fio: o fio de decode e quem chama.
void corviva_anotar(const char *chave, const CorvivaPaleta *p);
// Diz qual arte esta em cena neste quadro. Do fio de desenho. Barato: um hash
// de string e uma comparacao; chamar todo quadro e o uso esperado.
void corviva_definir(const char *chave, int prioridade);
// Uma vez por quadro, antes do desenho. `dt` em segundos.
void corviva_quadro(float dt, int modo, int reduzido);

// A cor corrente (animada). So faz sentido com o modo ligado.
void corviva_acento(float *r, float *g, float *b);
// Fundo corrente: NV_COR_FUNDO (#0D0D0D) fora do estilizado. E este vetor que
// NV_COR_FUNDO_R/G/B leem (layout.h), e gfx.c o passa as rampas do destaque e
// do detalhe — por isso o fundo tingido nao exige editar cada tela.
extern float nv_cor_fundo_viva[3];

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
