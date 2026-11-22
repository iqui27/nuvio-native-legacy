#!/bin/bash
# Farol — amostra nativa TPK/TPK40 (issue 7, CORRECAO-FAROL7D). Duas provas:
#  (a) estatica: video_pos_amostra() de src/video_tpk.c so aceita posicao lida
#      do firmware depois da abertura corrente — o host publica o marcador por
#      nv_tpk_video_registrar_pos_lida (posLidoMs, so escrito na leitura real
#      do GetPlayPosition e zerado ao abrir e a cada seek) — e exclui a
#      posMantida da reabertura, a base do fluxo e a posConhecida sem
#      deslocamento, o alvo do posReconLeg, o alvo do ack do seek e o marcador
#      com alvo de seek em voo (EV_BUSCOU/EV_BUSCA_FALHOU desarmam so o MESMO
#      alvo, por CAS); o calculo
#      do deslocamento do progressivo tambem le o marcador, nao o cache;
#  (b) harness: posReal/video_pos/video_duracao/video_pos_amostra extraidos de
#      src/video_tpk.c (o getter REAL, nao stub) rodam com a guarda do
#      memQuadroPendente extraida de src/player.c os casos da sonda da r3 —
#      ack com o alvo antes da leitura, posMantida, posConhecida, base de
#      fluxo e posReconLeg nao emitem FE_PRONTO; a leitura real emite. Roda em
#      TPK e TPK40, normal, ASan+UBSan (halt_on_error=1) e clang TSan.
set -eu
cd "$(dirname "$0")/.."

falhou=0
falha() { echo "farol_amostra_tpk: FALHA - $1" >&2; falhou=1; }

# (a) estatica
python3 - <<'PYEOF' || falhou=1
import re, sys
ruim = 0
tpk = open("src/video_tpk.c", encoding="utf-8").read()
cs = open("tizen-tpk/Video.cs", encoding="utf-8").read()

# A lib exporta o registrador do marcador (visibility default, como os irmaos).
m = re.search(r'__attribute__\(\(visibility\("default"\)\)\)\s*\n'
              r'void nv_tpk_video_registrar_pos_lida\(FnPos \w+\) '
              r'\{ hPosLida = \w+; \}', tpk)
if not m:
    print("farol_amostra_tpk: nv_tpk_video_registrar_pos_lida ausente/nao exportado",
          file=sys.stderr); ruim = 1
if "static FnPos    hPosLida;" not in tpk and not re.search(r"static FnPos\s+hPosLida;", tpk):
    print("farol_amostra_tpk: hPosLida nao declarado", file=sys.stderr); ruim = 1

# A amostra: so leitura real; nada de posMantida, base, posConhecida, alvo do
# ack nem alvo da reconexao.
am = re.search(r"^double video_pos_amostra\(Uint64 \*t\) \{(.*?)^\}", tpk, re.M | re.S)
if not am:
    print("farol_amostra_tpk: video_pos_amostra nao extraida", file=sys.stderr); ruim = 1
else:
    corpo = am.group(1)
    for trecho, msg in [
        ("cv.reabrindo) return 0.0", "posMantida da reabertura nao excluida"),
        ("cv.prog && !cv.deslocOk) return 0.0", "base do fluxo/posConhecida nao excluidas"),
        ("posReconLeg >= 0.0) return 0.0", "alvo da reconexao nao excluido"),
        ("hPosLida", "amostra nao usa o marcador de leitura real"),
        ("ms < 0) return 0.0", "amostra nao barra 'sem leitura real'"),
        ("buscaAlvoMs) >= 0) return 0.0", "amostra aceita marcador com alvo de seek em voo"),
    ]:
        if trecho not in corpo:
            print("farol_amostra_tpk: %s" % msg, file=sys.stderr); ruim = 1
    if re.search(r"return video_pos\(\);", corpo) and "hPosLida" not in corpo:
        print("farol_amostra_tpk: amostra devolve video_pos() sem marcador",
              file=sys.stderr); ruim = 1

