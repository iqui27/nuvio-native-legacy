"""Cobertura das categorias de Ajustes. Ver tests/ajustes_secoes.sh."""
import re
import sys

fonte = open(sys.argv[1], encoding="utf-8").read()

corpo = re.search(r"typedef enum \{(.*?)\n\} OpcaoId;", fonte, re.S).group(1)
corpo = re.sub(r"//[^\n]*", "", corpo)
opcoes = [n.strip() for n in corpo.split(",") if n.strip() and n.strip() != "AJ_N"]
indice = {nome: i for i, nome in enumerate(opcoes)}

tabela = re.search(r"\} SECOES\[\] = \{(.*?)\n\};", fonte, re.S).group(1)
secoes = []
for linha in tabela.splitlines():
    achou = re.search(r'\{\s*"([^"]+)".*?(AJ_[A-Z_0-9]+)\s*\}', linha)
    if achou:
        secoes.append((achou.group(1), achou.group(2)))

falhas = []
if not secoes:
    falhas.append("nenhuma categoria encontrada em SECOES[]")

for nome, ini in secoes:
    if ini not in indice:
        falhas.append('categoria "%s" comeca em %s, que nao existe no enum' % (nome, ini))

if not falhas:
    if indice[secoes[0][1]] != 0:
        falhas.append('a primeira categoria ("%s") nao comeca na primeira opcao (%s)'
                      % (secoes[0][0], opcoes[0]))
    # ORDEM CRESCENTE E O QUE GARANTE A COBERTURA. Com `n` derivado do `ini` da
    # proxima categoria, faixas em ordem crescente cobrem o enum inteiro por
    # construcao; fora de ordem, uma faixa teria tamanho negativo e as opcoes
    # entre as duas ficariam sem linha, que e o defeito original.
    for i in range(1, len(secoes)):
        ant, atual = secoes[i - 1], secoes[i]
        if indice[atual[1]] <= indice[ant[1]]:
            falhas.append('categoria "%s" (%s) comeca antes ou junto de "%s" (%s)'
                          % (atual[0], atual[1], ant[0], ant[1]))

if falhas:
    for f in falhas:
        print("FALHA: " + f)
    sys.exit(1)

cobertas = len(opcoes)
print("PASS: %d opcoes de Ajustes em %d categorias, nenhuma orfa."
      % (cobertas, len(secoes)))
