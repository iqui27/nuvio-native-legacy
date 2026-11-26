// Nuvio Casa, regra pura (src/casa.c): descoberta, escolha do ajudante, rotas
// do servidor e os filtros de quem entra. Sem rede.
#include "casa.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define IP(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))
static const char *TOK = "0123456789abcdef01234567";
static const char *HASH = "dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c";

static void redes(void) {
  assert(casa_ip_privado(IP(192, 168, 1, 20)) && casa_ip_privado(IP(10, 0, 0, 1)));
  assert(casa_ip_privado(IP(172, 16, 0, 1)) && casa_ip_privado(IP(172, 31, 255, 255)));
  assert(casa_ip_privado(IP(127, 0, 0, 1)) && casa_ip_privado(IP(169, 254, 3, 4)));
  assert(!casa_ip_privado(IP(172, 32, 0, 1)) && !casa_ip_privado(IP(8, 8, 8, 8)));
  assert(!casa_ip_privado(IP(192, 169, 1, 1)) && !casa_ip_privado(0));
}

static void descoberta(void) {
  char m[200];
  CasaAjudante a;
  int n;
  assert(casa_e_pergunta(CASA_PERGUNTA, (int)strlen(CASA_PERGUNTA)));
  assert(casa_e_pergunta("NUVIO-CASA/1 ?\r\n", 16));
  assert(!casa_e_pergunta("NUVIO-CASA/2 ?", 14) && !casa_e_pergunta("", 0) && !casa_e_pergunta(NULL, 4));
  assert(!casa_e_pergunta("M-SEARCH * HTTP/1.1", 19));

  n = casa_montar_resposta(47471, TOK, "Sala de estar", 1, m, sizeof m);
  assert(n > 0 && (int)strlen(m) == n);
  memset(&a, 0, sizeof a);
  assert(casa_ler_resposta(m, n, &a));
  assert(a.porta == 47471 && !strcmp(a.token, TOK) && a.p2p == 1 && !strcmp(a.nome, "Sala de estar"));
  // Sem a capacidade, o ajudante existe mas nao serve para P2P.
  n = casa_montar_resposta(47471, TOK, "x", 0, m, sizeof m);
  assert(n > 0 && casa_ler_resposta(m, n, &a) && a.p2p == 0);
  // Nome com controle ou aspas nao vira log nem tela: e limpo.
  n = casa_montar_resposta(47471, TOK, "a\"b\n<c>", 1, m, sizeof m);
  assert(n > 0 && casa_ler_resposta(m, n, &a) && !strchr(a.nome, '"') && !strchr(a.nome, '\n') && !strchr(a.nome, '<'));
  // Token e porta invalidos nao montam nem sao lidos.
  assert(!casa_montar_resposta(0, TOK, "x", 1, m, sizeof m));
  assert(!casa_montar_resposta(47471, "curto", "x", 1, m, sizeof m));
  assert(!casa_montar_resposta(47471, "0123456789abcdef0123456/", "x", 1, m, sizeof m));
  assert(!casa_montar_resposta(47471, TOK, "x", 1, m, 10));
  {
    static const char *RUINS[] = {
      "NUVIO-CASA/1 ! porta=0 token=0123456789abcdef01234567 cap=p2p nome=x",
      "NUVIO-CASA/1 ! porta=70000 token=0123456789abcdef01234567 cap=p2p nome=x",
      "NUVIO-CASA/1 ! porta=80 token=../../etc cap=p2p nome=x",
      "NUVIO-CASA/1 ! porta=80 cap=p2p nome=x",
      "NUVIO-CASA/1 ? porta=80 token=0123456789abcdef01234567 cap=p2p nome=x",
      "HTTP/1.1 200 OK",
      "",
    };
    for (unsigned i = 0; i < sizeof RUINS / sizeof *RUINS; i++)
      assert(!casa_ler_resposta(RUINS[i], (int)strlen(RUINS[i]), &a));
    assert(!casa_ler_resposta(NULL, 3, &a));
  }
}

