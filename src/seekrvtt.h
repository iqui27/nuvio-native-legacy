// SEEKR (seekr.tv): a parte PURA — ler a resposta do /sprites, ler o WebVTT
// das miniaturas e escolher a miniatura de uma posicao. Sem rede, sem SDL, sem
// GL, para o teste rodar sozinho (tests/seekr.sh). O resto mora em seekr.c.
//
// O formato, conferido no SDK oficial (github.com/AKhalil609/seekr-android-sdk,
// seekr-core/.../internal/Vtt.kt e LookupModels.kt):
//
//   GET https://api.seekr.tv/sprites?duration_ms=..&imdb_id=tt..      (filme)
//   GET https://api.seekr.tv/sprites?duration_ms=..&show_imdb_id=tt..&season=S&episode=E
//   cabecalho X-API-Key: <chave pessoal>
//   200 {"vtt_url":"https://sprites.seekr.tv/...vtt?sig=..","scale":1.0,
//        "source_duration_ms":7200000, ...}
//   401 {"error":"invalid or missing API key"}               (medido 02/10/2026)
//
// O VTT (sem chave, assinado) e um WEBVTT comum: uma linha "INICIO --> FIM" e
// a linha seguinte com a URL da FOLHA de miniaturas e o fragmento
// "#xywh=x,y,w,h" do recorte de 320x180 dentro dela.
#ifndef NV_SEEKRVTT_H
#define NV_SEEKRVTT_H
#include <stddef.h>

typedef struct {
  long ini, fim;          // ms
  int  folha;             // indice em SeekrVtt.folhas
  int  x, y, w, h;        // recorte dentro da folha, em pixels
  long rotulo;            // OpenSeek: pts real do quadro (o t= da URL), ms; -1 no Seekr
} SeekrCue;

typedef struct {
  SeekrCue *cues; int nCues;
  char **folhas;  int nFolhas;   // URLs absolutas, sem o fragmento
} SeekrVtt;

// vtt_url da resposta do /sprites, ja absoluta (relativa ganha
// https://sprites.seekr.tv na frente, como a pagina do servico diz). 1 se leu.
int  seekr_ler_lookup(const char *json, char *vtt, size_t tam);
// Monta a URL do /sprites. `t`/`e` zero = filme. 1 se coube.
int  seekr_url_lookup(char *dst, size_t tam, const char *imdb, int t, int e,
                      long durMs);
// Le o VTT inteiro. Devolve o numero de cues (0 = nada aproveitavel). As cues
// saem em ordem de inicio. Liberar com seekr_vtt_liberar.
int  seekr_vtt_ler(const char *vtt, SeekrVtt *out);
void seekr_vtt_liberar(SeekrVtt *v);
// Indice da cue para `posMs`, ou -1 sem cues. A cue que CONTEM a posicao so e
// a mais proxima na primeira metade dela (o quadro foi tirado no INICIO);
// passada a metade vale a seguinte — a regra do "Preview accuracy" da
// documentacao. Antes da primeira, a primeira; depois da ultima, a ultima.
int  seekr_vtt_cue(const SeekrVtt *v, long posMs);

// --- Provedor com outra base (OpenSeek, servidor proprio) ---------------------
// As funcoes acima sao estas com base = https://sprites.seekr.tv; a saida para
// o Seekr e identica byte a byte (tests/seekr.c, tests/openseek_vtt.c).
// `base` = "esquema://host[:porta]" sem barra no fim.
int  seekr_ler_lookup_base(const char *json, const char *base, char *vtt, size_t tam);
int  seekr_vtt_ler_base(const char *vtt, const char *base, SeekrVtt *out);
// OpenSeek (CONTRACT-v1 "VTT semantics"): so URLs ABSOLUTAS na origem pareada
// `origem` (as outras cues sao descartadas: o pino so vale para ela) e so com
// "t=<pts_ms>"; o t vira cue.rotulo e sai da URL da folha (a chave da folha).
int  seekr_vtt_ler_openseek(const char *vtt, const char *origem, SeekrVtt *out);
// Regra do OpenSeek: a cue so vale com ini <= posMs < fim; -1 fora de todas
// (sem "mais proxima" antes/depois/nos buracos, SCRUBBING.md §3).
int  seekr_vtt_cue_estrita(const SeekrVtt *v, long posMs);
#endif
