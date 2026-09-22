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
  // NA FILA POR ESCOLHA. So quem foi ADICIONADO com a home cheia fica ligado
  // alem do limite; tudo o mais que passar do limite vira "fora" quando a
  // folha abre (fil_normalizar). Sem esta marca nao ha como distinguir "a
  // pessoa pediu e esta esperando vaga" de "sobrou do arquivo antigo" ou de
  // "o addon declarou hoje e entrou no fim ligado". Vai para o arquivo como
  // quinto numero, ANTES do titulo; o leitor aceita a linha sem ele.
  int  fila;
  // SO EM MEMORIA — nao entram no arquivo. Ver o cabecalho de fil_registrar em
  // fileiras.h: o formato gravado tem o titulo no fim da linha e acrescentar
  // campos faria o arquivo de quem ja usa o app ser descartado inteiro.
  char addon[48];      // nome do addon que declara o catalogo; "" desconhecido
  char conteudo[8];    // "movie" | "series" | ""
  int  itens;          // titulos que a fileira tem agora; -1 desconhecido
  int  vista;          // registrada nesta sessao (descoberta ou home)
  int  naHome;         // estava na ultima lista que a home montou
} Linha;

static Linha linhas[FIL_MAX];
static int   nLinhas;
static int   limite = FIL_LIMITE_PADRAO;
// PERFIL DONO DESTA ESCOLHA. Ate aqui fileirasui.txt era UM por aparelho:
// quem trocava de perfil na tela "Quem esta assistindo?" herdava a home que o
// outro arrumou. Agora cada perfil tem o seu (fileirasui-p<N>.txt); 0 e o
// nome antigo, que continua valendo para quem nunca escolheu perfil e serve de
// SEMENTE para o primeiro arquivo de cada perfil — ninguem perde a ordem que
// ja tinha no dia em que a separacao entrou.
static int   perfil;
static const char *arquivoDoPerfil(void) {
  static char nome[48];
  if (perfil <= 0) return "fileirasui.txt";
  snprintf(nome, sizeof nome, "fileirasui-p%d.txt", perfil);
  return nome;
}
static int   ordemLocal;      // 1 = a pessoa MOVEU algo; ver fil_tem_ordem
static int   carregado;
static int   registroSujo;   // ver fil_gravar_registro
// FONTE DO DESTAQUE. "" = automatico (os primeiros titulos do catalogo, que e
// o que a home sempre fez), "*" = sorteio do catalogo, qualquer outra coisa = a
// chave da fileira que alimenta o destaque.
//
// Mora aqui e nao em ajustes.txt porque o valor E UMA CHAVE DE FILEIRA: quem
// sabe se ela ainda existe, quem a renomeia e quem a poda quando o addon sai da
// conta e este arquivo. Em ajustes.txt seria um texto solto que ninguem
// revisita.
static char  heroFonte[FIL_CHAVE];
static unsigned revisao;
// A leitura acontece no fio da DESCOBERTA (montar) e a escrita no fio de
// desenho (tela de Ajustes). Sao os dois unicos, e a secao critica e uma varredura
// de 64 strings — mutex simples, sem leitura sem trava: uma lista sendo
// reordenada no meio de uma varredura devolveria a mesma fileira duas vezes.
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

// Rotulos em portugues. Passam por i18n em text.c como todo texto de tela; os
// pares en estao em idioma_tab.h.
static const char *TIPO_ROT[FIL_TIPO_N] = {
  "Automático", "Cartaz em pé", "Destaque largo", "Coleção", "Serviço", "Top 10",
  "Destaque 4:3"
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
  // Linha propria e com prefixo, como `limite` e `ordem`: o leitor ignora
  // prefixo que nao conhece, entao um arquivo escrito por esta versao continua
  // valendo numa anterior (ela so nao ve o destaque) e vice-versa.
  if (heroFonte[0] && k < cap)
    k += (size_t)snprintf(txt + k, cap - k, "hero %s\n", heroFonte);
  for (i = 0; i < nLinhas && k < cap; i++)
    // Tabulacao e nao espaco: titulo de catalogo tem espaco dentro ("For You -
    // Filme") e a chave do Xperience carrega o id inteiro do addon.
    k += (size_t)snprintf(txt + k, cap - k, "linha %s\t%d\t%d\t%d\t%d\t%s\n",
                          linhas[i].chave, linhas[i].oculta, linhas[i].tipo,
                          linhas[i].tam, linhas[i].fila, linhas[i].titulo);
  if (k < cap) dados_gravar(arquivoDoPerfil(), txt);
  free(txt);
  revisao++;
}

