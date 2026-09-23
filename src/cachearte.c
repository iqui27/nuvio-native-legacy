#include "cachearte.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/threading.h>
#include <errno.h>
#include <stdio.h>

#define NV_CACHE_ARTE_DB "nuvio-art-v1"
#define NV_CACHE_ARTE_MAX_DEFAULT (48L * 1024L * 1024L)
#define NV_CACHE_ARTE_MAX_ENTRY (8L * 1024L * 1024L)
#define NV_CACHE_ARTE_MAX_QUEUED (16L * 1024L * 1024L)

typedef struct { int32_t state, ptr, n; } ArteJob;
static _Atomic long s_hits, s_misses, s_writes, s_errors, s_queued;
static _Atomic long s_inventory[4];
static long s_limit = NV_CACHE_ARTE_MAX_DEFAULT;
static _Atomic int s_started;

/* Called only after an IndexedDB write transaction settles. The C-side byte
 * budget includes queued buffers until this point, bounding both the bridge
 * and browser transaction backlog. */
EMSCRIPTEN_KEEPALIVE void nv_cachearte_write_done(int n, int ok) {
  atomic_fetch_sub(&s_queued, n);
  if (ok) atomic_fetch_add(&s_writes, 1);
  else atomic_fetch_add(&s_errors, 1);
}
EMSCRIPTEN_KEEPALIVE void nv_cachearte_inventory_done(int items, int bytes, int essential,
                                                      int expected) {
  atomic_store(&s_inventory[0], items);
  atomic_store(&s_inventory[1], bytes);
  atomic_store(&s_inventory[2], essential);
  atomic_store(&s_inventory[3], expected);
}

