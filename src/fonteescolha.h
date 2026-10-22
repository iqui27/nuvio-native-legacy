#ifndef NUVIO_FONTEESCOLHA_H
#define NUVIO_FONTEESCOLHA_H
// ORIGEM DA FONTE ESCOLHIDA NA FOLHA (#364, 2.0.4).
//
// A folha abre com o foco na fonte que o bingeGroup/fontepref lembrou (a marca
// "Sua escolha anterior", stream_preferir). OK nessa linha NAO e uma decisao
// nova da pessoa: e a preferencia sendo aplicada. Tratar como "escolha manual"
// (sem fallback) deixava a pessoa na tela "Nao foi possivel abrir a fonte" com
// a lista inteira ainda por tentar (Samsung S95C, registro 60527).
//
// PREFERENCIA = a linha escolhida e a que a folha marcou como lembrada. Vale o
// mesmo caminho do automatico (fallback para a proxima). Qualquer outra linha
// e escolha manual de verdade: mensagem e nenhuma troca por tras da tela.
// So fonte direta de VOD: torrent (sem url) e canal nao entram.
static inline int fonteescolha_e_preferencia(int escolhida, int preferida) {
  return escolhida >= 0 && escolhida == preferida;
}
static inline int fonteescolha_cai_para_proxima(int escolhida, int preferida,
                                                int temUrl, int canal) {
  return temUrl && !canal && fonteescolha_e_preferencia(escolhida, preferida);
}
#endif