# A falha do seek desarma so o MESMO alvo (CAS): um alvo novo armado por cima
# continua em voo — sem isso a falha do seek anterior reabre a janela do farol.
mf = re.search(r"case EV_BUSCA_FALHOU:(.*?)break;", tpk, re.S)
if not mf or "atomic_compare_exchange_strong(&buscaAlvoMs" not in mf.group(1):
    print("farol_amostra_tpk: EV_BUSCA_FALHOU desarma alvo diverso em voo",
          file=sys.stderr); ruim = 1

# O deslocamento do progressivo le o marcador quando existe: um alvo de ack
# nao pode virar desloc da abertura inteira.
desl = re.search(r"cv\.prog && !cv\.deslocOk && pronto && hPos\) \{(.*?)^  \}",
                 tpk, re.M | re.S)
if desl and "hPosLida" not in desl.group(1):
    print("farol_amostra_tpk: desloc do progressivo ignora o marcador",
          file=sys.stderr); ruim = 1

# O host: posLidoMs so no GetPlayPosition real; -1 ao abrir e a cada seek;
# registrado como opcional (lib encenada antiga nao exporta) e ligado no
# resolve do carregador ELF do TPK40.
if "posLidoMs = -1" not in cs:
    print("farol_amostra_tpk: host sem posLidoMs", file=sys.stderr); ruim = 1
# A janela do seek: o Buscar marca o pendente, o Tique o descarta e o
# ack/falha o limpa andando a geracao — nenhuma amostra do intervalo vale.
for trecho, msg in [
    ("buscaEmVoo = true", "Buscar nao marca o seek pendente"),
    ("&& !buscaEmVoo", "Tique nao descarta a amostra da janela do seek"),
    ("buscaEmVoo = false", "ack/falha nao encerra o seek pendente"),
]:
    if trecho not in cs:
        print("farol_amostra_tpk: %s" % msg, file=sys.stderr); ruim = 1
if not re.search(r"posMs = ms; posLidoMs = ms;", cs):
    print("farol_amostra_tpk: GetPlayPosition nao marca posLidoMs", file=sys.stderr)
    ruim = 1
if "nv_tpk_video_registrar_pos_lida" not in cs or "RegistrarPosLida" not in cs:
    print("farol_amostra_tpk: host nao registra o marcador", file=sys.stderr); ruim = 1
if not re.search(r"resolve\(\"nv_tpk_video_registrar_pos_lida\"\)", cs):
    print("farol_amostra_tpk: Ligar (rota ELF do 4/5) sem o marcador",
          file=sys.stderr); ruim = 1
# O ack do seek publica o ALVO em posMs: nao pode marcar leitura real ali.
ack = re.search(r"lock \(trocaTrava\) \{\s*\n\s*if \(minha == sessao && player == p\) \{\s*\n"
                r"\s*posMs = ms;", cs)
if ack and "posLidoMs = ms" in ack.group(0):
    print("farol_amostra_tpk: ack do seek marcando leitura real", file=sys.stderr)
    ruim = 1
sys.exit(ruim)
PYEOF

# (b) harness: o getter REAL do video_tpk.c + a guarda REAL do player.c, com o
# firmware simulado como na sonda da r3.
gen=tests/farol_amostra_tpk_gen.c
python3 - <<'PYEOF' > "$gen"
import re
def hit(rx, s):
    m = re.search(rx, s, re.M | re.S)
    if not m: raise SystemExit("farol_amostra_tpk: extracao falhou: " + rx)
    return m.group(0)
def func(s, sig):
    return hit(r'^' + re.escape(sig) + r' \{.*?^\}', s)

tpk = open("src/video_tpk.c", encoding="utf-8").read()
p = open("src/player.c", encoding="utf-8").read()
guarda = hit(r'^  if \(memQuadroPendente && comVideo.*?^  \}', p)

print(r'''
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
typedef uint64_t Uint64;
static Uint64 now;
static Uint64 nv_ticks64(void) { return now; }
static unsigned SDL_GetTicks(void) { return (unsigned)now; }
static int memQuadroPendente, comVideo = 1, prontoAnel, retomarPct, medirAbertura, tocando, pronto;
static unsigned loadEm, abertoEm;
static int ehCanal(void) { return 0; }
static void memlog_evento(const char *s) { (void)s; }
static enum { FM_PLAYER, FE_PRONTO } types __attribute__((unused));
static int events;
static void farol_ev(int m, int evt, int f, int32_t a, int b, const char *tok) {
  (void)m; (void)evt; (void)f; (void)a; (void)b; (void)tok; events++;
}
''')

