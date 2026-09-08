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
typedef struct { char nome[64]; char url[600]; int ativo; } AddonRemoto;
// Devolve 1 quando a lista MUDOU e foi aplicada; 0 quando nada mudou, quando
// veio vazia, ou quando nada nela era utilizavel. Quem chama usa isso para
// decidir se vale remontar o catalogo — e nao para saber quantos addons ha.
int  addons_definir_lista(const AddonRemoto *lista, int n);

// Lista atual, para o sync poder empurrar de volta o que este aparelho tem.
int  addons_exportar(AddonRemoto *saida, int max);

// Esquece a lista da conta. Chamado ao SAIR: sem isto, a proxima pessoa a usar
// esta TV navega com os addons da anterior — e como as chaves de debrid vao
// embutidas nas URLs, ela tambem consome a assinatura da anterior — ate o
// primeiro sync terminar. Ficar sem fonte por alguns segundos e o
// comportamento correto de "ninguem logado".
void addons_esquecer(void);
int  addons_n(void);
const char *addons_base(int i);   // URL base, sem /manifest.json
const char *addons_base_por_id(const char *idManifesto);   // "" ate a sonda conhecer o id
int  addons_tem_catalogo(int i);  // 1 quando o addon fornece catalogo

// Dispara a busca das fontes de `imdb` ("tt1234567", ou "tt1234567:1:2" para
// episodio). Volta na hora; o resultado chega por stream_definir_lista.
void addons_buscar(const char *imdb, const char *tipo);

// --- legendas externas (OpenSubtitles) ---------------------------------------
// Addon de legenda responde em /subtitles/<tipo>/<id>.json com
// {"subtitles":[{lang,url,subtitleFileName,...}]}. Sao dezenas por titulo, a
// maioria em idiomas que nao interessam — por isso a lista e FILTRADA por
// idioma antes de chegar na tela: 70 linhas para rolar seria pior que nenhuma.
#define LEG_MAX 12

typedef struct {
  char rotulo[64];   // "Portugues (BR)  ·  Silo.S01E05.WEB"
  char idioma[8];
  char url[600];
} Legenda;

void addons_buscar_legendas(const char *imdb, const char *tipo);

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
const Legenda *addons_legenda(int i);


// --- lista para a tela de addons --------------------------------------------
//
// A conta pode ter addon DESLIGADO, e ele continua na lista: some das consultas
// mas aparece na tela, para poder ser religado sem pegar o celular.
enum { ADD_CATALOGO = 0, ADD_STREAM, ADD_LEGENDA };

const char *addons_nome(int i);
int  addons_ativo(int i);
int  addons_alternar(int i);          // devolve o estado NOVO
// O addon fornece este recurso? Ate a sonda responder e uma suposicao
// otimista; addons_sondado() diz qual dos dois casos e.
int  addons_fornece(int i, int oque);
int  addons_sondado(int i);
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

AddEstado addons_estado(void);
void addons_encerrar(void);

#endif
