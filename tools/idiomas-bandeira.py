#!/usr/bin/env python3
"""Language -> flag table for the source list (formatter text, "From addon").

Writes src/formatador_idiomas.inc: { "key", "CC" } sorted by key, where key is a
lowercase ISO 639-1/639-2 code, a BCP-47 tag ("pt-br", "es-419") or an English
language name ("english", "portuguese (brazil)", "latino"), and CC the flagpack
country code. Source: AIOStreams' own language list, so a code NuvioLN turns
into a flag gets the same flag AIOStreams' formatters print:

  Viren070/AIOStreams packages/core/src/utils/language-list.ts  (FULL_LANGUAGE_MAPPING)
                      packages/core/src/utils/languages.ts      (languageEmojiMap overrides)
  pinned at 113ab6b (2026-11-14)

Usage: python3 tools/idiomas-bandeira.py <dir with the two .ts files>
"""
import os, re, sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = sys.argv[1] if len(sys.argv) > 1 else '/tmp/aio-lang'
lista = open(os.path.join(src, 'language-list.ts'), encoding='utf-8').read()
langs = open(os.path.join(src, 'languages.ts'), encoding='utf-8').read()

def flag_cc(f):
    cps = [ord(c) for c in f if 0x1F1E6 <= ord(c) <= 0x1F1FF]
    return ''.join(chr(c - 0x1F1E6 + 65) for c in cps) if len(cps) == 2 else None

entradas = []
for bloco in re.findall(r'\{([^{}]*?iso_639_2[^{}]*?)\}', lista, re.S):
    def campo(n):
        m = re.search(n + r":\s*('([^']*)'|null|true)", bloco)
        return None if not m or m.group(1) == 'null' else (m.group(2) if m.group(2) is not None else m.group(1))
    e = dict(i1=campo('iso_639_1'), i2=campo('iso_639_2'), cc=campo('iso_3166_1'),
             en=campo('english_name'), interno=campo('internal_english_name'),
             pri=campo('flag_priority') == 'true')
    if e['cc'] and e['i2']:
        entradas.append(e)

tab = {}
def poe(chave, cc, forte=False):
    chave = chave.strip().lower()
    if chave and (forte or chave not in tab):
        tab[chave] = cc

# Base codes and names: flag_priority entry first, then the first entry.
for e in sorted(entradas, key=lambda e: not e['pri']):
    for k in (e['i1'], e['i2']):
        if k: poe(k, e['cc'])
    base = re.sub(r'\s*\(.*\)$', '', e['en'] or '')
    poe(base, e['cc'])
# Exact names and region tags of every entry.
for e in entradas:
    for k in (e['en'], e['interno']):
        if k: poe(k, e['cc'], forte=True)
    if e['i1']:
        poe(e['i1'] + '-' + e['cc'], e['cc'], forte=True)
# AIOStreams' formatter overrides (languageEmojiMap): english = 🇬🇧, latino = 🇲🇽 ...
mapa = re.search(r'languageEmojiMap[^{]*\{(.*?)\};', langs, re.S).group(1)
for nome, f in re.findall(r"'?([A-Za-z() ]+?)'?\s*:\s*'([^']+)'", mapa):
    cc = flag_cc(f)
    if cc:
        poe(nome, cc, forte=True)
        for e in entradas:   # the codes follow the name ("en" -> GB, "pt" -> PT)
            if (e['interno'] or e['en'] or '').lower() == nome.lower() and e['i1'] and '(' not in nome:
                poe(e['i1'], cc, forte=True); poe(e['i2'], cc, forte=True)
for k, cc in {'es-419': 'MX', 'es-la': 'MX', 'lat': 'MX', 'pt-br': 'BR', 'pt-pt': 'PT', 'br': 'BR',
              'en-us': 'US', 'en-gb': 'GB', 'zh-tw': 'TW', 'zh-hant': 'TW', 'zh-cn': 'CN', 'zh-hans': 'CN',
              'fr-ca': 'CA', 'brazilian': 'BR', 'brazilian portuguese': 'BR', 'castilian': 'ES',
              'spanish (latin america)': 'MX', 'latin american spanish': 'MX'}.items():
    poe(k, cc, forte=True)
