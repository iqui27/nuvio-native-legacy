// Ver gifcolecao.h (#141).
#include "gifcolecao.h"
#include <stdio.h>
#include <string.h>

int gifcol_eh_gif(const unsigned char *m) {
  return m && m[0] == 'G' && m[1] == 'I' && m[2] == 'F' && m[3] == '8';
}

const char *gifcol_fonte(const char *focusGif, const char *capa,
                         const unsigned char *magicaCapa, int *daCapa) {
  if (daCapa) *daCapa = 0;
  if (focusGif && focusGif[0]) return focusGif;
  // A CAPA SO QUANDO OS BYTES DELA SAO GIF. Pedir o arquivo de toda capa em
  // foco "para ver" custaria um fopen por quadro e, no Tizen, nada: la so GIF
  // vira arquivo, a capa JPEG/WebP nunca teria arquivo para achar.
  if (capa && capa[0] && gifcol_eh_gif(magicaCapa)) {
    if (daCapa) *daCapa = 1;
    return capa;
  }
  return NULL;
}

GcMotivo gifcol_sem_fonte(const char *capa, const unsigned char *magicaCapa) {
  if (!capa || !capa[0]) return GC_SEM_GIF;
  // Capa do PACOTE (caminho local) nao passa pelo fio de rede, entao nunca
  // tera magica — e o pacote nao traz capa GIF (o importador faz JPEG).
  if (strncmp(capa, "http://", 7) && strncmp(capa, "https://", 8)) return GC_SEM_GIF;
  return magicaCapa ? GC_SEM_GIF : GC_CAPA_AINDA;
}

GcMotivo gifcol_motivo_arquivo(const char *caminho, int animado, unsigned char magica[4]) {
  unsigned char m[4] = {0, 0, 0, 0};
  FILE *f;
  if (animado > 0) return GC_ANIMA;
  if (caminho && (f = fopen(caminho, "rb")) != NULL) {
    if (fread(m, 1, 4, f) != 4) memset(m, 0, 4);
    fclose(f);
  }
  if (magica) memcpy(magica, m, 4);
  // GIF8 que gif_animado recusou: um quadro so (ou truncado antes do segundo
  // fechar, que para quem olha e a mesma coisa: uma foto).
  return gifcol_eh_gif(m) ? GC_UM_QUADRO : GC_FORMATO;
}

void gifcol_sanear(const char *url, char *dst, size_t tam) {
  const char *p, *host, *hostFim, *fim, *seg;
  size_t hn, sn;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!url || !url[0]) return;
  p = strstr(url, "://");
  host = p ? p + 3 : url;
  // Tudo ate o primeiro / ? ou #: e o "usuario:senha@host:porta".
  hostFim = host + strcspn(host, "/?#");
  { const char *arroba = NULL, *q;
    for (q = host; q < hostFim; q++) if (*q == '@') arroba = q;
    if (arroba) host = arroba + 1; }
  hn = (size_t)(hostFim - host);
  // O caminho acaba na query ou no fragmento. O ultimo trecho nao vazio.
  fim = hostFim + strcspn(hostFim, "?#");
  while (fim > hostFim && fim[-1] == '/') fim--;
  seg = fim;
  while (seg > hostFim && seg[-1] != '/') seg--;
  sn = (size_t)(fim - seg);
  if (sn > 60) sn = 60;
  if (hn > 60) hn = 60;
  if (sn) snprintf(dst, tam, "%.*s/%.*s", (int)hn, host, (int)sn, seg);
  else    snprintf(dst, tam, "%.*s", (int)hn, host);
}

const char *gifcol_motivo_texto(GcMotivo m) {
  switch (m) {
    case GC_ANIMA:      return "anima";
    case GC_SEM_GIF:    return "sem focusGifUrl e a capa nao e GIF";
    case GC_CAPA_AINDA: return "sem focusGifUrl e a capa ainda nao chegou";
    case GC_ARQ_AINDA:  return "arquivo do GIF ainda nao chegou";
    case GC_SUMIU:      return "veio GIF mas o arquivo saiu do disco";
    case GC_FORMATO:    return "o arquivo nao e GIF";
    case GC_UM_QUADRO:  return "GIF de 1 quadro";
    case GC_ORCAMENTO:  return "recusado pelo orcamento de animacao (ver a linha [gif] acima)";
    case GC_REDUZIDAS:  return "animacoes reduzidas";
    case GC_APARELHO:   return "este aparelho nao anima GIF (webOS ou TV de 1 GB)";
  }
  return "?";
}

void gifcol_foco_iniciar(GcFoco *e) {
  memset(e, 0, sizeof *e);
  e->ultimo = -1;
  e->anima = -1;
}

int gifcol_focar(GcFoco *e, int id, const char *fonte, unsigned agora) {
  // A FONTE TAMBEM CONTA: a mesma pasta pode ganhar fonte depois do foco (a
  // capa chegou e era GIF), e ai `anima` foi respondido para OUTRO arquivo.
  const char *f = fonte ? fonte : "";
  if (e->ultimo == id && !strcmp(e->fonte, f)) return 0;
  e->ultimo = id;
  e->desde = agora;
  e->anima = -1;
  e->motivoArq = GC_ANIMA;
  memset(e->magicaArq, 0, sizeof e->magicaArq);
  snprintf(e->fonte, sizeof e->fonte, "%s", f);
  return 1;
}

// Cartazes que ja falaram nesta sessao. 256 = COL_MAX: mais que isso so com
// pastas que nem cabem na home. Cheio, para de registrar — e diagnostico, nao
// pode virar enxurrada.
#define GIFCOL_REG_MAX 256
static struct { unsigned long h; int nivel; } reg[GIFCOL_REG_MAX];   // 1 provisorio, 2 definitivo
static int nReg;

static unsigned long hashTexto(const char *s) {
  unsigned long h = 2166136261UL;
  for (; s && *s; s++) { h ^= (unsigned char)*s; h *= 16777619UL; }
  return h;
}

int gifcol_registrar(const char *chave, const char *titulo, GcMotivo m,
                     const char *url, int daCapa, const unsigned char *magica) {
  unsigned long h = hashTexto(chave);
  int nivel = (m == GC_CAPA_AINDA || m == GC_ARQ_AINDA) ? 1 : 2;
  int i;
  char limpo[140];
  if (m == GC_ANIMA) return 0;
  for (i = 0; i < nReg && reg[i].h != h; i++) {}
  if (i < nReg) {
    if (reg[i].nivel >= nivel) return 0;
  } else {
    if (nReg >= GIFCOL_REG_MAX) return 0;
    i = nReg++;
    reg[i].h = h;
  }
  reg[i].nivel = nivel;
  gifcol_sanear(url, limpo, sizeof limpo);
  if (m == GC_FORMATO && magica)
    printf("[gif] cartaz \"%.40s\" nao anima: %s (magica %02x%02x%02x%02x) | %s %s\n",
           titulo ? titulo : "", gifcol_motivo_texto(m),
           magica[0], magica[1], magica[2], magica[3],
           daCapa ? "capa" : "focusGif", limpo[0] ? limpo : "-");
  else
    printf("[gif] cartaz \"%.40s\" nao anima: %s | %s %s\n",
           titulo ? titulo : "", gifcol_motivo_texto(m),
           daCapa ? "capa" : "focusGif", limpo[0] ? limpo : "-");
  fflush(stdout);
  return 1;
}

void gifcol_registros_zerar(void) { nReg = 0; }
