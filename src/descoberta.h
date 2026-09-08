// Monta o catalogo EM EXECUCAO, a partir da rede.
//
// Antes tudo vinha de arquivos gerados a mao (catalogo.txt, ids.txt,
// episodios.txt) com a arte baixada junto do pacote. Funcionava, mas
// congelava: recomendacao de ontem, episodio da temporada de ontem, e cada
// mudanca exigia regerar e reinstalar. Agora as fileiras vem dos catalogos dos
// addons do dono e os episodios vem do Cinemeta na hora que o titulo abre.
//
// Os arquivos continuam servindo de RESERVA: sem rede, o app abre com o que
// veio no pacote em vez de abrir vazio.
#ifndef NV_DESCOBERTA_H
#define NV_DESCOBERTA_H
#include <stddef.h>
#include "catalogo.h"

// Dispara a montagem do catalogo num fio proprio. Volta na hora.
void desc_iniciar(void);

// Remonta o catalogo porque uma CREDENCIAL mudou. Diferente de desc_iniciar():
// se um ciclo ja estiver no ar, o pedido fica guardado e roda ao fim dele, em
// vez de ser descartado. Chamar do fio principal.
void desc_repetir(void);

// Quantas fileiras A MAIS a home mostraria se o limite fosse ao maximo. 0
// quando o limite nao esta cortando nada.
//
// Nao e a contagem crua do que sobrou: os addons declaram centenas de catalogos
// e "mais 240 disponiveis" seria verdadeiro e inutil, porque o teto do vetor de
// fileiras (CAT_FIL_MAX) limita o que aumentar o ajuste pode entregar. Tambem
// nao conta o que a pessoa desligou de proposito — aquilo nao e surpresa.
//
// Existe para a home poder DIZER que ha mais catalogo do que ela esta mostrando.
// Sem isso o limite e invisivel: quem tinha fileira de recomendacao abaixo da
// setima simplesmente parou de ve-la, sem nada na tela ligando a ausencia ao
// ajuste — foi exatamente o relato do issue #11 ("recommended no longer showing
// up like before").
int desc_catalogos_fora(void);

// Reordena e filtra as fileiras JA MONTADAS, sem tocar na rede. Para mudanca de
// ordem, de colecao ou de limite — ver a nota longa em descoberta.c. Chamar do
// fio principal.
void desc_remontar_fileiras(void);

// Le a chave do TMDB (art/tmdb.txt). Sem ela o elenco fica so com nomes, sem
// foto nem personagem.
void desc_tmdb(const char *dirArte);

// Chave do TMDB vinda da CONTA, no lugar de art/tmdb.txt. MEDIDO na conta do
// dono: `sync_pull_provider_credentials` devolve o provedor "tmdb" com um
// campo `api_key`. Enquanto a chave sair do arquivo, ela vai dentro do .ipk e
// e a chave de quem montou o pacote — cota dele, para todo mundo que instalar.
void desc_tmdb_definir(const char *chave);
// "pt-BR" ou "en-US", conforme o idioma da interface: sinopse, biografia e nome
// de colecao vem do TMDB ja traduzidos, e em ingles vinham em portugues.
const char *desc_tmdb_idioma(void);

// A chave do TMDB ja carregada. Devolve "" quando art/tmdb.txt nao existe.
// O modulo `pessoa` precisa dela para a filmografia, e ler o arquivo duas vezes
// daria duas fontes de verdade para o mesmo segredo.
const char *desc_chave_tmdb(void);

// "2026-07-29" -> "29 de julho de 2026". Vive aqui porque a descoberta ja
// precisava dela para a data de episodio; a tabela "Detalhes do Filme" e o
// segundo consumidor, e duplicar a lista de meses era pedir para as duas
// divergirem. Entrada fora do padrao ISO sai como veio.
void desc_data_extenso(const char *iso, char *dst, size_t tam);

// Nome de genero em portugues ("Action" -> "Ação"). O Cinemeta e o catalogo do
// pacote guardam os generos em INGLES, e eles apareciam crus numa interface em
// portugues. Genero fora da tabela sai como veio.
const char *desc_genero_pt(const char *g);

// --- busca por titulo --------------------------------------------------------
// Consulta o Cinemeta em filme e serie. NAO BLOQUEIA: dispara um fio e volta na
// hora; chamar de novo com o mesmo termo nao refaz o pedido, e com termo
// diferente faz o fio em voo descartar o resultado velho e ir atras do novo (o
// dono continua digitando enquanto a rede responde).
//
// Existe porque a tela de busca so filtrava o que ja estava em memoria, e
// procurar qualquer coisa fora das primeiras linhas de cada catalogo nao
// achava nada.
void desc_buscar(const char *termo);

