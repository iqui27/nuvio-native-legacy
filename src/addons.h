// Ponte com os addons (protocolo Stremio) — as fontes de verdade.
//
// Um addon e uma URL base; as fontes de um titulo saem de
//   <base>/stream/<movie|series>/<id>.json
// e vem como {"streams":[{name,title|description,url,behaviorHints},...]}.
// Nao ha autenticacao propria: a chave, quando existe, ja vem embutida no
// caminho da URL do addon (por isso addons.txt e conteudo sensivel do dono e
// nao deve ir para repositorio nenhum).
//
// A busca BLOQUEIA e roda num fio proprio. O resultado entra por
// stream_definir_lista, e a tela so precisa olhar addons_estado().
#ifndef NV_ADDONS_H
#define NV_ADDONS_H
#include "watchstate.h"
#include "addonurl.h"
#include "fontefalha.h"
#include <stdint.h>

// Um teto so para a lista e para os consumidores que precisam de TODOS os donos.
#define ADDONS_MAX 128
typedef struct { char id[96], base[NV_ADDON_URL_MAX]; int ativo; } AddonIdentidade;
// Copia sob a trava; -1 se a lista mudou, e de outro perfil ou nao cabe inteira.
int addons_copiar_identidades(AddonIdentidade *saida, int max, unsigned versao, int perfil);
// Publicar a limpeza so enquanto a MESMA lista da conta continua protegida.
// Ordem das travas: sessao de perfil, lista de addons, fileiras/colecoes.
int addons_travar_lista(unsigned versao, int perfil);
void addons_destravar_lista(void);

typedef enum { ADD_PARADO = 0, ADD_BUSCANDO, ADD_PRONTO, ADD_VAZIO } AddEstado;

// Le art/addons.txt (nome<TAB>url por linha). Sem ele a lista fica vazia.
int  addons_carregar(const char *dirArte);

// Lista vinda da CONTA, substituindo o arquivo. E isto que torna o pacote
// distribuivel: enquanto a lista sair de art/addons.txt, o .ipk carrega as
// chaves de debrid de quem o montou embutidas nas URLs.
//
// Uma lista VAZIA e ignorada de proposito. O servidor pode responder vazio por
// perfil errado, 401 mal tratado ou queda — e nenhum desses e "o usuario
// removeu todos os addons". Trocar por vazio deixaria a pessoa sem fonte
// nenhuma e sem entender por que.
// `url` com NV_ADDON_URL_MAX (addonurl.h), e nao 600: a URL do Comet tem 870
// e era cortada aqui, antes de qualquer outro modulo ve-la (#201).
typedef struct { char nome[64]; char url[NV_ADDON_URL_MAX]; int ativo; } AddonRemoto;
// Devolve 1 quando a lista MUDOU e foi aplicada; 0 quando nada mudou, quando
// veio vazia, ou quando nada nela era utilizavel. Quem chama usa isso para
// decidir se vale remontar o catalogo — e nao para saber quantos addons ha.
int  addons_definir_lista(const AddonRemoto *lista, int n);

// DE QUEM E A LISTA DE AGORA. O sync chama addons_marcar_da_conta(perfil) toda
// vez que a resposta da conta para o perfil ATIVO foi aplicada — mudando a
// lista ou nao —; addons_carregar (o art/addons.txt do pacote) e
// addons_esquecer voltam a 0. addons_perfil_da_lista devolve esse perfil, 0
// quando a lista nao veio da conta.
//
// Existe para a poda de fileiras (fil_podar_catalogos): no arranque a primeira
// volta da descoberta roda com a lista do PACOTE, e podar com ela apagava as
// escolhas do perfil sobre os addons que so a conta dele tem.
void addons_marcar_da_conta(int perfil);
int  addons_perfil_da_lista(void);

// Lista atual, para o sync poder empurrar de volta o que este aparelho tem.
int  addons_exportar(AddonRemoto *saida, int max);

// Quantos addons ENTRARAM e quantos SAIRAM entre duas listas, comparados pela
// URL (e nao pelo nome: dois addons podem se chamar igual), e quantos dos que
// ficaram MUDARAM (ligado/desligado ou ordem). E o que o botao "Sincronizar
// addons" (#360) diz depois da volta. Qualquer ponteiro de saida pode ser NULL.
void addons_diferenca(const AddonRemoto *antes, int na, const AddonRemoto *depois, int nd,
                      int *entraram, int *sairam, int *mudaram);

