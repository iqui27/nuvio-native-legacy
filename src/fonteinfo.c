#include "fonteinfo.h"
#include "streams.h"
#include "badges.h"
#include <stdio.h>
#include <string.h>

static struct {
  FonteInfo info;
  char alvo[64];
  unsigned geracao;
  uint64_t sessao;
  int indice, valido;
  uint64_t badges, tamanhoBytes;
  long tamanhoMB;
  int altura, dolbyVision, dolbyAtmos;
  FonteFiltroDados filtro;
} atual;
static void texto(FonteInfo *f, const char *s) {
  if (s && *s) for (int i = 0; i < f->nTextos; i++) if (!strcmp(f->texto[i],s)) return;
  if (s && *s && f->nTextos < FONTEINFO_TEXTOS)
    snprintf(f->texto[f->nTextos++], sizeof f->texto[0], "%s", s);
}
static int conhecido(const FonteFiltroDados *d, int c, int i) {
  return (d->valores[c] & fontefiltro_bit(c, i)) != 0;
}
void fonteinfo_montar(const Stream *s, FonteInfo *f) {
  memset(f, 0, sizeof *f);
  if (!s) return;
  const FonteFiltroDados *d = &s->filtro;
  char canonico[1536] = "";
  size_t usado = 0;
  for (int c = 0; c <= FONTEFILTRO_ENCODE; c++)
    for (int k = 0; k < fontefiltro_opcoes_n(c) - 1; k++) if (conhecido(d,c,k)) {
      int n = snprintf(canonico + usado, sizeof canonico - usado, " %s", fontefiltro_opcao(c,k));
      if (n > 0 && (size_t)n < sizeof canonico - usado) usado += (size_t)n;
    }
  uint64_t mask = s->badges | badges_detectar(canonico);
  static const char *const audioLogo[] = {"a-atmos","a-ddp","a-dd","a-dtsx","a-dtshdma","a-dtshd",NULL,"a-dts","a-truehd",NULL,NULL,NULL};
  for (int k = 0; k < fontefiltro_opcoes_n(FONTEFILTRO_AUDIO) - 1; k++)
    if (conhecido(d,FONTEFILTRO_AUDIO,k) && audioLogo[k]) mask |= badges_bit(audioLogo[k]);
  if (conhecido(d,FONTEFILTRO_CANAIS,0)) mask |= badges_bit("c-71");
  if (conhecido(d,FONTEFILTRO_CANAIS,2)) mask |= badges_bit("c-51");
  if (s->dolbyVision) mask |= badges_bit("v-dv");
  if (s->dolbyAtmos) mask |= badges_bit("a-atmos");
  uint64_t resolucoes = badges_bit("r-4k") | badges_bit("r-1080") | badges_bit("r-720") | badges_bit("r-sd");
  if (!(mask & resolucoes) && s->altura > 0) {
    if (s->altura == 2160) mask |= badges_bit("r-4k");
    else if (s->altura == 1080) mask |= badges_bit("r-1080");
    else if (s->altura == 720) mask |= badges_bit("r-720");
    else { char res[24]; snprintf(res,sizeof res,"%dp",s->altura); texto(f,res); }
  }
  // Combined legacy badges carry DV too. Show each specification just once.
  static const char *const combinados[][2] = {
    {"a-atmos-dv","a-atmos"},{"a-truehd-dv","a-truehd"},{"a-dd-dv","a-dd"}
  };
  for (unsigned i = 0; i < sizeof combinados / sizeof combinados[0]; i++)
    if (mask & badges_bit(combinados[i][0])) mask |= badges_bit("v-dv") | badges_bit(combinados[i][1]);
  static const char *const ids[] = {
    "r-4k","r-1080","r-720","r-sd","q-remux","q-bluray","q-webdl","q-webrip","q-seadex",
    "v-dv","v-hdr10plus","v-hdr10","v-hdr","v-hlg","v-imax-enhanced","v-imax","v-sdr",
    "a-atmos","a-truehd","a-dtsx","a-dtshdma","a-dtshd","a-dts","a-ddp","a-dd",
    "c-71","c-51","co-x265","co-x264","co-av1"
  };
  for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) f->selos |= mask & badges_bit(ids[i]);
  // Formats without an existing logo remain readable text.
  for (int k = 0; k < fontefiltro_opcoes_n(FONTEFILTRO_AUDIO) - 1; k++)
    if (conhecido(d,FONTEFILTRO_AUDIO,k) && !audioLogo[k]) texto(f,fontefiltro_opcao(FONTEFILTRO_AUDIO,k));
  for (int k = 0; k < fontefiltro_opcoes_n(FONTEFILTRO_CANAIS) - 1; k++)
    if (conhecido(d,FONTEFILTRO_CANAIS,k) && k != 0 && k != 2) texto(f,fontefiltro_opcao(FONTEFILTRO_CANAIS,k));
  static const int resolucaoTexto[] = {1,4,5,6};
  for (unsigned k = 0; k < sizeof resolucaoTexto / sizeof resolucaoTexto[0]; k++)
    if (conhecido(d,FONTEFILTRO_RESOLUCAO,resolucaoTexto[k])) texto(f,fontefiltro_opcao(FONTEFILTRO_RESOLUCAO,resolucaoTexto[k]));
  for (int k = 4; k < fontefiltro_opcoes_n(FONTEFILTRO_QUALIDADE) - 1; k++)
    if (conhecido(d,FONTEFILTRO_QUALIDADE,k)) texto(f,fontefiltro_opcao(FONTEFILTRO_QUALIDADE,k));
  for (int k = 3; k < fontefiltro_opcoes_n(FONTEFILTRO_ENCODE) - 1; k++)
    if (conhecido(d,FONTEFILTRO_ENCODE,k)) texto(f,fontefiltro_opcao(FONTEFILTRO_ENCODE,k));
  texto(f,d->grupo);
  uint64_t bytes = s->tamanhoBytes ? s->tamanhoBytes : d->tamanhoBytes;
  if (!bytes && s->tamanhoMB > 0 && (uint64_t)s->tamanhoMB <= UINT64_MAX / UINT64_C(1048576))
    bytes = (uint64_t)s->tamanhoMB * UINT64_C(1048576);
  if (bytes) {
    char tam[48];
    if (bytes >= UINT64_C(1073741824)) snprintf(tam,sizeof tam,"%.1f GB",(double)bytes / 1073741824.0);
    else snprintf(tam,sizeof tam,"%.1f MB",(double)bytes / 1048576.0);
    texto(f,tam);
  }
}
void fonteinfo_limpar(void) { memset(&atual, 0, sizeof atual); }
void fonteinfo_guardar(const Stream *s, const char *alvo, unsigned geracao, uint64_t sessao, int indice) {
  if (!s || !alvo || !*alvo || strlen(alvo) >= sizeof atual.alvo || indice < 0) { fonteinfo_limpar(); return; }
  // Enrichment can arrive without changing the source index or list generation.
  // Compare only display metadata; never copy playback URLs or credentials.
  if (atual.valido && atual.geracao == geracao && atual.sessao == sessao &&
      atual.indice == indice && !strcmp(atual.alvo,alvo) &&
      atual.badges == s->badges && atual.altura == s->altura &&
      atual.dolbyVision == s->dolbyVision && atual.dolbyAtmos == s->dolbyAtmos &&
      atual.tamanhoBytes == s->tamanhoBytes && atual.tamanhoMB == s->tamanhoMB &&
      atual.filtro.tamanhoBytes == s->filtro.tamanhoBytes &&
      !memcmp(atual.filtro.valores,s->filtro.valores,sizeof s->filtro.valores) &&
      !strcmp(atual.filtro.grupo,s->filtro.grupo)) return;
  fonteinfo_montar(s,&atual.info);
  snprintf(atual.alvo,sizeof atual.alvo,"%s",alvo);
  atual.geracao = geracao; atual.sessao = sessao; atual.indice = indice;
  atual.badges = s->badges; atual.altura = s->altura;
  atual.dolbyVision = s->dolbyVision; atual.dolbyAtmos = s->dolbyAtmos;
  atual.tamanhoBytes = s->tamanhoBytes; atual.tamanhoMB = s->tamanhoMB;
  atual.filtro = s->filtro;
  atual.valido = atual.info.selos || atual.info.nTextos;
}
int fonteinfo_ler(const char *alvo, unsigned geracao, uint64_t sessao, FonteInfo *f) {
  if (f) memset(f,0,sizeof *f);
  if (!atual.valido || !alvo || atual.geracao != geracao || atual.sessao != sessao || strcmp(alvo,atual.alvo)) return 0;
  if (f) *f = atual.info;
  return 1;
}
