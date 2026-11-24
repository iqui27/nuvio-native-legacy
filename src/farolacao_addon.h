// FAROL #17 — as acoes concretas de add-on e de debrid (ADENDO 2.1 C: o Farol
// conserta, nao so informa).
//
//  1. FAROL_A_ISOLAR_ADDON: poe o add-on lento ou mudo de lado por 24 h. Ele
//     segue na lista de add-ons, mas deixa de ser esperado e de ser consultado
//     nas buscas de FONTES ate o prazo vencer (addons.c le a quarentena que
//     mora em addonstats, num arquivo ao lado da latencia). Tem Desfazer: pela
//     ilha (ilhaacao_feita) e pela funcao pura farolacao_addon_desfazer.
//       - o prazo (86400 s) vence sozinho; o relogio e o de addonstats_agora(),
//         que os testes trocam por addonstats_relogio();
//       - no maximo FAAD_TETO_ISOLADOS add-ons de lado ao mesmo tempo;
//       - nunca o unico add-on de fontes que sobra.
//  2. FAROL_A_ABRIR_FONTES e FAROL_A_ABRIR_DEBRID/RECONECTAR_DEBRID: viram o
//     pedido que o app ja entende (resolver_pedido: RESOLVER_PEDIU_*). Quem
//     abre o login pelo QR e, ao voltar logado, reabre o video e o app (AGP_REPETIR,
//     addonguia_reconectar_debrid); aqui so se traduz a acao no pedido e se
//     registra no anel.
//
// Toda acao grava farol_registrar_acao e, ao terminar, farol_registrar_resultado.
// Sem estado por fio: tudo vive em addonstats, sob a trava dele.
#ifndef NV_FAROLACAO_ADDON_H
#define NV_FAROLACAO_ADDON_H

#define FAAD_ISOLA_S        86400   // 24 h
#define FAAD_TETO_ISOLADOS  3

enum {
  FAAD_OK = 0,
  FAAD_JA_ISOLADO,    // ja estava de lado: nada muda, o prazo nao renova
  FAAD_TETO,          // 3 add-ons ja estao de lado
  FAAD_UNICO,         // seria o unico add-on de fontes que sobra
  FAAD_INVALIDO,      // chave vazia/longa demais ou tabela cheia
  FAAD_NENHUM         // nenhum add-on lento a isolar (so em isolar_lento)
};

// Mesmos valores de RESOLVER_PEDIU_* (resolver.h); o teste confere. Duplicados
// aqui para o modulo nao puxar SDL so por tres inteiros.
#define FAAD_PEDIU_NADA   0
#define FAAD_PEDIU_FONTES 1
#define FAAD_PEDIU_DEBRID 2

// Poe a chave de estatistica do add-on de lado por 24 h. `fontesVivas` = quantos
// add-ons de fontes estao ligados e fora da quarentena AGORA (inclui este).
// Persiste e registra a acao e o resultado no anel.
int  farolacao_addon_isolar(const char *chave, int fontesVivas);
// Desfaz a quarentena e persiste. 1 = havia o que desfazer.
int  farolacao_addon_desfazer(const char *chave);
// 1 = de lado agora; `restamS` (opcional) recebe os segundos que faltam.
int  farolacao_addon_isolado(const char *chave, int *restamS);
// Quantos add-ons estao de lado agora.
int  farolacao_addon_isolados_n(void);

// A acao do veredito -> o pedido ao app (FAAD_PEDIU_*). Acao que nao e de
// fontes/debrid devolve FAAD_PEDIU_NADA e nao registra nada. Registra a acao.
int  farolacao_addon_pedido(int acaoFarol);
// O app terminou (logou de volta, abriu Fontes): fecha o ciclo no anel.
void farolacao_addon_resultado(int acaoFarol, int ok);

// Da UI: escolhe o add-on de fontes que mais segura a lista, aplica a politica,
// e avisa na ilha com Desfazer. Devolve FAAD_*.
int  farolacao_addon_isolar_lento(void);

#endif
