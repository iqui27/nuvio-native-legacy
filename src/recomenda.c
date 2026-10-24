#include "arteurl.h"
// Recomendacoes entre amigos — ver a nota longa em recomenda.h para o porque
// do servico proprio e por que "realtime" aqui e sondagem.
//
// O QUE ESTE ARQUIVO GUARDA EM DISCO, e por que nao so o id: `recomendacoes.txt`
// leva titulo, poster, quem mandou e a frase. A aba Social precisa desenhar no
// PRIMEIRO quadro do arranque, antes de existir catalogo e antes de a rede
// responder — exatamente a razao pela qual salvos.c guarda titulo e poster em
// vez de uma lista de ids nus (ver a nota em salvos.h). O cursor e o ETag ficam
// em `recomendacoes-cursor.txt`: sem persistir o ETag, a primeira sondagem
// depois de cada arranque baixaria a lista inteira para descobrir que nada
// mudou.
//
// O QUE ELE NAO GUARDA: token. A identidade e montada a cada ciclo a partir de
// trakt.c ou de sessao.c e vai so no cabecalho do pedido.
#include "recomenda.h"
#include "atividade.h"
#include "dados.h"
#include "rede.h"
#include "js.h"
#include "trakt.h"
#include "simklauth.h"
#include "sessao.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "layout.h"
#include "idioma.h"
#include "ajustes.h"
#include "progresso.h"
#include "salvos.h"
#include "logotitulo.h"
#include "artemetahub.h"
#include "perfis.h"
#include "recresp.h"
#define NV_ESCALA_TELA_ATIVA   // mede pela tela do fator ativo (escala.h)
#include "escala.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// VAZIO E O PADRAO, e nao um esquecimento: tools/env.sh so emite -DNV_REC_URL
// quando NUVIO_REC_URL existe em local.properties, e o dono publica builds sem
// ele. Ver recomenda_ativo().
#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

static int recPerfil = 1;
static uint64_t recSessaoDona;
typedef struct {
  uint64_t sessao, viaSessao;
  int perfil;
  unsigned vezNome;
  char nomes[8][96], perfilCab[40], viaVista[80];
} RecFioContexto;
static pthread_key_t recFioChave;
static pthread_once_t recFioUmaVez = PTHREAD_ONCE_INIT;
static int recFioChaveOk;
static void recFioChaveCriar(void) {
  recFioChaveOk = pthread_key_create(&recFioChave, free) == 0;
}
static RecFioContexto *recFioContexto(void) {
  pthread_once(&recFioUmaVez, recFioChaveCriar);
  if (!recFioChaveOk) return NULL;
  RecFioContexto *c = pthread_getspecific(recFioChave);
  if (!c) {
    c = calloc(1, sizeof *c);
    if (!c || pthread_setspecific(recFioChave, c) != 0) { free(c); return NULL; }
  }
  return c;
}
static uint64_t recSessaoFio(void) {
  RecFioContexto *c = recFioContexto();
  return c ? c->sessao : UINT64_MAX;
}
static int recSessaoDefinir(uint64_t sessao) {
  RecFioContexto *c = recFioContexto();
  if (!c) return 0;
  c->sessao = sessao; c->perfil = perfis_ativo();
  return 1;
}
static const char *recArquivo(const char *base) {
  RecFioContexto *c = recFioContexto();
  if (!c) return "";
  // O fio conserva o perfil capturado; a UI usa o dono do cache carregado.
  int perfil = c->sessao ? c->perfil : recPerfil;
  if (perfil == 1) return base;
  char *nome = c->nomes[c->vezNome++ % 8];
  const char *ext = strrchr(base, '.');
  size_t n = ext ? (size_t)(ext - base) : strlen(base);
  snprintf(nome, 96, "%.*s-p%d%s", (int)n, base, perfil, ext ? ext : "");
  return nome;
}
static char *recLer(const char *nome) {
  return dados_ler(nome);
}
static void recApagar(const char *nome) {
  dados_apagar(nome);
  if (recPerfil == 1) {
    char antigo[96]; const char *fim = strstr(nome, "-p1.");
    if (fim) { snprintf(antigo, sizeof antigo, "%.*s%s", (int)(fim - nome), nome, fim + 3); dados_apagar(antigo); }
  }
}

#define REC_ARQ        recArquivo("recomendacoes.txt")
#define REC_ARQ_CURSOR recArquivo("recomendacoes-cursor.txt")
#define REC_ARQ_CARTAO recArquivo("recomendacoes-cartao.txt")
// Quem eu sou para o servico: id estavel e codigo de pareamento. Separado
// dos outros dois porque tem outro tempo de vida — a lista e o cursor mudam a
// cada ciclo, isto muda uma vez na vida da conta.
#define REC_ARQ_EU     recArquivo("recomendacoes-eu.txt")
// A RESPOSTA SOBRE APARECER PARA OS OUTROS, em arquivo PROPRIO e nao numa
// coluna de `recomendacoes-eu.txt`. Os dois tem o mesmo tempo de vida, mas nao
// o mesmo dono: `eu` e devolvido pelo servidor e reescrito a cada arranque,
// isto e uma escolha da PESSOA e so ela escreve. Um campo a mais no arquivo do
// servidor seria uma linha de codigo a menos e uma chance a mais de a resposta
// dela ser sobrescrita por uma resposta de rede.
#define REC_ARQ_APARECER recArquivo("recomendacoes-aparecer.txt")
#define REC_ARQ_DONO "recomendacoes-dono.txt" // UUID da conta; nunca o token

#define REC_INTERVALO_MS  60000u   // sondagem com o app aberto
#define REC_ESPERA_MS      2000u   // sem identidade ainda: tentar de novo logo
#define REC_CONTATOS_MS  600000u   // a lista de contatos muda devagar
#define REC_TEMPO_REDE       12    // segundos por requisicao
// RECUO QUANDO O SERVIDOR NAO RESPONDE (#203). 36 pessoas viam o Worker passar
// de 12 s entre 22h e 1h; cada TV insistia a cada 60 s, somando carga a um
// servidor que ja estava lento. Cada falha seguida (sem resposta ou 5xx)
// dobra a espera, ate 8x (8 min); a primeira resposta boa volta ao normal.
// Roda no fio proprio: nada disso toca a interface.
#define REC_RECUO_MAX        3
static int recFalhas;
static void recMarcarRede(int st) {
  if (recSessaoFio() && !perfis_sessao_travar(recSessaoFio())) return;
  if (st >= 200 && st < 500 && st != 401) recFalhas = 0;
  else if (recFalhas < REC_RECUO_MAX) recFalhas++;
  if (recSessaoFio()) perfis_sessao_destravar();
}

// Cartao de abertura, com a mesma pegada do de atualizacao.c.
// LARGO E COM O CARTAZ MENOR, e a medida saiu da primeira captura: com 1120 de
// largura e um cartaz de 260, a coluna de texto ficava com 704px e
// "Um Sonho de Liberdade" saia cortado em "Um Sonho de...". O titulo e o unico
// dado que a pessoa precisa ler daqui — se ele nao cabe, o cartao nao serve.
#define RC_W        1280.0f
#define RC_H         470.0f
#define RC_X        ((NV_TELA_W - RC_W) * 0.5f)
#define RC_Y        ((NV_TELA_H - RC_H) * 0.5f)
#define RC_PAD        56.0f
#define RC_POSTER_W  220.0f
#define RC_POSTER_H  330.0f
#define RC_ABRIR_MS  280.0f
#define RC_FECHAR_MS 160.0f
// Disco da foto de quem mandou, no cartao. 56 e o menor em que a INICIAL ainda
// se le a 3 m — a mesma conta que PS_AV_MIN faz em perfilsel.c, so que ali o
// disco e a tela inteira e aqui ele divide a linha com o nome.
#define RC_LOGO_W    520.0f
#define RC_LOGO_H     96.0f
#define RC_AVATAR     56.0f
#define RC_AVATAR_GAP 18.0f

// REC_SELO_H e REC_SELO_GAP moram em recomenda.h: quem desenha precisa deles
// para posicionar a linha dos selos.
// A marca amarela do IMDb, na mesma proporcao do selo de detail.c (60x30)
// reduzida para caber na coluna de 568px do painel. As medidas moram em
// recomenda.h — ver o comentario la.

// Os modelos prontos. O TEXTO FINAL passa por i18n na hora de desenhar, como
// todo o resto; o que viaja ao servidor e o INDICE, nao a frase — assim a
// mesma recomendacao chega em portugues numa TV e em ingles na outra.
static const char *MODELOS[REC_MODELOS] = {
  "Assiste isso hoje",
  "Melhor do ano",
  "Confia em mim",
  "Você vai chorar",
  "Dá pra ver junto?",
  "Terminei, sua vez"
};

// --- ESTADO, TODO ATRAS DO MUTEX ---------------------------------------------
static SDL_mutex *mtx;
static SDL_Thread *fio;
static int fioLigado, fioParar;

static RecItem    itens[REC_MAX];
static int        nItens;
static RecContato contatos[REC_CONTATOS_MAX];
static int        nContatos;
static long long  cursor;
static char       etagRec[96];
static int        registrado;
static char       meuId[96];
static char       meuCodigo[16];
// IDENTIDADE UNIFICADA (F08). `identRecurso` = o /v1/eu desta sessao disse
// "identidade1"; sem isso nada de unir contas aparece. `identTrakt` = o slug
// ligado a esta pessoa (verificado). Um pedido de cada vez, como o vinculo
// por codigo: e sempre um OK numa linha que fica esperando.
static int  identRecurso;
static char identTrakt[64];
static int  identPedido;          // 0 nada, 1 unir, 2 separar
// Um resultado POR SERVICO (REC_IDENT_*): o Trakt e o de sempre (identOp); Simkl
// e Letterboxd entram pelo mesmo pedido unico (identProv), um de cada vez.
static int  identOpP[REC_IDENT_N];
#define identOp identOpP[REC_IDENT_TRAKT]
static int  identProv;            // de quem e o identPedido
static char identArg[40];         // usuario declarado do Letterboxd a enviar
static int  identSimklLig;        // Simkl verificado neste perfil
static char identLbUsuario[40];   // Letterboxd declarado ("" = nenhum)
static int  identSimklOff;        // o servidor respondeu 501: sem SIMKL_CLIENT_ID

// APARECER PARA OUTRAS PESSOAS. `aparecer` e um dos tres REC_APARECER_*;
// `aparecerPendente` e -1 quando nao ha nada a dizer ao servidor, ou 0/1 para
// enviar. Sao dois campos e nao um porque "a resposta dela" e "o que falta
// avisar" tem tempos de vida diferentes: a resposta e definitiva no disco no
// mesmo instante em que ela aperta OK, o aviso pode levar tres ciclos de rede.
static int aparecer;
static int aparecerPendente = -1;

static RecSugestao sugestoes[REC_SUGESTOES_MAX];
static int         nSugestoes;
// Um de cada vez, como `removerId` e pela mesma razao: e sempre um OK numa tela
// que fica esperando a resposta.
static char        sugAdicionar[96];
// Pede a lista de sugestoes fora da hora (ao abrir a aba Social). Fora disso
// ela acompanha o relogio dos contatos — ver REC_CONTATOS_MS.
static int         pedirSugestoes;

// PEDIDOS DE CONTATO. Sao tres e nenhum e uma fila: vincular por codigo,
// remover alguem e revarrer o Trakt acontecem um por vez, disparados por um OK
// numa tela que fica esperando a resposta. Um vetor aqui seria estrutura sem
// uso — a mesma conta que a nota da `fila` de envio ja faz.
static char vincCodigo[16];
static char vincNome[64];
static int  vincEstado;
static char removerId[96];
static int  pedirTrakt, traktEstado, traktAchados;

// Fila de envio: UMA de cada vez, de proposito. O fluxo e "escolho amigo,
// escolho frase, confirmo" — nao ha como o dono disparar dois antes de ver o
// resultado do primeiro, e uma fila de verdade seria estrutura sem uso.
static struct {
  char imdb[24], tipo[8], titulo[160], poster[NV_ARTE_URL_MAX], ano[16];
  char para[96], texto[72];
  int  modelo;
  int  nota;                  // centesimos, como o `nota` do CatItem
  int  cheia;
} fila;
static int envioEstado;
// Ids a confirmar como vistos no servidor. Cabe uma sondagem inteira (o
// servidor devolve no maximo 50 por vez).
static long long vistoFila[REC_MAX];
static int       nVistoFila;

static int      pedidoAgora;
static Uint32   proximoMs, contatosMs;
static int contatosForcar;

// GERACAO, incrementada por recomenda_esquecer. Um ciclo que ja estava no ar
// quando alguem saiu da conta voltaria com a lista de quem saiu e a GRAVARIA de
// volta em disco — apagar tudo e ver reaparecer segundos depois e o pior tipo
// de defeito, porque quem viu nao consegue reproduzir. O fio le a geracao antes
// do pedido e joga fora a resposta se ela mudou.
static unsigned geracao;

// Cartao de abertura (so o fio principal toca nisto).
static int   cartaoAberto, cartaoMostrado;
static float cartaoEntrada;
static RecItem cartaoItem;
static char  pedido[24];
static int   temPedido;

// =============================================================================
// ENTRE AMIGOS ALEM DO TRAKT — estado (perfil publico, busca, pedidos, feed).
// Ver a nota longa em recomenda.h e docs/SOCIAL-PRIVACIDADE.md.
// =============================================================================
//
// ONDE O PERFIL MORA: recomendacoes-perfil.txt, e so ali. Ele e apagado por
// recomenda_esquecer (troca de conta, sair) — uma pessoa que sai nao deixa o
// apelido e a bio dela numa TV que outra vai usar. No servidor ele continua
// existindo POR IDENTIDADE ate ela despublicar (Ajustes) ou apagar os dados
// sociais; a proxima TV dela o adota no primeiro /v1/eu.
#define REC_ARQ_PERFIL recArquivo("recomendacoes-perfil.txt")

static const char *const GENERO_ID[REC_GENEROS_N] = {
  "acao", "aventura", "animacao", "comedia", "crime", "documentario", "drama",
  "familia", "fantasia", "ficcao", "misterio", "romance", "suspense", "terror"
};
// Chaves de i18n (portugues), na mesma ordem dos ids.
static const char *const GENERO_ROT[REC_GENEROS_N] = {
  "Ação", "Aventura", "Animação", "Comédia", "Crime", "Documentário", "Drama",
  "Família", "Fantasia", "Ficção científica", "Mistério", "Romance", "Suspense",
  "Terror"
};
const char *rec_genero_id(int i) {
  return (i >= 0 && i < REC_GENEROS_N) ? GENERO_ID[i] : "";
}
const char *rec_genero_rotulo(int i) {
  return (i >= 0 && i < REC_GENEROS_N) ? GENERO_ROT[i] : "";
}

static RecPerfil perfil;
static int       perfilTocado;     // a pessoa mexeu: nao adotar o do servidor
static unsigned  perfilVersao;     // sobe a cada mudanca local (ver enviarPerfil)
static int       perfilPendente;   // 0 nada, 1 publicar, 2 despublicar
static int       ativPendente = -1;
static int       apagarPendente;

// A OPERACAO SOCIAL EM CURSO. Uma por vez, como vincular/remover: e sempre um
// OK numa tela que espera a resposta.
enum { SOC_BUSCAR = 1, SOC_GOSTO, SOC_VER, SOC_PEDIR, SOC_ACEITAR, SOC_RECUSAR,
       SOC_CANCELAR, SOC_BLOQUEAR, SOC_DESBLOQ, SOC_PEDIDOS, SOC_BLOQUEADOS,
       SOC_COMUNIDADE };
static int  socOp;
static char socArg[400];
static int  socEstado;
static unsigned socVersao;

static RecPessoa achados[REC_COMUNIDADE_MAX];   // busca/gosto usam so REC_BUSCA_MAX
static int       nAchados, achadosOrigem;
static int       comMais, comPagina, comFechada;
static RecPessoa cartaoP;
static int       temCartao;
static char      cartaoRec[6][72];
static int       nCartaoRec;
static RecPessoa pedidosRec[REC_PEDIDOS_MAX];
static int       nPedidos;
static unsigned  pedidosVersao;
static struct { char pub[16]; char nome[64]; } bloq[REC_BLOQ_MAX];
static int       nBloq;

// Atividade a enviar. Quatro bastam: o player gera um evento por titulo, e a
// fila esvazia a cada ciclo de 200 ms.
#define ATIV_FILA 4
static struct { char imdb[24], tipo[8], titulo[160], ano[8]; int nota, agora; }
  ativFila[ATIV_FILA];
static int    nAtivFila;
// O que ja foi dito, para nao repetir a cada quadro nem a cada retomada.
static char   ativUltimo[24];
static int    ativUltimoAgora;
static Uint32 ativUltimoMs;

// O feed dos amigos como veio da ultima leitura, para a tela "Ver perfil" do
// amigo Nuvio (social.c) responder SEM rede.
#define FEED_MAX 30
static struct { char de[96]; char imdb[24], titulo[160], tipo[8]; int agora; long long criado; }
  feed[FEED_MAX];
static int nFeed;

// Definidas mais abaixo, na parte publica desta secao; declaradas aqui porque
// iniciar e responder_aparecer (que vem antes) as usam.
static void gravarPerfil(void);
static void carregarPerfil(void);
static void perfilZerarPublico(void);
// Redesenho do Social (fim do arquivo). Chamar com o mutex TOMADO.
static void socNovoLimpar(int apagar);
static void socNovoCarregar(void);
static void socNovoRegistrado(const char *nome, const char *exib, int alcance);
static void avisarAlcance(void);

// "Recursos sociais" desligado (por perfil, ajustes.h) tira o servico inteiro:
// nenhuma chamada de rede, nenhuma tela. Cada funcao daqui ja começa por esta.
int recomenda_ativo(void) { return NV_REC_URL[0] != 0 && ajustes_social(); }
int recomenda_aberta(void) { return cartaoAberto; }

const char *recomenda_modelo(int i) {
  return (i >= 0 && i < REC_MODELOS) ? MODELOS[i] : NULL;
}

const char *recomenda_pediu_abrir(void) {
  if (!temPedido) return NULL;
  temPedido = 0;
  return pedido;
}

// --- ARQUIVO -----------------------------------------------------------------
//
// Uma linha por recomendacao, campos separados por TAB e o TITULO por ULTIMO —
// o mesmo formato e a mesma razao de salvos.c: TAB porque titulo de filme tem
// ";" e "|" e nunca tabulacao, e o ultimo campo nao precisa de separador
// depois dele. Campo que faltar na leitura vira vazio e a linha continua
// valida, para que uma versao futura com mais colunas nao invalide o arquivo
// de quem ja usa o app.
//
// v1: id criado visto modelo tipo ano de deNome imdb poster texto | titulo
// v2: ... os mesmos, mais NOTA e DEAVATAR                          | titulo
//
// AS COLUNAS NOVAS ENTRAM ANTES DO TITULO porque o titulo e o unico campo que
// pode conter qualquer coisa e por isso e lido como "o resto da linha".
//
// QUEM DECIDE A VERSAO E A CONTAGEM DE TABS DA PROPRIA LINHA, e nao o cabecalho
// "# nuvio recomendacoes vN". Sao 11 tabs na v1 e 13 na v2, sempre — campo
// vazio ainda gasta o seu separador, e semTab garante que nenhum VALOR contem
// tabulacao. Pelo cabecalho, um arquivo truncado na primeira linha (ou copiado
// sem ela) seria lido com o deslocamento errado e o titulo viraria a nota; pela
// contagem, cada linha se explica sozinha. O cabecalho continua sendo escrito
// como v2, para quem abrir o arquivo com o olho.
#define REC_TABS_V1 11
#define REC_TABS_V2 13

static int contaTabs(const char *s) {
  int n = 0;
  for (; *s; s++) if (*s == '\t') n++;
  return n;
}

// Tira TAB e quebra de linha do que veio da rede. O servidor ja limita tamanho
// e o texto livre a a-z0-9, mas o NOME de exibicao vem do Trakt e do Supabase
// sem essa limpeza — um nome com tabulacao partiria a linha em duas.
static void semTab(char *s) {
  for (; *s; s++) if (*s == '\t' || *s == '\n' || *s == '\r') *s = ' ';
}

static char *campo(char **p) {
  char *ini = *p, *t;
  if (!ini) return (char *)"";
  t = strchr(ini, '\t');
  if (t) { *t = 0; *p = t + 1; } else { *p = NULL; }
  return ini;
}

// Chamar com o mutex TOMADO.
static void gravar(void) {
  // 1400 E NAO 1000 desde a v2: poster (512) + deAvatar (256) + titulo (160) +
  // de (96) + deNome (64) + o resto ja passam de 1100 bytes numa linha cheia, e
  // com o teto antigo a ultima linha sairia cortada no meio de uma URL.
  size_t cap = (size_t)REC_MAX * (1400u + NV_ARTE_URL_MAX - 512u) + 64u;
  char *buf = (char *)malloc(cap);
  size_t k = 0;
  int i;
  if (!buf) return;
  k += (size_t)snprintf(buf + k, cap - k, "# nuvio recomendacoes v2\n");
  for (i = 0; i < nItens && k + 1 < cap; i++)
    k += (size_t)snprintf(buf + k, cap - k,
                          "%lld\t%lld\t%d\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t"
                          "%d\t%s\t%s\n",
                          itens[i].id, itens[i].criado, itens[i].visto,
                          itens[i].modelo, itens[i].tipo, itens[i].ano,
                          itens[i].de, itens[i].deNome, itens[i].imdb,
                          itens[i].poster, itens[i].texto,
                          itens[i].nota, itens[i].deAvatar, itens[i].titulo);
  dados_gravar(REC_ARQ, buf);
  free(buf);
}

// Chamar com o mutex TOMADO. O ETag vai junto do cursor porque os dois so
// fazem sentido em par: um ETag guardado com o cursor errado pede um 304 para
// uma pergunta diferente da que foi feita.
static void gravarCursor(void) {
  char s[160];
  snprintf(s, sizeof s, "%lld\t%s\n", cursor, etagRec);
  dados_gravar(REC_ARQ_CURSOR, s);
}

// Chamar com o mutex TOMADO.
static void gravarEu(void) {
  char t[160];
  snprintf(t, sizeof t, "%s\t%s\n", meuId, meuCodigo);
  dados_gravar(REC_ARQ_EU, t);
}

// Chamar com o mutex TOMADO. `dados_gravar` e nao `dados_gravar_leve`: esta e
// uma resposta da PESSOA e nao um dado re-obtivel. Perder a lista de posters
// custa um download; perder um "nao" custa perguntar de novo a alguem que ja
// tinha respondido — e uma pergunta de consentimento que reaparece ensina a
// responder sem ler.
static void gravarAparecer(void) {
  char s[32];
  snprintf(s, sizeof s, "%d\n", aparecer);
  dados_gravar(REC_ARQ_APARECER, s);
}

void recomenda_iniciar(void) {
  char *b, *linha, *prox;
  if (!recomenda_ativo()) return;
  uint64_t sessao = perfis_sessao_geracao();
  if (!perfis_sessao_travar(sessao)) return;
  char usuario[80], jwt[3000];
  int identidade = sessao_identidade_local_copiar(usuario, sizeof usuario, jwt, sizeof jwt);
  memset(jwt, 0, sizeof jwt);
  // Sem login (ou sem modulo de conta) o dono e vazio: o cache local carrega,
  // e so o cache de OUTRA conta e descartado abaixo.
  if (identidade <= 0) usuario[0] = 0;
  b = dados_ler(REC_ARQ_DONO);
  if (b) {
    b[strcspn(b, "\r\n")] = 0;
    if (strcmp(b, usuario)) recomenda_esquecer_conta();
    free(b);
  }
  // Migra o cache primario existente na primeira abertura. Daqui em diante,
  // outra conta recusa os bancos ANTES de desenhar, mesmo sem rede.
  if (!dados_gravar(REC_ARQ_DONO, usuario)) { perfis_sessao_destravar(); return; }
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  if (!recSessaoDona) {
    recPerfil = perfis_ativo();
    recSessaoDona = perfis_sessao_geracao();
  }
  nItens = 0;
  b = recLer(REC_ARQ);
  for (linha = b; linha && *linha && nItens < REC_MAX; linha = prox) {
    char *p, *fim = strchr(linha, '\n');
    RecItem *r;
    int tabs;
    prox = fim ? fim + 1 : NULL;
    if (fim) *fim = 0;
    if (linha[0] == '#' || !linha[0]) continue;
    // ANTES de campo(), que troca cada TAB por um terminador.
    tabs = contaTabs(linha);
    p = linha;
    r = &itens[nItens];
    memset(r, 0, sizeof *r);
    r->id     = atoll(campo(&p));
    r->criado = atoll(campo(&p));
    r->visto  = atoi(campo(&p));
    r->modelo = atoi(campo(&p));
    snprintf(r->tipo,   sizeof r->tipo,   "%s", campo(&p));
    snprintf(r->ano,    sizeof r->ano,    "%s", campo(&p));
    snprintf(r->de,     sizeof r->de,     "%s", campo(&p));
    snprintf(r->deNome, sizeof r->deNome, "%s", campo(&p));
    snprintf(r->imdb,   sizeof r->imdb,   "%s", campo(&p));
    snprintf(r->poster, sizeof r->poster, "%s", campo(&p));
    snprintf(r->texto,  sizeof r->texto,  "%s", campo(&p));
    // v1 NAO TEM ESTES DOIS, e nao e um arquivo corrompido: e o cache gravado
    // pela versao instalada hoje na TV do dono. Sem este desvio, o titulo dele
    // seria lido como a nota e a lista abriria com quatro linhas sem nome.
    if (tabs >= REC_TABS_V2) {
      r->nota = atoi(campo(&p));
      snprintf(r->deAvatar, sizeof r->deAvatar, "%s", campo(&p));
    }
    snprintf(r->titulo, sizeof r->titulo, "%s", p ? p : "");
    if (r->id <= 0 || strncmp(r->imdb, "tt", 2)) continue;   // linha inutil
    nItens++;
  }
  free(b);
  b = recLer(REC_ARQ_CURSOR);
  if (b) {
    char *p = b, *fimLinha = strchr(b, '\n');
    if (fimLinha) *fimLinha = 0;
    cursor = atoll(campo(&p));
    snprintf(etagRec, sizeof etagRec, "%s", p ? p : "");
    free(b);
  }
  // O CODIGO VEM DO DISCO ANTES DA REDE. `registrado` continua 0 de proposito:
  // o ciclo ainda chama /v1/eu (e ele que atualiza o `visto` da pessoa no
  // servidor). O que este bloco evita e a tela de amigos abrir com o campo do
  // codigo em branco por um ou dois segundos toda vez que a TV liga.
  b = recLer(REC_ARQ_EU);
  if (b) {
    char *p = b, *fimLinha = strchr(b, '\n');
    if (fimLinha) *fimLinha = 0;
    snprintf(meuId, sizeof meuId, "%s", campo(&p));
    snprintf(meuCodigo, sizeof meuCodigo, "%s", p ? p : "");
    free(b);
  }
  // A RESPOSTA SOBRE APARECER VEM DO DISCO E NAO DA REDE, e por isso ela e lida
  // aqui e nao no primeiro ciclo: a aba Social pode abrir no primeiro segundo,
  // e um estado "nao perguntado" por falta de resposta do servidor mostraria a
  // tela de consentimento de novo a quem ja respondeu.
  aparecer = REC_APARECER_NAO_PERGUNTADO;
  b = recLer(REC_ARQ_APARECER);
  if (b) {
    int v = atoi(b);
    if (v == REC_APARECER_NAO || v == REC_APARECER_SIM) aparecer = v;
    free(b);
  }
  carregarPerfil();
  socNovoCarregar();
  printf("[recomenda] %d na lista local, cursor %lld, aparecer %d\n",
         nItens, cursor, aparecer);
  fflush(stdout);
  SDL_UnlockMutex(mtx);
  perfis_sessao_destravar();
  avisarAlcance();
}

