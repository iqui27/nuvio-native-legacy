// Ver centrallista.h.
#include "centrallista.h"
#include <stdio.h>
#include <string.h>

// O TERCEIRO CAMPO e o nome no botao, numa chave de i18n SO DA CENTRAL (nunca
// o mesmo portugues de outra tela: a traducao curta de la nao serviria aqui e
// vice-versa). Cabe em 15 px Montserrat no botao em todos os idiomas:
// tests/central_rotulos.sh. Dolby Vision/Atmos sao nome proprio, sem chave.
// Ordem = ordem da lista "Editar atalhos": reproducao, trailers, tela,
// interface. So escolhas curtas, que o OK percorre sem lista: idioma e limite
// de fileiras pedem a tela de Ajustes. A cor de destaque tem seletor proprio
// na central ("corDestaque", ajustes_rapido_cor_*).
static const CentralItem CATALOGO[] = {
  { "qualidade", "aj_monitor-play", "Qualidade máx." },
  { "dolbyVision", "aj_sparkles", "Dolby Vision" },
  { "dolbyAtmos", "aj_audio-lines", "Dolby Atmos" },
  { "escolherFonteManual", "aj_list-video", "Escolher fonte" },
  { "fontePrioridadeLocal", "aj_gauge", "Prioridade" },
  { "fonteHdrLocal", "aj_sparkle", "HDR e DV" },
  { "fonteAutoLocal", "aj_list-ordered", "Fonte auto" },
  { "fonteReporLocal", "aj_rotate-cw", "Se falhar" },
  { "legendaForcadaLocal", "aj_quote", "Leg. forçada" },
  { "legendaSyncAudioLocal", "aj_move-horizontal", "Sync áudio" },
  { "pauseOverlayEnabled", "aj_pause", "Pausa" },
  { "legenda2PosLocal", "aj_captions", "2ª legenda" },
  { "reacaoCreditosLocal", "aj_thumbs-up", "Avaliar no fim" },
  { "trailerAuto", "aj_clapperboard", "Trailer auto" },
  { "trailerHero", "aj_film", "Trailer no topo" },
  { "esmaecerLocal", "aj_moon", "Descanso" },
  { "brilhoPlayerLocal", "aj_sun-dim", "Brilho" },
  { "corDestaque", "aj_palette", "Cor" },
  { "vidroLocal", "aj_layers", "Efeito vidro" },
  { "fundoLocal", "aj_image", "Plano de fundo" },
  { "animacoes", "aj_wand", "Movimento" },
  { "tamanhoUiLocal", "aj_maximize-2", "Escala" },
  { "relogioTelaLocal", "aj_clock", "Mostrar hora" },
  { "relogio12hLocal", "aj_alarm-clock", "12 ou 24 h" },
  { "medidorFormaLocal", "aj_activity", "Medidor" },
  { "seloVistoLocal", "aj_eye", "Selo de visto" },
  { "selosColoridosLocal", "aj_badge-check", "Selos em cor" },
};
#define N_CAT ((int)(sizeof CATALOGO / sizeof *CATALOGO))
_Static_assert(CENTRAL_MAX == N_CAT, "CENTRAL_MAX must cover every catalog tile");

// DE FABRICA: o que se troca no sofa entre um titulo e outro, sem entrar em
// Ajustes. Qualidade e Dolby Vision/Atmos mudam a proxima fonte; escolher a
// fonte decide se a folha abre; trailer automatico e o que mais incomoda
// quando nao se quer; esmaecer e a protecao de tela da TV OLED.
static const char *const PADRAO[] = {
  "qualidade", "dolbyVision", "dolbyAtmos",
  "escolherFonteManual", "trailerAuto", "corDestaque", "esmaecerLocal",
};

int central_catalogo_n(void) { return N_CAT; }
const CentralItem *central_catalogo(int i) { return i >= 0 && i < N_CAT ? &CATALOGO[i] : NULL; }
int central_catalogo_achar(const char *chave) {
  int i;
  if (!chave) return -1;
  for (i = 0; i < N_CAT; i++) if (!strcmp(CATALOGO[i].chave, chave)) return i;
  return -1;
}

int centrallista_tem(const CentralLista *l, int item) {
  int i;
  for (i = 0; l && i < l->n; i++) if (l->item[i] == item) return 1;
  return 0;
}

static void por(CentralLista *l, int item) {
  if (item < 0 || item >= N_CAT || l->n >= CENTRAL_MAX || centrallista_tem(l, item)) return;
  l->item[l->n++] = item;
}

void centrallista_padrao(CentralLista *l) {
  size_t i;
  if (!l) return;
  l->n = 0;
  for (i = 0; i < sizeof PADRAO / sizeof *PADRAO; i++) por(l, central_catalogo_achar(PADRAO[i]));
}

void centrallista_ler(CentralLista *l, const char *texto) {
  const char *p = texto;
  if (!l) return;
  if (!p || !p[0]) { centrallista_padrao(l); return; }
  l->n = 0;
  while (*p) {
    char chave[64];
    size_t k = 0;
    while (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t') p++;
    while (*p && *p != '\n' && *p != '\r' && *p != ' ' && *p != '\t') {
      if (k + 1 < sizeof chave) chave[k++] = *p;
      p++;
    }
    chave[k] = 0;
    if (k) por(l, central_catalogo_achar(chave));
  }
}

void centrallista_escrever(const CentralLista *l, char *dst, size_t n) {
  size_t u = 0;
  int i;
  if (!dst || !n) return;
  dst[0] = 0;
  if (!l || !l->n) { snprintf(dst, n, "-\n"); return; }
  for (i = 0; i < l->n; i++) {
    const CentralItem *c = central_catalogo(l->item[i]);
    int w;
    if (!c) continue;
    w = snprintf(dst + u, n - u, "%s\n", c->chave);
    if (w < 0 || (size_t)w >= n - u) break;
    u += (size_t)w;
  }
}

int centrallista_alternar(CentralLista *l, int item) {
  int i;
  if (!l || item < 0 || item >= N_CAT) return -1;
  for (i = 0; i < l->n; i++) {
    if (l->item[i] != item) continue;
    memmove(&l->item[i], &l->item[i + 1], (size_t)(l->n - i - 1) * sizeof l->item[0]);
    l->n--;
    return 0;
  }
  if (l->n >= CENTRAL_MAX) return -1;
  l->item[l->n++] = item;
  return 1;
}
