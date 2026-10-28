// Approval renderer and focused check; uses existing local Chromium tooling.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import {createRequire} from 'node:module';
import {fileURLToPath, pathToFileURL} from 'node:url';
const require=createRequire(import.meta.url);
const puppeteer=require(process.env.AIO_MOCK_PUPPETEER || '/Volumes/ExternalSSD/tmp/loading-mock/tools/node_modules/puppeteer-core');
const directory=path.dirname(fileURLToPath(import.meta.url));
const output='/Volumes/ExternalSSD/tmp/rev/aiopesq';
const context={window:{}};
vm.runInNewContext(fs.readFileSync(path.join(directory,'data.js'),'utf8'),context);
const data=context.window.AIO_RESEARCH;
assert.equal(data.catalogs,248);
assert.equal(data.catalogs-data.visibleCatalogLimit,120);
assert.equal(data.historyLimit,10);
assert.equal(data.healthMs,467);
assert.deepEqual(Object.keys(data.requests),['movie','series','anime']);
// Strict fixture vocabulary prevents accidental retention of owner URLs, IDs or labels.
const keys=new Set(['capturedAt','catalogs','visibleCatalogLimit','healthMs','manifestMs','historyLimit','requests','movie','series','anime','playable','statisticCount','errorCount','addons','phases','removals','services','name','status','streams','ms','reason','count','torrents','cached','uncached','unclassified','magnetMs','clientMs','ttfbMs','previous','Fetch','Service Wrap','Media Info','Filter','Dedup','Precompute','Sort','Limit','SEL','Total']);
const safeStrings=new Set(['Comet p2p','Torrentio p2p','Debridio Scraper AD','Debridio Scraper RD','SUCCESS','AllDebrid','Real-Debrid','Excluded Quality','Excluded Visual Tag','Excluded Keywords','Size',data.capturedAt]);
function check(value){if(Array.isArray(value)){value.forEach(check);return}if(value&&typeof value==='object'){for(const [k,v] of Object.entries(value)){assert(keys.has(k),`Unexpected fixture key: ${k}`);check(v)}}else if(typeof value==='string')assert(safeStrings.has(value),'Unexpected fixture string');else assert(Number.isFinite(value)&&value>=0,'Invalid numeric observation')}
check(data);
for(const [kind,request] of Object.entries(data.requests)){
 assert.equal(request.playable,20);assert.equal(request.errorCount,0);assert.equal(request.previous.playable,20);
 assert.equal(request.statisticCount,{movie:12,series:11,anime:5}[kind]);
 assert(request.clientMs>=request.ttfbMs);
 assert.equal(request.addons.length,kind==='anime'?1:4);
 for(const service of request.services)assert.equal(service.cached+service.uncached+service.unclassified,service.torrents);
}
assert.equal(data.requests.series.phases.Total,1210);
assert.equal(data.requests.series.phases.Total-data.requests.series.phases.Fetch-data.requests.series.phases['Service Wrap'],64);
fs.mkdirSync(output,{recursive:true});
const browser=await puppeteer.launch({executablePath:process.env.AIO_MOCK_CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true,args:['--no-first-run','--disable-background-networking']});
const screens=['overview','addons','pipeline','configuration','sources'];
const states=['healthy','loading','degraded','failing','offline'];
const pngs=[];
try{
 const page=await browser.newPage();await page.setViewport({width:1920,height:1080,deviceScaleFactor:1});
 const failures=[];
 page.on('pageerror',error=>failures.push(error.message));
 page.on('requestfailed',()=>failures.push('Asset failed'));
 await page.setRequestInterception(true);
 page.on('request',request=>{if(!request.url().startsWith('file:')){failures.push('Unexpected network request');request.abort()}else request.continue()});
 for(const screen of screens)for(const state of states){
  await page.goto(pathToFileURL(path.join(directory,'index.html')).href+`?screen=${screen}&state=${state}`);
  await page.evaluate(()=>document.fonts.ready);
  const result=await page.evaluate(()=>{
   const content=document.getElementById('content').getBoundingClientRect();
   const overflow=[...document.querySelectorAll('#content *')].filter(e=>{const r=e.getBoundingClientRect();return r.width>0&&r.height>0&&(r.bottom>content.bottom+1||r.right>content.right+1||e.scrollWidth>e.clientWidth+2)}).map(e=>e.tagName+': '+e.textContent.slice(0,45));
   return {overflow,images:[...document.images].every(i=>i.complete&&i.naturalWidth>0),focused:document.activeElement.tagName,refreshDisabled:document.getElementById('refresh').disabled,text:document.getElementById('content').textContent,fonts:document.fonts.check('23px InterDisplay'),pageOverflow:document.documentElement.scrollWidth>1920||document.documentElement.scrollHeight>1080};
  });
  assert.equal(result.pageOverflow,false,`${screen}/${state}: viewport overflow`);
  assert.equal(result.fonts,true);assert.equal(result.images,true);assert.equal(result.focused,'BUTTON');
  assert.deepEqual(result.overflow,[],`${screen}/${state}: content overflow`);
  assert.equal(result.refreshDisabled,state==='loading');
  if(screen==='sources')assert(!/Scrape Summary|Removal Reasons|Service Wrap Breakdown/.test(result.text));
  if(state==='healthy'||screen==='overview'){
   const filename=`${screen}-${state}.png`;await page.screenshot({path:path.join(output,filename)});pngs.push(path.join(output,filename));
  }
 }
 // Exercise D-pad, section routing, details and Back without pointer input.
 await page.goto(pathToFileURL(path.join(directory,'index.html')).href+'?screen=overview');
 await page.keyboard.press('ArrowUp');assert.equal(await page.evaluate(()=>document.activeElement.classList.contains('segment')),true);
 await page.keyboard.press('ArrowRight');await page.keyboard.press('Enter');assert.equal(await page.evaluate(()=>window.preview.screen),'addons');
 const first=await page.evaluate(()=>document.activeElement.dataset.detail);
 await page.keyboard.press('Enter');assert.equal(await page.$eval('[role="dialog"]',e=>e.getAttribute('aria-modal')),'true');
 await page.keyboard.press('Escape');assert.equal(await page.evaluate(()=>document.activeElement.dataset.detail),first);
 await page.keyboard.press('Escape');assert.equal(await page.evaluate(()=>window.preview.screen),'overview');
 await page.goto(pathToFileURL(path.join(directory,'index.html')).href+'?material=solid');await page.evaluate(()=>document.fonts.ready);
 const solid=path.join(output,'overview-solid.png');await page.screenshot({path:solid});pngs.push(solid);
 assert.deepEqual(failures,[]);
 console.log(`PASS: redacted fixture, 25 screen states, focus/Back, local assets; ${pngs.length} PNGs at ${output}`);
}finally{await browser.close()}
