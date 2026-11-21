#ifndef NUVIO_FAROLGOSTO_H
#define NUVIO_FAROLGOSTO_H
// MODELO DE GOSTO DO FAROL — o vetor esparso de afinidades por perfil (lote
// sense, issue 37; secoes 2.1 e 2.2 do PLANO-GOSTO). E o "o que a pessoa gosta"
// persistente que o Mapa de hoje nao tem: cada evento (terminou, gostou,
// abandonou, salvou, recusou) vira um delta espalhado pelas dimensoes do
// titulo — genero, pessoa, tema, decada, idioma, duracao e faixa de hora.
//
// Tudo em ponto fixo Q8.8, sem malloc, sem SDL, sem rede, sem dados.h: so o
// perfil ativo fica em RAM e o _Static_assert garante <= 6144 bytes. O relogio
// e INJETADO: `dia` (dias desde 1/1/2026) e `faixaHora` chegam por parametro —
// o modulo nao chama time() nem SDL_GetTicks, entao o teste controla o tempo.
//
// A ficha do titulo (FarolFicha, issue 36) ainda nao existe nesta base: quem
// chama monta a FarolGostoFicha abaixo com os MESMOS campos que a 36 expoe
// ({tmdb, tipo, ano, idioma, minutos, genero[4], pessoa[6], tema[8]}); a 38
// (farolsinal) faz a copia campo a campo. Quando farolficha.h existir este
// bloco pode virar um typedef sem mudar comportamento.
//
// Arquivo: farol-gosto-p<perfil>.txt em dados_dir() — uma linha por afinidade
// ("g 18 512 14 320 7"), texto, para o backup por aparelho da issue 43. Quem
// escreve/le o arquivo e o chamador (a 38); aqui ficam so a serializacao para
// um buffer e a leitura de volta, que tem de ser IDENTICA na ida e na volta.
// Versao 2 do formato: o 5o campo de cada linha (e do cabecalho) e o resto
// do decaimento — sem ele persistido, a recarga diaria congelava o peso
// (revisao FAROL37 r2). A leitura aceita 1 e 2; num arquivo 1 os restos
// ficam em 0.

#include <stddef.h>
#include <stdint.h>

#define FAROL_GOSTO_VERSAO 2

// Ponto fixo Q8.8 dos pesos e dos deltas: 1,0 = 256. Peso logico fica preso
// em [-32, +32] (o intervalo documentado no plano), ou seja, +-8192 crus.
#define FAROL_GOSTO_Q      256
#define FAROL_GOSTO_PESOMAX 8192

// Deltas da tabela 2.2, ja em Q8.8 (o farolsinal escolhe um por evento;
// "visto junto" multiplica por 0,5 antes de chamar; dormiu/falha = 0, nem
// chama). Serie largada/contagem por temporada fica a cargo de quem chama.
#define FG_D_TERMINOU    256   // +1,0  terminou (>= 90%)
#define FG_D_GOSTOU      512   // +2,0  terminou e gostou (coracao, nota >= 7)
#define FG_D_REVIU       384   // +1,5  reviu
#define FG_D_SALVOU      154   // +0,6  salvou / watchlist
#define FG_D_MAIS_ASSIM  384   // +1,5  "Mais assim" no menu do cartaz
#define FG_D_INTERESSE    51   // +0,2  detalhe > 20 s ou trailer inteiro
#define FG_D_ABANDONOU  -205   // -0,8  abandono cedo, sem falha e sem sono
#define FG_D_LARGOU     -256   // -1,0  serie largada (21 dias)
#define FG_D_REPROVOU   -512   // -2,0  nota <= 3, "Nao" ou "Nao e para mim"
#define FG_D_TIROU      -154   // -0,6  tirou do Continuar

// Meias-vidas do decaimento preguiçoso, em dias (tabela 2.2).
#define FG_MEIA_AFIN 120   // genero, pessoa, tema, decada, idioma
#define FG_MEIA_HORA  45   // duracao e hora

