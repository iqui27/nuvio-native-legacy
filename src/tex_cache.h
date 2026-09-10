// Cache de texturas com decode FORA da thread de desenho.
//
// Por que thread: decode medido no aparelho custa ~30ms por imagem. A 60fps o
// quadro inteiro tem 16,6ms — decodificar em linha significa perder 2 quadros
// por card que entra na tela. A thread decodifica para memoria; a thread de
// desenho so faz o upload GL (que precisa do contexto e e barato).
//
// Politica: LRU com teto de itens. Sem teto, percorrer o catalogo inteiro
// estoura a memoria do app — a TV tem orcamento apertado e ja vimos o web app
// em 266MB.
#ifndef NV_TEX_CACHE_H
#define NV_TEX_CACHE_H
#include "gl_compat.h"

int  tex_iniciar(int max_itens);

// Pasta onde as imagens vindas de URL sao guardadas em disco. Sem ela,
// tex_obter com http(s) simplesmente nao carrega — o app nao quebra, so fica
// sem arte.
void tex_cache_dir(const char *dir);
void tex_encerrar(void);

// Devolve a textura se ja estiver pronta; senao 0 e enfileira o decode.
// Nunca bloqueia a thread de desenho.
GLuint tex_obter(const char *caminho);
// Mesma coisa, com teto de 1920: para a arte que ocupa a tela inteira (hero da
// home, backdrop do detalhe, arte do player). Com o teto comum de 960 essas
// tres eram decodificadas com metade da resolucao e ampliadas na tela.
GLuint tex_obter_hero(const char *caminho);

// Escala entre o pixel do BUFFER e o pixel de layout (1 na TV, 2 no Mac
// retina). Definir uma vez no arranque, junto com a do texto.
void tex_escala(float e);

// Como tex_obter, mas dizendo COM QUE LARGURA a arte vai ser desenhada, em
// pixels de layout. O teto de decodificacao sai dai, em vez do padrao unico de
// 640 — que foi dimensionado pela maior arte de card e cobrava o mesmo preco de
// um poster de 212. Ver a nota em tex_cache.c: e a diferenca entre caber ~40
// texturas no orcamento e caber ~230.
//
// Prefira esta a tex_obter em qualquer arte de lista: e onde o cache estoura.
GLuint tex_obter_larg(const char *caminho, float largLayout);

// Caminho do ARQUIVO local de uma URL, ou NULL enquanto ele nao chegou. Um
// caminho que ja e local volta como veio. Nunca bloqueia: quando o arquivo
// ainda nao esta no cache de disco, pede o download pela MESMA fila de rede das
// imagens e devolve NULL — chamar de novo no quadro seguinte e o esperado.
//
// Existe para o GIF de foco das colecoes (#29): gif_textura() precisa do
// ARQUIVO para montar o blob que a <img> anima, e nao de uma textura. tex_obter
// nao serve porque devolve UM quadro decodificado e nenhum caminho de volta.
//
// Devolve ponteiro para buffer ESTATICO: use antes da proxima chamada, e so da
// thread de desenho.
const char *tex_arquivo(const char *url);

// Proporcao (w/h) da textura ja carregada; 0 se ainda nao esta pronta.
// Necessaria para o "cover" do shader — sem ela a arte estica.
float tex_aspecto(const char *caminho);

// 1 quando a arte e uma marca ESCURA E ACROMATICA — o caso do logo preto — e
// portanto deve ser desenhada tingida (GFX_MARCA) em vez de com as cores dela.
//
// Existe por causa do LOGO DO TITULO. O TMDB serve a mesma marca em versao
// clara e escura e NAO diz qual e qual: nao ha campo para isso, e o ranking do
// proprio app web ordena so por idioma e nota. Quando cai a escura, ela some
// sobre o backdrop escuro.
//
// SAO DUAS CONDICOES, e a segunda importa tanto quanto a primeira: escura o
// bastante (luminancia) E sem cor propria (croma). So a luminancia tingiria de
// branco tambem um logo de marca vermelho-escuro, que e cor deliberada e nao a
// variante errada — trocaria um defeito por outro. Ambas medidas uma unica vez,
// na thread de decode, amostrando 1/16 dos pixels opacos.
//
// Responde 0 enquanto a textura nao carregou: nao tingir e o padrao seguro.
int  tex_marca_escura(const char *caminho);

// Chamar uma vez por quadro, na thread de desenho: sobe para a GPU o que a
// thread de decode terminou. Devolve quantas subiu.
int tex_bombear(int max_por_quadro);

// Telemetria de quadro do cache: quantas buscas por caminho e quanto custaram.
// acharIndice era LINEAR sobre 192 slots e cada card da lista chama 2-3 vezes
// por quadro; estes numeros dizem se isso pesa de verdade ou nao.
extern int    tex_n_busca;
extern double tex_ms_busca;
void tex_novo_quadro(void);

void tex_estatisticas(int *itens, int *pendentes, long *bytes);

#endif

// Quanto o cache de DISCO ocupa. No Tizen o "disco" e MEMFS: isto e RAM que
// cresce a sessao inteira e nao aparece no [mem] nem no total de texturas.
long tex_cache_disco_bytes(void);
