// ORGANIZAR OS SALVOS: categorias da pessoa e o jeito de ver o painel.
//
// O pedido (dono, 01/10/2026): no painel de Salvos/Social (salvospainel.c) a
// pessoa cria CATEGORIAS proprias ("Fim de semana", "Kids"), move titulos
// salvos para elas e escolhe como a lista e ordenada, agrupada e desenhada.
//
// POR QUE UM MODULO A PARTE, e nao colunas novas em salvos.txt. A lista de
// salvos e a UNIAO de tres fontes (lista local, watchlist do Trakt, biblioteca
// da conta — ver salvos.h), e so a primeira mora num arquivo nosso. Um titulo
// que veio do Trakt tambem pode ir para "Kids", e nao ha linha de salvos.txt
// para ele. Entao a categoria e um MAPA titulo -> categoria, ao lado da lista,
// e salvos.txt continua exatamente como era: nenhuma versao anterior do app
// le um campo que nao conhece, e o sync de salvos nao muda em nada.
//
// POR PERFIL. "Kids" e a organizacao de quem esta no sofa, e a TV de sala tem
// varios perfis: o arquivo e salvos-org-p<perfil>.txt (o mesmo esquema da
// agenda), relido quando perfis_ativo() muda.
//
// O FORMATO E VERSIONADO E TOLERANTE: "# nuvio salvos-org v1" na primeira
// linha, uma chave por linha separada por TAB, e chave desconhecida e pulada.
// Uma versao futura que acrescente chave nao invalida o arquivo de quem ja usa,
// e uma versao anterior que leia um arquivo novo so ignora o que nao conhece.
// Gravado por dados_gravar (temporario + rename).
//
// FIO PRINCIPAL apenas.
#ifndef NV_SALVOSORG_H
#define NV_SALVOSORG_H

// Ordem dentro de cada grupo. A PRIMEIRA E A DE SEMPRE (a ordem em que a lista
// ja aparecia: a local na ordem de insercao, depois o que so o catalogo tem),
// para que ninguem abra o painel depois de atualizar e ache tudo embaralhado.
enum { SORG_ORDEM_SALVOU = 0, SORG_ORDEM_RECENTES, SORG_ORDEM_NOME,
       SORG_ORDEM_ANO, SORG_ORDEM_NOTA, SORG_ORDEM_RESTANTE, SORG_ORDEM_N };
// Grupos. O padrao e o de sempre: "Continuar" em cima, "Não começados" embaixo.
enum { SORG_GRUPO_PROGRESSO = 0, SORG_GRUPO_TIPO, SORG_GRUPO_CATEGORIA,
       SORG_GRUPO_NENHUM, SORG_GRUPO_N };
enum { SORG_ESTILO_LISTA = 0, SORG_ESTILO_GRADE, SORG_ESTILO_PAISAGEM,
       SORG_ESTILO_N };
// Aba Social: as recomendacoes na ordem de chegada (padrao) ou por pessoa.
enum { SORG_SOCIAL_RECENTES = 0, SORG_SOCIAL_PESSOA, SORG_SOCIAL_N };

#define SORG_CAT_MAX   32
#define SORG_NOME_MAX  32   // bytes, com o NUL; o teclado limita a 24 letras

// Le o arquivo do perfil ativo se ainda nao leu ou se o perfil mudou. Barata
// quando nada mudou (uma comparacao de inteiro): pode ser chamada por quadro.
void sorg_carregar(void);
void sorg_recarregar(void);
// Sobe a cada mudanca (leitura, preferencia, categoria, mover). O painel
// reorganiza por ela, como faz com salvos_revisao.
unsigned sorg_revisao(void);

int  sorg_ordem(void);
int  sorg_grupo(void);
int  sorg_estilo(void);
int  sorg_social(void);
void sorg_definir_ordem(int v);
void sorg_definir_grupo(int v);
void sorg_definir_estilo(int v);
void sorg_definir_social(int v);

// Categorias, na ordem em que foram criadas. O `id` e estavel (renomear nao o
// muda) e nunca e 0: 0 quer dizer "sem categoria".
int         sorg_n_categorias(void);
int         sorg_categoria_id(int i);
const char *sorg_categoria_nome_id(int id);   // NULL se nao existe
int         sorg_categoria_indice(int id);    // -1 se nao existe
// Cria e devolve o id; com o nome ja existente (sem diferenciar maiusculas)
// devolve o id da que existe. 0 = nome vazio ou lista cheia.
int  sorg_criar_categoria(const char *nome);
int  sorg_renomear_categoria(int id, const char *nome);
// Os titulos que estavam nela voltam a "sem categoria" — continuam salvos.
void sorg_excluir_categoria(int id);

// Categoria do titulo (id de TITULO; "tt123:1:2" vale como "tt123"), 0 = nenhuma.
int  sorg_categoria_de(const char *imdb);
// Poe o titulo na categoria `id` (0 tira de qualquer uma). Grava.
void sorg_mover(const char *imdb, int id);

// Normaliza o que veio do teclado: corta espacos nas pontas, junta espacos
// repetidos e poe a primeira letra em maiuscula ("  fim  de semana" -> "Fim de
// semana"). Devolve o tamanho final.
int  sorg_nome_limpo(const char *in, char *out, int tam);

// Apaga a organizacao de todos os perfis. Chamar de sync_esquecer_usuario,
// junto de salvos_esquecer: as categorias sao tao pessoais quanto a lista.
void sorg_esquecer(void);

#endif
