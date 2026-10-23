#include "audiomemoria.h"
#include "legmemoria.h"
#include "fontepref.h"
#include "salvosorg.h"

// Account-private device backup. Only the fixed allowlist can become a local
// filename; neither JSON property names nor a remote path reach filesystem APIs.
static int ajusteAparelho(int op) {
  switch (op) {
    case AJ_DV7: return 0;
    case AJ_VIDRO: case AJ_VIDRO_CONTORNO: case AJ_FUNDO:
    case AJ_VIDRO_OPAC: case AJ_VIDRO_FOSCO: case AJ_RAIL_BLUR:
    case AJ_PROVEDORES_ESCOPO: case AJ_AUDIO_OFFSET:
      return 1;
    default: return !ajustePortavel(op) && CHAVE[op] && CHAVE[op][0] != '-' &&
      (OPCOES[op].tipo == OP_ESCOLHA || OPCOES[op].tipo == OP_NUMERO);
  }
}

// JSON member iterator scoped to one object (unlike the recursive web reader).
static const char *ajJsonSpace(const char *p, const char *end) {
  while (p < end && (unsigned char)*p <= ' ') p++;
  return p;
}
static const char *ajJsonEnd(const char *p, const char *end) {
  if (p >= end) return NULL;
  if (*p == '{' || *p == '[') {
    const char *q = js_fim(p);
    return q > p && q <= end && q[-1] == (*p == '{' ? '}' : ']') ? q : NULL;
  }
  if (*p == '"') {
    for (const char *q = p + 1; q < end; q++) {
      if (*q == '"') return q + 1;
      if (*q == '\\') q++;
    }
    return NULL;
  }
  const char *q = p;
  while (q < end && !strchr(",}] \r\n\t", *q)) q++;
  return q > p ? q : NULL;
}
static int ajJsonNext(const char **cursor, const char *end, char *key, size_t cap,
                      const char **v, const char **f) {
  const char *p = ajJsonSpace(*cursor, end);
  if (p < end && (*p == '{' || *p == ',')) p = ajJsonSpace(p + 1, end);
  if (p >= end || *p != '"') return 0;
  const char *ke = ajJsonEnd(p, end);
  if (!ke) return 0;
  // Decode only this key: js_cadeia also strlen-scans the remaining document.
  char *quoted = strndup(p, (size_t)(ke - p));
  if (!quoted) return 0;
  int ok = js_cadeia(quoted, key, cap); free(quoted);
  if (!ok) return 0;
  p = ke;
  if (!p || (p = ajJsonSpace(p, end)) >= end || *p != ':') return 0;
  *v = ajJsonSpace(p + 1, end); *f = ajJsonEnd(*v, end);
  if (!*f) return 0;
  *cursor = *f; return 1;
}
static int ajJsonGet(const char *obj, const char *end, const char *key,
                     const char **v, const char **f) {
  if (!obj || *obj != '{') return 0;
  const char *p = obj; size_t cap = (size_t)(end - obj) + 1;
  char *k = malloc(cap); if (!k) return 0;
  while (ajJsonNext(&p, end, k, cap, v, f)) if (!strcmp(k, key)) { free(k); return 1; }
  free(k); return 0;
}

// Web options never live in C snapshots. Keep those potentially huge hex
// strings out of the repeated recursive web-key lookups on the main thread.
static char *ajustesWebBlob(const char *json) {
  const char *end = json + strlen(json), *p = json, *v, *f;
  char *key = malloc((size_t)(end - json) + 1);
  if (!key) return NULL;
  Jsw w; jsw_iniciar(&w); jsw_obj_ini(&w);
  while (ajJsonNext(&p, end, key, (size_t)(end - json) + 1, &v, &f)) {
    if (!strcmp(key, "c_settings_v1") || !strcmp(key, "c_devices_v1")) continue;
    jsw_chave(&w, key);
    char *raw = strndup(v, (size_t)(f - v));
    if (!raw) { free(key); jsw_livre(&w); return NULL; }
    jsw_bruto(&w, raw); free(raw);
  }
  free(key); jsw_obj_fim(&w);
  const char *s = jsw_texto_final(&w);
  char *out = s ? strdup(s) : NULL; jsw_livre(&w); return out;
}

