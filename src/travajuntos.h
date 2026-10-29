// TRAVA JUNTOS: o episodio que um membro termina conta para todos (plano
// social-pessoal, secao 5). Ouve o Diario: quando o PLAYER grava um episodio de
// uma serie com trava JUNTOS, cada outro membro que ainda nao o viu ganha a
// entrada no Diario (origem manual, JUNTOS) e o visto na conta, e a ilha diz
// "Marcado para X" com Desfazer. Sem trava Juntos, nao faz nada.
#ifndef NV_TRAVAJUNTOS_H
#define NV_TRAVAJUNTOS_H

// Registra o ouvinte no Diario (uma vez; depois de diario_iniciar/trava_iniciar).
void travajuntos_iniciar(void);

#endif
