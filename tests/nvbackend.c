#include "nvbackend.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define NV_TEST_DADOS "/tmp/nvbackend-test"

int main(void) {
  assert(nvbackend_definir_url("https://api.example.test") == 1);
  assert(!strcmp(nv_rec_url(), "https://api.example.test"));
  assert(nvbackend_definir_url("") == 1);
  puts("nvbackend: ok");
  return 0;
}
