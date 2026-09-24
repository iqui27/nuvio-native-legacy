// QUANTAS VEZES O TIZEN BAIXA UM GIF (tests/texgif-tizen.sh).
//
// POR QUE ESTE TESTE EXISTE. No Tizen so GIF vai a arquivo (gif.c le por
// caminho). O fio de rede baixava o corpo para a memoria, descobria que era
// GIF, jogava os bytes fora e chamava garantirLocal — que baixava tudo de novo
// quando o arquivo nao existia, e quando existia o download da memoria ja
// tinha sido perdido. Nos registros D1 da TV do rawldon o avatar GIF de 1 MB
// saiu da rede 2 a 4 vezes por sessao (`image_requests=2 net_ms=3169` no
// primeiro pedido da 2565). Aqui o XMLHttpRequest e falso e conta os pedidos
// (tests/texgif-shim.js); o que se mede e baixarParaItem, o mesmo que o fio
// de rede chama.
//
// RESULTADO esperado depois da correcao:
//   gif_primeiro=1 gif_de_novo=0 gif_depois_de_reabrir=0 jpg=1 jpg_em_arquivo=0
// Antes (v1.4.5): gif_primeiro=2 gif_de_novo=1 gif_depois_de_reabrir=1.
#include "../src/tex_cache.c"
#include <assert.h>

EM_JS(int, xhr_n, (), { return globalThis.nvXhrN | 0; });

#define URL_GIF "https://media4.giphy.com/media/teste/giphy.gif"
#define URL_JPG "https://image.tmdb.org/t/p/w342/teste.jpg"

static int pedir(const char *url) {
  char dst[600];
  int foiRede = 0, antes = xhr_n(), r;
  TexFetchTrace tr = {0, 0, 0, 0, 0};
  memset(&itens[0], 0, sizeof itens[0]);
  snprintf(itens[0].caminho, sizeof itens[0].caminho, "%s", url);
  itens[0].estado = PENDENTE;
  itens[0].limite = 352;
  r = baixarParaItem(0, url, dst, sizeof dst, &foiRede, &tr);
  assert(r == 1);
  soltarBruto(&itens[0]);
  return xhr_n() - antes;
}

int main(void) {
  int g1, g2, g3, j1, jArq = 0;
  mtx = SDL_CreateMutex();
  tex_cache_dir("/cache");
  g1 = pedir(URL_GIF);
  g2 = pedir(URL_GIF);
#ifdef NV_TESTE_REABRIR
  // "Reabrir o app": o registro em memoria some e a varredura da pasta, a
  // mesma do arranque, o refaz. O arquivo continua no /cache (IDBFS na TV).
  nArqDisco = 0;
  tex_cache_dir("/cache");
#endif
  g3 = pedir(URL_GIF);
  j1 = pedir(URL_JPG);
  { char arq[600]; struct stat st;
    nomeDeCache(URL_JPG, arq, sizeof arq);
    jArq = stat(arq, &st) == 0; }
  printf("RESULTADO gif_primeiro=%d gif_de_novo=%d gif_depois_de_reabrir=%d jpg=%d jpg_em_arquivo=%d\n",
         g1, g2, g3, j1, jArq);
  fflush(stdout);
  return 0;
}
