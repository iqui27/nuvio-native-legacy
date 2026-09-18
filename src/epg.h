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
// envelheceu. Chamar por quadro enquanto o guia ou o overlay de canais
// estiver vivos. Cada publicacao TROCA a grade inteira: os indices de
// epg_match e os `titulo` de EpgProg obtidos antes morrem — quem guarda um
// EpgProg entre quadros precisa refazer a consulta quando epg_passo publicar
// (o guia ja faz isso pelo estado voltar a PRONTO).
//
// QUANTO A GRADE COBRE (medido em 2026-09-18 nos XML reais do epgshare01):
// de 3,3 a 3,8 dias A FRENTE do download e ate 1,8 dia para tras, por fonte.
// Uma semana NAO vem no arquivo — nao e questao de retencao, e o publicador
// que para ali. O modulo retem tudo que e futuro e ate 48 h de passado; para
// saber ate onde vai, epg_janela/epg_janela_total, e o guia deve tratar o
// que esta ALEM da janela como "sem dados", nao como "nada no ar".
void epg_passo(void);

// Indice do canal da grade que corresponde a `nome` (o nome que o addon da),
// ou -1. So responde com epg_estado()==EPG_PRONTO; antes disso devolve -1 e
// quem chamou deve tentar de novo depois.
int  epg_match(const char *nome);

// Programa NO AR no instante `agora` (qualquer instante, nao so o presente:
// para desenhar a coluna de amanha, passe amanha) no canal `epg` (indice
// devolvido por epg_match). Devolve 1 e preenche *p. 0 = sem grade, fora da
// janela, ou buraco entre programas — os tres sao indistinguiveis aqui; use
// epg_janela para separar "sem dados" de "buraco".
int  epg_agora(int epg, time_t agora, EpgProg *p);

// k-esimo programa DEPOIS do que esta no ar em `agora` (k=0 e o proximo).
// Mesmo retorno.
int  epg_proximo(int epg, time_t agora, int k, EpgProg *p);

// --- GRADE DE VARIOS DIAS ----------------------------------------------------
// Janela coberta pela grade do canal: inicio do primeiro programa retido e fim
// do ultimo. Devolve 0 (e nao toca *ini/*fim) se o canal nao tem grade. E a
// unica forma de dizer com verdade "a programacao termina aqui".
int  epg_janela(int epg, time_t *ini, time_t *fim);

// Uniao das janelas de todos os canais. 0 = grade vazia.
int  epg_janela_total(time_t *ini, time_t *fim);

// Quantos programas a grade retem para o canal (para dimensionar buffers).
int  epg_total(int epg);

// Programas do canal que TOCAM o intervalo [de, ate) — fim > de e ini < ate —
// em ordem de inicio. Escreve no maximo `cap` em `out` (out pode ser NULL com
// cap 0 para so contar) e devolve quantos EXISTEM, que pode ser maior que
// `cap`: quem chamou sabe que cortou e pode continuar chamando com
// de = out[cap-1].fim. Um programa que atravessa `de` entra inteiro, com o
// ini real (anterior a `de`); o guia decide como desenhar a parte de fora.
// Sem alocacao; O(log n + resultado).
int  epg_faixa(int epg, time_t de, time_t ate, EpgProg *out, int cap);

// --- SUPERFICIE DE TESTE ----------------------------------------------------
// epg_xml_processar alimenta o parser sem rede nem disco: os testes chamam
// isto com um XMLTV de molde e conferem match/agora. Fora de teste, so o fio
// de carga a usa.
void epg_teste_limpar(void);
int  epg_xml_processar(char *xml);   // consome/estraga o buffer

#endif
