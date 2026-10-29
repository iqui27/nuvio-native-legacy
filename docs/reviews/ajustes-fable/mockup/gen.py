#!/usr/bin/env python3
"""Settings redesign mockup: three islands (sections / options / explanation).
Generates one HTML per frame with Lucide SVGs inlined. Fonts: InterDisplay."""
import os,re,sys,html
ROOT=os.path.abspath(os.path.join(os.path.dirname(__file__),'../../../..'))
ICON=os.path.join(ROOT,'deploy/app/art/icones/lucide')
FONT=os.path.join(ROOT,'design/universo-2026/assets')
ARTE=os.path.join(ROOT,'design/glass-ilha')
def ic(name,cls=''):
    s=open(os.path.join(ICON,name+'.svg')).read()
    s=re.sub(r'\s+',' ',s).replace('width="24" height="24"','')
    return f'<i class="ic {cls}">{s}</i>'
CSS=f"""
@font-face{{font-family:ID;src:url('file://{FONT}/InterDisplay-Regular.ttf');font-weight:400}}
@font-face{{font-family:ID;src:url('file://{FONT}/InterDisplay-Bold.ttf');font-weight:500 800}}
:root{{--ac:#EC407A;--tinta:#fff;--bg:#0D0D0D;--fg:#F3F2EF;--mut:rgba(243,242,239,.58);--mut2:rgba(243,242,239,.42);
--ilha:rgba(14,15,18,.80);--foco:rgba(255,255,255,.13);--leve:rgba(255,255,255,.08);--fio:rgba(255,255,255,.08);--trilho:rgba(255,255,255,.05)}}
body[data-mat=solido]{{--ilha:#121316;--foco:#2B2D34;--leve:#24262C;--fio:rgba(255,255,255,.07);--trilho:#1D1E23}}
*{{box-sizing:border-box}}
html,body{{margin:0;width:1920px;height:1080px;overflow:hidden;background:var(--bg);color:var(--fg);font-family:ID,Inter,system-ui,sans-serif;-webkit-font-smoothing:antialiased}}
.tv{{position:absolute;inset:0;overflow:hidden}}
.arte{{position:absolute;inset:0;background:url('file://{ARTE}/arte.jpg') center/cover;filter:saturate(.9)}}
body[data-mat=solido] .arte{{display:none}}
.veu{{position:absolute;inset:0;background:linear-gradient(90deg,rgba(8,8,10,.94) 0,rgba(8,8,10,.82) 40%,rgba(8,8,10,.55) 100%),linear-gradient(0deg,rgba(13,13,13,.95) 0,rgba(13,13,13,.2) 50%)}}
body[data-mat=solido] .veu{{background:radial-gradient(90% 70% at 50% 30%,#17181c,#0D0D0D)}}
.ilha{{position:absolute;background:var(--ilha);border-radius:28px;box-shadow:0 14px 40px rgba(0,0,0,.36);overflow:hidden}}
.ilha::before{{content:"";position:absolute;inset:0;background:radial-gradient(120% 80% at 18% -40%,rgba(255,255,255,.07),rgba(255,255,255,0) 60%);pointer-events:none}}
body[data-mat=solido] .ilha::before{{background:radial-gradient(120% 80% at 18% -40%,rgba(255,255,255,.045),rgba(255,255,255,0) 60%)}}
.relogio{{left:48px;top:36px;height:56px;border-radius:28px;padding:0 10px 0 24px;display:flex;align-items:center;gap:12px;font-size:24px;font-weight:600}}
.relogio .s{{width:1px;height:22px;background:rgba(255,255,255,.18)}}
.av{{width:36px;height:36px;border-radius:50%;display:flex;align-items:center;justify-content:center;font-size:16px;font-weight:700;background:#7c5cff}}
.rail{{left:48px;top:112px;width:88px;border-radius:44px;padding:14px;display:flex;flex-direction:column;gap:4px;align-items:center}}
.rail a{{width:60px;height:60px;border-radius:50%;display:flex;align-items:center;justify-content:center;color:rgba(243,242,239,.6);position:relative}}
.rail a.ativo{{color:#fff}} .rail a.ativo::after{{content:"";position:absolute;bottom:7px;width:5px;height:5px;border-radius:50%;background:var(--ac)}}
.ic{{display:inline-flex;width:26px;height:26px;flex:none}} .ic svg{{width:100%;height:100%;stroke-width:1.9}} .ic.p{{width:20px;height:20px}} .ic.g{{width:34px;height:34px}}
.kick{{font-size:15px;font-weight:700;letter-spacing:.14em;text-transform:uppercase;color:var(--mut2)}}
.h1{{font-size:56px;font-weight:800;letter-spacing:-.02em;line-height:1.05;margin:4px 0 0}}
.cab{{position:absolute;left:200px;top:112px;right:48px;display:flex;align-items:flex-end;justify-content:space-between}}
.chips{{display:flex;gap:12px;align-items:center;padding-bottom:6px}}
.chip{{height:56px;border-radius:28px;padding:0 22px;display:inline-flex;align-items:center;gap:10px;font-size:19px;font-weight:600;background:var(--leve);color:rgba(243,242,239,.86);white-space:nowrap}}
.chip .n{{color:var(--mut2);font-weight:500}} .chip.on .n{{color:var(--ac)}}
.chip.foco{{background:var(--ac);color:var(--tinta);box-shadow:0 8px 28px rgba(236,64,122,.35)}} .chip.foco .n{{color:inherit;opacity:.8}}
/* três ilhas */
.sec{{left:200px;top:232px;width:400px;height:768px;padding:12px 14px}}
.opc{{left:616px;top:232px;width:760px;height:768px;padding:0}}
.exp{{left:1392px;top:232px;width:480px;height:768px;padding:24px}}
.sec .r{{height:54px;border-radius:20px;display:flex;align-items:center;gap:16px;padding:0 18px;color:rgba(243,242,239,.78);font-size:22px;font-weight:600}}
.sec .r .ic{{color:rgba(243,242,239,.62)}} .sec .r.foco{{background:var(--foco);color:#fff}} .sec .r.foco .ic{{color:#fff}}
.sec .r.atual:not(.foco){{color:#fff}} .sec .r .pt{{margin-left:auto;width:5px;height:5px;border-radius:50%;background:var(--ac)}}
.sec .r .n{{margin-left:auto;font-size:16px;color:var(--mut2);font-weight:500}} .sec .r.foco .n{{color:rgba(255,255,255,.6)}}
.sec .grp{{font-size:12px;font-weight:700;letter-spacing:.14em;text-transform:uppercase;color:var(--mut2);padding:10px 18px 4px}}
.opc .topo{{padding:26px 30px 10px;display:flex;align-items:baseline;gap:14px}} .opc .topo b{{font-size:28px;font-weight:700}} .opc .topo span{{font-size:17px;color:var(--mut2)}}
.opc .lista{{position:absolute;left:14px;right:14px;top:84px;bottom:0}} .opc::after{{content:"";position:absolute;left:0;right:0;bottom:0;height:70px;background:linear-gradient(0deg,var(--ilha),rgba(14,15,18,0));pointer-events:none}} body[data-mat=solido] .opc::after{{background:linear-gradient(0deg,#121316,rgba(18,19,22,0))}}
.opc .grp{{font-size:14px;font-weight:700;letter-spacing:.14em;text-transform:uppercase;color:var(--mut2);padding:16px 18px 6px;display:flex;gap:10px;align-items:center}}
.opc .grp .fio{{flex:1;height:1px;background:var(--fio)}} .opc .grp .n{{font-weight:500;letter-spacing:0;text-transform:none;font-size:15px}}
.opc .grp.tec{{color:var(--ac)}} .opc .grp.tec .fio{{background:rgba(236,64,122,.25)}}
.row{{height:76px;border-radius:20px;display:flex;align-items:center;gap:18px;padding:0 18px;position:relative}}
.row .ic{{color:rgba(243,242,239,.6)}} .row .t{{display:flex;flex-direction:column;gap:3px;min-width:0}}
.row .t b{{font-size:22px;font-weight:600;color:rgba(243,242,239,.92);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}} .row .t s{{text-decoration:none;font-size:16px;color:var(--mut2);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}}
.row .v{{margin-left:auto;display:flex;align-items:center;gap:10px;font-size:20px;font-weight:500;color:var(--mut);white-space:nowrap}}
.row .v .ic{{width:20px;height:20px;color:var(--mut2)}}
.row.foco{{background:var(--foco)}} .row.foco .t b,.row.foco .ic{{color:#fff}} .row.foco .t s{{color:rgba(243,242,239,.62)}} .row.foco .v{{color:#fff}}
.row .dif{{font-size:12px;font-weight:700;letter-spacing:.1em;text-transform:uppercase;color:var(--ac);background:rgba(236,64,122,.16);padding:4px 8px;border-radius:999px}}
.row.off .t b,.row.off .t s,.row.off .v{{opacity:.45}}
.tg{{width:52px;height:30px;border-radius:15px;background:rgba(255,255,255,.14);position:relative;flex:none}} .tg i{{position:absolute;top:3px;left:3px;width:24px;height:24px;border-radius:50%;background:rgba(255,255,255,.75)}}
.tg.on{{background:var(--ac)}} .tg.on i{{left:25px;background:#fff}}
.mais{{height:64px;margin:12px 4px 0;border-radius:20px;display:flex;align-items:center;gap:14px;padding:0 18px;font-size:19px;font-weight:600;color:rgba(243,242,239,.78);background:var(--leve)}}
.mais .n{{color:var(--mut2);font-weight:500}} .mais .ic{{margin-left:auto;color:var(--mut2)}}
.mais.foco{{background:var(--ac);color:var(--tinta)}} .mais.foco .n,.mais.foco .ic{{color:inherit}}
/* explicação */
.exp .previa{{height:243px;border-radius:20px;overflow:hidden;position:relative;background:url('file://{ARTE}/detalhe.jpg') center/cover}}
.exp .previa::after{{content:"";position:absolute;inset:0;background:linear-gradient(0deg,rgba(0,0,0,.65),rgba(0,0,0,.05) 60%)}}
.exp .previa .ex{{position:absolute;right:12px;top:12px;font-size:12px;font-weight:700;letter-spacing:.1em;text-transform:uppercase;color:rgba(255,255,255,.7);background:rgba(10,10,12,.6);padding:5px 9px;border-radius:999px;z-index:1}}
.exp .previa .mini{{position:absolute;left:18px;right:18px;bottom:16px;z-index:1}}
.exp .previa .tit{{font-size:24px;font-weight:800;letter-spacing:.03em}} .exp .previa .marcas{{display:flex;gap:8px;margin-top:8px}}
.exp .previa .marca{{font-size:13px;font-weight:700;letter-spacing:.06em;padding:4px 8px;border-radius:7px;border:1.5px solid rgba(255,255,255,.55);color:rgba(255,255,255,.85)}}
.exp .previa .marca.ac{{border-color:var(--ac);color:#fff;background:rgba(236,64,122,.35)}} .exp .previa .marca.dim{{opacity:.35}}
.exp .previa .trilho{{height:5px;border-radius:3px;background:rgba(255,255,255,.2);margin-top:12px;position:relative}} .exp .previa .trilho i{{position:absolute;left:0;top:0;bottom:0;width:38%;border-radius:3px;background:var(--ac)}}
.exp .k{{margin:22px 0 8px}} .exp .texto{{font-size:19px;line-height:1.45;color:rgba(243,242,239,.86)}}
.exp .meta{{display:flex;flex-direction:column;gap:8px;margin-top:20px;padding-top:18px;border-top:1px solid var(--fio)}}
.exp .meta div{{display:flex;justify-content:space-between;font-size:17px;color:var(--mut)}} .exp .meta div b{{color:rgba(243,242,239,.9);font-weight:600}}
.exp .dica{{position:absolute;left:24px;right:24px;bottom:22px;font-size:15px;color:var(--mut2);display:flex;gap:8px;align-items:center}}
.exp .dica .key{{font-size:12px;font-weight:700;border:1.5px solid rgba(255,255,255,.3);border-radius:6px;padding:2px 7px;color:rgba(255,255,255,.8)}}
.exp .titulo{{font-size:26px;font-weight:700;margin-top:18px}} .exp .sub{{font-size:17px;color:var(--mut2);margin-top:4px}}
/* seletor dentro da ilha de explicação */
.val{{height:60px;border-radius:18px;display:flex;align-items:center;gap:14px;padding:0 16px;font-size:21px;font-weight:500;color:rgba(243,242,239,.8)}}
.val .ic{{margin-left:auto;width:22px;height:22px;color:var(--ac)}} .val.foco{{background:var(--foco);color:#fff}} .val.atual{{color:#fff;font-weight:600}}
.val .seg{{margin-left:auto;font-size:15px;color:var(--mut2);font-weight:500}}
.rodape{{position:absolute;left:200px;right:48px;bottom:30px;display:flex;justify-content:space-between;font-size:16px;color:var(--mut2)}}
.rodape span{{display:inline-flex;align-items:center;gap:8px;margin-right:26px}}
.key{{font-size:12px;font-weight:700;border:1.5px solid rgba(255,255,255,.3);border-radius:6px;padding:2px 7px;color:rgba(255,255,255,.8)}}
.leg{{position:absolute;left:200px;top:40px;font-size:15px;color:var(--mut2);letter-spacing:.04em}}
"""
SECS=[('circle-play','Reprodução',87),('languages','Idiomas e legendas',35),('puzzle','Fontes e addons',34),('tv-minimal-play','TV ao vivo',45),
('layout-dashboard','Tela inicial',53),('palette','Aparência',30),('file-text','Página do título',36),('clapperboard','Cartazes e trailers',36),
('user-round','Conta e serviços',19),('monitor-cog','Esta TV',14),('info','Sobre e ajuda',12)]
GRUPOS=[('ASSISTIR',0,4),('TELAS',4,8),('CONTA E SISTEMA',8,11)]
def rail():
    names=['house','compass','tv','search','library','calendar','user-round','settings-2']
    return '<nav class="ilha rail">'+''.join(f'<a class="{"ativo" if i==7 else ""}">{ic(n)}</a>' for i,n in enumerate(names))+'</nav>'
