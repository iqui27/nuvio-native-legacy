#include "tirarremoto.h"
#include "trakt.h"
#include "syncprog.h"

void tirarremoto_executar(const char *imdb, const char *chave, int ocultar) {
  trakt_playback_remover(imdb);
  // O "a seguir" vem do progresso da serie, nao do playback: esconde la tambem
  // (#203), senao outro aparelho o traz de volta. So o "Tirar de Continuar"
  // explicito faz isso; marcar como visto nao pode esconder a serie no Trakt
  // (sem volta local, e vale para os proximos episodios).
  if (ocultar) trakt_progresso_ocultar(imdb, 1);
  syncprog_remover(chave);
}
