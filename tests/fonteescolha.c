// #364: preferencia nao e escolha manual; sonda apos recusa sem caminho/query.
#include <stdio.h>
#include <string.h>
#include "fonteescolha.h"
#include "fontesonda.h"
static int falhas;
#define OK(c) do { if (!(c)) { printf("FALHOU %d: %s\n", __LINE__, #c); falhas++; } } while (0)
int main(void) {
  char l[400];
  // preferencia que falha -> proxima fonte; escolha manual -> nenhuma troca
  OK(fonteescolha_cai_para_proxima(3, 3, 1, 0));
  OK(!fonteescolha_cai_para_proxima(2, 3, 1, 0));   // outra linha: manual de verdade
  OK(!fonteescolha_cai_para_proxima(0, -1, 1, 0));  // nada lembrado
  OK(!fonteescolha_cai_para_proxima(3, 3, 0, 0));   // torrent: sem url
  OK(!fonteescolha_cai_para_proxima(3, 3, 1, 1));   // canal
  // erro de conexao do .tpk (log 9RXVST)
  OK(fontesonda_erro_conexao("[host] erro ConnectionFailed"));
  OK(fontesonda_erro_conexao("player error 0xfe6c0026"));
  OK(!fontesonda_erro_conexao(""));
  OK(!fontesonda_erro_conexao("decoder: unsupported codec"));
  // a linha: so host, nunca caminho/query/token
  fontesonda_linha(l, sizeof l,
    "https://user:pw@cdn.torbox.app:8443/v1/dl/SEGREDO123tokenABCDEFGHIJKLMNOP/Filme.mkv?token=CHAVE999&x=1#frag",
    206, 1, "video/x-matroska", 812);
  printf("%s\n", l);
  OK(strstr(l, "status=206 redirecionamentos=1 host_final=cdn.torbox.app tipo=video/x-matroska tempo=812"));
  OK(!strstr(l, "SEGREDO") && !strstr(l, "CHAVE") && !strstr(l, "Filme") && !strstr(l, "/v1") &&
     !strstr(l, "?") && !strstr(l, "pw") && !strstr(l, "8443") && !strstr(l, "frag"));
  OK(strstr(l, "responde"));
  fontesonda_linha(l, sizeof l, "http://a.example/x/segredo", 403, 0, "", 5);
  OK(strstr(l, "status=403") && strstr(l, "host_final=a.example") && !strstr(l, "segredo"));
  OK(!strstr(l, "responde"));
  fontesonda_linha(l, sizeof l, "https://h.example/p", 0, -1, NULL, 3000);
  OK(strstr(l, "status=0 redirecionamentos=?") && strstr(l, "sem resposta"));
  if (!falhas) puts("ok fonteescolha");
  return falhas != 0;
}
