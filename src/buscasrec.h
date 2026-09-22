// BUSCAS RECENTES da tela de Busca (dono, 22/09/2026: "temos que salvar na
// pagina de buscas as ultimas buscas para facilitar").
//
// Por que existe: no teclado em grade cada letra custa ~5 toques de D-pad, entao
// "stranger things" sao ~80 toques. Refazer a mesma busca amanha tem de custar
// UM toque (direita + OK), nao 80.
//
// REGRAS, e cada uma tem o seu porque:
//   - POR PERFIL (buscas-p<N>.txt; buscas.txt para perfil <= 0), como
//     fileirasui-p<N>.txt e bibliotecaui-p<N>.txt: o que o filho buscou nao
//     aparece na tela do pai, e vice-versa.
//   - NO MAXIMO BUSCASREC_MAX (10), MAIS RECENTE PRIMEIRO: 10 pilulas de ~200 px
//     cabem em 3 linhas na faixa de 1300 px a direita do teclado; mais que isso
//     empurraria o "Limpar" para fora da tela e viraria arquivo morto.
//   - SEM DUPLICATA: termo repetido SOBE para o topo. A comparacao ignora caixa
//     e espacos nas pontas ("Matrix " == "matrix"), porque o teclado da tela e
//     o fisico produzem a mesma busca com grafias diferentes.
//   - SO TERMOS COM >= 2 CARACTERES (depois de aparar), a mesma regua de
//     refiltrar() em busca.c: com uma letra a busca nem roda.
//   - NAO SOBE PARA A CONTA: e historico DESTE aparelho. Mas sair da conta
//     apaga (buscasrec_esquecer, chamado por sync_esquecer_usuario), senao a
//     proxima pessoa veria o que a anterior buscou.
//
// Quem decide QUANDO uma busca "foi feita" e busca.c, nao este modulo: aqui so
// ha a lista e o disco. Nada aqui e chamado de fio de trabalho.
#ifndef NV_BUSCASREC_H
#define NV_BUSCASREC_H

#define BUSCASREC_MAX   10
#define BUSCASREC_TERMO 48   // = BU_MAX_CONSULTA de busca.c (47 bytes + NUL)

// Quantos termos o perfil ATIVO tem. Recarrega do disco sozinho quando
// perfis_ativo() mudou desde a ultima leitura.
int         buscasrec_n(void);
// Termo i (0 = mais recente), ou "" fora da faixa. Ponteiro valido ate a
// proxima chamada que mude a lista.
const char *buscasrec_termo(int i);
// Registra uma busca feita. Devolve 1 se o termo entrou (ou subiu), 0 se foi
// recusado (curto demais). Grava no disco na hora.
int         buscasrec_registrar(const char *termo);
// Remove o termo i do perfil ativo. Grava na hora.
void        buscasrec_remover(int i);
// Remove todos os termos do perfil ativo (apaga o arquivo).
void        buscasrec_limpar(void);
// Logout: apaga o arquivo de TODOS os perfis (0..16) e a lista em memoria.
void        buscasrec_esquecer(void);

#endif
