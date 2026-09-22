// Legenda ASS embutida colhida por Range (#92, fase 3): o modulo mkvass.c
// contra um MKV de verdade, gerado pelo ffmpeg no tests/mkvass.sh e servido
// por tests/servidor_range.py. O que se prova aqui, em numeros:
//   - todo Dialogue: do .ass original chega ao overlay, com tempo igual (±20 ms);
//   - os bytes baixados sao uma fracao minima do arquivo (< 2 %);
//   - o teto de Ranges por segundo e respeitado;
//   - a segunda abertura da mesma URL sai do sidecar com ZERO pedidos;
//   - um sidecar parcial continua de onde parou;
//   - os no-go (faixa que nao e ASS, servidor sem Range, arquivo que nao e
//     MKV) sao declarados sem tentar colher.
//
//   tests/mkvass <base-url> <nome.mkv> <caminho.ass> <nome_srt.mkv> <nome.ass>
#include "../src/mkvass.h"
#include "../src/legenda.h"
#include "../src/rede.h"
#include "../src/dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

static int falhas;
static void ok(int cond, const char *o) {
  printf("  %-58s %s\n", o, cond ? "ok" : "FALHOU");
  if (!cond) falhas++;
}

static long agoraMs(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static char base[512];

static long contagemServidor(void) {
  char url[600]; char *r; long v;
  snprintf(url, sizeof url, "%s/contagem", base);
  r = rede_baixar(url, 5);
  v = r ? atol(r) : -1;
  free(r);
  return v;
}
static void zerarServidor(void) {
  char url[600]; char *r;
  snprintf(url, sizeof url, "%s/zerar", base);
  r = rede_baixar(url, 5); free(r);
}

static char *lerArquivo(const char *cam, long *tam) {
  FILE *f = fopen(cam, "rb"); long n; char *b;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1);
  if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
  b[n] = 0; fclose(f);
  if (tam) *tam = n;
  return b;
}

// O mesmo FNV-1a de mkvass.c: o nome do sidecar e um contrato deste teste.
static void nomeSidecar(const char *url, int faixa, char *dst, size_t tam) {
  unsigned long long h = 1469598103934665603ULL;
  const unsigned char *p = (const unsigned char *)url;
  while (*p) { h ^= *p++; h *= 1099511628211ULL; }
  snprintf(dst, tam, "mkvass-%016llx-%d.ass", h, faixa);
}

// Espera o estado ficar terminal (COMPLETO ou no-go) simulando o player: a
// posicao anda `fator` vezes o tempo real e mkvass_passo e chamado a cada
// "quadro". Devolve o maior numero de Ranges observado num mesmo segundo.
static int rodarAte(double fator, long timeoutMs, double posIni) {
  long t0 = agoraMs(), ultSeg = -1, ultPed = 0; int maxSeg = 0, noSeg = 0;
  for (;;) {
    long ag = agoraMs(), ped; int e = mkvass_estado();
    double pos = posIni + (ag - t0) / 1000.0 * fator;
    mkvass_passo(pos);
    mkvass_estatisticas(&ped, NULL, NULL, NULL);
    if (ag / 1000 != ultSeg) { ultSeg = ag / 1000; noSeg = 0; }
    noSeg += (int)(ped - ultPed); ultPed = ped;
    if (noSeg > maxSeg) maxSeg = noSeg;
    if (e == MKVASS_COMPLETO || e >= MKVASS_NOGO) return maxSeg;
    if (ag - t0 > timeoutMs) return -1;
    usleep(20 * 1000);
  }
}

static void esperarFio(void) {
  long t0 = agoraMs();
  while (mkvass_ocupado() && agoraMs() - t0 < 30000) usleep(10 * 1000);
}

// Confere que cada bloco esperado esta vivo no overlay no meio do seu tempo,
// com o mesmo texto e tempos iguais a ±20 ms. Devolve quantos casaram.
static int conferirCues(const LegendaCue *esp, int n) {
  int i, casou = 0;
  for (i = 0; i < n; i++) {
    LegendaCue v[LEGENDA_SIMULTANEAS]; int k, m;
    m = legenda_cues((esp[i].inicio + esp[i].fim) / 2.0, 0, v, LEGENDA_SIMULTANEAS);
    for (k = 0; k < m; k++)
      if (!strcmp(v[k].texto, esp[i].texto) &&
          fabs(v[k].inicio - esp[i].inicio) <= 0.020 &&
          fabs(v[k].fim - esp[i].fim) <= 0.020 &&
          v[k].an == esp[i].an && fabs(v[k].posX - esp[i].posX) < 0.5) { casou++; break; }
    if (k == m && i < 3)
      printf("    faltou: \"%s\" %.3f-%.3f (achou %d no instante)\n", esp[i].texto, esp[i].inicio, esp[i].fim, m);
  }
  return casou;
}

