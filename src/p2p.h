// P2P EXPERIMENTAL: tocar torrent (infoHash + fileIdx) SEM conta de debrid, por
// um servidor de streaming do Stremio (server.js, porta 11470) que a pessoa
// roda na rede local (PC, NAS, Docker: `stremio/server`).
//
// Ver docs/P2P-EXPERIMENTAL.md para a comparacao com as outras rotas (motor
// embutido, debrid direto) e por que esta foi a escolhida.
//
// A TV NAO BAIXA TORRENT NENHUM. Ela conversa HTTP com o servidor, que fala
// BitTorrent, e depois toca o proxy de Range dele:
//
//   1. GET  <base>/settings                 -> e mesmo um servidor Stremio?
//   2. POST <base>/<hash>/create            -> registra o torrent (com os
//                                              trackers do addon) e devolve a
//                                              lista de arquivos quando os
//                                              metadados chegam
//   3. GET  <base>/<hash>/<fileIdx>         -> o video, com Range
//
// TUDO MEDIDO contra stremio/server 4.21.2 em Docker, com o Big Buck Bunny
// (dd8255ecdc7c...): create devolve 200 com "files" em segundos, o GET com
// Range responde 206, fileIdx -1 escolhe o maior arquivo. Um hash SEM peers
// nao responde NUNCA (130 s sem uma linha de resposta): o prazo tem de ser do
// cliente, e e por isso que existe P2P_ERR_SEM_PEERS.
//
// DESLIGADO de fabrica. Ligado, so muda uma coisa: torrent sem url deixa de ser
// descartado e passa a poder ser ESCOLHIDO A DEDO na folha de fontes. O
// automatico nunca toca P2P: um torrent sem peers prende a TV parada.
#ifndef NV_P2P_H
#define NV_P2P_H

typedef enum {
  P2P_OK = 0,
  P2P_ERR_DESLIGADO,     // ajuste desligado ou sem endereco
  P2P_ERR_HASH,          // infoHash que nao e hexadecimal de 40 caracteres
  P2P_ERR_SERVIDOR,      // o servidor nao respondeu (fora do ar, IP errado)
  P2P_ERR_NAO_STREMIO,   // respondeu, mas nao e um servidor Stremio
  P2P_ERR_SEM_PEERS,     // metadados ou primeiros bytes nao chegaram no prazo
  P2P_ERR_SEM_VIDEO,     // o torrent nao tem arquivo de video
  P2P_ERR_RECUSOU,       // o servidor respondeu erro (5xx / 4xx)
  P2P_ERR_MOTOR,         // motor embutido (p2pmotor.h) nao subiu (libtorrent recusou)
  P2P_ERR_SEM_ESPACO,    // motor embutido: pouco livre (< 256 MB), ENOSPC ou teto duro
  P2P_ERR_DISCO,         // motor embutido: statvfs falhou -> recusa (conservador)
  P2P_ERR_RAM,           // motor embutido: cache em RAM passou do teto duro
  P2P_ERR_CANCELADO,     // pedido cancelado (outra escolha, player fechado, perfil)
  P2P_ERR_OCUPADO        // outro pedido ao motor ainda em curso

} P2pErro;

// Prazos em segundos. Metadados de um torrent com peers chegam em 1 a 5 s; 30
// cobre swarm pequeno sem deixar a TV parada. Os primeiros bytes incluem
// conectar em peers e baixar a primeira peca (alguns MB em torrent mal semeado).
#define P2P_PRAZO_TESTE     6
#define P2P_PRAZO_METADADOS 30
#define P2P_PRAZO_BYTES     25

// Normaliza o que a pessoa digitou: "192.168.1.5", "192.168.1.5:11470",
// "http://nas.local:11470/". Sem esquema vira http://; sem porta vira 11470;
// sem barra no fim. So http/https e so [A-Za-z0-9.-:[]] no host (IPv6 entre
// colchetes). 1 se valido (em `out`), 0 se nao. Texto vazio NAO e valido.
int p2p_normalizar_url(const char *entrada, char *out, unsigned n);

// Ligado E (com endereco valido OU com o motor embutido neste build): a unica
// pergunta que o resto do app faz. Endereco preenchido vence o motor: quem
// configurou um servidor na rede quer que a TV nao baixe nada.
int p2p_ativo(void);
// 1 quando o P2P ligado vai pelo motor embutido (sem endereco, com motor).
int p2p_usa_motor(void);

// NUVIO CASA (casa.h): `base` devolve 1 e o endereco do ajudante achado na rede
// (sem bloquear), ou 0. So vale sem endereco digitado e sem motor neste build.
void p2p_definir_casa(int (*base)(char *, unsigned));
// 1 quando o P2P ligado vai pelo ajudante da casa.
int p2p_usa_casa(void);

// 40 caracteres hexadecimais.
int p2p_hash_valido(const char *hash);

// Corpo do POST /<hash>/create. `fontes` sao as entradas do campo "sources" do
// stream, uma por linha ("tracker:udp://..." / "dht:<hash>"); NULL/vazio vale.
// O "dht:<hash>" do proprio torrent e sempre acrescentado. Devolve o tamanho,
// 0 se nao coube.
int p2p_corpo_criar(const char *hash, const char *fontes, char *out, unsigned n);

// Escolhe o arquivo de video dentro do JSON do create/stats.json ("files":
// [{"path","name","length","offset"}]). Ordem: o fileIdx do addon se for
// video > SxxEyy (temporada/episodio de debrid_episodio) > maior video.
// Devolve o indice no torrent (0..) ou -1 se nao ha video.
int p2p_escolher_arquivo(const char *json, int fileIdx, int temporada, int episodio);

// A mesma escolha sobre listas soltas (motor embutido): `nomes[i]` com pasta,
// `tam[i]` em bytes; nome NULL/vazio e pulado.
int p2p_escolher_lista(const char *const *nomes, const double *tam, int n,
                       int fileIdx, int temporada, int episodio);

// magnet:?xt=urn:btih:<hash>&tr=... com os "tracker:" de `fontes` (uma por
// linha, como em p2p_corpo_criar) percent-encoded, mais trackers publicos de
// reserva quando o addon nao mandou nenhum. 1 se coube.
int p2p_magnet(const char *hash, const char *fontes, char *out, unsigned n);

// <base>/<hash>/<idx>, hash em minusculas. 1 se coube.
int p2p_url_reproducao(const char *base, const char *hash, int idx, char *out, unsigned n);

// BLOQUEIA (fio proprio). Faz os passos 1 a 3 e devolve P2P_OK com `url` pronta,
// ou o motivo. O motivo tambem fica em p2p_ultimo_erro().
int p2p_resolver(const char *hash, int fileIdx, const char *fontes,
                 char *url, unsigned n);
int p2p_ultimo_erro(void);

// "Testar conexao" dos Ajustes: passo 1 sozinho. `detalhe` recebe a versao do
// servidor quando P2P_OK ("4.21.2").
int p2p_testar(char *detalhe, unsigned n);
#endif
