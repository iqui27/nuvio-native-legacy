// SINCRONIZACAO AUTOMATICA DA LEGENDA NA SESSAO DO PLAYER (F05, 1.8).
//
// Cola entre o player, a legenda externa (legenda.c), a engine temporal
// (autosync.c) e a referencia independente (legref.c). Um idioma: o slot
// PRINCIPAL. O slot 1 (segundo idioma) so expoe estado "disponivel depois".
//
// O QUE FAZ, nesta ordem:
//   1. legsync_iniciar no player_abrir: contexto criado uma vez (nao segura o
//      primeiro quadro), geracao de sessao nova por midia. legsync_passo
//      detecta troca de URL no meio da sessao e abre outra geracao.
//   2. A legenda EXTERNA escolhida vira LegendaDocumento no proprio fio que a
//      baixou (legenda_carregar_com), com idioma/origem/identidade opaca.
//      Embutida, nativa ou nenhuma: AutoSync indisponivel (a embutida ja foi
//      multiplexada com o video).
//   3. (R4: agora ELE COMECA SOZINHO, ver legsync_definir_trocador.) O legref le a faixa de texto
//      embutida do MKV por Range, em segundo plano, com orcamento. Sem
//      referencia COMPLETA: indisponivel, offset automatico zero.
//   4. legsync_offset_ms(manual) devolve manual + automatico ACEITO, para ser
//      aplicado UMA vez no overlay principal (positivo adianta, o mesmo sinal
//      de legenda_cues/assrender_desenhar). Sem documento dono do overlay,
//      devolve o manual intacto. Escala (fps) e trechos (corte) vem por
//      legsync_posicao, que troca o instante do video pelo da legenda.
//   5. Seek/buffer: cancela a analise e pausa a leitura; retoma sozinha 2 s
//      depois de calmo. legsync_encerrar no fim da sessao (sem join);
//      legsync_destruir no encerramento do app (join).
//
// NADA AQUI BLOQUEIA o fio de desenho por rede ou analise.
#ifndef NV_LEGSYNC_H
#define NV_LEGSYNC_H
#include "legref.h"
#include <stdint.h>

#define LEGSYNC_ACAO_RAPIDA   1
#define LEGSYNC_ACAO_COMPLETA 2
#define LEGSYNC_ACAO_DESFAZER 4
#define LEGSYNC_ACAO_OUTRA    8
#define LEGSYNC_ACAO_PARAR    16
#define LEGSYNC_ACAO_AUDIO    32   // F06: reference = speech heard in the playing audio

typedef enum {
  LEGSYNC_INDISPONIVEL = 0, LEGSYNC_AGUARDANDO, LEGSYNC_PRONTA, LEGSYNC_LENDO,
  LEGSYNC_ANALISANDO, LEGSYNC_ACEITA, LEGSYNC_RECUSADA, LEGSYNC_PAUSADA,
  LEGSYNC_DESFEITA, LEGSYNC_DEPOIS,
  LEGSYNC_OUVINDO              // F06: listening to the playing audio (progresso)
} LegSyncFase;

// Por que (para INDISPONIVEL/RECUSADA). Mapeado a texto traduzido por
// legsync_texto (legsyncui.c).
typedef enum {
  LEGSYNC_M_NENHUM = 0, LEGSYNC_M_SEM_EXTERNA, LEGSYNC_M_EMBUTIDA, LEGSYNC_M_PLATAFORMA,
  LEGSYNC_M_EXTERNA_INCOMPLETA, LEGSYNC_M_SEM_REFERENCIA, LEGSYNC_M_SEM_RANGE,
  LEGSYNC_M_REDE, LEGSYNC_M_ORCAMENTO, LEGSYNC_M_SEM_OUTRA, LEGSYNC_M_CONFIANCA,
  // F06 (Por audio): why the audio reference is not offered / was refused.
  LEGSYNC_M_AUD_PLATAFORMA, LEGSYNC_M_AUD_PASSTHROUGH, LEGSYNC_M_AUD_SEM_AUDIO,
  LEGSYNC_M_AUD_SEM_FALA
} LegSyncMotivo;

typedef struct {
  LegSyncFase fase;
  LegSyncMotivo motivo;
  int acoes;             // LEGSYNC_ACAO_* possiveis agora
  int offsetAutoMs;      // so o automatico aceito (0 sem aceite)
  int offsetTotalMs;     // manual + automatico, o que o overlay usa
  int progresso;         // 0..100 lendo a referencia
  char idiomaRef[24];    // idioma da faixa embutida usada/lida
  int audio;             // F06: o estado/resultado atual veio da referencia de AUDIO
  LegSyncMotivo motivoAudio;  // F06: Sincronia por audio ligada mas "Por audio" nao
                              // oferecido agora (plataforma/passthrough/sem audio)
  // R4 (automatico): 0 sem plano, 1 trabalhando, 2 sincronizou, 3 nao deu.
  int autoFase;
  int autoDesligado;          // ajuste local: sem plano nem pílula automáticos
  int falhaDownload;          // callback terminou sem corpo; não é um timeout da UI
  int autoTrocou;             // 1 = a escolhida nao sincronizou e outra legenda entrou no lugar
  char autoNome[64];          // nome da legenda que entrou (autoTrocou)
} LegSyncVisao;

void legsync_iniciar(const char *urlMidia);
void legsync_encerrar(void);
void legsync_destruir(void);
void legsync_passo(const char *urlMidia, double posSeg, double folgaSeg, int sensivel,
                   unsigned agoraMs);
