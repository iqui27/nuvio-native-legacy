// O DETECTOR DE GIF ANIMADO, SEM DECODIFICAR PIXEL E SEM ARQUIVO NO REPOSITORIO.
//
// gif_animado() responde "este arquivo tem mais de um quadro?" caminhando pela
// estrutura de blocos do GIF, que e toda prefixada por tamanho. E a pergunta
// que separa a capa de colecao que precisa animar (#29) da que so precisa ser
// desenhada: no Tizen a resposta "sim" manda a capa para gif_textura(), e no
// webOS ela nao muda nada porque la nao ha decodificador de animacao.
//
// POR QUE OS GIFS SAO MONTADOS AQUI, BYTE A BYTE, e nao gravados em
// tests/fixtures: um GIF binario no repositorio nao diz o que esta sendo
// testado. Escrito assim, cada caso e legivel — da para ver que o quadro tem
// paleta local olhando o bit, e da para cortar o arquivo num ponto ESCOLHIDO em
// vez de num offset magico.
//
// O QUE ESTE TESTE PROVA:
//   1. um quadro nao e animacao; dois quadros sao;
//   2. extensao de controle grafico (o bloco que carrega o atraso entre
//      quadros, e que na pratica todo GIF animado tem) nao confunde a contagem;
//   3. paleta local — que muda o TAMANHO do descritor de imagem — tambem nao;
//   4. arquivo truncado em QUALQUER ponto termina, nao le fora do buffer (isto
//      so tem valor sob ASan: SANITIZE=1 bash tests/gif.sh) e nao inventa
//      animacao antes de ver o segundo quadro;
//   5. arquivo que nao e GIF, arquivo vazio e arquivo que nao existe devolvem
//      0 em vez de tentar interpretar o lixo.
//
//   bash tests/gif.sh
//   SANITIZE=1 bash tests/gif.sh
#include "../src/gif.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAMINHO "/tmp/nuvio-gif-teste.gif"

// --- montador ---------------------------------------------------------------
static unsigned char buf[8192];
static size_t nbuf;
static int    nQuadros;           // quantos quadro() ja entraram no buffer
static size_t offSegundoQuadro;   // onde o separador 0x2C do 2o quadro caiu

static void b1(unsigned v) { assert(nbuf < sizeof buf); buf[nbuf++] = (unsigned char)v; }
static void b2(unsigned v) { b1(v & 0xFF); b1((v >> 8) & 0xFF); }
static void bn(const char *s, size_t n) { for (size_t i = 0; i < n; i++) b1((unsigned char)s[i]); }

// Cabecalho + descritor de tela logica. `paleta` liga a paleta GLOBAL, que sao
// 3 * 2^(N+1) bytes logo depois do descritor — com N=0, seis bytes. O detector
// tem de pula-los para achar o primeiro bloco.
static void cabecalho(const char *assinatura, int paleta) {
  nbuf = 0; nQuadros = 0; offSegundoQuadro = 0;
  bn(assinatura, 6);
  b2(2); b2(2);                    // 2x2 pixels; o tamanho nao importa aqui
  b1(paleta ? 0x80 : 0x00);        // bit 7: ha paleta global, N = 0
  b1(0);                           // cor de fundo
  b1(0);                           // proporcao do pixel
  if (paleta) for (int i = 0; i < 6; i++) b1(0);
}

// Uma cadeia de sub-blocos com UM sub-bloco de `n` bytes, mais o terminador.
static void subBlocos(unsigned n) {
  b1(n);
  for (unsigned i = 0; i < n; i++) b1(0x00);
  b1(0x00);                        // tamanho zero fecha a cadeia
}

// Extensao de controle grafico: e ela que carrega o atraso entre quadros, e
// vem ANTES de cada imagem num GIF animado de verdade.
static void controleGrafico(void) {
  b1(0x21); b1(0xF9);
  b1(4); b1(0x00); b2(10); b1(0x00);   // sub-bloco de 4 bytes
  b1(0x00);
}

// Extensao de aplicacao NETSCAPE2.0 (o bloco de repeticao). Serve para provar a
// cadeia de sub-blocos com MAIS DE UM sub-bloco.
static void extensaoNetscape(void) {
  b1(0x21); b1(0xFF);
  b1(11); bn("NETSCAPE2.0", 11);
  b1(3); b1(1); b2(0);
  b1(0x00);
}

// Um quadro. `paletaLocal` liga a paleta LOCAL, que muda o tamanho do bloco:
// quem contar 9 bytes fixos de descritor e seguir em frente cai no meio da
// paleta e para de entender o arquivo.
static void quadro(int paletaLocal) {
  // Onde o SEGUNDO quadro comeca: o teste de truncagem usa este offset para
  // saber ate onde o arquivo ainda nao pode ser chamado de animado.
  if (nQuadros == 1) offSegundoQuadro = nbuf;
  nQuadros++;
  b1(0x2C);
  b2(0); b2(0); b2(2); b2(2);          // esquerda, topo, largura, altura
  b1(paletaLocal ? 0x80 : 0x00);       // bit 7: ha paleta local, N = 0
  if (paletaLocal) for (int i = 0; i < 6; i++) b1(0);
  b1(2);                               // tamanho minimo do codigo LZW
  subBlocos(3);                        // dados comprimidos, que ninguem le
}

