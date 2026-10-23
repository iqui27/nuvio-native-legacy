// Jornal da conta (contapend.c) contra um SERVIDOR FALSO que guarda estado:
// vistos por perfil (push/delete por item) e biblioteca por perfil com
// sync_push_library SUBSTITUINDO a lista (o pior caso: e o que torna um push
// curto perigoso). O "disco" e um mapa em memoria, para testar o reabrir.
//
// Casos: offline -> online; desmarcar propaga so a chave; ultima mudanca vence
// no pull; pagina que falha nao vira push; lista remota vazia com copia nao
// vira push; tirar o ultimo nao vira push vazio; perfis isolados; poda.
#include "contapend.h"
#include "visto.h"
#include "vistoep.h"
#include "js.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OK(s) fprintf(stderr, "ok  %s\n", s)

// ---------------------------------------------------------------- dubles
static int perfil = 1, logada = 1;
static long long agora = 1759000000000LL;
static long long relogio(void) { return agora; }
static int switchAfterRead, gateHeld, trackerProfile, trackerTvdb;
static uint64_t generation;
int perfis_ativo(void) {
  int p = perfil;
  if (switchAfterRead && !--switchAfterRead && !gateHeld) {
    perfil = 2;
    generation++;
  }
  return p;
}
uint64_t perfis_sessao_geracao(void) { return generation; }
int sessao_logada(void) { return logada; }
int perfis_sessao_travar(uint64_t g) {
  if (g != generation) return 0;
  gateHeld++;
  return 1;
}
void perfis_sessao_destravar(void) { assert(gateHeld > 0); gateHeld--; }

static int retry(const char *id, const char *type, const VistoPar *pairs,
                 int n, int watched, int destinations, const int *seasons, int nt,
                 const int *tvdbEps) {
  (void)id; (void)type; (void)pairs; (void)n; (void)watched; (void)seasons; (void)nt;
  trackerTvdb = n && tvdbEps ? tvdbEps[0] : 0;
  trackerProfile = perfil;
  return destinations;
}
const char *sessao_usuario(void) { return logada ? "u-123" : ""; }
const char *dados_cliente_id(void) { return "cliente-teste"; }

static char *disco;          // um arquivo so basta: o nome e conferido
static char discoNome[128];
char *dados_ler(const char *n) {
  if (!disco || strcmp(n, discoNome)) return NULL;
  return strdup(disco);
}
int dados_gravar(const char *n, const char *c) {
  snprintf(discoNome, sizeof discoNome, "%s", n);
  free(disco); disco = strdup(c); return 1;
}
int dados_apagar(const char *n) { (void)n; free(disco); disco = NULL; return 1; }

static const char *copiaBib;   // contacache da biblioteca
char *contacache_ler(const char *s, int p, const char *u, long *q) {
  (void)s; (void)p; (void)u; (void)q;
  return copiaBib ? strdup(copiaBib) : NULL;
}

static char histId[64]; static int histVisto = -1;
void cat_historico_definir_id(const char *i, const char *t, int v) {
  (void)t; snprintf(histId, sizeof histId, "%s", i); histVisto = v;
}

// ---- servidor
#define SV_MAX 1200
typedef struct { int perfil; char id[24]; int t, e; } Vis;
static Vis vis[SV_MAX]; static int nVis;
typedef struct { int perfil; char id[24]; long long added; } Lib;
static Lib lib[SV_MAX]; static int nLib;
static int fora, falhaOffset = -1, nPushLib, nPushVis, nDelVis, nPull;
static char ultimoPushLib[1 << 20];

static int achaVis(int p, const char *id, int t, int e) {
  int i;
  for (i = 0; i < nVis; i++)
    if (vis[i].perfil == p && vis[i].t == t && vis[i].e == e && !strcmp(vis[i].id, id)) return i;
  return -1;
}