int recomenda_n(void) {
  int n;
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx); n = nItens; SDL_UnlockMutex(mtx);
  return n;
}

int recomenda_n_novas(void) {
  int i, n = 0;
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx);
  for (i = 0; i < nItens; i++) if (!itens[i].visto) n++;
  SDL_UnlockMutex(mtx);
  return n;
}

int recomenda_item(int i, RecItem *saida) {
  int ok = 0;
  if (!recomenda_ativo() || !mtx || !saida) return 0;
  SDL_LockMutex(mtx);
  if (i >= 0 && i < nItens) { *saida = itens[i]; ok = 1; }
  SDL_UnlockMutex(mtx);
  return ok;
}

int recomenda_contatos(RecContato *saida, int max) {
  int i, n;
  if (!recomenda_ativo() || !mtx || !saida || max < 1) return 0;
  SDL_LockMutex(mtx);
  n = nContatos < max ? nContatos : max;
  for (i = 0; i < n; i++) saida[i] = contatos[i];
  SDL_UnlockMutex(mtx);
  return n;
}

// Fim de uma string JSON que comeca na aspa `p` (logo depois da aspa final).
static const char *fimCadeia(const char *p) {
  if (!p || *p != '"') return p;
  for (p++; *p && *p != '"'; p++) if (*p == '\\' && p[1]) p++;
  return *p ? p + 1 : p;
}

int rec_contato_ids(const char *p, const char *f, RecContato *c) {
  const char *e;
  if (!c) return 0;
  c->nIds = 0;
  e = p ? js_array(p, f, "ids") : NULL;
  while (e && (!f || e < f) && *e == '"' && c->nIds < REC_CONTATO_IDS) {
    char v[80] = "";
    // So "provedor:sujeito"; o resto e ignorado sem derrubar o contato.
    if (js_cadeia(e, v, sizeof v) && strchr(v, ':') && strchr(v, ':')[1])
      snprintf(c->ids[c->nIds++], sizeof c->ids[0], "%s", v);
    e = js_prox(fimCadeia(e));
  }
  return c->nIds;
}

int rec_contatos_canonica(const RecContato *c, int n, const char *id, char *dst, size_t tam) {
  int i, k;
  if (!c || !id || !id[0]) return 0;
  for (i = 0; i < n; i++)
    for (k = 0; k < c[i].nIds && k < REC_CONTATO_IDS; k++)
      if (!strcmp(c[i].ids[k], id) && strcmp(c[i].id, id)) {
        if (dst && tam) snprintf(dst, tam, "%s", c[i].id);
        return 1;
      }
  return 0;
}

// O contato (copia) que provou ser `id`. 1 quando achou.
static int contatoCanonico(const char *id, RecContato *saida) {
  int i, ok = 0;
  char c[96];
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx);
  if (rec_contatos_canonica(contatos, nContatos, id, c, sizeof c))
    for (i = 0; i < nContatos; i++)
      if (!strcmp(contatos[i].id, c)) { *saida = contatos[i]; ok = 1; break; }
  SDL_UnlockMutex(mtx);
  return ok;
}

int recomenda_pessoa_canonica(const char *id, char *dst, size_t tam) {
  RecContato c;
  if (!contatoCanonico(id, &c)) return 0;
  if (dst && tam) snprintf(dst, tam, "%s", c.id);
  return 1;
}

int recomenda_identidade_situacao(void) {
  int r;
  if (!recomenda_ativo() || !mtx) return REC_IDENT_INDISPONIVEL;
  SDL_LockMutex(mtx);
  r = !identRecurso ? REC_IDENT_INDISPONIVEL
    : identTrakt[0] ? REC_IDENT_UNIDA
    : (trakt_ativo() && sessao_logada()) ? REC_IDENT_PODE_UNIR : REC_IDENT_SEM_TRAKT;
  SDL_UnlockMutex(mtx);
  return r;
}
int recomenda_identidade_op(void) {
  int v;
  if (!mtx) return REC_IDENT_OP_NADA;
  SDL_LockMutex(mtx); v = identOp; SDL_UnlockMutex(mtx);
  return v;
}
void recomenda_identidade_op_limpar(void) {
  if (!mtx) return;
  SDL_LockMutex(mtx);
  if (identOp != REC_IDENT_OP_INDO) identOp = REC_IDENT_OP_NADA;
  SDL_UnlockMutex(mtx);
}
const char *recomenda_identidade_trakt(void) {
  // Copia estatica: a tela le por quadro; o fio so troca no /v1/eu.
  static char c[64];
  if (!mtx) return "";
  SDL_LockMutex(mtx); snprintf(c, sizeof c, "%s", identTrakt); SDL_UnlockMutex(mtx);
  return c;
}
static int identOcupado(void) {
  int i;
  for (i = 0; i < REC_IDENT_N; i++) if (identOpP[i] == REC_IDENT_OP_INDO) return 1;
  return 0;
}
static int identEnfileirar(int prov, int op, const char *arg) {
  int ok = 0;
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx);
  if (identRecurso && !identOcupado()) {
    identPedido = op; identProv = prov; identOpP[prov] = REC_IDENT_OP_INDO;
    snprintf(identArg, sizeof identArg, "%s", arg ? arg : "");
    pedidoAgora = 1; ok = 1;
  }
  SDL_UnlockMutex(mtx);
  return ok;
}
int recomenda_identidade_unir(void)    { return identEnfileirar(REC_IDENT_TRAKT, 1, NULL); }
int recomenda_identidade_separar(void) { return identEnfileirar(REC_IDENT_TRAKT, 2, NULL); }

// SIMKL E LETTERBOXD (F08, linhas da aba Social). Simkl: so se esta TV ja tem
// login do Simkl (o token e o de Ajustes, nunca pedido de novo) ou se ja ha um
// ligado (para poder desligar). Letterboxd: sempre que o servidor sabe; o
// usuario e DECLARADO, o servidor nao confere nada e so a propria pessoa o ve.
int recomenda_identidade_estado(int prov) {
  int r = REC_IDENT_E_INDISPONIVEL;
  if ((prov != REC_IDENT_SIMKL && prov != REC_IDENT_LETTERBOXD) || !recomenda_ativo() || !mtx)
    return r;
  SDL_LockMutex(mtx);
  if (identRecurso) {
    if (prov == REC_IDENT_SIMKL)
      r = identSimklLig ? REC_IDENT_E_LIGADO : identSimklOff ? REC_IDENT_E_SEM_SERVICO
        : simklauth_token()[0] ? REC_IDENT_E_PODE : REC_IDENT_E_INDISPONIVEL;
    else
      r = identLbUsuario[0] ? REC_IDENT_E_LIGADO : REC_IDENT_E_PODE;
  }
  SDL_UnlockMutex(mtx);
  return r;
}
int recomenda_identidade_op_de(int prov) {
  int v;
  if (prov < 0 || prov >= REC_IDENT_N || !mtx) return REC_IDENT_OP_NADA;
  SDL_LockMutex(mtx); v = identOpP[prov]; SDL_UnlockMutex(mtx);
  return v;
}
void recomenda_identidade_op_limpar_de(int prov) {
  if (prov < 0 || prov >= REC_IDENT_N || !mtx) return;
  SDL_LockMutex(mtx);
  if (identOpP[prov] != REC_IDENT_OP_INDO) identOpP[prov] = REC_IDENT_OP_NADA;
  SDL_UnlockMutex(mtx);
}
const char *recomenda_identidade_usuario_letterboxd(void) {
  static char c[40];
  if (!mtx) return "";
  SDL_LockMutex(mtx); snprintf(c, sizeof c, "%s", identLbUsuario); SDL_UnlockMutex(mtx);
  return c;
}
int recomenda_identidade_simkl_unir(void)       { return identEnfileirar(REC_IDENT_SIMKL, 1, NULL); }
int recomenda_identidade_simkl_separar(void)    { return identEnfileirar(REC_IDENT_SIMKL, 2, NULL); }
int recomenda_identidade_letterboxd_declarar(const char *usuario) {
  char u[40]; size_t i, n = 0;
  // minusculas, a-z 0-9 _ (o que o servidor aceita); o resto some.
  for (i = 0; usuario && usuario[i] && n + 1 < sizeof u; i++) {
    char c = usuario[i];
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') u[n++] = c;
  }
  u[n] = 0;
  if (n < 2 || n > 30) return 0;
  return identEnfileirar(REC_IDENT_LETTERBOXD, 1, u);
}
int recomenda_identidade_letterboxd_separar(void) { return identEnfileirar(REC_IDENT_LETTERBOXD, 2, NULL); }

int recomenda_aparecer(void) {
  int v;
  // SEM SERVICO NAO HA PERGUNTA. Um pacote sem NUVIO_REC_URL nao tem aba
  // Social, e devolver "nao perguntado" aqui faria a tela de consentimento
  // existir num app onde ela nao pode levar a lugar nenhum.
  if (!recomenda_ativo() || !mtx) return REC_APARECER_NAO;
  SDL_LockMutex(mtx); v = aparecer; SDL_UnlockMutex(mtx);
  return v;
}

void recomenda_responder_aparecer(int sim) {
  if (!recomenda_ativo()) return;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  aparecer = sim ? REC_APARECER_SIM : REC_APARECER_NAO;
  aparecerPendente = sim ? 1 : 0;
  gravarAparecer();
  // "NAO" TAMBEM SOME COM O PERFIL: a rota /v1/descobrivel=0 despublica de
  // verdade no servidor, e o aparelho tem de concordar (senao o Ajustes diria
  // "pesquisavel" para um perfil que ja nao existe la).
  if (!sim && perfil.publicado) {
    perfilZerarPublico(); perfilTocado = 1; perfilVersao++; perfilPendente = 0;
    gravarPerfil();
  }
  SDL_UnlockMutex(mtx);
  // PEDE UM CICLO AGORA porque um "sim" so vale quando o servidor souber, e o
  // efeito visivel dele (as sugestoes) vem no mesmo ciclo.
  recomenda_pedir_agora();
}

int recomenda_n_sugestoes(void) {
  int n;
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx); n = nSugestoes; SDL_UnlockMutex(mtx);
  return n;
}

int recomenda_sugestao(int i, RecSugestao *saida) {
  int ok = 0;
  if (!recomenda_ativo() || !mtx || !saida) return 0;
  SDL_LockMutex(mtx);
  if (i >= 0 && i < nSugestoes) { *saida = sugestoes[i]; ok = 1; }
  SDL_UnlockMutex(mtx);
  return ok;
}

int recomenda_adicionar_sugerido(const char *id) {
  if (!recomenda_ativo() || !id || !id[0]) return 0;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  if (sugAdicionar[0]) { SDL_UnlockMutex(mtx); return 0; }
  snprintf(sugAdicionar, sizeof sugAdicionar, "%s", id);
  // TIRA DA LISTA LOCAL NA HORA, como recomenda_remover_contato faz com o
  // contato removido e pela mesma razao: sem isto o nome continua sugerido por
  // ate 200 ms, que e tempo de sobra para um segundo OK mandar o mesmo pedido.
  { int i, k = 0;
    for (i = 0; i < nSugestoes; i++)
      if (strcmp(sugestoes[i].id, id)) sugestoes[k++] = sugestoes[i];
    nSugestoes = k; }
  SDL_UnlockMutex(mtx);
  recomenda_pedir_agora();
  return 1;
}

const char *recomenda_meu_codigo(void) {
  // ESTATICO E COPIADO, e nao um ponteiro para `meuCodigo`: o fio de rede
  // reescreve aquele vetor quando /v1/eu responde, e quem desenha guardaria um
  // ponteiro para memoria que muda debaixo dele. Sao 16 bytes.
  static char copia[16];
  if (!recomenda_ativo() || !mtx) return "";
  SDL_LockMutex(mtx);
  snprintf(copia, sizeof copia, "%s", meuCodigo);
  SDL_UnlockMutex(mtx);
  return copia;
}

int recomenda_vincular(const char *codigo) {
  char limpo[16];
  size_t k = 0;
  if (!recomenda_ativo() || !codigo) return 0;
  // A LIMPEZA E AQUI, e nao no servidor: o teclado da TV so entrega a-z0-9,
  // mas um codigo ditado por telefone chega com espaco no meio e com a letra
  // maiuscula de quem leu de uma foto. Seis caracteres exatos ou nada — assim o
  // 400 de "codigo invalido" nunca chega a sair do aparelho.
  for (; *codigo && k + 1 < sizeof limpo; codigo++) {
    char c = *codigo;
    if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) limpo[k++] = c;
  }
  limpo[k] = 0;
  if (k != 6) return 0;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  if (vincCodigo[0] || vincEstado == REC_VINC_INDO) { SDL_UnlockMutex(mtx); return 0; }
  snprintf(vincCodigo, sizeof vincCodigo, "%s", limpo);
  vincNome[0] = 0;
  vincEstado = REC_VINC_INDO;
  SDL_UnlockMutex(mtx);
  recomenda_pedir_agora();
  return 1;
}

int recomenda_vinculo_estado(void) {
  int e;
  if (!mtx) return REC_VINC_NADA;
  SDL_LockMutex(mtx); e = vincEstado; SDL_UnlockMutex(mtx);
  return e;
}

const char *recomenda_vinculo_nome(void) {
  static char copia[64];
  if (!mtx) return "";
  SDL_LockMutex(mtx);
  snprintf(copia, sizeof copia, "%s", vincNome);
  SDL_UnlockMutex(mtx);
  return copia;
}

void recomenda_vinculo_limpar(void) {
  if (!mtx) return;
  SDL_LockMutex(mtx);
  if (vincEstado != REC_VINC_INDO) { vincEstado = REC_VINC_NADA; vincNome[0] = 0; }
  SDL_UnlockMutex(mtx);
}

int recomenda_procurar_trakt(void) {
  if (!recomenda_ativo()) return 0;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  if (traktEstado == REC_TRAKT_INDO) { SDL_UnlockMutex(mtx); return 0; }
  pedirTrakt = 1;
  traktEstado = REC_TRAKT_INDO;
  traktAchados = 0;
  SDL_UnlockMutex(mtx);
  recomenda_pedir_agora();
  return 1;
}

int recomenda_trakt_estado(void) {
  int e;
  if (!mtx) return REC_TRAKT_NADA;
  SDL_LockMutex(mtx); e = traktEstado; SDL_UnlockMutex(mtx);
  return e;
}

int recomenda_trakt_achados(void) {
  int n;
  if (!mtx) return 0;
  SDL_LockMutex(mtx); n = traktAchados; SDL_UnlockMutex(mtx);
  return n;
}

void recomenda_trakt_limpar(void) {
  if (!mtx) return;
  SDL_LockMutex(mtx);
  if (traktEstado != REC_TRAKT_INDO) traktEstado = REC_TRAKT_NADA;
  SDL_UnlockMutex(mtx);
}

int recomenda_remover_contato(const char *id) {
  if (!recomenda_ativo() || !id || !id[0]) return 0;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  if (removerId[0]) { SDL_UnlockMutex(mtx); return 0; }
  snprintf(removerId, sizeof removerId, "%s", id);
  // TIRA DA LISTA LOCAL NA HORA. A confirmacao do servidor chega no proximo
  // ciclo e a lista e relida depois dele; sem isto o nome removido continua na
  // tela ate 200 ms depois, que e tempo suficiente para a pessoa apertar OK de
  // novo e mandar a mesma remocao duas vezes.
  { int i, k = 0;
    for (i = 0; i < nContatos; i++)
      if (strcmp(contatos[i].id, id)) contatos[k++] = contatos[i];
    nContatos = k; }
  SDL_UnlockMutex(mtx);
  recomenda_pedir_agora();
  return 1;
}

void recomenda_marcar_vistas(void) {
  int i, mudou = 0;
  if (!recomenda_ativo() || !mtx) return;
  SDL_LockMutex(mtx);
  for (i = 0; i < nItens; i++) {
    if (itens[i].visto) continue;
    itens[i].visto = 1;
    mudou = 1;
    if (nVistoFila < REC_MAX) vistoFila[nVistoFila++] = itens[i].id;
  }
  if (mudou) { gravar(); pedidoAgora = 1; }
  SDL_UnlockMutex(mtx);
}

int recomenda_envio_estado(void) {
  int e;
  if (!mtx) return REC_ENVIO_NADA;
  SDL_LockMutex(mtx); e = envioEstado; SDL_UnlockMutex(mtx);
  return e;
}

void recomenda_envio_limpar(void) {
  if (!mtx) return;
  SDL_LockMutex(mtx);
  if (envioEstado != REC_ENVIO_INDO) envioEstado = REC_ENVIO_NADA;
  SDL_UnlockMutex(mtx);
}

// =============================================================================
// ENTRE AMIGOS ALEM DO TRAKT — API publica (o que a tela e o player chamam).
// =============================================================================

// Mesma limpeza do servidor (limparTexto): a-z0-9 e espaco, minusculo. Fazer
// aqui o que ele faria evita a pessoa ver "Ana Luiza!" e receber "ana luiza".
static void limpaCurto(char *dst, size_t tam, const char *src, size_t max) {
  size_t k = 0;
  int espaco = 0;
  if (!src) src = "";
  for (; *src && k < max && k + 1 < tam; src++) {
    char c = *src;
    if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      if (espaco && k) dst[k++] = ' ';
      espaco = 0;
      if (k + 1 < tam && k < max) dst[k++] = c;
    } else espaco = 1;
  }
  dst[k] = 0;
}

// Chamar com o mutex TOMADO.
static void gravarPerfil(void) {
  char t[400];
  snprintf(t, sizeof t, "%d\t%d\t%d\t%d\t%u\t%d\t%s\t%s\n",
           perfil.publicado, perfil.foto, perfil.recentes, perfil.ativ,
           perfil.generos, perfilTocado, perfil.apelido, perfil.bio);
  dados_gravar(REC_ARQ_PERFIL, t);
}

static void carregarPerfil(void) {
  char *b = recLer(REC_ARQ_PERFIL);
  memset(&perfil, 0, sizeof perfil);
  perfilTocado = 0;
  if (!b) return;
  { char *p = b, *fim = strchr(b, '\n');
    if (fim) *fim = 0;
    perfil.publicado = atoi(campo(&p));
    perfil.foto      = atoi(campo(&p));
    perfil.recentes  = atoi(campo(&p));
    perfil.ativ      = atoi(campo(&p));
    perfil.generos   = (unsigned)strtoul(campo(&p), NULL, 10);
    perfilTocado     = atoi(campo(&p));
    limpaCurto(perfil.apelido, sizeof perfil.apelido, campo(&p), REC_APELIDO_MAX);
    limpaCurto(perfil.bio, sizeof perfil.bio, p ? p : "", REC_BIO_MAX);
    if (perfil.ativ < 0 || perfil.ativ > 2) perfil.ativ = 0;
    perfil.generos &= (1u << REC_GENEROS_N) - 1u;
    // publicado sem apelido nao existe: o servidor recusaria, e a tela nao teria
    // o que mostrar como "meu perfil".
    if (perfil.publicado && strlen(perfil.apelido) < 2) perfil.publicado = 0; }
  free(b);
}

int recomenda_perfil(RecPerfil *saida) {
  if (!saida) return 0;
  memset(saida, 0, sizeof *saida);
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx); *saida = perfil; SDL_UnlockMutex(mtx);
  return 1;
}

int recomenda_pesquisavel(void) {
  int v;
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx); v = perfil.publicado; SDL_UnlockMutex(mtx);
  return v;
}

int recomenda_perfil_publicar(const RecPerfil *p) {
  RecPerfil novo;
  if (!recomenda_ativo() || !p) return 0;
  if (!mtx) mtx = SDL_CreateMutex();
  novo = *p;
  limpaCurto(novo.apelido, sizeof novo.apelido, p->apelido, REC_APELIDO_MAX);
  limpaCurto(novo.bio, sizeof novo.bio, p->bio, REC_BIO_MAX);
  if (strlen(novo.apelido) < 2) return 0;
  novo.generos &= (1u << REC_GENEROS_N) - 1u;
  novo.foto = novo.foto ? 1 : 0;
  novo.recentes = novo.recentes ? 1 : 0;
  novo.publicado = 1;
  SDL_LockMutex(mtx);
  // O nivel de atividade e uma decisao a parte (Ajustes): publicar o perfil nao
  // a liga nem a desliga. So se o chamador mandou um valor diferente.
  if (novo.ativ < 0 || novo.ativ > 2) novo.ativ = perfil.ativ;
  if (novo.ativ != perfil.ativ) ativPendente = novo.ativ;
  perfil = novo;
  perfilTocado = 1;
  perfilVersao++;
  perfilPendente = 1;
  // Publicar E aparecer sao a mesma escolha: quem publica um perfil aceita
  // aparecer nas sugestoes (o servidor liga `descobrivel` na mesma chamada).
  aparecer = REC_APARECER_SIM;
  aparecerPendente = -1;
  gravarAparecer();
  gravarPerfil();
  SDL_UnlockMutex(mtx);
  recomenda_verificar();
  SDL_LockMutex(mtx); pedidoAgora = 1; SDL_UnlockMutex(mtx);
  return 1;
}

// Chamar com o mutex TOMADO.
static void perfilZerarPublico(void) {
  perfil.publicado = 0;
  perfil.apelido[0] = 0;
  perfil.bio[0] = 0;
  perfil.generos = 0;
  perfil.foto = 0;
  perfil.recentes = 0;
}

void recomenda_perfil_despublicar(void) {
  if (!recomenda_ativo()) return;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  perfilZerarPublico();
  perfilTocado = 1;
  perfilVersao++;
  perfilPendente = 2;
  // "Nao aparecer" e a resposta que ela deu, e nao "nao perguntado": ninguem
  // volta a ser perguntado por ter desligado.
  aparecer = REC_APARECER_NAO;
  aparecerPendente = -1;
  gravarAparecer();
  gravarPerfil();
  SDL_UnlockMutex(mtx);
  recomenda_verificar();
  SDL_LockMutex(mtx); pedidoAgora = 1; SDL_UnlockMutex(mtx);
}

void recomenda_atividade_nivel(int nivel) {
  if (!recomenda_ativo()) return;
  if (nivel < 0) nivel = 0;
  if (nivel > 2) nivel = 2;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  perfil.ativ = nivel;
  perfilTocado = 1;
  perfilVersao++;
  ativPendente = nivel;
  // Desligar limpa o que ainda nao saiu: nada do que a pessoa acabou de negar
  // pode ser enviado depois do gesto.
  if (nivel == 0 && !perfil.recentes) nAtivFila = 0;
  gravarPerfil();
  SDL_UnlockMutex(mtx);
  recomenda_verificar();
  SDL_LockMutex(mtx); pedidoAgora = 1; SDL_UnlockMutex(mtx);
}

void recomenda_apagar_dados_sociais(void) {
  if (!recomenda_ativo()) return;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  memset(&perfil, 0, sizeof perfil);
  perfilTocado = 1;
  perfilVersao++;
  perfilPendente = 0;
  ativPendente = -1;
  apagarPendente = 1;
  nAtivFila = 0;
  aparecer = REC_APARECER_NAO;
  aparecerPendente = -1;
  gravarAparecer();
  gravarPerfil();
  SDL_UnlockMutex(mtx);
  recomenda_verificar();
  SDL_LockMutex(mtx); pedidoAgora = 1; SDL_UnlockMutex(mtx);
}

// --- operacoes sociais ---------------------------------------------------------

static int socIniciar(int op, const char *arg) {
  if (!recomenda_ativo()) return 0;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  if (socEstado == REC_SOC_INDO) { SDL_UnlockMutex(mtx); return 0; }
  socOp = op;
  socVersao++;
  snprintf(socArg, sizeof socArg, "%s", arg ? arg : "");
  socEstado = REC_SOC_INDO;
  SDL_UnlockMutex(mtx);
  recomenda_verificar();
  SDL_LockMutex(mtx); pedidoAgora = 1; SDL_UnlockMutex(mtx);
  return 1;
}

// Handle publico: 10 letras/numeros. Recusar aqui poupa um 404 do servidor e,
// principalmente, impede que algo maior que o buffer chegue ao JSON.
static int pubValido(const char *s) {
  int n = 0;
  if (!s) return 0;
  for (; *s; s++, n++)
    if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9'))) return 0;
  return n == 10;
}

int recomenda_buscar(const char *texto) {
  char q[48];
  limpaCurto(q, sizeof q, texto, 40);
  // Espacos saem: o servidor compara apelidos sem eles, e um codigo ditado
  // ("ab cd 12") tem de virar o codigo.
  { char *w = q, *r = q; for (; *r; r++) if (*r != ' ') *w++ = *r; *w = 0; }
  if (strlen(q) < 3) {
    if (mtx) { SDL_LockMutex(mtx); socEstado = REC_SOC_CURTA; SDL_UnlockMutex(mtx); }
    return 0;
  }
  return socIniciar(SOC_BUSCAR, q);
}
// Os salvos (lista local) entram como sinal de gosto alem do que foi concluido.
// salvos.c nao e thread-safe e o pedido sai do fio de rede, entao os ids sao
// copiados AQUI, no fio da interface, antes de enfileirar.
#define GOSTO_SALVOS_MAX 20
static char gostoSalvos[GOSTO_SALVOS_MAX][24];
static int  nGostoSalvos;
int recomenda_sugeridos_gosto(void) {
  int i, n = 0, total = salvos_n();
  for (i = 0; i < total && n < GOSTO_SALVOS_MAX; i++) {
    const SalvoItem *it = salvos_item(i);
    if (!it || strncmp(it->id, "tt", 2)) continue;
    SDL_LockMutex(mtx);
    snprintf(gostoSalvos[n], sizeof gostoSalvos[n], "%s", it->id);
    SDL_UnlockMutex(mtx);
    n++;
  }
  SDL_LockMutex(mtx); nGostoSalvos = n; SDL_UnlockMutex(mtx);
  return socIniciar(SOC_GOSTO, "");
}
int recomenda_comunidade(int pagina) {
  char arg[16];
  if (pagina < 0) pagina = 0;
  snprintf(arg, sizeof arg, "%d", pagina);
  return socIniciar(SOC_COMUNIDADE, arg);
}
int recomenda_comunidade_mais(void)    { return comMais; }
int recomenda_comunidade_pagina(void)  { return comPagina; }
int recomenda_comunidade_fechada(void) { return comFechada; }
int recomenda_ver_perfil(const char *pub)     { return pubValido(pub) && socIniciar(SOC_VER, pub); }
int recomenda_pedir_amizade(const char *pub)  { return pubValido(pub) && socIniciar(SOC_PEDIR, pub); }
int recomenda_aceitar(const char *pub)        { return recomenda_responder_pedido(pub, 1); }
int recomenda_recusar(const char *pub)        { return recomenda_responder_pedido(pub, 0); }
int recomenda_cancelar_pedido(const char *pub){ return pubValido(pub) && socIniciar(SOC_CANCELAR, pub); }
int recomenda_desbloquear(const char *pub)    { return pubValido(pub) && socIniciar(SOC_DESBLOQ, pub); }
int recomenda_listar_pedidos(void)    { return socIniciar(SOC_PEDIDOS, ""); }
int recomenda_listar_bloqueados(void) { return socIniciar(SOC_BLOQUEADOS, ""); }
// Bloquear aceita o handle (de um cartao) OU o id (da lista de amigos, onde ele
// ja e conhecido). O prefixo diz qual e.
int recomenda_bloquear(const char *pubOuId) {
  char arg[112];
  if (!pubOuId || !pubOuId[0]) return 0;
  if (pubValido(pubOuId)) snprintf(arg, sizeof arg, "p:%s", pubOuId);
  else if (strchr(pubOuId, ':') && strlen(pubOuId) < 100) snprintf(arg, sizeof arg, "i:%s", pubOuId);
  else return 0;
  return socIniciar(SOC_BLOQUEAR, arg);
}

