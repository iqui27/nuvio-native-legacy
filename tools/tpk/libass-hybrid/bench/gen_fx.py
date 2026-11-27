# Synthetic karaoke-template opening (FLE-style): every syllable/character is
# its own event with \move, \t, \blur and \fad, ~150-250 events on screen.
import random, sys
random.seed(11)
print("""[Script Info]
ScriptType: v4.00+
PlayResX: 1920
PlayResY: 1080
WrapStyle: 2
ScaledBorderAndShadow: yes

[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Romaji,Montserrat,56,&H00FFFFFF,&H000000FF,&H00603010,&H00000000,1,0,0,0,100,100,1,0,1,2.5,0,7,10,10,10,1
Style: Glow,Montserrat,56,&H00FFE0A0,&H000000FF,&H00FFB040,&H00000000,1,0,0,0,100,100,1,0,1,6,0,7,10,10,10,1

[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text""")
def t(ms): return "%d:%02d:%02d.%02d"%(ms//3600000,ms//60000%60,ms//1000%60,ms//10%100)
lines=["kimi no koe ga kikoeru made hashiri tsuzukeru","sora no mukou ni kagayaku hoshi wo sagashite",
       "namida no ato mo itsuka hikari ni kawaru","bokura wa mada owaranai yume no naka de"]
DUR=20000; LINE=4000
for li,start in enumerate(range(0,DUR,LINE)):
    text=lines[li%len(lines)]
    x=200; y=880 if li%2 else 120
    for ci,ch in enumerate(text):
        w=34 if ch!=' ' else 18
        if ch==' ': x+=w; continue
        k=start+ci*60
        # base char, fades in, then sits
        print("Dialogue: 1,%s,%s,Romaji,,0,0,0,,{\\an7\\move(%d,%d,%d,%d,0,300)\\fad(250,250)\\blur0.8}%s"%(t(start-300 if start>=300 else 0),t(start+LINE),x,y-30,x,y,ch))
        # highlight with glow while sung
        print("Dialogue: 2,%s,%s,Glow,,0,0,0,,{\\an7\\pos(%d,%d)\\blur4\\3a&H40&\\t(0,200,\\fscx130\\fscy130\\blur7)\\t(200,500,\\fscx100\\fscy100\\blur3\\alpha&HFF&)}%s"%(t(k),t(k+500),x,y,ch))
        # particles
        for p in range(3):
            dx=random.randint(-80,80); dy=random.randint(-120,-30)
            print("Dialogue: 0,%s,%s,Glow,,0,0,0,,{\\an5\\move(%d,%d,%d,%d)\\blur3\\bord0\\fscx40\\fscy40\\t(\\frz%d\\alpha&HFF&)\\p1}m 0 0 l 10 4 0 8 3 4{\\p0}"%(t(k),t(k+700),x+15,y+20,x+15+dx,y+20+dy,random.randint(-180,180)))
        x+=w
