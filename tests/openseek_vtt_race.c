#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static int test_unlock(pthread_mutex_t *m);
#define pthread_mutex_unlock test_unlock
#include "../src/openseek.c"
#undef pthread_mutex_unlock

static int replace_on_unlock;
static SeekrVtt replacement;
static int test_unlock(pthread_mutex_t *m) {
  int r = pthread_mutex_unlock(m);
  if (replace_on_unlock && m == &trava) {
    replace_on_unlock = 0;
    pthread_mutex_lock(&trava);
    trocarVtt(atual, &replacement);
    pthread_mutex_unlock(&trava);
  }
  return r;
}
int perftv_enabled, perftv_uploads;
void gfx_tex_esquecer(GLuint unidade) { (void)unidade; }
static SeekrVtt make_vtt(int count) {
  SeekrVtt v = {0};
  v.nCues = count; v.cues = calloc((size_t)count, sizeof *v.cues);
  v.nFolhas = 1; v.folhas = calloc(1, sizeof *v.folhas);
  v.folhas[0] = strdup("https://example.com/image.jpg");
  for (int j = 0; j < count; j++) {
    v.cues[j].ini = j * 1000; v.cues[j].fim = (j + 1) * 1000;
    v.cues[j].rotulo = -1; v.cues[j].x = j; v.cues[j].w = v.cues[j].h = 1;
  }
  return v;
}
int main(void) {
  Sessao s = {0}; GLuint out = 0;
  atual = &s; estado = SEEKR_PRONTO; s.ultimoQuadrosMs = -1;
  SeekrVtt original = make_vtt(8);
  trocarVtt(&s, &original);
  replacement = make_vtt(1);
  prontos[0].px = malloc(4); prontos[0].w = prontos[0].h = 1;
  prontos[0].g = geracao; strcpy(prontos[0].chaveTex, "pending");
  texG = geracao;
  openseek_lru_ini(&texLru, OS_TEX_LRU);
  for (int j = 0; j < texLru.n; j++) {
    texLru.it[j].ocupado = 1;
    strcpy(texLru.it[j].chave, "occupied");
  }
  replace_on_unlock = 1;
  assert(pQuadros(7.5, 1, &out, NULL) == 0);
  assert(!replace_on_unlock && !out && !s.nQuero);
  soltarVtt();
  return 0;
}
