// SOCIALVIS — o MODELO DA TELA do social: o que a fileira "Amigos assistindo"
// (amigosfil.c), o painel da tecla azul (abas Atividade e Amigos, em
// salvospainel.c) e o perfil do amigo (amigoperfil.c) desenham. Nenhuma dessas
// telas le recomenda.h, trakt.c ou o catalogo para saber de gente: elas leem
// SO daqui. Assim a fonte dos dados pode mudar (e vai: ver abaixo) sem mexer
// em desenho nenhum.
//
// DE ONDE VEM HOJE (02/10/2026), em socialvis.c, svDoQueExiste():
//   - a fileira "social_activity" do catalogo (Trakt + amigos do Nuvio,
//     montada em descoberta.c: trakt_social + recomenda_social_mesclar). Cada
//     CatItem vira um SvEvento: socialSlug -> pessoaId ("trakt:<slug>" ou o
//     "nuvio:..." que ja vem inteiro), socialNome, socialAvatar, socialAcao
//     ("assistindo agora" -> SV_AGORA, "assistiu" -> SV_FIM, "avaliou" ->
//     SV_AVALIOU; o resto e SV_ATIVIDADE e NAO entra no feed), backdrop/poster,
//     temporada/episodio. SEM "quando" (o trakt.c nao guarda a hora): esses
//     caem no grupo "Recentes" do feed.
//   - recomenda_contatos(): amigos SEM atividade ainda viram rosto (anel cinza).
//   - recomenda_amigo_atividades(): o feed do nosso servidor por amigo do Nuvio
//     (agora/criado), somado sem repetir titulo.
//   - recomenda_item(): o que me mandaram vira evento SV_MANDOU ("te mandou").
//
// O QUE ESPERO DO agente/socialsrv (recomenda.h), PARA LIGAR NUMA FUNCAO SO:
//
//   typedef struct {
//     char fonte[12];        // "nuvio" | "trakt" | "simkl" | "letterboxd"
//     char pessoaId[96];     // estavel por pessoa; o MESMO id de RecContato.id
//     char pessoaNome[64];   // nome do PERFIL ja resolvido (nunca "nuvio:5269…")
//     char pessoaAvatar[256];
//     char acao[12];         // "agora" | "inicio" | "fim" | "reacao" | "salvo"
//                            // | "abandono"
//     char titulo[160], imdb[24], tipo[8];   // tipo "movie" | "series"
//     char poster[512];      // 2:3
//     char arte[512];        // 16:9 (backdrop); vazio = cai no poster  [NOVO]
//     int  temporada, episodio;              // 0 = filme
//     int  pct;              // 0..100, -1 = nao se sabe
//     int  restanteMin;      // -1 = nao se sabe                         [NOVO]
//     long long quando;      // epoch em SEGUNDOS
//     int  reacao;           // 1 gostei, 0 mais ou menos, -1 nao gostei,
//                            // -2 sem reacao
//     int  sobreMinhaRec;    // 1 = o titulo foi recomendado POR MIM a essa
//                            // pessoa ("Pedro viu Dune 2, que voce mandou") [NOVO]
//   } RecEvento;
//   int recomenda_feed(RecEvento *saida, int max);   // mais novo primeiro,
//                                                    // ja sem os eventos de
//                                                    // quem desligou a atividade
//
//   typedef struct {
//     long long desde;            // epoch s em que viraram amigos; 0 = ?
//     char porOnde[12];           // "codigo" | "trakt" | "simkl" | "sugestao"
//     int  gostoTotal;                          // number of paired shared reactions
//   int  gostoPct, emComum;   // -1 = ainda nao ha dado
//     int  minutosMes, filmesMes, seriesCurso;   // do mes corrente; -1 = ?
//     int  recsVistas, recsTotal; // das MINHAS recomendacoes para ele
//     int  nEnviadas;
//     struct { char imdb[24], titulo[160], poster[512];
//              int estado;        // 0 entregue, 1 abriu, 2 viu
//              int reacao;        // como em RecEvento
//              long long quando; } enviadas[8];   // mais nova primeiro
//   } RecAmigoPerfil;
//   int recomenda_amigo_perfil(const char *pessoaId, RecAmigoPerfil *saida);
//
// A LIGACAO: em socialvis.c, svDoServidor() e svPerfilDoServidor() devolvem -1
// hoje ("o servidor ainda nao da isso") e o modelo cai em svDoQueExiste(). Com
// o socialsrv pronto, as duas copiam RecEvento -> SvEvento campo a campo (os
// nomes sao os mesmos de proposito; ver svDeRecEvento no comentario de la) e
// nada mais muda. Os rotulos de fonte ("Trakt", "Simkl") so aparecem no perfil.
//
// CUSTO: socialvis_atualizar() e chamada por quadro mas so refaz o modelo
// quando a fonte muda (cat_revisao, contagens do recomenda) e no maximo a cada
// SV_REFAZ_MS. O desenho le ponteiros const; nada aqui aloca por quadro.
#ifndef NV_SOCIALVIS_H
#define NV_SOCIALVIS_H

