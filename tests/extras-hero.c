#include "../src/extras.h"
#include <stdio.h>
#include <string.h>

static int check(const char *nome, int ok) {
  if (!ok) { fprintf(stderr, "FALHOU: %s\n", nome); return 1; }
  printf("ok %s\n", nome);
  return 0;
}

int main(void) {
  const char *json =
    "{\"results\":["
      "{\"site\":\"Vimeo\",\"type\":\"Trailer\",\"key\":\"bad-vimeo\"},"
      "{\"site\":\"YouTube\",\"type\":\"Behind the Scenes\",\"key\":\"badtype\"},"
      "{\"site\":\"YouTube\",\"type\":\"Trailer\",\"key\":\"AbC_123-xYz\"},"
      "{\"site\":\"YouTube\",\"type\":\"Teaser\",\"key\":\"bad/key\"},"
      "{\"site\":\"YouTube\",\"type\":\"Teaser\",\"key\":\"SecondValid\"}"
    "]}";
  char id[32] = "stale";
  int rc = 0;
  rc |= check("parser filtra site e tipo e preserva ordem", 
              extras_hero_trailer_parse(json, id, sizeof id) == 2 &&
              !strcmp(id, "AbC_123-xYz"));
  strcpy(id, "stale");
  rc |= check("parser rejeita resposta sem video elegivel",
              extras_hero_trailer_parse("{\"results\":[{\"site\":\"Vimeo\",\"type\":\"Trailer\",\"key\":\"abc123\"}]}",
                                        id, sizeof id) == 0 && !id[0]);
  rc |= check("parser tolera corpo ausente", extras_hero_trailer_parse(NULL, id, sizeof id) == 0 && !id[0]);
  puts(rc ? "extras-hero: FALHOU" : "extras-hero: tudo ok");
  return rc ? 1 : 0;
}
