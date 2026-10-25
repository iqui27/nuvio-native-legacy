const fs=require('fs');
const path=require('path');
const {chromium}=require('/Users/hrocha/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
(async()=>{
 const out=process.argv[2]||'/Volumes/ExternalSSD/tmp/rev/guiav5-png';
 const browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true,args:['--disable-background-networking']});
 const page=await browser.newPage({viewport:{width:390,height:844},deviceScaleFactor:1});
 const html=fs.readFileSync(path.join(out,'phone.html'),'utf8');
 let status={s:'idle'}, submitted;
 const groups=[{n:'Esportes',c:64},{n:'Família',c:214},{n:'Lançamentos',c:128}];
 await page.route('http://pair.test/**',async route=>{
   const req=route.request();
   if(req.method()==='POST'){
     submitted=JSON.parse(new URLSearchParams(req.postData()).get('t'));
     status=submitted.step===1?{s:'ok',live:640,movies:12480,series:1260,groups}:{s:'done',live:640,movies:12480,series:1260};
     return route.fulfill({body:'ok'});
   }
   return route.fulfill({contentType:req.url().endsWith('/s')?'application/json':'text/html',body:req.url().endsWith('/s')?JSON.stringify(status):html});
 });
 await page.goto('http://pair.test/paired');
 await page.screenshot({path:path.join(out,'celular-1-dados-app.png')});
 if(await page.locator('#playlist').isVisible())throw Error('M3U input exposed for Xtream');
 if(await page.evaluate(()=>document.documentElement.scrollHeight)>844)throw Error('Phone setup exceeds approved canvas');
 await page.fill('#name','IPTV da sala');await page.fill('#server','https://provider.test');await page.fill('#user','fixture');await page.fill('#password','fixture');
 await page.click('#next');
 await page.locator('#s2').waitFor({state:'visible'});
 await page.screenshot({path:path.join(out,'celular-2-importar-app.png')});
 await page.locator('#cat1').uncheck();
 await page.click('#send');
 await page.locator('#s3').waitFor({state:'visible'});
 if('server' in submitted||'password' in submitted||'user' in submitted)throw Error('Final choices resend credentials');
 if(JSON.stringify(submitted.off)!=='[1]')throw Error('Category choices must use tested indices');
 await page.screenshot({path:path.join(out,'celular-3-pronto-app.png')});
 // Provider-controlled labels must render as text, including markup characters.
 groups[0].n='<img src=x onerror="window.injected=1">';
 await page.goto('http://pair.test/paired');
 await page.fill('#server','https://provider.test');await page.fill('#user','fixture');await page.fill('#password','fixture');
 await page.click('#next');await page.locator('#s2').waitFor({state:'visible'});
 if(await page.locator('#catlist img').count())throw Error('Provider category interpreted as HTML');
 if(!(await page.locator('#catlist').innerText()).includes('<img'))throw Error('Provider label lost');
 await browser.close();console.log('PASS: phone form states captured; submission covered by HTTP target test');
})().catch(e=>{console.error(e);process.exit(1)});