static void escolha(void) {
  CasaAjudante l[4];
  char b[120];
  memset(l, 0, sizeof l);
  assert(casa_escolher(NULL, 0, 100) < 0 && casa_escolher(l, 0, 100) < 0);
  l[0].ip = IP(192, 168, 1, 30); l[0].porta = 47471; l[0].p2p = 1; l[0].visto = 1000; strcpy(l[0].token, TOK);
  l[1] = l[0]; l[1].ip = IP(192, 168, 1, 20);
  l[2] = l[0]; l[2].ip = IP(8, 8, 8, 8); l[2].visto = 1050;       // fora da rede: nunca
  l[3] = l[0]; l[3].ip = IP(192, 168, 1, 5); l[3].p2p = 0; l[3].visto = 1050;   // nao faz P2P
  assert(casa_escolher(l, 4, 1060) == 1);                    // empate: o menor IP (estavel)
  l[0].visto = 1040;
  assert(casa_escolher(l, 4, 1060) == 0);                    // o visto por ultimo
  assert(casa_escolher(l, 4, 1000 + CASA_VALIDADE_S) == 0);   // o [1] venceu
  assert(casa_escolher(l, 4, 1040 + CASA_VALIDADE_S) < 0);    // todos vencidos
  assert(casa_base(&l[0], b, sizeof b) && !strcmp(b, "http://192.168.1.30:47471/0123456789abcdef01234567"));
  assert(!casa_base(&l[2], b, sizeof b) && !casa_base(&l[0], b, 12) && !casa_base(NULL, b, sizeof b));
}

static void rotas(void) {
  CasaPedido p;
  char alvo[200];
  snprintf(alvo, sizeof alvo, "/%s/settings", TOK);
  assert(casa_rota("GET", alvo, TOK, &p) == CASA_ROTA_SETTINGS);
  assert(casa_rota("POST", alvo, TOK, &p) == CASA_ROTA_NADA);
  snprintf(alvo, sizeof alvo, "/%s/DD8255ECDC7CA55FB0BBF81323D87062DB1F6D1C/create?idx=3&s=1&e=2", TOK);
  assert(casa_rota("POST", alvo, TOK, &p) == CASA_ROTA_CRIAR);
  assert(!strcmp(p.hash, HASH) && p.idx == 3 && p.temporada == 1 && p.episodio == 2);
  assert(casa_rota("GET", alvo, TOK, &p) == CASA_ROTA_NADA);
  snprintf(alvo, sizeof alvo, "/%s/%s/create", TOK, HASH);
  assert(casa_rota("POST", alvo, TOK, &p) == CASA_ROTA_CRIAR && p.idx == -1 && !p.temporada && !p.episodio);
  snprintf(alvo, sizeof alvo, "/%s/%s/create?idx=-1&s=abc&e=999999999", TOK, HASH);
  assert(casa_rota("POST", alvo, TOK, &p) == CASA_ROTA_CRIAR && p.idx == -1 && !p.temporada);
  snprintf(alvo, sizeof alvo, "/%s/%s/0", TOK, HASH);
  assert(casa_rota("GET", alvo, TOK, &p) == CASA_ROTA_ARQUIVO && p.idx == 0 && !strcmp(p.hash, HASH));
  assert(casa_rota("HEAD", alvo, TOK, &p) == CASA_ROTA_ARQUIVO);
  assert(casa_rota("DELETE", alvo, TOK, &p) == CASA_ROTA_NADA && casa_rota("POST", alvo, TOK, &p) == CASA_ROTA_NADA);
  // Token errado, sem token, travessia, URL absoluta (proxy aberto), hash curto.
  snprintf(alvo, sizeof alvo, "/%s/%s/0", "ffffffffffffffffffffffff", HASH);
  assert(casa_rota("GET", alvo, TOK, &p) == CASA_ROTA_NADA);
  assert(casa_rota("GET", "/settings", TOK, &p) == CASA_ROTA_NADA);
  assert(casa_rota("GET", "http://example.com/x", TOK, &p) == CASA_ROTA_NADA);
  snprintf(alvo, sizeof alvo, "/%s/../settings", TOK);
  assert(casa_rota("GET", alvo, TOK, &p) == CASA_ROTA_NADA);
  snprintf(alvo, sizeof alvo, "/%s/abc/0", TOK);
  assert(casa_rota("GET", alvo, TOK, &p) == CASA_ROTA_NADA);
  snprintf(alvo, sizeof alvo, "/%s/%s/0/../../x", TOK, HASH);
  assert(casa_rota("GET", alvo, TOK, &p) == CASA_ROTA_NADA);
  snprintf(alvo, sizeof alvo, "/%s/%s/", TOK, HASH);
  assert(casa_rota("GET", alvo, TOK, &p) == CASA_ROTA_NADA);
  assert(casa_rota("GET", alvo, "", &p) == CASA_ROTA_NADA && casa_rota(NULL, alvo, TOK, &p) == CASA_ROTA_NADA);

  // Host: so IPv4 literal de rede privada com a porta certa (DNS rebinding).
  assert(casa_host_ok("192.168.1.30:47471", 47471) && casa_host_ok("127.0.0.1:47471", 47471));
  assert(!casa_host_ok("192.168.1.30:80", 47471) && !casa_host_ok("192.168.1.30", 47471));
  assert(!casa_host_ok("evil.example:47471", 47471) && !casa_host_ok("8.8.8.8:47471", 47471));
  assert(!casa_host_ok("192.168.1.30:47471x", 47471) && !casa_host_ok(NULL, 47471));
}

