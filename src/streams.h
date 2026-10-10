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
#include "streamfit.h"

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
  // Per-file bytes declared by behaviorHints.videoSize, without rounding or
  // text/season-pack heuristics. 0 = no trustworthy exact-size provenance.
  // https://github.com/Stremio/stremio-addon-sdk/blob/master/docs/api/responses/stream.md
  uint64_t tamanhoBytes;
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
  // behaviorHints.videoHash (hash do OpenSubtitles) quando o addon de fonte
  // manda; vai para a busca de legendas como extra do Stremio (#201).
  char videoHash[24];
  // Stream SEM url, so com o hash do torrent (Torrentio/Comet sem debrid na
  // URL). So entra na lista quando debrid_ativo(); a url e preenchida na
  // verificacao, por debrid_resolver.
  char infoHash[48];
  int  fileIdx;         // -1 quando o addon nao disse
  // Quantos semeiam o torrent, lido do texto do addon ("👤 12" no Torrentio).
  // So vale com temSemeadores: zero lido e zero desconhecido sao coisas
  // diferentes, e "0 seeds" e justamente o aviso que importa.
  int  semeadores, temSemeadores;
  // "sources" do stream (Stremio): trackers e nos DHT do torrent, UMA entrada
  // por linha ("tracker:udp://...", "dht:<hash>"). So serve ao P2P
  // experimental (p2p.c), que os repassa ao servidor de streaming. 640 cobre
  // uma dezena de trackers; o excedente e cortado numa entrada inteira.
  char fontes[640];
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
  // A SONDA VIU A URL RESPONDER PAGINA (HTML/JSON/texto) e nao video (2.0.2,
  // naovideo.h): linha de aviso do addon. A folha a mostra apagada ("Nao e
  // video") e o automatico a pula. Marcada na verificacao, nunca no parser.
  int  naoVideo;
  // PACOTE DE SELOS ATIVO (selospacote.h): os filtros que casaram com esta
  // fonte, calculados UMA vez (quando a lista chega, ou quando o pacote muda;
  // `selosPacoteVer` != selospacote_versao() manda recalcular), nunca no
  // desenho. 16 = SELOS_MAX_CASADOS.
  unsigned short selosPacote[16];
  unsigned char  nSelosPacote;
  unsigned       selosPacoteVer;
} Stream;

// Parser sem rede: o chamador libera *saida. Retorna -1 se a alocacao falhar.
int stream_extrair(const char *json, const char *provedor, Stream **saida);
void stream_definir_atual(int indice);
int stream_atual(void);
void stream_folha_contexto(const char *texto);
// O nome do conteudo da lista (titulo do filme ou da serie), para o titulo de
// cada linha no modo "Do Nuvio" ("Silo  Temporada 2 Episodio 5"). Chamar antes
// de stream_folha_abrir; vazio cai no nome do addon.
void stream_folha_nome(const char *nome);
// Indice no catalogo do conteudo da folha (-1: canal ou nenhum), para a logo
// do titulo no lugar do nome (Ajustes > Texto das fontes > Logo do titulo).
void stream_folha_item(int indice);
// Folha de um CANAL ao vivo: a resolucao e o codec saem do nome (UHD/FHD/HD/SD,
// H.265...) e entram no agrupamento e nos selos. Filme nao muda.
void stream_folha_canal(int sim);
void stream_canal_enriquecer(Stream *s);
int stream_folha_recarregar(void);
// Abertura animada da folha (0..1): o player apaga o OSD por baixo dela.
float stream_folha_anim(void);

