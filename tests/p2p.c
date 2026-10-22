// P2P experimental: tudo o que p2p.c decide, sem servidor e sem rede.
//
// O TRANSPORTE E FALSO: rede_baixar_st / rede_postar_st / rede_baixar_trecho_st
// respondem o que cada cenario manda. As formas das respostas (settings,
// create com "files") sao as MEDIDAS contra stremio/server 4.21.2 em Docker
// com o Big Buck Bunny; o que nao tem resposta ("hash sem peers") e modelado
// como o servidor real se comporta: nao responde nunca, e o prazo e nosso.
#include "p2p.h"
#include "streams.h"
#include "p2pmotor.h"
#include "rede.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------- ajustes
static int ligado;
static char base[200];
int ajustes_p2p_ligado(void) { return ligado; }
const char *ajustes_p2p_url(void) { return base; }
void debrid_episodio(int *t, int *e) { *t = 1; *e = 2; }
uint64_t badges_detectar(const char *m) { (void)m; return 0; }
uint64_t badges_bit(const char *id) { (void)id; return 0; }
// p2pmotor.c (motor embutido) entra no link; sem -DNV_P2P_MOTOR ele so diz
// "sem motor" e nunca chama estes.
const char *dados_dir(void) { return "/tmp"; }
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; r->erro = REDE_INDISPONIVEL; return 0; }
void rede_resposta_limpar(RedeResposta *r) { (void)r; }

// ---------------------------------------------------------------- rede falsa
static const char *respSettings; static int stSettings;   // NULL = sem resposta
static const char *respCreate;   static int stCreate;
static int trechoOk, stTrecho;
static int nSettings, nCreate, nTrecho;
static char urlCreate[400], corpoCreate[2000], cabCreate[80], urlTrecho[400];
static int prazoCreate;

char *rede_baixar_st(const char *url, int s, const char *const *c, int *st) {
  (void)s; (void)c;
  nSettings++;
  assert(strstr(url, "/settings"));
  if (st) *st = respSettings ? stSettings : 0;
  return respSettings ? strdup(respSettings) : NULL;
}
char *rede_postar_st(const char *url, int s, const char *const *c, const char *corpo, int *st) {
  nCreate++;
  prazoCreate = s;
  snprintf(urlCreate, sizeof urlCreate, "%s", url);
  snprintf(corpoCreate, sizeof corpoCreate, "%s", corpo);
  snprintf(cabCreate, sizeof cabCreate, "%s", c && c[0] ? c[0] : "");
  if (st) *st = respCreate ? stCreate : 0;
  return respCreate ? strdup(respCreate) : NULL;
}
char *rede_baixar_trecho_st(const char *url, int s, long long ini, long long fim, long *tam,
                            int *st, int *erro, char *final, unsigned tf) {
  (void)s; (void)ini; (void)fim; (void)erro; (void)final; (void)tf;
  nTrecho++;
  snprintf(urlTrecho, sizeof urlTrecho, "%s", url);
  if (st) *st = trechoOk ? 206 : stTrecho;
  if (tam) *tam = trechoOk ? 4 : 0;
  return trechoOk ? strdup("data") : NULL;
}

static const char *SETTINGS = "{\"options\":[],\"values\":{\"serverVersion\":\"4.21.2\",\"cacheSize\":2147483648},\"baseUrl\":\"http://172.17.0.3:11470\"}";
// Recorte do create real do Big Buck Bunny (idx 1 = o mp4).
static const char *BBB = "{\"infoHash\":\"dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c\",\"name\":\"Big Buck Bunny\",\"peers\":6,\"files\":[{\"path\":\"Big Buck Bunny/Big Buck Bunny.en.srt\",\"name\":\"Big Buck Bunny.en.srt\",\"length\":140,\"offset\":0},{\"path\":\"Big Buck Bunny/Big Buck Bunny.mp4\",\"name\":\"Big Buck Bunny.mp4\",\"length\":276134947,\"offset\":140},{\"path\":\"Big Buck Bunny/poster.jpg\",\"name\":\"poster.jpg\",\"length\":310380,\"offset\":276135087}]}";
static const char *HASH = "DD8255ECDC7CA55FB0BBF81323D87062DB1F6D1C";
static const char *HASHMIN = "dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c";

