#ifndef NUVIO_FAROLPECA_H
#define NUVIO_FAROLPECA_H
// PECAS DO FAROL — tipos das sete pecas da UI generativa, parser e validador
// do FarolCena v1 (lote sense, issue 39; PLANO-GOSTO secoes 4.1 e 4.2).
//
// O Farol nao desenha: ele entrega uma CENA e o app desenha com as pecas que
// ja sabe fazer. A cena vem das regras locais (farolcena, issue 44) ou do
// servidor com LLM (issues 40 e 52). Antes de subir na tela, toda cena passa
// aqui: o validador recusa o que nao reconhece, PE PECA — uma peca torta cai
// sozinha e as boas da mesma cena seguem valendo.
//
// CONTRATO DO ENVELOPE (secao 4.1):
//   {"v":1, "tela":"home", "perfil":2, "feita":1795000000, "vale_s":86400,
//    "origem":"regras", "pecas":[ {...}, ... ]}
// Obrigatorios: v (=1), tela, pecas. perfil, feita, vale_s e origem sao
// opcionais. Chave que o validador nao conhece recusa a cena inteira (nao
// ha peca para atribuir); v != 1 idem. Cena com mais de 8 KB nem e lida.
//
// CONTRATO DAS PECAS (secao 4.2): "t" e o tipo e define o resto do schema:
//   fileira  P1  {id, titulo, forma, itens[6..12], sub?, motivo?}
//   dica     P2  {id, l1, acao, l2?, icone?}
//   porque   P3  {alvo, confianca 0..3, linhas[1..2], selos?[<=3]}
//   comparar P4  {itens[2..3], eixos[1..8], veredito?, escolha?}
//   escolha  P5  {passo, de<=3, pergunta, opcoes[2..4]}
//   info     P6  {alvo, itens[1..3] de {k, l}}
//   achados  P7  {l1, itens[1..3], l2?, motivo?}
// Chave fora do schema da peca, tipo JSON errado, enum fora da lista,
// referencia que nao resolve, texto "x" reprovado sem "m" de reserva ou
// numero fora da faixa recusam A PECA. A 7a peca e recusada sozinha.
//
// TITULO POR REFERENCIA: "c<n>" e indice na lista de candidatos da cena
// (nao viaja no JSON — quem chama informa nCandidatos) e "tt<digitos>" e id
// IMDb. As duas formas valem em itens, alvo, escolha, chave de motivo e
// argumento de modelo; "c<n>" tem de resolver (n < nCandidatos).
//
// TEXTO: {"m":"FT_*","a":["..."]} e frase de modelo com argumentos; "m" tem
// de existir em FAROL_FRASES (faroltexto.h) e "a" tem de ter exatamente o
// numero de argumentos do modelo (%s/%d do literal). {"x":"frase pronta"} e
// o texto livre do LLM: so sobe se passar em faroltexto_aceita() (2 linhas
// de 62, sem URL, sem emoji, sem a lista "nunca diz" do ALMA 2). Reprovou,
// cai no "m" de reserva; sem "m", a peca e recusada. "x" aprovado sem "m"
// tambem vale (as regras nunca produzem "x"; e o LLM quem manda os dois) —
// ate o teto de FAROL_XS textos por cena: estourado o pool, o "x" aprovado
// tambem cai no "m" de reserva e, sem ele, a peca cai com FPE_LIMITE.
//
// MODULO PURO: sem malloc, sem SDL, sem rede, sem dados.h. Tudo estatico na
// FarolCena — 25,9 KB no pior caso. O orcamento da secao 4.4 reserva 8 KB
// para a cena serializada e 96 KB para o gosto inteiro: a FarolCena TEM de
// ser transitoria (valida, consome e descarta em seguida); quem a segurar
// em estado de UI estoura o orcamento.
// Nenhum titulo, nome de perfil ou gosto em log: este modulo nao loga.

#include <stddef.h>
#include <stdint.h>

// ------------------------------------------------------------- limites v1
#define FAROL_CENA_MAX    8192   // cena maior que 8 KB: recusada inteira
#define FAROL_PECA_MAX    6      // ate 6 pecas por cena
#define FAROL_ID_TAM      32     // id de peca/dica ("gosto1", "d_serie_parada")
#define FAROL_ENV_TAM     16     // tela e origem
#define FAROL_REF_TAM     20     // "c<n>" ou "tt<digitos>"
#define FAROL_TEXTO_M_TAM 40     // nome do modelo FT_*
#define FAROL_ARGS        4      // nenhum modelo do gosto tem mais de 3
#define FAROL_ARG_TAM     48     // argumentos sao ids e valores curtos
#define FAROL_XS          8      // textos livres "x" guardados por cena
#define FAROL_X_TAM       512    // 2 linhas x 62 pontos de codigo, pior caso
#define FAROL_ITENS_MAX   12     // fileira 6..12
#define FAROL_SELA_MAX    3
#define FAROL_OPCOES_MAX  4      // assistente: 2..4 opcoes
#define FAROL_INFO_MAX    3      // coluna de ate 3 linhas
#define FAROL_EIXO_MAX    8      // lista fechada da secao 4.2 (P4)
#define FAROL_LINHAS_MAX  2      // porque: 1..2 linhas
#define FAROL_ICONE_TAM   32
#define FAROL_OPCAO_TAM   16

