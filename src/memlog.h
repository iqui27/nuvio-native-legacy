#ifndef NV_MEMLOG_H
#define NV_MEMLOG_H
#include <stddef.h>

// So nos pontos de log, nunca por quadro. Sem /proc valido, sufixo vazio.
void memlog_amostra(char *sufixo, size_t tamanho);
void memlog_evento(const char *evento);
int memlog_pouca_memoria(void);
int memlog_android_fraco(void);   // Android com RAM total <= 3 GiB (box Onn); 0 fora
long memlog_livre_mb(void);   // MemAvailable em MiB; -1 = desconhecido
// Abaixo disto o webOS fecha o app ("liberar memoria"); e o limiar do aviso.
#define MEMLOG_PISO_MB 150
void memlog_devolver(void);    // malloc_trim: heap livre de volta ao sistema
// #415: reabrir o pipeline depois de um erro com a TV abaixo do piso e o OOM
// que fecha o app. Abaixo do piso chama `aliviar` e le de novo. 1 = pode abrir
// (memoria desconhecida conta como pode).
int memlog_pode_reabrir(long (*livre)(void), void (*aliviar)(void));

// #415: o aviso de MemAvailable baixo roda `cb` em vez de so malloc_trim —
// app.c registra aliviarMemoria (texturas + mkvass + texto + libass + heap).
void memlog_quando_aviso(void (*cb)(void));
#endif
