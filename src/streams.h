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
  // Stream SEM url, so com o hash do torrent (Torrentio/Comet sem debrid na
  // URL). So entra na lista quando debrid_ativo(); a url e preenchida na
  // verificacao, por debrid_resolver.
  char infoHash[48];
  int  fileIdx;         // -1 quando o addon nao disse
} Stream;

// Parser sem rede: o chamador libera *saida. Retorna -1 se a alocacao falhar.
int stream_extrair(const char *json, const char *provedor, Stream **saida);
void stream_definir_atual(int indice);
int stream_atual(void);
void stream_folha_contexto(const char *texto);
int stream_folha_recarregar(void);

// Substitui a lista do titulo corrente. Chamar quando os addons responderem.
void stream_definir_lista(const Stream *lista, int n);
int  stream_n(void);
const Stream *stream_item(int i);

// Indice do stream que o modo automatico escolhe, ou -1 se a lista esta vazia.
int  stream_automatico(void);

// Ha quantos ms a lista chegou. Os links de reproducao dos servicos de debrid
// sao ASSINADOS E EXPIRAM: usar um link de minutos atras faz o servidor
// redirecionar para um video de aviso ("This playback link couldn't be
// verified", 120s, 720p) que TOCA NORMALMENTE — ou seja, falha parecendo
// sucesso. Renovar antes de reproduzir e o que evita isso.
Uint32 stream_idade_ms(void);

// Percorre as fontes na ordem da regra e devolve a primeira cujo link resolve
// para conteudo DE VERDADE, testando ate `tentativas`. -1 se nenhuma serve.
// BLOQUEIA — chamar de fio proprio.
int  stream_primeira_boa(int tentativas);

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
