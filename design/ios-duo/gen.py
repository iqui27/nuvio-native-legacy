#!/usr/bin/env python3
"""Nuvio on iPhone Duo (iOS 27.2) and Mac: static mockups.

Builds one HTML page per flow in out/, each page holding the device frames
(outer display closed, inner display open; flat, book or tabletop pose), then
screenshots them with Playwright and writes JPGs <= 2000 px for the owner.

Device numbers come from Apple's design resource (pt): outer 466x678,
inner 951x669 (fold at 475.5), vertical bar 84, horizontal bars 84/95,
layout margin 20. They are mockup constants only; code must read size class,
safe area and ReservedRegion at runtime.

Usage: python3 gen.py [--no-shot]   (fonts embedded, art by file://)
"""
import base64, re, subprocess, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
ART = ROOT / 'deploy/app/art'
ICO = ART / 'icones/lucide'
FONTS = ROOT / 'deploy/app/fonts'
GLASS = ROOT / 'design/glass-ilha'
OUT = HERE / 'out'
ENVIO = Path('/Volumes/ExternalSSD/tmp/rev/dono-envio')

FALLOUT = GLASS / 'arte.jpg'
FALLOUT_LOGO = ART / 'logo/00.png'
PRADA = GLASS / 'detalhe.jpg'
MARTIAN = ART / '12.jpg'
QR = ART / 'marcas/discord-qr.png'
SYMBOL = ROOT / 'design/logo-novo/simbolo.svg'


def f(p):
    return f'file://{p}'


def ic(name, cls=''):
    flip = name == 'chevron-left'          # Lucide set in the repo has no chevron-left
    svg = (ICO / f'{"chevron-right" if flip else name}.svg').read_text()
    svg = re.sub(r'<svg([^>]*)>', lambda m: '<svg' + re.sub(r'\s(width|height)="[^"]*"', '', m.group(1)) + '>', svg, count=1)
    return f'<i class="ic {cls}"{" style=\"transform:scaleX(-1)\"" if flip else ""}>{svg}</i>'


def b64(p, mime):
    return f'data:{mime};base64,' + base64.b64encode(p.read_bytes()).decode()


def font_css():
    out = ''
    for w, file in ((400, 'InterDisplay-Regular.ttf'), (500, 'InterDisplay-Medium.ttf'), (700, 'InterDisplay-Bold.ttf')):
        out += f"@font-face{{font-family:Inter;font-weight:{w};src:url('{b64(FONTS / file, 'font/ttf')}')}}\n"
    return out


