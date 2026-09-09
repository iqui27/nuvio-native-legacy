// Cache em disco do catalogo: ONDE ele e gravado e DE QUEM ele e.
//
// Os dois defeitos que este arquivo cobre eram invisiveis no Mac e fatais no
// Tizen e depois de um logout, respectivamente:
//
//   (a) o cache ia para a pasta do PACOTE (`dirArte`). No Tizen /app/art vem de
//       --preload-file, ou seja MEMFS: RAM apagada a cada recarga. A escrita
//       nao falha, o arquivo simplesmente nao existe na proxima abertura, e o
//       app pagava de novo os 14,5 s de rede medidos na TV.
//
//   (b) o cabecalho validava so a ESTRUTURA (magia, versao, sizeof, contagens).
//       Depois de trocar de conta ou de perfil, a home nascia com o catalogo da
//       ANTERIOR — e cada CatFileira leva `base[600]`, campo desse tamanho
//       porque o Xperience embute um JWT no CAMINHO. O arquivo e credencial.
//
// So src/catalogo.c e linkado, com dubles para tudo o resto: o cache nao
// depende de SDL, de rede nem da descoberta, e linkar o app inteiro aqui
// tornaria o teste lento e fragil (mesma razao de tests/catordem.sh).
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "../src/catalogo.h"
#include "../src/progresso.h"

// O cache grava o idioma no cabecalho (texto ja montado nao pode ser lido de
// volta no idioma errado). 0 = portugues, que e o padrao.
int ajustes_idioma_ingles(void) { return 0; }
// cat_carregar traduz o rotulo do tipo do catalogo do pacote por aqui. Devolver
// a entrada e o que i18n faz em portugues, que e o idioma deste teste.
const char *i18n(const char *s) { return s; }

// --- DUBLES ------------------------------------------------------------------
// A pasta gravavel e a identidade sao entradas do teste, nao do ambiente: e
// mexendo nelas que se exercita a regra.
static char  dirDados[512];
static char  usuarioAtual[64];
static int   perfilAtual = 1;

const char *dados_dir(void)     { return dirDados; }
const char *sessao_usuario(void){ return usuarioAtual; }
int         perfis_ativo(void)  { return perfilAtual; }

const char *desc_genero_pt(const char *g) { return g; }
int prog_ler(ProgRegistro *saida, int max) { (void)saida; (void)max; return 0; }
int prog_gravar_local(const char *imdb, int t, int e, double p, double d) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; return 0;
}

// --- APOIO -------------------------------------------------------------------
static int existe(const char *dir, const char *nome) {
  char c[700]; struct stat st;
  snprintf(c, sizeof c, "%s/%s", dir, nome);
  return stat(c, &st) == 0;
}

// Nos blocos de IDENTIDADE nao interessa em qual das duas pastas o arquivo caiu
// — interessa se ele existe. Olhar so uma delas faria o resultado depender do
// bloco anterior em vez da regra sob teste.
static int cacheEmDisco(const char *a, const char *b) {
  return existe(a, "catalogo-rede.bin") || existe(b, "catalogo-rede.bin");
}

