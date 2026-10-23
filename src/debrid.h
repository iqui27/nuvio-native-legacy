// Resolucao LOCAL de torrent via debrid (Real-Debrid, TorBox, Premiumize e
// AllDebrid),
// directDebridResolver.js do app web.
//
// Addons como Torrentio/Comet sem chave de debrid na URL devolvem streams so
// com `infoHash`, sem `url`. O web pega a chave de debrid da CONTA
// (sync_pull_provider_credentials, provider "debrid:realdebrid") e transforma
// o hash num link direto na hora de tocar. Sem isto o app nativo descartava
// todos esses streams e a lista vinha vazia — "addon streams don't work".
//
// TRES SERVICOS: Real-Debrid, TorBox e Premiumize. Ate a 1.3.12 so o primeiro
// tinha resolvedor, e quem tinha outro via "servico sem resolvedor aqui" no
// log e depois perdia TODAS as fontes do torrent ("145 torrents sem debrid
// descartados") — 13 das 26 pessoas cujo registro chegou dessa versao.
//
// Regra do AUTOMATICO: so resolve o que JA ESTA EM CACHE no servico. Mandar
// o servico comecar a baixar e ficar esperando nao e "tocar agora", e a TV
// ficaria parada num fio de rede sem nada para mostrar. A escolha MANUAL de
// um torrent fora de cache e a excecao: debrid_resolver_escolhido.
#ifndef NV_DEBRID_H
#define NV_DEBRID_H

#include <stdint.h>

// IDs estaveis para a ordem e o orcamento locais de cada servico. Acrescentar
// novos servicos no fim: estes numeros podem ficar nos ajustes do aparelho.
enum { DEBRID_RD = 0, DEBRID_TB = 1, DEBRID_PM = 2, DEBRID_AD = 3, DEBRID_N = 4 };
const char *debrid_servico_nome(int servico);   // nome para a tela; "" se invalido
const char *debrid_servico_id(int servico);     // slug da credencial; "" se invalido
int debrid_servico_por_nome(const char *nome); // nome/slug do servico; -1 se desconhecido
// Tem chave efetiva e nao foi recusado nesta busca nem esta sem plano.
int debrid_servico_disponivel(int servico);

// Conta lida pelo sync: uma resposta e um conjunto completo, inclusive os
// provedores ausentes. O worker preenche a copia; so a sessao que a pediu a
// publica. Nunca transportar a chave em log, URL de diagnostico ou i18n.
typedef struct { char chave[4][200]; } DebridConta;
void debrid_conta_iniciar(DebridConta *conta);
void debrid_conta_definir(DebridConta *conta, const char *servico, const char *chave);
int  debrid_aplicar_chaves_conta(const DebridConta *conta, uint64_t sessao);
void debrid_aplicar_chaves_locais(const DebridConta *conta);
void debrid_abrir_sessao(uint64_t sessao);

// Capturar ao despachar um trabalho, antes de trocar de perfil. Todas as
// chamadas HTTP dele leem estas chaves e este episodio, nunca os da pessoa
// que entrou enquanto o worker estava na rede. Contexto vive ate sair().
typedef struct {
  char chave[4][200];
  int temporada, episodio;
  uint64_t sessao;
  unsigned busca;
} DebridContexto;
void debrid_contexto_capturar(DebridContexto *contexto);
void debrid_contexto_entrar(const DebridContexto *contexto);
void debrid_contexto_sair(void);

// Chave vinda da conta. `servico` e o sufixo de "debrid:<servico>":
// realdebrid/real-debrid, torbox e premiumize (com ou sem hifen). Uma chave
// por servico; chamar de novo para outro servico NAO apaga a anterior.
void debrid_definir_chave(const char *servico, const char *chave);
int  debrid_ativo(void);          // ha chave de um servico que sabemos resolver
// "https://host" da API de cada servico com chave que pode resolver (aquecer.c).
int  debrid_origens(const char **origens, int max);
void debrid_esquecer(void);       // logout (a chave local, digitada na TV, fica)