// A PONTE COM O SERVIDOR NOVO (feed unido, perfil do amigo, alcance) ESTA
// LIGADA desde o merge de agente/socialsrv. Um teste que nao linka recomenda.c
// pode desligar com -DNV_SOCIAL_V1.
#if !defined(NV_SOCIAL_V1) && !defined(NV_SOCIAL_V2)
#define NV_SOCIAL_V2 1
#endif
#include <stddef.h>

#define SV_AMIGOS_MAX   16
#define SV_TIT_MAX       3    // cartoes que abrem ao lado do rosto
#define SV_EVENTOS_MAX  64
#define SV_FILA_MAX      8    // fileiras do perfil

enum { SV_AGORA = 0, SV_INICIO, SV_FIM, SV_REACAO, SV_SALVO, SV_ABANDONO,
       SV_AVALIOU, SV_MANDOU, SV_ATIVIDADE, SV_JUNTO };
enum { SV_REAC_NADA = -2, SV_REAC_NAO = -1, SV_REAC_MEIO = 0, SV_REAC_GOSTOU = 1 };
enum { SV_FONTE_NUVIO = 0, SV_FONTE_TRAKT, SV_FONTE_SIMKL, SV_FONTE_LETTERBOXD };
// Estado de uma recomendacao que EU mandei.
enum { SV_REC_ENTREGUE = 0, SV_REC_ABRIU, SV_REC_VIU, SV_REC_COMECOU, SV_REC_REAGIU };
enum { SV_PERFIL_NADA = 0, SV_PERFIL_INDO, SV_PERFIL_OK, SV_PERFIL_FALHA, SV_PERFIL_NAO_ACHOU, SV_PERFIL_NEGADO };

typedef struct {
  char pessoaId[96], pessoaNome[64], pessoaAvatar[512];
  int  fonte;
  int  acao;            // SV_*
  int  reacao;          // SV_REAC_*
  char imdb[24], tipo[8], titulo[160];
  char poster[512];     // 2:3
  char arte[512];       // 16:9; vazio = o desenho cai no poster
  int  temporada, episodio;
  int  pct;             // 0..100, -1 = nao se sabe
  int  restanteMin;     // -1 = nao se sabe
  long long quando;     // epoch s; 0 = nao se sabe
  int  sobreMinhaRec;
  int  nota;            // so SV_AVALIOU: nota do tracker 0..100; 0 = nao se sabe
  char com[96];         // so SV_JUNTO: com quem assistiu ("Rafa, Fabi")
} SvEvento;

typedef struct {
  char id[96], nome[64], avatar[512];
  // Do CONTATO (servidor com /v1/contatos novo; vazio no antigo): o apelido que
  // a pessoa escolheu e o selo que o servidor deu ("criador").
  char apelido[32], selo[12];
  int  fonte;
  int  agora;           // assistindo agora (ponto verde)
  int  novo;            // ha evento que a pessoa ainda nao viu (anel laranja)
  int  nTit;
  SvEvento tit[SV_TIT_MAX];   // os ultimos, um por titulo, mais novo primeiro
} SvAmigo;

typedef struct {
  char imdb[24], titulo[160], poster[512];
  int  estado;          // SV_REC_*
  int  reacao;          // SV_REAC_*
  long long quando;
  char resposta[64];    // a mensagem curta de quem recebeu ("" = nenhuma)
  long long respondido; // epoch da resposta direta; 0 = nao respondeu
} SvEnviada;

// COMPARACAO (F08). Cada cartao tem um ESTADO explicito: dado privado,
// ausente, de amostra pequena ou de servidor antigo nunca vira um numero.
// Formula do "Match": mesma reacao (gostei/mais ou menos/nao gostei) entre os
// titulos que OS DOIS reagiram nos ultimos 90 dias; so com SV_CMP_MIN pares.
enum { SV_CMP_MATCH = 0, SV_CMP_FILMES, SV_CMP_SERIES, SV_CMP_COMUM, SV_CMP_GENEROS, SV_CMP_N };
enum { SV_CMPE_DESCONHECIDO = 0,  // o servidor nao mandou (antigo, ou sem perfil do servidor)
       SV_CMPE_CARREGANDO,        // pedindo, e ainda nao ha copia
       SV_CMPE_OK,
       SV_CMPE_POUCOS,            // menos de SV_CMP_MIN pares
       SV_CMPE_PRIVADO,           // a pessoa nao compartilha comigo
       SV_CMPE_EU_PRIVADO,        // EU nao compartilho: o servidor nao compara
       SV_CMPE_SEM_FONTE };       // nenhuma fonte tem esse dado (generos)
#define SV_CMP_MIN 5
typedef struct { int estado, pct, iguais, total, filmes, series; } SvCmp;
// O que a comparacao precisa saber, sem depender de recomenda.h.
typedef struct {
  int temDados;        // ha resposta do servidor (cache ou nova) para esta pessoa
  int carregando;
  int compartilha;     // -1 nao se sabe, 0 nao, 1 sim
  int euCompartilho;   // meu alcance >= 1
  int temGosto, total, iguais;          // "gosto" com pelo menos um par
  int temCmp, filmesTotal, filmesIguais, seriesTotal, seriesIguais, comumFilmes, comumSeries;
} SvCmpDados;
void socialvis_comparar(const SvCmpDados *d, SvCmp out[SV_CMP_N]);
// Texto pronto (traduzido) do valor de um cartao; `ok` = 1 quando e um dado.
void socialvis_cmp_texto(int qual, const SvCmp *c, char *dst, size_t tam, int *ok);
const char *socialvis_cmp_rotulo(int qual);

