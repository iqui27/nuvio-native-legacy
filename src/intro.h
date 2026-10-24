// MARCADORES DE ABERTURA, RESUMO E CREDITOS.
//
// TheIntroDB (api.theintrodb.org/v4/media) e a fonte principal. IntroDB
// (api.introdb.app/segments) e outro servico: preenche apenas tipos ausentes,
// sem substituir TheIntroDB, AniSkip ou capitulos do arquivo.
//
// v4 accepts imdb_id or tmdb_id for lookup; responses carry tvdb_id only.
// Movies need no season/episode. TV coordinates use the provider's numbering.
// Example from the v4 OpenAPI (episode-media):
//   {"tvdb_id":71663,"type":"tv","season":1,"episode":1,
//    "intro":[{"start_ms":30000,"end_ms":90000},
//             {"start_ms":120000,"end_ms":150000}]}
//
// SEM CHAVE e sem cadastro. Titulo desconhecido responde 404 com
// {"error":"media not found"}, que aqui vira "zero marcadores".
#ifndef NV_INTRO_H
#define NV_INTRO_H
#include <stddef.h>
// `fim` ZERO QUER DIZER "ATE O FIM DA MIDIA", que e como a API representa o
// `end_ms: null` dos creditos. Quem consome tem de tratar esse caso — ver
// intro_ativo.
typedef struct { double inicio,fim; int tipo; int ani; } IntroTrecho; // ani 1 = veio do AniSkip
enum { INTRO_ABERTURA=1, INTRO_RESUMO=2, INTRO_CREDITOS=3, INTRO_PREVIA=4 };
// `temporada` e `episodio` ZERO = filme: a consulta sai so com o imdb.
void intro_pedir(const char *imdb,int temporada,int episodio);
// Igual a intro_pedir, mas diz a duracao (s) dos episodios E-1 e E+1 quando o
// catalogo sabe (0 = desconhecida): sem marcador deste episodio, o marcador de
// um vizinho vira "quanto falta para o fim". Ver credfonte.h.
void intro_pedir_vizinhos(const char *imdb,int temporada,int episodio,double durAnt,double durProx);
// O pedido completo (2.0.3). `tmdb` > 0: pergunta por tmdb_id, e `temporada`/
// `episodio` TEM de ser o par do TMDB (CatEp.tmdbT/tmdbE; num filme, 0/0).
// tmdb 0: imdb_id com a numeracao do Cinemeta. A duracao vai depois, sozinha,
// quando o player a informar (intro_definir_duracao).
void intro_pedir_ids(const char *imdb,long tmdb,int temporada,int episodio,double durAnt,double durProx);
// O backup IntroDB usa o par original do catalogo/IMDb, mesmo quando a fonte
// principal recebe o par confirmado pelo TMDB. Nunca reutilizar o par remapeado.
void intro_pedir_ids_mapeados(const char *imdb,long tmdb,int t,int e,int imdbT,int imdbE,double durAnt,double durProx);
// Contrato do backup: URL /segments e objetos em segundos (ou ms). Retorna
// -1 para identidade/resposta invalida; zero para resposta valida sem trechos.
int intro_montar_url_backup(char *url,size_t n,const char *imdb,int t,int e);
int intro_extrair_backup(const char *json,const char *imdb,int t,int e,IntroTrecho *out,int max);
// Evidencia local: corpo TMDB ja baixado; Animation + ja/JP, sem GET novo.
// Se chegar depois do pedido, refaz uma vez ao reconhecer anime.
void intro_registrar_tmdb(const char *id,long tmdb,const char *json);
// URL do pedido (exposta para o teste). durSeg 0 = sem duration_ms.
void intro_montar_url(char *url,size_t n,const char *imdb,long tmdb,int t,int e,double durSeg);
// Troca quem faz o GET (teste). Devolve o status HTTP (0 = sem resposta) e o
// corpo em *corpo (malloc) quando 2xx. NULL volta ao padrao (rede_pedir).
void intro_definir_buscador(int (*f)(const char *url,char **corpo,int *status));
// ANISKIP (kitsu:/mal: ou IMDb/TMDB de anime mapeado pela Fribb, intro.c). Le a resposta de
// /v2/skip-times: op -> abertura, ed -> creditos, recap -> resumo; por tipo, o
// lancamento de duracao mais proxima de `dur`, recusado se diferir mais que
// INTRO_ANISKIP_TOLERA (outro corte). dur 0 = nao compara.
#define INTRO_ANISKIP_TOLERA 0.10
int  intro_tipos_completos(const IntroTrecho *v,int n);
int  intro_completar(IntroTrecho *dst,int n,const IntroTrecho *src,int ns,int max);
int  intro_extrair_aniskip(const char *json,double dur,IntroTrecho *out,int max);
void intro_montar_url_aniskip(char *url,size_t n,long mal,int ep,double dur);
// O id do MyAnimeList na resposta de kitsu.io/api/edge/anime/<id>/mappings; 0 = nao ha.
long intro_kitsu_mal(const char *json);
// Trechos declarados pelo PROPRIO arquivo (capitulos do MKV): substituem, por
// tipo, o que o TheIntroDB/AniSkip trouxe, tenha chegado antes ou depois. n=0
// limpa. Zerados por intro_desligar.
void intro_definir_capitulos(const IntroTrecho *v,int n);
void intro_desligar(void);
int  intro_ativo(double posSeg,double *fim,int *tipo);
// Segundo em que os creditos comecam, ou 0 quando nao ha marcador. Serve ao
// posplay.c, que precisa do INSTANTE e nao de "estou dentro".
double intro_creditos_seg(void);
// 1 se os trechos (e o marcador acima) vieram do AniSkip; 0 = TheIntroDB.
int intro_creditos_aniskip(void);
// Duracao real da midia (0 = desconhecida) e se e filme: base da guarda de
// janela. Tambem pede de novo com `duration_ms` quando ela chega ou muda (como
// o plugin oficial) e tenta de novo depois de uma falha de rede/5xx.
void intro_definir_duracao(double dur,int filme);
// Janela aceitavel? Creditos <= 900 s, abertura/resumo <= 180 s, creditos de
// filme so a partir de 50% da duracao, creditos de SERIE so na parte final
// (intro_creditos_janela), abertura/resumo so na primeira metade, e nenhum
// trecho com fim explicito alem da duracao (marcador de outro corte). `motivo`
// (opcional) diz o porque.
int  intro_janela_ok(int tipo,double ini,double fim,double dur,int filme,const char **motivo);

