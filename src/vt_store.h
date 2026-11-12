#ifndef NV_VT_STORE_H
#define NV_VT_STORE_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "catalogo.h"
#ifndef NV_VT_STORE_ALLOC
#define NV_VT_STORE_ALLOC malloc
#endif

/* Lossless sparse byte storage; limit includes each allocation's node header.
 * ponytail: linear lookup is bounded by the caller's byte budget; add an index
 * if a real view proves it too slow. This store is not yet used by the view. */
typedef struct VtStoreNode {
  struct VtStoreNode *next;
  size_t bytes;
  int raw;
  unsigned char data[];
} VtStoreNode;
typedef struct { VtStoreNode *first, *last; size_t used; int n; } VtStore;

static int vt_store_append(VtStore *s, const CatItem *item, size_t limit) {
  const unsigned char *p = (const unsigned char *)item;
  size_t len = sizeof *item, encoded = 0, i = 0;
  if (!s || !item || len > UINT16_MAX) return 0;
  while (i < len) {
    while (i < len && !p[i]) i++;
    size_t start = i;
    while (i < len && p[i]) i++;
    if (i > start) encoded += 4 + i - start;
  }
  int raw = encoded >= len;
  size_t bytes = raw ? len : encoded;
  if (bytes > SIZE_MAX - sizeof(VtStoreNode) || s->used > limit ||
      sizeof(VtStoreNode) + bytes > limit - s->used) return 0;
  VtStoreNode *node = NV_VT_STORE_ALLOC(sizeof *node + bytes);
  if (!node) return 0;
  node->next = NULL; node->bytes = bytes; node->raw = raw;
  if (raw) memcpy(node->data, p, len);
  else {
    size_t pos = 0; i = 0;
    while (i < len) {
      while (i < len && !p[i]) i++;
      size_t start = i;
      while (i < len && p[i]) i++;
      if (i > start) {
        uint16_t off = (uint16_t)start, n = (uint16_t)(i - start);
        memcpy(node->data + pos, &off, 2); memcpy(node->data + pos + 2, &n, 2);
        memcpy(node->data + pos + 4, p + start, n); pos += 4 + n;
      }
    }
  }
  if (s->last) s->last->next = node; else s->first = node;
  s->last = node; s->used += sizeof *node + bytes; s->n++;
  return 1;
}
static int vt_store_get(const VtStore *s, int index, CatItem *out) {
  if (!s || !out || index < 0 || index >= s->n) return 0;
  VtStoreNode *node = s->first;
  while (index--) node = node->next;
  if (node->raw) { memcpy(out, node->data, sizeof *out); return 1; }
  memset(out, 0, sizeof *out);
  for (size_t pos = 0; pos < node->bytes;) {
    uint16_t off, n;
    memcpy(&off, node->data + pos, 2); memcpy(&n, node->data + pos + 2, 2);
    memcpy((unsigned char *)out + off, node->data + pos + 4, n);
    pos += 4 + n;
  }
  return 1;
}
static void vt_store_free(VtStore *s) {
  VtStoreNode *p = s->first;
  while (p) { VtStoreNode *next = p->next; free(p); p = next; }
  memset(s, 0, sizeof *s);
}
#endif