int recomenda_soc_estado(void) {
  int e;
  if (!mtx) return REC_SOC_NADA;
  SDL_LockMutex(mtx); e = socEstado; SDL_UnlockMutex(mtx);
  return e;
}
void recomenda_soc_limpar(void) {
  if (!mtx) return;
  SDL_LockMutex(mtx);
  if (socEstado != REC_SOC_INDO) socEstado = REC_SOC_NADA;
  SDL_UnlockMutex(mtx);
}

int recomenda_n_achados(void) {
  int n;
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx); n = nAchados; SDL_UnlockMutex(mtx);
  return n;
}
int recomenda_achado(int i, RecPessoa *saida) {
  int ok = 0;
  if (!recomenda_ativo() || !mtx || !saida) return 0;
  SDL_LockMutex(mtx);
  if (i >= 0 && i < nAchados) { *saida = achados[i]; ok = 1; }
  SDL_UnlockMutex(mtx);
  return ok;
}
int recomenda_achados_origem(void) { return achadosOrigem; }
int recomenda_cartao(RecPessoa *saida) {
  int ok = 0;
  if (!recomenda_ativo() || !mtx || !saida) return 0;
  SDL_LockMutex(mtx);
  if (temCartao) { *saida = cartaoP; ok = 1; }
  SDL_UnlockMutex(mtx);
  return ok;
}
int recomenda_cartao_n_recentes(void) { return nCartaoRec; }
int recomenda_cartao_recente(int i, char *titulo, size_t tam) {
  int ok = 0;
  if (!mtx || !titulo || !tam) return 0;
  SDL_LockMutex(mtx);
  if (i >= 0 && i < nCartaoRec) { snprintf(titulo, tam, "%s", cartaoRec[i]); ok = 1; }
  SDL_UnlockMutex(mtx);
  return ok;
}
int recomenda_n_pedidos(void) {
  int n;
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx); n = nPedidos; SDL_UnlockMutex(mtx);
  return n;
}
int recomenda_pedido(int i, RecPessoa *saida) {
  int ok = 0;
  if (!recomenda_ativo() || !mtx || !saida) return 0;
  SDL_LockMutex(mtx);
  if (i >= 0 && i < nPedidos) { *saida = pedidosRec[i]; ok = 1; }
  SDL_UnlockMutex(mtx);
  return ok;
}
int recomenda_n_bloqueados(void) {
  int n;
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx); n = nBloq; SDL_UnlockMutex(mtx);
  return n;
}
int recomenda_bloqueado(int i, char *pub, size_t tp, char *nome, size_t tn) {
  int ok = 0;
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx);
  if (i >= 0 && i < nBloq) {
    if (pub && tp) snprintf(pub, tp, "%s", bloq[i].pub);
    if (nome && tn) snprintf(nome, tn, "%s", bloq[i].nome);
    ok = 1;
  }
  SDL_UnlockMutex(mtx);
  return ok;
}

// --- atividade (o player avisa; ESTE modulo decide se algo sai) --------------

// UNICO PONTO que prepara um imdb para o servico. O card de Continuar assistindo
// de serie traz "tt123:S:E" e o servidor so aceita /^tt\d{1,10}$/ (HTTP 400,
// issues #363 e a atividade). Tudo que sai com imdb passa por aqui: o id da serie.
static void imdbServidor(char *dst, size_t tam, const char *src) {
  atividade_id_puro(dst, tam, src);
}

// Chamar com o mutex TOMADO.
static void ativEnfileirar(const CatItem *ci, int agora) {
  int i, k = 0;
  char idPuro[24];
  if (nAtivFila >= ATIV_FILA) return;
  imdbServidor(idPuro, sizeof idPuro, ci->imdb);
  // Um titulo por vez na fila: o ultimo estado dele e o que vale.
  for (i = 0; i < nAtivFila; i++)
    if (strcmp(ativFila[i].imdb, idPuro)) ativFila[k++] = ativFila[i];
  nAtivFila = k;
  memset(&ativFila[nAtivFila], 0, sizeof ativFila[0]);
  snprintf(ativFila[nAtivFila].imdb,   sizeof ativFila[0].imdb,   "%s", idPuro);
  snprintf(ativFila[nAtivFila].tipo,   sizeof ativFila[0].tipo,   "%s",
           !strcmp(ci->tipo, "series") ? "series" : "movie");
  snprintf(ativFila[nAtivFila].titulo, sizeof ativFila[0].titulo, "%s", ci->titulo);
  { int j, n = 0;      // ano = os quatro primeiros digitos do meta, como recomenda_enviar
    for (j = 0; ci->meta[j] && n < 4; j++)
      if (ci->meta[j] >= '0' && ci->meta[j] <= '9') ativFila[nAtivFila].ano[n++] = ci->meta[j];
      else if (n) break;
    ativFila[nAtivFila].ano[n == 4 ? 4 : 0] = 0; }
  ativFila[nAtivFila].nota = (ci->nota >= 0 && ci->nota <= 100) ? ci->nota : 0;
  ativFila[nAtivFila].agora = agora;
  nAtivFila++;
  semTab(ativFila[nAtivFila - 1].titulo);
}

void recomenda_atividade_passo(const CatItem *ci, int tocando) {
  Uint32 t;
  if (!recomenda_ativo() || !mtx || !ci || !ci->imdb[0] || !tocando) return;
  // "ASSISTINDO AGORA" SO NO NIVEL 2, e so uma vez por titulo (e de novo depois
  // de 20 min, para o aviso de dez nao vencer com o filme ainda rodando).
  SDL_LockMutex(mtx);
  t = SDL_GetTicks();
  if (perfil.ativ >= 2 &&
      (strcmp(ativUltimo, ci->imdb) || !ativUltimoAgora ||
       (Sint32)(t - ativUltimoMs) > 20 * 60 * 1000)) {
    snprintf(ativUltimo, sizeof ativUltimo, "%s", ci->imdb);
    ativUltimoAgora = 1;
    ativUltimoMs = t;
    ativEnfileirar(ci, 1);
  }
  SDL_UnlockMutex(mtx);
}

void recomenda_atividade_fim(const CatItem *ci, int concluiu) {
  if (!recomenda_ativo() || !mtx || !ci || !ci->imdb[0]) return;
  SDL_LockMutex(mtx);
  // Saiu do titulo: da proxima vez que tocar, "agora" vale de novo.
  ativUltimo[0] = 0; ativUltimoAgora = 0;
  // "ASSISTIU" SO QUANDO CONCLUIU, e so com o interruptor (amigos) OU o
  // "vistos recentemente" publico ligado. Quem largou aos 8% nao assistiu.
  if (concluiu && (perfil.ativ >= 1 || perfil.recentes)) ativEnfileirar(ci, 0);
  SDL_UnlockMutex(mtx);
}

unsigned recomenda_geracao(void) {
  unsigned g;
  if (!mtx) return geracao;
  SDL_LockMutex(mtx); g = geracao; SDL_UnlockMutex(mtx);
  return g;
}

static void limparEstado(int apagar) {
  if (apagar) {
    char nome[48];
    snprintf(nome, sizeof nome, "amigos-vistos-p%d.txt", recPerfil);
    recApagar(nome);
  }
  if (apagar) recresp_esquecer();   // "Ja assisti" e respostas sao desta pessoa, como a lista
  if (!mtx) { geracao++; }
  if (!mtx) {
    if (apagar) {
      recApagar(REC_ARQ); recApagar(REC_ARQ_CURSOR);
      recApagar(REC_ARQ_CARTAO); recApagar(REC_ARQ_EU);
      recApagar(REC_ARQ_APARECER); recApagar(REC_ARQ_PERFIL);
    }
    socNovoLimpar(apagar);
    return;
  }
  SDL_LockMutex(mtx);
  geracao++;
  nItens = 0;
  nContatos = 0;
  nSugestoes = 0;
  sugAdicionar[0] = 0;
  pedirSugestoes = 0;
  // A RESPOSTA VOLTA A "NAO PERGUNTADO", e nao a "nao". Quem entra na conta
  // depois nao respondeu nada, e herdar o "nao" de quem saiu seria esconder a
  // pergunta de alguem que nunca a viu. Herdar o "sim" seria pior ainda. O
  // servidor guarda a resposta POR IDENTIDADE, entao a de quem saiu continua
  // valendo para ela, e a de quem entrar e relida no primeiro registro.
  aparecer = REC_APARECER_NAO_PERGUNTADO;
  aparecerPendente = -1;
  nVistoFila = 0;
  cursor = 0;
  etagRec[0] = 0;
  meuId[0] = 0;
  meuCodigo[0] = 0;
  registrado = 0;
  fila.cheia = 0;
  envioEstado = REC_ENVIO_NADA;
  // OS PEDIDOS DE CONTATO TAMBEM MORREM AQUI. Um vinculo por codigo no ar
  // quando alguem sai da conta voltaria vinculando a pessoa ERRADA — a
  // identidade do cabecalho ja e a da conta seguinte.
  vincCodigo[0] = 0; vincNome[0] = 0; vincEstado = REC_VINC_NADA;
  identRecurso = 0; identTrakt[0] = 0; identPedido = 0;
  memset(identOpP, 0, sizeof identOpP);
  identSimklLig = 0; identLbUsuario[0] = 0; identSimklOff = 0; identArg[0] = 0;
  removerId[0] = 0;
  pedirTrakt = 0; traktEstado = REC_TRAKT_NADA; traktAchados = 0;
  // O PERFIL, OS ACHADOS, OS PEDIDOS E A ATIVIDADE TAMBEM. Nada social de quem
  // saiu pode aparecer na tela (ou seguir na fila) de quem entrar: o apelido, a
  // bio, quem ela procurou, quem lhe pediu amizade, o que estava para ser dito
  // sobre o que ela assistiu. O servidor guarda por identidade; a proxima TV dela
  // o adota de novo no primeiro /v1/eu.
  memset(&perfil, 0, sizeof perfil);
  perfilTocado = 0; perfilVersao++;
  perfilPendente = 0; ativPendente = -1; apagarPendente = 0;
  socOp = 0; socArg[0] = 0; socEstado = REC_SOC_NADA; socVersao++;
  nAchados = 0; achadosOrigem = 0; temCartao = 0; nCartaoRec = 0;
  comMais = comPagina = comFechada = 0;
  nPedidos = 0; nBloq = 0; pedidosVersao++; contatosForcar = 0;
  nAtivFila = 0; ativUltimo[0] = 0; ativUltimoAgora = 0;
  nFeed = 0;
  nGostoSalvos = 0;
  socNovoLimpar(apagar);
  SDL_UnlockMutex(mtx);
  avisarAlcance();
  if (apagar) {
    recApagar(REC_ARQ_PERFIL); recApagar(REC_ARQ);
    recApagar(REC_ARQ_CURSOR); recApagar(REC_ARQ_CARTAO);
    recApagar(REC_ARQ_EU); recApagar(REC_ARQ_APARECER);
  }
  cartaoAberto = 0;
  cartaoMostrado = 0;
}

// --- REDE --------------------------------------------------------------------
//
// Tudo daqui para baixo roda NO FIO, com uma excecao anotada. O mutex e tomado
// so para ler a identidade e para publicar o resultado — nunca durante a
// requisicao, senao o desenho travaria pelo tempo da rede.

// Buffers do fio. ESTATICOS e nao na pilha porque o token do Supabase tem ate
// 3000 caracteres e a pilha de um fio no webOS nao e lugar para isso; so o fio
// de rede os toca, e ele e um so.
static char fioAut[3200];
static char fioVia[32];
static char fioPerfil[40];
static char fioUrl[600];

static int recTravarFio(void) {
  if (recSessaoFio() && !perfis_sessao_travar(recSessaoFio())) return 0;
  SDL_LockMutex(mtx);
  return 1;
}
static void recDestravarFio(void) {
  SDL_UnlockMutex(mtx);
  if (recSessaoFio()) perfis_sessao_destravar();
}

void recomenda_esquecer(void) {
  if (recSessaoFio() && !perfis_sessao_travar(recSessaoFio())) return;
  limparEstado(1);
  if (recSessaoFio()) perfis_sessao_destravar();
}

void recomenda_esquecer_conta(void) {
  // Bancos ficam em disco quando se troca de PERFIL, mas nao quando a conta
  // inteira sai. Nao usamos recArquivo: o fio pode estar preso ao perfil A.
  static const char *bases[] = {
    "recomendacoes.txt", "recomendacoes-cursor.txt", "recomendacoes-cartao.txt",
    "recomendacoes-eu.txt", "recomendacoes-aparecer.txt", "recomendacoes-perfil.txt",
    "recomendacoes-alcance.txt", "recomendacoes-feed.json", "recomendacoes-amigo.json",
    "recomendacoes-respostas.txt", "amigos-vistos.txt"
  };
  limparEstado(0);
  recresp_abrir_sessao();
  for (size_t i = 0; i < sizeof bases / sizeof bases[0]; i++) {
    dados_apagar(bases[i]);
    const char *ext = strrchr(bases[i], '.');
    size_t n = ext ? (size_t)(ext - bases[i]) : strlen(bases[i]);
    for (int p = 1; p <= CONTA_PERFIL_MAX; p++) {
      char nome[96];
      snprintf(nome, sizeof nome, "%.*s-p%d%s", (int)n, bases[i], p, ext ? ext : "");
      dados_apagar(nome);
    }
  }
  dados_apagar(REC_ARQ_DONO);
}

// CADA PERFIL NUVIO E UMA PESSOA NO SOCIAL. O cabecalho so sai quando o perfil
// ativo NAO e o principal: o principal continua `nuvio:<sub>` no servidor, e e
// por isso que nada gravado antes muda de dono. Trakt nao leva: o slug ja e de
// uma pessoa. 1 quando escreveu o cabecalho em `dst`.
static int perfilCab(char *dst, size_t tam, int viaNuvio) {
  const ContaPerfil *p;
  dst[0] = 0;
  if (!viaNuvio) return 0;
  p = perfis_item_ativo();
  if (!p || p->primario || p->indice < 1) return 0;
  snprintf(dst, tam, "X-Nuvio-Perfil: %d", p->indice);
  return 1;
}

// Monta os dois cabecalhos que TODA rota do servico exige. Preferencia pelo
// Trakt quando os dois existem: e a unica identidade que ja tem lista de
// amigos pronta (users/me/following), entao ela vira contatos sem o dono
// digitar codigo nenhum. 0 quando nao ha identidade — e "nao ha" e um estado
// normal, nao um erro: o app pode estar no primeiro segundo do arranque.
// "Authorization: Bearer <token da conta>" em dst; 0 sem sessao de usuario.
// Pela COPIA do token (#203): a renovacao reescreve o buffer da sessao em
// outro fio, e ler direto dele podia mandar um JWT pela metade.
static int bearerSessao(char *dst, size_t n) {
  char t[3000];
  int ok = sessao_token_copiar(t, sizeof t);
  if (ok) ok = snprintf(dst, n, "Authorization: Bearer %s", t) < (int)n;
  memset(t, 0, sizeof t);
  return ok;
}

// Renovar pode fazer HTTP: vem ANTES da trava da abertura. Depois dela,
// identidade/identidadeEm so copiam a prova local e conferem a geracao.
static void recAtualizarToken(void) {
  if (trakt_ativo()) return;
  char t[3000];
  sessao_token_copiar(t, sizeof t);
  memset(t, 0, sizeof t);
}
static int bearerSessaoLocal(char *dst, size_t n) {
  char usuario[80], t[3000];
  int ok = sessao_identidade_local_copiar(usuario, sizeof usuario, t, sizeof t);
  if (ok) ok = snprintf(dst, n, "Authorization: Bearer %s", t) < (int)n;
  memset(t, 0, sizeof t);
  return ok;
}

static char *recPostar(const char *url, int tempo, const char **cab, const char *corpo, int *status) {
  if (recSessaoFio() && !perfis_sessao_atual(recSessaoFio())) { if (status) *status = 0; return NULL; }
  return rede_postar_st(url, tempo, cab, corpo, status);
}
static char *recBaixar(const char *url, int tempo, const char **cab, int *status) {
  if (recSessaoFio() && !perfis_sessao_atual(recSessaoFio())) { if (status) *status = 0; return NULL; }
  return rede_baixar_st(url, tempo, cab, status);
}
static char *recBaixarCom(const char *url, int tempo, const char **cab) {
  if (recSessaoFio() && !perfis_sessao_atual(recSessaoFio())) return NULL;
  return rede_baixar_com(url, tempo, cab);
}
static char *recBaixarEtag(const char *url, int tempo, const char **cab, int *status, char *etag, size_t n) {
  if (recSessaoFio() && !perfis_sessao_atual(recSessaoFio())) { if (status) *status = 0; return NULL; }
  return rede_baixar_etag(url, tempo, cab, status, etag, n);
}

static int identidade(const char **cab) {
  if (perfis_precisa_escolher()) return 0;
  const char *tcab[4];
  char chave[160];
  if (trakt_ativo() && trakt_cabecalhos(tcab, fioAut, sizeof fioAut,
                                        chave, sizeof chave)) {
    snprintf(fioVia, sizeof fioVia, "X-Nuvio-Auth: trakt");
  } else if (bearerSessaoLocal(fioAut, sizeof fioAut)) {
    snprintf(fioVia, sizeof fioVia, "X-Nuvio-Auth: nuvio");
  } else {
    return 0;
  }
  cab[0] = fioAut;
  cab[1] = fioVia;
  cab[2] = perfilCab(fioPerfil, sizeof fioPerfil, !strcmp(fioVia, "X-Nuvio-Auth: nuvio"))
           ? fioPerfil : NULL;
  cab[3] = NULL;
  return 1;
}

static void url(const char *caminho) {
  snprintf(fioUrl, sizeof fioUrl, "%s%s", NV_REC_URL, caminho);
}

// Escapa para dentro de uma string JSON. So o necessario: aspas, barra e os
// controles. Um gerador completo seria mais codigo do que o app inteiro usa.
static void jsonEsc(char *dst, size_t tam, const char *s) {
  size_t k = 0;
  if (!s) s = "";
  for (; *s && k + 7 < tam; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') { dst[k++] = '\\'; dst[k++] = (char)c; }
    else if (c < 0x20)         { k += (size_t)snprintf(dst + k, tam - k, "\\u%04x", c); }
    else                        dst[k++] = (char)c;
  }
  dst[k] = 0;
}

// POST /v1/eu. Cria a pessoa na primeira chamada de cada TV e devolve o id
// estavel. Sem ele nenhuma outra rota tem a quem responder.
static int registrar(const char **cab) {
  char *r = NULL;
  char id[96] = "", codigo[16] = "";
  int st = 0, desc = 0, alc = -1;
  char nome[64] = "", exib[40] = "", corpoEu[800];
  char slugLigado[64] = "", lbLigado[40] = "";
  int recurso = 0, simklLigado = 0;
  // O NOME E A FOTO DO PERFIL ATIVO vao junto: o token da conta Nuvio nao diz
  // nome nenhum (user_metadata vazio), e era por isso que um amigo por codigo
  // aparecia como UUID. Sem perfil na lista, corpo vazio (o servidor mantem).
  { if (!recTravarFio()) return 0;
    const ContaPerfil *pf = perfis_item_ativo();
    corpoEu[0] = 0;
    if (pf && pf->nome[0]) {
      char n[160], a[700];
      jsonEsc(n, sizeof n, pf->nome);
      jsonEsc(a, sizeof a, pf->avatarUrl);
      snprintf(corpoEu, sizeof corpoEu, "{\"nome\":\"%s\",\"avatar\":\"%s\"}", n, a);
    } recDestravarFio(); }
  url("/v1/eu");
  r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpoEu, &st);
  recMarcarRede(st);
  if (r && st >= 200 && st < 300) {
    alc = (int)js_num(r, r + strlen(r), "alcance", -1.0);
    js_texto_raiz(r, "nome", nome, sizeof nome);
    js_texto_raiz(r, "exibicao", exib, sizeof exib);
    // O QUE O SERVIDOR GUARDOU SOBRE APARECER. Ele e a autoridade por
    // IDENTIDADE, e este aparelho pode ser o segundo da mesma pessoa.
    desc = (int)js_num(r, r + strlen(r), "descobrivel", 0.0) ? 1 : 0;
    js_texto_raiz(r, "id", id, sizeof id);
    // O CODIGO SEMPRE VEM NESTA RESPOSTA e o cliente o ignorava. Era o unico
    // ponto onde ele existe: nao ha rota para perguntar "qual e o meu codigo?"
    // depois, porque /v1/eu ja e ela.
    js_texto_raiz(r, "codigo", codigo, sizeof codigo);
    // DETECCAO DE RECURSO (F08): servidor com a migracao 008 diz "identidade1"
    // e lista as identidades ligadas. Servidor antigo: nenhum dos dois campos,
    // e o cliente segue exatamente como antes.
    { const char *e = js_array(r, NULL, "recursos");
      while (e && *e == '"') {
        char v[32] = "";
        if (js_cadeia(e, v, sizeof v) && !strcmp(v, "identidade1")) recurso = 1;
        e = js_prox(fimCadeia(e));
      } }
    { const char *e = js_array(r, NULL, "identidades");
      while (e && *e == '{') {
        const char *f = js_fim(e);
        char prov[16] = "", suj[64] = "";
        js_texto(e, f, "provedor", prov, sizeof prov);
        js_texto(e, f, "sujeito", suj, sizeof suj);
        if (!strcmp(prov, "trakt") && js_num(e, f, "verificado", 0.0) > 0 && suj[0])
          snprintf(slugLigado, sizeof slugLigado, "%s", suj);
        if (!strcmp(prov, "simkl") && js_num(e, f, "verificado", 0.0) > 0) simklLigado = 1;
        if (!strcmp(prov, "letterboxd") && suj[0]) snprintf(lbLigado, sizeof lbLigado, "%s", suj);
        e = js_prox(f);
      } }
  }
  free(r); r = NULL;
  if (!id[0]) {
    printf("[recomenda] /v1/eu nao respondeu (HTTP %d)\n", st);
    fflush(stdout);
    return 0;
  }
  // OUTRA PESSOA DESDE O ULTIMO ARRANQUE (#202). O ciclo so percebe a troca de
  // identidade NO MEIO da sessao; ligar a TV ja em outro perfil da casa, ou com
  // o Trakt no lugar da conta Nuvio, chegava aqui com o "Meu perfil" da pessoa
  // anterior no disco — e conciliarPerfil o republicava na identidade nova: o
  // mesmo apelido duas vezes na busca dos outros. O id gravado em REC_ARQ_EU e
  // de quem era; se o servidor diz outro, tudo o que esta aqui e dela. Mesma
  // regra da troca no meio da sessao: esquecer, e o proximo ciclo registra limpo.
  { int outra;
    if (!recTravarFio()) { free(r); return 0; }
    outra = meuId[0] && strcmp(meuId, id);
    recDestravarFio();
    if (outra) {
      printf("[recomenda] identidade mudou desde o ultimo arranque: dados sociais locais esquecidos\n");
      fflush(stdout);
      recomenda_esquecer();
      return 0;
    } }
  if (!recTravarFio()) { free(r); return 0; }
  snprintf(meuId, sizeof meuId, "%s", id);
  if (codigo[0]) snprintf(meuCodigo, sizeof meuCodigo, "%s", codigo);
  identRecurso = recurso;
  snprintf(identTrakt, sizeof identTrakt, "%s", slugLigado);
  identSimklLig = simklLigado;
  snprintf(identLbUsuario, sizeof identLbUsuario, "%s", lbLigado);
  registrado = 1;
  gravarEu();
  socNovoRegistrado(nome, exib, alc);
  recDestravarFio();
  avisarAlcance();
  if (!recTravarFio()) { free(r); return 0; }
  // RECONCILIACAO EM UM SO SENTIDO, e o sentido importa.
  //
  // Se este aparelho nunca perguntou e o servidor ja diz 1, a pessoa respondeu
  // SIM em outra TV: adotar isso e a resposta certa, e e o unico caminho pelo
  // qual `aparecer` vira SIM sem alguem apertar OK aqui. O contrario NAO vale:
  // servidor em 0 com este aparelho em "nao perguntado" continua "nao
  // perguntado", porque 0 e tambem o estado de quem nunca respondeu nada e
  // adota-lo como "nao" apagaria a pergunta sem ela ter sido feita.
  //
  // Quando este aparelho TEM uma resposta e o servidor discorda, quem manda e a
  // resposta daqui — ela e mais nova por construcao: ou foi dada nesta TV, ou
  // veio de um /v1/eu anterior.
  if (aparecer == REC_APARECER_NAO_PERGUNTADO) {
    if (desc) { aparecer = REC_APARECER_SIM; gravarAparecer(); }
  } else if (desc != (aparecer == REC_APARECER_SIM)) {
    aparecerPendente = (aparecer == REC_APARECER_SIM) ? 1 : 0;
  }
  recDestravarFio();
  printf("[recomenda] registrado como %s, codigo %s, descobrivel %d\n",
         id, codigo[0] ? codigo : "?", desc);
  fflush(stdout);
  return 1;
}

// Monta `{"slugs":[...]}` com quem o dono segue no Trakt, e devolve quantos
// entraram. 0 quando nao ha Trakt ligado ou a lista voltou vazia — e nesse caso
// `corpo` fica com um `{}` valido, porque as duas rotas que o usam aceitam
// corpo sem slugs (a de sugestoes ainda tem o ramo de amigo-de-amigo).
//
// A LISTA E CACHEADA POR REC_CONTATOS_MS. Ela e pedida por DUAS rotas agora
// (sugestoes e o vinculo manual do Trakt), e a aba Social pede um ciclo toda
// vez que abre: sem o cache, abrir e fechar o painel tres vezes custaria tres
// downloads de `users/me/following` ao Trakt, que tem limite de requisicao por
// aplicativo — nao por aparelho.
static char     fioSlugs[6000];
static int      fioSlugsN;
static Uint32   fioSlugsMs;
static int      fioSlugsTem;

static int corpoSlugsTrakt(char *corpo, size_t tam) {
  const char *tcab[4];
  char aut[3200], chave[160], *lista;
  const char *p;
  size_t k;
  int n = 0;
  if (!recTravarFio()) return 0;
  if (fioSlugsTem && (Sint32)(SDL_GetTicks() - fioSlugsMs) < 0) {
    snprintf(corpo, tam, "%s", fioSlugs);
    n = fioSlugsN;
    recDestravarFio();
    return n;
  }
  snprintf(corpo, tam, "{}");
  int ligado = trakt_ativo() && trakt_cabecalhos(tcab, aut, sizeof aut, chave, sizeof chave);
  recDestravarFio();
  if (!ligado) return 0;
  lista = recBaixarCom("https://api.trakt.tv/users/me/following", 10, tcab);
  if (!lista) return 0;
  k = (size_t)snprintf(corpo, tam, "{\"slugs\":[");
  p = strchr(lista, '[');
  p = p ? p + 1 : NULL;
  while (p && *p && n < 200 && k + 80 < tam) {
    const char *f, *u;
    char slug[96] = "";
    while (*p && (unsigned char)*p <= ' ') p++;
    if (*p != '{') break;
    f = js_fim(p);
    u = strstr(p, "\"user\"");
    if (u && u < f) {
      const char *ui = strchr(u, '{');
      if (ui) js_texto(ui, js_fim(ui), "slug", slug, sizeof slug);
    }
    if (slug[0]) {
      char esc[128];
      jsonEsc(esc, sizeof esc, slug);
      k += (size_t)snprintf(corpo + k, tam - k, "%s\"%s\"", n ? "," : "", esc);
      n++;
    }
    p = js_prox(f);
  }
  free(lista);
  snprintf(corpo + k, tam - k, "]}");
  if (!n) { snprintf(corpo, tam, "{}"); return 0; }
  if (!recTravarFio()) return 0;
  snprintf(fioSlugs, sizeof fioSlugs, "%s", corpo);
  fioSlugsN = n;
  fioSlugsTem = 1;
  fioSlugsMs = SDL_GetTicks() + REC_CONTATOS_MS;
  recDestravarFio();
  return n;
}