CSS = r"""
:root{--ac:#EC407A;--bg:#0D0D0D;--fg:#F3F2EF;--mut:rgba(243,242,239,.58);--mut2:rgba(243,242,239,.42);
  --ilha:rgba(14,15,18,.80);--foco:rgba(255,255,255,.13);--leve:rgba(255,255,255,.08);
  --glass:rgba(255,255,255,.11);--glassb:rgba(255,255,255,.17)}
*{box-sizing:border-box}
body{margin:0;background:#141416;color:var(--fg);font-family:Inter,system-ui,sans-serif;-webkit-font-smoothing:antialiased;font-weight:400}
.page{display:flex;gap:44px;padding:40px 44px 36px;align-items:flex-start;width:max-content}
.head{padding:28px 44px 0;display:flex;gap:16px;align-items:baseline}
.head h1{font-size:18px;font-weight:700;margin:0;letter-spacing:-.01em}
.head p{font-size:13px;color:#9a9a9a;margin:0;max-width:980px;line-height:1.45}
.head p b{color:#ddd;font-weight:600}
.fig{display:flex;flex-direction:column;gap:12px;align-items:center}
.cap{font-size:12.5px;color:#8f8f93;letter-spacing:.03em;text-align:center;line-height:1.5}
.cap b{color:#d8d8dc;font-weight:600}
.dev{position:relative;overflow:hidden;background:var(--bg);border-radius:48px;box-shadow:0 0 0 2px #2b2b2f,0 0 0 8px #0a0a0b,0 36px 90px rgba(0,0,0,.65);font-size:15px;line-height:1.3}
.dev.outer{width:466px;height:678px}
.dev.outer.land{width:678px;height:466px}
.dev.inner{width:951px;height:669px;border-radius:34px}
.dev.inner.port{width:669px;height:951px;border-radius:34px}
.dev.mac{width:1280px;height:800px;border-radius:14px;box-shadow:0 0 0 1px #333,0 30px 80px rgba(0,0,0,.6)}
.scr{position:absolute;left:0;top:0;bottom:0;right:0;overflow:hidden}
.scr.full{right:0}
.dev.port .scr{right:0}
/* fold: division region. flat = inactive (hairline), book/tabletop = active band */
.fold{display:none}
.fold::after{content:"";position:absolute;left:-.5px;top:0;bottom:0;width:1px;background:rgba(255,255,255,.07)}
.fold.on{width:28px;margin-left:-14px;background:linear-gradient(90deg,rgba(0,0,0,0),rgba(0,0,0,.42) 35%,rgba(0,0,0,.62) 50%,rgba(0,0,0,.42) 65%,rgba(0,0,0,0))}
.fold.on::after{left:13.5px;background:rgba(255,255,255,.10)}
.dev.port .fold{left:0;right:0;top:475.5px;bottom:auto;width:auto;height:0}
.dev.port .fold::after{left:0;right:0;top:-.5px;bottom:auto;height:1px;width:auto}
.dev.port .fold.on{height:28px;width:auto;margin-left:0;margin-top:-14px;background:linear-gradient(180deg,rgba(0,0,0,0),rgba(0,0,0,.42) 35%,rgba(0,0,0,.62) 50%,rgba(0,0,0,.42) 65%,rgba(0,0,0,0))}
.dev.port .fold.on::after{top:13.5px}
/* vertical bar (iOS 27.1): status, nav, tools, tabs share one 84 pt region */
.vbar{position:absolute;top:0;right:0;bottom:0;width:84px;display:flex;flex-direction:column;align-items:center;padding:12px 0 16px;z-index:30}
.vbar.over{background:none}
.st{display:flex;flex-direction:column;align-items:center;gap:6px;height:88px;font-size:15px;font-weight:600;letter-spacing:.01em;color:#fff}
.st .tm0{font-size:15px}
.ring{position:relative;width:46px;height:46px;border-radius:50%;background:conic-gradient(from 0deg,#fff 83%,rgba(255,255,255,.18) 0);-webkit-mask:radial-gradient(circle,transparent 62%,#000 64%);mask:radial-gradient(circle,transparent 62%,#000 64%)}
.ring em{position:absolute;left:0;right:0;top:-3px;text-align:center;font-style:normal;font-size:9px;font-weight:700}
.ring .ic{position:absolute;left:50%;top:50%;transform:translate(-50%,-22%);width:16px;height:16px}
.st .ring{-webkit-mask:none;mask:none;background:none}
.st .ring::before{content:"";position:absolute;inset:0;border-radius:50%;background:conic-gradient(from 200deg,#fff 83%,rgba(255,255,255,.18) 0);-webkit-mask:radial-gradient(circle,transparent 60%,#000 62%);mask:radial-gradient(circle,transparent 60%,#000 62%)}
.st .ring em{top:6px;font-size:10px}
.st .cam{width:12px;height:12px;border-radius:50%;background:#050506;box-shadow:inset 0 0 0 2px #17181b,0 0 0 1px rgba(255,255,255,.06);margin-bottom:4px}
.st .cam.hidden{visibility:hidden}
.st .sig{display:flex;gap:3px;align-items:flex-end;height:9px}
.st .sig b{display:block;width:3px;background:#fff;border-radius:1px}
.st .bat{width:22px;height:10px;border:1.5px solid rgba(255,255,255,.6);border-radius:3px;position:relative;margin-top:1px}
.st .bat::after{content:"";position:absolute;left:1.5px;top:1.5px;bottom:1.5px;width:70%;background:#fff;border-radius:1px}
.gl{backdrop-filter:blur(28px) saturate(1.6);-webkit-backdrop-filter:blur(28px) saturate(1.6);background:var(--glass);box-shadow:inset 0 1px 0 rgba(255,255,255,.28),inset 0 -1px 0 rgba(255,255,255,.06),0 10px 28px rgba(0,0,0,.42)}
.nav{display:flex;flex-direction:column;gap:10px;margin-top:6px;align-items:center}
.nb{width:48px;height:48px;border-radius:24px;display:flex;align-items:center;justify-content:center;color:#fff}
.nb.pro{background:var(--ac);color:#fff;box-shadow:0 8px 22px rgba(236,64,122,.4)}
.nb .ic{width:22px;height:22px}
.tools{display:flex;flex-direction:column;gap:10px;margin-top:18px;align-items:center}
.tools .nb{width:44px;height:44px;border-radius:22px}
.tools .nb .ic{width:21px;height:21px}
.vsp{flex:1}
.tabs{width:60px;border-radius:30px;padding:10px 0;display:flex;flex-direction:column;gap:10px;align-items:center}
.tabs i.tb{width:48px;height:48px;border-radius:24px;display:flex;align-items:center;justify-content:center;color:rgba(255,255,255,.78);position:relative}
.tabs i.tb.on{color:var(--ac)}
.tabs i.tb .ic{width:24px;height:24px}
.tabs i.tb.on .ic svg{fill:var(--ac);stroke:var(--ac)}
.tabs i.tb .avp{width:30px;height:30px;border-radius:50%;background:linear-gradient(135deg,#5B6CF2,#E07A3F);display:block}
.tabs i.tb b{position:absolute;top:4px;right:4px;background:var(--ac);color:#fff;font-size:9px;font-weight:700;min-width:15px;height:15px;border-radius:8px;display:flex;align-items:center;justify-content:center;padding:0 3px}
/* horizontal bars (inner display, portrait) */
.hbar{position:absolute;left:0;right:0;top:0;height:84px;z-index:30;padding:10px 20px 0}
.hbar .sth{display:flex;justify-content:space-between;font-size:13px;font-weight:600;height:22px;align-items:center}
.hbar .sth .r{display:flex;gap:6px;align-items:center}
.hbar .nbar{display:flex;align-items:center;gap:10px;height:48px}
.hbar .nbar .ttl{font-size:17px;font-weight:700;flex:1;text-align:center}
.tabsh{position:absolute;left:50%;bottom:20px;transform:translateX(-50%);height:60px;border-radius:30px;padding:0 8px;display:flex;gap:4px;align-items:center;z-index:30}
.tabsh i.tb{width:52px;height:48px;border-radius:24px;display:flex;align-items:center;justify-content:center;color:rgba(255,255,255,.62);position:relative}
.tabsh i.tb.on{color:var(--ac)}
.tabsh i.tb.on .ic svg{fill:var(--ac);stroke:var(--ac)}
.tabsh i.tb .avp{width:28px;height:28px;border-radius:50%;background:linear-gradient(135deg,#5B6CF2,#E07A3F);display:block}
.tabsh i.tb .ic{width:22px;height:22px}

/* icons */
.ic{display:inline-flex;width:22px;height:22px;flex:none;vertical-align:middle}
.ic svg{width:100%;height:100%;fill:none;stroke:currentColor;stroke-width:1.9;stroke-linecap:round;stroke-linejoin:round}
.ic.s{width:16px;height:16px}.ic.m{width:18px;height:18px}.ic.g{width:28px;height:28px}.ic.xl{width:40px;height:40px}
/* Nuvio material */
.ilha{background:var(--ilha);box-shadow:0 14px 40px rgba(0,0,0,.36);position:relative;overflow:hidden;backdrop-filter:blur(26px);-webkit-backdrop-filter:blur(26px)}
.ilha::before{content:"";position:absolute;inset:0;background:radial-gradient(120% 80% at 22% -40%,rgba(255,255,255,.09),rgba(255,255,255,0) 60%);pointer-events:none}
.kick{font-size:11px;font-weight:700;letter-spacing:.13em;text-transform:uppercase;color:var(--mut2)}
.kick.ac{color:var(--ac)}
.h1{font-size:28px;font-weight:700;letter-spacing:-.02em;margin:0;line-height:1.05}
.h2{font-size:20px;font-weight:700;letter-spacing:-.01em;margin:0}
.h3{font-size:16px;font-weight:700;margin:0}
.sub{font-size:13.5px;color:var(--mut);line-height:1.45}
.meta{display:flex;gap:9px;align-items:center;font-size:13px;color:rgba(243,242,239,.72);flex-wrap:wrap}
.meta .pt{width:3px;height:3px;border-radius:50%;background:rgba(255,255,255,.35)}
.tag{font-size:10.5px;font-weight:700;letter-spacing:.05em;padding:2px 6px;border-radius:5px;border:1.2px solid rgba(255,255,255,.35);color:rgba(255,255,255,.85)}
.pill{display:inline-flex;align-items:center;gap:8px;white-space:nowrap;height:44px;padding:0 20px;border-radius:22px;background:var(--ac);color:#fff;font-weight:700;font-size:15px;box-shadow:0 10px 26px rgba(236,64,122,.35)}
.pill.w{background:#F3F2EF;color:#111;box-shadow:0 10px 26px rgba(0,0,0,.35)}
.pill.g{background:rgba(255,255,255,.10);color:#fff;box-shadow:none;font-weight:600}
.pill .ic{width:18px;height:18px}
.dsc{width:44px;height:44px;border-radius:22px;background:rgba(255,255,255,.10);display:inline-flex;align-items:center;justify-content:center;color:#fff}
.dsc.on{background:var(--foco)}
.dsc .ic{width:20px;height:20px}
.chip{display:inline-flex;align-items:center;gap:6px;height:32px;padding:0 12px;border-radius:16px;background:rgba(255,255,255,.08);font-size:12.5px;font-weight:600;color:rgba(243,242,239,.85)}
.chip.on{background:rgba(236,64,122,.22);color:var(--ac)}
.chip .ic{width:15px;height:15px}
.seg{display:inline-flex;gap:3px;padding:3px;border-radius:999px;background:rgba(255,255,255,.06)}
.seg span{font-size:12.5px;font-weight:600;color:var(--mut);padding:7px 13px;border-radius:999px;display:inline-flex;gap:6px;align-items:center}
.seg span.on{background:rgba(255,255,255,.14);color:#fff}
.seg span em{font-style:normal;color:var(--mut2);font-weight:500;font-size:11.5px}
.qline{display:inline-flex;align-items:center;gap:8px;font-size:12px;color:rgba(243,242,239,.8);background:rgba(255,255,255,.07);padding:5px 10px 5px 8px;border-radius:9px}
.qline i.dot{width:6px;height:6px;border-radius:50%;background:#59D46B}
.sec{padding:18px 20px 0}
.rowh{display:flex;align-items:baseline;gap:10px;margin-bottom:10px}
.rowh .h2{font-size:17px}
.rowh span{font-size:12px;color:var(--mut2)}
.rowh em{margin-left:auto;font-style:normal;font-size:12px;color:var(--mut2);display:flex;gap:4px;align-items:center}
.strip{display:flex;gap:10px}
.poster{border-radius:12px;overflow:hidden;background:#1a1b1f;flex:none;position:relative}
.poster img{width:100%;height:100%;object-fit:cover;display:block}
.poster.foc{box-shadow:0 14px 30px rgba(0,0,0,.5);transform:scale(1.04)}
.card{border-radius:12px;overflow:hidden;background:#1a1b1f;flex:none;position:relative}
.card img{width:100%;height:100%;object-fit:cover;display:block}
.card .pr{position:absolute;left:0;right:0;bottom:0;height:3px;background:rgba(255,255,255,.18)}
.card .pr b{display:block;height:100%;background:var(--ac)}
.card .lb{position:absolute;left:8px;bottom:9px;font-size:11px;font-weight:600;text-shadow:0 1px 3px rgba(0,0,0,.6)}
.card .pl{position:absolute;right:8px;top:8px;width:26px;height:26px;border-radius:50%;background:rgba(14,15,18,.7);display:flex;align-items:center;justify-content:center}
.card .pl .ic{width:13px;height:13px}
.hero{position:relative;overflow:hidden}
.hero img.bg{position:absolute;inset:0;width:100%;height:100%;object-fit:cover}
.hero .veu{position:absolute;inset:0;background:linear-gradient(180deg,rgba(13,13,13,0) 35%,rgba(13,13,13,.55) 70%,var(--bg) 100%)}
.hero .veu.l{background:linear-gradient(90deg,rgba(13,13,13,.92) 0,rgba(13,13,13,.72) 32%,rgba(13,13,13,.12) 60%,rgba(13,13,13,0) 75%),linear-gradient(180deg,rgba(13,13,13,0) 55%,var(--bg) 100%)}
.hero .txt{position:absolute;left:20px;bottom:18px;display:flex;flex-direction:column;gap:10px}
.hero .logo{height:64px;width:auto;object-fit:contain;object-position:left}
.hero.full{position:absolute;inset:0}
.hero.full .logo{height:112px}
.meta.big{font-size:18px;font-weight:600;color:rgba(243,242,239,.9)}
.meta.big .pt{width:4px;height:4px}
.sub.big{font-size:16.5px;line-height:1.5;max-width:600px}
.pill.xl{height:56px;padding:0 30px;font-size:18px;border-radius:28px}
.pill.xl .ic{width:22px;height:22px}
.pill.wide{width:100%;justify-content:center}
.dsc.xl{width:56px;height:56px;border-radius:28px;background:rgba(255,255,255,.12)}
.dsc.xl .ic{width:24px;height:24px}
.stars{display:flex;gap:8px;color:rgba(255,255,255,.38)}
.stars .ic{width:30px;height:30px}
.stars .ic.on{color:var(--ac)}.stars .ic.on svg{fill:var(--ac)}
.dots{display:flex;gap:9px;align-items:center}
.dots i{width:8px;height:8px;border-radius:4px;background:rgba(255,255,255,.55);display:block}
.dots i.on{width:34px;background:#fff}
.rate{display:inline-flex;align-items:center;gap:6px;font-size:16px;font-weight:600}
.rate .lg{font-size:11px;font-weight:700;letter-spacing:.06em;padding:3px 6px;border-radius:5px;background:rgba(255,255,255,.14);color:rgba(243,242,239,.85)}
.rate .lg.imdb{background:#F5C518;color:#111}
.peek{position:absolute;left:0;right:0;bottom:0;padding:0 20px}
.peek .h2{font-size:20px}
.acts{display:flex;gap:10px;align-items:center}
.lrow{display:flex;align-items:center;gap:12px;padding:10px 12px;border-radius:14px}
.lrow.foc{background:var(--foco)}
.lrow .th{width:112px;height:63px;border-radius:9px;overflow:hidden;flex:none;background:#1a1b1f;position:relative}
.lrow .th img{width:100%;height:100%;object-fit:cover;display:block}
.lrow .th .pr{position:absolute;left:0;right:0;bottom:0;height:3px;background:rgba(255,255,255,.18)}
.lrow .th .pr b{display:block;height:100%;background:var(--ac)}
.lrow .tx{flex:1;min-width:0}
.lrow .tx .t{font-size:14.5px;font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.lrow .tx .s{font-size:12px;color:var(--mut);margin-top:2px}
.lrow .rt{font-size:12px;color:var(--mut2);display:flex;align-items:center;gap:6px}
.grp{display:flex;align-items:baseline;gap:8px;padding:14px 12px 6px;border-bottom:1px solid rgba(255,255,255,.08);margin-bottom:4px}
.grp b{font-size:17px;font-weight:700}
.grp span{font-size:11px;font-weight:700;letter-spacing:.12em;color:var(--mut2)}
.grp em{margin-left:auto;font-style:normal;font-size:12px;color:var(--mut2)}
.src{padding:10px 12px;border-radius:14px}
.src.best{background:var(--foco)}
.src .n{font-size:15px;font-weight:600;display:flex;justify-content:space-between;align-items:center}
.src .n span{font-size:12px;color:var(--mut2);display:flex;gap:6px;align-items:center;font-weight:500}
.src .n span i{width:6px;height:6px;border-radius:50%;background:#59D46B}
.src .b{display:flex;gap:8px;align-items:center;margin-top:4px;font-size:12.5px;color:rgba(243,242,239,.7)}
.src .b img{height:11px;opacity:.6}
.src .d{font-size:12px;color:var(--mut2);margin-top:3px}
.badge{display:inline-block;font-size:10.5px;font-weight:700;letter-spacing:.04em;color:rgba(243,242,239,.72)}
.av{border-radius:50%;display:flex;align-items:center;justify-content:center;font-weight:700;color:#fff;flex:none}
.setrow{display:flex;align-items:center;gap:12px;height:52px;padding:0 14px}
.setrow .ic{width:20px;height:20px;color:rgba(243,242,239,.8)}
.setrow .l{flex:1;font-size:15px;font-weight:500}
.setrow .v{font-size:13px;color:var(--mut2)}
.setrow .ch{color:var(--mut2)}
.setrow .ch .ic{width:16px;height:16px}
.setrow.on{background:var(--foco);border-radius:14px}
.sw{width:42px;height:26px;border-radius:13px;background:rgba(255,255,255,.14);position:relative;flex:none}
.sw::after{content:"";position:absolute;top:3px;left:3px;width:20px;height:20px;border-radius:50%;background:#fff}
.sw.on{background:var(--ac)}.sw.on::after{left:19px}
.ilist{border-radius:18px;overflow:hidden}
.ilist .setrow+.setrow{border-top:1px solid rgba(255,255,255,.06)}
.scrub{position:relative;height:4px;border-radius:2px;background:rgba(255,255,255,.22)}
.scrub b{position:absolute;left:0;top:0;bottom:0;border-radius:2px;background:#fff}
.scrub i{position:absolute;top:50%;width:14px;height:14px;border-radius:50%;background:#fff;transform:translate(-50%,-50%);box-shadow:0 2px 8px rgba(0,0,0,.5)}
.tm{display:flex;justify-content:space-between;font-size:12px;color:rgba(255,255,255,.75);font-variant-numeric:tabular-nums;margin-top:8px}
.pc{width:64px;height:64px;border-radius:50%;display:flex;align-items:center;justify-content:center;color:#fff}
.pc .ic{width:30px;height:30px}
.pc.big{width:76px;height:76px}
.pc.big .ic{width:34px;height:34px}
.pc.big .ic svg{fill:#fff;stroke-width:1.2}
.pc.gl{background:rgba(16,17,20,.42);box-shadow:inset 0 1px 0 rgba(255,255,255,.22),0 10px 28px rgba(0,0,0,.45)}
.vfill{position:absolute;inset:0;object-fit:cover;width:100%;height:100%}
.dim{position:absolute;inset:0;background:rgba(0,0,0,.42)}
.field{display:flex;align-items:center;gap:10px;height:46px;border-radius:23px;padding:0 14px 0 16px;background:rgba(255,255,255,.09);color:var(--mut);font-size:15px}
.field .ic{width:20px;height:20px;color:rgba(243,242,239,.8)}
.field span{flex:1}
.gridp{display:grid;gap:10px}
.gridp .poster{width:auto}
.ptitle{font-size:12px;font-weight:600;margin-top:6px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.psub{font-size:11px;color:var(--mut2)}
.cell{display:flex;flex-direction:column}
.guide .ch{display:grid;grid-template-columns:64px 1fr;gap:10px;align-items:center;padding:8px 0;border-bottom:1px solid rgba(255,255,255,.06)}
.guide .ch .lg{height:40px;border-radius:9px;background:rgba(255,255,255,.08);display:flex;align-items:center;justify-content:center;font-size:10.5px;font-weight:700;letter-spacing:.04em;color:rgba(243,242,239,.8)}
.guide .ch .pg{display:flex;gap:6px}
.guide .ch .pg div{height:40px;border-radius:9px;background:rgba(255,255,255,.055);padding:5px 8px;font-size:11.5px;font-weight:600;overflow:hidden;white-space:nowrap;text-overflow:ellipsis;position:relative}
.guide .ch .pg div.now{background:var(--foco)}
.guide .ch .pg div.now::after{content:"";position:absolute;left:0;right:0;bottom:0;height:2px;background:var(--ac);width:var(--p,40%)}
.guide .ch .pg div small{display:block;font-weight:400;color:var(--mut2);font-size:10.5px}
.tl{display:grid;grid-template-columns:64px 1fr;gap:10px;font-size:11px;color:var(--mut2);font-weight:600;padding-bottom:4px}
.tl .t{display:flex;gap:6px}
.tl .t span{flex:1}
.pane{position:absolute;top:0;bottom:0}
.sym{display:flex}.sym svg{width:100%;height:100%;fill:currentColor}
.note{position:absolute;left:50%;transform:translateX(-50%);bottom:10px;font-size:10.5px;color:rgba(255,255,255,.4);letter-spacing:.06em;text-transform:uppercase;z-index:40;white-space:nowrap}
/* TV sidebar as a drawer (swipe right from the left edge) */
.drawer{position:absolute;left:0;top:0;bottom:0;width:300px;padding:64px 14px 18px;display:flex;flex-direction:column;gap:2px;border-radius:0 32px 32px 0;z-index:25}
.drawer a{display:flex;gap:12px;align-items:center;height:46px;padding:0 12px;border-radius:14px;font-size:16px;font-weight:500;color:rgba(243,242,239,.82)}
.drawer a.on{background:var(--foco);color:#fff}
.drawer a .ic{width:22px;height:22px}
.drawer .k{font-size:11px;font-weight:700;letter-spacing:.12em;color:var(--mut2);padding:14px 12px 6px;text-transform:uppercase}
.drawer .hd{display:flex;align-items:center;gap:10px;padding:0 12px 10px}
.drawer .hd b{font-size:20px;font-weight:700}
.grip{position:absolute;left:6px;top:50%;transform:translateY(-50%);width:4px;height:56px;border-radius:2px;background:rgba(255,255,255,.35);z-index:26}
.hand{position:absolute;width:44px;height:44px;border-radius:50%;border:2.5px solid rgba(255,255,255,.9);box-shadow:0 0 0 6px rgba(255,255,255,.18);z-index:40}
.hand::after{content:"";position:absolute;left:46px;top:50%;width:70px;height:2.5px;background:linear-gradient(90deg,rgba(255,255,255,.9),rgba(255,255,255,0));transform:translateY(-50%)}
/* bar gallery cards: the right 300 pt of a closed display */
.bc{position:relative;width:300px;height:520px;overflow:hidden;border-radius:0 40px 40px 0;background:var(--bg);box-shadow:0 0 0 2px #2b2b2f,0 24px 60px rgba(0,0,0,.55)}
.bc img.bg{position:absolute;left:-200px;top:0;width:500px;height:520px;object-fit:cover;object-position:60% 20%}
.bc .veil{position:absolute;inset:0;background:linear-gradient(90deg,rgba(13,13,13,.1),rgba(13,13,13,.55) 70%)}
.bc .vbar{padding-top:12px}
.lbl{display:flex;flex-direction:column;gap:6px;width:300px}
.lbl b{font-size:15px;font-weight:700}
.lbl span{font-size:12.5px;color:#9a9a9e;line-height:1.45}
.grid3{display:grid;grid-template-columns:repeat(3,300px);gap:44px 48px;padding:36px 44px;width:max-content}
.tabs.wide{width:196px;align-items:stretch;padding:10px}
.tabs.wide i.tb{width:auto;justify-content:flex-start;gap:12px;padding:0 12px;font-size:15px;font-weight:600;font-style:normal;color:rgba(255,255,255,.8)}
.tabs.wide i.tb.on{background:rgba(255,255,255,.14);color:#fff}
.tabs.wide i.tb.on .ic svg{fill:none;stroke:#fff}
.tabs.mini{width:60px;padding:6px 0;gap:0}
.tabs.mini .dd{display:flex;flex-direction:column;gap:5px;align-items:center;padding:6px 0 4px}
.tabs.mini .dd i{width:5px;height:5px;border-radius:50%;background:rgba(255,255,255,.4);display:block}
.tabs.mini .dd i.on{background:var(--ac)}
.tabs.tint{background:linear-gradient(180deg,rgba(226,120,60,.28),rgba(236,64,122,.22));box-shadow:inset 0 1px 0 rgba(255,255,255,.35),0 10px 28px rgba(0,0,0,.42)}
.tabs .thumb{position:absolute;left:6px;width:48px;height:48px;border-radius:24px;background:rgba(255,255,255,.18);transition:top .2s}
.np{position:relative;width:56px;height:56px;border-radius:50%;overflow:hidden;margin-top:12px}
.np img{width:100%;height:100%;object-fit:cover}
.np::after{content:"";position:absolute;inset:0;border-radius:50%;background:conic-gradient(var(--ac) 38%,rgba(255,255,255,.18) 0);-webkit-mask:radial-gradient(circle,transparent 72%,#000 74%);mask:radial-gradient(circle,transparent 72%,#000 74%)}
.edge{position:absolute;right:0;top:120px;bottom:120px;width:4px;border-radius:2px 0 0 2px;background:linear-gradient(180deg,rgba(236,64,122,0),var(--ac) 30%,var(--ac) 70%,rgba(236,64,122,0));box-shadow:-2px 0 14px rgba(236,64,122,.55)}
.isl{width:64px;border-radius:32px;padding:10px 0 12px;display:flex;flex-direction:column;align-items:center;gap:8px;background:var(--ilha);box-shadow:0 10px 28px rgba(0,0,0,.4)}
.isl .ic{width:22px;height:22px;color:#fff}
.isl b{font-size:12px;font-weight:700;color:var(--ac)}
.isl small{font-size:10px;color:var(--mut);writing-mode:vertical-rl;transform:rotate(180deg);letter-spacing:.04em}
/* mac */
.mac .tb{position:absolute;left:0;right:0;top:0;height:52px;display:flex;align-items:center;padding:0 16px;gap:8px;z-index:30}
.mac .tb i.tl{width:12px;height:12px;border-radius:50%;display:block}
.mac .side{position:absolute;left:0;top:0;bottom:0;width:224px;padding:64px 12px 16px;display:flex;flex-direction:column;gap:2px;background:rgba(20,21,24,.82);backdrop-filter:blur(30px);-webkit-backdrop-filter:blur(30px);border-right:1px solid rgba(255,255,255,.06);z-index:20}
.mac .side a{display:flex;gap:10px;align-items:center;height:36px;padding:0 10px;border-radius:9px;font-size:13.5px;font-weight:500;color:rgba(243,242,239,.78)}
.mac .side a.on{background:rgba(255,255,255,.12);color:#fff}
.mac .side a .ic{width:18px;height:18px}
.mac .side .k{font-size:11px;font-weight:700;letter-spacing:.1em;color:var(--mut2);padding:14px 10px 6px;text-transform:uppercase}
.mac .cont{position:absolute;left:224px;top:0;right:0;bottom:0;overflow:hidden}
"""


