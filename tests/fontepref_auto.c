// A FONTE QUE O AUTOMATICO ESCOLHEU TAMBEM E LEMBRADA (2.0.4, issue #310).
//
// Quem nunca abre a folha de fontes nao tinha nenhuma preferencia gravada, e o
// episodio seguinte (a mao ou pelo "proximo") caia na pontuacao, que pode
// trocar de release. Agora o bingeGroup da fonte que o AUTOMATICO escolheu e
// TOCOU fica guardado, em tabela e arquivo SEPARADOS da escolha manual:
//   - a manual sempre vence e nunca e enfraquecida;
//   - fontepref_tem / fontepref_do_titulo continuam sendo so da manual (e o
//     que decide se a folha abre e se "Retomar" toca direto);
//   - sem bingeGroup nada e guardado; nunca baixa a faixa de resolucao.
// Mesmo harness de tests/fontepref.c (NUVIO_DADOS temporario).
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../src/fontepref.c"

#include "dados.h"

static int falhas;
#define CONFERE(cond, ...) do { \
    if (cond) { printf("ok    "); } else { printf("FALHA "); falhas++; } \
    printf(__VA_ARGS__); printf("\n"); } while (0)

static void fonteB(Stream *s, const char *prov, const char *rot, const char *url,
                   int altura, int mp4, const char *binge) {
  memset(s, 0, sizeof *s);
  snprintf(s->provedor, sizeof s->provedor, "%s", prov);
  snprintf(s->rotulo, sizeof s->rotulo, "%s", rot);
  snprintf(s->descricao, sizeof s->descricao, "Serie.S01.%dp", altura);
  snprintf(s->url, sizeof s->url, "%s", url);
  s->altura = altura;
  s->mp4 = mp4;
  snprintf(s->bingeGroup, sizeof s->bingeGroup, "%s", binge);
}

// [0] 1080p MP4 sem grupo (a que a pontuacao escolhe), [1] 1080p do grupo X,
// [2] 1080p do grupo Y, todas na mesma faixa.
static int lista(Stream *v, int ep) {
  char u[100];
  snprintf(u, sizeof u, "https://exemplo.invalido/a/%d.mp4", ep);
  fonteB(&v[0], "Torrentio", "Torrentio\n1080p", u, 1080, 1, "");
  snprintf(u, sizeof u, "https://exemplo.invalido/x/%d.mp4", ep);
  fonteB(&v[1], "Outro Addon", "Outro 1080p", u, 1080, 1, "addon|grupoX");
  snprintf(u, sizeof u, "https://exemplo.invalido/y/%d.mp4", ep);
  fonteB(&v[2], "Terceiro", "Terceiro 1080p", u, 1080, 1, "addon|grupoY");
  return 3;
}

