// MOTOR P2P EMBUTIDO: o proprio aparelho baixa o torrent (#171, F10).
//
// E o nuvio-engine (github.com/NuvioMedia/nuvio-engine, GPL-3.0, commit
// 02938d7 = 0.1.3, mais tools/p2p-motor/janela-334.patch = API 4, janela de
// streaming): libtorrent 2.0.12 por baixo, agendamento de pecas pela
// posicao que o player pede e um servidor HTTP com Range em 127.0.0.1 (porta
// livre). O player (Media3 no Android, uMS na LG, Tizen.Multimedia no .tpk)
// toca a URL local como qualquer link direto.
//
// So entra no binario com -DNV_P2P_MOTOR e as bibliotecas do motor ligadas
// (tools/p2p-motor/). Sem isso p2pmotor_disponivel() e 0 e o P2P segue pelo
// servidor Stremio da rede (p2p.c), como antes. No .wgt nao existe: o
// navegador nao abre socket TCP/UDP.
//
// CONCORRENCIA (PLANO F10). Uma trava curta guarda o estado; NENHUMA espera
// (metadados, prova dos primeiros bytes, statvfs, create/destroy do motor,
// soma do disco) acontece com ela presa.
//   - p2pmotor_cancelar() so sobe uma geracao: o pedido em curso confere a
//     geracao a cada volta (<= 20 ms) e sai com P2P_ERR_CANCELADO.
//   - Eventos do motor sao casados por request_id/torrent_id: o ADDED/
//     METADATA/PREPARED de um pedido velho e descartado (contado no log).
//   - Teto duro de disco (min(P2PM_DURO_MAX_MB, metade do livre ao subir)) e
//     de RAM (P2PM_RAM_DURO_MB sobre o cache em memoria do motor), conferidos
//     pela VIGIA, um fio proprio a cada P2PM_VIGIA_MS. Passou: o motor para
//     e p2pmotor_motivo_parada() diz por que (o fio da tela so le um atomico).
//   - statvfs que falha = recusa (P2P_ERR_DISCO), ao subir e na vigia.
//   - #334, JANELA: arquivo maior que o teto duro nao para mais no meio. O
//     motor recebe janela = teto duro - P2PM_JANELA_FOLGA_MB e, para arquivo
//     maior que ela, apaga do disco as pecas ja tocadas (e as muito a frente
//     depois de um seek para tras); voltar para um trecho apagado baixa de
//     novo. Arquivo que cabe: igual a antes. Janela menor que
//     P2PM_JANELA_MIN_MB com arquivo maior que o teto: recusa ANTES de tocar,
//     com os numeros (P2P_ERR_SEM_ESPACO + p2pmotor_arquivo_atual).
//   - Parar: o destroy roda num fio solto, so depois que todo usuario do motor
//     (pedido, vigia) soltou a referencia; a pasta de cache so e apagada
//     DEPOIS do destroy (escritores do libtorrent parados).
//   - Saida do app: cancela e espera o destroy por ate P2PM_SAIDA_MS. Nao
//     terminou: a pasta fica, e o proximo subir (sem motor vivo) a apaga.
#ifndef NV_P2PMOTOR_H
#define NV_P2PMOTOR_H

#include <stddef.h>
#include <stdint.h>

#ifndef P2PM_DISCO_MB      // teto MOLE do motor (disk_cache_capacity); nunca > 1/4 do livre
#ifdef NV_ANDROID
#define P2PM_DISCO_MB 28
#else
#define P2PM_DISCO_MB 512
#endif
#endif
#ifndef P2PM_DURO_MAX_MB   // teto DURO absoluto da pasta; nunca > 1/2 do livre ao subir
#ifdef NV_ANDROID
#define P2PM_DURO_MAX_MB 192 // 160 MiB streaming window plus 32 MiB headroom
#else
#define P2PM_DURO_MAX_MB 1536
#endif
#endif
#define P2PM_LIVRE_MIN_MB 256   // menos livre que isto ao subir: recusa
#define P2PM_RESERVA_MB   512   // limite FIXO (Ajustes): deixa ao menos isto livre na TV (#334)
#define P2PM_LIVRE_PISO_MB 128  // vigia: o livre caiu abaixo disto (outro app): para
#ifndef P2PM_JANELA_MIN_MB      // #334: menor janela que toca um 4K (read-ahead + cabeca/cauda)
#define P2PM_JANELA_MIN_MB 160
#endif
#define P2PM_JANELA_FOLGA_MB 32 // #334: janela = teto duro menos isto (resume, pecas de borda)
#define P2PM_RAM_MB       16    // cache em memoria do motor
#define P2PM_RAM_DURO_MB  32    // vigia: memory_cache_used acima disto: para
#define P2PM_VIGIA_MS     2000
#define P2PM_SAIDA_MS     1500

