// Original implementation: integer prescale followed by a one-destination-pixel transition.
// uPar = inverse source size; uFoco = integer scale, supplied by C.
// No derivatives; no-highp GLES2 compiles only the bilinear fallback.
#define NV_SHARP_BILINEAR_SHADER \
"void main() {\n" \
"#if !defined(GL_ES) || (defined(GL_FRAGMENT_PRECISION_HIGH) && !defined(NV_SGSR_MEDIUMP))\n" \
"  highp vec2 uv = vec2(vUv.x,1.0-vUv.y);\n" \
"  highp vec2 pixel = uv/uPar;\n" \
"  highp vec2 seam = floor(pixel+0.5);\n" \
"  highp vec2 f = clamp((pixel-seam)*uFoco,-0.5,0.5);\n" \
"  vec3 rgb = texture2D(uTex,(seam+f)*uPar).rgb;\n" \
"  gl_FragColor = vec4(rgb,texture2D(uTex,uv).a);\n" \
"#else\n" \
"  gl_FragColor = texture2D(uTex,vec2(vUv.x,1.0-vUv.y));\n" \
"#endif\n" \
"}\n"

// FSR 1 EASU and RCAS: GLES2 float ports of AMD FidelityFX ffx_fsr1.h
// (MIT, Copyright (c) 2021 Advanced Micro Devices, Inc.). Edge-adaptive
// 12-tap Lanczos-like kernel, deringed to the nearest 2x2. Keep bilinear alpha.
// uPar = source size in texels. No-highp GLES2 compiles only bilinear.
#define NV_EASU_SHADER \
"#if !defined(GL_ES) || (defined(GL_FRAGMENT_PRECISION_HIGH) && !defined(NV_SGSR_MEDIUMP))\n" \
"#ifdef GL_ES\nprecision highp float;\n#endif\n" \
"highp vec2 eD;\n" \
"vec3 eT(highp vec2 b, vec2 o) { return texture2D(uTex,(b+o+0.5)*eD).rgb; }\n" \
"float eL(vec3 c) { return c.b*0.5+(c.r*0.5+c.g); }\n" \
"void eSet(inout vec2 dir, inout float len, float w, float a, float b, float c, float d, float e) {\n" \
"  float dc = d-c, cb = c-b;\n" \
"  float lx = max(abs(dc),abs(cb)); float dx = d-b;\n" \
"  dir.x += dx*w; lx = clamp(abs(dx)/max(lx,1e-5),0.0,1.0); len += lx*lx*w;\n" \
"  float ec = e-c, ca = c-a;\n" \
"  float ly = max(abs(ec),abs(ca)); float dy = e-a;\n" \
"  dir.y += dy*w; ly = clamp(abs(dy)/max(ly,1e-5),0.0,1.0); len += ly*ly*w;\n" \
"}\n" \
"void eTap(inout vec3 ac, inout float aw, vec2 o, vec2 dir, vec2 len, float lob, float clp, vec3 c) {\n" \
"  vec2 v = vec2(o.x*dir.x+o.y*dir.y, o.y*dir.x-o.x*dir.y)*len;\n" \
"  float d2 = min(dot(v,v),clp);\n" \
"  float wb = 0.4*d2-1.0, wa = lob*d2-1.0;\n" \
"  wb *= wb; wa *= wa; wb = 1.5625*wb-0.5625;\n" \
"  ac += c*(wb*wa); aw += wb*wa;\n" \
"}\n" \
"void main() {\n" \
"  highp vec2 uv = vec2(vUv.x,1.0-vUv.y);\n" \
"  eD = 1.0/uPar;\n" \
"  highp vec2 pp = uv*uPar-0.5;\n" \
"  highp vec2 fp = floor(pp); pp -= fp;\n" \
"  vec3 b = eT(fp,vec2(0.0,-1.0)), c = eT(fp,vec2(1.0,-1.0));\n" \
"  vec3 e = eT(fp,vec2(-1.0,0.0)), f = eT(fp,vec2(0.0)), g = eT(fp,vec2(1.0,0.0)), h = eT(fp,vec2(2.0,0.0));\n" \
"  vec3 i = eT(fp,vec2(-1.0,1.0)), j = eT(fp,vec2(0.0,1.0)), k = eT(fp,vec2(1.0)), l = eT(fp,vec2(2.0,1.0));\n" \
"  vec3 n = eT(fp,vec2(0.0,2.0)), o = eT(fp,vec2(1.0,2.0));\n" \
"  float bL=eL(b), cL=eL(c), eL_=eL(e), fL=eL(f), gL=eL(g), hL=eL(h);\n" \
"  float iL=eL(i), jL=eL(j), kL=eL(k), lL=eL(l), nL=eL(n), oL=eL(o);\n" \
"  vec2 dir = vec2(0.0); float len = 0.0;\n" \
"  eSet(dir,len,(1.0-pp.x)*(1.0-pp.y),bL,eL_,fL,gL,jL);\n" \
"  eSet(dir,len,pp.x*(1.0-pp.y),cL,fL,gL,hL,kL);\n" \
"  eSet(dir,len,(1.0-pp.x)*pp.y,fL,iL,jL,kL,nL);\n" \
"  eSet(dir,len,pp.x*pp.y,gL,jL,kL,lL,oL);\n" \
"  float dr = dot(dir,dir);\n" \
"  if (dr < 1.0/32768.0) dir = vec2(1.0,0.0); else dir *= inversesqrt(dr);\n" \
"  len *= 0.5; len *= len;\n" \
"  float st = dot(dir,dir)/max(abs(dir.x),abs(dir.y));\n" \
"  vec2 len2 = vec2(1.0+(st-1.0)*len, 1.0-0.5*len);\n" \
"  float lob = 0.5+((1.0/4.0-0.04)-0.5)*len, clp = 1.0/lob;\n" \
"  vec3 ac = vec3(0.0); float aw = 0.0;\n" \
"  eTap(ac,aw,vec2(0.0,-1.0)-pp,dir,len2,lob,clp,b);\n" \
"  eTap(ac,aw,vec2(1.0,-1.0)-pp,dir,len2,lob,clp,c);\n" \
"  eTap(ac,aw,vec2(-1.0,1.0)-pp,dir,len2,lob,clp,i);\n" \
"  eTap(ac,aw,vec2(0.0,1.0)-pp,dir,len2,lob,clp,j);\n" \
"  eTap(ac,aw,vec2(0.0)-pp,dir,len2,lob,clp,f);\n" \
"  eTap(ac,aw,vec2(-1.0,0.0)-pp,dir,len2,lob,clp,e);\n" \
"  eTap(ac,aw,vec2(1.0)-pp,dir,len2,lob,clp,k);\n" \
"  eTap(ac,aw,vec2(2.0,1.0)-pp,dir,len2,lob,clp,l);\n" \
"  eTap(ac,aw,vec2(2.0,0.0)-pp,dir,len2,lob,clp,h);\n" \
"  eTap(ac,aw,vec2(1.0,0.0)-pp,dir,len2,lob,clp,g);\n" \
"  eTap(ac,aw,vec2(1.0,2.0)-pp,dir,len2,lob,clp,o);\n" \
"  eTap(ac,aw,vec2(0.0,2.0)-pp,dir,len2,lob,clp,n);\n" \
"  vec3 mn = min(min(f,g),min(j,k)), mx = max(max(f,g),max(j,k));\n" \
"  gl_FragColor = vec4(clamp(ac/aw,mn,mx),texture2D(uTex,uv).a);\n" \
"#else\n" \
"void main() {\n" \
"  gl_FragColor = texture2D(uTex,vec2(vUv.x,1.0-vUv.y));\n" \
"#endif\n" \
"}\n"

