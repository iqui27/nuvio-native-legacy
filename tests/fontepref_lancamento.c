// THE SAME RELEASE ON ANY EPISODE (owner, 15/10/2026).
//
// Pick a release by hand on one episode, then open ANY other episode of the
// show (earlier or later, from the episode list): the same release must be
// tried first. Before the release fingerprint, the remembered key was only the
// show name (fileKey cuts at the episode marker), so with a broad bingeGroup
// ("aio|1080p") every release of the show tied and the first in the addon's
// list won. Also covers: no match -> -1 (the automatic pick takes over), the
// audio guard, the automatic memory, the four Playback settings, and ids that
// are not IMDb (anime by kitsu id) surviving a cold start.
// Same harness as tests/fontepref_auto.c (NUVIO_DADOS temporary).
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../src/fontepref.c"

#include "dados.h"

static int falhas;
#define CONFERE(cond, ...) do { \
    if (cond) { printf("ok    "); } else { printf("FALHA "); falhas++; } \
    printf(__VA_ARGS__); printf("\n"); } while (0)

static void fonte(Stream *s, const char *prov, const char *binge, const char *file, int altura) {
  memset(s, 0, sizeof *s);
  snprintf(s->provedor, sizeof s->provedor, "%s", prov);
  snprintf(s->rotulo, sizeof s->rotulo, "%s %dp", prov, altura);
  snprintf(s->url, sizeof s->url, "https://exemplo.invalido/%s", file);
  snprintf(s->bingeGroup, sizeof s->bingeGroup, "%s", binge);
  snprintf(s->arquivo, sizeof s->arquivo, "%s", file);
  s->altura = altura;
  s->mp4 = 0;
}

// Four releases of one show from ONE addon, all in the SAME broad bingeGroup
// and with the same (absent) audio marks: only the release name differs. The
// order changes per episode, as addons reorder by seeders/size.
static const char *GRUPOS[4][2] = {
  { "NTb",    "WEB-DL.DDP5.1.H.264" },
  { "FLUX",   "WEB-DL.DDP5.1.H.265" },
  { "SPARKS", "BluRay.x264" },
  { "GRP",    "WEBRip.x264" },
};
static int lista(Stream *v, int ep, const int *ordem) {
  char f[200];
  int i;
  for (i = 0; i < 4; i++) {
    int g = ordem[i];
    snprintf(f, sizeof f, "The.Show.S01E%02d.1080p.%s-%s.mkv", ep, GRUPOS[g][1], GRUPOS[g][0]);
    fonte(&v[i], "AIOStreams", "aio|1080p", f, 1080);
  }
  return 4;
}
// semPerda for the Next tests: only a bingeGroup match ("forte") may play.
static int soForte(int i, int forte, void *ctx) { (void)i; (void)ctx; return forte; }
static int posDe(const Stream *v, int n, const char *grupo) {
  int i;
  for (i = 0; i < n; i++) if (strstr(v[i].arquivo, grupo)) return i;
  return -1;
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  Stream v[6];
  char l[FONTEPREF_LANC];
  int n, k, ep;
  static const int ORDENS[4][4] = { { 0, 1, 2, 3 }, { 3, 2, 0, 1 }, { 2, 0, 3, 1 }, { 0, 3, 1, 2 } };

  if (!dir || !dir[0]) { printf("fontepref_lancamento: NUVIO_DADOS ausente; recusando\n"); return 2; }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) { printf("fontepref_lancamento: dados_dir diverge; recusando\n"); return 2; }

  // ---- 1. the fingerprint itself