// Esquece a lista da conta. Chamado ao SAIR: sem isto, a proxima pessoa a usar
// esta TV navega com os addons da anterior — e como as chaves de debrid vao
// embutidas nas URLs, ela tambem consome a assinatura da anterior — ate o
// primeiro sync terminar. Ficar sem fonte por alguns segundos e o
// comportamento correto de "ninguem logado".
void addons_esquecer(void);
// Abre uma sessao de perfil limpa; nao espera nem refaz pedidos anteriores.
void addons_abrir_sessao(void);
int  addons_n(void);
// Numero que sobe a cada mudanca na lista de addons (conta, liga/desliga,
// adicao). app.c refaz a busca de fontes do titulo aberto quando ele muda —
// era o titulo aberto antes de a lista da conta chegar ficando sem fonte.
unsigned addons_versao(void);
// Capturar na UI para a consulta independente do proximo episodio. Bases
// privadas e proveniencia pertencem ao snapshot; nenhum worker le a lista
// de addons do perfil que entrou depois. NULL = captura indisponivel/incompleta.
typedef struct AddonsConsulta AddonsConsulta;
AddonsConsulta *addons_consulta_capturar(void);
int addons_consulta_atual(const AddonsConsulta *consulta);
// BLOQUEIA no worker; devolve lista propria completa, sem publicar/cachear.
// Uma consulta cancelada/incompleta devolve -1 e deixa *saida NULL.
int addons_consulta_consultar(const AddonsConsulta *consulta, const char *id, const char *tipo,
                              int fios, int (*cancelado)(void *), void *ctx, void *saida /* Stream ** */);
// Serial installed add-on availability probe; no plugins, retries, cache, or list publication.
int addons_consulta_filme(const AddonsConsulta *consulta, const char *id,
                          int (*cancelado)(void *), void *ctx);
void addons_consulta_liberar(AddonsConsulta *consulta);
const char *addons_base(int i);   // URL base, sem /manifest.json
void addons_catalogo_chave(const char *id, const char *base, const char *tipo,
                           const char *catalogo, char *dst, unsigned tam);
const char *addons_id_manifesto(int i);   // "id" do manifesto; "" ate a sonda ler
const char *addons_nome_por_id(const char *id);   // nome de exibicao; "" se nao ha
const char *addons_base_por_id(const char *idManifesto);   // "" ate a sonda conhecer o id
int addons_base_por_id_copiar(const char *idManifesto, char *base, unsigned tam);
typedef enum {
  ADD_IDENTIDADE_AUSENTE = 0, ADD_IDENTIDADE_PENDENTE,
  ADD_IDENTIDADE_FALHOU, ADD_IDENTIDADE_PRONTA
} AddonIdentidadeEstado;
// Ausencia so e confirmada depois de identificar toda a lista instalada.
// Uma tentativa sem resposta/id e falha de identificacao, nao addon removido.
AddonIdentidadeEstado addons_identidade_estado(const char *idManifesto);
// Publica SOMENTE o id, sem esperar a montagem da Home/CW. O pedido pertence
// a esta base, versao de lista e sessao; resposta velha nao altera a lista.
// NULL/manifesto sem id encerra a tentativa com falha, preservando id conhecido.
int addons_identidade_recebida(const char *base, unsigned versao, uint64_t sessao,
                               const char *corpo);
// Final capability publication keeps the same request ownership as identity.
int addons_manifesto_recebido(const char *base, unsigned versao, uint64_t sessao,
                              const char *corpo);
int  addons_tem_catalogo(int i);  // 1 quando o addon fornece catalogo

// Dispara a busca das fontes de `imdb` ("tt1234567", ou "tt1234567:1:2" para
// episodio). Volta na hora; o resultado chega por stream_definir_lista.
//
// CONSULTA O CACHE ANTES DA REDE (fontecache.h): canal que o guia engatilhou
// responde sem fio nenhum, e canal cujo prefetch esta na rede agora e esperado
// em vez de repetido. O caminho de rede e o de sempre.
// Origem do proximo alvo (base do addon que publicou o canal). Com ela a busca
// pergunta SO a esse addon; vazia, pergunta a todos. Ver alvoBase em addons.c.
void addons_definir_origem(const char *base);

