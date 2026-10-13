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
#include "colecoes.h"

// Solta a cache de corpos de manifesto entre ciclos e zera o estado da
// descoberta. Chamar no logout, junto de addons_esquecer/fil_esquecer: a cache
// e da conta que saiu e nao deve vazar para a proxima.
void desc_esquecer(void);
// Troca de perfil (#294): tira da tela e da memoria o catalogo do perfil que saiu.
void desc_esquecer_catalogo_perfil(void);

// CACHE UNICA DE MANIFESTO (corpo de manifest.json), por url + versao da lista
// de addons (addons_versao()). Antes havia DOIS caminhos baixando o mesmo
// manifest.json: a descoberta (aqui) e a sonda de capacidades de addons.c
// (addons_sondar_manifestos). Cada um tinha a sua copia e nenhum via o que o
// outro ja tinha baixado — dois GET para o mesmo addon, as vezes na mesma
// rodada de arranque. Estas duas funcoes sao a cache unica: quem baixa
// primeiro guarda, quem pede depois reaproveita, os dois lados sem saber nada
// um do outro.
//
// Chamaveis de QUALQUER FIO: a trava e a mesma que protege mani[] internamente
// (maniTrava em descoberta.c). Nenhuma delas faz rede.

// Devolve uma COPIA (malloc) do corpo cacheado para `url`+`versao`, ou NULL se
// nao houver entrada valida. Quem chama e dono do retorno e deve free() depois
// de usar.
char *desc_manifesto_cache_obter(const char *url, unsigned versao);

// Guarda uma COPIA de `corpo` na cache, associada a `url`+`versao`. NAO toma
// posse: quem chama continua dono de `corpo` e deve libera-lo como sempre
// (esta funcao nao muda nada na vida util do ponteiro recebido). Corpos
// maiores que o teto de memoria da cache (ver MANI_CACHE_BYTES em
// descoberta.c) sao silenciosamente ignorados — o chamador continua
// funcionando, so nao ganha cache para aquele manifesto.
void desc_manifesto_cache_guardar(const char *url, unsigned versao, const char *corpo);

// Dispara a montagem do catalogo num fio proprio. Volta na hora.
void desc_iniciar(void);

// Remonta o catalogo porque uma CREDENCIAL mudou. Diferente de desc_iniciar():
// se um ciclo ja estiver no ar, o pedido fica guardado e roda ao fim dele, em
// vez de ser descartado. Chamar do fio principal.
void desc_repetir(void);
// Igual a desc_repetir, mas a volta nao acende o alerta "Carregamento da Home"
// da ilha: para o que a pessoa nao pediu (sync, remontagem interna). Se ja ha
// uma volta visivel em curso ela continua visivel.
void desc_repetir_silencioso(void);
// Remonta porque a LISTA DE ADDONS mudou (sync). Mais barato que desc_repetir:
// se a montagem em curso ainda nao leu a lista, ela ja vai ler a nova, e o
// pedido e atendido por ela — sem jogar fora o Trakt que ela ja buscou. Se ja
// leu, e o mesmo que desc_repetir. Chamar do fio principal.
void desc_repetir_addons(void);
// #319: tira da tela, sem rede, as fileiras de addon removido ou desligado.
// Devolve quantas saíram.
int desc_tirar_fileiras_de_addons_ausentes(void);
// Refaz so a fileira "Continuar assistindo", fora do ciclo completo (issue
// #38). Fio proprio: remontar a fileira faz rede. Pedido repetido enquanto um
// fio ja roda vira UMA rodada a mais no fim, nao uma fila.
void desc_refazer_continuar(void);
// 1 enquanto a home ainda esta sendo montada: a volta de catalogos no ar (ou
// uma pedida para o fim dela) ou o "Continuar assistindo" sendo refeito. E o
// que a troca de perfil espera antes de mostrar a home (app.c).
int  desc_montando(void);
// `ativo` aqui e so a volta VISIVEL (arranque ou pedida pela pessoa): o que
// acende o alerta da ilha. O Continuar refeito e as voltas silenciosas nao.
typedef struct { int ativo, fase, fileiras, falhas, addonsProntos, addonsTotal; unsigned ms; } DescHomeCarga;
void desc_home_carga(DescHomeCarga *estado);
// A metade LOCAL de "Tirar de Continuar assistindo": progresso, carimbo de
// remocao e o card fora da fileira no mesmo quadro. Sem rede. Ver descoberta.c.
int desc_tirar_continuar(const char *imdb, int temporada, int episodio);
// O titulo que saiu do player no meio vai para a frente do "Continuar
// assistindo" no mesmo quadro, sem rede (cwfrente.h). 1 = a fileira mudou.
int desc_continuar_otimista(int indice);

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
// Keywords anexadas ao detalhe do TMDB: 179430/179431 sinalizam cena extra.
int desc_tmdb_pos_creditos(const char *json);
// "pt-BR" ou "en-US", conforme o idioma da interface: sinopse, biografia e nome
// de colecao vem do TMDB ja traduzidos, e em ingles vinham em portugues.
const char *desc_tmdb_idioma(void);