static void js_init(void) {
  MAIN_THREAD_ASYNC_EM_ASM({
    var A = Module.nvArtCache || (Module.nvArtCache = {});
    A.queue = A.queue || [];
    A.running = !!A.running;
    A.pins = A.pins || Object.create(null);
    A.limit = A.limit || 50331648;
    A.key = function (url, variant) { return url + "\\n" + variant; };
    A.flags = function (groups, fallbackEssential, fallbackUse) {
      var essential = !!fallbackEssential; var inUse = !!fallbackUse;
      groups = groups || {};
      Object.keys(groups).forEach(function (g) {
        essential = essential || !!groups[g].essential;
        inUse = inUse || !!groups[g].inUse;
      });
      var f = {}; f.essential = essential; f.inUse = inUse; return f;
    };
    A.finish = function (job, state, ptr, n) {
      HEAP32[(job >> 2) + 1] = ptr | 0;
      HEAP32[(job >> 2) + 2] = n | 0;
      if (Atomics.compareExchange(HEAP32, job >> 2, 0, state | 0) === 0) {
        Atomics.notify(HEAP32, job >> 2);
        return true;
      }
      if (ptr) Module._free(ptr);
      if (Atomics.load(HEAP32, job >> 2) === 3) Module._free(job);
      return false;
    };
    A.valid = function (b) {
      if (!b || b.byteLength <= 512) return false;
      var u = new Uint8Array(b, 0, Math.min(12, b.byteLength));
      return (u[0] === 255 && u[1] === 216) ||
        (u[0] === 137 && u[1] === 80 && u[2] === 78 && u[3] === 71) ||
        (u[0] === 71 && u[1] === 73 && u[2] === 70) ||
        (u[0] === 82 && u[1] === 73 && u[2] === 70 && u[3] === 70 &&
         u[8] === 87 && u[9] === 69 && u[10] === 66 && u[11] === 80);
    };
    A.run = function () {
      if (A.running || !A.queue.length || (!A.db && A.opening)) return;
      A.running = true;
      var op = A.queue.shift(); var ended = false;
      var next = function () {
        if (ended) return;
        ended = true; A.running = false; A.run();
      };
      try { op(A.db || null, next); } catch (e) { next(); }
    };
    A.open = function () {
      if (A.db) { A.run(); return; }
      if (A.opening) return;
      A.opening = true;
      setTimeout(function () {
        if (!A.opening) return;
        A.opening = false; A.openFailed = true; A.run();
      }, 1500);
      var r;
      try { r = indexedDB.open("nuvio-art-v1", 1); }
      catch (e) { A.opening = false; A.openFailed = true; A.run(); return; }
      r.onupgradeneeded = function () {
        var db = r.result;
        if (!db.objectStoreNames.contains("meta")) db.createObjectStore("meta", { keyPath: "key" });
        if (!db.objectStoreNames.contains("blob")) db.createObjectStore("blob", { keyPath: "key" });
      };
      r.onerror = function () { A.opening = false; A.openFailed = true; A.run(); };
      r.onsuccess = function () {
        A.opening = false; A.openFailed = false; A.db = r.result;
        A.db.onversionchange = function () { A.db.close(); A.db = null; };
        A.run();
      };
    };
    A.enqueue = function (op) { A.queue.push(op); A.open(); };
    A.read = function (url, variant, job) {
      A.enqueue(function (db, done) {
        if (Atomics.load(HEAP32, job >> 2) === 3) { Module._free(job); done(); return; }
        if (!db) { A.finish(job, 1, 0, 0); done(); return; }
        var key = A.key(url, variant); var settled = false;
        var pendingPtr = 0, pendingN = 0;
        var end = function (state) {
          if (settled) return;
          settled = true;
          /* This operation owns the buffer until transaction commit. */
          var ptr = pendingPtr, n = pendingN;
          pendingPtr = 0; pendingN = 0;
          if (state !== 2 && ptr) { Module._free(ptr); ptr = 0; n = 0; }
          A.finish(job, state, ptr, n); done();
        };
        var tx;
        try {
          tx = db.transaction(["meta", "blob"], "readwrite");
          var bs = tx.objectStore("blob"); var ms = tx.objectStore("meta");
          var g = bs.get(key); var gm = ms.get(key);
          g.onerror = gm.onerror = function () { end(1); };
          g.onsuccess = function () { gm.onsuccess = function () {
            var b = g.result; var existing = gm.result; var data = b && b.bytes;
            if (!data || !A.valid(data)) {
              ms.delete(key); bs.delete(key);
              tx.oncomplete = function () { end(1); };
              tx.onerror = tx.onabort = function () { end(4); };
              return;
            }
            try { pendingPtr = Module._malloc(data.byteLength); } catch (e) {}
            if (!pendingPtr) { end(1); return; }
            pendingN = data.byteLength;
            HEAPU8.set(new Uint8Array(data), pendingPtr);
            var pin = A.pins[key] || {};
            var meta = existing || {};
            var scopes = meta.scopes || {};
            Object.keys(pin.groups || {}).forEach(function (g) { scopes[g] = pin.groups[g]; });
            var flags = A.flags(scopes, false, false);
            meta.key = key; meta.url = url; meta.variant = variant; meta.bytes = data.byteLength;
            meta.scopes = scopes; meta.essential = flags.essential; meta.inUse = flags.inUse;
            meta.lastUsed = Date.now(); ms.put(meta);
            tx.oncomplete = function () { end(2); };
            tx.onerror = tx.onabort = function () { end(4); };
          }; };
        } catch (e) { end(1); }
      });
    };
    A.mark = function (key, group, url, variant, essential, inUse) {
      var old = A.pins[key] || {};
      old.url = url; old.variant = variant;
      old.groups = old.groups || {};
      var mark = {}; mark.essential = !!essential; mark.inUse = !!inUse;
      old.groups[String(group)] = mark;
      A.pins[key] = old;
      A.enqueue(function (db, done) {
        if (!db) { done(); return; }
        try {
          var tx = db.transaction("meta", "readwrite"); var m = tx.objectStore("meta");
          var g = m.get(key);
          g.onsuccess = function () {
            var row = g.result;
            if (row) {
              row.scopes = row.scopes || {};
              row.scopes[String(group)] = mark;
              var flags = A.flags(row.scopes, false, false);
              row.essential = flags.essential; row.inUse = flags.inUse; row.lastUsed = Date.now(); m.put(row);
            }
          };
          tx.oncomplete = done; tx.onerror = tx.onabort = done;
        } catch (e) { done(); }
      });
    };
    A.clear = function (essentialOnly) {
      Object.keys(A.pins).forEach(function (k) {
        var groups = A.pins[k].groups || {};
        Object.keys(groups).forEach(function (g) {
          if (essentialOnly) groups[g].inUse = false;
          else { groups[g].essential = false; groups[g].inUse = false; }
        });
        if (!Object.keys(groups).length) delete A.pins[k];
      });
      A.enqueue(function (db, done) {
        if (!db) { done(); return; }
        try {
          var tx = db.transaction("meta", "readwrite");
          ["meta"].forEach(function (name) {
            var req = tx.objectStore(name).openCursor();
            req.onsuccess = function (e) {
              var c = e.target.result;
              if (!c) return;
              var v = c.value;
              if (essentialOnly) {
                var scopes = v.scopes || {};
                Object.keys(scopes).forEach(function (g) { scopes[g].inUse = false; });
                var flags = A.flags(scopes, false, false);
                v.scopes = scopes; v.essential = flags.essential; v.inUse = flags.inUse;
                c.update(v);
              } else if (v.inUse || v.essential || v.scopes) {
                v.inUse = false; v.essential = false; v.scopes = {}; c.update(v);
              }
              c.continue();
            };
          });
          tx.oncomplete = done; tx.onerror = tx.onabort = done;
        } catch (e) { done(); }
      });
    };
    A.clearGroup = function (group) {
      var id = String(group);
      Object.keys(A.pins).forEach(function (k) {
        if (A.pins[k].groups) {
          delete A.pins[k].groups[id];
          if (!Object.keys(A.pins[k].groups).length) delete A.pins[k];
        }
      });
      A.enqueue(function (db, done) {
        if (!db) { done(); return; }
        try {
          var tx = db.transaction("meta", "readwrite");
          ["meta"].forEach(function (name) {
            var req = tx.objectStore(name).openCursor();
            req.onsuccess = function (e) {
              var c = e.target.result; if (!c) return;
              var v = c.value; v.scopes = v.scopes || {}; delete v.scopes[id];
              var flags = A.flags(v.scopes, false, false);
              v.essential = flags.essential; v.inUse = flags.inUse; c.update(v); c.continue();
            };
          });
          tx.oncomplete = done; tx.onerror = tx.onabort = done;
        } catch (e) { done(); }
      });
    };
    A.prune = function (done, forceBytes) {
      if (!A.db) { if (done) done(false); return; }
      var settled = false;
      var finish = function (ok) { if (settled) return; settled = true; if (done) done(ok); };
      var all;
      try { all = A.db.transaction("meta", "readonly").objectStore("meta").getAll(); }
      catch (e) { finish(false); return; }
      all.onerror = function () { finish(false); };
      all.onsuccess = function () {
        var rows = all.result || []; var total = rows.reduce(function (n, r) { return n + (r.bytes | 0); }, 0);
        var need = forceBytes > 0 ? Math.max(forceBytes, total + forceBytes - A.limit) : 0;
        var freed = 0;
        rows.sort(function (a, b) { return (a.lastUsed || 0) - (b.lastUsed || 0); });
        var kill = [];
        for (var i = 0; i < rows.length && (total > A.limit || (need > 0 && freed < need)); i++) {
          if (rows[i].essential || rows[i].inUse) continue;
          kill.push(rows[i].key); var size = rows[i].bytes | 0; total -= size; freed += size;
        }
        if (!kill.length) { finish(total <= A.limit); return; }
        try {
          var tx = A.db.transaction(["meta", "blob"], "readwrite");
          kill.forEach(function (k) { tx.objectStore("meta").delete(k); tx.objectStore("blob").delete(k); });
          tx.oncomplete = function () { finish(need > 0 ? freed >= need : total <= A.limit); };
          tx.onerror = tx.onabort = function () { finish(false); };
        } catch (e) { finish(false); }
      };
    };
    A.enqueue(function (db, done) { if (db) A.prune(function () { done(); }); else done(); });
  });
}