def status(cam=True):
    return (f'<div class="st"><span class="cam{"" if cam else " hidden"}"></span><span class="tm0">9:41</span>'
            f'<span class="ring"><em>83</em>{ic("wifi", "s")}</span></div>')


TABS = [('house', 'home'), ('search', 'search'), ('tv', 'guide'), ('library', 'library'), ('avatar', 'profile')]


def tabs(active, cls='tabs', badge=None):
    out = f'<div class="{cls} gl">'
    for icon, key in TABS:
        b = f'<b>{badge[1]}</b>' if badge and badge[0] == key else ''
        glyph = '<span class="avp"></span>' if icon == 'avatar' else ic(icon)
        out += f'<i class="tb{" on" if key == active else ""}">{glyph}{b}</i>'
    return out + '</div>'


def vbar(active='home', nav=(), tools=(), cam=True, show_tabs=True, over=False):
    """Vertical bar, top to bottom: status, primary navigation, prominent action,
    toolbar groups, (spacer), tab bar. Every item has icon+title (title omitted here)."""
    h = f'<aside class="vbar{" over" if over else ""}">{status(cam)}'
    if nav:
        h += '<div class="nav">' + ''.join(f'<span class="nb {c}">{ic(i)}</span>' for i, c in nav) + '</div>'
    if tools:
        h += '<div class="tools">' + ''.join(f'<span class="nb gl">{ic(i)}</span>' for i in tools) + '</div>'
    h += '<div class="vsp"></div>'
    if show_tabs:
        h += tabs(active)
    return h + '</aside>'


def hbars(title, active='home', left='chevron-left', right=('ellipsis',)):
    """Inner display in portrait keeps horizontal bars."""
    l = f'<span class="nb gl" style="width:44px;height:44px">{ic(left)}</span>' if left else '<span style="width:44px"></span>'
    r = ''.join(f'<span class="nb gl" style="width:44px;height:44px">{ic(i)}</span>' for i in right)
    return (f'<div class="hbar"><div class="sth"><span>9:41</span><span class="r">{ic("wifi", "s")}<span class="bat" style="width:22px;height:10px;border:1.5px solid rgba(255,255,255,.6);border-radius:3px;display:inline-block"></span></span></div>'
            f'<div class="nbar">{l}<span class="ttl">{title}</span>{r}</div></div>' + tabs(active, 'tabsh'))


def dev(kind, inner_html, cap, pose='flat', bar='', cls=''):
    """kind: outer | outer land | inner | inner port | mac. pose: flat | book | tabletop."""
    fold = ''
    if kind.startswith('inner'):
        fold = f'<div class="fold{" on" if pose in ("book", "tabletop") else ""}"></div>'
    return (f'<figure class="fig" style="margin:0"><div class="dev {kind} {cls}">{inner_html}{fold}{bar}</div>'
            f'<figcaption class="cap">{cap}</figcaption></figure>')


def page(title, note, figs):
    return (f'<!doctype html><html><head><meta charset="utf-8"><title>{title}</title><style>{font_css()}{CSS}</style></head><body>'
            f'<div class="head"><h1>Nuvio · iPhone Duo · {title}</h1><p>{note}</p></div><div class="page">{"".join(figs)}</div></body></html>')


# ---------- content helpers ----------
POSTERS = [ART / f'poster/{i:02d}.jpg' for i in range(20)]
EPS = [ART / f'ep/00_1_{i:02d}.jpg' for i in range(1, 6)]
CAST = [ART / f'elenco/00_{i}.jpg' for i in range(5)]
TITLES = ['Severance', 'Shōgun', 'The Bear', 'Dune: Part Two', 'Slow Horses', 'Fallout', 'The Martian', 'Andor', 'Oppenheimer', 'Poor Things', 'Dark', 'Succession', 'Chernobyl', 'Arrival', 'Pachinko', 'Silo', 'Blade Runner 2049', 'Heat', 'Her', 'Past Lives']
EPN = ['The End', 'The Target', 'The Head', 'The Ghouls', 'The Past']


def posters(idx, w, h, foc=None):
    return ''.join(f'<div class="poster{" foc" if i == foc else ""}" style="width:{w}px;height:{h}px"><img src="{f(POSTERS[i % 20])}"></div>' for i in idx)


def cards(idx, w, h, labels=True):
    out = ''
    for k, i in enumerate(idx):
        p = [62, 18, 84, 40, 55][k % 5]
        out += (f'<div class="card" style="width:{w}px;height:{h}px"><img src="{f(EPS[i % 5])}">'
                f'<div class="pl">{ic("play")}</div>' + (f'<div class="lb">{TITLES[i % 20]} · S{1 + i % 3}:E{2 + i % 7}</div>' if labels else '')
                + f'<div class="pr"><b style="width:{p}%"></b></div></div>')
    return out


def hero(h, bg, logo=None, title=None, left=False, wide=False, continue_ep='Continue S1:E3', extra=''):
    t = f'<img class="logo" src="{f(logo)}">' if logo else f'<h1 class="h1" style="font-size:32px">{title}</h1>'
    return (f'<div class="hero" style="height:{h}px"><img class="bg" src="{f(bg)}" style="object-position:{"62% 30%" if not wide else "50% 25%"}">'
            f'<div class="veu{" l" if left else ""}"></div><div class="txt">{t}'
            f'<div class="meta"><span class="kick" style="color:rgba(243,242,239,.7)">Series · 2024 · 2 seasons</span></div>'
            f'<div class="acts"><span class="pill">{ic("play")}{continue_ep}</span><span class="dsc">{ic("plus")}</span><span class="dsc">{ic("ellipsis")}</span></div>'
            f'<span class="qline"><i class="dot"></i>4K · Dolby Vision · Atmos · 8.3 GB</span>{extra}</div></div>')


def row(title, body, more='See all', extra=''):
    return f'<div class="sec"><div class="rowh"><h2 class="h2">{title}</h2>{extra}<em>{more} {ic("chevron-right", "s")}</em></div><div class="strip">{body}</div></div>'


