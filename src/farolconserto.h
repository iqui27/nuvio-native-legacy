#ifndef NV_FAROLCONSERTO_H
#define NV_FAROLCONSERTO_H
// CONFIRMACAO DO CONSERTO DO FAROL (ADENDO 2.1 C, item 33).
//
// O Farol aplicou uma acao de conserto (reabrir, trocar de fonte, converter o
// audio...). Esta maquina confirma se o video VOLTOU a tocar de verdade, lendo
// o anel de eventos (farolanel) a partir da fronteira da acao:
//
//   - sucesso  : FE_PRONTO e, depois dele, FAROL_CONSERTO_ANDANDO_MS (5 s) de
//                video andando — sem novo FE_ERRO_FONTE / FE_ERRO_NATIVO /
//                FE_PIPELINE_MORREU nem FE_PARADO prolongado — tudo DENTRO do
//                prazo total de FAROL_CONSERTO_PRAZO_MS (20 s) contado desde
//                farol_conserto_iniciar. Registra FE_FAROL_CONSERTOU.
//   - falha    : novo erro, ou prazo vencido sem a confirmacao completa.
//                Registra o resultado da acao (ok=0). Com `acaoAlt` valida e
//                ainda dentro do teto, manda executar a alternativa e ja REARMA
//                a observacao dela; senao, ESCALA (oferta de registro tecnico).
//   - teto     : no maximo FAROL_CONSERTO_ACOES_MAX (2) acoes por falha. O
//                teto vive na estrutura, nao na chamada: chamar iniciar de novo
//                na mesma falha, sem cancelar, e RECUSADO ao estourar o teto.
//
// MODULO PURO: sem SDL, sem malloc, sem log, SEM ESTADO ESTATICO (tudo na
// FarolConserto do chamador), relogio injetado em todas as chamadas. So fala
// com o mundo por farolanel (leitura) e farol_registrar_* (anel). NAO e
// thread-safe: um dono (o fio principal) por estrutura.
//
// FRONTEIRA: capture `farolanel_seq()` ANTES de aplicar a acao e passe-a em
// iniciar — os eventos que a propria acao produz (FE_ABRIR, FE_PRONTO de uma
// reabertura) tem de ser vistos. Passar a sequencia lida depois da acao perde
// o FE_PRONTO rapido.
//
// Modo de uso:
//   FarolConserto c = {0};                      // ou farol_conserto_cancelar
//   uint64_t f = farolanel_seq();
//   aplicar(acao);
//   farol_conserto_iniciar(&c, acao, acaoAlt, agoraMs, f);
//   ... a cada quadro/tick:
//   switch (farol_conserto_passo(&c, agoraMs)) {
//     case FCP_CONSERTOU:   mostrar(farol_conserto_texto(&c)); break;
//     case FCP_EXECUTAR_ALT: aplicar(farol_conserto_acao(&c)); break; // ja armada
//     case FCP_ESCALAR:     oferecer_registro_tecnico(); break;
//   }
// Em FCP_EXECUTAR_ALT NAO chame iniciar de novo: a observacao da 2a acao ja
// esta armada (fronteira = fim do anel neste instante, prazo desde `agoraMs`).
//
// Voltar / trocar de titulo ou perfil: farol_conserto_cancelar — descarta a
// observacao sem registrar nada e sem escalar depois.

#include <stdint.h>

#define FAROL_CONSERTO_PRAZO_MS   20000   // prazo total por acao
#define FAROL_CONSERTO_ANDANDO_MS  5000   // video andando apos FE_PRONTO
#define FAROL_CONSERTO_PARADO_MS   3000   // FE_PARADO a>=isto e "prolongado"
#define FAROL_CONSERTO_ACOES_MAX      2   // teto de acoes por falha

typedef enum {
  FCS_OCIOSO = 0,   // nada observando (zerado, cancelado ou recem-consertado)
  FCS_OBSERVANDO,   // acao aplicada, esperando a confirmacao
  FCS_CONSERTOU,    // confirmado (terminal)
  FCS_ESCALOU       // desistiu; oferta de registro (terminal ate cancelar)
} FarolConsertoEstado;

typedef enum {
  FCP_NADA = 0,       // nada a fazer (observando, ocioso ou ja reportado)
  FCP_CONSERTOU,      // voltou a tocar: mostrar farol_conserto_texto()
  FCP_EXECUTAR_ALT,   // aplicar farol_conserto_acao() agora; observacao armada
  FCP_ESCALAR         // nao voltou: oferecer o registro tecnico
} FarolConsertoPasso;

// Zerada (= {0}) e um estado valido (FCS_OCIOSO). Campos so para leitura via
// farol_conserto_*; nao mexa neles de fora.
typedef struct {
  int      estado;       // FarolConsertoEstado
  int      falha;        // 1 = ha uma falha em curso (o teto vale ate cancelar)
  int      acoes;        // acoes consumidas nesta falha (0..FAROL_CONSERTO_ACOES_MAX)
  int      acao;         // FAROL_A_* sob observacao (ou a ultima)
  int      acaoAlt;      // FAROL_A_* alternativa ainda disponivel (0 = nenhuma)
  uint32_t inicioMs;     // relogio de iniciar (ou do rearme da alternativa)
  uint64_t proxSeq;      // proximo evento do anel a processar
  int      pronto;       // 1 = viu FE_PRONTO desde o inicio
  uint32_t prontoMs;     // t_ms do FE_PRONTO mais recente
} FarolConserto;

// Arma a observacao de `acao` (FAROL_A_*, valida e != ENVIAR_REGISTRO) aplicada
// em `agoraMs`, com eventos do anel a partir de `fronteiraSeq`. `acaoAlt`
// invalida, igual a `acao` ou ENVIAR_REGISTRO = sem alternativa.
// Devolve 1 se armou; 0 se recusou (argumento invalido, ou o teto de
// FAROL_CONSERTO_ACOES_MAX acoes da falha ja foi gasto): NAO aplique a acao.
// Uma falha nova so comeca com a estrutura ociosa/consertada/cancelada; fora
// disso a chamada conta como a proxima acao da MESMA falha.
int farol_conserto_iniciar(FarolConserto *c, int acao, int acaoAlt,
                           uint32_t agoraMs, uint64_t fronteiraSeq);

// Avanca a maquina com os eventos novos do anel e o relogio `agoraMs`. Cada
// resultado diferente de FCP_NADA sai UMA vez. Barato (uma copia do anel).
int  farol_conserto_passo(FarolConserto *c, uint32_t agoraMs);

// Descarta a observacao e a falha em curso (Voltar, outro titulo/perfil). Nao
// registra nada no anel e nao deixa escalada tardia.
void farol_conserto_cancelar(FarolConserto *c);

int  farol_conserto_estado(const FarolConserto *c);   // FarolConsertoEstado
int  farol_conserto_acao(const FarolConserto *c);     // acao em curso/ultima
int  farol_conserto_acoes(const FarolConserto *c);    // acoes gastas na falha

// Texto da fala: FT_VOLTOU_A_TOCAR ao consertar, FT_OBSERVANDO_VIDEO enquanto
// observa, NULL nos demais. Literal de faroltexto.h (quem desenha aplica i18n).
const char *farol_conserto_texto(const FarolConserto *c);

#endif
