// Discord: "Assistindo <titulo>" no perfil da pessoa (#222).
//
// COMO, e por que nao pelo caminho de sempre. O Rich Presence classico fala com
// o app do Discord rodando NA MESMA MAQUINA (RPC local), e a TV nao tem. O
// escopo HTTP que escreveria a atividade (`activities.write`) esta fechado para
// apps. O que existe para aparelho sem Discord e o do Social SDK: a pessoa
// vincula a conta com os escopos `openid sdk.social_layer_presence`, e o app
// abre o GATEWAY do Discord com esse token OAuth e manda a presenca (op 3).
//
// O binario do Social SDK nao serve aqui: so ha build para Windows/Mac/Linux
// x64, Android, iOS e consoles — nada de ARM webOS, nada de navegador. Entao o
// protocolo e falado direto: login por codigo (OAuth2 device flow, a mesma
// tela do Trakt), websocket proprio (discordws.c) e tres mensagens de gateway.
// O mesmo que a biblioteca MIT Discord-Social-RPC faz em Rust.
//
// PRIVACIDADE: nada sai antes da pessoa vincular, e o vinculo e por perfil
// (perfil = pessoa). Sem nada tocando por 60 s o gateway fecha, e o Discord
// apaga a atividade sozinho quando a sessao cai.
#ifndef NV_DISCORD_H
#define NV_DISCORD_H

typedef enum {
  DIS_PARADO = 0,  // sem vinculo
  DIS_PEDINDO,     // buscando o codigo
  DIS_AGUARDANDO,  // codigo na tela, esperando a pessoa autorizar
  DIS_LIGADO,      // token guardado
  DIS_ERRO,        // o pedido de codigo ou a autorizacao falhou
  DIS_INVALIDO     // token recusado e o refresh tambem
} DisEstado;

// 1 se o app foi compilado com o client id do Discord. Sem ele a linha nem
// aparece nos Ajustes.
int discord_disponivel(void);

DisEstado discord_estado(void);
void discord_recarregar(void); // reload account-restored connection
void discord_comecar(void);       // pede codigo novo e mostra
void discord_cancelar(void);      // fecha o cartao de vinculo
void discord_esquecer(void);      // desvincula o perfil ativo
const char *discord_codigo(void);
const char *discord_url(void);
const char *discord_erro(void);

// Uma vez por quadro, do fio principal. Cuida de tudo: troca de perfil, poll
// do vinculo, renovacao do token, gateway e o que esta tocando no player.
void discord_passo(unsigned agoraMs);

// Ao sair do app: fecha o gateway (a atividade some na hora, sem esperar o
// Discord perceber a queda).
void discord_encerrar(void);

#endif
