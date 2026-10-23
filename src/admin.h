// PIN de ADMINISTRADOR desta TV (2.1, ajustes por perfil).
//
// O administrador e o perfil principal da conta (indice 1). Se o principal ja
// tem PIN na conta, e ele que vale (perfis_verificar_pin). Se NAO tem, a TV
// cria um PIN proprio, local: este arquivo. Ele fica so nesta TV, guardado como
// sal + hash (SHA-256 iterado), nunca em claro, e some ao sair da conta.
//
// E uma trava de casa (impedir que uma crianca mexa nos ajustes dos outros),
// nao um cofre: 4 a 8 digitos nao resistem a quem copia o arquivo. Por isso o
// PIN nunca vai para a conta, nem para o log, nem para a memoria do m3.
#ifndef NV_ADMIN_H
#define NV_ADMIN_H

#define ADMIN_PIN_MIN 4
#define ADMIN_PIN_MAX 8

// 1 se ja existe PIN de administrador local.
int  admin_pin_definido(void);
// Cria/troca o PIN (so digitos, ADMIN_PIN_MIN..MAX). 1 se gravou.
int  admin_pin_definir(const char *pin);
// 1 se `pin` confere com o gravado. Sem PIN gravado devolve 0.
int  admin_pin_conferir(const char *pin);
// Apaga o PIN (sair da conta).
void admin_pin_esquecer(void);

// SHA-256 hex (64 chars + NUL) de um buffer; exposto para o teste.
void admin_sha256_hex(const unsigned char *buf, unsigned long n, char *hex65);

#endif
