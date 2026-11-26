# Gera o C do teste capfonte: executa o codigo REAL de publicacao (lerMkv do
# .wgt), limpeza de troca de fonte e leitura (credEfetivo/posplay) compilando
# os trechos extraidos dos fontes — mesmo metodo do ensaio da revisao r2.
#
#   python3 tests/capfonte.py <raiz-do-repo> > capfonte.c
#
# Cenarios (F57):
#   1. troca de fonte DURANTE a sonda .wgt: lerMkv lia "a.mkv" quando a tela ja
#      abriu "b.mp4" — capitulos e capmkv nao podem sujar a fonte nova;
#   2. titulo seguinte sem capitulos: estado capmkv/local do MKV anterior nao
#      pode virar marcador do MP4 (efetivo/painel 0; episodio abre em 285 s);
#   3. HTML5 (--pthreads): o capitulo nomeado guardado so nos locais tem de
#      chegar ao credEfetivo (episodio abre no marcador, nao na estimativa);
#   4. precedencia no painel do filme: nomeado do arquivo > marcador de
#      provedor (intro_creditos_seg).
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
tizen = (root / "src/video_tizen.c").read_text()
html5 = (root / "src/video_html5.c").read_text()
player = (root / "src/player.c").read_text()
posplay = (root / "src/posplay.c").read_text()


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index("\n}", start) + 2]


publish = function(tizen[tizen.index("// Le o cabecalho do Matroska"):],
                   "static void *lerMkv(void *arg) {")
# O bloco de publicacao inteiro — com ou sem a guarda de troca de fonte, que e
# justamente o que o cenario 1 mede.
pubblock = re.search(r"if \(nCaps > 0.*?\n  \}", publish, re.S)[0]
html_publish = function(html5[html5.index("static void *lerMkv(void *arg) {"):],
                        "static void *lerMkv(void *arg) {")
assert "capmkv_" not in html_publish
section = function(posplay, "void posplay_atualizar(float")
selection = re.search(r"double creditosSeg = .*?(?=  anim =)", section, re.S)[0]
film_half = re.search(r"#define PP_FILME_METADE[^\n]*", posplay)[0]

print(f'#define main intro_fixture\n#include "{root / "tests/intro.c"}"\n#undef main')
print(f'#include "{root / "src/catalogo.h"}"')
print("""
static double creditosNomeado, creditosUltimo, duracaoSeg;
static char urlAtual[8192];
static int epT, epE, credFonteAtual;
static const CatItem *item(void) { return NULL; }
static double cred_aprendido_resto(const char *imdb) { (void)imdb; return 0; }
static int video_pronto(void) { return 0; }
static double video_duracao(void) { return duracaoSeg; }
double video_creditos_nomeado(void);
double video_creditos_ultimo(double dur);
""")
print(function(tizen, "double video_creditos(void) {"))
print(function(html5, "double video_creditos(void) {").replace(
    "double video_creditos(void)", "double video_creditos_html5(void)", 1))
for signature in ("double video_creditos_nomeado(void) {", "double video_creditos_ultimo(double dur) {"):
    print(function(tizen, signature))
    print(function(html5, signature).replace("video_creditos_", "video_creditos_html5_"))
effective = function(player, "static double credEfetivo(void) {")
print(effective)
print(effective.replace("credEfetivo(", "credEfetivoHtml5(")
      .replace("video_creditos", "video_creditos_html5"))