def header(foco=None,n_dif=3):
    chips=[('search','Buscar ajuste',''),('wand-sparkles','Predefinições',''),('rotate-ccw','Diferente do padrão',f'· {n_dif}'),('sparkles','O que o Nuvio faz?','')]
    out=''.join(f'<span class="chip {"on" if c[2] else ""} {"foco" if foco==i else ""}">{ic(c[0],"p")}{c[1]}<span class="n">{c[2]}</span></span>' for i,c in enumerate(chips))
    return f'<div class="cab"><div><div class="kick">Nuvio · TV da sala</div><div class="h1">Ajustes</div></div><div class="chips">{out}</div></div>'
def secoes(foco=None,atual=0):
    out=''
    for g,a,b in GRUPOS:
        out+=f'<div class="grp">{g}</div>'
        for i in range(a,b):
            n,t,c=SECS[i]; cls=('foco' if foco==i else '')+(' atual' if atual==i else '')
            out+=f'<div class="r {cls}">{ic(n)}{t}<span class="n">{c}</span></div>'
    return f'<div class="ilha sec">{out}</div>'
def row(icon,t,s,v=None,tg=None,foco=False,dif=False,off=False):
    val=''
    if tg is not None: val=f'<span class="tg {"on" if tg else ""}"><i></i></span>'
    elif v is not None: val=f'{html.escape(v)}{ic("chevron-right","p")}'
    d='<span class="dif">alterado</span>' if dif else ''
    return f'<div class="row {"foco" if foco else ""} {"off" if off else ""}">{ic(icon)}<div class="t"><b>{html.escape(t)}</b><s>{html.escape(s)}</s></div><div class="v">{d}{val}</div></div>'
