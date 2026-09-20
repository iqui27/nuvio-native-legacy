// Aviso de atualizacao: comparacao de versao, leitura do JSON do GitHub e
// limpeza das notas — e uma captura do cartao para ser OLHADA.
//
//   bash tests/atualizacao.sh            # so as conferencias
//   bash tests/atualizacao.sh /tmp/x.bmp # e a captura
//
// Inclui o .c de proposito: as funcoes de parse sao estaticas, e expo-las so
// para o teste seria API a mais. O .sh compila tudo MENOS src/atualizacao.c.
#define NV_VERSAO "1.0.53"
#include "../src/atualizacao.c"
#include "tex_cache.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

static int falhas = 0;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHA: " __VA_ARGS__); printf("\n"); } } while (0)

static char *lerArquivo(const char *nome) {
  FILE *f = fopen(nome, "rb"); long n; char *s;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  s = malloc((size_t)n + 1); fread(s, 1, (size_t)n, f); s[n] = 0; fclose(f);
  return s;
}

int main(int argc, char **argv) {
  CONFERE(maisNova("1.0.54", "1.0.53"), "1.0.54 > 1.0.53");
  CONFERE(!maisNova("1.0.53", "1.0.53"), "igual nao e mais nova");
  CONFERE(!maisNova("1.0.52", "1.0.53"), "1.0.52 nao e mais nova");
  CONFERE(maisNova("1.1.0", "1.0.99"), "1.1.0 > 1.0.99");
  CONFERE(maisNova("2.0", "1.9.9"), "2.0 > 1.9.9");
  CONFERE(maisNova("1.0.53", "dev"), "qualquer tag e mais nova que dev");

  { char tag[48] = "", body[8192] = "";
    char *j = lerArquivo("tests/atualizacao_latest.json");
    CONFERE(j != NULL, "fixture tests/atualizacao_latest.json");
    if (j) {
      CONFERE(textoJson(j, "tag_name", tag, sizeof tag), "tag_name");
      CONFERE(tag[0] == 'v', "tag comeca com v: %s", tag);
      CONFERE(textoJson(j, "body", body, sizeof body), "body");
      CONFERE(strchr(body, '\n') != NULL, "body tem quebra de linha de verdade");
      CONFERE(strstr(body, "\\n") == NULL, "body sem \\n literal");
      limparNotas(body, notas, sizeof notas);
      printf("--- notas limpas ---\n%s--------------------\n", notas);
      CONFERE(strstr(notas, "\001Fixed") != NULL || strstr(notas, "\001Added") != NULL, "secao virou \\x01");
      CONFERE(strstr(notas, "\xe2\x80\xa2 ") != NULL, "item virou bala");
      CONFERE(strstr(notas, "**") == NULL, "sem negrito markdown");
      CONFERE(strstr(notas, "Notes") == NULL, "secao Notes cortada");
      CONFERE(strstr(notas, ".wgt") == NULL, "rodape de instalacao cortado");
      // O ANEXO CERTO ENTRE DOIS. A release leva .wgt E .ipk, e o .wgt vem
      // PRIMEIRO no JSON — que e exatamente o caso em que procurar a primeira
      // ocorrencia da chave (o que textoJson faz) baixaria o pacote do Samsung
      // para instalar numa LG. Quem decide e o sufixo.
      // A fixture e uma release REAL (v1.0.52), de quando so havia UM .ipk.
      // Por isso o resultado depende da variante: a build normal acha o anexo
      // dela; a de cache grande NAO acha, e nao achar e o comportamento certo
      // — release sem o anexo da variante nao deve instalar nada.
      { char url[512], hash[80];
        int achou = acharIpk(j, url, sizeof url, hash, sizeof hash, AT_SUFIXO);
#ifdef NV_TEX_MB_FIXO
        CONFERE(!achou, "build highcache nao aceita release so com o .ipk normal");
        CONFERE(url[0] == 0, "e a url fica vazia");
#else
        CONFERE(achou, "achou um .ipk nos assets");
        CONFERE(strstr(url, ".ipk") != NULL && strstr(url, ".wgt") == NULL,
                "e o .ipk, nao o .wgt: [%s]", url);
        CONFERE(!strncmp(url, "https://", 8), "url absoluta: [%s]", url);
#endif
      }
      free(j);
    } }

  // Sem anexo nenhum nao ha o que instalar, e isso NAO e erro: release de
  // codigo-fonte apenas, ou alvo que nao instala.
  { char url[64], hash[80];
    CONFERE(!acharIpk("{\"assets\":[]}", url, sizeof url, hash, sizeof hash, AT_SUFIXO),
            "sem anexo devolve 0");
    CONFERE(url[0] == 0, "e a url fica vazia");
    CONFERE(!acharIpk("{\"assets\":[{\"browser_download_url\":\"https://x/y.wgt\"}]}",
                      url, sizeof url, hash, sizeof hash, AT_SUFIXO), "so .wgt tambem devolve 0"); }

  // O DIGEST DO ANEXO CERTO. Sem ele o instalador do Homebrew Channel recusa
  // com "Invalid file checksum", e o digest do .wgt (que vem ANTES no JSON)
  // seria o erro facil de cometer.
  { char url[128], hash[80];
    const char *j =
      "{\"assets\":["
      "{\"digest\":\"sha256:aaaa\",\"browser_download_url\":\"https://x/a.wgt\"},"
      "{\"digest\":\"sha256:bbbb\",\"browser_download_url\":\"https://x/b_arm.ipk\"}]}";
    { int achou = acharIpk(j, url, sizeof url, hash, sizeof hash, AT_SUFIXO);
#ifdef NV_TEX_MB_FIXO
      CONFERE(!achou, "sem anexo highcache, nao instala"); }
#else
      CONFERE(achou, "achou o ipk");
      CONFERE(!strcmp(url, "https://x/b_arm.ipk"), "url do ipk: [%s]", url);
      CONFERE(!strcmp(hash, "bbbb"), "digest do MESMO anexo, sem o prefixo: [%s]", hash); }
#endif
  }

  // A VARIANTE CERTA ENTRE OS TRES ANEXOS, NA ORDEM REAL DO GITHUB.
  //
  // Este caso e o defeito de 18/09: acharIpk devolvia o primeiro ".ipk", e o
  // GitHub ordena os anexos alfabeticamente, onde "_arm-highcache.ipk" vem
  // ANTES de "_arm.ipk" porque '-' e menor que '.'. Toda LG que apertasse
  // "Atualizar agora" trocava para a build de cache grande sem saber — o id do
  // pacote e a versao sao iguais nos dois, entao nada denunciava a troca.
  { char url[160], hash[80];
    const char *j =
      "{\"assets\":["
      "{\"digest\":\"sha256:1111\",\"browser_download_url\":\"https://x/NuvioTV-1.1.3-tizen.wgt\"},"
      "{\"digest\":\"sha256:2222\",\"browser_download_url\":\"https://x/space.nuvio_1.1.3_arm-highcache.ipk\"},"
      "{\"digest\":\"sha256:3333\",\"browser_download_url\":\"https://x/space.nuvio_1.1.3_arm.ipk\"}]}";
    CONFERE(acharIpk(j, url, sizeof url, hash, sizeof hash, AT_SUFIXO), "achou o anexo da variante");
    CONFERE(strstr(url, AT_SUFIXO) != NULL, "e o sufixo desta build (%s): [%s]", AT_SUFIXO, url);
#ifdef NV_TEX_MB_FIXO
    CONFERE(!strcmp(hash, "2222"), "digest do anexo highcache: [%s]", hash);
#else
    CONFERE(strstr(url, "highcache") == NULL, "build normal NAO leva a highcache: [%s]", url);
    CONFERE(!strcmp(hash, "3333"), "digest do anexo normal: [%s]", hash);
#endif
  }

  // OS NOMES DAS RELEASES 1.3.1/1.3.2 ("NuvioTV-1.3.2-webos.ipk"): a LG normal
  // ficou duas versoes sem botao porque "-webos.ipk" nao termina em "_arm.ipk".
  // Agora os dois nomes valem, e a normal continua sem pegar a highcache.
  { char url[160], hash[80];
    const char *j =
      "{\"assets\":["
      "{\"digest\":\"sha256:aaaa\",\"browser_download_url\":\"https://x/NuvioTV-1.3.2-tizen.wgt\"},"
      "{\"digest\":\"sha256:bbbb\",\"browser_download_url\":\"https://x/NuvioTV-1.3.2-webos-highcache.ipk\"},"
      "{\"digest\":\"sha256:cccc\",\"browser_download_url\":\"https://x/NuvioTV-1.3.2-webos.ipk\"}]}";
    CONFERE(acharIpk(j, url, sizeof url, hash, sizeof hash, AT_SUFIXO), "nome NuvioTV-*-webos: achou a variante");
#ifdef NV_TEX_MB_FIXO
    CONFERE(!strcmp(hash, "bbbb"), "highcache pelo nome novo: [%s]", url);
#else
    CONFERE(!strcmp(hash, "cccc") && !strstr(url, "highcache"), "normal pelo nome novo, sem highcache: [%s]", url);
#endif
  }

  // RELEASE SEM O ANEXO DESTA VARIANTE: nao instala, e isso e o certo. Trocar
  // de variante calada e o defeito; mandar para a pagina e a saida honesta.
  { char url[160], hash[80];
    const char *so_outra =
#ifdef NV_TEX_MB_FIXO
      "{\"assets\":[{\"browser_download_url\":\"https://x/space.nuvio_1.1.3_arm.ipk\"}]}";
#else
      "{\"assets\":[{\"browser_download_url\":\"https://x/space.nuvio_1.1.3_arm-highcache.ipk\"}]}";
#endif
    CONFERE(!acharIpk(so_outra, url, sizeof url, hash, sizeof hash, AT_SUFIXO),
            "so a outra variante devolve 0");
    CONFERE(url[0] == 0, "e a url fica vazia"); }

  // JSON com escapes variados
  { char d[64];
    CONFERE(textoJson("{\"body\":\"a\\nb \\\"c\\\" \\u00e9 d\"}", "body", d, sizeof d), "escapes");
    CONFERE(!strcmp(d, "a\nb \"c\"   d"), "escapes decodificados: [%s]", d); }

  printf(falhas ? "atualizacao: %d falhas\n" : "atualizacao: ok\n", falhas);
  if (argc < 2 || falhas) return falhas ? 1 : 0;

  // captura
  { SDL_Window *w; SDL_GLContext gl; int i;
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    w = SDL_CreateWindow("atualizacao", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    assert(w); gl = SDL_GL_CreateContext(w); assert(gl);
    SDL_GL_SetSwapInterval(0);
    glViewport(0, 0, 1920, 1080);
    gfx_tamanho_alvo(1920, 1080);
    assert(gfx_iniciar());
    assert(txt_iniciar("deploy/app", 1));
    tex_iniciar(64);
    snprintf(tagNova, sizeof tagNova, "1.0.54");
    aberto = 1;
    for (i = 0; i < 60; i++) {
      SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro();
      atualizacao_atualizar(1.0f / 60.0f, SDL_GetTicks());
      glClearColor(0.025f, 0.025f, 0.03f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
      atualizacao_desenhar(SDL_GetTicks());
      if (i == 59) {
        unsigned char *pix = malloc(1920 * 1080 * 4); SDL_Surface *s; int y;
        glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
        s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
        for (y = 0; y < 1080; y++)
          memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
        assert(SDL_SaveBMP(s, argv[1]) == 0);
        SDL_FreeSurface(s); free(pix);
      }
      SDL_GL_SwapWindow(w);
    }
    printf("captura: %s\n", argv[1]); }
  return 0;
}