static int contarDialogue(const char *s) {
  int n = 0; const char *p = s;
  while ((p = strstr(p, "\nDialogue:"))) { n++; p++; }
  return n;
}

int main(int argc, char **argv) {
  char url[600], urlSrt[600], urlNoRange[600], urlAss[600], urlLento[600], urlCurto[600];
  char sidecar[64], sidecarLento[64], sidecarCurto[64], cam[600];
  LegendaCue *esp = NULL; int nEsp; long tamMkv = 0, tamAss = 0;
  long ped, bytes, ped1; int colhidos, total, maxSeg, r;
  char *corpo;

  if (argc < 6) { fprintf(stderr, "uso: %s base mkv ass srtmkv assnome\n", argv[0]); return 2; }
  snprintf(base, sizeof base, "%s", argv[1]);
  snprintf(url, sizeof url, "%s/%s", base, argv[2]);
  snprintf(urlSrt, sizeof urlSrt, "%s/%s", base, argv[4]);
  snprintf(urlNoRange, sizeof urlNoRange, "%s/norange/%s", base, argv[2]);
  snprintf(urlAss, sizeof urlAss, "%s/%s", base, argv[5]);
  snprintf(urlLento, sizeof urlLento, "%s/lento/%s", base, argv[2]);
  snprintf(urlCurto, sizeof urlCurto, "%s/curto/%s", base, argv[2]);
  rede_preparar();
  dados_iniciar(".");
  ok(dados_dir()[0] != 0, "dados_dir() (NUVIO_DADOS do .sh)");

  { struct stat st; ok(stat(argv[3], &st) == 0, "o .ass de referencia existe");
    corpo = lerArquivo(argv[3], &tamAss);
    nEsp = corpo ? legenda_extrair_ass(corpo, &esp) : 0;
    free(corpo);
    ok(nEsp >= 30, "referencia: >= 30 Dialogue no .ass"); }
  { char *r0 = rede_baixar_trecho(url, 5, 0, 3, &tamMkv);
    ok(r0 && tamMkv == 4, "servidor responde Range (4 bytes)"); free(r0);
    // Tamanho do arquivo: por HEAD nao ha ajuda em rede.h; le do disco.
    snprintf(cam, sizeof cam, "%s/%s", getenv("MKV_DIR") ? getenv("MKV_DIR") : ".", argv[2]);
    { struct stat st; tamMkv = stat(cam, &st) == 0 ? (long)st.st_size : 0; }
    ok(tamMkv > 1000000, "MKV gerado tem mais de 1 MB"); }
  nomeSidecar(url, 3, sidecar, sizeof sidecar);
  dados_apagar(sidecar);

  printf("\n[1] primeira abertura: colhe pela rede\n");
  zerarServidor();
  mkvass_iniciar(url, 3);
  maxSeg = rodarAte(4.0, 90000, 0.0);
  ok(mkvass_estado() == MKVASS_COMPLETO, "estado COMPLETO");
  mkvass_estatisticas(&ped, &bytes, &colhidos, &total);
  printf("    %d/%d blocos, %ld Ranges, %ld bytes de %ld (%.2f %%), pico %d/s\n",
         colhidos, total, ped, bytes, tamMkv, 100.0 * bytes / tamMkv, maxSeg);
  ok(total == nEsp, "CuePoints da faixa == Dialogue do .ass");
  ok(colhidos == nEsp, "blocos colhidos == Dialogue do .ass");
  ok(bytes * 100 < tamMkv * 2, "bytes lidos < 2 % do arquivo");
  ok(ped == contagemServidor(), "pedidos contados == GETs no servidor");
  ok(maxSeg >= 0 && maxSeg <= 8, "teto: no maximo 8 Ranges num mesmo segundo");
  r = conferirCues(esp, nEsp);
  printf("    %d/%d cues casaram (texto, ±20 ms, \\an, \\pos)\n", r, nEsp);
  ok(r == nEsp, "todos os cues batem com o .ass original");
  { int i, an8 = 0, pos = 0;
    for (i = 0; i < nEsp; i++) { if (esp[i].an == 8) an8++; if (esp[i].posX >= 0) pos++; }
    ok(an8 > 0 && pos > 0, "referencia cobre \\an8 e \\pos"); }
  esperarFio();
  corpo = dados_ler(sidecar);
  ok(corpo && !strncmp(corpo, "; mkvass-estado: completo", 25), "sidecar gravado como completo");
  ok(corpo && contarDialogue(corpo) == nEsp, "sidecar tem todos os Dialogue");
  { LegendaCue *v = NULL; int n = corpo ? legenda_extrair_ass(corpo, &v) : 0;
    ok(n == nEsp, "sidecar parseia com o mesmo numero de cues"); free(v); }
  free(corpo);
  ped1 = ped;

  printf("\n[2] segunda abertura: sidecar, zero rede\n");
  mkvass_parar(); esperarFio(); legenda_desligar();
  zerarServidor();
  mkvass_iniciar(url, 3);
  rodarAte(4.0, 10000, 0.0);
  ok(mkvass_estado() == MKVASS_COMPLETO, "estado COMPLETO");
  mkvass_estatisticas(&ped, &bytes, NULL, NULL);
  ok(ped == 0 && bytes == 0, "0 Ranges e 0 bytes");
  ok(contagemServidor() == 0, "servidor nao recebeu nenhum GET");
  r = conferirCues(esp, nEsp);
  ok(r == nEsp, "todos os cues batem (vindos do sidecar)");

  printf("\n[3] seek: janela muda de perto do fim para o inicio\n");
  mkvass_parar(); esperarFio(); legenda_desligar();
  dados_apagar(sidecar);
  zerarServidor();
  mkvass_iniciar(url, 3);
  { long t0 = agoraMs(); int c = 0; LegendaCue v[LEGENDA_SIMULTANEAS];
    // 112 s deixa so os ultimos blocos dentro da janela [104,202]. O passo
    // seguinte volta ao inicio e precisa buscar os que ficaram para tras,
    // sem duplicar os que ja entraram no overlay.
    mkvass_passo(112.0);
    while (agoraMs() - t0 < 30000) {
      mkvass_passo(112.0); mkvass_estatisticas(NULL, NULL, &c, NULL);
      if (c > 0 && c < nEsp) break;
      usleep(20 * 1000);
    }
    ok(c > 0 && c < nEsp, "seek para 112 s colheu so a janela do fim");
    ok(legenda_cues(112.0, 0, v, LEGENDA_SIMULTANEAS) > 0,
       "seek para 112 s entregou os cues proximos");
    mkvass_passo(0.0);
    rodarAte(4.0, 90000, 0.0);
    ok(mkvass_estado() == MKVASS_COMPLETO, "volta ao inicio termina COMPLETO");
    ok(conferirCues(esp, nEsp) == nEsp, "seek ida e volta preserva todos os cues"); }
  mkvass_parar(); esperarFio(); legenda_desligar(); dados_apagar(sidecar);

  printf("\n[4] sidecar parcial: sai no meio e continua depois\n");
  mkvass_parar(); esperarFio(); legenda_desligar();
  dados_apagar(sidecar);
  zerarServidor();
  mkvass_iniciar(url, 3);
  { long t0 = agoraMs(); int c = 0;
    // Playhead parado em 0: a janela de 90 s cobre so parte do arquivo.
    while (agoraMs() - t0 < 30000) {
      mkvass_passo(0.0); mkvass_estatisticas(NULL, NULL, &c, NULL);
      if (c >= 10 || mkvass_estado() >= MKVASS_NOGO) break;
      usleep(20 * 1000);
    }
    ok(c >= 10 && mkvass_estado() == MKVASS_COLHENDO, "colheu >= 10 blocos e ainda esta colhendo"); }
  mkvass_parar(); esperarFio();
  mkvass_estatisticas(&ped, NULL, &colhidos, NULL);
  corpo = dados_ler(sidecar);
  ok(corpo && !strncmp(corpo, "; mkvass-estado: parcial ", 25), "sidecar gravado como parcial");
  ok(corpo && contarDialogue(corpo) == colhidos, "sidecar parcial tem os blocos colhidos");
  free(corpo);
  legenda_desligar();
  zerarServidor();
  mkvass_iniciar(url, 3);
  rodarAte(4.0, 90000, 0.0);
  ok(mkvass_estado() == MKVASS_COMPLETO, "retomada termina COMPLETO");
  mkvass_estatisticas(&ped, NULL, &colhidos, &total);
  printf("    retomada: %ld Ranges (primeira vez foram %ld), %d/%d blocos\n", ped, ped1, colhidos, total);
  ok(ped < ped1, "retomada fez MENOS Ranges que a primeira vez");
  ok(colhidos == nEsp, "retomada colheu tudo");
  ok(conferirCues(esp, nEsp) == nEsp, "todos os cues batem apos a retomada");
  esperarFio();
  corpo = dados_ler(sidecar);
  ok(corpo && !strncmp(corpo, "; mkvass-estado: completo", 25), "sidecar virou completo");
  free(corpo);

  printf("\n[5] no-go\n");
  mkvass_parar(); esperarFio(); legenda_desligar();
  zerarServidor();
  mkvass_iniciar(url, 1);                 // faixa de VIDEO
  rodarAte(1.0, 20000, 0.0);
  ok(mkvass_estado() == MKVASS_NOGO_FAIXA, "faixa 1 (video): NOGO_FAIXA");
  ok(mkvass_nogo(), "mkvass_nogo() == 1");
  mkvass_estatisticas(&ped, NULL, NULL, NULL);
  ok(ped <= 2, "no-go de faixa custou <= 2 Ranges (so o cabecalho)");

  mkvass_parar(); esperarFio();
  mkvass_iniciar(urlSrt, 3);              // S_TEXT/UTF8
  rodarAte(1.0, 20000, 0.0);
  ok(mkvass_estado() == MKVASS_NOGO_FAIXA, "faixa SRT: NOGO_FAIXA");

  mkvass_parar(); esperarFio();
  zerarServidor();
  mkvass_iniciar(urlNoRange, 3);          // servidor ignora Range
  rodarAte(1.0, 60000, 0.0);
  ok(mkvass_estado() == MKVASS_NOGO_SEM_RANGE, "servidor sem Range: NOGO_SEM_RANGE");
  mkvass_estatisticas(&ped, &bytes, NULL, NULL);
  printf("    sem Range: %ld Ranges, %ld bytes\n", ped, bytes);
  ok(bytes < 64L * 1024 + 16L * 1024 + 64, "sem Range: o teto do rede cortou o corpo");

  mkvass_parar(); esperarFio();
  mkvass_iniciar(urlAss, 3);              // o .ass servido: nao e MKV
  rodarAte(1.0, 20000, 0.0);
  ok(mkvass_estado() == MKVASS_NOGO_NAO_MKV, "arquivo que nao e MKV: NOGO_NAO_MKV");

  printf("\n[6] troca durante Range lento e resposta curta\n");
  mkvass_parar(); esperarFio(); legenda_desligar();
  nomeSidecar(urlLento, 1, sidecarLento, sizeof sidecarLento);
  dados_apagar(sidecarLento);
  dados_apagar(sidecar);
  // A faixa 1 do pedido velho falha depois de um Range atrasado. O pedido
  // novo ja esta ativo nesse instante; o no-go velho nao pode aparecer nele.
  mkvass_iniciar(urlLento, 1);
  usleep(100 * 1000);
  mkvass_iniciar(url, 3);
  // Sem sidecar, uma janela parada em zero so cobre parte dos Dialogues.
  // Percorra uma janela sobreposta e volte ao inicio para que o teste valide
  // a troca durante Range e a conclusao da faixa inteira.
  mkvass_passo(60.0);
  { long t0 = agoraMs(); int viuNogo = 0, voltou = 0;
    int c = 0, n = 0;
    while (mkvass_ocupado() && agoraMs() - t0 < 30000) {
      mkvass_passo(voltou ? 0.0 : 60.0);
      mkvass_estatisticas(NULL, NULL, &c, &n);
      // Em 60 s a janela [52,150] cobre 22 dos 40 Dialogues desta fixture;
      // so volte ao inicio depois de esvaziar essa janela, senao os ultimos
      // cues ficam fora tanto da janela de 60 s quanto da de zero.
      if (!voltou && n == 40 && c >= n - 18) { mkvass_passo(0.0); voltou = 1; }
      if (mkvass_estado() >= MKVASS_NOGO) viuNogo = 1;
      usleep(20 * 1000);
    }
    ok(!viuNogo, "troca durante Range nao publica no-go do worker antigo");
    ok(mkvass_estado() == MKVASS_COMPLETO, "pedido novo termina COMPLETO"); }

  mkvass_parar(); esperarFio(); legenda_desligar();
  nomeSidecar(urlCurto, 3, sidecarCurto, sizeof sidecarCurto);
  dados_apagar(sidecarCurto);
  zerarServidor();
  mkvass_iniciar(urlCurto, 3);
  rodarAte(1.0, 30000, 0.0);
  ok(mkvass_estado() == MKVASS_NOGO_REDE, "Range truncado vira NOGO_REDE");
  mkvass_estatisticas(NULL, NULL, &colhidos, &total);
  ok(colhidos < total, "Range truncado nao conta cue ausente como concluido");
  corpo = dados_ler(sidecarCurto);
  ok(!corpo || strncmp(corpo, "; mkvass-estado: completo", 25),
     "Range truncado nao grava sidecar completo");
  free(corpo);

  mkvass_parar(); esperarFio();
  free(esp);
  printf("\n%s\n", falhas ? "FALHOU" : "mkvass: ok");
  return falhas ? 1 : 0;
}
