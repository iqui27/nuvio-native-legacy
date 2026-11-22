// tests/timeshift_disco.sh: o anel em disco do timeshift (src/timeshift_disco.h)
// num diretorio de verdade, com a medida de escrita e leitura.
#include "../src/timeshift_disco.h"
#include <time.h>
static int falhas;
#define OK(c, m) do { if (c) printf("ok  %s\n", m); else { printf("FALHOU %s\n", m); falhas++; } } while (0)
#define MB (1ull << 20)
#define BLOCO (188u * 256u)
static double agora(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
// Pacote k: sync 0x47 e o numero do pacote, para conferir o que volta do disco.
static void encher(unsigned char *b, uint64_t pos, size_t n) {
  size_t i;
  for (i = 0; i < n; i += 188) {
    uint64_t k = (pos + i) / 188;
    memset(b + i, (int)(k & 0xff), 188);
    b[i] = 0x47; memcpy(b + i + 4, &k, sizeof k);
  }
}
static int conferir(const unsigned char *b, uint64_t pos, long n) {
  long i;
  for (i = 0; i + 188 <= n; i += 188) {
    uint64_t k; memcpy(&k, b + i + 4, sizeof k);
    if (b[i] != 0x47 || k != (pos + (uint64_t)i) / 188) return 0;
  }
  return 1;
}
static int arquivos(const char *dir) {
  DIR *p = opendir(dir); struct dirent *e; int n = 0;
  while (p && (e = readdir(p)) != NULL) if (!strncmp(e->d_name, "ts-", 3)) n++;
  if (p) closedir(p);
  return n;
}
int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "/tmp/nuvio-timeshift-disco";
  unsigned char *b = malloc(BLOCO), *r = malloc(BLOCO);
  TsDisco *d;
  TsLeitor l = { -1, 0 };
  uint64_t pos = 0, lido;
  int64_t t = 0;
  int pulou, certo = 1;
  long n;
  double t0, dt;

  OK(tsd_abrir("/proc/nao-existe/x", 0) == NULL, "diretorio que nao da para criar: sem timeshift");
  { char sobra[800]; FILE *f; mkdir(dir, 0755); snprintf(sobra, sizeof sobra, "%s/ts-00000099.ts", dir);
    f = fopen(sobra, "w"); if (f) { fputs("x", f); fclose(f); } }
  d = tsd_abrir(dir, 64 * MB);
  OK(d && arquivos(dir) == 0, "abre e limpa a sobra da sessao anterior");
  if (!d) return 1;

  // 200 MB a 1 MB por "segundo" de relogio falso, marco a cada 2 s: teto de 64 MB.
  t0 = agora();
  while (pos < 200 * MB) {
    int marco = pos % (2 * MB) < BLOCO;
    encher(b, pos, BLOCO);
    if (!tsd_gravar(d, b, BLOCO, marco, t)) { certo = 0; break; }
    pos += BLOCO; t = (int64_t)(pos * 1000 / MB);
  }
  dt = agora() - t0;
  OK(certo && d->ix.fim == pos, "200 MB gravados");
  printf("    medida: escrita %.0f MB/s (%s)\n", 200.0 / dt, dir);
  OK(tsr_usado(&d->ix) <= 64 * MB && tsr_usado(&d->ix) >= 50 * MB, "disco usado fica no teto de 64 MB");
  OK(arquivos(dir) == (int)d->ix.n && d->ix.n >= 8 && d->ix.n <= 11, "segmentos de ~6 MB; os velhos foram apagados");
  { int64_t j = tsd_janela_ms(d, t); printf("    janela: %lld s em 64 MB a 8 Mbps\n", (long long)(j / 1000));
    OK(j >= 50000 && j <= 64000, "janela de ~1 min"); }

  // ler 30 s atras ate a ponta: comeca em inicio de segmento e bate byte a byte
  lido = tsd_pos_atras(d, 30000, t);
  OK(lido % (2 * MB) < BLOCO && pos - lido >= 30 * MB && pos - lido < 37 * MB, "30 s atras: inicio do segmento que cobre");
  t0 = agora(); certo = 1;
  { uint64_t ini = lido;
    while ((n = tsd_ler(d, &l, &lido, r, BLOCO, &pulou)) > 0) if (!conferir(r, lido - (uint64_t)n, n) || pulou) certo = 0;
    dt = agora() - t0;
    OK(certo && n == 0 && lido == pos, "leitura ate a ponta confere com o gravado");
    printf("    medida: leitura %.0f MB/s\n", (double)(pos - ini) / MB / (dt > 0 ? dt : 1e-9)); }

  // posicao que ja foi apagada pula para o mais antigo
  lido = 0;
  n = tsd_ler(d, &l, &lido, r, BLOCO, &pulou);
  OK(n > 0 && pulou && conferir(r, lido - (uint64_t)n, n) && lido - (uint64_t)n == tsr_inicio(&d->ix), "posicao apagada: pula e segue certo");

  // leitor atrasado enquanto a gravacao expurga o arquivo que ele tem aberto
  { uint64_t p = tsr_inicio(&d->ix); int k, bom = 1;
    for (k = 0; k < 2000 && bom; k++) {
      int marco = pos % (2 * MB) < BLOCO;
      encher(b, pos, BLOCO); tsd_gravar(d, b, BLOCO, marco, t); pos += BLOCO; t = (int64_t)(pos * 1000 / MB);
      n = tsd_ler(d, &l, &p, r, 188 * 16, &pulou);
      if (n <= 0 || !conferir(r, p - (uint64_t)n, n)) bom = 0;
    }
    OK(bom, "leitor lento sobrevive ao expurgo (pula, nunca le lixo)"); }
  tsd_leitor_fechar(&l);

  // falha de escrita desliga so a gravacao
  close(d->fd); d->fd = open("/dev/full", O_WRONLY);
  if (d->fd >= 0) {
    OK(!tsd_gravar(d, b, BLOCO, 0, t) && d->erro == ENOSPC && tsd_janela_ms(d, t) == 0, "disco cheio: gravacao desligada, janela 0");
    OK(!tsd_gravar(d, b, BLOCO, 0, t), "e continua desligada");
  }
  tsd_fechar(d);
  OK(arquivos(dir) == 0, "fechar apaga os segmentos");
  rmdir(dir);
  printf(falhas ? "\n%d FALHA(S)\n" : "\ntimeshift_disco: tudo certo\n", falhas);
  return falhas ? 1 : 0;
}
