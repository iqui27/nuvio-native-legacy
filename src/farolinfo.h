#ifndef NV_FAROLINFO_H
#define NV_FAROLINFO_H
// FAROLINFO — os fatos da peca P6 ("info", PLANO-GOSTO 4.2). Issue 47.
//
// A P6 e a coluna de ate 3 linhas ao lado do "por que voce vai gostar" na
// pagina do titulo: qualidade, continuar, tempo, onde, amigos, saga, rever e
// retomada. Cada linha e um FATO medido a partir de dados que ja foram lidos
// (fontes carregadas, notas por temporada, duracao, relogio, Diario) — nada
// aqui pede rede nem toca o catalogo.
//
// CODIGOS, NAO TEXTO. A frase mora em src/faroltexto.h (issue 35); este
// modulo devolve a CHAVE do modelo (FarolInfoLinha.m = "FT_INFO_*") e os
// argumentos crus (a[]), no mesmo formato do "l" da cena: quem desenha faz
// i18n(m) e encaixa os argumentos. Argumento de tempo/data vai codificado
// (ver FAROLINFO_MES e o comentario de retomada) para a camada de texto
// localizar — o modulo nao escreve nem mes por extenso.
//
// PURO: sem SDL, sem rede, sem dados.h, sem estado. O RELOGIO E INJETADO:
// quem chama passa struct tm/epoch; o modulo nao chama time() nem
// localtime_r(). Nenhum titulo, nome de perfil ou gosto em log — este modulo
// nao loga nada.
//
// "Sem dado medido, sem linha" (regra da 4.2): toda funcao devolve quantas
// linhas escreveu em `sai` (0 quando o fato nao existe ou nao se sustenta).

#include <time.h>

#define FI_ARGS   3    // nenhuma FT_INFO_* tem mais de 3 argumentos
#define FI_ARG_N 64    // um nome de amigo UTF-8 (63 + NUL) cabe inteiro
#define FI_K_NOMES "qualidade\0continuar\0tempo\0onde\0amigos\0saga\0rever\0retomada\0"

enum {
  FI_K_QUALIDADE = 0, FI_K_CONTINUAR, FI_K_TEMPO, FI_K_ONDE,
  FI_K_AMIGOS, FI_K_SAGA, FI_K_REVER, FI_K_RETOMADA, FI_K_N
};

// Uma linha da P6, ainda sem texto: k = a chave do fato, m = o modelo FT_*,
// a[] = os argumentos na ordem do modelo, na = quantos.
typedef struct {
  int  k;
  char m[40];
  int  na;
  char a[FI_ARGS][FI_ARG_N];
} FarolInfoLinha;

// O nome curto do k para serializar a cena ("qualidade" ... "retomada").
const char *farolinfo_k_nome(int k);

// --- entradas --------------------------------------------------------------

// Uma fonte resumida pelo chamador a partir do Stream carregado (o modulo
// nao conhece streams.h, que puxa SDL). toca: -1 nao se sabe (NUNCA gera
// linha nesta P6 — a linha afirma, e "nao se sabe" nao e fato), 0 a TV
// nao toca, 1 a TV toca confirmado.
typedef struct {
  int  altura;             // 2160, 1080, 720... 0 = desconhecida
  int  dv;                 // 1 = Dolby Vision declarado na fonte
  int  hdr;                // 1 = HDR (nao DV) declarado — generico
  int  toca;               // -1 desconhecido, 0 nao toca nesta TV, 1 toca
  char audio[12];          // ISO do idioma de audio principal; "" = nao dito
  char legenda[12];        // idem legenda
} FarolInfoFonte;

// O que a TV toca (o chamador le de stream_capacidades): 1 sim, 0 nao,
// -1 nao se sabe. DV so e citado com dv == 1 — "desconhecido" nao vira
// afirmacao ("so o que esta TV toca", 4.2).
typedef struct { int hdr, dv; } FarolInfoCaps;

// Uma temporada para a linha "continuar": a media das notas (x10, a escala
// de CatEp.notaImdb) dos episodios COM nota. comNota < 5 nunca sustenta
// comparacao — a regra do aceite pede 5 ou mais de cada lado.
typedef struct {
  int numero;              // numero da temporada; 0 (especiais) e ignorado
  int notaX10;             // media x10 dos episodios com nota; <=0 = sem base
  int comNota;             // quantos episodios tem nota nesta temporada
} FarolInfoTemp;

