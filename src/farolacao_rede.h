// FAROL — ACOES DE REDE, CACHE, BUFFER E HDR (issue #34, PLANO 2.4 / 5 item 34,
// ADENDO 2.1 C: o Farol conserta, nao so informa).
//
//   FAROL_A_RECONECTAR_REDE  zera o cache de DNS e as conexoes ociosas do
//                            cliente (rede_zerar_cache) e refaz um teste curto
//                            de conectividade. Nao interrompe transferencia.
//   FAROL_A_LIMPAR_CACHE     apaga SO o cache descartavel (imagens hash.ext da
//                            pasta de cache e o cache de listas de fontes em
//                            memoria). NUNCA toca ajustes, contas, sessoes,
//                            tokens, Diario, perfis nem fontefalha.txt.
//   FAROL_A_BUFFER_MAIOR     buffer maior SO nesta reproducao (override em
//                            memoria em ajustes.c; o ajuste da pessoa nao
//                            muda) e pede a reabertura na posicao atual.
//   FAROL_A_DESLIGAR_HDR     DV/HDR desligado SO nesta reproducao (override em
//                            memoria) e reabertura na posicao atual. Com
//                            persistir=1 ("sempre nesta TV") grava o ajuste e
//                            mostra a acao feita na ilha com Desfazer, que
//                            devolve o valor anterior.
//
// Os overrides temporarios acabam em farolacao_rede_encerrar(), chamado por
// player_encerrar() (fim da reproducao; a troca de fonte da MESMA reproducao
// usa player_encerrar_fonte e os conserva).
//
// Toda execucao registra farol_registrar_acao e farol_registrar_resultado.
//
// FIO: o executor roda no fio principal. O teste de conectividade corre num fio
// proprio (no maximo ~2 s de rede); quem registra o resultado de RECONECTAR_REDE
// e esse fio. Estado: uma trava folha e atomicos; nenhuma outra funcao e
// chamada com a trava tomada.
#ifndef NV_FAROLACAO_REDE_H
#define NV_FAROLACAO_REDE_H

// 1 se `acao` e uma das quatro desta unidade.
int farolacao_rede_acao_valida(int acao);

// Executa a acao. `persistir` so vale em DESLIGAR_HDR. Devolve 1 se a acao foi
// aplicada (RECONECTAR_REDE: 1 = o teste foi disparado; o resultado vai ao anel
// quando ele termina); 0 se nao havia o que fazer (sem efeito alem do registro).
int farolacao_rede_executar(int acao, int persistir);

// Fim da reproducao: esquece buffer maior, HDR temporario e reabertura pedida.
void farolacao_rede_encerrar(void);

// app.c: 1 uma unica vez quando ha reabertura pedida; *posSeg = posicao do
// video quando foi pedida (< 0 = desconhecida).
int farolacao_rede_reabertura_pedida(double *posSeg);

// Desfaz o "desligar o HDR sempre nesta TV" mais recente (o que a ilha
// oferece): devolve o ajuste anterior e solta o override. 1 se havia o que
// desfazer.
int farolacao_rede_desfazer_hdr(void);

// Estado dos overrides desta reproducao (testes e diagnostico).
int farolacao_rede_buffer_maior_ativo(void);
int farolacao_rede_hdr_desligado_ativo(void);

// Espera o teste de conectividade em curso (testes); 1 = terminou. `ms` maximo.
int farolacao_rede_aguardar(unsigned ms);

// Teste de conectividade: 1 chegou, 0 nao chegou, -1 nao testavel (sem URL de
// saude nesta build). NULL restaura o teste real. So testes.
typedef int (*FarolRedeTeste)(void);
void farolacao_rede_teste_conectividade(FarolRedeTeste f);

#endif
