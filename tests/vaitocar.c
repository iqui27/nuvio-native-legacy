// 2.1, tela 4 "Titulo e Fontes": o TEXTO da faixa "Vai tocar", o nome amigavel,
// a origem das marcas, o cartao da escolha automatica e o agrupamento de copias
// do mesmo arquivo. Sem GL e sem rede: so a lista em memoria e o cache de marcas.
#include "streams.h"
#include "vaitocar.h"
#include "ajustes.h"
#include "badges.h"
#include "intro.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void fonte(Stream *s, const char *prov, const char *rot, const char *arq,
                  const char *desc, int altura, int mp4, unsigned long long mb) {
  memset(s, 0, sizeof *s);
  snprintf(s->provedor, sizeof s->provedor, "%s", prov);
  snprintf(s->rotulo, sizeof s->rotulo, "%s", rot);
  snprintf(s->arquivo, sizeof s->arquivo, "%s", arq);
  snprintf(s->descricao, sizeof s->descricao, "%s", desc);
  snprintf(s->url, sizeof s->url, "https://exemplo.invalido/%s-%s", prov, arq);
  s->altura = altura; s->mp4 = mp4; s->fileIdx = -1;
  s->tamanhoMB = (long)mb; s->tamanhoBytes = mb << 20;
  { char t[3200]; snprintf(t, sizeof t, "%s %s %s", rot, arq, desc); s->badges = badges_detectar(t); }
}

static int tem(const char *s, const char *agulha) { return strstr(s, agulha) != NULL; }