static void fim(void) { b1(0x3B); }

static void gravarBruto(const unsigned char *p, size_t n) {
  FILE *f = fopen(CAMINHO, "wb");
  size_t esc;
  int fechou;
  assert(f);
  esc = n ? fwrite(p, 1, n, f) : 0;
  fechou = fclose(f);
  assert(esc == n && fechou == 0);
}

// Os primeiros `n` bytes do que foi montado. `n` igual a nbuf grava tudo, e
// zero grava um arquivo VAZIO — que e um caso de teste, nao um atalho.
static void gravar(size_t n) {
  assert(n <= nbuf);
  gravarBruto(buf, n);
}

static void gravarTudo(void) { gravarBruto(buf, nbuf); }

// --- casos ------------------------------------------------------------------
int main(void) {
  // A) UM QUADRO NAO E ANIMACAO. E a capa parada, que e o caso comum: a maioria
  //    das pastas nao tem GIF de foco, e as que tem uma imagem so nao podem
  //    entrar no caminho de animacao — la o quadro seria recomposto a cada
  //    volta sem nada mudar na tela.
  cabecalho("GIF89a", 1);
  quadro(0);
  fim();
  gravarTudo();
  assert(gif_animado(CAMINHO) == 0);
  puts("ok  um quadro nao e animacao");

  // B) DOIS QUADROS SAO. O minimo que faz o detector dizer sim.
  cabecalho("GIF89a", 1);
  quadro(0);
  quadro(0);
  fim();
  gravarTudo();
  assert(gif_animado(CAMINHO) == 1);
  puts("ok  dois quadros sao animacao");

  // C) GIF87a TAMBEM CONTA. O detector compara so "GIF8" de proposito: 87a e
  //    89a tem a mesma estrutura de blocos, e recusar o 87a rejeitaria arquivo
  //    valido por causa de um digito.
  cabecalho("GIF87a", 1);
  quadro(0);
  quadro(0);
  fim();
  gravarTudo();
  assert(gif_animado(CAMINHO) == 1);
  puts("ok  GIF87a de dois quadros tambem e animacao");

  // D) EXTENSAO DE CONTROLE GRAFICO ANTES DE CADA QUADRO. E a forma real de um
  //    GIF animado — sem ela nao ha atraso entre quadros. A extensao NAO e
  //    quadro: contar blocos em vez de imagens daria 4 aqui.
  cabecalho("GIF89a", 1);
  extensaoNetscape();
  controleGrafico(); quadro(0);
  controleGrafico(); quadro(0);
  fim();
  gravarTudo();
  assert(gif_animado(CAMINHO) == 1);
  puts("ok  extensao de controle grafico nao vira quadro");

  //    E o mesmo arquivo com UM quadro so continua nao sendo animacao: prova
  //    que o "sim" acima veio das imagens e nao das extensoes.
  cabecalho("GIF89a", 1);
  extensaoNetscape();
  controleGrafico(); quadro(0);
  fim();
  gravarTudo();
  assert(gif_animado(CAMINHO) == 0);
  puts("ok  extensoes sozinhas nao fazem um quadro virar dois");

  // E) PALETA LOCAL. Ela fica ENTRE o descritor de imagem e os dados, e o
  //    tamanho dela sai dos bits do proprio descritor. Quem pular 9 bytes fixos
  //    cai dentro da paleta, le uma cor como se fosse separador de bloco e
  //    desiste — o GIF animado passaria por parado.
  cabecalho("GIF89a", 0);          // sem paleta global: so a local existe
  controleGrafico(); quadro(1);
  controleGrafico(); quadro(1);
  fim();
  gravarTudo();
  assert(gif_animado(CAMINHO) == 1);
  puts("ok  paleta local nao desalinha a leitura dos blocos");

  // F) TRUNCADO EM QUALQUER PONTO. Um download interrompido, ou um corpo de
  //    erro do CDN gravado com o nome do arquivo, chega assim.
  //
  //    Sao tres afirmacoes numa varredura so:
  //      - TERMINA. Se o caminhamento entrasse em laco (um sub-bloco de tamanho
  //        zero que nao fechasse a cadeia, um `p` que nao avanca) o teste nao
  //        acabaria nunca, e isso e uma falha tao visivel quanto um assert.
  //      - NAO LE FORA. So vale rodando com SANITIZE=1; sem ASan a leitura
  //        invalida passaria despercebida.
  //      - NAO INVENTA. Enquanto o arquivo acaba antes do separador do segundo
  //        quadro, nao ha como haver dois quadros: a resposta tem de ser 0.
  cabecalho("GIF89a", 1);
  controleGrafico(); quadro(1);
  controleGrafico(); quadro(1);
  controleGrafico(); quadro(1);
  fim();
  assert(offSegundoQuadro > 0 && offSegundoQuadro < nbuf);
  { size_t corte;
    size_t inteiro = nbuf;
    for (corte = 0; corte <= inteiro; corte++) {
      int r;
      gravar(corte);
      r = gif_animado(CAMINHO);
      assert(r == 0 || r == 1);
      if (corte <= offSegundoQuadro + 1) assert(r == 0);
    }
    // O arquivo inteiro, no fim da varredura, continua sendo animacao: a
    // varredura nao pode ter deixado estado para tras.
    gravarTudo();
    assert(gif_animado(CAMINHO) == 1); }
  puts("ok  truncado em qualquer ponto termina, nao le fora e nao inventa quadro");

  // G) SUB-BLOCO QUE PROMETE MAIS BYTES DO QUE O ARQUIVO TEM. Nao e truncagem
  //    acidental: e o campo de tamanho MENTINDO, que e como um arquivo
  //    corrompido derruba um leitor ingenuo. `p + len > n` e a guarda.
  cabecalho("GIF89a", 1);
  b1(0x2C);
  b2(0); b2(0); b2(2); b2(2);
  b1(0x00);
  b1(2);
  b1(0xFF);                        // 255 bytes de dados que nao existem
  b1(0x00); b1(0x00);
  gravarTudo();
  assert(gif_animado(CAMINHO) == 0);
  puts("ok  sub-bloco maior que o arquivo nao le fora do buffer");

  //    A mesma mentira dentro de uma EXTENSAO, que segue outro ramo do codigo.
  cabecalho("GIF89a", 1);
  b1(0x21); b1(0xF9);
  b1(0xFF);
  b1(0x00);
  gravarTudo();
  assert(gif_animado(CAMINHO) == 0);
  puts("ok  extensao com tamanho mentiroso nao le fora do buffer");

  // H) SEPARADOR DESCONHECIDO. Um byte que nao e 0x21, 0x2C nem 0x3B significa
  //    que a leitura se perdeu; seguir adiante seria ler ruido como estrutura.
  cabecalho("GIF89a", 1);
  quadro(0);
  b1(0x99);
  quadro(0);
  fim();
  gravarTudo();
  assert(gif_animado(CAMINHO) == 0);
  puts("ok  separador desconhecido para a leitura em vez de adivinhar");

  // I) NAO E GIF. O cache de disco aceita JPEG, PNG, GIF e WEBP no mesmo lugar,
  //    entao a maioria dos arquivos que chegam aqui NAO e GIF.
  { unsigned char jpeg[64];
    memset(jpeg, 0x20, sizeof jpeg);
    jpeg[0] = 0xFF; jpeg[1] = 0xD8; jpeg[2] = 0xFF; jpeg[3] = 0xE0;
    gravarBruto(jpeg, sizeof jpeg); }
  assert(gif_animado(CAMINHO) == 0);
  puts("ok  arquivo que nao e GIF devolve 0");

  //    Inclusive um que COMECA parecido: "GIF7" nao e assinatura de GIF.
  cabecalho("GIF7xx", 1);
  quadro(0);
  quadro(0);
  fim();
  gravarTudo();
  assert(gif_animado(CAMINHO) == 0);
  puts("ok  assinatura parecida nao passa por GIF");

  // J) ARQUIVO VAZIO, e o curto demais para ter cabecalho.
  gravarBruto((const unsigned char *)"", 0);
  assert(gif_animado(CAMINHO) == 0);
  gravarBruto((const unsigned char *)"GIF89a\0\0\0\0\0\0", 12);
  assert(gif_animado(CAMINHO) == 0);
  puts("ok  arquivo vazio e arquivo curto demais devolvem 0");

  // K) O QUE NEM CHEGA A SER ARQUIVO. gif_animado e chamada com o que o cache
  //    de disco devolver, e ele devolve NULL enquanto o download nao chegou.
  assert(gif_animado(NULL) == 0);
  assert(gif_animado("") == 0);
  assert(gif_animado("/tmp/nuvio-gif-que-nao-existe.gif") == 0);
  puts("ok  NULL, vazio e caminho inexistente devolvem 0");

  // L) FORA DO TIZEN NAO HA ANIMACAO, e isso e contrato e nao acidente: o
  //    SDL2_image da TV LG so exporta IMG_LoadGIF_RW, que devolve UM quadro.
  //    Ver o cabecalho de src/gif.h. Chamar mesmo assim tem de ser inofensivo.
  cabecalho("GIF89a", 1);
  quadro(0); quadro(0);
  fim();
  gravarTudo();
  assert(gif_textura(CAMINHO, 480) == 0);
  assert(gif_textura(NULL, 480) == 0);
  gif_parar();
  gif_parar();                     // duas vezes seguidas nao pode reclamar
  puts("ok  fora do Tizen gif_textura devolve 0 e gif_parar e inofensiva");

  remove(CAMINHO);
  puts("gif: tudo ok");
  return 0;
}
