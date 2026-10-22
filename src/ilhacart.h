// QUEM POE E TIRA OS CARTOES DA ILHA (ilha.h): a atividade ao vivo e a estreia.
//
// ATIVIDADE AO VIVO. O player, ao fechar, entrega a sessao (ilhacart_player_saiu).
// Ela vira cartao pelo MESMO criterio da faixa "Retomar agora" (entre 1% e o
// Percentual assistido, home_retorno_vale) e fica enquanto a faixa ficaria: a
// proxima saida do player a troca ou a tira (retomou e terminou, ou largou
// outro titulo no meio). Alem disso ela SAI SOZINHA depois de 30 min sem
// nenhuma tecla (VIVO_OCIOSO_MS): a TV esquecida ligada na home nao fica a
// noite inteira oferecendo um episodio de horas atras, e quem volta ainda tem
// a faixa da home. "Fechar" no modal tira na hora. Nada e gravado: fechar o
// app encerra.
//
// ESTREIA. O episodio novo de serie com lembrete (AV_AGENDA em avisos.c) ainda
// nao lido: o mais recente fica na pilula ate ser lido — a aba Avisos aberta,
// "Depois" ou "Dispensar" no modal, ou a pagina do titulo aberta (com o relogio
// ligado; desligado nada aqui marca coisa alguma). Desde 06/10 o modal da
// estreia e Assistir / Depois / Dispensar: "Depois" tira o cartao ate o app
// fechar, "Dispensar" tira aquele episodio de vez (ver ilha.h, ILHA_PEDIU_*).
//
// AMIGO VENDO AGORA (02/10; passageiro desde 06/10). Eventos de inicio de
// amigos com menos de 15 min viram UM aviso que some sozinho ("Ana está vendo
// Severance" ou "Ana e mais 2 estão assistindo"), uma vez por amigo + titulo,
// lembrado em disco (avisodisp). Nao ha mais cartao fixo ao lado do relogio.
//
// FIO PRINCIPAL.
#ifndef NV_ILHACART_H
#define NV_ILHACART_H
#include <SDL2/SDL.h>
#include <stddef.h>
#include "ilha.h"
#include "recomenda.h"

void ilhacart_player_saiu(int indice, double posSeg, double durSeg, int t, int e);
// A atividade ao vivo pertence a conta/perfil que encerrou o player. So
// compara identidade em RAM; chamar antes de eventos/quadro, mesmo no login.
void ilhacart_validar_identidade(void);
void ilhacart_esquecer_vivo(void);
// Valida tambem uma copia devolvida pelo modal antes de retomar/abrir titulo.
int ilhacart_vivo_vale(const IlhaCartao *cartao);
// Toda tecla ou clique (app_evento): o relogio da ociosidade recomeca.
void ilhacart_tecla(Uint32 agora);
// Sobe a cada saida do player que POS o cartao: compare antes e depois de
// player_encerrar para saber se ESTA saida virou atividade ao vivo.
unsigned ilhacart_vivo_seq(void);
// O titulo que o cartao ao vivo segura ("" sem cartao). Ver cwretido.h.
const char *ilhacart_vivo_imdb(void);
// Por quadro. `imdbAberto` = o titulo com a pagina aberta agora, ou NULL.
void ilhacart_atualizar(Uint32 agora, const char *imdbAberto);
// O modal pediu para tirar o cartao (Fechar / Dispensar). Na estreia,
// Dispensar grava (avisos_dispensar): aquele episodio nao volta.
void ilhacart_dispensar(int qual);
// "Depois" no modal da estreia: sai da pilula so ate o app fechar.
void ilhacart_adiar(int qual);
// OK no aviso "vendo:<imdb>" (ilhasinais.c entrega a chave). app.c le com
// ilhacart_pediu_atividade: imdb "" = varios amigos (so a aba Atividade).
void ilhacart_vendo_acao(const char *chave);
int  ilhacart_pediu_atividade(char *imdb, size_t tam);
// A chave em disco de "ja avisei este amigo neste titulo" (testes).
void ilhacart_vendo_chave(char *dst, size_t tam, const char *pessoa, const char *imdb);
// O aviso de um evento de AMIGO do feed, se e noticia (puro, para o teste):
// 1 = comecou uma serie ("vendo"), 2 = terminou / reagiu / deu nota, 0 = nada
// (filme comecado, saiu do player, salvo, grau 2, mais velho que 15 min).
// `txt` sai com o nome e o titulo em negrito.
int ilhacart_noticia(const RecEvento *ev, long long agora, char *txt, size_t tam);

#endif
