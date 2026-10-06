// O DIARIO — o que cada perfil viu, e quando. Fundacao do social & pessoal
// (docs/plans/social-pessoal/social-pessoal-plano.md, secao 4).
//
// UMA ENTRADA = UMA VEZ VISTA. Filme visto duas vezes = duas entradas (a
// segunda com DIARIO_F_REVER). Cada episodio e uma entrada propria. Entradas
// de nivel de titulo (temporada=episodio=0) existem para filme e para "serie
// inteira marcada" (conta, Trakt).
//
// FONTES (o diario se monta sozinho, nada pede cadastro):
//   'p' player   — fim de reproducao (90% ou creditos), com seg, hora e cor
//   'c' conta    — sync_pull_watched_items (contalib), no perfil ativo
//   't' Trakt    — watched/* + history do ano corrente
//   'l' Letterboxd — importacao feita pela propria pessoa (celular)
//   'm' manual   — a pessoa marcou/avaliou na TV
// FUSAO: a mesma (id, temporada, episodio) com `quando` a menos de 6 h e a
// MESMA vez; fica a fonte de mais confianca (p > t > c > l > m) e os campos
// que ela nao tem vem da outra (nota, cor, seg...). Fora da janela e outra vez.
//
// ARQUIVO: diario-p<N>.txt (dados_gravar, atomico), uma linha por entrada.
// Teto de DIARIO_MAX entradas por perfil: passou, saem as mais antigas.
// Na memoria, so os perfis consultados (cada um carregado sob demanda).
//
// FIOS: toda funcao publica e segura de qualquer fio (mutex interno). Os
// OUVINTES, porem, so sao chamados no laco principal, dentro de diario_passo().
// diario_geracao() e o jeito barato de um fio saber que algo mudou.
//
// PRIVACIDADE: local, por perfil. O cofre na nuvem (diario_nuvem_*) so liga com
// o "sim" da pessoa, perguntado uma vez na primeira abertura do Diario.
//
// ESTABILIDADE: esta API e consumida pela Trava (feat/sp-trava) e pelas
// Conquistas (feat/sp-conquistas). Mudancas vao anotadas em
// docs/plans/social-pessoal/API-diario.md.
#ifndef NV_DIARIO_H
#define NV_DIARIO_H
#include <stddef.h>

#define DIARIO_MAX        20000   // entradas por perfil
#define DIARIO_JANELA_S   (6 * 3600)
#define DIARIO_ID_MAX     32      // "tt0111161", "tmdb:123", "kitsu:..." (sem ":T:E")

// Origem (DiarioEntrada.origem). Ordem de confianca na fusao: p > t > c > l > m.
#define DIARIO_ORIG_PLAYER     'p'
#define DIARIO_ORIG_TRAKT      't'
#define DIARIO_ORIG_CONTA      'c'
#define DIARIO_ORIG_LETTERBOXD 'l'
#define DIARIO_ORIG_MANUAL     'm'

// Flags. Os 5 primeiros bits sao os mesmos da tabela `diario` do servidor
// (migracao 011); os de cima sao so locais e tambem sobem (o servidor guarda
// o inteiro sem interpretar).
#define DIARIO_F_REVER      0x0001  // nao e a primeira vez deste (id,T,E)
#define DIARIO_F_DORMIU     0x0002  // pegou no sono (CONFIRMADO pela pessoa)
#define DIARIO_F_CORACAO    0x0004  // "gostei" a moda do Letterboxd (independe da nota)
#define DIARIO_F_JUNTOS     0x0008  // visto com outros perfis/amigos (ver comPerfis)
#define DIARIO_F_IMPORTADO  0x0010  // veio de importacao (Letterboxd)
#define DIARIO_F_DATA_APROX 0x0020  // sem hora confiavel (conta/Trakt sem data): "data aproximada"
#define DIARIO_F_DORMIU_SUS 0x0040  // SUSPEITA de sono (dormiu_regra), ainda nao perguntada/confirmada
#define DIARIO_F_REVIEW     0x0080  // ha review (texto no servidor ou link do Letterboxd)
#define DIARIO_F_PARCIAL    0x0100  // o player parou antes do fim (so com sono: nao conta como visto)

