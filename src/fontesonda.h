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

// POR QUE A FONTE NAO ABRIU, so pelo texto de erro do player (nunca a URL).
// Diz de quem e o problema: FS_CAUSA_HTTP = o servidor da fonte recusou o link
// (addon/debrid; *http recebe o status, 0 = nao se sabe qual), FS_CAUSA_REDE =
// conexao/tempo esgotado ate a fonte, FS_CAUSA_FORMATO = formato/decoder (pode
// ser o app ou a TV), FS_CAUSA_NENHUMA = nao da para dizer.
// Codigos lidos: uMS "<40000+status> server error" (40404 = HTTP 404) e Media3
// 2001/2002 rede, 2004-2006 HTTP, 3xxx/4xxx formato/decoder.
enum { FS_CAUSA_NENHUMA, FS_CAUSA_HTTP, FS_CAUSA_REDE, FS_CAUSA_FORMATO };
int fontesonda_causa(const char *erroTexto, int *http);
// One safe failure line: classified reason, HTTP status and host only.
void fontesonda_recusa(const char *url, const char *erroTexto, int sincrona);

// Monta "[fonte] sonda apos recusa: status=<n> redirecionamentos=<n>
// host_final=<host> tipo=<mime> tempo=<ms> <veredito>". `url` e a URL final
// (ou a de origem quando a sonda nao chegou a resposta); so o host entra.
// redir < 0 = desconhecido (o pedido nao diz quantos saltos deu). status 0 =
// sem resposta. Devolve dst.
const char *fontesonda_linha(char *dst, unsigned tam, const char *url, int status,
                             int redir, const char *mime, unsigned ms);
#endif