print(film_half)
print(function(posplay, "static int credAceito(double"))
print(function(posplay, "int posplay_regra_filme(double"))
print("static double credJanelaDe(double durSeg) { return intro_creditos_janela(durSeg); }")
print(function(player, "int player_regra_proximo(double"))
print("static double creditoPainel(void) {", selection, "return creditosSeg; }")
print(f"""
// A sonda .wgt publica depois de ler a rede inteira: `u` e a URL que o fio
// copiou no disparo; urlAtual e o que a tela toca AGORA.
static void sondaPublica(const char *u, MkvCap *caps, int nCaps) {{
  char url[8192];
  unsigned capGer = capmkv_geracao();
  snprintf(url, sizeof url, "%s", u);
  {pubblock}
}}

int main(void) {{
  MkvCap caps[] = {{{{0, "Chapter 1"}}, {{950, "End Credits"}}}};

  // 1) Troca de fonte DURANTE a sonda: o fio lia "a.mkv" e a tela ja abriu
  //    "b.mp4". O resultado da leitura nao pode sujar a fonte nova.
  snprintf(urlAtual, sizeof urlAtual, "b.mp4");
  sondaPublica("a.mkv", caps, 2);
  printf("troca na sonda: local %.0f, capmkv %.0f\\n",
         creditosNomeado, capmkv_creditos_nomeado());
  assert(creditosNomeado == 0.0);
  assert(capmkv_creditos_nomeado() == 0.0);

  // 2) Sonda da fonte certa publica; o titulo SEGUINTE (MP4 sem capitulos)
  //    nao herda o marcador: video_tocar = intro_desligar + zerar locais.
  snprintf(urlAtual, sizeof urlAtual, "a.mkv");
  sondaPublica("a.mkv", caps, 2);
  duracaoSeg = 1000;
  printf("MKV: local %.0f, efetivo %.0f, painel %.0f\\n",
         video_creditos(), credEfetivo(), creditoPainel());
  assert(credEfetivo() == 950.0 && creditoPainel() == 950.0);
  intro_desligar();
  capmkv_zerar();
  creditosNomeado = creditosUltimo = 0.0;
  duracaoSeg = 1200;
  printf("MP4 seguinte: local %.0f, efetivo %.0f, painel %.0f; filme@950=%d, episodio@950=%d\\n",
         video_creditos(), credEfetivo(), creditoPainel(),
         posplay_regra_filme(950, duracaoSeg, creditoPainel()),
         player_regra_proximo(950, duracaoSeg, credEfetivo()));
  assert(credEfetivo() == 0.0 && creditoPainel() == 0.0);
  assert(posplay_regra_filme(950, duracaoSeg, creditoPainel()) == 0);
  assert(player_regra_proximo(950, duracaoSeg, credEfetivo()) == 0);
  assert(posplay_regra_filme(1110, duracaoSeg, creditoPainel()) == 1);
  duracaoSeg = 300;
  printf("MP4 300s: episodio@285=%d\\n",
         player_regra_proximo(285, duracaoSeg, credEfetivo()));
  assert(player_regra_proximo(285, duracaoSeg, credEfetivo()) == 1);

  // 3) HTML5 com pthreads: o nomeado mora so nos locais do backend e tem de
  //    chegar ao credEfetivo (o card do episodio abre no marcador).
  capmkv_zerar();
  intro_desligar();
  creditosNomeado = 880;
  duracaoSeg = 1000;
  printf("HTML5 MKV: local %.0f, efetivo %.0f; episodio@880=%d\\n",
         video_creditos_html5(), credEfetivoHtml5(),
         player_regra_proximo(880, duracaoSeg, credEfetivoHtml5()));
  assert(video_creditos_html5() == 880.0);
  assert(credEfetivoHtml5() == 880.0);
  assert(player_regra_proximo(880, duracaoSeg, credEfetivoHtml5()) == 1);

  // 4) Painel do filme: o nomeado do arquivo ganha do marcador de provedor.
  {{ IntroTrecho prov[] = {{{{1100, 1200, INTRO_CREDITOS, 0, 0}}}};
    sondaPublica("a.mkv", caps, 2);
    intro_desligar();
    intro_definir_duracao(1200, 1);
    intro_shot_rede(prov, 1, NULL, 0);
    duracaoSeg = 1200;
    creditosNomeado = 950;
    assert(intro_creditos_seg() == 1100);
    printf("painel: nomeado 950 vs provedor %.0f -> %.0f\\n",
           intro_creditos_seg(), creditoPainel());
    assert(creditoPainel() == 950.0); }}
  intro_desligar();
  creditosNomeado = creditosUltimo = 0.0;
  printf("capfonte: ok\\n");
  return 0;
}}
""")