# Native names (endonyms): addons that print a language in its own script
# ("العربية", "Español", "日本語") get the same flag as the English name. Weak
# (forte=False): an English name or code with the same spelling keeps its flag.
# The C lookup folds ASCII case only, so each name goes in as written, in
# lower case and in upper case (ASCII letters folded in all three).
NATIVOS = {
    'العربية': 'SA', 'عربي': 'SA', 'বাংলা': 'BD', 'български': 'BG', 'català': 'ES',
    'čeština': 'CZ', 'česky': 'CZ', 'dansk': 'DK', 'deutsch': 'DE', 'ελληνικά': 'GR',
    'español': 'ES', 'castellano': 'ES', 'español (latinoamérica)': 'MX', 'español latino': 'MX',
    'eesti': 'EE', 'euskara': 'ES', 'فارسی': 'IR', 'suomi': 'FI', 'français': 'FR',
    'français (canada)': 'CA', 'galego': 'ES', 'עברית': 'IL', 'हिन्दी': 'IN', 'hrvatski': 'HR',
    'magyar': 'HU', 'bahasa indonesia': 'ID', 'íslenska': 'IS', 'italiano': 'IT', '日本語': 'JP',
    'ქართული': 'GE', 'қазақ тілі': 'KZ', 'ខ្មែរ': 'KH', 'ಕನ್ನಡ': 'IN', '한국어': 'KR',
    'lietuvių': 'LT', 'latviešu': 'LV', 'македонски': 'MK', 'മലയാളം': 'IN', 'монгол': 'MN',
    'मराठी': 'IN', 'bahasa melayu': 'MY', 'nederlands': 'NL', 'norsk': 'NO', 'norsk bokmål': 'NO',
    'ਪੰਜਾਬੀ': 'IN', 'polski': 'PL', 'português': 'PT', 'português (brasil)': 'BR',
    'português do brasil': 'BR', 'română': 'RO', 'русский': 'RU', 'slovenčina': 'SK',
    'slovenščina': 'SI', 'shqip': 'AL', 'српски': 'RS', 'srpski': 'RS', 'svenska': 'SE',
    'kiswahili': 'TZ', 'தமிழ்': 'IN', 'తెలుగు': 'IN', 'ไทย': 'TH', 'filipino': 'PH',
    'tagalog': 'PH', 'türkçe': 'TR', 'українська': 'UA', 'اردو': 'PK', 'oʻzbekcha': 'UZ',
    'tiếng việt': 'VN', '中文': 'CN', '简体中文': 'CN', '繁體中文': 'TW', '國語': 'TW',
    '普通话': 'CN', '粵語': 'HK', '廣東話': 'HK', 'bosanski': 'BA',
}
def soAscii(s, f):
    return ''.join(f(c) if c.isascii() else c for c in s)
for nome, cc in NATIVOS.items():
    for v in {nome, nome.lower(), nome.upper(), nome[:1].upper() + nome[1:]}:
        tab.setdefault(soAscii(v, str.lower).strip(), cc)   # not poe(): it lowercases non-ASCII too
for k in ('multi', 'und', 'mul', 'zxx', 'mis'):
    tab.pop(k, None)

with open(os.path.join(RAIZ, 'src/formatador_idiomas.inc'), 'w', encoding='utf-8') as f:
    f.write('// GERADO por tools/idiomas-bandeira.py (lista de idiomas do AIOStreams 113ab6b).\n')
    f.write('// Nao editar a mao. Ordenado por chave (strcmp): busca binaria.\n')
    for k in sorted(tab, key=lambda s: s.encode()):
        f.write('{ "%s", "%s" },\n' % (k.replace('"', '\\"'), tab[k]))
print(len(tab), 'keys')
