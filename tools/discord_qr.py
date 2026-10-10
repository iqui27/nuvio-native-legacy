#!/usr/bin/env python3
# Gera deploy/app/art/marcas/discord-qr.png e discord-badge.png (QR do convite
# do Discord nas cores da marca, logo no centro, correcao H).
#   pip install segno; python3 tools/discord_qr.py   (escreve .svg/.html no diretorio atual)
#   Chrome --headless=new --screenshot=discord-qr-820.png --window-size=820,820 discord-qr.html
#   magick discord-qr-820.png -resize 410x410 -strip deploy/app/art/marcas/discord-qr.png
#   (selo: 672x356 -> 336x178). Conferir a leitura com zxing-cpp antes de commitar.
# O convite tem de ser o mesmo de NV_URL_DISCORD em src/apoio.h.
import segno
URL='https://discord.gg/9NWr6SHyzJ'
q=segno.make(URL,error='h'); M=[list(r) for r in q.matrix]; n=len(M)
LOGO="M20.317 4.3698a19.7913 19.7913 0 00-4.8851-1.5152.0741.0741 0 00-.0785.0371c-.211.3753-.4447.8648-.6083 1.2495-1.8447-.2762-3.68-.2762-5.4868 0-.1636-.3933-.4058-.8742-.6177-1.2495a.077.077 0 00-.0785-.037 19.7363 19.7363 0 00-4.8852 1.515.0699.0699 0 00-.0321.0277C.5334 9.0458-.319 13.5799.0992 18.0578a.0824.0824 0 00.0312.0561c2.0528 1.5076 4.0413 2.4228 5.9929 3.0294a.0777.0777 0 00.0842-.0276c.4616-.6304.8731-1.2952 1.226-1.9942a.076.076 0 00-.0416-.1057c-.6528-.2476-1.2743-.5495-1.8722-.8923a.077.077 0 01-.0076-.1277c.1258-.0943.2517-.1923.3718-.2914a.0743.0743 0 01.0776-.0105c3.9278 1.7933 8.18 1.7933 12.0614 0a.0739.0739 0 01.0785.0095c.1202.099.246.1981.3728.2924a.077.077 0 01-.0066.1276 12.2986 12.2986 0 01-1.873.8914.0766.0766 0 00-.0407.1067c.3604.698.7719 1.3628 1.225 1.9932a.076.076 0 00.0842.0286c1.961-.6067 3.9495-1.5219 6.0023-3.0294a.077.077 0 00.0313-.0552c.5004-5.177-.8382-9.6739-3.5485-13.6604a.061.061 0 00-.0312-.0286zM8.02 15.3312c-1.1825 0-2.1569-1.0857-2.1569-2.419 0-1.3332.9555-2.4189 2.157-2.4189 1.2108 0 2.1757 1.0952 2.1568 2.419 0 1.3332-.9555 2.4189-2.1569 2.4189zm7.9748 0c-1.1825 0-2.1569-1.0857-2.1569-2.419 0-1.3332.9554-2.4189 2.1569-2.4189 1.2108 0 2.1757 1.0952 2.1568 2.419 0 1.3332-.946 2.4189-2.1568 2.4189Z"
W=820; Q=3; u=W/(n+2*Q); o=Q*u   # 3 modulos de silencio + cartao branco em volta no app
def fin(x,y): return (x<7 and y<7) or (x>=n-7 and y<7) or (x<7 and y>=n-7)
c0=(n-9)//2; c1=c0+9            # janela 9x9 do logo (27% do lado; H aguenta 30% da area: 81/1089=7%)
s=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{W}" viewBox="0 0 {W} {W}">',
   '<defs><linearGradient id="g" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#5865F2"/><stop offset="1" stop-color="#3C45A5"/></linearGradient></defs>',
   f'<rect width="{W}" height="{W}" fill="#fff"/>','<g fill="url(#g)">']
r=u*0.46
for y in range(n):
    for x in range(n):
        if not M[y][x] or fin(x,y) or (c0<=x<c1 and c0<=y<c1): continue
        s.append(f'<circle cx="{o+(x+.5)*u:.2f}" cy="{o+(y+.5)*u:.2f}" r="{r:.2f}"/>')
s.append('</g>')
for fx,fy in ((0,0),(n-7,0),(0,n-7)):
    X=o+fx*u; Y=o+fy*u
    s.append(f'<rect x="{X:.2f}" y="{Y:.2f}" width="{7*u:.2f}" height="{7*u:.2f}" rx="{2.1*u:.2f}" fill="#5865F2"/>')
    s.append(f'<rect x="{X+u:.2f}" y="{Y+u:.2f}" width="{5*u:.2f}" height="{5*u:.2f}" rx="{1.3*u:.2f}" fill="#fff"/>')
    s.append(f'<rect x="{X+2*u:.2f}" y="{Y+2*u:.2f}" width="{3*u:.2f}" height="{3*u:.2f}" rx="{0.9*u:.2f}" fill="#23272A"/>')
cx=o+c0*u; L=9*u; pad=0.45*u
s.append(f'<rect x="{cx+pad:.2f}" y="{cx+pad:.2f}" width="{L-2*pad:.2f}" height="{L-2*pad:.2f}" rx="{2.2*u:.2f}" fill="#5865F2"/>')
k=(L-2*pad)*0.62/24; t=cx+pad+(L-2*pad)*0.19
s.append(f'<path transform="translate({t:.2f},{t+0.2*u:.2f}) scale({k:.4f})" fill="#fff" d="{LOGO}"/>')
s.append('</svg>'); open('discord-qr.svg','w').write('\n'.join(s))
# selo 672x356 como o do Ko-fi
b=['<svg xmlns="http://www.w3.org/2000/svg" width="672" height="356" viewBox="0 0 672 356">',
 '<rect width="672" height="356" rx="36" fill="#5865F2"/>',
 f'<path transform="translate(56,96) scale(6.9)" fill="#fff" d="{LOGO}"/>',
 '<text x="262" y="150" font-family="Montserrat, Helvetica Neue, Arial" font-weight="500" font-size="50" fill="#E0E3FF">Join us on</text>',
 '<text x="258" y="252" font-family="Montserrat, Helvetica Neue, Arial" font-weight="800" font-size="100" fill="#fff">Discord</text>','</svg>']
open('discord-badge.svg','w').write('\n'.join(b))
for f,w,h in (('discord-qr',820,820),('discord-badge',672,356)):
    open(f+'.html','w').write(f'<html><body style="margin:0;background:transparent"><img src="{f}.svg" width="{w}" height="{h}" style="display:block"></body></html>')
