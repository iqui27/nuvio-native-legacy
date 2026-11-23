// FAROLCENA — compositor deterministico do FarolCena v1 (lote sense,
// issue 44; PLANO-GOSTO secoes 3.4, 3.5, 4.2, 4.3 e 5.2).
//
// O que este modulo faz: monta a CENA que o validador da 39 (farolpeca_ler)
// aceita, usando so regras locais — sem rede, sem LLM, sem I/O. Tres cenas:
//
//   home      P1 (ate 2 fileiras) + P2 (1 dica) — PLANO-GOSTO 4.3
//   detalhe   P3 (porque) + P6 (info) — um bloco, idem
//   comparar  P4 com o veredito das regras ("maior pontuacao que cabe no
//             tempo", 5.2)
//
// Quem chama ja tem os candidatos nas fontes da 3.1 (catalogo, recs das
// sementes, pessoa, salvos, amigos, recs recebidas, clima), o modelo do
// perfil (farolgosto, 37) e os fatos da P6 (47). A pontuacao, a diversidade
// e os MOTIVOS sao do farolrec (41) — as regras de verdade da 3.5 moram la
// e nao sao duplicadas aqui: este modulo so decide quais motivos viram
// linha e como os argumentos sao codificados.
//
// CONTRATO DE ARGUMENTOS (os "a[]" dos textos de modelo):
//   "c<n>"        indice na lista de candidatos da cena (itens, alvo,
//                 escolha e chaves de motivo; resolve contra nCand — o
//                 mesmo numero que o chamador passa a farolpeca_ler)
//   "tt<digitos>" id IMDb (titulos vistos: sementes e alvos de detalhe)
//   "p<id>"       pessoa (id TMDB; < 0 = hash de nome, convencao farolficha)
//   "k<id>"       tema/palavra-chave (idem)
//   "g<id>"       genero (idem)
//   "u<id>"       amigo/indicante — resolve pela lista de amigos do app
//   "<n><u2>"     tempo decorrido codificado como farolinfo_retomada:
//                 u2 em {d,w,M,y} ("5M" -> "5 meses"); camada de texto
//                 localiza — aqui nunca entra frase pronta nem nome.
// Os demais argumentos sao numeros ("2","4","137") ou valores curtos ja
// medidos pelo chamador (hora "23:40", formato "Dolby Vision").
//
// SAIDA E CANONICA: chaves na ordem fixa do envelope da secao 4.1, sem
// espacos, tipos na ordem [fileira, fileira, dica] / [porque, info] /
// [comparar]. Mesma entrada + mesma semente = mesma cena, byte a byte.
// A cena NUNCA tem texto livre "x" (ele e so do LLM; as regras emitem
// sempre {"m","a"} — e nunca emitem virgula final: o JSON sai por
// construcao, nao por varredura).
//
// NENHUMA PECA SEM MOTIVO VERDADEIRO LEVA LINHA DE MOTIVO: motivo que o
// farolrec emitiu mas cujo argumento nao pode ser produzido com dado real
// (semente sem referencia, salvo sem data) e omitido — nunca sai frase
// generica no lugar (3.5). Linha da farolinfo cujo modelo a 35 ainda nao
// tem (FALTA_TEXTO) tambem e omitida, porque subi-la quebraria a cena no
// validador.
//
// PURO como os irmaos: sem malloc, sem SDL, sem rede, sem dados.h, sem
// relogio (dia/faixaHora/feita entram no contexto) e sem log. Memoria de
// trabalho na pilha da CHAMADA (~42 KB no pior caso: a farolrec_escolher
// ja detem ~38 KB — ressalva da revisao FAROL39 r3: nao chamar no fio da
// UI; o local certo e o passo ocioso/momentoBom, como manda a 4.4).
#ifndef NV_FAROLCENA_H
#define NV_FAROLCENA_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>
// COLISAO DE NOME nos irmaos: farolrec.h e farolpeca.h definem dois
// `FarolMotivo` diferentes — o do motor e {tipo,a,b} (3.5), o da peca e
// {item, FarolTexto} (4.2). A issue 44 consome os dois SEM altera-los,
// entao este cabecalho entra com o do motor rebatizado `FarolRecMotivo`
// apenas nesta unidade de traducao. Preco: INCLUA farolcena.h ANTES de
// qualquer include direto de farolrec.h — se o motor ja entrou com o
// nome curto, o erro de redefinicao aparece na hora, nada quebra calado.
// O do validador segue chamando `FarolMotivo`, como sempre.
#define FarolMotivo FarolRecMotivo
#include "farolrec.h"     // FarolRecCand, FarolRecSemente, FarolRecItem
#undef FarolMotivo
#include "farolinfo.h"    // FarolInfoFonte/Caps/Temp/Amigo (47)
#include "farolpeca.h"    // FP_*, limites do envelope (39)

