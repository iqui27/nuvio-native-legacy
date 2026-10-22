// A TELA DO DOLBY VISION EM MKV (webOS, #203).
//
// O caminho do DV em MKV troca o player da TV pelo nosso demux: a TV abre o
// arquivo em HDR10, o app le o cabecalho, decide DV e reabre pelo caminho
// proprio. Sem cobertura a pessoa via tudo isso — o HDR10 com som, o "Abrindo
// fonte" duas vezes e o painel de pausa no preroll (dono, C9, 08/10). Esta tela
// cobre a abertura inteira: entra quando o Play cai numa fonte que vai tentar
// DV e so sai com o DV tocando, ou com o motivo honesto de nao ter DV.
//
// MAQUINA PURA. dvt_* nao le relogio nem backend: recebe os sinais do quadro
// (DvtelaSinais, montados pelo player a partir de video_dv_fase) e o `agora`.
// E isto que os testes exercitam (tests/dvtela.c). A instancia unica do app,
// com o log e a animacao, sao as funcoes dvtela_*; o desenho mora em
// dvtelaui.c.
#ifndef NV_DVTELA_H
#define NV_DVTELA_H
#include <SDL2/SDL.h>

// Os passos, na ordem em que acontecem. Cada um so acende com o sinal real.
enum {
  DVT_PASSO_LER = 0,   // lendo o cabecalho do arquivo
  DVT_PASSO_ACHOU,     // Dolby Vision perfil N no arquivo
  DVT_PASSO_AUDIO,     // a faixa de audio foi trocada (so quando acontece)
  DVT_PASSO_ABRIR,     // o nosso demux abrindo a fonte (a parte lenta)
  DVT_PASSO_IMAGEM,    // a TV aceitou o fluxo; enchendo antes do 1o quadro
  DVT_PASSOS
};

// Por que a tela saiu.
enum {
  DVT_SAIDA_NADA = 0,
  DVT_SAIDA_DV,        // DV confirmado pelo pipeline e tocando
  DVT_SAIDA_RECUSA,    // o arquivo fica em HDR10 (motivo em `recusa`)
  DVT_SAIDA_VOLTAR,    // a pessoa voltou (Voltar)
  DVT_SAIDA_HDR10,     // a pessoa escolheu assistir em HDR10 agora
  DVT_SAIDA_FONTE      // a fonte mudou para uma sem DV, ou falhou
};

// O que uma tecla fez com a tela de pe.
enum {
  DVT_EV_NADA = 0,     // tela fora: a tecla segue o caminho de sempre
  DVT_EV_ENGOLIU,      // a tela ficou com a tecla
  DVT_EV_HDR10,        // OK no botao: o player chama video_dv_recusar
  DVT_EV_VOLTAR        // Voltar: o player sai como sempre
};

// O que o backend disse NESTE quadro (video_dv_fase, VideoDvFase).
typedef struct {
  int sessao;
  int sondado, perfil;
  int audioTrocado;
  char audioDe[16], audioPara[16];
  int caminho, fonteAberta, carregado, dvConfirmado, tocando;
  int recusa;          // VIDEO_DV_NAO_*
  int falhou;
} DvtelaSinais;

typedef struct {
  int ativa;           // a tela tem a imagem e as teclas
  int passo;           // DVT_PASSO_* em curso
  int perfil;
  int audioTrocado;
  char audioDe[16], audioPara[16];
  int saida, recusa;   // por que saiu (DVT_SAIDA_*, VIDEO_DV_NAO_*)
  int dica;            // nada mudou ha DVT_DICA_MS: a dica calma
  unsigned sinaisVistos;   // os sinais do ultimo quadro (o que conta como mudanca)
  Uint32 entrouEm, mudouEm, saiuEm;
} DvtelaEstado;

// Sem mudanca de passo por este tempo, a dica calma (nao e erro).
#define DVT_DICA_MS 60000u

void dvt_entrar(DvtelaEstado *e, Uint32 agora);
// Um quadro. Devolve DVT_SAIDA_* quando a tela saiu NESTE quadro, 0 senao.
int  dvt_passo(DvtelaEstado *e, const DvtelaSinais *s, Uint32 agora);
void dvt_sair(DvtelaEstado *e, int saida, Uint32 agora);

// --- a instancia do app ------------------------------------------------------
void dvtela_entrar(Uint32 agora);
int  dvtela_ativa(void);
// Ainda desenhando (ativa, ou esvaindo sobre o filme): o player nao mostra a
// pausa nem o cartao de abertura enquanto isto for 1.
int  dvtela_visivel(void);
float dvtela_alfa(void);
float dvtela_dica_alfa(void);
// A arte do titulo (o fundo desfocado da tela); "" ou NULL = so o escuro.
void dvtela_definir_arte(const char *url);
// O quadro: passo da maquina, mola da entrada/saida e o log. Devolve
// DVT_SAIDA_* no quadro em que a tela saiu.
int  dvtela_atualizar(const DvtelaSinais *s, float dt, Uint32 agora);
// Tecla com a tela de pe (DVT_EV_*).
int  dvtela_evento(const SDL_Event *e, Uint32 agora);
void dvtela_sair(int saida, Uint32 agora);
const DvtelaEstado *dvtela_estado(void);
int  dvtela_foco_botao(void);
// Linha do log "[dvtela] ..." (sem a URL; numero e nome do passo).
const char *dvtela_nome_passo(int passo);
const char *dvtela_nome_saida(int saida);

// Desenho (dvtelaui.c), por cima de tudo do player e da ilha.
void dvtela_desenhar(Uint32 agora);

// A previa das novidades (novidades_cartao.c): o mesmo cartao, em miniatura,
// com o estado que o chamador monta (dvt_entrar / dvt_passo) e molas proprias.
// Nao mexe na instancia do player. dt em segundos, agora em ms.
void  dvtela_previa_zerar(void);
void  dvtela_previa_avancar(const DvtelaEstado *e, float dt);
void  dvtela_previa_desenhar(float x, float y, float s, const DvtelaEstado *e, Uint32 agora, float a);
float dvtela_previa_largura(void);
float dvtela_previa_altura(void);

#ifdef NV_SHOT_HOOKS
void dvtela_shot_relogio(Uint32 ms);   // capturas: o brilho do passo parado num instante
#endif

// Testes do ponteiro (#99): 1 com o anteparo e o botao registrados.
int dvtela_teste_ponteiro(void);
#endif