// Overlay known fields recursively. Unknown fields, including other devices and
// future fields inside values/stores, remain byte-for-byte JSON values.
static char *ajJsonMerge(const char *base, const char *be, const char *patch, const char *pe) {
  Jsw w; jsw_iniciar(&w); jsw_obj_ini(&w);
  const char *p = base, *v, *f, *a, *b;
  size_t cap = (size_t)((be - base > pe - patch) ? be - base : pe - patch) + 1;
  char *key = malloc(cap); if (!key) { jsw_livre(&w); return NULL; }
  while (ajJsonNext(&p, be, key, cap, &v, &f)) {
    jsw_chave(&w, key);
    if (ajJsonGet(patch, pe, key, &a, &b)) {
      if (*v == '{' && *a == '{') {
        char *merged = ajJsonMerge(v, f, a, b);
        if (!merged) { free(key); jsw_livre(&w); return NULL; }
        jsw_bruto(&w, merged); free(merged);
      } else {
        char *raw = strndup(a, (size_t)(b - a));
        if (!raw) { free(key); jsw_livre(&w); return NULL; }
        jsw_bruto(&w, raw); free(raw);
      }
    } else {
      char *raw = strndup(v, (size_t)(f - v));
      if (!raw) { free(key); jsw_livre(&w); return NULL; }
      jsw_bruto(&w, raw); free(raw);
    }
  }
  p = patch;
  while (ajJsonNext(&p, pe, key, cap, &v, &f)) {
    if (ajJsonGet(base, be, key, &a, &b)) continue;
    jsw_chave(&w, key);
    char *raw = strndup(v, (size_t)(f - v));
    if (!raw) { free(key); jsw_livre(&w); return NULL; }
    jsw_bruto(&w, raw); free(raw);
  }
  jsw_obj_fim(&w); free(key);
  const char *s = jsw_texto_final(&w); char *out = s ? strdup(s) : NULL;
  jsw_livre(&w); return out;
}