// POST /v1/contatos/trakt com os slugs de quem o dono segue.
//
// SO PELO BOTAO "PROCURAR AMIGOS DO TRAKT", e nao mais no primeiro ciclo. Ver a
// nota longa em ciclo().
static int vincularTrakt(const char **cab) {
  char corpo[6000];
  int n = corpoSlugsTrakt(corpo, sizeof corpo);
  if (!n) return 0;
  url("/v1/contatos/trakt");
  { int st = 0, vinculados = 0;
    char *r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
    // QUANTOS DELES JA USAM O SERVICO, que e a unica resposta que interessa a
    // tela: "2 seguidos oferecidos" e trabalho do cliente, "0 vinculados" e o
    // que a pessoa precisa ler para entender por que a lista continua vazia.
    if (r && st >= 200 && st < 300)
      vinculados = (int)js_num(r, r + strlen(r), "vinculados", 0.0);
    printf("[recomenda] %d seguidos do Trakt oferecidos, %d vinculados (HTTP %d)\n",
           n, vinculados, st);
    fflush(stdout);
    free(r);
    return vinculados; }
}

static void lerContatos(const char **cab) {
  char *r = NULL;
  const char *p;
  RecContato novos[REC_CONTATOS_MAX];
  int n = 0, st = 0;
  url("/v1/contatos");
  r = recBaixar(fioUrl, REC_TEMPO_REDE, cab, &st);
  if (!r || st < 200 || st >= 300) { free(r); r = NULL; return; }
  p = js_array(r, NULL, "contatos");
  while (p && *p == '{' && n < REC_CONTATOS_MAX) {
    const char *f = js_fim(p);
    memset(&novos[n], 0, sizeof novos[n]);
    js_texto(p, f, "id",     novos[n].id,     sizeof novos[n].id);
    js_texto(p, f, "nome",   novos[n].nome,   sizeof novos[n].nome);
    js_texto(p, f, "avatar", novos[n].avatar, sizeof novos[n].avatar);
    js_texto(p, f, "origem", novos[n].origem, sizeof novos[n].origem);
    js_texto(p, f, "apelido", novos[n].apelido, sizeof novos[n].apelido);
    js_texto(p, f, "selo",   novos[n].selo,   sizeof novos[n].selo);
    rec_contato_ids(p, f, &novos[n]);
    semTab(novos[n].nome); semTab(novos[n].apelido);
    // Contato sem nome nao e contato quebrado: no Trakt vira o slug, e o slug
    // e o que o dono reconhece; na conta Nuvio vira "Amigo #n" — o resto do id
    // e um UUID, e foi assim que um amigo por codigo aparecia na tela.
    // "Amigo #n" do servidor tambem nao e nome: com apelido escolhido, ele e o
    // que a linha mostra (o amigo escolheu ser chamado assim).
    if ((!novos[n].nome[0] || rec_nome_reserva(novos[n].nome)) && novos[n].apelido[0])
      snprintf(novos[n].nome, sizeof novos[n].nome, "%s", novos[n].apelido);
    if (!novos[n].nome[0] && novos[n].id[0])
      rec_nome_exibicao(novos[n].nome, sizeof novos[n].nome, "", novos[n].id);
    if (novos[n].id[0]) n++;
    p = js_prox(f);
  }
  free(r); r = NULL;
  if (!recTravarFio()) { free(r); return; }
  memcpy(contatos, novos, sizeof(RecContato) * (size_t)n);
  nContatos = n;
  recDestravarFio();
}

// POST /v1/descobrivel. So sai quando ha resposta a dar: `aparecerPendente` e
// -1 no caso comum e o ciclo inteiro custa um teste de inteiro.
//
// FALHA NAO DESFAZ A RESPOSTA NO DISCO, e por isso o pendente SO e limpo com o
// servidor confirmando. Com a rede fora, a escolha continua gravada aqui e o
// aviso sai no proximo ciclo; a pessoa nao e perguntada de novo e tambem nao
// fica com a tela dizendo "sim" enquanto o servidor pensa "nao".
static void enviarAparecer(const char **cab) {
  char corpo[48];
  char *r = NULL;
  int quer, st = 0;
  if (!recTravarFio()) { free(r); return; }
  quer = aparecerPendente;
  recDestravarFio();
  if (quer < 0) return;
  snprintf(corpo, sizeof corpo, "{\"descobrivel\":%d}", quer ? 1 : 0);
  url("/v1/descobrivel");
  r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
  printf("[recomenda] aparecer=%d HTTP %d\n", quer, st);
  fflush(stdout);
  free(r); r = NULL;
  if (st >= 200 && st < 300) {
    if (!recTravarFio()) { free(r); return; }
    // SO LIMPA SE NINGUEM MUDOU DE IDEIA NO MEIO. A pessoa pode ter apertado o
    // interruptor de novo enquanto este pedido estava no ar; limpar cegamente
    // perderia a segunda resposta, que e a que vale.
    if (aparecerPendente == quer) aparecerPendente = -1;
    recDestravarFio();
  }
}

// POST /v1/sugestoes. O corpo leva os slugs do Trakt (cacheados) e nada mais —
// o ramo de amigo-de-amigo e um JOIN do servidor sobre a tabela de contatos,
// que este aparelho nao tem e nao vai ter.
static void lerSugestoes(const char **cab) {
  char corpo[6000];
  char *r = NULL;
  const char *p;
  RecSugestao novos[REC_SUGESTOES_MAX];
  unsigned ger;
  int n = 0, st = 0;
  if (!recTravarFio()) { free(r); return; }
  ger = geracao;
  recDestravarFio();
  corpoSlugsTrakt(corpo, sizeof corpo);
  url("/v1/sugestoes");
  r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
  if (!r || st < 200 || st >= 300) { free(r); r = NULL; return; }
  p = js_array(r, NULL, "sugestoes");
  while (p && *p == '{' && n < REC_SUGESTOES_MAX) {
    const char *f = js_fim(p);
    memset(&novos[n], 0, sizeof novos[n]);
    js_texto(p, f, "id",      novos[n].id,      sizeof novos[n].id);
    js_texto(p, f, "nome",    novos[n].nome,    sizeof novos[n].nome);
    js_texto(p, f, "avatar",  novos[n].avatar,  sizeof novos[n].avatar);
    js_texto(p, f, "origem",  novos[n].origem,  sizeof novos[n].origem);
    js_texto(p, f, "viaNome", novos[n].viaNome, sizeof novos[n].viaNome);
    js_texto(p, f, "selo",    novos[n].selo,    sizeof novos[n].selo);
    semTab(novos[n].nome);
    semTab(novos[n].viaNome);
    // Sem nome, o slug. Mesma regra de lerContatos: quem nunca preencheu o
    // perfil no Trakt aparece so com o slug, e o slug e o que se reconhece.
    if (!novos[n].nome[0] && novos[n].id[0])
      rec_nome_exibicao(novos[n].nome, sizeof novos[n].nome, "", novos[n].id);
    if (novos[n].id[0]) n++;
    p = js_prox(f);
  }
  free(r); r = NULL;
  if (!recTravarFio()) { free(r); return; }
  // Saiu da conta enquanto isto estava no ar: sugestao de outra pessoa na tela
  // de quem acabou de entrar seria o pior tipo de vazamento deste recurso.
  if (ger == geracao) {
    memcpy(sugestoes, novos, sizeof(RecSugestao) * (size_t)n);
    nSugestoes = n;
  }
  recDestravarFio();
}

// POST /v1/contatos/sugerido. Devolve 1 quando vinculou — quem chama releia a
// lista de contatos depois.
static int adicionarSugerido(const char **cab, const char *id) {
  char corpo[6200], slugs[6000], esc[120];
  char *r;
  int st = 0, ok;
  jsonEsc(esc, sizeof esc, id);
  // OS SLUGS VAO JUNTO porque o servidor RECALCULA as sugestoes antes de
  // vincular, e sem eles o ramo do Trakt nao existe naquele recalculo — um
  // seguido do Trakt seria recusado com 403 na hora de adicionar, depois de ter
  // aparecido na tela. O ramo de amigo-de-amigo nao precisa de nada.
  corpoSlugsTrakt(slugs, sizeof slugs);
  { const char *interno = strchr(slugs, '[');
    if (interno) {
      const char *fim = strrchr(slugs, ']');
      size_t tam = fim && fim > interno ? (size_t)(fim - interno + 1) : 0;
      snprintf(corpo, sizeof corpo, "{\"id\":\"%s\",\"slugs\":%.*s}",
               esc, (int)tam, interno);
    } else {
      snprintf(corpo, sizeof corpo, "{\"id\":\"%s\"}", esc);
    } }
  url("/v1/contatos/sugerido");
  r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
  ok = r && st >= 200 && st < 300 && strstr(r, "\"ok\"") != NULL;
  printf("[recomenda] adicionar sugerido HTTP %d%s\n", st, ok ? "" : " -- falhou");
  fflush(stdout);
  free(r);
  return ok;
}

// Insere `r` na posicao certa por id DECRESCENTE. Chamar com o mutex TOMADO.
//
// NAO E "SEMPRE NO TOPO", e a diferenca aparece na primeira leitura: o servidor
// devolve a pagina em ordem DECRESCENTE (ORDER BY r.id DESC), entao empilhar
// cada item no topo entregaria a pagina invertida — a recomendacao mais velha
// em cima, que e exatamente a linha que ninguem quer ver primeiro.
//
// Duplicata (mesmo id) nao entra duas vezes: o cursor evita o caso comum, mas
// um cache de disco antigo com o cursor zerado nao.
static int inserir(const RecItem *r) {
  int i, k;
  for (i = 0; i < nItens; i++) if (itens[i].id == r->id) return 0;
  for (k = 0; k < nItens && itens[k].id > r->id; k++) { }
  // Lista cheia e o recem-chegado e mais velho que todos: ele nao entra. A que
  // sai por baixo quando ele entra e sempre a mais velha, e nao a ultima lida.
  if (k >= REC_MAX) return 0;
  if (nItens < REC_MAX) nItens++;
  if (k < nItens - 1)
    memmove(&itens[k + 1], &itens[k],
            sizeof(RecItem) * (size_t)(nItens - 1 - k));
  itens[k] = *r;
  return 1;
}

// Le UM item do JSON. Separada de lerRecs porque e a unica parte que o teste
// consegue exercitar sem rede — ver tests/recomenda.c.
static void lerItem(const char *p, const char *f, RecItem *r) {
  memset(r, 0, sizeof *r);
  r->id     = (long long)js_num(p, f, "id", 0.0);
  r->criado = (long long)js_num(p, f, "criado", 0.0);
  r->modelo = (int)js_num(p, f, "modelo", 0.0);
  r->nota   = (int)js_num(p, f, "nota", 0.0);
  r->visto  = (int)js_num(p, f, "visto", 0.0);
  // FORA DA FAIXA VIRA 0 e nao um selo com "25,5": o servidor ja recusa, mas o
  // cache em disco pode ter vindo de uma versao futura, e desenhar lixo e pior
  // que nao desenhar.
  if (r->nota < 0 || r->nota > 100) r->nota = 0;
  js_texto(p, f, "de",     r->de,     sizeof r->de);
  js_texto(p, f, "deNome", r->deNome, sizeof r->deNome);
  js_texto(p, f, "deAvatar", r->deAvatar, sizeof r->deAvatar);
  js_texto(p, f, "imdb",   r->imdb,   sizeof r->imdb);
  js_texto(p, f, "tipo",   r->tipo,   sizeof r->tipo);
  js_texto(p, f, "titulo", r->titulo, sizeof r->titulo);
  js_texto(p, f, "poster", r->poster, sizeof r->poster);
  js_texto(p, f, "ano",    r->ano,    sizeof r->ano);
  js_texto(p, f, "texto",  r->texto,  sizeof r->texto);
  semTab(r->deNome);
  semTab(r->deAvatar);
  semTab(r->titulo);
  semTab(r->texto);
  if (!r->tipo[0]) snprintf(r->tipo, sizeof r->tipo, "movie");
  if (!r->deNome[0] && r->de[0]) rec_nome_exibicao(r->deNome, sizeof r->deNome, "", r->de);
}

// O ESTADO "ASSISTIDA"/RESPOSTA QUE O SERVIDOR TEM (GET /v1/rec: campos
// terminou/reacao/resposta/respondido em cada item e o vetor `respostas` com o
// estado de TODAS as recs, ids velhos inclusive). Funde em recresp.c: o que a
// pessoa fez aqui e ainda nao foi enviado vence; fora isso o servidor acrescenta.
// Servidor antigo nao manda nada disto e nada acontece.
static void fundirRespostaDoServidor(const char *p, const char *f, unsigned ger) {
  long long id = (long long)js_num(p, f, "id", 0.0);
  int terminou = (int)js_num(p, f, "terminou", 0.0) > 0;
  int reacao = (int)js_num(p, f, "reacao", (double)RECRESP_SEM_REACAO);
  long long resp = (long long)js_num(p, f, "respondido", 0.0);
  char texto[RECRESP_TEXTO_MAX + 4] = "";
  if (id <= 0) return;
  js_texto(p, f, "resposta", texto, sizeof texto);
  SDL_LockMutex(mtx);
  if (ger == geracao) recresp_do_servidor(id, terminou, reacao, texto, resp);
  SDL_UnlockMutex(mtx);
}
static void fundirRespostasJson(const char *r, unsigned ger) {
  const char *p = js_array(r, NULL, "respostas");
  while (p && *p == '{') {
    const char *f = js_fim(p);
    fundirRespostaDoServidor(p, f, ger);
    p = js_prox(f);
  }
  p = js_array(r, NULL, "itens");
  while (p && *p == '{') {
    const char *f = js_fim(p);
    fundirRespostaDoServidor(p, f, ger);
    p = js_prox(f);
  }
}
// Para o teste (tests/recresp.c): o mesmo caminho da rede, sem rede.
void recomenda_fundir_respostas(const char *corpo) {
  unsigned ger;
  SDL_LockMutex(mtx); ger = geracao; SDL_UnlockMutex(mtx);
  fundirRespostasJson(corpo, ger);
}

// GET /v1/rec?desde=<cursor>, com If-None-Match. Devolve 1 quando falou com o
// servidor (inclusive no 304, que e a resposta NORMAL e nao uma falha).
static int lerRecs(const char **cab) {
  const char *cabs[5];
  char cabEtag[160];
  char etagNovo[96] = "";
  char *r = NULL;
  const char *p;
  long long desde;
  unsigned ger;
  int st = 0, novos = 0;

  if (!recTravarFio()) { free(r); return 1; }
  ger = geracao;
  desde = cursor;
  { int k = 0;
    while (k < 3 && cab[k]) { cabs[k] = cab[k]; k++; }
    cabs[k] = NULL; cabs[k + 1] = NULL;
    if (etagRec[0]) {
      snprintf(cabEtag, sizeof cabEtag, "If-None-Match: %s", etagRec);
      cabs[k] = cabEtag;
    } }
  recDestravarFio();

  // MONTADA EM DOIS PASSOS, e nao num snprintf so. "desde" e o nome do
  // parametro que o servidor espera, e tambem uma palavra em portugues: num
  // formato unico a varredura de i18n (tools/varredura-i18n.py) acusa esta URL
  // como frase nao traduzida. Separar o texto fixo do numero tira o falso
  // positivo sem inventar um nome de parametro que o servidor nao conhece.
  url("/v1/rec?desde=");
  { size_t k = strlen(fioUrl);
    snprintf(fioUrl + k, sizeof fioUrl - k, "%lld", desde); }
  r = recBaixarEtag(fioUrl, REC_TEMPO_REDE, cabs, &st,
                       etagNovo, sizeof etagNovo);
  recMarcarRede(st);
  // 304: nada mudou desde a ultima vez, e e o caso comum. Sem corpo, sem
  // trabalho, e o ETag guardado continua valendo.
  if (st == 304) { free(r); r = NULL; return 1; }
  if (!r || st < 200 || st >= 300) {
    free(r); r = NULL;
    return st != 0;
  }
  p = js_array(r, NULL, "itens");
  while (p && *p == '{') {
    const char *f = js_fim(p);
    RecItem it;
    lerItem(p, f, &it);
    if (it.id > 0 && it.imdb[0]) {
      if (!recTravarFio()) { free(r); return 1; }
      if (ger == geracao) {
        if (inserir(&it)) novos++;
        if (it.id > cursor) cursor = it.id;
      }
      recDestravarFio();
    }
    p = js_prox(f);
  }
  if (recSessaoFio() && !perfis_sessao_travar(recSessaoFio())) { free(r); return 1; }
  fundirRespostasJson(r, ger);
  if (recSessaoFio()) perfis_sessao_destravar();
  if (!recTravarFio()) { free(r); return 1; }
  // Saiu da conta enquanto isto estava no ar: nada do que voltou e desta
  // pessoa, e gravar seria desfazer o logout.
  if (ger != geracao) { recDestravarFio(); free(r); r = NULL; return 1; }
  snprintf(etagRec, sizeof etagRec, "%s", etagNovo);
  if (novos) gravar();
  gravarCursor();
  recDestravarFio();
  free(r); r = NULL;
  if (novos) {
    printf("[recomenda] %d nova(s); cursor %lld\n", novos, cursor);
    fflush(stdout);
  }
  return 1;
}

static void enviarFila(const char **cab) {
  char corpo[NV_ARTE_URL_MAX * 2 + 1600];
  char t[400], po[NV_ARTE_URL_MAX * 2], ti[64], an[48], pa[260], tx[200];
  char *r = NULL;
  int st = 0, ok;
  if (!recTravarFio()) { free(r); return; }
  if (!fila.cheia) { recDestravarFio(); return; }
  jsonEsc(t,  sizeof t,  fila.titulo);
  jsonEsc(po, sizeof po, fila.poster);
  jsonEsc(ti, sizeof ti, fila.tipo);
  jsonEsc(an, sizeof an, fila.ano);
  jsonEsc(pa, sizeof pa, fila.para);
  jsonEsc(tx, sizeof tx, fila.texto);
  snprintf(corpo, sizeof corpo,
           "{\"para\":\"%s\",\"imdb\":\"%s\",\"tipo\":\"%s\",\"titulo\":\"%s\","
           "\"poster\":\"%s\",\"ano\":\"%s\",\"modelo\":%d,\"nota\":%d,"
           "\"texto\":\"%s\"}",
           pa, fila.imdb, ti, t, po, an, fila.modelo, fila.nota, tx);
  fila.cheia = 0;
  envioEstado = REC_ENVIO_INDO;
  recDestravarFio();

  url("/v1/rec");
  r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
  ok = r && st >= 200 && st < 300 && strstr(r, "\"ok\"");
  printf("[recomenda] envio HTTP %d%s\n", st, ok ? "" : " -- falhou");
  fflush(stdout);
  free(r); r = NULL;
  if (!recTravarFio()) { free(r); return; }
  envioEstado = ok ? REC_ENVIO_OK : REC_ENVIO_FALHA;
  recDestravarFio();
}

// OS TRES PEDIDOS DE CONTATO, num ciclo so: vincular por codigo, remover
// alguem e revarrer os seguidos do Trakt.
//
// A LISTA E RELIDA DEPOIS DE CADA UM, e nao remendada aqui: o servidor e quem
// sabe o nome de quem entrou e a origem dele, e uma copia montada de memoria
// divergiria da lista de verdade na primeira diferenca de nome. Custa um GET
// que so acontece quando alguem apertou OK numa tela de amigos.
static void tratarContatos(const char **cab) {
  char codigo[16], remover[96], sugerido[96];
  int querTrakt, mudou = 0;

  if (!recTravarFio()) { return; }
  snprintf(codigo,   sizeof codigo,   "%s", vincCodigo);   vincCodigo[0] = 0;
  snprintf(remover,  sizeof remover,  "%s", removerId);    removerId[0] = 0;
  snprintf(sugerido, sizeof sugerido, "%s", sugAdicionar); sugAdicionar[0] = 0;
  querTrakt = pedirTrakt; pedirTrakt = 0;
  recDestravarFio();

  // ADICIONAR UMA SUGESTAO USA O MESMO ESTADO DE "VINCULAR POR CODIGO"
  // (REC_VINC_*), e nao um par de enums parecido: as duas acoes terminam do
  // mesmo jeito para quem esta olhando — "fulano virou contato" ou "nao deu" —
  // e duas maquinas de estado para uma frase so divergiriam na primeira
  // correcao, como o desenho da frase do modelo divergiu antes de rec_frase.
  if (sugerido[0]) {
    int ok = adicionarSugerido(cab, sugerido);
    if (!recTravarFio()) { return; }
    vincEstado = ok ? REC_VINC_OK : REC_VINC_FALHA;
    vincNome[0] = 0;
    // RELE AS SUGESTOES NO MESMO CICLO, e nao daqui a dez minutos. Quem entrou
    // como contato tem de sair da lista de sugeridos, e quem o servidor recusou
    // (a pessoa revogou entre a tela e o OK) tem de sair tambem.
    pedirSugestoes = 1;
    recDestravarFio();
    mudou = 1;
  }

  if (codigo[0]) {
    char corpo[64], nome[64] = "", *r;
    int st = 0, estado;
    snprintf(corpo, sizeof corpo, "{\"codigo\":\"%s\"}", codigo);
    url("/v1/contatos");
    r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
    if (st >= 200 && st < 300 && r) {
      // "nome" so existe dentro de "contato" nesta resposta, entao a primeira
      // ocorrencia e a certa.
      js_texto(r, r + strlen(r), "nome", nome, sizeof nome);
      estado = REC_VINC_OK;
      mudou = 1;
    } else if (st == 404) {
      estado = REC_VINC_NAO_ACHOU;
    } else if (st == 400) {
      // O SERVIDOR DA 400 PARA DOIS CASOS e o corpo e o unico jeito de separar:
      // "codigo invalido" (que recomenda_vincular ja impede de sair daqui) e
      // "esse codigo e seu". Dizer "codigo nao encontrado" para quem digitou o
      // proprio codigo manda a pessoa conferir uma coisa que esta certa.
      estado = (r && strstr(r, "seu")) ? REC_VINC_EU_MESMO : REC_VINC_FALHA;
    } else {
      estado = REC_VINC_FALHA;
    }
    printf("[recomenda] vincular por codigo HTTP %d\n", st);
    fflush(stdout);
    free(r);
    if (!recTravarFio()) { return; }
    vincEstado = estado;
    snprintf(vincNome, sizeof vincNome, "%s", nome);
    recDestravarFio();
  }

  if (remover[0]) {
    char corpo[160], esc[120];
    int st = 0;
    jsonEsc(esc, sizeof esc, remover);
    snprintf(corpo, sizeof corpo, "{\"id\":\"%s\"}", esc);
    url("/v1/contatos/remover");
    free(recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st));
    printf("[recomenda] remover contato HTTP %d\n", st);
    fflush(stdout);
    mudou = 1;
  }

  if (querTrakt) {
    int achados = 0, estado;
    if (!trakt_ativo()) {
      // SEM TRAKT NAO HA O QUE PROCURAR, e isto nao e falha: quem entrou so com
      // conta Nuvio nao tem lista de seguidos em lugar nenhum. A tela diz isso
      // em vez de girar para sempre.
      estado = REC_TRAKT_SEM_CONTA;
    } else {
      achados = vincularTrakt(cab);
      estado = REC_TRAKT_PRONTO;
      mudou = 1;
    }
    if (!recTravarFio()) { return; }
    traktEstado = estado;
    traktAchados = achados;
    recDestravarFio();
  }

  if (mudou) lerContatos(cab);
}

// POST /v1/rec/visto. O selo ja sumiu localmente quando a aba abriu; isto e so
// para os OUTROS aparelhos da mesma pessoa concordarem. Falhar aqui nao
// desfaz nada — a marca local e a que manda na tela.
static void confirmarVistas(const char **cab) {
  char corpo[900];
  size_t k;
  int i, n;
  long long copia[REC_MAX];
  if (!recTravarFio()) { return; }
  n = nVistoFila;
  for (i = 0; i < n; i++) copia[i] = vistoFila[i];
  nVistoFila = 0;
  recDestravarFio();
  if (n < 1) return;
  k = (size_t)snprintf(corpo, sizeof corpo, "{\"ids\":[");
  for (i = 0; i < n && k + 24 < sizeof corpo; i++)
    k += (size_t)snprintf(corpo + k, sizeof corpo - k, "%s%lld",
                          i ? "," : "", copia[i]);
  snprintf(corpo + k, sizeof corpo - k, "]}");
  url("/v1/rec/visto");
  free(recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, NULL));
}

// POST /v1/rec/resposta (recresp.h): "Ja assisti", gostei/nao gostei e a
// mensagem curta. Uma por ciclo de 200 ms basta — sao gestos de uma pessoa.
// 2xx confirma a versao enviada; 404 (rec que o servidor ja apagou pela
// retencao) tambem, para a linha nao ficar tentando para sempre. O resto fica
// pendente e sai no proximo ciclo.
static void enviarRespostas(const char **cab) {
  char corpo[256];
  long long rec;
  unsigned versao;
  int i, st;
  for (i = 0; i < 4; i++) {
    if (!recTravarFio()) return;
    int pendente = recresp_pendente(corpo, sizeof corpo, &rec, &versao);
    recDestravarFio();
    if (!pendente) return;
    st = 0;
    url("/v1/rec/resposta");
    free(recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st));
    printf("[recomenda] resposta rec %lld HTTP %d\n", rec, st);
    fflush(stdout);
    if (!recTravarFio()) return;
    if ((st >= 200 && st < 300) || st == 404) recresp_confirmar(rec, versao);
    recDestravarFio();
    if (!(st >= 200 && st < 300) && st != 404) break;
  }
}

// =============================================================================
// ENTRE AMIGOS ALEM DO TRAKT — rede (no fio, salvo recomenda_social_mesclar).
// =============================================================================

static unsigned mascaraGeneros(const char *p, const char *f) {
  const char *g = strstr(p, "\"generos\"");
  unsigned m = 0;
  if (!g || g >= f || !(g = strchr(g, '['))) return 0;
  for (g++; g < f && *g != ']'; g++) {
    if (*g == '"') {
      char id[24]; size_t k = 0; int i;
      for (g++; g < f && *g != '"' && k + 1 < sizeof id; g++) id[k++] = *g;
      id[k] = 0;
      for (i = 0; i < REC_GENEROS_N; i++) if (!strcmp(id, GENERO_ID[i])) m |= 1u << i;
      while (g < f && *g != '"') g++;
    }
  }
  return m;
}

static int lerPessoa(const char *p, const char *f, RecPessoa *x) {
  memset(x, 0, sizeof *x);
  js_texto(p, f, "pub",     x->pub,     sizeof x->pub);
  js_texto(p, f, "apelido", x->apelido, sizeof x->apelido);
  js_texto(p, f, "avatar",  x->avatar,  sizeof x->avatar);
  js_texto(p, f, "bio",     x->bio,     sizeof x->bio);
  js_texto(p, f, "relacao", x->relacao, sizeof x->relacao);
  js_texto(p, f, "vendo",   x->vendo,   sizeof x->vendo);
  js_texto(p, f, "nome",    x->nome,    sizeof x->nome);
  js_texto(p, f, "selo",    x->selo,    sizeof x->selo);
  semTab(x->vendo); semTab(x->nome);
  x->emComum = (int)js_num(p, f, "emComum", 0.0);
  x->generos = mascaraGeneros(p, f);
  {
    const char *m = strstr(p, "\"motivo\"");
    if (m && m < f) {
      char tp[16];
      js_texto(m, f, "tipo", tp, sizeof tp);
      x->motivo = !strcmp(tp, "titulos") ? REC_MOTIVO_TITULOS
                : !strcmp(tp, "generos") ? REC_MOTIVO_GENEROS
                : !strcmp(tp, "ativos")  ? REC_MOTIVO_ATIVOS : REC_MOTIVO_NENHUM;
      x->motivoGeneros = mascaraGeneros(m, f);
    }
  }
  semTab(x->apelido); semTab(x->bio); semTab(x->avatar);
  // Handle malformado ou sem nome: nao ha o que mostrar nem como agir.
  return x->pub[0] && (x->apelido[0] || x->nome[0]);
}

