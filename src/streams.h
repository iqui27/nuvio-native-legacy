// Escolha da fonte de reproducao.
//
// Duas coisas moram aqui: a REGRA de qual stream tocar quando ninguem escolhe,
// e a FOLHA que lista as fontes quando o usuario quer escolher na mao.
//
// A regra vem do dono, e a ordem importa: MP4 em 4K com Dolby Vision primeiro;
// nao havendo, o primeiro da lista. "Automatico" nunca deve travar por falta do
// preferido — um app que abre a lista de fontes toda vez que o melhor formato
// falta transfere ao usuario um trabalho que e da maquina.
//
// Lista real dos addons. Resposta vazia permanece vazia, sem fontes de exemplo.
#ifndef NV_STREAMS_H
#define NV_STREAMS_H
#include <SDL2/SDL.h>
#include <stdint.h>

// A lista cresce conforme a resposta dos addons; a UI virtualiza as linhas.

typedef struct {
  char rotulo[192];     // Nome curto da fonte
  char provedor[96];
  // 1024 e nao 512. MEDIDO: os links de reproducao do AIOStreams tem 525 a 547
  // caracteres (dois segmentos assinados), e com 512 TODOS eram cortados em
  // silencio. O servidor entao respondia com um MP4 de aviso de 120s que TOCA
  // NORMALMENTE — o app parecia funcionar e mostrava o cartao de erro. Nao ha
  // erro para detectar nesse caminho, so o tamanho do campo.
  char url[4096];
  int  altura;          // 2160, 1080, 720...
  int  dolbyVision;
  int  dolbyAtmos;
  uint64_t badges;     // classificados uma vez, nunca regex no desenho
  int  mp4;             // 1 = MP4 progressivo; 0 = HLS ou outro
  long tamanhoMB;       // 0 quando desconhecido
  char descricao[2048];
  char arquivo[512];
  // O QUE O ADDON DECLARA COMO "a mesma fonte" entre episodios:
  // behaviorHints.bingeGroup, a convencao do Stremio. Quem manda o campo
  // resolve o casamento entre episodios SEM heuristica nenhuma — e o proprio
  // addon dizendo "este stream e o mesmo daquele". Vazio e o normal: muitos
  // addons nao mandam, e fontepref.c cai na assinatura de audio quando falta.
  //
  // 128 e folga sobre o que a convencao produz ("torrentio|1080p",
  // "mediafusion|<servico>|<qualidade>"): sao rotulos de agrupamento, nao
  // texto livre. Cortado dos dois lados igual continua casando, pelo mesmo
  // motivo de FONTEPREF_TRILHA.
  char bingeGroup[128];
  // Stream SEM url, so com o hash do torrent (Torrentio/Comet sem debrid na
  // URL). So entra na lista quando debrid_ativo(); a url e preenchida na
  // verificacao, por debrid_resolver.
  char infoHash[48];
  int  fileIdx;         // -1 quando o addon nao disse
  // CABECALHOS QUE O ADDON EXIGE, de behaviorHints.proxyHeaders.request, uma
  // linha "Nome: valor" por cabecalho (o mesmo formato que rede.h aceita).
  //
  // Nao e enfeite: medido em 17/09 contra o addon do relato #, o CDN responde
  // 403 sem Referer/Origin/User-Agent e 200 com eles. Ate aqui o parser entrava
  // em behaviorHints so para pegar o bingeGroup e jogava o resto fora, entao
  // TODO addon que depende de Referer estava quebrado nos dois alvos.
  //
  // 512 cobre os tres cabecalhos da convencao com folga; addon que peca mais
  // que isso perde o excedente em vez de estourar.
  char cabecalhos[512];
  // O ADDON DIZ QUE A FONTE NAO ESTA EM CACHE NO DEBRID (o "P2P" do TorBox):
  // "⏳" do AIOStreams, "[TB download]"/"[RD download]" do Torrentio, "⬇" ou
  // "uncached" no nome/descricao. Abrir o link manda o servico BAIXAR, e o que
  // toca antes de terminar e um clipe de aviso de ~8 s (registros 1136, 2191,
  // 2501). Serve a duas coisas: o automatico poe estas no fim da fila (pontos()
  // em streams.c), e a escolha manual avisa na tela que o servico esta
  // baixando. Nao exclui nada: escolhida a dedo, toca como na 1.3.5.
  int  foraCache;
} Stream;

// Parser sem rede: o chamador libera *saida. Retorna -1 se a alocacao falhar.
int stream_extrair(const char *json, const char *provedor, Stream **saida);
void stream_definir_atual(int indice);
int stream_atual(void);
void stream_folha_contexto(const char *texto);
int stream_folha_recarregar(void);

// Substitui a lista do titulo corrente. Chamar quando os addons responderem.
void stream_definir_lista(const Stream *lista, int n);

// DE QUEM E A LISTA QUE ESTA EM MEMORIA — issue #101.
//
// Ate a 1.3.12 a lista aqui era global E ANONIMA: um vetor de fontes sem
// nenhum registro do episodio para o qual foi pedida. Quem a usava (a escolha
// automatica em app.c e a folha de fontes) so podia confiar que quem trocou de
// episodio tambem mandou refazer a busca. Bastava UM caminho nao mandar para o
// E6 reproduzir a fonte do E5 — sem erro nenhum no log, porque nao ha erro: a
// lista estava la, valida, do episodio errado.
//
// O alvo e o mesmo id que vai aos addons ("tt1234567:temporada:episodio", ou o
// id do canal). stream_definir_alvo carimba o PROXIMO pedido; a lista que
// chegar herda o carimbo, porque quem publica (addons.c) nao conhece o alvo em
// que o app esta — ele conhece o dele, que pode ja estar obsoleto.
//
// stream_lista_do_alvo devolve 0 tambem quando a lista esta vazia ou sem
// carimbo: em duvida a resposta e "nao e sua", e o custo de errar para este
// lado e uma busca a mais.
void stream_definir_alvo(const char *id);
int  stream_lista_do_alvo(const char *id);
// Descarta a lista porque ela e de outro alvo. `porque` so entra no log.
void stream_invalidar(const char *porque);
int  stream_n(void);
const Stream *stream_item(int i);

