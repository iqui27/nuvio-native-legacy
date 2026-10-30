import assert from 'node:assert/strict';
import vm from 'node:vm';
import { AioPair, PAIR_TTL, routeAioPair, phonePage } from '../servidor/recomendacoes/src/aiopair.js';
const tv='11111111-1111-4111-a111-111111111111', phone='2222222222224222a222222222222222';
const origin='https://instance.invalid';
let alarm, writes=0;
const pair=new AioPair({storage:{setAlarm(t){alarm=t;},put(){writes++;}}});
async function call(b) {return pair.fetch(new Request('https://pair.invalid/v1/aiopair/33333333-3333-4333-a333-333333333333',{method:'POST',body:JSON.stringify(b)}));}
assert.equal((await call({op:'create',origin,tv,phone})).status,200);
assert.ok(alarm>Date.now() && alarm<=Date.now()+PAIR_TTL);
assert.equal((await call({op:'take',key:phone})).status,400);
assert.equal((await call({op:'send',key:phone,origin:'https://other.invalid',uuid:'sample',password:'synthetic-password'})).status,400);
assert.equal((await call({op:'take',key:'bad'})).status,403);
// Execute the shipped phone script. Fetch is a fake instance plus real session
// handler, so assertions cover form submission, POST contents and redirects.
const script=phonePage().match(/<script>([\s\S]*)<\/script>/)[1];
const elements=Object.fromEntries(['#form','#status','#origin','#uuid','#password'].map(k=>[k,{textContent:'',hidden:false,value:'',querySelector:()=>({disabled:false})}]));
const seen=[],logs=[]; let valid=true,redirect=false;
const context={location:{hash:'#'+phone,pathname:'/aio/'},history:{replaceState(){}},
 document:{querySelector:s=>elements[s]},setTimeout:()=>1,console:{log:(...a)=>logs.push(a.join(' '))},btoa:s=>Buffer.from(s,'binary').toString('base64'),unescape,encodeURIComponent,
 fetch:async(u,opt)=>{
  seen.push({u,opt});assert.ok(!u.includes('synthetic-password')&&!u.includes('sample-uuid'));
  if(u.startsWith('/v1/'))return call(JSON.parse(opt.body));
  assert.equal(u,origin+'/api/v1/user');assert.equal(opt.redirect,'error');assert.equal(opt.credentials,'omit');
  if(redirect)throw Error('redirect refused');
  return new Response(JSON.stringify({data:{userData:{presets:[]}}}),{status:valid?200:401});
 }};
vm.createContext(context);vm.runInContext(script,context);
await new Promise(r=>setImmediate(r));
elements['#uuid'].value='sample-uuid';elements['#password'].value='synthetic-password';
valid=false;await elements['#form'].onsubmit({preventDefault(){}});
assert.equal((await (await call({op:'take',key:tv})).json()).uuid,undefined);
valid=true;redirect=true;await elements['#form'].onsubmit({preventDefault(){}});
assert.equal((await (await call({op:'take',key:tv})).json()).uuid,undefined);
redirect=false;await elements['#form'].onsubmit({preventDefault(){}});
const payload=await (await call({op:'take',key:tv})).json();
assert.equal(payload.origin,origin);assert.equal(payload.uuid,'sample-uuid');assert.equal(payload.password,'synthetic-password');
assert.equal(elements['#password'].value,'');
assert.equal((await (await call({op:'take',key:tv})).json()).password,undefined);
assert.equal((await call({op:'result',key:tv,state:'synced'})).status,200);
await context.poll();assert.equal(elements['#status'].textContent,'Synced. Your TV is ready.');
assert.equal(writes,0);assert.equal(logs.join(''),'');
assert.ok(seen.filter(x=>x.opt.body?.includes('synthetic-password')).every(x=>x.u.startsWith('/v1/aiopair/')));
await pair.alarm();assert.equal((await call({op:'status',key:phone})).status,410);
const response=await routeAioPair(new Request('https://pair.invalid/aio/'),{});
assert.equal(response.headers.get('cache-control'),'no-store');
assert.ok(!phonePage().includes('localStorage'));
console.log('aiopair: phone script, valid/wrong access, redirect refusal, origin binding, single-use inbox, expiry, no secret logs/storage passed');
