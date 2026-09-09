#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// PERSISTENCIA NO ALVO TIZEN: IDBFS, e nao o sistema de arquivos padrao.
//
// O sistema de arquivos que o Emscripten monta por padrao (MEMFS) vive na
// memoria da pagina e MORRE A CADA RECARGA. Com ele o app grava a sessao, diz
// "[dados] gravando em ...", e no arranque seguinte esqueceu tudo — o mesmo
// defeito mudo que o cabecalho de dados.h descreve para o webOS, com outra
// causa. Pior aqui do que la: cada esquecimento obriga a refazer o login por QR,
// e cada login consome cota do servidor.
//
// IDBFS guarda em IndexedDB, que sobrevive a recarga e a reinicio da TV. Nao e
// automatico: e preciso montar, carregar UMA vez na abertura (syncfs(true)) e
// gravar de volta depois das mudancas (syncfs(false)).
EM_ASYNC_JS(int, nv_idbfs_montar, (const char *ponto), {
  var caminho = UTF8ToString(ponto);
  try {
    FS.mkdirTree(caminho);
    FS.mount(IDBFS, {}, caminho);
  } catch (e) {
    // EBUSY = ja montado (uma segunda chamada nao e erro).
    if (!e || e.errno !== 16) { return 0; }
  }
  return await new Promise(function (r) {
    FS.syncfs(true, function (err) { r(err ? 0 : 1); });
  });
});

// DESCARGA SEM ESPERAR O IndexedDB.
//
// A versao anterior era EM_ASYNC_JS e o laco principal ficava suspenso ate a
// transacao do IndexedDB confirmar. MEDIDO no Chrome, com a conta sincronizando
// e a home baixando arte: picos isolados de 100 ms (que e o TETO do clamp de dt
// em main.c — o pico real e maior) e duas janelas de 3 s inteiras caindo para
// 11,7 e 15,7 FPS, contra 120 FPS e pior quadro de 10 ms no resto do tempo.
//
// O caro NAO e o trabalho de sistema de arquivos. FS.syncfs varre a arvore, le
// o conteudo dos arquivos mudados e entrega tudo ao IndexedDB de forma
// SINCRONA; so entao espera a transacao confirmar. Essa espera e a maior parte
// dos 100 ms, e ela nao precisa acontecer dentro do quadro — quando a chamada
// volta, os dados ja sairam do MEMFS e estao com o navegador.
//
// A bandeira em voo existe porque duas syncfs simultaneas reconciliam a mesma
// arvore contra o mesmo banco: a segunda enxerga um estado que a primeira ainda
// esta gravando.
EM_JS(void, nv_idbfs_gravar, (), {
  Module.nvSyncEmVoo = 1;
  try {
    FS.syncfs(false, function (err) {
      Module.nvSyncEmVoo = 0;
      if (err) { out("[dados] syncfs falhou: " + err); }
    });
  } catch (e) { Module.nvSyncEmVoo = 0; out("[dados] syncfs lancou: " + e); }
});

EM_JS(int, nv_idbfs_em_voo, (), { return Module.nvSyncEmVoo ? 1 : 0; });

// REDE DE SEGURANCA DE SAIDA. A descarga periodica cobre o app rodando; ela nao
// cobre o quadro que nunca vai existir. Quando a pagina e escondida o
// requestAnimationFrame PARA, entao o laco principal para junto e a escrita mais
// recente ficaria so no MEMFS — que morre com a pagina. Sair do app na TV e
// recarregar no navegador passam por aqui, e perder a sessao nesse ponto obriga
// a refazer o login por QR.
//
// Nao ha como pegar o mutex do C a partir de um ouvinte de evento JS. O
// contador `ocupado` diz quantos fios estao DENTRO da trava; esperar alguns
// milissegundos por ele e invisivel numa pagina que esta sumindo, e a
// alternativa — uma syncfs concorrente com a escrita de um fio — e exatamente o
// congelamento silencioso descrito na nota da trava, abaixo.
EM_JS(void, nv_idbfs_rede_de_seguranca, (int *ocupado), {
  if (Module.nvSaidaArmada) return;
  Module.nvSaidaArmada = 1;
  var idx = ocupado >> 2;
  var descarregar = function () {
    var t = Date.now();
    while (Atomics.load(HEAP32, idx) !== 0 && Date.now() - t < 50) {}
    // Ja ha uma descarga em voo: os dados dela ja foram entregues ao IndexedDB
    // e comecar outra por cima e o caso que a bandeira existe para impedir.
    if (Module.nvSyncEmVoo) return;
    Module.nvSyncEmVoo = 1;
    try { FS.syncfs(false, function () { Module.nvSyncEmVoo = 0; }); }
    catch (e) { Module.nvSyncEmVoo = 0; }
  };
  addEventListener("pagehide", descarregar);
  // 'hidden' e o que chega ao sair do app na TV; 'pagehide' pode nem vir.
  addEventListener("visibilitychange", function () {
    if (document.visibilityState === "hidden") descarregar();
  });
});