static int idb_read(const char *url, int variant, unsigned char **bytes, long *n) {
  ArteJob *job;
  char *copy;
  if (bytes) *bytes = NULL;
  if (n) *n = 0;
  if (!url || !*url || !bytes || !n) return 0;
  job = (ArteJob *)calloc(1, sizeof *job);
  copy = strdup(url);
  if (!job || !copy) { free(job); free(copy); return 0; }
  MAIN_THREAD_ASYNC_EM_ASM({
    var url = UTF8ToString($1); Module._free($1);
    var A = Module.nvArtCache;
    if (!A || !A.read) { HEAP32[$0 >> 2] = 1; Atomics.notify(HEAP32, $0 >> 2); return; }
    A.read(url, $2, $0);
  }, (int)(intptr_t)job, (int)(intptr_t)copy, variant);
  for (;;) {
    int state = __atomic_load_n(&job->state, __ATOMIC_ACQUIRE);
    if (state == 2 && job->ptr && job->n > 512) {
      *bytes = (unsigned char *)(intptr_t)job->ptr; *n = job->n; free(job);
      atomic_fetch_add(&s_hits, 1); return 1;
    }
    if (state == 1) { free(job); atomic_fetch_add(&s_misses, 1); return 0; }
    if (state == 4) { free(job); atomic_fetch_add(&s_errors, 1); atomic_fetch_add(&s_misses, 1); return 0; }
    if (state == 0 && emscripten_futex_wait(&job->state, 0, 1500.0) == -ETIMEDOUT) {
      int expected = 0;
      if (__atomic_compare_exchange_n(&job->state, &expected, 3, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        atomic_fetch_add(&s_errors, 1); atomic_fetch_add(&s_misses, 1);
        return 0; /* JS owns the tombstone until the late completion. */
      }
    }
  }
}

int cachearte_buscar(const char *url, int variante, unsigned char **bytes, long *n) {
  cachearte_iniciar();
  return idb_read(url, variante, bytes, n);
}

void cachearte_iniciar(void) {
  int expected = 0;
  if (!atomic_compare_exchange_strong(&s_started, &expected, 1)) return;
  js_init();
}

void cachearte_salvar(const char *url, int variante, const unsigned char *bytes,
                      long n, int essencial) {
  long queued, before;
  cachearte_iniciar();
  unsigned char *copy;
  char *urlCopy;
  if (!url || !*url || !bytes || n <= 512 || n > NV_CACHE_ARTE_MAX_ENTRY) return;
  before = atomic_load(&s_queued);
  do {
    if (before > NV_CACHE_ARTE_MAX_QUEUED - n) {
      atomic_fetch_add(&s_errors, 1); return;
    }
  } while (!atomic_compare_exchange_weak(&s_queued, &before, before + n));
  queued = before + n;
  (void)queued;
  copy = (unsigned char *)malloc((size_t)n); urlCopy = strdup(url);
  if (!copy || !urlCopy) {
    free(copy); free(urlCopy); atomic_fetch_sub(&s_queued, n); atomic_fetch_add(&s_errors, 1); return;
  }
  memcpy(copy, bytes, (size_t)n);
  MAIN_THREAD_ASYNC_EM_ASM({
    var p = $0; var n = $1 | 0; var url = UTF8ToString($2); var variant = $3 | 0; var essential = !!$4;
    Module._free($2);
    var A = Module.nvArtCache; var key = A.key(url, variant); var data = HEAPU8.slice(p, p + n).buffer;
    Module._free(p);
    A.enqueue(function (db, done) {
      if (!db) { Module._nv_cachearte_write_done(n, 0); done(); return; }
      var pin = A.pins[key] || {};
      var flags = A.flags(pin.groups, pin.essential, pin.inUse);
      var row = {};
      row.key = key; row.url = url; row.variant = variant; row.bytes = n;
      row.scopes = pin.groups || {};
      row.essential = !!(essential || flags.essential); row.inUse = flags.inUse; row.lastUsed = Date.now();
      var complete = function (ok) {
        Module._nv_cachearte_write_done(n, ok ? 1 : 0);
        if (ok) A.prune(function () { done(); }); else done();
      };
      var attempt = function (retried) {
        var settled = false, tx;
        var fail = function (err) {
          if (settled) return;
          settled = true;
          var quota = err && err.name === "QuotaExceededError";
          if (quota && !retried) {
            A.prune(function (freed) {
              if (freed) attempt(true); else complete(false);
            }, n);
          } else complete(false);
        };
        try {
          tx = db.transaction(["meta", "blob"], "readwrite");
          var blob = { key: key, bytes: data };
          tx.objectStore("meta").put(row); tx.objectStore("blob").put(blob);
          tx.oncomplete = function () {
            if (settled) return;
            settled = true; complete(true);
          };
          tx.onerror = tx.onabort = function () { fail(tx.error); };
        } catch (e) { fail(e); }
      };
      attempt(false);
    });
  }, (int)(intptr_t)copy, (int)n, (int)(intptr_t)urlCopy, variante, essencial ? 1 : 0);
}

void cachearte_invalidar(const char *url, int variante) {
  char *copy;
  if (!url || !*url || !(copy = strdup(url))) return;
  cachearte_iniciar();
  MAIN_THREAD_ASYNC_EM_ASM({
    var url = UTF8ToString($0); Module._free($0);
    var A = Module.nvArtCache; var key = A.key(url, $1 | 0);
    A.enqueue(function (db, done) {
      if (!db) { done(); return; }
      try {
        var tx = db.transaction(["meta", "blob"], "readwrite");
        tx.objectStore("meta").delete(key); tx.objectStore("blob").delete(key);
        tx.oncomplete = done; tx.onerror = tx.onabort = done;
      } catch (e) { done(); }
    });
  }, (int)(intptr_t)copy, variante);
}

void cachearte_marcar(const char *url, int variante, int essencial, int emUso) {
  cachearte_marcar_grupo(NV_CACHE_ARTE_GRUPO_GLOBAL, url, variante, essencial, emUso);
}

void cachearte_marcar_grupo(int grupo, const char *url, int variante,
                            int essencial, int emUso) {
  char *copy;
  cachearte_iniciar();
  if (!url || !*url || !(copy = strdup(url))) return;
  MAIN_THREAD_ASYNC_EM_ASM({
    var url = UTF8ToString($0); Module._free($0);
    var A = Module.nvArtCache; var key = A.key(url, $1 | 0);
    A.mark(key, $2 | 0, url, $1 | 0, !!$3, !!$4);
  }, (int)(intptr_t)copy, variante, grupo, essencial ? 1 : 0, emUso ? 1 : 0);
}

void cachearte_limpar_referencias_grupo(int grupo) {
  cachearte_iniciar();
  MAIN_THREAD_ASYNC_EM_ASM({ Module.nvArtCache.clearGroup($0 | 0); }, grupo);
}

void cachearte_limpar_uso(void) {
  cachearte_iniciar();
  MAIN_THREAD_ASYNC_EM_ASM({ Module.nvArtCache.clear(true); });
}

void cachearte_limpar_referencias(void) {
  cachearte_iniciar();
  MAIN_THREAD_ASYNC_EM_ASM({ Module.nvArtCache.clear(false); });
}

void cachearte_estatisticas_pedir(void) {
  cachearte_iniciar();
  MAIN_THREAD_ASYNC_EM_ASM({
    var A = Module.nvArtCache;
    A.enqueue(function (db, done) {
      if (!db) { _nv_cachearte_inventory_done(0, 0, 0, 0); done(); return; }
      try {
        var req = db.transaction("meta", "readonly").objectStore("meta").getAll();
        req.onsuccess = function () {
          var rows = req.result || []; var n = rows.length; var b = 0; var e = 0;
          rows.forEach(function (x) { b += x.bytes | 0; if (x.essential) e++; });
          var wanted = Object.keys(A.pins).filter(function (k) {
            return A.flags((A.pins[k] || {}).groups, false, false).essential;
          }).length;
          _nv_cachearte_inventory_done(n, b, e, wanted); done();
        };
        req.onerror = function () { _nv_cachearte_inventory_done(0, 0, 0, 0); done(); };
      } catch (e) { _nv_cachearte_inventory_done(0, 0, 0, 0); done(); }
    });
  });
}

void cachearte_estatisticas(NvCacheArteStats *out) {
  if (!out) return;
  out->itens = atomic_load(&s_inventory[0]); out->bytes = atomic_load(&s_inventory[1]);
  out->essenciais = atomic_load(&s_inventory[2]); out->essenciais_esperados = atomic_load(&s_inventory[3]);
  out->hits = atomic_load(&s_hits); out->misses = atomic_load(&s_misses);
  out->gravacoes = atomic_load(&s_writes); out->falhas = atomic_load(&s_errors);
}

void cachearte_limite_bytes(long bytes) {
  cachearte_iniciar();
  if (bytes > 1024 * 1024) s_limit = bytes;
  MAIN_THREAD_ASYNC_EM_ASM({ var A = Module.nvArtCache; if (A) A.limit = $0; }, (int)s_limit);
}

#else

#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

/* NOME DO ARQUIVO CALCULADO UMA VEZ, NA CRIACAO DO PINO (22/09/2026).
 *
 * A versao anterior recalculava o FNV da URL de CADA pino para CADA arquivo
 * da pasta: atualizarInventarioNativo (tex_cache.c) varria o diretorio e
 * chamava cachearte_nativo_essencial por arquivo, que andava a lista de pinos
 * inteira fazendo hash. Com 3309 arquivos e ~500 pinos da home isso e 1,6
 * milhao de hashes de ~100 bytes, duas vezes por download e sob discoMtx.
 * MEDIDO no Mac (tests/texlento.c, 3000 arquivos, 500 pinos, disco
 * instantaneo): 447 ms por download so disso; na C9, ~10x mais lenta, e o
 * piso de ~3,9 s de persist_ms que o log mostrava em todo fetch http.
 * Aqui o nome fica no pino e a comparacao e um strcmp de 13 bytes. */
typedef struct NativePin {
  char *url; char name[32]; unsigned long h;
  int group, essential, inUse; struct NativePin *next;
} NativePin;
static pthread_mutex_t s_native_mtx = PTHREAD_MUTEX_INITIALIZER;
static NativePin *s_pins;
static _Atomic long s_hits, s_misses, s_writes, s_errors, s_items, s_bytes;
static _Atomic long s_essential_present;
static _Atomic int s_inventory_running;
static char s_native_dir[512];

static unsigned long native_hash(const char *url) {
  unsigned long h = 2166136261UL; const char *p = url;
  for (; *p; p++) { h ^= (unsigned char)*p; h *= 16777619UL; }
  return h;
}
/* Mesmo nome que nomeDeCache (tex_cache.c) da: hash de 8 hex + extensao. */
static void native_name(const char *url, char *name, size_t cap) {
  unsigned long h = native_hash(url); const char *dot = strrchr(url, '.'); char ext[8] = ".jpg";
  if (dot && strlen(dot) <= 5 && !strchr(dot, '/')) snprintf(ext, sizeof ext, "%s", dot);
  snprintf(name, cap, "%08lx%s", h, ext);
}
static const char *base_name(const char *path) {
  const char *b = strrchr(path, '/'); return b ? b + 1 : path;
}

static NativePin *native_find(const char *url, int group, int create) {
  NativePin *p;
  for (p = s_pins; p; p = p->next)
    if (p->group == group && !strcmp(p->url, url)) return p;
  if (!create || !(p = (NativePin *)calloc(1, sizeof *p))) return NULL;
  p->url = strdup(url); if (!p->url) { free(p); return NULL; } p->group = group;
  p->h = native_hash(url); native_name(url, p->name, sizeof p->name);
  p->next = s_pins; s_pins = p; return p;
}

/* INDICE EM MEMORIA DA PASTA DE CACHE (22/09/2026).
 *
 * Antes cada download relia a pasta inteira: prepararDisco -> readdir+lstat de
 * todos os arquivos, e de novo depois da gravacao, sempre sob discoMtx. Com
 * 3309 arquivos no eMMC da C9 isso segurava a trava por segundos, e o acerto de
 * disco dos OUTROS fios (garantirLocal, inclusive a chamada que o fio de decode
 * faz) esperava na mesma trava: `phase=disk-cache cache_ms=4568` e `ler 4479`.
 * Agora a pasta e lida UMA vez por sessao, num fio proprio, e daqui em diante
 * so o fio de gravacao, a poda e a invalidacao mexem no indice, em O(1).
 * A trava do indice so cobre operacoes de memoria: nenhum I/O e feito com ela. */
typedef struct NvEnt { char nome[24]; long bytes; long uso; struct NvEnt *prox; } NvEnt;
#define NV_IDX_BALDES 4096
static pthread_mutex_t s_idx_mtx = PTHREAD_MUTEX_INITIALIZER;
static NvEnt *s_idx[NV_IDX_BALDES];
static long s_idx_n, s_idx_bytes;
static _Atomic long s_idx_bytes_pub;
/* 0 = nao lido, 1 = lendo, 2 = pronto. */
static _Atomic int s_idx_estado;

static unsigned idx_balde(const char *nome) {
  unsigned long h = 5381; const unsigned char *p = (const unsigned char *)nome;
  for (; *p; p++) h = h * 33 + *p;
  return (unsigned)(h % NV_IDX_BALDES);
}
/* Chamador segura s_idx_mtx. */
static NvEnt **idx_achar(const char *nome) {
  NvEnt **e = &s_idx[idx_balde(nome)];
  while (*e && strcmp((*e)->nome, nome)) e = &(*e)->prox;
  return e;
}
static void idx_por(const char *nome, long bytes, long uso, int substituir) {
  NvEnt **e;
  if (strlen(nome) >= sizeof((NvEnt *)0)->nome) return;
  pthread_mutex_lock(&s_idx_mtx);
  e = idx_achar(nome);
  if (*e) {
    if (substituir) { s_idx_bytes += bytes - (*e)->bytes; (*e)->bytes = bytes; (*e)->uso = uso; }
  } else {
    NvEnt *n = (NvEnt *)calloc(1, sizeof *n);
    if (n) {
      snprintf(n->nome, sizeof n->nome, "%s", nome); n->bytes = bytes; n->uso = uso;
      *e = n; s_idx_n++; s_idx_bytes += bytes;
    }
  }
  atomic_store(&s_idx_bytes_pub, s_idx_bytes);
  pthread_mutex_unlock(&s_idx_mtx);
}
static int native_image_name(const char *name) {
  const char *p = name;
  while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
         (*p >= 'A' && *p <= 'F')) p++;
  if (p - name < 8 || p - name > 16) return 0;
  return !strcmp(p, ".jpg") || !strcmp(p, ".jpeg") || !strcmp(p, ".png") ||
         !strcmp(p, ".webp") || !strcmp(p, ".gif");
}
/* A UNICA VARREDURA DA SESSAO. Roda sem trava nenhuma durante o readdir/lstat;
 * so a insercao pega s_idx_mtx, e com "nao substituir": o que o fio de
 * gravacao registrou enquanto isto rodava e mais novo que o lstat daqui. */
