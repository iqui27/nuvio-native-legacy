// Offline candidates only; original implementations from cubic/contrast equations.
// No downloaded shader source. Production keeps only measured winners.
static const char *smoothstep =
"void main(){ highp vec2 uv=vec2(vUv.x,1.0-vUv.y), pixel=uv*uPar;\n" \
"highp vec2 seam=floor(pixel+0.5), f=clamp((pixel-seam)*uFoco,-0.5,0.5)+0.5;\n" \
"f=f*f*(3.0-2.0*f)-0.5;\n" \
"gl_FragColor=vec4(texture2D(uTex,(seam+f)/uPar).rgb,texture2D(uTex,uv).a); }\n"
;

static const char *catmull4_invalid =
"void main(){ highp vec2 uv=vec2(vUv.x,1.0-vUv.y), d=1.0/uPar;\n" \
"vec4 center=texture2D(uTex,uv);\n" \
"highp vec2 p=uv*uPar-0.5, base=floor(p), f=p-base;\n" \
"vec2 f2=f*f, f3=f2*f;\n" \
"vec2 w0=-0.5*f+f2-0.5*f3, w1=1.0-2.5*f2+1.5*f3;\n" \
"vec2 w2=0.5*f+2.0*f2-1.5*f3, w3=-0.5*f2+0.5*f3;\n" \
"// Deliberately measure the proposed two-pair shortcut: negative lobes put\n" \
"// these positions outside their texel pair, so GL_LINEAR cannot implement CR.\n" \
"vec2 a=w0+w1,b=w2+w3;\n" \
"highp vec2 pa=(base-0.5+w1/a)*d,pb=(base+1.5+w3/b)*d;\n" \
"vec3 rgb=texture2D(uTex,pa).rgb*(a.x*a.y)\n" \
"+texture2D(uTex,vec2(pb.x,pa.y)).rgb*(b.x*a.y)\n" \
"+texture2D(uTex,vec2(pa.x,pb.y)).rgb*(a.x*b.y)\n" \
"+texture2D(uTex,pb).rgb*(b.x*b.y);\n" \
"gl_FragColor=vec4(clamp(rgb,0.0,1.0),center.a); }\n"
;

static const char *catmull5 =
"void main(){ highp vec2 uv=vec2(vUv.x,1.0-vUv.y), d=1.0/uPar;\n" \
"vec4 center=texture2D(uTex,uv);\n" \
"highp vec2 p=uv*uPar-0.5, base=floor(p), f=p-base;\n" \
"vec2 f2=f*f, f3=f2*f;\n" \
"vec2 w0=-0.5*f+f2-0.5*f3, w1=1.0-2.5*f2+1.5*f3;\n" \
"vec2 w2=0.5*f+2.0*f2-1.5*f3, w3=-0.5*f2+0.5*f3;\n" \
"vec2 m=w1+w2;\n" \
"highp vec2 a=(base-0.5)*d,b=(base+0.5+w2/m)*d,c=(base+2.5)*d;\n" \
"vec3 rgb=texture2D(uTex,vec2(b.x,a.y)).rgb*(m.x*w0.y)\n" \
"+texture2D(uTex,vec2(a.x,b.y)).rgb*(w0.x*m.y)\n" \
"+texture2D(uTex,b).rgb*(m.x*m.y)\n" \
"+texture2D(uTex,vec2(c.x,b.y)).rgb*(w3.x*m.y)\n" \
"+texture2D(uTex,vec2(b.x,c.y)).rgb*(m.x*w3.y);\n" \
"float sum=m.x*(w0.y+m.y+w3.y)+(w0.x+w3.x)*m.y;\n" \
"gl_FragColor=vec4(clamp(rgb/max(sum,0.0001),0.0,1.0),center.a); }\n"
;

static const char *cas =
"varying highp vec4 vLR; varying highp vec4 vUD;\n" \
"void main(){vec4 center=texture2D(uTex,vec2(vUv.x,1.0-vUv.y));\n" \
"vec3 l=texture2D(uTex,vLR.xy).rgb,r=texture2D(uTex,vLR.zw).rgb;\n" \
"vec3 u=texture2D(uTex,vUD.xy).rgb,d=texture2D(uTex,vUD.zw).rgb;\n" \
"vec3 lum=vec3(0.2126,0.7152,0.0722);\n" \
"float cL=dot(center.rgb,lum),lL=dot(l,lum),rL=dot(r,lum),uL=dot(u,lum),dL=dot(d,lum);\n" \
"float lo=min(cL,min(min(lL,rL),min(uL,dL))),hi=max(cL,max(max(lL,rL),max(uL,dL)));\n" \
"float amp=sqrt(clamp(min(lo,1.0-hi)/max(hi,0.0001),0.0,1.0));\n" \
"float w=-amp/mix(8.0,5.0,uFoco);\n" \
"gl_FragColor=vec4(clamp((center.rgb+w*(l+r+u+d))/max(1.0+4.0*w,0.0001),0.0,1.0),center.a);}\n"
;