// Chave DIGITADA NESTA TV (Ajustes > Integracoes > Debrid). Vale no lugar da
// que veio da conta, para o mesmo servico; vazia apaga. Aceita os mesmos nomes
// de debrid_definir_chave, mais "alldebrid"/"all-debrid" — servico que o app
// web oficial nao tem, entao so chega por aqui.
void debrid_definir_chave_local(const char *servico, const char *chave);
// De onde vem a chave em uso: 0 nenhuma, 1 conta, 2 digitada nesta TV.
int  debrid_origem(const char *servico);
// "····abcd" (so os 4 ultimos) ou "" sem chave. Devolve `dst`.
const char *debrid_chave_mascarada(const char *servico, char *dst, unsigned n);
// "Testar chave" do AllDebrid (GET /v4/user). BLOQUEIA: usar num fio. 1 =
// conta premium (`msg` diz ate quando); 0 = nao (`msg` diz o motivo). A frase
// nunca leva a chave nem o usuario/e-mail da conta.
// `msg` e sempre uma chave de i18n; premium com vencimento traz a data
// dd/mm/aaaa em `data` (quem mostra usa "premium até %s").
int  debrid_testar_alldebrid(char *msg, unsigned n, char *data, unsigned nd);
// Mesmo contrato para RD/TB/PM; cache por chave de 5 min, inclusive falhas.
// BLOQUEIA: usar num fio. Tambem alimenta os avisos de conta antes de resolver.
int  debrid_testar_conta(int servico, char *msg, unsigned n, char *data, unsigned nd);

// Episodio alvo da proxima resolucao (0,0 = filme). Serve para escolher o
// arquivo certo dentro de um torrent de temporada inteira.
void debrid_definir_episodio(int temporada, int episodio);
// O mesmo par, de volta: o P2P (p2p.c) escolhe o arquivo do episodio com ele.
void debrid_episodio(int *temporada, int *episodio);

// BLOQUEIA. Devolve 1 e grava em `url` um link direto que toca; 0 se nao deu.
// So conteudo EM CACHE: e o caminho da escolha AUTOMATICA.
int  debrid_resolver(const char *infoHash, int fileIdx, char *url, unsigned n);
// Mesma consulta: *fora recebe a negativa conhecida deste torrent, nunca erro HTTP.
int  debrid_resolver_cache(const char *infoHash, int fileIdx, char *url, unsigned n, int *fora);
// Resolve SOMENTE este servico, sem gerar links nos seguintes. A url sai vazia
// em toda falha; *fora = 1 somente se ESTE servico respondeu fora de cache.
// Erro de conta, HTTP e falta de chave nao sao uma negativa de cache.
int  debrid_resolver_servico_cache(int servico, const char *infoHash, int fileIdx,
                                   char *url, unsigned n, int *fora);

// A PESSOA ESCOLHEU ESTE TORRENT NA FOLHA — e ai a regra "so o que esta em
// cache" nao vale mais. E o "P2P" do TorBox: o torrent fora de cache que o
// servico baixa por P2P. No Stremio/Nuvio oficial, escolher um desses manda o
// servico baixar (o addon abre um link que faz o createtorrent) e toca quando
// termina; aqui, ate esta correcao, o TorBox respondia "fora de cache" e a
// fonte simplesmente nao tocava — e a escolha manual nem chegava ao debrid
// (ver stream_resolver_escolhida em streams.h).
//
// Primeiro tenta como debrid_resolver (em cache em QUALQUER servico com
// chave — cacheado no Premiumize nao manda o TorBox baixar). Nao havendo,
// pede ao TorBox para BAIXAR (createtorrent SEM add_only_if_cached) e olha o
// progresso por uns segundos; o Real-Debrid ja comeca a baixar no addMagnet.
//   1               -> `url` pronta, toca; `servico` recebe quem resolveu
//   DEBRID_BAIXANDO -> o servico esta baixando: `servico` recebe o nome
//                      ("TorBox") e `*pct` o progresso 0-100 (-1 se nao sabe).
//                      O torrent fica na conta; escolher de novo depois toca.
//   0               -> nao deu (sem chave, conta recusou, sem video)
#define DEBRID_BAIXANDO 2
int  debrid_resolver_escolhido(const char *infoHash, int fileIdx, char *url,
                               unsigned n, char *servico, unsigned ns, int *pct);

// Quantos torrents a busca atual achou FORA DE CACHE (o automatico nao os
// toca). Zera em debrid_nova_busca. Serve ao cartao de "nenhuma fonte serve"
// dizer que ha torrent a baixar e que a folha manda baixar.
int  debrid_fora_de_cache(void);