// Quantos resultados ha PARA ESTE TERMO. Devolve 0 quando o que chegou e de uma
// consulta anterior — assim a tela nunca mostra o resultado de outra palavra.
int  desc_busca_n(const char *termo);

// Copia o resultado `i`. 1 se copiou.
int  desc_busca_item(int i, CatItem *dst);

// --- busca em VARIOS catalogos ----------------------------------------------
//
// Um "alvo" e um catalogo que declara busca no manifesto. Sao os 2 do Cinemeta
// mais os que os addons do dono declararem (hoje 8: Xperience, AIOStreams por
// TMDB e por TVDB, e Akashi TV — filme e serie em cada um).
//
// A tela desenha UMA FILEIRA POR ALVO, na ordem em que os alvos foram
// registrados, pulando os que ainda nao responderam ou vieram vazios. Assim o
// primeiro catalogo a responder ja aparece, em vez de a tela esperar o mais
// lento de dez.
int  desc_busca_n_alvos(void);
const char *desc_busca_alvo_titulo(int alvo);   // "Filmes", "Séries"
const char *desc_busca_alvo_addon(int alvo);    // "Cinemeta", "Xperience"
int  desc_busca_alvo_n(int alvo, const char *termo);
int  desc_busca_alvo_item(int alvo, int i, CatItem *dst);
// Sobe a cada termo novo. Quem guarda posicao de foco entre quadros deve
// reajustar quando este numero mudar.
int  desc_busca_geracao(void);

// Registro dos alvos, chamado pelo carregamento dos manifestos. `zerar` repoe
// so o Cinemeta.
void desc_alvos_busca_zerar(void);
void desc_alvo_busca(const char *base, const char *tipo, const char *id,
                     const char *titulo, const char *addon);

// 1 enquanto busca; a home pode usar isto para um indicador.
int  desc_buscando(void);

// Pede os episodios do titulo `indiceItem` na temporada `temporada`
// (0 = a temporada onde o dono parou, ou a primeira). Idempotente: pedir duas
// vezes a mesma coisa nao refaz a busca.
// --- VER TUDO: um catalogo inteiro, em paginas ------------------------------
//
// A home mostra 12 itens por fileira (MAX_POR_FILEIRA). O catalogo tem mais, e
// o protocolo Stremio pagina por `skip`:
//   <base>/catalog/<tipo>/<id>/skip=<n>.json
// E o mesmo caminho da busca, com outro filtro no lugar do termo.
//
// Assincrono, como todo o resto: dispara e volta na hora. Quem desenha pergunta
// quantos ja chegaram.
#define VT_MAX 1000

// Comeca (ou continua) a leitura do catalogo. `pagina` 0 e o inicio; cada
// pagina seguinte pede skip = pagina * VT_PASSO. Repetir a mesma pagina nao
// refaz o pedido.
void desc_vertudo_abrir(const char *base, const char *tipo, const char *catId);
void desc_vertudo_filtro(const char *base, const char *tipo, const char *catId, const char *genre);
int desc_vertudo_erro(void);
// Pede a proxima pagina, se houver. Nada acontece se a ultima veio curta — sinal
// de fim de lista no protocolo.
void desc_vertudo_mais(void);
int  desc_vertudo_n(void);
int  desc_vertudo_item(int i, CatItem *dst);
int  desc_vertudo_carregando(void);
// 1 quando a ultima pagina veio curta: nao ha mais o que pedir.
int  desc_vertudo_fim(void);
void desc_vertudo_fechar(void);

void desc_episodios(int indiceItem, int temporada);
// Solta o pedido de episodios que chegou enquanto outro carregava. Chame por
// quadro; sem isto uma troca de temporada feita durante um carregamento fica
// pendurada e a lista nunca chega na temporada escolhida.
void desc_episodios_pendente(void);
int desc_episodios_carregando(int indiceItem);

// Busca o meta de um titulo que o catalogo NAO tem e o acrescenta ao fim.
// Nao bloqueia. Serve ao credito de um ator e ao item de "Mais como este":
// sem isto, tudo que estivesse fora do catalogo do dono nao abria.
void desc_pedir_titulo(const char *imdb);
// Mesma coisa a partir do id do TMDB, que e o que o credito de um ator traz.
// `tipo` e "movie" ou "tv". Resolve o IMDb por external_ids antes de pedir o
// meta — uma chamada a mais, so quando o dono abre o credito.
void desc_pedir_titulo_tmdb(long tmdbId, const char *tipo);
// Indice do titulo que acabou de entrar, ou -1. CONSOME o resultado.
int  desc_titulo_pronto(void);
int  desc_titulo_buscando(void);

#endif