static void corpoEMotor(void) {
  char f[400], j[160];
  const char *c = NULL;
  assert(casa_fontes_do_corpo("{\"torrent\":{\"infoHash\":\"x\",\"peerSearch\":{\"sources\":[\"tracker:udp://t.example:80\","
                              "\"http://evil/\",\"dht:dd82\",\"tracker:a\\\\b\"],\"min\":40}}}", f, sizeof f) == 2);
  assert(!strcmp(f, "tracker:udp://t.example:80\ndht:dd82\n"));
  assert(casa_fontes_do_corpo("{}", f, sizeof f) == 0 && !f[0]);
  assert(casa_fontes_do_corpo(NULL, f, sizeof f) == 0 && casa_fontes_do_corpo("{\"sources\":[\"tracker:x", f, sizeof f) == 0);
  assert(casa_json_arquivos(1234, j, sizeof j) && strstr(j, "\"files\"") && strstr(j, "1234") && strstr(j, ".mkv"));
  assert(!casa_json_arquivos(1234, j, 8));
  // O repasse so vai ao motor deste aparelho: 127.0.0.1, http, com caminho.
  assert(casa_url_do_motor("http://127.0.0.1:8123/stream/0", &c) == 8123 && !strcmp(c, "/stream/0"));
  assert(casa_url_do_motor("http://127.0.0.1:8123/x", NULL) == 8123);
  assert(!casa_url_do_motor("http://192.168.1.2:8123/x", &c) && !casa_url_do_motor("https://127.0.0.1:8123/x", &c));
  assert(!casa_url_do_motor("http://127.0.0.1.evil.com:80/x", &c) && !casa_url_do_motor("http://127.0.0.1/x", &c));
  assert(!casa_url_do_motor("http://127.0.0.1:99999/x", &c) && !casa_url_do_motor("http://127.0.0.1:80", &c));
  assert(!casa_url_do_motor("http://127.0.0.1:80/a b", &c) && !casa_url_do_motor(NULL, &c));
}

int main(void) {
  redes();
  descoberta();
  escolha();
  rotas();
  corpoEMotor();
  puts("casa: regra pura ok");
  return 0;
}