# ---------- HOME ----------
SYN_FALLOUT = 'In a future, post-apocalyptic Los Angeles, citizens must live in underground bunkers to protect themselves from radiation, mutants and bandits.'


def ratings(big=False):
    return (f'<div class="meta{" big" if big else ""}" style="gap:14px"><span class="rate"><span class="lg imdb">IMDb</span>8.4</span><span class="rate"><span class="lg">TMDB</span>87</span>'
            '<span class="rate"><span class="lg">Trakt</span>92%</span><span class="rate"><span class="lg">RT</span>93%</span><span class="rate"><span class="lg">MC</span>74</span></div>')


def dots(n=8, on=5):
    return '<div class="dots">' + ''.join(f'<i class="{"on" if i == on else ""}"></i>' for i in range(n)) + '</div>'


def home_outer():
    # closed: art edge to edge, logo and meta centred, full-width Continue pill, the next row peeking
    scr = (f'<div class="scr full"><div class="hero full"><img class="bg" src="{f(FALLOUT)}" style="object-position:58% 20%">'
           '<div class="veu" style="background:linear-gradient(180deg,rgba(13,13,13,0) 38%,rgba(13,13,13,.72) 62%,var(--bg) 82%)"></div></div>'
           '<div style="position:absolute;left:20px;right:104px;top:296px;display:flex;flex-direction:column;align-items:center;text-align:center;gap:12px">'
           f'<img src="{f(FALLOUT_LOGO)}" style="height:84px;object-fit:contain">'
           '<div class="meta big" style="justify-content:center;font-size:16px"><span>Series</span><span class="pt"></span><span>Sci-Fi</span><span class="pt"></span><span>2024</span><span class="pt"></span><span>2 seasons</span></div>'
           '<div class="meta" style="justify-content:center;gap:12px"><span class="tag">RELEASED</span><span class="rate"><span class="lg imdb">IMDb</span>8.4</span><span class="qline"><i class="dot"></i>4K · DV · Atmos</span></div>'
           f'<span class="pill xl wide" style="margin-top:6px">{ic("play")}Continue S1:E3</span>'
           f'<div class="acts" style="gap:14px"><span class="dsc xl">{ic("plus")}</span><span class="dsc xl">{ic("layers")}</span><span class="dsc xl">{ic("clapperboard")}</span><span class="dsc xl">{ic("ellipsis")}</span></div></div>'
           f'<div class="peek" style="bottom:-70px"><div class="rowh"><h2 class="h2">Continue watching</h2></div><div class="strip">{cards([5, 1, 7], 150, 84)}</div></div></div>')
    bar = vbar('home', cam=True)
    return dev('outer', scr, '<b>Closed · outer display</b> · 466×678 pt · art edge to edge under the side region<br>logo, meta and actions centred; the next row peeks from the bottom', bar=bar)


def home_inner(pose='flat'):
    # open: one hero across the whole inner display (media may cross the fold); text, actions and dots bottom-left
    scr = (f'<div class="scr full"><div class="hero full"><img class="bg" src="{f(FALLOUT)}" style="object-position:50% 18%">'
           '<div class="veu" style="background:linear-gradient(90deg,rgba(13,13,13,.78) 0,rgba(13,13,13,.45) 34%,rgba(13,13,13,0) 60%),linear-gradient(180deg,rgba(13,13,13,0) 45%,rgba(13,13,13,.7) 78%,var(--bg) 100%)"></div></div>'
           '<div style="position:absolute;left:40px;bottom:78px;display:flex;flex-direction:column;gap:14px;max-width:640px">'
           f'<img src="{f(FALLOUT_LOGO)}" style="height:124px;width:auto;object-fit:contain;object-position:left">'
           '<div class="meta big"><span>Series</span><span class="pt"></span><span>Sci-Fi</span><span class="pt"></span><span>2 seasons</span><span class="pt"></span><span>2024</span></div>'
           '<div class="meta" style="gap:12px"><span class="tag">RELEASED</span><span class="rate"><span class="lg imdb">IMDb</span>8.4</span><span class="rate"><span class="lg">Trakt</span>92%</span><span class="qline"><i class="dot"></i>4K · Dolby Vision · Atmos · 8.3 GB</span></div>'
           f'<p class="sub big" style="margin:0">{SYN_FALLOUT}</p>'
           f'<div class="acts" style="gap:12px"><span class="pill xl">{ic("play")}Continue S1:E3</span><span class="dsc xl">{ic("plus")}</span><span class="dsc xl">{ic("layers")}</span><span class="dsc xl">{ic("clapperboard")}</span><span class="dsc xl">{ic("ellipsis")}</span></div>'
           + dots() + '</div>'
           f'<div class="peek" style="padding:0 40px;bottom:-64px"><div class="rowh"><h2 class="h2" style="font-size:24px">Continue watching</h2></div><div class="strip" style="gap:16px">{cards([5, 1, 7, 3, 4], 186, 105)}</div></div></div>')
    bar = vbar('home', cam=False)
    cap = ('<b>Open · inner display</b> · 951×669 pt · one hero across the whole display<br>'
           'Continue watching peeks from the bottom')
    return dev('inner', scr, cap, pose=pose, bar=bar)


# ---------- SEARCH ----------
def search_grid(cols, w, h, n, start=0):
    cells = ''.join(f'<div class="cell"><div class="poster" style="width:{w}px;height:{h}px"><img src="{f(POSTERS[(start + i) % 20])}"></div>'
                    f'<div class="ptitle">{TITLES[(start + i) % 20]}</div><div class="psub">{2015 + (i * 3) % 10} · {"Series" if i % 2 else "Movie"}</div></div>' for i in range(n))
    return f'<div class="gridp" style="grid-template-columns:repeat({cols},{w}px)">{cells}</div>'


def search_outer():
    scr = ('<div class="scr"><div style="padding:66px 20px 0"><h1 class="h1">Search</h1>'
           f'<div class="field" style="margin-top:14px">{ic("search")}<span>Titles, people, channels</span>{ic("mic")}</div>'
           '<div class="strip" style="margin-top:12px;gap:8px"><span class="chip on">All</span><span class="chip">Movies</span><span class="chip">Series</span><span class="chip">Live</span><span class="chip">People</span></div>'
           '<div class="kick" style="margin:18px 0 10px">Results · 24</div>'
           + search_grid(3, 107, 160, 6) + '</div></div>')
    bar = vbar('search', tools=['sliders-horizontal'])
    return dev('outer', scr, '<b>Closed</b> · search field and filters inset from the side region<br>3 columns of 107 pt on 382 pt of content', bar=bar)


def search_inner():
    sidebar = ('<div class="pane" style="left:0;width:322px;padding:66px 16px 0 20px;border-right:1px solid rgba(255,255,255,.07)">'
               f'<div class="field">{ic("search")}<span style="color:#fff">mars</span>{ic("x", "s")}</div>'
               '<div class="strip" style="margin-top:10px;gap:8px"><span class="chip on">All</span><span class="chip">Movies</span><span class="chip">Series</span></div>'
               '<div style="margin-top:14px;display:flex;flex-direction:column;gap:2px">')
    items = [('The Martian', '2015 · Movie · 2h 24m', 6), ('Mars', '2016 · Series · 2 seasons', 7), ('Mission to Mars', '2000 · Movie', 8), ('For All Mankind', '2019 · Series · 4 seasons', 9), ('Red Planet', '2000 · Movie', 10), ('Life', '2017 · Movie', 11)]
    for k, (t, s, i) in enumerate(items):
        sidebar += (f'<div class="lrow{" foc" if k == 0 else ""}" style="padding:8px 10px"><div class="poster" style="width:44px;height:66px"><img src="{f(POSTERS[i])}"></div>'
                    f'<div class="tx"><div class="t">{t}</div><div class="s">{s}</div></div>{ic("chevron-right", "s") if k == 0 else ""}</div>')
    sidebar += '</div></div>'
    detail = ('<div class="pane" style="left:322px;right:0">'
              + hero(360, MARTIAN, title='The Martian', left=True, continue_ep='Watch')
              .replace('Series · 2024 · 2 seasons', 'Movie · 2015 · 2h 24m · Sci-fi').replace('S1:E3', '')
              + '<div class="sec"><p class="sub" style="margin:0;max-width:440px">An astronaut becomes stranded on Mars after his team assumes him dead, and must rely on his ingenuity to find a way to signal to Earth that he is alive.</p>'
              '<div class="meta" style="margin-top:12px"><span class="badge" style="background:#F5C518;color:#111;padding:2px 5px;border-radius:4px">IMDb</span><span>8.0</span><span class="pt"></span><span>Trakt 85%</span><span class="pt"></span><span>Ridley Scott</span></div></div></div>')
    scr = f'<div class="scr">{sidebar}{detail}</div>'
    bar = vbar('search', nav=[('chevron-left', 'gl')], tools=['bookmark', 'share-2'], cam=False)
    cap = '<b>Open · flat</b> · NavigationSplitView: results list + detail preview side by side<br>only the detail column feeds the vertical bar; list controls stay above the list'
    return dev('inner', scr, cap, bar=bar)


# ---------- DETAIL ----------
def ep_rows(n=4, foc=2, thumb=112):
    out = ''
    for k in range(n):
        out += (f'<div class="lrow{" foc" if k == foc else ""}"><div class="th" style="width:{thumb}px;height:{int(thumb * 9 / 16)}px"><img src="{f(EPS[k % 5])}">'
                + (f'<div class="pr"><b style="width:{[100, 100, 46, 0, 0][k]}%"></b></div>' if k < 3 else '') + '</div>'
                f'<div class="tx"><div class="t">{k + 1} · {EPN[k]}</div><div class="s">{[61, 60, 54, 63, 58][k]} min · {"Watched" if k < 2 else ("Resume 27:40" if k == 2 else "Unwatched")}</div></div>'
                f'<div class="rt">{ic("circle-check", "s") if k < 2 else ""}</div></div>')
    return out


def stars(on=0):
    return '<div class="stars">' + ''.join(ic('star', 'on' if i < on else '') for i in range(5)) + '</div>'


def detail_actions(xl=True):
    c = 'dsc xl' if xl else 'dsc'
    return (f'<span class="{c}">{ic("layers")}</span><span class="{c}">{ic("clapperboard")}</span>'
            f'<span class="{c}">{ic("eye-off")}</span><span class="{c}">{ic("plus")}</span><span class="{c}">{ic("share-2")}</span>')


def detail_outer():
    # closed: art on top, logo and genres centred over it, full-width Continue pill, action circles, your rating
    scr = (f'<div class="scr full"><div style="position:absolute;left:0;right:0;top:0;height:380px"><img class="bg" src="{f(FALLOUT)}" style="position:absolute;inset:0;width:100%;height:100%;object-fit:cover;object-position:58% 20%">'
           '<div style="position:absolute;inset:0;background:linear-gradient(180deg,rgba(13,13,13,0) 45%,rgba(13,13,13,.75) 78%,var(--bg) 100%)"></div></div>'
           '<div style="position:absolute;left:20px;right:104px;top:248px;display:flex;flex-direction:column;align-items:center;text-align:center;gap:11px">'
           f'<img src="{f(FALLOUT_LOGO)}" style="height:80px;object-fit:contain">'
           '<div class="meta big" style="justify-content:center;font-size:16px;color:var(--mut)"><span>Sci-Fi</span><span class="pt"></span><span>Drama</span><span class="pt"></span><span>2 seasons</span><span class="pt"></span><span>TV-MA</span></div>'
           '<div class="meta" style="justify-content:center;gap:12px"><span class="rate"><span class="lg imdb">IMDb</span>8.4</span><span class="rate"><span class="lg">Trakt</span>92%</span><span class="rate"><span class="lg">RT</span>93%</span></div>'
           f'<span class="pill xl wide" style="margin-top:4px">{ic("play")}Continue S1:E3</span>'
           f'<div class="acts" style="gap:12px">{detail_actions()}</div>'
           + stars() + '</div>'
           f'<div class="peek" style="bottom:-58px"><div class="rowh"><h2 class="h2">Episodes</h2><span>Season 1 · 8</span></div><div class="strip">{cards([0, 1, 2], 150, 84, labels=False)}</div></div></div>')
    bar = vbar('home', nav=[('chevron-left', 'gl')], cam=True, show_tabs=False)
    return dev('outer', scr, '<b>Closed</b> · art on top, logo and genres centred, full-width Continue pill<br>Sources · Trailer · Mark watched · Library · Share as circles; your rating in stars; episodes peek below', bar=bar)


