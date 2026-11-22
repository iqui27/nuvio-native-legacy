// tests/timeshift_e2e.sh: timeshift local de ponta a ponta contra um HLS AO
// VIVO de verdade (playlist deslizante do ffmpeg): assistir, pausar (sem
// leitor), retomar atrasado, voltar ao vivo, sair do canal.
#include "../src/proxyts.c"
#include <assert.h>
int ajustes_livetv_resolucao(void) { return 0; }
static int falhas;
#define OK(c, m) do { if (c) printf("ok  %s\n", m); else { printf("FALHOU %s\n", m); falhas++; } } while (0)
static long baixar(const char *url, int segundos, const char *arq) {
  char cmd[700];
  struct stat st;
  snprintf(cmd, sizeof cmd, "curl -s --max-time %d -o '%s' '%s'", segundos, arq, url);
  if (system(cmd)) {}
  return stat(arq, &st) ? 0 : (long)st.st_size;
}
// Duracao (s) do video num arquivo TS, pelo ffprobe.
static double duracao(const char *arq) {
  char cmd[700], r[64] = "";
  FILE *f;
  snprintf(cmd, sizeof cmd, "ffprobe -v error -select_streams v:0 -show_entries stream=duration -of csv=p=0 '%s' 2>/dev/null", arq);
  f = popen(cmd, "r"); if (f) { if (!fgets(r, sizeof r, f)) r[0] = 0; pclose(f); }
  return atof(r);
}
static long noDisco(const char *dir, int *arqs) {
  DIR *p = opendir(dir); struct dirent *e; long t = 0; char c[1024]; struct stat st;
  *arqs = 0;
  while (p && (e = readdir(p)) != NULL) {
    if (strncmp(e->d_name, "ts-", 3)) continue;
    snprintf(c, sizeof c, "%s/%s", dir, e->d_name);
    if (!stat(c, &st)) { t += (long)st.st_size; (*arqs)++; }
  }
  if (p) closedir(p);
  return t;
}
int main(int argc, char **argv) {
  const char *fonte = argv[1], *dir = argv[2], *tmp = argv[3];
  char url[200], tsu[200], a1[400], a2[400], a3[400];
  long n1, n2, n3, d0, d1;
  int arqs, jan0, jan1, k;
  double s2, s3;
  assert(argc == 4);
  snprintf(a1, sizeof a1, "%s/vivo.ts", tmp); snprintf(a2, sizeof a2, "%s/atras.ts", tmp); snprintf(a3, sizeof a3, "%s/vivo2.ts", tmp);

  proxyts_definir_timeshift(dir);
  assert(proxyts_url(fonte, url, sizeof url));
  // 1. assistir ao vivo 12 s
  n1 = baixar(url, 12, a1);
  jan0 = proxyts_timeshift_janela_s(); d0 = noDisco(dir, &arqs);
  printf("    ao vivo 12 s: %ld KB entregues; gravado %ld KB em %d arquivo(s); janela %d s\n", n1 / 1024, d0 / 1024, arqs, jan0);
  OK(n1 > 200000 && d0 > 200000 && jan0 >= 4, "assistindo: o canal toca e a gravacao cresce");
  // 2. pausa de 15 s: o player para de ler; a ingestao segue porque ele segurou
  proxyts_timeshift_segurar(1800);
  for (k = 0; k < 15; k++) sleep(1);
  jan1 = proxyts_timeshift_janela_s(); d1 = noDisco(dir, &arqs);
  printf("    pausado 15 s sem leitor: gravado %ld KB em %d arquivo(s); janela %d s\n", d1 / 1024, arqs, jan1);
  OK(d1 > d0 + 200000 && jan1 >= jan0 + 12, "pausado: a gravacao continuou sem leitor");
  // 3. retomar 15 s atras: em 5 s de relogio chega o atraso inteiro + o que seguiu
  assert(proxyts_timeshift_url(15, tsu, sizeof tsu));
  proxyts_timeshift_segurar(20);
  n2 = baixar(tsu, 5, a2); s2 = duracao(a2);
  printf("    retomada 15 s atras: %ld KB em 5 s de relogio = %.1f s de video\n", n2 / 1024, s2);
  OK(s2 >= 14.0 && s2 <= 32.0, "retomada: o video recomeca ~15 s atras (15 a 20 s + 5 s em 5 s de relogio)");
  // 4. voltar ao vivo: a mesma sessao sem atras entrega so o presente
  assert(proxyts_timeshift_url(0, tsu, sizeof tsu) && strstr(tsu, "&atras=0"));
  n3 = baixar(tsu, 5, a3); s3 = duracao(a3);
  printf("    voltar ao vivo: %ld KB em 5 s de relogio = %.1f s de video\n", n3 / 1024, s3);
  OK(n3 > 100000 && s3 >= 2.0 && s3 <= 10.0, "ao vivo: so o presente (do segmento mais novo)");
  OK(proxyts_timeshift_janela_s() >= jan1 + 8, "a janela seguiu crescendo durante tudo");
  // 5. sem segurar, 3 s sem leitor encerram a ingestao (como antes do timeshift)
  proxyts_timeshift_segurar(0);
  for (k = 0; k < 8; k++) sleep(1);
  d0 = noDisco(dir, &arqs); sleep(3); d1 = noDisco(dir, &arqs);
  OK(d1 == d0, "sem pausa pedida: sem leitor a ingestao para em segundos");
  // 6. sair do canal apaga tudo
  proxyts_timeshift_encerrar();
  for (k = 0; k < 30 && (noDisco(dir, &arqs), arqs); k++) usleep(200 * 1000);
  OK(!arqs, "sair do canal apaga a gravacao");
  printf(falhas ? "\n%d FALHA(S)\n" : "\ntimeshift_e2e: tudo certo\n", falhas);
  return falhas ? 1 : 0;
}
