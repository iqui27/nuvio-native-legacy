// Local HTTPS fake instance + real worker handler + real TV polling thread.
import https from 'node:https';
import fs from 'node:fs';
import vm from 'node:vm';
import { spawn } from 'node:child_process';
import assert from 'node:assert/strict';
import { AioPair, phonePage } from '../servidor/recomendacoes/src/aiopair.js';
const [binary,dir]=process.argv.slice(2),options={key:fs.readFileSync(dir+'/key.pem'),cert:fs.readFileSync(dir+'/cert.pem')};
let pair,instanceOrigin,pairOrigin,instanceRequests=0;
const instance=https.createServer(options,(req,res)=>{
  assert.equal(req.url,'/api/v1/user');instanceRequests++;
  assert.equal(req.headers.authorization,'Basic '+Buffer.from('sample-uuid:synthetic-password').toString('base64'));
  res.writeHead(200,{'content-type':'application/json','access-control-allow-origin':'*'});
  res.end('{"success":true,"data":{"userData":{"presets":[{"type":"Torrentio"}]}}}');
});
const service=https.createServer(options,async(req,res)=>{
  const chunks=[];for await(const c of req)chunks.push(c);
  if(!pair)pair=new AioPair({storage:{setAlarm(){}}});
  const r=await pair.fetch(new Request(pairOrigin+req.url,{method:req.method,body:Buffer.concat(chunks)}));
  res.writeHead(r.status,Object.fromEntries(r.headers));res.end(await r.text());
});
await new Promise(r=>instance.listen(0,'127.0.0.1',r));await new Promise(r=>service.listen(0,'127.0.0.1',r));
instanceOrigin='https://localhost:'+instance.address().port;pairOrigin='https://localhost:'+service.address().port;
const child=spawn(binary,[pairOrigin,instanceOrigin,dir+'/cert.pem',dir+'/url',dir+'/done'],{stdio:['ignore','pipe','pipe']});
let output='';child.stdout.on('data',b=>output+=b);child.stderr.on('data',b=>output+=b);
const ended=new Promise(r=>child.on('exit',r));
try {
  for(let i=0;i<120&&!fs.existsSync(dir+'/url');i++)await new Promise(r=>setTimeout(r,100));
  const pairing=new URL(fs.readFileSync(dir+'/url','utf8'));
  const elements=Object.fromEntries(['#form','#status','#origin','#uuid','#password'].map(s=>[s,{value:'',querySelector:()=>({disabled:false})}]));
  async function fetchLocal(url,opt={}) {
    const u=new URL(url,pairOrigin);assert.ok(u.origin===pairOrigin||u.origin===instanceOrigin);
    assert.equal(u.search,'');
    return new Promise((resolve,reject)=>{const req=https.request(u,{method:opt.method||'GET',headers:opt.headers,ca:options.cert},res=>{
      const chunks=[];res.on('data',b=>chunks.push(b));res.on('end',()=>resolve(new Response(Buffer.concat(chunks),{status:res.statusCode})));});req.on('error',reject);req.end(opt.body);});
  }
  const context={location:{hash:pairing.hash,pathname:'/aio/'},history:{replaceState(){}},document:{querySelector:s=>elements[s]},
    URL,AbortSignal,TextEncoder,setTimeout:()=>1,fetch:fetchLocal,btoa:s=>Buffer.from(s,'binary').toString('base64'),unescape,encodeURIComponent};
  vm.createContext(context);vm.runInContext(phonePage().match(/<script>([\s\S]*)<\/script>/)[1],context);
  for(let i=0;i<40&&!elements['#origin'].textContent;i++)await new Promise(r=>setTimeout(r,50));
  assert.equal(elements['#origin'].textContent,instanceOrigin);
  elements['#uuid'].value='sample-uuid';elements['#password'].value='synthetic-password';
  await elements['#form'].onsubmit({preventDefault(){}});
  const rc=await ended;assert.equal(rc,0,output);assert.equal(fs.readFileSync(dir+'/done','utf8'),'ok');
  assert.equal(instanceRequests,2); // phone verification, then independent TV verification
  assert.ok(!/synthetic-password|sample-uuid|Authorization:/.test(output));
  console.log('aiopair HTTPS end-to-end: phone verifies, worker relays once, TV independently validates and syncs; no secret logs');
}finally{child.kill();await new Promise(r=>service.close(r));await new Promise(r=>instance.close(r));}