// Origem extra de fontes (os plugins Nuvio, plugins.h — F09), consultada em
// paralelo com os addons e somada depois deles. `ativa` diz se vale perguntar.
// CADA PARTE DA ORIGEM EXTRA E MAIS UMA ORIGEM NA FOLHA (#221): `aviso` (pode
// ser NULL) e chamado do fio da origem para a parte `k` (o scraper, na ordem do
// manifesto) com estado 1 = vai rodar, 2 = terminou (com `n` fontes em
// `fontes`, um Stream *; copiadas na hora), 3 = desistiu. `id` e o id estavel
// da parte (o do scraper no manifesto; pode ser NULL quando nao existe).
typedef void (*OrigemAviso)(void *u, int k, const char *nome, const char *id,
                            int estado,
                            const void *fontes /* const Stream * */, int n);
typedef int (*OrigemExtra)(const char *id, const char *tipo, int (*cancelado)(void *),
                           void *ctx, OrigemAviso aviso, void *avisoU,
                           void *saida /* Stream ** */);
void addons_definir_origem_extra(OrigemExtra f, int (*ativa)(void));
// Capturar na UI; consultar somente o estado proprio no worker. 0 e uma
// resposta vazia valida; -1 e falha, cancelamento ou resposta incompleta.
typedef struct Stream Stream;
typedef struct {
  void *(*capturar)(void);
  int (*atual)(const void *);
  int (*consultar)(const void *, const char *id, const char *tipo,
                   int (*cancelado)(void *), void *ctx,
                   OrigemAviso aviso, void *avisoU, Stream **saida);
  void (*liberar)(void *);
} OrigemPreparoOps;
void addons_definir_origem_extra_preparo(OrigemExtra f, const OrigemPreparoOps *ops);
int  addons_origem_extra_ativa(void);
void addons_buscar(const char *imdb, const char *tipo);
// Recarregar explicito: descarta a resposta anterior e vai a rede, inclusive
// quando a lista atual esta vazia ou foi filtrada por falta de debrid.
void addons_buscar_renovar(const char *imdb, const char *tipo);

// --- busca em andamento, addon a addon (#221) --------------------------------
// A busca real de filme/serie publica cada addon que responde, sem esperar os
// outros: a folha enche aos poucos e a escolha automatica pode sair antes do
// fim (app.c). addons_estado ja drena; addons_drenar e so a drenagem, para o
// app.c chamar enquanto a verificacao roda (sem tocar no resto do estado).
void addons_drenar(void);
// 1 enquanto a busca real de VOD esta no ar publicando por addon.
int  addons_busca_parcial(void);
// Sobe a cada mudanca de estado de addon/plugin (anuncio, fim, desistencia):
// a folha inclui na chave do cache para a aba sair de "buscando" mesmo sem
// fontes novas.
unsigned addons_parcial_rev(void);
int  addons_busca_incompleta(void); // Finished search still missing responses.
// Milissegundos desde o disparo dessa busca; 0 fora dela.
unsigned addons_busca_ms(void);
// Quantos addons ainda nao responderam (contando quem espera a segunda chance)
// e, em `nomes`, os nomes deles separados por virgula. 0 fora da busca.
int  addons_faltam(char *nomes, unsigned tam);
// O mesmo contando tambem os scrapers de plugin; *plugins = quantos deles.
int  addons_faltam_tipo(char *nomes, unsigned tam, int *plugins);
// Quantos ainda seguram a decisao AUTOMATICA: addons_faltam sem quem e mudo
// seguido ou mais lento que a espera de Ajustes nesta TV (addonstats.h, #202).
// Quem sai daqui continua sendo consultado e entra na lista quando responde.
int  addons_faltam_decisivos(void);
// A espera de Ajustes em ms, que e o limite do "lento"; <= 0 = ninguem sai.
void addons_definir_espera_decisao(int ms);
// Para a ilha explicar a espera (inicio.h).
typedef struct {
  int pendentes, pendenteMudo, semResposta;
  char pendente[64], semRespostaNome[64];
} AddonsInicioInfo;
void addons_inicio_info(AddonsInicioInfo *o);
// Algum addon de indice menor que `idx` (ordem de instalacao) ainda falta?
int  addons_pendente_antes(int idx);
// O addon com este nome (Stream.provedor) ainda falta?
int  addons_pendente_nome(const char *nome);
// O menor grupo de auto-play (fonteregra.h, #202) que um pendente pode trazer;
// `f` recebe o nome e se e plugin e devolve o grupo (-1 = nunca). 99 = nenhum.
int  addons_pendente_grupo_min(int (*f)(const char *nome, int plugin, void *u), void *u);

