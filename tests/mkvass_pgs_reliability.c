#include "mkvass.h"
#include "imgsub.h"
#include "dados.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "mkvass_pgs_transport.h"

static void start(int track, double pos, double headroom) {
  pthread_mutex_lock(&trava);
  mkvass_iniciar(esperado, track);
  mkvass_passo(pos); mkvass_folga(headroom);
  pthread_mutex_unlock(&trava);
}
static MkvassDiagnostico wait_cursor(double t) {
  MkvassDiagnostico d;
  double deadline = now()+5;
  do {
    mkvass_diagnostico(&d);
    if ((d.reader_seconds >= t && d.reader_seconds <= t+120) || d.state >= MKVASS_NOGO) break;
    usleep(1000);
  } while (now()<deadline);
  assert(d.reader_seconds >= t && d.reader_seconds <= t+120);
  return d;
}
static void frame(double t, double start_time) {
  ImgsubQuadro *q = NULL;
  double deadline = now()+5;
  do { q=imgsub_pegar(t); if (!q) usleep(1000); } while (!q && now()<deadline);
  assert(q && q->n == 1 && q->ini == start_time && q->fim == start_time+2);
  assert(q->obj[0].x == 100 && q->obj[0].y == 900);
  imgsub_soltar(q);
}
int main(int argc, char **argv) {
  assert(argc==3); arquivo=argv[1]; dados_iniciar(".");
  mkvass_aceitar_imagem(1); legenda_desligar();
  if (!strcmp(argv[2],"sparse")) {
    start(6,0,3);
    MkvassDiagnostico d=wait_cursor(120);
    assert(d.packets==0 && d.network_bytes>0 && d.state==MKVASS_COLHENDO);
    assert(!imgsub_pegar(1)); // No cues yet, not a failed decoder or empty track.
    mkvass_passo(300.5); // Seek while the reader is window-limited.
    frame(300.5,300);
    d=wait_cursor(300);
    assert(d.decoded>=1 && d.late>=1 && d.max_lateness>=0.5);
    mkvass_passo(600.5); frame(600.5,600);
    // Seek back: retained authored timestamps and position survive.
    frame(300.5,300);
    // Switching to an empty track clears the old generation immediately.
    start(7,300,0);
    d=wait_cursor(300);
    assert(!imgsub_pegar(300.5) && d.packets==0);
    // A reader waiting on a bandwidth allowance must stop promptly.
    double t=now(); parar(); assert(now()-t<0.5);
    start(6,600,0); frame(600.5,600); // Zero headroom also has bounded progress.
    parar();
    // Sustained request spacing is enforced at the actual Range boundary.
    // Ignore discovery requests (large offsets include Cues; headers start at 0).
    int paced=0;
    for (int i=1;i<pedidos;i++)
      if (request_lengths[i]==65536 && request_lengths[i-1]==65536 &&
          request_times[i]-request_times[i-1]>=0.045) paced++;
    assert(paced>0);
  } else if (!strcmp(argv[2],"nocues")) {
    start(6,600,3); frame(600.5,600);
    MkvassDiagnostico d; mkvass_diagnostico(&d);
    assert(d.scanning && d.packets>=2 && !d.decode_errors);
    parar();
  } else if (!strcmp(argv[2],"http")) {
    falhar=1; start(6,0,3);
    double deadline=now()+5;
    while (!mkvass_nogo() && now()<deadline) usleep(1000);
    MkvassDiagnostico d; mkvass_diagnostico(&d);
    assert(d.state>=MKVASS_NOGO && d.range_failures>0 && d.packets==0);
    parar();
  } else {
    start(6,600,3);
    wait_cursor(600);
    // Completion of this window is not completion of all earlier clusters.
    double deadline=now()+5;
    MkvassDiagnostico progress;
    do { mkvass_diagnostico(&progress); if (progress.scan_state==MKVASS_SCAN_WINDOW) break; usleep(1000); } while (now()<deadline);
    MkvassDiagnostico d; mkvass_diagnostico(&d);
    if (!strcmp(argv[2],"empty")) {
      assert(d.state==MKVASS_COLHENDO && !d.packets && !d.decode_errors && !d.restore_errors);
      mkvass_passo(0); wait_cursor(120);
      mkvass_passo(300); wait_cursor(300);
      deadline=now()+5;
      while (mkvass_estado()!=MKVASS_COMPLETO && now()<deadline) usleep(1000);
      mkvass_diagnostico(&d);
      assert(d.state==MKVASS_COMPLETO && !d.packets); // Proven empty only after full scan.
    }
    if (!strcmp(argv[2],"malformed")) assert(d.packets && d.decode_errors && !d.restore_errors);
    if (!strcmp(argv[2],"badzlib")) assert(d.packets && d.restore_errors);
    if (!strcmp(argv[2],"truncated")) assert(d.parse_errors);
    assert(!d.range_failures);
    parar();
  }
  puts("pgs reliability: ok");
}