static void carregar(void) {
  char caminho[600], buf[900];
  FILE *f;
  carregado = 1;
  // ZERA A FONTE DO DESTAQUE ANTES DE LER, e nao so ao achar a linha.
  //
  // `heroFonte` e estatico e carregar() roda de novo na TROCA DE PERFIL. Sem
  // isto, um perfil cuja escolha e o automatico (arquivo sem a linha `hero`)
  // herdava a fileira escolhida pelo perfil anterior — e o destaque do Gustavo
  // apareceria na home do Henrique sem ninguem ter escolhido nada. O teste que
  // pegou isso usa um arquivo de versao anterior, que e o mesmo caso: linha
  // ausente tem de significar "automatico", nunca "o que estava na memoria".
  heroFonte[0] = 0;
  if (!dados_caminho(caminho, sizeof caminho, arquivoDoPerfil())) return;
  f = fopen(caminho, "r");
  // Perfil sem arquivo proprio ainda: comeca do arquivo antigo do aparelho,
  // que e o que a pessoa via ate ontem. A primeira mutacao grava o proprio.
  if (!f && perfil > 0 && dados_caminho(caminho, sizeof caminho, "fileirasui.txt"))
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
    } else if (!strncmp(buf, "hero ", 5)) {
      snprintf(heroFonte, sizeof heroFonte, "%s", buf + 5);
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
      // QUINTO NUMERO OPCIONAL (fila), antes do titulo. Arquivo antigo nao o
      // tem, e ai `p` ja e o titulo. Um titulo nunca comeca por "digito+TAB".
      { char *tab = strchr(p, '\t');
        if (tab && tab > p && tab - p <= 2 && p[0] >= '0' && p[0] <= '9') {
          linhas[nLinhas].fila = atoi(p) ? 1 : 0;
          p = tab + 1;
        } }
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

// ----------------------------------------------------------------- destaque

const char *fil_hero_fonte(void) {
  static char copia[FIL_CHAVE];
  pthread_mutex_lock(&trava);
  garantir();
  snprintf(copia, sizeof copia, "%s", heroFonte);
  pthread_mutex_unlock(&trava);
  return copia;
}

void fil_definir_hero_fonte(const char *chave) {
  pthread_mutex_lock(&trava);
  garantir();
  if (strcmp(heroFonte, chave ? chave : "")) {
    snprintf(heroFonte, sizeof heroFonte, "%s", chave ? chave : "");
    gravar();
  }
  pthread_mutex_unlock(&trava);
}

// As primeiras `limite` linhas LIGADAS, na ordem local, sao a home; as ligadas
// depois disso sao a FILA (entram sozinhas quando alguem sai); as ocultas estao
// fora. Ver fil_estado. Devolve, para a linha i, quantas ligadas ha antes dela.
static int posicaoLigada(int i) {
  int k, p = 0;
  for (k = 0; k < i && k < nLinhas; k++) if (!linhas[k].oculta) p++;
  return p;
}

void fil_definir_limite(int n) {
  pthread_mutex_lock(&trava);
  garantir();
  n = limita(n, FIL_LIMITE_MIN, FIL_LIMITE_MAX);
  if (n != limite) {
    // LIMITE MENOR: quem ficou de fora VIRA "fora da home", e nao fila. Decisao
    // do dono ("viram fora da home"): a fila e para quem a pessoa ACABOU de
    // pedir e nao coube; quem foi empurrado por um limite menor nao pediu
    // nada, e re-entrar sozinho depois seria a home mudando por conta propria.
    if (n < limite) {
      int i, p = 0;
      for (i = 0; i < nLinhas; i++) {
        if (linhas[i].oculta) continue;
        if (p >= n) { linhas[i].oculta = 1; linhas[i].fila = 0; }
        p++;
      }
    }
    limite = n; gravar();
  }
  pthread_mutex_unlock(&trava);
}

int fil_estado(int i) {
  int r = FIL_FORA;
  pthread_mutex_lock(&trava);
  garantir();
  if (i >= 0 && i < nLinhas && !linhas[i].oculta)
    r = posicaoLigada(i) < limite ? FIL_NA_HOME : FIL_NA_FILA;
  pthread_mutex_unlock(&trava);
  return r;
}

int fil_n_na_home(void) {
  int i, p = 0;
  pthread_mutex_lock(&trava);
  garantir();
  for (i = 0; i < nLinhas; i++) if (!linhas[i].oculta) p++;
  pthread_mutex_unlock(&trava);
  return p < limite ? p : limite;
}

int fil_n_fila(void) {
  int i, p = 0;
  pthread_mutex_lock(&trava);
  garantir();
  for (i = 0; i < nLinhas; i++) if (!linhas[i].oculta) p++;
  pthread_mutex_unlock(&trava);
  return p > limite ? p - limite : 0;
}

// ADICIONAR A HOME: liga e vai para o FIM do bloco ligado. Se ainda cabe no
// limite, entra na home; senao entra na fila, atras de quem ja esperava — e
// sobe sozinha quando alguem for removido, porque a fila e so a ordem. Devolve
// o indice novo da linha (ela pode ter se movido) e escreve o estado em
// `estado`. Marca ordemLocal: colocar alguem no fim e uma escolha de posicao.
int fil_adicionar(int i, int *estado) {
  int j, ultimo = -1;
  Linha tmp;
  pthread_mutex_lock(&trava);
  garantir();
  if (i < 0 || i >= nLinhas) { pthread_mutex_unlock(&trava); if (estado) *estado = FIL_FORA; return i; }
  linhas[i].oculta = 0;
  for (j = nLinhas - 1; j >= 0; j--) if (!linhas[j].oculta && j != i) { ultimo = j; break; }
  // Ja esta depois do ultimo ligado: nao ha para onde ir.
  if (ultimo >= 0 && i < ultimo) {
    tmp = linhas[i];
    memmove(&linhas[i], &linhas[i + 1], sizeof(Linha) * (size_t)(ultimo - i));
    linhas[ultimo] = tmp;
    i = ultimo;
    ordemLocal = 1;
  }
  linhas[i].fila = posicaoLigada(i) < limite ? 0 : 1;
  if (estado) *estado = linhas[i].fila ? FIL_NA_FILA : FIL_NA_HOME;
  gravar();
  pthread_mutex_unlock(&trava);
  return i;
}

// NORMALIZA: ligada alem do limite SEM a marca de fila vira "fora"; ligada
// dentro do limite perde a marca (ja entrou). Decisao do dono: "o que tiver
// fora do limite ja colocar no fora da home, para facilitar". Roda quando a
// folha abre e depois de cada leva de registros — e o que impede o arquivo
// antigo (100 ligadas, limite 14) de virar uma fila de 86.
//
// PROTECAO: linha que a home ESTA desenhando (naHome) nunca e escondida por
// aqui, mesmo alem do limite — e o caso da "vaga garantida por addon", que a
// descoberta promove para dentro da janela por conta propria.
void fil_normalizar(void) {
  int i, p = 0, mudou = 0;
  pthread_mutex_lock(&trava);
  garantir();
  for (i = 0; i < nLinhas; i++) {
    if (linhas[i].oculta) continue;
    if (p < limite) { if (linhas[i].fila) { linhas[i].fila = 0; mudou = 1; } }
    else if (!linhas[i].fila && !linhas[i].naHome) { linhas[i].oculta = 1; mudou = 1; continue; }
    p++;
  }
  if (mudou) gravar();
  pthread_mutex_unlock(&trava);
}

// REMOVER DA HOME: desliga. A posicao fica — se a pessoa religar, volta ao
// fim do bloco por fil_adicionar. Quem estava na fila sobe por consequencia.
void fil_remover(int i) {
  pthread_mutex_lock(&trava);
  garantir();
  if (i >= 0 && i < nLinhas && !linhas[i].oculta) { linhas[i].oculta = 1; linhas[i].fila = 0; gravar(); }
  pthread_mutex_unlock(&trava);
}

// PODA DE FANTASMAS. Um addon removido da conta deixa os catalogos dele na
// lista — e na home, ate o proximo login (relato do @rawldon: "ghost
// entries... only disappear after I sign out and back in"). Chamada quando
// TODOS os manifestos da volta foram lidos, com os ids e as bases dos addons
// que existem AGORA: catalogo cuja chave nao comeca por nenhum deles, e que
// ninguem registrou nesta sessao, e de um addon que ja nao esta na conta.
//
// So catalogo: fileira do app e grupo de colecao nao tem addon. So `vista == 0`:
// o que foi visto nesta sessao esta vivo por definicao, e a dupla condicao
// protege contra chamar isto cedo demais. Devolve quantas linhas sairam.
int fil_podar_catalogos(const char *const *ids, const char *const *bases, int n) {
  int i, w = 0, fora = 0;
  pthread_mutex_lock(&trava);
  garantir();
  for (i = 0; i < nLinhas; i++) {
    int vivo = 1;
    if (fil_origem_de(linhas[i].chave) == FIL_ORIGEM_CATALOGO && !linhas[i].vista) {
      int k;
      vivo = 0;
      for (k = 0; k < n && !vivo; k++) {
        size_t li = ids[k] ? strlen(ids[k]) : 0, lb = bases[k] ? strlen(bases[k]) : 0;
        if (li && !strncmp(linhas[i].chave, ids[k], li) && linhas[i].chave[li] == '_') vivo = 1;
        if (lb && !strncmp(linhas[i].chave, bases[k], lb) && linhas[i].chave[lb] == '_') vivo = 1;
      }
    }
    if (vivo) { if (w != i) linhas[w] = linhas[i]; w++; }
    else fora++;
  }
  if (fora) { nLinhas = w; gravar(); }
  pthread_mutex_unlock(&trava);
  if (fora) { printf("[fileiras] %d fileira(s) de addon que ja nao existe sairam da lista\n", fora); fflush(stdout); }
  return fora;
}

// TROCA DE PERFIL: solta a lista e le o arquivo do perfil novo na proxima
// consulta. Nao grava nada aqui — o arquivo do perfil que saiu ja esta em dia.
void fil_definir_perfil(int p) {
  pthread_mutex_lock(&trava);
  if (p < 0) p = 0;
  if (p != perfil) {
    perfil = p;
    nLinhas = 0; ordemLocal = 0; limite = FIL_LIMITE_PADRAO;
    memset(linhas, 0, sizeof linhas);
    carregado = 0;
    revisao++;
  }
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
    // TABELA CHEIA: DESPEJA UMA DISPENSAVEL EM VEZ DE RECUSAR A NOVA.
    //
    // Recusar em silencio foi um defeito real, medido na C9 em 15/09/2026: a
    // conta declara 279 catalogos, o arquivo tinha as 192 linhas do teto de
    // entao, e SEIS fileiras desenhadas na home nao existiam na tela de
    // fileiras — nao dava para move-las nem para desliga-las. A lista tem de
    // conter, sempre, o que a pessoa esta vendo; e o proposito dela.
    //
    // O QUE PODE SAIR: so entrada que (a) nao esta na home agora, (b) nao foi
    // vista nesta sessao, e (c) esta com tudo no padrao — ninguem desligou,
    // nem escolheu forma ou tamanho. Configuracao da pessoa nunca e despejada
    // por falta de espaco; se so houver linhas configuradas, a nova fica de
    // fora, mas AGORA COM LOG, que e o que faltava para isto ser diagnosticavel.
    //
    // SAI A ULTIMA ELEGIVEL, e nao a primeira: a posicao na tabela carrega a
    // ordem da home, e tirar do fim e o que menos mexe no que ja esta em cima.
    //
    // CORRIGIDO em 20/09/2026, na C9 do dono: o arquivo tinha 320 linhas, 283
    // delas do Xperience (que declara 605 catalogos) com FORMA escolhida
    // (tipo 2/3/4) e o resto oculto — NADA dispensavel pela regra acima. Duas
    // consequencias: (1) canais, AICat e colecoes que a home desenhava ficaram
    // fora da folha ("varias fileiras nao aparecem no reorder"); (2) PIOR:
    // antes de lotar, a regra despejou "Continuar assistindo" e "Amigos
    // assistindo" — sao AUTO/padrao e no arranque ainda nao estao naHome nem
    // vista — e elas voltaram a entrar NO FIM da lista ("desceram"). Portanto:
    //   a. fileira do APP (continue_watching, social_activity) NUNCA sai;
    //   b. segunda passada: sem dispensavel puro, sai a ultima que nao esta
    //      na home, nao foi vista e nao esta oculta MESMO com forma/tamanho
    //      escolhidos — a forma de uma fileira que nem aparece vale menos do
    //      que a pessoa poder mexer na que aparece. Oculta continua protegida:
    //      despeja-la faria a fileira voltar.
    //   c. as primeiras `limite` LIGADAS tambem nao saem: sao a home por
    //      definicao (fil_unir emite nesta ordem e a home corta em `limite`),
    //      mesmo que naHome ainda nao tenha sido marcado neste arranque —
    //      e no arranque que a descoberta registra centenas de chaves.
    //   d. terceira passada, ultimo recurso: `vista` deixa de proteger. Na C9
    //      TODAS as 283 do Xperience sao declaradas em todo arranque (vista=1)
    //      e nenhuma esta na home — sem esta passada a fileira que a pessoa
    //      VE continuava fora da folha. Sai a ultima que nao esta na home,
    //      nao e do app, nao e do topo e nao esta oculta.
    if (nLinhas >= FIL_MAX) {
      int v = -1, k, passo, ligadas = 0, topo[FIL_MAX];
      for (k = 0; k < nLinhas; k++) {
        topo[k] = !linhas[k].oculta && ligadas < limite;
        if (!linhas[k].oculta) ligadas++;
      }
      for (passo = 0; passo < 3 && v < 0; passo++)
        for (k = nLinhas - 1; k >= 0 && v < 0; k--)
          if (!linhas[k].naHome && (passo == 2 || !linhas[k].vista) &&
              !linhas[k].oculta && !topo[k] &&
              fil_origem_de(linhas[k].chave) != FIL_ORIGEM_APP &&
              (passo >= 1 || (linhas[k].tipo == FIL_TIPO_AUTO &&
                              linhas[k].tam == FIL_TAM_PADRAO)))
            v = k;
      if (v < 0) {
        pthread_mutex_unlock(&trava);
        printf("[fileiras] tabela cheia (%d) e nada dispensavel: \"%s\" ficou "
               "fora da lista de fileiras\n", FIL_MAX, chave);
        fflush(stdout);
        return;
      }
      printf("[fileiras] tabela cheia (%d): \"%s\" saiu para \"%s\" entrar\n",
             FIL_MAX, linhas[v].titulo[0] ? linhas[v].titulo : linhas[v].chave,
             chave);
      fflush(stdout);
      if (v < nLinhas - 1)
        memmove(&linhas[v], &linhas[v + 1],
                sizeof(Linha) * (size_t)(nLinhas - 1 - v));
      nLinhas--;
    }
    i = nLinhas++;
    memset(&linhas[i], 0, sizeof linhas[i]);
    snprintf(linhas[i].chave, FIL_CHAVE, "%s", chave);
    linhas[i].tam = FIL_TAM_PADRAO;
    linhas[i].itens = -1;
    grava = 1;
  }
  linhas[i].vista = 1;
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
int fil_linha_na_home(int i) {
  return (i >= 0 && i < nLinhas) ? linhas[i].naHome : 0;
}
int fil_linha_vista(int i) {
  return (i >= 0 && i < nLinhas) ? linhas[i].vista : 0;
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
// A LISTA SEGUE A HOME ATE A PRIMEIRA REORDENACAO — e quem a home nao tem
// fica marcado, nao apagado nem reordenado.
//
// Duas ausencias tem destinos diferentes de proposito:
//
// - AUSENTE FICA ONDE ESTA. Uma linha fora da home pode estar so AINDA NAO
//   MONTADA — no arranque a home publica em ~16 etapas e as colecoes chegam
//   por ultimo. Mover as ausentes para o fim (a tentacao obvia) regravaria a
//   posicao que a pessoa deu a elas, e a cada boot uma colecao tardia
//   reapareceria no fim. Elas ficam paradas e a folha as marca "Fora da
//   Home"; o poço do #30 nao volta porque a visibilidade nao depende da
//   posicao — fil_unir emite na ordem da propria lista, ausente ou nao.
//
// - CHAVE VIVA SEM REGISTRO ENTRA NO FIM. fil_registrar descarta quando a
//   tabela lota (FIL_MAX=192 e o arquivo do dono esta cheio de chaves mortas
//   que o AICat gera a cada ciclo — "because_watched_v11201192"). Era assim
//   que a colecao do FrostView aparecia na home e nunca na folha. O resgate
//   toma a vaga de uma linha morta SEM nenhuma escolha da pessoa (ligada,
//   forma e tamanho padrao) — perder esse registro nao perde nada.
void fil_espelhar_ordem(const char *const *chaves,
                        const char *const *titulos, int n) {
  static Linha novo[FIL_MAX];
  char usado[FIL_MAX];
  int i, k = 0;
  if (!chaves || n < 1) return;
  pthread_mutex_lock(&trava);
  garantir();
  memset(usado, 0, sizeof usado);
  for (i = 0; i < n; i++) {
    // "last_session" nao entra nem no mapa: e contexto do player, nao fileira
    // — home.c nao a registra e ela nao pode virar linha da folha.
    int p = (chaves[i] && strcmp(chaves[i], "last_session"))
            ? achar(chaves[i]) : -1;
    if (p >= 0) usado[p] = 1;
  }
  // O RESGATE. O titulo vem de quem chama — a home o tem na mao; sem ele a
  // linha mostraria a chave crua ate o proximo registro. A vaga sai de linha
  // morta sem escolha, compactada para fora; sem morta e sem vaga, fica para
  // a proxima montagem.
  for (i = 0; i < n; i++) {
    int v;
    if (!chaves[i] || !strcmp(chaves[i], "last_session") ||
        achar(chaves[i]) >= 0) continue;
    if (nLinhas < FIL_MAX) {
      v = nLinhas++;
    } else {
      // Morta de verdade e NUNCA vista nesta sessao: uma colecao que so monta
      // tarde tem vista=1 e naHome=0 nesta volta — sem o !vista o resgate
      // roubava o registro dela na primeira publicacao parcial da home.
      for (v = 0; v < nLinhas; v++)
        if (!usado[v] && !linhas[v].vista && !linhas[v].oculta &&
            linhas[v].tipo == FIL_TIPO_AUTO && linhas[v].tam == FIL_TAM_PADRAO)
          break;
      if (v >= nLinhas) continue;
      memmove(linhas + v, linhas + v + 1,
              sizeof(Linha) * (size_t)(nLinhas - v - 1));
      nLinhas--;
      memset(usado, 0, sizeof usado);
      for (k = 0; k < n; k++) {
        int p = (chaves[k] && strcmp(chaves[k], "last_session"))
                ? achar(chaves[k]) : -1;
        if (p >= 0) usado[p] = 1;
      }
      v = nLinhas++;
    }
    memset(&linhas[v], 0, sizeof linhas[v]);
    snprintf(linhas[v].chave, FIL_CHAVE, "%s", chaves[i]);
    if (titulos && titulos[i])
      snprintf(linhas[v].titulo, FIL_TITULO, "%s", titulos[i]);
    linhas[v].tam = FIL_TAM_PADRAO;
    linhas[v].itens = -1;
    linhas[v].vista = 1;
    usado[v] = 1;
    registroSujo = 1;
  }
  for (i = 0; i < nLinhas; i++) linhas[i].naHome = usado[i] ? 1 : 0;
  if (ordemLocal) {
    // A ordem e da pessoa: NAO REORDENA. O resgate acima ja poe a chave nova
    // no fim, que e onde fil_unir a colocaria na home.
    pthread_mutex_unlock(&trava);
    return;
  }
  if (nLinhas > 0) {
    // SO REORDENA O QUE A TELA CONHECE, dentro das posicoes que elas ja
    // ocupam — quem ela nao mencionou fica exatamente onde estava. E o que
    // impede a home pela metade de rebaixar a colecao que ainda nao chegou.
    int slots[FIL_MAX], m = 0;
    char posto[FIL_MAX];
    k = 0;
    for (i = 0; i < nLinhas && m < FIL_MAX; i++)
      if (usado[i]) slots[m++] = i;
    memcpy(novo, linhas, sizeof(Linha) * (size_t)nLinhas);
    memset(posto, 0, sizeof posto);
    for (i = 0; i < n && k < m; i++) {
      int p = (chaves[i] && strcmp(chaves[i], "last_session"))
              ? achar(chaves[i]) : -1;
      if (p >= 0 && !posto[p]) { posto[p] = 1; novo[slots[k++]] = linhas[p]; }
    }
    memcpy(linhas, novo, sizeof(Linha) * (size_t)nLinhas);
  }
  pthread_mutex_unlock(&trava);
}

int fil_mover(int i, int direcao) {
  int j, ret = i;
  pthread_mutex_lock(&trava);
  if (i >= 0 && i < nLinhas) {
    int dir = direcao > 0 ? 1 : -1;
    // LINHA OCULTA E TRANSPARENTE: mover pula por cima dela e troca com a
    // proxima LIGADA. A regra anterior pulava quem NAO ESTAVA NA HOME
    // (naHome), e isso quebrou a folha em duas abas: a aba "Na Home" lista as
    // ligadas, e uma ligada recem-adicionada (ou na fila) ainda tem naHome=0
    // ate a home remontar — o movimento saltava por cima dela, ou nao
    // encontrava vizinho nenhum e travava ("clico na fileira e ela trava e
    // nao mexe, as vezes move so uma"). Na aba, o vizinho visivel e a proxima
    // ligada; e o que a home tambem entende, porque ela e as primeiras N
    // ligadas na ordem.
    j = i + dir;
    while (j >= 0 && j < nLinhas && linhas[j].oculta) j += dir;
    if (j >= 0 && j < nLinhas) {
      Linha t = linhas[i]; linhas[i] = linhas[j]; linhas[j] = t;
      // A partir do primeiro movimento a ordem local EXISTE e passa a vencer
      // a da conta. Antes disso o arquivo so guarda liga/desliga e forma, e a
      // ordem continua sendo a do app web — que e o certo para quem nunca
      // mexeu aqui.
      ordemLocal = 1;
      gravar();
      ret = j;
    }
  }
  pthread_mutex_unlock(&trava);
  return ret;
}

// O GRUPO de uma linha: o nome do addon que a declarou, ou "" para as fixas do
// app. Duas linhas com o mesmo addon formam um bloco, e o bloco inteiro se move
// junto. Ver o comentario de fil_registrar sobre o campo addon.
// O "grupo" de uma linha para fins de mover-em-bloco: nao e so o nome do
// addon. Linhas do app tem addon vazio e COLECOES tambem — pelo nome cru as
// tres coisas virariam um bloco so, e mover uma colecao arrastaria
// "Continuar assistindo" junto. Por isso o grupo segue a ORIGEM da chave:
// todas as do app sao um bloco, todas as colecoes sao outro, e cada addon e
// um bloco proprio — as mesmas secoes que a folha de Ajustes desenha.
static const char *grupoDe(int i) {
  static char app[] = "\x01" "app", col[] = "\x01" "col", vazio[] = "";
  int o;
  if (i < 0 || i >= nLinhas) return vazio;
  o = fil_origem_de(linhas[i].chave);
  if (o == FIL_ORIGEM_APP)      return app;
  if (o == FIL_ORIGEM_COLECAO)  return col;
  return linhas[i].addon;
}

// Extensao do bloco que contem a linha `i`: anda para tras e para a frente
// enquanto o addon for o mesmo. Devolve [ini, fim] inclusive.
static void blocoDe(int i, int *ini, int *fim) {
  const char *addon = grupoDe(i);
  int a = i, b = i;
  for (; a > 0 && !strcmp(grupoDe(a - 1), addon); a--) {}
  for (; b < nLinhas - 1 && !strcmp(grupoDe(b + 1), addon); b++) {}
  *ini = a; *fim = b;
}

// Move o BLOCO inteiro de um addon para cima ou para baixo, trocando com o
// bloco vizinho inteiro (que pode ter varias linhas). Devolve o novo indice
// da linha `i` dentro do bloco, ou `i` se nao deu para mover.
//
// BLOCO FANTASMA NAO VALE TROCA. Um bloco todo fora da home (addon que saiu,
// catalogo ainda nao montado) trocado com um bloco vivo mudava a folha sem
// mudar a home — o mesmo defeito de fil_mover, uma camada acima. Um bloco
// vivo pula quantos blocos fantasmas estiverem no caminho ate o proximo vivo;
// um bloco fantasma troca com o vizinho direto, vivo ou nao — a ordem dele
// so vale entre os fantasmas.
int fil_mover_grupo(int i, int direcao) {
  int ret = i;
  pthread_mutex_lock(&trava);
  garantir();
  if (i >= 0 && i < nLinhas) {
    int ini, fim, vIni, vFim, b, q, meuVivo = 0;
    blocoDe(i, &ini, &fim);
    // Mesma regra de fil_mover: bloco "vivo" e bloco com alguma linha LIGADA.
    for (q = ini; q <= fim; q++) if (!linhas[q].oculta) { meuVivo = 1; break; }
    if (direcao > 0) {
      b = fim + 1;
      if (b >= nLinhas) goto sair;
      blocoDe(b, &vIni, &vFim);
      while (meuVivo) {
        int vivo = 0;
        for (q = vIni; q <= vFim; q++) if (!linhas[q].oculta) { vivo = 1; break; }
        if (vivo) break;
        b = vFim + 1;
        if (b >= nLinhas) goto sair;
        blocoDe(b, &vIni, &vFim);
      }
      // Rotaciona [eu][o que passou por cima][vizinho] -> [meio][vizinho][eu].
      { int tamMeu = fim - ini + 1, tamMeio = vFim - fim;
        static Linha tmp[FIL_MAX];
        memcpy(tmp, linhas + ini, sizeof(Linha) * (size_t)tamMeu);
        memmove(linhas + ini, linhas + fim + 1, sizeof(Linha) * (size_t)tamMeio);
        memcpy(linhas + ini + tamMeio, tmp, sizeof(Linha) * (size_t)tamMeu);
        ordemLocal = 1; gravar();
        ret = ini + tamMeio + (i - ini); }
    } else {
      b = ini - 1;
      if (b < 0) goto sair;
      blocoDe(b, &vIni, &vFim);
      while (meuVivo) {
        int vivo = 0;
        for (q = vIni; q <= vFim; q++) if (!linhas[q].oculta) { vivo = 1; break; }
        if (vivo) break;
        b = vIni - 1;
        if (b < 0) goto sair;
        blocoDe(b, &vIni, &vFim);
      }
      // Rotaciona [vizinho][o que passei por baixo][eu] -> [eu][vizinho][meio].
      { int tamMeu = fim - ini + 1, tamMeio = ini - vIni;
        static Linha tmp[FIL_MAX];
        memcpy(tmp, linhas + ini, sizeof(Linha) * (size_t)tamMeu);
        memmove(linhas + vIni + tamMeu, linhas + vIni,
                sizeof(Linha) * (size_t)tamMeio);
        memcpy(linhas + vIni, tmp, sizeof(Linha) * (size_t)tamMeu);
        ordemLocal = 1; gravar();
        ret = vIni + (i - ini); }
    }
  }
sair:
  pthread_mutex_unlock(&trava);
  return ret;
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

#ifdef FIL_TESTE
// Costura de teste: esquecer() NAO pode reler o arquivo (apagou o de quem
// saiu), mas o teste da tabela cheia precisa de linhas "mortas" de verdade —
// carregadas do disco, com vista=0, que e o que o resgate procura.
void fil_teste_recarregar(void) { carregado = 0; }
// Zera a marca de "vista nesta sessao" de UMA linha. O despejo da tabela cheia
// so pode tirar quem nao foi vista, e num teste todas acabam de ser
// registradas — sem esta costura nao da para exercitar o caminho que o defeito
// da C9 percorreu (linhas antigas, lidas do disco, com vista=0).
void fil_teste_esquecer_vista(int i) {
  if (i >= 0 && i < nLinhas) linhas[i].vista = 0;
}
#endif

// ORDENA A LISTA POR ADDON: primeiro as fixas do app, depois colecoes, depois
// os catalogos agrupados pelo nome do addon. Dentro de cada addon a ordem
// relativa se preserva — a funcao e um sort estavel.
//
// A pessoa chama isto quando a lista esta misturada e ela quer ver "o que e do
// Xperience junto". Como muda a ordem, `ordemLocal` vira 1 — a ordenacao e a
// escolha dela, nao um efeito colateral.
void fil_ordenar_por_addon(void) {
  static Linha novo[FIL_MAX];
  int i, k = 0;
  pthread_mutex_lock(&trava);
  garantir();
  // Passo 1: app e colecao primeiro, na ordem em que ja estao.
  for (i = 0; i < nLinhas; i++)
    if (fil_origem_de(linhas[i].chave) != FIL_ORIGEM_CATALOGO)
      novo[k++] = linhas[i];
  // Passo 2: catalogos agrupados por addon, na ordem de primeira aparicao.
  { char vistos[FIL_MAX][48]; int nV = 0;
    for (i = 0; i < nLinhas; i++) {
      const char *a = linhas[i].addon;
      int j;
      if (fil_origem_de(linhas[i].chave) != FIL_ORIGEM_CATALOGO) continue;
      for (j = 0; j < nV; j++) if (!strcmp(vistos[j], a)) break;
      if (j < nV) continue;   // addon ja teve a vez
      if (nV < FIL_MAX) snprintf(vistos[nV], 48, "%s", a);
      nV++;
      for (j = i; j < nLinhas; j++)
        if (fil_origem_de(linhas[j].chave) == FIL_ORIGEM_CATALOGO &&
            !strcmp(linhas[j].addon, a))
          novo[k++] = linhas[j];
    }
  }
  memcpy(linhas, novo, sizeof(Linha) * (size_t)k);
  nLinhas = k;
  ordemLocal = 1;
  gravar();
  pthread_mutex_unlock(&trava);
}