typedef struct {
  SvAmigo a;
  int estado;          // SV_PERFIL_*; refreshing keeps the cached first frame
  int compartilha;     // -1 unknown, 0 private, 1 shared with this viewer
  long long desde;      // 0 = nao se sabe
  int  porOnde;         // SV_FONTE_* (NUVIO = pelo codigo)
  int  gostoTotal;                          // number of paired shared reactions
  int  gostoPct, emComum;                    // -1 = sem dado
  int  minutosMes, filmesMes, seriesCurso;   // -1 = sem dado
  int  recsVistas, recsTotal;                // -1 = sem dado
  int  nAssistindo, nGostou, nMandou;
  SvEvento assistindo[SV_FILA_MAX];
  SvEvento gostou[SV_FILA_MAX];
  SvEnviada mandou[SV_FILA_MAX];
  SvCmp cmp[SV_CMP_N];
} SvPerfil;

// Por quadro. Barato (ver o topo).
void socialvis_atualizar(void);
// Sobe a cada reconstrucao: quem guarda copia/medida sabe que ficou velha.
unsigned socialvis_revisao(void);

int  socialvis_n_amigos(void);
const SvAmigo *socialvis_amigo(int i);         // NULL fora da faixa
int  socialvis_amigo_indice(const char *id);   // -1 se nao ha
int  socialvis_n_ao_vivo(void);

// O feed da aba Atividade: so o que conta (sem SV_ATIVIDADE), mais novo
// primeiro; "agora" na frente de tudo.
int  socialvis_n_eventos(void);
const SvEvento *socialvis_evento(int i);

// Refresh explicitly on each opening, including reopening the same friend.
void socialvis_abrir_perfil(const char *id);
// O perfil (tela C). 1 = achou a pessoa.
int  socialvis_perfil(const char *id, SvPerfil *saida);
// O que EU mandei para essa pessoa, o mais novo (para a cadeia "Voce mandou ›
// viu › gostou" da linha do amigo). 1 = ha.
int  socialvis_ultima_enviada(const char *id, SvEnviada *saida);
// O estado de uma rec que EU mandei, em UMA expressao ja traduzida: "ainda não
// viu", "abriu", "começou", "viu", "viu · gostou" / "mais ou menos" / "não
// gostou" (a resposta de quem recebeu), "Gostou"... Vale para a legenda do
// cartaz e para a cadeia da linha do amigo. `ok` (opcional) = 1 quando ele ja viu.
const char *socialvis_enviada_rotulo(const SvEnviada *m, int *ok);

// O anel laranja apaga: a novidade dessa pessoa foi vista. Grava em disco so
// quando muda (amigos-vistos.txt).
void socialvis_marcar_visto(const char *id);

// O MEU estado num titulo que me mandaram, pelo catalogo: 0 nada, 1 comecei
// (pct/T/E preenchidos), 2 vi. Para a cadeia da linha de recomendacao.
int  socialvis_meu_estado(const char *imdb, int *pct, int *t, int *e);

// Textos prontos (todos ja traduzidos). `dst` sempre termina em 0.
//   status: "Agora · T3E4 · faltam 12 min" / "Terminou · ontem" ...
void socialvis_status(const SvEvento *ev, char *dst, size_t tam);
//   quando relativo: "agora", "há 18 min", "ontem", "12/09"
void socialvis_quando(long long quando, char *dst, size_t tam);
//   "T3E4" ou "" (filme)
void socialvis_ep(const SvEvento *ev, char *dst, size_t tam);
//   o verbo do evento: "está vendo", "terminou", "gostou de"...
const char *socialvis_verbo(const SvEvento *ev);
//   "Trakt", "Simkl", "Letterboxd", "Nuvio"
const char *socialvis_fonte_nome(int fonte);
//   grupo de dia do feed: 0 Agora, 1 Hoje, 2 Ontem, 3 data, 4 Recentes (sem
//   hora); `rot` recebe o rotulo traduzido.
int  socialvis_dia(const SvEvento *ev, char *rot, size_t tam);

// O CAMINHO DO socialsrv: substitui o feed inteiro de uma vez (ver o topo).
// Hoje so os testes chamam; o socialsrv pode chamar daqui ou de svDoServidor.
void socialvis_definir_feed(const SvEvento *ev, int n);
void socialvis_definir_perfil_extra(const char *id, const SvPerfil *extra);

#ifdef NV_SOCIALVIS_DEMO
// DADOS DE EXEMPLO, so para as capturas (*_shot). 0 = ninguem, 1 = um amigo,
// 3 = tres amigos, 4 = tres com um assistindo agora e novidade.
void socialvis_demo(int cenario);
#endif

#endif
