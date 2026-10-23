const fs = require('node:fs');
const vm = require('node:vm');
const cp = require('node:child_process');
const assert = require('node:assert/strict');
const source = fs.readFileSync('src/rede.c', 'utf8');
const helperDefs = source.match(/static int nvPrazo\([^]*?\n\}\n/)[0];
const probe = helperDefs + source.match(/static int sondaVidaa\([^]*?\n\}/)[0] + '\n' +
  source.match(/int rede_url_final_vidaa\([^]*?\n\}/)[0];
fs.mkdirSync('build/tests-vidaa-review', {recursive:true});
fs.writeFileSync('build/tests-vidaa-review/probe.c', `
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
static int response;
static char *nv_http(const char *m,const char *u,const char *c,void *b,int *n,int *s,char *d,int t,void *e,int et,void *r,int p) { (void)p;
assert(!strcmp(m,"HEAD")); assert(!c);
*s=response; if (!response) return NULL; strcpy(d,"https://media/final"); return malloc(1);
}
${probe}
int main(void) { char dst[128];
int statuses[]={200,206,403,404,405,500,501,0};
for (int i=0;i<8;i++) { response=statuses[i]; assert(rede_url_final_vidaa("https://media",1,dst,sizeof dst)==(response==0||response==403||response==405||response==501?-1:response<300?1:0)); }
}
`);
let failures = 0;
try {
  cp.execFileSync('cc', ['build/tests-vidaa-review/probe.c','-o','build/tests-vidaa-review/probe']);
  cp.execFileSync('build/tests-vidaa-review/probe', {stdio:'pipe'});
  console.log('PASS: HTTP probe rejects completed error responses');
} catch(e) { failures++; console.error('FAIL: HTTP probe rejects completed error responses'); }
try {
  // Every wasm probe (stream_url_serve, fontevolta, diagnostico) goes through
  // rede_url_final_tipo: on VIDAA it must HEAD, never the sync GET sonda.
  const tipo = source.match(/int rede_url_final_tipo\([^]*?\n\}/)[0];
  const helper = helperDefs + (source.match(/static int sondaVidaa\([^]*?\n\}/) || [''])[0];
  fs.writeFileSync('build/tests-vidaa-review/tipo.c', `
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>
#define NV_VIDAA 1
static int response, heads;
static char *juntarCabs(const char *const *c, const char *x) { (void)x; return c ? strdup(c[0]) : NULL; }
static char *nv_http(const char *m,const char *u,const char *c,void *b,int *n,int *s,char *d,int t,void *e,int et,void *r,int p) { (void)p;
assert(!strcmp(m,"HEAD")); assert(c && !strcmp(c,"Authorization: x")); heads++;
*s=response; if (!response) return NULL; strcpy(d,"https://media/final"); return malloc(1);
}
static int nv_url_sonda(const char *u,const char *c,char *d,int t,int *s,char *m,int mt,long *b,int p) { abort(); }
${helper}
${tipo}
int main(void) { char dst[128]; int st; long corpo; const char *cab[]={"Authorization: x",NULL};
response=404; assert(rede_url_final_tipo("https://media",5,cab,dst,sizeof dst,&st,NULL,0,&corpo)==0 && st==404);
response=200; assert(rede_url_final_tipo("https://media",5,cab,dst,sizeof dst,&st,NULL,0,&corpo)==1 && !strcmp(dst,"https://media/final") && corpo==-1);
response=405; assert(rede_url_final_tipo("https://media",5,cab,dst,sizeof dst,&st,NULL,0,&corpo)==1 && !strcmp(dst,"https://media"));
response=0; assert(rede_url_final_tipo("https://media",5,cab,dst,sizeof dst,&st,NULL,0,&corpo)==1 && !strcmp(dst,"https://media"));
assert(heads==4);
}
`);
  cp.execFileSync('cc', ['-w','build/tests-vidaa-review/tipo.c','-o','build/tests-vidaa-review/tipo']);
  cp.execFileSync('build/tests-vidaa-review/tipo', {stdio:'pipe'});
  console.log('PASS: every VIDAA URL probe uses HEAD');
} catch(e) { failures++; console.error('FAIL: every VIDAA URL probe uses HEAD'); }
try {
  const shell = fs.readFileSync('tools/vidaa-shell.html','utf8');
  const fallback = shell.slice(shell.indexOf("  if (NV_MODO === 'mt'"), shell.indexOf('  // DIAGNOSTICO'));
  let redirected;
  vm.runInNewContext(fallback, {NV_MODO:'mt',SharedArrayBuffer:undefined,self:{},localStorage:{setItem(){throw Error('blocked');}},location:{pathname:'/app/mt/index.html',search:'?x=1',replace(v){redirected=v;}},console:{log(){}}});
  assert.equal(redirected, '/app/st/index.html?x=1');
  console.log('PASS: MT fallback survives blocked storage');
} catch(e) { failures++; console.error('FAIL: MT fallback survives blocked storage'); }
process.exitCode = failures ? 1 : 0;