// Tamanhos dos vetores esparsos (secao 2.1).
#define FG_GENERO_N  32
#define FG_PESSOA_N  64
#define FG_TEMA_N    96
#define FG_DECADA_N  12
#define FG_IDIOMA_N  16
#define FG_DURACAO_N 10    // 5 faixas x filme/serie
#define FG_HORA_F     8    // faixas do dia (manha/tarde/noite/madrugada x sem/fds)
#define FG_HORA_D     5    // faixas de duracao
#define FG_RECUS_N  128
#define FG_MOSTR_N  160

// Dimensoes do modelo, para farolgosto_peso.
enum {
  FG_DIM_GENERO, FG_DIM_PESSOA, FG_DIM_TEMA,
  FG_DIM_DECADA, FG_DIM_IDIOMA, FG_DIM_DURACAO, FG_DIM_N
};

// tipo da ficha: decide a metade da tabela de duracao (filme 0-4, serie 5-9).
// ATENCAO (issue 38): os valores NAO batem com os da FarolFicha da 36, onde
// FF_FILME=1 e FF_SERIE=2. Ao montar a FarolGostoFicha converta pelo enum:
// `tipo == FF_SERIE ? FG_TIPO_SERIE : FG_TIPO_FILME`. A copia direta do
// valor cru grava filme no balde de serie e serie no de filme (com 45 min:
// filme cai no id 7 em vez do 2 e vice-versa).
enum { FG_TIPO_FILME, FG_TIPO_SERIE };

// Espelho, para este modulo, dos campos que a FarolFicha (issue 36) tem e o
// modelo usa. Arrays terminam no primeiro id 0; id 0 nunca entra no vetor.
typedef struct {
  int32_t tmdb;              // id do titulo (so conta; nunca vai ao vetor)
  uint8_t tipo;              // FG_TIPO_FILME / FG_TIPO_SERIE
  int16_t ano;               // 0 = desconhecido -> nao toca decada
  int32_t idioma;            // codigo ISO empacotado ('p'<<8|'t'); 0 = ignora
  int16_t minutos;           // <=0 = desconhecido -> nao toca duracao/hora
  int32_t genero[4];         // ids TMDB (filme e serie usam tabelas proprias)
  int32_t pessoa[6];         // ids TMDB; negativo = hash do nome (sem TMDB)
  int32_t tema[8];           // ids de palavra-chave do TMDB
} FarolGostoFicha;

// Uma afinidade: id da dimensao, peso Q8.8, n de evidencias e o dia da ultima.
// pack(2): o plano fixa FarolAfin em 10 bytes — 230 delas pesam 460 B a menos.
#pragma pack(push, 2)
typedef struct { int32_t id; int16_t peso; uint16_t n; uint16_t dia; } FarolAfin;

typedef struct {
  uint16_t versao;           // FAROL_GOSTO_VERSAO
  uint16_t perfil;
  uint32_t titulos;          // obras distintas com ficha (ver obraNova)
  uint16_t diaBase;          // referencia do decaimento do bloco hora[][]
  uint16_t recProx;          // cursor do anel recusado (gravacoes totais)
  uint16_t mostProx;         // cursor do anel mostrado
  FarolAfin genero[FG_GENERO_N];
  FarolAfin pessoa[FG_PESSOA_N];
  FarolAfin tema[FG_TEMA_N];
  FarolAfin decada[FG_DECADA_N];     // 1920...2030, indice = (ano-1920)/10
  FarolAfin idioma[FG_IDIOMA_N];
  FarolAfin duracao[FG_DURACAO_N];   // id = faixa+1 (+5 se serie)
  int16_t   hora[FG_HORA_F][FG_HORA_D]; // Q8.8 acumulado, meia-vida 45 d
  uint32_t  recusado[FG_RECUS_N];    // hash do titulo ("Nao e para mim"), anel
  struct { uint32_t h; uint8_t vezes, dia; } mostrado[FG_MOSTR_N];
  // Restos de decaimento, em dezesseis-avos de dia (< meia-vida cada): o que
  // falta para fechar o proximo avo de meia-vida. Saem no arquivo desde o
  // formato FG 2 (5o campo); a leitura de FG 1 os deixa em 0 — sem eles,
  // gravar/ler entre as sessoes diarias jogava fora o intervalo pendente
  // e o peso congelava na meia-vida inteira (revisao FAROL37 r2).
  uint8_t   horaResto;               // resto do bloco hora[][] (ref: diaBase)
  uint8_t   restoGenero[FG_GENERO_N];
  uint8_t   restoPessoa[FG_PESSOA_N];
  uint8_t   restoTema[FG_TEMA_N];
  uint8_t   restoDecada[FG_DECADA_N];
  uint8_t   restoIdioma[FG_IDIOMA_N];
  uint8_t   restoDuracao[FG_DURACAO_N];
} FarolGosto;
#pragma pack(pop)