def grupo(t,n=None,av=False):
    return f'<div class="grp {"tec" if av else ""}">{t}<span class="fio"></span>{f"<span class=n>{n}</span>" if n else ""}</div>'
def opcoes(titulo,sub,corpo):
    return f'<div class="ilha opc"><div class="topo"><b>{titulo}</b><span>{sub}</span></div><div class="lista">{corpo}</div></div>'
def rodape(txt_dir):
    return f'<div class="rodape"><div><span><b class="key">OK</b>abrir</span><span>{ic("arrow-left","p")}voltar</span><span>{ic("arrow-up-down","p")}navegar</span><span>{ic("arrow-right","p")}próxima ilha</span></div><div>{txt_dir}</div></div>'
def page(mat,leg,body):
    return f'<!doctype html><html lang="pt-BR"><head><meta charset="utf-8"><title>Ajustes</title><style>{CSS}</style></head><body data-mat="{mat}"><div class="tv"><div class="arte"></div><div class="veu"></div><div class="ilha relogio">20:44<span class="s"></span><span class="av">M</span></div>{rail()}{body}</div></body></html>'

REPRO_ROWS=lambda f:[grupo('Imagem e som'),
 row('gauge','Qualidade máxima','Teto de resolução que o Nuvio busca','Automática',foco=(f=='qual')),
 row('sparkles','HDR e Dolby Vision','Que versão HDR preferir na escolha da fonte','HDR10 e DV'),
 row('zap','Dolby Vision','Aceita fontes em Dolby Vision','', tg=True),
 row('audio-lines','Dolby Atmos','Aceita faixas de áudio Atmos','',tg=True,dif=True),
 row('volume-2','Normalizar volume','DTS convertido para PCM e player Android','',tg=False),
 grupo('Escolha da fonte'),
 row('list-video','Escolher a fonte ao reproduzir','Abre a lista de fontes antes de tocar','',tg=False),
 row('list-ordered','A escolha automática prioriza','O critério que decide a fonte','Qualidade'),
 row('languages','Preferir o idioma do áudio','Fonte com a faixa no seu idioma primeiro','',tg=True),
 row('hourglass','Espera pelos add-ons','Quanto tempo esperar as fontes chegarem','6 s'),
 ]
