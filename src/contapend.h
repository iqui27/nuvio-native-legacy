// O QUE A PESSOA MARCOU NA TV E AINDA NAO CHEGOU NA CONTA (jornal persistido).
//
// O pedido do dono: "tem um problema com o watched que salvamos, temos que
// sincronizar com o perfil para que nada se perca, watched, marcados etc".
//
// ONDE SE PERDIA, medido no codigo antes deste modulo:
//   - "Marcar como assistido" (visto.c) mandava UMA RPC sincrona num fio solto.
//     Sem rede, 401, 5xx ou app fechado no meio: a marca so existia em memoria
//     (vistoep.c e o historico de titulo do catalogo NAO vao para disco) e nunca
//     mais era tentada. Fechou o app, perdeu.
//   - Desmarcar sem rede tambem se perdia, e o pull seguinte da conta re-marcava
//     o item (a linha antiga voltava sozinha).
//   - O "+" (Salvos) nunca subia para a conta: so salvos.txt local e Trakt/Simkl.
//     Reinstalar, trocar de TV ou abrir outro app Nuvio = lista perdida. E tirar
//     um titulo que veio da conta voltava no ciclo seguinte.
//
// O QUE ESTE MODULO GARANTE:
//   - Cada gesto vira UMA entrada (por perfil, por item), gravada em disco antes
//     de qualquer rede. A ultima intencao por item vence (marcar e desmarcar o
//     mesmo episodio offline vira so "desmarcar").
//   - O envio e por ITEM, nunca "a lista local inteira": vistos por
//     sync_push_watched_items / sync_delete_watched_items com exatamente o que
//     mudou. Entrada so sai do jornal depois de 2xx; falha fica para o proximo
//     ciclo (o sync roda a cada poucos minutos) ou para o proximo gesto.
//   - A biblioteca so tem sync_push_library (sem RPC de delete, ver
//     contapend.c). O push la e feito como LER-MESCLAR-ESCREVER: puxa a lista
//     INTEIRA da conta, aplica so as entradas do jornal, e sobe o resultado.
//     Qualquer pagina falhando, lista remota vazia com copia guardada nao
//     vazia, ou resultado vazio = NADA sobe. Nunca sai uma lista menor que a
//     remota alem dos itens que a pessoa tirou.
//   - O pull respeita o jornal: linha remota de um item que a pessoa desmarcou/
//     tirou (e que nao e mais nova que o gesto) nao re-marca nada; linha remota
//     MAIS NOVA que o gesto ganha (ultima mudanca vence) e a entrada cai.
//
// O ARQUIVO e por USUARIO (conta-pend-<sub>.txt) e sobrevive ao logout: so tem
// ids, perfil e horario — nenhum token. Outro usuario nunca le o do anterior.
#ifndef NV_CONTAPEND_H
#define NV_CONTAPEND_H

#include "vistoep.h"

#define CONTAPEND_MAX 4000

// ---- registro (FIO PRINCIPAL ou quem chama visto_*_ja) --------------------
// Episodios (temporada >= 0, episodio >= 1) ou titulo inteiro (n == 0 com
// pares NULL: use contapend_titulo). `visto` 1 marca, 0 desmarca. Perfil =
// perfis_ativo() no momento do gesto. Devolve quantas entradas mudaram.
int  contapend_episodios(const char *imdb, const char *tipo,
                         const VistoPar *pares, int n, int visto);
int  contapend_titulo(const char *imdb, const char *tipo, int visto);
// "+" / tirar da lista. `nome`/`poster` so servem ao item novo na conta.
int  contapend_lista(const char *imdb, const char *tipo, const char *nome,
                     const char *poster, int salvo);

// Persist local watched intent even without a Nuvio account. Destinations are
// retried independently; seasons are retained for safe Simkl series removal.
long long contapend_gesto(const char *id, const char *tipo, const VistoPar *pares, int n,
                          int visto, int destinos, const int *temporadas, int nt);
long long contapend_gesto_alvos(const char *id, const char *tipo,
                          const VistoPar *pares, const int *tvdbEps, int n,
                          int visto, int destinos, const int *temporadas, int nt);
// Check queued intent after acquiring the profile write gate.
int contapend_gesto_atual(const char *id, int t, int e, long long ms);
void contapend_confirmar_gesto(const char *id, const VistoPar *pares, int n,
                              long long ms, int destinos);
// Resolve against the latest local gesture (including whole-series intent).
// Unknown/older remote timestamps cannot undo it; a strictly newer one wins.
int contapend_estado(const char *id, const char *tipo, int t, int e, long long remotoMs, int remoto);

// ---- envio ------------------------------------------------------------------
// SINCRONO. Manda tudo que esta pendente (todos os perfis do usuario logado).
// Devolve quantas entradas foram confirmadas, ou -1 se alguma falhou (o que
// falhou continua pendente).
int  contapend_enviar(void);
// Replay newer episode overrides rearmed by a successful whole-series write.
int contapend_reenviar_overrides(const char *id, long long ms);
int  contapend_reenviar(void); // Retry pending Trakt/Simkl for the active profile.
void contapend_reenviar_com(int (*f)(const char *, const char *, const VistoPar *,
                                     int, int, int, const int *, int,
                                     const int *));
// Dispara contapend_enviar num fio, se nenhum estiver no ar (se estiver, ele
// repete uma volta ao terminar).
void contapend_chutar(void);

// ---- pull (FIO PRINCIPAL) ----------------------------------------------------
// 1 quando a linha remota (perfil ativo) deve ser IGNORADA porque a pessoa
// desmarcou/tirou o item nesta TV depois de `remotoMs`. Linha remota mais nova
// que o gesto derruba a entrada e devolve 0.
int  contapend_visto_oculto(const char *imdb, int temporada, int episodio,
                            long long remotoMs);
int  contapend_lista_oculta(const char *imdb, long long remotoMs);
// 1 quando ha um '+' de lista PENDENTE (nao confirmado) para o titulo neste
// perfil: o snapshot da conta nao pode apagar o espelho local dele.
int  contapend_lista_pendente(const char *imdb);
// Reaplica no estado local (vistoep + historico de titulo) o que esta no
// jornal para o perfil ativo. Barato; chamado a cada ciclo aplicado.
int  contapend_aplicar_local(void);
// Prune confirmed library entries before `desdeMs`. Watched intent stays
// until a newer remote timestamp supersedes it or the bounded journal fills.
void contapend_podar(long long desdeMs);

// ---- estado -------------------------------------------------------------------
int  contapend_pendentes(void);      // nao confirmadas, todos os perfis
void contapend_esquecer(void);       // logout: larga a memoria, arquivo fica
// Teste: relogio em ms e o "fio" (0 = chutar roda sincrono).
void contapend_relogio(long long (*f)(void));
void contapend_sem_fio(int sim);
long long contapend_agora_ms(void);

#endif
