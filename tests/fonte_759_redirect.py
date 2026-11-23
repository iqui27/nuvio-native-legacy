#!/usr/bin/env python3
# #759(b): o orcamento de redirect do debrid nao pode reabrir conexao depois
# de vencido. Extrai a regra de producao do conectar()/conectar1() e aplica
# a mesma aritmetica sobre uma linha do tempo simulada.
import re, sys

FONTE = 'android/app/src/main/java/space/nuvio/nativelegacy/ParaleloDataSource.kt'

def novo_caminho(url_inicial, respostas, agora):
    """respostas: lista de (espera_ms, http_code) por hop; agora() avanca o relogio.
    Retorna lista de hosts de conexao efetivamente abertos."""
    src = open(FONTE, encoding='utf-8').read()
    con = src[src.index('private fun conectar(u0'):src.index('internal fun conectarComRetry')]
    aborta_topo = bool(re.search(r'restante <= 0\) throw', con))
    aborta_apos_302 = bool(re.search(r'rest2 <= 0\) throw|restante <= 0\) throw[^\n]*// ?apos', con))
    assert 'limiteMs' in con and 'elapsedRealtime' in con
    limite = 25000   # connectCap de host debrid
    t0 = agora()
    hosts, u, hops = [], url_inicial, iter(respostas)
    for _ in range(10):
        restante = limite - (agora() - t0)
        if aborta_topo and restante <= 0:
            break            # producao lanca IOException sem abrir conexao
        espera, code = next(hops, (0, 200))
        hosts.append(u)      # conexao aberta
        agora(adv=espera)
        if code in range(300, 400) and code != 304:
            restante2 = limite - (agora() - t0)
            if aborta_apos_302 and restante2 <= 0:
                break
            u = u.replace('real-debrid.com', 'cdn.example')
            continue
        return hosts
    return hosts

relogio = [0]
def agora(adv=0):
    relogio[0] += adv
    return relogio[0]

# Cenario do achado: resposta 302 chega aos 25.001 ms — o prazo ja venceu.
# Antes: a candidata abria cdn.example com limites novos de 15/20 s.
hosts = novo_caminho('https://real-debrid.com/d/abc', [(25001, 302), (2000, 200)], agora)
ok = hosts == ['https://real-debrid.com/d/abc']
print('apos prazo vencido hosts abertos:', hosts, '->', 'OK' if ok else 'FALHA')

# Cenario saudavel: 302 rapido dentro do prazo segue para o CDN.
relogio[0] = 0
hosts = novo_caminho('https://real-debrid.com/d/abc', [(2000, 302), (2000, 200)], agora)
ok2 = hosts[-1].endswith('cdn.example') or len(hosts) == 2
print('redirect dentro do prazo segue:', hosts, '->', 'OK' if ok2 else 'FALHA')

sys.exit(0 if (ok and ok2) else 1)
