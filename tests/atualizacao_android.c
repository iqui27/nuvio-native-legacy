// Release asset selection for full and per-ABI Lite Android installs.
//   bash tests/atualizacao_android.sh
// Inclui atualizacao.c com NV_ANDROID definido aqui (acharApk e estatica); o resto do
// nucleo compila sem a flag, e a ponte JNI e substituida pelo stub abaixo.
#define NV_VERSAO "1.0.53"
#define NV_ANDROID 1
#include <stdint.h>
#ifdef TEST_ARMV7
#undef UINTPTR_MAX
#define UINTPTR_MAX UINT32_MAX
#endif
#include "../src/atualizacao.c"

#ifdef NV_APK_LITE
#ifdef TEST_ARMV7
#define EXPECTED_APK "Nuvio-1.7.0-android-lite-armv7.apk"
#define EXPECTED_HASH "7777"
#else
#define EXPECTED_APK "Nuvio-1.7.0-android-lite-arm64.apk"
#define EXPECTED_HASH "6464"
#endif
#else
#define EXPECTED_APK "Nuvio-1.7.0-android.apk"
#define EXPECTED_HASH "dddd"
#endif

int android_instalar_apk(const char *c) { (void)c; return 0; }
int android_preparar_apk(const char *c) { (void)c; return 0; }

static int falhas = 0;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHA: " __VA_ARGS__); printf("\n"); } } while (0)

#define ASSET(nome, dig) "{\"name\":\"" nome "\",\"size\":1,\"digest\":\"sha256:" dig "\",\"download_count\":0," \
  "\"browser_download_url\":\"https://x/" nome "\"}"

int main(void) {
  char url[512], hash[80];
  const char *rel = "{\"assets\":["
    ASSET("Nuvio-1.7.0-android-debug.apk", "aaaa") ","
    ASSET("Nuvio-1.7.0-android-preview.2.apk", "bbbb") ","
    ASSET("Nuvio-1.7.0_arm.ipk", "cccc") ","
    ASSET("Nuvio-1.7.0-android.apk", "dddd") ","
    ASSET("Nuvio-1.7.0-android-lite.apk", "ffff") ","
    ASSET("Nuvio-1.7.0-android-lite-armv7-debug.apk", "aaaa") ","
    ASSET("Nuvio-1.7.0-android-lite-arm64-preview.2.apk", "bbbb") ","
    ASSET("Nuvio-1.7.0-android-lite-armv7.apk", "7777") ","
    ASSET("Nuvio-1.7.0-android-lite-arm64.apk", "6464") ","
    ASSET("Nuvio-1.7.0-tpk-arm.so", "eeee") "]}";
  CONFERE(acharApk(rel, url, sizeof url, hash, sizeof hash), "matching release asset found");
  CONFERE(!strcmp(url, "https://x/" EXPECTED_APK), "url certa: [%s]", url);
  CONFERE(!strcmp(hash, EXPECTED_HASH), "digest do MESMO anexo: [%s]", hash);
  CONFERE(!acharApk("{\"assets\":[" ASSET("Nuvio-1-android-debug.apk", "aa") "," ASSET("Nuvio-1-android-preview.1.apk", "bb") "]}",
                    url, sizeof url, hash, sizeof hash), "debug e preview nao casam");
  CONFERE(url[0] == 0 && hash[0] == 0, "e saem vazios");
  CONFERE(!acharApk("{\"assets\":[{\"browser_download_url\":\"https://x/" EXPECTED_APK "\"}]}",
                    url, sizeof url, hash, sizeof hash), "sem digest nao casa");
#ifdef NV_APK_LITE
  CONFERE(!acharApk("{\"assets\":[" ASSET("Nuvio-1.7.0-android.apk", "dddd") ","
                    ASSET("Nuvio-1.7.0-android-lite.apk", "ffff") "]}",
                    url, sizeof url, hash, sizeof hash), "Lite never falls back to universal APKs");
#endif
  CONFERE(!acharApk("{\"assets\":[]}", url, sizeof url, hash, sizeof hash), "sem anexo devolve 0");
  printf(falhas ? "atualizacao_android: %d FALHA(S)\n" : "atualizacao_android: ok\n", falhas);
  return falhas != 0;
}