# O host simulado replica o contrato do Video.cs: posMs recebe tambem o alvo
# do ack do seek; posLidoMs so recebe a leitura real do GetPlayPosition e
# volta a -1 ao abrir e a cada seek pedido.
print(r'''
#include <stdatomic.h>
typedef int (*FnPos)(void);
static int raw = -1, lido = -1;
static int getpos(void) { return raw; }
static int getlido(void) { return lido; }
static FnPos hPos = getpos;
static FnPos hPosLida;
// O alvo em voo do video_buscar: armado no pedido, desarmado no ack e na falha.
static atomic_int buscaAlvoMs = -1;
static void hostRegistrar(void) { hPosLida = getlido; }
static void hostAbrir(void) { raw = 0; lido = -1; atomic_store(&buscaAlvoMs, -1); hostRegistrar(); }
static void hostSeek(int ms) { atomic_store(&buscaAlvoMs, ms); lido = -1; }  // pedido: alvo em voo, marcador invalido
static void hostAckSeek(int ms) { int a = ms; if (atomic_compare_exchange_strong(&buscaAlvoMs, &a, -1)) raw = ms; }
static void hostSeekFalha(int ms) { int a = ms; atomic_compare_exchange_strong(&buscaAlvoMs, &a, -1); }  // EV_BUSCA_FALHOU
static void hostLeituraReal(int ms) { raw = ms; lido = ms; }
static int terminou, voo, durMs;
static int video_busca_em_voo(void) { return voo; }
static int video_tocando(void) { return tocando; }
static double inicio = -1, posReconLeg = -1;
static double dts_tpk_inicio_stream(void) { return inicio; }
static double dts_tpk_duracao(void) { return 1800; }
static struct {
  int ativo, prog, deslocOk, seekPend, reabrindo;
  double posConhecida, desloc, seekAlvo, posMantida;
} cv;
static double video_duracao(void);
static void convEstadoZera(void) {
  cv.ativo = cv.prog = cv.deslocOk = cv.seekPend = cv.reabrindo = 0;
  cv.posConhecida = cv.desloc = cv.seekAlvo = cv.posMantida = 0;
  inicio = posReconLeg = -1; terminou = voo = durMs = 0;
}
''')

print(hit(r'^int  video_pronto\(void\).*?\n', tpk))
for sig in ('static double posReal(void)', 'double video_pos(void)',
            'double video_duracao(void)', 'double video_pos_amostra(Uint64 *t)'):
    print(func(tpk, sig))
