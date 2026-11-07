// CLI sobre o codigo REAL do cliente (openseekproto.c + osp1.c) para o teste de duas identidades do
// backend (OpenSeek 2.0, backend/tests/test_nuvio_client_protocol.py): o Python faz so o transporte HTTP,
// cada decisao de protocolo (ler convite, corpos, derivar K, provas, ler a procura) e deste codigo.
//   entrada <texto>                       -> forma / base / pino / convite / segredo
//   start-body <convite>                  -> corpo do /v1/pair/start
//   claim <segredo> <start.json> <nome>   -> corpo do /v1/pair/claim
//   finish <segredo> <start.json> <claim.json> <nome> -> "ok role=.. device=.. token=.." ou "bad"
//   lookup-body <imdb> <t> <e> <durMs> <tamanho>
//   parse-lookup <base> <json> -> id / vtt / tipo / aprox / verif / a / b / rotulo
//   offset-body <ms>       offset-after <b> <deslocaMs>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "openseekproto.h"
#include "osp1.h"

static char *ler(const char *caminho) {
  FILE *f = fopen(caminho, "rb");
  char *b;
  long n;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1);
  if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
  if (b) b[n] = 0;
  fclose(f);
  return b;
}

int main(int argc, char **argv) {
  char out[4096];
  const char *c = argc > 1 ? argv[1] : "";
  if (!strcmp(c, "entrada") && argc == 3) {
    OpenSeekEntrada e;
    if (!openseek_ler_entrada(argv[2], &e)) { puts("forma=0"); return 0; }
    printf("forma=%d\nbase=%s\npino=%s\nconvite=%s\nsegredo=%s\n", e.forma, e.base, e.pino, e.convite, e.segredo);
    return 0;
  }
  if (!strcmp(c, "start-body") && argc == 3) {
    if (!openseek_json_start(out, sizeof out, argv[2])) return 1;
    puts(out);
    return 0;
  }
  if ((!strcmp(c, "claim") && argc == 5) || (!strcmp(c, "finish") && argc == 6)) {
    char *sj = ler(argv[3]), *cj = !strcmp(c, "finish") ? ler(argv[4]) : NULL;
    const char *nome = argv[argc - 1];
    OpenSeekStart s;
    unsigned char k[32];
    char prova[65];
    if (!sj || !openseek_ler_start(sj, &s)) { puts("bad-start"); return 1; }
    if (!osp1_derivar(argv[2], s.pairId, s.salt, s.pino, s.iter, OSP1_ITER_MIN, k, NULL, NULL)) { puts("bad-kdf"); return 1; }
    if (!strcmp(c, "claim")) {
      osp1_prova_cliente(k, s.pairId, nome, prova);
      if (!openseek_json_claim(out, sizeof out, s.pairId, nome, prova)) return 1;
      puts(out);
      return 0;
    } else {
      OpenSeekClaim cl;
      char esperado[65];
      if (!cj || !openseek_ler_claim(cj, &cl)) { puts("bad-claim"); return 1; }
      osp1_prova_servidor(k, s.pairId, cl.token, esperado);
      if (!osp1_iguais(esperado, cl.prova)) { puts("bad-proof"); return 1; }
      printf("ok role=%s device=%s token=%s\n", openseek_papel_de(cl.papel) == OS_PAPEL_CONVIDADO ? "guest" : "owner",
             cl.deviceId, cl.token);
      return 0;
    }
  }
  if (!strcmp(c, "lookup-body") && argc == 7) {
    if (!openseek_json_procura(out, sizeof out, argv[2], atoi(argv[3]), atoi(argv[4]), atol(argv[5]),
                               strtoull(argv[6], NULL, 10))) { puts("refused"); return 0; }
    puts(out);
    return 0;
  }
  if (!strcmp(c, "parse-lookup") && argc == 4) {
    OpenSeekCompart m;
    if (!openseek_ler_procura(argv[3], argv[2], &m)) { puts("refused"); return 0; }
    printf("id=%s\nvtt=%s\ntipo=%s\naprox=%d\nverif=%d\na=%.6f\nb=%.1f\nrotulo=%s\n", m.id, m.vtt, m.tipo, m.aproximado,
           m.verificado, m.a, m.b, openseek_rotulo_compart(&m));
    return 0;
  }
  if (!strcmp(c, "offset-body") && argc == 3) {
    if (!openseek_json_offset(out, sizeof out, atol(argv[2]))) return 1;
    puts(out);
    return 0;
  }
  if (!strcmp(c, "offset-after") && argc == 4) {
    printf("%ld\n", openseek_offset_apos(atof(argv[2]), atol(argv[3])));
    return 0;
  }
  if (!strcmp(c, "contribute-body") && argc == 8) {      // url cabecalhos imdb t e durMs
    OpenSeekFonte f = { argv[2], argv[3], atol(argv[7]), argv[4], atoi(argv[5]), atoi(argv[6]), 0, "", "", 0, -1 };
    if (!openseek_json_contribuir(out, sizeof out, &f)) { puts("refused"); return 0; }
    puts(out);
    return 0;
  }
  if (!strcmp(c, "can-contribute") && argc == 3) {
    puts(openseek_ler_pode_contribuir(argv[2]) ? "yes" : "no");
    return 0;
  }
  fputs("uso invalido\n", stderr);
  return 2;
}