// Capacidade do buffer de farolgosto_escrever. O texto normal fica em poucos
// KB (o plano mira <= 8 KB); o teto cobre o pior caso com todos os vetores
// cheios. Escrever sem caber devolve 0 e o chamador NAO grava o arquivo.
//
// Pior caso medido campo a campo (ressalva herdada do lote, issue 38 — o
// antigo 12288 nao fechava: o teto real e 12291):
//   cabecalho "FG 2 p t d r\n"                                  <= 40 B
//   afinidade "k -2147483648 -32768 65535 65535 255\n"          <= 37 B cada
//   hora      "h 7 -32768 x5\n"                                 <= 39 B cada
//   recusado  "r 4294967295 x16\n"                              <= 178 B por 16
//   mostrado  "m 4294967295 255 255 x8\n"                       <= 154 B por 8
// O _Static_assert abaixo amarra a formula: se uma linha engordar, o teto
// quebra na compilacao, nao em disco.
#define FAROL_GOSTO_TXT_MAX \
  (40 + (FG_GENERO_N + FG_PESSOA_N + FG_TEMA_N + FG_DECADA_N + FG_IDIOMA_N + \
         FG_DURACAO_N) * 37 + FG_HORA_F * 39 + \
   ((FG_RECUS_N + 15) / 16) * 178 + ((FG_MOSTR_N + 7) / 8) * 154)
_Static_assert(FAROL_GOSTO_TXT_MAX >= 12291,
               "buffer do farol-gosto-p<n>.txt abaixo do pior caso medido");

// Zera o modelo e marca versao/perfil. Tambem serve para "Zerar o gosto".
void    farolgosto_iniciar(FarolGosto *g, uint16_t perfil);

// Aplica um delta do evento a todas as dimensoes da ficha, no dia `dia`
// (dias desde 1/1/2026). Antes de somar, decai o peso guardado pela
// meia-vida da dimensao sem perder intervalo: cada toque aplica so os
// dezesseis-avos de meia-vida completos e guarda a fracao restante, entao
// sessoes diarias acumulam decaimento normal em vez de congela-lo.
// `faixaHora` 0-7 atualiza hora[faixaHora][faixa de duracao]; fora de faixa
// (ou duracao desconhecida) nao toca o bloco. delta == 0 nao toca as
// dimensoes.
//
// `obraNova` e o contrato do contador `titulos` (plano 3.4: nivel frio < 3,
// maduro >= 10 titulos com ficha) com a farolsinal (issue 38): quem chama
// deduplica pela CHAVE DA FICHA — farolficha_chave, que cobre tmdb, tipo e
// reserva e identifica tambem obra sem id do TMDB — e passa obraNova = 1
// somente no primeiro evento de cada obra. Quarenta episodios da mesma
// serie valem 1; tres fichas distintas, incluindo reserva, valem 3. A 38
// guarda ou reconstroi o conjunto de chaves ja vistas (o contador persistido
// no arquivo nao pode ser recomputado por aqui). Vale tambem com delta == 0:
// a obra tem ficha, so o sinal do evento e neutro.
void    farolgosto_aplicar(FarolGosto *g, const FarolGostoFicha *f,
                           int delta, uint16_t dia, int faixaHora,
                           int obraNova);

