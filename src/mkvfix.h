// SEEK NUM MKV CUJO INDICE A TV NAO ACHA (dono, 16/10/2026, logs 21 e 24).
//
// Baka and Test [KH] S1-01: todo seek (e o Retomar) voltava "Failed to set play
// position : InvalidState" e o video seguia de onde estava. Os outros episodios
// do mesmo pacote buscavam normal. O que so esse arquivo tem: o SeekHead do
// inicio tem 17 bytes e UMA entrada, que aponta para um SEGUNDO SeekHead no fim
// do arquivo — e e o segundo que diz onde esta o Cues (o indice de seek). E o
// que o mkvpropedit deixa quando edita um arquivo e falta espaco no inicio. O
// player da Samsung nao segue o segundo SeekHead: acha que nao ha indice e
// recusa todo seek.
//
// O CONSERTO, SEM CONVERSOR: um servidor em 127.0.0.1 repassa os Ranges do
// player ao link de verdade (o mesmo arquivo, byte a byte) e troca so a regiao
// do SeekHead do inicio mais o Void logo atras dele (70 bytes no caso real) por
// um SeekHead que lista o Cues DIRETO (e mantem a entrada para o segundo, e o
// que mais couber), completado por um Void do mesmo tamanho. Nenhum
// deslocamento do arquivo muda.
//
// Quando: video_tpk.c sonda o link que o player recebeu (2-3 Ranges pequenos)
// quando o player fica pronto. So o PRIMEIRO SEEK de um arquivo que precisa
// reabre pelo servidor, ja no alvo (o mesmo caminho da troca do link do CDN);
// arquivo normal nunca passa por aqui. O link do CDN vencido: o servidor pede
// de novo ao link de origem e segue.
//
// LEGENDAS DE TEXTO ESCONDIDAS DA TV (dono, 20/10/2026, S01E03 Itsuki Gang).
//
// O Tizen.Multimedia.Player decodifica SEMPRE a faixa de legenda padrao do MKV,
// mesmo com o app desenhando a legenda (o .tpk nao tem como desligar a
// legenda nativa: "legenda nativa desligada" so parava de escrever a faixa).
// Num evento ASS de desenho vetorial gigante (\p1 de 71 KB com \move, aos
// 22:15.08, nas duas faixas do arquivo; o resto do episodio nao passa de 640
// bytes por evento) o player da TV trava inteiro: o relogio para ~2 s depois,
// GetPlayPosition leva 5 s e o app via "conexao caiu". Mesma temporada inteira,
// sempre no mesmo quadro, com legenda desligada ou externa. O AVPlay (outros
// apps, NuvioTVSmart) silencia o motor de legenda da TV e passa.
//
// O CONSERTO: o mesmo servidor local serve o arquivo byte a byte, mas o
// TrackType de cada legenda de texto vira "controle" (0x11 -> 0x20: um byte,
// mesmo tamanho, nenhum deslocamento muda). A TV nao cria a faixa e ignora os
// blocos dela; o app segue lendo e desenhando a legenda pelo link original
// (mkvass). So arquivos com faixa ASS/SSA passam por aqui.
// Medido no corte do S01E03 (dono, 20/10): GStreamer (base do player nativo
// do Tizen) e FFmpeg entregam video e audio identicos ao original, byte a
// byte. Trocar a TrackEntry inteira por Void NAO serve: o FFmpeg acusa
// "Invalid track number" e joga fora o resto do Cluster (metade do video).
//
// Nunca vai para log: link, cabecalhos, token.
#ifndef NV_MKVFIX_H
#define NV_MKVFIX_H
#include <stddef.h>
#include <stdint.h>

enum {
  MKVFIX_NADA = 0,      // nada sondado
  MKVFIX_SONDANDO,      // lendo o cabecalho
  MKVFIX_NORMAL,        // o SeekHead do inicio ja lista o Cues (ou nao e MKV)
  MKVFIX_PRECISA,       // so o segundo SeekHead lista o Cues: o servidor conserta
  MKVFIX_SEM_INDICE,    // nenhum Cues achado: nada a consertar
  MKVFIX_FALHOU         // rede / Range recusado
};

