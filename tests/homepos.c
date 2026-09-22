// Onde a pessoa estava na home: sobrevive a REMONTAGEM e morre no FECHAMENTO.
//
// Duas coisas que este arquivo cobra, e que nenhum outro teste ve:
//   (a) `colunaLembrada[]` e `scrollX[]` atravessam uma remontagem — era o que
//       morria no memset de focus_iniciar, e o defeito que focus.h diz que a
//       estrutura existe para evitar. E o "ir ao detalhe e voltar", e tambem as
//       ~16 republicacoes que a descoberta faz no arranque;
//   (b) #95: REABRIR O APP nao devolve coluna nenhuma. Toda fileira comeca no
//       primeiro cartaz e nada e escrito em disco a respeito — nem por
//       home_encerrar nem pelo foco parado.
//
// Sem janela, rede ou TV, como tests/home_layout.c: inclui src/home.c direto.
#include <assert.h>
#include "../src/home.c"

// DUBLE DE DISCO EM MEMORIA, e nao os dubles vazios de home_layout.c: aqui o
// ciclo gravar/ler E o assunto do teste, entao dados_gravar tem de devolver
// depois o que recebeu antes. Um arquivo de verdade em /tmp daria a mesma
// prova e ainda deixaria lixo entre execucoes.
#define TESTE_MAX_ARQ 8
static char *arqNome[TESTE_MAX_ARQ], *arqDado[TESTE_MAX_ARQ];
static int   nArq;