static int arquivoExiste(const char *dir, const char *nome) {
  char c[700];
  FILE *f;
  snprintf(c, sizeof c, "%s/%s", dir, nome);
  f = fopen(c, "rb");
  if (f) fclose(f);
  return f != NULL;
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  Stream v[4], tocou;
  int n, k;

  if (!dir || !dir[0]) { printf("fontepref_auto: NUVIO_DADOS ausente; recusando\n"); return 2; }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) { printf("fontepref_auto: dados_dir diverge; recusando\n"); return 2; }

  // ---- 1. auto tocou X no S1E1 -> S1E2 prefere X sobre a nao-X melhor ranqueada
  fontepref_definir_perfil(1);
  n = lista(v, 1);
  stream_definir_lista(v, n);
  CONFERE(stream_automatico() == 0, "a pontuacao escolheria a sem grupo (%d)", stream_automatico());
  tocou = v[1];
  CONFERE(fontepref_auto_guardar("tt3000001:1:1", &tocou) == 1, "S1E1: grupo X do automatico guardado");
  n = lista(v, 2);
  stream_definir_lista(v, n);
  CONFERE(stream_automatico() == 0, "no S1E2 a pontuacao ainda poria a sem grupo na frente");
  k = fontepref_escolher("tt3000001:1:2");
  CONFERE(k == 1, "S1E2 (a mao ou proximo): o grupo X e o preferido (%d)", k);

  // ---- 4. a automatica NAO e uma escolha da pessoa
  CONFERE(!fontepref_tem("tt3000001"), "fontepref_tem continua falso: a folha nao some");
  CONFERE(fontepref_do_titulo("tt3000001") == NULL, "fontepref_do_titulo e so da manual");
  CONFERE(fontepref_escolher_manual("tt3000001:1:2") == -1,
          "a leitura so-manual (folha, 'escolher ao reproduzir') nao a enxerga");
  CONFERE(!arquivoExiste(dir, "fontepref-p1.txt"), "o arquivo da manual nem foi criado");

  // ---- sem bingeGroup tambem e guardada (16/10): casa por provedor + audio,
  // como uma escolha a mao.
  CONFERE(fontepref_auto_guardar("tt3000002:1:1", &v[0]) == 1, "fonte sem bingeGroup: guardada");
  CONFERE(fontepref_escolher("tt3000002:1:2") == 0, "e o episodio seguinte vai no mesmo provedor e audio");

  // ---- outra serie / outro perfil nao herdam
  CONFERE(fontepref_escolher("tt3000009:1:2") == -1, "outra serie nao herda");
  fontepref_definir_perfil(2);
  CONFERE(fontepref_escolher("tt3000001:1:2") == -1, "outro perfil nao herda");
  fontepref_definir_perfil(1);

  // ---- sobrevive ao arranque; arquivo proprio
  CONFERE(arquivoExiste(dir, "fontepref-auto-p1.txt"), "arquivo proprio da automatica");
  nTab = 0; carregado = 0;
  k = fontepref_escolher("tt3000001:1:3");
  CONFERE(k == 1, "depois do arranque frio o grupo X continua valendo (%d)", k);

  // ---- grupo ausente na lista: cai calado e NAO apaga a memoria
  stream_definir_lista(v, 1);   // so a sem grupo
  CONFERE(fontepref_escolher("tt3000001:1:4") == -1, "grupo ausente: segue a pontuacao, calado");
  n = lista(v, 5);
  stream_definir_lista(v, n);
  CONFERE(fontepref_escolher("tt3000001:1:5") == 1, "um episodio sem o grupo nao apagou a memoria");

  // ---- outro grupo que TOCOU substitui (a ultima release que funcionou)
  tocou = v[2];
  CONFERE(fontepref_auto_guardar("tt3000001:1:5", &tocou) == 1, "outro grupo tocou: substitui");
  CONFERE(fontepref_escolher("tt3000001:1:6") == 2, "e agora o Y e o preferido");

  // ---- 2. a manual vence e nao e sobrescrita
  fontepref_definir_perfil(3);
  n = lista(v, 1);
  stream_definir_lista(v, n);
  CONFERE(fontepref_guardar("tt3000003:1:1", &v[2]) == 1, "manual: Y escolhido na folha");
  tocou = v[1];
  CONFERE(fontepref_auto_guardar("tt3000003:1:1", &tocou) == 0, "automatico tocou X: nao grava com manual");
  n = lista(v, 2);
  stream_definir_lista(v, n);
  CONFERE(fontepref_escolher("tt3000003:1:2") == 2, "e a manual Y ainda vence");
  CONFERE(fontepref_do_titulo("tt3000003") && !strcmp(fontepref_do_titulo("tt3000003")->bingeGroup, "addon|grupoY"),
          "a linha manual segue com o grupo Y");
  // automatica anterior + manual nova: a manual apaga a automatica
  fontepref_definir_perfil(4);
  CONFERE(fontepref_auto_guardar("tt3000004:1:1", &v[1]) == 1, "perfil 4: automatica X");
  CONFERE(fontepref_guardar("tt3000004:1:1", &v[2]) == 1, "e depois a pessoa escolhe Y");
  stream_definir_lista(v, n);
  CONFERE(fontepref_escolher("tt3000004:1:2") == 2, "a manual Y vence");
  { Stream so[1]; so[0] = v[0];
    stream_definir_lista(so, 1);
    CONFERE(fontepref_escolher("tt3000004:1:2") == -1, "e a automatica antiga nao ressurge por baixo"); }

  // ---- nunca baixa a faixa: grupo em 1080p, a pontuacao tem 2160p
  fontepref_definir_perfil(5);
  n = lista(v, 1);
  stream_definir_lista(v, n);
  CONFERE(fontepref_auto_guardar("tt3000005:1:1", &v[1]) == 1, "perfil 5: automatica X");
  n = lista(v, 2);
  fonteB(&v[3], "Torrentio", "Torrentio\n4k", "https://exemplo.invalido/4k/2.mp4", 2160, 1, "");
  stream_definir_lista(v, 4);
  CONFERE(stream_automatico() == 3, "a pontuacao escolhe a 4K (%d)", stream_automatico());
  CONFERE(fontepref_escolher("tt3000005:1:2") == 1,
          "o grupo X (1080p) que ja tocou vence a 4K (16/10: qualidade nao tira a que tocou)");

  // ---- a regra do MP4 da LG (2.0.3) fica ACIMA: grupo em MKV nao ganha do MP4
  // da mesma faixa. Fora da LG a regra nao existe e o grupo vale.
  fontepref_definir_perfil(7);
  n = lista(v, 1);
  stream_definir_lista(v, n);
  CONFERE(fontepref_auto_guardar("tt3000007:1:1", &v[1]) == 1, "perfil 7: automatica X");
  n = lista(v, 2);
  v[1].mp4 = 0; snprintf(v[1].url, sizeof v[1].url, "https://exemplo.invalido/x/2.mkv");
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt3000007:1:2");
#if !defined(NV_ANDROID) && !defined(NV_TPK) && !defined(__EMSCRIPTEN__) && !defined(NV_LINUX_DESKTOP)
  CONFERE(k == -1, "LG: o grupo X em MKV nao passa na frente do MP4 da faixa (%d)", k);
