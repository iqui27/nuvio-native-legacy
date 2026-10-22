// AMIGOS DE UM TITULO: quem, entre os amigos, ASSISTIU e/ou GOSTOU de um
// titulo. Alimenta o chip de rostos no cartaz da Home, a linha "Fabi gostou ·
// Rafa e Mari assistiram" do destaque e a ilha da pagina do titulo.
//
// NAO HA INDICE POR TITULO NO SERVIDOR, e nenhuma rota nova foi criada: o
// indice sai do que socialvis.h ja tem — os eventos CRUS dos amigos
// (socialvis_bruto: assistindo agora, terminou, reacao do "O que achou?", nota
// de tracker, e o que me mandaram), virado do avesso: imdb -> ate AMT_MAX
// amigos. Crus e nao o feed limpo: o feed junta linhas e tira o "comecou". So existe o que o amigo
// PUBLICOU (o servidor ja filtra por alcance/atividade ligada); este modulo
// nunca pede nem inventa dado.
//
// CUSTO: o indice e remontado so quando socialvis_revisao() muda (no maximo a
// cada 250 ms a consulta ve isso). Quem desenha le uma copia pequena e nunca
// aloca. `amigostitulo_revisao()` sobe a cada remontagem.
#ifndef NV_AMIGOSTITULO_H
#define NV_AMIGOSTITULO_H
#include "socialvis.h"

#define AMT_MAX 8          // amigos guardados por titulo (o total real vai em `total`)
#define AMT_TITULOS_MAX SV_EVENTOS_MAX
// "Parou no E3" so quando parou DE VERDADE: sem evento novo ha mais que isto.
#define AMT_PAROU_S (21LL * 86400)

typedef struct {
  char id[96], nome[64], avatar[256];
  int  gostou;             // reacao mais nova positiva; sem reacao, nota de tracker >= 70
  int  viu;                // terminou, esta vendo, reagiu ou avaliou
  int  agora;              // assistindo agora
  int  temporada, episodio;// ate onde viu; 0 = nao se sabe / filme
  // O QUE E UTIL NA PAGINA DO TITULO (dono, 06/10/2026: "so aparecer que viu
  // nao muda muito, tem que dar informacoes uteis"):
  int  recomendou;         // me mandou este titulo (SV_MANDOU)
  char recTexto[72];       // a frase da recomendacao ("" = nenhuma)
  int  reacao;             // SV_REAC_* mais nova (SV_REAC_NADA = nao reagiu)
  int  nota;               // nota de tracker 0..100; 0 = nao avaliou
  int  terminou;           // filme terminado / ao menos um episodio terminado
  int  serie;
  long long quando;        // o evento mais novo dele neste titulo (fora o que me mandou)
  char resposta[64];       // o que respondeu a uma rec MINHA deste titulo ("" = nada)
  int  gosto;              // % de gosto parecido comigo; -1 = nao se sabe
} AmigoTit;

typedef struct {
  char imdb[24];
  int  n;                  // quantos em a[] (<= AMT_MAX)
  int  total;              // quantos amigos ao todo
  int  nGostou, nViu;      // totais: quem gostou / quem so assistiu
  int  nRecomendou;        // quantos me mandaram o titulo
  int  nOpiniao;           // quantos reagiram ou deram nota
  int  nNaoGostou, nMeio;  // das opinioes: nao gostou / mais ou menos
  AmigoTit a[AMT_MAX];     // recomendou > opinou (gostou antes) > progresso > so viu; o mais novo antes
} AmigosTitulo;

// Monta o indice a partir de eventos (mais novo primeiro). Puro: nao le
// socialvis; e o que os testes usam. Devolve quantos titulos entraram.
int  amigostitulo_montar(const SvEvento *ev, int n);
// Por quadro, barato: reconfere o feed e remonta so quando ele mudou.
void amigostitulo_atualizar(void);
unsigned amigostitulo_revisao(void);
// 1 = ha amigos neste titulo (copia em *saida, que pode ser NULL).
int  amigostitulo_obter(const char *imdb, AmigosTitulo *saida);
// Texto da linha do destaque: "Fabi gostou · Rafa e Mari assistiram". "" sem amigos.
void amigostitulo_linha_destaque(const AmigosTitulo *t, char *dst, size_t tam);
// Texto da ilha: "Mari gostou · Fabi viu até o E8" (ate 2 amigos, "+N" no resto).
void amigostitulo_linha_ilha(const AmigosTitulo *t, char *dst, size_t tam);
// A ILHA DA PAGINA DO TITULO, em duas linhas, na prioridade do dono:
// recomendou para voce > reacoes/notas ("4 amigos: 3 gostaram") > progresso
// numa serie ("Rafa: T2E5", "Rafa terminou a série", "Rafa parou no T1E3")
// > so assistiu. `l2` leva o que ficou de fora (e o "gosto parecido").
// `ultT/ultE` = ultimo episodio da serie (0 = nao se sabe: nunca diz
// "terminou a série"); `agora` = epoch s.
void amigostitulo_resumo(const AmigosTitulo *t, int ultT, int ultE, long long agora,
                         char *l1, size_t t1, char *l2, size_t t2);
// UMA LINHA DA LISTA (OK na ilha): `st` = opiniao e progresso ("Gostou · Viu
// até T2E5"); `extra` = recomendacao, resposta e gosto parecido. "" = nada.
void amigostitulo_frase_amigo(const AmigoTit *a, int ultT, int ultE, long long agora,
                              char *st, size_t tst, char *extra, size_t textra);
// Primeiro nome do amigo (ate o primeiro espaco), para caber nas frases.
void amigostitulo_primeiro_nome(const char *nome, char *dst, size_t tam);
#endif