// ------------------------------------------------------ erros de CENA (rc)
// farolpeca_ler devolve -FCE_* quando a cena inteira e recusada.
enum {
  FCE_OK = 0,
  FCE_TAMANHO,   // cena > 8 KB
  FCE_JSON,      // nao e JSON valido: nao e um objeto fechado, lixo depois
                 // do '}', estrutura torta EM QUALQUER NIVEL (virgula ou
                 // dois-pontos fora do lugar, escape invalido, controle
                 // cru, token que nao e valor) ou chave duplicada no
                 // envelope. Documento malformado recusa a cena inteira —
                 // a recusa por peca e so para JSON valido fora do schema.
  FCE_V,         // "v" ausente ou != 1
  FCE_CHAVE,     // chave desconhecida no envelope
  FCE_CAMPO,     // campo do envelope com tipo ou valor errado
  FCE_FALTA      // falta tela ou pecas
};

// ------------------------------------------------------ erros de PECA
// Guardados por posicao em FarolCena.erro[]; a peca cai sozinha.
enum {
  FPE_OK = 0,
  FPE_JSON,        // elemento nao e objeto ou chave duplicada (literal ou
                   // resolvendo ao mesmo item, como "c0" e "c00" em motivo)
  FPE_TIPO,        // falta "t" ou tipo fora das sete pecas
  FPE_CHAVE,       // chave desconhecida na peca (ou em texto/acao/selo/...)
  FPE_TIPO_VALOR,  // chave conhecida com tipo JSON errado
  FPE_FALTA,       // campo obrigatorio ausente
  FPE_ENUM,        // forma/eixo/acao.tipo/selo.k/info.k fora da lista fechada
  FPE_REF,         // "c<n>" fora da lista de candidatos, ref malformada,
                   // item duplicado, motivo/escolha fora de itens
  FPE_TEXTO,       // texto sem m nem x, m desconhecido, args tortos,
                   // id/icone malformado
  FPE_X,           // "x" reprovado por faroltexto_aceita e sem "m" de reserva
  FPE_FAIXA,       // numero fora da faixa, lista curta/longas, duplicado
                   // em eixo/opcao/info.k, alvo onde nao cabe
  FPE_LIMITE       // peca alem da 6a, ou "x" aprovado com o pool cheio e
                   // sem "m" de reserva (com "m", cai no modelo)
};

// ------------------------------------------------------------------- tipos
enum { FP_FILEIRA, FP_DICA, FP_PORQUE, FP_COMPARAR, FP_ESCOLHA, FP_INFO,
       FP_ACHADOS };

enum { FP_FORMA_CARTAZ, FP_FORMA_LARGA, FP_FORMA_FICHA, FP_FORMA_PALCO };

enum { FP_ACAO_ABRIR, FP_ACAO_TOCAR, FP_ACAO_COMPARAR, FP_ACAO_ESCOLHA,
       FP_ACAO_SALVAR, FP_ACAO_NADA };

enum { FP_EIXO_DURACAO, FP_EIXO_NOTA, FP_EIXO_CLIMA, FP_EIXO_PORQUE,
       FP_EIXO_QUALIDADE, FP_EIXO_TERMINA, FP_EIXO_ONDE, FP_EIXO_AMIGOS };

enum { FP_SELO_PESSOA, FP_SELO_TEMA, FP_SELO_GENERO };

enum { FP_INFO_QUALIDADE, FP_INFO_CONTINUAR, FP_INFO_TEMPO, FP_INFO_ONDE,
       FP_INFO_AMIGOS, FP_INFO_SAGA, FP_INFO_REVER, FP_INFO_RETOMADA };

// Referencia a titulo: "c<n>" (indice na lista de candidatos da cena) ou
// "tt<digitos>" (IMDb). c >= 0 so depois de resolvida.
typedef struct {
  int16_t c;                 // indice do candidato, ou -1 quando e "tt"
  char    s[FAROL_REF_TAM];  // forma textual como veio
} FarolRef;

// Texto de peca: modelo FT_* com argumentos e/ou "x" do LLM ja filtrado.
// O que sobe na tela: x quando usaX, senao o modelo m com os argumentos a[].
typedef struct {
  uint8_t usaX;                 // 1 = exibir xs[xi]; 0 = exibir o modelo
  uint8_t xReprovada;           // x veio e nao e o exibido: reprovou no
                                // aceita ou o pool de xs encheu — caiu no m
  int8_t  xi;                   // indice em FarolCena.xs quando usaX, senao -1
  uint8_t nArgs;
  char    m[FAROL_TEXTO_M_TAM]; // "FT_*" ou "" quando so x
  char    a[FAROL_ARGS][FAROL_ARG_TAM];
} FarolTexto;

