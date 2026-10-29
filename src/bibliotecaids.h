// Title IDs are canonicalized once per rebuild; counters reuse integer slots.
#ifndef NV_BIBLIOTECAIDS_H
#define NV_BIBLIOTECAIDS_H
#define BIB_ID_SLOTS (CAT_MAX * 4 + 1)
static struct { char id[64]; unsigned seen, addons; } bibIds[BIB_ID_SLOTS];
static int bibSlot[CAT_MAX];
static unsigned bibSeen;
static unsigned bibIdSlot(const char *id) {
  char key[64]; salvos_id_titulo(id, key, sizeof key);
  unsigned h = 2166136261u;
  for (const unsigned char *p = (const unsigned char *)key; *p; p++) h = (h ^ *p) * 16777619u;
  unsigned slot = h % BIB_ID_SLOTS;
  while (bibIds[slot].id[0] && strcmp(bibIds[slot].id, key)) slot = (slot + 1) % BIB_ID_SLOTS;
  return slot;
}
static void bibIdsMontar(int n) {
  memset(bibIds, 0, sizeof bibIds); bibSeen = 0;
  for (int i = 0; i < n; i++) {
    const CatItem *c = cat_item(i);
    bibSlot[i] = -1;
    if (!c || !c->imdb[0]) continue;
    unsigned slot = bibIdSlot(c->imdb);
    salvos_id_titulo(c->imdb, bibIds[slot].id, sizeof bibIds[slot].id);
    bibSlot[i] = (int)slot;
  }
}
static void bibIdsLimpar(void) { nContados = 0; ++bibSeen; }
static int bibIdJaContado(int i) {
  int slot = bibSlot[i];
  if (slot < 0) return 0;
  if (bibIds[slot].seen == bibSeen) return 1;
  bibIds[slot].seen = bibSeen;
  return 0;
}
static int bibIdNoCatalogo(const char *id) { return id && *id && bibIds[bibIdSlot(id)].id[0]; }
#endif
