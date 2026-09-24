// GIF ANIMADO nas capas de colecao.
//
// As colecoes que vem do PACOTE animam por uma sequencia de JPEG numerado
// (folder->frames + frameDir/001.jpg), tocada a ~15 fps em home.c. As que vem
// da CONTA entregam um GIF, e ai a animacao nao acontecia: o app manda todo
// arquivo para um decodificador que devolve UM quadro, entao a capa virava a
// primeira imagem parada. E o issue #29.
//
// ONDE ANIMA E ONDE NAO ANIMA, e o porque:
//
//   Tizen  — anima, e desde a 1.4.7 o GIF e decodificado AQUI, em C: LZW,
//            paleta, descarte e reducao ao tamanho do card num fio proprio
//            (gif_fio_*). O navegador nao decodifica quadro nenhum; o fio
//            principal so sobe as linhas que mudaram para a textura. Ver a
//            nota "por que nativo" em gif.c (#84).
//
//   webOS  — NAO anima, e devolve 0. O decodificador daqui compila e roda no
//            LG (e o mesmo C, testado no Mac), mas ligar a animacao la e outra
//            mudanca, nao medida no aparelho: gif_pode_animar segue 0 e o
//            chamador desenha a capa parada, como sempre.
#ifndef NV_GIF_H
#define NV_GIF_H
#include <stddef.h>
#include "gl_compat.h"

// ESTE APARELHO ANIMA GIF? 0 no webOS e no Mac; no Tizen, 1 quando o
// orcamento abaixo nao e zero (TV de 1 GB: 0, fica a foto parada). Existe para
// quem CHAMA nao pagar nada onde a resposta e nao: sem ela, o cartaz em foco
// pediria o download do GIF a cada foco, para gif_textura devolver 0 no fim —
// banda e espaco de cache gastos num arquivo que nunca vai ser desenhado.
int gif_pode_animar(void);

// ORCAMENTO DE ANIMACAO POR RAM (24/09/2026, TV de 1 GB que morria na tela de
// perfis e na home). O custo de um GIF e o que ele decodifica por volta:
// quadros x tela logica x 4 bytes (gif_custo) — o que o decodificador compoe
// por volta. `memGB` e o
// navigator.deviceMemory (0 = o navegador nao disse). Devolve bytes; 0 = nao
// anima; GIF_SEM_TETO = sem limite (o comportamento de antes). A tabela, e de
// onde veio cada numero, esta em gif.c. Aritmetica pura: testada no Mac.
#define GIF_SEM_TETO ((size_t)-1)
size_t gif_orcamento_para(double memGB);
size_t gif_custo(int quadros, int telaW, int telaH);

// Chamar UMA VEZ POR QUADRO de tela (main.c). Solta a animacao corrente
// quando ninguem pediu gif_textura ha mais de NV_GIF_OCIOSO_MS: e o que
// termina o fio de decode e libera o arquivo e os buffers quando a
// tela de perfis fecha ou o foco sai do cartaz — antes disso eles ficavam
// presos ate o PROXIMO GIF. Fora do Tizen nao faz nada.
void gif_ocioso(void);

// O arquivo e um GIF com MAIS DE UM quadro? Le so a estrutura de blocos, que e
// toda prefixada por tamanho — nao decodifica pixel nenhum.
int gif_animado(const char *caminho);

// UM QUADRO LOCALIZADO NO ARQUIVO, sem nenhum pixel decodificado.
//
// `descarte` e o metodo de descarte do GIF (campo do Graphic Control
// Extension), e ele decide o que sobra na tela ANTES do proximo quadro ser
// desenhado por cima:
//   0/1 = deixa como esta   2 = limpa a area deste quadro   3 = volta ao estado anterior
// Sem honrar isso, GIF de quadro parcial (que e a maioria dos GIF animados,
// porque so a parte que muda vai em cada quadro) vira sujeira acumulada.
typedef struct {
  int atraso;                 // ms que este quadro fica na tela
  int descarte;               // 0..3, ver acima
  int esq, topo, larg, alt;   // retangulo deste quadro dentro da tela logica
  // Faixas de BYTES no arquivo original. Uso do decodificador; nao interpretar.
  size_t gce, gceN, ini, fim;
} GifQuadro;

// Caminha pelos blocos de um GIF ja lido na memoria e preenche ate `max`
// quadros. Devolve quantos achou (0 quando nao e GIF, ou quando o arquivo
// acaba antes do primeiro quadro fechar). So conta quadro cuja cadeia de
// sub-blocos FECHOU: arquivo truncado nao vira quadro pela metade.
int gif_mapear(const unsigned char *b, size_t n, GifQuadro *q, int max);

