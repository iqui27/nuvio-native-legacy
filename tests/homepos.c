// Onde a pessoa estava na home: sobrevive a REMONTAGEM e ao FECHAMENTO.
//
// Tres coisas que este arquivo cobra, e que nenhum outro teste via:
//   (a) fechar e reabrir devolve fileira, coluna e rolagem horizontal;
//   (b) se as fileiras de hoje nao sao as de ontem, o foco cai em (0,0) e NADA
//       e posicionado por aproximacao;
//   (c) `colunaLembrada[]` atravessa uma remontagem — era o que morria no
//       memset de focus_iniciar, e o defeito que focus.h diz que a estrutura
//       existe para evitar.
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
int dados_gravar(const char *nome, const char *conteudo) {
  for (int i = 0; i < nArq; i++)
    if (!strcmp(arqNome[i], nome)) {
      free(arqDado[i]); arqDado[i] = strdup(conteudo); return 1;
    }
  if (nArq >= TESTE_MAX_ARQ) return 0;
  arqNome[nArq] = strdup(nome); arqDado[nArq] = strdup(conteudo); nArq++;
  return 1;
}
// NULL de proposito, como em home_layout.c: fileiras.c sai cedo em carregar() e
// a escolha local nasce vazia, que e o estado de quem nunca abriu o app.
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  (void)dst; (void)tam; (void)nome; return NULL;
}
int   dados_apagar(const char *nome) { (void)nome; return 1; }
int   perfis_ativo(void) { return 1; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }

// Indice da fileira de chave `chave`, ou -1.
static int idx(const char *chave) {
  for (int r = 0; r < nFileiras; r++)
    if (!strcmp(fileiras[r].chave, chave)) return r;
  return -1;
}

// Estado de "app recem-aberto": o foco zerado e a rolagem em zero, com as
// tabelas vivas vazias. E o que home_iniciar deixa antes da primeira montagem.
static void reabrir(void) {
  memset(&foco, 0, sizeof foco);
  memset(scrollX, 0, sizeof scrollX);
  memset(velX, 0, sizeof velX);
  nPosViva = 0; posVivaFoco[0] = 0; posVivaCol = 0;
  posLer();
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

  // --- (c) colunaLembrada atravessa a remontagem ----------------------------
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

  // --- (a) a posicao volta depois de um ciclo gravar/ler --------------------
  foco.fileira = alvo; foco.coluna = 2;
  posDiscoPendente = 0;
  posGravar();
  const char *gravado = NULL;
  for (int i = 0; i < nArq; i++)
    if (!strcmp(arqNome[i], "home-pos.txt")) gravado = arqDado[i];
  assert(gravado);
  assert(strstr(gravado, "f 2 pos_3\n"));
  // Nada de credencial no arquivo: so chave de fileira, coluna e rolagem. A
  // `base` da fileira e URL de addon com JWT no caminho e nao pode vazar.
  assert(!strstr(gravado, "example.invalid"));

  reabrir();
  assert(posDiscoPendente);
  montar(itens, ontem, 6);
  alvo = idx("pos_3"); vizinha = idx("pos_1");
  assert(foco.fileira == alvo && foco.coluna == 2);
  assert(scrollX[alvo] == 240.0f);
  assert(foco.colunaLembrada[vizinha] == 3);
  assert(!posDiscoPendente);   // consumido uma vez so

  // --- (b) conjunto diferente: (0,0), em silencio ---------------------------
  reabrir();
  assert(posDiscoPendente);
  montar(itens, hoje, 6);
  assert(idx("pos_3") < 0);
  assert(foco.fileira == 0 && foco.coluna == 0);
  for (int r = 0; r < nFileiras; r++) assert(foco.colunaLembrada[r] == 0);
  // A leitura NAO foi consumida: nenhuma fileira de ontem estava na tela para
  // receber o foco, e restaurar pela metade seria pior que nao restaurar.
  assert(posDiscoPendente);

  // Um SUBCONJUNTO tambem nao serve: a home do arranque passa por estados
  // intermediarios em que a chave gravada ja existe e a home ainda vai mudar
  // de forma embaixo do foco.
  reabrir();
  montar(itens, ontem, 3);
  assert(idx("pos_0") >= 0);            // uma chave de ontem ja esta na tela
  assert(foco.fileira == 0 && foco.coluna == 0);
  assert(posDiscoPendente);
  // E quando o resto chega, ai sim.
  montar(itens, ontem, 6);
  assert(foco.fileira == idx("pos_3") && foco.coluna == 2);
  assert(!posDiscoPendente);

  // --- arquivo de outra versao e ignorado, nao interpretado ----------------
  reabrir();
  dados_gravar("home-pos.txt", "v9\nf 2 pos_3\nr 2 240 pos_3\n");
  posLer();
  assert(!posDiscoPendente && nPosDisco == 0);

  free(itens);
  for (int i = 0; i < nArq; i++) { free(arqNome[i]); free(arqDado[i]); }
  puts("home pos: PASS (remontagem, ciclo gravar/ler, conjunto diferente)");
  return 0;
}