void cachearte_nativo_indice_construir(void) {
  char dir[512]; DIR *d; struct dirent *e; int esperado = 0; long n = 0, b = 0;
  if (!atomic_compare_exchange_strong(&s_idx_estado, &esperado, 1)) return;
  pthread_mutex_lock(&s_native_mtx);
  snprintf(dir, sizeof dir, "%s", s_native_dir);
  pthread_mutex_unlock(&s_native_mtx);
  d = dir[0] ? opendir(dir) : NULL;
  if (d) {
    while ((e = readdir(d)) != NULL) {
      char path[1024]; struct stat st;
      if (!native_image_name(e->d_name)) continue;
      snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
      if (lstat(path, &st) || !S_ISREG(st.st_mode)) continue;
      idx_por(e->d_name, (long)st.st_size, (long)st.st_mtime, 0);
      n++; b += (long)st.st_size;
    }
    closedir(d);
  }
  atomic_store(&s_idx_estado, 2);
  if (n) { printf("[tex] cache de disco ja tinha %ld arquivo(s), %.1f MB\n", n, b / 1048576.0); fflush(stdout); }
}
int cachearte_nativo_indice_pronto(void) { return atomic_load(&s_idx_estado) == 2; }
void cachearte_nativo_indice_registrar(const char *path, long bytes) {
  if (path && native_image_name(base_name(path))) idx_por(base_name(path), bytes, (long)time(NULL), 1);
}
void cachearte_nativo_indice_remover(const char *path) {
  NvEnt **e, *x;
  if (!path) return;
  pthread_mutex_lock(&s_idx_mtx);
  e = idx_achar(base_name(path));
  if ((x = *e) != NULL) { *e = x->prox; s_idx_n--; s_idx_bytes -= x->bytes; free(x); }
  atomic_store(&s_idx_bytes_pub, s_idx_bytes);
  pthread_mutex_unlock(&s_idx_mtx);
}
/* Acerto de disco: o LRU passa a ver o uso sem nenhuma escrita no eMMC. */
void cachearte_nativo_indice_tocar(const char *path) {
  NvEnt *x;
  if (!path) return;
  pthread_mutex_lock(&s_idx_mtx);
  if ((x = *idx_achar(base_name(path))) != NULL) x->uso = (long)time(NULL);
  pthread_mutex_unlock(&s_idx_mtx);
}
long cachearte_nativo_indice_bytes(void) { return atomic_load(&s_idx_bytes_pub); }

