// #584: Automatic trailer quality starts at 720p on Samsung TVs with <= 2 GB.
#include "../src/perfiltv.h"
#include <stdio.h>
int main(void) {
  static const int alt[] = { 1080, 720, 480, 360 };
  int bad = 0;
#define CHECK(c) do { if (!(c)) { printf("FALHOU: %s\n", #c); bad = 1; } } while (0)
  CHECK(ptv_trailer_indice(PTV_TPK, 1500, 0, alt, 4) == 1);     // Auto, low RAM -> 720
  CHECK(ptv_trailer_indice(PTV_TPK, 4096, 0, alt, 4) == 0);     // Auto, big RAM -> 1080
  CHECK(ptv_trailer_indice(PTV_TPK, 0, 0, alt, 4) == 0);        // RAM unknown -> 1080
  CHECK(ptv_trailer_indice(PTV_LG, 1500, 0, alt, 4) == 0);      // LG untouched
  CHECK(ptv_trailer_indice(PTV_TPK, 1500, 1080, alt, 4) == 0);  // explicit 1080 wins
  CHECK(ptv_trailer_indice(PTV_TPK, 1500, 480, alt, 4) == 2);
  CHECK(ptv_trailer_indice(PTV_TPK, 1500, 0, alt + 3, 1) == 0); // only 360
  CHECK(ptv_trailer_indice_duro(PTV_ANDROID, 1500, 720, alt, 4) == 1);
  CHECK(ptv_trailer_indice_duro(PTV_ANDROID, 1500, 720, alt, 1) == -1);  // only 1080: rejected
  if (!bad) printf("trailerimdb_auto: tudo ok\n");
  return bad;
}
