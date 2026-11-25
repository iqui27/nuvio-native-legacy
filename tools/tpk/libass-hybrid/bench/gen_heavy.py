# Synthetic heavy typesetting (like FLE openings / frame-by-frame signs).
import math, random, sys
random.seed(7)
hdr = """[Script Info]
ScriptType: v4.00+
PlayResX: 1920
PlayResY: 1080
WrapStyle: 0
ScaledBorderAndShadow: yes

[V4+ Styles]
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding
Style: Default,Plus Jakarta Sans,64,&H00FFFFFF,&H000000FF,&H00101010,&H80000000,0,0,0,0,100,100,0,0,1,3.5,1.5,2,60,60,50,1
Style: Sign,Roboto,72,&H00FFFFFF,&H000000FF,&H00202020,&H80000000,1,0,0,0,100,100,0,0,1,4,2,5,10,10,10,1
Style: OP,Montserrat,58,&H00F0F0FF,&H000060FF,&H00402010,&H00000000,1,0,0,0,100,100,2,0,1,3,0,8,10,10,40,1

[Events]
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
"""
def t(ms):
    h=ms//3600000; m=ms//60000%60; s=ms//1000%60; cs=ms//10%100
    return "%d:%02d:%02d.%02d"%(h,m,s,cs)
out=[hdr]
DUR=20000
words="Kimi no na wa shirogane miyuki fujiwara chika kaguya sama love is war".split()
# 1) frame-by-frame motion-tracked signs (new event every 42 ms, no cache reuse)
for i,ms in enumerate(range(0,DUR,42)):
    for k in range(4):
        x=400+k*350+80*math.sin(ms/700+k); y=250+k*120+40*math.cos(ms/500+k)
        rz=10*math.sin(ms/900+k); bl=2+k*1.5
        out.append("Dialogue: %d,%s,%s,Sign,,0,0,0,,{\\pos(%.2f,%.2f)\\frz%.2f\\blur%.1f\\fscx%d\\3c&H%02X4020&}%s %s\n"%(
            2+k,t(ms),t(ms+42),x,y,rz,bl,95+k*5,(i*7)&255,words[(k*3)%len(words)].upper(),words[(k*5+1)%len(words)].upper()))
# 2) big blurred vector drawing backdrop with clip that moves
for ms in range(0,DUR,84):
    cx=200+ms/40
    out.append("Dialogue: 0,%s,%s,Sign,,0,0,0,,{\\pos(0,0)\\an7\\blur6\\bord0\\shad0\\1c&H302010&\\1a&H60&\\clip(m %d 0 l %d 0 %d 1080 %d 1080)\\p1}m 0 0 l 1920 0 1920 1080 0 1080{\\p0}\n"%(
        t(ms),t(ms+84),int(cx),int(cx+900),int(cx+700),int(cx-200)))
# 3) karaoke OP lines with \t transforms, 3D rotation and \be
for j,ms in enumerate(range(0,DUR,2500)):
    syl="".join("{\\kf%d}%s "%(random.randint(15,45),w) for w in random.sample(words,8))
    out.append("Dialogue: 5,%s,%s,OP,,0,0,0,,{\\fad(200,200)\\be3\\frx%d\\fry%d\\t(0,2500,\\frz8\\fscx120\\blur4)}%s\n"%(
        t(ms),t(ms+2500),random.randint(-20,20),random.randint(-20,20),syl))
# 4) dialogue
for j,ms in enumerate(range(0,DUR,1800)):
    out.append("Dialogue: 10,%s,%s,Default,,0,0,0,,%s\\N%s\n"%(t(ms),t(ms+1700),
        " ".join(random.sample(words,6)).capitalize()+".", " ".join(random.sample(words,5)).capitalize()+"?"))
sys.stdout.write("".join(out))
