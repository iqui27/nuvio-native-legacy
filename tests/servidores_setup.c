#include "srvsetup.h"
#include "srvprefs.h"
#include "listas.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  assert(srv_biblioteca_selecionada("", "123"));
  assert(srv_biblioteca_selecionada("123\n456", "123"));
  assert(srv_biblioteca_selecionada("123\n456", "456"));
  assert(!srv_biblioteca_selecionada("123\n456", "12"));
  assert(!srv_biblioteca_selecionada("123\n456", "789"));
  CatItem folder = {0}; char key[96];
  strcpy(folder.imdb, "px:12345678.42"); strcpy(folder.tipo, "group");
  assert(lst_servidor_chave(&folder, key, sizeof key) && !strcmp(key, "plex_group_42"));
  strcpy(folder.imdb, "em:12345678.42"); strcpy(folder.tipo, "list");
  assert(lst_servidor_chave(&folder, key, sizeof key) && !strcmp(key, "emby_list_42"));
  strcpy(folder.imdb, "jf:12345678.42"); strcpy(folder.tipo, "group");
  assert(lst_servidor_chave(&folder, key, sizeof key) && !strcmp(key, "jellyfin_42"));
  strcat(folder.imdb, ":1:2"); assert(!lst_servidor_chave(&folder, key, sizeof key));
  SrvRede s = {0};
  assert(srv_ler_descoberta(1, "{\"Name\":\"Living room\",\"Address\":\"http://192.168.1.2:8096\",\"Id\":\"abc\"}", &s));
  assert(!strcmp(s.nome, "Living room") && !strcmp(s.url, "http://192.168.1.2:8096"));
  assert(!srv_ler_descoberta(1, "{\"Address\":\"file:///etc/passwd\"}", &s));
  assert(!srv_ler_descoberta(1, "{\"Address\":\"http://user:secret@host\"}", &s));
  assert(srv_ler_descoberta(0, "HTTP/1.0 200 OK\r\nName: Home\r\nPort: 32400\r\nResource-Identifier: abc\r\n", &s));
  assert(!strcmp(s.nome, "Home"));
  char token[128] = "previous", user[64] = "previous";
  assert(em_connect_ler_conta("{\"ConnectAccessToken\":\"fixture\",\"ConnectUserId\":\"123\"}", token, sizeof token, user, sizeof user));
  assert(!strcmp(token, "fixture") && !strcmp(user, "123"));
  assert(em_connect_ler_conta("{\"AccessToken\":\"fixture\",\"UserId\":\"123\"}", token, sizeof token, user, sizeof user));
  assert(em_connect_ler_conta("{\"AccessToken\":\"fixture\",\"User\":{\"Id\":\"123\"}}", token, sizeof token, user, sizeof user));
  assert(!em_connect_ler_conta("{\"ConnectAccessToken\":\"fixture\"}", token, sizeof token, user, sizeof user));
  assert(!token[0] && !user[0]);
  puts("Server setup: discovery URL validation and Connect credentials: PASS");
  return 0;
}