char *sessao_rpc(const char *fn, const char *corpo, int *st) {
  int p = (int)js_num(corpo, NULL, "p_profile_id", -1);
  if (fora) { *st = 0; return NULL; }
  *st = 200;
  if (!strcmp(fn, "sync_push_watched_items") || !strcmp(fn, "sync_delete_watched_items")) {
    int del = fn[5] == 'd';
    const char *a = js_array(corpo, NULL, del ? "p_keys" : "p_items"), *q;
    assert(strstr(corpo, "\"p_origin_client_id\":\"cliente-teste\""));
    assert(a);
    if (del) nDelVis++; else nPushVis++;
    for (q = a; q; q = js_prox(js_fim(q))) {
      const char *f = js_fim(q);
      char id[24]; int t, e, k;
      assert(js_texto(q, f, "content_id", id, sizeof id));
      t = (int)js_num(q, f, "season", 0); e = (int)js_num(q, f, "episode", 0);
      if (del) {
        // A forma do delete e CHAVE: nada de watched_at nem content_type.
        assert(!js_tem(q, f, "watched_at") && !js_tem(q, f, "content_type"));
        if ((k = achaVis(p, id, t, e)) >= 0) vis[k] = vis[--nVis];
      } else {
        assert(js_tem(q, f, "watched_at") && js_tem(q, f, "content_type"));
        if (achaVis(p, id, t, e) < 0) {
          vis[nVis].perfil = p; snprintf(vis[nVis].id, 24, "%s", id);
          vis[nVis].t = t; vis[nVis].e = e; nVis++;
        }
      }
    }
    return strdup("null");
  }
  if (!strcmp(fn, "sync_pull_library")) {
    int lim = (int)js_num(corpo, NULL, "p_limit", 0), off = (int)js_num(corpo, NULL, "p_offset", 0);
    int i, k = 0, out = 0;
    size_t cap = 256 + (size_t)lim * 160, n;
    char *r = malloc(cap);
    nPull++;
    if (off == falhaOffset) { *st = 503; free(r); return strdup("{\"message\":\"down\"}"); }
    n = (size_t)snprintf(r, cap, "[");
    for (i = 0; i < nLib; i++) {
      if (lib[i].perfil != p) continue;
      if (k++ < off) continue;
      if (out == lim) break;
      n += (size_t)snprintf(r + n, cap - n, "%s{\"content_id\":\"%s\",\"content_type\":\"movie\","
                            "\"name\":\"T %s\",\"genres\":[\"Drama\"],\"added_at\":%lld}",
                            out ? "," : "", lib[i].id, lib[i].id, lib[i].added);
      out++;
    }
    snprintf(r + n, cap - n, "]");
    return r;
  }
  if (!strcmp(fn, "sync_push_library")) {
    const char *a = js_array(corpo, NULL, "p_items"), *q;
    int i, j;
    assert(strstr(corpo, "\"p_origin_client_id\":\"cliente-teste\""));
    nPushLib++;
    snprintf(ultimoPushLib, sizeof ultimoPushLib, "%s", corpo);
    // SUBSTITUI a lista do perfil (o pior caso).
    for (i = j = 0; i < nLib; i++) if (lib[i].perfil != p) lib[j++] = lib[i];
    nLib = j;
    for (q = a; q; q = js_prox(js_fim(q))) {
      lib[nLib].perfil = p;
      assert(js_texto(q, js_fim(q), "content_id", lib[nLib].id, 24));
      lib[nLib].added = (long long)js_num(q, js_fim(q), "added_at", 0);
      nLib++;
    }
    return strdup("null");
  }
  assert(!"rpc inesperada");
  return NULL;
}

static int temLib(int p, const char *id) {
  int i;
  for (i = 0; i < nLib; i++) if (lib[i].perfil == p && !strcmp(lib[i].id, id)) return 1;
  return 0;
}
static int libDe(int p) { int i, k = 0; for (i = 0; i < nLib; i++) k += lib[i].perfil == p; return k; }
static void addLib(int p, const char *id, long long added) {
  lib[nLib].perfil = p; snprintf(lib[nLib].id, 24, "%s", id); lib[nLib].added = added; nLib++;
}

