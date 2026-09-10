#include "fileiras.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Uma linha da escolha local. A ORDEM do vetor E a ordem da home: nao ha campo
// de posicao porque um numero de ordem gravado ao lado do indice sempre acaba
// discordando dele — foi o que o formato moderno do blob da conta precisou
// resolver com um sort, e aqui basta mover o elemento.
typedef struct {
  char chave[FIL_CHAVE];
  char titulo[FIL_TITULO];
  int  oculta;
  int  tipo;    // FilTipo
  int  tam;     // FilTam
  // SO EM MEMORIA — nao entram no arquivo. Ver o cabecalho de fil_registrar em
  // fileiras.h: o formato gravado tem o titulo no fim da linha e acrescentar
  // campos faria o arquivo de quem ja usa o app ser descartado inteiro.
  char addon[48];      // nome do addon que declara o catalogo; "" desconhecido
  char conteudo[8];    // "movie" | "series" | ""
  int  itens;          // titulos que a fileira tem agora; -1 desconhecido
} Linha;

static Linha linhas[FIL_MAX];
static int   nLinhas;
static int   limite = FIL_LIMITE_PADRAO;
static int   ordemLocal;      // 1 = a pessoa MOVEU algo; ver fil_tem_ordem
static int   carregado;
static int   registroSujo;   // ver fil_gravar_registro
static unsigned revisao;
// A leitura acontece no fio da DESCOBERTA (montar) e a escrita no fio de
// desenho (tela de Ajustes). Sao os dois unicos, e a secao critica e uma varredura
// de 64 strings — mutex simples, sem leitura sem trava: uma lista sendo
// reordenada no meio de uma varredura devolveria a mesma fileira duas vezes.
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

// Rotulos em portugues. Passam por i18n em text.c como todo texto de tela; os
// pares en estao em idioma_tab.h.
static const char *TIPO_ROT[FIL_TIPO_N] = {
  "Automático", "Cartaz em pé", "Destaque largo", "Coleção", "Serviço", "Top 10"
};
static const char *TAM_ROT[FIL_TAM_N] = { "Compacto", "Padrão", "Grande" };
// 0,85 e 1,2 e nao 0,5 e 2,0: o card do web mede 212x322 e o passo da fileira
// foi dimensionado para ele. Um fator maior que ~1,2 faz a fileira deitada
// ocupar mais que a metade util da tela e a de baixo sair do recorte.
static const float TAM_ESC[FIL_TAM_N] = { 0.85f, 1.0f, 1.2f };

const char *fil_tipo_rotulo(int t) {
  return (t >= 0 && t < FIL_TIPO_N) ? TIPO_ROT[t] : TIPO_ROT[FIL_TIPO_AUTO];
}
const char *fil_tam_rotulo(int t) {
  return (t >= 0 && t < FIL_TAM_N) ? TAM_ROT[t] : TAM_ROT[FIL_TAM_PADRAO];
}
float fil_tam_escala(int t) {
  return (t >= 0 && t < FIL_TAM_N) ? TAM_ESC[t] : 1.0f;
}

// ------------------------------------------------------------------ origem

// A CHAVE JA DIZ DE ONDE A FILEIRA VEIO; esta funcao so le o que esta la.
//
// As tres chaves sinteticas sao escritas a mao em home.c e nao existem em addon
// nenhum: "continue_watching" e a retomada, "social_activity" e o feed dos
// amigos e "last_session" e o "Retomar agora" do pos-player. O prefixo
// `collection_` e o que col_chave_grupo monta, e e o mesmo que a conta usa na
// ordem de catalogos (ver colecoes.h). Todo o resto e homeCatalogKey —
// <addonId>_<tipo>_<catalogoId> — e portanto catalogo de addon.
//
// NAO HA ORIGEM "LISTA DO TRAKT", e a ausencia foi conferida: a watchlist e a
// colecao do Trakt entram como ITENS do catalogo (descoberta.c, trakt_lista),
// nunca como fileira. Inventar um selo para uma origem que a home nao produz
// seria mentira na tela.
int fil_origem_de(const char *chave) {
  if (!chave || !chave[0]) return FIL_ORIGEM_APP;
  if (!strcmp(chave, "continue_watching") ||
      !strcmp(chave, "social_activity") ||
      !strcmp(chave, "last_session")) return FIL_ORIGEM_APP;
  if (!strncmp(chave, "collection_", 11)) return FIL_ORIGEM_COLECAO;
  return FIL_ORIGEM_CATALOGO;
}