// POST /v1/perfil, /despublicar, /apagar e /atividade — o que o aparelho decidiu
// e o servidor ainda nao sabe. O pendente SO e limpo com o servidor confirmando
// E se ninguem mudou de ideia no meio (perfilVersao).
static void enviarPerfil(const char **cab) {
  int op, ativ, apagar, st = 0;
  unsigned ver;
  RecPerfil p;
  char corpo[512], *r = NULL;
  if (!recTravarFio()) { free(r); return; }
  op = perfilPendente; ativ = ativPendente; apagar = apagarPendente;
  ver = perfilVersao; p = perfil;
  recDestravarFio();
  if (apagar) {
    url("/v1/perfil/apagar");
    r = recPostar(fioUrl, REC_TEMPO_REDE, cab, "", &st);
    free(r); r = NULL;
    printf("[recomenda] apagar dados sociais HTTP %d\n", st); fflush(stdout);
    if (st >= 200 && st < 300) {
      if (!recTravarFio()) { free(r); return; } if (perfilVersao == ver) apagarPendente = 0; recDestravarFio();
    }
    return;
  }
  if (op == 1) {
    char ap[64], bi[220], gens[400] = "";
    size_t k = 0;
    int i;
    jsonEsc(ap, sizeof ap, p.apelido);
    jsonEsc(bi, sizeof bi, p.bio);
    for (i = 0; i < REC_GENEROS_N; i++)
      if (p.generos & (1u << i))
        k += (size_t)snprintf(gens + k, sizeof gens - k, "%s\"%s\"", k ? "," : "", GENERO_ID[i]);
    snprintf(corpo, sizeof corpo,
             "{\"apelido\":\"%s\",\"bio\":\"%s\",\"generos\":[%s],\"avatar\":%d,\"recentes\":%d}",
             ap, bi, gens, p.foto ? 1 : 0, p.recentes ? 1 : 0);
    url("/v1/perfil");
    r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
    free(r); r = NULL;
    printf("[recomenda] publicar perfil HTTP %d\n", st); fflush(stdout);
    if (st >= 200 && st < 300) {
      if (!recTravarFio()) { free(r); return; } if (perfilVersao == ver) perfilPendente = 0; recDestravarFio();
    }
  } else if (op == 2) {
    url("/v1/perfil/despublicar");
    r = recPostar(fioUrl, REC_TEMPO_REDE, cab, "", &st);
    free(r); r = NULL;
    printf("[recomenda] despublicar perfil HTTP %d\n", st); fflush(stdout);
    if (st >= 200 && st < 300) {
      if (!recTravarFio()) { free(r); return; } if (perfilVersao == ver) perfilPendente = 0; recDestravarFio();
    }
  }
  if (ativ >= 0) {
    snprintf(corpo, sizeof corpo, "{\"nivel\":%d}", ativ);
    url("/v1/perfil/atividade");
    r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
    free(r); r = NULL;
    printf("[recomenda] atividade nivel=%d HTTP %d\n", ativ, st); fflush(stdout);
    if (st >= 200 && st < 300) {
      if (!recTravarFio()) { free(r); return; } if (ativPendente == ativ) ativPendente = -1; recDestravarFio();
    }
  }
}

// GET /v1/perfil uma vez por arranque. O servidor guarda por IDENTIDADE, e esta
// TV pode ser a segunda da mesma pessoa (ou a mesma depois de um sair/entrar):
// quem nunca mexeu aqui ADOTA o que o servidor tem; quem ja mexeu manda o dela.
static void conciliarPerfil(const char **cab) {
  char *r = NULL;
  int st = 0;
  url("/v1/perfil");
  r = recBaixar(fioUrl, REC_TEMPO_REDE, cab, &st);
  if (r && st >= 200 && st < 300) {
    int pub = (int)js_num(r, r + strlen(r), "publicado", 0.0) ? 1 : 0;
    RecPerfil s;
    memset(&s, 0, sizeof s);
    s.publicado = pub;
    js_texto(r, r + strlen(r), "apelido", s.apelido, sizeof s.apelido);
    js_texto(r, r + strlen(r), "bio", s.bio, sizeof s.bio);
    s.generos = mascaraGeneros(r, r + strlen(r));
    s.foto = (int)js_num(r, r + strlen(r), "avatar", 0.0) ? 1 : 0;
    s.recentes = (int)js_num(r, r + strlen(r), "recentes", 0.0) ? 1 : 0;
    s.ativ = (int)js_num(r, r + strlen(r), "ativ", 0.0);
    if (s.ativ < 0 || s.ativ > 2) s.ativ = 0;
    if (!recTravarFio()) { free(r); return; }
    if (!perfilTocado) {
      limpaCurto(s.apelido, sizeof s.apelido, s.apelido, REC_APELIDO_MAX);
      limpaCurto(s.bio, sizeof s.bio, s.bio, REC_BIO_MAX);
      if (s.publicado && strlen(s.apelido) < 2) s.publicado = 0;
      perfil = s;
      if (s.publicado) { aparecer = REC_APARECER_SIM; gravarAparecer(); }
      gravarPerfil();
    } else if (!perfilPendente && !apagarPendente) {
      // Ja mexeu aqui e o servidor discorda: vale o daqui (mais novo por
      // construcao), reenviado no proximo ciclo.
      if (perfil.publicado != s.publicado) perfilPendente = perfil.publicado ? 1 : 2;
      if (perfil.ativ != s.ativ && ativPendente < 0) ativPendente = perfil.ativ;
    }
    recDestravarFio();
    printf("[recomenda] perfil no servidor: publicado %d, atividade %d\n", pub, s.ativ);
    fflush(stdout);
  }
  free(r); r = NULL;
}

static void enviarAtividade(const char **cab) {
  int n = 0;
  for (;;) {
    char corpo[560], t[340], ti[24], an[24], *r = NULL;
    int st = 0, nota, agora;
    char imdb[24], tipo[8], titulo[160], ano[8];
    if (!recTravarFio()) { free(r); return; }
    if (nAtivFila < 1 || (perfil.ativ < 1 && !perfil.recentes)) {
      nAtivFila = 0; recDestravarFio(); return;
    }
    snprintf(imdb, sizeof imdb, "%s", ativFila[0].imdb);
    snprintf(tipo, sizeof tipo, "%s", ativFila[0].tipo);
    snprintf(titulo, sizeof titulo, "%s", ativFila[0].titulo);
    snprintf(ano, sizeof ano, "%s", ativFila[0].ano);
    nota = ativFila[0].nota; agora = ativFila[0].agora;
    memmove(ativFila, ativFila + 1, sizeof ativFila[0] * (size_t)(--nAtivFila));
    recDestravarFio();
    jsonEsc(t, sizeof t, titulo);
    jsonEsc(ti, sizeof ti, tipo);
    jsonEsc(an, sizeof an, ano);
    snprintf(corpo, sizeof corpo,
             "{\"imdb\":\"%s\",\"tipo\":\"%s\",\"titulo\":\"%s\",\"ano\":\"%s\","
             "\"nota\":%d,\"agora\":%d}",
             imdb, ti, t, an, nota, agora ? 1 : 0);
    url("/v1/atividade");
    r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
    free(r); r = NULL;
    printf("[recomenda] atividade %s HTTP %d\n", agora ? "agora" : "assistiu", st);
    fflush(stdout);
    if (++n >= ATIV_FILA) return;
  }
}

// Os titulos que ESTA pessoa concluiu, do progresso LOCAL (prog_ler), so para
// perguntar "quem tem gosto parecido?". Vao no corpo do pedido e o servidor NAO
// os guarda; o pedido so sai quando ela abre a tela de sugestoes.
static int corpoGosto(char *corpo, size_t tam) {
  ProgRegistro *reg = (ProgRegistro *)malloc(sizeof *reg * 120);
  size_t k;
  int i, n = 0, achou;
  char vistos[40][24], idv[24];
  if (!reg) { snprintf(corpo, tam, "{\"imdbs\":[]}"); return 0; }
  achou = prog_ler(reg, 120);
  k = (size_t)snprintf(corpo, tam, "{\"imdbs\":[");
  for (i = 0; i < achou && n < 40 && k + 32 < tam; i++) {
    int j, dup = 0;
    if (reg[i].durSeg < 120.0 || reg[i].posSeg < reg[i].durSeg * 0.8) continue;
    if (strncmp(reg[i].contentId, "tt", 2)) continue;
    imdbServidor(idv, sizeof idv, reg[i].contentId);   // serie: "tt:S:E" -> "tt"
    for (j = 0; j < n; j++) if (!strcmp(vistos[j], idv)) dup = 1;
    if (dup) continue;
    snprintf(vistos[n], sizeof vistos[n], "%s", idv);
    k += (size_t)snprintf(corpo + k, tam - k, "%s\"%s\"", n ? "," : "", vistos[n]);
    n++;
  }
  // Salvos: so os que ainda nao entraram como concluidos (copiados sob mutex).
  int nFim = n;
  if (!recTravarFio()) { free(reg); return 0; }
  for (i = 0; i < nGostoSalvos && n < 60 && k + 32 < tam; i++) {
    int j, dup = 0;
    for (j = 0; j < nFim; j++) if (!strcmp(vistos[j], gostoSalvos[i])) dup = 1;
    if (dup) continue;
    k += (size_t)snprintf(corpo + k, tam - k, "%s\"%s\"", n ? "," : "", gostoSalvos[i]);
    n++;
  }
  recDestravarFio();
  snprintf(corpo + k, tam - k, "]}");
  free(reg);
  return n;
}

static int socEstadoDeStatus(int st) {
  if (st >= 200 && st < 300) return REC_SOC_OK;
  if (st == 404) return REC_SOC_NAO_ACHOU;
  if (st == 409) return REC_SOC_SEM_APELIDO;
  if (st == 429) return REC_SOC_LIMITE;
  return REC_SOC_FALHA;
}

// Aceitar/recusar e uma escolha ja confirmada, nao uma leitura de tela.
// A fila conserva a prova da pessoa no momento do OK; limpar a tela de A
// nao pode cancelar sua escolha nem reenvia-la como B. Um unico fio drena
// uma fila limitada, sem tokens em disco e sem trava durante a rede.
typedef struct RecPedidoAcao {
  struct RecPedidoAcao *proxima;
  uint64_t sessao;
  unsigned geracao, versao;
  int aceitar;
  char pub[16], aut[3200], via[32], perfil[40];
} RecPedidoAcao;
#define REC_PEDIDO_FILA_MAX 40
static pthread_mutex_t pedidoAcaoMtx = PTHREAD_MUTEX_INITIALIZER;
static RecPedidoAcao *pedidoAcaoInicio, *pedidoAcaoFim;
static int pedidoAcaoN, pedidoAcaoFio;

static int pedidoAcaoLaco(void *arg) {
  (void)arg;
  for (;;) {
    pthread_mutex_lock(&pedidoAcaoMtx);
    RecPedidoAcao *a = pedidoAcaoInicio;
    if (!a) {
      pedidoAcaoFio = 0;
      pthread_mutex_unlock(&pedidoAcaoMtx);
      return 0;
    }
    pedidoAcaoInicio = a->proxima;
    if (!pedidoAcaoInicio) pedidoAcaoFim = NULL;
    pthread_mutex_unlock(&pedidoAcaoMtx);
    const char *cab[4] = { a->aut, a->via, a->perfil[0] ? a->perfil : NULL, NULL };
    char caminho[600], corpo[64];
    int st = 0;
    snprintf(caminho, sizeof caminho, "%s/v1/pedidos/%s", NV_REC_URL,
             a->aceitar ? "aceitar" : "recusar");
    snprintf(corpo, sizeof corpo, "{\"pub\":\"%s\"}", a->pub);
    // Intencionalmente nao usa recPostar: esta acao pertence a identidade
    // copiada, mesmo que sua abertura ja tenha terminado.
    free(rede_postar_st(caminho, REC_TEMPO_REDE, cab, corpo, &st));
    if (perfis_sessao_travar(a->sessao)) {
      SDL_LockMutex(mtx);
      if (a->geracao == geracao) {
        int estado = socEstadoDeStatus(st);
        if (estado == REC_SOC_OK) {
          pedidosVersao++;
          int k = 0;
          for (int i = 0; i < nPedidos; i++)
            if (strcmp(pedidosRec[i].pub, a->pub)) pedidosRec[k++] = pedidosRec[i];
          nPedidos = k;
          if (a->aceitar) {
            for (int i = 0; i < nAchados; i++)
              if (!strcmp(achados[i].pub, a->pub))
                snprintf(achados[i].relacao, sizeof achados[i].relacao, "amigo");
            if (temCartao && !strcmp(cartaoP.pub, a->pub))
              snprintf(cartaoP.relacao, sizeof cartaoP.relacao, "amigo");
            contatosMs = SDL_GetTicks() - 1;
            contatosForcar = 1;
            pedidoAgora = 1;
          }
        }
        if (a->versao == socVersao) socEstado = estado;
      }
      SDL_UnlockMutex(mtx);
      perfis_sessao_destravar();
    }
    memset(a, 0, sizeof *a); free(a);
    pthread_mutex_lock(&pedidoAcaoMtx);
    pedidoAcaoN--;
    pthread_mutex_unlock(&pedidoAcaoMtx);
  }
}

int recomenda_responder_pedido(const char *pub, int aceitar) {
  if (!pubValido(pub)) return 0;
  RecPedidoAcao *a = calloc(1, sizeof *a);
  if (!a) return 0;
  a->sessao = perfis_sessao_geracao();
  if (!perfis_sessao_travar(a->sessao)) { free(a); return 0; }
  int ok = 0;
  if (recomenda_ativo()) {
    const char *tcab[4];
    char chave[160], usuario[80], jwt[3000];
    if (trakt_ativo() && trakt_cabecalhos(tcab, a->aut, sizeof a->aut,
                                        chave, sizeof chave)) {
      snprintf(a->via, sizeof a->via, "X-Nuvio-Auth: trakt");
      ok = 1;
    } else if (sessao_identidade_local_copiar(usuario, sizeof usuario, jwt, sizeof jwt)) {
      snprintf(a->aut, sizeof a->aut, "Authorization: Bearer %s", jwt);
      snprintf(a->via, sizeof a->via, "X-Nuvio-Auth: nuvio");
      perfilCab(a->perfil, sizeof a->perfil, 1);
      ok = 1;
    }
    memset(chave, 0, sizeof chave); memset(jwt, 0, sizeof jwt);
    if (ok) {
      if (!mtx) mtx = SDL_CreateMutex();
      if (!mtx) ok = 0;
      else {
        SDL_LockMutex(mtx);
        pthread_mutex_lock(&pedidoAcaoMtx);
        ok = pedidoAcaoN < REC_PEDIDO_FILA_MAX;
        if (ok) {
          a->geracao = geracao; a->versao = socVersao + 1;
          a->aceitar = !!aceitar; snprintf(a->pub, sizeof a->pub, "%s", pub);
          if (pedidoAcaoFim) pedidoAcaoFim->proxima = a;
          else pedidoAcaoInicio = a;
          pedidoAcaoFim = a; pedidoAcaoN++;
          if (!pedidoAcaoFio) {
            SDL_Thread *novo = SDL_CreateThread(pedidoAcaoLaco, "nv-pedido", NULL);
            if (novo) { pedidoAcaoFio = 1; SDL_DetachThread(novo); }
            else {
              pedidoAcaoInicio = pedidoAcaoFim = NULL; pedidoAcaoN--; ok = 0;
            }
          }
          if (ok) { socVersao = a->versao; socEstado = REC_SOC_INDO; socOp = 0; }
        }
        pthread_mutex_unlock(&pedidoAcaoMtx);
        SDL_UnlockMutex(mtx);
      }
    }
  }
  perfis_sessao_destravar();
  if (!ok) { memset(a, 0, sizeof *a); free(a); }
  return ok;
}

// GET /v1/pedidos: os pedidos de amizade que CHEGARAM. Devolve o HTTP.
static int lerPedidos(const char **cab) {
  char *r = NULL;
  int st = 0;
  unsigned ger, versao;
  if (!recTravarFio()) { free(r); return 0; }
  ger = geracao; versao = pedidosVersao; recDestravarFio();
  url("/v1/pedidos");
  r = recBaixar(fioUrl, REC_TEMPO_REDE, cab, &st);
  if (r && st >= 200 && st < 300) {
    const char *p = js_array(r, NULL, "recebidos");
    RecPessoa novos[REC_PEDIDOS_MAX];
    int n = 0;
    while (p && *p == '{' && n < REC_PEDIDOS_MAX) {
      const char *f = js_fim(p);
      if (lerPessoa(p, f, &novos[n])) n++;
      p = js_prox(f);
    }
    if (!recTravarFio()) { free(r); return 0; }
    if (ger == geracao && versao == pedidosVersao) {
      memcpy(pedidosRec, novos, sizeof(RecPessoa) * (size_t)n); nPedidos = n;
    }
    recDestravarFio();
  }
  free(r); r = NULL;
  return st;
}

// POST /v1/identidades/vincular | desvincular (F08). Unir pede as DUAS provas
// no mesmo pedido: o pedido sai autenticado pela identidade de sempre (o Trakt,
// quando ligado) e o corpo leva o token da OUTRA conta, que o servidor confere
// na hora contra o emissor e nao guarda. Depois de qualquer resposta boa o
// /v1/eu e refeito: e ele que diz o id canonico e o que ficou ligado.
static void tratarIdentidade(const char **cab) {
  static char corpo[3600], tok[3300];
  int op, prov, st = 0, res = REC_IDENT_OP_FALHA;
  char arg[40], *r = NULL;
  unsigned g;
  if (!recTravarFio()) { free(r); return; }
  op = identPedido; identPedido = 0; prov = identProv; g = geracao;
  snprintf(arg, sizeof arg, "%s", identArg);
  recDestravarFio();
  if (!op) return;
  corpo[0] = 0;
  if (prov == REC_IDENT_SIMKL || prov == REC_IDENT_LETTERBOXD) {
    // SIMKL: a prova e o token que o app ja guarda (Ajustes); o servidor o
    // confere em api.simkl.com e nao o guarda. NUNCA vai para o log.
    // LETTERBOXD: so o usuario, declarado.
    const char *nome = prov == REC_IDENT_SIMKL ? "simkl" : "letterboxd";
    if (op == 2) {
      snprintf(corpo, sizeof corpo, "{\"provedor\":\"%s\"}", nome);
      url("/v1/identidades/desvincular");
    } else if (prov == REC_IDENT_SIMKL) {
      if (simklauth_token()[0]) {
        jsonEsc(tok, sizeof tok, simklauth_token());
        snprintf(corpo, sizeof corpo, "{\"provedor\":\"simkl\",\"token\":\"%s\"}", tok);
      }
      url("/v1/identidades/vincular");
    } else {
      jsonEsc(tok, sizeof tok, arg);
      snprintf(corpo, sizeof corpo, "{\"provedor\":\"letterboxd\",\"usuario\":\"%s\"}", tok);
      url("/v1/identidades/vincular");
    }
    if (corpo[0]) {
      r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
      free(r); r = NULL;
      res = (st >= 200 && st < 300) ? REC_IDENT_OP_OK
          : st == 409 ? REC_IDENT_OP_CONFLITO
          : st == 501 ? REC_IDENT_OP_SEM_SERVICO
          : (st == 401 || st == 403) ? REC_IDENT_OP_RECUSADO : REC_IDENT_OP_FALHA;
    }
    memset(tok, 0, sizeof tok); memset(corpo, 0, sizeof corpo);
    printf("[recomenda] identidade %s %s HTTP %d\n", nome, op == 1 ? "ligar" : "desligar", st);
    fflush(stdout);
    if (!recTravarFio()) { free(r); return; }
    if (g == geracao) {
      identOpP[prov] = res;
      if (res == REC_IDENT_OP_SEM_SERVICO) identSimklOff = 1;
      if (res == REC_IDENT_OP_OK) {
        // Reflete na hora; o /v1/eu refeito logo abaixo confirma.
        if (prov == REC_IDENT_SIMKL) identSimklLig = op == 1;
        else snprintf(identLbUsuario, sizeof identLbUsuario, "%s", op == 1 ? arg : "");
        registrado = 0;
        pedidoAgora = 1;
      }
    }
    recDestravarFio();
    return;
  }
  if (op == 1) {
    if (!strcmp(fioVia, "X-Nuvio-Auth: trakt")) {
      // Pelo Trakt: a prova e a conta Nuvio, com o PERFIL ativo (o servidor
      // confere no Supabase que o indice e desta conta).
      const ContaPerfil *pf = perfis_item_ativo();
      int indice = (pf && !pf->primario && pf->indice >= 1) ? pf->indice : 0;
      char st0[3000];
      if (sessao_token_copiar(st0, sizeof st0)) {
        jsonEsc(tok, sizeof tok, st0);
        if (indice) snprintf(corpo, sizeof corpo, "{\"provedor\":\"nuvio\",\"token\":\"%s\",\"perfil\":%d}", tok, indice);
        else snprintf(corpo, sizeof corpo, "{\"provedor\":\"nuvio\",\"token\":\"%s\"}", tok);
      }
      memset(st0, 0, sizeof st0);
    } else {
      // Pela conta Nuvio: a prova e o token do Trakt.
      const char *tcab[4];
      char aut[3200], chave[160];
      if (trakt_ativo() && trakt_cabecalhos(tcab, aut, sizeof aut, chave, sizeof chave) &&
          !strncmp(aut, "Authorization: Bearer ", 22)) {
        jsonEsc(tok, sizeof tok, aut + 22);
        snprintf(corpo, sizeof corpo, "{\"provedor\":\"trakt\",\"token\":\"%s\"}", tok);
      }
    }
    if (corpo[0]) {
      url("/v1/identidades/vincular");
      r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
      free(r); r = NULL;
      res = (st >= 200 && st < 300) ? REC_IDENT_OP_OK : st == 409 ? REC_IDENT_OP_CONFLITO : REC_IDENT_OP_FALHA;
    }
  } else {
    url("/v1/identidades/desvincular");
    r = recPostar(fioUrl, REC_TEMPO_REDE, cab, "{\"provedor\":\"trakt\"}", &st);
    free(r); r = NULL;
    res = (st >= 200 && st < 300) ? REC_IDENT_OP_OK : REC_IDENT_OP_FALHA;
  }
  memset(tok, 0, sizeof tok); memset(corpo, 0, sizeof corpo);
  printf("[recomenda] identidade %s HTTP %d\n", op == 1 ? "unir" : "separar", st);
  fflush(stdout);
  if (!recTravarFio()) { free(r); return; }
  if (g == geracao) {
    identOp = res;
    if (res == REC_IDENT_OP_OK) {
      if (op == 2) identTrakt[0] = 0;
      registrado = 0;            // o proximo ciclo refaz /v1/eu: id canonico e ligacoes
      contatosMs = SDL_GetTicks(); // e relê contatos/feed ja fundidos
      pedidoAgora = 1;
    }
  }
  recDestravarFio();
}

static void tratarSocial(const char **cab) {
  int op, st = 0, estado;
  char arg[400], corpo[1200], esc[420], *r = NULL;
  unsigned ger, versao;
  if (!recTravarFio()) { free(r); return; }
  op = socOp; snprintf(arg, sizeof arg, "%s", socArg); socOp = 0; ger = geracao; versao = socVersao;
  recDestravarFio();
  if (!op) return;
  jsonEsc(esc, sizeof esc, arg);
  switch (op) {
    case SOC_BUSCAR:
      snprintf(corpo, sizeof corpo, "{\"q\":\"%s\"}", esc);
      url("/v1/perfis/buscar"); r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st); break;
    case SOC_GOSTO:
      corpoGosto(corpo, sizeof corpo);
      url("/v1/perfis/sugeridos"); r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st); break;
    case SOC_COMUNIDADE:
      snprintf(corpo, sizeof corpo, "{\"pagina\":%d}", atoi(arg));
      url("/v1/perfis/comunidade"); r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st); break;
    case SOC_VER:
      snprintf(corpo, sizeof corpo, "{\"pub\":\"%s\"}", esc);
      url("/v1/perfis/ver"); r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st); break;
    case SOC_PEDIR: case SOC_ACEITAR: case SOC_RECUSAR: case SOC_CANCELAR: case SOC_DESBLOQ:
      snprintf(corpo, sizeof corpo, "{\"pub\":\"%s\"}", esc);
      url(op == SOC_PEDIR ? "/v1/pedidos/enviar" : op == SOC_ACEITAR ? "/v1/pedidos/aceitar"
          : op == SOC_RECUSAR ? "/v1/pedidos/recusar" : op == SOC_CANCELAR ? "/v1/pedidos/cancelar"
          : "/v1/desbloquear");
      r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st); break;
    case SOC_BLOQUEAR: {
      char e2[420];
      jsonEsc(e2, sizeof e2, arg + 2);
      snprintf(corpo, sizeof corpo, "{\"%s\":\"%s\"}", arg[0] == 'p' ? "pub" : "id", e2);
      url("/v1/bloquear"); r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st); break; }
    case SOC_PEDIDOS:
      st = lerPedidos(cab); break;
    case SOC_BLOQUEADOS:
      url("/v1/bloqueados"); r = recBaixar(fioUrl, REC_TEMPO_REDE, cab, &st); break;
    default: break;
  }
  estado = socEstadoDeStatus(st);
  printf("[recomenda] social op=%d HTTP %d\n", op, st); fflush(stdout);
  if (!recTravarFio()) { free(r); return; }
  if (ger != geracao || versao != socVersao) { recDestravarFio(); free(r); r = NULL; return; }
  if (estado == REC_SOC_OK && r) {
    const char *fim = r + strlen(r);
    if (op == SOC_BUSCAR || op == SOC_GOSTO) {
      const char *p = js_array(r, NULL, op == SOC_BUSCAR ? "resultados" : "sugeridos");
      int n = 0;
      while (p && *p == '{' && n < REC_BUSCA_MAX) {
        const char *f = js_fim(p);
        if (lerPessoa(p, f, &achados[n])) n++;
        p = js_prox(f);
      }
      nAchados = n;
      achadosOrigem = op == SOC_BUSCAR ? 1 : 2;
    } else if (op == SOC_COMUNIDADE) {
      // Pagina 0 recomeca; as outras ACRESCENTAM, sem repetir quem ja esta (a
      // ordem e por atividade e pode ter mudado entre uma pagina e outra).
      const char *p = js_array(r, NULL, "pessoas");
      int pag = atoi(arg), n = (pag == 0 || achadosOrigem != 3) ? 0 : nAchados;
      while (p && *p == '{' && n < REC_COMUNIDADE_MAX) {
        const char *f = js_fim(p);
        if (lerPessoa(p, f, &achados[n])) {
          int j, dup = 0;
          for (j = 0; j < n; j++) if (!strcmp(achados[j].pub, achados[n].pub)) dup = 1;
          if (!dup) n++;
        }
        p = js_prox(f);
      }
      nAchados = n;
      achadosOrigem = 3;
      comPagina = pag;
      comFechada = (int)js_num(r, fim, "fechado", 0.0) ? 1 : 0;
      comMais = (int)js_num(r, fim, "mais", 0.0) && n < REC_COMUNIDADE_MAX ? 1 : 0;
    } else if (op == SOC_VER) {
      if (lerPessoa(r, fim, &cartaoP)) {
        const char *p = js_array(r, NULL, "recentes");
        temCartao = 1; nCartaoRec = 0;
        while (p && *p == '{' && nCartaoRec < 6) {
          const char *f = js_fim(p);
          js_texto(p, f, "titulo", cartaoRec[nCartaoRec], sizeof cartaoRec[0]);
          semTab(cartaoRec[nCartaoRec]);
          if (cartaoRec[nCartaoRec][0]) nCartaoRec++;
          p = js_prox(f);
        }
      } else estado = REC_SOC_FALHA;
    } else if (op == SOC_BLOQUEADOS) {
      const char *p = js_array(r, NULL, "bloqueados");
      int n = 0;
      while (p && *p == '{' && n < REC_BLOQ_MAX) {
        const char *f = js_fim(p);
        js_texto(p, f, "pub", bloq[n].pub, sizeof bloq[n].pub);
        js_texto(p, f, "nome", bloq[n].nome, sizeof bloq[n].nome);
        semTab(bloq[n].nome);
        if (bloq[n].pub[0]) n++;
        p = js_prox(f);
      }
      nBloq = n;
    } else if (op == SOC_RECUSAR) {
      int i, k = 0;   // o pedido tratado sai da caixa na hora
      for (i = 0; i < nPedidos; i++)
        if (strcmp(pedidosRec[i].pub, arg)) pedidosRec[k++] = pedidosRec[i];
      nPedidos = k;
    } else if (op == SOC_PEDIR || op == SOC_CANCELAR || op == SOC_ACEITAR) {
      // A LISTA DE ONDE O CARTAO VEIO tambem fica sabendo: voltar do cartao
      // para a comunidade e ver "Pedir" de novo ao lado de quem ja recebeu o
      // pedido leria como se o OK nao tivesse feito nada.
      char est[12] = "";
      int i;
      if (op == SOC_PEDIR) js_texto(r, fim, "estado", est, sizeof est);
      else if (op == SOC_ACEITAR) snprintf(est, sizeof est, "%s", "amigo");
      for (i = 0; i < nAchados; i++)
        if (!strcmp(achados[i].pub, arg)) snprintf(achados[i].relacao, sizeof achados[i].relacao, "%s", est);
      // ACEITAR TAMBEM TIRA DA CAIXA. Este ramo e o de cima eram um if/else e
      // o aceite caia so no de cima: a pessoa saia dos pedidos, mas a lista de
      // onde ela foi aceita (busca, comunidade) continuava oferecendo "Aceitar".
      if (op == SOC_ACEITAR) {
        int k = 0;
        for (i = 0; i < nPedidos; i++)
          if (strcmp(pedidosRec[i].pub, arg)) pedidosRec[k++] = pedidosRec[i];
        nPedidos = k;
      }
      if (temCartao && !strcmp(cartaoP.pub, arg) && est[0])
        snprintf(cartaoP.relacao, sizeof cartaoP.relacao, "%s", est);
    } else if (op == SOC_BLOQUEAR) {
      // A amizade (se havia) caiu no servidor: a lista de contatos e relida.
      contatosMs = SDL_GetTicks() - 1;
    }
  }
  socEstado = estado;
  recDestravarFio();
  free(r); r = NULL;
  // Aceitar/bloquear muda quem e contato.
  if (estado == REC_SOC_OK && (op == SOC_ACEITAR || op == SOC_BLOQUEAR || op == SOC_PEDIR))
    lerContatos(cab);
}