// A chave do TMDB ja carregada. Devolve "" quando art/tmdb.txt nao existe.
// O modulo `pessoa` precisa dela para a filmografia, e ler o arquivo duas vezes
// daria duas fontes de verdade para o mesmo segredo.
const char *desc_chave_tmdb(void);
const char *desc_chave_tmdb_reserva(void);   // para artereserva.c: ignora o ajuste "TMDB"

// "2026-07-29" -> "29 de julho de 2026". Vive aqui porque a descoberta ja
// precisava dela para a data de episodio; a tabela "Detalhes do Filme" e o
// segundo consumidor, e duplicar a lista de meses era pedir para as duas
// divergirem. Entrada fora do padrao ISO sai como veio.
void desc_data_extenso(const char *iso, char *dst, size_t tam);

// Nome de genero em portugues ("Action" -> "Ação"). O Cinemeta e o catalogo do
// pacote guardam os generos em INGLES, e eles apareciam crus numa interface em
// portugues. Genero fora da tabela sai como veio.
const char *desc_genero_pt(const char *g);
// Valores crus do TMDB/Trakt/Cinemeta que vao para a tela (ver descoberta.c).
// desc_status_chave devolve a CHAVE em portugues (passe por i18n) ou NULL.
const char *desc_status_chave(const char *raw, int serie);
void desc_pais_txt(const char *lista, char *dst, size_t tam);
void desc_duracao_min(int min, char *dst, size_t tam);
void desc_duracao_txt(const char *cru, char *dst, size_t tam);

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
int desc_busca_alvo_nuvio(int alvo);             // 1 = catalogo do Nuvio (#311)
int desc_busca_n_fontes(void);                   // fontes distintas consultadas (#311)
const char *desc_busca_alvo_addon(int alvo);    // "Cinemeta", "Xperience"
// O NOME que o manifesto da a um catalogo (base sem /manifest.json, tipo, id);
// "" enquanto o manifesto nao passou pela descoberta. Para a aba da colecao
// quando a conta manda a fonte sem titulo (#76).
const char *desc_nome_catalogo(const char *base, const char *tipo, const char *id);
int  desc_busca_alvo_n(int alvo, const char *termo);
int  desc_busca_alvo_item(int alvo, int i, CatItem *dst);
// Sobe a cada termo novo. Quem guarda posicao de foco entre quadros deve
// reajustar quando este numero mudar.
int  desc_busca_geracao(void);
int  desc_busca_chegou(const char *termo);   // 1 = ja houve resposta de algum alvo para `termo`

// Registro dos alvos, chamado pelo carregamento dos manifestos. `zerar` repoe
// so o Cinemeta.
void desc_alvos_busca_zerar(void);
// 1 quando "Buscar no Cinemeta" esta desligado e `base` e a de um addon
// Cinemeta: a busca nao pode mostrar nada dela (alvos de rede e fileiras locais).
int  desc_busca_base_oculta(const char *base);
// A primeira volta espera os addons da conta em vez de montar a Home com a
// lista vazia. `f` devolve 0 (nao espere: sem conta), 1 (espere: perfil ainda
// nao escolhido) ou 2 (espere ate 10 s). Sem gancho, nao ha espera.
void desc_espera_addons_definir(int (*f)(void));
void desc_alvo_busca(const char *base, const char *tipo, const char *id,
                     const char *titulo, const char *addon);

// 1 enquanto busca; a home pode usar isto para um indicador.
int  desc_buscando(void);

// Pede os episodios do titulo `indiceItem` na temporada `temporada`
// (0 = a temporada onde o dono parou, ou a primeira). Idempotente: pedir duas
// vezes a mesma coisa nao refaz a busca.
// --- VER TUDO: um catalogo inteiro, em paginas ------------------------------
//
// A home mostra DESC_ITENS_POR_FILEIRA itens por fileira. O catalogo tem mais, e
// o protocolo Stremio pagina por `skip`:
//   <base>/catalog/<tipo>/<id>/skip=<n>.json
// E o mesmo caminho da busca, com outro filtro no lugar do termo.
//
// Assincrono, como todo o resto: dispara e volta na hora. Quem desenha pergunta
// quantos ja chegaram.
#define VT_MAX 1000

