#include "dts_tv.h"
#include <stdio.h>
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
int main(void) {
  /* C9 and older (webOS < 5) keep the native path; 2020+ always converts, since
     support (back on some 2023/24 models) can't be detected. Unknown keeps the old path. */
  CHECK(nv_dts_tv_decodifica(4)); CHECK(nv_dts_tv_decodifica(0)); CHECK(nv_dts_tv_decodifica(-1));
  CHECK(!nv_dts_converter_ja(4, 1, 1, 0)); CHECK(!nv_dts_converter_ja(0, 1, 1, 0));
  /* 2020+ LG (5, 6, 10, 11, webOS 26, marketing numbers): no DTS, convert now. */
  CHECK(!nv_dts_tv_decodifica(5)); CHECK(!nv_dts_tv_decodifica(6));
  CHECK(!nv_dts_tv_decodifica(10)); CHECK(!nv_dts_tv_decodifica(11));
  CHECK(!nv_dts_tv_decodifica(26)); CHECK(!nv_dts_tv_decodifica(99));
  CHECK(nv_dts_converter_ja(5, 1, 1, 0)); CHECK(nv_dts_converter_ja(26, 1, 1, 0));
  /* Not a DTS track, already tried, or no converter: do not start one. */
  CHECK(!nv_dts_converter_ja(26, 0, 1, 0));
  CHECK(!nv_dts_converter_ja(26, 1, 1, 1));
  CHECK(!nv_dts_converter_ja(26, 1, 0, 0));
  /* No DTS and no converter: say so instead of playing silent. */
  CHECK(nv_dts_sem_som(26, 1, 0)); CHECK(!nv_dts_sem_som(26, 1, 1));
  /* tv.model.edidType: what the TV declares. */
  CHECK(nv_dts_edid("{\"configs\":{\"tv.model.edidType\":\"TrueHD+dts\"},\"returnValue\":true}") == 1);
  CHECK(nv_dts_edid("{\"tv.model.edidType\" : \"AC3+DTS\"}") == 1);
  CHECK(nv_dts_edid("{\"configs\":{\"tv.model.edidType\":\"TrueHD\"},\"x\":\"dts\"}") == 0);
  CHECK(nv_dts_edid("{\"configs\":{\"tv.model.edidType\":\"\"}}") == 0);
  CHECK(nv_dts_edid("{\"returnValue\":false,\"errorText\":\"Not permitted to send to com.webos.service.config\"}") == -1);
  CHECK(nv_dts_edid("{\"tv.model.edidType\":null}") == -1);
  CHECK(nv_dts_edid(NULL) == -1);
  { char v[48];
    CHECK(nv_dts_edid_valor("{\"configs\":{\"tv.model.edidType\":\"TrueHD+dts\"}}", v, sizeof v) && !strcmp(v, "TrueHD+dts"));
    CHECK(nv_dts_edid_valor("{\"tv.model.edidType\":\"a\\\"b<c> d\"}", v, sizeof v) && !strcmp(v, "a"));
    CHECK(nv_dts_edid_valor("{\"tv.model.edidType\":\"\"}", v, sizeof v) && !v[0]);
    CHECK(!nv_dts_edid_valor("{\"returnValue\":false}", v, sizeof v) && !v[0]);
    CHECK(nv_dts_edid_erro("{\"returnValue\":false,\"errorCode\":-1}") == -1);
    CHECK(nv_dts_edid_erro("{\"returnValue\":true}") == 0); }
  CHECK(!nv_dts_sem_som(26, 0, 0)); CHECK(!nv_dts_sem_som(4, 1, 0)); CHECK(!nv_dts_sem_som(0, 1, 0));
  if (!fails) puts("dts_tv ok");
  return fails != 0;
}
