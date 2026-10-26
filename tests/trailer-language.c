// #687: native trailer localization, actual audio and language-separated cache.
#define static
#include "../src/trailerapple.c"
#undef static
#include <assert.h>
#include <unistd.h>

static char pref[16] = "de-DE", pasta[512];
static int pedidos, falhas, negarDe, qualidade = 720;
void extras_trailer_audio_idioma(char *dst, unsigned cap) { snprintf(dst, cap, "%s", pref); }
char *dados_caminho(char *dst, unsigned cap, const char *nome) { snprintf(dst, cap, "%s/%s", pasta, nome); return dst; }
void dados_marcar_sujo(int leve) { (void)leve; }
int ajustes_trailer_qualidade(void) { return qualidade; }
static const char *MASTER =
  "#EXTM3U\n"
  "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"eng\",LANGUAGE=\"en\",URI=\"https://cdn/en.m3u8\"\n"
  "#EXT-X-STREAM-INF:BANDWIDTH=2000000,CODECS=\"avc1.64001f,mp4a.40.2\",RESOLUTION=1920x1080,AUDIO=\"eng\"\n"
  "https://cdn/english-only.m3u8\n"
  "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"a\",LANGUAGE=\"en\",URI=\"https://cdn/en.m3u8\"\n"
  "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"a\",LANGUAGE=\"%s\",URI=\"https://cdn/local.m3u8\"\n"
  "#EXT-X-STREAM-INF:BANDWIDTH=1000000,CODECS=\"avc1.64001f,mp4a.40.2\",RESOLUTION=1280x720,AUDIO=\"a\"\n"
  "https://cdn/video-%s.m3u8\n";
