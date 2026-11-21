#!/bin/bash
# Farol — produtores player/LG (issue 7; PLANO.md 2.2, 5). Tres provas:
#  (a) estatica: cada ponto da tabela tem a chamada farol_ev em src/player.c /
#      src/video.c e nenhum argumento e url, titulo, playUrl, c->titulo ou
#      literal com %s — o anel so aceita tokens; FE_PRONTO tem de estar sob a
#      guarda do primeiro quadro (amostra nativa, nao o alvo de seek pendente)
#      e o rearm junto do FE_ABRIR, num ponto unico que a troca de fonte e a
#      recarga manual do VOD compartilham; nenhum farol_ev pode receber fe.cod
#      cru — so via farolcod_anel (lista fechada);
#  (b) harness: o trecho de tratamento de erro do uMS (numeroDe, farolUmsErro),
#      o leitor/publicador do modelo (farolTvModeloLer, edidResposta,
#      video_tv_modelo), a abertura (farolAbrir/farolRearmar), a recarga VOD
#      (player_recarregar_vod -> video_recarregar -> recarregarMesmaFonte,
#      chamada por recarregarFonteVOD), o video_buscar e os blocos de primeiro
#      quadro e de erro do player sao extraidos de src/app.c/src/video.c/
#      src/player.c e rodam contra respostas JSON reais — errorCode 200, 204,
#      40403 e "is not running" — conferindo os eventos no anel, o FE_PRONTO
#      so na primeira amostra nativa, o rearm na segunda abertura e na
#      recarga, o `a` medido da fonte corrente e, no alvo .wgt, o canario apos
#      "player_error" fora do anel (tests/farol_produtores_lg.c);
#  (c) corrida: o callback do EDID e o getter do modelo em fios concorrentes,
#      sob TSan quando o compilador oferece.
set -eu
cd "$(dirname "$0")/.."

falhou=0
falha() { echo "farol_produtores_lg: FALHA - $1" >&2; falhou=1; }

# (a) cada ponto listado tem a chamada; nenhum argumento proibido; o cod que
# vai ao anel passa pela lista fechada; FE_PRONTO so sob a guarda de quadro.
python3 - <<'PYEOF' || falhou=1
import re, sys
pts = {
    "src/player.c": ["FE_ABRIR", "FE_PRONTO", "FE_ERRO_FONTE",
                     "FE_RECUSA_SINC", "FE_PARADO"],
    "src/video.c":  ["FE_ERRO_NATIVO", "FE_AUDIO_RECUSADO",
                     "FE_PIPELINE_MORREU", "FE_RECONEXAO"],
}
proib = re.compile(r"url|titulo|%s", re.I)
ruim = 0
for arq, eventos in pts.items():
    src = open(arq, encoding="utf-8").read()
    chamadas = re.findall(r"farol_ev\s*\((.*?)\)\s*;", src, re.S)
    if not chamadas:
        print("farol_produtores_lg: nenhum farol_ev em %s" % arq, file=sys.stderr)
        ruim = 1
        continue
    juntos = " ".join(chamadas)
    for ev in eventos:
        if ev not in juntos:
            print("farol_produtores_lg: %s sem chamada %s" % (arq, ev), file=sys.stderr)
            ruim = 1
    for c in chamadas:
        if proib.search(c):
            print("farol_produtores_lg: argumento proibido em farol_ev(%s) de %s"
                  % (c.replace("\n", " "), arq), file=sys.stderr)
            ruim = 1
        # cod nativo so chega ao anel filtrado pela lista fechada.
        if re.search(r"\bfe\.cod\b", c):
            print("farol_produtores_lg: fe.cod cru em farol_ev(%s) de %s"
                  % (c.replace("\n", " "), arq), file=sys.stderr)
            ruim = 1
    if "farolcod_anel" not in src:
        print("farol_produtores_lg: %s sem o filtro farolcod_anel" % arq,
              file=sys.stderr)
        ruim = 1