// ---------------------------------------------------------------------------
// A FILEIRA "ENTRE AMIGOS": atividade dos amigos Nuvio.
// ---------------------------------------------------------------------------

// Identidade em buffers do CHAMADOR. identidade() usa os estaticos do fio de
// rede; esta e chamada pela descoberta, em outro fio, e nao pode pisar neles.
static int identidadeEm(const char **cab, char *aut, size_t na, char *via, size_t nv) {
  if (perfis_precisa_escolher()) return 0;
  const char *tcab[4];
  char chave[160];
  if (trakt_ativo() && trakt_cabecalhos(tcab, aut, na, chave, sizeof chave)) {
    snprintf(via, nv, "X-Nuvio-Auth: trakt");
  } else if (bearerSessaoLocal(aut, na)) {
    snprintf(via, nv, "X-Nuvio-Auth: nuvio");
  } else return 0;
  cab[0] = aut; cab[1] = via; cab[2] = NULL; cab[3] = NULL;
  { RecFioContexto *ctx = recFioContexto();
    if (!ctx) return 0;
    char *pf = ctx->perfilCab;
    if (perfilCab(pf, sizeof ctx->perfilCab, !strcmp(via, "X-Nuvio-Auth: nuvio"))) cab[2] = pf; }
  return 1;
}

// O PARSE DO FEED, separado da rede para o teste exercita-lo com o JSON que o
// Worker emite de verdade. `r` e o corpo de GET /v1/amigos/atividade.
static int lerFeedCorpo(const char *r, CatItem *saida, int max) {
  const char *p;
  int n = 0;
  SDL_LockMutex(mtx);
  nFeed = 0;
  SDL_UnlockMutex(mtx);
  p = js_array(r, NULL, "itens");
  while (p && *p == '{' && n < max) {
    const char *f = js_fim(p);
    CatItem *d = &saida[n];
    char nome[96] = "", tipo[8] = "", id[96] = "";
    int agora;
    long long criado;
    memset(d, 0, sizeof *d);
    js_texto(p, f, "imdb",   d->imdb,   sizeof d->imdb);
    js_texto(p, f, "titulo", d->titulo, sizeof d->titulo);
    js_texto(p, f, "tipo",   tipo,      sizeof tipo);
    js_texto(p, f, "de",     id,        sizeof id);
    js_texto(p, f, "deNome", nome,      sizeof nome);
    js_texto(p, f, "deAvatar", d->socialAvatar, sizeof d->socialAvatar);
    d->nota = (int)js_num(p, f, "nota", 0.0);
    if (d->nota < 0 || d->nota > 100) d->nota = 0;
    agora = (int)js_num(p, f, "agora", 0.0);
    criado = (long long)js_num(p, f, "criado", 0.0);
    semTab(d->titulo); semTab(nome);
    if (!d->imdb[0] || strncmp(d->imdb, "tt", 2) || !id[0]) { p = js_prox(f); continue; }
    snprintf(d->tipo, sizeof d->tipo, "%s", !strcmp(tipo, "series") ? "series" : "movie");
    // O nome e o APELIDO que o amigo escolheu (o servidor manda o apelido, com
    // o nome da conta so como reserva). Sem nome algum, "Amigo".
    rec_nome_exibicao(d->socialNome, sizeof d->socialNome, nome, id);
    snprintf(d->pais, sizeof d->pais, "%s", d->socialNome);
    // O id do amigo vai em socialSlug com o prefixo do servico ("nuvio:..."),
    // que e o que separa o amigo Nuvio do slug do Trakt (slugValido nao aceita
    // ':'). social.c usa isso para abrir a atividade dele SEM o Trakt.
    snprintf(d->socialSlug, sizeof d->socialSlug, "%s", id);
    snprintf(d->socialAcao, sizeof d->socialAcao, "%s",
             agora ? "assistindo agora" : "assistiu");
    snprintf(d->provNome, sizeof d->provNome, "%s", d->socialAcao);
    snprintf(d->direcao, sizeof d->direcao, "%s",
             !strcmp(d->tipo, "series") ? "Série" : "Filme");
    d->retomadoMs = criado * 1000LL;
    SDL_LockMutex(mtx);
    if (nFeed < FEED_MAX) {
      snprintf(feed[nFeed].de, sizeof feed[0].de, "%s", id);
      snprintf(feed[nFeed].imdb, sizeof feed[0].imdb, "%s", d->imdb);
      snprintf(feed[nFeed].titulo, sizeof feed[0].titulo, "%s", d->titulo);
      snprintf(feed[nFeed].tipo, sizeof feed[0].tipo, "%s", d->tipo);
      feed[nFeed].agora = agora; feed[nFeed].criado = criado;
      nFeed++;
    }
    SDL_UnlockMutex(mtx);
    n++;
    p = js_prox(f);
  }
  return n;
}

static int lerFeedAmigos(CatItem *saida, int max) {
  const char *cab[4];
  char aut[3200], via[32], url_[300], *r;
  int st = 0, n;
  if (!perfis_sessao_atual(recSessaoFio())) return 0;
  recAtualizarToken();
  if (!perfis_sessao_travar(recSessaoFio())) return 0;
  int ident = identidadeEm(cab, aut, sizeof aut, via, sizeof via);
  perfis_sessao_destravar();
  if (!ident) return 0;
  snprintf(url_, sizeof url_, "%s%s", NV_REC_URL, "/v1/amigos/atividade");
  r = recBaixar(url_, REC_TEMPO_REDE, cab, &st);
  if (!r || st < 200 || st >= 300) { free(r); return 0; }
  if (!perfis_sessao_travar(recSessaoFio())) { free(r); return 0; }
  n = lerFeedCorpo(r, saida, max);
  perfis_sessao_destravar();
  free(r);
  return n;
}

// A REGRA DE UNIAO, separada da rede para o teste. Ordem: quem esta assistindo
// AGORA primeiro; depois as duas fontes se alternam (Trakt, Nuvio, Trakt...).
// Cada titulo (imdb) entra UMA vez — o primeiro que aparece, que e o mais
// recente de cada fonte. Nada passa de `max`.
static int jaTem(const CatItem *v, int n, const char *imdb) {
  int k;
  for (k = 0; k < n; k++) if (!strcmp(v[k].imdb, imdb)) return 1;
  return 0;
}

int rec_social_unir(CatItem *itens, int nTrakt, const CatItem *nuvio, int nNuvio, int max) {
  CatItem *tmp;
  int i = 0, j = 0, n = 0;
  if (nNuvio <= 0) return nTrakt;
  if (max < 1) return 0;
  tmp = (CatItem *)malloc(sizeof *tmp * (size_t)max);
  if (!tmp) return nTrakt;
  for (j = 0; j < nNuvio && n < max; j++)
    if (!strcmp(nuvio[j].socialAcao, "assistindo agora") && !jaTem(tmp, n, nuvio[j].imdb))
      tmp[n++] = nuvio[j];
  i = 0; j = 0;
  while (n < max && (i < nTrakt || j < nNuvio)) {
    if (i < nTrakt) { if (!jaTem(tmp, n, itens[i].imdb)) tmp[n++] = itens[i]; i++; }
    if (n < max && j < nNuvio) { if (!jaTem(tmp, n, nuvio[j].imdb)) tmp[n++] = nuvio[j]; j++; }
  }
  memcpy(itens, tmp, sizeof *tmp * (size_t)n);
  free(tmp);
  return n;
}

int recomenda_social_mesclar_sessao(CatItem *itens, int nTrakt, int max, uint64_t sessao) {
  CatItem *nu;
  int n;
  uint64_t anterior = recSessaoFio();
  if (!perfis_sessao_atual(sessao)) return 0;
  if (!recomenda_ativo() || max < 1) return nTrakt;
  if (!mtx) mtx = SDL_CreateMutex();
  nu = (CatItem *)calloc((size_t)max, sizeof *nu);
  if (!nu) return nTrakt;
  if (!recSessaoDefinir(sessao)) { free(nu); return 0; }
  n = lerFeedAmigos(nu, max);
  if (n > 0 && perfis_sessao_atual(sessao)) n = trakt_enfeitar_lote_sessao(nu, n, sessao);
  if (perfis_sessao_travar(sessao)) {
    n = rec_social_unir(itens, nTrakt, nu, n, max);
    perfis_sessao_destravar();
  } else n = 0;
  printf("[recomenda] entre amigos: %d do Trakt + Nuvio = %d\n", nTrakt, n);
  fflush(stdout);
  free(nu);
  recSessaoDefinir(anterior);
  return n;
}

int recomenda_social_mesclar(CatItem *itens, int nTrakt, int max) {
  return recomenda_social_mesclar_sessao(itens, nTrakt, max, perfis_sessao_geracao());
}

int recomenda_amigo_atividades(const char *id, RecAtivAmigo *saida, int max) {
  int i, n = 0;
  if (!recomenda_ativo() || !mtx || !id || !id[0] || !saida) return 0;
  SDL_LockMutex(mtx);
  for (i = 0; i < nFeed && n < max; i++)
    if (!strcmp(feed[i].de, id)) {
      snprintf(saida[n].imdb, sizeof saida[n].imdb, "%s", feed[i].imdb);
      snprintf(saida[n].titulo, sizeof saida[n].titulo, "%s", feed[i].titulo);
      snprintf(saida[n].tipo, sizeof saida[n].tipo, "%s", feed[i].tipo);
      saida[n].agora = feed[i].agora;
      saida[n].criado = feed[i].criado;
      n++;
    }
  SDL_UnlockMutex(mtx);
  return n;
}

// =============================================================================
// REDESENHO DO SOCIAL (02/10/2026): nome de exibicao, nivel de atividade
// (alcance), eventos do player, feed do nosso servidor + merge com o Trakt e o
// perfil do amigo. Contrato: docs/social-contrato.md. API: recomenda.h.
// =============================================================================

// A RESPOSTA SOBRE O ALCANCE mora em arquivo proprio pela mesma razao de
// REC_ARQ_APARECER: e uma escolha da pessoa, nao um dado do servidor.
#define REC_ARQ_ALCANCE recArquivo("recomendacoes-alcance.txt")
// "etag\n" + o corpo cru de GET /v1/feed. O mesmo parse serve disco e rede.
#define REC_ARQ_FEED    recArquivo("recomendacoes-feed.json")
// "id\n" + o corpo cru do ultimo GET /v1/amigo.
#define REC_ARQ_AMIGO   recArquivo("recomendacoes-amigo.json")
#define ATIVN_FILA 16

static char meuNome[64], minhaExib[40];
static int  alcance = REC_ALCANCE_NAO_PERGUNTADO;
static int  alcancePendente = -2;            // -2 = nada a dizer ao servidor
static char nomePendente[40];
static int  temNomePendente;
static RecAtiv ativN[ATIVN_FILA];
static int  nAtivN;
static RecEvento feedN[REC_FEED_MAX];
static int  nFeedN;
static char etagFeed[96];
static int  pedirFeed;
static RecAmigo amigo;
static int  temAmigo, amigoEstado, amigoBuscar;
static char amigoPedido[96];

void rec_nome_exibicao(char *dst, size_t tam, const char *nome, const char *id) {
  unsigned h = 2166136261u;
  const char *c;
  if (!dst || !tam) return;
  if (nome && nome[0]) { snprintf(dst, tam, "%s", nome); return; }
  if (id && !strncmp(id, "trakt:", 6) && id[6]) { snprintf(dst, tam, "%s", id + 6); return; }
  // "Amigo #n" com n de 100 a 999 derivado do id: estavel entre arranques e
  // curto. O servidor novo ja manda "Amigo #<rowid>" no nome; isto so cobre a
  // resposta de um servidor velho ou um cache antigo.
  for (c = id ? id : ""; *c; c++) { h ^= (unsigned char)*c; h *= 16777619u; }
  snprintf(dst, tam, i18n("Amigo #%d"), (int)(h % 900u) + 100);
}

static int acaoDeEv(const char *ev) {
  if (!strcmp(ev, "inicio"))   return REC_ACAO_INICIO;
  if (!strcmp(ev, "fim"))      return REC_ACAO_FIM;
  if (!strcmp(ev, "abandono")) return REC_ACAO_ABANDONO;
  if (!strcmp(ev, "reacao"))   return REC_ACAO_REACAO;
  if (!strcmp(ev, "salvo"))    return REC_ACAO_SALVO;
  return 0;
}

// Le os campos de UM evento do servidor em [p,f). 0 se a linha nao serve.
static int lerEvento(const char *p, const char *f, RecEvento *e) {
  char ev[16] = "", nome[96] = "";
  memset(e, 0, sizeof *e);
  e->fonte = REC_FONTE_NUVIO;
  e->id = (long long)js_num(p, f, "id", 0.0);
  js_texto(p, f, "ev", ev, sizeof ev);
  js_texto(p, f, "de", e->pessoa, sizeof e->pessoa);
  js_texto(p, f, "deNome", nome, sizeof nome);
  js_texto(p, f, "deAvatar", e->pessoaAvatar, sizeof e->pessoaAvatar);
  js_texto(p, f, "via", e->via, sizeof e->via);
  js_texto(p, f, "imdb", e->imdb, sizeof e->imdb);
  js_texto(p, f, "midia", e->midia, sizeof e->midia);
  js_texto(p, f, "titulo", e->titulo, sizeof e->titulo);
  js_texto(p, f, "poster", e->poster, sizeof e->poster);
  e->grau = (int)js_num(p, f, "grau", 1.0);
  e->temporada = (int)js_num(p, f, "temporada", 0.0);
  e->episodio = (int)js_num(p, f, "episodio", 0.0);
  e->pct = (int)js_num(p, f, "pct", 0.0);
  e->reacao = (int)js_num(p, f, "reacao", 0.0);
  e->quando = (long long)js_num(p, f, "criado", 0.0);
  // O servidor agrupado (06/10/2026): episodios contados e a reacao junto do
  // "terminou". Servidor antigo nao manda: 0 e "nenhuma".
  e->eps = (int)js_num(p, f, "eps", 0.0);
  if (e->eps < 0 || e->eps > 9999) e->eps = 0;
  e->reacFim = (int)js_num(p, f, "reac", -2.0);
  e->temReacFim = e->reacFim >= -1 && e->reacFim <= 1;
  if (!e->temReacFim) e->reacFim = 0;
  semTab(nome); semTab(e->titulo); semTab(e->via);
  e->acao = acaoDeEv(ev);
  if (!e->acao || strncmp(e->imdb, "tt", 2) || !e->pessoa[0]) return 0;
  if (strcmp(e->midia, "series")) snprintf(e->midia, sizeof e->midia, "movie");
  if (e->reacao < -1 || e->reacao > 1) e->reacao = 0;
  if (e->grau != 2) e->grau = 1;
  rec_nome_exibicao(e->pessoaNome, sizeof e->pessoaNome, nome, e->pessoa);
  return 1;
}

static int feedParse(const char *r, RecEvento *saida, int max) {
  const char *p = r ? js_array(r, NULL, "itens") : NULL;
  int n = 0;
  while (p && *p == '{' && n < max) {
    const char *f = js_fim(p);
    if (lerEvento(p, f, &saida[n])) n++;
    p = js_prox(f);
  }
  return n;
}

// Objeto de nome `chave` (o primeiro), ou NULL (ausente ou null).
static const char *objeto(const char *r, const char *chave, const char **fim) {
  char k[40];
  const char *p;
  snprintf(k, sizeof k, "\"%s\":", chave);
  p = strstr(r, k);
  if (!p) return NULL;
  p += strlen(k);
  while (*p == ' ') p++;
  if (*p != '{') return NULL;
  *fim = js_fim(p);
  return *fim ? p : NULL;
}

// O mesmo, procurando so em [ini, fim).
static const char *objetoEm(const char *ini, const char *fim, const char *chave, const char **f) {
  char k[40];
  const char *p = ini;
  size_t lk;
  snprintf(k, sizeof k, "\"%s\":", chave);
  lk = strlen(k);
  while ((p = strstr(p, k)) && p < fim) {
    const char *v = p + lk;
    while (*v == ' ') v++;
    if (*v == '{') { *f = js_fim(v); return (*f && *f <= fim) ? v : NULL; }
    p = v;
  }
  return NULL;
}

static int estadoRecDe(const char *s) {
  if (!strcmp(s, "reacao"))   return REC_REC_REAGIU;
  if (!strcmp(s, "terminou")) return REC_REC_TERMINOU;
  if (!strcmp(s, "comecou"))  return REC_REC_COMECOU;
  if (!strcmp(s, "aberta"))   return REC_REC_ABERTA;
  return REC_REC_ENTREGUE;
}

// O parse de GET /v1/amigo. Separado da rede para o teste.
static int amigoParse(const char *r, RecAmigo *a) {
  const char *o, *of, *p;
  char nome[96] = "";
  memset(a, 0, sizeof *a);
  if (!r || !js_texto_raiz(r, "id", a->id, sizeof a->id) || !a->id[0]) return 0;
  js_texto_raiz(r, "nome", nome, sizeof nome);
  js_texto_raiz(r, "avatar", a->avatar, sizeof a->avatar);
  js_texto_raiz(r, "via", a->via, sizeof a->via);
  js_texto_raiz(r, "origem", a->origem, sizeof a->origem);
  semTab(nome); semTab(a->via);
  rec_nome_exibicao(a->nome, sizeof a->nome, nome, a->id);
  a->grau = (int)js_num(r, NULL, "grau", 1.0) == 2 ? 2 : 1;
  a->desde = (long long)js_num(r, NULL, "desde", 0.0);
  a->compartilha = (int)js_num(r, NULL, "compartilha", 0.0) ? 1 : 0;
  if ((o = objeto(r, "mes", &of))) {
    a->temMes = 1;
    js_texto(o, of, "mes", a->mes, sizeof a->mes);
    a->seg = (long long)js_num(o, of, "seg", 0.0);
    a->filmes = (int)js_num(o, of, "filmes", 0.0);
    a->series = (int)js_num(o, of, "series", 0.0);
  }
  if ((o = objeto(r, "agora", &of))) {
    RecEvento *e = &a->agora;
    a->temAgora = 1;
    e->fonte = REC_FONTE_NUVIO; e->acao = REC_ACAO_INICIO; e->grau = a->grau;
    snprintf(e->pessoa, sizeof e->pessoa, "%s", a->id);
    snprintf(e->pessoaNome, sizeof e->pessoaNome, "%s", a->nome);
    js_texto(o, of, "imdb", e->imdb, sizeof e->imdb);
    js_texto(o, of, "midia", e->midia, sizeof e->midia);
    js_texto(o, of, "titulo", e->titulo, sizeof e->titulo);
    js_texto(o, of, "poster", e->poster, sizeof e->poster);
    e->temporada = (int)js_num(o, of, "temporada", 0.0);
    e->episodio = (int)js_num(o, of, "episodio", 0.0);
    e->pct = (int)js_num(o, of, "pct", 0.0);
    e->quando = (long long)js_num(o, of, "atualizado", 0.0);
    semTab(e->titulo);
  }
  p = js_array(r, NULL, "gostou");
  while (p && *p == '{' && a->nGostou < REC_AMIGO_GOSTOU) {
    const char *f = js_fim(p);
    RecEvento *e = &a->gostou[a->nGostou];
    e->fonte = REC_FONTE_NUVIO; e->acao = REC_ACAO_REACAO; e->reacao = 1; e->grau = a->grau;
    snprintf(e->pessoa, sizeof e->pessoa, "%s", a->id);
    snprintf(e->pessoaNome, sizeof e->pessoaNome, "%s", a->nome);
    js_texto(p, f, "imdb", e->imdb, sizeof e->imdb);
    js_texto(p, f, "midia", e->midia, sizeof e->midia);
    js_texto(p, f, "titulo", e->titulo, sizeof e->titulo);
    js_texto(p, f, "poster", e->poster, sizeof e->poster);
    e->temporada = (int)js_num(p, f, "temporada", 0.0);
    e->episodio = (int)js_num(p, f, "episodio", 0.0);
    e->quando = (long long)js_num(p, f, "criado", 0.0);
    semTab(e->titulo);
    if (!strncmp(e->imdb, "tt", 2)) a->nGostou++;
    p = js_prox(f);
  }
  p = js_array(r, NULL, "recs");
  while (p && *p == '{' && a->nRecs < REC_AMIGO_RECS) {
    const char *f = js_fim(p);
    char est[16] = "";
    a->recs[a->nRecs].id = (long long)js_num(p, f, "id", 0.0);
    a->recs[a->nRecs].criado = (long long)js_num(p, f, "criado", 0.0);
    js_texto(p, f, "imdb", a->recs[a->nRecs].imdb, sizeof a->recs[0].imdb);
    js_texto(p, f, "tipo", a->recs[a->nRecs].tipo, sizeof a->recs[0].tipo);
    js_texto(p, f, "titulo", a->recs[a->nRecs].titulo, sizeof a->recs[0].titulo);
    js_texto(p, f, "poster", a->recs[a->nRecs].poster, sizeof a->recs[0].poster);
    js_texto(p, f, "estado", est, sizeof est);
    semTab(a->recs[a->nRecs].titulo);
    a->recs[a->nRecs].estado = estadoRecDe(est);
    a->recs[a->nRecs].terminou = (int)js_num(p, f, "terminou", 0.0) > 0 ||
                                a->recs[a->nRecs].estado == REC_REC_TERMINOU;
    // A resposta direta de quem recebeu (migracao 007): so texto limpo e o
    // instante. Servidor sem ela: campos ausentes = sem resposta.
    js_texto(p, f, "resposta", a->recs[a->nRecs].resposta, sizeof a->recs[0].resposta);
    semTab(a->recs[a->nRecs].resposta);
    a->recs[a->nRecs].respondido = (long long)js_num(p, f, "respondido", 0.0);
    if (a->recs[a->nRecs].estado == REC_REC_REAGIU) {
      a->recs[a->nRecs].temReacao = 1;
      a->recs[a->nRecs].reacao = (int)js_num(p, f, "reacao", 0.0);
    }
    if (a->recs[a->nRecs].id > 0) a->nRecs++;
    p = js_prox(f);
  }
  if ((o = objeto(r, "gosto", &of))) {
    a->gostoTotal = (int)js_num(o, of, "total", 0.0);
    a->gostoIguais = (int)js_num(o, of, "iguais", 0.0);
    a->gostoPct = (int)js_num(o, of, "pct", 0.0);
    a->temGosto = a->compartilha && a->gostoTotal > 0 && a->gostoPct >= 0 && a->gostoPct <= 100;
    // F08: o detalhe por midia, o em comum e a cobertura, so DENTRO de "gosto"
    // (o "mes" tambem tem "filmes", e um numero ali nao e este objeto).
    { const char *so, *sf;
      if ((so = objetoEm(o, of, "filmes", &sf))) {
        a->temCmp = 1;
        a->filmesTotal = (int)js_num(so, sf, "total", 0.0);
        a->filmesIguais = (int)js_num(so, sf, "iguais", 0.0);
      }
      if ((so = objetoEm(o, of, "series", &sf))) {
        a->seriesTotal = (int)js_num(so, sf, "total", 0.0);
        a->seriesIguais = (int)js_num(so, sf, "iguais", 0.0);
      }
      if ((so = objetoEm(o, of, "comum", &sf))) {
        a->comumFilmes = (int)js_num(so, sf, "filmes", 0.0);
        a->comumSeries = (int)js_num(so, sf, "series", 0.0);
      }
      if ((so = objetoEm(o, of, "cobertura", &sf))) {
        a->cobEu = (int)js_num(so, sf, "eu", 0.0);
        a->cobEle = (int)js_num(so, sf, "ele", 0.0);
      } }
  }
  // A cached or malformed response must never resurrect withdrawn sharing.
  if (!a->compartilha) { a->temMes = a->temAgora = a->temGosto = a->nGostou = a->temCmp = 0; }
  return 1;
}

// --- estado no disco (mutex TOMADO) -------------------------------------------

static void gravarAlcance(void) {
  char s[16];
  snprintf(s, sizeof s, "%d\n", alcance);
  dados_gravar(REC_ARQ_ALCANCE, s);
}

static void socNovoCarregar(void) {
  char *b = recLer(REC_ARQ_ALCANCE);
  alcance = REC_ALCANCE_NAO_PERGUNTADO;
  if (b) {
    int v = atoi(b);
    if (b[0] && v >= 0 && v <= 2) alcance = v;
    free(b);
  }
  nFeedN = 0; etagFeed[0] = 0;
  b = recLer(REC_ARQ_FEED);
  if (b) {
    char *nl = strchr(b, '\n');
    if (nl) {
      *nl = 0;
      snprintf(etagFeed, sizeof etagFeed, "%s", b);
      nFeedN = feedParse(nl + 1, feedN, REC_FEED_MAX);
    }
    free(b);
  }
}

