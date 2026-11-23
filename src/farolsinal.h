// SINAIS DO FAROL — o tradutor da tabela 2.2 (lote sense, issue 38;
// PLANO-GOSTO secoes 1.3 e 2.2). Liga os sinais que ja existem no app ao
// modelo farolgosto: cada evento (terminou, gostou, reviu, abandonou cedo,
// salvou, tirou do Continuar, "mais assim", "nao e para mim", serie largada,
// gesto de visto/desvisto) vira UM delta que se espalha pelas dimensoes da
// ficha do titulo (farolficha, issue 36).
//
// O que NAO pesa (secao 1.3): sono confirmado ou suspeito, parada antes de 1
// min, abandono com FE_ERRO_FONTE/FE_PARADO nos 5 min anteriores, evento de
// perfil diferente do carregado, e tudo enquanto "Recomendar nesta TV" estiver
// desligado. Desmarcar visto desfaz o que a vez somou.
//
// FICHA QUE FALTA vai para a fila de pendencias (32, persistida em
// farol-sinal-p<n>.txt): o evento guarda chave, delta e instante; quando a
// ficha chega — pelo cache LRU, pelo catalogo ou pela fila lenta — o delta
// esperado aplica. A CHAVE normaliza os dois mundos da ressalva do lote: o
// "tt..." sem detalhe gera a reserva negativa que farolficha_chave produz
// para o mesmo CatItem; quando o detalhe do TMDB chega com tmdb>0 a chave
// real e outra e um alias liga as duas — nem pendente eterna, nem peso
// dobrado.
//
// FIOS. Um mutex unico fecha modelo, cache de fichas, filas e arquivo
// farol-fichas.bin (o leitor da 36 apaga duplicata corrompida: leitura e
// escrita nunca cruzam sem a trava — ressalva do lote). Ouvintes e sinais
// chegam no fio principal; a fila lenta roda num fio proprio que so entra
// aqui por farolsinal_ficha_ok/falhou, e as conferidas de temporada, a
// varredura de serie-largada e as gravacoes rolam no fio de tras que o
// passo acorda. Nada dentro da trava chama o diario: leituras de
// diario/progresso acontecem antes ou depois do bloco fechado, senao o fio
// dos ouvintes do diario (diario->sinal) cruzava com sinal->diario e
// travava os dois.
//
// PRIVACIDADE (ALMA.md): nenhum titulo, nome de perfil ou gosto em log ou
// arquivo — so ids, chaves e numeros. O arquivo por perfil e texto curto
// (farol-sinal-p<n>.txt) ao lado do modelo farol-gosto-p<n>.txt.
#ifndef NV_FAROLSINAL_H
#define NV_FAROLSINAL_H
#include <stddef.h>
#include <stdint.h>
#include "catalogo.h"            // CatItem
#include "diario.h"              // DIARIO_FILME/DIARIO_SERIE na saida do player
#include "farolficha.h"          // FarolFicha
#include "farolgosto.h"          // FarolGosto, FG_D_*

// Origem de um Play, para a medida da recomendacao (secao 2.3). Quem chama
// farolsinal_origem diz de onde o cartaz saiu.
enum {
  FS_ORIGEM_NENHUMA,
  FS_ORIGEM_FAROL,        // fileira montada pelo gosto
  FS_ORIGEM_BUSCA,        // busca manual
  FS_ORIGEM_CATALOGO,     // fileira normal do catalogo
  FS_ORIGEM_SIMILAR,      // "parecidos" da pagina do titulo
  FS_ORIGEM_N
};