// Tipo (DiarioEntrada.tipo)
#define DIARIO_FILME 'm'
#define DIARIO_SERIE 's'

// Tags rapidas (lista fechada do plano 6.2), bits de DiarioEntrada.tags.
enum {
  DIARIO_TAG_VISUAL = 0, DIARIO_TAG_TRILHA, DIARIO_TAG_ATUACOES,
  DIARIO_TAG_ROTEIRO, DIARIO_TAG_FINAL, DIARIO_TAG_RITMO_LENTO,
  DIARIO_TAG_CHOREI, DIARIO_TAG_RI_MUITO, DIARIO_TAG_ASSUSTADOR,
  DIARIO_TAG_QUERO_REVER, DIARIO_TAG_SUPERESTIMADO, DIARIO_TAG_SUBESTIMADO,
  DIARIO_TAG_N
};

typedef struct {
  char id[DIARIO_ID_MAX];   // id do TITULO, sem ":T:E" ("tt123")
  char tipo;                // DIARIO_FILME | DIARIO_SERIE
  char origem;              // DIARIO_ORIG_*
  short temporada, episodio;// 0,0 = nivel de titulo
  long long quando;         // epoch (s) do FIM da vez (ou watched_at)
  int  seg;                 // segundos TOCANDO de verdade; 0 = nao se sabe
  short hora;               // dia_semana*24 + hora local do INICIO (0..167); -1 = nao se sabe
  unsigned char nota10;     // 0 = sem nota; 1..10 (5 estrelas com meia: 1 = meia estrela)
  unsigned char comPerfis;  // bits dos perfis da casa juntos (bit i = perfil i+1)
  unsigned short flags;     // DIARIO_F_*
  unsigned short tags;      // bits DIARIO_TAG_*
  int  tmdb;                // 0 = nao se sabe
  unsigned int cor;         // 0xRRGGBB da arte (corviva); 0 = nenhuma
  int  dormiuSeg;           // posicao provavel do sono (s), com DORMIU/DORMIU_SUS; senao 0
  int  durSeg;              // duracao do video (s), quando o player sabe; 0 = nao se sabe
} DiarioEntrada;

// Agregado por TITULO (o indice que fica em memoria).
typedef struct {
  char id[DIARIO_ID_MAX];
  char tipo;
  int  vezes;               // filme: quantas vezes; serie: quantas entradas (episodios)
  int  episodios;           // serie: episodios DISTINTOS vistos
  long long primeiro, ultimo;
  unsigned char nota10;     // nota mais recente (0 = nenhuma)
  unsigned short flags;     // OR das flags das entradas
  int  seg;                 // soma dos seg
  int  tmdb;
} DiarioTitulo;

// Filtro para diario_percorrer. Zerado = tudo.
typedef struct {
  const char *id;           // so este titulo (NULL = todos)
  char tipo;                // 0 | DIARIO_FILME | DIARIO_SERIE
  long long de, ate;        // [de, ate) em epoch; 0 = sem limite
  unsigned short comFlags;  // so entradas com TODAS estas flags
  unsigned short semFlags;  // e com NENHUMA destas
  char origem;              // 0 = qualquer
  int  decrescente;         // 1 = mais novas primeiro
} DiarioFiltro;

// Eventos para os ouvintes.
#define DIARIO_EV_NOVA       1   // entrada nova (e != NULL)
#define DIARIO_EV_ALTERADA   2   // nota/flags/tags/data mudaram, ou fusao enriqueceu (e != NULL)
#define DIARIO_EV_APAGADA    3   // e != NULL (a copia de antes)
#define DIARIO_EV_RECARREGADO 4  // muitas de uma vez (importacao, restauracao, carga): e == NULL

// perfil: o indice do perfil (perfis_ativo(): 1..CONTA_PERFIL_MAX). 0 = o ativo.