// Marcadas por quem grava, consumidas por dados_sincronizar no laco de desenho.
//
// Gravar dentro de dados_gravar seria o obvio e esta ERRADO: dados_gravar e
// chamada de fios de trabalho, e a descarga tem de sair de UM lugar so para
// poder ser agrupada e contada. O laco principal recolhe.
//
// Duas bandeiras e nao uma: o cache de imagens suja em TODO quadro enquanto a
// home rola, e com uma bandeira so ele arrastaria a cadencia da sessao para a
// dele — ou o contrario, uma descarga por quadro, que e justamente o defeito
// que esta funcao existe para remover.
static volatile int sujo, sujoLeve;
static int idbfsMontado = 0;

// Espera minima entre duas descargas de dado do USUARIO. A varredura sincrona
// da arvore custa quase o mesmo tendo mudado um arquivo ou trinta, entao
// agrupar a rajada de escritas de um ciclo de sync (perfis, progresso, addons,
// colecoes) numa descarga so troca dez varreduras por uma. 700 ms e curto o
// bastante para a rede de seguranca de saida quase nunca ser a unica coisa
// entre a escrita e o IndexedDB.
#define NV_DESC_MIN_MS   700.0
// E a do cache de imagens sozinho. Conteudo re-obtivel nao paga uma descarga
// por si so; quando o usuario grava qualquer coisa, ele pega carona.
#define NV_DESC_LEVE_MS  15000.0
static double ultimaDesc;

// TRAVA DE SISTEMA DE ARQUIVOS, e so no alvo Tizen.
//
// No webOS e no Mac cada fio abre o seu FILE* e o kernel resolve. No WASM o
// "sistema de arquivos" e uma estrutura de dados JavaScript compartilhada entre
// os workers, e ela NAO e segura entre fios: dois fios gravando ao mesmo tempo,
// ou um fio gravando enquanto o laco principal roda syncfs, corrompem o estado.
//
// MEDIDO: com o fio de sync escrevendo 169 colecoes e o blob de ajustes de 19 KB
// enquanto o laco principal descarregava para o IndexedDB, o app CONGELOU — o
// log parou em 37 linhas e o contador de quadro nunca mais imprimiu. Nao houve
// erro, nao houve excecao: so parou.
static pthread_mutex_t dadosTrava = PTHREAD_MUTEX_INITIALIZER;
// Quantos fios estao DENTRO da trava. Existe para o JS, que nao tem como pegar
// um pthread_mutex_t: e o unico jeito de a rede de seguranca de saida saber que
// nao pode chamar syncfs agora.
static volatile int fsOcupado;
#define NV_FS_TRAVAR()   do { pthread_mutex_lock(&dadosTrava); \
                              __atomic_add_fetch(&fsOcupado, 1, __ATOMIC_SEQ_CST); } while (0)
#define NV_FS_LIBERAR()  do { __atomic_sub_fetch(&fsOcupado, 1, __ATOMIC_SEQ_CST); \
                              pthread_mutex_unlock(&dadosTrava); } while (0)
#else
#define NV_FS_TRAVAR()   ((void)0)
#define NV_FS_LIBERAR()  ((void)0)
#endif

// Fora do #ifdef: main.c imprime estes numeros em todo alvo, e no webOS e no
// Mac eles ficam em zero porque nao ha descarga nenhuma a fazer.
int    dados_desc_n;
double dados_desc_ms;
void dados_desc_zerar(void) { dados_desc_n = 0; dados_desc_ms = 0.0; }

void dados_fs_travar(void)  { NV_FS_TRAVAR(); }
void dados_fs_liberar(void) { NV_FS_LIBERAR(); }

void dados_marcar_sujo(int leve) {
#ifdef __EMSCRIPTEN__
  if (leve) sujoLeve = 1; else sujo = 1;
#else
  (void)leve;
#endif
}

static char dir[512];
static char clienteId[64];

// Tenta criar a pasta e escrever nela. Criar nao basta: em varios pontos do
// sistema de arquivos do aparelho o mkdir passa e o open falha depois, e um
// teste que so olha o mkdir escolheria uma pasta onde nada e gravado.
static int serve(const char *candidato) {
  char teste[600];
  FILE *f;
  if (!candidato || !*candidato) return 0;
  mkdir(candidato, 0755);   // ja existir nao e erro para o que interessa aqui
  snprintf(teste, sizeof teste, "%s/.escrita", candidato);
  f = fopen(teste, "w");
  if (!f) return 0;
  if (fputs("ok\n", f) < 0) { fclose(f); return 0; }
  if (fclose(f) != 0) return 0;
  remove(teste);
  return 1;
}