// --- ciclo de vida -----------------------------------------------------------
// Depois de dados_iniciar: registra ouvintes (diario, visto, salvos,
// progresso) e carrega o modelo do perfil ativo. Sem ela o modulo acorda do
// mesmo jeito no primeiro sinal (o estado carrega sob demanda).
void farolsinal_iniciar(void);
// Laco principal, uma vez por quadro (ou por segundo): reconcilia troca de
// perfil e varre o diario depois de RECARREGADO em FATIAS de teto rigido
// (cursor composto, nunca uma leva inteira de mesmo instante no quadro).
// `momentoBom` e o mesmo do caca_passo (!player_aberto()): a varredura —
// inclusive o aquecimento de ate 400 titulos do primeiro uso — e a fila
// lenta de fichas so andam sem player aberto (issue 43, secao 4.4).
// Serie largada, conferidas de temporada e gravacao do modelo com folga de
// 60 s rolam num fio de tras que o passo so acorda — disco e conferidas
// nao voltam ao fio chamador. Custo ocioso: comparacoes e nada mais.
void farolsinal_passo(int momentoBom);
// Saida do app: grava modelo, estado e cache de fichas.
void farolsinal_sair(void);
// Logout/reset da CONTA: apaga modelo, estado e pendencias de TODOS os
// perfis (a conta pode ter varios e o logout e dela, nao do perfil ativo),
// TODA marca "notas do Trakt ja lidas" — o esquecer roda depois de
// sessao_sair(), que ja apagou o usuario do nome — e o cache de fichas do
// aparelho. A conta dona dos arquivos, como o diario. Chamada por
// sync_esquecer_usuario junto de diario_esquecer/mapa_esquecer (issue 43,
// secao 2.3).
void farolsinal_esquecer(void);
// "Zerar/apagar o gosto" do perfil ATIVO (farol_gosto_zerar, contrato da
// issue 45): modelo, aneis e pendencias zeram, e os arquivos do perfil no
// disco somem para a proxima carga nao reviver o gosto velho. O cache de
// fichas (metadado do TMDB, nao gosto) e os demais perfis ficam — o
// esquecer da CONTA inteira e farolsinal_esquecer.
void farolsinal_zerar_gosto(void);
// O backup por aparelho restaurou os arquivos por baixo: agenda uma troca
// para o proprio perfil que so LE o disco — o modelo em RAM nao grava por
// cima do recem-restaurado antes (issue 43). Sem efeito com troca em curso.
void farolsinal_recarregar(void);
// O backup por aparelho vai regravar os arquivos por baixo: marca a
// releitura ANTES da escrita para que nem a gravacao de folga nem o
// instantaneo descarreguem a RAM velha por cima do recem-restaurado no
// intervalo (issue 43). O aplica-restauracao chama antes de gravar cada
// arquivo do Farol e farolsinal_recarregar() ao fim.
void farolsinal_restaura_marcar(void);
// Forca a gravacao do que a folga de 60 s ainda nao desceu — o instantaneo
// de backup le os arquivos do disco e quer o estado de agora (issue 43).
void farolsinal_gravar(void);
// Ajuste "Recomendar nesta TV" (Ajustes > Farol > Gosto, issue 51): desligado
// zera coleta — sinais novos sao ignorados — mas nao apaga o modelo; ligar
// de volta continua de onde parou. Padrao: ligado.
int  farolsinal_recomendar(void);
void farolsinal_definir_recomendar(int ligado);

// --- sinais (fio principal) --------------------------------------------------
// Saida do player, chamada por diariofontes_player_saiu com o que a sessao
// sabia (id do titulo sem ":T:E", tipo DIARIO_*, t/e, tmdb quando o catalogo
// tinha). `concluiu` = chegou no fim — a entrada do diario e quem credita
// esse caso; aqui so entra o abandono cedo (>= 60 s e < 20% de dur): -0,8,
// exceto com sono suspeito/confirmado ou falha de fonte nos ultimos 5 min.
void farolsinal_saida(const char *id, char tipo, int temporada, int episodio,
                      long tmdb, double posSeg, double durSeg, int concluiu);
// Tempo no detalhe > 20 s ou trailer ate o fim: +0,2. Uma vez por titulo/dia.
void farolsinal_interesse(const CatItem *ci);
// "Mais assim" no menu do cartaz: +1,5. Uma vez por titulo.
void farolsinal_mais_assim(const CatItem *ci);
// "Nao e para mim": -2,0 e o titulo entra no anel de recusados (sai de todas
// as pecas — issue 41 le farolgosto_recusado). Uma vez por titulo.
void farolsinal_recusar(const CatItem *ci);
// Desfaz a recusa: devolve o que a recusa tirou. O anel de recusados nao tem
// "desrecusar" (e um anel), mas a dimensao volta.
void farolsinal_desfazer_recusa(const CatItem *ci);
// O titulo apareceu numa fileira do Farol: marca no anel de mostrados (a
// recusa passiva — mostrado 3 vezes sem abrir — e da issue 41/45, aqui so a
// marcacao). Sem delta.
void farolsinal_mostrado(const CatItem *ci);
// De onde veio o Play (medida da recomendacao): anel de 64 origens com hash
// do titulo e dia. Quem montou o cartaz diz a origem.
void farolsinal_origem(const CatItem *ci, int origem);