static void socNovoLimpar(int apagar) {
  meuNome[0] = 0; minhaExib[0] = 0;
  alcance = REC_ALCANCE_NAO_PERGUNTADO; alcancePendente = -2;
  nomePendente[0] = 0; temNomePendente = 0;
  nAtivN = 0;
  nFeedN = 0; etagFeed[0] = 0; pedirFeed = 0;
  temAmigo = 0; amigoEstado = REC_SOC_NADA; amigoBuscar = 0; amigoPedido[0] = 0;
  if (apagar) {
    recApagar(REC_ARQ_ALCANCE); recApagar(REC_ARQ_FEED); recApagar(REC_ARQ_AMIGO);
  }
}


// Resposta de /v1/eu. Mesma reconciliacao em UM SENTIDO de `aparecer`: aparelho
// que nunca perguntou adota a resposta dada em outra TV; aparelho que tem
// resposta e o servidor discorda manda a dele (e mais nova por construcao).
static void socNovoRegistrado(const char *nome, const char *exib, int alc) {
  if (nome && nome[0]) { snprintf(meuNome, sizeof meuNome, "%s", nome); semTab(meuNome); }
  if (!temNomePendente) snprintf(minhaExib, sizeof minhaExib, "%s", exib ? exib : "");
  if (alcance == REC_ALCANCE_NAO_PERGUNTADO) {
    if (alc >= 0 && alc <= 2) { alcance = alc; gravarAlcance(); }
  } else if (alc != alcance && alcancePendente == -2) {
    alcancePendente = alcance;
  }
}

// --- API publica ---------------------------------------------------------------

static void (*alcanceCb)(int);
static int alcanceAvisado = -9;

// Fora do mutex: o outro modulo pode chamar recomenda_* de dentro do aviso.
static void avisarAlcance(void) {
  int v;
  void (*fn)(int);
  if (!mtx) return;
  SDL_LockMutex(mtx);
  v = alcance > 0 ? alcance : 0;
  fn = alcanceCb;
  if (v == alcanceAvisado) fn = NULL;
  else alcanceAvisado = v;
  SDL_UnlockMutex(mtx);
  if (fn) fn(v);
}

void recomenda_ao_mudar_alcance(void (*fn)(int nivel)) {
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx); alcanceCb = fn; alcanceAvisado = -9; SDL_UnlockMutex(mtx);
  avisarAlcance();
}

int recomenda_cabecalhos(const char **cab, char *aut, size_t na, char *via, size_t nv,
                         char *perfil, size_t np) {
  const char *tcab[4];
  char chave[160];
  if (!recomenda_ativo() || perfis_precisa_escolher() || !cab || !aut || !via || !perfil) return 0;
  if (trakt_ativo() && trakt_cabecalhos(tcab, aut, na, chave, sizeof chave)) {
    snprintf(via, nv, "X-Nuvio-Auth: trakt");
  } else if (bearerSessao(aut, na)) {
    snprintf(via, nv, "X-Nuvio-Auth: nuvio");
  } else return 0;
  cab[0] = aut; cab[1] = via; cab[2] = NULL; cab[3] = NULL;
  if (perfilCab(perfil, np, !strcmp(via, "X-Nuvio-Auth: nuvio"))) cab[2] = perfil;
  return 1;
}

const char *recomenda_meu_nome(void) { return meuNome; }
const char *recomenda_minha_exibicao(void) { return minhaExib; }

static void acordar(void) {
  recomenda_verificar();
  SDL_LockMutex(mtx); pedidoAgora = 1; SDL_UnlockMutex(mtx);
}

int recomenda_definir_nome(const char *nome) {
  if (!recomenda_ativo()) return 0;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  snprintf(nomePendente, sizeof nomePendente, "%s", nome ? nome : "");
  semTab(nomePendente);
  snprintf(minhaExib, sizeof minhaExib, "%s", nomePendente);
  temNomePendente = 1;
  SDL_UnlockMutex(mtx);
  acordar();
  return 1;
}

int recomenda_alcance(void) {
  int v;
  if (!recomenda_ativo() || !mtx) return REC_ALCANCE_NAO_PERGUNTADO;
  SDL_LockMutex(mtx); v = alcance; SDL_UnlockMutex(mtx);
  return v;
}

void recomenda_responder_alcance(int nivel) {
  if (!recomenda_ativo()) return;
  if (nivel < 0) nivel = 0;
  if (nivel > 2) nivel = 2;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  alcance = nivel;
  alcancePendente = nivel;
  // Nada do que a pessoa acabou de negar pode sair depois do gesto.
  if (nivel == 0) nAtivN = 0;
  gravarAlcance();
  SDL_UnlockMutex(mtx);
  avisarAlcance();
  acordar();
}

int recomenda_atividade(const RecAtiv *a) {
  int i, progresso;
  RecAtiv pura;
  if (!recomenda_ativo() || !a || strncmp(a->imdb, "tt", 2)) return 0;
  pura = *a;                    // o id que viaja e o da serie, nao o do episodio
  imdbServidor(pura.imdb, sizeof pura.imdb, a->imdb);
  a = &pura;
  if (!(!strcmp(a->ev, "inicio") || !strcmp(a->ev, "progresso") || !strcmp(a->ev, "fim") ||
        !strcmp(a->ev, "abandono") || !strcmp(a->ev, "reacao") || !strcmp(a->ev, "salvo")))
    return 0;
  if (!mtx) mtx = SDL_CreateMutex();
  progresso = !strcmp(a->ev, "progresso");
  SDL_LockMutex(mtx);
  // SEM RESPOSTA OU COM "NINGUEM", NEM FILA: o servidor tambem recusaria, mas
  // o que nao pode sair nao deve nem esperar na memoria.
  if (alcance < 1) { SDL_UnlockMutex(mtx); return 0; }
  // Progresso seguido do MESMO titulo funde no que ja esta na fila: o ultimo
  // pct vale e os segundos somam.
  if (progresso)
    for (i = nAtivN - 1; i >= 0; i--)
      if (!strcmp(ativN[i].ev, "progresso") && !strcmp(ativN[i].imdb, a->imdb) &&
          ativN[i].temporada == a->temporada && ativN[i].episodio == a->episodio) {
        int seg = ativN[i].seg + (a->seg > 0 ? a->seg : 0);
        ativN[i] = *a;
        ativN[i].seg = seg;
        SDL_UnlockMutex(mtx);
        return 1;
      }
  if (nAtivN >= ATIVN_FILA) {          // cheia: sai o mais velho
    memmove(ativN, ativN + 1, sizeof ativN[0] * (ATIVN_FILA - 1));
    nAtivN--;
  }
  ativN[nAtivN] = *a;
  semTab(ativN[nAtivN].titulo);
  nAtivN++;
  SDL_UnlockMutex(mtx);
  // Progresso espera o ciclo de 60 s; o resto (comecou, terminou, reagiu) sai ja.
  if (!progresso) acordar();
  return 1;
}

void recomenda_feed_pedir(void) {
  if (!recomenda_ativo()) return;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx); pedirFeed = 1; SDL_UnlockMutex(mtx);
  acordar();
}

int recomenda_feed_n(void) {
  int n;
  if (!recomenda_ativo() || !mtx) return 0;
  SDL_LockMutex(mtx); n = nFeedN; SDL_UnlockMutex(mtx);
  return n;
}

int recomenda_feed_item(int i, RecEvento *saida) {
  int ok = 0;
  if (!recomenda_ativo() || !mtx || !saida) return 0;
  SDL_LockMutex(mtx);
  if (i >= 0 && i < nFeedN) { *saida = feedN[i]; ok = 1; }
  SDL_UnlockMutex(mtx);
  return ok;
}

int rec_evento_de_trakt(const CatItem *ci, long long quando, RecEvento *e) {
  const char *a;
  if (!ci || !e || strncmp(ci->imdb, "tt", 2) || !ci->socialSlug[0]) return 0;
  memset(e, 0, sizeof *e);
  e->fonte = REC_FONTE_TRAKT;
  a = ci->socialAcao;
  // Os rotulos que trakt_social grava (trakt.c). "assistindo agora" e o
  // "inicio" do nosso lado: e o mesmo fato visto por duas fontes.
  if (!strcmp(a, "assistindo agora") || !strcmp(a, "registrou um check-in"))
    e->acao = REC_ACAO_INICIO;
  else if (!strcmp(a, "avaliou")) e->acao = REC_ACAO_NOTA;
  else if (!strcmp(a, "assistiu")) e->acao = REC_ACAO_FIM;
  else return 0; // Unknown tracker actions are not proof that someone finished.
  e->agora = !strcmp(a, "assistindo agora");
  // O amigo Nuvio que ja vem no item (recomenda_social_mesclar) tem o id do
  // servico ("nuvio:..."); o do Trakt e o slug puro.
  if (strchr(ci->socialSlug, ':')) {
    e->fonte = REC_FONTE_NUVIO;
    snprintf(e->pessoa, sizeof e->pessoa, "%s", ci->socialSlug);
  } else {
    snprintf(e->pessoa, sizeof e->pessoa, "trakt:%s", ci->socialSlug);
  }
  rec_nome_exibicao(e->pessoaNome, sizeof e->pessoaNome, ci->socialNome, e->pessoa);
  snprintf(e->pessoaAvatar, sizeof e->pessoaAvatar, "%s", ci->socialAvatar);
  e->grau = 1;
  snprintf(e->imdb, sizeof e->imdb, "%s", ci->imdb);
  snprintf(e->midia, sizeof e->midia, "%s", !strcmp(ci->tipo, "series") ? "series" : "movie");
  snprintf(e->titulo, sizeof e->titulo, "%s", ci->titulo);
  snprintf(e->poster, sizeof e->poster, "%s", ci->poster);
  e->temporada = ci->temporada;
  e->episodio = ci->episodio;
  e->quando = quando > 0 ? quando : 0;
  return 1;
}

// Mesma pessoa, mesmo titulo, mesmo fato, perto no tempo (ou sem hora).
static int mesmoFato(const RecEvento *a, const RecEvento *b) {
  long long dt;
  if (a->acao != b->acao || strcmp(a->imdb, b->imdb) || strcmp(a->pessoa, b->pessoa) ||
      strcmp(a->midia, b->midia) || a->temporada != b->temporada || a->episodio != b->episodio) return 0;
  if (a->acao == REC_ACAO_REACAO && a->reacao != b->reacao) return 0;
  if (!a->quando || !b->quando) return 1;
  dt = a->quando - b->quando;
  return dt >= -3600 && dt <= 3600;
}

int rec_eventos_unir(RecEvento *dst, int n, const RecEvento *src, int nsrc, int max) {
  int i, j;
  if (!dst || max < 1) return 0;
  if (n > max) n = max;
  for (j = 0; src && j < nsrc; j++) {
    const RecEvento *s = &src[j];
    for (i = 0; i < n; i++) if (mesmoFato(&dst[i], s)) break;
    if (i < n) {
      // FUSAO: fica o do NOSSO servidor (reacao, grau, capa filtrada); o outro
      // so completa o que faltar.
      int srcGanha = s->fonte == REC_FONTE_NUVIO && dst[i].fonte != REC_FONTE_NUVIO;
      RecEvento base = srcGanha ? *s : dst[i];
      const RecEvento *o = srcGanha ? &dst[i] : s;
      if (!base.quando || (o->quando && o->quando > base.quando)) base.quando = o->quando;
      if (!base.poster[0]) snprintf(base.poster, sizeof base.poster, "%s", o->poster);
      if (!base.pessoaAvatar[0]) snprintf(base.pessoaAvatar, sizeof base.pessoaAvatar, "%s", o->pessoaAvatar);
      if (!base.titulo[0]) snprintf(base.titulo, sizeof base.titulo, "%s", o->titulo);
      dst[i] = base;
      continue;
    }
    if (n < max) dst[n++] = *s;
    else {
      // Cheia: so entra se for mais novo que o mais velho (que sai).
      int velho = 0;
      for (i = 1; i < n; i++)
        if (dst[i].quando < dst[velho].quando) velho = i;
      if (s->quando > dst[velho].quando) dst[velho] = *s;
    }
  }
  // Insercao estavel por `quando` decrescente; 0 (sem hora) vai para o fim.
  for (i = 1; i < n; i++) {
    RecEvento t = dst[i];
    long long q = t.quando ? t.quando : -1;
    for (j = i - 1; j >= 0 && (dst[j].quando ? dst[j].quando : -1) < q; j--) dst[j + 1] = dst[j];
    dst[j + 1] = t;
  }
  return n;
}

int recomenda_feed_unido(RecEvento *saida, int max, const CatItem *trakt,
                         const long long *quandoTrakt, int nTrakt) {
  RecEvento *tr;
  int n = 0, i, k = 0;
  if (!saida || max < 1) return 0;
  if (recomenda_ativo() && mtx) {
    SDL_LockMutex(mtx);
    n = nFeedN < max ? nFeedN : max;
    memcpy(saida, feedN, sizeof *saida * (size_t)n);
    SDL_UnlockMutex(mtx);
  }
  if (!trakt || nTrakt < 1) return n;
  tr = (RecEvento *)malloc(sizeof *tr * (size_t)nTrakt);
  if (!tr) return n;
  for (i = 0; i < nTrakt; i++)
    if (rec_evento_de_trakt(&trakt[i], quandoTrakt ? quandoTrakt[i] : 0, &tr[k])) k++;
  // A MESMA PESSOA PELAS DUAS FONTES (F08): o amigo que ligou o Trakt ao perfil
  // Nuvio vem do Trakt como "trakt:<slug>" e do nosso servidor como o id do
  // contato. Com o id trocado, rec_eventos_unir junta os dois fatos e a fileira
  // mostra um rosto so. Servidor antigo (sem ids): nada troca.
  for (i = 0; i < k; i++) {
    RecContato c;
    if (!contatoCanonico(tr[i].pessoa, &c)) continue;
    snprintf(tr[i].pessoa, sizeof tr[i].pessoa, "%s", c.id);
    if (c.nome[0]) snprintf(tr[i].pessoaNome, sizeof tr[i].pessoaNome, "%s", c.nome);
    if (c.avatar[0]) snprintf(tr[i].pessoaAvatar, sizeof tr[i].pessoaAvatar, "%s", c.avatar);
  }
  n = rec_eventos_unir(saida, n, tr, k, max);
  free(tr);
  return n;
}

// So letras, numeros e ":._-": o id vai na URL sem escapar.
static int idSeguro(const char *s) {
  size_t n = 0;
  if (!s) return 0;
  for (; *s; s++, n++)
    if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') ||
          *s == ':' || *s == '.' || *s == '_' || *s == '-')) return 0;
  return n > 0 && n < 96;
}

int recomenda_amigo_pedir(const char *id) {
  char *b;
  if (!recomenda_ativo() || !idSeguro(id)) return 0;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  if (strcmp(amigoPedido, id)) {
    temAmigo = 0;
    // CACHE DE DISCO: a tela abre com o ultimo perfil aberto desta pessoa.
    b = recLer(REC_ARQ_AMIGO);
    if (b) {
      char *nl = strchr(b, '\n');
      if (nl) { *nl = 0; if (!strcmp(b, id) && amigoParse(nl + 1, &amigo)) temAmigo = 1; }
      free(b);
    }
  }
  snprintf(amigoPedido, sizeof amigoPedido, "%s", id);
  amigoBuscar = 1;
  amigoEstado = REC_SOC_INDO;
  SDL_UnlockMutex(mtx);
  acordar();
  return 1;
}

int recomenda_amigo(RecAmigo *saida) {
  int ok = 0;
  if (!recomenda_ativo() || !mtx || !saida) return 0;
  SDL_LockMutex(mtx);
  if (temAmigo) { *saida = amigo; ok = 1; }
  SDL_UnlockMutex(mtx);
  return ok;
}

int recomenda_amigo_estado(void) {
  int e;
  if (!mtx) return REC_SOC_NADA;
  SDL_LockMutex(mtx); e = amigoEstado; SDL_UnlockMutex(mtx);
  return e;
}

// --- REDE (no fio) ---------------------------------------------------------------

static void enviarNomeNovo(const char **cab) {
  char corpo[160], esc[120], nome[64] = "", *r = NULL;
  int st = 0;
  if (!recTravarFio()) { free(r); return; }
  if (!temNomePendente) { recDestravarFio(); return; }
  jsonEsc(esc, sizeof esc, nomePendente);
  recDestravarFio();
  snprintf(corpo, sizeof corpo, "{\"nome\":\"%s\"}", esc);
  url("/v1/eu/nome");
  r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
  if (r && st >= 200 && st < 300) {
    js_texto_raiz(r, "nome", nome, sizeof nome);
    if (!recTravarFio()) { free(r); return; }
    temNomePendente = 0;
    if (nome[0]) { snprintf(meuNome, sizeof meuNome, "%s", nome); semTab(meuNome); }
    recDestravarFio();
  }
  printf("[recomenda] nome de exibicao HTTP %d\n", st); fflush(stdout);
  free(r); r = NULL;
}

static void enviarAlcance(const char **cab) {
  char corpo[32], *r = NULL;
  int quer, st = 0;
  if (!recTravarFio()) { free(r); return; } quer = alcancePendente; recDestravarFio();
  if (quer < 0) return;
  snprintf(corpo, sizeof corpo, "{\"nivel\":%d}", quer);
  url("/v1/alcance");
  r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
  free(r); r = NULL;
  printf("[recomenda] alcance=%d HTTP %d\n", quer, st); fflush(stdout);
  if (st >= 200 && st < 300) {
    if (!recTravarFio()) { free(r); return; } if (alcancePendente == quer) alcancePendente = -2; recDestravarFio();
  }
}

static void enviarAtivNova(const char **cab) {
  int n = 0;
  while (n < 8) {
    RecAtiv a;
    char corpo[NV_ARTE_URL_MAX * 2 + 800], t[400], po[NV_ARTE_URL_MAX * 2], *r = NULL;
    int st = 0;
    if (!recTravarFio()) { free(r); return; }
    // A fila so vale com o nivel ligado E o servidor sabendo disso: com o
    // "sim" ainda pendente, o evento chegaria antes do consentimento e seria
    // descartado la.
    if (alcance < 1) nAtivN = 0;
    if (nAtivN < 1 || alcancePendente >= 0) { recDestravarFio(); return; }
    a = ativN[0];
    memmove(ativN, ativN + 1, sizeof ativN[0] * (size_t)(--nAtivN));
    recDestravarFio();
    jsonEsc(t, sizeof t, a.titulo);
    jsonEsc(po, sizeof po, a.poster);
    snprintf(corpo, sizeof corpo,
             "{\"ev\":\"%s\",\"imdb\":\"%s\",\"midia\":\"%s\",\"titulo\":\"%s\",\"poster\":\"%s\","
             "\"temporada\":%d,\"episodio\":%d,\"pct\":%d,\"seg\":%d,\"reacao\":%d,\"rec\":%lld}",
             a.ev, a.imdb, !strcmp(a.midia, "series") ? "series" : "movie", t, po,
             a.temporada, a.episodio, a.pct, a.seg, a.reacao, a.rec);
    url("/v1/atividade");
    r = recPostar(fioUrl, REC_TEMPO_REDE, cab, corpo, &st);
    free(r); r = NULL;
    printf("[recomenda] evento %s HTTP %d\n", a.ev, st); fflush(stdout);
    n++;
  }
}

static void lerFeedNovo(const char **cab, int forcar) {
  const char *cabs[5];
  char cabEtag[160], etagNovo[96] = "", *r = NULL;
  unsigned ger;
  int st = 0, k = 0, n;
  RecEvento *tmp = NULL;
  if (!recTravarFio()) { free(r); free(tmp); return; }
  if (!forcar && !pedirFeed) { recDestravarFio(); return; }
  pedirFeed = 0;
  ger = geracao;
  while (k < 3 && cab[k]) { cabs[k] = cab[k]; k++; }
  cabs[k] = NULL; cabs[k + 1] = NULL;
  if (etagFeed[0] && nFeedN) {
    snprintf(cabEtag, sizeof cabEtag, "If-None-Match: %s", etagFeed);
    cabs[k] = cabEtag;
  }
  recDestravarFio();
  // Sempre desde 0 (50 mais novos): quem baixa o nivel tem de SUMIR da lista,
  // e um cursor incremental nunca apagaria o que ja chegou.
  url("/v1/feed");
  r = recBaixarEtag(fioUrl, REC_TEMPO_REDE, cabs, &st, etagNovo, sizeof etagNovo);
  if (st == 304 || !r || st < 200 || st >= 300) { free(r); r = NULL; return; }
  tmp = (RecEvento *)malloc(sizeof *tmp * REC_FEED_MAX);
  if (!tmp) { free(r); r = NULL; return; }
  n = feedParse(r, tmp, REC_FEED_MAX);
  if (!recTravarFio()) { free(r); free(tmp); return; }
  if (ger == geracao) {
    size_t tam = strlen(r) + strlen(etagNovo) + 2;
    char *arq = (char *)malloc(tam);
    memcpy(feedN, tmp, sizeof *tmp * (size_t)n);
    nFeedN = n;
    snprintf(etagFeed, sizeof etagFeed, "%s", etagNovo);
    if (arq) { snprintf(arq, tam, "%s\n%s", etagNovo, r); dados_gravar_leve(REC_ARQ_FEED, arq); free(arq); }
  }
  recDestravarFio();
  printf("[recomenda] feed: %d eventos\n", n); fflush(stdout);
  free(tmp); tmp = NULL;
  free(r); r = NULL;
}

static void lerAmigo(const char **cab) {
  static RecAmigo tmp;      // ~30 KB: fora da pilha do fio (um fio so)
  char id[96], *r = NULL;
  unsigned ger;
  int st = 0, ok;
  if (!recTravarFio()) { free(r); return; }
  if (!amigoBuscar) { recDestravarFio(); return; }
  amigoBuscar = 0;
  snprintf(id, sizeof id, "%s", amigoPedido);
  ger = geracao;
  recDestravarFio();
  url("/v1/amigo?id=");
  { size_t k = strlen(fioUrl); snprintf(fioUrl + k, sizeof fioUrl - k, "%s", id); }
  r = recBaixar(fioUrl, REC_TEMPO_REDE, cab, &st);
  ok = r && st >= 200 && st < 300 && amigoParse(r, &tmp);
  if (!recTravarFio()) { free(r); return; }
  if (ger == geracao && !strcmp(id, amigoPedido)) {
    if (ok) {
      size_t tam = strlen(r) + strlen(id) + 2;
      char *arq = (char *)malloc(tam);
      // O id DO PEDIDO, nao o da resposta: e por ele que a tela pergunta.
      snprintf(tmp.id, sizeof tmp.id, "%s", id);
      amigo = tmp; temAmigo = 1;
      if (arq) { snprintf(arq, tam, "%s\n%s", id, r); dados_gravar_leve(REC_ARQ_AMIGO, arq); free(arq); }
    }
    if (!ok && (st == 403 || st == 404)) {
      temAmigo = 0; memset(&amigo, 0, sizeof amigo); dados_apagar(REC_ARQ_AMIGO);
    }
    amigoEstado = ok ? REC_SOC_OK : st == 403 ? REC_SOC_NEGADO
                : st == 404 ? REC_SOC_NAO_ACHOU : REC_SOC_FALHA;
  }
  recDestravarFio();
  printf("[recomenda] perfil do amigo HTTP %d\n", st); fflush(stdout);
  free(r); r = NULL;
}

// Um ciclo completo. 1 quando falou com o servidor (ou tentou); 0 quando nem
// havia identidade para tentar, que e o caso do primeiro segundo do arranque.
static int ciclo(void) {
  const char *cab[4];
  int reg, querSug, forcarContatos;
  if (!recomenda_ativo()) return 0;
  if (!recSessaoDefinir(perfis_sessao_geracao())) return 0;
  if (!recTravarFio()) return 0;
  int pronta = recSessaoDona == recSessaoFio();
  recDestravarFio();
  if (!pronta) return 0;
  recAtualizarToken();
  if (!perfis_sessao_travar(recSessaoFio())) return 0;
  int ident = identidade(cab);
  perfis_sessao_destravar();
  if (!ident) return 0;
  // TROCOU DE IDENTIDADE NO MEIO DA SESSAO (ligou ou desligou o Trakt): tudo o
  // que este aparelho guarda pertence a OUTRA pessoa para o servidor — o perfil
  // publicado, os pedidos, os achados, a fila de atividade. Guardar seria
  // mostrar/enviar a conta errada; a regra e a de sair da conta.
  // TROCAR DE PERFIL E O MESMO CASO: outro perfil e outra pessoa no servidor.
  { RecFioContexto *ctx = recFioContexto();
    if (!ctx) return 0;
    char *viaVista = ctx->viaVista;
    char agoraVia[80];
    if (ctx->viaSessao != recSessaoFio()) { viaVista[0] = 0; ctx->viaSessao = recSessaoFio(); }
    snprintf(agoraVia, sizeof agoraVia, "%s|%s", fioVia, cab[2] ? cab[2] : "");
    if (viaVista[0] && strcmp(viaVista, agoraVia)) {
      snprintf(viaVista, sizeof ctx->viaVista, "%s", agoraVia);
      recomenda_esquecer();
      return 1;
    }
    snprintf(viaVista, sizeof ctx->viaVista, "%s", agoraVia); }
  if (!recTravarFio()) return 0;
  reg = registrado;
  forcarContatos = contatosForcar; contatosForcar = 0;
  recDestravarFio();
  if (!reg) {
    if (!registrar(cab)) return 1;   // servidor fora; tentar de novo no proximo
    // O VINCULO AUTOMATICO DOS SEGUIDOS DO TRAKT SAIU DAQUI, e a troca e
    // deliberada. Ele transformava em contato, sem ninguem apertar nada, toda
    // pessoa que o dono segue no Trakt e que tambem usa o servico — dos dois
    // lados, e a cada arranque do app. O pedido que originou esta mudanca diz
    // "mostrar os que instalaram o app ... e adicionar como amigo": mostrar e
    // adicionar sao dois passos, e o segundo e de quem esta olhando.
    //
    // O QUE NAO MUDOU: a rota /v1/contatos/trakt continua existindo e continua
    // vinculando na hora — ela e o botao "procurar amigos do Trakt" da tela de
    // amigos, que e uma acao que alguem toma. O que deixou de existir e a
    // varredura silenciosa no arranque. Reverter e recolocar uma linha aqui.
    lerContatos(cab);
    lerSugestoes(cab);
    conciliarPerfil(cab);
    lerPedidos(cab);
    lerFeedNovo(cab, 1);
    if (!recTravarFio()) return 0;
    contatosMs = SDL_GetTicks() + REC_CONTATOS_MS;
    recDestravarFio();
  } else {
    if (forcarContatos || (Sint32)(SDL_GetTicks() - contatosMs) >= 0) {
      lerContatos(cab);
      lerSugestoes(cab);
      lerFeedNovo(cab, 1);
      if (!recTravarFio()) return 0;
      contatosMs = SDL_GetTicks() + REC_CONTATOS_MS;
      recDestravarFio();
    }
    // OS PEDIDOS NAO ESPERAM OS DEZ MINUTOS DOS CONTATOS (06/10, relato do
    // dono: "nao da pra ver quando alguem te adicionou, e nao mostra na ilha").
    // Eles moravam neste bloco: um pedido feito com a TV ligada so chegava na
    // proxima volta do relogio de 600 s — nem a ilha, nem a aba Amigos (que so
    // chama recomenda_pedir_agora, que nao mexe nesse relogio) ficavam sabendo.
    // Agora sao lidos a cada ciclo (60 s, ou na hora em que a aba abre): um
    // GET pequeno, do tamanho do feed que ja sai a cada ciclo.
    lerPedidos(cab);
  }
  lerFeedNovo(cab, 0);
  enviarAparecer(cab);
  enviarAlcance(cab);
  enviarNomeNovo(cab);
  enviarAtivNova(cab);
  lerAmigo(cab);
  enviarPerfil(cab);
  enviarAtividade(cab);
  tratarSocial(cab);
  tratarIdentidade(cab);
  enviarFila(cab);
  tratarContatos(cab);
  confirmarVistas(cab);
  enviarRespostas(cab);
  if (!recTravarFio()) return 0;
  querSug = pedirSugestoes; pedirSugestoes = 0;
  recDestravarFio();
  // FORA DO RELOGIO DE DEZ MINUTOS so quando alguem pediu: abrir a aba Social
  // ou aceitar uma sugestao. A sondagem de 60 s NAO pede sugestao — a lista de
  // quem a pessoa talvez conheca nao muda de minuto em minuto, e cada pedido
  // custa um JOIN no servidor e, no ramo do Trakt, uma leitura do cache.
  if (querSug) lerSugestoes(cab);
  lerRecs(cab);
  return 1;
}

