#ifndef NUVIO_FAROLANEL_H
#define NUVIO_FAROLANEL_H
// ANEL DE EVENTOS DO FAROL — a memoria do que aconteceu (lote sense, issue 2,
// secao 2.2 do PLANO). O Resolver de hoje so le estado; o Farol precisa saber
// o que aconteceu NOS ULTIMOS 5 MINUTOS para julgar ("falhou 2 vezes seguidas",
// "parou 2 vezes em 5 min").
//
// O que entra aqui e so o que um engenheiro precisa: modulo, severidade, tipo
// do evento, dois numeros e um token curto. NUNCA URL, titulo, nome de perfil
// nem host cru — o token e saneado NA GRAVACAO (primeira das tres barreiras de
// privacidade; um produtor que passar URL grava "x").
//
// Memoria: 256 x 56 = 14 336 bytes estaticos + uma trava. Sem malloc, sem
// arquivo, sem SDL. Cabe ate na C9; o teto e a RAM livre da TV, nao o anel.
//
// Regra de fios: produtores gravam de qualquer fio (trava curta, memcpy de 56
// bytes). PROIBIDO chamar por quadro ou por pedido HTTP — so em troca de
// estado, nos pontos que ja tem printf/marco hoje.

#include <stdint.h>

#define FAROL_ANEL_N   256
#define FAROL_TOKEN_N  40     // 39 uteis + NUL

typedef enum {
  FM_PLAYER, FM_VIDEO, FM_REDE, FM_ADDON, FM_DEBRID, FM_FONTE,
  FM_AUDIO, FM_LEGENDA, FM_MEMORIA, FM_FAROL, FM_N
} FarolMod;

// Tipo fechado de evento (uint16_t no registro). Quem produz e quem consome
// casam por este enum — texto livre nao existe aqui.
enum {
  FE_NENHUM,
  FE_ABRIR,             // player: abertura de fonte (a=tentativa, b=max)
  FE_PRONTO,            // player: primeiro quadro (a=ms ate abrir, b=retomada %)
  FE_ERRO_FONTE,        // player: erro ao abrir (a=causa FS_CAUSA_*, b=http)
  FE_RECUSA_SINC,       // fontesonda: recusa na abertura (a=causa, b=http)
  FE_PARADO,            // player: VOD parado sem andar (a=idade ms, b=buf ms)
  FE_ERRO_NATIVO,       // video_*: erro nativo (a=codigo nativo, b=renderer)
  FE_AUDIO_RECUSADO,    // video_*: TV recusou a faixa de audio (a=cod. nativo)
  FE_PIPELINE_MORREU,   // video.c: pipeline "is not running" (a=posicao s)
  FE_RECONEXAO,         // nv_recon_erro: retentativa (a=tentativa, b=espera ms)
  FE_REDE_ESTADO,       // redesaude: TROCA de estado (a=offline 0/1, b=falhas)
  FE_ADDON_BUSCA,       // fim da busca de fontes (a=ms total, b=quantos falt.)
  FE_ADDON_LENTO,       // add-on mudo/lento na busca (a=ms; tok=hash do nome)
  FE_DEBRID,            // recusa/sem plano/fora de cache (a=http, b=servico)
  FE_FONTE_ESCOLHIDA,   // escolha automatica (a=pontos, b=GBx10; tok=selos)
  FE_FONTE_PROXIMA,     // fontefalha_proxima (a=tentativa, b=teto)
  FE_MEM_BAIXA,         // memlog: aviso de memoria (a=MB livres, b=MB processo)
  FE_FAROL_VEREDITO,    // o proprio Farol: veredito julgado (a=codigo)
  FE_FAROL_ACAO,        // o proprio Farol: acao executada (a=acao)
  FE_FAROL_RESULTADO,   // o proprio Farol: resultado da acao (a=acao, b=ok)
  FE_FAROL_ENVIO,       // o proprio Farol: envio do registro (a=http)
  FE_FAROL_CONSERTOU,   // adendo 2.1 C/33: conserto confirmado (a=acao)
  FE_N
};

typedef struct {
  uint32_t t_ms;                 // relogio monotonico em ms (ou o do teste)
  uint8_t  mod;                  // FarolMod
  uint8_t  sev;                  // 0 info, 1 aviso, 2 erro
  uint16_t ev;                   // FE_* acima
  int32_t  a, b;                 // numeros do evento (ms, http, codigo, MB)
  char     tok[FAROL_TOKEN_N];   // token seguro, nunca texto livre
} FarolEvento;                   // 56 bytes; _Static_assert no .c garante <= 64

// Grava um evento. Seguro entre fios. `tok` e saneado na gravacao por
// farolanel_token: fora do alfabeto vira "x". mod/ev/sev fora de faixa sao
// enquadrados (mod desconhecido vira FM_FAROL — o que ja diz "produtor errado").
void farol_ev(FarolMod mod, int ev, int sev, int32_t a, int32_t b, const char *tok);

// Copia para `dst` os eventos com t_ms >= desde_ms (diferenca com sinal, a
// prova de virada do relogio de 32 bits), do mais antigo para o mais novo.
// desde_ms 0 = sem filtro. Devolve quantos copiou (<= max).
int  farolanel_ler(FarolEvento *dst, int max, uint32_t desde_ms);

// Quantos eventos do tipo `ev` nos ultimos janela_ms terminando em `agora`
// (mesma aritmetica com sinal). janela_ms 0 = conta todos os que estao no anel.
int  farolanel_contar(int ev, uint32_t janela_ms, uint32_t agora);

// Esvazia o anel (zerando o conteudo: evento gravado nao fica na RAM).
void farolanel_zerar(void);

// Relogio do anel: fn devolve ms monotonicos (na pratica, SDL_GetTicks ou
// equivalente). NULL volta ao relogio monotono interno. Chamar UMA vez no
// arranque, antes do primeiro evento — nao e para trocar no meio do voo.
void farolanel_relogio(uint32_t (*fn)(void));

// Puro, sem trava: o que sobra de `s` como token. Saida so tem
// [a-z0-9_.:+-], no maximo 39 bytes + NUL. Maiusculas caem para minusculas.
// Qualquer caractere fora do alfabeto — "://", '/', '?', '@', '%', espaco,
// byte >= 0x80 — ou entrada vazia/nula vira "x": e a linha de defesa, nao um
// filtro por caractere.
void farolanel_token(const char *s, char out[FAROL_TOKEN_N]);

#endif