// ITENS POR FILEIRA DA HOME (#163: "da para passar de 12?"). Eram 12. O web
// usa 15 no layout padrao e 24 no classico (HOME_MAX_ITEMS_PER_ROW_CLASSIC,
// homeConstants.js); aqui e 24. home.c reserva MAX_CARDS = 33 colunas (os
// cartazes mais o "Ver tudo"), entao cabe. Custo: ~15,6 KB por item em RAM,
// e as capas continuam carregando so quando entram na tela.
#define DESC_ITENS_POR_FILEIRA 24

// Comeca (ou continua) a leitura do catalogo. `pagina` 0 e o inicio; cada
// pagina seguinte pede skip = pagina * VT_PASSO. Repetir a mesma pagina nao
// refaz o pedido.
void desc_vertudo_abrir(const char *base, const char *tipo, const char *catId);
void desc_vertudo_filtro(const char *base, const char *tipo, const char *catId, const char *genre);
// Fonte nao-addon de pasta de colecao (issue #44): provider "tmdb" ou
// "trakt" vindo do site. Os itens sao pedidos direto a esses servicos —
// nao passa por catalogo de addon.
void desc_vertudo_fonte(const ColSource *s);
int desc_vertudo_erro(void);
// Pede a proxima pagina, se houver. Nada acontece se a ultima veio curta — sinal
// de fim de lista no protocolo.
void desc_vertudo_mais(void);
int  desc_vertudo_n(void);
int  desc_vertudo_item(int i, CatItem *dst);
// O QUE A LISTA DO TRAKT DIZ SOBRE O ITEM, alem do titulo: a posicao (rank), o
// instante em que entrou (listed_at) e a nota de quem montou a lista (notes).
// Mora numa tabela paralela a vtItens e NAO no CatItem: o CatItem vai inteiro
// para o cache em disco, e mudar o tamanho dele invalidaria o catalogo de quem
// nunca abriu uma lista. Vazio/0 = a fonte nao disse (addon, TMDB, Simkl).
typedef struct {
  int       rank;        // posicao na lista, 0 = nao informada
  long long entrouMs;    // listed_at em ms desde a epoca, 0 = nao informado
  char      nota[320];   // "" = sem nota do curador
  long      itemId;      // id da ENTRADA na lista (reordenar), 0 = nao informado
} DescItemLista;
int  desc_vertudo_extra(int i, DescItemLista *dst);
// Le rank/listed_at/notes de UMA entrada de /lists/<id>/items ({rank, listed_at,
// notes, type, movie:{...}}). Exposto para o teste de fixture.
void desc_trakt_extra_ler(const char *p, const char *f, DescItemLista *d);
int  desc_vertudo_carregando(void);
// 1 quando a ultima pagina veio curta: nao ha mais o que pedir.
int  desc_vertudo_fim(void);
void desc_vertudo_fechar(void);

void desc_episodios(int indiceItem, int temporada);
// Solta o pedido de episodios que chegou enquanto outro carregava. Chame por
// quadro; sem isto uma troca de temporada feita durante um carregamento fica
// pendurada e a lista nunca chega na temporada escolhida.
void desc_episodios_pendente(void);
void desc_episodios_garantir(int indiceItem);
int desc_episodios_precarregar(int indiceItem);   // -1 ocupado, 0 nada a pedir, 1 pediu
int desc_episodios_carregando(int indiceItem);
// Preenche `nota` (x10) e a `sinopse` no idioma pedido (#150) dos eps da
// temporada dada a partir do JSON de /tv/<id>/season/<n> do TMDB. Pura;
// devolve quantos episodios mudaram.
int  desc_tmdb_notas_temporada(const char *json, CatEp *eps, int n,
                               int temporada);
// O mesmo, escolhendo o que entra alem da nota (#176): DESC_EPT_SINOPSE,
// DESC_EPT_NOME (nome traduzido; o generico "Episodio 3" nunca entra) e
// DESC_EPT_SO_VAZIO (so preenche o que esta vazio, para nao pisar num texto que
// veio do addon de metadados quando a pessoa o prefere).
#define DESC_EPT_SINOPSE  1
#define DESC_EPT_NOME     2
#define DESC_EPT_SO_VAZIO 4
int  desc_tmdb_notas_temporada_ex(const char *json, CatEp *eps, int n,
                                  int temporada, int textos);