#define AJ_DEVICE_FILES 33
static void ajusteDeviceFile(int k, char *name, size_t cap) {
  static const char *const fixed[] = {
    "player.txt", "player-externo.txt", "fanart.txt", "seekr.txt", "posteres.txt",
    "p2p.txt", "curadoria.txt", "debrid.txt"
  };
  static const char *const profile[] = {
    "fileirasui-p%d.txt", "ltv-fontes-p%d.txt", "ltv-urls-p%d.txt",
    "stalker-p%d.txt", "xtream-p%d.txt", "jellyfin-p%d.txt", "emby-p%d.txt",
    "plex-p%d.txt", "discord-p%d.txt", "selos-p%d.txt",
    "selos-p%d-0.json", "selos-p%d-1.json", "selos-p%d-2.json"
  };
  // Shared rules/filters are restored only from c_settings_v1, including when
  // an older client left whole-file copies in these device slots.
  if (k == 22 || k == 24 || k == 25) { name[0] = 0; return; }
  if (k < 8) snprintf(name, cap, "%s", fixed[k]);
  else if (k < 21) snprintf(name, cap, profile[k - 8], perfis_ativo());
  else if (k == 21) ajustes_provedores_arquivo(name, (unsigned)cap, "debrid");
  else if (k == 23) {
    unsigned long long h = 1469598103934665603ULL;
    for (const char *p = sessao_usuario(); *p; p++) { h ^= (unsigned char)*p; h *= 1099511628211ULL; }
    snprintf(name, cap, "plugins-%08llx-p%d.txt", h & 0xffffffffULL, perfis_ativo_addons());
  } else if (k == 26) debNomePerfil(name, (unsigned)cap, "provider-scope");
  else {
    static const char *const memory[] = {
      "audiomemoria-p%d.txt", "legmemoria-p%d.txt", "fontepref-p%d.txt",
      "fontepref-auto-p%d.txt", "listas-p%d.txt", "salvos-org-p%d.txt"
    };
    snprintf(name, cap, memory[k - 27], perfis_ativo());
  }
}
// Split the mixed row file on both save and restore, including legacy snapshots.
static void ajusteFileirasCampos(char *text, int device) {
  if (!text) return;
  char *out = text;
  for (char *p = text; *p;) {
    char *end = strchr(p, '\n');
    size_t n = end ? (size_t)(end + 1 - p) : strlen(p);
    int local = !strncmp(p, "limite ", 7) || !strncmp(p, "conta_auto ", 11);
    if (local == device) { memmove(out, p, n); out += n; }
    p += n;
  }
  *out = 0;
}
static char *ajusteFileirasMesclar(char *shared, char *device) {
  ajusteFileirasCampos(shared, 0); ajusteFileirasCampos(device, 1);
  size_t n = shared ? strlen(shared) : 0;
  size_t cap = n + (device ? strlen(device) : 0) + 2;
  char *out = malloc(cap);
  if (out) snprintf(out, cap, "%s%s%s", shared ? shared : "",
                    n && shared[n - 1] != '\n' ? "\n" : "", device ? device : "");
  return out;
}
static void ajusteHex(Jsw *w, const char *key, const char *text) {
  if (!text) { jsw_chave(w, key); jsw_bruto(w, "null"); return; }
  size_t n = strlen(text); char *hex = malloc(n * 2 + 1);
  if (!hex) { w->erro = 1; return; }
  static const char digits[] = "0123456789abcdef";
  for (size_t i = 0; i < n; i++) {
    unsigned byte = (unsigned char)text[i];
    hex[i * 2] = digits[byte >> 4]; hex[i * 2 + 1] = digits[byte & 15];
  }
  hex[n * 2] = 0; jsw_cs(w, key, hex); free(hex);
}
static char *ajusteUnhex(const char *v, const char *f) {
  if (f - v < 2 || *v != '"' || f[-1] != '"') return NULL;
  size_t n = (size_t)(f - v - 2);
  if (n % 2) return NULL;
  char *text = malloc(n / 2 + 1); if (!text) return NULL;
  for (size_t i = 0; i < n; i += 2) {
    if (!strchr("0123456789abcdef", v[1 + i]) || !strchr("0123456789abcdef", v[2 + i])) { free(text); return NULL; }
    // sscanf can scan the entire remaining string even with a field width.
    // Decode exactly two digits: linear time on TV libc as well as the host.
    unsigned hi = v[1 + i] <= '9' ? v[1 + i] - '0' : v[1 + i] - 'a' + 10;
    unsigned lo = v[2 + i] <= '9' ? v[2 + i] - '0' : v[2 + i] - 'a' + 10;
    unsigned byte = hi * 16 + lo;
    if (!byte) { free(text); return NULL; }
    text[i / 2] = (char)byte;
  }
  text[n / 2] = 0; return text;
}
static char *ajustesDeviceSnapshot(void) {
  Jsw w; jsw_iniciar(&w); jsw_obj_ini(&w);
  jsw_chave(&w, "values"); jsw_obj_ini(&w);
  for (int i = 0; i < AJ_N; i++) if (ajusteAparelho(i)) {
    char key[96]; snprintf(key, sizeof key, "c_%s", CHAVE[i]); jsw_ci(&w, key, valor[i]);
  }
  jsw_ci(&w, "c_provider_scope", valor[AJ_PROVEDORES_ESCOPO]);
  jsw_ci(&w, "c_memory_chosen", memoriaEscolhida);
  jsw_ci(&w, "c_library_chosen", bibliotecaFonteEscolhida);
  jsw_ci(&w, "c_language_source", idiomaFonteGravada);
  jsw_obj_fim(&w);
  char order[EX_NFONTES * 8] = ""; size_t n = 0;
  for (int i = 0; i < EX_NFONTES; i++) n += (size_t)snprintf(order + n, sizeof order - n, "%d ", ajustes_nota_fonte(i));
  jsw_cs(&w, "rating_order", order);
  jsw_chave(&w, "files"); jsw_obj_ini(&w);
  for (int i = 0; i < AJ_DEVICE_FILES; i++) {
    char name[128] = "", key[24]; ajusteDeviceFile(i, name, sizeof name);
    char *s = i == 26 ? strdup(valor[AJ_PROVEDORES_ESCOPO] ? "1\n" : "0\n") :
              (*name ? dados_ler(name) : NULL);
    if (i == 8) ajusteFileirasCampos(s, 1);
    snprintf(key, sizeof key, "file%d", i); ajusteHex(&w, key, s); free(s);
  }
  jsw_obj_fim(&w); jsw_obj_fim(&w);
  const char *s = jsw_texto_final(&w); char *out = s ? strdup(s) : NULL;
  jsw_livre(&w); return out;
}

static void ajustesSnapshotAtual(void) {
  // These values already use today's schema. A fresh install has not passed
  // through the legacy-file migrations; do not run those over restored choices.
  static const char *const markers[] = {
    RES_ARQ_MIGRADA, "envio-151.txt", "trailer-1310.txt",
    "ajustesescala-203.txt", "ajustesescala-205.txt", "perfilfundo-20.txt",
    "aparencia-20.txt", "profundidade-205.txt", "idiomas-206.txt",
    "memoriaimagens-205.txt", "efeitos720-205.txt", "ampliarUi-206.txt"
  };
  for (size_t i = 0; i < sizeof markers / sizeof *markers; i++) {
    char *old = dados_ler(markers[i]);
    if (!old || strcmp(old, "1\n")) dados_gravar(markers[i], "1\n");
    free(old);
  }
}