// Rotulos CURTOS: o selo e lido de relance, a 3 m, ao lado do nome da fileira.
// A frase inteira mora em fil_origem_ajuda e vai para a area de ajuda.
static const char *ORIGEM_ROT[FIL_ORIGEM_N] = { "Do app", "Coleção", "Catálogo" };
// Icones REAIS (SVG do app web rasterizado em art/icones), nunca forma
// desenhada a mao no shader — ver a nota de gfx_icone em gfx.h.
static const char *ORIGEM_ICONE[FIL_ORIGEM_N] = {
  "menu_home",     // fileira montada pelo proprio app
  "menu_library",  // grupo de colecoes
  "addon"          // catalogo de addon (o encaixe de quebra-cabeca)
};
static const char *ORIGEM_AJUDA[FIL_ORIGEM_N] = {
  "Fileira montada pelo próprio app com o que você já assistiu ou salvou. Não vem de addon nenhum.",
  "Grupo de coleções: uma pasta de atalhos para catálogos, montada na sua conta.",
  "Catálogo de um addon. Cada fileira destas é um pedido pela rede quando a Home monta."
};

const char *fil_origem_rotulo(int o) {
  return (o >= 0 && o < FIL_ORIGEM_N) ? ORIGEM_ROT[o] : ORIGEM_ROT[FIL_ORIGEM_APP];
}
const char *fil_origem_icone(int o) {
  return (o >= 0 && o < FIL_ORIGEM_N) ? ORIGEM_ICONE[o] : ORIGEM_ICONE[FIL_ORIGEM_APP];
}
const char *fil_origem_ajuda(int o) {
  return (o >= 0 && o < FIL_ORIGEM_N) ? ORIGEM_AJUDA[o] : ORIGEM_AJUDA[FIL_ORIGEM_APP];
}

// ------------------------------------------------------------------ arquivo

// Chaves SINTETICAS e grupos de colecao: a forma do card delas nao e escolha
// desta tela. Ver o cabecalho de fil_aceita_tipo em fileiras.h. E a mesma
// pergunta que fil_origem_de responde, e por isso nao ha uma segunda lista de
// chaves aqui: duas listas divergem no dia em que uma chave nova nascer.
static int formaFixa(const char *chave) {
  return fil_origem_de(chave) != FIL_ORIGEM_CATALOGO;
}