def mais(n,foco=False,aberto=False):
    return f'<div class="mais {"foco" if foco else ""}">{ic("sliders-horizontal","p")}Mais opções<span class="n">· {n} técnicas</span>{ic("chevron-up" if aberto else "chevron-down")}</div>'
def exp_categoria():
    return ('<div class="ilha exp"><div class="previa"><span class="ex">Prévia</span><div class="mini"><div class="tit">OS RELOJOEIROS</div>'
      '<div class="marcas"><span class="marca ac">4K</span><span class="marca">DV</span><span class="marca">ATMOS</span></div><div class="trilho"><i></i></div></div></div>'
      '<div class="titulo">Reprodução</div><div class="sub">Qualidade, som e o que o player faz</div>'
      '<div class="kick k">O que tem aqui</div><div class="texto">Resolução e HDR, como o Nuvio escolhe a fonte, o que aparece ao pausar e depois do vídeo, e a miniatura ao avançar.</div>'
      '<div class="meta"><div>Opções<b>46 + 41 técnicas</b></div><div>Diferentes do padrão<b>3</b></div></div>'
      f'<div class="dica"><span class="key">OK</span> entra na seção · <span class="key">→</span> também</div></div>')
def exp_opcao():
    return ('<div class="ilha exp"><div class="previa"><span class="ex">Prévia · exemplo</span><div class="mini"><div class="tit">OS RELOJOEIROS</div>'
      '<div class="marcas"><span class="marca ac">4K</span><span class="marca">DV</span><span class="marca">ATMOS</span></div><div class="trilho"><i></i></div></div></div>'
      '<div class="titulo">Qualidade máxima</div><div class="sub">Reprodução › Imagem e som</div>'
      '<div class="kick k">O que faz</div><div class="texto">Limita a resolução que a escolha automática aceita. Em Automática o Nuvio pega a melhor fonte que esta TV decodifica; 1080p ou 720p economizam banda e abrem mais rápido.</div>'
      '<div class="meta"><div>Agora<b>Automática</b></div><div>Padrão<b>Automática</b></div><div>Vale para<b>esta conta, todas as TVs</b></div></div>'
      f'<div class="dica"><span class="key">OK</span> escolhe entre 4 valores · <span class="key">←</span> volta às seções</div></div>')