// 1 quando este build tem o motor (nao diz se ele ja subiu).
int p2pmotor_disponivel(void);
// "0.1.3 / libtorrent 2.0.12.0", ou "" sem motor.
const char *p2pmotor_versao(void);
// Por que nao ha motor neste build ("" quando ha). So para o log.
const char *p2pmotor_motivo_indisponivel(void);

// "Testar" dos Ajustes, FORA do fio da tela (faz statvfs): versao e o teto de
// disco que o motor teria agora. P2P_OK, P2P_ERR_DISCO ou P2P_ERR_SEM_ESPACO.
int p2pmotor_resumo(char *detalhe, unsigned n);
// Teto duro (MB) medido no ultimo resumo/subida; 0 sem medida.
unsigned p2pmotor_teto_mb(void);

// LIMITE DE ESPACO (#334, "Limite de espaço do P2P" em Ajustes). escolhaMb 0 =
// Automatico (o de sempre: min(P2PM_DURO_MAX_MB, metade do livre)); > 0 = a
// pessoa pediu ate tanto, limitado ao livre menos P2PM_RESERVA_MB e nunca
// abaixo do Automatico. Vale na proxima subida do motor. Qualquer fio.
void p2pmotor_definir_limite_mb(unsigned mb);
// #205: 1 = tentar cache em pendrive USB (cai para interno se nao houver ou falhar).
void p2pmotor_definir_usb(int usar);
// 1 se ha pendrive gravavel agora — Ajustes cinza a opcao "Pendrive USB" com
// isto. Resposta cacheada por ~2 s; a sonda nao cria nem apaga nada no volume.
int p2pmotor_usb_presente(void);
// Most actionable rejection from the last scan; NONE after success. No I/O.
typedef enum {
  P2PM_USB_NONE, P2PM_USB_NO_BASE, P2PM_USB_NOT_MOUNT, P2PM_USB_ALREADY_SEEN,
  P2PM_USB_TIMEOUT, P2PM_USB_DATA_DIR, P2PM_USB_IO, P2PM_USB_READONLY,
  P2PM_USB_DISABLED
} P2pmUsbMotivo;
P2pmUsbMotivo p2pmotor_usb_motivo(void);
// Everything the USB cache modal draws. estado: 0 no drive, 1 found (emUso = the
// cache goes there), 2 found but rejected (motivo says why). Sizes in bytes;
// cache/limite/gravavel/emUso only for estado 1. Cached ~2 s, never writes.
typedef struct {
  int estado, emUso, gravavel, fat;
  P2pmUsbMotivo motivo;
  char nome[64], caminho[256], fs[32];
  uint64_t total, livre, cache, limite;
} P2pmUsbInfo;
void p2pmotor_usb_info(P2pmUsbInfo *o);
// A conta acima, pura (testes).
uint64_t p2pmotor_teto_duro(uint64_t livre, unsigned escolhaMb);
// #334: janela de streaming que o motor recebe para um teto duro (0 = sem
// janela), e se um arquivo de `tam` bytes toca com esse teto (1) ou e recusado
// antes de tocar (0: maior que o teto e a janela abaixo de P2PM_JANELA_MIN_MB).
uint64_t p2pmotor_janela(uint64_t duro);
int p2pmotor_cabe(uint64_t tam, uint64_t duro);
// Tamanho do arquivo do ultimo pedido, o teto duro e o livre de quando o motor
// subiu (0 = desconhecido). Para a tela explicar a parada por espaco.
void p2pmotor_arquivo_atual(uint64_t *tam, uint64_t *teto, uint64_t *livre);
// Inicio do app: apaga a pasta do motor que sobrou de uma sessao que caiu (TV
// desligada com o filme tocando), em fio solto, sem abrir o motor. Call after
// settings load: USB discovery runs on that worker only when P2P is enabled.
// Um pedido que chegue durante a limpeza espera por ela.
void p2pmotor_limpar_sobra(void);

// BLOQUEIA (fio proprio). Sobe o motor se preciso, registra o magnet (com os
// trackers do addon), espera os metadados (P2P_PRAZO_METADADOS), escolhe o
// arquivo (p2p_escolher_lista), prepara o stream, prova os primeiros bytes
// e escreve a URL local em `url`. Devolve um P2pErro. Um pedido por vez:
// outro em curso devolve P2P_ERR_OCUPADO.
int p2pmotor_resolver(const char *hash, int fileIdx, const char *fontes,
                      int temporada, int episodio, char *url, unsigned n);

// Qualquer fio, nunca bloqueia: o pedido em curso sai com P2P_ERR_CANCELADO.
void p2pmotor_cancelar(void);

// 1 quando `url` e do servidor local do motor (a lista de fontes nao guarda
// essa url: ela morre com o stream).
int p2pmotor_e_url(const char *url);

// 1 com o motor de pe (ou subindo/parando).
int p2pmotor_ativo(void);
// Cancela e para em fio solto (o destroy do libtorrent leva segundos). Fio da
// tela pode chamar a cada quadro: so pega a trava curta.
void p2pmotor_parar_fundo(void);
// Igual, mas espera o fim (testes, porta de teste). Nao use no fio da tela.
void p2pmotor_parar(void);
// Saida do app (ver acima).
void p2pmotor_saida(void);