// ---- puro (testes) --------------------------------------------------------
#define MKVFIX_ENTRADAS 16
typedef struct {
  int64_t segIni;        // inicio dos dados do Segment (absoluto)
  int64_t regiao;        // inicio do 1o SeekHead (absoluto); -1 = nao ha
  int regiaoN;           // bytes do SeekHead + o Void logo depois dele
  int n;                 // entradas do 1o SeekHead
  uint32_t id[MKVFIX_ENTRADAS];
  int64_t pos[MKVFIX_ENTRADAS];   // relativas ao Segment, como no arquivo
} MkvfixCab;
// O inicio do arquivo (n bytes). 1 = Matroska com o 1o elemento do Segment lido.
int  mkvfix_ler_inicio(const unsigned char *b, long n, MkvfixCab *c);
// Um elemento SeekHead inteiro (cabecalho incluso) em `b`: as entradas.
// Devolve quantas, -1 se nao e SeekHead / esta cortado.
int  mkvfix_ler_seekhead(const unsigned char *b, long n, uint32_t *id, int64_t *pos, int max);
// Posicao (relativa) de `alvo` nas entradas, ou -1.
int64_t mkvfix_entrada(const uint32_t *id, const int64_t *pos, int n, uint32_t alvo);
// Monta exatamente `len` bytes: SeekHead com as entradas na ordem dada (as que
// couberem; repetidas pelo id ficam de fora) e um Void no resto. Devolve
// quantas entradas entraram (0 = nao coube nem a primeira).
int  mkvfix_montar(const uint32_t *id, const int64_t *pos, int n, unsigned char *out, int len);
// O elemento Tracks inteiro (cabecalho incluso) em `b`. Quando ha ao menos uma
// legenda ASS/SSA, devolve quantas legendas de TEXTO (S_TEXT/*, S_ASS, S_SSA)
// esconder e, em `off`, a posicao (relativa a `b`) do byte de VALOR do
// TrackType de cada uma (hoje 0x11; o servidor manda 0x20). 0 = nada a
// esconder; -1 = nao e Tracks / esta cortado.
#define MKVFIX_TIPO_LEGENDA  0x11
#define MKVFIX_TIPO_ESCONDIDA 0x20
#define MKVFIX_OCULTAS 16
int  mkvfix_legendas_texto(const unsigned char *b, long n, int64_t *off, int max);
// Posicao absoluta do Tracks a partir do inicio do arquivo ja lido (pelo
// SeekHead ou pelos elementos do topo dentro de `n`); -1 se nao achou.
int64_t mkvfix_tracks_pos(const unsigned char *b, long n, const MkvfixCab *c);

// ---- o servidor (fio do app) ----------------------------------------------
// Sonda `url` (o link que o player tocou) num fio. `origem` (pode ser NULL ou
// igual) e de onde pedir um link novo se o do CDN vencer. Troca a sessao
// anterior (cancela as conexoes dela).
void mkvfix_sondar(const char *url, const char *cabecalhos, const char *origem);
// Selected-file refresh identity is copied before the probe thread starts.
void mkvfix_sondar_arquivo(const char *url, const char *cabecalhos, int usenet, int item, int file);
int  mkvfix_estado(void);
// Legendas de texto que a sessao atual esconde da TV (0 = nenhuma ou sondando).
// > 0: o arquivo deve tocar pelo servidor (mkvfix_url) desde a abertura.
int  mkvfix_ocultas(void);
// O endereco local do arquivo consertado (sobe o servidor na primeira vez).
// 0 se a sessao atual nao e MKVFIX_PRECISA nem esconde legendas, ou o servidor
// nao subiu.
int  mkvfix_url(char *out, size_t n);
int  mkvfix_e_url(const char *url);
// Fim do video: cancela a sessao e as conexoes (o servidor fica de pe).
void mkvfix_parar(void);

#endif