// ------------------------------------------------------------ contexto
// Tudo injetado. `sementeRef` e paralelo a `sementes`: a ref ("tt..." ou
// "c<n>") que nomeia aquele titulo visto nos argumentos ("Porque voce
// terminou X"); NULL ou entrada vazia = a semente nao e nomeavel e o
// motivo TERMINOU dela nao sai (regra "sem motivo verdadeiro").
// `salvoDia` e paralelo a `cand`: o dia em que o titulo entrou nos salvos
// (0 = sem dado -> o motivo SALVO nao sai). NULL = nenhum salvo tem data.
typedef struct {
  const FarolGosto *g;                 // modelo do perfil; NULL = frio 0
  const FarolRecCand *cand;            // candidatos da cena; "c<n>" indexa
  int nCand;
  const FarolRecSemente *sementes;     // sementes vistas (farolrec)
  int nSementes;
  const char *const *sementeRef;       // [nSementes] ref ou NULL
  const uint16_t *salvoDia;            // [nCand] ou NULL
  uint16_t dia;                        // hoje, dias desde 1/1/2026
  uint8_t  faixaHora;                  // 0..FG_HORA_F-1
  uint8_t  fimSemana;                  // 1 = sab/dom (titulo "fim de semana")
  int32_t  perfil;                     // < 0 = omite o campo
  int64_t  feita;                      // epoch da cena (0 = omite)
  int64_t  valeS;                      // vale_s; <= 0 = omite
  uint32_t semente;                    // sorteio deterministico (desempate)
} FarolCenaCtx;

// ------------------------------------------------------- dicas (P2)
// O chamador ja aplicou os limites da casa (1 por dia por perfil, id
// dispensada em avisodisp) e passa so os fatos elegiveis com os argumentos
// prontos. O compositor escolhe UMA pela ordem de tipos (a prioridade e a
// ordem do enum: retomada/continuidade primeiro, generica por ultimo) e
// confere a aridade de cada argumento contra o modelo — fato com numero
// errado de argumentos e ignorado, nunca monta frase torta.
//
//   tipo        l1 (modelo/args)             l2 (opcional)       acao
//   PAROU       FT_DICA_PAROU_L1 [ref,T,E]   PAROU_L2 [semanas]  abrir
//   FALTA       FT_DICA_FALTA_L1 [n,T,ref]   FALTA_L2 []         abrir
//   ESTREIA     FT_DICA_ESTREIA   [p<id>]    -                   abrir
//   TEMPORADA   FT_DICA_TEMPORADA [ref]      -                   abrir
//   SALVO       FT_DICA_SALVO_L1  ["<n><u>"] SALVO_L2 [duracao]  abrir
//   REVER       FT_DICA_REVER     [ref]      -                   abrir
//   CURTO       FT_DICA_CURTO     [ref]      -                   abrir/tocar
//   PERGUNTAS   FT_DICA_PERGUNTAS []         -                   escolha
//   TRAKT       FT_DICA_TRAKT     []         -                   nada
// Frio 0 aceita so PERGUNTAS e TRAKT (3.4: o assistente e a porta); os
// demais tipos sao descartados antes da escolha.
enum {
  FC_DICA_PAROU, FC_DICA_FALTA, FC_DICA_ESTREIA, FC_DICA_TEMPORADA,
  FC_DICA_SALVO, FC_DICA_REVER, FC_DICA_CURTO, FC_DICA_PERGUNTAS,
  FC_DICA_TRAKT, FC_DICA_N
};