// UMA BUSCA DE FONTES COMECOU (addons_buscar). Esquece as recusas de CONTA da
// busca anterior: a proxima busca volta a tentar todo servico com chave.
//
// Por que existe (registro 1541, webOS 1.4.0): o TorBox devolveu HTTP 403 no
// createtorrent para 8 torrents seguidos da MESMA busca. Erro de conta (plano,
// limite, chave) nao muda de um torrent para o outro — cada tentativa a mais
// era uma viagem de ate 15 s e a mesma linha no log. Ver debrid.c.
void debrid_nova_busca(void);

// O servico que recusou pela CONTA nesta busca, como texto curto
// ("TorBox 403"). Devolve 1 e escreve em `dst` quando houve; 0 se nao. Serve a
// quem mostra "nenhuma fonte serve" dizer a causa em vez da frase generica.
int  debrid_recusa(char *dst, unsigned n);

// CONTA SEM PLANO (TorBox PLAN_RESTRICTED_FEATURE, Premiumize "Account not
// premium."): o servico fica fora pela SESSAO, nao so pela busca. Ver debrid.c.
//   debrid_eh_sem_plano    — a resposta (status + corpo) diz isso?
//   debrid_sem_plano       — mascara dos servicos com chave nesse estado
//   debrid_sem_plano_novo  — a mesma mascara, mas so dos que ainda nao foram
//                            avisados; marca como avisados (um aviso por sessao)
//   debrid_sem_plano_frase — a frase curta em portugues (chave de i18n) para a
//                            mascara, ou NULL se ela for 0
int  debrid_eh_sem_plano(int st, const char *corpo);
int  debrid_sem_plano(void);
int  debrid_sem_plano_novo(void);
const char *debrid_sem_plano_frase(int mascara);
// O NOME do servico quando a mascara tem um so e e caso de PLANO (nao a chave
// do AllDebrid recusada): "TorBox", "Premiumize", "Real-Debrid", "AllDebrid".
// NULL nos outros casos — quem chama usa a frase longa de cima. Para a frase
// curta da ilha ("Seu TorBox está sem plano...").
const char *debrid_sem_plano_nome(int mascara);

// ARQUIVOS DE UMA FONTE NO TORBOX (partes.h). Nao dependem da busca em curso;
// a chave e a efetiva do TorBox na hora da chamada. Nenhuma URL vai para log.
//   debrid_tb_disponivel: 1 ha chave que pode usar a API, 0 sem chave,
//                         -1 conta sem plano para a API.
//   debrid_tb_mylist:     BLOQUEIA. GET torrents/mylist ou usenet/mylist
//                         (id <= 0 = todos os itens; fresco = bypass_cache,
//                         sem ele vem a copia em cache do TorBox, mais
//                         rapida). Corpo (free) ou NULL; *st recebe o HTTP.
//   debrid_tb_link:       BLOQUEIA. requestdl de um arquivo; 1 = `url` e o
//                         link do CDN.
//   debrid_tb_adicionar:  BLOQUEIA. Poe o torrent na conta SO se ja estiver em
//                         cache; devolve o id ou -1.
//   debrid_tb_airlock:    BLOQUEIA. PUT de edicao do item (torrent ou usenet) so
//                         com `airlocked`: 1 liga o Airlock (fora da purga de 30
//                         dias), 0 desliga. 1 = feito; 0 = recusado ou sem
//                         resposta, e `detalhe` (pode ser NULL) recebe a frase do
//                         TorBox (cota do plano, por exemplo).
// Changes only with account session or effective TorBox key, never a search.
unsigned debrid_tb_geracao(void);
int   debrid_tb_disponivel(void);
// Uma pagina de DEBRID_TB_PAGINA itens (o teto do TorBox): conta com mais
// itens que isso precisa de mais paginas (`pagina` 0, 1, ...).
#define DEBRID_TB_PAGINA 1000
char *debrid_tb_mylist(int usenet, int id, int pagina, int fresco, int *st);
int   debrid_tb_link(int usenet, int id, int arquivo, char *url, unsigned n);
int   debrid_tb_adicionar(const char *hash);
int   debrid_tb_airlock(int usenet, int item, int ligar, char *detalhe, unsigned n);

#endif
