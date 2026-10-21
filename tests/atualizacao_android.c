// Casamento do anexo "-android.apk" da release (auto-atualizacao no Android).
//   bash tests/atualizacao_android.sh
// Inclui atualizacao.c com NV_ANDROID definido aqui (acharApk e estatica); o resto do
// nucleo compila sem a flag, e a ponte JNI e substituida pelo stub abaixo.
#define NV_VERSAO "1.0.53"
#define NV_ANDROID 1
#include "../src/atualizacao.c"

int android_instalar_apk(const char *c) { (void)c; return 0; }

static int falhas = 0;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHA: " __VA_ARGS__); printf("\n"); } } while (0)

#define ASSET(nome, dig) "{\"name\":\"" nome "\",\"size\":1,\"digest\":\"sha256:" dig "\",\"download_count\":0," \
  "\"browser_download_url\":\"https://github.com/iqui27/nuvio-native-legacy/releases/download/v1/" nome "\"}"

int main(void) {
  char url[512], hash[80];
  const char *rel = "{\"assets\":["
    ASSET("Nuvio-1.7.0-android-debug.apk", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") ","
    ASSET("Nuvio-1.7.0-android-preview.2.apk", "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb") ","
    ASSET("Nuvio-1.7.0_arm.ipk", "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc") ","
    ASSET("Nuvio-1.7.0-android.apk", "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd") ","
    ASSET("Nuvio-1.7.0-tpk-arm.so", "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee") "]}";
  CONFERE(acharApk(rel, url, sizeof url, hash, sizeof hash), "achou o -android.apk");
  CONFERE(!strcmp(url, "https://github.com/iqui27/nuvio-native-legacy/releases/download/v1/Nuvio-1.7.0-android.apk"), "url certa: [%s]", url);
  CONFERE(!strcmp(hash, "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"), "digest do MESMO anexo: [%s]", hash);
  CONFERE(!acharApk("{\"assets\":[" ASSET("Nuvio-1-android-debug.apk", "aa") "," ASSET("Nuvio-1-android-preview.1.apk", "bb") "]}",
                    url, sizeof url, hash, sizeof hash), "debug e preview nao casam");
  CONFERE(url[0] == 0 && hash[0] == 0, "e saem vazios");
  CONFERE(!acharApk("{\"assets\":[{\"browser_download_url\":\"https://github.com/iqui27/nuvio-native-legacy/releases/download/v1/Nuvio-1-android.apk\"}]}",
                    url, sizeof url, hash, sizeof hash), "sem digest nao casa");
  CONFERE(!acharApk("{\"assets\":[]}", url, sizeof url, hash, sizeof hash), "sem anexo devolve 0");
  printf(falhas ? "atualizacao_android: %d FALHA(S)\n" : "atualizacao_android: ok\n", falhas);
  return falhas != 0;
}