static int ajustesDeviceAplicar(const char *json) {
  const char *devices, *de, *dev, *end, *values, *ve, *v, *f;
  const char *id = dados_aparelho_id();
  if (!*id || !ajJsonGet(json, json + strlen(json), "c_devices_v1", &devices, &de) ||
      !ajJsonGet(devices, de, id, &dev, &end)) return 0;
  int changed = 0;
  ajustesSnapshotAtual();
  if (ajJsonGet(dev, end, "values", &values, &ve)) {
    for (int i = 0; i < AJ_N; i++) if (ajusteAparelho(i)) {
      char key[96]; snprintf(key, sizeof key, "c_%s", CHAVE[i]);
      if (!ajJsonGet(values, ve, key, &v, &f)) continue;
      char *tail; long num = strtol(v, &tail, 10);
      if (tail != f || num < INT_MIN || num > INT_MAX) continue;
      int next = limita(i, (int)num);
      if (next != valor[i]) { valor[i] = next; changed++; }
    }
    struct { const char *key; int *value; int min, max; } extra[] = {
      {"c_provider_scope", &valor[AJ_PROVEDORES_ESCOPO], 0, 1},
      {"c_memory_chosen", &memoriaEscolhida, 0, 1},
      {"c_library_chosen", &bibliotecaFonteEscolhida, 0, 1},
      {"c_language_source", &idiomaFonteGravada, IDA_TMDB, IDA_PADRAO}
    };
    for (size_t i = 0; i < sizeof extra / sizeof *extra; i++) {
      if (!ajJsonGet(values, ve, extra[i].key, &v, &f)) continue;
      char *tail; long n = strtol(v, &tail, 10);
      if (tail == f && n >= extra[i].min && n <= extra[i].max && *extra[i].value != (int)n) {
        *extra[i].value = (int)n; changed++;
      }
    }
  }
  char order[EX_NFONTES * 8];
  if (js_texto(dev, end, "rating_order", order, sizeof order)) {
    int seen[EX_NFONTES] = {0}, next[EX_NFONTES], i; char *p = order;
    for (i = 0; i < EX_NFONTES; i++) {
      char *tail; long n = strtol(p, &tail, 10);
      if (tail == p || n < 0 || n >= EX_NFONTES || seen[n]) break;
      seen[n] = 1; next[i] = (int)n; p = tail;
    }
    if (i == EX_NFONTES && memcmp(notaFontes, next, sizeof next)) {
      memcpy(notaFontes, next, sizeof next); changed++;
    }
  }
  if (ajJsonGet(dev, end, "files", &values, &ve)) {
    for (int i = 0; i < AJ_DEVICE_FILES; i++) {
      char key[24], name[128] = "";
      snprintf(key, sizeof key, "file%d", i);
      if (!ajJsonGet(values, ve, key, &v, &f)) continue;
      ajusteDeviceFile(i, name, sizeof name); if (!*name) continue;
      if (f - v == 4 && !strncmp(v, "null", 4)) {
        char *old = dados_ler(name);
        if (i != 8 && old) { dados_apagar(name); changed++; }
        free(old); continue;
      }
      char *text = ajusteUnhex(v, f); if (!text) continue;
      if (i == 8) {
        char *shared = dados_ler(name), *merged = ajusteFileirasMesclar(shared, text);
        free(shared); free(text); text = merged;
        if (!text) continue;
      }
      char *old = dados_ler(name);
      if ((!old || strcmp(old, text)) && debGravarArquivo(name, text)) changed++;
      free(old);
      free(text);
    }
  }
  // Reload only when something changed; unchanged snapshots need no side effects.
  if (!changed) return 0;
  player_dir(dados_dir()); player_leg_estilo_carregar();
  ppCarregar(); fanartCarregar(); seekrCarregar(); p2pCarregar(); pstCarregar();
  ajustes_provedores_abrir_sessao();
  fil_recarregar(); fonteregra_carregar(); fontefalha_config_ler();
  ltv_pref_carregar(perfis_ativo()); ltv_urls_carregar(perfis_ativo());
  stalker_carregar(); xtream_carregar(); jellyfin_carregar(); emby_carregar(); plex_carregar();
  plugins_iniciar(); selospacote_recarregar();
  // These readers cache even an empty profile. Invalidate before loading the
  // files just restored for the active profile.
  audiomem_definir_perfil(0); audiomem_definir_perfil(perfis_ativo());
  legmem_definir_perfil(0); legmem_definir_perfil(perfis_ativo());
  fontepref_definir_perfil(0); fontepref_definir_perfil(perfis_ativo());
  lst_iniciar(); sorg_recarregar(); discord_recarregar();
  char *curated = dados_ler("curadoria.txt");
  if (curated) { curadoria_definir_url(curated); free(curated); }
  gfx_escala_ui_definir(ajustes_tamanho_ui()); gfx_escala_txt_definir(ajustes_escala_texto());
  interfaceAplicar();
  if (changed) gravar();
  return changed;
}