// --- ciclo de vida ---------------------------------------------------------
void diario_iniciar(void);          // depois de dados_iniciar
// Laco principal, uma vez por quadro: entrega eventos aos ouvintes, grava o
// que mudou (com folga de 2 s), importa os vistos da conta quando mudaram e
// anda a fila do cofre. Custo de quadro ocioso: duas leituras atomicas.
void diario_passo(void);
void diario_esquecer(void);         // logout: solta a memoria (NAO apaga arquivos)
// A CONTA dona dos arquivos (sessao_usuario()). Gravado como hash no cabecalho
// do diario-p<N>.txt; arquivo de OUTRA conta e ignorado (e sobrescrito na
// proxima gravacao) — os perfis sao por indice, e o perfil 1 de quem entrou
// nao pode ler o diario do perfil 1 de quem saiu. "" = sem conta.
void diario_dono(const char *conta);
void diario_gravar_agora(void);     // saida do app

// --- escrita ---------------------------------------------------------------
// Acrescenta (ou funde). Devolve 1 nova, 0 fundida numa existente, -1 erro.
// `e->id` pode vir com ":T:E" — e truncado, e T/E vem do proprio id se os
// campos estiverem zerados. REVER e calculado aqui (nao precisa vir).
int  diario_adicionar(int perfil, const DiarioEntrada *e);
// Muitas de uma vez (importacao): um evento RECARREGADO no fim. Devolve novas.
int  diario_adicionar_lote(int perfil, const DiarioEntrada *v, int n);

// Uma entrada e identificada por (id, temporada, episodio, quando).
// `quando` = 0 escolhe a MAIS RECENTE desse (id,T,E).
int  diario_definir_nota(int perfil, const char *id, int t, int e, long long quando, int nota10);
int  diario_definir_flags(int perfil, const char *id, int t, int e, long long quando,
                          unsigned ligar, unsigned desligar);
int  diario_definir_tags(int perfil, const char *id, int t, int e, long long quando, unsigned tags);
int  diario_definir_data(int perfil, const char *id, int t, int e, long long quando, long long novo);
// Apaga a entrada do DIARIO (nunca o "visto" do catalogo/conta/Trakt).
int  diario_apagar(int perfil, const char *id, int t, int e, long long quando);

// Nota de TITULO (pagina do titulo, reacao dos creditos): grava na entrada de
// nivel de titulo mais recente; sem nenhuma, cria uma 'm' agora com DATA_APROX.
// Devolve 1 se gravou.
int  diario_avaliar(int perfil, const char *id, char tipo, int nota10);
// Reacao dos creditos (REACAO_GOSTEI=1 / MAIS_MENOS=0 / NAO=-1) -> 8 / 5 / 2.
int  diario_nota_da_reacao(int reacao);

// --- leitura ---------------------------------------------------------------
int  diario_n(int perfil);                     // entradas
int  diario_n_titulos(int perfil);
// Entrada i em ordem de `quando` crescente. 1 se existe.
int  diario_entrada(int perfil, int i, DiarioEntrada *saida);
// Agregado do titulo. 1 = visto pelo menos uma vez.
int  diario_titulo(int perfil, const char *id, DiarioTitulo *saida);
int  diario_titulo_i(int perfil, int i, DiarioTitulo *saida);  // i em [0, n_titulos)
// Episodio visto? Devolve vezes (0 = nao); `quando` recebe a mais recente.
int  diario_episodio(int perfil, const char *id, int t, int e, long long *quando);
// O episodio mais adiantado (maior T, depois E) visto deste titulo. 1 se houver.
int  diario_ultimo_episodio(int perfil, const char *id, int *t, int *e, long long *quando);
// Todas as entradas de um titulo (episodios incluidos), crescente. Devolve
// quantas existem (pode ser > max; so `max` sao copiadas). `saida` NULL conta.
int  diario_por_titulo(int perfil, const char *id, DiarioEntrada *saida, int max);
// Janela de datas [de, ate). Mesmo contrato de contagem.
int  diario_por_periodo(int perfil, long long de, long long ate, DiarioEntrada *saida, int max);
// "Por pessoa": a filmografia vem do meta cache (Conquistas). Entra a lista
// de ids; `saida[i]` (opcional) recebe o agregado (vezes = 0 se nao viu).
// Devolve quantos da lista foram vistos.
int  diario_por_lista(int perfil, const char *const *ids, int n, DiarioTitulo *saida);
// Generico. `cb` devolve 0 para parar. Chamado COM o mutex do diario: nao
// chame o diario de dentro do cb. Devolve quantas entradas passaram no filtro.
typedef int (*DiarioVisita)(const DiarioEntrada *e, void *ctx);
int  diario_percorrer(int perfil, const DiarioFiltro *f, DiarioVisita cb, void *ctx);