typedef struct {
  int tipo;                            // FC_DICA_*
  int acao;                            // FP_ACAO_*
  const char *id;                      // id estavel ("d_..."), p/ dispensar
  const char *alvo;                    // "tt.."/"c<n>"; "" p/ escolha/nada
  const char *l1a[FAROL_ARGS];         // argumentos da l1, na ordem
  int l1na;
  const char *l2a[FAROL_ARGS];         // argumentos da l2; l2na=0 -> sem l2
  int l2na;
} FarolDicaFato;

// ------------------------------------------------------- detalhe (P3+P6)
// `alvo` e o titulo da pagina resumido como candidato (ficha, flags e
// fontes da 3.1 — o motor aplica nele as mesmas regras de verdade da 3.5,
// inclusive "visto/recusado/abandono recente nao recomenda"). `alvoRef` e
// a ref que nomeia o titulo ("tt..." ou "c<n>" se ele tambem e candidato).
// Visto (FRC_FL_VISTO) -> sem P3 (a linha "rever" e da P6). Frio 0 -> sem
// P3. A P6 sai em qualquer nivel: sao fatos, nao gosto.
typedef struct {
  FarolRecCand alvo;
  const char *alvoRef;
  uint16_t salvoDia;                   // dia em que o alvo foi salvo (0=n/a)
  // entradas da farolinfo (47): NULL/0 desliga a linha correspondente
  const FarolInfoFonte *fontes; int nFontes;   // qualidade
  FarolInfoCaps caps;                          // o que a TV toca (-1 = ?)
  const FarolInfoTemp *temps; int nTemps;
  int faltam, minEp;                           // continuar
  const struct tm *agora; int h12;             // tempo (relogio injetado)
  int minutos;                                 // duracao p/ "termina as"
  int minNoite;                                // habito da faixa (0 = s/d)
  const char *onde;                            // onde ("Netflix", NULL)
  const FarolInfoAmigo *amigos; int nAmigos;   // amigos
  int sagaPos, sagaTotal, sagaVistos;          // saga
  int reverAno, reverMes, reverNota10;         // rever
  int retT, retE; long long retQuando;         // retomada (agora=feita)
} FarolCenaDet;

// ------------------------------------------------------- comparar (P4)
// 2 ou 3 colunas, refs na ordem da tela. `pontos` e o Q15 do motor (ou do
// LLM); `minutos` 0 = duracao desconhecida; `tempoMin` = quanto a pessoa
// tem agora (0 = sem dado). Veredito das regras (5.2): a coluna de maior
// pontuacao que CABE no tempo ganha "escolha" e o FT_CMP_VEREDITO_CABE.
// Nenhuma cabendo (ou tempo desconhecido): escolha = a de maior pontuacao
// e NENHUM veredito — afirmar "cabe" sem dado e mentira (mesma regra de
// alma da 3.5). `eixos` e a lista fechada da 4.2, na ordem das linhas.
typedef struct {
  const char *itens[3]; int nItens;            // refs, 2..3, sem repetir
  int32_t pontos[3];                           // Q15 por coluna
  uint16_t minutos[3];                         // 0 = desconhecida
  int tempoMin;                                // 0 = sem dado
  uint8_t eixos[FAROL_EIXO_MAX]; int nEixos;   // FP_EIXO_*, >= 1
} FarolCenaCmp;

// ------------------------------------------------------------- funcoes
// Cada uma escreve a cena canonica (UTF-8, compacta, <= 8 KB) em `dst` e
// devolve o comprimento (sem o NUL), ou -1 com argumento invalido /
// `cap` insuficiente para o envelope. Cena sem peca elegivel sai com
// "pecas":[] — JSON valido e deterministico, nao erro.
//
// Toda cena produzida passa em farolpeca_ler com nRecusadas == 0 — e' a
// propriedade que o teste confere em cada saida, inclusive em perfis
// aleatorios.
int farolcena_home(char *dst, size_t cap, const FarolCenaCtx *ctx,
                   const FarolDicaFato *dicas, int nDicas);
int farolcena_detalhe(char *dst, size_t cap, const FarolCenaCtx *ctx,
                      const FarolCenaDet *det);
int farolcena_comparar(char *dst, size_t cap, const FarolCenaCtx *ctx,
                       const FarolCenaCmp *cmp);

#endif // NV_FAROLCENA_H