// Indice do stream que o modo automatico escolhe, ou -1 se a lista esta vazia.
int  stream_automatico(void);
// Exclui uma candidata que ja foi entregue ao player e travou no pipeline.
// A exclusao vale so para a lista atual; uma resposta nova limpa a memoria.
int  stream_automatico_excluir(int indice);

// A FONTE LEMBRADA DESTE TITULO, quando ela existe nesta lista. Quem decide
// qual e (provedor + trilha de audio) e fontepref.c; aqui ela e um indice que
// stream_primeira_boa poe NA FRENTE da fila de verificacao, e que a folha
// marca na tela. -1 desliga.
//
// Nao substitui stream_automatico(): a preferida e uma CANDIDATA, verificada
// como as outras. Sumiu da lista, ou o link nao resolve, e a pontuacao assume
// sem que ninguem precise escolher nada.
//
// A lista nova zera isto (stream_definir_lista): indice da lista de ontem
// aponta para outra fonte hoje.
void stream_preferir(int indice);
int  stream_preferida(void);

// Ha quantos ms a lista chegou. Os links de reproducao dos servicos de debrid
// sao ASSINADOS E EXPIRAM: usar um link de minutos atras faz o servidor
// redirecionar para um video de aviso ("This playback link couldn't be
// verified", 120s, 720p) que TOCA NORMALMENTE — ou seja, falha parecendo
// sucesso. Renovar antes de reproduzir e o que evita isso.
Uint32 stream_idade_ms(void);

// Percorre as fontes na ordem da regra e devolve a primeira cujo link resolve
// para conteudo DE VERDADE, testando ate `tentativas`. -1 se nenhuma serve.
// BLOQUEIA — chamar de fio proprio.
//
// EM SERIE, UMA URL POR VEZ, e para na primeira que serve (issue #130): cada
// candidata conferida de debrid vira um arquivo na conta da pessoa. Com
// "Fonte automatica" em "Primeira da lista" `tentativas` vira 1 — so a fonte
// que vai tocar e conferida. As que falham saem da fila desta lista
// (stream_automatico_excluir) e nao sao conferidas de novo.
int  stream_primeira_boa(int tentativas);

// 1 quando o texto da fonte diz "fora de cache" (ver Stream.foraCache).
// Publica para o parser e o teste; a lista ja vem com o campo preenchido.
int  stream_texto_fora_de_cache(const char *texto);

// TORRENT SEM URL ESCOLHIDO A DEDO NA FOLHA. A escolha manual chamava
// player_definir_fonte(s->url) com a url VAZIA — e player_definir_fonte volta
// calado com url vazia: o player ficava em "carregando" para sempre. So o
// automatico passava por debrid_resolver (registros 1739, 2325: "fonte
// escolhida: Torrentio 1080p" e nenhuma linha "[video] URL" depois).
//
// BLOQUEIA (rede do debrid) — chamar de fio proprio. Resolve com
// debrid_resolver_escolhido, que PODE mandar o servico baixar um torrent fora
// de cache, porque quem escolheu foi a pessoa. `geracao` e a de
// stream_lista_geracao() no momento da escolha: lista trocada no meio devolve
// -1 sem gravar nada.
//   1  -> `url` pronta (e gravada na linha, para a proxima vez)
//   2  -> DEBRID_BAIXANDO: `servico` e `pct` dizem quem baixa e quanto falta
//   0  -> nao deu; -1 -> lista trocada
unsigned stream_lista_geracao(void);
int  stream_resolver_escolhida(int i, unsigned geracao, char *url, unsigned nu,
                               char *servico, unsigned ns, int *pct);

// CANAL AO VIVO: a primeira fonte da lista cuja PLAYLIST tem segmento, com as
// candidatas conferidas em paralelo. Existe porque a verificacao de filme
// (stream_primeira_boa) custa um rede_url_final de 10 s por candidata, e porque
// entregar a primeira sem conferir custava 12 s de watchdog POR FONTE MORTA —
// medido em quase dois minutos num canal com seis mortas. Ver a nota longa na
// definicao. Devolve -1 quando nenhuma respondeu com segmento.
int  stream_canal_primeira_viva(int tentativas);

// Classe da fonte que stream_canal_primeira_viva acabou de escolher:
// 1 = VIVA (playlist com segmento), 3 = MUDA (nao respondeu a tempo), 0 = nenhuma.
// Serve para o chamador dar prazo menor a quem ja provou estar ruim.
int  stream_canal_classe_escolhida(void);

// --- folha de fontes (a lista que sobe por cima do player/detalhe) ---
void stream_folha_abrir(void);
int  stream_folha_aberta(void);
void stream_folha_evento(const SDL_Event *e);
void stream_folha_atualizar(float dt, Uint32 agora);
void stream_folha_desenhar(Uint32 agora);
// Devolve 1 uma vez quando o usuario escolheu, com o indice em *escolhido.
int  stream_folha_escolheu(int *escolhido);

#endif
