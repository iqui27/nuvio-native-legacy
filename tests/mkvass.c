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
#include "../src/mkv.h"
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
static long contagemDe(const char *nome) {
  char url[600]; char *r; long v;
  snprintf(url, sizeof url, "%s/%s", base, nome);
  r = rede_baixar(url, 5);
  v = r ? atol(r) : -1;
  free(r);
  return v;
}

// O papel de faixas.c numa falha passageira, sem SDL: no-go -> recuo curto
// -> nova tentativa, ate COMPLETO, no-go definitivo ou o prazo. `tv` faz o
// que a folha faz depois de MKVASS_TENTATIVAS_OVERLAY: desliga o overlay (a
// TV desenharia) e retoma SEGURANDO — e confere que o overlay so religa com
// fala nova. Devolve o numero de tentativas; *religouCedo = 1 se o overlay
// voltou antes de um bloco novo.
static void esperarFio(void);
static int retomarAte(long timeoutMs, int tv, int *religouCedo) {
  long t0 = agoraMs(); int tent = 0, naTV = 0, colNoGo = 0;
  if (religouCedo) *religouCedo = 0;
  while (agoraMs() - t0 < timeoutMs) {
    int e = mkvass_estado(), col = 0;
    mkvass_passo(0.0);
    mkvass_estatisticas(NULL, NULL, &col, NULL);
    if (naTV && legenda_ligada_em(legenda_geracao())) {
      if (col <= colNoGo && e != MKVASS_COMPLETO && religouCedo) *religouCedo = 1;
      naTV = 0;
    }
    if (e == MKVASS_COMPLETO) break;
    if (e >= MKVASS_NOGO) {
      long recuo = mkvass_recuo_ms(e, tent, 0);
      if (!recuo) break;
      tent++;
      esperarFio();
      if (tv && tent > MKVASS_TENTATIVAS_OVERLAY && !naTV) {
        mkvass_estatisticas(NULL, NULL, &colNoGo, NULL);
        legenda_desligar(); naTV = 1;
      }
      usleep(300 * 1000);      // o recuo de verdade e 2-60 s
      if (naTV) mkvass_retomar_segurando(); else mkvass_retomar();
    }
    usleep(20 * 1000);
  }
  return tent;
}

// PRE-BUSCA (#92, v1.4.7): o papel do player. A escolha pelo idioma e do
// player (ling_legenda_auto); aqui, "a primeira legenda" ou "nenhuma".
static int escolherPrimeira(const char *const *idiomas, int n) { (void)idiomas; return n > 0 ? 0 : -1; }
static int escolherNenhuma(const char *const *idiomas, int n) { (void)idiomas; (void)n; return -1; }

// O "video" do /rdN: com ele aberto o servidor corta e recusa o resto.
static void videoServidor(int aberto) {
  char u[600]; char *r;
  snprintf(u, sizeof u, "%s/%s", base, aberto ? "videoabrir" : "videofechar");
  r = rede_baixar(u, 5); free(r);
  mkvass_video_aberto(aberto);
}

