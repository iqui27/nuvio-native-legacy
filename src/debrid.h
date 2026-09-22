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
// Regra comum aos tres: so resolve o que JA ESTA EM CACHE no servico. Mandar
// o servico comecar a baixar e ficar esperando nao e "tocar agora", e a TV
// ficaria parada num fio de rede sem nada para mostrar.
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
int  debrid_resolver(const char *infoHash, int fileIdx, char *url, unsigned n);

#endif