player = open("src/player.c", encoding="utf-8").read()
video = open("src/video.c", encoding="utf-8").read()

# Toda abertura que grava FE_ABRIR rearma o primeiro quadro.
abrir = re.search(r"^static void farolAbrir\(void\) \{(.*?)^\}",
                  player, re.M | re.S)
if not abrir or "FE_ABRIR" not in abrir.group(1) \
        or "inicioImagem = 0" not in abrir.group(1) \
        or "prontoAnel = 0" not in abrir.group(1):
    print("farol_produtores_lg: farolAbrir sem FE_ABRIR + rearm", file=sys.stderr)
    ruim = 1
# O rearm do ciclo e um ponto UNICO (farolRearmar): FE_ABRIR, primeiro quadro
# pendente e o relogio da fonte corrente. Trocar a fonte e recarregar o VOD
# passam por ele; a recarga so rearma com a carga aceita pelo backend.
rearm = re.search(r"^static void farolRearmar\(void\) \{(.*?)^\}",
                  player, re.M | re.S)
if not rearm or "farolAbrir();" not in rearm.group(1) \
        or "loadEm = SDL_GetTicks()" not in rearm.group(1) \
        or "memQuadroPendente = 1" not in rearm.group(1):
    print("farol_produtores_lg: farolRearmar sem o rearm completo do ciclo",
          file=sys.stderr)
    ruim = 1
interno = re.search(r"^static void tocarFonteInterno\(const char \*url\) \{(.*?)^\}",
                    player, re.M | re.S)
if not interno or "farolRearmar();" not in interno.group(1):
    print("farol_produtores_lg: tocarFonteInterno sem farolRearmar()", file=sys.stderr)
    ruim = 1
recarga = re.search(r"^int player_recarregar_vod\(void\) \{(.*?)^\}",
                    player, re.M | re.S)
if not recarga or "video_recarregar()" not in recarga.group(1) \
        or "farolRearmar();" not in recarga.group(1):
    print("farol_produtores_lg: player_recarregar_vod sem backend + rearm",
          file=sys.stderr)
    ruim = 1
app = open("src/app.c", encoding="utf-8").read()
refontevod = re.search(r"^static void recarregarFonteVOD\(void\) \{(.*?)^\}",
                       app, re.M | re.S)
if not refontevod or "player_recarregar_vod()" not in refontevod.group(1):
    print("farol_produtores_lg: recarregarFonteVOD fora de player_recarregar_vod",
          file=sys.stderr)
    ruim = 1
# player_definir_fonte (troca direta, ex.: renovacao Stalker) passa por
# tocarFonte -> tocarFonteInterno: a abertura direta emite FE_ABRIR e rearma.
definir = re.search(r"^void player_definir_fonte\(const char \*url\) \{(.*?)^\}",
                    player, re.M | re.S)
if not definir or "tocarFonte(url);" not in definir.group(1):
    print("farol_produtores_lg: player_definir_fonte sem tocarFonte(url)",
          file=sys.stderr)
    ruim = 1

# FE_PRONTO: dentro do bloco do primeiro quadro (tocando + AMOSTRA nativa de
# posicao — o alvo gravado por um seek pendente nao conta), nao no bloco do
# pronto do backend. O `a` mede da abertura da fonte corrente (loadEm).
quadro = re.search(r"^  if \(memQuadroPendente && comVideo(.*?)^  \}",
                   player, re.M | re.S)
if not quadro or "FE_PRONTO" not in quadro.group(1) \
        or "video_tocando()" not in quadro.group(0) \
        or "video_pos_amostra(NULL) > 0.0" not in quadro.group(0):
    print("farol_produtores_lg: FE_PRONTO fora da guarda da amostra nativa",
          file=sys.stderr)
    ruim = 1
if quadro and "loadEm ? loadEm : abertoEm" not in quadro.group(0):
    print("farol_produtores_lg: FE_PRONTO sem o relogio da fonte corrente",
          file=sys.stderr)
    ruim = 1