// Um amigo do feed (recomenda.h), ja com o nome de exibicao resolvido.
typedef struct {
  char nome[64];
  int  terminou;           // terminou este titulo
  int  gostou;             // reacao positiva neste titulo
} FarolInfoAmigo;

// --- as oito linhas ----------------------------------------------------------
// Cada funcao devolve quantas linhas escreveu (0 = fato ausente). `sai` tem
// espaco para o maximo indicado no comentario.

// qualidade: a melhor fonte CONFIRMADA tocavel nesta TV (toca == 1 — fonte
// desconhecida nao entra nem desempata; maior altura; desempate prefere
// DV/HDR que a TV toca e audio declarado). Ate 2 linhas: a principal e a
// de legenda. DV entra no texto SO quando tv->dv == 1; HDR so com
// tv->hdr == 1 e sai "HDR" — o campo e o indicador generico, entao
// HDR10/HLG so entrariam com evidencia do formato (ver src/vaitocar.c).
int farolinfo_qualidade(FarolInfoLinha *sai, int max,
                        const FarolInfoFonte *f, int n, const FarolInfoCaps *tv);

// continuar: sobe/cai de nota entre temporadas CONSECUTIVAS (cada uma com
// >= 5 episodios de nota e |diff| >= 0,4); vale o par mais recente.
// `faltam` = episodios que faltam, `minEp` = duracao media (0 = sem dado).
// Ate 1 linha.
int farolinfo_continuar(FarolInfoLinha *sai,
                        const FarolInfoTemp *t, int n, int faltam, int minEp);

// tempo: "Termina as 23:40." ou "Da em 2 noites de 50 min." `agora` e o
// relogio injetado; `h12` e o ajuste de relogio de 12 h; `minutos` e a
// duracao; `minNoite` e quanto a pessoa costuma ver nesta faixa de hora
// (0 = sem modelo: sempre "Termina as"). A virada do dia sai da aritmetica
// mod 1440 — nao ha "amanh" no modelo. Ate 1 linha.
int farolinfo_tempo(FarolInfoLinha *sai, const struct tm *agora,
                    int h12, int minutos, int minNoite);

// onde: "Tambem esta na Netflix." Ate 1 linha; "" / NULL = sem linha.
int farolinfo_onde(FarolInfoLinha *sai, const char *servico);

// amigos: "A Ana e o Rafa terminaram." Quem terminou tem prioridade sobre
// quem gostou; o modelo comporta ate 2 nomes. Ate 1 linha.
int farolinfo_amigos(FarolInfoLinha *sai, const FarolInfoAmigo *am, int n);

// saga: "E o 2o de 3 — voce viu 1." pos = posicao do titulo na colecao
// (1..total), vistos = quantos da colecao ja foram vistos. Colecao de 1
// nao e saga; pos desconhecida (<=0) nao vira linha. Ate 1 linha.
int farolinfo_saga(FarolInfoLinha *sai, int pos, int total, int vistos);

// rever: "Voce viu em marco de 2024 e deu 4 estrelas." O argumento do mes
// sai codificado "AAAA-MM" (a camada de texto escreve por extenso).
// `nota10` na escala do Diario (estrelas x2): meia estrela (impar) nao cabe
// no modelo "%d estrelas" e cai para a forma simples, sem estrelas.
// Ate 1 linha.
int farolinfo_rever(FarolInfoLinha *sai, int ano, int mes, int nota10);

// retomada: "Voce parou no T2E4 ha 7 semanas." O "ha quanto tempo" sai
// codificado "<n><u>" com u em {min, h, d, w, M, y} para a camada de texto
// localizar ("7w" -> "7 semanas"); quando <= 0 ou futuro cai na forma
// simples sem tempo. t <= 0 ou e <= 0 nao e episodio: sem linha.
// Ate 1 linha.
int farolinfo_retomada(FarolInfoLinha *sai, int t, int e,
                       long long agora, long long quando);

#endif
