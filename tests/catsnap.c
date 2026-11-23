#include "catsnap.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  const int64_t then = 1000000;
  assert(catsnap_needs_fetch(0, 1, 1, 0, then, then));
  assert(catsnap_needs_fetch(1, 0, 1, 0, then, then));
  assert(catsnap_needs_fetch(1, 1, 0, 0, then, then));
  assert(catsnap_needs_fetch(1, 1, 1, 1, then, then));
  assert(catsnap_needs_fetch(1, 1, 1, 0, then, then + CATSNAP_MAX_AGE + 1));
  assert(!catsnap_needs_fetch(1, 1, 1, 0, then, then + CATSNAP_MAX_AGE));
  assert(!catsnap_needs_fetch(1, 1, 1, 0, then, then));
  assert(catsnap_needs_fetch(1, 1, 1, 0, then, then - 1));
  assert(catsnap_needs_fetch(1, 1, 1, 0, 0, then));
  puts("catsnap: missing, version, identity, refresh, age, fresh, clock rollback OK");
  return 0;
}
