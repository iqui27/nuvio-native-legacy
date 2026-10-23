#!/usr/bin/env python3
"""Smoke do app DE VERDADE no Mac, com uma COPIA da pasta de dados logada.

    python3 tools/auditoria/smoke_mac.py [--sem-build] [--serie tt0903747]

Regras (docs/auditoria-release.md):
  - Nunca escreve em ~/.nuvio: copia para $TMPDIR/nuvio-auditoria-dados.
  - Nunca aperta Reproduzir. O unico OK enviado e na tela de escolha de perfil;
    titulo so abre por "abrir:<tt>" (porta de teste do main.c).
  - Nunca mata processo que nao seja o binario que ele mesmo abriu.
  - /tmp/nuvio-key e /tmp/nuvio-shot-req sao do sistema todo: com outro app do
    Mac aberto o smoke nao roda (sai 2, "nao verificado").

Saida: uma linha PASS/FAIL/NAO VERIFICADO por afirmacao; codigo 0 (tudo PASS),
1 (alguma FAIL) ou 2 (algum check executavel ficou sem verificar).
INFO registra limites de cobertura; nao afirma que houve reproducao.
"""
import argparse
import os
import re
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]
TMP = Path(os.environ.get("TMPDIR", "/tmp"))
BIN = TMP / "nuvio-auditoria-mac"
DADOS = TMP / "nuvio-auditoria-dados"
SAIDA = TMP / "nv-auditoria-smoke"
ORIGEM = Path(os.environ.get("NUVIO_AUD_ORIGEM", str(Path.home() / ".nuvio")))
KEY, SHOT_REQ, SHOT = "/tmp/nuvio-key", "/tmp/nuvio-shot-req", "/tmp/nuvio-shot.bmp"
CW_CONCLUIDO = 90          # "fora de 1-90%" no log do Continuar

resultados = []


def res(estado, nome, detalhe=""):
    resultados.append((estado, nome, detalhe))
    print(f"  {estado:<15} {nome}" + (f" — {detalhe}" if detalhe else ""), flush=True)


