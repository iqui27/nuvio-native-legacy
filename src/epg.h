// EPG: grade de programacao dos canais ao vivo, vinda de um XMLTV externo.
//
// O FrostView (e os addons de canal em geral) nao publicam programacao: o
// catalogo traz nome, logo e categoria, e o /stream devolve so a URL do HLS.
// Sem uma fonte a parte, o guia de TV mostraria canais sem dizer o que esta
// passando — que era exatamente o pedido.
//
// A fonte e o epgshare01, que republica grades XMLTV gratuitas por regiao.
// Carregamos BR1+BR2 (Brasil, ~700 KB gzip cada) mais PT1/MX1/AR1 (Portugal,
// Mexico, Argentina) para cobrir canais de addons nao-brasileiros que o dono
// instale. US/UK ficam de fora: ~6 MB de gzip viram ~60 MB de XML, caro
// demais para ganho raro. O download acontece UMA vez por sessao num fio
// proprio, e o XML fica gravado na pasta de dados: na proxima abertura o
// guia ja tem grade sem tocar na rede.
//
// O casamento entre "o canal que o addon anuncia" e "o canal que a grade
// conhece" e por NOME NORMALIZADO (acento fora, maiuscula fora, sufixos de
// qualidade fora) com uma tabela de apelidos para os casos que divergem.
// Alem do exato, ha regras para afiliada regional ("SBT RJ" casa com a rede
// "sbt" pelo primeiro token; "TV Cidade - RecordTV" por substring unica) —
// medido no catalogo real: ~40% dos 768 canais tem grade; o resto e loop
// "24h" sem programacao em nenhuma fonte e fica como "AO VIVO".
// Canais sem grade real (os "24h" de um filme so, cams de reality, feeds de
// evento) ficam sem casamento — o guia os mostra como "ao vivo", sem linha de
// programa. Ver GUIA-EPG.md ou o comentario de epg_match.
#ifndef NV_EPG_H
#define NV_EPG_H

#include <time.h>

// Um programa da grade. `titulo` aponta para memoria do modulo: vale ate a
// proxima recarga (epg_passo com grade nova), nunca precisa de free.
typedef struct {
  time_t ini, fim;
  const char *titulo;
} EpgProg;

// Estado da carga. PARADO nunca tentou; BAIXANDO tem fio em voo; PRONTO tem
// grade em memoria; FALHOU e terminal ate a proxima guia_iniciar.
enum { EPG_PARADO, EPG_BAIXANDO, EPG_PRONTO, EPG_FALHOU };
int epg_estado(void);

// Dispara o fio de carga se ainda nao rodou. Idempotente.
void epg_iniciar(void);

// Publica o resultado do fio no fio principal e pede re-carga quando a grade
// envelheceu (o arquivo cobre ~3,5 dias; renovar a cada 12 h basta). Chamar
// por quadro enquanto o guia ou o overlay de canais estiver vivos.
void epg_passo(void);

// Indice do canal da grade que corresponde a `nome` (o nome que o addon da),
// ou -1. So responde com epg_estado()==EPG_PRONTO; antes disso devolve -1 e
// quem chamou deve tentar de novo depois.
int  epg_match(const char *nome);

// Programa NO AR agora no canal `epg` (indice devolvido por epg_match).
// Devolve 1 e preenche *p. 0 = sem grade ou fora dela.
int  epg_agora(int epg, time_t agora, EpgProg *p);

// k-esimo programa DEPOIS do que esta no ar (k=0 e o proximo). Mesmo retorno.
int  epg_proximo(int epg, time_t agora, int k, EpgProg *p);

// --- SUPERFICIE DE TESTE ----------------------------------------------------
// epg_xml_processar alimenta o parser sem rede nem disco: os testes chamam
// isto com um XMLTV de molde e conferem match/agora. Fora de teste, so o fio
// de carga a usa.
void epg_teste_limpar(void);
int  epg_xml_processar(char *xml);   // consome/estraga o buffer

#endif