// Estado de um addon/plugin na busca parcial, para a folha de Fontes.
enum { ADDON_ABA_PRONTO = 0, ADDON_ABA_BUSCANDO = 1, ADDON_ABA_FALHOU = 2 };
typedef struct {
  char nome[96];
  char chave[FONTEFALHA_CHAVE];
  int ordem;   // ordem de instalacao (addons) ou ADD_MAX + k (plugins)
  int estado;  // ADDON_ABA_*
} AddonAba;
// Lista os addons/plugins desta busca, na ordem de exibicao, com quem ainda
// nao respondeu ou falhou. Devolve quantos cabem em `out`.
int addons_parcial_abas(AddonAba *out, int max);

// A mesma consulta, SINCRONA E REENTRANTE, e addons_consultar — declarada em
// fontecache.h, e nao aqui, porque a assinatura precisa de Stream (streams.h,
// que puxa SDL) e este cabecalho e incluido por modulos que os testes compilam
// sem SDL (tests/colecoes.sh). Definida em addons.c.

// --- legendas externas dos addons -------------------------------------------
// Addon de legenda responde em /subtitles/<tipo>/<id>.json com
// {"subtitles":[{lang,url,subtitleFileName,...}]}. Sao dezenas por titulo, a
// maioria em idiomas que nao interessam — por isso a lista e FILTRADA por
// idioma antes de chegar na tela: 70 linhas para rolar seria pior que nenhuma.
// 36 = 12 por idioma com tres grupos (principal, secundario, ingles); com um
// grupo so ("Todas"/sem preferencia) sao 36 de uma vez (#268). Cada Legenda tem
// ~1,3 KB: copias de LEG_MAX NAO vao para a pilha (fio de 128 KB na LG/Tizen),
// usam malloc (faixas.c, legendasui.c, addons.c).
#define LEG_MAX 36

typedef struct {
  char rotulo[64];   // "Portugues (BR)  ·  Silo.S01E05.WEB"
  char idioma[8];
  // 1024 e nao 600 (#201): addon que embute a configuracao no link de
  // download (o Community Subtitles poe um base64 do contexto, que cresce com
  // o nome do arquivo) passava de 600 e o link saia CORTADO, sem erro nenhum,
  // ate a hora de baixar. Link que nem assim cabe e descartado (addons.c).
  char url[1024];
  char provedor[64]; // nome do addon que devolveu esta legenda
  char arquivo[96];  // subtitleFileName / movieReleaseName as sent; "" = not sent
  char id[256];      // identidade enviada pelo addon; "" = nao enviada
  int addonIndice;   // origem para dedup por id+addon
} Legenda;

void addons_buscar_legendas(const char *imdb, const char *tipo);

// O ARQUIVO QUE TOCA, para os extras do Stremio (legextras.h, #201): com a
// fonte confirmada, a busca do titulo e refeita mandando `filename`,
// `videoSize` e `videoHash`. `hash` e o behaviorHints.videoHash quando o addon
// de fonte manda; vazio, ele e medido aqui por dois Range de 64 KiB em
// `urlVideo` (so link remoto, sem cabecalho exigido e com `tamanho` exato).
// Soma a lista nova a atual sem mover indices, ate LEG_MAX; erro/vazio mantem
// o que ja existe. Duplicatas por URL ou id+addon ficam fora. Mesmo id e mesmos
// extras de antes: nao faz nada.
void addons_legendas_fonte(const char *imdb, const char *tipo, const char *arquivo,
                           unsigned long long tamanho, const char *hash,
                           const char *urlVideo, int exigeCabecalhos);