// Lanczos-2 (a = 2), separable 4x4 texel-center taps. uPar = source size.
#define NV_LANCZOS_SHADER \
"#if !defined(GL_ES) || (defined(GL_FRAGMENT_PRECISION_HIGH) && !defined(NV_SGSR_MEDIUMP))\n" \
"#ifdef GL_ES\nprecision highp float;\n#endif\n" \
"vec4 lzW(float f) {\n" \
"  vec4 x = max(abs(vec4(f+1.0,f,1.0-f,2.0-f))*3.14159265,1e-5);\n" \
"  vec4 w = sin(x)*sin(0.5*x)/(0.5*x*x);\n" \
"  return w/dot(w,vec4(1.0));\n" \
"}\n" \
"vec3 lzRow(highp vec2 b, float y, vec4 wx) {\n" \
"  highp vec2 d = 1.0/uPar;\n" \
"  return texture2D(uTex,(b+vec2(-0.5,y))*d).rgb*wx.x + texture2D(uTex,(b+vec2(0.5,y))*d).rgb*wx.y\n" \
"       + texture2D(uTex,(b+vec2(1.5,y))*d).rgb*wx.z + texture2D(uTex,(b+vec2(2.5,y))*d).rgb*wx.w;\n" \
"}\n" \
"void main() {\n" \
"  highp vec2 uv = vec2(vUv.x,1.0-vUv.y);\n" \
"  highp vec2 p = uv*uPar-0.5;\n" \
"  highp vec2 b = floor(p);\n" \
"  vec2 f = p-b;\n" \
"  vec4 wx = lzW(f.x), wy = lzW(f.y);\n" \
"  vec3 c = lzRow(b,-0.5,wx)*wy.x + lzRow(b,0.5,wx)*wy.y + lzRow(b,1.5,wx)*wy.z + lzRow(b,2.5,wx)*wy.w;\n" \
"  gl_FragColor = vec4(clamp(c,0.0,1.0),texture2D(uTex,uv).a);\n" \
"#else\n" \
"void main() {\n" \
"  gl_FragColor = texture2D(uTex,vec2(vUv.x,1.0-vUv.y));\n" \
"#endif\n" \
"}\n"

