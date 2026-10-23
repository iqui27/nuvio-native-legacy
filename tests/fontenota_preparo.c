// Exercise the real preparation callback and fingerprint without resolving URLs.
#include "../src/streams.c"
#include <assert.h>
#include <unistd.h>

static int failures;
#define CHECK(ok, label) do { \
  printf("%s: %s\n", (ok) ? "PASS" : "FAIL", label); \
  if (!(ok)) failures++; \
} while (0)

static void configure(int first, int priority) {
  char path[1024];
  snprintf(path, sizeof path, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
  FILE *f = fopen(path, "w"); assert(f);
  fprintf(f, "fonteAutoLocal %d\nfontePrioridadeLocal %d\nfonteIdiomaAudioLocal %d\n",
          first, priority, 1);
  fclose(f);
  ajustes_dir(getenv("NUVIO_DADOS"));
  assert(ajustes_fonte_primeira() == first);
  assert(ajustes_fonte_prioridade() == priority);
}
static void language(int enabled) {
  char path[1024];
  snprintf(path, sizeof path, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
  FILE *f = fopen(path, "w"); assert(f);
  fprintf(f, "fonteAutoLocal 0\nfontePrioridadeLocal 1\nfonteIdiomaAudioLocal %d\n",
          enabled ? 0 : 1);
  fclose(f);
  ajustes_dir(getenv("NUVIO_DADOS"));
  assert(ajustes_fonte_idioma_audio() == enabled);
}

static void memory(void) {
  Stream sources[2] = {0};
  strcpy(sources[0].rotulo, "1080p WEB-DL AAC");
  strcpy(sources[1].rotulo, "1080p BluRay REMUX FLAC");
  for (int i = 0; i < 2; i++) sources[i].altura = 1080;
  const int order[] = {0, 1};
  const signed char group[] = {0, 0};
  const unsigned char excluded[] = {0, 0};
  PreparoQualidade p = {sources, order, group, excluded, 2, 1, 0};
  configure(0, 1);
  language(1);
  CHECK(!preparoGrupoSemPerda(0, 0, &p), "P2: prepared memory rejects weaker WEB-DL");
  CHECK(preparoGrupoSemPerda(1, 0, &p), "prepared winner remains eligible");
  sources[0] = sources[1];
  CHECK(preparoGrupoSemPerda(0, 0, &p), "equal evidence preserves memory");
  strcpy(sources[0].rotulo, "1080p WEB-DL AAC");
  configure(0, 2);
  CHECK(preparoGrupoSemPerda(0, 0, &p), "start fast preserves memory");
  configure(1, 1);
  CHECK(preparoGrupoSemPerda(0, 0, &p), "first in list preserves memory");
}

static void fingerprint(void) {
  StreamPreparo prepared = {0};
  prepared.sessaoPerfil = nv_perfil_sessao_ler();
  language(0);
  ling_local_audio("en");
  prepared.configuracao = stream_preparo_config_atual();
  CHECK(stream_preparo_contexto_atual(&prepared), "unchanged preparation remains valid");
  language(1);
  CHECK(!stream_preparo_contexto_atual(&prepared), "P2: enabling language preference invalidates preparation");
  prepared.configuracao = stream_preparo_config_atual();
  ling_local_audio("ja");
  CHECK(!stream_preparo_contexto_atual(&prepared), "P2: changing effective audio language invalidates preparation");
  language(0);
  prepared.configuracao = stream_preparo_config_atual();
  ling_local_audio("en");
  CHECK(stream_preparo_contexto_atual(&prepared), "disabled preference ignores audio language changes");
}

int main(void) {
  assert(getenv("NUVIO_DADOS"));
  memory();
  fingerprint();
  return failures ? 1 : 0;
}