// As celulas de afinidade que farolgosto_aplicar toca para esta ficha, na
// MESMA ordem (generos deduplicados, pessoas, temas, decada, idioma,
// duracao): o estorno exato da 38 se alinha por esse indice. Devolve o n
// (<= max; o teto e FG_CEL_N).
#define FG_CEL_N 21
int     farolgosto_celulas(const FarolGostoFicha *f, int *dims, int32_t *ids,
                           int max);

// Devolve a contribuicao de UMA vez, celula a celula — o inverso exato de
// aplicar, para desmarcar/apagar sem residuo e sem subir evidencia
// (correcao FAROL38B). Para cada celula ainda presente, o peso perde o
// adendo saturado que a vez somou — delta*8/(antes+8), com
// antes = n_atual-1-depois[k]: depois[k] conta as vezes POSTERIORES a esta
// que tocaram a celula, entao creds ja despejados continuam contados — ja
// decaido ate o dia de referencia do slot, e uma evidencia sai de n. A
// celula de hora perde o delta cru, decaido ate o diaBase do bloco. Celula
// despejada e pulada (devolver nao recria slot). delta == 0 nao toca nada.
void    farolgosto_estornar(FarolGosto *g, const FarolGostoFicha *f, int delta,
                            const int *depois, uint16_t diaVez, int faixaHora);

// Peso projetado da afinidade (dim,id) no dia `dia`, em Q8.8 — o valor ja
// decaido, sem mexer no guardado. 0 se a afinidade nao existe.
int32_t farolgosto_peso(const FarolGosto *g, int dim, int32_t id, uint16_t dia);

// Valor projetado de hora[faixaHora][faixaDur] no dia `dia`, Q8.8.
int16_t farolgosto_hora(const FarolGosto *g, int faixaHora, int faixaDur,
                        uint16_t dia);

// "Nao e para mim": guarda o hash do titulo no anel (ignora h == 0 e
// duplicado). Recusado sai de todas as pecas (issue 41) e volta no arquivo.
void    farolgosto_recusar(FarolGosto *g, uint32_t h);
int     farolgosto_recusado(const FarolGosto *g, uint32_t h);
// Tira o hash do anel (desfazer "nao e para mim" tambem limpa a recusa —
// correcao FAROL38B). O slot volta a zero e entra de novo numa recusa futura.
void    farolgosto_desrecusar(FarolGosto *g, uint32_t h);

// Recusa passiva: o titulo apareceu na fileira do Farol no dia `dia` (0-255).
// `vezes` so conta dias diferentes. Anel de 160, sai o mais antigo.
void    farolgosto_mostrado(FarolGosto *g, uint32_t h, uint8_t dia);
int     farolgosto_mostrado_dias(const FarolGosto *g, uint32_t h);

// Serializa o modelo inteiro para `dst` (texto, uma linha por afinidade).
// Devolve o comprimento escrito, ou 0 se `cap` nao couber — o chamador nao
// grava arquivo nesse caso. Cada emissao usa snprintf, entao na pratica e
// preciso cap >= n+1: com cap == n a ultima linha nao cabe com o NUL e a
// funcao devolve 0. Escreve texto puro, sem NUL; o chamador fecha a string.
size_t  farolgosto_escrever(const FarolGosto *g, char *dst, size_t cap);

// Le o texto de volta. rc 0 = ok; -1 = cabecalho ausente ou versao
// desconhecida (o arquivo e ignorado e *g fica como estava). Linha torta e
// pulada. Escrever(g) -> Ler(g2) -> Escrever(g2) e byte a byte identico.
int     farolgosto_ler(FarolGosto *g, const char *txt, size_t len);

// "farol-gosto-p<perfil>.txt" em dst. rc 0 ok, -1 sem espaco.
int     farolgosto_nome(uint16_t perfil, char *dst, size_t cap);

#endif // NUVIO_FAROLGOSTO_H
