// CODIGO DE 6 DIGITOS DO LOGIN: a parte que fala com o servidor.
//
// A tela de login chama parear_rede_passo a cada quadro enquanto o QR esta na
// tela; aqui um fio pede o codigo UMA vez por endereco e a tela so le o
// resultado. Nada bloqueia o desenho, nada vai para o log alem do status.
// Sem NV_REC_URL (pacote sem servidor), com o ajuste desligado ou com o
// servidor sem a rota, parear_rede_codigo() fica "" e a tela e a de sempre.
#ifndef NV_PAREARREDE_H
#define NV_PAREARREDE_H

// `urlLogin` = o endereco do QR (sessao_url_login). `ligado` = o ajuste.
void parear_rede_passo(const char *urlLogin, int ligado);
// "483 921" ou "" enquanto nao ha codigo valido para o endereco atual.
const char *parear_rede_codigo(void);
// "exemplo.com/c": onde quem ajuda digita o codigo.
const char *parear_rede_pagina(void);
int parear_rede_falhou(void);
// Esquece codigo e tentativas (a tela de login abriu de novo).
void parear_rede_esquecer(void);
// "Novo codigo" (botao da tela de login): o codigo mostrado some na hora e a
// TV pede ja outro para o MESMO endereco; o servidor mata o antigo. 1 = aceito,
// 0 = sem endereco em uso ou cedo demais (PAREAR_RENOVAR_INTERVALO).
int parear_rede_renovar(void);

#endif
