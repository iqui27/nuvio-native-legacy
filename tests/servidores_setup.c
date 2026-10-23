#include "srvsetup.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
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
  assert(!em_connect_ler_conta("{\"ConnectAccessToken\":\"fixture\"}", token, sizeof token, user, sizeof user));
  assert(!token[0] && !user[0]);
  puts("Server setup: discovery URL validation and Connect credentials: PASS");
  return 0;
}
