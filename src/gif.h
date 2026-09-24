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
//   Tizen  — anima. Nao mais porque "o navegador anima a <img> sozinho": ver
//            a nota grande em gif.c, secao "animacao". Quem conta o tempo e
//            troca de quadro e o APP; do navegador so se usa o decodificador
//            de imagem PARADA, quadro a quadro.
//
//   webOS  — NAO anima, e devolve 0. O SDL2_image da TV so exporta
//            IMG_LoadGIF_RW (um quadro; nao ha IMG_LoadAnimation, conferido no
//            aparelho com `nm -D`), e nao existe libgif no sistema — so
//            libwebp, que e como webp.c se safa. Animar la exige um
//            decodificador LZW proprio, que e trabalho de verdade e nao esta
//            escrito. O chamador desenha a capa parada, como hoje.
//            gif_mapear/gif_montar abaixo sao metade desse caminho e ja estao
//            compiladas aqui: falta so o LZW.
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
// quadros x tela logica x 4 bytes (gif_custo) — e o que o Worker compoe, quadro
// a quadro, e o que a <img> de reserva guardaria inteiro. `memGB` e o
// navigator.deviceMemory (0 = o navegador nao disse). Devolve bytes; 0 = nao
// anima; GIF_SEM_TETO = sem limite (o comportamento de antes). A tabela, e de
// onde veio cada numero, esta em gif.c. Aritmetica pura: testada no Mac.
#define GIF_SEM_TETO ((size_t)-1)
size_t gif_orcamento_para(double memGB);
size_t gif_custo(int quadros, int telaW, int telaH);

// Chamar UMA VEZ POR QUADRO de tela (main.c). Solta a animacao corrente
// quando ninguem pediu gif_textura ha mais de NV_GIF_OCIOSO_MS: e o que
// libera os quadros comprimidos, a sessao do Worker e os canvases quando a
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
  // Faixas de BYTES no arquivo original. Uso de gif_montar; nao interpretar.
  size_t gce, gceN, ini, fim;
} GifQuadro;

// Caminha pelos blocos de um GIF ja lido na memoria e preenche ate `max`
// quadros. Devolve quantos achou (0 quando nao e GIF, ou quando o arquivo
// acaba antes do primeiro quadro fechar). So conta quadro cuja cadeia de
// sub-blocos FECHOU: arquivo truncado nao vira quadro pela metade.
int gif_mapear(const unsigned char *b, size_t n, GifQuadro *q, int max);

// Monta o quadro `q` como um GIF de UM QUADRO SO — mesmo cabecalho, mesma tela
// logica, mesma paleta global, mais o bloco de imagem dele. E o que permite
// entregar um quadro por vez a um decodificador de imagem parada.
//
// Com `saida` NULL devolve quantos bytes o resultado ocupa; com `saida` e
// `cap` suficientes, escreve e devolve o mesmo numero. Devolve 0 em erro.
size_t gif_montar(const unsigned char *b, size_t n, const GifQuadro *q,
                  unsigned char *saida, size_t cap);

// Textura com o quadro que a animacao esta mostrando AGORA, ou 0 quando o alvo
// nao anima, o arquivo nao e GIF animado, o GIF passa do orcamento acima (e ai
// devolve 0 para sempre, sem reler o arquivo), ou o quadro ainda nao
// decodificou. Em todos esses casos quem chama desenha a foto parada.
// A textura e reaproveitada entre chamadas: e sempre a mesma, com o conteudo
// trocado. So um cartaz anima por vez (o que esta em foco), e e nisso que esta
// funcao se apoia para nao guardar quadro nenhum.
//
// O RELOGIO E DAQUI, e nao de quem chama: quem chama pode perguntar na
// frequencia que quiser (home.c pergunta a cada 67 ms) que o quadro so troca
// quando o `atraso` do proprio GIF vence.
GLuint gif_textura(const char *caminho, int largAlvo);

// Solta o que estiver preso ao caminho corrente. Chamar quando o foco sai.
void gif_parar(void);

#endif
