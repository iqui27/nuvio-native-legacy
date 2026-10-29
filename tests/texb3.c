// B3 (05/10/2026): o que a frente de texturas mudou, conferido com as funcoes
// reais do tex_cache.
//  1. ENVIO EM FAIXAS: heroi de 1920x1080 sobe em 4 faixas, uma por quadro com
//     o orcamento estourado, so vira PRONTO na ultima, e os pixels saem iguais.
//  2. O decodificado URGENTE (heroi/logo) sobe antes do cartaz.
//  3. DISCO DIRETO: arte que o indice do disco ja tem vai a fila de DECODE,
//     sem passar pela fila de rede.
//  4. MEMORIA DE ARTE INEXISTENTE: 404 lembrado (e no disco), prazo estourado
//     so por 30 s.
#define NV_TEX_UPLOAD_BUDGET_MS 0.0   // uma faixa por chamada: o pior caso (C9)
#include "../src/sdlcompat.h"
#include <SDL2/SDL_image.h>
#include <unistd.h>
#include "../src/tex_cache.c"
#include <assert.h>
#include <stdio.h>
#include "nvtmp.h"

static void preparar(void) {
  memset(itens, 0, sizeof itens);
  nMax = 16;
  filaIni = filaFim = decIni = decFim = 0;
  mtx = SDL_CreateMutex(); cond = SDL_CreateCond();
  condDec = SDL_CreateCond(); condLivre = SDL_CreateCond();
  rodando = 1;
  orcamento = 512L * 1024 * 1024;
  bytesUsados = 0;
}

static SDL_Surface *superficie(int w, int h, unsigned semente) {
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
  int x, y;
  assert(s);
  for (y = 0; y < h; y++) {
    unsigned char *l = (unsigned char *)s->pixels + (size_t)y * s->pitch;
    for (x = 0; x < w; x++) {
      l[x * 4 + 0] = (unsigned char)(x + semente);
      l[x * 4 + 1] = (unsigned char)(y * 3);
      l[x * 4 + 2] = (unsigned char)((x ^ y) + semente);
      l[x * 4 + 3] = 255;
    }
  }
  return s;
}

static void decodificado(int i, const char *nome, SDL_Surface *s, int urgente) {
  snprintf(itens[i].caminho, sizeof itens[i].caminho, "%s", nome);
  itens[i].hash = hashCaminho(nome);
  itens[i].estado = DECODIFICADO;
  itens[i].sup = s;
  itens[i].urgente = urgente;
  itens[i].limite = urgente ? 1920 : 640;
}