def detail_inner(pose='flat'):
    # open: the reference layout at full width, with Nuvio's information and buttons
    scr = (f'<div class="scr full"><div class="hero full"><img class="bg" src="{f(FALLOUT)}" style="object-position:72% 20%">'
           '<div class="veu" style="background:linear-gradient(90deg,rgba(13,13,13,.94) 0,rgba(13,13,13,.86) 30%,rgba(13,13,13,.45) 55%,rgba(13,13,13,.1) 78%),linear-gradient(180deg,rgba(13,13,13,0) 60%,rgba(13,13,13,.55) 100%)"></div></div>'
           '<div style="position:absolute;left:40px;top:70px;right:280px;display:flex;flex-direction:column;gap:14px">'
           f'<img src="{f(FALLOUT_LOGO)}" style="height:150px;width:auto;object-fit:contain;object-position:left">'
           '<div class="meta big" style="font-size:20px;gap:18px"><span>2 seasons</span><span>1h</span><span class="tag" style="font-size:12px">TV-MA</span></div>'
           + ratings(big=True) +
           '<div class="meta" style="font-size:16px;color:var(--mut)"><span>Sci-Fi</span><span class="pt"></span><span>Drama</span><span class="pt"></span><span>Adventure</span></div>'
           '<div class="meta" style="font-size:16px"><span style="color:var(--mut)">Created by</span><span>Geneva Robertson-Dworet, Graham Wagner</span></div>'
           f'<p class="sub big" style="margin:4px 0 0;max-width:760px;font-size:17px;color:rgba(243,242,239,.85)">{SYN_FALLOUT} Lucy, a vault dweller, leaves the safety of her shelter to find her kidnapped father; above ground she meets Maximus, a Brotherhood of Steel squire, and a ghoul bounty hunter who has survived the wasteland for two hundred years.</p>'
           f'<div class="acts" style="gap:22px;margin-top:10px"><span class="pill xl" style="padding:0 40px">{ic("play")}Continue S1:E3</span>{stars()}<span style="flex:1"></span>{detail_actions()}</div></div>'
           '<div class="peek" style="padding:0 40px;bottom:10px"><div class="rowh"><h2 class="h2" style="font-size:24px">Episodes</h2><span>Season 1 · 8</span></div></div></div>')
    bar = vbar('home', nav=[('chevron-left', 'gl')], cam=False, show_tabs=False)
    cap = ('<b>Open · flat</b> · the whole display: big logo, ratings from every service, creators, full synopsis<br>'
           'Continue pill, your rating and the action circles on one line; Episodes follow below')
    return dev('inner', scr, cap, pose=pose, bar=bar)


# ---------- SOURCES ----------
def sources_body(w, compact=True):
    hdr = (f'<div style="padding:18px 20px 0 20px"><div class="kick">Fallout · S1:E3 · The Head</div>'
           '<div style="display:flex;align-items:center;gap:8px;margin-top:4px"><h1 class="h1" style="font-size:26px;flex:1">Sources</h1>'
           + ('' if compact else f'<span class="dsc">{ic("refresh-cw")}</span><span class="dsc">{ic("x")}</span>') + '</div>'
           '<div class="strip" style="margin-top:12px;gap:8px"><span class="chip on">MP4 only</span><span class="chip">Cached</span><span class="chip">Audio: EN</span></div>'
           f'<div class="seg" style="margin-top:10px"><span>Where to watch <em>2</em></span><span class="on">All <em>14</em></span><span>Torrentio <em>9</em></span><span>AIO <em>5</em></span></div></div>')
    rows = ('<div style="padding:8px 8px 0">'
            '<div class="src best"><div class="kick ac" style="margin-bottom:4px">Best for this iPhone</div><div class="n">Fallout S01E03 2160p DV HDR10 <span><i></i> AIOStreams</span></div>'
            '<div class="b"><span class="badge">4K</span><span class="badge">DV</span><span class="badge">HEVC</span><span class="badge">Atmos</span><span>8.3 GB</span></div><div class="d">Cached on RD · Prime WEB-DL · 1 min ago</div></div>'
            '<div class="grp"><b>4K</b><span>ULTRA HD · HDR</span><em>3 sources</em></div>'
            '<div class="src"><div class="n">Fallout S01E03 2160p HDR10+ <span><i></i> Torrentio</span></div><div class="b"><span class="badge">4K</span><span class="badge">HDR10+</span><span class="badge">HEVC</span><span>6.1 GB</span></div></div>'
            '<div class="src"><div class="n">Fallout S01E03 2160p DV <span><i style="background:#F2B134"></i> Comet</span></div><div class="b"><span class="badge">4K</span><span class="badge">DV</span><span class="badge">EAC3</span><span>5.4 GB</span></div></div>'
            '<div class="grp"><b>1080p</b><span>FULL HD</span><em>7 sources</em></div>'
            '<div class="src"><div class="n">Fallout S01E03 1080p WEB-DL <span><i></i> AIOStreams</span></div><div class="b"><span class="badge">1080p</span><span class="badge">x264</span><span class="badge">5.1</span><span>2.6 GB</span></div></div>'
            '<div class="src"><div class="n">Fallout S01E03 1080p 10bit <span><i></i> Torrentio</span></div><div class="b"><span class="badge">1080p</span><span class="badge">HEVC</span><span>1.9 GB</span></div></div></div>')
    return hdr + rows


def sources_outer():
    behind = '<div class="scr">' + hero(330, FALLOUT, FALLOUT_LOGO) + '<div class="dim"></div></div>'
    sheet = f'<div class="ilha" style="position:absolute;left:0;right:84px;top:92px;bottom:0;border-radius:28px 28px 0 0">{sources_body(382)}</div>'
    bar = vbar('home', nav=[('x', 'gl'), ('refresh-cw', 'gl')], tools=[], show_tabs=False)
    return dev('outer', behind + sheet, '<b>Closed</b> · the Sources sheet keeps the vertical bar: Close on top, Refresh below<br>island material; "Best for this iPhone" in the accent, HDR groups as on the TV', bar=bar)


def sources_inner():
    behind = ('<div class="scr">' + hero(330, FALLOUT, FALLOUT_LOGO, left=True)
              + '<div class="sec"><p class="sub" style="margin:0;max-width:400px">In a future, post-apocalyptic Los Angeles, citizens must live in underground bunkers to protect themselves from radiation, mutants and bandits.</p></div><div class="dim" style="background:rgba(0,0,0,.3)"></div></div>')
    sheet = f'<div class="ilha" style="position:absolute;left:493px;width:370px;top:16px;bottom:0;border-radius:28px 28px 0 0">{sources_body(370, compact=False)}</div>'
    bar = vbar('home', nav=[('chevron-left', 'gl')], tools=['bookmark', 'share-2'], cam=False)
    cap = ('<b>Open</b> · the Sources sheet slides in from the trailing edge over the detail<br>'
           'Close / Refresh inside the sheet header; the detail stays readable behind it')
    return dev('inner', behind + sheet, cap, bar=bar)


# ---------- PLAYER ----------
def controls(w, compact=True, scrub_top=None):
    return ('<div style="position:absolute;left:20px;right:20px;bottom:22px;z-index:5">'
            '<div class="scrub"><b style="width:38%"></b><i style="left:38%"></i></div>'
            '<div class="tm"><span>22:14</span><span>-35:46</span></div></div>')


def player_outer_land():
    v = (f'<div class="scr full"><img class="vfill" src="{f(FALLOUT)}" style="object-position:60% 40%"><div class="dim" style="background:linear-gradient(180deg,rgba(0,0,0,.55),rgba(0,0,0,0) 30%,rgba(0,0,0,0) 60%,rgba(0,0,0,.6))"></div>'
         '<div style="position:absolute;left:20px;top:16px;z-index:5"><div class="kick" style="color:rgba(255,255,255,.7)">Fallout · S1:E3</div><div class="h3" style="font-size:17px">The Head</div></div>'
         f'<div style="position:absolute;left:0;right:84px;top:50%;transform:translateY(-50%);display:flex;justify-content:center;gap:34px;align-items:center;z-index:5"><span class="pc gl">{ic("rotate-ccw")}</span><span class="pc big gl">{ic("pause")}</span><span class="pc gl">{ic("rotate-cw")}</span></div>'
         + controls(594).replace('right:20px', 'right:104px') + '</div>')
    bar = vbar('home', nav=[('chevron-left', 'gl')], tools=['captions', 'audio-lines', 'list-video', 'picture-in-picture'], show_tabs=False, over=True)
    return dev('outer land', v, '<b>Closed · landscape</b> · 678×466 pt · video full-bleed under the bar (immersive)<br>Back, then Subtitles / Audio / Episodes / PiP as symbols in the side region; tab bar hidden while playing', bar=bar)


def player_inner_book():
    left = (f'<div class="pane" style="left:0;width:520px;background:#000"><img src="{f(FALLOUT)}" style="position:absolute;left:0;top:50%;transform:translateY(-50%);width:520px;height:292px;object-fit:cover">'
            '<div style="position:absolute;left:0;right:0;top:50%;transform:translateY(-50%);height:292px">'
            f'<div style="position:absolute;left:0;right:0;top:50%;transform:translateY(-50%);display:flex;justify-content:center;gap:30px;align-items:center"><span class="pc gl">{ic("rotate-ccw")}</span><span class="pc big gl">{ic("pause")}</span><span class="pc gl">{ic("rotate-cw")}</span></div>'
            + controls(520).replace('bottom:22px', 'bottom:14px') + '</div>'
            '<div style="position:absolute;left:20px;top:60px"><div class="kick" style="color:rgba(255,255,255,.7)">Fallout · S1:E3</div><div class="h3" style="font-size:17px">The Head</div></div></div>')
    chat = ''.join(f'<div style="display:flex;gap:10px;align-items:flex-start"><span class="av" style="width:28px;height:28px;font-size:12px;background:{c}">{n[0]}</span><div><div style="font-size:12.5px;font-weight:600">{n} <span style="color:var(--mut2);font-weight:400">· {t}</span></div><div class="sub" style="font-size:13px">{m}</div></div></div>'
                   for n, c, t, m in [('marina', '#5B6CF2', '22:10', 'the ghoul scene is coming'), ('pedro', '#2EA67B', '22:12', 'no spoilers!!'), ('ju', '#E07A3F', '22:13', 'subtitles PT-BR on the right pane, nice')])
    right = ('<div class="pane" style="left:520px;right:0;padding:66px 96px 0 10px;display:flex;flex-direction:column">'
             '<div class="seg" style="align-self:flex-start;margin-left:12px"><span class="on">Up next</span><span>Watch Together <em>3</em></span><span>Details</span></div>'
             '<div style="display:flex;flex-direction:column;gap:2px;margin-top:12px">' + ep_rows(3, 1, 104).replace('3 · The Head', '5 · The Past').replace('2 · The Target', '4 · The Ghouls').replace('1 · The End', '3 · The Head') + '</div>'
             '<div class="kick" style="margin:16px 12px 8px">Watch Together · room 7K3F</div><div style="display:flex;flex-direction:column;gap:10px;padding:0 12px">' + chat + '</div>'
             f'<div class="field" style="margin:12px 12px 0;height:40px">{ic("message-circle")}<span>Say something</span></div></div>')
    scr = f'<div class="scr">{left}{right}</div>'
    bar = vbar('home', nav=[('chevron-left', 'gl')], tools=['captions', 'audio-lines', 'picture-in-picture'], cam=False, show_tabs=False, over=True)
    cap = ('<b>Open</b> · video | Up next + Watch Together, one continuous screen<br>'
           'the player keeps its aspect on the left; list, chat and composer on the right')
    return dev('inner', scr, cap, bar=bar)


