// FAROL — a cola entre a cena do gosto (farolcena/farolpeca/farolrec) e a
// Home (issue 45, PLANO-GOSTO 4.2 P1 e CONTRATO-HOME-POKE).
//
// O compositor (44) e puro e nao conhece SDL, catalogo nem a Home. Aqui fica
// a ADOCAO da cena: quem decide quando recompor (24 h ou evento forte, nunca
// com a Home na tela), quem transforma as pecas `fileira` em CatFileira e quem
// renderiza os textos FT_* com os argumentos resolvidos a nomes de verdade —
// uma peca cujo argumento nao resolve sai SEM a linha, nunca com id cru.
//
// CONTRATO (CONTRATO-HOME-POKE, "Respostas do Poke"):
//   - so as chaves farol_gosto1 e farol_gosto2 (prefixo CAT_PREFIXO_FAROL);
//   - entregues em filsLote ANTES de ordenarPorSnapshot, a cada volta;
//   - compositor falhou -> a fileira antiga NAO e reaproveitada;
//   - cat_ler_cache ja tira as farol_ do disco: so sobem se reentregues;
//   - remocao (sair da conta, zerar o gosto, desligar Recomendar) chama
//     cat_remover_fileiras_prefixo ANTES de a identidade/perfil mudar.
//
// FIO: farol_home_fileiras roda no fio da descoberta, dentro de montar();
// farol_motivo e lido no fio de desenho; as remocoes vem do fio principal.
// A cena adotada fica sob `travaCena` e NENHUMA funcao de outro modulo e
// chamada com ela tomada (ordem de travas: copia local primeiro, modulo
// depois).
#ifndef NV_FAROL_H
#define NV_FAROL_H
#include "catalogo.h"          // CatItem, CatFileira, CAT_PREFIXO_FAROL
#include "farolregra.h"        // FarolFatos, FarolVeredito
#include "farolanel.h"         // FarolEvento, FAROL_ANEL_N

// Itens que a Home ganha no lote numa volta: 2 fileiras x ate 12 cartazes.
#define FAROL_HOME_ITENS 24

// 1 = chave reservada do Farol (comeca por "farol_").
int farol_fileira(const char *chave);

// ENTREGA A CADA VOLTA, antes de ordenarPorSnapshot. Escreve ate 2 CatFileira
// em fils[] (fils[*nFils..]) e copia os itens para o fim do lote (lote[*n..],
// respeitando cap). Sem cena adotada ou sem peca elegivel, nao escreve nada.
// `fils` tem capFils vagas no total.
void farol_home_fileiras(CatItem *lote, int *n, int cap,
                         CatFileira *fils, int *nFils, int capFils);

// A linha de motivo do cartaz em foco: `coluna` e a posicao dentro de
// itens[] da fileira entregue e `imdb` o id do cartaz PUBLICADO nela — o
// motivo so sai se o imdb bate com o item da cena adotada (a fileira na
// tela pode ser de uma cena anterior). Ponteiro para buffer estatico —
// consumir no ato (o desenho faz exatamente isso). NULL = item sem motivo
// verdadeiro ou imdb divergente.
const char *farol_motivo(const char *chaveFil, int coluna,
                         const char *imdb);

// A geracao da cena adotada: sobe a cada adocao e a cada invalidacao. A
// Home cacheia o motivo por ela e nao toma trava por quadro.
unsigned farol_cena_geracao(void);

// 1 quando a fileira do Farol pode estar publicada: "Recomendar nesta TV"
// ligado e modo seguro fora. A descoberta confere de novo no ponto de
// publicacao e na remontagem sem rede — um opt-out no meio da montagem nao
// republica as fileiras farol_ (revisao FAROL45 r1).
int farol_ativo(void);

// O titulo da fileira adotada, ja renderizado e localizado (para quem precisar
// dele fora do CatFileira; a Home le o dela). "" se a fileira nao existe.
const char *farol_fileira_titulo(const char *chaveFil);

// REMOCAO (contrato): tira as fileiras farol_ do snapshot do perfil atual,
// na hora, e esquece a cena adotada. CHAMAR ANTES de a identidade/perfil
// mudar — depois, o cache regravado cairia no perfil novo.
// `dirArte` segue o de cat_ler_cache; NULL usa dados_dir().
void farol_remover_fileiras(const char *dirArte);

