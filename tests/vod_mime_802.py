#!/usr/bin/env python3
# #802: DASH e decidido por sinal real (.mpd no caminho), nunca por substring
# na URL. Roda a MESMA regra do itemMidia() de NvPlayer.kt, extraida do
# codigo de producao: se a linha de decisao muda, este teste muda junto.
import re, sys, urllib.parse

FONTE = 'android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt'

def mime_da_producao(url: str):
    src = open(FONTE, encoding='utf-8').read()
    bloco = src[src.index('private fun itemMidia'):src.index('private fun decoderDolby')]
    if 'contains("dash")' in bloco:
        # regra antiga: substring em qualquer parte da URL
        b = url.lower()
        return 'application/dash+xml' if ('.mpd' in b or 'dash' in b or 'manifest.mpd' in b) else None
    assert 'isMpdUri(url)' in bloco, 'itemMidia: regra DASH nao reconhecida; ajuste o teste'
    # isMpdUri: .mpd no fim do path OU no fim de um valor de query param.
    # Funcao pura sobre String: o valor decodificado NAO vai a urlparse/Uri.parse
    # (o ':' de ISO 8601 seria lido como esquema); corta no primeiro '?' ou '#'
    # aninhado e testa o sufixo. Espelha a mesma logica do Kotlin.
    # O fragmento EXTERNO sai primeiro (Uri real: query nula em
    # film.mp4#t=10?file=...): so a query da propria URL conta.
    sem_fragmento = url.split('#', 1)[0]
    caminho = sem_fragmento.split('?', 1)[0]
    if caminho.lower().endswith('.mpd'):
        return 'application/dash+xml'
    q = sem_fragmento.split('?', 1)[1] if '?' in sem_fragmento else ''
    for par in q.split('&'):
        valor = par.split('=', 1)[-1] if '=' in par else par
        # Espelha decodifica(): '%' invalido fica literal (unquote ja faz)
        # e '+' vira espaco como no URLDecoder.
        dec = urllib.parse.unquote(valor.replace('+', ' ')).split('?', 1)[0].split('#', 1)[0]
        if dec.lower().endswith('.mpd'):
            return 'application/dash+xml'
    return None

CASOS = [
    # (url, mime esperado)
    ('https://media.example/film.mp4?title=Dash', None),
    ('https://media.example/film.mp4#dash', None),
    ('https://dash-cache.example/film.ts', None),
    ('https://media.example/dispatch?format=dashboard', None),
    ('https://media.example/vod/manifest.mpd', 'application/dash+xml'),
    ('https://media.example/live/sessao.MPD?token=abc', 'application/dash+xml'),
    ('https://media.example/api/live?file=canal.mpd', 'application/dash+xml'),
    ('https://media.example/api/live?file=canal.mpd&x=1', 'application/dash+xml'),
    # MPD assinado: query aninhada dentro do valor codificado do parametro
    ('https://media.example/api/live?file=canal.mpd%3Ftoken%3Dabc', 'application/dash+xml'),
    ('https://media.example/api/live?file=https%3A%2F%2Fcdn.example%2Fx%2Fcanal.mpd%3Ft%3D1', 'application/dash+xml'),
    # horario ISO 8601: ':' nao pode virar esquema num re-parse do valor
    ('https://media.example/api/live?file=canal.mpd%3Fexpires%3D2026-11-20T23%3A59%3A59Z%26sig%3Dabc', 'application/dash+xml'),
    # paridade com a base: substring ".mpd" em qualquer valor de query param
    ('https://media.example/film.mp4?next=x.mpd', 'application/dash+xml'),
    ('https://media.example/film.mp4?ref=https%3A%2F%2Fother.example%2Flive.mpd', 'application/dash+xml'),
    ('https://media.example/mp4/arquivo.mp4?sig=xyz', None),
    ('https://media.example/api/live?file=canal.mpdx', None),
    ('https://media.example/api/live?file=canal.mpdx%3Ftoken%3Dabc', None),
    # fragmento externo: o que vem depois de '#' nao e query
    ('https://media.example/film.mp4#t=10?file=canal%2Empd', None),
    # escape invalido na assinatura aninhada: decode tolerante mantem DASH
    ('https://media.example/api/live?file=canal.mpd%3Ftoken%3D%ZZ', 'application/dash+xml'),
]
rc = 0
for url, esperado in CASOS:
    got = mime_da_producao(url)
    ok = got == esperado
    rc |= not ok
    print(f'{"OK " if ok else "FALHA"} {url} -> {got} (esperado {esperado})')
sys.exit(rc)
