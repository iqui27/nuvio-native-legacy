from pathlib import Path
import subprocess
import sys

root = Path(sys.argv[1])
tizen = (root / "src/video_tizen.c").read_text()


def function(signature):
    start = tizen.index(signature)
    return tizen[start:tizen.index("\n}", start) + 2]


fixture = subprocess.check_output(
    [sys.executable, str(root / "tests/capfonte.py"), str(root)], text=True)
print(fixture.split("// A sonda .wgt publica", 1)[0].replace(
    "static char urlAtual[8192];", ""))
print(r'''
#include "video_reconexao.h"
#include "mediaurl.h"
static char urlAtual[NV_MEDIA_URL_MAX], erroTexto[128];
static NvReconexao recon;
static int ligado = 1, temAvplay = 1;
static int roiResetTentativas, roiAtivo, reconPermitida, reconProxima, reconIniciou;
static int reconAudio, reconLeg, reconFaixasPend, recarregarPausa, pausaPedida;
static int nAudio, nLeg, audioAtual, legAtual, faixasLidas, vidW, vidH, houveErro;
static int terminou, seekEm, fonteMp4, mkvPendente, fioMkvVivo;
static int ativo, aberturaEm, janX, janY, janW, janH, tocando, pronto;
static int intercalarTroca, trocas;
static double reconBuscar, posSeg, durSeg, posAmostra;
static unsigned long long posAmostraEm;
static int video_iniciar(void) { return 1; }
static void logarErro(const char *cmd) { (void)cmd; }
static void marco(const char *name) { (void)name; }
static unsigned nv_ticks64(void) { return 1; }
static unsigned SDL_GetTicks(void) { return 1; }
static unsigned video_id_fonte(const char *url) { (void)url; return 0; }
static int avChamar(const char *cmd, ...) { (void)cmd; return 1; }
#define AV0(cmd) avChamar(cmd)
#define AVS(cmd, url) avChamar(cmd, url)
static void aplicarIdiomasDoMkv(MkvFaixa *faixas, int n) { (void)faixas; (void)n; }
static int lerFixture(const char *url, MkvFaixa *faixas, int maxFaixas,
                      MkvCap *caps, int maxCaps, int *nCaps) {
  (void)faixas; (void)maxFaixas;
  assert(!strcmp(url, "a.mkv"));
  assert(maxCaps >= 2);
  caps[0] = (MkvCap){0, "Chapter 1"};
  caps[1] = (MkvCap){950, "End Credits"};
  *nCaps = 2;
  return 1;
}
''')
# A geracao que video_sondar_mkv_agora congela no disparo e entrega ao fio.
print("static unsigned mkvFioCapGer;")
print(function("static void limparCreditosCapitulos(void) {"))
print(function("int video_tocar(const char *url) {"))
print(function("void video_parar(void) {"))
print(r'''
static void verificarMp4(void) {
  MkvCap lista[2];
  assert(capmkv_lista(lista, 2) == 0);
  assert(video_creditos() == 0);
  assert(video_creditos_nomeado() == 0);
  assert(video_creditos_ultimo(duracaoSeg) == 0);
  assert(credEfetivo() == 0);
  assert(credEfetivoHtml5() == 0);
  assert(intro_creditos_seg() == 0);
  assert(!posplay_regra_filme(950, duracaoSeg, creditoPainel()));
}
static void pararEAbrirMp4(void) {
  video_parar();
  intro_desligar();
  fonteMp4 = 1;
  assert(video_tocar("b.mp4"));
  duracaoSeg = 1200;
  intro_definir_duracao(duracaoSeg, 1);
  verificarMp4();
}
static int trocarAntesDePublicar(FILE *stream) {
  int rc = fflush(stream);
  if (intercalarTroca) {
    intercalarTroca = 0;
    trocas++;
    pararEAbrirMp4();
  }
  return rc;
}
#define mkv_faixas_e_caps lerFixture
#define fflush trocarAntesDePublicar
''')
print(function("static void *lerMkv(void *arg) {"))
print(r'''
#undef fflush
#undef mkv_faixas_e_caps
int main(void) {
  MkvCap lista[2];
  assert(video_tocar("a.mkv"));
  lerMkv(NULL);
  assert(capmkv_lista(lista, 2) == 2);
  assert(video_creditos() == 950);
  video_parar();
  assert(capmkv_lista(lista, 2) == 0);
  assert(video_creditos() == 0);
  assert(video_tocar("a.mkv"));
  lerMkv(NULL);
  assert(capmkv_lista(lista, 2) == 2);
  assert(video_tocar("b.mp4"));
  duracaoSeg = 1200;
  verificarMp4();
  intercalarTroca = 1;
  assert(video_tocar("a.mkv"));
  lerMkv(NULL);
  assert(trocas == 1);
  verificarMp4();
  puts("wgt_publicacao: troca durante e apos publicacao sem capitulos/creditos herdados");
  return 0;
}
''')