// "Zerar/apagar o gosto": remove as fileiras com a identidade atual e depois
// apaga o gosto aprendido (farolsinal, dono do modelo). NESTA ordem, sempre.
void farol_gosto_zerar(const char *dirArte);

// A epoca do gosto do perfil: sobe a cada "Zerar o gosto". A descoberta
// carimba a emissao na CatFileira (farolSelo) e confere na publicacao e na
// remontagem — o que foi montado antes do reset nao republica (FAROL45C).
unsigned farol_selo(void);

// "Recomendar nesta TV": ao DESLIGAR, remove as fileiras com a identidade
// atual antes de registrar a preferencia — ligar nao toca no catalogo.
void farol_recomendar_definir(int ligado, const char *dirArte);

// "Nao e para mim": tira `imdb` de todas as pecas do Farol na hora (cena
// adotada + fileiras farol_* no catalogo) e guarda retrato para Desfazer.
int  farol_recusar_titulo(const char *imdb);
int  farol_restaurar_titulo(const char *imdb);

// Top `max` nomes do gosto ativo para uma dimensao (FG_DIM_GENERO,
// FG_DIM_PESSOA ou FG_DIM_TEMA), ja traduzidos/resolvidos (sem id cru).
int  farol_gosto_top(int dim, char (*nomes)[64], int max);

// Esquece so a cena adotada e o estado diagnostico da sessao (o catalogo ja
// foi tratado por quem chamou). Troca de conta/perfil usa isto: a cena e o
// diagnostico do perfil anterior nao podem vazar para o proximo.
void farol_esquecer(void);

// Relogio injetado para teste: `s` devolve epoch em segundos (NULL = time()).
void farol_teste_relogio(long long (*s)(void));
// Relogio de visibilidade da Home para teste: `v` 1 = Home na tela.
void farol_teste_home_visivel(int (*v)(void));

// --- Diagnostico e veredito (issue 14, PLANO Onda 2, ADENDO 2.1 C) ---------
// Contratos:
//   - Zero rede; leitura puramente em memoria / cache / anel (< 1 ms).
//   - Fio: chamar farol_fatos_ler, farol_julgar e farol_veredito no fio principal
//     (ou usar farol_veredito_copiar fora dele). Nenhum outro modulo e chamado
//     com travas internas do Farol tomadas.
//   - UI/navegacao em resolver.c pertencem a #19; acoes/envio a #15-#18, #20,
//     #33, #34; redator/montador de evidencias a #11 (preserva ate 256 eventos).

// Coleta todos os fatos atuais do app, reproducao, audio, streaming e anel
// em `*out` (sem rede, < 1 ms). Respeita fatos fixos se RESOLVER_TESTE ativo.
void farol_fatos_ler(FarolFatos *out);

// Le os fatos, julga `sintoma` (RES_S_*), guarda o veredito da sessao e
// registra FE_FAROL_VEREDITO no anel. Devolve o codigo FAROL_C_* resultante.
int  farol_julgar(int sintoma);

// Ultimo veredito julgado na sessao (NULL se nenhum ainda ou apos reset).
// Consumir no fio principal; para copia segura entre fios use
// farol_veredito_copiar().
const FarolVeredito *farol_veredito(void);
int  farol_veredito_copiar(FarolVeredito *veredito, FarolFatos *fatos);

// Registro dos demais eventos FE_FAROL_* no anel com token saneado.
void farol_registrar_acao(int acao);
void farol_registrar_resultado(int acao, int ok);
void farol_registrar_envio(int http);
void farol_registrar_consertou(int acao);

// Copia ate `max` eventos mais recentes (ate FAROL_ANEL_N = 256) em ordem
// cronologica para alimentar o montador de evidencias (#11).
int  farol_eventos(FarolEvento *dst, int max);

// Limpa o veredito guardado e avanca a fronteira de falhas consecutivas para
// uma nova sessao sem apagar os 256 eventos recentes do anel.
void farol_sessao_reiniciar(void);

#ifdef RESOLVER_TESTE
// Fixa os fatos devolvidos por farol_fatos_ler() e usados em farol_julgar().
// Passar NULL restaura a coleta ao vivo.
void farol_teste_fatos(const FarolFatos *f);
#endif

#endif