int main(void) {
  SDL_Window *w; SDL_GLContext gl;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("texb3", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w); gl = SDL_GL_CreateContext(w); assert(gl);

  // 1. Faixas.
  preparar();
  { SDL_Surface *s = superficie(1920, 1080, 7);
    SDL_Surface *copia = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
    int chamadas = 0, subiu = 0, i;
    unsigned char *lido;
    decodificado(0, "https://x/heroi.jpg", s, 1);
    while (!subiu && chamadas < 20) {
      tex_upl_bytes = 0;
      subiu = tex_bombear(3);
      chamadas++;
      assert(tex_upl_bytes <= NV_TEX_FAIXA_BYTES);   // nunca mais de uma faixa
      if (!subiu) {
        assert(itens[0].estado == DECODIFICADO && itens[0].tex == 0);
        assert(itens[0].sup == s);                    // a superficie fica no item
        tex_novo_quadro();                            // e nao vira pedido velho
        itens[0].ultimoQuadro = quadroAtual;
      }
    }
    // 1 MiB strips hold 136 complete 1920-pixel rows: eight calls for 1080 rows.
    printf("heroi 1920x1080: %d chamadas de tex_bombear, <= %ld B por chamada\n",
           chamadas, NV_TEX_FAIXA_BYTES);
    assert(chamadas == 8);
    assert(itens[0].estado == PRONTO && itens[0].tex && itens[0].sup == NULL);
    assert(uplIdx == -1 && uplTex == 0);
    lido = malloc((size_t)1920 * 1080 * 4);
    glBindTexture(GL_TEXTURE_2D, itens[0].tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, lido);
    for (i = 0; i < 1080; i++)
      assert(!memcmp(lido + (size_t)i * 1920 * 4,
                     (unsigned char *)copia->pixels + (size_t)i * copia->pitch, 1920 * 4));
    free(lido); SDL_FreeSurface(copia);
    puts("ok  heroi sobe em faixas, so publica inteiro, pixels identicos"); }

  // 1b. Item que deixa de ser o pedido no meio do envio: a textura reservada
  // vai embora e nada e publicado.
  { SDL_Surface *s = superficie(1920, 1080, 9);
    decodificado(1, "https://x/outro.jpg", s, 1);
    assert(tex_bombear(3) == 0 && uplIdx == 1);
    snprintf(itens[1].caminho, sizeof itens[1].caminho, "%s", "https://x/trocado.jpg");
    itens[1].estado = PENDENTE;   // o slot virou outro pedido
    assert(tex_bombear(3) == 0);
    assert(uplIdx == -1 && uplTex == 0 && itens[1].tex == 0);
    SDL_FreeSurface(s); itens[1].sup = NULL; itens[1].estado = VAZIO;
    puts("ok  envio interrompido nao publica e nao vaza textura"); }

  // 2. Urgente primeiro (cartazes pequenos sobem inteiros).
  { SDL_Surface *a = superficie(200, 300, 1), *b = superficie(200, 300, 2);
    decodificado(2, "https://x/cartaz.jpg", a, 0);
    decodificado(3, "https://x/logo.png", b, 2);
    assert(tex_bombear(3) >= 1);
    assert(itens[3].estado == PRONTO);
    puts("ok  logo/heroi decodificado sobe antes do cartaz"); }

  // 3. Disco direto.
  { char dir[NVT_MAX], arq[700]; nvt_set(dir, sizeof dir, "/texb3-XXXXXX");
    FILE *f;
    int i;
    assert(mkdtemp(dir));
    snprintf(dirCache, sizeof dirCache, "%s", dir);
    cachearte_nativo_configurar_diretorio(dir);
    nomeDeCache("https://x/no-disco.jpg", arq, sizeof arq);
    f = fopen(arq, "wb"); assert(f); fputs("\xff\xd8 jpeg de mentira, mais de 512 bytes nao importa aqui", f); fclose(f);
    cachearte_nativo_indice_construir();
    assert(cachearte_nativo_indice_tem(arq));
    filaIni = filaFim = decIni = decFim = 0;
    assert(tex_obter("https://x/no-disco.jpg") == 0);
    assert(tex_obter("https://x/fora-do-disco.jpg") == 0);
    i = acharIndice("https://x/no-disco.jpg", hashCaminho("https://x/no-disco.jpg"));
    assert(i >= 0 && itens[i].naFilaDec);
    assert(tirarFila(filaDec, &decIni, decFim) == i);
    i = acharIndice("https://x/fora-do-disco.jpg", hashCaminho("https://x/fora-do-disco.jpg"));
    assert(i >= 0 && !itens[i].naFilaDec);
    assert(tirarFila(fila, &filaIni, filaFim) == i);
    assert(filaIni == filaFim);   // o do disco nao entrou na fila de rede
    printf("disco-direto=%d\n", tex_disco_direto);
    puts("ok  arte do disco vai direto ao decode; a que falta vai a rede");

  // 4. Memoria de arte inexistente.
    assert(!negTem("https://x/404.jpg"));
    negPor("https://x/404.jpg", NV_NEG_404_S, 1);
    negPor("https://x/prazo.jpg", NV_NEG_FALHA_S, 0);
    assert(negTem("https://x/404.jpg") && negTem("https://x/prazo.jpg"));
    assert(baixarImagem("https://x/404.jpg", &(long){0}, NULL) == NULL);
    memset(negArte, 0, sizeof negArte);
    negCarregar();                                   // reinicio do app
    assert(negTem("https://x/404.jpg"));             // 404 voltou do disco
    assert(!negTem("https://x/prazo.jpg"));          // prazo nao persiste
    for (i = 0; i < NV_NEG_N; i++) if (negArte[i].h) negArte[i].ate = (long)time(NULL) - 1;
    assert(!negTem("https://x/404.jpg"));            // vence
    puts("ok  404 lembrado e persistido; prazo estourado so na RAM; tudo vence"); }

  puts("texb3: tudo ok");
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w);
  return 0;
}