int main(void) {
  char dir[] = "/tmp/nv-vaitocar-XXXXXX", caminho[600];
  Stream v[6];
  VaiTocar vt;
  VtEscolha e;
  char nome[256];
  FILE *f;
  IntroTrecho tr[2];
  int origem, k, fora = 0, copias = 0;

  assert(mkdtemp(dir));
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  f = fopen(caminho, "w"); assert(f);
  fprintf(f, "idioma 0\npularAberturaLocal 0\n");
  fclose(f);
  ajustes_dir(dir);
  ajustes_iniciar();
  badges_carregar("deploy/app/art");

  const char *ARQ = "Silo.S02E05.2160p.WEB-DL.DV.HDR.Atmos.mkv";
  fonte(&v[0], "Torrentio", "Torrentio 4k", ARQ, "Silo 2160p WEB-DL DV HDR Atmos 5.1", 2160, 0, 11264);
  fonte(&v[1], "AIOStreams", "Silo S02 E05", ARQ, "mesmo arquivo, outro add-on", 2160, 0, 11264);   // copia de 0
  fonte(&v[2], "MediaFusion", "1080p WEB-DL", "Silo.2024.S02E05.WEB-DL.1080p.mkv", "WEB-DL 1080p", 1080, 0, 4096);
  fonte(&v[3], "Comet", "Comet 2160p", ARQ, "mesmo nome, outro tamanho", 2160, 0, 9000);          // NAO e copia
  fonte(&v[4], "Comet", "Comet curto", "S02E05.mkv", "nome curto demais", 1080, 0, 2048);
  fonte(&v[5], "Torrentio", "Torrentio curto", "S02E05.mkv", "nome curto demais", 1080, 0, 2048); // NAO: < 16 letras

  stream_definir_alvo("tt14688458:2:5");
  stream_definir_lista(v, 6);

  // --- copias do mesmo arquivo (#457/#458) ---
  assert(vt_copias_mesmas(0, 1) == 1);
  assert(vt_copias_mesmas(0, 3) == 0);   // tamanho diferente
  assert(vt_copias_mesmas(0, 2) == 0);   // outro arquivo
  assert(vt_copias_mesmas(4, 5) == 0);   // nome curto nunca agrupa
  assert(vt_copia_rep(1) == 0 && vt_copia_rep(0) == 0 && vt_copia_rep(3) == 3);
  assert(vt_copias_n(0) == 1 && vt_copias_n(1) == 0);

  // --- descricao da fonte ---
  memset(&vt, 0, sizeof vt);
  vt_descrever(&v[0], &vt);
  assert(vt.tem);
  assert(!strcmp(vt.res, "4K"));
  assert(!strcmp(vt.tipo, "WEB-DL"));
  assert(vt.hdr[0]);
  assert(!strcmp(vt.tamanho, "11,0 GB") || !strcmp(vt.tamanho, "11.0 GB"));
  assert(!strcmp(vt.provedor, "Torrentio"));
  vt_nome_amigavel(&vt, nome, sizeof nome);
  assert(tem(nome, "WEB-DL") && tem(nome, "4K"));
  assert(!tem(nome, ".mkv") && !tem(nome, "Silo."));   // frase amigavel, nao o nome cru

  // --- origem das marcas ---
  assert(!strcmp(vt_origem_rotulo(VT_O_NENHUMA), ""));
  assert(tem(vt_origem_rotulo(VT_O_THEINTRODB), "TheIntroDB"));
  assert(tem(vt_origem_rotulo(VT_O_ANISKIP), "AniSkip"));
  assert(tem(vt_origem_rotulo(VT_O_MISTA), "TheIntroDB"));

  // --- marcas lembradas: so o que a rede respondeu; 0 = sem dado ---
  assert(vt_pular_ligado());
  assert(vt_marcas_episodio("tt14688458", 2, 5) == 0);
  memset(tr, 0, sizeof tr);
  tr[0].tipo = INTRO_ABERTURA; tr[0].inicio = 0; tr[0].fim = 90;
  tr[1].tipo = INTRO_CREDITOS; tr[1].inicio = 3000; tr[1].fim = 3300;
  intro_shot_marcas("tt14688458", 2, 5, tr, 2, 0);
  assert(vt_marcas_episodio("tt14688458", 2, 5) == (VT_T_A | VT_T_C));
  assert(vt_marcas_episodio("tt14688458", 2, 6) == 0);   // outro episodio: sem dado
  assert(intro_marcas("tt14688458", 2, 5, &origem) && origem == INTRO_O_THEINTRODB);
  tr[0].ani = 1;
  intro_shot_marcas("tt14688458", 3, 1, tr, 1, 0);
  assert(intro_marcas("tt14688458", 3, 1, &origem) == VT_T_A && origem == VT_O_ANISKIP);

  // --- faixa montada do que o automatico escolhe ---
  assert(vt_montar(&vt, "tt14688458", 2, 5) == 1);
  assert(vt.tem && vt.pularOn && vt.marcas == (VT_T_A | VT_T_C) && vt.origem == VT_O_THEINTRODB);
  assert(vt_montar(&vt, "tt14688458", 2, 6) == 0);   // lista e do 2:5, nao do 2:6
  assert(vt_montar(&vt, "tt999", 1, 1) == 0 && !vt.tem);                // lista de outro alvo

  // --- cartao da escolha automatica ---
  assert(vt_escolha(&e) == 1 && e.tem);
  assert(e.indice >= 0 && e.indice < 6 && e.nome[0] && e.prov[0]);
  assert(e.nLinha >= 1 && e.nLinha <= VT_CRITERIA_MAX);
  for (k = 0; k < e.nLinha; k++) {
    assert(e.linha[k][0]);
    if (e.fora[k]) fora++;
    if (tem(e.linha[k], "Cópias do mesmo arquivo agrupadas: 1")) copias++;
  }
  assert(fora >= 1 && copias == 1);   // o que ficou de fora vem DEPOIS dos criterios cumpridos
  for (k = 1; k < e.nLinha; k++) assert(!(e.fora[k - 1] && !e.fora[k]));

  printf("ok vaitocar (escolha: %s, %d linhas, %d fora)\n", e.nome, e.nLinha, fora);
  return 0;
}
