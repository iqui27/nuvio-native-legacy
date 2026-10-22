#ifndef NUVIO_FONTESONDA_H
#define NUVIO_FONTESONDA_H
// SONDA DEPOIS DA RECUSA DO PLAYER (#364, 2.0.4). Quando o player nativo recusa
// uma URL na abertura (Samsung: "[host] erro ConnectionFailed" / 0xfe6c0026,
// 3,2 s depois de "abrir: url ao pipeline"), nada no log diz se o LINK estava
// vivo. A sonda repete o pedido pelo nosso cliente e escreve UMA linha.
//
// NUNCA a URL inteira: so o host (links de debrid levam a chave no caminho e
// na query).

// 1 quando o texto de erro do player e de conexao/abertura de rede (e vale
// sondar). Sem erro ou erro de decodificacao/formato: 0.
int fontesonda_erro_conexao(const char *erroTexto);

// Monta "[fonte] sonda apos recusa: status=<n> redirecionamentos=<n>
// host_final=<host> tipo=<mime> tempo=<ms> <veredito>". `url` e a URL final
// (ou a de origem quando a sonda nao chegou a resposta); so o host entra.
// redir < 0 = desconhecido (o pedido nao diz quantos saltos deu). status 0 =
// sem resposta. Devolve dst.
const char *fontesonda_linha(char *dst, unsigned tam, const char *url, int status,
                             int redir, const char *mime, unsigned ms);
#endif