static int limita(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// GRAVA POR dados_gravar, E NAO COM fopen DIRETO. As duas coisas que o fopen
// direto nao fazia sao invisiveis no Mac e no webOS e FATAIS no Tizen:
//
//   1. NAO MARCA A LOJA COMO SUJA. No alvo Tizen "gravar" escreve em MEMFS, que
//      e RAM; o que leva o arquivo para o IndexedDB e dados_sincronizar(), e ela
//      so descarrega quando dados_gravar avisou que houve escrita. Sem esse
//      aviso a escolha de fileiras vivia ate a recarga e sumia — que e
//      exatamente o relato "mexo nos catalogos e eles nao recarregam ou salvam".
//   2. NAO PEGA A TRAVA DO SISTEMA DE ARQUIVOS. O FS do Emscripten e uma
//      estrutura JavaScript compartilhada entre os workers e NAO e segura entre
//      fios; esta funcao roda no fio do desenho (a tela de Ajustes) enquanto a
//      descoberta le no fio dela. O sintoma documentado de ignorar isso, em
//      dados.c, foi o app inteiro CONGELAR sem erro nenhum.
//
// dados_gravar ja e atomica (temporario + rename), ja pega a trava e ja marca a
// sujeira. Montar o texto e entrega-lo pronto e menos codigo do que estava aqui.
static void gravar(void) {
  // 64 linhas de ate ~310 bytes (chave 192 + titulo 96 + quatro numeros), mais
  // o cabecalho. Alocado e nao na pilha: sao ~20 KB e esta funcao roda no fio
  // do desenho.
  size_t cap = 128 + 64 + (size_t)FIL_MAX * (FIL_CHAVE + FIL_TITULO + 40);
  char *txt = malloc(cap);
  size_t k;
  int i;
  if (!txt) return;
  // O comentario vai NO ARQUIVO: quem o encontrar pela primeira vez vai
  // procurar de onde ele e sincronizado, e a resposta e "de lugar nenhum".
  k = (size_t)snprintf(txt, cap,
        "# Fileiras da Home, escolha DESTE aparelho. Nunca e enviada para\n"
        "# a conta: ver o cabecalho de src/fileiras.h.\n"
        "limite %d\nordem %d\n", limite, ordemLocal);
  for (i = 0; i < nLinhas && k < cap; i++)
    // Tabulacao e nao espaco: titulo de catalogo tem espaco dentro ("For You -
    // Filme") e a chave do Xperience carrega o id inteiro do addon.
    k += (size_t)snprintf(txt + k, cap - k, "linha %s\t%d\t%d\t%d\t%s\n",
                          linhas[i].chave, linhas[i].oculta, linhas[i].tipo,
                          linhas[i].tam, linhas[i].titulo);
  if (k < cap) dados_gravar("fileirasui.txt", txt);
  free(txt);
  revisao++;
}

static void carregar(void) {
  char caminho[600], buf[900];
  FILE *f;
  carregado = 1;
  if (!dados_caminho(caminho, sizeof caminho, "fileirasui.txt")) return;
  f = fopen(caminho, "r");
  if (!f) return;
  while (fgets(buf, sizeof buf, f)) {
    char *fim = buf + strlen(buf);
    while (fim > buf && (fim[-1] == '\n' || fim[-1] == '\r')) *--fim = 0;
    if (!buf[0] || buf[0] == '#') continue;
    if (!strncmp(buf, "limite ", 7)) {
      limite = limita(atoi(buf + 7), FIL_LIMITE_MIN, FIL_LIMITE_MAX);
    } else if (!strncmp(buf, "ordem ", 6)) {
      ordemLocal = atoi(buf + 6) ? 1 : 0;
    } else if (!strncmp(buf, "linha ", 6) && nLinhas < FIL_MAX) {
      char *p = buf + 6, *campo[4];
      int c;
      for (c = 0; c < 4; c++) {
        char *tab = strchr(p, '\t');
        // Linha truncada (arquivo de outra versao, ou editado a mao) e
        // DESCARTADA em vez de aplicada pela metade: metade dela poria a chave
        // de uma fileira com o tipo de outra.
        if (!tab) break;
        *tab = 0; campo[c] = p; p = tab + 1;
      }
      if (c < 4) continue;
      memset(&linhas[nLinhas], 0, sizeof linhas[nLinhas]);
      // -1 e nao 0: "ainda nao sei" e diferente de "vazia". Uma fileira lida do
      // arquivo so ganha contagem quando a home a monta nesta sessao, e mostrar
      // "0 títulos" antes disso acusaria de vazia uma fileira cheia.
      linhas[nLinhas].itens = -1;
      snprintf(linhas[nLinhas].chave,  FIL_CHAVE,  "%s", campo[0]);
      snprintf(linhas[nLinhas].titulo, FIL_TITULO, "%s", p);
      linhas[nLinhas].oculta = atoi(campo[1]) ? 1 : 0;
      linhas[nLinhas].tipo   = limita(atoi(campo[2]), 0, FIL_TIPO_N - 1);
      linhas[nLinhas].tam    = limita(atoi(campo[3]), 0, FIL_TAM_N - 1);
      if (formaFixa(linhas[nLinhas].chave)) linhas[nLinhas].tipo = FIL_TIPO_AUTO;
      nLinhas++;
    }
  }
  fclose(f);
  printf("[fileiras] escolha local: limite %d, %d fileira(s) conhecida(s)%s\n",
         limite, nLinhas, ordemLocal ? ", ordem propria" : "");
  fflush(stdout);
}

static void garantir(void) { if (!carregado) carregar(); }

// ------------------------------------------------------------------ limite

int fil_limite(void) {
  int v;
  pthread_mutex_lock(&trava);
  garantir();
  v = limite;
  pthread_mutex_unlock(&trava);
  return v;
}

void fil_definir_limite(int n) {
  pthread_mutex_lock(&trava);
  garantir();
  n = limita(n, FIL_LIMITE_MIN, FIL_LIMITE_MAX);
  if (n != limite) { limite = n; gravar(); }
  pthread_mutex_unlock(&trava);
}

// ------------------------------------------------------------------ registro

static int achar(const char *chave) {
  int i;
  for (i = 0; i < nLinhas; i++) if (!strcmp(linhas[i].chave, chave)) return i;
  return -1;
}

void fil_registrar(const char *chave, const char *titulo,
                   const char *addon, const char *conteudo, int itens) {
  int i, grava = 0;
  if (!chave || !chave[0]) return;
  pthread_mutex_lock(&trava);
  garantir();
  i = achar(chave);
  if (i < 0) {
    // Chave nova entra no FIM, nunca no meio: a mesma regra do
    // ensureOrderKeysWithPrefs do web. Catalogo que o addon passou a declarar
    // hoje nao pode empurrar para baixo a fileira que a pessoa deixou no topo.
    if (nLinhas >= FIL_MAX) { pthread_mutex_unlock(&trava); return; }
    i = nLinhas++;
    memset(&linhas[i], 0, sizeof linhas[i]);
    snprintf(linhas[i].chave, FIL_CHAVE, "%s", chave);
    linhas[i].tam = FIL_TAM_PADRAO;
    linhas[i].itens = -1;
    grava = 1;
  }
  // O PRIMEIRO A REGISTRAR MANDA NO NOME, e nao o ultimo. Sao dois
  // registradores: a descoberta com o nome do catalogo (ja com customTitles) e
  // a home com o nome que ela desenha, que para meia dezena de chaves conhecidas
  // e diferente ("Trending Movies" no lugar de "trending movies - Filme"). Com
  // "o ultimo manda", cada ciclo de sync trocava o nome de volta, a revisao
  // subia e a lista era remontada por nada — e o nome piscava na tela de
  // Ajustes. O nome do catalogo e o que IDENTIFICA a fileira, entao ele fica.
  if (titulo && titulo[0] && !linhas[i].titulo[0]) {
    snprintf(linhas[i].titulo, FIL_TITULO, "%s", titulo);
    grava = 1;
  }
  // NENHUM DOS TRES MEXE EM `grava`. Eles nao vao para o arquivo (ver
  // fil_registrar em fileiras.h), e `itens` em particular muda a cada ciclo de
  // sync: marcar sujeira aqui reescreveria fileirasui.txt e bumparia `revisao`
  // a cada volta da descoberta, que e o defeito que o nome piscando ja causou.
  //
  // Mesma regra de "quem souber primeiro preenche" do titulo, e pelo mesmo
  // motivo: sao DOIS registradores e so um deles conhece o addon. A descoberta
  // registra com o nome do addon e o tipo do catalogo; home.c registra a lista
  // final, onde ha fileira que addon nenhum declarou.
  if (addon && addon[0] && !linhas[i].addon[0])
    snprintf(linhas[i].addon, sizeof linhas[i].addon, "%s", addon);
  if (conteudo && conteudo[0] && !linhas[i].conteudo[0])
    snprintf(linhas[i].conteudo, sizeof linhas[i].conteudo, "%s", conteudo);
  // A contagem e a excecao: o ULTIMO a saber manda, porque ela e um retrato do
  // agora. -1 significa "nao sei" e nunca apaga um numero ja conhecido.
  if (itens >= 0) linhas[i].itens = itens;
  // Marca, nao grava: quem varre registra dezenas de chaves em sequencia e uma
  // reescrita por chave e escrita em flash sem motivo. O flush e explicito, em
  // fil_gravar_registro. E "so quando MUDOU": a descoberta registra as mesmas
  // ~16 chaves a cada ciclo de sync, e nenhuma delas e novidade.
  if (grava) registroSujo = 1;
  pthread_mutex_unlock(&trava);
}

void fil_gravar_registro(void) {
  pthread_mutex_lock(&trava);
  if (registroSujo) { registroSujo = 0; gravar(); }
  pthread_mutex_unlock(&trava);
}

int fil_n(void) { int v; pthread_mutex_lock(&trava); garantir(); v = nLinhas; pthread_mutex_unlock(&trava); return v; }

const char *fil_chave(int i) {
  return (i >= 0 && i < nLinhas) ? linhas[i].chave : "";
}
const char *fil_titulo(int i) {
  // Sem titulo conhecido a tela mostra a CHAVE, que e feia mas verdadeira.
  // Inventar "Fileira 3" esconderia qual catalogo esta sendo desligado.
  if (i < 0 || i >= nLinhas) return "";
  return linhas[i].titulo[0] ? linhas[i].titulo : linhas[i].chave;
}
int fil_linha_oculta(int i) { return (i >= 0 && i < nLinhas) ? linhas[i].oculta : 0; }
int fil_linha_tipo(int i)   { return (i >= 0 && i < nLinhas) ? linhas[i].tipo : FIL_TIPO_AUTO; }
int fil_linha_tam(int i)    { return (i >= 0 && i < nLinhas) ? linhas[i].tam : FIL_TAM_PADRAO; }

int fil_linha_origem(int i) {
  return fil_origem_de((i >= 0 && i < nLinhas) ? linhas[i].chave : "");
}
const char *fil_linha_addon(int i) {
  return (i >= 0 && i < nLinhas) ? linhas[i].addon : "";
}
const char *fil_linha_conteudo(int i) {
  // Traduz o codigo do protocolo para a palavra da tela. "movie"/"series" e o
  // que os addons falam; ninguem sentado no sofa precisa ver isso.
  const char *t = (i >= 0 && i < nLinhas) ? linhas[i].conteudo : "";
  if (!strcmp(t, "movie"))  return "Filmes";
  if (!strcmp(t, "series")) return "Séries";
  return "";
}
int fil_linha_itens(int i) {
  return (i >= 0 && i < nLinhas) ? linhas[i].itens : -1;
}
int fil_aceita_tipo(int i)  { return (i >= 0 && i < nLinhas) && !formaFixa(linhas[i].chave); }

// ------------------------------------------------------------------ mutacao

void fil_alternar(int i) {
  pthread_mutex_lock(&trava);
  if (i >= 0 && i < nLinhas) { linhas[i].oculta = !linhas[i].oculta; gravar(); }
  pthread_mutex_unlock(&trava);
}

void fil_ciclar_tipo(int i) {
  pthread_mutex_lock(&trava);
  if (i >= 0 && i < nLinhas && !formaFixa(linhas[i].chave)) {
    linhas[i].tipo = (linhas[i].tipo + 1) % FIL_TIPO_N;
    gravar();
  }
  pthread_mutex_unlock(&trava);
}

void fil_ciclar_tam(int i) {
  pthread_mutex_lock(&trava);
  if (i >= 0 && i < nLinhas) {
    linhas[i].tam = (linhas[i].tam + 1) % FIL_TAM_N;
    gravar();
  }
  pthread_mutex_unlock(&trava);
}

// A LISTA DE AJUSTES SEGUE A HOME ATE A PRIMEIRA REORDENACAO.
//
// `linhas[]` nascia na ordem de REGISTRO, que e a ordem em que cada fileira
// apareceu pela primeira vez — nao a ordem em que a home a desenha. Enquanto
// ninguem reordena isso nao aparece, porque fil_unir devolve a identidade. Mas
// no PRIMEIRO fil_mover `ordemLocal` vira 1 e `linhas[]` passa a mandar de uma
// vez: a home inteira salta da ordem da descoberta para a ordem de registro.
//
// Na TV o efeito e o do relato — "mexo na reordenacao e ele caga, joga pra
// baixo o do topo": as fileiras fixas ("Continuar assistindo", "Amigos
// assistindo") sao registradas por home.c DEPOIS das de catalogo, entao no
// primeiro movimento elas desabavam para o fim da home.
//
// Com isto a lista de Ajustes ja mostra a ordem real da home, e o primeiro
// movimento move UMA fileira em vez de reembaralhar todas.
//
// NAO GRAVA E NAO MEXE EM `revisao`: e chamada do fio de desenho a cada
// remontagem, e bumpar a revisao aqui faria a home se remontar para sempre.
// Nada se perde por nao gravar — enquanto ordemLocal e 0 esta ordem e
// recalculada da home a cada arranque.
void fil_espelhar_ordem(const char *const *chaves, int n) {
  static Linha novo[FIL_MAX];
  char usado[FIL_MAX];
  int i, k = 0;
  if (!chaves || n < 1) return;
  pthread_mutex_lock(&trava);
  garantir();
  if (!ordemLocal && nLinhas > 0) {
    // SO REORDENA O QUE A TELA CONHECE. Quem ela nao mencionou fica EXATAMENTE
    // onde estava — nao vai para o fim.
    //
    // A versao antiga reconstruia a lista inteira: primeiro as chaves da tela,
    // na ordem dela, e depois "todo o resto" atras. Parece inofensivo, mas esta
    // funcao roda A CADA REMONTAGEM, e no arranque a home esta pela metade — os
    // catalogos entram em ~16 publicacoes e as colecoes da conta chegam depois
    // de todas elas. Cada remontagem precoce empurrava as colecoes para o fim
    // da ordem.
    //
    // E dali elas nao voltavam: no fim da ordem o limite de fileiras as corta,
    // cortadas nunca aparecem na tela, e nao aparecendo nunca sao promovidas de
    // volta. Um poco. A unica saida era desligar e religar a fileira a mao — e
    // o proximo arranque rebaixava tudo de novo, que e exatamente o relato do
    // #30: "toggling makes it appear again; exit and reopen and it disappears".
    //
    // Agora a tela so permuta as fileiras dela DENTRO DAS POSICOES QUE ELAS JA
    // OCUPAM. Uma home pela metade nao tem como mexer em quem ela nem viu.
    int slots[FIL_MAX], m = 0;
    char posto[FIL_MAX];
    memset(usado, 0, sizeof usado);
    for (i = 0; i < n; i++) {
      int p = chaves[i] ? achar(chaves[i]) : -1;
      if (p >= 0) usado[p] = 1;
    }
    for (i = 0; i < nLinhas && m < FIL_MAX; i++)
      if (usado[i]) slots[m++] = i;
    memcpy(novo, linhas, sizeof(Linha) * (size_t)nLinhas);
    memset(posto, 0, sizeof posto);
    for (i = 0; i < n && k < m; i++) {
      int p = chaves[i] ? achar(chaves[i]) : -1;
      if (p >= 0 && !posto[p]) { posto[p] = 1; novo[slots[k++]] = linhas[p]; }
    }
    memcpy(linhas, novo, sizeof(Linha) * (size_t)nLinhas);
  }
  pthread_mutex_unlock(&trava);
}

int fil_mover(int i, int direcao) {
  int j = i + (direcao > 0 ? 1 : -1);
  pthread_mutex_lock(&trava);
  if (i >= 0 && i < nLinhas && j >= 0 && j < nLinhas) {
    Linha t = linhas[i]; linhas[i] = linhas[j]; linhas[j] = t;
    // A partir do primeiro movimento a ordem local EXISTE e passa a vencer a da
    // conta. Antes disso o arquivo so guarda liga/desliga e forma, e a ordem
    // continua sendo a do app web — que e o certo para quem nunca mexeu aqui.
    ordemLocal = 1;
    gravar();
    i = j;
  }
  pthread_mutex_unlock(&trava);
  return i;
}

// ------------------------------------------------------------------ consulta

int fil_oculta(const char *chave) {
  int i, v = 0;
  if (!chave || !chave[0]) return 0;
  pthread_mutex_lock(&trava);
  garantir();
  i = achar(chave);
  if (i >= 0) v = linhas[i].oculta;
  pthread_mutex_unlock(&trava);
  return v;
}

int fil_tipo(const char *chave) {
  int i, v = FIL_TIPO_AUTO;
  if (!chave || !chave[0]) return FIL_TIPO_AUTO;
  pthread_mutex_lock(&trava);
  garantir();
  i = achar(chave);
  if (i >= 0) v = linhas[i].tipo;
  pthread_mutex_unlock(&trava);
  return v;
}

float fil_escala(const char *chave) {
  int i;
  float v = 1.0f;
  if (!chave || !chave[0]) return 1.0f;
  pthread_mutex_lock(&trava);
  garantir();
  i = achar(chave);
  if (i >= 0) v = fil_tam_escala(linhas[i].tam);
  pthread_mutex_unlock(&trava);
  return v;
}

unsigned fil_revisao(void) {
  unsigned v;
  pthread_mutex_lock(&trava);
  garantir();
  v = revisao;
  pthread_mutex_unlock(&trava);
  return v;
}

int fil_tem_ordem(void) {
  int v;
  pthread_mutex_lock(&trava);
  garantir();
  v = ordemLocal && nLinhas > 0;
  pthread_mutex_unlock(&trava);
  return v;
}

int fil_unir(const char *const *chaves, int n, int *saida, int max) {
  int q = 0, i, j;
  char usado[FIL_MAX * 8];
  if (!chaves || !saida || n < 1) return 0;
  if (n > (int)sizeof usado) n = (int)sizeof usado;
  pthread_mutex_lock(&trava);
  garantir();
  memset(usado, 0, (size_t)n);
  if (ordemLocal) {
    for (i = 0; i < nLinhas; i++)
      for (j = 0; j < n && q < max; j++)
        if (!usado[j] && chaves[j] && !strcmp(chaves[j], linhas[i].chave)) {
          saida[q++] = j; usado[j] = 1; break;
        }
  }
  // O QUE A ESCOLHA LOCAL NAO CONHECE NAO SE PERDE: vai para o fim, na ordem em
  // que ja estava (que e a da conta, aplicada antes deste ponto). Mesma regra e
  // mesmo motivo de catordem_unir — sem este laco, catalogo instalado depois de
  // a ordem local ter sido gravada sumiria da home.
  for (j = 0; j < n && q < max; j++) if (!usado[j]) saida[q++] = j;
  pthread_mutex_unlock(&trava);
  return q;
}

void fil_esquecer(void) {
  pthread_mutex_lock(&trava);
  nLinhas = 0;
  ordemLocal = 0;
  limite = FIL_LIMITE_PADRAO;
  carregado = 1;   // nao reler o arquivo de quem saiu
  memset(linhas, 0, sizeof linhas);
  gravar();
  pthread_mutex_unlock(&trava);
}