// 1 quando `nome` e o rotulo sem traducao do TMDB ("Episode 3"). Pura.
int  desc_nome_episodio_generico(const char *nome, int episodio);
// Junta duas listas de episodios ORDENADAS por (temporada, episodio): para cada
// episodio de `base` que `outro` tambem tem, `modo` DESC_MESCLA_TEXTO troca
// nome e sinopse pelos do outro quando ele os tem; DESC_MESCLA_VAZIOS so
// preenche o que a base nao tem (nome, sinopse, thumb, duracao). Pura.
#define DESC_MESCLA_TEXTO  1
#define DESC_MESCLA_VAZIOS 2
void desc_mesclar_episodios(CatEp *base, int nb, const CatEp *outro, int no,
                            int modo);
// Localiza (titulo e sinopse) os itens de catalogo de indices `idx`, em fio
// proprio e sem bloquear: fonte e o addon de metadados quando a pessoa o
// prefere, ou o TMDB no idioma configurado (#176). Chamada barata e repetivel:
// o que ja foi resolvido vem do cache.
void desc_localizar_indices(const int *idx, int n);
// Sinopse dos candidatos do destaque (indices do catalogo), pedida ao mesmo
// /meta do detalhe, em fio proprio e um pedido por vez. Quem falha segue sem
// sinopse, com a falha no log. Ver o bloco em descoberta.c.
void desc_sinopse_hero(const int *idx, int n);
// Texto e arte localizados ja conhecidos (memoria + loc-texto.txt) aplicados ao
// catalogo que veio do cache, sem rede (#213). Devolve quantos mudaram.
int desc_localizar_catalogo_cache(void);
// Esquece o texto localizado (memoria e arquivo). Logout.
void desc_loc_apagar(void);
// Casa o `cast` de /credits do TMDB com o elenco do item POR NOME (sem acento,
// caixa nem pontuacao) e completa a lista com o resto do TMDB (#153). Pura;
// devolve quantos nomes casaram.
int  desc_tmdb_elenco(const char *json, CatItem *d);
// Tipo(s) em que perguntar o /meta do Cinemeta, na ordem (1 ou 2). Tipo
// incerto ("anime" etc.) tenta serie e depois filme. Puras.
int  desc_meta_tipos(const char *tipo, const char *saida[2]);
void desc_meta_chave(char *dst, size_t n, const char *tipo, const char *id);
int  desc_meta_tem_temporadas(const char *corpo);
int  desc_meta_n_episodios(const char *corpo);

// Busca o meta de um titulo que o catalogo NAO tem e o acrescenta ao fim.
// Nao bloqueia. Serve ao credito de um ator e ao item de "Mais como este":
// sem isto, tudo que estivesse fora do catalogo do dono nao abria.
void desc_pedir_titulo(const char *imdb);
// Mesma coisa a partir do id do TMDB, que e o que o credito de um ator traz.
// `tipo` e "movie" ou "tv". Resolve o IMDb por external_ids antes de pedir o
// meta — uma chamada a mais, so quando o dono abre o credito.
void desc_pedir_titulo_tmdb(long tmdbId, const char *tipo);
// Abre JA, com o que o clique sabia (nome, ano, cartaz); a ficha chega depois.
void desc_pedir_titulo_semente(const char *imdb, long tmdb, const char *tipo,
                               const char *titulo, const char *ano, const char *poster);
// Indice do titulo que acabou de entrar, ou -1. CONSOME o resultado.
int  desc_titulo_pronto(void);
int  desc_titulo_buscando(void);

// O PRIMEIRO EPISODIO DESMARCADO NA TV (vistonao_primeira), ligado por app.c.
// Sem ele o "a seguir" do Continuar assistindo fica como o remoto o deu.
void desc_lapides_primeira(int (*primeira)(const char *imdb, int temporada, int episodio,
                                           long long remotoMs, int *pt, int *pe));

// RITMO DOS PEDIDOS DE CATALOGO (fileiras ilimitadas no Android, 2.0.4).
// Quantas fileiras uma rodada de montar() pede quando faltam `alvo`, e quantos
// GET de catalogo ficam em voo ao mesmo tempo (os fios da rodada). Expostos
// para tests/fileiras_android.sh provar que 100 fileiras nao viram 100 pedidos.
int desc_catalogos_por_rodada(int alvo);
int desc_catalogos_em_voo(void);

#endif