static void reabrir(void) { contapend_esquecer(); }   // memoria some, disco fica

static void zerar(void) {
  free(disco); disco = NULL; discoNome[0] = 0;
  contapend_esquecer();
  nVis = nLib = 0; fora = 0; falhaOffset = -1;
  nPushLib = nPushVis = nDelVis = nPull = 0;
  perfil = 1; logada = 1; copiaBib = NULL;
  vistoep_esquecer();
}

// ---------------------------------------------------------------- casos
int main(int argc, char **argv) {
  if (argc > 1) {
    VistoPar pair = {1, 1};
    contapend_relogio(relogio);
    zerar();
    if (!strcmp(argv[1], "retry")) {
      contapend_reenviar_com(retry);
      assert(contapend_gesto("tt0903747", "series", &pair, 1, 1,
                             VISTO_TRAKT | VISTO_SIMKL, NULL, 0));
      // Switch immediately after the final active-profile check returns A.
      switchAfterRead = 2;
      contapend_reenviar();
      assert(trackerProfile != 2);
      assert(!gateHeld);
      perfil = 1;
      generation++;
      assert(contapend_reenviar() >= 0);
      assert(trackerProfile == 1 && contapend_pendentes() == 0);
      OK("retry never sends profile A's gesture with profile B's credentials");
    } else if (!strcmp(argv[1], "identity")) {
      int tvdb = 11209906;
      contapend_reenviar_com(retry);
      long long ms = contapend_gesto_alvos("tt21975436", "series", &pair,
                                           &tvdb, 1, 1, VISTO_TRAKT, NULL, 0);
      assert(ms);
      reabrir();
      assert(contapend_reenviar() >= 0);
      assert(trackerTvdb == tvdb && contapend_pendentes() == 0);
      OK("exact TVDB retry target survives journal reopen");
    } else {
      assert(!strcmp(argv[1], "progress"));
      assert(contapend_gesto("tt0903747", "series", &pair, 1, 1,
                             VISTO_TRAKT, NULL, 0));
      contapend_confirmar_gesto("tt0903747", &pair, 1, agora, VISTO_TRAKT);
      reabrir();
      assert(contapend_aplicar_local() == 1);
      assert(vistoep_estado("tt0903747", 1, 1) == 1);
      assert(vistoep_ler_progresso("tt0903747",
        "{\"seasons\":[{\"number\":1,\"episodes\":["
        "{\"number\":1,\"completed\":false},{\"number\":2,\"completed\":false}]}]}") == 2);
      assert(vistoep_estado("tt0903747", 1, 1) == 1);
      assert(vistoep_estado("tt0903747", 1, 2) == 0);
      assert(vistoep_fonte("tt0903747", 1, 1, 0, agora + 1, NULL));
      assert(vistoep_estado("tt0903747", 1, 1) == 0);
      assert(vistoep_fonte("tt0903747", 1, 1, 1, agora - 1, NULL));
      assert(vistoep_estado("tt0903747", 1, 1) == 0);
      OK("restored watched gesture survives stale progress; newer remote wins");
    }
    return 0;
  }
  VistoPar eps[3] = { {1, 1}, {1, 2}, {1, 3} };
  contapend_relogio(relogio);
  contapend_sem_fio(1);

  // 1. OFFLINE marca, app fecha, abre online: tudo chega.
  zerar();
  fora = 1;
  assert(contapend_episodios("tt0903747", "series", eps, 3, 1) == 3);
  assert(contapend_titulo("tt1375666", "movie", 1) == 1);
  assert(contapend_enviar() < 0);
  assert(contapend_pendentes() == 4);
  assert(!strcmp(discoNome, "conta-pend-u-123.txt"));
  reabrir();
  assert(contapend_pendentes() == 4);          // sobreviveu ao "fechar o app"
  fora = 0;
  assert(contapend_enviar() == 4);
  assert(contapend_pendentes() == 0);
  assert(nVis == 4 && achaVis(1, "tt0903747", 1, 2) >= 0 && achaVis(1, "tt1375666", 0, 0) >= 0);
  assert(nPushVis == 1 && nDelVis == 0);        // mesma op, mesmo perfil: UM lote
  OK("offline: marcas vao a disco, sobrevivem ao reabrir e sobem quando volta a rede");

  // Reabrir sem rede: o estado local volta do jornal (vistoep nao vai a disco).
  vistoep_esquecer();
  histVisto = -1;
  agora += 1000;
  fora = 1;
  assert(contapend_episodios("tt0903747", "series", eps + 1, 1, 0) == 1);
  vistoep_esquecer();
  reabrir();
  assert(contapend_aplicar_local() >= 1);
  assert(vistoep_estado("tt0903747", 1, 2) == 0);
  assert(vistoep_estado("tt0903747", 1, 1) == 1);   // confirmado, ainda nao podado
  assert(!strcmp(histId, "tt1375666") && histVisto == 1);
  OK("reabrir offline: o jornal reaplica marcas e desmarcas na tela");

  // 2. DESMARCAR propaga so a chave daquele episodio.
  fora = 0;
  nPushVis = nDelVis = 0;
  assert(contapend_enviar() == 1);
  assert(nDelVis == 1 && nPushVis == 0);
  assert(achaVis(1, "tt0903747", 1, 2) < 0 && achaVis(1, "tt0903747", 1, 1) >= 0 &&
         achaVis(1, "tt0903747", 1, 3) >= 0);
  OK("desmarcar: sync_delete_watched_items com a chave do item, o resto fica");

  // Marcar e desmarcar offline o mesmo item = so a ultima intencao sobe.
  agora += 1000;
  fora = 1;
  { VistoPar x = { 3, 7 };
    contapend_episodios("tt0903747", "series", &x, 1, 1);
    agora += 10;
    contapend_episodios("tt0903747", "series", &x, 1, 0);
    assert(contapend_pendentes() == 1);
    fora = 0; nPushVis = nDelVis = 0;
    assert(contapend_enviar() == 1 && nPushVis == 0 && nDelVis == 1); }
  OK("marcar+desmarcar offline coalesce: so o delete sobe");

  // 3. ULTIMA MUDANCA VENCE no pull (local evidence survives cycle pruning).
  zerar();
  agora = 1759000100000LL;
  fora = 1;
  assert(contapend_titulo("tt0111161", "movie", 0) == 1);
  // linha remota MAIS VELHA que o "desmarcar": oculta
  assert(contapend_visto_oculto("tt0111161", 0, 0, agora - 5000) == 1);
  assert(contapend_estado("tt0111161", "movie", 0, 0, agora - 5000, 1) == 0);
  // outra TV re-marcou DEPOIS: remote newer evidence wins for the screen
  assert(contapend_estado("tt0111161", "movie", 0, 0, agora + 5000, 1) == 1);
  // um item sem gesto local nunca e oculto
  assert(contapend_visto_oculto("tt7777777", 1, 1, 1) == 0);
  // idem lista
  assert(contapend_lista("tt0068646", "movie", "O Poderoso", "", 0) == 1);
  assert(contapend_lista_oculta("tt0068646", agora - 1) == 1);
  assert(contapend_lista_oculta("tt0068646:1:2", agora - 1) == 1);  // sufixo de episodio
  assert(contapend_lista_oculta("tt0068646", agora + 1) == 0);

  OK("pull: linha remota velha nao re-marca o desmarcado; remota mais nova vence");

  // 3b. Serie desmarcada inteira + pull de episodio: tipo=NULL nao pode cair no strcmp.
  assert(contapend_titulo("tt0944947", "series", 0) == 1);
  assert(contapend_visto_oculto("tt0944947", 1, 2, agora - 1) == 1);
  OK("pull de episodio contra desmarca-serie: tipo NULL nao segfaulta");

  // 4. LISTA: pagina que falha nao vira push (e nao apaga nada).
  zerar();
  { int i; char id[24];
    for (i = 0; i < 600; i++) { snprintf(id, sizeof id, "tt%07d", i + 1); addLib(1, id, 1000 + i); } }
  agora = 1759000200000LL;
  assert(contapend_lista("tt9000001", "series", "Nova", "https://p/x.jpg", 1) == 1);
  falhaOffset = 500;
  assert(contapend_enviar() < 0);
  assert(nPushLib == 0 && libDe(1) == 600 && contapend_pendentes() == 1);
  falhaOffset = -1;
  assert(contapend_enviar() == 1);
  assert(nPushLib == 1 && libDe(1) == 601 && temLib(1, "tt9000001") && temLib(1, "tt0000600"));
  // A linha remota volta com o que tinha (genres, nome) e added_at numerico.
  assert(strstr(ultimoPushLib, "\"content_id\":\"tt0000001\",\"content_type\":\"movie\",\"name\":\"T tt0000001\""));
  assert(strstr(ultimoPushLib, "\"genres\":[\"Drama\"]"));
  assert(strstr(ultimoPushLib, "\"content_id\":\"tt9000001\",\"content_type\":\"series\",\"name\":\"Nova\",\"poster\":\"https://p/x.jpg\""));
  OK("lista: pagina 2 falhou -> nada sobe; depois sobe remota inteira (600) + o salvo");

  // 5. TIRAR: so o item sai; o resto da conta fica.
  agora += 1000;
  assert(contapend_lista("tt0000002", "movie", "", "", 0) == 1);
  nPushLib = 0;
  assert(contapend_enviar() == 1);
  assert(nPushLib == 1 && libDe(1) == 600 && !temLib(1, "tt0000002") && temLib(1, "tt0000003"));
  OK("lista: tirar manda a remota inteira menos aquele item");

  // Tirado aqui, mas salvo de novo em OUTRA TV depois: a conta vence.
  agora += 1000;
  assert(contapend_lista("tt0000003", "movie", "", "", 0) == 1);
  { int i; for (i = 0; i < nLib; i++) if (!strcmp(lib[i].id, "tt0000003")) lib[i].added = agora + 9999; }
  nPushLib = 0;
  assert(contapend_enviar() >= 0);
  assert(nPushLib == 0 && temLib(1, "tt0000003") && contapend_pendentes() == 0);
  OK("lista: remota salva depois do tirar ganha (nenhum push, pendente cai)");

  // 6. VAZIO NUNCA APAGA.
  zerar();
  copiaBib = "[{\"content_id\":\"tt1\"},{\"content_id\":\"tt2\"}]";
  assert(contapend_lista("tt3333333", "movie", "X", "", 1) == 1);
  assert(contapend_enviar() < 0 && nPushLib == 0 && contapend_pendentes() == 1);
  OK("conta respondeu vazia com copia nao vazia: nada sobe");
  copiaBib = NULL;
  zerar();
  addLib(1, "tt5555555", 1);
  agora += 1000;
  assert(contapend_lista("tt5555555", "movie", "", "", 0) == 1);
  assert(contapend_enviar() == 0 && nPushLib == 0 && temLib(1, "tt5555555"));
  assert(contapend_pendentes() == 1);
  assert(contapend_lista_oculta("tt5555555", 1) == 1);   // fica fora da tela
  // Outro titulo entra (outra TV): o tirar pendente sai junto, sem push vazio.
  addLib(1, "tt6666666", 2);
  assert(contapend_enviar() == 1 && nPushLib == 1);
  assert(!temLib(1, "tt5555555") && temLib(1, "tt6666666") && contapend_pendentes() == 0);
  OK("tirar o ultimo: sem push vazio, pendente e oculto; sai quando a lista ganha outro");
  // Sem gesto nenhum, nunca ha RPC de escrita.
  zerar();
  addLib(1, "tt1", 1);
  assert(contapend_enviar() == 0 && nPushLib == 0 && nPushVis == 0 && nDelVis == 0 && nPull == 0);
  OK("jornal vazio: nenhuma escrita (nem leitura) na conta");

  // 7. PERFIS ISOLADOS.
  zerar();
  addLib(1, "ttA", 1); addLib(2, "ttB", 1);
  perfil = 1;
  contapend_lista("ttP1", "movie", "", "", 1);
  contapend_titulo("ttF1", "movie", 1);
  perfil = 2;
  contapend_lista("ttP2", "movie", "", "", 1);
  contapend_titulo("ttF2", "movie", 1);
  assert(contapend_enviar() == 4);
  assert(temLib(1, "ttA") && temLib(1, "ttP1") && !temLib(1, "ttP2") && !temLib(1, "ttB"));
  assert(temLib(2, "ttB") && temLib(2, "ttP2") && !temLib(2, "ttP1"));
  assert(achaVis(1, "ttF1", 0, 0) >= 0 && achaVis(2, "ttF1", 0, 0) < 0);
  assert(achaVis(2, "ttF2", 0, 0) >= 0 && achaVis(1, "ttF2", 0, 0) < 0);
  // O gesto do perfil 1 vai no perfil 1 mesmo com o 2 ativo na hora do envio.
  perfil = 1;
  contapend_titulo("ttG1", "movie", 1);
  perfil = 2;
  assert(contapend_enviar() == 1 && achaVis(1, "ttG1", 0, 0) >= 0);
  // e a tela do perfil 2 nao recebe a marca do 1
  histVisto = -1; histId[0] = 0;
  contapend_aplicar_local();
  assert(strcmp(histId, "ttG1") && strcmp(histId, "ttF1"));
  OK("perfis: cada gesto sobe e aparece so no perfil onde foi feito");

  // 8. PODA: confirmadas antes do inicio do ciclo saem; pendentes nunca.
  agora += 1000;
  fora = 1;
  contapend_titulo("ttH", "movie", 1);
  contapend_podar(agora + 1);
  assert(contapend_pendentes() == 1);
  fora = 0;
  assert(contapend_enviar() == 1);
  contapend_podar(agora);            // confirmada AGORA: o pull deste ciclo e velho
  assert(contapend_visto_oculto("ttH", 0, 0, 0) == 0);
  assert(disco && strstr(disco, "ttH"));
  contapend_podar(agora + 1);
  assert(disco && strstr(disco, "ttH"));
  OK("poda: watched stays as evidence, stale remote loses until newer timestamp");

  // #639: confirmed removals survive later cycles, disk reload and stale pulls.
  zerar();
  addLib(1, "tt5555555", 1); addLib(1, "tt6666666", 2);
  agora += 1000;
  assert(contapend_lista("tt5555555", "movie", "", "", 0) == 1);
  assert(contapend_enviar() == 1);
  contapend_podar(agora + 1);
  contapend_esquecer();
  assert(contapend_lista_oculta("tt5555555", 1) == 1);
  perfil = 2;
  assert(contapend_lista_oculta("tt5555555", 1) == 0);
  perfil = 1;
  assert(contapend_lista_oculta("tt5555555", agora + 1) == 0);
  OK("confirmed library tombstone survives stale sync and reload; newer save wins");

  // 9. Sem conta, nada entra.
  zerar();
  logada = 0;
  assert(contapend_titulo("tt1", "movie", 1) == 0 && contapend_enviar() == 0);
  OK("deslogado: jornal nao registra nem envia");

  fprintf(stderr, "contapend: tudo ok\n");
  return 0;
}
