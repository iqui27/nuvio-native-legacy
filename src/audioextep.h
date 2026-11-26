#ifndef NV_AUDIOEXTEP_H
#define NV_AUDIOEXTEP_H
// "From another source": the source list of ANOTHER episode of the series
// (owner's log 21/11, Farscape: the 4K release numbers the episodes in
// production order, the Polish ones in broadcast order, so episode 2's audio
// is in episode 3's list). Asked of the add-ons on a worker of its own,
// never installed as the player's list. Every call is from the UI thread.
#include "streams.h"

enum { AEXTEP_NADA = 0, AEXTEP_BUSCANDO, AEXTEP_PRONTA, AEXTEP_FALHOU };
// The sources of `alvo` ("tt123:1:3"). The same target already on hand or on
// its way: nothing new. NULL or "" drops the list.
void audioextep_pedir(const char *alvo);
// AEXTEP_*; also takes in a list that came back (pointers from
// audioextep_item stay valid until the next audioextep_* call that returns
// AEXTEP_PRONTA for another list, or audioextep_pedir).
int audioextep_estado(void);
const char *audioextep_alvo(void);
int audioextep_n(void);
const Stream *audioextep_item(int i);
// Changes whenever another list is taken in.
unsigned audioextep_geracao(void);
#endif
