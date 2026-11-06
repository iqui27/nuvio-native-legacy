// The subtitle audio-sync fallback relays in src/video_android.c (06/11) talk
// to legsync/legenda/addons/rede, which these JNI tests do not link: inert stubs.
void legsync_audio_backend(const LegSyncAudio *b) { (void)b; }
void legsync_audio_modelo(const char *c, const LegSyncAudioTrecho *t, int n, int e) { (void)c; (void)t; (void)n; (void)e; }
void legsync_audio_falhou(const char *c, const char *m) { (void)c; (void)m; }
void legsync_audio_trocar(const char *c, const char *u) { (void)c; (void)u; }
int addons_legendas_copiar(Legenda *d, int max, unsigned *g, int *p) { (void)d; (void)max; (void)g; (void)p; return 0; }
const char *legenda_idioma(const char *i, const char *a) { (void)a; return i ? i : ""; }
char *rede_baixar_bin(const char *u, int s, long *n) { (void)u; (void)s; (void)n; return NULL; }
LegendaDocumento *legenda_documento_bytes(const char *b, long n, const LegendaDocumentoInfo *i) { (void)b; (void)n; (void)i; return NULL; }
const LegendaCue *legenda_documento_dados(const LegendaDocumento *d, int *n) { (void)d; if (n) *n = 0; return NULL; }
void legenda_documento_liberar(LegendaDocumento *d) { (void)d; }