// Substitui a lista do titulo corrente. Chamar quando os addons responderem.
void stream_definir_lista(const Stream *lista, int n);
// Lista reaproveitada: conserva a idade da resposta original dos addons.
// O cache de metadados nao pode dar validade nova a um link assinado antigo.
void stream_definir_lista_idade(const Stream *lista, int n, Uint32 idade);
// ACRESCENTA as fontes de UM addon a lista corrente, sem substitui-la (#221):
// a busca publica cada addon que responde. Os indices de quem ja estava NAO
// mudam (a verificacao em curso, a fonte tocando, a preferida e as excluidas
// continuam valendo); a ORDEM DE EXIBICAO e por `ordemAddon` (o indice do
// addon na lista instalada) e, dentro dele, a ordem que o addon mandou — a
// mesma da lista inteira de antes. Com a folha aberta, o foco fica no mesmo
// cartao e a rolagem compensa as linhas que entraram acima dele.
void stream_lista_acrescentar(const Stream *lista, int n, int ordemAddon);
// O addon (ordemAddon) de cada fonte, como entrou; 0 na lista inteira.
int  stream_ordem_addon(int i);
// A escolha automatica ja pode sair com a lista parcial? Ver
// fonteauto_pode_decidir. `preferida` e o indice da lembrada nesta lista (-1).
// `instantaneo` (#202): "Espera pelos add-ons" = Instantaneo.
int  stream_auto_pode_decidir(int preferida, int prefPendente, int prazoPassou, int instantaneo);
// REGRAS DE AUTO-PLAY (#202, fonteregra.h): o grupo da fonte i (-1 = fora,
// 0..3 = ordem) e se a ultima escolha so falhou por causa delas.
int  stream_grupo_regra(int i);
int  stream_regra_bloqueou(void);
// Candidatas que o automatico ainda pode tentar nesta lista (nao excluidas).
int  stream_n_candidatas(void);

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
// Fileira de selos do pacote ativo (ver selospacote.h) para fora da folha, como
// o cartao "Abrindo fonte" do player: padrao em cinza `tom`, colorido em pecas.
// `stream_selos_ha` diz se ha o que desenhar (a resolucao entra); 0 de largura
// = nada, e quem chama usa a mascara de badges.h.
int   stream_selos_ha(const Stream *s);
float stream_selos_fileira(const Stream *s, float x, float y, float maxW, float h, float tom, float a);

// Indice do stream que o modo automatico escolhe, ou -1 se a lista esta vazia.
int  stream_automatico(void);
// A candidata que o automatico usaria se `atual` falhar (e nao esta excluida)
// existe e NAO e pior que ela: mesma resolucao ou maior, Dolby Vision igual ou
// melhor. 0 = nao ha proxima, ou a proxima baixaria a qualidade (#202).
int  stream_proxima_sem_perda(int atual);
// O mesmo sem as regras de auto-play (#202): canal ao vivo.
int  stream_automatico_canal(void);
// Exclui uma candidata que ja foi entregue ao player e travou no pipeline.
// A exclusao vale so para a lista atual; uma resposta nova limpa a memoria.
// 1 = a fonte `indice` existe e ainda nao foi descartada pelo automatico nesta lista.
int  stream_automatico_disponivel(int indice);
int  stream_automatico_excluir(int indice);
// Exclui tambem as IRMAS da candidata (mesmo addon e mesmo rotulo), mas so
// quando sobra outra candidata: quando o player nao conectou numa, as outras
// costumam falhar igual.
int  stream_automatico_excluir_irmas(int indice);

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
// TOCAR ENQUANTO CONFERE (fonteantecipa.h): o indice da candidata que o player
// ja pode abrir enquanto a conferencia dela roda, e o estado (FA_*). -1 = nada.
int  stream_antecipada(int *estado);
// Ate `max` URLs (256 bytes cada; so o host importa) das primeiras fontes que o
// automatico tentaria, para aquecer.c abrir a conexao antes. Nunca resolve nada.
int  stream_urls_para_aquecer(char dst[][256], int max);
// Conferencia de uma URL avulsa, sem lista (bloqueia; chamar de fio proprio).
int  stream_url_serve(const char *url, const char *cabecalhos);

// 1 quando o texto da fonte diz "fora de cache" (ver Stream.foraCache).
// Publica para o parser e o teste; a lista ja vem com o campo preenchido.
int  stream_texto_fora_de_cache(const char *texto);

