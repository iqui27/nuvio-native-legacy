// AQUECER CONEXOES (2.0.2). Quando a pagina de um titulo abre, e quando a lista
// de fontes chega, abre antes DNS + TCP + TLS dos hosts que a escolha da fonte
// vai pedir (a API do debrid, o host de cada uma das primeiras fontes), para o
// primeiro pedido de verdade nao pagar o handshake (150 a 500 ms numa TV).
//
// O QUE NUNCA FAZ: nao resolve link de stream, nao pede caminho nenhum alem de
// "/" da origem e nao cria arquivo no painel do debrid. E um HEAD na raiz.
//
// A regra mora aqui, sem SDL: tests/aquecer.c troca o motor (aquecer_definir)
// e confere dedup, limite por origem, limite por lote e fio unico.
#ifndef NV_AQUECER_H
#define NV_AQUECER_H

// "https://host[:porta]" de uma URL http(s); 0 para o que nao e (e para host
// vazio). Cabe em `dst` de `n` bytes.
int aquecer_origem(const char *url, char *dst, unsigned n);

// Quantas origens por lote e de quanto em quanto tempo a mesma origem e
// aquecida de novo (menos que a validade do handle estacionado, rede.c).
#define AQUECER_LOTE_MAX 4
#define AQUECER_REPETIR_MS 12000UL

// O motor: abre as origens e devolve quantas abriram, com o tempo de cada uma
// em ms[] (0 = falhou). O padrao e rede_aquecer_lote.
typedef int (*AquecerMotor)(const char *const *origens, int n, unsigned *ms);
void aquecer_definir(AquecerMotor m);

// Decide, SEM rede: das URLs dadas, as origens que valem aquecer agora (http,
// distintas, ainda nao aquecidas ha AQUECER_REPETIR_MS, no maximo
// AQUECER_LOTE_MAX). Marca-as como aquecidas. `agora` em ms.
int aquecer_planejar(const char *const *urls, int n, unsigned long agora,
                     char origens[][96]);

// Pede o aquecimento num fio proprio e descartavel (no maximo um por vez; um
// pedido com outro em curso e ignorado, nao enfileira). Devolve quantas origens
// foram planejadas (0 = nada a fazer). Loga "[rede] aquecida: host (ms)".
int aquecer_pedir(const char *const *urls, int n);

#endif
