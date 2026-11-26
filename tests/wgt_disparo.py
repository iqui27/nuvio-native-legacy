# Gera o C da sonda de concorrencia do DISPARO (correcao da r4, PR 761):
# o fio da sonda de MKV e criado para "a.mkv" mas so comeca a rodar DEPOIS que
# video_tocar("b.mp4") avancou a geracao do capmkv e ANTES de trocar urlAtual.
# O pedido (geracao + URL) tem de ser o do disparo; se o fio as captura na sua
# primeira instrucao, le a geracao nova com a URL antiga e capmkv_aplicar_ger
# aceita os capitulos do MKV dentro da reproducao do MP4 — painel de creditos
# aos 950 s num filme de 1200 s. rc 1 em f7d0dc426, rc 0 com o pedido no
# disparo. AVPlay e a resposta de rede sao simulados; a ordem e forcada com
# variaveis de condicao (mesmo desenho da sonda-concorrencia.py da r4).
#
#   python3 tests/wgt_disparo.py <raiz-do-repo> > wgt_disparo.c
from pathlib import Path
import subprocess
import sys

root = Path(sys.argv[1])
tizen = (root / "src/video_tizen.c").read_text()
fixture = subprocess.check_output(
    [sys.executable, str(root / "tests/wgt_publicacao.py"), str(root)], text=True)
fixture = fixture.split("int main(void) {", 1)[0]


def function(signature):
    start = tizen.index(signature)
    return tizen[start:tizen.index("\n}", start) + 2]


assert "#undef mkv_faixas_e_caps\n" in fixture
print("#include <pthread.h>\n#include <sched.h>\n#include <stdatomic.h>")
open_def = "int video_tocar(const char *url) {"
assert fixture.count(open_def) == 1
fixture = fixture.replace(open_def, r'''
static pthread_mutex_t testLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t testCond = PTHREAD_COND_INITIALIZER;
static int iniciarFio, prontoParaPublicar, retomar;
static pthread_t fioMkv;
static int copiarUrl(char *dst, size_t tamanho, const char *formato, const char *url) {
  if (!strcmp(url, "b.mp4")) {
    // video_tocar já invalidou a geração e ainda não atualizou a URL.
    printf("b.mp4 antes de escrever URL: ger=%u url=%s\n", capmkv_geracao(), urlAtual);
    assert(!strcmp(urlAtual, "a.mkv"));
    pthread_mutex_lock(&testLock);
    iniciarFio = 1;
    pthread_cond_broadcast(&testCond);
    while (!prontoParaPublicar) pthread_cond_wait(&testCond, &testLock);
    pthread_mutex_unlock(&testLock);
  }
  return snprintf(dst, tamanho, formato, url);
}
#define snprintf copiarUrl
int video_tocar(const char *url) {''')
fixture = fixture.replace("void video_parar(void) {", "#undef snprintf\nvoid video_parar(void) {")
fixture = fixture.replace("  return rc;\n}", r'''
  pthread_mutex_lock(&testLock);
  prontoParaPublicar = 1;
  pthread_cond_broadcast(&testCond);
  while (!retomar) pthread_cond_wait(&testCond, &testLock);
  pthread_mutex_unlock(&testLock);
  return rc;
}''')
print(fixture)
print(r'''
static void *iniciarDepoisDaLimpeza(void *arg) {
  pthread_mutex_lock(&testLock);
  while (!iniciarFio) pthread_cond_wait(&testCond, &testLock);
  pthread_mutex_unlock(&testLock);
  return lerMkv(arg);
}
static int criarFioControlado(pthread_t *thread, const pthread_attr_t *attr,
                            void *(*start)(void *), void *arg) {
  assert(start == lerMkv);
  return pthread_create(thread, attr, iniciarDepoisDaLimpeza, arg);
}
#define pthread_create criarFioControlado
#define pthread_detach(thread) ((void)(thread))
''')
print(function("void video_sondar_mkv_agora(void) {"))
print(r'''
#undef pthread_create
#undef pthread_detach
int main(void) {
  MkvCap lista[2];
  setvbuf(stdout, NULL, _IONBF, 0);
  assert(video_tocar("a.mkv"));
  printf("a.mkv no disparo: ger=%u url=%s\n", capmkv_geracao(), urlAtual);
  video_sondar_mkv_agora();
  assert(fioMkvVivo == 1);
  fonteMp4 = 1;
  assert(video_tocar("b.mp4"));
  duracaoSeg = 1200;
  intro_definir_duracao(1200, 1);
  assert(capmkv_lista(lista, 2) == 0 && intro_creditos_seg() == 0);
  printf("b.mp4 antes de publicar: lista=0 local=%.0f intro=0\n", video_creditos());
  pthread_mutex_lock(&testLock);
  retomar = 1;
  pthread_cond_broadcast(&testCond);
  pthread_mutex_unlock(&testLock);
  assert(pthread_join(fioMkv, NULL) == 0);
  int n = capmkv_lista(lista, 2);
  double intro = intro_creditos_seg();
  int painel = posplay_regra_filme(950, duracaoSeg, creditoPainel());
  printf("b.mp4 aberto: lista=%d local=%.0f intro=%.0f painel@950=%d\n",
         n, video_creditos(), intro, painel);
  return !!(n || video_creditos() || intro || painel);
}
''')
