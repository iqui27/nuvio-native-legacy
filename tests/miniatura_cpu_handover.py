"""Exercise the production CPU worker with a packet read spanning cancellation."""
from pathlib import Path
import subprocess
import tempfile

source = Path("src/miniatura_android.c").read_text()
start = source.index("typedef struct {\n  Job *j; long long h; MiniMotor *cpu;")
end = source.index("\n#endif", start)
worker = source[start:end]
prefix = r'''
#define _DEFAULT_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef struct { int parar, cpuFeitos; long cpuMs, cpuDecMs; } Job;
typedef struct { int unused; } MiniMotor;
typedef struct { unsigned totalMs, decodMs, resizeMs, colourMs, jpegMs; long bytesPacote; } MiniQuadro;
#define PISO_KB (300L * 1024)
#define CPU_AMOSTRAS 20
static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int reading, proceed, decodes, gpuCloses;
static void mini_dovi_gpu_close(void) { gpuCloses++; }
static int parou(Job *j) { return j->parar; }
static long memDisponivelKB(void) { return 1024L * 1024; }
static size_t mini_motor_memoria(MiniMotor *m, int threads) { (void)m; (void)threads; return 1; }
static int mPacoteCpu(long long h, long k, long *p, unsigned char **pk, int *n) {
  (void)h; (void)k; *p = 42000;
  pthread_mutex_lock(&gate);
  reading = 1; pthread_cond_broadcast(&changed);
  while (!proceed) pthread_cond_wait(&changed, &gate);
  pthread_mutex_unlock(&gate);
  *pk = malloc(1); *n = 1; return 1;
}
static int mFalhaRede(long long h) { (void)h; return 0; }
static int mini_motor_pacote(MiniMotor *m, unsigned char *pk, int n, int t,
                            unsigned char **jpg, int *nj, MiniQuadro *q, char *e, size_t ne) {
  (void)m; (void)pk; (void)n; (void)t; (void)jpg; (void)nj; (void)q; (void)e; (void)ne;
  decodes++; return -1;
}
static void guardarTileDireto(Job *j, long p, unsigned char *jpg, int n, int *f) {
  (void)j; (void)p; (void)n; (void)f; free(jpg);
}
'''
suffix = r'''
int main(void) {
  // Both a normal pause and a handover/stop must revoke a decode whose packet
  // read was already in progress; the queued picture must remain available.
  for (int stop = 0; stop < 2; stop++) {
    reading = proceed = decodes = gpuCloses = 0;
    Job j = {0}; MiniMotor motor = {0};
    long pending[2] = {42000}; int count = 1;
    pthread_mutex_t queue = PTHREAD_MUTEX_INITIALIZER;
    CpuFaixa cf = {.j=&j, .cpu=&motor, .pend=pending, .nPend=&count, .mp=&queue};
    pthread_t thread; assert(!pthread_create(&thread, NULL, cpuTrabalho, &cf));
    pthread_mutex_lock(&gate);
    while (!reading) pthread_cond_wait(&changed, &gate);
    cf.pausa = 1;
    if (stop) cf.fim = 1;
    proceed = 1; pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&gate);
    if (!stop) {
      // Wait for the production worker to return the packet to its queue.
      for (;;) {
        pthread_mutex_lock(&queue); int returned = count == 1; pthread_mutex_unlock(&queue);
        if (returned) break;
        usleep(1000);
      }
      cf.fim = 1;
    }
    pthread_join(thread, NULL);
    assert(decodes == 0 && count == 1 && pending[0] == 42000);
    assert(gpuCloses == 1); // GL cleanup occurs on the exiting worker, even after cancellation.
    pthread_mutex_destroy(&queue);
  }
  puts("PASS: pause and handover revoke a blocked CPU packet read without losing its keyframe");
  return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / "worker.c").write_text(prefix + "\nstatic void mPipelineState(int state,int begin) { (void)state; (void)begin; }\n" + worker + suffix)
    subprocess.run(["cc", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    str(path / "worker.c"), "-pthread", "-o", str(path / "worker")], check=True)
    subprocess.run([str(path / "worker")], check=True, timeout=15)
