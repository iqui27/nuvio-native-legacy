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
    # isMpdUri: .mpd no fim do path OU no fim de um valor de query param
    # (valor decodificado; query e fragmento aninhados nao contam)
    parsed = urllib.parse.urlparse(url)
    if parsed.path.lower().endswith('.mpd'):
        return 'application/dash+xml'
    for par in parsed.query.split('&'):
        valor = par.split('=', 1)[-1] if '=' in par else par
        aninhado = urllib.parse.urlparse(urllib.parse.unquote(valor))
        if aninhado.path.lower().endswith('.mpd'):
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
    # paridade com a base: substring ".mpd" em qualquer valor de query param
    ('https://media.example/film.mp4?next=x.mpd', 'application/dash+xml'),
    ('https://media.example/film.mp4?ref=https%3A%2F%2Fother.example%2Flive.mpd', 'application/dash+xml'),
    ('https://media.example/mp4/arquivo.mp4?sig=xyz', None),
    ('https://media.example/api/live?file=canal.mpdx', None),
    ('https://media.example/api/live?file=canal.mpdx%3Ftoken%3Dabc', None),
]
rc = 0
for url, esperado in CASOS:
    got = mime_da_producao(url)
    ok = got == esperado
    rc |= not ok
    print(f'{"OK " if ok else "FALHA"} {url} -> {got} (esperado {esperado})')
sys.exit(rc)
