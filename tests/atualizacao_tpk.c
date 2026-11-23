// Seletor do .so da release para o auto-update do .tpk/.tpk40: sufixo EXATO
// da ABI, URL oficial de anexo e sha256 do PROPRIO objeto do anexo — um .so
// sem digest atras de um anexo com digest nao pode herdar o do vizinho, o
// mesmo defeito do .ipk da revisao F89 (o hash errado ia para a comparacao
// dos bytes baixados).
//
//   bash tests/atualizacao_tpk.sh           # NV_TPK e NV_TPK40
//
// Inclui o .c de proposito: acharSo e estatica.
#define NV_VERSAO "1.0.53"
#ifdef NV_TPK40
void nv_tpk40_etapa(const char *linha) { (void)linha; }
#endif
#include "../src/atualizacao.c"

static int falhas = 0;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHA: " __VA_ARGS__); printf("\n"); } } while (0)

int main(void) {
  char url[512], hash[80];
  const char *dig = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  const char *oficial = "https://github.com/iqui27/nuvio-native-legacy/releases/download/v9.9.9/libnuvio-9.9.9" AT_SO_SUFIXO;

  // Anexo oficial com digest proprio: selecionado.
  { char j[1400];
    snprintf(j, sizeof j, "{\"assets\":[{\"name\":\"x\",\"size\":1,\"digest\":\"sha256:%s\",\"download_count\":0,"
             "\"browser_download_url\":\"%s\"}]}", dig, oficial);
    CONFERE(acharSo(j, url, sizeof url, hash, sizeof hash), "anexo oficial desta ABI casa");
    CONFERE(!strcmp(url, oficial) && !strcmp(hash, dig), "url e digest do proprio anexo: [%s]", url); }

  // O .so SEM digest depois de um anexo COM digest: recusado, sem herdar o
  // sha256 do vizinho (antes da correcao a busca pegava o ultimo "digest": do
  // corpo inteiro ate a URL).
  { char j[1600];
    snprintf(j, sizeof j,
             "{\"assets\":[{\"digest\":\"sha256:%s\",\"browser_download_url\":\"https://github.com/iqui27/nuvio-native-legacy/releases/download/v9.9.9/nuvio.wgt\"},"
             "{\"browser_download_url\":\"%s\"}]}", dig, oficial);
    CONFERE(!acharSo(j, url, sizeof url, hash, sizeof hash),
            ".so sem digest proprio apos anexo com digest: recusado");
    CONFERE(url[0] == 0 && hash[0] == 0, "e saem vazios"); }

  // "digest" dentro do objeto aninhado do anexo nao conta como digest do
  // arquivo: sem o proprio no nivel do anexo, recusa.
  { char j[1700];
    snprintf(j, sizeof j,
             "{\"assets\":[{\"uploader\":{\"login\":\"x\",\"digest\":\"sha256:%s\"},"
             "\"browser_download_url\":\"%s\"}]}", dig, oficial);
    CONFERE(!acharSo(j, url, sizeof url, hash, sizeof hash),
            "digest dentro de objeto aninhado nao e do anexo: recusado"); }

  // Host fora do repositorio e digest que nao e sha256 hex: recusados.
  { char j[1400];
    snprintf(j, sizeof j, "{\"assets\":[{\"digest\":\"sha256:%s\",\"browser_download_url\":\"https://example.test/r/x%s\"}]}",
             dig, AT_SO_SUFIXO);
    CONFERE(!acharSo(j, url, sizeof url, hash, sizeof hash), "host externo nao casa"); }
  { char j[1400];
    snprintf(j, sizeof j, "{\"assets\":[{\"digest\":\"sha256:abc\",\"browser_download_url\":\"%s\"}]}", oficial);
    CONFERE(!acharSo(j, url, sizeof url, hash, sizeof hash), "digest invalido nao casa"); }

  // O pacote do OUTRO alvo nao casa: "-tpk-arm.so" nao termina em
  // "-tpk40-arm.so" e vice-versa — cada .tpk so pega a propria lib.
  { char j[1400];
#ifdef NV_TPK40
    const char *outro = "https://github.com/iqui27/nuvio-native-legacy/releases/download/v9.9.9/libnuvio-9.9.9-tpk-arm.so";
#else
    const char *outro = "https://github.com/iqui27/nuvio-native-legacy/releases/download/v9.9.9/libnuvio-9.9.9-tpk40-arm.so";
#endif
    snprintf(j, sizeof j, "{\"assets\":[{\"digest\":\"sha256:%s\",\"browser_download_url\":\"%s\"}]}", dig, outro);
    CONFERE(!acharSo(j, url, sizeof url, hash, sizeof hash), "a lib do outro pacote nao casa"); }

  printf(falhas ? "atualizacao_tpk: %d FALHA(S)\n" : "atualizacao_tpk: ok\n", falhas);
  return falhas != 0;
}
