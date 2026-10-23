// Coto do que o player.c com NV_ANDROID usa e so existe no video_android.c ou
// atras de NV_ANDROID no faixas.c (o resto do src/ compila sem NV_ANDROID).
#include "video.h"

void faixas_reiniciar_preparada(void) {}
int video_precarregar(const char *url, const char *cab, double seg, double pct, int buffer,
                      int cacheMb, unsigned token, const char *idiomaAudio, int kbps) {
  (void)kbps; (void)url; (void)cab; (void)seg; (void)pct; (void)buffer; (void)cacheMb; (void)token; (void)idiomaAudio;
  return 0;
}
int video_precarregar_proximo(const char *url, const char *cab, double seg, double pct, int buffer,
                              int cacheMb, unsigned token, const char *idiomaAudio, int kbps) {
  return video_precarregar(url, cab, seg, pct, buffer, cacheMb, token, idiomaAudio, kbps);
}
int video_precarregar_estado(unsigned token) { (void)token; return 0; }
void video_precarregar_cancelar(unsigned token) { (void)token; }
void video_precarregar_adotar(unsigned token) { (void)token; }
