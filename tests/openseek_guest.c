// OpenSeek, biblioteca compartilhada (convidado): entrada do convite, corpos JSON, leitura do lookup, papel,
// rotulos honestos e o calculo do ajuste. C puro, sem rede nem SDL. Ver tests/openseek_guest.sh.
#include "openseekproto.h"
#include "osp1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

int main(void) {
  OpenSeekEntrada e;
  OpenSeekCompart c;
  OpenSeekClaim cl;
  char b[512];
  size_t n;
  const char *pin = "sha256//ARR8fC1f3w5x3AUwJJcblNj5Y1S8BElulcpzA6ySMvg=";

  // 1. a string de convite (como o painel do dono a mostra): host, pino e id codificados, segredo com tracos
  CHECK(openseek_ler_entrada("openseek://join/1?h=192.168.0.107%3A8787&p=sha256%2F%2FARR8fC1f3w5x3AUwJJcblNj5Y1S8BElulcpzA6ySMvg%3D"
                             "&i=00112233445566778899aabbccddeeff&c=K7QF-M2XD-P9WA", &e) == 1);
  CHECK(e.forma == 2 && !strcmp(e.base, "https://192.168.0.107:8787") && !strcmp(e.pino, pin));
  CHECK(!strcmp(e.convite, "00112233445566778899aabbccddeeff") && !strcmp(e.segredo, "K7QFM2XDP9WA"));
  // sem h= e p=: so id + segredo (o endereco ja foi digitado)
  CHECK(openseek_ler_entrada("  openseek://join/1?i=00112233445566778899aabbccddeeff&c=k7qfm2xdp9wa", &e) == 1 && !e.base[0] && !e.pino[0]);
  // 2. o codigo digitado: etiqueta (4) + segredo (12), maiusculas/minusculas/tracos/espacos
  CHECK(openseek_ler_entrada("7k3q-xymp-zxpn-ka2y", &e) == 1 && e.forma == 2 && !strcmp(e.convite, "7K3Q") && !strcmp(e.segredo, "XYMPZXPNKA2Y"));
  CHECK(openseek_ler_entrada("7K3Q XYMP ZXPN KA2Y", &e) == 1 && !strcmp(e.convite, "7K3Q"));
  // 3. o codigo de 8 da janela do dono continua valendo, como antes
  CHECK(openseek_ler_entrada("k7qf-m2xd", &e) == 1 && e.forma == 1 && !strcmp(e.segredo, "K7QFM2XD") && !e.convite[0]);
  // 4. o que nao e nada disso, ou e perigoso, e recusado (sem 0, O, 1, I; tamanhos errados; esquema errado; pino ruim)
  const char *ruim[] = { "", "   ", "12345678", "ABCDEFGHJKMN", "7K3QXYMPZXPNKA2", "7K3QXYMPZXPNKA2YZ", "openseek://join/2?i=x&c=y",
    "openseek://join/1?i=zz&c=K7QFM2XDP9WA", "openseek://join/1?i=00112233445566778899aabbccddeeff&c=short",
    "openseek://join/1?h=ftp%3A%2F%2Fx&i=00112233445566778899aabbccddeeff&c=K7QFM2XDP9WA",
    "openseek://join/1?p=sha256%2Fnot-a-pin&i=00112233445566778899aabbccddeeff&c=K7QFM2XDP9WA",
    "http://evil/openseek://join/1?i=00112233445566778899aabbccddeeff&c=K7QFM2XDP9WA" };
  for (size_t i = 0; i < sizeof ruim / sizeof *ruim; i++) CHECK(openseek_ler_entrada(ruim[i], &e) == 0 && e.forma == 0);
  CHECK(openseek_ler_entrada(NULL, &e) == 0);

  // 5. corpos
  n = openseek_json_start(b, sizeof b, NULL);                     CHECK(n == 2 && !strcmp(b, "{}"));
  n = openseek_json_start(b, sizeof b, "7K3Q");                   CHECK(n > 0 && !strcmp(b, "{\"invite\":\"7K3Q\"}"));
  n = openseek_json_start(b, sizeof b, "bad\"id");                CHECK(n == 0);
  n = openseek_json_procura(b, sizeof b, "tt1856101", 0, 0, 9759134L, 1100000000ULL);
  CHECK(n > 0 && strstr(b, "\"imdb_id\":\"tt1856101\"") && strstr(b, "\"duration_ms\":9759134") && strstr(b, "\"size_bytes\":1100000000"));
  CHECK(!strstr(b, "http") && !strstr(b, "url") && !strstr(b, "header") && !strstr(b, "token"));    // nunca URL, nunca segredo
  n = openseek_json_procura(b, sizeof b, "tt0944947", 1, 2, 3000000L, 0);
  CHECK(strstr(b, "\"season\":1") && strstr(b, "\"episode\":2") && !strstr(b, "size_bytes"));
  CHECK(openseek_json_procura(b, sizeof b, "", 0, 0, 1000, 0) == 0 && b[0] == 0);              // sem imdb: nada a procurar
  CHECK(openseek_json_procura(b, sizeof b, "tt1\"x", 0, 0, 1000, 0) == 0);
  CHECK(openseek_json_procura(b, sizeof b, "tt1856101", 0, 0, 0, 0) == 0);
  CHECK(openseek_json_procura(b, 20, "tt1856101", 0, 0, 1000, 0) == 0);                        // nao coube
  n = openseek_json_offset(b, sizeof b, -49633L);                 CHECK(n > 0 && !strcmp(b, "{\"offset_ms\":-49633}"));
  CHECK(openseek_json_offset(b, sizeof b, 99999999L) == 0);

  // 6. o papel vem do claim (servidor antigo sem "role" = dono) e nunca de outro lugar
  const char *claim = "{\"device_id\":\"0123456789abcdef\",\"token\":\"AbCdEfGhIjKlMnOpQrStUvWxYz0123456789-_abcde\","
                      "\"server_proof\":\"00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff\",\"role\":\"guest\"}";
  CHECK(openseek_ler_claim(claim, &cl) == 1 && !strcmp(cl.papel, "guest") && openseek_papel_de(cl.papel) == OS_PAPEL_CONVIDADO);
  claim = "{\"device_id\":\"0123456789abcdef\",\"token\":\"AbCdEfGhIjKlMnOpQrStUvWxYz0123456789-_abcde\","
          "\"server_proof\":\"00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff\"}";
  CHECK(openseek_ler_claim(claim, &cl) == 1 && !strcmp(cl.papel, "owner") && openseek_papel_de(cl.papel) == OS_PAPEL_DONO);
  claim = "{\"device_id\":\"0123456789abcdef\",\"token\":\"AbCdEfGhIjKlMnOpQrStUvWxYz0123456789-_abcde\","
          "\"server_proof\":\"00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff\",\"role\":\"root\"}";
  CHECK(openseek_ler_claim(claim, &cl) == 1 && openseek_papel_de(cl.papel) == OS_PAPEL_DONO);   // papel desconhecido nunca eleva nem rebaixa em silencio
  CHECK(openseek_papel_de(NULL) == OS_PAPEL_DONO && openseek_papel_de("guest") == OS_PAPEL_CONVIDADO);

  // 7. a resposta do lookup: aproximada por omissao, nunca "verificada" so por metadado
  const char *base = "https://192.168.0.107:8787";
  const char *resp = "{\"release_key\":\"abc\",\"candidates\":[{\"preview_id\":\"00112233445566778899aabbccddeeff\",\"kind\":\"length_differs\","
    "\"approximate\":true,\"verified\":false,\"reason\":\"ids match but the durations differ\",\"mapping\":{\"a\":1.0,\"b\":0.0},"
    "\"source\":{\"duration_ms\":9759134,\"tiles\":976,\"n_tiles\":976,\"coverage\":1.0},"
    "\"vtt_url\":\"https://192.168.0.107:8787/v1/c/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/00112233445566778899aabbccddeeff/thumbs.vtt?exp=1900000000&layout=hybrid\"},"
    "{\"preview_id\":\"ffeeddccbbaa99887766554433221100\",\"kind\":\"same_length\"}]}";
  CHECK(openseek_ler_procura(resp, base, &c) == 1 && !strcmp(c.id, "00112233445566778899aabbccddeeff") && !strcmp(c.tipo, "length_differs"));
  CHECK(c.aproximado == 1 && c.verificado == 0 && c.a == 1.0 && c.b == 0.0 && c.tiles == 976 && c.nTiles == 976 && c.fonteMs == 9759134);
  CHECK(c.exp == 1900000000LL && c.nCandidatos == 2 && strstr(openseek_rotulo_compart(&c), "aproximadas"));
  CHECK(!strstr(openseek_rotulo_compart(&c), "verificadas"));
  // ajuste manual: o servidor devolve user_adjusted com b negativo
  resp = "{\"candidates\":[{\"preview_id\":\"00112233445566778899aabbccddeeff\",\"kind\":\"user_adjusted\",\"approximate\":true,\"verified\":false,"
         "\"mapping\":{\"a\":1.0,\"b\":-49633.0},\"vtt_url\":\"https://192.168.0.107:8787/v1/c/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/00112233445566778899aabbccddeeff/thumbs.vtt?exp=1900000000\"}]}";
  CHECK(openseek_ler_procura(resp, base, &c) == 1 && !strcmp(c.tipo, "user_adjusted") && c.b == -49633.0 && strstr(openseek_rotulo_compart(&c), "ajustadas por você"));
  // so se o servidor disser verified=true E approximate=false o rotulo muda (reservado a uma etapa futura)
  resp = "{\"candidates\":[{\"preview_id\":\"00112233445566778899aabbccddeeff\",\"kind\":\"same_length\",\"approximate\":false,\"verified\":true,"
         "\"mapping\":{\"a\":1.0,\"b\":0.0},\"vtt_url\":\"https://192.168.0.107:8787/v1/c/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/00112233445566778899aabbccddeeff/thumbs.vtt?exp=1900000000\"}]}";
  CHECK(openseek_ler_procura(resp, base, &c) == 1 && c.aproximado == 0 && c.verificado == 1);
  // verified=true com approximate=true continua aproximado; sem os campos continua aproximado
  resp = "{\"candidates\":[{\"preview_id\":\"00112233445566778899aabbccddeeff\",\"approximate\":true,\"verified\":true,\"mapping\":{\"a\":1.0,\"b\":0.0},"
         "\"vtt_url\":\"https://192.168.0.107:8787/v1/c/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/00112233445566778899aabbccddeeff/thumbs.vtt?exp=1900000000\"}]}";
  CHECK(openseek_ler_procura(resp, base, &c) == 1 && c.aproximado == 1);
  resp = "{\"candidates\":[{\"preview_id\":\"00112233445566778899aabbccddeeff\",\"mapping\":{\"a\":1.0,\"b\":0.0},"
         "\"vtt_url\":\"https://192.168.0.107:8787/v1/c/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/00112233445566778899aabbccddeeff/thumbs.vtt?exp=1900000000\"}]}";
  CHECK(openseek_ler_procura(resp, base, &c) == 1 && c.aproximado == 1 && c.verificado == 0);
  // 8. respostas ruins: sem candidatos, id invalido, VTT fora da origem pareada, mapeamento absurdo
  CHECK(openseek_ler_procura("{\"candidates\":[]}", base, &c) == 0);
  CHECK(openseek_ler_procura("{\"error\":\"not_cached\"}", base, &c) == 0);
  CHECK(openseek_ler_procura(NULL, base, &c) == 0);
  CHECK(openseek_ler_procura("{\"candidates\":[{\"preview_id\":\"zz\",\"vtt_url\":\"https://192.168.0.107:8787/x\"}]}", base, &c) == 0);
  CHECK(openseek_ler_procura("{\"candidates\":[{\"preview_id\":\"00112233445566778899aabbccddeeff\",\"mapping\":{\"a\":1.0,\"b\":0.0},"
        "\"vtt_url\":\"https://evil.example:8787/v1/c/A/B/thumbs.vtt?exp=1\"}]}", base, &c) == 0);                // o pino so cobre a origem pareada
  CHECK(openseek_ler_procura("{\"candidates\":[{\"preview_id\":\"00112233445566778899aabbccddeeff\",\"mapping\":{\"a\":9.0,\"b\":0.0},"
        "\"vtt_url\":\"https://192.168.0.107:8787/v1/c/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/00112233445566778899aabbccddeeff/thumbs.vtt?exp=1\"}]}", base, &c) == 0);

  // 9. o ajuste: "+5 s" = miniaturas 5 s mais TARDE = b menor; acumula; trava em +-6 h; zera com o delta inverso
  CHECK(openseek_offset_apos(0.0, 5000) == -5000 && openseek_offset_apos(-5000.0, 5000) == -10000 && openseek_offset_apos(-10000.0, -30000) == 20000);
  CHECK(openseek_offset_apos(-49633.0, -49633) == 0 || openseek_offset_apos(-49633.0, -49633) == 0);
  CHECK(openseek_offset_apos(0.0, 99999999L) == -21600000L && openseek_offset_apos(0.0, -99999999L) == 21600000L);
  CHECK(openseek_offset_apos(0.4, 0) == 0 && openseek_offset_apos(-0.4, 0) == 0);

  // 10. CONTRIBUINTE: o corpo leva so link https, cabecalhos permitidos, ids e duracao; recusa o que nao serve
  {
    char c[1024];
    const char *cabs = "User-Agent: Nuvio\nCookie: SEGREDO\nReferer: https://ref.example/\nAuthorization: Bearer ABC";
    OpenSeekFonte f = { "https://store.torbox.app/dl/x.mkv?token=TT", cabs, 1462462, "tt0994314", 1, 2, 123, "", "", 0, -1 };
    CHECK(openseek_json_contribuir(c, sizeof c, &f) > 0);
    CHECK(strstr(c, "\"url\":\"https://store.torbox.app/dl/x.mkv?token=TT\"") && strstr(c, "\"imdb_id\":\"tt0994314\""));
    CHECK(strstr(c, "\"season\":1") && strstr(c, "\"episode\":2") && strstr(c, "\"duration_ms\":1462462"));
    CHECK(strstr(c, "User-Agent") && strstr(c, "Referer") && !strstr(c, "Cookie") && !strstr(c, "Authorization") && !strstr(c, "SEGREDO"));
    CHECK(!strstr(c, "video_hash") && !strstr(c, "filename") && !strstr(c, "playhead"));
    f.url = "http://store.torbox.app/x.mkv"; CHECK(openseek_json_contribuir(c, sizeof c, &f) == 0 && c[0] == 0);        // so https
    f.url = "https://store.torbox.app/x.mkv"; f.imdb = "kitsu:41370"; CHECK(openseek_json_contribuir(c, sizeof c, &f) == 0);   // sem id tt
    f.imdb = "tt0994314"; f.durMs = 1000; CHECK(openseek_json_contribuir(c, sizeof c, &f) == 0);                          // duracao real
    CHECK(openseek_ler_pode_contribuir("{\"device_id\":\"a\",\"device_name\":\"n\",\"contribute\":true}"));
    CHECK(!openseek_ler_pode_contribuir("{\"device_id\":\"a\",\"contribute\":false}") && !openseek_ler_pode_contribuir("{\"device_id\":\"a\"}") && !openseek_ler_pode_contribuir(NULL));
  }

  if (falhas) { printf("openseek_guest: %d falha(s)\n", falhas); return 1; }
  printf("openseek_guest: ok\n");
  return 0;
}