pronto = re.search(r"^  if \(!inicioImagem && comVideo && video_pronto\(\)\) \{(.*?)^  \}",
                   player, re.M | re.S)
if pronto and "FE_PRONTO" in pronto.group(1):
    print("farol_produtores_lg: FE_PRONTO ainda sai no pronto do backend",
          file=sys.stderr)
    ruim = 1

# O modelo publica sob trava e o getter devolve copia do proprio fio —
# ponteiro para o vetor compartilhado nao sai da funcao.
if "tvModeloTrava" not in video or "_Thread_local" not in video:
    print("farol_produtores_lg: video.c sem a publicacao sincronizada do modelo",
          file=sys.stderr)
    ruim = 1
sys.exit(ruim)
PYEOF

# (b) harness: extrai os trechos de producao e roda contra os JSONs reais.
awk '/^static double numeroDe\(const char \*p, const char \*chave\) \{/,/^}/' \
    src/video.c > tests/farol_ums.inc
awk '/^static void farolUmsErro\(const char \*p, double posicaoS, char \*erroTxt, size_t nErro\) \{/,/^}/' \
    src/video.c >> tests/farol_ums.inc
awk '/^static void farolTvModeloLer\(const char \*json, char \*out, size_t nOut\) \{/,/^}/' \
    src/video.c >> tests/farol_ums.inc
grep -q "farolUmsErro" tests/farol_ums.inc || falha "extracao de video.c vazia"
grep -q "farolTvModeloLer" tests/farol_ums.inc || falha "leitor de modelo nao extraido"

# 7B: publicacao do modelo (buffer duplo + callback), a abertura com rearm, o
# bloco do primeiro quadro com o FE_PRONTO e os dois produtores de erro com o
# filtro de cod — tudo codigo de producao, extraido por nome.
{
  grep -E "^static .*tvModelo" src/video.c
  awk '/^const char \*video_tv_modelo\(void\) \{$/,/^}$/' src/video.c
  awk '/^static int edidResposta\(LSHandle \*h, LSMessage \*m, void \*u\) \{/,/^}/' \
      src/video.c
  python3 - <<'PYEOF2'
import re
player = open("src/player.c", encoding="utf-8").read()
app = open("src/app.c", encoding="utf-8").read()
video = open("src/video.c", encoding="utf-8").read()
macros = re.search(r"^#if defined\(NV_ANDROID\)\n#  define NV_FAROL_PLAT.*?^#endif",
                   player, re.M | re.S)[0]
abrir = re.search(r"^static void farolAbrir\(void\) \{.*?^\}",
                  player, re.M | re.S)[0]
rearm = re.search(r"^static void farolRearmar\(void\) \{.*?^\}",
                  player, re.M | re.S)[0]
# A cadeia da recarga manual do VOD, na ordem de quem chama quem: o botao do
# app (recarregarFonteVOD) -> o rearm do anel (player_recarregar_vod) -> o
# backend (video_recarregar -> recarregarMesmaFonte) — mais o seek que grava
# o alvo antes do pipeline (video_buscar), o caso da retomada.
reabre = re.search(r"^static int recarregarMesmaFonte\(double alvo, int aud, int leg, const char \*legUrl\) \{.*?^\}",
                   video, re.M | re.S)[0]
vrecarga = re.search(r"^int video_recarregar\(void\) \{.*?^\}",
                     video, re.M | re.S)[0]
vbuscar = re.search(r"^void video_buscar\(double segundos\) \{.*?^\}",
                    video, re.M | re.S)[0]
precarrega = re.search(r"^int player_recarregar_vod\(void\) \{.*?^\}",
                       player, re.M | re.S)[0]
refontevod = re.search(r"^static void recarregarFonteVOD\(void\) \{.*?^\}",
                       app, re.M | re.S)[0]
quadro = re.search(r"^  if \(memQuadroPendente && comVideo.*?^  \}",
                   player, re.M | re.S)[0]
pronto = re.search(r"^  if \(!inicioImagem && comVideo && video_pronto\(\)\) \{.*?^  \}",
                   player, re.M | re.S)[0]
erro = re.search(r"\{ int http = 0, causa = fontesonda_causa[^}]*FE_ERRO_FONTE[^}]*\}",
                 player, re.S)[0]
recusa = re.search(r"\{ int http = 0, causa = fontesonda_causa[^}]*FE_RECUSA_SINC[^}]*\}",
                   player, re.S)[0]
print(macros)
print(abrir)
print(rearm)
print(reabre)
print(vrecarga)
print(precarrega)
print(refontevod)
print(vbuscar)
print("static void quadro_atualizar(unsigned agora) {")
print(quadro)
print(pronto)
print("  (void)agora;\n}")
print("static void emitir_erro_fonte(void) {")
print(erro)
print("}")
print("static void emitir_recusa(void) {")
print(recusa)
print("}")
PYEOF2
} >> tests/farol_ums.inc
grep -q "tvModeloTrava" tests/farol_ums.inc || falha "publicacao do modelo nao extraida"
grep -q "edidResposta" tests/farol_ums.inc || falha "callback do EDID nao extraido"
grep -q "farolAbrir" tests/farol_ums.inc || falha "abertura com rearm nao extraida"
grep -q "farolRearmar" tests/farol_ums.inc || falha "ponto unico de rearm nao extraido"
grep -q "player_recarregar_vod" tests/farol_ums.inc || falha "recarga VOD do player nao extraida"
grep -q "recarregarFonteVOD" tests/farol_ums.inc || falha "botao de recarga do app.c nao extraido"
grep -q "recarregarMesmaFonte" tests/farol_ums.inc || falha "reabertura do backend nao extraida"
grep -q "video_buscar" tests/farol_ums.inc || falha "seek pendente nao extraido"
grep -q "emitir_recusa" tests/farol_ums.inc || falha "produtores de erro nao extraidos"
grep -q "video_pos_amostra(NULL) > 0.0" tests/farol_ums.inc || \
  falha "guarda da amostra nativa ausente"
