#!/usr/bin/env python3
"""Gera o site estatico do painel de issues a partir de docs/issues/mapa.json."""
import argparse, datetime, json, os, re, shutil, subprocess, sys

AQUI = os.path.dirname(os.path.abspath(__file__))


def git(repo, *args):
    try:
        r = subprocess.run(["git", "-C", repo, *args], capture_output=True, text=True, check=True)
        return r.stdout.strip()
    except Exception:
        return ""


def publico(dados, repo):
    # Construct every object explicitly: no internal field survives by default.
    def text(value, fallback=""):
        value = str(value or "")
        return fallback if re.search("dono|decis|valida|respond|relato", value, re.I) else value

    def version(value):
        m = re.match(r"^\d+\.\d+(?:\.\d+)?", str(value or ""))
        return m.group() if m else ""

    def platform(value):
        value = str(value or "").strip()
        if value in ("Android", "LG", "Samsung .tpk", "Samsung .wgt", "Samsung", "VIDAA", "PC", "Mac", "all"):
            return value
        for name in ("Android", "LG", "Samsung", "VIDAA", "PC", "Mac"):
            if re.match(re.escape(name) + r"(?:\?|\s|$)", value):
                return name
        return "—"

    itens = []
    for i in dados["itens"]:
        status = i.get("status")
        status = {"consertada-nao-lancada": "precisa-teste"}.get(status, status)
        if status in ("respondida", "por-desenho", "fora-do-escopo", "duplicada", "fechada-sem-resposta"):
            status = "encerrada"
        elif status not in ("lancada", "precisa-teste", "precisa-log", "em-andamento", "em-progresso", "good-first-issue"):
            status = "aberta"
        texto = json.dumps([i.get("notas"), i.get("proximo_passo"), i.get("conserto")], ensure_ascii=False)
        tokens = set(re.findall(r"[A-Za-z0-9][A-Za-z0-9._/-]{2,80}", texto))
        branches, commits = [], []
        if status in ("em-andamento", "em-progresso", "precisa-teste"):
            branches = [b for b in dados["meta"]["branches"] if b in tokens]
            hashes = {h for h in dados["meta"]["commits"] if h in tokens}
            hashes.update(dados["meta"]["branch_heads"][b] for b in branches)
            for h in sorted(hashes):
                fmt = git(repo, "log", "-1", "--format=%H%x09%s", h)
                sha, _, assunto = fmt.partition("\t")
                if sha:
                    commits.append({"hash": sha, "assunto": text(assunto, "Commit")})
        itens.append({"numero": int(i["numero"]), "titulo": text(i.get("titulo"), "Issue #" + str(i["numero"])),
                      "plataforma": platform(i.get("plataforma")),
                      "estado_github": "aberta" if i.get("estado_github") == "aberta" else "fechada",
                      "status": status, "alvo": version(i.get("alvo")) or (i.get("alvo") if i.get("alvo") in ("futuro", "ja-lancada", "nao vamos fazer") else ""),
                      "release": version(i.get("release")), "branches": branches, "commits": commits})
    roadmap = {}
    for r in dados.get("roadmap", []):
        release = version(r.get("release")) or ("futuro" if r.get("release") == "futuro" else "")
        if release:
            roadmap.setdefault(release, {"release": release, "situacao": "lancada" if re.search("lançada|lancada", r.get("situacao", "")) else "planejada"})
    return {"meta": {"repo": dados["meta"]["repo"], "publico": True}, "itens": itens,
            "roadmap": list(roadmap.values())}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--repo", default=os.path.abspath(os.path.join(AQUI, "..", "..")))
    ap.add_argument("--out", default=os.path.join(AQUI, "site"))
    ap.add_argument("--publico", action="store_true", help="exporta somente campos públicos")
    a = ap.parse_args()

    mapa = os.path.join(a.repo, "docs", "issues", "mapa.json")
    try:
        with open(mapa, encoding="utf-8") as f:
            dados = json.load(f)
    except FileNotFoundError:
        sys.exit("ERRO: %s nao existe" % mapa)
    except (OSError, ValueError) as e:
        sys.exit("ERRO: nao consegui ler %s: %s" % (mapa, e))
    if not isinstance(dados, dict) or not isinstance(dados.get("itens"), list):
        sys.exit("ERRO: %s sem a lista 'itens'" % mapa)

    fmt = git(a.repo, "log", "-1", "--format=%h%x09%s")
    h, _, assunto = fmt.partition("\t")
    dados["meta"] = {
        "commit": h,
        "assunto": assunto,
        "branch": git(a.repo, "rev-parse", "--abbrev-ref", "HEAD"),
        "gerado_em": datetime.datetime.now().astimezone().isoformat(timespec="seconds"),
        "mapa_atualizado_em": dados.get("atualizado_em", ""),
    }

    # Hashes citados em qualquer texto do mapa que EXISTEM no repo: so esses
    # viram link de commit no painel (um prefixo de sha256 nao vira link morto).
    texto = json.dumps(dados, ensure_ascii=False)
    commits = {}
    for h8 in sorted(set(re.findall(r"\b[0-9a-f]{7,12}\b", texto))):
        full = git(a.repo, "rev-parse", "--verify", "--quiet", h8 + "^{commit}")
        if full: commits[h8] = full
    remoto = git(a.repo, "remote", "get-url", "origin")
    mrep = re.search(r"github\.com[:/]([^/]+/[^/.]+)", remoto)
    dados["meta"]["commits"] = commits
    dados["meta"]["repo"] = "https://github.com/" + mrep.group(1) if mrep else ""

    # Branches citadas no mapa que EXISTEM no repo local: os outros viram texto
    # puro, nunca link morto. Tokeniza uma vez e cruza com as branches do repo.
    tokens = set(re.findall(r"[A-Za-z0-9][A-Za-z0-9._/-]{2,80}", texto))
    ramos = git(a.repo, "for-each-ref", "--format=%(refname:short)", "refs/heads").split()
    dados["meta"]["branches"] = sorted(set(ramos) & tokens)

    # Only branch tips have an ordering guarantee; cited hashes are not a timeline.
    dados["meta"]["branch_heads"] = {}
    for branch in dados["meta"]["branches"]:
        head = git(a.repo, "rev-parse", branch + "^{commit}")
        dados["meta"]["branch_heads"][branch] = head[:12]
        commits[head[:12]] = head

    if a.publico:
        dados = publico(dados, a.repo)

    os.makedirs(a.out, exist_ok=True)
    fonts = os.path.join(a.out, "fonts")
    os.makedirs(fonts, exist_ok=True)
    for name in ("InterDisplay-Regular.ttf", "InterDisplay-Bold.ttf"):
        shutil.copy2(os.path.join(a.repo, "deploy", "app", "fonts", name), fonts)

    with open(os.path.join(a.out, "data.json"), "w", encoding="utf-8") as f:
        json.dump(dados, f, ensure_ascii=False, indent=1)
    for nome in sorted(os.listdir(AQUI)):
        if nome.endswith((".html", ".css", ".js")):
            shutil.copy2(os.path.join(AQUI, nome), os.path.join(a.out, nome))
            if a.publico and nome == "index.html":
                path = os.path.join(a.out, nome)
                with open(path, encoding="utf-8") as f:
                    html = f.read()
                html = html.replace('lang="pt-BR"', 'lang="en"').replace(
                    '<title>Nuvio Legacy - issues e roadmap</title>',
                    '<title>Nuvio Legacy - issues and roadmap</title>').replace(
                    'aria-label="Navegação"', 'aria-label="Navigation"').replace(
                    '<a href="#ajudar">Como ajudar</a>', '<a href="#ajudar">How to help</a>')
                with open(path, "w", encoding="utf-8") as f:
                    f.write(html)
    print("ok: %d issues -> %s (commit %s)" % (len(dados["itens"]), a.out, h or "?"))


if __name__ == "__main__":
    main()
