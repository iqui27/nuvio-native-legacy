#ifndef NV_WEBOSVER_H
#define NV_WEBOSVER_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
// UMA fonte da versao maior do webOS desta TV. Ordem: "webos_release" de
// /var/run/nyx/os_info.json (a mesma da linha [tv]; existe nas TVs de 2017,
// webOS 3.9, que NAO tem /etc/starfish-release), depois a linha "release N" de
// /etc/starfish-release. 0 = desconhecida: cada porta decide o que isso
// significa, e nada aqui chuta. Valor guardado no primeiro uso (nao muda com o
// app aberto), exceto se a leitura deu erro de E/S; seguro entre fios.
// So le arquivos no build webOS (NV_WEBOS); fora dele devolve 0 sem abrir nada,
// a nao ser que nv_webos_testar() tenha dado caminhos.
int nv_webos_major(void);
// Fonte que valeu: "nyx", "starfish" ou "-".
const char *nv_webos_fonte(void);
// Copia a linha do starfish-release ("" se o arquivo falta) para o log.
void nv_webos_starfish_linha(char *out, size_t cap);
// Parsers puros. 0 para qualquer coisa que nao seja uma versao fechada.
// nyx: objeto JSON; so a chave "webos_release" do nivel 1, string "N.N[.x][-x]",
// documento fechado sem sobra; duplicata conflitante = 0.
int nv_webos_parse_nyx(const char *json);
int nv_webos_parse_starfish(const char *texto, char *linha, size_t cap);
#define NV_WEBOS_LEITURA 4096
// So para teste (nao ha idioma de "codigo so de teste" neste repo): troca os
// caminhos (copiados; NULL = os de verdade) e esquece o valor.
#ifdef AJUSTES_TESTE
void nv_webos_testar(const char *nyx, const char *starfish);
#endif
#ifdef __cplusplus
}
#endif
#endif
