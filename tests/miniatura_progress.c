#include <assert.h>
#include "../src/miniatura_progress.h"
int main(void) {
  MiniProgress p={0};
  assert(mini_progress(&p,1,1,0,0,0,0)==1);
  assert(mini_progress(&p,1,1,1,0,0,3999)==1);
  assert(mini_progress(&p,1,1,1,0,0,4000)==2);
  assert(mini_progress(&p,1,1,1,1,0,8000)==3);
  assert(mini_progress(&p,1,1,1,1,0,12000)==0);
  assert(mini_progress(&p,2,1,1,1,0,13000)==1);
  assert(mini_progress(&p,2,0,1,1,0,13001)==0);
  assert(mini_progress(&p,3,1,1,1,0,14000)==1);
  assert(mini_progress(&p,3,1,1,1,0,18000)==2);
  assert(mini_progress(&p,3,1,1,1,0,22000)==3);
  // Complete cache: only cached, never started/half/generated again.
  assert(mini_progress(&p,4,1,1,1,2,23000)==4);
  assert(mini_progress(&p,4,1,1,1,2,27000)==0);
  assert(mini_progress(&p,4,1,1,1,2,31000)==0);
  // Partial cache below half can genuinely cross half and complete later.
  assert(mini_progress(&p,5,1,0,0,1,32000)==4);
  assert(mini_progress(&p,5,1,1,0,1,36000)==2);
  assert(mini_progress(&p,5,1,1,1,1,40000)==3);
  // Partial cache already above half must not replay 50%.
  assert(mini_progress(&p,6,1,1,0,1,44000)==4);
  assert(mini_progress(&p,6,1,1,0,1,48000)==0);
  assert(mini_progress(&p,6,1,1,1,1,50000)==3);
  assert(mini_progress(&p,7,1,0,0,2,55000)==4);
  assert(mini_progress(&p,7,1,1,1,2,59000)==0);
  return 0;
}