char *rede_baixar_com(const char *url, int segundos, const char *const *cab) {
  (void)segundos; (void)cab; pedidos++;
  if (strstr(url, "/search?")) return strdup(
    "{\"data\":{\"canvas\":{\"shelves\":[{\"items\":[{\"id\":\"umc.test\",\"type\":\"Movie\",\"title\":\"Test Movie\",\"releaseDate\":1646092800000}]}]}}}");
  if (strstr(url, "/movies/")) {
    char json[1600];
    const char *lg = strstr(url, "sf=143443") ? "de" : strstr(url, "sf=143442") ? "fr" :
                     strstr(url, "sf=143480") ? "tr" : strstr(url,"sf=143462") ? "ja" : "en";
    if (negarDe && !strcmp(lg,"de")) return strdup("{\"data\":{\"content\":{}}}");
    snprintf(json,sizeof json,"{\"data\":{\"canvas\":{\"shelves\":[{\"content\":{\"backgroundVideo\":{\"assets\":{\"hlsUrl\":\"https://wrong-title.m3u8\"}}}}]},\"content\":{\"originalSpokenLanguages\":[{\"locale\":\"ja_JP\"}],\"backgroundVideo\":{\"assets\":{\"hlsUrl\":\"https://trailer/%s.m3u8\"}}}}}",lg);
    return strdup(json);
  }
  if (strstr(url,"https://trailer/")) {
    char m[2000];
    const char *lg = strstr(url,"/de.") ? "de-DE" : strstr(url,"/fr.") ? "fr-FR" : strstr(url,"/ja.") ? "ja-JP" : "en";
    snprintf(m,sizeof m,MASTER,lg,lg); return strdup(m);
  }
  assert(!"unexpected network request"); return NULL;
}
static void check(const char *msg, int ok) { printf("%s %s\n",ok?"ok":"FAIL",msg); if(!ok)falhas++; }
static void resolver(const char *idioma) {
  Pedido *p=calloc(1,sizeof *p); assert(p);
  snprintf(pref,sizeof pref,"%s",idioma);
  snprintf(p->imdb,sizeof p->imdb,"tt0000001");
  chaveIdioma(p->imdb,p->chave,sizeof p->chave,p->idioma,sizeof p->idioma);
  snprintf(p->titulo,sizeof p->titulo,"Test Movie"); snprintf(p->meta,sizeof p->meta,"2022");
  buscar(p);
}
static int audioNoArquivo(const char *url,const char *lg) {
#ifdef NV_TRAILER_SAMSUNG
  return url && strstr(url,lg)!=NULL;
#else
  char b[3000]; FILE *f = url && !strncmp(url,"file://",7) ? fopen(url+7,"r") : NULL;
  if(!f)return 0;
  size_t n=fread(b,1,sizeof b-1,f);b[n]=0;fclose(f);
  return strstr(b,lg)!=NULL && !strstr(b,"https://cdn/en.m3u8") && !strstr(b,"english-only");
#endif
}
int main(void) {
  char temp[]="/tmp/nuvio-trailer-language-XXXXXX"; assert(mkdtemp(temp));
  snprintf(pasta,sizeof pasta,"%s",temp);
  char dir[600];snprintf(dir,sizeof dir,"%s/trailer",pasta);assert(!mkdir(dir,0755));
  char query[240],a[16];
  consultaIdioma("de-DE",query,sizeof query);
  check("German storefront and request language",strstr(query,"sf=143443")&&strstr(query,"locale=de-DE"));
  check("actual audio inspected independently of regional URL",!audioDoMaster("#EXTM3U\n#EXT-X-MEDIA:TYPE=AUDIO,LANGUAGE=\"en\"\n","tr",a,sizeof a));
  resolver("de-DE");
  check("German lookup publishes playable trailer",audioNoArquivo(trailerapple_url("tt0000001"),"de-DE"));
  check("verified German is preferred",trailerapple_idioma_preferido("tt0000001")==1);
  char de[600];snprintf(de,sizeof de,"%s",trailerapple_url("tt0000001"));
  snprintf(pref,sizeof pref,"fr-FR");
  check("changing preference cannot reuse German cache",!trailerapple_url("tt0000001"));
  resolver("fr-FR");
  check("French lookup uses French audio",audioNoArquivo(trailerapple_url("tt0000001"),"fr-FR"));
  check("French and German use separate playback files",strcmp(de,trailerapple_url("tt0000001"))!=0);
  snprintf(pref,sizeof pref,"de-DE");
  check("switching back reuses correct cached German",audioNoArquivo(trailerapple_url("tt0000001"),"de-DE"));
  // Simulate process restart: the on-disk cache must retain the audio metadata.
  memset(tab,0,sizeof tab);nTab=prox=0;int antes=pedidos;
  qualidade = 0; // Auto must rebuild the native reduced master after restart.
  trailerapple_pedir("tt0000001","Test Movie","2022",0);
  check("disk cache restores language without network",pedidos==antes&&audioNoArquivo(trailerapple_url("tt0000001"),"de-DE"));
  resolver("tr-TR");
  check("regional English audio is a fallback, not Turkish",trailerapple_url("tt0000001")&&trailerapple_idioma_preferido("tt0000001")==0);
  resolver("~");
  check("Original resolves title language before choosing region",audioNoArquivo(trailerapple_url("tt0000001"),"ja-JP"));
  negarDe=1;resolver("de-DE");
  check("missing localized trailer falls back to English",trailerapple_url("tt0000001")&&trailerapple_idioma_preferido("tt0000001")==0);
  // A completed request from an old language cannot overwrite the new one.
  Pedido *late=calloc(1,sizeof *late);snprintf(pref,sizeof pref,"de-DE");
  snprintf(late->imdb,sizeof late->imdb,"tt0000002");chaveIdioma(late->imdb,late->chave,sizeof late->chave,late->idioma,sizeof late->idioma);
  snprintf(late->titulo,sizeof late->titulo,"Test Movie");snprintf(late->meta,sizeof late->meta,"2022");
  snprintf(pref,sizeof pref,"fr-FR");negarDe=0;buscar(late);
  check("late result is isolated from current preference",!trailerapple_url("tt0000002"));
  // Test artifacts only; no user data is touched.
  char cmd[700];snprintf(cmd,sizeof cmd,"rm -rf '%s'",pasta);assert(!system(cmd));
  return falhas?1:0;
}
