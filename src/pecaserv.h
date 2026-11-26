// "Peca como fala": a porta na rede local por onde o celular pareado fala com
// a TV. Regras de seguranca em peca.h (peca_rota); aqui so sockets e o fio.
//
// PAREAMENTO PERMANENTE (#798): o token e a porta nao mudam entre aberturas
// (quem chama guarda o token), entao a pagina vira favorito no celular. A
// porta so existe com o ajuste ligado E depois de a pessoa parear uma vez.
// Depois de PECA_ERROS_MAX caminhos errados a porta se cala por
// PECA_CASTIGO_S (nao morre: um vizinho barulhento nao despareia a casa).
// O servidor so responde, nunca chama ninguem; pedido e chave nunca vao ao log.
//
// SEM SOCKET (.wgt/Emscripten, Windows): pecaserv_disponivel() = 0. La a ideia
// precisa de retransmissao por servidor nosso, que esta etapa nao tem.
#ifndef NV_PECASERV_H
#define NV_PECASERV_H
#include <stddef.h>

#define PECASERV_PORTA 47803

int  pecaserv_disponivel(void);
// 12 simbolos de /dev/urandom. 1 = ok.
int  pecaserv_gerar_token(char *dst, size_t n);
// Sobe (ou troca token/pagina). `html` e copiado. 1 = no ar.
int  pecaserv_iniciar(const char *token, const char *html);
void pecaserv_parar(void);
int  pecaserv_no_ar(void);
// http://<ip>:<porta>/<token>, ou "" fora do ar.
const char *pecaserv_url(void);
// Segundos desde o ultimo pedido valido do celular; -1 = nunca.
int  pecaserv_visto_ha(void);
// O JSON que GET /<token>/s devolve (peca_estado_json). Copiado.
void pecaserv_estado(const char *json);
// 1 UMA VEZ por coisa que chegou. Fio do app.
int  pecaserv_pegar_pedido(char *dst, size_t n);
int  pecaserv_pegar_chave(char *dst, size_t n);
int  pecaserv_pegar_abrir(int *indice);
#endif