void dados_iniciar(const char *dirArte) {
  char lar[512];
  const char *env = getenv("NUVIO_DADOS");
  const char *home = getenv("HOME");
  const char *candidatos[5];
  int n = 0, i;

#ifdef __EMSCRIPTEN__
  // Primeiro candidato, e nao mais um da fila: no navegador todos os outros
  // caminhos existem e aceitam escrita (MEMFS aceita tudo), entao qualquer um
  // deles "venceria" a sonda e a escrita seria perdida na recarga seguinte sem
  // uma linha de log sequer.
  if (nv_idbfs_montar("/nuvio")) {
    candidatos[n++] = "/nuvio";
    idbfsMontado = 1;
    nv_idbfs_rede_de_seguranca((int *)&fsOcupado);
  } else {
    // ESTA LINHA ROLAVA DO PAINEL e o defeito passava por outra coisa.
    //
    // Sem IDBFS nada persiste: Trakt reautentica a cada abertura, progresso nao
    // e gravado e o sync nao tem o que empurrar — os tres sintomas que o dono
    // relatou juntos. E o widget roda em origem file:// (os rastros de erro
    // mostram file:///index.js), onde o Chromium BLOQUEIA IndexedDB; entao esta
    // e uma falha esperada neste alvo, nao um acidente.
    //
    // Por isso o estado passa a sair TAMBEM no relatorio de 3 s, onde nao rola:
    // um estado assim precisa ser visivel o tempo todo, nao uma vez no arranque.
    printf("[dados] IDBFS NAO MONTOU: nada persiste — Trakt, progresso e sync\n"
           "        morrem ao fechar. Origem file:// bloqueia IndexedDB.\n");
  }
#endif
  if (env && *env) candidatos[n++] = env;
  if (home && *home) {
    snprintf(lar, sizeof lar, "%s/.nuvio", home);
    candidatos[n++] = lar;
  }
  // Pasta de trabalho do modo desenvolvedor do webOS. Existe e e gravavel nos
  // aparelhos onde este app roda hoje; num aparelho de loja pode nao existir, e
  // por isso ela e candidata e nao resposta.
  candidatos[n++] = "/media/developer/temp/nuvio";
  if (dirArte && *dirArte) candidatos[n++] = dirArte;

  for (i = 0; i < n; i++) {
    if (serve(candidatos[i])) {
      snprintf(dir, sizeof dir, "%s", candidatos[i]);
      printf("[dados] gravando em %s\n", dir);
      fflush(stdout);
      return;
    }
    printf("[dados] recusou %s\n", candidatos[i]);
  }
  dir[0] = 0;
  printf("[dados] NENHUMA pasta gravavel: sessao e ajustes nao vao sobreviver "
         "ao proximo arranque\n");
  fflush(stdout);
}

const char *dados_dir(void) { return dir; }

void dados_sincronizar(void) {
#ifdef __EMSCRIPTEN__
  double agora, espera, custo;
  if (!sujo && !sujoLeve) return;
  agora = emscripten_get_now();
  espera = sujo ? NV_DESC_MIN_MS : NV_DESC_LEVE_MS;
  if (agora - ultimaDesc < espera) return;
  if (nv_idbfs_em_voo()) return;
  // Limpar ANTES de chamar, e nao depois, e o que garante que nenhuma escrita
  // se perde: quem gravar durante a varredura ou ja entrou nela (e a bandeira
  // volta a 1 para uma descarga extra, inofensiva) ou esta bloqueado na trava
  // e sera pego na proxima. O caro seria o contrario — limpar depois apagaria a
  // marca de uma escrita que a varredura nao viu.
  sujo = 0; sujoLeve = 0;
  ultimaDesc = agora;
  // A trava cobre a parte SINCRONA do syncfs, que e onde a arvore e lida.
  NV_FS_TRAVAR();
  nv_idbfs_gravar();
  NV_FS_LIBERAR();
  custo = emscripten_get_now() - agora;
  dados_desc_n++;
  if (custo > dados_desc_ms) dados_desc_ms = custo;
#endif
}

char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  if (!dir[0] || !nome || !*nome) return NULL;
  snprintf(dst, tam, "%s/%s", dir, nome);
  return dst;
}

