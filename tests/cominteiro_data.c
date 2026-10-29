// Real extras parser and ownership: long, multiline reviews from both endpoints.
#define main ratings_fixture_main
#define rede_baixar_com_teste ratings_fixture_get
#include "notas0.c"
#undef rede_baixar_com_teste
#undef main
static char review[24000];
char *rede_baixar_com_teste(const char *url, int seg, const char *const *cab) {
  if (strstr(url, "/comments/likes")) {
    size_t cap = strlen(review) + 512;
    char *json = malloc(cap); assert(json);
    snprintf(json, cap, "[{\"comment\":\"%s\\n\\nEND\",\"user\":{\"username\":\"reviewer\"},\"created_at\":\"2026-10-12T12:00:00Z\",\"spoiler\":true,\"replies\":7,\"likes\":42,\"user_rating\":9,\"language\":\"en\"}]", review);
    return json;
  }
  return ratings_fixture_get(url, seg, cab);
}
int main(void) {
  for (int i = 0; i < 150; i++) strcat(review, "Full reviews must retain every paragraph and their final sentence. ");
  extras_pedir("tt12637874", 1, 0);
  for (int i = 0; i < 10000 && extras_carregando(); i++) usleep(1000);
  assert(!extras_carregando() && extras_n_comentarios() == 1);
  const char *text = extras_comentario_texto(0);
  assert(strlen(text) == strlen(review) + 5 && strstr(text, "\n\nEND"));
  assert(extras_comentario_spoiler(0, 0) && extras_comentario_respostas(0, 0) == 7);
  assert(!strcmp(extras_comentario_data(0, 0), "2026-10-12T12:00:00Z"));
  extras_pedir_comentarios_ep("tt12637874", 1, 1);
  for (int i = 0; i < 10000 && extras_comentarios_ep_carregando(); i++) usleep(1000);
  assert(!extras_comentarios_ep_carregando() && extras_n_comentarios_ep() == 1);
  assert(!strcmp(extras_comentario_ep_texto(0), extras_comentario_texto(0)));
  assert(extras_comentario_spoiler(1, 0) && extras_comentario_respostas(1, 0) == 7);
  assert(!strcmp(extras_comentario_data(1, 0), "2026-10-12T12:00:00Z"));
  extras_abrir_sessao(); assert(!extras_n_comentarios() && !extras_n_comentarios_ep());
  puts("PASS: full multiline JSON reviews and metadata survive title/episode parsing and reset");
  return 0;
}