// REFAZ a busca do titulo que esta carregado agora, descartando a lista atual.
//
// addons_buscar_legendas DECLINA quando o id pedido e o que ja esta em memoria
// — e a decisao certa, senao cada quadro do detalhe refaria dezenas de
// requisicoes. Mas a lista depende TAMBEM do idioma preferido (gruposIdioma), e
// trocar o idioma em Ajustes nao mudava id nenhum: a folha de legendas
// continuava mostrando exatamente o que o idioma anterior deixou, o que do sofa
// se le como "mudei no ajuste e nada muda" (issue #9). Chamar isto e o que
// torna o ajuste observavel sem enfraquecer a guarda.
//
// Sem titulo carregado nao faz nada.
void addons_legendas_reiniciar(void);

int  addons_n_legendas(void);
// 1 quando a busca do titulo pedido TERMINOU (a lista nao cresce mais). 0
// enquanto o fio corre e tambem quando nada foi pedido: sem pedido, a lista em
// memoria pode ser de outro titulo. Quem liga legenda sozinho (faixas.c, #129)
// precisa dessa distincao; a folha nao, ela so mostra o que ha.
int  addons_legendas_prontas(void);
// Thread-local copy, valid until this thread calls addons_legenda() again.
const Legenda *addons_legenda(int i);
// ATOMIC SNAPSHOT of the subtitle list: copies up to `max` entries while the
// list mutex is held, so a reader never sees a list being replaced.
// `geracao` (optional) receives the list generation; `prontas`
// (optional) the same answer as addons_legendas_prontas(), from the same lock.
// Returns how many entries were copied.
int  addons_legendas_copiar(Legenda *dst, int max, unsigned *geracao, int *prontas);
#ifdef NV_SHOT_HOOKS
void addons_shot_legendas(const Legenda *v, int n);
#endif


// --- lista para a tela de addons --------------------------------------------
//
// A conta pode ter addon DESLIGADO, e ele continua na lista: some das consultas
// mas aparece na tela, para poder ser religado sem pegar o celular.
enum { ADD_CATALOGO = 0, ADD_STREAM, ADD_LEGENDA, ADD_META };

const char *addons_nome(int i);
int  addons_ativo(int i);
int  addons_base_desligada(const char *base);   // 1 = addon da lista desligado na conta
int  addons_alternar(int i);          // devolve o estado NOVO
// Acrescenta um addon sem refazer a lista (ver a nota em addons.c). 1 = entrou,
// 0 = lista cheia, ja instalado ou URL que nao cabe (addonurl.h). #360 mao
// unica: o acrescimo vale so nesta TV; o proximo pull traz a conta por cima.
int  addons_adicionar(const char *nome, const char *urlManifest);
// O addon fornece este recurso? Ate a sonda responder e uma suposicao
// otimista; addons_sondado() diz qual dos dois casos e.
int  addons_fornece(int i, int oque);
int  addons_sondado(int i);
// O addon `i` serve /meta/<tipo>/<id>.json? Le o que o manifesto DECLARA no
// resource "meta" (types e idPrefixes, do resource ou da raiz):
//   1 = declara meta para esse tipo e esse prefixo de id;
//   0 = nao serve (sem "meta", ou declarou tipos/prefixos e este nao esta);
//  -1 = nao da para saber (manifesto nao lido, ou sem idPrefixes).
// `tipo` vazio nao filtra por tipo.
int  addons_aceita_id(int i, const char *tipo, const char *id);
// O addon `i` (ligado, manifesto lido) declara push watch_state v2? Copia a
// declaracao para `out` e devolve 1; 0 = nao participa. Quem empurra e
// watchstate.c, que cruza isto com tipo/ids/evento (ws_decl_aceita).
int  addons_watchstate(int i, WsDecl *out);
// Protege snapshots de base/identidade/declaracao com a mesma trava dos
// escritores da lista e do manifesto. Soltar antes de rede ou wsenvio_podar.
void addons_lista_travar(void);
void addons_lista_destravar(void);
// Le o manifesto de cada addon num fio proprio, uma vez por lista.
//
// SO SERVE COMO RESERVA hoje, e a distincao importa: ela era chamada de um
// lugar so — a tela de addons dos Ajustes (addonsui.c) — entao quem nunca abria
// aquela tela passava a sessao INTEIRA com `addon[].id` vazio. Como o id do
// manifesto e a chave que addons_base_por_id usa, e como as fontes das colecoes
// da conta guardam esse id e nenhuma URL, TODA colecao da conta abria vazia. E
// a causa raiz do issue #10, e ela nao estava em nenhuma das funcoes que
// pareciam culpadas. O caminho normal agora e addons_manifesto_lido, abaixo.
void addons_sondar_manifestos(void);