def exp_seletor():
    vals=[('Da conta','segue o idioma do perfil',False,False),('Original do título','a faixa de origem, sem dublagem',False,False),('Português (Brasil)','',True,True),('English','',False,False),('Español','',False,False),('Français','',False,False),('日本語','',False,False)]
    out=''.join(f'<div class="val {"foco" if f else ""} {"atual" if a else ""}">{html.escape(v)}{f"<span class=seg>{s}</span>" if s else ""}{ic("check") if a else ""}</div>' for v,s,a,f in vals)
    return ('<div class="ilha exp"><div class="kick">Idioma do áudio</div><div class="titulo" style="margin-top:6px">Escolha a faixa</div>'
      f'<div style="margin-top:16px">{out}</div>'
      '<div class="meta"><div>Agora<b>Português (Brasil)</b></div><div>Padrão<b>Da conta</b></div></div>'
      '<div class="texto" style="margin-top:14px;font-size:17px;color:var(--mut)">Se o filme não tiver essa faixa, toca a original e avisa na ilha do relógio.</div>'
      f'<div class="dica"><span class="key">OK</span> aplica · <span class="key">←</span> cancela e volta à linha</div></div>')
def exp_avancada():
    return ('<div class="ilha exp"><div class="previa" style="background:url(\'file://'+ARTE+'/arte.jpg\') center/cover"><span class="ex">Prévia</span><div class="mini"><div class="tit">BUFFER</div>'
      '<div class="trilho" style="height:10px;background:rgba(255,255,255,.14)"><i style="width:62%"></i></div><div style="font-size:14px;color:rgba(255,255,255,.7);margin-top:8px">pronto 24 s à frente · conversão DTS/DV</div></div></div>'
      '<div class="titulo">Buffer</div><div class="sub">Reprodução › Som e vídeo desta TV · <span style="color:var(--ac)">técnica</span></div>'
      '<div class="kick k">O que faz</div><div class="texto">Quanto vídeo o Nuvio prepara antes de mostrar, quando esta TV precisa converter DTS ou Dolby Vision. Maior aguenta rede instável; menor abre mais rápido.</div>'
      '<div class="meta"><div>Agora<b>Equilibrado</b></div><div>Padrão<b>Equilibrado</b></div><div>Vale para<b>só esta TV</b></div></div>'
      f'<div class="dica"><span class="key">OK</span> escolhe · <span class="key">↑</span> fecha Mais opções ao chegar no topo</div></div>')

