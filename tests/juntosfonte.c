// Watch Together: impressao do release e casamento com a lista do convidado.
//   bash tests/juntosfonte.sh
#include "juntosfonte.h"
#include <stdio.h>
#include <string.h>

static int falhas;
#define OK(c, ...) do { if (!(c)) { falhas++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static Stream st(const char *hash, int idx, unsigned long long bytes, const char *arq, const char *binge, int alt) {
  Stream s;
  memset(&s, 0, sizeof s);
  snprintf(s.infoHash, sizeof s.infoHash, "%s", hash);
  s.fileIdx = idx;
  s.tamanhoBytes = bytes;
  snprintf(s.arquivo, sizeof s.arquivo, "%s", arq);
  snprintf(s.bingeGroup, sizeof s.bingeGroup, "%s", binge);
  s.altura = alt;
  snprintf(s.url, sizeof s.url, "https://real-debrid.com/d/SEGREDO/%s", arq);
  return s;
}

int main(void) {
  const char *H = "ABCDEF0123456789ABCDEF0123456789ABCDEF01";
  const char *h2 = "1111111111111111111111111111111111111111";
  Stream anf = st(H, 3, 2100000000ULL, "Show/Season 1/Show.S01E04.1080p.WEB-DL.DDP5.1.mkv", "torrentio|1080p", 1080);
  JsImpressao i, rt;
  Stream l[6];
  int k, pal;
  unsigned char ign[6] = { 0 };
  char b[600];

  jsf_impressao(&anf, 2700.5, &i);
  OK(!strcmp(i.hash, "abcdef0123456789abcdef0123456789abcdef01") && i.idx == 3, "hash minusculo + idx");
  OK(!strcmp(i.arquivo, "Show.S01E04.1080p.WEB-DL.DDP5.1.mkv"), "so o nome do arquivo: %s", i.arquivo);
  OK(i.bytes == 2100000000LL && i.altura == 1080 && i.durMs == 2700500, "bytes/altura/dur");
  OK(!strcmp(i.rotulo, "1080p WEB-DL"), "rotulo: %s", i.rotulo);
  // A impressao, montada e lida de volta, nunca leva a URL.
  jsp_impressao(b, sizeof b, &i);
  OK(!strstr(b, "debrid") && !strstr(b, "SEGREDO") && !strstr(b, "http"), "sem URL: %s", b);
  { char c[700]; snprintf(c, sizeof c, "{\"imp\":%s}", b); jsp_ler_impressao(c, NULL, &rt); }
  OK(!strcmp(rt.hash, i.hash) && rt.idx == 3 && rt.bytes == i.bytes && !strcmp(rt.arquivo, i.arquivo), "ida e volta: %s / [%s] %d %lld [%s]", b, rt.hash, rt.idx, rt.bytes, rt.arquivo);
  { Stream u = st("nao-hash", -1, 0, "https://x.test/a.mkv", "a://b", 720); JsImpressao j; jsf_impressao(&u, 0, &j);
    OK(!j.hash[0] && j.idx == -1 && !j.arquivo[0] && !j.binge[0], "lixo e URL ficam de fora"); }

  // 1. mesmo hash + fileIdx, mesmo em outra posicao e com outro debrid
  l[0] = st(h2, 0, 999, "Outro.mkv", "torrentio|1080p", 1080);
  l[1] = st("abcdef0123456789abcdef0123456789abcdef01", 2, 0, "", "", 1080);   // outro arquivo do pacote
  l[2] = st(H, 3, 0, "", "comet|x", 720);
  OK(jsf_casar(&i, l, 3, NULL, &k) == JSF_MESMA && k == 2, "hash+idx (%d)", k);
  ign[2] = 1;
  OK(jsf_casar(&i, l, 3, ign, &k) == JSF_PARECIDA && k == 0, "ignorando a que falhou: reserva na mesma altura (%d)", k);
  // fileIdx desconhecido de um lado ainda casa
  l[1].fileIdx = -1;
  OK(jsf_casar(&i, l, 2, NULL, &k) == JSF_MESMA && k == 1, "hash sem idx");
  // 2. tamanho exato + nome (addon HTTP sem hash)
  l[0] = st("", -1, 2100000000ULL, "show s01e04 1080p web-dl ddp5 1.mkv", "", 1080);
  l[1] = st("", -1, 2100000001ULL, "Show.S01E04.1080p.WEB-DL.DDP5.1.mkv", "", 1080);
  OK(jsf_casar(&i, l, 2, NULL, &k) == JSF_MESMA && k == 0, "bytes+nome (%d)", k);
  l[0].arquivo[0] = 0;
  OK(jsf_casar(&i, l, 1, NULL, &k) == JSF_MESMA, "bytes exatos > 50 MB sem nome");
  // 3. mesmo bingeGroup + altura + nome parecido
  l[0] = st("", -1, 0, "Show.S01E04.1080p.WEB.H264-GRUPO.mkv", "torrentio|1080p", 1080);
  l[1] = st("", -1, 0, "Show.S01E04.1080p.WEB-DL.DDP5.1.H.264-OUTRO.mkv", "torrentio|1080p", 1080);
  l[2] = st("", -1, 0, "Show.S01E04.2160p.mkv", "torrentio|2160p", 2160);
  OK(jsf_casar(&i, l, 3, NULL, &k) == JSF_PARECIDA && k == 1, "binge + nome mais parecido (%d)", k);
  // 4. reserva por altura
  l[0] = st("", -1, 0, "a.mkv", "", 2160);
  l[1] = st("", -1, 0, "b.mkv", "", 720);
  l[2] = st("", -1, 0, "c.mkv", "", 1080);
  OK(jsf_casar(&i, l, 3, NULL, &k) == JSF_PARECIDA && k == 2, "mesma altura (%d)", k);
  OK(jsf_casar(&i, l, 2, NULL, &k) == JSF_PARECIDA && k == 1, "a mais perto (%d)", k);
  OK(jsf_casar(&i, l, 0, NULL, &k) == JSF_NENHUMA && k == -1, "lista vazia");

  // Duracao depois do load.
  OK(jsf_duracao(JSF_MESMA, 2700500, 2701.9, &pal) == JSF_MESMA, "1,4 s: ainda a mesma");
  OK(jsf_duracao(JSF_PARECIDA, 2700500, 2600.0, &pal) == JSF_DIFERENTE && !pal, "100 s: diferente");
  OK(jsf_duracao(JSF_PARECIDA, 2700500, 120.0, &pal) == JSF_NENHUMA, "clipe de aviso de 120 s: nenhuma");
  OK(jsf_duracao(JSF_PARECIDA, 2700500, 2700.5 / (25.0 / 23.976), &pal) == JSF_DIFERENTE && pal, "PAL marcado");
  OK(jsf_duracao(JSF_PARECIDA, 0, 2600.0, &pal) == JSF_PARECIDA, "sala sem duracao: nao decide");
  OK(!strcmp(jsf_nome(JSF_DIFERENTE), "diferente"), "nome");
  if (falhas) { printf("juntosfonte: %d falha(s)\n", falhas); return 1; }
  printf("juntosfonte: impressao sem URL, hash/bytes/binge/altura e duracao ok\n");
  return 0;
}
