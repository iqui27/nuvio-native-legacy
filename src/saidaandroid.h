#ifndef NV_SAIDAANDROID_H
#define NV_SAIDAANDROID_H
// POR QUE O PROCESSO ANTERIOR MORREU, lido da linha que o NuvioActivity monta
// com ApplicationExitInfo (NUVIO_SAIDA_ANTERIOR, "motivo=<nome>(<codigo>) ...").
//
// A marca de sessao viva (avisos.c) so diz que a sessao nao se despediu. No
// Android isso inclui o que nao e queda: a atualizacao do proprio app ("stop
// ... due to installPackageLI", 86 sessoes da 2.0.1 no D1), forcar parada,
// arrastar o app para fora dos recentes ("SwipeUpClean") e troca de permissao.
// Contadas como queda, elas acendiam o aviso "o app fechou sozinho" e, abaixo
// de 60 s, entravam na conta de quedas rapidas do modo seguro (seguro.h).
//
// Devolve 1 quando o codigo e um desses motivos; 0 para queda, ANR, falta de
// memoria, sinal (o SIGKILL do sistema e ambiguo), motivo desconhecido ou linha
// ausente/ilegivel — na duvida, continua contando como queda.
int saida_android_nao_foi_queda(const char *saida);
#endif