typedef struct { char nome[24]; long bytes; long uso; } NvCand;
static int cand_ordem(const void *a, const void *b) {
  const NvCand *x = a, *y = b;
  if (x->uso != y->uso) return x->uso < y->uso ? -1 : 1;
  return strcmp(x->nome, y->nome);
}
/* PODA PELO INDICE, com a mesma politica de nv_cache_podar (mais velho
 * primeiro, folga de 25%, reserva de espaco livre, protegidos ficam). A copia
 * dos candidatos e feita sob s_idx_mtx; o unlink e o callback de protecao,
 * que pega a trava das texturas, rodam sem ela. So o fio de gravacao chama
 * isto no caminho normal. */
long cachearte_nativo_podar(long entrada, long teto, uint64_t reserva, int forcar,
                            NvCacheProtegido protegido, void *ctx) {
  static pthread_mutex_t so_uma = PTHREAD_MUTEX_INITIALIZER;
  char dir[512]; struct statvfs fs; uint64_t livre = 0, falta = 0;
  long total, alvo = teto, apagados = 0, liberados = 0;
  NvCand *lista = NULL; size_t n = 0, i;
  if (!cachearte_nativo_indice_pronto()) return 0;
  if (pthread_mutex_trylock(&so_uma)) return 0;   /* outra poda ja esta nisso */
  pthread_mutex_lock(&s_native_mtx);
  snprintf(dir, sizeof dir, "%s", s_native_dir);
  pthread_mutex_unlock(&s_native_mtx);
  if (dir[0] && statvfs(dir, &fs) == 0) {
    livre = (uint64_t)fs.f_bavail * fs.f_frsize;
    if (livre < reserva + (uint64_t)entrada) falta = reserva + (uint64_t)entrada - livre;
  }
  total = cachearte_nativo_indice_bytes();
  if (!forcar && total <= teto - entrada && !falta) { pthread_mutex_unlock(&so_uma); return 0; }
  if (total > teto - entrada) alvo = teto - teto / 4 - entrada;
  if (alvo < 0) alvo = 0;
  if (forcar && falta < 32UL * 1024 * 1024) falta = 32UL * 1024 * 1024;
  pthread_mutex_lock(&s_idx_mtx);
  lista = s_idx_n > 0 ? (NvCand *)malloc((size_t)s_idx_n * sizeof *lista) : NULL;
  if (lista) {
    for (i = 0; i < NV_IDX_BALDES; i++) {
      NvEnt *e;
      for (e = s_idx[i]; e && n < (size_t)s_idx_n; e = e->prox) {
        memcpy(lista[n].nome, e->nome, sizeof lista[n].nome);
        lista[n].bytes = e->bytes; lista[n].uso = e->uso; n++;
      }
    }
  }
  pthread_mutex_unlock(&s_idx_mtx);
  if (n > 1) qsort(lista, n, sizeof *lista, cand_ordem);
  for (i = 0; i < n && (total > alvo || (uint64_t)liberados < falta); i++) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, lista[i].nome);
    if (protegido && protegido(path, ctx)) continue;
    if (unlink(path) && errno != ENOENT) continue;
    cachearte_nativo_indice_remover(path);
    total -= lista[i].bytes; liberados += lista[i].bytes; apagados++;
  }
  free(lista);
  pthread_mutex_unlock(&so_uma);
  if (apagados) {
    printf("[tex] cache de disco podado: %ld arquivo(s), %.1f MB liberados, agora %.1f MB (teto %.0f MB)\n",
           apagados, liberados / 1048576.0, cachearte_nativo_indice_bytes() / 1048576.0,
           teto / 1048576.0);
    fflush(stdout);
  }
  return apagados;
}