frames={}
frames['01-indice']=('vidro','1 · Índice: foco na seção, a do meio mostra o conteúdo, a da direita explica',
  header()+secoes(foco=0,atual=0)+opcoes('Reprodução','46 opções · 41 técnicas',''.join(REPRO_ROWS(None))+mais(41))+exp_categoria()+rodape('Ajustes · Reprodução'))
frames['02-opcao']=('vidro','2 · Dentro da seção: linha em foco, explicação, padrão e alcance à direita',
  header()+secoes(atual=0)+opcoes('Reprodução','46 opções · 41 técnicas',''.join(REPRO_ROWS('qual'))+mais(41))+exp_opcao()+rodape('Reprodução · Imagem e som · 1 de 46'))
IDI=[grupo('Idiomas'),row('globe','Idioma do app','O idioma da interface da TV','Português (Brasil)'),row('volume-2','Idioma do áudio','A faixa que você prefere','Português (Brasil)',foco=True),
 row('audio-lines','Preferir faixa original','Antes da dublagem, quando existir','',tg=False),row('languages','Segundo idioma do áudio','Se a primeira escolha faltar','English'),
 row('captions','Idioma da legenda','A legenda que aparece primeiro','Português (Brasil)'),row('captions','Legenda forçada automática','Liga quando o áudio é no seu idioma','',tg=True),
 row('funnel','Mostrar no player','Só os seus idiomas ou todas','Só meus idiomas'),row('audio-lines','Sincronia automática da legenda','Ajusta a legenda ao áudio sozinha','',tg=True),
 grupo('Legenda principal'),row('type','Tamanho','O tamanho da letra na tela','100%'),row('type','Fonte da legenda','A letra que a legenda usa','Inter')]
frames['03-seletor']=('vidro','3 · OK numa escolha: a ilha da direita vira o seletor; Voltar devolve o foco à linha',
  header()+secoes(atual=1)+opcoes('Idiomas e legendas','34 opções · 1 técnica',''.join(IDI)+mais(1))+exp_seletor()+rodape('Idiomas e legendas · Idiomas · 2 de 34'))
AV=[grupo('Miniatura ao avançar'),row('film','Fita de miniaturas','Quadro anterior e seguinte ao lado','',tg=True),
 mais(41,aberto=True),
 grupo('Escolha da fonte',av=True),row('code','Regra automática (regex)','Expressão que escolhe a fonte sozinha','',tg=False),row('layers','Alcance da escolha','Quais add-ons entram na automática','Todos'),
 grupo('Som e vídeo desta TV',av=True),row('audio-lines','DTS para AC3','Converte DTS para a TV tocar','',tg=True),row('database','Cache do avanço','Memória para avançar sem recarregar','64 MB'),
 row('hourglass','Buffer','Quanto preparar antes de mostrar','Equilibrado',foco=True),row('timer','Manter o vídeo pronto','Por quanto tempo depois de sair','2 min'),
 row('rotate-ccw','Retomada rápida','Retoma sem reconferir a fonte','',tg=True)]
frames['04-tecnicas-solido']=('solido','4 · Mais opções abertas, material sólido (vidro desligado): mesmas formas, só muda o miolo',
  header()+secoes(atual=0)+opcoes('Reprodução','46 opções · 41 técnicas',''.join(AV))+exp_avancada()+rodape('Reprodução · Som e vídeo desta TV · 52 de 87'))
out=sys.argv[1] if len(sys.argv)>1 else '.'
for k,(mat,leg,body) in frames.items():
    open(os.path.join(out,k+'.html'),'w').write(page(mat,leg,body))
print(' '.join(frames))