// O player segurando o video: espera a pre-busca acabar ou o teto vencer.
// Devolve os ms ate o video ser "solto".
static long esperarPrebusca(void) {
  long t0 = agoraMs();
  while (mkvass_prebusca_fase() == 1 && agoraMs() - t0 < MKVASS_PREBUSCA_MS) usleep(5 * 1000);
  return agoraMs() - t0;
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

// Quantos blocos esperados estao vivos no overlay agora (mesmo texto, no
// meio do tempo deles). Sem log: e para amostrar em laco.
static int vivosDe(const LegendaCue *esp, int n) {
  int i, vivos = 0;
  for (i = 0; i < n; i++) {
    LegendaCue v[LEGENDA_SIMULTANEAS]; int k, m;
    m = legenda_cues((esp[i].inicio + esp[i].fim) / 2.0, 0, v, LEGENDA_SIMULTANEAS);
    for (k = 0; k < m; k++) if (!strcmp(v[k].texto, esp[i].texto)) { vivos++; break; }
  }
  return vivos;
}

static int contarDialogue(const char *s) {
  int n = 0; const char *p = s;
  while ((p = strstr(p, "\nDialogue:"))) { n++; p++; }
  return n;
}

static const char *maiorTextoDialogue(const char *s, size_t *tamanho) {
  const char *p = s, *maior = NULL;
  size_t maiorN = 0;
  while ((p = strstr(p, "Dialogue:"))) {
    const char *fim = strchr(p, '\n'), *q, *texto = NULL;
    size_t n;
    if (!fim) fim = p + strlen(p);
    for (q = p; q + 1 < fim; q++)
      if (q[0] == ',' && q[1] == ',') texto = q + 2;
    if (texto) {
      n = (size_t)(fim - texto);
      if (n > maiorN) { maior = texto; maiorN = n; }
    }
    p = fim;
  }
  if (tamanho) *tamanho = maiorN;
  return maior;
}

int main(int argc, char **argv) {
  char url[600], urlSrt[600], urlNoRange[600], urlAss[600], urlLento[600], urlCurto[600], urlCurtoCues[600];
  char sidecar[64], sidecarFontes[80], sidecarOrdinal[64], sidecarLento[64], sidecarCurto[64], sidecarCurtoCues[64], cam[600];
  char caminhoSidecar[600];
  LegendaCue *esp = NULL; int nEsp; long tamMkv = 0, tamAss = 0;
  long ped, bytes, ped1; int colhidos, total, maxSeg, r;
  char *corpo;
  char *eventoLongo = NULL; size_t eventoLongoN = 0;

  if (argc < 6) { fprintf(stderr, "uso: %s base mkv ass srtmkv assnome\n", argv[0]); return 2; }
  snprintf(base, sizeof base, "%s", argv[1]);
  snprintf(url, sizeof url, "%s/%s", base, argv[2]);
  snprintf(urlSrt, sizeof urlSrt, "%s/%s", base, argv[4]);
  snprintf(urlNoRange, sizeof urlNoRange, "%s/norange/%s", base, argv[2]);
  snprintf(urlAss, sizeof urlAss, "%s/%s", base, argv[5]);
  snprintf(urlLento, sizeof urlLento, "%s/lento/%s", base, argv[2]);
  snprintf(urlCurto, sizeof urlCurto, "%s/curto/%s", base, argv[2]);
  snprintf(urlCurtoCues, sizeof urlCurtoCues, "%s/curtocues/%s", base, argv[2]);
  rede_preparar();
  dados_iniciar(".");
  ok(dados_dir()[0] != 0, "dados_dir() (NUVIO_DADOS do .sh)");

  { struct stat st; ok(stat(argv[3], &st) == 0, "o .ass de referencia existe");
    corpo = lerArquivo(argv[3], &tamAss);
    nEsp = corpo ? legenda_extrair_ass(corpo, &esp) : 0;
    { const char *texto = corpo ? maiorTextoDialogue(corpo, &eventoLongoN) : NULL;
      if (texto && eventoLongoN > 1024) {
        eventoLongo = malloc(eventoLongoN);
        if (eventoLongo) memcpy(eventoLongo, texto, eventoLongoN);
      }
      ok(eventoLongo != NULL, "referencia inclui evento ASS com mais de 1 KiB"); }
    free(corpo);
    ok(nEsp >= 30, "referencia: >= 30 Dialogue no .ass"); }
  { char *r0 = rede_baixar_trecho(url, 5, 0, 3, &tamMkv);
    ok(r0 && tamMkv == 4, "servidor responde Range (4 bytes)"); free(r0);
    // Tamanho do arquivo: por HEAD nao ha ajuda em rede.h; le do disco.
    snprintf(cam, sizeof cam, "%s/%s", getenv("MKV_DIR") ? getenv("MKV_DIR") : ".", argv[2]);
    { struct stat st; tamMkv = stat(cam, &st) == 0 ? (long)st.st_size : 0; }
    ok(tamMkv > 1000000, "MKV gerado tem mais de 1 MB"); }
  nomeSidecar(url, 3, sidecar, sizeof sidecar);
  snprintf(sidecarFontes, sizeof sidecarFontes, "%s.fonts", sidecar);
  dados_apagar(sidecar);
  dados_apagar(sidecarFontes);

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
  { size_t sidecarLongoN = 0;
    const char *texto = corpo ? maiorTextoDialogue(corpo, &sidecarLongoN) : NULL;
    ok(texto && sidecarLongoN == eventoLongoN && eventoLongo &&
       !memcmp(texto, eventoLongo, eventoLongoN),
       "sidecar preserva integralmente o evento acima de 1 KiB"); }
  { struct stat fonteOriginal, fonteCache;
    dados_caminho(caminhoSidecar, sizeof caminhoSidecar, sidecarFontes);
    ok(stat("deploy/app/fonts/InterDisplay-Regular.ttf", &fonteOriginal) == 0 &&
       stat(caminhoSidecar, &fonteCache) == 0 && fonteCache.st_size > fonteOriginal.st_size,
       "fontes anexadas salvas no cache versionado"); }
  { LegendaCue *v = NULL; int n = corpo ? legenda_extrair_ass(corpo, &v) : 0;
    ok(n == nEsp, "sidecar parseia com o mesmo numero de cues"); free(v); }
  free(corpo);
  free(eventoLongo);
  ped1 = ped;

  printf("\n[1b] selecao por ordinal do Tizen\n");
  mkvass_parar(); esperarFio(); legenda_desligar();
  nomeSidecar(url, -1, sidecarOrdinal, sizeof sidecarOrdinal);
  dados_apagar(sidecarOrdinal);
  zerarServidor();
  mkvass_iniciar_ordinal(url, 0);
  rodarAte(4.0, 90000, 0.0);
  ok(mkvass_estado() == MKVASS_COMPLETO, "ordinal Tizen resolve a primeira faixa de texto");
  mkvass_estatisticas(&ped, NULL, &colhidos, &total);
  ok(total == nEsp && colhidos == nEsp, "ordinal entrega todos os eventos ASS esperados");
  ok(ped == contagemServidor(), "Ranges do ordinal contabilizados");
  r = conferirCues(esp, nEsp);
  ok(r == nEsp, "ordinal preserva textos e tempos dos eventos");
  esperarFio(); legenda_desligar();

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
  ok(corpo && !strncmp(corpo, "; mkvass-estado: parcial", 24), "sidecar gravado como parcial");
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
  // Cabecalho (16 KB) + a primeira leitura do Cues, que desde #92 e de 256 KB
  // de uma vez (cada ida custa ~1,5 s na C9) + a folga antiga de 64 KB.
  ok(bytes < 64L * 1024 + 16L * 1024 + 256L * 1024 + 64, "sem Range: o teto do rede cortou o corpo");

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
      // Em 60 s a janela [52,150] cobre a maior parte dos Dialogues desta fixture;
      // so volte ao inicio depois de esvaziar essa janela, senao os ultimos
      // cues ficam fora tanto da janela de 60 s quanto da de zero.
      if (!voltou && n == nEsp && c >= n - 18) { mkvass_passo(0.0); voltou = 1; }
      if (mkvass_estado() >= MKVASS_NOGO) viuNogo = 1;
      usleep(20 * 1000);
    }
    ok(!viuNogo, "troca durante Range nao publica no-go do worker antigo");
    ok(mkvass_estado() == MKVASS_COMPLETO, "pedido novo termina COMPLETO"); }

  mkvass_parar(); esperarFio(); legenda_desligar();
  nomeSidecar(urlCurtoCues, 3, sidecarCurtoCues, sizeof sidecarCurtoCues);
  dados_apagar(sidecarCurtoCues);
  zerarServidor();
  mkvass_iniciar(urlCurtoCues, 3);
  rodarAte(1.0, 30000, 0.0);
  ok(mkvass_estado() == MKVASS_NOGO_REDE, "Range curto nos CuePoints vira NOGO_REDE");
  mkvass_estatisticas(NULL, NULL, &colhidos, &total);
  ok(colhidos == 0 && total == 0, "CuePoints truncados nao inventam blocos ASS");
  corpo = dados_ler(sidecarCurtoCues);
  ok(!corpo || strncmp(corpo, "; mkvass-estado: completo", 25),
     "CuePoints truncados nao gravam sidecar completo");
  free(corpo);

  // #92 (webOS 25): sem CuePoint da faixa de legenda (mkvmerge --cues none) e
  // sem Cues nenhum. Antes: NOGO_SEM_INDICE e a faixa voltava a TV. Agora o
  // modulo VARRE os Clusters e entrega os mesmos eventos, com os mesmos tempos.
  if (argc > 7 && argv[6][0]) {
    char urlSem[600], scSem[64], scSemF[80];
    printf("\n[6b] sem CuePoint da faixa: varredura dos Clusters pelo indice do video\n");
    snprintf(urlSem, sizeof urlSem, "%s/%s", base, argv[6]);
    nomeSidecar(urlSem, 3, scSem, sizeof scSem);
    snprintf(scSemF, sizeof scSemF, "%s.fonts", scSem);
    dados_apagar(scSem); dados_apagar(scSemF);
    mkvass_parar(); esperarFio(); legenda_desligar();
    zerarServidor();
    mkvass_iniciar(urlSem, 3);
    maxSeg = rodarAte(4.0, 120000, 0.0);
    ok(mkvass_estado() == MKVASS_COMPLETO, "sem cues da faixa: termina COMPLETO (antes: NOGO_SEM_INDICE)");
    ok(mkvass_varredura() == 1, "colheu em modo varredura");
    mkvass_estatisticas(&ped, &bytes, &colhidos, &total);
    printf("    %d/%d trechos, %ld Ranges, %ld bytes de %ld (%.1f %%), pico %d/s\n",
           colhidos, total, ped, bytes, tamMkv, 100.0 * bytes / tamMkv, maxSeg);
    ok(total > 1 && colhidos == total, "todos os trechos varridos");
    ok(maxSeg >= 0 && maxSeg <= 8, "teto: no maximo 8 Ranges num mesmo segundo (varredura)");
    r = conferirCues(esp, nEsp);
    printf("    %d/%d cues casaram (texto, ±20 ms, \\an, \\pos)\n", r, nEsp);
    ok(r == nEsp, "varredura: todos os cues batem com o .ass original");
    esperarFio();
    corpo = dados_ler(scSem);
    ok(corpo && !strncmp(corpo, "; mkvass-estado: completo", 25), "varredura grava sidecar completo");
    free(corpo);
  } else printf("\n[6b] pulado: fixture sem cues da faixa nao gerada (mkvmerge?)\n");

  if (argc > 7 && argv[7][0]) {
    char urlNo[600], scNo[64], scNoF[80];
    printf("\n[6c] sem Cues nenhum: varredura do primeiro Cluster ao fim do Segment\n");
    snprintf(urlNo, sizeof urlNo, "%s/%s", base, argv[7]);
    nomeSidecar(urlNo, 3, scNo, sizeof scNo);
    snprintf(scNoF, sizeof scNoF, "%s.fonts", scNo);
    dados_apagar(scNo); dados_apagar(scNoF);
    mkvass_parar(); esperarFio(); legenda_desligar();
    zerarServidor();
    mkvass_iniciar(urlNo, 3);
    maxSeg = rodarAte(4.0, 120000, 0.0);
    ok(mkvass_estado() == MKVASS_COMPLETO, "--no-cues: termina COMPLETO");
    ok(mkvass_varredura() == 1, "colheu em modo varredura");
    mkvass_estatisticas(&ped, &bytes, &colhidos, &total);
    printf("    %d/%d trechos, %ld Ranges, %ld bytes de %ld (%.1f %%), pico %d/s\n",
           colhidos, total, ped, bytes, tamMkv, 100.0 * bytes / tamMkv, maxSeg);
    ok(total == 1 && colhidos == 1, "um trecho so, varrido inteiro");
    r = conferirCues(esp, nEsp);
    printf("    %d/%d cues casaram\n", r, nEsp);
    ok(r == nEsp, "--no-cues: todos os cues batem com o .ass original");
  } else printf("\n[6c] pulado: fixture sem Cues nao gerada (mkvmerge?)\n");

  // A varredura e PRESA A JANELA (30 s neste binario, ver mkvass.sh): com o
  // playhead parado os bytes sao uma fracao do arquivo; com o buffer de video
  // curto ela pausa; um seek recomeca na nova posicao em vez de ler o meio.
  { int caso;
    for (caso = 0; caso < 2; caso++) {
      const char *nome = caso == 0 ? (argc > 6 ? argv[6] : "") : (argc > 7 ? argv[7] : "");
      char urlJ[600], scJ[64], scJF[80]; long b0, b1, b2, b3; int c, n, temPerto, temLonge;
      LegendaCue v[LEGENDA_SIMULTANEAS];
      if (!nome[0]) continue;
      printf("\n[6%c] varredura presa a janela (%s): bytes ~ janela, pausa por folga, seek\n",
             caso ? 'e' : 'd', caso ? "sem Cues nenhum" : "cues do video");
      snprintf(urlJ, sizeof urlJ, "%s/%s", base, nome);
      nomeSidecar(urlJ, 3, scJ, sizeof scJ);
      snprintf(scJF, sizeof scJF, "%s.fonts", scJ);
      dados_apagar(scJ); dados_apagar(scJF);
      mkvass_parar(); esperarFio(); legenda_desligar();
      zerarServidor();
      mkvass_folga(-1.0);
      mkvass_iniciar(urlJ, 3);
      { long t0 = agoraMs(); while (agoraMs() - t0 < 8000) { mkvass_passo(0.0); usleep(20 * 1000); } }
      mkvass_estatisticas(NULL, &b0, &c, &n);
      printf("    playhead 0, janela 30 s: %ld bytes de %ld (%.1f %%), %d/%d trechos\n",
             b0, tamMkv, 100.0 * b0 / tamMkv, c, n);
      ok(mkvass_estado() == MKVASS_COLHENDO, "com o playhead parado a varredura NAO fecha o arquivo");
      ok(b0 > tamMkv / 10 && b0 * 2 < tamMkv, "bytes lidos entre 10 % e 50 % do arquivo (janela de 30 s em 120 s)");
      temPerto = legenda_cues(5.5, 0, v, LEGENDA_SIMULTANEAS) > 0;
      temLonge = legenda_cues(100.0 + 0.5, 0, v, LEGENDA_SIMULTANEAS) > 0;
      ok(temPerto && !temLonge, "fala dos 5 s entregue, fala dos 100 s ainda nao");
      // Buffer de video curto: pausa. O playhead anda 5 s (a janela pede mais
      // um Cluster) mas nada e lido enquanto a folga esta abaixo de 20 s.
      mkvass_folga(5.0);
      { long t0 = agoraMs(); while (agoraMs() - t0 < 3000) { mkvass_passo(5.0); usleep(20 * 1000); } }
      mkvass_estatisticas(NULL, &b1, NULL, NULL);
      ok(b1 <= b0 + 64L * 1024, "buffer de video < 20 s: a varredura pausou (bytes parados)");
      mkvass_folga(60.0);
      { long t0 = agoraMs(); while (agoraMs() - t0 < 3000) { mkvass_passo(5.0); usleep(20 * 1000); } }
      mkvass_estatisticas(NULL, &b2, NULL, NULL);
      ok(b2 > b1, "buffer de volta: a varredura retomou");
      // Seek longo: recomeca na nova posicao, nao le o meio.
      mkvass_passo(100.0);
      { long t0 = agoraMs(); while (agoraMs() - t0 < 8000) { mkvass_passo(100.0); usleep(20 * 1000); } }
      mkvass_estatisticas(NULL, &b3, &c, &n);
      printf("    apos seek para 100 s: %ld bytes (%.1f %%), %d/%d trechos\n", b3, 100.0 * b3 / tamMkv, c, n);
      temPerto = legenda_cues(100.0 + 0.5, 0, v, LEGENDA_SIMULTANEAS) > 0;
      temLonge = legenda_cues(59.0 + 0.5, 0, v, LEGENDA_SIMULTANEAS) > 0;
      ok(temPerto, "seek: fala dos 100 s entregue");
      ok(!temLonge, "seek: fala dos 59 s (fora das duas janelas) NAO foi lida");
      ok(b3 * 10 < tamMkv * 8, "seek: bytes totais < 80 % do arquivo (o meio ficou de fora)");
      ok(mkvass_estado() == MKVASS_COLHENDO, "sem passar pelo meio o arquivo nao fecha completo");
      mkvass_parar(); esperarFio(); legenda_desligar();
    } }
  mkvass_folga(-1.0);

  // CuePoint DA FAIXA existe, mas o bloco nao se acha por ele: sem
  // CueRelativePosition (mkvmerge --engage no_cue_relative_position) ou com
  // uma posicao que nao cai num bloco da faixa. Antes: varredura (a folha
  // dizia "varrendo o arquivo") ou o ponto era dado como desistido e a fala
  // sumia. Agora o Cluster que o CueClusterPosition aponta e lido inteiro.
  { int caso;
    for (caso = 0; caso < 2; caso++) {
      const char *nome = argc > 8 + caso ? argv[8 + caso] : "";
      char urlR[600], scR[64], scRF[80];
      if (!nome[0]) { printf("\n[6%c] pulado: fixture nao gerada\n", caso ? 'g' : 'f'); continue; }
      printf("\n[6%c] %s: le o Cluster do CuePoint inteiro\n", caso ? 'g' : 'f',
             caso ? "CueRelativePosition invalido" : "sem CueRelativePosition");
      snprintf(urlR, sizeof urlR, "%s/%s", base, nome);
      nomeSidecar(urlR, 3, scR, sizeof scR);
      snprintf(scRF, sizeof scRF, "%s.fonts", scR);
      dados_apagar(scR); dados_apagar(scRF);
      mkvass_parar(); esperarFio(); legenda_desligar();
      zerarServidor();
      mkvass_iniciar(urlR, 3);
      maxSeg = rodarAte(4.0, 120000, 0.0);
      ok(mkvass_estado() == MKVASS_COMPLETO, "termina COMPLETO");
      ok(mkvass_varredura() == 0, "pelo indice da faixa, sem cair na varredura");
      mkvass_estatisticas(&ped, &bytes, &colhidos, &total);
      printf("    %d/%d pontos, %ld Ranges, %ld bytes de %ld (%.1f %%), pico %d/s\n",
             colhidos, total, ped, bytes, tamMkv, 100.0 * bytes / tamMkv, maxSeg);
      ok(total == nEsp && colhidos == nEsp, "um ponto por Dialogue, todos colhidos (nenhum desistido)");
      ok(maxSeg >= 0 && maxSeg <= 8, "teto: no maximo 8 Ranges num mesmo segundo");
      r = conferirCues(esp, nEsp);
      printf("    %d/%d cues casaram\n", r, nEsp);
      ok(r == nEsp, "todos os cues batem com o .ass original");
      mkvass_parar(); esperarFio(); legenda_desligar();
    } }

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

  printf("\n[7] playhead parado: a faixa INTEIRA chega em segundo plano\n");
  mkvass_parar(); esperarFio(); legenda_desligar();
  dados_apagar(sidecar); dados_apagar(sidecarFontes);
  zerarServidor();
  mkvass_iniciar(url, 3);
  { long t0 = agoraMs(); LegendaCue v[LEGENDA_SIMULTANEAS]; long tPrimeira = -1;
    // A primeira fala (1,0 s) precisa estar no overlay logo, sem esperar a
    // passada; depois o resto, inclusive o que esta alem da janela de 90 s.
    while (agoraMs() - t0 < 60000 && mkvass_estado() != MKVASS_COMPLETO &&
           mkvass_estado() < MKVASS_NOGO) {
      mkvass_passo(0.0);
      if (tPrimeira < 0 && legenda_cues(1.2, 0, v, LEGENDA_SIMULTANEAS) > 0) tPrimeira = agoraMs() - t0;
      usleep(10 * 1000);
    }
    mkvass_estatisticas(&ped, NULL, &colhidos, &total);
    printf("    primeira fala no overlay em %ld ms; %d/%d blocos, %ld Ranges\n", tPrimeira, colhidos, total, ped);
    ok(tPrimeira >= 0 && tPrimeira < 1500, "primeira fala entregue em < 1,5 s (sem esperar a passada)");
    ok(mkvass_estado() == MKVASS_COMPLETO, "playhead parado em 0: faixa completa (antes: so a janela)");
    ok(colhidos == nEsp, "todos os blocos, inclusive os alem da janela");
    ok(ped <= nEsp + 8, "palpite do Cluster: ~1 Range por bloco (antes: 2)");
    ok(conferirCues(esp, nEsp) == nEsp, "tempos pelo CueTime batem com o .ass (±20 ms)"); }

  printf("\n[8] servidor lento (1,2 s por Range, o medido na C9): Ranges em paralelo\n");
  mkvass_parar(); esperarFio(); legenda_desligar();
  { char scLento[64]; long t0, tIndice = -1, tPrimeira = -1; int c = 0, n = 0;
    LegendaCue v[LEGENDA_SIMULTANEAS];
    nomeSidecar(urlLento, 3, scLento, sizeof scLento);
    dados_apagar(scLento);
    { char f2[80]; snprintf(f2, sizeof f2, "%s.fonts", scLento); dados_apagar(f2); }
    zerarServidor();
    t0 = agoraMs();
    mkvass_iniciar(urlLento, 3);
    while (agoraMs() - t0 < 15000 && mkvass_estado() < MKVASS_NOGO) {
      mkvass_passo(0.0);
      mkvass_estatisticas(NULL, NULL, &c, &n);
      if (tIndice < 0 && n > 0) tIndice = agoraMs() - t0;
      if (tPrimeira < 0 && legenda_cues(1.2, 0, v, LEGENDA_SIMULTANEAS) > 0) tPrimeira = agoraMs() - t0;
      if (mkvass_estado() == MKVASS_COMPLETO) break;
      usleep(20 * 1000);
    }
    printf("    indice em %ld ms, primeira fala em %ld ms, %d/%d blocos em %ld ms\n",
           tIndice, tPrimeira, c, n, agoraMs() - t0);
    ok(tIndice >= 0 && tIndice < 3000, "indice com 2 Ranges (cabecalho + Cues de uma vez), sem esperar as fontes");
    ok(tPrimeira >= 0 && tPrimeira < 4500, "primeira fala em < 4,5 s (antes: fontes e Cues em serie, ~7 Ranges)");
    ok(c >= 20, "15 s colhem >= 20 blocos (um Range por vez: ~10)"); }

  // Troca de faixa com colheita EM VOO (servidor lento: sempre ha um Range
  // no ar). Nenhum lote da faixa anterior pode chegar a tela depois.
  if (argc > 10 && argv[10][0]) {
    char urlB[600], scB[64], scA[64], f2[80]; long t0; int viu = 0, voltou = 0, viuA = 0, viuB = 0;
    LegendaCue v[LEGENDA_SIMULTANEAS];
    printf("\n[9] troca de faixa com colheita em voo: nada da faixa antiga\n");
    snprintf(urlB, sizeof urlB, "%s/%s", base, argv[10]);
    nomeSidecar(urlB, 3, scB, sizeof scB); dados_apagar(scB);
    snprintf(f2, sizeof f2, "%s.fonts", scB); dados_apagar(f2);
    nomeSidecar(urlLento, 3, scA, sizeof scA); dados_apagar(scA);
    snprintf(f2, sizeof f2, "%s.fonts", scA); dados_apagar(f2);
    mkvass_parar(); esperarFio(); legenda_desligar();
    mkvass_iniciar(urlLento, 3);
    t0 = agoraMs();
    while (agoraMs() - t0 < 15000 && !viu) {
      mkvass_passo(0.0);
      viu = legenda_cues(1.2, 0, v, LEGENDA_SIMULTANEAS) > 0;
      usleep(20 * 1000);
    }
    ok(viu, "faixa A entregou a primeira fala");
    // Outro dono tomou a legenda (desligada, externa) SEM parar o fio: o lote
    // seguinte de A carrega a geracao antiga e tem de ser descartado.
    legenda_desligar();
    t0 = agoraMs();
    while (agoraMs() - t0 < 6000) {
      mkvass_passo(0.0);
      if (vivosDe(esp, nEsp) > 0) voltou = 1;
      usleep(50 * 1000);
    }
    ok(mkvass_estado() == MKVASS_COLHENDO, "faixa A continuou colhendo (o lote e que foi descartado)");
    ok(!voltou, "lote atrasado de A nao religa a legenda desligada");
    // Troca de verdade: A com Range no ar, B escolhida.
    mkvass_parar(); legenda_desligar();
    mkvass_iniciar(urlB, 3);
    t0 = agoraMs();
    while (agoraMs() - t0 < 30000 && mkvass_estado() != MKVASS_COMPLETO && mkvass_estado() < MKVASS_NOGO) {
      mkvass_passo(0.0);
      if (vivosDe(esp, nEsp) > 0) viuA = 1;
      { int m = legenda_cues(1.5, 0, v, LEGENDA_SIMULTANEAS), k;
        for (k = 0; k < m; k++) if (!strncmp(v[k].texto, "Outra faixa", 11)) viuB = 1; }
      usleep(20 * 1000);
    }
    if (vivosDe(esp, nEsp) > 0) viuA = 1;
    ok(mkvass_estado() == MKVASS_COMPLETO, "faixa B termina COMPLETO");
    ok(viuB, "faixa B entregue ao overlay");
    ok(!viuA, "nenhum texto da faixa A depois da troca");
  } else printf("\n[9] pulado: segunda faixa nao gerada\n");

  printf("\n[10] no-go passageiro x definitivo: politica de recuo\n");
  ok(mkvass_recuo_ms(MKVASS_NOGO_REDE, 0, 0) == 2000 && mkvass_recuo_ms(MKVASS_NOGO_REDE, 1, 0) == 5000 &&
     mkvass_recuo_ms(MKVASS_NOGO_REDE, 2, 0) == 15000, "rede: recuo de 2, 5 e 15 s");
  ok(mkvass_recuo_ms(MKVASS_NOGO_REDE, 3, 0) == 30000 && mkvass_recuo_ms(MKVASS_NOGO_REDE, 4, 0) == 60000 &&
     mkvass_recuo_ms(MKVASS_NOGO_REDE, 50, 0) == 60000,
     "rede: 30 s, depois 60 s, SEM limite (#92: tres falhas nao devolvem a faixa a TV de vez)");
  ok(mkvass_recuo_ms(MKVASS_NOGO_HTTP, 0, 0) == 0, "recusa HTTP definitiva: volta a TV na hora");
  ok(mkvass_recuo_ms(MKVASS_NOGO_SEM_RANGE, 0, 0) > 0, "Range recusado uma vez: passageiro");
  ok(mkvass_recuo_ms(MKVASS_NOGO_SEM_RANGE, 1, 1) == 0, "Range recusado de novo: definitivo");
  ok(!mkvass_recuo_ms(MKVASS_NOGO_NAO_MKV, 0, 0) && !mkvass_recuo_ms(MKVASS_NOGO_FAIXA, 0, 0) &&
     !mkvass_recuo_ms(MKVASS_NOGO_SEM_INDICE, 0, 0), "nao e MKV, codec, sem indice: definitivos");

  // Falha PASSAGEIRA de verdade: o servidor responde 503 a uma rajada de
  // pedidos no meio da colheita (e, no outro caso, ao primeiro, o cabecalho).
  // O teste faz o papel de faixas.c: no-go -> mkvass_recuo_ms -> mkvass_retomar,
  // SEM legenda_desligar. O que ja estava no overlay fica, a geracao da
  // legenda nao muda (nada recarrega, nada pisca) e a faixa fecha COMPLETA.
  { int caso;
    for (caso = 0; caso < 2; caso++) {
      char urlF[4096], scF[64], scFF[80]; long t0; int falhasF = 0, recF = 0, viuNogo = 0, manteve = 1;
      int antes = 0; unsigned g0 = 0; int e;
      snprintf(urlF, sizeof urlF, "%s/%s/%s", base, caso ? "falha1a1" : "falha10a16", argv[2]);
      // URL no limite aceito pelos streams. O servidor usa o basename;
      // cortar qualquer copia (estado, worker ou pool) perde o nome do MKV.
      { size_t prefixo = strlen(base) + 1 + strlen(caso ? "falha1a1" : "falha10a16") + 1;
        size_t fim = sizeof urlF - strlen(argv[2]) - 2;
        memset(urlF + prefixo, 'a', fim - prefixo);
        urlF[fim] = '/';
        strcpy(urlF + fim + 1, argv[2]); }
      printf("\n[10%c] 503 %s: tenta de novo sem devolver a TV\n", caso ? 'b' : 'a',
             caso ? "no primeiro pedido (cabecalho)" : "em rajada no meio da colheita");
      nomeSidecar(urlF, 3, scF, sizeof scF); dados_apagar(scF);
      snprintf(scFF, sizeof scFF, "%s.fonts", scF); dados_apagar(scFF);
      mkvass_parar(); esperarFio(); legenda_desligar();
      zerarServidor();
      mkvass_iniciar(urlF, 3);
      t0 = agoraMs();
      while (agoraMs() - t0 < 120000) {
        e = mkvass_estado();
        mkvass_passo(0.0);
        if (e == MKVASS_COMPLETO) break;
        if (e >= MKVASS_NOGO) {
          long recuo = mkvass_recuo_ms(e, falhasF, recF);
          if (!viuNogo) {
            viuNogo = e;
            antes = vivosDe(esp, nEsp); g0 = legenda_geracao();
            printf("    no-go %d com %d falas no overlay; recuo %ld ms\n", e, antes, recuo);
          }
          if (!recuo) break;
          falhasF++; if (e == MKVASS_NOGO_SEM_RANGE) recF++;
          esperarFio();
          // O overlay nao foi desligado: o que chegou antes continua.
          if (vivosDe(esp, nEsp) < antes) manteve = 0;
          usleep(300 * 1000);   // o recuo de verdade e 2 s; aqui so o bastante para o 503 passar
          mkvass_retomar();
        }
        usleep(20 * 1000);
      }
      e = mkvass_estado();
      printf("    %d tentativa(s), estado final %d\n", falhasF, e);
      // Desde o recuo dentro do fio (#92, 1.4.6) uma rajada curta de 503 e
      // absorvida sem no-go nenhum; se houver, e o passageiro.
      ok(!viuNogo || viuNogo == MKVASS_NOGO_REDE, "503 nunca vira no-go definitivo");
      ok(falhasF <= 1, "rajada curta absorvida no proprio fio (no maximo 1 retomada)");
      ok(e == MKVASS_COMPLETO, "termina COMPLETO depois de tentar de novo");
      ok(conferirCues(esp, nEsp) == nEsp, "todos os cues batem apos a retomada");
      if (!caso && viuNogo) {
        ok(antes > 0 && manteve, "o que ja estava no overlay ficou durante o recuo");
        ok(g0 && legenda_geracao() == g0, "retomada nao recarregou a legenda (mesma geracao: sem pisca)");
      }
    } }

  // #92, 1.4.5: "Legenda ASS: a TV vai desenhar (falha de rede)". Os caminhos
  // de falha de um link de debrid, um por um.
  { char urlL[600], scL[64], scLF[80]; int e, tent, cedo = 0; long r429, ped429;
    printf("\n[11a] CDN que aceita UMA conexao por link: 429 na segunda\n");
    snprintf(urlL, sizeof urlL, "%s/limite1/%s", base, argv[2]);
    nomeSidecar(urlL, 3, scL, sizeof scL); dados_apagar(scL);
    snprintf(scLF, sizeof scLF, "%s.fonts", scL); dados_apagar(scLF);
    mkvass_parar(); esperarFio(); legenda_desligar();
    zerarServidor();
    mkvass_iniciar(urlL, 3);
    tent = retomarAte(120000, 0, NULL);
    e = mkvass_estado();
    r429 = contagemDe("contagem429");
    mkvass_estatisticas(&ped429, NULL, NULL, NULL);
    printf("    estado %d, %d tentativa(s) de faixas.c, %ld recusas 429 em %ld Ranges\n", e, tent, r429, ped429);
    ok(e == MKVASS_COMPLETO, "termina COMPLETO com uma conexao so");
    ok(conferirCues(esp, nEsp) == nEsp, "todos os cues batem");
    ok(r429 >= 1 && r429 <= 6, "freio: poucas recusas, nao uma por Range");

    printf("\n[11b] link que redireciona (307) ao arquivo: url final UMA vez\n");
    snprintf(urlL, sizeof urlL, "%s/redir/%s", base, argv[2]);
    nomeSidecar(urlL, 3, scL, sizeof scL); dados_apagar(scL);
    snprintf(scLF, sizeof scLF, "%s.fonts", scL); dados_apagar(scLF);
    mkvass_parar(); esperarFio(); legenda_desligar();
    zerarServidor();
    mkvass_iniciar(urlL, 3);
    rodarAte(4.0, 90000, 0.0);
    { long rd = contagemDe("contagemredir"), p = 0;
      mkvass_estatisticas(&p, NULL, NULL, NULL);
      printf("    %ld redirecionamento(s) para %ld Ranges\n", rd, p);
      ok(mkvass_estado() == MKVASS_COMPLETO, "COMPLETO pela url final");
      ok(rd == 1 && p > 10, "o 307 foi pago uma vez, nao um por Range"); }
    { char *sc = dados_ler(scL);
      ok(sc && !strncmp(sc, "; mkvass-estado: completo", 25),
         "o sidecar continua pelo nome da url PEDIDA (a proxima abertura acha)");
      free(sc); }

    printf("\n[11c] arquivo que nao existe (404): definitivo, sem insistir\n");
    snprintf(urlL, sizeof urlL, "%s/nao-existe-%ld.mkv", base, (long)agoraMs());
    mkvass_parar(); esperarFio(); legenda_desligar();
    zerarServidor();
    mkvass_iniciar(urlL, 3);
    rodarAte(1.0, 30000, 0.0);
    { int http = 0, curl = -1;
      mkvass_ultima_falha(&http, &curl);
      e = mkvass_estado();
      printf("    estado %d, HTTP %d, curl %d, %ld GET(s)\n", e, http, curl, contagemServidor());
      ok(e == MKVASS_NOGO_HTTP, "404 vira NOGO_HTTP, nao NOGO_REDE");
      ok(http == 404, "o codigo HTTP chega a quem avisa");
      ok(mkvass_recuo_ms(e, 0, 0) == 0, "definitivo: a faixa volta a TV sem recuo"); }

    printf("\n[11d] 503 por mais tempo que tres tentativas: nao desiste, e a TV segura\n");
    snprintf(urlL, sizeof urlL, "%s/falha10a70/%s", base, argv[2]);
    nomeSidecar(urlL, 3, scL, sizeof scL); dados_apagar(scL);
    snprintf(scLF, sizeof scLF, "%s.fonts", scL); dados_apagar(scLF);
    mkvass_parar(); esperarFio(); legenda_desligar();
    zerarServidor();
    mkvass_iniciar(urlL, 3);
    tent = retomarAte(300000, 1, &cedo);
    e = mkvass_estado();
    printf("    %d tentativa(s), estado final %d\n", tent, e);
    ok(tent > 3, "precisou de mais de 3 tentativas (a 1.4.5 desistia na 3a)");
    ok(e == MKVASS_COMPLETO, "e mesmo assim termina COMPLETO");
    ok(conferirCues(esp, nEsp) == nEsp, "todos os cues batem apos as retomadas");
    ok(!cedo, "com a TV desenhando, o overlay so religou com fala nova");

    // #92, 1.4.6 (Real-Debrid pelo Torrentio): o CDN responde 206 ao Range de
    // 256 KB e fecha a conexao depois de 77465 bytes, TODA vez. O modulo
    // jogava o que veio fora e pedia o MESMO Range de novo, para sempre.
    printf("\n[11e] CDN que corta todo Range em 77465 bytes (206 + curl 18)\n");
    snprintf(urlL, sizeof urlL, "%s/corta77465/%s", base, argv[2]);
    nomeSidecar(urlL, 3, scL, sizeof scL); dados_apagar(scL);
    snprintf(scLF, sizeof scLF, "%s.fonts", scL); dados_apagar(scLF);
    mkvass_parar(); esperarFio(); legenda_desligar();
    zerarServidor();
    mkvass_iniciar(urlL, 3);
    tent = retomarAte(120000, 0, NULL);
    e = mkvass_estado();
    { long cortes = contagemDe("contagemcortes"), p = 0, gets = contagemServidor();
      mkvass_estatisticas(&p, NULL, NULL, NULL);
      printf("    estado %d, %d tentativa(s), %ld corte(s) em %ld GET(s), %ld Ranges; teto do host %ld\n",
             e, tent, cortes, gets, p, rede_corte_host(urlL));
      ok(e == MKVASS_COMPLETO, "termina COMPLETO com o corpo cortado");
      ok(tent == 0, "sem no-go: o corte nao virou falha passageira");
      ok(conferirCues(esp, nEsp) == nEsp, "todos os cues batem (os pedacos juntaram certo)");
      ok(cortes >= 1 && cortes <= 6, "o teto foi aprendido: poucos cortes, nao um por Range");
      ok(rede_corte_host(urlL) >= 32 * 1024 && rede_corte_host(urlL) <= 77465,
         "teto do host entre 32 KB e o que o CDN entregou"); }

    printf("\n[11f] CDN que derruba a conexao a mais no mesmo link (206 + corte)\n");
    snprintf(urlL, sizeof urlL, "http://localhost:%s/conex1/%s", strrchr(base, ':') + 1, argv[2]);
    nomeSidecar(urlL, 3, scL, sizeof scL); dados_apagar(scL);
    snprintf(scLF, sizeof scLF, "%s.fonts", scL); dados_apagar(scLF);
    mkvass_parar(); esperarFio(); legenda_desligar();
    zerarServidor();
    mkvass_iniciar(urlL, 3);
    tent = retomarAte(120000, 0, NULL);
    e = mkvass_estado();
    { long cortes = contagemDe("contagemcortes"), p = 0;
      mkvass_estatisticas(&p, NULL, NULL, NULL);
      printf("    estado %d, %d tentativa(s), %ld corte(s) em %ld Ranges\n", e, tent, cortes, p);
      ok(e == MKVASS_COMPLETO, "termina COMPLETO");
      ok(conferirCues(esp, nEsp) == nEsp, "todos os cues batem");
      ok(cortes <= 8, "depois do corte, uma conexao so: poucos cortes"); } }

  // #92, v1.4.7 (webOS 25, Torrentio -> Real-Debrid): com o video tocando, todo
  // Range do mkvass era cortado e o resto recusado. PRE-BUSCA antes do video,
  // recuo longo no resto recusado, fontes que nao derrubam a legenda e a
  // varredura quando o Cues nao vem.
  { char urlP[600], scP[64], scPF[80]; long ms, pedAntes, pedDepois, gets; int e, col = 0, tot = 0;
    printf("\n[12a] pre-busca antes do video, CDN que corta e recusa o resto COM o video aberto\n");
    snprintf(urlP, sizeof urlP, "%s/rd11929/%s", base, argv[2]);
    nomeSidecar(urlP, -1, scP, sizeof scP); dados_apagar(scP);
    snprintf(scPF, sizeof scPF, "%s.fonts", scP); dados_apagar(scPF);
    mkvass_parar(); esperarFio(); legenda_desligar();
    videoServidor(0);
    zerarServidor();
    ok(mkvass_prebuscar(urlP, escolherPrimeira, 0.0), "pre-busca comecou");
    ms = esperarPrebusca();
    mkvass_estatisticas(&pedAntes, NULL, &col, &tot);
    printf("    video solto em %ld ms: %d/%d blocos, %ld Ranges, fase %d\n", ms, col, tot, pedAntes,
           mkvass_prebusca_fase());
    ok(ms <= MKVASS_PREBUSCA_MS + 250, "o video sai dentro do teto da pre-busca");
    ok(mkvass_prebusca_fase() == 2, "pre-busca terminou dentro do teto (servidor local)");
    ok(tot == nEsp && col == nEsp, "cabecalho, Cues e os blocos da janela lidos antes do video");
    ok(!legenda_ligada_em(legenda_geracao()), "sem adocao, nada foi entregue ao overlay");
    { unsigned char *cab = NULL; long cabN = 0; MkvFaixa fx[MKV_MAX_FAIXAS]; int nfx = 0;
      long g0 = contagemServidor();
      if (mkvass_cabecalho(urlP, &cab, &cabN)) nfx = mkv_faixas_do_trecho(cab, cabN, fx, MKV_MAX_FAIXAS, NULL, 0, NULL);
      free(cab);
      ok(nfx >= 3 && contagemServidor() == g0, "sonda do cabecalho pelo trecho da pre-busca: faixas, sem rede"); }
    // O video abre; faixas.c escolhe a faixa (legenda_desligar + ordinal 0).
    videoServidor(1);
    legenda_desligar();
    mkvass_iniciar_ordinal(urlP, 0);
    { long t0 = agoraMs();
      while (!legenda_ligada_em(legenda_geracao()) && agoraMs() - t0 < 3000) usleep(5 * 1000);
      printf("    adocao: overlay com a legenda em %ld ms\n", agoraMs() - t0);
      ok(legenda_ligada_em(legenda_geracao()), "adotada: a legenda chega ao overlay"); }
    rodarAte(4.0, 30000, 0.0);
    e = mkvass_estado();
    mkvass_estatisticas(&pedDepois, NULL, NULL, NULL);
    printf("    estado %d, %ld Ranges antes da adocao, %ld depois, %ld resto(s) recusado(s)\n",
           e, pedAntes, pedDepois, contagemDe("contagemrecusas"));
    ok(e == MKVASS_COMPLETO, "COMPLETO com o video aberto, sem no-go");
    ok(pedDepois == pedAntes, "a adocao nao pediu nada de novo ao servidor");
    ok(conferirCues(esp, nEsp) == nEsp, "todos os cues batem");
    esperarFio(); videoServidor(0);

    printf("\n[12b] resto recusado com o video aberto: para e recua LONGO (20 s), sem martelar\n");
    snprintf(urlP, sizeof urlP, "http://localhost:%s/rd11929/%s", strrchr(base, ':') + 1, argv[2]);
    nomeSidecar(urlP, -1, scP, sizeof scP); dados_apagar(scP);
    mkvass_parar(); esperarFio(); legenda_desligar();
    videoServidor(1);
    zerarServidor();
    mkvass_iniciar_ordinal(urlP, 0);
    rodarAte(1.0, 30000, 0.0);
    e = mkvass_estado();
    gets = contagemServidor();
    printf("    estado %d, %ld GET(s), %ld resto(s) recusado(s), recuo %ld ms\n", e, gets,
           contagemDe("contagemrecusas"), mkvass_recuo_ms(e, 0, 0));
    ok(e == MKVASS_NOGO_RESTO, "o cabecalho cortado + resto recusado vira NOGO_RESTO");
    ok(mkvass_recuo_ms(e, 0, 0) == 20000 && mkvass_recuo_ms(e, 1, 0) == 30000 &&
       mkvass_recuo_ms(e, 9, 0) == 60000, "recuo 20, 30... 60 s (nao 2 s e 5 s)");
    ok(gets <= 3, "no maximo 3 GETs: nao martela o resto recusado");
    esperarFio(); videoServidor(0);

    // O Cues do MKV de teste tem poucos KB (no do relato, centenas): o /rdfim
    // corta o que toca o FIM do arquivo, que e onde ele mora.
    printf("\n[12c] Cues cortado e resto recusado (video aberto): varre os Clusters perto do playhead\n");
    snprintf(urlP, sizeof urlP, "%s/rdfim/%s", base, argv[2]);
    nomeSidecar(urlP, -1, scP, sizeof scP); dados_apagar(scP);
    mkvass_parar(); esperarFio(); legenda_desligar();
    videoServidor(1);
    zerarServidor();
    mkvass_iniciar_ordinal(urlP, 0);
    { long t0 = agoraMs(); int vivos = 0, i, nJan = 0;
      while (agoraMs() - t0 < 20000) {
        mkvass_passo(0.0);
        if (mkvass_nogo()) break;
        vivos = 0;
        for (i = 0; i < nEsp; i++) if (esp[i].inicio < 25.0) { LegendaCue v[LEGENDA_SIMULTANEAS];
          int k, m = legenda_cues((esp[i].inicio + esp[i].fim) / 2.0, 0, v, LEGENDA_SIMULTANEAS);
          for (k = 0; k < m; k++) if (!strcmp(v[k].texto, esp[i].texto)) { vivos++; break; } }
        for (nJan = 0, i = 0; i < nEsp; i++) if (esp[i].inicio < 25.0) nJan++;
        if (vivos == nJan) break;
        usleep(50 * 1000);
      }
      e = mkvass_estado();
      printf("    estado %d, varredura %d, %d/%d falas dos primeiros 25 s no overlay, %ld resto(s) recusado(s)\n",
             e, mkvass_varredura(), vivos, nJan, contagemDe("contagemrecusas"));
      ok(!mkvass_nogo(), "sem no-go: o Cues ilegivel nao devolveu a faixa a TV");
      ok(mkvass_varredura(), "caiu para a varredura dos Clusters");
      ok(nJan > 0 && vivos == nJan, "as falas perto do playhead chegaram pela varredura"); }
    mkvass_parar(); esperarFio(); videoServidor(0);

    printf("\n[12d] fontes anexadas que nunca vem (503): a legenda continua no app\n");
    snprintf(urlP, sizeof urlP, "%s/semfontes/%s", base, argv[2]);
    nomeSidecar(urlP, 3, scP, sizeof scP); dados_apagar(scP);
    mkvass_parar(); esperarFio(); legenda_desligar();
    zerarServidor();
    mkvass_iniciar(urlP, 3);
    { int tent = retomarAte(90000, 0, NULL);
      e = mkvass_estado();
      printf("    estado %d, %d tentativa(s) de faixas.c\n", e, tent);
      ok(e == MKVASS_COMPLETO, "COMPLETO sem as fontes (o libass usa as do app)");
      ok(tent == 0, "nenhum no-go: fonte que falha nao conta como falha da legenda");
      ok(conferirCues(esp, nEsp) == nEsp, "todos os cues batem"); }
    esperarFio();

    printf("\n[12e] sem faixa ASS a colher: a pre-busca nao atrasa o video\n");
    { char urlS[600]; long t0;
      snprintf(urlS, sizeof urlS, "%s/%s", base, argv[4]);
      mkvass_parar(); esperarFio(); legenda_desligar();
      zerarServidor();
      t0 = agoraMs();
      mkvass_prebuscar(urlS, escolherPrimeira, 0.0);
      ms = esperarPrebusca();
      esperarFio();
      printf("    SRT: video solto em %ld ms, %ld GET(s), estado %d\n", ms, contagemServidor(), mkvass_estado());
      // O cabecalho da pre-busca e de 64 KB: num host que ja cortou (teto de
      // 32 KB, aprendido no [12c]) ele sai em dois pedacos. Nada alem dele.
      ok(ms < 1000 && contagemServidor() <= 2, "faixa SRT: so o cabecalho e o video sai");
      ok(mkvass_estado() == MKVASS_OCIOSO, "sem no-go para ninguem: volta a ocioso");
      snprintf(urlS, sizeof urlS, "%s/%s", base, argv[2]);
      zerarServidor();
      t0 = agoraMs();
      mkvass_prebuscar(urlS, escolherNenhuma, 0.0);
      ms = esperarPrebusca();
      esperarFio();
      printf("    nenhum idioma casou: video solto em %ld ms, %ld GET(s)\n", ms, contagemServidor());
      ok(ms < 1000 && contagemServidor() <= 2, "nenhuma legenda no idioma: so o cabecalho e o video sai");
      (void)t0; }

    // O TETO: servidor de 1,2 s por Range (o medido na C9). O video sai no
    // teto com o que chegou, e o fio segue em segundo plano.
    printf("\n[12f] servidor lento: o video sai no teto (%d ms) com o indice lido\n", MKVASS_PREBUSCA_MS);
    snprintf(urlP, sizeof urlP, "http://localhost:%s/lento/%s", strrchr(base, ':') + 1, argv[2]);
    nomeSidecar(urlP, -1, scP, sizeof scP); dados_apagar(scP);
    mkvass_parar(); esperarFio(); legenda_desligar();
    zerarServidor();
    mkvass_prebuscar(urlP, escolherPrimeira, 0.0);
    ms = esperarPrebusca();
    mkvass_estatisticas(&pedAntes, NULL, &col, &tot);
    printf("    video solto em %ld ms (fase %d): %d/%d blocos, %ld Ranges\n", ms, mkvass_prebusca_fase(),
           col, tot, pedAntes);
    ok(ms >= MKVASS_PREBUSCA_MS - 50 && ms <= MKVASS_PREBUSCA_MS + 250, "o video sai no teto, nao depois");
    ok(tot == nEsp, "cabecalho e Cues chegaram antes do teto");
    mkvass_parar(); esperarFio(); }

  mkvass_parar(); esperarFio();
  free(esp);
  printf("\n%s\n", falhas ? "FALHOU" : "mkvass: ok");
  return falhas ? 1 : 0;
}