trap 'rm -f tests/farol_ums.inc' EXIT

bin="${TMPDIR:-/tmp}/nuvio-farol-prod-lg-test"
cc -Wall -Wextra -O1 -g -Itests -Isrc -pthread \
   tests/farol_produtores_lg.c src/farolanel.c src/farolerro.c src/fontesonda.c \
   -o "$bin"
"$bin"
rm -f "$bin"

# O produtor do .wgt: o MESMO harness compilado como __EMSCRIPTEN__ — o macro
# NV_FAROL_PLAT vira FP_TIZEN_WGT e o bloco extraido roda o caminho do AVPlay.
cc -Wall -Wextra -O1 -g -D__EMSCRIPTEN__ -Itests -Isrc -pthread \
   tests/farol_produtores_lg.c src/farolanel.c src/farolerro.c src/fontesonda.c \
   -o "$bin"
"$bin"
rm -f "$bin"

# O harness roda uma vez por compilacao; com ASan+UBSan quando houver suporte.
case "${SANITIZE:-1}" in
  1)
    cc -fsanitize=address,undefined -fno-omit-frame-pointer \
       -Wall -Wextra -O1 -g -Itests -Isrc -pthread \
       tests/farol_produtores_lg.c src/farolanel.c src/farolerro.c src/fontesonda.c \
       -o "$bin"
    "$bin"
    rm -f "$bin"
    ;;
esac

# (c) corrida do callback do EDID com o getter do modelo, sob TSan.
if command -v clang >/dev/null 2>&1; then
  clang -fsanitize=thread -fno-omit-frame-pointer \
        -Wall -Wextra -O1 -g -Itests -Isrc -pthread \
        tests/farol_produtores_lg.c src/farolanel.c src/farolerro.c src/fontesonda.c \
        -o "$bin"
  "$bin" race
  rm -f "$bin"
fi

[ "$falhou" = 0 ] || exit 1
echo "farol_produtores_lg: tudo ok"