#define LANC(file, esperado) do { Stream s_; fonte(&s_, "A", "", file, 1080); \
    fontepref_lancamento(&s_, l, sizeof l); \
    CONFERE(!strcmp(l, esperado), "lancamento(%s) = %s (esperado %s)", file, l, esperado); } while (0)
  LANC("The.Show.S01E05.1080p.WEB-DL.DDP5.1.H.264-NTb.mkv", "ntb|1080|webdl|avc|");
  LANC("Show.S01E05.2160p.BluRay.REMUX.HEVC.DV.TrueHD.Atmos-FraMeSToR.mkv", "framestor|2160|remux|hevc|dv");
  LANC("[SubsPlease] Show - 05 (1080p) [ABCD1234].mkv", "subsplease|1080|||");
  LANC("Movie.2023.1080p.BluRay.x264-SPARKS.mkv", "sparks|1080|bluray|avc|");
  LANC("4K.Show.S01E01.720p.HDTV.x264-GRP.mkv", "grp|720|hdtv|avc|");
  LANC("Show.S02E01.1080p.WEB-DL.mkv", "|1080|webdl||");
  LANC("Show.S01E05/Show.S01E05.2160p.WEB-DL.HDR10+.HEVC-GRP.mkv", "grp|2160|webdl|hevc|hdr10p");
  LANC("Show.S01E05.Some.Title.mkv", "");
  // The episode marker cuts the show away: two episodes agree.
  { Stream a, b; char la[FONTEPREF_LANC], lb[FONTEPREF_LANC];
    fonte(&a, "A", "", "The.Show.S01E02.1080p.WEB-DL.H.265-FLUX.mkv", 1080);
    fonte(&b, "A", "", "The.Show.S01E09.1080p.WEB-DL.H.265-FLUX.mkv", 1080);
    fontepref_lancamento(&a, la, sizeof la); fontepref_lancamento(&b, lb, sizeof lb);
    CONFERE(!strcmp(la, lb), "E02 e E09 do mesmo lancamento: mesma impressao (%s)", la); }

  // ---- 2. hand pick on E7 -> E2, E5, E9 and E1 pick the SAME release
  fontepref_definir_perfil(1);
  n = lista(v, 7, ORDENS[0]);
  stream_definir_lista(v, n);
  k = posDe(v, n, "FLUX");
  CONFERE(fontepref_guardar("tt4000001:1:7", &v[k]) == 1, "E7: FLUX escolhido a mao");
  for (ep = 0; ep < 4; ep++) {
    static const int EPS[4] = { 2, 5, 9, 1 };
    n = lista(v, EPS[ep], ORDENS[ep]);
    stream_definir_lista(v, n);
    k = fontepref_escolher("tt4000001:1:x");
    CONFERE(k >= 0 && strstr(v[k].arquivo, "-FLUX"), "E%d: o FLUX e o escolhido (posicao %d, ordem %d)",
            EPS[ep], k, ep);
  }
  // Before the fingerprint: in ORDENS[1] FLUX is last; the old rule took the first.
  // The release also survives a cold start (ninth field on disk).
  nTab = 0; nAuto = 0; carregado = 0;
  n = lista(v, 3, ORDENS[1]);
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt4000001:1:3");
  CONFERE(k == 3 && strstr(v[k].arquivo, "-FLUX"), "depois do arranque frio: FLUX (%d)", k);

  // ---- 3. the release is missing in this episode: the bingeGroup still counts
  { static const int SEM_FLUX[4] = { 3, 2, 0, 0 };
    n = lista(v, 10, SEM_FLUX);
    stream_definir_lista(v, n);
    k = fontepref_escolher("tt4000001:1:10");
    CONFERE(k == 0, "sem o FLUX: cai no mesmo bingeGroup, primeira da lista (%d)", k); }
  // ... and nothing of it at all: -1, the automatic pick takes over.
  fonte(&v[0], "Torrentio", "torrentio|1080p", "The.Show.S01E11.1080p.WEB-DL.H.264-NTb.mkv", 1080);
  fonte(&v[1], "Comet", "comet|x", "The.Show.S01E11.1080p.WEB-DL.H.265-FLUX.mkv", 1080);
  stream_definir_lista(v, 2);
  k = fontepref_escolher("tt4000001:1:11");
  CONFERE(k == -1, "outro addon, outro grupo: -1, o automatico assume (%d)", k);

  // ---- 4. a release match never pulls the wrong audio
  fontepref_definir_perfil(2);
  fonte(&v[0], "AIOStreams", "aio|a", "The.Show.S01E01.1080p.WEB-DL.H.265-FLUX.mkv", 1080);
  snprintf(v[0].descricao, sizeof v[0].descricao, "Dublado");
  stream_definir_lista(v, 1);
  CONFERE(fontepref_guardar("tt4000002:1:1", &v[0]) == 1, "DUB FLUX escolhido");
  fonte(&v[0], "AIOStreams", "aio|b", "The.Show.S01E02.1080p.WEB-DL.H.265-FLUX.mkv", 1080);
  stream_definir_lista(v, 1);
  k = fontepref_escolher("tt4000002:1:2");
  CONFERE(k == -1, "mesmo lancamento sem a marca DUB: nao casa (%d)", k);

  // ---- 5. the automatic memory also keeps the release
  fontepref_definir_perfil(3);
  n = lista(v, 1, ORDENS[0]);
  for (k = 0; k < n; k++) v[k].bingeGroup[0] = 0;   // addon without bingeGroup
  stream_definir_lista(v, n);
  CONFERE(fontepref_auto_guardar("tt4000003:1:1", &v[posDe(v, n, "SPARKS")]) == 1,
          "automatico tocou SPARKS (sem bingeGroup): guardado pelo lancamento");
  n = lista(v, 6, ORDENS[2]);
  for (k = 0; k < n; k++) v[k].bingeGroup[0] = 0;
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt4000003:1:6");
  CONFERE(k >= 0 && strstr(v[k].arquivo, "-SPARKS"), "E6: SPARKS de novo (%d)", k);
  CONFERE(fontepref_escolher_lista("tt4000003:1:6", v, n, NULL, NULL) == k,
          "a lista propria do proximo episodio concorda (%d)", k);
  // It survives a cold start (auto file v2, full record) ...
  nTab = 0; nAuto = 0; carregado = 0;
  k = fontepref_escolher("tt4000003:1:6");
  CONFERE(k >= 0 && strstr(v[k].arquivo, "-SPARKS"), "auto depois do arranque frio: SPARKS (%d)", k);
  // ... and is not a hand pick: no "Sua escolha anterior", no direct play in
  // "escolher a fonte ao reproduzir".
  CONFERE(!fontepref_tem("tt4000003") && fontepref_escolher_manual("tt4000003:1:6") == -1,
          "a do automatico nao conta como escolha a mao");
  // The hand pick is in charge: with one in force, what auto-play played is
  // not stored; once it has expired, it is.
  fontepref_definir_perfil(6);
  n = lista(v, 1, ORDENS[0]);
  stream_definir_lista(v, n);
  CONFERE(fontepref_guardar("tt4000006:1:1", &v[posDe(v, n, "NTb")]) == 1, "NTb a mao");
  CONFERE(fontepref_auto_guardar("tt4000006:1:2", &v[posDe(v, n, "GRP")]) == 0,
          "com a mao em vigor, o GRP que o automatico tocou nao e guardado");
  n = lista(v, 3, ORDENS[1]);
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt4000006:1:3");
  CONFERE(k >= 0 && strstr(v[k].arquivo, "-NTb"), "E3: a escolha a mao (NTb) (%d)", k);
  tabela[achar("tt4000006")].quandoS = (long long)time(NULL) - 200LL * 24 * 3600;
  CONFERE(fontepref_auto_guardar("tt4000006:1:3", &v[posDe(v, n, "GRP")]) == 1,
          "a mao venceu: agora o GRP que tocou e guardado");
  n = lista(v, 4, ORDENS[2]);
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt4000006:1:4");
  CONFERE(k >= 0 && strstr(v[k].arquivo, "-GRP"), "E4: o GRP (%d)", k);
  CONFERE(fontepref_guardar("tt4000006:1:4", &v[posDe(v, n, "FLUX")]) == 1, "nova escolha a mao: FLUX");
  k = fontepref_escolher("tt4000006:1:4");
  CONFERE(k >= 0 && strstr(v[k].arquivo, "-FLUX"), "e ela manda de novo (%d)", k);
  // A fake 4K does not pull the remembered release off.
  fontepref_definir_perfil(7);
  n = lista(v, 1, ORDENS[0]);
  stream_definir_lista(v, n);
  CONFERE(fontepref_auto_guardar("tt4000007:1:1", &v[posDe(v, n, "SPARKS")]) == 1, "SPARKS 1080p tocou");
  n = lista(v, 2, ORDENS[3]);
  fonte(&v[4], "AIOStreams", "aio|2160p", "The.Show.S01E02.2160p.AI.Upscale.HEVC-FAKE.mkv", 2160);
  stream_definir_lista(v, 5);
  k = fontepref_escolher("tt4000007:1:2");
  CONFERE(k >= 0 && strstr(v[k].arquivo, "-SPARKS"), "uma 4K a mais nao tira o SPARKS (%d)", k);

  // ---- 6. the settings, as app.c passes them: "Prefer binge group" = g
  // (the hand pick for the whole show AND the automatic memory), "across
  // sessions" = e: fontepref_configurar(g, 0, g, e).
  fontepref_definir_perfil(1);
  n = lista(v, 4, ORDENS[1]);
  stream_definir_lista(v, n);
  fontepref_configurar(0, 0, 0, 1);
  CONFERE(fontepref_escolher("tt4000001:1:4") == -1 && !fontepref_tem("tt4000001"),
          "Preferir o grupo desligado: a escolha a mao nao conta");
  fontepref_definir_perfil(3);
  n = lista(v, 6, ORDENS[2]);
  for (k = 0; k < n; k++) v[k].bingeGroup[0] = 0;
  stream_definir_lista(v, n);
  CONFERE(fontepref_escolher("tt4000003:1:6") == -1, "e a memoria do automatico parada");
  fontepref_configurar(1, 0, 1, 1);
  k = fontepref_escolher("tt4000003:1:6");
  CONFERE(k >= 0 && strstr(v[k].arquivo, "-SPARKS"), "religado: o automatico volta (%d)", k);
  fontepref_definir_perfil(1);
  n = lista(v, 4, ORDENS[1]);
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt4000001:1:4");
  CONFERE(k >= 0 && strstr(v[k].arquivo, "-FLUX"), "religado: a escolha a mao volta (nada foi apagado) (%d)", k);
  tabela[achar("tt4000001")].quandoS = (long long)time(NULL) - 200LL * 24 * 3600;
  CONFERE(fontepref_escolher("tt4000001:1:4") == -1, "escolha de 200 dias venceu (prazo de fabrica, 180)");
  tabela[achar("tt4000001")].quandoS = (long long)time(NULL);
  // Across sessions off: what this process picked or played counts, a cold
  // start does not, for the hand pick and for the automatic memory.
  fontepref_configurar(1, 0, 1, 0);
  k = fontepref_escolher("tt4000001:1:4");
  CONFERE(k >= 0 && strstr(v[k].arquivo, "-FLUX"), "entre sessoes desligado: a escolha desta sessao vale (%d)", k);
  fontepref_definir_perfil(9); fontepref_definir_perfil(1);   // a profile switch rereads the file
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt4000001:1:4");
  CONFERE(k >= 0 && strstr(v[k].arquivo, "-FLUX"), "trocar de perfil nao tira a marca desta sessao (%d)", k);
  nSessao = 0; fontepref_definir_perfil(9); fontepref_definir_perfil(1);   // = a new app start
  CONFERE(fontepref_escolher("tt4000001:1:4") == -1, "a escolha a mao lida do disco (sessao anterior) nao");
  CONFERE(fontepref_guardar("tt4000001:1:4", &v[posDe(v, n, "FLUX")]) == 0, "escolher de novo a mesma...");
  k = fontepref_escolher("tt4000001:1:5");
  CONFERE(k >= 0 && strstr(v[k].arquivo, "-FLUX"), "...a confirma nesta sessao (%d)", k);
  fontepref_definir_perfil(3);
  n = lista(v, 6, ORDENS[2]);
  for (k = 0; k < n; k++) v[k].bingeGroup[0] = 0;
  stream_definir_lista(v, n);
  CONFERE(fontepref_escolher("tt4000003:1:6") == -1, "e a do automatico lida do disco tambem nao");
  fontepref_configurar(1, 0, 1, 1);
  k = fontepref_escolher("tt4000003:1:6");
  CONFERE(k >= 0 && strstr(v[k].arquivo, "-SPARKS"), "religado: volta (%d)", k);

  // ---- 7. anime by kitsu id survives a cold start
  fontepref_definir_perfil(5);
  fonte(&v[0], "AIOStreams", "", "[SubsPlease] Show - 01 (1080p) [ABCD1234].mkv", 1080);
  fonte(&v[1], "AIOStreams", "", "[Erai-raws] Show - 01 [1080p][Multiple Subtitle].mkv", 1080);
  stream_definir_lista(v, 2);
  CONFERE(fontepref_guardar("kitsu:41370:1", &v[0]) == 1, "kitsu: SubsPlease a mao");
  nTab = 0; nAuto = 0; carregado = 0;
  fonte(&v[0], "AIOStreams", "", "[Erai-raws] Show - 07 [1080p][Multiple Subtitle].mkv", 1080);
  fonte(&v[1], "AIOStreams", "", "[SubsPlease] Show - 07 (1080p) [9F00AA11].mkv", 1080);
  stream_definir_lista(v, 2);
  k = fontepref_escolher("kitsu:41370:7");
  CONFERE(k == 1, "kitsu depois do arranque frio: SubsPlease no ep 7 (%d)", k);

  // ---- 8. Next: the bingeGroup of the source PLAYING NOW comes first
  fontepref_definir_perfil(6);
  fontepref_configurar(1, 0, 1, 1);
  fonte(&v[0], "AIOStreams", "aio|NTb", "The.Show.S01E01.1080p.WEB-DL.DDP5.1.H.264-NTb.mkv", 1080);
  fonte(&v[1], "AIOStreams", "aio|FLUX", "The.Show.S01E01.1080p.WEB-DL.DDP5.1.H.265-FLUX.mkv", 1080);
  fonte(&v[2], "Torrentio", "tio|SPARKS", "The.Show.S01E01.1080p.BluRay.x264-SPARKS.mkv", 1080);
  stream_definir_lista(v, 3);
  CONFERE(fontepref_guardar("tt4000009:1:1", &v[1]) == 1, "next: FLUX picked by hand on E1");
  fontepref_tocando("tt4000009:1:1", &v[2]);   // a fallback played SPARKS instead
  fonte(&v[0], "AIOStreams", "aio|FLUX", "The.Show.S01E02.1080p.WEB-DL.DDP5.1.H.265-FLUX.mkv", 1080);
  fonte(&v[1], "Torrentio", "tio|SPARKS", "The.Show.S01E02.1080p.BluRay.x264-SPARKS.mkv", 1080);
  fonte(&v[2], "Torrentio", "tio|SPARKS", "The.Show.S01E02.720p.BluRay.x264-SPARKS.mkv", 720);
  CONFERE(fontepref_escolher_lista("tt4000009:1:2", v, 3, NULL, NULL) == 0, "next: title memory alone = FLUX");
  k = fontepref_escolher_lista_next("tt4000009:1:2", v, 3, NULL, NULL);
  CONFERE(k == 1, "next: the playing source's bingeGroup wins, first in addon order (%d)", k);
  k = fontepref_escolher_lista_next("tt4000009:1:2", v, 3, soForte, NULL);
  CONFERE(k == 1, "next: a bingeGroup match is passed as forte to semPerda (%d)", k);
  stream_definir_lista(v, 3);
  CONFERE(fontepref_binge_tocando("tt4000009:1:2") == 1, "next: same on the installed list");
  CONFERE(fontepref_binge_tocando("tt4000010:1:2") == -1, "next: another title never follows it");
  fontepref_configurar(1, 0, 0, 1);
  k = fontepref_escolher_lista_next("tt4000009:1:2", v, 3, NULL, NULL);
  CONFERE(k != 1, "next: Prefer binge group off ignores the playing source (%d)", k);
  fontepref_configurar(1, 0, 1, 1);
  v[1].bingeGroup[0] = 0; v[2].bingeGroup[0] = 0;
  k = fontepref_escolher_lista_next("tt4000009:1:2", v, 3, NULL, NULL);
  CONFERE(k == 0, "next: no source with that bingeGroup falls back to the title memory (%d)", k);
  // Shared groups must follow the playing release, independently of title memory.
  n = lista(v, 1, ORDENS[0]);
  fontepref_tocando("tt4000009:1:1", &v[1]); // FLUX plays; title memory below becomes NTb.
  fontepref_guardar("tt4000009:1:1", &v[0]);
  for (ep = 2; ep <= 4; ep++) {
    n = lista(v, ep, ORDENS[ep - 1]);
    k = fontepref_escolher_lista_next("tt4000009:1:2", v, n, soForte, NULL);
    CONFERE(k == posDe(v, n, "FLUX"), "next: shared group keeps playing FLUX on E%d (%d)", ep, k);
    stream_definir_lista(v, n);
    CONFERE(fontepref_binge_tocando("tt4000009:1:2") == posDe(v, n, "FLUX"),
            "next: installed shared group keeps FLUX on E%d", ep);
  }
  // Same release, different audio: the playing audio breaks the group tie.
  fonte(&v[0], "AIOStreams", "aio|1080p", "The.Show.S01E01.1080p.WEB-DL.H.265-FLUX.mkv", 1080);
  strcpy(v[0].descricao, "DUBLADO");
  fontepref_tocando("tt4000009:1:1", &v[0]);
  fonte(&v[0], "AIOStreams", "aio|1080p", "The.Show.S01E02.1080p.WEB-DL.H.265-FLUX.mkv", 1080);
  v[1] = v[0]; strcpy(v[1].descricao, "DUBLADO");
  CONFERE(fontepref_escolher_lista_next("tt4000009:1:2", v, 2, NULL, NULL) == 1,
          "next: shared release keeps playing audio");
  v[1] = v[0];
  CONFERE(fontepref_escolher_lista_next("tt4000009:1:2", v, 2, NULL, NULL) == 0,
          "next: equal identity keeps addon order");

  // the automatic memory: a bingeGroup match ignores the evidence rule (forte)
  fontepref_definir_perfil(7);
  fonte(&v[0], "AIOStreams", "aio|NTb", "The.Show.S01E01.1080p.WEB-DL.DDP5.1.H.264-NTb.mkv", 1080);
  fonte(&v[1], "AIOStreams", "aio|FLUX", "The.Show.S01E01.1080p.WEB-DL.DDP5.1.H.265-FLUX.mkv", 1080);
  stream_definir_lista(v, 2);
  CONFERE(fontepref_auto_guardar("tt4000011:1:1", &v[1]) == 1, "auto: FLUX played on E1");
  k = fontepref_escolher_lista("tt4000011:1:2", v, 2, soForte, NULL);
  CONFERE(k == 1, "auto memory: the bingeGroup match reaches semPerda as forte (%d)", k);

  printf(falhas ? "fontepref_lancamento: %d falha(s)\n" : "fontepref_lancamento: tudo ok\n", falhas);
  return falhas ? 1 : 0;
}