static int falhas;
static void confere(const char *o_que, int ok) {
  printf("  %-58s %s\n", o_que, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}

// Um catalogo minimo mas REALISTA: a fileira leva uma `base` com cara de URL de
// addon com token no caminho, que e o dado cujo vazamento o teste (b) impede.
static void semearCatalogo(const char *marca) {
  CatItem *itens = calloc(4, sizeof *itens);
  CatFileira fil;
  int i;
  assert(itens);
  for (i = 0; i < 4; i++) {
    snprintf(itens[i].imdb, sizeof itens[i].imdb, "tt%d%s", 1000 + i, marca);
    snprintf(itens[i].titulo, sizeof itens[i].titulo, "Titulo %d de %s", i, marca);
  }
  memset(&fil, 0, sizeof fil);
  snprintf(fil.chave, sizeof fil.chave, "addon_movie_top");
  snprintf(fil.titulo, sizeof fil.titulo, "Top de %s", marca);
  snprintf(fil.tipo, sizeof fil.tipo, "movie");
  snprintf(fil.base, sizeof fil.base,
           "https://xperience.invalid/manifest/pid-%s/jwt-do-%s", marca, marca);
  snprintf(fil.catId, sizeof fil.catId, "top");
  fil.ini = 0; fil.n = 4;
  cat_definir_tudo(itens, 4, &fil, 1);
  free(itens);
}

int main(void) {
  char raiz[512], dirPacote[600];
  const char *tmp = getenv("TMPDIR");
  snprintf(raiz, sizeof raiz, "%snuvio-catcache-%d",
           tmp && *tmp ? tmp : "/tmp/", (int)getpid());
  snprintf(dirDados, sizeof dirDados, "%s/dados", raiz);
  snprintf(dirPacote, sizeof dirPacote, "%s/pacote", raiz);
  mkdir(raiz, 0755); mkdir(dirDados, 0755); mkdir(dirPacote, 0755);

  // (a) A PASTA GRAVAVEL GANHA DA PASTA DO PACOTE.
  //
  // O chamador continua passando `dirArte` — home.c e descoberta.c nao mudaram.
  // Quem decide e catalogo.c, e por isso as duas pontas concordam sozinhas.
  printf("==> cache vai para a pasta gravavel, nao para a do pacote\n");
  snprintf(usuarioAtual, sizeof usuarioAtual, "uuid-da-alice");
  perfilAtual = 1;
  semearCatalogo("alice");
  confere("cat_gravar_cache disse que gravou", cat_gravar_cache(dirPacote));
  confere("arquivo na pasta gravavel",    existe(dirDados, "catalogo-rede.bin"));
  confere("NADA na pasta do pacote",     !existe(dirPacote, "catalogo-rede.bin"));
  confere("nenhum .tmp esquecido",       !existe(dirDados, "catalogo-rede.bin.tmp"));
  // E o leitor tem de achar o que o escritor deixou, recebendo o MESMO dirArte.
  confere("cat_ler_cache acha o que foi gravado", cat_ler_cache(dirPacote) == 1);
  confere("catalogo do cache na memoria", cat_n() == 4 && cat_do_cache());

  // (a2) SEM PASTA GRAVAVEL, VOLTA A SER COMO ERA.
  //
  // dados_dir() pode ser "" quando nenhum candidato aceitou escrita (src/dados.h).
  // Nesse caso perder o cache seria pior que grava-lo no pacote.
  printf("\n==> sem pasta gravavel, cai para dirArte como antes\n");
  dirDados[0] = 0;
  confere("gravou mesmo assim",           cat_gravar_cache(dirPacote));
  confere("arquivo na pasta do pacote",   existe(dirPacote, "catalogo-rede.bin"));
  confere("leitor acha no mesmo lugar",   cat_ler_cache(dirPacote) == 1);
  { char c[700];
    snprintf(c, sizeof c, "%s/catalogo-rede.bin", dirPacote);
    remove(c); }
  snprintf(dirDados, sizeof dirDados, "%s/dados", raiz);

  // Daqui para baixo o assunto e a IDENTIDADE, e nao mais o lugar: cada bloco
  // grava logo antes de ler, e a presenca do arquivo e conferida nas duas
  // pastas (cacheEmDisco). Assim estas assercoes falham por causa da regra que
  // testam, e nao de tabela por causa do defeito do bloco anterior.

  // (b) CACHE DE OUTRO USUARIO E RECUSADO E APAGADO.
  printf("\n==> cache de outro usuario e recusado e apagado\n");
  snprintf(usuarioAtual, sizeof usuarioAtual, "uuid-da-alice");
  semearCatalogo("alice");
  confere("alice gravou",                   cat_gravar_cache(dirPacote));
  confere("arquivo da alice em disco",      cacheEmDisco(dirDados, dirPacote));
  snprintf(usuarioAtual, sizeof usuarioAtual, "uuid-do-bob");
  confere("bob NAO recebe o cache da alice", cat_ler_cache(dirPacote) == 0);
  confere("e o arquivo dela foi APAGADO",   !cacheEmDisco(dirDados, dirPacote));

  // (b2) TROCAR DE PERFIL NA MESMA CONTA TAMBEM CONTA.
  //
  // Este e o caso que o logout nao ve: sync_esquecer_usuario nao roda numa troca
  // de perfil, e a watchlist e o continuar assistindo sao POR PERFIL.
  printf("\n==> trocar de perfil na mesma conta tambem invalida\n");
  semearCatalogo("bob-p1");
  confere("perfil 1 gravou",                cat_gravar_cache(dirPacote));
  perfilAtual = 2;
  confere("perfil 2 NAO recebe o do 1",     cat_ler_cache(dirPacote) == 0);
  confere("arquivo apagado",               !cacheEmDisco(dirDados, dirPacote));
  perfilAtual = 1;

  // (b3) O MESMO DONO CONTINUA SENDO ACEITO — a regra nao pode virar "nunca ha
  // cache", que passaria em (b) e desligaria a funcionalidade inteira.
  printf("\n==> o proprio dono continua aproveitando o cache\n");
  semearCatalogo("bob-de-novo");
  confere("gravou",                         cat_gravar_cache(dirPacote));
  confere("e releu",                        cat_ler_cache(dirPacote) == 1);
  confere("arquivo continua em disco",      cacheEmDisco(dirDados, dirPacote));

#ifndef NV_CACHE_SEM_APAGAR
  // cat_apagar_cache: a metade do logout que mora aqui. Compile com
  // -DNV_CACHE_SEM_APAGAR para rodar este teste contra a versao ANTERIOR ao
  // conserto, onde a funcao ainda nao existia.
  printf("\n==> cat_apagar_cache tira o arquivo do disco\n");
  confere("apagou",                         cat_apagar_cache());
  confere("nao ha mais arquivo",           !cacheEmDisco(dirDados, dirPacote));
  confere("apagar de novo diz que nao havia", cat_apagar_cache() == 0);
#endif

  printf("\n%s\n", falhas ? "catcache: TEM FALHA" : "catcache: tudo ok");
  return falhas ? 1 : 0;
}