// FSR 1 RCAS at output resolution, without the optional denoise.
// uPar = inverse texture size; uFoco = sharpness (1 = AMD's 0-stop maximum).
#define NV_RCAS_SHADER \
"#if !defined(GL_ES) || (defined(GL_FRAGMENT_PRECISION_HIGH) && !defined(NV_SGSR_MEDIUMP))\n" \
"#ifdef GL_ES\nprecision highp float;\n#endif\n" \
"void main() {\n" \
"  highp vec2 uv = vec2(vUv.x,1.0-vUv.y);\n" \
"  vec4 e = texture2D(uTex,uv);\n" \
"  vec3 b = texture2D(uTex,uv-vec2(0.0,uPar.y)).rgb, h = texture2D(uTex,uv+vec2(0.0,uPar.y)).rgb;\n" \
"  vec3 d = texture2D(uTex,uv-vec2(uPar.x,0.0)).rgb, f = texture2D(uTex,uv+vec2(uPar.x,0.0)).rgb;\n" \
"  vec3 mn = min(min(b,d),min(f,h)), mx = max(max(b,d),max(f,h));\n" \
"  vec3 lo = max(-min(mn,e.rgb)/max(4.0*mx,1e-4), (1.0-max(mx,e.rgb))/min(4.0*mn-4.0,-1e-4));\n" \
"  float lobe = max(-0.1875, min(max(lo.r,max(lo.g,lo.b)),0.0))*uFoco;\n" \
"  gl_FragColor = vec4(clamp((lobe*(b+d+f+h)+e.rgb)/(4.0*lobe+1.0),0.0,1.0),e.a);\n" \
"}\n" \
"#else\n" \
"void main() { gl_FragColor = texture2D(uTex,vec2(vUv.x,1.0-vUv.y)); }\n" \
"#endif\n"