print("\nstatic void update(void) {\n" + guarda + "\n}\n")
print(r'''
static int check(const char *name, double espera) {
  events = prontoAnel = 0; memQuadroPendente = 1; loadEm = 100; now = 150;
  double got = video_pos_amostra(NULL);
  update();
  int quer = espera > 0.0;
  printf("%s: sample=%.3f FE_PRONTO=%d expected_sample=%.3f expected_FE_PRONTO=%d\n",
         name, got, events, espera, quer);
  int ok = (quer ? got > 0.0 : got == 0.0) && events == quer;
  if (!ok) printf("  ^ FALHA\n");
  return !ok;
}
int main(void) {
  int rc = 0;
  pronto = tocando = 1;

  convEstadoZera(); hostAbrir();
  rc |= check("inicial sem clock", 0);

  convEstadoZera(); hostAbrir(); hostLeituraReal(400);
  rc |= check("clock real", 0.4);

  convEstadoZera(); hostAbrir();
  cv.ativo = cv.prog = cv.seekPend = 1; cv.seekAlvo = 120; raw = 0;
  rc |= check("alvo pendente com deslocamento desconhecido", 0);

  convEstadoZera(); hostAbrir();
  cv.ativo = cv.prog = 1; inicio = 15; raw = 0;
  rc |= check("base do stream com relogio bruto em 0", 0);

  convEstadoZera(); hostAbrir();
  cv.ativo = cv.prog = 1; inicio = -1; cv.posConhecida = 120; raw = 0;
  rc |= check("posicao lembrada com relogio bruto em 0", 0);

  convEstadoZera(); hostAbrir(); hostLeituraReal(400);
  cv.ativo = cv.prog = cv.deslocOk = 1; cv.desloc = 15;
  rc |= check("progressivo com deslocamento e leitura real", 15.4);

  convEstadoZera(); hostAbrir();
  cv.ativo = cv.prog = cv.deslocOk = 1; cv.desloc = 120;
  cv.reabrindo = 1; cv.posMantida = 120; raw = 0;
  rc |= check("alvo mantido em reabertura", 0);

  convEstadoZera(); hostAbrir();
  posReconLeg = 120; raw = 0;
  rc |= check("alvo de reconexao", 0);

  convEstadoZera(); hostAbrir(); hostSeek(120000); hostAckSeek(120000);
  rc |= check("host publicou ms solicitado no ack, sem leitura GetPlayPosition", 0);

  hostLeituraReal(400);
  rc |= check("leitura real depois do ack", 0.4);

  convEstadoZera(); hostAbrir(); hostLeituraReal(400);
  hostSeek(120000); hostAckSeek(120000);
  rc |= check("ack depois de leitura real: alvo nao volta a valer", 0);

  // Janela do seek: um marcador positivo com o alvo ainda em voo e a leitura
  // de antes do pedido (host velho ou consulta na janela) — nao vale.
  convEstadoZera(); hostAbrir(); hostLeituraReal(1234);
  hostSeek(120000); lido = 1234;
  rc |= check("marcador positivo com alvo em voo", 0);

  hostAckSeek(120000); hostLeituraReal(120100);
  rc |= check("leitura real depois do ack da janela", 120.1);

  convEstadoZera(); hostAbrir(); hostSeek(120000); lido = 1234;
  hostSeekFalha(120000);
  rc |= check("leitura real depois do seek recusado", 1.234);

  // Seek sobre seek: o segundo pedido rearma o alvo antes do primeiro
  // responder; a falha do alvo velho nao pode desarmar o novo em voo.
  convEstadoZera(); hostAbrir(); hostLeituraReal(1234);
  hostSeek(120000); hostSeek(60000); lido = 1234;
  hostSeekFalha(120000);
  rc |= check("falha do alvo velho com alvo novo em voo", 0);

  hostSeekFalha(60000);
  rc |= check("leitura real depois da falha do alvo corrente", 1.234);

  return rc;
}
''')
PYEOF
trap 'rm -f "$gen" "$bin"' EXIT

# Falha esperada na ponta anterior: os casos com alvo/base/lembrada/ack
# emitiam FE_PRONTO sem leitura real (sonda r3, 05-backends.txt).
bin=tests/farol_amostra_tpk_bin
for alvo in "TPK -DNV_TPK" "TPK40 -DNV_TPK -DNV_TPK40"; do
  set -- $alvo; rot="$1"; shift
  cc -Wall -Wextra -O1 -g "$@" "$gen" -o "$bin" || falha "$rot normal nao compilou"
  "$bin" || falha "$rot normal"
done

case "${SANITIZE:-1}" in
  1)
    if command -v clang >/dev/null 2>&1; then
      for alvo in "TPK -DNV_TPK" "TPK40 -DNV_TPK -DNV_TPK40"; do
        set -- $alvo; rot="$1"; shift
        clang -fsanitize=address,undefined -fno-omit-frame-pointer \
              -Wall -Wextra -O1 -g "$@" "$gen" -o "$bin" || falha "$rot ASan+UBSan compilou"
        ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
          "$bin" || falha "$rot ASan+UBSan"
      done
      for alvo in "TPK -DNV_TPK" "TPK40 -DNV_TPK -DNV_TPK40"; do
        set -- $alvo; rot="$1"; shift
        clang -fsanitize=thread -fno-omit-frame-pointer \
              -Wall -Wextra -O1 -g "$@" "$gen" -o "$bin" || falha "$rot TSan compilou"
        { setarch -R "$bin" 2>/dev/null || "$bin"; } || falha "$rot TSan"
      done
    fi
    ;;
esac

[ "$falhou" = 0 ] || exit 1
echo "farol_amostra_tpk: tudo ok"