// --- DECODIFICADOR (C puro, sem GL nem navegador) --------------------------
//
// Um GIF inteiro na memoria vira uma sequencia de quadros JA COMPOSTOS (tela
// logica com descarte honrado) e REDUZIDOS a saidaW x saidaH. So um quadro
// composto existe por vez: memoria = tela logica x 4 (x 2 com descarte 3) +
// saida x 4, e nao quadros x tela.
typedef struct GifDec GifDec;

// Tamanho da saida para um card de `largAlvo` px: nunca maior que a tela
// logica (ampliar e trabalho da GPU, de graca), proporcao da tela logica.
void gif_tamanho_saida(int telaW, int telaH, int largAlvo, int *saidaW, int *saidaH);

// GifDec passa a ser DONO de `b` (vindo de malloc) e o libera em
// gif_dec_fechar — e ja o libera quando gif_dec_abrir falha. NULL quando nao e GIF de
// 2+ quadros ou a tela passa de GIF_TELA_MAX pixels.
#define GIF_TELA_MAX (1280 * 1280)
GifDec *gif_dec_abrir(unsigned char *b, size_t n, int saidaW, int saidaH);
int  gif_dec_quadros(const GifDec *d);
void gif_dec_tela(const GifDec *d, int *w, int *h);
// Decodifica o PROXIMO quadro (volta ao 0 no fim). Devolve o indice dele e as
// linhas da saida que mudaram, [*y0, *y1) — o quadro 0 sempre muda tudo.
// Devolve -1 so se o GifDec e invalido. Dados LZW corrompidos nao falham: o
// quadro fica com o que deu para ler, como no navegador.
int  gif_dec_proximo(GifDec *d, int *y0, int *y1);
int  gif_dec_atraso(const GifDec *d, int quadro);   // ms
const unsigned char *gif_dec_saida(const GifDec *d); // RGBA, saidaW*saidaH*4
const unsigned char *gif_dec_composta(const GifDec *d); // RGBA da tela logica
void gif_dec_fechar(GifDec *d);

// --- FIO DE DECODE ------------------------------------------------------------
//
// O GifDec rodando num pthread proprio, GIF_FILA quadros a frente. Quem
// consome (o fio principal) nunca espera: gif_fio_pegar devolve 0 quando o
// proximo quadro ainda nao esta pronto. Cada quadro chega como uma FAIXA de
// linhas inteiras da saida (contiguas na memoria: sobem com um
// glTexSubImage2D so), e as faixas tem de ser consumidas EM ORDEM — somadas
// sobre o quadro anterior, reconstroem a saida.
#define GIF_FILA 3
typedef struct GifFio GifFio;
typedef struct {
  int quadro, atraso;         // indice e ms que ele fica na tela
  int y0, y1;                 // linhas [y0, y1) da saida
  const unsigned char *px;    // (y1 - y0) * saidaW * 4 bytes, linha y0 primeiro
} GifFaixa;
// Dono de `b`, como gif_dec_abrir. NULL em falha (e `b` ja foi liberado).
GifFio *gif_fio_abrir(unsigned char *b, size_t n, int saidaW, int saidaH);
void gif_fio_tamanho(const GifFio *f, int *saidaW, int *saidaH, int *quadros);
int  gif_fio_nominal(const GifFio *f);            // ms de uma volta, pelo arquivo
int  gif_fio_pegar(GifFio *f, GifFaixa *faixa);   // 1 = ha faixa; nao bloqueia
void gif_fio_devolver(GifFio *f);                 // depois de subir a faixa
// Nao bloqueia: o fio termina sozinho e libera tudo. `f` nao vale mais.
void gif_fio_fechar(GifFio *f);
// Medida (bancada, registro): quadros decodificados e ms de CPU gastos neles.
void gif_fio_medida(const GifFio *f, int *quadros, double *ms);

// Textura com o quadro que a animacao esta mostrando AGORA, ou 0 quando o alvo
// nao anima, o arquivo nao e GIF animado, o GIF passa do orcamento acima (e ai
// devolve 0 para sempre, sem reler o arquivo), ou o quadro ainda nao
// decodificou. Em todos esses casos quem chama desenha a foto parada.
// A textura e reaproveitada entre chamadas: e sempre a mesma, com o conteudo
// trocado. So um cartaz anima por vez (o que esta em foco), e e nisso que esta
// funcao se apoia para nao guardar quadro nenhum.
//
// O RELOGIO E DAQUI, e nao de quem chama: chamar A CADA DESENHO. Sem quadro
// novo vencido a chamada nao sobe nada (uma comparacao e um load atomico); com
// quadro vencido sobe so as linhas que mudaram. Quem chamava a cada 67 ms
// prendia o GIF a 15 fps, abaixo do ritmo do proprio arquivo.
GLuint gif_textura(const char *caminho, int largAlvo);

// Solta o que estiver preso ao caminho corrente. Chamar quando o foco sai.
void gif_parar(void);

#endif
