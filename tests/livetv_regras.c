// tests/livetv_regras.sh: resolucao pelo nome, nome-base das variantes e
// recomendacoes do diagnostico da Live TV.
#include "../src/livetv_regras.h"
static int falhas;
#define OK(c, m) do { if (c) printf("ok  %s\n", m); else { printf("FALHOU %s\n", m); falhas++; } } while (0)
static int base(const char *a, const char *b) {
  char x[160], y[160]; nv_nome_base(a, x, sizeof x); nv_nome_base(b, y, sizeof y);
  return !strcmp(x, y);
}
int main(void) {
  OK(nv_tipo_vod("movie") && nv_tipo_vod("SERIES"), "filtros somente de filme e episodio");
  OK(!nv_tipo_vod("arbitrary-live-resource") && !nv_tipo_vod("events") &&
     !nv_tipo_vod("sports") && !nv_tipo_vod("tv"), "tipos ao vivo sem lista de nomes");
  OK(nv_tipo_canal("tv") && nv_tipo_canal("channel") && nv_tipo_canal("channels") &&
     nv_tipo_canal("live") && nv_tipo_canal("IPTV") && nv_tipo_canal("sport") &&
     nv_tipo_canal("sports"), "todos os tipos de canal e eventos ao vivo");
  OK(!nv_tipo_canal(NULL) && !nv_tipo_canal("") && !nv_tipo_canal("movie") &&
     !nv_tipo_canal("series") && !nv_tipo_canal("anime"), "VOD nao vira canal");
  char b[160];
  OK(nv_res_do_texto("RO| CINEMAX FHD") == 1080, "FHD = 1080");
  OK(nv_res_do_texto("RO| CINEMAX HD") == 720, "HD = 720");
  OK(nv_res_do_texto("HBO 4K") == 2160 && nv_res_do_texto("Sport UHD") == 2160, "4K/UHD = 2160");
  OK(nv_res_do_texto("Globo SD") == 480 && nv_res_do_texto("TVR 576p") == 480, "SD/576 = 480");
  OK(nv_res_do_texto("RO| CINEMAX RAW 1080p") == 1080, "1080p no nome");
  OK(nv_res_do_texto("HDTV Canal") == 0 && nv_res_do_texto("SDTV") == 0, "HDTV/SDTV nao sao marca");
  OK(nv_res_do_texto("FrostView FHD") == 1080 && nv_res_do_texto("FrostView HD+") == 720, "rotulos de addon");
  nv_nome_base("RO| CINEMAX FHD", b, sizeof b);
  OK(!strcmp(b, "ro| cinemax"), "nome-base tira FHD e baixa a caixa");
  OK(base("RO| CINEMAX FHD", "RO| Cinemax HD") && base("RO| CINEMAX FHD", "RO|  CINEMAX  SD"), "variantes casam");
  OK(base("DIGI SPORT 1 HD", "DIGI SPORT 1 FHD HEVC"), "codec sai do nome-base");
  OK(!base("DIGI SPORT 1 HD", "DIGI SPORT 2 HD"), "canal diferente nao casa");
  OK(!base("RO| CINEMAX FHD", "RO| CINEMAX 2 FHD"), "Cinemax 2 nao e Cinemax");
  OK(nv_res_opcao_altura(2) == 1080 && nv_res_altura_opcao(720) == 3, "opcao <-> altura");
  { LtdCanal c[4]; LtdRecomendacao r;
    memset(c, 0, sizeof c);
    // C4 do pasha (registros 14195/13526): TS com buffer sem decoder; um
    // abriu em 14,2 s. HLS tocou em 4 s.
    c[0] = (LtdCanal){ 1, 0, 1, LTD_SEM_DECODER, 0, 300, 9000, 0, 0 };
    c[1] = (LtdCanal){ 1, 1, 1, LTD_OK, 14200, 280, 8000, 720, 0 };
    c[2] = (LtdCanal){ 0, 1, 1, LTD_OK, 4000, 250, 7000, 1080, 0 };
    c[3] = (LtdCanal){ 0, 1, 1, LTD_OK, 5000, 260, 10000, 1080, 0 };
    ltd_recomendar(c, 4, &r);
    OK(r.formato == 1, "HLS tocou mais: recomenda HLS");
    OK(r.resolucao == 3, "8-10 Mbps: 720p");
    OK(r.espera == 1, "um canal abriu em 14 s: espera de 25 s");
    OK(r.semDecoder == 1 && r.kbpsMediana == 9000 && r.latenciaMs == 280, "contagens e medianas");
    memset(c, 0, sizeof c);
    c[0] = (LtdCanal){ 1, 0, 0, LTD_SEM_TESTE, -1, 100, 50000, 0, 0 };
    ltd_recomendar(c, 1, &r);
    OK(r.formato == 0 && r.resolucao == 1 && r.espera == 0, "sem player (Mac): formato automatico, 4K pela vazao");
  }
  { char d[64];
    nv_dobrar("România Ș Ţ ÇÃO", d, sizeof d);
    OK(!strcmp(d, "romania s t cao"), "dobrar: acento e caixa saem");
    OK(nv_contem_dobrado("RO| PRO TV România", "romania"), "busca sem acento acha com acento");
    OK(nv_contem_dobrado("SporTV 2", "sportv") && !nv_contem_dobrado("ESPN", "sportv"), "busca sem caixa"); }
  if (falhas) return 1;
  printf("livetv_regras: tudo ok\n");
  return 0;
}