class App:
    def __init__(self, rotulo):
        self.log = SAIDA / f"{rotulo}.log"
        self.rotulo = rotulo
        env = dict(os.environ, NUVIO_DADOS=str(DADOS), NUVIO_CW_LOG="1")
        open(KEY, "w").close()
        self.p = subprocess.Popen([str(BIN), str(RAIZ / "deploy/app/art")], env=env,
                                  stdout=open(self.log, "w"), stderr=subprocess.STDOUT,
                                  start_new_session=True)

    def texto(self):
        try:
            return self.log.read_text(errors="ignore")
        except OSError:
            return ""

    def esperar(self, regex, seg, desde=0):
        fim = time.time() + seg
        while time.time() < fim:
            m = list(re.finditer(regex, self.texto()[desde:], re.M))
            if m:
                return m[-1]
            if self.p.poll() is not None:
                return None
            time.sleep(0.4)
        return None

    def tecla(self, *ts, pausa=1.2):
        for t in ts:
            with open(KEY, "w") as f:
                f.write(t + "\n")
            time.sleep(pausa)

    def foto(self, nome):
        try:
            os.remove(SHOT)
        except OSError:
            pass
        with open(SHOT_REQ, "w") as f:
            f.write("1\n")
        for _ in range(20):
            time.sleep(0.25)
            if os.path.exists(SHOT) and os.path.getsize(SHOT) > 0:
                time.sleep(0.4)
                dst = SAIDA / f"{self.rotulo}-{nome}.png"
                subprocess.run(["sips", "-s", "format", "png", "-Z", "960", SHOT, "--out", str(dst)],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                return dst
        return None

    def fechar(self):
        if self.p.poll() is None:
            self.p.send_signal(signal.SIGTERM)
            try:
                self.p.wait(6)
            except subprocess.TimeoutExpired:
                self.p.kill(); self.p.wait()


def outro_app_aberto():
    r = subprocess.run(["pgrep", "-f", "nuvio-native-legacy-mac|nuvio-auditoria-mac|NuvioMac"],
                       capture_output=True, text=True)
    return [l for l in r.stdout.split() if l.strip() and int(l) != os.getpid()]


def compilar():
    env = dict(os.environ, NUVIO_MAC_BIN=str(BIN), NUVIO_MAC_SO_COMPILAR="1")
    if "NUVIO_PROPERTIES" not in env:
        cand = RAIZ.parent / "NuvioWeb-0.3.38-beta/local.properties"
        padrao = Path.home() / "Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties"
        for c in (cand, padrao):
            if c.exists():
                env["NUVIO_PROPERTIES"] = str(c); break
    r = subprocess.run(["bash", "tools/env.sh", "--require-core"], cwd=RAIZ, env=env,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    if r.returncode:
        return "env.sh --require-core falhou (NUVIO_PROPERTIES?): build sairia sem login"
    r = subprocess.run(["bash", "tools/mac.sh"], cwd=RAIZ, env=env, capture_output=True, text=True)
    if r.returncode:
        return "tools/mac.sh falhou:\n" + r.stderr[-2000:]
    return None


def copiar_dados():
    if DADOS.exists():
        shutil.rmtree(DADOS)
    shutil.copytree(ORIGEM, DADOS, ignore=shutil.ignore_patterns("cache", "epg-*"))
    # As telas de novidades cobrem a Home e so saem com OK: marcadas como vistas
    # NA COPIA. Os nomes saem do proprio codigo, entao versao nova entra sozinha.
    nomes = set()
    # O motor atual inclui o conteudo (e a marca) de src/novidades/<versao>.inc.
    fontes = (list((RAIZ / "src").glob("*.c")) + list((RAIZ / "src").glob("*.h"))
              + list((RAIZ / "src/novidades").glob("*.inc")))
    for c in fontes:
        nomes |= set(re.findall(r'"(novidades-[A-Za-z0-9_-]+\.txt)"', c.read_text(errors="ignore")))
        for m in re.finditer(r'#define\s+\w+_ARQ\s+"(novidades[^"]+)"', c.read_text(errors="ignore")):
            nomes.add(m.group(1))
    for n in nomes:
        p = DADOS / n
        if not p.exists():
            p.write_text("1\n")
    # O cartao "Recomendacao de um amigo" abre sozinho na Home, uma vez por
    # recomendacao nova, e e MODAL: engole toda tecla menos OK/Voltar
    # (app.c: recomenda_aberta -> recomenda_evento). Sem a marca de "ja
    # anunciado" na copia, o Cima do painel de Salvos morria nele. A marca e
    # so um arquivo local; nada vai ao servidor (so OK -> recomenda_marcar_vistas
    # escreve la, e o smoke nunca aperta OK nele).
    (DADOS / "recomendacoes-cartao.txt").write_text("99999999999999999\n")


def ler_progresso(perfil):
    """Candidatos ao Continuar deste perfil, como o app os escolhe: >= 60 s,
    1..90%, o mais recente por titulo, fora do que foi tirado da fileira."""
    ocultos = {}
    p = DADOS / "cwoculto.txt"
    if p.exists():
        for l in p.read_text().split("\n"):
            c = l.split("\t")
            if len(c) >= 3 and c[0] == str(perfil):
                ocultos[c[1]] = int(c[2] or 0)
    porTitulo = {}
    p = DADOS / "progresso.txt"
    if not p.exists():
        return []
    for l in p.read_text().split("\n"):
        c = l.split("\t")
        if len(c) < 9 or c[0] != str(perfil):
            continue
        try:
            pos, dur, ms = float(c[6]), float(c[7]), int(c[8])
        except ValueError:
            continue
        obra = c[2]
        if obra not in porTitulo or ms > porTitulo[obra][2]:
            porTitulo[obra] = (c[3], pos / dur * 100 if dur > 0 else 0, ms, dur)
    out = []
    for obra, (tipo, pct, ms, dur) in porTitulo.items():
        if dur < 60 or pct < 1 or pct >= CW_CONCLUIDO:
            continue
        if obra in ocultos and ocultos[obra] >= ms:
            continue
        out.append((ms, obra, tipo, int(pct)))
    return sorted(out, reverse=True)


def cw_ultimo(texto, desde=0):
    """O ultimo bloco de cw[i] publicado: lista de (imdb base, T, E, ms)."""
    blocos, atual = [], []
    for l in texto[desde:].split("\n"):
        m = re.match(r"\[desc\] cw\[(\d+)\] (\S+) T(\d+)E(\d+) (\d+)% ms=(\d+)", l)
        if m:
            if m.group(1) == "0" and atual:
                blocos.append(atual); atual = []
            atual.append((m.group(2).split(":")[0], int(m.group(3)), int(m.group(4)), int(m.group(6))))
        elif atual and "continuar assistindo:" in l:
            blocos.append(atual); atual = []
    if atual:
        blocos.append(atual)
    return blocos[-1] if blocos else None


def conferir_cw(nome, cw, perfil, A=None):
    if cw is None:
        res("FAIL", f"{nome}: Continuar publicado", "nenhuma linha [desc] cw[...] no log"); return
    if not cw:
        res("FAIL", f"{nome}: Continuar nao vazio"); return
    res("PASS", f"{nome}: Continuar nao vazio", f"{len(cw)} itens")
    cand = ler_progresso(perfil)
    nacw = {c[0] for c in cw}
    # Fileira cheia (12): so conta quem e ESTRITAMENTE mais novo que o ultimo
    # card. Empate no ms (linhas importadas juntas) e corte legitimo.
    piso = min(c[3] for c in cw) if len(cw) >= 12 else -1
    faltam = [(o, t, p) for ms, o, t, p in cand if ms > piso and o not in nacw]
    filmes = [x for x in cand if x[2] == "movie"]
    if faltam:
        res("FAIL", f"{nome}: itens em andamento do perfil {perfil} na fileira",
            "faltam " + ", ".join(f"{o} ({t} {p}%)" for o, t, p in faltam[:6])
            + f" — progresso.txt da copia, mais recentes que o ultimo card")
    else:
        res("PASS", f"{nome}: itens em andamento do perfil {perfil} na fileira",
            f"{len(cand)} candidatos locais, {len(filmes)} filmes; nenhum mais recente que o ultimo card ficou fora")


def rodada_a(serie_pedida):
    print("\n== rodada A: o perfil que ja estava ativo", flush=True)
    a = App("A")
    try:
        m = a.esperar(r"a tela de escolha ja pode abrir|\[perfis\] \d+ perfil\(is\), dono=", 40)
        if not m:
            res("FAIL", "A: app abriu e leu a sessao", "sem linha [perfis] em 40 s (log em %s)" % a.log)
            return None
        time.sleep(2.5)
        a.foto("escolha")
        # OK SO COM A TELA DE ESCOLHA ANUNCIADA: ali o foco esta no perfil ativo.
        # Sem ela, o OK cairia num cartao da Home (e o do Continuar pode tocar).
        if "a tela de escolha ja pode abrir" in a.texto():
            a.tecla("ok")
        if not a.esperar(r"\[desc\] catalogo montado com \d+", 120):
            res("FAIL", "A: catalogo montado", "sem '[desc] catalogo montado' em 120 s")
            return None
        time.sleep(4)
        t = a.texto()
        mp = re.findall(r"\[perfis\] perfil ativo: (\d+)|ativo=(\d+)", t)
        perfil = int([x for x in mp[-1] if x][0]) if mp else 1
        cw = cw_ultimo(t)
        conferir_cw("A", cw, perfil)
        a.foto("home")
        serie = serie_pedida or next((c[0] for c in (cw or []) if c[1] > 0), None)
        if not serie:
            res("NAO VERIFICADO", "A: pagina de serie", "sem serie no Continuar; use --serie tt...")
        else:
            n0 = len(a.texto())
            a.tecla(f"abrir:{serie}", pausa=0.5)
            m = a.esperar(rf"\[detail\] episodios na pagina: {serie}\S* ([1-9]\d*)", 25, n0)
            ultimo = list(re.finditer(rf"\[detail\] episodios na pagina: {serie}\S* (\d+)", a.texto()[n0:]))
            a.foto("detalhe-home")
            if m:
                res("PASS", "A: serie aberta pela Home mostra episodios", f"{serie}: {m.group(1)}")
            else:
                res("FAIL", "A: serie aberta pela Home mostra episodios",
                    f"{serie}: {ultimo[-1].group(1) if ultimo else 'pagina sem linha [detail]'} em 25 s")
            # Salvos: "Cima" na pagina do titulo abre o painel direto (sem o
            # modal da ilha, que a AZUL abre quando ha cartao na pilula).
            n1 = len(a.texto())
            a.tecla("up", "up")
            m = a.esperar(r"\[spainel\] aberto: (\d+) salvos", 6, n1)
            a.foto("salvos")
            if not m:
                res("FAIL", "A: painel de Salvos abre", "sem '[spainel] aberto' depois de Cima na pagina")
            elif int(m.group(1)) > 0:
                res("PASS", "A: painel de Salvos nao vazio", f"{m.group(1)} linhas")
            else:
                res("FAIL", "A: painel de Salvos nao vazio", "0 linhas")
            a.tecla("back", "back")
            # A MESMA serie de novo: e o caminho da ilha/player (o roteador ja
            # viu esse alvo). Episodios tem de continuar na pagina.
            n2 = len(a.texto())
            a.tecla(f"abrir:{serie}", pausa=0.5)
            time.sleep(10)
            v = re.findall(rf"\[detail\] episodios na pagina: {serie}\S* (\d+)", a.texto()[n2:])
            a.foto("detalhe-reaberto")
            if v and int(v[-1]) > 0:
                res("PASS", "A: mesma serie reaberta mostra episodios", f"{serie}: {v[-1]}")
            else:
                res("FAIL", "A: mesma serie reaberta mostra episodios", f"{serie}: {v[-1] if v else 'sem linha [detail]'}")
            a.tecla("back")
        res("INFO", "A: limite de cobertura ilha / player",
            "abertura real nao exercitada: exige reproducao; pedido de episodios coberto pela estatica 1")
        return {"perfil": perfil, "cw": cw or []}
    finally:
        a.fechar()


def rodada_b(A):
    print("\n== rodada B: troca de perfil na tela de escolha", flush=True)
    perfis = DADOS / "perfis.txt"
    n = len([l for l in perfis.read_text().split("\n") if l.strip()]) if perfis.exists() else 0
    if n < 2:
        res("NAO VERIFICADO", "B: troca de perfil", "a conta copiada tem um perfil so"); return
    b = App("B")
    try:
        if not b.esperar(r"a tela de escolha ja pode abrir", 40):
            res("NAO VERIFICADO", "B: troca de perfil", "tela de escolha nao anunciada; sem OK as cegas")
            return
        time.sleep(2.5)
        b.tecla("right" if A["perfil"] == 1 else "left", pausa=1.0)
        b.foto("escolha")
        b.tecla("ok")                         # o unico OK do smoke: escolhe o perfil
        m = b.esperar(r"\[perfis\] home do perfil (\d+) (pronta|aberta pelo teto)", 40)
        if not m:
            res("FAIL", "B: home do perfil novo", "sem '[perfis] home do perfil N' em 40 s"); return
        novo = int(m.group(1))
        if novo == A["perfil"]:
            res("NAO VERIFICADO", "B: troca de perfil", f"continuou no perfil {novo}"); return
        res("PASS", "B: troca de perfil", f"{A['perfil']} -> {novo} ({m.group(2)})")
        # O Continuar do perfil novo sai ANTES da home ficar pronta (ela espera
        # por ele): a janela comeca na linha que troca o perfil ativo.
        k = b.texto().rfind(f"[perfis] perfil ativo: {novo}")
        desde = k if k >= 0 else m.end()
        b.esperar(r"\[desc\] catalogo montado com \d+", 90, desde)
        time.sleep(5)
        cw = cw_ultimo(b.texto(), desde)
        b.foto("home")
        conferir_cw("B", cw, novo)
        if cw:
            meus = {o for _, o, _, _ in ler_progresso(novo)}
            herdados = [c[0] for c in cw if c[0] in {x[0] for x in A["cw"]} and c[0] not in meus]
            if herdados and len(herdados) == len(cw):
                res("FAIL", "B: Continuar e do perfil novo",
                    f"todos os {len(cw)} itens sao os do perfil {A['perfil']} ({', '.join(herdados[:4])}...)")
            else:
                res("PASS", "B: Continuar e do perfil novo",
                    f"{len(cw) - len(herdados)} de {len(cw)} itens nao vinham do perfil {A['perfil']}")
        b.tecla("back")
    finally:
        b.fechar()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sem-build", action="store_true")
    ap.add_argument("--serie", default=os.environ.get("NUVIO_AUD_SERIE", ""))
    a = ap.parse_args()
    SAIDA.mkdir(parents=True, exist_ok=True)
    print("SMOKE MAC (app real, copia dos dados)", flush=True)
    vivos = outro_app_aberto()
    if vivos:
        res("NAO VERIFICADO", "smoke", f"ha app do Mac aberto (pid {', '.join(vivos)}); as portas /tmp/nuvio-* sao compartilhadas")
        return 2
    if not ORIGEM.exists():
        res("NAO VERIFICADO", "smoke", f"{ORIGEM} nao existe (sem conta logada para copiar)")
        return 2
    if not a.sem_build or not BIN.exists():
        t0 = time.time()
        erro = compilar()
        if erro:
            res("FAIL", "build do Mac", erro); return 1
        res("PASS", "build do Mac", f"{time.time() - t0:.0f} s, {BIN}")
    copiar_dados()
    A = rodada_a(a.serie)
    if A:
        rodada_b(A)
    print(f"\nlogs e capturas: {SAIDA}")
    if any(r[0] == "FAIL" for r in resultados):
        return 1
    return 2 if any(r[0] == "NAO VERIFICADO" for r in resultados) else 0


if __name__ == "__main__":
    sys.exit(main())
