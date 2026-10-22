#ifndef CREDFIO_H
#define CREDFIO_H
// Push de credencial (trakt/simkl) para a conta FORA do laco principal (#203).
// sync_empurrar_credencial e rede sincrona (ate o timeout, a cada 60 s quando
// o servidor falha); no laco de quadros travava a UI. Um fio solto por
// provedor, com copia propria do provedor e do JSON; no maximo um no ar por
// provedor; sem trava segurada durante a rede.
//
// credfio_iniciar: 1 se saiu, 0 se ja ha um no ar para o provedor (ou sem fio).
// credfio_resultado: 1 uma vez quando terminou, com o retorno de
//   sync_empurrar_credencial em *res (1 ok, 0 tentar depois, -1 recusa 4xx).
int credfio_iniciar(const char *provider, const char *credJson);
int credfio_resultado(const char *provider, int *res);
#endif