static int fioLaco(void *arg) {
  (void)arg;
  while (!fioParar) {
    Uint32 agora = SDL_GetTicks();
    int agir;
    SDL_LockMutex(mtx);
    agir = pedidoAgora || (Sint32)(agora - proximoMs) >= 0;
    pedidoAgora = 0;
    SDL_UnlockMutex(mtx);
    if (agir) {
      int falou = ciclo();
      // Jitter de ate ~12% para as TVs nao baterem juntas no servidor.
      Uint32 passo = falou ? (REC_INTERVALO_MS << recFalhas) : REC_ESPERA_MS;
      if (falou) passo += (SDL_GetTicks() * 2654435761u >> 16) % (passo / 8);
      if (recTravarFio()) {
        proximoMs = SDL_GetTicks() + passo;
        recDestravarFio();
      }
    }
    // 200 ms e a granularidade de reacao a um envio ou a abertura da aba. Com
    // a sondagem em 60 s, o laco acorda 300 vezes para fazer uma requisicao —
    // e cada despertar e uma leitura de inteiro atras do mutex.
    SDL_Delay(200);
  }
  return 0;
}

void recomenda_verificar(void) {
  if (!recomenda_ativo()) return;
  if (!mtx) mtx = SDL_CreateMutex();
  // SAIR CEDO E O PONTO DESTA FUNCAO, e nao uma micro-otimizacao. app.c a
  // chama no bloco `homePronta`, que roda uma vez POR QUADRO; marcar
  // `pedidoAgora` aqui faria o fio rodar um ciclo a cada 200 ms — 300 vezes a
  // sondagem combinada, contra o mesmo servidor, de todas as TVs.
  if (fioLigado) return;
  fioLigado = 1;
  SDL_LockMutex(mtx);
  pedidoAgora = 1;
  SDL_UnlockMutex(mtx);
  fio = SDL_CreateThread(fioLaco, "nv-recomenda", NULL);
  if (fio) SDL_DetachThread(fio);
  else {
    fioLigado = 0;
    printf("[recomenda] sem fio para falar com o servico\n");
    fflush(stdout);
  }
}

void recomenda_pedir_agora(void) {
  if (!recomenda_ativo()) return;
  recomenda_verificar();            // garante o fio de pe
  if (!mtx) return;
  SDL_LockMutex(mtx);
  pedidoAgora = 1;
  // AS SUGESTOES ACOMPANHAM O PEDIDO MANUAL, e nao a sondagem de 60 s. Quem
  // chama esta funcao e sempre uma acao de quem esta na sala — abrir a aba
  // Social, vincular alguem, responder a pergunta do consentimento — e depois
  // de qualquer uma delas a lista de sugeridos pode ter mudado. A sondagem
  // periodica continua sem pedir nada disso.
  pedirSugestoes = 1;
  SDL_UnlockMutex(mtx);
}

int recomenda_enviar(const CatItem *ci, const char *paraId, int modelo,
                     const char *texto) {
  if (!recomenda_ativo() || !ci || !ci->imdb[0] || !paraId || !paraId[0])
    return 0;
  if (modelo != -1 && (modelo < 0 || modelo >= REC_MODELOS)) return 0;
  if (!mtx) mtx = SDL_CreateMutex();
  SDL_LockMutex(mtx);
  if (fila.cheia || envioEstado == REC_ENVIO_INDO) { SDL_UnlockMutex(mtx); return 0; }
  memset(&fila, 0, sizeof fila);
  // O CARD DE CONTINUAR ASSISTINDO DE SERIE TRAZ "tt123:S:E" (descoberta.c), e
  // o servidor so aceita /^tt\d+$/ (HTTP 400, issue #363). A recomendacao e do
  // titulo, nao do episodio: corta no primeiro ':'.
  imdbServidor(fila.imdb, sizeof fila.imdb, ci->imdb);
  snprintf(fila.tipo,   sizeof fila.tipo,   "%s", ci->tipo[0] ? ci->tipo : "movie");
  snprintf(fila.titulo, sizeof fila.titulo, "%s", ci->titulo);
  snprintf(fila.poster, sizeof fila.poster, "%s", ci->poster);
  // O ANO E OS PRIMEIROS QUATRO DIGITOS DO META. O CatItem nao tem campo de ano
  // — ele guarda "2022 · 3 temporadas" em `meta` — e mandar a linha inteira
  // encheria a coluna `ano` do servidor com texto que ninguem le.
  { int i, k = 0;
    for (i = 0; ci->meta[i] && k < 4; i++)
      if (ci->meta[i] >= '0' && ci->meta[i] <= '9') fila.ano[k++] = ci->meta[i];
      else if (k) break;
    fila.ano[k] = 0;
    if (k != 4) fila.ano[0] = 0; }
  // A NOTA SAI DAQUI, do CatItem de quem MANDA, e nao do catalogo de quem
  // recebe: o titulo recomendado pode nao existir no catalogo do amigo — e esse
  // e justamente o caso em que uma recomendacao e util. A unidade e a do
  // CatItem (centesimos, 83 = 8,3); fora da faixa vira 0 e o selo some.
  fila.nota = (ci->nota >= 0 && ci->nota <= 100) ? ci->nota : 0;
  snprintf(fila.para,  sizeof fila.para,  "%s", paraId);
  snprintf(fila.texto, sizeof fila.texto, "%s", texto ? texto : "");
  fila.modelo = modelo;
  fila.cheia = 1;
  envioEstado = REC_ENVIO_INDO;
  SDL_UnlockMutex(mtx);
  recomenda_pedir_agora();
  return 1;
}

// --- CARTAO DE ABERTURA ------------------------------------------------------

// GANCHO DE TODO QUADRO COM A HOME DE PE — E JA NAO ABRE NADA. Antes abria o
// cartao modal assim que a sondagem trazia "N nova(s)" (e uma vez por
// recomendacao), por cima da home: toda tecla de quem estava rolando uma
// fileira era descartada ate a pessoa apertar OK ou Voltar (so os dois
// saiam). Quem anuncia a recomendacao que chega e a ILHA ("Ana recomendou
// Fallout", avisos.c anunciarItem), que nao pega tecla nenhuma: a navegacao
// segue livre e o modal dela (Ver / Salvar / Dispensar) so abre quando a
// pessoa o aciona. O cartao continua existindo para a abertura de proposito
// (recomenda_abrir_cartao). Fica a funcao porque app.c e os testes a chamam.
void recomenda_mostrar_se_houver(void) {
}

// Abre o cartao AGORA, de proposito (nao a chegada de uma recomendacao): a
// mais nova nao lida, sem a marca de "uma vez por recomendacao".
void recomenda_abrir_cartao(void) {
  int i, achou = -1;
  if (!recomenda_ativo() || !mtx || cartaoAberto) return;
  SDL_LockMutex(mtx);
  for (i = 0; i < nItens; i++)
    if (!itens[i].visto) { achou = i; break; }
  if (achou >= 0) cartaoItem = itens[achou];
  SDL_UnlockMutex(mtx);
  if (achou >= 0) { cartaoMostrado = 1; cartaoAberto = 1; }
}

int recomenda_tecla(const SDL_Event *e) {
  if (!cartaoAberto) return 0;
  recomenda_evento(e);
  return 1;
}

void recomenda_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!cartaoAberto || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    cartaoAberto = 0;
    snprintf(pedido, sizeof pedido, "%s", cartaoItem.imdb);
    temPedido = 1;
    recomenda_marcar_vistas();
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK) {
    // FECHAR NAO E LER. O selo continua na aba; quem dispensou o cartao nao
    // disse que ja viu a recomendacao, disse que nao quer decidir agora.
    cartaoAberto = 0;
  }
}

void recomenda_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!cartaoAberto && cartaoEntrada < 0.002f) { cartaoEntrada = 0.0f; return; }
  cartaoEntrada = anim_rampa(cartaoEntrada, cartaoAberto ? 1.0f : 0.0f, dt,
                             cartaoAberto ? RC_ABRIR_MS : RC_FECHAR_MS);
}

// "há 2 h". A FRASE INTEIRA passa por i18n como FORMATO, nao montada de
// pedacos: "há" e "atrás" trocam de lugar na traducao (mesma regra de
// salvospainel.c).
void rec_quando_texto(char *dst, size_t tam, long long quandoS) {
  long long d = (long long)time(NULL) - quandoS;
  if (quandoS <= 0) { dst[0] = 0; return; }
  if (d < 0) d = 0;
  // "agora mesmo" e nao "agora": uma chave de UMA palavra comum e um risco real
  // neste sistema, porque a chave da tabela e o proprio portugues e qualquer
  // texto dinamico identico a ela tambem seria traduzido (ver a nota em
  // idioma.h). Duas palavras tornam a colisao improvavel.
  if (d < 90)          snprintf(dst, tam, "%s", i18n("agora mesmo"));
  else if (d < 5400)   snprintf(dst, tam, i18n("há %d min"), (int)(d / 60));
  else if (d < 172800) snprintf(dst, tam, i18n("há %d h"),   (int)(d / 3600));
  else                 snprintf(dst, tam, i18n("há %d dias"), (int)(d / 86400));
}

// "Segue no Trakt" / "Amigo de Gustavo". A FRASE INTEIRA passa por i18n como
// FORMATO, e nao montada de "Amigo de" + nome: em ingles a preposicao e a ordem
// mudam, e a chave da tabela e sempre uma string inteira.
void rec_sugestao_origem(char *dst, size_t tam, const RecSugestao *s) {
  if (!dst || tam < 2) return;
  dst[0] = 0;
  if (!s) return;
  if (!strcmp(s->origem, "trakt")) {
    // "VOCE SEGUE", com o sujeito. `users/me/following` e quem o dono segue, e
    // nao quem o segue — "Segue no Trakt" sozinho le como o contrario, e a
    // frase existe justamente para a pessoa decidir se conhece o sugerido.
    snprintf(dst, tam, "%s", i18n("Você segue no Trakt"));
    return;
  }
  // SEM O NOME DO INTERMEDIARIO ainda ha o que dizer, e dizer importa: a frase
  // e a unica coisa na linha que explica por que um estranho esta sendo
  // sugerido. Vazio acontece quando quem faz a ponte nunca preencheu o perfil.
  if (s->viaNome[0]) snprintf(dst, tam, i18n("Amigo de %s"), s->viaNome);
  else               snprintf(dst, tam, "%s", i18n("Amigo de um contato seu"));
}

// Frase da recomendacao: o modelo traduzido, ou o texto livre como veio.
const char *rec_frase(const RecItem *r) {
  if (!r) return "";
  if (r->modelo >= 0 && r->modelo < REC_MODELOS) return i18n(MODELOS[r->modelo]);
  return r->texto;
}

// --- AS TRES MARCAS DA LINHA -------------------------------------------------

// Primeiro CARACTERE, e nao primeiro byte: "Álvaro" tem dois bytes na primeira
// letra e cortar no byte produz um glifo invalido. Copiada de perfilsel.c de
// proposito — la ela e estatica, e exportar uma funcao de uma tela de perfil
// para um modulo de rede seria a dependencia errada.
static void recInicial(const char *nome, char *dst, size_t tam) {
  size_t z = 1;
  if (tam < 5) { if (tam) dst[0] = 0; return; }
  if (!nome || !nome[0]) { dst[0] = '?'; dst[1] = 0; return; }
  while (z < 4 && (nome[z] & 0xc0) == 0x80) z++;
  memcpy(dst, nome, z);
  dst[z] = 0;
}

// A COR DO DISCO SAI DO ID, e nao de um acaso nem do acento do aparelho.
//
// perfilsel.c usa a cor que a CONTA guarda para cada perfil; um contato deste
// servico nao tem cor nenhuma no servidor, e inventar uma nova a cada quadro
// faria o mesmo amigo mudar de cor entre duas linhas. Um hash do id estavel
// resolve os dois: a cor e sempre a mesma para a mesma pessoa, e duas pessoas
// diferentes quase sempre caem em discos diferentes.
//
// As seis cores sao as da paleta de acentos que ajustes.c ja oferece, e nao um
// arco-iris novo: elas ja foram escolhidas para ter contraste contra o #0D0D0D
// do fundo e para a letra branca se ler por cima.
static void recCorDoId(const char *id, float *r, float *g, float *b) {
  static const float PALETA[6][3] = {
    { 0.180f, 0.490f, 0.910f },   // azul
    { 0.400f, 0.733f, 0.416f },   // verde
    { 0.855f, 0.420f, 0.290f },   // coral
    { 0.560f, 0.420f, 0.850f },   // violeta
    { 0.910f, 0.650f, 0.200f },   // ambar
    { 0.180f, 0.680f, 0.700f },   // turquesa
  };
  unsigned h = 2166136261u;       // FNV-1a: oito linhas a menos que um md5 e
  const char *p = id ? id : "";   // com a unica propriedade que interessa aqui
  for (; *p; p++) { h ^= (unsigned char)*p; h *= 16777619u; }
  h %= 6u;
  *r = PALETA[h][0]; *g = PALETA[h][1]; *b = PALETA[h][2];
}

int rec_nome_reserva(const char *s) {
  const char *h;
  if (!s || !s[0]) return 0;
  h = strrchr(s, '#');
  if (!h || !h[1] || h == s || h[-1] != ' ') return 0;
  for (h++; *h; h++) if (*h < '0' || *h > '9') return 0;
  return 1;
}

void rec_identidade(const char *nome, const char *apelido, char *l1, size_t n1,
                    char *l2, size_t n2) {
  const char *n = (nome && nome[0] && !rec_nome_reserva(nome)) ? nome : "";
  const char *ap = (apelido && apelido[0] && !rec_nome_reserva(apelido)) ? apelido : "";
  // O APELIDO E O MESMO TEXTO DO NOME em quem escolheu o proprio nome como
  // apelido ("fabi" e "@fabi"): a segunda linha so repetiria a primeira.
  if (n[0] && ap[0] && !SDL_strcasecmp(n, ap)) ap = "";
  if (l2 && n2) l2[0] = 0;
  if (n[0]) {
    snprintf(l1, n1, "%s", n);
    if (ap[0] && l2 && n2) snprintf(l2, n2, "@%s", ap);
  } else if (ap[0]) snprintf(l1, n1, "%s", ap);
  else snprintf(l1, n1, "%s", i18n("Sem nome"));
}

// O SELO DE CRIADOR. E um selo da tabela unica (badges.h: BADGE_H, raio
// pilula, TXT_CAPTION2, BADGE_PADX), no estilo BADGE_REALCE — o acento a 18 %
// por baixo —, com duas diferencas que o fazem ler como MARCA e nao como
// estado: o icone de verificado na frente e o texto no tom do acento (o acento
// clareado, para ler sobre o escuro mesmo nos acentos fechados).
#define REC_SELO_ICONE 18.0f
static void tomDoSelo(float *r, float *g, float *b) {
  ajustes_acento(r, g, b);
  *r = *r + (1.0f - *r) * 0.35f; *g = *g + (1.0f - *g) * 0.35f; *b = *b + (1.0f - *b) * 0.35f;
}
float rec_selo_pessoa_largura(const char *selo) {
  if (!selo || strcmp(selo, "criador")) return 0.0f;
  return BADGE_PADX - 2.0f + REC_SELO_ICONE + 6.0f + (float)txt_largura(TXT_CAPTION2, "Criador") + BADGE_PADX;
}
float rec_selo_pessoa(float x, float y, const char *selo, float alfa) {
  float w = rec_selo_pessoa_largura(selo), ar, ag, ab, tr, tg, tb;
  if (w <= 0.0f) return 0.0f;
  ajustes_acento(&ar, &ag, &ab);
  tomDoSelo(&tr, &tg, &tb);
  gfx_cor((GfxRect){ x, y, w, BADGE_H }, 0.5f, ar, ag, ab, 0.20f * alfa);
  gfx_icone((GfxRect){ x + BADGE_PADX - 2.0f, y + (BADGE_H - REC_SELO_ICONE) * 0.5f,
                       REC_SELO_ICONE, REC_SELO_ICONE }, "aj_badge-check", tr, tg, tb, alfa);
  { TxtLinha l = txt_linha(TXT_CAPTION2, "Criador", (int)(tr * 255.0f), (int)(tg * 255.0f),
                           (int)(tb * 255.0f), 255);
    txt_desenhar_alpha(l, x + BADGE_PADX - 2.0f + REC_SELO_ICONE + 6.0f,
                       y + (BADGE_H - (float)l.h) * 0.5f, alfa); }
  return w;
}

void rec_avatar(GfxRect a, const char *url, const char *nome, const char *id,
                float alfa) {
  rec_avatar_estilo(a, url, nome, id, alfa, -1);
}

void rec_avatar_estilo(GfxRect a, const char *url, const char *nome, const char *id,
                       float alfa, int estilo) {
  GLuint foto = (url && url[0]) ? tex_obter_larg(url, a.w) : 0;
  // SOQUETE ESCURO POR BAIXO SEMPRE, como perfilsel.c: enquanto a foto nao
  // chega da rede, o lugar dela e um disco e nao um buraco com o fundo do
  // painel aparecendo — e um buraco redondo le como defeito.
  gfx_rect(a, 0, GFX_DISCO, 0, 0, 0, 0, 0.09f, 0.09f, 0.10f, alfa);
  if (foto) {
    gfx_tex_aspect_atual = tex_aspecto(url);
    gfx_rect(a, foto, GFX_AVATAR, 0, 0, 0, 0, 1, 1, 1, alfa);
    gfx_tex_aspect_atual = 0.0f;
    return;
  }
  { float cr, cg, cb;
    char ini[8];
    TxtLinha l;
    recCorDoId(id && id[0] ? id : nome, &cr, &cg, &cb);
    gfx_rect(a, 0, GFX_DISCO, 0, 0, 0, 0, cr, cg, cb, alfa);
    recInicial(nome, ini, sizeof ini);
    l = txt_linha(estilo >= 0 ? (TxtEstilo)estilo : a.w >= 48.0f ? TXT_CALLOUT : TXT_CAPTION2,
                  ini, 255, 255, 255, 255);
    txt_desenhar_alpha(l, a.x + (a.w - l.w) * 0.5f, a.y + (a.h - l.h) * 0.5f, alfa); }
}

float rec_selo_tipo(float x, float y, const char *tipo, int escuro, float alfa) {
  // "Série" e "Filme" JA SAO CHAVES DA TABELA — as mesmas que metaTexto usa na
  // aba Salvos. Reaproveita-las e o que faz a aba Social e a aba Salvos dizerem
  // "Série" com a mesma palavra em qualquer idioma.
  //
  // O DESENHO E O DA TABELA UNICA (badges.h, 21/09/2026): neutro sobre o
  // painel, e a variante SOBRE_REALCE quando a linha esta em foco — a pilula
  // clara do foco engolia um preenchimento branco a 0.10, entao `escuro`
  // troca o estilo e nao so a cor.
  int serie = tipo && !strncmp(tipo, "series", 6);
  return badge_desenhar(x, y, serie ? "Série" : "Filme",
                        escuro ? BADGE_SOBRE_REALCE : BADGE_NEUTRO, alfa);
}

float rec_selo_imdb(float x, float y, int nota, int escuro, float alfa) {
  // A MARCA da tabela unica: amarelo #F5C518 e "IMDb" preto em todo lugar
  // (home, card de Continuar, detalhe, aqui). Separador decimal pelo idioma
  // fica dentro de badge_imdb.
  return badge_imdb(x, y, nota, escuro, alfa);
}

// O titulo do cartao como CatItem, para o logo: o item do catalogo quando o
// titulo esta nele (logo do addon/TMDB, escolha a mao); senao uma copia so com
// o logo do metahub pelo IMDb, refeita quando o cartao troca de recomendacao.
static const CatItem *cartaoLogoItem(void) {
  static CatItem ci;
  static long long deId = -1;
  static char deImdb[24];
  int i = cartaoItem.imdb[0] ? cat_indice_por_imdb(cartaoItem.imdb) : -1;
  const CatItem *c = i >= 0 ? cat_item(i) : NULL;
  if (c) return c;
  if (deId != cartaoItem.id || strcmp(deImdb, cartaoItem.imdb)) {
    deId = cartaoItem.id;
    snprintf(deImdb, sizeof deImdb, "%s", cartaoItem.imdb);
    memset(&ci, 0, sizeof ci);
    snprintf(ci.imdb, sizeof ci.imdb, "%s", cartaoItem.imdb);
    snprintf(ci.tipo, sizeof ci.tipo, "%s", cartaoItem.tipo);
    snprintf(ci.titulo, sizeof ci.titulo, "%s", cartaoItem.titulo);
    arte_metahub_preencher(&ci);
  }
  return &ci;
}

static void recomenda_desenharCorpo_(Uint32 agora);
// Cartao de tela quase cheia: ampliado so se ainda couber (escala.h).
void recomenda_desenhar(Uint32 agora) {
  ESCALA_SE_COUBER_INI(RC_W, RC_H);
  recomenda_desenharCorpo_(agora);
  ESCALA_SE_COUBER_FIM();
}
static void recomenda_desenharCorpo_(Uint32 agora) {
  float a, dy, x, y;
  char buf[320];
  (void)agora;
  if (cartaoEntrada < 0.002f) return;
  a = anim_suave(cartaoEntrada);
  dy = (1.0f - a) * 36.0f;

  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0.0f, 0, 0, 0,
          0.72f * cartaoEntrada);
  { GfxRect c = { RC_X, RC_Y + dy, RC_W, RC_H };
    gfx_vidro_ou_solido(c, 0.030f, GFX_VIDRO_DENS_MODAL, a, 0.075f, 0.078f, 0.088f, 0.98f); }
  gfx_recorte(RC_X, RC_Y + dy, RC_W, RC_H);

  { GfxRect p = { RC_X + RC_PAD, RC_Y + dy + (RC_H - RC_POSTER_H) * 0.5f,
                  RC_POSTER_W, RC_POSTER_H };
    // Poster pedido pela largura com que desenha (RC_POSTER_W=220, cap 288):
    // o 640 unico decodificava 2,4 MB por poster de cartao. Ver
    // tests/artemenor.c.
    GLuint tex = cartaoItem.poster[0] ? tex_obter_larg(cartaoItem.poster, RC_POSTER_W) : 0;
    if (tex) {
      gfx_tex_aspect_atual = tex_aspecto(cartaoItem.poster);
      gfx_rect(p, tex, GFX_CARD, 0.0f, 0.0f, 0.0f, 0.06f, 0, 0, 0, a);
      gfx_tex_aspect_atual = 0.0f;
    } else {
      gfx_cor(p, 0.06f, NV_COR_ESQUELETO_R, NV_COR_ESQUELETO_G,
              NV_COR_ESQUELETO_B, a);
    } }

  x = RC_X + RC_PAD + RC_POSTER_W + 44.0f;
  y = RC_Y + dy + 66.0f;
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("RECOMENDAÇÃO DE UM AMIGO"),
                           150, 154, 165, 255);
    txt_desenhar_alpha(t, x, y, a * 0.92f); y += t.h + 14.0f; }
  // A CARA DE QUEM MANDOU AO LADO DO NOME. Era so o nome, e um nome sozinho num
  // cartao que aparece no arranque nao diz de quem e ate a pessoa LER — a foto
  // diz antes. Sem foto (conta Nuvio) o disco leva a inicial, exatamente como
  // perfilsel.c faz com perfil sem foto.
  { GfxRect av = { x, y + 2.0f, RC_AVATAR, RC_AVATAR };
    float tx2 = x + RC_AVATAR + RC_AVATAR_GAP;
    rec_avatar(av, cartaoItem.deAvatar, cartaoItem.deNome, cartaoItem.de, a);
    // Formato inteiro em i18n: em ingles o nome vem antes do verbo e depois do
    // objeto, e uma frase remendada aqui sairia na ordem errada.
    snprintf(buf, sizeof buf, i18n("%s te recomendou"), cartaoItem.deNome);
    { TxtLinha t = txt_linha_corta(TXT_CALLOUT, buf, 200, 204, 214, 255,
                                   RC_W - (tx2 - RC_X) - RC_PAD);
      txt_desenhar_alpha(t, tx2, y + (RC_AVATAR - t.h) * 0.5f, a * 0.95f); }
    y += RC_AVATAR + 14.0f; }
  // O LOGO DO TITULO no lugar do nome (logotitulo.h), numa caixa reservada:
  // sem logo, o nome escrito ocupa a mesma altura e o cartao nao pula.
  logotitulo_desenhar(cartaoLogoItem(), cartaoItem.titulo, TXT_TITULO2, x, y,
                      RC_LOGO_W, RC_LOGO_H, RC_W - (x - RC_X) - RC_PAD, a);
  y += RC_LOGO_H + 12.0f;
  { const char *frase = rec_frase(&cartaoItem);
    if (frase[0]) {
      snprintf(buf, sizeof buf, "\xe2\x80\x9c%s\xe2\x80\x9d", frase);
      y += txt_bloco(TXT_BODY, buf, 214, 218, 228, x, y,
                     RC_W - (x - RC_X) - RC_PAD, 38.0f, a * 0.96f, 2) + 16.0f;
    } }
  // FILME OU SÉRIE, A NOTA E O QUANDO, NA MESMA LINHA. Sao as tres coisas que
  // se olham de relance e nenhuma delas merece uma linha propria — juntas elas
  // custam os mesmos 30px que o "há 2 h" sozinho custava.
  { float sx = x;
    sx += rec_selo_tipo(sx, y, cartaoItem.tipo, 0, a * 0.95f) + REC_SELO_GAP;
    { float w = rec_selo_imdb(sx, y, cartaoItem.nota, 0, a * 0.95f);
      if (w > 0.0f) sx += w + REC_SELO_GAP + 6.0f; }
    rec_quando_texto(buf, sizeof buf, cartaoItem.criado);
    if (buf[0]) {
      TxtLinha t = txt_linha(TXT_CAPTION, buf, 150, 154, 165, 255);
      txt_desenhar_alpha(t, sx, y + (REC_SELO_H - t.h) * 0.5f, a * 0.85f);
    } }

  y = RC_Y + dy + RC_H - 70.0f;
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("OK para abrir"), 200, 204, 214, 255);
    txt_desenhar_alpha(t, x, y, a * 0.9f); }
  { TxtLinha t = txt_linha(TXT_CAPTION2, i18n("Voltar para depois"),
                           150, 154, 165, 255);
    txt_desenhar_alpha(t, RC_X + RC_W - RC_PAD - t.w, y, a * 0.85f); }
  gfx_sem_recorte();
}

void recomenda_abrir_sessao(void) {
  uint64_t sessao = perfis_sessao_geracao();
  if (!mtx) mtx = SDL_CreateMutex();
  limparEstado(0);
  recresp_abrir_sessao();
  SDL_LockMutex(mtx);
  recPerfil = perfis_ativo();
  recSessaoDona = 0;
  temPedido = 0; pedido[0] = 0; cartaoEntrada = 0.0f;
  fioSlugsTem = fioSlugsN = 0; fioSlugsMs = 0;
  pedidoAgora = 1; proximoMs = contatosMs = 0; recFalhas = 0;
  SDL_UnlockMutex(mtx);
  recomenda_iniciar();
  SDL_LockMutex(mtx);
  recSessaoDona = sessao;
  SDL_UnlockMutex(mtx);
}
