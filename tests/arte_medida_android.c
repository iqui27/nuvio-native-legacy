// src/arte_medida.c built with NV_ANDROID against tests/stub_android: the
// debug.nuvio.artdraw property path, with a fake __system_property_get.
#include "arte_medida.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/system_properties.h>
#define OK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

static const char *prop;   // NULL = property not set
static int lidas;
int __system_property_get(const char *name, char *value) {
  lidas++;
  OK(!strcmp(name, "debug.nuvio.artdraw"));
  value[0] = 0;
  if (!prop) return 0;
  snprintf(value, PROP_VALUE_MAX, "%s", prop);
  return (int)strlen(value);
}

int main(void) {
  unsetenv("NV_ART_DRAW_MEASURE");
  prop = NULL;  OK(arte_medida_resolver() == 0);
  prop = "0";   OK(arte_medida_resolver() == 0);
  prop = "";    OK(arte_medida_resolver() == 0);
  prop = "11";  OK(arte_medida_resolver() == 0);
  prop = "true"; OK(arte_medida_resolver() == 0);
  prop = "1";   OK(arte_medida_resolver() == 1);
  OK(lidas == 6);   // one read per resolve, never per draw
  prop = NULL; setenv("NV_ART_DRAW_MEASURE", "1", 1);
  OK(arte_medida_resolver() == 1);   // generic env switch still works
  printf("arte_medida_android OK\n");
  return 0;
}