def player_tabletop():
    top = (f'<div style="position:absolute;left:0;right:0;top:0;height:475px;background:#000"><img src="{f(FALLOUT)}" style="position:absolute;left:0;top:50%;transform:translateY(-50%);width:669px;height:376px;object-fit:cover">'
           '<div style="position:absolute;left:20px;top:96px"><div class="kick" style="color:rgba(255,255,255,.7)">Fallout · S1:E3</div><div class="h3" style="font-size:17px">The Head</div></div></div>')
    bottom = ('<div style="position:absolute;left:0;right:0;top:475px;bottom:0;padding:34px 24px 0">'
              '<div class="scrub"><b style="width:38%"></b><i style="left:38%"></i></div><div class="tm"><span>22:14</span><span>-35:46</span></div>'
              f'<div style="display:flex;justify-content:center;gap:28px;align-items:center;margin-top:22px"><span class="dsc">{ic("captions")}</span><span class="pc gl">{ic("rotate-ccw")}</span><span class="pc big gl">{ic("pause")}</span><span class="pc gl">{ic("rotate-cw")}</span><span class="dsc">{ic("audio-lines")}</span></div>'
              '<div class="rowh" style="margin-top:28px"><h2 class="h2">Up next</h2><em>Season 1 ' + ic('chevron-down', 's') + '</em></div>'
              f'<div class="strip">{cards([3, 4, 0, 1], 148, 83)}</div></div>')
    scr = f'<div class="scr full">{top}{bottom}</div>'
    hb = ('<div class="hbar"><div class="sth"><span>9:41</span><span class="r">' + ic('wifi', 's') + '</span></div>'
          f'<div class="nbar"><span class="nb gl" style="width:44px;height:44px">{ic("chevron-down")}</span><span class="ttl"></span><span class="nb gl" style="width:44px;height:44px">{ic("picture-in-picture")}</span><span class="nb gl" style="width:44px;height:44px">{ic("ellipsis")}</span></div></div>')
    cap = ('<b>Open · tabletop pose</b> · 669×951 pt, folded like a laptop<br>media on the raised half, every control on the flat half; ArrangementView(.overlay) separates them as the hinge bends')
    return dev('inner port', scr, cap, pose='tabletop', bar=hb)


def player_inner_flat():
    v = (f'<div class="scr full"><img class="vfill" src="{f(FALLOUT)}" style="object-position:55% 35%"><div class="dim" style="background:linear-gradient(180deg,rgba(0,0,0,.5),rgba(0,0,0,0) 28%,rgba(0,0,0,0) 62%,rgba(0,0,0,.62))"></div>'
         '<div style="position:absolute;left:20px;top:16px;z-index:5"><div class="kick" style="color:rgba(255,255,255,.7)">Fallout · S1:E3</div><div class="h3" style="font-size:17px">The Head</div></div>'
         f'<div style="position:absolute;left:0;right:84px;top:50%;transform:translateY(-50%);display:flex;justify-content:center;gap:40px;align-items:center;z-index:5"><span class="pc gl">{ic("rotate-ccw")}</span><span class="pc big gl">{ic("pause")}</span><span class="pc gl">{ic("rotate-cw")}</span></div>'
         + controls(867).replace('right:20px', 'right:104px') + '</div>')
    bar = vbar('home', nav=[('chevron-left', 'gl')], tools=['captions', 'audio-lines', 'list-video', 'picture-in-picture'], cam=False, show_tabs=False, over=True)
    cap = '<b>Open · flat</b> · the division region is inactive, so the video fills the whole inner display<br>controls return to the overlay (same ArrangementView, .overlay state)'
    return dev('inner', v, cap, pose='flat', bar=bar)


# ---------- GUIDE ----------
CH = [('GLOBO', 'Jornal Nacional', 'Novela das 9', 72), ('ESPN', 'Flamengo × Palmeiras', 'SportsCenter', 44), ('HBO', 'The Last of Us', 'Succession', 20), ('CNN', 'Newsroom', 'Amanpour', 58), ('TNT', 'Dune', 'Inception', 10), ('BAND', 'Jogo Aberto', 'Brasil Urgente', 85), ('DISC', 'Gold Rush', 'Deadliest Catch', 33), ('SPORTV', 'Premiere', 'Boletim', 50)]


def guide_list(w, n=7):
    out = '<div class="guide"><div class="tl"><span></span><div class="t"><span>Now</span><span>21:30</span><span>22:00</span></div></div>'
    for k, (c, a, b, p) in enumerate(CH[:n]):
        out += (f'<div class="ch"><div class="lg">{c}</div><div class="pg"><div class="now" style="flex:1.4;--p:{p}%">{a}<small>21:00 · {p}% in</small></div><div style="flex:1">{b}<small>21:30</small></div></div></div>')
    return out + '</div>'


def guide_outer():
    scr = ('<div class="scr"><div style="padding:66px 20px 0"><h1 class="h1">TV Guide</h1>'
           '<div class="seg" style="margin-top:12px"><span class="on">' + ic('layout-list', 's') + 'Grid</span><span>' + ic('layout-grid', 's') + 'Cards</span><span>' + ic('activity', 's') + 'Sports</span><span>' + ic('film', 's') + 'VOD</span></div>'
           '<div style="margin-top:14px">' + guide_list(342, 8) + '</div></div></div>')
    bar = vbar('guide', tools=['search', 'star', 'sliders-horizontal'])
    return dev('outer', scr, '<b>Closed</b> · the grid as a channel list: now + next, progress in the accent<br>Search / Favourites / Options as symbols; segmented control stays in content (too wide for the bar)', bar=bar)


def guide_inner():
    left = ('<div class="pane" style="left:0;width:462px;padding:66px 12px 0 20px"><div style="display:flex;align-items:center;gap:12px"><h1 class="h1" style="font-size:24px">TV Guide</h1>'
            '<span class="seg"><span class="on">Grid</span><span>Cards</span><span>Sports</span><span>VOD</span></span></div>'
            '<div style="margin-top:12px">' + guide_list(430, 8) + '</div></div>')
    right = (f'<div class="pane" style="left:489px;right:0;padding:66px 16px 0 0"><div class="ilha" style="border-radius:22px;overflow:hidden">'
             f'<div style="position:relative;height:212px"><img src="{f(ART / "esportes/football-high.jpg")}" style="width:100%;height:100%;object-fit:cover;display:block"><span class="chip" style="position:absolute;left:12px;top:12px;background:rgba(236,64,122,.9);color:#fff">LIVE</span>'
             f'<span class="pc gl" style="position:absolute;left:50%;top:50%;transform:translate(-50%,-50%)">{ic("play")}</span></div>'
             '<div style="padding:14px 16px 16px"><div class="kick">ESPN · Brasileirão</div><h2 class="h2" style="margin-top:3px">Flamengo × Palmeiras</h2><div class="sub" style="margin-top:4px">21:00–23:00 · 2nd half · 1–1</div>'
             f'<div class="acts" style="margin-top:12px"><span class="pill">{ic("play")}Watch</span><span class="dsc">{ic("star")}</span><span class="dsc">{ic("picture-in-picture")}</span></div></div></div>'
             '<div class="kick" style="margin:16px 0 8px">Up next on ESPN</div><div style="display:flex;flex-direction:column;gap:2px">'
             '<div class="lrow" style="padding:8px 10px"><div class="tx"><div class="t">SportsCenter</div><div class="s">23:00 · 60 min</div></div><span class="chip">' + ic('bell', 's') + 'Remind</span></div>'
             '<div class="lrow" style="padding:8px 10px"><div class="tx"><div class="t">Flamengo × Palmeiras (replay)</div><div class="s">00:00 · 120 min</div></div></div></div></div>')
    scr = f'<div class="scr">{left}{right}</div>'
    bar = vbar('guide', tools=['search', 'star', 'sliders-horizontal'], cam=False)
    cap = ('<b>Open · flat</b> · ArrangementView(.split.axes(.horizontal)): grid | channel preview<br>list-detail with the preview and its actions in the right pane; falls back to the list alone in portrait')
    return dev('inner', scr, cap, pose='flat', bar=bar)


# ---------- LIBRARY ----------
def library_outer():
    scr = ('<div class="scr"><div style="padding:66px 20px 0"><div style="display:flex;align-items:baseline;gap:10px"><h1 class="h1">Library</h1><span class="sub">142 titles</span></div>'
           '<div class="seg" style="margin-top:12px"><span class="on">All</span><span>Movies</span><span>Series</span><span>Lists <em>6</em></span></div>'
           '<div style="margin-top:14px">' + search_grid(3, 107, 160, 9, 2) + '</div></div></div>')
    bar = vbar('library', tools=['arrow-up-down', 'funnel'])
    return dev('outer', scr, '<b>Closed</b> · 3 columns; Sort and Filter as symbols in the bar<br>segmented filter stays in content', bar=bar)


def library_inner():
    scr = ('<div class="scr"><div style="padding:66px 20px 0"><div style="display:flex;align-items:center;gap:12px"><h1 class="h1" style="font-size:24px">Library</h1><span class="sub">142 titles</span>'
           '<span class="seg" style="margin-left:8px"><span class="on">All</span><span>Movies</span><span>Series</span><span>Lists <em>6</em></span></span></div>'
           f'<div style="margin-top:14px">{search_grid(9, 86, 129, 18, 2).replace("gap:10px", "gap:14px 16px")}</div></div></div>')
    bar = vbar('library', tools=['arrow-up-down', 'funnel'], cam=False)
    cap = '<b>Open</b> · one continuous grid across the whole display, 9 columns<br>the grid scrolls on; Sort and Filter stay in the bar'
    return dev('inner', scr, cap, bar=bar)


# ---------- ONBOARDING / PROFILES ----------
def qr_block(size=190):
    return (f'<div style="width:{size}px;height:{size}px;border-radius:18px;background:#fff;padding:10px;display:flex"><img src="{f(QR)}" style="width:100%;height:100%;object-fit:contain"></div>')


def onboarding_outer():
    sym = SYMBOL.read_text()
    scr = (f'<div class="scr"><img class="vfill" src="{f(PRADA)}" style="filter:blur(18px) brightness(.5);transform:scale(1.15)"><div class="dim" style="background:linear-gradient(180deg,rgba(13,13,13,.15),rgba(13,13,13,.94) 55%)"></div>'
           '<div style="position:absolute;left:20px;right:20px;bottom:24px;display:flex;flex-direction:column;align-items:center;text-align:center;gap:12px">'
           f'<div class="sym" style="width:56px;height:56px;color:#fff">{sym}</div><h1 class="h1" style="font-size:30px">Sign in to Nuvio</h1>'
           '<p class="sub" style="margin:0;max-width:300px">Scan with your phone, or open <b style="color:#fff;font-weight:600">nuvio.space/tv</b> and type the code.</p>'
           + qr_block(170) + '<div class="h1" style="letter-spacing:.26em;font-size:30px">7K3F·2Q</div>'
           f'<span class="pill w" style="margin-top:4px">{ic("log-in")}I have an account</span><span class="sub" style="font-size:12.5px">Add-ons, collections and Home rows come from your account. Nothing to set up here.</span></div></div>')
    bar = vbar('home', show_tabs=False, tools=['languages', 'life-buoy'])
    return dev('outer', scr, '<b>Closed</b> · QR + code login, as on the TV; Language and Help in the bar<br>no tab bar before sign-in', bar=bar)


def onboarding_inner():
    sym = SYMBOL.read_text()
    left = (f'<div class="pane" style="left:0;width:462px"><img class="vfill" src="{f(PRADA)}" style="filter:blur(18px) brightness(.5);transform:scale(1.15)"><div class="dim" style="background:linear-gradient(180deg,rgba(13,13,13,.15),rgba(13,13,13,.94) 55%)"></div>'
            '<div style="position:absolute;left:20px;right:20px;bottom:30px;display:flex;flex-direction:column;align-items:center;text-align:center;gap:12px">'
            f'<div class="sym" style="width:56px;height:56px;color:#fff">{sym}</div><h1 class="h1" style="font-size:30px">Sign in to Nuvio</h1>'
            + qr_block(180) + '<div class="h1" style="letter-spacing:.26em;font-size:30px">7K3F·2Q</div>'
            f'<span class="pill w">{ic("log-in")}I have an account</span></div></div>')
    steps = [('smartphone', 'Set it up on your phone', 'AIOManager, Xperience or the Configurator write to your Nuvio account.'),
             ('cloud', 'The account is the source of truth', 'Add-ons, collections, Home rows and badges sync every ~5 minutes.'),
             ('users', 'Pick a profile', 'Each profile keeps its own add-ons, progress and lists.'),
             ('tv', 'Same account on every screen', 'LG, Samsung, Android TV, this iPhone and your Mac.')]
    right = ('<div class="pane" style="left:462px;right:0;padding:90px 20px 0 37px;background:var(--bg)"><div class="kick">How it works</div><h2 class="h2" style="margin-top:4px;font-size:24px">One account, every screen</h2><div style="display:flex;flex-direction:column;gap:18px;margin-top:22px">'
             + ''.join(f'<div style="display:flex;gap:14px;align-items:flex-start"><span class="dsc on" style="flex:none">{ic(i)}</span><div><div class="h3">{t}</div><div class="sub" style="margin-top:2px">{s}</div></div></div>' for i, t, s in steps)
             + f'</div><span class="pill g" style="margin-top:26px">Continue without an account {ic("chevron-right")}</span></div>')
    scr = f'<div class="scr">{left}{right}</div>'
    bar = vbar('home', show_tabs=False, tools=['languages', 'life-buoy'], cam=False)
    cap = ('<b>Open</b> · the same step expands into two panes: sign-in left, "how it works" right<br>opening the device never restarts the flow; closing it collapses back to the single column')
    return dev('inner', scr, cap, bar=bar)