#else
  CONFERE(k == 1, "fora da LG o grupo X em MKV vale (%d)", k);
#endif

  // ---- manual preferences file: has one more field, and old parsers degrade cleanly.
  // 7-field lines from 2.0.3 remain readable.
  fontepref_definir_perfil(6);
  n = lista(v, 1);
  stream_definir_lista(v, n);
  CONFERE(fontepref_guardar("tt3000006:1:1", &v[1]) == 1, "perfil 6: manual gravada");
  { char c[700]; char linha[600]; FILE *f; int campos = 0; char *p;
    snprintf(c, sizeof c, "%s/fontepref-p6.txt", dir);
    f = fopen(c, "rb");
    CONFERE(f != NULL, "arquivo da manual existe");
    if (f) {
      fgets(linha, sizeof linha, f);
      CONFERE(!strcmp(linha, "# nuvio fontes v1\n"), "cabecalho v1 intacto");
      fgets(linha, sizeof linha, f);
      for (p = linha; *p; p++) if (*p == '\t') campos++;
      CONFERE(campos == 9, "ten fields, tier (SeaDex/Remux T1) last (%d tabs)", campos);
      fclose(f); } }

  // ---- sair da conta apaga tambem a automatica
  fontepref_esquecer();
  CONFERE(!arquivoExiste(dir, "fontepref-auto-p1.txt"), "sair da conta apaga a automatica");
  CONFERE(fontepref_escolher("tt3000001:1:2") == -1, "e a memoria some");

  printf(falhas ? "fontepref_auto: %d falha(s)\n" : "fontepref_auto: tudo ok\n", falhas);
  return falhas ? 1 : 0;
}
