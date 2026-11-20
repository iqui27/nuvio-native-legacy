// #458: a dedup por copia acontece DEPOIS dos chips em "Todas": uma copia so
// fica escondida quando outra do MESMO grupo passa nos filtros ativos. Com
// chips desligados o grupo vira 1 linha; com "So MP4" a unica copia MP4 fica.
#include "streams.h"
#include <assert.h>
#include <string.h>
const char *i18n(const char *s) { return s; }
void wsenvio_podar(void) {}
#include "../src/streams.c"

static void fonte(Stream *s, const char *hash, const char *prov, int mp4, const char *rot) {
  memset(s, 0, sizeof *s);
  if (hash) snprintf(s->infoHash, sizeof s->infoHash, "%s", hash);
  snprintf(s->provedor, sizeof s->provedor, "%s", prov);
  snprintf(s->rotulo, sizeof s->rotulo, "%s", rot);
  snprintf(s->url, sizeof s->url, "https://cdn.example/%s.mp4", rot);
  s->mp4 = mp4;
}

int main(void) {
  Stream s[4];
  // A e B: mesmo hash = copias do mesmo arquivo, de addons diferentes.
  // So B e MP4. C e outra fonte qualquer.
  fonte(&s[0], "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "A", 0, "x");
  fonte(&s[1], "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "B", 1, "x");
  fonte(&s[2], "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", "A", 1, "y");
  stream_definir_lista(s, 3);
  assert(vt_copias_mesmas(0, 1));
  assert(vt_copia_rep(1) == 0 && vt_copia_rep(0) == 0);

  // "Todas" (filtro=0), chips desligados: a dedup agrupa as copias -> so a
  // primeira da ORDEM aparece; a copia B fica escondida atras do contador.
  filtro = 0; filtroServidor = -1; soMp4 = soCache = soDub = 0;
  assert(passaFiltro(0) == 1);
  assert(passaFiltro(1) == 0);   // copia escondida: o representante A passa
  assert(passaFiltro(2) == 1);

  // "So MP4": A nao e MP4. B e a UNICA copia elegivel do grupo -> TEM de
  // aparecer (sem esta regra o chip escondia a unica fonte MP4 do grupo).
  soMp4 = 1;
  assert(passaFiltro(0) == 0);
  assert(passaFiltro(1) == 1);   // a copia MP4 aparece: ninguem do grupo passa
  assert(passaFiltro(2) == 1);

  // Dois MP4 irmaos: a dedup continua valendo entre os que passam nos chips.
  fonte(&s[3], "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "C", 1, "x");
  stream_definir_lista(s, 4);
  assert(passaFiltro(0) == 0);            // nao MP4
  assert(passaFiltro(1) == 1);            // primeiro MP4 do grupo na ordem
  assert(passaFiltro(3) == 0);            // segundo MP4 do grupo: dedup
  assert(passaFiltro(2) == 1);
  soMp4 = 0;
  assert(passaFiltro(0) == 1);
  assert(passaFiltro(1) == 0);
  assert(passaFiltro(3) == 0);

  // Aba de um addon: a copia so aparece na aba do addon DONO dela; na aba do
  // representante a copia nao mora (e na do dono ela nao e "copia" — a dedup
  // entre addons so reina em "Todas").
  nProvedores = 3;
  snprintf(provedores[1], sizeof provedores[1], "A");
  snprintf(provedores[2], sizeof provedores[2], "B");
  filtro = 1;   // aba do addon A
  assert(passaFiltro(0) == 1);   // representante na propria aba
  assert(passaFiltro(1) == 0);   // copia de outro addon nao aparece aqui
  assert(passaFiltro(2) == 1);
  filtro = 2;   // aba do addon B
  assert(passaFiltro(0) == 0);
  assert(passaFiltro(1) == 1);   // na aba dela, a copia e a linha do addon B
  filtro = 0;

  puts("d1_fontes_duplicadas: ok");
  return 0;
}