PROFILES = [('Henrique', '#5B6CF2', 'H'), ('Marcela', '#E07A3F', 'M'), ('Kids', '#2EA67B', 'K'), ('Guests', '#8E5BF2', 'G')]


def prof(size, gap, cols, foc=0, font=34):
    cells = ''
    for k, (n, c, l) in enumerate(PROFILES):
        cells += (f'<div style="display:flex;flex-direction:column;align-items:center;gap:10px"><span class="av" style="width:{size}px;height:{size}px;font-size:{font}px;background:{c};{"box-shadow:0 0 0 4px #0D0D0D,0 0 0 7px var(--ac)" if k == foc else ""}">{l}</span><span style="font-size:14px;font-weight:600">{n}</span></div>')
    cells += (f'<div style="display:flex;flex-direction:column;align-items:center;gap:10px"><span class="av" style="width:{size}px;height:{size}px;background:rgba(255,255,255,.08);color:rgba(255,255,255,.7)">{ic("plus", "g")}</span><span style="font-size:14px;color:var(--mut)">Add</span></div>')
    return f'<div style="display:grid;grid-template-columns:repeat({cols},{size}px);gap:{gap}px;justify-content:center">{cells}</div>'


def profiles_outer():
    scr = ('<div class="scr"><div style="padding:120px 20px 0;display:flex;flex-direction:column;align-items:center;text-align:center"><div class="kick">Nuvio</div><h1 class="h1" style="margin-top:6px">Who is watching?</h1>'
           '<div style="margin-top:38px">' + prof(104, 26, 2) + '</div>'
           f'<span class="pill g" style="margin-top:38px">{ic("pencil")}Manage profiles</span></div></div>')
    bar = vbar('home', show_tabs=False, tools=['lock'])
    return dev('outer', scr, '<b>Closed</b> · 2 columns of avatars; the focused one carries the accent ring<br>Lock (PIN) as the only toolbar symbol', bar=bar)


def profiles_inner():
    scr = ('<div class="scr"><div style="padding:150px 20px 0;display:flex;flex-direction:column;align-items:center;text-align:center"><div class="kick">Nuvio</div><h1 class="h1" style="margin-top:6px;font-size:34px">Who is watching?</h1>'
           '<div style="margin-top:46px;width:100%">' + prof(118, 44, 5, font=38).replace('justify-content:center', 'justify-content:center;grid-template-columns:118px 118px 118px 118px 118px;column-gap:44px') + '</div>'
           f'<span class="pill g" style="margin-top:46px">{ic("pencil")}Manage profiles</span></div></div>')
    bar = vbar('home', show_tabs=False, tools=['lock'], cam=False)
    cap = '<b>Open</b> · one row of five across the display'
    return dev('inner', scr, cap, pose='flat', bar=bar)


# ---------- SETTINGS ----------
SETTINGS = [('user-round', 'Account', 'henrique'), ('smartphone', 'Interface mode', 'Auto'), ('layers', 'Sources and add-ons', '14'), ('play', 'Playback', ''), ('captions', 'Subtitles', 'EN, PT-BR'),
            ('audio-lines', 'Audio', 'Atmos'), ('palette', 'Appearance', 'Glass UI'), ('languages', 'Language', 'English'), ('bell', 'Notifications', ''), ('info', 'About', '2.1.0')]


def settings_list(on=None):
    out = '<div class="ilha ilist" style="border-radius:18px">'
    for k, (i, l, v) in enumerate(SETTINGS):
        out += f'<div class="setrow{" on" if k == on else ""}">{ic(i)}<span class="l">{l}</span><span class="v">{v}</span><span class="ch">{ic("chevron-right")}</span></div>'
    return out + '</div>'


def subtitles_page():
    return ('<div class="kick">Subtitles</div><h2 class="h2" style="margin-top:4px;font-size:24px">Subtitles</h2>'
            '<div class="ilha ilist" style="border-radius:18px;margin-top:16px">'
            f'<div class="setrow"><span class="l">Show subtitles</span><span class="sw on"></span></div>'
            f'<div class="setrow"><span class="l">Preferred languages</span><span class="v">English, Português</span><span class="ch">{ic("chevron-right")}</span></div>'
            f'<div class="setrow"><span class="l">Size</span><span class="seg"><span>S</span><span class="on">M</span><span>L</span><span>XL</span></span></div>'
            f'<div class="setrow"><span class="l">Font</span><span class="v">Inter Display</span><span class="ch">{ic("chevron-right")}</span></div>'
            f'<div class="setrow"><span class="l">Background</span><span class="seg"><span>None</span><span class="on">Soft</span><span>Box</span></span></div>'
            f'<div class="setrow"><span class="l">ASS styling</span><span class="sw on"></span></div>'
            '</div><div class="kick" style="margin:18px 0 8px">Preview</div>'
            f'<div style="position:relative;height:120px;border-radius:14px;overflow:hidden"><img src="{f(EPS[1])}" style="width:100%;height:100%;object-fit:cover"><div style="position:absolute;left:0;right:0;bottom:12px;text-align:center;font-size:15px;font-weight:600;text-shadow:0 1px 3px #000">Okey-dokey.</div></div>')


def settings_outer():
    scr = ('<div class="scr"><div style="padding:66px 20px 0"><h1 class="h1">Settings</h1><div style="margin-top:14px">' + settings_list() + '</div></div></div>')
    bar = vbar('settings', tools=['search'])
    return dev('outer', scr, '<b>Closed</b> · one list; each row pushes a page (NavigationStack)<br>island material for the group, chevrons in the support tint', bar=bar)


def settings_inner():
    left = ('<div class="pane" style="left:0;width:340px;padding:66px 12px 0 20px;border-right:1px solid rgba(255,255,255,.07)"><h1 class="h1" style="font-size:24px">Settings</h1><div style="margin-top:14px">' + settings_list(4) + '</div></div>')
    right = '<div class="pane" style="left:340px;right:0;padding:66px 20px 0 24px">' + subtitles_page() + '</div>'
    scr = f'<div class="scr">{left}{right}</div>'
    bar = vbar('settings', tools=['search'], cam=False)
    cap = '<b>Open</b> · NavigationSplitView: list | page; the same rows, one more level visible'
    return dev('inner', scr, cap, pose='flat', bar=bar)


# ---------- MAC ----------
def mac_home():
    side = ('<div class="side"><div class="k">Nuvio</div>'
            + ''.join(f'<a class="{"on" if k == 0 else ""}">{ic(i)}{l}</a>' for k, (i, l) in enumerate([('house', 'Home'), ('search', 'Search'), ('tv', 'TV Guide'), ('library', 'Library'), ('list', 'Lists'), ('trophy', 'Achievements')]))
            + '<div class="k">Streaming</div>' + ''.join(f'<a>{ic(i)}{l}</a>' for i, l in [('layers', 'Sources and add-ons'), ('activity', 'Sports'), ('users', 'Watch Together')])
            + '<div style="flex:1"></div>' + f'<a>{ic("settings")}Settings</a><a><span class="av" style="width:20px;height:20px;font-size:10px;background:#5B6CF2">H</span>Henrique</a></div>')
    tb = ('<div class="tb"><i class="tl" style="background:#FF5F57"></i><i class="tl" style="background:#FEBC2E"></i><i class="tl" style="background:#28C840"></i>'
          f'<span style="flex:1"></span><span class="field gl" style="width:260px;height:30px;font-size:13px;background:rgba(255,255,255,.08)">{ic("search", "s")}<span>Search</span></span></div>')
    g = lambda idx: f'<div class="strip">{posters(idx, 120, 180)}</div>'
    cont = ('<div class="cont">' + hero(470, FALLOUT, FALLOUT_LOGO, left=True, wide=True).replace('left:20px;bottom:18px', 'left:40px;bottom:30px')
            + '<div class="sec" style="padding:16px 40px 0"><div class="rowh"><h2 class="h2">Continue watching</h2><em>See all ' + ic('chevron-right', 's') + '</em></div><div class="strip">' + cards([5, 1, 7, 3, 4], 200, 112) + '</div></div>'
            + '<div class="sec" style="padding:18px 40px 0"><div class="rowh"><h2 class="h2">Trending now</h2><em>See all ' + ic('chevron-right', 's') + '</em></div>' + g([0, 1, 2, 3, 4, 5, 6, 7]) + '</div></div>')
    return dev('mac', side + tb + cont, '<b>macOS</b> · 1280×800 window · the Apple TV / Dynamic sidebar becomes the Mac sidebar, toolbar search in the title bar<br>same hero, rows and material; the window resizes down to the inner-display layout', cls='')



# ---------- SIDEBAR DRAWER ----------
SIDE = [('house', 'Home'), ('search', 'Search'), ('tv', 'TV Guide'), ('library', 'Library'), ('list', 'Lists'), ('trophy', 'Achievements')]
SIDE2 = [('layers', 'Sources and add-ons'), ('activity', 'Sports'), ('users', 'Watch Together'), ('settings', 'Settings')]


def sidebar_panel(x=0, w=300, on='Home'):
    body = (f'<div class="hd"><span class="av" style="width:34px;height:34px;font-size:14px;background:#5B6CF2">H</span><b>Henrique</b><span class="chip" style="margin-left:auto;height:26px;font-size:11px">{ic("chevron-down", "s")}</span></div>'
            + ''.join(f'<a class="{"on" if l == on else ""}">{ic(i)}{l}</a>' for i, l in SIDE)
            + '<div class="k">Streaming</div>' + ''.join(f'<a>{ic(i)}{l}</a>' for i, l in SIDE2[:3])
            + '<div style="flex:1"></div>' + f'<a>{ic("settings")}Settings</a>')
    return f'<div class="drawer ilha" style="left:{x}px;width:{w}px">{body}</div>'


def drawer_outer():
    # mid-gesture: the panel follows the finger from the left edge; Home dims behind it
    scr = (f'<div class="scr full"><div class="hero full"><img class="bg" src="{f(FALLOUT)}" style="object-position:58% 20%">'
           '<div class="veu" style="background:linear-gradient(180deg,rgba(13,13,13,0) 38%,rgba(13,13,13,.72) 62%,var(--bg) 82%)"></div></div>'
           '<div style="position:absolute;left:20px;right:104px;top:296px;display:flex;flex-direction:column;align-items:center;text-align:center;gap:12px">'
           f'<img src="{f(FALLOUT_LOGO)}" style="height:84px;object-fit:contain">'
           '<div class="meta big" style="justify-content:center;font-size:16px"><span>Series</span><span class="pt"></span><span>Sci-Fi</span><span class="pt"></span><span>2024</span></div>'
           f'<span class="pill xl wide" style="margin-top:6px">{ic("play")}Continue S1:E3</span></div>'
           '<div class="dim" style="background:rgba(0,0,0,.45)"></div>'
           + sidebar_panel(-54) + '<span class="hand" style="left:226px;top:330px"></span></div>')
    bar = vbar('home', cam=True)
    return dev('outer', scr, '<b>Closed · swipe right from the left edge</b> · the TV sidebar follows the finger as an island<br>Home dims behind it; release past a third and it snaps open; the vertical bar stays', bar=bar)


