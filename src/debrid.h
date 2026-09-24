// Resolucao LOCAL de torrent via debrid (Real-Debrid, TorBox, Premiumize),
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

// Chave vinda da conta. `servico` e o sufixo de "debrid:<servico>":
// realdebrid/real-debrid, torbox e premiumize (com ou sem hifen). Uma chave
// por servico; chamar de novo para outro servico NAO apaga a anterior.
void debrid_definir_chave(const char *servico, const char *chave);
int  debrid_ativo(void);          // ha chave de um servico que sabemos resolver
void debrid_esquecer(void);       // logout

// Episodio alvo da proxima resolucao (0,0 = filme). Serve para escolher o
// arquivo certo dentro de um torrent de temporada inteira.
void debrid_definir_episodio(int temporada, int episodio);

// BLOQUEIA. Devolve 1 e grava em `url` um link direto que toca; 0 se nao deu.
// So conteudo EM CACHE: e o caminho da escolha AUTOMATICA.
int  debrid_resolver(const char *infoHash, int fileIdx, char *url, unsigned n);

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
//   1               -> `url` pronta, toca
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

#endif
