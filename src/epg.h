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

// Device-local memory policy, set on the drawing thread. Workers capture it;
// low mode retains current programmes and the next 24 hours. Full mode may
// refresh a shortened binary cache to recover later programmes.
enum { EPG_MEMORIA_AUTO, EPG_MEMORIA_BAIXA, EPG_MEMORIA_COMPLETA };
void epg_memoria_definir(int modo, long ramMB);

// Native account schedules never match another account's IDs or names.
// Configure on the drawing thread; the worker captures each URL and profile.
typedef struct { int tipo; unsigned id; char url[1100]; } EpgContaFonte;
void epg_contas_configurar(const EpgContaFonte *fontes, int n);
void epg_conta_ids(int tipo, unsigned id, const char *const *ids, int n);
int epg_canal_estado(const char *canal);
// State of one source while other account schedules may still be loading.
int epg_fonte_estado(const char *chave);
int epg_match_canal(const char *canal, const char *epgId, const char *nome);
unsigned epg_revisao(void);

// Dispara o fio de carga se ainda nao rodou. Idempotente.
void epg_iniciar(void);
void epg_fonte_m3u(const char *url);
void epg_so_provedores(int so);
void epg_recarregar(void);
// Invalida a grade privada e o worker anterior. Os arquivos publicos de
// epgshare01 continuam no disco, prontos para reaproveitar sem rede.
void epg_abrir_sessao(int perfil);

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
// Pelo id do <channel> da grade (o epg_channel_id do Xtream), exato. So
// devolve canal COM programa; -1 e "tente pelo nome" (#158).
int  epg_match_id(const char *id);
// Sexta fonte, opcional: a grade XMLTV do proprio provedor de IPTV (gzip ou
// XML). "" tira. Pode chamar de qualquer fio; a (re)carga acontece em
// epg_passo. A URL pode levar credencial: epg.c nunca a imprime.
void epg_fonte_extra(const char *url);
// Os ids (epg_channel_id do Xtream) que interessam na grade do provedor: so
// eles entram dela (o arquivo traz o painel inteiro). NULL/0 tira o filtro.
// Copia; vale a partir da proxima carga.
void epg_fonte_extra_ids(const char *const *ids, int n);

// PAISES DA GRADE (#158): quais arquivos do epgshare01 entram. `codigos` e uma
// lista de paises ISO ("RO", "RO,BR"; "GB" = "UK"); vazio ou so desconhecidos
// = as cinco de sempre (BR1, BR2, PT1, MX1, AR1). Mudar recarrega a grade no
// proximo epg_passo. Devolve quantos arquivos ficaram. epg_paises_ativos: os
// paises em uso, para log e tela. epg_pais_existe: ha arquivo para o pais.
int epg_paises_definir(const char *codigos);
const char *epg_paises_ativos(void);
int epg_pais_existe(const char *pais);

// O nome sem o prefixo de pais/pacote do painel ("RO: Pro TV" -> "Pro TV").
// `pais` (opcional, 4 bytes) recebe o codigo de 2 letras quando o prefixo e
// um, em maiusculas; "" senao. Nao aloca: devolve um ponteiro dentro de `s`.
const char *epg_sem_prefixo(const char *s, char pais[4]);

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
// em ordem de inicio. Com `out`, escreve e devolve no maximo `cap` (nunca mais
// do que cabe no buffer: #344); se devolveu `cap`, pode haver mais, e quem
// quiser tudo pagina com epg_faixa_desde. Com out NULL, so conta
// e devolve quantos EXISTEM. Um programa que atravessa `de` entra inteiro, com o
// ini real (anterior a `de`); o guia decide como desenhar a parte de fora.
// Sem alocacao; O(log n + resultado).
int  epg_faixa(int epg, time_t de, time_t ate, EpgProg *out, int cap);
// O mesmo, pulando os `pular` primeiros da sequencia. Para paginar: cada
// chamada passa o total ja entregue, e cada programa e visto uma vez so, mesmo
// com sobreposicao na grade (o horario nao serve de cursor).
int  epg_faixa_desde(int epg, time_t de, time_t ate, int pular, EpgProg *out, int cap);