static void cenario(int lig, const char *b, const char *sr, int ss, const char *cr, int cs, int tOk, int tSt) {
  ligado = lig; snprintf(base, sizeof base, "%s", b);
  respSettings = sr; stSettings = ss; respCreate = cr; stCreate = cs;
  trechoOk = tOk; stTrecho = tSt;
  nSettings = nCreate = nTrecho = 0;
  urlCreate[0] = corpoCreate[0] = urlTrecho[0] = 0;
}

int main(void) {
  char o[300], url[300], det[32];

  // ---------------------------------------------------------- endereco
  assert(p2p_normalizar_url("192.168.1.5", o, sizeof o) && !strcmp(o, "http://192.168.1.5:11470"));
  assert(p2p_normalizar_url("192.168.1.5:8080", o, sizeof o) && !strcmp(o, "http://192.168.1.5:8080"));
  assert(p2p_normalizar_url("  HTTP://NAS.local:11470/  ", o, sizeof o) && !strcmp(o, "http://NAS.local:11470"));
  assert(p2p_normalizar_url("http://nas/algo?x=1#y", o, sizeof o) && !strcmp(o, "http://nas:11470"));
  assert(p2p_normalizar_url("https://meu-servidor.example.com", o, sizeof o)
         && !strcmp(o, "https://meu-servidor.example.com:12470"));
  assert(p2p_normalizar_url("[fe80::1]:11470", o, sizeof o) && !strcmp(o, "http://[fe80::1]:11470"));
  assert(!p2p_normalizar_url("", o, sizeof o));
  assert(!p2p_normalizar_url("   ", o, sizeof o));
  assert(!p2p_normalizar_url("ftp://x", o, sizeof o));
  assert(!p2p_normalizar_url("magnet:?xt=urn:btih:abc", o, sizeof o));
  assert(!p2p_normalizar_url("host:99999", o, sizeof o));
  assert(!p2p_normalizar_url("host:", o, sizeof o));
  assert(!p2p_normalizar_url("host:80a", o, sizeof o));
  assert(!p2p_normalizar_url("ho st", o, sizeof o));
  assert(!p2p_normalizar_url("a\"b", o, sizeof o));
  assert(!p2p_normalizar_url("http://", o, sizeof o));
  puts("p2p: enderecos ok");

  // ---------------------------------------------------------- hash e corpo
  assert(p2p_hash_valido(HASH) && p2p_hash_valido(HASHMIN));
  assert(!p2p_hash_valido("") && !p2p_hash_valido("dd8255") && !p2p_hash_valido(NULL));
  assert(!p2p_hash_valido("zd8255ecdc7ca55fb0bbf81323d87062db1f6d1c"));
  assert(!p2p_hash_valido("dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c0"));
  { char corpo[1800];
    int n = p2p_corpo_criar(HASH, "tracker:udp://tracker.opentrackr.org:1337/announce\n"
                                  "tracker:http://t.example:80/announce\ndht:abc123", corpo, sizeof corpo);
    assert(n > 0 && (unsigned)n == strlen(corpo));
    assert(strstr(corpo, "\"infoHash\":\"dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c\""));   // minusculo
    assert(strstr(corpo, "\"tracker:udp://tracker.opentrackr.org:1337/announce\""));
    assert(strstr(corpo, "\"tracker:http://t.example:80/announce\""));
    assert(strstr(corpo, "\"dht:abc123\""));
    assert(strstr(corpo, "\"dht:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c\"]"));           // o do proprio torrent
    assert(strstr(corpo, "\"min\":40,\"max\":150"));
    // O addon e terceiro: aspa, barra invertida e controle NAO entram, senao
    // fechariam a string e injetariam campos no JSON.
    n = p2p_corpo_criar(HASH, "tracker:udp://a\",\"evil\":\"1\nlixo\ntracker:udp://b\\c\ntracker:udp://ok:1", corpo, sizeof corpo);
    assert(n > 0 && !strstr(corpo, "evil") && !strstr(corpo, "lixo") && !strstr(corpo, "b\\c"));
    assert(strstr(corpo, "\"tracker:udp://ok:1\""));
    // Sem fontes: so o dht do proprio hash.
    assert(p2p_corpo_criar(HASH, NULL, corpo, sizeof corpo) > 0);
    assert(strstr(corpo, "[\"dht:dd82") != NULL);
    assert(!p2p_corpo_criar("nao-e-hash", NULL, corpo, sizeof corpo));
    // Buffer pequeno demais nunca estoura nem devolve JSON cortado.
    { char peq[140];
      int r = p2p_corpo_criar(HASH, "tracker:udp://tracker.opentrackr.org:1337/announce", peq, sizeof peq);
      assert(r == 0 || (unsigned)r == strlen(peq)); }
  }
  puts("p2p: corpo do create ok");

  // ---------------------------------------------------------- arquivo
  assert(p2p_escolher_arquivo(BBB, -1, 0, 0) == 1);            // maior video
  assert(p2p_escolher_arquivo(BBB, 1, 0, 0) == 1);             // o do addon
  assert(p2p_escolher_arquivo(BBB, 0, 0, 0) == 1);             // fileIdx aponta a .srt: nao e video
  assert(p2p_escolher_arquivo(BBB, 2, 0, 0) == 1);             // .jpg idem
  assert(p2p_escolher_arquivo(BBB, 99, 0, 0) == 1);            // fora da faixa
  { const char *pack = "{\"files\":[{\"path\":\"S/Serie.S01E01.mkv\",\"length\":900},"
      "{\"path\":\"S/Serie.S01E02.mkv\",\"length\":800},{\"path\":\"S/Serie.S01E03.mkv\",\"length\":1000},"
      "{\"path\":\"S/info.nfo\",\"length\":5}]}";
    assert(p2p_escolher_arquivo(pack, 0, 0, 0) == 0);          // o addon manda: ele manda
    assert(p2p_escolher_arquivo(pack, -1, 1, 2) == 1);         // sem fileIdx: SxxEyy
    assert(p2p_escolher_arquivo(pack, 3, 1, 2) == 1);          // fileIdx e .nfo: cai no SxxEyy
    assert(p2p_escolher_arquivo(pack, -1, 0, 0) == 2);         // filme/sem padrao: o maior
    assert(p2p_escolher_arquivo(pack, -1, 9, 9) == 2); }       // padrao que nao existe: o maior
  assert(p2p_escolher_arquivo("{\"files\":[{\"path\":\"a.nfo\",\"length\":5},{\"path\":\"b.jpg\",\"length\":9}]}", -1, 0, 0) == -1);
  assert(p2p_escolher_arquivo("{\"files\":[]}", -1, 0, 0) == -1);
  assert(p2p_escolher_arquivo("{}", -1, 0, 0) == -1);
  assert(p2p_escolher_arquivo(NULL, -1, 0, 0) == -1);
  puts("p2p: escolha de arquivo ok");

  // ---------------------------------------------------------- URL
  assert(p2p_url_reproducao("http://h:11470", HASH, 1, url, sizeof url));
  assert(!strcmp(url, "http://h:11470/dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c/1"));
  assert(!p2p_url_reproducao("http://h:11470", HASH, -1, url, sizeof url));
  assert(!p2p_url_reproducao("http://h:11470", "x", 1, url, sizeof url));
  assert(!p2p_url_reproducao("http://h:11470", HASH, 1, url, 20));
  puts("p2p: url de reproducao ok");

  // ---------------------------------------------------------- parse do stream
  { Stream *v = NULL; int n;
    const char *tor =
      "{\"streams\":["
      "{\"name\":\"Torrentio\\n1080p\",\"title\":\"Big Buck Bunny\\n\\ud83d\\udc64 12\",\"infoHash\":\"dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c\","
      "\"fileIdx\":1,\"sources\":[\"tracker:udp:\\/\\/tracker.opentrackr.org:1337\\/announce\",\"udp://solto.example:80/announce\",\"dht:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c\",\"lixo\"],"
      "\"behaviorHints\":{\"bingeGroup\":\"torrentio|1080p\"}},"
      "{\"name\":\"Direta\",\"url\":\"https://cdn.example/v.mp4\",\"sources\":[\"tracker:udp://nao.deve.entrar:1\"]},"
      "{\"name\":\"AIO\",\"clientResolve\":{\"infoHash\":\"1111111111111111111111111111111111111111\"},\"title\":\"x\"}"
      "]}";
    n = stream_extrair(tor, "T", &v);
    assert(n == 3);
    assert(!v[0].url[0] && !strcmp(v[0].infoHash, HASHMIN) && v[0].fileIdx == 1);
    assert(!strcmp(v[0].fontes,
      "tracker:udp://tracker.opentrackr.org:1337/announce\n"
      "tracker:udp://solto.example:80/announce\n"
      "dht:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c"));           // \/ decodificado, url solta ganha o prefixo, "lixo" cai
    assert(v[1].url[0] && !v[1].infoHash[0] && v[1].fileIdx == -1 && !v[1].fontes[0]);
    assert(!strcmp(v[2].infoHash, "1111111111111111111111111111111111111111") && v[2].fileIdx == -1);
    free(v);
    // Excesso de trackers: cabe o que cabe, INTEIRO (nunca um tracker cortado).
    { char big[9000]; int u = 0, i;
      u += snprintf(big, sizeof big, "{\"streams\":[{\"infoHash\":\"%s\",\"sources\":[", HASHMIN);
      for (i = 0; i < 60; i++) u += snprintf(big + u, sizeof big - (unsigned)u, "%s\"tracker:udp://tracker%02d.example.org:6969/announce\"", i ? "," : "", i);
      snprintf(big + u, sizeof big - (unsigned)u, "]}]}");
      n = stream_extrair(big, "T", &v);
      assert(n == 1 && v[0].fontes[0]);
      { const char *p = v[0].fontes; int linhas = 0;
        while (*p) { const char *nl = strchr(p, '\n'); size_t L = nl ? (size_t)(nl - p) : strlen(p);
          assert(L == strlen("tracker:udp://tracker00.example.org:6969/announce")); linhas++; p = nl ? nl + 1 : p + L; }
        assert(linhas > 5 && linhas < 60); }
      free(v); }
  }
  puts("p2p: parse de infoHash/fileIdx/sources ok");

  // ---------------------------------------------------------- resolver
  // 1) caminho feliz
  cenario(1, "http://192.168.1.5:11470", SETTINGS, 200, BBB, 200, 1, 0);
  assert(p2p_resolver(HASH, -1, "tracker:udp://t.example:1\ndht:x", url, sizeof url) == P2P_OK);
  assert(!strcmp(url, "http://192.168.1.5:11470/dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c/1"));
  assert(!strcmp(urlTrecho, url));                               // os bytes sao pedidos do MESMO endereco que toca
  assert(!strcmp(urlCreate, "http://192.168.1.5:11470/dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c/create"));
  // text/plain, e nao application/json: o servidor so libera o cabecalho Range
  // no preflight, entao um POST JSON cross-origin (Tizen) seria barrado.
  assert(!strcmp(cabCreate, "Content-Type: text/plain"));
  assert(strstr(corpoCreate, "tracker:udp://t.example:1"));
  assert(prazoCreate == P2P_PRAZO_METADADOS);
  assert(p2p_ultimo_erro() == P2P_OK);
  // 2) desligado / sem endereco: nao toca a rede
  cenario(0, "http://192.168.1.5:11470", SETTINGS, 200, BBB, 200, 1, 0);
  assert(p2p_resolver(HASH, -1, NULL, url, sizeof url) == P2P_ERR_DESLIGADO && !url[0]);
  assert(!p2p_ativo() && nSettings == 0 && nCreate == 0);
  cenario(1, "", SETTINGS, 200, BBB, 200, 1, 0);
  assert(p2p_resolver(HASH, -1, NULL, url, sizeof url) == P2P_ERR_DESLIGADO && !p2p_ativo());
  // 3) hash torto: nem pergunta ao servidor
  cenario(1, "http://h:11470", SETTINGS, 200, BBB, 200, 1, 0);
  assert(p2p_resolver("torto", -1, NULL, url, sizeof url) == P2P_ERR_HASH && nSettings == 0);
  // 4) servidor fora do ar: erro em segundos, sem gastar os 30 s dos metadados
  cenario(1, "http://h:11470", NULL, 0, BBB, 200, 1, 0);
  assert(p2p_resolver(HASH, -1, NULL, url, sizeof url) == P2P_ERR_SERVIDOR && nCreate == 0 && !url[0]);
  // 5) responde 200, mas nao e Stremio (roteador, Plex...)
  cenario(1, "http://h:11470", "<html>router</html>", 200, BBB, 200, 1, 0);
  assert(p2p_resolver(HASH, -1, NULL, url, sizeof url) == P2P_ERR_NAO_STREMIO && nCreate == 0);
  cenario(1, "http://h:11470", "{\"error\":\"x\"}", 404, BBB, 200, 1, 0);
  assert(p2p_resolver(HASH, -1, NULL, url, sizeof url) == P2P_ERR_NAO_STREMIO);
  // 6) sem peers: o create nunca responde (medido: 130 s de silencio)
  cenario(1, "http://h:11470", SETTINGS, 200, NULL, 0, 1, 0);
  assert(p2p_resolver(HASH, -1, NULL, url, sizeof url) == P2P_ERR_SEM_PEERS && nTrecho == 0);
  cenario(1, "http://h:11470", SETTINGS, 200, "{}", 504, 1, 0);
  assert(p2p_resolver(HASH, -1, NULL, url, sizeof url) == P2P_ERR_SEM_PEERS);
  // 7) servidor recusa
  cenario(1, "http://h:11470", SETTINGS, 200, "oops", 500, 1, 0);
  assert(p2p_resolver(HASH, -1, NULL, url, sizeof url) == P2P_ERR_RECUSOU);
  // 8) torrent sem video
  cenario(1, "http://h:11470", SETTINGS, 200, "{\"files\":[{\"path\":\"a.nfo\",\"length\":5}]}", 200, 1, 0);
  assert(p2p_resolver(HASH, -1, NULL, url, sizeof url) == P2P_ERR_SEM_VIDEO && nTrecho == 0 && !url[0]);
  // 9) metadados vieram, mas nenhum byte de video no prazo
  cenario(1, "http://h:11470", SETTINGS, 200, BBB, 200, 0, 0);
  assert(p2p_resolver(HASH, -1, NULL, url, sizeof url) == P2P_ERR_SEM_PEERS && !url[0] && nTrecho == 1);
  cenario(1, "http://h:11470", SETTINGS, 200, BBB, 200, 0, 404);
  assert(p2p_resolver(HASH, -1, NULL, url, sizeof url) == P2P_ERR_RECUSOU && !url[0]);
  // 10) o fileIdx do addon vence; episodio de pacote
  cenario(1, "http://h:11470", SETTINGS, 200,
          "{\"files\":[{\"path\":\"S.S01E01.mkv\",\"length\":9},{\"path\":\"S.S01E02.mkv\",\"length\":8}]}", 200, 1, 0);
  assert(p2p_resolver(HASH, 0, NULL, url, sizeof url) == P2P_OK && strstr(url, "/0"));
  assert(p2p_resolver(HASH, -1, NULL, url, sizeof url) == P2P_OK);            // debrid_episodio = T1E2
  assert(strlen(url) > 2 && url[strlen(url) - 1] == '1');
  puts("p2p: resolvedor e mapeamento de erros ok");

  // ---------------------------------------------------------- testar
  cenario(0, "http://h:11470", SETTINGS, 200, BBB, 200, 1, 0);       // desligado: TESTA mesmo assim
  assert(p2p_testar(det, sizeof det) == P2P_OK && !strcmp(det, "4.21.2"));
  cenario(0, "", SETTINGS, 200, BBB, 200, 1, 0);
  assert(p2p_testar(det, sizeof det) == P2P_ERR_DESLIGADO && nSettings == 0);
  cenario(0, "http://h:11470", NULL, 0, BBB, 200, 1, 0);
  assert(p2p_testar(det, sizeof det) == P2P_ERR_SERVIDOR);
  cenario(0, "http://h:11470", "{}", 200, BBB, 200, 1, 0);
  assert(p2p_testar(det, sizeof det) == P2P_ERR_NAO_STREMIO && !det[0]);
  puts("p2p: testar conexao ok");

  // ---------------------------------------------------------- motor embutido
  // Sem -DNV_P2P_MOTOR os cotos dizem indisponivel: sem endereco, P2P inativo
  // (o comportamento de antes deste build).
  cenario(1, "", SETTINGS, 200, BBB, 200, 1, 0);
  assert(!p2p_ativo() && !p2p_usa_motor());
  cenario(1, "http://h:11470", SETTINGS, 200, BBB, 200, 1, 0);
  assert(p2p_ativo() && !p2p_usa_motor());
  {
    char m[2400];
    // trackers do addon: so os "tracker:", percent-encoded; dht: fica de fora
    assert(p2p_magnet(HASH, "tracker:udp://t.example:1337/announce\ndht:x\ntracker:http://a.b/an?x=1",
                      m, sizeof m));
    assert(!strncmp(m, "magnet:?xt=urn:btih:dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c&tr=", 64));
    assert(strstr(m, "&tr=udp%3A%2F%2Ft.example%3A1337%2Fannounce"));
    assert(strstr(m, "&tr=http%3A%2F%2Fa.b%2Fan%3Fx%3D1"));
    assert(!strstr(m, "dht"));
    assert(!strstr(m, "opentrackr"));                     // reserva so sem trackers
    // sem trackers: os de reserva
    assert(p2p_magnet(HASH, NULL, m, sizeof m) && strstr(m, "&tr=udp%3A%2F%2Ftracker.opentrackr.org"));
    // tracker com espaco/controle nao entra
    assert(p2p_magnet(HASH, "tracker:udp://a b", m, sizeof m) && strstr(m, "opentrackr"));
    assert(!p2p_magnet("xyz", NULL, m, sizeof m));
    // buffer curto: o hash cabe, o tracker que nao cabe fica de fora inteiro
    assert(p2p_magnet(HASH, "tracker:udp://muito-longo.example:1337/announce", m, 70) && strlen(m) < 70);
  }
  {
    const char *nomes[] = { "Serie/S01E01.srt", "Serie/Serie.S01E01.mkv", "Serie/Serie.S01E02.mkv",
                            NULL, "Serie/sample.mkv" };
    double tam[] = { 10, 900e6, 950e6, 0, 5e6 };
    assert(p2p_escolher_lista(nomes, tam, 5, -1, 0, 0) == 2);    // maior video
    assert(p2p_escolher_lista(nomes, tam, 5, -1, 1, 1) == 1);    // SxxEyy
    assert(p2p_escolher_lista(nomes, tam, 5, 4, 1, 1) == 4);     // fileIdx do addon vence
    assert(p2p_escolher_lista(nomes, tam, 5, 0, 1, 2) == 2);     // fileIdx de legenda: cai no SxxEyy
    assert(p2p_escolher_lista(nomes, tam, 0, -1, 0, 0) == -1);
  }
  puts("p2p: motor (cotos, magnet, escolha) ok");

  puts("p2p: ok");
  return 0;
}
