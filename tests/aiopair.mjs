import assert from 'node:assert/strict';
import vm from 'node:vm';
import { AioPair, PAIR_TTL, routeAioPair, phonePage } from '../servidor/recomendacoes/src/aiopair.js';
// Synthetic capabilities and credentials; no real account or remote transport.
const tv='11111111-1111-4111-a111-111111111111', phone='2222222222224222a222222222222222';
const origin='https://instance.invalid';
const script=phonePage().match(/<script>([\s\S]*)<\/script>/)[1];
async function fixture() {
  let alarm,writes=0;
  const pair=new AioPair({storage:{setAlarm(t){alarm=t;},put(){writes++;}}});
  const call=b=>pair.fetch(new Request('https://pair.invalid/v1/aiopair/22222222',{method:'POST',body:JSON.stringify(b)}));
  assert.equal((await call({op:'create',origin,tv,phone})).status,200);
  assert.ok(alarm>Date.now() && alarm<=Date.now()+PAIR_TTL);
  const button={disabled:false};
  const elements=Object.fromEntries(['#form','#status','#origin','#uuid','#password'].map(k=>[k,{textContent:'',hidden:false,value:'',querySelector:()=>button}]));
  const seen=[],logs=[];
  const f={pair,call,elements,button,seen,logs,instance:'good',service:'good',statusOrigin:origin,afterValidation:null};
  const context={location:{hash:'#'+phone,pathname:'/aio/'},history:{replaceState(){}},URL,AbortSignal,TextEncoder,
    document:{querySelector:s=>elements[s]},setTimeout:()=>1,
    console:Object.fromEntries(['log','warn','error'].map(k=>[k,(...a)=>logs.push(a.join(' '))])),
    btoa:s=>Buffer.from(s,'binary').toString('base64'),unescape,encodeURIComponent,
    fetch:async(u,opt)=>{
      seen.push({u,opt}); assert.ok(!u.includes('synthetic-password')&&!u.includes('sample-uuid'));
      assert.equal(opt.redirect,'error'); assert.equal(opt.credentials,'omit');
      assert.equal(opt.referrerPolicy,'no-referrer'); assert.ok(opt.signal);
      if(u.startsWith('/v1/')) {
        assert.equal(u,'/v1/aiopair/22222222');
        if(f.service==='network') throw Error('private transport detail');
        if(f.service==='server') return new Response('',{status:503});
        if(f.service==='html') return new Response('private response detail');
        const b=JSON.parse(opt.body);
        if(b.op==='status' && f.statusOrigin!==origin) return Response.json({state:'waiting',origin:f.statusOrigin});
        return call(b);
      }
      assert.equal(u,origin+'/api/v1/user');
      assert.equal(opt.headers.Authorization,'Basic '+Buffer.from('sample-uuid:synthetic-password').toString('base64'));
      if(f.instance==='network' || f.instance==='redirect') throw Error('private transport detail');
      if(f.afterValidation) await f.afterValidation();
      return Response.json(f.instance==='invalid'?{success:false}:{success:true,data:{userData:{presets:[]}}},
        {status:f.instance==='auth'?401:f.instance==='server'?503:200});
    }};
  vm.createContext(context); await vm.runInContext(script,context);
  f.context=context;
  f.submit=async()=>{
    elements['#uuid'].value='sample-uuid'; elements['#password'].value='synthetic-password';
    await elements['#form'].onsubmit({preventDefault(){}});
  };
  f.take=async()=> (await call({op:'take',key:tv})).json();
  f.clean=()=>{
    assert.equal(writes,0); assert.equal(logs.join(''),'');
    assert.ok(!elements['#status'].textContent.includes('private'));
    assert.ok(seen.filter(x=>x.opt.body?.includes('synthetic-password')).every(x=>x.u.startsWith('/v1/aiopair/')));
  };
  return f;
}
for(const [mode,message] of [['auth','rejected access'],['network','CORS'],['redirect','CORS'],['server','rate limited'],['invalid','invalid user response']]) {
  const f=await fixture(); f.instance=mode; await f.submit();
  assert.ok(f.elements['#status'].textContent.includes(message));
  assert.equal((await f.take()).uuid,undefined); assert.equal(f.elements['#password'].value,'');
  assert.equal(f.button.disabled,false); f.clean();
}
{
  const f=await fixture();
  assert.equal((await f.call(null)).status,400);
  assert.equal((await f.call({op:'send',key:phone,origin,uuid:'sample',password:'é'.repeat(385)})).status,400);
  assert.equal((await f.call({op:'take',key:phone})).status,400);
  assert.equal((await f.call({op:'take',key:'bad'})).status,403);
  assert.equal((await f.call({op:'send',key:phone,origin:'https://other.invalid',uuid:'sample',password:'synthetic-password'})).status,400);
  assert.equal((await f.call({op:'result',key:tv,state:'synced'})).status,409);
  await f.submit(); assert.equal(f.button.disabled,true);
  const before=f.seen.length; await f.submit(); assert.equal(f.seen.length,before);
  const payload=await f.take(); assert.equal(payload.origin,origin); assert.equal(payload.uuid,'sample-uuid'); assert.equal(payload.password,'synthetic-password');
  // Simulate a lost take response. Delivery remains available only to this TV.
  assert.deepEqual(await f.take(),payload);
  assert.equal((await f.call({op:'send',key:phone,origin,uuid:'sample',password:'synthetic-password'})).status,409);
  assert.equal((await f.call({op:'result',key:tv,state:'synced'})).status,200);
  assert.equal((await f.take()).password,undefined);
  assert.equal((await f.call({op:'result',key:tv,state:'synced'})).status,200); // lost confirmation response
  await f.context.poll(); assert.equal(f.elements['#status'].textContent,'Synced. Your TV is ready.');
  assert.ok(f.elements['#form'].hidden); f.clean();
}
for(const changed of ['https://other.invalid','http://instance.invalid','https://instance.invalid/path','https://user:pass@instance.invalid']) {
  const f=await fixture(); f.statusOrigin=changed; await f.context.poll(); await f.submit();
  assert.ok(f.elements['#form'].hidden); assert.ok(f.elements['#status'].textContent.includes('origin changed'));
  assert.ok(!f.seen.some(x=>!x.u.startsWith('/v1/'))); f.clean();
}
for(const mode of ['network','server','html']) {
  const f=await fixture(); f.service=mode; await f.context.poll();
  assert.ok(f.button.disabled); assert.ok(!f.elements['#form'].hidden);
  f.service='good'; await f.context.poll(); assert.equal(f.button.disabled,false); f.clean();
}
for(const reason of ['network','server','auth','origin','response']) {
  const f=await fixture(); await f.submit(); await f.take();
  assert.equal((await f.call({op:'result',key:tv,state:'error',reason})).status,200);
  await f.context.poll(); assert.equal(f.button.disabled,reason==='network'||reason==='server');
  if(f.button.disabled) assert.ok(f.elements['#status'].textContent.includes('new QR'));
  assert.ok(!f.elements['#status'].textContent.includes('private')); f.clean();
}
for(const cancelled of [false,true]) {
  const f=await fixture();
  if(cancelled) await f.call({op:'close',key:tv}); else await f.pair.alarm();
  await f.context.poll(); assert.ok(f.elements['#form'].hidden && f.button.disabled);
  assert.ok(f.elements['#status'].textContent.includes('expired or cancelled')); f.clean();
}
{
  const f=await fixture(); f.afterValidation=()=>f.call({op:'close',key:tv}); await f.submit();
  assert.ok(f.elements['#form'].hidden && f.button.disabled); assert.equal(f.elements['#password'].value,''); f.clean();
}
const page=await routeAioPair(new Request('https://pair.invalid/aio/'),{});
assert.equal(page.headers.get('cache-control'),'no-store');
assert.equal(page.headers.get('referrer-policy'),'no-referrer');
assert.equal((await routeAioPair(new Request('https://pair.invalid/v1/aiopair/22222222'),{})).status,503);
assert.ok(!phonePage().includes('localStorage'));
console.log('aiopair: shipped phone/worker, origin pinning, auth/network/server errors, retry delivery/confirmation, expiry/cancellation and private logs passed');