// --- GRADES POR FONTE (guia de TV, M4/M5/M9) -----------------------------------
// Alem do epgshare01 (fonte 0, por pais), ate EPG_MAX_EXTRAS grades de quem
// SABE delas: o xmltv.php do Xtream, o url-tvg de uma M3U, um XMLTV por URL dos
// Ajustes. Cada uma tem a CHAVE da fonte de canais dona dela ("xt", "m1a2b3c4";
// "" = vale para todas), a URL (pode levar credencial: nunca vai a log, no disco
// fica so um hash) e um FILTRO: os epg ids e os nomes da lista. A grade e lida
// EM FLUXO (xmltv.h) e so os canais do filtro ficam; o resultado vai para um
// cache binario por fonte (epg-x-<chave>.bin), que a proxima abertura le sem
// tocar a rede enquanto tiver menos de 12 h. Sem teto de tamanho de arquivo: o
// teto agora e de memoria retida (programas e titulos), com uma linha de log.
#define EPG_MAX_EXTRAS 40
typedef struct {
  const char *chave;           // chave da fonte de canais (ltv.h)
  const char *url;
  const char *const *ids;      // epg ids da lista (tvg-id, epg_channel_id)
  int nIds;
  const char *const *nomes;    // nomes da lista (crus: o epg normaliza)
  int nNomes;
} EpgExtra;
// Copia tudo. Mudou a URL ou a fonte: recarrega no proximo epg_passo. Mudou so o
// filtro: vale a partir da proxima carga.
void epg_extras_definir(const EpgExtra *v, int n);

// O resultado de cada grade na ultima carga, para a saude da fonte e o log.
typedef struct {
  char chave[24];
  int  ok;                 // 1 = entrou (da rede ou do cache)
  int  deCache;            // 1 = veio do cache binario
  int  canais, programas;
  long long bytes;         // bytes lidos da rede (0 do cache)
  long idadeS;             // idade do que entrou
  int  descartados;        // programas fora por teto de memoria
} EpgFonteInfo;
int epg_fontes_info(EpgFonteInfo *out, int max);

// CASAMENTO EM CASCATA (M5), explicavel: 1) epg id exato (sem caixa), primeiro
// na grade da propria fonte (`fonte` = chave, NULL = qualquer); 2) tvg-name e
// 3) nome normalizados exatos; 4) apelidos e as regras de afiliada/prefixo de
// epg_match. O fuzzy NAO casa sozinho: so sugere (epg_sugerir). O manual e do
// guia (ele guarda a escolha). Devolve o indice ou -1; *metodo = EPG_M_*.
enum { EPG_M_NENHUM = 0, EPG_M_ID, EPG_M_NOME, EPG_M_ALIAS, EPG_M_MANUAL };
int epg_casar(const char *fonte, const char *epgId, const char *tvgNome,
              const char *nome, int *metodo);
// Canal da grade por fonte + id exato (a escolha manual guardada). -1 = nao ha.
int epg_achar(const char *fonte, const char *id);
// Para o painel "Escolher grade deste canal": os canais da grade COM programa
// mais parecidos com `nome` (palavras em comum), do melhor para o pior.
int epg_sugerir(const char *nome, int *out, int max);
int epg_n_canais(void);
const char *epg_canal_id(int epg);
const char *epg_canal_nome(int epg);
const char *epg_canal_icone(int epg);      // XMLTV <icon src>; "" = absent
const char *epg_icone_id(const char *fonte, const char *id); // also channels without programmes
const char *epg_canal_fonte(int epg);      // "" = epgshare01

// --- SUPERFICIE DE TESTE ----------------------------------------------------
// epg_xml_processar alimenta o parser sem rede nem disco: os testes chamam
// isto com um XMLTV de molde e conferem match/agora. Fora de teste, so o fio
// de carga a usa.
void epg_teste_limpar(void);
int  epg_xml_processar(char *xml);   // consome/estraga o buffer

#endif