int  legsync_offset_ms(int manualMs);
// Posicao na LEGENDA para o instante `posSeg` do video quando o aceito e
// escala/trechos (offset puro: posSeg). O overlay desenha em
// legsync_posicao(pos) + legsync_offset_ms(manual). AUTOSYNC_SEM_LEGENDA
// (muito negativo) = nada desta legenda neste trecho do video.
double legsync_posicao(double posSeg);

// Escolha da legenda PRINCIPAL (faixas.c). externa: no lugar de
// legenda_carregar. outra: embutida (1) ou nenhuma (0).
void legsync_primaria_externa(const char *url, const char *idioma, const char *origem);
void legsync_primaria_outra(int embutida);

// Ajuste local, ligado por padrão. Desligar cancela o plano, preserva o manual.
void legsync_auto_habilitar(int ligado);
// F06: Ajustes > Sincronia por audio (local, padrao desligado). Barato; o
// player chama a cada quadro. Desligar no meio cancela a escuta.
void legsync_audio_habilitar(int ligado);
// F06: a pessoa trocou a faixa de AUDIO: a escuta em curso nao vale mais.
void legsync_audio_trocou(void);

// R4: SINCRONIA AUTOMATICA. Toda legenda externa que entra como principal
// (escolha da pessoa ou a automatica de idioma) e sincronizada sozinha contra a
// melhor referencia (faixa embutida; audio quando o ajuste esta ligado). Se a
// escolhida nao fecha com confianca, legsync pede ao `trocador` (faixas.c) OUTRA
// legenda do mesmo idioma; ela e comparada igual, ate 3 trocas. Esgotadas,
// a escolha original volta (nada alterado) e o estado diz que nao deu.
// O trocador roda na thread da UI, FORA de qualquer lock, e liga a legenda pelo
// mesmo caminho da escolha da pessoa (legsync_primaria_externa). `voltar` = 1
// pede a primeira da lista `tentadas`. Devolve 1 se ligou alguma, e o nome dela.
typedef int (*LegSyncTrocador)(const char *idioma, const uint64_t *tentadas, int n, int voltar,
                               char *nome, unsigned tamNome);
void legsync_definir_trocador(LegSyncTrocador t);
uint64_t legsync_hash_url(const char *url);   // identidade opaca usada em `tentadas`

int  legsync_acao(int acao);            // 1 = aceita no estado atual
LegSyncVisao legsync_visao(int slot);   // slot 1: LEGSYNC_DEPOIS

// --- API DE APRESENTACAO (legsyncui.c) -------------------------------------
void legsync_texto(const LegSyncVisao *v, char *dst, unsigned tam);  // i18n
const char *legsync_acao_rotulo(int acao);
// R4: a linha unica e simples do seletor (sem menu). Vazio = esconder a linha.
void legsync_texto_simples(const LegSyncVisao *v, char *dst, unsigned tam);                            // i18n
// Final line of the automatic-subtitle pill once the plan stopped working.
// "Legenda sincronizada · <provedor> · +x,xx s" ONLY when an accepted result
// is the offset in force (fase ACEITA); anything else (refused, no reference,
// 45 s cap, no plan, undone) is "Legenda aplicada · <provedor> · não
// sincronizada". Returns 1 when synced. i18n.
int legsync_pilula_final(const LegSyncVisao *v, const char *provedor, char *dst, unsigned tam);
// A narracao da ilha sem GL (faixas.c a usa; testavel sozinha). Estados:
// "Procurando" -> "Sincronizando…" (LEGSYNC_PIL_SINC_MS, depois a ilha fecha) ->
// fechada enquanto o plano trabalha (espera) -> UM aviso final (sincronizada ou
// "nao sincronizada") pelo tempo normal -> some. Cancelar nao diz nada.
enum { LEGSYNC_PIL_OFF = 0, LEGSYNC_PIL_PROCURANDO, LEGSYNC_PIL_SINCRONIZANDO, LEGSYNC_PIL_APLICADA, LEGSYNC_PIL_FALHOU };
#define LEGSYNC_PIL_MIN_MS      900u
#define LEGSYNC_PIL_SINC_MS     3000u   // "Sincronizando…" na ilha antes de ela voltar ao relogio
#define LEGSYNC_PIL_APLICADA_MS 3600u
#define LEGSYNC_PIL_SEMSYNC_MS  6000u   // "nao sincronizada" fica mais: e um aviso, nao um sucesso
enum { LEGSYNC_PIL_ESCONDEU = 1, LEGSYNC_PIL_FINAL_TARDE = 2, LEGSYNC_PIL_LEMBRAR = 4,
       LEGSYNC_PIL_BAIXAR = 8, LEGSYNC_PIL_ZERAR = 16 };
typedef struct {
  int estado, rastreia, espera, final, troca;   // final: 0 nao sincronizada, 1 sincronizada, 2 embutida
  unsigned desde, iniciou;
} LegSyncPil;
// Um passo (so com rastreia ou espera). Devolve bits LEGSYNC_PIL_*.
int legsync_pil_passo(LegSyncPil *p, const LegSyncVisao *v, unsigned agora, const char *provedor, char *texto, unsigned tam);
// Liga o AutoSync como provedor da linha de sincronizacao do seletor de
// legendas do F04 (legendasui_definir_sync). Idempotente; thread da UI.
void legsync_ui_ligar(void);

// So para testes: leitor do legref antes do primeiro legsync_iniciar.
void legsync_teste_leitor(LegRefLer ler, void *u);
void legsync_teste_auto(int ligado);   // so testes: 0 = so as acoes manuais

#endif