// --- mudancas --------------------------------------------------------------
// Sobe a cada mudanca daquele perfil (qualquer fio). 0 = perfil nunca carregado.
unsigned diario_geracao(int perfil);
typedef void (*DiarioOuvinte)(int perfil, int evento, const DiarioEntrada *e, void *ctx);
// Ate 8 ouvintes. Chamados so no laco principal (diario_passo). 1 se registrou.
int  diario_ouvir(DiarioOuvinte fn, void *ctx);
void diario_parar_de_ouvir(DiarioOuvinte fn, void *ctx);

// --- cofre na nuvem (opcional, privado) -----------------------------------
#define DIARIO_NUVEM_NAO_PERGUNTADO 0
#define DIARIO_NUVEM_SIM            1
#define DIARIO_NUVEM_NAO            2
int  diario_nuvem_estado(int perfil);
void diario_nuvem_definir(int perfil, int estado);
int  diario_nuvem_pendentes(int perfil);       // entradas esperando envio
// 1 se o pacote tem servidor (NUVIO_REC_URL) e o perfil disse sim.
int  diario_nuvem_ativa(int perfil);

// --- nomes (cache COMPARTILHADO entre perfis: id -> titulo, ano, poster) ---
// O diario guarda ids; as telas (Perfil e Stats, CSV do Letterboxd, Caca)
// precisam do nome e da capa. Alimentado por quem ja tem o CatItem (player,
// pagina do titulo, biblioteca da conta). Teto DIARIO_NOMES_MAX, sai o mais
// antigo. Arquivo diario-nomes.txt. Seguro de qualquer fio.
#define DIARIO_NOMES_MAX 4000
void diario_nome_definir(const char *id, const char *titulo, int ano, const char *poster);
// 1 se conhece. Qualquer saida pode ser NULL.
int  diario_nome(const char *id, char *titulo, size_t tamTitulo, int *ano,
                 char *poster, size_t tamPoster);

// --- puro, para teste e para outras telas -----------------------------------
// Mesma vez? (mesmo id/T/E e |quando| < 6 h)
int  diario_regra_mesma_vez(const DiarioEntrada *a, const DiarioEntrada *b);
// Confianca da origem (maior = mais confiavel).
int  diario_regra_confianca(char origem);
// Linha de texto <-> entrada (formato do arquivo). Devolve tamanho / 1.
// No arquivo, flags pode trazer o bit 0x8000 (interno: envio ao cofre pendente).
size_t diario_linha(const DiarioEntrada *e, char *dst, size_t tam);
int  diario_ler_linha(const char *linha, DiarioEntrada *e);
// "tt123:2:5" -> id "tt123", t 2, e 5.
void diario_separar_id(const char *src, char *id, size_t tam, int *t, int *e);
// Linha do CSV de importacao do Letterboxd (imdbID, tmdbID, WatchedDate,
// Rating10, Rewatch, Tags). Cabecalho incluso quando `cabecalho`.
size_t diario_csv_letterboxd(const DiarioEntrada *e, const char *titulo, int ano,
                             int cabecalho, char *dst, size_t tam);

#endif