void cachearte_iniciar(void) {}
int cachearte_buscar(const char *url, int variante, unsigned char **bytes, long *n) {
  (void)url; (void)variante; if (bytes) *bytes = NULL; if (n) *n = 0; return 0;
}
void cachearte_salvar(const char *url, int variante, const unsigned char *bytes, long n, int essencial) {
  (void)url; (void)variante; (void)bytes; (void)n; (void)essencial;
}
void cachearte_invalidar(const char *url, int variante) { (void)url; (void)variante; }
void cachearte_marcar(const char *url, int variante, int essencial, int emUso) {
  cachearte_marcar_grupo(NV_CACHE_ARTE_GRUPO_GLOBAL, url, variante, essencial, emUso);
}
void cachearte_marcar_grupo(int grupo, const char *url, int variante,
                            int essencial, int emUso) {
  NativePin *p, **link; (void)variante; if (!url || !*url) return;
  pthread_mutex_lock(&s_native_mtx); p = native_find(url, grupo, 1);
  if (p) { p->essential = !!essencial; p->inUse = !!emUso; }
  for (link = &s_pins; *link;) {
    p = *link;
    if (!p->essential && !p->inUse) { *link = p->next; free(p->url); free(p); }
    else link = &p->next;
  }
  pthread_mutex_unlock(&s_native_mtx);
}
void cachearte_limpar_uso(void) {
  NativePin *p, **link; pthread_mutex_lock(&s_native_mtx);
  for (p = s_pins; p; p = p->next) p->inUse = 0;
  for (link = &s_pins; *link;) {
    p = *link;
    if (!p->essential) { *link = p->next; free(p->url); free(p); }
    else link = &p->next;
  }
  pthread_mutex_unlock(&s_native_mtx);
}
void cachearte_limpar_referencias(void) {
  NativePin *p, **link; pthread_mutex_lock(&s_native_mtx);
  for (link = &s_pins; *link;) {
    p = *link; *link = p->next; free(p->url); free(p);
  }
  pthread_mutex_unlock(&s_native_mtx);
}
void cachearte_limpar_referencias_grupo(int grupo) {
  NativePin *p, **link; pthread_mutex_lock(&s_native_mtx);
  for (link = &s_pins; *link;) {
    p = *link;
    if (p->group == grupo) { *link = p->next; free(p->url); free(p); }
    else link = &p->next;
  }
  pthread_mutex_unlock(&s_native_mtx);
}
/* INVENTARIO SEM VARRER A PASTA. Itens e bytes vem do indice; "essenciais
 * presentes" e uma consulta ao indice por pino essencial, O(pinos). A pasta so
 * e lida se o indice ainda nao existe, e ai num fio proprio, uma vez. Antes
 * main.c pedia isto a cada 3 s e cada pedido criava um fio que relia 3309
 * arquivos no eMMC e fazia o hash de todos os pinos por arquivo. */
