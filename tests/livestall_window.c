// A live player's currentPosition is relative to a moving window, not monotonic.
#include <assert.h>
#include <stdio.h>
#include "../src/livestall.h"

static int rolling(unsigned window, int resumed) {
  NvLiveStall s; nv_ls_zerar(&s);
  if(resumed){s.tentativa=2;nv_ls_reaberto(&s,1000);}
  for(unsigned elapsed=0;elapsed<=120000;elapsed+=250) {
    double position=20.0+(elapsed%window)/1000.0;
    int result=nv_ls_passo(&s,1000+elapsed,position,1,0,0,0);
    if(result!=NV_LS_NADA) {
      fprintf(stderr,"FAIL: healthy rolling window=%u ms, elapsed=%u ms, position=%.2f triggered restart=%d\n",window,elapsed,position,result);
      return 0;
    }
  }
  assert(s.tentativa==0); // continuous playback also earns the ordinary retry reset
  return 1;
}
int main(void) {
  const unsigned windows[]={4000,6000,10000,15000,30000};
  for(unsigned i=0;i<sizeof windows/sizeof windows[0];i++) {
    if(!rolling(windows[i],0)||!rolling(windows[i],1))return 1;
  }
  NvLiveStall s;nv_ls_zerar(&s);
  assert(nv_ls_passo(&s,1000,20,1,0,0,0)==NV_LS_NADA);
  assert(nv_ls_passo(&s,2000,21,1,0,0,0)==NV_LS_NADA);
  // Rebase alone is NOT proof of playback. A truly frozen stream still times out
  // eight seconds from its last forward movement, even after the window moved.
  for(unsigned t=3000;t<10000;t+=250)assert(nv_ls_passo(&s,t,11,1,0,0,0)==NV_LS_NADA);
  assert(nv_ls_passo(&s,10000,11,1,0,0,0)==NV_LS_AGENDOU);
  nv_ls_zerar(&s);nv_ls_passo(&s,1000,100,1,0,0,0);
  for(unsigned t=2000;t<9000;t+=1000)assert(nv_ls_passo(&s,t,101-t/1000.0,1,0,0,0)==NV_LS_NADA);
  assert(nv_ls_passo(&s,9000,92,1,0,0,0)==NV_LS_AGENDOU);
  nv_ls_zerar(&s);nv_ls_passo(&s,1000,20,1,0,0,0);
  // Explicit buffering and EOF keep their existing meanings after a rebase.
  assert(nv_ls_passo(&s,2000,10,1,0,0,8000)==NV_LS_AGENDOU);
  nv_ls_zerar(&s);nv_ls_passo(&s,1000,20,1,0,0,0);
#ifdef NV_TPK
  assert(nv_ls_passo(&s,2000,10,1,0,1,0)==NV_LS_REABRIR);
#else
  assert(nv_ls_passo(&s,2000,10,1,0,1,0)==NV_LS_AGENDOU);
#endif
  nv_ls_zerar(&s);nv_ls_passo(&s,1000,20,1,0,0,0);
  for(unsigned t=2000;t<60000;t+=1000)assert(nv_ls_passo(&s,t,10,1,1,0,60000)==NV_LS_NADA);
  // Cube log: READY arrived 80 ms before a queued reconnect. Cancel even
  // before the next position sample, and don't consume a retry that never ran.
  for(unsigned recovered=10500;recovered<=11000;recovered+=500) {
    nv_ls_zerar(&s);nv_ls_passo(&s,1000,31,1,0,0,0);
    assert(nv_ls_passo(&s,10000,31,1,0,0,8006)==NV_LS_AGENDOU);
    assert(nv_ls_passo(&s,recovered,31,1,0,0,0)==NV_LS_NADA);
    assert(!s.pendente && s.tentativa==0);
    for(unsigned t=recovered+250;t<recovered+8000;t+=250)
      assert(nv_ls_passo(&s,t,31+(t-recovered)/1000.0,1,0,0,0)==NV_LS_NADA);
  }
  // READY is not a permanent exemption: a decoder that stays frozen must
  // still be retried after the new grace period.
  nv_ls_zerar(&s);nv_ls_passo(&s,1000,31,1,0,0,0);
  assert(nv_ls_passo(&s,10000,31,1,0,0,8006)==NV_LS_AGENDOU);
  assert(nv_ls_passo(&s,10500,20,1,0,0,0)==NV_LS_NADA);
  assert(nv_ls_passo(&s,18500,20,1,0,0,0)==NV_LS_AGENDOU);
  // A moving window alone still cannot cancel a frozen-position retry;
  // forward motion after that rebase can.
  nv_ls_zerar(&s);nv_ls_passo(&s,1000,31,1,0,0,0);
  assert(nv_ls_passo(&s,9000,31,1,0,0,0)==NV_LS_AGENDOU);
  assert(nv_ls_passo(&s,9250,20,1,0,0,0)==NV_LS_ESPERA);
  assert(nv_ls_passo(&s,9500,20.25,1,0,0,0)==NV_LS_NADA);
  assert(!s.pendente && s.tentativa==0);
  assert(nv_ls_buffer_prazo(0)==8000);
  assert(nv_ls_buffer_prazo(6000)==30000);
  assert(nv_ls_buffer_prazo(12000)==36000);
  assert(nv_ls_buffer_prazo(13000)==39000);
  assert(nv_ls_buffer_prazo(0xffffffffu)==60000);
  // Logs69: a 7.8-second segment needed 20.7 seconds. Neither a moving
  // window nor a delayed READY position sample should cause an early reopen.
  nv_ls_zerar(&s);nv_ls_passo(&s,1000,30,1,0,0,0);
  for(unsigned t=1250;t<=22000;t+=250)
    assert(nv_ls_passo_buf_prazo(&s,t,t<10000?30:0,1,0,0,t-1000,0,36000)==NV_LS_NADA);
  assert(nv_ls_passo_buf_prazo(&s,22250,0,1,0,0,0,0,36000)==NV_LS_NADA);
  assert(!s.pendente && s.tentativa==0);
  assert(nv_ls_passo_buf_prazo(&s,22500,0.25,1,0,0,0,0,36000)==NV_LS_NADA);
  // Dead downloads remain bounded; READY still cancels the queued restart.
  nv_ls_zerar(&s);nv_ls_passo(&s,1000,30,1,0,0,0);
  for(unsigned t=1250;t<37000;t+=250)
    assert(nv_ls_passo_buf_prazo(&s,t,30,1,0,0,t-1000,0,36000)==NV_LS_NADA);
  assert(nv_ls_passo_buf_prazo(&s,37000,30,1,0,0,36000,0,36000)==NV_LS_AGENDOU);
  assert(nv_ls_passo_buf_prazo(&s,38000,30,1,0,0,37000,0,36000)==NV_LS_REABRIR);
  assert(nv_ls_passo_buf_prazo(&s,38001,30,1,0,0,0,0,36000)==NV_LS_NADA);
  // The longer allowance is only for buffering, not a frozen decoder.
  nv_ls_zerar(&s);nv_ls_passo(&s,1000,30,1,0,0,0);
  assert(nv_ls_passo_buf_prazo(&s,9000,30,1,0,0,0,0,36000)==NV_LS_AGENDOU);
  nv_ls_zerar(&s);nv_ls_passo(&s,1000,30,1,0,0,0);
#ifdef NV_TPK
  assert(nv_ls_passo_buf_prazo(&s,2000,30,1,0,1,1000,0,36000)==NV_LS_REABRIR);
#else
  assert(nv_ls_passo_buf_prazo(&s,2000,30,1,0,1,1000,0,36000)==NV_LS_AGENDOU);
#endif
  nv_ls_zerar(&s);nv_ls_passo(&s,1000,30,1,0,0,0);
  assert(nv_ls_passo_buf_prazo(&s,61000,30,1,1,0,60000,0,36000)==NV_LS_NADA);
  // Malformed durations cannot disable the watchdog.
  nv_ls_zerar(&s);nv_ls_passo(&s,1000,30,1,0,0,0);
  assert(nv_ls_passo_buf_prazo(&s,60999,30,1,0,0,59999,0,0xffffffffu)==NV_LS_NADA);
  assert(nv_ls_passo_buf_prazo(&s,61000,30,1,0,0,60000,0,0xffffffffu)==NV_LS_AGENDOU);
#ifdef NV_ANDROID
  // Logs75: READY playback advanced to within two seconds of a stale buffer
  // edge. Low reserve alone must not tear down Media3's still-playing session.
  nv_ls_zerar(&s);
  for (unsigned t=0;t<=29000;t+=250)
    assert(nv_ls_passo_buf_prazo(&s,1000+t,28.591+t/1000.0,1,0,0,0,58.591,36000)==NV_LS_NADA);
  assert(!s.pendente && s.tentativa==0);
  // A delayed segment can arrive without a reconnect or consumed retry.
  assert(nv_ls_passo_buf_prazo(&s,30250,57.841,1,0,0,0,70.591,36000)==NV_LS_NADA);
  // If video really freezes, eight seconds without motion still schedules it.
  assert(nv_ls_passo_buf_prazo(&s,38250,57.841,1,0,0,0,70.591,36000)==NV_LS_AGENDOU);
  // An empty buffer gets the bounded HLS wait instead of the dry-source rule.
  nv_ls_zerar(&s);
  for (unsigned t=0;t<=30000;t+=250)
    assert(nv_ls_passo_buf_prazo(&s,1000+t,28.591+t/1000.0,1,0,0,0,58.591,36000)==NV_LS_NADA);
  for (unsigned t=250;t<36000;t+=250)
    assert(nv_ls_passo_buf_prazo(&s,31000+t,58.591,1,0,0,t,58.591,36000)==NV_LS_NADA);
  assert(nv_ls_passo_buf_prazo(&s,67000,58.591,1,0,0,36000,58.591,36000)==NV_LS_AGENDOU);
#endif
  puts("PASS: sliding live windows keep playing; actual stalls, buffering, EOF, pause and retry limits remain protected");
}