// O MANIFESTO DO ADDON `i`, JA BAIXADO POR OUTRO. Preenche id, nome e
// capacidades a partir de `corpo`.
//
// Existe para nao baixar o mesmo arquivo duas vezes: descoberta.c ja le o
// manifesto de todo addon no arranque, para enumerar os catalogos. Antes desta
// funcao havia dois leitores do mesmo arquivo com propositos diferentes, e o
// que rodava sempre (o da descoberta) jogava fora exatamente o dado que faltava
// ao outro.
//
// CONCORRENCIA, dita e nao escondida: escreve em `addon[i]` do fio de quem
// chama, e outros fios leem esse vetor sem trava — o que ja era verdade da
// sonda. O pior caso e uma leitura de `id` pela metade num quadro; quem depende
// dele (vertudo.c) reconfere por quadro. `base`, que e o campo que os fios de
// busca usam, nao e tocado aqui.
void addons_manifesto_lido(int i, const char *corpo);

// O MESMO, com recheck da base/configuracao, destino (conta/perfil) e versao
// copiados sob a trava ANTES do download: so grava se o slot ainda e o mesmo.
// E o caminho da descoberta, que baixa com a lista de um instante e pode receber a resposta
// depois de a conta ter trocado/reordenado a lista — o manifesto atrasado de
// um addon nao pode instalar capacidades nem declaracao watch_state no slot
// de outro (ver addons_manifesto_lido_base em addons.c).
void addons_manifesto_lido_base(int i, const char *base, const char *destino,
                                unsigned versao, const char *corpo);

// OS CATALOGOS DE CANAL QUE O MANIFESTO DECLARA (type tv/channel/channels/
// live/iptv), lidos pela mesma passagem de addons_manifesto_lido. Existe para
// o Guia de TV NAO baixar o manifesto de novo: a descoberta ja leu todos no
// arranque, em paralelo, e o guia refazia os mesmos GETs em serie — MEDIDO
// como a maior parte da espera para o guia abrir. Devolve quantos copiou para
// `saida`; -1 quando o manifesto deste addon ainda nao foi lido (ai o guia
// baixa ele mesmo, como antes).
typedef struct { char tipo[16], id[96], nome[96]; } AddCatCanal;
#define ADD_CANAL_MAX 4
int addons_catalogos_canal(int i, AddCatCanal *saida, int max);

// POR QUE A ULTIMA BUSCA NAO TROUXE FONTE, em frase curta ja traduzida
// ("4 add-ons responderam: nenhum tem este título", "Comet não respondeu",
// "Nenhum add-on de fontes instalado"). Devolve 1 e escreve em `dst`; 0
// quando a causa nao e conhecida (lista do cache, busca em curso, ou algum
// addon trouxe fonte). Chamar do fio da UI, depois de addons_estado() sair de
// ADD_BUSCANDO.
int addons_motivo_vazio(char *dst, unsigned n);
// ADDON FORA DO AR (ilha do relogio, 02/10): sobe 1 cada vez que um addon NAO
// RESPONDEU a uma busca de fontes de verdade (nem na segunda chance), e so na
// primeira consulta seguida que falha — responder de novo rearma. Copia o nome
// dele em `nome`. Quem avisa compara o numero com o ultimo que viu.
unsigned addons_fora_do_ar(char *nome, unsigned tam);

AddEstado addons_estado(void);
// HA BUSCA DE FONTES EM ANDAMENTO? Leitura pura, sem os efeitos de
// addons_estado (que junta o fio e PUBLICA a lista — app.c evita chama-la
// durante a verificacao de fonte, e quem so quer saber se pode usar a rede nao
// deve ter esse efeito por tabela). E o que fontecache.c pergunta antes de
// arrancar um prefetch.
int  addons_ocupado(void);
void addons_encerrar(void);

#endif
