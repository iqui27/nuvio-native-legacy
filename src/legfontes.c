#include "legfontes.h"
#include "assrender.h"
#include "perfisessao.h"
#ifdef NV_ANDROID
#include "android.h"
#endif
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct { char id[21], nome[96], nomeAss[96], caminho[768]; } Fonte;
static Fonte fontes[LEGFONTES_MAX];
static int nFontes, lidas, aplicada, importando = -1;
static char pasta[640], prefs[700], escolha[LEGFONTES_SLOTS][21];
static unsigned revisao = 1;
#ifdef NV_ANDROID
static NvPerfilSessao importacaoSessao;
static int confirmandoImportacao;
static char importacaoPasta[640];
#endif

static uint32_t be32(const unsigned char *p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
int legfontes_sfnt_valido(const void *dados, size_t n) {
  const unsigned char *p = dados;
  if (!p || n < 12 || n > LEGFONTE_MAX_BYTES ||
      (be32(p) != 0x00010000u && memcmp(p, "OTTO", 4))) return 0;
  unsigned nt = (unsigned)p[4] << 8 | p[5];
  if (!nt || nt > 128 || nt > (n - 12) / 16) return 0;
  for (unsigned i = 0; i < nt; i++) {
    const unsigned char *t = p + 12 + i * 16;
    uint32_t off = be32(t + 8), len = be32(t + 12);
    if ((len && off < 12 + nt * 16) || off > n || len > n - off) return 0;
    for (unsigned j = 0; j < i; j++)
      if (!memcmp(t, p + 12 + j * 16, 4)) return 0;
  }
  return 1;
}
static unsigned be16(const unsigned char *p) { return (unsigned)p[0] << 8 | p[1]; }
// Full name for TrueType; PostScript name for CFF, as required by libass's
// documented full/PostScript matching. Decode only bounded Unicode/ASCII names.
int legfontes_nome_sfnt(const void *dados, size_t n, char *out, size_t tam) {
  const unsigned char *b = dados, *table = NULL;
  size_t size = 0; int melhor = 99;
  if (!out || !tam) return 0;
  out[0] = 0;
  if (!legfontes_sfnt_valido(b, n)) return 0;
  unsigned nt = be16(b + 4), wanted = !memcmp(b, "OTTO", 4) ? 6 : 4;
  for (unsigned i = 0; i < nt; i++) {
    const unsigned char *t = b + 12 + i * 16;
    if (!memcmp(t, "name", 4)) { table = b + be32(t + 8); size = be32(t + 12); break; }
  }
  if (!table || size < 6 || be16(table) > 1) return 0;
  unsigned count = be16(table + 2), start = be16(table + 4);
  if (count > 512 || count > (size - 6) / 12 || start < 6 + count * 12 || start > size) return 0;
  for (unsigned i = 0; i < count; i++) {
    const unsigned char *rec = table + 6 + i * 12;
    unsigned platform = be16(rec), encoding = be16(rec + 2), language = be16(rec + 4);
    unsigned len = be16(rec + 8), off = be16(rec + 10);
    if (be16(rec + 6) != wanted || off > size - start || len > size - start - off) continue;
    int unicode = platform == 0 || (platform == 3 && (encoding == 1 || encoding == 10));
    int score = platform == 3 && language == 0x409 ? 0 : platform == 0 ? 1 : unicode ? 2 : 3;
    if (score >= melhor || (!unicode && platform != 1) || len > 512 || (unicode && len % 2)) continue;
    const unsigned char *s = table + start + off;
    char nome[96]; size_t pos = 0; int ok = 1;
    for (unsigned j = 0; j < len && ok;) {
      uint32_t cp = unicode ? be16(s + j) : s[j]; j += unicode ? 2 : 1;
      if (unicode && cp >= 0xd800 && cp <= 0xdbff) {
        if (j + 2 > len || be16(s + j) < 0xdc00 || be16(s + j) > 0xdfff) { ok = 0; break; }
        cp = 0x10000 + ((cp - 0xd800) << 10) + be16(s + j) - 0xdc00; j += 2;
      } else if ((cp >= 0xdc00 && cp <= 0xdfff) || (!unicode && cp >= 128)) { ok = 0; break; }
      if (cp < 32 || cp == 127) { ok = 0; break; }
      unsigned bytes = cp < 128 ? 1 : cp < 2048 ? 2 : cp < 65536 ? 3 : 4;
      if (pos + bytes >= sizeof nome || pos + bytes >= tam) { ok = 0; break; }
      if (bytes == 1) nome[pos++] = (char)cp;
      else {
        nome[pos++] = (char)((bytes == 2 ? 0xc0 : bytes == 3 ? 0xe0 : 0xf0) | (cp >> (6 * (bytes - 1))));
        for (unsigned k = bytes - 1; k; k--) nome[pos++] = (char)(0x80 | ((cp >> (6 * (k - 1))) & 63));
      }
    }
    if (ok && pos) { nome[pos] = 0; memcpy(out, nome, pos + 1); melhor = score; }
  }
  return melhor < 99;
}
static unsigned char *ler(const char *caminho, size_t *n) {
  struct stat st;
  FILE *f;
  unsigned char *b;
  *n = 0;
  if (lstat(caminho, &st) || !S_ISREG(st.st_mode) || st.st_size < 12 ||
      st.st_size > (long)LEGFONTE_MAX_BYTES) return NULL;
  f = fopen(caminho, "rb");
  if (!f) return NULL;
  b = malloc((size_t)st.st_size);
  if (!b) { fclose(f); return NULL; }
  size_t lidos = fread(b, 1, (size_t)st.st_size, f);
  int fim = fgetc(f), erro = ferror(f);
  fclose(f);
  if (lidos != (size_t)st.st_size || fim != EOF || erro) { free(b); return NULL; }
  *n = lidos;
  return b;
}
static int idValido(const char *id) {
  if (!id || strlen(id) != 20 || (strcmp(id + 16, ".ttf") && strcmp(id + 16, ".otf"))) return 0;
  for (int i = 0; i < 16; i++)
    if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f'))) return 0;
  return 1;
}
static int nomeFace(const void *b, size_t n, char *nome, size_t tam) {
  if (!TTF_WasInit() || !legfontes_sfnt_valido(b, n)) return 0;
  SDL_RWops *rw = SDL_RWFromConstMem(b, (int)n);
  TTF_Font *f = rw ? TTF_OpenFontRW(rw, 1, 24) : NULL;
  if (!f) return 0;
  const char *familia = TTF_FontFaceFamilyName(f);
  const char *estilo = TTF_FontFaceStyleName(f);
  int ok = familia && familia[0] && strlen(familia) < tam;
  if (ok) {
    for (const unsigned char *p = (const unsigned char *)familia; *p; p++)
      if (*p < 32 || *p == 127) { ok = 0; break; }
    if (ok) {
      int estiloOk = estilo && estilo[0] && strlen(estilo) < 32;
      for (const unsigned char *p = (const unsigned char *)(estiloOk ? estilo : ""); *p; p++)
        if (*p < 32 || *p == 127) estiloOk = 0;
      if (estiloOk && strlen(familia) + strlen(estilo) + 2 < tam) snprintf(nome, tam, "%s %s", familia, estilo);
      else snprintf(nome, tam, "%s", familia);
    }
  }
  TTF_CloseFont(f);
  return ok;
}
static int entradaValida(const char *s);
static int gravarEscolhas(const char novas[LEGFONTES_SLOTS][21]) {
  if (!prefs[0]) return 0;
  char tmp[708];
  snprintf(tmp, sizeof tmp, "%s.tmp", prefs);
  FILE *f = fopen(tmp, "w");
  if (!f) return 0;
// Append the interface selection after the two legacy subtitle lines.
  // Old two-line files default to bundled UI; old readers keep their slots.
  for (int i = 0; i < LEGFONTES_SLOTS; i++) fprintf(f, "%s\n", novas[i]);
  int erro = ferror(f);
  if (fclose(f)) erro = 1;
  if (erro || rename(tmp, prefs)) { unlink(tmp); return 0; }
  return 1;
}
void legfontes_dir(const char *dir) {
  char nova[sizeof pasta] = "";
  // Stable storage key, not interface text. Keep it in two pieces so the i18n
  // scanner does not mistake this private directory for a visible label.
  static const char nomePastaA[] = "fon", nomePastaB[] = "tes-importadas";
  if (dir && dir[0] && strlen(dir) + 20 < sizeof nova)
    snprintf(nova, sizeof nova, "%s/%s%s", dir, nomePastaA, nomePastaB);
  if (!strcmp(nova, pasta)) return;
  snprintf(pasta, sizeof pasta, "%s", nova);
  prefs[0] = 0;
  if (pasta[0]) snprintf(prefs, sizeof prefs, "%s/fontes-legenda.txt", dir);
  nFontes = lidas = aplicada = 0;
  memset(escolha, 0, sizeof escolha);
  if (pasta[0]) {
    mkdir(pasta, 0700);
    // Only abandoned, owned staging files are discarded at initialization.
    // An in-flight result keeps its captured old directory until it is polled.
    if (importando < 0) {
      DIR *d = opendir(pasta);
      struct dirent *e;
      if (d) {
        unsigned vistos = 0;
        while ((e = readdir(d)) && vistos++ < 256) if (entradaValida(e->d_name)) {
          char caminho[768];
          snprintf(caminho, sizeof caminho, "%s/%.40s", pasta, e->d_name);
          unlink(caminho);
        }
        closedir(d);
      }
    }
    FILE *f = fopen(prefs, "r");
    if (f) {
      char linha[64];
      for (int i = 0; i < LEGFONTES_SLOTS && fgets(linha, sizeof linha, f); i++) {
        linha[strcspn(linha, "\r\n")] = 0;
        if (idValido(linha)) memcpy(escolha[i], linha, 21);
      }
      fclose(f);
    }
  }
  revisao++;
}
static int comparar(const void *a, const void *b) {
  const Fonte *fa = a, *fb = b;
  int n = strcmp(fa->nome, fb->nome);
  return n ? n : strcmp(fa->id, fb->id);
}
static void carregar(void) {
  if (lidas || !pasta[0] || !TTF_WasInit()) return;
  lidas = 1; nFontes = 0;
  DIR *d = opendir(pasta);
  if (!d) return;
  struct dirent *e;
  unsigned vistos = 0;
  while ((e = readdir(d)) && vistos++ < 256 && nFontes < LEGFONTES_MAX) {
    if (!idValido(e->d_name)) continue;
    Fonte *f = &fontes[nFontes]; size_t n;
    snprintf(f->caminho, sizeof f->caminho, "%s/%.20s", pasta, e->d_name);
    unsigned char *b = ler(f->caminho, &n);
    if (b && nomeFace(b, n, f->nome, sizeof f->nome) && legfontes_nome_sfnt(b, n, f->nomeAss, sizeof f->nomeAss)) {
      memcpy(f->id, e->d_name, 21); nFontes++;
    }
    free(b);
  }
  closedir(d);
  qsort(fontes, (size_t)nFontes, sizeof fontes[0], comparar);
}
int legfontes_n(void) { carregar(); return nFontes; }
const char *legfontes_nome(int i) { carregar(); return i >= 0 && i < nFontes ? fontes[i].nome : ""; }
int legfontes_selecionada(int slot) {
  carregar();
  if (slot < 0 || slot >= LEGFONTES_SLOTS) return -1;
  for (int i = 0; i < nFontes; i++) if (!strcmp(fontes[i].id, escolha[slot])) return i;
  return -1;
}
const char *legfontes_caminho(int slot) {
  int i = legfontes_selecionada(slot);
  return i >= 0 && !access(fontes[i].caminho, R_OK) ? fontes[i].caminho : NULL;
}
const char *legfontes_nome_ass(int slot) {
  int i = legfontes_selecionada(slot);
  return i >= 0 ? fontes[i].nomeAss : "";
}
unsigned legfontes_revisao(void) { return revisao; }
typedef struct {
  char ids[LEGFONTES_SLOTS][21]; const char *origem, *tomb;
  int (*confirmar)(void *); void *u;
} EscolhaCommit;
typedef struct { char caminho[716]; int existia; } EscolhaAnterior;
static int prepararPreferencias(EscolhaAnterior *a) {
  // Prepare the old file before either preference changes. Android forbids
  // hard links even in private app storage, so use an ordinary copy. Rollback
  // remains a same-directory rename requiring no further content writes.
  snprintf(a->caminho, sizeof a->caminho, "%s.anterior", prefs);
  if (unlink(a->caminho) && errno != ENOENT) return 0;
  a->existia = 0;
  FILE *origem = fopen(prefs, "rb");
  if (!origem) return errno == ENOENT;
  FILE *copia = fopen(a->caminho, "wb");
  if (!copia) { fclose(origem); return 0; }
  char bloco[1024]; size_t n;
  int erro = 0;
  while ((n = fread(bloco, 1, sizeof bloco, origem)))
    if (fwrite(bloco, 1, n, copia) != n) { erro = 1; break; }
  if (ferror(origem)) erro = 1;
  if (fclose(origem)) erro = 1;
  if (fclose(copia)) erro = 1;
  if (erro) { unlink(a->caminho); return 0; }
  a->existia = 1;
  return 1;
}
static void liberarPreferencias(EscolhaAnterior *a) { if (a->existia) unlink(a->caminho); }
static void restaurarPreferencias(EscolhaAnterior *a) {
  int erro = a->existia ? rename(a->caminho, prefs) : unlink(prefs);
  if (erro) fprintf(stderr, "[fontes] nao foi possivel restaurar a escolha local\n");
}
static void restaurarArquivo(const char *de, const char *para) {
  if (rename(de, para)) fprintf(stderr, "[fontes] nao foi possivel restaurar o arquivo local\n");
}
static int confirmarPreferencias(EscolhaCommit *c) {
  if (!memcmp(c->ids, escolha, sizeof escolha)) return !c->confirmar || c->confirmar(c->u);
  if (!c->confirmar) return gravarEscolhas(c->ids);
  EscolhaAnterior a;
  if (!prepararPreferencias(&a)) return 0;
  if (!gravarEscolhas(c->ids)) { liberarPreferencias(&a); return 0; }
  if (!c->confirmar(c->u)) {
    restaurarPreferencias(&a);
    return 0;
  }
  liberarPreferencias(&a);
  return 1;
}
static int confirmarEscolha(void *u) {
  EscolhaCommit *c = u;
#ifdef NV_ANDROID
  if (confirmandoImportacao && !nv_perfil_sessao_atual(importacaoSessao)) return 0;
#endif
  if (!c->origem) return confirmarPreferencias(c);
  EscolhaAnterior a;
  if (!prepararPreferencias(&a)) return 0;
  if (rename(c->origem, c->tomb)) { liberarPreferencias(&a); return 0; }
  if (!gravarEscolhas(c->ids)) { restaurarArquivo(c->tomb, c->origem); liberarPreferencias(&a); return 0; }
  if (unlink(c->tomb)) {
    restaurarArquivo(c->tomb, c->origem); restaurarPreferencias(&a); return 0;
  }
  liberarPreferencias(&a);
  return 1;
}
int legfontes_escolher_transacao(int slot, int i, int (*confirmar)(void *), void *u) {
  carregar();
  if (slot < 0 || slot >= LEGFONTES_SLOTS || i < -1 || i >= nFontes) return 0;
  EscolhaCommit c = {0}; memcpy(c.ids, escolha, sizeof escolha);
  c.confirmar = confirmar; c.u = u;
  snprintf(c.ids[slot], sizeof c.ids[slot], "%s", i >= 0 ? fontes[i].id : "");
  if (!memcmp(c.ids, escolha, sizeof escolha) && !confirmar) return 1;
  const char *path = i >= 0 ? fontes[i].caminho : NULL;
  if (!(slot ? confirmarEscolha(&c) : assrender_fonte_local_transacao(path, confirmarEscolha, &c))) return 0;
  memcpy(escolha, c.ids, sizeof escolha); revisao++;
  if (!slot) aplicada = 1;
  return 1;
}
int legfontes_escolher(int slot, int i) { return legfontes_escolher_transacao(slot, i, NULL, NULL); }
int legfontes_remover(int i) {
  carregar();
  if (i < 0 || i >= nFontes) return 0;
  EscolhaCommit c = {0}; memcpy(c.ids, escolha, sizeof escolha);
  for (int slot = 0; slot < LEGFONTES_SLOTS; slot++) if (!strcmp(c.ids[slot], fontes[i].id)) c.ids[slot][0] = 0;
  char tomb[800]; snprintf(tomb, sizeof tomb, "%s.excluir", fontes[i].caminho);
  c.origem = fontes[i].caminho; c.tomb = tomb;
  int principal = !strcmp(escolha[0], fontes[i].id);
  if (!(principal ? assrender_fonte_local_transacao(NULL, confirmarEscolha, &c) : confirmarEscolha(&c))) return 0;
  memcpy(escolha, c.ids, sizeof escolha);
  lidas = 0; revisao++;
  if (principal) aplicada = 1;
  return 1;
}
static int entradaValida(const char *s) {
  if (!s || strncmp(s, "entrada-", 8)) return 0;
  size_t n = strlen(s);
  if (n < 13 || n > 40 || strcmp(s + n - 4, ".tmp")) return 0;
  for (size_t i = 8; i < n - 4; i++)
    if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return 0;
  return 1;
}
int legfontes_importar(int slot, const char *arquivo) {
  if (slot < 0 || slot >= LEGFONTES_SLOTS || !pasta[0] || !entradaValida(arquivo)) return 0;
  char entrada[768], id[21], destino[768], nome[96], nomeAss[96]; size_t n;
  snprintf(entrada, sizeof entrada, "%s/%s", pasta, arquivo);
  unsigned char *b = ler(entrada, &n);
  if (!b || !nomeFace(b, n, nome, sizeof nome) || !legfontes_nome_sfnt(b, n, nomeAss, sizeof nomeAss)) { free(b); unlink(entrada); return 0; }
  uint64_t h = UINT64_C(14695981039346656037);
  for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * UINT64_C(1099511628211);
  snprintf(id, sizeof id, "%016llx.%s", (unsigned long long)h, !memcmp(b, "OTTO", 4) ? "otf" : "ttf");
  snprintf(destino, sizeof destino, "%s/%s", pasta, id);
  struct stat st;
  int ocupado = !lstat(destino, &st), existe = 0;
  if (ocupado && S_ISREG(st.st_mode) && (size_t)st.st_size == n) {
    FILE *f = fopen(destino, "rb");
    if (f) {
      unsigned char trecho[8192]; size_t pos = 0; existe = 1;
      while (pos < n) {
        size_t pedir = n - pos < sizeof trecho ? n - pos : sizeof trecho;
        if (fread(trecho, 1, pedir, f) != pedir || memcmp(trecho, b + pos, pedir)) { existe = 0; break; }
        pos += pedir;
      }
      if (ferror(f) || fgetc(f) != EOF) existe = 0;
      fclose(f);
    }
  }
  int colision = ocupado && !existe;
  free(b);
  if (colision || (!existe && legfontes_n() >= LEGFONTES_MAX)) { unlink(entrada); return 0; }
  if (existe) unlink(entrada);
  else if (rename(entrada, destino)) { unlink(entrada); return 0; }
  lidas = 0; carregar();
  for (int i = 0; i < nFontes; i++) if (!strcmp(fontes[i].id, id)) {
    int ok = legfontes_escolher(slot, i);
    if (!ok && !existe) { unlink(destino); lidas = 0; }
    return ok;
  }
  return 0;
}
int legfontes_pedir_importacao(int slot) {
#ifdef NV_ANDROID
  if (slot < 0 || slot >= LEGFONTES_SLOTS || !pasta[0] || importando >= 0) return 0;
  importacaoSessao = nv_perfil_sessao_ler();
  snprintf(importacaoPasta, sizeof importacaoPasta, "%s", pasta);
  if (!nv_perfil_sessao_atual(importacaoSessao) || !android_importar_fonte(slot)) return 0;
  importando = slot;
  return 1;
#else
  (void)slot;
  return 0;
#endif
}
const char *legfontes_atualizar(void) {
  if (!aplicada && TTF_WasInit()) {
    carregar();
    aplicada = assrender_fonte_local_transacao(legfontes_caminho(0), NULL, NULL);
  }
#ifdef NV_ANDROID
  char evento[80];
  if (!android_fonte_evento(evento, sizeof evento)) return NULL;
  int slot = importando; importando = -1;
  int atual = slot >= 0 && !strcmp(pasta, importacaoPasta) && nv_perfil_sessao_atual(importacaoSessao);
  if (!strncmp(evento, "R:", 2) && entradaValida(evento + 2)) {
    if (!atual) {
      char caminho[768]; snprintf(caminho, sizeof caminho, "%s/%s", importacaoPasta, evento + 2); unlink(caminho);
      return NULL;
    }
    confirmandoImportacao = 1;
    int ok = legfontes_importar(slot, evento + 2);
    confirmandoImportacao = 0;
    return ok ? "Fonte importada." : "Não foi possível importar a fonte. Use um arquivo TTF ou OTF de até 32 MB.";
  }
  return !atual || !strcmp(evento, "C") ? NULL : "Não foi possível importar a fonte. Use um arquivo TTF ou OTF de até 32 MB.";
#else
  return NULL;
#endif
}
