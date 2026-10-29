// SGSR 1 GLES2 port of sgsr/v1/include/glsl/sgsr1_shader_mobile.frag:
// https://github.com/SnapdragonStudios/snapdragon-gsr
// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 3. Neither the name of the copyright holder nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Scalar texel-center reads replace textureGather (no GLES3/extensions).
// Original RGBA mode uses green as its edge signal. Keep bilinear alpha.
// uPar = source dimensions, or (0,1) for bilinear. vUv is the existing quad UV.
#define NV_SGSR_SHADER \
"// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.\n" \
"// SPDX-License-Identifier: BSD-3-Clause; full terms in src/gfx_sgsr.h\n" \
"#if defined(GL_ES) && (!defined(GL_FRAGMENT_PRECISION_HIGH) || defined(NV_SGSR_MEDIUMP))\n" \
"#define SRP mediump\n" \
"#else\n" \
"#define SRP highp\n" \
"#endif\n" \
"vec2 srWeight(vec2 d, float c, float stddev) {\n" \
"  float x = dot(d,d)*0.55 + clamp(abs(c)*stddev,0.0,1.0);\n" \
"  float a = x-4.0, b = x*a-a;\n" \
"  return vec2(1.0,c)*(b*a*a);\n" \
"}\n" \
"void main() {\n" \
"  SRP vec2 uv = uPar.y > 0.5 ? vec2(vUv.x,1.0-vUv.y) : vUv;\n" \
"  vec4 color = texture2D(uTex,uv);\n" \
"  if (uPar.x > 1.0) {\n" \
"    SRP vec2 d = 1.0/uPar;\n" \
"    SRP vec2 pixel = uv*uPar + vec2(-0.5,0.5);\n" \
"    SRP vec2 base = floor(pixel);\n" \
"    vec2 p = pixel-base;\n" \
"    SRP vec2 coord = (base+0.5)*d;\n" \
"    vec4 left;\n" \
"    left.x = texture2D(uTex,coord+vec2(-d.x,0.0)).g;\n" \
"    left.y = texture2D(uTex,coord).g;\n" \
"    left.z = texture2D(uTex,coord-vec2(0.0,d.y)).g;\n" \
"    left.w = texture2D(uTex,coord-d).g;\n" \
"    float vote = abs(left.z-left.y)+abs(color.g-left.y)+abs(color.g-left.z);\n" \
"    if (vote > 8.0/255.0) {\n" \
"      vec4 right, ud;\n" \
"      right.x = texture2D(uTex,coord+vec2(d.x,0.0)).g;\n" \
"      right.y = texture2D(uTex,coord+vec2(2.0*d.x,0.0)).g;\n" \
"      right.z = texture2D(uTex,coord+vec2(2.0*d.x,-d.y)).g;\n" \
"      right.w = texture2D(uTex,coord+vec2(d.x,-d.y)).g;\n" \
"      ud.x = texture2D(uTex,coord+vec2(0.0,-2.0*d.y)).g;\n" \
"      ud.y = texture2D(uTex,coord+vec2(d.x,-2.0*d.y)).g;\n" \
"      ud.z = texture2D(uTex,coord+vec2(d.x,d.y)).g;\n" \
"      ud.w = texture2D(uTex,coord+vec2(0.0,d.y)).g;\n" \
"      float mean = (left.y+left.z+right.x+right.w)*0.25;\n" \
"      left -= mean; right -= mean; ud -= mean;\n" \
"      float sum = dot(abs(left)+abs(right)+abs(ud),vec4(1.0));\n" \
"      float stddev = 2.181818/max(sum,0.0001);\n" \
"      vec2 w = srWeight(p+vec2(0.0,1.0),ud.x,stddev);\n" \
"      w += srWeight(p+vec2(-1.0,1.0),ud.y,stddev);\n" \
"      w += srWeight(p+vec2(-1.0,-2.0),ud.z,stddev);\n" \
"      w += srWeight(p+vec2(0.0,-2.0),ud.w,stddev);\n" \
"      w += srWeight(p+vec2(1.0,-1.0),left.x,stddev);\n" \
"      w += srWeight(p+vec2(0.0,-1.0),left.y,stddev);\n" \
"      w += srWeight(p,left.z,stddev);\n" \
"      w += srWeight(p+vec2(1.0,0.0),left.w,stddev);\n" \
"      w += srWeight(p+vec2(-1.0,-1.0),right.x,stddev);\n" \
"      w += srWeight(p+vec2(-2.0,-1.0),right.y,stddev);\n" \
"      w += srWeight(p+vec2(-2.0,0.0),right.z,stddev);\n" \
"      w += srWeight(p+vec2(-1.0,0.0),right.w,stddev);\n" \
"      float lo = min(min(left.y,left.z),min(right.x,right.w));\n" \
"      float hi = max(max(left.y,left.z),max(right.x,right.w));\n" \
"      float y = clamp(2.0*w.y/w.x,lo,hi);\n" \
"      float delta = clamp(y-(color.g-mean),-23.0/255.0,23.0/255.0);\n" \
"      color.rgb = clamp(color.rgb+delta,0.0,1.0);\n" \
"    }\n" \
"  }\n" \
"  gl_FragColor = color;\n" \
"}\n"