// ONDE OS CREDITOS DE UM EPISODIO PODEM COMECAR (2.0.3). Um marcador de creditos
// so vale se sobrar ate esta janela de episodio depois dele: 15% da duracao,
// com piso de 2 min (episodio curto, anime com ED + previa ~110 s) e teto de
// 5 min (o #34: "dez minutos antes do fim nao e credito"). Nunca mais que
// metade do episodio. 22 min -> 198 s, 25 min -> 225 s, 45 min+ -> 300 s.
#define INTRO_CRED_FRAC   0.15
#define INTRO_CRED_MIN_S  120.0
#define INTRO_CRED_MAX_S  300.0
double intro_creditos_janela(double dur);

// SEM MARCADOR NENHUM, quanto antes do fim o cartao do proximo episodio sobe
// (2.0.3, pedido do dono: NADA de porcentagem). Tempo FIXO de creditos finais
// tipicos: medidos no TheIntroDB, Breaking Bad T1E1/T1E2 tem 47 s/45 s de
// creditos, Silo T1E1 ~100 s, anime ~110 s (ED + previa). Eram 40 s; 50 s desde
// 08/10: na TCL do dono os creditos de Silo T2E6 comecam ~50 s antes do fim
// (o marcador do TheIntroDB, 2519 s de 3077 s, e de outro corte e e recusado
// pela janela), e com 40 s o cartao chegava atrasado. Episodio curto
// (< 10 min, desenho/web serie) tem creditos de ~10-20 s: 15 s. Abaixo de 2 min
// nao e episodio (clipe de erro, duracao provisoria): nao ha estimativa.
#define INTRO_FIM_SERIE_S   50.0
#define INTRO_FIM_CURTO_S   15.0
#define INTRO_CURTO_ATE_S  600.0
#define INTRO_DUR_MIN_S    120.0
// Segundos antes do fim em que a estimativa vale; 0 = nao estimar.
double intro_fim_estimado(double dur);