def drawer_outer_open():
    scr = (f'<div class="scr full"><div class="hero full"><img class="bg" src="{f(FALLOUT)}" style="object-position:58% 20%">'
           '<div class="veu" style="background:linear-gradient(180deg,rgba(13,13,13,0) 38%,rgba(13,13,13,.72) 62%,var(--bg) 82%)"></div></div>'
           '<div class="dim" style="background:rgba(0,0,0,.55)"></div>' + sidebar_panel(0, 312) + '</div>')
    bar = vbar('home', cam=True)
    return dev('outer', scr, '<b>Closed · sidebar open</b> · the same rail, sections and profile as on the TV<br>tap outside or swipe left to close; the active page is the lit row', bar=bar)


def drawer_inner():
    # open display, book pose: the sidebar lives entirely in the left half; content keeps the right half
    scr = (f'<div class="scr full"><div class="hero full"><img class="bg" src="{f(FALLOUT)}" style="object-position:50% 18%">'
           '<div class="veu" style="background:linear-gradient(90deg,rgba(13,13,13,.78) 0,rgba(13,13,13,.45) 34%,rgba(13,13,13,0) 60%),linear-gradient(180deg,rgba(13,13,13,0) 45%,rgba(13,13,13,.7) 78%,var(--bg) 100%)"></div></div>'
           '<div style="position:absolute;left:380px;bottom:78px;display:flex;flex-direction:column;gap:14px;max-width:460px">'
           f'<img src="{f(FALLOUT_LOGO)}" style="height:110px;width:auto;object-fit:contain;object-position:left">'
           '<div class="meta big"><span>Series</span><span class="pt"></span><span>Sci-Fi</span><span class="pt"></span><span>2 seasons</span></div>'
           f'<p class="sub big" style="margin:0;max-width:440px">{SYN_FALLOUT}</p>'
           f'<div class="acts" style="gap:12px"><span class="pill xl">{ic("play")}Continue S1:E3</span><span class="dsc xl">{ic("plus")}</span><span class="dsc xl">{ic("ellipsis")}</span></div></div>'
           + sidebar_panel(0, 330) + '</div>')
    bar = vbar('home', cam=False)
    cap = ('<b>Open · sidebar open</b> · the drawer takes the left third; Home reflows beside it<br>on regular width the sidebar can stay pinned, like the Apple TV layout')
    return dev('inner', scr, cap, bar=bar)


# ---------- SIDE BAR OPTIONS ----------
def barcard(inner_bar, title, text, extra=''):
    return (f'<div class="lbl"><div class="bc"><img class="bg" src="{f(FALLOUT)}"><div class="veil"></div>{extra}{inner_bar}</div>'
            f'<b>{title}</b><span>{text}</span></div>')


def bar_option_cards():
    cards_ = []
    # 1 system default
    cards_.append(barcard(vbar('home', cam=True), '1 · System bar (today)',
                          'Status, nav and the tab capsule share the 84 pt region. Free from UITabBarController; fold and camera avoidance included.'))
    # 2 expand on hold
    wide = ('<aside class="vbar">' + status(True) + '<div class="vsp"></div><div class="tabs wide gl" style="margin-right:-0px;align-self:flex-end">'
            + ''.join(f'<i class="tb{" on" if k == 0 else ""}">{ic(i)}{l}</i>' for k, (i, l) in enumerate([('house', 'Home'), ('search', 'Search'), ('tv', 'TV Guide'), ('library', 'Library')]))
            + '</div></aside>')
    cards_.append(barcard(wide, '2 · Expands on hold',
                          'Touch and hold the capsule: it grows leftwards into icon + label, like the TV rail opening. Release on a row to go there.'))
    # 3 context morph
    ctx = ('<aside class="vbar">' + status(True) + f'<div class="nav"><span class="nb gl">{ic("chevron-left")}</span></div>'
           f'<div class="tools">' + ''.join(f'<span class="nb gl">{ic(i)}</span>' for i in ['layers', 'clapperboard', 'eye-off', 'plus']) + '</div>'
           '<div class="vsp"></div><div class="tabs mini gl"><i class="tb on">' + ic('house') + '</i></div></aside>')
    cards_.append(barcard(ctx, '3 · Morphs per screen',
                          'Detail: Back, then Sources / Trailer / Watched / Library. The tab capsule folds to the active icon; the items cross-fade as you navigate (the 2.1 "More" morph).'))
    # 4 scroll aware
    mini = ('<aside class="vbar">' + status(True) + '<div class="vsp"></div><div class="tabs mini gl"><i class="tb on">' + ic('house')
            + '</i><div class="dd"><i class="on"></i><i></i><i></i><i></i></div></div></aside>')
    cards_.append(barcard(mini, '4 · Shrinks while scrolling',
                          'Rows moving: the capsule collapses to the active icon and four dots. Stop, or pull up, and it springs back. More art, less chrome.'))
    # 5 clock island on the side
    isl = ('<aside class="vbar"><div class="isl" style="margin-top:8px"><span style="font-size:14px;font-weight:700">9:41</span>' + ic('cloud') + '<b>14</b><small>add-ons synced</small></div>'
           '<div class="vsp"></div>' + tabs('home') + '</aside>')
    cards_.append(barcard(isl, '5 · The clock island, vertical',
                          'The TV\'s clock island moves into the side region: time on top, then the toast (sync, download, reminder) grows downwards like a vertical Dynamic Island.'))
    # 6 now playing dock
    npd = ('<aside class="vbar">' + status(True) + '<div class="vsp"></div>' + tabs('home')
           + f'<div class="np"><img src="{f(EPS[1])}"></div></aside>')
    cards_.append(barcard(npd, '6 · Now-playing dock',
                          'Leave the player and its episode docks under the capsule with a progress ring. Tap to return, swipe up for PiP, swipe down to stop.'))
    # 7 thumb scrub
    scrub = ('<aside class="vbar">' + status(True) + '<div class="vsp"></div><div class="tabs gl" style="position:relative"><span class="thumb" style="top:68px"></span>'
             + ''.join(f'<i class="tb" style="position:relative;z-index:1;{"color:#fff" if k == 1 else ""}">{ic(i)}</i>' for k, i in enumerate(['house', 'search', 'tv', 'library']))
             + '<i class="tb" style="position:relative;z-index:1"><span class="avp"></span></i></div>'
             '<span class="hand" style="right:14px;bottom:214px;width:40px;height:40px"></span></aside>')
    cards_.append(barcard(scrub, '7 · Scrub the capsule',
                          'Drag along the capsule and the highlight follows the thumb with a haptic tick per tab; the page underneath switches live. Lift to commit.'))
    # 8 tinted glass
    tint = ('<aside class="vbar">' + status(True) + '<div class="vsp"></div>' + tabs('home').replace('class="tabs gl"', 'class="tabs gl tint"') + '</aside>')
    cards_.append(barcard(tint, '8 · Glass tinted by the hero',
                          'The capsule takes the hero palette (the same source as ajustes_acento on the TV). Liquid Glass refracts the art; the tint shifts as the hero changes.'))
    # 9 immersive edge
    edge = ('<aside class="vbar">' + status(True) + '</aside><span class="edge"></span>')
    cards_.append(barcard(edge, '9 · Immersive edge',
                          'In the player the bar fades to a thin glow on the hardware edge. Swipe in from it to bring the controls back; nothing else sits over the picture.'))
    return cards_


def bar_options_page():
    cards_ = bar_option_cards()
    return (f'<!doctype html><html><head><meta charset="utf-8"><title>Side bar options</title><style>{font_css()}{CSS}</style></head><body>'
            '<div class="head"><h1>Nuvio · iPhone Duo · The side region, nine ways</h1><p>Every option keeps the system contract: back on top, tab bar at the bottom, nothing in the fold. '
            '1 is free; 2, 4, 7 and 8 are tab-capsule customisations; 3, 5, 6 and 9 are Nuvio behaviours layered on the bar. They combine: 1 + 3 + 6 is the recommended set.</p></div>'
            f'<div class="grid3">{"".join(cards_)}</div></body></html>')

# ---------- PAGES ----------
PAGES = [
    ('01-home', 'Home', 'Compact width is the phone you know; open, one continuous screen with the same hierarchy and the same tab bar.', [home_outer(), home_inner()]),
    ('02-search', 'Search', 'List-detail: the result list is the hierarchy on the outer display; the inner display shows the list and the selected title at once.', [search_outer(), search_inner()]),
    ('03-detail', 'Detail', 'Full-width detail: big logo, every rating, creators and the whole synopsis, then Continue, your rating and the action circles. Episodes follow below; in book pose they take the right half.', [detail_outer(), detail_inner()]),
    ('04-sources', 'Sources', 'The Sources sheet keeps Nuvio\'s island material. Closed, it gets a vertical bar; open, it slides in from the trailing edge over the detail.', [sources_outer(), sources_inner()]),
    ('05-player-poses', 'Player · closed and open', 'Immersive video under the side region when closed; video | Up next + Watch Together on one continuous screen when open.', [player_outer_land(), player_inner_book()]),
    ('06-player-tabletop', 'Player · tabletop and open', 'On a table, folded like a laptop: picture on the raised half, every control on the flat half. Open, the video fills the display and the controls overlay it.', [player_tabletop(), player_inner_flat()]),
    ('07-guide', 'TV Guide', 'Grid first; the inner display adds the channel preview with Watch, Favourite and PiP next to it.', [guide_outer(), guide_inner()]),
    ('08-library', 'Library', 'Three columns closed, nine open: one continuous grid across the whole display.', [library_outer(), library_inner()]),
    ('09-onboarding', 'Sign-in', 'QR and code as on the TV. Opening the device expands the same step into two panes; it never asks the user to open it.', [onboarding_outer(), onboarding_inner()]),
    ('10-profiles', 'Profiles', 'Who is watching, on both displays. Touch ID in the side button for the PIN-locked profile (no Face ID on this device).', [profiles_outer(), profiles_inner()]),
    ('11-settings', 'Settings', 'A list that pushes pages when closed and becomes list | page when open.', [settings_outer(), settings_inner()]),
    ('12-mac', 'Mac', 'The same design on macOS: sidebar navigation, title-bar search, Liquid Glass chrome, Nuvio islands inside.', [mac_home()]),
    ('13-sidebar', 'Sidebar by swipe', 'The TV sidebar (rail, sections, profile) is a drawer: swipe right from the left edge on any screen. Closed, it overlays; open, it sits in the left half or stays pinned.', [drawer_outer(), drawer_outer_open(), drawer_inner()]),
    ('14-sidebar-options', 'Side bar options', '', None),
]


def build():
    OUT.mkdir(exist_ok=True)
    for slug, title, note, figs in PAGES:
        (OUT / f'{slug}.html').write_text(page(title, note, figs) if figs is not None else bar_options_page())
    print(f'{len(PAGES)} pages in {OUT}')


def shoot():
    from playwright.sync_api import sync_playwright
    ENVIO.mkdir(parents=True, exist_ok=True)
    with sync_playwright() as p:
        b = p.chromium.launch(channel='chrome')
        for slug, *_ in PAGES:
            url = (OUT / f'{slug}.html').as_uri()
            ctx = b.new_context(viewport={'width': 400, 'height': 300}, device_scale_factor=1)
            pg = ctx.new_page(); pg.goto(url); pg.wait_for_timeout(300)
            w, h = pg.evaluate('[Math.ceil((document.querySelector(".page")||document.querySelector(".grid3")).getBoundingClientRect().right + 44), document.documentElement.scrollHeight]')
            ctx.close()
            w, h = min(int(w), 4000), min(int(h), 4000)
            s = min(2.0, 2000 / w, 2000 / h)
            ctx = b.new_context(viewport={'width': w, 'height': h}, device_scale_factor=s)
            pg = ctx.new_page(); pg.goto(url); pg.wait_for_timeout(400)
            png = OUT / f'{slug}.png'
            pg.screenshot(path=str(png), full_page=True)
            ctx.close()
            jpg = ENVIO / f'ios-{slug}.jpg'
            subprocess.run(['magick', str(png), '-resize', '2000x2000>', '-quality', '88', str(jpg)], check=True)
            print(jpg, f'{w}x{h} @{s:.2f}')
        b.close()


if __name__ == '__main__':
    build()
    if '--no-shot' not in sys.argv:
        shoot()