static void publicar_inventario(void) {
  NativePin *p; long essential = 0;
  pthread_mutex_lock(&s_native_mtx);
  pthread_mutex_lock(&s_idx_mtx);
  for (p = s_pins; p; p = p->next) if (p->essential) {
    NativePin *q; int first = 1;
    for (q = s_pins; q != p; q = q->next)
      if (q->essential && q->h == p->h && !strcmp(q->name, p->name)) { first = 0; break; }
    if (first && *idx_achar(p->name)) essential++;
  }
  atomic_store(&s_items, s_idx_n); atomic_store(&s_bytes, s_idx_bytes);
  pthread_mutex_unlock(&s_idx_mtx);
  pthread_mutex_unlock(&s_native_mtx);
  atomic_store(&s_essential_present, essential);
}
static void *native_inventory_thread(void *unused) {
  (void)unused;
  cachearte_nativo_indice_construir();
  publicar_inventario();
  atomic_store(&s_inventory_running, 0);
  return NULL;
}
void cachearte_nativo_configurar_diretorio(const char *dir) {
  pthread_mutex_lock(&s_native_mtx);
  snprintf(s_native_dir, sizeof s_native_dir, "%s", dir ? dir : "");
  pthread_mutex_unlock(&s_native_mtx);
}
void cachearte_estatisticas_pedir(void) {
  pthread_t thread; int expected = 0;
  if (cachearte_nativo_indice_pronto()) { publicar_inventario(); return; }
  if (!atomic_compare_exchange_strong(&s_inventory_running, &expected, 1)) return;
  if (pthread_create(&thread, NULL, native_inventory_thread, NULL) != 0) {
    atomic_store(&s_inventory_running, 0); return;
  }
  pthread_detach(thread);
}
void cachearte_estatisticas(NvCacheArteStats *out) {
  NativePin *p;
  long expected = 0;
  if (!out) return;
  /* Deduplica por nome de arquivo (o hash compara primeiro): o mesmo arquivo
   * pinado pela home e pelo perfil conta uma vez, como antes. */
  pthread_mutex_lock(&s_native_mtx);
  for (p = s_pins; p; p = p->next) if (p->essential) {
    NativePin *prior; int first = 1;
    for (prior = s_pins; prior != p; prior = prior->next)
      if (prior->essential && prior->h == p->h && !strcmp(prior->url, p->url)) { first = 0; break; }
    if (first) expected++;
  }
  pthread_mutex_unlock(&s_native_mtx);
  out->itens = atomic_load(&s_items); out->bytes = atomic_load(&s_bytes);
  out->essenciais = atomic_load(&s_essential_present); out->essenciais_esperados = expected;
  out->hits = atomic_load(&s_hits);
  out->misses = atomic_load(&s_misses); out->gravacoes = atomic_load(&s_writes);
  out->falhas = atomic_load(&s_errors);
}
void cachearte_limite_bytes(long bytes) { (void)bytes; }
void cachearte_nativo_hit(void) { atomic_fetch_add(&s_hits, 1); }
void cachearte_nativo_miss(void) { atomic_fetch_add(&s_misses, 1); }
void cachearte_nativo_gravacao(int ok) { if (ok) atomic_fetch_add(&s_writes, 1); else atomic_fetch_add(&s_errors, 1); }
void cachearte_nativo_inventario(long itens, long bytes, long essenciais_presentes) {
  atomic_store(&s_items, itens); atomic_store(&s_bytes, bytes);
  atomic_store(&s_essential_present, essenciais_presentes);
}
int cachearte_nativo_protegido(const char *path) {
  NativePin *p; const char *base; int protected = 0;
  if (!path) return 0; base = base_name(path);
  pthread_mutex_lock(&s_native_mtx);
  for (p = s_pins; p; p = p->next)
    if ((p->essential || p->inUse) && !strcmp(p->name, base)) { protected = 1; break; }
  pthread_mutex_unlock(&s_native_mtx); return protected;
}
int cachearte_nativo_essencial(const char *path) {
  NativePin *p; const char *base; int essential = 0;
  if (!path) return 0; base = base_name(path);
  pthread_mutex_lock(&s_native_mtx);
  for (p = s_pins; p; p = p->next)
    if (p->essential && !strcmp(p->name, base)) { essential = 1; break; }
  pthread_mutex_unlock(&s_native_mtx); return essential;
}

#endif