// FILME (2.0.3). Creditos de filme sao longos (10-20 min nos de super-heroi),
// entao a janela do marcador e mais larga: 12% da duracao, piso de 5 min, teto
// de 15 min, nunca mais que metade. 2h -> 864 s, 1h30 -> 648 s, 2h30+ -> 900 s.
// Abertura de filme so nos primeiros 20% (Inception: 0-38 s; Interstellar 0-53 s).
#define INTRO_CRED_FILME_FRAC   0.12
#define INTRO_CRED_FILME_MIN_S  300.0
#define INTRO_CRED_FILME_MAX_S  900.0
#define INTRO_ABERTURA_FILME_FRAC 0.20
double intro_creditos_janela_filme(double dur);
// SEM MARCADOR, o painel do fim do filme ("Mais como este") sobe a um tempo
// FIXO antes do fim (dono, 07/10: sem porcentagem; era 4,5% com piso de 150 s
// e teto de 330 s): 3 min; filme de menos de 1 h, 90 s; menos de 10 min (curta,
// clipe), nada.
#define INTRO_FIM_FILME_S        180.0
#define INTRO_FIM_FILME_CURTO_S   90.0
#define INTRO_FILME_CURTO_ATE_S 3600.0
#define INTRO_FILME_MIN_S        600.0
double intro_fim_estimado_filme(double dur);

// A RESPOSTA E DESTE EPISODIO? O TheIntroDB remapeia a numeracao do IMDb para a
// do TMDB e ecoa `season`/`episode` do que achou. Medido em 07/10/2026: One
// Piece imdb T4E1 e T4E2 voltam AMBOS como T1E48 — o marcador seria de outro
// episodio. So vale a resposta que ecoa o mesmo par pedido (filme: sempre).
int  intro_resposta_confere(const char *json,int temporada,int episodio);
// Botao de pular com tempo: some em INTRO_BOTAO_SEG sem foco, uma aparicao
// automatica por trecho, volta so com os controles (osd) de pe.
#define INTRO_BOTAO_SEG 10.0
int  intro_botao(double posSeg,double agora,int osd,int focado,double *fim,int *tipo);
int  intro_botao_visivel(double *fim,int *tipo);
void intro_botao_esconder(double agora);
// #329/#331: o trecho (tipo, fim) deve ser pulado sozinho? Abertura/resumo: ajuste ligado e
// fim conhecido. Creditos: ajuste proprio, fim EXPLICITO do marcador e a pelo menos 5 s do fim
// da midia `dur` (creditos ate o fim ficam com o proximo episodio, nunca um seek para alem do fim).
int  intro_auto_pular(int tipo,double fim,double dur,int pularAbertura,int pularResumo,int pularCreditos);
// Mesmo trecho ativo; creditos exigem limites explicitos da resposta atual.
int  intro_auto_ativo(double pos,double *fim,int *tipo);
// Pulou sozinho este trecho nesta reproducao? (uma vez so; voltar nao repete). Zera a cada pedido.
int  intro_auto_ja(int tipo,double fim);
void intro_auto_marcar(int tipo,double fim);
// Recuo manual protege tambem creditos cujo marcador ainda nao chegou.
void intro_auto_recuar(double de,double para);
int  intro_extrair(const char *json,IntroTrecho *saida,int max);
// Copia ate `max` trechos conhecidos; devolve quantos.
int  intro_trechos(IntroTrecho *saida,int max);
// Copia apenas creditos com inicio/fim numericos explicitos e verificados na
// resposta atual. Sem fim, vizinhos, capitulos e estimativas nao entram.
int  intro_creditos_limitados(IntroTrecho *saida,int max);
#ifdef NV_SHOT_HOOKS
void intro_shot_definir(const IntroTrecho *v,int n);   // capturas: trechos fixos
#endif
#endif
