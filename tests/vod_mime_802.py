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
    m = re.search(r'else if \(android\.net\.Uri\.parse\(url\)\.path\?\.endsWith\("([^"]+)"', bloco)
    assert m, 'itemMidia: regra DASH nao reconhecida; ajuste o teste'
    return 'application/dash+xml' if urllib.parse.urlparse(url).path.lower().endswith(m.group(1)) else None

CASOS = [
    # (url, mime esperado)
    ('https://media.example/film.mp4?title=Dash', None),
    ('https://media.example/film.mp4#dash', None),
    ('https://dash-cache.example/film.ts', None),
    ('https://media.example/dispatch?format=dashboard', None),
    ('https://media.example/vod/manifest.mpd', 'application/dash+xml'),
    ('https://media.example/live/sessao.MPD?token=abc', 'application/dash+xml'),
    ('https://media.example/mp4/arquivo.mp4?sig=xyz', None),
]
rc = 0
for url, esperado in CASOS:
    got = mime_da_producao(url)
    ok = got == esperado
    rc |= not ok
    print(f'{"OK " if ok else "FALHA"} {url} -> {got} (esperado {esperado})')
sys.exit(rc)
