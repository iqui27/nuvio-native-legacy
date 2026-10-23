// COPIA LOCAL DO QUE A CONTA DEVOLVEU POR ULTIMO, para o app sobreviver ao
// servidor da conta fora do ar.
//
// MEDIDO em 02/10/2026 ~13h BRT: api.nuvio.tv devolveu 504 em /rest/v1 (e
// depois 502 em tudo) por horas. A lista de addons so existia na memoria, vinda
// da conta a cada arranque; sem resposta, "[sync] leitura de addons: HTTP 504",
// "[desc] 0 catalogos declarados pelos addons" e a home abria vazia — em todo
// aparelho, de toda pessoa (#215). Quem tentou "sair e entrar de novo" para
// consertar ficou sem conta nenhuma, porque o login fala com o mesmo servidor.
//
// O que fica aqui: a ULTIMA resposta 2xx de cada superficie, crua, por CONTA
// (o `sub` do JWT) e por PERFIL, num envelope versionado. Quem le confere o
// dono e o perfil, como catordemcache.c (que continua sendo o cache da ordem
// de catalogos). Gravacao atomica (dados_gravar). Apagado no logout.
//
// CONTEUDO SENSIVEL: a resposta da tabela `addons` traz as URLs com as chaves
// de debrid embutidas. Por isso contacache_esquecer e chamado JUNTO com o resto
// de sync_esquecer_usuario, e o arquivo nunca vai para log.
#ifndef NV_CONTACACHE_H
#define NV_CONTACACHE_H

// Superficies guardadas. O nome entra no nome do arquivo.
#define CC_ADDONS_APLICADOS "addons-aplicados"
#define CC_ADDONS_TOGGLES "addons-toggles"
#define CC_ADDONS     "addons"
#define CC_COLECOES   "colecoes"
#define CC_BIBLIOTECA "biblioteca"
#define CC_VISTOS     "vistos"

// Guarda `corpo` (a resposta crua) como a copia desta superficie. 1 se gravou.
int   contacache_gravar(const char *superficie, int perfil, const char *usuario,
                        const char *corpo);
// Uma resposta iniciada antes do logout nao pode recriar os arquivos apagados.
unsigned contacache_geracao(void);
int   contacache_gravar_geracao(const char *superficie, int perfil,
                               const char *usuario, const char *corpo,
                               unsigned geracao);

// Le a copia, se existir e for desta conta e deste perfil. Devolve o corpo
// (free pelo chamador) ou NULL. `quando` recebe o instante da gravacao (epoch).
char *contacache_ler(const char *superficie, int perfil, const char *usuario,
                     long *quando);

// Apaga as copias de todas as superficies e perfis. Logout.
void  contacache_esquecer(void);

// "02/10 13:05" para o log e o resumo. "?" quando `quando` e 0.
void  contacache_data(long quando, char *dst, unsigned tam);

// A falha e do SERVIDOR e passa sozinha: sem resposta (0), 429 ou 5xx. So
// nesses casos a copia substitui a resposta — um 4xx diz que o PEDIDO esta
// errado, e esconder isso atras de uma copia velha esconderia o defeito.
int   contacache_falha_transitoria(int status);

#endif