// Motivo da ultima parada pela vigia (P2P_ERR_SEM_ESPACO/DISCO/RAM) e zera;
// 0 sem nada. Fio da tela, a cada quadro: um atomico.
int p2pmotor_motivo_parada(void);

// Orcamento e uso, para a tela dizer a verdade. Copia sob a trava curta; os
// numeros sao os da ultima volta da vigia (nao faz I/O).
typedef struct {
  int ativo;
  uint64_t disco_teto, disco_duro, disco_usado, livre;
  uint64_t ram_teto, ram_duro, ram_usada;
  uint64_t baixando_bps;
  unsigned pares;
} P2pmEstado;
void p2pmotor_estado(P2pmEstado *e);

// Porta de teste (main.c, "p2p:<hash>"): segura o motor de pe sem o player
// aberto, para o app.c nao para-lo. 0 solta.
void p2pmotor_segurar(int s);
int p2pmotor_segurado(void);

// ------------------------------------------------------------ fronteira
// O motor real (p2pmotor_motor.c, so com -DNV_P2P_MOTOR) e os testes (motor
// falso) entram por esta tabela. Nada de C++ atravessa: so tipos C.
enum {
  P2PM_EV_OUTRO = 0, P2PM_EV_ADDED, P2PM_EV_META, P2PM_EV_ERRO,
  P2PM_EV_PREPARADO, P2PM_EV_REMOVIDO, P2PM_EV_PARADO
};
typedef struct {
  int tipo;
  uint64_t rid;
  char tid[65], sid[65], url[512], msg[256];
} P2pmEvento;
typedef struct {
  const char *dados, *cache;
  uint64_t ram, disco, upload_bps;
  uint64_t janela;           // #334: streaming_window_bytes (0 = arquivo inteiro)
} P2pmConfig;
typedef struct {
  uint64_t ram_usada, disco_motor, baixando_bps;
  uint64_t baixado;          // bytes do torrent recebidos desde que o motor subiu
  unsigned pares;
  uint64_t janela_apagado;   // #334: bytes ja apagados pela janela
  uint64_t janela_rebaixadas;// pecas apagadas e baixadas de novo (seek para tras)
  int janela_sem_suporte;    // o sistema de arquivos nao abre buraco: so o teto vale
} P2pmStats;
typedef struct {
  const char *(*versao)(void);
  int  (*criar)(const P2pmConfig *c, void **motor);          // 0 ok
  void (*destruir)(void *motor);                              // pode levar segundos
  int  (*add_magnet)(void *motor, const char *magnet, uint64_t *rid);
  int  (*poll)(void *motor, P2pmEvento *ev);                  // 1 evento, 0 nenhum
  int  (*qtd_arquivos)(void *motor, const char *tid, size_t *n);
  int  (*arquivo)(void *motor, const char *tid, size_t i, char *path, unsigned np,
                  uint64_t *tam);
  int  (*preparar)(void *motor, const char *tid, uint32_t idx, uint64_t *rid);
  void (*parar_stream)(void *motor, const char *sid);
  void (*remover)(void *motor, const char *tid);
  int  (*stats)(void *motor, P2pmStats *s);
} P2pmOps;
// NULL sem -DNV_P2P_MOTOR (ou, no .tpk, sem a libnuvio_engine.so no pacote).
const P2pmOps *p2pmotor_ops_reais(void);
// Por que o motor dinamico (.tpk) nao carregou; "" se carregou ou e estatico.
const char *p2pmotor_motor_erro(void);

// Testes: troca motor, statvfs, prova de bytes e pasta raiz. NULL mantem o
// atual. vigia_ms 0 mantem. Chamar sem motor de pe.
typedef int (*P2pmLivreFn)(const char *pasta, uint64_t *livre);      // 0 ok
typedef int (*P2pmSondaFn)(const char *url, int (*parar)(void *), void *u); // 1 ok, 0 sem bytes, -1 HTTP erro
void p2pmotor_teste_injetar(const P2pmOps *ops, P2pmLivreFn livre, P2pmSondaFn sonda,
                            const char *raiz, unsigned vigia_ms);
void p2pmotor_teste_usb_base(const char *base);
// Tizen: troca /opt/media por `dir` e devolve as raizes candidatas (uma por linha).
unsigned p2pmotor_teste_usb_tizen(const char *dir, char *out, unsigned nout);
// #205: lista (uma por linha, linha 0 = volume primario, ignorada) no lugar de
// getExternalFilesDirs, e arquivo no formato /proc/mounts para o tipo do fs.
void p2pmotor_teste_usb_android(const char *lista, const char *mounts);
// Quantos eventos velhos (de outro pedido) foram descartados desde o inicio.
unsigned p2pmotor_teste_descartados(void);
#endif