// A PONTUACAO DA FONTE AUTOMATICA (modo "Melhor fonte") e o teto de
// "Qualidade maxima", para quem precisa ordenar uma lista que NAO e a do
// titulo aberto: o teste de velocidade do diagnostico mede primeiro a fonte
// que o automatico escolheria, com a mesma regra, sem copia-la.
long stream_pontos(const Stream *s);
// A fonte e MP4: s->mp4 (o addon disse), ".mp4" na URL ou ".mp4" no rotulo.
// UMA resposta para o cartao (MP4/MKV) e para o anuncio ao video
// (video_definir_mp4), que decide se a tela/sonda do Dolby Vision em MKV entra.
int  stream_e_mp4(const Stream *s);
// R9b: o que a tela mostra (1/0; -1 = desconhecido, o padrao, nao penaliza).
void stream_definir_tela(int hdr, int dv);
// Android: -1 desconhecido, 0 sem decoder 3840x2160, 1 suportado. Nao filtra a folha manual.
void stream_definir_decoder4k(int hevc, int avc, int vp9, int av1);
// Erro Media3 no automatico: nao repetir tamanho/codec nesta lista. `renderer`
// diz de onde veio o erro: 0 video, 1 audio, 2 desconhecido. Erros de audio
// (EAC3/AC4 sem suporte) nao bloqueiam codecs de video.
void stream_automatico_erro_decoder(int indice, int codigo, int renderer);
int  stream_cabe_no_teto(const Stream *s);

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
//   3  -> STREAM_P2P_FALHOU: o debrid nao resolveu (ou nao ha) e o servidor
//         P2P experimental (p2p.h) tambem nao; o motivo esta em p2p_ultimo_erro()
#define STREAM_P2P_FALHOU 3
unsigned stream_lista_geracao(void);
// Quantos torrents SEM url (so infoHash) ha na lista: os que so o P2P ou o
// debrid tocam. Serve ao cartao "nenhuma fonte serve" dizer que ha P2P.
int  stream_qtd_torrents(void);
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

// A proxima fonte a tentar depois de `atual`, na fila que a sonda montou
// (viva, incerta, muda, morta; teto antes de acima dele). -1 = acabou. Sem
// fila valida para a lista atual, a ordem do addon (atual + 1).
int  stream_canal_proxima(int atual);
// 1 quando a fonte `idx` merece o prazo cheio de abertura: viva, incerta, ou
// sonda que nao achou nenhuma viva (nao informou nada). 0 = muda/morta numa
// lista em que outras responderam, prazo curto.
int  stream_canal_prazo_longo(int idx);

// --- folha de fontes (a lista que sobe por cima do player/detalhe) ---
void stream_folha_abrir(void);
// Root feeds an actual movie/episode metadata runtime or measured media
// duration for this exact target. Never supply PLR_DUR_PADRAO, a season's
// total runtime or a guessed "45 minutes". 0/unknown clears provenance.
// Thread-safe; a sheet already open keeps its frozen duration/speed data.
// SF_DUR_METADATA and SF_DUR_MEDIA are stored apart; 0 clears only the one
// named, SF_DUR_DESCONHECIDA clears both. Media beats metadata of the target.
void stream_fit_duracao(const char *alvo, double segundos, StreamfitDuracao origem);
// Catalog runtime lookup used when the sheet opens and no pushed value
// exists for the target. Called on the UI thread with the exact target id;
// must answer 0 unless it can prove the runtime belongs to that id.
void stream_fit_fonte_metadados(double (*fonte)(const char *alvo));
// Frozen classification of an existing source while the sheet is open.
// Returns unknown without evidence. For UI, demand is an estimate, not a
// guarantee; the caller can display age, budget and diagnostic origin.
StreamfitClasse stream_fit_folha_estado(int indice, StreamfitResultado *saida);
// Medida de rede REAL do host de `s`, agora (cartao de "Abrindo fonte" expandido).
// 0 = sem medida: l1/l2 vazias.
int stream_fit_abrindo(const Stream *s, char *l1, size_t n1, char *l2, size_t n2);
int  stream_folha_aberta(void);
// QUANTAS LINHAS A FOLHA MOSTRA AGORA — issue #132 ("so 1 fonte listada"). A
// folha lista a lista INTEIRA de stream_definir_lista; so os filtros que a
// pessoa liga NA PROPRIA FOLHA (provedor, "MP4") tiram linha. A verificacao do
// automatico, "Fonte automatica" (Melhor/Primeira da lista), as candidatas que
// falharam (stream_automatico_excluir) e a fonte fora de cache NAO escondem
// nada. tests/fontes_lista.sh prende isso; abrir a folha escreve a contagem no
// log ("[fonte] folha: N de M na lista"), para o proximo relato dizer onde caiu.
int  stream_folha_n(void);
void stream_folha_evento(const SDL_Event *e);
void stream_folha_atualizar(float dt, Uint32 agora);
void stream_folha_desenhar(Uint32 agora);
// Devolve 1 uma vez quando o usuario escolheu, com o indice em *escolhido.
int  stream_folha_escolheu(int *escolhido);

#endif
