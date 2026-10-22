#ifndef NV_QUEDA_H
#define NV_QUEDA_H
#include <stdio.h>
// ONDE O APP MORREU. "A sessao anterior nao se despediu" (avisos.c) diz que
// houve queda, nao onde: na 2.0.0 duas TVs webOS 5+ somaram 12 quedas e o log
// nao tinha uma linha sobre o motivo. Aqui o sinal fatal grava, num arquivo
// proprio, o sinal, o endereco da falha, pc/lr, um trecho da pilha e o mapa de
// modulos; a abertura seguinte traduz tudo para "modulo+deslocamento" e poe no
// log, que e o que o envio automatico leva. O deslocamento e o do modulo
// carregado (o rel_pc do tombstone): vale direto no llvm-addr2line.
//
// queda_armar so serve onde o processo e nosso (webOS): SA_RESETHAND e voltar,
// sem chamar ninguem. No Android o processo e da ART, que usa SIGSEGV por
// conta propria; para ele existe queda_armar_encadeado (abaixo).

// Instala os tratadores. `arquivo` e o caminho completo do relato; a string e
// copiada. Chamar uma vez, cedo, no fio principal.
void queda_armar(const char *arquivo);

// ANDROID < 12 (#318): sem tombstone no ApplicationExitInfo, o "crash-nativo
// status=11" chega sem dizer onde. Mesmo relato, mas o tratador ENCADEIA:
//  * a ART instala o dela pela libsigchain, que intercepta o sigaction() de
//    toda biblioteca do processo. Quem chama sigaction(SIGSEGV) vira o
//    tratador "do usuario" da cadeia; a ART roda ANTES (checagem implicita de
//    nulo, estouro de pilha, suspensao no codigo gerenciado) e so passa adiante
//    o que nao e dela. Nao ha ordem a montar aqui: o sigaction normal ja entra
//    depois da ART.
//  * o tratador anterior que o sigaction devolve e o do debuggerd (instalado no
//    arranque do processo). Depois de gravar, este chama aquele com o mesmo
//    siginfo/contexto: tombstone, ApplicationExitInfo e a morte do processo
//    seguem como seriam sem nos. Nada e engolido.
//  * pc dentro de codigo gerenciado (.oat/.odex/jit) nao grava: so chegaria
//    aqui sem a libsigchain, e entao o tratador anterior e o da ART.
//  * so open/read/write/poll no tratador. Sem dladdr nem _Unwind_Backtrace: os
//    dois tomam o mutex do linker (dl_iterate_phdr), e uma queda no meio de um
//    dlopen (codec ou FFmpeg carregando) viraria travamento em vez de queda. O
//    modulo sai do /proc/self/maps, filtrado ali mesmo para as linhas que
//    contem algum endereco do relato (o maps de um app tem milhares de linhas).
// `antes` (pode ser NULL) roda no tratador com o relato ja gravado e tambem
// precisa ser async-signal-safe (android.c drena o fim do log para o arquivo).
void queda_armar_encadeado(const char *arquivo, void (*antes)(void));

// Le o relato da sessao anterior, imprime as linhas "[queda] ..." e apaga o
// arquivo. Devolve 1 se havia relato.
int  queda_relatar(const char *arquivo);
// Segundo destino do MESMO relato: o tratador grava nos dois (so open/write).
// O rastro do arranque (arranque.c) rearma o tratador depois, para outro
// arquivo; a queda no meio do app_iniciar tem de continuar achavel no de /tmp.
void queda_espelho(const char *arquivo);
// O mesmo que queda_relatar, escrevendo em `saida` (ex.: open_memstream).
int  queda_relatar_em(const char *arquivo, FILE *saida);
#endif