// GRAVACAO QUE NAO MERECE UMA DESCARGA SO PARA ELA.
//
// `leve` escolhe entre os dois relogios do topo deste arquivo: 700 ms para dado
// do usuario, 15 s para conteudo re-obtivel. A diferenca aparece na TV Samsung
// e nao na LG, porque no WASM a descarga e FS.syncfs e ela custa 30 a 52 ms
// SINCRONOS — medido no aparelho do relator do #21, uma descarga a cada janela
// de relatorio enquanto ele so andava com o foco.
//
// A posicao da home era o gatilho: ela e gravada quando o foco descansa, e ia
// pelo caminho de 700 ms. Andar pela home virava uma descarga a cada movimento.
// Posicao de cursor e re-obtivel — perde-la ao fechar o app custa uma rolagem,
// contra 30-52 ms de quadro travado a cada passo.
static int gravarInterno(const char *nome, const char *conteudo, int leve) {
  char caminho[600], tmp[600];
  FILE *f;
  size_t n;
  if (!dados_caminho(caminho, sizeof caminho, nome)) return 0;
  snprintf(tmp, sizeof tmp, "%s.tmp", caminho);
  NV_FS_TRAVAR();
  f = fopen(tmp, "w");
  if (!f) { NV_FS_LIBERAR(); return 0; }
  n = conteudo ? strlen(conteudo) : 0;
  if (n && fwrite(conteudo, 1, n, f) != n) { fclose(f); remove(tmp); NV_FS_LIBERAR(); return 0; }
  if (fclose(f) != 0) { remove(tmp); NV_FS_LIBERAR(); return 0; }
  if (rename(tmp, caminho) != 0) { remove(tmp); NV_FS_LIBERAR(); return 0; }
  NV_FS_LIBERAR();
#ifdef __EMSCRIPTEN__
  if (leve) sujoLeve = 1; else sujo = 1;
#else
  (void)leve;
#endif
  return 1;
}

int dados_gravar(const char *nome, const char *conteudo) {
  return gravarInterno(nome, conteudo, 0);
}

int dados_gravar_leve(const char *nome, const char *conteudo) {
  return gravarInterno(nome, conteudo, 1);
}

char *dados_ler(const char *nome) {
  char caminho[600];
  FILE *f;
  long n;
  char *buf;
  if (!dados_caminho(caminho, sizeof caminho, nome)) return NULL;
  NV_FS_TRAVAR();
  f = fopen(caminho, "rb");
  if (!f) { NV_FS_LIBERAR(); return NULL; }
  fseek(f, 0, SEEK_END);
  n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n < 0) { fclose(f); NV_FS_LIBERAR(); return NULL; }
  buf = (char *)malloc((size_t)n + 1);
  if (!buf) { fclose(f); NV_FS_LIBERAR(); return NULL; }
  n = (long)fread(buf, 1, (size_t)n, f);
  fclose(f);
  NV_FS_LIBERAR();
  buf[n] = 0;
  return buf;
}

int dados_apagar(const char *nome) {
  char caminho[600];
  if (!dados_caminho(caminho, sizeof caminho, nome)) return 0;
  NV_FS_TRAVAR();
  if (remove(caminho) != 0) { NV_FS_LIBERAR(); return 0; }
  NV_FS_LIBERAR();
#ifdef __EMSCRIPTEN__
  sujo = 1;
#endif
  return 1;
}

void dados_uuid(char *dst, unsigned tam) {
  static const char *hex = "0123456789abcdef";
  static int semeado;
  int i;
  if (tam < 37) { if (tam) dst[0] = 0; return; }
  if (!semeado) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid() ^ (unsigned)(size_t)dst);
    semeado = 1;
  }
  for (i = 0; i < 36; i++) {
    if (i == 8 || i == 13 || i == 18 || i == 23) { dst[i] = '-'; continue; }
    if (i == 14) { dst[i] = '4'; continue; }              // versao
    if (i == 19) { dst[i] = hex[8 + (rand() & 3)]; continue; }  // variante
    dst[i] = hex[rand() & 15];
  }
  dst[36] = 0;
}

const char *dados_cliente_id(void) {
  char *lido;
  if (clienteId[0]) return clienteId;

  lido = dados_ler("cliente.txt");
  if (lido) {
    char *fim = lido + strlen(lido);
    while (fim > lido && (fim[-1] == '\n' || fim[-1] == '\r' || fim[-1] == ' ')) *--fim = 0;
    if (lido[0]) snprintf(clienteId, sizeof clienteId, "%s", lido);
    free(lido);
    if (clienteId[0]) return clienteId;
  }

  // Formato de UUID v4 porque e o que o servidor recebe do Android e do web; a
  // aleatoriedade nao precisa ser criptografica — este numero identifica um
  // aparelho para nao ecoar a propria escrita, nao protege nada.
  dados_uuid(clienteId, sizeof clienteId);

  { char linha[64];
    snprintf(linha, sizeof linha, "%s\n", clienteId);
    dados_gravar("cliente.txt", linha); }
  return clienteId;
}

int dados_persistente(void) {
#ifdef __EMSCRIPTEN__
  return idbfsMontado;
#else
  return 1;   /* disco de verdade */
#endif
}