char *dados_ler(const char *nome) {
  for (int i = 0; i < nArq; i++)
    if (!strcmp(arqNome[i], nome)) return strdup(arqDado[i]);
  return NULL;
}
// Quantas vezes home-pos.txt (ou qualquer sucessor dele) foi escrito. O #95
// cobra ZERO: ver o bloco (b).
static int gravouPos;
int dados_gravar(const char *nome, const char *conteudo) {
  if (strstr(nome, "home-pos")) gravouPos++;
  for (int i = 0; i < nArq; i++)
    if (!strcmp(arqNome[i], nome)) {
      free(arqDado[i]); arqDado[i] = strdup(conteudo); return 1;
    }
  if (nArq >= TESTE_MAX_ARQ) return 0;
  arqNome[nArq] = strdup(nome); arqDado[nArq] = strdup(conteudo); nArq++;
  return 1;
}
// A posicao da home grava por AQUI, e nao por dados_gravar: a unica diferenca
// entre as duas e quando o Tizen descarrega para o IndexedDB (ver
// dados_gravar_leve), o que nao existe neste teste. O duble delega para nao
// haver duas copias do disco falso — com duas, gravar por um caminho e ler pelo
// outro passaria a devolver NULL e o teste acusaria a home, nao o duble.
int dados_gravar_leve(const char *nome, const char *conteudo) {
  return dados_gravar(nome, conteudo);
}
// NULL de proposito, como em home_layout.c: fileiras.c sai cedo em carregar() e
// a escolha local nasce vazia, que e o estado de quem nunca abriu o app.
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  (void)dst; (void)tam; (void)nome; return NULL;
}
int   dados_apagar(const char *nome) { (void)nome; return 1; }
void  dados_marcar_sujo(int leve) { (void)leve; }
void  sync_proteger_ajustes_locais(void) {}
int   perfis_ativo(void) { return 1; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
const char *addons_nome_por_id(const char *id) { (void)id; return ""; }

// Indice da fileira de chave `chave`, ou -1.
static int idx(const char *chave) {
  for (int r = 0; r < nFileiras; r++)
    if (!strcmp(fileiras[r].chave, chave)) return r;
  return -1;
}

// Estado de "app recem-aberto": o foco zerado e a rolagem em zero, com as
// tabelas vivas vazias. E exatamente o que home_iniciar deixa antes da
// primeira montagem — e, desde o #95, e TUDO o que ele deixa: nao ha leitura
// de disco nenhuma para imitar aqui.
static void reabrir(void) {
  memset(&foco, 0, sizeof foco);
  memset(scrollX, 0, sizeof scrollX);
  memset(velX, 0, sizeof velX);
  nPosViva = 0; posVivaFoco[0] = 0; posVivaCol = 0;
}

static void montar(CatItem *itens, CatFileira *fils, int n) {
  cat_definir_tudo(itens, 48, fils, n);
  filsAplicadas = -1;          // forca a remontagem mesmo sem mudanca de catalogo
  sincronizarFileiras();
}

int main(void) {
  // O teto no maximo: o limite de fabrica (7) cortaria fileiras e mudaria o
  // conjunto entre uma montagem e outra, que e justamente o que o caso (b)
  // quer isolar.
  fil_definir_limite(FIL_LIMITE_MAX);

  CatItem *itens = calloc(48, sizeof *itens);
  CatFileira ontem[6] = {0}, hoje[6] = {0};
  assert(itens);
  for (int i = 0; i < 6; i++) {
    snprintf(ontem[i].chave,  sizeof ontem[i].chave,  "pos_%d", i);
    snprintf(ontem[i].titulo, sizeof ontem[i].titulo, "Lista %d", i);
    snprintf(ontem[i].base,   sizeof ontem[i].base,   "https://example.invalid/addon");
    snprintf(ontem[i].tipo,   sizeof ontem[i].tipo,   "movie");
    snprintf(ontem[i].catId,  sizeof ontem[i].catId,  "pos%d", i);
    ontem[i].ini = i * 4; ontem[i].n = 4;
    hoje[i] = ontem[i];
    snprintf(hoje[i].chave, sizeof hoje[i].chave, "outra_%d", i);
    snprintf(hoje[i].catId, sizeof hoje[i].catId, "outra%d", i);
  }

  montar(itens, ontem, 6);
  int alvo = idx("pos_3"), vizinha = idx("pos_1");
  assert(alvo > 0 && vizinha >= 0 && alvo != vizinha);

  // A pessoa parou na coluna 2 de "pos_3", e ja tinha estado na coluna 3 de
  // "pos_1" — e essa segunda que so `colunaLembrada[]` sabe.
  foco.fileira = alvo; foco.coluna = 2;
  foco.colunaLembrada[alvo] = 2;
  foco.colunaLembrada[vizinha] = 3;
  scrollX[alvo] = 240.0f;

  // --- (a) colunaLembrada atravessa a remontagem ----------------------------
  //
  // A troca de duas fileiras de lugar muda a revisao do catalogo, entao a
  // montagem roda de verdade e os INDICES se mexem — que e o ponto: guardar por
  // indice devolveria a coluna de uma fileira para dentro de outra.
  { CatFileira t = ontem[1]; ontem[1] = ontem[4]; ontem[4] = t; }
  cat_definir_tudo(itens, 48, ontem, 6);
  sincronizarFileiras();
  alvo = idx("pos_3"); vizinha = idx("pos_1");
  assert(alvo >= 0 && vizinha >= 0);
  assert(foco.fileira == alvo && foco.coluna == 2);
  assert(scrollX[alvo] == 240.0f);
  assert(foco.colunaLembrada[vizinha] == 3);
  // E a memoria de coluna volta a fazer o que focus.h promete: descer e subir
  // devolve a coluna 3, nao a coluna 0.
  foco.fileira = vizinha; foco.coluna = foco.colunaLembrada[vizinha];
  assert(foco.coluna == 3);

  // --- (b) #95: reabrir comeca no primeiro cartaz --------------------------
  //
  // A pessoa fecha o app na coluna 2 de "pos_3", com a fileira rolada 240 px.
  // Reabrir tem de devolver a home INTEIRA na coluna 0 — foi o pedido literal
  // do issue ("I would expect the row to start from the first tile again").
  foco.fileira = alvo; foco.coluna = 2;
  foco.colunaLembrada[alvo] = 2;
  scrollX[alvo] = 240.0f;
  home_encerrar();
  // NENHUM home-pos.txt FOI ESCRITO. Este e o lado do #95 que uma asserção
  // sobre colunas nao cobre: enquanto o arquivo existir, basta alguem voltar a
  // le-lo no arranque para o defeito reaparecer inteiro.
  assert(gravouPos == 0);

  reabrir();
  montar(itens, ontem, 6);
  alvo = idx("pos_3"); vizinha = idx("pos_1");
  assert(alvo >= 0 && vizinha >= 0);
  assert(focoHero);
  assert(foco.fileira == 0 && foco.coluna == 0);
  for (int r = 0; r < nFileiras; r++) {
    assert(foco.colunaLembrada[r] == 0);
    assert(scrollX[r] == 0.0f);
  }
  // E continua sem gravar nada depois de a home montar.
  home_encerrar();
  assert(gravouPos == 0);

  free(itens);
  for (int i = 0; i < nArq; i++) { free(arqNome[i]); free(arqDado[i]); }
  puts("home pos: PASS (remontagem guarda coluna, reabrir zera e nao grava — #95)");
  return 0;
}