// --- fila lenta de fichas ----------------------------------------------------
// A ficha chegou (ou nao) para a chave pedida. `chavePed` e a chave que foi
// para o pedido — a da reserva ou a do TMDB; se a ficha trouxe chave real
// diferente, um alias passa a ligar as duas. Seguro de qualquer fio.
void farolsinal_ficha_ok(int64_t chavePed, const FarolFicha *f);
// O pedido falhou de vez (sem detalhe, sem rede na 2a tentativa): as
// pendencias da chave sao soltas — um sinal novo as recria se fizer falta.
void farolsinal_ficha_falhou(int64_t chavePed);
// Transporte da fila lenta: a integracao injeta o pedido ao mapa/TMDB
// (issue 43). fn(idOuNulo, tmdbOu0, serie, dst, cap) devolve bytes do corpo
// JSON (>0) ou 0. Sem transporte registrado as pendencias so esperam ficha
// vinda por outros caminhos (cache, catalogo, ficha_ok direto).
typedef int (*FarolSinalBusca)(const char *id, long tmdb, int serie,
                               char *dst, size_t cap);
void farolsinal_busca(FarolSinalBusca fn);
// Quantas pendencias esperam ficha agora (0..32).
int  farolsinal_pendencias(void);

// --- notas antigas do Trakt (issue 43, secao 1.3) ---------------------------
// /sync/ratings, UMA leitura por conta, no fio do Trakt — nunca no da UI.
// quer() devolve 1 enquanto a leitura ainda nao aconteceu (a marca
// farol-notas.txt nao existe); notas() recebe o corpo e, se o perfil que
// pediu ainda e o residente, aplica as notas como sinais e grava a marca.
// Uma nota so aquece titulo que o modelo ainda nao conhece: <=3 vira -2,0,
// >=7 vira +2,0, 4-6 nao pesa. Perfil trocado no meio da viagem: a entrega
// e descartada sem marca e a volta seguinte tenta de novo.
int  farolsinal_trakt_quer(void);
void farolsinal_trakt_notas(int perfil, const char *json);

// --- leitura (medida e issues seguintes) -------------------------------------
// Copia o modelo do perfil carregado (a 41 pontua com ele).
int  farolsinal_modelo(FarolGosto *dst);
// Quantas origens de Play o anel guarda; copia em `h/o/d` ate `max`.
int  farolsinal_origens(uint32_t *h, uint8_t *o, uint8_t *d, int max);
// A ficha ja conhecida do item: a da LRU (detalhe do TMDB, com tema e
// pessoas reais) antes de qualquer reconstrucao de catalogo. So leitura —
// nao enche a LRU com a ficha pobre de CatItem. A cola da Home (farol.c)
// usa para dar tema a segunda fileira. 1 = `dst` recebeu a ficha.
int  farolsinal_ficha_do_item(const CatItem *ci, FarolFicha *dst);

// --- teste -------------------------------------------------------------------
// Relogio injetavel: s devolve epoch em segundos, ms os milissegundos do anel
// (mesma base de farolanel). NULL volta ao relogio real.
void farolsinal_relogio(long long (*s)(void), uint32_t (*ms)(void));
// Pasta dos arquivos (padrao dados_dir()). Chamar antes do primeiro sinal.
void farolsinal_pasta(const char *dir);
// Esquece tudo em memoria como num reboot (sem tocar arquivos nem ouvintes).
void farolsinal_teste_zera(void);
// 1 enquanto houver trabalho de fundo pendente ou em curso (varredura,
// conferida, largada ou gravacao delegada). Para testes esperarem o fio.
int  farolsinal_teste_trabalho(void);

#endif // NV_FAROLSINAL_H