// linha "motivo" de um item da fileira/achados (P1/P7): chave = o item.
typedef struct { int8_t item; FarolTexto t; } FarolMotivo;  // item = indice em itens[]

typedef struct { uint8_t k; int32_t id; } FarolSelo;         // k = FP_SELO_*
typedef struct { uint8_t k; FarolTexto l; } FarolInfoItem;   // k = FP_INFO_*
typedef struct { char id[FAROL_OPCAO_TAM]; FarolTexto r; } FarolOpcao;

typedef struct {
  uint8_t tipo;   // FP_*
  union {
    struct {                                        // P1
      char      id[FAROL_ID_TAM];
      FarolTexto titulo, sub;                       // sub opcional
      uint8_t   forma;                              // FP_FORMA_*
      FarolRef  itens[FAROL_ITENS_MAX];  uint8_t nItens;
      FarolMotivo motivo[FAROL_ITENS_MAX]; uint8_t nMotivo;
    } fileira;
    struct {                                        // P2
      char      id[FAROL_ID_TAM];
      FarolTexto l1, l2;                            // l2 opcional
      uint8_t   acao;                               // FP_ACAO_*
      uint8_t   temAlvo;
      FarolRef  alvo;                               // so p/ abrir/tocar/comparar/salvar
      char      icone[FAROL_ICONE_TAM];             // "" = sem icone
    } dica;
    struct {                                        // P3
      FarolRef  alvo;
      uint8_t   confianca;                          // 0..3
      FarolTexto linhas[FAROL_LINHAS_MAX]; uint8_t nLinhas;
      FarolSelo selos[FAROL_SELA_MAX];   uint8_t nSelos;
    } porque;
    struct {                                        // P4
      FarolRef  itens[3];                uint8_t nItens;
      uint8_t   eixos[FAROL_EIXO_MAX];   uint8_t nEixos; // FP_EIXO_*, na ordem
      FarolTexto veredito;                          // opcional
      int8_t    escolha;                            // indice em itens, -1 sem
    } comparar;
    struct {                                        // P5
      uint8_t   passo, de;                          // 1..de, de<=3
      FarolTexto pergunta;
      FarolOpcao opcoes[FAROL_OPCOES_MAX]; uint8_t nOpcoes;
    } escolha;
    struct {                                        // P6
      FarolRef  alvo;
      FarolInfoItem itens[FAROL_INFO_MAX]; uint8_t nItens;
    } info;
    struct {                                        // P7
      FarolTexto l1, l2;                            // l2 opcional
      FarolRef  itens[3];                uint8_t nItens;
      FarolMotivo motivo[3];             uint8_t nMotivo;
    } achados;
  } u;
} FarolPeca;

// Bits de FarolCena.opt: quais campos opcionais do envelope vieram.
enum { FP_O_PERFIL = 1, FP_O_FEITA = 2, FP_O_VALE = 4, FP_O_ORIGEM = 8 };

typedef struct {
  char      tela[FAROL_ENV_TAM];      // "home", "detalhe", ...
  int32_t   perfil;                   // quando FP_O_PERFIL
  int64_t   feita, vale_s;
  char      origem[FAROL_ENV_TAM];    // "regras", "llm", ... (quando FP_O_ORIGEM)
  uint8_t   opt;
  int       nPecas;                   // pecas aceitas, compactadas em pecas[]
  int       nRecusadas;               // recusadas por peca (inclui a 7a em diante)
  uint8_t   erro[FAROL_PECA_MAX];     // FPE_* por posicao 0..5 no array "pecas"
  FarolPeca pecas[FAROL_PECA_MAX];
  int       nXs;
  char      xs[FAROL_XS][FAROL_X_TAM];  // textos livres "x" aprovados
} FarolCena;

// ------------------------------------------------------------------- funcoes
// Le e valida a cena. `json` e uma string C fechada em NUL (o contrato de
// todo o js.h da casa: o transporte fecha o corpo antes de entregar).
// nCandidatos e o tamanho da lista de candidatos da cena, para resolver
// "c<n>" (< 0 vale como lista vazia: qualquer "cN" falha).
//
// rc >= 0: numero de pecas aceitas (c->pecas[0..rc), c->nPecas == rc).
// rc < 0: a cena inteira foi recusada; -rc e o codigo FCE_* e nada em c
//         esta garantido alem de zerado.
int farolpeca_ler(FarolCena *c, const char *json, int nCandidatos);

// Nome estavel do codigo ("FPE_ENUM", "FCE_TAMANHO"), para o corpus de
// tests/farol_cenas/ — o servidor porta o validador e confere pelo nome.
const char *farolpeca_erro_nome(int fpe);
const char *farolpeca_cena_erro_nome(int fce);
const char *farolpeca_tipo_nome(int tipo);   // "fileira", "dica", ...

// 1 se o texto tem o que mostrar (modelo ou x aprovado).
int farolpeca_texto_tem(const FarolTexto *t);

// O texto a exibir: xs[xi] quando usaX, "" quando nao usaX.
const char *farolpeca_x(const FarolCena *c, const FarolTexto *t);

#endif // NUVIO_FAROLPECA_H
