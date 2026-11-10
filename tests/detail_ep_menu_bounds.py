#!/usr/bin/env python3
"""Source guards for menu summary text width in desenhaEpisodioEm."""
from pathlib import Path
import re

source = Path('src/detail.c').read_text()
card = source.split('static void desenhaEpisodioEm(', 1)[1].split('// Abas de informacao:', 1)[0]
assert 'float epTextoW = resumo ? fmaxf(48.0f, r.w - 2.0f * NV_DETP_EP_PAD)' in card
body = card.split('float epTextoW', 1)[1]
assert 'NV_DETP_EP_TEXTO_W' not in body.split('epTextoW', 1)[1]
for needle in ('txt_linha_corta(TXT_PLR_CORPO', 'NV_DETP_EP_SIN_Y, epTextoW', 'NV_DETP_EP_BARRA_Y, epTextoW'):
    assert needle in card, needle
menu = source.split('static void menuDeVisto', 1)[1].split('static int progMelhor', 1)[0]
assert 'episodios_menu_fontes(&mt, &me)' in menu
assert 'xItem(SEC_EPISODIOS, col)' in menu
print('PASS: detail_ep_menu_bounds guards')
